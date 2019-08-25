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
//**  Preparation of data for rendering, generation of lookups.
//**
//**************************************************************************
#include "gamedefs.h"
#include "r_local.h"


extern VCvarB dbg_show_missing_classes;
static VCvarI r_color_distance_algo("r_color_distance_algo", "1", "What algorithm use to calculate color distance?\n  0: standard\n  1: advanced.", CVAR_Archive);
// there is no sense to store this in config, because config is loaded after brightmaps
static VCvarB x_brightmaps_ignore_iwad("x_brightmaps_ignore_iwad", false, "Ignore \"iwad\" option when *loading* brightmaps?", CVAR_PreInit);


struct VTempSpriteEffectDef {
  VStr Sprite;
  VStr Light;
  VStr Part;
};

struct VTempClassEffects {
  VStr ClassName;
  VStr StaticLight;
  TArray<VTempSpriteEffectDef>  SpriteEffects;
};


// main palette
//
rgba_t r_palette[256];
vuint8 r_black_color;
vuint8 r_white_color;

vuint8 r_rgbtable[VAVOOM_COLOR_COMPONENT_MAX*VAVOOM_COLOR_COMPONENT_MAX*VAVOOM_COLOR_COMPONENT_MAX+4];

// variables used to look up
// and range check thing_t sprites patches
//spritedef_t sprites[MAX_SPRITE_MODELS];
TArray<spritedef_t> sprites;

VTextureTranslation **TranslationTables;
int NumTranslationTables;
VTextureTranslation IceTranslation;
TArray<VTextureTranslation *> DecorateTranslations;
TArray<VTextureTranslation *> BloodTranslations;

// they basicly work the same as translations
VTextureTranslation ColorMaps[CM_Max];

// temporary variables for sprite installing
enum { MAX_SPR_TEMP = 30 };
static spriteframe_t sprtemp[MAX_SPR_TEMP];
static int maxframe;
static const char *spritename;

static TArray<VLightEffectDef> GLightEffectDefs;
static TArray<VParticleEffectDef> GParticleEffectDefs;

static VCvarB spr_report_missing_rotations("spr_report_missing_rotations", false, "Report missing sprite rotations?");
static VCvarB spr_report_missing_patches("spr_report_missing_patches", false, "Report missing sprite patches?");


//==========================================================================
//
//  SetClassFieldInt
//
//==========================================================================
static void SetClassFieldInt (VClass *Class, const char *FieldName, int Value, int Idx=0) {
  VField *F = Class->FindFieldChecked(FieldName);
  check(F->Type.Type == TYPE_Int);
  vint32 *Ptr = (vint32 *)(Class->Defaults+F->Ofs);
  Ptr[Idx] = Value;
}


//==========================================================================
//
//  SetClassFieldBool
//
//==========================================================================
static void SetClassFieldBool (VClass *Class, const char *FieldName, int Value) {
  VField *F = Class->FindFieldChecked(FieldName);
  check(F->Type.Type == TYPE_Bool);
  vuint32 *Ptr = (vuint32 *)(Class->Defaults+F->Ofs);
  if (Value) *Ptr |= F->Type.BitMask; else *Ptr &= ~F->Type.BitMask;
}


//==========================================================================
//
//  SetClassFieldFloat
//
//==========================================================================
static void SetClassFieldFloat (VClass *Class, const char *FieldName, float Value) {
  VField *F = Class->FindFieldChecked(FieldName);
  check(F->Type.Type == TYPE_Float);
  float *Ptr = (float*)(Class->Defaults+F->Ofs);
  *Ptr = Value;
}


//==========================================================================
//
//  SetClassFieldVec
//
//==========================================================================
static void SetClassFieldVec (VClass *Class, const char *FieldName, const TVec &Value) {
  VField *F = Class->FindFieldChecked(FieldName);
  check(F->Type.Type == TYPE_Vector);
  TVec *Ptr = (TVec*)(Class->Defaults+F->Ofs);
  *Ptr = Value;
}


//==========================================================================
//
//  InitPalette
//
//==========================================================================
static void InitPalette () {
  if (W_CheckNumForName(NAME_playpal) == -1) {
    Sys_Error("Palette lump not found. Did you forgot to specify path to IWAD file?");
  }
  // We use color 0 as transparent color, so we must find an alternate
  // index for black color. In Doom, Heretic and Strife there is another
  // black color, in Hexen it's almost black.
  // I think that originaly Doom uses color 255 as transparent color,
  // but utilites created by others uses the alternate black color and
  // these graphics can contain pixels of color 255.
  // Heretic and Hexen also uses color 255 as transparent, even more - in
  // colormaps it's maped to color 0. Posibly this can cause problems
  // with modified graphics.
  // Strife uses color 0 as transparent. I already had problems with fact
  // that color 255 is normal color, now there shouldn't be any problems.
  VStream *lumpstream = W_CreateLumpReaderName(NAME_playpal);
  VCheckedStream Strm(lumpstream);
  rgba_t *pal = r_palette;
  int best_dist_black = 0x7fffffff;
  int best_dist_white = (r_color_distance_algo ? 0x7fffffff : -0x7fffffff);
  for (int i = 0; i < 256; ++i) {
    Strm << pal[i].r << pal[i].g << pal[i].b;
    if (i == 0) {
      //k8: force color 0 to transparent black (it doesn't matter, but anyway)
      //GCon->Logf("color #0 is (%02x_%02x_%02x)", pal[0].r, pal[0].g, pal[0].b);
      pal[i].r = 0;
      pal[i].g = 0;
      pal[i].b = 0;
      pal[i].a = 0;
    } else {
      pal[i].a = 255;
      // black
      int dist;
      if (r_color_distance_algo) {
        dist = rgbDistanceSquared(pal[i].r, pal[i].g, pal[i].b, 0, 0, 0);
      } else {
        dist = pal[i].r*pal[i].r+pal[i].g*pal[i].g+pal[i].b*pal[i].b;
      }
      if (dist < best_dist_black) {
        r_black_color = i;
        best_dist_black = dist;
      }
      // white
      if (r_color_distance_algo) {
        dist = rgbDistanceSquared(pal[i].r, pal[i].g, pal[i].b, 255, 255, 255);
        if (dist < best_dist_white) {
          r_white_color = i;
          best_dist_white = dist;
        }
      } else {
        //dist = pal[i].r*pal[i].r+pal[i].g*pal[i].g+pal[i].b*pal[i].b;
        if (dist > best_dist_white) {
          r_white_color = i;
          best_dist_white = dist;
        }
      }
    }
  }
  //GCon->Logf("black=%d:(%02x_%02x_%02x); while=%d:(%02x_%02x_%02x)", r_black_color, pal[r_black_color].r, pal[r_black_color].g, pal[r_black_color].b,
  //  r_white_color, pal[r_white_color].r, pal[r_white_color].g, pal[r_white_color].b);
}


//==========================================================================
//
//  InitRgbTable
//
//==========================================================================
static void InitRgbTable () {
  VStr rtblsize = VStr::size2human((unsigned)sizeof(r_rgbtable));
  if (developer) GCon->Logf(NAME_Dev, "building color translation table (%d bits, %d items per color, %s)...", VAVOOM_COLOR_COMPONENT_BITS, VAVOOM_COLOR_COMPONENT_MAX, *rtblsize);
  memset(r_rgbtable, 0, sizeof(r_rgbtable));
  for (int ir = 0; ir < VAVOOM_COLOR_COMPONENT_MAX; ++ir) {
    for (int ig = 0; ig < VAVOOM_COLOR_COMPONENT_MAX; ++ig) {
      for (int ib = 0; ib < VAVOOM_COLOR_COMPONENT_MAX; ++ib) {
        const int r = (int)(ir*255.0f/((float)(VAVOOM_COLOR_COMPONENT_MAX-1))/*+0.5f*/);
        const int g = (int)(ig*255.0f/((float)(VAVOOM_COLOR_COMPONENT_MAX-1))/*+0.5f*/);
        const int b = (int)(ib*255.0f/((float)(VAVOOM_COLOR_COMPONENT_MAX-1))/*+0.5f*/);
        int best_color = -1;
        int best_dist = 0x7fffffff;
        for (int i = 1; i < 256; ++i) {
          vint32 dist;
          if (r_color_distance_algo) {
            dist = rgbDistanceSquared(r_palette[i].r, r_palette[i].g, r_palette[i].b, r, g, b);
          } else {
            dist = (r_palette[i].r-r)*(r_palette[i].r-r)+
                   (r_palette[i].g-g)*(r_palette[i].g-g)+
                   (r_palette[i].b-b)*(r_palette[i].b-b);
          }
          if (best_color < 0 || dist < best_dist) {
            best_color = i;
            best_dist = dist;
            if (!dist) break;
          }
        }
        check(best_color > 0 && best_color <= 255);
        r_rgbtable[ir*VAVOOM_COLOR_COMPONENT_MAX*VAVOOM_COLOR_COMPONENT_MAX+ig*VAVOOM_COLOR_COMPONENT_MAX+ib] = best_color;
      }
    }
  }
}


//==========================================================================
//
//  InitTranslationTables
//
//==========================================================================
static void InitTranslationTables () {
  GCon->Log(NAME_Init, "building texture translations tables...");
  {
    VStream *lumpstream = W_CreateLumpReaderName(NAME_translat);
    VCheckedStream Strm(lumpstream);
    NumTranslationTables = Strm.TotalSize()/256;
    TranslationTables = new VTextureTranslation*[NumTranslationTables];
    for (int j = 0; j < NumTranslationTables; ++j) {
      VTextureTranslation *Trans = new VTextureTranslation;
      TranslationTables[j] = Trans;
      Strm.Serialise(Trans->Table, 256);
      // make sure that 0 always maps to 0
      Trans->Table[0] = 0;
      Trans->Palette[0] = r_palette[0];
      for (int i = 1; i < 256; ++i) {
        // make sure that normal colors doesn't map to color 0
        if (Trans->Table[i] == 0) Trans->Table[i] = r_black_color;
        Trans->Palette[i] = r_palette[Trans->Table[i]];
      }
    }
  }

  // calculate ice translation
  IceTranslation.Table[0] = 0;
  IceTranslation.Palette[0] = r_palette[0];
  for (int i = 1; i < 256; ++i) {
    int r = int(r_palette[i].r*0.5f+64*0.5f);
    int g = int(r_palette[i].g*0.5f+64*0.5f);
    int b = int(r_palette[i].b*0.5f+255*0.5f);
    IceTranslation.Palette[i].r = r;
    IceTranslation.Palette[i].g = g;
    IceTranslation.Palette[i].b = b;
    IceTranslation.Palette[i].a = 255;
    IceTranslation.Table[i] = R_LookupRGB(r, g, b);
  }
}


//==========================================================================
//
//  InitColorMaps
//
//==========================================================================
static void InitColorMaps () {
  GCon->Log(NAME_Init, "building colormaps...");

  // calculate inverse colormap
  VTextureTranslation *T = &ColorMaps[CM_Inverse];
  T->Table[0] = 0;
  T->Palette[0] = r_palette[0];
  for (int i = 1; i < 256; ++i) {
    int Gray = (r_palette[i].r*77+r_palette[i].g*143+r_palette[i].b*37)>>8;
    int Val = 255-Gray;
    T->Palette[i].r = Val;
    T->Palette[i].g = Val;
    T->Palette[i].b = Val;
    T->Palette[i].a = 255;
    T->Table[i] = R_LookupRGB(Val, Val, Val);
  }

  // calculate gold colormap
  T = &ColorMaps[CM_Gold];
  T->Table[0] = 0;
  T->Palette[0] = r_palette[0];
  for (int i = 1; i < 256; ++i) {
    int Gray = (r_palette[i].r*77+r_palette[i].g*143+r_palette[i].b*37)>>8;
    T->Palette[i].r = min2(255, Gray+Gray/2);
    T->Palette[i].g = Gray;
    T->Palette[i].b = 0;
    T->Palette[i].a = 255;
    T->Table[i] = R_LookupRGB(T->Palette[i].r, T->Palette[i].g, T->Palette[i].b);
  }

  // calculate red colormap
  T = &ColorMaps[CM_Red];
  T->Table[0] = 0;
  T->Palette[0] = r_palette[0];
  for (int i = 1; i < 256; ++i) {
    int Gray = (r_palette[i].r*77+r_palette[i].g*143+r_palette[i].b*37)>>8;
    T->Palette[i].r = min2(255, Gray+Gray/2);
    T->Palette[i].g = 0;
    T->Palette[i].b = 0;
    T->Palette[i].a = 255;
    T->Table[i] = R_LookupRGB(T->Palette[i].r, T->Palette[i].g, T->Palette[i].b);
  }

  // calculate green colormap
  T = &ColorMaps[CM_Green];
  T->Table[0] = 0;
  T->Palette[0] = r_palette[0];
  for (int i = 1; i < 256; ++i) {
    int Gray = (r_palette[i].r*77+r_palette[i].g*143+r_palette[i].b*37)>>8;
    T->Palette[i].r = min2(255, Gray+Gray/2);
    T->Palette[i].g = min2(255, Gray+Gray/2);
    T->Palette[i].b = Gray;
    T->Palette[i].a = 255;
    T->Table[i] = R_LookupRGB(T->Palette[i].r, T->Palette[i].g, T->Palette[i].b);
  }
}


//==========================================================================
//
//  InstallSpriteLump
//
//  local function for R_InitSprites
//
//==========================================================================
static void InstallSpriteLump (int lumpnr, int frame, char Rot, bool flipped) {
  int rotation;

  //GCon->Logf(NAME_Init, "!!INSTALL_SPRITE_LUMP: <%s> (lumpnr=%d; frame=%d; Rot=%c; flipped=%d)", *GTextureManager[lumpnr]->Name, lumpnr, frame, Rot, (flipped ? 1 : 0));

       if (Rot >= '0' && Rot <= '9') rotation = Rot-'0';
  else if (Rot >= 'a') rotation = Rot-'a'+10;
  else if (Rot >= 'A') rotation = Rot-'A'+10;
  else rotation = 17;

  VTexture *Tex = GTextureManager[lumpnr];
  if ((vuint32)frame >= 30 || (vuint32)rotation > 16) {
    //Sys_Error("InstallSpriteLump: Bad frame characters in lump '%s'", *Tex->Name);
    GCon->Logf(NAME_Error, "InstallSpriteLump: Bad frame characters in lump '%s'", *Tex->Name);
    if ((vuint32)frame < MAX_SPR_TEMP) {
      for (int r = 0; r < 16; ++r) {
        sprtemp[frame].lump[r] = -1;
        sprtemp[frame].flip[r] = false;
      }
    }
    return;
  }

  if (frame > maxframe) maxframe = frame;

  if (rotation == 0) {
    // the lump should be used for all rotations
    sprtemp[frame].rotate = 0;
    for (int r = 0; r < 16; ++r) {
      sprtemp[frame].lump[r] = lumpnr;
      sprtemp[frame].flip[r] = flipped;
    }
    return;
  }

       if (rotation <= 8) rotation = (rotation-1)*2;
  else rotation = (rotation-9)*2+1;

  // the lump is only used for one rotation
  if (sprtemp[frame].rotate == 0) {
    for (int r = 0; r < 16; r++) {
      sprtemp[frame].lump[r] = -1;
      sprtemp[frame].flip[r] = false;
    }
  }

  sprtemp[frame].rotate = 1;
  sprtemp[frame].lump[rotation] = lumpnr;
  sprtemp[frame].flip[rotation] = flipped;
}


struct SpriteTexInfo {
  int texid;
  int next; // next id of texture with this 4-letter prefix, or 0
};

static TArray<SpriteTexInfo> spriteTextures;
static TMap<vuint32, int> spriteTexMap; // key: 4 bytes of a name; value: index in spriteTextures
static int spriteTexturesLengthCheck = -1;


//==========================================================================
//
//  sprprefix2u32
//
//==========================================================================
static vuint32 sprprefix2u32 (const char *name) {
  if (!name || !name[0] || !name[1] || !name[2] || !name[3]) return 0;
  vuint32 res = 0;
  for (int f = 0; f < 4; ++f, ++name) {
    vuint8 ch = (vuint8)name[0];
    if (ch >= 'A' && ch <= 'Z') ch = ch-'A'+'a';
    res = (res<<8)|ch;
  }
  return res;
}


//==========================================================================
//
//  BuildSpriteTexturesList
//
//==========================================================================
static void BuildSpriteTexturesList () {
  if (spriteTexturesLengthCheck == GTextureManager.GetNumTextures()) return;
  spriteTexturesLengthCheck = GTextureManager.GetNumTextures();
  spriteTextures.reset();
  spriteTexMap.reset();

  // scan the lumps, filling in the frames for whatever is found
  for (int l = 0; l < GTextureManager.GetNumTextures(); ++l) {
    if (GTextureManager[l]->Type == TEXTYPE_Sprite) {
      const char *lumpname = *GTextureManager[l]->Name;
      if (!lumpname[0] || !lumpname[1] || !lumpname[2] || !lumpname[3] || !lumpname[4] || !lumpname[5]) continue;
      vuint32 pfx = sprprefix2u32(lumpname);
      if (!pfx) continue;
      // create new record
      int cidx = spriteTextures.length();
      SpriteTexInfo &sti = spriteTextures.alloc();
      sti.texid = l;
      sti.next = 0;
      // append to list
      auto pip = spriteTexMap.find(pfx);
      if (pip) {
        // append to the list
        int last = *pip;
        check(last >= 0 && last < spriteTextures.length()-1);
        while (spriteTextures[last].next) last = spriteTextures[last].next;
        check(last >= 0 && last < spriteTextures.length()-1);
        check(spriteTextures[last].next == 0);
        spriteTextures[last].next = cidx;
      } else {
        // list head
        spriteTexMap.put(pfx, cidx);
      }
    }
  }
}


//==========================================================================
//
//  R_InstallSpriteComplete
//
//==========================================================================
void R_InstallSpriteComplete () {
  spriteTexturesLengthCheck = -1;
  spriteTextures.clear();
  spriteTexMap.clear();
}


//==========================================================================
//
//  R_InstallSprite
//
//  Builds the sprite rotation matrixes to account for horizontally flipped
//  sprites. Will report an error if the lumps are inconsistant.
//
//  Sprite lump names are 4 characters for the actor, a letter for the frame,
//  and a number for the rotation. A sprite that is flippable will have an
//  additional letter/number appended. The rotation character can be 0 to
//  signify no rotations.
//
//==========================================================================
void R_InstallSprite (const char *name, int index) {
  if (index < 0) Host_Error("Invalid sprite index %d for sprite %s", index, name);
  //GCon->Logf("!!INSTALL_SPRITE: <%s> (%d)", name, index);
  spritename = name;
#if 1
  memset(sprtemp, -1, sizeof(sprtemp));
#else
  for (unsigned idx = 0; idx < MAX_SPR_TEMP; ++idx) {
    sprtemp[idx].rotate = -1;
    for (unsigned c = 0; c < 16; ++c) {
      sprtemp[idx].lump[c] = -1;
      sprtemp[idx].flip[c] = false;
    }
  }
#endif
  maxframe = -1;

  while (index >= sprites.length()) {
    spritedef_t &ss = sprites.alloc();
    ss.numframes = 0;
    ss.spriteframes = nullptr;
  }
  check(index < sprites.length());
  sprites[index].numframes = 0;
  if (sprites[index].spriteframes) Z_Free(sprites[index].spriteframes);
  sprites[index].spriteframes = nullptr;

  // scan all the lump names for each of the names, noting the highest frame letter
  // just compare 4 characters as ints
  //int intname = *(int*)*VName(spritename, VName::AddLower8);
  const char *intname = *VName(spritename, VName::AddLower8);
  if (!intname[0] || !intname[1] || !intname[2] || !intname[3]) {
    GCon->Logf(NAME_Warning, "trying to install sprite with invalid name '%s'", intname);
    return;
  }

  // scan the lumps, filling in the frames for whatever is found
  /*
  for (int l = 0; l < GTextureManager.GetNumTextures(); ++l) {
    if (GTextureManager[l]->Type == TEXTYPE_Sprite) {
      const char *lumpname = *GTextureManager[l]->Name;
      if (!lumpname[0] || !lumpname[1] || !lumpname[2] || !lumpname[3] || !lumpname[4] || !lumpname[5]) continue;
      if (memcmp(lumpname, intname, 4) != 0) continue;
      //GCon->Logf("  !!<%s> [4]=%c; [6]=%c; [7]=%c", lumpname, lumpname[4], lumpname[6], (lumpname[6] ? lumpname[7] : 0));
      InstallSpriteLump(l, VStr::ToUpper(lumpname[4])-'A', lumpname[5], false);
      if (lumpname && strlen(lumpname) >= 6 && lumpname[6]) {
        InstallSpriteLump(l, VStr::ToUpper(lumpname[6])-'A', lumpname[7], true);
      }
    }
  }
  */
  vuint32 intpfx = sprprefix2u32(intname);
  if (!intpfx) {
    GCon->Logf(NAME_Warning, "trying to install sprite with invalid name '%s'!", intname);
    return;
  }

  BuildSpriteTexturesList();
  {
    auto pip = spriteTexMap.find(intpfx);
    if (pip) {
      int slidx = *pip;
      do {
        int l = spriteTextures[slidx].texid;
        slidx = spriteTextures[slidx].next;
        check(GTextureManager[l]->Type == TEXTYPE_Sprite);
        const char *lumpname = *GTextureManager[l]->Name;
        if (lumpname[0] && lumpname[1] && lumpname[2] && lumpname[3] && lumpname[4] && lumpname[5]) {
          if (memcmp(lumpname, intname, 4) == 0) {
            InstallSpriteLump(l, VStr::ToUpper(lumpname[4])-'A', lumpname[5], false);
            if (lumpname && strlen(lumpname) >= 6 && lumpname[6]) {
              InstallSpriteLump(l, VStr::ToUpper(lumpname[6])-'A', lumpname[7], true);
            }
          }
        }
      } while (slidx != 0);
    }
  }

  // check the frames that were found for completeness
  if (maxframe == -1) return;

  ++maxframe;

  //GCon->Logf(NAME_Init, "sprite '%s', maxframe=%d", intname, maxframe);

  for (int frame = 0; frame < maxframe; ++frame) {
    //fprintf(stderr, "  frame=%d; rot=%d (%u)\n", frame, (int)sprtemp[frame].rotate, *((unsigned char *)&sprtemp[frame].rotate));
    switch ((int)sprtemp[frame].rotate) {
      case -1:
        // no rotations were found for that frame at all
        if (GArgs.CheckParm("-sprstrict")) {
          Sys_Error("R_InstallSprite: No patches found for '%s' frame '%c'", spritename, frame+'A');
        } else {
          if (spr_report_missing_patches) {
            GCon->Logf(NAME_Error, "R_InstallSprite: No patches found for '%s' frame '%c'", spritename, frame+'A');
          }
        }
        break;

      case 0:
        // only the first rotation is needed
        break;

      case 1:
        // copy missing frames for 16-angle rotation
        for (int rotation = 0; rotation < 8; ++rotation) {
          if (sprtemp[frame].lump[rotation*2+1] == -1) {
            sprtemp[frame].lump[rotation*2+1] = sprtemp[frame].lump[rotation*2];
            sprtemp[frame].flip[rotation*2+1] = sprtemp[frame].flip[rotation*2];
          }
          if (sprtemp[frame].lump[rotation*2] == -1) {
            sprtemp[frame].lump[rotation*2] = sprtemp[frame].lump[rotation*2+1];
            sprtemp[frame].flip[rotation*2] = sprtemp[frame].flip[rotation*2+1];
          }
        }
        // must have all 8 frames
        for (int rotation = 0; rotation < 8; ++rotation) {
          if (sprtemp[frame].lump[rotation] == -1) {
            if (GArgs.CheckParm("-sprstrict")) {
              Sys_Error("R_InstallSprite: Sprite '%s' frame '%c' is missing rotations", spritename, frame+'A');
            } else {
              if (spr_report_missing_rotations) {
                GCon->Logf(NAME_Error, "R_InstallSprite: Sprite '%s' frame '%c' is missing rotations", spritename, frame+'A');
              }
            }
          }
        }
        break;
    }
  }

  // allocate space for the frames present and copy sprtemp to it
  sprites[index].numframes = maxframe;
  sprites[index].spriteframes = (spriteframe_t*)Z_Malloc(maxframe*sizeof(spriteframe_t));
  memcpy(sprites[index].spriteframes, sprtemp, maxframe*sizeof(spriteframe_t));
}


//==========================================================================
//
//  FreeSpriteData
//
//==========================================================================
static void FreeSpriteData () {
  for (auto &&ss : sprites) if (ss.spriteframes) Z_Free(ss.spriteframes);
  sprites.clear();
}


//==========================================================================
//
//  R_AreSpritesPresent
//
//==========================================================================
bool R_AreSpritesPresent (int Index) {
  return (Index >= 0 && Index < sprites.length() && sprites.ptr()[Index].numframes > 0);
}


//==========================================================================
//
//  R_InitData
//
//==========================================================================
void R_InitData () {
  // load palette
  InitPalette();
  // calculate RGB table
  InitRgbTable();
  // init standard translation tables
  InitTranslationTables();
  // init color maps
  InitColorMaps();
}


//==========================================================================
//
//  R_ShutdownData
//
//==========================================================================
void R_ShutdownData () {
  if (TranslationTables) {
    for (int i = 0; i < NumTranslationTables; ++i) {
      delete TranslationTables[i];
      TranslationTables[i] = nullptr;
    }
    delete[] TranslationTables;
    TranslationTables = nullptr;
  }

  for (int i = 0; i < DecorateTranslations.Num(); ++i) {
    delete DecorateTranslations[i];
    DecorateTranslations[i] = nullptr;
  }
  DecorateTranslations.Clear();

  FreeSpriteData();

  GLightEffectDefs.Clear();
  GParticleEffectDefs.Clear();
}


//==========================================================================
//
//  R_ParseDecorateTranslation
//
//==========================================================================
int R_ParseDecorateTranslation (VScriptParser *sc, int GameMax) {
  // first check for standard translation
  if (sc->CheckNumber()) {
    if (sc->Number < 0 || sc->Number >= max2(NumTranslationTables, GameMax)) {
      //sc->Error(va("Translation must be in range [0, %d]", max2(NumTranslationTables, GameMax)-1));
      GCon->Logf(NAME_Warning, "%s: Translation must be in range [0, %d]", *sc->GetLoc().toStringNoCol(), max2(NumTranslationTables, GameMax)-1);
      sc->Number = 2; // red
    }
    return (TRANSL_Standard<<TRANSL_TYPE_SHIFT)+sc->Number;
  }

  // check for special ice translation
  if (sc->Check("Ice")) return (TRANSL_Standard<<TRANSL_TYPE_SHIFT)+7;

  VTextureTranslation *Tr = new VTextureTranslation;
  do {
    sc->ExpectString();
    Tr->AddTransString(sc->String);
  } while (sc->Check(","));

  // see if we already have this translation
  for (int i = 0; i < DecorateTranslations.Num(); ++i) {
    if (DecorateTranslations[i]->Crc != Tr->Crc) continue;
    if (memcmp(DecorateTranslations[i]->Palette, Tr->Palette, sizeof(Tr->Palette))) continue;
    // found a match
    delete Tr;
    Tr = nullptr;
    return (TRANSL_Decorate<<TRANSL_TYPE_SHIFT)+i;
  }

  // add it
  if (DecorateTranslations.Num() >= MAX_DECORATE_TRANSLATIONS) {
    sc->Error("Too many translations in DECORATE scripts");
  }
  DecorateTranslations.Append(Tr);
  return (TRANSL_Decorate<<TRANSL_TYPE_SHIFT)+DecorateTranslations.Num()-1;
}


//==========================================================================
//
//  R_GetBloodTranslation
//
//==========================================================================
int R_GetBloodTranslation (int Col) {
  // check for duplicate blood translation
  for (int i = 0; i < BloodTranslations.Num(); ++i) {
    if (BloodTranslations[i]->Color == Col) {
      return (TRANSL_Blood<<TRANSL_TYPE_SHIFT)+i;
    }
  }

  // create new translation
  VTextureTranslation *Tr = new VTextureTranslation;
  Tr->BuildBloodTrans(Col);

  // add it
  if (BloodTranslations.Num() >= MAX_BLOOD_TRANSLATIONS) {
    Sys_Error("Too many blood colors in DECORATE scripts");
  }
  BloodTranslations.Append(Tr);
  return (TRANSL_Blood<<TRANSL_TYPE_SHIFT)+BloodTranslations.Num()-1;
}


//==========================================================================
//
//  R_FindLightEffect
//
//==========================================================================
VLightEffectDef *R_FindLightEffect (VStr Name) {
  for (int i = 0; i < GLightEffectDefs.Num(); ++i) {
    if (Name.ICmp(*GLightEffectDefs[i].Name) == 0) return &GLightEffectDefs[i];
  }
  return nullptr;
}


//==========================================================================
//
//  ParseLightDef
//
//==========================================================================
static void ParseLightDef (VScriptParser *sc, int LightType) {
  // get name, find it in the list or add it if it's not there yet
  sc->ExpectString();
  VLightEffectDef *L = R_FindLightEffect(sc->String);
  if (!L) L = &GLightEffectDefs.Alloc();

  // set default values
  L->Name = *sc->String.ToLower();
  L->Type = LightType;
  L->Color = 0xffffffff;
  L->Radius = 0.0f;
  L->Radius2 = 0.0f;
  L->MinLight = 0.0f;
  L->Offset = TVec(0, 0, 0);
  L->Chance = 0.0f;
  L->Interval = 0.0f;
  L->Scale = 0.0f;
  L->NoSelfShadow = 0;

  // parse light def
  sc->Expect("{");
  while (!sc->Check("}")) {
    if (sc->Check("colour") || sc->Check("color")) {
      sc->ExpectFloat();
      float r = midval(0.0f, (float)sc->Float, 1.0f);
      sc->ExpectFloat();
      float g = midval(0.0f, (float)sc->Float, 1.0f);
      sc->ExpectFloat();
      float b = midval(0.0f, (float)sc->Float, 1.0f);
      L->Color = ((int)(r*255)<<16)|((int)(g*255)<<8)|(int)(b*255)|0xff000000;
    } else if (sc->Check("radius")) {
      sc->ExpectFloat();
      L->Radius = sc->Float;
    } else if (sc->Check("radius2")) {
      sc->ExpectFloat();
      L->Radius2 = sc->Float;
    } else if (sc->Check("minlight")) {
      sc->ExpectFloat();
      L->MinLight = sc->Float;
    } else if (sc->Check("noselfshadow")) {
      L->NoSelfShadow = 1;
    } else if (sc->Check("offset")) {
      sc->ExpectFloat();
      L->Offset.x = sc->Float;
      sc->ExpectFloat();
      L->Offset.y = sc->Float;
      sc->ExpectFloat();
      L->Offset.z = sc->Float;
    } else {
      sc->Error(va("Bad point light parameter (%s)", *sc->String));
    }
  }
}


//==========================================================================
//
//  GZSizeToRadius
//
//==========================================================================
static inline float GZSizeToRadius (float Val) {
  /*
  if (Val <= 20.0f) return Val*4.5f;
  if (Val <= 30.0f) return Val*3.6f;
  if (Val <= 40.0f) return Val*3.3f;
  if (Val <= 60.0f) return Val*2.8f;
  return Val*2.5f;
  */
  //k8: 1.04f is just because i feel
  return Val*1.04f; // size in map units
}


//==========================================================================
//
//  ParseGZLightDef
//
//==========================================================================
static void ParseGZLightDef (VScriptParser *sc, int LightType, float lightsizefactor) {
  // get name, find it in the list or add it if it's not there yet
  sc->ExpectString();
  VLightEffectDef *L = R_FindLightEffect(sc->String);
  if (!L) L = &GLightEffectDefs.Alloc();

  // set default values
  L->Name = *sc->String.ToLower();
  L->Type = LightType;
  L->Color = 0xffffffff;
  L->Radius = 0.0f;
  L->Radius2 = 0.0f;
  L->MinLight = 0.0f;
  L->Offset = TVec(0, 0, 0);
  L->Chance = 0.0f;
  L->Interval = 0.0f;
  L->Scale = 0.0f;
  L->NoSelfShadow = 0;

  bool attenuated = false;

  // parse light def
  sc->Expect("{");
  while (!sc->Check("}")) {
    if (sc->Check("color") || sc->Check("colour")) {
      sc->ExpectFloat();
      float r = midval(0.0f, (float)sc->Float, 1.0f);
      sc->ExpectFloat();
      float g = midval(0.0f, (float)sc->Float, 1.0f);
      sc->ExpectFloat();
      float b = midval(0.0f, (float)sc->Float, 1.0f);
      L->Color = ((int)(r*255)<<16)|((int)(g*255)<<8)|(int)(b*255)|0xff000000;
    } else if (sc->Check("size")) {
      sc->ExpectFloat();
      L->Radius = sc->Float;
    } else if (sc->Check("secondarySize")) {
      sc->ExpectFloat();
      L->Radius2 = sc->Float;
    } else if (sc->Check("offset")) {
      // GZDoom manages Z offset as Y offset
      sc->ExpectFloat();
      L->Offset.x = sc->Float;
      sc->ExpectFloat();
      L->Offset.z = sc->Float;
      sc->ExpectFloat();
      L->Offset.y = sc->Float;
    } else if (sc->Check("subtractive")) {
      sc->ExpectNumber();
      sc->Message(va("Subtractive light ('%s') is not supported yet.", *L->Name));
    } else if (sc->Check("chance")) {
      sc->ExpectFloat();
      L->Chance = sc->Float;
    } else if (sc->Check("scale")) {
      sc->ExpectFloat();
      L->Scale = sc->Float;
    } else if (sc->Check("interval")) {
      sc->ExpectFloat();
      L->Interval = sc->Float;
    } else if (sc->Check("additive")) {
      sc->ExpectNumber();
      sc->Message(va("Additive light ('%s') parameter not supported yet.", *L->Name));
    } else if (sc->Check("halo")) {
      sc->ExpectNumber();
      sc->Message(va("Halo light ('%s') parameter not supported.", *L->Name));
    } else if (sc->Check("dontlightself")) {
      sc->ExpectNumber();
      sc->Message(va("DontLightSelf light ('%s') parameter not supported.", *L->Name));
    } else if (sc->Check("attenuate")) {
      sc->ExpectNumber();
      if (sc->Number) {
        attenuated = true;
      } else {
        attenuated = false;
        sc->Message(va("Non-attenuated light ('%s') will be attenuated anyway.", *L->Name));
      }
    } else {
      sc->Error(va("Bad gz light ('%s') parameter (%s)", *L->Name, *sc->String));
    }
  }

  if (attenuated) {
    L->Radius *= lightsizefactor;
    L->Radius2 *= lightsizefactor;
  }

  L->Radius = GZSizeToRadius(L->Radius);
  L->Radius2 = GZSizeToRadius(L->Radius2);
}


//==========================================================================
//
//  FindParticleEffect
//
//==========================================================================
static VParticleEffectDef *FindParticleEffect (VStr Name) {
  for (int i = 0; i < GParticleEffectDefs.Num(); ++i) {
    if (Name.ICmp(*GParticleEffectDefs[i].Name) == 0) return &GParticleEffectDefs[i];
  }
  return nullptr;
}


//==========================================================================
//
//  ParseParticleEffect
//
//==========================================================================
static void ParseParticleEffect (VScriptParser *sc) {
  // get name, find it in the list or add it if it's not there yet
  sc->ExpectString();
  VParticleEffectDef *P = FindParticleEffect(sc->String);
  if (!P) P = &GParticleEffectDefs.Alloc();

  // set default values
  P->Name = *sc->String.ToLower();
  P->Type = 0;
  P->Type2 = 0;
  P->Color = 0xffffffff;
  P->Offset = TVec(0, 0, 0);
  P->Count = 0;
  P->OrgRnd = 0;
  P->Velocity = TVec(0, 0, 0);
  P->VelRnd = 0;
  P->Accel = 0;
  P->Grav = 0;
  P->Duration = 0;
  P->Ramp = 0;

  // parse light def
  sc->Expect("{");
  while (!sc->Check("}")) {
    if (sc->Check("type")) {
           if (sc->Check("static")) P->Type = 0;
      else if (sc->Check("explode")) P->Type = 1;
      else if (sc->Check("explode2")) P->Type = 2;
      else sc->Error("Bad type");
    } else if (sc->Check("type2")) {
           if (sc->Check("static")) P->Type2 = 0;
      else if (sc->Check("explode")) P->Type2 = 1;
      else if (sc->Check("explode2")) P->Type2 = 2;
      else sc->Error("Bad type");
    } else if (sc->Check("colour") || sc->Check("color")) {
      sc->ExpectFloat();
      float r = midval(0.0f, (float)sc->Float, 1.0f);
      sc->ExpectFloat();
      float g = midval(0.0f, (float)sc->Float, 1.0f);
      sc->ExpectFloat();
      float b = midval(0.0f, (float)sc->Float, 1.0f);
      P->Color = ((int)(r*255)<<16)|((int)(g*255)<<8)|(int)(b*255)|0xff000000;
    } else if (sc->Check("offset")) {
      sc->ExpectFloat();
      P->Offset.x = sc->Float;
      sc->ExpectFloat();
      P->Offset.y = sc->Float;
      sc->ExpectFloat();
      P->Offset.z = sc->Float;
    } else if (sc->Check("count")) {
      sc->ExpectNumber();
      P->Count = sc->Number;
    } else if (sc->Check("originrandom")) {
      sc->ExpectFloat();
      P->OrgRnd = sc->Float;
    } else if (sc->Check("velocity")) {
      sc->ExpectFloat();
      P->Velocity.x = sc->Float;
      sc->ExpectFloat();
      P->Velocity.y = sc->Float;
      sc->ExpectFloat();
      P->Velocity.z = sc->Float;
    } else if (sc->Check("velocityrandom")) {
      sc->ExpectFloat();
      P->VelRnd = sc->Float;
    } else if (sc->Check("acceleration")) {
      sc->ExpectFloat();
      P->Accel = sc->Float;
    } else if (sc->Check("gravity")) {
      sc->ExpectFloat();
      P->Grav = sc->Float;
    } else if (sc->Check("duration")) {
      sc->ExpectFloat();
      P->Duration = sc->Float;
    } else if (sc->Check("ramp")) {
      sc->ExpectFloat();
      P->Ramp = sc->Float;
    } else {
      sc->Error(va("Bad particle effect parameter (%s)", *sc->String));
    }
  }
}


//==========================================================================
//
//  ParseClassEffects
//
//==========================================================================
static void ParseClassEffects (VScriptParser *sc, TArray<VTempClassEffects> &ClassDefs) {
  // get name, find it in the list or add it if it's not there yet
  sc->ExpectString();
  VTempClassEffects *C = nullptr;
  for (int i = 0; i < ClassDefs.Num(); ++i) {
    if (ClassDefs[i].ClassName.ICmp(sc->String) == 0) {
      C = &ClassDefs[i];
      break;
    }
  }
  if (!C) C = &ClassDefs.Alloc();

  // set defaults
  C->ClassName = sc->String;
  C->StaticLight.Clean();
  C->SpriteEffects.Clear();

  // parse
  sc->Expect("{");
  while (!sc->Check("}")) {
    if (sc->Check("frame")) {
      sc->ExpectString();
      VTempSpriteEffectDef &S = C->SpriteEffects.Alloc();
      S.Sprite = sc->String;
      sc->Expect("{");
      while (!sc->Check("}")) {
        if (sc->Check("light")) {
          sc->ExpectString();
          S.Light = sc->String.ToLower();
        } else if (sc->Check("particles")) {
          sc->ExpectString();
          S.Part = sc->String.ToLower();
        } else {
          sc->Error("Bad frame parameter");
        }
      }
    } else if (sc->Check("static_light")) {
      sc->ExpectString();
      C->StaticLight = sc->String.ToLower();
    } else {
      sc->Error("Bad class parameter");
    }
  }
}


//==========================================================================
//
//  ParseEffectDefs
//
//==========================================================================
static void ParseEffectDefs (VScriptParser *sc, TArray<VTempClassEffects> &ClassDefs) {
  if (developer) GCon->Logf(NAME_Dev, "...parsing k8vavoom effect definitions from '%s'...", *sc->GetScriptName());
  while (!sc->AtEnd()) {
    if (sc->Check("#include")) {
      sc->ExpectString();
      int Lump = W_CheckNumForFileName(sc->String);
      // check WAD lump only if it's no longer than 8 characters and has no path separator
      if (Lump < 0 && sc->String.Length() <= 8 && sc->String.IndexOf('/') < 0) {
        Lump = W_CheckNumForName(VName(*sc->String, VName::AddLower8));
      }
      if (Lump < 0) sc->Error(va("Lump '%s' not found", *sc->String));
      ParseEffectDefs(new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)), ClassDefs);
      continue;
    }
    else if (sc->Check("pointlight")) ParseLightDef(sc, DLTYPE_Point);
    else if (sc->Check("muzzleflashlight")) ParseLightDef(sc, DLTYPE_MuzzleFlash);
    else if (sc->Check("particleeffect")) ParseParticleEffect(sc);
    else if (sc->Check("class")) ParseClassEffects(sc, ClassDefs);
    else if (sc->Check("clear")) ClassDefs.Clear();
    else sc->Error(va("Unknown command (%s)", *sc->String));
  }
  delete sc;
}


//==========================================================================
//
//  ParseBrightmap
//
//==========================================================================
static void ParseBrightmap (int SrcLump, VScriptParser *sc) {
  int ttype = TEXTYPE_Any;
       if (sc->Check("flat")) ttype = TEXTYPE_Flat;
  else if (sc->Check("sprite")) ttype = TEXTYPE_Sprite;
  else if (sc->Check("texture")) ttype = TEXTYPE_Wall;
  else sc->Error("unknown brightmap type");
  sc->ExpectName8Warn();
  VName img = sc->Name8;
  VStr bmap;
  bool iwad = false;
  bool thiswad = false;
  bool nofb = false;
  sc->Expect("{");
  while (!sc->Check("}")) {
    if (sc->Check("map")) {
      if (!sc->GetString()) sc->Error(va("brightmap image name expected for image '%s'", *img));
      if (sc->String.isEmpty()) sc->Error(va("empty brightmap image for image '%s'", *img));
      if (!bmap.isEmpty()) GCon->Logf(NAME_Warning, "duplicate brightmap image for image '%s'", *img);
      bmap = sc->String.fixSlashes();
    } else if (sc->Check("iwad")) {
      iwad = true;
    } else if (sc->Check("thiswad")) {
      thiswad = true;
    } else if (sc->Check("disablefullbright")) {
      nofb = true;
    } else {
      //sc->Error(va("Unknown command (%s)", *sc->String));
      sc->ExpectString();
      sc->Message(va("Unknown command (%s) for image '%s'", *sc->String, *img));
    }
  }
  // there is no need to load brightmap textures for server
#ifdef CLIENT
  if (img != NAME_None && !VTextureManager::IsDummyTextureName(img) && !bmap.isEmpty()) {
    static int doWarn = -1;
    if (doWarn < 0) doWarn = (GArgs.CheckParm("-Wall") || GArgs.CheckParm("-Wbrightmap") ? 1 : 0);

    static int doLoadDump = -1;
    if (doLoadDump < 0) doLoadDump = (GArgs.CheckParm("-dump-brightmaps") ? 1 : 0);

    VTexture *basetex = GTextureManager.GetExistingTextureByName(VStr(img), ttype);
    if (!basetex) {
      if (doWarn) GCon->Logf(NAME_Warning, "texture '%s' not found, cannot attach brightmap", *img);
      return;
    }

    if (x_brightmaps_ignore_iwad) iwad = false;

    if (iwad && !W_IsIWADLump(basetex->SourceLump)) {
      // oops
      if (doWarn) GCon->Logf(NAME_Warning, "IWAD SKIP! '%s' (%s[%d]; '%s')", *img, *W_FullLumpName(basetex->SourceLump), basetex->SourceLump, *basetex->Name);
      return;
    }
    //if (iwad && doWarn) GCon->Logf(NAME_Warning, "IWAD PASS! '%s'", *img);

    basetex->nofullbright = nofb;
    delete basetex->Brightmap; // it is safe to remove it, as each brightmap texture is unique
    basetex->Brightmap = nullptr;

    int lmp = W_CheckNumForFileName(bmap);
    if (lmp < 0) {
      static const EWadNamespace lookns[] = {
        WADNS_Graphics,
        WADNS_Sprites,
        WADNS_Flats,
        WADNS_NewTextures,
        WADNS_HiResTextures,
        WADNS_Patches,
        WADNS_Global,
        // end marker
        WADNS_ZipSpecial,
      };

      // this can be ordinary texture lump, try hard to find it
      if (bmap.indexOf('/') < 0 && bmap.indexOf('.') < 0) {
        for (unsigned nsidx = 0; lookns[nsidx] != WADNS_ZipSpecial; ++nsidx) {
          for (int Lump = W_IterateNS(-1, lookns[nsidx]); Lump >= 0; Lump = W_IterateNS(Lump, lookns[nsidx])) {
            if (W_LumpFile(Lump) > W_LumpFile(SrcLump)) break;
            if (Lump <= lmp) continue;
            if (bmap.ICmp(*W_LumpName(Lump)) == 0) lmp = Lump;
          }
        }
        //if (lmp >= 0) GCon->Logf(NAME_Warning, "std. brightmap texture '%s' is '%s'", *bmap, *W_FullLumpName(lmp));
      }
      if (lmp < 0) GCon->Logf(NAME_Warning, "brightmap texture '%s' not found", *bmap);
      return;
    }

    if (thiswad && W_LumpFile(lmp) != W_LumpFile(basetex->SourceLump)) return;

    VTexture *bm = VTexture::CreateTexture(TEXTYPE_Any, lmp, false); // don't set name
    if (!bm) {
      GCon->Logf(NAME_Warning, "cannot load brightmap texture '%s'", *bmap);
      return;
    }
    bm->nofullbright = nofb; // just in case
    bm->Name = VName(*(W_RealLumpName(lmp).ExtractFileBaseName().StripExtension()), VName::AddLower);

    if (bm->GetWidth() != basetex->GetWidth() || bm->GetHeight() != basetex->GetHeight()) {
      if (doWarn) {
        GCon->Logf(NAME_Warning, "texture '%s' has dimensions (%d,%d), but brightmap '%s' has dimensions (%d,%d)",
          *img, basetex->GetWidth(), basetex->GetHeight(), *bmap, bm->GetWidth(), bm->GetHeight());
      }
      bm->ResizeCanvas(basetex->GetWidth(), basetex->GetHeight());
      check(bm->GetWidth() == basetex->GetWidth());
      check(bm->GetHeight() == basetex->GetHeight());
    }

    basetex->Brightmap = bm;
    if (doLoadDump) GCon->Logf(NAME_Warning, "texture '%s' got brightmap '%s' (%p : %p) (lump %d:%s)", *basetex->Name, *bm->Name, basetex, bm, lmp, *W_FullLumpName(lmp));
  }
#else
  (void)img;
  (void)iwad;
  (void)thiswad;
  (void)nofb;
  (void)ttype;
#endif
}


//==========================================================================
//
//  ParseGlow
//
//==========================================================================
static void ParseGlow (VScriptParser *sc) {
  sc->Expect("{");
  while (!sc->Check("}")) {
    // not implemented gozzo feature (in gozzo too)
    if (sc->Check("Texture")) {
      sc->GetString();
      while (sc->Check(",")) sc->ExpectString();
      continue;
    }
    // texture list
    int ttype = -1;
         if (sc->Check("flats")) ttype = TEXTYPE_Flat;
    else if (sc->Check("walls")) ttype = TEXTYPE_Wall;
    if (ttype > 0) {
      sc->Expect("{");
      while (!sc->Check("}")) {
        if (sc->Check(",")) continue;
        sc->ExpectName8Warn();
#ifdef CLIENT
        VName img = sc->Name8;
        if (img != NAME_None && !VTextureManager::IsDummyTextureName(img)) {
          VTexture *basetex = GTextureManager.GetExistingTextureByName(VStr(img), ttype);
          if (basetex) {
            //GCon->Logf("GLOW: <%s>", *img);
            //basetex->glowing = true;
            rgb_t gclr = basetex->GetAverageColor(153);
            if (gclr.r || gclr.g || gclr.b) {
              basetex->glowing = 0xff000000u|(gclr.r<<16)|(gclr.g<<8)|gclr.b;
            } else {
              basetex->glowing = 0;
            }
          }
        }
#endif
      }
      continue;
    }
    // something strange
    if (sc->Check("{")) {
      sc->SkipBracketed(true);
      continue;
    }
    if (!sc->GetString()) sc->Error("unexpected EOF");
  }
}


//==========================================================================
//
//  ParseGZDoomEffectDefs
//
//==========================================================================
static void ParseGZDoomEffectDefs (int SrcLump, VScriptParser *sc, TArray<VTempClassEffects> &ClassDefs) {
  // for old mods (before Apr 2018) it should be `0.667f` (see https://forum.zdoom.org/viewtopic.php?t=60280 )
  // sadly, there is no way to autodetect it, so let's use what GZDoom is using now
  float lightsizefactor = 1.0; // for attenuated lights
  if (developer) GCon->Logf(NAME_Dev, "...parsing GZDoom light definitions from '%s'...", *sc->GetScriptName());
  while (!sc->AtEnd()) {
    if (sc->Check("#include")) {
      sc->ExpectString();
      int Lump = W_CheckNumForFileName(sc->String);
      // check WAD lump only if it's no longer than 8 characters and has no path separator
      if (Lump < 0 && sc->String.Length() <= 8 && sc->String.IndexOf('/') < 0) {
        Lump = W_CheckNumForName(VName(*sc->String, VName::AddLower8));
      }
      if (Lump < 0) sc->Error(va("Lump '%s' not found", *sc->String));
      ParseGZDoomEffectDefs(Lump, new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)), ClassDefs);
      continue;
    }
    else if (sc->Check("pointlight")) ParseGZLightDef(sc, DLTYPE_Point, lightsizefactor);
    else if (sc->Check("pulselight")) ParseGZLightDef(sc, DLTYPE_Pulse, lightsizefactor);
    else if (sc->Check("flickerlight")) ParseGZLightDef(sc, DLTYPE_Flicker, lightsizefactor);
    else if (sc->Check("flickerlight2")) ParseGZLightDef(sc, DLTYPE_FlickerRandom, lightsizefactor);
    else if (sc->Check("sectorlight")) ParseGZLightDef(sc, DLTYPE_Sector, lightsizefactor);
    else if (sc->Check("object")) ParseClassEffects(sc, ClassDefs);
    else if (sc->Check("skybox")) R_ParseMapDefSkyBoxesScript(sc);
    else if (sc->Check("brightmap")) ParseBrightmap(SrcLump, sc);
    else if (sc->Check("glow")) ParseGlow(sc);
    else if (sc->Check("hardwareshader")) { sc->Message("Shaders are not supported"); sc->SkipBracketed(); }
    else if (sc->Check("lightsizefactor")) { sc->ExpectFloat(); lightsizefactor = clampval((float)sc->Float, 0.0f, 4.0f); }
    else if (sc->Check("material")) {
      sc->Message("Materials are not supported");
      while (sc->GetString()) {
        if (sc->String == "}") break;
        if (sc->String == "{") { sc->SkipBracketed(true); break; }
      }
    } else { sc->Error(va("Unknown command (%s)", *sc->String)); }
  }
  delete sc;
}


//==========================================================================
//
//  R_ParseEffectDefs
//
//==========================================================================
void R_ParseEffectDefs () {
  GCon->Log(NAME_Init, "Parsing effect defs...");

  TArray<VTempClassEffects> ClassDefs;

  // parse VFXDEFS, GLDEFS, etc. scripts
  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) == NAME_vfxdefs) {
      ParseEffectDefs(new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)), ClassDefs);
    }
    if (W_LumpName(Lump) == NAME_gldefs ||
        W_LumpName(Lump) == NAME_doomdefs || W_LumpName(Lump) == NAME_hticdefs ||
        W_LumpName(Lump) == NAME_hexndefs || W_LumpName(Lump) == NAME_strfdefs)
    {
      ParseGZDoomEffectDefs(Lump, new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)), ClassDefs);
    }
  }

  // add effects to the classes
  for (int i = 0; i < ClassDefs.Num(); ++i) {
    VTempClassEffects &CD = ClassDefs[i];
    VClass *Cls = VClass::FindClassNoCase(*CD.ClassName);
    if (Cls) {
      // get class replacement
      Cls = Cls->GetReplacement();
    } else {
      if (dbg_show_missing_classes) {
        if (CD.StaticLight.IsNotEmpty()) {
          GCon->Logf(NAME_Warning, "No such class `%s` for static light \"%s\"", *CD.ClassName, *CD.StaticLight);
        } else {
          GCon->Logf(NAME_Warning, "No such class `%s` for effect", *CD.ClassName);
        }
      }
      continue;
    }

    if (CD.StaticLight.IsNotEmpty()) {
      VLightEffectDef *SLight = R_FindLightEffect(CD.StaticLight);
      if (SLight) {
        SetClassFieldBool(Cls, "bStaticLight", true);
        SetClassFieldInt(Cls, "LightColor", SLight->Color);
        SetClassFieldFloat(Cls, "LightRadius", SLight->Radius);
        SetClassFieldVec(Cls, "LightOffset", SLight->Offset);
      } else {
        GCon->Logf(NAME_Warning, "Light \"%s\" not found.", *CD.StaticLight);
      }
    }

    for (int j = 0; j < CD.SpriteEffects.Num(); j++) {
      VTempSpriteEffectDef &SprDef = CD.SpriteEffects[j];
      // sprite name must be either 4 or 5 chars
      if (SprDef.Sprite.Length() != 4 && SprDef.Sprite.Length() != 5) {
        GCon->Logf(NAME_Warning, "Bad sprite name length '%s', sprite effects ignored.", *SprDef.Sprite);
        continue;
      }

      if (SprDef.Sprite.length() == 5) {
        char ch = VStr::ToUpper(SprDef.Sprite[4]);
        if (ch < 'A' || ch-'A' > 36) {
          GCon->Logf(NAME_Warning, "Bad sprite frame in '%s', sprite effects ignored.", *SprDef.Sprite);
          continue;
        }
      }

      // find sprite index
      char SprName[8];
      SprName[0] = VStr::ToLower(SprDef.Sprite[0]);
      SprName[1] = VStr::ToLower(SprDef.Sprite[1]);
      SprName[2] = VStr::ToLower(SprDef.Sprite[2]);
      SprName[3] = VStr::ToLower(SprDef.Sprite[3]);
      SprName[4] = 0;
      int SprIdx = VClass::FindSprite(SprName, false);
      if (SprIdx == -1) {
        GCon->Logf(NAME_Warning, "No such sprite '%s', sprite effects ignored.", SprName);
        continue;
      }

      VSpriteEffect &SprFx = Cls->SpriteEffects.Alloc();
      SprFx.SpriteIndex = SprIdx;
      SprFx.Frame = (SprDef.Sprite.Length() == 4 ? -1 : VStr::ToUpper(SprDef.Sprite[4])-'A');
      SprFx.LightDef = nullptr;
      if (SprDef.Light.IsNotEmpty()) {
        SprFx.LightDef = R_FindLightEffect(SprDef.Light);
        if (!SprFx.LightDef) {
          GCon->Logf(NAME_Warning, "Light '%s' not found.", *SprDef.Light);
        }
      }
      SprFx.PartDef = nullptr;
      if (SprDef.Part.IsNotEmpty()) {
        SprFx.PartDef = FindParticleEffect(SprDef.Part);
        if (!SprFx.PartDef) {
          GCon->Logf(NAME_Warning, "Particle effect '%s' not found.", *SprDef.Part);
        }
      }
    }
  }
}
