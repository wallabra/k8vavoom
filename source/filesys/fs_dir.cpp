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


//==========================================================================
//
//  VFilesDir::VFilesDir
//
//==========================================================================
VFilesDir::VFilesDir (const VStr &aPath)
  : path(aPath)
  , cachedFiles()
  , cachedMap()
  //, cacheInited(false)
{
  //while (path.length() > 0 && path[path.length()-1] == '/') path = path.chopRight(1);
  if (path.length() == 0) path = "./";
#ifdef _WIN32
  if (path[path.length()-1] != '/' && path[path.length()-1] != '\\') path += "/";
#else
  if (path[path.length()-1] != '/') path += "/";
#endif
  //cacheDir();
}


//==========================================================================
//
//  VFilesDir::cacheDir
//
//==========================================================================
/*
void VFilesDir::cacheDir () {
  if (!cacheInited) {
    cacheInited = true;
    // scan directory
    auto dh = Sys_OpenDir(path);
    if (dh) {
      for (;;) {
        VStr dsk = Sys_ReadDir(dh);
        if (dsk.length() == 0) break;
        cachedFiles.Append(dsk);
      }
      Sys_CloseDir(dh);
    }
  }
}
*/

//==========================================================================
//
//  VFilesDir::findFileCI
//
//==========================================================================
int VFilesDir::findFileCI (VStr fname) {
  // search
  VStr fn = fname.ToLower(); //(ignoreExt ? fname.stripExtension() : fname);
  int *idx = cachedMap.find(fn);
  if (idx) return *idx;
  // add to cache
  fn = path+fname;
  if (!Sys_FileExists(path+fname)) return -1;
  //fn = (path+fname).ToLower();
  int newidx = cachedFiles.length();
  cachedFiles.append(fname);
  cachedMap.put(fn, newidx);
  return newidx;
  /*
  for (int f = cachedFiles.length()-1; f >= 0; --f) {
    VStr cfn = cachedFiles[f];
    //if (ignoreExt) cfn.stripExtension();
    if (cfn.ICmp(fn) == 0) return f;
  }
  return -1;
  */
}


//==========================================================================
//
//  VFilesDir::CheckNumForFileName
//
//==========================================================================
int VFilesDir::CheckNumForFileName (const VStr &Name) {
  guard(VFilesDir::CheckNumForFileName);
  return findFileCI(Name);
  unguard;
}


//==========================================================================
//
//  VFilesDir::FileExists
//
//==========================================================================
bool VFilesDir::FileExists (const VStr &Name) {
  guard(VFilesDir::FileExists);
  return (findFileCI(Name) >= 0);
  unguard;
}


//==========================================================================
//
//  VFilesDir::OpenFileRead
//
//==========================================================================
VStream *VFilesDir::OpenFileRead (const VStr &Name) {
  guard(FL_OpenFileRead);
  VStr storedName = Name;
  int fidx = findFileCI(storedName);
  if (fidx == -1) return nullptr;
  VStr tmpName = path+"/"+storedName;
  FILE *File = fopen(*tmpName, "rb");
  if (!File) return nullptr;
  //fprintf(stderr, "***DISK: <%s:%s>\n", *GetPrefix(), *Name);
  return new VStreamFileReader(File, GCon, tmpName);
  unguard;
}


//==========================================================================
//
//  VFilesDir::ReadFromLump
//
//==========================================================================
void VFilesDir::ReadFromLump (int LumpNum, void *Dest, int Pos, int Size) {
  guard(VFilesDir::ReadFromLump);
  check(LumpNum >= 0);
  check(LumpNum < cachedFiles.length());
  VStream *Strm = CreateLumpReaderNum(LumpNum);
  check(Strm);
  Strm->Seek(Pos);
  Strm->Serialise(Dest, Size);
  delete Strm;
  unguard;
}


//==========================================================================
//
//  VFilesDir::LumpLength
//
//==========================================================================
int VFilesDir::LumpLength (int LumpNum) {
  guard(VFilesDir::LumpLength);
  check(LumpNum >= 0);
  check(LumpNum < cachedFiles.length());
  VStream *Strm = CreateLumpReaderNum(LumpNum);
  check(Strm);
  int Ret = Strm->TotalSize();
  delete Strm;
  return Ret;
  unguard;
}


//==========================================================================
//
//  VFilesDir::CreateLumpReaderNum
//
//==========================================================================
VStream *VFilesDir::CreateLumpReaderNum (int LumpNum) {
  guard(VFilesDir::CreateLumpReaderNum);
  check(LumpNum >= 0);
  check(LumpNum < cachedFiles.length());
  VStream *Strm = OpenFileRead(cachedFiles[LumpNum]);
  check(Strm);
  return Strm;
  unguard;
}


//==========================================================================
//
//  VFilesDir::Close
//
//==========================================================================
void VFilesDir::Close () {
}


//==========================================================================
//
//  VFilesDir::CheckNumForName
//
//==========================================================================
int VFilesDir::CheckNumForName (VName LumpName, EWadNamespace InNS) {
  /*
  if (InNS >= WADNS_ZipSpecial) InNS = WADNS_Global;
  if (InNS != WADNS_Global) return -1;
  int fidx = findFileCI(LumpName);
  if (fidx != -1) return fidx;
  for (int f = cachedFiles.length()-1; f >= 0; --f) {
    VStr cfn = cachedFiles[f].stripExtension();
    if (cfn.ICmp(LumpName) == 0) return f;
  }
  */
  return -1;
}


//==========================================================================
//
//  VFilesDir::LumpName
//
//==========================================================================
VName VFilesDir::LumpName (int LumpNum) {
  return (LumpNum >= 0 || LumpNum < cachedFiles.length() ? VName(*cachedFiles[LumpNum]) : NAME_None);
}


//==========================================================================
//
//  VFilesDir::LumpFileName
//
//==========================================================================
VStr VFilesDir::LumpFileName (int LumpNum) {
  return (LumpNum >= 0 || LumpNum < cachedFiles.length() ? cachedFiles[LumpNum] : VStr());
}


//==========================================================================
//
//  VFilesDir::IterateNS
//
//==========================================================================
int VFilesDir::IterateNS (int Start, EWadNamespace NS) {
  /*
  if (InNS >= WADNS_ZipSpecial) InNS = WADNS_Global;
  if (InNS != WADNS_Global) return -1;
  if (Start < 0 || Start >= cachedFiles.length()) return -1;
  return Start;
  */
  return -1;
}


//==========================================================================
//
//  VFilesDir::RenameSprites
//
//==========================================================================
void VFilesDir::RenameSprites (const TArray<VSpriteRename> &A, const TArray<VLumpRename> &LA) {
}
