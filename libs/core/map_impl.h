//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2018 Ketmar Dark
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
//**  Template for mapping kays to values.
//**
//**************************************************************************
//#define TMAP_DO_DTOR
//#define TMAP_NO_CLEAR


// ////////////////////////////////////////////////////////////////////////// //
template<class TK, class TV> class TMap_Class_Name {
public:
  static inline vuint32 nextPOTU32 (vuint32 x) {
    vuint32 res = x;
    res |= (res>>1);
    res |= (res>>2);
    res |= (res>>4);
    res |= (res>>8);
    res |= (res>>16);
    // already pot?
    if (x != 0 && (x&(x-1)) == 0) res &= ~(res>>1); else ++res;
    return res;
  }

  inline static vuint32 hashU32 (vuint32 a) {
    vuint32 res = (vuint32)a;
    res -= (res<<6);
    res = res^(res>>17);
    res -= (res<<9);
    res = res^(res<<4);
    res -= (res<<3);
    res = res^(res<<10);
    res = res^(res>>15);
    return res;
  }

private:
  enum {
    InitSize = 256, // *MUST* be power of two
    LoadFactorPrc = 90, // it is ok for robin hood hashes
  };

  struct TEntry {
    TK key;
    TV value;
    vuint32 hash;
    TEntry *nextFree; // next free entry
    bool empty;
  };

private:
  vuint32 mEBSize;
  TEntry *mEntries;
  TEntry **mBuckets;
  int mBucketsUsed;
  TEntry *mFreeEntryHead;
  int mFirstEntry, mLastEntry;
  vuint32 mSeed;
  vuint32 mSeedCount;

public:
  class TIterator {
  private:
    TMap_Class_Name &map;
    vuint32 index;
  public:
    TIterator (TMap_Class_Name &amap) : map(amap), index(0) {
      if (amap.mFirstEntry < 0) {
        index = amap.mEBSize;
      } else {
        index = (vuint32)amap.mFirstEntry;
        while (index < amap.mEBSize && amap.mEntries[index].empty) ++index;
      }
    }
    inline operator bool () const { return (index < map.mEBSize); }
    inline void operator ++ () {
      if (index < map.mEBSize) {
        ++index;
        if ((int)index > map.mLastEntry) {
          index = map.mEBSize;
        } else {
          while (index < map.mEBSize && map.mEntries[index].empty) ++index;
        }
      }
    }
    inline const TK &GetKey () const { return map.mEntries[index].key; }
    inline const TV &GetValue () const { return map.mEntries[index].value; }
    inline TV &GetValue () { return map.mEntries[index].value; }
    inline const TK &getKey () const { return map.mEntries[index].key; }
    inline const TV &getValue () const { return map.mEntries[index].value; }
    inline TV &getValue () { return map.mEntries[index].value; }
    inline void removeCurrent () {
      if (index < map.mEBSize && !map.mEntries[index].empty) {
        map.del(map.mEntries[index].key);
        operator++();
        //while (index < map.mEBSize && map.mEntries[index].empty) ++index;
      }
    }
    inline void RemoveCurrent () { removeCurrent(); }
    inline void resetToFirst () {
      if (map.mFirstEntry < 0) {
        index = map.mEBSize;
      } else {
        index = (vuint32)map.mFirstEntry;
        while (index < map.mEBSize && map.mEntries[index].empty) ++index;
      }
    }
  };

  friend class TIterator;

private:
  void freeEntries () {
#if defined(TMAP_DO_DTOR) || !defined(TMAP_NO_CLEAR)
    if (mFirstEntry >= 0) {
      for (int f = mFirstEntry; f <= mLastEntry; ++f) {
        TEntry *e = &mEntries[f];
        if (!e->empty) {
          // free key
#if defined(TMAP_DO_DTOR)
          e->key.~TK();
          e->value.~TV();
#elif !defined(TMAP_NO_CLEAR)
          e->key = TK();
          e->value = TV();
#endif
        }
      }
    }
#endif
    if (mEBSize > 0) {
      memset((void *)(&mEntries[0]), 0, mEBSize*sizeof(TEntry));
      for (vuint32 f = 0; f < mEBSize; ++f) {
        TEntry *e = &mEntries[f];
        e->empty = true;
      }
    }
    mFreeEntryHead = nullptr;
    mFirstEntry = mLastEntry = -1;
  }

  TEntry *allocEntry () {
    TEntry *res;
    if (!mFreeEntryHead) {
      // nothing was allocated, so allocate something now
      if (mEBSize == 0) {
        mEBSize = InitSize;
        mBuckets = (TEntry **)Z_Malloc(mEBSize*sizeof(TEntry *));
        memset(&mBuckets[0], 0, mEBSize*sizeof(TEntry *));
        mEntries = (TEntry *)Z_Malloc(mEBSize*sizeof(TEntry));
        memset((void *)(&mEntries[0]), 0, mEBSize*sizeof(TEntry));
        for (vuint32 f = 0; f < mEBSize; ++f) mEntries[f].empty = true;
      }
      ++mLastEntry;
      if (mFirstEntry == -1) {
        //if (mLastEntry != 0) then raise Exception.Create('internal error in hash entry allocator (0.1)');
        mFirstEntry = 0;
      }
      res = &mEntries[mLastEntry];
    } else {
      res = mFreeEntryHead;
      mFreeEntryHead = res->nextFree;
      // fix mFirstEntry and mLastEntry
      int idx = (int)(res-&mEntries[0]);
      if (mFirstEntry < 0 || idx < mFirstEntry) mFirstEntry = idx;
      if (idx > mLastEntry) mLastEntry = idx;
    }
    res->nextFree = nullptr; // just in case
    res->empty = false;
    return res;
  }

  void releaseEntry (TEntry *e) {
    int idx = (int)(e-&mEntries[0]);
#if defined(TMAP_DO_DTOR)
    e->key.~TK();
    e->value.~TV();
#elif !defined(TMAP_NO_CLEAR)
    e->key = TK();
    e->value = TV();
#endif
    memset((void *)e, 0, sizeof(*e));
    e->empty = true;
    e->nextFree = mFreeEntryHead;
    mFreeEntryHead = e;
    // fix mFirstEntry and mLastEntry
    if (mFirstEntry == mLastEntry) {
      mFreeEntryHead = nullptr;
      mFirstEntry = mLastEntry = -1;
    } else {
      // fix first entry index
      if (idx == mFirstEntry) {
        int cidx = idx+1;
        while (mEntries[cidx].empty) ++cidx;
        mFirstEntry = cidx;
      }
      // fix last entry index
      if (idx == mLastEntry) {
        int cidx = idx-1;
        while (mEntries[cidx].empty) --cidx;
        mLastEntry = cidx;
      }
    }
  }

  inline vuint32 distToStIdx (vuint32 idx) const {
    vuint32 res = (mBuckets[idx]->hash^mSeed)&(vuint32)(mEBSize-1);
    res = (res <= idx ? idx-res : idx+(mEBSize-res));
    return res;
  }

  void putEntryInternal (TEntry *swpe) {
    vuint32 bhigh = (vuint32)(mEBSize-1);
    vuint32 idx = (swpe->hash^mSeed)&bhigh;
    vuint32 pcur = 0;
    for (vuint32 dist = 0; dist <= bhigh; ++dist) {
      if (!mBuckets[idx]) {
        // put entry
        mBuckets[idx] = swpe;
        ++mBucketsUsed;
        return;
      }
      vuint32 pdist = distToStIdx(idx);
      if (pcur > pdist) {
        // swapping the current bucket with the one to insert
        TEntry *tmpe = mBuckets[idx];
        mBuckets[idx] = swpe;
        swpe = tmpe;
        pcur = pdist;
      }
      idx = (idx+1)&bhigh;
      ++pcur;
    }
    abort();
  }

  inline int getCapacity () const { return (int)mEBSize; }

public:
  TMap_Class_Name () : mEBSize(0), mEntries(nullptr), mBuckets(nullptr), mBucketsUsed(0), mFreeEntryHead(nullptr), mFirstEntry(-1), mLastEntry(-1), mSeed(0), mSeedCount(0) {}

  TMap_Class_Name (TMap_Class_Name &other) : mEBSize(0), mEntries(nullptr), mBuckets(nullptr), mBucketsUsed(0), mFreeEntryHead(nullptr), mFirstEntry(-1), mLastEntry(-1), mSeed(0), mSeedCount(0) {
    operator=(other);
  }

  ~TMap_Class_Name () { clear(); }

  TMap_Class_Name &operator = (const TMap_Class_Name &other) {
    if (&other != this) {
      clear();
      // copy entries
      if (other.mBucketsUsed > 0) {
        // has some entries
        mEBSize = nextPOTU32(vuint32(mBucketsUsed));
        mBuckets = (TEntry **)Z_Malloc(mEBSize*sizeof(TEntry *));
        memset(&mBuckets[0], 0, mEBSize*sizeof(TEntry *));
        mEntries = (TEntry *)Z_Malloc(mEBSize*sizeof(TEntry));
        memset((void *)(&mEntries[0]), 0, mEBSize*sizeof(TEntry));
        for (vuint32 f = 0; f < mEBSize; ++f) mEntries[f].empty = true;
        mFirstEntry = mLastEntry = -1;
        for (vuint32 f = 0; f < mEBSize; ++f) {
          if (f >= other.mEBSize) break;
          mEntries[f] = other.mEntries[f];
          if (!mEntries[f].empty) {
            if (mFirstEntry < 0) mFirstEntry = (int)f;
            mLastEntry = (int)f;
          }
        }
        rehash();
      }
    }
    return *this;
  }

  void clear () {
#if defined(TMAP_DO_DTOR) || !defined(TMAP_NO_CLEAR)
    freeEntries();
#endif
    mFreeEntryHead = nullptr;
    Z_Free(mBuckets);
    mBucketsUsed = 0;
    mBuckets = nullptr;
    Z_Free((void *)mEntries);
    mEBSize = 0;
    mEntries = nullptr;
    mFreeEntryHead = nullptr;
    mFirstEntry = mLastEntry = -1;
  }

  // won't shrink buckets
  void reset () {
    freeEntries();
    if (mBucketsUsed > 0) {
      memset(mBuckets, 0, mEBSize*sizeof(TEntry *));
      mBucketsUsed = 0;
    }
  }

  void rehash () {
    // change seed, to minimize pathological cases
    //TODO: use prng to generate new hash
    if (++mSeedCount == 0) mSeedCount = 1;
    mSeed = hashU32(mSeedCount);
    // clear buckets
    memset(mBuckets, 0, mEBSize*sizeof(TEntry *));
    mBucketsUsed = 0;
    // reinsert entries
    mFreeEntryHead = nullptr;
    TEntry *lastfree = nullptr;
    for (vuint32 idx = 0; idx < mEBSize; ++idx) {
      TEntry *e = &mEntries[idx];
      if (!e->empty) {
        // no need to recalculate hash
        putEntryInternal(e);
      } else {
        if (lastfree) lastfree->nextFree = e; else mFreeEntryHead = e;
        lastfree = e;
      }
    }
    if (lastfree) lastfree->nextFree = nullptr;
  }

  // call this instead of `rehash()` after alot of deletions
  void compact () {
    vuint32 newsz = nextPOTU32((vuint32)mBucketsUsed);
    if (newsz >= 1024*1024*1024) return;
    if (newsz*2 >= mEBSize) return;
    if (newsz*2 < 128) return;
    newsz *= 2;
#ifdef CORE_MAP_TEST
    printf("compacting; old size=%u; new size=%u; used=%d; fe=%d; le=%d\n", mEBSize, newsz, mBucketsUsed, mFirstEntry, mLastEntry);
#endif
    // move all entries to top
    if (mFirstEntry >= 0) {
      vuint32 didx = 0;
      while (didx < mEBSize) if (!mEntries[didx].empty) ++didx; else break;
      vuint32 f = didx+1;
      // copy entries
      for (;;) {
        if (!mEntries[f].empty) {
          mEntries[didx] = mEntries[f];
          mEntries[f].empty = true;
          ++didx;
          if (f == (vuint32)mLastEntry) break;
          while (didx < mEBSize) if (!mEntries[didx].empty) ++didx; else break;
        }
        if (++f > (vuint32)mLastEntry) break;
      }
      mFirstEntry = 0;
      mLastEntry = mBucketsUsed-1;
    }
    // shrink
    mBuckets = (TEntry **)Z_Realloc(mBuckets, newsz*sizeof(TEntry *));
    // shrink
    mEntries = (TEntry *)Z_Realloc((void *)mEntries, newsz*sizeof(TEntry));
    mEBSize = newsz;
    // mFreeEntryHead will be fixed in `rehash()`
    // reinsert entries
    rehash();
  }

  bool has (const TK &akey) const {
    if (mBucketsUsed == 0) return false;
    vuint32 bhigh = (vuint32)(mEBSize-1);
    vuint32 khash = GetTypeHash(akey);
    vuint32 idx = (khash^mSeed)&bhigh;
    if (!mBuckets[idx]) return false;
    bool res = false;
    for (vuint32 dist = 0; dist <= bhigh; ++dist) {
      if (!mBuckets[idx]) break;
      vuint32 pdist = distToStIdx(idx);
      if (dist > pdist) break;
      res = (mBuckets[idx]->hash == khash && mBuckets[idx]->key == akey);
      if (res) break;
      idx = (idx+1)&bhigh;
    }
    return res;
  }

  const TV *get (const TK &akey) const {
    if (mBucketsUsed == 0) return nullptr;
    vuint32 bhigh = (vuint32)(mEBSize-1);
    vuint32 khash = GetTypeHash(akey);
    vuint32 idx = (khash^mSeed)&bhigh;
    if (!mBuckets[idx]) return nullptr;
    for (vuint32 dist = 0; dist <= bhigh; ++dist) {
      if (!mBuckets[idx]) break;
      vuint32 pdist = distToStIdx(idx);
      if (dist > pdist) break;
      if (mBuckets[idx]->hash == khash && mBuckets[idx]->key == akey) return &(mBuckets[idx]->value);
      idx = (idx+1)&bhigh;
    }
    return nullptr;
  }

  TV *get (const TK &akey) {
    if (mBucketsUsed == 0) return nullptr;
    vuint32 bhigh = (vuint32)(mEBSize-1);
    vuint32 khash = GetTypeHash(akey);
    vuint32 idx = (khash^mSeed)&bhigh;
    if (!mBuckets[idx]) return nullptr;
    for (vuint32 dist = 0; dist <= bhigh; ++dist) {
      if (!mBuckets[idx]) break;
      vuint32 pdist = distToStIdx(idx);
      if (dist > pdist) break;
      if (mBuckets[idx]->hash == khash && mBuckets[idx]->key == akey) return &(mBuckets[idx]->value);
      idx = (idx+1)&bhigh;
    }
    return nullptr;
  }

  //WARNING! returned pointer will be invalidated by any map mutation
  inline TV *Find (const TK &Key) { return get(Key); }
  inline TV *find (const TK &Key) { return get(Key); }
  inline const TV *Find (const TK &Key) const { return get(Key); }
  inline const TV *find (const TK &Key) const { return get(Key); }

  inline const TV FindPtr (const TK &Key) const {
    auto res = get(Key);
    if (res) return *res;
    return nullptr;
  }
  inline const TV findptr (const TK &Key) const { return FindPtr(Key); }

  // see http://codecapsule.com/2013/11/17/robin-hood-hashing-backward-shift-deletion/
  bool del (const TK &akey) {
    if (mBucketsUsed == 0) return false;

    vuint32 bhigh = (vuint32)(mEBSize-1);
    vuint32 khash = GetTypeHash(akey);
    vuint32 idx = (khash^mSeed)&bhigh;

    // find key
    if (!mBuckets[idx]) return false; // no key
    bool res = false;
    for (vuint32 dist = 0; dist <= bhigh; ++dist) {
      if (!mBuckets[idx]) break;
      vuint32 pdist = distToStIdx(idx);
      if (dist > pdist) break;
      res = (mBuckets[idx]->hash == khash && mBuckets[idx]->key == akey);
      if (res) break;
      idx = (idx+1)&bhigh;
    }

    if (!res) return false; // key not found

    releaseEntry(mBuckets[idx]);

    vuint32 idxnext = (idx+1)&bhigh;
    for (vuint32 dist = 0; dist <= bhigh; ++dist) {
      if (!mBuckets[idxnext]) { mBuckets[idx] = nullptr; break; }
      vuint32 pdist = distToStIdx(idxnext);
      if (pdist == 0) { mBuckets[idx] = nullptr; break; }
      mBuckets[idx] = mBuckets[idxnext];
      idx = (idx+1)&bhigh;
      idxnext = (idxnext+1)&bhigh;
    }

    --mBucketsUsed;
    return true;
  }

  bool Remove (const TK &Key) { return del(Key); }
  bool remove (const TK &Key) { return del(Key); }

  // returns `true` if old value was replaced
  bool put (const TK &akey, const TV &aval) {
    vuint32 bhigh = (vuint32)(mEBSize-1);
    vuint32 khash = GetTypeHash(akey);
    vuint32 idx = (khash^mSeed)&bhigh;

    // check if we already have this key
    if (mBucketsUsed != 0 && mBuckets[idx]) {
      for (vuint32 dist = 0; dist <= bhigh; ++dist) {
        TEntry *e = mBuckets[idx];
        if (!e) break;
        vuint32 pdist = distToStIdx(idx);
        if (dist > pdist) break;
        if (e->hash == khash && e->key == akey) {
          // replace element
          e->value = aval;
          return true;
        }
        idx = (idx+1)&bhigh;
      }
    }

    // need to resize hash?
    if ((vuint32)mBucketsUsed >= (bhigh+1)*LoadFactorPrc/100) {
      vuint32 newsz = (vuint32)mEBSize;
      //if (Length(mEntries) <> newsz) then raise Exception.Create('internal error in hash table (resize)');
      //if (newsz <= 1024*1024*1024) then newsz *= 2 else raise Exception.Create('hash table too big');
      newsz *= 2;
      // resize buckets array
      mBuckets = (TEntry **)Z_Realloc(mBuckets, newsz*sizeof(TEntry *));
      memset(mBuckets+mEBSize, 0, (newsz-mEBSize)*sizeof(TEntry *));
      // resize entries array
      mEntries = (TEntry *)Z_Realloc((void *)mEntries, newsz*sizeof(TEntry));
      memset((void *)(mEntries+mEBSize), 0, (newsz-mEBSize)*sizeof(TEntry));
      for (vuint32 f = mEBSize; f < newsz; ++f) mEntries[f].empty = true;
      mEBSize = newsz;
      // mFreeEntryHead will be fixed in `rehash()`
      // reinsert entries
      rehash();
    }

    // create new entry
    TEntry *swpe = allocEntry();
    swpe->key = akey;
    swpe->value = aval;
    swpe->hash = khash;

    putEntryInternal(swpe);
    return false;
  }

  inline void Set (const TK &Key, const TV &Value) { put(Key, Value); }
  inline void set (const TK &Key, const TV &Value) { put(Key, Value); }

  inline int count () const { return (int)mBucketsUsed; }
  inline int capacity () const { return getCapacity(); }

#ifdef CORE_MAP_TEST
  int countItems () const {
    int res = 0;
    for (vuint32 f = 0; f < mEBSize; ++f) if (!mEntries[f].empty) ++res;
    return res;
  }
#endif

  TIterator first () { return TIterator(*this); }
};
