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


//==========================================================================
//
//  VFlatTexture::VFlatTexture
//
//==========================================================================
VTexture *VFlatTexture::Create (VStream &, int LumpNum) {
  return new VFlatTexture(LumpNum);
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
  mFormat = TEXFMT_8;
  Name = W_LumpName(SourceLump);
  Width = 64;
  // check for larger flats
  while (W_LumpLength(SourceLump) >= Width*Width*4) Width *= 2;
  Height = Width;
  // scale large flats to 64x64
  if (Width > 64) {
    SScale = Width/64;
    TScale = Width/64;
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
    VStream *lumpstream = W_CreateLumpReaderNum(SourceLump);
    VCheckedStream Strm(lumpstream);
    for (int i = 0; i < Width*Height; ++i) {
      Strm << Pixels[i];
      if (!Pixels[i]) Pixels[i] = r_black_color;
    }
  }

  ConvertPixelsToShaded();
  return Pixels;
}


//==========================================================================
//
//  VFlatTexture::Unload
//
//==========================================================================
void VFlatTexture::Unload () {
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
}
