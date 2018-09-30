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

#include "../fsys.h"

#define Z_LZMA  (14)
#define Z_STORE  (0)

//#define K8_UNLZMA_DEBUG


// /////////////////////////////////////////////////////////////////////////// /
enum {
  SIZECENTRALDIRITEM = 0x2e,
  SIZEZIPLOCALHEADER = 0x1e
};


//  VZipFileInfo contain information about a file in the zipfile
struct VZipFileInfo {
  VStr name; // name of the file
  vuint16 flag; // general purpose bit flag
  vuint16 compression_method; // compression method
  vuint32 crc; // crc-32
  vuint32 compressed_size; // compressed size
  vuint32 uncompressed_size; // uncompressed size
  vuint16 size_filename; // filename length
  vuint32 offset_curfile; // relative offset of local header
};


class VZipFileReader : public VStreamPakFile {
private:
  enum { UNZ_BUFSIZE = 16384 };
  //enum { UNZ_BUFSIZE = 65536 };

  mythread_mutex lock;
  VStream *fileStream; // source stream of the zipfile
  const VZipFileInfo &info; // info about the file we are reading

  Bytef readBuffer[UNZ_BUFSIZE];//  Internal buffer for compressed data
  z_stream stream;         //  ZLib stream structure for inflate
  lzma_stream lzmastream;
  lzma_options_lzma lzmaopts;
  lzma_filter filters[2];
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

  virtual const VStr &GetName () const override;
  virtual void Serialise (void*, int) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
};


// /////////////////////////////////////////////////////////////////////////// /
class VZipFile : public FSysDriverBase {
private:
  VStr zipFileName;
  VStream *fileStream; // source stream of the zipfile
  VZipFileInfo *files;
  vint32 fileCount; // total number of files
  vuint32 bytesBeforeZipFile; // bytes before the zipfile, (>0 for sfx)

private:
  vuint32 searchCentralDir ();
  void openArchive ();

protected:
  virtual const VStr &getNameByIndex (int idx) const override;
  virtual int getNameCount () const override;
  // should return `nullptr` on failure
  virtual VStream *openWithIndex (int idx) override;

public:
  VZipFile (VStream* fstream, const VStr &aname=VStr("<memory>")); // takes ownership on success
  virtual ~VZipFile() override;

  inline bool isOpened () const { return (fileStream != nullptr); }
};


// /////////////////////////////////////////////////////////////////////////// /
// takes ownership
VZipFile::VZipFile (VStream *fstream, const VStr &aname)
  : FSysDriverBase()
  , zipFileName(aname)
  , fileStream(nullptr)
  , files(nullptr)
  , fileCount(0)
{
  if (fstream) {
    fileStream = fstream;
    openArchive();
    if (fstream->IsError()) fileStream = nullptr;
  }
}


VZipFile::~VZipFile () {
  delete[] files;
  delete fileStream;
}


const VStr &VZipFile::getNameByIndex (int idx) const {
  if (idx < 0 || idx >= fileCount) return VStr::EmptyString;
  return files[idx].name;
}


int VZipFile::getNameCount () const {
  return fileCount;
}


void VZipFile::openArchive () {
  vuint32 central_pos = searchCentralDir();
  if (central_pos == 0) { fileStream = nullptr; return; } // signal failure
  //check(central_pos);

  //fprintf(stderr, "cdpos=0x%08x\n", central_pos);

  fileStream->Seek(central_pos);

  vuint32 Signature;
  vuint16 number_disk; // number of the current dist, used for spaning ZIP, unsupported, always 0
  vuint16 number_disk_with_CD; // number the the disk with central dir, used for spaning ZIP, unsupported, always 0
  vuint16 number_entry_CD; // total number of entries in the central dir (same than number_entry on nospan)
  vuint16 size_comment; // size of the global comment of the zipfile
  vuint16 fcount16;

  *fileStream
    // the signature, already checked
    << Signature
    // number of this disk
    << number_disk
    // number of the disk with the start of the central directory
    << number_disk_with_CD
    // total number of entries in the central dir on this disk
    << fcount16
    // total number of entries in the central dir
    << number_entry_CD;

  //check(number_entry_CD == fileCount);
  //check(number_disk_with_CD == 0);
  //check(number_disk == 0);

  fileCount = fcount16;

  //fprintf(stderr, "sign==0x%08x; nd=%u; cd=%u; fc=%d; ecd=%u\n", Signature, number_disk, number_disk_with_CD, fileCount, number_entry_CD);

  vuint32 size_central_dir; // size of the central directory
  vuint32 offset_central_dir; // offset of start of central directory with respect to the starting disk number

  *fileStream
    << size_central_dir
    << offset_central_dir
    << size_comment;

  //check(central_pos >= offset_central_dir+size_central_dir);

  bytesBeforeZipFile = central_pos-(offset_central_dir+size_central_dir);

  files = new VZipFileInfo[fileCount];

  bool canHasPrefix = fsysKillCommonZipPrefix;

  //fprintf(stderr, "cdpos=0x%08x; fc=%d\n", central_pos, fileCount);
  // set the current file of the zipfile to the first file
  vuint32 pos_in_central_dir = offset_central_dir;
  for (int i = 0; i < fileCount; ++i) {
    VZipFileInfo &file_info = files[i];

   again:
    files[i].name = VStr();
    fileStream->Seek(pos_in_central_dir+bytesBeforeZipFile);

    vuint32 Magic;
    vuint16 version; // version made by
    vuint16 version_needed; // version needed to extract
    vuint32 dosDate; // last mod file date in Dos fmt
    vuint16 size_file_extra; // extra field length
    vuint16 size_file_comment; // file comment length
    vuint16 disk_num_start; // disk number start
    vuint16 internal_fa; // internal file attributes
    vuint32 external_fa; // external file attributes

    // we check the magic
    *fileStream << Magic;

    if (fileStream->IsError()) { fileCount = i; break; }

    // digital signature?
    if (Magic == 0x05054b50) {
      // yes, skip it
      vuint16 dslen;
      *fileStream << dslen;
      pos_in_central_dir += 4+dslen;
      goto again;
    }

    if (Magic == 0x08074b50) {
      pos_in_central_dir += 4+3*4;
      goto again;
    }

    *fileStream
      << version // version made by
      << version_needed // version needed to extract
      << file_info.flag // general purpose bit flag (gflags)
      << file_info.compression_method // compression method
      << dosDate // last mod file time and date
      << file_info.crc
      << file_info.compressed_size
      << file_info.uncompressed_size
      << file_info.size_filename
      << size_file_extra // extra field length
      << size_file_comment // file comment length
      << disk_num_start // disk number start
      << internal_fa // internal file attributes (iattr)
      << external_fa // external file attributes (attr)
      << file_info.offset_curfile; // relative offset of local header

    if (Magic != 0x02014b50 || fileStream->IsError()) {
      //fprintf(stderr, "FUCK! #%d: <%08x>\n", i, Magic);
      fileCount = i;
      break;
    }

    if (file_info.size_filename == 0 || (file_info.flag&0x2061) != 0 || (external_fa&0x58) != 0) {
      // ignore this
      // set the current file of the zipfile to the next file
      pos_in_central_dir += SIZECENTRALDIRITEM+file_info.size_filename+size_file_extra+size_file_comment;
      continue;
    }

    char *filename_inzip = new char[file_info.size_filename+1];
    filename_inzip[file_info.size_filename] = '\0';
    fileStream->Serialise(filename_inzip, file_info.size_filename);
    for (int f = 0; f < file_info.size_filename; ++f) if (filename_inzip[f] == '\\') filename_inzip[f] = '/';
    files[i].name = VStr(filename_inzip);
    //fprintf(stderr, "** NAME: <%s> (%s) (%s)\n", *files[i].name, filename_inzip, *files[i].name.utf2win());
    delete[] filename_inzip;
    if (files[i].name.isUtf8Valid()) files[i].name = files[i].name.utf2win();
    //fprintf(stderr, "NAME: <%s>\n", *files[i].name);

    if (fileStream->IsError()) { fileCount = i; break; }

    // if we have extra field, parse it to get utf-8 name
    int extraleft = size_file_extra;
    while (extraleft >= 4) {
      vuint16 eid;
      vuint16 esize;
      *fileStream << eid << esize;
      extraleft -= esize+4;
      //fprintf(stderr, " xtra: 0x%04x %u\n", eid, esize);
      auto pos = fileStream->Tell();
      // utf-8 name?
      if (eid == 0x7075 && esize > 5) {
        vuint8 ver;
        vuint32 ecrc;
        *fileStream << ver << ecrc;
        //TODO: check crc
        filename_inzip = new char[esize-5+1];
        filename_inzip[esize-5] = 0;
        fileStream->Serialise(filename_inzip, esize-5);
        for (int f = 0; f < esize-5; ++f) if (filename_inzip[f] == '\\') filename_inzip[f] = '/';
        files[i].name = VStr(filename_inzip).utf2win();
        delete[] filename_inzip;
        //fprintf(stderr, "  UTF: <%s>\n", *files[i].name);
        break;
      }
      fileStream->Seek(pos+esize);
    }

    if (files[i].name.length() == 0 || files[i].name.endsWith("/")) {
      files[i].name.clear();
    } else if (file_info.compression_method != Z_STORE && file_info.compression_method != Z_DEFLATED && file_info.compression_method != Z_LZMA) {
      files[i].name.clear();
    } else {
      if (canHasPrefix && files[i].name.IndexOf('/') == -1) canHasPrefix = false;
    }

    // set the current file of the zipfile to the next file
    pos_in_central_dir += SIZECENTRALDIRITEM+file_info.size_filename+size_file_extra+size_file_comment;
  }

  // find and remove common prefix
  if (canHasPrefix && fileCount > 0) {
    VStr xpfx = files[0].name;
    int sli = xpfx.IndexOf('/');
    if (sli > 0) {
      xpfx = VStr(xpfx, 0, sli+1); // extract prefix
      if (!xpfx.startsWith("packages/")) {
        for (int i = 0; i < fileCount; ++i) {
          if (!files[i].name.startsWith(xpfx)) { canHasPrefix = false; break; }
        }
        if (canHasPrefix) {
          // remove prefix
          for (int i = 0; i < fileCount; ++i) {
            files[i].name = VStr(files[i].name, sli+1, files[i].name.Length()-sli-1);
            //if (files[i].name.length() == 0) { removeAtIndex(i); --i; }
          }
        }
      }
    }
  }

  // remove empty names
  {
    int spos = 0, dpos = 0;
    while (spos < fileCount) {
      if (files[spos].name.length() == 0) {
        ++spos;
      } else {
        if (spos != dpos) {
          files[dpos] = files[spos];
        }
        ++spos;
        ++dpos;
      }
    }
    for (int f = dpos; f < fileCount; ++f) files[f].name.clear();
    fileCount = dpos;
  }

  buildNameHashTable();
}


struct SelfDestructBuf {
  vuint8 *buf;

  SelfDestructBuf (vint32 sz) {
    buf = (vuint8 *)malloc(sz);
    if (!buf) Sys_Error("Out of memory!");
  }
  ~SelfDestructBuf () { free(buf); }
};


vuint32 VZipFile::searchCentralDir () {
  enum { MaxBufSize = 65578 };

  vint32 fsize = fileStream->TotalSize();
  if (fsize < 16) return 0;

  vuint32 rd = (fsize < MaxBufSize ? fsize : MaxBufSize);
  SelfDestructBuf sdbuf(rd);

  fileStream->Seek(fsize-rd);
  if (fileStream->IsError()) return 0;
  fileStream->Serialise(sdbuf.buf, rd);
  if (fileStream->IsError()) return 0;

  for (int f = rd-8; f >= 0; --f) {
    if (sdbuf.buf[f] == 0x50 && sdbuf.buf[f+1] == 0x4b && sdbuf.buf[f+2] == 0x05 && sdbuf.buf[f+3] == 0x06) {
      // i found her!
      return fsize-rd+f;
    }
  }

  return 0;
}


VStream *VZipFile::openWithIndex (int idx) {
  if (idx < 0 || idx >= fileCount) return nullptr;
  return new VZipFileReader(fileStream, bytesBeforeZipFile, files[idx], this);
}


// ////////////////////////////////////////////////////////////////////////// //
VZipFileReader::VZipFileReader (VStream *InStream, vuint32 bytesBeforeZipFile, const VZipFileInfo &aInfo, FSysDriverBase *aDriver)
  : VStreamPakFile(aDriver)
  , fileStream(InStream)
  , info(aInfo)
{
  mythread_mutex_init(&lock);
  MyThreadLocker locker(&lock);

  // open the file in the zip
  usezlib = true;
  nextpos = currpos = 0;

  vuint32 iSizeVar;
  if (!checkCurrentFileCoherencyHeader(&iSizeVar, bytesBeforeZipFile)) { bError = true; return; }

  stream_initialised = false;
  lzmastream = LZMA_STREAM_INIT;

  if (info.compression_method != Z_STORE && info.compression_method != Z_DEFLATED && info.compression_method != Z_LZMA) {
    bError = true;
    //error->Logf("Compression method %d is not supported", info.compression_method);
    return;
  }

  Crc32 = 0;

  stream.total_out = 0;
  lzmastream.total_out = 0;
  pos_in_zipfile = info.offset_curfile+SIZEZIPLOCALHEADER+iSizeVar+bytesBeforeZipFile;
  start_pos = pos_in_zipfile;
  rest_read_compressed = info.compressed_size;
  rest_read_uncompressed = info.uncompressed_size;

  if (info.compression_method == Z_DEFLATED) {
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
      * In unzip, i don't wait absolutely Z_STREAM_END because I know the
      * size of both compressed and uncompressed data
      */
      bError = true;
      //error->Log("Failed to initialise inflate stream");
      return;
    }
    stream_initialised = true;
  } else if (info.compression_method == Z_LZMA) {
    // LZMA
    usezlib = false;
    if (!lzmaRestart()) return; // error already set
  }

  stream.avail_in = 0;
  lzmastream.avail_in = 0;
  bLoading = true;
}


VZipFileReader::~VZipFileReader () {
  Close();
  mythread_mutex_destroy(&lock);
}


const VStr &VZipFileReader::GetName () const {
  return info.name;
}


void VZipFileReader::setError () {
  bError = true;
  if (usezlib) {
    if (stream_initialised) inflateEnd(&stream);
  } else {
    if (stream_initialised) lzma_end(&lzmastream);
  }
  stream_initialised = false;
}


bool VZipFileReader::Close () {
  if (!bError && rest_read_uncompressed == 0) {
    if (Crc32 != info.crc) { bError = true; /*error->Log("Bad CRC");*/ }
  }
  if (usezlib) {
    if (stream_initialised) inflateEnd(&stream);
  } else {
    if (stream_initialised) lzma_end(&lzmastream);
  }
  stream_initialised = false;
  return !bError;
}


// just read, no `nextpos` advancement
// returns number of bytes read, -1 on error, or 0 on EOF
int VZipFileReader::readSomeBytes (void *buf, int len) {
  stream.next_out = (Bytef *)buf;
  stream.avail_out = len;
  lzmastream.next_out = (Bytef *)buf;
  lzmastream.avail_out = len;
#ifdef K8_UNLZMA_DEBUG
  fprintf(stderr, "VZipFileReader::readSomeBytes: %d available in out, %d bytes left in in\n", (int)(usezlib ? stream.avail_out : lzmastream.avail_out), (int)(usezlib ? stream.avail_in : lzmastream.avail_in));
#endif
  int res = 0;
  while ((usezlib ? stream.avail_out : lzmastream.avail_out) > 0) {
    // read compressed data (if necessary)
    if ((usezlib ? stream.avail_in : lzmastream.avail_in) == 0) {
      if (rest_read_compressed > 0) {
        vuint32 uReadThis = UNZ_BUFSIZE;
        if (rest_read_compressed < uReadThis) uReadThis = rest_read_compressed;
#ifdef K8_UNLZMA_DEBUG
        /*if (!usezlib)*/ fprintf(stderr, "LZMA: reading compressed bytes from ofs %u\n", (unsigned)(pos_in_zipfile-start_pos));
#endif
        fileStream->Seek(pos_in_zipfile);
        fileStream->Serialise(readBuffer, uReadThis);
        if (fileStream->IsError()) {
#ifdef K8_UNLZMA_DEBUG
          fprintf(stderr, "LZMA: FAILED to read %d compressed bytes\n", uReadThis);
#endif
          setError();
          return -1;
        }
#ifdef K8_UNLZMA_DEBUG
        /*if (!usezlib)*/ fprintf(stderr, "LZMA: read %d compressed bytes\n", uReadThis);
#endif
        pos_in_zipfile += uReadThis;
        rest_read_compressed -= uReadThis;
        stream.next_in = readBuffer;
        stream.avail_in = uReadThis;
        lzmastream.next_in = readBuffer;
        lzmastream.avail_in = uReadThis;
      }
    }

    // decompress data
    if (info.compression_method == Z_STORE) {
      // stored data
      if (stream.avail_in == 0 && rest_read_compressed == 0) break;
      int uDoCopy = (stream.avail_out < stream.avail_in ? stream.avail_out : stream.avail_in);
      memcpy(stream.next_out, stream.next_in, uDoCopy);
      Crc32 = crc32(Crc32, stream.next_out, uDoCopy);
      rest_read_uncompressed -= uDoCopy;
      stream.avail_in -= uDoCopy;
      stream.avail_out -= uDoCopy;
      stream.next_out += uDoCopy;
      stream.next_in += uDoCopy;
      stream.total_out += uDoCopy;
      res += uDoCopy;
    } else if (info.compression_method == Z_DEFLATED) {
      // zlib data
      int flush = Z_SYNC_FLUSH;
      uLong uTotalOutBefore = stream.total_out;
      const Bytef *bufBefore = stream.next_out;
      int err = inflate(&stream, flush);
      if (err >= 0 && stream.msg != nullptr) {
#ifdef K8_UNLZMA_DEBUG
        fprintf(stderr, "ZIP: FAILED to read %d compressed bytes\n", len);
#endif
        setError();
        return -1;
      }
      uLong uTotalOutAfter = stream.total_out;
      vuint32 uOutThis = uTotalOutAfter-uTotalOutBefore;
      Crc32 = crc32(Crc32, bufBefore, (uInt)uOutThis);
      rest_read_uncompressed -= uOutThis;
      res += (uInt)(uTotalOutAfter-uTotalOutBefore);
#ifdef K8_UNLZMA_DEBUG
      fprintf(stderr, "ZIP: read %d bytes (res=%d)\n", (int)(uTotalOutAfter-uTotalOutBefore), res);
      if (err != Z_OK) fprintf(stderr, "ZIP: FAILED to read %d compressed bytes (err=%d)\n", len, err);
#endif
      if (err != Z_OK) break;
    } else {
      // lzma data
#ifdef K8_UNLZMA_DEBUG
      fprintf(stderr, "LZMA: processing %u compressed bytes into %u uncompressed bytes\n", (unsigned)lzmastream.avail_in, (unsigned)lzmastream.avail_out);
      auto inbefore = lzmastream.avail_in;
#endif
      auto outbefore = lzmastream.avail_out;
      const Bytef *bufBefore = lzmastream.next_out;
      int err = lzma_code(&lzmastream, LZMA_RUN);
      if (err != LZMA_OK && err != LZMA_STREAM_END) {
#ifdef K8_UNLZMA_DEBUG
        fprintf(stderr, "LZMA: stream error (%d)\n", (int)err);
#endif
        setError();
        return -1;
      }
      vuint32 uOutThis = outbefore-lzmastream.avail_out;
#ifdef K8_UNLZMA_DEBUG
      fprintf(stderr, "LZMA: processed %u packed bytes, unpacked %u bytes (err=%d)\n", (unsigned)(inbefore-lzmastream.avail_in), uOutThis, err);
#endif
      Crc32 = crc32(Crc32, bufBefore, (uInt)uOutThis);
      rest_read_uncompressed -= uOutThis;
      res += (uInt)uOutThis; //(uInt)(uTotalOutAfter - uTotalOutBefore);
      if (err != LZMA_OK) {
#ifdef K8_UNLZMA_DEBUG
        fprintf(stderr, "LZMA: stream end\n");
#endif
        break;
      }
    }
  }
#ifdef K8_UNLZMA_DEBUG
  fprintf(stderr, "VZipFileReader::readSomeBytes: read %d bytes\n", res);
#endif
  return res;
}


// `pos_in_zipfile` must be valid
bool VZipFileReader::lzmaRestart () {
  vuint8 ziplzmahdr[4];
  vuint8 lzmaprhdr[5];

  if (usezlib) {
#ifdef K8_UNLZMA_DEBUG
    fprintf(stderr, "LZMA: FAILED to restart (not lzma)\n");
#endif
    setError();
    return false;
  }

  if (stream_initialised) { lzma_end(&lzmastream); stream_initialised = false; }
  rest_read_uncompressed = info.uncompressed_size;
  lzmastream = LZMA_STREAM_INIT;

  if (rest_read_compressed < 4+5) {
    bError = true;
    //error->Log("Invalid lzma header (out of data)");
    return false;
  }

  fileStream->Seek(pos_in_zipfile);
  fileStream->Serialise(ziplzmahdr, 4);
  fileStream->Serialise(lzmaprhdr, 5);
  rest_read_compressed -= 4+5;

  if (fileStream->IsError()) {
    bError = true;
    //error->Log("error reading lzma headers");
    return false;
  }

  if (ziplzmahdr[3] != 0 || ziplzmahdr[2] == 0 || ziplzmahdr[2] < 5) {
    bError = true;
    //error->Log("Invalid lzma header (0)");
    return false;
  }

  if (ziplzmahdr[2] > 5) {
    vuint32 skip = ziplzmahdr[2]-5;
    if (rest_read_compressed < skip) {
      bError = true;
      //error->Log("Invalid lzma header (out of extra data)");
      return false;
    }
    rest_read_compressed -= skip;
    vuint8 tmp;
    for (; skip > 0; --skip) {
      *fileStream << tmp;
      if (fileStream->IsError()) {
        bError = true;
        //error->Log("error reading extra lzma headers");
        return false;
      }
    }
  }

#ifdef K8_UNLZMA_DEBUG
  fprintf(stderr, "LZMA: %u bytes in header\n", (unsigned)(fileStream->Tell()-pos_in_zipfile));
#endif

  lzma_lzma_preset(&lzmaopts, 9|LZMA_PRESET_EXTREME);
  filters[0].id = LZMA_FILTER_LZMA1;
  filters[0].options = &lzmaopts;
  filters[1].id = LZMA_VLI_UNKNOWN;

  vuint32 prpsize;
  if (lzma_properties_size(&prpsize, &filters[0]) != LZMA_OK) {
    bError = true;
    //error->Log("Failed to initialise lzma stream");
    return false;
  }
  if (prpsize != 5) {
    bError = true;
    //error->Log("Failed to initialise lzma stream");
    return false;
  }

  if (lzma_properties_decode(&filters[0], nullptr, lzmaprhdr, prpsize) != LZMA_OK) {
    bError = true;
    //error->Log("Failed to initialise lzma stream");
    return false;
  }

  if (lzma_raw_decoder(&lzmastream, &filters[0]) != LZMA_OK) {
    bError = true;
    //error->Log("Failed to initialise lzma stream");
    return false;
  }

  pos_in_zipfile = fileStream->Tell();

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
    Crc32 = 0;
    currpos = 0;
    rest_read_compressed = info.compressed_size;
    rest_read_uncompressed = info.uncompressed_size;
    pos_in_zipfile = start_pos;
    if (info.compression_method == Z_DEFLATED) {
      check(stream_initialised);
      check(usezlib);
      if (stream_initialised) { inflateEnd(&stream); stream_initialised = false; }
      memset(&stream, 0, sizeof(stream));
      if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) { bError = true; return; }
      stream_initialised = true;
    } else if (info.compression_method == Z_LZMA) {
#ifdef K8_UNLZMA_DEBUG
      fprintf(stderr, "LZMA: rewind (nextpos=%d)\n", nextpos);
#endif
      check(stream_initialised);
      check(!usezlib);
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
#ifdef K8_UNLZMA_DEBUG
      fprintf(stderr, "LZMA: skip error (rd=%d)\n", rd);
#endif
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


// ////////////////////////////////////////////////////////////////////////// //
static FSysDriverBase *zipLoader (VStream *strm) {
  if (!strm) return nullptr;
  //fprintf(stderr, "trying <%s> as zip...\n", *strm->GetName());
  auto res = new VZipFile(strm);
  if (!res->isOpened()) { delete res; res = nullptr; }
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
void fsys_Register_ZIP () {
  //fprintf(stderr, "registering ZIP loader...\n");
  FSysRegisterDriver(&zipLoader);
}
