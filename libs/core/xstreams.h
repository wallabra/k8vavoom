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
//**  Copyright (C) 2018-2020 Ketmar Dark
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
  VMemoryStreamRO (VStr strmName, const void *adata, int adataSize, bool takeOwnership=false);
  VMemoryStreamRO (VStr strmName, VStream *strm); // from current position to stream end

  virtual ~VMemoryStreamRO () override;

  void Clear ();

  void Setup (VStr strmName, const void *adata, int adataSize, bool takeOwnership=false);
  void Setup (VStr strmName, VStream *strm); // from current position to stream end

  virtual void Serialise (void *Data, int Length) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;

  inline const vuint8 *GetPtr () const { return Data; }

  virtual VStr GetName () const override;
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
  VMemoryStream (VStr strmName);
  // initialise reading streams
  VMemoryStream (VStr strmName, const void *, int, bool takeOwnership=false);
  VMemoryStream (VStr strmName, const TArray<vuint8> &);
  VMemoryStream (VStr strmName, VStream *strm); // from current position to stream end

  virtual ~VMemoryStream () override;

  virtual void Serialise (void *Data, int Length) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;

  inline void BeginRead () { bLoading = true; }
  inline void BeginWrite () { bLoading = false; }
  inline TArray<vuint8> &GetArray () { return Array; }

  virtual VStr GetName () const override;
};


// similar to VMemoryStream, but uses reference to an external array
class VArrayStream : public VStream {
protected:
  TArray<vuint8> &Array;
  int Pos;
  VStr StreamName;

public:
  VArrayStream (VStr strmName, TArray<vuint8> &);
  virtual ~VArrayStream () override;

  virtual void Serialise (void *Data, int Length) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;

  inline void BeginRead () { bLoading = true; }
  inline void BeginWrite () { bLoading = false; }
  inline TArray<vuint8> &GetArray () { return Array; }

  virtual VStr GetName () const override;
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
  VPagedMemoryStream (VStr strmName);
  virtual ~VPagedMemoryStream () override;

  virtual void Serialise (void *Data, int Length) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool Close () override;

  inline void BeginRead () { bLoading = true; pos = 0; curr = first; }
  inline void BeginWrite () { bLoading = true; pos = 0; curr = first; }

  inline void SwitchToReader () { bLoading = true; }
  inline void SwitchToWriter () { bLoading = false; }

  void CopyTo (VStream *strm);

  virtual VStr GetName () const override;
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
  bool bAllowExpand;

protected:
  bool Expand () noexcept;

  inline bool ReadBitInternal () noexcept {
    if (Pos >= Data.length()*8) {
      bError = true;
      return false;
    }
    bool Ret = !!(Data.ptr()[Pos>>3]&(1<<(Pos&7)));
    ++Pos;
    return Ret;
  }

public:
  VBitStreamWriter (vint32, bool allowExpand=false);
  virtual ~VBitStreamWriter () override;

  virtual void Serialise (void *Data, int Length) override;
  virtual void SerialiseBits (void *Data, int Length) override;
  virtual void SerialiseInt (vuint32 &Value/*, vuint32 Max*/) override;
  void WriteInt (vuint32/*, vuint32*/);
  inline vuint8 *GetData () noexcept { return Data.Ptr(); }
  inline int GetNumBits () const noexcept { return Pos; }
  inline int GetNumBytes () const noexcept { return (Pos+7)>>3; }

  inline void Clear () noexcept { bError = false; Pos = 0; }

  inline bool IsExpanded () const noexcept { return (Pos > Max); }

  void WriteBit (bool Bit) noexcept;
  /*
  inline void WriteBit (bool Bit) {
    if (bError) return;
    if (Pos+1 > / *Max* /Data.length()*8) {
      if (!Expand()) {
        bError = true;
        return;
      }
    }
    if (Bit) Data.ptr()[Pos>>3] |= 1<<(Pos&7);
    ++Pos;
  }
  */

  static inline int CalcIntBits (vuint32 Val) noexcept {
    int res = 1; // sign bit
    if (Val&0x80000000u) Val ^= 0xffffffffu;
    //vuint32 mask = 0x0f;
    while (Val) {
      res += 5; // continute bit, 4 data bits
      //Val &= ~mask;
      //mask <<= 4;
      Val >>= 4;
    }
    return res+1; // and stop bit
  }

  inline int GetPos () const noexcept { return Pos; }
  inline int GetNum () const noexcept { return Max; }
};


class VBitStreamReader : public VStream {
protected:
  TArray<vuint8> Data;
  vint32 Num;
  vint32 Pos;

public:
  VBitStreamReader (vuint8* = nullptr, vint32 = 0);
  virtual ~VBitStreamReader () override;

  void SetData (VBitStreamReader&, int) noexcept;
  virtual void Serialise (void *Data, int Length) override;
  virtual void SerialiseBits (void *Data, int Length) override;
  virtual void SerialiseInt (vuint32 &Value/*, vuint32 Max*/) override;
  vuint32 ReadInt (/*vuint32*/);
  virtual bool AtEnd () override;
  inline vuint8 *GetData () noexcept { return Data.Ptr(); }
  inline int GetNumBits () const noexcept { return Num; }
  inline int GetNumBytes () const noexcept { return (Num+7)>>3; }
  inline int GetPosBits () const noexcept { return Pos; }

  inline bool ReadBit () noexcept {
    if (Pos+1 > Num) {
      bError = true;
      return false;
    }
    bool Ret = !!(Data.ptr()[Pos>>3]&(1<<(Pos&7)));
    ++Pos;
    return Ret;
  }

  static inline int CalcIntBits (vuint32 n) noexcept { return VBitStreamWriter::CalcIntBits(n); }

  inline int GetPos () const noexcept { return Pos; }
  inline int GetNum () const noexcept { return Num; }
};


// ////////////////////////////////////////////////////////////////////////// //
// owns afl
class VStdFileStreamBase : public VStream {
private:
  FILE *mFl;
  VStr mName;
  int size; // <0: not determined yet

private:
  void setError ();

public:
  VStdFileStreamBase (FILE *afl, VStr aname, bool asWriter);
  virtual ~VStdFileStreamBase () override;

  virtual VStr GetName () const override;
  virtual void Seek (int pos) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
  virtual void Serialise (void *buf, int len) override;
};

// owns afl
class VStdFileStreamRead : public VStdFileStreamBase {
public:
  inline VStdFileStreamRead (FILE *afl, VStr aname=VStr()) : VStdFileStreamBase(afl, aname, false) {}
};

// owns afl
class VStdFileStreamWrite : public VStdFileStreamBase {
public:
  inline VStdFileStreamWrite (FILE *afl, VStr aname=VStr()) : VStdFileStreamBase(afl, aname, true) {}
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
  bool checkValidityCond (bool mustBeTrue) noexcept;
  inline bool checkValidity () noexcept { return checkValidityCond(true); }

public:
  // doesn't own passed stream
  VPartialStreamRO (VStream *ASrcStream, int astpos, int apartlen=-1, bool aOwnSrc=false);
  VPartialStreamRO (VStr aname, VStream *ASrcStream, int astpos, int apartlen, mythread_mutex *alockptr);
  // will free source stream if necessary
  virtual ~VPartialStreamRO () override;

  // stream interface
  virtual VStr GetName () const override;
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
