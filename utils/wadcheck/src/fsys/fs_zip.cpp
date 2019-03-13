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
#include "fs_local.h"
#ifdef USE_INTERNAL_LZMA
# include "../libs/lzma/src/api/lzma.h"
#else
# include <lzma.h>
#endif
#ifdef USE_INTERNAL_ZLIB
# include "../libs/zlib/src/zlib.h"
#else
# include <zlib.h>
#endif

#define Z_LZMA  (14)
#define Z_STORE  (0)

//#define K8_UNLZMA_DEBUG


enum {
  SIZECENTRALDIRITEM = 0x2e,
  SIZEZIPLOCALHEADER = 0x1e,
};


static const char *moreresdirs[] = {
  "models/",
  nullptr,
};


// ////////////////////////////////////////////////////////////////////////// //
// information about a file in the zipfile
/*
struct VPakFileInfo {
  VStr fileName; // name of the file
  vuint16 flag; // general purpose bit flag
  vuint16 compression; // compression method
  vuint32 crc32; // crc-32
  vuint32 packedsize; // compressed size
  vuint32 filesize; // uncompressed size
  vuint16 filenamesize; // filename length
  vuint32 pakdataofs; // relative offset of local header
  // for WAD-like access
  VName lumpName;
  vint32 lumpNamespace;
  int nextLump; // next lump with the same name
};
*/


// ////////////////////////////////////////////////////////////////////////// //
class VZipFileReader : public VStream {
private:
  //enum { UNZ_BUFSIZE = 16384 };
  enum { UNZ_BUFSIZE = 65536 };

  mythread_mutex *rdlock;
  VStream *FileStream; // source stream of the zipfile
  VStr fname;
  const VPakFileInfo &Info; // info about the file we are reading
  //FOutputDevice *Error;

  Bytef ReadBuffer[UNZ_BUFSIZE]; // internal buffer for compressed data
  z_stream stream; // zlib stream structure for inflate
  lzma_stream lzmastream;
  lzma_options_lzma lzmaopts;
  lzma_filter filters[2];
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
  bool CheckCurrentFileCoherencyHeader (vuint32 *, vuint32);
  bool LzmaRestart (); // `pos_in_zipfile` must be valid
  void readBytes (void *buf, int length);
  void cacheAllData ();

public:
  VZipFileReader (const VStr &afname, VStream *, vuint32, const VPakFileInfo &, mythread_mutex *ardlock);
  virtual ~VZipFileReader () override;
  virtual const VStr &GetName () const override;
  virtual void Serialise (void *, int) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
};


//==========================================================================
//
//  VZipFile::VZipFile
//
//  takes ownership
//
//==========================================================================
VZipFile::VZipFile (VStream *fstream)
  : VPakFileBase("<memory>", true)
{
  mythread_mutex_init(&rdlock);
  if (fstream->GetName().length()) PakFileName = fstream->GetName();
  OpenArchive(fstream);
}


//==========================================================================
//
//  VZipFile::VZipFile
//
//  takes ownership
//
//==========================================================================
VZipFile::VZipFile (VStream *fstream, const VStr &name)
  : VPakFileBase(name, true)
{
  mythread_mutex_init(&rdlock);
  OpenArchive(fstream);
}


//==========================================================================
//
//  VZipFile::VZipFile
//
//==========================================================================
VZipFile::VZipFile (const VStr &zipfile)
  : VPakFileBase(zipfile, true)
{
  mythread_mutex_init(&rdlock);
  //if (fsys_report_added_paks) GCon->Logf(NAME_Init, "Adding \"%s\"...", *PakFileName);
  auto fstream = FL_OpenSysFileRead(PakFileName);
  check(fstream);
  OpenArchive(fstream);
}


//==========================================================================
//
//  VZipFile::~VZipFile
//
//==========================================================================
VZipFile::~VZipFile () {
  Close();
  mythread_mutex_destroy(&rdlock);
}


//==========================================================================
//
//  VZipFile::VZipFile
//
//==========================================================================
void VZipFile::OpenArchive (VStream *fstream) {
  FileStream = fstream;
  check(FileStream);

  vuint32 central_pos = SearchCentralDir();
  if (central_pos == 0 || (vint32)central_pos == -1) Sys_Error("cannot load zip/pk3 file \"%s\"", *PakFileName);
  //check(central_pos);

  FileStream->Seek(central_pos);

  vuint32 Signature;
  vuint16 number_disk; // number of the current dist, used for spaning ZIP
  vuint16 number_disk_with_CD; // number the the disk with central dir, used for spaning ZIP
  vuint16 number_entry_CD; // total number of entries in the central dir (same than number_entry on nospan)
  vuint16 size_comment; // size of the global comment of the zipfile
  vuint16 NumFiles;

  *FileStream
    // the signature, already checked
    << Signature
    // number of this disk
    << number_disk
    // number of the disk with the start of the central directory
    << number_disk_with_CD
    // total number of entries in the central dir on this disk
    << NumFiles
    // total number of entries in the central dir
    << number_entry_CD;

  check(number_entry_CD == NumFiles);
  check(number_disk_with_CD == 0);
  check(number_disk == 0);

  vuint32 size_central_dir; // size of the central directory
  vuint32 offset_central_dir; // offset of start of central directory with respect to the starting disk number

  *FileStream
    << size_central_dir
    << offset_central_dir
    << size_comment;

  check(central_pos >= offset_central_dir+size_central_dir);

  BytesBeforeZipFile = central_pos-(offset_central_dir+size_central_dir);

  //files = new VPakFileInfo[NumFiles];

  bool canHasPrefix = true;

  // set the current file of the zipfile to the first file
  vuint32 pos_in_central_dir = offset_central_dir;
  for (int i = 0; i < NumFiles; ++i) {
    //VPakFileInfo &file_info = files[i];
    VPakFileInfo file_info;

    FileStream->Seek(pos_in_central_dir+BytesBeforeZipFile);

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
    *FileStream
      << Magic
      << version
      << version_needed
      << file_info.flag
      << file_info.compression
      << dosDate
      << file_info.crc32
      << file_info.packedsize
      << file_info.filesize
      << file_info.filenamesize
      << size_file_extra
      << size_file_comment
      << disk_num_start
      << internal_fa
      << external_fa
      << file_info.pakdataofs;

    if (Magic != 0x02014b50) Sys_Error("corrupted ZIP file \"%s\"", *fstream->GetName());

    char *filename_inzip = new char[file_info.filenamesize+1];
    filename_inzip[file_info.filenamesize] = '\0';
    FileStream->Serialise(filename_inzip, file_info.filenamesize);
    VStr zfname = VStr(filename_inzip).ToLower().FixFileSlashes();
    delete[] filename_inzip;
    filename_inzip = nullptr;

    // fix some idiocity introduced by some shitdoze doom tools
    for (;;) {
           if (zfname.startsWith("./")) zfname.chopLeft(2);
      else if (zfname.startsWith("../")) zfname.chopLeft(3);
      else if (zfname.startsWith("/")) zfname.chopLeft(1);
      else break;
    }
    file_info.fileName = zfname;

    if (canHasPrefix && file_info.fileName.IndexOf('/') == -1) canHasPrefix = false;

    if (canHasPrefix) {
      for (const VPK3ResDirInfo *di = PK3ResourceDirs; di->pfx; ++di) {
        if (file_info.fileName.StartsWith(di->pfx)) { canHasPrefix = false; break; }
      }
      if (canHasPrefix) {
        for (const char **dn = moreresdirs; *dn; ++dn) {
          if (file_info.fileName.StartsWith(*dn)) { canHasPrefix = false; break; }
        }
      }
    }

    pakdir.append(file_info);

    // set the current file of the zipfile to the next file
    pos_in_central_dir += SIZECENTRALDIRITEM+file_info.filenamesize+size_file_extra+size_file_comment;
  }

  // find and remove common prefix
  if (canHasPrefix && pakdir.files.length() > 0) {
    VStr xpfx = pakdir.files[0].fileName;
    int sli = xpfx.IndexOf('/');
    if (sli > 0) {
      xpfx = VStr(xpfx, 0, sli+1); // extract prefix
      for (int i = 0; i < pakdir.files.length(); ++i) {
        if (!pakdir.files[i].fileName.StartsWith(xpfx)) { canHasPrefix = false; break; }
      }
      if (canHasPrefix) {
        // remove prefix
        for (int i = 0; i < pakdir.files.length(); ++i) {
          pakdir.files[i].fileName = VStr(pakdir.files[i].fileName, sli+1, pakdir.files[i].fileName.length()-sli-1);
          //printf("new: <%s>\n", *Files[i].Name);
        }
      }
    }
  }

  pakdir.buildLumpNames();
  pakdir.buildNameMaps();

  GLog.WriteLine("added '%s' (%d lumps)", *PakFileName, pakdir.lumpmap.length());
}


//==========================================================================
//
//  VZipFile::SearchCentralDir
//
//  Locate the Central directory of a zipfile (at the end, just before
// the global comment)
//
//==========================================================================
struct SelfDestructBuf {
  vuint8 *buf;

  SelfDestructBuf (vint32 sz) {
    buf = (vuint8 *)Z_Malloc(sz);
    if (!buf) Sys_Error("Out of memory!");
  }

  ~SelfDestructBuf () { Z_Free(buf); }
};


//==========================================================================
//
//  VZipFile::SearchCentralDir
//
//==========================================================================
vuint32 VZipFile::SearchCentralDir () {
  enum { MaxBufSize = 65578 };

  vint32 fsize = FileStream->TotalSize();
  if (fsize < 16) return 0;

  vuint32 rd = (fsize < MaxBufSize ? fsize : MaxBufSize);
  SelfDestructBuf sdbuf(rd);

  FileStream->Seek(fsize-rd);
  if (FileStream->IsError()) return 0;
  FileStream->Serialise(sdbuf.buf, rd);
  if (FileStream->IsError()) return 0;

  for (int f = rd-8; f >= 0; --f) {
    if (sdbuf.buf[f] == 0x50 && sdbuf.buf[f+1] == 0x4b && sdbuf.buf[f+2] == 0x05 && sdbuf.buf[f+3] == 0x06) {
      // i found her!
      return fsize-rd+f;
    }
  }

  return 0;
}


//==========================================================================
//
//  VZipFile::Close
//
//==========================================================================
void VZipFile::Close () {
  VPakFileBase::Close();
  if (FileStream) { delete FileStream; FileStream = nullptr; }
}


//==========================================================================
//
//  VZipFile::OpenFileRead
//
//==========================================================================
VStream *VZipFile::OpenFileRead (const VStr &fname) {
  int fidx = pakdir.findFile(fname);
  if (fidx < 0) return nullptr;
  return new VZipFileReader(PakFileName+":"+fname, FileStream, BytesBeforeZipFile, pakdir.files[fidx], &rdlock);
}


//==========================================================================
//
//  VZipFile::CreateLumpReaderNum
//
//==========================================================================
VStream *VZipFile::CreateLumpReaderNum (int Lump) {
  check(Lump >= 0);
  check(Lump < pakdir.files.length());
  return new VZipFileReader(PakFileName+":"+pakdir.files[Lump].fileName, FileStream, BytesBeforeZipFile, pakdir.files[Lump], &rdlock);
}



//==========================================================================
//
//  VZipFileReader::VZipFileReader
//
//==========================================================================
VZipFileReader::VZipFileReader (const VStr &afname, VStream *InStream, vuint32 BytesBeforeZipFile,
                               const VPakFileInfo &aInfo, mythread_mutex *ardlock)
  : rdlock(ardlock)
  , FileStream(InStream)
  , fname(afname)
  , Info(aInfo)
  //, Error(InError)
  , wholeBuf(nullptr)
  , wholeSize(-2)
  , currpos(0)
{
  // open the file in the zip
  usezlib = true;

  if (!rdlock) Sys_Error("VZipFileReader::VZipFileReader: empty lock!");

  //MyThreadLocker locker(rdlock);

  vuint32 iSizeVar;
  if (!CheckCurrentFileCoherencyHeader(&iSizeVar, BytesBeforeZipFile)) {
    bError = true;
    return;
  }

  stream_initialised = false;
  lzmastream = LZMA_STREAM_INIT;

  if (Info.compression != Z_STORE && Info.compression != Z_DEFLATED && Info.compression != Z_LZMA) {
    bError = true;
    GLog.WriteLine("Compression method %d is not supported for file '%s'", Info.compression, *afname);
    return;
  }

  Crc32 = 0;

  stream.total_out = 0;
  lzmastream.total_out = 0;
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
      GLog.WriteLine("Failed to initialise inflate stream for file '%s'", *fname);
      return;
    }
    stream_initialised = true;
  } else if (Info.compression == Z_LZMA) {
    // LZMA
    usezlib = false;
    if (!LzmaRestart()) return; // error already set
  }

  stream.avail_in = 0;
  lzmastream.avail_in = 0;
  bLoading = true;
  //Error->Logf("***LOADING '%s'", *fname);
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
const VStr &VZipFileReader::GetName () const {
  return fname;
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
    if (usezlib) inflateEnd(&stream); else lzma_end(&lzmastream);
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
  //MyThreadLocker locker(rdlock);

  if (wholeBuf) { Z_Free(wholeBuf); wholeBuf = nullptr; }
  wholeSize = -2;

  if (stream_initialised) {
    //Error->Logf("***CLOSING '%s'", *fname);
    /*
    if (!bError && rest_read_uncompressed == 0) {
      if (Crc32 != Info.crc32) {
        bError = true;
        Error->Logf("Bad CRC for file '%s'", *fname);
      }
    }
    */
    if (usezlib) inflateEnd(&stream); else lzma_end(&lzmastream);
    stream_initialised = false;
  }

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
    GLog.WriteLine("Cannot lzma-restart non-lzma stream for file '%s'", *fname);
    return false;
  }

  if (stream_initialised) { lzma_end(&lzmastream); stream_initialised = false; }
  rest_read_uncompressed = Info.filesize;
  lzmastream = LZMA_STREAM_INIT;

  if (rest_read_compressed < 4+5) {
    bError = true;
    GLog.WriteLine("Invalid lzma header (out of data) for file '%s'", *fname);
    return false;
  }

  MyThreadLocker locker(rdlock);

  FileStream->Seek(pos_in_zipfile);
  FileStream->Serialise(ziplzmahdr, 4);
  FileStream->Serialise(lzmaprhdr, 5);
  bool err = FileStream->IsError();
  rest_read_compressed -= 4+5;

  if (err) {
    bError = true;
    GLog.WriteLine("Error reading lzma headers for file '%s'", *fname);
    return false;
  }

  if (ziplzmahdr[3] != 0 || ziplzmahdr[2] == 0 || ziplzmahdr[2] < 5) {
    bError = true;
    GLog.WriteLine("Invalid lzma header (0) for file '%s'", *fname);
    return false;
  }

  if (ziplzmahdr[2] > 5) {
    vuint32 skip = ziplzmahdr[2]-5;
    if (rest_read_compressed < skip) {
      bError = true;
      GLog.WriteLine("Invalid lzma header (out of extra data) for file '%s'", *fname);
      return false;
    }
    rest_read_compressed -= skip;
    vuint8 tmp;
    for (; skip > 0; --skip) {
      *FileStream << tmp;
      if (FileStream->IsError()) {
        bError = true;
        GLog.WriteLine("Error reading extra lzma headers for file '%s'", *fname);
        return false;
      }
    }
  }

#ifdef K8_UNLZMA_DEBUG
  fprintf(stderr, "LZMA: %u bytes in header, pksize=%d, unpksize=%d\n", (unsigned)(FileStream->Tell()-pos_in_zipfile), (int)Info.packedsize, (int)Info.filesize);
#endif

  lzma_lzma_preset(&lzmaopts, 9|LZMA_PRESET_EXTREME);
  filters[0].id = LZMA_FILTER_LZMA1;
  filters[0].options = &lzmaopts;
  filters[1].id = LZMA_VLI_UNKNOWN;

  vuint32 prpsize;
  if (lzma_properties_size(&prpsize, &filters[0]) != LZMA_OK) {
    bError = true;
    GLog.WriteLine("Failed to initialise lzma stream for file '%s'", *fname);
    return false;
  }
  if (prpsize != 5) {
    bError = true;
    GLog.WriteLine("Failed to initialise lzma stream for file '%s'", *fname);
    return false;
  }

  if (lzma_properties_decode(&filters[0], nullptr, lzmaprhdr, prpsize) != LZMA_OK) {
    bError = true;
    GLog.WriteLine("Failed to initialise lzma stream for file '%s'", *fname);
    return false;
  }

  if (lzma_raw_decoder(&lzmastream, &filters[0]) != LZMA_OK) {
    bError = true;
    GLog.WriteLine("Failed to initialise lzma stream for file '%s'", *fname);
    return false;
  }

  pos_in_zipfile = FileStream->Tell();

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
    GLog.WriteLine("Error reading archive for file '%s'", *fname);
    return false;
  }

  if (Magic != 0x04034b50) {
    GLog.WriteLine("Bad file magic for file '%s'", *fname);
    return false;
  }

  if (ComprMethod != Info.compression) {
    GLog.WriteLine("Compression method doesn't match for file '%s'", *fname);
    return false;
  }

  if (Crc != Info.crc32 && (Flags&8) == 0) {
    GLog.WriteLine("CRC doesn't match for file '%s'", *fname);
    return false;
  }

  if (ComprSize != Info.packedsize && (Flags&8) == 0) {
    GLog.WriteLine("Compressed size doesn't match for file '%s'", *fname);
    return false;
  }

  if (UncomprSize != (vuint32)Info.filesize && (Flags&8) == 0) {
    GLog.WriteLine("Uncompressed size doesn't match for file '%s'", *fname);
    return false;
  }

  if (FileNameSize != Info.filenamesize) {
    GLog.WriteLine("File name length doesn't match for file '%s'", *fname);
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
  check(length >= 0);
  if (length == 0) return;

  if ((vuint32)length > rest_read_uncompressed) {
    //Error->Logf("Want to read past the end of the file '%s': rest=%d; length=%d", *GetName(), (vint32)rest_read_uncompressed, length);
    setError();
    return;
  }

  stream.next_out = (Bytef *)buf;
  stream.avail_out = length;
  lzmastream.next_out = (Bytef *)buf;
  lzmastream.avail_out = length;

  int iRead = 0;
  while ((usezlib ? stream.avail_out : lzmastream.avail_out) > 0) {
    // read compressed data (if necessary)
    if ((usezlib ? stream.avail_in : lzmastream.avail_in) == 0 && rest_read_compressed > 0) {
      vuint32 uReadThis = UNZ_BUFSIZE;
      if (rest_read_compressed < uReadThis) uReadThis = rest_read_compressed;
#ifdef K8_UNLZMA_DEBUG
      if (!usezlib) fprintf(stderr, "LZMA: reading compressed bytes from ofs %u\n", (unsigned)(pos_in_zipfile-start_pos));
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
        GLog.WriteLine("Failed to read from zip for file '%s'", *fname);
        return;
      }
#ifdef K8_UNLZMA_DEBUG
      if (!usezlib) fprintf(stderr, "LZMA: read %d compressed bytes\n", uReadThis);
#endif
      //Error->Logf("Read %u packed bytes from '%s' (%u left)", uReadThis, *GetName(), rest_read_compressed-uReadThis);
      pos_in_zipfile += uReadThis;
      rest_read_compressed -= uReadThis;
      stream.next_in = ReadBuffer;
      stream.avail_in = uReadThis;
      lzmastream.next_in = ReadBuffer;
      lzmastream.avail_in = uReadThis;
    }

    // decompress data
    if (Info.compression == Z_STORE) {
      // stored data
      if (stream.avail_in == 0 && rest_read_compressed == 0) break;
      int uDoCopy = (stream.avail_out < stream.avail_in ? stream.avail_out : stream.avail_in);
      check(uDoCopy > 0);
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
        GLog.WriteLine("Decompression failed: %s for file '%s'", stream.msg, *fname);
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
#ifdef K8_UNLZMA_DEBUG
      fprintf(stderr, "LZMA: processing %u compressed bytes into %u uncompressed bytes\n", (unsigned)lzmastream.avail_in, (unsigned)lzmastream.avail_out);
      auto inbefore = lzmastream.avail_in;
#endif
      auto outbefore = lzmastream.avail_out;
      const Bytef *bufBefore = lzmastream.next_out;
      int err = lzma_code(&lzmastream, LZMA_RUN);
      if (err != LZMA_OK && err != LZMA_STREAM_END) {
        setError();
        GLog.WriteLine("LZMA decompression failed (%d) for file '%s'", err, *fname);
        return;
      }
      vuint32 uOutThis = outbefore-lzmastream.avail_out;
#ifdef K8_UNLZMA_DEBUG
      fprintf(stderr, "LZMA: processed %u packed bytes, unpacked %u bytes (err=%d); (want %d, got so far %d, left %d : %d)\n", (unsigned)(inbefore-lzmastream.avail_in), uOutThis, err, length, iRead, length-iRead, length-iRead-uOutThis);
#endif
      Crc32 = crc32(Crc32, bufBefore, (uInt)uOutThis);
      rest_read_uncompressed -= uOutThis;
      iRead += (uInt)uOutThis; //(uInt)(uTotalOutAfter - uTotalOutBefore);
      if (err != LZMA_OK) {
#ifdef K8_UNLZMA_DEBUG
        fprintf(stderr, "LZMA: stream end\n");
#endif
        break;
      }
    }
  }

  if (iRead != length) {
    GLog.WriteLine("Only read %d of %d bytes for file '%s'", iRead, length, *fname);
    if (!bError) setError();
  } else if (!bError && rest_read_uncompressed == 0) {
    if (Crc32 != Info.crc32) {
      setError();
      GLog.WriteLine("Bad CRC for file '%s'", *fname);
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
    GLog.WriteLine("Cannot read into nullptr buffer for file '%s'", *fname);
    return;
  }

  // use data cache?
  if (wholeSize >= 0) {
    //Error->Logf("  ***READING '%s' (currpos=%d; size=%d; left=%d; len=%d)", *fname, currpos, wholeSize, wholeSize-currpos, length);
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
  //Error->Logf("Back-seek in '%s' (curr=%d; new=%d)", *GetName(), Tell(), InPos);
  Crc32 = 0;
  rest_read_compressed = Info.packedsize;
  rest_read_uncompressed = Info.filesize;
  pos_in_zipfile = start_pos;
  if (Info.compression == Z_DEFLATED) {
    check(stream_initialised);
    check(usezlib);
    if (stream_initialised) inflateEnd(&stream);
    memset(&stream, 0, sizeof(stream));
    verify(inflateInit2(&stream, -MAX_WBITS) == Z_OK);
  } else if (Info.compression == Z_LZMA) {
#ifdef K8_UNLZMA_DEBUG
    fprintf(stderr, "LZMA: seek to %d (now at %d)\n", InPos, Tell());
#endif
    check(stream_initialised);
    check(!usezlib);
    if (!LzmaRestart()) return; // error already set
  } else {
    memset(&stream, 0, sizeof(stream));
  }
  // cache data
  wholeSize = (vint32)Info.filesize;
  //Error->Logf("*** CACHING '%s' (cpos=%d; newpos=%d; size=%d)", *GetName(), cpos, InPos, wholeSize);
  if (wholeSize < 0) { setError(); return; }
  wholeBuf = (vuint8 *)Z_Malloc(wholeSize ? wholeSize : 1);
  readBytes(wholeBuf, wholeSize);
  //Error->Logf("*** CACHED: %s", (bError ? "error" : "ok"));
}


//==========================================================================
//
//  VZipFileReader::Seek
//
//==========================================================================
void VZipFileReader::Seek (int InPos) {
  check(InPos >= 0);
  check(InPos <= (int)Info.filesize);

  if (bError) return;

  if (wholeSize >= 0) {
    //Error->Logf("  ***SEEKING '%s' (currpos=%d; size=%d; newpos=%d)", *fname, currpos, wholeSize, InPos);
    currpos = InPos;
    return;
  }

  // if seeking backwards, reset input stream to the begining of the file
  const int cpos = Tell();
  if (InPos < cpos) {
    // check if we have to cache data
    ++wholeSize;
    if (wholeSize >= 0) {
      //Error->Logf("*** (0)CACHING '%s' (cpos=%d; newpos=%d; size=%u)", *GetName(), cpos, InPos, Info.filesize);
      cacheAllData();
      currpos = InPos;
      return;
    }
    //Error->Logf("Back-seek in '%s' (curr=%d; new=%d)", *GetName(), Tell(), InPos);
    Crc32 = 0;
    rest_read_compressed = Info.packedsize;
    rest_read_uncompressed = Info.filesize;
    pos_in_zipfile = start_pos;
    if (Info.compression == Z_DEFLATED) {
      check(stream_initialised);
      check(usezlib);
      if (stream_initialised) inflateEnd(&stream);
      memset(&stream, 0, sizeof(stream));
      verify(inflateInit2(&stream, -MAX_WBITS) == Z_OK);
    } else if (Info.compression == Z_LZMA) {
#ifdef K8_UNLZMA_DEBUG
      fprintf(stderr, "LZMA: seek to %d (now at %d)\n", InPos, Tell());
#endif
      check(stream_initialised);
      check(!usezlib);
      if (!LzmaRestart()) return; // error already set
    } else {
      memset(&stream, 0, sizeof(stream));
    }
  } else {
    // cache file if we're seekint near the end
    if (cpos < 32768 && InPos >= (vint32)(Info.filesize-Info.filesize/3)) {
      ++wholeSize;
      if (wholeSize >= 0) {
        //Error->Logf("*** (1)CACHING '%s' (cpos=%d; newpos=%d; size=%u)", *GetName(), cpos, InPos, Info.filesize);
        cacheAllData();
        currpos = InPos;
        return;
      }
    }
  }

  // read data into a temporary buffer untill we reach needed position
  int ToSkip = InPos-Tell();
  check(ToSkip >= 0);
  if (ToSkip > 0) {
    int bsz = ToSkip;
    if (bsz > 65536) bsz = 65536;
    vuint8 tmpbuf[512];
    vuint8 *tmpbufptr;
    if (bsz <= (int)sizeof(tmpbuf)) {
      tmpbufptr = tmpbuf;
    } else {
      check(!wholeBuf);
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
  return (wholeSize >= 0 ? currpos : (usezlib ? stream.total_out : lzmastream.total_out));
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
