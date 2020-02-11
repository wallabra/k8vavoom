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
//**  Copyright (C) 2018-2020 Ketmar Dark
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


bool fsys_developer_debug = false;
int fsys_IgnoreZScript = 1;
bool fsys_DisableBDW = false;
bool fsys_report_added_paks = true;

bool fsys_skipSounds = false;
bool fsys_skipSprites = false;
bool fsys_skipDehacked = false;

// autodetected wad/pk3
int fsys_detected_mod = AD_NONE;
VStr fsys_detected_mod_wad;

int fsys_ignoreSquare = 1; // do not check for "Adventures of Square"

// local
int fsys_dev_dump_paks = 0;


TArray<VSearchPath *> SearchPaths;
TArray<VStr> wadfiles;
TArray<VStr> fsys_game_filters;

mythread_mutex fsys_glock;


// ////////////////////////////////////////////////////////////////////////// //
class FSys_Internal_Init_Class {
public:
  FSys_Internal_Init_Class (bool) {
    mythread_mutex_init(&fsys_glock);
  }
};

__attribute__((used)) FSys_Internal_Init_Class fsys_internal_init_class_variable_(true);


//==========================================================================
//
//  FSYS_Shutdown
//
//==========================================================================
void FSYS_InitOptions (VParsedArgs &pargs) {
  pargs.RegisterFlagSet("-ignore-square", "do not check for \"Adventures of Square\"", &fsys_ignoreSquare);
  pargs.RegisterFlagSet("-ignore-zscript", "!", &fsys_IgnoreZScript);
  pargs.RegisterFlagSet("-fsys-dump-paks", "!dump loaded pak files", &fsys_dev_dump_paks);
}


//==========================================================================
//
//  FSYS_Shutdown
//
//==========================================================================
void FSYS_Shutdown () {
  MyThreadLocker glocker(&fsys_glock);
  for (int i = 0; i < SearchPaths.length(); ++i) {
    delete SearchPaths[i];
    SearchPaths[i] = nullptr;
  }
  SearchPaths.Clear();
  wadfiles.Clear();
}


//==========================================================================
//
//  FL_ClearGameFilters
//
//==========================================================================
void FL_ClearGameFilters () {
  fsys_game_filters.clear();
}


extern "C" {
  int cmpGameFilter (const void *aa, const void *bb, void *) {
    if (aa == bb) return 0;
    const VStr *a = (const VStr *)aa;
    const VStr *b = (const VStr *)bb;
    if (a->length() < b->length()) return -1;
    if (a->length() > b->length()) return 1;
    return a->ICmp(*b);
  }
}


//==========================================================================
//
//  FL_AddGameFilter
//
//  add new filter; it should start with "filter/"
//  duplicate filters will be ignored
//  returns 0 for "no error"
//
//==========================================================================
int FL_AddGameFilter (VStr path) {
  path = path.fixSlashes();
  while (path.length() && path[0] == '/') path.chopLeft(1);
  while (path.length() && path.endsWith("/")) path.chopRight(1);
  if (path.isEmpty()) return FL_ADDFILTER_INVALID;
  if (!path.startsWithCI("filter/")) return FL_ADDFILTER_INVALID;
  if (path.length() < 8) return FL_ADDFILTER_INVALID; // just in case
  // look for duplicates
  for (auto &&flt : fsys_game_filters) if (flt.strEquCI(path)) return FL_ADDFILTER_DUPLICATE;
  fsys_game_filters.append(path);
  // sort them, because why not?
  //GLog.Log(NAME_Debug, ":::: B: ===="); for (auto &&s : fsys_game_filters) GLog.Logf(NAME_Debug, "  <%s>", *s);
  timsort_r(fsys_game_filters.ptr(), fsys_game_filters.length(), sizeof(VStr), &cmpGameFilter, nullptr);
  //GLog.Log(NAME_Debug, ":::: A: ===="); for (auto &&s : fsys_game_filters) GLog.Logf(NAME_Debug, "  <%s>", *s);
  return FL_ADDFILTER_OK;
}


//==========================================================================
//
//  FL_CheckFilterName
//
//  returns `false` if file was filtered out (and clears name)
//  returns `true` if file should be kept (and modifies name if necessary)
//
//==========================================================================
bool FL_CheckFilterName (VStr &fname) {
  if (fname.isEmpty()) return false; // empty names should not be kept ;-)
  if (fsys_game_filters.length() == 0) return true; // keep it
  if (!fname.startsWithNoCase("filter/")) return true; // keep it
  if (fname.endsWith("/")) { fname.clear(); return false; } // drop it (it is a directory)
  // latest filter is always the best one
  for (int f = fsys_game_filters.length()-1; f >= 0; --f) {
    VStr fs = fsys_game_filters[f];
    if (fname.length() > fs.length()+1 && fname[fs.length()] == '/' && fname.startsWithNoCase(fs)) {
      fname.chopLeft(fs.length()+1);
      while (fname.length() && fname[0] == '/') fname.chopLeft(1);
      return !fname.isEmpty(); // just in case
    }
  }
  // drop it
  fname.clear();
  return false;
}


//==========================================================================
//
//  FL_FileExists
//
//==========================================================================
bool FL_FileExists (VStr fname) {
  if (fname.isEmpty()) return false;
  MyThreadLocker glocker(&fsys_glock);
  for (int i = SearchPaths.length()-1; i >= 0; --i) {
    if (SearchPaths[i]->FileExists(fname)) return true;
  }
  return false;
}


//==========================================================================
//
//  FL_OpenFileReadBaseOnly_NoLock
//
//==========================================================================
VStream *FL_OpenFileReadBaseOnly_NoLock (VStr Name) {
  if (Name.isEmpty()) return nullptr;
  for (int i = SearchPaths.length()-1; i >= 0; --i) {
    if (!SearchPaths[i]->basepak) continue;
    VStream *Strm = SearchPaths[i]->OpenFileRead(Name);
    if (Strm) return Strm;
  }
  return nullptr;
}


//==========================================================================
//
//  FL_OpenFileRead_NoLock
//
//==========================================================================
VStream *FL_OpenFileRead_NoLock (VStr Name) {
  if (Name.isEmpty()) return nullptr;
  if (Name.length() >= 2 && Name[0] == '/' && Name[1] == '/') {
    return FL_OpenFileReadBaseOnly_NoLock(Name.mid(2, Name.length()));
  } else {
    for (int i = SearchPaths.length()-1; i >= 0; --i) {
      VStream *Strm = SearchPaths[i]->OpenFileRead(Name);
      if (Strm) return Strm;
    }
  }
  return nullptr;
}


//==========================================================================
//
//  FL_OpenFileReadBaseOnly
//
//==========================================================================
VStream *FL_OpenFileReadBaseOnly (VStr Name) {
  if (Name.isEmpty()) return nullptr;
  MyThreadLocker glocker(&fsys_glock);
  return FL_OpenFileReadBaseOnly_NoLock(Name);
}


//==========================================================================
//
//  FL_OpenFileRead
//
//==========================================================================
VStream *FL_OpenFileRead (VStr Name) {
  if (Name.isEmpty()) return nullptr;
  MyThreadLocker glocker(&fsys_glock);
  return FL_OpenFileRead_NoLock(Name);
}


//==========================================================================
//
//  FL_CreatePath
//
//==========================================================================
void FL_CreatePath (VStr Path) {
  if (Path.isEmpty() || Path == ".") return;
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
}


//==========================================================================
//
//  FL_OpenSysFileRead
//
//==========================================================================
VStream *FL_OpenSysFileRead (VStr Name) {
  if (Name.isEmpty()) return nullptr;
  FILE *File = fopen(*Name, "rb");
  if (!File) return nullptr;
  return new VStdFileStreamRead(File, Name);
}


//==========================================================================
//
//  FL_OpenSysFileWrite
//
//==========================================================================
VStream *FL_OpenSysFileWrite (VStr Name) {
  if (Name.isEmpty()) return nullptr;
  FL_CreatePath(Name.ExtractFilePath());
  FILE *File = fopen(*Name, "wb");
  if (!File) return nullptr;
  return new VStdFileStreamWrite(File, Name);
}


//==========================================================================
//
//  FL_IsSafeDiskFileName
//
//==========================================================================
bool FL_IsSafeDiskFileName (VStr fname) {
  if (fname.isEmpty()) return false;
  return fname.isSafeDiskFileName();
}


// ////////////////////////////////////////////////////////////////////////// //
// i have to do this, otherwise the linker will optimise openers away
#include "fsys_register.cpp"
