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

/*k8
  range animations are tricky: they depend on wad ordering,
  and this is so fuckin' broken, that i don't even know where to start.
  ok, for range animation, we have to do this: for each texture in range,
  we should use its current offset with currentframe...
  fuck. it is so fuckin' broken, that i cannot even explain it.
  ok, take two.
  for range animations, `Index` in `AnimDef_t` doesn't matter at all.
  we have to go through all `FrameDef_t` (from `StartFrameDef`, and up to
  `StartFrameDef+NumFrames`, use `currfdef` as index), and do this:
    for texture with index from `Index` field of framedef, calculate another
    framedef number as:
      afdidx = (currfdef-StartFrameDef+CurrentFrame)%NumFrames
    and use `Index` from `afdidx` as current texture step
    the only timing that matters is timing info in `StartFrameDef`.
  still cannot understand a fuckin' shit? me too. but this is how i did it.
 */


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

// partially parsed from TEXTUREx lumps
static TArray<VStr> txxnames;
// collected from loaded wads
static TArray<VStr> flatnames;


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
//  FillLists
//
//  fill `txxnames`, and `flatnames`
//
//==========================================================================
static void FillLists () {
  txxnames.clear();
  flatnames.clear();

  /*
  // collect all flats
  for (auto &&it : WadNSIterator(WADNS_Flats)) {
    GCon->Logf("FLAT: lump=%d; name=<%s> (%s); size=%d", it.lump, *it.getName(), *it.getFullName(), it.getSize());
  }

  // parse all TEXTUREx lumps
  for (auto &&it : WadNSIterator(WADNS_Global)) {
    if (it.getName() == NAME_texture1 || it.getName() == NAME_texture2) {
      GCon->Logf("TEXTUREx: lump=%d; name=<%s> (%s)", it.lump, *it.getName(), *it.getFullName());
    }
  }
  */
}


//==========================================================================
//
//  BuildTextureRange
//
//  scan pwads, and build texture range
//  clears `ids` on error (range too long, for example)
//
//  int txtype = (Type&1 ? TEXTYPE_Wall : TEXTYPE_Flat);
//
//==========================================================================
static void BuildTextureRange (VName nfirst, VName nlast, int txtype, TArray<int> &ids, int limit=64, bool checkadseen=false) {
  ids.clear();
  if (nfirst == NAME_None || nlast == NAME_None) {
    GCon->Logf(NAME_Warning, "ANIMATED: skipping animation sequence between '%s' and '%s'", *nfirst, *nlast);
    return;
  }

  EWadNamespace txns = (txtype == TEXTYPE_Flat ? WADNS_Flats : WADNS_Global);
  int pic1lmp = W_FindFirstLumpOccurence(nfirst, txns);
  if (pic1lmp == -1 && txtype == TEXTYPE_Flat) {
    pic1lmp = W_FindFirstLumpOccurence(nfirst, WADNS_NewTextures);
    if (pic1lmp >= 0) txns = WADNS_NewTextures;
  }
  int pic2lmp = W_FindFirstLumpOccurence(nlast, txns);
  if (pic1lmp == -1 && pic2lmp == -1) return; // invalid episode

  if (pic1lmp == -1) {
    if (developer) GCon->Logf(NAME_Dev, "BOOMANIM: first animtex '%s' not found (but '%s' is found)", *nfirst, *nlast);
    return;
  } else if (pic2lmp == -1) {
    if (developer) GCon->Logf(NAME_Dev, "BOOMANIM: second animtex '%s' not found (but '%s' is found)", *nlast, *nfirst);
    return;
  }

  if (GTextureManager.CheckNumForName(W_LumpName(pic1lmp), txtype, true) <= 0) {
    if (developer) GCon->Logf(NAME_Dev, "BOOMANIM: first animtex '%s' not a texture", *nfirst);
    return;
  }

  if (GTextureManager.CheckNumForName(W_LumpName(pic2lmp), txtype, true) <= 0) {
    if (developer) GCon->Logf(NAME_Dev, "BOOMANIM: second animtex '%s' not a texture", *nlast);
    return;
  }

  check(pic1lmp != -1);
  check(pic2lmp != -1);

  bool backward = (pic2lmp < pic1lmp);

  int start = (backward ? pic2lmp : pic1lmp);
  int end = (backward ? pic1lmp : pic2lmp);
  check(start <= end);

  // try to find common texture prefix, to filter out accidental shit introduced by pwads
  VStr pfx;
  {
    const char *n0 = *nfirst;
    const char *n1 = *nlast;
    while (*n0 && *n1) {
      if (*n0 != *n1) break;
      pfx += *n0;
      ++n0;
      ++n1;
    }
  }

  if (developer) GCon->Logf(NAME_Dev, "=== %s : %s === (0x%08x : 0x%08x) [%s] (0x%08x -> 0x%08x; ns=%d)", *nfirst, *nlast, pic1lmp, pic2lmp, *pfx, start, end, txns);
  // find all textures in animation (up to arbitrary limit)
  // it is safe to not check for `-1` here, as it is guaranteed that the last texture is present
  for (; start <= end; start = W_IterateNS(start, txns)) {
    check(start != -1); // should not happen
    if (developer) GCon->Logf(NAME_Dev, "  lump: 0x%08x; name=<%s> : <%s>", start, *W_LumpName(start), *W_FullLumpName(start));
    // check prefix
    if (pfx.length()) {
      const char *lname = *W_LumpName(start);
      if (!VStr::startsWith(lname, *pfx)) {
        if (developer) GCon->Logf(NAME_Dev, "    PFX SKIP: %s : 0x%08x (0x%08x)", lname, start, end);
        continue;
      }
    }
    int txidx = GTextureManager.CheckNumForName(W_LumpName(start), txtype, true);
    if (developer) {
      GCon->Logf(NAME_Dev, "  %s : 0x%08x (0x%08x)", (txidx == -1 ? "----" : *GTextureManager.GetTextureName(txidx)), start, end);
    }
    if (txidx == -1) continue;
    // if we have seen this texture in animdef, skip the whole sequence
    if (checkadseen) {
      if (animPicSeen.has(W_LumpName(start))) {
        if (developer) GCon->Logf(NAME_Dev, " SEEN IN ANIMDEF, SKIPPED");
        ids.clear();
        return;
      }
    }
    // check for overlong sequences
    if (ids.length() > limit) {
      if (developer) GCon->Logf(NAME_Dev, "BOOMANIM: too long animtex sequence ('%s' -- '%s')", *nfirst, *nlast);
      ids.clear();
      return;
    }
    ids.append(txidx);
    if (start == end) break;
  }

  if (backward && ids.length() > 1) {
    // reverse list
    for (int f = 0; f < ids.length()/2; ++f) {
      int nidx = ids.length()-f-1;
      int tmp = ids[f];
      ids[f] = ids[nidx];
      ids[nidx] = tmp;
    }
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
//  k8: this is horribly broken with PWADs. what i will try to do to fix it
//      is to check pwad with the earliest texture found, and try to build
//      a sequence with names extracted from it. this is not ideal, but
//      should fix some broken shit.
//
//      alas, there is no way to properly fix this, 'cause rely on WAD
//      ordering is fuckin' broken, and cannot be repaired. i'll try my
//      best, though.
//
//==========================================================================
void P_InitAnimated () {
  AnimDef_t ad;

  int animlump = W_CheckNumForName(NAME_animated);
  if (animlump < 0) return;
  GCon->Logf(NAME_Init, "loading Boom animated lump from '%s'", *W_FullLumpName(animlump));

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
    BuildTextureRange(tn28, tn18, txtype, ids, 32, true); // limit to 32 frames

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
//  GetTextureIdWithOffset
//
//==========================================================================
static int GetTextureIdWithOffset (int txbase, int offset, int fttype) {
  //if (developer) GCon->Logf(NAME_Dev, "GetTextureIdWithOffset: txbase=%d; offset=%d; IsFlat=%d", txbase, offset, IsFlat);
  if (txbase <= 0) return -1; // oops
  if (offset < 0) return -1; // oops
  if (offset == 0) return txbase;
  int txtype = (fttype == FT_Flat ? TEXTYPE_Flat : TEXTYPE_Wall);
  EWadNamespace txns = (fttype == FT_Flat ? WADNS_Flats : WADNS_Global);
  VName txname = GTextureManager.GetTextureName(txbase);
  if (txname == NAME_None) {
    //if (developer) GCon->Logf(NAME_Dev, "GetTextureIdWithOffset: FOOO (txbase=%d; offset=%d; name=%s)", txbase, offset, *txname);
    return -1; // oops
  }
  int lmp = W_FindFirstLumpOccurence(txname, txns);
  if (lmp == -1 && fttype != FT_Flat) {
    lmp = W_FindFirstLumpOccurence(txname, WADNS_NewTextures);
    if (lmp >= 0) txns = WADNS_NewTextures;
  }
  if (lmp == -1) {
    //if (developer) GCon->Logf(NAME_Dev, "GetTextureIdWithOffset: cannot find first lump (txbase=%d; offset=%d; name=%s)", txbase, offset, *txname);
    return -1; // oops
  }
  // now scan loaded paks until we skip enough textures
  for (;;) {
    lmp = W_IterateNS(lmp, txns); // skip one lump
    if (lmp == -1) break; // oops
    VName lmpName = W_LumpName(lmp);
    if (lmpName == NAME_None) continue;
    int txidx = GTextureManager.CheckNumForName(lmpName, txtype, true);
    if (txidx == -1) {
      //if (developer) GCon->Logf(NAME_Dev, "GetTextureIdWithOffset: trying to force-load lump 0x%08x (%s)", lmp, *lmpName);
      // not a texture, try to load one
      VTexture *tx = VTexture::CreateTexture(txtype, lmp);
      if (tx) {
        tx->Name = lmpName;
        GCon->Logf(NAME_Warning, "force-loaded animation texture '%s'", *tx->Name);
        txidx = GTextureManager.AddTexture(tx);
        check(txidx > 0);
      } else {
        // not a texture
        continue;
      }
    }
    //if (developer) GCon->Logf(NAME_Dev, "GetTextureIdWithOffset: txbase=%d; offset=%d; txidx=%d; txname=%s; lmpname=%s", txbase, offset, txidx, *GTextureManager.GetTextureName(txidx), *lmpName);
    if (--offset == 0) return txidx;
  }
  return -1; // not found
}


//==========================================================================
//
//  ParseFTAnim
//
//  Parse flat or texture animation.
//
//==========================================================================
static void ParseFTAnim (VScriptParser *sc, int fttype) {
  AnimDef_t ad;
  FrameDef_t fd;

  memset(&ad, 0, sizeof(ad));

  // optional flag
  bool optional = false;
  if (sc->Check("optional")) optional = true;

  // name
  bool ignore = false;
  sc->ExpectName8Warn();
  ad.Index = GTextureManager.CheckNumForNameAndForce(sc->Name8, (fttype == FT_Flat ? TEXTYPE_Flat : TEXTYPE_Wall), true, optional);
  if (ad.Index == -1) {
    ignore = true;
    if (!optional) GCon->Logf(NAME_Warning, "ANIMDEFS: Can't find texture \"%s\"", *sc->Name8);
  } else {
    animPicSeen.put(sc->Name8, true);
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
        check(ad.range == 1);
        // simple pic
        check(CurType == 1);
        fd.Index = GTextureManager.CheckNumForNameAndForce(sc->Name8, (fttype == FT_Flat ? TEXTYPE_Flat : TEXTYPE_Wall), true, optional);
        if (fd.Index == -1 && !optional) sc->Message(va("Unknown texture \"%s\"", *sc->String));
        animPicSeen.put(sc->Name8, true);
        fd.BaseTime = vanillaTics;
        fd.RandomRange = 0;
      }
    } else {
      if (sc->CheckNumber()) {
        //if (developer) GCon->Logf(NAME_Dev, "%s: pic: num=%d", *sc->GetLoc().toStringNoCol(), sc->Number);
        if (!ignore) {
          if (!ad.range) {
            // simple pic
            check(CurType == 1);
            if (sc->Number < 0) sc->Number = 1;
            int txidx = GetTextureIdWithOffset(ad.Index, sc->Number-1, fttype);
            if (txidx == -1) {
              sc->Message(va("Cannot find %stexture '%s'+%d", (fttype == FT_Flat ? "flat " : ""), *GTextureManager.GetTextureName(ad.Index), sc->Number-1));
            } else {
              animPicSeen.put(GTextureManager.GetTextureName(txidx), true);
            }
            fd.Index = txidx;
          } else {
            // range
            check(CurType == 2);
            if (!ignore) {
              // create frames
              for (int ofs = 0; ofs <= sc->Number; ++ofs) {
                int txidx = GetTextureIdWithOffset(ad.Index, ofs, fttype);
                if (txidx == -1) {
                  sc->Message(va("Cannot find %stexture '%s'+%d", (fttype == FT_Flat ? "flat " : ""), *GTextureManager.GetTextureName(ad.Index), ofs));
                } else {
                  animPicSeen.put(GTextureManager.GetTextureName(txidx), true);
                  ids.append(txidx);
                }
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
            check(CurType == 1);
            fd.Index = GTextureManager.CheckNumForNameAndForce(sc->Name8, (fttype == FT_Flat ? TEXTYPE_Flat : TEXTYPE_Wall), true, optional);
            if (fd.Index == -1 && !optional) sc->Message(va("Unknown texture \"%s\"", *sc->String));
            animPicSeen.put(sc->Name8, true);
          } else {
            // range
            check(CurType == 2);
            int txtype = (fttype == FT_Flat ? TEXTYPE_Flat : TEXTYPE_Wall);
            BuildTextureRange(GTextureManager.GetTextureName(ad.Index), sc->Name8, txtype, ids, 64); // limit to 64 frames
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
      check(ad.range == 1);
      if (ids.length() == 0) continue; // nothing to do
      for (int f = 0; f < ids.length(); ++f) {
        FrameDef_t &nfd = FrameDefs.alloc();
        nfd = fd;
        nfd.Index = ids[f];
      }
    } else {
      // this is simple pic
      check(CurType == 1);
      check(ad.range == 0 || vanilla);
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
//TEMPORARY
#if 0
  for (int i = 0; i < Switches.length(); ++i) {
    if (Switches[i]->Tex == Switch->Tex) {
      delete Switches[i];
      Switches[i] = nullptr;
      Switches[i] = Switch;
      return i;
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
static void ParseFTAnims (VScriptParser *sc) {
  while (!sc->AtEnd()) {
         if (sc->Check("flat")) ParseFTAnim(sc, true);
    else if (sc->Check("texture")) ParseFTAnim(sc, false);
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
  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) == NAME_animdefs) {
      ParseFTAnims(new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)));
    }
  }

  // optionally parse script file
  /*
  if (fl_devmode && FL_FileExists("scripts/animdefs.txt")) {
    ParseFTAnims(new VScriptParser("scripts/animdefs.txt", FL_OpenFileRead("scripts/animdefs.txt")));
  }
  */

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
