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

#define XXX_CLIPPER_DEBUG


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB clip_bsp("clip_bsp", true, "Clip geometry behind some BSP nodes?", CVAR_PreInit);
static VCvarB clip_subregion("clip_subregion", true, "Clip subregions?", CVAR_PreInit);
static VCvarB clip_enabled("clip_enabled", true, "Do geometry cliping optimizations?", CVAR_PreInit);
static VCvarB clip_with_polyobj("clip_with_polyobj", true, "Do clipping with polyobjects?", CVAR_PreInit);
static VCvarB clip_platforms("clip_platforms", true, "Clip geometry behind some closed doors and lifts?", CVAR_PreInit);
VCvarB clip_frustum("clip_frustum", true, "Clip geometry with frustum?", CVAR_PreInit);
VCvarB clip_frustum_mirror("clip_frustum_mirror", true, "Clip mirrored geometry with frustum?", CVAR_PreInit);
VCvarB clip_frustum_init_range("clip_frustum_init_range", true, "Init clipper range with frustum?", CVAR_PreInit);
VCvarB clip_frustum_bsp("clip_frustum_bsp", true, "Clip BSP geometry with frustum?", CVAR_PreInit); // sometimes this glitches
VCvarB clip_frustum_sub("clip_frustum_sub", false, "Clip subsectors with frustum?", CVAR_PreInit);
VCvarB clip_frustum_bbox("clip_frustum_bbox", false, "Clip BSP bounding boxes with frustum?", CVAR_PreInit);
VCvarB clip_frustum_region("clip_frustum_region", false, "Clip drawing regions with frustum?", CVAR_PreInit);
//VCvarB clip_frustum_seg("clip_frustum_seg", false, "Clip segs with frustum?", CVAR_PreInit);
VCvarB clip_frustum_seg_2s("clip_frustum_seg_2s", false, "Clip 2-sided segs with frustum?", CVAR_PreInit);
VCvarB clip_frustum_seg_backface("clip_frustum_seg_backface", false, "Add backface-looking segs to clipper?", CVAR_PreInit);
//VCvarI clip_frustum_check_mask("clip_frustum_check_mask", TFrustum::LeftBit|TFrustum::RightBit|TFrustum::BackBit, "Which frustum planes we should check?", CVAR_PreInit);
//VCvarI clip_frustum_check_mask("clip_frustum_check_mask", "19", "Which frustum planes we should check?", CVAR_PreInit);
VCvarI clip_frustum_check_mask("clip_frustum_check_mask", "255", "Which frustum planes we should check?", CVAR_PreInit);
static VCvarB clip_frustum_add_clipped("clip_frustum_add_clipped", false, "Add frustum-clipped geometry to clipped ranges?", CVAR_PreInit);
static VCvarB clip_frustum_add_clipped_sub("clip_frustum_add_clipped_sub", false, "Add frustum-clipped subsector to clipped ranges?", CVAR_PreInit);

static VCvarB clip_skip_slopes_1side("clip_skip_slopes_1side", false, "Skip clipping with one-sided slopes?", CVAR_PreInit);

static VCvarB clip_height("clip_height", true, "Clip with top and bottom frustum?", CVAR_PreInit);
static VCvarB clip_midsolid("clip_midsolid", true, "Clip with solid midtex?", CVAR_PreInit);


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


//==========================================================================
//
//  IsSlopedSeg
//
//==========================================================================
static inline bool IsGoodSegForPoly (const VViewClipper &clip, const seg_t *seg) {
  const line_t *ldef = seg->linedef;
  if (!ldef) return false;

  if (ldef->flags&ML_3DMIDTEX) return false; // 3dmidtex never blocks anything
  if ((ldef->flags&ML_TWOSIDED) == 0) return true; // one-sided wall always blocks everything

  // mirrors and horizons always block the view
  switch (ldef->special) {
    case LNSPEC_LineHorizon:
    case LNSPEC_LineMirror:
      return false;
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

  const line_t *ldef = seg->linedef;

  if (ldef->alpha < 1.0f) return false; // skip translucent walls
  //k8: this was checked by caller
  //if (ldef->flags&ML_3DMIDTEX) return false; // 3dmidtex never blocks anything
  //k8: this was checked by caller
  //if ((ldef->flags&ML_TWOSIDED) == 0) return true; // one-sided wall always blocks everything

  // mirrors and horizons always block the view
  switch (ldef->special) {
    case LNSPEC_LineHorizon:
    case LNSPEC_LineMirror:
      return true;
  }

  const sector_t *fsec = ldef->frontsector;
  const sector_t *bsec = ldef->backsector;

  if (fsec == bsec) return false; // self-referenced sector

  int fcpic, ffpic;
  int bcpic, bfpic;

  TPlane ffplane, fcplane;
  TPlane bfplane, bcplane;

  CopyHeight(fsec, &ffplane, &fcplane, &ffpic, &fcpic);
  CopyHeight(bsec, &bfplane, &bcplane, &bfpic, &bcpic);

  // only apply this to sectors without slopes
  if (ffplane.normal.z == 1.0f && bfplane.normal.z == 1.0f &&
      fcplane.normal.z == -1.0f && bcplane.normal.z == -1.0f)
  {
    bool hasTopTex = !GTextureManager.IsEmptyTexture(seg->sidedef->TopTexture);
    bool hasBotTex = !GTextureManager.IsEmptyTexture(seg->sidedef->BottomTexture);
    bool hasMidTex = !GTextureManager.IsEmptyTexture(seg->sidedef->MidTexture);
    if (hasTopTex || // a seg without top texture isn't a door
        hasBotTex || // a seg without bottom texture isn't an elevator/plat
        hasMidTex) // a seg without mid texture isn't a polyobj door
    {
      const TVec vv1 = *ldef->v1;
      const TVec vv2 = *ldef->v2;

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
                toffs = sec->topregion->ceiling->TexZ;
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
            /*
            float ttop, tbot;
            if (P_GetMidTexturePosition(ldef, seg->side, &ttop, &tbot)) {
              float minz, maxz;
              if (seg->side) {
                minz = MAX(backfz1, backfz2);
                maxz = MIN(backcz1, backcz2);
              } else {
                minz = MAX(frontfz1, frontfz2);
                maxz = MIN(frontcz1, frontcz2);
              }
              / *
              GCon->Logf("side:%d, tbot=%f; ttop=%f; minz=%f; maxz=%f (ffloor:%f,%f; fceiling:%f,%f) (bfloor:%f,%f; bceiling:%f,%f)",
                seg->side,
                tbot, ttop, minz, maxz,
                frontfz1, frontfz2, frontcz1, frontcz2,
                backfz1, backfz2, backcz1, backcz2);
              * /
              if (tbot <= minz && ttop >= maxz) return true;
            }
            */
          }
        }
      }

      // taken from Zandronum
      // now check for closed sectors
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

      /*
      // original
      if ((backcz2 <= frontfz2 && backcz2 <= frontfz1 && backcz1 <= frontfz2 && backcz1 <= frontfz1) &&
          (frontcz2 <= backfz2 && frontcz2 <= backfz1 && frontcz1 <= backfz2 && frontcz1 <= backfz1))
      {
        // it's a closed door/elevator/polydoor
        return true;
      }
      */

      if (clip_height && /*clip_frustum &&*/
          seg->partner && seg->partner != seg &&
          seg->partner->front_sub && seg->partner->front_sub != seg->front_sub)
      {
#ifdef XXX_CLIPPER_DEBUG_X
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

          float bbox[6];
          const float bfz1 = bssec->floor.GetPointZ(sv1);
          const float bfz2 = bssec->floor.GetPointZ(sv2);
          const float bcz1 = bssec->ceiling.GetPointZ(sv1);
          const float bcz2 = bssec->ceiling.GetPointZ(sv2);

          // min
          bbox[0] = MIN(sv1.x, sv2.x);
          bbox[1] = MIN(sv1.y, sv2.y);
          bbox[2] = MIN(bfz1, bfz2);
          // max
          bbox[3] = MAX(sv1.x, sv2.x);
          bbox[4] = MAX(sv1.y, sv2.y);
          bbox[5] = MAX(bcz1, bcz2);
          FixBBoxZ(bbox);

          //FIXME
          if (bbox[2] >= bbox[5]) {
            // definitely closed
            return true;
          }

          // debug
          /*
          const int lidx = (int)(ptrdiff_t)(ldef-clip.GetLevel()->Lines);
          if (lidx == / *126* / 132) {
            GCon->Logf("bbox:(%f,%f,%f)-(%f,%f,%f) : %d : %d", bbox[0], bbox[1], bbox[2], bbox[3], bbox[4], bbox[5], (int)FrustumTop.checkBox(bbox), (int)FrustumBot.checkBox(bbox));
            for (int f = bss->firstline; f < bss->firstline+bss->numlines; ++f) {
              const seg_t *s2 = &clip.GetLevel()->Segs[f];
              GCon->Logf("  %d: (%f,%f)-(%f,%f)", f, s2->v1->x, s2->v1->y, s2->v2->x, s2->v2->y);
            }
          }
          */

          if (Frustum && Frustum->isValid()) {
            // check (only top, bottom, and back)
            if (!Frustum->checkBox(bbox, TFrustum::TopBit|TFrustum::BottomBit|TFrustum::BackBit)) {
              // out of frustum
              return true;
            }
          }

          // check if light can see midtex
          if (lcheck) {
            if (!CheckSphereVsAABB(bbox, *lorg, *lrad)) return true; // cannot see midtex, can block
          }
        }
      }
      // still unsure; check if midtex is transparent
      //if (midSolid) return true; // texture is not transparent, block
    }
  } else {
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
void VViewClipper::ClearClipNodes (const TVec &AOrigin, VLevel *ALevel) {
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
  if (viewforward.z >= 0.985f || viewforward.z <= -0.985f) {
    // looking up or down, can see behind
    //return;
  }

  if (!clip_frustum) return;
  if (!clip_frustum_init_range) return;

  VFloat d1 = (VFloat)0;
  VFloat d2 = (VFloat)0;

  TVec Pts[4];
  TVec TransPts[4];
  Pts[0] = TVec(fovx, fovy, 1.0f);
  Pts[1] = TVec(fovx, -fovy, 1.0f);
  Pts[2] = TVec(-fovx, fovy, 1.0f);
  Pts[3] = TVec(-fovx, -fovy, 1.0f);
  TVec clipforward = TVec(viewforward.x, viewforward.y, 0.0f);
  //k8: i don't think that we need to normalize it
  //clipforward.normaliseInPlace();
  //unsigned canBehind = 0;

  for (unsigned i = 0; i < 4; ++i) {
    TransPts[i].x = VSUM3(Pts[i].x*viewright.x, Pts[i].y*viewup.x, /*Pts[i].z* */viewforward.x);
    TransPts[i].y = VSUM3(Pts[i].x*viewright.y, Pts[i].y*viewup.y, /*Pts[i].z* */viewforward.y);
    TransPts[i].z = VSUM3(Pts[i].x*viewright.z, Pts[i].y*viewup.z, /*Pts[i].z* */viewforward.z);

    if (DotProduct(TransPts[i], clipforward) <= 0.0f) {
      // player can see behind, use back frustum plane to clip
      //GCon->Logf("can see behind; i=%u", i);
      //++canBehind;
      //continue;
      return;
    }

    VFloat a = VVC_matan(TransPts[i].y, TransPts[i].x);
    if (a < (VFloat)0) a += (VFloat)360;
    VFloat d = VVC_AngleMod180(a-viewangles.yaw);

    if (d1 > d) d1 = d;
    if (d2 < d) d2 = d;
  }

  //if (canBehind == 4) return;

  VFloat a1 = VVC_AngleMod(viewangles.yaw+d1);
  VFloat a2 = VVC_AngleMod(viewangles.yaw+d2);

  // /*if (canBehind)*/ GCon->Logf("d1=%f; d2=%f; a1=%f; a2=%f", d1, d2, a1, a2);

  if (a1 > a2) {
    ClipHead = NewClipNode();
    ClipTail = ClipHead;
    ClipHead->From = a2;
    ClipHead->To = a1;
    ClipHead->Prev = nullptr;
    ClipHead->Next = nullptr;
  } else {
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
    ClipHead->From = (VFloat)0;
    ClipHead->To = (VFloat)360;
    ClipHead->Prev = nullptr;
    ClipHead->Next = nullptr;
    return;
  }

  // add head and tail ranges
  if (Range.ClipHead->From > (VFloat)0) DoAddClipRange((VFloat)0, Range.ClipHead->From);
  if (Range.ClipTail->To < (VFloat)360) DoAddClipRange(Range.ClipTail->To, (VFloat)360);

  // add middle ranges
  for (VClipNode *N = Range.ClipHead; N->Next; N = N->Next) DoAddClipRange(N->To, N->Next->From);
}


//==========================================================================
//
//  VViewClipper::DoAddClipRange
//
//==========================================================================
void VViewClipper::DoAddClipRange (VFloat From, VFloat To) {
  if (From < (VFloat)(VFloat)0) From = (VFloat)0; else if (From >= (VFloat)360) From = (VFloat)360;
  if (To < (VFloat)0) To = (VFloat)0; else if (To >= (VFloat)360) To = (VFloat)360;
  //check(From <= To || (From > To && To == (VFloat)360));

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
      VClipNode *N = NewClipNode();
      N->From = From;
      N->To = To;
      N->Prev = Node->Prev;
      N->Next = Node;

      if (Node->Prev) Node->Prev->Next = N; else ClipHead = N;
      Node->Prev = N;

      return;
    }

    if (Node->From <= From && Node->To >= To) return; // it contains this range

    if (From < Node->From) Node->From = From; // extend start of the current range

    if (To <= Node->To) return; // end is included, so we are done here

    // merge with following nodes if needed
    while (Node->Next && Node->Next->From <= To) {
      Node->To = Node->Next->To;
      RemoveClipNode(Node->Next);
    }

    if (To > Node->To) Node->To = To; // extend end

    // we are done here
    return;
  }

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
void VViewClipper::AddClipRange (const VFloat From, const VFloat To) {
  if (From > To) {
    DoAddClipRange((VFloat)0, To);
    DoAddClipRange(From, (VFloat)360);
  } else {
    DoAddClipRange(From, To);
  }
}


//==========================================================================
//
//  VViewClipper::DoIsRangeVisible
//
//==========================================================================
bool VViewClipper::DoIsRangeVisible (const VFloat From, const VFloat To) const {
#if 0
  for (const VClipNode *N = ClipHead; N; N = N->Next) {
    if (From >= N->From && To <= N->To) return false;
  }
#else
  // walk from two sides, this may be slightly faster
  const VClipNode *s = ClipHead;
  if (!s) return true;
  const VClipNode *e = ClipTail;
  if (s == e) return !(From >= s->From && To <= s->To);
  for (;;) {
    if (From >= s->From && To <= s->To) return false;
    if ((s = s->Next) == e) break;
    if (From >= e->From && To <= e->To) return false;
    if ((e = e->Prev) == s) break;
  }
#endif
  return true;
}


//==========================================================================
//
//  VViewClipper::IsRangeVisible
//
//==========================================================================
bool VViewClipper::IsRangeVisible (const VFloat From, const VFloat To) const {
  if (From > To) return (DoIsRangeVisible((VFloat)0, To) || DoIsRangeVisible(From, (VFloat)360));
  return DoIsRangeVisible(From, To);
}


//==========================================================================
//
//  CreateBBVerts
//
//==========================================================================
inline static void CreateBBVerts (TVec &v1, TVec &v2, const float BBox[6], const TVec &origin) {
  enum { MIN_X, MIN_Y, MIN_Z, MAX_X, MAX_Y, MAX_Z };
  //v1 = TVec(0, 0, 0);
  //v2 = TVec(0, 0, 0);
  if (BBox[MIN_X] > origin.x) {
    if (BBox[MIN_Y] > origin.y) {
      v1.x = BBox[MAX_X];
      v1.y = BBox[MIN_Y];
      v2.x = BBox[MIN_X];
      v2.y = BBox[MAX_Y];
    } else if (BBox[MAX_Y] < origin.y) {
      v1.x = BBox[MIN_X];
      v1.y = BBox[MIN_Y];
      v2.x = BBox[MAX_X];
      v2.y = BBox[MAX_Y];
    } else {
      v1.x = BBox[MIN_X];
      v1.y = BBox[MIN_Y];
      v2.x = BBox[MIN_X];
      v2.y = BBox[MAX_Y];
    }
  } else if (BBox[MAX_X] < origin.x) {
    if (BBox[MIN_Y] > origin.y) {
      v1.x = BBox[MAX_X];
      v1.y = BBox[MAX_Y];
      v2.x = BBox[MIN_X];
      v2.y = BBox[MIN_Y];
    } else if (BBox[MAX_Y] < origin.y) {
      v1.x = BBox[MIN_X];
      v1.y = BBox[MAX_Y];
      v2.x = BBox[MAX_X];
      v2.y = BBox[MIN_Y];
    } else {
      v1.x = BBox[MAX_X];
      v1.y = BBox[MAX_Y];
      v2.x = BBox[MAX_X];
      v2.y = BBox[MIN_Y];
    }
  } else {
    if (BBox[MIN_Y] > origin.y) {
      v1.x = BBox[MAX_X];
      v1.y = BBox[MIN_Y];
      v2.x = BBox[MIN_X];
      v2.y = BBox[MIN_Y];
    } else {
      v1.x = BBox[MIN_X];
      v1.y = BBox[MAX_Y];
      v2.x = BBox[MAX_X];
      v2.y = BBox[MAX_Y];
    }
  }
  v1.z = v2.z = 0;
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
  //return CheckSubsectorFrustum(seg->front_sub);
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
bool VViewClipper::ClipIsBBoxVisible (const float BBox[6], bool checkFrustum) const {
  if (!clip_enabled || !clip_bsp) return true;
  if (BBox[0] <= Origin.x && BBox[3] >= Origin.x &&
      BBox[1] <= Origin.y && BBox[4] >= Origin.y &&
      BBox[2] <= Origin.z && BBox[5] >= Origin.z)
  {
    // viewer is inside the box
    return true;
  }
  if (ClipIsFull()) return false;
  if (checkFrustum && clip_frustum && clip_frustum_bbox && Frustum.isValid()) {
    if (!Frustum.checkBox(BBox, clip_frustum_check_mask)) return false;
  }
  if (ClipIsEmpty()) return true; // no clip nodes yet
  TVec v1, v2;
  CreateBBVerts(v1, v2, BBox, Origin);
  return IsRangeVisible(v1, v2);
}


//==========================================================================
//
//  VViewClipper::ClipCheckRegion
//
//==========================================================================
bool VViewClipper::ClipCheckRegion (const subregion_t *region, const subsector_t *sub) const {
  if (!clip_enabled || !clip_subregion) return true;
  if (ClipIsFull()) return false;
  int sfres = 1;
  if (clip_frustum && clip_frustum_region /*&& Frustum.isValid()*/) {
    sfres = CheckSubsectorFrustum(sub);
    if (!sfres) return false;
  }
  if (ClipIsEmpty() && sfres > 0) return true; // no clip nodes yet
  const drawseg_t *ds = region->lines;
  for (auto count = sub->numlines; count--; ++ds) {
    const TVec &v1 = *ds->seg->v1;
    const TVec &v2 = *ds->seg->v2;
    if (ds->seg->PointOnSide(Origin)) continue; // viewer is in back side or on plane?
    if (IsRangeVisible(v2, v1)) {
      if (sfres > 0 || !clip_frustum || !clip_frustum_sub || !clip_frustum_region || CheckSegFrustum(ds->seg)) {
        return true;
      }
    }
  }
  return false;
}


//==========================================================================
//
//  VViewClipper::ClipCheckSubsector
//
//==========================================================================
bool VViewClipper::ClipCheckSubsector (const subsector_t *sub, bool addFrustumClipped) {
  if (!clip_enabled) return true;
  if (ClipIsFull()) return false;
  int sfres = 1;
  if (clip_frustum && clip_frustum_sub /*&& Frustum.isValid()*/) {
    sfres = CheckSubsectorFrustum(sub);
    if (!sfres) {
      if (addFrustumClipped && clip_frustum_add_clipped_sub) {
        const seg_t *sg = &Level->Segs[sub->firstline];
        for (int count = sub->numlines; count--; ++sg) {
          const TVec &v1 = *sg->v1;
          const TVec &v2 = *sg->v2;
          if (sg->PointOnSide(Origin)) {
            // viewer is in back side or on plane?
            AddClipRange(v1, v2);
          } else {
            AddClipRange(v2, v1);
          }
        }
      }
      return false;
    }
  }
  if (ClipIsEmpty() && sfres > 0) return true; // no clip nodes yet
  const seg_t *seg = &Level->Segs[sub->firstline];
  for (int count = sub->numlines; count--; ++seg) {
    /*
    const TVec &v1 = *seg->v1;
    const TVec &v2 = *seg->v2;
    if (seg->PointOnSide(Origin)) continue; // viewer is in back side or on plane?
    if (IsRangeVisible(v2, v1)) {
      if (sfres > 0 || !clip_frustum || !clip_frustum_sub || CheckSegFrustum(seg)) {
        return true;
      }
    }
    */
    //k8: i am not sure here, but why don't check both sides?
    const TVec *v1, *v2;
    if (seg->PointOnSide(Origin)) {
      v1 = seg->v2;
      v2 = seg->v1;
    } else {
      v1 = seg->v1;
      v2 = seg->v2;
    }
    if (IsRangeVisible(*v2, *v1)) {
      if (sfres > 0 || !clip_frustum || !clip_frustum_sub || CheckSegFrustum(seg)) {
        return true;
      }
    }
  }
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
    if (Mirror->PointOnSide(v1) && Mirror->PointOnSide(v2)) return false;
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
void VViewClipper::CheckAddClipSeg (const seg_t *seg, const TPlane *Mirror, bool skipSphereCheck) {
  const line_t *ldef = seg->linedef;
  if (!ldef) return; // miniseg cannot clip anything

  const TVec &v1 = *seg->v1;
  const TVec &v2 = *seg->v2;

  // viewer is in back side or on plane?
  if (seg->PointOnSide(Origin)) {
    // if it is out of view frustum, still clip with it
    if (!skipSphereCheck && clip_frustum && clip_frustum_seg_backface &&
        //seg->backsector && (ldef->flags&(ML_TWOSIDED|ML_3DMIDTEX)) == ML_TWOSIDED &&
        !CheckSegFrustum(seg, TFrustum::LeftBit|TFrustum::RightBit|TFrustum::BackBit))
    {
      AddClipRange(v1, v2);
      //GCon->Logf("get lost, silly line #%d!", (int)(ptrdiff_t)(ldef-Level->Lines));
      return;
    }
    // if this is two-sided line, clip with it too
    if (seg->backsector && seg->backsector != seg->frontsector &&
        (ldef->flags&(ML_TWOSIDED|ML_3DMIDTEX)) == ML_TWOSIDED)
    {
#ifdef XXX_CLIPPER_DEBUG
      currLevel = Level;
#endif
      if (IsSegAClosedSomething(&Frustum, seg)) AddClipRange(v1, v2);
    }
    return;
  }

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

  if (!MirrorCheck(Mirror, v1, v2)) return;

  /*
  if (!skipSphereCheck && clip_frustum && clip_frustum_seg && !CheckSegFrustum(seg)) {
    if (clip_frustum_add_clipped) AddClipRange(v2, v1); //k8: not sure
    return;
  }
  */

  // for 2-sided line, determine if it can be skipped
  if (seg->backsector && (ldef->flags&(ML_TWOSIDED|ML_3DMIDTEX)) == ML_TWOSIDED) {
#ifdef XXX_CLIPPER_DEBUG
    currLevel = Level;
#endif
    if (!IsSegAClosedSomething(&Frustum, seg)) {
      // it can still be culled by frustum
      // if it is, clip with it
      if (skipSphereCheck || !clip_frustum || !clip_frustum_seg_2s || CheckSegFrustum(seg)) {
        return;
      }
      //GCon->Logf("get lost, line #%d!", (int)(ptrdiff_t)(ldef-Level->Lines));
    }
  }

  AddClipRange(v2, v1);
}


//==========================================================================
//
//  VViewClipper::ClipAddSubsectorSegs
//
//==========================================================================
void VViewClipper::ClipAddSubsectorSegs (const subsector_t *sub, const TPlane *Mirror) {
  if (!clip_enabled) return;
  if (ClipIsFull()) return;

  bool doPoly = (sub->poly && clip_with_polyobj);

  const int ssFrustum = (clip_frustum && clip_frustum_sub ? CheckSubsectorFrustum(sub) : TFrustum::PARTIALLY);

  if (ssFrustum != TFrustum::OUTSIDE) {
    const seg_t *seg = &Level->Segs[sub->firstline];
    for (int count = sub->numlines; count--; ++seg) {
      if (doPoly && !IsGoodSegForPoly(*this, seg)) doPoly = false;
      CheckAddClipSeg(seg, Mirror, (ssFrustum == TFrustum::INSIDE));
    }
  } else {
    // completely outside
    //if (!doPoly) return;
    const seg_t *seg = &Level->Segs[sub->firstline];
    for (int count = sub->numlines; count--; ++seg) {
      if (doPoly && !IsGoodSegForPoly(*this, seg)) doPoly = false;
      const TVec &v1 = *seg->v1;
      const TVec &v2 = *seg->v2;
      if (!MirrorCheck(Mirror, v1, v2)) continue;
      if (clip_frustum_add_clipped) AddClipRange(v2, v1); //k8: not sure
    }
  }

  if (doPoly) {
    seg_t **polySeg = sub->poly->segs;
    for (int count = sub->poly->numsegs; count--; ++polySeg) {
      const seg_t *seg = *polySeg;
      if (IsGoodSegForPoly(*this, seg)) {
        CheckAddClipSeg(seg, Mirror);
      }
    }
  }
}


//==========================================================================
//
//  VViewClipper::ClipAddAllSubsectorSegs
//
//==========================================================================
void VViewClipper::ClipAddAllSubsectorSegs (const subsector_t *sub, const TPlane *Mirror) {
  if (!clip_enabled) return;
  if (ClipIsFull()) return;

  bool doPoly = (sub->poly && clip_with_polyobj);
  {
    const seg_t *seg = &Level->Segs[sub->firstline];
    for (int count = sub->numlines; count--; ++seg) {
      if (doPoly && !IsGoodSegForPoly(*this, seg)) doPoly = false;
      const line_t *ldef = seg->linedef;
      if (!ldef) continue; // miniseg cannot clip anything
      const TVec &v1 = *seg->v1;
      const TVec &v2 = *seg->v2;
      if (seg->PointOnSide(Origin)) {
        AddClipRange(v1, v2);
      } else {
        AddClipRange(v2, v1);
      }
    }
  }

  if (doPoly) {
    seg_t **polySeg = sub->poly->segs;
    for (int count = sub->poly->numsegs; count--; ++polySeg) {
      const seg_t *seg = *polySeg;
      if (IsGoodSegForPoly(*this, seg)) {
        const line_t *ldef = seg->linedef;
        if (!ldef) continue; // miniseg cannot clip anything
        const TVec &v1 = *seg->v1;
        const TVec &v2 = *seg->v2;
        if (seg->PointOnSide(Origin)) {
          AddClipRange(v1, v2);
        } else {
          AddClipRange(v2, v1);
        }
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
int VViewClipper::CheckSubsectorLight (const subsector_t *sub, const TVec &CurrLightPos, const float CurrLightRadius) const {
  if (!sub) return 0;
  float bbox[6];
  Level->GetSubsectorBBox(sub, bbox);

  if (bbox[0] <= CurrLightPos.x && bbox[3] >= CurrLightPos.x &&
      bbox[1] <= CurrLightPos.y && bbox[4] >= CurrLightPos.y &&
      bbox[2] <= CurrLightPos.z && bbox[5] >= CurrLightPos.z)
  {
    // inside the box
    return 1;
  }

  if (!CheckSphereVsAABB(bbox, CurrLightPos, CurrLightRadius)) return 0;

  // check if all box vertices are inside a sphere
  // early exit if sphere is smaller than bbox
  if (CurrLightRadius < bbox[3+0]-bbox[0]) return -1;
  if (CurrLightRadius < bbox[3+1]-bbox[1]) return -1;
  if (CurrLightRadius < bbox[3+2]-bbox[2]) return -1;

  const float xradSq = CurrLightRadius*CurrLightRadius;

  for (unsigned bidx = 0; bidx < 8; ++bidx) {
    TVec bv = TVec(bbox[BBoxVertexIndex[bidx][0]]-CurrLightPos.x, BBoxVertexIndex[bidx][1]-CurrLightPos.y, BBoxVertexIndex[bidx][2]-CurrLightPos.z);
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
bool VViewClipper::ClipLightIsBBoxVisible (const float BBox[6], const TVec &CurrLightPos, const float CurrLightRadius) const {
  //if (!clip_enabled || !clip_bsp) return true;
  if (CurrLightRadius < 2 || ClipIsFull()) return false;

  /*
  //k8: most BSP bboxes has non-sensical z, so no z checks
  if (BBox[0] <= CurrLightPos.x && BBox[3] >= CurrLightPos.x &&
      BBox[1] <= CurrLightPos.y && BBox[4] >= CurrLightPos.y)
  {
    // viewer is inside the box
    return true;
  }
  */

  if (!CheckSphereVsAABBIgnoreZ(BBox, CurrLightPos, CurrLightRadius)) return false;

  if (ClipIsEmpty()) return true; // no clip nodes yet

  TVec v1, v2;
  CreateBBVerts(v1, v2, BBox, CurrLightPos);

  return IsRangeVisible(v1, v2);
}


//==========================================================================
//
//  VViewClipper::ClipLightCheckRegion
//
//==========================================================================
bool VViewClipper::ClipLightCheckRegion (const subregion_t *region, const subsector_t *sub, const TVec &CurrLightPos, const float CurrLightRadius) const {
  if (CurrLightRadius < 2 || ClipIsFull()) return false;
  const int slight = CheckSubsectorLight(sub, CurrLightPos, CurrLightRadius);
  if (!slight) return false;
  if (slight > 0 && ClipIsEmpty()) return true; // no clip nodes yet
  const drawseg_t *ds = region->lines;
  for (auto count = sub->numlines; count--; ++ds) {
    if (slight < 0) {
      if (!ds->seg->SphereTouches(CurrLightPos, CurrLightRadius)) continue;
    }
    // we have to check even "invisible" segs here, 'cause we need them all
    const TVec *v1, *v2;
    if (ds->seg->PointOnSide(Origin)) {
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
bool VViewClipper::ClipLightCheckSeg (const seg_t *seg, const TVec &CurrLightPos, const float CurrLightRadius) const {
  if (ClipIsEmpty()) return true; // no clip nodes yet
  if (!seg->SphereTouches(CurrLightPos, CurrLightRadius)) return false;
  // we have to check even "invisible" segs here, 'cause we need them all
  const TVec *v1, *v2;
  if (seg->PointOnSide(Origin)) {
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
bool VViewClipper::ClipLightCheckSubsector (const subsector_t *sub, const TVec &CurrLightPos, const float CurrLightRadius) const {
  if (CurrLightRadius < 2 || ClipIsFull()) return false;
  const int slight = CheckSubsectorLight(sub, CurrLightPos, CurrLightRadius);
  if (!slight) return false;
  if (slight > 0 && ClipIsEmpty()) return true; // no clip nodes yet
  const seg_t *seg = &Level->Segs[sub->firstline];
  for (int count = sub->numlines; count--; ++seg) {
    if (slight < 0) {
      if (!seg->SphereTouches(CurrLightPos, CurrLightRadius)) continue;
    }
    // we have to check even "invisible" segs here, 'cause we need them all
    const TVec *v1, *v2;
    if (seg->PointOnSide(Origin)) {
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
//  returns `true` if clip is full
//
//==========================================================================
bool VViewClipper::CheckLightAddClipSeg (const seg_t *seg, const TVec &CurrLightPos,
                                         const float CurrLightRadius, const TPlane *Mirror)
{
  // no need to check light radius, it is already done
  const line_t *ldef = seg->linedef;
  if (!ldef) return false; // miniseg should not clip
  /*
  if (clip_skip_slopes_1side) {
    // do not clip with slopes, if it has no midtex
    if ((ldef->flags&ML_TWOSIDED) == 0) {
      int fcpic, ffpic;
      TPlane ffplane, fcplane;
      CopyHeight(ldef->frontsector, &ffplane, &fcplane, &ffpic, &fcpic);
      // only apply this to sectors without slopes
      if (ffplane.normal.z != 1.0f || fcplane.normal.z != -1.0f) return false;
    }
  }
  */

  // light has 360 degree FOV, so clip with all walls
  const TVec *v1, *v2;
  if (seg->PointOnSide(Origin)) {
    v1 = seg->v2;
    v2 = seg->v1;
  } else {
    v1 = seg->v1;
    v2 = seg->v2;
  }

  if (!MirrorCheck(Mirror, *v1, *v2)) return false;

  // for 2-sided line, determine if it can be skipped
  if (seg->backsector && (ldef->flags&(ML_TWOSIDED|ML_3DMIDTEX)) == ML_TWOSIDED) {
#ifdef XXX_CLIPPER_DEBUG
    currLevel = Level;
#endif
    if (!IsSegAClosedSomething(&Frustum, seg, &CurrLightPos, &CurrLightRadius)) return false;
  }

  AddClipRange(*v2, *v1);
  return ClipIsFull();
}


//==========================================================================
//
//  VViewClipper::ClipLightAddSubsectorSegs
//
//==========================================================================
void VViewClipper::ClipLightAddSubsectorSegs (const subsector_t *sub, const TVec &CurrLightPos, const float CurrLightRadius, const TPlane *Mirror) {
  if (CurrLightRadius < 2 || ClipIsFull()) return;

  bool doPoly = (sub->poly && clip_with_polyobj);

  {
    const seg_t *seg = &Level->Segs[sub->firstline];
    for (int count = sub->numlines; count--; ++seg) {
      if (doPoly && !IsGoodSegForPoly(*this, seg)) doPoly = false;
      if (CheckLightAddClipSeg(seg, CurrLightPos, CurrLightRadius, Mirror)) {
        // clip is full, there is nothing more to do
        return;
      }
    }
  }

  if (doPoly) {
    seg_t **polySeg = sub->poly->segs;
    for (int count = sub->poly->numsegs; count--; ++polySeg) {
      const seg_t *seg = *polySeg;
      if (IsGoodSegForPoly(*this, seg)) {
        if (CheckLightAddClipSeg(seg, CurrLightPos, CurrLightRadius, Mirror)) {
          // clip is full, there is nothing more to do
          return;
        }
      }
    }
  }
}
#endif
