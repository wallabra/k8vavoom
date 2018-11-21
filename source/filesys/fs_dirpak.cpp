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


extern "C" {
  static int FileCmpFunc (const void *aa, const void *bb) {
    if (aa == bb) return 0;
    const VDirPakFile::FileEntry *a = (const VDirPakFile::FileEntry *)aa;
    const VDirPakFile::FileEntry *b = (const VDirPakFile::FileEntry *)bb;
    return VStr::Cmp(*a->pakname, *b->pakname);
  }
}


//==========================================================================
//
//  VDirPakFile::VDirPakFile
//
//==========================================================================
VDirPakFile::VDirPakFile (const VStr &aPath)
  : PakFileName(aPath.fixSlashes())
  , files()
  , filemap()
{
  while (PakFileName.endsWith("/")) PakFileName.chopRight(1);
  if (PakFileName.length() == 0) PakFileName = ".";
  ScanAllDirs();
}


//==========================================================================
//
//  VDirPakFile::~VDirPakFile
//
//==========================================================================
VDirPakFile::~VDirPakFile () {
}


//==========================================================================
//
//  VDirPakFile::ScanAllDirs
//
//==========================================================================
void VDirPakFile::ScanAllDirs () {
  ScanDirectory("", 0, false);
  if (files.length() > 65530) Sys_Error("too many files in '%s'", *PakFileName);
  // sort files alphabetically (have to do this, or file searching is failing for some reason)
  qsort(files.ptr(), files.length(), sizeof(FileEntry), &FileCmpFunc);
  //for (int f = 0; f < files.length(); ++f) GCon->Logf(NAME_Dev, "%d: ns=%d; pakname=<%s>; diskname=<%s>; lumpname=<%s>", f, files[f].ns, *files[f].pakname, *files[f].diskname, *files[f].lumpname);
}


//==========================================================================
//
//  VDirPakFile::ScanDirectory
//
//==========================================================================
void VDirPakFile::ScanDirectory (VStr relpath, int depth, bool inProgs) {
  if (!inProgs && relpath.ICmp("progs") == 0) inProgs = true;
  if (inProgs) {
    if (depth > 6) return; // too deep
  } else {
    if (depth > 1) return; // too deep
  }
  VStr scanPath = PakFileName;
  if (relpath.length()) scanPath = scanPath+"/"+relpath;
  auto dh = Sys_OpenDir(scanPath, true); // want dirs
  if (!dh) return;
  //GCon->Logf(NAME_Dev, "scanning '%s' (depth=%d)...", *scanPath, depth);
  EWadNamespace ns = (!inProgs && relpath.length() == 0 ? WADNS_Global : (EWadNamespace)-1);
  // map relpath to known namespaces
  {
    VStr xdr = relpath.toLowerCase()+"/";
    for (const VPK3ResDirInfo *di = PK3ResourceDirs; di->pfx; ++di) {
      if (xdr.Cmp(di->pfx) == 0) {
        ns = di->wadns;
        break;
      }
    }
  }
  TArray<VStr> dirlist;
  for (;;) {
    VStr dsk = Sys_ReadDir(dh);
    if (dsk.length() == 0) break;
    VStr diskname = (relpath.length() ? relpath+"/"+dsk : dsk);
    //GCon->Logf("...<%s> : <%s>", *dsk, *diskname);
    if (dsk.endsWith("/")) {
      // directory, scan it
      diskname.chopRight(1);
      dirlist.append(diskname);
    } else {
      VStr loname = diskname.toLowerCase();
      //GCon->Logf(NAME_Dev, "***dsk=<%s>; diskname=<%s>; loname=<%s>", *dsk, *diskname, *loname);
      if (filemap.has(loname)) continue;
      filemap.put(loname, files.length());
      FileEntry &fe = files.alloc();
      fe.pakname = VName(*loname);
      VStr lumpname = (ns != -1 ? loname.extractFileName().stripExtension() : VStr());
      fe.diskname = diskname;
      fe.ns = ns;
      // hide wad files, 'cause they may conflict with normal files
      // wads will be correctly added by a separate function
      if (loname.endsWith(".wad") || loname.endsWith(".zip") || loname.endsWith(".7z") ||
          loname.endsWith(".pk3") || loname.endsWith(".pk7"))
      {
        fe.ns = (EWadNamespace)-1;
        lumpname = VStr();
      }
      // skip sounds and/or sprites
      if ((fsys_skipSounds && ns == WADNS_Sounds) ||
          (fsys_skipSprites && ns == WADNS_Sprites))
      {
        fe.ns = (EWadNamespace)-1;
        lumpname = VStr();
      }
      // skip dehacked
      if (fsys_skipDehacked && lumpname.length() && lumpname.ICmp("dehacked") == 0) {
        fe.ns = (EWadNamespace)-1;
        lumpname = VStr();
      }
      // for sprites \ is a valid frame character but is not allowed to
      // be in a file name, so we do a little mapping here
      if (fe.ns == WADNS_Sprites) lumpname = lumpname.replace("^", "\\");
      // final lump name
      fe.lumpname = VName(*lumpname, VName::AddLower8);
      //GCon->Logf(NAME_Dev, "%d: ns=%d; pakname=<%s>; diskname=<%s>; lumpname=<%s>", files.length()-1, fe.ns, *fe.pakname, *fe.diskname, *fe.lumpname);
    }
  }
  Sys_CloseDir(dh);
  // scan subdirs
  for (int f = 0; f < dirlist.length(); ++f) ScanDirectory(dirlist[f], depth+1, inProgs);
}


//==========================================================================
//
//  VDirPakFile::CheckNumForFileName
//
//==========================================================================
int VDirPakFile::CheckNumForFileName (const VStr &fname) {
  VStr fn = fname.toLowerCase();
  auto lp = filemap.find(fn);
  //GCon->Logf(NAME_Dev, "DPK<%s>: '%s' is %d", *PakFileName, *fn, (lp ? *lp : -1));
  return (lp ? *lp : -1);
}


//==========================================================================
//
//  VDirPakFile::FileExists
//
//==========================================================================
bool VDirPakFile::FileExists (const VStr &fname) {
  return (CheckNumForFileName(fname) >= 0);
}


//==========================================================================
//
//  VDirPakFile::OpenFileRead
//
//==========================================================================
VStream *VDirPakFile::OpenFileRead (const VStr &fname) {
  int lump = CheckNumForFileName(fname);
  if (lump < 0) return nullptr;
  VStr tmpname = PakFileName+"/"+files[lump].diskname;
  FILE *fl = fopen(*tmpname, "rb");
  if (!fl) return nullptr;
  return new VStreamFileReader(fl, GCon, files[lump].diskname);
}


//==========================================================================
//
//  VDirPakFile::ReadFromLump
//
//==========================================================================
void VDirPakFile::ReadFromLump (int LumpNum, void *Dest, int Pos, int Size) {
  check(LumpNum >= 0);
  check(LumpNum < files.length());
  VStream *Strm = CreateLumpReaderNum(LumpNum);
  check(Strm);
  Strm->Seek(Pos);
  Strm->Serialise(Dest, Size);
  delete Strm;
}


//==========================================================================
//
//  VDirPakFile::LumpLength
//
//==========================================================================
int VDirPakFile::LumpLength (int LumpNum) {
  check(LumpNum >= 0);
  check(LumpNum < files.length());
  VStream *Strm = CreateLumpReaderNum(LumpNum);
  check(Strm);
  int Ret = Strm->TotalSize();
  delete Strm;
  return Ret;
}


//==========================================================================
//
//  VDirPakFile::CreateLumpReaderNum
//
//==========================================================================
VStream *VDirPakFile::CreateLumpReaderNum (int lump) {
  check(lump >= 0);
  check(lump < files.length());
  VStr tmpname = PakFileName+"/"+files[lump].diskname;
  FILE *fl = fopen(*tmpname, "rb");
  if (!fl) return nullptr;
  return new VStreamFileReader(fl, GCon, files[lump].diskname);
}


//==========================================================================
//
//  VDirPakFile::Close
//
//==========================================================================
void VDirPakFile::Close () {
  files.clear();
  filemap.clear();
}


//==========================================================================
//
//  VDirPakFile::CheckNumForName
//
//==========================================================================
int VDirPakFile::CheckNumForName (VName lname, EWadNamespace ns) {
  if (lname == NAME_None) return -1;
  for (int f = files.length()-1; f >= 0; --f) {
    if (files[f].ns == ns && files[f].lumpname != NAME_None && VStr::ICmp(*files[f].lumpname, *lname) == 0) return f;
  }
  return -1;
}


//==========================================================================
//
//  VDirPakFile::LumpName
//
//==========================================================================
VName VDirPakFile::LumpName (int lump) {
  return (lump >= 0 || lump < files.length() ? files[lump].lumpname : NAME_None);
}


//==========================================================================
//
//  VDirPakFile::LumpFileName
//
//==========================================================================
VStr VDirPakFile::LumpFileName (int lump) {
  return (lump >= 0 || lump < files.length() ? files[lump].diskname : VStr());
}


//==========================================================================
//
//  VDirPakFile::IterateNS
//
//==========================================================================
int VDirPakFile::IterateNS (int start, EWadNamespace ns) {
  for (; start < files.length(); ++start) {
    // it is important to not skip "hidden" files here!
    if (files[start].ns == ns) return start;
  }
  return -1;
}


//==========================================================================
//
//  VDirPakFile::RenameSprites
//
//==========================================================================
void VDirPakFile::RenameSprites (const TArray<VSpriteRename> &A, const TArray<VLumpRename> &LA) {
}


//==========================================================================
//
//  VDirPakFile::ListWadFiles
//
//==========================================================================
void VDirPakFile::ListWadFiles (TArray<VStr> &list) {
  for (int f = 0; f < files.length(); ++f) {
    if (files[f].diskname.toLowerCase().endsWith(".wad")) list.append(files[f].diskname);
  }
}


//==========================================================================
//
//  VDirPakFile::ListPk3Files
//
//==========================================================================
void VDirPakFile::ListPk3Files (TArray<VStr> &list) {
  for (int f = 0; f < files.length(); ++f) {
    if (files[f].diskname.toLowerCase().endsWith(".pk3")) list.append(files[f].diskname);
  }
}
