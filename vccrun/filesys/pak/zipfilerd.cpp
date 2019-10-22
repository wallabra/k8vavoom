//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**  Copyright (C) 2018 Ketmar Dark
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
#define MAX_WBITS 15
#define MAX_MEM_LEVEL 9

static const ISzAlloc fsysLzmaAlloc = {
  .Alloc = [](ISzAllocPtr p, size_t size) -> void * { void *res = ::malloc((int)size); if (!res) Sys_Error("out of memory!"); return res; },
  .Free = [](ISzAllocPtr p, void *addr) -> void { if (addr) ::free(addr); },
};


class VZipFileReader : public VStreamPakFile {
private:
  enum { UNZ_BUFSIZE = 16384 };
  //enum { UNZ_BUFSIZE = 65536 };

  mythread_mutex lock;
  VStream *fileStream; // source stream of the zipfile
  const VZipFileInfo &info; // info about the file we are reading

  vuint8 readBuffer[UNZ_BUFSIZE];//  Internal buffer for compressed data
  mz_stream stream;         //  ZLib stream structure for inflate
  CLzmaDec lzmastate;
  bool lzmainited;
  int lzmatotalout;
  vuint8 *lzmainbufpos;
  size_t lzmainbufleft;
  bool usezlib;
  int nextpos; // position we want
  int currpos; // position we are currently on

  vuint32 pos_in_zipfile; // position in byte on the zipfile
  vuint32 start_pos; // initial position, for restart
  bool stream_initialised; // flag set if stream structure is initialised

  vuint32 Crc32; // crc32 of all data uncompressed
  vuint32 rest_read_compressed; // number of byte to be decompressed
  vuint32 rest_read_uncompressed; // number of byte to be obtained after decomp

private:
  bool checkCurrentFileCoherencyHeader (vuint32 *, vuint32);
  bool lzmaRestart (); // `pos_in_zipfile` must be valid

  void setError ();

  // just read, no `nextpos` advancement
  // returns number of bytes read, -1 on error, or 0 on EOF
  int readSomeBytes (void *buf, int len);

public:
  VZipFileReader (VStream *InStream, vuint32 bytesBeforeZipFile, const VZipFileInfo &aInfo, FSysDriverBase *aDriver);
  virtual ~VZipFileReader () override;

  virtual VStr GetName () const override;
  virtual void Serialise (void*, int) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
};


VZipFileReader::VZipFileReader (VStream *InStream, vuint32 bytesBeforeZipFile, const VZipFileInfo &aInfo, FSysDriverBase *aDriver)
  : VStreamPakFile(aDriver)
  , fileStream(InStream)
  , info(aInfo)
  , lzmainited(false)
  , lzmatotalout(0)
  , lzmainbufpos(nullptr)
  , lzmainbufleft(0)
{
  mythread_mutex_init(&lock);
  MyThreadLocker locker(&lock);

  // open the file in the zip
  usezlib = true;
  nextpos = currpos = 0;

  vuint32 iSizeVar;
  if (!checkCurrentFileCoherencyHeader(&iSizeVar, bytesBeforeZipFile)) { bError = true; return; }

  stream_initialised = false;

  if (info.compression_method != Z_STORE && info.compression_method != MZ_DEFLATED && info.compression_method != Z_LZMA) {
    bError = true;
    //error->Logf("Compression method %d is not supported", info.compression_method);
    return;
  }

  Crc32 = 0;

  stream.total_out = 0;
  pos_in_zipfile = info.offset_curfile+SIZEZIPLOCALHEADER+iSizeVar+bytesBeforeZipFile;
  start_pos = pos_in_zipfile;
  rest_read_compressed = info.compressed_size;
  rest_read_uncompressed = info.uncompressed_size;

  if (info.compression_method == MZ_DEFLATED) {
    stream.zalloc = (mz_alloc_func)0;
    stream.zfree = (mz_free_func)0;
    stream.opaque = (void *)0;
    stream.next_in = (vuint8*)0;
    stream.avail_in = 0;

    int err = mz_inflateInit2(&stream, -MAX_WBITS);
    if (err != MZ_OK) {
     /* windowBits is passed < 0 to tell that there is no zlib header.
      * Note that in this case inflate *requires* an extra "dummy" byte
      * after the compressed stream in order to complete decompression and
      * return MZ_STREAM_END.
      * In unzip, i don't wait absolutely MZ_STREAM_END because I know the
      * size of both compressed and uncompressed data
      */
      bError = true;
      //error->Log("Failed to initialise inflate stream");
      return;
    }
    stream_initialised = true;
    //fprintf(stderr, "DBG: opened '%s' (deflate)\n", *aInfo.name);
  } else if (info.compression_method == Z_LZMA) {
    // LZMA
    usezlib = false;
    if (!lzmaRestart()) return; // error already set
    //fprintf(stderr, "DBG: opened '%s' (lzma)\n", *aInfo.name);
  }

  stream.avail_in = 0;
  bLoading = true;
}


VZipFileReader::~VZipFileReader () {
  Close();
  mythread_mutex_destroy(&lock);
}


VStr VZipFileReader::GetName () const {
  return info.name.cloneUnique();
}


void VZipFileReader::setError () {
  bError = true;
  if (usezlib) {
    if (stream_initialised) mz_inflateEnd(&stream);
  } else {
    if (lzmainited) { lzmainited = false; LzmaDec_Free(&lzmastate, &fsysLzmaAlloc); }
  }
  stream_initialised = false;
}


bool VZipFileReader::Close () {
  if (!bError && rest_read_uncompressed == 0) {
    if (Crc32 != info.crc) { bError = true; /*error->Log("Bad CRC");*/ }
  }
  if (usezlib) {
    if (stream_initialised) mz_inflateEnd(&stream);
  } else {
    if (lzmainited) { lzmainited = false; LzmaDec_Free(&lzmastate, &fsysLzmaAlloc); }
  }
  stream_initialised = false;
  return !bError;
}


// just read, no `nextpos` advancement
// returns number of bytes read, -1 on error, or 0 on EOF
int VZipFileReader::readSomeBytes (void *buf, int len) {
  stream.next_out = (vuint8 *)buf;
  stream.avail_out = len;
  vuint8 *lzmadest = (vuint8 *)buf;
  size_t lzmadestleft = (size_t)len;
  int res = 0;
  bool forceReadCPD = false;
  while ((usezlib ? (int)stream.avail_out : (int)lzmadestleft) > 0) {
    // read compressed data (if necessary)
    if ((usezlib ? (int)stream.avail_in : (int)lzmainbufleft) == 0 && (forceReadCPD || rest_read_compressed > 0)) {
      if (forceReadCPD && rest_read_compressed == 0) break;
      forceReadCPD = false;
      vuint32 uReadThis = UNZ_BUFSIZE;
      if (uReadThis> rest_read_compressed) uReadThis = rest_read_compressed;
      fileStream->Seek(pos_in_zipfile);
      fileStream->Serialise(readBuffer, uReadThis);
      if (fileStream->IsError()) {
        setError();
        return -1;
      }
      pos_in_zipfile += uReadThis;
      rest_read_compressed -= uReadThis;
      stream.next_in = readBuffer;
      stream.avail_in = uReadThis;
      lzmainbufpos = (vuint8 *)readBuffer;
      lzmainbufleft = (size_t)uReadThis;
    }

    // decompress data
    if (info.compression_method == Z_STORE) {
      // stored data
      if (stream.avail_in == 0 && rest_read_compressed == 0) break;
      int uDoCopy = (stream.avail_out < stream.avail_in ? stream.avail_out : stream.avail_in);
      memcpy(stream.next_out, stream.next_in, uDoCopy);
      Crc32 = mz_crc32(Crc32, stream.next_out, uDoCopy);
      rest_read_uncompressed -= uDoCopy;
      stream.avail_in -= uDoCopy;
      stream.avail_out -= uDoCopy;
      stream.next_out += uDoCopy;
      stream.next_in += uDoCopy;
      stream.total_out += uDoCopy;
      res += uDoCopy;
    } else if (info.compression_method == MZ_DEFLATED) {
      // zlib data
      int flush = MZ_SYNC_FLUSH;
      vuint32 uTotalOutBefore = stream.total_out;
      const vuint8 *bufBefore = stream.next_out;
      int err = mz_inflate(&stream, flush);
      if (err >= 0 && stream.msg != nullptr) {
#ifdef K8_UNLZMA_DEBUG
        fprintf(stderr, "ZIP: FAILED to read %d compressed bytes\n", len);
#endif
        setError();
        return -1;
      }
      vuint32 uTotalOutAfter = stream.total_out;
      vuint32 uOutThis = uTotalOutAfter-uTotalOutBefore;
      Crc32 = mz_crc32(Crc32, bufBefore, (vuint32)uOutThis);
      rest_read_uncompressed -= uOutThis;
      res += (vuint32)(uTotalOutAfter-uTotalOutBefore);
#ifdef K8_UNLZMA_DEBUG
      fprintf(stderr, "ZIP: read %d bytes (res=%d)\n", (int)(uTotalOutAfter-uTotalOutBefore), res);
      if (err != MZ_OK) fprintf(stderr, "ZIP: FAILED to read %d compressed bytes (err=%d)\n", len, err);
#endif
      if (err != MZ_OK) break;
    } else {
      // lzma data
      //vassert(lzmainbufleft > 0);
      ELzmaStatus status;
      size_t lzmainprocessed = lzmainbufleft;
      size_t lzmaouted = lzmadestleft;
      int lzmares = LzmaDec_DecodeToBuf(&lzmastate, lzmadest, &lzmaouted, lzmainbufpos, &lzmainprocessed, LZMA_FINISH_ANY, &status);
      vassert(lzmaouted <= lzmadestleft);
      vassert(lzmainprocessed <= lzmainbufleft);
      //GLog.Logf(NAME_Debug, "LZMA: wanted %u (%u packed bytes in buffer), read %u (%u packed bytes read)", (unsigned)lzmadestleft, (unsigned)lzmainbufleft, (unsigned)lzmaouted, (unsigned)lzmainprocessed);
      // calculate crc
      if (lzmaouted) Crc32 = mz_crc32(Crc32, lzmadest, (vuint32)lzmaouted);
      // move input pointer
      lzmainbufleft -= lzmainprocessed;
      lzmainbufpos += lzmainprocessed;
      // move output pointer
      lzmadestleft -= lzmaouted;
      lzmadest += lzmaouted;
      // bookkeeping
      rest_read_uncompressed -= (int)lzmaouted;
      lzmatotalout += (int)lzmaouted;
      res += (vuint32)lzmaouted;
      //GLog.Logf(NAME_Debug, "LZMA: res=%d; rest_read_uncompressed=%d; iRead=%d", (int)lzmares, rest_read_uncompressed, res);
      // check for errors
      if (lzmares != SZ_OK) break;
      //if (lzmaouted == 0 && lzmainbufleft == 0 && rest_read_compressed == 0) break;
      if (lzmaouted == 0 && lzmainbufleft == 0) forceReadCPD = true;
    }
  }
  return res;
}


// `pos_in_zipfile` must be valid
bool VZipFileReader::lzmaRestart () {
  vuint8 ziplzmahdr[4];
  vuint8 lzmaprhdr[5];

  if (usezlib) {
    setError();
    return false;
  }


  if (lzmainited) { lzmainited = false; LzmaDec_Free(&lzmastate, &fsysLzmaAlloc); }
  lzmatotalout = 0;
  lzmainbufpos = nullptr;
  lzmainbufleft = 0;
  Crc32 = 0;
  rest_read_uncompressed = info.uncompressed_size;
  rest_read_compressed = info.compressed_size;

  if (rest_read_compressed < 4+5) {
    bError = true;
    //fprintf(stderr, "DBG: Invalid lzma header (out of data)\n");
    return false;
  }

  fileStream->Seek(pos_in_zipfile);
  fileStream->Serialise(ziplzmahdr, 4);
  fileStream->Serialise(lzmaprhdr, 5);
  rest_read_compressed -= 4+5;

  if (fileStream->IsError()) {
    bError = true;
    //fprintf(stderr, "DBG: error reading lzma headers\n");
    return false;
  }

  if (ziplzmahdr[3] != 0 || ziplzmahdr[2] == 0 || ziplzmahdr[2] < 5) {
    bError = true;
    //fprintf(stderr, "DBG: Invalid lzma header (0)\n");
    return false;
  }

  if (ziplzmahdr[2] > 5) {
    vuint32 skip = ziplzmahdr[2]-5;
    if (rest_read_compressed < skip) {
      bError = true;
      //fprintf(stderr, "DBG: Invalid lzma header (out of extra data)\n");
      return false;
    }
    rest_read_compressed -= skip;
    vuint8 tmp;
    for (; skip > 0; --skip) {
      *fileStream << tmp;
      if (fileStream->IsError()) {
        bError = true;
        //fprintf(stderr, "DBG: error reading extra lzma headers\n");
        return false;
      }
    }
  }

  pos_in_zipfile = fileStream->Tell();
  if (fileStream->IsError()) {
    //fprintf(stderr, "DBG: error getting position in lzma restarter\n");
    bError = true;
    return false;
  }

  // header is: LZMA properties (5 bytes) and uncompressed size (8 bytes, little-endian)
  static_assert(LZMA_PROPS_SIZE == 5, "invalid LZMA properties size");
  static_assert(sizeof(vuint64) == 8, "invalid vuint64 size");

  vuint8 lzmaheader[LZMA_PROPS_SIZE+8];
  memcpy(lzmaheader, lzmaprhdr, LZMA_PROPS_SIZE);
  vuint64 *sizeptr = (vuint64 *)(lzmaheader+LZMA_PROPS_SIZE);
  *sizeptr = (vuint64)info.uncompressed_size;
  LzmaDec_Construct(&lzmastate);
  auto res = LzmaDec_Allocate(&lzmastate, lzmaheader, LZMA_PROPS_SIZE, &fsysLzmaAlloc);
  if (res != SZ_OK) {
    //fprintf(stderr, "DBG: error allocating lzma decoder\n");
    bError = true;
    return false;
  }
  LzmaDec_Init(&lzmastate);
  lzmainited = true;

  stream_initialised = true;

  return true;
}


// read the local header of the current zipfile
// check the coherency of the local header and info in the end of central directory about this file
// store in *piSizeVar the size of extra info in local header (filename and size of extra field data)
bool VZipFileReader::checkCurrentFileCoherencyHeader (vuint32 *piSizeVar, vuint32 byte_before_the_zipfile) {
  vuint32 Magic, DateTime, Crc, ComprSize, UncomprSize;
  vuint16 Version, Flags, ComprMethod, FileNameSize, ExtraFieldSize;

  *piSizeVar = 0;

  fileStream->Seek(info.offset_curfile+byte_before_the_zipfile);

  *fileStream
    << Magic
    << Version
    << Flags
    << ComprMethod
    << DateTime
    << Crc
    << ComprSize
    << UncomprSize
    << FileNameSize
    << ExtraFieldSize;

  if (fileStream->IsError()) return false;

  if (Magic != 0x04034b50) {
    //error->Log("Bad file magic");
    return false;
  }

  if (ComprMethod != info.compression_method) {
    //error->Log("Compression method doesn\'t match");
    return false;
  }

  if (Crc != info.crc && (Flags&8) == 0) {
    //error->Log("CRC doesn\'t match");
    return false;
  }

  if (ComprSize != info.compressed_size && (Flags&8) == 0) {
    //error->Log("Compressed size doesn\'t match");
    return false;
  }

  if (UncomprSize != info.uncompressed_size && (Flags&8) == 0) {
    //error->Log("Uncompressed size doesn\'t match");
    return false;
  }

  if (FileNameSize != info.size_filename) {
    //error->Log("File name length doesn\'t match");
    return false;
  }

  *piSizeVar += FileNameSize+ExtraFieldSize;

  return true;
}


void VZipFileReader::Serialise (void* buf, int len) {
  if (bError) return; // don't read anything from already broken stream
  MyThreadLocker locker(&lock);
  if (len < 0) {
#ifdef K8_UNLZMA_DEBUG
    fprintf(stderr, "LZMA: FAILED to read %d bytes\n", len);
#endif
    setError();
    return;
  }
  if (fileStream->IsError()) {
#ifdef K8_UNLZMA_DEBUG
    fprintf(stderr, "LZMA: FAILED to read %d bytes (underlying stream is fucked)\n", len);
#endif
    setError();
    return;
  }
  if (len == 0) return;
  if (!buf) {
#ifdef K8_UNLZMA_DEBUG
    fprintf(stderr, "LZMA: FAILED to read %d bytes (null buffer)\n", len);
#endif
    setError();
    return;
  }

  if (currpos > nextpos) {
    // rewind stream
    //fprintf(stderr, "DBG: rewinding '%s' (currpos=%d; nextpos=%d)\n", *info.name, currpos, nextpos);
    Crc32 = 0;
    currpos = 0;
    rest_read_compressed = info.compressed_size;
    rest_read_uncompressed = info.uncompressed_size;
    pos_in_zipfile = start_pos;
    if (info.compression_method == MZ_DEFLATED) {
      vassert(stream_initialised);
      vassert(usezlib);
      if (stream_initialised) { mz_inflateEnd(&stream); stream_initialised = false; }
      memset(&stream, 0, sizeof(stream));
      if (mz_inflateInit2(&stream, -MAX_WBITS) != MZ_OK) { bError = true; return; }
      stream_initialised = true;
    } else if (info.compression_method == Z_LZMA) {
      //fprintf(stderr, "DBG: rewind '%s' (lzma)\n", *info.name);
      vassert(stream_initialised);
      vassert(!usezlib);
      if (!lzmaRestart()) return; // error already set
    } else {
      memset(&stream, 0, sizeof(stream));
    }
  }

  while (currpos < nextpos) {
    char tmpbuf[256];
    int toread = nextpos-currpos;
    if (toread > 256) toread = 256;
    int rd = readSomeBytes(tmpbuf, toread);
    if (rd <= 0) {
      setError();
      return;
    }
    currpos += rd;
  }

  // just in case
  if (nextpos != currpos) {
#ifdef K8_UNLZMA_DEBUG
    fprintf(stderr, "LZMA: FAILED to read %d bytes (skipper fucked; nextpos=%d; currpos=%d)\n", len, nextpos, currpos);
#endif
    setError();
    return;
  }

  vuint8 *dest = (vuint8 *)buf;
  while (len > 0) {
    int rd = readSomeBytes(dest, len);
    if (rd <= 0) {
#ifdef K8_UNLZMA_DEBUG
      fprintf(stderr, "LZMA: read error (rd=%d)\n", rd);
#endif
      setError();
      return;
    }
    len -= rd;
    nextpos = (currpos += rd);
    dest += rd;
  }

  if (rest_read_uncompressed == 0 && currpos == (int)info.uncompressed_size && Crc32 != info.crc) {
#ifdef K8_UNLZMA_DEBUG
    fprintf(stderr, "LZMA: FAILED to read %d bytes (CRC fucked; got: 0x%08x; expected: 0x%08x)\n", len, Crc32, info.crc);
#endif
    setError();
    return;
  }
#ifdef K8_UNLZMA_DEBUG
  if (rest_read_uncompressed == 0 && currpos == (int)info.uncompressed_size && Crc32 == info.crc) { fprintf(stderr, "ZIP CRC CHECK: OK\n"); }
#endif
}


void VZipFileReader::Seek (int pos) {
  if (bError) return;
  if (pos < 0) pos = 0;
  if (pos > (int)info.uncompressed_size) pos = (int)info.uncompressed_size;
  nextpos = pos;
}


int VZipFileReader::Tell () {
  //return (usezlib ? stream.total_out : lzmastream.total_out);
  return nextpos;
}


int VZipFileReader::TotalSize () {
  return info.uncompressed_size;
}


bool VZipFileReader::AtEnd () {
  return (bError || /*rest_read_uncompressed == 0*/(vuint32)nextpos >= info.uncompressed_size);
}
