//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2018 Ketmar Dark
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
#include "../fsys.h"


// /////////////////////////////////////////////////////////////////////////// /
class VDFWadFile : public FSysDriverBase {
private:
  struct FileInfo {
    VStr name;
    vuint32 pksize;
    vuint32 pkofs;
  };

private:
  VStream *fileStream; // source stream
  FileInfo *files;
  vint32 fileCount; // total number of files

private:
  bool openArchive ();

protected:
  virtual const VStr &getNameByIndex (int idx) const override;
  virtual int getNameCount () const override;
  // should return `nullptr` on failure
  virtual VStream *open (int idx) override;

public:
  VDFWadFile (VStream* fstream, const VStr &aname=VStr("<memory>")); // takes ownership on success
  virtual ~VDFWadFile() override;

  inline bool isOpened () const { return (fileStream != nullptr); }
};


// /////////////////////////////////////////////////////////////////////////// /
// takes ownership
VDFWadFile::VDFWadFile (VStream *fstream, const VStr &aname)
  : fileStream(nullptr)
  , files(nullptr)
  , fileCount(0)
{
  if (fstream) {
    fileStream = fstream;
         if (!openArchive()) fileStream = nullptr;
    else if (fstream->IsError()) fileStream = nullptr;
  }
}


VDFWadFile::~VDFWadFile () {
  delete[] files;
  delete fileStream;
}


const VStr &VDFWadFile::getNameByIndex (int idx) const {
  if (idx < 0 || idx >= fileCount) return VStr::EmptyString;
  return files[idx].name;
}


int VDFWadFile::getNameCount () const {
  return fileCount;
}


bool VDFWadFile::openArchive () {
  char sign[6];
  fileStream->Serialize(sign, 6);
  if (memcmp(sign, "DFWAD\x01", 6) != 0) return false;

  vuint16 count;
  *fileStream << count;
  if (fileStream->IsError()) return false;
  fileCount = count;
  if (count == 0) return true;

  files = new FileInfo[fileCount];

  VStr curpath = VStr();

  for (int i = 0; i < fileCount; ++i) {
    files[i].name = VStr();

    char nbuf[17];
    fileStream->Serialize(nbuf, 16);
    for (int f = 0; f < 16; ++f) if (nbuf[f] == '/') nbuf[f] = '_';
    nbuf[16] = 0;

    vuint32 pkofs, pksize;
    *fileStream << pkofs << pksize;

    if (pkofs == 0 && pksize == 0) {
      curpath = VStr(nbuf).toLowerCase1251();
      if (curpath.length() && !curpath.endsWith("/")) curpath += "/";
      continue;
    }

    files[i].name = curpath+VStr(nbuf).toLowerCase1251(); // 1251
    files[i].pksize = pksize;
    files[i].pkofs = pkofs;
    //fprintf(stderr, "DFWAD: <%s>\n", *files[i].name);
  }

  // remove empty names
  {
    int spos = 0, dpos = 0;
    while (spos < fileCount) {
      if (files[spos].name.length() == 0) {
        ++spos;
      } else {
        if (spos != dpos) {
          files[dpos] = files[spos];
        }
        ++spos;
        ++dpos;
      }
    }
    for (int f = dpos; f < fileCount; ++f) files[f].name.clear();
    fileCount = dpos;
  }

  buildNameHashTable();

  return true;
}


VStream *VDFWadFile::open (int idx) {
  if (idx < 0 || idx >= fileCount) return nullptr;
  fileStream->Seek(files[idx].pkofs);
  return new VZipStreamReader(fileStream, files[idx].pksize);
}


// ////////////////////////////////////////////////////////////////////////// //
static FSysDriverBase *dfwadLoader (VStream *strm) {
  if (!strm) return nullptr;
  //fprintf(stderr, "trying <%s> as dfwad...\n", *strm->GetName());
  auto res = new VDFWadFile(strm);
  if (!res->isOpened()) { delete res; res = nullptr; }
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
class DFWadVFSRegistrator {
public:
  DFWadVFSRegistrator (int n) {
    FSysRegisterDriver(&dfwadLoader);
    //fprintf(stderr, "FSYS: added DFWAD reader.\n");
  }
};


static DFWadVFSRegistrator ldr(666);
