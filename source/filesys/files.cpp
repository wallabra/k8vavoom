//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
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
#include "gamedefs.h"
#include "fs_local.h"

extern VCvarB game_release_mode;

static VCvarB dbg_dump_gameinfo("dbg_dump_gameinfo", false, "Dump parsed game.txt?", 0);

bool fsys_skipSounds = false;
bool fsys_skipSprites = false;
bool fsys_skipDehacked = false;

int fsys_warp_n0 = -1;
int fsys_warp_n1 = -1;
VStr fsys_warp_cmd;

static bool fsys_onlyOneBaseFile = false;


struct version_t {
  VStr param;
  TArray<VStr> MainWads;
  VStr GameDir;
  TArray<VStr> AddFiles;
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


static VCvarS game_name("game_name", "unknown", "The Name Of The Game.", CVAR_Rom);


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
    VStr GwaName = Wads[i].StripExtension()+".gwa";
    VStream *WadStrm = Zip->OpenFileRead(Wads[i]);
    VStream *GwaStrm = Zip->OpenFileRead(GwaName);

    // decompress WAD and GWA files into a memory stream since reading from ZIP will be very slow
    size_t Len = WadStrm->TotalSize();
    vuint8 *Buf = new vuint8[Len];
    WadStrm->Serialise(Buf, Len);
    delete WadStrm;
    WadStrm = nullptr;
    WadStrm = new VMemoryStream(Buf, Len);
    delete[] Buf;
    Buf = nullptr;

    if (GwaStrm) {
      Len = GwaStrm->TotalSize();
      Buf = new vuint8[Len];
      GwaStrm->Serialise(Buf, Len);
      delete GwaStrm;
      GwaStrm = nullptr;
      GwaStrm = new VMemoryStream(Buf, Len);
      delete[] Buf;
      Buf = nullptr;
    }

    W_AddFileFromZip(ZipName+":"+Wads[i], WadStrm, ZipName+":"+GwaName, GwaStrm);
  }

  if (!allowpk3) return;

  // add all pk3 files in the root of the ZIP file
  TArray<VStr> pk3s;
  Zip->ListPk3Files(pk3s);
  for (int i = 0; i < pk3s.length(); ++i) {
    VStream *ZipStrm = Zip->OpenFileRead(pk3s[i]);

    // decompress file into a memory stream since reading from ZIP will be very slow
    size_t Len = ZipStrm->TotalSize();
    vuint8 *Buf = new vuint8[Len];
    ZipStrm->Serialise(Buf, Len);
    delete ZipStrm;
    ZipStrm = nullptr;

    ZipStrm = new VMemoryStream(Buf, Len);
    delete[] Buf;
    Buf = nullptr;

    if (fsys_report_added_paks) GCon->Logf(NAME_Init, "Adding nested pk3 '%s:%s'...", *ZipName, *pk3s[i]);
    VZipFile *pk3 = new VZipFile(ZipStrm, ZipName+":"+pk3s[i]);
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
    GCon->Logf("cannot add file \"%s\"", *fname);
    return;
  }
  if (ext == "pk3" || ext == "zip") {
    AddZipFile(fname);
  } else {
    if (allowFail) {
      W_AddFile(fname, VStr(), false);
    } else {
      W_AddFile(fname, fl_savedir, fixVoices);
    }
  }
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
      if (test[0] == '_' || test[0] =='.') continue; // skip it
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
  if (!fsys_onlyOneBaseFile) {
    for (int i = 0; i < WadFiles.length(); ++i) {
      //if (i == 0 && ZipFiles.length() == 0) wpkAppend(dir+"/"+WadFiles[i], true); // system pak
      W_AddFile(bdx+"/"+WadFiles[i], VStr(), false);
    }
  }

  for (int i = 0; i < ZipFiles.length(); ++i) {
    //if (i == 0) wpkAppend(dir+"/"+ZipFiles[i], true); // system pak
    AddZipFile(bdx+"/"+ZipFiles[i]);
    if (fsys_onlyOneBaseFile) break;
  }

  // finally add directory itself
  VFilesDir *info = new VFilesDir(bdx);
  SearchPaths.Append(info);
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

  if (dbg_dump_gameinfo) GCon->Logf("Parsing game definition file \"%s\"...", *UseName);
  VScriptParser *sc = new VScriptParser(UseName, FL_OpenSysFileRead(UseName));
  while (!sc->AtEnd()) {
    version_t &dst = games.Alloc();
    dst.ParmFound = 0;
    dst.FixVoices = false;
    sc->Expect("game");
    sc->ExpectString();
    dst.GameDir = sc->String;
    if (dbg_dump_gameinfo) GCon->Logf(" game dir: \"%s\"", *dst.GameDir);
    for (;;) {
      if (sc->Check("iwad")) {
        sc->ExpectString();
        if (sc->String.isEmpty()) continue;
        if (dst.MainWads.length() == 0) {
          dst.MainWads.Append(sc->String);
          if (dbg_dump_gameinfo) GCon->Logf("  iwad: \"%s\"", *sc->String);
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
        if (dbg_dump_gameinfo) GCon->Logf("  alternate iwad: \"%s\"", *sc->String);
        continue;
      }
      if (sc->Check("addfile")) {
        sc->ExpectString();
        if (sc->String.isEmpty()) continue;
        dst.AddFiles.Append(sc->String);
        if (dbg_dump_gameinfo) GCon->Logf("  aux file: \"%s\"", *sc->String);
        continue;
      }
      if (sc->Check("base")) {
        sc->ExpectString();
        if (sc->String.isEmpty()) continue;
        dst.BaseDirs.Append(sc->String);
        if (dbg_dump_gameinfo) GCon->Logf("  base: \"%s\"", *sc->String);
        continue;
      }
      if (sc->Check("param")) {
        sc->ExpectString();
        if (sc->String.length() < 2 || sc->String[0] != '-') sc->Error(va("invalid game (%s) param!", *dst.GameDir));
        dst.param = (*sc->String)+1;
        if (dbg_dump_gameinfo) GCon->Logf("  param: \"%s\"", (*sc->String)+1);
        dst.ParmFound = GArgs.CheckParm(*sc->String);
        continue;
      }
      if (sc->Check("fixvoices")) {
        dst.FixVoices = true;
        if (dbg_dump_gameinfo) GCon->Logf("  fix voices: tan");
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
  if (dbg_dump_gameinfo) GCon->Logf("Done parsing game definition file \"%s\"...", *UseName);

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
    if (dbg_dump_gameinfo) GCon->Logf("SELECTED GAME: \"%s\"", *games[selectedGame].param);
  } else {
    if (games.length() != 1) {
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
    if (selectedGame < 0) Sys_Error("please, select game!");
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
    VStr FName = FindMainWad(gmi.AddFiles[j]);
    if (FName.IsEmpty()) Sys_Error("Required file \"%s\" not found", *gmi.AddFiles[j]);
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
//  FL_Init
//
//==========================================================================
void FL_Init () {
  guard(FL_Init);
  const char *p;
  VStr mainIWad = VStr();
  int wmap1 = -1, wmap2 = -1; // warp

  //GCon->Logf(NAME_Init, "=== INITIALIZING VaVoom ===");

  if (GArgs.CheckParm("-fast") != 0) fastparm = true;
  if (GArgs.CheckParm("-respawn") != 0) respawnparm = true;
  if (GArgs.CheckParm("-nomonsters") != 0) NoMonsters = true;

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

  fsys_onlyOneBaseFile = GArgs.CheckParm("-nakedbase");

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

  if (game_release_mode) {
    if (GArgs.CheckParm("-gore") != 0) AddGameDir("basev/mods/gore");
  } else {
    if (GArgs.CheckParm("-nogore") == 0) AddGameDir("basev/mods/gore");
  }

  int fp = GArgs.CheckParm("-file");
  if (fp) {
    fsys_report_added_paks = reportPWads;
    fsys_skipSounds = false;
    fsys_skipSprites = false;
    bool noStoreInSave = false;
    for (int f = 1; f < fp; ++f) {
           if (VStr::Cmp(GArgs[f], "-skipsounds") == 0) fsys_skipSounds = true;
      else if (VStr::Cmp(GArgs[f], "-allowsounds") == 0) fsys_skipSounds = false;
      else if (VStr::Cmp(GArgs[f], "-skipsprites") == 0) fsys_skipSprites = true;
      else if (VStr::Cmp(GArgs[f], "-allowsprites") == 0) fsys_skipSprites = false;
      else if (VStr::Cmp(GArgs[f], "-skipdehacked") == 0) fsys_skipDehacked = true;
      else if (VStr::Cmp(GArgs[f], "-allodehacked") == 0) fsys_skipDehacked = false;
    }
    bool inFile = true;
    while (++fp != GArgs.Count()) {
      if (GArgs[fp][0] == '-' || GArgs[fp][0] == '+') {
             if (VStr::Cmp(GArgs[fp], "-skipsounds") == 0) fsys_skipSounds = true;
        else if (VStr::Cmp(GArgs[fp], "-allowsounds") == 0) fsys_skipSounds = false;
        else if (VStr::Cmp(GArgs[fp], "-skipsprites") == 0) fsys_skipSprites = true;
        else if (VStr::Cmp(GArgs[fp], "-allowsprites") == 0) fsys_skipSprites = false;
        else if (VStr::Cmp(GArgs[fp], "-skipdehacked") == 0) fsys_skipDehacked = true;
        else if (VStr::Cmp(GArgs[fp], "-allowdehacked") == 0) fsys_skipDehacked = false;
        else if (VStr::Cmp(GArgs[fp], "-cosmetic") == 0) noStoreInSave = true;
        else { inFile = (VStr::Cmp(GArgs[fp], "-file") == 0); if (inFile) noStoreInSave = false; }
        continue;
      }
      if (!inFile) continue;
      if (!Sys_FileExists(VStr(GArgs[fp]))) {
        GCon->Logf(NAME_Init, "WARNING: File \"%s\" doesn't exist.", GArgs[fp]);
      } else {
        if (!noStoreInSave) wpkAppend(VStr(GArgs[fp]), false); // non-system pak
        AddAnyFile(GArgs[fp], true);
      }
      noStoreInSave = false; // autoreset
    }
    fsys_skipSounds = false;
    fsys_skipSprites = false;
  }

  fsys_report_added_paks = reportIWads;
  if (GArgs.CheckParm("-bdw") != 0) AddGameDir("basev/mods/bdw");
  fsys_report_added_paks = reportPWads;

  RenameSprites();
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
//  FL_GetConfigDir
//
//==========================================================================
VStr FL_GetConfigDir () {
  VStr res;
#if !defined(_WIN32)
  const char *HomeDir = getenv("HOME");
  if (HomeDir && HomeDir[0]) {
    res = VStr(HomeDir)+"/.vavoom";
  }
#else
  res = ".";
#endif
  return res;
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
  fseek(File, 0, SEEK_SET);
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
