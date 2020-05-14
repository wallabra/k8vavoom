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
#include "server/sv_local.h"


//**************************************************************************
//
// blockmap light tracing
//
//**************************************************************************
static intercept_t *intercepts = nullptr;
static unsigned interAllocated = 0;
static unsigned interUsed = 0;


//==========================================================================
//
//  EnsureFreeIntercept
//
//==========================================================================
static inline void EnsureFreeIntercept () {
  if (interAllocated <= interUsed) {
    unsigned oldAlloc = interAllocated;
    interAllocated = ((interUsed+4)|0xfffu)+1;
    intercepts = (intercept_t *)Z_Realloc(intercepts, interAllocated*sizeof(intercept_t));
    if (oldAlloc) GCon->Logf(NAME_Debug, "more interceptions allocated; interUsed=%u; allocated=%u (old=%u)", interUsed, interAllocated, oldAlloc);
  }
}


struct LightTraceInfo {
  TVec Start;
  TVec End;
  TVec Delta;
  TPlane Plane;
  TVec LineStart;
  TVec LineEnd;
  sector_t *StartSector;
  sector_t *EndSector;
};


//==========================================================================
//
//  LightCheckRegions
//
//==========================================================================
static bool LightCheckRegions (const sector_t *sec, const TVec point) {
  for (sec_region_t *reg = sec->eregions->next; reg; reg = reg->next) {
    if (reg->regflags&(sec_region_t::RF_BaseRegion|sec_region_t::RF_OnlyVisual|sec_region_t::RF_NonSolid)) continue;
    // get opening points
    const float fz = reg->efloor.GetPointZClamped(point);
    const float cz = reg->eceiling.GetPointZClamped(point);
    if (fz >= cz) continue; // ignore paper-thin regions
    // if we are inside it, we cannot pass
    if (point.z >= fz && point.z <= cz) return false;
  }
  return true;
}


//==========================================================================
//
//  LightCanPassOpening
//
//  ignore 3d midtex here (for now)
//
//==========================================================================
static bool LightCanPassOpening (const line_t *linedef, const TVec point, const TVec dir) {
  if (linedef->sidenum[1] == -1 || !linedef->backsector) return false; // single sided line

  const sector_t *fsec = linedef->frontsector;
  const sector_t *bsec = linedef->backsector;

  if (!fsec || !bsec) return false;

  // check base region first
  const float ffz = fsec->floor.GetPointZClamped(point);
  const float fcz = fsec->ceiling.GetPointZClamped(point);
  const float bfz = bsec->floor.GetPointZClamped(point);
  const float bcz = bsec->ceiling.GetPointZClamped(point);

  const float pfz = max2(ffz, bfz);
  const float pcz = max2(fcz, bcz);

  // TODO: check for transparent doors here
  if (pfz >= pcz) return false;

  if (point.z <= pfz || point.z >= pcz) return false;

  // fast algo for two sectors without 3d floors
  if (!linedef->frontsector->Has3DFloors() &&
      !linedef->backsector->Has3DFloors())
  {
    // no 3d regions, we're ok
    return true;
  }

  // has 3d floors at least on one side, do full-featured search

  // front sector
  if (!LightCheckRegions(fsec, point)) return false;
  // back sector
  if (!LightCheckRegions(bsec, point)) return false;

  // done
  return true;
}


#define UPDATE_PLANE_HIT(plane_)  do { \
  if (!LightCheckPlanePass((plane_), linestart, lineend, currhit, isSky)) { \
    const float dist = (currhit-linestart).lengthSquared(); \
    if (!wasHit || dist < besthdist) { \
      besthit = currhit; \
      bestIsSky = isSky; \
      besthdist = dist; \
    } \
    wasHit = true; \
  } \
} while (0)


//==========================================================================
//
//  LightCheckPlanePass
//
//  WARNING: `currhit` should not be the same as `lineend`!
//
//  returns `true` if plane wasn't hit
//
//==========================================================================
static bool LightCheckPlanePass (const TSecPlaneRef &plane, const TVec &linestart, const TVec &lineend, TVec &currhit, bool &isSky) {
  const float d1 = plane.DotPointDist(linestart);
  if (d1 < 0.0f) return true; // don't shoot back side

  const float d2 = plane.DotPointDist(lineend);
  if (d2 >= 0.0f) return true; // didn't hit plane

  //if (d2 > 0.0f) return true; // didn't hit plane (was >=)
  //if (fabsf(d2-d1) < 0.0001f) return true; // too close to zero

  //frac = d1/(d1-d2); // [0..1], from start

  currhit = lineend;
  // sky?
  if (plane.splane->pic == skyflatnum) {
    // don't shoot the sky!
    isSky = true;
  } else {
    isSky = false;
    currhit -= (lineend-linestart)*d2/(d2-d1);
  }

  // don't go any farther
  return false;
}
//==========================================================================
//
//  LightCheckPassPlanes
//
//  checks all sector regions, returns `false` if any region plane was hit
//  sets `outXXX` arguments on hit (and only on hit!)
//  if `checkSectorBounds` is false, skip checking sector bounds
//  (and the first sector region)
//
//  any `outXXX` can be `nullptr`
//
//  returns `true` if no hit was detected
//
//==========================================================================
static bool LightCheckPassPlanes (sector_t *sector, TVec linestart, TVec lineend,
                                  TVec *outHitPoint=nullptr, bool *outIsSky=nullptr)
{
  if (!sector) return true;

  TVec besthit = lineend;
  bool bestIsSky = false;
  TVec currhit(0.0f, 0.0f, 0.0f);
  bool wasHit = false;
  #ifdef INFINITY
  float besthdist = INFINITY;
  #else
  float besthdist = 9999999.0f;
  #endif
  bool isSky = false;
  TPlane bestHitPlane;

  // make fake floors and ceilings block view
  TSecPlaneRef bfloor, bceil;
  /*
  sector_t *hs = sector->heightsec;
  if (!hs) hs = sector;
  bfloor.set(&hs->floor, false);
  bceil.set(&hs->ceiling, false);
  // check sector floor
  UPDATE_PLANE_HIT(bfloor);
  // check sector ceiling
  UPDATE_PLANE_HIT(bceil);
  */
  sector_t *hs = sector->heightsec;
  if (hs) {
    bfloor.set(&hs->floor, false);
    bceil.set(&hs->ceiling, false);
    // check sector floor
    if (GTextureManager.IsSightBlocking(hs->floor.pic)) {
      UPDATE_PLANE_HIT(bfloor);
    }
    // check sector ceiling
    if (GTextureManager.IsSightBlocking(hs->ceiling.pic)) {
      UPDATE_PLANE_HIT(bceil);
    }
  }

  bfloor.set(&sector->floor, false);
  bceil.set(&sector->ceiling, false);
  // check sector floor
  UPDATE_PLANE_HIT(bfloor);
  // check sector ceiling
  UPDATE_PLANE_HIT(bceil);

  for (sec_region_t *reg = sector->eregions->next; reg; reg = reg->next) {
    if (reg->regflags&(sec_region_t::RF_BaseRegion|sec_region_t::RF_OnlyVisual|sec_region_t::RF_NonSolid)) continue;
    if ((reg->efloor.splane->flags&sec_region_t::RF_SkipFloorSurf) == 0) {
      if (GTextureManager.IsSightBlocking(reg->efloor.splane->pic)) {
        UPDATE_PLANE_HIT(reg->efloor);
      }
    }
    if ((reg->efloor.splane->flags&sec_region_t::RF_SkipCeilSurf) == 0) {
      if (GTextureManager.IsSightBlocking(reg->eceiling.splane->pic)) {
        UPDATE_PLANE_HIT(reg->eceiling);
      }
    }
  }

  if (!wasHit) return true;

  // hit floor or ceiling
  if (outHitPoint) *outHitPoint = besthit;
  if (outIsSky) *outIsSky = bestIsSky;
  return false;
}


//==========================================================================
//
//  LightCheckPlanes
//
//==========================================================================
static bool LightCheckPlanes (LightTraceInfo &trace, sector_t *sec) {
  //k8: for some reason, real sight checks ignores base sector region
  return LightCheckPassPlanes(sec, trace.LineStart, trace.LineEnd, &trace.LineEnd);
}


//==========================================================================
//
//  LightTraverse
//
//  returns `false` if blocked
//
//==========================================================================
static bool LightTraverse (LightTraceInfo &trace, const intercept_t *in) {
  line_t *line = in->line;
  const int s1 = line->PointOnSide2(trace.Start);
  sector_t *front = (s1 == 0 || s1 == 2 ? line->frontsector : line->backsector);
  TVec hitpoint = trace.Start+in->frac*trace.Delta;
  trace.LineEnd = hitpoint;
  if (!LightCheckPlanes(trace, front)) return false;
  trace.LineStart = trace.LineEnd;

  if (line->flags&ML_TWOSIDED) {
    if (LightCanPassOpening(line, hitpoint, trace.Delta)) return true;
  }

  return false; // stop
}


//==========================================================================
//
//  LightTraverseIntercepts
//
//  returns `true` if the traverser function returns true for all lines
//
//==========================================================================
static bool LightTraverseIntercepts (LightTraceInfo &trace) {
  int count = (int)interUsed;

  if (count > 0) {
    // go through in order
    intercept_t *scan = intercepts;
    for (int i = count; i--; ++scan) {
      if (!LightTraverse(trace, scan)) return false; // don't bother going further
    }
  }

  trace.LineEnd = trace.End;
  return LightCheckPlanes(trace, trace.EndSector);
}


//==========================================================================
//
//  LightCheckLine
//
//  return `true` if line is not crossed or put into intercept list
//  return `false` to stop checking due to blocking
//
//==========================================================================
static bool LightCheckLine (LightTraceInfo &trace, line_t *ld) {
  if (ld->validcount == validcount) return true;

  ld->validcount = validcount;

  // signed distances from the line points to the trace line plane
  float dot1 = trace.Plane.PointDistance(*ld->v1);
  float dot2 = trace.Plane.PointDistance(*ld->v2);

  // do not use multiplication to check: zero speedup, lost accuracy
  //if (dot1*dot2 >= 0) return true; // line isn't crossed
  if (dot1 < 0.0f && dot2 < 0.0f) return true; // didn't reached back side
  // if the line is parallel to the trace plane, ignore it
  if (dot1 >= 0.0f && dot2 >= 0.0f) return true; // didn't reached front side

  // signed distances from the trace points to the line plane
  dot1 = ld->PointDistance(trace.Start);
  dot2 = ld->PointDistance(trace.End);

  // do not use multiplication to check: zero speedup, lost accuracy
  //if (dot1*dot2 >= 0) return true; // line isn't crossed
  if (dot1 < 0.0f && dot2 < 0.0f) return true; // didn't reached back side
  // if the trace is parallel to the line plane, ignore it
  if (dot1 >= 0.0f && dot2 >= 0.0f) return true; // didn't reached front side

  // try to early out the check
  if (!ld->backsector || !(ld->flags&ML_TWOSIDED)) {
    return false; // stop checking
  }

  // store the line for later intersection testing
  // signed distance
  const float den = DotProduct(ld->normal, trace.Delta);
  if (fabsf(den) < 0.00001f) return true; // wtf?!
  const float num = ld->dist-DotProduct(trace.Start, ld->normal);
  const float frac = num/den;

  // find place to put our new record
  // this is usually faster than sorting records, as we are traversing blockmap
  // more-or-less in order
  EnsureFreeIntercept();
  intercept_t *icept;
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
//  LightBlockLinesIterator
//
//==========================================================================
static bool LightBlockLinesIterator (LightTraceInfo &trace, const VLevel *level, int x, int y) {
  int offset = y*level->BlockMapWidth+x;
  polyblock_t *polyLink = level->PolyBlockMap[offset];
  while (polyLink) {
    if (polyLink->polyobj) {
      // only check non-empty links
      if (polyLink->polyobj->validcount != validcount) {
        seg_t **segList = polyLink->polyobj->segs;
        for (int i = 0; i < polyLink->polyobj->numsegs; ++i, ++segList) {
          if (!LightCheckLine(trace, (*segList)->linedef)) return false;
        }
        polyLink->polyobj->validcount = validcount;
      }
    }
    polyLink = polyLink->next;
  }

  offset = *(level->BlockMap+offset);

  for (const vint32 *list = level->BlockMapLump+offset+1; *list != -1; ++list) {
    if (!LightCheckLine(trace, &level->Lines[*list])) return false;
  }

  return true; // everything was checked
}


//==========================================================================
//
//  LightPathTraverse
//
//  Traces a line from x1,y1 to x2,y2, calling the traverser function for
//  each. Returns true if the traverser function returns true for all lines
//
//==========================================================================
static bool LightPathTraverse (LightTraceInfo &trace, VLevel *level) {
  VBlockMapWalker walker;

  interUsed = 0;
  trace.LineStart = trace.Start;
  trace.Delta = trace.End-trace.Start;

  if (fabs(trace.Delta.x) <= 0.0001f && fabs(trace.Delta.y) <= 0.0001f) {
    // vertical trace; check starting sector planes and get out
    trace.Delta.x = trace.Delta.y = 0; // to simplify further checks
    trace.LineEnd = trace.End;
    // point cannot hit anything!
    if (fabsf(trace.Delta.z) <= 0.0001f) {
      trace.Delta.z = 0;
      return false;
    }
    return LightCheckPlanes(trace, trace.StartSector);
  }

  if (walker.start(level, trace.Start.x, trace.Start.y, trace.End.x, trace.End.y)) {
    trace.Plane.SetPointDirXY(trace.Start, trace.Delta);
    //++validcount;
    level->IncrementValidCount();
    int mapx, mapy;
    //int guard = 1000;
    while (walker.next(mapx, mapy)) {
      if (!LightBlockLinesIterator(trace, level, mapx, mapy)) {
        return false; // early out
      }
      //if (--guard == 0) Sys_Error("DDA walker fuckup!");
    }
    // couldn't early out, so go through the sorted list
    return LightTraverseIntercepts(trace);
  }

  // out of map, see nothing
  return false;
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

  // do not use buggy vanilla algo here!

  if (!Sector) Sector = PointInSubsector(org)->sector;

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

  LightTraceInfo trace;

  trace.StartSector = Sector;
  trace.EndSector = OtherSector;
  trace.Start = org;
  trace.End = dest;
  return LightPathTraverse(trace, this);
}
