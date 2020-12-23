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
#include "../core.h"

#define MAX_WBITS 15
#define MAX_MEM_LEVEL 9

//#define VV_ZLIB_ALLOW_GZIP  (32)
// miniz cannot into gzip reading
#define VV_ZLIB_ALLOW_GZIP  (0)


//==========================================================================
//
//  VZLibStreamReader::VZLibStreamReader
//
//==========================================================================
VZLibStreamReader::VZLibStreamReader (VStream *ASrcStream, vuint32 ACompressedSize, vuint32 AUncompressedSize, Type atype)
  : VStream()
  , srcStream(ASrcStream)
  , rdlock(nullptr)
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
  , wholeBuf(nullptr)
  , wholeSize(-2)
{
  initialize();
}


//==========================================================================
//
//  VZLibStreamReader::VZLibStreamReader
//
//==========================================================================
VZLibStreamReader::VZLibStreamReader (VStr fname, VStream *ASrcStream, vuint32 ACompressedSize, vuint32 AUncompressedSize, Type atype)
  : VStream()
  , srcStream(ASrcStream)
  , rdlock(nullptr)
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
  , wholeBuf(nullptr)
  , wholeSize(-2)
{
  vassert(ASrcStream);
  initialize();
}


//==========================================================================
//
//  VZLibStreamReader::VZLibStreamReader
//
//==========================================================================
VZLibStreamReader::VZLibStreamReader (bool useCurrSrcPos, VStream *ASrcStream, vuint32 ACompressedSize, vuint32 AUncompressedSize, Type atype)
  : VStream()
  , srcStream(ASrcStream)
  , rdlock(nullptr)
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
  , wholeBuf(nullptr)
  , wholeSize(-2)
{
  vassert(ASrcStream);
  initialize();
}


//==========================================================================
//
//  VZLibStreamReader::VZLibStreamReader
//
//==========================================================================
VZLibStreamReader::VZLibStreamReader (bool useCurrSrcPos, VStr fname, VStream *ASrcStream, vuint32 ACompressedSize, vuint32 AUncompressedSize, Type atype)
  : VStream()
  , srcStream(ASrcStream)
  , rdlock(nullptr)
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
  , wholeBuf(nullptr)
  , wholeSize(-2)
{
  vassert(ASrcStream);
  initialize();
}


//==========================================================================
//
//  VZLibStreamReader::VZLibStreamReader
//
//==========================================================================
VZLibStreamReader::VZLibStreamReader (mythread_mutex *ardlock, VStream *ASrcStream, vuint32 ACompressedSize, vuint32 AUncompressedSize, Type atype)
  : VStream()
  , srcStream(ASrcStream)
  , rdlock(ardlock)
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
  , wholeBuf(nullptr)
  , wholeSize(-2)
{
  initialize();
}


//==========================================================================
//
//  VZLibStreamReader::VZLibStreamReader
//
//==========================================================================
VZLibStreamReader::VZLibStreamReader (mythread_mutex *ardlock, VStr fname, VStream *ASrcStream, vuint32 ACompressedSize, vuint32 AUncompressedSize, Type atype)
  : VStream()
  , srcStream(ASrcStream)
  , rdlock(ardlock)
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
  , wholeBuf(nullptr)
  , wholeSize(-2)
{
  vassert(ASrcStream);
  initialize();
}


//==========================================================================
//
//  VZLibStreamReader::VZLibStreamReader
//
//==========================================================================
VZLibStreamReader::VZLibStreamReader (mythread_mutex *ardlock, bool useCurrSrcPos, VStream *ASrcStream, vuint32 ACompressedSize, vuint32 AUncompressedSize, Type atype)
  : VStream()
  , srcStream(ASrcStream)
  , rdlock(ardlock)
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
  , wholeBuf(nullptr)
  , wholeSize(-2)
{
  vassert(ASrcStream);
  initialize();
}


//==========================================================================
//
//  VZLibStreamReader::VZLibStreamReader
//
//==========================================================================
VZLibStreamReader::VZLibStreamReader (mythread_mutex *ardlock, bool useCurrSrcPos, VStr fname, VStream *ASrcStream, vuint32 ACompressedSize, vuint32 AUncompressedSize, Type atype)
  : VStream()
  , srcStream(ASrcStream)
  , rdlock(ardlock)
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
  , wholeBuf(nullptr)
  , wholeSize(-2)
{
  vassert(ASrcStream);
  initialize();
}


//==========================================================================
//
//  VZLibStreamReader::~VZLibStreamReader
//
//==========================================================================
VZLibStreamReader::~VZLibStreamReader () {
  Close();
}


//==========================================================================
//
//  VZLibStreamReader::GetName
//
//==========================================================================
VStr VZLibStreamReader::GetName () const {
  return mFileName.cloneUnique();
}


//==========================================================================
//
//  VZLibStreamReader::setCrc
//
//  turns on CRC checking
//
//==========================================================================
void VZLibStreamReader::setCrc (vuint32 acrc) {
  if (doCrcCheck && origCrc32 == acrc) return;
  origCrc32 = acrc;
  doCrcCheck = true;
  currCrc32 = 0;
  if (wholeSize >= 0) {
    // calculate crc now
    if (wholeSize > 0) currCrc32 = mz_crc32(currCrc32, (const vuint8 *)wholeBuf, wholeSize);
    if (currCrc32 != origCrc32) SetError();
  } else {
    forceRewind = true;
  }
}


//==========================================================================
//
//  VZLibStreamReader::Close
//
//==========================================================================
bool VZLibStreamReader::Close () {
  if (wholeBuf) { Z_Free(wholeBuf); wholeBuf = nullptr; }
  wholeSize = -2;
  deinitZStream();
  //if (srcStream) { delete srcStream; srcStream = nullptr; }
  srcStream = nullptr;
  return !bError;
}


//==========================================================================
//
//  VZLibStreamReader::SetError
//
//==========================================================================
void VZLibStreamReader::SetError () {
  if (wholeBuf) { Z_Free(wholeBuf); wholeBuf = nullptr; }
  wholeSize = -2;
  deinitZStream();
  //if (srcStream) { delete srcStream; srcStream = nullptr; }
  srcStream = nullptr;
  VStream::SetError();
}


//==========================================================================
//
//  VZLibStreamReader::deinitZStream
//
//==========================================================================
void VZLibStreamReader::deinitZStream () {
  if (initialised) { mz_inflateEnd(&zStream); initialised = false; }
  memset(&zStream, 0, sizeof(zStream));
}


//==========================================================================
//
//  VZLibStreamReader::resetZStream
//
//==========================================================================
bool VZLibStreamReader::resetZStream () {
  deinitZStream();
  bool wasError = false;
  if (compressedSize == 0) return true;
  {
    MyThreadLocker locker(rdlock);
    srccurpos = stpos;
    srcStream->Seek(stpos);
    wasError = srcStream->IsError();
    if (!wasError && compressedSize == UNKNOWN_SIZE) {
      int sz = srcStream->TotalSize();
      wasError = srcStream->IsError();
      if (sz >= stpos) compressedSize = (vuint32)(sz-stpos);
    }
  }
  if (wasError) {
    GLog.Log(NAME_Error, "Error seeking in source inflated stream");
    SetError();
    return false;
  }
  int res = fillPackedBuffer();
  if (res < 0) return false;
  int err = (strmType == Type::RAW ? mz_inflateInit2(&zStream, -MAX_WBITS) : mz_inflateInit2(&zStream, MAX_WBITS+VV_ZLIB_ALLOW_GZIP));
  if (err != MZ_OK) {
    GLog.Logf(NAME_Error, "Error initializing inflate (%d)", err);
    SetError();
    return false;
  }
  initialised = true;
  currpos = 0;
  forceRewind = false;
  currCrc32 = 0;
  return true;
}


//==========================================================================
//
//  VZLibStreamReader::initialize
//
//==========================================================================
void VZLibStreamReader::initialize () {
  bLoading = true;

  if (wholeBuf) { Z_Free(wholeBuf); wholeBuf = nullptr; }
  wholeSize = -2;

  vassert(initialised == false);
  // initialise zip stream structure
  memset(&zStream, 0, sizeof(zStream));

  if (srcStream) {
    // read in some initial data
    bool wasError = false;
    {
      MyThreadLocker locker(rdlock);
      if (doSeekToSrcStart) {
        srcStream->Seek(0);
        stpos = 0;
      } else {
        stpos = srcStream->Tell();
      }
      srccurpos = stpos;
      wasError = srcStream->IsError();
    }
    if (wasError) {
      GLog.Log(NAME_Error, "Error seeking in source inflated stream");
      SetError();
      return;
    }
    if (!resetZStream()) return;
  } else {
    VStream::SetError();
  }
}


//==========================================================================
//
//  VZLibStreamReader::fillPackedBuffer
//
//  returns 0 if no more data, -1 on error, 1 if something was read
//
//==========================================================================
int VZLibStreamReader::fillPackedBuffer () {
  vassert(zStream.avail_in >= 0 && zStream.avail_in <= BUFFER_SIZE);
  if (zStream.avail_in >= BUFFER_SIZE) return 1;
  vint32 left = (int)compressedSize-(srccurpos-stpos);
  if (left < 0) {
    GLog.Logf(NAME_Error, "internal error 0 in VZLibStreamReader::fillPackedBuffer (csz=%d; stpos=%d; cpos=%d; left=%d)", (int)compressedSize, (int)stpos, (int)srccurpos, (int)left);
    //Sys_Error("fuck!");
    SetError();
    return -1;
  }
  vint32 bytesToRead = BUFFER_SIZE;
  if (bytesToRead > left) bytesToRead = left;
  if (zStream.avail_in == 0) {
    zStream.next_in = buffer;
  } else {
    if (bytesToRead > (int)(BUFFER_SIZE-zStream.avail_in)) bytesToRead = (int)(BUFFER_SIZE-zStream.avail_in);
  }
  if (bytesToRead == 0) return 0;
  bool strmerr = false;
  bool seekerr = false;
  {
    MyThreadLocker locker(rdlock);
    strmerr = srcStream->IsError();
    if (!strmerr) {
      srcStream->Seek(srccurpos);
      seekerr = srcStream->IsError();
      if (!seekerr) {
        srcStream->Serialise((void *)zStream.next_in, bytesToRead);
        strmerr = srcStream->IsError();
      }
    }
  }
  if (seekerr) {
    GLog.Log(NAME_Error, "Error seeking in source inflated stream");
    SetError();
    return -1;
  }
  if (strmerr) {
    GLog.Log(NAME_Error, "Error reading source inflated stream data");
    SetError();
    return -1;
  }
  zStream.avail_in += bytesToRead;
  srccurpos += bytesToRead;
  return 1;
}


//==========================================================================
//
//  VZLibStreamReader::readSomeBytes
//
//  just read, no `nextpos` advancement
//  returns number of bytes read, -1 on error, or 0 on EOF
//
//==========================================================================
int VZLibStreamReader::readSomeBytes (void *buf, int len) {
  if (len <= 0) return -1;
  if (!srcStream) return -1;
  if (bError) return -1;
  zStream.next_out = (vuint8 *)buf;
  zStream.avail_out = len;
  int bytesRead = 0;
  while (zStream.avail_out > 0) {
    //GLog.Logf(NAME_Debug, "read: left=%u; avail_in=%u; cleft=%d", (unsigned)zStream.avail_out, (unsigned)zStream.avail_in, (int)compressedSize-(srccurpos-stpos));
    // get more compressed data (if necessary)
    if (zStream.avail_in == 0) {
      int fbres = fillPackedBuffer();
      if (fbres < 0) return -1; // error
    }
    // unpack some data
    vuint32 totalOutBefore = zStream.total_out;
    int err = mz_inflate(&zStream, MZ_SYNC_FLUSH/*MZ_NO_FLUSH*/);
    if (err != MZ_OK && err != MZ_STREAM_END) {
      GLog.Logf(NAME_Error, "%s (err=%d)", "Error unpacking inflated stream", err);
      SetError();
      //GLog.Logf(NAME_Error, "%s (err=%s)", "Error unpacking inflated stream", zStream.msg);
      //GLog.Logf(NAME_Error, "%s (%d; avout=%u; tobef=%u; tocur=%u; inav=%u)", "Error unpacking inflated stream", err, (unsigned)zStream.avail_out, totalOutBefore, (unsigned)zStream.total_out, (unsigned)zStream.avail_in);
      return -1;
    }
    vuint32 totalOutAfter = zStream.total_out;
    bytesRead += totalOutAfter-totalOutBefore;
    if (err != MZ_OK) break;
  }
  if (bytesRead && doCrcCheck) currCrc32 = mz_crc32(currCrc32, (const vuint8 *)buf, bytesRead);
  return bytesRead;
}


//==========================================================================
//
//  VZLibStreamReader::Serialise
//
//==========================================================================
void VZLibStreamReader::Serialise (void* buf, int len) {
  if (len == 0) return;

  // use data cache?
  if (wholeSize >= 0) {
   doCached:
    if (len < 0 || currpos < 0 || currpos >= wholeSize || wholeSize-currpos < len) SetError();
    if (bError) return;
    // here, currpos is always valid
    if (len > 0) memcpy(buf, wholeBuf+currpos, len);
    nextpos = (currpos += len);
    return;
  }

  if (!initialised || len < 0 || !srcStream /*|| srcStream->IsError()*/) SetError();
  if (bError) return;

  vassert(wholeSize < 0);
  if (currpos > nextpos || forceRewind) {
    ++wholeSize;
    //fprintf(stderr, "*** ZIP BACKSEEK(%s); new counter=%d\n", *GetName(), wholeSize);
    if (wholeSize >= 0) {
      // got two back-seeks, cache data
      cacheAllData();
      if (bError) return;
      if (currpos < 0 || currpos > wholeSize) { SetError(); return; }
      goto doCached;
    }
  }

  if (currpos <= nextpos && currpos < 32768 && uncompressedSize != UNKNOWN_SIZE && nextpos >= (vint32)(uncompressedSize-uncompressedSize/3)) {
    ++wholeSize;
    //fprintf(stderr, "*** ZIP BACKSEEK(%s); new counter=%d\n", *GetName(), wholeSize);
    if (wholeSize >= 0) {
      // cache it
      cacheAllData();
      if (bError) return;
      if (currpos < 0 || currpos > wholeSize) { SetError(); return; }
      goto doCached;
    }
  }

  if (currpos > nextpos || forceRewind) {
    //fprintf(stderr, "+++ REWIND <%s>: currpos=%d; nextpos=%d\n", *GetName(), currpos, nextpos);
    // rewind stream
    if (!resetZStream()) return;
  }

  //if (currpos < nextpos) fprintf(stderr, "+++ SKIPPING <%s>: currpos=%d; nextpos=%d; toskip=%d\n", *GetName(), currpos, nextpos, nextpos-currpos);
  if (currpos < nextpos) {
    if (!wholeBuf) wholeBuf = (vuint8 *)Z_Malloc(65536);
    while (currpos < nextpos) {
      int toread = nextpos-currpos;
      if (toread > 65536) toread = 65536;
      int rd = readSomeBytes(wholeBuf, toread);
      //fprintf(stderr, "+++   SKIPREAD <%s>: currpos=%d; nextpos=%d; rd=%d; read=%d\n", *GetName(), currpos, nextpos, rd, toread);
      if (rd <= 0) { SetError(); return; }
      currpos += rd;
    }
  }

  if (nextpos != currpos) { SetError(); return; } // just in case

  //fprintf(stderr, "+++ ZREAD <%s>: pos=%d; len=%d; end=%d (%u)\n", *GetName(), currpos, len, currpos+len, uncompressedSize);

  vuint8 *dest = (vuint8 *)buf;
  while (len > 0) {
    int rd = readSomeBytes(dest, len);
    if (rd <= 0) { SetError(); return; }
    len -= rd;
    nextpos = (currpos += rd);
    dest += rd;
  }

  if (doCrcCheck && uncompressedSize != UNKNOWN_SIZE && (vuint32)nextpos == uncompressedSize) {
    if (currCrc32 != origCrc32) { SetError(); return; } // alas
  }
}


//==========================================================================
//
//  VZLibStreamReader::cacheAllData
//
//==========================================================================
void VZLibStreamReader::cacheAllData () {
  // rewind stream
  if (!resetZStream()) return;

  // read data
  if (uncompressedSize == UNKNOWN_SIZE) {
    //TODO: find the better way to do this
    //GLog.Logf(NAME_Debug, "caching unknown number of bytes from '%s'...", *GetName());
    wholeSize = 0;
    vuint8 *rdbuf = (vuint8 *)Z_Malloc(BUFFER_SIZE);
    for (;;) {
      int rd = readSomeBytes(rdbuf, BUFFER_SIZE);
      if (rd < 0) { Z_Free(rdbuf); SetError(); return; }
      if (rd == 0) break;
      //GLog.Logf(NAME_Debug, "  read %d bytes (new total is %d)", rd, wholeSize+rd);
      if (wholeSize+rd < 0) { Z_Free(rdbuf); SetError(); return; }
      wholeBuf = (vuint8 *)Z_Realloc(wholeBuf, wholeSize+rd);
      memcpy(wholeBuf+wholeSize, rdbuf, rd);
      wholeSize += rd;
    }
    Z_Free(rdbuf);
    uncompressedSize = (vuint32)wholeSize;
    if (nextpos > wholeSize) { SetError(); return; }
  } else {
    wholeSize = (vint32)uncompressedSize;
    if (wholeSize < 0) { SetError(); return; }
    wholeBuf = (vuint8 *)Z_Realloc(wholeBuf, wholeSize ? wholeSize : 1);
    if (wholeSize) {
      int rd = readSomeBytes(wholeBuf, wholeSize);
      if (rd != wholeSize) { SetError(); return; }
    }
  }

  currpos = nextpos;

  // check crc, if necessary
  if (doCrcCheck) {
    currCrc32 = 0;
    // calculate crc now
    if (wholeSize > 0) currCrc32 = mz_crc32(currCrc32, (const vuint8 *)wholeBuf, wholeSize);
    if (currCrc32 != origCrc32) SetError();
  }

  // we don't need zlib stream anymore
  deinitZStream();
  // we don't need source stream anymore
  //if (srcStream) { delete srcStream; srcStream = nullptr; }
  srcStream = nullptr;
  // and we don't need to lock the stream anymore too
  rdlock = nullptr;
}


//==========================================================================
//
//  VZLibStreamReader::Seek
//
//==========================================================================
void VZLibStreamReader::Seek (int pos) {
  if (bError) return;

  if (pos < 0) { SetError(); return; }

  if (wholeSize >= 0) {
    if (pos > wholeSize) { SetError(); return; }
    currpos = nextpos = pos;
    return;
  }

  if (nextpos == pos) return;

  if (uncompressedSize == UNKNOWN_SIZE) {
    // size is unknown
    nextpos = pos;
    //fprintf(stderr, "*** ZIP SEEK UNK(%s); curr=%d; new=%d\n", *GetName(), currpos, nextpos);
  } else {
    if ((vuint32)pos > uncompressedSize) {
      //pos = (vint32)uncompressedSize;
      SetError();
      return;
    }
    nextpos = pos;
    //fprintf(stderr, "*** ZIP SEEK(%s); curr=%d; new=%d\n", *GetName(), currpos, nextpos);
  }
}


//==========================================================================
//
//  VZLibStreamReader::Tell
//
//==========================================================================
int VZLibStreamReader::Tell () {
  return nextpos;
}


//==========================================================================
//
//  VZLibStreamReader::TotalSize
//
//==========================================================================
int VZLibStreamReader::TotalSize () {
  if (bError) return 0;
  if (uncompressedSize == UNKNOWN_SIZE) {
    // cache all data here, why not
    cacheAllData();
    if (bError) return 0;
  }
  return uncompressedSize;
}


//==========================================================================
//
//  VZLibStreamReader::AtEnd
//
//==========================================================================
bool VZLibStreamReader::AtEnd () {
  return (bError || nextpos >= TotalSize());
}



//==========================================================================
//
//  VZLibStreamWriter::VZLibStreamWriter
//
//==========================================================================
VZLibStreamWriter::VZLibStreamWriter (VStream *ADstStream, int clevel, Type atype)
  : dstStream(ADstStream)
  , initialised(false)
  , currCrc32(0)
  , doCrcCalc(false)
{
  bLoading = false;

  // initialise zip stream structure
  memset((void *)&zStream, 0, sizeof(zStream));

  vassert(ADstStream);

  if (clevel < 0) clevel = 0; else if (clevel > 9) clevel = 9;

  // open zip stream
  int err = MZ_STREAM_ERROR;
  switch (atype) {
    case Type::ZLIB:
      err = mz_deflateInit(&zStream, clevel);
      break;
    case Type::RAW:
      err = mz_deflateInit2(&zStream, clevel, MZ_DEFLATED, -15, 9, MZ_DEFAULT_STRATEGY);
      break;
    case Type::GZIP:
      err = mz_deflateInit2(&zStream, clevel, MZ_DEFLATED, 15+16, 9, MZ_DEFAULT_STRATEGY);
      break;
  }

  //int err = mz_deflateInit(&zStream, MZ_BEST_COMPRESSION);
  if (err != MZ_OK) {
    VStream::SetError();
    GLog.Log(NAME_Error, "Failed to initialise deflate ZStream");
    return;
  }

  zStream.next_out = buffer;
  zStream.avail_out = BUFFER_SIZE;
  initialised = true;
}


//==========================================================================
//
//  VZLibStreamWriter::~VZLibStreamWriter
//
//==========================================================================
VZLibStreamWriter::~VZLibStreamWriter () {
  Close();
}


//==========================================================================
//
//  VZLibStreamWriter::setRequireCrc
//
//==========================================================================
void VZLibStreamWriter::setRequireCrc () {
  if (!bError && !doCrcCalc) {
    if (zStream.total_in == 0) doCrcCalc = true; else SetError();
  }
}


//==========================================================================
//
//  VZLibStreamWriter::getCrc32
//
//==========================================================================
vuint32 VZLibStreamWriter::getCrc32 () const {
  return currCrc32;
}


//==========================================================================
//
//  VZLibStreamWriter::SetError
//
//==========================================================================
void VZLibStreamWriter::SetError () {
  if (initialised) { mz_deflateEnd(&zStream); initialised = false; }
  //if (dstStream) { delete dstStream; dstStream = nullptr; }
  dstStream = nullptr;
  VStream::SetError();
}


//==========================================================================
//
//  VZLibStreamWriter::Serialise
//
//==========================================================================
void VZLibStreamWriter::Serialise (void *buf, int len) {
  if (len == 0) return;

  if (!initialised || len < 0 || !dstStream || dstStream->IsError()) SetError();
  if (bError) return;

  if (doCrcCalc) currCrc32 = mz_crc32(currCrc32, (const vuint8 *)buf, len);

  zStream.next_in = (vuint8 *)buf;
  zStream.avail_in = len;
  do {
    zStream.next_out = buffer;
    zStream.avail_out = BUFFER_SIZE;
    int err = mz_deflate(&zStream, MZ_NO_FLUSH);
    if (err == MZ_STREAM_ERROR) { SetError(); return; }
    if (zStream.avail_out != BUFFER_SIZE) {
      dstStream->Serialise(buffer, BUFFER_SIZE-zStream.avail_out);
      if (dstStream->IsError()) { SetError(); return; }
    }
  } while (zStream.avail_out == 0);
  //vassert(zStream.avail_in == 0);
}


//==========================================================================
//
//  VZLibStreamWriter::Seek
//
//==========================================================================
void VZLibStreamWriter::Seek (int pos) {
  GLog.Log(NAME_Error, "Cannot seek in compressed writer");
  SetError();
}


void VZLibStreamWriter::Flush () {
  if (!initialised || !dstStream || dstStream->IsError()) SetError();
  if (bError) return;

  zStream.avail_in = 0;
  do {
    zStream.next_out = buffer;
    zStream.avail_out = BUFFER_SIZE;
    int err = mz_deflate(&zStream, MZ_FULL_FLUSH);
    if (err == MZ_STREAM_ERROR) { SetError(); return; }
    if (zStream.avail_out != BUFFER_SIZE) {
      dstStream->Serialise(buffer, BUFFER_SIZE-zStream.avail_out);
      if (dstStream->IsError()) { SetError(); return; }
    }
  } while (zStream.avail_out == 0);
  dstStream->Flush();
  if (dstStream->IsError()) { SetError(); return; }
}


bool VZLibStreamWriter::Close () {
  if (initialised) {
    if (!bError) {
      zStream.avail_in = 0;
      do {
        zStream.next_out = buffer;
        zStream.avail_out = BUFFER_SIZE;
        int err = mz_deflate(&zStream, MZ_FINISH);
        if (err == MZ_STREAM_ERROR) { SetError(); break; }
        if (zStream.avail_out != BUFFER_SIZE) {
          dstStream->Serialise(buffer, BUFFER_SIZE-zStream.avail_out);
          if (dstStream->IsError()) { SetError(); break; }
        }
      } while (zStream.avail_out == 0);
    }
    mz_deflateEnd(&zStream);
    initialised = false;
  }
  return !bError;
}
