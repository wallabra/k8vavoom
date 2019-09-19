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
VDFWadFile::VDFWadFile (VStream *fstream, VStr fname)
  : VPakFileBase(fname)
{
  vassert(fstream);
  OpenArchive(fstream);
}


//==========================================================================
//
//  VDFWadFile::VDFWadFile
//
//==========================================================================
void VDFWadFile::OpenArchive (VStream *fstream) {
  archStream = fstream;
  vassert(archStream);

  char sign[6];
  memset(sign, 0, 6);
  archStream->Serialise(sign, 6);
  if (memcmp(sign, "DFWAD\x01", 6) != 0) Sys_Error("not a DFWAD file \"%s\"", *PakFileName);

  vuint16 count;
  *archStream << count;
  if (archStream->IsError()) Sys_Error("error reading DFWAD file \"%s\"", *PakFileName);

  VStr curpath = VStr();
  char namebuf[17];
  while (count--) {
    //files[i].name = VStr();
    //memset(namebuf, 0, sizeof(namebuf));
    archStream->Serialize(namebuf, 16);
    for (int f = 0; f < 16; ++f) {
      char ch = namebuf[f];
      if (!ch) break;
      if (ch == '/') ch = '_';
      namebuf[f] = VStr::locase1251(ch);
    }
    namebuf[16] = 0;

    vuint32 pkofs, pksize;
    *archStream << pkofs << pksize;

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
//  VDFWadFile::CreateLumpReaderNum
//
//==========================================================================
VStream *VDFWadFile::CreateLumpReaderNum (int Lump) {
  vassert(Lump >= 0);
  vassert(Lump < pakdir.files.length());
  const VPakFileInfo &fi = pakdir.files[Lump];
  // this is mt-protected
  VStream *S = new VPartialStreamRO(GetPrefix()+":"+fi.fileName, archStream, fi.pakdataofs, fi.filesize, &rdlock);
  return S;
}
