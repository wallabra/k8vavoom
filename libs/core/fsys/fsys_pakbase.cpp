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
#include "fsys_local.h"


extern bool fsys_skipSounds;
extern bool fsys_skipSprites;
extern bool fsys_skipDehacked;
bool fsys_no_dup_reports = false;


// ////////////////////////////////////////////////////////////////////////// //
const VPK3ResDirInfo PK3ResourceDirs[] = {
  { "sprites/", WADNS_Sprites },
  { "flats/", WADNS_Flats },
  { "colormaps/", WADNS_ColorMaps },
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
  ".com",
  ".bat",
  ".ini",
  ".cpp",
  //".acs",
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


//==========================================================================
//
//  VFS_ShouldIgnoreExt
//
//==========================================================================
bool VFS_ShouldIgnoreExt (VStr fname) {
  if (fname.length() == 0) return false;
  for (const char **s = PK3IgnoreExts; *s; ++s) if (fname.endsWithNoCase(*s)) return true;
  //if (fname.length() > 12 && fname.endsWithNoCase(".txt")) return true;
  return false;
}



//==========================================================================
//
//  VSearchPath::VSearchPath
//
//==========================================================================
VSearchPath::VSearchPath () : iwad(false), basepak(false), normalwad(false) {
}


//==========================================================================
//
//  VSearchPath::~VSearchPath
//
//==========================================================================
VSearchPath::~VSearchPath () {
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


struct FilterItem {
  int fileno; // file index
  int filter; // filter number

  FilterItem () {}
  FilterItem (int afno, int afidx) : fileno(afno), filter(afidx) {}
};


//==========================================================================
//
//  VFileDirectory::buildLumpNames
//
//  won't touch entries with `lumpName != NAME_None`
//
//  this also performs filtering
//
//==========================================================================
void VFileDirectory::buildLumpNames () {
  if (files.length() > 65520) Sys_Error("Archive \"%s\" contains too many files", *getArchiveName());

  TMapNC<VName, FilterItem> flumpmap; // lump name -> filter info
  TMap<VStr, FilterItem> fnamemap; // file name -> filter info

  for (int i = 0; i < files.length(); ++i) {
    VPakFileInfo &fi = files[i];
    fi.fileName = fi.fileName.toLowerCase(); // just in case

    if (fi.lumpName != NAME_None) {
      if (!VStr::isLowerCase(*fi.lumpName)) {
        GLog.Logf(NAME_Warning, "Archive \"%s\" contains non-lowercase lump name '%s'", *getArchiveName(), *fi.lumpName);
      }
    } else {
      if (fi.fileName.length() == 0) continue;

      //!!!HACK!!!
      if (fi.fileName.Cmp("default.cfg") == 0) continue;
      if (fi.fileName.Cmp("startup.vs") == 0) continue;

      VStr origName = fi.fileName;
      int fidx = -1;

      // filtering
      if (fi.fileName.startsWith("filter/")) {
        VStr fn = fi.fileName;
        fidx = FL_CheckFilterName(fn);
        //if (fidx >= 0) GLog.Logf("FILTER CHECK: fidx=%d; oname=<%s>; name=<%s>", fidx, *origName, *fn);
        if (fidx < 0) {
          // hide this file
          fi.fileName.clear(); // hide this file
          continue;
        }
        fi.fileName = fn;
      }

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

      // do filtering
      if (fidx >= 0) {
        // filter hit: check file name
        VStr fn = fi.fileName;
        auto np = fnamemap.find(fn);
        if (np) {
          // found previous file: hide it if it has lower filter index
          if (np->filter > fidx) {
            // hide this file
            //GLog.Logf("removed '%s'", *origName);
            fi.fileName.clear();
            continue;
          }
        }
        // put this file to name map
        fnamemap.put(fn, FilterItem(i, fidx));
        // check lump name
        VName ln = (lumpName.length() ? VName(*lumpName, VName::AddLower8) : NAME_None);
        if (ln != NAME_None) {
          auto lp = flumpmap.find(ln);
          if (lp) {
            // found previous lump: hide it if it has lower filter index
            if (lp->filter > fidx) {
              // don't register this file as a lump
              continue;
            }
          }
          // put this lump to lump map
          flumpmap.put(ln, FilterItem(i, fidx));
        }
        //GLog.Logf("replaced '%s' -> '%s' (%s)", *origName, *fn, *lumpName);
      }

      // final lump name
      if (lumpName.length() != 0) {
        fi.lumpName = VName(*lumpName, VName::AddLower8);
      }
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
void VFileDirectory::buildNameMaps (bool rebuilding, VPakFileBase *pak) {
  bool doReports = (rebuilding ? false : !fsys_no_dup_reports);
  if (doReports) {
    VStr fn = getArchiveName().ExtractFileBaseName().toLowerCase();
    doReports =
      fn != "doom.wad" &&
      fn != "doomu.wad" &&
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
  int seenZScriptLump = -1; // so we can calculate checksum later
  vuint32 squareChecked = 0u;
  if (fsys_ignoreSquare) squareChecked = ~0u;
  lumpmap.clear();
  filemap.clear();
  TMap<VStr, bool> dupsReported;
  TMapNC<VName, int> lastSeenLump;
  bool warnZScript = true;
  bool zscriptAllowed = false;
  for (int f = 0; f < files.length(); ++f) {
    VPakFileInfo &fi = files[f];
    // link lumps
    VName lmp = fi.lumpName;
    fi.nextLump = -1; // just in case
    if (/*lmp != NAME_None &&*/ seenZScriptLump < 0 && fi.lumpNamespace == WADNS_Global && VStr::strEquCI(*lmp, "zscript")) {
      seenZScriptLump = f;
      // check for "adventures of square"
      if (!fsys_IgnoreZScript && !squareChecked) {
        for (auto &&fit : files) {
               if (fit.fileName.strEquCI("SQU-SWE1.txt")) squareChecked |= 0b0001u;
          else if (fit.fileName.strEquCI("GAMEINFO.sq")) squareChecked |= 0b0010u;
          else if (fit.fileName.strEquCI("acs/sqcommon.o")) squareChecked |= 0b0100u;
          else if (fit.fileName.strEquCI("acs/sq_jump.o")) squareChecked |= 0b1000u;
        }
        if (squareChecked == 0b1111u) {
          //fsys_IgnoreZScript = true;
          zscriptAllowed = true;
          fsys_DisableBDW = true;
          warnZScript = false;
          GLog.Log(NAME_Init, "Detected PWAD: 'Adventures of Square'");
        } else {
          squareChecked = ~0u;
        }
      }
      // ignore it for now
      fi.lumpName = NAME_None;
      continue;
    }

    if (lmp != NAME_None) {
      auto lsidp = lastSeenLump.find(lmp);
      if (!lsidp) {
        // new lump
        lumpmap.put(lmp, f);
        lastSeenLump.put(lmp, f); // for index chain
      } else {
        // we'we seen it before
        vassert(files[*lsidp].nextLump == -1);
        files[*lsidp].nextLump = f; // link to previous one
        *lsidp = f; // update index
        if (doReports) {
          if (lmp == "decorate" || lmp == "sndinfo" || lmp == "dehacked") {
            if (!dupsReported.put(fi.fileName, true)) {
              GLog.Logf(NAME_Warning, "duplicate file \"%s\" in archive \"%s\".", *fi.fileName, *getArchiveName());
              GLog.Logf(NAME_Warning, "THIS IS FUCKIN' WRONG. DO NOT USE BROKEN TOOLS TO CREATE %s FILES!", (aszip ? "ARCHIVE" : "WAD"));
            }
          }
        }
      }
    }

    if (fi.fileName.length()) {
      if (doReports) {
        if ((aszip || lmp == "decorate") && filemap.has(fi.fileName)) {
          if (!dupsReported.put(fi.fileName, true)) {
            GLog.Logf(NAME_Warning, "duplicate file \"%s\" in archive \"%s\".", *fi.fileName, *getArchiveName());
            GLog.Logf(NAME_Warning, "THIS IS FUCKIN' WRONG. DO NOT USE BROKEN TOOLS TO CREATE %s FILES!", (aszip ? "ARCHIVE" : "WAD"));
          }
        } else if (f > 0) {
          for (int pidx = f-1; pidx >= 0; --pidx) {
            if (files[pidx].fileName.length()) {
              if (files[pidx].fileName == fi.fileName) {
                if (!dupsReported.put(fi.fileName, true)) {
                  //GLog.Logf(NAME_Warning, "duplicate file \"%s\" in archive \"%s\" (%d:%d).", *fi.fileName, *getArchiveName(), pidx, f);
                  GLog.Logf(NAME_Warning, "duplicate file \"%s\" in archive \"%s\".", *fi.fileName, *getArchiveName());
                  GLog.Logf(NAME_Warning, "THIS IS FUCKIN' WRONG. DO NOT USE BROKEN TOOLS TO CREATE %s FILES!", (aszip ? "ARCHIVE" : "WAD"));
                }
              }
              break;
            }
          }
        }
      }
      // put files into hashmap
      filemap.put(fi.fileName, f);
    }
    //if (fsys_dev_dump_paks) GLog.Logf(NAME_Debug, "%s: %s", *PakFileName, *Files[f].fileName);
  }

  // seen zscript, and not a square?
  if (!zscriptAllowed && seenZScriptLump >= 0) {
    // detect boringternity
    if (pak) {
      //GLog.Logf(NAME_Debug, "*** seenZScriptLump=%d (%s); size=%d (%p); rebuilding=%d", seenZScriptLump, *files[seenZScriptLump].fileName, files[seenZScriptLump].filesize, pak, (int)rebuilding);
      if (files[seenZScriptLump].filesize == 13153 && pak->CalculateMD5(seenZScriptLump) == "9e53e2d46de1d0f6cfc004c74e1660cf") {
        // this is possibly boringternity, do some more checks?
        GLog.Log(NAME_Init, "Detected PWAD: boringternity (or something with weather system from 'mapping sadness 5'");
        zscriptAllowed = true;
        //fsys_IgnoreZScript = true;
        warnZScript = false;
      }
    }
    // bomb out
    if (!zscriptAllowed) {
      if (fsys_IgnoreZScript) {
        if (warnZScript) { warnZScript = false; GLog.Logf(NAME_Error, "Archive \"%s\" contains zscript!", *getArchiveName()); }
      } else {
        Sys_Error("Archive \"%s\" contains zscript!", *getArchiveName());
      }
    }
  }

  if (!rebuilding && fsys_dev_dump_paks) {
    GLog.Logf("======== PAK: %s ========", *getArchiveName());
    for (int f = 0; f < files.length(); ++f) {
      VPakFileInfo &fi = files[f];
      GLog.Logf("  %d: file=<%s>; lump=<%s>; ns=%d; size=%d; ofs=%d", f, *fi.fileName, *fi.lumpName, fi.lumpNamespace, fi.filesize, fi.pakdataofs);
    }
  }
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
    if (ns == WADNS_Any || files[f].lumpNamespace == ns) return true;
  }
  return false;
}


//==========================================================================
//
//  VFileDirectory::findFile
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
    if (ns == WADNS_Any || files[f].lumpNamespace == ns) return f;
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
    if (ns == WADNS_Any || files[f].lumpNamespace == ns) res = f;
  }
  return res;
}


//==========================================================================
//
//  VFileDirectory::nextLump
//
//==========================================================================
int VFileDirectory::nextLump (vint32 curridx, vint32 ns, bool allowEmptyName8) {
  if (curridx < 0) curridx = 0;
  for (; curridx < files.length(); ++curridx) {
    if (!allowEmptyName8 && files[curridx].lumpName == NAME_None) continue;
    if (ns == WADNS_Any || files[curridx].lumpNamespace == ns) return curridx;
  }
  return -1;
}



//==========================================================================
//
//  VPakFileBase::VPakFileBase
//
//==========================================================================
VPakFileBase::VPakFileBase (VStr apakfilename, bool aaszip)
  : VSearchPath()
  , PakFileName(apakfilename)
  , pakdir(this, aaszip)
  , archStream(nullptr)
  , rdlockInited(false)
{
  initLock();
}


//==========================================================================
//
//  VPakFileBase::~VPakFileBase
//
//==========================================================================
VPakFileBase::~VPakFileBase () {
  Close();
  // just in case
  if (archStream) { archStream->Close(); delete archStream; archStream = nullptr; }
  deinitLock();
}


//==========================================================================
//
//  VPakFileBase::initLock
//
//==========================================================================
void VPakFileBase::initLock () {
  if (!rdlockInited) {
    mythread_mutex_init(&rdlock);
    rdlockInited = true;
  }
}


//==========================================================================
//
//  VPakFileBase::deinitLock
//
//==========================================================================
void VPakFileBase::deinitLock () {
  if (rdlockInited) {
    rdlockInited = false;
    mythread_mutex_destroy(&rdlock);
  }
}


//==========================================================================
//
//  VPakFileBase::GetPrefix
//
//==========================================================================
VStr VPakFileBase::GetPrefix () {
  return PakFileName;
}


//==========================================================================
//
//  VPakFileBase::RenameSprites
//
//==========================================================================
void VPakFileBase::RenameSprites (const TArray<VSpriteRename> &A, const TArray<VLumpRename> &LA) {
  bool wasRename = false;
  for (auto &&L : pakdir.files) {
    //VPakFileInfo &L = pakdir.files[i];
    if (L.lumpNamespace != WADNS_Sprites) continue;
    for (auto &&ren : A) {
      if ((*L.lumpName)[0] != ren.Old[0] ||
          (*L.lumpName)[1] != ren.Old[1] ||
          (*L.lumpName)[2] != ren.Old[2] ||
          (*L.lumpName)[3] != ren.Old[3])
      {
        continue;
      }
      char newname[16];
      auto len = (int)strlen(*L.lumpName);
      if (len) {
        if (len > 12) len = 12;
        memcpy(newname, *L.lumpName, len);
      }
      newname[len] = 0;
      newname[0] = ren.New[0];
      newname[1] = ren.New[1];
      newname[2] = ren.New[2];
      newname[3] = ren.New[3];
      GLog.Logf(NAME_Dev, "%s: renaming sprite '%s' to '%s'", *PakFileName, *L.lumpName, newname);
      L.lumpName = newname;
      wasRename = true;
    }
    for (auto &&lra : LA) {
      if (L.lumpName == lra.Old) {
        L.lumpName = lra.New;
        wasRename = true;
      }
    }
  }
  if (wasRename) pakdir.buildNameMaps(true);
}


//==========================================================================
//
//  VPakFileBase::FileExists
//
//==========================================================================
bool VPakFileBase::FileExists (VStr fname) {
  return pakdir.fileExists(fname);
}


//==========================================================================
//
//  VPakFileBase::Close
//
//==========================================================================
void VPakFileBase::Close () {
  pakdir.clear();
  if (archStream) { archStream->Close(); delete archStream; archStream = nullptr; }
  deinitLock();
}


//==========================================================================
//
//  VPakFileBase::CheckNumForName
//
//==========================================================================
int VPakFileBase::CheckNumForName (VName lumpName, EWadNamespace NS, bool wantFirst) {
  int res = (wantFirst ? pakdir.findFirstLump(lumpName, NS) : pakdir.findLastLump(lumpName, NS));
  //GLog.Logf("CheckNumForName:<%s>: ns=%d; first=%d; res=%d; name=<%s> (%s)", *PakFileName, NS, (int)wantFirst, res, *pakdir.normalizeLumpName(lumpName), *lumpName);
  return res;
}


//==========================================================================
//
//  VPakFileBase::CheckNumForFileName
//
//==========================================================================
int VPakFileBase::CheckNumForFileName (VStr fileName) {
  return pakdir.findFile(fileName);
}


//==========================================================================
//
//  VPakFileBase::FindACSObject
//
//==========================================================================
int VPakFileBase::FindACSObject (VStr fname) {
  VStr afn = fname.ExtractFileBaseName();
  if (afn.ExtractFileExtension().strEquCI(".o")) afn = afn.StripExtension();
  if (afn.length() == 0) return -1;
  VName ln = VName(*afn, VName::AddLower8);
  if (fsys_developer_debug) GLog.Logf(NAME_Dev, "*** ACS: looking for '%s' (normalized: '%s'; shorten: '%s')", *fname, *afn, *ln);
  int rough = -1;
  int prevlen = -1;
  const int count = pakdir.files.length();
  char sign[4];
  for (int f = 0; f < count; ++f) {
    const VPakFileInfo &fi = pakdir.files[f];
    if (fi.lumpNamespace != WADNS_ACSLibrary) continue;
    if (fi.lumpName != ln) continue;
    if (fi.filesize >= 0 && fi.filesize < 12) continue; // why not? (it is <0 for disk mounts, so check them anyway)
    if (fsys_developer_debug) GLog.Logf(NAME_Dev, "*** ACS0: fi='%s', lump='%s', ns=%d (%d)", *fi.fileName, *fi.lumpName, fi.lumpNamespace, WADNS_ACSLibrary);
    const VStr fn = fi.fileName.ExtractFileBaseName().StripExtension();
    if (fn.ICmp(afn) == 0) {
      // exact match
      if (fsys_developer_debug) GLog.Logf(NAME_Dev, "*** ACS0: %s", *fi.fileName);
      memset(sign, 0, 4);
      ReadFromLump(f, sign, 0, 4);
      if (memcmp(sign, "ACS", 3) != 0) continue;
      if (sign[3] != 0 && sign[3] != 'E' && sign[3] != 'e') continue;
      if (fsys_developer_debug) GLog.Logf(NAME_Dev, "*** ACS0: %s -- HIT!", *fi.fileName);
      return f;
    }
    if (rough < 0 || (fn.length() >= afn.length() && prevlen > fn.length())) {
      if (fsys_developer_debug) GLog.Logf(NAME_Dev, "*** ACS1: %s", *fi.fileName);
      memset(sign, 0, 4);
      ReadFromLump(f, sign, 0, 4);
      if (memcmp(sign, "ACS", 3) != 0) continue;
      if (sign[3] != 0 && sign[3] != 'E' && sign[3] != 'e') continue;
      if (fsys_developer_debug) GLog.Logf(NAME_Dev, "*** ACS1: %s -- HIT!", *fi.fileName);
      rough = f;
      prevlen = fn.length();
    }
  }
  return rough;
}


//==========================================================================
//
//  VPakFileBase::ReadFromLump
//
//==========================================================================
void VPakFileBase::ReadFromLump (int Lump, void *Dest, int Pos, int Size) {
  vassert(Lump >= 0);
  vassert(Lump < pakdir.files.length());
  vassert(Size >= 0);
  if (Size == 0) return;
  VStream *Strm = CreateLumpReaderNum(Lump);
  if (!Strm) Sys_Error("error reading lump '%s:%s'", *GetPrefix(), *pakdir.files[Lump].fileName);
  // special case: unpacked size is unknown, cache it
  int fsize = pakdir.files[Lump].filesize;
  if (fsize == -1) {
    pakdir.files[Lump].filesize = Strm->TotalSize();
    fsize = pakdir.files[Lump].filesize;
  }
  if (fsize < 0 || Pos >= fsize || fsize-Pos < Size) {
    delete Strm;
    Sys_Error("error reading lump '%s:%s' (out of data)", *GetPrefix(), *pakdir.files[Lump].fileName);
  }
  Strm->Seek(Pos);
  Strm->Serialise(Dest, Size);
  bool wasError = Strm->IsError();
  delete Strm;
  if (wasError) Sys_Error("error reading lump '%s:%s'", *GetPrefix(), *pakdir.files[Lump].fileName);
}


//==========================================================================
//
//  VPakFileBase::LumpLength
//
//==========================================================================
int VPakFileBase::LumpLength (int Lump) {
  if (Lump < 0 || Lump >= pakdir.files.length()) return 0;
  // special case: unpacked size is unknown, read and cache it
  if (pakdir.files[Lump].filesize == -1) {
    VStream *Strm = CreateLumpReaderNum(Lump);
    if (!Strm) Sys_Error("cannot get length for lump '%s:%s'", *GetPrefix(), *pakdir.files[Lump].fileName);
    pakdir.files[Lump].filesize = Strm->TotalSize();
    bool wasError = Strm->IsError();
    delete Strm;
    if (wasError || pakdir.files[Lump].filesize < 0) Sys_Error("cannot get length for lump '%s:%s'", *GetPrefix(), *pakdir.files[Lump].fileName);
  }
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
int VPakFileBase::IterateNS (int Start, EWadNamespace NS, bool allowEmptyName8) {
  return pakdir.nextLump(Start, NS, allowEmptyName8);
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


//==========================================================================
//
//  VPakFileBase::CalculateMD5
//
//==========================================================================
VStr VPakFileBase::CalculateMD5 (int lumpidx) {
  if (lumpidx < 0 || lumpidx >= pakdir.files.length()) return VStr::EmptyString;
  VStream *strm = CreateLumpReaderNum(lumpidx);
  if (!strm) return VStr::EmptyString;
  MD5Context md5ctx;
  md5ctx.Init();
  enum { BufSize = 65536 };
  vuint8 *buf = new vuint8[BufSize];
  auto left = strm->TotalSize();
  while (left > 0) {
    int rd = (left > BufSize ? BufSize : left);
    strm->Serialise(buf, rd);
    if (strm->IsError()) {
      delete[] buf;
      delete strm;
      return VStr::EmptyString;
    }
    left -= rd;
    md5ctx.Update(buf, (unsigned)rd);
  }
  delete[] buf;
  delete strm;
  vuint8 md5digest[MD5Context::DIGEST_SIZE];
  md5ctx.Final(md5digest);
  return VStr::buf2hex(md5digest, MD5Context::DIGEST_SIZE);
}


//==========================================================================
//
//  VPakFileBase::OpenFileRead
//
//==========================================================================
VStream *VPakFileBase::OpenFileRead (VStr fname) {
  int fidx = pakdir.findFile(fname);
  if (fidx < 0) return nullptr;
  return CreateLumpReaderNum(fidx);
}
