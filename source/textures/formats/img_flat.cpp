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
#include "../../gamedefs.h"
#include "../r_tex.h"


//==========================================================================
//
//  isFuckedHereticLump
//
//==========================================================================
static bool isFuckedHereticLump (int LumpNum) {
  if (LumpNum <= 0) return false;
  int lmpsize = W_LumpLength(LumpNum);
  if (lmpsize != 4160) return false;
  static const char *fuckedHeretic[7] = {"fltflww1","fltflww2","fltflww3","fltlava1","fltlava2","fltlava3","fltlava4"};
  const char *lname = *W_LumpName(LumpNum);
  for (unsigned f = 0; f < 7; ++f) {
    if (VStr::strEquCI(lname, fuckedHeretic[f])) return true;
  }
  return false;
}


//==========================================================================
//
//  VFlatTexture::Create
//
//==========================================================================
VTexture *VFlatTexture::Create (VStream &, int LumpNum) {
  if (isFuckedHereticLump(LumpNum)) return new VFlatTexture(LumpNum); // fucked Heretic flat
  const int lmpsize = W_LumpLength(LumpNum);
  if (lmpsize == 8192) return new VFlatTexture(LumpNum); // 64x128 (some idiots does this)
  for (int Width = 8; Width <= 256; Width <<= 1) {
    if (lmpsize < Width*Width) return nullptr;
    if (lmpsize == Width*Width) return new VFlatTexture(LumpNum); // Doom flat
  }
  return nullptr;
}


//==========================================================================
//
//  VFlatTexture::VFlatTexture
//
//==========================================================================
VFlatTexture::VFlatTexture (int InLumpNum)
  : VTexture()
{
  SourceLump = InLumpNum;
  Type = TEXTYPE_Flat;
  mFormat = mOrigFormat = TEXFMT_8;
  Name = W_LumpName(SourceLump);
  Width = 64;
  // check for larger flats
  if (isFuckedHereticLump(SourceLump)) {
    Height = 64;
  } else {
    //while (W_LumpLength(SourceLump) >= Width*Width*4) Width *= 2;
    int lmpsize = W_LumpLength(SourceLump);
    if (lmpsize == 8192) {
      // 64x128 (some idiots does this)
      Width = 64;
      Height = 128;
    } else {
      for (int Width = 8; Width <= 256; Width <<= 1) {
        if (lmpsize < Width*Width) Sys_Error("invalid doom flat texture '%s' (curWidth=%d; lmpsize=%d; reqsize=%d)", *W_FullLumpName(SourceLump), Width, lmpsize, Width*Width);
        if (lmpsize == Width*Width) break;
      }
      Height = Width;
    }
  }
  // scale large flats to 64x64
  if (Width > 64) {
    SScale = Width/64.0f;
    TScale = Width/64.0f;
  }
}


//==========================================================================
//
//  VFlatTexture::~VFlatTexture
//
//==========================================================================
VFlatTexture::~VFlatTexture () {
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
}


//==========================================================================
//
//  VFlatTexture::GetPixels
//
//==========================================================================
vuint8 *VFlatTexture::GetPixels () {
  // if already got pixels, then just return them
  if (Pixels) return Pixels;

  // allocate memory buffer
  Pixels = new vuint8[Width*Height];
  transparent = false;
  translucent = false;

  // a flat must be at least 64x64, if it's smaller, then ignore it
  if (W_LumpLength(SourceLump) < 64*64) {
    //memset(Pixels, 0, 64*64);
    checkerFill8(Pixels, Width, Height);
  } else {
    // read data
    VCheckedStream Strm(SourceLump);
    for (int i = 0; i < Width*Height; ++i) {
      Strm << Pixels[i];
      if (!Pixels[i]) Pixels[i] = r_black_color;
    }
  }

  ConvertPixelsToShaded();
  return Pixels;
}
