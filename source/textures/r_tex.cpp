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


enum {
  ANIM_Normal,
  ANIM_Forward,
  ANIM_Backward,
  ANIM_OscillateUp,
  ANIM_OscillateDown,
  ANIM_Random,
};


struct frameDef_t {
  vint16 Index;
  vint16 BaseTime;
  vint16 RandomRange;
};


struct animDef_t {
  vint16 Index;
  vint16 NumFrames;
  float Time;
  vint16 StartFrameDef;
  vint16 CurrentFrame;
  vuint8 Type;
  int allowDecals;
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


// ////////////////////////////////////////////////////////////////////////// //
// PRIVATE DATA DEFINITIONS ------------------------------------------------

static TArray<animDef_t> AnimDefs;
static TArray<frameDef_t> FrameDefs;
static TArray<VAnimDoorDef> AnimDoorDefs;

//static TStrSet patchesWarned;
static TMapNC<VName, bool> patchesWarned;


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
VTextureManager::VTextureManager()
  : DefaultTexture(-1)
  , Time(0)
{
  for (int i = 0; i < HASH_SIZE; ++i) TextureHash[i] = -1;
}


//==========================================================================
//
//  VTextureManager::Init
//
//==========================================================================
void VTextureManager::Init() {
  guard(VTextureManager::Init);
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
  for (int i = 0; i < Textures.Num(); ++i) { delete Textures[i]; Textures[i] = nullptr; }
  Textures.Clear();
  unguard;
}


//==========================================================================
//
//  VTextureManager::AddTexture
//
//==========================================================================
int VTextureManager::AddTexture (VTexture *Tex) {
  guard(VTextureManager::AddTexture);
  if (!Tex) return -1;
  //if (Textures.length() > 0 && Tex->Name == NAME_None) *(int*)0 = 0;
  //GCon->Logf("AddTexture0: <%s>; i=%d; %p  (%p)", *Tex->Name, Textures.length(), Tex, Textures.ptr());
  //if (Textures.length() > 2666) fprintf(stderr, "  [2666]=%p <%s>  (%p)\n", Textures[2666], *Textures[2666]->Name, Textures.ptr());
  Textures.Append(Tex);
  Tex->TextureTranslation = Textures.Num()-1;
  AddToHash(Textures.Num()-1);
  //fprintf(stderr, "AddTexture1: <%s>; i=%d; %p  (%p)\n", *Textures[Textures.Num()-1]->Name, Textures.length(), Textures[Textures.Num()-1], Textures.ptr());
  return Textures.Num()-1;
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
  check(Index < Textures.Num());
  check(NewTex);
  VTexture *OldTex = Textures[Index];
  //int HashIndex = GetTypeHash(Textures[Index]->Name)&(HASH_SIZE-1);
  //fprintf(stderr, "ReplaceTexture: <%s>; HashIndex=%d; i=%d; len=%d; [hi]=%d; HashNext=%d\n", *Textures[Index]->Name, HashIndex, Index, Textures.length(), TextureHash[HashIndex], OldTex->HashNext);
  NewTex->Name = OldTex->Name;
  NewTex->Type = OldTex->Type;
  NewTex->TextureTranslation = OldTex->TextureTranslation;
  NewTex->HashNext = OldTex->HashNext;
  Textures[Index] = NewTex;
  unguard;
}


//==========================================================================
//
//  VTextureManager::AddToHash
//
//==========================================================================
void VTextureManager::AddToHash (int Index) {
  guard(VTextureManager::AddToHash);
  int HashIndex = GetTypeHash(Textures[Index]->Name)&(HASH_SIZE-1);
  //fprintf(stderr, "AddToHash: <%s>; HashIndex=%d; i=%d; len=%d; [hi]=%d\n", *Textures[Index]->Name, HashIndex, Index, Textures.length(), TextureHash[HashIndex]);
  Textures[Index]->HashNext = TextureHash[HashIndex];
  TextureHash[HashIndex] = Index;
  //HashIndex = 1006;
  /*
  if (HashIndex == 1006) {
    for (int n = TextureHash[HashIndex]; n >= 0; n = Textures[n]->HashNext) {
      if (n >= 0 && n < Textures.length()) {
        fprintf(stderr, "  n=%d  <%s>  %p\n", n, *Textures[n]->Name, Textures[n]);
      } else {
        fprintf(stderr, "  n=%d  <#$$#^&@!%%@$5>\n", n);
      }
    }
  }
  */
  unguard;
}


//==========================================================================
//
//  VTextureManager::RemoveFromHash
//
//==========================================================================
void VTextureManager::RemoveFromHash (int Index) {
  guard(VTextureManager::RemoveFromHash);
  int HashIndex = GetTypeHash(Textures[Index]->Name)&(HASH_SIZE-1);
  //fprintf(stderr, "RemoveFromHash: <%s>; HashIndex=%d; i=%d; len=%d; [hi]=%d\n", *Textures[Index]->Name, HashIndex, Index, Textures.length(), TextureHash[HashIndex]);
  int *Prev = &TextureHash[HashIndex];
  while (*Prev != -1 && *Prev != Index) {
    Prev = &Textures[*Prev]->HashNext;
  }
  check(*Prev != -1);
  *Prev = Textures[Index]->HashNext;
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
    for (int i = TextureHash[HashIndex]; i >= 0; i = Textures[i]->HashNext) {
      //fprintf(stderr, "CheckNumForName: <%s>; HashIndex=%d; i=%d; len=%d\n", *currname, HashIndex, i, Textures.length());
      if (i < 0 || i >= Textures.length()) continue;
      if (Textures[i]->Name != currname) continue;
      //GCon->Logf("CheckNumForName: <%s>; HashIndex=%d; i=%d; len=%d; type=%d", *currname, HashIndex, i, Textures.length(), Textures[i]->Type);

      if (Type == TEXTYPE_Any || Textures[i]->Type == Type ||
          (bOverload && Textures[i]->Type == TEXTYPE_Overload))
      {
        if (Textures[i]->Type == TEXTYPE_Null) return 0;
        return i;
      }
      /*
      if ((Type == TEXTYPE_Wall && Textures[i]->Type == TEXTYPE_WallPatch) ||
          (Type == TEXTYPE_WallPatch && Textures[i]->Type == TEXTYPE_Wall))
      {
        if (Textures[i]->Type == TEXTYPE_Null) return 0;
        return i;
      }
      */
    }
  }

  if (bCheckAny) return CheckNumForName(Name, TEXTYPE_Any, bOverload, false);

#if 0
  if (VStr::Cmp(*Name, "ml_sky1") == 0 /*|| VStr::Cmp(*Name, "ml_sky2") == 0 || VStr::Cmp(*Name, "ml_sky3") == 0*/) {
    for (int f = 0; f < Textures.length(); ++f) {
      fprintf(stderr, "#%d: %d:<%s>\n", f, Textures[f]->Type, *Textures[f]->Name);
    }
  }
#endif

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
          if (VStr::ICmp(*Name, *Textures[f]->Name) == 0) {
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
  for (int i = 0; i < Textures.Num(); ++i) {
    if (Textures[i]->SourceLump == LumpNum) return i;
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
  guard(VTextureManager::GetTextureName);
  if (TexNum < 0 || TexNum >= Textures.Num()) return NAME_None;
  return Textures[TexNum]->Name;
  unguard;
}


//==========================================================================
//
//  VTextureManager::TextureWidth
//
//==========================================================================
float VTextureManager::TextureWidth (int TexNum) {
  guard(VTextureManager::TextureWidth);
  return Textures[TexNum]->GetWidth()/Textures[TexNum]->SScale;
  unguard;
}


//==========================================================================
//
//  VTextureManager::TextureHeight
//
//==========================================================================
float VTextureManager::TextureHeight (int TexNum) {
  guard(VTextureManager::TextureHeight);
  return Textures[TexNum]->GetHeight()/Textures[TexNum]->TScale;
  unguard;
}


//==========================================================================
//
//  VTextureManager::SetFrontSkyLayer
//
//==========================================================================
void VTextureManager::SetFrontSkyLayer (int tex) {
  guard(VTextureManager::SetFrontSkyLayer);
  Textures[tex]->SetFrontSkyLayer();
  unguard;
}


//==========================================================================
//
//  VTextureManager::GetTextureInfo
//
//==========================================================================
void VTextureManager::GetTextureInfo (int TexNum, picinfo_t *info) {
  guard(VTextureManager::GetTextureInfo);
  if (TexNum < 0) {
    memset((void *)info, 0, sizeof(*info));
  } else {
    VTexture *Tex = Textures[TexNum];
    info->width = Tex->GetWidth();
    info->height = Tex->GetHeight();
    info->xoffset = Tex->SOffset;
    info->yoffset = Tex->TOffset;
  }
  unguard;
}


//==========================================================================
//
//  VTextureManager::findAndLoadTexture
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

  return -1;
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
  if (shade < 1) return AddFileTexture(Name, Type);

  VName shName = VName(va("%s %06x", *Name, (vuint32)shade));

  /*
  static TMapNC<VName, int> shadeMap;
  auto txf = shadeMap.find(shName);
  if (txf) return *txf;
  */

  int i = CheckNumForName(shName, Type);
  if (i >= 0) return i;

  i = W_CheckNumForFileName(*Name);
  if (i >= 0) {
    VTexture *Tex = VTexture::CreateTexture(Type, i);
    if (Tex) {
      //shadeMap.put(shName, Tex);
      Tex->Name = shName;
      if (shade > 0) Tex->Shade(shade);
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
//  CheckNumForNameAndForce
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
//  Initialises the texture list with the textures from the world map.
//
//==========================================================================
void VTextureManager::AddTextures () {
  guard(VTextureManager::AddTextures);
  int NamesFile = -1;
  int LumpTex1 = -1;
  int LumpTex2 = -1;
  int FirstTex;

  // for each PNAMES lump load TEXTURE1 and TEXTURE2 from the same wad
  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) != NAME_pnames) continue;
    NamesFile = W_LumpFile(Lump);
    LumpTex1 = W_CheckNumForNameInFile(NAME_texture1, NamesFile);
    LumpTex2 = W_CheckNumForNameInFile(NAME_texture2, NamesFile);
    FirstTex = Textures.Num();
    AddTexturesLump(Lump, LumpTex1, FirstTex, true);
    AddTexturesLump(Lump, LumpTex2, FirstTex, false);
  }

  // if last TEXTURE1 or TEXTURE2 are in a wad without a PNAMES, they must be loaded too
  int LastTex1 = W_CheckNumForName(NAME_texture1);
  int LastTex2 = W_CheckNumForName(NAME_texture2);
  if (LastTex1 >= 0 && (LastTex1 == LumpTex1 || W_LumpFile(LastTex1) <= NamesFile)) LastTex1 = -1;
  if (LastTex2 >= 0 && (LastTex2 == LumpTex2 || W_LumpFile(LastTex2) <= NamesFile)) LastTex2 = -1;
  FirstTex = Textures.Num();
  AddTexturesLump(W_GetNumForName(NAME_pnames), LastTex1, FirstTex, true);
  AddTexturesLump(W_GetNumForName(NAME_pnames), LastTex2, FirstTex, false);
  unguard;
}


//==========================================================================
//
//  VTextureManager::AddTexturesLump
//
//==========================================================================
void VTextureManager::AddTexturesLump (int NamesLump, int TexLump, int FirstTex, bool First) {
  guard(VTextureManager::AddTexturesLump);
  if (TexLump < 0) return;

  // load the patch names from pnames.lmp
  VStream *Strm = W_CreateLumpReaderNum(NamesLump);
  vint32 nummappatches = Streamer<vint32>(*Strm);
  VTexture **patchtexlookup = new VTexture*[nummappatches];
  for (int i = 0; i < nummappatches; ++i) {
    // read patch name
    char TmpName[12];
    Strm->Serialise(TmpName, 8);
    TmpName[8] = 0;

    if ((vuint8)TmpName[0] < 32 || (vuint8)TmpName[0] >= 127) {
      Sys_Error("TEXTURES: record #%d, name is <%s>", i, TmpName);
    }

    VName PatchName(TmpName, VName::AddLower8);

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

  //k8: force-load numbered textures
  for (int i = 0; i < nummappatches; ++i) {
    VTexture *tex = patchtexlookup[i];
    if (!tex) continue;
    const char *txname = *tex->Name;
    int namelen = VStr::length(txname);
    if (namelen && txname[namelen-1] == '1') {
      char nbuf[130];
      snprintf(nbuf, sizeof(nbuf), "%s", txname);
      for (int f = 2; f < 10; ++f) {
        nbuf[namelen-1] = '0'+f;
        VName PatchName(nbuf, VName::AddLower8);
        int PIdx = CheckNumForName(PatchName, TEXTYPE_WallPatch, false, false);
        if (PIdx >= 0) continue;
        // get wad lump number
        int LNum = W_CheckNumForName(PatchName, WADNS_Patches);
        // sprites also can be used as patches
        if (LNum < 0) LNum = W_CheckNumForName(PatchName, WADNS_Sprites);
        if (LNum < 0) LNum = W_CheckNumForName(PatchName, WADNS_Global); // just in case
        // add it to textures
        if (LNum >= 0) {
          tex = VTexture::CreateTexture(TEXTYPE_WallPatch, LNum);
          if (tex) {
            GCon->Logf(NAME_Init, "Textures: force-loaded numbered texture '%s'", nbuf);
            AddTexture(tex);
          }
        }
      }
    }
  }

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
      GCon->Log(NAME_Init, "Strife textures detected");
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
  Strm = nullptr;
  delete[] patchtexlookup;
  patchtexlookup = nullptr;
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
    if (W_GetNumForName(W_LumpName(Lump), Namespace) != Lump) continue;
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

        sc->ExpectName8();
        int OldIdx = CheckNumForName(sc->Name8, Type, Overload, false);
        if (OldIdx < 0) OldIdx = AddPatch(sc->Name8, TEXTYPE_Pic, true);

        sc->ExpectName8();
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
//  P_InitAnimated
//
//  Load the table of animation definitions, checking for existence of
// the start and end of each frame. If the start doesn't exist the sequence
// is skipped, if the last doesn't exist, BOOM exits.
//
//  Wall/Flat animation sequences, defined by name of first and last frame,
// The full animation sequence is given using all lumps between the start
// and end entry, in the order found in the WAD file.
//
//  This routine modified to read its data from a predefined lump or
// PWAD lump called ANIMATED rather than a static table in this module to
// allow wad designers to insert or modify animation sequences.
//
//  Lump format is an array of byte packed animdef_t structures, terminated
// by a structure with istexture == -1. The lump can be generated from a
// text source file using SWANTBLS.EXE, distributed with the BOOM utils.
// The standard list of switches and animations is contained in the example
// source text file DEFSWANI.DAT also in the BOOM util distribution.
//
//==========================================================================
void P_InitAnimated () {
  guard(P_InitAnimated);
  animDef_t ad;
  frameDef_t fd;

  if (W_CheckNumForName(NAME_animated) < 0) return;

  VStream *Strm = W_CreateLumpReaderName(NAME_animated);
  while (Strm->TotalSize()-Strm->Tell() >= 23) {
    int pic1, pic2;
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
    if (Type&1) {
      pic1 = GTextureManager.CheckNumForName(VName(TmpName2, VName::AddLower8), TEXTYPE_Wall, true, false);
      pic2 = GTextureManager.CheckNumForName(VName(TmpName1, VName::AddLower8), TEXTYPE_Wall, true, false);
    } else {
      pic1 = GTextureManager.CheckNumForName(VName(TmpName2, VName::AddLower8), TEXTYPE_Flat, true, false);
      pic2 = GTextureManager.CheckNumForName(VName(TmpName1, VName::AddLower8), TEXTYPE_Flat, true, false);
    }

    // different episode ?
    if (pic1 == -1 || pic2 == -1) continue;

    memset(&ad, 0, sizeof(ad));
    memset(&fd, 0, sizeof(fd));

    ad.StartFrameDef = FrameDefs.Num();
    ad.Type = ANIM_Forward;

    // [RH] Allow for either forward or backward animations
    if (pic1 < pic2) {
      ad.Index = pic1;
      fd.Index = pic2;
    } else {
      ad.Index = pic2;
      fd.Index = pic1;
      ad.Type = ANIM_Backward;
    }

    if (fd.Index-ad.Index < 1) Sys_Error("P_InitPicAnims: bad cycle from '%s' to '%s' (ofs:0x%08x)", TmpName2, TmpName1, (vuint32)(Strm->Tell()-4-9*2-1));

    fd.BaseTime = BaseTime;
    fd.RandomRange = 0;
    FrameDefs.Append(fd);

    ad.NumFrames = FrameDefs[ad.StartFrameDef].Index-ad.Index+1;
    ad.CurrentFrame = ad.NumFrames-1;
    ad.Time = 0.01; // Force 1st game tic to animate
    ad.allowDecals = (Type == 3);
    AnimDefs.Append(ad);
  }
  delete Strm;
  Strm = nullptr;
  unguard;
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
  animDef_t   ad;
  frameDef_t  fd;

  memset(&ad, 0, sizeof(ad));

  // optional flag
  bool optional = false;
  if (sc->Check("optional")) optional = true;

  // name
  bool ignore = false;
  sc->ExpectName8();
  ad.Index = GTextureManager.CheckNumForNameAndForce(sc->Name8, (IsFlat ? TEXTYPE_Flat : TEXTYPE_Wall), true, true, !optional);
  if (ad.Index == -1) {
    ignore = true;
    if (!optional) GCon->Logf(NAME_Warning, "ANIMDEFS: Can't find texture \"%s\"", *sc->Name8);
  }
  VName adefname = sc->Name8;
  bool missing = ignore && optional;

  int CurType = 0;
  ad.StartFrameDef = FrameDefs.Num();
  ad.Type = ANIM_Normal;
  ad.allowDecals = 0;
  while (1) {
    if (sc->Check("allowdecals")) {
      ad.allowDecals = 1;
      continue;
    }

    if (sc->Check("random")) {
      ad.Type = ANIM_Random;
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
    } else {
      break;
    }

    memset(&fd, 0, sizeof(fd));
    if (sc->CheckNumber()) {
      fd.Index = ad.Index+sc->Number-1;
    } else {
      sc->ExpectName8();
      fd.Index = GTextureManager.CheckNumForNameAndForce(sc->Name8, (IsFlat ? TEXTYPE_Flat : TEXTYPE_Wall), true, true, false);
      if (fd.Index == -1 && !missing) sc->Message(va("Unknown texture \"%s\"", *sc->String));
    }

    if (sc->Check("tics")) {
      sc->ExpectNumber(true);
      fd.BaseTime = sc->Number;
      fd.RandomRange = 0;
    } else if (sc->Check("rand")) {
      sc->ExpectNumber(true);
      fd.BaseTime = sc->Number;
      sc->ExpectNumber(true);
      fd.RandomRange = sc->Number-fd.BaseTime+1;
    } else {
      sc->Error(va("bad command (%s)", *sc->String));
    }

    if (ad.Type != ANIM_Normal && ad.Type != ANIM_Random) {
      if (fd.Index < ad.Index) {
        int tmp = ad.Index;
        ad.Index = fd.Index;
        fd.Index = tmp;
        ad.Type = ANIM_Backward;
      }
      if (sc->Check("oscillate")) ad.Type = ANIM_OscillateUp;
    }
    if (!ignore) FrameDefs.Append(fd);
  }

  if (!ignore && ad.Type == ANIM_Normal && FrameDefs.Num()-ad.StartFrameDef < 2) {
    sc->Error(va("AnimDef '%s' has framecount < 2", *adefname));
  }

  if (!ignore) {
    if (ad.Type == ANIM_Normal) {
      ad.NumFrames = FrameDefs.Num()-ad.StartFrameDef;
      ad.CurrentFrame = ad.NumFrames-1;
    } else {
      ad.NumFrames = FrameDefs[ad.StartFrameDef].Index-ad.Index+1;
      if (ad.Type != ANIM_Random) ad.CurrentFrame = 0; else ad.CurrentFrame = (int)(Random()*ad.NumFrames);
    }
    ad.Time = 0.01; // Force 1st game tic to animate
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
  for (int i = 0; i < Switches.Num(); ++i) {
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
  TArray<TSwitchFrame>  Frames;
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
      sc->ExpectName8();
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

  if (!Frames.Num()) sc->Error("Switch state needs at least one frame");
  if (Bad) return nullptr;

  TSwitch *Def = new TSwitch();
  Def->Sound = Sound;
  Def->NumFrames = Frames.Num();
  Def->Frames = new TSwitchFrame[Frames.Num()];
  for (int i = 0; i < Frames.Num(); ++i) {
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
       if (sc->Check("doom")) { sc->ExpectNumber(); }
  else if (sc->Check("heretic")) {}
  else if (sc->Check("hexen")) {}
  else if (sc->Check("strife")) {}
  else if (sc->Check("any")) {}

  // switch texture
  sc->ExpectName8();
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
  sc->ExpectName8();
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
        sc->ExpectName8();
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
    A.NumFrames = Frames.Num();
    A.Frames = new vint32[Frames.Num()];
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

  sc->ExpectName8();
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
  P_InitAnimated();

  FrameDefs.Condense();
  AnimDefs.Condense();
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
  for (int i = 0; i < AnimDoorDefs.Num(); ++i) {
    if (AnimDoorDefs[i].Texture == BaseTex) return &AnimDoorDefs[i];
  }
  return nullptr;
  unguard;
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
  for (int i = 0; i < AnimDefs.Num(); ++i) {
    animDef_t &ad = AnimDefs[i];
    ad.Time -= host_frametime;
    if (ad.Time > 0.0) continue;

    bool validAnimation = true;
    if (ad.NumFrames > 1) {
      switch (ad.Type) {
        case ANIM_Normal:
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

    //const frameDef_t &fd = FrameDefs[ad.StartFrameDef+(ad.Type == ANIM_Normal ? ad.CurrentFrame : 0)];
    //fprintf(stderr, "ANIM #%d: texture %d (%s); type=%d; curframe=%d; framenum=%d; fdefs=%d; stfdef=%d; cfr=%d\n", i, ad.Index, *GTextureManager[ad.Index]->Name, (int)ad.Type, ad.CurrentFrame, ad.NumFrames, FrameDefs.length(), ad.StartFrameDef, ad.StartFrameDef+ad.CurrentFrame);
    const frameDef_t &fd = FrameDefs[ad.StartFrameDef+(validAnimation ? (ad.Type == ANIM_Normal ? ad.CurrentFrame : 0) : 0)];
    ad.Time = fd.BaseTime/35.0;
    if (fd.RandomRange) ad.Time += Random()*(fd.RandomRange/35.0); // random tics

    static int wantMissingAnimWarning = -1;

    VTexture *atx = GTextureManager[ad.Index];
    if (atx) {
      atx->noDecals = (ad.allowDecals == 0);
      atx->animNoDecals = (ad.allowDecals == 0);
      atx->animated = true;
    }

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
  InitFTAnims(); // Init flat and texture animations
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
  for (int i = 0; i < Switches.Num(); ++i) {
    delete Switches[i];
    Switches[i] = nullptr;
  }
  Switches.Clear();
  AnimDefs.Clear();
  FrameDefs.Clear();
  for (int i = 0; i < AnimDoorDefs.Num(); ++i) {
    delete[] AnimDoorDefs[i].Frames;
    AnimDoorDefs[i].Frames = nullptr;
  }
  AnimDoorDefs.Clear();

  // shut down texture manager
  GTextureManager.Shutdown();
  unguard;
}
