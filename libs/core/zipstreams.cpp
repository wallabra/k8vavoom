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
  , wholeBuf(nullptr)
  , wholeSize(-2)
{
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
  , wholeBuf(nullptr)
  , wholeSize(-2)
{
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
  , wholeBuf(nullptr)
  , wholeSize(-2)
{
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
  , wholeBuf(nullptr)
  , wholeSize(-2)
{
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
}


//==========================================================================
//
//  void
//
//==========================================================================
void VZipStreamReader::initialize () {
  bLoading = true;

  if (wholeBuf) { Z_Free(wholeBuf); wholeBuf = nullptr; }
  wholeSize = -2;

  // initialise zip stream structure
  memset(&zStream, 0, sizeof(zStream));

  if (srcStream) {
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
  if (wholeSize >= 0) {
    // calculate crc now
    if (wholeSize > 0) currCrc32 = crc32(currCrc32, (const Bytef *)wholeBuf, wholeSize);
    if (currCrc32 != origCrc32) setError();
  } else {
    forceRewind = true;
  }
}


//==========================================================================
//
//  VZipStreamReader::Close
//
//==========================================================================
bool VZipStreamReader::Close () {
  if (wholeBuf) { Z_Free(wholeBuf); wholeBuf = nullptr; }
  wholeSize = -2;
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
  if (wholeBuf) { Z_Free(wholeBuf); wholeBuf = nullptr; }
  wholeSize = -2;
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

  // use data cache?
  if (wholeSize >= 0) {
   doCached:
    // we're releasing source stream when cache is active, why not?
    if (len < 0 || currpos < 0 || currpos >= wholeSize || wholeSize-currpos < len) setError();
    if (bError) return;
    // here, currpos is always valid
    memcpy(buf, wholeBuf+currpos, len);
    nextpos = (currpos += len);
    return;
  }

  if (!initialised || len < 0 || !srcStream || srcStream->IsError()) setError();
  if (bError) return;

  if (currpos > nextpos || forceRewind) {
    ++wholeSize;
    //fprintf(stderr, "*** ZIP BACKSEEK(%s); new counter=%d\n", *GetName(), wholeSize);
    if (wholeSize >= 0) {
      // got two back-seeks, cache data
      cacheAllData();
      if (bError) return;
      if (currpos < 0 || currpos > wholeSize) { setError(); return; }
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
      if (currpos < 0 || currpos > wholeSize) { setError(); return; }
      goto doCached;
    }
  }

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
    int err = (strmType == Type::RAW ? inflateInit2(&zStream, -MAX_WBITS) : inflateInit2(&zStream, MAX_WBITS+32)); // allow gzip
    if (err != Z_OK) { setError(); return; }
    initialised = true;
    currpos = 0;
    forceRewind = false;
    currCrc32 = 0; // why not?
  }

  //if (currpos < nextpos) fprintf(stderr, "+++ SKIPPING <%s>: currpos=%d; nextpos=%d; toskip=%d\n", *GetName(), currpos, nextpos, nextpos-currpos);
  if (currpos < nextpos) {
    int bsz = bsz;
    if (bsz > 65536) bsz = 65536;
    check(!wholeBuf);
    wholeBuf = (vuint8 *)Z_Malloc(bsz);
    while (currpos < nextpos) {
      int toread = nextpos-currpos;
      if (toread > bsz) toread = bsz;
      int rd = readSomeBytes(wholeBuf, toread);
      //fprintf(stderr, "+++   SKIPREAD <%s>: currpos=%d; nextpos=%d; rd=%d; read=%d\n", *GetName(), currpos, nextpos, rd, toread);
      if (rd <= 0) { setError(); return; }
      currpos += rd;
    }
    Z_Free(wholeBuf);
    wholeBuf = nullptr;
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
//  VZipStreamReader::cacheAllData
//
//==========================================================================
void VZipStreamReader::cacheAllData () {
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
  int err = (strmType == Type::RAW ? inflateInit2(&zStream, -MAX_WBITS) : inflateInit2(&zStream, MAX_WBITS+32)); // allow gzip
  if (err != Z_OK) { setError(); return; }
  initialised = true;
  forceRewind = false;
  currCrc32 = 0; // why not?

  // read data
  if (uncompressedSize == UNKNOWN_SIZE) {
    //TODO: find the better way to do this
    wholeSize = 0;
    for (;;) {
      int rd = readSomeBytes(buffer, BUFFER_SIZE);
      if (rd < 0) { setError(); return; }
      if (rd == 0) break;
      if (wholeSize+rd < 0) { setError(); return; }
      wholeBuf = (vuint8 *)Z_Realloc(wholeBuf, wholeSize+rd);
      memcpy(wholeBuf+wholeSize, buffer, rd);
      wholeSize += rd;
    }
    uncompressedSize = (vuint32)wholeBuf;
    if (nextpos > wholeSize) { setError(); return; }
  } else {
    wholeSize = (vint32)uncompressedSize;
    if (wholeSize < 0) { setError(); return; }
    wholeBuf = (vuint8 *)Z_Malloc(wholeSize ? wholeSize : 1);
    if (wholeSize) {
      int rd = readSomeBytes(wholeBuf, wholeSize);
      if (rd != wholeSize) { setError(); return; }
    }
  }

  currpos = nextpos;

  // check crc, if necessary
  if (doCrcCheck) {
    currCrc32 = 0;
    // calculate crc now
    if (wholeSize > 0) currCrc32 = crc32(currCrc32, (const Bytef *)wholeBuf, wholeSize);
    if (currCrc32 != origCrc32) setError();
  }

  // we don't need zlib stream anymore
  if (initialised) { inflateEnd(&zStream); initialised = false; }
  // we don't need source stream anymore
  //if (srcStream) { delete srcStream; srcStream = nullptr; }
  srcStream = nullptr;
}


//==========================================================================
//
//  VZipStreamReader::Seek
//
//==========================================================================
void VZipStreamReader::Seek (int pos) {
  if (bError) return;

  if (pos < 0) { setError(); return; }

  if (wholeSize >= 0) {
    if (pos > wholeSize) { setError(); return; }
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
      setError();
      return;
    }
    nextpos = pos;
    //fprintf(stderr, "*** ZIP SEEK(%s); curr=%d; new=%d\n", *GetName(), currpos, nextpos);
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
    // cache all data here, why not
    cacheAllData();
    if (bError) return 0;
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
  bLoading = false;

  // initialise zip stream structure
  memset((void *)&zStream, 0, sizeof(zStream));

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
}


//==========================================================================
//
//  VZipStreamWriter::setRequireCrc
//
//==========================================================================
void VZipStreamWriter::setRequireCrc () {
  if (!bError && !doCrcCalc) {
    if (zStream.total_in == 0) doCrcCalc = true; else setError();
  }
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
