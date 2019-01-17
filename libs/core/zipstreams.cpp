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
    /*
    check(Initialised);
    inflateEnd(&ZStream);
    memset(&ZStream, 0, sizeof(ZStream));
    verify(inflateInit2(&ZStream, -MAX_WBITS) == Z_OK);
    SrcStream->Seek(srcStartPos);
    */
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
//  VZipStreamWriter::~VZipStreamWriter
//
//==========================================================================
VZipStreamWriter::~VZipStreamWriter () {
  Close();
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
