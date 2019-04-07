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
vuint8 r_black_colour;
vuint8 r_white_colour;

vuint8 r_rgbtable[VAVOOM_COLOR_COMPONENT_MAX*VAVOOM_COLOR_COMPONENT_MAX*VAVOOM_COLOR_COMPONENT_MAX+4];

// variables used to look up
// and range check thing_t sprites patches
spritedef_t sprites[MAX_SPRITE_MODELS];

VTextureTranslation **TranslationTables;
int NumTranslationTables;
VTextureTranslation IceTranslation;
TArray<VTextureTranslation *> DecorateTranslations;
TArray<VTextureTranslation *> BloodTranslations;

// they basicly work the same as translations
VTextureTranslation ColourMaps[CM_Max];

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
  // We use colour 0 as transparent colour, so we must find an alternate
  // index for black colour. In Doom, Heretic and Strife there is another
  // black colour, in Hexen it's almost black.
  // I think that originaly Doom uses colour 255 as transparent colour,
  // but utilites created by others uses the alternate black colour and
  // these graphics can contain pixels of colour 255.
  // Heretic and Hexen also uses colour 255 as transparent, even more - in
  // colourmaps it's maped to colour 0. Posibly this can cause problems
  // with modified graphics.
  // Strife uses colour 0 as transparent. I already had problems with fact
  // that colour 255 is normal colour, now there shouldn't be any problems.
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
        r_black_colour = i;
        best_dist_black = dist;
      }
      // white
      if (r_color_distance_algo) {
        dist = rgbDistanceSquared(pal[i].r, pal[i].g, pal[i].b, 255, 255, 255);
        if (dist < best_dist_white) {
          r_white_colour = i;
          best_dist_white = dist;
        }
      } else {
        //dist = pal[i].r*pal[i].r+pal[i].g*pal[i].g+pal[i].b*pal[i].b;
        if (dist > best_dist_white) {
          r_white_colour = i;
          best_dist_white = dist;
        }
      }
    }
  }
  //GCon->Logf("black=%d:(%02x_%02x_%02x); while=%d:(%02x_%02x_%02x)", r_black_colour, pal[r_black_colour].r, pal[r_black_colour].g, pal[r_black_colour].b,
  //  r_white_colour, pal[r_white_colour].r, pal[r_white_colour].g, pal[r_white_colour].b);
}


//==========================================================================
//
//  InitRgbTable
//
//==========================================================================
static void InitRgbTable () {
  VStr rtblsize;
       if (sizeof(r_rgbtable) < 1024*1024) rtblsize = va("%u KB", (unsigned)(sizeof(r_rgbtable)/1024));
  else rtblsize = va("%u MB", (unsigned)(sizeof(r_rgbtable)/1024/1024));
  GCon->Logf(NAME_Init, "building color translation table (%d, %s)...", VAVOOM_COLOR_COMPONENT_MAX, *rtblsize);
  memset(r_rgbtable, 0, sizeof(r_rgbtable));
  for (int ir = 0; ir < VAVOOM_COLOR_COMPONENT_MAX; ++ir) {
    for (int ig = 0; ig < VAVOOM_COLOR_COMPONENT_MAX; ++ig) {
      for (int ib = 0; ib < VAVOOM_COLOR_COMPONENT_MAX; ++ib) {
        const int r = (int)(ir*255.0f/((float)(VAVOOM_COLOR_COMPONENT_MAX-1))/*+0.5f*/);
        const int g = (int)(ig*255.0f/((float)(VAVOOM_COLOR_COMPONENT_MAX-1))/*+0.5f*/);
        const int b = (int)(ib*255.0f/((float)(VAVOOM_COLOR_COMPONENT_MAX-1))/*+0.5f*/);
        int best_colour = -1;
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
          if (best_colour < 0 || dist < best_dist) {
            best_colour = i;
            best_dist = dist;
            if (!dist) break;
          }
        }
        check(best_colour > 0 && best_colour <= 255);
        r_rgbtable[ir*VAVOOM_COLOR_COMPONENT_MAX*VAVOOM_COLOR_COMPONENT_MAX+ig*VAVOOM_COLOR_COMPONENT_MAX+ib] = best_colour;
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
        // make sure that normal colours doesn't map to colour 0
        if (Trans->Table[i] == 0) Trans->Table[i] = r_black_colour;
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
//  InitColourMaps
//
//==========================================================================
static void InitColourMaps () {
  // calculate inverse colourmap
  VTextureTranslation *T = &ColourMaps[CM_Inverse];
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

  // calculate gold colourmap
  T = &ColourMaps[CM_Gold];
  T->Table[0] = 0;
  T->Palette[0] = r_palette[0];
  for (int i = 1; i < 256; ++i) {
    int Gray = (r_palette[i].r*77+r_palette[i].g*143+r_palette[i].b*37)>>8;
    T->Palette[i].r = MIN(255, Gray+Gray/2);
    T->Palette[i].g = Gray;
    T->Palette[i].b = 0;
    T->Palette[i].a = 255;
    T->Table[i] = R_LookupRGB(T->Palette[i].r, T->Palette[i].g, T->Palette[i].b);
  }

  // calculate red colourmap
  T = &ColourMaps[CM_Red];
  T->Table[0] = 0;
  T->Palette[0] = r_palette[0];
  for (int i = 1; i < 256; ++i) {
    int Gray = (r_palette[i].r*77+r_palette[i].g*143+r_palette[i].b*37)>>8;
    T->Palette[i].r = MIN(255, Gray+Gray/2);
    T->Palette[i].g = 0;
    T->Palette[i].b = 0;
    T->Palette[i].a = 255;
    T->Table[i] = R_LookupRGB(T->Palette[i].r, T->Palette[i].g, T->Palette[i].b);
  }

  // calculate green colourmap
  T = &ColourMaps[CM_Green];
  T->Table[0] = 0;
  T->Palette[0] = r_palette[0];
  for (int i = 1; i < 256; ++i) {
    int Gray = (r_palette[i].r*77+r_palette[i].g*143+r_palette[i].b*37)>>8;
    T->Palette[i].r = MIN(255, Gray+Gray/2);
    T->Palette[i].g = MIN(255, Gray+Gray/2);
    T->Palette[i].b = Gray;
    T->Palette[i].a = 255;
    T->Table[i] = R_LookupRGB(T->Palette[i].r, T->Palette[i].g, T->Palette[i].b);
  }
}


//==========================================================================
//
//  InstallSpriteLump
//
//  Local function for R_InitSprites.
//
//==========================================================================
static void InstallSpriteLump (int lumpnr, int frame, char Rot, bool flipped) {
  int rotation;

  //fprintf(stderr, "!!INSTALL_SPRITE_LUMP: <%s> (lumpnr=%d; frame=%d; Rot=%c; flipped=%d)\n", *GTextureManager[lumpnr]->Name, lumpnr, frame, Rot, (flipped ? 1 : 0));

       if (Rot >= '0' && Rot <= '9') rotation = Rot-'0';
  else if (Rot >= 'a') rotation = Rot-'a'+10;
  else rotation = 17;

  VTexture *Tex = GTextureManager[lumpnr];
  if ((vuint32)frame >= 30 || (vuint32)rotation > 16) {
    //Sys_Error("InstallSpriteLump: Bad frame characters in lump '%s'", *Tex->Name);
    GCon->Logf("ERROR:InstallSpriteLump: Bad frame characters in lump '%s'", *Tex->Name);
    for (int r = 0; r < 16; ++r) {
      sprtemp[frame].lump[r] = -1;
      sprtemp[frame].flip[r] = false;
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
  if ((vuint32)index >= MAX_SPRITE_MODELS) Host_Error("Invalid sprite index %d for sprite %s", index, name);
  //fprintf(stderr, "!!INSTALL_SPRITE: <%s> (%d)\n", name, index);
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

  // scan all the lump names for each of the names, noting the highest frame letter
  // just compare 4 characters as ints
  int intname = *(int*)*VName(spritename, VName::AddLower8);

  // scan the lumps, filling in the frames for whatever is found
  for (int l = 0; l < GTextureManager.GetNumTextures(); ++l) {
    if (GTextureManager[l]->Type == TEXTYPE_Sprite) {
      const char *lumpname = *GTextureManager[l]->Name;
      if (*(int*)lumpname == intname) {
        //fprintf(stderr, "!!<%s> [4]=%c; [6]=%c; [7]=%c\n", lumpname, lumpname[4], lumpname[6], (lumpname[6] ? lumpname[7] : 0));
        InstallSpriteLump(l, VStr::ToUpper(lumpname[4])-'A', lumpname[5], false);
        if (lumpname && strlen(lumpname) >= 6 && lumpname[6]) {
          InstallSpriteLump(l, VStr::ToUpper(lumpname[6])-'A', lumpname[7], true);
        }
      }
    }
  }

  // check the frames that were found for completeness
  if (maxframe == -1) {
    sprites[index].numframes = 0;
    return;
  }

  ++maxframe;

  for (int frame = 0; frame < maxframe; ++frame) {
    //fprintf(stderr, "  frame=%d; rot=%d (%u)\n", frame, (int)sprtemp[frame].rotate, *((unsigned char *)&sprtemp[frame].rotate));
    switch ((int)sprtemp[frame].rotate) {
      case -1:
        // no rotations were found for that frame at all
        if (GArgs.CheckParm("-sprstrict")) {
          Sys_Error("R_InstallSprite: No patches found for '%s' frame '%c'", spritename, frame+'A');
        } else {
          if (spr_report_missing_patches) {
            GCon->Logf("R_InstallSprite: No patches found for '%s' frame '%c'", spritename, frame+'A');
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
                GCon->Logf("R_InstallSprite: Sprite '%s' frame '%c' is missing rotations", spritename, frame+'A');
              }
            }
          }
        }
        break;
    }
  }

  if (sprites[index].spriteframes) {
    Z_Free(sprites[index].spriteframes);
    sprites[index].spriteframes = nullptr;
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
  for (int i = 0; i < MAX_SPRITE_MODELS; ++i) {
    if (sprites[i].spriteframes) {
      Z_Free(sprites[i].spriteframes);
    }
  }
}


//==========================================================================
//
//  R_AreSpritesPresent
//
//==========================================================================
bool R_AreSpritesPresent (int Index) {
  return (sprites[Index].numframes > 0);
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
  // init colour maps
  InitColourMaps();
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
//  VTextureTranslation::VTextureTranslation
//
//==========================================================================
VTextureTranslation::VTextureTranslation()
  : Crc(0)
  , TranslStart(0)
  , TranslEnd(0)
  , Colour(0)
{
  Clear();
}


//==========================================================================
//
//  VTextureTranslation::Clear
//
//==========================================================================
void VTextureTranslation::Clear () {
  for (int i = 0; i < 256; ++i) {
    Table[i] = i;
    Palette[i] = r_palette[i];
  }
  Commands.Clear();
  CalcCrc();
}


//==========================================================================
//
//  VTextureTranslation::CalcCrc
//
//==========================================================================
void VTextureTranslation::CalcCrc () {
  auto Work = TCRC16();
  for (int i = 1; i < 256; ++i) {
    Work += Palette[i].r;
    Work += Palette[i].g;
    Work += Palette[i].b;
  }
  Crc = Work;
}


//==========================================================================
//
//  VTextureTranslation::Serialise
//
//==========================================================================
void VTextureTranslation::Serialise (VStream &Strm) {
  vuint8 xver = 0; // current version is 0
  Strm << xver;
  Strm.Serialise(Table, 256);
  Strm.Serialise(Palette, sizeof(Palette));
  Strm << Crc
    << TranslStart
    << TranslEnd
    << Colour;
  int CmdsSize = Commands.Num();
  Strm << STRM_INDEX(CmdsSize);
  if (Strm.IsLoading()) Commands.SetNum(CmdsSize);
  for (int i = 0; i < CmdsSize; ++i) {
    VTransCmd &C = Commands[i];
    Strm << C.Type << C.Start << C.End << C.R1 << C.G1 << C.B1 << C.R2 << C.G2 << C.B2;
  }
}


//==========================================================================
//
//  VTextureTranslation::BuildPlayerTrans
//
//==========================================================================
void VTextureTranslation::BuildPlayerTrans (int Start, int End, int Col) {
  int Count = End-Start+1;
  vuint8 r = (Col>>16)&255;
  vuint8 g = (Col>>8)&255;
  vuint8 b = Col&255;
  vuint8 h, s, v;
  M_RgbToHsv(r, g, b, h, s, v);
  for (int i = 0; i < Count; ++i) {
    int Idx = Start+i;
    vuint8 TmpH, TmpS, TmpV;
    M_RgbToHsv(Palette[Idx].r, Palette[Idx].g,Palette[Idx].b, TmpH, TmpS, TmpV);
    M_HsvToRgb(h, s, v*TmpV/255, Palette[Idx].r, Palette[Idx].g, Palette[Idx].b);
    Table[Idx] = R_LookupRGB(Palette[Idx].r, Palette[Idx].g, Palette[Idx].b);
  }
  //for (int f = 0; f < 256; ++f) Table[f] = R_LookupRGB(255, 0, 0);
  CalcCrc();
  TranslStart = Start;
  TranslEnd = End;
  Colour = Col;
}


//==========================================================================
//
//  VTextureTranslation::MapToRange
//
//==========================================================================
void VTextureTranslation::MapToRange (int AStart, int AEnd, int ASrcStart, int ASrcEnd) {
  int Start;
  int End;
  int SrcStart;
  int SrcEnd;
  // swap range if necesary
  if (AStart > AEnd) {
    Start = AEnd;
    End = AStart;
    SrcStart = ASrcEnd;
    SrcEnd = ASrcStart;
  } else {
    Start = AStart;
    End = AEnd;
    SrcStart = ASrcStart;
    SrcEnd = ASrcEnd;
  }
  // check for single colour change
  if (Start == End) {
    Table[Start] = SrcStart;
    Palette[Start] = r_palette[SrcStart];
    return;
  }
  float CurCol = SrcStart;
  float ColStep = (float(SrcEnd)-float(SrcStart))/(float(End)-float(Start));
  for (int i = Start; i < End; ++i, CurCol += ColStep) {
    Table[i] = int(CurCol);
    Palette[i] = r_palette[Table[i]];
  }
  //for (int f = 0; f < 256; ++f) Table[f] = R_LookupRGB(255, 0, 0);
  VTransCmd &C = Commands.Alloc();
  C.Type = 0;
  C.Start = Start;
  C.End = End;
  C.R1 = SrcStart;
  C.R2 = SrcEnd;
  CalcCrc();
}


//==========================================================================
//
//  VTextureTranslation::MapToColours
//
//==========================================================================
void VTextureTranslation::MapToColours (int AStart, int AEnd,
                                        int AR1, int AG1, int AB1,
                                        int AR2, int AG2, int AB2)
{
  int Start;
  int End;
  int R1, G1, B1;
  int R2, G2, B2;
  // swap range if necesary
  if (AStart > AEnd) {
    Start = AEnd;
    End = AStart;
    R1 = AR2;
    G1 = AG2;
    B1 = AB2;
    R2 = AR1;
    G2 = AG1;
    B2 = AB1;
  } else {
    Start = AStart;
    End = AEnd;
    R1 = AR1;
    G1 = AG1;
    B1 = AB1;
    R2 = AR2;
    G2 = AG2;
    B2 = AB2;
  }
  // check for single colour change
  if (Start == End) {
    Palette[Start].r = R1;
    Palette[Start].g = G1;
    Palette[Start].b = B1;
    Table[Start] = R_LookupRGB(R1, G1, B1);
    return;
  }
  float CurR = R1;
  float CurG = G1;
  float CurB = B1;
  float RStep = (float(R2)-float(R1))/(float(End)-float(Start));
  float GStep = (float(G2)-float(G1))/(float(End)-float(Start));
  float BStep = (float(B2)-float(B1))/(float(End)-float(Start));
  if (!isFiniteF(RStep)) RStep = 0;
  if (!isFiniteF(GStep)) GStep = 0;
  if (!isFiniteF(BStep)) BStep = 0;
  for (int i = Start; i < End; i++, CurR += RStep, CurG += GStep, CurB += BStep) {
    Palette[i].r = int(CurR);
    Palette[i].g = int(CurG);
    Palette[i].b = int(CurB);
    Table[i] = R_LookupRGB(Palette[i].r, Palette[i].g, Palette[i].b);
  }
  VTransCmd &C = Commands.Alloc();
  C.Type = 1;
  C.Start = Start;
  C.End = End;
  C.R1 = R1;
  C.G1 = G1;
  C.B1 = B1;
  C.R2 = R2;
  C.G2 = G2;
  C.B2 = B2;
  CalcCrc();
}


//==========================================================================
//
//  VTextureTranslation::BuildBloodTrans
//
//==========================================================================
void VTextureTranslation::BuildBloodTrans (int Col) {
  vuint8 r = (Col>>16)&255;
  vuint8 g = (Col>>8)&255;
  vuint8 b = Col&255;
  // don't remap colour 0
  for (int i = 1; i < 256; ++i) {
    int Bright = MAX(MAX(r_palette[i].r, r_palette[i].g), r_palette[i].b);
    Palette[i].r = r*Bright/255;
    Palette[i].g = g*Bright/255;
    Palette[i].b = b*Bright/255;
    Table[i] = R_LookupRGB(Palette[i].r, Palette[i].g, Palette[i].b);
    //Table[i] = R_LookupRGB(255, 0, 0);
  }
  CalcCrc();
  Colour = Col;
}


//==========================================================================
//
//  CheckChar
//
//==========================================================================
static bool CheckChar (const char *&pStr, char Chr) {
  // skip whitespace
  while (*pStr && *((const vuint8 *)pStr) <= ' ') ++pStr;
  if (*pStr != Chr) return false;
  ++pStr;
  return true;
}


//==========================================================================
//
//  VTextureTranslation::AddTransString
//
//==========================================================================
void VTextureTranslation::AddTransString (const VStr &Str) {
  const char *pStr = *Str;

  // parse start and end of the range
  int Start = strtol(pStr, (char **)&pStr, 10);
  if (!CheckChar(pStr, ':')) return;

  int End = strtol(pStr, (char **)&pStr, 10);
  if (!CheckChar(pStr, '=')) return;

  if (!CheckChar(pStr, '[')) {
    int SrcStart = strtol(pStr, (char **)&pStr, 10);
    if (!CheckChar(pStr, ':')) return;
    int SrcEnd = strtol(pStr, (char **)&pStr, 10);
    MapToRange(Start, End, SrcStart, SrcEnd);
  } else {
    int R1 = strtol(pStr, (char **)&pStr, 10);
    if (!CheckChar(pStr, ',')) return;
    int G1 = strtol(pStr, (char **)&pStr, 10);
    if (!CheckChar(pStr, ',')) return;
    int B1 = strtol(pStr, (char **)&pStr, 10);
    if (!CheckChar(pStr, ']')) return;
    if (!CheckChar(pStr, ':')) return;
    if (!CheckChar(pStr, '[')) return;
    int R2 = strtol(pStr, (char **)&pStr, 10);
    if (!CheckChar(pStr, ',')) return;
    int G2 = strtol(pStr, (char **)&pStr, 10);
    if (!CheckChar(pStr, ',')) return;
    int B2 = strtol(pStr, (char **)&pStr, 10);
    if (!CheckChar(pStr, ']')) return;
    MapToColours(Start, End, R1, G1, B1, R2, G2, B2);
  }
}


//==========================================================================
//
//  R_ParseDecorateTranslation
//
//==========================================================================
int R_ParseDecorateTranslation (VScriptParser *sc, int GameMax) {
  // first check for standard translation
  if (sc->CheckNumber()) {
    if (sc->Number < 0 || sc->Number >= MAX(NumTranslationTables, GameMax)) {
      //sc->Error(va("Translation must be in range [0, %d]", MAX(NumTranslationTables, GameMax)-1));
      GCon->Logf(NAME_Warning, "%s: Translation must be in range [0, %d]", *sc->GetLoc().toStringNoCol(), MAX(NumTranslationTables, GameMax)-1);
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
    if (BloodTranslations[i]->Colour == Col) {
      return (TRANSL_Blood<<TRANSL_TYPE_SHIFT)+i;
    }
  }

  // create new translation
  VTextureTranslation *Tr = new VTextureTranslation;
  Tr->BuildBloodTrans(Col);

  // add it
  if (BloodTranslations.Num() >= MAX_BLOOD_TRANSLATIONS) {
    Sys_Error("Too many blood colours in DECORATE scripts");
  }
  BloodTranslations.Append(Tr);
  return (TRANSL_Blood<<TRANSL_TYPE_SHIFT)+BloodTranslations.Num()-1;
}


//==========================================================================
//
//  R_FindLightEffect
//
//==========================================================================
VLightEffectDef *R_FindLightEffect (const VStr &Name) {
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
  L->Colour = 0xffffffff;
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
    if (sc->Check("colour")) {
      sc->ExpectFloat();
      float r = MID(0.0f, (float)sc->Float, 1.0f);
      sc->ExpectFloat();
      float g = MID(0.0f, (float)sc->Float, 1.0f);
      sc->ExpectFloat();
      float b = MID(0.0f, (float)sc->Float, 1.0f);
      L->Colour = ((int)(r*255)<<16)|((int)(g*255)<<8)|(int)(b*255)|0xff000000;
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
//  IntensityToRadius
//
//==========================================================================
static inline float IntensityToRadius (float Val) {
  if (Val <= 20.0f) return Val*4.5f;
  if (Val <= 30.0f) return Val*3.6f;
  if (Val <= 40.0f) return Val*3.3f;
  if (Val <= 60.0f) return Val*2.8f;
  return Val*2.5f;
}


//==========================================================================
//
//  ParseGZLightDef
//
//==========================================================================
static void ParseGZLightDef (VScriptParser *sc, int LightType) {
  // get name, find it in the list or add it if it's not there yet
  sc->ExpectString();
  VLightEffectDef *L = R_FindLightEffect(sc->String);
  if (!L) L = &GLightEffectDefs.Alloc();

  // set default values
  L->Name = *sc->String.ToLower();
  L->Type = LightType;
  L->Colour = 0xffffffff;
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
    if (sc->Check("color")) {
      sc->ExpectFloat();
      float r = MID(0.0f, (float)sc->Float, 1.0f);
      sc->ExpectFloat();
      float g = MID(0.0f, (float)sc->Float, 1.0f);
      sc->ExpectFloat();
      float b = MID(0.0f, (float)sc->Float, 1.0f);
      L->Colour = ((int)(r*255)<<16)|((int)(g*255)<<8)|(int)(b*255)|0xff000000;
    } else if (sc->Check("size")) {
      sc->ExpectNumber();
      L->Radius = IntensityToRadius(float(sc->Number));
    } else if (sc->Check("secondarySize")) {
      sc->ExpectNumber();
      L->Radius2 = IntensityToRadius(float(sc->Number));
    } else if (sc->Check("offset")) {
      // GZDoom manages Z offset as Y offset
      sc->ExpectNumber();
      L->Offset.x = float(sc->Number);
      sc->ExpectNumber();
      L->Offset.z = float(sc->Number);
      sc->ExpectNumber();
      L->Offset.y = float(sc->Number);
    } else if (sc->Check("subtractive")) {
      sc->ExpectNumber();
      sc->Message("Subtractive lights not supported.");
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
      sc->Message("Additive parameter not supported.");
    } else if (sc->Check("halo")) {
      sc->ExpectNumber();
      sc->Message("Halo parameter not supported.");
    } else if (sc->Check("dontlightself")) {
      sc->ExpectNumber();
      sc->Message("DontLightSelf parameter not supported.");
    } else if (sc->Check("attenuate")) {
      sc->ExpectNumber();
      sc->Message("attenuate parameter not supported.");
    } else {
      sc->Error(va("Bad gz light parameter (%s)", *sc->String));
    }
  }
}


//==========================================================================
//
//  FindParticleEffect
//
//==========================================================================
static VParticleEffectDef *FindParticleEffect (const VStr &Name) {
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
  P->Colour = 0xffffffff;
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
    } else if (sc->Check("colour")) {
      sc->ExpectFloat();
      float r = MID(0.0f, (float)sc->Float, 1.0f);
      sc->ExpectFloat();
      float g = MID(0.0f, (float)sc->Float, 1.0f);
      sc->ExpectFloat();
      float b = MID(0.0f, (float)sc->Float, 1.0f);
      P->Colour = ((int)(r*255)<<16)|((int)(g*255)<<8)|(int)(b*255)|0xff000000;
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
  while (!sc->AtEnd()) {
    if (sc->Check("#include")) {
      sc->ExpectString();
      int Lump = W_CheckNumForFileName(sc->String);
      // check WAD lump only if it's no longer than 8 characters and has no path separator
      if (Lump < 0 && sc->String.Length() <= 8 && sc->String.IndexOf('/') < 0) {
        Lump = W_CheckNumForName(VName(*sc->String, VName::AddLower8));
      }
      if (Lump < 0) sc->Error(va("Lump %s not found", *sc->String));
      ParseEffectDefs(new VScriptParser(/*sc->String*/W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)), ClassDefs);
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
static void ParseBrightmap (VScriptParser *sc) {
       if (sc->Check("flat")) {}
  else if (sc->Check("sprite")) {}
  else if (sc->Check("texture")) {}
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
      if (!sc->GetString()) sc->Error("brightmap image name expected");
      if (sc->String.isEmpty()) sc->Error("empty brightmap image");
      if (!bmap.isEmpty()) GCon->Logf(NAME_Warning, "duplicate brightmap image");
      bmap = sc->String;
    } else if (sc->Check("iwad")) {
      iwad = true;
    } else if (sc->Check("thiswad")) {
      thiswad = true;
    } else if (sc->Check("disablefullbright")) {
      nofb = true;
    } else {
      //sc->Error(va("Unknown command (%s)", *sc->String));
      sc->ExpectString();
      sc->Message(va("Unknown command (%s)", *sc->String));
    }
  }
  (void)img;
  (void)iwad;
  (void)thiswad;
  (void)nofb;
  // there is no need to load brightmap textures for server
#ifdef CLIENT
  if (img != NAME_None && VStr::Cmp(*img, "-") != 0 && !bmap.isEmpty()) {
    static int doWarn = -1;
    if (doWarn < 0) doWarn = (GArgs.CheckParm("-Wall") || GArgs.CheckParm("-Wbrightmap") ? 1 : 0);

    static int doLoadDump = -1;
    if (doLoadDump < 0) doLoadDump = (GArgs.CheckParm("-dump-brightmaps") ? 1 : 0);

    VTexture *basetex = GTextureManager.GetExistingTextureByName(VStr(img));
    if (!basetex) {
      if (doWarn) GCon->Logf(NAME_Warning, "texture '%s' not found, cannot attach brightmap", *img);
      return;
    }

    if (iwad && !W_IsIWADLump(basetex->SourceLump)) {
      // oops
      if (doWarn) GCon->Logf(NAME_Warning, "IWAD SKIP! '%s' (%s[%d]; '%s')", *img, *W_FullLumpName(basetex->SourceLump), basetex->SourceLump, *basetex->Name);
      return;
    }
    //if (iwad && doWarn) GCon->Logf(NAME_Warning, "IWAD PASS! '%s'", *img);

    basetex->nofullbright = nofb;
    delete basetex->Brightmap;
    basetex->Brightmap = nullptr;

    int lmp = W_GetNumForFileName(bmap);
    if (lmp < 0) {
      GCon->Logf(NAME_Warning, "brightmap texture '%s' not found", *bmap);
      return;
    }

    if (thiswad && W_LumpFile(lmp) != W_LumpFile(basetex->SourceLump)) return;

    VTexture *bm = VTexture::CreateTexture(TEXTYPE_Any, lmp);
    if (!bm) {
      GCon->Logf(NAME_Warning, "cannot load brightmap texture '%s'", *bmap);
      return;
    }
    bm->nofullbright = nofb; // just in case

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
    if (doLoadDump) GCon->Logf(NAME_Warning, "texture '%s' got brightmap '%s' (%p)", *basetex->Name, *bm->Name, basetex);
  }
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
      while (sc->Check(",")) sc->GetString();
      continue;
    }
    // texture list
    if (sc->Check("flats") || sc->Check("walls")) {
      sc->Expect("{");
      while (!sc->Check("}")) {
        if (sc->Check(",")) continue;
        sc->ExpectName8Warn();
#ifdef CLIENT
        VName img = sc->Name8;
        if (img != NAME_None && img != "-") {
          VTexture *basetex = GTextureManager.GetExistingTextureByName(VStr(img));
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
static void ParseGZDoomEffectDefs (VScriptParser *sc, TArray<VTempClassEffects> &ClassDefs) {
  while (!sc->AtEnd()) {
    if (sc->Check("#include")) {
      sc->ExpectString();
      int Lump = W_CheckNumForFileName(sc->String);
      // check WAD lump only if it's no longer than 8 characters and has no path separator
      if (Lump < 0 && sc->String.Length() <= 8 && sc->String.IndexOf('/') < 0) {
        Lump = W_CheckNumForName(VName(*sc->String, VName::AddLower8));
      }
      if (Lump < 0) sc->Error(va("Lump %s not found", *sc->String));
      ParseGZDoomEffectDefs(new VScriptParser(/*sc->String*/W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)), ClassDefs);
      continue;
    }
    else if (sc->Check("pointlight")) ParseGZLightDef(sc, DLTYPE_Point);
    else if (sc->Check("pulselight")) ParseGZLightDef(sc, DLTYPE_Pulse);
    else if (sc->Check("flickerlight")) ParseGZLightDef(sc, DLTYPE_Flicker);
    else if (sc->Check("flickerlight2")) ParseGZLightDef(sc, DLTYPE_FlickerRandom);
    else if (sc->Check("sectorlight")) ParseGZLightDef(sc, DLTYPE_Sector);
    else if (sc->Check("object")) ParseClassEffects(sc, ClassDefs);
    else if (sc->Check("skybox")) R_ParseMapDefSkyBoxesScript(sc);
    else if (sc->Check("brightmap")) ParseBrightmap(sc);
    else if (sc->Check("glow")) ParseGlow(sc);
    else if (sc->Check("hardwareshader")) { sc->Message("Shaders are not supported"); sc->SkipBracketed(); }
    else { sc->Error(va("Unknown command (%s)", *sc->String)); }
  }
  delete sc;
}


//==========================================================================
//
//  R_ParseEffectDefs
//
//==========================================================================
void R_ParseEffectDefs () {
  GCon->Log(NAME_Init, "Parsing effect defs");

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
      ParseGZDoomEffectDefs(new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)), ClassDefs);
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
        SetClassFieldInt(Cls, "LightColour", SLight->Colour);
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
          GCon->Logf(NAME_Warning, "Light \"%s\" not found.", *SprDef.Light);
        }
      }
      SprFx.PartDef = nullptr;
      if (SprDef.Part.IsNotEmpty()) {
        SprFx.PartDef = FindParticleEffect(SprDef.Part);
        if (!SprFx.PartDef) {
          GCon->Logf(NAME_Warning, "Particle effect \"%s\" not found.", *SprDef.Part);
        }
      }
    }
  }
}
