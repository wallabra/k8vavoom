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
#ifndef VCCMOD_ENTGRID_HEADER_FILE
#define VCCMOD_ENTGRID_HEADER_FILE

#include "../vcc_run.h"


// ////////////////////////////////////////////////////////////////////////// //
enum /*Edge*/ {
  EdgeNone,
  EdgeTop,
  EdgeRight,
  EdgeBottom,
  EdgeLeft,
};

//TODO: serialisation
class EntityGridImpl {
protected:
  struct __attribute__((packed)) CellInfo {
    vuint32 objIdx; // index in objects array (that slot can be `nullptr`); this is `MAX_VUINT32` for free cell slots
    vuint32 nextInfo; // next in info/free arrays (note that 0 means "done", because zero slot is never used)
    vuint32 checkVis; // this is used to reject duplicates
    vuint32 padding;
  };

  struct __attribute__((packed)) ObjInfo {
    VObject *obj;
    vint32 x, y; // object position
    vint32 w, h; // object size
    vuint32 checkVis; // this is used to reject duplicates; it is the same as in cellinfo
    union __attribute__((packed)) {
      vuint32 nextFree; // next free slot+1 (so 0 means "end of the list")
      // how many cells referenced to this object?
      // this is used to kick out objects that aren't in a grid anymore
      vuint32 refCount;
    };
    // `rw` and `rh` must be greater than zero
    inline bool isRectIntersects (int rx, int ry, int rw, int rh) const {
      return
        rx < x+w &&
        ry < y+h &&
        rx+rw >= x &&
        ry+rh >= y;
    }
  };

protected:
  CellInfo *cells; // slot 0 is never used
  vuint32 cellsAlloted;
  vuint32 cellsFreeHead; // first free slot or 0

  // this is kept in the separate array to speed up reference cleanups
  ObjInfo *objects;
  vuint32 objectsAlloted;
  vuint32 objectsUsed;
  vuint32 objectsFreeHead; // +1

  TMapNC<VObject *, vuint32> objmap; // key: object; value: index in objects array
  TMapNC<vuint32, vuint32> cellmap; // key: cell coords; value: cell index in cells array

  vint32 mCellWidth;
  vint32 mCellHeight;

  vuint32 visCount; // last used

protected:
  // inits only `checkVis`
  vuint32 allocCell ();
  // cell must be allocated (or be zero)
  // calls `derefObject()`
  void freeCell (vuint32 cellidx);

  vuint32 putObject (VObject *obj); // returns object index
  void derefObject (VObject *obj);
  void derefObject (vuint32 objidx);

  static inline vuint32 cell2u32 (int tx, int ty) { return (((vuint32)(ty&0xffff))<<16)|((vuint32)(tx&0xffff)); }

  inline CellInfo *getCellAt (int x, int y) const {
    auto cpp = cellmap.find(cell2u32(x/mCellWidth, y/mCellHeight));
    return (cpp ? &cells[*cpp] : nullptr);
  }

  inline ObjInfo *getObjAtIdx (vuint32 idx) const {
    return (idx < objectsUsed ? &objects[idx] : nullptr);
  }

  inline vuint32 nextVisCount () {
    if (++visCount != 0) return visCount;
    // clear viscounts
    if (cellsAlloted) {
      CellInfo *ci = cells;
      for (vuint32 count = cellsAlloted; --count; ++ci) ci->checkVis = 0;
    }
    if (objectsAlloted) {
      ObjInfo *oi = objects;
      for (vuint32 count = objectsAlloted; --count; ++oi) oi->checkVis = 0;
    }
    visCount = 1;
    return 1;
  }

public:
  inline EntityGridImpl ()
    : cells(nullptr)
    , cellsAlloted(0)
    , cellsFreeHead(0)
    , objects(nullptr)
    , objectsAlloted(0)
    , objectsUsed(0)
    , objectsFreeHead(0)
    , objmap()
    , cellmap()
    , mCellWidth(16)
    , mCellHeight(16)
    , visCount(0)
  {}

  inline EntityGridImpl (int cellWdt, int cellHgt)
    : cells(nullptr)
    , cellsAlloted(0)
    , cellsFreeHead(0)
    , objects(nullptr)
    , objectsAlloted(0)
    , objectsUsed(0)
    , objectsFreeHead(0)
    , objmap()
    , cellmap()
    , mCellWidth(cellHgt)
    , mCellHeight(cellHgt)
    , visCount(0)
  {
    vensure(cellWdt > 0 && cellWdt <= 8192);
    vensure(cellHgt > 0 && cellHgt <= 8192);
  }

  inline ~EntityGridImpl () { clear(); }

  inline int getCellWidth () const { return mCellWidth; }
  inline int getCellHeight () const { return mCellHeight; }

  inline void setup (int cellWdt, int cellHgt) {
    vensure(cellWdt > 0 && cellWdt <= 8192);
    vensure(cellHgt > 0 && cellHgt <= 8192);
    clear();
    mCellWidth = cellWdt;
    mCellHeight = cellHgt;
  }

  void clear ();
  void reset ();

  void clearRefs ();

  // it is ok to insert an object twice, but DON'T DO IT!
  // `nullptr` obj is not valid, and won't be inserted (obviously)
  void insert (VObject *obj, int x, int y, int w, int h);
  // it is ok to remove `nullptr` or not inserted object
  void remove (VObject *obj);
  bool has (VObject *obj);
  bool find (VObject *obj, int *x, int *y, int *w, int *h);
  // the following functions return `false` if no such object found in the grid
  bool change (VObject *obj, int x, int y, int w, int h); // this CANNOT insert a new object!
  bool move (VObject *obj, int x, int y);
  bool moveBy (VObject *obj, int dx, int dy);
  bool resize (VObject *obj, int w, int h);
  bool resizeBy (VObject *obj, int dw, int dh);

public:
  friend struct IteratorAABB;

  // foreach-style iteration is slower than D-like range
  struct IteratorAABB {
    EntityGridImpl *grid;
    int x, y, w, h;
    int currTileX, currTileY;
    CellInfo *currCell;
    vuint32 visCount;

    inline IteratorAABB () : grid(nullptr), x(0), y(0), w(0), h(0), currTileX(0), currTileY(0), currCell(nullptr) {}
    inline IteratorAABB (EntityGridImpl *agrid, int ax, int ay, int aw, int ah) : grid(agrid), x(ax), y(ay), w(aw), h(ah), currTileX(0), currTileY(0), currCell(nullptr) {
      if (!agrid || aw < 1 || ah < 1) return; // we're done here
      const int tx0 = x/agrid->mCellWidth;
      const int ty0 = y/agrid->mCellHeight;
      const int tx1 = (x+agrid->mCellWidth-1)/agrid->mCellWidth;
      const int ty1 = (y+agrid->mCellHeight-1)/agrid->mCellHeight;
      for (int ty = ty0; ty <= ty1; ++ty) {
        for (int tx = tx0; tx <= tx1; ++tx) {
          CellInfo *ci = grid->getCellAt(tx, ty);
          while (ci) {
            ObjInfo *oi = grid->getObjAtIdx(ci->objIdx);
            if (oi->obj && oi->isRectIntersects(ax, ay, aw, ah)) {
              visCount = agrid->nextVisCount();
              ci->checkVis = visCount;
              oi->checkVis = visCount;
            }
            ci = (ci->nextInfo ? &grid->cells[ci->nextInfo] : nullptr);
          }
          if (ci) {
            currTileX = tx;
            currTileY = ty;
            currCell = ci;
            return;
          }
        }
      }
    }
    inline IteratorAABB (const IteratorAABB &src) : grid(src.grid), x(src.x), y(src.y), w(src.w), h(src.h), currTileX(src.currTileX), currTileY(src.currTileY), currCell(src.currCell) {}
    inline IteratorAABB &operator = (const IteratorAABB &src) {
      if (this != &src) {
        grid = src.grid;
        x = src.x;
        y = src.y;
        w = src.w;
        h = src.h;
        currTileX = src.currTileX;
        currTileY = src.currTileY;
        currCell = src.currCell;
      }
      return *this;
    }

    inline IteratorAABB begin () { return IteratorAABB(*this); }
    inline IteratorAABB end () { return IteratorAABB(); }
    inline bool operator == (const IteratorAABB &b) const { return (currCell == b.currCell); }
    inline bool operator != (const IteratorAABB &b) const { return (currCell == b.currCell); }
    inline IteratorAABB operator * () const { return IteratorAABB(*this); } /* required for iterator */
    inline void operator ++ () { /* this is enough for iterator */
      CellInfo *ci = currCell;
      if (!ci) return;
      const int tx1 = (x+grid->mCellWidth-1)/grid->mCellWidth;
      const int ty1 = (y+grid->mCellHeight-1)/grid->mCellHeight;
      const vuint32 visCount = this->visCount;
      for (;;) {
        // move to next cell info
        if (ci) {
          for (;;) {
            ci = (ci->nextInfo ? &grid->cells[ci->nextInfo] : nullptr);
            if (!ci) break;
            if (ci->checkVis != visCount) {
              ObjInfo *oi = grid->getObjAtIdx(ci->objIdx);
              if (oi->obj && oi->checkVis != visCount && oi->isRectIntersects(x, y, w, h)) {
                ci->checkVis = visCount;
                oi->checkVis = visCount;
                currCell = ci;
                return;
              }
            }
          }
        }
        // we're done with this cell, move to the next one
        if (++currTileX > tx1) {
          if (++currTileY > ty1) {
            // no more cells
            currCell = nullptr;
            return;
          }
          currTileX = x/grid->mCellWidth;
        }
        ci = grid->getCellAt(currTileX, currTileY);
      }
    }

    inline bool isEmpty () const { return !currCell; }
    inline void popFront () { operator++(); }

    inline VObject *getObject () const { return (currCell ? grid->getObjAtIdx(currCell->objIdx)->obj : nullptr); }
    inline void getCoords (int *x, int *y, int *w, int *h) const {
      if (currCell) {
        const ObjInfo *oi = grid->getObjAtIdx(currCell->objIdx);
        if (x) *x = oi->x;
        if (y) *y = oi->y;
        if (w) *w = oi->w;
        if (h) *h = oi->h;
      } else {
        if (x) *x = 0;
        if (y) *y = 0;
        if (w) *w = 0;
        if (h) *h = 0;
      }
    }
    inline int getX () const { return (currCell ? grid->getObjAtIdx(currCell->objIdx)->x : 0); }
    inline int getY () const { return (currCell ? grid->getObjAtIdx(currCell->objIdx)->y : 0); }
    inline int getW () const { return (currCell ? grid->getObjAtIdx(currCell->objIdx)->w : 0); }
    inline int getH () const { return (currCell ? grid->getObjAtIdx(currCell->objIdx)->h : 0); }
  };

  // order is undefined
  IteratorAABB allInAABB (int x, int y, int w, int h);

  // order is from the nearest to the furthest
  // if starts inside some object, returns only that object
  // note that if the starting point is inside of several objects, any of them can be returned
  //IteratorLine allAtPath (int ax0, int ay0, int ax1, int ay1);
};


// ////////////////////////////////////////////////////////////////////////// //
class VEntityGridBase : public VObject {
  DECLARE_CLASS(VEntityGridBase, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VEntityGridBase)

protected:
  EntityGridImpl *impl;

public:
  virtual void Destroy () override;

  virtual void ClearReferences () override;

public:
  DECLARE_FUNCTION(Create)
};


#endif
