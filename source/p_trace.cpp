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
//**  Copyright (C) 2018-2020 Ketmar Dark
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
#include "gamedefs.h"
#include "sv_local.h"

// try to dance around line trace imprecision
// mostly used for lightmap tracing
#define VV_BMAP_TRACE_EXPERIMENT

static VCvarB dbg_bsp_trace_strict_flats("dbg_bsp_trace_strict_flats", false, "use strict checks for flats?", /*CVAR_Archive|*/CVAR_PreInit);
static VCvarB dbg_sight_trace_bsp("dbg_sight_trace_bsp", false, "use simple BSP raycast for sight (debug)?", /*CVAR_Archive|*/CVAR_PreInit);


//==========================================================================
//
//  CheckPlanes
//
//==========================================================================
static bool CheckPlanes (linetrace_t &trace, sector_t *sec) {
  TVec outHit(0.0f, 0.0f, 0.0f), outNorm(0.0f, 0.0f, 0.0f);

  if (!VLevel::CheckPassPlanes(sec, true/*sector bounds*/, trace.LineStart, trace.LineEnd, (unsigned)trace.PlaneNoBlockFlags, &outHit, &outNorm, nullptr, &trace.HitPlane)) {
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
//  returns `fales` if the line blocked the ray
//
//==========================================================================
bool VLevel::CheckLine (linetrace_t &trace, seg_t *seg) const {
  line_t *line = seg->linedef;
  if (!line) return true; // ignore minisegs, they cannot block anything

  // allready checked other side?
  if (line->validcount == validcount) return true;
  line->validcount = validcount;

  int s1 = trace.LinePlane.PointOnSide2(*line->v1);
  int s2 = trace.LinePlane.PointOnSide2(*line->v2);

  // line isn't crossed?
  if (s1 == s2) return true;

  s1 = line->PointOnSide2(trace.Start);
  s2 = line->PointOnSide2(trace.End);

  // line isn't crossed?
  if (s1 == s2 || (s1 == 2 && s2 == 0)) return true;

  // crosses a two sided line
  //sector_t *front = (s1 == 0 || s1 == 2 ? line->frontsector : line->backsector);
  sector_t *front = (s1 == 1 ? line->backsector : line->frontsector);

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

  if (line->flags&ML_TWOSIDED) {
    // crosses a two sided line
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

  // hit line
  //trace.HitPlaneNormal = (s1 == 0 || s1 == 2 ? line->normal : -line->normal);
  trace.HitPlaneNormal = (s1 == 1 ? -line->normal : line->normal);
  trace.HitPlane = *line;
  trace.HitLine = line;

  if (!(line->flags&ML_TWOSIDED)) trace.Flags |= linetrace_t::SightEarlyOut;
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
bool VLevel::CrossBSPNode (linetrace_t &trace, int BspNum) const {
  if (BspNum == -1) return CrossSubsector(trace, 0); // just in case

  if ((BspNum&NF_SUBSECTOR) == 0) {
    const node_t *bsp = &Nodes[BspNum];
    // decide which side the start point is on
    int side = bsp->PointOnSide2(trace.Start);
    bool both = (side == 2);
    //if (both) side = 0; // an "on" should cross both sides
    side &= 1;
    // cross the starting side
    if (!CrossBSPNode(trace, bsp->children[side])) {
      return (both ? CrossBSPNode(trace, bsp->children[side^1]) : false);
    }
    // the partition plane is crossed here
    if (!both && side == bsp->PointOnSide2(trace.End)) return true; // the line doesn't touch the other side
    // cross the ending side
    return CrossBSPNode(trace, bsp->children[side^1]);
  } else {
    return CrossSubsector(trace, BspNum&(~NF_SUBSECTOR));
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
bool VLevel::TraceLine (linetrace_t &trace, const TVec &Start, const TVec &End, int PlaneNoBlockFlags) {
  trace.Start = Start;
  trace.End = End;
  trace.Delta = End-Start;
  trace.LineStart = Start;
  trace.PlaneNoBlockFlags = PlaneNoBlockFlags;
  trace.HitLine = nullptr;
  trace.Flags = 0;

  //k8: HACK!
  if (trace.Delta.x == 0.0f && trace.Delta.y == 0.0f) {
    // this is vertical trace; end subsector is known
    trace.EndSubsector = PointInSubsector(End);
    //trace.Plane.SetPointNormal3D(Start, TVec(0.0f, 0.0f, (trace.Delta.z >= 0.0f ? 1.0f : -1.0f))); // arbitrary orientation
    if (trace.Delta.z == 0.0f) {
      // always succeed
      trace.LineEnd = End;
      return true;
    } else {
      return CheckPlanes(trace, trace.EndSubsector->sector);
    }
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
  if (res) {
    if (HitPoint) *HitPoint = trace.LineEnd;
    if (HitNormal) *HitNormal = trace.HitPlaneNormal;
  } else {
    if (HitPoint) *HitPoint = TVec(0, 0, 0);
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


// ////////////////////////////////////////////////////////////////////////// //
static intercept_t *intercepts = nullptr;
static unsigned interAllocated = 0;
static unsigned interUsed = 0;

static inline void EnsureFreeIntercept () {
  if (interAllocated <= interUsed) {
    unsigned oldAlloc = interAllocated;
    interAllocated = ((interUsed+4)|0xfffu)+1;
    intercepts = (intercept_t *)Z_Realloc(intercepts, interAllocated*sizeof(intercept_t));
    if (oldAlloc) GCon->Logf(NAME_Debug, "more interceptions allocated; interUsed=%u; allocated=%u (old=%u)", interUsed, interAllocated, oldAlloc);
  }
}


struct SightTraceInfo {
  TVec Start;
  TVec End;
  TVec Delta;
  TPlane Plane;
  bool EarlyOut; // `true` means "hit one-sided wall"
  TVec LineStart;
  TVec LineEnd;

  bool CheckBaseRegion;
  unsigned LineBlockMask;

  vuint32 PlaneNoBlockFlags;
  TVec HitPlaneNormal;
};


//==========================================================================
//
//  SightCheckPlanes
//
//==========================================================================
static bool SightCheckPlanes (SightTraceInfo &trace, sector_t *sec) {
  //k8: for some reason, real sight checks ignores base sector region
  return VLevel::CheckPassPlanes(sec, trace.CheckBaseRegion, trace.LineStart, trace.LineEnd, (unsigned)trace.PlaneNoBlockFlags, &trace.LineEnd, &trace.HitPlaneNormal, nullptr, nullptr);
}


//==========================================================================
//
//  SightTraverse
//
//==========================================================================
static bool SightTraverse (SightTraceInfo &trace, const intercept_t *in) {
  line_t *line = in->line;
  int s1 = line->PointOnSide2(trace.Start);
  sector_t *front = (s1 == 0 || s1 == 2 ? line->frontsector : line->backsector);
  //sector_t *front = (li->PointOnSideFri(trace.Start) ? li->frontsector : li->backsector);
  TVec hitpoint = trace.Start+in->frac*trace.Delta;
  trace.LineEnd = hitpoint;
  if (!SightCheckPlanes(trace, front)) return false;
  trace.LineStart = trace.LineEnd;

  if (line->flags&ML_TWOSIDED) {
    // crosses a two sided line
    opening_t *open = SV_LineOpenings(line, hitpoint, trace.PlaneNoBlockFlags&SPF_FLAG_MASK);
    while (open) {
      if (open->bottom <= hitpoint.z && open->top >= hitpoint.z) return true;
      open = open->next;
    }
  }

  // hit line
  trace.HitPlaneNormal = (s1 == 0 || s1 == 2 ? line->normal : -line->normal);

  if (!(line->flags&ML_TWOSIDED)) trace.EarlyOut = true;
  return false; // stop
}


/*
extern "C" {
  static int compareIcept (const void *aa, const void *bb, void *) {
    if (aa == bb) return 0;
    const intercept_t *a = (const intercept_t *)aa;
    const intercept_t *b = (const intercept_t *)bb;
    if (a->frac < b->frac) return -1;
    if (a->frac > b->frac) return 1;
    return 0;
  }
}
*/


//==========================================================================
//
//  SightTraverseIntercepts
//
//  Returns true if the traverser function returns true for all lines
//
//==========================================================================
static bool SightTraverseIntercepts (SightTraceInfo &trace, sector_t *EndSector) {
  int count = (int)interUsed;

  if (count > 0) {
    intercept_t *scan;
    /*
    if (resort) {
      // calculate intercept distance
      scan = intercepts;
      for (int i = count; i--; ++scan) {
        const float den = DotProduct(scan->line->normal, trace.Delta);
        const float num = scan->line->dist-DotProduct(trace.Start, scan->line->normal);
        scan->frac = num/den;
      }
      // sort intercepts
      timsort_r(intercepts, (size_t)count, sizeof(intercepts[0]), &compareIcept, nullptr);
    }
    */

    // go through in order
    scan = intercepts;
    for (int i = count; i--; ++scan) {
      if (!SightTraverse(trace, scan)) return false; // don't bother going farther
    }
  }

  trace.LineEnd = trace.End;
  return SightCheckPlanes(trace, EndSector);
}


//==========================================================================
//
//  SightCheckLine
//
//  return `true` if line is not crossed or put into intercept list
//  return `false` to stop checking due to blocking
//
//==========================================================================
static bool SightCheckLine (SightTraceInfo &trace, line_t *ld) {
  if (ld->validcount == validcount) return true;

  ld->validcount = validcount;

  float dot1 = DotProduct(*ld->v1, trace.Plane.normal)-trace.Plane.dist;
  float dot2 = DotProduct(*ld->v2, trace.Plane.normal)-trace.Plane.dist;

  // for some reason, `VV_BMAP_TRACE_EXPERIMENT` gives slightly less lightmap misses
  // let's hope it won't break sight checks...

  #ifdef VV_BMAP_TRACE_EXPERIMENT
  const float mul1 = dot1*dot2;
  //if (mul1 >= 0) return true; // line isn't crossed
  if (mul1 >= 0.01f) return true; // line isn't crossed
  #else
  if (dot1*dot2 >= 0) return true; // line isn't crossed
  #endif

  dot1 = DotProduct(trace.Start, ld->normal)-ld->dist;
  dot2 = DotProduct(trace.End, ld->normal)-ld->dist;

  #ifdef VV_BMAP_TRACE_EXPERIMENT
  const float mul2 = dot1*dot2;
  //if (mul2 >= 0) return true; // line isn't crossed
  if (mul2 >= 0.01f) return true; // line isn't crossed
  #else
  if (dot1*dot2 >= 0) return true; // line isn't crossed
  #endif

  #ifdef VV_BMAP_TRACE_EXPERIMENT
  if (mul1 >= 0 && mul2 >= 0 && mul1 <= 0.001f && mul2 <= 0.001f) return true;
  #endif

  // try to early out the check
  if (!ld->backsector || !(ld->flags&ML_TWOSIDED) || (ld->flags&trace.LineBlockMask)) {
    return false; // stop checking
  }

  // store the line for later intersection testing
  // distance
  const float den = DotProduct(ld->normal, trace.Delta);
  if (fabsf(den) < 0.00001f) return true; // wtf?!
  const float num = ld->dist-DotProduct(trace.Start, ld->normal);
  const float frac = num/den;
  intercept_t *icept;

  // find place to put our new record
  // this is usually faster than sorting records, as we are traversing blockmap
  // more-or-less in order
  EnsureFreeIntercept();
  if (interUsed > 0) {
    unsigned ipos = interUsed;
    while (ipos > 0 && frac < intercepts[ipos-1].frac) --ipos;
    // here we should insert at `ipos` position
    if (ipos == interUsed) {
      // as last
      icept = &intercepts[interUsed++];
    } else {
      // make room
      memmove(intercepts+ipos+1, intercepts+ipos, (interUsed-ipos)*sizeof(intercepts[0]));
      ++interUsed;
      icept = &intercepts[ipos];
    }
  } else {
    icept = &intercepts[interUsed++];
  }

  icept->line = ld;
  icept->frac = frac;

  return true;
}


//==========================================================================
//
//  SightBlockLinesIterator
//
//==========================================================================
static bool SightBlockLinesIterator (SightTraceInfo &trace, const VLevel *level, int x, int y) {
  int offset = y*level->BlockMapWidth+x;
  polyblock_t *polyLink = level->PolyBlockMap[offset];
  while (polyLink) {
    if (polyLink->polyobj) {
      // only check non-empty links
      if (polyLink->polyobj->validcount != validcount) {
        seg_t **segList = polyLink->polyobj->segs;
        for (int i = 0; i < polyLink->polyobj->numsegs; ++i, ++segList) {
          if (!SightCheckLine(trace, (*segList)->linedef)) return false;
        }
        polyLink->polyobj->validcount = validcount;
      }
    }
    polyLink = polyLink->next;
  }

  offset = *(level->BlockMap+offset);

  for (const vint32 *list = level->BlockMapLump+offset+1; *list != -1; ++list) {
    if (!SightCheckLine(trace, &level->Lines[*list])) return false;
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
static bool SightPathTraverse (SightTraceInfo &trace, VLevel *level, sector_t *EndSector) {
  VBlockMapWalker walker;

  interUsed = 0;
  //trace.Intercepts.Clear();

  {
    float x1 = trace.Start.x;
    float y1 = trace.Start.y;
    float x2 = trace.End.x;
    float y2 = trace.End.y;
    //k8: check if `Length()` and `SetPointDirXY()` are happy
    if (x1 == x2 && y1 == y2) { x2 += 0.02f; y2 += 0.02f; }

    trace.Delta = trace.End-trace.Start;
    trace.Plane.SetPointDirXY(trace.Start, trace.Delta);
    trace.EarlyOut = false;
    trace.LineStart = trace.Start;
  }

  if (walker.start(level, trace.Start.x, trace.Start.y, trace.End.x, trace.End.y)) {
    //++validcount;
    level->IncrementValidCount();
    int mapx, mapy;
    //int guard = 1000;
    while (walker.next(mapx, mapy)) {
      if (!SightBlockLinesIterator(trace, level, mapx, mapy)) {
        trace.EarlyOut = true;
        return false; // early out
      }
      //if (--guard == 0) Sys_Error("DDA walker fuckup!");
    }
    // couldn't early out, so go through the sorted list
    return SightTraverseIntercepts(trace, EndSector);
  }

  // out of map, see nothing
  return false;
}


//==========================================================================
//
//  SightPathTraverse2
//
//  Rechecks trace.Intercepts with different ending z value.
//
//==========================================================================
static bool SightPathTraverse2 (SightTraceInfo &trace, sector_t *EndSector) {
  trace.Delta = trace.End-trace.Start;
  trace.LineStart = trace.Start;
  return SightTraverseIntercepts(trace, EndSector);
}


//==========================================================================
//
//  isNotInsideBM
//
//  right edge is not included
//
//==========================================================================
static VVA_CHECKRESULT inline bool isNotInsideBM (const TVec &pos, const VLevel *level) {
  // horizontal check
  const int intx = (int)pos.x;
  const int intbx0 = (int)level->BlockMapOrgX;
  if (intx < intbx0 || intx >= intbx0+level->BlockMapWidth*MAPBLOCKUNITS) return true;
  // vertical checl
  const int inty = (int)pos.y;
  const int intby0 = (int)level->BlockMapOrgY;
  return (inty < intby0 || inty >= intby0+level->BlockMapHeight*MAPBLOCKUNITS);
}


//==========================================================================
//
//  VLevel::CastCanSee
//
//  doesn't check pvs or reject
//  if better sight is allowed, `orgdirRight` and `orgdirFwd` MUST be valid!
//
//==========================================================================
bool VLevel::CastCanSee (sector_t *Sector, const TVec &org, float myheight, const TVec &orgdirFwd, const TVec &orgdirRight,
                         const TVec &dest, float radius, float height, bool skipBaseRegion, sector_t *DestSector,
                         bool allowBetterSight, bool ignoreBlockAll, bool ignoreFakeFloors) {
  if (lengthSquared(org-dest) <= 1) return true;

  // if starting or ending point is out of blockmap bounds, don't bother tracing
  // we can get away with this, because nothing can see anything beyound the map extents
  if (isNotInsideBM(org, this)) return false;
  if (isNotInsideBM(dest, this)) return false;

  SightTraceInfo trace;

  if (!Sector) Sector = PointInSubsector(org)->sector;

  if (radius < 0.0f) radius = 0.0f;
  if (height < 0.0f) height = 0.0f;
  if (myheight < 0.0f) myheight = 0.0f;

  // killough 4/19/98: make fake floors and ceilings block view
  if (!ignoreFakeFloors && Sector->heightsec) {
    const sector_t *hs = Sector->heightsec;
    if ((org.z+myheight <= hs->floor.GetPointZClamped(org) && dest.z >= hs->floor.GetPointZClamped(dest)) ||
        (org.z >= hs->ceiling.GetPointZClamped(org) && dest.z+height <= hs->ceiling.GetPointZClamped(dest)))
    {
      return false;
    }
  }

  sector_t *OtherSector = DestSector;
  if (!OtherSector) OtherSector = PointInSubsector(dest)->sector;

  if (!ignoreFakeFloors && OtherSector->heightsec) {
    const sector_t *hs = OtherSector->heightsec;
    if ((dest.z+height <= hs->floor.GetPointZClamped(dest) && org.z >= hs->floor.GetPointZClamped(org)) ||
        (dest.z >= hs->ceiling.GetPointZClamped(dest) && org.z+myheight <= hs->ceiling.GetPointZClamped(org)))
    {
      return false;
    }
  }

  //if (length2DSquared(org-dest) <= 1) return true;

  trace.PlaneNoBlockFlags = SPF_NOBLOCKSIGHT|(ignoreFakeFloors ? SPF_IGNORE_FAKE_FLOORS : 0u);
  trace.LineBlockMask = (ignoreBlockAll ? 0 : ML_BLOCKEVERYTHING)|ML_BLOCKSIGHT;
  trace.CheckBaseRegion = !skipBaseRegion;

  if (!allowBetterSight || radius < 4.0f || height < 4.0f || myheight < 4.0f || dbg_sight_trace_bsp) {
    trace.Start = org;
    trace.Start.z += myheight*0.75f;
    trace.End = dest;
    trace.End.z += height*0.5f;
    //GCon->Log(NAME_Debug, "==================");
    if (!dbg_sight_trace_bsp) return SightPathTraverse(trace, this, OtherSector);
    linetrace_t ltr;
    return TraceLine(ltr,  trace.Start, trace.End, trace.PlaneNoBlockFlags);
  } else {
    /*static*/ const float ithmult[2] = { 0.15f, 0.85f };
    /*static*/ const float sidemult[3] = { 0.0f, -0.75f, 0.75f };
    /*static*/ const float fwdmult[2] = { 0.75f, 0.0f };
    //GCon->Logf("=== forward:(%g,%g,%g) ===", orgdirFwd.x, orgdirFwd.y, orgdirFwd.z);
    //GCon->Logf("=== right:(%g,%g,%g) ===", orgdirRight.x, orgdirRight.y, orgdirRight.z);
    for (unsigned myf = 0; myf < 2; ++myf) {
      TVec orgStartFwd = org+orgdirFwd*(radius*fwdmult[myf]);
      orgStartFwd.z += myheight*0.75f;
      for (unsigned myx = 0; myx < 3; ++myx) {
        // an unobstructed LOS is possible
        // now look from eyes of t1 to any part of t2
        TVec orgStart = orgStartFwd+orgdirRight*(radius*sidemult[myx]);
        trace.Start = orgStart;
        trace.End = dest;
        trace.End.z += height*0.5f;
        //GCon->Logf("myx=%u; itsz=*; org=(%g,%g,%g); dest=(%g,%g,%g); s=(%g,%g,%g); e=(%g,%g,%g)", myx, org.x, org.y, org.z, dest.x, dest.y, dest.z, trace.Start.x, trace.Start.y, trace.Start.z, trace.End.x, trace.End.y, trace.End.z);

        // check middle
        if (SightPathTraverse(trace, this, OtherSector)) return true;
        if (trace.EarlyOut) continue;

        // check up and down
        for (unsigned itsz = 0; itsz < 2; ++itsz) {
          trace.Start = orgStart;
          trace.End = dest;
          trace.End.z += height*ithmult[itsz];
          //GCon->Logf("myx=%u; itsz=%u; org=(%g,%g,%g); dest=(%g,%g,%g); s=(%g,%g,%g); e=(%g,%g,%g)", myx, itsz, org.x, org.y, org.z, dest.x, dest.y, dest.z, trace.Start.x, trace.Start.y, trace.Start.z, trace.End.x, trace.End.y, trace.End.z);
          if (SightPathTraverse2(trace, OtherSector)) return true;
        }
      }
    }
  }

  return false;
}


//==========================================================================
//
//  VLevel::CastLightRay
//
//  doesn't check pvs or reject
//
//==========================================================================
bool VLevel::CastLightRay (sector_t *Sector, const TVec &org, const TVec &dest, sector_t *DestSector) {
  // if starting or ending point is out of blockmap bounds, don't bother tracing
  // we can get away with this, because nothing can see anything beyound the map extents
  if (isNotInsideBM(org, this)) return false;
  if (isNotInsideBM(dest, this)) return false;

  if (lengthSquared(org-dest) <= 1) return true;

  SightTraceInfo trace;

  // killough 4/19/98: make fake floors and ceilings block view
  if (Sector->heightsec) {
    const sector_t *hs = Sector->heightsec;
    if ((org.z <= hs->floor.GetPointZClamped(org) && dest.z >= hs->floor.GetPointZClamped(dest)) ||
        (org.z >= hs->ceiling.GetPointZClamped(org) && dest.z <= hs->ceiling.GetPointZClamped(dest)))
    {
      return false;
    }
  }

  sector_t *OtherSector = DestSector;
  if (!OtherSector) OtherSector = PointInSubsector(dest)->sector;

  if (OtherSector->heightsec) {
    const sector_t *hs = OtherSector->heightsec;
    if ((dest.z <= hs->floor.GetPointZClamped(dest) && org.z >= hs->floor.GetPointZClamped(org)) ||
        (dest.z >= hs->ceiling.GetPointZClamped(dest) && org.z <= hs->ceiling.GetPointZClamped(org)))
    {
      return false;
    }
  }

  //if (length2DSquared(org-dest) <= 1) return true;

  //trace.PlaneNoBlockFlags = blockflags;
  trace.PlaneNoBlockFlags = SPF_NOBLOCKSIGHT;
  trace.CheckBaseRegion = true;
  //trace.LineBlockMask = ML_BLOCKEVERYTHING; // this method is used to trace light, so "block everything" is not valid here
  trace.LineBlockMask = ML_BLOCKSIGHT;
  //if (trace.PlaneNoBlockFlags&SPF_NOBLOCKSIGHT) trace.LineBlockMask |= ML_BLOCKSIGHT;
  //if (trace.PlaneNoBlockFlags&SPF_NOBLOCKSHOOT) trace.LineBlockMask |= ML_BLOCKHITSCAN;

  trace.Start = org;
  trace.End = dest;
  return SightPathTraverse(trace, this, OtherSector);
}
