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
#include "core.h"

#if 1

//==========================================================================
//
//  VZipStreamReader::VZipStreamReader
//
//==========================================================================
VZipStreamReader::VZipStreamReader (VStream *ASrcStream, vuint32 ACompressedSize, vuint32 AUncompressedSize, Type atype)
  : VStream()
  , srcStream(ASrcStream)
  , initialised(false)
  , compressedSize(ACompressedSize)
  , uncompressedSize(AUncompressedSize)
  , nextpos(0)
  , currpos(0)
  , strmType(atype)
  , origCrc32(0)
  , currCrc32(0)
  , doCrcCheck(false)
  , forceRewind(false)
  , mFileName(ASrcStream ? ASrcStream->GetName() : VStr::EmptyString)
  , doSeekToSrcStart(true)
{
  mythread_mutex_init(&lock);
  initialize();
}


//==========================================================================
//
//  VZipStreamReader::VZipStreamReader
//
//==========================================================================
VZipStreamReader::VZipStreamReader (const VStr &fname, VStream *ASrcStream, vuint32 ACompressedSize, vuint32 AUncompressedSize, Type atype)
  : VStream()
  , srcStream(ASrcStream)
  , initialised(false)
  , compressedSize(ACompressedSize)
  , uncompressedSize(AUncompressedSize)
  , nextpos(0)
  , currpos(0)
  , strmType(atype)
  , origCrc32(0)
  , currCrc32(0)
  , doCrcCheck(false)
  , forceRewind(false)
  , mFileName(fname)
  , doSeekToSrcStart(true)
{
  mythread_mutex_init(&lock);
  check(ASrcStream);
  initialize();
}


//==========================================================================
//
//  VZipStreamReader::VZipStreamReader
//
//==========================================================================
VZipStreamReader::VZipStreamReader (bool useCurrSrcPos, VStream *ASrcStream, vuint32 ACompressedSize, vuint32 AUncompressedSize, Type atype)
  : VStream()
  , srcStream(ASrcStream)
  , initialised(false)
  , compressedSize(ACompressedSize)
  , uncompressedSize(AUncompressedSize)
  , nextpos(0)
  , currpos(0)
  , strmType(atype)
  , origCrc32(0)
  , currCrc32(0)
  , doCrcCheck(false)
  , forceRewind(false)
  , mFileName(ASrcStream ? ASrcStream->GetName() : VStr::EmptyString)
  , doSeekToSrcStart(!useCurrSrcPos)
{
  mythread_mutex_init(&lock);
  check(ASrcStream);
  initialize();
}


//==========================================================================
//
//  VZipStreamReader::VZipStreamReader
//
//==========================================================================
VZipStreamReader::VZipStreamReader (bool useCurrSrcPos, const VStr &fname, VStream *ASrcStream, vuint32 ACompressedSize, vuint32 AUncompressedSize, Type atype)
  : VStream()
  , srcStream(ASrcStream)
  , initialised(false)
  , compressedSize(ACompressedSize)
  , uncompressedSize(AUncompressedSize)
  , nextpos(0)
  , currpos(0)
  , strmType(atype)
  , origCrc32(0)
  , currCrc32(0)
  , doCrcCheck(false)
  , forceRewind(false)
  , mFileName(fname)
  , doSeekToSrcStart(!useCurrSrcPos)
{
  mythread_mutex_init(&lock);
  check(ASrcStream);
  initialize();
}


//==========================================================================
//
//  VZipStreamReader::~VZipStreamReader
//
//==========================================================================
VZipStreamReader::~VZipStreamReader () {
  Close();
  mythread_mutex_destroy(&lock);
}


//==========================================================================
//
//  void
//
//==========================================================================
void VZipStreamReader::initialize () {
  bLoading = true;

  // initialise zip stream structure
  memset(&zStream, 0, sizeof(zStream));
  /*
  zStream.total_out = 0;
  zStream.zalloc = (alloc_func)0;
  zStream.zfree = (free_func)0;
  zStream.opaque = (voidpf)0;
  */

  if (srcStream) {
    MyThreadLocker locker(&lock);
    // read in some initial data
    if (doSeekToSrcStart) {
      srcStream->Seek(0);
      stpos = 0;
    } else {
      stpos = srcStream->Tell();
    }
    if (compressedSize == UNKNOWN_SIZE) compressedSize = (vuint32)(srcStream->TotalSize()-stpos);
    vint32 bytesToRead = BUFFER_SIZE;
    if (bytesToRead > (int)compressedSize) bytesToRead = (int)compressedSize;
    srcStream->Seek(stpos);
    srcStream->Serialise(buffer, bytesToRead);
    if (srcStream->IsError()) {
      GLog.WriteLine(NAME_Error, "%s", "Error reading source inflated stream");
      setError();
      return;
    }
    srccurpos = stpos+bytesToRead;
    zStream.next_in = buffer;
    zStream.avail_in = bytesToRead;
    // open zip stream
    //int err = (zipArchive ? inflateInit2(&zStream, -MAX_WBITS) : inflateInit(&zStream));
    int err = (strmType == Type::RAW ? inflateInit2(&zStream, -MAX_WBITS) : inflateInit2(&zStream, MAX_WBITS+32)); // allow gzip
    if (err != Z_OK) {
      GLog.WriteLine(NAME_Error, "Error initializing inflate (%d)", err);
      setError();
      return;
    }
    initialised = true;
  } else {
    bError = true;
  }
}


//==========================================================================
//
//  VZipStreamReader::GetName
//
//==========================================================================
const VStr &VZipStreamReader::GetName () const {
  return mFileName;
}


//==========================================================================
//
//  VZipStreamReader::setCrc
//
//  turns on CRC checking
//
//==========================================================================
void VZipStreamReader::setCrc (vuint32 acrc) {
  if (doCrcCheck && origCrc32 == acrc) return;
  origCrc32 = acrc;
  doCrcCheck = true;
  currCrc32 = 0;
  forceRewind = true;
}


//==========================================================================
//
//  VZipStreamReader::Close
//
//==========================================================================
bool VZipStreamReader::Close () {
  if (initialised) { inflateEnd(&zStream); initialised = false; }
  //if (srcStream) { delete srcStream; srcStream = nullptr; }
  srcStream = nullptr;
  return !bError;
}


//==========================================================================
//
//  VZipStreamReader::setError
//
//==========================================================================
void VZipStreamReader::setError () {
  if (initialised) { inflateEnd(&zStream); initialised = false; }
  //if (srcStream) { delete srcStream; srcStream = nullptr; }
  srcStream = nullptr;
  bError = true;
}


//==========================================================================
//
//  VZipStreamReader::readSomeBytes
//
//  just read, no `nextpos` advancement
//  returns number of bytes read, -1 on error, or 0 on EOF
//  no need to lock here
//
//==========================================================================
int VZipStreamReader::readSomeBytes (void *buf, int len) {
  if (len <= 0) return -1;
  if (!srcStream) return -1;
  if (bError) return -1;
  if (srcStream->IsError()) return -1;

  zStream.next_out = (Bytef *)buf;
  zStream.avail_out = len;
  int bytesRead = 0;
  while (zStream.avail_out > 0) {
    // get more compressed data (if necessary)
    if (zStream.avail_in == 0) {
      vint32 left = (int)compressedSize-(srccurpos-stpos);
      if (left <= 0) break; // eof
      srcStream->Seek(srccurpos);
      if (srcStream->IsError()) {
        GLog.WriteLine(NAME_Error, "%s", "Error seeking in source inflated stream");
        return -1;
      }
      vint32 bytesToRead = BUFFER_SIZE;
      if (bytesToRead > left) bytesToRead = left;
      srcStream->Serialise(buffer, bytesToRead);
      if (srcStream->IsError()) {
        GLog.WriteLine(NAME_Error, "%s", "Error reading source inflated stream data");
        return -1;
      }
      srccurpos += bytesToRead;
      zStream.next_in = buffer;
      zStream.avail_in = bytesToRead;
    }
    // unpack some data
    vuint32 totalOutBefore = zStream.total_out;
    int err = inflate(&zStream, /*Z_SYNC_FLUSH*/Z_NO_FLUSH);
    if (err != Z_OK && err != Z_STREAM_END) {
      GLog.WriteLine(NAME_Error, "%s", "Error unpacking inflated stream");
      return -1;
    }
    vuint32 totalOutAfter = zStream.total_out;
    bytesRead += totalOutAfter-totalOutBefore;
    if (err != Z_OK) break;
  }
  if (bytesRead && doCrcCheck) currCrc32 = crc32(currCrc32, (const Bytef *)buf, bytesRead);
  return bytesRead;
}


//==========================================================================
//
//  VZipStreamReader::Serialise
//
//==========================================================================
void VZipStreamReader::Serialise (void* buf, int len) {
  if (len == 0) return;
  MyThreadLocker locker(&lock);

  if (!initialised || len < 0 || !srcStream || srcStream->IsError()) setError();
  if (bError) return;

  if (currpos > nextpos || forceRewind) {
    //fprintf(stderr, "+++ REWIND <%s>: currpos=%d; nextpos=%d\n", *GetName(), currpos, nextpos);
    // rewind stream
    if (initialised) { inflateEnd(&zStream); initialised = false; }
    vint32 bytesToRead = BUFFER_SIZE;
    if (bytesToRead > (int)compressedSize) bytesToRead = (int)compressedSize;
    memset(&zStream, 0, sizeof(zStream));
    srcStream->Seek(stpos);
    srcStream->Serialise(buffer, bytesToRead);
    if (srcStream->IsError()) { setError(); return; }
    srccurpos = stpos+bytesToRead;
    zStream.next_in = buffer;
    zStream.avail_in = bytesToRead;
    // open zip stream
    //int err = (zipArchive ? inflateInit2(&zStream, -MAX_WBITS) : inflateInit(&zStream));
    int err = (strmType == Type::RAW ? inflateInit2(&zStream, -MAX_WBITS) : inflateInit2(&zStream, MAX_WBITS+32)); // allow gzip
    if (err != Z_OK) { setError(); return; }
    initialised = true;
    currpos = 0;
    forceRewind = false;
    currCrc32 = 0; // why not?
  }

  //if (currpos < nextpos) fprintf(stderr, "+++ SKIPPING <%s>: currpos=%d; nextpos=%d; toskip=%d\n", *GetName(), currpos, nextpos, nextpos-currpos);
  while (currpos < nextpos) {
    char tmpbuf[256];
    int toread = nextpos-currpos;
    if (toread > 256) toread = 256;
    int rd = readSomeBytes(tmpbuf, toread);
    //fprintf(stderr, "+++   SKIPREAD <%s>: currpos=%d; nextpos=%d; rd=%d; read=%d\n", *GetName(), currpos, nextpos, rd, toread);
    if (rd <= 0) { setError(); return; }
    currpos += rd;
  }

  if (nextpos != currpos) { setError(); return; } // just in case

  //fprintf(stderr, "+++ ZREAD <%s>: pos=%d; len=%d; end=%d (%u)\n", *GetName(), currpos, len, currpos+len, uncompressedSize);

  vuint8 *dest = (vuint8 *)buf;
  while (len > 0) {
    int rd = readSomeBytes(dest, len);
    if (rd <= 0) { setError(); return; }
    len -= rd;
    nextpos = (currpos += rd);
    dest += rd;
  }

  if (doCrcCheck && uncompressedSize != UNKNOWN_SIZE && (vuint32)nextpos == uncompressedSize) {
    if (currCrc32 != origCrc32) { setError(); return; } // alas
  }
}


//==========================================================================
//
//  VZipStreamReader::Seek
//
//==========================================================================
void VZipStreamReader::Seek (int pos) {
  if (bError) return;

  if (pos < 0) { setError(); return; }

  if (uncompressedSize == UNKNOWN_SIZE) {
    // size is unknown
    nextpos = pos;
  } else {
    if ((vuint32)pos > uncompressedSize) pos = (vint32)uncompressedSize;
    nextpos = pos;
  }
}


//==========================================================================
//
//  VZipStreamReader::Tell
//
//==========================================================================
int VZipStreamReader::Tell () {
  return nextpos;
}


//==========================================================================
//
//  VZipStreamReader::TotalSize
//
//==========================================================================
int VZipStreamReader::TotalSize () {
  if (bError) return 0;
  if (uncompressedSize == UNKNOWN_SIZE) {
    // calculate size
    MyThreadLocker locker(&lock);
    for (;;) {
      char tmpbuf[256];
      int rd = readSomeBytes(tmpbuf, 256);
      if (rd < 0) { setError(); return 0; }
      if (rd == 0) break;
      currpos += rd;
    }
    uncompressedSize = (vuint32)currpos;
    //fprintf(stderr, "+++ scanned <%s>: size=%u\n", *GetName(), uncompressedSize);
  }
  return uncompressedSize;
}


//==========================================================================
//
//  VZipStreamReader::AtEnd
//
//==========================================================================
bool VZipStreamReader::AtEnd () {
  return (bError || nextpos >= TotalSize());
}



//==========================================================================
//
//  VZipStreamWriter::VZipStreamWriter
//
//==========================================================================
VZipStreamWriter::VZipStreamWriter (VStream *ADstStream, int clevel, Type atype)
  : dstStream(ADstStream)
  , initialised(false)
  , currCrc32(0)
  , doCrcCalc(false)
{
  mythread_mutex_init(&lock);
  bLoading = false;

  // initialise zip stream structure
  memset((void *)&zStream, 0, sizeof(zStream));
  /*
  zStream.total_in = 0;
  zStream.zalloc = (alloc_func)0;
  zStream.zfree = (free_func)0;
  zStream.opaque = (voidpf)0;
  */

  check(ADstStream);

  if (clevel < 0) clevel = 0; else if (clevel > 9) clevel = 9;

  // open zip stream
  int err = Z_STREAM_ERROR;
  switch (atype) {
    case Type::ZLIB:
      err = deflateInit(&zStream, clevel);
      break;
    case Type::RAW:
      err = deflateInit2(&zStream, clevel, Z_DEFLATED, -15, 9, Z_DEFAULT_STRATEGY);
      break;
    case Type::GZIP:
      err = deflateInit2(&zStream, clevel, Z_DEFLATED, 15+16, 9, Z_DEFAULT_STRATEGY);
      break;
  }

  //int err = deflateInit(&zStream, Z_BEST_COMPRESSION);
  if (err != Z_OK) {
    bError = true;
    GLog.WriteLine(NAME_Error, "%s", "Failed to initialise deflate ZStream");
    return;
  }

  zStream.next_out = buffer;
  zStream.avail_out = BUFFER_SIZE;
  initialised = true;
}


//==========================================================================
//
//  VZipStreamWriter::~VZipStreamWriter
//
//==========================================================================
VZipStreamWriter::~VZipStreamWriter () {
  Close();
  mythread_mutex_destroy(&lock);
}


//==========================================================================
//
//  VZipStreamWriter::setRequireCrc
//
//==========================================================================
void VZipStreamWriter::setRequireCrc () {
  if (!doCrcCalc && zStream.total_in == 0) doCrcCalc = true;
}


//==========================================================================
//
//  VZipStreamWriter::getCrc32
//
//==========================================================================
vuint32 VZipStreamWriter::getCrc32 () const {
  return currCrc32;
}


//==========================================================================
//
//  VZipStreamWriter::setError
//
//==========================================================================
void VZipStreamWriter::setError () {
  if (initialised) { deflateEnd(&zStream); initialised = false; }
  //if (dstStream) { delete dstStream; dstStream = nullptr; }
  dstStream = nullptr;
  bError = true;
}


//==========================================================================
//
//  VZipStreamWriter::Serialise
//
//==========================================================================
void VZipStreamWriter::Serialise (void *buf, int len) {
  if (len == 0) return;
  MyThreadLocker locker(&lock);

  if (!initialised || len < 0 || !dstStream || dstStream->IsError()) setError();
  if (bError) return;

  if (doCrcCalc) currCrc32 = crc32(currCrc32, (const Bytef *)buf, len);

  zStream.next_in = (Bytef *)buf;
  zStream.avail_in = len;
  do {
    zStream.next_out = buffer;
    zStream.avail_out = BUFFER_SIZE;
    int err = deflate(&zStream, Z_NO_FLUSH);
    if (err == Z_STREAM_ERROR) { setError(); return; }
    if (zStream.avail_out != BUFFER_SIZE) {
      dstStream->Serialise(buffer, BUFFER_SIZE-zStream.avail_out);
      if (dstStream->IsError()) { setError(); return; }
    }
  } while (zStream.avail_out == 0);
  //check(zStream.avail_in == 0);
}


//==========================================================================
//
//  VZipStreamWriter::Seek
//
//==========================================================================
void VZipStreamWriter::Seek (int pos) {
  GLog.WriteLine(NAME_Error, "%s", "Cannot seek in compressed writer");
  setError();
}


void VZipStreamWriter::Flush () {
  MyThreadLocker locker(&lock);

  if (!initialised || !dstStream || dstStream->IsError()) setError();
  if (bError) return;

  zStream.avail_in = 0;
  do {
    zStream.next_out = buffer;
    zStream.avail_out = BUFFER_SIZE;
    int err = deflate(&zStream, Z_FULL_FLUSH);
    if (err == Z_STREAM_ERROR) { setError(); return; }
    if (zStream.avail_out != BUFFER_SIZE) {
      dstStream->Serialise(buffer, BUFFER_SIZE-zStream.avail_out);
      if (dstStream->IsError()) { setError(); return; }
    }
  } while (zStream.avail_out == 0);
  dstStream->Flush();
  if (dstStream->IsError()) { setError(); return; }
}


bool VZipStreamWriter::Close () {
  if (initialised) {
    if (!bError) {
      MyThreadLocker locker(&lock);
      zStream.avail_in = 0;
      do {
        zStream.next_out = buffer;
        zStream.avail_out = BUFFER_SIZE;
        int err = deflate(&zStream, Z_FINISH);
        if (err == Z_STREAM_ERROR) { setError(); break; }
        if (zStream.avail_out != BUFFER_SIZE) {
          dstStream->Serialise(buffer, BUFFER_SIZE-zStream.avail_out);
          if (dstStream->IsError()) { setError(); break; }
        }
      } while (zStream.avail_out == 0);
    }
    deflateEnd(&zStream);
    initialised = false;
  }
  return !bError;
}


#else

/*
//==========================================================================
//
//  VZipStreamReader::VZipStreamReader
//
//==========================================================================
VZipStreamReader::VZipStreamReader (VStream *ASrcStream, vuint32 AUncompressedSize, Type atype)
  : SrcStream(ASrcStream)
  , Initialised(false)
  , UncompressedSize(AUncompressedSize)
  , srcStartPos(0)
  , srcCurrPos(0)
  , type(atype)
  , StreamName()
  , useInternalStreamName(false)
{
  mythread_mutex_init(&lock);
  initialize();
}


//==========================================================================
//
//  VZipStreamReader::VZipStreamReader
//
//==========================================================================
VZipStreamReader::VZipStreamReader (const VStr &strmName, VStream *ASrcStream, vuint32 AUncompressedSize, Type atype)
  : SrcStream(ASrcStream)
  , Initialised(false)
  , UncompressedSize(AUncompressedSize)
  , srcStartPos(0)
  , srcCurrPos(0)
  , type(atype)
  , StreamName(strmName)
  , useInternalStreamName(true)
{
  mythread_mutex_init(&lock);
  initialize();
}


//==========================================================================
//
//  VZipStreamReader::VZipStreamReader
//
//==========================================================================
VZipStreamReader::VZipStreamReader (bool useCurrSrcPos, VStream *ASrcStream, vuint32 AUncompressedSize, Type atype)
  : SrcStream(ASrcStream)
  , Initialised(false)
  , UncompressedSize(AUncompressedSize)
  , srcStartPos(0)
  , srcCurrPos(0)
  , type(atype)
  , StreamName()
  , useInternalStreamName(false)
{
  mythread_mutex_init(&lock);
  if (useCurrSrcPos) {
    MyThreadLocker locker(&lock);
    srcStartPos = SrcStream->Tell();
  }
  initialize();
}


//==========================================================================
//
//  VZipStreamReader::VZipStreamReader
//
//==========================================================================
VZipStreamReader::VZipStreamReader (const VStr &strmName, bool useCurrSrcPos, VStream *ASrcStream, vuint32 AUncompressedSize, Type atype)
  : SrcStream(ASrcStream)
  , Initialised(false)
  , UncompressedSize(AUncompressedSize)
  , srcStartPos(0)
  , srcCurrPos(0)
  , type(atype)
  , StreamName(strmName)
  , useInternalStreamName(true)
{
  mythread_mutex_init(&lock);
  if (useCurrSrcPos) {
    MyThreadLocker locker(&lock);
    srcStartPos = SrcStream->Tell();
  }
  initialize();
}


//==========================================================================
//
//  VZipStreamReader::~VZipStreamReader
//
//==========================================================================
VZipStreamReader::~VZipStreamReader () {
  Close();
  mythread_mutex_destroy(&lock);
}


//==========================================================================
//
//  VZipStreamReader::Close
//
//==========================================================================
bool VZipStreamReader::Close () {
  if (Initialised) inflateEnd(&ZStream);
  Initialised = false;
  StreamName.clear();
  useInternalStreamName = true;
  return !bError;
}


//==========================================================================
//
//  VZipStreamReader::initialize
//
//==========================================================================
void VZipStreamReader::initialize () {
  // initialise zip stream structure
  ZStream.total_out = 0;
  ZStream.zalloc = (alloc_func)0;
  ZStream.zfree = (free_func)0;
  ZStream.opaque = (voidpf)0;

  {
    mythread_mutex_destroy(&lock);
    // read in some initial data
    vint32 BytesToRead = BUFFER_SIZE;
    auto srcleft = SrcStream->TotalSize()-srcStartPos;
    if (BytesToRead > srcleft) BytesToRead = srcleft;
    SrcStream->Seek(srcStartPos);
    SrcStream->Serialise(Buffer, BytesToRead);
    srcCurrPos = BytesToRead;
    if (SrcStream->IsError()) {
      bError = true;
      return;
    }
    ZStream.next_in = Buffer;
    ZStream.avail_in = BytesToRead;
  }

  // open zip stream
  //verify(inflateInit2(&ZStream, -MAX_WBITS) == Z_OK);
  int err = (type == Type::RAW ? inflateInit2(&ZStream, -MAX_WBITS) : inflateInit2(&ZStream, MAX_WBITS+32)); // allow gzip
  //inflateInit(&ZStream);
  if (err != Z_OK) {
    bError = true;
    GLog.WriteLine(NAME_Error, "%s", "Failed to initialise inflate ZStream");
    return;
  }

  Initialised = true;
  bLoading = true;
}


//==========================================================================
//
//  VZipStreamReader::reinitialize
//
//==========================================================================
void VZipStreamReader::reinitialize () {
  if (bError) return;
  if (Initialised) inflateEnd(&ZStream);
  Initialised = false;
  if (bError) return;
  initialize();
}


//==========================================================================
//
//  VZipStreamReader::GetName
//
//==========================================================================
const VStr &VZipStreamReader::GetName () const {
  if (useInternalStreamName) return StreamName;
  if (SrcStream) {
    MyThreadLocker locker(&lock);
    return SrcStream->GetName();
  }
  return VStr::EmptyString;
}


//==========================================================================
//
//  VZipStreamReader::Serialise
//
//==========================================================================
void VZipStreamReader::Serialise (void *V, int Length) {
  check(Length >= 0);

  if (bError) return; // don't read anything from already broken stream
  if (Length == 0) return;

  //if (SrcStream->IsError()) { bError = true; return; }

  ZStream.next_out = (Bytef *)V;
  ZStream.avail_out = Length;

  if (UncompressedSize != UNKNOWN_SIZE) {
    if (ZStream.total_out >= UncompressedSize) {
      bError = true;
      return;
    }
    if (Length > (int)(UncompressedSize-ZStream.total_out)) {
      bError = true;
      Length = (int)(UncompressedSize-ZStream.total_out);
      if (Length == 0) return; // just in case
    }
  }

  int BytesRead = 0;
  while (ZStream.avail_out > 0) {
    if (ZStream.avail_in == 0) {
      MyThreadLocker locker(&lock);
      if (SrcStream->IsError()) { bError = true; return; }
      SrcStream->Seek(srcStartPos+srcCurrPos);
      if (SrcStream->IsError()) { bError = true; return; }
      if (SrcStream->AtEnd()) break;
      vint32 BytesToRead = BUFFER_SIZE;
      if (BytesToRead > SrcStream->TotalSize()-SrcStream->Tell()) {
        BytesToRead = SrcStream->TotalSize()-SrcStream->Tell();
      }
      SrcStream->Serialise(Buffer, BytesToRead);
      srcCurrPos += BytesToRead;
      if (SrcStream->IsError()) { bError = true; return; }
      ZStream.next_in = Buffer;
      ZStream.avail_in = BytesToRead;
    }

    vuint32 TotalOutBefore = ZStream.total_out;
    int err = inflate(&ZStream, Z_SYNC_FLUSH);
    if (err >= 0 && ZStream.msg != nullptr) {
      bError = true;
      GLog.WriteLine(NAME_Error, "Decompression failed: %s", ZStream.msg);
      return;
    }
    vuint32 TotalOutAfter = ZStream.total_out;
    BytesRead += TotalOutAfter-TotalOutBefore;

    if (err != Z_OK) break;
  }

  if (BytesRead != Length) {
    bError = true;
    GLog.WriteLine(NAME_Error, "VZipStreamReader: only read %d of %d bytes", BytesRead, Length);
  }
}


//==========================================================================
//
//  VZipStreamReader::Seek
//
//==========================================================================
void VZipStreamReader::Seek (int InPos) {
  check(InPos >= 0);
  check(InPos <= (int)UncompressedSize);

  if (UncompressedSize == UNKNOWN_SIZE) Sys_Error("Seek on zip ZStream with unknown total size");

  if (bError) return;

  // if seeking backwards, reset input ZStream to the begining of the file
  if (InPos < Tell()) {
    /+
    check(Initialised);
    inflateEnd(&ZStream);
    memset(&ZStream, 0, sizeof(ZStream));
    verify(inflateInit2(&ZStream, -MAX_WBITS) == Z_OK);
    SrcStream->Seek(srcStartPos);
    +/
    reinitialize();
    if (bError) return;
  }

  // read data into a temporary buffer until we reach needed position
  int ToSkip = InPos-Tell();
  vuint8 TmpBuf[1024];
  while (ToSkip > 0) {
    int Count = (ToSkip > 1024 ? 1024 : ToSkip);
    ToSkip -= Count;
    Serialise(TmpBuf, Count);
    if (bError) return;
  }
}


//==========================================================================
//
//  VZipStreamReader::Tell
//
//==========================================================================
int VZipStreamReader::Tell () {
  return ZStream.total_out;
}


//==========================================================================
//
//  VZipStreamReader::TotalSize
//
//==========================================================================
int VZipStreamReader::TotalSize () {
  if (UncompressedSize == UNKNOWN_SIZE) Sys_Error("TotalSize on zip ZStream with unknown total size");
  return UncompressedSize;
}


//==========================================================================
//
//  VZipStreamReader::AtEnd
//
//==========================================================================
bool VZipStreamReader::AtEnd () {
  return (ZStream.avail_in == 0 && SrcStream->AtEnd());
}



//==========================================================================
//
//  VZipStreamWriter::VZipStreamWriter
//
//==========================================================================
VZipStreamWriter::VZipStreamWriter (VStream *ADstStream, int clevel, Type atype)
  : DstStream(ADstStream)
  , Initialised(false)
{
  // initialise zip stream structure
  ZStream.total_in = 0;
  ZStream.zalloc = (alloc_func)0;
  ZStream.zfree = (free_func)0;
  ZStream.opaque = (voidpf)0;

  if (clevel < 0) clevel = 0; else if (clevel > 9) clevel = 9;

  // open zip stream
  int err = Z_STREAM_ERROR;
  switch (atype) {
    case Type::ZLIB:
      err = deflateInit(&ZStream, clevel);
      break;
    case Type::RAW:
      err = deflateInit2(&ZStream, clevel, Z_DEFLATED, -15, 9, Z_DEFAULT_STRATEGY);
      break;
    case Type::GZIP:
      err = deflateInit2(&ZStream, clevel, Z_DEFLATED, 15+16, 9, Z_DEFAULT_STRATEGY);
      break;
  }
  if (err != Z_OK) {
    bError = true;
    GLog.WriteLine(NAME_Error, "%s", "Failed to initialise deflate ZStream");
    return;
  }
  ZStream.next_out = Buffer;
  ZStream.avail_out = BUFFER_SIZE;

  Initialised = true;
  bLoading = false;
}


//==========================================================================
//
//  VZipStreamWriter::Close
//
//==========================================================================
bool VZipStreamWriter::Close () {
  if (Initialised) {
    if (!bError) {
      ZStream.avail_in = 0;
      do {
        ZStream.next_out = Buffer;
        ZStream.avail_out = BUFFER_SIZE;

        int err = deflate(&ZStream, Z_FINISH);
        if (err == Z_STREAM_ERROR) {
          bError = true;
          break;
        }

        if (ZStream.avail_out != BUFFER_SIZE) {
          DstStream->Serialise(Buffer, BUFFER_SIZE-ZStream.avail_out);
          if (DstStream->IsError()) {
            bError = true;
            break;
          }
        }
      } while (ZStream.avail_out == 0);
    }
    deflateEnd(&ZStream);
  }
  Initialised = false;
  return !bError;
}


//==========================================================================
//
//  VZipStreamWriter::VStr
//
//==========================================================================
const VStr &VZipStreamWriter::GetName () const {
  return (DstStream ? DstStream->GetName() : VStr::EmptyString);
}


//==========================================================================
//
//  VZipStreamWriter::Serialise
//
//==========================================================================
void VZipStreamWriter::Serialise (void *V, int Length) {
  check(Length >= 0);

  if (bError) return; // don't write anything to already broken stream
  if (DstStream->IsError()) { bError = false; return; }

  if (Length == 0) return;

  ZStream.next_in = (Bytef *)V;
  ZStream.avail_in = Length;

  do {
    ZStream.next_out = Buffer;
    ZStream.avail_out = BUFFER_SIZE;

    int err = deflate(&ZStream, Z_NO_FLUSH);
    if (err == Z_STREAM_ERROR) {
      bError = true;
      return;
    }

    if (ZStream.avail_out != BUFFER_SIZE) {
      DstStream->Serialise(Buffer, BUFFER_SIZE-ZStream.avail_out);
      if (DstStream->IsError()) {
        bError = true;
        return;
      }
    }
  } while (ZStream.avail_out == 0);
  check(ZStream.avail_in == 0);
}


//==========================================================================
//
//  VZipStreamWriter::Seek
//
//==========================================================================
void VZipStreamWriter::Seek (int InPos) {
  Sys_Error("Can't seek on zip compression stream");
}


//==========================================================================
//
//  VZipStreamWriter::Flush
//
//==========================================================================
void VZipStreamWriter::Flush () {
  if (bError) return; // don't read anything from already broken stream
  if (DstStream->IsError()) { bError = true; return; }

  ZStream.avail_in = 0;
  do {
    ZStream.next_out = Buffer;
    ZStream.avail_out = BUFFER_SIZE;

    int err = deflate(&ZStream, Z_FULL_FLUSH);
    if (err == Z_STREAM_ERROR) {
      bError = true;
      return;
    }

    if (ZStream.avail_out != BUFFER_SIZE) {
      DstStream->Serialise(Buffer, BUFFER_SIZE-ZStream.avail_out);
      if (DstStream->IsError()) {
        bError = true;
        return;
      }
    }
  } while (ZStream.avail_out == 0);
  DstStream->Flush();
}
*/


#endif
