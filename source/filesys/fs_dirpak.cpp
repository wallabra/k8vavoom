//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
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


// ////////////////////////////////////////////////////////////////////////// //
/*
extern "C" {
  static int FileCmpFunc (const void *aa, const void *bb, void *udata) {
    if (aa == bb) return 0;
    const VDirPakFile::FileEntry *a = (const VDirPakFile::FileEntry *)aa;
    const VDirPakFile::FileEntry *b = (const VDirPakFile::FileEntry *)bb;
    return a->pakname.Cmp(*b->pakname);
  }
}
*/


//==========================================================================
//
//  VDirPakFile::VDirPakFile
//
//==========================================================================
VDirPakFile::VDirPakFile (const VStr &aPath)
  : VPakFileBase(aPath.fixSlashes())
{
  while (PakFileName.endsWith("/")) PakFileName.chopRight(1);
  if (PakFileName.length() == 0) PakFileName = ".";
  ScanAllDirs();
}


//==========================================================================
//
//  VDirPakFile::ScanAllDirs
//
//==========================================================================
void VDirPakFile::ScanAllDirs () {
  ScanDirectory("", 0);

  pakdir.buildLumpNames();
  pakdir.buildNameMaps();
}


//==========================================================================
//
//  VDirPakFile::ScanDirectory
//
//==========================================================================
void VDirPakFile::ScanDirectory (VStr relpath, int depth) {
  if (depth > 16) return; // too deep
  VStr scanPath = PakFileName;
  if (relpath.length()) scanPath = scanPath+"/"+relpath;
  auto dh = Sys_OpenDir(scanPath, true); // want dirs
  if (!dh) return;
  //GCon->Logf(NAME_Dev, "scanning '%s' (depth=%d)...", *scanPath, depth);
  // /*EWadNamespace*/vint32 ns = (relpath.length() == 0 ? WADNS_Global : /*(EWadNamespace)*/-1);
  // map relpath to known namespaces
  /*
  if (relpath.length()) {
    int sidx = relpath.indexOf('/');
    VStr xdr;
    if (sidx < 0) {
      xdr = relpath+"/";
    } else {
      xdr = relpath.left(sidx+1);
    }
    for (const VPK3ResDirInfo *di = PK3ResourceDirs; di->pfx; ++di) {
      if (xdr.ICmp(di->pfx) == 0) {
        ns = di->wadns;
        break;
      }
    }
  }
  */
  TArray<VStr> dirlist;
  for (;;) {
    VStr dsk = Sys_ReadDir(dh);
    if (dsk.length() == 0) break;
    VStr diskname = (relpath.length() ? relpath+"/"+dsk : dsk).fixSlashes();
    //GCon->Logf("...<%s> : <%s>", *dsk, *diskname);
    if (dsk.endsWith("/")) {
      // directory, scan it
      diskname.chopRight(1);
      dirlist.append(diskname);
    } else {
      VStr loname = diskname.toLowerCase();
      //GCon->Logf(NAME_Dev, "***dsk=<%s>; diskname=<%s>; loname=<%s>", *dsk, *diskname, *loname);
      if (pakdir.filemap.has(loname)) continue;
      pakdir.filemap.put(loname, pakdir.files.length());
      VPakFileInfo fe;
      fe.filesize = -1; // unknown yet
      fe.fileName = loname;
      fe.diskName = diskname;
      check(fe.lumpName == NAME_None);
      check(fe.lumpNamespace == -1);
      // fe.lumpNamespace = ns;
      //GCon->Logf(NAME_Dev, "%d: ns=%d; pakname=<%s>; diskname=<%s>; lumpname=<%s>", files.length()-1, fe.ns, *fe.pakname, *fe.diskname, *fe.lumpname);
      pakdir.append(fe);
    }
  }
  Sys_CloseDir(dh);
  // scan subdirs
  for (int f = 0; f < dirlist.length(); ++f) ScanDirectory(dirlist[f], depth+1);
}


//==========================================================================
//
//  VDirPakFile::OpenFileRead
//
//==========================================================================
VStream *VDirPakFile::OpenFileRead (const VStr &fname) {
  int lump = CheckNumForFileName(fname);
  if (lump < 0) return nullptr;
  VStr tmpname = PakFileName+"/"+pakdir.files[lump].diskName;
  FILE *fl = fopen(*tmpname, "rb");
  if (!fl) return nullptr;
  return new VStreamFileReader(fl, GCon, pakdir.files[lump].diskName);
}


//==========================================================================
//
//  VDirPakFile::LumpLength
//
//==========================================================================
int VDirPakFile::LumpLength (int LumpNum) {
  check(LumpNum >= 0);
  check(LumpNum < pakdir.files.length());
  if (pakdir.files[LumpNum].filesize == -1) {
    VStream *Strm = CreateLumpReaderNum(LumpNum);
    check(Strm);
    pakdir.files[LumpNum].filesize = Strm->TotalSize();
    delete Strm;
  }
  return pakdir.files[LumpNum].filesize;
}


//==========================================================================
//
//  VDirPakFile::CreateLumpReaderNum
//
//==========================================================================
VStream *VDirPakFile::CreateLumpReaderNum (int lump) {
  check(lump >= 0);
  check(lump < pakdir.files.length());
  VStr tmpname = PakFileName+"/"+pakdir.files[lump].diskName;
  FILE *fl = fopen(*tmpname, "rb");
  if (!fl) return nullptr;
  return new VStreamFileReader(fl, GCon, pakdir.files[lump].diskName);
}
