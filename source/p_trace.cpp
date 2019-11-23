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

static VCvarB dbg_bsp_trace_strict_flats("dbg_bsp_trace_strict_flats", false, "use strict checks for flats?", /*CVAR_Archive|*/CVAR_PreInit);


//==========================================================================
//
//  CheckPlanes
//
//==========================================================================
static bool CheckPlanes (linetrace_t &trace, sector_t *sec) {
  TVec outHit(0.0f, 0.0f, 0.0f), outNorm(0.0f, 0.0f, 0.0f);

  if (!VLevel::CheckHitPlanes(sec, true/*sector bounds*/, trace.LineStart, trace.LineEnd, (unsigned)trace.PlaneNoBlockFlags, &outHit, &outNorm, nullptr, &trace.HitPlane/*, -0.1f*/)) {
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
  TVec hit_point = trace.Start+frac*trace.Delta;

  trace.LineEnd = hit_point;

  if (front) {
    if (!CheckPlanes(trace, front)) return false;
  }
  trace.LineStart = trace.LineEnd;

  if (line->flags&ML_TWOSIDED) {
    // crosses a two sided line
    opening_t *open = SV_LineOpenings(line, hit_point, trace.PlaneNoBlockFlags);
    if (dbg_bsp_trace_strict_flats) {
      while (open) {
        if (open->bottom < hit_point.z && open->top > hit_point.z) return true;
        open = open->next;
      }
    } else {
      while (open) {
        if (open->bottom <= hit_point.z && open->top >= hit_point.z) return true;
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
#define FRACBITS  (16)
#define FRACUNIT  (1<<FRACBITS)

#define FL(x) ((float)(x)/(float)FRACUNIT)
#define FX(x) ((/*fixed_t*/int)((x)*FRACUNIT))

// mapblocks are used to check movement against lines and things
#define MAPBLOCKUNITS  (128)
#define MAPBLOCKSIZE   (MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT  (FRACBITS+7)
#define MAPBTOFRAC     (MAPBLOCKSHIFT-FRACBITS)


// ////////////////////////////////////////////////////////////////////////// //
#define MAX_ST_INTERCEPTS  (65536)

static intercept_t intercepts[MAX_ST_INTERCEPTS];
static int interUsed;


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
  return VLevel::CheckHitPlanes(sec, trace.CheckBaseRegion, trace.LineStart, trace.LineEnd, (unsigned)trace.PlaneNoBlockFlags, &trace.LineEnd, &trace.HitPlaneNormal, nullptr, nullptr/*, -0.1f*/);
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
  TVec hit_point = trace.Start+in->frac*trace.Delta;
  trace.LineEnd = hit_point;
  if (!SightCheckPlanes(trace, front)) return false;
  trace.LineStart = trace.LineEnd;

  if (line->flags&ML_TWOSIDED) {
    // crosses a two sided line
    opening_t *open = SV_LineOpenings(line, hit_point, trace.PlaneNoBlockFlags);
    while (open) {
      if (open->bottom <= hit_point.z && open->top >= hit_point.z) return true;
      open = open->next;
    }
  }

  // hit line
  trace.HitPlaneNormal = (s1 == 0 || s1 == 2 ? line->normal : -line->normal);

  if (!(line->flags&ML_TWOSIDED)) trace.EarlyOut = true;
  return false; // stop
}


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


//==========================================================================
//
//  SightTraverseIntercepts
//
//  Returns true if the traverser function returns true for all lines
//
//==========================================================================
static bool SightTraverseIntercepts (SightTraceInfo &trace, sector_t *EndSector, bool resort) {
  int count = interUsed;

  if (count > 0) {
    intercept_t *scan;
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
//==========================================================================
static bool SightCheckLine (SightTraceInfo &trace, line_t *ld) {
  if (ld->validcount == validcount) return true;

  ld->validcount = validcount;

  float dot1 = DotProduct(*ld->v1, trace.Plane.normal)-trace.Plane.dist;
  float dot2 = DotProduct(*ld->v2, trace.Plane.normal)-trace.Plane.dist;

  if (dot1*dot2 >= 0) return true; // line isn't crossed

  dot1 = DotProduct(trace.Start, ld->normal)-ld->dist;
  dot2 = DotProduct(trace.End, ld->normal)-ld->dist;

  if (dot1*dot2 >= 0) return true; // line isn't crossed

  // try to early out the check
  if (!ld->backsector || !(ld->flags&ML_TWOSIDED) || (ld->flags&trace.LineBlockMask)) {
    return false; // stop checking
  }

  // store the line for later intersection testing
  if (interUsed < MAX_ST_INTERCEPTS) {
    // distance
    const float den = DotProduct(ld->normal, trace.Delta);
    const float num = ld->dist-DotProduct(trace.Start, ld->normal);
    const float frac = num/den;
    intercept_t *icept;

    // find place to put our new record
    // this is usually faster than sorting records, as we are traversing blockmap
    // more-or-less in order
    if (interUsed > 0) {
      int ipos = interUsed;
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
  }

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
  float x1 = trace.Start.x;
  float y1 = trace.Start.y;
  float x2 = trace.End.x;
  float y2 = trace.End.y;
  float xstep, ystep;
  float partialx, partialy;
  float xintercept, yintercept;
  int mapx, mapy, mapxstep, mapystep;

  //++validcount;
  level->IncrementValidCount();
  //trace.Intercepts.Clear();
  interUsed = 0;

  if (((FX(x1-level->BlockMapOrgX))&(MAPBLOCKSIZE-1)) == 0) x1 += (x1 <= x2 ? 1.0f : -1.0f); // don't side exactly on a line
  if (((FX(y1-level->BlockMapOrgY))&(MAPBLOCKSIZE-1)) == 0) y1 += (y1 <= y2 ? 1.0f : -1.0f); // don't side exactly on a line

  //k8: check if `Length()` and `SetPointDirXY()` are happy
  if (x1 == x2 && y1 == y2) { x2 += 0.02f; y2 += 0.02f; }

  trace.Delta = trace.End-trace.Start;
  trace.Plane.SetPointDirXY(trace.Start, trace.Delta);
  trace.EarlyOut = false;
  trace.LineStart = trace.Start;

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
    trace.EarlyOut = true;
    return false;
  }

  if (xt2 > xt1) {
    mapxstep = 1;
    partialx = 1.0f-FL((FX(x1)>>MAPBTOFRAC)&(FRACUNIT-1));
    ystep = (y2-y1)/fabsf(x2-x1);
  } else if (xt2 < xt1) {
    mapxstep = -1;
    partialx = FL((FX(x1)>>MAPBTOFRAC)&(FRACUNIT-1));
    ystep = (y2-y1)/fabsf(x2-x1);
  } else {
    mapxstep = 0;
    partialx = 1.0f;
    ystep = 256.0f;
  }
  yintercept = FL(FX(y1)>>MAPBTOFRAC)+partialx*ystep;

  if (yt2 > yt1) {
    mapystep = 1;
    partialy = 1.0f-FL((FX(y1)>>MAPBTOFRAC)&(FRACUNIT-1));
    xstep = (x2-x1)/fabsf(y2-y1);
  } else if (yt2 < yt1) {
    mapystep = -1;
    partialy = FL((FX(y1)>>MAPBTOFRAC)&(FRACUNIT-1));
    xstep = (x2-x1)/fabsf(y2-y1);
  } else {
    mapystep = 0;
    partialy = 1.0f;
    xstep = 256.0f;
  }
  xintercept = FL(FX(x1)>>MAPBTOFRAC)+partialy*xstep;

  // [RH] fix for traces that pass only through blockmap corners. in that case,
  // xintercept and yintercept can both be set ahead of mapx and mapy, so the
  // for loop would never advance anywhere.
  if (fabsf(xstep) == 1.0f && fabsf(ystep) == 1.0f) {
    if (ystep < 0.0f) partialx = 1.0f-partialx;
    if (xstep < 0.0f) partialy = 1.0f-partialy;
    if (partialx == partialy) { xintercept = xt1; yintercept = yt1; }
  }

  // step through map blocks
  // count is present to prevent a round off error from skipping the break
  mapx = xt1;
  mapy = yt1;

  //k8: zdoom is using 1000 here; why?
  for (int count = 0; count < /*64*/1000; ++count) {
    if (!SightBlockLinesIterator(trace, level, mapx, mapy)) {
      trace.EarlyOut = true;
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
      if (!SightBlockLinesIterator(trace, level, mapx+mapxstep, mapy) ||
          !SightBlockLinesIterator(trace, level, mapx, mapy+mapystep))
      {
        trace.EarlyOut = true;
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
  return SightTraverseIntercepts(trace, EndSector, false);
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
  return SightTraverseIntercepts(trace, EndSector, true);
}


//==========================================================================
//
//  VLevel::CastCanSee
//
//  doesn't check pvs or reject
//
//==========================================================================
bool VLevel::CastCanSee (sector_t *Sector, const TVec &org, float myheight, const TVec &orgdirFwd, const TVec &orgdirRight,
                         const TVec &dest, float radius, float height, bool skipBaseRegion, sector_t *DestSector,
                         bool alwaysBetter) {
  if (lengthSquared(org-dest) <= 1) return true;

  SightTraceInfo trace;

  if (!Sector) Sector = PointInSubsector(org)->sector;

  if (radius < 0.0f) radius = 0.0f;
  if (height < 0.0f) height = 0.0f;
  if (myheight < 0.0f) myheight = 0.0f;

  // killough 4/19/98: make fake floors and ceilings block view
  if (Sector->heightsec) {
    const sector_t *hs = Sector->heightsec;
    if ((org.z+myheight <= hs->floor.GetPointZClamped(org) && dest.z >= hs->floor.GetPointZClamped(dest)) ||
        (org.z >= hs->ceiling.GetPointZClamped(org) && dest.z+height <= hs->ceiling.GetPointZClamped(dest)))
    {
      return false;
    }
  }

  sector_t *OtherSector = DestSector;
  if (!OtherSector) OtherSector = PointInSubsector(dest)->sector;

  if (OtherSector->heightsec) {
    const sector_t *hs = OtherSector->heightsec;
    if ((dest.z+height <= hs->floor.GetPointZClamped(dest) && org.z >= hs->floor.GetPointZClamped(org)) ||
        (dest.z >= hs->ceiling.GetPointZClamped(dest) && org.z+myheight <= hs->ceiling.GetPointZClamped(org)))
    {
      return false;
    }
  }

  //if (length2DSquared(org-dest) <= 1) return true;

  trace.PlaneNoBlockFlags = SPF_NOBLOCKSIGHT;
  trace.LineBlockMask = ML_BLOCKEVERYTHING|ML_BLOCKSIGHT;
  trace.CheckBaseRegion = !skipBaseRegion;

  if ((radius < 4.0f && height < 4.0f && myheight < 4.0f) || (!alwaysBetter && orgdirRight.isZero())) {
    trace.Start = org;
    trace.Start.z += myheight*0.75f;
    trace.End = dest;
    trace.End.z += height*0.5f;
    return SightPathTraverse(trace, this, OtherSector);
  } else {
    static const float ithmult[2] = { 0.15f, 0.85f };
    static const float sidemult[3] = { 0.0f, -0.75f, 0.75f };
    static const float fwdmult[2] = { 0.75f, 0.0f };
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
//  VLevel::CastEx
//
//  doesn't check pvs or reject
//
//==========================================================================
bool VLevel::CastEx (sector_t *Sector, const TVec &org, const TVec &dest, unsigned blockflags, sector_t *DestSector) {
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

  trace.PlaneNoBlockFlags = blockflags;
  trace.CheckBaseRegion = true;
  trace.LineBlockMask = ML_BLOCKEVERYTHING;
  if (trace.PlaneNoBlockFlags&SPF_NOBLOCKSIGHT) trace.LineBlockMask |= ML_BLOCKSIGHT;
  if (trace.PlaneNoBlockFlags&SPF_NOBLOCKSHOOT) trace.LineBlockMask |= ML_BLOCKHITSCAN;

  trace.Start = org;
  trace.End = dest;
  return SightPathTraverse(trace, this, OtherSector);
}
