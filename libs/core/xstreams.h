//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
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

// stream for reading in memory
class VMemoryStreamRO : public VStream {
protected:
  const vuint8 *Data;
  int Pos;
  int DataSize;
  bool FreeData; // free data with `Z_Free()` when this stream is destroyed?
  VStr StreamName;

public:
  VMemoryStreamRO (); // so we can declare it, and initialize later
  VMemoryStreamRO (const VStr &strmName, const void *adata, int adataSize, bool takeOwnership=false);
  VMemoryStreamRO (const VStr &strmName, VStream *strm); // from current position to stream end

  void Clear ();

  void Setup (const VStr &strmName, const void *adata, int adataSize, bool takeOwnership=false);
  void Setup (const VStr &strmName, VStream *strm); // from current position to stream end

  virtual void Serialise (void *Data, int Length) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;

  inline const vuint8 *GetPtr () const { return Data; }

  virtual const VStr &GetName () const override;
};


// stream for reading and writing in memory
class VMemoryStream : public VStream {
protected:
  TArray<vuint8> Array;
  int Pos;
  VStr StreamName;

public:
  // initialise empty writing stream
  VMemoryStream ();
  VMemoryStream (const VStr &strmName);
  // initialise reading streams
  VMemoryStream (const VStr &strmName, const void *, int, bool takeOwnership=false);
  VMemoryStream (const VStr &strmName, const TArray<vuint8> &);
  VMemoryStream (const VStr &strmName, VStream *strm); // from current position to stream end

  virtual void Serialise (void *Data, int Length) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;

  inline void BeginRead () { bLoading = true; }
  inline void BeginWrite () { bLoading = false; }
  inline TArray<vuint8> &GetArray () { return Array; }

  virtual const VStr &GetName () const override;
};


// similar to VMemoryStream, but uses reference to an external array
class VArrayStream : public VStream {
protected:
  TArray<vuint8> &Array;
  int Pos;
  VStr StreamName;

public:
  VArrayStream (const VStr &strmName, TArray<vuint8> &);

  virtual void Serialise (void *Data, int Length) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;

  inline void BeginRead () { bLoading = true; }
  inline void BeginWrite () { bLoading = false; }
  inline TArray<vuint8> &GetArray () { return Array; }

  virtual const VStr &GetName () const override;
};


// stream for reading and writing in memory
// this does allocation by 8KB pages, and you cannot get direct pointer
// the idea is that you will use this as writer, then switch it to reader,
// and pass it to zip stream or something
class VPagedMemoryStream : public VStream {
private:
  enum { FullPageSize = 8192 };
  enum { DataPerPage = FullPageSize-sizeof(vuint8 *) };
  // each page contains pointer to the next page, and `FullPageSize-sizeof(void*)` bytes of data
  vuint8 *first; // first page
  vuint8 *curr; // current page for reader, last page for writer
  int pos; // current position
  int size; // current size
  VStr StreamName;

public:
  // initialise empty writing stream
  VPagedMemoryStream (const VStr &strmName);

  virtual bool Close () override;
  virtual void Serialise (void *Data, int Length) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;

  inline void BeginRead () { bLoading = true; pos = 0; curr = first; }

  void CopyTo (VStream *strm);

  virtual const VStr &GetName () const override;
};


//**************************************************************************
//**
//**  MESSAGE IO FUNCTIONS
//**
//**    Handles byte ordering and avoids alignment errors
//**
//**************************************************************************

class VBitStreamWriter : public VStream {
protected:
  TArray<vuint8> Data;
  vint32 Max;
  vint32 Pos;

public:
  VBitStreamWriter (vint32);
  virtual void Serialise (void *Data, int Length) override;
  virtual void SerialiseBits (void *Data, int Length) override;
  virtual void SerialiseInt (vuint32 &Value/*, vuint32 Max*/) override;
  void WriteInt (vuint32/*, vuint32*/);
  inline vuint8 *GetData () { return Data.Ptr(); }
  inline int GetNumBits () const { return Pos; }
  inline int GetNumBytes () const { return (Pos+7)>>3; }

  inline void WriteBit (bool Bit) {
    if (Pos+1 > Max) {
      bError = true;
      return;
    }
    if (Bit) Data[Pos>>3] |= 1<<(Pos&7);
    ++Pos;
  }

  static inline int CalcIntBits (vuint32 Val) {
    int res = 1; // sign bit
    if (Val&0x80000000u) Val ^= 0xffffffffu;
    vuint32 mask = 0x0f;
    while (Val) {
      res += 5; // continute bit, 4 data bits
      Val &= ~mask;
      mask <<= 4;
    }
    return res+1; // and stop bit
  }
};


class VBitStreamReader : public VStream {
protected:
  TArray<vuint8> Data;
  vint32 Num;
  vint32 Pos;

public:
  VBitStreamReader (vuint8* = nullptr, vint32 = 0);
  void SetData (VBitStreamReader&, int);
  virtual void Serialise (void *Data, int Length) override;
  virtual void SerialiseBits (void *Data, int Length) override;
  virtual void SerialiseInt (vuint32 &Value/*, vuint32 Max*/) override;
  vuint32 ReadInt (/*vuint32*/);
  virtual bool AtEnd () override;
  inline vuint8 *GetData () { return Data.Ptr(); }
  inline int GetNumBits () const { return Num; }
  inline int GetNumBytes () const { return (Num+7)>>3; }
  inline int GetPosBits () const { return Pos; }

  inline bool ReadBit () {
    if (Pos+1 > Num) {
      bError = true;
      return false;
    }
    bool Ret = !!(Data[Pos>>3]&(1<<(Pos&7)));
    ++Pos;
    return Ret;
  }

  static inline int CalcIntBits (vuint32 n) { return VBitStreamWriter::CalcIntBits(n); }

  inline int GetPos () const { return Pos; }
  inline int GetNum () const { return Num; }
};


// ////////////////////////////////////////////////////////////////////////// //
// owns afl
class VStdFileStream : public VStream {
private:
  FILE *mFl;
  VStr mName;
  int size; // <0: not determined yet

private:
  void setError ();

public:
  VStdFileStream (FILE *afl, const VStr &aname=VStr(), bool asWriter=false);

  virtual const VStr &GetName () const override;
  virtual void Seek (int pos) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
  virtual void Serialise (void *buf, int len) override;
};


// ////////////////////////////////////////////////////////////////////////// //
// doesn't own srcstream by default
// does full stream proxing (i.e. forwards all virtual methods)
class VPartialStreamRO : public VStream {
private:
  mutable mythread_mutex lock;
  mutable mythread_mutex *lockptr;
  VStream *srcStream;
  int stpos;
  int srccurpos;
  int partlen;
  bool srcOwned;
  bool closed;
  VStr myname;

private:
  bool checkValidityCond (bool mustBeTrue);
  inline bool checkValidity () { return checkValidityCond(true); }

public:
  // doesn't own passed stream
  VPartialStreamRO (VStream *ASrcStream, int astpos, int apartlen=-1, bool aOwnSrc=false);
  VPartialStreamRO (const VStr &aname, VStream *ASrcStream, int astpos, int apartlen, mythread_mutex *alockptr);
  // will free source stream if necessary
  virtual ~VPartialStreamRO () override;

  // stream interface
  virtual const VStr &GetName () const override;
  virtual bool IsError () const override;
  virtual void Serialise (void *Data, int Length) override;
  virtual void SerialiseBits (void *Data, int Length) override;
  virtual void SerialiseInt (vuint32 &Value/*, vuint32 Max*/) override;

  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual void Flush () override;
  // won't free stream
  virtual bool Close () override;

  // interface functions for objects and classes streams
  virtual void io (VName &) override;
  virtual void io (VStr &) override;
  virtual void io (VObject *&) override;
  virtual void io (VMemberBase *&) override;
  virtual void io (VSerialisable *&) override;

  virtual void SerialiseStructPointer (void *&Ptr, VStruct *Struct) override;
};
