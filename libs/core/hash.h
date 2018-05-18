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
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
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
//**  Hash template.
//**
//**************************************************************************
#ifndef VAVOOM_CORE_LIB_HASH
#define VAVOOM_CORE_LIB_HASH


static inline unsigned int fnvHashBuf (const void *buf, unsigned int len) {
  // fnv-1a: http://www.isthe.com/chongo/tech/comp/fnv/
  unsigned int hash = 2166136261U; // fnv offset basis
  const unsigned char *s = (const unsigned char *)buf;
  while (len-- > 0) {
    hash ^= *s++;
    hash *= 16777619U; // 32-bit fnv prime
  }
  return (hash ? hash : 1); // this is unlikely, but...
}


//k8 TODO: rewrite it!
/*template<class T>*/ class TStrSet {
private:
  struct BucketItem {
    VStr key;
    bool value;
    unsigned int hash;
  };

  BucketItem* mData;
  unsigned int mDataSize; // in items
  unsigned int mCount;

public:
  TStrSet () : mData(NULL), mDataSize(0), mCount(0) {}
  ~TStrSet () { clear(); }

  void clear () {
    /*
    for (unsigned int n = 0; n < mDataSize; ++n) {
      if (mData[n]) delete mData[n]->iptr;
    }
    */
    delete[] mData;
    mData = NULL;
    mDataSize = mCount = 0;
  }

  unsigned int count () const { return mCount; }

  // return `true` if replaced
  bool put (const VStr& key, bool value=true) {
    if (mDataSize == 0) {
      mDataSize = 512;
      mData = new BucketItem[mDataSize];
      for (unsigned int n = 0; n < mDataSize; ++n) {
        mData[n].key = VStr("");
        mData[n].value = false;
        mData[n].hash = 0;
      }
    }
    unsigned int hash = fnvHashBuf(*key, key.Length());
    unsigned int bnum = hash%mDataSize;
    // replace, if it is already there
    for (unsigned int n = mDataSize; n > 0; --n) {
      if (!mData[bnum].hash) break;
      if (mData[bnum].hash == hash && mData[bnum].key == key) {
        mData[bnum].value = value;
        return true;
      }
      ++bnum;
    }
    // not found; check if we need to grow data array
    if (mDataSize-mCount < mDataSize/3) {
      // grow it
      unsigned int newsz = mDataSize*2;
      BucketItem* newarr = new BucketItem[newsz];
      for (unsigned int n = 0; n < newsz; ++n) {
        newarr[n].key = VStr("");
        newarr[n].value = false;
        newarr[n].hash = 0;
      }
      // put items in new hash
      unsigned int oldsz = mDataSize;
      BucketItem* oldarr = mData;
      mData = newarr;
      mDataSize = newsz;
      mCount = 0;
      for (unsigned int n = 0; n < oldsz; ++n) {
        if (oldarr[n].hash) put(oldarr[n].key, oldarr[n].value);
      }
      delete[] oldarr;
      return put(key, value);
    } else {
      // insert new item into hash
      mData[bnum].key = key;
      mData[bnum].value = value;
      mData[bnum].hash = hash;
      ++mCount;
      return false;
    }
  }

  bool has (const VStr& key) {
    unsigned int hash = fnvHashBuf(*key, key.Length());
    unsigned int bnum = hash%mDataSize;
    for (unsigned int n = mDataSize; n > 0; --n) {
      if (!mData[bnum].hash) return false;
      if (mData[bnum].hash == hash && mData[bnum].key == key) return true;
      ++bnum;
    }
    return false;
  }

  bool get (const VStr& key, bool defval=false) {
    unsigned int hash = fnvHashBuf(*key, key.Length());
    unsigned int bnum = hash%mDataSize;
    for (unsigned int n = mDataSize; n > 0; --n) {
      if (!mData[bnum].hash) return defval;
      if (mData[bnum].hash == hash && mData[bnum].key == key) return mData[bnum].value;
      ++bnum;
    }
    return defval;
  }
};


#endif
