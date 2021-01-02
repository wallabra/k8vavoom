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


//**************************************************************************
//
// blockmap tracing
//
//**************************************************************************
static intercept_t *intercepts = nullptr;
static unsigned interAllocated = 0;
static unsigned interUsed = 0;


//==========================================================================
//
//  ResetIntercepts
//
//==========================================================================
static void ResetIntercepts () {
  interUsed = 0;
}


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


// ////////////////////////////////////////////////////////////////////////// //
struct SightTraceInfo {
  // the following should be set
  TVec Start;
  TVec End;
  sector_t *StartSector;
  sector_t *EndSector;
  VLevel *Level;
  unsigned LineBlockMask;
  vuint32 PlaneNoBlockFlags;
  // the following are working vars, and should not be set
  TVec Delta;
  TPlane Plane;
  bool Hit1S; // `true` means "hit one-sided wall"
  TVec LineStart;
  TVec LineEnd;

  inline void setup (VLevel *alevel, const TVec &org, const TVec &dest, sector_t *sstart, sector_t *send) {
    Level = alevel;
    Start = org;
    End = dest;
    // use buggy vanilla algo here, because this is what used for world linking
    StartSector = (sstart ?: alevel->PointInSubsector_Buggy(org)->sector);
    EndSector = (send ?: alevel->PointInSubsector_Buggy(dest)->sector);
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct PlaneHitInfo {
  TVec linestart;
  TVec lineend;
  bool bestIsSky;
  bool wasHit;
  float besthtime;

  inline PlaneHitInfo (const TVec &alinestart, const TVec &alineend) noexcept
    : linestart(alinestart)
    , lineend(alineend)
    , bestIsSky(false)
    , wasHit(false)
    , besthtime(9999.0f)
  {}

  inline TVec getPointAtTime (const float time) const noexcept __attribute__((always_inline)) {
    return linestart+(lineend-linestart)*time;
  }

  inline TVec getHitPoint () const noexcept __attribute__((always_inline)) {
    return linestart+(lineend-linestart)*(wasHit ? besthtime : 0.0f);
  }

  inline void update (const TSecPlaneRef &plane) noexcept {
    const float d1 = plane.PointDistance(linestart);
    if (d1 < 0.0f) return; // don't shoot back side

    const float d2 = plane.PointDistance(lineend);
    if (d2 >= 0.0f) return; // didn't hit plane

    // d1/(d1-d2) -- from start
    // d2/(d2-d1) -- from end

    const float time = d1/(d1-d2);
    if (time < 0.0f || time > 1.0f) return; // hit time is invalid

    if (!wasHit || time < besthtime) {
      bestIsSky = (plane.splane->pic == skyflatnum);
      besthtime = time;
    }

    wasHit = true;
  }

  inline void update (sec_plane_t &plane, bool flip=false) noexcept __attribute__((always_inline)) {
    TSecPlaneRef pp(&plane, flip);
    update(pp);
  }
};


//==========================================================================
//
//  SightCheckPlanes
//
//  returns `true` if no hit was detected
//  sets `trace.LineEnd` if hit was detected
//
//==========================================================================
static bool SightCheckPlanes (SightTraceInfo &trace, sector_t *sector, const bool ignoreSectorBounds) {
  PlaneHitInfo phi(trace.LineStart, trace.LineEnd);

  unsigned flagmask = trace.PlaneNoBlockFlags;
  const bool checkFakeFloors = !(flagmask&SPF_IGNORE_FAKE_FLOORS);
  const bool checkSectorBounds = (!ignoreSectorBounds && !(flagmask&SPF_IGNORE_BASE_REGION));
  flagmask &= SPF_FLAG_MASK;

  if (checkFakeFloors) {
    // make fake floors and ceilings block view
    sector_t *hs = sector->heightsec;
    if (hs) {
      phi.update(hs->floor);
      phi.update(hs->ceiling);
    }
  }

  if (checkSectorBounds) {
    // check base sector planes
    phi.update(sector->floor);
    phi.update(sector->ceiling);
  }

  for (sec_region_t *reg = sector->eregions->next; reg; reg = reg->next) {
    if (reg->regflags&(sec_region_t::RF_BaseRegion|sec_region_t::RF_OnlyVisual|sec_region_t::RF_NonSolid)) continue;
    if ((reg->efloor.splane->flags&flagmask) == 0) {
      phi.update(reg->efloor);
    }
    if ((reg->eceiling.splane->flags&flagmask) == 0) {
      phi.update(reg->eceiling);
    }
  }

  if (phi.wasHit) trace.LineEnd = phi.getHitPoint();
  return !phi.wasHit;
}


//==========================================================================
//
//  SightCheckRegions
//
//==========================================================================
static bool SightCheckRegions (const sector_t *sec, const TVec point, const unsigned flagmask) {
  for (sec_region_t *reg = sec->eregions->next; reg; reg = reg->next) {
    if (reg->regflags&(sec_region_t::RF_BaseRegion|sec_region_t::RF_OnlyVisual|sec_region_t::RF_NonSolid)) continue;
    if (((reg->efloor.splane->flags|reg->eceiling.splane->flags)&flagmask) != 0) continue; // bad flags
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
//  SightCanPassOpening
//
//  ignore 3d midtex here (for now)
//
//==========================================================================
static bool SightCanPassOpening (const line_t *linedef, const TVec point, const unsigned flagmask) {
  if (linedef->sidenum[1] == -1 || !linedef->backsector) return false; // single sided line

  const sector_t *fsec = linedef->frontsector;
  const sector_t *bsec = linedef->backsector;

  if (!fsec || !bsec) return false;

  // check base region first
  const float ffz = fsec->floor.GetPointZClamped(point);
  const float fcz = fsec->ceiling.GetPointZClamped(point);
  const float bfz = bsec->floor.GetPointZClamped(point);
  const float bcz = bsec->ceiling.GetPointZClamped(point);

  const float pfz = max2(ffz, bfz); // highest floor
  const float pcz = min2(fcz, bcz); // lowest ceiling

  // closed sector?
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
  if (!SightCheckRegions(fsec, point, flagmask)) return false;
  // back sector
  if (!SightCheckRegions(bsec, point, flagmask)) return false;

  // done
  return true;
}


//==========================================================================
//
//  SightCheckLineHit
//
//  returns `false` if blocked
//
//==========================================================================
static bool SightCheckLineHit (SightTraceInfo &trace, const line_t *line, const float frac) {
  const int s1 = line->PointOnSide2(trace.Start);
  sector_t *front = (s1 == 0 || s1 == 2 ? line->frontsector : line->backsector);
  TVec hitpoint = trace.Start+frac*trace.Delta;
  trace.LineEnd = hitpoint;
  if (!SightCheckPlanes(trace, front, (front == trace.StartSector))) return false;

  trace.LineStart = trace.LineEnd;
  if (line->flags&ML_TWOSIDED) {
    // crosses a two sided line
    if (SightCanPassOpening(line, hitpoint, trace.PlaneNoBlockFlags&SPF_FLAG_MASK)) return true;
  } else {
    trace.Hit1S = true;
  }

  return false; // stop
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

  // signed distances from the line points to the trace line plane
  const float ldot1 = trace.Plane.PointDistance(*ld->v1);
  const float ldot2 = trace.Plane.PointDistance(*ld->v2);

  // do not use multiplication to check: zero speedup, lost accuracy
  //if (dot1*dot2 >= 0) return true; // line isn't crossed
  if (ldot1 < 0.0f && ldot2 < 0.0f) return true; // didn't reached back side
  // if the line is parallel to the trace plane, ignore it
  if (ldot1 >= 0.0f && ldot2 >= 0.0f) return true; // didn't reached front side

  // signed distances from the trace points to the line plane
  const float dot1 = ld->PointDistance(trace.Start);
  const float dot2 = ld->PointDistance(trace.End);

  // if starting point is on a line, ignore this line
  if (fabsf(dot1) <= 0.1f) return true;

  // do not use multiplication to check: zero speedup, lost accuracy
  //if (dot1*dot2 >= 0) return true; // line isn't crossed
  if (dot1 < 0.0f && dot2 < 0.0f) return true; // didn't reached back side
  // if the trace is parallel to the line plane, ignore it
  if (dot1 >= 0.0f && dot2 >= 0.0f) return true; // didn't reached front side

  // if we hit an "early exit" line, don't bother doing anything more, the sight is blocked
  if (!ld->backsector || !(ld->flags&ML_TWOSIDED) || (ld->flags&trace.LineBlockMask)) {
    // note that we hit 1s line
    trace.Hit1S = true;
    return false;
  }

  // signed distance
  const float den = DotProduct(ld->normal, trace.Delta);
  if (fabsf(den) < 0.00001f) return true; // wtf?!
  const float num = ld->dist-DotProduct(trace.Start, ld->normal);
  const float frac = num/den;

  // store the line for later intersection testing
  intercept_t *icept;

  EnsureFreeIntercept();
  // find place to put our new record
  // this is usually faster than sorting records, as we are traversing blockmap
  // more-or-less in order
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

  return true; // continue scanning
}


//==========================================================================
//
//  SightBlockLinesIterator
//
//==========================================================================
static bool SightBlockLinesIterator (SightTraceInfo &trace, int x, int y) {
  int offset = y*trace.Level->BlockMapWidth+x;
  polyblock_t *polyLink = trace.Level->PolyBlockMap[offset];
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

  offset = *(trace.Level->BlockMap+offset);

  for (const vint32 *list = trace.Level->BlockMapLump+offset+1; *list != -1; ++list) {
    if (!SightCheckLine(trace, &trace.Level->Lines[*list])) return false;
  }

  return true; // everything was checked
}


//==========================================================================
//
//  SightTraverseIntercepts
//
//  Returns true if the traverser function returns true for all lines
//
//==========================================================================
static bool SightTraverseIntercepts (SightTraceInfo &trace) {
  int count = (int)interUsed;

  if (count > 0) {
    // go through in order
    intercept_t *scan = intercepts;
    for (int i = count; i--; ++scan) {
      if (!SightCheckLineHit(trace, scan->line, scan->frac)) return false; // don't bother going further
    }
  }

  trace.LineEnd = trace.End;
  return SightCheckPlanes(trace, trace.EndSector, (trace.EndSector == trace.StartSector));
}


//==========================================================================
//
//  SightPathTraverse
//
//  traces a sight ray from `trace.Start` to `trace.End`, possibly
//  collecting intercepts
//
//  `trace.StartSector` and `trace.EndSector` must be set
//
//  returns `true` if no obstacle was hit
//  sets `trace.LineEnd` if something was hit
//
//==========================================================================
static bool SightPathTraverse (SightTraceInfo &trace) {
  VBlockMapWalker walker;

  ResetIntercepts();

  trace.LineStart = trace.Start;
  trace.Delta = trace.End-trace.Start;
  trace.Hit1S = false;

  if (fabsf(trace.Delta.x) <= 1.0f && fabsf(trace.Delta.y) <= 1.0f) {
    // vertical trace; check starting sector planes and get out
    trace.Delta.x = trace.Delta.y = 0; // to simplify further checks
    trace.LineEnd = trace.End;
    // point cannot hit anything!
    if (fabsf(trace.Delta.z) <= 1.0f) {
      trace.Hit1S = true;
      trace.Delta.z = 0;
      return false;
    }
    return SightCheckPlanes(trace, trace.StartSector, true);
  }

  if (walker.start(trace.Level, trace.Start.x, trace.Start.y, trace.End.x, trace.End.y)) {
    trace.Plane.SetPointDirXY(trace.Start, trace.Delta);
    trace.Level->IncrementValidCount();
    int mapx, mapy;
    while (walker.next(mapx, mapy)) {
      if (!SightBlockLinesIterator(trace, mapx, mapy)) {
        trace.Hit1S = true;
        return false; // early out
      }
    }
    // couldn't early out, so go through the sorted list
    return SightTraverseIntercepts(trace);
  }

  // out of map, see nothing
  return false;
}


//==========================================================================
//
//  SightPathTraverse2
//
//  rechecks intercepts with different ending z value
//
//==========================================================================
static bool SightPathTraverse2 (SightTraceInfo &trace) {
  trace.Delta = trace.End-trace.Start;
  trace.LineStart = trace.Start;
  if (fabsf(trace.Delta.x) <= 1.0f && fabsf(trace.Delta.y) <= 1.0f) {
    // vertical trace; check starting sector planes and get out
    trace.Delta.x = trace.Delta.y = 0; // to simplify further checks
    trace.LineEnd = trace.End;
    // point cannot hit anything!
    if (fabsf(trace.Delta.z) <= 1.0f) {
      trace.Hit1S = true;
      trace.Delta.z = 0;
      return false;
    }
    return SightCheckPlanes(trace, trace.StartSector, true);
  }
  return SightTraverseIntercepts(trace);
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
  if (lengthSquared(org-dest) <= 2.0f) return true;

  // if starting or ending point is out of blockmap bounds, don't bother tracing
  // we can get away with this, because nothing can see anything beyound the map extents
  if (isNotInsideBM(org, this)) return false;
  if (isNotInsideBM(dest, this)) return false;

  if (radius < 0.0f) radius = 0.0f;
  if (height < 0.0f) height = 0.0f;
  if (myheight < 0.0f) myheight = 0.0f;

  SightTraceInfo trace;
  trace.setup(this, org, dest, Sector, DestSector);

  trace.PlaneNoBlockFlags =
    SPF_NOBLOCKSIGHT|
    (ignoreFakeFloors ? SPF_IGNORE_FAKE_FLOORS : 0u)|
    (skipBaseRegion ? SPF_IGNORE_BASE_REGION : 0u);

  trace.LineBlockMask =
    ML_BLOCKSIGHT|
    (ignoreBlockAll ? 0 : ML_BLOCKEVERYTHING)|
    (trace.PlaneNoBlockFlags&SPF_NOBLOCKSHOOT ? ML_BLOCKHITSCAN : 0u);

  const TVec lookOrigin = org+TVec(0, 0, myheight*0.75f); // look from the eyes (roughly)

  if (!allowBetterSight || radius < 4.0f || height < 4.0f || myheight < 4.0f) {
    trace.Start = lookOrigin;
    trace.End = dest;
    trace.End.z += height*0.75f; // roughly at the head
    const bool collectIntercepts = (trace.Delta.length2DSquared() <= 820.0f*820.0f); // arbitrary number
    if (SightPathTraverse(trace)) return true;
    if (trace.Hit1S || !collectIntercepts) return false; // hit one-sided wall, or too far, no need to do other checks
    // another fast check
    trace.End = dest;
    trace.End.z += height*0.5f;
    return SightPathTraverse2(trace);
  } else {
    const float sidemult[3] = { 0.0f, -0.75f, 0.75f }; // side shift multiplier (by radius)
    const float ithmult[2] = { 0.35f, 0.75f }; // destination height multiplier (0.5f is checked first)
    // check side looks
    for (unsigned myx = 0; myx < 3; ++myx) {
      // now look from eyes of t1 to some parts of t2
      trace.Start = lookOrigin+orgdirRight*(radius*sidemult[myx]);
      trace.End = dest;
      //trace.collectIntercepts = true;

      //DUNNO: if our point is not in a starting sector, fix it?

      // check middle
      trace.End.z += height*0.5f;
      if (SightPathTraverse(trace)) return true;
      if (trace.Hit1S || interUsed == 0) continue;

      // check up and down
      for (unsigned itsz = 0; itsz < 2; ++itsz) {
        trace.End = dest;
        trace.End.z += height*ithmult[itsz];
        if (SightPathTraverse2(trace)) return true;
      }
    }
  }

  return false;
}
