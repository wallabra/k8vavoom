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
//TODO: serialisation
class EntityGridImpl {
protected:
  struct __attribute__((packed)) CellInfo {
    vuint32 objIdx; // index in objects array (that slot can be `nullptr`); this is `MAX_VUINT32` for free cell slots
    vuint32 nextInfo; // next in info/free arrays (note that 0 means "done", because zero slot is never used)
    vuint32 checkVis; // this is used to reject duplicates
    vuint32 padding;
  };

  struct ObjInfo {
    VObject *obj;
    vint32 x, y; // object position
    vint32 w, h; // object size
    union {
      vuint32 nextFree; // next free slot+1 (so 0 means "end of the list")
      // how many cells referenced to this object?
      // this is used to kick out objects that aren't in a grid anymore
      vuint32 refCount;
    };
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

  static inline vuint32 cell2u32 (int x, int y) { return (((vuint32)(y&0xffff))<<16)|((vuint32)(x&0xffff)); }

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
