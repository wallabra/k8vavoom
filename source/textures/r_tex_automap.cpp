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
//  VAutopageTexture::VAutopageTexture
//
//==========================================================================
VTexture *VAutopageTexture::Create (VStream &Strm, int LumpNum) {
  guard(VAutopageTexture::Create);
  if (Strm.TotalSize() < 320) return nullptr;
  return new VAutopageTexture(LumpNum);
  unguard;
}


//==========================================================================
//
//  VAutopageTexture::VAutopageTexture
//
//==========================================================================
VAutopageTexture::VAutopageTexture (int ALumpNum)
  : VTexture()
{
  SourceLump = ALumpNum;
  Name = W_LumpName(SourceLump);
  Width = 320;
  Height = W_LumpLength(SourceLump)/320;
  mFormat = TEXFMT_8;
}


//==========================================================================
//
//  VAutopageTexture::~VAutopageTexture
//
//==========================================================================
VAutopageTexture::~VAutopageTexture () {
  //guard(VAutopageTexture::~VAutopageTexture);
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
  //unguard;
}


//==========================================================================
//
//  VAutopageTexture::GetPixels
//
//==========================================================================
vuint8 *VAutopageTexture::GetPixels () {
  guard(VAutopageTexture::GetPixels);
  // if already got pixels, then just return them
  if (Pixels) return Pixels;

  // read data
  VStream *lumpstream = W_CreateLumpReaderNum(SourceLump);
  VCheckedStream Strm(lumpstream);
  int len = Strm.TotalSize();
  Pixels = new vuint8[len];
  vuint8 *dst = Pixels;
  for (int i = 0; i < len; ++i, ++dst) {
    Strm << *dst;
    if (!*dst) *dst = r_black_colour;
  }

  return Pixels;
  unguard;
}


//==========================================================================
//
//  VAutopageTexture::Unload
//
//==========================================================================
void VAutopageTexture::Unload () {
  guard(VAutopageTexture::Unload);
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
  unguard;
}
