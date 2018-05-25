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
#include "imago.h"


// ////////////////////////////////////////////////////////////////////////// //
//typedef VImage* (*VImageLoaderFn) (VStream *, const VStr &name);

struct ImagoLoader {
  VImageLoaderFn ldr;
  VStr ext;
  VStr desc;
  int prio;
  ImagoLoader *next;
};

static ImagoLoader *loaders = nullptr;


// `ext` may, or may not include dot
void ImagoRegisterLoader (const char *fmtext, const char *fmtdesc, VImageLoaderFn ldr, int prio) {
  VStr ext, desc;

  if (!ldr) return;

  if (fmtext && fmtext[0]) {
    if (fmtext[0] == '.' && fmtext[1] == 0) {
    } else {
      if (fmtext[0] == '.') ext = VStr(fmtext); else ext = VStr(".")+fmtext;
    }
  }

  desc = VStr(fmtdesc);
  ImagoLoader *prev = nullptr, *cur = loaders;
  while (cur && cur->prio >= prio) {
    prev = cur;
    cur = cur->next;
  }

  auto it = new ImagoLoader;
  it->ldr = ldr;
  it->ext = ext;
  it->desc = desc;
  it->prio = prio;
  it->next = cur;

  if (prev) prev->next = it; else loaders = it;
}


// ////////////////////////////////////////////////////////////////////////// //
VImage::VImage (ImageType atype, int awidth, int aheight) {
  if (awidth < 1 || aheight < 1) awidth = aheight = 0;

  mFormat = atype;
  mWidth = awidth;
  mHeight = aheight;
  mName = VStr();

  if (awidth && aheight) {
    mPixels = new vuint8[awidth*aheight*(atype == IT_RGBA ? 4 : 1)];
  } else {
    mPixels = nullptr;
  }
  memset(mPalette, 0, sizeof(mPalette));
  mPalUsed = 0;
}


VImage::~VImage () {
  delete mPixels;
  mPixels = nullptr;
}


vuint8 VImage::appendColor (const VImage::RGBA &col) {
  int bestn = 0, bestdist = 0x7fffffff;
  for (int f = 0; f < mPalUsed; ++f) {
    if (mPalette[f] == col) return (vuint8)f;
    int idst = mPalette[f].distSq(col);
    if (idst < bestdist) { bestn = f; bestdist = idst; }
  }
  if (mPalUsed < 256) {
    mPalette[mPalUsed++] = col;
    return (vuint8)(mPalUsed-1);
  }
  return (vuint8)bestn;
}


VImage::RGBA VImage::getPixel (int x, int y) const {
  if (x < 0 || y < 0 || x >= mWidth || y >= mHeight || !mPixels) return VImage::RGBA(0, 0, 0, 0);

  const vuint8 *data = mPixels;

  int pitch = 0;
  switch (mFormat) {
    case IT_Pal: pitch = 1; break;
    case IT_RGBA: pitch = 4; break;
    default: return VImage::RGBA(0, 0, 0, 0);
  }

  data += y*(mWidth*pitch)+x*pitch;
  switch (mFormat) {
    case IT_Pal: return mPalette[*data];
    case IT_RGBA: return *((const VImage::RGBA *)data);
  }

  return VImage::RGBA(0, 0, 0, 0);
}


void VImage::setPixel (int x, int y, const RGBA &col) {
  if (x < 0 || y < 0 || x >= mWidth || y >= mHeight || !mPixels) return;

  vuint8 *data = mPixels;

  int pitch = 0;
  switch (mFormat) {
    case IT_Pal: pitch = 1; break;
    case IT_RGBA: pitch = 4; break;
    default: return;
  }

  data += y*(mWidth*pitch)+x*pitch;
  switch (mFormat) {
    case IT_Pal: *data = appendColor(col); break;
    case IT_RGBA: *(VImage::RGBA *)data = col; break;
  }
}


void VImage::checkerFill () {
  if (!mPixels || width < 1 || height < 1) return;
  for (int y = 0; y < mHeight; ++y) {
    for (int x = 0; x < mWidth; ++x) {
      RGBA col = (((x/8)^(y/8))&1 ? RGBA(255, 255, 255, 255) : RGBA(0, 255, 255, 255));
      setPixel(x, y, col);
    }
  }
}


void VImage::setPalette (const RGBA *pal, int colnum) {
  memset(mPalette, 0, sizeof(mPalette));
  if (pal) {
    if (colnum > 256) colnum = 256;
    if (colnum > 0 && pal) memcpy(mPalette, pal, sizeof(RGBA)*colnum);
    mPalUsed = colnum;
  } else {
    mPalUsed = 0;
  }
}
