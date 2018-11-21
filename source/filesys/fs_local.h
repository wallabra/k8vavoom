//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
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
#ifndef __FS_LOCAL_H__
#define __FS_LOCAL_H__


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
  virtual int CheckNumForName (VName LumpName, EWadNamespace InNS) = 0;
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
  virtual void Close () override;
  virtual int CheckNumForName (VName, EWadNamespace) override;
  virtual int CheckNumForFileName (const VStr &) override;
  virtual void ReadFromLump (int, void*, int, int) override;
  virtual int LumpLength (int) override;
  virtual VName LumpName (int) override;
  virtual VStr LumpFileName (int) override;
  virtual int IterateNS (int, EWadNamespace) override;
  virtual VStream *CreateLumpReaderNum (int) override;
  virtual void RenameSprites (const TArray<VSpriteRename>&, const TArray<VLumpRename>&) override;
  virtual VStr GetPrefix () override { return path; }
};


//==========================================================================
//  VWadFile
//==========================================================================
struct lumpinfo_t;

class VWadFile : public VSearchPath {
private:
  VStr Name;
  VStream *Stream;
  int NumLumps;
  lumpinfo_t *LumpInfo; // location of each lump on disk
  VStr GwaDir;

private:
  void InitNamespaces ();
  void FixVoiceNamespaces ();
  void InitNamespace (EWadNamespace NS, VName Start, VName End, VName AltStart=NAME_None, VName AltEnd=NAME_None);

public:
  VWadFile ();
  virtual ~VWadFile () override;
  void Open (const VStr &FileName, const VStr &AGwaDir, bool FixVoices, VStream *InStream);
  void OpenSingleLump (const VStr &FileName);
  virtual void Close () override;
  virtual int CheckNumForName (VName LumpName, EWadNamespace NS) override;
  virtual int CheckNumForFileName (const VStr &) override;
  virtual void ReadFromLump (int lump, void *dest, int pos, int size) override;
  virtual int LumpLength (int) override;
  virtual VName LumpName (int) override;
  virtual VStr LumpFileName (int) override;
  virtual int IterateNS (int, EWadNamespace) override;
  virtual bool FileExists (const VStr &) override;
  virtual VStream *OpenFileRead (const VStr &) override;
  virtual VStream *CreateLumpReaderNum (int) override;
  virtual void RenameSprites (const TArray<VSpriteRename>&, const TArray<VLumpRename>&) override;
  virtual VStr GetPrefix () override { return Name; }
};


//==========================================================================
//  VZipFile
//==========================================================================
struct VZipFileInfo;

class VZipFile : public VSearchPath {
private:
  mythread_mutex rdlock;
  VStr ZipFileName;
  VStream *FileStream; // source stream of the zipfile
  VZipFileInfo *Files;
  vuint16 NumFiles; // total number of files
  vuint32 BytesBeforeZipFile; // byte before the zipfile, (>0 for sfx)

  vuint32 SearchCentralDir ();
  //static int FileCmpFunc (const void*, const void*);
  void OpenArchive (VStream *fstream);

public:
  VZipFile (const VStr &);
  VZipFile (VStream *fstream); // takes ownership
  VZipFile (VStream *fstream, const VStr &name); // takes ownership
  virtual ~VZipFile () override;
  virtual bool FileExists (const VStr &) override;
  virtual VStream *OpenFileRead (const VStr &)  override;
  virtual void Close () override;
  virtual int CheckNumForName (VName, EWadNamespace) override;
  virtual int CheckNumForFileName (const VStr &) override;
  virtual void ReadFromLump (int, void*, int, int) override;
  virtual int LumpLength (int) override;
  virtual VName LumpName (int) override;
  virtual VStr LumpFileName (int) override;
  virtual int IterateNS (int, EWadNamespace) override;
  virtual VStream *CreateLumpReaderNum (int) override;
  virtual void RenameSprites (const TArray<VSpriteRename> &, const TArray<VLumpRename> &) override;

  void ListWadFiles (TArray<VStr> &);
  void ListPk3Files (TArray<VStr> &);

  virtual VStr GetPrefix () override { return ZipFileName; }
};


//==========================================================================
//  VDirPakFile
//==========================================================================
class VDirPakFile : public VSearchPath {
public: // fuck you, shitplusplus
  struct FileEntry {
    VName pakname;
    VName lumpname; // without extension
    VStr diskname;
    //vint32 size;
    EWadNamespace ns;
  };

private:
  VStr PakFileName; // never ends with slash
  TArray<FileEntry> files;
  TMap<VStr, int> filemap; // maps names (with pathes) to file entries; names are lowercased

  // relative to PakFileName
  void ScanDirectory (VStr relpath, int depth, bool inProgs);

  void ScanAllDirs ();

public:
  VDirPakFile (const VStr &);
  virtual ~VDirPakFile () override;

  inline bool hasFiles () const { return (files.length() > 0); }

  virtual bool FileExists (const VStr &) override;
  virtual VStream *OpenFileRead (const VStr &)  override;
  virtual void Close () override;
  virtual int CheckNumForName (VName, EWadNamespace) override;
  virtual int CheckNumForFileName (const VStr &) override;
  virtual void ReadFromLump (int, void *, int, int) override;
  virtual int LumpLength (int) override;
  virtual VName LumpName (int) override;
  virtual VStr LumpFileName (int) override;
  virtual int IterateNS (int, EWadNamespace) override;
  virtual VStream *CreateLumpReaderNum (int) override;
  virtual void RenameSprites (const TArray<VSpriteRename> &, const TArray<VLumpRename> &) override;

  void ListWadFiles (TArray<VStr> &);
  void ListPk3Files (TArray<VStr> &);

  virtual VStr GetPrefix () override { return PakFileName; }
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
void W_AddFileFromZip (const VStr &WadName, VStream *WadStrm, const VStr &GwaName, VStream *GwaStrm);

bool GLBSP_BuildNodes (const char *name, const char *gwafile);
void GLVis_BuildPVS (const char *srcfile, const char *gwafile);


// ////////////////////////////////////////////////////////////////////////// //
extern const VPK3ResDirInfo PK3ResourceDirs[];
extern TArray<VSearchPath *> SearchPaths;

extern bool fsys_report_added_paks;


#endif
