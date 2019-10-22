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
//**  the Free Software Foundation, version 3 of the License ONLY.
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
//**
//**  Dynamic array template.
//**
//**************************************************************************
//#define VAVOOM_CORELIB_ARRAY_MINIMAL_RESIZE

enum EArrayNew { E_ArrayNew };

inline void *operator new (size_t, void *ptr, EArrayNew, EArrayNew) { return ptr; }


template<class T> class TArray {
private:
  // 2d info is from `VScriptArray`
  int ArrNum; // if bit 31 is set, this is 1st dim of 2d array
  int ArrSize; // if bit 31 is set in `ArrNum`, this is 2nd dim of 2d array
  T *ArrData;

public:
  TArray () noexcept : ArrNum(0), ArrSize(0), ArrData(nullptr) {}
  TArray (ENoInit) noexcept {}
  TArray (const TArray<T> &other) noexcept : ArrNum(0), ArrSize(0), ArrData(nullptr) { *this = other; }

  ~TArray () noexcept { clear(); }

  inline int length1D () const noexcept { return (ArrNum >= 0 ? ArrNum : (ArrNum&0x7fffffff)*(ArrSize&0x7fffffff)); }

  // this is from `VScriptArray`
  inline bool Is2D () const noexcept { return (ArrNum < 0); }
  inline void Flatten () noexcept { if (Is2D()) { int oldlen = length1D(); ArrSize = ArrNum = oldlen; } }

  inline void Clear () noexcept { clear(); }

  inline void clear () noexcept {
    if (ArrData) {
      Flatten(); // just in case
      for (int i = 0; i < ArrNum; ++i) ArrData[i].~T();
      Z_Free(ArrData);
    }
    ArrData = nullptr;
    ArrNum = ArrSize = 0;
  }

  // don't free array itself
  inline void reset () noexcept {
    Flatten(); // just in case
    for (int f = 0; f < ArrNum; ++f) ArrData[f].~T();
    ArrNum = 0;
  }

  // don't free array itself
  inline void resetNoDtor () noexcept {
    Flatten(); // just in case
    ArrNum = 0;
  }

  inline int Num () const noexcept { return ArrNum; }
  inline int Length () const noexcept { return ArrNum; }
  inline int length () const noexcept { return ArrNum; }

  inline int NumAllocated () const noexcept { return ArrSize; }
  inline int numAllocated () const noexcept { return ArrSize; }
  inline int capacity () const noexcept { return ArrSize; }

  // don't do any sanity checks here, `ptr()` can be used even on empty arrays
  inline T *Ptr () noexcept { return ArrData; }
  inline const T *Ptr () const noexcept { return ArrData; }

  inline T *ptr () noexcept { return ArrData; }
  inline const T *ptr () const noexcept { return ArrData; }

  inline void SetPointerData (void *adata, int datalen) noexcept {
    vassert(datalen >= 0);
    if (datalen == 0) {
      if (ArrData && adata) {
        Flatten();
        vassert((uintptr_t)adata < (uintptr_t)ArrData || (uintptr_t)adata >= (uintptr_t)(ArrData+ArrSize));
      }
      clear();
      if (adata) Z_Free(adata);
    } else {
      vassert(adata);
      vassert(datalen%(int)sizeof(T) == 0);
      if (ArrData) {
        Flatten();
        vassert((uintptr_t)adata < (uintptr_t)ArrData || (uintptr_t)adata >= (uintptr_t)(ArrData+ArrSize));
        clear();
      }
      ArrData = (T *)adata;
      ArrNum = ArrSize = datalen/(int)sizeof(T);
    }
  }

  // this changes only capacity, length will not be increased (but can be decreased)
  void Resize (int NewSize) noexcept {
    vassert(NewSize >= 0);

    if (NewSize <= 0) { clear(); return; }

    Flatten(); // just in case
    if (NewSize == ArrSize) return;

    // free unused elements
    for (int i = NewSize; i < ArrNum; ++i) ArrData[i].~T();

    // realloc buffer
    ArrData = (T *)Z_Realloc(ArrData, NewSize*sizeof(T));

    // no need to init new data, allocator will do it later
    ArrSize = NewSize;
    if (ArrNum > NewSize) ArrNum = NewSize;
  }
  inline void resize (int NewSize) noexcept { Resize(NewSize); }

  void SetNum (int NewNum, bool bResize=true) noexcept {
    vassert(NewNum >= 0);
    Flatten(); // just in case
    if (bResize || NewNum > ArrSize) Resize(NewNum);
    vassert(ArrSize >= NewNum);
    if (ArrNum > NewNum) {
      // destroy freed elements
      for (int i = NewNum; i < ArrNum; ++i) ArrData[i].~T();
    } else if (ArrNum < NewNum) {
      // initialize new elements
      memset((void *)(ArrData+ArrNum), 0, (NewNum-ArrNum)*sizeof(T));
      for (int i = ArrNum; i < NewNum; ++i) new(&ArrData[i], E_ArrayNew, E_ArrayNew)T;
    }
    ArrNum = NewNum;
  }
  inline void setNum (int NewNum, bool bResize=true) noexcept { SetNum(NewNum, bResize); }
  inline void setLength (int NewNum, bool bResize=true) noexcept { SetNum(NewNum, bResize); }
  inline void SetLength (int NewNum, bool bResize=true) noexcept { SetNum(NewNum, bResize); }

  inline void SetNumWithReserve (int NewNum) noexcept {
    vassert(NewNum >= 0);
    if (NewNum > ArrSize) {
#ifdef VAVOOM_CORELIB_ARRAY_MINIMAL_RESIZE
      Resize(NewNum+64);
#else
      Resize(NewNum+NewNum*3/8+32);
#endif
    }
    SetNum(NewNum, false); // don't resize
  }
  inline void setLengthReserve (int NewNum) noexcept { SetNumWithReserve(NewNum); }

  inline void Condense () noexcept { Resize(ArrNum); }
  inline void condense () noexcept { Resize(ArrNum); }

  // this won't copy capacity (there is no reason to do it)
  TArray<T> &operator = (const TArray<T> &other) noexcept {
    if (&other == this) return *this; // oops
    vassert(!other.Is2D());
    clear();
    int newsz = other.ArrNum;
    if (newsz) {
      ArrNum = ArrSize = newsz;
      ArrData = (T *)Z_Malloc(newsz*sizeof(T));
      memset((void *)ArrData, 0, newsz*sizeof(T));
      for (int i = 0; i < newsz; ++i) {
        new(&ArrData[i], E_ArrayNew, E_ArrayNew)T;
        ArrData[i] = other.ArrData[i];
      }
    }
    return *this;
  }

  inline T &operator [] (int index) noexcept {
    vassert(index >= 0);
    vassert(index < ArrNum);
    return ArrData[index];
  }

  inline const T &operator [] (int index) const noexcept {
    vassert(index >= 0);
    vassert(index < ArrNum);
    return ArrData[index];
  }

  inline T &last () noexcept {
    vassert(!Is2D());
    vassert(ArrNum > 0);
    return ArrData[ArrNum-1];
  }

  inline const T &last () const noexcept {
    vassert(!Is2D());
    vassert(ArrNum > 0);
    return ArrData[ArrNum-1];
  }

  inline void drop () noexcept {
    vassert(!Is2D());
    if (ArrNum > 0) {
      --ArrNum;
      ArrData[ArrNum].~T();
    }
  }

  inline void Insert (int index, const T &item) noexcept {
    vassert(!Is2D());
    int oldlen = ArrNum;
    setLengthReserve(oldlen+1);
    for (int i = oldlen; i > index; --i) ArrData[i] = ArrData[i-1];
    ArrData[index] = item;
  }
  inline void insert (int index, const T &item) noexcept { Insert(index, item); }

  inline int Append (const T &item) noexcept {
    vassert(!Is2D());
    int oldlen = ArrNum;
    setLengthReserve(oldlen+1);
    ArrData[oldlen] = item;
    return oldlen;
  }
  inline int append (const T &item) noexcept { return Append(item); }

  inline T &Alloc () noexcept {
    vassert(!Is2D());
    int oldlen = ArrNum;
    setLengthReserve(oldlen+1);
    return ArrData[oldlen];
  }
  inline T &alloc () noexcept { return Alloc(); }

  inline void RemoveIndex (int index) noexcept {
    vassert(ArrData != nullptr);
    vassert(index >= 0);
    vassert(index < ArrNum);
    Flatten(); // just in case
    --ArrNum;
    for (int i = index; i < ArrNum; ++i) ArrData[i] = ArrData[i+1];
    ArrData[ArrNum].~T();
  }
  inline void removeAt (int index) noexcept { return RemoveIndex(index); }

  inline int Remove (const T &item) noexcept {
    Flatten(); // just in case
    int count = 0;
    for (int i = 0; i < ArrNum; ++i) {
      if (ArrData[i] == item) { ++count; RemoveIndex(i--); }
    }
    return count;
  }
  inline int remove (const T &item) noexcept { return Remove(item); }

  friend VStream &operator << (VStream &Strm, TArray<T> &Array) noexcept {
    vassert(!Array.Is2D());
    int NumElem = Array.Num();
    Strm << STRM_INDEX(NumElem);
    if (Strm.IsLoading()) Array.SetNum(NumElem);
    for (int i = 0; i < Array.Num(); ++i) Strm << Array[i];
    return Strm;
  }

public:
  // range iteration
  // WARNING! don't add/remove array elements in iterator loop!

  inline T *begin () noexcept { return (length1D() > 0 ? ArrData : nullptr); }
  inline const T *begin () const noexcept { return (length1D() > 0 ? ArrData : nullptr); }
  inline T *end () noexcept { return (length1D() > 0 ? ArrData+length1D() : nullptr); }
  inline const T *end () const noexcept { return (length1D() > 0 ? ArrData+length1D() : nullptr); }

  #define VARR_DEFINE_ITEMS_ITERATOR(xconst_)  \
    class xconst_##IndexIterator { \
    public: \
      xconst_ T *currvalue; \
      xconst_ T *endvalue; \
      int currindex; \
    public: \
      xconst_##IndexIterator (xconst_ TArray<T> *arr) noexcept { \
        if (arr->length1D() > 0) { \
          currvalue = arr->ArrData; \
          endvalue = currvalue+arr->length1D(); \
        } else { \
          currvalue = endvalue = nullptr; \
        } \
        currindex = 0; \
      } \
      xconst_##IndexIterator (const xconst_##IndexIterator &it) noexcept : currvalue(it.currvalue), endvalue(it.endvalue), currindex(it.currindex) {} \
      xconst_##IndexIterator (const xconst_##IndexIterator &it, bool asEnd) noexcept : currvalue(it.endvalue), endvalue(it.endvalue), currindex(it.currindex) {} \
      inline xconst_##IndexIterator begin () noexcept { return xconst_##IndexIterator(*this); } \
      inline xconst_##IndexIterator end () noexcept { return xconst_##IndexIterator(*this, true); } \
      inline bool operator == (const xconst_##IndexIterator &b) const noexcept { return (currvalue == b.currvalue); } \
      inline bool operator != (const xconst_##IndexIterator &b) const noexcept { return (currvalue != b.currvalue); } \
      inline xconst_##IndexIterator operator * () const noexcept { return xconst_##IndexIterator(*this); } /* required for iterator */ \
      inline void operator ++ () noexcept { ++currvalue; ++currindex; } /* this is enough for iterator */ \
      inline xconst_ T &value () noexcept { return *currvalue; } \
      /*inline const T &value () const noexcept { return *currvalue; }*/ \
      inline int index () const noexcept { return currindex; } \
    }; \
    inline xconst_##IndexIterator itemsIdx () xconst_ noexcept { return xconst_##IndexIterator(this); }

  VARR_DEFINE_ITEMS_ITERATOR()
  VARR_DEFINE_ITEMS_ITERATOR(const)
  #undef VARR_DEFINE_ITEMS_ITERATOR

  #define VARR_DEFINE_ITEMS_ITERATOR(xconst_)  \
    class xconst_##IndexIteratorRev { \
    public: \
      xconst_ TArray<T> *arr; \
      int currindex; \
    public: \
      xconst_##IndexIteratorRev (xconst_ TArray<T> *aarr) noexcept : arr(aarr) { currindex = (arr->length1D() > 0 ? arr->length1D()-1 : -1); } \
      xconst_##IndexIteratorRev (const xconst_##IndexIteratorRev &it) noexcept : arr(it.arr), currindex(it.currindex) {} \
      xconst_##IndexIteratorRev (const xconst_##IndexIteratorRev &it, bool asEnd) noexcept : arr(it.arr), currindex(-1) {} \
      inline xconst_##IndexIteratorRev begin () noexcept { return xconst_##IndexIteratorRev(*this); } \
      inline xconst_##IndexIteratorRev end () noexcept { return xconst_##IndexIteratorRev(*this, true); } \
      inline bool operator == (const xconst_##IndexIteratorRev &b) const noexcept { return (arr == b.arr && currindex == b.currindex); } \
      inline bool operator != (const xconst_##IndexIteratorRev &b) const noexcept { return (arr != b.arr || currindex != b.currindex); } \
      inline xconst_##IndexIteratorRev operator * () const noexcept { return xconst_##IndexIteratorRev(*this); } /* required for iterator */ \
      inline void operator ++ () noexcept { --currindex; } /* this is enough for iterator */ \
      inline xconst_ T &value () noexcept { return arr->ArrData[currindex]; } \
      /*inline const T &value () const noexcept { return arr->ArrData[currindex]; }*/ \
      inline int index () const noexcept { return currindex; } \
    }; \
    inline xconst_##IndexIteratorRev itemsIdxRev () xconst_ noexcept { return xconst_##IndexIteratorRev(this); }

  VARR_DEFINE_ITEMS_ITERATOR()
  VARR_DEFINE_ITEMS_ITERATOR(const)
  #undef VARR_DEFINE_ITEMS_ITERATOR
};
