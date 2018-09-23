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
