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
#include "fsys_local.h"


bool fsys_developer_debug = false;
bool fsys_IgnoreZScript = true;
bool fsys_DisableBDW = false;
bool fsys_report_added_paks = true;

TArray<VSearchPath *> SearchPaths;
TArray<VStr> wadfiles;
TArray<VStr> fsys_game_filters;


//==========================================================================
//
//  FSYS_Shutdown
//
//==========================================================================
void FSYS_Shutdown () {
  for (int i = 0; i < SearchPaths.length(); ++i) {
    delete SearchPaths[i];
    SearchPaths[i] = nullptr;
  }
  SearchPaths.Clear();
  wadfiles.Clear();
}


//==========================================================================
//
//  FL_CheckFilterName
//
//  removes prefix, returns filter index (or -1, and does nothing)
//
//==========================================================================
int FL_CheckFilterName (VStr &fname) {
  if (fname.isEmpty() || fsys_game_filters.length() == 0) return -1;
  if (!fname.startsWithNoCase("filter/")) return -1;
  //GLog.Logf("!!! %d", fsys_game_filters.length());
  int bestIdx = -1;
  for (int f = 0; f < fsys_game_filters.length(); ++f) {
    const VStr &fs = fsys_game_filters[f];
    //GLog.Logf("f=%d; fs=<%s>; fname=<%s>", f, *fs, *fname);
    if (fname.length() > fs.length()+1 && fname[fs.length()] == '/' && fname.startsWith(fs)) {
      bestIdx = f;
    }
  }
  if (bestIdx >= 0) {
    const VStr &fs = fsys_game_filters[bestIdx];
    fname.chopLeft(fs.length()+1);
    while (fname.length() && fname[0] == '/') fname.chopLeft(1);
  }
  return bestIdx;
}


//==========================================================================
//
//  FL_FileExists
//
//==========================================================================
bool FL_FileExists (const VStr &fname) {
  for (int i = SearchPaths.length()-1; i >= 0; --i) {
    if (SearchPaths[i]->FileExists(fname)) return true;
  }
  return false;
}


//==========================================================================
//
//  FL_OpenFileRead
//
//==========================================================================
VStream *FL_OpenFileRead (const VStr &Name) {
  for (int i = SearchPaths.length()-1; i >= 0; --i) {
    VStream *Strm = SearchPaths[i]->OpenFileRead(Name);
    if (Strm) return Strm;
  }
  return nullptr;
}


//==========================================================================
//
//  FL_OpenFileReadBaseOnly
//
//==========================================================================
VStream *FL_OpenFileReadBaseOnly (const VStr &Name) {
  for (int i = SearchPaths.length()-1; i >= 0; --i) {
    if (!SearchPaths[i]->basepak) continue;
    VStream *Strm = SearchPaths[i]->OpenFileRead(Name);
    if (Strm) return Strm;
  }
  return nullptr;
}


//==========================================================================
//
//  FL_CreatePath
//
//==========================================================================
void FL_CreatePath (const VStr &Path) {
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
VStream *FL_OpenSysFileRead (const VStr &Name) {
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
VStream *FL_OpenSysFileWrite (const VStr &Name) {
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
bool FL_IsSafeDiskFileName (const VStr &fname) {
  return fname.isSafeDiskFileName();
}
