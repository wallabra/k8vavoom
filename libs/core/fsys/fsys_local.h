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
#ifndef VAVOOM_FSYS_LOCAL_HEADER
#define VAVOOM_FSYS_LOCAL_HEADER

#include "fsys.h"


//==========================================================================
//  VSpriteRename
//==========================================================================
struct VSpriteRename {
  char Old[4];
  char New[4];
};

struct VLumpRename {
  VName Old;
  VName New;
};

struct VPK3ResDirInfo {
  const char *pfx;
  EWadNamespace wadns;
};


//==========================================================================
//  VSearchPath
//==========================================================================
class VSearchPath {
public:
  bool iwad;
  bool basepak;
  bool normalwad;

public:
  VSearchPath ();
  virtual ~VSearchPath ();

  // all following methods are supposed to be called with global mutex protection
  // (i.e. they should not be called from multiple threads simultaneously)
  virtual bool FileExists (VStr Name) = 0;
  virtual VStream *OpenFileRead (VStr Name) = 0;
  virtual void Close () = 0;
  virtual int CheckNumForName (VName LumpName, EWadNamespace InNS, bool wantFirst=true) = 0;
  virtual int CheckNumForFileName (VStr Name) = 0;
  virtual int FindACSObject (VStr fname) = 0;
  virtual void ReadFromLump (int LumpNum, void *Dest, int Pos, int Size) = 0;
  virtual int LumpLength (int LumpNum) = 0;
  virtual VName LumpName (int LumpNum) = 0;
  virtual VStr LumpFileName (int LumpNum) = 0;
  virtual int IterateNS (int Start, EWadNamespace NS, bool allowEmptyName8=false) = 0;
  virtual VStream *CreateLumpReaderNum (int LumpNum) = 0;
  virtual void RenameSprites (const TArray<VSpriteRename> &A, const TArray<VLumpRename> &LA) = 0;
  virtual VStr GetPrefix () = 0; // for logging

  virtual void ListWadFiles (TArray<VStr> &list);
  virtual void ListPk3Files (TArray<VStr> &list);
};


//==========================================================================
//  VFilesDir
//==========================================================================
class VFilesDir : public VSearchPath {
private:
  VStr path;
  TArray<VStr> cachedFiles;
  TMap<VStr, int> cachedMap;
  //bool cacheInited;

private:
  //void cacheDir ();

  // -1, or index in `CachedFiles`
  int findFileCI (VStr fname);

public:
  VFilesDir (VStr aPath);
  virtual ~VFilesDir () override;
  virtual bool FileExists (VStr) override;
  virtual VStream *OpenFileRead (VStr) override;
  virtual VStream *CreateLumpReaderNum (int) override;
  virtual void Close () override;
  virtual int CheckNumForName (VName LumpName, EWadNamespace InNS, bool wantFirst=true) override;
  virtual int CheckNumForFileName (VStr) override;
  virtual void ReadFromLump (int, void*, int, int) override;
  virtual int LumpLength (int) override;
  virtual VName LumpName (int) override;
  virtual VStr LumpFileName (int) override;
  virtual int IterateNS (int, EWadNamespace, bool allowEmptyName8=false) override;
  virtual void RenameSprites (const TArray<VSpriteRename>&, const TArray<VLumpRename>&) override;
  virtual VStr GetPrefix () override { return path; }
};


//==========================================================================
//  VPakFileInfo
//==========================================================================
// information about a file in the zipfile
// also used for "pakdir", and "wad"
struct VPakFileInfo {
  VStr fileName; // name of the file (i.e. full name, lowercased)
  VName lumpName;
  /*EWadNamespace*/int lumpNamespace;
  int nextLump; // next lump with the same name
  // zip info
  vuint16 flag; // general purpose bit flag
  vuint16 compression; // compression method
  vuint32 crc32; // crc-32
  vuint32 packedsize; // compressed size (if compression is not supported, then 0)
  vint32 filesize; // uncompressed size
  // for zips
  vuint16 filenamesize; // filename length
  vuint32 pakdataofs; // relative offset of local header
  // for dirpaks
  VStr diskName;

  VPakFileInfo ()
    : fileName()
    , lumpName(NAME_None)
    , lumpNamespace(-1)
    , nextLump(-1)
    , flag(0)
    , compression(0)
    , crc32(0)
    , packedsize(0)
    , filesize(0)
    , filenamesize(0)
    , pakdataofs(0)
    , diskName()
  {
  }
};


// ////////////////////////////////////////////////////////////////////////// //
class VPakFileBase;

struct VFileDirectory {
public:
  VPakFileBase *owner;
  TArray<VPakFileInfo> files;
  TMap<VName, int> lumpmap; // maps lump names to file entries; names are lowercased
  TMap<VStr, int> filemap; // maps names (with pathes) to file entries; names are lowercased
  bool aszip;

public:
  VFileDirectory ();
  VFileDirectory (VPakFileBase *aowner, bool aaszip=false);
  ~VFileDirectory ();

  static void normalizeFileName (VStr &fname);
  static VName normalizeLumpName (VName lname);

  const VStr getArchiveName () const;

  void clear ();

  void append (const VPakFileInfo &fi);

  int appendAndRegister (const VPakFileInfo &fi);

  // won't touch entries with `lumpName != NAME_None`
  void buildLumpNames ();

  // call this when all lump names are built
  void buildNameMaps (bool rebuilding=false); // `true` to suppress warnings

  bool fileExists (VStr name);
  bool lumpExists (VName lname, vint32 ns); // namespace -1 means "any"

  int findFile (VStr fname);

  // namespace -1 means "any"
  int findFirstLump (VName lname, vint32 ns);
  int findLastLump (VName lname, vint32 ns);

  int nextLump (vint32 curridx, vint32 ns, bool allowEmptyName8=false);
};


//==========================================================================
//
//  VPakFileBase
//
//  this is base class for various archive readers
//  it implements file list, and sprite renaming functionality
//
//==========================================================================
class VPakFileBase : public VSearchPath {
public:
  VStr PakFileName; // never ends with slash
  VFileDirectory pakdir;

public:
  VPakFileBase (VStr apakfilename, bool aaszip=false);
  virtual ~VPakFileBase () override;

  //inline bool hasFiles () const { return (pakdir.files.length() > 0); }

  virtual void Close () override;

  virtual bool FileExists (VStr fname) override;
  virtual VStream *OpenFileRead (VStr fname) override;
  virtual int CheckNumForName (VName LumpName, EWadNamespace InNS, bool wantFirst=true) override;
  virtual int CheckNumForFileName (VStr fname) override;
  virtual int FindACSObject (VStr fname) override;
  virtual void ReadFromLump (int, void *, int, int) override;
  virtual int LumpLength (int) override;
  virtual VName LumpName (int) override;
  virtual VStr LumpFileName (int) override;
  virtual int IterateNS (int, EWadNamespace, bool allowEmptyName8=false) override;
  //virtual VStream *CreateLumpReaderNum (int) override; // override this!
  virtual void RenameSprites (const TArray<VSpriteRename> &, const TArray<VLumpRename> &) override; // override this if you don't want any renaming

  virtual void ListWadFiles (TArray<VStr> &list) override;
  virtual void ListPk3Files (TArray<VStr> &list) override;

  VStr CalculateMD5 (int lumpidx);

  virtual VStr GetPrefix () override;
};


//==========================================================================
//  VWadFile
//==========================================================================
class VWadFile : public VPakFileBase {
private:
  mythread_mutex rdlock;
  VStream *Stream;
  bool lockInited;

private:
  void InitNamespaces ();
  void FixVoiceNamespaces ();
  void InitNamespace (EWadNamespace NS, VName Start, VName End, VName AltStart=NAME_None, VName AltEnd=NAME_None, bool flatNS=false);

public:
  VWadFile ();
  virtual ~VWadFile () override;

  void Open (VStr FileName, bool FixVoices, VStream *InStream);
  void OpenSingleLumpStream (VStream *strm, VStr FileName);
  virtual void Close () override;
  virtual VStream *CreateLumpReaderNum (int) override;
  virtual int CheckNumForName (VName LumpName, EWadNamespace InNS, bool wantFirst=true) override;
  virtual void ReadFromLump (int lump, void *dest, int pos, int size) override;
  virtual int IterateNS (int, EWadNamespace, bool allowEmptyName8=false) override;
};


//==========================================================================
//  VZipFile
//==========================================================================
class VZipFile : public VPakFileBase {
private:
  mythread_mutex rdlock;
  VStream *FileStream; // source stream of the zipfile
  vuint32 BytesBeforeZipFile; // byte before the zipfile, (>0 for sfx)

  // you can pass central dir offset here
  void OpenArchive (VStream *fstream, vuint32 cdofs=0);

public:
  VZipFile (VStr zipfile); // only disk files
  VZipFile (VStream *fstream); // takes ownership
  // you can pass central dir offset here
  VZipFile (VStream *fstream, VStr name, vuint32 cdofs=0); // takes ownership
  virtual ~VZipFile () override;

  virtual VStream *CreateLumpReaderNum (int) override;
  virtual void Close () override;

public: // fuck shitpp friend idiocity
  // returns 0 if not found
  static vuint32 SearchCentralDir (VStream *strm);
};


//==========================================================================
//  VQuakePakFile
//==========================================================================
class VQuakePakFile : public VPakFileBase {
private:
  mythread_mutex rdlock;
  VStream *Stream; // source stream of the zipfile

  void OpenArchive (VStream *fstream, int signtype=0);

public:
  VQuakePakFile (VStr);
  VQuakePakFile (VStream *fstream); // takes ownership
  VQuakePakFile (VStream *fstream, VStr name, int signtype=0); // takes ownership
  virtual ~VQuakePakFile () override;

  virtual VStream *CreateLumpReaderNum (int) override;
  virtual void ReadFromLump (int lump, void *dest, int pos, int size) override;
  virtual void Close () override;
};


//==========================================================================
//  VDirPakFile
//==========================================================================
class VDirPakFile : public VPakFileBase {
private:
  // relative to PakFileName
  void ScanDirectory (VStr relpath, int depth);

  void ScanAllDirs ();

public:
  VDirPakFile (VStr);

  //inline bool hasFiles () const { return (files.length() > 0); }

  virtual VStream *OpenFileRead (VStr)  override;
  virtual VStream *CreateLumpReaderNum (int) override;
  virtual int LumpLength (int) override;
};


// ////////////////////////////////////////////////////////////////////////// //
struct FArchiveReaderInfo {
public:
  // stream is guaranteed to be seeked after the signature
  // return `nullptr` to reject this archive format
  typedef VSearchPath *(*OpenCB) (VStream *strm, VStr filename, bool FixVoices);

private:
  FArchiveReaderInfo *next;
  OpenCB openCB;

public:
  const char *fmtname; // short name of the format, like "wad", or "pk3"
  const char *sign; // you can give a signature to check; can be `nullptr` or empty
  int priority; // lower priorities will be tried first

public:
  FArchiveReaderInfo (const char *afmtname, OpenCB ocb, const char *asign=nullptr, int apriority=666);

  // this owns the `strm` on success
  static VSearchPath *OpenArchive (VStream *strm, VStr filename, bool FixVoices=false);
};


// ////////////////////////////////////////////////////////////////////////// //
void W_AddFileFromZip (VStr WadName, VStream *WadStrm);

bool VFS_ShouldIgnoreExt (VStr fname);

// removes prefix, returns filter index (or -1, and does nothing)
int FL_CheckFilterName (VStr &fname);

VStream *FL_OpenFileRead_NoLock (VStr Name);
VStream *FL_OpenFileReadBaseOnly_NoLock (VStr Name);


// ////////////////////////////////////////////////////////////////////////// //
extern const VPK3ResDirInfo PK3ResourceDirs[];
extern TArray<VSearchPath *> SearchPaths;

extern bool fsys_report_added_paks;
extern bool fsys_no_dup_reports;

// autodetected wad/pk3
enum {
  AD_NONE,
  AD_SKULLDASHEE,
};

extern int fsys_detected_mod;

extern mythread_mutex fsys_glock;

extern int fsys_dev_dump_paks;

#endif
