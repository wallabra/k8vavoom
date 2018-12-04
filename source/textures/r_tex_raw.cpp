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
#include "r_tex.h"


//==========================================================================
//
//  VRawPicTexture::Create
//
//==========================================================================
VTexture *VRawPicTexture::Create (VStream &Strm, int LumpNum) {
  guard(VRawPicTexture::Create);
  if (Strm.TotalSize() != 64000) return nullptr; // wrong size

  // do an extra check to see if it's a valid patch
  vint16 Width;
  vint16 Height;
  vint16 SOffset;
  vint16 TOffset;

  Strm.Seek(0);
  Strm << Width << Height << SOffset << TOffset;
  if (Width > 0 && Height > 0 && Width <= 2048 && Height < 510) {
    // dimensions seem to be valid; check column directory to see if it's valid
    // we expect at least one column to start exactly right after the directory
    bool GapAtStart = true;
    bool IsValid = true;
    vint32 *Offsets = new vint32[Width];
    for (int i = 0; i < Width; ++i) {
      Strm << Offsets[i];
      if (Offsets[i] == 8+Width*4) {
        GapAtStart = false;
      } else if (Offsets[i] < 8+Width*4 || Offsets[i] >= Strm.TotalSize()) {
        IsValid = false;
        break;
      }
    }
    if (IsValid && !GapAtStart) {
      // offsets seem to be valid
      delete[] Offsets;
      return nullptr;
    }
    delete[] Offsets;
  }

  return new VRawPicTexture(LumpNum, -1);
  unguard;
}


//==========================================================================
//
//  VRawPicTexture::VRawPicTexture
//
//==========================================================================
VRawPicTexture::VRawPicTexture (int ALumpNum, int APalLumpNum)
  : VTexture()
  , PalLumpNum(APalLumpNum)
  , Pixels(nullptr)
  , Palette(nullptr)
{
  SourceLump = ALumpNum;
  Type = TEXTYPE_Pic;
  Name = W_LumpName(SourceLump);
  Width = 320;
  Height = 200;
  Format = (PalLumpNum >= 0 ? TEXFMT_8Pal : TEXFMT_8);
}


//==========================================================================
//
//  VRawPicTexture::~VRawPicTexture
//
//==========================================================================
VRawPicTexture::~VRawPicTexture () {
  //guard(VRawPicTexture::~VRawPicTexture);
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
  if (Palette) {
    delete[] Palette;
    Palette = nullptr;
  }
  //unguard;
}


//==========================================================================
//
//  VRawPicTexture::GetPixels
//
//==========================================================================
vuint8 *VRawPicTexture::GetPixels () {
  guard(VRawPicTexture::GetPixels);
  // if already got pixels, then just return them
  if (Pixels) return Pixels;

  Pixels = new vuint8[64000];

  // set up palette
  int black;
  if (PalLumpNum < 0) {
    black = r_black_colour;
  } else {
    // load palette and find black colour for remaping
    Palette = new rgba_t[256];
    VStream *PStrm = W_CreateLumpReaderNum(PalLumpNum);
    int best_dist = 0x10000;
    black = 0;
    for (int i = 0; i < 256; ++i) {
      *PStrm << Palette[i].r << Palette[i].g << Palette[i].b;
      if (i == 0) {
        Palette[i].a = 0;
      } else {
        Palette[i].a = 255;
        int dist = Palette[i].r*Palette[i].r+Palette[i].g*Palette[i].g+Palette[i].b*Palette[i].b;
        if (dist < best_dist) {
          black = i;
          best_dist = dist;
        }
      }
    }
    delete PStrm;
  }

  // read data
  VStream *Strm = W_CreateLumpReaderNum(SourceLump);
  vuint8 *dst = Pixels;
  for (int i = 0; i < 64000; ++i, ++dst) {
    *Strm << *dst;
    if (!*dst) *dst = black;
  }
  delete Strm;

  if (origFormat != -1) {
    Format = origFormat;
    Pixels = ConvertPixelsToShaded(Pixels);
  }

  return Pixels;
  unguard;
}


//==========================================================================
//
//  VRawPicTexture::GetPalette
//
//==========================================================================
rgba_t *VRawPicTexture::GetPalette () {
  guardSlow(VRawPicTexture::GetPalette);
  return (Palette ? Palette : r_palette);
  unguardSlow;
}


//==========================================================================
//
//  VRawPicTexture::Unload
//
//==========================================================================
void VRawPicTexture::Unload () {
  guard(VRawPicTexture::Unload);
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
  if (Palette) {
    delete[] Palette;
    Palette = nullptr;
  }
  unguard;
}
