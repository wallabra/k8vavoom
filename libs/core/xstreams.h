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

  virtual void Serialise (void*, int) override;
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

  virtual void Serialise (void*, int) override;
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

  virtual void Serialise (void*, int) override;
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
  enum { PAGE_SIZE = 8192 };
  enum { DATA_BYTES = PAGE_SIZE-sizeof(vuint8 *) };
  // each page contains pointer to the next page, and `PAGE_SIZE-sizeof(void*)` bytes of data
  vuint8 *first; // first page
  vuint8 *curr; // current page for reader, last page for writer
  int pos; // current position
  int size; // current size
  VStr StreamName;

public:
  // initialise empty writing stream
  VPagedMemoryStream (const VStr &strmName);

  virtual bool Close () override;
  virtual void Serialise (void*, int) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;

  inline void BeginRead () { bLoading = true; pos = 0; curr = first; }

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
  virtual void Serialise (void*, int) override;
  virtual void SerialiseBits (void*, int) override;
  virtual void SerialiseInt (vuint32&, vuint32) override;
  void WriteBit (bool);
  void WriteInt (vuint32, vuint32);
  inline vuint8 *GetData () { return Data.Ptr(); }
  inline int GetNumBits () const { return Pos; }
  inline int GetNumBytes () const { return (Pos+7)>>3; }
};


class VBitStreamReader : public VStream {
protected:
  TArray<vuint8> Data;
  vint32 Num;
  vint32 Pos;

public:
  VBitStreamReader (vuint8* = nullptr, vint32 = 0);
  void SetData (VBitStreamReader&, int);
  virtual void Serialise (void*, int) override;
  virtual void SerialiseBits (void*, int) override;
  virtual void SerialiseInt (vuint32 &Value, vuint32 Max) override;
  bool ReadBit ();
  vuint32 ReadInt (vuint32);
  virtual bool AtEnd () override;
  inline vuint8 *GetData () { return Data.Ptr(); }
  inline int GetNumBits () const { return Num; }
  inline int GetNumBytes () const { return (Num+7)>>3; }
  inline int GetPosBits () const { return Pos; }
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
class VPartialStreamRO : public VStream {
private:
  mutable mythread_mutex lock;
  VStream *srcStream;
  int stpos;
  int srccurpos;
  int partlen;
  bool srcOwned;

private:
  void setError ();

public:
  // doesn't own passed stream
  VPartialStreamRO (VStream *ASrcStream, int astpos, int apartlen=-1, bool aOwnSrc=false);
  virtual ~VPartialStreamRO () override;

  virtual const VStr &GetName () const override;
  virtual void Serialise (void *, int) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
};
