/**************************************************************************
 *
 * Coded by Ketmar Dark, 2018
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 **************************************************************************/
#include "mod_entgrid.h"

#include <limits.h>
#include <float.h>
#include <math.h>


// Liang-Barsky
// returns -1 if no collision, -2 if the line is inside the box, otherwise entering side
// enter time defined only for intersection case
static __attribute__((unused)) int lineAABB (int x0, int y0, int x1, int y1,
                                             int boxx0, int boxy0, int boxwdt, int boxhgt,
                                             float *enterTime)
{
  if (boxwdt < 1 || boxhgt < 1) return -1; // degenerate box
  const int dx = x1-x0;
  const int dy = y1-y0;
  const int boxx1 = boxx0+boxwdt-1;
  const int boxy1 = boxy0+boxhgt-1;

  // check for a point
  if ((dx|dy) == 0) {
    // a point, it can be only inside/outside
    return (x0 < boxx0 || y0 < boxy0 || x0 > boxx1 || y0 > boxy1 ? -1/*outside*/ : -2/*inside*/);
  }

  // false check for completely outside (using line bounding box)
  int lminx, lmaxx;
  if (x0 < x1) { lminx = x0; lmaxx = x1; } else { lminx = x1; lmaxx = x0; }
  if (lmaxx < boxx0 || lminx > boxx1) return -1; // completely outsize horizontally
  int lminy, lmaxy;
  if (y0 < y1) { lminy = y0; lmaxy = y1; } else { lminy = y1; lmaxy = y0; }
  if (lmaxy < boxy0 || lminy > boxy1) return -1; // completely outsize vertically

  // check if completely inside
  if (lminx >= boxx0 && lminy >= boxy0 && lmaxx <= boxx1 && lmaxy <= boxy1) return -2; // inside

  // ok, do Liang-Barsky check
  const int p[4] = {-dx, dx, -dy, dy};
  const int q[4] = {x0-boxx0, boxx1-x0, y0-boxy0, boxy1-y0};
  float u1 = /*NEGATIVE_INFINITY*/-FLT_MAX;
  float u2 = /*POSITIVE_INFINITY*/+FLT_MAX;
  unsigned enterSide = 0;
  for (unsigned i = 0; i < 4; ++i) {
    if (p[i] == 0) {
      if (q[i] < 0) return false;
    } else {
      const float t = (float)q[i]/(float)p[i];
           if (p[i] < 0 && u1 < t) { u1 = t; enterSide = i; }
      else if (p[i] > 0 && u2 > t) u2 = t;
    }
  }

  if (u1 > u2 || u1 > 1 || u1 < 0) return -1; // no intersection

  //hitx = x+u1*dx;
  //hity = y+u1*dy;
  if (enterTime) *enterTime = u1;

  return (int)enterSide;
}


// ////////////////////////////////////////////////////////////////////////// //
static inline bool axisOverlap (float &tin, float &tout, int &hitedge,
                                int me0, int me1, int it0, int it1, int d, int he0, int he1)
{
  if (me1 < it0) {
    if (d >= 0) return false; // oops, no hit
    float t = (me1-it0+1)/d;
    if (t > tin) { tin = t; hitedge = he1; }
  } else if (it1 < me0) {
    if (d <= 0) return false; // oops, no hit
    float t = (me0-it1-1)/d;
    if (t > tin) { tin = t; hitedge = he0; }
  }

  if (d < 0 && it1 > me0) {
    float t = (me0-it1-1)/d;
    if (t < tout) tout = t;
  } else if (d > 0 && me1 > it0) {
    float t = (me1-it0+1)/d;
    if (t < tout) tout = t;
  }

  return true;
}


// ////////////////////////////////////////////////////////////////////////// //
// sweep two AABB's to see if and when they are overlapping
// returns `true` if collision was detected (but boxes aren't overlap)
// `u1` and `u1` has no sense if no collision was detected (`hitx` and `hity` either)
// u0 = normalized time of first collision (i.e. collision starts at myMove*u0)
// u1 = normalized time of second collision (i.e. collision stops after myMove*u1)
// hitedge for `it`: it will probably be `None` if no collision was detected, but it is not guaranteed
// enter/exit coords will form non-intersecting configuration (i.e. will be before/after the actual collision)
// but beware of floating point inexactness; `sweepAABB()` will try to (crudely) compensate for it
// while calculating `hitx` and `hity`.
__attribute__((unused))
static bool sweepAABB (int mex0, int mey0, int mew, int meh, // my box
                       int medx, int medy, // my speed
                       int itx0, int ity0, int itw, int ith, // other box
                       /*optional*/ float *u0, // enter time
                       /*optional*/ int *hitedge, // Edge.XXX
                       /*optional*/ float *u1, // exit time
                       /*optional*/ int *hitx, /*optional*/ int *hity) // edge hit coords
{
  if (u0) *u0 = -1.0;
  if (u1) *u1 = -1.0;
  if (hitx) *hitx = mex0;
  if (hity) *hity = mey0;
  if (hitedge) *hitedge = EdgeNone;

  if (mew < 1 || meh < 1 || itw < 1 || ith < 1) return false;

  int mex1 = mex0+mew-1;
  int mey1 = mey0+meh-1;
  int itx1 = itx0+itw-1;
  int ity1 = ity0+ith-1;

  // check if they are overlapping right now (SAT)
  //if (mex1 >= itx0) and (mex0 <= itx1) and (mey1 >= ity0) and (mey0 <= ity1) then begin result := true; exit; end;

  if (medx == 0 && medy == 0) return false; // both boxes are sationary

  // treat b as stationary, so invert v to get relative velocity
  int vx = -medx;
  int vy = -medy;

  float tin = -100000000.0;
  float tout = 100000000.0;
  int hitedgeTmp = EdgeNone;

  if (!axisOverlap(tin, tout, hitedgeTmp, mex0, mex1, itx0, itx1, vx, EdgeRight, EdgeLeft)) return false;
  if (!axisOverlap(tin, tout, hitedgeTmp, mey0, mey1, ity0, ity1, vy, EdgeBottom, EdgeTop)) return false;

  if (u0) *u0 = tin;
  if (u1) *u1 = tout;
  if (hitedge) *hitedge = hitedgeTmp;

  if (tin <= tout && tin >= 0.0 && tin <= 1.0) {
    if (hitx || hity) {
      int ex = mex0+int(roundf(medx*tin)); // poor man's `roundi()`
      int ey = mey0+int(roundf(medy*tin)); // poor man's `roundi()`
      // just in case, compensate for floating point inexactness
      if (ex >= itx0 && ey >= ity0 && ex < itx0+itw && ey < ity0+ith) {
        ex = mex0+int(medx*tin);
        ey = mey0+int(medy*tin);
      }
      if (hitx) *hitx = ex;
      if (hity) *hity = ey;
    }
    return true;
  }

  return false;
}


// ////////////////////////////////////////////////////////////////////////// //
struct DDALineWalker {
private:
  int x0, y0, x1, y1;
  int tileWidth, tileHeight;
  // worker variables
  int currTileX, currTileY;
  int endTileX, endTileY;
  bool reportFirstPoint;
  double fltx0, flty0;
  double fltx1, flty1;
  double deltaDistX, deltaDistY; // length of ray from one x or y-side to next x or y-side
  double sideDistX, sideDistY; // length of ray from current position to next x-side
  int stepX, stepY; // what direction to step in x/y (either +1 or -1)
  int currX, currY; // for simple walkers
  int lastStepVert; // -1: none (first point); 0: horizontal; 1: vertical

private:
  // returns `false` if we're done
  bool doStep ();

public:
  inline DDALineWalker () : x0(0), y0(0), x1(0), y1(0), tileWidth(1), tileHeight(1), currTileX(0), currTileY(0), endTileX(0), endTileY(0), reportFirstPoint(false), lastStepVert(-1) {}

  inline int getX0 () const { return x0; }
  inline int getY0 () const { return y0; }
  inline int getX1 () const { return x1; }
  inline int getY1 () const { return y1; }
  inline int getTileWidth () const { return tileWidth; }
  inline int getTileHeight () const { return tileHeight; }

  // -1: no steps was done yet
  inline int getLastStepVert () const { return lastStepVert; }
  inline int getXSign () const { return stepX; }
  inline int getYSign () const { return stepY; }

  // you can walk with a box by setting box size (in this case starting coords are box mins)
  // coordinates are in pixels
  void start (int aTileWidth, int aTileHeight, int ax0, int ay0, int ax1, int ay1);

  // next tile to check
  // returns `false` if you're arrived to a destination
  // if `false` is returned, tile coordinates are undefined (there's nothing more to check)
  // it may report out-of-path or duplicate tiles sometimes
  bool next (int *tilex, int *tiley);

  // for box walking, we will move starting point to the corresponding box corner
  // (the one at the direction of the tracing), and then we'll step with it.
  // this way, the walker can only check a correspondig row.
  // destination point will be moved too.
};


//==========================================================================
//
//  DDALineWalker::start
//
//==========================================================================
void DDALineWalker::start (int aTileWidth, int aTileHeight, int ax0, int ay0, int ax1, int ay1) {
  vassert(aTileWidth > 0);
  vassert(aTileHeight > 0);

  x0 = ax0;
  y0 = ay0;
  x1 = ax1;
  y1 = ay1;
  tileWidth = aTileHeight;
  tileHeight = aTileHeight;
  reportFirstPoint = true;
  lastStepVert = -1;

  // fill initial set
  int tileSX = ax0/aTileWidth, tileSY = ay0/aTileHeight;
  currTileX = tileSX;
  currTileY = tileSY;
  endTileX = ax1/aTileWidth;
  endTileY = ay1/aTileHeight;

  currX = ax0;
  currY = ay0;
  if (ax0 == ax1 || ay0 == ay1) return; // point or a straight line, no need to calculate anything here
  if (tileSX == endTileX && tileSY == endTileY) return; // nowhere to go anyway

  // convert coordinates to floating point
  fltx0 = double(ax0)/double(aTileWidth);
  flty0 = double(ay0)/double(aTileHeight);
  fltx1 = double(ax1)/double(aTileWidth);
  flty1 = double(ay1)/double(aTileHeight);

  // calculate ray direction
  //TVec dv = (vector(fltx1, flty1)-vector(fltx0, flty0)).normalise2d;
  const double dvx = fltx1-fltx0;
  const double dvy = flty1-flty0;
  const double dvinvlen = sqrtf(dvx*dvx+dvy*dvy); // inverted length
  // length of ray from one x or y-side to next x or y-side
  // this is 1/normalized_component, which is the same as inverted normalised component
  deltaDistX = dvinvlen/dvx;
  deltaDistY = dvinvlen/dvy;

  // calculate step and initial sideDists
  if (dvx < 0) {
    stepX = -1;
    sideDistX = (fltx0-tileSX)*deltaDistX;
  } else {
    stepX = 1;
    sideDistX = (tileSX+1-fltx0)*deltaDistX;
  }

  if (dvy < 0) {
    stepY = -1;
    sideDistY = (flty0-tileSY)*deltaDistY;
  } else {
    stepY = 1;
    sideDistY = (tileSY+1-flty0)*deltaDistY;
  }
}


//==========================================================================
//
//  DDALineWalker::doStep
//
//  returns `false` if we're done
//
//==========================================================================
bool DDALineWalker::doStep () {
  // check if we're done
  if (currTileX == endTileX && currTileY == endTileY) return false;

  if (y0 == y1) {
    // horizontal line
    vassert(x0 != x1);
    currTileX += (x0 < x1 ? 1 : -1);
  } else if (x0 == x1) {
    // vertical line
    vassert(y0 != y1);
    currTileY += (y0 < y1 ? 1 : -1);
  } else {
    // perform DDA
    // jump to next map square, either in x-direction, or in y-direction
    if (sideDistX < sideDistY) {
      sideDistX += deltaDistX;
      currTileX += stepX;
      lastStepVert = 0; // EW, horizontal step
    } else {
      sideDistY += deltaDistY;
      currTileY += stepY;
      lastStepVert = 1; // NS, vertical step
    }
  }

  return true;
}


//==========================================================================
//
//  DDALineWalker::next
//
//==========================================================================
bool DDALineWalker::next (int *tilex, int *tiley) {
  // first point?
  if (!reportFirstPoint) {
    // perform a step
    if (!doStep()) return false;
  } else {
    reportFirstPoint = false;
  }
  if (tilex) *tilex = currTileX;
  if (tiley) *tiley = currTileY;
  return true;
}



//==========================================================================
//
//  EntityGridImpl::allocCell
//
//==========================================================================
vuint32 EntityGridImpl::allocCell () {
  if (!cellsFreeHead) {
    vuint32 newsz = cellsAlloted+0x1000;
    cells = (CellInfo *)Z_Realloc(cells, newsz*sizeof(CellInfo));
    memset(cells+cellsAlloted, 0, (newsz-cellsAlloted)*sizeof(CellInfo));
    for (vuint32 cidx = cellsAlloted; cidx < newsz; ++cidx) {
      cells[cidx].objIdx = MAX_VUINT32;
      cells[cidx].nextInfo = cidx+1;
    }
    if (!cellsAlloted) cells[0].nextInfo = 0;
    cells[newsz-1].nextInfo = 0;
    cellsFreeHead = cellsAlloted;
    cellsAlloted = newsz;
  }
  vuint32 res = cellsFreeHead;
  vassert(res > 0 && res < cellsAlloted);
  vassert(cells[res].objIdx == MAX_VUINT32);
  cellsFreeHead = cells[res].nextInfo;
  cells[res].checkVis = 0;
  return res;
}


//==========================================================================
//
//  EntityGridImpl::freeCell
//
//==========================================================================
void EntityGridImpl::freeCell (vuint32 cellidx) {
  if (!cellidx) return;
  vassert(cellidx > 0 && cellidx < cellsAlloted);
  vassert(cells[cellidx].objIdx != MAX_VUINT32);
  derefObject(cells[cellidx].objIdx);
  cells[cellidx].objIdx = MAX_VUINT32;
  cells[cellidx].nextInfo = cellsFreeHead;
  cellsFreeHead = cellidx;
}


//==========================================================================
//
//  EntityGridImpl::putObject
//
//  returns object index
//
//==========================================================================
vuint32 EntityGridImpl::putObject (VObject *obj) {
  vassert(obj);
  auto idp = objmap.find(obj);
  if (idp) {
    vuint32 res = *idp;
    vassert(res >= 0 && res < objectsAlloted);
    vassert(objects[res].obj == obj);
    ++objects[res].refCount;
    return res;
  } else {
    if (!objectsAlloted || (!objectsFreeHead && objectsUsed == objectsAlloted)) {
      vassert(objectsUsed == 0);
      vassert(objectsFreeHead == 0);
      vuint32 newsz = objectsAlloted+0x1000;
      objects = (ObjInfo *)Z_Realloc(objects, newsz*sizeof(ObjInfo));
      /*
      //memset(objects+objectsAlloted, 0, (newsz-objectsAlloted)*sizeof(ObjInfo));
      for (vuint32 idx = objectsAlloted; idx < newsz; ++idx) {
        objects[idx].obj = nullptr;
        objects[idx].nextFree = idx+2;
      }
      objects[newsz-1].nextFree = 0;
      objectsFreeHead = objectsAlloted+1;
      */
      objectsAlloted = newsz;
    }
    vuint32 res;
    if (objectsFreeHead) {
      res = objectsFreeHead-1;
      vassert(res >= 0 && res < objectsAlloted);
      vassert(objects[res].obj == nullptr);
      objectsFreeHead = objects[res].nextFree;
      if (objectsUsed < res+1) objectsUsed = res+1;
    } else {
      vassert(objectsUsed < objectsAlloted);
      res = objectsUsed++;
      vassert(res >= 0 && res < objectsAlloted);
    }
    objects[res].obj = obj;
    objects[res].refCount = 1;
    objects[res].checkVis = 0;
    return res;
  }
}


//==========================================================================
//
//  EntityGridImpl::derefObject
//
//==========================================================================
void EntityGridImpl::derefObject (VObject *obj) {
  if (!obj) return;
  auto idp = objmap.find(obj);
  if (!idp) return; // wtf?!
  vassert(objects[*idp].obj == obj);
  derefObject(*idp);
}


//==========================================================================
//
//  EntityGridImpl::derefObject
//
//==========================================================================
void EntityGridImpl::derefObject (vuint32 objidx) {
  vassert(objidx >= 0 && objidx < objectsAlloted);
  if (objects[objidx].obj) {
    if (--objects[objidx].refCount == 0) {
      objmap.del(objects[objidx].obj);
      objects[objidx].obj = nullptr;
      objects[objidx].nextFree = objectsFreeHead;
      objectsFreeHead = objidx+1;
    }
  }
}


//==========================================================================
//
//  EntityGridImpl::clear
//
//==========================================================================
void EntityGridImpl::clear () {
  Z_Free(cells);
  cells = nullptr;
  cellsAlloted = cellsFreeHead = 0;

  Z_Free(objects);
  objects = nullptr;
  objectsAlloted = objectsUsed = objectsFreeHead = 0;

  objmap.clear();
  cellmap.clear();

  visCount = 0;
}


//==========================================================================
//
//  EntityGridImpl::reset
//
//==========================================================================
void EntityGridImpl::reset () {
  if (cellsAlloted) {
    for (vuint32 idx = 1; idx < cellsAlloted; ++idx) {
      cells[idx].objIdx = MAX_VUINT32;
      cells[idx].nextInfo = idx+1;
      cells[idx].checkVis = 0;
    }
    cells[cellsAlloted-1].nextInfo = 0;
    cellsFreeHead = 1;
  } else {
    cellsFreeHead = 0;
  }
  objectsUsed = objectsFreeHead = 0;
  visCount = 0;
  objmap.reset();
  cellmap.reset();
}


//==========================================================================
//
//  EntityGridImpl::clearRefs
//
//==========================================================================
void EntityGridImpl::clearRefs () {
  const vuint32 len = objectsUsed;
  if (!len) {
    objectsFreeHead = 0;
    return;
  }
  // this also will rebuild free list, because why not?
  ObjInfo *nfo = objects;
  objectsFreeHead = 0;
  vuint32 prevFree = 0; // +1
  vuint32 lastUsed = 0; // +1
  for (vuint32 idx = 0; idx < len; ++nfo, ++idx) {
    if (nfo->obj && nfo->obj->IsRefToCleanup()) {
      objmap.del(nfo->obj);
      nfo->obj = nullptr;
    }
    if (!nfo->obj) {
      if (!prevFree) objectsFreeHead = idx+1; else objects[prevFree-1].nextFree = idx+1;
      //nfo->nextFree = 0; // will be cleared later
      prevFree = idx+1;
    } else {
      lastUsed = idx+1;
    }
  }
  // no used slots?
  if (!lastUsed) {
    objectsFreeHead = 0;
    objectsUsed = 0;
  } else if (objectsUsed > lastUsed) {
    // shring used count
    objectsUsed = lastUsed;
    // cut free list short
    vassert(prevFree != lastUsed);
    while (prevFree > lastUsed) prevFree = objects[prevFree-1].nextFree;
  }
  if (prevFree) objects[prevFree-1].nextFree = 0; else objectsFreeHead = 0;
}


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_CLASS(V, EntityGridBase);


//==========================================================================
//
//  VEntityGridBase::Destroy
//
//==========================================================================
void VEntityGridBase::Destroy () {
  delete impl; impl = nullptr;
  Super::Destroy();
}


//==========================================================================
//
//  VEntityGridBase::ClearReferences
//
//==========================================================================
void VEntityGridBase::ClearReferences () {
  if (impl) impl->clearRefs();
  Super::ClearReferences();
}


// ////////////////////////////////////////////////////////////////////////// //
//native static final EntityGridBase Create (class objType, int cellWdt, int cellHgt);
IMPLEMENT_FUNCTION(VEntityGridBase, Create) {
  int cellWdt, cellHgt;
  vobjGetParam(cellWdt, cellHgt);
  //if (!Self) { VObject::VMDumpCallStack(); Sys_Error("empty self"); }
  if (cellWdt < 1 || cellWdt > 8192) { VObject::VMDumpCallStack(); Sys_Error("invalid cell width (%d)", cellWdt); }
  if (cellHgt < 1 || cellHgt > 8192) { VObject::VMDumpCallStack(); Sys_Error("invalid cell height (%d)", cellHgt); }
  VEntityGridBase *Self = (VEntityGridBase *)StaticSpawnWithReplace(StaticClass()); // disable replacements?
  vassert(!Self->impl);
  Self->impl = new EntityGridImpl();
  Self->impl->setup(cellWdt, cellHgt);
  RET_REF(Self);
}
