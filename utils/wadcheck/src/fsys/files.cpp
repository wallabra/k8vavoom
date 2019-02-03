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
#include "files.h"
#include "fs_local.h"


TArray<VSearchPath *> SearchPaths;


#define GET_LUMP_FILE(num)    SearchPaths[((num)>>16)&0xffff]
#define FILE_INDEX(num)       ((num)>>16)
#define LUMP_INDEX(num)       ((num)&0xffff)
#define MAKE_HANDLE(wi, num)  (((wi)<<16)+(num))


//==========================================================================
//
//  AddPakDir
//
//==========================================================================
/*
static void AddPakDir (const VStr &dirname) {
  if (dirname.length() == 0) return;
  VDirPakFile *dpak = new VDirPakFile(dirname);
  //if (!dpak->hasFiles()) { delete dpak; return; }

  SearchPaths.append(dpak);

  // add all WAD files in the root
  TArray<VStr> wads;
  dpak->ListWadFiles(wads);
  for (int i = 0; i < wads.length(); ++i) {
    VStream *wadst = dpak->OpenFileRead(wads[i]);
    if (!wadst) continue;
    W_AddFileFromZip(dpak->GetPrefix()+":"+wads[i], wadst, VStr(), nullptr);
  }

  // add all pk3 files in the root
  TArray<VStr> pk3s;
  dpak->ListPk3Files(pk3s);
  for (int i = 0; i < pk3s.length(); ++i) {
    VStream *pk3st = dpak->OpenFileRead(pk3s[i]);
    if (fsys_report_added_paks) GCon->Logf(NAME_Init, "Adding nested pk3 '%s:%s'...", *dpak->GetPrefix(), *pk3s[i]);
    VZipFile *pk3 = new VZipFile(pk3st, dpak->GetPrefix()+":"+pk3s[i]);
    AddZipFile(dpak->GetPrefix()+":"+pk3s[i], pk3, false);
  }
}
*/


//==========================================================================
//
//  FL_OpenFileRead
//
//==========================================================================
VStream *FL_OpenFileRead (const VStr &Name) {
  guard(FL_OpenFileRead);
  for (int i = SearchPaths.length()-1; i >= 0; --i) {
    VStream *Strm = SearchPaths[i]->OpenFileRead(Name);
    if (Strm) return Strm;
  }
  return nullptr;
  unguard;
}


//==========================================================================
//
//  FL_OpenSysFileRead
//
//==========================================================================
VStream *FL_OpenSysFileRead (const VStr &Name) {
  guard(FL_OpenSysFileRead);
  FILE *File = fopen(*Name, "rb");
  if (!File) return nullptr;
  return new VStreamFileReader(File, Name);
  unguard;
}


//==========================================================================
//
//  VStreamFileWriter
//
//==========================================================================
class VStreamFileWriter : public VStream {
protected:
  FILE *File;
  //FOutputDevice *Error;
  VStr fname;

public:
  VStreamFileWriter (FILE *InFile, const VStr &afname) : File(InFile), fname(afname) {
    guard(VStreamFileWriter::VStreamFileReader);
    bLoading = false;
    unguard;
  }

  virtual ~VStreamFileWriter () override {
    //guard(VStreamFileWriter::~VStreamFileWriter);
    if (File) Close();
    //unguard;
  }

  virtual const VStr &GetName () const override { return fname; }

  virtual void Seek (int InPos) override {
    if (!File || bError || fseek(File, InPos, SEEK_SET)) bError = true;
  }

  virtual int Tell () override {
    if (File && !bError) {
      int res = (int)ftell(File);
      if (res < 0) { bError = true; return 0; }
      return res;
    } else {
      bError = true;
      return 0;
    }
  }

  virtual int TotalSize () override {
    if (!File || bError) { bError = true; return 0; }
    int CurPos = ftell(File);
    if (fseek(File, 0, SEEK_END) != 0) { bError = true; return 0; }
    int Size = ftell(File);
    if (Size < 0) { bError = true; return 0; }
    if (fseek(File, CurPos, SEEK_SET) != 0) { bError = true; return 0; }
    return Size;
  }

  virtual bool AtEnd () override {
    if (File && !bError) return !!feof(File);
    bError = true;
    return true;
  }

  virtual bool Close () override {
    if (File && fclose(File)) bError = true;
    File = nullptr;
    return !bError;
  }

  virtual void Serialise (void *V, int Length) override {
    if (!File || bError) { bError = true; return; }
    if (fwrite(V, Length, 1, File) != 1) bError = true;
  }

  virtual void Flush () override {
    if (!File || bError) { bError = true; return; }
    if (fflush(File)) bError = true;
  }
};


//==========================================================================
//
//  FL_OpenFileWrite
//
//==========================================================================
VStream *FL_OpenFileWrite (const VStr &Name) {
  guard(FL_OpenFileWrite);
  VStr tmpName = Name;
  //FL_CreatePath(tmpName.ExtractFilePath());
  FILE *File = fopen(*tmpName, "wb");
  if (!File) return nullptr;
  return new VStreamFileWriter(File, tmpName);
  unguard;
}


//==========================================================================
//
//  FL_OpenSysFileWrite
//
//==========================================================================
VStream *FL_OpenSysFileWrite (const VStr &Name) {
  return FL_OpenFileWrite(Name);
}


//==========================================================================
//
//  VStreamFileReader
//
//==========================================================================
VStreamFileReader::VStreamFileReader(FILE *InFile, const VStr &afname)
  : File(InFile)
  //, Error(InError)
  , fname(afname)
{
  guard(VStreamFileReader::VStreamFileReader);
  fseek(File, 0, SEEK_SET);
  bLoading = true;
  unguard;
}


//==========================================================================
//
//  VStreamFileReader::~VStreamFileReader
//
//==========================================================================
VStreamFileReader::~VStreamFileReader() {
  if (File) Close();
}


//==========================================================================
//
//  VStreamFileReader::GetName
//
//==========================================================================
const VStr &VStreamFileReader::GetName () const {
  return fname;
}


//==========================================================================
//
//  VStreamFileReader::Seek
//
//==========================================================================
void VStreamFileReader::Seek (int InPos) {
#ifdef __SWITCH__
  // I don't know how or why this works, but unless you seek to 0 first,
  // fseeking on the Switch seems to set the pointer to an incorrect
  // position, but only sometimes
  if (File) fseek(File, 0, SEEK_SET);
#endif
  if (!File || bError || fseek(File, InPos, SEEK_SET)) bError = true;
}


//==========================================================================
//
//  VStreamFileReader::Tell
//
//==========================================================================
int VStreamFileReader::Tell () {
  if (File && !bError) {
    int res = (int)ftell(File);
    if (res < 0) { bError = true; return 0; }
    return res;
  } else {
    bError = true;
    return 0;
  }
}


//==========================================================================
//
//  VStreamFileReader::TotalSize
//
//==========================================================================
int VStreamFileReader::TotalSize () {
  if (!File || bError) { bError = true; return 0; }
  int CurPos = ftell(File);
  if (fseek(File, 0, SEEK_END) != 0) { bError = true; return 0; }
  int Size = ftell(File);
  if (Size < 0) { bError = true; return 0; }
  if (fseek(File, CurPos, SEEK_SET) != 0) { bError = true; return 0; }
  return Size;
}


//==========================================================================
//
//  VStreamFileReader::AtEnd
//
//==========================================================================
bool VStreamFileReader::AtEnd () {
  if (File && !bError) return !!feof(File);
  bError = true;
  return true;
}


//==========================================================================
//
//  VStreamFileReader::Close
//
//==========================================================================
bool VStreamFileReader::Close () {
  if (File) fclose(File);
  File = nullptr;
  return !bError;
}


//==========================================================================
//
//  VStreamFileReader::Serialise
//
//==========================================================================
void VStreamFileReader::Serialise (void *V, int Length) {
  if (!File || bError) { bError = true; return; }
  if (fread(V, Length, 1, File) != 1) bError = true;
}



//==========================================================================
//
//  W_AddFileStream
//
//  returns first lump id or -1
//
//==========================================================================
int W_AddFileStream (VStream *fstrm) {
  if (fstrm->TotalSize() < 16) Sys_Error("Cannot determine format for '%s'", *fstrm->GetName());
  char sign[4];
  fstrm->Seek(0);
  fstrm->Serialise(sign, 4);
  if (fstrm->IsError()) Sys_Error("Error reading '%s'", *fstrm->GetName());
  if (memcmp(sign, "IWAD", 4) == 0 || memcmp(sign, "PWAD", 4) == 0) {
    fstrm->Seek(0);
    VWadFile *wad = new VWadFile;
    wad->Open(fstrm->GetName(), fstrm);
    SearchPaths.Append(wad);
  } else {
    VZipFile *zip = new VZipFile(fstrm, fstrm->GetName());
    SearchPaths.Append(zip);
    // scan for embedded wads
    TArray<VStr> list;
    // scan for wads
    zip->ListWadFiles(list);
    for (int f = 0; f < list.length(); ++f) {
      VStream *wadstrm = zip->OpenFileRead(list[f]);
      if (!wadstrm) continue;
      if (wadstrm->TotalSize() < 16) { delete wadstrm; continue; }
      VStream *memstrm = new VMemoryStream(zip->GetPrefix()+":"+list[f], wadstrm);
      bool err = wadstrm->IsError();
      delete wadstrm;
      if (err) { delete memstrm; continue; }
      char sign[4];
      memstrm->Serialise(sign, 4);
      if (memcmp(sign, "PWAD", 4) != 0 && memcmp(sign, "IWAD", 4) != 0) { delete memstrm; continue; }
      memstrm->Seek(0);
      VWadFile *wad = new VWadFile;
      wad->Open(fstrm->GetName()+":"+list[f], memstrm);
      SearchPaths.Append(wad);
    }
  }
  return MAKE_HANDLE(SearchPaths.length()-1, 0);
}


//==========================================================================
//
//  W_AddFile
//
//  All files are optional, but at least one file must be found (PWAD, if
//  all required lumps are present). Files with a .wad extension are wadlink
//  files with multiple lumps. Other files are single lumps with the base
//  filename for the lump name.
//
//==========================================================================
int W_AddFile (const VStr &FileName) {
  VStream *fstrm = FL_OpenSysFileRead(FileName);
  if (!fstrm) fstrm = FL_OpenFileRead(FileName);
  if (!fstrm) Sys_Error("Required file '%s' doesn't exist", *FileName);
  return W_AddFileStream(fstrm);
}


//==========================================================================
//
//  W_LumpLength
//
//  Returns the buffer size needed to load the given lump.
//
//==========================================================================
int W_LumpLength (int lump) {
  guard(W_LumpLength);
  if (lump < 0 || FILE_INDEX(lump) >= SearchPaths.length()) Sys_Error("W_LumpLength: %i >= num_wad_files", FILE_INDEX(lump));
  VSearchPath *w = GET_LUMP_FILE(lump);
  int lumpindex = LUMP_INDEX(lump);
  return w->LumpLength(lumpindex);
  unguard;
}


//==========================================================================
//
//  W_LumpName
//
//==========================================================================
VName W_LumpName (int lump) {
  guard(W_LumpName);
  if (lump < 0 || FILE_INDEX(lump) >= SearchPaths.length()) return NAME_None;
  VSearchPath *w = GET_LUMP_FILE(lump);
  int lumpindex = LUMP_INDEX(lump);
  return w->LumpName(lumpindex);
  unguard;
}


//==========================================================================
//
//  W_FullLumpName
//
//==========================================================================
VStr W_FullLumpName (int lump) {
  guard(W_FullLumpName);
  if (lump < 0 || FILE_INDEX(lump) >= SearchPaths.length()) return VStr("<invalid>");
  VSearchPath *w = GET_LUMP_FILE(lump);
  int lumpindex = LUMP_INDEX(lump);
  //return w->GetPrefix()+":"+*(w->LumpName(lumpindex));
  return w->GetPrefix()+":"+*(w->LumpFileName(lumpindex));
  unguard;
}


//==========================================================================
//
//  W_FullPakNameForLump
//
//==========================================================================
VStr W_FullPakNameForLump (int lump) {
  guard(W_FullPakNameForLump);
  if (lump < 0 || FILE_INDEX(lump) >= SearchPaths.length()) return VStr("<invalid>");
  VSearchPath *w = GET_LUMP_FILE(lump);
  return w->GetPrefix();
  unguard;
}


//==========================================================================
//
//  W_FullPakNameByFile
//
//==========================================================================
VStr W_FullPakNameByFile (int fidx) {
  guard(W_FullPakNameByFile);
  if (fidx < 0 || fidx >= SearchPaths.length()) return VStr("<invalid>");
  VSearchPath *w = SearchPaths[fidx];
  return w->GetPrefix();
  unguard;
}


//==========================================================================
//
//  W_LumpFile
//
//  Returns file index of the given lump.
//
//==========================================================================
int W_LumpFile (int lump) {
  return (lump < 0 ? -1 : FILE_INDEX(lump));
}


//==========================================================================
//
//  W_ReadFromLump
//
//==========================================================================
void W_ReadFromLump (int lump, void *dest, int pos, int size) {
  guard(W_ReadFromLump);
  if (lump < 0 || FILE_INDEX(lump) >= SearchPaths.length()) Sys_Error("W_ReadFromLump: %i >= num_wad_files", FILE_INDEX(lump));
  VSearchPath *w = GET_LUMP_FILE(lump);
  w->ReadFromLump(LUMP_INDEX(lump), dest, pos, size);
  unguard;
}


//==========================================================================
//
//  W_CreateLumpReaderNum
//
//==========================================================================
VStream *W_CreateLumpReaderNum (int lump) {
  guard(W_CreateLumpReaderNum);
  if (lump < 0 || FILE_INDEX(lump) >= SearchPaths.length()) Sys_Error("W_CreateLumpReaderNum: %i >= num_wad_files", FILE_INDEX(lump));
  return GET_LUMP_FILE(lump)->CreateLumpReaderNum(LUMP_INDEX(lump));
  unguard;
}


//==========================================================================
//
//  W_StartIterationFromLumpFile
//
//==========================================================================
int W_StartIterationFromLumpFile (int File) {
  if (File < 0) return -1;
  if (File >= SearchPaths.length()) return MAKE_HANDLE(SearchPaths.length()+1, 69)-1;
  return MAKE_HANDLE(File, 0)-1;
}


//==========================================================================
//
//  W_IterateNS
//
//==========================================================================
int W_IterateNS (int Prev, EWadNamespace NS) {
  guard(W_IterateNS);
  if (Prev < 0) Prev = -1;
  int wi = FILE_INDEX(Prev+1);
  int li = LUMP_INDEX(Prev+1);
  //GLog.WriteLine("!!! %d %d (file=%d; idx=%d; %d)", Prev, (int)NS, wi, li, SearchPaths.length());
  for (; wi < SearchPaths.length(); ++wi, li = 0) {
    li = SearchPaths[wi]->IterateNS(li, NS);
    if (li != -1) return MAKE_HANDLE(wi, li);
  }
  return -1;
  unguard;
}


//==========================================================================
//
//  W_IterateFile
//
//==========================================================================
int W_IterateFile (int Prev, const VStr &Name) {
  guard(W_IterateFile);
  //GCon->Logf(NAME_Dev, "W_IterateFile: Prev=%d (%d); fn=<%s>", Prev, SearchPaths.length(), *Name);
  for (int wi = FILE_INDEX(Prev)+1; wi < SearchPaths.length(); ++wi) {
    int li = SearchPaths[wi]->CheckNumForFileName(Name);
    //GCon->Logf(NAME_Dev, "W_IterateFile: wi=%d (%d); fn=<%s>; li=%d", wi, SearchPaths.length(), *Name, li);
    if (li != -1) return MAKE_HANDLE(wi, li);
  }
  return -1;
  unguard;
}


//==========================================================================
//
//  W_LoadLumpIntoArray
//
//==========================================================================
void W_LoadLumpIntoArray (int lump, TArray<vuint8> &Array) {
  VStream *Strm = W_CreateLumpReaderNum(lump);
  check(Strm);
  Array.SetNum(Strm->TotalSize());
  Strm->Serialise(Array.Ptr(), Strm->TotalSize());
  if (Strm->IsError()) { delete Strm; Sys_Error("error reading lump '%s'", *W_FullLumpName(lump)); }
  delete Strm;
}
