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
//**
//**  Based on sources from zlib with following notice:
//**
//**  Copyright (C) 1998-2004 Gilles Vollant
//**
//**  This software is provided 'as-is', without any express or implied
//**  warranty.  In no event will the authors be held liable for any damages
//**  arising from the use of this software.
//**
//**  Permission is granted to anyone to use this software for any purpose,
//**  including commercial applications, and to alter it and redistribute it
//**  freely, subject to the following restrictions:
//**
//**  1. The origin of this software must not be misrepresented; you must
//**  not claim that you wrote the original software. If you use this
//**  software in a product, an acknowledgment in the product documentation
//**  would be appreciated but is not required.
//**  2. Altered source versions must be plainly marked as such, and must
//**  not be misrepresented as being the original software.
//**  3. This notice may not be removed or altered from any source
//**  distribution.
//**
//**************************************************************************
// directly included from "fsys_zip.cpp"

#ifndef VAVOOM_USE_LIBLZMA
static const ISzAlloc fsysLzmaAlloc = {
  .Alloc = [](ISzAllocPtr p, size_t size) -> void * { return Z_Malloc((int)size); },
  .Free = [](ISzAllocPtr p, void *addr) -> void { Z_Free(addr); },
};
#endif

// ////////////////////////////////////////////////////////////////////////// //
class VZipFileReader : public VStream {
private:
  //enum { UNZ_BUFSIZE = 16384 };
  enum { UNZ_BUFSIZE = 65536 };

  mythread_mutex *rdlock;
  VStream *FileStream; // source stream of the zipfile
  VStr fname;
  const VPakFileInfo &Info; // info about the file we are reading

  Bytef ReadBuffer[UNZ_BUFSIZE]; // internal buffer for compressed data
  z_stream stream; // zlib stream structure for inflate
  #ifdef VAVOOM_USE_LIBLZMA
  lzma_stream lzmastream;
  lzma_options_lzma *lzmaopts;
  lzma_filter filters[2];
  #else
  CLzmaDec lzmastate;
  bool lzmainited;
  int lzmatotalout;
  vuint8 *lzmainbufpos;
  size_t lzmainbufleft;
  #endif
  bool usezlib;

  vuint32 pos_in_zipfile; // position in byte on the zipfile
  vuint32 start_pos; // initial position, for restart
  bool stream_initialised; // flag set if stream structure is initialised

  vuint32 Crc32; // crc32 of all data uncompressed
  vuint32 rest_read_compressed; // number of byte to be decompressed
  vuint32 rest_read_uncompressed; // number of byte to be obtained after decomp

  // on second back-seek, read the whole data into this buffer, and use it
  vuint8 *wholeBuf;
  vint32 wholeSize; // this is abused as back-seek counter: -2 means none, -1 means "one issued"
  int currpos; // used only for cached data

private:
  void setError ();
  bool CheckCurrentFileCoherencyHeader (vuint32 *, vuint32); // does locking
  bool LzmaRestart (); // `pos_in_zipfile` must be valid; does locking
  void readBytes (void *buf, int length); // does locking, if necessary
  void cacheAllData ();

  #ifdef VAVOOM_USE_LIBLZMA
  inline int lzmaGetTotalOut () const { return (int)lzmastream.total_out; }
  inline bool lzmaIsInStreamEmpty () const { return (lzmastream.avail_in == 0); }
  #else
  inline int lzmaGetTotalOut () const { return lzmatotalout; }
  inline bool lzmaIsInStreamEmpty () const { return (lzmainbufleft == 0); }
  #endif

public:
  VZipFileReader (VStr afname, VStream *, vuint32, const VPakFileInfo &, mythread_mutex *ardlock);
  virtual ~VZipFileReader () override;
  virtual VStr GetName () const override;
  virtual void Serialise (void *, int) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
};


//==========================================================================
//
//  VZipFileReader::VZipFileReader
//
//==========================================================================
VZipFileReader::VZipFileReader (VStr afname, VStream *InStream, vuint32 BytesBeforeZipFile,
                                const VPakFileInfo &aInfo, mythread_mutex *ardlock)
  : rdlock(ardlock)
  , FileStream(InStream)
  , fname(afname)
  , Info(aInfo)
  #ifdef VAVOOM_USE_LIBLZMA
  , lzmaopts(nullptr)
  #else
  , lzmainited(false)
  , lzmatotalout(0)
  , lzmainbufpos(nullptr)
  , lzmainbufleft(0)
  #endif
  , wholeBuf(nullptr)
  , wholeSize(-2)
  , currpos(0)
{
  // open the file in the zip
  // `rdlock` is not locked here
  usezlib = true;

  if (!rdlock) Sys_Error("VZipFileReader::VZipFileReader: empty lock!");

  vuint32 iSizeVar;
  if (!CheckCurrentFileCoherencyHeader(&iSizeVar, BytesBeforeZipFile)) {
    bError = true;
    return;
  }

  stream_initialised = false;
  #ifdef VAVOOM_USE_LIBLZMA
  lzmastream = LZMA_STREAM_INIT;
  #endif

  if (Info.compression != Z_STORE && Info.compression != Z_DEFLATED && Info.compression != Z_LZMA) {
    bError = true;
    GLog.Logf(NAME_Error, "Compression method %d is not supported for file '%s'", Info.compression, *afname);
    return;
  }

  Crc32 = 0;

  stream.total_out = 0;
  #ifdef VAVOOM_USE_LIBLZMA
  lzmastream.total_out = 0;
  #endif
  pos_in_zipfile = Info.pakdataofs+SIZEZIPLOCALHEADER+iSizeVar+BytesBeforeZipFile;
  start_pos = pos_in_zipfile;
  rest_read_compressed = Info.packedsize;
  rest_read_uncompressed = Info.filesize;

  if (Info.compression == Z_DEFLATED) {
    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;
    stream.opaque = (voidpf)0;
    stream.next_in = (Bytef*)0;
    stream.avail_in = 0;

    int err = inflateInit2(&stream, -MAX_WBITS);
    if (err != Z_OK) {
      /* windowBits is passed < 0 to tell that there is no zlib header.
       * Note that in this case inflate *requires* an extra "dummy" byte
       * after the compressed stream in order to complete decompression and
       * return Z_STREAM_END.
       * In unzip, i don't wait absolutely Z_STREAM_END because I known the
       * size of both compressed and uncompressed data
       */
      bError = true;
      GLog.Logf(NAME_Error, "Failed to initialise inflate stream for file '%s'", *fname);
      return;
    }
    stream_initialised = true;
  } else if (Info.compression == Z_LZMA) {
    // LZMA
    usezlib = false;
    if (!LzmaRestart()) return; // error already set
  }

  stream.avail_in = 0;
  #ifdef VAVOOM_USE_LIBLZMA
  lzmastream.avail_in = 0;
  #endif
  bLoading = true;
  //GLog.Logf("***LOADING '%s'", *fname);
}


//==========================================================================
//
//  VZipFileReader::~VZipFileReader
//
//==========================================================================
VZipFileReader::~VZipFileReader() {
  Close();
}


//==========================================================================
//
//  VZipFileReader::GetName
//
//==========================================================================
VStr VZipFileReader::GetName () const {
  return fname.cloneUnique();
}


//==========================================================================
//
//  VZipFileReader::setError
//
//==========================================================================
void VZipFileReader::setError () {
  if (wholeBuf) { Z_Free(wholeBuf); wholeBuf = nullptr; }
  wholeSize = -2;
  if (stream_initialised) {
    if (usezlib) {
      inflateEnd(&stream);
    } else {
      #ifdef VAVOOM_USE_LIBLZMA
      lzma_end(&lzmastream);
      #else
      if (lzmainited) { lzmainited = false; LzmaDec_Free(&lzmastate, &fsysLzmaAlloc); }
      #endif
    }
    stream_initialised = false;
  }
  bError = true;
}


//==========================================================================
//
//  VZipFileReader::Close
//
//==========================================================================
bool VZipFileReader::Close () {
  if (wholeBuf) { Z_Free(wholeBuf); wholeBuf = nullptr; }
  wholeSize = -2;

  if (stream_initialised) {
    //GLog.Logf("***CLOSING '%s'", *fname);
    /*
    if (!bError && rest_read_uncompressed == 0) {
      if (Crc32 != Info.crc32) {
        bError = true;
        GLog.Logf("Bad CRC for file '%s'", *fname);
      }
    }
    */
    if (usezlib) {
      inflateEnd(&stream);
    } else {
      #ifdef VAVOOM_USE_LIBLZMA
      lzma_end(&lzmastream);
      #else
      if (lzmainited) { lzmainited = false; LzmaDec_Free(&lzmastate, &fsysLzmaAlloc); }
      #endif
    }
    stream_initialised = false;
  }

  #ifdef VAVOOM_USE_LIBLZMA
  if (lzmaopts) { free(lzmaopts); lzmaopts = nullptr; }
  #endif

  return !bError;
}


//==========================================================================
//
//  VZipFileReader::LzmaRestart
//
//  `pos_in_zipfile` must be valid
//
//==========================================================================
bool VZipFileReader::LzmaRestart () {
  vuint8 ziplzmahdr[4];
  vuint8 lzmaprhdr[5];

  if (usezlib) {
    bError = true;
    GLog.Logf(NAME_Error, "Cannot lzma-restart non-lzma stream for file '%s'", *fname);
    return false;
  }

  #ifdef VAVOOM_USE_LIBLZMA
  if (stream_initialised) { lzma_end(&lzmastream); stream_initialised = false; }
  lzmastream = LZMA_STREAM_INIT;
  lzmastream.total_out = 0;
  #else
  if (lzmainited) { lzmainited = false; LzmaDec_Free(&lzmastate, &fsysLzmaAlloc); }
  lzmatotalout = 0;
  #endif
  rest_read_uncompressed = Info.filesize;
  vensure(Crc32 == 0);

  if (rest_read_compressed < 4+5) {
    bError = true;
    GLog.Logf(NAME_Error, "Invalid lzma header (out of data) for file '%s'", *fname);
    return false;
  }

  {
    MyThreadLocker locker(rdlock);

    FileStream->Seek(pos_in_zipfile);
    FileStream->Serialise(ziplzmahdr, 4);
    FileStream->Serialise(lzmaprhdr, 5);
    bool err = FileStream->IsError();
    rest_read_compressed -= 4+5;

    if (err) {
      bError = true;
      GLog.Logf(NAME_Error, "Error reading lzma headers for file '%s'", *fname);
      return false;
    }

    if (ziplzmahdr[3] != 0 || ziplzmahdr[2] == 0 || ziplzmahdr[2] < 5) {
      bError = true;
      GLog.Logf(NAME_Error, "Invalid lzma header (0) for file '%s'", *fname);
      return false;
    }

    if (ziplzmahdr[2] > 5) {
      vuint32 skip = ziplzmahdr[2]-5;
      if (rest_read_compressed < skip) {
        bError = true;
        GLog.Logf(NAME_Error, "Invalid lzma header (out of extra data) for file '%s'", *fname);
        return false;
      }
      rest_read_compressed -= skip;
      vuint8 tmp;
      for (; skip > 0; --skip) {
        *FileStream << tmp;
        if (FileStream->IsError()) {
          bError = true;
          GLog.Logf(NAME_Error, "Error reading extra lzma headers for file '%s'", *fname);
          return false;
        }
      }
    }

    pos_in_zipfile = FileStream->Tell();
    if (FileStream->IsError()) {
      bError = true;
      GLog.Logf(NAME_Error, "Error reiniting lzma stream in file '%s'", *fname);
      return false;
    }
  }

#ifdef K8_UNLZMA_DEBUG
  GLog.Logf(NAME_Debug, "LZMA: %u bytes in header, pksize=%d, unpksize=%d", (unsigned)(FileStream->Tell()-pos_in_zipfile), (int)Info.packedsize, (int)Info.filesize);
#endif

  #ifdef VAVOOM_USE_LIBLZMA
  //lzma_lzma_preset(&lzmaopts, 9|LZMA_PRESET_EXTREME);
  if (lzmaopts) { free(lzmaopts); lzmaopts = nullptr; }
  filters[0].id = LZMA_FILTER_LZMA1;
  //filters[0].options = &lzmaopts;
  filters[0].options = nullptr;
  filters[1].id = LZMA_VLI_UNKNOWN;

  vuint32 prpsize;
  if (lzma_properties_size(&prpsize, &filters[0]) != LZMA_OK) {
    bError = true;
    GLog.Logf(NAME_Error, "Failed to initialise lzma stream for file '%s'", *fname);
    return false;
  }
  if (prpsize != 5) {
    bError = true;
    GLog.Logf(NAME_Error, "Failed to initialise lzma stream for file '%s'", *fname);
    return false;
  }

  if (lzma_properties_decode(&filters[0], nullptr, lzmaprhdr, prpsize) != LZMA_OK) {
    lzmaopts = (lzma_options_lzma *)filters[0].options;
    bError = true;
    GLog.Logf(NAME_Error, "Failed to initialise lzma stream for file '%s'", *fname);
    return false;
  }
  lzmaopts = (lzma_options_lzma *)filters[0].options;

  if (lzma_raw_decoder(&lzmastream, &filters[0]) != LZMA_OK) {
    bError = true;
    GLog.Logf(NAME_Error, "Failed to initialise lzma stream for file '%s'", *fname);
    return false;
  }
  #else
  // header is: LZMA properties (5 bytes) and uncompressed size (8 bytes, little-endian)
  static_assert(LZMA_PROPS_SIZE == 5, "invalid LZMA properties size");
  static_assert(sizeof(vuint64) == 8, "invalid vuint64 size");
  vuint8 lzmaheader[LZMA_PROPS_SIZE+8];
  memcpy(lzmaheader, lzmaprhdr, LZMA_PROPS_SIZE);
  vuint64 *sizeptr = (vuint64 *)(lzmaheader+LZMA_PROPS_SIZE);
  *sizeptr = (vuint64)Info.filesize;
  LzmaDec_Construct(&lzmastate);
  auto res = LzmaDec_Allocate(&lzmastate, lzmaheader, LZMA_PROPS_SIZE, &fsysLzmaAlloc);
  if (res != SZ_OK) {
    bError = true;
    GLog.Logf(NAME_Error, "Failed to initialise lzma stream for file '%s'", *fname);
    return false;
  }
  LzmaDec_Init(&lzmastate);
  lzmainited = true;
  #endif

  stream_initialised = true;

  return true;
}


//==========================================================================
//
//  VZipFileReader::CheckCurrentFileCoherencyHeader
//
//  Read the local header of the current zipfile.
//  Check the coherency of the local header and info in the end of central
//  directory about this file.
//  Store in *piSizeVar the size of extra info in local header
//  (filename and size of extra field data).
//
//==========================================================================
bool VZipFileReader::CheckCurrentFileCoherencyHeader (vuint32 *piSizeVar, vuint32 byte_before_the_zipfile) {
  vuint32 Magic, DateTime, Crc, ComprSize, UncomprSize;
  vuint16 Version, Flags, ComprMethod, FileNameSize, ExtraFieldSize;

  *piSizeVar = 0;

  bool err = false;
  {
    MyThreadLocker locker(rdlock);
    FileStream->Seek(Info.pakdataofs+byte_before_the_zipfile);

    *FileStream
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

    err = FileStream->IsError();
  }

  if (err) {
    GLog.Logf(NAME_Error, "Error reading archive for file '%s'", *fname);
    return false;
  }

  if (Magic != 0x04034b50) {
    GLog.Logf(NAME_Error, "Bad file magic for file '%s'", *fname);
    return false;
  }

  if (ComprMethod != Info.compression) {
    GLog.Logf(NAME_Error, "Compression method doesn't match for file '%s'", *fname);
    return false;
  }

  if (Crc != Info.crc32 && (Flags&8) == 0) {
    GLog.Logf(NAME_Error, "CRC doesn't match for file '%s'", *fname);
    return false;
  }

  if (ComprSize != Info.packedsize && (Flags&8) == 0) {
    GLog.Logf(NAME_Error, "Compressed size doesn't match for file '%s'", *fname);
    return false;
  }

  if (UncomprSize != (vuint32)Info.filesize && (Flags&8) == 0) {
    GLog.Logf(NAME_Error, "Uncompressed size doesn't match for file '%s'", *fname);
    return false;
  }

  if (FileNameSize != Info.filenamesize) {
    GLog.Logf(NAME_Error, "File name length doesn't match for file '%s'", *fname);
    return false;
  }

  *piSizeVar += FileNameSize+ExtraFieldSize;

  return true;
}


//==========================================================================
//
//  VZipFileReader::readBytes
//
//  prerequisites: stream must be valid, initialized, and not cached
//
//==========================================================================
void VZipFileReader::readBytes (void *buf, int length) {
  vassert(length >= 0);
  if (length == 0) return;

  if ((vuint32)length > rest_read_uncompressed) {
    //GLog.Logf(NAME_Error, "Want to read past the end of the file '%s': rest=%d; length=%d", *GetName(), (vint32)rest_read_uncompressed, length);
    setError();
    return;
  }

  stream.next_out = (Bytef *)buf;
  stream.avail_out = length;
  #ifdef VAVOOM_USE_LIBLZMA
  lzmastream.next_out = (Bytef *)buf;
  lzmastream.avail_out = length;
  #define VZFR_LZMA_LEFT  lzmastream.avail_out
  #else
  vuint8 *lzmadest = (vuint8 *)buf;
  size_t lzmadestleft = (size_t)length;
  #define VZFR_LZMA_LEFT  lzmadestleft
  #endif

  int iRead = 0;
  while ((usezlib ? (int)stream.avail_out : (int)VZFR_LZMA_LEFT) > 0) {
    // read compressed data (if necessary)
    if ((usezlib ? (stream.avail_in == 0) : lzmaIsInStreamEmpty()) && rest_read_compressed > 0) {
      vuint32 uReadThis = UNZ_BUFSIZE;
      if (uReadThis > rest_read_compressed) uReadThis = rest_read_compressed;
#ifdef K8_UNLZMA_DEBUG
      if (!usezlib) GLog.Logf(NAME_Debug, "LZMA: reading compressed bytes from ofs %u (want=%d; left=%d)", (unsigned)(pos_in_zipfile-start_pos), (int)uReadThis, (int)rest_read_compressed);
#endif
      bool err = false;
      {
        MyThreadLocker locker(rdlock);
        err = FileStream->IsError();
        if (!err) {
          FileStream->Seek(pos_in_zipfile);
          FileStream->Serialise(ReadBuffer, uReadThis);
          err = FileStream->IsError();
        }
      }
      if (err) {
        bError = true;
        GLog.Logf(NAME_Error, "Failed to read from zip for file '%s'", *fname);
        return;
      }
#ifdef K8_UNLZMA_DEBUG
      if (!usezlib) GLog.Logf(NAME_Debug, "LZMA: read %d compressed bytes", uReadThis);
#endif
      //GLog.Logf(NAME_Debug, "Read %u packed bytes from '%s' (%u left)", uReadThis, *GetName(), rest_read_compressed-uReadThis);
      pos_in_zipfile += uReadThis;
      rest_read_compressed -= uReadThis;
      stream.next_in = ReadBuffer;
      stream.avail_in = uReadThis;
      #ifdef VAVOOM_USE_LIBLZMA
      lzmastream.next_in = ReadBuffer;
      lzmastream.avail_in = uReadThis;
      #else
      lzmainbufpos = (vuint8 *)ReadBuffer;
      lzmainbufleft = (size_t)uReadThis;
      #endif
    } else {
      if (rest_read_compressed == 0 && (usezlib ? (stream.avail_in == 0) : lzmaIsInStreamEmpty())) break;
    }

    // decompress data
    if (Info.compression == Z_STORE) {
      // stored data
      //if (stream.avail_in == 0 && rest_read_compressed == 0) break;
      int uDoCopy = (stream.avail_out < stream.avail_in ? stream.avail_out : stream.avail_in);
      vassert(uDoCopy > 0);
      //for (int i = 0; i < uDoCopy; ++i) *(stream.next_out+i) = *(stream.next_in+i);
      memcpy(stream.next_out, stream.next_in, uDoCopy);
      Crc32 = crc32(Crc32, stream.next_out, uDoCopy);
      rest_read_uncompressed -= uDoCopy;
      stream.avail_in -= uDoCopy;
      stream.avail_out -= uDoCopy;
      stream.next_out += uDoCopy;
      stream.next_in += uDoCopy;
      stream.total_out += uDoCopy;
      iRead += uDoCopy;
    } else if (Info.compression == Z_DEFLATED) {
      // zlib data
      int flush = Z_SYNC_FLUSH;
      uLong uTotalOutBefore = stream.total_out;
      const Bytef *bufBefore = stream.next_out;
      int err = inflate(&stream, flush);
      if (err >= 0 && stream.msg != nullptr) {
        setError();
        GLog.Logf(NAME_Error, "Decompression failed: %s for file '%s'", stream.msg, *fname);
        return;
      }
      uLong uTotalOutAfter = stream.total_out;
      vuint32 uOutThis = uTotalOutAfter-uTotalOutBefore;
      Crc32 = crc32(Crc32, bufBefore, (uInt)uOutThis);
      rest_read_uncompressed -= uOutThis;
      iRead += (uInt)(uTotalOutAfter-uTotalOutBefore);
      if (err != Z_OK) break;
    } else {
      // lzma data
      #ifdef VAVOOM_USE_LIBLZMA
        #ifdef K8_UNLZMA_DEBUG
        GLog.Logf(NAME_Debug, "LZMA: processing %u compressed bytes into %u uncompressed bytes", (unsigned)lzmastream.avail_in, (unsigned)lzmastream.avail_out);
        auto inbefore = lzmastream.avail_in;
        #endif
        auto outbefore = lzmastream.avail_out;
        const Bytef *bufBefore = lzmastream.next_out;
        int err = lzma_code(&lzmastream, LZMA_RUN);
        if (err != LZMA_OK && err != LZMA_STREAM_END) {
          setError();
          GLog.Logf(NAME_Error, "LZMA decompression failed (%d) for file '%s'", err, *fname);
          return;
        }
        vuint32 uOutThis = outbefore-lzmastream.avail_out;
        #ifdef K8_UNLZMA_DEBUG
        GLog.Logf(NAME_Debug, "LZMA: processed %u packed bytes, unpacked %u bytes (err=%d); (want %d, got so far %d, left %d : %d)", (unsigned)(inbefore-lzmastream.avail_in), uOutThis, err, length, iRead, length-iRead, length-iRead-uOutThis);
        #endif
        Crc32 = crc32(Crc32, bufBefore, (uInt)uOutThis);
        rest_read_uncompressed -= uOutThis;
        iRead += (uInt)uOutThis; //(uInt)(uTotalOutAfter - uTotalOutBefore);
        if (err != LZMA_OK) {
          #ifdef K8_UNLZMA_DEBUG
          GLog.Logf(NAME_Debug, "LZMA: stream end");
          #endif
          break;
        }
      #else
        //auto inbefore = lzmainbufleft;
        vassert(lzmainbufleft > 0);
        ELzmaStatus status;
        size_t lzmainprocessed = lzmainbufleft;
        size_t lzmaouted = lzmadestleft;
        int lzmares = LzmaDec_DecodeToBuf(&lzmastate, lzmadest, &lzmaouted, lzmainbufpos, &lzmainprocessed, LZMA_FINISH_ANY, &status);
        vassert(lzmaouted <= lzmadestleft);
        vassert(lzmainprocessed <= lzmainbufleft);
        //GLog.Logf(NAME_Debug, "LZMA: wanted %u (%u packed bytes in buffer), read %u (%u packed bytes read)", (unsigned)lzmadestleft, (unsigned)lzmainbufleft, (unsigned)lzmaouted, (unsigned)lzmainprocessed);
        // calculate crc
        if (lzmaouted) Crc32 = crc32(Crc32, lzmadest, (uInt)lzmaouted);
        // move input pointer
        lzmainbufleft -= lzmainprocessed;
        lzmainbufpos += lzmainprocessed;
        // move output pointer
        lzmadestleft -= lzmaouted;
        lzmadest += lzmaouted;
        // bookkeeping
        rest_read_uncompressed -= (int)lzmaouted;
        lzmatotalout += (int)lzmaouted;
        iRead += (uInt)lzmaouted;
        //GLog.Logf(NAME_Debug, "LZMA: res=%d; rest_read_uncompressed=%d; iRead=%d", (int)lzmares, rest_read_uncompressed, iRead);
        // check for errors
        if (lzmares != SZ_OK) {
          #ifdef K8_UNLZMA_DEBUG
          GLog.Log(NAME_Debug, "LZMA: stream end");
          #endif
          break;
        }
      #endif
    }
  }

  if (iRead != length) {
    GLog.Logf(NAME_Error, "Only read %d of %d bytes for file '%s'", iRead, length, *fname);
    if (!bError) setError();
  } else if (!bError && rest_read_uncompressed == 0) {
    if (Crc32 != Info.crc32) {
      setError();
      GLog.Logf(NAME_Error, "Bad CRC for file '%s' (wanted 0x%08x, got 0x%08x)", *fname, (unsigned)Info.crc32, (unsigned)Crc32);
    }
  }
}


//==========================================================================
//
//  VZipFileReader::Serialise
//
//==========================================================================
void VZipFileReader::Serialise (void *V, int length) {
  if (bError) return;
  if (length < 0) { setError(); return; }
  if (length == 0) return;

  if (!V) {
    setError();
    GLog.Logf(NAME_Error, "Cannot read into nullptr buffer for file '%s'", *fname);
    return;
  }

  // use data cache?
  if (wholeSize >= 0) {
    //GLog.Logf(NAME_Debug, "  ***READING '%s' (currpos=%d; size=%d; left=%d; len=%d)", *fname, currpos, wholeSize, wholeSize-currpos, length);
    if (length < 0 || currpos < 0 || currpos >= wholeSize || wholeSize-currpos < length) setError();
    if (bError) return;
    memcpy(V, wholeBuf+currpos, length);
    currpos += length;
  } else {
    if (!FileStream) { setError(); return; }
    if ((vuint32)length > rest_read_uncompressed) { setError(); return; }
    readBytes(V, length);
  }
}


//==========================================================================
//
//  VZipFileReader::cacheAllData
//
//==========================================================================
void VZipFileReader::cacheAllData () {
  //GLog.Logf(NAME_Debug, "Back-seek in '%s' (curr=%d; new=%d)", *GetName(), Tell(), InPos);
  Crc32 = 0;
  rest_read_compressed = Info.packedsize;
  rest_read_uncompressed = Info.filesize;
  pos_in_zipfile = start_pos;
  if (Info.compression == Z_DEFLATED) {
    vassert(stream_initialised);
    vassert(usezlib);
    if (stream_initialised) inflateEnd(&stream);
    memset(&stream, 0, sizeof(stream));
    vensure(inflateInit2(&stream, -MAX_WBITS) == Z_OK);
  } else if (Info.compression == Z_LZMA) {
#ifdef K8_UNLZMA_DEBUG
    GLog.Logf(NAME_Debug, "LZMA: seek to %d (now at %d)", InPos, Tell());
#endif
    vassert(stream_initialised);
    vassert(!usezlib);
    if (!LzmaRestart()) return; // error already set
  } else {
    memset(&stream, 0, sizeof(stream));
  }
  // cache data
  wholeSize = (vint32)Info.filesize;
  //GLog.Logf(NAME_Debug, "*** CACHING '%s' (cpos=%d; newpos=%d; size=%d)", *GetName(), cpos, InPos, wholeSize);
  if (wholeSize < 0) { setError(); return; }
  wholeBuf = (vuint8 *)Z_Malloc(wholeSize ? wholeSize : 1);
  readBytes(wholeBuf, wholeSize);
  //GLog.Logf(NAME_Debug, "*** CACHED: %s", (bError ? "error" : "ok"));
}


//==========================================================================
//
//  VZipFileReader::Seek
//
//==========================================================================
void VZipFileReader::Seek (int InPos) {
  vassert(InPos >= 0);
  vassert(InPos <= (int)Info.filesize);

  if (bError) return;

  if (wholeSize >= 0) {
    //GLog.Logf(NAME_Debug, "  ***SEEKING '%s' (currpos=%d; size=%d; newpos=%d)", *fname, currpos, wholeSize, InPos);
    currpos = InPos;
    return;
  }

  // if seeking backwards, reset input stream to the begining of the file
  const int cpos = Tell();
  if (InPos < cpos) {
    // check if we have to cache data
    ++wholeSize;
    if (wholeSize >= 0) {
      //GLog.Logf(NAME_Debug, "*** (0)CACHING '%s' (cpos=%d; newpos=%d; size=%u)", *GetName(), cpos, InPos, Info.filesize);
      cacheAllData();
      currpos = InPos;
      return;
    }
    //GLog.Logf(NAME_Debug, "Back-seek in '%s' (curr=%d; new=%d)", *GetName(), Tell(), InPos);
    Crc32 = 0;
    rest_read_compressed = Info.packedsize;
    rest_read_uncompressed = Info.filesize;
    pos_in_zipfile = start_pos;
    if (Info.compression == Z_DEFLATED) {
      vassert(stream_initialised);
      vassert(usezlib);
      if (stream_initialised) inflateEnd(&stream);
      memset(&stream, 0, sizeof(stream));
      vensure(inflateInit2(&stream, -MAX_WBITS) == Z_OK);
    } else if (Info.compression == Z_LZMA) {
#ifdef K8_UNLZMA_DEBUG
      GLog.Logf(NAME_Debug, "LZMA: seek to %d (now at %d)", InPos, Tell());
#endif
      vassert(stream_initialised);
      vassert(!usezlib);
      if (!LzmaRestart()) return; // error already set
    } else {
      memset(&stream, 0, sizeof(stream));
    }
  } else {
    // cache file if we're seekint near the end
    if (cpos < 32768 && InPos >= (vint32)(Info.filesize-Info.filesize/3)) {
      ++wholeSize;
      if (wholeSize >= 0) {
        //GLog.Logf(NAME_Debug, "*** (1)CACHING '%s' (cpos=%d; newpos=%d; size=%u)", *GetName(), cpos, InPos, Info.filesize);
        cacheAllData();
        currpos = InPos;
        return;
      }
    }
  }

  // read data into a temporary buffer untill we reach needed position
  int ToSkip = InPos-Tell();
  vassert(ToSkip >= 0);
  if (ToSkip > 0) {
    int bsz = ToSkip;
    if (bsz > 65536) bsz = 65536;
    vuint8 tmpbuf[512];
    vuint8 *tmpbufptr;
    if (bsz <= (int)sizeof(tmpbuf)) {
      tmpbufptr = tmpbuf;
    } else {
      vassert(!wholeBuf);
      wholeBuf = (vuint8 *)Z_Malloc(bsz);
      tmpbufptr = wholeBuf;
    }
    while (ToSkip > 0) {
      int Count = (ToSkip > bsz ? bsz : ToSkip);
      ToSkip -= Count;
      Serialise(tmpbufptr, Count);
    }
    if (wholeBuf) { Z_Free(wholeBuf); wholeBuf = nullptr; }
  }
}


//==========================================================================
//
//  VZipFileReader::Tell
//
//==========================================================================
int VZipFileReader::Tell () {
  return (wholeSize >= 0 ? currpos : (usezlib ? stream.total_out : lzmaGetTotalOut()));
}


//==========================================================================
//
//  VZipFileReader::TotalSize
//
//==========================================================================
int VZipFileReader::TotalSize () {
  return Info.filesize;
}


//==========================================================================
//
//  VZipFileReader::AtEnd
//
//==========================================================================
bool VZipFileReader::AtEnd () {
  return (wholeSize >= 0 ? (currpos >= wholeSize) : (rest_read_uncompressed == 0));
}
