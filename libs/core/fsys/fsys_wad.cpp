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
#include "fsys_local.h"


extern bool fsys_skipSounds;
extern bool fsys_skipSprites;
extern bool fsys_skipDehacked;


#pragma pack(push, 1)
// WAD file types
struct wadinfo_t {
  // should be "IWAD" or "PWAD"
  char identification[4];
  int numlumps;
  int infotableofs;
};


struct filelump_t {
  int filepos;
  int size;
  char name[8];
};
#pragma pack(pop)


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
  : VPakFileBase("")
  , Stream(nullptr)
  //, NumLumps(0)
  //, LumpInfo(nullptr)
  , lockInited(false)
{
}


//==========================================================================
//
//  VWadFile::~VWadFile
//
//==========================================================================
/*
VWadFile::~VWadFile () {
  Close();
}
*/


//==========================================================================
//
//  VWadFile::Open
//
//==========================================================================
void VWadFile::Open (VStr FileName, bool FixVoices, VStream *InStream) {
  wadinfo_t header;
  //lumpinfo_t *lump_p;
  int length;
  filelump_t *fileinfo;
  filelump_t *fi_p;

  if (!lockInited) {
    lockInited = true;
    mythread_mutex_init(&rdlock);
  }

  //Name = FileName;
  PakFileName = FileName;
  pakdir.clear();

  if (InStream) {
    Stream = InStream;
  } else {
    // open the file and add to directory
    Stream = FL_OpenSysFileRead(FileName);
    if (!Stream) Sys_Error("Couldn't open \"%s\"", *FileName);
  }
  if (fsys_report_added_paks && !FileName.isEmpty()) GLog.Logf(NAME_Init, "Adding \"%s\"...", *FileName);

  // WAD file or homebrew levels?
  Stream->Serialise(&header, sizeof(header));
  if (VStr::NCmp(header.identification, "IWAD", 4) != 0 &&
      VStr::NCmp(header.identification, "PWAD", 4) != 0)
  {
    Sys_Error("Wad file \"%s\" is neither IWAD nor PWAD\n", *FileName);
  }

  header.numlumps = LittleLong(header.numlumps);
  header.infotableofs = LittleLong(header.infotableofs);
  int NumLumps = header.numlumps;
  if (NumLumps < 0 || NumLumps > 65520) Sys_Error("invalid number of lumps in wad file '%s'", *FileName);

  // moved here to make static data less fragmented
  //LumpInfo = new lumpinfo_t[NumLumps];
  length = header.numlumps*(int)sizeof(filelump_t);
  fi_p = fileinfo = (filelump_t *)Z_Malloc(length);
  Stream->Seek(header.infotableofs);
  Stream->Serialise(fileinfo, length);
  if (Stream->IsError()) Sys_Error("cannot read directory of wad file '%s'", *FileName);

  // fill in lumpinfo
  //lump_p = LumpInfo;
  for (int i = 0; i < NumLumps; ++i, /*++lump_p,*/ ++fileinfo) {
    // Mac demo hexen.wad:  many (1784) of the lump names
    // have their first character with the high bit (0x80)
    // set. I don't know the reason for that. We must clear
    // the high bits for such Mac wad files to work in this
    // engine. This shouldn't break other wads.
    VPakFileInfo fi;
    char namebuf[9];
    for (int j = 0; j < 8; ++j) namebuf[j] = fileinfo->name[j]&0x7f;
    namebuf[8] = 0;
    //
    fi.lumpName = VName(namebuf, VName::AddLower8);
    fi.pakdataofs = LittleLong(fileinfo->filepos);
    fi.filesize = LittleLong(fileinfo->size);
    fi.lumpNamespace = WADNS_Global;
    fi.fileName = VStr(fi.lumpName);
    pakdir.append(fi);
    /*
    lump_p->Name = VName(namebuf, VName::AddLower8);
    lump_p->Position = LittleLong(fileinfo->filepos);
    lump_p->Size = LittleLong(fileinfo->size);
    lump_p->Namespace = WADNS_Global;
    */
  }

  Z_Free(fi_p);

  // set up namespaces
  InitNamespaces();

  if (FixVoices) FixVoiceNamespaces();

  pakdir.buildNameMaps();
}


//==========================================================================
//
//  VWadFile::OpenSingleLump
//
//==========================================================================
void VWadFile::OpenSingleLump (VStr FileName) {
  // open the file and add to directory
  Stream = FL_OpenSysFileRead(FileName);
  if (!Stream) Sys_Error("Couldn't open \"%s\"", *FileName);
  if (fsys_report_added_paks) GLog.Logf(NAME_Init, "Adding \"%s\"...", *FileName);

  PakFileName = FileName;
  VPakFileInfo fi;

  // single lump file
  //NumLumps = 1;
  //LumpInfo = new lumpinfo_t[1];

  // fill in lumpinfo
  fi.lumpName = VName(*FileName.ExtractFileBase(), VName::AddLower8);
  fi.pakdataofs = 0;
  fi.filesize = Stream->TotalSize();
  fi.lumpNamespace = WADNS_Global;
  fi.fileName = FileName.toLowerCase();
  //pakdir.appendAndRegister(fi);
  pakdir.append(fi);
  pakdir.buildNameMaps();
  /*
  LumpInfo->Name = VName(*FileName.ExtractFileBase(), VName::AddLower8);
  LumpInfo->Position = 0;
  LumpInfo->Size = Stream->TotalSize();
  LumpInfo->Namespace = WADNS_Global;
  */
}


//==========================================================================
//
//  VWadFile::Close
//
//==========================================================================
void VWadFile::Close () {
  if (Stream) {
    delete Stream;
    Stream = nullptr;
  }
  if (lockInited) {
    mythread_mutex_destroy(&rdlock);
    lockInited = false;
  }
}


//==========================================================================
//
//  VWadFile::InitNamespaces
//
//==========================================================================
void VWadFile::InitNamespaces () {
  InitNamespace(WADNS_Sprites, NAME_s_start, NAME_s_end, NAME_ss_start, NAME_ss_end);
  InitNamespace(WADNS_Flats, NAME_f_start, NAME_f_end, NAME_ff_start, NAME_ff_end, true);
  InitNamespace(WADNS_ColorMaps, NAME_c_start, NAME_c_end, NAME_cc_start, NAME_cc_end);
  InitNamespace(WADNS_ACSLibrary, NAME_a_start, NAME_a_end, NAME_aa_start, NAME_aa_end);
  InitNamespace(WADNS_NewTextures, NAME_tx_start, NAME_tx_end);
  InitNamespace(WADNS_Voices, NAME_v_start, NAME_v_end, NAME_vv_start, NAME_vv_end);
  InitNamespace(WADNS_HiResTextures, NAME_hi_start, NAME_hi_end);
  if (fsys_skipSounds || fsys_skipDehacked) {
    for (int i = 0; i < pakdir.files.length(); ++i) {
      VPakFileInfo &fi = pakdir.files[i];
      if (fi.lumpNamespace != WADNS_Global) continue;
      if (fi.lumpName == NAME_None) continue;
      const char *nn = *fi.lumpName;
      if (fsys_skipSounds) {
        if (nn[0] == 'd' && (nn[1] == 's' || nn[1] == 'p')) {
          fi.lumpNamespace = /*(EWadNamespace)*/-1;
          fi.lumpName = NAME_None;
          continue;
        }
      }
      if (fsys_skipDehacked) {
        if (fi.lumpName == "dehacked") {
          fi.lumpNamespace = /*(EWadNamespace)*/-1;
          fi.lumpName = NAME_None;
          continue;
        }
      }
    }
  }
}


struct NSInfo {
public:
  EWadNamespace NS;
  VName Start, End;
  VName AltStart, AltEnd;
  bool flatNS;
  bool inNS; // currently
  VName CurrEnd;

public:
  NSInfo (EWadNamespace aNS, VName aStart, VName aEnd, VName aAltStart, VName aAltEnd, bool aflatNS)
    : NS(aNS)
    , Start(aStart)
    , End(aEnd)
    , AltStart(aAltStart)
    , AltEnd(aAltEnd)
    , flatNS(aflatNS)
    , inNS(false)
    , CurrEnd(NAME_None)
  {}

  // returns ending lump name, or `NAME_None`
  static VName isFlatStartLump (VName lumpName) {
    const char *s = *lumpName;
    if (s[0] != 'f') return NAME_None;
    if (!VStr::endsWith(s, "_start")) return NAME_None;
    if (s[1] != '_' && (s[1] < '0' || s[1] > '9')) return NAME_None;
    // start of flats section
    VStr str(s);
    int under = str.indexOf('_');
    if (under < 0) return NAME_None;
    str = str.left(under);
    str += "_end";
    if (str.length() > 8) return NAME_None;
    return VName(*str);
  }

  static bool isFlatEndLump (VName lumpName) {
    const char *s = *lumpName;
    if (s[0] != 'f') return false;
    if (!VStr::endsWith(s, "_end")) return false;
    if (s[1] != '_' && (s[1] < '0' || s[1] > '9')) return false;
    // end of flats section
    return true;
  }

  // returns `true` if this lump must be cleared
  bool checkName (VName lumpName) {
    if (lumpName == NAME_None) return false; // just in case
    if (inNS) {
      // inside the namespace, check for ending lump
      if (lumpName == CurrEnd || lumpName == End || (AltEnd != NAME_None && lumpName == AltEnd)) {
        inNS = false;
        CurrEnd = NAME_None;
        return true;
      }
      if (!flatNS) return false;
      // if in the main lump
      if (CurrEnd == End || (AltEnd != NAME_None && CurrEnd == AltEnd)) {
        return (isFlatEndLump(lumpName) || isFlatStartLump(lumpName) != NAME_None);
      } else {
        // not in the main lump
        if (isFlatEndLump(lumpName)) {
          // end of flats section
          inNS = false;
          CurrEnd = NAME_None;
          return true;
        }
        // ignore sporadic start sublumps
        return (isFlatStartLump(lumpName) != NAME_None);
      }
    } else {
      // not inside the namespace, check for starting lump
      if (lumpName == Start) { CurrEnd = End; inNS = true; return true; }
      if (AltStart != NAME_None && lumpName == AltStart) { CurrEnd = AltEnd; inNS = true; return true; }
      if (flatNS) {
        VName el = isFlatStartLump(lumpName);
        if (el != NAME_None) { inNS = true; CurrEnd = el; return true; }
      }
    }
    return false;
  }
};


//==========================================================================
//
//  VWadFile::InitNamespace
//
//==========================================================================
void VWadFile::InitNamespace (EWadNamespace NS, VName Start, VName End, VName AltStart, VName AltEnd, bool flatNS) {
  NSInfo nsi(NS, Start, End, AltStart, AltEnd, flatNS);
  for (auto &&fi : pakdir.files) {
    //VPakFileInfo &fi = pakdir.files[i];
    // skip if lump is already in other namespace
    if (fi.lumpNamespace != WADNS_Global) continue;
    if (fi.lumpName == NAME_None) continue;
    if (nsi.checkName(fi.lumpName)) {
      // this is special lump, clear it
      fi.lumpNamespace = /*(EWadNamespace)*/-1;
      fi.lumpName = NAME_None;
      continue;
    }
    if (nsi.inNS) fi.lumpNamespace = NS;
  }
}


//==========================================================================
//
//  VWadFile::FixVoiceNamespaces
//
//==========================================================================
void VWadFile::FixVoiceNamespaces () {
  for (int i = 0; i < pakdir.files.length(); ++i) {
    VPakFileInfo &fi = pakdir.files[i];
    // skip if lump is already in other namespace
    if (fi.lumpNamespace != WADNS_Global) continue;
    const char *LName = *fi.lumpName;
    if (LName[0] == 'v' && LName[1] == 'o' && LName[2] == 'c' && LName[3] >= '0' && LName[3] <= '9' &&
        (LName[4] == 0 || (LName[4] >= '0' && LName[4] <= '9' &&
        (LName[5] == 0 || (LName[5] >= '0' && LName[5] <= '9' &&
        (LName[6] == 0 || (LName[6] >= '0' && LName[6] <= '9' &&
        (LName[7] == 0 || (LName[7] >= '0' && LName[7] <= '9')))))))))
    {
      fi.lumpNamespace = WADNS_Voices;
    }
  }
}


//==========================================================================
//
//  VWadFile::CreateLumpReaderNum
//
//==========================================================================
VStream *VWadFile::CreateLumpReaderNum (int lump) {
  check((vuint32)lump < (vuint32)pakdir.files.length());
  //lumpinfo_t &l = LumpInfo[lump];
  const VPakFileInfo &fi = pakdir.files[lump];

  // read the lump in
#if 0
  void *ptr = (fi.filesize ? Z_Malloc(fi.filesize) : nullptr);
  if (fi.filesize) {
    check(lockInited);
    MyThreadLocker locker(&rdlock);
    Stream->Seek(fi.pakdataofs);
    Stream->Serialise(ptr, fi.filesize);
    //check(!Stream->IsError());
    if (Stream->IsError()) Host_Error("cannot load lump '%s'", *W_FullLumpName(lump));
  }

  // create stream
  VStream *S = new VMemoryStream(GetPrefix()+":"+pakdir.files[lump].fileName, ptr, fi.filesize, true);
#else
  // this is mt-protected
  VStream *S = new VPartialStreamRO(GetPrefix()+":"+pakdir.files[lump].fileName, Stream, fi.pakdataofs, fi.filesize, &rdlock);
#endif

  //GLog.Logf("WAD<%s>: lump=%d; name=<%s>; size=(%d:%d); ofs=0x%08x", *PakFileName, lump, *fi.lumpName, fi.filesize, S->TotalSize(), fi.pakdataofs);
  //Z_Free(ptr);
  return S;
}


//==========================================================================
//
//  VWadFile::OpenFileRead
//
//==========================================================================
VStream *VWadFile::OpenFileRead (VStr fname) {
  //VStr fn = fname.stripExtension();
  int lump = CheckNumForFileName(fname);
  if (lump < 0) return nullptr;
  return CreateLumpReaderNum(lump);
}


//==========================================================================
//
//  VWadFile::CheckNumForName
//
//  Returns -1 if name not found.
//
//==========================================================================
int VWadFile::CheckNumForName (VName LumpName, EWadNamespace NS, bool wantFirst) {
  if (LumpName == NAME_None) return -1;
  // special ZIP-file namespaces in WAD file are in global namespace
  //EWadNamespace NS = InNS;
  if (NS > WADNS_ZipSpecial && NS != WADNS_Any) NS = WADNS_Global;
  return VPakFileBase::CheckNumForName(LumpName, NS, wantFirst);
}


//==========================================================================
//
//  VWadFile::IterateNS
//
//==========================================================================
int VWadFile::IterateNS (int Start, EWadNamespace NS, bool allowEmptyName8) {
  if (NS > WADNS_ZipSpecial && NS != WADNS_Any) NS = WADNS_Global;
  return VPakFileBase::IterateNS(Start, NS, allowEmptyName8);
}


//==========================================================================
//
//  VWadFile::ReadFromLump
//
//  Loads part of the lump into the given buffer.
//
//==========================================================================
void VWadFile::ReadFromLump (int lump, void *dest, int pos, int size) {
  check(size >= 0);
  check(pos >= 0);
  if ((vuint32)lump >= (vuint32)pakdir.files.length()) Sys_Error("VWadFile::ReadFromLump: %i >= numlumps", lump);
  VPakFileInfo &fi = pakdir.files[lump];
  if (pos >= fi.filesize || !size) {
    if (size > 0) memset(dest, 0, (unsigned)size);
    return;
  }
  if (size > 0) {
    check(lockInited);
    MyThreadLocker locker(&rdlock);
    Stream->Seek(fi.pakdataofs+pos);
    Stream->Serialise(dest, size);
    check(!Stream->IsError());
  }
}


//==========================================================================
//
//  VWadFile::RenameSprites
//
//==========================================================================
void VWadFile::RenameSprites (const TArray<VSpriteRename> &A, const TArray<VLumpRename> &LA) {
  bool wasRename = false;
  for (int i = 0; i < pakdir.files.length(); ++i) {
    VPakFileInfo &fi = pakdir.files[i];
    if (fi.lumpNamespace != WADNS_Sprites) continue;
    for (int j = 0; j < A.Num(); ++j) {
      if ((*fi.lumpName)[0] != A[j].Old[0] || (*fi.lumpName)[1] != A[j].Old[1] ||
          (*fi.lumpName)[2] != A[j].Old[2] || (*fi.lumpName)[3] != A[j].Old[3])
      {
        continue;
      }
      char newname[16];
      auto len = (int)strlen(*fi.lumpName);
      if (len) {
        if (len > 12) len = 12;
        memcpy(newname, *fi.lumpName, len);
      }
      newname[len] = 0;
      newname[0] = A[j].New[0];
      newname[1] = A[j].New[1];
      newname[2] = A[j].New[2];
      newname[3] = A[j].New[3];
      GLog.Logf(NAME_Dev, "renaming WAD sprite '%s' to '%s'", *fi.lumpName, newname);
      fi.lumpName = newname;
      wasRename = true;
    }
    for (int j = 0; j < LA.Num(); ++j) {
      if (fi.lumpName == LA[j].Old) {
        fi.lumpName = LA[j].New;
        wasRename = true;
      }
    }
  }
  if (wasRename) pakdir.buildNameMaps(true);
}
