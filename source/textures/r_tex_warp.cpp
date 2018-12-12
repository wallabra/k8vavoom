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
//  VWarpTexture::VWarpTexture
//
//==========================================================================
VWarpTexture::VWarpTexture (VTexture *ASrcTex, float aspeed)
  : VTexture()
  , SrcTex(ASrcTex)
  , GenTime(0)
  , Speed(aspeed)
  , WarpXScale(1.0)
  , WarpYScale(1.0)
  , XSin1(nullptr)
  , XSin2(nullptr)
  , YSin1(nullptr)
  , YSin2(nullptr)
{
  Width = SrcTex->GetWidth();
  Height = SrcTex->GetHeight();
  SOffset = SrcTex->SOffset;
  TOffset = SrcTex->TOffset;
  SScale = SrcTex->SScale;
  TScale = SrcTex->TScale;
  WarpType = 1;
  if (Speed < 1) Speed = 1; else if (Speed > 16) Speed = 16;
  mFormat = (SrcTex ? SrcTex->Format : TEXFMT_RGBA);
}


//==========================================================================
//
//  VWarpTexture::~VWarpTexture
//
//==========================================================================
VWarpTexture::~VWarpTexture () {
  //guard(VWarpTexture::~VWarpTexture);
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
  if (SrcTex) {
    delete SrcTex;
    SrcTex = nullptr;
  }
  if (XSin1) {
    delete[] XSin1;
    XSin1 = nullptr;
  }
  if (XSin2) {
    delete[] XSin2;
    XSin2 = nullptr;
  }
  if (YSin1) {
    delete[] YSin1;
    YSin1 = nullptr;
  }
  if (YSin2) {
    delete[] YSin2;
    YSin2 = nullptr;
  }
  //unguard;
}


//==========================================================================
//
//  VWarpTexture::SetFrontSkyLayer
//
//==========================================================================
void VWarpTexture::SetFrontSkyLayer () {
  guardSlow(VWarpTexture::SetFrontSkyLayer);
  SrcTex->SetFrontSkyLayer();
  unguardSlow;
}


//==========================================================================
//
//  VWarpTexture::CheckModified
//
//==========================================================================
bool VWarpTexture::CheckModified () {
  return (GenTime != GTextureManager.Time*Speed);
}


//==========================================================================
//
//  VWarpTexture::GetPixels
//
//==========================================================================
vuint8 *VWarpTexture::GetPixels () {
  guard(VWarpTexture::GetPixels);
  if (Pixels && GenTime == GTextureManager.Time*Speed) return Pixels;

  const vuint8 *SrcPixels = SrcTex->GetPixels();
  mFormat = SrcTex->Format;

  GenTime = GTextureManager.Time*Speed;
  Pixels8BitValid = false;
  Pixels8BitAValid = false;

  if (!XSin1) {
    XSin1 = new float[Width];
    YSin1 = new float[Height];
  }

  // precalculate sine values
  for (int x = 0; x < Width; ++x) {
    XSin1[x] = msin(GenTime*44+x/WarpXScale*5.625+95.625)*8*WarpYScale+8*WarpYScale*Height;
  }
  for (int y = 0; y < Height; ++y) {
    YSin1[y] = msin(GenTime*50+y/WarpYScale*5.625)*8*WarpXScale+8*WarpXScale*Width;
  }

  if (mFormat == TEXFMT_8 || mFormat == TEXFMT_8Pal) {
    if (!Pixels) Pixels = new vuint8[Width*Height];
    vuint8 *Dst = Pixels;
    for (int y = 0; y < Height; ++y) {
      for (int x = 0; x < Width; ++x) {
        *Dst++ = SrcPixels[(((int)YSin1[y]+x)%Width)+(((int)XSin1[x]+y)%Height)*Width];
      }
    }
  } else {
    if (!Pixels) Pixels = new vuint8[Width*Height*4];
    vuint32 *Dst = (vuint32*)Pixels;
    for (int y = 0; y < Height; ++y) {
      for (int x = 0; x < Width; ++x) {
        *Dst++ = ((vuint32*)SrcPixels)[(((int)YSin1[y]+x)%Width)+(((int)XSin1[x]+y)%Height)*Width];
      }
    }
  }

  return Pixels;
  unguard;
}


//==========================================================================
//
//  VWarpTexture::GetPalette
//
//==========================================================================
rgba_t *VWarpTexture::GetPalette () {
  guard(VWarpTexture::GetPalette);
  return SrcTex->GetPalette();
  unguard;
}


//==========================================================================
//
//  VWarpTexture::GetHighResolutionTexture
//
//==========================================================================
VTexture *VWarpTexture::GetHighResolutionTexture () {
  guard(VWarpTexture::GetHighResolutionTexture);
  if (!r_hirestex) return nullptr;
  // if high resolution texture is already created, then just return it
  if (HiResTexture) return HiResTexture;

  VTexture *SrcTex = VTexture::GetHighResolutionTexture();
  if (!SrcTex) return nullptr;

  VWarpTexture *NewTex;
  if (WarpType == 1) NewTex = new VWarpTexture(SrcTex); else NewTex = new VWarp2Texture(SrcTex);
  NewTex->Name = Name;
  NewTex->Type = Type;
  NewTex->WarpXScale = NewTex->GetWidth()/GetWidth();
  NewTex->WarpYScale = NewTex->GetHeight()/GetHeight();
  HiResTexture = NewTex;
  return HiResTexture;
  unguard;
}


//==========================================================================
//
//  VWarpTexture::Unload
//
//==========================================================================
void VWarpTexture::Unload () {
  guard(VWarpTexture::Unload);
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
  SrcTex->Unload();
  unguard;
}


//==========================================================================
//
//  VWarp2Texture::VWarp2Texture
//
//==========================================================================
VWarp2Texture::VWarp2Texture (VTexture *ASrcTex, float aspeed)
  : VWarpTexture(ASrcTex, aspeed)
{
  WarpType = 2;
}


//==========================================================================
//
//  VWarp2Texture::GetPixels
//
//==========================================================================
vuint8 *VWarp2Texture::GetPixels () {
  guard(VWarp2Texture::GetPixels);
  if (Pixels && GenTime == GTextureManager.Time*Speed) return Pixels;

  const vuint8 *SrcPixels = SrcTex->GetPixels();
  mFormat = SrcTex->Format;

  GenTime = GTextureManager.Time*Speed;
  Pixels8BitValid = false;
  Pixels8BitAValid = false;

  if (!XSin1) {
    XSin1 = new float[Height];
    XSin2 = new float[Width];
    YSin1 = new float[Height];
    YSin2 = new float[Width];
  }

  // precalculate sine values
  for (int y = 0; y < Height; ++y) {
    XSin1[y] = msin(y/WarpYScale*5.625+GenTime*313.895+39.55)*2*WarpXScale;
    YSin1[y] = y+(2*Height+msin(y/WarpYScale*5.625+GenTime*118.337+30.76)*2)*WarpYScale;
  }
  for (int x = 0; x < Width; ++x) {
    XSin2[x] = x+(2*Width+msin(x/WarpXScale*11.25+GenTime*251.116+13.18)*2)*WarpXScale;
    YSin2[x] = msin(x/WarpXScale*11.25+GenTime*251.116+52.73)*2*WarpYScale;
  }

  if (mFormat == TEXFMT_8 || mFormat == TEXFMT_8Pal) {
    if (!Pixels) Pixels = new vuint8[Width*Height];
    vuint8 *dest = Pixels;
    for (int y = 0; y < Height; ++y) {
      for (int x = 0; x < Width; ++x) {
        *dest++ = SrcPixels[((int)(XSin1[y]+XSin2[x])%Width)+((int)(YSin1[y]+YSin2[x])%Height)*Width];
      }
    }
  } else {
    if (!Pixels) Pixels = new vuint8[Width*Height*4];
    vuint32 *dest = (vuint32*)Pixels;
    for (int y = 0; y < Height; ++y) {
      for (int x = 0; x < Width; ++x) {
        int Idx = ((int)(XSin1[y]+XSin2[x])%Width)*4+((int)(YSin1[y]+YSin2[x])%Height)*Width*4;
        *dest++ = *(vuint32*)(SrcPixels+Idx);
      }
    }
  }

  return Pixels;
  unguard;
}
