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

#define MAX_WBITS 15
#define MAX_MEM_LEVEL 9


// ////////////////////////////////////////////////////////////////////////// //
class VZipFileReader : public VStream {
private:
  //enum { UNZ_BUFSIZE = 16384 };
  enum { UNZ_BUFSIZE = 65536 };

  mythread_mutex *rdlock;
  VStream *FileStream; // source stream of the zipfile
  VStr fname;
  const VPakFileInfo &Info; // info about the file we are reading

  vuint8 ReadBuffer[UNZ_BUFSIZE]; // internal buffer for compressed data
  mz_stream stream; // zlib stream structure for inflate
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
  int currpos; // we can do several seeks in a row; perform real seek in `Serialise()`; unused if `wholeSize >= 0`

private:
  bool CheckCurrentFileCoherencyHeader (vuint32 *, vuint32); // does locking
  bool LzmaRestart (); // `pos_in_zipfile` must be valid; does locking
  bool rewind ();
  void readBytes (void *buf, int length); // does locking, if necessary
  void cacheAllData ();

  #ifdef VAVOOM_USE_LIBLZMA
  inline int lzmaGetTotalOut () const { return (int)lzmastream.total_out; }
  inline bool lzmaIsInStreamEmpty () const { return (lzmastream.avail_in == 0); }
  #else
  inline int lzmaGetTotalOut () const { return lzmatotalout; }
  inline bool lzmaIsInStreamEmpty () const { return (lzmainbufleft == 0); }
  #endif

  inline int totalOut () const { return (usezlib ? stream.total_out : lzmaGetTotalOut()); }

public:
  VV_DISABLE_COPY(VZipFileReader)

  VZipFileReader (VStr afname, VStream *, vuint32, const VPakFileInfo &, mythread_mutex *ardlock);
  virtual ~VZipFileReader () override;

  virtual VStr GetName () const override;
  virtual void SetError () override;
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
  , stream_initialised(false)
  , wholeBuf(nullptr)
  , wholeSize(-2)
  , currpos(0)
{
  // open the file in the zip
  // `rdlock` is not locked here
  usezlib = true;
  //stream_initialised = false;

  if (!rdlock) Sys_Error("VZipFileReader::VZipFileReader: empty lock!");

  vuint32 iSizeVar;
  if (!CheckCurrentFileCoherencyHeader(&iSizeVar, BytesBeforeZipFile)) {
    SetError();
    return;
  }

  #ifdef VAVOOM_USE_LIBLZMA
  lzmastream = LZMA_STREAM_INIT;
  #endif

  if (Info.compression != Z_STORE && Info.compression != MZ_DEFLATED && Info.compression != Z_LZMA) {
    SetError();
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

  if (Info.compression == MZ_DEFLATED) {
    stream.zalloc = (mz_alloc_func)0;
    stream.zfree = (mz_free_func)0;
    stream.opaque = (void *)0;
    stream.next_in = (vuint8 *)0;
    stream.avail_in = 0;

    int err = mz_inflateInit2(&stream, -MAX_WBITS);
    if (err != MZ_OK) {
      /* windowBits is passed < 0 to tell that there is no zlib header.
       * Note that in this case inflate *requires* an extra "dummy" byte
       * after the compressed stream in order to complete decompression and
       * return MZ_STREAM_END.
       * In unzip, i don't wait absolutely MZ_STREAM_END because I known the
       * size of both compressed and uncompressed data
       */
      SetError();
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
  //!GLog.Logf(NAME_Debug, "******** OPENED '%s' ********", *fname);
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
//  VZipFileReader::SetError
//
//==========================================================================
void VZipFileReader::SetError () {
  if (wholeBuf) { Z_Free(wholeBuf); wholeBuf = nullptr; }
  wholeSize = -2;
  if (stream_initialised) {
    if (usezlib) {
      mz_inflateEnd(&stream);
    } else {
      #ifdef VAVOOM_USE_LIBLZMA
      lzma_end(&lzmastream);
      #else
      if (lzmainited) { lzmainited = false; LzmaDec_Free(&lzmastate, &fsysLzmaAlloc); }
      #endif
    }
    stream_initialised = false;
  }
  VStream::SetError();
}


//==========================================================================
//
//  VZipFileReader::Close
//
//==========================================================================
bool VZipFileReader::Close () {
  //!GLog.Logf(NAME_Debug, "******** CLOSING '%s' ********", *fname);
  if (wholeBuf) { Z_Free(wholeBuf); wholeBuf = nullptr; }
  wholeSize = -2;

  if (stream_initialised) {
    //GLog.Logf("***CLOSING '%s'", *fname);
    /*
    if (!bError && rest_read_uncompressed == 0) {
      if (Crc32 != Info.crc32) {
        SetError();
        GLog.Logf("Bad CRC for file '%s'", *fname);
      }
    }
    */
    if (usezlib) {
      mz_inflateEnd(&stream);
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
//  VZipFileReader::LzmaRestart
//
//  `pos_in_zipfile` must be valid
//
//==========================================================================
bool VZipFileReader::LzmaRestart () {
  vuint8 ziplzmahdr[4];
  vuint8 lzmaprhdr[5];

  if (usezlib) {
    SetError();
    GLog.Logf(NAME_Error, "Cannot lzma-restart non-lzma stream for file '%s'", *fname);
    return false;
  }

  #ifdef VAVOOM_USE_LIBLZMA
  if (stream_initialised) { lzma_end(&lzmastream); stream_initialised = false; }
  lzmastream = LZMA_STREAM_INIT;
  lzmastream.total_out = 0;
  lzmastream.avail_in = 0;
  #else
  stream_initialised = false;
  if (lzmainited) { lzmainited = false; LzmaDec_Free(&lzmastate, &fsysLzmaAlloc); }
  lzmatotalout = 0;
  lzmainbufleft = 0;
  #endif
  vensure(Crc32 == 0);

  if (rest_read_compressed < 4+5) {
    SetError();
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
      SetError();
      GLog.Logf(NAME_Error, "Error reading lzma headers for file '%s'", *fname);
      return false;
    }

    if (ziplzmahdr[3] != 0 || ziplzmahdr[2] == 0 || ziplzmahdr[2] < 5) {
      SetError();
      GLog.Logf(NAME_Error, "Invalid lzma header (0) for file '%s'", *fname);
      return false;
    }

    if (ziplzmahdr[2] > 5) {
      vuint32 skip = ziplzmahdr[2]-5;
      if (rest_read_compressed < skip) {
        SetError();
        GLog.Logf(NAME_Error, "Invalid lzma header (out of extra data) for file '%s'", *fname);
        return false;
      }
      rest_read_compressed -= skip;
      vuint8 tmp;
      for (; skip > 0; --skip) {
        *FileStream << tmp;
        if (FileStream->IsError()) {
          SetError();
          GLog.Logf(NAME_Error, "Error reading extra lzma headers for file '%s'", *fname);
          return false;
        }
      }
    }

    pos_in_zipfile = FileStream->Tell();
    if (FileStream->IsError()) {
      SetError();
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
    SetError();
    GLog.Logf(NAME_Error, "Failed to initialise lzma stream for file '%s'", *fname);
    return false;
  }
  if (prpsize != 5) {
    SetError();
    GLog.Logf(NAME_Error, "Failed to initialise lzma stream for file '%s'", *fname);
    return false;
  }

  if (lzma_properties_decode(&filters[0], nullptr, lzmaprhdr, prpsize) != LZMA_OK) {
    lzmaopts = (lzma_options_lzma *)filters[0].options;
    SetError();
    GLog.Logf(NAME_Error, "Failed to initialise lzma stream for file '%s'", *fname);
    return false;
  }
  lzmaopts = (lzma_options_lzma *)filters[0].options;

  if (lzma_raw_decoder(&lzmastream, &filters[0]) != LZMA_OK) {
    SetError();
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
    SetError();
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
//  VZipFileReader::rewind
//
//==========================================================================
bool VZipFileReader::rewind () {
  if (bError) return false;
  //!GLog.Logf(NAME_Debug, "*** REWIND '%s'", *fname);
  Crc32 = 0;
  rest_read_compressed = Info.packedsize;
  rest_read_uncompressed = Info.filesize;
  pos_in_zipfile = start_pos;
  if (Info.compression == MZ_DEFLATED) {
    vassert(stream_initialised);
    vassert(usezlib);
    if (stream_initialised) mz_inflateEnd(&stream);
    memset(&stream, 0, sizeof(stream));
    vensure(mz_inflateInit2(&stream, -MAX_WBITS) == MZ_OK);
  } else if (Info.compression == Z_LZMA) {
    #ifdef K8_UNLZMA_DEBUG
    GLog.Log(NAME_Debug, "LZMA: rewind");
    #endif
    vassert(stream_initialised);
    vassert(!usezlib);
    if (!LzmaRestart()) return false; // error already set
  } else {
    memset(&stream, 0, sizeof(stream));
  }
  return !bError;
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
    //GLog.Logf(NAME_Error, "Want to read past the end of the file '%s': rest=%d; length=%d", *fname, (vint32)rest_read_uncompressed, length);
    SetError();
    return;
  }

  stream.next_out = (vuint8 *)buf;
  stream.avail_out = length;
  #ifdef VAVOOM_USE_LIBLZMA
  lzmastream.next_out = (vuint8 *)buf;
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
        SetError();
        GLog.Logf(NAME_Error, "Failed to read from zip for file '%s'", *fname);
        return;
      }
      #ifdef K8_UNLZMA_DEBUG
      if (!usezlib) GLog.Logf(NAME_Debug, "LZMA: read %d compressed bytes", uReadThis);
      #endif
      //GLog.Logf(NAME_Debug, "Read %u packed bytes from '%s' (%u left)", uReadThis, *fname, rest_read_compressed-uReadThis);
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
    }

    // decompress data
    if (Info.compression == Z_STORE) {
      // stored data
      if (stream.avail_in == 0 && rest_read_compressed == 0) break;
      int uDoCopy = (stream.avail_out < stream.avail_in ? stream.avail_out : stream.avail_in);
      vassert(uDoCopy > 0);
      //for (int i = 0; i < uDoCopy; ++i) *(stream.next_out+i) = *(stream.next_in+i);
      memcpy(stream.next_out, stream.next_in, uDoCopy);
      Crc32 = mz_crc32(Crc32, stream.next_out, uDoCopy);
      rest_read_uncompressed -= uDoCopy;
      stream.avail_in -= uDoCopy;
      stream.avail_out -= uDoCopy;
      stream.next_out += uDoCopy;
      stream.next_in += uDoCopy;
      stream.total_out += uDoCopy;
      iRead += uDoCopy;
    } else if (Info.compression == MZ_DEFLATED) {
      // zlib data
      int flush = MZ_SYNC_FLUSH;
      vuint32 uTotalOutBefore = stream.total_out;
      const vuint8 *bufBefore = stream.next_out;
      int err = mz_inflate(&stream, flush);
      if (err >= 0 && stream.msg != nullptr) {
        SetError();
        GLog.Logf(NAME_Error, "Decompression failed: %s for file '%s'", stream.msg, *fname);
        return;
      }
      vuint32 uTotalOutAfter = stream.total_out;
      vuint32 uOutThis = uTotalOutAfter-uTotalOutBefore;
      Crc32 = mz_crc32(Crc32, bufBefore, (vuint32)uOutThis);
      rest_read_uncompressed -= uOutThis;
      iRead += (vuint32)(uTotalOutAfter-uTotalOutBefore);
      if (err != MZ_OK) break;
    } else {
      // lzma data
      #ifdef VAVOOM_USE_LIBLZMA
        #ifdef K8_UNLZMA_DEBUG
        GLog.Logf(NAME_Debug, "LZMA: processing %u compressed bytes into %u uncompressed bytes", (unsigned)lzmastream.avail_in, (unsigned)lzmastream.avail_out);
        auto inbefore = lzmastream.avail_in;
        #endif
        auto outbefore = lzmastream.avail_out;
        const vuint8 *bufBefore = lzmastream.next_out;
        int err = lzma_code(&lzmastream, LZMA_RUN);
        if (err != LZMA_OK && err != LZMA_STREAM_END) {
          SetError();
          GLog.Logf(NAME_Error, "LZMA decompression failed (%d) for file '%s'", err, *fname);
          return;
        }
        vuint32 uOutThis = outbefore-lzmastream.avail_out;
        #ifdef K8_UNLZMA_DEBUG
        GLog.Logf(NAME_Debug, "LZMA: processed %u packed bytes, unpacked %u bytes (err=%d); (want %d, got so far %d, left %d : %d)", (unsigned)(inbefore-lzmastream.avail_in), uOutThis, err, length, iRead, length-iRead, length-iRead-uOutThis);
        #endif
        Crc32 = mz_crc32(Crc32, bufBefore, (vuint32)uOutThis);
        rest_read_uncompressed -= uOutThis;
        iRead += (vuint32)uOutThis; //(vuint32)(uTotalOutAfter - uTotalOutBefore);
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
        iRead += (vuint32)lzmaouted;
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
    if (!bError) SetError();
  } else if (!bError && rest_read_uncompressed == 0) {
    if (Crc32 != Info.crc32) {
      SetError();
      GLog.Logf(NAME_Error, "Bad CRC for file '%s' (wanted 0x%08x, got 0x%08x)", *fname, (unsigned)Info.crc32, (unsigned)Crc32);
    }
  }
}


//==========================================================================
//
//  VZipFileReader::cacheAllData
//
//  must NOT modify `currpos`
//
//==========================================================================
void VZipFileReader::cacheAllData () {
  //GLog.Logf(NAME_Debug, "Back-seek in '%s' (curr=%d; new=%d)", *fname, Tell(), InPos);
  //if (!skipRewind && !rewind()) return; // error already set
  if (totalOut() != 0) {
    if (!rewind()) return; // error already set
  }
  vassert(totalOut() == 0);
  // cache data
  wholeSize = (vint32)Info.filesize;
  //GLog.Logf(NAME_Debug, "*** CACHING '%s' (size=%d)", *fname, wholeSize);
  if (wholeSize < 0) { SetError(); return; }
  wholeBuf = (vuint8 *)Z_Realloc(wholeBuf, (wholeSize ? wholeSize : 1));
  readBytes(wholeBuf, wholeSize);
  //GLog.Logf(NAME_Debug, "*** CACHED: %s", (bError ? "error" : "ok"));
}


//==========================================================================
//
//  VZipFileReader::Serialise
//
//==========================================================================
void VZipFileReader::Serialise (void *V, int length) {
  if (bError) return;
  if (length < 0) { SetError(); return; }
  if (length == 0) return;

  if (!V) {
    SetError();
    GLog.Logf(NAME_Error, "Cannot read into nullptr buffer for file '%s'", *fname);
    return;
  }

  // use data cache?
  if (wholeSize >= 0) {
    //!GLog.Logf(NAME_Debug, "  ***READING (CACHE) '%s' (currpos=%d; size=%d; left=%d; len=%d)", *fname, currpos, wholeSize, wholeSize-currpos, length);
    if (length < 0 || currpos < 0 || currpos >= wholeSize || wholeSize-currpos < length) SetError();
    if (bError) return;
    memcpy(V, wholeBuf+currpos, length);
    currpos += length;
  } else {
    // uncached
    if (!FileStream) { SetError(); return; }
    //!GLog.Logf(NAME_Debug, "  ***READING (DIRECT) '%s' (currpos=%d; length=%u; realpos=%d; size=%u; rru=%u)", *fname, currpos, length, totalOut(), Info.filesize, rest_read_uncompressed);
    if (length > (int)Info.filesize || (int)Info.filesize-currpos < length) { SetError(); return; }
    // cache file if it is LZMA (LZMA is quite slow, and texture detection seeks alot)
    /*
    if (Info.compression == Z_LZMA) {
      cacheAllData();
      if (bError) return;
      Serialise(V, length);
      return;
    }
    */
    // cache file if we're seeking near the end
    if (totalOut() < 32768 && currpos >= (vint32)(Info.filesize-Info.filesize/3)) {
      ++wholeSize;
      // cache file if it is LZMA (LZMA is quite slow, and texture detection seeks alot)
      if (wholeSize >= 0 || Info.compression == Z_LZMA) {
        //!GLog.Logf(NAME_Debug, "*** (0)CACHING '%s' (cpos=%d; newpos=%d; size=%u)", *fname, totalOut(), currpos, Info.filesize);
        cacheAllData();
        if (bError) return;
        Serialise(V, length);
        return;
      }
    }
    // rewind if necessary
    if (currpos < totalOut()) {
      // check if we have to cache data
      // cache file if it is LZMA (LZMA is quite slow, and texture detection seeks alot)
      if (totalOut() > 8192 || Info.compression == Z_LZMA) {
        ++wholeSize;
        if (wholeSize >= 0 || Info.compression == Z_LZMA) {
          //!GLog.Logf(NAME_Debug, "*** (1)CACHING '%s' (cpos=%d; newpos=%d; size=%u)", *fname, totalOut(), currpos, Info.filesize);
          cacheAllData();
          if (bError) return;
          Serialise(V, length);
          return;
        }
      }
      //!GLog.Logf(NAME_Debug, "Back-seek in '%s' (curr=%d; new=%d)", *fname, totalOut(), currpos);
      if (!rewind()) return; // error already set
      vassert(totalOut() == 0);
    }
    // skip bytes if necessary
    if (currpos > totalOut()) {
      // read data into a temporary buffer until we reach the required position
      int toSkip = currpos-totalOut();
      vassert(toSkip > 0);
      vuint8 tmpbuf[512];
      vuint8 *tmpbufptr;
      int bsz;
      if (!wholeBuf && toSkip <= (int)sizeof(tmpbuf)) {
        bsz = (int)sizeof(tmpbuf);
        tmpbufptr = tmpbuf;
      } else {
        bsz = 65536;
        if (!wholeBuf) wholeBuf = (vuint8 *)Z_Malloc(bsz);
        tmpbufptr = wholeBuf;
      }
      //!GLog.Logf(NAME_Debug, "*** SKIPPING '%s' (cpos=%d; newpos=%d; size=%u; skip=%d)", *fname, totalOut(), currpos, Info.filesize, toSkip);
      while (toSkip > 0) {
        int count = min2(toSkip, bsz);
        toSkip -= count;
        readBytes(tmpbufptr, count);
        if (bError) return;
      }
      vassert(toSkip == 0);
    }
    vassert(currpos == totalOut());
    readBytes(V, length);
    currpos = totalOut();
    //!GLog.Logf(NAME_Debug, "*** READ '%s' (cpos=%d; newpos=%d; size=%u; len=%d; err=%d)", *fname, totalOut(), currpos, Info.filesize, length, (int)bError);
  }
}


//==========================================================================
//
//  VZipFileReader::Seek
//
//==========================================================================
void VZipFileReader::Seek (int InPos) {
  if (bError) return;
  if (InPos < 0 || InPos > (int)Info.filesize) { SetError(); return; }
  //!GLog.Logf(NAME_Debug, "*** SEEK '%s' (currpos=%d; newpos=%d; realpos=%d; size=%u)", *fname, currpos, InPos, totalOut(), Info.filesize);
  currpos = InPos;
}


//==========================================================================
//
//  VZipFileReader::Tell
//
//==========================================================================
int VZipFileReader::Tell () {
  return currpos;
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
  return (currpos >= (int)Info.filesize);
}
