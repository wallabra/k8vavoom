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
//  VQuakePakFile::VQuakePakFile
//
//  takes ownership
//
//==========================================================================
VQuakePakFile::VQuakePakFile (VStream *fstream, VStr name, int signtype)
  : VPakFileBase(name)
{
  OpenArchive(fstream, signtype);
  if (fstream->IsError()) Sys_Error("error opening archive \"%s\"", *PakFileName);
}


//==========================================================================
//
//  VQuakePakFile::VQuakePakFile
//
//==========================================================================
void VQuakePakFile::OpenArchive (VStream *fstream, int signtype) {
  archStream = fstream;
  vassert(archStream);

  bool isSinPack = false;

  //archStream->Seek(0);
  if (!signtype) {
    char sign[4];
    memset(sign, 0, 4);
    archStream->Serialise(sign, 4);
         if (memcmp(sign, "PACK", 4) == 0) isSinPack = false;
    else if (memcmp(sign, "SPAK", 4) == 0) isSinPack = true;
    else Sys_Error("not a quake pak file \"%s\"", *PakFileName);
  } else {
    isSinPack = (signtype > 1);
  }

  vuint32 dirofs;
  vuint32 dirsize;

  *archStream << dirofs << dirsize;

  if (!isSinPack) dirsize /= 64;

  char namebuf[121];
  vuint32 ofs, size;

  archStream->Seek(dirofs);
  if (archStream->IsError()) Sys_Error("cannot read quake pak file \"%s\"", *PakFileName);

  while (dirsize > 0) {
    --dirsize;
    memset(namebuf, 0, sizeof(namebuf));
    if (!isSinPack) {
      archStream->Serialise(namebuf, 56);
    } else {
      archStream->Serialise(namebuf, 120);
    }
    *archStream << ofs << size;
    if (archStream->IsError()) Sys_Error("cannot read quake pak file \"%s\"", *PakFileName);

    VStr zfname = VStr(namebuf).ToLower().FixFileSlashes();

    // fix some idiocity introduced by some shitdoze doom tools
    for (;;) {
           if (zfname.startsWith("./")) zfname.chopLeft(2);
      else if (zfname.startsWith("../")) zfname.chopLeft(3);
      else if (zfname.startsWith("/")) zfname.chopLeft(1);
      else break;
    }
    if (zfname.length() == 0 || zfname.endsWith("/")) continue; // something strange

    VPakFileInfo fi;
    fi.fileName = zfname;
    fi.pakdataofs = ofs;
    fi.filesize = size;
    pakdir.append(fi);
  }

  pakdir.buildLumpNames();
  pakdir.buildNameMaps();
}


//==========================================================================
//
//  VQuakePakFile::CreateLumpReaderNum
//
//==========================================================================
VStream *VQuakePakFile::CreateLumpReaderNum (int Lump) {
  vassert(Lump >= 0);
  vassert(Lump < pakdir.files.length());
  const VPakFileInfo &fi = pakdir.files[Lump];
  // this is mt-protected
  VStream *S = new VPartialStreamRO(GetPrefix()+":"+fi.fileName, archStream, fi.pakdataofs, fi.filesize, &rdlock);
  return S;
}
