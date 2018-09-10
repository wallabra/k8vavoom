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


static VCvarB dbg_dump_gameinfo("dbg_dump_gameinfo", false, "Dump parsed game.txt?", 0);


struct version_t {
  //VStr MainWad;
  VStr param;
  TArray<VStr> MainWads;
  VStr GameDir;
  TArray<VStr> AddFiles;
  TArray<VStr> BaseDirs;
  int ParmFound;
  bool FixVoices;
};


VStr fl_basedir;
VStr fl_savedir;
VStr fl_gamedir;

TArray<VSearchPath *> SearchPaths;

TArray<VStr> wadfiles;
//static bool bIwadAdded;
static TArray<VStr> IWadDirs;
static int IWadIndex;
static TArray<VStr> wpklist;

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
TArray<VStr> GetWadPk3List () {
  TArray<VStr> res;
  for (int f = 0; f < wpklist.length(); ++f) {
    bool found = false;
    for (int c = 0; c < res.length(); ++c) if (res[c] == wpklist[f]) { found = true; break; }
    if (!found) res.Append(wpklist[f]);
  }
  // and sort it
  //qsort(res.Ptr(), res.length(), sizeof(VStr), cmpfuncCI);
  return res;
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

    GCon->Logf("Adding nested pk3 '%s:%s'...", *ZipName, *pk3s[i]);
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
//  AddZipFile
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

  // first add all .pk3 files in that directory
  auto dirit = Sys_OpenDir(bdx);
  if (dirit) {
    TArray<VStr> ZipFiles;
    for (VStr test = Sys_ReadDir(dirit); test.IsNotEmpty(); test = Sys_ReadDir(dirit)) {
      //fprintf(stderr, "  <%s>\n", *test);
      VStr ext = test.ExtractFileExtension().ToLower();
      if (ext == "pk3") ZipFiles.Append(test);
    }
    Sys_CloseDir(dirit);
    qsort(ZipFiles.Ptr(), ZipFiles.length(), sizeof(VStr), cmpfuncCINoExt);
    for (int i = 0; i < ZipFiles.length(); ++i) {
      wpklist.Append(dir+"/"+ZipFiles[i]);
      AddZipFile(bdx+"/"+ZipFiles[i]);
    }
  }

  // then add wad##.wad files
  /*
  VStr gwadir;
  if (fl_savedir.IsNotEmpty() && basedir != fl_savedir) gwadir = fl_savedir+"/"+dir;
  for (int i = 0; i < 1024; ++i) {
    VStr buf = bdx+"/wad"+i+".wad";
    if (!Sys_FileExists(buf)) break;
    wpklist.Append(dir+"/wad"+i+".wad");
    W_AddFile(buf, gwadir, false);
  }
  */

  // finally add directory itself
  VFilesDir *info = new VFilesDir(bdx);
  //wpklist.Append(bdx+"/");
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
  // first check in IWAD directories
  for (int i = 0; i < IWadDirs.length(); ++i) {
    if (Sys_FileExists(IWadDirs[i]+"/"+MainWad)) return IWadDirs[i]+"/"+MainWad;
  }

  // then look in the save directory
  //if (fl_savedir.IsNotEmpty() && Sys_FileExists(fl_savedir+"/"+MainWad)) return fl_savedir+"/"+MainWad;

  // finally in base directory
  if (Sys_FileExists(fl_basedir+"/"+MainWad)) return fl_basedir+"/"+MainWad;

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
static void ParseBase (const VStr &name) {
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
      break;
    }
    sc->Expect("end");
  }
  delete sc;
  if (dbg_dump_gameinfo) GCon->Logf("Done parsing game definition file \"%s\"...", *UseName);

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
    if (games.length() != 1) Sys_Error("please, select game!");
    selectedGame = 0;
  }

  version_t &gmi = games[selectedGame];

  // look for the main wad file
  VStr mainWadPath;
  for (int f = 0; f < gmi.MainWads.length(); ++f) {
    mainWadPath = FindMainWad(gmi.MainWads[f]);
    if (!mainWadPath.isEmpty()) break;
  }

  if (mainWadPath.isEmpty()) Sys_Error("Main wad file \"%s\" not found.", (gmi.MainWads.length() ? *gmi.MainWads[0] : "<none>"));

  IWadIndex = SearchPaths.length();
  //GCon->Logf("MAIN WAD(1): '%s'", *MainWadPath);
  AddAnyFile(mainWadPath, false, gmi.FixVoices);

  for (int j = 0; j < gmi.AddFiles.length(); j++) {
    VStr FName = FindMainWad(gmi.AddFiles[j]);
    if (FName.IsEmpty()) Sys_Error("Required file \"%s\" not found", *gmi.AddFiles[j]);
    AddAnyFile(FName, false);
  }

  //AddAnyFile(fl_basedir+"/"+gmi.GameDir+"/basepak.pk3", true);

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


//==========================================================================
//
//  FL_Init
//
//==========================================================================
void FL_Init () {
  guard(FL_Init);
  const char *p;

  GCon->Logf(NAME_Init, "=== INITIALIZING VaVoom ===");

  //  Set up base directory (main data files).
  fl_basedir = ".";
  p = GArgs.CheckValue("-basedir");
  if (p && p[0]) fl_basedir = p;

  //  Set up save directory (files written by engine).
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
  }

  AddGameDir("basev/common");

  ParseBase("basev/games.txt");
#ifdef DEVELOPER
  // i need progs to be loaded from files
  //fl_devmode = true;
#endif

  if (GArgs.CheckParm("-nogore") == 0) AddGameDir("basev/mods/gore");

  int fp = GArgs.CheckParm("-file");
  if (fp) {
    while (++fp != GArgs.Count() && GArgs[fp][0] != '-' && GArgs[fp][0] != '+') {
      if (!Sys_FileExists(VStr(GArgs[fp]))) {
        GCon->Logf(NAME_Init, "WARNING: File \"%s\" doesn't exist.", GArgs[fp]);
      } else {
        wpklist.Append(VStr(GArgs[fp]));
        AddAnyFile(GArgs[fp], true);
      }
    }
  }

  if (GArgs.CheckParm("-bdw") != 0) AddGameDir("basev/mods/bdw");

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

  void Serialise (void *V, int Length) {
    if (!File || bError) { bError = true; return; }
    if (fwrite(V, Length, 1, File) != 1) bError = true;
  }

  void Flush () {
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
