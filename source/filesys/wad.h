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
//**
//**  WAD I/O functions.
//**
//**************************************************************************
// k8: use .gwa supplemental wads?
// those are remnants of the early times, and not really used these days
// you still can put PVS there, but PVS seems to be totally unused
// these days too; so i opted to exclude GWA support
//#define VAVOOM_USE_GWA


// boom namespaces
enum EWadNamespace {
  WADNS_Global,
  WADNS_Sprites,
  WADNS_Flats,
  WADNS_ColourMaps,
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
};


void W_AddFile (const VStr &FileName, bool FixVoices, const VStr &GwaDir=VStr());
void W_Shutdown ();

enum WAuxFileType {
  Wad,
  Zip,
  Pk3,
};

// returns lump handle
int W_StartAuxiliary (); // returns first aux index
int W_OpenAuxiliary (const VStr &FileName); // -1: not found
//int W_AddAuxiliary (const VStr &FileName); // -1: not found
int W_AddAuxiliaryStream (VStream *strm, WAuxFileType ftype); // -1: error/not found; otherwise handle of the first appended file
void W_CloseAuxiliary (); // close all aux files

int W_CheckNumForName (VName Name, EWadNamespace NS = WADNS_Global);
int W_GetNumForName (VName Name, EWadNamespace NS = WADNS_Global);
int W_CheckNumForNameInFile (VName Name, int File, EWadNamespace NS = WADNS_Global);

int W_CheckNumForFileName (const VStr &Name);
int W_CheckNumForFileNameInSameFile (int filelump, const VStr &Name);
int W_CheckNumForFileNameInSameFileOrLower (int filelump, const VStr &Name);
int W_CheckNumForTextureFileName (const VStr &Name);
int W_GetNumForFileName (const VStr &Name);
int W_FindLumpByFileNameWithExts (const VStr &BaseName, const char **Exts);

int W_LumpLength (int lump);
VName W_LumpName (int lump);
VStr W_FullLumpName (int lump);
int W_LumpFile (int lump);

// this is used to resolve animated ranges
// returns handle or -1
int W_FindFirstLumpOccurence (VName lmpname, EWadNamespace NS);


void W_ReadFromLump (int lump, void *dest, int pos, int size);
VStr W_LoadTextLump (VName name);
void W_LoadLumpIntoArray (VName Lump, TArray<vuint8>& Array);
VStream *W_CreateLumpReaderNum (int lump);
VStream *W_CreateLumpReaderName (VName Name, EWadNamespace NS = WADNS_Global);

int W_IterateNS (int Prev, EWadNamespace NS);
int W_IterateFile (int Prev, const VStr &Name);

int W_NextMountFileId ();
VStr W_FindMapInLastFile (int fileid, int *mapnum);
VStr W_FindMapInAuxuliaries (int *mapnum);
