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


//==========================================================================
//
//  VFilesDir::VFilesDir
//
//==========================================================================
VFilesDir::VFilesDir (const VStr &aPath)
  : path(aPath)
  , CachedFiles()
  , cacheInited(false)
{
  //while (path.length() > 0 && path[path.length()-1] == '/') path = path.chopRight(1);
  if (path.length() == 0) path = "./";
#ifdef _WIN32
  if (path[path.length()-1] != '/' && path[path.length()-1] != '\\') path += "/";
#else
  if (path[path.length()-1] != '/') path += "/";
#endif
  cacheDir();
}


//==========================================================================
//
//  VFilesDir::cacheDir
//
//==========================================================================
void VFilesDir::cacheDir () {
  if (!cacheInited) {
    cacheInited = true;
    // scan directory
    auto dh = Sys_OpenDir(path);
    if (dh) {
      for (;;) {
        VStr dsk = Sys_ReadDir(dh);
        if (dsk.length() == 0) break;
        CachedFiles.Append(dsk);
      }
      Sys_CloseDir(dh);
    }
  }
}


//==========================================================================
//
//  VFilesDir::findFileCI
//
//==========================================================================
int VFilesDir::findFileCI (const VStr &fname) {
  // search
  VStr fn = fname; //(ignoreExt ? fname.stripExtension() : fname);
  for (int f = CachedFiles.length()-1; f >= 0; --f) {
    VStr cfn = CachedFiles[f];
    //if (ignoreExt) cfn.stripExtension();
    if (cfn.ICmp(fn) == 0) return f;
  }
  return -1;
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
  int fidx = findFileCI(Name);
  if (fidx == -1) return nullptr;
  VStr tmpName = path+"/"+Name;
  FILE *File = fopen(*tmpName, "rb");
  if (!File) return nullptr;
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
  check(LumpNum < CachedFiles.length());
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
  check(LumpNum < CachedFiles.length());
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
  check(LumpNum < CachedFiles.length());
  VStream *Strm = OpenFileRead(CachedFiles[LumpNum]);
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
  for (int f = CachedFiles.length()-1; f >= 0; --f) {
    VStr cfn = CachedFiles[f].stripExtension();
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
  return (LumpNum >= 0 || LumpNum < CachedFiles.length() ? VName(*CachedFiles[LumpNum]) : NAME_None);
}


//==========================================================================
//
//  VFilesDir::LumpFileName
//
//==========================================================================
VStr VFilesDir::LumpFileName (int LumpNum) {
  return (LumpNum >= 0 || LumpNum < CachedFiles.length() ? CachedFiles[LumpNum] : VStr());
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
  if (Start < 0 || Start >= CachedFiles.length()) return -1;
  return Start;
  */
  return -1;
}


//==========================================================================
//
//  VFilesDir::BuildGLNodes
//
//==========================================================================
void VFilesDir::BuildGLNodes (VSearchPath *GlWad) {
  Sys_Error("BuildGLNodes on directory");
}


//==========================================================================
//
//  VFilesDir::BuildPVS
//
//==========================================================================
void VFilesDir::BuildPVS (VSearchPath *BaseWad) {
  Sys_Error("BuildPVS on directory");
}


//==========================================================================
//
//  VFilesDir::RenameSprites
//
//==========================================================================
void VFilesDir::RenameSprites (const TArray<VSpriteRename> &A, const TArray<VLumpRename> &LA) {
}
