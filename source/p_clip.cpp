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


struct VViewClipper::VClipNode {
  float From;
  float To;
  VClipNode *Prev;
  VClipNode *Next;
};


static VCvarB clip_bsp("clip_bsp", true, "Clip geometry behind some BSP nodes?"/*, CVAR_Archive*/);
static VCvarB clip_enabled("clip_enabled", true, "Do geometry cliping optimizations?"/*, CVAR_Archive*/);
static VCvarB clip_with_polyobj("clip_with_polyobj", true, "Do clipping with polyobjects?"/*, CVAR_Archive*/);

static VCvarB clip_platforms("clip_platforms", true, "Clip geometry behind some closed doors and lifts?"/*, CVAR_Archive*/);


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
//  IsSegAClosedSomething
//
//  prerequisite: has front and back sectors, has linedef
//
//==========================================================================
static inline bool IsSegAClosedSomething (VLevel *Level, const seg_t *seg) {
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

  auto fsec = ldef->frontsector;
  auto bsec = ldef->backsector;

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
    }
  } else {
    // sloped
    if (((fsec->floor.maxz > bsec->ceiling.minz && fsec->ceiling.maxz < bsec->floor.minz) ||
         (fsec->floor.minz > bsec->ceiling.maxz && fsec->ceiling.minz < bsec->floor.maxz)) ||
        ((bsec->floor.maxz > fsec->ceiling.minz && bsec->ceiling.maxz < fsec->floor.minz) ||
         (bsec->floor.minz > fsec->ceiling.maxz && bsec->ceiling.minz < fsec->floor.maxz)))
    {
      return true;
    }
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
  guard(VViewClipper::NewClipNode);
  VClipNode *Ret = FreeClipNodes;
  if (Ret) FreeClipNodes = Ret->Next; else Ret = new VClipNode();
  return Ret;
  unguard;
}


//==========================================================================
//
//  VViewClipper::RemoveClipNode
//
//==========================================================================
void VViewClipper::RemoveClipNode (VViewClipper::VClipNode *Node) {
  guard(VViewClipper::RemoveClipNode);
  if (Node->Next) Node->Next->Prev = Node->Prev;
  if (Node->Prev) Node->Prev->Next = Node->Next;
  if (Node == ClipHead) ClipHead = Node->Next;
  if (Node == ClipTail) ClipTail = Node->Prev;
  Node->Next = FreeClipNodes;
  FreeClipNodes = Node;
  unguard;
}


//==========================================================================
//
//  VViewClipper::ClearClipNodes
//
//==========================================================================
void VViewClipper::ClearClipNodes (const TVec &AOrigin, VLevel *ALevel) {
  guard(VViewClipper::ClearClipNodes);
  if (ClipHead) {
    ClipTail->Next = FreeClipNodes;
    FreeClipNodes = ClipHead;
  }
  ClipHead = nullptr;
  ClipTail = nullptr;
  Origin = AOrigin;
  Level = ALevel;
  unguard;
}


//==========================================================================
//
//  VViewClipper::ClipInitFrustrumRange
//
//==========================================================================
void VViewClipper::ClipInitFrustrumRange (const TAVec &viewangles, const TVec &viewforward,
                                          const TVec &viewright, const TVec &viewup,
                                          float fovx, float fovy)
{
  guard(VViewClipper::ClipInitFrustrumRange);
  check(!ClipHead);

  //if (viewforward.z > 0.9f || viewforward.z < -0.9f) return; // looking up or down, can see behind
  if (viewforward.z >= 0.985f || viewforward.z <= -0.985f) return; // looking up or down, can see behind

  TVec Pts[4];
  TVec TransPts[4];
  Pts[0] = TVec(1, fovx, fovy);
  Pts[1] = TVec(1, fovx, -fovy);
  Pts[2] = TVec(1, -fovx, fovy);
  Pts[3] = TVec(1, -fovx, -fovy);
  TVec clipforward = Normalise(TVec(viewforward.x, viewforward.y, 0.0f));
  float d1 = 0;
  float d2 = 0;

  for (int i = 0; i < 4; ++i) {
    TransPts[i].x = Pts[i].y*viewright.x+Pts[i].z*viewup.x+Pts[i].x*viewforward.x;
    TransPts[i].y = Pts[i].y*viewright.y+Pts[i].z*viewup.y+Pts[i].x*viewforward.y;
    TransPts[i].z = Pts[i].y*viewright.z+Pts[i].z*viewup.z+Pts[i].x*viewforward.z;

    if (DotProduct(TransPts[i], clipforward) <= 0) return; // player can see behind

    float a = matan(TransPts[i].y, TransPts[i].x);
    if (a < 0.0f) a += 360.0f;

    float d = AngleMod180(a-viewangles.yaw);
    if (d1 > d) d1 = d;
    if (d2 < d) d2 = d;
  }
  float a1 = AngleMod(viewangles.yaw+d1);
  float a2 = AngleMod(viewangles.yaw+d2);

  if (a1 > a2) {
    ClipHead = NewClipNode();
    ClipTail = ClipHead;
    ClipHead->From = a2;
    ClipHead->To = a1;
    ClipHead->Prev = nullptr;
    ClipHead->Next = nullptr;
  } else {
    ClipHead = NewClipNode();
    ClipHead->From = 0.0f;
    ClipHead->To = a1;
    ClipTail = NewClipNode();
    ClipTail->From = a2;
    ClipTail->To = 360.0f;
    ClipHead->Prev = nullptr;
    ClipHead->Next = ClipTail;
    ClipTail->Prev = ClipHead;
    ClipTail->Next = nullptr;
  }
  unguard;
}


//==========================================================================
//
//  VViewClipper::ClipToRanges
//
//==========================================================================
void VViewClipper::ClipToRanges (const VViewClipper &Range) {
  guard(VViewClipper::ClipToRanges);
  if (!Range.ClipHead) {
    // no ranges, everything is clipped away
    DoAddClipRange(0.0f, 360.0f);
    return;
  }

  // add head and tail ranges
  if (Range.ClipHead->From > 0.0f) DoAddClipRange(0.0f, Range.ClipHead->From);
  if (Range.ClipTail->To < 360.0f) DoAddClipRange(Range.ClipTail->To, 360.0f);

  // add middle ranges
  for (VClipNode *N = Range.ClipHead; N->Next; N = N->Next) DoAddClipRange(N->To, N->Next->From);
  unguard;
}


//==========================================================================
//
//  VViewClipper::DoAddClipRange
//
//==========================================================================
void VViewClipper::DoAddClipRange (float From, float To) {
  guard(VViewClipper::DoAddClipRange);

  if (From < 0.0f) From = 0.0f; else if (From >= 360.0f) From = 360.0f;
  if (To < 0.0f) To = 0.0f; else if (To >= 360.0f) To = 360.0f;
  check(From <= To || (From > To && To == 360.0f));

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
  unguard;
}


//==========================================================================
//
//  VViewClipper::AddClipRange
//
//==========================================================================
void VViewClipper::AddClipRange (float From, float To) {
  guard(VViewClipper::AddClipRange);
  if (From > To) {
    DoAddClipRange(0.0f, To);
    DoAddClipRange(From, 360.0f);
  } else {
    DoAddClipRange(From, To);
  }
  unguard;
}


//==========================================================================
//
//  VViewClipper::DoIsRangeVisible
//
//==========================================================================
bool VViewClipper::DoIsRangeVisible (float From, float To) {
  guard(VViewClipper::DoIsRangeVisible);
  for (VClipNode *N = ClipHead; N; N = N->Next) {
    if (From >= N->From && To <= N->To) return false;
  }
  return true;
  unguard;
}


//==========================================================================
//
//  VViewClipper::IsRangeVisible
//
//==========================================================================
bool VViewClipper::IsRangeVisible (float From, float To) {
  guard(VViewClipper::IsRangeVisible);
  if (From > To) return (DoIsRangeVisible(0.0f, To) || DoIsRangeVisible(From, 360.0f));
  return DoIsRangeVisible(From, To);
  unguard;
}


//==========================================================================
//
//  VViewClipper::ClipIsFull
//
//==========================================================================
bool VViewClipper::ClipIsFull () {
  guard(VViewClipper::ClipIsFull);
  return (ClipHead && ClipHead->From == 0.0f && ClipHead->To == 360.0f);
  unguard;
}


//==========================================================================
//
//  CreateBBVerts
//
//==========================================================================
inline static void CreateBBVerts (const float *BBox, const TVec origin, TVec *v1, TVec *v2) {
  v1->z = v2->z = 0;
  if (BBox[0] > origin.x) {
    if (BBox[1] > origin.y) {
      v1->x = BBox[3];
      v1->y = BBox[1];
      v2->x = BBox[0];
      v2->y = BBox[4];
    } else if (BBox[4] < origin.y) {
      v1->x = BBox[0];
      v1->y = BBox[1];
      v2->x = BBox[3];
      v2->y = BBox[4];
    } else {
      v1->x = BBox[0];
      v1->y = BBox[1];
      v2->x = BBox[0];
      v2->y = BBox[4];
    }
  } else if (BBox[3] < origin.x) {
    if (BBox[1] > origin.y) {
      v1->x = BBox[3];
      v1->y = BBox[4];
      v2->x = BBox[0];
      v2->y = BBox[1];
    } else if (BBox[4] < origin.y) {
      v1->x = BBox[0];
      v1->y = BBox[4];
      v2->x = BBox[3];
      v2->y = BBox[1];
    } else {
      v1->x = BBox[3];
      v1->y = BBox[4];
      v2->x = BBox[3];
      v2->y = BBox[1];
    }
  } else {
    if (BBox[1] > origin.y) {
      v1->x = BBox[3];
      v1->y = BBox[1];
      v2->x = BBox[0];
      v2->y = BBox[1];
    } else {
      v1->x = BBox[0];
      v1->y = BBox[4];
      v2->x = BBox[3];
      v2->y = BBox[4];
    }
  }
}


//==========================================================================
//
//  VViewClipper::ClipIsBBoxVisible
//
//==========================================================================
bool VViewClipper::ClipIsBBoxVisible (const float *BBox, bool shadowslight, const TVec &CurrLightPos, float CurrLightRadius) {
  guard(VViewClipper::ClipIsBBoxVisible);
  if (!ClipHead) return true; // no clip nodes yet

  if (!clip_enabled) return true;

  if (shadowslight) {
    if (BBox[0] <= CurrLightPos.x && BBox[3] >= CurrLightPos.x &&
        BBox[1] <= CurrLightPos.y && BBox[4] >= CurrLightPos.y)
    {
      // viewer is inside the box
      return true;
    }
  } else if (BBox[0] <= Origin.x && BBox[3] >= Origin.x &&
             BBox[1] <= Origin.y && BBox[4] >= Origin.y)
  {
    // viewer is inside the box
    return true;
  }

  TVec v1(0, 0, 0), v2(0, 0, 0);
  CreateBBVerts(BBox, (shadowslight ? CurrLightPos : Origin), &v1, &v2);

  // clip sectors that are behind rendered segs
  if (!shadowslight) {
    if (!ClipVerts(v1, v2)) return false;
  } else {
    if (!ClipVertsWithLight(v1, v2, CurrLightPos, CurrLightRadius)) return false;
  }

  return IsRangeVisible(PointToClipAngle(v1), PointToClipAngle(v2));
  unguard;
}


//==========================================================================
//
//  VViewClipper::ClipCheckRegion
//
//==========================================================================
bool VViewClipper::ClipCheckRegion (subregion_t *region, subsector_t *sub, bool shadowslight, const TVec &CurrLightPos, float CurrLightRadius) {
  guard(VViewClipper::ClipCheckRegion);
  if (!ClipHead) return true;

  if (!clip_enabled) return true;

  drawseg_t *ds = region->lines;
  for (auto count = sub->numlines; count--; ++ds) {
    TVec v1 = *ds->seg->v1;
    TVec v2 = *ds->seg->v2;

    /*
    if (!ds->seg->linedef) {
      // miniseg
      if (!IsRangeVisible(PointToClipAngle(v2), PointToClipAngle(v1))) continue;
    }
    */

    // clip sectors that are behind rendered segs
    if (!shadowslight) {
      if (!ClipVerts(v1, v2)) return false;
    } else {
      if (!ClipVertsWithLight(v1, v2, CurrLightPos, CurrLightRadius)) return false;
    }

    if (IsRangeVisible(PointToClipAngle(v2), PointToClipAngle(v1))) return true;
  }
  return false;
  unguard;
}


//==========================================================================
//
//  VViewClipper::ClipCheckSubsector
//
//==========================================================================
bool VViewClipper::ClipCheckSubsector (subsector_t *Sub, bool shadowslight, const TVec &CurrLightPos, float CurrLightRadius) {
  guard(VViewClipper::ClipCheckSubsector);
  if (!ClipHead) return true;

  if (!clip_enabled) return true;

  for (int i = 0; i < Sub->numlines; ++i) {
    seg_t *seg = &Level->Segs[Sub->firstline+i];

    TVec v1 = *seg->v1;
    TVec v2 = *seg->v2;

    /*
    if (!seg->linedef) {
      // miniseg
      if (!IsRangeVisible(PointToClipAngle(v2), PointToClipAngle(v1))) continue;
    }
    */

    // clip sectors that are behind rendered segs
    if (!shadowslight) {
      if (!ClipVerts(v1, v2)) return false;
    } else {
      if (!ClipVertsWithLight(v1, v2, CurrLightPos, CurrLightRadius)) return false;
    }

    if (IsRangeVisible(PointToClipAngle(v2), PointToClipAngle(v1))) return true;
  }
  return false;
  unguard;
}


//==========================================================================
//
//  VViewClipper::CheckAddClipSeg
//
//==========================================================================
void VViewClipper::CheckAddClipSeg (const seg_t *seg, bool shadowslight, TPlane *Mirror, const TVec &CurrLightPos, float CurrLightRadius) {
  if (!seg->linedef) return; // miniseg
  if (seg->PointOnSide(Origin)) return; // viewer is in back side or on plane

  TVec v1 = *seg->v1;
  TVec v2 = *seg->v2;

#if 0
  // only apply this to sectors without slopes
  // k8: originally, slopes were checked only for polyobjects; wtf?!
  if (true /*seg->frontsector->floor.normal.z == 1.0f && seg->frontsector->ceiling.normal.z == -1.0f*/) {
    TVec r1 = Origin-v1;
    TVec r2 = Origin-v2;
    float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), Origin);
    float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), Origin);

    if (shadowslight) {
      TVec rLight1 = CurrLightPos-v1;
      TVec rLight2 = CurrLightPos-v2;
      float DLight1 = DotProduct(Normalise(CrossProduct(rLight1, rLight2)), CurrLightPos);
      float DLight2 = DotProduct(Normalise(CrossProduct(rLight2, rLight1)), CurrLightPos);

      TVec rView1 = Origin-v1-CurrLightPos;
      TVec rView2 = Origin-v2-CurrLightPos;
      float DView1 = DotProduct(Normalise(CrossProduct(rView1, rView2)), Origin);
      float DView2 = DotProduct(Normalise(CrossProduct(rView2, rView1)), Origin);

      if (D1 <= 0.0f && D2 <= 0.0f && DView1 < -CurrLightRadius && DView2 < -CurrLightRadius) return;
      if (D1 > r_lights_radius && D2 > r_lights_radius) return;

      if ((DLight1 > CurrLightRadius && DLight2 > CurrLightRadius) ||
          (DLight1 < -CurrLightRadius && DLight2 < -CurrLightRadius))
      {
        return;
      }
    } else {
      if (D1 <= 0.0f && D2 <= 0.0f) return;
    }
  }
#endif
  if (!shadowslight) {
    if (!ClipVerts(v1, v2, false)) return;
  } else {
    if (!ClipVertsWithLight(v1, v2, CurrLightPos, CurrLightRadius, false)) return;
  }

  if (Mirror) {
    // clip seg with mirror plane
    const float Dist1 = DotProduct(v1, Mirror->normal)-Mirror->dist;
    const float Dist2 = DotProduct(v2, Mirror->normal)-Mirror->dist;

    if (Dist1 <= 0.0f && Dist2 <= 0.0f) return;

    // and clip it while we are here
         if (Dist1 > 0.0f && Dist2 <= 0.0f) v2 = v1+(v2-v1)*Dist1/(Dist1-Dist2);
    else if (Dist2 > 0.0f && Dist1 <= 0.0f) v1 = v2+(v1-v2)*Dist2/(Dist2-Dist1);
  }

  // for 2-sided line, determine if it can be skipped
  if (seg->backsector && (seg->linedef->flags&ML_TWOSIDED) != 0) {
    if (seg->linedef->alpha != 1.0f) return; //k8: skip translucent walls (for now)
    if (!IsSegAClosedSomething(Level, seg)) return;
  }

  AddClipRange(PointToClipAngle(v2), PointToClipAngle(v1));
}


//==========================================================================
//
//  VViewClipper::ClipAddSubsectorSegs
//
//==========================================================================
void VViewClipper::ClipAddSubsectorSegs (subsector_t *Sub, bool shadowslight, TPlane *Mirror, const TVec &CurrLightPos, float CurrLightRadius) {
  guard(VViewClipper::ClipAddSubsectorSegs);

  if (!clip_enabled) return;

  for (int i = 0; i < Sub->numlines; ++i) {
    const seg_t *seg = &Level->Segs[Sub->firstline+i];
    CheckAddClipSeg(seg, shadowslight, Mirror, CurrLightPos, CurrLightRadius);
  }

  if (Sub->poly && clip_with_polyobj) {
    seg_t **polySeg = Sub->poly->segs;
    for (int polyCount = Sub->poly->numsegs; --polyCount; ++polySeg) {
      const seg_t *seg = *polySeg;
      CheckAddClipSeg(seg, shadowslight, nullptr, CurrLightPos, CurrLightRadius);
    }
  }
  unguard;
}
