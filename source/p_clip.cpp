//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**  You should have received a copy of the GNU General Public License
//**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//**
//**************************************************************************
#include "gamedefs.h"
#include "sv_local.h"

//#define VV_CLIPPER_USE_DIAMONDS

//#define XXX_CLIPPER_DEBUG
#define XXX_CLIPPER_MANY_DUMPS

#ifdef VAVOOM_CLIPPER_USE_REAL_ANGLES
# define MIN_ANGLE  ((VFloat)0)
# define MAX_ANGLE  ((VFloat)360)
#else
# define MIN_ANGLE  ((VFloat)-1)
# define MAX_ANGLE  ((VFloat)3)
#endif


// ////////////////////////////////////////////////////////////////////////// //
#ifdef CLIENT
extern VCvarB r_draw_pobj;
#else
enum { r_draw_pobj = true };
#endif

static VCvarB clip_bbox("clip_bbox", true, "Clip BSP bboxes with 1D clipper?", CVAR_PreInit);
static VCvarB clip_check_bbox_z("clip_check_bbox_z", false, "Consider bbox height when cliping BSP bboxes with 1D clipper?", CVAR_PreInit);
static VCvarB clip_subregion("clip_subregion", true, "Clip subregions?", CVAR_PreInit);
static VCvarB clip_enabled("clip_enabled", true, "Do geometry cliping optimizations?", CVAR_PreInit);
static VCvarB clip_with_polyobj("clip_with_polyobj", true, "Do clipping with polyobjects?", CVAR_PreInit);
static VCvarB clip_platforms("clip_platforms", true, "Clip geometry behind some closed doors and lifts?", CVAR_PreInit);
VCvarB clip_frustum("clip_frustum", true, "Clip geometry with frustum?", CVAR_PreInit);
VCvarB clip_frustum_mirror("clip_frustum_mirror", true, "Clip mirrored geometry with frustum?", CVAR_PreInit);
VCvarB clip_frustum_init_range("clip_frustum_init_range", true, "Init clipper range with frustum?", CVAR_PreInit);
VCvarB clip_frustum_bsp("clip_frustum_bsp", true, "Clip BSP geometry with frustum?", CVAR_PreInit); // sometimes this glitches
//VCvarI clip_frustum_check_mask("clip_frustum_check_mask", TFrustum::LeftBit|TFrustum::RightBit|TFrustum::BackBit, "Which frustum planes we should check?", CVAR_PreInit);
//VCvarI clip_frustum_check_mask("clip_frustum_check_mask", "19", "Which frustum planes we should check?", CVAR_PreInit);
VCvarI clip_frustum_check_mask("clip_frustum_check_mask", "255", "Which frustum planes we should check?", CVAR_PreInit);

static VCvarB clip_skip_slopes_1side("clip_skip_slopes_1side", false, "Skip clipping with one-sided slopes?", CVAR_PreInit);

static VCvarB clip_height("clip_height", true, "Clip with top and bottom frustum?", CVAR_PreInit);
static VCvarB clip_midsolid("clip_midsolid", true, "Clip with solid midtex?", CVAR_PreInit);

static VCvarB clip_use_transfers("clip_use_transfers", false, "Use transfer sectors to clip?", CVAR_PreInit);

VCvarB clip_use_1d_clipper("clip_use_1d_clipper", true, "Use 1d clipper?", CVAR_PreInit);

VCvarB dbg_clip_dump_added_ranges("dbg_clip_dump_added_ranges", false, "Dump added ranges in 1D clipper?", CVAR_PreInit);
VCvarB dbg_clip_dump_sub_checks("dbg_clip_dump_sub_checks", false, "Dump subsector checks in 1D clipper?", CVAR_PreInit);


// ////////////////////////////////////////////////////////////////////////// //
/*
  // Lines with stacked sectors must never block!
  if (backsector->portals[sector_t::ceiling] != NULL || backsector->portals[sector_t::floor] != NULL ||
      frontsector->portals[sector_t::ceiling] != NULL || frontsector->portals[sector_t::floor] != NULL)
  {
    return false;
  }
*/


//==========================================================================
//
//  CopyPlaneIfValid
//
//==========================================================================
static inline bool CopyPlaneIfValid (TPlane *dest, const TPlane *source, const TPlane *opp) {
  bool copy = false;

  // if the planes do not have matching slopes, then always copy them
  // because clipping would require creating new sectors
  if (source->normal != dest->normal) {
    copy = true;
  } else if (opp->normal != -dest->normal) {
    if (source->dist < dest->dist) copy = true;
  } else if (source->dist < dest->dist && source->dist > -opp->dist) {
    copy = true;
  }

  if (copy) *(TPlane *)dest = *(TPlane *)source;

  return copy;
}


//==========================================================================
//
//  CopyHeight
//
//==========================================================================
static inline void CopyHeight (const sector_t *sec, TPlane *fplane, TPlane *cplane, int *fpic, int *cpic) {
  *cpic = sec->ceiling.pic;
  *fpic = sec->floor.pic;
  *fplane = *(TPlane *)&sec->floor;
  *cplane = *(TPlane *)&sec->ceiling;

  if (clip_use_transfers) {
    // check transferred (k8: do we need more checks here?)
    const sector_t *hs = sec->heightsec;
    if (!hs) return;
    if ((hs->SectorFlags&sector_t::SF_IgnoreHeightSec) != 0) return;

    if (hs->SectorFlags&sector_t::SF_ClipFakePlanes) {
      if (!CopyPlaneIfValid(fplane, &hs->floor, &sec->ceiling)) {
        if (hs->SectorFlags&sector_t::SF_FakeFloorOnly) return;
      }
      *fpic = hs->floor.pic;
      if (!CopyPlaneIfValid(cplane, &hs->ceiling, &sec->floor)) {
        return;
      }
      *cpic = hs->ceiling.pic;
    } else {
      if (hs->SectorFlags&sector_t::SF_FakeCeilingOnly) {
        *cplane = *(TPlane *)&hs->ceiling;
      } else if (hs->SectorFlags&sector_t::SF_FakeFloorOnly) {
        *fplane = *(TPlane *)&hs->floor;
      } else {
        *cplane = *(TPlane *)&hs->ceiling;
        *fplane = *(TPlane *)&hs->floor;
      }
    }
  }
}


//==========================================================================
//
//  IsGoodSegForPoly
//
//==========================================================================
static inline bool IsGoodSegForPoly (const VViewClipper &clip, const seg_t *seg) {
  const line_t *ldef = seg->linedef;
  if (!ldef) return true;

  if (ldef->flags&ML_3DMIDTEX) return true; // 3dmidtex never blocks anything
  if ((ldef->flags&ML_TWOSIDED) == 0) return true; // one-sided wall always blocks everything

  // mirrors and horizons always block the view
  switch (ldef->special) {
    case LNSPEC_LineHorizon:
    case LNSPEC_LineMirror:
      return true;
  }

  auto fsec = ldef->frontsector;
  auto bsec = ldef->backsector;

  if (fsec == bsec) return false; // self-referenced sector

  int fcpic, ffpic;
  TPlane ffplane, fcplane;

  CopyHeight(fsec, &ffplane, &fcplane, &ffpic, &fcpic);
  if (ffplane.normal.z != 1.0f || fcplane.normal.z != -1.0f) return false;

  if (bsec) {
    TPlane bfplane, bcplane;
    int bcpic, bfpic;
    CopyHeight(bsec, &bfplane, &bcplane, &bfpic, &bcpic);
    if (bfplane.normal.z != 1.0f || bcplane.normal.z != -1.0f) return false;
  }

  return true;
}


// debug
#ifdef XXX_CLIPPER_DEBUG
static const VLevel *currLevel;
#endif


//==========================================================================
//
//  VViewClipper::IsSegAClosedSomething
//
//  prerequisite: has front and back sectors, has linedef
//
//==========================================================================
bool VViewClipper::IsSegAClosedSomething (/*const VViewClipper &clip*/const TFrustum *Frustum, const seg_t *seg, const TVec *lorg, const float *lrad) {
  if (!clip_platforms) return false;
  if (!seg->backsector) return false;

  if (!seg->partner || seg->partner == seg) return false;

  const line_t *ldef = seg->linedef;

  if (ldef->alpha < 1.0f) return false; // skip translucent walls
  //k8: this was checked by caller
  //if (ldef->flags&ML_3DMIDTEX) return false; // 3dmidtex never blocks anything
  //k8: this was checked by caller
  //if ((ldef->flags&ML_TWOSIDED) == 0) return true; // one-sided wall always blocks everything
  // just in case
  if ((ldef->flags&(ML_TWOSIDED|ML_3DMIDTEX)) != ML_TWOSIDED) return true;

  // mirrors and horizons always block the view
  switch (ldef->special) {
    case LNSPEC_LineHorizon:
    case LNSPEC_LineMirror:
      return true;
  }

  const sector_t *fsec = ldef->frontsector;
  const sector_t *bsec = ldef->backsector;

  if (fsec == bsec) return false; // self-referenced sector

  // only apply this to sectors without slopes
  /*k8: why?
  if (ffplane.normal.z == 1.0f && bfplane.normal.z == 1.0f &&
      fcplane.normal.z == -1.0f && bcplane.normal.z == -1.0f)
  */
  {
    bool hasTopTex = !GTextureManager.IsEmptyTexture(seg->sidedef->TopTexture);
    bool hasBotTex = !GTextureManager.IsEmptyTexture(seg->sidedef->BottomTexture);
    bool hasMidTex = !GTextureManager.IsEmptyTexture(seg->sidedef->MidTexture);
    if (hasTopTex || // a seg without top texture isn't a door
        hasBotTex || // a seg without bottom texture isn't an elevator/plat
        hasMidTex) // a seg without mid texture isn't a polyobj door
    {
      /*
      const TVec vv1 = *ldef->v1;
      const TVec vv2 = *ldef->v2;
      */

      int fcpic, ffpic;
      int bcpic, bfpic;

      TPlane ffplane, fcplane;
      TPlane bfplane, bcplane;

      CopyHeight(fsec, &ffplane, &fcplane, &ffpic, &fcpic);
      CopyHeight(bsec, &bfplane, &bcplane, &bfpic, &bcpic);


      const TVec vv1 = *seg->v1;
      const TVec vv2 = *seg->v2;

      const float frontcz1 = fcplane.GetPointZ(vv1);
      const float frontcz2 = fcplane.GetPointZ(vv2);
      //check(frontcz1 == frontcz2);
      const float frontfz1 = ffplane.GetPointZ(vv1);
      const float frontfz2 = ffplane.GetPointZ(vv2);
      //check(frontfz1 == frontfz2);

      const float backcz1 = bcplane.GetPointZ(vv1);
      const float backcz2 = bcplane.GetPointZ(vv2);
      //check(backcz1 == backcz2);
      const float backfz1 = bfplane.GetPointZ(vv1);
      const float backfz2 = bfplane.GetPointZ(vv2);
      //check(backfz1 == backfz2);

      if (clip_midsolid && hasMidTex) {
        const bool midSolid = (hasMidTex && !GTextureManager[seg->sidedef->MidTexture]->isTransparent());
        if (midSolid) {
          const sector_t *sec = (!seg->side ? ldef->backsector : ldef->frontsector);
          //const sector_t *secb = (seg->side ? ldef->backsector : ldef->frontsector);
          // check if we have only one region
          if (!sec->botregion->next) {
            VTexture *MTex = GTextureManager(seg->sidedef->MidTexture);
            // here we should check if midtex covers the whole height, as it is not tiled vertically
            {
              const float mheight = MTex->GetScaledHeight();
              float toffs;
              if (ldef->flags&ML_DONTPEGBOTTOM) {
                // bottom of texture at bottom
                toffs = sec->floor.TexZ+mheight;
                //GCon->Logf("000");
              } else if (ldef->flags&ML_DONTPEGTOP) {
                // top of texture at top of top region
                toffs = sec->topregion->eceiling->TexZ;
                //GCon->Logf("001");
              } else {
                // top of texture at top
                toffs = sec->ceiling.TexZ;
                //GCon->Logf("002");
              }
              toffs *= MTex->TScale;
              toffs += seg->sidedef->MidRowOffset*(MTex->bWorldPanning ? MTex->TScale : 1.0f);
              //GCon->Logf("  TScale:%f; MidRowOffset=%f; toffs=%f; mheight=%f", MTex->TScale, seg->sidedef->MidRowOffset, toffs, mheight);
              //GCon->Logf("  fsec: %f : %f", sec->floor.maxz, sec->ceiling.minz);
              float floorz, ceilz;
              if (sec == fsec) {
                floorz = MIN(frontfz1, frontfz2);
                ceilz = MAX(frontcz1, frontcz2);
                //GCon->Logf("  Xsec: %f : %f", MIN(frontfz1, frontfz2), MAX(frontcz1, frontcz2));
              } else {
                floorz = MIN(backfz1, backfz2);
                ceilz = MAX(backcz1, backcz2);
                //GCon->Logf("  Xsec: %f : %f", MIN(backfz1, backfz2), MAX(backcz1, backcz2));
              }
              //GCon->Logf("  bsec: %f : %f", secb->floor.maxz, secb->ceiling.minz);
              //if (toffs >= sec->ceiling.minz && toffs-mheight <= sec->floor.maxz) return true; // fully covered
              if (toffs >= ceilz && toffs-mheight <= floorz) return true; // fully covered
            }
          }
        }
      }

      // taken from Zandronum
      // now check for closed sectors

      // closed door
      if (backcz1 <= frontfz1 && backcz2 <= frontfz2) {
        // preserve a kind of transparent door/lift special effect:
        if (!hasTopTex) return false;
        // properly render skies (consider door "open" if both ceilings are sky):
        if (bcpic == skyflatnum && fcpic == skyflatnum) return false;
        return true;
      }

      if (frontcz1 <= backfz1 && frontcz2 <= backfz2) {
        // preserve a kind of transparent door/lift special effect:
        if (!hasBotTex) return false;
        // properly render skies (consider door "open" if both ceilings are sky):
        if (bcpic == skyflatnum && fcpic == skyflatnum) return false;
        return true;
      }

      // if door is closed because back is shut
      if (backcz1 <= backfz1 && backcz2 <= backfz2) {
        // preserve a kind of transparent door/lift special effect:
        if (backcz1 < frontcz1 || backcz2 < frontcz2) {
          if (!hasTopTex) return false;
        }
        if (backfz1 > frontfz1 || backfz2 > frontfz2) {
          if (!hasBotTex) return false;
        }
        // properly render skies
        if (bcpic == skyflatnum && fcpic == skyflatnum) return false;
        if (bfpic == skyflatnum && ffpic == skyflatnum) return false;
        return true;
      }

#ifdef XXX_CLIPPER_DEBUG_X
        if (currLevel) {
          const unsigned lnum = (unsigned)(ptrdiff_t)(ldef-currLevel->Lines);
          const unsigned snum = (unsigned)(ptrdiff_t)(seg-currLevel->Segs);
          if (lnum == 9) {
            //if (snum == 2019) return false;
            GCon->Logf("=== LINEDEF %u; seg %u ===", lnum, snum);
            GCon->Logf("  ldef sectors: %u : %u", (unsigned)(ptrdiff_t)(ldef->frontsector-currLevel->Sectors), (unsigned)(ptrdiff_t)(ldef->backsector-currLevel->Sectors));
            GCon->Logf("  seg  sectors: %u : %u", (unsigned)(ptrdiff_t)(seg->front_sub->sector-currLevel->Sectors), (unsigned)(ptrdiff_t)(seg->partner->front_sub->sector-currLevel->Sectors));
            return true;
          }
        }
#endif

      if (clip_height && /*clip_frustum &&*/ (hasTopTex || hasBotTex) &&
          seg->partner && seg->partner != seg &&
          seg->partner->front_sub && seg->partner->front_sub != seg->front_sub &&
          (!hasMidTex || !GTextureManager[seg->sidedef->MidTexture]->isTransparent()))
      {
        /*
        const side_t *otherside = seg->partner->sidedef;

        if (GTextureManager.IsEmptyTexture(otherside->TopTexture) &&
            GTextureManager.IsEmptyTexture(otherside->BottomTexture) &&
            GTextureManager.IsEmptyTexture(otherside->MidTexture))
        {
          return false;
        }
        */

#ifdef XXX_CLIPPER_DEBUG
        if (currLevel) {
          const unsigned lnum = (unsigned)(ptrdiff_t)(ldef-currLevel->Lines);
          const unsigned snum = (unsigned)(ptrdiff_t)(seg-currLevel->Segs);
          if (/*lnum == 21 || lnum == 41 || lnum == 125*/snum == 2013) {
            //if (snum == 2019) return false;
            GCon->Logf("=== LINEDEF %u; seg %u ===", lnum, snum);
            GCon->Logf("  ldef sectors: %u : %u", (unsigned)(ptrdiff_t)(ldef->frontsector-currLevel->Sectors), (unsigned)(ptrdiff_t)(ldef->backsector-currLevel->Sectors));
            GCon->Logf("  seg  sectors: %u : %u", (unsigned)(ptrdiff_t)(seg->front_sub->sector-currLevel->Sectors), (unsigned)(ptrdiff_t)(seg->partner->front_sub->sector-currLevel->Sectors));
            return true;
          }
        }
#endif
        //return true;
        // here we can check if midtex is in frustum; if it doesn't,
        // we can add this seg to clipper.
        // this way, we can clip alot of things when camera looks at
        // floor/ceiling, and we can clip away too high/low windows.
        //const TFrustum &Frustum = clip.GetFrustum();
        bool lcheck = false;
        if (lorg) {
          if (!ldef->SphereTouches(*lorg, *lrad)) return true;
          lcheck = true;
        }
        if ((Frustum && Frustum->isValid()) || lcheck) {
          // create bounding box for linked subsector
          const subsector_t *bss = seg->partner->front_sub;
          const sector_t *bssec = bss->sector;
          const TVec sv1 = *seg->v1;
          const TVec sv2 = *seg->v2;

          const float bfz1 = bssec->floor.GetPointZ(sv1);
          const float bfz2 = bssec->floor.GetPointZ(sv2);
          const float bcz1 = bssec->ceiling.GetPointZ(sv1);
          const float bcz2 = bssec->ceiling.GetPointZ(sv2);

          if (Frustum && Frustum->isValid()) {
            TVec verts[4];
            verts[0] = TVec(sv1.x, sv1.y, bfz1);
            verts[1] = TVec(sv1.x, sv1.y, bcz1);
            verts[2] = TVec(sv2.x, sv2.y, bcz2);
            verts[3] = TVec(sv2.x, sv2.y, bfz2);
            if (MIN(verts[0].z, verts[3].z) >= MAX(verts[1].z, verts[2].z)) return true; // definitely closed
            // check (only top, bottom, and back)
            if (!Frustum->checkVerts(verts, 4, TFrustum::TopBit|TFrustum::BottomBit/*|TFrustum::BackBit*/)) {
              /*
              if (!hasMidTex) GCon->Logf("NO MIDTEX");
              else GCon->Logf("SOLID for '%s': %s", *GTextureManager[seg->sidedef->MidTexture]->Name, (GTextureManager[seg->sidedef->MidTexture]->isTransparent() ? "ona" : "tan"));
              GCon->Logf("verts: (%f,%f,%f)-(%f,%f,%f)-(%f,%f,%f)-(%f,%f,%f)",
                verts[0].x, verts[0].y, verts[0].z,
                verts[1].x, verts[1].y, verts[1].z,
                verts[2].x, verts[2].y, verts[2].z,
                verts[3].x, verts[3].y, verts[3].z);
              GCon->Logf("front: floor=(%f,%f); ceil=(%f,%f)", frontfz1, frontfz2, frontcz1, frontcz2);
              GCon->Logf("back : floor=(%f,%f); ceil=(%f,%f)", backfz1, backfz2, backcz1, backcz2);
              */
              return true;
            }
          }

          // check if light can see midtex
          if (lcheck) {
            // min
            float bbox[6];
            bbox[0] = MIN(sv1.x, sv2.x);
            bbox[1] = MIN(sv1.y, sv2.y);
            bbox[2] = MIN(bfz1, bfz2);
            // max
            bbox[3+0] = MAX(sv1.x, sv2.x);
            bbox[3+1] = MAX(sv1.y, sv2.y);
            bbox[3+2] = MAX(bcz1, bcz2);
            FixBBoxZ(bbox);

            if (bbox[2] >= bbox[3+2]) return true; // definitely closed

            if (!CheckSphereVsAABB(bbox, *lorg, *lrad)) return true; // cannot see midtex, can block
          }
        }
      }
      // still unsure; check if midtex is transparent
      //if (midSolid) return true; // texture is not transparent, block
    }
  }
#if 0
   else {
    // sloped
    /*
    if (((fsec->floor.maxz > bsec->ceiling.minz && fsec->ceiling.maxz < bsec->floor.minz) ||
         (fsec->floor.minz > bsec->ceiling.maxz && fsec->ceiling.minz < bsec->floor.maxz)) ||
        ((bsec->floor.maxz > fsec->ceiling.minz && bsec->ceiling.maxz < fsec->floor.minz) ||
         (bsec->floor.minz > fsec->ceiling.maxz && bsec->ceiling.minz < fsec->floor.maxz)))
    {
      return true;
    }
    */
  }
#endif

  return false;
}



//==========================================================================
//
//  VViewClipper::VViewClipper
//
//==========================================================================
VViewClipper::VViewClipper ()
  : FreeClipNodes(nullptr)
  , ClipHead(nullptr)
  , ClipTail(nullptr)
  , ClearClipNodesCalled(false)
{
}


//==========================================================================
//
//  VViewClipper::~VViewClipper
//
//==========================================================================
VViewClipper::~VViewClipper () {
  ClearClipNodes(TVec(), nullptr);
  VClipNode *Node = FreeClipNodes;
  while (Node) {
    VClipNode *Next = Node->Next;
    delete Node;
    Node = Next;
  }
}


//==========================================================================
//
//  VViewClipper::NewClipNode
//
//==========================================================================
VViewClipper::VClipNode *VViewClipper::NewClipNode () {
  VClipNode *res = FreeClipNodes;
  if (res) FreeClipNodes = res->Next; else res = new VClipNode();
  return res;
}


//==========================================================================
//
//  VViewClipper::RemoveClipNode
//
//==========================================================================
void VViewClipper::RemoveClipNode (VViewClipper::VClipNode *Node) {
  if (Node->Next) Node->Next->Prev = Node->Prev;
  if (Node->Prev) Node->Prev->Next = Node->Next;
  if (Node == ClipHead) ClipHead = Node->Next;
  if (Node == ClipTail) ClipTail = Node->Prev;
  Node->Next = FreeClipNodes;
  FreeClipNodes = Node;
}


//==========================================================================
//
//  VViewClipper::ClearClipNodes
//
//==========================================================================
void VViewClipper::ClearClipNodes (const TVec &AOrigin, VLevel *ALevel, float aradius) {
  if (ClipHead) {
    ClipTail->Next = FreeClipNodes;
    FreeClipNodes = ClipHead;
  }
  ClipHead = nullptr;
  ClipTail = nullptr;
  Origin = AOrigin;
  Level = ALevel;
#ifdef XXX_CLIPPER_DEBUG
  currLevel = ALevel;
#endif
  ClipResetFrustumPlanes();
  ClearClipNodesCalled = true;
  Radius = aradius;
}


//==========================================================================
//
//  VViewClipper::ClipResetFrustumPlanes
//
//==========================================================================
void VViewClipper::ClipResetFrustumPlanes () {
  Frustum.clear();
}


void VViewClipper::ClipInitFrustumPlanes (const TAVec &viewangles, const TVec &viewforward, const TVec &viewright, const TVec &viewup,
                                          const float fovx, const float fovy)
{
  if (clip_frustum && !viewright.z && isFiniteF(fovy) && fovy != 0 && isFiniteF(fovx) && fovx != 0) {
    // no view roll, create frustum
    TClipBase cb(fovx, fovy);
    TFrustumParam fp(Origin, viewangles, viewforward, viewright, viewup);
    Frustum.setup(cb, fp, true); // create back plane, no far plane
    /*
    Frustum.planes[TFrustum::Left].invalidate();
    Frustum.planes[TFrustum::Right].invalidate();
    Frustum.planes[TFrustum::Top].invalidate();
    Frustum.planes[TFrustum::Bottom].invalidate();
    Frustum.planes[TFrustum::Back].invalidate();
    Frustum.planes[TFrustum::Forward].invalidate();
    */
    /*
    // sanity check
    const TClipPlane *cp = &Frustum.planes[0];
    for (unsigned i = 0; i < 6; ++i, ++cp) {
      if (!cp->clipflag) continue; // don't need to clip against it
      if (cp->PointOnSide(Origin+viewforward)) GCon->Logf("invalid frustum plane #%u", i); // viewer is in back side or on plane (k8: why check this?)
    }
    */
  } else {
    ClipResetFrustumPlanes();
  }
  //ClipResetFrustumPlanes();
}


//==========================================================================
//
//  VViewClipper::ClipInitFrustumPlanes
//
//==========================================================================
void VViewClipper::ClipInitFrustumPlanes (const TAVec &viewangles, const float fovx, const float fovy) {
  TVec f, r, u;
  AngleVectors(viewangles, f, r, u);
  ClipInitFrustumPlanes(viewangles, f, r, u, fovx, fovy);
}


//==========================================================================
//
//  VViewClipper::ClipInitFrustumRange
//
//==========================================================================
void VViewClipper::ClipInitFrustumRange (const TAVec &viewangles, const TVec &viewforward,
                                         const TVec &viewright, const TVec &viewup,
                                         const float fovx, const float fovy)
{
  check(ClipIsEmpty());

  ClipInitFrustumPlanes(viewangles, viewforward, viewright, viewup, fovx, fovy);

  //if (viewforward.z > 0.9f || viewforward.z < -0.9f) return; // looking up or down, can see behind
  /*
  if (viewforward.z >= 0.985f || viewforward.z <= -0.985f) {
    // looking up or down, can see behind
    //return;
  }
  */

  ClearClipNodesCalled = false;
  if (dbg_clip_dump_added_ranges) GCon->Log("==== ClipInitFrustumRange ===");

  if (!clip_frustum) return;
  if (!clip_frustum_init_range) return;

#ifdef VAVOOM_CLIPPER_USE_REAL_ANGLES
# if 0
    float fov = RAD2DEGF(atanf(fovx)*2.0f);
    float frangle;

    /*
    float tilt = 45.0f-fabsf(AngleMod180(viewangles.pitch)-90.0f);

    if (tilt > 90.0f) {
      frangle = 180.0f;
    } else {
      float range = 64.0f/(fov-(/ *widescreen* /0 ? -4.0f : 10.0f));
      if (range > 1.0f) range = 1.0f;
      float floatangle = (float)tilt*range;
      frangle = 270.0f-floatangle;
    }
    GCon->Logf("fovx=%f; fov=%f; frangle=%f; tilt=%f; pitch=%f", fovx, fov, frangle, tilt, viewangles.pitch);
    */
    float tilt = AngleMod180(fabsf(viewangles.pitch));
    if (tilt > 90.0f) return;
    float range = 64.0f/(fov-(/*widescreen*/0 ? -4.0f : 10.0f));
    if (range > 1.0f) range = 1.0f;
    float floatangle = tilt*range;
    //frangle = fov+floatangle;
    frangle = fov+tilt;

    float a0 = AngleMod(viewangles.yaw-frangle*0.5f);
    float a1 = AngleMod(viewangles.yaw+frangle*0.5f);

    //GCon->Logf("fovx=%f; fov=%f; tilt=%f; pitch=%f; range=%f; floatangle=%f; frangle=%f; (%f : %f); %f", fovx, fov, tilt, viewangles.pitch, range, floatangle, frangle, a0, a1, viewangles.yaw);

    if (a0 > a1) {
      ClipHead = NewClipNode();
      ClipTail = ClipHead;
      ClipHead->From = a1;
      ClipHead->To = a0;
      ClipHead->Prev = nullptr;
      ClipHead->Next = nullptr;
    } else {
      ClipHead = NewClipNode();
      ClipHead->From = MIN_ANGLE;
      ClipHead->To = a0;

      ClipTail = NewClipNode();
      ClipTail->From = a1;
      ClipTail->To = MAX_ANGLE;

      ClipHead->Prev = nullptr;
      ClipHead->Next = ClipTail;
      ClipTail->Prev = ClipHead;
      ClipTail->Next = nullptr;
    }
# else
  VFloat d1 = (VFloat)0;
  VFloat d2 = (VFloat)0;

  TVec Pts[4];
  TVec TransPts[4];
  Pts[0] = TVec(fovx, fovy, 1.0f);
  Pts[1] = TVec(fovx, -fovy, 1.0f);
  Pts[2] = TVec(-fovx, fovy, 1.0f);
  Pts[3] = TVec(-fovx, -fovy, 1.0f);
  TVec clipforward = TVec(viewforward.x, viewforward.y, 0.0f);
  //k8: i don't think that we need to normalize it, but...
  //clipforward.normaliseInPlace();

  for (unsigned i = 0; i < 4; ++i) {
    TransPts[i].x = VSUM3(Pts[i].x*viewright.x, Pts[i].y*viewup.x, /*Pts[i].z* */viewforward.x);
    TransPts[i].y = VSUM3(Pts[i].x*viewright.y, Pts[i].y*viewup.y, /*Pts[i].z* */viewforward.y);
    TransPts[i].z = VSUM3(Pts[i].x*viewright.z, Pts[i].y*viewup.z, /*Pts[i].z* */viewforward.z);

    if (DotProduct(TransPts[i], clipforward) <= 0.01f) { // was 0.0f
#ifdef VV_CLIPPER_USE_DIAMONDS
      GCon->Log("diamond hit!");
      // use 180 degree clip, because why not?
      d1 = -90;
      d2 = 90;
      break;
#else
      // player can see behind, use back frustum plane to clip
      return;
#endif
    }

    VFloat a = VVC_matan(TransPts[i].y, TransPts[i].x);
    if (a < (VFloat)0) a += (VFloat)360;
    VFloat d = VVC_AngleMod180(a-viewangles.yaw);

    if (d1 > d) d1 = d;
    if (d2 < d) d2 = d;
  }

  VFloat a1 = VVC_AngleMod(viewangles.yaw+d1);
  VFloat a2 = VVC_AngleMod(viewangles.yaw+d2);

  if (a1 > a2) {
    if (dbg_clip_dump_added_ranges) GCon->Logf("  clip frustum: %f : %f", a2, a1);
    ClipHead = NewClipNode();
    ClipTail = ClipHead;
    ClipHead->From = a2;
    ClipHead->To = a1;
    ClipHead->Prev = nullptr;
    ClipHead->Next = nullptr;
  } else {
    if (dbg_clip_dump_added_ranges) {
      GCon->Logf("  clip frustum: %f : %f", 0.0, a1);
      GCon->Logf("  clip frustum: %f : %f", a1, 360.0);
    }
    ClipHead = NewClipNode();
    ClipHead->From = 0;
    ClipHead->To = a1;
    ClipTail = NewClipNode();
    ClipTail->From = a2;
    ClipTail->To = (VFloat)360;
    ClipHead->Prev = nullptr;
    ClipHead->Next = ClipTail;
    ClipTail->Prev = ClipHead;
    ClipTail->Next = nullptr;
  }
# endif
#else
  TVec fwd;
  AngleVector(TAVec(0, viewangles.yaw-90), fwd);
  TVec v0 = Origin+fwd*42;
  AngleVector(TAVec(0, viewangles.yaw+90), fwd);
  TVec v1 = Origin+fwd*42;
  check(!ClipHead);
  check(!ClipTail);
  AddClipRangeAngle(PointToClipAngle(v0), PointToClipAngle(v1));
#endif
}


//==========================================================================
//
//  VViewClipper::ClipInitFrustumRange
//
//==========================================================================
void VViewClipper::ClipInitFrustumRange (const TAVec &viewangles, const float fovx, const float fovy) {
  TVec f, r, u;
  AngleVectors(viewangles, f, r, u);
  ClipInitFrustumRange(viewangles, f, r, u, fovx, fovy);
}


//==========================================================================
//
//  VViewClipper::ClipToRanges
//
//==========================================================================
void VViewClipper::ClipToRanges (const VViewClipper &Range) {
  if (&Range == this) return; // just in case

  if (!Range.ClipHead) {
    // no ranges, everything is clipped away
    //DoAddClipRange((VFloat)0, (VFloat)360);
    /*
    // remove free clip nodes
    VClipNode *Node = FreeClipNodes;
    while (Node) {
      VClipNode *Next = Node->Next;
      delete Node;
      Node = Next;
    }
    FreeClipNodes = nullptr;
    // remove used clip nodes
    Node = ClipHead;
    while (Node) {
      VClipNode *Next = Node->Next;
      delete Node;
      Node = Next;
    }
    */
    // remove used clip nodes
    if (ClipHead) {
      ClipTail->Next = FreeClipNodes;
      FreeClipNodes = ClipHead;
    }
    ClipHead = ClipTail = nullptr;
    // add new clip node
    ClipHead = NewClipNode();
    ClipTail = ClipHead;
    ClipHead->From = MIN_ANGLE;
    ClipHead->To = MAX_ANGLE;
    ClipHead->Prev = nullptr;
    ClipHead->Next = nullptr;
    return;
  }

  // add head and tail ranges
  if (Range.ClipHead->From > MIN_ANGLE) DoAddClipRange(MIN_ANGLE, Range.ClipHead->From);
  if (Range.ClipTail->To < MAX_ANGLE) DoAddClipRange(Range.ClipTail->To, MAX_ANGLE);

  // add middle ranges
  for (VClipNode *N = Range.ClipHead; N->Next; N = N->Next) DoAddClipRange(N->To, N->Next->From);
}


//==========================================================================
//
//  VViewClipper::Dump
//
//==========================================================================
void VViewClipper::Dump () const {
  GCon->Logf("=== CLIPPER: origin=(%f,%f,%f) ===", Origin.x, Origin.y, Origin.z);
#ifdef VV_CLIPPER_FULL_CHECK
  GCon->Logf(" full: %d", (int)ClipIsFull());
#endif
  GCon->Logf(" empty: %d", (int)ClipIsEmpty());
  for (VClipNode *node = ClipHead; node; node = node->Next) {
    GCon->Logf("  node: (%f : %f)", node->From, node->To);
  }
}


//==========================================================================
//
//  VViewClipper::DoAddClipRange
//
//==========================================================================
void VViewClipper::DoAddClipRange (VFloat From, VFloat To) {
#ifndef VAVOOM_CLIPPER_USE_REAL_ANGLES
  if (From < (VFloat)(VFloat)0) From = (VFloat)0; else if (From >= (VFloat)360) From = (VFloat)360;
  if (To < (VFloat)0) To = (VFloat)0; else if (To >= (VFloat)360) To = (VFloat)360;
  //check(From <= To || (From > To && To == (VFloat)360));
#endif

  if (ClipIsEmpty()) {
    ClipHead = NewClipNode();
    ClipTail = ClipHead;
    ClipHead->From = From;
    ClipHead->To = To;
    ClipHead->Prev = nullptr;
    ClipHead->Next = nullptr;
    return;
  }

  for (VClipNode *Node = ClipHead; Node; Node = Node->Next) {
    if (Node->To < From) continue; // before this range

    if (To < Node->From) {
      // insert a new clip range before current one
#if defined(XXX_CLIPPER_MANY_DUMPS)
    if (dbg_clip_dump_added_ranges) GCon->Logf(" +++ inserting (%f : %f) before (%f : %f)", From, To, Node->From, Node->To);
#endif
      VClipNode *N = NewClipNode();
      N->From = From;
      N->To = To;
      N->Prev = Node->Prev;
      N->Next = Node;

      if (Node->Prev) Node->Prev->Next = N; else ClipHead = N;
      Node->Prev = N;

      return;
    }

    if (Node->From <= From && Node->To >= To) {
      // it contains this range
#if defined(XXX_CLIPPER_MANY_DUMPS)
      if (dbg_clip_dump_added_ranges) GCon->Logf(" +++ hidden (%f : %f) with (%f : %f)", From, To, Node->From, Node->To);
#endif
      return;
    }

    if (From < Node->From) {
      // extend start of the current range
#if defined(XXX_CLIPPER_MANY_DUMPS)
      if (dbg_clip_dump_added_ranges) GCon->Logf(" +++ using (%f : %f) to extend head of (%f : %f)", From, To, Node->From, Node->To);
#endif
      Node->From = From;
    }

    if (To <= Node->To) {
      // end is included, so we are done here
#if defined(XXX_CLIPPER_MANY_DUMPS)
      if (dbg_clip_dump_added_ranges) GCon->Logf(" +++ hidden (end) (%f : %f) with (%f : %f)", From, To, Node->From, Node->To);
#endif
      return;
    }

    // merge with following nodes if needed
    while (Node->Next && Node->Next->From <= To) {
#if defined(XXX_CLIPPER_MANY_DUMPS)
      if (dbg_clip_dump_added_ranges) GCon->Logf(" +++ merger (%f : %f) eat (%f : %f)", From, To, Node->From, Node->To);
#endif
      Node->To = Node->Next->To;
      RemoveClipNode(Node->Next);
    }

    if (To > Node->To) {
      // extend end
#if defined(XXX_CLIPPER_MANY_DUMPS)
      if (dbg_clip_dump_added_ranges) GCon->Logf(" +++ using (%f : %f) to extend end of (%f : %f)", From, To, Node->From, Node->To);
#endif
      Node->To = To;
    }

    // we are done here
    return;
  }

#if defined(XXX_CLIPPER_MANY_DUMPS)
  if (dbg_clip_dump_added_ranges) GCon->Logf(" +++ appending (%f : %f) to the end (%f : %f)", From, To, ClipTail->From, ClipTail->To);
#endif
  // if we are here it means it's a new range at the end
  VClipNode *NewTail = NewClipNode();
  NewTail->From = From;
  NewTail->To = To;
  NewTail->Prev = ClipTail;
  NewTail->Next = nullptr;
  ClipTail->Next = NewTail;
  ClipTail = NewTail;
}


//==========================================================================
//
//  VViewClipper::AddClipRange
//
//==========================================================================
void VViewClipper::AddClipRangeAngle (const VFloat From, const VFloat To) {
  if (From > To) {
    DoAddClipRange(MIN_ANGLE, To);
    DoAddClipRange(From, MAX_ANGLE);
  } else {
    DoAddClipRange(From, To);
  }
}


//==========================================================================
//
//  VViewClipper::DoRemoveClipRange
//
//  NOT TESTED!
//
//==========================================================================
void VViewClipper::DoRemoveClipRange (VFloat From, VFloat To) {
  if (ClipHead) {
    // check to see if range contains any old ranges
    VClipNode *node = ClipHead;
    while (node != nullptr && node->From < To) {
      if (node->From >= From && node->To <= To) {
        VClipNode *temp = node;
        node = node->Next;
        RemoveClipNode(temp);
      } else {
        node = node->Next;
      }
    }

    // check to see if range overlaps a range (or possibly 2)
    node = ClipHead;
    while (node) {
      if (node->From >= From && node->From <= To) {
        node->From = To;
        break;
      } else if (node->To >= From && node->To <= To) {
        node->To = From;
      } else if (node->From < From && node->To > To) {
        // split node
        VClipNode *temp = NewClipNode();
        temp->From = To;
        temp->To = node->To;
        node->To = From;
        temp->Next = node->Next;
        temp->Prev = node;
        node->Next = temp;
        if (temp->Next) temp->Next->Prev = temp;
        break;
      }
      node = node->Next;
    }
  }
}


//==========================================================================
//
//  VViewClipper::RemoveClipRange
//
//==========================================================================
void VViewClipper::RemoveClipRangeAngle (VFloat From, VFloat To) {
  if (From > To) {
    DoRemoveClipRange(MIN_ANGLE, To);
    DoRemoveClipRange(From, MAX_ANGLE);
  } else {
    DoRemoveClipRange(From, To);
  }
}


//==========================================================================
//
//  VViewClipper::DoIsRangeVisible
//
//==========================================================================
bool VViewClipper::DoIsRangeVisible (const VFloat From, const VFloat To) const {
  for (const VClipNode *N = ClipHead; N; N = N->Next) {
    if (From >= N->From && To <= N->To) return false;
  }
  return true;
}


//==========================================================================
//
//  VViewClipper::IsRangeVisible
//
//==========================================================================
bool VViewClipper::IsRangeVisibleAngle (const VFloat From, const VFloat To) const {
  if (From > To) return (DoIsRangeVisible(MIN_ANGLE, To) || DoIsRangeVisible(From, MAX_ANGLE));
  return DoIsRangeVisible(From, To);
}


//==========================================================================
//
//  CreateBBVerts
//
//  returns `true` if no check required (origin in is a box)
//
//==========================================================================
inline static bool CreateBBVerts (TVec &v1, TVec &v2, const float bbox[6], const TVec &origin) {
  enum { MIN_X, MIN_Y, MIN_Z, MAX_X, MAX_Y, MAX_Z };

#ifndef VAVOOM_CLIPPER_USE_REAL_ANGLES
  static const vuint8 checkcoord[12][4] = {
    { MAX_Y,MIN_X,MAX_X,MIN_Y },
    { MAX_Y,MIN_X,MAX_X,MIN_X },
    { MAX_Y,MIN_Y,MAX_X,MIN_X },
    { MIN_X },
    { MAX_X,MIN_X,MAX_X,MIN_Y },
    { MIN_X },
    { MAX_Y,MIN_Y,MAX_Y,MIN_X },
    { MIN_X },
    { MAX_X,MIN_X,MAX_Y,MIN_Y },
    { MAX_X,MIN_Y,MAX_Y,MIN_Y },
    { MAX_X,MIN_Y,MAX_Y,MIN_X }
  };

  // find the corners of the box that define the edges from current viewpoint
  const float ox = origin.x, oy = origin.y;
  const unsigned boxpos = (ox <= bbox[MIN_X] ? 0 : ox < bbox[MAX_X] ? 1 : 2)+
                          (oy >= bbox[MAX_Y] ? 0 : oy > bbox[MIN_Y] ? 4 : 8);

  if (boxpos == 5) return true;
  check(boxpos != 3 && boxpos != 7);

  const vuint8 *check = checkcoord[boxpos];
  v1 = TVec(bbox[check[0]], bbox[check[1]], 0.0f);
  v2 = TVec(bbox[check[2]], bbox[check[3]], 0.0f);

#else
  if (bbox[0] <= origin.x && bbox[3] >= origin.x &&
      bbox[1] <= origin.y && bbox[4] >= origin.y &&
      (!clip_check_bbox_z || (bbox[2] <= origin.z && bbox[5] >= origin.z)))
  {
    // viewer is inside the box
    return true;
  }

  if (bbox[MIN_X] > origin.x) {
    if (bbox[MIN_Y] > origin.y) {
      v1.x = bbox[MAX_X];
      v1.y = bbox[MIN_Y];
      v2.x = bbox[MIN_X];
      v2.y = bbox[MAX_Y];
    } else if (bbox[MAX_Y] < origin.y) {
      v1.x = bbox[MIN_X];
      v1.y = bbox[MIN_Y];
      v2.x = bbox[MAX_X];
      v2.y = bbox[MAX_Y];
    } else {
      v1.x = bbox[MIN_X];
      v1.y = bbox[MIN_Y];
      v2.x = bbox[MIN_X];
      v2.y = bbox[MAX_Y];
    }
  } else if (bbox[MAX_X] < origin.x) {
    if (bbox[MIN_Y] > origin.y) {
      v1.x = bbox[MAX_X];
      v1.y = bbox[MAX_Y];
      v2.x = bbox[MIN_X];
      v2.y = bbox[MIN_Y];
    } else if (bbox[MAX_Y] < origin.y) {
      v1.x = bbox[MIN_X];
      v1.y = bbox[MAX_Y];
      v2.x = bbox[MAX_X];
      v2.y = bbox[MIN_Y];
    } else {
      v1.x = bbox[MAX_X];
      v1.y = bbox[MAX_Y];
      v2.x = bbox[MAX_X];
      v2.y = bbox[MIN_Y];
    }
  } else {
    if (bbox[MIN_Y] > origin.y) {
      v1.x = bbox[MAX_X];
      v1.y = bbox[MIN_Y];
      v2.x = bbox[MIN_X];
      v2.y = bbox[MIN_Y];
    } else {
      v1.x = bbox[MIN_X];
      v1.y = bbox[MAX_Y];
      v2.x = bbox[MAX_X];
      v2.y = bbox[MAX_Y];
    }
  }
  v1.z = v2.z = 0.0f;
#endif

  return false;
}


//==========================================================================
//
//  VViewClipper::CheckSubsectorFrustum
//
//==========================================================================
int VViewClipper::CheckSubsectorFrustum (const subsector_t *sub, const unsigned mask) const {
  if (!sub || !Frustum.isValid() || !mask) return 1;
  float bbox[6];
  Level->GetSubsectorBBox(sub, bbox);

  if (bbox[0] <= Origin.x && bbox[3] >= Origin.x &&
      bbox[1] <= Origin.y && bbox[4] >= Origin.y &&
      bbox[2] <= Origin.z && bbox[5] >= Origin.z)
  {
    // viewer is inside the box
    return 1;
  }

  // check
  return Frustum.checkBoxEx(bbox, clip_frustum_check_mask&mask);
}


//==========================================================================
//
//  VViewClipper::CheckSegFrustum
//
//==========================================================================
bool VViewClipper::CheckSegFrustum (const seg_t *seg, const unsigned mask) const {
  if (!seg || !seg->front_sub || !Frustum.isValid() || !mask) return true;
  const TVec sv1 = *seg->v1;
  const TVec sv2 = *seg->v2;
  float bbox[6];
  Level->GetSubsectorBBox(seg->front_sub, bbox);
  //const sector_t *bssec = seg->front_sub->sector;
  if (sv1.x < sv2.x) {
    bbox[0] = sv1.x;
    bbox[3] = sv2.x;
  } else {
    bbox[0] = sv2.x;
    bbox[3] = sv1.x;
  }
  if (sv1.y < sv2.y) {
    bbox[1] = sv1.y;
    bbox[4] = sv2.y;
  } else {
    bbox[1] = sv2.y;
    bbox[4] = sv1.y;
  }
  /*
  // floor
  const float fz1 = bssec->floor.GetPointZ(sv1);
  const float fz2 = bssec->floor.GetPointZ(sv2);
  bbox[2] = MIN(fz1, fz2);
  // ceiling
  const float cz1 = bssec->ceiling.GetPointZ(sv1);
  const float cz2 = bssec->ceiling.GetPointZ(sv2);
  bbox[5] = MAX(cz1, cz2);
  FixBBoxZ(bbox);
  */

  if (bbox[0] <= Origin.x && bbox[3] >= Origin.x &&
      bbox[1] <= Origin.y && bbox[4] >= Origin.y &&
      bbox[2] <= Origin.z && bbox[5] >= Origin.z)
  {
    // viewer is inside the box
    return true;
  }

  return Frustum.checkBox(bbox, clip_frustum_check_mask&mask);
}


//==========================================================================
//
//  VViewClipper::ClipIsBBoxVisible
//
//==========================================================================
bool VViewClipper::ClipIsBBoxVisible (const float bbox[6]) const {
  if (!clip_enabled || !clip_bbox) return true;
  if (ClipIsEmpty()) return true; // no clip nodes yet
#ifdef VV_CLIPPER_FULL_CHECK
  if (ClipIsFull()) return false;
#endif

  TVec v1, v2;
  if (CreateBBVerts(v1, v2, bbox, Origin)) return true;
  return IsRangeVisible(v1, v2);
}


//==========================================================================
//
//  VViewClipper::ClipCheckRegion
//
//==========================================================================
bool VViewClipper::ClipCheckRegion (const subregion_t *region, const subsector_t *sub) const {
  if (!clip_enabled || !clip_subregion) return true;
#ifdef VV_CLIPPER_FULL_CHECK
  if (ClipIsFull()) return false;
#endif
  const drawseg_t *ds = region->lines;
  for (auto count = sub->numlines; count--; ++ds) {
    const int orgside = ds->seg->PointOnSide2(Origin);
    if (orgside) {
      if (orgside == 2) return true; // origin is on plane, we cannot do anything sane
      continue; // viewer is in back side
    }
    if (IsRangeVisible(*ds->seg->v2, *ds->seg->v1)) return true;
  }
  return false;
}


//==========================================================================
//
//  VViewClipper::ClipCheckSubsector
//
//==========================================================================
bool VViewClipper::ClipCheckSubsector (const subsector_t *sub) {
  if (!clip_enabled) return true;
#ifdef VV_CLIPPER_FULL_CHECK
  if (ClipIsFull()) {
    if (dbg_clip_dump_sub_checks) GCon->Logf("  ::: sub #%d: clip is full", (int)(ptrdiff_t)(sub-Level->Subsectors));
    return false;
  }
#endif
  if (dbg_clip_dump_sub_checks) Dump();
  const seg_t *seg = &Level->Segs[sub->firstline];
  for (int count = sub->numlines; count--; ++seg) {
    //k8: q: i am not sure here, but why don't check both sides?
    //    a: because differently oriented side is not visible in any case
    const int orgside = seg->PointOnSide2(Origin);
    if (orgside) {
      if (orgside == 2) {
        if (dbg_clip_dump_sub_checks) GCon->Logf("  ::: sub #%d: seg #%u contains origin", (int)(ptrdiff_t)(sub-Level->Subsectors), (unsigned)(ptrdiff_t)(seg-Level->Segs));
        return true; // origin is on plane, we cannot do anything sane
      }
      continue; // viewer is in back side
    }
    const TVec &v1 = *seg->v1;
    const TVec &v2 = *seg->v2;
    if (IsRangeVisible(v2, v1)) {
      if (dbg_clip_dump_sub_checks) GCon->Logf("  ::: sub #%d: visible seg #%u (%f : %f) (vis=%d)", (int)(ptrdiff_t)(sub-Level->Subsectors), (unsigned)(ptrdiff_t)(seg-Level->Segs), PointToClipAngle(v2), PointToClipAngle(v1), IsRangeVisible(v2, v1));
      return true;
    }
    if (dbg_clip_dump_sub_checks) GCon->Logf("  ::: sub #%d: invisible seg #%u (%f : %f)", (int)(ptrdiff_t)(sub-Level->Subsectors), (unsigned)(ptrdiff_t)(seg-Level->Segs), PointToClipAngle(v2), PointToClipAngle(v1));
  }
  if (dbg_clip_dump_sub_checks) GCon->Logf("  ::: sub #%d: no visible segs", (int)(ptrdiff_t)(sub-Level->Subsectors));
  return false;
}


//==========================================================================
//
//  MirrorCheck
//
//==========================================================================
static inline bool MirrorCheck (const TPlane *Mirror, const TVec &v1, const TVec &v2) {
  if (Mirror) {
    // clip seg with mirror plane
    if (Mirror->PointOnSideThreshold(v1) && Mirror->PointOnSideThreshold(v2)) return false;
    // and clip it while we are here
    //const float dist1 = DotProduct(v1, Mirror->normal)-Mirror->dist;
    //const float dist2 = DotProduct(v2, Mirror->normal)-Mirror->dist;
    // k8: really?
    /*
         if (dist1 > 0.0f && dist2 <= 0.0f) v2 = v1+(v2-v1)*Dist1/(Dist1-Dist2);
    else if (dist2 > 0.0f && dist1 <= 0.0f) v1 = v2+(v1-v2)*Dist2/(Dist2-Dist1);
    */
  }
  return true;
}


//==========================================================================
//
//  VViewClipper::CheckAddClipSeg
//
//==========================================================================
void VViewClipper::CheckAddClipSeg (const seg_t *seg, const TPlane *Mirror, bool clipAll) {
  const line_t *ldef = seg->linedef;
  if (!clipAll && !ldef) return; // miniseg

  // viewer is in back side or on plane?
  const int orgside = seg->PointOnSide2(Origin);
  if (orgside) return; // origin is on plane, or on back

  if (clip_skip_slopes_1side) {
    // do not clip with slopes, if it has no midtex
    if (ldef->frontsector && (ldef->flags&ML_TWOSIDED) == 0) {
      int fcpic, ffpic;
      TPlane ffplane, fcplane;
      CopyHeight(ldef->frontsector, &ffplane, &fcplane, &ffpic, &fcpic);
      // only apply this to sectors without slopes
      if (ffplane.normal.z != 1.0f || fcplane.normal.z != -1.0f) return;
    }
  }

  const TVec &v1 = *seg->v1;
  const TVec &v2 = *seg->v2;

  if (!MirrorCheck(Mirror, v1, v2)) return;

  // for 2-sided line, determine if it can be skipped
  if (!clipAll && seg->backsector && (ldef->flags&(ML_TWOSIDED|ML_3DMIDTEX)) == ML_TWOSIDED) {
#ifdef XXX_CLIPPER_DEBUG
    currLevel = Level;
#endif
    if (!IsSegAClosedSomething(&Frustum, seg)) return;
  }

#if defined(XXX_CLIPPER_MANY_DUMPS)
  if (dbg_clip_dump_added_ranges) {
    GCon->Logf("added seg #%d (line #%d); range=(%f : %f); vis=%d",
      (int)(ptrdiff_t)(seg-Level->Segs),
      (int)(ptrdiff_t)(ldef-Level->Lines),
      PointToClipAngle(v2), PointToClipAngle(v1),
      (int)IsRangeVisible(v2, v1));
    //if (!IsRangeVisible(v2, v1)) return;
  }
#endif
  AddClipRange(v2, v1);
}


//==========================================================================
//
//  VViewClipper::ClipAddSubsectorSegs
//
//==========================================================================
void VViewClipper::ClipAddSubsectorSegs (const subsector_t *sub, const TPlane *Mirror, bool clipAll) {
  if (!clip_enabled) return;
#ifdef VV_CLIPPER_FULL_CHECK
  if (ClipIsFull()) return;
#endif

  bool doPoly = (sub->poly && clip_with_polyobj && r_draw_pobj);

  {
    const seg_t *seg = &Level->Segs[sub->firstline];
    for (int count = sub->numlines; count--; ++seg) {
      if (doPoly && !IsGoodSegForPoly(*this, seg)) doPoly = false;
      CheckAddClipSeg(seg, Mirror, clipAll);
    }
  }

  if (doPoly) {
    seg_t **polySeg = sub->poly->segs;
    for (int count = sub->poly->numsegs; count--; ++polySeg) {
      const seg_t *seg = *polySeg;
      if (IsGoodSegForPoly(*this, seg)) {
        CheckAddClipSeg(seg, Mirror, clipAll);
      }
    }
  }
}


#ifdef CLIENT
//==========================================================================
//
//  VViewClipper::CheckSubsectorLight
//
//==========================================================================
int VViewClipper::CheckSubsectorLight (const subsector_t *sub) const {
  if (!sub) return 0;
  float bbox[6];
  Level->GetSubsectorBBox(sub, bbox);

  if (bbox[0] <= Origin.x && bbox[3] >= Origin.x &&
      bbox[1] <= Origin.y && bbox[4] >= Origin.y &&
      bbox[2] <= Origin.z && bbox[5] >= Origin.z)
  {
    // inside the box
    return 1;
  }

  if (!CheckSphereVsAABB(bbox, Origin, Radius)) return 0;

  // check if all box vertices are inside a sphere
  // early exit if sphere is smaller than bbox
  if (Radius < bbox[3+0]-bbox[0]) return -1;
  if (Radius < bbox[3+1]-bbox[1]) return -1;
  if (Radius < bbox[3+2]-bbox[2]) return -1;

  const float xradSq = Radius*Radius;

  for (unsigned bidx = 0; bidx < 8; ++bidx) {
    TVec bv = TVec(bbox[BBoxVertexIndex[bidx][0]]-Origin.x, BBoxVertexIndex[bidx][1]-Origin.y, BBoxVertexIndex[bidx][2]-Origin.z);
    if (bv.lengthSquared() > xradSq) return -1; // partially inside
  }

  // fully inside
  return 1;
}


//==========================================================================
//
//  VViewClipper::ClipLightIsBBoxVisible
//
//==========================================================================
bool VViewClipper::ClipLightIsBBoxVisible (const float bbox[6]) const {
#ifdef VV_CLIPPER_FULL_CHECK
  if (ClipIsFull()) return false;
#endif
  if (!CheckSphereVsAABBIgnoreZ(bbox, Origin, Radius)) return false;
  if (ClipIsEmpty()) return true; // no clip nodes yet

  TVec v1, v2;
  if (CreateBBVerts(v1, v2, bbox, Origin)) return true;
  return IsRangeVisible(v1, v2);
}


//==========================================================================
//
//  VViewClipper::ClipLightCheckRegion
//
//==========================================================================
bool VViewClipper::ClipLightCheckRegion (const subregion_t *region, const subsector_t *sub, bool asShadow) const {
#ifdef VV_CLIPPER_FULL_CHECK
  if (ClipIsFull()) return false;
#endif
  const int slight = CheckSubsectorLight(sub);
  if (!slight) return false;
  if (slight > 0 && ClipIsEmpty()) return true; // no clip nodes yet
  const drawseg_t *ds = region->lines;
  for (auto count = sub->numlines; count--; ++ds) {
    if (slight < 0) {
      if (!ds->seg->SphereTouches(Origin, Radius)) continue;
    }
    // we have to check even "invisible" segs here, 'cause we need them all
    const TVec *v1, *v2;
    const int orgside = ds->seg->PointOnSide2(Origin);
    if (orgside == 2) return true; // origin is on plane, we cannot do anything sane
    if (orgside) {
      if (!asShadow) continue;
      v1 = ds->seg->v2;
      v2 = ds->seg->v1;
    } else {
      v1 = ds->seg->v1;
      v2 = ds->seg->v2;
    }
    if (IsRangeVisible(*v2, *v1)) return true;
  }
  return false;
}


//==========================================================================
//
//  VViewClipper::ClipLightCheckSeg
//
//  this doesn't do raduis and subsector checks: this is done in
//  `BuildLightVis()`
//
//==========================================================================
bool VViewClipper::ClipLightCheckSeg (const seg_t *seg, bool asShadow) const {
  if (ClipIsEmpty()) return true; // no clip nodes yet
  if (!seg->SphereTouches(Origin, Radius)) return false;
  // we have to check even "invisible" segs here, 'cause we need them all
  const TVec *v1, *v2;
  const int orgside = seg->PointOnSide2(Origin);
  if (orgside == 2) return true; // origin is on plane, we cannot do anything sane
  if (orgside) {
    //if (!asShadow) return false;
    v1 = seg->v2;
    v2 = seg->v1;
  } else {
    v1 = seg->v1;
    v2 = seg->v2;
  }
  return IsRangeVisible(*v2, *v1);
}


//==========================================================================
//
//  VViewClipper::ClipLightCheckSubsector
//
//==========================================================================
bool VViewClipper::ClipLightCheckSubsector (const subsector_t *sub, bool asShadow) const {
#ifdef VV_CLIPPER_FULL_CHECK
  if (ClipIsFull()) return false;
#endif
  const int slight = CheckSubsectorLight(sub);
  if (!slight) return false;
  if (slight > 0 && ClipIsEmpty()) return true; // no clip nodes yet
  const seg_t *seg = &Level->Segs[sub->firstline];
  for (int count = sub->numlines; count--; ++seg) {
    if (slight < 0) {
      if (!seg->SphereTouches(Origin, Radius)) continue;
    }
    // we have to check even "invisible" segs here, 'cause we need them all
    const TVec *v1, *v2;
    const int orgside = seg->PointOnSide2(Origin);
    if (orgside == 2) return true; // origin is on plane, we cannot do anything sane
    if (orgside) {
      //if (!asShadow) continue;
      v1 = seg->v2;
      v2 = seg->v1;
    } else {
      v1 = seg->v1;
      v2 = seg->v2;
    }
    if (IsRangeVisible(*v2, *v1)) return true;
  }
  return false;
}


//==========================================================================
//
//  VViewClipper::CheckLightAddClipSeg
//
//==========================================================================
void VViewClipper::CheckLightAddClipSeg (const seg_t *seg, const TPlane *Mirror, bool asShadow) {
  // no need to check light radius, it is already done
  const line_t *ldef = seg->linedef;
  if (!ldef) return; // miniseg should not clip
  /*
  if (clip_skip_slopes_1side) {
    // do not clip with slopes, if it has no midtex
    if ((ldef->flags&ML_TWOSIDED) == 0) {
      int fcpic, ffpic;
      TPlane ffplane, fcplane;
      CopyHeight(ldef->frontsector, &ffplane, &fcplane, &ffpic, &fcpic);
      // only apply this to sectors without slopes
      if (ffplane.normal.z != 1.0f || fcplane.normal.z != -1.0f) return;
    }
  }
  */

  // light has 360 degree FOV, so clip with all walls
  const TVec *v1, *v2;
  const int orgside = seg->PointOnSide2(Origin);
#if 1
  if (orgside == 2) return; // origin is on plane, we cannot do anything sane
  if (orgside) {
    if (!asShadow) return;
    v1 = seg->v2;
    v2 = seg->v1;
  } else {
    v1 = seg->v1;
    v2 = seg->v2;
  }
#else
  if (orgside > 0) return; // ignore backfacing segs
  v1 = seg->v1;
  v2 = seg->v2;
#endif

  if (!MirrorCheck(Mirror, *v1, *v2)) return;

  // for 2-sided line, determine if it can be skipped
  if (seg->backsector && (ldef->flags&(ML_TWOSIDED|ML_3DMIDTEX)) == ML_TWOSIDED) {
#ifdef XXX_CLIPPER_DEBUG
    currLevel = Level;
#endif
    if (!IsSegAClosedSomething(/*&Frustum*/nullptr, seg, &Origin, &Radius)) return;
  }

  AddClipRange(*v2, *v1);
}


//==========================================================================
//
//  VViewClipper::ClipLightAddSubsectorSegs
//
//==========================================================================
void VViewClipper::ClipLightAddSubsectorSegs (const subsector_t *sub, bool asShadow, const TPlane *Mirror) {
#ifdef VV_CLIPPER_FULL_CHECK
  if (ClipIsFull()) return;
#endif

  bool doPoly = (sub->poly && clip_with_polyobj && r_draw_pobj);

  {
    const seg_t *seg = &Level->Segs[sub->firstline];
    for (int count = sub->numlines; count--; ++seg) {
      if (doPoly && !IsGoodSegForPoly(*this, seg)) doPoly = false;
      CheckLightAddClipSeg(seg, Mirror, asShadow);
    }
  }

  if (doPoly
#ifdef VV_CLIPPER_FULL_CHECK
      && !ClipIsFull()
#endif
     )
  {
    seg_t **polySeg = sub->poly->segs;
    for (int count = sub->poly->numsegs; count--; ++polySeg) {
      const seg_t *seg = *polySeg;
      if (IsGoodSegForPoly(*this, seg)) {
        CheckLightAddClipSeg(seg, Mirror, asShadow);
      }
    }
  }
}
#endif
