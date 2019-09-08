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
#include "fsys_local.h"

#ifdef VAVOOM_USE_LIBLZMA
# ifdef USE_INTERNAL_LZMA
#  include "../../liblzma/api/lzma.h"
# else
#  include <lzma.h>
# endif
#endif

#ifdef USE_INTERNAL_ZLIB
# include "../../zlib/zlib.h"
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
  "filter/",
  nullptr,
};


#include "fsys_zipfread.cpp"


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
VZipFile::VZipFile (VStream *fstream, VStr name, vuint32 cdofs)
  : VPakFileBase(name, true)
{
  mythread_mutex_init(&rdlock);
  OpenArchive(fstream, cdofs);
}


//==========================================================================
//
//  VZipFile::VZipFile
//
//==========================================================================
VZipFile::VZipFile (VStr zipfile)
  : VPakFileBase(zipfile, true)
{
  mythread_mutex_init(&rdlock);
  if (fsys_report_added_paks) GLog.Logf(NAME_Init, "Adding \"%s\"...", *PakFileName);
  auto fstream = FL_OpenSysFileRead(PakFileName);
  vassert(fstream);
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
void VZipFile::OpenArchive (VStream *fstream, vuint32 cdofs) {
  FileStream = fstream;
  vassert(FileStream);

  vuint32 central_pos = (cdofs ? cdofs : SearchCentralDir(FileStream));
  if (central_pos == 0 || (vint32)central_pos == -1) {
    // check for 7zip idiocity
    if (!fstream->IsError() && fstream->TotalSize() >= 2) {
      char hdr[2] = {0};
      fstream->Seek(0);
      fstream->Serialise(hdr, 2);
      if (memcmp(hdr, "7z", 2) == 0) Sys_Error("DO NOT RENAME YOUR 7Z ARCHIVES TO PK3, THIS IS IDIOCITY! REJECTED \"%s\"", *PakFileName);
    }
    Sys_Error("cannot load zip/pk3 file \"%s\"", *PakFileName);
  }
  //vassert(central_pos);

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

  vassert(number_entry_CD == NumFiles);
  vassert(number_disk_with_CD == 0);
  vassert(number_disk == 0);

  vuint32 size_central_dir; // size of the central directory
  vuint32 offset_central_dir; // offset of start of central directory with respect to the starting disk number

  *FileStream
    << size_central_dir
    << offset_central_dir
    << size_comment;

  vassert(central_pos >= offset_central_dir+size_central_dir);

  BytesBeforeZipFile = central_pos-(offset_central_dir+size_central_dir);

  bool isPK3 = PakFileName.ExtractFileExtension().strEquCI(".pk3");
  bool canHasPrefix = true;
  if (isPK3) canHasPrefix = false; // do not remove prefixes in pk3
  //GLog.Logf("*** ARK: <%s>:<%s> pfx=%d", *PakFileName, *PakFileName.ExtractFileExtension(), (int)canHasPrefix);

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
        //GLog.Logf("*** ARK: <%s>:<%s> pfx=<%s>", *PakFileName, *PakFileName.ExtractFileExtension(), *xpfx);
        for (int i = 0; i < pakdir.files.length(); ++i) {
          pakdir.files[i].fileName = VStr(pakdir.files[i].fileName, sli+1, pakdir.files[i].fileName.length()-sli-1);
          //printf("new: <%s>\n", *Files[i].Name);
        }
      }
    }
  }

  pakdir.buildLumpNames();
  pakdir.buildNameMaps();

  // detect SkullDash EE
  if (isPK3) {
    int tmidx = pakdir.findFile("maps/titlemap.wad");
    //GLog.Logf("***%s: tmidx=%d", *PakFileName, tmidx);
    if (tmidx >= 0 && pakdir.files[tmidx].filesize == 286046) {
      //GLog.Logf("*** TITLEMAP: %s", *CalculateMD5(tmidx));
      // check md5
      if (CalculateMD5(tmidx) == "def4f5e00c60727aeb3a25d1982cfcf1") {
        fsys_detected_mod = AD_SKULLDASHEE;
      }
    }
  }
}


//==========================================================================
//
//  SelfDestructBuf
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
//  locate the Central directory of a zipfile
//  (at the end, just before the global comment)
//
//==========================================================================
vuint32 VZipFile::SearchCentralDir (VStream *strm) {
  enum { MaxBufSize = 65578 };

  vint32 fsize = strm->TotalSize();
  if (fsize < 16) return 0;

  vuint32 rd = (fsize < MaxBufSize ? fsize : MaxBufSize);
  SelfDestructBuf sdbuf(rd);

  strm->Seek(fsize-rd);
  if (strm->IsError()) return 0;
  strm->Serialise(sdbuf.buf, rd);
  if (strm->IsError()) return 0;

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
//  VZipFile::CreateLumpReaderNum
//
//==========================================================================
VStream *VZipFile::CreateLumpReaderNum (int Lump) {
  vassert(Lump >= 0);
  vassert(Lump < pakdir.files.length());
  return new VZipFileReader(PakFileName+":"+pakdir.files[Lump].fileName, FileStream, BytesBeforeZipFile, pakdir.files[Lump], &rdlock);
}


// ////////////////////////////////////////////////////////////////////////// //
static VSearchPath *openArchiveZIP (VStream *strm, VStr filename, bool FixVoices) {
  if (strm->TotalSize() < 16) return nullptr;
  vuint32 cdofs = VZipFile::SearchCentralDir(strm);
  if (cdofs == 0) return nullptr;
  return new VZipFile(strm, filename, cdofs);
}


// checking for this is slow, but the sorter will check it last, as it has no signature
// still, give it lower priority in case we'll have other signature-less formats
FArchiveReaderInfo vavoom_fsys_archive_opener_zip("zip", &openArchiveZIP, nullptr, 999);
