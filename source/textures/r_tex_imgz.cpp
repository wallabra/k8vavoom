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
//  VImgzTexture::Create
//
//==========================================================================
VTexture *VImgzTexture::Create (VStream &Strm, int LumpNum) {
  if (Strm.TotalSize() < 24) return nullptr; // not enough space for IMGZ header

  vuint8 Id[4];
  vuint16 Width;
  vuint16 Height;
  vuint16 SOffset;
  vuint16 TOffset;

  Strm.Seek(0);
  Strm.Serialise(Id, 4);
  if (Id[0] != 'I' || Id[1] != 'M' || Id[2] != 'G' || Id[3] == 'Z') return nullptr;

  Strm << Width << Height << SOffset << TOffset;
  return new VImgzTexture(LumpNum, Width, Height, SOffset, TOffset);
}


//==========================================================================
//
//  VImgzTexture::VImgzTexture
//
//==========================================================================
VImgzTexture::VImgzTexture (int ALumpNum, int AWidth, int AHeight, int ASOffset, int ATOffset)
  : VTexture()
{
  SourceLump = ALumpNum;
  Name = W_LumpName(SourceLump);
  mFormat = TEXFMT_8;
  Width = AWidth;
  Height = AHeight;
  SOffset = ASOffset;
  TOffset = ATOffset;
}


//==========================================================================
//
//  VImgzTexture::~VImgzTexture
//
//==========================================================================
VImgzTexture::~VImgzTexture () {
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
}


//==========================================================================
//
//  VImgzTexture::GetPixels
//
//==========================================================================
vuint8 *VImgzTexture::GetPixels () {
  // if already got pixels, then just return them
  if (Pixels) return Pixels;

  VStream *lumpstream = W_CreateLumpReaderNum(SourceLump);
  VCheckedStream Strm(lumpstream);

  // read header
  Strm.Seek(4); // skip magic
  Width = Streamer<vuint16>(Strm);
  Height = Streamer<vuint16>(Strm);
  SOffset = Streamer<vint16>(Strm);
  TOffset = Streamer<vint16>(Strm);
  vuint8 Compression = Streamer<vuint8>(Strm);
  Strm.Seek(24); // skip reserved space

  // read data
  Pixels = new vuint8[Width*Height];
  memset(Pixels, 0, Width*Height);
  if (!Compression) {
    Strm.Serialise(Pixels, Width*Height);
  } else {
    // IMGZ compression is the same RLE used by IFF ILBM files
    vuint8 *pDst = Pixels;
    int runlen = 0, setlen = 0;
    vuint8 setval = 0; // shut up, GCC

    for (int y = Height; y != 0; --y) {
      for (int x = Width; x != 0; ) {
        if (runlen != 0) {
          Strm << *pDst;
          ++pDst;
          --x;
          --runlen;
        } else if (setlen != 0) {
          *pDst = setval;
          ++pDst;
          --x;
          --setlen;
        } else {
          vint8 code;
          Strm << code;
          if (code >= 0) {
            runlen = code+1;
          } else if (code != -128) {
            setlen = (-code)+1;
            Strm << setval;
          }
        }
      }
    }
  }

  ConvertPixelsToShaded();
  return Pixels;
}


//==========================================================================
//
//  VImgzTexture::Unload
//
//==========================================================================
void VImgzTexture::Unload () {
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
}
