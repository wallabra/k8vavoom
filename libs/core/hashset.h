//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
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
#ifndef VAVOOM_CORE_LIB_HASH
#define VAVOOM_CORE_LIB_HASH


#define TSTRSET_HASH  djbHashBuf

//k8 TODO: rewrite it!
/*template<class T>*/ class TStrSet {
private:
  struct BucketItem {
    VStr key;
    bool value;
    vuint32 hash;
  };

  BucketItem *mData;
  vuint32 mDataSize; // in items
  vuint32 mCount;

public:
  TStrSet () : mData(nullptr), mDataSize(0), mCount(0) {}
  ~TStrSet () { clear(); }

  void clear () {
    /*
    for (unsigned int n = 0; n < mDataSize; ++n) {
      if (mData[n]) delete mData[n]->iptr;
    }
    */
    delete[] mData;
    mData = nullptr;
    mDataSize = mCount = 0;
  }

  vuint32 count () const { return mCount; }

  // return `true` if replaced
  bool put (const VStr &key, bool value=true) {
    if (mDataSize == 0) {
      mDataSize = 512;
      mData = new BucketItem[mDataSize];
      for (vuint32 n = 0; n < mDataSize; ++n) {
        mData[n].key = VStr("");
        mData[n].value = false;
        mData[n].hash = 0;
      }
    }
    vuint32 hash = TSTRSET_HASH(*key, key.Length());
    vuint32 bnum = hash%mDataSize;
    // replace, if it is already there
    for (vuint32 n = mDataSize; n > 0; --n) {
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
      vuint32 newsz = mDataSize*2;
      BucketItem *newarr = new BucketItem[newsz];
      for (vuint32 n = 0; n < newsz; ++n) {
        newarr[n].key = VStr("");
        newarr[n].value = false;
        newarr[n].hash = 0;
      }
      // put items in new hash
      vuint32 oldsz = mDataSize;
      BucketItem *oldarr = mData;
      mData = newarr;
      mDataSize = newsz;
      mCount = 0;
      for (vuint32 n = 0; n < oldsz; ++n) {
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

  bool has (const VStr &key) {
    if (!mDataSize) return false;
    vuint32 hash = TSTRSET_HASH(*key, key.Length());
    vuint32 bnum = hash%mDataSize;
    for (vuint32 n = mDataSize; n > 0; --n) {
      if (!mData[bnum].hash) return false;
      if (mData[bnum].hash == hash && mData[bnum].key == key) return true;
      ++bnum;
    }
    return false;
  }

  bool get (const VStr &key, bool defval=false) {
    if (!mDataSize) return defval;
    vuint32 hash = TSTRSET_HASH(*key, key.Length());
    vuint32 bnum = hash%mDataSize;
    for (vuint32 n = mDataSize; n > 0; --n) {
      if (!mData[bnum].hash) return defval;
      if (mData[bnum].hash == hash && mData[bnum].key == key) return mData[bnum].value;
      ++bnum;
    }
    return defval;
  }
};


#endif
