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
//**
//**  Movement/collision utility functions, as used by function in
//**  p_map.c. BLOCKMAP Iterator functions, and some PIT_* functions to use
//**  for iteration.
//**
//**************************************************************************
#include "gamedefs.h"

static VCvarB dbg_use_buggy_thing_traverser("dbg_use_buggy_thing_traverser", false, "Use old and buggy thing traverser (for debug)?", 0);


#define FRACBITS  (16)
#define FRACUNIT  (1<<FRACBITS)

typedef int fixed_t;

#define FL(x)  ((float)(x)/(float)FRACUNIT)
#define FX(x)  (fixed_t)((x)*FRACUNIT)

// mapblocks are used to check movement against lines and things
#define MAPBLOCKUNITS  (128)
#define MAPBLOCKSIZE   (MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT  (FRACBITS+7)
#define MAPBTOFRAC     (MAPBLOCKSHIFT-FRACBITS)


//==========================================================================
//
//  VBlockLinesIterator::VBlockLinesIterator
//
//==========================================================================
VBlockLinesIterator::VBlockLinesIterator (VLevel *ALevel, int x, int y, line_t **ALinePtr)
  : Level(ALevel)
  , LinePtr(ALinePtr)
  , PolyLink(nullptr)
  , PolySegIdx(-1)
  , List(nullptr)
{
  if (x < 0 || x >= Level->BlockMapWidth || y < 0 || y >= Level->BlockMapHeight) return; // off the map

  int offset = y*Level->BlockMapWidth+x;
  PolyLink = Level->PolyBlockMap[offset];

  offset = *(Level->BlockMap+offset);
  List = Level->BlockMapLump+offset+1;
}


//==========================================================================
//
//  VBlockLinesIterator::GetNext
//
//==========================================================================
bool VBlockLinesIterator::GetNext () {
  guard(VBlockLinesIterator::GetNext);
  if (!List) return false; // off the map

  // check polyobj blockmap
  while (PolyLink) {
    if (PolySegIdx >= 0) {
      while (PolySegIdx < PolyLink->polyobj->numsegs) {
        seg_t *Seg = PolyLink->polyobj->segs[PolySegIdx];
        ++PolySegIdx;
        if (Seg->linedef->validcount == validcount) continue;
        Seg->linedef->validcount = validcount;
        *LinePtr = Seg->linedef;
        return true;
      }
      PolySegIdx = -1;
    }
    if (PolyLink->polyobj) {
      if (PolyLink->polyobj->validcount != validcount) {
        PolyLink->polyobj->validcount = validcount;
        PolySegIdx = 0;
        continue;
      }
    }
    PolyLink = PolyLink->next;
  }

  while (*List != -1) {
#ifdef PARANOID
    if (*List < 0 || *List >= Level->NumLines) Host_Error("Broken blockmap - line %d", *List);
#endif
    line_t *Line = &Level->Lines[*List];
    ++List;

    if (Line->validcount == validcount) continue; // line has already been checked

    Line->validcount = validcount;
    *LinePtr = Line;
    return true;
  }

  return false;
  unguard;
}


//==========================================================================
//
//  VRadiusThingsIterator::VRadiusThingsIterator
//
//==========================================================================
VRadiusThingsIterator::VRadiusThingsIterator(VThinker *ASelf, VEntity **AEntPtr, TVec Org, float Radius)
  : Self(ASelf)
  , EntPtr(AEntPtr)
{
  xl = MapBlock(Org.x-Radius-Self->XLevel->BlockMapOrgX-MAXRADIUS);
  xh = MapBlock(Org.x+Radius-Self->XLevel->BlockMapOrgX+MAXRADIUS);
  yl = MapBlock(Org.y-Radius-Self->XLevel->BlockMapOrgY-MAXRADIUS);
  yh = MapBlock(Org.y+Radius-Self->XLevel->BlockMapOrgY+MAXRADIUS);
  x = xl;
  y = yl;
  if (x < 0 || x >= Self->XLevel->BlockMapWidth || y < 0 || y >= Self->XLevel->BlockMapHeight) {
    Ent = nullptr;
  } else {
    Ent = Self->XLevel->BlockLinks[y*Self->XLevel->BlockMapWidth+x];
  }
}


//==========================================================================
//
//  VRadiusThingsIterator::GetNext
//
//==========================================================================
bool VRadiusThingsIterator::GetNext () {
  guard(VRadiusThingsIterator::GetNext);
  while (1) {
    while (Ent) {
      *EntPtr = Ent;
      Ent = Ent->BlockMapNext;
      return true;
    }

    ++y;
    if (y > yh) {
      x++;
      y = yl;
      if (x > xh) return false;
    }

    if (x < 0 || x >= Self->XLevel->BlockMapWidth || y < 0 || y >= Self->XLevel->BlockMapHeight) {
      Ent = nullptr;
    } else {
      Ent = Self->XLevel->BlockLinks[y*Self->XLevel->BlockMapWidth+x];
    }
  }
  unguard;
}


// path traverser will never build intercept list recursively, so
// we can use static hashmap to mark things. yay!
static TMapNC<VEntity *, bool> vptSeenThings;


//==========================================================================
//
//  VPathTraverse::VPathTraverse
//
//==========================================================================
VPathTraverse::VPathTraverse (VThinker *Self, intercept_t **AInPtr, float InX1,
                              float InY1, float x2, float y2, int flags)
  : Count(0)
  , In(nullptr)
  , InPtr(AInPtr)
{
  Init(Self, InX1, InY1, x2, y2, flags);
}


//==========================================================================
//
//  VPathTraverse::Init
//
//==========================================================================
void VPathTraverse::Init (VThinker *Self, float InX1, float InY1, float x2, float y2, int flags) {
  guard(VPathTraverse::Init);
  float x1 = InX1;
  float y1 = InY1;
  int xt1;
  int yt1;
  int xt2;
  int yt2;

  float xstep;
  float ystep;

  float partialx;
  float partialy;

  float xintercept;
  float yintercept;

  int mapx;
  int mapy;

  int mapxstep;
  int mapystep;

  ++validcount;

  if (((FX(x1-Self->XLevel->BlockMapOrgX))&(MAPBLOCKSIZE-1)) == 0) x1 += 1.0;  // don't side exactly on a line
  if (((FX(y1-Self->XLevel->BlockMapOrgY))&(MAPBLOCKSIZE-1)) == 0) y1 += 1.0;  // don't side exactly on a line

  // check if `Length()` and `SetPointDirXY()` are happy
  if (x1 == x2 && y1 == y2) { x2 += 0.01; y2 += 0.01; }

  vptSeenThings.reset(); // don't shrink buckets

  trace_org = TVec(x1, y1, 0);
  trace_dest = TVec(x2, y2, 0);
  trace_delta = trace_dest-trace_org;
  trace_dir = Normalise(trace_delta);
  trace_len = Length(trace_delta);

  trace_plane.SetPointDirXY(trace_org, trace_delta);

  x1 -= Self->XLevel->BlockMapOrgX;
  y1 -= Self->XLevel->BlockMapOrgY;
  xt1 = MapBlock(x1);
  yt1 = MapBlock(y1);

  x2 -= Self->XLevel->BlockMapOrgX;
  y2 -= Self->XLevel->BlockMapOrgY;
  xt2 = MapBlock(x2);
  yt2 = MapBlock(y2);

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

  float earlyFrac = 1000;

  //k8: zdoom is using 1000 here; why?
  for (int count = 0 ; count < 100; ++count) {
    if (flags&PT_ADDLINES) {
      if (!AddLineIntercepts(Self, mapx, mapy, !!(flags&PT_EARLYOUT), earlyFrac)) {
        // don't throw away things before the early out line
        if (flags&PT_ADDTHINGS) AddThingIntercepts(Self, mapx, mapy, earlyFrac);
        return; // early out
      }
    }

    //k8: should this be moved to top?
    if (flags&PT_ADDTHINGS) AddThingIntercepts(Self, mapx, mapy);

    if (mapx == xt2 && mapy == yt2) break;

    // [RH] handle corner cases properly instead of pretending they don't exist
    if (int(yintercept) == mapy) {
      yintercept += ystep;
      mapx += mapxstep;
    } else if (int(xintercept) == mapx) {
      xintercept += xstep;
      mapy += mapystep;
    } else if (int(fabs(yintercept)) == mapy && int(fabs(xintercept)) == mapx) {
      // the trace is exiting a block through its corner. not only does the block
      // being entered need to be checked (which will happen when this loop
      // continues), but the other two blocks adjacent to the corner also need to
      // be checked.
      if (flags&PT_ADDLINES) {
        float ef1 = 1000;
        if (!AddLineIntercepts(Self, mapx+mapxstep, mapy, !!(flags&PT_EARLYOUT), earlyFrac) ||
            !AddLineIntercepts(Self, mapx, mapy+mapystep, !!(flags&PT_EARLYOUT), ef1))
        {
          // don't throw away things before the line
          if (flags&PT_ADDTHINGS) {
            const float frac = (earlyFrac < ef1 ? earlyFrac : ef1);
            AddThingIntercepts(Self, mapx+mapxstep, mapy, frac);
            AddThingIntercepts(Self, mapx, mapy+mapystep, frac);
          }
          return; // early out
        }
      }

      if (flags&PT_ADDTHINGS) {
        AddThingIntercepts(Self, mapx+mapxstep, mapy);
        AddThingIntercepts(Self, mapx, mapy+mapystep);
      }

      xintercept += xstep;
      yintercept += ystep;
      mapx += mapxstep;
      mapy += mapystep;
    } else {
      // stop traversing, because somebody screwed up
      //count = 100; //k8: does `break` forbidden by some religious taboo?
      break;
    }
  }

  Count = Intercepts.Num();
  In = Intercepts.Ptr();
  unguard;
}


//==========================================================================
//
//  VPathTraverse::NewIntercept
//
//  insert new intercept
//  this is faster than sorting, as most intercepts are already sorted
//
//==========================================================================
intercept_t &VPathTraverse::NewIntercept (const float frac, bool asThing) {
  int pos = Intercepts.length();
  if (pos == 0 || Intercepts[pos-1].frac < frac) {
    // no need to bubble, just append it
    intercept_t &xit = Intercepts.Alloc();
    xit.frac = frac;
    return xit;
  }
  // bubble
  while (pos > 0 && Intercepts[pos-1].frac > frac) --pos;
  if (asThing) {
    while (pos > 0 && Intercepts[pos-1].frac >= frac && !Intercepts[pos-1].thing) --pos;
  }
  // insert
  intercept_t it;
  it.frac = frac;
  Intercepts.insert(pos, it);
  return Intercepts[pos];
}


//==========================================================================
//
//  VPathTraverse::AddLineIntercepts
//
//  Looks for lines in the given block that intercept the given trace to add
//  to the intercepts list.
//  A line is crossed if its endpoints are on opposite sides of the trace.
//  Returns `false` if earlyout and a solid line hit.
//
//==========================================================================
bool VPathTraverse::AddLineIntercepts (VThinker *Self, int mapx, int mapy, bool EarlyOut, float &earlyFrac) {
  guard(VPathTraverse::AddLineIntercepts);
  line_t *ld;
  earlyFrac = 1000;
  for (VBlockLinesIterator It(Self->XLevel, mapx, mapy, &ld); It.GetNext(); ) {
    float dot1 = DotProduct(*ld->v1, trace_plane.normal)-trace_plane.dist;
    float dot2 = DotProduct(*ld->v2, trace_plane.normal)-trace_plane.dist;

    if (dot1*dot2 >= 0) continue; // line isn't crossed
    // hit the line

    // find the fractional intercept point along the trace line
    const float den = DotProduct(ld->normal, trace_delta);
    if (den == 0) continue;
    const float num = ld->dist-DotProduct(trace_org, ld->normal);
    const float frac = num/den;
    if (frac < 0 || frac > 1.0) continue; // behind source or beyond end point

    // try to early out the check
    if (EarlyOut && frac < 1.0 && !ld->backsector) {
      // stop checking
      earlyFrac = frac;
      return false;
    }

    intercept_t &In = NewIntercept(frac, false);
    In.frac = frac;
    In.Flags = intercept_t::IF_IsALine;
    In.line = ld;
    In.thing = nullptr;
  }
  return true;
  unguard;
}


//==========================================================================
//
//  VPathTraverse::AddThingIntercepts
//
//==========================================================================
void VPathTraverse::AddThingIntercepts (VThinker *Self, int mapx, int mapy, float maxfrac) {
  guard(VPathTraverse::AddThingIntercepts);
  if (dbg_use_buggy_thing_traverser) {
    for (VBlockThingsIterator It(Self->XLevel, mapx, mapy); Self && It; ++It) {
      const float dot = DotProduct(It->Origin, trace_plane.normal)-trace_plane.dist;
      if (dot >= It->Radius || dot <= -It->Radius) continue; // thing is too far away
      const float dist = DotProduct((It->Origin-trace_org), trace_dir); //dist -= sqrt(It->radius * It->radius - dot * dot);
      if (dist < 0) continue; // behind source
      const float frac = dist/trace_len;
      if (frac >= maxfrac) continue;

      intercept_t &In = NewIntercept(frac, true);
      In.frac = frac;
      In.Flags = 0;
      In.line = nullptr;
      In.thing = *It;
    }
  } else {
    // "better"
    for (int dy = -1; dy < 2; ++dy) {
      for (int dx = -1; dx < 2; ++dx) {
        for (VBlockThingsIterator It(Self->XLevel, mapx+dx, mapy+dy); Self && It; ++It) {
          const float dot = DotProduct(It->Origin, trace_plane.normal)-trace_plane.dist;
          if (dot >= It->Radius || dot <= -It->Radius) continue; // thing is too far away
          const float dist = DotProduct((It->Origin-trace_org), trace_dir); //dist -= sqrt(It->radius * It->radius - dot * dot);
          if (dist < 0) continue; // behind source
          const float frac = dist/trace_len;
          if (frac >= maxfrac) continue;

          if (vptSeenThings.has(*It)) continue;
          vptSeenThings.put(*It, true);

          intercept_t &In = NewIntercept(frac, true);
          In.frac = frac;
          In.Flags = 0;
          In.line = nullptr;
          In.thing = *It;
        }
      }
    }
  }
  unguard;
}


//==========================================================================
//
//  VPathTraverse::GetNext
//
//==========================================================================
bool VPathTraverse::GetNext () {
  guard(VPathTraverse::GetNext);
  if (!Count) return false; // everything was traversed
  --Count;

  *InPtr = In++;
  return true;

  /*k8: it is already sorted
  if (In) In->frac = 99999.0; // mark previous intercept as checked

  // go through the sorted list
  float Dist = 99999.0;
  intercept_t *EndIn = Intercepts.Ptr()+Intercepts.Num();
  for (intercept_t *Scan = Intercepts.Ptr(); Scan < EndIn; ++Scan) {
    if (Scan->frac < Dist) {
      Dist = Scan->frac;
      In = Scan;
    }
  }

  if (Dist > 1.0) return false; // checked everything in range

  *InPtr = In;
  return true;
  */
  unguard;
}
