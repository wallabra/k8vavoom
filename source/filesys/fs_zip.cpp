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
#include "gamedefs.h"
#include "fs_local.h"
#ifdef USE_INTERNAL_LZMA
# include "../../libs/liblzma/api/lzma.h"
#else
# include <lzma.h>
#endif
#ifdef USE_INTERNAL_ZLIB
# include "../../libs/zlib/zlib.h"
#else
# include <zlib.h>
#endif

#define Z_LZMA  (14)
#define Z_STORE  (0)

//#define K8_UNLZMA_DEBUG

extern bool fsys_skipSounds;
extern bool fsys_skipSprites;
extern bool fsys_skipDehacked;


enum {
  SIZECENTRALDIRITEM = 0x2e,
  SIZEZIPLOCALHEADER = 0x1e,
};


// information about a file in the zipfile
struct VZipFileInfo {
  VStr Name; // name of the file
  vuint16 flag; // general purpose bit flag
  vuint16 compression_method; // compression method
  vuint32 crc; // crc-32
  vuint32 compressed_size; // compressed size
  vuint32 uncompressed_size; // uncompressed size
  vuint16 size_filename; // filename length
  vuint32 offset_curfile; // relative offset of local header
  // for WAD-like access
  VName LumpName;
  vint32 LumpNamespace;
  int nextLump; // next lump with the same name
};


// ////////////////////////////////////////////////////////////////////////// //
class VZipFileReader : public VStream {
private:
  //enum { UNZ_BUFSIZE = 16384 };
  enum { UNZ_BUFSIZE = 65536 };

  mythread_mutex *rdlock;
  VStream *FileStream; // source stream of the zipfile
  VStr fname;
  const VZipFileInfo &Info; // info about the file we are reading
  FOutputDevice *Error;

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

  bool CheckCurrentFileCoherencyHeader (vuint32 *, vuint32);
  bool LzmaRestart (); // `pos_in_zipfile` must be valid

public:
  VZipFileReader (const VStr &afname, VStream *, vuint32, const VZipFileInfo &, FOutputDevice *, mythread_mutex *ardlock);
  virtual ~VZipFileReader () override;
  virtual const VStr &GetName () const override;
  virtual void Serialise (void *, int) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
};


// ////////////////////////////////////////////////////////////////////////// //
extern "C" {
  int FileCmpFunc (const void *v1, const void *v2) {
    return ((VZipFileInfo *)v1)->Name.Cmp(((VZipFileInfo *)v2)->Name);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
const VPK3ResDirInfo PK3ResourceDirs[] = {
  { "sprites/", WADNS_Sprites },
  { "flats/", WADNS_Flats },
  { "colormaps/", WADNS_ColourMaps },
  { "acs/", WADNS_ACSLibrary },
  { "textures/", WADNS_NewTextures },
  { "voices/", WADNS_Voices },
  { "hires/", WADNS_HiResTextures },
  { "patches/", WADNS_Patches },
  { "graphics/", WADNS_Graphics },
  { "sounds/", WADNS_Sounds },
  { "music/", WADNS_Music },
  { nullptr, WADNS_ZipSpecial },
};


static const char *moreresdirs[] = {
  "models/",
  nullptr,
};


static const char *PK3IgnoreExts[] = {
  ".wad",
  ".zip",
  ".7z",
  ".pk3",
  ".pk7",
  ".exe",
  ".bat",
  ".ini",
  ".cpp",
  ".acs",
  ".doc",
  ".me",
  ".rtf",
  ".rsp",
  ".now",
  ".htm",
  ".html",
  ".wri",
  ".nfo",
  ".diz",
  ".bbs",
  nullptr
};


bool VFS_ShouldIgnoreExt (const VStr &fname) {
  if (fname.length() == 0) return false;
  for (const char **s = PK3IgnoreExts; *s; ++s) if (fname.endsWithNoCase(*s)) return true;
  //if (fname.length() > 12 && fname.endsWithNoCase(".txt")) return true;
  return false;
}


//==========================================================================
//
//  VZipFile::VZipFile
//
//  takes ownership
//
//==========================================================================
VZipFile::VZipFile (VStream *fstream)
  : ZipFileName("<memory>")
  , Files(nullptr)
  , NumFiles(0)
  , filemap()
  , lumpmap()
{
  guard(VZipFile::VZipFile);
  mythread_mutex_init(&rdlock);
  OpenArchive(fstream);
  unguard;
}


//==========================================================================
//
//  VZipFile::VZipFile
//
//  takes ownership
//
//==========================================================================
VZipFile::VZipFile (VStream *fstream, const VStr &name)
  : ZipFileName(name)
  , Files(nullptr)
  , NumFiles(0)
{
  guard(VZipFile::VZipFile);
  mythread_mutex_init(&rdlock);
  OpenArchive(fstream);
  unguard;
}


//==========================================================================
//
//  VZipFile::VZipFile
//
//==========================================================================
VZipFile::VZipFile (const VStr &zipfile)
  : ZipFileName(zipfile)
  , Files(nullptr)
  , NumFiles(0)
{
  guard(VZipFile::VZipFile);
  mythread_mutex_init(&rdlock);
  if (fsys_report_added_paks) GCon->Logf(NAME_Init, "Adding \"%s\"...", *ZipFileName);
  auto fstream = FL_OpenSysFileRead(ZipFileName);
  check(fstream);
  OpenArchive(fstream);
  unguard;
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
  guard(VZipFile::OpenArchive);
  FileStream = fstream;
  check(FileStream);

  vuint32 central_pos = SearchCentralDir();
  check(central_pos);

  FileStream->Seek(central_pos);

  vuint32 Signature;
  vuint16 number_disk; // number of the current dist, used for spaning ZIP
  vuint16 number_disk_with_CD; // number the the disk with central dir, used for spaning ZIP
  vuint16 number_entry_CD; // total number of entries in the central dir (same than number_entry on nospan)
  vuint16 size_comment; // size of the global comment of the zipfile

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

  Files = new VZipFileInfo[NumFiles];

  bool canHasPrefix = true;

  // set the current file of the zipfile to the first file
  vuint32 pos_in_central_dir = offset_central_dir;
  for (int i = 0; i < NumFiles; ++i) {
    VZipFileInfo &file_info = Files[i];

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
      << file_info.compression_method
      << dosDate
      << file_info.crc
      << file_info.compressed_size
      << file_info.uncompressed_size
      << file_info.size_filename
      << size_file_extra
      << size_file_comment
      << disk_num_start
      << internal_fa
      << external_fa
      << file_info.offset_curfile;

    if (Magic != 0x02014b50) Sys_Error("corrupted ZIP file \"%s\"", *fstream->GetName());

    char *filename_inzip = new char[file_info.size_filename+1];
    filename_inzip[file_info.size_filename] = '\0';
    FileStream->Serialise(filename_inzip, file_info.size_filename);
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
    Files[i].Name = zfname;

    if (canHasPrefix && Files[i].Name.IndexOf('/') == -1) canHasPrefix = false;

    if (canHasPrefix) {
      for (const VPK3ResDirInfo *di = PK3ResourceDirs; di->pfx; ++di) {
        if (Files[i].Name.StartsWith(di->pfx)) { canHasPrefix = false; break; }
      }
      if (canHasPrefix) {
        for (const char **dn = moreresdirs; *dn; ++dn) {
          if (Files[i].Name.StartsWith(*dn)) { canHasPrefix = false; break; }
        }
      }
    }

    // set the current file of the zipfile to the next file
    pos_in_central_dir += SIZECENTRALDIRITEM+file_info.size_filename+size_file_extra+size_file_comment;
  }

  // find and remove common prefix
  if (canHasPrefix && NumFiles > 0) {
    VStr xpfx = Files[0].Name;
    int sli = xpfx.IndexOf('/');
    if (sli > 0) {
      xpfx = VStr(xpfx, 0, sli+1); // extract prefix
      for (int i = 0; i < NumFiles; ++i) {
        if (!Files[i].Name.StartsWith(xpfx)) { canHasPrefix = false; break; }
      }
      if (canHasPrefix) {
        // remove prefix
        for (int i = 0; i < NumFiles; ++i) {
          Files[i].Name = VStr(Files[i].Name, sli+1, Files[i].Name.Length()-sli-1);
          //printf("new: <%s>\n", *Files[i].Name);
        }
      }
    }
  }

  // remove duplicate files
  /*
  TMap<VStr, bool> nameset;
  for (int i = NumFiles-1; i >= 0; --i) {
    if (nameset.has(Files[i].Name)) {
      Files[i].Name = VStr("\x01");
    } else {
      nameset.put(Files[i].Name, true);
    }
  }
  */

  // build lump names
  for (int i = 0; i < NumFiles; ++i) {
    if (Files[i].Name.Length() > 0) {
      // set up lump name for WAD-like access
      VStr LumpName = Files[i].Name.ExtractFileName().StripExtension();

      // map some directories to WAD namespaces
      if (Files[i].Name.IndexOf('/') == -1) {
        Files[i].LumpNamespace = WADNS_Global;
      } else {
        Files[i].LumpNamespace = -1;
        for (const VPK3ResDirInfo *di = PK3ResourceDirs; di->pfx; ++di) {
          if (Files[i].Name.StartsWith(di->pfx)) {
            Files[i].LumpNamespace = di->wadns;
            break;
          }
        }
      }

      // anything from other directories won't be accessed as lump
      if (Files[i].LumpNamespace == -1) {
        LumpName = VStr();
      } else {
        // hide wad files, 'cause they may conflict with normal files
        // wads will be correctly added by a separate function
        if (VFS_ShouldIgnoreExt(Files[i].Name)) {
          Files[i].LumpNamespace = -1;
          LumpName = VStr();
        }
      }

      if ((fsys_skipSounds && Files[i].LumpNamespace == WADNS_Sounds) ||
          (fsys_skipSprites && Files[i].LumpNamespace == WADNS_Sprites))
      {
        Files[i].LumpNamespace = -1;
        LumpName = VStr();
      }

      if (fsys_skipDehacked && LumpName.length() && LumpName.ICmp("dehacked") == 0) {
        Files[i].LumpNamespace = -1;
        LumpName = VStr();
      }

      // for sprites \ is a valid frame character but is not allowed to
      // be in a file name, so we do a little mapping here
      if (Files[i].LumpNamespace == WADNS_Sprites) {
        LumpName = LumpName.Replace("^", "\\");
      }

      //if (LumpName.length() == 0) fprintf(stderr, "ZIP <%s> mapped to nothing\n", *Files[i].Name);
      //fprintf(stderr, "ZIP <%s> mapped to <%s> (%d)\n", *Files[i].Name, *LumpName, Files[i].LumpNamespace);

      // final lump name
      if (LumpName.length()) {
        Files[i].LumpName = VName(*LumpName, VName::AddLower8);
      } else {
        Files[i].LumpName = NAME_None;
      }
    } else {
      Files[i].LumpName = NAME_None;
    }
  }

  if (NumFiles > 65520) Sys_Error("Archive '%s' has too many files", *ZipFileName);
  // sort files alphabetically (have to do this, or file searching is failing for some reason)
  // k8: it seems that we don't need to sort files anymore
  if (GArgs.CheckParm("-pk3sort")) {
    GCon->Logf(NAME_Init, "sorting files in '%s'...", *fstream->GetName());
    qsort(Files, NumFiles, sizeof(VZipFileInfo), FileCmpFunc);
  }

  bool dumpZips = GArgs.CheckParm("-dev-dump-zips");

  // now create hashmaps, and link lumps
  TMapNC<VName, int> lastSeenLump;
  for (int f = 0; f < NumFiles; ++f) {
    // link lumps
    VName lmp = Files[f].LumpName;
    Files[f].nextLump = -1; // just in case
    if (lmp != NAME_None) {
      if (!lumpmap.has(lmp)) {
        // new lump
        lumpmap.put(lmp, f);
        lastSeenLump.put(lmp, f); // for index chain
      } else {
        // we'we seen it before
        auto lsidp = lastSeenLump.find(lmp); // guaranteed to succeed
        Files[*lsidp].nextLump = f; // link previous to this one
        *lsidp = f; // update index
      }
    }
    if (filemap.has(Files[f].Name)) {
      GCon->Logf(NAME_Warning, "duplicate file \"%s\" in archive \"%s\".", *Files[f].Name, *fstream->GetName());
      GCon->Log(NAME_Warning, "THIS IS FUCKIN' WRONG. DO NOT USE BROKEN TOOLS TO CREATE PK3 FILES!");
    }
    // put files into hashmap
    filemap.put(Files[f].Name, f);
    if (dumpZips) GCon->Logf(NAME_Dev, "%s: %s", *ZipFileName, *Files[f].Name);
  }

  unguard;
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
//  VZipFile::FileExists
//
//==========================================================================
bool VZipFile::FileExists (const VStr &FName) {
  return filemap.has(FName.toLowerCase());
}


//==========================================================================
//
//  VZipFile::OpenFileRead
//
//==========================================================================
VStream *VZipFile::OpenFileRead (const VStr &FName) {
  auto fp = filemap.find(FName.toLowerCase());
  if (!fp) return nullptr;
  return new VZipFileReader(ZipFileName+":"+FName, FileStream, BytesBeforeZipFile, Files[*fp], GCon, &rdlock);
}


//==========================================================================
//
//  VZipFile::Close
//
//==========================================================================
void VZipFile::Close () {
  if (Files) { delete[] Files; Files = nullptr; }
  if (FileStream) { delete FileStream; FileStream = 0; }
  filemap.clear();
  lumpmap.clear();
}


//==========================================================================
//
//  VZipFile::CheckNumForName
//
//==========================================================================
int VZipFile::CheckNumForName (VName LumpName, EWadNamespace NS) {
  if (LumpName == NAME_None) return -1;
  if (!VStr::isLowerCase(*LumpName)) LumpName = VName(*LumpName, VName::AddLower);
  auto fp = lumpmap.find(LumpName);
  if (!fp) return -1;
  int res = -1; // default: none
  // find last one
  for (int f = *fp; f >= 0; f = Files[f].nextLump) if (Files[f].LumpNamespace == NS) res = f;
  return res;
}


//==========================================================================
//
//  VZipFile::CheckNumForFileName
//
//==========================================================================
int VZipFile::CheckNumForFileName (const VStr &Name) {
  auto fp = filemap.find(Name.toLowerCase());
  //GCon->Logf(NAME_Dev, "ZIP:%s:%s is %d", *ZipFileName, *Name.toLowerCase(), (fp ? *fp : -1));
  return (fp ? *fp : -1);
}


//==========================================================================
//
//  VZipFile::ReadFromLump
//
//==========================================================================
void VZipFile::ReadFromLump (int Lump, void *Dest, int Pos, int Size) {
  guard(VZipFile::ReadFromLump);
  check(Lump >= 0);
  check(Lump < NumFiles);
  VStream *Strm = CreateLumpReaderNum(Lump);
  Strm->Seek(Pos);
  Strm->Serialise(Dest, Size);
  delete Strm;
  unguard;
}


//==========================================================================
//
//  VZipFile::LumpLength
//
//==========================================================================
int VZipFile::LumpLength (int Lump) {
  if (Lump < 0 || Lump >= NumFiles) return 0;
  return Files[Lump].uncompressed_size;
}


//==========================================================================
//
//  VZipFile::LumpName
//
//==========================================================================
VName VZipFile::LumpName (int Lump) {
  if (Lump < 0 || Lump >= NumFiles) return NAME_None;
  return Files[Lump].LumpName;
}


//==========================================================================
//
//  VZipFile::LumpFileName
//
//==========================================================================
VStr VZipFile::LumpFileName (int Lump) {
  if (Lump < 0 || Lump >= NumFiles) return VStr();
  return Files[Lump].Name;
}


//==========================================================================
//
//  VZipFile::IterateNS
//
//==========================================================================
int VZipFile::IterateNS (int Start, EWadNamespace NS) {
  if (Start < 0) Start = 0;
  if (Start >= NumFiles) return -1;
  for (int li = Start; li < NumFiles; ++li) {
    if (Files[li].LumpNamespace == NS && Files[li].LumpName != NAME_None) return li;
  }
  return -1;
}


//==========================================================================
//
//  VZipFile::CreateLumpReaderNum
//
//==========================================================================
VStream *VZipFile::CreateLumpReaderNum (int Lump) {
  guard(VZipFile::CreateLumpReaderNum);
  check(Lump >= 0);
  check(Lump < NumFiles);
  return new VZipFileReader(ZipFileName+":"+Files[Lump].Name, FileStream, BytesBeforeZipFile, Files[Lump], GCon, &rdlock);
  unguard;
}


//==========================================================================
//
//  VZipFile::RenameSprites
//
//==========================================================================
void VZipFile::RenameSprites (const TArray<VSpriteRename> &A, const TArray<VLumpRename> &LA) {
  guard(VZipFile::RenameSprites);
  for (int i = 0; i < NumFiles; ++i) {
    VZipFileInfo &L = Files[i];
    if (L.LumpNamespace != WADNS_Sprites) continue;
    for (int j = 0; j < A.Num(); ++j) {
      if ((*L.LumpName)[0] != A[j].Old[0] ||
          (*L.LumpName)[1] != A[j].Old[1] ||
          (*L.LumpName)[2] != A[j].Old[2] ||
          (*L.LumpName)[3] != A[j].Old[3])
      {
        continue;
      }
      char newname[16];
      auto len = (int)strlen(*L.Name);
      if (len) {
        if (len > 12) len = 12;
        memcpy(newname, *L.Name, len);
      }
      newname[len] = 0;
      newname[0] = A[j].New[0];
      newname[1] = A[j].New[1];
      newname[2] = A[j].New[2];
      newname[3] = A[j].New[3];
      GCon->Logf(NAME_Dev, "renaming ZIP sprite '%s' to '%s'", *L.Name, newname);
      L.LumpName = newname;
    }
    for (int j = 0; j < LA.Num(); ++j) {
      if (L.LumpName == LA[j].Old) L.LumpName = LA[j].New;
    }
  }
  unguard;
}


//==========================================================================
//
//  VZipFile::ListWadFiles
//
//==========================================================================
void VZipFile::ListWadFiles (TArray<VStr> &List) {
  guard(VZipFile::ListWadFiles);
  TMap<VStr, bool> hits;
  for (int i = 0; i < NumFiles; ++i) {
    // only .wad files
    if (!Files[i].Name.EndsWith(".wad")) continue;
    // don't add WAD files in subdirectories
    if (Files[i].Name.IndexOf('/') != -1) continue;
    if (hits.has(Files[i].Name)) continue;
    hits.put(Files[i].Name, true);
    List.Append(Files[i].Name);
  }
  unguard;
}


//==========================================================================
//
//  VZipFile::ListPk3Files
//
//==========================================================================
void VZipFile::ListPk3Files (TArray<VStr> &List) {
  guard(VZipFile::ListPk3Files);
  TMap<VStr, bool> hits;
  for (int i = 0; i < NumFiles; ++i) {
    // only .pk3 files
    if (!Files[i].Name.EndsWith(".pk3")) continue;
    // don't add pk3 files in subdirectories
    if (Files[i].Name.IndexOf('/') != -1) continue;
    if (hits.has(Files[i].Name)) continue;
    hits.put(Files[i].Name, true);
    List.Append(Files[i].Name);
  }
  unguard;
}


//==========================================================================
//
//  VZipFileReader::VZipFileReader
//
//==========================================================================
VZipFileReader::VZipFileReader (const VStr &afname, VStream *InStream, vuint32 BytesBeforeZipFile,
                               const VZipFileInfo &aInfo, FOutputDevice *InError, mythread_mutex *ardlock)
  : rdlock(ardlock)
  , FileStream(InStream)
  , fname(afname)
  , Info(aInfo)
  , Error(InError)
{
  guard(VZipFileReader::VZipFileReader);
  // open the file in the zip
  usezlib = true;

  if (!rdlock) Sys_Error("VZipFileReader::VZipFileReader: empty lock!");

  MyThreadLocker locker(rdlock);

  vuint32 iSizeVar;
  if (!CheckCurrentFileCoherencyHeader(&iSizeVar, BytesBeforeZipFile)) {
    bError = true;
    return;
  }

  stream_initialised = false;
  lzmastream = LZMA_STREAM_INIT;

  if (Info.compression_method != Z_STORE && Info.compression_method != Z_DEFLATED && Info.compression_method != Z_LZMA) {
    bError = true;
    Error->Logf("Compression method %d is not supported", Info.compression_method);
    return;
  }

  Crc32 = 0;

  stream.total_out = 0;
  lzmastream.total_out = 0;
  pos_in_zipfile = Info.offset_curfile+SIZEZIPLOCALHEADER+iSizeVar+BytesBeforeZipFile;
  start_pos = pos_in_zipfile;
  rest_read_compressed = Info.compressed_size;
  rest_read_uncompressed = Info.uncompressed_size;

  if (Info.compression_method == Z_DEFLATED) {
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
      Error->Log("Failed to initialise inflate stream");
      return;
    }
    stream_initialised = true;
  } else if (Info.compression_method == Z_LZMA) {
    // LZMA
    usezlib = false;
    if (!LzmaRestart()) return; // error already set
  }

  stream.avail_in = 0;
  lzmastream.avail_in = 0;
  bLoading = true;
  unguard;
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
    Error->Log("Cannot lzma-restart non-lzma stream");
    return false;
  }

  if (stream_initialised) { lzma_end(&lzmastream); stream_initialised = false; }
  rest_read_uncompressed = Info.uncompressed_size;
  lzmastream = LZMA_STREAM_INIT;

  if (rest_read_compressed < 4+5) {
    bError = true;
    Error->Log("Invalid lzma header (out of data)");
    return false;
  }

  FileStream->Seek(pos_in_zipfile);
  FileStream->Serialise(ziplzmahdr, 4);
  FileStream->Serialise(lzmaprhdr, 5);
  rest_read_compressed -= 4+5;

  if (FileStream->IsError()) {
    bError = true;
    Error->Log("Error reading lzma headers");
    return false;
  }

  if (ziplzmahdr[3] != 0 || ziplzmahdr[2] == 0 || ziplzmahdr[2] < 5) {
    bError = true;
    Error->Log("Invalid lzma header (0)");
    return false;
  }

  if (ziplzmahdr[2] > 5) {
    vuint32 skip = ziplzmahdr[2]-5;
    if (rest_read_compressed < skip) {
      bError = true;
      Error->Log("Invalid lzma header (out of extra data)");
      return false;
    }
    rest_read_compressed -= skip;
    vuint8 tmp;
    for (; skip > 0; --skip) {
      *FileStream << tmp;
      if (FileStream->IsError()) {
        bError = true;
        Error->Log("Error reading extra lzma headers");
        return false;
      }
    }
  }

#ifdef K8_UNLZMA_DEBUG
  fprintf(stderr, "LZMA: %u bytes in header, pksize=%d, unpksize=%d\n", (unsigned)(FileStream->Tell()-pos_in_zipfile), (int)Info.compressed_size, (int)Info.uncompressed_size);
#endif

  lzma_lzma_preset(&lzmaopts, 9|LZMA_PRESET_EXTREME);
  filters[0].id = LZMA_FILTER_LZMA1;
  filters[0].options = &lzmaopts;
  filters[1].id = LZMA_VLI_UNKNOWN;

  vuint32 prpsize;
  if (lzma_properties_size(&prpsize, &filters[0]) != LZMA_OK) {
    bError = true;
    Error->Log("Failed to initialise lzma stream");
    return false;
  }
  if (prpsize != 5) {
    bError = true;
    Error->Log("Failed to initialise lzma stream");
    return false;
  }

  if (lzma_properties_decode(&filters[0], nullptr, lzmaprhdr, prpsize) != LZMA_OK) {
    bError = true;
    Error->Log("Failed to initialise lzma stream");
    return false;
  }

  if (lzma_raw_decoder(&lzmastream, &filters[0]) != LZMA_OK) {
    bError = true;
    Error->Log("Failed to initialise lzma stream");
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
  guard(VZipFileReader::CheckCurrentFileCoherencyHeader);
  vuint32 Magic, DateTime, Crc, ComprSize, UncomprSize;
  vuint16 Version, Flags, ComprMethod, FileNameSize, ExtraFieldSize;

  *piSizeVar = 0;

  FileStream->Seek(Info.offset_curfile + byte_before_the_zipfile);

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

  if (Magic != 0x04034b50) {
    Error->Log("Bad file magic");
    return false;
  }

  if (ComprMethod != Info.compression_method) {
    Error->Log("Compression method doesn\'t match");
    return false;
  }

  if (Crc != Info.crc && (Flags&8) == 0) {
    Error->Log("CRC doesn\'t match");
    return false;
  }

  if (ComprSize != Info.compressed_size && (Flags&8) == 0) {
    Error->Log("Compressed size doesn\'t match");
    return false;
  }

  if (UncomprSize != Info.uncompressed_size && (Flags&8) == 0) {
    Error->Log("Uncompressed size doesn\'t match");
    return false;
  }

  if (FileNameSize != Info.size_filename) {
    Error->Log("File name length doesn\'t match");
    return false;
  }

  *piSizeVar += FileNameSize+ExtraFieldSize;

  return true;
  unguard;
}


//==========================================================================
//
//  VZipFileReader::Serialise
//
//==========================================================================
void VZipFileReader::Serialise (void *V, int Length) {
  guard(VZipFileReader::Serialise);
  MyThreadLocker locker(rdlock);

  if (!FileStream || bError) { bError = true; return; } // don't read anything from already broken stream
  if (FileStream->IsError()) { bError = true; return; }

  if (Length == 0) return;

  if (!V) {
    bError = true;
    Error->Log("Cannot read into nullptr buffer");
    return;
  }

  stream.next_out = (Bytef *)V;
  stream.avail_out = Length;
  lzmastream.next_out = (Bytef *)V;
  lzmastream.avail_out = Length;

  if ((vuint32)Length > rest_read_uncompressed) {
    stream.avail_out = rest_read_uncompressed;
    lzmastream.avail_out = rest_read_uncompressed;
  }

  int iRead = 0;
  while ((usezlib ? stream.avail_out : lzmastream.avail_out) > 0) {
    // read compressed data (if necessary)
    if ((usezlib ? stream.avail_in : lzmastream.avail_in) == 0 && rest_read_compressed > 0) {
      vuint32 uReadThis = UNZ_BUFSIZE;
      if (rest_read_compressed < uReadThis) uReadThis = rest_read_compressed;
#ifdef K8_UNLZMA_DEBUG
      if (!usezlib) fprintf(stderr, "LZMA: reading compressed bytes from ofs %u\n", (unsigned)(pos_in_zipfile-start_pos));
#endif
      FileStream->Seek(pos_in_zipfile);
      FileStream->Serialise(ReadBuffer, uReadThis);
      if (FileStream->IsError()) {
        bError = true;
        Error->Log("Failed to read from zip file");
        return;
      }
#ifdef K8_UNLZMA_DEBUG
      if (!usezlib) fprintf(stderr, "LZMA: read %d compressed bytes\n", uReadThis);
#endif
      pos_in_zipfile += uReadThis;
      rest_read_compressed -= uReadThis;
      stream.next_in = ReadBuffer;
      stream.avail_in = uReadThis;
      lzmastream.next_in = ReadBuffer;
      lzmastream.avail_in = uReadThis;
    }

    // decompress data
    if (Info.compression_method == Z_STORE) {
      // stored data
      if (stream.avail_in == 0 && rest_read_compressed == 0) break;
      int uDoCopy = (stream.avail_out < stream.avail_in ? stream.avail_out : stream.avail_in);
      for (int i = 0; i < uDoCopy; ++i) *(stream.next_out+i) = *(stream.next_in+i);
      Crc32 = crc32(Crc32, stream.next_out, uDoCopy);
      rest_read_uncompressed -= uDoCopy;
      stream.avail_in -= uDoCopy;
      stream.avail_out -= uDoCopy;
      stream.next_out += uDoCopy;
      stream.next_in += uDoCopy;
      stream.total_out += uDoCopy;
      iRead += uDoCopy;
    } else if (Info.compression_method == Z_DEFLATED) {
      // zlib data
      int flush = Z_SYNC_FLUSH;
      uLong uTotalOutBefore = stream.total_out;
      const Bytef *bufBefore = stream.next_out;
      int err = inflate(&stream, flush);
      if (err >= 0 && stream.msg != nullptr) {
        bError = true;
        Error->Logf("Decompression failed: %s", stream.msg);
        return;
      }
      uLong uTotalOutAfter = stream.total_out;
      vuint32 uOutThis = uTotalOutAfter - uTotalOutBefore;
      Crc32 = crc32(Crc32, bufBefore, (uInt)uOutThis);
      rest_read_uncompressed -= uOutThis;
      iRead += (uInt)(uTotalOutAfter - uTotalOutBefore);
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
        bError = true;
        Error->Logf("LZMA decompression failed (%d)", err);
        return;
      }
      vuint32 uOutThis = outbefore-lzmastream.avail_out;
#ifdef K8_UNLZMA_DEBUG
      fprintf(stderr, "LZMA: processed %u packed bytes, unpacked %u bytes (err=%d); (want %d, got so far %d, left %d : %d)\n", (unsigned)(inbefore-lzmastream.avail_in), uOutThis, err, Length, iRead, Length-iRead, Length-iRead-uOutThis);
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

  if (iRead != Length) {
    bError = true;
    Error->Logf("Only read %d of %d bytes", iRead, Length);
  }
  unguard;
}


//==========================================================================
//
//  VZipFileReader::Seek
//
//==========================================================================
void VZipFileReader::Seek (int InPos) {
  guard(VZipFileReader::Seek);
  check(InPos >= 0);
  check(InPos <= (int)Info.uncompressed_size);
  //MyThreadLocker locker(rdlock);

  if (bError) return;

  // if seeking backwards, reset input stream to the begining of the file
  if (InPos < Tell()) {
    Crc32 = 0;
    rest_read_compressed = Info.compressed_size;
    rest_read_uncompressed = Info.uncompressed_size;
    pos_in_zipfile = start_pos;
    if (Info.compression_method == Z_DEFLATED) {
      check(stream_initialised);
      check(usezlib);
      if (stream_initialised) inflateEnd(&stream);
      memset(&stream, 0, sizeof(stream));
      verify(inflateInit2(&stream, -MAX_WBITS) == Z_OK);
    } else if (Info.compression_method == Z_LZMA) {
#ifdef K8_UNLZMA_DEBUG
      fprintf(stderr, "LZMA: seek to %d (now at %d)\n", InPos, Tell());
#endif
      check(stream_initialised);
      check(!usezlib);
      if (!LzmaRestart()) return; // error already set
    } else {
      memset(&stream, 0, sizeof(stream));
    }
  }

  // read data into a temporary buffer untill we reach needed position
  int ToSkip = InPos-Tell();
  while (ToSkip > 0) {
    int Count = ToSkip > 1024 ? 1024 : ToSkip;
    ToSkip -= Count;
    vuint8 TmpBuf[1024];
    Serialise(TmpBuf, Count);
  }
  unguard;
}


//==========================================================================
//
//  VZipFileReader::Tell
//
//==========================================================================
int VZipFileReader::Tell () {
  return (usezlib ? stream.total_out : lzmastream.total_out);
}


//==========================================================================
//
//  VZipFileReader::TotalSize
//
//==========================================================================
int VZipFileReader::TotalSize () {
  return Info.uncompressed_size;
}


//==========================================================================
//
//  VZipFileReader::AtEnd
//
//==========================================================================
bool VZipFileReader::AtEnd () {
  return (rest_read_uncompressed == 0);
}


//==========================================================================
//
//  VZipFileReader::Close
//
//==========================================================================
bool VZipFileReader::Close () {
  guard(VZipFileReader::Close);
  //MyThreadLocker locker(rdlock);

  if (!bError && rest_read_uncompressed == 0) {
    if (Crc32 != Info.crc) {
      bError = true;
      Error->Log("Bad CRC");
    }
  }
  if (usezlib) {
    if (stream_initialised) inflateEnd(&stream);
  } else {
    if (stream_initialised) lzma_end(&lzmastream);
  }

  stream_initialised = false;
  return !bError;
  unguard;
}
