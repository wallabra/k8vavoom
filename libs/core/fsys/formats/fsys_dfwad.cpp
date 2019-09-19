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


//==========================================================================
//
//  VDFWadFile::VDFWadFile
//
//==========================================================================
VDFWadFile::VDFWadFile (VStr fname)
  : VPakFileBase(fname, true)
{
  mythread_mutex_init(&rdlock);
  if (fsys_report_added_paks) GLog.Logf(NAME_Init, "Adding \"%s\"...", *PakFileName);
  auto fstream = FL_OpenSysFileRead(PakFileName);
  vassert(fstream);
  OpenArchive(fstream);
}


//==========================================================================
//
//  VDFWadFile::VDFWadFile
//
//  takes ownership
//
//==========================================================================
VDFWadFile::VDFWadFile (VStream *fstream)
  : VPakFileBase("<memory>", true)
{
  mythread_mutex_init(&rdlock);
  if (fstream->GetName().length()) PakFileName = fstream->GetName();
  OpenArchive(fstream);
}


//==========================================================================
//
//  VDFWadFile::VDFWadFile
//
//==========================================================================
VDFWadFile::VDFWadFile (VStream *fstream, VStr fname)
  : VPakFileBase(fname, true)
{
  mythread_mutex_init(&rdlock);
  vassert(fstream);
  OpenArchive(fstream);
}


//==========================================================================
//
//  VDFWadFile::~VDFWadFile
//
//==========================================================================
VDFWadFile::~VDFWadFile () {
  Close();
  mythread_mutex_destroy(&rdlock);
}


//==========================================================================
//
//  VDFWadFile::VDFWadFile
//
//==========================================================================
void VDFWadFile::OpenArchive (VStream *fstream) {
  Stream = fstream;
  vassert(Stream);

  char sign[6];
  memset(sign, 0, 6);
  Stream->Serialise(sign, 6);
  if (memcmp(sign, "DFWAD\x01", 6) != 0) Sys_Error("not a DFWAD file \"%s\"", *PakFileName);

  vuint16 count;
  *Stream << count;
  if (Stream->IsError()) Sys_Error("error reading DFWAD file \"%s\"", *PakFileName);

  VStr curpath = VStr();
  char namebuf[17];
  while (count--) {
    //files[i].name = VStr();
    //memset(namebuf, 0, sizeof(namebuf));
    Stream->Serialize(namebuf, 16);
    for (int f = 0; f < 16; ++f) {
      char ch = namebuf[f];
      if (!ch) break;
      if (ch == '/') ch = '_';
      namebuf[f] = VStr::locase1251(ch);
    }
    namebuf[16] = 0;

    vuint32 pkofs, pksize;
    *Stream << pkofs << pksize;

    if (!namebuf[0]) continue; // ignore empty names

    // directory?
    if (pkofs == 0 && pksize == 0) {
      curpath = VStr(namebuf);
      if (curpath.length() && !curpath.endsWith("/")) curpath += "/";
      continue;
    }

    VPakFileInfo fi;
    fi.fileName = curpath+VStr(namebuf);
    fi.pakdataofs = pkofs;
    fi.packedsize = pksize;
    fi.filesize = -1;
    pakdir.append(fi);
  }

  pakdir.buildLumpNames();
  pakdir.buildNameMaps();
}


//==========================================================================
//
//  VDFWadFile::Close
//
//==========================================================================
void VDFWadFile::Close () {
  VPakFileBase::Close();
  if (Stream) { delete Stream; Stream = nullptr; }
}


//==========================================================================
//
//  VDFWadFile::CreateLumpReaderNum
//
//==========================================================================
VStream *VDFWadFile::CreateLumpReaderNum (int Lump) {
  vassert(Lump >= 0);
  vassert(Lump < pakdir.files.length());
  const VPakFileInfo &fi = pakdir.files[Lump];
  // this is mt-protected
  VStream *S = new VPartialStreamRO(GetPrefix()+":"+fi.fileName, Stream, fi.pakdataofs, fi.filesize, &rdlock);
  return S;
}
