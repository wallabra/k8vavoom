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
//**  Dynamic array template.
//**
//**************************************************************************

enum EArrayNew { E_ArrayNew };

inline void *operator new (size_t, void *Ptr, EArrayNew, EArrayNew) { return Ptr; }


template<class T> class TArray {
private:
  // 2d info is from `VScriptArray`
  int ArrNum; // if bit 31 is set, this is 1st dim of 2d array
  int ArrSize; // if bit 31 is set in `ArrNum`, this is 2nd dim of 2d array
  T *ArrData;

public:
  TArray () : ArrNum(0), ArrSize(0), ArrData(nullptr) {}
  TArray (ENoInit) {}
  TArray (const TArray<T> &Other) : ArrNum(0), ArrSize(0), ArrData(nullptr) { *this = Other; }

  ~TArray () { clear(); }

  inline int length1D () const { return (ArrNum >= 0 ? ArrNum : (ArrNum&0x7fffffff)*(ArrSize&0x7fffffff)); }

  // this is from `VScriptArray`
  inline bool Is2D () const { return (ArrNum < 0); }
  inline void Flatten () { if (Is2D()) { int oldlen = length1D(); ArrSize = ArrNum = oldlen; } }

  inline void Clear () { clear(); }

  inline void clear () {
    if (ArrData) {
      Flatten(); // just in case
      for (int i = 0; i < ArrSize; ++i) ArrData[i].~T();
      Z_Free(ArrData);
    }
    ArrData = nullptr;
    ArrNum = ArrSize = 0;
  }

  // don't free array data
  inline void reset () {
    Flatten(); // just in case
    for (int f = 0; f < ArrNum; ++f) ArrData[f].~T();
    ArrNum = 0;
  }

  inline int Num () const { return ArrNum; }
  inline int Length () const { return ArrNum; }
  inline int length () const { return ArrNum; }

  inline int NumAllocated () const { return ArrSize; }
  inline int numAllocated () const { return ArrSize; }

  inline T *Ptr () { return ArrData; }
  inline const T *Ptr () const { return ArrData; }

  inline T *ptr () { return ArrData; }
  inline const T *ptr () const { return ArrData; }

  void Resize (int NewSize) {
    check(NewSize >= 0);

    if (NewSize <= 0) { clear(); return; }

    Flatten(); // just in case
    if (NewSize == ArrSize) return;

    //k8: ok, it was fubared anyway, so we'll go with realloc
    if (NewSize < ArrSize) {
      // free unused elements
      for (int i = NewSize; i < ArrSize; ++i) ArrData[i].~T();
    }
    // realloc buffer
    T *newbuf = (T *)Z_Realloc(ArrData, NewSize*sizeof(T));
    if (!newbuf) { fprintf(stderr, "FATAL: out of memory!\n"); abort(); }
    ArrData = newbuf;
    // init new data
    if (NewSize > ArrSize) {
      memset((void *)(ArrData+ArrSize), 0, (NewSize-ArrSize)*sizeof(T));
      for (int i = ArrSize; i < NewSize; ++i) new(&ArrData[i], E_ArrayNew, E_ArrayNew)T;
    }

    ArrSize = NewSize;
    if (ArrNum > NewSize) ArrNum = NewSize;

    /*
    T *OldData = ArrData;
    int OldSize = ArrSize;
    ArrSize = NewSize;
    if (ArrNum > NewSize) ArrNum = NewSize;

    ArrData = (T *)Z_Malloc(ArrSize*sizeof(T));
    memset((void *)ArrData, 0, ArrSize*sizeof(T));
    for (int i = 0; i < ArrSize; ++i) new(&ArrData[i], E_ArrayNew, E_ArrayNew)T;
    for (int i = 0; i < ArrNum; ++i) ArrData[i] = OldData[i];

    if (OldData) {
      for (int i = 0; i < OldSize; ++i) OldData[i].~T();
      Z_Free(OldData);
    }
    */
  }
  inline void resize (int NewSize) { Resize(NewSize); }

  void SetNum (int NewNum, bool bResize=true) {
    check(NewNum >= 0);
    if (bResize || NewNum > ArrSize) Resize(NewNum);
    ArrNum = NewNum;
  }
  inline void setNum (int NewNum, bool bResize=true) { SetNum(NewNum, bResize); }
  inline void setLength (int NewNum, bool bResize=true) { SetNum(NewNum, bResize); }
  inline void SetLength (int NewNum, bool bResize=true) { SetNum(NewNum, bResize); }

  void SetNumWithReserve (int NewNum) {
    check(NewNum >= 0);
    if (NewNum > ArrSize) Resize(NewNum+NewNum*3/8+32);
    ArrNum = NewNum;
  }
  inline void setLengthReserve (int NewNum) { SetNumWithReserve(NewNum); }

  inline void Condense () { Resize(ArrNum); }
  inline void condense () { Resize(ArrNum); }

  TArray<T> &operator = (const TArray<T> &Other) {
    clear();
    ArrNum = Other.ArrNum;
    ArrSize = Other.ArrSize;
    if (ArrSize) {
      ArrData = (T *)Z_Malloc(ArrSize*sizeof(T));
      memset((void *)ArrData, 0, ArrSize*sizeof(T));
      for (int i = 0; i < ArrSize; ++i) new(&ArrData[i], E_ArrayNew, E_ArrayNew)T;
      for (int i = 0; i < ArrNum; ++i) ArrData[i] = Other.ArrData[i];
    }
    return *this;
  }

  inline T &operator [] (int Index) {
    check(Index >= 0);
    check(Index < ArrNum);
    return ArrData[Index];
  }

  inline const T &operator [] (int Index) const {
    check(Index >= 0);
    check(Index < ArrNum);
    return ArrData[Index];
  }

  void Insert (int Index, const T &Item) {
    if (ArrNum == ArrSize) Resize(ArrSize+ArrSize*3/8+32);
    ++ArrNum;
    for (int i = ArrNum-1; i > Index; --i) ArrData[i] = ArrData[i-1];
    ArrData[Index] = Item;
  }
  void insert (int Index, const T &Item) { Insert(Index, Item); }

  inline int Append (const T &Item) {
    if (ArrNum == ArrSize) Resize(ArrSize+ArrSize*3/8+32);
    ArrData[ArrNum] = Item;
    ++ArrNum;
    return ArrNum-1;
  }

  inline int append (const T &Item) { return Append(Item); }

  inline T &Alloc () {
    if (ArrNum == ArrSize) Resize(ArrSize+ArrSize*3/8+32);
    return ArrData[ArrNum++];
  }

  inline T &alloc () {
    if (ArrNum == ArrSize) Resize(ArrSize+ArrSize*3/8+32);
    return ArrData[ArrNum++];
  }

  bool RemoveIndex (int Index) {
    check(ArrData != nullptr);
    check(Index >= 0);
    check(Index < ArrNum);
    if (Index < 0 || Index >= ArrNum) return false;
    --ArrNum;
    for (int i = Index; i < ArrNum; ++i) ArrData[i] = ArrData[i+1];
    return true;
  }

  bool removeAt (int Index) { return RemoveIndex(Index); }

  inline void Remove (const T &Item) {
    for (int i = 0; i < ArrNum; ++i) {
      if (ArrData[i] == Item) RemoveIndex(i--);
    }
  }

  inline void remove (const T &Item) { Remove(Item); }

  friend VStream &operator << (VStream &Strm, TArray<T> &Array) {
    int NumElem = Array.Num();
    Strm << STRM_INDEX(NumElem);
    if (Strm.IsLoading()) Array.SetNum(NumElem);
    for (int i = 0; i < Array.Num(); ++i) Strm << Array[i];
    return Strm;
  }
};
