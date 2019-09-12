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
struct DDALineWalker {
private:
  enum { STCSize = 64 }; // up 8x8 box won't require allocations

  struct __attribute__((packed)) Point { vint32 x, y; };

private:
  // on each step, we'll put all new coords in a buffer
  Point stCoords[STCSize];
  Point *dynCoords;
  unsigned dynCoordsSize;

  vint32 x0, y0, x1, y1;
  vint32 tileWidth, tileHeight;
  vint32 boxWidth, boxHeight;

  unsigned pointCount, pointCurr;

private:
  float fltx0, flty0;
  float fltx1, flty1;
  float deltaDistX, deltaDistY; // length of ray from one x or y-side to next x or y-side
  float sideDistX, sideDistY; // length of ray from current position to next x-side
  vint32 stepX, stepY; // what direction to step in x/y (either +1 or -1)
  vint32 currX, currY; // for simple walkers
  vint32 currTileX, currTileY;
  vint32 endTileX, endTileY;

private:
  inline void resetPoints () { pointCount = pointCurr = 0; }
  Point *allocPoint ();

  // returns nullptr if we have no more points at this step
  inline const Point *nextPoint () {
    unsigned ptc = pointCurr++;
    if (ptc >= pointCount) return nullptr;
    return (ptc < STCSize ? &stCoords[ptc] : &dynCoords[ptc-STCSize]);
  }

  // coords are in pixels
  void reportBoxPointsAt (vint32 x, vint32 y);

  // returns `false` if we're done
  bool doStep ();

public:
  inline DDALineWalker () : dynCoords(nullptr), dynCoordsSize(0), x0(0), y0(0), x1(0), y1(0), pointCount(0), pointCurr(0), currTileX(0), currTileY(0), endTileX(0), endTileY(0) {}
  inline ~DDALineWalker () { Z_Free(dynCoords); }

  inline vint32 getX0 () const { return x0; }
  inline vint32 getY0 () const { return y0; }
  inline vint32 getX1 () const { return x1; }
  inline vint32 getY1 () const { return y1; }
  inline vint32 getTileWidth () const { return tileWidth; }
  inline vint32 getTileHeight () const { return tileHeight; }
  inline vint32 getBoxWidth () const { return boxWidth; }
  inline vint32 getBoxHeight () const { return boxHeight; }

  // you can walk with a box by setting box size (in this case starting coords are box mins)
  // coordinates are in pixels
  void start (vint32 aTileWidth, vint32 aTileHeight, vint32 ax0, vint32 ay0, vint32 ax1, vint32 ay1, vint32 aBoxWidth=0, vint32 aBoxHeight=0);

  // next tile to check
  // returns `false` if you're arrived to a destination
  // if `false` is returned, tile coordinates are undefined (there's nothing more to check)
  // for box walks, it may report the same tile several times (yet it tries to not do that)
  // it may also report out-of-path tiles sometimes
  bool next (vint32 *tilex, vint32 *tiley);
};


//==========================================================================
//
//  DDALineWalker::allocPoint
//
//==========================================================================
DDALineWalker::Point *DDALineWalker::allocPoint () {
  unsigned ptc = pointCount++;
  if (ptc < STCSize) return &stCoords[ptc];
  ptc -= STCSize;
  // check if we need more dynamic points
  if (ptc == dynCoordsSize) {
    dynCoordsSize += 128;
    dynCoords = (Point *)Z_Realloc(dynCoords, dynCoordsSize*sizeof(dynCoords[0]));
  }
  return &dynCoords[ptc];
}


//==========================================================================
//
//  DDALineWalker::reportBoxPointsAt
//
//  coords are in pixels
//
//==========================================================================
void DDALineWalker::reportBoxPointsAt (vint32 x, vint32 y) {
  vassert(boxWidth);
  vint32 tx0 = x/tileWidth;
  vint32 ty0 = y/tileHeight;
  vint32 tx1 = (x+boxWidth-1+tileWidth-1)/tileWidth;
  vint32 ty1 = (y+boxHeight-1+tileHeight-1)/tileHeight;
  for (vint32 ty = ty0; ty <= ty1; ++ty) {
    for (vint32 tx = tx0; tx <= tx1; ++tx) {
      Point *p = allocPoint();
      p->x = tx;
      p->y = ty;
    }
  }
}


//==========================================================================
//
//  DDALineWalker::start
//
//==========================================================================
void DDALineWalker::start (vint32 aTileWidth, vint32 aTileHeight, vint32 ax0, vint32 ay0, vint32 ax1, vint32 ay1, vint32 aBoxWidth, vint32 aBoxHeight) {
  vassert(aTileWidth > 0);
  vassert(aTileHeight > 0);
  vassert(aBoxWidth >= 0);
  vassert(aBoxHeight >= 0);

  resetPoints();

  if (!aBoxWidth || !aBoxHeight) {
    aBoxWidth = aBoxHeight = 0; // a point
  } else {
    // force box to cover integral number of tiles
    /*
    if (aBoxWidth%aTileWidth) aBoxWidth = (aBoxWidth/aTileWidth)*aTileWidth+1;
    if (aBoxHeight%aTileHeight) aBoxHeight = (aBoxHeight/aTileHeight)*aTileHeight+1;
    aBoxWidth /= aTileWidth;
    aBoxHeight /= aTileHeight;
    vassert(aBoxWidth > 0);
    vassert(aBoxHeight > 0);
    */
  }

  x0 = ax0;
  y0 = ay0;
  x1 = ax1;
  y1 = ay1;
  tileWidth = aTileHeight;
  tileHeight = aTileHeight;
  boxWidth = aBoxWidth;
  boxHeight = aBoxHeight;

  // fill initial set
  vint32 tileSX = ax0/aTileWidth, tileSY = ay0/aTileHeight;
  currTileX = tileSX;
  currTileY = tileSY;
  endTileX = ax1/aTileWidth;
  endTileY = ay1/aTileHeight;

  if (aBoxWidth) {
    // box
    reportBoxPointsAt(ax0, ay0);
  } else {
    // point
    Point *p = allocPoint();
    p->x = tileSX;
    p->y = tileSY;
  }

  currX = ax0;
  currY = ay0;
  if (ax0 == ax1 || ay0 == ay1) return; // point or a straight line, no need to calculate anything here

  // convert coordinates to floating point
  fltx0 = float(ax0)/float(aTileWidth);
  flty0 = float(ay0)/float(aTileHeight);
  fltx1 = float(ax1)/float(aTileWidth);
  flty1 = float(ay1)/float(aTileHeight);

  //vint32 tileSX = (int)fltx0, tileSY = (int)flty0;
  //vint32 tileEX = (int)fltx1, tileEY = (int)flty1;

  // calculate ray direction
  //TVec dv = (vector(fltx1, flty1)-vector(fltx0, flty0)).normalise2d;
  float dvx = fltx1-fltx0;
  float dvy = flty1-flty0;
  float dvinvlen = sqrtf(dvx*dvx+dvy*dvy); // inverted lentgh
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
    sideDistX = (tileSX+1.0f-fltx0)*deltaDistX;
  }

  if (dvy < 0) {
    stepY = -1;
    sideDistY = (flty0-tileSY)*deltaDistY;
  } else {
    stepY = 1;
    sideDistY = (tileSY+1.0f-flty0)*deltaDistY;
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
  // clear points array
  resetPoints();

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
    //int side; // was a NS or a EW wall hit?
    // jump to next map square, either in x-direction, or in y-direction
    if (sideDistX < sideDistY) {
      sideDistX += deltaDistX;
      currTileX += stepX;
      //side = 0; // EW
    } else {
      sideDistY += deltaDistY;
      currTileY += stepY;
      //side = 1; // NS
    }
  }

  // got some points
  if (!boxWidth) {
    // point
    Point *p = allocPoint();
    p->x = currTileX;
    p->y = currTileY;
  } else {
    // box
    reportBoxPointsAt(currTileX*tileWidth, currTileY*tileHeight);
  }

  return true;
}


//==========================================================================
//
//  DDALineWalker::next
//
//==========================================================================
bool DDALineWalker::next (vint32 *tilex, vint32 *tiley) {
  for (;;) {
    const Point *p = nextPoint();
    if (p) {
      if (tilex) *tilex = p->x;
      if (tiley) *tiley = p->y;
      return true;
    }
    if (!doStep()) return false;
  }
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
