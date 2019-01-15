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


extern bool fsys_skipSounds;
extern bool fsys_skipSprites;
extern bool fsys_skipDehacked;


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
//  VFileDirectory::VFileDirectory
//
//==========================================================================
VFileDirectory::VFileDirectory ()
  : owner(nullptr)
  , files()
  , lumpmap()
  , filemap()
{
}


//==========================================================================
//
//  VFileDirectory::VFileDirectory
//
//==========================================================================
VFileDirectory::VFileDirectory (VPakFileBase *aowner)
  : owner(aowner)
  , files()
  , lumpmap()
  , filemap()
{
}


//==========================================================================
//
//  VFileDirectory::~VFileDirectory
//
//==========================================================================
VFileDirectory::~VFileDirectory () {
}


//==========================================================================
//
//  VFileDirectory::getArchiveName
//
//==========================================================================
const VStr VFileDirectory::getArchiveName () const {
  return (owner ? owner->PakFileName : VStr::EmptyString);
}


//==========================================================================
//
//  VFileDirectory::append
//
//==========================================================================
void VFileDirectory::append (const VPakFileInfo &fi) {
  if (files.length() >= 65520) Sys_Error("Archive \"%s\" contains too many files", *getArchiveName());
  files.append(fi);
}


//==========================================================================
//
//  VFileDirectory::clear
//
//==========================================================================
void VFileDirectory::clear () {
  filemap.clear();
  lumpmap.clear();
  files.clear();
}


//==========================================================================
//
//  VFileDirectory::buildLumpNames
//
//  won't touch entries with `lumpName != NAME_None`
//
//==========================================================================
void VFileDirectory::buildLumpNames () {
  if (files.length() > 65520) Sys_Error("Archive \"%s\" contains too many files", *getArchiveName());

  for (int i = 0; i < files.length(); ++i) {
    VPakFileInfo &fi = files[i];
    if (fi.lumpName != NAME_None) continue;
    if (fi.fileName.length() == 0) continue;

    //!!!HACK!!!
    if (fi.fileName.Cmp("default.cfg") == 0) continue;
    if (fi.fileName.Cmp("startup.vs") == 0) continue;

    // set up lump name for WAD-like access
    VStr lumpName = fi.fileName.ExtractFileName().StripExtension();

    if (fi.lumpNamespace == -1) {
      // map some directories to WAD namespaces
      if (fi.fileName.IndexOf('/') == -1) {
        fi.lumpNamespace = WADNS_Global;
      } else {
        fi.lumpNamespace = -1;
        for (const VPK3ResDirInfo *di = PK3ResourceDirs; di->pfx; ++di) {
          if (fi.fileName.StartsWith(di->pfx)) {
            fi.lumpNamespace = di->wadns;
            break;
          }
        }
      }
    }

    // anything from other directories won't be accessed as lump
    if (fi.lumpNamespace == -1) {
      lumpName = VStr();
    } else {
      // hide wad files, 'cause they may conflict with normal files
      // wads will be correctly added by a separate function
      if (VFS_ShouldIgnoreExt(fi.fileName)) {
        fi.lumpNamespace = -1;
        lumpName = VStr();
      }
    }

    if ((fsys_skipSounds && fi.lumpNamespace == WADNS_Sounds) ||
        (fsys_skipSprites && fi.lumpNamespace == WADNS_Sprites))
    {
      fi.lumpNamespace = -1;
      lumpName = VStr();
    }

    if (fsys_skipDehacked && lumpName.length() && lumpName.ICmp("dehacked") == 0) {
      fi.lumpNamespace = -1;
      lumpName = VStr();
    }

    // for sprites \ is a valid frame character, but is not allowed to
    // be in a file name, so we do a little mapping here
    if (fi.lumpNamespace == WADNS_Sprites) {
      lumpName = lumpName.Replace("^", "\\");
    }

    //if (LumpName.length() == 0) fprintf(stderr, "ZIP <%s> mapped to nothing\n", *Files[i].Name);
    //fprintf(stderr, "ZIP <%s> mapped to <%s> (%d)\n", *Files[i].Name, *LumpName, Files[i].LumpNamespace);

    // final lump name
    if (lumpName.length() != 0) fi.lumpName = VName(*lumpName, VName::AddLower8);
  }
}


//==========================================================================
//
//  VFileDirectory::buildNameMaps
//
//  call this when all lump names are built
//
//==========================================================================
void VFileDirectory::buildNameMaps () {
  lumpmap.clear();
  filemap.clear();
  //bool dumpZips = GArgs.CheckParm("-dev-dump-zips");
  TMapNC<VName, int> lastSeenLump;
  for (int f = 0; f < files.length(); ++f) {
    VPakFileInfo &fi = files[f];
    // link lumps
    VName lmp = fi.lumpName;
    fi.nextLump = -1; // just in case
    if (lmp != NAME_None) {
      auto lsidp = lastSeenLump.find(lmp);
      if (!lsidp) {
        // new lump
        lumpmap.put(lmp, f);
        lastSeenLump.put(lmp, f); // for index chain
      } else {
        // we'we seen it before
        fi.nextLump = *lsidp; // link to previous one
        *lsidp = f; // update index
      }
    }
    if (fi.fileName.length()) {
      if (filemap.has(fi.fileName)) {
        GCon->Logf(NAME_Warning, "duplicate file \"%s\" in archive \"%s\".", *fi.fileName, *getArchiveName());
        GCon->Log(NAME_Warning, "THIS IS FUCKIN' WRONG. DO NOT USE BROKEN TOOLS TO CREATE PK3 FILES!");
      }
      // put files into hashmap
      filemap.put(fi.fileName, f);
    }
    //if (dumpZips) GCon->Logf(NAME_Dev, "%s: %s", *PakFileName, *Files[f].fileName);
  }

  if (GArgs.CheckParm("-dump-paks")) {
    GCon->Logf("======== PAK: %s ========", *getArchiveName());
    for (int f = 0; f < files.length(); ++f) {
      VPakFileInfo &fi = files[f];
      GCon->Logf("  %d: file=<%s>; lump=<%s>; ns=%d", f, *fi.fileName, *fi.lumpName, fi.lumpNamespace);
    }
  }
    //if (LumpName.length() == 0) fprintf(stderr, "ZIP <%s> mapped to nothing\n", *Files[i].Name);
    //fprintf(stderr, "ZIP <%s> mapped to <%s> (%d)\n", *Files[i].Name, *LumpName, Files[i].LumpNamespace);
}


//==========================================================================
//
//  VFileDirectory::normalizeFileName
//
//==========================================================================
void VFileDirectory::normalizeFileName (VStr &fname) {
  if (fname.length() == 0) return;
  for (;;) {
         if (fname.startsWith("./")) fname.chopLeft(2);
    else if (fname.startsWith("../")) fname.chopLeft(3);
    else if (fname.startsWith("/")) fname.chopLeft(1);
    else break;
  }
  if (!fname.isLowerCase()) fname = fname.toLowerCase();
  return;
}


//==========================================================================
//
//  VFileDirectory::normalizeLumpName
//
//==========================================================================
VName VFileDirectory::normalizeLumpName (VName lname) {
  if (lname == NAME_None) return NAME_None;
  if (!VStr::isLowerCase(*lname)) lname = VName(*lname, VName::AddLower);
  return lname;
}


//==========================================================================
//
//  VFileDirectory::fileExists
//
//==========================================================================
bool VFileDirectory::fileExists (VStr fname) {
  normalizeFileName(fname);
  if (fname.length() == 0) return false;
  return filemap.has(fname);
}


//==========================================================================
//
//  VFileDirectory::lumpExists
//
//  namespace -1 means "any"
//
//==========================================================================
bool VFileDirectory::lumpExists (VName lname, vint32 ns) {
  lname = normalizeLumpName(lname);
  if (lname == NAME_None) return false;
  auto fp = lumpmap.find(lname);
  if (!fp) return false;
  if (ns < 0) return true;
  for (int f = *fp; f >= 0; f = files[f].nextLump) {
    if (files[f].lumpNamespace == ns) return true;
  }
  return false;
}


//==========================================================================
//
//  VFileDirectory::findFirstFile
//
//==========================================================================
int VFileDirectory::findFile (VStr fname) {
  normalizeFileName(fname);
  if (fname.length() == 0) return false;
  auto fp = filemap.find(fname);
  return (fp ? *fp : -1);
}


//==========================================================================
//
//  VFileDirectory::findFirstLump
//
//  namespace -1 means "any"
//
//==========================================================================
int VFileDirectory::findFirstLump (VName lname, vint32 ns) {
  lname = normalizeLumpName(lname);
  if (lname == NAME_None) return -1;
  auto fp = lumpmap.find(lname);
  if (!fp) return -1;
  int res = -1;
  for (int f = *fp; f >= 0; f = files[f].nextLump) {
    if (ns < 0 || files[f].lumpNamespace == ns) res = f;
  }
  return res;
}


//==========================================================================
//
//  VFileDirectory::findLastLump
//
//==========================================================================
int VFileDirectory::findLastLump (VName lname, vint32 ns) {
  lname = normalizeLumpName(lname);
  if (lname == NAME_None) return -1;
  auto fp = lumpmap.find(lname);
  if (!fp) return -1;
  if (ns < 0) return *fp;
  for (int f = *fp; f >= 0; f = files[f].nextLump) {
    if (files[f].lumpNamespace == ns) return f;
  }
  return -1;
}


//==========================================================================
//
//  VFileDirectory::nextLump
//
//==========================================================================
int VFileDirectory::nextLump (vint32 curridx, vint32 ns) {
  if (curridx < 0) curridx = 0;
  for (; curridx < files.length(); ++curridx) {
    if (files[curridx].lumpName == NAME_None) continue;
    if (ns < 0 || files[curridx].lumpNamespace == ns) return curridx;
  }
  return -1;
}


// ////////////////////////////////////////////////////////////////////////// //
VPakFileBase::VPakFileBase (const VStr &apakfilename)
  : PakFileName(apakfilename)
  , pakdir(this)
{
}


//==========================================================================
//
//  VPakFileBase::~VPakFileBase
//
//==========================================================================
VPakFileBase::~VPakFileBase () {
  Close();
}


//==========================================================================
//
//  VPakFileBase::RenameSprites
//
//==========================================================================
void VPakFileBase::RenameSprites (const TArray<VSpriteRename> &A, const TArray<VLumpRename> &LA) {
}


//==========================================================================
//
//  VPakFileBase::FileExists
//
//==========================================================================
bool VPakFileBase::FileExists (const VStr &fname) {
  return pakdir.fileExists(fname);
}


//==========================================================================
//
//  VPakFileBase::Close
//
//==========================================================================
void VPakFileBase::Close () {
  pakdir.clear();
}


//==========================================================================
//
//  VPakFileBase::CheckNumForName
//
//==========================================================================
int VPakFileBase::CheckNumForName (VName lumpName, EWadNamespace NS, bool wantFirst) {
  return (wantFirst ? pakdir.findFirstLump(lumpName, NS) : pakdir.findLastLump(lumpName, NS));
}


//==========================================================================
//
//  VPakFileBase::CheckNumForFileName
//
//==========================================================================
int VPakFileBase::CheckNumForFileName (const VStr &fileName) {
  return pakdir.findFile(fileName);
}


//==========================================================================
//
//  VPakFileBase::ReadFromLump
//
//==========================================================================
void VPakFileBase::ReadFromLump (int Lump, void *Dest, int Pos, int Size) {
  check(Lump >= 0);
  check(Lump < pakdir.files.length());
  VStream *Strm = CreateLumpReaderNum(Lump);
  Strm->Seek(Pos);
  Strm->Serialise(Dest, Size);
  delete Strm;
}


//==========================================================================
//
//  VPakFileBase::LumpLength
//
//==========================================================================
int VPakFileBase::LumpLength (int Lump) {
  if (Lump < 0 || Lump >= pakdir.files.length()) return 0;
  return pakdir.files[Lump].filesize;
}


//==========================================================================
//
//  VPakFileBase::LumpName
//
//==========================================================================
VName VPakFileBase::LumpName (int Lump) {
  if (Lump < 0 || Lump >= pakdir.files.length()) return NAME_None;
  return pakdir.files[Lump].lumpName;
}


//==========================================================================
//
//  VPakFileBase::LumpFileName
//
//==========================================================================
VStr VPakFileBase::LumpFileName (int Lump) {
  if (Lump < 0 || Lump >= pakdir.files.length()) return VStr();
  return pakdir.files[Lump].fileName;
}


//==========================================================================
//
//  VPakFileBase::IterateNS
//
//==========================================================================
int VPakFileBase::IterateNS (int Start, EWadNamespace NS) {
  return pakdir.nextLump(Start, NS);
}


//==========================================================================
//
//  VPakFileBase::ListWadFiles
//
//==========================================================================
void VPakFileBase::ListWadFiles (TArray<VStr> &list) {
  TMap<VStr, bool> hits;
  for (int i = 0; i < pakdir.files.length(); ++i) {
    const VPakFileInfo &fi = pakdir.files[i];
    if (fi.fileName.length() < 5) continue;
    // only .wad files
    if (!fi.fileName.EndsWith(".wad")) continue;
    // don't add WAD files in subdirectories
    if (fi.fileName.IndexOf('/') != -1) continue;
    if (hits.has(fi.fileName)) continue;
    hits.put(fi.fileName, true);
    list.Append(fi.fileName);
  }
}


//==========================================================================
//
//  VPakFileBase::ListPk3Files
//
//==========================================================================
void VPakFileBase::ListPk3Files (TArray<VStr> &list) {
  TMap<VStr, bool> hits;
  for (int i = 0; i < pakdir.files.length(); ++i) {
    const VPakFileInfo &fi = pakdir.files[i];
    if (fi.fileName.length() < 5) continue;
    // only .pk3 files
    if (!fi.fileName.EndsWith(".pk3")) continue;
    // don't add pk3 files in subdirectories
    if (fi.fileName.IndexOf('/') != -1) continue;
    if (hits.has(fi.fileName)) continue;
    hits.put(fi.fileName, true);
    list.Append(fi.fileName);
  }
}
