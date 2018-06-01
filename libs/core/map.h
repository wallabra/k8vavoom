//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
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
#ifdef CORE_MAP_TEST
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int vuint32;

static vuint32 GetTypeHash (int a) {
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
#endif



/*
template<class TK, class TV> class TMap {
protected:
  struct TPair {
    TK Key;
    TV Value;
    vint32 HashNext;
  };

  TArray<TPair> Pairs;
  vint32 *HashTable;
  vint32 HashSize;

  void Rehash () {
    guardSlow(TMap::Rehash);
    checkSlow(HashSize >= 16);
    if (HashTable) {
      delete[] HashTable;
      HashTable = nullptr;
    }
    HashTable = new vint32[HashSize];
    for (int i = 0; i < HashSize; ++i) HashTable[i] = -1;
    for (int i = 0; i < Pairs.Num(); ++i) {
      int Hash = GetTypeHash(Pairs[i].Key)&(HashSize-1);
      Pairs[i].HashNext = HashTable[Hash];
      HashTable[Hash] = i;
    }
    unguardSlow;
  }

  void Relax () {
    guardSlow(TMap::Relax);
    while (HashSize > Pairs.Num()+16) HashSize >>= 1;
    Rehash();
    unguardSlow;
  }

public:
  TMap () : HashTable(nullptr), HashSize(16) {
    guardSlow(TMap::TMap);
    Rehash();
    unguardSlow;
  }

  TMap (TMap &Other) : Pairs(Other.Pairs), HashTable(nullptr), HashSize(Other.HashSize) {
    guardSlow(TMap::TMap);
    Rehash();
    unguardSlow;
  }

  ~TMap () {
    guardSlow(TMap::~TMap);
    if (HashTable) {
      delete[] HashTable;
      HashTable = nullptr;
    }
    HashTable = nullptr;
    HashSize = 0;
    unguardSlow;
  }

  TMap &operator = (const TMap &Other) {
    guardSlow(TMap::operator=);
    Pairs = Other.Pairs;
    HashSize = Other.HashSize;
    Rehash();
    return *this;
    unguardSlow;
  }

  void Set (const TK &Key, const TV &Value) {
    guardSlow(TMap::Set);
    int HashIndex = GetTypeHash(Key) & (HashSize - 1);
    for (int i = HashTable[HashIndex]; i != -1; i = Pairs[i].HashNext) {
      if (Pairs[i].Key == Key) {
        Pairs[i].Value = Value;
        return;
      }
    }
    TPair &Pair = Pairs.Alloc();
    Pair.HashNext = HashTable[HashIndex];
    HashTable[HashIndex] = Pairs.Num()-1;
    Pair.Key = Key;
    Pair.Value = Value;
    if (HashSize*2+16 < Pairs.Num()) {
      HashSize <<= 1;
      Rehash();
    }
    unguardSlow;
  }
  inline void set (const TK &Key, const TV &Value) { Set(Key, Value); }
  inline void put (const TK &Key, const TV &Value) { Set(Key, Value); }

  //FIXME: this is dog slow!
  bool Remove (const TK &Key) {
    guardSlow(TMap::Remove);
    bool Removed = false;
    int HashIndex = GetTypeHash(Key)&(HashSize-1);
    for (int i = HashTable[HashIndex]; i != -1; i = Pairs[i].HashNext) {
      if (Pairs[i].Key == Key) {
        Pairs.RemoveIndex(i);
        Removed = true;
        break;
      }
    }
    if (Removed) Relax();
    return Removed;
    unguardSlow;
  }
  inline bool remove (const TK &Key) { return Remove(Key); }

  bool has (const TK &Key) const {
    int HashIndex = GetTypeHash(Key)&(HashSize-1);
    for (int i = HashTable[HashIndex]; i != -1; i = Pairs[i].HashNext) {
      if (Pairs[i].Key == Key) return true;
    }
    return false;
  }

  //WARNING! returned pointer will be invalidated by any map mutation
  TV *Find (const TK &Key) {
    guardSlow(TMap::Find);
    int HashIndex = GetTypeHash(Key)&(HashSize-1);
    for (int i = HashTable[HashIndex]; i != -1; i = Pairs[i].HashNext) {
      if (Pairs[i].Key == Key) return &Pairs[i].Value;
    }
    return nullptr;
    unguardSlow;
  }
  inline TV *find (const TK &Key) { return Find(Key); }

  //WARNING! returned pointer will be invalidated by any map mutation
  const TV *Find (const TK &Key) const {
    guardSlow(TMap::Find);
    int HashIndex = GetTypeHash(Key)&(HashSize-1);
    for (int i = HashTable[HashIndex]; i != -1; i = Pairs[i].HashNext) {
      if (Pairs[i].Key == Key) return &Pairs[i].Value;
    }
    return nullptr;
    unguardSlow;
  }
  inline const TV *find (const TK &Key) const { return Find(Key); }

  const TV FindPtr (const TK &Key) const {
    guardSlow(TMap::Find);
    int HashIndex = GetTypeHash(Key)&(HashSize-1);
    for (int i = HashTable[HashIndex]; i != -1; i = Pairs[i].HashNext) {
      if (Pairs[i].Key == Key) return Pairs[i].Value;
    }
    return nullptr;
    unguardSlow;
  }
  inline const TV *findptr (const TK &Key) const { return FindPtr(Key); }

  class TIterator {
  private:
    TMap &Map;
    vint32 Index;
  public:
    TIterator (TMap &AMap) : Map(AMap), Index(0) {}
    inline operator bool () const { return Index < Map.Pairs.Num(); }
    inline void operator ++ () { ++Index; }
    inline const TK &GetKey () const { return Map.Pairs[Index].Key; }
    inline const TV &GetValue () const { return Map.Pairs[Index].Value; }
    inline void RemoveCurrent () { Map.Pairs.RemoveIndex(Index); Map.Relax(); --Index; }
  };

  friend class TIterator;
};
*/

// ////////////////////////////////////////////////////////////////////////// //
template<class TK, class TV> class TMap {
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
  TEntry* mEntries;
  TEntry** mBuckets;
  int mBucketsUsed;
  TEntry *mFreeEntryHead;
  int mFirstEntry, mLastEntry;
  vuint32 mSeed;
  vuint32 mSeedCount;

public:
  class TIterator {
  private:
    TMap &map;
    vuint32 index;
  public:
    TIterator (TMap &amap) : map(amap), index(0) {
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
    inline const TK &getKey () const { return map.mEntries[index].key; }
    inline const TV &getValue () const { return map.mEntries[index].value; }
    inline void removeCurrent () {
      if (index < map.mEBSize && !map.mEntries[index].empty) {
        map.del(map.mEntries[index].key);
        operator++();
        //while (index < map.mEBSize && map.mEntries[index].empty) ++index;
      }
    }
    inline void RemoveCurrent () { removeCurrent(); }
  };

  friend class TIterator;

private:
  void freeEntries () {
    if (mFirstEntry >= 0) {
      for (int f = mFirstEntry; f <= mLastEntry; ++f) {
        TEntry *e = &mEntries[f];
        if (!e->empty) {
          // free key
          e->key = TK();
          e->value = TV();
          e->empty = true;
          e->nextFree = nullptr;
        }
      }
    } else if (mEBSize > 0) {
      for (vuint32 f = 0; f < mEBSize; ++f) {
        TEntry *e = &mEntries[f];
        e->key = TK();
        e->value = TV();
        e->empty = true;
        e->nextFree = nullptr;
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
        mBuckets = (TEntry **)malloc(mEBSize*sizeof(TEntry *));
        memset(&mBuckets[0], 0, mEBSize*sizeof(TEntry *));
        mEntries = (TEntry *)malloc(mEBSize*sizeof(TEntry));
        memset(&mEntries[0], 0, mEBSize*sizeof(TEntry));
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
    e->key = TK();
    e->value = TV();
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
    *(int *)0 = 0;
  }

  inline int getCapacity () const { return mBucketsUsed; }

public:
  TMap () : mEBSize(0), mEntries(nullptr), mBuckets(nullptr), mBucketsUsed(0), mFreeEntryHead(nullptr), mFirstEntry(-1), mLastEntry(-1), mSeed(0), mSeedCount(0) {}

  TMap (TMap &other) : mEBSize(0), mEntries(nullptr), mBuckets(nullptr), mBucketsUsed(0), mFreeEntryHead(nullptr), mFirstEntry(-1), mLastEntry(-1), mSeed(0), mSeedCount(0) {
    operator=(other);
  }

  ~TMap () { clear(); }

  TMap &operator = (const TMap &other) {
    if (&other != this) {
      clear();
      // copy entries
      if (other.mBucketsUsed > 0) {
        // has some entries
        mEBSize = nextPOTU32(vuint32(mBucketsUsed));
        mBuckets = (TEntry **)malloc(mEBSize*sizeof(TEntry *));
        memset(&mBuckets[0], 0, mEBSize*sizeof(TEntry *));
        mEntries = (TEntry *)malloc(mEBSize*sizeof(TEntry));
        memset(&mEntries[0], 0, mEBSize*sizeof(TEntry));
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
    freeEntries();
    mFreeEntryHead = nullptr;
    free(mBuckets);
    mBuckets = nullptr;
    free(mEntries);
    mEntries = nullptr;
    mFirstEntry = mLastEntry = -1;
    mBucketsUsed = 0;
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
    mBuckets = (TEntry **)realloc(mBuckets, newsz*sizeof(TEntry *));
    // shrink
    mEntries = (TEntry *)realloc(mEntries, newsz*sizeof(TEntry));
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
      mBuckets = (TEntry **)realloc(mBuckets, newsz*sizeof(TEntry *));
      memset(mBuckets+mEBSize, 0, (newsz-mEBSize)*sizeof(TEntry *));
      // resize entries array
      mEntries = (TEntry *)realloc(mEntries, newsz*sizeof(TEntry));
      memset(mEntries+mEBSize, 0, (newsz-mEBSize)*sizeof(TEntry));
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


/*
{$PUSH}
{$RANGECHECKS OFF}
function joaatHash (constref buf; len: LongWord; seed: LongWord=0): LongWord;
var
  b: PByte;
  f: LongWord;
begin
  result := seed;
  b := PByte(@buf);
  for f := 1 to len do
  begin
    result += b^;
    result += (result shl 10);
    result := result xor (result shr 6);
    Inc(b);
  end;
  // finalize
  result += (result shl 3);
  result := result xor (result shr 11);
  result += (result shl 15);
end;
{$POP}

{$PUSH}
{$RANGECHECKS OFF}
// fnv-1a: http://www.isthe.com/chongo/tech/comp/fnv/
function fnvHash (constref buf; len: LongWord): LongWord;
var
  b: PByte;
begin
  b := @buf;
  result := 2166136261; // fnv offset basis
  while (len > 0) do
  begin
    result := result xor b^;
    result := result*16777619; // 32-bit fnv prime
    Inc(b);
    Dec(len);
  end;
end;
{$POP}

{$PUSH}
{$RANGECHECKS OFF}
function u32Hash (a: LongWord): LongWord; inline;
begin
  result := a;
  result -= (result shl 6);
  result := result xor (result shr 17);
  result -= (result shl 9);
  result := result xor (result shl 4);
  result -= (result shl 3);
  result := result xor (result shl 10);
  result := result xor (result shr 15);
end;
{$POP}
*/

// ////////////////////////////////////////////////////////////////////////// //
#ifdef CORE_MAP_TEST
#include <stdio.h>
#include <stdlib.h>

static void fatal (const char *msg) {
  fprintf(stderr, "FATAL: %s\n", msg);
  abort();
}

// tests for hash table
enum { MaxItems = 16384 };


int its[MaxItems];
bool marks[MaxItems];
TMap<int, int> hash;


void checkHash (bool dump=false) {
  int count = 0;
  if (dump) printf("====== CHECK ======\n");
  for (int i = 0; i < MaxItems; ++i) {
    if (dump) {
      auto flag = hash.get(i);
      printf(" check #%d; v=%d; flag=%d\n", i, (flag ? *flag : 0), (flag ? 1 : 0));
    }
    if (its[i] >= 0) {
      ++count;
      if (!hash.has(i)) fatal("(0.0) fuuuuuuuuuuuu");
      auto vp = hash.get(i);
      if (!vp) fatal("(0.1) fuuuuuuuuuuuu");
      if (*vp != its[i]) fatal("(0.2) fuuuuuuuuuuuu");
    } else {
      if (hash.has(i)) fatal("(0.3) fuuuuuuuuuuuu");
    }
  }
  if (count != hash.count()) fatal("(0.4) fuuuuuuuuuuuu");
  if (dump) printf("------\n");
}


void testIterator () {
  int count = 0;
  for (int i = 0; i < MaxItems; ++i) marks[i] = false;
  for (auto it = hash.first(); it; ++it) {
    //writeln('key=', k, '; value=', v);
    auto k = it.getKey();
    auto v = it.getValue();
    if (marks[k]) fatal("duplicate entry in iterator");
    if (its[k] != v) fatal("invalid entry in iterator");
    marks[k] = true;
    ++count;
  }
  if (count != hash.count()) {
    printf("0: count=%d; hash.count=%d\n", count, hash.count());
    //raise Exception.Create('lost entries in iterator');
  }
  count = 0;
  for (int i = 0; i < MaxItems; ++i) if (marks[i]) ++count;
  if (count != hash.count()) {
    printf("1: count=%d; hash.count=%d\n", count, hash.count());
    fatal("lost entries in iterator");
  }
  if (hash.count() != hash.countItems()) {
    printf("OOPS: count=%d; countItems=%d\n", hash.count(), hash.countItems());
    fatal("fuck");
  }
}


#define EXCESSIVE_CHECKS
#define EXCESSIVE_CHECKS_ITERATOR
#define EXCESSIVE_COMPACT

int main () {
  int xcount;

  for (int i = 0; i < MaxItems; ++i) its[i] = -1;

  //Randomize();

  printf("testing: insertion\n");
  xcount = 0;
  for (int i = 0; i < MaxItems; ++i) {
    int v = rand()%MaxItems;
    //writeln('i=', i, '; v=', v, '; its[v]=', its[v]);
    if (its[v] >= 0) {
      if (!hash.has(v)) fatal("(1.0) fuuuuuuuuuuuu");
      auto vp = hash.get(v);
      if (!vp) fatal("(1.1) fuuuuuuuuuuuu");
      if (*vp != its[v]) fatal("(1.2) fuuuuuuuuuuuu");
    } else {
      its[v] = i;
      if (hash.put(v, i)) fatal("(1.3) fuuuuuuuuuuuu");
      ++xcount;
      if (xcount != hash.count()) fatal("(1.4) fuuuuuuuuuuuu");
    }
    #ifdef EXCESSIVE_CHECKS
    checkHash();
    #endif
    #ifdef EXCESSIVE_CHECKS_ITERATOR
    testIterator();
    #endif
  }
  if (xcount != hash.count()) fatal("(1.5) fuuuuuuuuuuuu");
  checkHash();
  testIterator();

  printf("testing: deletion\n");
  for (int i = 0; i < MaxItems*8; ++i) {
    int v = rand()%MaxItems;
    //writeln('trying to delete ', v, '; its[v]=', its[v]);
    bool del = hash.del(v);
    //writeln('  del=', del);
    if (del) {
      if (its[v] < 0) fatal("(2.0) fuuuuuuuuuuuu");
      --xcount;
    } else {
      if (its[v] >= 0) fatal("(2.1) fuuuuuuuuuuuu");
    }
    its[v] = -1;
    if (xcount != hash.count()) fatal("(2.2) fuuuuuuuuuuuu");
    #ifdef EXCESSIVE_COMPACT
    hash.compact();
    if (xcount != hash.count()) fatal("(2.3) fuuuuuuuuuuuu");
    #endif
    #ifdef EXCESSIVE_CHECKS
    checkHash();
    #endif
    #ifdef EXCESSIVE_CHECKS_ITERATOR
    testIterator();
    #endif
    if (hash.count() == 0) break;
  }

  printf("testing: complete\n");
  checkHash();
  testIterator();

  return 0;
}
#endif
