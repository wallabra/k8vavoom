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
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
# include "gamedefs.h"
# include "net/network.h"
#else
# if defined(IN_VCC)
#  include "../../utils/vcc/vcc.h"
# elif defined(VCC_STANDALONE_EXECUTOR)
#  include "../../vccrun/vcc_run.h"
# endif
#endif

#define VC_GARBAGE_COLLECTOR_CHECKS
//#define VC_GARBAGE_COLLECTOR_LOGS_BASE
//#define VC_GARBAGE_COLLECTOR_LOGS_MORE
//#define VC_GARBAGE_COLLECTOR_LOGS_XTRA

// compact slot storage on each GC cycle?
//#define VC_GARBAGE_COLLECTOR_COMPACTING_DEBUG

//static VCvarB gc_use_compacting_collector("gc_use_compacting_collector", true, "Use new compacting GC?", 0);


// ////////////////////////////////////////////////////////////////////////// //
// this does allocations in 8KB chunks
// `T` should be a POD, as it won't be properly copied/destructed
template<class T> class VQueueLifo {
private:
  enum {
    BlockSize = 8192, // 8KB blocks
    ItemsPerBlock = (BlockSize-2*sizeof(void **))/sizeof(T),
    PrevIndex = BlockSize/sizeof(void **)-2,
    NextIndex = BlockSize/sizeof(void **)-1,
  };

private:
  T *first; // first alloted block
  T *currblock; // currently using block
  int blocksAlloted; // number of allocated blocks, for stats
  int used; // total number of elements pushed

private:
  // disable copying
  VQueueLifo (const VQueueLifo &) = delete;
  VQueueLifo &operator = (const VQueueLifo &) = delete;

  inline static T *getPrevBlock (T *blk) { return (blk ? (T *)(((void **)blk)[PrevIndex]) : nullptr); }
  inline static T *getNextBlock (T *blk) { return (blk ? (T *)(((void **)blk)[NextIndex]) : nullptr); }

  inline static void setPrevBlock (T *blk, T* ptr) { if (blk) ((void **)blk)[PrevIndex] = ptr; }
  inline static void setNextBlock (T *blk, T* ptr) { if (blk) ((void **)blk)[NextIndex] = ptr; }

  inline int freeInCurrBlock () const { return (used%ItemsPerBlock ?: ItemsPerBlock); }

public:
  VQueueLifo () : first(nullptr), currblock(nullptr), blocksAlloted(0), used(0) {}
  ~VQueueLifo () { clear(); }

  T operator [] (int idx) {
    check(idx >= 0 && idx < used);
    int bnum = idx/ItemsPerBlock;
    T *blk = first;
    while (bnum--) blk = getNextBlock(blk);
    return blk[idx%ItemsPerBlock];
  }

  inline int length () const { return used; }
  inline int capacity () const { return blocksAlloted*ItemsPerBlock; }

  // free all pool memory
  inline void clear () {
    while (first) {
      T *nb = getNextBlock(first);
      Z_Free(first);
      first = nb;
    }
    first = currblock = nullptr;
    blocksAlloted = 0;
    used = 0;
  }

  // reset pool, but don't deallocate memory
  inline void reset () {
    currblock = first;
    used = 0;
  }

  // allocate new element to fill
  // won't clear it
  inline T *alloc () {
    if (currblock) {
      if (used) {
        int cbpos = freeInCurrBlock();
        if (cbpos < ItemsPerBlock) {
          // can use it
          ++used;
          return (currblock+cbpos);
        }
        // has next allocated block?
        T *nb = getNextBlock(currblock);
        if (nb) {
          currblock = nb;
          ++used;
          check(freeInCurrBlock() == 1);
          return nb;
        }
      } else {
        // no used items, yay
        check(currblock == first);
        ++used;
        return currblock;
      }
    } else {
      check(used == 0);
    }
    // need a new block
    check(getNextBlock(currblock) == nullptr);
    ++blocksAlloted;
    T *nblk = (T *)Z_Malloc(BlockSize);
    setPrevBlock(nblk, currblock);
    setNextBlock(nblk, nullptr);
    setNextBlock(currblock, nblk);
    if (!first) {
      check(used == 0);
      first = nblk;
    }
    currblock = nblk;
    ++used;
    check(freeInCurrBlock() == 1);
    return nblk;
  }

  inline void push (const T &value) { T *cp = alloc(); *cp = value; }
  inline void append (const T &value) { T *cp = alloc(); *cp = value; }

  // forget last element
  inline void pop () {
    check(used);
    if (freeInCurrBlock() == 1) {
      // go to previous block (but don't go beyond first one)
      T *pblk = getPrevBlock(currblock);
      if (pblk) currblock = pblk;
    }
    --used;
  }

  // forget last element
  inline T popValue () {
    check(used);
    T *res = getLast();
    pop();
    return *res;
  }

  // get pointer to last element (or `nullptr`)
  inline T *getLast () { return (used ? currblock+freeInCurrBlock()-1 : nullptr); }
};


// ////////////////////////////////////////////////////////////////////////// //
// register a class at startup time
VClass VObject::PrivateStaticClass (
  EC_NativeConstructor,
  sizeof(VObject),
  VObject::StaticClassFlags,
  nullptr,
  NAME_Object,
  VObject::InternalConstructor
);
VClass *autoclassVObject = VObject::StaticClass();


// ////////////////////////////////////////////////////////////////////////// //
static vuint32 gLastUsedUniqueId = 0;
TArray<VObject *> VObject::GObjObjects;
// this is LIFO queue of free slots in `GObjObjects`
// it is not required in compacting collector
// but Spelunky Remake *may* use "immediate delete" mode, so leave it here
static int gObjFirstFree = 0; // frist free index in `GObjObjects`
int VObject::GNumDeleted = 0;
bool VObject::GInGarbageCollection = false;
static void *GNewObject = nullptr;
#ifdef VCC_STANDALONE_EXECUTOR
bool VObject::GImmediadeDelete = true;
#endif
bool VObject::GGCMessagesAllowed = false;
bool VObject::GCDebugMessagesAllowed = false;
bool (*VObject::onExecuteNetMethodCB) (VObject *obj, VMethod *func) = nullptr; // return `false` to do normal execution

#ifdef VCC_STANDALONE_EXECUTOR
static VQueueLifo<vint32> gDelayDeadObjects;
#endif


VObject::GCStats VObject::gcLastStats;


/*
static void dumpFieldDefs (VClass *cls, const vuint8 *data) {
  if (!cls || !cls->Fields) return;
  GLog.Logf("=== CLASS:%s ===", cls->GetName());
  for (VField *fi = cls->Fields; fi; fi = fi->Next) {
    if (fi->Type.Type == TYPE_Int) {
      GLog.Logf("  %s: %d v=%d", fi->GetName(), fi->Ofs, *(const vint32 *)(data+fi->Ofs));
    } else if (fi->Type.Type == TYPE_Float) {
      GLog.Logf("  %s: %d v=%f", fi->GetName(), fi->Ofs, *(const float *)(data+fi->Ofs));
    } else {
      GLog.Logf("  %s: %d", fi->GetName(), fi->Ofs);
    }
  }
}
*/


//==========================================================================
//
//  VScriptIterator::Finished
//
//==========================================================================
void VScriptIterator::Finished () {
  delete this;
}


//==========================================================================
//
//  VObject::VObject
//
//==========================================================================
VObject::VObject () : Index(-1), UniqueId(0) {
}


//==========================================================================
//
//  VObject::PostCtor
//
//==========================================================================
void VObject::PostCtor () {
  //fprintf(stderr, "VObject::PostCtor(%p): '%s'\n", this, GetClass()->GetName());
}


//==========================================================================
//
//  VObject::~VObject
//
//==========================================================================
VObject::~VObject () {
  check(Index >= 0); // the thing that should be

  ConditionalDestroy();
  GObjObjects[Index] = nullptr;

  if (!GInGarbageCollection) {
    check(GNumDeleted > 0);
    check(ObjectFlags&_OF_Destroyed);
    --GNumDeleted;
    --gcLastStats.markedDead;
    ObjectFlags |= _OF_CleanupRef;
#if defined(VC_GARBAGE_COLLECTOR_LOGS_BASE) && !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
    if (GCDebugMessagesAllowed && developer) GLog.Logf(NAME_Dev, "marked object(%u) #%d: %p (%s)", UniqueId, Index, this, GetClass()->GetName());
#endif
    //fprintf(stderr, "Cleaning up for `%s`\n", *this->GetClass()->Name);
    // no need to delete index from queues, next GC cycle will take care of that
    const int ilen = gObjFirstFree;
    VObject **goptr = GObjObjects.ptr();
    for (int i = 0; i < ilen; ++i, ++goptr) {
      VObject *obj = *goptr;
      if (!obj || (obj->ObjectFlags&_OF_Destroyed)) continue;
      obj->GetClass()->CleanObject(obj);
    }
    ++gcLastStats.lastCollected;
    // cheap check, why not?
    while (gObjFirstFree > 0 && !GObjObjects[gObjFirstFree-1]) --gObjFirstFree;
  }
}


//==========================================================================
//
//  VObject::operator new
//
//==========================================================================
void *VObject::operator new (size_t) {
  //Sys_Error("do not use `new` on `VObject`");
  check(GNewObject);
  return GNewObject;
}


//==========================================================================
//
//  VObject::operator new
//
//==========================================================================
void *VObject::operator new (size_t, const char *, int) {
  //Sys_Error("do not use `new` on `VObject`");
  check(GNewObject);
  return GNewObject;
}


//==========================================================================
//
//  VObject::operator delete
//
//==========================================================================
void VObject::operator delete (void *Object) {
  Z_Free(Object);
}


//==========================================================================
//
//  VObject::operator delete
//
//==========================================================================
void VObject::operator delete (void *Object, const char *, int) {
  Z_Free(Object);
}


//==========================================================================
//
//  VObject::StaticInit
//
//==========================================================================
void VObject::StaticInit () {
  VMemberBase::StaticInit();
  //GObjInitialised = true;
  memset(&gcLastStats, 0, sizeof(gcLastStats)); // easy way
}


//==========================================================================
//
//  VObject::StaticExit
//
//==========================================================================
void VObject::StaticExit () {
  VMemberBase::StaticExit();
}


//==========================================================================
//
//  VObject::StaticSpawnObject
//
//==========================================================================
VObject *VObject::StaticSpawnObject (VClass *AClass, bool skipReplacement) {
  check(AClass);

  VObject *Obj = nullptr;
  try {
    // actually, spawn a replacement
    if (!skipReplacement) {
      VClass *newclass = AClass->GetReplacement();
      if (newclass && newclass->IsChildOf(AClass)) AClass = newclass;
    }

    // allocate memory
    Obj = (VObject *)Z_Calloc(AClass->ClassSize);

    // find native class
    VClass *NativeClass = AClass;
    while (NativeClass != nullptr && !(NativeClass->ObjectFlags&CLASSOF_Native)) {
      NativeClass = NativeClass->GetSuperClass();
    }
    check(NativeClass);

    // call constructor of the native class to set up C++ virtual table
    GNewObject = Obj;
    NativeClass->ClassConstructor();

    // copy values from the default object
    check(AClass->Defaults);
    //GLog.Logf(NAME_Dev, "000: INITIALIZING fields of `%s`...", AClass->GetName());
    AClass->CopyObject(AClass->Defaults, (vuint8 *)Obj);
    //GLog.Logf(NAME_Dev, "001: DONE INITIALIZING fields of `%s`...", AClass->GetName());

    // set up object fields
    Obj->Class = AClass;
    Obj->vtable = AClass->ClassVTable;

    // postinit
    Obj->PostCtor();
  } catch (...) {
    Z_Free(Obj);
    GNewObject = nullptr;
    throw;
  }
  GNewObject = nullptr;
  check(Obj);

  // register in pool
  // this sets `Index` and `UniqueId`
  Obj->Register();

  // we're done
  return Obj;
}


//==========================================================================
//
//  VObject::Register
//
//==========================================================================
void VObject::Register () {
  UniqueId = ++gLastUsedUniqueId;
  if (gLastUsedUniqueId == 0) gLastUsedUniqueId = 1;
  if (gObjFirstFree < GObjObjects.length()) {
    Index = gObjFirstFree;
    GObjObjects[gObjFirstFree++] = this;
    gcLastStats.firstFree = gObjFirstFree;
  } else {
#ifdef VC_GARBAGE_COLLECTOR_CHECKS
    check(gObjFirstFree == GObjObjects.length());
#endif
    Index = GObjObjects.append(this);
    gObjFirstFree = Index+1;
#ifdef VC_GARBAGE_COLLECTOR_CHECKS
    check(gObjFirstFree == GObjObjects.length());
#endif
    gcLastStats.firstFree = gObjFirstFree;
    gcLastStats.poolSize = gObjFirstFree;
    gcLastStats.poolAllocated = GObjObjects.NumAllocated();
  }
#if defined(VC_GARBAGE_COLLECTOR_LOGS_BASE) && !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  if (GCDebugMessagesAllowed && developer) GLog.Logf(NAME_Dev, "created object(%u) #%d: %p (%s)", UniqueId, Index, this, GetClass()->GetName());
#endif
  ++gcLastStats.alive;
}


//==========================================================================
//
//  VObject::SetFlags
//
//==========================================================================
void VObject::SetFlags (vuint32 NewFlags) {
  if ((NewFlags&_OF_Destroyed) && !(ObjectFlags&_OF_Destroyed)) {
    // new dead object
    // no need to remove from delayed deletion list, GC cycle will take care of that
    // set "cleanup ref" flag here, 'cause why not?
    NewFlags |= _OF_CleanupRef;
    ++GNumDeleted;
    ++gcLastStats.markedDead;
#if defined(VC_GARBAGE_COLLECTOR_LOGS_BASE) && !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
    if (GCDebugMessagesAllowed && developer) GLog.Logf(NAME_Dev, "marked object(%u) #%d: %p (%s)", UniqueId, Index, this, GetClass()->GetName());
#endif
  }
#ifdef VCC_STANDALONE_EXECUTOR
  else if ((NewFlags&_OF_DelayedDestroy) && !(ObjectFlags&_OF_DelayedDestroy)) {
    // "delayed destroy" flag is set, put it into delayed list
    gDelayDeadObjects.push(Index);
    ++GNumDeleted;
  }
#endif
  ObjectFlags |= NewFlags;
}


//==========================================================================
//
//  VObject::ConditionalDestroy
//
//==========================================================================
void VObject::ConditionalDestroy () {
  if (!(ObjectFlags&_OF_Destroyed)) {
    SetFlags(_OF_Destroyed);
    Destroy();
  }
}


//==========================================================================
//
//  VObject::Destroy
//
//==========================================================================
void VObject::Destroy () {
  Class->DestructObject(this);
  if (!(ObjectFlags&_OF_Destroyed)) SetFlags(_OF_Destroyed);
#if defined(VC_GARBAGE_COLLECTOR_LOGS_BASE) && !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  if (GCDebugMessagesAllowed && developer) GLog.Logf(NAME_Dev, "destroyed object(%u) #%d: %p (%s)", UniqueId, Index, this, GetClass()->GetName());
#endif
}


//==========================================================================
//
//  VObject::ClearReferences
//
//==========================================================================
void VObject::ClearReferences () {
  GetClass()->CleanObject(this);
}


//==========================================================================
//
//  VObject::CollectGarbage
//
//==========================================================================
void VObject::CollectGarbage (
#ifdef VCC_STANDALONE_EXECUTOR
bool destroyDelayed
#endif
) {
  if (GInGarbageCollection) return; // recursive calls are not allowed
  //check(GObjInitialised);

#ifdef VCC_STANDALONE_EXECUTOR
  if (!GNumDeleted && !destroyDelayed) return;
  check(GNumDeleted >= 0);
#else
  if (!GNumDeleted) return;
  check(GNumDeleted > 0);
#endif

  GInGarbageCollection = true;

#if defined(VC_GARBAGE_COLLECTOR_LOGS_BASE) && !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  if (GCDebugMessagesAllowed && developer) GLog.Logf(NAME_Dev, "collecting garbage...");
#endif

#ifdef VCC_STANDALONE_EXECUTOR
  // destroy all delayed-destroy objects
  if (destroyDelayed) {
    while (gDelayDeadObjects.length()) {
      int idx = gDelayDeadObjects.popValue();
      check(idx >= 0 && idx < gObjFirstFree);
      VObject *obj = GObjObjects[idx];
      if (!obj) continue;
      if ((obj->ObjectFlags&(_OF_Destroyed|_OF_DelayedDestroy)) == _OF_DelayedDestroy) {
        obj->ConditionalDestroy();
      }
    }
  }
#endif

  // no need to mark objects to be cleaned, `_OF_CleanupRef` was set in `SetFlag()`
  int alive = 0, bodycount = 0;
  double lasttime = -Sys_Time();

  const int ilen = gObjFirstFree;
  VObject **goptr = GObjObjects.ptr();

  // we can do a trick here: instead of simply walking the array, we can do
  // both object destroying, and list compaction.
  // let's put a finger on a last used element in the array.
  // on each iteration, check if we hit a dead object, and if we did, delete it.
  // now check if the current slot is empty. if it is not, continue iteration.
  // yet if the current slot is not empty, take object from the last used slot, and
  // move it to the current slot; then check the current slot again. also,
  // move finger to the previous non-empty slot.
  // invariant: when iteration position and finger met, it means that we hit
  // the last slot, so we can stop after processing it.
  //
  // we can do this trick 'cause internal object indicies doesn't matter, and
  // object ordering doesn't matter too. if we will call GC on each frame, it
  // means that our "available slot list" is always empty, and we have no "holes"
  // in slot list.
  //
  // this is slightly complicated by the fact that we cannot really destroy objects
  // on our way (as `ClearReferences()` will check their "dead" flag). so instead of
  // destroying dead objects right away, we'll use finger to point to only alive
  // objects, and swap dead and alive objects. this way all dead objects will
  // be moved to the end of the slot area (with possible gap between dead and alive
  // objects, but it doesn't matter).

  // there is no reason to use old non-compacting collector, so i removed it
  //if (gc_use_compacting_collector)
  {
    // move finger to first alive object
    int finger = /*gObjFirstFree*/ilen-1;
    while (finger >= 0) {
      VObject *fgobj = goptr[finger];
      if (fgobj && (fgobj->ObjectFlags&_OF_Destroyed) == 0) break;
      --finger;
    }

    // sanity check
    check(finger < 0 || (goptr[finger] && (goptr[finger]->ObjectFlags&_OF_Destroyed) == 0));

#ifdef VC_GARBAGE_COLLECTOR_COMPACTING_DEBUG
    fprintf(stderr, "ilen=%d; finger=%d\n", ilen, finger);
#endif

    // now finger points to the last alive object; start iteration
    int itpos = 0;
    while (itpos <= finger) {
      VObject *obj = goptr[itpos];
      if (!obj) {
        // free slot, move alive object here
#ifdef VC_GARBAGE_COLLECTOR_COMPACTING_DEBUG
        const int swp = finger;
#endif
        obj = (goptr[itpos] = goptr[finger]);
        obj->Index = itpos;
        goptr[finger] = nullptr; // it is moved
#ifdef VC_GARBAGE_COLLECTOR_CHECKS
        check(obj && (obj->ObjectFlags&_OF_Destroyed) == 0);
#endif
        // move finger
        --finger; // anyway
        while (finger > itpos) {
          VObject *fgobj = goptr[finger];
          if (fgobj && (fgobj->ObjectFlags&_OF_Destroyed) == 0) break;
          --finger;
        }
#ifdef VC_GARBAGE_COLLECTOR_COMPACTING_DEBUG
        fprintf(stderr, "  hit free slot %d; moved object %d; new finger is %d\n", itpos, swp, finger);
#endif
      } else if ((obj->ObjectFlags&_OF_Destroyed) != 0) {
        // dead object, swap with alive object
#ifdef VC_GARBAGE_COLLECTOR_COMPACTING_DEBUG
        const int swp = finger;
#endif
        VObject *liveobj = goptr[finger];
#ifdef VC_GARBAGE_COLLECTOR_CHECKS
        check(obj->Index == itpos);
        check(liveobj && liveobj->Index == finger && (liveobj->ObjectFlags&_OF_Destroyed) == 0);
#endif
        obj->Index = finger;
        liveobj->Index = itpos;
        goptr[finger] = obj;
        obj = (goptr[itpos] = liveobj);
        check(obj && (obj->ObjectFlags&_OF_Destroyed) == 0);
        // move finger
        --finger; // anyway
        while (finger > itpos) {
          VObject *fgobj = goptr[finger];
          if (fgobj && (fgobj->ObjectFlags&_OF_Destroyed) == 0) break;
          --finger;
        }
#ifdef VC_GARBAGE_COLLECTOR_COMPACTING_DEBUG
        fprintf(stderr, "  swapped slots %d and %d; new finger is %d\n", itpos, swp, finger);
#endif
      }
#ifdef VC_GARBAGE_COLLECTOR_CHECKS
      check(obj && (obj->ObjectFlags&_OF_Destroyed) == 0 && obj->Index == itpos);
#endif
      // we have alive object, clear references
      obj->ClearReferences();
      ++itpos; // move to next object
    }

#ifdef VC_GARBAGE_COLLECTOR_COMPACTING_DEBUG
    fprintf(stderr, " done; itpos=%d; ilen=%d\n", itpos, ilen);
#endif
    // update alive count: it is equal to itpos, as we compacted slot storage
    alive = itpos;
    // update last free position; we cached it, so it is safe
    gObjFirstFree = itpos;

    // use itpos to delete dead objects
    while (itpos < ilen) {
      VObject *obj = goptr[itpos];
      if (obj) {
#ifdef VC_GARBAGE_COLLECTOR_COMPACTING_DEBUG
        fprintf(stderr, "  killing object #%d of %d (dead=%d) (%s)\n", itpos, ilen-1, (obj->ObjectFlags&_OF_Destroyed ? 1 : 0), *obj->GetClass()->Name);
#endif
#ifdef VC_GARBAGE_COLLECTOR_CHECKS
        check(obj->Index == itpos && (obj->ObjectFlags&_OF_Destroyed) != 0);
#endif
        ++bodycount;
        delete obj;
#ifdef VC_GARBAGE_COLLECTOR_CHECKS
        check(goptr[itpos] == nullptr); // sanity check
#endif
      }
      ++itpos;
    }
  } // gc complete

  gcLastStats.lastCollectTime = Sys_Time();
  lasttime += gcLastStats.lastCollectTime;

  GNumDeleted = 0;

  // shring object pool, why not?
  if (GObjObjects.length() > 8192 && gObjFirstFree+8192 < GObjObjects.length()/2) {
    GObjObjects.setLength(gObjFirstFree+8192);
  }

  // update GC stats
  gcLastStats.alive = alive;
  if (bodycount) {
    gcLastStats.lastCollected = bodycount;
    gcLastStats.lastCollectDuration = lasttime;
  }
  gcLastStats.poolSize = GObjObjects.length();
  gcLastStats.poolAllocated = GObjObjects.NumAllocated();
  gcLastStats.firstFree = gObjFirstFree;

#if defined(VC_GARBAGE_COLLECTOR_LOGS_BASE) && !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  if (GCDebugMessagesAllowed && developer) GLog.Logf(NAME_Dev, "garbage collection complete in %d msecs; %d objects deleted, %d objects live, %d of %d array slots used; firstfree=%d",
    (int)(gcLastStats.lastCollectDuration*1000), gcLastStats.lastCollected, gcLastStats.alive, gcLastStats.poolSize, gcLastStats.poolAllocated, gObjFirstFree);
#endif

#if !defined(IN_VCC)
  if (GGCMessagesAllowed && bodycount) {
    const char *msg = va("GC: %d objects deleted, %d objects left; array:[%d/%d]; firstfree=%d", bodycount, alive, GObjObjects.length(), GObjObjects.NumAllocated(), gObjFirstFree);
    GLog.Log(msg);
  }
#endif

  GInGarbageCollection = false;
}


//==========================================================================
//
//  VObject::GetIndexObject
//
//==========================================================================
VObject *VObject::GetIndexObject (int Index) {
  check(Index >= 0 && Index < GObjObjects.length());
  return GObjObjects[Index];
}


//==========================================================================
//
//  VObject::GetObjectsCount
//
//==========================================================================
int VObject::GetObjectsCount () {
  return GObjObjects.Num();
}


//==========================================================================
//
//  VObject::SerialiseFields
//
//  this serialises object fields
//
//==========================================================================
void VObject::SerialiseFields (VStream &Strm) {
  //GetClass()->SerialiseObject(Strm, this);
  if (Strm.IsLoading()) {
    // reading
    // read field count
    vint32 fldcount = -1;
    Strm << STRM_INDEX(fldcount);
    if (fldcount < 0) Host_Error("invalid number of saved fields in class `%s` (%d)", GetClass()->GetName(), fldcount);
    if (fldcount == 0) return; // nothing to do
    // build field list to speedup loading
    TMapNC<VName, VField *> fldmap;
    TMapNC<VName, bool> fldseen;
    for (VClass *cls = GetClass(); cls; cls = cls->GetSuperClass()) {
      for (VField *fld = cls->Fields; fld; fld = fld->Next) {
        if (fld->Flags&(FIELD_Native|FIELD_Transient)) continue;
        if (fld->Name == NAME_None) continue;
        if (fldmap.put(fld->Name, fld)) Host_Error("duplicate field `%s` in class `%s`", *fld->Name, GetClass()->GetName());
      }
    }
    // now load fields
    while (fldcount--) {
      VName fldname = NAME_None;
      Strm << fldname;
      auto fpp = fldmap.find(fldname);
      if (!fpp) {
        GLog.WriteLine(NAME_Warning, "saved field `%s` not found in class `%s`, value ignored", *fldname, GetClass()->GetName());
        VField::SkipSerialisedValue(Strm);
      } else {
        if (fldseen.put(fldname, true)) {
          GLog.WriteLine(NAME_Warning, "duplicate saved field `%s` in class `%s`", *fldname, GetClass()->GetName());
        }
        VField *fld = *fpp;
        VField::SerialiseFieldValue(Strm, (vuint8 *)this+fld->Ofs, fld->Type);
      }
    }
    // show missing fields
    for (auto fit = fldmap.first(); fit; ++fit) {
      VName fldname = fit.getKey();
      if (!fldseen.has(fldname)) {
        GLog.WriteLine(NAME_Warning, "field `%s` is missing in saved data for class `%s`", *fldname, GetClass()->GetName());
      }
    }
  } else {
    // writing
    // count fields, collect them into array
    // serialise fields
    TMapNC<VName, bool> fldseen;
    TArray<VField *> fldlist;
    for (VClass *cls = GetClass(); cls; cls = cls->GetSuperClass()) {
      for (VField *fld = cls->Fields; fld; fld = fld->Next) {
        if (fld->Flags&(FIELD_Native|FIELD_Transient)) continue;
        if (fld->Name == NAME_None) continue;
        if (fldseen.put(fld->Name, true)) Host_Error("duplicate field `%s` in class `%s`", *fld->Name, GetClass()->GetName());
        fldlist.append(fld);
      }
    }
    // now write all fields in backwards order, so they'll appear in natural order in stream
    vint32 fldcount = fldlist.length();
    Strm << STRM_INDEX(fldcount);
    for (int f = fldlist.length()-1; f >= 0; --f) {
      VField *fld = fldlist[f];
      Strm << fld->Name;
      VField::SerialiseFieldValue(Strm, (vuint8 *)this+fld->Ofs, fld->Type);
    }
  }
}


//==========================================================================
//
//  VObject::SerialiseOther
//
//  this serialises other object internal data
//
//==========================================================================
void VObject::SerialiseOther (VStream &Strm) {
}


//==========================================================================
//
//  VObject::Serialise
//
//  this calls field serialisation, then other serialisation
//
//==========================================================================
void VObject::Serialise (VStream &strm) {
  if (strm.IsLoading()) {
    // reading
    VName clsname = NAME_None;
    strm << clsname;
    if (strm.IsError()) Host_Error("error reading object of class `%s`", GetClass()->GetName());
    if (clsname == NAME_None) Host_Error("cannot load object of `none` class");
    VClass *cls = VClass::FindClass(*clsname);
    if (!cls) Host_Error("cannot load object of unknown `%s` class", *clsname);
    //if (!GetClass()->IsChildOf(cls)) Host_Error("cannot load object of class `%s` class (not a subclass of `%s`)", GetClass()->GetName(), *clsname);
    if (GetClass() != cls) Host_Error("cannot load object of class `%s` (expected class `%s`)", GetClass()->GetName(), *clsname);
    // skip data size
    vint32 size;
    strm << size;
    if (size < 1) Host_Error("error reading object of class `%s` (invalid size: %d)", *clsname, size);
    auto endpos = strm.Tell()+size;
    // read flags
    vint32 flg;
    strm << STRM_INDEX(flg);
    if (strm.IsError()) Host_Error("error reading object of class `%s`", *clsname);
    if (flg) SetFlags(flg);
    if (ObjectFlags&_OF_Destroyed) {
      // no need to read anything, just skip it all
      if (strm.Tell() > endpos) Host_Error("error reading object of class `%s` (invalid data size)", *clsname);
      strm.Seek(endpos);
      if (strm.IsError()) Host_Error("error reading object of class `%s`", *clsname);
      return;
    }
    // read fields
    SerialiseFields(strm);
    if (strm.IsError()) Host_Error("error reading object of class `%s`", *clsname);
    // read other data
    SerialiseOther(strm);
    if (strm.IsError()) Host_Error("error reading object of class `%s`", *clsname);
    // check if all data was read
    if (strm.Tell() != endpos) Host_Error("error reading object of class `%s` (not all data read)", *clsname);
  } else {
    // writing
    VName clsname = GetClass()->Name;
    strm << clsname;
    // write dummy data size (will be fixed later)
    auto szpos = strm.Tell();
    vint32 size = 0;
    strm << size;
    // write flags
    vint32 flg = ObjectFlags;
    strm << STRM_INDEX(flg);
    // is object dead? don't write data of dead objects
    if ((ObjectFlags&_OF_Destroyed) == 0) {
      // write fields
      SerialiseFields(strm);
      if (strm.IsError()) return;
      // write other data
      SerialiseOther(strm);
      if (strm.IsError()) return;
    }
    // fix data size
    auto cpos = strm.Tell();
    size = cpos-(szpos+(int)sizeof(vint32));
    strm.Seek(szpos);
    strm << size;
    strm.Seek(cpos);
  }
}


//==========================================================================
//
//  VObject::ExecuteNetMethod
//
//==========================================================================
bool VObject::ExecuteNetMethod (VMethod *func) {
  if (onExecuteNetMethodCB) return onExecuteNetMethodCB(this, func);
  return false;
}


// ////////////////////////////////////////////////////////////////////////// //
class VClassesIterator : public VScriptIterator {
private:
  VClass *BaseClass;
  VClass **Out;
  int Index;

public:
  VClassesIterator (VClass *ABaseClass, VClass **AOut) : BaseClass(ABaseClass), Out(AOut), Index(0) {}

  virtual bool GetNext () override {
    while (Index < VMemberBase::GMembers.Num()) {
      VMemberBase *Check = VMemberBase::GMembers[Index];
      ++Index;
      if (Check->MemberType == MEMBER_Class && ((VClass*)Check)->IsChildOf(BaseClass)) {
        *Out = (VClass*)Check;
        return true;
      }
    }
    *Out = nullptr;
    return false;
  }
};

// ////////////////////////////////////////////////////////////////////////// //
class VClassStatesIterator : public VScriptIterator {
private:
  VState *curr;
  VState **out;

public:
  VClassStatesIterator (VClass *aclass, VState **aout) : curr(nullptr), out(aout) {
    if (aclass) curr = aclass->States;
  }

  virtual bool GetNext () override {
    *out = curr;
    if (curr) {
      curr = curr->Next;
      return true;
    }
    return false;
  }
};

//==========================================================================
//
//  Iterators
//
//==========================================================================
class VObjectsIterator : public VScriptIterator {
private:
  VClass *BaseClass;
  VObject **Out;
  int Index;

public:
  VObjectsIterator (VClass *ABaseClass, VObject **AOut) : BaseClass(ABaseClass), Out(AOut), Index(0) {}

  virtual bool GetNext () override {
    while (Index < VObject::GetObjectsCount()) {
      VObject *Check = VObject::GetIndexObject(Index);
      ++Index;
      if (Check != nullptr && !(Check->GetFlags()&(_OF_DelayedDestroy|_OF_Destroyed)) && Check->IsA(BaseClass)) {
        *Out = Check;
        return true;
      }
    }
    *Out = nullptr;
    return false;
  }
};


//==========================================================================
//
//  VObject::NameFromVKey
//
//==========================================================================
VStr VObject::NameFromVKey (int vkey) {
  if (vkey >= K_N0 && vkey <= K_N9) return VStr(char(vkey));
  if (vkey >= K_a && vkey <= K_z) return VStr(char(vkey-32));
  switch (vkey) {
    case K_ESCAPE: return "ESCAPE";
    case K_ENTER: return "ENTER";
    case K_TAB: return "TAB";
    case K_BACKSPACE: return "BACKSPACE";

    case K_SPACE: return "SPACE";

    case K_UPARROW: return "UP";
    case K_LEFTARROW: return "LEFT";
    case K_RIGHTARROW: return "RIGHT";
    case K_DOWNARROW: return "DOWN";
    case K_INSERT: return "INSERT";
    case K_DELETE: return "DELETE";
    case K_HOME: return "HOME";
    case K_END: return "END";
    case K_PAGEUP: return "PGUP";
    case K_PAGEDOWN: return "PGDOWN";

    case K_PAD0: return "PAD0";
    case K_PAD1: return "PAD1";
    case K_PAD2: return "PAD2";
    case K_PAD3: return "PAD3";
    case K_PAD4: return "PAD4";
    case K_PAD5: return "PAD5";
    case K_PAD6: return "PAD6";
    case K_PAD7: return "PAD7";
    case K_PAD8: return "PAD8";
    case K_PAD9: return "PAD9";

    case K_NUMLOCK: return "NUM";
    case K_PADDIVIDE: return "PAD/";
    case K_PADMULTIPLE: return "PAD*";
    case K_PADMINUS: return "PAD-";
    case K_PADPLUS: return "PAD+";
    case K_PADENTER: return "PADENTER";
    case K_PADDOT: return "PAD.";

    case K_CAPSLOCK: return "CAPS";
    case K_BACKQUOTE: return "BACKQUOTE";

    case K_F1: return "F1";
    case K_F2: return "F2";
    case K_F3: return "F3";
    case K_F4: return "F4";
    case K_F5: return "F5";
    case K_F6: return "F6";
    case K_F7: return "F7";
    case K_F8: return "F8";
    case K_F9: return "F9";
    case K_F10: return "F10";
    case K_F11: return "F11";
    case K_F12: return "F12";

    case K_LSHIFT: return "LSHIFT";
    case K_RSHIFT: return "RSHIFT";
    case K_LCTRL: return "LCTRL";
    case K_RCTRL: return "RCTRL";
    case K_LALT: return "LALT";
    case K_RALT: return "RALT";

    case K_LWIN: return "LWIN";
    case K_RWIN: return "RWIN";
    case K_MENU: return "MENU";

    case K_PRINTSCRN: return "PSCRN";
    case K_SCROLLLOCK: return "SCROLL";
    case K_PAUSE: return "PAUSE";

    case K_MOUSE1: return "MOUSE1";
    case K_MOUSE2: return "MOUSE2";
    case K_MOUSE3: return "MOUSE3";
    case K_MWHEELUP: return "MWHEELUP";
    case K_MWHEELDOWN: return "MWHEELDOWN";

    case K_JOY1: return "JOY1";
    case K_JOY2: return "JOY2";
    case K_JOY3: return "JOY3";
    case K_JOY4: return "JOY4";
    case K_JOY5: return "JOY5";
    case K_JOY6: return "JOY6";
    case K_JOY7: return "JOY7";
    case K_JOY8: return "JOY8";
    case K_JOY9: return "JOY9";
    case K_JOY10: return "JOY10";
    case K_JOY11: return "JOY11";
    case K_JOY12: return "JOY12";
    case K_JOY13: return "JOY13";
    case K_JOY14: return "JOY14";
    case K_JOY15: return "JOY15";
    case K_JOY16: return "JOY16";
  }
  if (vkey > 32 && vkey < 127) return VStr(char(vkey));
  return VStr();
}


//==========================================================================
//
//  VObject::VKeyFromName
//
//==========================================================================
int VObject::VKeyFromName (const VStr &kn) {
  if (kn.isEmpty()) return 0;

  if (kn.length() == 1) {
    vuint8 ch = (vuint8)kn[0];
    if (ch >= '0' && ch <= '9') return K_N0+(ch-'0');
    if (ch >= 'A' && ch <= 'Z') return K_a+(ch-'A');
    if (ch >= 'a' && ch <= 'z') return K_a+(ch-'a');
    if (ch == ' ') return K_SPACE;
    if (ch == '`' || ch == '~') return K_BACKQUOTE;
    if (ch == 27) return K_ESCAPE;
    if (ch == 13 || ch == 10) return K_ENTER;
    if (ch == 8) return K_BACKSPACE;
    if (ch == 9) return K_TAB;
    if (ch > 32 && ch < 127) return (int)ch;
  }

  if (kn.ICmp("ESCAPE") == 0) return K_ESCAPE;
  if (kn.ICmp("ENTER") == 0) return K_ENTER;
  if (kn.ICmp("RETURN") == 0) return K_ENTER;
  if (kn.ICmp("TAB") == 0) return K_TAB;
  if (kn.ICmp("BACKSPACE") == 0) return K_BACKSPACE;

  if (kn.ICmp("SPACE") == 0) return K_SPACE;

  if (kn.ICmp("UPARROW") == 0 || kn.ICmp("UP") == 0) return K_UPARROW;
  if (kn.ICmp("LEFTARROW") == 0 || kn.ICmp("LEFT") == 0) return K_LEFTARROW;
  if (kn.ICmp("RIGHTARROW") == 0 || kn.ICmp("RIGHT") == 0) return K_RIGHTARROW;
  if (kn.ICmp("DOWNARROW") == 0 || kn.ICmp("DOWN") == 0) return K_DOWNARROW;
  if (kn.ICmp("INSERT") == 0 || kn.ICmp("INS") == 0) return K_INSERT;
  if (kn.ICmp("DELETE") == 0 || kn.ICmp("DEL") == 0) return K_DELETE;
  if (kn.ICmp("HOME") == 0) return K_HOME;
  if (kn.ICmp("END") == 0) return K_END;
  if (kn.ICmp("PAGEUP") == 0 || kn.ICmp("PGUP") == 0) return K_PAGEUP;
  if (kn.ICmp("PAGEDOWN") == 0 || kn.ICmp("PGDOWN") == 0 || kn.ICmp("PGDN") == 0) return K_PAGEDOWN;

  if (kn.ICmp("PAD0") == 0 || kn.ICmp("PAD_0") == 0) return K_PAD0;
  if (kn.ICmp("PAD1") == 0 || kn.ICmp("PAD_1") == 0) return K_PAD1;
  if (kn.ICmp("PAD2") == 0 || kn.ICmp("PAD_2") == 0) return K_PAD2;
  if (kn.ICmp("PAD3") == 0 || kn.ICmp("PAD_3") == 0) return K_PAD3;
  if (kn.ICmp("PAD4") == 0 || kn.ICmp("PAD_4") == 0) return K_PAD4;
  if (kn.ICmp("PAD5") == 0 || kn.ICmp("PAD_5") == 0) return K_PAD5;
  if (kn.ICmp("PAD6") == 0 || kn.ICmp("PAD_6") == 0) return K_PAD6;
  if (kn.ICmp("PAD7") == 0 || kn.ICmp("PAD_7") == 0) return K_PAD7;
  if (kn.ICmp("PAD8") == 0 || kn.ICmp("PAD_8") == 0) return K_PAD8;
  if (kn.ICmp("PAD9") == 0 || kn.ICmp("PAD_9") == 0) return K_PAD9;

  if (kn.ICmp("NUMLOCK") == 0 || kn.ICmp("NUM") == 0) return K_NUMLOCK;
  if (kn.ICmp("PADDIVIDE") == 0 || kn.ICmp("PAD_DIVIDE") == 0 || kn.ICmp("PAD/") == 0) return K_PADDIVIDE;
  if (kn.ICmp("PADMULTIPLE") == 0 || kn.ICmp("PAD_MULTIPLE") == 0 || kn.ICmp("PAD*") == 0) return K_PADMULTIPLE;
  if (kn.ICmp("PADMINUS") == 0 || kn.ICmp("PAD_MINUS") == 0 || kn.ICmp("PAD-") == 0) return K_PADMINUS;
  if (kn.ICmp("PADPLUS") == 0 || kn.ICmp("PAD_PLUS") == 0 || kn.ICmp("PAD+") == 0) return K_PADPLUS;
  if (kn.ICmp("PADENTER") == 0 || kn.ICmp("PAD_ENTER") == 0) return K_PADENTER;
  if (kn.ICmp("PADDOT") == 0 || kn.ICmp("PAD_DOT") == 0 || kn.ICmp("PAD.") == 0) return K_PADDOT;

  if (kn.ICmp("CAPSLOCK") == 0 || kn.ICmp("CAPS") == 0) return K_CAPSLOCK;
  if (kn.ICmp("BACKQUOTE") == 0) return K_BACKQUOTE;

  if (kn.ICmp("F1") == 0) return K_F1;
  if (kn.ICmp("F2") == 0) return K_F2;
  if (kn.ICmp("F3") == 0) return K_F3;
  if (kn.ICmp("F4") == 0) return K_F4;
  if (kn.ICmp("F5") == 0) return K_F5;
  if (kn.ICmp("F6") == 0) return K_F6;
  if (kn.ICmp("F7") == 0) return K_F7;
  if (kn.ICmp("F8") == 0) return K_F8;
  if (kn.ICmp("F9") == 0) return K_F9;
  if (kn.ICmp("F10") == 0) return K_F10;
  if (kn.ICmp("F11") == 0) return K_F11;
  if (kn.ICmp("F12") == 0) return K_F12;

  if (kn.ICmp("LSHIFT") == 0) return K_LSHIFT;
  if (kn.ICmp("RSHIFT") == 0) return K_RSHIFT;
  if (kn.ICmp("LCTRL") == 0) return K_LCTRL;
  if (kn.ICmp("RCTRL") == 0) return K_RCTRL;
  if (kn.ICmp("LALT") == 0) return K_LALT;
  if (kn.ICmp("RALT") == 0) return K_RALT;

  if (kn.ICmp("LWIN") == 0) return K_LWIN;
  if (kn.ICmp("RWIN") == 0) return K_RWIN;
  if (kn.ICmp("MENU") == 0) return K_MENU;

  if (kn.ICmp("PRINTSCRN") == 0 || kn.ICmp("PSCRN") == 0 || kn.ICmp("PRINTSCREEN") == 0 ) return K_PRINTSCRN;
  if (kn.ICmp("SCROLLLOCK") == 0 || kn.ICmp("SCROLL") == 0) return K_SCROLLLOCK;
  if (kn.ICmp("PAUSE") == 0) return K_PAUSE;

  if (kn.ICmp("MOUSE1") == 0) return K_MOUSE1;
  if (kn.ICmp("MOUSE2") == 0) return K_MOUSE2;
  if (kn.ICmp("MOUSE3") == 0) return K_MOUSE3;
  if (kn.ICmp("MWHEELUP") == 0) return K_MWHEELUP;
  if (kn.ICmp("MWHEELDOWN") == 0) return K_MWHEELDOWN;

  if (kn.ICmp("JOY1") == 0) return K_JOY1;
  if (kn.ICmp("JOY2") == 0) return K_JOY2;
  if (kn.ICmp("JOY3") == 0) return K_JOY3;
  if (kn.ICmp("JOY4") == 0) return K_JOY4;
  if (kn.ICmp("JOY5") == 0) return K_JOY5;
  if (kn.ICmp("JOY6") == 0) return K_JOY6;
  if (kn.ICmp("JOY7") == 0) return K_JOY7;
  if (kn.ICmp("JOY8") == 0) return K_JOY8;
  if (kn.ICmp("JOY9") == 0) return K_JOY9;
  if (kn.ICmp("JOY10") == 0) return K_JOY10;
  if (kn.ICmp("JOY11") == 0) return K_JOY11;
  if (kn.ICmp("JOY12") == 0) return K_JOY12;
  if (kn.ICmp("JOY13") == 0) return K_JOY13;
  if (kn.ICmp("JOY14") == 0) return K_JOY14;
  if (kn.ICmp("JOY15") == 0) return K_JOY15;
  if (kn.ICmp("JOY16") == 0) return K_JOY16;

  return 0;
}

#include "vc_object_evqueue.cpp"

#include "vc_object_common.cpp"

#if defined(VCC_STANDALONE_EXECUTOR)
# include "vc_object_vccrun.cpp"
#elif defined(IN_VCC)
# include "vc_object_vcc.cpp"
#else
# include "vc_object_vavoom.cpp"
#endif
