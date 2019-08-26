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
#ifndef VAVOOM_FSYS_H
#define VAVOOM_FSYS_H

#ifndef VAVOOM_CORE_HEADER
# include "../core.h"
#endif

// k8: about .gwa supplemental wads.
// those are remnants of the early times, and not really used these days.
// you still can put PVS there, but PVS seems to be totally unused
// these days too; so i opted to exclude GWA support.
// i had a define to turn GWA support on, but now i removed it completely.

void FSYS_Shutdown ();

bool FL_FileExists (VStr fname);
void FL_CreatePath (VStr Path);

VStream *FL_OpenFileRead (VStr Name);

VStream *FL_OpenSysFileRead (VStr Name);
VStream *FL_OpenSysFileWrite (VStr Name);

// search file only in "basepaks"
VStream *FL_OpenFileReadBaseOnly (VStr Name);

// reject absolute names, names with ".", and names with ".."
bool FL_IsSafeDiskFileName (VStr fname);


extern bool fsys_developer_debug;
extern bool fsys_IgnoreZScript;
extern bool fsys_DisableBDW;

// used for game detection
extern bool fsys_EnableAuxSearch;

extern bool fsys_skipSounds;
extern bool fsys_skipSprites;
extern bool fsys_skipDehacked;

extern TArray<VStr> fsys_game_filters;


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

  // all
  WADNS_Any,
};


// for non-wad files (zip, pk3, pak), it adds add subarchives from a root directory
void W_AddDiskFile (VStr FileName, bool FixVoices=false);
// returns `true` if file was added
bool W_AddDiskFileOptional (VStr FileName, bool FixVoices=false);
// this mounts disk directory as PK3 archive
void W_MountDiskDir (VStr dirname);
void W_Shutdown ();

enum WAuxFileType {
  VFS_Wad, // caller is 100% sure that this is IWAD/PWAD
  VFS_Zip, // guess it, and allow nested files
  VFS_Archive, // guess it, but don't allow nested files
};


// returns lump handle
int W_StartAuxiliary (); // returns first aux index
int W_OpenAuxiliary (VStr FileName); // -1: not found
//int W_AddAuxiliary (VStr FileName); // -1: not found
int W_AddAuxiliaryStream (VStream *strm, WAuxFileType ftype); // -1: error/not found; otherwise handle of the first appended file
void W_CloseAuxiliary (); // close all aux files

int W_CheckNumForName (VName Name, EWadNamespace NS = WADNS_Global);
int W_GetNumForName (VName Name, EWadNamespace NS = WADNS_Global);
int W_CheckNumForNameInFile (VName Name, int File, EWadNamespace NS = WADNS_Global);
int W_CheckFirstNumForNameInFile (VName Name, int File, EWadNamespace NS = WADNS_Global);
int W_FindACSObjectInFile (VStr Name, int File);

int W_CheckNumForFileName (VStr Name);
int W_CheckNumForFileNameInSameFile (int filelump, VStr Name);
int W_CheckNumForFileNameInSameFileOrLower (int filelump, VStr Name);
int W_CheckNumForTextureFileName (VStr Name);
int W_GetNumForFileName (VStr Name);
int W_FindLumpByFileNameWithExts (VStr BaseName, const char **Exts);

int W_LumpLength (int lump);
VName W_LumpName (int lump);
VStr W_FullLumpName (int lump);
VStr W_RealLumpName (int lump); // without pak prefix
VStr W_FullPakNameForLump (int lump);
VStr W_FullPakNameByFile (int fidx); // pass result of `W_LumpFile()`
int W_LumpFile (int lump);
bool W_IsAuxLump (int lump); // -1 is not aux ;-)

// this is used to resolve animated ranges
// returns handle or -1
int W_FindFirstLumpOccurence (VName lmpname, EWadNamespace NS);

bool W_IsIWADFile (int file);
bool W_IsWADFile (int file); // not pk3, not disk

bool W_IsIWADLump (int lump);
bool W_IsWADLump (int lump); // not pk3, not disk

void W_ReadFromLump (int lump, void *dest, int pos, int size);
VStr W_LoadTextLump (VName name);
void W_LoadLumpIntoArray (VName Lump, TArray<vuint8>& Array);
void W_LoadLumpIntoArrayIdx (int Lump, TArray<vuint8>& Array);
VStream *W_CreateLumpReaderNum (int lump);
VStream *W_CreateLumpReaderName (VName Name, EWadNamespace NS = WADNS_Global);

int W_StartIterationFromLumpFileNS (int File, EWadNamespace NS); // returns -1 if not found
int W_IterateNS (int Prev, EWadNamespace NS);
int W_IterateFile (int Prev, VStr Name);

int W_NextMountFileId ();
VStr W_FindMapInLastFile (int fileid, int *mapnum);
VStr W_FindMapInAuxuliaries (int *mapnum);


// ////////////////////////////////////////////////////////////////////////// //
struct WadNSIterator {
public:
  int lump; // current lump
  EWadNamespace ns;

public:
  WadNSIterator () : lump(-1), ns(WADNS_Global) {}
  WadNSIterator (EWadNamespace aNS) : lump(-1), ns(aNS) { lump = W_IterateNS(-1, aNS); }
  WadNSIterator (const WadNSIterator &it) : lump(it.lump), ns(it.ns) {}
  WadNSIterator (const WadNSIterator &it, bool asEnd) : lump(-1), ns(it.ns) {}
  inline WadNSIterator &operator = (const WadNSIterator &it) { lump = it.lump; ns = it.ns; return *this; }

  static inline WadNSIterator FromWadFile (int aFile, EWadNamespace aNS) {
    WadNSIterator it;
    it.ns = aNS;
    it.lump = W_StartIterationFromLumpFileNS(aFile, aNS);
    return it;
  }

  inline WadNSIterator begin () { return WadNSIterator(ns); }
  inline WadNSIterator end () { return WadNSIterator(*this, true); }
  inline bool operator == (const WadNSIterator &b) const { return (lump == b.lump && ns == b.ns); }
  inline bool operator != (const WadNSIterator &b) const { return (lump != b.lump || ns != b.ns); }
  inline WadNSIterator operator * () const { return WadNSIterator(*this); } /* required for iterator */
  inline void operator ++ () { lump = (lump >= 0 ? W_IterateNS(lump, ns) : -1); } /* this is enough for iterator */

  inline bool isEmpty () const { return (lump < 0); }

  inline int getFile () const { return (lump >= 0 ? W_LumpFile(lump) : -1); }
  inline int getLength () const { return (lump >= 0 ? W_LumpLength(lump) : -1); }
  inline int getSize () const { return (lump >= 0 ? W_LumpLength(lump) : -1); }
  inline VName getName () const { return (lump >= 0 ? W_LumpName(lump) : NAME_None); }
  inline VStr getFullName () const { return (lump >= 0 ? W_FullLumpName(lump) : VStr::EmptyString); }
  inline VStr getRealName () const { return (lump >= 0 ? W_RealLumpName(lump) : VStr::EmptyString); } // without pak prefix
  inline VStr getFullPakName () const { return (lump >= 0 ? W_FullPakNameForLump(lump) : VStr::EmptyString); } // without pak prefix

  inline bool isAux () const { return (lump >= 0 ? W_IsAuxLump(lump) : false); }
  inline bool isIWAD () const { return (lump >= 0 ? W_IsIWADLump(lump) : false); }
  inline bool isIWad () const { return (lump >= 0 ? W_IsIWADLump(lump) : false); }
};


// ////////////////////////////////////////////////////////////////////////// //
struct WadNSNameIterator {
public:
  int lump; // current lump
  EWadNamespace ns;
  VName lumpname;

private:
  // all fields are set
  inline void startIteration () {
    if (lumpname != NAME_None) {
      if (lump < 0) lump = W_IterateNS(-1, ns);
      for (; lump >= 0; lump = W_IterateNS(lump, ns)) {
        if (W_LumpName(lump) == lumpname) break;
      }
    } else {
      lump = -1;
    }
  }

  inline void nextStep () {
    if (lump >= 0) {
      for (lump = W_IterateNS(lump, ns); lump >= 0; lump = W_IterateNS(lump, ns)) {
        if (W_LumpName(lump) == lumpname) break;
      }
    }
  }

public:
  WadNSNameIterator () : lump(-1), ns(WADNS_Global), lumpname(NAME_None) {}
  WadNSNameIterator (VName aname, EWadNamespace aNS) : lump(-1), ns(aNS), lumpname(aname) { if (aname != NAME_None) lumpname = VName(*aname, VName::FindLower); startIteration(); }
  WadNSNameIterator (const char *aname, EWadNamespace aNS) : lump(-1), ns(aNS), lumpname(NAME_None) { lumpname = VName(aname, VName::FindLower); startIteration(); }
  WadNSNameIterator (VStr &str, EWadNamespace aNS) : lump(-1), ns(aNS), lumpname(NAME_None) { lumpname = VName(*str, VName::FindLower); startIteration(); }
  WadNSNameIterator (const WadNSNameIterator &it) : lump(it.lump), ns(it.ns), lumpname(it.lumpname) {}
  WadNSNameIterator (const WadNSNameIterator &it, bool asEnd) : lump(-1), ns(it.ns), lumpname(it.lumpname) {}
  inline WadNSNameIterator &operator = (const WadNSNameIterator &it) { lump = it.lump; ns = it.ns; lumpname = it.lumpname; return *this; }

  static inline WadNSNameIterator FromWadFile (const char *aname, int aFile, EWadNamespace aNS) {
    WadNSNameIterator it;
    it.ns = aNS;
    it.lump = W_StartIterationFromLumpFileNS(aFile, aNS);
    it.lumpname = (aname && aname[0] ? VName(aname, VName::FindLower) : NAME_None);
    it.startIteration();
    return it;
  }

  static inline WadNSNameIterator FromWadFile (VName aname, int aFile, EWadNamespace aNS) {
    return FromWadFile((aname != NAME_None ? *aname : nullptr), aFile, aNS);
  }

  static inline WadNSNameIterator FromWadFile (VStr aname, int aFile, EWadNamespace aNS) {
    return FromWadFile(*aname, aFile, aNS);
  }

  inline WadNSNameIterator begin () { return WadNSNameIterator(lumpname, ns); }
  inline WadNSNameIterator end () { return WadNSNameIterator(*this, true); }
  inline bool operator == (const WadNSNameIterator &b) const { return (lump == b.lump && ns == b.ns && lumpname == b.lumpname); }
  inline bool operator != (const WadNSNameIterator &b) const { return (lump != b.lump || ns != b.ns || lumpname != b.lumpname); }
  inline WadNSNameIterator operator * () const { return WadNSNameIterator(*this); } /* required for iterator */
  inline void operator ++ () { nextStep(); } /* this is enough for iterator */

  inline bool isEmpty () const { return (lump < 0); }

  inline int getFile () const { return (lump >= 0 ? W_LumpFile(lump) : -1); }
  inline int getLength () const { return (lump >= 0 ? W_LumpLength(lump) : -1); }
  inline int getSize () const { return (lump >= 0 ? W_LumpLength(lump) : -1); }
  inline VName getName () const { return (lump >= 0 ? W_LumpName(lump) : NAME_None); }
  inline VStr getFullName () const { return (lump >= 0 ? W_FullLumpName(lump) : VStr::EmptyString); }
  inline VStr getRealName () const { return (lump >= 0 ? W_RealLumpName(lump) : VStr::EmptyString); } // without pak prefix
  inline VStr getFullPakName () const { return (lump >= 0 ? W_FullPakNameForLump(lump) : VStr::EmptyString); } // without pak prefix

  inline bool isAux () const { return (lump >= 0 ? W_IsAuxLump(lump) : false); }
  inline bool isIWAD () const { return (lump >= 0 ? W_IsIWADLump(lump) : false); }
  inline bool isIWad () const { return (lump >= 0 ? W_IsIWADLump(lump) : false); }
};


// ////////////////////////////////////////////////////////////////////////// //
struct WadFileIterator {
public:
  int lump; // current lump
  VStr fname;

public:
  WadFileIterator (VStr afname) : lump(-1), fname(afname) { lump = W_IterateFile(-1, afname); }
  WadFileIterator (const WadFileIterator &it) : lump(it.lump), fname(it.fname) {}
  WadFileIterator (const WadFileIterator &it, bool asEnd) : lump(-1), fname(it.fname) {}
  inline WadFileIterator &operator = (const WadFileIterator &it) { lump = it.lump; fname = it.fname; return *this; }

  inline WadFileIterator begin () { return WadFileIterator(fname); }
  inline WadFileIterator end () { return WadFileIterator(*this, true); }
  inline bool operator == (const WadFileIterator &b) const { return (lump == b.lump); }
  inline bool operator != (const WadFileIterator &b) const { return (lump != b.lump); }
  inline WadFileIterator operator * () const { return WadFileIterator(*this); } /* required for iterator */
  inline void operator ++ () { lump = (lump >= 0 ? W_IterateFile(-1, fname) : -1); } /* this is enough for iterator */

  inline bool isEmpty () const { return (lump < 0); }

  inline int getFile () const { return (lump >= 0 ? W_LumpFile(lump) : -1); }
  inline int getLength () const { return (lump >= 0 ? W_LumpLength(lump) : -1); }
  inline int getSize () const { return (lump >= 0 ? W_LumpLength(lump) : -1); }
  inline VName getName () const { return (lump >= 0 ? W_LumpName(lump) : NAME_None); }
  inline VStr getFullName () const { return (lump >= 0 ? W_FullLumpName(lump) : VStr::EmptyString); }
  inline VStr getRealName () const { return (lump >= 0 ? W_RealLumpName(lump) : VStr::EmptyString); } // without pak prefix
  inline VStr getFullPakName () const { return (lump >= 0 ? W_FullPakNameForLump(lump) : VStr::EmptyString); } // without pak prefix

  inline bool isAux () const { return (lump >= 0 ? W_IsAuxLump(lump) : false); }
  inline bool isIWAD () const { return (lump >= 0 ? W_IsIWADLump(lump) : false); }
  inline bool isIWad () const { return (lump >= 0 ? W_IsIWADLump(lump) : false); }
};


#endif
