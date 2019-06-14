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
#ifndef FSYS_HEADER
#define FSYS_HEADER

#include "../../core.h"

VStream *FL_OpenFileRead (const VStr &Name);
// set `isFullName` to `true` to prevent adding anything to file name
VStream *FL_OpenFileWrite (const VStr &Name);

VStream *FL_OpenSysFileRead (const VStr &Name);
VStream *FL_OpenSysFileWrite (const VStr &Name);


// boom namespaces
enum EWadNamespace {
  WADNS_Global,
  WADNS_Sprites,
  WADNS_Flats,
  WADNS_ColorMaps,
  WADNS_ACSLibrary,
  WADNS_NewTextures,
  WADNS_Voices,
  WADNS_HiResTextures,

  // special namespaces for zip files, in wad file they will be searched in global namespace
  WADNS_ZipSpecial,
  WADNS_Patches,
  WADNS_Graphics,
  WADNS_Sounds,
  WADNS_Music,

  WADNS_Any,
};


// returns first lump id or -1
int W_AddFileStream (const VStr &FileName, VStream *fstrm);
// returns first lump id or -1
int W_AddFile (const VStr &FileName);


int W_LumpLength (int lump);
VName W_LumpName (int lump);
VStr W_FullLumpName (int lump);
VStr W_FullPakNameForLump (int lump);
VStr W_FullPakNameByFile (int fidx); // pass result of `W_LumpFile()`
int W_LumpFile (int lump);
EWadNamespace W_LumpNS (int lump);

void W_ReadFromLump (int lump, void *dest, int pos, int size);
void W_LoadLumpIntoArray (int lump, TArray<vuint8>& Array);

int W_StartIterationFromLumpFile (int File);
int W_IterateNS (int Prev, EWadNamespace NS);
int W_IterateFile (int Prev, const VStr &Name);


#endif
