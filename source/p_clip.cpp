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

//#define VAVOOM_CLIPPER_DO_VERTEX_BACKCHECK


// ////////////////////////////////////////////////////////////////////////// //
struct VViewClipper::VClipNode {
  VFloat From;
  VFloat To;
  VClipNode *Prev;
  VClipNode *Next;
};


// ////////////////////////////////////////////////////////////////////////// //
#ifdef CLIENT
extern VCvarF r_lights_radius;
extern VCvarF fov;
#endif
static VCvarB clip_bsp("clip_bsp", true, "Clip geometry behind some BSP nodes?", CVAR_PreInit);
static VCvarB clip_enabled("clip_enabled", true, "Do geometry cliping optimizations?", CVAR_PreInit);
static VCvarB clip_with_polyobj("clip_with_polyobj", true, "Do clipping with polyobjects?", CVAR_PreInit);
static VCvarB clip_platforms("clip_platforms", true, "Clip geometry behind some closed doors and lifts?", CVAR_PreInit);
VCvarB clip_frustum("clip_frustum", true, "Clip geometry with frustum?", CVAR_PreInit);
VCvarB clip_frustum_bsp("clip_frustum_bsp", true, "Clip BSP geometry with frustum?", CVAR_PreInit);
VCvarB clip_frustum_sub("clip_frustum_sub", true, "Clip subsectors with frustum?", CVAR_PreInit);

static VCvarB clip_skip_slopes_1side("clip_skip_slopes_1side", false, "Skip clipping with one-sided slopes?", CVAR_PreInit);

static VCvarB clip_height("clip_height", true, "Clip with top and bottom frustum?", CVAR_PreInit);


// ////////////////////////////////////////////////////////////////////////// //
/*
  // Lines with stacked sectors must never block!
  if (backsector->portals[sector_t::ceiling] != NULL || backsector->portals[sector_t::floor] != NULL ||
      frontsector->portals[sector_t::ceiling] != NULL || frontsector->portals[sector_t::floor] != NULL)
  {
    return false;
  }
*/


#ifdef VAVOOM_CLIPPER_DO_VERTEX_BACKCHECK
//==========================================================================
//
//  CheckAndClipVerts
//
//==========================================================================
static inline bool CheckAndClipVerts (const TVec &v1, const TVec &v2, const TVec &Origin) {
  // clip sectors that are behind rendered segs
  const TVec r1 = Origin-v1;
  const TVec r2 = Origin-v2;
  // here, z is almost always zero
  if (!r1.z && !r2.z) {
    // with zero z case, crossproduct becomes (0,0,magnitude)
    // then Length(v) is (0,0,sqrt(magnitude*magnitude)) -> (0,0,magnitude)
    // then Normalise(v) is (0,0,magnitude/magnitude) -> (0,0,1) or (0,0,-1)
    // that is, it effectively calculates a sign of 2d cross product
    // then, DotProduct(v, org) does: (0*org.x+0*org.y+msign*org.z) -> msign*org.z
    float msign1 = CrossProduct2D(r1, r2);
    if (msign1 < 0.0f) msign1 = -1.0f; else if (msign1 > 0.0f) msign1 = 0.0f;
    const float D1 = msign1*Origin.z;
    if (D1 < 0.0f) {
      float msign2 = CrossProduct2D(r2, r1);
      if (msign2 < 0.0f) msign2 = -1.0f; else if (msign2 > 0.0f) msign2 = 0.0f;
      const float D2 = msign2*Origin.z;
      if (D2 < 0.0f) return false;
    }
    //k8: do nothing at all here?
  } else {
    const float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), Origin); // distance to plane
    if (D1 < 0.0f) {
      const float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), Origin); // distance to plane
      if (D2 < 0.0f) return false;
    }
  }

  //if (D1 < 0.0f && D2 < 0.0f) return false;

  /*k8: i don't know what Janis wanted to accomplish with this, but it actually
        makes clipping WORSE due to limited precision
  if (doClipVerts) {
    // there might be a better method of doing this, but this one works for now...
         if (D1 > 0.0f && D2 <= 0.0f) v2 += (v2-v1)*D1/(D1-D2);
    else if (D2 > 0.0f && D1 <= 0.0f) v1 += (v1-v2)*D2/(D2-D1);
  }
  */

  return true;
}


//==========================================================================
//
//  CheckVerts
//
//==========================================================================
static inline bool CheckVerts (const TVec &v1, const TVec &v2, const TVec &Origin) {
  return CheckAndClipVerts(v1, v2, Origin);
}
#endif


#ifdef CLIENT
//==========================================================================
//
//  CheckAndClipVertsWithLight
//
//==========================================================================
static inline bool CheckAndClipVertsWithLight (const TVec &v1, const TVec &v2, const TVec &Origin, const TVec &CurrLightPos, const float CurrLightRadius) {
  // check if light touches a seg
  const TVec rLight1 = CurrLightPos-v1;
  const TVec rLight2 = CurrLightPos-v2;
  const float DLight1 = DotProduct(Normalise(CrossProduct(rLight1, rLight2)), CurrLightPos);
  const float DLight2 = DotProduct(Normalise(CrossProduct(rLight2, rLight1)), CurrLightPos);
  if ((DLight1 > CurrLightRadius && DLight2 > CurrLightRadius) ||
      (DLight1 < -CurrLightRadius && DLight2 < -CurrLightRadius))
  {
    return false;
  }

  const TVec r1 = Origin-v1;
  const TVec r2 = Origin-v2;
  const float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), Origin);
  if (D1 >= 0.0f) {
    if (D1 > r_lights_radius) return false;
    const float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), Origin);
    if (D2 > r_lights_radius) return false;
  } else {
    const float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), Origin);
    if (D2 < 0.0f) {
      //k8: wtf is this?
      //const TVec rView1 = Origin-v1-CurrLightPos;
      //const TVec rView2 = Origin-v2-CurrLightPos;
      const TVec rView1 = r1-CurrLightPos;
      const TVec rView2 = r2-CurrLightPos;
      const float DView1 = DotProduct(Normalise(CrossProduct(rView1, rView2)), Origin);
      if (DView1 < -CurrLightRadius) {
        const float DView2 = DotProduct(Normalise(CrossProduct(rView2, rView1)), Origin);
        if (DView2 < -CurrLightRadius) return false;
      }
    }
  }


  /*k8: i don't know what Janis wanted to accomplish with this, but it actually
        makes clipping WORSE due to limited precision
  if (doClipVerts) {
    // there might be a better method of doing this, but this one works for now...
         if (DLight1 > 0.0f && DLight2 <= 0.0f) v2 += (v2-v1)*D1/(D1-D2);
    else if (DLight2 > 0.0f && DLight1 <= 0.0f) v1 += (v1-v2)*D2/(D2-D1);
  }
  */
  return true;
}


//==========================================================================
//
//  CheckVertsWithLight
//
//==========================================================================
static inline bool CheckVertsWithLight (const TVec &v1, const TVec &v2, const TVec &Origin, const TVec &CurrLightPos, const float CurrLightRadius) {
  return CheckAndClipVertsWithLight(v1, v2, Origin, CurrLightPos, CurrLightRadius);
}
#endif


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


//==========================================================================
//
//  IsSegAClosedSomething
//
//  prerequisite: has front and back sectors, has linedef
//
//==========================================================================
static inline bool IsSegAClosedSomething (const VViewClipper &clip, const seg_t *seg) {
  if (!clip_platforms) return false;

  const line_t *ldef = seg->linedef;

  if (ldef->flags&ML_3DMIDTEX) return false; // 3dmidtex never blocks anything
  if ((ldef->flags&ML_TWOSIDED) == 0) return true; // one-sided wall always blocks everything

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

      /*
      // original
      if ((backcz2 <= frontfz2 && backcz2 <= frontfz1 && backcz1 <= frontfz2 && backcz1 <= frontfz1) &&
          (frontcz2 <= backfz2 && frontcz2 <= backfz1 && frontcz1 <= backfz2 && frontcz1 <= backfz1))
      {
        // it's a closed door/elevator/polydoor
        return true;
      }
      */

      if (clip_height && clip_frustum &&
          seg->partner && seg->partner != seg &&
          seg->partner->front_sub && seg->partner->front_sub != seg->front_sub)
      {
        // here we can check if midtex is in frustum; if it doesn't,
        // we can add this seg to clipper.
        // this way, we can clip alot of things when camera looks at
        // floor/ceiling, and we can clip away too high/low windows.
        //const TClipPlane &FrustumTop = clip.GetFrustumTop();
        //const TClipPlane &FrustumBot = clip.GetFrustumBottom();
        const TFrustum &Frustum = clip.GetFrustum();
        if (/*FrustumTop.isValid() && FrustumBot.isValid()*/ Frustum.isValid()) {
          /*
          const int lidx = (int)(ptrdiff_t)(ldef-clip.GetLevel()->Lines);
          if (lidx == / *126* / 132) {
            GCon->Logf("--- SEG SIDE: %d; front floor:(%f,%f); front ceil:(%f,%f); back floor:(%f,%f); back ceil:(%f,%f)",
              seg->side, frontfz1, frontfz2, frontcz1, frontcz2, backfz1, backfz2, backcz1, backcz2);
            GCon->Logf("SEG SIDE: %d; ftop: floorpt0=%d; floorpt1=%d; ceilpt0=%d; ceilpt1=%d", seg->side,
              FrustumTop.PointOnBackTh(TVec(vv1.x, vv1.y, frontfz1)),
              FrustumTop.PointOnBackTh(TVec(vv2.x, vv2.y, frontfz2)),
              FrustumTop.PointOnBackTh(TVec(vv1.x, vv1.y, frontcz1)),
              FrustumTop.PointOnBackTh(TVec(vv2.x, vv2.y, frontcz2)));
            GCon->Logf("SEG SIDE: %d; fbot: floorpt0=%d; floorpt1=%d; ceilpt0=%d; ceilpt1=%d", seg->side,
              FrustumBot.PointOnBackTh(TVec(vv1.x, vv1.y, frontfz1)),
              FrustumBot.PointOnBackTh(TVec(vv2.x, vv2.y, frontfz2)),
              FrustumBot.PointOnBackTh(TVec(vv1.x, vv1.y, frontcz1)),
              FrustumBot.PointOnBackTh(TVec(vv2.x, vv2.y, frontcz2)));
            GCon->Logf("SEG SIDE: %d; btop: floorpt0=%d; floorpt1=%d; ceilpt0=%d; ceilpt1=%d", seg->side,
              FrustumTop.PointOnBackTh(TVec(vv1.x, vv1.y, backfz1)),
              FrustumTop.PointOnBackTh(TVec(vv2.x, vv2.y, backfz2)),
              FrustumTop.PointOnBackTh(TVec(vv1.x, vv1.y, backcz1)),
              FrustumTop.PointOnBackTh(TVec(vv2.x, vv2.y, backcz2)));
            GCon->Logf("SEG SIDE: %d; bbot: floorpt0=%d; floorpt1=%d; ceilpt0=%d; ceilpt1=%d", seg->side,
              FrustumBot.PointOnBackTh(TVec(vv1.x, vv1.y, backfz1)),
              FrustumBot.PointOnBackTh(TVec(vv2.x, vv2.y, backfz2)),
              FrustumBot.PointOnBackTh(TVec(vv1.x, vv1.y, backcz1)),
              FrustumBot.PointOnBackTh(TVec(vv2.x, vv2.y, backcz2)));
          }
          */
          // create bounding box for linked subsector
          const subsector_t *bss = seg->partner->front_sub;
          float bbox[6];
          // min
          bbox[0] = bss->bbox[0];
          bbox[1] = bss->bbox[1];
          bbox[2] = bss->sector->floor.minz;
          // max
          bbox[3] = bss->bbox[2];
          bbox[4] = bss->bbox[3];
          bbox[5] = bss->sector->ceiling.maxz;
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
          // check
          if (!Frustum.checkBox(bbox)) {
            // out of frustum
            return true;
          }
        }
      }
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
  VClipNode *Ret = FreeClipNodes;
  if (Ret) FreeClipNodes = Ret->Next; else Ret = new VClipNode();
  return Ret;
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
  ClipResetFrustumPlanes();
}


//==========================================================================
//
//  VViewClipper::ClipResetFrustumPlanes
//
//==========================================================================
void VViewClipper::ClipResetFrustumPlanes () {
  //FrustumTop.invalidate();
  //FrustumBottom.invalidate();
  Frustum.clear();
}


void VViewClipper::ClipInitFrustumPlanes (const TAVec &viewangles, const TVec &viewforward, const TVec &viewright, const TVec &viewup,
                                          const float fovx, const float fovy)
{
  if (clip_frustum && !viewright.z && isFiniteF(fovy) && fovy != 0 && isFiniteF(fovx) && fovx != 0) {
    // no view roll, create frustum
    /*
    Frustum.setupFromFOVs(fovx, fovy, Origin, viewangles, false); // no need to create back plane
    // also, remove left and right planes, regular clipper will take care of that
    Frustum.planes[TFrustum::Left].invalidate();
    Frustum.planes[TFrustum::Right].invalidate();
    */
    /*
    const TVec vtop = TVec(
      TVEC_SUM3(0, -viewup.x/fovy, viewforward.x),
      TVEC_SUM3(0, -viewup.y/fovy, viewforward.y),
      TVEC_SUM3(0, -viewup.z/fovy, viewforward.z));
    // top side clip
    FrustumTop.SetPointDir3D(Origin, vtop.normalised());
    FrustumTop.clipflag = 0x04u;
    FrustumTop.setupBoxIndicies();
    // bottom side clip
    const TVec vbot = TVec(
      TVEC_SUM3(0, viewup.x/fovy, viewforward.x),
      TVEC_SUM3(0, viewup.y/fovy, viewforward.y),
      TVEC_SUM3(0, viewup.z/fovy, viewforward.z));
    FrustumBottom.SetPointDir3D(Origin, vbot.normalised());
    FrustumBottom.clipflag = 0x08u;
    FrustumBottom.setupBoxIndicies();
    */
    TClipBase cb(fovx, fovy);
    Frustum.setup(cb, Origin, viewangles, viewforward, viewright, viewup, true); // create back plane
  } else {
    ClipResetFrustumPlanes();
  }
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
  check(!ClipHead);

  ClipInitFrustumPlanes(viewangles, viewforward, viewright, viewup, fovx, fovy);

  //if (viewforward.z > 0.9f || viewforward.z < -0.9f) return; // looking up or down, can see behind
  if (viewforward.z >= 0.985f || viewforward.z <= -0.985f) {
    // looking up or down, can see behind
    return;
  }

  VFloat d1 = (VFloat)0;
  VFloat d2 = (VFloat)0;

  if (!clip_frustum) {
    // just cut everything at back
/*
#ifdef CLIENT
    d1 = -(fov/2.0f)-30;
    d2 = fov/2.0f+30;
#else
    d1 = -90;
    d2 = 90;
#endif
*/
    return;
  } else {
    TVec Pts[4];
    TVec TransPts[4];
    Pts[0] = TVec(fovx, fovy, 1.0f);
    Pts[1] = TVec(fovx, -fovy, 1.0f);
    Pts[2] = TVec(-fovx, fovy, 1.0f);
    Pts[3] = TVec(-fovx, -fovy, 1.0f);
    TVec clipforward = TVec(viewforward.x, viewforward.y, 0.0f);
    clipforward.normaliseInPlace();

    for (int i = 0; i < 4; ++i) {
      TransPts[i].x = TVEC_SUM3(Pts[i].x*viewright.x, Pts[i].y*viewup.x, /*Pts[i].z* */viewforward.x);
      TransPts[i].y = TVEC_SUM3(Pts[i].x*viewright.y, Pts[i].y*viewup.y, /*Pts[i].z* */viewforward.y);
      TransPts[i].z = TVEC_SUM3(Pts[i].x*viewright.z, Pts[i].y*viewup.z, /*Pts[i].z* */viewforward.z);

      if (DotProduct(TransPts[i], clipforward) <= 0.0f) return; // player can see behind

      VFloat a = VVC_matan(TransPts[i].y, TransPts[i].x);
      if (a < (VFloat)0) a += (VFloat)360;
      VFloat d = VVC_AngleMod180(a-viewangles.yaw);

      if (d1 > d) d1 = d;
      if (d2 < d) d2 = d;
    }
  }

  VFloat a1 = VVC_AngleMod(viewangles.yaw+d1);
  VFloat a2 = VVC_AngleMod(viewangles.yaw+d2);

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

  if (!ClipHead) {
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
  for (const VClipNode *N = ClipHead; N; N = N->Next) {
    if (From >= N->From && To <= N->To) return false;
  }
  return true;
}


//==========================================================================
//
//  VViewClipper::ClipIsFull
//
//==========================================================================
bool VViewClipper::ClipIsFull () const {
  return (ClipHead && ClipHead->From == (VFloat)0 && ClipHead->To == (VFloat)360);
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
inline static void CreateBBVerts (TVec &v1, TVec &v2, const float *BBox, const TVec &origin) {
  v1 = TVec(0, 0, 0);
  v2 = TVec(0, 0, 0);
  if (BBox[0] > origin.x) {
    if (BBox[1] > origin.y) {
      v1.x = BBox[3];
      v1.y = BBox[1];
      v2.x = BBox[0];
      v2.y = BBox[4];
    } else if (BBox[4] < origin.y) {
      v1.x = BBox[0];
      v1.y = BBox[1];
      v2.x = BBox[3];
      v2.y = BBox[4];
    } else {
      v1.x = BBox[0];
      v1.y = BBox[1];
      v2.x = BBox[0];
      v2.y = BBox[4];
    }
  } else if (BBox[3] < origin.x) {
    if (BBox[1] > origin.y) {
      v1.x = BBox[3];
      v1.y = BBox[4];
      v2.x = BBox[0];
      v2.y = BBox[1];
    } else if (BBox[4] < origin.y) {
      v1.x = BBox[0];
      v1.y = BBox[4];
      v2.x = BBox[3];
      v2.y = BBox[1];
    } else {
      v1.x = BBox[3];
      v1.y = BBox[4];
      v2.x = BBox[3];
      v2.y = BBox[1];
    }
  } else {
    if (BBox[1] > origin.y) {
      v1.x = BBox[3];
      v1.y = BBox[1];
      v2.x = BBox[0];
      v2.y = BBox[1];
    } else {
      v1.x = BBox[0];
      v1.y = BBox[4];
      v2.x = BBox[3];
      v2.y = BBox[4];
    }
  }
}


//==========================================================================
//
//  VViewClipper::ClipIsBBoxVisible
//
//==========================================================================
bool VViewClipper::ClipIsBBoxVisible (const float *BBox) const {
  if (!clip_enabled || !clip_bsp) return true;
  if (!ClipHead) return true; // no clip nodes yet

  if (BBox[0] <= Origin.x && BBox[3] >= Origin.x &&
      BBox[1] <= Origin.y && BBox[4] >= Origin.y)
  {
    // viewer is inside the box
    return true;
  }

  TVec v1, v2;
  CreateBBVerts(v1, v2, BBox, Origin);

#ifdef VAVOOM_CLIPPER_DO_VERTEX_BACKCHECK
  // clip sectors that are behind rendered segs
  if (!CheckAndClipVerts(v1, v2, Origin)) return false;
#endif

  return IsRangeVisible(v1, v2);
}


//==========================================================================
//
//  VViewClipper::ClipCheckRegion
//
//==========================================================================
bool VViewClipper::ClipCheckRegion (const subregion_t *region, const subsector_t *sub) const {
  if (!clip_enabled) return true;
  if (!ClipHead) return true; // no clip nodes yet
  if (clip_frustum && clip_frustum_sub && !CheckSubsectorFrustum(sub)) return false;
  const drawseg_t *ds = region->lines;
  for (auto count = sub->numlines-1; count--; ++ds) {
    const TVec &v1 = *ds->seg->v1;
    const TVec &v2 = *ds->seg->v2;
#ifdef VAVOOM_CLIPPER_DO_VERTEX_BACKCHECK
    if (!ds->seg->linedef) {
      // miniseg
      if (IsRangeVisible(v2, v1)) return true;
    } else {
      // clip sectors that are behind rendered segs
      if (!CheckAndClipVerts(v1, v2, Origin)) return false;
      if (IsRangeVisible(v2, v1)) return true;
    }
#else
    if (IsRangeVisible(v2, v1)) return true;
#endif
  }
  return false;
}


//==========================================================================
//
//  VViewClipper::CheckSubsectorFrustum
//
//==========================================================================
bool VViewClipper::CheckSubsectorFrustum (const subsector_t *sub) const {
  if (!sub || !Frustum.isValid()) return true;
  float bbox[6];
  // min
  bbox[0] = sub->bbox[0];
  bbox[1] = sub->bbox[1];
  bbox[2] = sub->sector->floor.minz;
  // max
  bbox[3] = sub->bbox[2];
  bbox[4] = sub->bbox[3];
  bbox[5] = sub->sector->ceiling.maxz;

  if (bbox[0] <= Origin.x && bbox[3] >= Origin.x &&
      bbox[1] <= Origin.y && bbox[4] >= Origin.y &&
      bbox[2] <= Origin.z && bbox[5] >= Origin.z)
  {
    // viewer is inside the box
    return true;
  }

  // check
  return Frustum.checkBox(bbox);
}


//==========================================================================
//
//  VViewClipper::CheckSegFrustum
//
//==========================================================================
bool VViewClipper::CheckSegFrustum (const seg_t *seg) const {
  if (!seg || !Frustum.isValid()) return true;
  return CheckSubsectorFrustum(seg->front_sub);
}


//==========================================================================
//
//  VViewClipper::CheckPartnerSegFrustum
//
//==========================================================================
bool VViewClipper::CheckPartnerSegFrustum (const seg_t *seg) const {
  if (!seg || !seg->partner || !seg->partner->front_sub || !Frustum.isValid()) return true;
  return CheckSubsectorFrustum(seg->partner->front_sub);
}


//==========================================================================
//
//  VViewClipper::ClipCheckSubsector
//
//==========================================================================
bool VViewClipper::ClipCheckSubsector (const subsector_t *sub) const {
  if (!clip_enabled) return true;
  if (!ClipHead) return true; // no clip nodes yet
  if (clip_frustum && clip_frustum_sub && !CheckSubsectorFrustum(sub)) return false;
  const seg_t *seg = &Level->Segs[sub->firstline];
  for (int count = sub->numlines; count--; ++seg) {
    const TVec &v1 = *seg->v1;
    const TVec &v2 = *seg->v2;
#ifdef VAVOOM_CLIPPER_DO_VERTEX_BACKCHECK
    if (!seg->linedef) {
      // miniseg
      if (IsRangeVisible(v2, v1)) return true;
    } else {
      // clip sectors that are behind rendered segs
      if (!CheckAndClipVerts(v1, v2, Origin)) return false;
      if (IsRangeVisible(v2, v1)) return true;
    }
#else
    if (IsRangeVisible(v2, v1)) return true;
#endif
  }
  return false;
}


//==========================================================================
//
//  VViewClipper::CheckAddClipSeg
//
//==========================================================================
void VViewClipper::CheckAddClipSeg (const seg_t *seg, const TPlane *Mirror, bool doCheckFrustum) {
  const line_t *ldef = seg->linedef;
  if (!ldef) return; // miniseg
  if (seg->PointOnSide(Origin)) return; // viewer is in back side or on plane

  if (clip_skip_slopes_1side) {
    // do not clip with slopes, if it has no midsec
    if ((ldef->flags&ML_TWOSIDED) == 0) {
      int fcpic, ffpic;
      TPlane ffplane, fcplane;
      CopyHeight(ldef->frontsector, &ffplane, &fcplane, &ffpic, &fcpic);
      // only apply this to sectors without slopes
      if (ffplane.normal.z != 1.0f || fcplane.normal.z != -1.0f) return;
    }
  }

  const TVec &v1 = *seg->v1;
  const TVec &v2 = *seg->v2;

  if (!doCheckFrustum || !clip_frustum || !clip_frustum_sub || CheckSegFrustum(seg)) {
#ifdef VAVOOM_CLIPPER_DO_VERTEX_BACKCHECK
    if (!CheckVerts(v1, v2, Origin)) return;
#endif

    if (Mirror) {
      // clip seg with mirror plane
      const float Dist1 = DotProduct(v1, Mirror->normal)-Mirror->dist;
      if (Dist1 <= 0.0f) {
        const float Dist2 = DotProduct(v2, Mirror->normal)-Mirror->dist;
        if (Dist2 <= 0.0f) return;
      }

      //if (Dist1 <= 0.0f && Dist2 <= 0.0f) return;

      // and clip it while we are here
      // k8: really?
      /*
           if (Dist1 > 0.0f && Dist2 <= 0.0f) v2 = v1+(v2-v1)*Dist1/(Dist1-Dist2);
      else if (Dist2 > 0.0f && Dist1 <= 0.0f) v1 = v2+(v1-v2)*Dist2/(Dist2-Dist1);
      */
    }

    // for 2-sided line, determine if it can be skipped
    if (seg->backsector && (ldef->flags&ML_TWOSIDED) != 0) {
      if (ldef->alpha < 1.0f) return; // skip translucent walls
      if (!IsSegAClosedSomething(*this, seg)) return;
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

  bool doPoly = (sub->poly && clip_with_polyobj);

  const seg_t *seg = &Level->Segs[sub->firstline];

  if (clip_frustum && clip_frustum_sub && !CheckSubsectorFrustum(sub)) {
    // completely out of frustum
    for (int count = sub->numlines; count--; ++seg) {
      const line_t *ldef = seg->linedef;
      if (!ldef) continue; // miniseg
      if (seg->PointOnSide(Origin)) continue; // viewer is in back side or on plane
      const TVec &v1 = *seg->v1;
      const TVec &v2 = *seg->v2;
      AddClipRange(v2, v1);
      if (doPoly && !IsGoodSegForPoly(*this, seg)) doPoly = false;
    }
  } else {
    // do slower checks
    for (int count = sub->numlines; count--; ++seg) {
      CheckAddClipSeg(seg, Mirror, false); // no need to do frustum checks
      if (doPoly && !IsGoodSegForPoly(*this, seg)) doPoly = false;
    }
  }

  if (doPoly) {
    seg_t **polySeg = sub->poly->segs;
    for (int count = sub->poly->numsegs; count--; ++polySeg) {
      seg = *polySeg;
      if (IsGoodSegForPoly(*this, seg)) {
        CheckAddClipSeg(seg, nullptr);
      }
    }
  }
}


#ifdef CLIENT
//==========================================================================
//
//  VViewClipper::ClipLightIsBBoxVisible
//
//==========================================================================
bool VViewClipper::ClipLightIsBBoxVisible (const float *BBox, const TVec &CurrLightPos, const float CurrLightRadius) const {
  if (!clip_enabled || !clip_bsp) return true;
  if (!ClipHead) return true; // no clip nodes yet
  if (CurrLightRadius < 2) return false;

  if (BBox[0] <= CurrLightPos.x && BBox[3] >= CurrLightPos.x &&
      BBox[1] <= CurrLightPos.y && BBox[4] >= CurrLightPos.y)
  {
    // viewer is inside the box
    return true;
  }

  TVec v1, v2;
  CreateBBVerts(v1, v2, BBox, CurrLightPos);

  // clip sectors that are behind rendered segs
  if (!CheckAndClipVertsWithLight(v1, v2, Origin, CurrLightPos, CurrLightRadius)) return false;

  return IsRangeVisible(v1, v2);
}


//==========================================================================
//
//  VViewClipper::ClipLightCheckRegion
//
//==========================================================================
bool VViewClipper::ClipLightCheckRegion (const subregion_t *region, const subsector_t *sub, const TVec &CurrLightPos, const float CurrLightRadius) const {
  if (!clip_enabled) return true;
  if (!ClipHead) return true; // no clip nodes yet
  if (CurrLightRadius < 2) return false;

  const drawseg_t *ds = region->lines;
  for (auto count = sub->numlines-1; count--; ++ds) {
    const TVec &v1 = *ds->seg->v1;
    const TVec &v2 = *ds->seg->v2;
    if (!ds->seg->linedef) {
      // miniseg
      if (IsRangeVisible(v2, v1)) return true;
    } else {
      // clip sectors that are behind rendered segs
      if (!CheckAndClipVertsWithLight(v1, v2, Origin, CurrLightPos, CurrLightRadius)) return false;
      if (IsRangeVisible(v2, v1)) return true;
    }
  }
  return false;
}


//==========================================================================
//
//  VViewClipper::ClipLightCheckSubsector
//
//==========================================================================
bool VViewClipper::ClipLightCheckSubsector (const subsector_t *sub, const TVec &CurrLightPos, const float CurrLightRadius) const {
  if (!clip_enabled) return true;
  if (!ClipHead) return true; // no clip nodes yet
  if (CurrLightRadius < 2) return false;

  const seg_t *seg = &Level->Segs[sub->firstline];
  for (int count = sub->numlines; count--; ++seg) {
    const TVec &v1 = *seg->v1;
    const TVec &v2 = *seg->v2;
    if (!seg->linedef) {
      // miniseg
      if (IsRangeVisible(v2, v1)) return true;
    } else {
      // clip sectors that are behind rendered segs
      if (!CheckAndClipVertsWithLight(v1, v2, Origin, CurrLightPos, CurrLightRadius)) return false;
      if (IsRangeVisible(v2, v1)) return true;
    }
  }
  return false;
}


//==========================================================================
//
//  VViewClipper::CheckLightAddClipSeg
//
//==========================================================================
void VViewClipper::CheckLightAddClipSeg (const seg_t *seg, const TVec &CurrLightPos, const float CurrLightRadius, const TPlane *Mirror) {
  const line_t *ldef = seg->linedef;
  if (!ldef) return; // miniseg
  //if (CurrLightRadius < 2) return false;
  if (seg->PointOnSide(Origin)) return; // viewer is in back side or on plane

  if (clip_skip_slopes_1side) {
    // do not clip with slopes, if it has no midsec
    if ((ldef->flags&ML_TWOSIDED) == 0) {
      int fcpic, ffpic;
      TPlane ffplane, fcplane;
      CopyHeight(ldef->frontsector, &ffplane, &fcplane, &ffpic, &fcpic);
      // only apply this to sectors without slopes
      if (ffplane.normal.z != 1.0f || fcplane.normal.z != -1.0f) return;
    }
  }

  const TVec &v1 = *seg->v1;
  const TVec &v2 = *seg->v2;

  if (!CheckVertsWithLight(v1, v2, Origin, CurrLightPos, CurrLightRadius)) return;

  if (Mirror) {
    // clip seg with mirror plane
    const float Dist1 = DotProduct(v1, Mirror->normal)-Mirror->dist;
    if (Dist1 <= 0.0f) {
      const float Dist2 = DotProduct(v2, Mirror->normal)-Mirror->dist;
      if (Dist2 <= 0.0f) return;
    }

    //if (Dist1 <= 0.0f && Dist2 <= 0.0f) return;

    // and clip it while we are here
    // k8: really?
    /*
         if (Dist1 > 0.0f && Dist2 <= 0.0f) v2 = v1+(v2-v1)*Dist1/(Dist1-Dist2);
    else if (Dist2 > 0.0f && Dist1 <= 0.0f) v1 = v2+(v1-v2)*Dist2/(Dist2-Dist1);
    */
  }

  // for 2-sided line, determine if it can be skipped
  if (seg->backsector && (ldef->flags&ML_TWOSIDED) != 0) {
    if (ldef->alpha < 1.0f) return; // skip translucent walls
    if (!IsSegAClosedSomething(*this, seg)) return;
  }

  AddClipRange(v2, v1);
}


//==========================================================================
//
//  VViewClipper::ClipLightAddSubsectorSegs
//
//==========================================================================
void VViewClipper::ClipLightAddSubsectorSegs (const subsector_t *sub, const TVec &CurrLightPos, const float CurrLightRadius, const TPlane *Mirror) {
  if (!clip_enabled) return;
  if (CurrLightRadius < 2) return;

  bool doPoly = (sub->poly && clip_with_polyobj);

  const seg_t *seg = &Level->Segs[sub->firstline];
  for (int count = sub->numlines; count--; ++seg) {
    CheckLightAddClipSeg(seg, CurrLightPos, CurrLightRadius, Mirror);
    if (doPoly && !IsGoodSegForPoly(*this, seg)) doPoly = false;
  }

  if (doPoly) {
    seg_t **polySeg = sub->poly->segs;
    for (int count = sub->poly->numsegs; count--; ++polySeg) {
      seg = *polySeg;
      if (IsGoodSegForPoly(*this, seg)) {
        CheckLightAddClipSeg(seg, CurrLightPos, CurrLightRadius, nullptr);
      }
    }
  }
}
#endif
