//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2018-2021 Ketmar Dark
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
#include "../fsys_local.h"


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
VDirPakFile::VDirPakFile (VStr aPath)
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
  //GLog.Logf(NAME_Dev, "scanning '%s' (depth=%d)...", *scanPath, depth);
  // /*EWadNamespace*/vint32 ns = (relpath.length() == 0 ? WADNS_Global : WADNS_Any);
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
    //GLog.Logf("...<%s> : <%s>", *dsk, *diskname);
    if (dsk.endsWith("/")) {
      // directory, scan it
      diskname.chopRight(1);
      dirlist.append(diskname);
    } else {
      VStr loname = diskname.toLowerCase();
      //GLog.Logf(NAME_Dev, "***dsk=<%s>; diskname=<%s>; loname=<%s>", *dsk, *diskname, *loname);
      if (pakdir.filemap.has(loname)) continue;
      pakdir.filemap.put(loname, pakdir.files.length());
      VPakFileInfo fe;
      fe.filesize = -1; // unknown yet
      fe.fileName = loname;
      fe.diskName = diskname;
      vassert(fe.lumpName == NAME_None);
      vassert(fe.lumpNamespace == -1);
      // fe.lumpNamespace = ns;
      //GLog.Logf(NAME_Dev, "%d: ns=%d; pakname=<%s>; diskname=<%s>; lumpname=<%s>", files.length()-1, fe.ns, *fe.pakname, *fe.diskname, *fe.lumpname);
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
//  if `lump` is not `nullptr`, sets it to file lump or to -1
//
//==========================================================================
VStream *VDirPakFile::OpenFileRead (VStr fname, int *plump) {
  int lump = CheckNumForFileName(fname);
  if (plump) *plump = lump;
  if (lump < 0) return nullptr;
  VStr tmpname = PakFileName.appendPath(pakdir.files[lump].diskName);
  VStream *strm = CreateDiskStreamRead(tmpname, pakdir.files[lump].diskName);
  // update file size
  if (strm && pakdir.files[lump].filesize < 0) {
    pakdir.files[lump].filesize = strm->TotalSize();
    strm->Seek(0);
  }
  return strm;
}


//==========================================================================
//
//  VDirPakFile::CreateLumpReaderNum
//
//==========================================================================
VStream *VDirPakFile::CreateLumpReaderNum (int lump) {
  vassert(lump >= 0);
  vassert(lump < pakdir.files.length());
  VStr tmpname = PakFileName.appendPath(pakdir.files[lump].diskName);
  VStream *strm = CreateDiskStreamRead(tmpname, pakdir.files[lump].diskName);
  // update file size
  if (strm && pakdir.files[lump].filesize < 0) {
    pakdir.files[lump].filesize = strm->TotalSize();
    strm->Seek(0);
  }
  return strm;
}
