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


struct pcx_t {
  vint8 manufacturer;
  vint8 version;
  vint8 encoding;
  vint8 bits_per_pixel;

  vuint16 xmin;
  vuint16 ymin;
  vuint16 xmax;
  vuint16 ymax;

  vuint16 hres;
  vuint16 vres;

  vuint8 palette[48];

  vint8 reserved;
  vint8 colour_planes;
  vuint16 bytes_per_line;
  vuint16 palette_type;

  vuint16 horz_screen_size;
  vuint16 vert_screen_size;

  vint8 filler[54];

  friend VStream &operator << (VStream &Strm, pcx_t &h) {
    Strm << h.manufacturer << h.version << h.encoding << h.bits_per_pixel
      << h.xmin << h.ymin << h.xmax << h.ymax << h.hres << h.vres;
    Strm.Serialise(h.palette, 48);
    Strm << h.reserved << h.colour_planes << h.bytes_per_line
      << h.palette_type << h.horz_screen_size << h.vert_screen_size;
    Strm.Serialise(h.filler, 54);
    return Strm;
  }
};


//==========================================================================
//
//  VPcxTexture::Create
//
//==========================================================================
VTexture *VPcxTexture::Create (VStream &Strm, int LumpNum) {
  if (Strm.TotalSize() < 128) return nullptr; // file is too small

  pcx_t Hdr;
  Strm.Seek(0);
  Strm << Hdr;

  if (Hdr.manufacturer != 0x0a || Hdr.encoding != 1 ||
      Hdr.version == 1 || Hdr.version > 5 || Hdr.reserved != 0 ||
      (/*Hdr.bits_per_pixel != 1 &&*/ Hdr.bits_per_pixel != 8) ||
      //(Hdr.bits_per_pixel == 1 && Hdr.colour_planes != 1 && Hdr.colour_planes != 4) ||
      (Hdr.bits_per_pixel == 8 && (Hdr.colour_planes != 1 || Hdr.bytes_per_line != Hdr.xmax-Hdr.xmin+1)) ||
      (Hdr.palette_type != 1 && Hdr.palette_type != 2))
  {
    return nullptr;
  }

  /*k8: filler *might* be zero-filled, but it is not required by any spec
  for (int i = 0; i < 54; ++i) {
    if (Hdr.filler[i] != 0) return nullptr;
  }
  */

  return new VPcxTexture(LumpNum, Hdr);
}


//==========================================================================
//
//  VPcxTexture::VPcxTexture
//
//==========================================================================
VPcxTexture::VPcxTexture (int ALumpNum, pcx_t &Hdr)
  : VTexture()
  , Palette(nullptr)
{
  SourceLump = ALumpNum;
  Name = W_LumpName(SourceLump);
  Width = Hdr.xmax-Hdr.xmin+1;
  Height = Hdr.ymax-Hdr.ymin+1;
  mFormat = TEXFMT_8Pal;
}


//==========================================================================
//
//  VPcxTexture::~VPcxTexture
//
//==========================================================================
VPcxTexture::~VPcxTexture () {
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
  if (Palette) {
    delete[] Palette;
    Palette = nullptr;
  }
}


//==========================================================================
//
//  VPcxTexture::GetPixels
//
//==========================================================================
vuint8 *VPcxTexture::GetPixels () {
  int c;
  int bytes_per_line;
  vint8 ch;

  // if we already have loaded pixels, return them
  if (Pixels) return Pixels;
  transparent = false;

  // open stream
  VStream *lumpstream = W_CreateLumpReaderNum(SourceLump);
  VCheckedStream Strm(lumpstream);

  // read header
  pcx_t pcx;
  Strm << pcx;

  // we only support 8-bit pcx files
  if (pcx.bits_per_pixel != 8) Sys_Error("No 8-bit planes\n"); // we like 8 bit colour planes
  if (pcx.colour_planes != 1) Sys_Error("Not 8 bpp\n");

  Width = pcx.xmax-pcx.xmin+1;
  Height = pcx.ymax-pcx.ymin+1;
  mFormat = TEXFMT_8Pal;

  bytes_per_line = pcx.bytes_per_line;

  Pixels = new vuint8[Width*Height];

  for (int y = 0; y < Height; ++y) {
    // decompress RLE encoded PCX data
    int x = 0;

    while (x < bytes_per_line) {
      Strm << ch;
      if ((ch&0xC0) == 0xC0) {
        c = (ch&0x3F);
        Strm << ch;
      } else {
        c = 1;
      }

      while (c--) {
        if (x < Width) Pixels[y*Width+x] = ch;
        ++x;
      }
    }
  }

  // if not followed by palette ID, assume palette is at the end of file
  Strm << ch;
  if (ch != 12) Strm.Seek(Strm.TotalSize()-768);

  // read palette
  Palette = new rgba_t[256];
  for (c = 0; c < 256; ++c) {
    Strm << Palette[c].r << Palette[c].g << Palette[c].b;
    Palette[c].a = 255;
  }

  FixupPalette(Palette);

  if (Width > 0 && Height > 0) {
    const vuint8 *s = Pixels;
    for (int count = Width*Height; count--; ++s) {
      if (s[0] == 0) { transparent = true; break; }
    }
  }

  ConvertPixelsToShaded();
  return Pixels;
}


//==========================================================================
//
//  VPcxTexture::GetPalette
//
//==========================================================================
rgba_t *VPcxTexture::GetPalette () {
  return Palette;
}


//==========================================================================
//
//  VPcxTexture::Unload
//
//==========================================================================
void VPcxTexture::Unload () {
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
  if (Palette) {
    delete[] Palette;
    Palette = nullptr;
  }
}


#ifdef CLIENT
//==========================================================================
//
//  WritePCX
//
//==========================================================================
void WritePCX (const VStr &FileName, void *data, int width, int height, int bpp, bool bot2top) {
  VStream *Strm = FL_OpenFileWrite(FileName, true);
  if (!Strm) { GCon->Log("Couldn't write pcx"); return; }

  pcx_t pcx;
  pcx.manufacturer = 0x0a; // PCX id
  pcx.version = 5; // 256 colour
  pcx.encoding = 1; // uncompressed
  pcx.bits_per_pixel = 8; // 256 colour
  pcx.xmin = 0;
  pcx.ymin = 0;
  pcx.xmax = width-1;
  pcx.ymax = height-1;
  pcx.hres = width;
  pcx.vres = height;
  memset(pcx.palette, 0, sizeof(pcx.palette));
  pcx.colour_planes = (bpp == 8 ? 1 : 3);
  pcx.bytes_per_line = width;
  pcx.palette_type = 1; // not a grey scale
  pcx.horz_screen_size = 0;
  pcx.vert_screen_size = 0;
  memset(pcx.filler, 0, sizeof(pcx.filler));
  *Strm << pcx;

  // pack the image
  if (bpp == 8) {
    for (int j = 0; j < height; ++j) {
      vuint8 *src = (vuint8 *)data+j*width;
      for (int i = 0; i < width; ++i) {
        if ((src[i]&0xc0) == 0xc0) {
          vuint8 tmp = 0xc1;
          *Strm << tmp;
        }
        *Strm << src[i];
      }
    }

    // write the palette
    vuint8 PalId = 0x0c; // palette ID byte
    *Strm << PalId;
    for (int i = 0; i < 256; ++i) {
      *Strm << r_palette[i].r << r_palette[i].g << r_palette[i].b;
    }
  } else if (bpp == 24 || bpp == 32) {
    for (int j = 0; j < height; ++j) {
      const vuint8 *srcb = (const vuint8 *)data+(bot2top ? height-j-1 : j)*(width*(bpp/8));
      for (int p = 0; p < 3; ++p) {
        for (int i = 0; i < width; ++i) {
          const rgb_t *src = (const rgb_t *)(srcb+i*(bpp/8));
          vuint8 c = (p == 0 ? src->r : p == 1 ? src->g : src->b);
          if ((c&0xc0) == 0xc0) {
            vuint8 tmp = 0xc1;
            *Strm << tmp;
          }
          *Strm << c;
        }
      }
    }
  }

  delete Strm;
}
#endif
