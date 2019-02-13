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
#include "gamedefs.h"
#include "fs_local.h"


extern bool fsys_skipSounds;
extern bool fsys_skipSprites;
extern bool fsys_skipDehacked;
bool fsys_no_dup_reports = false;


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
VFileDirectory::VFileDirectory (VPakFileBase *aowner, bool aaszip)
  : owner(aowner)
  , files()
  , lumpmap()
  , filemap()
  , aszip(aaszip)
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
//  VFileDirectory::appendAndRegister
//
//==========================================================================
int VFileDirectory::appendAndRegister (const VPakFileInfo &fi) {
  // link lumps
  int f = files.length();
  files.append(fi);
  VPakFileInfo &nfo = files[f];
  VName lmp = nfo.lumpName;
  nfo.nextLump = -1; // just in case
  if (lmp != NAME_None) {
    auto lp = lumpmap.find(lmp);
    if (lp) {
      int lnum = *lp, pnum = -1;
      for (;;) {
        pnum = lnum;
        lnum = files[lnum].nextLump;
        if (lnum == -1) break;
      }
      nfo.nextLump = pnum;
    }
    lumpmap.put(lmp, f);
  }
  if (nfo.fileName.length()) {
    // put files into hashmap
    filemap.put(nfo.fileName, f);
  }
  return f;
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
    fi.fileName = fi.fileName.toLowerCase(); // just in case

    if (fi.lumpName != NAME_None) {
      if (!VStr::isLowerCase(*fi.lumpName)) {
        GCon->Logf(NAME_Warning, "Archive \"%s\" contains non-lowercase lump name '%s'", *getArchiveName(), *fi.lumpName);
      }
    } else {
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
}


//==========================================================================
//
//  VFileDirectory::buildNameMaps
//
//  call this when all lump names are built
//
//==========================================================================
void VFileDirectory::buildNameMaps (bool rebuilding) {
  bool doReports = (rebuilding ? false : !fsys_no_dup_reports);
  if (doReports) {
    VStr fn = getArchiveName().ExtractFileBaseName();
    doReports =
      fn != "doom.wad" &&
      fn != "doom2.wad" &&
      fn != "tnt.wad" &&
      fn != "plutonia.wad" &&
      fn != "nerve.wad" &&
      fn != "heretic.wad" &&
      fn != "hexen.wad" &&
      fn != "strife1.wad" &&
      fn != "voices.wad" &&
      true;
  }
  lumpmap.clear();
  filemap.clear();
  //bool dumpZips = GArgs.CheckParm("-dev-dump-zips");
  TMapNC<VName, int> lastSeenLump;
  for (int f = 0; f < files.length(); ++f) {
    VPakFileInfo &fi = files[f];
    // link lumps
    VName lmp = fi.lumpName;
    fi.nextLump = -1; // just in case
    if (lmp != NAME_None && fi.lumpNamespace == WADNS_Global && VStr::Cmp(*lmp, "zscript") == 0) {
#ifdef VAVOOM_K8_DEVELOPER
      if (GArgs.CheckParm("-ignore-zscript") != 0)
#else
      if (false)
#endif
      {
        GCon->Logf(NAME_Warning, "Archive \"%s\" contains zscript", *getArchiveName());
        fi.lumpName = NAME_None;
        lmp = NAME_None;
      } else {
        Sys_Error("Archive \"%s\" contains zscript", *getArchiveName());
      }
    }
    if (lmp != NAME_None) {
      auto lsidp = lastSeenLump.find(lmp);
      if (!lsidp) {
        // new lump
        lumpmap.put(lmp, f);
        lastSeenLump.put(lmp, f); // for index chain
      } else {
        // we'we seen it before
        check(files[*lsidp].nextLump == -1);
        files[*lsidp].nextLump = f; // link to previous one
        *lsidp = f; // update index
        if (lmp == "decorate" || lmp == "sndinfo" || lmp == "dehacked") {
          GCon->Logf(NAME_Warning, "duplicate file \"%s\" in archive \"%s\".", *fi.fileName, *getArchiveName());
          GCon->Logf(NAME_Warning, "THIS IS FUCKIN' WRONG. DO NOT USE BROKEN TOOLS TO CREATE %s FILES!", (aszip ? "PK3/ZIP" : "WAD"));
        }
      }
    }
    if (fi.fileName.length()) {
      if (doReports) {
        if ((aszip || lmp == "decorate") && filemap.has(fi.fileName)) {
          GCon->Logf(NAME_Warning, "duplicate file \"%s\" in archive \"%s\".", *fi.fileName, *getArchiveName());
          GCon->Logf(NAME_Warning, "THIS IS FUCKIN' WRONG. DO NOT USE BROKEN TOOLS TO CREATE %s FILES!", (aszip ? "PK3/ZIP" : "WAD"));
        } else if (f > 0) {
          for (int pidx = f-1; pidx >= 0; --pidx) {
            if (files[pidx].fileName.length()) {
              if (files[pidx].fileName == fi.fileName) {
                //GCon->Logf(NAME_Warning, "duplicate file \"%s\" in archive \"%s\" (%d:%d).", *fi.fileName, *getArchiveName(), pidx, f);
                GCon->Logf(NAME_Warning, "duplicate file \"%s\" in archive \"%s\".", *fi.fileName, *getArchiveName());
                GCon->Logf(NAME_Warning, "THIS IS FUCKIN' WRONG. DO NOT USE BROKEN TOOLS TO CREATE %s FILES!", (aszip ? "PK3/ZIP" : "WAD"));
              }
              break;
            }
          }
        }
      }
      // put files into hashmap
      filemap.put(fi.fileName, f);
    }
    //if (dumpZips) GCon->Logf(NAME_Dev, "%s: %s", *PakFileName, *Files[f].fileName);
  }

  if (!rebuilding && GArgs.CheckParm("-dump-paks")) {
    GCon->Logf("======== PAK: %s ========", *getArchiveName());
    for (int f = 0; f < files.length(); ++f) {
      VPakFileInfo &fi = files[f];
      GCon->Logf("  %d: file=<%s>; lump=<%s>; ns=%d; size=%d; ofs=%d", f, *fi.fileName, *fi.lumpName, fi.lumpNamespace, fi.filesize, fi.pakdataofs);
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
  if (ns < 0) return *fp;
  for (int f = *fp; f >= 0; f = files[f].nextLump) {
    if (files[f].lumpNamespace == ns) return f;
  }
  return -1;
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
  int res = -1;
  for (int f = *fp; f >= 0; f = files[f].nextLump) {
    if (ns < 0 || files[f].lumpNamespace == ns) res = f;
  }
  return res;
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
VPakFileBase::VPakFileBase (const VStr &apakfilename, bool aaszip)
  : PakFileName(apakfilename)
  , pakdir(this, aaszip)
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
  int res = (wantFirst ? pakdir.findFirstLump(lumpName, NS) : pakdir.findLastLump(lumpName, NS));
  //GCon->Logf("CheckNumForName:<%s>: ns=%d; first=%d; res=%d; name=<%s> (%s)", *PakFileName, NS, (int)wantFirst, res, *pakdir.normalizeLumpName(lumpName), *lumpName);
  return res;
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
