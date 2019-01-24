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


extern VCvarB game_release_mode;
//extern VCvarI game_override_mode;

static VCvarB dbg_dump_gameinfo("dbg_dump_gameinfo", false, "Dump parsed game.txt?", 0);
static VCvarB gz_skip_menudef("_gz_skip_menudef", false, "Skip gzdoom menudef parsing?", 0);

bool fsys_skipSounds = false;
bool fsys_skipSprites = false;
bool fsys_skipDehacked = false;
bool fsys_hasPwads = false; // or paks
bool fsys_hasMapPwads = false; // or paks
bool fsys_ignoreZScript = false;


int fsys_warp_n0 = -1;
int fsys_warp_n1 = -1;
VStr fsys_warp_cmd;

static bool fsys_onlyOneBaseFile = false;

struct AuxFile {
  VStr name;
  bool optional;
};


struct version_t {
  VStr param;
  TArray<VStr> MainWads;
  VStr GameDir;
  TArray<AuxFile> AddFiles;
  TArray<VStr> BaseDirs;
  int ParmFound;
  bool FixVoices;
  VStr warp;
};


VStr fl_basedir;
VStr fl_savedir;
VStr fl_gamedir;

TArray<VSearchPath *> SearchPaths;
bool fsys_report_added_paks = true;
//static VCvarB fsys_report_system_wads("fsys_report_system_wads", false, "Report loaded system wads?", CVAR_Archive);
//static VCvarB fsys_report_user_wads("fsys_report_user_wads", true, "Report loaded user wads?", CVAR_Archive);

TArray<VStr> wadfiles;
//static bool bIwadAdded;
static TArray<VStr> IWadDirs;
static int IWadIndex;
static VStr warpTpl;


// ////////////////////////////////////////////////////////////////////////// //
struct PWadFile {
  VStr fname;
  bool skipSounds;
  bool skipSprites;
  bool skipDehacked;
  bool storeInSave;
  bool asDirectory;

  PWadFile ()
    : fname()
    , skipSounds(false)
    , skipSprites(false)
    , skipDehacked(false)
    , storeInSave(false)
    , asDirectory(false)
  {}
};


static TArray<PWadFile> pwadList;
static bool doStartMap = false;


static void collectPWads () {
  int fp = GArgs.CheckParm("-file");
  if (!fp) return;
  bool wasAnyFile = false;
  bool skipSounds = false;
  bool skipSprites = false;
  bool skipDehacked = fsys_skipDehacked;
  bool storeInSave = true;
  // setup flags
  for (int f = 1; f < fp; ++f) {
         if (VStr::Cmp(GArgs[f], "-skipsounds") == 0) skipSounds = true;
    else if (VStr::Cmp(GArgs[f], "-allowsounds") == 0) skipSounds = false;
    else if (VStr::Cmp(GArgs[f], "-skipsprites") == 0) skipSprites = true;
    else if (VStr::Cmp(GArgs[f], "-allowsprites") == 0) skipSprites = false;
    else if (VStr::Cmp(GArgs[f], "-skipdehacked") == 0) skipDehacked = true;
    else if (VStr::Cmp(GArgs[f], "-allowdehacked") == 0) skipDehacked = false;
  }
  // process pwads
  bool inFile = true;
  while (++fp < GArgs.Count()) {
    const char *arg = GArgs[fp];
    if (!arg || !arg[0]) continue;
    if (arg[0] == '-' || arg[0] == '+') {
           if (VStr::Cmp(arg, "-skipsounds") == 0) skipSounds = true;
      else if (VStr::Cmp(arg, "-allowsounds") == 0) skipSounds = false;
      else if (VStr::Cmp(arg, "-skipsprites") == 0) skipSprites = true;
      else if (VStr::Cmp(arg, "-allowsprites") == 0) skipSprites = false;
      else if (VStr::Cmp(arg, "-skipdehacked") == 0) skipDehacked = true;
      else if (VStr::Cmp(arg, "-allowdehacked") == 0) skipDehacked = false;
      else if (VStr::Cmp(arg, "-cosmetic") == 0) storeInSave = false;
      else { inFile = (VStr::Cmp(arg, "-file") == 0); if (inFile) { wasAnyFile = false; storeInSave = true; } }
      continue;
    }
    if (!inFile) continue;

    PWadFile pwf;
    pwf.skipSounds = skipSounds;
    pwf.skipSprites = skipSprites;
    pwf.skipDehacked = skipDehacked;
    pwf.storeInSave = storeInSave;
    pwf.asDirectory = false;

    if (Sys_DirExists(arg)) {
      //REVERTED: never append dirs to saves, 'cause it is meant to be used by developers
      pwf.asDirectory = true;
      if (!wasAnyFile) {
        wasAnyFile = true;
      } else {
        GCon->Logf(NAME_Init, "To mount directory '%s' as emulated PK3 file, you should use \"-file\".", arg);
        continue;
      }
    } else if (Sys_FileExists(arg)) {
      wasAnyFile = true;
    } else {
      GCon->Logf(NAME_Init, "WARNING: File \"%s\" doesn't exist.", arg);
      continue;
    }

    pwf.fname = arg;
    pwadList.append(pwf);

    storeInSave = true; // autoreset
  }
}


static void tempMount (const PWadFile &pwf) {
  if (pwf.fname.isEmpty()) return;

  W_StartAuxiliary(); // just in case

  if (pwf.asDirectory) {
    VDirPakFile *dpak = new VDirPakFile(pwf.fname);
    //if (!dpak->hasFiles()) { delete dpak; return; }

    SearchPaths.append(dpak);

    // add all WAD files in the root
    TArray<VStr> wads;
    dpak->ListWadFiles(wads);
    for (int i = 0; i < wads.length(); ++i) {
      VStream *wadst = dpak->OpenFileRead(wads[i]);
      if (!wadst) continue;
      W_AddAuxiliaryStream(wadst, WAuxFileType::Wad);
    }

    // add all pk3 files in the root
    TArray<VStr> pk3s;
    dpak->ListPk3Files(pk3s);
    for (int i = 0; i < pk3s.length(); ++i) {
      VStream *pk3st = dpak->OpenFileRead(pk3s[i]);
      W_AddAuxiliaryStream(pk3st, WAuxFileType::Pk3);
    }
  } else {
    VStream *strm = FL_OpenSysFileRead(pwf.fname);
    if (!strm) {
      //GCon->Logf(NAME_Init, "TEMPMOUNT: OOPS0: %s", *pwf.fname);
      return;
    }
    if (strm->TotalSize() < 16) {
      delete strm;
      //GCon->Logf(NAME_Init, "TEMPMOUNT: OOPS1: %s", *pwf.fname);
      return;
    }
    char sign[4];
    strm->Serialise(sign, 4);
    strm->Seek(0);
    if (memcmp(sign, "PWAD", 4) == 0 || memcmp(sign, "IWAD", 4) == 0) {
      //GCon->Logf(NAME_Init, "TEMPMOUNT: WAD: %s", *pwf.fname);
      W_AddAuxiliaryStream(strm, WAuxFileType::Wad);
    } else {
      VStr ext = pwf.fname.ExtractFileExtension().ToLower();
      if (ext == "pk3") {
        //GCon->Logf(NAME_Init, "TEMPMOUNT: PK3: %s", *pwf.fname);
        W_AddAuxiliaryStream(strm, WAuxFileType::Pk3);
      } else if (ext == "zip") {
        //GCon->Logf(NAME_Init, "TEMPMOUNT: ZIP: %s", *pwf.fname);
        W_AddAuxiliaryStream(strm, WAuxFileType::Zip);
      }
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
static TArray<VStr> wpklist; // everything is lowercased

// asystem
static void wpkAppend (const VStr &fname, bool asystem) {
  if (fname.length() == 0) return;
  VStr fn = fname.toLowerCase();
  if (!asystem) {
    fn = fn.extractFileName();
    if (fn.length() == 0) fn = fname.toLowerCase();
  }
  // check for duplicates
  for (int f = wpklist.length()-1; f >= 0; --f) {
    if (wpklist[f] != fn) continue;
    // i found her!
    return;
  }
  /*
  WadPakInfo &wi = wpklist.Alloc();
  wi.path = fn;
  wi.isSystem = asystem;
  */
  wpklist.append(fn);
}


VCvarS game_name("game_name", "unknown", "The Name Of The Game.", CVAR_Rom);


// ////////////////////////////////////////////////////////////////////////// //
__attribute__((unused)) static int cmpfuncCI (const void *v1, const void *v2) {
  return ((VStr*)v1)->ICmp((*(VStr*)v2));
}

static int cmpfuncCINoExt (const void *v1, const void *v2) {
  return ((VStr *)v1)->StripExtension().ICmp(((VStr *)v2)->StripExtension());
}


//==========================================================================
//
//  AddZipFile
//
//==========================================================================
const TArray<VStr> &GetWadPk3List () {
  /*
  TArray<VStr> res;
  for (int f = 0; f < wpklist.length(); ++f) {
    VStr fname = wpklist[f].path;
    //if (fname == "/opt/vavoom/share/vavoom/doomu.wad") fname = "/usr/local/share/vavoom/doomu.wad";
    if (!fullpath && !wpklist[f].isSystem) fname = fname.extractFileName();
    res.append(fname);
  }
  // and sort it
  //qsort(res.Ptr(), res.length(), sizeof(VStr), cmpfuncCI);
  return res;
  */
  return wpklist;
}


//==========================================================================
//
//  AddZipFile
//
//==========================================================================
static void AddZipFile (const VStr &ZipName, VZipFile *Zip, bool allowpk3) {
  SearchPaths.Append(Zip);

  // add all WAD files in the root of the ZIP file
  TArray<VStr> Wads;
  Zip->ListWadFiles(Wads);
  for (int i = 0; i < Wads.length(); ++i) {
#ifdef VAVOOM_USE_GWA
    VStr GwaName = Wads[i].StripExtension()+".gwa";
#endif
    VStream *WadStrm = Zip->OpenFileRead(Wads[i]);
#ifdef VAVOOM_USE_GWA
    VStream *GwaStrm = Zip->OpenFileRead(GwaName);
#endif

    if (!WadStrm) continue;
    if (WadStrm->TotalSize() < 16) { delete WadStrm; continue; }
    char sign[4];
    WadStrm->Serialise(sign, 4);
    if (memcmp(sign, "PWAD", 4) != 0 && memcmp(sign, "IWAD", 4) != 0) { delete WadStrm; continue; }
    WadStrm->Seek(0);
    // decompress WAD and GWA files into a memory stream since reading from ZIP will be very slow
    VStream *MemStrm = new VMemoryStream(ZipName+":"+Wads[i], WadStrm);
    delete WadStrm;

#ifdef VAVOOM_USE_GWA
    if (GwaStrm) {
      Len = GwaStrm->TotalSize();
      //Buf = new vuint8[Len];
      Buf = (vuint8 *)Z_Calloc(Len);
      GwaStrm->Serialise(Buf, Len);
      delete GwaStrm;
      GwaStrm = nullptr;
      GwaStrm = new VMemoryStream(ZipName+":"+GwaName, Buf, Len, true);
      //delete[] Buf;
      Buf = nullptr;
    }

    W_AddFileFromZip(ZipName+":"+Wads[i], WadStrm, ZipName+":"+GwaName, GwaStrm);
#else
    W_AddFileFromZip(ZipName+":"+Wads[i], MemStrm);
#endif
  }

  if (!allowpk3) return;

  // add all pk3 files in the root of the ZIP file
  TArray<VStr> pk3s;
  Zip->ListPk3Files(pk3s);
  for (int i = 0; i < pk3s.length(); ++i) {
    VStream *ZipStrm = Zip->OpenFileRead(pk3s[i]);
    if (ZipStrm->TotalSize() < 16) { delete ZipStrm; continue; }
    // decompress file into a memory stream since reading from ZIP will be very slow
    VStream *MemStrm = new VMemoryStream(ZipName+":"+pk3s[i], ZipStrm);
    delete ZipStrm;
    if (fsys_report_added_paks) GCon->Logf(NAME_Init, "Adding nested pk3 '%s:%s'...", *ZipName, *pk3s[i]);
    VZipFile *pk3 = new VZipFile(MemStrm, ZipName+":"+pk3s[i]);
    AddZipFile(ZipName+":"+pk3s[i], pk3, false);
  }
}


//==========================================================================
//
//  AddZipFile
//
//==========================================================================
static void AddZipFile (const VStr &ZipName) {
  VZipFile *Zip = new VZipFile(ZipName);
  AddZipFile(ZipName, Zip, true);
}


//==========================================================================
//
//  AddAnyFile
//
//==========================================================================
static void AddAnyFile (const VStr &fname, bool allowFail, bool fixVoices=false) {
  if (fname.length() == 0) {
    if (!allowFail) Sys_Error("cannot add empty file");
    return;
  }
  VStr ext = fname.ExtractFileExtension().ToLower();
  if (!Sys_FileExists(fname)) {
    if (!allowFail) Sys_Error("cannot add file \"%s\"", *fname);
    GCon->Logf(NAME_Warning,"cannot add file \"%s\"", *fname);
    return;
  }
  if (ext == "pk3" || ext == "zip") {
    AddZipFile(fname);
  } else {
    if (allowFail) {
      W_AddFile(fname, false);
    } else {
      W_AddFile(fname, fixVoices, fl_savedir);
    }
  }
}


//==========================================================================
//
//  AddPakDir
//
//==========================================================================
static void AddPakDir (const VStr &dirname) {
  if (dirname.length() == 0) return;
  VDirPakFile *dpak = new VDirPakFile(dirname);
  //if (!dpak->hasFiles()) { delete dpak; return; }

  SearchPaths.append(dpak);

  // add all WAD files in the root
  TArray<VStr> wads;
  dpak->ListWadFiles(wads);
  for (int i = 0; i < wads.length(); ++i) {
    VStream *wadst = dpak->OpenFileRead(wads[i]);
    if (!wadst) continue;
    W_AddFileFromZip(dpak->GetPrefix()+":"+wads[i], wadst, VStr(), nullptr);
  }

  // add all pk3 files in the root
  TArray<VStr> pk3s;
  dpak->ListPk3Files(pk3s);
  for (int i = 0; i < pk3s.length(); ++i) {
    VStream *pk3st = dpak->OpenFileRead(pk3s[i]);
    if (fsys_report_added_paks) GCon->Logf(NAME_Init, "Adding nested pk3 '%s:%s'...", *dpak->GetPrefix(), *pk3s[i]);
    VZipFile *pk3 = new VZipFile(pk3st, dpak->GetPrefix()+":"+pk3s[i]);
    AddZipFile(dpak->GetPrefix()+":"+pk3s[i], pk3, false);
  }
}


//==========================================================================
//
//  AddAutoloadRC
//
//==========================================================================
void AddAutoloadRC (const VStr &aubasedir) {
  VStream *aurc = FL_OpenSysFileRead(aubasedir+"autoload.rc");
  if (!aurc) return;

  // collect autoload groups to skip
  TArray<VStr> skipGrp;
  bool inSkipArg = false;
  for (int asp = 1; asp < GArgs.Count(); ++asp) {
    if (VStr::Cmp(GArgs[asp], "-skip-auto") == 0 || VStr::Cmp(GArgs[asp], "-skip-autoload") == 0) {
      inSkipArg = true;
    } else if (inSkipArg) {
      VStr sg = GArgs[asp];
      if (!sg.isEmpty() && (*sg)[0] != '-' && (*sg)[0] != '+') {
        skipGrp.append(sg);
      }
      inSkipArg = false;
    }
  }

  VScriptParser *sc = new VScriptParser(aurc->GetName(), aurc);

  while (!sc->AtEnd()) {
    sc->Expect("group");
    sc->ExpectString();
    VStr grpname = sc->String;
    sc->Expect("{");
    bool doSkip = false;
    for (int f = 0; f < skipGrp.length(); ++f) if (skipGrp[f].ICmp(grpname) == 0) { doSkip = true; break; }
    if (doSkip) {
      GCon->Logf(NAME_Init, "skipping autoload group '%s'", *grpname);
      sc->SkipBracketed(true); // bracket eaten
      continue;
    }
    GCon->Logf(NAME_Init, "processing autoload group '%s'", *grpname);
    // get file list
    while (!sc->Check("}")) {
      if (sc->Check(",")) continue;
      if (!sc->GetString()) sc->Error("wad/pk3 path expected");
      if (sc->String.isEmpty()) continue;
      VStr fname = ((*sc->String)[0] == '/' ? sc->String : aubasedir+sc->String);
      if (Sys_FileExists(fname)) {
        VStr ext = fname.ExtractFileExtension().ToLower();
             if (ext == "wad") W_AddFile(fname, false);
        else if (ext == "pk3") AddZipFile(fname);
        else GCon->Logf(NAME_Warning, "ignored unrecognized autoload file '%s'", *fname);
      }
    }
  }

  delete sc;
}


//==========================================================================
//
//  AddGameDir
//
//==========================================================================
static void AddGameDir (const VStr &basedir, const VStr &dir) {
  guard(AddGameDir);

  VStr bdx = basedir;
  if (bdx.length() == 0) bdx = "./";
  bdx = bdx+"/"+dir;
  //fprintf(stderr, "bdx:<%s>\n", *bdx);

  if (!Sys_DirExists(bdx)) return;

  TArray<VStr> WadFiles;
  TArray<VStr> ZipFiles;

  // find all .wad/.pk3 files in that directory
  auto dirit = Sys_OpenDir(bdx);
  if (dirit) {
    for (VStr test = Sys_ReadDir(dirit); test.IsNotEmpty(); test = Sys_ReadDir(dirit)) {
      //fprintf(stderr, "  <%s>\n", *test);
      if (test[0] == '_' || test[0] == '.') continue; // skip it
      VStr ext = test.ExtractFileExtension().ToLower();
           if (ext == "wad") WadFiles.Append(test);
      else if (ext == "pk3") ZipFiles.Append(test);
    }
    Sys_CloseDir(dirit);
    qsort(WadFiles.Ptr(), WadFiles.length(), sizeof(VStr), cmpfuncCINoExt);
    qsort(ZipFiles.Ptr(), ZipFiles.length(), sizeof(VStr), cmpfuncCINoExt);
  }

  // add system dir, if it has any files
  if (ZipFiles.length() || WadFiles.length()) wpkAppend(dir+"/", true); // don't strip path

  // now add wads, then pk3s
  for (int i = 0; i < WadFiles.length(); ++i) {
    //if (i == 0 && ZipFiles.length() == 0) wpkAppend(dir+"/"+WadFiles[i], true); // system pak
    W_AddFile(bdx+"/"+WadFiles[i], false);
  }

  for (int i = 0; i < ZipFiles.length(); ++i) {
    //if (i == 0) wpkAppend(dir+"/"+ZipFiles[i], true); // system pak
    AddZipFile(bdx+"/"+ZipFiles[i]);
  }

  // add "autoload/*"
  if (!fsys_onlyOneBaseFile) {
    WadFiles.clear();
    ZipFiles.clear();
    if (bdx[bdx.length()-1] != '/') bdx += "/";
    bdx += "autoload";
    VStr bdxSlash = bdx+"/";
    // find all .wad/.pk3 files in that directory
    dirit = Sys_OpenDir(bdx);
    if (dirit) {
      for (VStr test = Sys_ReadDir(dirit); test.IsNotEmpty(); test = Sys_ReadDir(dirit)) {
        //fprintf(stderr, "  <%s>\n", *test);
        if (test[0] == '_' || test[0] == '.') continue; // skip it
        VStr ext = test.ExtractFileExtension().ToLower();
             if (ext == "wad") WadFiles.Append(test);
        else if (ext == "pk3") ZipFiles.Append(test);
      }
      Sys_CloseDir(dirit);
      qsort(WadFiles.Ptr(), WadFiles.length(), sizeof(VStr), cmpfuncCINoExt);
      qsort(ZipFiles.Ptr(), ZipFiles.length(), sizeof(VStr), cmpfuncCINoExt);
    }

    // now add wads, then pk3s
    for (int i = 0; i < WadFiles.length(); ++i) W_AddFile(bdxSlash+WadFiles[i], false);
    for (int i = 0; i < ZipFiles.length(); ++i) AddZipFile(bdxSlash+ZipFiles[i]);

    AddAutoloadRC(bdxSlash);
  }

  // finally add directory itself
  // k8: nope
  /*
  VFilesDir *info = new VFilesDir(bdx);
  SearchPaths.Append(info);
  */
  unguard;
}


//==========================================================================
//
//  AddGameDir
//
//==========================================================================
static void AddGameDir (const VStr &dir) {
  guard(AddGameDir);
  AddGameDir(fl_basedir, dir);
  if (fl_savedir.IsNotEmpty()) AddGameDir(fl_savedir, dir);
  fl_gamedir = dir;
  unguard;
}


//==========================================================================
//
//  FindMainWad
//
//==========================================================================
static VStr FindMainWad (const VStr &MainWad) {
  if (MainWad.length() == 0) return VStr();

  // if we have path separators, try relative path first
  bool hasSep = false;
  for (const char *s = *MainWad; *s; ++s) {
#ifdef _WIN32
    if (*s == '/' || *s == '\\') { hasSep = true; break; }
#else
    if (*s == '/') { hasSep = true; break; }
#endif
  }
#ifdef _WIN32
  if (!hasSep && MainWad.length() >= 2 && MainWad[1] == ':') hasSep = true;
#endif

  if (hasSep) {
    if (Sys_FileExists(MainWad)) return MainWad;
  }

  // first check in IWAD directories
  for (int i = 0; i < IWadDirs.length(); ++i) {
    if (Sys_FileExists(IWadDirs[i]+"/"+MainWad)) return IWadDirs[i]+"/"+MainWad;
  }

  // then look in the save directory
  //if (fl_savedir.IsNotEmpty() && Sys_FileExists(fl_savedir+"/"+MainWad)) return fl_savedir+"/"+MainWad;

  // finally in base directory
  if (Sys_FileExists(fl_basedir+"/"+MainWad)) return fl_basedir+"/"+MainWad;

  // just in case, check it as-is
  if (Sys_FileExists(MainWad)) return MainWad;

  return VStr();
}


//==========================================================================
//
//  SetupGameDir
//
//==========================================================================
static void SetupGameDir (const VStr &dirname) {
  guard(SetupGameDir);
  AddGameDir(dirname);
  unguard;
}


//==========================================================================
//
//  ParseBase
//
//==========================================================================
static void ParseBase (const VStr &name, const VStr &mainiwad) {
  guard(ParseBase);
  TArray<version_t> games;
  int selectedGame = -1;
  VStr UseName;

       if (fl_savedir.IsNotEmpty() && Sys_FileExists(fl_savedir+"/"+name)) UseName = fl_savedir+"/"+name;
  else if (Sys_FileExists(fl_basedir+"/"+name)) UseName = fl_basedir+"/"+name;
  else return;

  if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, "Parsing game definition file \"%s\"...", *UseName);
  VScriptParser *sc = new VScriptParser(UseName, FL_OpenSysFileRead(UseName));
  while (!sc->AtEnd()) {
    version_t &dst = games.Alloc();
    dst.ParmFound = 0;
    dst.FixVoices = false;
    sc->Expect("game");
    sc->ExpectString();
    dst.GameDir = sc->String;
    if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, " game dir: \"%s\"", *dst.GameDir);
    for (;;) {
      if (sc->Check("iwad")) {
        sc->ExpectString();
        if (sc->String.isEmpty()) continue;
        if (dst.MainWads.length() == 0) {
          dst.MainWads.Append(sc->String);
          if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, "  iwad: \"%s\"", *sc->String);
        } else {
          sc->Error(va("duplicate iwad (%s) for game \"%s\"!", *sc->String, *dst.GameDir));
        }
        continue;
      }
      if (sc->Check("altiwad")) {
        sc->ExpectString();
        if (sc->String.isEmpty()) continue;
        if (dst.MainWads.length() == 0) {
          sc->Error(va("no iwad for game \"%s\"!", *dst.GameDir));
        }
        dst.MainWads.Append(sc->String);
        if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, "  alternate iwad: \"%s\"", *sc->String);
        continue;
      }
      if (sc->Check("addfile")) {
        bool optional = sc->Check("optional");
        sc->ExpectString();
        if (sc->String.isEmpty()) continue;
        AuxFile &aux = dst.AddFiles.alloc();
        aux.name = sc->String;
        aux.optional = optional;
        if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, "  aux file: \"%s\"", *sc->String);
        continue;
      }
      if (sc->Check("base")) {
        sc->ExpectString();
        if (sc->String.isEmpty()) continue;
        dst.BaseDirs.Append(sc->String);
        if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, "  base: \"%s\"", *sc->String);
        continue;
      }
      if (sc->Check("param")) {
        sc->ExpectString();
        if (sc->String.length() < 2 || sc->String[0] != '-') sc->Error(va("invalid game (%s) param!", *dst.GameDir));
        dst.param = (*sc->String)+1;
        if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, "  param: \"%s\"", (*sc->String)+1);
        dst.ParmFound = GArgs.CheckParm(*sc->String);
        continue;
      }
      if (sc->Check("fixvoices")) {
        dst.FixVoices = true;
        if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, "  fix voices: tan");
        continue;
      }
      if (sc->Check("warp")) {
        sc->ExpectString();
        dst.warp = VStr(sc->String);
        continue;
      }
      break;
    }
    sc->Expect("end");
  }
  delete sc;
  if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, "Done parsing game definition file \"%s\"...", *UseName);

  if (games.length() == 0) Sys_Error("No game definitions found!");

  int bestPIdx = -1;
  for (int gi = 0; gi < games.length(); ++gi) {
    version_t &G = games[gi];
    if (!G.ParmFound) continue;
    if (G.ParmFound > bestPIdx) {
      bestPIdx = G.ParmFound;
      selectedGame = gi;
    }
  }

  if (selectedGame >= 0) {
    game_name = *games[selectedGame].param;
    if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, "SELECTED GAME: \"%s\"", *games[selectedGame].param);
  } else {
    if (games.length() != 1) {
      // try to select DooM or DooM II automatically
      if (true) {
        bool oldReport = fsys_no_dup_reports;
        fsys_no_dup_reports = true;
        W_CloseAuxiliary();
        for (int pwidx = 0; pwidx < pwadList.length(); ++pwidx) tempMount(pwadList[pwidx]);
        VStr mname = W_FindMapInAuxuliaries(nullptr);
        W_CloseAuxiliary();
        fsys_no_dup_reports = oldReport;
        if (!mname.isEmpty()) {
          // found map, find DooM or DooM II game definition
          VStr gamename = (mname[0] == 'e' ? "doom" : "doom2");
          for (int gi = 0; gi < games.length(); ++gi) {
            version_t &G = games[gi];
            if (G.param.Cmp(gamename) == 0) {
              selectedGame = gi;
              break;
            }
          }
        }
      }
      if (selectedGame < 0) {
        // try to detect game
        if (mainiwad.length() > 0) {
          for (int gi = 0; gi < games.length(); ++gi) {
            version_t &gmi = games[gi];
            bool okwad = false;
            VStr mw = mainiwad.extractFileBaseName();
            for (int f = 0; f < gmi.MainWads.length(); ++f) {
              VStr gw = gmi.MainWads[f].extractFileBaseName();
              if (mw.ICmp(gw) == 0) { okwad = true; break; }
            }
            if (okwad) { selectedGame = gi; break; }
          }
        }
      }
      if (selectedGame < 0) {
        for (int gi = 0; gi < games.length(); ++gi) {
          version_t &gmi = games[gi];
          VStr mainWadPath;
          for (int f = 0; f < gmi.MainWads.length(); ++f) {
            mainWadPath = FindMainWad(gmi.MainWads[f]);
            if (!mainWadPath.isEmpty()) { selectedGame = gi; break; }
          }
          if (selectedGame >= 0) break;
        }
      }
    } else {
      selectedGame = 0;
    }
    if (selectedGame < 0) Sys_Error("Looks like I cannot find any IWADs. Did you forgot to specify -iwaddir?");
  }

  version_t &gmi = games[selectedGame];

  // look for the main wad file
  VStr mainWadPath;

  // try user-specified iwad
  if (mainiwad.length() > 0) mainWadPath = FindMainWad(mainiwad);

  if (mainWadPath.length() == 0) {
    for (int f = 0; f < gmi.MainWads.length(); ++f) {
      mainWadPath = FindMainWad(gmi.MainWads[f]);
      if (!mainWadPath.isEmpty()) break;
    }
  }

  if (mainWadPath.isEmpty()) Sys_Error("Main wad file \"%s\" not found.", (gmi.MainWads.length() ? *gmi.MainWads[0] : "<none>"));

  warpTpl = gmi.warp;

  IWadIndex = SearchPaths.length();
  //GCon->Logf("MAIN WAD(1): '%s'", *MainWadPath);
  wpkAppend(mainWadPath, false); // mark iwad as "non-system" file, so path won't be stored in savegame
  AddAnyFile(mainWadPath, false, gmi.FixVoices);

  for (int j = 0; j < gmi.AddFiles.length(); j++) {
    VStr FName = FindMainWad(gmi.AddFiles[j].name);
    if (FName.IsEmpty()) {
      if (gmi.AddFiles[j].optional) continue;
      Sys_Error("Required file \"%s\" not found", *gmi.AddFiles[j].name);
    }
    wpkAppend(FName, false); // mark additional files as "non-system", so path won't be stored in savegame
    AddAnyFile(FName, false);
  }

  for (int f = 0; f < gmi.BaseDirs.length(); ++f) AddGameDir(gmi.BaseDirs[f]);

  SetupGameDir(gmi.GameDir);
  unguard;
}


//==========================================================================
//
//  RenameSprites
//
//==========================================================================
static void RenameSprites () {
  guard(RenameSprites);
  VStream *Strm = FL_OpenFileRead("sprite_rename.txt");
  if (!Strm) return;

  VScriptParser *sc = new VScriptParser("sprite_rename.txt", Strm);
  TArray<VSpriteRename> Renames;
  TArray<VSpriteRename> AlwaysRenames;
  TArray<VLumpRename> LumpRenames;
  TArray<VLumpRename> AlwaysLumpRenames;
  while (!sc->AtEnd()) {
    bool Always = sc->Check("always");

    if (sc->Check("lump")) {
      sc->ExpectString();
      VStr Old = sc->String.ToLower();
      sc->ExpectString();
      VStr New = sc->String.ToLower();
      VLumpRename &R = (Always ? AlwaysLumpRenames.Alloc() : LumpRenames.Alloc());
      R.Old = *Old;
      R.New = *New;
      continue;
    }

    sc->ExpectString();
    if (sc->String.Length() != 4) sc->Error("Sprite name must be 4 chars long");
    VStr Old = sc->String.ToLower();

    sc->ExpectString();
    if (sc->String.Length() != 4) sc->Error("Sprite name must be 4 chars long");
    VStr New = sc->String.ToLower();

    VSpriteRename &R = Always ? AlwaysRenames.Alloc() : Renames.Alloc();
    R.Old[0] = Old[0];
    R.Old[1] = Old[1];
    R.Old[2] = Old[2];
    R.Old[3] = Old[3];
    R.New[0] = New[0];
    R.New[1] = New[1];
    R.New[2] = New[2];
    R.New[3] = New[3];
  }
  delete sc;

  bool RenameAll = !!GArgs.CheckParm("-oldsprites");
  for (int i = 0; i < SearchPaths.length(); ++i) {
    if (RenameAll || i == IWadIndex) SearchPaths[i]->RenameSprites(Renames, LumpRenames);
    SearchPaths[i]->RenameSprites(AlwaysRenames, AlwaysLumpRenames);
  }
  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //
extern VCvarB respawnparm;
extern VCvarB fastparm;
extern VCvarB NoMonsters;
extern VCvarI Skill;


//==========================================================================
//
//  countFmtHash
//
//==========================================================================
static int countFmtHash (const VStr &str) {
  if (str.length() == 0) return 0;
  int count = 0;
  bool inHash = false;
  for (const char *s = *str; *s; ++s) {
    if (*s == '#') {
      if (!inHash) ++count;
      inHash = true;
    } else {
      inHash = false;
    }
  }
  return count;
}


//==========================================================================
//
//  FL_InitOptions
//
//==========================================================================
void FL_InitOptions () {
  GArgs.AddFileOption("!1-game"); // '!' means "has args, and breaking" (number is argc)
  GArgs.AddFileOption("!1-logfile"); // don't register log file in saves
  GArgs.AddFileOption("!1-skip-autoload");
  GArgs.AddFileOption("!1-skip-auto");
  GArgs.AddFileOption("-skipsounds");
  GArgs.AddFileOption("-allowsounds");
  GArgs.AddFileOption("-skipsprites");
  GArgs.AddFileOption("-allowsprites");
  GArgs.AddFileOption("-skipdehacked");
  GArgs.AddFileOption("-allowdehacked");
  GArgs.AddFileOption("-cosmetic");
}


//==========================================================================
//
//  FL_Init
//
//==========================================================================
void FL_Init () {
  guard(FL_Init);
  const char *p;
  VStr mainIWad = VStr();
  int wmap1 = -1, wmap2 = -1; // warp

  doStartMap = (GArgs.CheckParm("-k8runmap") != 0);

  //GCon->Logf(NAME_Init, "=== INITIALIZING VaVoom ===");

  if (GArgs.CheckParm("-fast") != 0) fastparm = 1;
  if (GArgs.CheckParm("-respawn") != 0) respawnparm = true;
  if (GArgs.CheckParm("-nomonsters") != 0) NoMonsters = true;

  if (GArgs.CheckParm("-nomenudef") != 0) gz_skip_menudef = true;

  if (GArgs.CheckParm("-ignore-zscript") != 0) fsys_ignoreZScript = true;

  bool isChex = false;

  if (GArgs.CheckParm("-chex") != 0) {
    //game_override_mode = GAME_Chex;
    fsys_onlyOneBaseFile = true; // disable autoloads
    isChex = true;
  }

  {
    auto v = GArgs.CheckValue("-skill");
    if (v) {
      int skn = -1;
      if (!VStr::convertInt(v, &skn)) skn = 3;
      if (skn < 1) skn = 1; else if (skn > 5) skn = 5;
      Skill = skn-1;
    }
  }

  {
    auto v = GArgs.CheckValue("-iwad");
    if (v) mainIWad = VStr(v);
  }

  fsys_warp_n0 = -1;
  fsys_warp_n1 = -1;
  fsys_warp_cmd = VStr();

  {
    int wp = GArgs.CheckParm("-warp");
    if (wp && wp+1 < GArgs.Count()) {
      bool mapok = VStr::convertInt(GArgs[wp+1], &wmap1);
      bool epiok = VStr::convertInt(GArgs[wp+2], &wmap2);
      if (!mapok) wmap1 = -1;
      if (!epiok) wmap2 = -1;
    }
    if (wmap1 < 0) wmap1 = -1;
    if (wmap2 < 0) wmap2 = -1;
    fsys_warp_n0 = wmap1;
    fsys_warp_n1 = wmap2;
  }


  bool reportIWads = (GArgs.CheckParm("-reportiwad") || GArgs.CheckParm("-reportiwads"));
  bool reportPWads = (GArgs.CheckParm("-silentpwad") || GArgs.CheckParm("-silencepwad") || GArgs.CheckParm("-silentpwads") || GArgs.CheckParm("-silencepwads") ? false : true);

  fsys_report_added_paks = reportIWads;

  if (!isChex) {
    fsys_onlyOneBaseFile = GArgs.CheckParm("-nakedbase");
  }

  // set up base directory (main data files)
  fl_basedir = ".";
  p = GArgs.CheckValue("-basedir");
  if (p) {
    fl_basedir = p;
    if (fl_basedir.isEmpty()) fl_basedir = ".";
  } else {
    static const char *defaultBaseDirs[] = {
#ifdef __SWITCH__
      "/switch/vavoom",
      ".",
#elif !defined(_WIN32)
      "/opt/vavoom/share/vavoom",
      "/usr/local/share/vavoom",
      "/usr/share/vavoom",
      "!/../share/vavoom",
      ".",
#else
      "!/share",
      "!/.",
      ".",
#endif
      nullptr,
    };
    for (const char **tbd = defaultBaseDirs; *tbd; ++tbd) {
      VStr dir = VStr(*tbd);
      if (dir[0] == '!') {
        dir.chopLeft(1);
        dir = GArgs[0]+dir;
      } else if (dir[0] == '~') {
        dir.chopLeft(1);
        const char *hdir = getenv("HOME");
        if (hdir && hdir[0]) {
          dir = VStr(hdir)+dir;
        } else {
          continue;
        }
      }
      if (Sys_DirExists(dir)) {
        fl_basedir = dir;
        break;
      }
    }
    if (fl_basedir.isEmpty()) Sys_Error("cannot find basedir; use \"-basedir dir\" to set it");
  }

  // set up save directory (files written by engine)
  p = GArgs.CheckValue("-savedir");
  if (p && p[0]) {
    fl_savedir = p;
  } else {
#if !defined(_WIN32)
    const char *HomeDir = getenv("HOME");
    if (HomeDir && HomeDir[0]) fl_savedir = VStr(HomeDir) + "/.vavoom";
#else
    fl_savedir = ".";
#endif
  }

  // set up additional directories where to look for IWAD files
  int iwp = GArgs.CheckParm("-iwaddir");
  if (iwp) {
    while (++iwp != GArgs.Count() && GArgs[iwp][0] != '-' && GArgs[iwp][0] != '+') {
      IWadDirs.Append(GArgs[iwp]);
    }
  } else {
    static const char *defaultIwadDirs[] = {
      ".",
      "!/.",
#ifdef __SWITCH__
      "/switch/vavoom/iwads",
      "/switch/vavoom",
#elif !defined(_WIN32)
      "~/.vavoom/iwads",
      "~/.vavoom/iwad",
      "~/.vavoom",
      "/opt/vavoom/share/vavoom",
      "/usr/local/share/vavoom",
      "/usr/share/vavoom",
      "!/../share/vavoom",
#else
      "~",
      "!/iwads",
      "!/iwad",
#endif
      nullptr,
    };
    for (const char **tbd = defaultIwadDirs; *tbd; ++tbd) {
      VStr dir = VStr(*tbd);
      if (dir[0] == '!') {
        dir.chopLeft(1);
        dir = GArgs[0]+dir;
      } else if (dir[0] == '~') {
        dir.chopLeft(1);
        const char *hdir = getenv("HOME");
        if (hdir && hdir[0]) {
          dir = VStr(hdir)+dir;
        } else {
          continue;
        }
      }
      if (Sys_DirExists(dir)) IWadDirs.Append(dir);
    }
  }
  // envvar
  {
    const char *dwd = getenv("DOOMWADDIR");
    if (dwd && dwd[0]) IWadDirs.Append(dwd);
  }
#ifdef _WIN32
  // home dir (if any)
  /*
  {
    const char *hd = getenv("HOME");
    if (hd && hd[0]) IWadDirs.Append(hd);
  }
  */
#endif
  // and current dir
  //IWadDirs.Append(".");

  AddGameDir("basev/common");

  collectPWads();

  ParseBase("basev/games.txt", mainIWad);
#ifdef DEVELOPER
  // i need progs to be loaded from files
  //fl_devmode = true;
#endif

  // process "warp", do it here, so "+var" will be processed after "map nnn"
  // postpone, and use `P_TranslateMapEx()` in host initialization
  if (warpTpl.length() > 0 && wmap1 >= 0) {
    int fmtc = countFmtHash(warpTpl);
    if (fmtc >= 1 && fmtc <= 2) {
      if (fmtc == 2 && wmap2 == -1) { wmap2 = wmap1; wmap1 = 1; } // "-warp n" is "-warp 1 n" for ExMx
      VStr cmd = "map ";
      int spos = 0;
      int numidx = 0;
      while (spos < warpTpl.length()) {
        if (warpTpl[spos] == '#') {
          int len = 0;
          while (spos < warpTpl.length() && warpTpl[spos] == '#') { ++len; ++spos; }
          char tbuf[64];
          snprintf(tbuf, sizeof(tbuf), "%d", (numidx == 0 ? wmap1 : wmap2));
          VStr n = VStr(tbuf);
          while (n.length() < len) n = VStr("0")+n;
          cmd += n;
          ++numidx;
        } else {
          cmd += warpTpl[spos++];
        }
      }
      cmd += "\n";
      //GCmdBuf.Insert(cmd);
      fsys_warp_cmd = cmd;
    }
  }

  if (game_release_mode || isChex) {
    if (GArgs.CheckParm("-gore") != 0) AddGameDir("basev/mods/gore");
  } else {
    if (GArgs.CheckParm("-nogore") == 0) AddGameDir("basev/mods/gore");
  }

  if (isChex) AddGameDir("basev/mods/chex");

  int mapnum = -1;
  VStr mapname;
  bool mapinfoFound = false;

  // mount pwads
  fsys_report_added_paks = reportPWads;
  for (int pwidx = 0; pwidx < pwadList.length(); ++pwidx) {
    PWadFile &pwf = pwadList[pwidx];
    fsys_skipSounds = pwf.skipSounds;
    fsys_skipSprites = pwf.skipSprites;
    int nextfid = W_NextMountFileId();

    if (pwf.asDirectory) {
      if (pwf.storeInSave) wpkAppend(pwf.fname, false); // non-system pak
      GCon->Logf(NAME_Init, "Mounting directory '%s' as emulated PK3 file.", *pwf.fname);
      AddPakDir(pwf.fname);
    } else {
      if (pwf.storeInSave) wpkAppend(pwf.fname, false); // non-system pak
      AddAnyFile(pwf.fname, true);
    }
    fsys_hasPwads = true;

    //GCon->Log("**************************");
    if (doStartMap && !mapinfoFound) {
      //GCon->Logf("::: %d : %d", nextfid, W_NextMountFileId());
      for (; nextfid < W_NextMountFileId(); ++nextfid) {
        if (W_CheckNumForNameInFile(NAME_mapinfo, nextfid) >= 0) {
          GCon->Logf(NAME_Init, "FOUND 'mapinfo'!");
          mapinfoFound = true;
          fsys_hasMapPwads = true;
          break;
        }
        int midx = -1;
        VStr mname = W_FindMapInLastFile(nextfid, &midx);
        if (mname.length() && (mapnum < 0 || midx < mapnum)) {
          mapnum = midx;
          mapname = mname;
          fsys_hasMapPwads = true;
        }
      }
    } else if (!fsys_hasMapPwads) {
      for (; nextfid < W_NextMountFileId(); ++nextfid) {
        if (W_CheckNumForNameInFile(NAME_mapinfo, nextfid) >= 0) {
          fsys_hasMapPwads = true;
          break;
        }
        int midx = -1;
        VStr mname = W_FindMapInLastFile(nextfid, &midx);
        if (mname.length() && (mapnum < 0 || midx < mapnum)) {
          fsys_hasMapPwads = true;
          break;
        }
      }
    }
  }
  fsys_skipSounds = false;
  fsys_skipSprites = false;

  fsys_report_added_paks = reportIWads;
  if (GArgs.CheckParm("-bdw") != 0) AddGameDir("basev/mods/bdw");
  fsys_report_added_paks = reportPWads;

  RenameSprites();

  if (doStartMap && fsys_warp_cmd.isEmpty() && !mapinfoFound && mapnum > 0 && mapname.length()) {
    GCon->Logf(NAME_Init, "FOUND MAP: %s", *mapname);
    mapname = va("map \"%s\"\n", *mapname.quote());
    //GCmdBuf.Insert(mapname);
    fsys_warp_cmd = mapname;
  } else if (doStartMap && fsys_warp_cmd.isEmpty() && mapinfoFound) {
    fsys_warp_cmd = "__k8_run_first_map";
  }

  unguard;
}


//==========================================================================
//
//  FL_Shutdown
//
//==========================================================================
void FL_Shutdown () {
  guard(FL_Shutdown);
  for (int i = 0; i < SearchPaths.length(); ++i) {
    delete SearchPaths[i];
    SearchPaths[i] = nullptr;
  }
  SearchPaths.Clear();
  fl_basedir.Clean();
  fl_savedir.Clean();
  fl_gamedir.Clean();
  wadfiles.Clear();
  IWadDirs.Clear();
  unguard;
}


//==========================================================================
//
//  FL_FileExists
//
//==========================================================================
bool FL_FileExists (const VStr &fname) {
  guard(FL_FileExists);
  for (int i = SearchPaths.length()-1; i >= 0; --i) {
    if (SearchPaths[i]->FileExists(fname)) return true;
  }
  return false;
  unguard;
}


//==========================================================================
//
//  FL_CreatePath
//
//==========================================================================
void FL_CreatePath (const VStr &Path) {
  guard(FL_CreatePath);
  TArray<VStr> spp;
  Path.SplitPath(spp);
  if (spp.length() == 0 || (spp.length() == 1 && spp[0] == "/")) return;
  if (spp.length() > 0) {
    VStr newpath;
    for (int pos = 0; pos < spp.length(); ++pos) {
      if (newpath.length() && newpath[newpath.length()-1] != '/') newpath += "/";
      newpath += spp[pos];
      if (!Sys_DirExists(newpath)) Sys_CreateDirectory(newpath);
    }
  }
  /*
  VStr Temp = Path;
  for (size_t i = 3; i <= Temp.Length(); i++)
  {
    if (Temp[i] == '/' || Temp[i] == '\\' || Temp[i] == 0)
    {
      char Save = Temp[i];
      Temp[i] = 0;
      if (!Sys_DirExists(Temp))
        Sys_CreateDirectory(Temp);
      Temp[i] = Save;
    }
  }
  */
  unguard;
}


//==========================================================================
//
//  FL_OpenFileRead
//
//==========================================================================
VStream *FL_OpenFileRead (const VStr &Name) {
  guard(FL_OpenFileRead);
  for (int i = SearchPaths.length()-1; i >= 0; --i) {
    VStream *Strm = SearchPaths[i]->OpenFileRead(Name);
    if (Strm) return Strm;
  }
  return nullptr;
  unguard;
}


//==========================================================================
//
//  FL_OpenSysFileRead
//
//==========================================================================
VStream *FL_OpenSysFileRead (const VStr &Name) {
  guard(FL_OpenSysFileRead);
  FILE *File = fopen(*Name, "rb");
  if (!File) return nullptr;
  return new VStreamFileReader(File, GCon, Name);
  unguard;
}


//==========================================================================
//
//  VStreamFileWriter
//
//==========================================================================
class VStreamFileWriter : public VStream {
protected:
  FILE *File;
  FOutputDevice *Error;
  VStr fname;

public:
  VStreamFileWriter (FILE *InFile, FOutputDevice *InError, const VStr &afname) : File(InFile), Error(InError), fname(afname) {
    guard(VStreamFileWriter::VStreamFileReader);
    bLoading = false;
    unguard;
  }

  virtual ~VStreamFileWriter () override {
    //guard(VStreamFileWriter::~VStreamFileWriter);
    if (File) Close();
    //unguard;
  }

  virtual const VStr &GetName () const override { return fname; }

  virtual void Seek (int InPos) override {
    if (!File || bError || fseek(File, InPos, SEEK_SET)) bError = true;
  }

  virtual int Tell () override {
    if (File && !bError) {
      int res = (int)ftell(File);
      if (res < 0) { bError = true; return 0; }
      return res;
    } else {
      bError = true;
      return 0;
    }
  }

  virtual int TotalSize () override {
    if (!File || bError) { bError = true; return 0; }
    int CurPos = ftell(File);
    if (fseek(File, 0, SEEK_END) != 0) { bError = true; return 0; }
    int Size = ftell(File);
    if (Size < 0) { bError = true; return 0; }
    if (fseek(File, CurPos, SEEK_SET) != 0) { bError = true; return 0; }
    return Size;
  }

  virtual bool AtEnd () override {
    if (File && !bError) return !!feof(File);
    bError = true;
    return true;
  }

  virtual bool Close () override {
    if (File && fclose(File)) bError = true;
    File = nullptr;
    return !bError;
  }

  virtual void Serialise (void *V, int Length) override {
    if (!File || bError) { bError = true; return; }
    if (fwrite(V, Length, 1, File) != 1) bError = true;
  }

  virtual void Flush () override {
    if (!File || bError) { bError = true; return; }
    if (fflush(File)) bError = true;
  }
};


//==========================================================================
//
//  FL_OpenFileWrite
//
//==========================================================================
VStream *FL_OpenFileWrite (const VStr &Name, bool isFullName) {
  guard(FL_OpenFileWrite);
  VStr tmpName;
  if (isFullName) {
    tmpName = Name;
  } else {
    if (fl_savedir.IsNotEmpty()) {
      tmpName = fl_savedir+"/"+fl_gamedir+"/"+Name;
    } else {
      tmpName = fl_basedir+"/"+fl_gamedir+"/"+Name;
    }
  }
  FL_CreatePath(tmpName.ExtractFilePath());
  FILE *File = fopen(*tmpName, "wb");
  if (!File) return nullptr;
  return new VStreamFileWriter(File, GCon, tmpName);
  unguard;
}


//==========================================================================
//
//  FL_OpenFileReadInCfgDir
//
//==========================================================================
VStream *FL_OpenFileReadInCfgDir (const VStr &Name) {
  VStr diskName = FL_GetConfigDir()+"/"+Name;
  FILE *File = fopen(*diskName, "rb");
  if (File) return new VStreamFileReader(File, GCon, Name);
  return FL_OpenFileRead(Name);
}


//==========================================================================
//
//  FL_OpenFileWriteInCfgDir
//
//==========================================================================
VStream *FL_OpenFileWriteInCfgDir (const VStr &Name) {
  VStr diskName = FL_GetConfigDir()+"/"+Name;
  FL_CreatePath(diskName.ExtractFilePath());
  FILE *File = fopen(*diskName, "wb");
  if (!File) return nullptr;
  return new VStreamFileWriter(File, GCon, Name);
}


//==========================================================================
//
//  FL_OpenSysFileWrite
//
//==========================================================================
VStream *FL_OpenSysFileWrite (const VStr &Name) {
  return FL_OpenFileWrite(Name, true);
}


//==========================================================================
//
//  VStreamFileReader
//
//==========================================================================
VStreamFileReader::VStreamFileReader(FILE *InFile, FOutputDevice *InError, const VStr &afname)
  : File(InFile)
  , Error(InError)
  , fname(afname)
{
  guard(VStreamFileReader::VStreamFileReader);
  fseek(File, 0, SEEK_SET);
  bLoading = true;
  unguard;
}


//==========================================================================
//
//  VStreamFileReader::~VStreamFileReader
//
//==========================================================================
VStreamFileReader::~VStreamFileReader() {
  if (File) Close();
}


//==========================================================================
//
//  VStreamFileReader::GetName
//
//==========================================================================
const VStr &VStreamFileReader::GetName () const {
  return fname;
}


//==========================================================================
//
//  VStreamFileReader::Seek
//
//==========================================================================
void VStreamFileReader::Seek (int InPos) {
#ifdef __SWITCH__
  // I don't know how or why this works, but unless you seek to 0 first,
  // fseeking on the Switch seems to set the pointer to an incorrect
  // position, but only sometimes
  if (File) fseek(File, 0, SEEK_SET);
#endif
  if (!File || bError || fseek(File, InPos, SEEK_SET)) bError = true;
}


//==========================================================================
//
//  VStreamFileReader::Tell
//
//==========================================================================
int VStreamFileReader::Tell () {
  if (File && !bError) {
    int res = (int)ftell(File);
    if (res < 0) { bError = true; return 0; }
    return res;
  } else {
    bError = true;
    return 0;
  }
}


//==========================================================================
//
//  VStreamFileReader::TotalSize
//
//==========================================================================
int VStreamFileReader::TotalSize () {
  if (!File || bError) { bError = true; return 0; }
  int CurPos = ftell(File);
  if (fseek(File, 0, SEEK_END) != 0) { bError = true; return 0; }
  int Size = ftell(File);
  if (Size < 0) { bError = true; return 0; }
  if (fseek(File, CurPos, SEEK_SET) != 0) { bError = true; return 0; }
  return Size;
}


//==========================================================================
//
//  VStreamFileReader::AtEnd
//
//==========================================================================
bool VStreamFileReader::AtEnd () {
  if (File && !bError) return !!feof(File);
  bError = true;
  return true;
}


//==========================================================================
//
//  VStreamFileReader::Close
//
//==========================================================================
bool VStreamFileReader::Close () {
  if (File) fclose(File);
  File = nullptr;
  return !bError;
}


//==========================================================================
//
//  VStreamFileReader::Serialise
//
//==========================================================================
void VStreamFileReader::Serialise (void *V, int Length) {
  if (!File || bError) { bError = true; return; }
  if (fread(V, Length, 1, File) != 1) bError = true;
}


//==========================================================================
//
//  FL_GetConfigDir
//
//==========================================================================
VStr FL_GetConfigDir () {
  VStr res;
#if !defined(_WIN32)
  const char *HomeDir = getenv("HOME");
  if (HomeDir && HomeDir[0]) {
    res = VStr(HomeDir)+"/.vavoom";
    Sys_CreateDirectory(res);
  } else {
    //res = (fl_savedir.IsNotEmpty() ? fl_savedir : fl_basedir);
    res = (fl_savedir.IsNotEmpty() ? fl_savedir : ".");
  }
#else
  //res = (fl_savedir.IsNotEmpty() ? fl_savedir : fl_basedir);
  res = (fl_savedir.IsNotEmpty() ? fl_savedir : ".");
#endif
  Sys_CreateDirectory(res);
  return res;
}


//==========================================================================
//
//  FL_GetCacheDir
//
//==========================================================================
VStr FL_GetCacheDir () {
  VStr res = FL_GetConfigDir();
  if (res.isEmpty()) return res;
  res += "/.mapcache";
  Sys_CreateDirectory(res);
  return res;
}


//==========================================================================
//
//  FL_GetSavesDir
//
//==========================================================================
VStr FL_GetSavesDir () {
  VStr res = FL_GetConfigDir();
  if (res.isEmpty()) return res;
  res += "/saves";
  Sys_CreateDirectory(res);
  return res;
}


//==========================================================================
//
//  FL_GetScreenshotsDir
//
//==========================================================================
VStr FL_GetScreenshotsDir () {
  VStr res = FL_GetConfigDir();
  if (res.isEmpty()) return res;
  res += "/sshots";
  Sys_CreateDirectory(res);
  return res;
}
