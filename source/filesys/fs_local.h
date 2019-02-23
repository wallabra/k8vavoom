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
#ifndef FS_LOCAL_HEADER
#define FS_LOCAL_HEADER


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
  virtual ~VSearchPath () {}
  virtual bool FileExists (const VStr &Name) = 0;
  virtual VStream *OpenFileRead (const VStr &Name) = 0;
  virtual void Close () = 0;
  virtual int CheckNumForName (VName LumpName, EWadNamespace InNS, bool wantFirst=true) = 0;
  virtual int CheckNumForFileName (const VStr &Name) = 0;
  virtual void ReadFromLump (int LumpNum, void *Dest, int Pos, int Size) = 0;
  virtual int LumpLength (int LumpNum) = 0;
  virtual VName LumpName (int LumpNum) = 0;
  virtual VStr LumpFileName (int LumpNum) = 0;
  virtual int IterateNS (int Start, EWadNamespace NS) = 0;
  virtual VStream *CreateLumpReaderNum (int LumpNum) = 0;
  virtual void RenameSprites (const TArray<VSpriteRename> &A, const TArray<VLumpRename> &LA) = 0;
  virtual VStr GetPrefix () = 0; // for logging
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
  VFilesDir (const VStr &aPath);
  virtual bool FileExists (const VStr&) override;
  virtual VStream *OpenFileRead (const VStr&) override;
  virtual VStream *CreateLumpReaderNum (int) override;
  virtual void Close () override;
  virtual int CheckNumForName (VName LumpName, EWadNamespace InNS, bool wantFirst=true) override;
  virtual int CheckNumForFileName (const VStr &) override;
  virtual void ReadFromLump (int, void*, int, int) override;
  virtual int LumpLength (int) override;
  virtual VName LumpName (int) override;
  virtual VStr LumpFileName (int) override;
  virtual int IterateNS (int, EWadNamespace) override;
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

  int nextLump (vint32 curridx, vint32 ns);
};


//==========================================================================
//  VPakFileBase
//==========================================================================
class VPakFileBase : public VSearchPath {
public:
  VStr PakFileName; // never ends with slash
  VFileDirectory pakdir;

public:
  VPakFileBase (const VStr &apakfilename, bool aaszip=false);
  virtual ~VPakFileBase () override;

  //inline bool hasFiles () const { return (pakdir.files.length() > 0); }

  virtual void Close () override;

  virtual bool FileExists (const VStr &fname) override;
  //virtual VStream *OpenFileRead (const VStr &fname) override;
  virtual int CheckNumForName (VName LumpName, EWadNamespace InNS, bool wantFirst=true) override;
  virtual int CheckNumForFileName (const VStr &fname) override;
  virtual void ReadFromLump (int, void *, int, int) override;
  virtual int LumpLength (int) override;
  virtual VName LumpName (int) override;
  virtual VStr LumpFileName (int) override;
  virtual int IterateNS (int, EWadNamespace) override;
  //virtual VStream *CreateLumpReaderNum (int) override;
  virtual void RenameSprites (const TArray<VSpriteRename> &, const TArray<VLumpRename> &) override;

  void ListWadFiles (TArray<VStr> &list);
  void ListPk3Files (TArray<VStr> &list);

  VStr CalculateMD5 (int lumpidx);

  virtual VStr GetPrefix () override { return PakFileName; }
};


//==========================================================================
//  VWadFile
//==========================================================================
//struct lumpinfo_t;

class VWadFile : public VPakFileBase {
private:
  mythread_mutex rdlock;
  //VStr Name;
  VStream *Stream;
  //int NumLumps;
  //lumpinfo_t *LumpInfo; // location of each lump on disk
#ifdef VAVOOM_USE_GWA
  VStr GwaDir;
#endif
  bool lockInited;

private:
  void InitNamespaces ();
  void FixVoiceNamespaces ();
  void InitNamespace (EWadNamespace NS, VName Start, VName End, VName AltStart=NAME_None, VName AltEnd=NAME_None);

public:
  VWadFile ();
  //virtual ~VWadFile () override;
  void Open (const VStr &FileName, bool FixVoices, VStream *InStream, const VStr &AGwaDir);
  void OpenSingleLump (const VStr &FileName);
  virtual void Close () override;
  virtual VStream *OpenFileRead (const VStr &) override;
  virtual VStream *CreateLumpReaderNum (int) override;
  virtual int CheckNumForName (VName LumpName, EWadNamespace InNS, bool wantFirst=true) override;
  //virtual int CheckNumForFileName (const VStr &) override;
  virtual void ReadFromLump (int lump, void *dest, int pos, int size) override;
  //virtual int LumpLength (int) override;
  //virtual VName LumpName (int) override;
  //virtual VStr LumpFileName (int) override;
  virtual int IterateNS (int, EWadNamespace) override;
  //virtual bool FileExists (const VStr &) override;
  virtual void RenameSprites (const TArray<VSpriteRename>&, const TArray<VLumpRename>&) override;
  //virtual VStr GetPrefix () override { return Name; }
};


//==========================================================================
//  VZipFile
//==========================================================================
class VZipFile : public VPakFileBase {
private:
  mythread_mutex rdlock;
  VStream *FileStream; // source stream of the zipfile
  //vuint16 NumFiles; // total number of files
  vuint32 BytesBeforeZipFile; // byte before the zipfile, (>0 for sfx)

  vuint32 SearchCentralDir ();
  void OpenArchive (VStream *fstream);

public:
  VZipFile (const VStr &);
  VZipFile (VStream *fstream); // takes ownership
  VZipFile (VStream *fstream, const VStr &name); // takes ownership
  virtual ~VZipFile () override;

  virtual VStream *OpenFileRead (const VStr &)  override;
  virtual VStream *CreateLumpReaderNum (int) override;
  virtual void Close () override;
  virtual void RenameSprites (const TArray<VSpriteRename> &, const TArray<VLumpRename> &) override;
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
  VDirPakFile (const VStr &);

  //inline bool hasFiles () const { return (files.length() > 0); }

  virtual VStream *OpenFileRead (const VStr &)  override;
  virtual VStream *CreateLumpReaderNum (int) override;
  virtual int LumpLength (int) override;
};


//==========================================================================
//  VStreamFileReader
//==========================================================================
class VStreamFileReader : public VStream {
protected:
  FILE *File;
  FOutputDevice *Error;
  VStr fname;

public:
  VStreamFileReader (FILE*, FOutputDevice*, const VStr &afname);
  virtual ~VStreamFileReader () override;
  virtual const VStr &GetName () const override;
  virtual void Seek (int InPos) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
  virtual void Serialise (void *V, int Length) override;
};


// ////////////////////////////////////////////////////////////////////////// //
void W_AddFileFromZip (const VStr &WadName, VStream *WadStrm, const VStr &GwaName=VStr(), VStream *GwaStrm=nullptr);

bool VFS_ShouldIgnoreExt (const VStr &fname);


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


#endif
