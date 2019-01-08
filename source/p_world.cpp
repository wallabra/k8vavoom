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
//**
//**  Movement/collision utility functions, as used by function in
//**  p_map.c. BLOCKMAP Iterator functions, and some PIT_* functions to use
//**  for iteration.
//**
//**************************************************************************
#include "gamedefs.h"

static VCvarB dbg_use_buggy_thing_traverser("dbg_use_buggy_thing_traverser", false, "Use old and buggy thing traverser (for debug)?", 0);
static VCvarB dbg_use_vavoom_thing_coldet("dbg_use_vavoom_thing_coldet", false, "Use original VaVoom buggy thing coldet (for debug)?", 0);


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


// ////////////////////////////////////////////////////////////////////////// //
#define EQUAL_EPSILON (1.0f/65536.0f)

// used in new thing coldet
struct divline_t {
  float x, y;
  float dx, dy;
};


//==========================================================================
//
//  pointOnDLineSide
//
//  returns 0 (front/on) or 1 (back)
//
//==========================================================================
static inline int pointOnDLineSide (const float x, const float y, const divline_t &line) {
  return ((y-line.y)*line.dx+(line.x-x)*line.dy > EQUAL_EPSILON);
}


//==========================================================================
//
//  interceptVector
//
//  returns the fractional intercept point along the first divline
//
//==========================================================================
static float interceptVector (const divline_t &v2, const divline_t &v1) {
  const float v1x = v1.x;
  const float v1y = v1.y;
  const float v1dx = v1.dx;
  const float v1dy = v1.dy;
  const float v2x = v2.x;
  const float v2y = v2.y;
  const float v2dx = v2.dx;
  const float v2dy = v2.dy;
  const float den = v1dy*v2dx-v1dx*v2dy;
  if (den == 0) return 0; // parallel
  const float num = (v1x-v2x)*v1dy+(v2y-v1y)*v1dx;
  return num/den;
}


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
VRadiusThingsIterator::VRadiusThingsIterator (VThinker *ASelf, VEntity **AEntPtr, TVec Org, float Radius)
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

  //k8: zdoom is using 1000 here; why?
  for (int count = 0 ; count < 100; ++count) {
    if (flags&PT_ADDTHINGS) AddThingIntercepts(Self, mapx, mapy);

    if (flags&PT_ADDLINES) {
      if (!AddLineIntercepts(Self, mapx, mapy, !!(flags&PT_EARLYOUT))) break; // early out
    }

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
      if (flags&PT_ADDTHINGS) {
        AddThingIntercepts(Self, mapx+mapxstep, mapy);
        AddThingIntercepts(Self, mapx, mapy+mapystep);
      }

      if (flags&PT_ADDLINES) {
        if (!AddLineIntercepts(Self, mapx+mapxstep, mapy, !!(flags&PT_EARLYOUT)) ||
            !AddLineIntercepts(Self, mapx, mapy+mapystep, !!(flags&PT_EARLYOUT)))
        {
          break; // early out
        }
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

  // just in case
#ifdef PARANOID
  for (int f = 1; f < Count; ++f) if (In[f].frac < In[f-1].frac) Sys_Error("VPathTraverse: internal sorting error");
#endif

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
intercept_t &VPathTraverse::NewIntercept (const float frac) {
  int pos = Intercepts.length();
  if (pos == 0 || Intercepts[pos-1].frac <= frac) {
    // no need to bubble, just append it
    intercept_t &xit = Intercepts.Alloc();
    xit.frac = frac;
    return xit;
  }
  // bubble
  while (pos > 0 && Intercepts[pos-1].frac > frac) --pos;
  // insert
  intercept_t it;
  it.frac = frac;
  Intercepts.insert(pos, it);
  return Intercepts[pos];
}


//==========================================================================
//
//  VPathTraverse::RemoveInterceptsAfter
//
//==========================================================================
void VPathTraverse::RemoveInterceptsAfter (const float frac) {
  int len = Intercepts.length();
  while (len > 0 && Intercepts[len-1].frac >= frac) --len;
  if (len != Intercepts.length()) Intercepts.setLength(len, false); // don't resize
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
bool VPathTraverse::AddLineIntercepts (VThinker *Self, int mapx, int mapy, bool EarlyOut) {
  guard(VPathTraverse::AddLineIntercepts);
  line_t *ld;
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
    if (frac < 0 || frac > 1.0f) continue; // behind source or beyond end point

    bool doExit = false;
    // try to early out the check
    if (EarlyOut && frac < 1.0f && (!ld->backsector || !(ld->flags&ML_TWOSIDED))) {
      // stop checking
      RemoveInterceptsAfter(frac); // this will remove blocking line, but we need it, hence the flag
      doExit = true;
    }

    intercept_t &In = NewIntercept(frac);
    In.frac = frac;
    In.Flags = intercept_t::IF_IsALine;
    In.line = ld;
    In.thing = nullptr;

    if (doExit) return false; // early out flag
  }
  return true;
  unguard;
}


//==========================================================================
//
//  VPathTraverse::AddThingIntercepts
//
//==========================================================================
void VPathTraverse::AddThingIntercepts (VThinker *Self, int mapx, int mapy) {
  guard(VPathTraverse::AddThingIntercepts);
  if (dbg_use_buggy_thing_traverser) {
    // original
    for (VBlockThingsIterator It(Self->XLevel, mapx, mapy); Self && It; ++It) {
      const float dot = DotProduct(It->Origin, trace_plane.normal)-trace_plane.dist;
      if (dot >= It->Radius || dot <= -It->Radius) continue; // thing is too far away
      const float dist = DotProduct((It->Origin-trace_org), trace_dir); //dist -= sqrt(It->radius * It->radius - dot * dot);
      if (dist < 0) continue; // behind source
      const float frac = dist/trace_len;
      if (frac < 0 || frac > 1.0f) continue;

      intercept_t &In = NewIntercept(frac);
      In.frac = frac;
      In.Flags = 0;
      In.line = nullptr;
      In.thing = *It;
    }
  } else if (dbg_use_vavoom_thing_coldet) {
    // better original
    for (int dy = -1; dy < 2; ++dy) {
      for (int dx = -1; dx < 2; ++dx) {
        for (VBlockThingsIterator It(Self->XLevel, mapx+dx, mapy+dy); Self && It; ++It) {
          const float dot = DotProduct(It->Origin, trace_plane.normal)-trace_plane.dist;
          if (dot >= It->Radius || dot <= -It->Radius) continue; // thing is too far away
          const float dist = DotProduct((It->Origin-trace_org), trace_dir); //dist -= sqrt(It->radius * It->radius - dot * dot);
          if (dist < 0) continue; // behind source
          const float frac = dist/trace_len;
          if (frac < 0 || frac > 1.0f) continue;

          if (vptSeenThings.has(*It)) continue;
          vptSeenThings.put(*It, true);

          intercept_t &In = NewIntercept(frac);
          In.frac = frac;
          In.Flags = 0;
          In.line = nullptr;
          In.thing = *It;
        }
      }
    }
  } else {
    // better, gz
    divline_t trace;
    trace.x = trace_org.x;
    trace.y = trace_org.y;
    trace.dx = trace_delta.x;
    trace.dy = trace_delta.y;
    divline_t line;
    for (int dy = -1; dy < 2; ++dy) {
      for (int dx = -1; dx < 2; ++dx) {
        for (VBlockThingsIterator It(Self->XLevel, mapx+dx, mapy+dy); Self && It; ++It) {
          if (vptSeenThings.has(*It)) continue;
          // [RH] don't check a corner to corner crossection for hit
          // instead, check against the actual bounding box

          // there's probably a smarter way to determine which two sides
          // of the thing face the trace than by trying all four sides...
          int numfronts = 0;
          for (int i = 0; i < 4; ++i) {
            switch (i) {
              case 0: // top edge
                line.y = It->Origin.y+It->Radius;
                if (trace_org.y < line.y) continue;
                line.x = It->Origin.x+It->Radius;
                line.dx = -It->Radius*2;
                line.dy = 0;
                break;
              case 1: // right edge
                line.x = It->Origin.x+It->Radius;
                if (trace_org.x < line.x) continue;
                line.y = It->Origin.y-It->Radius;
                line.dx = 0;
                line.dy = It->Radius*2;
                break;
              case 2: // bottom edge
                line.y = It->Origin.y-It->Radius;
                if (trace_org.y > line.y) continue;
                line.x = It->Origin.x-It->Radius;
                line.dx = It->Radius*2;
                line.dy = 0;
                break;
              case 3: // left edge
                line.x = It->Origin.x-It->Radius;
                if (trace_org.x > line.x) continue;
                line.y = It->Origin.y + It->Radius;
                line.dx = 0;
                line.dy = -It->Radius*2;
                break;
            }
            ++numfronts;

            // check if this side is facing the trace origin
            // if it is, see if the trace crosses it
            if (pointOnDLineSide(line.x, line.y, trace) != pointOnDLineSide(line.x+line.dx, line.y+line.dy, trace)) {
              // it's a hit
              float frac = interceptVector(trace, line);
              if (frac < 0 || frac > 1.0f) continue;

              vptSeenThings.put(*It, true);

              intercept_t &In = NewIntercept(frac);
              In.frac = frac;
              In.Flags = 0;
              In.line = nullptr;
              In.thing = *It;
              break;
            }
          }
          // if none of the sides was facing the trace, then the trace
          // must have started inside the box, so add it as an intercept
          if (numfronts == 0) {
            vptSeenThings.put(*It, true);

            intercept_t &In = NewIntercept(0);
            In.frac = 0;
            In.Flags = 0;
            In.line = nullptr;
            In.thing = *It;
          }
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

  //k8: it is already sorted
  *InPtr = In++;
  return true;

  unguard;
}
