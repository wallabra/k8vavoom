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


//, left:Number, right:Number, top:Number, bottom:Number) : Boolean
/*
static bool lineRectangleIntersect (int x0, int y0, int x1, int y1) {
  // calculate m and c for the equation for the line (y = mx+c)
  m = (y1-y0)/(x1-x0);
  c = y0-(m*x0);

  if (m > 0) {
    // going up from right to left then the top intersect point is on the left
    top_intersection = (m*l+c);
    bottom_intersection = (m*r+c);
  } else {
    // otherwise it's on the right
    top_intersection = (m*r+c);
    bottom_intersection = (m*l+c);
  }

  // work out the top and bottom extents for the triangle
  if (y0 < y1) {
    toptrianglepoint = y0;
    bottomtrianglepoint = y1;
  } else {
    toptrianglepoint = y1;
    bottomtrianglepoint = y0;
  }

  var topoverlap : Number;
  var botoverlap : Number;

  // and calculate the overlap between those two bounds
  topoverlap = top_intersection>toptrianglepoint ? top_intersection : toptrianglepoint;
  botoverlap = bottom_intersection<bottomtrianglepoint ? bottom_intersection : bottomtrianglepoint;

  // (topoverlap<botoverlap) :
  // if the intersection isn't the right way up then we have no overlap

  // (!((botoverlap<t) || (topoverlap>b)) :
  // If the bottom overlap is higher than the top of the rectangle or the top overlap is
  // lower than the bottom of the rectangle we don't have intersection. So return the negative
  // of that. Much faster than checking each of the points is within the bounds of the rectangle.
  return (topoverlap<botoverlap) && (!((botoverlap<t) || (topoverlap>b)));
}
*/

/*
static bool LSegsIntersectionPoint (int l0x0, int l0y0, int l0x1, int l0y1, int l1x0, int l1y0, int l1x1, int l1y1) {
  // Get A,B of first line - points : ps1 to pe1
  float A1 = l0y1-l0y0;
  float B1 = l0x0-l0x1;
  // Get A,B of second line - points : ps2 to pe2
  float A2 = l1y1-l1y0;
  float B2 = l1x0-l1x1;

  // Get delta and check if the lines are parallel
  float delta = A1*B2 - A2*B1;
  if(delta == 0) return null;

  // Get C of first and second lines
  float C2 = A2*l1x0+B2*l1y0;
  float C1 = A1*l0x0+B1*l0y0;
  //invert delta to make division cheaper
  float invdelta = 1/delta;
  // now return the Vector2 intersection point
  return new Vector2( (B2*C1 - B1*C2)*invdelta, (A1*C2 - A2*C1)*invdelta );
}
*/


/*
Vector2? LSegRec_IntersPoint_v02(Vector2 p1, Vector2 p2, float min_x, float min_y, float max_x, float max_y)
{
    Vector2? intersection;

    if (p2.x < min_x) //If the second point of the segment is at left/bottom-left/top-left of the AABB
    {
        if (p2.y > min_y && p2.y < max_y) { return LSegsIntersectionPoint(p1, p2, new Vector2(min_x, min_y), new Vector2(min_x, max_y)); } //If it is at the left
        else if (p2.y < min_y) //If it is at the bottom-left
        {
            intersection = LSegsIntersectionPoint(p1, p2, new Vector2(min_x, min_y), new Vector2(max_x, min_y));
            if (intersection == null) intersection = LSegsIntersectionPoint(p1, p2, new Vector2(min_x, min_y), new Vector2(min_x, max_y));
            return intersection;
        }
        else //if p2.y > max_y, i.e. if it is at the top-left
        {
            intersection = LSegsIntersectionPoint(p1, p2, new Vector2(min_x, max_y), new Vector2(max_x, max_y));
            if (intersection == null) intersection = LSegsIntersectionPoint(p1, p2, new Vector2(min_x, min_y), new Vector2(min_x, max_y));
            return intersection;
        }
    }

    else if (p2.x > max_x) //If the second point of the segment is at right/bottom-right/top-right of the AABB
    {
        if (p2.y > min_y && p2.y < max_y) { return LSegsIntersectionPoint(p1, p2, new Vector2(max_x, min_y), new Vector2(max_x, max_y)); } //If it is at the right
        else if (p2.y < min_y) //If it is at the bottom-right
        {
            intersection = LSegsIntersectionPoint(p1, p2, new Vector2(min_x, min_y), new Vector2(max_x, min_y));
            if (intersection == null) intersection = LSegsIntersectionPoint(p1, p2, new Vector2(max_x, min_y), new Vector2(max_x, max_y));
            return intersection;
        }
        else //if p2.y > max_y, i.e. if it is at the top-left
        {
            intersection = LSegsIntersectionPoint(p1, p2, new Vector2(min_x, max_y), new Vector2(max_x, max_y));
            if (intersection == null) intersection = LSegsIntersectionPoint(p1, p2, new Vector2(max_x, min_y), new Vector2(max_x, max_y));
            return intersection;
        }
    }

    else //If the second point of the segment is at top/bottom of the AABB
    {
        if (p2.y < min_y) return LSegsIntersectionPoint(p1, p2, new Vector2(min_x, min_y), new Vector2(max_x, min_y)); //If it is at the bottom
        if (p2.y > max_y) return LSegsIntersectionPoint(p1, p2, new Vector2(min_x, max_y), new Vector2(max_x, max_y)); //If it is at the top
    }

    return null;

}
*/

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


/*
final GameEntity checkTilesAtLine (int ax0, int ay0, int ax1, int ay1, optional scope bool delegate (GameEntity dg) dg) {
  // point check?
  if (ax0 == ax1 && ay0 == ay1) {
    foreach (GameEntity ge; inCellPix(ax0, ay0, precise:true, castClass:GameEntity)) {
      if (!dg || dg(ge)) return ge;
    }
    return none;
  }

  // do it faster if we can
  #if 0
  // strict vertical check?
  if (ax0 == ax1 && ay0 <= ay1) return checkTilesInRect(ax0, ay0, 1, ay1-ay0+1, dg!optional);
  // strict horizontal check?
  if (ay0 == ay1 && ax0 <= ax1) return checkTilesInRect(ax0, ay0, ax1-ax0+1, 1, dg!optional);
  #endif

  float x0 = float(ax0-mXOfs)/float(CellSize), y0 = float(ay0-mYOfs)/float(CellSize);
  float x1 = float(ax1-mXOfs)/float(CellSize), y1 = float(ay1-mYOfs)/float(CellSize);

  // fix delegate
  //if (!dg) dg = &cbCollisionAnySolid;

  int gridW = mWidth, grigH = mHeight;

  // get starting and enging tile
  int tileSX = trunci(x0), tileSY = trunci(y0);
  int tileEX = trunci(x1), tileEY = trunci(y1);

  // first hit is always landed
  if (tileSX >= 0 && tileSY >= 0 && tileSX < gridW && tileSY < grigH) {
    foreach (GameEntity ge; inCellPix(tileSX*CellSize, tileSY+CellSize, precise:false, castClass:GameEntity)) {
      if (!Geom.lineAABBIntersects(ax0, ay0, ax1, ay1, ge.x0, ge.y0, ge.width, ge.height)) continue;
      if (!dg || dg(ge)) return ge;
    }
  }

  // if starting and ending tile is the same, we don't need to do anything more
  if (tileSX == tileEX && tileSY == tileEY) return none;

  // calculate ray direction
  TVec dv = (vector(x1, y1)-vector(x0, y0)).normalise2d;

  // length of ray from one x or y-side to next x or y-side
  float deltaDistX = (fabs(dv.x) > 0.0001 ? fabs(1.0/dv.x) : 0.0);
  float deltaDistY = (fabs(dv.y) > 0.0001 ? fabs(1.0/dv.y) : 0.0);

  // calculate step and initial sideDists

  float sideDistX; // length of ray from current position to next x-side
  int stepX; // what direction to step in x (either +1 or -1)
  if (dv.x < 0) {
    stepX = -1;
    sideDistX = (x0-tileSX)*deltaDistX;
  } else {
    stepX = 1;
    sideDistX = (tileSX+1.0-x0)*deltaDistX;
  }

  float sideDistY; // length of ray from current position to next y-side
  int stepY; // what direction to step in y (either +1 or -1)
  if (dv.y < 0) {
    stepY = -1;
    sideDistY = (y0-tileSY)*deltaDistY;
  } else {
    stepY = 1;
    sideDistY = (tileSY+1.0-y0)*deltaDistY;
  }

  // perform DDA
  //int side; // was a NS or a EW wall hit?
  for (;;) {
    // jump to next map square, either in x-direction, or in y-direction
    if (sideDistX < sideDistY) {
      sideDistX += deltaDistX;
      tileSX += stepX;
      //side = 0; // EW
    } else {
      sideDistY += deltaDistY;
      tileSY += stepY;
      //side = 1; // NS
    }
    // check tile
    if (tileSX >= 0 && tileSY >= 0 && tileSX < gridW && tileSY < grigH) {
      foreach (GameEntity ge; inCellPix(tileSX*CellSize, tileSY+CellSize, precise:false, castClass:GameEntity)) {
        if (!Geom.lineAABBIntersects(ax0, ay0, ax1, ay1, ge.x0, ge.y0, ge.width, ge.height)) continue;
        if (!dg || dg(ge)) return ge;
      }
    }
    // did we arrived at the destination?
    if (tileSX == tileEX && tileSY == tileEY) break;
  }

  return none;
}
*/
