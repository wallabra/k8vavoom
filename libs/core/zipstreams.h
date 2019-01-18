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
#ifndef VAVOOM_CORELIB_ZIPSTREAMS_HEADER
#define VAVOOM_CORELIB_ZIPSTREAMS_HEADER

#ifdef USE_INTERNAL_ZLIB
# include "../zlib/zlib.h"
#else
# include <zlib.h>
#endif


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

  mythread_mutex lock;
  VStream *srcStream;
  int stpos;
  int srccurpos;
  Bytef buffer[BUFFER_SIZE];
  z_stream zStream;
  bool initialised;
  vuint32 compressedSize;
  vuint32 uncompressedSize;
  int nextpos;
  int currpos;
  //bool zipArchive;
  Type strmType;
  vuint32 origCrc32;
  vuint32 currCrc32;
  bool doCrcCheck;
  bool forceRewind;
  VStr mFileName;
  bool doSeekToSrcStart;

private:
  void initialize ();

  void setError ();

  // just read, no `nextpos` advancement
  // returns number of bytes read, -1 on error, or 0 on EOF
  int readSomeBytes (void *buf, int len);

public:
  // doesn't own passed stream
  VZipStreamReader (VStream *ASrcStream, vuint32 ACompressedSize=UNKNOWN_SIZE, vuint32 AUncompressedSize=UNKNOWN_SIZE, Type atype=Type::ZLIB);
  VZipStreamReader (const VStr &fname, VStream *ASrcStream, vuint32 ACompressedSize=UNKNOWN_SIZE, vuint32 AUncompressedSize=UNKNOWN_SIZE, Type atype=Type::ZLIB);

  VZipStreamReader (bool useCurrSrcPos, VStream *ASrcStream, vuint32 ACompressedSize=UNKNOWN_SIZE, vuint32 AUncompressedSize=UNKNOWN_SIZE, Type atype=Type::ZLIB);
  VZipStreamReader (bool useCurrSrcPos, const VStr &fname, VStream *ASrcStream, vuint32 ACompressedSize=UNKNOWN_SIZE, vuint32 AUncompressedSize=UNKNOWN_SIZE, Type atype=Type::ZLIB);

  /*
  VZipStreamReader (VStream *ASrcStream, vuint32 AUncompressedSize=UNKNOWN_SIZE, Type atype=Type::ZLIB);
  VZipStreamReader (const VStr &strmName, VStream *ASrcStream, vuint32 AUncompressedSize=UNKNOWN_SIZE, Type atype=Type::ZLIB);
  VZipStreamReader (bool useCurrSrcPos, VStream *ASrcStream, vuint32 AUncompressedSize=UNKNOWN_SIZE, Type atype=Type::ZLIB);
  VZipStreamReader (const VStr &strmName, bool useCurrSrcPos, VStream *ASrcStream, vuint32 AUncompressedSize=UNKNOWN_SIZE, Type atype=Type::ZLIB);
  */

  virtual ~VZipStreamReader () override;

  virtual const VStr &GetName () const override;

  void setCrc (vuint32 acrc); // turns on CRC checking

  virtual void Serialise (void *, int) override;
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

  mythread_mutex lock;
  VStream *dstStream;
  Bytef buffer[BUFFER_SIZE];
  z_stream zStream;
  bool initialised;
  vuint32 currCrc32;
  bool doCrcCalc;

private:
  void setError ();

public:
  // doesn't own passed stream
  VZipStreamWriter (VStream *, int clevel=6, Type atype=Type::ZLIB);
  virtual ~VZipStreamWriter () override;
  void setRequireCrc ();
  vuint32 getCrc32 () const; // crc32 over uncompressed data (if enabled)
  virtual void Serialise (void *, int) override;
  virtual void Seek (int) override;
  virtual void Flush () override;
  virtual bool Close () override;
};


/*
// doesn't own srcstream
class VZipStreamReader : public VStream {
public:
  // stream type
  enum Type {
    ZLIB, // and gzip
    RAW,
  };

  enum { UNKNOWN_SIZE = 0xffffffffU }; // for uncompressed

private:
  enum { BUFFER_SIZE = 65536 };

  VStream *SrcStream;
  Bytef Buffer[BUFFER_SIZE];
  z_stream ZStream;
  bool Initialised;
  vuint32 UncompressedSize;
  int srcStartPos;
  int srcCurrPos;
  Type type;
  VStr StreamName;
  bool useInternalStreamName;
  mutable mythread_mutex lock;

private:
  void initialize ();
  void reinitialize ();

public:
  VZipStreamReader (VStream *ASrcStream, vuint32 AUncompressedSize=UNKNOWN_SIZE, Type atype=Type::ZLIB);
  VZipStreamReader (const VStr &strmName, VStream *ASrcStream, vuint32 AUncompressedSize=UNKNOWN_SIZE, Type atype=Type::ZLIB);
  VZipStreamReader (bool useCurrSrcPos, VStream *ASrcStream, vuint32 AUncompressedSize=UNKNOWN_SIZE, Type atype=Type::ZLIB);
  VZipStreamReader (const VStr &strmName, bool useCurrSrcPos, VStream *ASrcStream, vuint32 AUncompressedSize=UNKNOWN_SIZE, Type atype=Type::ZLIB);
  virtual ~VZipStreamReader () override;
  virtual const VStr &GetName () const override;
  virtual void Serialise (void*, int) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
};


// doesn't own dststream
class VZipStreamWriter : public VStream {
public:
  // stream type
  enum Type {
    ZLIB,
    RAW,
    GZIP,
  };
private:
  enum { BUFFER_SIZE = 65536 };

  VStream *DstStream;
  Bytef Buffer[BUFFER_SIZE];
  z_stream ZStream;
  bool Initialised;

public:
  VZipStreamWriter (VStream *, int clevel=6, Type atype=Type::ZLIB);

  virtual const VStr &GetName () const override;
  virtual void Serialise (void*, int) override;
  virtual void Seek (int) override;
  virtual void Flush () override;
  virtual bool Close () override;
};
*/


#endif
