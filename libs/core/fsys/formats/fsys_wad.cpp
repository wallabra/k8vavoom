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
//**  Copyright (C) 2018-2020 Ketmar Dark
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
#include "../fsys_local.h"


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
VWadFile::VWadFile () : VPakFileBase("", false) // not a zip
{
  normalwad = true;
}


//==========================================================================
//
//  VWadFile::Open
//
//==========================================================================
VWadFile *VWadFile::Create (VStr FileName, bool FixVoices, VStream *InStream) {
  wadinfo_t header;

  VWadFile *wad = new VWadFile();

  wad->PakFileName = FileName;
  wad->pakdir.clear();

  if (InStream) {
    wad->archStream = InStream;
    //archStream->Seek(0);
  } else {
    // open the file and add to directory
    wad->archStream = FL_OpenSysFileRead(FileName);
    if (!wad->archStream) { delete wad; Sys_Error("Couldn't open \"%s\"", *FileName); }
  }
  if (fsys_report_added_paks && !FileName.isEmpty()) GLog.Logf(NAME_Init, "Adding \"%s\"...", *FileName);

  // WAD file or homebrew levels?
  memset(&header, 0, sizeof(header));
  wad->archStream->Serialise(&header, sizeof(header));
  if (VStr::NCmp(header.identification, "IWAD", 4) != 0 &&
      VStr::NCmp(header.identification, "PWAD", 4) != 0)
  {
    delete wad;
    Sys_Error("Wad file \"%s\" is neither IWAD nor PWAD", *FileName);
  }

  header.numlumps = LittleLong(header.numlumps);
  header.infotableofs = LittleLong(header.infotableofs);
  int NumLumps = header.numlumps;
  if (NumLumps < 0 || NumLumps > 65520) { delete wad; Sys_Error("invalid number of lumps in wad file '%s'", *FileName); }

  // moved here to make static data less fragmented
  if (NumLumps > 0) {
    wad->archStream->Seek(header.infotableofs);
    for (int decount = NumLumps; decount > 0; --decount) {
      VPakFileInfo fi;

      vuint32 ofs, size;
      *wad->archStream << ofs << size;

      char namebuf[9];
      wad->archStream->Serialise(namebuf, 8);
      if (wad->archStream->IsError()) { delete wad; Sys_Error("cannot read wad file '%s'", *FileName); }
      if (!namebuf[0]) continue; // something strange happened here

      // Mac demo hexen.wad: many (1784) of the lump names
      // have their first character with the high bit (0x80)
      // set. I don't know the reason for that. We must clear
      // the high bits for such Mac wad files to work in this
      // engine. This shouldn't break other wads.
      for (int j = 0; j < 8; ++j) namebuf[j] &= 0x7f;
      namebuf[8] = 0;

      fi.lumpName = VName(namebuf, VName::AddLower8);
      fi.pakdataofs = ofs;
      fi.filesize = size;
      fi.lumpNamespace = WADNS_Global;
      fi.fileName = VStr(fi.lumpName);
      wad->pakdir.append(fi);
    }
  }

  // set up namespaces
  wad->InitNamespaces();

  if (FixVoices) wad->FixVoiceNamespaces();

  wad->pakdir.buildNameMaps(false, wad);

  if (wad->archStream->IsError()) { delete wad; Sys_Error("error opening archive \"%s\"", *FileName); }

  return wad;
}


//==========================================================================
//
//  VWadFile::OpenSingleLumpStream
//
//  open the file and add to directory
//
//==========================================================================
VWadFile *VWadFile::CreateSingleLumpStream (VStream *strm, VStr FileName) {
  vassert(strm);

  VWadFile *wad = new VWadFile();
  wad->archStream = strm;
  if (fsys_report_added_paks) GLog.Logf(NAME_Init, "Adding \"%s\"...", *FileName);

  wad->PakFileName = FileName;
  VPakFileInfo fi;

  // fill in lumpinfo
  fi.lumpName = VName(*FileName.ExtractFileBase(), VName::AddLower8);
  fi.pakdataofs = 0;
  fi.filesize = wad->archStream->TotalSize();
  fi.lumpNamespace = WADNS_Global;
  fi.fileName = FileName.toLowerCase();
  wad->pakdir.append(fi);
  wad->pakdir.buildNameMaps();

  if (wad->archStream->IsError()) { delete wad; Sys_Error("error opening file \"%s\"", *FileName); }

  return wad;
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


// ////////////////////////////////////////////////////////////////////////// //
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
  vassert((vuint32)lump < (vuint32)pakdir.files.length());
  //lumpinfo_t &l = LumpInfo[lump];
  const VPakFileInfo &fi = pakdir.files[lump];

  // read the lump in
#if 0
  void *ptr = (fi.filesize ? Z_Malloc(fi.filesize) : nullptr);
  if (fi.filesize) {
    MyThreadLocker locker(&rdlock);
    archStream->Seek(fi.pakdataofs);
    archStream->Serialise(ptr, fi.filesize);
    //vassert(!archStream->IsError());
    if (archStream->IsError()) Host_Error("cannot load lump '%s'", *W_FullLumpName(lump));
  }

  // create stream
  VStream *S = new VMemoryStream(GetPrefix()+":"+fi.fileName, ptr, fi.filesize, true);
#else
  // this is mt-protected
  VStream *S = new VPartialStreamRO(GetPrefix()+":"+fi.fileName, archStream, fi.pakdataofs, fi.filesize, &rdlock);
#endif

  //GLog.Logf("WAD<%s>: lump=%d; name=<%s>; size=(%d:%d); ofs=0x%08x", *PakFileName, lump, *fi.lumpName, fi.filesize, S->TotalSize(), fi.pakdataofs);
  //Z_Free(ptr);
  return S;
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
