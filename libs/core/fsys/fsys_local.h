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
  enum Type {
    WAD,
    PAK, // .pk3, .ipk3, .pak, etc. -- "extended game archive"
    OTHER, // zip file, for example
  };

public:
  Type type;
  bool iwad;
  bool basepak;
  bool userwad;
  bool cosmetic;
  bool required;

public:
  VSearchPath ();
  virtual ~VSearchPath ();

  // is this a Doom wad file (i.e. "IWAD"/"PWAD" one)?
  inline bool IsWad () const noexcept { return (type == WAD); }
  // is this any pak type?
  inline bool IsAnyPak () const noexcept { return (type >= WAD && type < OTHER); }
  // is this not a known archive (probably zip container)?
  inline bool IsNonPak () const noexcept { return (type == OTHER); }

  // all following methods are supposed to be called with global mutex protection
  // (i.e. they should not be called from multiple threads simultaneously)
  // if `lump` is not `nullptr`, sets it to file lump or to -1
  virtual bool FileExists (VStr Name, int *lump) = 0;
  // if `lump` is not `nullptr`, sets it to file lump or to -1
  virtual VStream *OpenFileRead (VStr Name, int *lump) = 0;
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
  // this is called by various lump methods when `filesize` is -1
  // can be used to cache lump sizes for archives where getting those sizes on open is expensive
  virtual void UpdateLumpLength (int Lump, int len) = 0;

  virtual void ListWadFiles (TArray<VStr> &list);
  virtual void ListPk3Files (TArray<VStr> &list);
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
  int nextLump; // next lump with the same name (forward order)
  int prevFile; // next file with the same name (backward order)
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
    , prevFile(-1)
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
  void buildNameMaps (bool rebuilding=false, VPakFileBase *pak=nullptr); // `true` to suppress warnings

  bool fileExists (VStr name, int *lump);
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
  // most archives needs this; it will be *deleted* in `Close()`/dtor!
  VStream *archStream;
  // most archives require shared lock, so i moved it here
  bool rdlockInited;
  mythread_mutex rdlock;

protected:
  // WARNING! lock init/deinit is not recursive, they're protected with a simple `bool` value!
  void initLock (); // call this in ctor
  void deinitLock (); // call this in `Clear()`/dtor

public:
  VPakFileBase (VStr apakfilename, bool aaszip=true);
  virtual ~VPakFileBase () override;

  virtual void Close () override;

  // if `lump` is not `nullptr`, sets it to file lump or to -1
  virtual bool FileExists (VStr fname, int *lump) override;
  // if `lump` is not `nullptr`, sets it to file lump or to -1
  virtual VStream *OpenFileRead (VStr fname, int *lump) override;
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

  virtual void UpdateLumpLength (int Lump, int len) override;
};


#include "formats/fsys_allfmts.h"


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

// returns `false` if file was filtered out (and clears name)
// returns `true` if file should be kept (and modifies name if necessary)
bool FL_CheckFilterName (VStr &fname);

VStream *FL_OpenFileRead_NoLock (VStr Name, int *lump);
VStream *FL_OpenFileReadBaseOnly_NoLock (VStr Name, int *lump);


// ////////////////////////////////////////////////////////////////////////// //
extern const VPK3ResDirInfo PK3ResourceDirs[];
extern TArray<VSearchPath *> fsysSearchPaths;

extern bool fsys_report_added_paks;
extern bool fsys_no_dup_reports;

extern mythread_mutex fsys_glock;

extern int fsys_dev_dump_paks;


// ////////////////////////////////////////////////////////////////////////// //
// mod detection mechanics

// autodetected wad/pk3
enum {
  AD_NONE,
};

class FSysModDetectorHelper {
  friend class VFileDirectory;
private:
  VFileDirectory *dir;
  VPakFileBase *pak;

public:
  inline FSysModDetectorHelper (VFileDirectory *adir, VPakFileBase *apak) noexcept : dir(adir), pak(apak) {}

  // hasLump("dehacked", 1066, "6bf56571d1f34d7cd7378b95556d67f8")
  bool hasLump (const char *lumpname, int size=-1, const char *md5=nullptr);
  // this checks for file; no globs allowed!
  bool hasFile (const char *filename, int size=-1, const char *md5=nullptr);
  // can be used to check zscript lump
  bool checkLump (int lumpidx, int size=-1, const char *md5=nullptr);

  // return lump *index*, or -1
  int findLump (const char *lumpname, int size=-1, const char *md5=nullptr);
  int findFile (const char *filename, int size=-1, const char *md5=nullptr);

  // returns -1 for invalid or disk lumps; never refreshes lump sizes, never opens lump streams to detect real sizes
  int getLumpSize (int lumpidx);
  // returns empty string for invalid lumps
  VStr getLumpMD5 (int lumpidx);

  // returns `nullptr` on error
  // WARNING: ALWAYS DELETE THE STREAM!
  VStream *createLumpReader (int lumpidx);
};

// returns AD_NONE or mod id
// mod detectors are called after registering an archive
// `seenZScriptLump` < 0: no zscript lump was seen
// return negative number to enable zscript, but don't register any mod
typedef int (*fsysModDetectorCB) (FSysModDetectorHelper &hlp, int seenZScriptLump);

void fsysRegisterModDetector (fsysModDetectorCB cb);

extern int fsys_detected_mod;
extern VStr fsys_detected_mod_wad;


// ////////////////////////////////////////////////////////////////////////// //
// GROSS HACK: you can "save" current open archives, and "append" them later
// without reopening. this is used to open user-specified archives at startup,
// and then move 'em down after base archives.
// ////////////////////////////////////////////////////////////////////////// //

// no autorestore; also, you can only save once
class FSysSavedState {
private:
  TArray<VSearchPath *> svSearchPaths;
  TArray<VStr> svwadfiles;
  bool saved;

public:
  VV_DISABLE_COPY(FSysSavedState)

  inline FSysSavedState () noexcept : svSearchPaths(), svwadfiles(), saved(false) {}
  inline ~FSysSavedState () noexcept {} // no autorestore

  inline bool isActive () const noexcept { return saved; }

  void save ();
  // this resets saved state
  void restore ();
};


// used in FSYS to disable sprofs from basepaks for known standalones
extern bool fsys_hide_sprofs;


#endif
