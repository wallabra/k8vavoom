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
#include "gamedefs.h"
#include "r_tex.h"


static VCvarB dbg_dump_animdef_ranges("dbg_dump_animdef_ranges", false, "Dump ANIMDEF ranges?", CVAR_PreInit);


enum {
  FT_Texture,
  FT_Flat,
};


enum {
  ANIM_Forward,
  ANIM_Backward,
  ANIM_OscillateUp,
  ANIM_OscillateDown,
  ANIM_Random,
};


struct FrameDef_t {
  vint16 Index; // texture index
  float BaseTime; // in tics for animdefs
  vint16 RandomRange;
};


struct AnimDef_t {
  vint16 Index;
  vint16 NumFrames;
  float Time;
  vint16 StartFrameDef;
  vint16 CurrentFrame;
  vuint8 Type;
  int allowDecals;
  int range; // is this range animation?
};


// ////////////////////////////////////////////////////////////////////////// //
// the basic idea is to fill up both arrays, and use them to build animation lists.
// arrays are filled in the file order

struct ListLump {
  VName texName;
  int lumpFile;
  int nextIndex; // next list index for this name; -1 is EOL
};


struct ListOfLumps {
public:
  TArray<ListLump> list;
  TMapNC<VName, int> map; // first lump for the given name, so we don't have to loop over and over again

public:
  ListOfLumps () : list(), map() {}
  ListOfLumps (const ListOfLumps &) = delete;
  ListOfLumps &operator = (const ListOfLumps &) = delete;

  inline void clear () { map.clear(); list.clear(); }

  void append (VName txname, int filenum);

  // returns -1 if not found
  int findInFileFrom (int stidx, VName aname) const;

  inline int length () const { return list.length(); }
  const ListLump &operator [] (int idx) const { vassert(idx >= 0 && idx < list.length()); return list[idx]; }

public:
  struct NameIterator {
    const TArray<ListLump> *list;
    int currIndex;

    NameIterator () : list(nullptr), currIndex(-1) {}
    NameIterator (const ListOfLumps *alist, VName aname) : list(&alist->list) { auto mip = alist->map.find(aname); currIndex = (mip ? *mip : -1); }
    NameIterator (const NameIterator &it) : list(it.list), currIndex(it.currIndex) {}
    NameIterator (const NameIterator &it, bool asEnd) : list(it.list), currIndex(-1) {}
    inline NameIterator &operator = (const NameIterator &it) { list = it.list; currIndex = it.currIndex; return *this; }

    inline NameIterator begin () { return NameIterator(*this); }
    inline NameIterator end () { return NameIterator(*this, true); }
    inline bool operator == (const NameIterator &b) const { return (list == b.list && currIndex == b.currIndex); }
    inline bool operator != (const NameIterator &b) const { return (list != b.list || currIndex != b.currIndex); }
    inline NameIterator operator * () const { return NameIterator(*this); } /* required for iterator */
    inline void operator ++ () { if (currIndex > 0) currIndex = (*list)[currIndex].nextIndex; } /* this is enough for iterator */

    inline bool isEmpty () const { return (currIndex < 0); }
    inline int index () const { return currIndex; }
    inline VName getName () const { return (currIndex >= 0 ? (*list)[currIndex].texName : NAME_None); }
    inline int getFile () const { return (currIndex >= 0 ? (*list)[currIndex].lumpFile : -1); }
  };

  inline NameIterator named (VName aname) const { return NameIterator(this, aname); }

  inline int findInFileFrom (const NameIterator &it, VName aname) const { return findInFileFrom(it.index(), aname); }
};


//==========================================================================
//
//  ListOfLumps::append
//
//==========================================================================
void ListOfLumps::append (VName txname, int filenum) {
  if (txname == NAME_None) return;
  ListLump &newlilu = list.alloc();
  newlilu.texName = txname;
  newlilu.lumpFile = filenum;
  newlilu.nextIndex = -1;
  auto mip = map.find(txname);
  if (mip) {
    // append to the name chain
    int lidx = *mip;
    while (list[lidx].nextIndex != -1) {
      vassert(list[lidx].texName == txname);
      lidx = list[lidx].nextIndex;
    }
    vassert(lidx >= 0 && lidx < list.length()-1);
    vassert(list[lidx].nextIndex == -1);
    list[lidx].nextIndex = list.length()-1;
  } else {
    // first with such name
    map.put(txname, list.length()-1);
  }
}


//==========================================================================
//
//  ListOfLumps::findInFileFrom
//
//==========================================================================
int ListOfLumps::findInFileFrom (int stidx, VName aname) const {
  if (stidx < 0 || stidx >= list.length()) return -1;
  int filenum = list[stidx].lumpFile;
  for (; stidx < list.length(); ++stidx) {
    const ListLump &lilu = list[stidx];
    if (lilu.lumpFile != filenum) return -1;
    if (lilu.texName == aname) return stidx;
  }
  return -1;
}


// ////////////////////////////////////////////////////////////////////////// //
static ListOfLumps txxnames; // collected from TEXTUREx lumps
static ListOfLumps flatnames; // collected from loaded wads


// ////////////////////////////////////////////////////////////////////////// //
// switches
TArray<TSwitch *> Switches;

static TArray<AnimDef_t> AnimDefs;
static TArray<FrameDef_t> FrameDefs;
static TArray<VAnimDoorDef> AnimDoorDefs;

static TMapNC<VName, bool> animPicSeen; // temporary
static TMapNC<int, bool> animTexMap; // to make `R_IsAnimatedTexture()` faster


//==========================================================================
//
//  ClearLists
//
//==========================================================================
static void ClearLists () {
  txxnames.clear();
  flatnames.clear();
}


//==========================================================================
//
//  R_ShutdownFTAnims
//
//==========================================================================
void R_ShutdownFTAnims () {
  // clean up animation and switch definitions
  for (int i = 0; i < Switches.length(); ++i) {
    delete Switches[i];
    Switches[i] = nullptr;
  }
  Switches.Clear();
  AnimDefs.Clear();
  FrameDefs.Clear();
  for (int i = 0; i < AnimDoorDefs.length(); ++i) {
    delete[] AnimDoorDefs[i].Frames;
    AnimDoorDefs[i].Frames = nullptr;
  }
  AnimDoorDefs.Clear();
}


//==========================================================================
//
//  ParseTXXLump
//
//==========================================================================
static void ParseTXXLump (int lump) {
  if (lump < 0) return;
  VStream *strm = W_CreateLumpReaderNum(lump);
  if (!strm) return;
  //GCon->Logf("...parsing binary texture lump '%s'", *W_FullLumpName(lump));
  // texture count
  vuint32 tcount;
  *strm << tcount;
  if (tcount == 0 || tcount > 65535) { delete strm; return; }
  // texture offsets
  TArray<vuint32> offsets;
  offsets.setLength((int)tcount);
  for (auto &&ofs : offsets) *strm << ofs;
  if (strm->IsError()) { delete strm; return; }
  // read texture names
  for (auto &&ofs : offsets) {
    if (ofs >= (vuint32)strm->TotalSize()) continue;
    strm->Seek((int)ofs);
    char namebuf[9];
    strm->Serialise(namebuf, 8);
    if (strm->IsError()) { delete strm; return; }
    namebuf[8] = 0;
    txxnames.append(VName(namebuf, VName::AddLower), W_LumpFile(lump));
  }
  delete strm;
}


//==========================================================================
//
//  FillLists
//
//  fill `txxnames`, and `flatnames`
//
//==========================================================================
static void FillLists () {
  ClearLists();

  // collect all flats
  for (auto &&it : WadNSIterator(WADNS_Flats)) {
    //GCon->Logf(NAME_Debug, "FLAT: lump=%d; name=<%s> (%s); size=%d", it.lump, *it.getName(), *it.getFullName(), it.getSize());
    flatnames.append(it.getName(), it.getFile());
  }

  // parse all TEXTUREx lumps
  int totalTxx = 0;
  int lastTXXFile = -1;
  int lastTX1Lump = -1, lastTX2Lump = -1;
  for (auto &&it : WadNSIterator(WADNS_Global)) {
    if (it.getName() == NAME_texture1 || it.getName() == NAME_texture2) {
      //GCon->Logf("TXX: lump=%d; name=<%s> (%s); size=%d; file=%d (%d)", it.lump, *it.getName(), *it.getFullName(), it.getSize(), it.getFile(), lastTXXFile);
      if (lastTXXFile != it.getFile()) {
        if (lastTX1Lump >= 0 || lastTX2Lump >= 0) {
          if (lastTX1Lump >= 0) ++totalTxx;
          if (lastTX2Lump >= 0) ++totalTxx;
          ParseTXXLump(lastTX1Lump);
          ParseTXXLump(lastTX2Lump);
        }
        lastTX1Lump = -1;
        lastTX2Lump = -1;
        lastTXXFile = it.getFile();
      }
           if (it.getName() == NAME_texture1) { if (lastTX1Lump < 0) lastTX1Lump = it.lump; }
      else if (it.getName() == NAME_texture2) { if (lastTX2Lump < 0) lastTX2Lump = it.lump; }
      //GCon->Logf("  %d : %d", lastTX1Lump, lastTX2Lump);
    }
  }
  if (lastTX1Lump >= 0 || lastTX2Lump >= 0) {
    if (lastTX1Lump >= 0) ++totalTxx;
    if (lastTX2Lump >= 0) ++totalTxx;
    ParseTXXLump(lastTX1Lump);
    ParseTXXLump(lastTX2Lump);
  }

  GCon->Logf(NAME_Init, "%d flat%s found", flatnames.length(), (flatnames.length() != 1 ? "s" : ""));
  GCon->Logf(NAME_Init, "%d texture list%s found, total %d texture%s", totalTxx, (totalTxx != 1 ? "s" : ""), txxnames.length(), (txxnames.length() != 1 ? "s" : ""));
}


//==========================================================================
//
//  BuildTextureRange
//
//  scan pwads, and build texture range
//  clears `ids` on error (range too long, for example)
//  used in Boom ANIMATED parser
//
//  int txtype = (Type&1 ? TEXTYPE_Wall : TEXTYPE_Flat);
//
//==========================================================================
static void BuildTextureRange (int wadfile, VName nfirst, VName nlast, int txtype, TArray<int> &ids) {
  ids.clear();
  if (nfirst == NAME_None || nlast == NAME_None) {
    GCon->Logf(NAME_Warning, "ANIMATED: skipping animation sequence between '%s' and '%s'", *nfirst, *nlast);
    return;
  }

  const char *atypestr = (txtype == TEXTYPE_Flat ? "flat" : "texture");

  ListOfLumps &list = (txtype == TEXTYPE_Flat ? flatnames : txxnames);
  int firstIdx = -1, listLen = 0;
  int skipFile = -1;
  //GCon->Logf("***************** %d: <%s> : <%s>", wadfile, *nfirst, *nlast);
  for (auto &&it : list.named(nfirst)) {
    if (dbg_dump_animdef_ranges) GCon->Logf("  checking %d (%d : %s)", it.index(), it.getFile(), *it.getName());
    //k8: this seems to be unnecessary
    //!!!if (it.getFile() > wadfile) continue;
    if (it.getFile() == skipFile) continue;
    if (!W_IsWADFile(it.getFile())) continue; // ignore non-wad paks
    vassert(it.getName() == nfirst);
    // skip this file in any case
    skipFile = it.getFile();
    // find list end, set `firstIdx` and `listLen` if found
    int lend = list.findInFileFrom(it, nlast);
    if (lend >= 0) {
      // i found her!
      int len = lend+1-it.index();
      /*
      if (len > limit) {
        GCon->Logf(NAME_Warning, "ANIMATED: skipping %ss animation between '%s' and '%s' due to limits violation (%d, but only %d allowed)", atypestr, *nfirst, *nlast, len, limit);
      } else
      */
      {
        firstIdx = it.index();
        listLen = len;
      }
    }
  }

  // build ids
  ids.reset();
  while (listLen-- > 0) {
    const ListLump &lilu = list[firstIdx++];
    if (lilu.texName == NAME_None) continue;
    int tid = GTextureManager.CheckNumForName(lilu.texName, txtype, true);
    if (tid > 0) {
      ids.append(tid);
      if (dbg_dump_animdef_ranges) GCon->Logf(NAME_Init, "ANIM<%s : %s> %d (%d : %s)", *nfirst, *nlast, firstIdx-1, lilu.lumpFile, *lilu.texName);
    } else {
      GCon->Logf(NAME_Warning, "ANIMATED: %s '%s' not found in animation sequence between '%s' and '%s'", atypestr, *lilu.texName, *nfirst, *nlast);
    }
  }

  if (ids.length() == 0) {
    // can absent in doom1
    if (!dbg_dump_animdef_ranges) {
      if (nfirst == "swater1" && nlast == "swater4") return;
      if (nfirst == "rrock05" && nlast == "rrock08") return;
      if (nfirst == "slime01" && nlast == "slime04") return;
      if (nfirst == "slime05" && nlast == "slime08") return;
      if (nfirst == "slime09" && nlast == "slime12") return;
      if (nfirst == "bfall1" && nlast == "bfall4") return;
      if (nfirst == "sfall1" && nlast == "sfall4") return;
      if (nfirst == "dbrain1" && nlast == "dbrain4") return;
      // can absent in doom2
      if (nfirst == "swater1" && nlast == "swater4") return;
      if (nfirst == "blodgr1" && nlast == "blodgr4") return;
      if (nfirst == "sladrip1" && nlast == "sladrip3") return;
      // can absent both in doom1, and in doom2
      if (nfirst == "wfall1" && nlast == "wfall4") return;
    }
    GCon->Logf(NAME_Warning, "ANIMATED: %ss animation sequence between '%s' and '%s' not found", atypestr, *nfirst, *nlast);
  }
}


//==========================================================================
//
//  P_InitAnimated
//
//  Load the table of animation definitions, checking for existence of
//  the start and end of each frame. If the start doesn't exist the sequence
//  is skipped, if the last doesn't exist, BOOM exits.
//
//  Wall/Flat animation sequences, defined by name of first and last frame,
//  The full animation sequence is given using all lumps between the start
//  and end entry, in the order found in the WAD file.
//
//  This routine modified to read its data from a predefined lump or
//  PWAD lump called ANIMATED rather than a static table in this module to
//  allow wad designers to insert or modify animation sequences.
//
//  Lump format is an array of byte packed animdef_t structures, terminated
//  by a structure with istexture == -1. The lump can be generated from a
//  text source file using SWANTBLS.EXE, distributed with the BOOM utils.
//  The standard list of switches and animations is contained in the example
//  source text file DEFSWANI.DAT also in the BOOM util distribution.
//
//  k8: alas, there is no way to properly fix this, 'cause rely on WAD
//      ordering is fuckin' broken, and cannot be repaired. i'll try my
//      best, though.
//
//==========================================================================
void P_InitAnimated () {
  AnimDef_t ad;

  int animlump = W_CheckNumForName(NAME_animated);
  if (animlump < 0) return;
  GCon->Logf(NAME_Init, "loading Boom animated lump from '%s'...", *W_FullLumpName(animlump));

  VStream *lumpstream = W_CreateLumpReaderName(NAME_animated);
  VCheckedStream Strm(lumpstream);
  while (Strm.TotalSize()-Strm.Tell() >= 23) {
    //int pic1, pic2;
    vuint8 Type;
    char TmpName1[9];
    char TmpName2[9];
    vint32 BaseTime;

    memset(TmpName1, 0, sizeof(TmpName1));
    memset(TmpName2, 0, sizeof(TmpName2));

    Strm << Type;
    if (Type == 255) break; // terminator marker

    if (Type != 0 && Type != 1 && Type != 3) Sys_Error("P_InitPicAnims: bad type 0x%02x (ofs:0x%02x)", (vuint32)Type, (vuint32)(Strm.Tell()-1));

    Strm.Serialise(TmpName1, 9);
    Strm.Serialise(TmpName2, 9);
    Strm << BaseTime;

    if (!TmpName1[0] && !TmpName2[0]) {
      GCon->Log(NAME_Init, "ANIMATED: skipping empty entry");
      continue;
    }

    if (!TmpName2[0]) Sys_Error("P_InitPicAnims: empty first texture (ofs:0x%08x)", (vuint32)(Strm.Tell()-4-2*9-1));
    if (!TmpName1[0]) Sys_Error("P_InitPicAnims: empty second texture (ofs:0x%08x)", (vuint32)(Strm.Tell()-4-2*9-1));

    // 0 is flat, 1 is texture, 3 is texture with decals allowed
    int txtype = (Type&1 ? TEXTYPE_Wall : TEXTYPE_Flat);

    VName tn18 = VName(TmpName1, VName::AddLower8); // last
    VName tn28 = VName(TmpName2, VName::AddLower8); // first

    if (animPicSeen.find(tn18) || animPicSeen.find(tn28)) {
      GCon->Logf(NAME_Warning, "ANIMATED: skipping animation sequence between '%s' and '%s' due to animdef", TmpName2, TmpName1);
      continue;
    }

    // if we have seen this texture in animdef, skip the whole sequence
    TArray<int> ids;
    BuildTextureRange(W_LumpFile(animlump), tn28, tn18, txtype, ids);

    if (ids.length() == 1) {
      if (developer) GCon->Logf(NAME_Dev, "BOOMANIM: ignored zero-step animtex sequence ('%s' -- '%s')", TmpName2, TmpName1);
    }
    if (ids.length() < 2) continue; // nothing to do

    if (developer) GCon->Logf(NAME_Dev, "BOOMANIM: found animtex sequence ('%s' -- '%s'): %d (tics=%d)", TmpName2, TmpName1, ids.length(), BaseTime);

    memset(&ad, 0, sizeof(ad));
    //memset(&fd, 0, sizeof(fd));

    ad.StartFrameDef = FrameDefs.length();
    ad.range = 1; // this is ranged animation
    // we are always goind forward, indicies in framedefs will take care of the rest
    ad.Type = ANIM_Forward;
    ad.NumFrames = ids.length();

    // create frames
    for (int f = 0; f < ad.NumFrames; ++f) {
      FrameDef_t &fd = FrameDefs.alloc();
      memset((void *)&fd, 0, sizeof(FrameDef_t));
      fd.Index = ids[f];
      fd.BaseTime = BaseTime; // why not?
    }

    ad.CurrentFrame = ad.NumFrames-1; // so we'll "animate" to the first frame
    ad.Time = 0.0001f; // force 1st game tic to animate
    ad.allowDecals = (Type == 3);
    AnimDefs.Append(ad);
  }
}


//==========================================================================
//
//  FindFirstTextureInSequence
//
//==========================================================================
static bool FindFirstTextureInSequence (int wadfile, VName txname, int fttype, int &txbase) {
  txbase = -1;
  if (txname == NAME_None) return false;

  for (auto &&it : (fttype == FT_Flat ? flatnames.named(txname) : txxnames.named(txname))) {
    //k8: this seems to be unnecessary
    //!!!if (it.getFile() > wadfile) continue;
    txbase = it.index();
  }

  if (txbase < 0) {
    const char *atypestr = (fttype == FT_Flat ? "flat" : "texture");
    GCon->Logf(NAME_Warning, "%s texture '%s' not found for animation sequence", atypestr, *txname);
    return false;
  }

  //GCon->Logf("*** <%s> %d : %d", *txname, txlist, txbase);
  return true;
}


//==========================================================================
//
//  GetTextureIdWithOffset
//
//  `txlist` and `txbase` are list pointers
//
//==========================================================================
static int GetTextureIdWithOffset (int wadfile, int txbase, int offset, int fttype, bool optional) {
  //GCon->Logf("***   %d : %d : %d", txlist, txbase, offset);

  if (txbase < 0) return -1;
  if (offset < 0) return -1;

  ListOfLumps &list = (fttype == FT_Flat ? flatnames : txxnames);
  if (txbase >= list.length() || list.length()-txbase <= offset) return -1;
  //k8: this seems to be unnecessary
  //!!!if (list[txbase+offset].lumpFile != wadfile) return -1;

  VName txname = list[txbase+offset].texName;
  if (txname == NAME_None) return -1;

  int txtype = (fttype == FT_Flat ? TEXTYPE_Flat : TEXTYPE_Wall);
  const char *atypestr = (fttype == FT_Flat ? "flat" : "texture");

  int tid = GTextureManager.CheckNumForName(txname, txtype, true);
  if (tid == 0) return -1; // just in case
  if (tid < 0) {
    tid = GTextureManager.CheckNumForNameAndForce(txname, txtype, true, optional);
    if (tid <= 0) return -1;
    GCon->Logf(NAME_Warning, "force-loaded animation %s '%s' for animation sequence '%s'", atypestr, *txname, *list[txbase].texName);
  }

  if (dbg_dump_animdef_ranges) GCon->Logf(NAME_Init, "ANIM<%s+%d> (%d : %s)", *list[txbase].texName, offset, list[txbase+offset].lumpFile, *txname);

  //GCon->Logf("   1: <%s>", *txname);
  return (tid > 0 ? tid : -1);
}


//==========================================================================
//
//  ParseFTAnim
//
//  Parse flat or texture animation.
//
//==========================================================================
static void ParseFTAnim (int wadfile, VScriptParser *sc, int fttype) {
  AnimDef_t ad;
  FrameDef_t fd;

  int currStBase = -1;

  memset(&ad, 0, sizeof(ad));

  // optional flag
  bool optional = false;
  if (sc->Check("optional")) optional = true;

  // name
  bool ignore = false;
  sc->ExpectName8Warn();
  VName startTxName = sc->Name8;
  ad.Index = GTextureManager.CheckNumForNameAndForce(startTxName, (fttype == FT_Flat ? TEXTYPE_Flat : TEXTYPE_Wall), true, optional);
  if (ad.Index == -1) {
    ignore = true;
    if (!optional) GCon->Logf(NAME_Warning, "ANIMDEFS: Can't find texture \"%s\"", *sc->Name8);
  } else {
    animPicSeen.put(startTxName, true);
  }

  bool vanilla = false;
  float vanillaTics = 8.0f;
  if (sc->Check("vanilla")) {
    vanilla = true;
    if (sc->Check("tics")) {
      sc->ExpectFloat();
      vanillaTics = sc->Float;
      if (vanillaTics < 0.1f) vanillaTics = 0.1f; // this is tics
    }
  }

  int CurType = 0;
  ad.StartFrameDef = FrameDefs.length();
  ad.Type = ANIM_Forward;
  ad.allowDecals = 0;
  ad.range = (vanilla ? 1 : 0);
  TArray<int> ids;

  for (;;) {
    if (sc->Check("allowdecals")) {
      ad.allowDecals = 1;
      continue;
    }

    if (sc->Check("random")) {
      ad.Type = ANIM_Random;
      continue;
    }

    if (sc->Check("oscillate")) {
      ad.Type = ANIM_OscillateUp;
      continue;
    }

    if (sc->Check("pic")) {
      if (CurType == 2) sc->Error("You cannot use pic together with range.");
      CurType = 1;
    } else if (sc->Check("range")) {
      if (vanilla) sc->Error("Vanilla animations should use pic.");
      if (CurType == 2) sc->Error("You can only use range once in a single animation.");
      if (CurType == 1) sc->Error("You cannot use range together with pic.");
      CurType = 2;
      if (ad.Type != ANIM_Random) ad.Type = ANIM_Forward;
      ad.range = 1;
    } else {
      break;
    }

    memset(&fd, 0, sizeof(fd));

    if (vanilla) {
      sc->ExpectName8Warn();
      if (!ignore) {
        vassert(ad.range == 1);
        // simple pic
        vassert(CurType == 1);
        fd.Index = GTextureManager.CheckNumForNameAndForce(sc->Name8, (fttype == FT_Flat ? TEXTYPE_Flat : TEXTYPE_Wall), true, optional);
        if (fd.Index == -1 && !optional) sc->Message(va("Unknown texture \"%s\"", *sc->String));
        animPicSeen.put(sc->Name8, true);
        fd.BaseTime = vanillaTics;
        fd.RandomRange = 0;
      }
    } else {
      if (sc->CheckNumber()) {
        //if (developer) GCon->Logf(NAME_Dev, "%s: pic: num=%d", *sc->GetLoc().toStringNoCol(), sc->Number);
        if (!ignore && currStBase < 0) {
          if (!FindFirstTextureInSequence(wadfile, startTxName, fttype, currStBase)) {
            ignore = true;
          }
        }
        if (!ignore) {
          if (!ad.range) {
            // simple pic
            vassert(CurType == 1);
            if (sc->Number < 0) sc->Number = 1;
            int txidx = GetTextureIdWithOffset(wadfile, currStBase, sc->Number-1, fttype, optional);
            if (txidx == -1) {
              sc->Message(va("Cannot find %stexture '%s'+%d (pic)", (fttype == FT_Flat ? "flat " : ""), *startTxName, sc->Number-1));
            } else {
              animPicSeen.put(GTextureManager.GetTextureName(txidx), true);
            }
            fd.Index = txidx;
          } else {
            // range
            vassert(CurType == 2);
            // create frames
            for (int ofs = 0; ofs <= sc->Number; ++ofs) {
              int txidx = GetTextureIdWithOffset(wadfile, currStBase, ofs, fttype, optional);
              if (txidx == -1) {
                sc->Message(va("Cannot find %stexture '%s'+%d (range)", (fttype == FT_Flat ? "flat " : ""), *GTextureManager.GetTextureName(ad.Index), ofs));
              } else {
                animPicSeen.put(GTextureManager.GetTextureName(txidx), true);
                ids.append(txidx);
              }
            }
          }
        }
      } else {
        sc->ExpectName8Warn();
        //if (developer) GCon->Logf(NAME_Dev, "%s: pic: name=%s", *sc->GetLoc().toStringNoCol(), *sc->Name8);
        if (!ignore) {
          if (!ad.range) {
            // simple pic
            vassert(CurType == 1);
            fd.Index = GTextureManager.CheckNumForNameAndForce(sc->Name8, (fttype == FT_Flat ? TEXTYPE_Flat : TEXTYPE_Wall), true, optional);
            if (fd.Index == -1 && !optional) sc->Message(va("Unknown texture \"%s\"", *sc->String));
            animPicSeen.put(sc->Name8, true);
          } else {
            // range
            vassert(CurType == 2);
            int txtype = (fttype == FT_Flat ? TEXTYPE_Flat : TEXTYPE_Wall);
            BuildTextureRange(wadfile, GTextureManager.GetTextureName(ad.Index), sc->Name8, txtype, ids);
            for (int f = 0; f < ids.length(); ++f) animPicSeen.put(GTextureManager.GetTextureName(ids[f]), true);
          }
        }
      }
    }

    if (sc->Check("tics")) {
      sc->ExpectFloat();
      fd.BaseTime = sc->Float;
      if (fd.BaseTime < 0.1f) fd.BaseTime = 0.1f; // this is tics
      fd.RandomRange = 0;
    } else if (sc->Check("rand")) {
      sc->ExpectNumber(true);
      fd.BaseTime = sc->Number;
      sc->ExpectNumber(true);
      fd.RandomRange = sc->Number-(int)fd.BaseTime+1;
      if (fd.RandomRange < 0) {
        sc->Message("ignored invalid random range");
        fd.RandomRange = 0;
      }
    } else {
      if (!vanilla) sc->Error(va("bad command (%s)", *sc->String));
    }

    if (ignore) continue;

    // create range frames, if necessary
    if (CurType == 2) {
      vassert(ad.range == 1);
      if (ids.length() == 0) continue; // nothing to do
      for (int f = 0; f < ids.length(); ++f) {
        FrameDef_t &nfd = FrameDefs.alloc();
        nfd = fd;
        nfd.Index = ids[f];
      }
    } else {
      // this is simple pic
      vassert(CurType == 1);
      vassert(ad.range == 0 || vanilla);
      if (fd.Index != -1) FrameDefs.Append(fd);
    }
  }

  if (!ignore && FrameDefs.length() > ad.StartFrameDef) {
    ad.NumFrames = FrameDefs.length()-ad.StartFrameDef;
    ad.CurrentFrame = (ad.Type != ANIM_Random ? ad.NumFrames-1 : (int)(Random()*ad.NumFrames));
    ad.Time = 0.0001f; // force 1st game tic to animate
    AnimDefs.Append(ad);
  } else if (!ignore && !optional) {
    // report something here
  }
}


//==========================================================================
//
//  AddSwitchDef
//
//==========================================================================
static int AddSwitchDef (TSwitch *Switch) {
//meh, RAM is cheap; besides, replacing switches may cause problems
#if 0
  for (auto &&it : Switches.itemsIdx()) {
    TSwitch *sw = it.value();
    if (sw->Tex == Switch->Tex) {
      if (sw != Switch) {
        delete Switches[it.index()];
        Switches[it.index()] = Switch;
      }
      return it.index();
    }
  }
#endif
  return Switches.Append(Switch);
}


//==========================================================================
//
//  ParseSwitchState
//
//==========================================================================
static TSwitch *ParseSwitchState (VScriptParser *sc, bool IgnoreBad) {
  TArray<TSwitchFrame> Frames;
  int Sound = 0;
  bool Bad = false;
  bool silentTexError = (GArgs.CheckParm("-Wswitch-textures") == 0);

  //GCon->Logf("+============+");
  while (1) {
    if (sc->Check("sound")) {
      if (Sound) sc->Error("Switch state already has a sound");
      sc->ExpectString();
      Sound = GSoundManager->GetSoundID(*sc->String);
    } else if (sc->Check("pic")) {
      sc->ExpectName8Warn();
      int Tex = GTextureManager.CheckNumForNameAndForce(sc->Name8, TEXTYPE_Wall, true, /*false*/IgnoreBad || silentTexError);
      if (Tex < 0 && !IgnoreBad) Bad = true;
      TSwitchFrame &F = Frames.Alloc();
      F.Texture = Tex;
      if (sc->Check("tics")) {
        sc->ExpectNumber(true);
        F.BaseTime = sc->Number;
        F.RandomRange = 0;
      } else if (sc->Check("range")) {
        sc->ExpectNumber();
        int Min = sc->Number;
        sc->ExpectNumber();
        int Max = sc->Number;
        if (Min < Max) {
          F.BaseTime = Min;
          F.RandomRange = Max-Min+1;
        } else {
          F.BaseTime = Max;
          F.RandomRange = Min-Max+1;
        }
      } else {
        sc->Error("Must specify a duration for switch frame");
      }
    } else {
      break;
    }
  }
  //GCon->Logf("*============*");

  if (!Frames.length()) sc->Error("Switch state needs at least one frame");
  if (Bad) return nullptr;

  TSwitch *Def = new TSwitch();
  Def->Sound = Sound;
  Def->NumFrames = Frames.length();
  Def->Frames = new TSwitchFrame[Frames.length()];
  for (int i = 0; i < Frames.length(); ++i) {
    Def->Frames[i].Texture = Frames[i].Texture;
    Def->Frames[i].BaseTime = Frames[i].BaseTime;
    Def->Frames[i].RandomRange = Frames[i].RandomRange;
  }
  return Def;
}


//==========================================================================
//
//  ParseSwitchDef
//
//==========================================================================
static void ParseSwitchDef (VScriptParser *sc) {
  bool silentTexError = (GArgs.CheckParm("-Wswitch-textures") == 0);

  // skip game specifier
       if (sc->Check("doom")) { /*sc->ExpectNumber();*/ sc->CheckNumber(); }
  else if (sc->Check("heretic")) {}
  else if (sc->Check("hexen")) {}
  else if (sc->Check("strife")) {}
  else if (sc->Check("any")) {}

  // switch texture
  sc->ExpectName8Warn();
  int t1 = GTextureManager.CheckNumForNameAndForce(sc->Name8, TEXTYPE_Wall, true, silentTexError);
  bool Quest = false;
  TSwitch *Def1 = nullptr;
  TSwitch *Def2 = nullptr;

  // currently only basic switch definition is supported
  while (1) {
    if (sc->Check("quest")) {
      Quest = true;
    } else if (sc->Check("on")) {
      if (Def1) sc->Error("Switch already has an on state");
      Def1 = ParseSwitchState(sc, t1 == -1);
    } else if (sc->Check("off")) {
      if (Def2) sc->Error("Switch already has an off state");
      Def2 = ParseSwitchState(sc, t1 == -1);
    } else {
      break;
    }
  }

  if (t1 < 0 || !Def1) {
    if (Def1) delete Def1;
    if (Def2) delete Def2;
    return;
  }

  if (!Def2) {
    // if switch has no off state create one that just switches back to base texture
    Def2 = new TSwitch();
    Def2->Sound = Def1->Sound;
    Def2->NumFrames = 1;
    Def2->Frames = new TSwitchFrame[1];
    Def2->Frames[0].Texture = t1;
    Def2->Frames[0].BaseTime = 0;
    Def2->Frames[0].RandomRange = 0;
  }

  Def1->Tex = t1;
  Def2->Tex = Def1->Frames[Def1->NumFrames-1].Texture;
  if (Def1->Tex == Def2->Tex) {
    sc->Error("On state must not end on base texture");
    //sc->Message("On state must not end on base texture");
  }
  Def1->Quest = Quest;
  Def2->Quest = Quest;
  Def2->PairIndex = AddSwitchDef(Def1);
  Def1->PairIndex = AddSwitchDef(Def2);
}


//==========================================================================
//
//  ParseAnimatedDoor
//
//==========================================================================
static void ParseAnimatedDoor (VScriptParser *sc) {
  // get base texture name
  bool ignore = false;
  sc->ExpectName8Warn();
  vint32 BaseTex = GTextureManager.CheckNumForNameAndForce(sc->Name8, TEXTYPE_Wall, true, false);
  if (BaseTex == -1) {
    ignore = true;
    GCon->Logf(NAME_Warning, "ANIMDEFS: Can't find animdoor texture \"%s\"", *sc->String);
  }

  VName OpenSound(NAME_None);
  VName CloseSound(NAME_None);
  TArray<vint32> Frames;
  while (!sc->AtEnd()) {
    if (sc->Check("opensound")) {
      sc->ExpectString();
      OpenSound = *sc->String;
    } else if (sc->Check("closesound")) {
      sc->ExpectString();
      CloseSound = *sc->String;
    } else if (sc->Check("pic")) {
      vint32 v;
      if (sc->CheckNumber()) {
        v = BaseTex+sc->Number-1;
      } else {
        sc->ExpectName8Warn();
        v = GTextureManager.CheckNumForNameAndForce(sc->Name8, TEXTYPE_Wall, true, false);
        if (v == -1 && !ignore) sc->Message(va("Unknown texture %s", *sc->String));
      }
      Frames.Append(v);
    } else {
      break;
    }
  }

  if (!ignore) {
    VAnimDoorDef &A = AnimDoorDefs.Alloc();
    A.Texture = BaseTex;
    A.OpenSound = OpenSound;
    A.CloseSound = CloseSound;
    A.NumFrames = Frames.length();
    A.Frames = new vint32[Frames.length()];
    for (int i = 0; i < A.NumFrames; i++) A.Frames[i] = Frames[i];
  }
};


//==========================================================================
//
//  ParseWarp
//
//==========================================================================
static void ParseWarp (VScriptParser *sc, int Type) {
  int TexType = TEXTYPE_Wall;
       if (sc->Check("texture")) TexType = TEXTYPE_Wall;
  else if (sc->Check("flat")) TexType = TEXTYPE_Flat;
  else sc->Error("Texture type expected");

  sc->ExpectName8Warn();
  int TexNum = GTextureManager.CheckNumForNameAndForce(sc->Name8, TexType, true, false);
  if (TexNum < 0) return;

  float speed = 1;
  if (sc->CheckFloat()) speed = sc->Float;

  VTexture *SrcTex = GTextureManager[TexNum];
  VTexture *WarpTex = SrcTex;
  // warp only once
  if (!SrcTex->WarpType) {
    if (Type == 1) {
      WarpTex = new VWarpTexture(SrcTex, speed);
    } else {
      WarpTex = new VWarp2Texture(SrcTex, speed);
    }
    GTextureManager.ReplaceTexture(TexNum, WarpTex);
  }
  if (WarpTex) {
    WarpTex->noDecals = true;
    WarpTex->staticNoDecals = true;
    WarpTex->animNoDecals = true;
  }
  if (sc->Check("allowdecals")) {
    if (WarpTex) {
      WarpTex->noDecals = false;
      WarpTex->staticNoDecals = false;
      WarpTex->animNoDecals = false;
    }
  }
}


//==========================================================================
//
//  ParseCameraTexture
//
//==========================================================================
static void ParseCameraTexture (VScriptParser *sc) {
  // name
  sc->ExpectName(); // was 8
  VName Name = NAME_None;
  if (VStr::Length(*sc->Name) > 8) {
    GCon->Logf(NAME_Warning, "cameratexture texture name too long (\"%s\")", *sc->Name);
  }
  Name = sc->Name;
  // dimensions
  sc->ExpectNumber();
  int Width = sc->Number;
  sc->ExpectNumber();
  int Height = sc->Number;
  int FitWidth = Width;
  int FitHeight = Height;

  VCameraTexture *Tex = nullptr;
  if (Name != NAME_None) {
    // check for replacing an existing texture
    Tex = new VCameraTexture(Name, Width, Height);
    int TexNum = GTextureManager.CheckNumForNameAndForce(Name, TEXTYPE_Flat, true, false);
    if (TexNum != -1) {
      // by default camera texture will fit in old texture
      VTexture *OldTex = GTextureManager[TexNum];
      FitWidth = OldTex->GetScaledWidth();
      FitHeight = OldTex->GetScaledHeight();
      GTextureManager.ReplaceTexture(TexNum, Tex);
      delete OldTex;
      OldTex = nullptr;
    } else {
      GTextureManager.AddTexture(Tex);
    }
  }

  // optionally specify desired scaled size
  if (sc->Check("fit")) {
    sc->ExpectNumber();
    FitWidth = sc->Number;
    sc->ExpectNumber();
    FitHeight = sc->Number;
  }

  if (Tex) {
    Tex->SScale = (float)Width/(float)FitWidth;
    Tex->TScale = (float)Height/(float)FitHeight;
  }
}


//==========================================================================
//
//  ParseFTAnims
//
//  Initialise flat and texture animation lists.
//
//==========================================================================
static void ParseFTAnims (int wadfile, VScriptParser *sc) {
  while (!sc->AtEnd()) {
         if (sc->Check("flat")) ParseFTAnim(wadfile, sc, true);
    else if (sc->Check("texture")) ParseFTAnim(wadfile, sc, false);
    else if (sc->Check("switch")) ParseSwitchDef(sc);
    else if (sc->Check("animateddoor")) ParseAnimatedDoor(sc);
    else if (sc->Check("warp")) ParseWarp(sc, 1);
    else if (sc->Check("warp2")) ParseWarp(sc, 2);
    else if (sc->Check("cameratexture")) ParseCameraTexture(sc);
    else sc->Error(va("bad command (%s)", *sc->String));
  }
  delete sc;
  sc = nullptr;
}


//==========================================================================
//
//  R_InitFTAnims
//
//  Initialise flat and texture animation lists.
//
//==========================================================================
void R_InitFTAnims () {
  if (GArgs.CheckParm("-disable-animdefs")) return;

  FillLists();

  // process all animdefs lumps
  for (auto &&it : WadNSNameIterator(NAME_animdefs, WADNS_Global)) {
    GCon->Logf(NAME_Init, "parsing ANIMDEF from '%s'...", *it.getFullName());
    ParseFTAnims(W_LumpFile(it.lump), new VScriptParser(it.getFullName(), W_CreateLumpReaderNum(it.lump)));
  }

  // read Boom's animated lump if present
  // do it here, so we can skip already animated textures
  if (GArgs.CheckParm("-no-boom-animated") == 0) P_InitAnimated();

  animPicSeen.clear();

  FrameDefs.Condense();
  AnimDefs.Condense();

  // build `animTexMap`
  {
    const int len = AnimDefs.length();
    for (int i = 0; i < len; ++i) {
      AnimDef_t &ad = AnimDefs[i];
      if (!ad.range) {
        animTexMap.put(ad.Index, true);
      } else {
        for (int fi = 0; fi < ad.NumFrames; ++fi) {
          animTexMap.put(FrameDefs[ad.StartFrameDef+fi].Index, true);
        }
      }
    }
  }

  ClearLists();
}


//==========================================================================
//
//  P_InitSwitchList
//
//  Only called at game initialization.
//  Parse BOOM style switches lump.
//
//==========================================================================
void P_InitSwitchList () {
  int lump = W_CheckNumForName(NAME_switches);
  if (lump != -1) {
    VStream *lumpstream = W_CreateLumpReaderNum(lump);
    VCheckedStream Strm(lumpstream);
    while (Strm.TotalSize()-Strm.Tell() >= 20) {
      char TmpName1[9];
      char TmpName2[9];
      vint16 Episode;

      // read data
      Strm.Serialise(TmpName1, 9);
      Strm.Serialise(TmpName2, 9);
      Strm << Episode;
      if (!Episode) break; // terminator marker
      TmpName1[8] = 0;
      TmpName2[8] = 0;

      // Check for switches that aren't really switches
      if (!VStr::ICmp(TmpName1, TmpName2)) {
        GCon->Logf(NAME_Warning, "Switch \"%s\" in SWITCHES has the same 'on' state", TmpName1);
        continue;
      }
      int t1 = GTextureManager.CheckNumForNameAndForce(VName(TmpName1, VName::AddLower8), TEXTYPE_Wall, true, false);
      int t2 = GTextureManager.CheckNumForNameAndForce(VName(TmpName2, VName::AddLower8), TEXTYPE_Wall, true, false);
      if (t1 < 0 || t2 < 0) continue;
      TSwitch *Def1 = new TSwitch();
      TSwitch *Def2 = new TSwitch();
      Def1->Sound = 0;
      Def2->Sound = 0;
      Def1->Tex = t1;
      Def2->Tex = t2;
      Def1->NumFrames = 1;
      Def2->NumFrames = 1;
      Def1->Quest = false;
      Def2->Quest = false;
      Def1->Frames = new TSwitchFrame[1];
      Def2->Frames = new TSwitchFrame[1];
      Def1->Frames[0].Texture = t2;
      Def1->Frames[0].BaseTime = 0;
      Def1->Frames[0].RandomRange = 0;
      Def2->Frames[0].Texture = t1;
      Def2->Frames[0].BaseTime = 0;
      Def2->Frames[0].RandomRange = 0;
      Def2->PairIndex = AddSwitchDef(Def1);
      Def1->PairIndex = AddSwitchDef(Def2);
    }
  }
  Switches.Condense();
}


//==========================================================================
//
//  R_FindAnimDoor
//
//==========================================================================
VAnimDoorDef *R_FindAnimDoor (vint32 BaseTex) {
  for (int i = 0; i < AnimDoorDefs.length(); ++i) {
    if (AnimDoorDefs[i].Texture == BaseTex) return &AnimDoorDefs[i];
  }
  return nullptr;
}


//==========================================================================
//
//  R_IsAnimatedTexture
//
//==========================================================================
bool R_IsAnimatedTexture (int texid) {
  if (texid < 1 || GTextureManager.IsMapLocalTexture(texid)) return false;
  VTexture *tx = GTextureManager[texid];
  if (!tx) return false;
  return animTexMap.has(texid);
}


//==========================================================================
//
//  R_AnimateSurfaces
//
//==========================================================================
#ifdef CLIENT
static float lastSurfAnimGameTime = 0;

void R_AnimateSurfaces () {
  if (!GClLevel) {
    lastSurfAnimGameTime = 0;
    return;
  }
  const float dtime = GClLevel->Time-lastSurfAnimGameTime;
  // if less than zero, it means that we're started a new map; do not animate
  if (dtime > 0.0f) {
    // animate flats and textures
    for (int i = 0; i < AnimDefs.length(); ++i) {
      AnimDef_t &ad = AnimDefs[i];
      //ad.Time -= host_frametime;
      ad.Time -= dtime;
      for (int trycount = 128; trycount > 0; --trycount) {
        if (ad.Time > 0.0f) break;

        bool validAnimation = true;
        if (ad.NumFrames > 1) {
          switch (ad.Type) {
            case ANIM_Forward:
              ad.CurrentFrame = (ad.CurrentFrame+1)%ad.NumFrames;
              break;
            case ANIM_Backward:
              ad.CurrentFrame = (ad.CurrentFrame+ad.NumFrames-1)%ad.NumFrames;
              break;
            case ANIM_OscillateUp:
              if (++ad.CurrentFrame >= ad.NumFrames-1) {
                ad.Type = ANIM_OscillateDown;
                ad.CurrentFrame = ad.NumFrames-1;
              }
              break;
            case ANIM_OscillateDown:
              if (--ad.CurrentFrame <= 0) {
                ad.Type = ANIM_OscillateUp;
                ad.CurrentFrame = 0;
              }
              break;
            case ANIM_Random:
              if (ad.NumFrames > 1) ad.CurrentFrame = (int)(Random()*ad.NumFrames);
              break;
            default:
              fprintf(stderr, "unknown animation type for texture %d (%s): %d\n", ad.Index, *GTextureManager[ad.Index]->Name, (int)ad.Type);
              validAnimation = false;
              ad.CurrentFrame = 0;
              break;
          }
        } else {
          ad.CurrentFrame = 0;
        }
        if (!validAnimation) continue;

        const FrameDef_t &fd = FrameDefs[ad.StartFrameDef+(ad.range ? 0 : ad.CurrentFrame)];

        ad.Time += fd.BaseTime/35.0f;
        if (fd.RandomRange) ad.Time += Random()*(fd.RandomRange/35.0f); // random tics

        if (!ad.range) {
          // simple case
          VTexture *atx = GTextureManager[ad.Index];
          if (atx) {
            atx->noDecals = (ad.allowDecals == 0);
            atx->animNoDecals = (ad.allowDecals == 0);
            atx->animated = true;
            // protect against missing textures
            if (fd.Index != -1) {
              atx->TextureTranslation = fd.Index;
            }
          }
        } else {
          // range animation, hard case; see... "explanation" at the top of this file
          FrameDef_t *fdp = &FrameDefs[ad.StartFrameDef];
          for (int currfdef = 0; currfdef < ad.NumFrames; ++currfdef, ++fdp) {
            VTexture *atx = GTextureManager[fdp->Index];
            if (!atx) continue;
            atx->noDecals = (ad.allowDecals == 0);
            atx->animNoDecals = (ad.allowDecals == 0);
            atx->animated = true;
            int afdidx = ad.StartFrameDef+(currfdef+ad.CurrentFrame)%ad.NumFrames;
            if (FrameDefs[afdidx].Index < 1) continue;
            atx->TextureTranslation = FrameDefs[afdidx].Index;
          }
        }
      }
    }
  }
  lastSurfAnimGameTime = GClLevel->Time;
}
#endif
