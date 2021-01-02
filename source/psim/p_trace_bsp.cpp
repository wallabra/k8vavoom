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
//**  Copyright (C) 2018-2021 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, version 3 of the License ONLY.
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
#include "../gamedefs.h"
#include "../server/sv_local.h"

static VCvarB dbg_bsp_trace_strict_flats("dbg_bsp_trace_strict_flats", false, "use strict checks for flats?", /*CVAR_Archive|*/CVAR_PreInit);


//**************************************************************************
//
// BSP raycasting
//
//**************************************************************************

//==========================================================================
//
//  PlaneFlagsToLineFlags
//
//==========================================================================
/*
static inline VVA_CHECKRESULT VVA_OKUNUSED unsigned PlaneFlagsToLineFlags (unsigned planeblockflags) {
  return
    ((planeblockflags&SPF_NOBLOCKING) ? ((ML_BLOCKING|ML_BLOCKEVERYTHING)|(planeblockflags&SPF_PLAYER ? ML_BLOCKPLAYERS : 0u)|(planeblockflags&SPF_MONSTER ? ML_BLOCKMONSTERS : 0u)) : 0u)|
    ((planeblockflags&SPF_NOBLOCKSIGHT) ? ML_BLOCKSIGHT : 0u)|
    ((planeblockflags&SPF_NOBLOCKSHOOT) ? ML_BLOCKHITSCAN : 0u);
  //ML_BLOCK_FLOATERS      = 0x00040000u,
  //ML_BLOCKPROJECTILE     = 0x01000000u,
}
*/


//==========================================================================
//
//  CheckPlanes
//
//==========================================================================
static bool CheckPlanes (linetrace_t &trace, sector_t *sec) {
  TVec outHit(0.0f, 0.0f, 0.0f), outNorm(0.0f, 0.0f, 0.0f);

  if (!VLevel::CheckPassPlanes(sec, trace.LineStart, trace.LineEnd, trace.PlaneNoBlockFlags, &outHit, &outNorm, nullptr, &trace.HitPlane)) {
    // hit floor or ceiling
    trace.LineEnd = outHit;
    trace.HitPlaneNormal = outNorm;
    return false;
  }

  return true;
}


//==========================================================================
//
//  VLevel::CheckLine
//
//  returns `true` if the line isn't crossed
//  returns `false` if the line blocked the ray
//
//==========================================================================
bool VLevel::CheckLine (linetrace_t &trace, seg_t *seg) const {
  line_t *line = seg->linedef;
  if (!line) return true; // ignore minisegs, they cannot block anything

  // allready checked other side?
  if (line->validcount == validcount) return true;
  line->validcount = validcount;

  #if 0
    int s1 = trace.LinePlane.PointOnSide2(*line->v1);
    int s2 = trace.LinePlane.PointOnSide2(*line->v2);

    // line isn't crossed?
    if (s1 == s2) return true;

    s1 = line->PointOnSide2(trace.Start);
    s2 = line->PointOnSide2(trace.End);

    // line isn't crossed?
    if (s1 == s2 || (s1 == 2 && s2 == 0)) return true;

    const bool backside = (s1 == 1);
  #else
    // k8: dunno, this doesn't make any difference

    // signed distances from the line points to the trace line plane
    float dot1 = trace.LinePlane.PointDistance(*line->v1);
    float dot2 = trace.LinePlane.PointDistance(*line->v2);

    // do not use multiplication to check: zero speedup, lost accuracy
    //if (dot1*dot2 >= 0) return true; // line isn't crossed
    if (dot1 < 0.0f && dot2 < 0.0f) return true; // didn't reached back side
    // if the line is parallel to the trace plane, ignore it
    if (dot1 >= 0.0f && dot2 >= 0.0f) return true; // didn't reached front side

    // signed distances from the trace points to the line plane
    dot1 = line->PointDistance(trace.Start);
    dot2 = line->PointDistance(trace.End);

    // do not use multiplication to check: zero speedup, lost accuracy
    //if (dot1*dot2 >= 0) return true; // line isn't crossed
    if (dot1 < 0.0f && dot2 < 0.0f) return true; // didn't reached back side
    // if the trace is parallel to the line plane, ignore it
    if (dot1 >= 0.0f && dot2 >= 0.0f) return true; // didn't reached front side

    const bool backside = (dot1 < 0.0f);
  #endif

  // crosses a two sided line
  //sector_t *front = (s1 == 0 || s1 == 2 ? line->frontsector : line->backsector);
  sector_t *front = (backside ? line->backsector : line->frontsector);

  // intercept vector
  // no need to check if den == 0, because then planes are parallel
  // (they will never cross) or it's the same plane (also rejected)
  const float den = DotProduct(trace.Delta, line->normal);
  const float num = line->dist-DotProduct(trace.Start, line->normal);
  const float frac = num/den;
  TVec hitpoint = trace.Start+frac*trace.Delta;

  trace.LineEnd = hitpoint;

  if (front) {
    if (!CheckPlanes(trace, front)) return false;
  }
  trace.LineStart = trace.LineEnd;

  if (!(line->flags&ML_TWOSIDED) || (line->flags&trace.LineBlockFlags)) {
    trace.Flags |= linetrace_t::SightEarlyOut;
  } else {
    if (line->flags&ML_TWOSIDED) {
      // crossed a two sided line
      opening_t *open = SV_LineOpenings(line, hitpoint, trace.PlaneNoBlockFlags&SPF_FLAG_MASK);
      if (dbg_bsp_trace_strict_flats) {
        while (open) {
          if (open->bottom < hitpoint.z && open->top > hitpoint.z) return true;
          open = open->next;
        }
      } else {
        while (open) {
          if (open->bottom <= hitpoint.z && open->top >= hitpoint.z) return true;
          open = open->next;
        }
      }
    }
  }

  // hit line
  //trace.HitPlaneNormal = (s1 == 0 || s1 == 2 ? line->normal : -line->normal);
  trace.HitPlaneNormal = (backside ? -line->normal : line->normal);
  trace.HitPlane = *line;
  trace.HitLine = line;

  return false;
}


//==========================================================================
//
//  VLevel::CrossSubsector
//
//  Returns true if trace crosses the given subsector successfully.
//
//==========================================================================
bool VLevel::CrossSubsector (linetrace_t &trace, int num) const {
  subsector_t *sub = &Subsectors[num];

  if (sub->HasPObjs()) {
    // check the polyobjects in the subsector first
    for (auto &&it : sub->PObjFirst()) {
      polyobj_t *pobj = it.value();
      seg_t **polySeg = pobj->segs;
      for (int polyCount = pobj->numsegs; polyCount--; ++polySeg) {
        if (!CheckLine(trace, *polySeg)) {
          trace.EndSubsector = sub;
          return false;
        }
      }
    }
  }

  // check lines
  seg_t *seg = &Segs[sub->firstline];
  for (int count = sub->numlines; count--; ++seg) {
    if (!CheckLine(trace, seg)) {
      trace.EndSubsector = sub;
      return false;
    }
  }

  // passed the subsector ok
  return true;
}


//==========================================================================
//
//  VLevel::CrossBSPNode
//
//  Returns true if trace crosses the given node successfully.
//
//==========================================================================
bool VLevel::CrossBSPNode (linetrace_t &trace, int bspnum) const {
  if (bspnum == -1) return CrossSubsector(trace, 0); // just in case

  if ((bspnum&NF_SUBSECTOR) == 0) {
    const node_t *bsp = &Nodes[bspnum];
    // decide which side the start point is on
    // if bit 1 is set (i.e. `(side&2) != 0`), the point lies on the plane
    const int side = bsp->PointOnSide2(trace.Start);
    // cross the starting side
    if (!CrossBSPNode(trace, bsp->children[side&1])) {
      // if on the plane, check other side
      if (side&2) return CrossBSPNode(trace, bsp->children[(side&1)^1]);
      // definitely blocked
      return false;
    }
    // the partition plane is crossed here
    // if not on the plane, and endpoint is on the same side, there's nothing more to do
    if (!(side&2) && side == bsp->PointOnSide2(trace.End)) return true; // the line doesn't touch the other side
    // cross the ending side
    return CrossBSPNode(trace, bsp->children[(side&1)^1]);
  } else {
    return CrossSubsector(trace, bspnum&(~NF_SUBSECTOR));
  }
}


//==========================================================================
//
//  VLevel::TraceLine
//
//  returns `true` if the line is not blocked
//  returns `fales` if the line was blocked, and sets hit normal/point
//
//==========================================================================
bool VLevel::TraceLine (linetrace_t &trace, const TVec &Start, const TVec &End, unsigned PlaneNoBlockFlags, unsigned moreLineBlockFlags) {
  trace.Start = Start;
  trace.End = End;
  trace.Delta = End-Start;
  trace.LineStart = Start;
  trace.PlaneNoBlockFlags = PlaneNoBlockFlags;
  trace.HitLine = nullptr;
  trace.Flags = 0;

  if (PlaneNoBlockFlags&SPF_NOBLOCKING) {
    moreLineBlockFlags |= ML_BLOCKING|ML_BLOCKEVERYTHING;
    if (PlaneNoBlockFlags&SPF_PLAYER) moreLineBlockFlags |= ML_BLOCKPLAYERS;
    if (PlaneNoBlockFlags&SPF_MONSTER) moreLineBlockFlags |= ML_BLOCKMONSTERS;
    //ML_BLOCK_FLOATERS      = 0x00040000u,
    //ML_BLOCKPROJECTILE     = 0x01000000u,
  }
  if (PlaneNoBlockFlags&SPF_NOBLOCKSIGHT) moreLineBlockFlags |= ML_BLOCKSIGHT;
  if (PlaneNoBlockFlags&SPF_NOBLOCKSHOOT) moreLineBlockFlags |= ML_BLOCKHITSCAN;

  trace.LineBlockFlags = moreLineBlockFlags;

  //k8: HACK!
  if (trace.Delta.x == 0.0f && trace.Delta.y == 0.0f) {
    // this is vertical trace; end subsector is known
    trace.EndSubsector = PointInSubsector(End);
    //trace.Plane.SetPointNormal3D(Start, TVec(0.0f, 0.0f, (trace.Delta.z >= 0.0f ? 1.0f : -1.0f))); // arbitrary orientation
    // point cannot hit anything!
    if (fabsf(trace.Delta.z) <= 0.0001f) {
      // always succeed
      trace.LineEnd = End;
      return true;
    }
    return CheckPlanes(trace, trace.EndSubsector->sector);
  } else {
    IncrementValidCount();
    trace.LinePlane.SetPointDirXY(Start, trace.Delta);
    // the head node is the last node output
    if (CrossBSPNode(trace, NumNodes-1)) {
      trace.LineEnd = End;
      // end subsector is known
      trace.EndSubsector = PointInSubsector(End);
      return CheckPlanes(trace, trace.EndSubsector->sector);
    }
  }
  return false;
}


//==========================================================================
//
//  Script natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VLevel, BSPTraceLine) {
  TVec Start, End;
  TVec *HitPoint;
  TVec *HitNormal;
  VOptParamInt noBlockFlags(SPF_NOBLOCKING);
  vobjGetParamSelf(Start, End, HitPoint, HitNormal, noBlockFlags);
  linetrace_t trace;
  bool res = Self->TraceLine(trace, Start, End, noBlockFlags);
  if (!res) {
    if (HitPoint) *HitPoint = trace.LineEnd;
    if (HitNormal) *HitNormal = trace.HitPlaneNormal;
  } else {
    if (HitPoint) *HitPoint = End;
    if (HitNormal) *HitNormal = TVec(0, 0, 1); // arbitrary
  }
  RET_BOOL(res);
}

IMPLEMENT_FUNCTION(VLevel, BSPTraceLineEx) {
  linetrace_t tracetmp;
  TVec Start, End;
  linetrace_t *tracep;
  VOptParamInt noBlockFlags(SPF_NOBLOCKING);
  vobjGetParamSelf(Start, End, tracep, noBlockFlags);
  if (!tracep) tracep = &tracetmp;
  RET_BOOL(Self->TraceLine(*tracep, Start, End, noBlockFlags));
}
