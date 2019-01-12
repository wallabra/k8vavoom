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
  //ANIM_Normal, // set texture with index `ad.Index` to `fd.Index`
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

VCvarB r_hirestex("r_hirestex", true, "Allow high-resolution texture replacements?", CVAR_Archive);
VCvarB r_showinfo("r_showinfo", false, "Show some info about loaded textures?", CVAR_Archive);

static VCvarB r_reupload_textures("r_reupload_textures", false, "Reupload textures to GPU when new map is loaded?", CVAR_Archive);


// ////////////////////////////////////////////////////////////////////////// //
// PRIVATE DATA DEFINITIONS ------------------------------------------------

static TArray<AnimDef_t> AnimDefs;
static TArray<FrameDef_t> FrameDefs;
static TArray<VAnimDoorDef> AnimDoorDefs;

//static TStrSet patchesWarned;
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
  guard(VTextureManager::Init);

  check(inMapTextures == 0);

  // add a dummy texture
  AddTexture(new VDummyTexture);

  // initialise wall textures
  AddTextures();

  // initialise flats
  AddGroup(TEXTYPE_Flat, WADNS_Flats);

  // initialise overloaded textures
  AddGroup(TEXTYPE_Overload, WADNS_NewTextures);

  // initialise sprites
  AddGroup(TEXTYPE_Sprite, WADNS_Sprites);

  // initialise hires textures
  AddHiResTextures();

  // find default texture
  DefaultTexture = CheckNumForName("-noflat-", TEXTYPE_Overload, false, false);
  if (DefaultTexture == -1) Sys_Error("Default texture -noflat- not found");

  // find sky flat number
  skyflatnum = CheckNumForName(NAME_f_sky, TEXTYPE_Flat, true, false);
  if (skyflatnum < 0) skyflatnum = CheckNumForName(NAME_f_sky001, TEXTYPE_Flat, true, false);
  if (skyflatnum < 0) skyflatnum = NumForName(NAME_f_sky1, TEXTYPE_Flat, true, false);

  unguard;
}


//==========================================================================
//
//  VTextureManager::Shutdown
//
//==========================================================================
void VTextureManager::Shutdown () {
  guard(VTextureManager::Shutdown);
  for (int i = 0; i < Textures.length(); ++i) { delete Textures[i]; Textures[i] = nullptr; }
  for (int i = 0; i < MapTextures.length(); ++i) { delete MapTextures[i]; MapTextures[i] = nullptr; }
  Textures.clear();
  MapTextures.clear();
  unguard;
}


//==========================================================================
//
//  VTextureManager::rehashTextures
//
//==========================================================================
void VTextureManager::rehashTextures () {
  for (int i = 0; i < HASH_SIZE; ++i) TextureHash[i] = -1;
  for (int f = 0; f < Textures.length(); ++f) if (Textures[f]) AddToHash(f);
  for (int f = 0; f < MapTextures.length(); ++f) if (MapTextures[f]) AddToHash(FirstMapTextureIndex+f);
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
  guard(VTextureManager::AddTexture);
  if (!Tex) return -1;
  if (!inMapTextures) {
    Textures.Append(Tex);
    Tex->TextureTranslation = Textures.length()-1;
    AddToHash(Textures.length()-1);
    return Textures.length()-1;
  } else {
    MapTextures.Append(Tex);
    Tex->TextureTranslation = FirstMapTextureIndex+MapTextures.length()-1;
    AddToHash(FirstMapTextureIndex+MapTextures.length()-1);
    return FirstMapTextureIndex+MapTextures.length()-1;
  }
  unguard;
}


//==========================================================================
//
//  VTextureManager::ReplaceTexture
//
//==========================================================================
void VTextureManager::ReplaceTexture (int Index, VTexture *NewTex) {
  guard(VTextureManager::ReplaceTexture);
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
  unguard;
}


//==========================================================================
//
//  VTextureManager::AddToHash
//
//==========================================================================
void VTextureManager::AddToHash (int Index) {
  guard(VTextureManager::AddToHash);
  int HashIndex = GetTypeHash(getTxByIndex(Index)->Name)&(HASH_SIZE-1);
  if (Index < FirstMapTextureIndex) {
    Textures[Index]->HashNext = TextureHash[HashIndex];
  } else {
    MapTextures[Index-FirstMapTextureIndex]->HashNext = TextureHash[HashIndex];
  }
  TextureHash[HashIndex] = Index;
  unguard;
}


//==========================================================================
//
//  VTextureManager::CheckNumForName
//
//  Check whether texture is available. Filter out NoTexture indicator.
//
//==========================================================================
int VTextureManager::CheckNumForName (VName Name, int Type, bool bOverload, bool bCheckAny) {
  guard(VTextureManager::CheckNumForName);
  // check for "NoTexture" marker
  if ((*Name)[0] == '-' && (*Name)[1] == 0) return 0;

  VName currname = VName(*Name, VName::AddLower);
  for (int trynum = 2; trynum > 0; --trynum) {
    if (trynum == 1) {
      // try short name too
      if (VStr::length(*Name) <= 8) break;
      currname = VName(*Name, VName::AddLower8);
    }
    int HashIndex = GetTypeHash(currname)&(HASH_SIZE-1);
    for (int i = TextureHash[HashIndex]; i >= 0; i = getTxByIndex(i)->HashNext) {
      if (i < 0 || i >= Textures.length()) continue;
      if (getTxByIndex(i)->Name != currname) continue;

      if (Type == TEXTYPE_Any || getTxByIndex(i)->Type == Type ||
          (bOverload && getTxByIndex(i)->Type == TEXTYPE_Overload))
      {
        if (getTxByIndex(i)->Type == TEXTYPE_Null) return 0;
        return i;
      }
      /*
      if ((Type == TEXTYPE_Wall && getTxByIndex(i)->Type == TEXTYPE_WallPatch) ||
          (Type == TEXTYPE_WallPatch && getTxByIndex(i)->Type == TEXTYPE_Wall))
      {
        if (getTxByIndex(i)->Type == TEXTYPE_Null) return 0;
        return i;
      }
      */
    }
  }

  if (bCheckAny && Type != TEXTYPE_Any) return CheckNumForName(Name, TEXTYPE_Any, bOverload, false);

  return -1;
  unguard;
}


//==========================================================================
//
//  VTextureManager::NumForName
//
//  Calls R_CheckTextureNumForName, aborts with error message.
//
//==========================================================================
int VTextureManager::NumForName (VName Name, int Type, bool bOverload, bool bCheckAny) {
  static TStrSet numForNameWarned;
  guard(VTextureManager::NumForName);
  int i = CheckNumForName(Name, Type, bOverload, bCheckAny);
  if (i == -1) {
    if (!numForNameWarned.put(*Name)) {
      GCon->Logf(NAME_Warning, "VTextureManager::NumForName: '%s' not found (type:%d; over:%d; any:%d)", *Name, (int)Type, (int)bOverload, (int)bCheckAny);
      /*
      if (VStr::ICmp(*Name, "ml_sky1") == 0) {
        GCon->Logf("!!!!!!!!!!!!!!!!!!");
        for (int f = 0; f < Textures.length(); ++f) {
          if (VStr::ICmp(*Name, *getTxByIndex(f)->Name) == 0) {
            GCon->Logf("****************** FOUND ******************");
          }
        }
      }
      */
    }
    i = DefaultTexture;
  }
  return i;
  unguard;
}


//==========================================================================
//
//  VTextureManager::FindTextureByLumpNum
//
//==========================================================================
int VTextureManager::FindTextureByLumpNum (int LumpNum) {
  guard(VTextureManager::FindTextureByLumpNum);
  for (int i = 0; i < Textures.length(); ++i) {
    if (Textures[i]->SourceLump == LumpNum) return i;
  }
  for (int i = 0; i < MapTextures.length(); ++i) {
    if (MapTextures[i]->SourceLump == LumpNum) return i+FirstMapTextureIndex;
  }
  return -1;
  unguard;
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
  guard(VTextureManager::AddPatch);

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

  /*
  // find the lump number
  //GCon->Logf("VTextureManager::AddPatch: '%s' (%d)", *Name, Type);
  int LumpNum = W_CheckNumForName(Name, WADNS_Graphics);
  if (LumpNum < 0) LumpNum = W_CheckNumForName(Name, WADNS_Sprites);
  if (LumpNum < 0) LumpNum = W_CheckNumForName(Name, WADNS_Global);
  if (LumpNum < 0) LumpNum = W_CheckNumForFileName(VStr(*Name));
  if (LumpNum < 0) {
    if (!patchesWarned.put(*VName(*Name, VName::AddLower))) {
      if (!Silent) {
        GCon->Logf(NAME_Warning, "VTextureManager::AddPatch: Pic \"%s\" not found", *Name);
      }
    }
    return -1;
  }
  */

  // create new patch texture
  //return AddTexture(VTexture::CreateTexture(Type, LumpNum));
  unguard;
}


//==========================================================================
//
//  VTextureManager::AddPatchLump
//
//==========================================================================
int VTextureManager::AddPatchLump (int LumpNum, VName Name, int Type, bool Silent) {
  guard(VTextureManager::AddPatchLump);

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

  unguard;
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
  guard(VTextureManager::AddRawWithPal);
  //TODO
  int LumpNum = W_CheckNumForName(Name, WADNS_Graphics);
  if (LumpNum < 0) LumpNum = W_CheckNumForName(Name, WADNS_Sprites);
  if (LumpNum < 0) LumpNum = W_CheckNumForName(Name, WADNS_Global);
  if (LumpNum < 0) LumpNum = W_CheckNumForFileName(VStr(*Name));
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
  unguard;
}


//==========================================================================
//
//  VTextureManager::AddFileTextureChecked
//
//  returns -1 if no file texture found
//
//==========================================================================
int VTextureManager::AddFileTextureChecked (VName Name, int Type) {
  guard(VTextureManager::AddFileTextureChecked)
  int i = CheckNumForName(Name, Type);
  if (i >= 0) return i;

  i = W_CheckNumForFileName(*Name);
  if (i >= 0) {
    VTexture *Tex = VTexture::CreateTexture(Type, i);
    if (Tex) {
      Tex->Name = Name;
      return AddTexture(Tex);
    }
  }

  return CheckNumForNameAndForce(Name, Type, true/*bOverload*/, false/*bCheckAny*/, true/*silent*/);

  unguard;
}


//==========================================================================
//
//  VTextureManager::AddFileTexture
//
//==========================================================================
int VTextureManager::AddFileTexture (VName Name, int Type) {
  guard(VTextureManager::AddFileTexture)
  int i = AddFileTextureChecked(Name, Type);
  if (i == -1) {
    GCon->Logf(NAME_Warning, "Couldn\'t create texture \"%s\".", *Name);
    return DefaultTexture;
  }
  return i;
  unguard;
}


//==========================================================================
//
//  VTextureManager::AddFileTextureShaded
//
//  shade==-1: don't shade
//
//==========================================================================
int VTextureManager::AddFileTextureShaded (VName Name, int Type, int shade) {
  guard(VTextureManager::AddFileTexture)
  if (shade == -1) return AddFileTexture(Name, Type);

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
  unguard;
}


//==========================================================================
//
//  VTextureManager::AddPatchShaded
//
//==========================================================================
int VTextureManager::AddPatchShaded (VName Name, int Type, int shade, bool Silent) {
  guard(VTextureManager::AddPatchShaded);
  if (shade == -1) return AddPatch(Name, Type, Silent);

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
  unguard;
}


//==========================================================================
//
//  VTextureManager::CheckNumForNameAndForce
//
//  find or force-load texture
//
//==========================================================================
int VTextureManager::CheckNumForNameAndForce (VName Name, int Type, bool bOverload, bool bCheckAny, bool silent) {
  int tidx = CheckNumForName(Name, Type, bOverload, bCheckAny);
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
    tidx = CheckNumForName(Name, Type, bOverload, bCheckAny);
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
void VTextureManager::AddTextures () {
  guard(VTextureManager::AddTextures);
  int NamesFile = -1;
  int LumpTex1 = -1;
  int LumpTex2 = -1;
  int FirstTex;

  // we have to force-load textures after adding textures lump, so
  // texture numbering for animations won't break
  TArray<VName> numberedNames;

  // for each PNAMES lump load TEXTURE1 and TEXTURE2 from the same wad
  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) != NAME_pnames) continue;
    NamesFile = W_LumpFile(Lump);
    LumpTex1 = W_CheckNumForNameInFile(NAME_texture1, NamesFile);
    LumpTex2 = W_CheckNumForNameInFile(NAME_texture2, NamesFile);
    FirstTex = Textures.length();
    AddTexturesLump(Lump, LumpTex1, FirstTex, true, numberedNames);
    AddTexturesLump(Lump, LumpTex2, FirstTex, false, numberedNames);
  }

  // if last TEXTURE1 or TEXTURE2 are in a wad without a PNAMES, they must be loaded too
  int LastTex1 = W_CheckNumForName(NAME_texture1);
  int LastTex2 = W_CheckNumForName(NAME_texture2);
  if (LastTex1 >= 0 && (LastTex1 == LumpTex1 || W_LumpFile(LastTex1) <= NamesFile)) LastTex1 = -1;
  if (LastTex2 >= 0 && (LastTex2 == LumpTex2 || W_LumpFile(LastTex2) <= NamesFile)) LastTex2 = -1;
  FirstTex = Textures.length();
  AddTexturesLump(W_GetNumForName(NAME_pnames), LastTex1, FirstTex, true, numberedNames);
  AddTexturesLump(W_GetNumForName(NAME_pnames), LastTex2, FirstTex, false, numberedNames);

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
        if (CheckNumForName(PatchName, TEXTYPE_WallPatch, false, false) < 0) {
          int tid = CheckNumForNameAndForce(PatchName, TEXTYPE_WallPatch, false, false, true);
          if (tid > 0) GCon->Logf(NAME_Init, "Textures: force-loaded numbered texture '%s'", nbuf);
        }
      }
    }
  }

  unguard;
}


//==========================================================================
//
//  VTextureManager::AddTexturesLump
//
//==========================================================================
void VTextureManager::AddTexturesLump (int NamesLump, int TexLump, int FirstTex, bool First, TArray<VName> &numberedNames) {
  guard(VTextureManager::AddTexturesLump);
  if (TexLump < 0) return;

  check(inMapTextures == 0);

  // load the patch names from pnames.lmp
  VStream *Strm = W_CreateLumpReaderNum(NamesLump);
  vint32 nummappatches = Streamer<vint32>(*Strm);
  VTexture **patchtexlookup = new VTexture *[nummappatches];
  for (int i = 0; i < nummappatches; ++i) {
    // read patch name
    char TmpName[12];
    Strm->Serialise(TmpName, 8);
    TmpName[8] = 0;

    if ((vuint8)TmpName[0] < 32 || (vuint8)TmpName[0] >= 127) {
      Sys_Error("TEXTURES: record #%d, name is <%s>", i, TmpName);
    }

    VName PatchName(TmpName, VName::AddLower8);

    {
      const char *txname = *PatchName;
      int namelen = VStr::length(txname);
      if (namelen && VStr::digitInBase(txname[namelen-1], 10) >= 0) numberedNames.append(PatchName);
    }

    // check if it's already has been added
    int PIdx = CheckNumForName(PatchName, TEXTYPE_WallPatch, false, false);
    if (PIdx >= 0) {
      patchtexlookup[i] = Textures[PIdx];
      continue;
    }

    // get wad lump number
    int LNum = W_CheckNumForName(PatchName, WADNS_Patches);
    // sprites also can be used as patches
    if (LNum < 0) LNum = W_CheckNumForName(PatchName, WADNS_Sprites);
    if (LNum < 0) LNum = W_CheckNumForName(PatchName, WADNS_Global); // just in case

    // add it to textures
    if (LNum < 0) {
      patchtexlookup[i] = nullptr;
    } else {
      patchtexlookup[i] = VTexture::CreateTexture(TEXTYPE_WallPatch, LNum);
      if (patchtexlookup[i]) AddTexture(patchtexlookup[i]);
    }

  }
  delete Strm;
  Strm = nullptr;

  // load the map texture definitions from textures.lmp
  // the data is contained in one or two lumps, TEXTURE1 for shareware, plus TEXTURE2 for commercial
  Strm = W_CreateLumpReaderNum(TexLump);
  vint32 NumTex = Streamer<vint32>(*Strm);

  // check the texture file format
  bool IsStrife = false;
  vint32 PrevOffset = Streamer<vint32>(*Strm);
  for (int i = 0; i < NumTex-1; ++i) {
    vint32 Offset = Streamer<vint32>(*Strm);
    if (Offset-PrevOffset == 24) {
      IsStrife = true;
      GCon->Logf(NAME_Init, "Strife textures detected in lump '%s'", *W_FullLumpName(TexLump));
      break;
    }
    PrevOffset = Offset;
  }

  for (int i = 0; i < NumTex; ++i) {
    VMultiPatchTexture *Tex = new VMultiPatchTexture(*Strm, i, patchtexlookup, nummappatches, FirstTex, IsStrife);
    AddTexture(Tex);
    if (i == 0 && First) {
      // copy dimensions of the first texture to the dummy texture in case they are used
      Textures[0]->Width = Tex->Width;
      Textures[0]->Height = Tex->Height;
      Tex->Type = TEXTYPE_Null;
    }
  }
  delete Strm;
  delete[] patchtexlookup;
  unguard;
}


//==========================================================================
//
//  VTextureManager::AddGroup
//
//==========================================================================
void VTextureManager::AddGroup (int Type, EWadNamespace Namespace) {
  guard(VTextureManager::AddGroup);
  for (int Lump = W_IterateNS(-1, Namespace); Lump >= 0; Lump = W_IterateNS(Lump, Namespace)) {
    // to avoid duplicates, add only the last one
    if (W_GetNumForName(W_LumpName(Lump), Namespace) != Lump) {
      //GCon->Logf(NAME_Dev, "VTextureManager::AddGroup(%d:%d): skipped lump '%s'", Type, Namespace, *W_FullLumpName(Lump));
      continue;
    }
    //GCon->Logf(NAME_Dev, "VTextureManager::AddGroup(%d:%d): loading lump '%s'", Type, Namespace, *W_FullLumpName(Lump));
    AddTexture(VTexture::CreateTexture(Type, Lump));
  }
  unguard;
}


//==========================================================================
//
//  VTextureManager::AddHiResTextures
//
//==========================================================================
void VTextureManager::AddHiResTextures () {
  check(inMapTextures == 0);

  guard(VTextureManager::AddHiResTextures);
  for (int Lump = W_IterateNS(-1, WADNS_HiResTextures); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_HiResTextures)) {
    VName Name = W_LumpName(Lump);
    // to avoid duplicates, add only the last one
    if (W_GetNumForName(Name, WADNS_HiResTextures) != Lump) continue;

    // create new texture
    VTexture *NewTex = VTexture::CreateTexture(TEXTYPE_Any, Lump);
    if (!NewTex) continue;

    // find texture to replace
    int OldIdx = CheckNumForName(Name, TEXTYPE_Wall, true, true);
    if (OldIdx < 0) OldIdx = AddPatch(Name, TEXTYPE_Pic, true);

    if (OldIdx < 0) {
      // add it as a new texture
      NewTex->Type = TEXTYPE_Overload;
      AddTexture(NewTex);
    } else {
      // repalce existing texture by adjusting scale and offsets
      VTexture *OldTex = Textures[OldIdx];
      //fprintf(stderr, "REPLACE0 <%s> (%d)\n", *OldTex->Name, OldIdx);
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
        int OldIdx = CheckNumForName(sc->Name8, Type, Overload, false);
        if (OldIdx < 0) OldIdx = AddPatch(sc->Name8, TEXTYPE_Pic, true);

        sc->ExpectName8Warn();
        int LumpIdx = W_CheckNumForName(sc->Name8, WADNS_Graphics);
        if (OldIdx < 0 || LumpIdx < 0) continue;

        // create new texture
        VTexture *NewTex = VTexture::CreateTexture(TEXTYPE_Any, LumpIdx);
        if (!NewTex) continue;
        // repalce existing texture by adjusting scale and offsets
        VTexture *OldTex = Textures[OldIdx];
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
      } else if (sc->Check("define")) {
        sc->ExpectName();
        VName Name = sc->Name;
        int LumpIdx = W_CheckNumForName(sc->Name, WADNS_Graphics);
        if (LumpIdx < 0 && sc->String.length() > 8) {
          VName Name8 = VName(*sc->String, VName::AddLower8);
          LumpIdx = W_CheckNumForName(Name8, WADNS_Graphics);
          //FIXME: fix name?
          if (LumpIdx >= 0) {
            GCon->Logf(NAME_Warning, "Texture: found short texture '%s' for long texture '%s'", *Name8, *Name);
            Name = Name8;
          }
        }
        sc->Check("force32bit");

        //  Dimensions
        sc->ExpectNumber();
        int Width = sc->Number;
        sc->ExpectNumber();
        int Height = sc->Number;
        if (LumpIdx < 0) continue;

        // create new texture
        VTexture *NewTex = VTexture::CreateTexture(TEXTYPE_Overload, LumpIdx);
        if (!NewTex) continue;

        // repalce existing texture by adjusting scale and offsets
        NewTex->bWorldPanning = true;
        NewTex->SScale = NewTex->GetWidth()/Width;
        NewTex->TScale = NewTex->GetHeight()/Height;
        NewTex->Name = Name;

        int OldIdx = CheckNumForName(Name, TEXTYPE_Overload, false, false);
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
        AddTexture(new VMultiPatchTexture(sc, TEXTYPE_Wall));
      } else if (sc->Check("flat")) {
        AddTexture(new VMultiPatchTexture(sc, TEXTYPE_Flat));
      } else if (sc->Check("texture")) {
        AddTexture(new VMultiPatchTexture(sc, TEXTYPE_Overload));
      } else if (sc->Check("sprite")) {
        AddTexture(new VMultiPatchTexture(sc, TEXTYPE_Sprite));
      } else if (sc->Check("graphic")) {
        AddTexture(new VMultiPatchTexture(sc, TEXTYPE_Pic));
      } else {
        sc->Error(va("Bad command: '%s'", *sc->String));
      }
    }
    delete sc;
    sc = nullptr;
  }
  unguard;
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
  int pic2lmp = W_FindFirstLumpOccurence(nlast, txns);
  if (pic1lmp == -1 && pic2lmp == -1) return; // invalid episode

  if (pic1lmp == -1) {
    if (developer) GCon->Logf(NAME_Dev, "BOOMANIM: first animtex '%s' not found (but '%s' is found)", *nfirst, *nlast);
    return;
  } else if (pic2lmp == -1) {
    if (developer) GCon->Logf(NAME_Dev, "BOOMANIM: second animtex '%s' not found (but '%s' is found)", *nlast, *nfirst);
    return;
  }

  if (GTextureManager.CheckNumForName(W_LumpName(pic1lmp), txtype, true, false) <= 0) {
    if (developer) GCon->Logf(NAME_Dev, "BOOMANIM: first animtex '%s' not a texture", *nfirst);
    return;
  }

  if (GTextureManager.CheckNumForName(W_LumpName(pic2lmp), txtype, true, false) <= 0) {
    if (developer) GCon->Logf(NAME_Dev, "BOOMANIM: second animtex '%s' not a texture", *nlast);
    return;
  }

  check(pic1lmp != -1);
  check(pic2lmp != -1);

  bool backward = (pic2lmp < pic1lmp);

  int start = (backward ? pic2lmp : pic1lmp);
  int end = (backward ? pic1lmp : pic2lmp);

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

  if (developer) GCon->Logf("=== %s : %s === (0x%08x : 0x%08x) [%s]", *nfirst, *nlast, pic1lmp, pic2lmp, *pfx);
  // find all textures in animation (up to arbitrary limit)
  // it is safe to not check for `-1` here, as it is guaranteed that the last texture is present
  for (; start <= end; start = W_IterateNS(start, txns)) {
    check(start != -1); // should not happen
    // check prefix
    if (pfx.length()) {
      const char *lname = *W_LumpName(start);
      if (!VStr::startsWith(lname, *pfx)) {
        if (developer) GCon->Logf("    PFX SKIP: %s : 0x%08x (0x%08x)", lname, start, end);
        continue;
      }
    }
    int txidx = GTextureManager.CheckNumForName(W_LumpName(start), txtype, true, false);
    if (developer) {
      GCon->Logf("  %s : 0x%08x (0x%08x)", (txidx == -1 ? "----" : *GTextureManager.GetTextureName(txidx)), start, end);
    }
    if (txidx == -1) continue;
    // if we have seen this texture in animdef, skip the whole sequence
    if (checkadseen) {
      if (animPicSeen.has(W_LumpName(start))) {
        if (developer) GCon->Logf(" SEEN IN ANIMDEF, SKIPPED");
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
  guard(P_InitAnimated);
  AnimDef_t ad;
  //FrameDef_t fd;

  int animlump = W_CheckNumForName(NAME_animated);
  if (animlump < 0) return;
  GCon->Logf(NAME_Init, "loading Boom animated lump from '%s'", *W_FullLumpName(animlump));

  VStream *Strm = W_CreateLumpReaderName(NAME_animated);
  while (Strm->TotalSize()-Strm->Tell() >= 23) {
    //int pic1, pic2;
    vint8 Type;
    char TmpName1[9];
    char TmpName2[9];
    vint32 BaseTime;

    memset(TmpName1, 0, sizeof(TmpName1));
    memset(TmpName2, 0, sizeof(TmpName2));

    *Strm << Type;
    if (Type == 255) break; // terminator marker

    if (Type != 0 && Type != 1 && Type != 3) Sys_Error("P_InitPicAnims: bad type %u (ofs:0x%08x)", (vuint32)Type, (vuint32)(Strm->Tell()-1));

    Strm->Serialise(TmpName1, 9);
    Strm->Serialise(TmpName2, 9);
    *Strm << BaseTime;

    if (!TmpName1[0] && !TmpName2[0]) {
      GCon->Log(NAME_Init, "ANIMATED: skipping empty entry");
      continue;
    }

    if (!TmpName1[0]) Sys_Error("P_InitPicAnims: empty first texture (ofs:0x%08x)", (vuint32)(Strm->Tell()-4-2*9-1));
    if (!TmpName2[0]) Sys_Error("P_InitPicAnims: empty second texture (ofs:0x%08x)", (vuint32)(Strm->Tell()-4-2*9-1));

    // 0 is flat, 1 is texture, 3 is texture with decals allowed
    int txtype = (Type&1 ? TEXTYPE_Wall : TEXTYPE_Flat);
    //EWadNamespace txns = (Type&1 ? WADNS_Global : WADNS_Flats);

    VName tn18 = VName(TmpName1, VName::AddLower8); // last
    VName tn28 = VName(TmpName2, VName::AddLower8); // first

    if (animPicSeen.find(tn18) || animPicSeen.find(tn28)) {
      GCon->Logf(NAME_Warning, "ANIMATED: skipping animation sequence between '%s' and '%s' due to animdef", TmpName1, TmpName2);
      continue;
    }

    //pic1 = GTextureManager.CheckNumForName(tn28, txtype, true, false);
    //pic2 = GTextureManager.CheckNumForName(tn18, txtype, true, false);

    // different episode ?
    //if (pic1 == -1 || pic2 == -1) continue;

    // if we have seen this texture in animdef, skip the whole sequence
    TArray<int> ids;
    BuildTextureRange(tn28, tn18, txtype, ids, 32, true); // limit to 32 frames

    if (ids.length() == 1) {
      if (developer) GCon->Logf(NAME_Dev, "BOOMANIM: ignored zero-step animtex sequence ('%s' -- '%s')", TmpName1, TmpName2);
    }
    if (ids.length() < 2) continue; // nothing to do

    if (developer) GCon->Logf(NAME_Dev, "BOOMANIM: found animtex sequence ('%s' -- '%s'): %d", TmpName1, TmpName2, ids.length());

    memset(&ad, 0, sizeof(ad));
    //memset(&fd, 0, sizeof(fd));

    ad.StartFrameDef = FrameDefs.length();
    ad.range = 1; // this is ranged animation
    //ad.Type = (pic2lmp > pic1lmp ? ANIM_Forward : ANIM_Backward);
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

  delete Strm;
  unguard;
}


//==========================================================================
//
//  GetTextureIdWithOffset
//
//==========================================================================
static int GetTextureIdWithOffset (int txbase, int offset, int IsFlat) {
  if (txbase <= 0) return -1; // oops
  if (offset < 0) return -1; // oops
  if (offset == 0) return txbase;
  int txtype = (IsFlat ? TEXTYPE_Flat : TEXTYPE_Wall);
  EWadNamespace txns = (IsFlat ? WADNS_Flats : WADNS_Global);
  VName txname = GTextureManager.GetTextureName(txbase);
  if (txname == NAME_None) return -1; // oops
  int lmp = W_FindFirstLumpOccurence(txname, txns);
  if (lmp == -1) return -1; // oops
  // now scan loaded paks until we skip enough textures
  for (;;) {
    lmp = W_IterateNS(lmp, txns); // skip one lump
    if (lmp == -1) break; // oops
    int txidx = GTextureManager.CheckNumForName(W_LumpName(lmp), txtype, true, false);
    if (!txidx) continue; // not a texture
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
  guard(ParseFTAnim);
  AnimDef_t ad;
  FrameDef_t fd;

  memset(&ad, 0, sizeof(ad));

  // optional flag
  bool optional = false;
  if (sc->Check("optional")) optional = true;

  // name
  bool ignore = false;
  sc->ExpectName8Warn();
  ad.Index = GTextureManager.CheckNumForNameAndForce(sc->Name8, (IsFlat ? TEXTYPE_Flat : TEXTYPE_Wall), true, true, !optional);
  if (ad.Index == -1) {
    ignore = true;
    if (!optional) GCon->Logf(NAME_Warning, "ANIMDEFS: Can't find texture \"%s\"", *sc->Name8);
  } else {
    animPicSeen.put(sc->Name8, true);
  }
  //VName adefname = sc->Name8;
  bool missing = ignore && optional;

  int CurType = 0;
  ad.StartFrameDef = FrameDefs.length();
  ad.Type = ANIM_Forward; //ANIM_Normal;
  ad.allowDecals = 0;
  ad.range = 0; // for now
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
      if (CurType == 2) sc->Error("You can only use range once in a single animation.");
      if (CurType == 1) sc->Error("You cannot use range together with pic.");
      CurType = 2;
      if (ad.Type != ANIM_Random) ad.Type = ANIM_Forward;
      ad.range = 1;
    } else {
      break;
    }

    memset(&fd, 0, sizeof(fd));

    if (sc->CheckNumber()) {
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
              }
              ids.append(txidx);
            }
          }
        }
      }
      //fd.Index = ad.Index+sc->Number-1;
    } else {
      sc->ExpectName8Warn();
      if (!ignore) {
        if (!ad.range) {
          // simple pic
          check(CurType == 1);
          fd.Index = GTextureManager.CheckNumForNameAndForce(sc->Name8, (IsFlat ? TEXTYPE_Flat : TEXTYPE_Wall), true, true, false);
          if (fd.Index == -1 && !missing) sc->Message(va("Unknown texture \"%s\"", *sc->String));
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
    } else {
      sc->Error(va("bad command (%s)", *sc->String));
    }

    /*
    if (ad.Type != ANIM_Normal && ad.Type != ANIM_Random) {
      if (fd.Index < ad.Index) {
        int tmp = ad.Index;
        ad.Index = fd.Index;
        fd.Index = tmp;
        ad.Type = ANIM_Backward;
      }
    }
    */

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
      check(ad.range == 0);
      FrameDefs.Append(fd);
    }
  }

  /*
  if (!ignore && ad.Type == ANIM_Normal && FrameDefs.length()-ad.StartFrameDef < 2) {
    sc->Error(va("AnimDef '%s' has framecount < 2", *adefname));
  }
  */

  if (!ignore && FrameDefs.length() > ad.StartFrameDef) {
    ad.NumFrames = FrameDefs.length()-ad.StartFrameDef;
    ad.CurrentFrame = (ad.Type != ANIM_Random ? ad.NumFrames-1 : (int)(Random()*ad.NumFrames));
    /*
    if (ad.Type == ANIM_Normal) {
      ad.NumFrames = FrameDefs.length()-ad.StartFrameDef;
      ad.CurrentFrame = ad.NumFrames-1;
    } else {
      ad.NumFrames = FrameDefs[ad.StartFrameDef].Index-ad.Index+1;
      if (ad.Type != ANIM_Random) ad.CurrentFrame = 0; else ad.CurrentFrame = (int)(Random()*ad.NumFrames);
    }
    */
    ad.Time = 0.0001; // force 1st game tic to animate
    AnimDefs.Append(ad);
  }
  unguard;
}


//==========================================================================
//
//  AddSwitchDef
//
//==========================================================================
static int AddSwitchDef (TSwitch *Switch) {
  guard(AddSwitchDef);
  for (int i = 0; i < Switches.length(); ++i) {
    if (Switches[i]->Tex == Switch->Tex) {
      delete Switches[i];
      Switches[i] = nullptr;
      Switches[i] = Switch;
      return i;
    }
  }
  return Switches.Append(Switch);
  unguard;
}


//==========================================================================
//
//  ParseSwitchState
//
//==========================================================================
static TSwitch *ParseSwitchState (VScriptParser *sc, bool IgnoreBad) {
  guard(ParseSwitchState);
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
      int Tex = GTextureManager.CheckNumForNameAndForce(sc->Name8, TEXTYPE_Wall, true, false, /*false*/IgnoreBad || silentTexError);
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
  unguard;
}


//==========================================================================
//
//  ParseSwitchDef
//
//==========================================================================
static void ParseSwitchDef (VScriptParser *sc) {
  guard(ParseSwitchDef);
  bool silentTexError = (GArgs.CheckParm("-Wswitch-textures") == 0);

  // skip game specifier
       if (sc->Check("doom")) { /*sc->ExpectNumber();*/ sc->CheckNumber(); }
  else if (sc->Check("heretic")) {}
  else if (sc->Check("hexen")) {}
  else if (sc->Check("strife")) {}
  else if (sc->Check("any")) {}

  // switch texture
  sc->ExpectName8Warn();
  int t1 = GTextureManager.CheckNumForNameAndForce(sc->Name8, TEXTYPE_Wall, true, false, silentTexError);
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
    if (Def1) {
      delete Def1;
      Def1 = nullptr;
    }
    if (Def2) {
      delete Def2;
      Def2 = nullptr;
    }
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
  if (Def1->Tex == Def2->Tex) sc->Error("On state must not end on base texture");
  Def1->Quest = Quest;
  Def2->Quest = Quest;
  Def2->PairIndex = AddSwitchDef(Def1);
  Def1->PairIndex = AddSwitchDef(Def2);
  unguard;
}


//==========================================================================
//
//  ParseAnimatedDoor
//
//==========================================================================
static void ParseAnimatedDoor (VScriptParser *sc) {
  guard(ParseAnimatedDoor);
  // get base texture name
  bool ignore = false;
  sc->ExpectName8Warn();
  vint32 BaseTex = GTextureManager.CheckNumForNameAndForce(sc->Name8, TEXTYPE_Wall, true, true, false);
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
        v = GTextureManager.CheckNumForNameAndForce(sc->Name8, TEXTYPE_Wall, true, true, false);
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
  unguard;
};


//==========================================================================
//
//  ParseWarp
//
//==========================================================================
static void ParseWarp (VScriptParser *sc, int Type) {
  guard(ParseWarp);
  int TexType = TEXTYPE_Wall;
       if (sc->Check("texture")) TexType = TEXTYPE_Wall;
  else if (sc->Check("flat")) TexType = TEXTYPE_Flat;
  else sc->Error("Texture type expected");

  sc->ExpectName8Warn();
  int TexNum = GTextureManager.CheckNumForNameAndForce(sc->Name8, TexType, true, true, false);
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
  unguard;
}


//==========================================================================
//
//  ParseCameraTexture
//
//==========================================================================
static void ParseCameraTexture (VScriptParser *sc) {
  guard(ParseCameraTexture);
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
    int TexNum = GTextureManager.CheckNumForNameAndForce(Name, TEXTYPE_Flat, true, true, false);
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

  unguard;
}


//==========================================================================
//
//  ParseFTAnims
//
//  Initialise flat and texture animation lists.
//
//==========================================================================
static void ParseFTAnims (VScriptParser *sc) {
  guard(ParseFTAnims);
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
  unguard;
}


//==========================================================================
//
//  InitFTAnims
//
//  Initialise flat and texture animation lists.
//
//==========================================================================
static void InitFTAnims () {
  guard(InitFTAnims);

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

  unguard;
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
  guard(P_InitSwitchList);
  int lump = W_CheckNumForName(NAME_switches);
  if (lump != -1) {
    VStream *Strm = W_CreateLumpReaderNum(lump);
    while (Strm->TotalSize()-Strm->Tell() >= 20) {
      char TmpName1[9];
      char TmpName2[9];
      vint16 Episode;

      // read data
      Strm->Serialise(TmpName1, 9);
      Strm->Serialise(TmpName2, 9);
      *Strm << Episode;
      if (!Episode) break; // terminator marker
      TmpName1[8] = 0;
      TmpName2[8] = 0;

      // Check for switches that aren't really switches
      if (!VStr::ICmp(TmpName1, TmpName2)) {
        GCon->Logf(NAME_Warning, "Switch \"%s\" in SWITCHES has the same 'on' state", TmpName1);
        continue;
      }
      int t1 = GTextureManager.CheckNumForNameAndForce(VName(TmpName1, VName::AddLower8), TEXTYPE_Wall, true, false, false);
      int t2 = GTextureManager.CheckNumForNameAndForce(VName(TmpName2, VName::AddLower8), TEXTYPE_Wall, true, false, false);
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
    delete Strm;
    Strm = nullptr;
  }
  Switches.Condense();
  unguard;
}


//==========================================================================
//
//  R_FindAnimDoor
//
//==========================================================================
VAnimDoorDef *R_FindAnimDoor (vint32 BaseTex) {
  guard(R_FindAnimDoor);
  for (int i = 0; i < AnimDoorDefs.length(); ++i) {
    if (AnimDoorDefs[i].Texture == BaseTex) return &AnimDoorDefs[i];
  }
  return nullptr;
  unguard;
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
  /*
  const int len = AnimDefs.length();
  for (int i = 0; i < len; ++i) {
    AnimDef_t &ad = AnimDefs[i];
    if (!ad.range) {
      if (texid == ad.Index) return true;
    } else {
    }
  }
  return false;
  */
  return animTexMap.has(texid);
}


//==========================================================================
//
//  R_AnimateSurfaces
//
//==========================================================================
#ifdef CLIENT
void R_AnimateSurfaces () {
  guard(R_AnimateSurfaces);
  // animate flats and textures
  for (int i = 0; i < AnimDefs.length(); ++i) {
    AnimDef_t &ad = AnimDefs[i];
    ad.Time -= host_frametime;
    for (int trycount = 128; trycount > 0; --trycount) {
      if (ad.Time > 0.0) break;

      bool validAnimation = true;
      if (ad.NumFrames > 1) {
        switch (ad.Type) {
          //case ANIM_Normal:
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

      //const frameDef_t &fd = FrameDefs[ad.StartFrameDef+(ad.Type == ANIM_Normal ? ad.CurrentFrame : 0)];
      //fprintf(stderr, "ANIM #%d: texture %d (%s); type=%d; curframe=%d; framenum=%d; fdefs=%d; stfdef=%d; cfr=%d\n", i, ad.Index, *GTextureManager[ad.Index]->Name, (int)ad.Type, ad.CurrentFrame, ad.NumFrames, FrameDefs.length(), ad.StartFrameDef, ad.StartFrameDef+ad.CurrentFrame);

      //const FrameDef_t &fd = FrameDefs[ad.StartFrameDef+(validAnimation ? (ad.Type == ANIM_Normal ? ad.CurrentFrame : 0) : 0)];
      //old:const frameDef_t &fd = FrameDefs[ad.StartFrameDef+(validAnimation ? ad.CurrentFrame : 0)];

      const FrameDef_t &fd = FrameDefs[ad.StartFrameDef+(ad.range ? 0 : ad.CurrentFrame)];

      ad.Time += fd.BaseTime/35.0;
      if (fd.RandomRange) ad.Time += Random()*(fd.RandomRange/35.0); // random tics

      /*
      static int wantMissingAnimWarning = -1;
      if (wantMissingAnimWarning < 0) wantMissingAnimWarning = (GArgs.CheckParm("-Wmissing-anim") ? 1 : 0);
      */

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

#if 0
      VTexture *atx = GTextureManager[ad.Index];
      if (atx) {
        atx->noDecals = (ad.allowDecals == 0);
        atx->animNoDecals = (ad.allowDecals == 0);
        atx->animated = true;
        atx->TextureTranslation = ad.Index;
      }

      /*
      if (!validAnimation) {
        if (atx) atx->TextureTranslation = -1;
      } else {
        // fix all animated textures
        for (int fn = 0; fn < ad.NumFrames; ++fn) {
          atx = GTextureManager[FrameDefs[ad.StartFrameDef+fn].Index];
          if (atx) {
            atx->TextureTranslation = ad.Index+(ad.CurrentFrame+fn)%ad.NumFrames;
            if (atx->TextureTranslation == -1) {
              atx->TextureTranslation = ad.Index+fn;
              if (wantMissingAnimWarning < 0) wantMissingAnimWarning = (GArgs.CheckParm("-Wmissing-anim") ? 1 : 0);
              if (wantMissingAnimWarning) {
                GCon->Logf(NAME_Warning, "(1) animated surface got invalid texture index");
              }
            }
            atx->noDecals = (ad.allowDecals == 0);
            atx->animNoDecals = (ad.allowDecals == 0);
            atx->animated = true;
          }
        }
      }
      */

      if (ad.Type == ANIM_Normal || !validAnimation) {
        if (atx) {
          atx->TextureTranslation = fd.Index;
          if (atx->TextureTranslation == -1) {
            //k8:dunno
            atx->TextureTranslation = ad.Index;
            if (wantMissingAnimWarning < 0) wantMissingAnimWarning = (GArgs.CheckParm("-Wmissing-anim") ? 1 : 0);
            if (wantMissingAnimWarning) {
              GCon->Logf(NAME_Warning, "(0:%d) animated surface got invalid texture index (texidx=%d; '%s'); valid=%d; animtype=%d; curfrm=%d; numfrm=%d", fd.Index, ad.Index, *GTextureManager[ad.Index]->Name, (int)validAnimation, (int)ad.Type, ad.CurrentFrame, ad.NumFrames);
            }
          }
        }
      } else {
        for (int fn = 0; fn < ad.NumFrames; ++fn) {
          atx = GTextureManager[ad.Index+fn];
          if (atx) {
            atx->TextureTranslation = ad.Index+(ad.CurrentFrame+fn)%ad.NumFrames;
            if (atx->TextureTranslation == -1) {
              atx->TextureTranslation = ad.Index+fn;
              if (wantMissingAnimWarning < 0) wantMissingAnimWarning = (GArgs.CheckParm("-Wmissing-anim") ? 1 : 0);
              if (wantMissingAnimWarning) {
                GCon->Logf(NAME_Warning, "(1) animated surface got invalid texture index");
              }
            }
            atx->noDecals = (ad.allowDecals == 0);
            atx->animNoDecals = (ad.allowDecals == 0);
            atx->animated = true;
          }
        }
      }
#endif
    }
  }
  unguard;
}
#endif


//==========================================================================
//
//  R_InitTexture
//
//==========================================================================
void R_InitTexture () {
  guard(R_InitTexture);
  GTextureManager.Init();
  InitFTAnims(); // init flat and texture animations
  //GTextureManager.FinishedKnownTextures();
  unguard;
}


//==========================================================================
//
//  R_ShutdownTexture
//
//==========================================================================
void R_ShutdownTexture () {
  guard(R_ShutdownTexture);
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
  unguard;
}
