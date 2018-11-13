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
#include "gamedefs.h"
#include "fs_local.h"
#include "fwaddefs.h"


extern bool fsys_skipSounds;
extern bool fsys_skipSprites;
extern bool fsys_skipDehacked;


struct lumpinfo_t {
  VName Name;
  vint32 Position;
  vint32 Size;
  EWadNamespace Namespace;
};


//==========================================================================
//
//  VWadFile::VWadFile
//
//==========================================================================
VWadFile::VWadFile ()
  : Name(NAME_None)
  , Stream(nullptr)
  , NumLumps(0)
  , LumpInfo(nullptr)
  , GwaDir()
{
}


//==========================================================================
//
//  VWadFile::~VWadFile
//
//==========================================================================
VWadFile::~VWadFile () {
  Close();
}


//==========================================================================
//
//  VWadFile::Open
//
//==========================================================================
void VWadFile::Open (const VStr &FileName, const VStr &AGwaDir, bool FixVoices, VStream *InStream) {
  guard(VWadFile::Open);
  wadinfo_t header;
  lumpinfo_t *lump_p;
  int length;
  filelump_t *fileinfo;
  filelump_t *fi_p;

  Name = FileName;
  GwaDir = AGwaDir;

  if (InStream) {
    Stream = InStream;
  } else {
    // open the file and add to directory
    Stream = FL_OpenSysFileRead(FileName);
    if (!Stream) Sys_Error("Couldn't open \"%s\"", *FileName);
  }
  if (fsys_report_added_paks) GCon->Logf(NAME_Init, "Adding \"%s\"...", *FileName);

  // WAD file or homebrew levels?
  Stream->Serialise(&header, sizeof(header));
  if (VStr::NCmp(header.identification, "IWAD", 4) != 0 &&
      VStr::NCmp(header.identification, "PWAD", 4) != 0)
  {
    Sys_Error("Wad file \"%s\" is neither IWAD nor PWAD id\n", *FileName);
  }

  header.numlumps = LittleLong(header.numlumps);
  header.infotableofs = LittleLong(header.infotableofs);
  NumLumps = header.numlumps;
  // moved here to make static data less fragmented
  LumpInfo = new lumpinfo_t[NumLumps];
  length = header.numlumps*(int)sizeof(filelump_t);
  fi_p = fileinfo = (filelump_t *)Z_Malloc(length);
  Stream->Seek(header.infotableofs);
  Stream->Serialise(fileinfo, length);

  // fill in lumpinfo
  lump_p = LumpInfo;
  for (int i = 0; i < NumLumps; ++i, ++lump_p, ++fileinfo) {
    // Mac demo hexen.wad:  many (1784) of the lump names
    // have their first character with the high bit (0x80)
    // set. I don't know the reason for that. We must clear
    // the high bits for such Mac wad files to work in this
    // engine. This shouldn't break other wads.
    for (int j = 0; j < 8; ++j) fileinfo->name[j] &= 0x7f;
    lump_p->Name = VName(fileinfo->name, VName::AddLower8);
    lump_p->Position = LittleLong(fileinfo->filepos);
    lump_p->Size = LittleLong(fileinfo->size);
    lump_p->Namespace = WADNS_Global;
  }

  Z_Free(fi_p);

  // set up namespaces
  InitNamespaces();

  if (FixVoices) FixVoiceNamespaces();
  unguard;
}


//==========================================================================
//
//  VWadFile::OpenSingleLump
//
//==========================================================================
void VWadFile::OpenSingleLump (const VStr &FileName) {
  guard(VWadFile::OpenSingleLump);
  // open the file and add to directory
  Stream = FL_OpenSysFileRead(FileName);
  if (!Stream) Sys_Error("Couldn't open \"%s\"", *FileName);
  if (fsys_report_added_paks) GCon->Logf(NAME_Init, "Adding \"%s\"...", *FileName);

  Name = FileName;
  GwaDir = VStr();

  // single lump file
  NumLumps = 1;
  LumpInfo = new lumpinfo_t[1];

  // fill in lumpinfo
  LumpInfo->Name = VName(*FileName.ExtractFileBase(), VName::AddLower8);
  LumpInfo->Position = 0;
  LumpInfo->Size = Stream->TotalSize();
  LumpInfo->Namespace = WADNS_Global;
  unguard;
}


//==========================================================================
//
//  VWadFile::Close
//
//==========================================================================
void VWadFile::Close () {
  guard(VWadFile::Close);
  if (LumpInfo) {
    delete[] LumpInfo;
    LumpInfo = nullptr;
  }
  NumLumps = 0;
  Name.Clean();
  GwaDir.Clean();
  if (Stream) {
    delete Stream;
    Stream = nullptr;
  }
  unguard;
}


//==========================================================================
//
//  VWadFile::CheckNumForName
//
//  Returns -1 if name not found.
//
//==========================================================================
int VWadFile::CheckNumForName (VName LumpName, EWadNamespace InNS) {
  guard(VWadFile::CheckNumForName);
  // special ZIP-file namespaces in WAD file are in global namespace
  EWadNamespace NS = InNS;
  if (NS > WADNS_ZipSpecial) NS = WADNS_Global;
  for (int i = NumLumps-1; i >= 0; --i) {
    if (LumpInfo[i].Namespace == NS && LumpInfo[i].Name == LumpName) return i;
  }
  // not found
  return -1;
  unguard;
}


//==========================================================================
//
//  VWadFile::ReadFromLump
//
//  Loads part of the lump into the given buffer.
//
//==========================================================================
void VWadFile::ReadFromLump (int lump, void *dest, int pos, int size) {
  guard(VWadFile::ReadFromLump);
  if ((vuint32)lump >= (vuint32)NumLumps) Sys_Error("VWadFile::ReadFromLump: %i >= numlumps", lump);
  lumpinfo_t &l = LumpInfo[lump];
  if (pos >= l.Size) return;
  Stream->Seek(l.Position+pos);
  Stream->Serialise(dest, size);
  unguard;
}


//==========================================================================
//
//  VWadFile::InitNamespaces
//
//==========================================================================
void VWadFile::InitNamespaces () {
  guard(VWadFile::InitNamespaces);
  InitNamespace(WADNS_Sprites, NAME_s_start, NAME_s_end, NAME_ss_start, NAME_ss_end);
  InitNamespace(WADNS_Flats, NAME_f_start, NAME_f_end, NAME_ff_start, NAME_ff_end);
  InitNamespace(WADNS_ColourMaps, NAME_c_start, NAME_c_end, NAME_cc_start, NAME_cc_end);
  InitNamespace(WADNS_ACSLibrary, NAME_a_start, NAME_a_end, NAME_aa_start, NAME_aa_end);
  InitNamespace(WADNS_NewTextures, NAME_tx_start, NAME_tx_end);
  InitNamespace(WADNS_Voices, NAME_v_start, NAME_v_end, NAME_vv_start, NAME_vv_end);
  InitNamespace(WADNS_HiResTextures, NAME_hi_start, NAME_hi_end);
  if (fsys_skipSounds || fsys_skipDehacked) {
    for (int i = 0; i < NumLumps; ++i) {
      lumpinfo_t &L = LumpInfo[i];
      if (L.Namespace != WADNS_Global) continue;
      if (L.Name == NAME_None) continue;
      const char *nn = *L.Name;
      if (fsys_skipSounds) {
        if ((nn[0] == 'D' || nn[0] == 'd') &&
            (nn[1] == 'S' || nn[1] == 's' || nn[1] == 'P' || nn[1] == 'p'))
        {
          L.Namespace = (EWadNamespace)-1;
          L.Name = NAME_None;
          continue;
        }
      }
      if (fsys_skipDehacked) {
        if (VStr::ICmp(nn, "dehacked") == 0) {
          L.Namespace = (EWadNamespace)-1;
          L.Name = NAME_None;
          continue;
        }
      }
    }
  }
  unguard;
}


//==========================================================================
//
//  VWadFile::InitNamespace
//
//==========================================================================
void VWadFile::InitNamespace (EWadNamespace NS, VName Start, VName End, VName AltStart, VName AltEnd) {
  guard(VWadFile::InitNamespace);
  bool InNS = false;
  for (int i = 0; i < NumLumps; ++i) {
    lumpinfo_t &L = LumpInfo[i];
    // skip if lump is already in other namespace
    if (L.Namespace != WADNS_Global) continue;
    if (InNS) {
      // check for ending marker
      if (L.Name == End || (AltEnd != NAME_None && L.Name == AltEnd)) {
        InNS = false;
      } else {
        if ((fsys_skipSounds && NS == WADNS_Sounds) ||
            (fsys_skipSprites && NS == WADNS_Sprites))
        {
          L.Namespace = (EWadNamespace)-1;
          L.Name = NAME_None;
        }
        L.Namespace = NS;
      }
    } else {
      // check for starting marker
      if (L.Name == Start || (AltStart != NAME_None && L.Name == AltStart)) {
        InNS = true;
      }
    }
  }
  unguard;
}


//==========================================================================
//
//  VWadFile::FixVoiceNamespaces
//
//==========================================================================
void VWadFile::FixVoiceNamespaces () {
  guard(VWadFile::FixVoiceNamespaces);
  for (int i = 0; i < NumLumps; ++i) {
    lumpinfo_t &L = LumpInfo[i];
    // skip if lump is already in other namespace
    if (L.Namespace != WADNS_Global) continue;
    const char *LName = *L.Name;
    if (LName[0] == 'v' && LName[1] == 'o' && LName[2] == 'c' && LName[3] >= '0' && LName[3] <= '9' &&
        (LName[4] == 0 || (LName[4] >= '0' && LName[4] <= '9' &&
        (LName[5] == 0 || (LName[5] >= '0' && LName[5] <= '9' &&
        (LName[6] == 0 || (LName[6] >= '0' && LName[6] <= '9' &&
        (LName[7] == 0 || (LName[7] >= '0' && LName[7] <= '9')))))))))
    {
      L.Namespace = WADNS_Voices;
    }
  }
  unguard;
}


//==========================================================================
//
//  VWadFile::LumpLength
//
//  Returns the buffer size needed to load the given lump.
//
//==========================================================================
int VWadFile::LumpLength (int lump) {
  guard(VWadFile::LumpLength);
  return (lump >= 0 && lump < NumLumps ? LumpInfo[lump].Size : 0);
  unguard;
}


//==========================================================================
//
//  VWadFile::LumpName
//
//==========================================================================
VName VWadFile::LumpName (int lump) {
  guard(VWadFile::LumpName);
  return (lump >= 0 && lump < NumLumps ? LumpInfo[lump].Name : NAME_None);
  unguard;
}


//==========================================================================
//
//  VWadFile::LumpFileName
//
//==========================================================================
VStr VWadFile::LumpFileName (int lump) {
  guard(VWadFile::LumpName);
  return (lump >= 0 && lump < NumLumps ? VStr(*LumpInfo[lump].Name) : VStr());
  unguard;
}


//==========================================================================
//
//  VWadFile::IterateNS
//
//==========================================================================
int VWadFile::IterateNS (int Start, EWadNamespace NS) {
  guard(VWadFile::IterateNS);
  for (int li = Start; li < NumLumps; ++li) {
    if (LumpInfo[li].Namespace == NS) return li;
  }
  return -1;
  unguard;
}


//==========================================================================
//
//  VWadFile::CreateLumpReaderNum
//
//==========================================================================
VStream *VWadFile::CreateLumpReaderNum (int lump) {
  guard(VWadFile::CreateLumpReaderNum);
  check((vuint32)lump < (vuint32)NumLumps);
  lumpinfo_t &l = LumpInfo[lump];

  // read the lump in
  void *ptr = Z_Malloc(l.Size);
  if (l.Size) {
    Stream->Seek(l.Position);
    Stream->Serialise(ptr, l.Size);
  }

  // create stream
  VStream *S = new VMemoryStream(ptr, l.Size);
  Z_Free(ptr);
  return S;
  unguard;
}


//==========================================================================
//
//  VWadFile::RenameSprites
//
//==========================================================================
void VWadFile::RenameSprites (const TArray<VSpriteRename> &A, const TArray<VLumpRename> &LA) {
  guard(VWadFile::RenameSprites);
  for (int i = 0; i < NumLumps; ++i) {
    lumpinfo_t &L = LumpInfo[i];
    if (L.Namespace != WADNS_Sprites) continue;
    for (int j = 0; j < A.Num(); ++j) {
      if ((*L.Name)[0] != A[j].Old[0] || (*L.Name)[1] != A[j].Old[1] ||
          (*L.Name)[2] != A[j].Old[2] || (*L.Name)[3] != A[j].Old[3])
      {
        continue;
      }
      char newname[16];
      auto len = (int)strlen(*L.Name);
      if (len) {
        if (len > 12) len = 12;
        memcpy(newname, *L.Name, len);
      }
      newname[len] = 0;
      newname[0] = A[j].New[0];
      newname[1] = A[j].New[1];
      newname[2] = A[j].New[2];
      newname[3] = A[j].New[3];
      L.Name = newname;
    }
    for (int j = 0; j < LA.Num(); ++j) {
      if (L.Name == LA[j].Old) L.Name = LA[j].New;
    }
  }
  unguard;
}


//==========================================================================
//
//  VWadFile::CheckNumForFileName
//
//==========================================================================
int VWadFile::CheckNumForFileName (const VStr &fname) {
  //VStr fn = fname.stripExtension();
  VStr fn = fname;
  if (fn.isEmpty()) return -1;
  for (int f = NumLumps-1; f >= 0; --f) {
    if (fn.ICmp(*LumpInfo[f].Name) == 0) return f;
  }
  return -1;
}


//==========================================================================
//
//  VWadFile::FileExists
//
//==========================================================================
bool VWadFile::FileExists (const VStr &fname) {
  //VStr fn = fname.stripExtension();
  VStr fn = fname;
  if (fn.isEmpty()) return -1;
  for (int f = NumLumps-1; f >= 0; --f) {
    if (fn.ICmp(*LumpInfo[f].Name) == 0) return true;
  }
  return false;
}


//==========================================================================
//
//  VWadFile::OpenFileRead
//
//==========================================================================
VStream *VWadFile::OpenFileRead (const VStr &fname) {
  /*
  int lidx = CheckNumForFileName(fname);
  if (lidx == -1) return nullptr;
  //fprintf(stderr, "***WAD: <%s:%s>\n", *GetPrefix(), *fname);
  return CreateLumpReaderNum(lidx);
  */
  //VStr fn = fname.stripExtension();
  VStr fn = fname;
  if (fn.isEmpty()) return nullptr;
  for (int f = NumLumps-1; f >= 0; --f) {
    if (fn.ICmp(*LumpInfo[f].Name) == 0) {
      return CreateLumpReaderNum(f);
    }
  }
  return nullptr;
}
