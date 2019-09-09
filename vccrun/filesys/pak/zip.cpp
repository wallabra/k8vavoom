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

/*
#define MAX_WBITS 15
#define MAX_MEM_LEVEL 9

#ifndef VAVOOM_USE_LIBLZMA
static const ISzAlloc fsysLzmaAlloc = {
  .Alloc = [](ISzAllocPtr p, size_t size) -> void * { void *res = ::malloc((int)size); if (!res) Sys_Error("out of memory!"); return res; },
  .Free = [](ISzAllocPtr p, void *addr) -> void { if (addr) ::free(addr); },
};
#endif
*/


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
  virtual VStr getNameByIndex (int idx) const override;
  virtual int getNameCount () const override;
  // should return `nullptr` on failure
  virtual VStream *openWithIndex (int idx) override;

public:
  VZipFile (VStream* fstream, VStr aname=VStr("<memory>")); // takes ownership on success
  virtual ~VZipFile() override;

  inline bool isOpened () const { return (fileStream != nullptr); }
};


// /////////////////////////////////////////////////////////////////////////// /
// takes ownership
VZipFile::VZipFile (VStream *fstream, VStr aname)
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


VStr VZipFile::getNameByIndex (int idx) const {
  if (idx < 0 || idx >= fileCount) return VStr::EmptyString;
  return files[idx].name;
}


int VZipFile::getNameCount () const {
  return fileCount;
}


void VZipFile::openArchive () {
  vuint32 central_pos = searchCentralDir();
  if (central_pos == 0) { fileStream = nullptr; return; } // signal failure
  //vassert(central_pos);

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

  //vassert(number_entry_CD == fileCount);
  //vassert(number_disk_with_CD == 0);
  //vassert(number_disk == 0);

  fileCount = fcount16;

  //fprintf(stderr, "sign==0x%08x; nd=%u; cd=%u; fc=%d; ecd=%u\n", Signature, number_disk, number_disk_with_CD, fileCount, number_entry_CD);

  vuint32 size_central_dir; // size of the central directory
  vuint32 offset_central_dir; // offset of start of central directory with respect to the starting disk number

  *fileStream
    << size_central_dir
    << offset_central_dir
    << size_comment;

  //vassert(central_pos >= offset_central_dir+size_central_dir);

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
    } else if (file_info.compression_method != Z_STORE && file_info.compression_method != MZ_DEFLATED && file_info.compression_method != Z_LZMA) {
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


// ////////////////////////////////////////////////////////////////////////// //
#include "zipfilerd.cpp"


VStream *VZipFile::openWithIndex (int idx) {
  if (idx < 0 || idx >= fileCount) return nullptr;
  return new VZipFileReader(fileStream, bytesBeforeZipFile, files[idx], this);
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
