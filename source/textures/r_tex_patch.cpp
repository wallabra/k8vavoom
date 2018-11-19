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
//  VPatchTexture::Create
//
//==========================================================================
VTexture *VPatchTexture::Create (VStream &Strm, int LumpNum) {
  guard(VPatchTexture::Create);
  if (Strm.TotalSize() < 13) return nullptr; // lump is too small

  Strm.Seek(0);
  int Width = Streamer<vint16>(Strm);
  int Height = Streamer<vint16>(Strm);
  int SOffset = Streamer<vint16>(Strm);
  int TOffset = Streamer<vint16>(Strm);

  if (Width < 1 || Height < 1 || Width > 2048 || Height > 2048) return nullptr; // not valid dimensions
  if (Strm.TotalSize() < Width*4+8) return nullptr; // file has no space for offsets table

  vint32 *Offsets = new vint32[Width];
  for (int i = 0; i < Width; ++i) Strm << Offsets[i];
  // make sure that all offsets are valid and that at least one is right at the end of offsets table
  bool GapAtStart = true;
  for (int i = 0; i < Width; ++i) {
    if (Offsets[i] == Width*4+8) {
      GapAtStart = false;
    } else if (Offsets[i] < Width*4+8 || Offsets[i] >= Strm.TotalSize()) {
      delete[] Offsets;
      return nullptr;
    }
  }
  delete[] Offsets;
  if (GapAtStart) return nullptr;

  return new VPatchTexture(LumpNum, Width, Height, SOffset, TOffset);
  unguard;
}


//==========================================================================
//
//  VPatchTexture::VPatchTexture
//
//==========================================================================
VPatchTexture::VPatchTexture (int ALumpNum, int AWidth, int AHeight, int ASOffset, int ATOffset)
  : VTexture()
  , Pixels(nullptr)
{
  SourceLump = ALumpNum;
  Name = W_LumpName(SourceLump);
  Format = TEXFMT_8;
  Width = AWidth;
  Height = AHeight;
  SOffset = ASOffset;
  TOffset = ATOffset;
}


//==========================================================================
//
//  VPatchTexture::~VPatchTexture
//
//==========================================================================
VPatchTexture::~VPatchTexture () {
  //guard(VPatchTexture::~VPatchTexture);
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
  //unguard;
}


//==========================================================================
//
//  VPatchTexture::GetPixels
//
//==========================================================================
vuint8 *VPatchTexture::GetPixels () {
  guard(VPatchTexture::GetPixels);
  if (Pixels) return Pixels; // if already got pixels, then just return them

  // open stream
  VStream *Strm = W_CreateLumpReaderNum(SourceLump);

  // make sure header is present
  if (Strm->TotalSize() < 8) {
    GCon->Logf(NAME_Warning, "Patch \"%s\" is too small", *Name);
    Width = 1;
    Height = 1;
    SOffset = 0;
    TOffset = 0;
    Pixels = new vuint8[1];
    Pixels[0] = 0;
    return Pixels;
  }

  // read header
  Width = Streamer<vint16>(*Strm);
  Height = Streamer<vint16>(*Strm);
  SOffset = Streamer<vint16>(*Strm);
  TOffset = Streamer<vint16>(*Strm);

  // allocate image data
  Pixels = new vuint8[Width*Height];
  memset(Pixels, 0, Width*Height);

  // make sure all column offsets are there
  if (Strm->TotalSize() < 8+Width*4) {
    GCon->Logf(NAME_Warning, "Patch \"%s\" is too small", *Name);
    checkerFill8(Pixels, Width, Height);
    return Pixels;
  }

  // read data
  for (int x = 0; x < Width; ++x) {
    // get offset of the column
    Strm->Seek(8+x*4);
    vint32 Offset = Streamer<vint32>(*Strm);
    if (Offset < 8+Width*4 || Offset > Strm->TotalSize()-1) {
      GCon->Logf(NAME_Warning, "Bad offset in patch \"%s\"", *Name);
      //checkerFillColumn8(Pixels+x, x, Width, Height);
      checkerFill8(Pixels, Width, Height);
      continue;
    }
    Strm->Seek(Offset);

    // step through the posts in a column
    int top = -1; // DeepSea tall patches support
    vuint8 TopDelta;
    *Strm << TopDelta;
    while (TopDelta != 0xff) {
      // make sure length is there
      if (Strm->TotalSize()-Strm->Tell() < 2) {
        GCon->Logf(NAME_Warning, "Broken column in patch \"%s\"", *Name);
        //checkerFillColumn8(Pixels+x, x, Width, Height);
        checkerFill8(Pixels, Width, Height);
        x = Width;
        break;
      }

      // calculate top offset
      if (TopDelta <= top) top += TopDelta; else top = TopDelta;

      // read column length and skip unused byte
      vuint8 Len;
      *Strm << Len;
      Streamer<vuint8>(*Strm);

      // make sure column doesn't go out of the bounds of the image
      if (top+Len > Height) {
        GCon->Logf(NAME_Warning, "Column too long in patch \"%s\"", *Name);
        //checkerFillColumn8(Pixels+x, x, Width, Height);
        checkerFill8(Pixels, Width, Height);
        x = Width;
        break;
      }

      // make sure all post data is there
      if (Strm->TotalSize()-Strm->Tell() < Len) {
        GCon->Logf(NAME_Warning, "Broken column in patch \"%s\"", *Name);
        //checkerFillColumn8(Pixels+x, x, Width, Height);
        checkerFill8(Pixels, Width, Height);
        x = Width;
        break;
      }

      // read post, convert colour 0 to black if needed
      int count = Len;
      vuint8 *dest = Pixels+top*Width+x;
      while (count--) {
        *Strm << *dest;
        if (!*dest && !bNoRemap0) *dest = r_black_colour;
        dest += Width;
      }

      // make sure unused byte and next post's top offset is there
      if (Strm->TotalSize()-Strm->Tell() <21) {
        GCon->Logf(NAME_Warning, "Broken column in patch \"%s\"", *Name);
        //checkerFillColumn8(Pixels+x, x, Width, Height);
        checkerFill8(Pixels, Width, Height);
        x = Width;
        break;
      }

      // skip unused byte and get top offset of the next post
      Streamer<vuint8>(*Strm);
      *Strm << TopDelta;
    }
  }

  // close stream
  delete Strm;

  return Pixels;
  unguard;
}


//==========================================================================
//
//  VPatchTexture::Unload
//
//==========================================================================
void VPatchTexture::Unload () {
  guard(VPatchTexture::Unload);
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
  unguard;
}
