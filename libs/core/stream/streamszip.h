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
#ifndef VAVOOM_CORELIB_ZIPSTREAMS_HEADER
#define VAVOOM_CORELIB_ZIPSTREAMS_HEADER


// this will cache the whole stream on 2nd back-seek
class VZipStreamReader : public VStream {
public:
  // stream type
  enum Type {
    ZLIB, // and gzip
    RAW,
  };

  enum { UNKNOWN_SIZE = 0xffffffffU }; // for uncompressed

private:
  enum { BUFFER_SIZE = 16384 };

  VStream *srcStream;
  int stpos;
  int srccurpos;
  mythread_mutex *rdlock; // can be `nullptr`
  vuint8 buffer[BUFFER_SIZE];
  mz_stream zStream;
  bool initialised;
  vuint32 compressedSize;
  vuint32 uncompressedSize;
  int nextpos;
  int currpos;
  Type strmType;
  vuint32 origCrc32;
  vuint32 currCrc32;
  bool doCrcCheck;
  bool forceRewind;
  VStr mFileName;
  bool doSeekToSrcStart;
  // on second back-seek, read the whole data into this buffer, and use it
  vuint8 *wholeBuf;
  vint32 wholeSize; // this is abused as back-seek counter: -2 means none, -1 means "one issued"

private:
  void initialize ();

  // returns 0 if no more data, -1 on error, 1 if something was read
  int fillPackedBuffer (); // this locks; also, calls `SetError()` if necessary

  bool resetZStream (); // this locks; also, calls `SetError()` if necessary
  void deinitZStream ();

  // just read, no `nextpos` advancement
  // returns number of bytes read, -1 on error, or 0 on EOF
  int readSomeBytes (void *buf, int len);

  void cacheAllData ();

public:
  VV_DISABLE_COPY(VZipStreamReader)

  // doesn't own passed stream
  VZipStreamReader (VStream *ASrcStream, vuint32 ACompressedSize=UNKNOWN_SIZE, vuint32 AUncompressedSize=UNKNOWN_SIZE, Type atype=Type::ZLIB);
  VZipStreamReader (VStr fname, VStream *ASrcStream, vuint32 ACompressedSize=UNKNOWN_SIZE, vuint32 AUncompressedSize=UNKNOWN_SIZE, Type atype=Type::ZLIB);

  // doesn't own passed stream
  VZipStreamReader (bool useCurrSrcPos, VStream *ASrcStream, vuint32 ACompressedSize=UNKNOWN_SIZE, vuint32 AUncompressedSize=UNKNOWN_SIZE, Type atype=Type::ZLIB);
  VZipStreamReader (bool useCurrSrcPos, VStr fname, VStream *ASrcStream, vuint32 ACompressedSize=UNKNOWN_SIZE, vuint32 AUncompressedSize=UNKNOWN_SIZE, Type atype=Type::ZLIB);

  // lock should not be held for the following ctors

  // doesn't own passed stream
  VZipStreamReader (mythread_mutex *ardlock, VStream *ASrcStream, vuint32 ACompressedSize=UNKNOWN_SIZE, vuint32 AUncompressedSize=UNKNOWN_SIZE, Type atype=Type::ZLIB);
  VZipStreamReader (mythread_mutex *ardlock, VStr fname, VStream *ASrcStream, vuint32 ACompressedSize=UNKNOWN_SIZE, vuint32 AUncompressedSize=UNKNOWN_SIZE, Type atype=Type::ZLIB);

  // doesn't own passed stream
  VZipStreamReader (mythread_mutex *ardlock, bool useCurrSrcPos, VStream *ASrcStream, vuint32 ACompressedSize=UNKNOWN_SIZE, vuint32 AUncompressedSize=UNKNOWN_SIZE, Type atype=Type::ZLIB);
  VZipStreamReader (mythread_mutex *ardlock, bool useCurrSrcPos, VStr fname, VStream *ASrcStream, vuint32 ACompressedSize=UNKNOWN_SIZE, vuint32 AUncompressedSize=UNKNOWN_SIZE, Type atype=Type::ZLIB);

  virtual ~VZipStreamReader () override;

  void setCrc (vuint32 acrc); // turns on CRC checking

  virtual VStr GetName () const override;
  virtual void SetError () override;
  virtual void Serialise (void *Data, int Length) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
};


class VZipStreamWriter : public VStream {
public:
  // stream type
  enum Type {
    ZLIB,
    RAW,
    GZIP,
  };
private:
  enum { BUFFER_SIZE = 16384 };

  VStream *dstStream;
  vuint8 buffer[BUFFER_SIZE];
  mz_stream zStream;
  bool initialised;
  vuint32 currCrc32;
  bool doCrcCalc;

public:
  VV_DISABLE_COPY(VZipStreamWriter)

  // doesn't own passed stream
  VZipStreamWriter (VStream *, int clevel=6, Type atype=Type::ZLIB);
  virtual ~VZipStreamWriter () override;

  void setRequireCrc ();
  vuint32 getCrc32 () const; // crc32 over uncompressed data (if enabled)

  virtual void SetError () override;
  virtual void Serialise (void *Data, int Length) override;
  virtual void Seek (int) override;
  virtual void Flush () override;
  virtual bool Close () override;
};


#endif
