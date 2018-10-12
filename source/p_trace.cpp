//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
#include "gamedefs.h"
#include "sv_local.h"


//==========================================================================
//
//  VLevel::CheckPlane
//
//==========================================================================
bool VLevel::CheckPlane (linetrace_t &Trace, const sec_plane_t *Plane) const {
  if (Plane->flags&Trace.PlaneNoBlockFlags) return true; // plane doesn't block

  const float OrgDist = DotProduct(Trace.LineStart, Plane->normal)-Plane->dist;
  if (OrgDist < -0.1) return true; // ignore back side

  const float HitDist = DotProduct(Trace.LineEnd, Plane->normal)-Plane->dist;
  if (HitDist >= -0.1) return true; // didn't cross plane

  if (Plane->pic == skyflatnum) return false; // hit sky, don't clip

  // hit Plane
  Trace.LineEnd -= (Trace.LineEnd-Trace.LineStart)*HitDist/(HitDist-OrgDist);
  Trace.HitPlaneNormal = Plane->normal;

  // crosses Plane
  return false;
}


//==========================================================================
//
//  VLevel::CheckPlanes
//
//==========================================================================
bool VLevel::CheckPlanes (linetrace_t &Trace, sector_t *Sec) const {
  guard(VLevel::CheckPlanes);
  sec_region_t *StartReg = SV_PointInRegion(Sec, Trace.LineStart);

  if (StartReg != nullptr) {
    for (sec_region_t *Reg = StartReg; Reg != nullptr; Reg = Reg->next) {
      if (!CheckPlane(Trace, Reg->floor)) return false; // hit floor
      if (!CheckPlane(Trace, Reg->ceiling)) return false; // hit ceiling
    }

    for (sec_region_t *Reg = StartReg->prev; Reg != nullptr; Reg = Reg->prev) {
      if (!CheckPlane(Trace, Reg->floor)) return false; // hit floor
      if (!CheckPlane(Trace, Reg->ceiling)) return false; // hit ceiling
    }
  }

  return true;
  unguard;
}


//==========================================================================
//
//  VLevel::CheckLine
//
//==========================================================================
bool VLevel::CheckLine (linetrace_t &Trace, seg_t *Seg) const {
  guard(VLevel::CheckLine);
  line_t *line;
  int s1;
  int s2;
  sector_t *front;
  float frac;
  float num;
  float den;
  TVec hit_point;

  line = Seg->linedef;
  if (!line) return true;

  // allready checked other side?
  if (line->validcount == validcount) return true;

  line->validcount = validcount;

  s1 = Trace.Plane.PointOnSide2(*line->v1);
  s2 = Trace.Plane.PointOnSide2(*line->v2);

  // line isn't crossed?
  if (s1 == s2) return true;

  s1 = line->PointOnSide2(Trace.Start);
  s2 = line->PointOnSide2(Trace.End);

  // line isn't crossed?
  if (s1 == s2 || (s1 == 2 && s2 == 0)) return true;

  // crosses a two sided line
  front = (s1 == 0 || s1 == 2 ? line->frontsector : line->backsector);

  // intercept vector
  // don't need to check if den == 0, because then planes are paralel
  // (they will never cross) or it's the same plane (also rejected)
  den = DotProduct(Trace.Delta, line->normal);
  num = line->dist-DotProduct(Trace.Start, line->normal);
  frac = num/den;
  hit_point = Trace.Start+frac*Trace.Delta;

  Trace.LineEnd = hit_point;

  if (front) {
    if (!CheckPlanes(Trace, front)) return false;
  }
  Trace.LineStart = Trace.LineEnd;

  if (line->flags&ML_TWOSIDED) {
    // crosses a two sided line
    opening_t *open;

    open = SV_LineOpenings(line, hit_point, Trace.PlaneNoBlockFlags);
    while (open) {
      if (open->bottom <= hit_point.z && open->top >= hit_point.z) return true;
      open = open->next;
    }
  }

  // hit line
  if (s1 == 0 || s1 == 2) {
    Trace.HitPlaneNormal = line->normal;
  } else {
    Trace.HitPlaneNormal = -line->normal;
  }

  if (!(line->flags&ML_TWOSIDED)) Trace.SightEarlyOut = true;
  return false;
  unguard;
}


//==========================================================================
//
//  VLevel::CrossSubsector
//
//  Returns true if trace crosses the given subsector successfully.
//
//==========================================================================
bool VLevel::CrossSubsector (linetrace_t &Trace, int num) const {
  guard(VLevel::CrossSubsector);
  subsector_t *sub;
  int count;
  seg_t *seg;
  int polyCount;
  seg_t **polySeg;

  sub = &Subsectors[num];

  if (sub->poly) {
    // check the polyobj in the subsector first
    polyCount = sub->poly->numsegs;
    polySeg = sub->poly->segs;
    while (polyCount--) {
      if (!CheckLine(Trace, *polySeg++)) return false;
    }
  }

  // check lines
  count = sub->numlines;
  seg = &Segs[sub->firstline];

  for ( ; count ; seg++, count--) {
    if (!CheckLine(Trace, seg)) return false;
  }
  // passed the subsector ok
  return true;
  unguard;
}


//==========================================================================
//
//  VLevel::CrossBSPNode
//
//  Returns true if trace crosses the given node successfully.
//
//==========================================================================
bool VLevel::CrossBSPNode (linetrace_t &Trace, int BspNum) const {
  guard(VLevel::CrossBSPNode);
  if (BspNum == -1) return CrossSubsector(Trace, 0);

  if (!(BspNum&NF_SUBSECTOR)) {
    node_t *Bsp = &Nodes[BspNum];

    // decide which side the start point is on
    int Side = Bsp->PointOnSide2(Trace.Start);
    if (Side == 2) Side = 0; // an "on" should cross both sides

    // cross the starting side
    if (!CrossBSPNode(Trace, Bsp->children[Side])) return false;

    // the partition plane is crossed here
    if (Side == Bsp->PointOnSide2(Trace.End)) return true; // the line doesn't touch the other side

    // cross the ending side
    return CrossBSPNode(Trace, Bsp->children[Side^1]);
  }

  return CrossSubsector(Trace, BspNum&(~NF_SUBSECTOR));
  unguard;
}


//==========================================================================
//
//  VLevel::TraceLine
//
//==========================================================================
bool VLevel::TraceLine (linetrace_t &Trace, const TVec &Start, const TVec &End, int PlaneNoBlockFlags) const {
  guard(VLevel::TraceLine);
  ++validcount;

  TVec realEnd = End;

  //k8: HACK!
  if (realEnd.x == Start.x && realEnd.y == Start.y) {
    //fprintf(stderr, "TRACE: same point!\n");
    realEnd.y += 0.2;
  }

  Trace.Start = Start;
  Trace.End = realEnd;
  Trace.Delta = realEnd-Start;

  Trace.Plane.SetPointDirXY(Start, Trace.Delta);

  Trace.LineStart = Trace.Start;

  Trace.PlaneNoBlockFlags = PlaneNoBlockFlags;
  Trace.SightEarlyOut = false;

  // the head node is the last node output
  if (CrossBSPNode(Trace, NumNodes-1)) {
    Trace.LineEnd = realEnd;
    return CheckPlanes(Trace, PointInSubsector(realEnd)->sector);
  }
  return false;
  unguard;
}


//==========================================================================
//
//  Script natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VLevel, TraceLine) {
  P_GET_PTR(TVec, HitNormal);
  P_GET_PTR(TVec, HitPoint);
  P_GET_VEC(End);
  P_GET_VEC(Start);
  P_GET_SELF;
  linetrace_t Trace;
  bool Ret = Self->TraceLine(Trace, Start, End, SPF_NOBLOCKING);
  *HitPoint = Trace.LineEnd;
  *HitNormal = Trace.HitPlaneNormal;
  RET_BOOL(Ret);
}


// ////////////////////////////////////////////////////////////////////////// //
#define FRACBITS  (16)
#define FRACUNIT  (1<<FRACBITS)

#define FL(x) ((float)(x)/(float)FRACUNIT)
#define FX(x) (fixed_t)((x)*FRACUNIT)

// mapblocks are used to check movement against lines and things
#define MAPBLOCKUNITS  (128)
#define MAPBLOCKSIZE   (MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT  (FRACBITS+7)
#define MAPBTOFRAC     (MAPBLOCKSHIFT-FRACBITS)


// ////////////////////////////////////////////////////////////////////////// //
typedef int fixed_t;

#define MAX_ST_INTERCEPTS  (65536)

static intercept_t intercepts[MAX_ST_INTERCEPTS];
static int interUsed;


struct SightTraceInfo {
  TVec Start;
  TVec End;
  TVec Delta;
  TPlane Plane;
  bool EarlyOut;
  TVec LineStart;
  TVec LineEnd;
  //TArray<intercept_t> Intercepts;
};


//==========================================================================
//
//  SightCheckPlane
//
//==========================================================================
static bool SightCheckPlane (const SightTraceInfo &Trace, const sec_plane_t *Plane) {
  if (Plane->flags&SPF_NOBLOCKSIGHT) return true; // plane doesn't block

  const float OrgDist = DotProduct(Trace.LineStart, Plane->normal)-Plane->dist;
  if (OrgDist < -0.1) return true; // ignore back side

  const float HitDist = DotProduct(Trace.LineEnd, Plane->normal)-Plane->dist;
  if (HitDist >= -0.1) return true; // didn't cross plane

  if (Plane->pic == skyflatnum) return false; // hit sky, don't clip

  // crosses plane
  return false;
}


//==========================================================================
//
//  SightCheckPlanes
//
//==========================================================================
static bool SightCheckPlanes (const SightTraceInfo &Trace, sector_t *Sec) {
  if (Sec->topregion == Sec->botregion) return true; // don't bother with planes if there are no 3D floors

  sec_region_t *StartReg = SV_PointInRegion(Sec, Trace.LineStart);

  if (StartReg != nullptr) {
    for (sec_region_t *Reg = StartReg; Reg; Reg = Reg->next) {
      if (!SightCheckPlane(Trace, Reg->floor)) return false; // hit floor
      if (!SightCheckPlane(Trace, Reg->ceiling)) return false; // hit ceiling
    }

    for (sec_region_t *Reg = StartReg->prev; Reg != nullptr; Reg = Reg->prev) {
      if (!SightCheckPlane(Trace, Reg->floor)) return false; // hit floor
      if (!SightCheckPlane(Trace, Reg->ceiling)) return false; // hit ceiling
    }
  }

  return true;
}


//==========================================================================
//
//  SightTraverse
//
//==========================================================================
static bool SightTraverse (SightTraceInfo &Trace, const intercept_t *in) {
  line_t *li = in->line;
  int s1 = li->PointOnSide2(Trace.Start);
  sector_t *front = (s1 == 0 || s1 == 2 ? li->frontsector : li->backsector);
  TVec hit_point = Trace.Start+in->frac*Trace.Delta;
  Trace.LineEnd = hit_point;
  if (!SightCheckPlanes(Trace, front)) return false;
  Trace.LineStart = Trace.LineEnd;

  // crosses a two sided line
  opening_t *open = SV_LineOpenings(li, hit_point, SPF_NOBLOCKSIGHT);
  while (open) {
    if (open->bottom <= hit_point.z && open->top >= hit_point.z) return true;
    open = open->next;
  }

  return false; // stop
}


//==========================================================================
//
//  SightTraverseIntercepts
//
//  Returns true if the traverser function returns true for all lines
//
//==========================================================================
static bool SightTraverseIntercepts (SightTraceInfo &Trace, sector_t *EndSector) {
  int count = interUsed; //Trace.Intercepts.length();

  // calculate intercept distance
  intercept_t *scan = intercepts; //Trace.Intercepts.ptr();
  for (int i = 0; i < count; ++i, ++scan) {
    const float den = DotProduct(scan->line->normal, Trace.Delta);
    const float num = scan->line->dist-DotProduct(Trace.Start, scan->line->normal);
    scan->frac = num/den;
  }

  // go through in order
  intercept_t *in = nullptr; // shut up compiler warning
  while (count--) {
    float dist = 99999.0;
    scan = intercepts; //Trace.Intercepts.ptr();
    for (int i = 0; i < interUsed/*Trace.Intercepts.length()*/; ++i, ++scan) {
      if (scan->frac < dist) {
        dist = scan->frac;
        in = scan;
      }
    }
    if (!SightTraverse(Trace, in)) return false; // don't bother going farther
    in->frac = 99999.0;
  }

  Trace.LineEnd = Trace.End;
  return SightCheckPlanes(Trace, EndSector);
}


//==========================================================================
//
//  SightCheckLine
//
//==========================================================================
static bool SightCheckLine (SightTraceInfo &Trace, line_t *ld) {
  if (ld->validcount == validcount) return true;

  ld->validcount = validcount;

  float dot1 = DotProduct(*ld->v1, Trace.Plane.normal)-Trace.Plane.dist;
  float dot2 = DotProduct(*ld->v2, Trace.Plane.normal)-Trace.Plane.dist;

  if (dot1*dot2 >= 0) return true; // line isn't crossed

  dot1 = DotProduct(Trace.Start, ld->normal)-ld->dist;
  dot2 = DotProduct(Trace.End, ld->normal)-ld->dist;

  if (dot1*dot2 >= 0) return true; // line isn't crossed

  // try to early out the check
  if (!ld->backsector || !(ld->flags&ML_TWOSIDED) || (ld->flags&ML_BLOCKEVERYTHING)) return false; // stop checking

  // store the line for later intersection testing
  if (interUsed < MAX_ST_INTERCEPTS) {
    intercept_t *In = &intercepts[interUsed++];//Trace.Intercepts.Alloc();
    In->line = ld;
  }

  return true;
}


//==========================================================================
//
//  SightBlockLinesIterator
//
//==========================================================================
static bool SightBlockLinesIterator (SightTraceInfo &Trace, const VLevel *level, int x, int y) {
  int offset = y*level->BlockMapWidth+x;
  polyblock_t *polyLink = level->PolyBlockMap[offset];
  while (polyLink) {
    if (polyLink->polyobj) {
      // only check non-empty links
      if (polyLink->polyobj->validcount != validcount) {
        seg_t **segList = polyLink->polyobj->segs;
        for (int i = 0; i < polyLink->polyobj->numsegs; ++i, ++segList) {
          if (!SightCheckLine(Trace, (*segList)->linedef)) return false;
        }
        polyLink->polyobj->validcount = validcount;
      }
    }
    polyLink = polyLink->next;
  }

  offset = *(level->BlockMap+offset);

  for (const vint32 *list = level->BlockMapLump+offset+1; *list != -1; ++list) {
    if (!SightCheckLine(Trace, &level->Lines[*list])) return false;
  }

  return true; // everything was checked
}


//==========================================================================
//
//  SightPathTraverse
//
//  Traces a line from x1,y1 to x2,y2, calling the traverser function for
//  each. Returns true if the traverser function returns true for all lines
//
//==========================================================================
static bool SightPathTraverse (SightTraceInfo &Trace, const VLevel *level, sector_t *EndSector) {
  float x1 = Trace.Start.x;
  float y1 = Trace.Start.y;
  float x2 = Trace.End.x;
  float y2 = Trace.End.y;
  float xstep, ystep;
  float partialx, partialy;
  float xintercept, yintercept;
  int mapx, mapy, mapxstep, mapystep;

  ++validcount;
  //Trace.Intercepts.Clear();
  interUsed = 0;

  if (((FX(x1-level->BlockMapOrgX))&(MAPBLOCKSIZE-1)) == 0) x1 += 1.0; // don't side exactly on a line
  if (((FX(y1-level->BlockMapOrgY))&(MAPBLOCKSIZE-1)) == 0) y1 += 1.0; // don't side exactly on a line

  //k8: check if `Length()` and `SetPointDirXY()` are happy
  if (x1 == x2 && y1 == y2) { x2 += 0.01; y2 += 0.01; }

  Trace.Delta = Trace.End-Trace.Start;
  Trace.Plane.SetPointDirXY(Trace.Start, Trace.Delta);
  Trace.EarlyOut = false;
  Trace.LineStart = Trace.Start;

  x1 -= level->BlockMapOrgX;
  y1 -= level->BlockMapOrgY;
  int xt1 = MapBlock(x1);
  int yt1 = MapBlock(y1);

  x2 -= level->BlockMapOrgX;
  y2 -= level->BlockMapOrgY;
  int xt2 = MapBlock(x2);
  int yt2 = MapBlock(y2);

  // points should never be out of bounds, but check once instead of each block
  // FIXME:TODO: k8: allow traces outside of blockmap (do line clipping)
  if (xt1 < 0 || yt1 < 0 || xt1 >= level->BlockMapWidth || yt1 >= level->BlockMapHeight ||
      xt2 < 0 || yt2 < 0 || xt2 >= level->BlockMapWidth || yt2 >= level->BlockMapHeight)
  {
    Trace.EarlyOut = true;
    return false;
  }

  if (xt2 > xt1) {
    mapxstep = 1;
    partialx = 1.0-FL((FX(x1)>>MAPBTOFRAC)&(FRACUNIT-1));
    ystep = (y2-y1)/fabs(x2-x1);
  } else if (xt2 < xt1) {
    mapxstep = -1;
    partialx = FL((FX(x1)>>MAPBTOFRAC)&(FRACUNIT-1));
    ystep = (y2-y1)/fabs(x2-x1);
  } else {
    mapxstep = 0;
    partialx = 1.0;
    ystep = 256.0;
  }
  yintercept = FL(FX(y1)>>MAPBTOFRAC)+partialx*ystep;

  if (yt2 > yt1) {
    mapystep = 1;
    partialy = 1.0-FL((FX(y1)>>MAPBTOFRAC)&(FRACUNIT-1));
    xstep = (x2-x1)/fabs(y2-y1);
  } else if (yt2 < yt1) {
    mapystep = -1;
    partialy = FL((FX(y1)>>MAPBTOFRAC)&(FRACUNIT-1));
    xstep = (x2-x1)/fabs(y2-y1);
  } else {
    mapystep = 0;
    partialy = 1.0;
    xstep = 256.0;
  }
  xintercept = FL(FX(x1)>>MAPBTOFRAC)+partialy*xstep;

  // [RH] fix for traces that pass only through blockmap corners. in that case,
  // xintercept and yintercept can both be set ahead of mapx and mapy, so the
  // for loop would never advance anywhere.
  if (fabs(xstep) == 1.0 && fabs(ystep) == 1.0) {
    if (ystep < 0.0) partialx = 1.0-partialx;
    if (xstep < 0.0) partialy = 1.0-partialy;
    if (partialx == partialy) { xintercept = xt1; yintercept = yt1; }
  }

  // step through map blocks
  // count is present to prevent a round off error from skipping the break
  mapx = xt1;
  mapy = yt1;

  //k8: zdoom is using 1000 here; why?
  for (int count = 0; count < /*64*/100; ++count) {
    if (!SightBlockLinesIterator(Trace, level, mapx, mapy)) {
      Trace.EarlyOut = true;
      return false; // early out
    }

    if (mapx == xt2 && mapy == yt2) break;

    // [RH] Handle corner cases properly instead of pretending they don't exist
    if ((int)yintercept == mapy) {
      yintercept += ystep;
      mapx += mapxstep;
      if (mapx == xt2) mapxstep = 0;
    } else if ((int)xintercept == mapx) {
      xintercept += xstep;
      mapy += mapystep;
      if (mapy == yt2) mapystep = 0;
    } else if ((int)yintercept == mapy && (int)xintercept == mapx) {
      // the trace is exiting a block through its corner. not only does the block
      // being entered need to be checked (which will happen when this loop
      // continues), but the other two blocks adjacent to the corner also need to
      // be checked.
      if (!SightBlockLinesIterator(Trace, level, mapx+mapxstep, mapy) ||
          !SightBlockLinesIterator(Trace, level, mapx, mapy+mapystep))
      {
        return false;
      }
      xintercept += xstep;
      yintercept += ystep;
      mapx += mapxstep;
      mapy += mapystep;
      if (mapx == xt2) mapxstep = 0;
      if (mapy == yt2) mapystep = 0;
    } else {
      // stop traversing, because somebody screwed up
      //count = 64; //k8: does `break` forbidden by some religious taboo?
      break;
    }
  }

  // couldn't early out, so go through the sorted list
  return SightTraverseIntercepts(Trace, EndSector);
}


//==========================================================================
//
//  SightPathTraverse2
//
//  Rechecks Trace.Intercepts with different ending z value.
//
//==========================================================================
static bool SightPathTraverse2 (SightTraceInfo &Trace, sector_t *EndSector) {
  Trace.Delta = Trace.End-Trace.Start;
  Trace.LineStart = Trace.Start;
  return SightTraverseIntercepts(Trace, EndSector);
}


//==========================================================================
//
//  VLevel::CastCanSee
//
//  doesn't check pvs or reject
//
//==========================================================================
bool VLevel::CastCanSee (const TVec &org, const TVec &dest, float radius) const {
  SightTraceInfo Trace;

  if (length2DSquared(org-dest) < 1) return true;

  //sector_t *Sector = PointInSubsector(org)->sector;
  sector_t *OtherSector = PointInSubsector(dest)->sector;

  // killough 4/19/98: make fake floors and ceilings block monster view
  /*
  if ((Sector->heightsec &&
       ((org.z+Height <= Sector->heightsec->floor.GetPointZ(org.x, org.y) &&
         dest.z >= Sector->heightsec->floor.GetPointZ(dest.x, dest.y)) ||
        (org.z >= Sector->heightsec->ceiling.GetPointZ (org.x, org.y) &&
         dest.z+Height <= Sector->heightsec->ceiling.GetPointZ (dest.x, dest.y))))
     ||
      (Other->Sector->heightsec &&
       ((dest.z+Other->Height <= Other->Sector->heightsec->floor.GetPointZ (dest.x, dest.y) &&
         org.z >= Other->Sector->heightsec->floor.GetPointZ (org.x, org.y)) ||
        (dest.z >= Other->Sector->heightsec->ceiling.GetPointZ (dest.x, dest.y) &&
         org.z+Other->Height <= Other->Sector->heightsec->ceiling.GetPointZ (org.x, org.y)))))
  {
    return false;
  }
  */

  static const float xmult[3] = { 0.0f, -1.0f, 1.0f };
  const float xofs = radius*0.73f;

  for (int d = 0; d < 3; ++d) {
    // an unobstructed LOS is possible
    // now look from eyes of t1 to any part of t2
    Trace.Start = org;
    Trace.End = dest;
    Trace.End.x += xofs*xmult[d];

    // check middle
    if (SightPathTraverse(Trace, this, OtherSector)) return true;
    if (Trace.EarlyOut) {
      if (radius < 12) return false; // player is 16
      continue;
    }

    // check up and down
    if (radius >= 12) {
      Trace.End = dest;
      Trace.End.x += xofs*xmult[d];
      Trace.End.z += radius*0.73f;
      if (SightPathTraverse2(Trace, OtherSector)) return true;

      Trace.End = dest;
      Trace.End.x += xofs*xmult[d];
      Trace.End.z -= radius*0.73f;
      if (SightPathTraverse2(Trace, OtherSector)) return true;
    }

    if (radius < 12) break; // player is 16
  }

  return false;
}


//==========================================================================
//
//  VLevel::HasAny2SLinesAtRadius
//
//==========================================================================
bool VLevel::HasAny2SLinesAtRadius (const TVec &org, float radius) {
  if (radius < 2) return false; // nobody cares

  const int xl = MapBlock(org.x-radius-BlockMapOrgX);
  const int xh = MapBlock(org.x+radius-BlockMapOrgX);
  const int yl = MapBlock(org.y-radius-BlockMapOrgY);
  const int yh = MapBlock(org.y+radius-BlockMapOrgY);

  ++validcount; // used to make sure we only process a line once

  for (int bx = xl; bx <= xh; ++bx) {
    for (int by = yl; by <= yh; ++by) {
      line_t *ld;
      for (VBlockLinesIterator It(this, bx, by, &ld); It.GetNext(); ) {
        if (ld->backsector) return true; // don't check flags, check sector link
      }
    }
  }

  return false;
}
