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
#include "../gamedefs.h"
#include "r_tex.h"


//==========================================================================
//
//  VCameraTexture::VCameraTexture
//
//==========================================================================
VCameraTexture::VCameraTexture (VName AName, int AWidth, int AHeight)
  : VTexture()
  , bUsedInFrame(false)
  , NextUpdateTime(0)
  , bUpdated(false)
  , camfboidx(-1)
  , bPixelsLoaded(false)
{
  Name = AName;
  Type = TEXTYPE_Wall;
  mFormat = mOrigFormat = TEXFMT_RGBA;
  Width = AWidth;
  Height = AHeight;
  bIsCameraTexture = true;
  needFBO = true;
  transFlags = TransValueSolid; // anyway
  Pixels8BitValid = false;
  Pixels8BitAValid = false;
  //vassert(!Pixels);
}


//==========================================================================
//
//  VCameraTexture::~VCameraTexture
//
//==========================================================================
VCameraTexture::~VCameraTexture () {
  camfboidx = -1;
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
}


//==========================================================================
//
//  VCameraTexture::IsDynamicTexture
//
//==========================================================================
bool VCameraTexture::IsDynamicTexture () const noexcept {
  return true;
}


//==========================================================================
//
//  VCameraTexture::IsHugeTexture
//
//==========================================================================
bool VCameraTexture::IsHugeTexture () const noexcept {
  return false; // anyway
}


//==========================================================================
//
//  VCameraTexture::CheckModified
//
//==========================================================================
bool VCameraTexture::CheckModified () {
  if (bUpdated) {
    bUpdated = false;
    return true;
  }
  return false;
}


//==========================================================================
//
//  VCameraTexture::NeedUpdate
//
//==========================================================================
bool VCameraTexture::NeedUpdate () {
  if (!bUsedInFrame && NextUpdateTime != 0.0f) return false;
  // set update time here, why not?
  double ctime = Sys_Time();
  if (NextUpdateTime > ctime) return false;
  unsigned rndmsecs = (GenRandomU31()&0x1f);
  NextUpdateTime = ctime+(1.0/17.5)+((double)rndmsecs/1000.0);
  return true;
}


//==========================================================================
//
//  VCameraTexture::GetPixels
//
//==========================================================================
vuint8 *VCameraTexture::GetPixels () {
  //bUsedInFrame = true;
  transFlags = TransValueSolid; // anyway
  Pixels8BitValid = false;
  Pixels8BitAValid = false;
  // if already got pixels, then just return them
  if (Pixels) return Pixels;

  // allocate image data
  Pixels = new vuint8[Width*Height*4];

  rgba_t *pDst = (rgba_t *)Pixels;
  for (int i = 0; i < Height; ++i) {
    for (int j = 0; j < Width; ++j) {
      if (j < Width/2) {
        pDst->r = 0;
        pDst->g = 0;
        pDst->b = 0;
        pDst->a = 255;
      } else {
        pDst->r = 255;
        pDst->g = 255;
        pDst->b = 255;
        pDst->a = 255;
      }
      ++pDst;
    }
  }

  return Pixels;
}


#ifdef CLIENT
//==========================================================================
//
//  VCameraTexture::CopyImage
//
//==========================================================================
void VCameraTexture::CopyImage () {
  if (!Pixels) {
    //Pixels = new vuint8[Width*Height*4];
    (void)GetPixels();
  }
  transFlags = TransValueSolid; // anyway
  /* it will be done in texture selection code
  if (gl_camera_texture_use_readpixels) {
    Drawer->ReadBackScreen(Width, Height, (rgba_t *)Pixels);
    bPixelsLoaded = true;
  } else {
    bPixelsLoaded = false;
  }
  */
  bPixelsLoaded = false;
  bUpdated = true;
  Pixels8BitValid = false;
  Pixels8BitAValid = false;
  bUsedInFrame = false;
  /*
  double ctime = Sys_Time();
  unsigned rndmsecs = (GenRandomU31()&0x1f);
  NextUpdateTime = ctime+(1.0/17.5)+((double)rndmsecs/1000.0);
  */
}
#endif


//==========================================================================
//
//  VCameraTexture::GetHighResolutionTexture
//
//==========================================================================
VTexture *VCameraTexture::GetHighResolutionTexture () {
  return nullptr;
}


//==========================================================================
//
//  VCameraTexture::ReleasePixels
//
//==========================================================================
void VCameraTexture::ReleasePixels () {
  //VTexture::ReleasePixels();
  //NextUpdateTime = 0;
  // do nothing here
}
