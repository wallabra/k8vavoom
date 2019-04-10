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
//**
//**  Preparation of data for rendering, generation of lookups, caching,
//**  retrieval by name.
//**
//**  Graphics.
//**
//**  DOOM graphics for walls and sprites is stored in vertical runs of
//**  opaque pixels (posts). A column is composed of zero or more posts, a
//**  patch or sprite is composed of zero or more columns.
//**
//**  Texture definition.
//**
//**  Each texture is composed of one or more patches, with patches being
//**  lumps stored in the WAD. The lumps are referenced by number, and
//**  patched into the rectangular texture space using origin and possibly
//**  other attributes.
//**
//**  Texture definition.
//**
//**  A DOOM wall texture is a list of patches which are to be combined in
//**  a predefined order.
//**
//**  A single patch from a texture definition, basically a rectangular
//**  area within the texture rectangle.
//**
//**  A maptexturedef_t describes a rectangular texture, which is composed
//**  of one or more mappatch_t structures that arrange graphic patches.
//**
//**  MAPTEXTURE_T CACHING
//**
//**  When a texture is first needed, it counts the number of composite
//**  columns required in the texture and allocates space for a column
//**  directory and any new columns. The directory will simply point inside
//**  other patches if there is only one patch in a given column, but any
//**  columns with multiple patches will have new column_ts generated.
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
//  Texture manager
// ////////////////////////////////////////////////////////////////////////// //
VTextureManager GTextureManager;


// ////////////////////////////////////////////////////////////////////////// //
// Flats data
// ////////////////////////////////////////////////////////////////////////// //
int skyflatnum; // sky mapping

// switches
TArray<TSwitch *>  Switches;

VCvarB r_hirestex("r_hirestex", false, "Allow high-resolution texture replacements?", CVAR_Archive|CVAR_PreInit);
VCvarB r_showinfo("r_showinfo", false, "Show some info about loaded textures?", CVAR_Archive);

static VCvarB r_reupload_textures("r_reupload_textures", false, "Reupload textures to GPU when new map is loaded?", CVAR_Archive);


// ////////////////////////////////////////////////////////////////////////// //
static TArray<AnimDef_t> AnimDefs;
static TArray<FrameDef_t> FrameDefs;
static TArray<VAnimDoorDef> AnimDoorDefs;

static TMapNC<VName, bool> patchesWarned;

static TMapNC<VName, bool> animPicSeen; // temporary
static TMapNC<int, bool> animTexMap; // to make `R_IsAnimatedTexture()` faster


//==========================================================================
//
//  warnMissingTexture
//
//==========================================================================
static void warnMissingTexture (VName Name, bool silent) {
  if (Name == NAME_None) return; // just in case
  VName xxn = VName(*Name, VName::AddLower);
  if (!patchesWarned.has(xxn)) {
    patchesWarned.put(xxn, true);
    if (!silent) {
      GCon->Logf(NAME_Warning,"Texture: texture \"%s\" not found", *Name);
    }
  }
}


//==========================================================================
//
//  isSeenMissingTexture
//
//==========================================================================
static bool isSeenMissingTexture (VName Name) {
  if (Name == NAME_None) return true; // just in case
  VName xxn = VName(*Name, VName::AddLower);
  return patchesWarned.has(xxn);
}


//==========================================================================
//
//  VTextureManager::VTextureManager
//
//==========================================================================
VTextureManager::VTextureManager ()
  : inMapTextures(0)
  , DefaultTexture(-1)
  , Time(0)
{
  for (int i = 0; i < HASH_SIZE; ++i) TextureHash[i] = -1;
}


//==========================================================================
//
//  VTextureManager::Init
//
//==========================================================================
void VTextureManager::Init () {
  check(inMapTextures == 0);

  // we have to force-load textures after adding textures lump, so
  // texture numbering for animations won't break
  TArray<VName> numberedNames;

  // add a dummy texture
  AddTexture(new VDummyTexture);

  // initialise wall textures
  AddTextures(numberedNames);

  // initialise flats
  AddGroup(TEXTYPE_Flat, WADNS_Flats);

  // initialise overloaded textures
  AddGroup(TEXTYPE_Overload, WADNS_NewTextures);

  // initialise sprites
  AddGroup(TEXTYPE_Sprite, WADNS_Sprites);

  // initialise hires textures
  AddTextureTextLumps(false); // only normal for now

  // force-load numbered textures
  AddMissingNumberedTextures(numberedNames);

  // find default texture
  DefaultTexture = CheckNumForName("-noflat-", TEXTYPE_Overload, false);
  if (DefaultTexture == -1) Sys_Error("Default texture -noflat- not found");

  // find sky flat number
  skyflatnum = CheckNumForName(NAME_f_sky, TEXTYPE_Flat, true);
  if (skyflatnum < 0) skyflatnum = CheckNumForName(NAME_f_sky001, TEXTYPE_Flat, true);
  if (skyflatnum < 0) skyflatnum = NumForName(NAME_f_sky1, TEXTYPE_Flat, false);
}


//==========================================================================
//
//  VTextureManager::Shutdown
//
//==========================================================================
void VTextureManager::Shutdown () {
  for (int i = 0; i < Textures.length(); ++i) { delete Textures[i]; Textures[i] = nullptr; }
  for (int i = 0; i < MapTextures.length(); ++i) { delete MapTextures[i]; MapTextures[i] = nullptr; }
  Textures.clear();
  MapTextures.clear();
}


//==========================================================================
//
//  VTextureManager::Shutdown
//
//==========================================================================
void VTextureManager::DumpHashStats (EName logName) {
  int maxBucketLen = 0;
  int usedBuckets = 0;
  for (int bidx = 0; bidx < HASH_SIZE; ++bidx) {
    if (TextureHash[bidx] < 0) continue;
    ++usedBuckets;
    int blen = 0;
    for (int i = TextureHash[bidx]; i >= 0; i = getTxByIndex(i)->HashNext) ++blen;
    if (maxBucketLen < blen) maxBucketLen = blen;
  }
  GCon->Logf(logName, "TextureManager: maximum %d textures in bucket, used %d out of %d buckets", maxBucketLen, usedBuckets, HASH_SIZE-1);
}


//==========================================================================
//
//  VTextureManager::rehashTextures
//
//==========================================================================
void VTextureManager::rehashTextures () {
  for (int i = 0; i < HASH_SIZE; ++i) TextureHash[i] = -1;
  if (Textures.length()) {
    check(Textures[0]->Name == NAME_None);
    check(Textures[0]->Type == TEXTYPE_Null);
    for (int f = 1; f < Textures.length(); ++f) if (Textures[f]) AddToHash(f);
  }
  for (int f = 0; f < MapTextures.length(); ++f) if (MapTextures[f]) AddToHash(FirstMapTextureIndex+f);
}


//==========================================================================
//
//  VTextureManager::ResetMapTextures
//
//==========================================================================
void VTextureManager::WipeWallPatches () {
  if (GArgs.CheckParm("-wipe-wall-patches")) {
    int count = 0;
    for (int f = 0; f < Textures.length(); ++f) {
      VTexture *tx = Textures[f];
      if (!tx || tx->Type != TEXTYPE_WallPatch || tx->Name != NAME_None) continue;
      tx->Name = NAME_None;
      tx->Type = TEXTYPE_Null;
      ++count;
    }
    if (count) {
      rehashTextures();
      GCon->Logf("WipeWallPatches: %d textures wiped", count);
    }
  }
}


//==========================================================================
//
//  VTextureManager::ResetMapTextures
//
//==========================================================================
void VTextureManager::ResetMapTextures () {
  if (MapTextures.length() == 0) {
#ifdef CLIENT
    if (r_reupload_textures && Drawer) {
      GCon->Logf("Unloading textures from GPU...");
      Drawer->FlushTextures();
      //rehashTextures();
    }
#endif
    return;
  }

  check(MapTextures.length() != 0);

  GCon->Logf(NAME_Dev, "*** *** MapTextures.length()=%d *** ***", MapTextures.length());
#ifdef CLIENT
  if (Drawer && r_reupload_textures) Drawer->FlushTextures();
#endif
  for (int f = MapTextures.length()-1; f >= 0; --f) {
    if (developer) {
      if (MapTextures[f]) GCon->Logf(NAME_Dev, "removing map texture #%d (%s)", f, *MapTextures[f]->Name);
    }
#ifdef CLIENT
    if (Drawer && !r_reupload_textures && MapTextures[f]) Drawer->FlushOneTexture(MapTextures[f]);
#endif
    delete MapTextures[f];
    MapTextures[f] = nullptr;
  }
  GCon->Logf("TextureManager: %d map textures removed", MapTextures.length());
  MapTextures.setLength(0, false); // don't resize
  rehashTextures();
}


//==========================================================================
//
//  VTextureManager::AddTexture
//
//==========================================================================
int VTextureManager::AddTexture (VTexture *Tex) {
  if (!Tex) return -1;

  if (Tex->Name == NAME_None && Textures.length()) {
    R_DumpTextures();
    abort();
  }

  if (Tex->Name == "-") return 0; // "no texture"

  // also, replace existing texture with similar name, if we aren't in "map-local" mode
  if (!inMapTextures) {
    if (Tex->Name != NAME_None && (*Tex->Name)[0] != 0x7f) {
      int repidx = -1;
      // loop, no shinking allowed
      for (auto it = firstWithName(Tex->Name, false); !it.empty(); it.next()) {
        if (it.isMapTexture()) continue; // skip map textures
        VTexture *tx = it.tex();
        if (tx->Name != Tex->Name) continue;
        if (tx->Type != Tex->Type) continue;
        repidx = it.index();
        break;
      }
      if (repidx > 0) {
        check(repidx > 0 && repidx < FirstMapTextureIndex);
        static int warnReplace = -1;
        if (warnReplace < 0) warnReplace = (GArgs.CheckParm("-Wduplicate-textures") ? 1 : 0);
        if (warnReplace > 0 || developer) GCon->Logf(NAME_Warning, "replacing duplicate texture '%s' with new one (id=%d)", *Tex->Name, repidx);
        ReplaceTexture(repidx, Tex);
        return repidx;
      }
    }
    if (developer) GCon->Logf(NAME_Dev, "***NEW TEXTURE #%d: <%s> (%s)", Textures.length(), *Tex->Name, VTexture::TexTypeToStr(Tex->Type));
    Textures.Append(Tex);
    Tex->TextureTranslation = Textures.length()-1;
    AddToHash(Textures.length()-1);
    return Textures.length()-1;
  } else {
    if (developer) GCon->Logf(NAME_Dev, "***MAP-TEXTURE #%d: <%s> (%s)", MapTextures.length(), *Tex->Name, VTexture::TexTypeToStr(Tex->Type));
    MapTextures.Append(Tex);
    Tex->TextureTranslation = FirstMapTextureIndex+MapTextures.length()-1;
    AddToHash(FirstMapTextureIndex+MapTextures.length()-1);
    return FirstMapTextureIndex+MapTextures.length()-1;
  }
}


//==========================================================================
//
//  VTextureManager::ReplaceTexture
//
//==========================================================================
void VTextureManager::ReplaceTexture (int Index, VTexture *NewTex) {
  check(Index >= 0);
  check((Index < FirstMapTextureIndex && Index < Textures.length()) || (Index >= FirstMapTextureIndex && Index-FirstMapTextureIndex < MapTextures.length()));
  check(NewTex);
  //VTexture *OldTex = Textures[Index];
  VTexture *OldTex = getTxByIndex(Index);
  if (OldTex == NewTex) return;
  NewTex->Name = OldTex->Name;
  NewTex->Type = OldTex->Type;
  NewTex->TextureTranslation = OldTex->TextureTranslation;
  NewTex->HashNext = OldTex->HashNext;
  //Textures[Index] = NewTex;
  if (Index < FirstMapTextureIndex) {
    Textures[Index] = NewTex;
  } else {
    MapTextures[Index-FirstMapTextureIndex] = NewTex;
  }
  //FIXME: delete OldTex?
}


//==========================================================================
//
//  VTextureManager::AddToHash
//
//==========================================================================
void VTextureManager::AddToHash (int Index) {
  VTexture *tx = getTxByIndex(Index);
  check(tx);
  tx->HashNext = -1;
  if (tx->Name == NAME_None || (*tx->Name)[0] == 0x7f) return;
  int HashIndex = GetTypeHash(tx->Name)&(HASH_SIZE-1);
  if (Index < FirstMapTextureIndex) {
    Textures[Index]->HashNext = TextureHash[HashIndex];
  } else {
    MapTextures[Index-FirstMapTextureIndex]->HashNext = TextureHash[HashIndex];
  }
  TextureHash[HashIndex] = Index;
}


//==========================================================================
//
//  VTextureManager::firstWithStr
//
//==========================================================================
VTextureManager::Iter VTextureManager::firstWithStr (const VStr &s) {
  if (s.isEmpty()) return Iter();
  VName n = VName(*s, VName::FindLower);
  if (n == NAME_None && s.length() > 8) n = VName(*s, VName::FindLower8);
  if (n == NAME_None) return Iter();
  return firstWithName(n);
}


//==========================================================================
//
//  VTextureManager::CheckNumForName
//
//  Check whether texture is available. Filter out NoTexture indicator.
//
//==========================================================================
int VTextureManager::CheckNumForName (VName Name, int Type, bool bOverload) {
  if ((unsigned)Type >= (unsigned)TEXTYPE_MAX) return -1; // oops

  if (Name == NAME_None) return -1;
  if (IsDummyTextureName(Name)) return 0;

  bool secondary = false;

doitagain:
  int seenOther = -1;
  int seenType = -1;
  int seenOne = -1;
  int seenOneType = -1;

  //if (secondary) GCon->Logf("*** SECONDARY lookup for texture '%s'", *Name);

  //GCon->Logf("::: LOOKING FOR '%s' (%s)", *Name, VTexture::TexTypeToStr(Type));
  for (auto it = firstWithName(Name); !it.empty(); it.next()) {
    //GCon->Logf("  (---) %d", it.index());
    VTexture *ctex = it.tex();
    //GCon->Logf("* %s * idx=%d; name='%s' (%s : %s)", *Name, it.index(), *ctex->Name, VTexture::TexTypeToStr(Type), VTexture::TexTypeToStr(ctex->Type));
    if (Type == TEXTYPE_Any || ctex->Type == Type || (bOverload && ctex->Type == TEXTYPE_Overload)) {
      //GCon->Logf("  (000) %d", it.index());
      if (secondary) {
        // secondary check
        switch (ctex->Type) {
          case TEXTYPE_WallPatch:
          case TEXTYPE_Overload:
          case TEXTYPE_Skin:
          case TEXTYPE_Autopage:
          case TEXTYPE_Null:
          case TEXTYPE_FontChar:
            //GCon->Logf("  (001) %d", it.index());
            continue;
        }
      }
      //GCon->Logf("   HIT! '%s' (%s)", *ctex->Name, VTexture::TexTypeToStr(ctex->Type));
      if (ctex->Type == TEXTYPE_Null) {
        //GCon->Logf("  (002) %d", it.index());
        return 0;
      }
      //GCon->Logf("  (003) %d", it.index());
      return it.index();
    } else if (Type == TEXTYPE_WallPatch && ctex->Type != TEXTYPE_Null) {
      //GCon->Logf("  (004) %d", it.index());
      bool repl = false;
      switch (ctex->Type) {
        case TEXTYPE_Wall: repl = (seenType < 0 || seenType == TEXTYPE_Sprite || seenType == TEXTYPE_Flat); break;
        case TEXTYPE_Flat: repl = (seenType < 0 || seenType == TEXTYPE_Sprite); break;
        case TEXTYPE_Sprite: repl = (seenType < 0); break;
        case TEXTYPE_Pic: repl =(seenType < 0 || seenType == TEXTYPE_Sprite || seenType == TEXTYPE_Flat || seenType == TEXTYPE_Wall); break;
      }
      if (repl) {
        //GCon->Logf("  (005) %d", it.index());
        seenOther = it.index();
        seenType = ctex->Type;
      }
    } else {
      //GCon->Logf("  (100) %d", it.index());
      switch (ctex->Type) {
        case TEXTYPE_WallPatch:
        case TEXTYPE_Overload:
        case TEXTYPE_Skin:
        case TEXTYPE_Autopage:
        case TEXTYPE_Null:
        case TEXTYPE_FontChar:
          break;
        default:
          if (seenOneType < 0) {
            seenOneType = ctex->Type;
            seenOne = (seenOneType != TEXTYPE_Null ? it.index() : 0);
          } else {
            seenOne = -1;
          }
          break;
      }
    }
  }

  if (seenOther >= 0) {
    //GCon->Logf("  SO-HIT: * %s * idx=%d; name='%s' (%s : %s)", *Name, seenOther, *getTxByIndex(seenOther)->Name, VTexture::TexTypeToStr(Type), VTexture::TexTypeToStr(getTxByIndex(seenOther)->Type));
    return seenOther;
  }

  //GCon->Logf("* %s * NOT FOUND! (%s)", *Name, VTexture::TexTypeToStr(Type));

  if (!secondary && Type != TEXTYPE_Any) {
    //GCon->Logf("  (006)");
    if (seenOne >= 0) {
      //VTexture *ctex = getTxByIndex(seenOne);
      //GCon->Logf("SEENONE '%s' for type '%s' (%d : %s : %s)", *Name, VTexture::TexTypeToStr(Type), seenOne, *ctex->Name, VTexture::TexTypeToStr(ctex->Type));
      return seenOne;
    }
    switch (Type) {
      case TEXTYPE_Wall:
      case TEXTYPE_Flat:
      case TEXTYPE_Pic:
        //GCon->Logf("*** looking for any texture for %s '%s'", VTexture::TexTypeToStr(Type), *Name);
        secondary = true;
        Type = TEXTYPE_Any;
        goto doitagain;
    }
  }

  return -1;
}


//==========================================================================
//
//  VTextureManager::FindPatchByName
//
//  used in multipatch texture builder
//
//==========================================================================
int VTextureManager::FindPatchByName (VName Name) {
  return CheckNumForName(Name, TEXTYPE_WallPatch, true);
}


//==========================================================================
//
//  VTextureManager::FindWallByName
//
//  used to find wall texture (but can return non-wall)
//
//==========================================================================
int VTextureManager::FindWallByName (VName Name, bool bOverload) {
  if (Name == NAME_None) return -1;
  if (IsDummyTextureName(Name)) return 0;

  int seenOther = -1;
  int seenType = -1;

  for (int trynum = 0; trynum < 2; ++trynum) {
    if (trynum == 1) {
      if (VStr::length(*Name) < 8) return -1;
      Name = VName(*Name, VName::FindLower8);
      if (Name == NAME_None) return -1;
    }

    for (auto it = firstWithName(Name); !it.empty(); (void)it.next()) {
      VTexture *ctex = it.tex();
      if (ctex->Type == TEXTYPE_Wall || (bOverload && ctex->Type == TEXTYPE_Overload)) {
        if (ctex->Type == TEXTYPE_Null) return 0;
        return it.index();
      } else if (ctex->Type != TEXTYPE_Null) {
        bool repl = false;
        switch (ctex->Type) {
          case TEXTYPE_Flat: repl = (seenType < 0 || seenType == TEXTYPE_Sprite); break;
          case TEXTYPE_Sprite: repl = (seenType < 0); break;
          case TEXTYPE_Pic: repl =(seenType < 0 || seenType == TEXTYPE_Sprite || seenType == TEXTYPE_Flat); break;
        }
        if (repl) {
          seenOther = it.index();
          seenType = ctex->Type;
        }
      }
    }

    if (seenOther >= 0) return seenOther;
  }

  return -1;
}


//==========================================================================
//
//  VTextureManager::FindFlatByName
//
//  used to find flat texture (but can return non-flat)
//
//==========================================================================
int VTextureManager::FindFlatByName (VName Name, bool bOverload) {
  if (Name == NAME_None) return -1;
  if (IsDummyTextureName(Name)) return 0;

  int seenOther = -1;
  int seenType = -1;

  for (int trynum = 0; trynum < 2; ++trynum) {
    if (trynum == 1) {
      if (VStr::length(*Name) < 8) return -1;
      Name = VName(*Name, VName::FindLower8);
      if (Name == NAME_None) return -1;
    }

    for (auto it = firstWithName(Name); !it.empty(); (void)it.next()) {
      VTexture *ctex = it.tex();
      if (ctex->Type == TEXTYPE_Flat || (bOverload && ctex->Type == TEXTYPE_Overload)) {
        if (ctex->Type == TEXTYPE_Null) return 0;
        return it.index();
      } else if (ctex->Type != TEXTYPE_Null) {
        bool repl = false;
        switch (ctex->Type) {
          case TEXTYPE_Wall: repl = (seenType < 0 || seenType == TEXTYPE_Sprite); break;
          case TEXTYPE_Sprite: repl = (seenType < 0); break;
          case TEXTYPE_Pic: repl =(seenType < 0 || seenType == TEXTYPE_Sprite || seenType == TEXTYPE_Wall); break;
        }
        if (repl) {
          seenOther = it.index();
          seenType = ctex->Type;
        }
      }
    }

    if (seenOther >= 0) return seenOther;
  }

  return -1;
}


//==========================================================================
//
//  VTextureManager::NumForName
//
//  Calls R_CheckTextureNumForName, aborts with error message.
//
//==========================================================================
int VTextureManager::NumForName (VName Name, int Type, bool bOverload) {
  static TStrSet numForNameWarned;
  if (Name == NAME_None) return 0;
  if (IsDummyTextureName(Name)) return 0;
  int i = CheckNumForName(Name, Type, bOverload);
  if (i == -1) {
    if (!numForNameWarned.put(*Name)) {
      GCon->Logf(NAME_Warning, "VTextureManager::NumForName: '%s' not found (type:%d; over:%d)", *Name, (int)Type, (int)bOverload);
    }
    i = DefaultTexture;
  }
  return i;
}


//==========================================================================
//
//  VTextureManager::FindTextureByLumpNum
//
//==========================================================================
int VTextureManager::FindTextureByLumpNum (int LumpNum) {
  for (int i = 0; i < MapTextures.length(); ++i) {
    if (MapTextures[i]->SourceLump == LumpNum) return i+FirstMapTextureIndex;
  }
  for (int i = 0; i < Textures.length(); ++i) {
    if (Textures[i]->SourceLump == LumpNum) return i;
  }
  return -1;
}


//==========================================================================
//
//  VTextureManager::GetTextureName
//
//==========================================================================
VName VTextureManager::GetTextureName (int TexNum) {
  VTexture *tx = getTxByIndex(TexNum);
  return (tx ? tx->Name : NAME_None);
}


//==========================================================================
//
//  VTextureManager::TextureWidth
//
//==========================================================================
float VTextureManager::TextureWidth (int TexNum) {
  VTexture *tx = getTxByIndex(TexNum);
  return (tx ? tx->GetWidth()/tx->SScale : 0);
}


//==========================================================================
//
//  VTextureManager::TextureHeight
//
//==========================================================================
float VTextureManager::TextureHeight (int TexNum) {
  VTexture *tx = getTxByIndex(TexNum);
  return (tx ? tx->GetHeight()/tx->TScale : 0);
}


//==========================================================================
//
//  VTextureManager::SetFrontSkyLayer
//
//==========================================================================
void VTextureManager::SetFrontSkyLayer (int tex) {
  VTexture *tx = getTxByIndex(tex);
  if (tx) tx->SetFrontSkyLayer();
}


//==========================================================================
//
//  VTextureManager::GetTextureInfo
//
//==========================================================================
void VTextureManager::GetTextureInfo (int TexNum, picinfo_t *info) {
  VTexture *Tex = getTxByIndex(TexNum);
  if (Tex) {
    info->width = Tex->GetWidth();
    info->height = Tex->GetHeight();
    info->xoffset = Tex->SOffset;
    info->yoffset = Tex->TOffset;
  } else {
    memset((void *)info, 0, sizeof(*info));
  }
}


//==========================================================================
//
//  findAndLoadTexture
//
//  FIXME: make this faster!
//
//==========================================================================
static bool findAndLoadTexture (VName Name, int Type, EWadNamespace NS) {
  if (Name == NAME_None) return false;
  if (VTextureManager::IsDummyTextureName(Name)) return false;
  VName PatchName(*Name, VName::AddLower8);
  // need to collect 'em to go in backwards order
  TArray<int> fulllist; // full names
  TArray<int> shortlist; // short names
  for (int LNum = W_IterateNS(-1, NS); LNum >= 0; LNum = W_IterateNS(LNum, NS)) {
    //GCon->Logf("FOR '%s': #%d is '%s'", *Name, LNum, *W_LumpName(LNum));
    VName lmpname = W_LumpName(LNum);
         if (VStr::ICmp(*lmpname, *Name) == 0) fulllist.append(LNum);
    else if (VStr::ICmp(*lmpname, *PatchName) == 0) shortlist.append(LNum);
  }
  // now go with first list (full name)
  for (int f = fulllist.length()-1; f >= 0; --f) {
    int LNum = fulllist[f];
    VTexture *tex = VTexture::CreateTexture(Type, LNum);
    if (!tex) continue;
    GTextureManager.AddTexture(tex);
    // if lump name is not identical to short, add with short name too
    if (VStr::ICmp(*W_LumpName(LNum), *PatchName) != 0) {
      tex = VTexture::CreateTexture(Type, LNum);
      tex->Name = PatchName;
      GTextureManager.AddTexture(tex);
    }
    return true;
  }
  // and with second list (short name)
  for (int f = shortlist.length()-1; f >= 0; --f) {
    int LNum = shortlist[f];
    VTexture *tex = VTexture::CreateTexture(Type, LNum);
    if (!tex) continue;
    GTextureManager.AddTexture(tex);
    // if lump name is not identical to long, add with long name too
    if (VStr::ICmp(*W_LumpName(LNum), *Name) != 0) {
      tex = VTexture::CreateTexture(Type, LNum);
      tex->Name = VName(*Name, VName::AddLower);
      GTextureManager.AddTexture(tex);
    }
    return true;
  }
  return false;
}


//==========================================================================
//
//  findAndLoadTextureShaded
//
//  FIXME: make this faster!
//
//==========================================================================
static bool findAndLoadTextureShaded (VName Name, VName shName, int Type, EWadNamespace NS, int shade) {
  if (Name == NAME_None) return false;
  if (VTextureManager::IsDummyTextureName(Name)) return false;
  VName PatchName(*Name, VName::AddLower8);
  // need to collect 'em to go in backwards order
  TArray<int> fulllist; // full names
  TArray<int> shortlist; // short names
  for (int LNum = W_IterateNS(-1, NS); LNum >= 0; LNum = W_IterateNS(LNum, NS)) {
    //GCon->Logf("FOR '%s': #%d is '%s'", *Name, LNum, *W_LumpName(LNum));
    VName lmpname = W_LumpName(LNum);
         if (VStr::ICmp(*lmpname, *Name) == 0) fulllist.append(LNum);
    else if (VStr::ICmp(*lmpname, *PatchName) == 0) shortlist.append(LNum);
  }
  // now go with first list (full name)
  for (int f = fulllist.length()-1; f >= 0; --f) {
    int LNum = fulllist[f];
    VTexture *tex = VTexture::CreateTexture(Type, LNum);
    if (!tex) continue;
    tex->Name = shName;
    tex->Shade(shade);
    GTextureManager.AddTexture(tex);
    return true;
  }
  // and with second list (short name)
  for (int f = shortlist.length()-1; f >= 0; --f) {
    int LNum = shortlist[f];
    VTexture *tex = VTexture::CreateTexture(Type, LNum);
    if (!tex) continue;
    tex->Name = shName;
    tex->Shade(shade);
    GTextureManager.AddTexture(tex);
    return true;
  }
  return false;
}


//==========================================================================
//
//  VTextureManager::AddPatch
//
//==========================================================================
int VTextureManager::AddPatch (VName Name, int Type, bool Silent) {
  if (Name == NAME_None) return 0;
  if (IsDummyTextureName(Name)) return 0;

  // check if it's already registered
  int i = CheckNumForName(Name, Type);
  if (i >= 0) return i;

  // do not try to load already seen missing texture
  if (isSeenMissingTexture(Name)) return -1; // alas

  // load it
  if (findAndLoadTexture(Name, Type, WADNS_Patches) ||
      findAndLoadTexture(Name, Type, WADNS_Graphics) ||
      findAndLoadTexture(Name, Type, WADNS_Sprites) ||
      findAndLoadTexture(Name, Type, WADNS_Flats) ||
      findAndLoadTexture(Name, Type, WADNS_Global))
  {
    int tidx = CheckNumForName(Name, Type);
    check(tidx > 0);
    return tidx;
  }

  warnMissingTexture(Name, Silent);
  return -1;
}


//==========================================================================
//
//  VTextureManager::AddPatchLump
//
//==========================================================================
int VTextureManager::AddPatchLump (int LumpNum, VName Name, int Type, bool Silent) {
  check(Name != NAME_None);

  // check if it's already registered
  int i = CheckNumForName(Name, Type);
  if (i >= 0) return i;

  if (LumpNum >= 0) {
    VTexture *tex = VTexture::CreateTexture(Type, LumpNum);
    if (tex) {
      tex->Name = Name;
      GTextureManager.AddTexture(tex);
      int tidx = CheckNumForName(Name, Type);
      check(tidx > 0);
      return tidx;
    }
  }

  // do not try to load already seen missing texture
  if (isSeenMissingTexture(Name)) return -1; // alas

  warnMissingTexture(Name, Silent);
  return -1;
}


//==========================================================================
//
//  VTextureManager::AddRawWithPal
//
//  Adds a raw image with custom palette lump. It's here to support
//  Heretic's episode 2 finale pic.
//
//==========================================================================
int VTextureManager::AddRawWithPal (VName Name, VName PalName) {
  if (Name == NAME_None) abort();
  if (IsDummyTextureName(Name)) abort();
  //TODO
  int LumpNum = W_CheckNumForName(Name, WADNS_Graphics);
  if (LumpNum < 0) LumpNum = W_CheckNumForName(Name, WADNS_Sprites);
  if (LumpNum < 0) LumpNum = W_CheckNumForName(Name, WADNS_Global);
  if (LumpNum < 0) LumpNum = W_CheckNumForFileName(VStr(Name));
  if (LumpNum < 0) {
    GCon->Logf(NAME_Warning, "VTextureManager::AddRawWithPal: \"%s\" not found", *Name);
    return -1;
  }
  // check if lump's size to see if it really is a raw image; if not, load it as regular image
  if (W_LumpLength(LumpNum) != 64000) {
    GCon->Logf(NAME_Warning, "VTextureManager::AddRawWithPal: \"%s\" doesn't appear to be a raw image", *Name);
    return AddPatch(Name, TEXTYPE_Pic);
  }

  int i = CheckNumForName(Name, TEXTYPE_Pic);
  if (i >= 0) return i;

  return AddTexture(new VRawPicTexture(LumpNum, W_GetNumForName(PalName)));
}


//==========================================================================
//
//  VTextureManager::AddFileTextureChecked
//
//  returns -1 if no file texture found
//
//==========================================================================
int VTextureManager::AddFileTextureChecked (VName Name, int Type) {
  if (Name == NAME_None) return 0;
  if (IsDummyTextureName(Name)) return 0;

  int i = CheckNumForName(Name, Type);
  if (i >= 0) return i;

  i = W_CheckNumForFileName(*Name);
  if (i >= 0) {
    VTexture *Tex = VTexture::CreateTexture(Type, i);
    if (Tex) {
      if (developer) GCon->Logf(NAME_Dev, "******************** %s : %s ********************", *Name, *Tex->Name);
      Tex->Name = Name;
      return AddTexture(Tex);
    }
  }

  return CheckNumForNameAndForce(Name, Type, true/*bOverload*/, true/*silent*/);
}


//==========================================================================
//
//  VTextureManager::AddFileTexture
//
//==========================================================================
int VTextureManager::AddFileTexture (VName Name, int Type) {
  if (Name == NAME_None) return 0;
  if (IsDummyTextureName(Name)) return 0;
  int i = AddFileTextureChecked(Name, Type);
  if (i == -1) {
    GCon->Logf(NAME_Warning, "Couldn\'t create texture \"%s\".", *Name);
    return DefaultTexture;
  }
  return i;
}


//==========================================================================
//
//  VTextureManager::AddFileTextureShaded
//
//  shade==-1: don't shade
//
//==========================================================================
int VTextureManager::AddFileTextureShaded (VName Name, int Type, int shade) {
  if (shade == -1) return AddFileTexture(Name, Type);

  if (Name == NAME_None) return 0;
  if (IsDummyTextureName(Name)) return 0;

  VName shName = VName(va("%s %08x", *Name, (vuint32)shade));

  int i = CheckNumForName(shName, Type);
  if (i >= 0) return i;

  i = W_CheckNumForFileName(*Name);
  if (i >= 0) {
    VTexture *Tex = VTexture::CreateTexture(Type, i);
    if (Tex) {
      Tex->Name = shName;
      Tex->Shade(shade);
      int res = AddTexture(Tex);
      //GCon->Logf("TEXMAN: loaded shaded texture '%s' (named '%s'; id=%d)", *Name, *shName, res);
      return res;
    }
  }

  GCon->Logf(NAME_Warning, "Couldn't create shaded texture \"%s\".", *Name);
  return DefaultTexture;
}


//==========================================================================
//
//  VTextureManager::AddPatchShaded
//
//==========================================================================
int VTextureManager::AddPatchShaded (VName Name, int Type, int shade, bool Silent) {
  if (shade == -1) return AddPatch(Name, Type, Silent);

  if (Name == NAME_None) return 0;
  if (IsDummyTextureName(Name)) return 0;

  VName shName = VName(va("%s %08x", *Name, (vuint32)shade));

  // check if it's already registered
  int i = CheckNumForName(shName, Type);
  if (i >= 0) return i;

  // do not try to load already seen missing texture
  if (isSeenMissingTexture(Name)) return -1; // alas

  // load it
  if (findAndLoadTextureShaded(Name, shName, Type, WADNS_Patches, shade) ||
      findAndLoadTextureShaded(Name, shName, Type, WADNS_Graphics, shade) ||
      findAndLoadTextureShaded(Name, shName, Type, WADNS_Sprites, shade) ||
      findAndLoadTextureShaded(Name, shName, Type, WADNS_Flats, shade) ||
      findAndLoadTextureShaded(Name, shName, Type, WADNS_Global, shade))
  {
    int tidx = CheckNumForName(shName, Type);
    check(tidx > 0);
    return tidx;
  }

  warnMissingTexture(Name, Silent);
  return -1;
}


//==========================================================================
//
//  VTextureManager::CheckNumForNameAndForce
//
//  find or force-load texture
//
//==========================================================================
int VTextureManager::CheckNumForNameAndForce (VName Name, int Type, bool bOverload, bool silent) {
  int tidx = CheckNumForName(Name, Type, bOverload);
  if (tidx >= 0) return tidx;
  // do not try to load already seen missing texture
  if (isSeenMissingTexture(Name)) return -1; // alas
  // load it
  if (findAndLoadTexture(Name, Type, WADNS_Patches) ||
      findAndLoadTexture(Name, Type, WADNS_Sprites) || // sprites also can be used as patches
      findAndLoadTexture(Name, Type, WADNS_Graphics) || // just in case
      findAndLoadTexture(Name, Type, WADNS_Flats) || // why not?
      findAndLoadTexture(Name, Type, WADNS_Global))
  {
    tidx = CheckNumForName(Name, Type, bOverload);
    if (developer && tidx <= 0) {
      GCon->Logf(NAME_Dev, "CheckNumForNameAndForce: OOPS for '%s'; type=%d; overload=%d", *Name, Type, (int)bOverload);
      int HashIndex = GetTypeHash(Name)&(HASH_SIZE-1);
      for (int i = TextureHash[HashIndex]; i >= 0; i = getTxByIndex(i)->HashNext) {
        VTexture *tx = getTxByIndex(i);
        if (!tx) abort();
        GCon->Logf(NAME_Dev, "  %d: name='%s'; type=%d", i, *tx->Name, tx->Type);
      }
    }
    check(tidx > 0);
    return tidx;
  }
  // alas
  //if (!silent) GCon->Logf(NAME_Warning, "Textures: missing texture \"%s\"", *Name);
  warnMissingTexture(Name, silent);
  return -1;
}


//==========================================================================
//
//  VTextureManager::AddTextures
//
//  Initialises the texture list with the textures from the textures lump
//
//==========================================================================
void VTextureManager::AddTextures (TArray<VName> &numberedNames) {
  int NamesFile = -1;
  int LumpTex1 = -1;
  int LumpTex2 = -1;
  int FirstTex;

  TArray<WallPatchInfo> patchtexlookup;
  // for each PNAMES lump load TEXTURE1 and TEXTURE2 from the same wad
  int lastPNameFile = -1; // fuck you, slade!
  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) != NAME_pnames) continue;
    NamesFile = W_LumpFile(Lump);
    if (lastPNameFile == NamesFile) continue;
    lastPNameFile = NamesFile;
    LoadPNames(Lump, patchtexlookup, numberedNames);
    LumpTex1 = W_CheckFirstNumForNameInFile(NAME_texture1, NamesFile);
    LumpTex2 = W_CheckFirstNumForNameInFile(NAME_texture2, NamesFile);
    FirstTex = Textures.length();
    AddTexturesLump(patchtexlookup, LumpTex1, FirstTex, true);
    AddTexturesLump(patchtexlookup, LumpTex2, FirstTex, false);
  }

  // if last TEXTURE1 or TEXTURE2 are in a wad without a PNAMES, they must be loaded too
  int LastTex1 = W_CheckNumForName(NAME_texture1);
  int LastTex2 = W_CheckNumForName(NAME_texture2);
  if (LastTex1 >= 0 && (LastTex1 == LumpTex1 || W_LumpFile(LastTex1) <= NamesFile)) LastTex1 = -1;
  if (LastTex2 >= 0 && (LastTex2 == LumpTex2 || W_LumpFile(LastTex2) <= NamesFile)) LastTex2 = -1;
  FirstTex = Textures.length();
  if (LastTex1 != -1 || LastTex2 != -1) {
    LoadPNames(W_GetNumForName(NAME_pnames), patchtexlookup, numberedNames);
    AddTexturesLump(patchtexlookup, LastTex1, FirstTex, true);
    AddTexturesLump(patchtexlookup, LastTex2, FirstTex, false);
  }
}


//==========================================================================
//
//  VTextureManager::LoadPNames
//
//  load the patch names from pnames.lmp
//
//==========================================================================
void VTextureManager::LoadPNames (int NamesLump, TArray<WallPatchInfo> &patchtexlookup, TArray<VName> &numberedNames) {
  patchtexlookup.clear();
  if (NamesLump < 0) return;
  int pncount = 0;
  int NamesFile = W_LumpFile(NamesLump);
  VStr pkname = W_FullPakNameForLump(NamesLump);
  while (NamesLump >= 0 && W_LumpFile(NamesLump) == NamesFile) {
    if (W_LumpName(NamesLump) != NAME_pnames) {
      // next one
      NamesLump = W_IterateNS(NamesLump, WADNS_Global);
      continue;
    }
    if (pncount++ == 1) {
      GCon->Logf(NAME_Warning, "duplicate file \"PNAMES\" in archive \"%s\".", *W_FullPakNameForLump(NamesLump));
      GCon->Log(NAME_Warning, "THIS IS FUCKIN' WRONG. DO NOT USE BROKEN TOOLS TO CREATE PK3/WAD FILES!");
    }
    VStream *lumpstream = W_CreateLumpReaderNum(NamesLump);
    VCheckedStream Strm(lumpstream);
    if (developer) GCon->Logf(NAME_Dev, "READING '%s' 0x%08x (%d)", *W_FullLumpName(NamesLump), NamesLump, Strm.TotalSize());
    vint32 nummappatches = Streamer<vint32>(Strm);
    if (nummappatches < 0 || nummappatches > 1024*1024) Sys_Error("%s: invalid number of patches in pnames (%d)", *W_FullLumpName(NamesLump), nummappatches);
    //VTexture **patchtexlookup = new VTexture *[nummappatches];
    for (int i = 0; i < nummappatches; ++i) {
      // read patch name
      char TmpName[12];
      Strm.Serialise(TmpName, 8);
      //if (Strm.IsError()) Sys_Error("%s: error reading PNAMES", *W_FullLumpName(NamesLump));
      TmpName[8] = 0;

      if ((vuint8)TmpName[0] < 32 || (vuint8)TmpName[0] >= 127) {
        Sys_Error("%s: record #%d, name is <%s>", *W_FullLumpName(NamesLump), i, TmpName);
      }

      //if (developer) GCon->Logf(NAME_Dev, "PNAMES entry #%d is '%s'", i, TmpName);
      if (!TmpName[0]) {
        GCon->Logf(NAME_Warning, "PNAMES entry #%d is empty!", i);
        WallPatchInfo &wpi = patchtexlookup.alloc();
        wpi.index = patchtexlookup.length()-1;
        wpi.name = NAME_None;
        wpi.tx = nullptr;
        continue;
      }

      VName PatchName(TmpName, VName::AddLower8);

      {
        const char *txname = *PatchName;
        int namelen = VStr::length(txname);
        if (namelen && VStr::digitInBase(txname[namelen-1], 10) >= 0) numberedNames.append(PatchName);
      }

      WallPatchInfo &wpi = patchtexlookup.alloc();
      wpi.index = patchtexlookup.length()-1;
      wpi.name = PatchName;

      // check if it's already has been added
      int PIdx = CheckNumForName(PatchName, TEXTYPE_WallPatch, false);
      if (PIdx >= 0) {
        //patchtexlookup[i] = Textures[PIdx];
        wpi.tx = Textures[PIdx];
        check(wpi.tx);
        continue;
      }

      bool isFlat = false;
      // get wad lump number
      int LNum = W_CheckNumForName(PatchName, WADNS_Patches);
      // sprites, flats, etc. also can be used as patches
      if (LNum < 0) { LNum = W_CheckNumForName(PatchName, WADNS_Flats); if (LNum >= 0) isFlat = true; } // eh, why not?
      if (LNum < 0) LNum = W_CheckNumForName(PatchName, WADNS_NewTextures);
      if (LNum < 0) LNum = W_CheckNumForName(PatchName, WADNS_Sprites);
      if (LNum < 0) LNum = W_CheckNumForName(PatchName, WADNS_Global); // just in case

      // add it to textures
      if (LNum < 0) {
        wpi.tx = nullptr;
        //patchtexlookup[i] = nullptr;
        GCon->Logf(NAME_Warning, "PNAMES(%s): cannot find texture patch '%s' (%d/%d)", *W_FullLumpName(NamesLump), *PatchName, i, nummappatches-1);
      } else {
        //patchtexlookup[i] = VTexture::CreateTexture(TEXTYPE_WallPatch, LNum);
        //if (patchtexlookup[i]) AddTexture(patchtexlookup[i]);
        wpi.tx = VTexture::CreateTexture((isFlat ? TEXTYPE_Flat : TEXTYPE_WallPatch), LNum);
        if (!wpi.tx) GCon->Logf(NAME_Warning, "%s: loading patch '%s' (%d/%d) failed", *W_FullLumpName(NamesLump), *PatchName, i, nummappatches-1);
        if (wpi.tx) AddTexture(wpi.tx);
      }
    }
    // next one
    NamesLump = W_IterateNS(NamesLump, WADNS_Global);
  }

  if (developer) {
    for (int f = 0; f < patchtexlookup.length(); ++f) {
      check(patchtexlookup[f].index == f);
      VTexture *tx = patchtexlookup[f].tx;
      GCon->Logf(NAME_Dev, "%s:PNAME (%d/%d): name=%s; tx=%d; txname=%s", *pkname, f, patchtexlookup.length()-1, *patchtexlookup[f].name, (tx ? 1 : 0), (tx ? *tx->Name : "----"));
    }
  }
}


//==========================================================================
//
//  VTextureManager::AddMissingNumberedTextures
//
//  Initialises the texture list with the textures from the textures lump
//
//==========================================================================
void VTextureManager::AddMissingNumberedTextures (TArray<VName> &numberedNames) {
  //k8: force-load numbered textures
  for (int f = 0; f < numberedNames.length(); ++f) {
    const char *txname = *numberedNames[f];
    int namelen = VStr::length(txname);
    if (namelen && txname[namelen-1] == '1') {
      char nbuf[130];
      snprintf(nbuf, sizeof(nbuf), "%s", txname);
      for (int c = 2; c < 10; ++c) {
        nbuf[namelen-1] = '0'+c;
        VName PatchName(nbuf, VName::AddLower8);
        if (CheckNumForName(PatchName, TEXTYPE_WallPatch, false) < 0) {
          int tid = CheckNumForNameAndForce(PatchName, TEXTYPE_WallPatch, true, true);
          if (tid > 0) GCon->Logf(NAME_Init, "Textures: force-loaded numbered texture '%s'", nbuf);
        } else {
          if (developer) GCon->Logf(NAME_Dev, "Textures: already loaded numbered texture '%s'", nbuf);
        }
      }
    }
  }
}


//==========================================================================
//
//  VTextureManager::AddTexturesLump
//
//==========================================================================
void VTextureManager::AddTexturesLump (TArray<WallPatchInfo> &patchtexlookup, int TexLump, int FirstTex, bool First) {
  if (TexLump < 0) return;

  check(inMapTextures == 0);
  VName tlname = W_LumpName(TexLump);
  TMapNC<VName, bool> tseen; // only the first seen texture is relevant, so ignore others

  VName dashName = VName("-"); // empty texture
  int pncount = 0;
  int TexFile = W_LumpFile(TexLump);
  while (TexLump >= 0 && W_LumpFile(TexLump) == TexFile) {
    if (W_LumpName(TexLump) != tlname) {
      // next one
      TexLump = W_IterateNS(TexLump, WADNS_Global);
      continue;
    }
    if (pncount++ == 1) {
      GCon->Logf(NAME_Warning, "duplicate file \"%s\" in archive \"%s\".", *tlname, *W_FullPakNameForLump(TexLump));
      GCon->Log(NAME_Warning, "THIS IS *ABSOLUTELY* FUCKIN' WRONG. DO NOT USE BROKEN TOOLS TO CREATE PK3/WAD FILES!");
      break;
    }

    // load the map texture definitions from textures.lmp
    // the data is contained in one or two lumps, TEXTURE1 for shareware, plus TEXTURE2 for commercial
    VStream *lumpstream = W_CreateLumpReaderNum(TexLump);
    VCheckedStream Strm(lumpstream);
    vint32 NumTex = Streamer<vint32>(Strm);

    // check the texture file format
    bool IsStrife = false;
    vint32 PrevOffset = Streamer<vint32>(Strm);
    for (int i = 0; i < NumTex-1; ++i) {
      vint32 Offset = Streamer<vint32>(Strm);
      if (Offset-PrevOffset == 24) {
        IsStrife = true;
        GCon->Logf(NAME_Init, "Strife textures detected in lump '%s'", *W_FullLumpName(TexLump));
        break;
      }
      PrevOffset = Offset;
    }

    for (int i = 0; i < NumTex; ++i) {
      VMultiPatchTexture *Tex = new VMultiPatchTexture(Strm, i, patchtexlookup, FirstTex, IsStrife);
      // copy dimensions of the first texture to the dummy texture in case they are used, and
      // set it to be `TEXTYPE_Null`, as this is how DooM works
      if (i == 0 && First) {
        Textures[0]->Width = Tex->Width;
        Textures[0]->Height = Tex->Height;
        Tex->Type = TEXTYPE_Null;
      }
      // ignore empty and duplicate textures
      if (Tex->Name == NAME_None || Tex->Name == dashName || tseen.has(Tex->Name)) { delete Tex; continue; }
      if (Tex->SourceLump < TexLump) Tex->SourceLump = TexLump;
      tseen.put(Tex->Name, true);
      AddTexture(Tex);
    }
    // next one
    TexLump = W_IterateNS(TexLump, WADNS_Global);
  }
}


//==========================================================================
//
//  VTextureManager::AddGroup
//
//==========================================================================
void VTextureManager::AddGroup (int Type, EWadNamespace Namespace) {
  for (int Lump = W_IterateNS(-1, Namespace); Lump >= 0; Lump = W_IterateNS(Lump, Namespace)) {
    // to avoid duplicates, add only the last one
    if (W_GetNumForName(W_LumpName(Lump), Namespace) != Lump) {
      //GCon->Logf(NAME_Dev, "VTextureManager::AddGroup(%d:%d): skipped lump '%s'", Type, Namespace, *W_FullLumpName(Lump));
      continue;
    }
    //GCon->Logf("VTextureManager::AddGroup(%d:%d): loading lump '%s'", Type, Namespace, *W_FullLumpName(Lump));
    AddTexture(VTexture::CreateTexture(Type, Lump));
  }
}


//==========================================================================
//
//  VTextureManager::AddTextureTextLumps
//
//==========================================================================
void VTextureManager::AddTextureTextLumps (bool onlyHiRes) {
  check(inMapTextures == 0);
  //GCon->Logf("HIRES: %d", (r_hirestex ? 1 : 0));

  if (onlyHiRes) {
    for (int Lump = W_IterateNS(-1, WADNS_HiResTextures); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_HiResTextures)) {
      VName Name = W_LumpName(Lump);
      // to avoid duplicates, add only the last one
      if (W_GetNumForName(Name, WADNS_HiResTextures) != Lump) continue;

      // create new texture
      VTexture *NewTex = VTexture::CreateTexture(TEXTYPE_Any, Lump);
      if (!NewTex) continue;
      if (NewTex->Type == TEXTYPE_Any) NewTex->Type = TEXTYPE_Wall;

      // find texture to replace
      int OldIdx = CheckNumForName(Name, TEXTYPE_Wall, true);
      if (OldIdx < 0) {
        if (!r_hirestex) continue; // don't replace
        OldIdx = AddPatch(Name, TEXTYPE_Pic, true);
      }

      if (OldIdx < 0) {
        // add it as a new texture
        if (r_hirestex) {
          //NewTex->Type = TEXTYPE_Overload;
          AddTexture(NewTex);
        }
      } else {
        if (r_hirestex) {
          // repalce existing texture by adjusting scale and offsets
          VTexture *OldTex = Textures[OldIdx];
          //NewTex->Type = OldTex->Type;
          NewTex->Type = TEXTYPE_Overload;
          //GCon->Logf("REPLACE0 <%s> (%d)", *OldTex->Name, OldIdx);
          NewTex->Name = OldTex->Name;
          NewTex->bWorldPanning = true;
          NewTex->SScale = NewTex->GetWidth()/OldTex->GetWidth();
          NewTex->TScale = NewTex->GetHeight()/OldTex->GetHeight();
          NewTex->SOffset = (int)floor(OldTex->SOffset*NewTex->SScale);
          NewTex->TOffset = (int)floor(OldTex->TOffset*NewTex->TScale);
          NewTex->Type = OldTex->Type;
          NewTex->TextureTranslation = OldTex->TextureTranslation;
          NewTex->HashNext = OldTex->HashNext;
          Textures[OldIdx] = NewTex;
          delete OldTex;
          OldTex = nullptr;
        }
      }
    }
  }

  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) != NAME_hirestex && W_LumpName(Lump) != NAME_textures) continue;

    GCon->Logf(NAME_Init, "parsing textures script \"%s\"...", *W_FullLumpName(Lump));
    VScriptParser *sc = new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump));
    while (!sc->AtEnd()) {
      if (sc->Check("remap")) {
        int Type = TEXTYPE_Any;
        bool Overload = false;
        if (sc->Check("wall")) {
          Type = TEXTYPE_Wall;
          Overload = true;
        } else if (sc->Check("flat")) {
          Type = TEXTYPE_Flat;
          Overload = true;
        } else if (sc->Check("sprite")) {
          Type = TEXTYPE_Sprite;
        }

        sc->ExpectName8Warn();
        if (!onlyHiRes) {
          sc->ExpectName8Warn();
          continue;
        }

        int OldIdx = CheckNumForName(sc->Name8, Type, Overload);
        if (OldIdx < 0) {
          if (!r_hirestex) {
            // don't replace
            sc->ExpectName8Warn();
            continue;
          }
          OldIdx = AddPatch(sc->Name8, TEXTYPE_Pic, true);
        }

        sc->ExpectName8Warn();
        int LumpIdx = W_CheckNumForName(sc->Name8, WADNS_Graphics);
        if (OldIdx < 0 || LumpIdx < 0) continue;

        if (r_hirestex) {
          // create new texture
          VTexture *NewTex = VTexture::CreateTexture(TEXTYPE_Any, LumpIdx);
          if (!NewTex) continue;
          //if (NewTex->Type == TEXTYPE_Any) NewTex->Type = (Type == TEXTYPE_Any ? TEXTYPE_Wall : Type);
          // repalce existing texture by adjusting scale and offsets
          VTexture *OldTex = Textures[OldIdx];
          NewTex->Type = OldTex->Type;
          //fprintf(stderr, "REPLACE1 <%s> (%d)\n", *OldTex->Name, OldIdx);
          NewTex->bWorldPanning = true;
          NewTex->SScale = NewTex->GetWidth()/OldTex->GetWidth();
          NewTex->TScale = NewTex->GetHeight()/OldTex->GetHeight();
          NewTex->SOffset = (int)floor(OldTex->SOffset*NewTex->SScale);
          NewTex->TOffset = (int)floor(OldTex->TOffset*NewTex->TScale);
          NewTex->Name = OldTex->Name;
          NewTex->Type = OldTex->Type;
          NewTex->TextureTranslation = OldTex->TextureTranslation;
          NewTex->HashNext = OldTex->HashNext;
          Textures[OldIdx] = NewTex;
          delete OldTex;
          OldTex = nullptr;
        }
      } else if (sc->Check("define")) {
        sc->ExpectName();
        VName Name = sc->Name;
        int LumpIdx = W_CheckNumForName(sc->Name, WADNS_Graphics);
        if (LumpIdx < 0 && sc->String.length() > 8) {
          VName Name8 = VName(*sc->String, VName::AddLower8);
          LumpIdx = W_CheckNumForName(Name8, WADNS_Graphics);
          //FIXME: fix name?
          if (LumpIdx >= 0) {
            if (onlyHiRes) GCon->Logf(NAME_Warning, "Texture: found short texture '%s' for long texture '%s'", *Name8, *Name);
            Name = Name8;
          }
        }
        sc->Check("force32bit");

        // dimensions
        sc->ExpectNumber();
        int Width = sc->Number;
        sc->ExpectNumber();
        int Height = sc->Number;
        if (LumpIdx < 0) continue;

        if (!onlyHiRes) continue;

        int OldIdx = CheckNumForName(Name, TEXTYPE_Overload, false);
        if (!r_hirestex && OldIdx < 0) continue; // don't replace

        // create new texture
        VTexture *NewTex = VTexture::CreateTexture(TEXTYPE_Overload, LumpIdx);
        if (!NewTex) continue;

        // repalce existing texture by adjusting scale and offsets
        NewTex->bWorldPanning = true;
        NewTex->SScale = NewTex->GetWidth()/Width;
        NewTex->TScale = NewTex->GetHeight()/Height;
        NewTex->Name = Name;

        if (OldIdx >= 0) {
          VTexture *OldTex = Textures[OldIdx];
          //fprintf(stderr, "REPLACE2 <%s> (%d)\n", *OldTex->Name, OldIdx);
          NewTex->TextureTranslation = OldTex->TextureTranslation;
          NewTex->HashNext = OldTex->HashNext;
          Textures[OldIdx] = NewTex;
          delete OldTex;
          OldTex = nullptr;
        } else {
          AddTexture(NewTex);
        }
      } else if (sc->Check("walltexture")) {
        if (!onlyHiRes) AddTexture(new VMultiPatchTexture(sc, TEXTYPE_Wall)); else sc->SkipBracketed(false);
      } else if (sc->Check("flat")) {
        if (!onlyHiRes) AddTexture(new VMultiPatchTexture(sc, TEXTYPE_Flat)); else sc->SkipBracketed(false);
      } else if (sc->Check("texture")) {
        if (!onlyHiRes) AddTexture(new VMultiPatchTexture(sc, TEXTYPE_Overload)); else sc->SkipBracketed(false);
      } else if (sc->Check("sprite")) {
        if (!onlyHiRes) AddTexture(new VMultiPatchTexture(sc, TEXTYPE_Sprite)); else sc->SkipBracketed(false);
      } else if (sc->Check("graphic")) {
        if (!onlyHiRes) AddTexture(new VMultiPatchTexture(sc, TEXTYPE_Pic)); else sc->SkipBracketed(false);
      } else {
        sc->Error(va("Bad command: '%s'", *sc->String));
      }
    }
    delete sc;
    sc = nullptr;
  }
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
    ad.Time = 0.0001; // force 1st game tic to animate
    ad.allowDecals = (Type == 3);
    AnimDefs.Append(ad);
  }
}


//==========================================================================
//
//  GetTextureIdWithOffset
//
//==========================================================================
static int GetTextureIdWithOffset (int txbase, int offset, int IsFlat) {
  //if (developer) GCon->Logf(NAME_Dev, "GetTextureIdWithOffset: txbase=%d; offset=%d; IsFlat=%d", txbase, offset, IsFlat);
  if (txbase <= 0) return -1; // oops
  if (offset < 0) return -1; // oops
  if (offset == 0) return txbase;
  int txtype = (IsFlat ? TEXTYPE_Flat : TEXTYPE_Wall);
  EWadNamespace txns = (IsFlat ? WADNS_Flats : WADNS_Global);
  VName txname = GTextureManager.GetTextureName(txbase);
  if (txname == NAME_None) {
    //if (developer) GCon->Logf(NAME_Dev, "GetTextureIdWithOffset: FOOO (txbase=%d; offset=%d; name=%s)", txbase, offset, *txname);
    return -1; // oops
  }
  int lmp = W_FindFirstLumpOccurence(txname, txns);
  if (lmp == -1 && !IsFlat) {
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
static void ParseFTAnim (VScriptParser *sc, int IsFlat) {
  AnimDef_t ad;
  FrameDef_t fd;

  memset(&ad, 0, sizeof(ad));

  // optional flag
  bool optional = false;
  if (sc->Check("optional")) optional = true;

  // name
  bool ignore = false;
  sc->ExpectName8Warn();
  ad.Index = GTextureManager.CheckNumForNameAndForce(sc->Name8, (IsFlat ? TEXTYPE_Flat : TEXTYPE_Wall), true, optional);
  if (ad.Index == -1) {
    ignore = true;
    if (!optional) GCon->Logf(NAME_Warning, "ANIMDEFS: Can't find texture \"%s\"", *sc->Name8);
  } else {
    animPicSeen.put(sc->Name8, true);
  }

  bool vanilla = false;
  float vanillaTics = 8;
  if (sc->Check("vanilla")) {
    vanilla = true;
    if (sc->Check("tics")) {
      sc->ExpectFloat();
      vanillaTics = sc->Float;
      if (vanillaTics < 0.1) vanillaTics = 0.1; // this is tics
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
        fd.Index = GTextureManager.CheckNumForNameAndForce(sc->Name8, (IsFlat ? TEXTYPE_Flat : TEXTYPE_Wall), true, optional);
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
            int txidx = GetTextureIdWithOffset(ad.Index, sc->Number-1, IsFlat);
            if (txidx == -1) {
              sc->Message(va("Cannot find %stexture '%s'+%d", (IsFlat ? "flat " : ""), *GTextureManager.GetTextureName(ad.Index), sc->Number-1));
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
                int txidx = GetTextureIdWithOffset(ad.Index, ofs, IsFlat);
                if (txidx == -1) {
                  sc->Message(va("Cannot find %stexture '%s'+%d", (IsFlat ? "flat " : ""), *GTextureManager.GetTextureName(ad.Index), ofs));
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
            fd.Index = GTextureManager.CheckNumForNameAndForce(sc->Name8, (IsFlat ? TEXTYPE_Flat : TEXTYPE_Wall), true, optional);
            if (fd.Index == -1 && !optional) sc->Message(va("Unknown texture \"%s\"", *sc->String));
            animPicSeen.put(sc->Name8, true);
          } else {
            // range
            check(CurType == 2);
            int txtype = (IsFlat ? TEXTYPE_Flat : TEXTYPE_Wall);
            BuildTextureRange(GTextureManager.GetTextureName(ad.Index), sc->Name8, txtype, ids, 64); // limit to 64 frames
            for (int f = 0; f < ids.length(); ++f) animPicSeen.put(GTextureManager.GetTextureName(ids[f]), true);
          }
        }
      }
    }

    if (sc->Check("tics")) {
      sc->ExpectFloat();
      fd.BaseTime = sc->Float;
      if (fd.BaseTime < 0.1) fd.BaseTime = 0.1; // this is tics
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
    ad.Time = 0.0001; // force 1st game tic to animate
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
//  InitFTAnims
//
//  Initialise flat and texture animation lists.
//
//==========================================================================
static void InitFTAnims () {
  if (GArgs.CheckParm("-disable-animdefs")) return;

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
void R_AnimateSurfaces () {
  // animate flats and textures
  for (int i = 0; i < AnimDefs.length(); ++i) {
    AnimDef_t &ad = AnimDefs[i];
    ad.Time -= host_frametime;
    for (int trycount = 128; trycount > 0; --trycount) {
      if (ad.Time > 0.0) break;

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

      ad.Time += fd.BaseTime/35.0;
      if (fd.RandomRange) ad.Time += Random()*(fd.RandomRange/35.0); // random tics

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
#endif


//==========================================================================
//
//  R_DumpTextures
//
//==========================================================================
void R_DumpTextures () {
  GCon->Log("=========================");
  GCon->Logf("texture count: %d", GTextureManager.Textures.length());
  for (int f = 0; f < GTextureManager.Textures.length(); ++f) {
    VTexture *tx = GTextureManager.Textures[f];
    if (!tx) { GCon->Logf(" %d: <EMPTY>", f); continue; }
    GCon->Logf("  %d: name='%s'; size=(%dx%d); srclump=%d", f, *tx->Name, tx->Width, tx->Height, tx->SourceLump);
  }
  GCon->Logf(" map-local texture count: %d", GTextureManager.MapTextures.length());
  for (int f = 0; f < GTextureManager.MapTextures.length(); ++f) {
    VTexture *tx = GTextureManager.MapTextures[f];
    if (!tx) { GCon->Logf(" %d: <EMPTY>", f+VTextureManager::FirstMapTextureIndex); continue; }
    GCon->Logf("  %d: name='%s'; size=(%dx%d); srclump=%d", f+VTextureManager::FirstMapTextureIndex, *tx->Name, tx->Width, tx->Height, tx->SourceLump);
  }

  GCon->Log("=========================");
  for (int hb = 0; hb < VTextureManager::HASH_SIZE; ++hb) {
    GCon->Logf(" hash bucket %05d", hb);
    for (int i = GTextureManager.TextureHash[hb]; i >= 0; i = GTextureManager.getTxByIndex(i)->HashNext) {
      VTexture *tx = GTextureManager.getTxByIndex(i);
      if (!tx) abort();
      GCon->Logf(NAME_Dev, "  %05d:%d: name='%s'; type=%d", hb, i, *tx->Name, tx->Type);
    }
  }
}


//==========================================================================
//
//  R_InitTexture
//
//==========================================================================
void R_InitTexture () {
  GTextureManager.Init();
  InitFTAnims(); // init flat and texture animations
  GTextureManager.WipeWallPatches();
  check(GTextureManager.MapTextures.length() == 0);
  if (developer) GTextureManager.DumpHashStats(NAME_Dev);
  if (GArgs.CheckParm("-dbg-dump-textures")) {
    R_DumpTextures();
  }
}


//==========================================================================
//
//  R_InitHiResTextures
//
//==========================================================================
void R_InitHiResTextures (bool onlyHiRes) {
  // initialise hires textures
  GTextureManager.AddTextureTextLumps(onlyHiRes);
}


//==========================================================================
//
//  R_ShutdownTexture
//
//==========================================================================
void R_ShutdownTexture () {
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

  // shut down texture manager
  GTextureManager.Shutdown();
}


//==========================================================================
//
//  VTextureManager::FillNameAutocompletion
//
//  to use in `ExportTexture` command
//
//==========================================================================
void VTextureManager::FillNameAutocompletion (const VStr &prefix, TArray<VStr> &list) {
  if (prefix.length() == 0) {
    GCon->Logf("\034KThere are about %d textures, be more specific, please!", Textures.length()+MapTextures.length()-1);
    return;
  }
  for (int f = 1; f < Textures.length(); ++f) {
    if (Textures[f]->Name == NAME_None) continue;
    if (VStr::startsWithNoCase(*Textures[f]->Name, *prefix)) list.append(VStr(Textures[f]->Name));
  }
  for (int f = 0; f < MapTextures.length(); ++f) {
    if (MapTextures[f]->Name == NAME_None) continue;
    if (VStr::startsWithNoCase(*MapTextures[f]->Name, *prefix)) list.append(VStr(MapTextures[f]->Name));
  }
}


//==========================================================================
//
//  VTextureManager::GetExistingTextureByName
//
//  to use in `ExportTexture` command
//
//==========================================================================
VTexture *VTextureManager::GetExistingTextureByName (const VStr &txname, int type) {
  VName nn = VName(*txname, VName::FindLower);
  if (nn == NAME_None) return nullptr;

  int idx = CheckNumForName(nn, type, true/*overloads*/);
  if (idx >= 0) return this->operator()(idx);
  return nullptr;
}


// ////////////////////////////////////////////////////////////////////////// //
extern "C" {
  static int sortCmpVStrCI (const void *a, const void *b, void *udata) {
    if (a == b) return 0;
    VStr *sa = (VStr *)a;
    VStr *sb = (VStr *)b;
    return sa->ICmp(*sb);
  }
}


//==========================================================================
//
//  ExportTexture
//
//==========================================================================
COMMAND_WITH_AC(ExportTexture) {
  if (Args.length() < 2 || Args.length() > 3) {
    GCon->Log(
      "usage: ExportTexture name [type]\n"
      "where \"type\" is one of:\n"
      "  any\n"
      "  patch\n"
      "  wall\n"
      "  flat\n"
      "  sprite\n"
      "  sky\n"
      "  skin\n"
      "  pic\n"
      "  autopage\n"
      "  fontchar"
      "");
    return;
  }

  int ttype = TEXTYPE_Any;
  if (Args.length() == 3) {
    const VStr &tstr = Args[2];
         if (tstr.ICmp("any") == 0) ttype = TEXTYPE_Any;
    else if (tstr.ICmp("patch") == 0) ttype = TEXTYPE_WallPatch;
    else if (tstr.ICmp("wall") == 0) ttype = TEXTYPE_Wall;
    else if (tstr.ICmp("flat") == 0) ttype = TEXTYPE_Flat;
    else if (tstr.ICmp("sprite") == 0) ttype = TEXTYPE_Sprite;
    else if (tstr.ICmp("sky") == 0) ttype = TEXTYPE_SkyMap;
    else if (tstr.ICmp("skin") == 0) ttype = TEXTYPE_Skin;
    else if (tstr.ICmp("pic") == 0) ttype = TEXTYPE_Pic;
    else if (tstr.ICmp("autopage") == 0) ttype = TEXTYPE_Autopage;
    else if (tstr.ICmp("fontchar") == 0) ttype = TEXTYPE_FontChar;
    else { GCon->Logf("unknown texture type '%s'", *tstr); return; }
  }

  VTexture *tx = GTextureManager.GetExistingTextureByName(Args[1], ttype);
  if (!tx) {
    GCon->Logf(NAME_Error, "Texture '%s' not found!", *Args[1]);
    return;
  }

  // find a file name to save it to
  VStr fname = va("%s.png", *tx->Name);
  auto strm = FL_OpenFileWrite(fname, true); // as full name
  if (!strm) {
    GCon->Logf(NAME_Error, "cannot create file '%s'", *fname);
    return;
  }

  tx->WriteToPNG(strm);
  delete strm;

  GCon->Logf("Exported texture '%s' of type '%s' to '%s'", *tx->Name, VTexture::TexTypeToStr(tx->Type), *fname);
}

COMMAND_AC(ExportTexture) {
  //if (aidx != 1) return VStr::EmptyString;
  TArray<VStr> list;
  VStr prefix = (aidx < args.length() ? args[aidx] : VStr());
  if (aidx == 1) {
    GTextureManager.FillNameAutocompletion(prefix, list);
    if (!list.length()) return VStr::EmptyString;
    // sort
    timsort_r(list.ptr(), list.length(), sizeof(VStr), &sortCmpVStrCI, nullptr);
    // remove possible duplicates
    int pos = 1;
    while (pos < list.length()) {
      if (list[pos].ICmp(list[pos-1]) == 0) {
        list.removeAt(pos);
      } else {
        ++pos;
      }
    }
    return AutoCompleteFromList(prefix, list, true); // return unchanged as empty
  } else if (aidx == 2) {
    // type
    list.append(VStr("any"));
    list.append(VStr("autopage"));
    list.append(VStr("flat"));
    list.append(VStr("fontchar"));
    list.append(VStr("patch"));
    list.append(VStr("pic"));
    list.append(VStr("skin"));
    list.append(VStr("sky"));
    list.append(VStr("sprite"));
    list.append(VStr("wall"));
    return AutoCompleteFromList(prefix, list, true); // return unchanged as empty
  } else {
    return VStr::EmptyString;
  }
}
