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

#if defined(CLIENT) || !defined(SERVER)
# ifdef VAVOOM_USE_LIBPNG
#  include <png.h>
# endif
#endif

#include "gamedefs.h"
#include "r_tex.h"


//  This one is missing in older versions of libpng
#ifdef VAVOOM_USE_LIBPNG
# ifndef png_jmpbuf
#  define png_jmpbuf(png_ptr)   ((png_ptr)->jmpbuf)
# endif
#endif


//==========================================================================
//
//  VPngTexture::Create
//
//==========================================================================
VTexture *VPngTexture::Create (VStream &Strm, int LumpNum) {
  guard(VPngTexture::Create);

  if (Strm.TotalSize() < 29) return nullptr; // file is too small

  vuint8 Id[8];

  // verify signature
  Strm.Seek(0);
  Strm.Serialise(Id, 8);
  if (Id[0] != 137 || Id[1] != 'P' || Id[2] != 'N' || Id[3] != 'G' ||
      Id[4] != 13 || Id[5] != 10 || Id[6] != 26 || Id[7] != 10)
  {
    // not a PNG file
    return nullptr;
  }

  // make sure it's followed by an image header
  Strm.Serialise(Id, 8);
  if (Id[0] != 0 || Id[1] != 0 || Id[2] != 0 || Id[3] != 13 ||
      Id[4] != 'I' || Id[5] != 'H' || Id[6] != 'D' || Id[7] != 'R')
  {
    // assume it's a corupted file
    return nullptr;
  }

  // read image info
  vint32 Width;
  vint32 Height;
  vuint8 BitDepth;
  vuint8 ColourType;
  vuint8 Compression;
  vuint8 Filter;
  vuint8 Interlace;
  vuint32 CRC;
  Strm.SerialiseBigEndian(&Width, 4);
  Strm.SerialiseBigEndian(&Height, 4);
  Strm << BitDepth << ColourType << Compression << Filter << Interlace;
  Strm << CRC;

  // scan other chunks looking for grAb chunk with offsets
  vint32 SOffset = 0;
  vint32 TOffset = 0;
  while (Strm.TotalSize()-Strm.Tell() >= 12) {
    vuint32 Len;
    Strm.SerialiseBigEndian(&Len, 4);
    Strm.Serialise(Id, 4);
    if (Id[0] == 'g' && Id[1] == 'r' && Id[2] == 'A' && Id[3] == 'b') {
      Strm.SerialiseBigEndian(&SOffset, 4);
      Strm.SerialiseBigEndian(&TOffset, 4);
      if (SOffset < -32768 || SOffset > 32767) {
        GCon->Logf("S-offset for PNG texture %s is bad: %d (0x%08x)", *W_LumpName(LumpNum), SOffset, SOffset);
        SOffset = 0;
      }
      if (TOffset < -32768 || TOffset > 32767) {
        GCon->Logf("T-offset for PNG texture %s is bad: %d (0x%08x)", *W_LumpName(LumpNum), TOffset, TOffset);
        TOffset = 0;
      }
    } else {
      Strm.Seek(Strm.Tell()+Len);
    }
    Strm << CRC;
  }

  return new VPngTexture(LumpNum, Width, Height, SOffset, TOffset);
  unguard;
}


//==========================================================================
//
//  VPngTexture::VPngTexture
//
//==========================================================================
VPngTexture::VPngTexture (int ALumpNum, int AWidth, int AHeight, int ASOffset, int ATOffset)
  : VTexture()
  , Pixels(nullptr)
{
  SourceLump = ALumpNum;
  Name = W_LumpName(SourceLump);
  Width = AWidth;
  Height = AHeight;
  SOffset = ASOffset;
  TOffset = ATOffset;
}


//==========================================================================
//
//  VPngTexture::~VPngTexture
//
//==========================================================================
VPngTexture::~VPngTexture () {
  //guard(VPngTexture::~VPngTexture);
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
  //unguard;
}


#ifdef VAVOOM_USE_LIBPNG
//==========================================================================
//
//  ReadFunc
//
//==========================================================================
#ifdef CLIENT
static void ReadFunc (png_structp png, png_bytep data, png_size_t len) {
  guard(ReadFunc);
  VStream *Strm = (VStream *)png_get_io_ptr(png);
  Strm->Serialise(data, len);
  unguard;
}
#endif
#endif


//==========================================================================
//
//  VPngTexture::GetPixels
//
//==========================================================================
vuint8 *VPngTexture::GetPixels () {
  guard(VPngTexture::GetPixels);

#ifdef CLIENT
  // if we already have loaded pixels, return them
  if (Pixels) return Pixels;

#ifdef VAVOOM_USE_LIBPNG
  // create reading structure
  png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png_ptr) Sys_Error("Couldn't create png_ptr");

  // create info structure
  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) Sys_Error("Couldn't create info_ptr");

  // create end info structure
  png_infop end_info = png_create_info_struct(png_ptr);
  if (!end_info) Sys_Error("Couldn't create end_info");

  // set up error handling
  if (setjmp(png_jmpbuf(png_ptr))) Sys_Error("Error reading PNG file");

  // open stream
  VStream *Strm = W_CreateLumpReaderNum(SourceLump);

  // verify signature
  png_byte Signature[8];
  Strm->Seek(0);
  Strm->Serialise(Signature, 8);
  if (png_sig_cmp(Signature, 0, 8)) Sys_Error("%s is not a valid PNG file", *Name);

  // set my read function
  Strm->Seek(0);
  png_set_read_fn(png_ptr, Strm, ReadFunc);

  // read image info
  png_read_info(png_ptr, info_ptr);
  Width = png_get_image_width(png_ptr, info_ptr);
  Height = png_get_image_height(png_ptr, info_ptr);
  int BitDepth = png_get_bit_depth(png_ptr, info_ptr);
  int ColourType = png_get_color_type(png_ptr, info_ptr);

  // set up transformations
  if (ColourType == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_ptr);
  if (ColourType == PNG_COLOR_TYPE_GRAY && BitDepth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png_ptr);
  if (BitDepth == 16) png_set_strip_16(png_ptr);
  if (ColourType == PNG_COLOR_TYPE_PALETTE ||
      ColourType == PNG_COLOR_TYPE_RGB ||
      ColourType == PNG_COLOR_TYPE_GRAY)
  {
    png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
  }
  if (ColourType == PNG_COLOR_TYPE_GRAY || ColourType == PNG_COLOR_TYPE_GRAY_ALPHA) {
    png_set_gray_to_rgb(png_ptr);
  }

  // set up unpacking buffer and row pointers
  Format = TEXFMT_RGBA;
  Pixels = new vuint8[Width*Height*4];
  png_bytep *RowPtrs = new png_bytep[Height];
  for (int i = 0; i < Height; i++) RowPtrs[i] = Pixels+i*Width*4;
  png_read_image(png_ptr, RowPtrs);

  // finish reading
  png_read_end(png_ptr, end_info);
  png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

  delete[] RowPtrs;
  RowPtrs = nullptr;

#else // VAVOOM_USE_LIBPNG

  // open stream
  VStream *Strm = W_CreateLumpReaderNum(SourceLump);
  if (!Strm) Sys_Error("Can't open PNG file '%s'", *Name);

  PNGHandle *png = M_VerifyPNG(Strm);
  if (!png) Sys_Error("'%s' is not a valid PNG file", *Name);

  if (!png->loadIDAT()) Sys_Error("Error reading PNG file '%s'", *Name);

  if (r_showinfo) {
    GCon->Logf("PNG '%s': %dx%d (bitdepth:%u; colortype:%u; interlace:%u)", *Name, png->width, png->height, png->bitdepth, png->colortype, png->interlace);
  }

  Width = png->width;
  Height = png->height;
  Format = TEXFMT_RGBA;
  Pixels = new vuint8[Width*Height*4];

  vuint8 *dest = Pixels;
  for (int y = 0; y < png->height; ++y) {
    for (int x = 0; x < png->width; ++x) {
      auto clr = png->getPixel(x, y); // unmultiplied
      *dest++ = clr.r;
      *dest++ = clr.g;
      *dest++ = clr.b;
      *dest++ = clr.a;
    }
  }

#endif

  // free memory
  delete Strm;
  //Strm = nullptr;
  return Pixels;

#else
  Sys_Error("ReadPixels on dedicated server");
  return nullptr;
#endif

  unguard;
}


//==========================================================================
//
//  VPngTexture::Unload
//
//==========================================================================
void VPngTexture::Unload () {
  guard(VPngTexture::Unload);
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
  unguard;
}


//==========================================================================
//
//  WritePNG
//
//==========================================================================
#ifdef CLIENT
void WritePNG(const VStr &FileName, const void *Data, int Width, int Height, int Bpp, bool Bot2top) {
  guard(WritePNG);

#if 0
  VStream *Strm = FL_OpenFileWrite(FileName, true);
  if (!Strm) { GCon->Log("Couldn't write png"); return; }

  // create writing structure
  png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png_ptr) Sys_Error("Couldn't create png_ptr");

  // create info structure
  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) Sys_Error("Couldn't create info_ptr");

  // set up error handling
  if (setjmp(png_jmpbuf(png_ptr))) Sys_Error("Error writing PNG file");

  // set my read function
  png_set_write_fn(png_ptr, Strm, ReadFunc, nullptr);

  png_set_IHDR(png_ptr, info_ptr, Width, Height, 8,
    Bpp == 8 ? PNG_COLOR_TYPE_PALETTE : PNG_COLOR_TYPE_RGB,
    PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
  if (Bpp == 8) png_set_PLTE(png_ptr, info_ptr, (png_colorp)r_palette, 256);
  png_write_info(png_ptr, info_ptr);

  TArray<png_bytep> RowPointers;
  RowPointers.SetNum(Height);
  for (int i = 0; i < Height; ++i) {
    RowPointers[i] = ((byte *)Data)+(Bot2top ? Height-i-1 : i)*Width*(Bpp/8);
  }
  png_write_image(png_ptr, RowPointers.Ptr());

  png_write_end(png_ptr, nullptr);
  png_destroy_write_struct(&png_ptr, &info_ptr);

  Strm->Close();
  delete Strm;
  //Strm = nullptr;

#else

  ESSType ptp = SS_RGB;
  switch (Bpp) {
    case 8: ptp = SS_PAL; break;
    case 24: ptp = SS_RGB; break;
    case 32: ptp = SS_RGBA; break;
    default: GCon->Log("Couldn't write png (invalid bpp)"); return;
  }

  VStream *Strm = FL_OpenFileWrite(FileName, true);
  if (!Strm) { GCon->Log("Couldn't write png"); return; }

  PalEntry pal[256];
  for (int f = 0; f < 256; ++f) {
    pal[f].r = r_palette[f].r;
    pal[f].g = r_palette[f].g;
    pal[f].b = r_palette[f].b;
    pal[f].a = 255;
  }

  if (!M_CreatePNG(Strm, (const vuint8 *)Data, pal, ptp, Width, (Bot2top ? -Height : Height), Width*(Bpp/8), 1.0f)) {
    GCon->Log("Error writing png");
  }

  Strm->Close();
  delete Strm;

#endif

  unguard;
}
#endif
