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
  vint8 color_planes;
  vuint16 bytes_per_line;
  vuint16 palette_type;

  vuint16 horz_screen_size;
  vuint16 vert_screen_size;

  vint8 filler[54];

  friend VStream &operator << (VStream &Strm, pcx_t &h) {
    Strm << h.manufacturer << h.version << h.encoding << h.bits_per_pixel
      << h.xmin << h.ymin << h.xmax << h.ymax << h.hres << h.vres;
    Strm.Serialise(h.palette, 48);
    Strm << h.reserved << h.color_planes << h.bytes_per_line
      << h.palette_type << h.horz_screen_size << h.vert_screen_size;
    Strm.Serialise(h.filler, 54);
    return Strm;
  }

  bool isValid () const {
    if (manufacturer != 0x0a) return false;
    if (reserved != 0) return false;
    if (encoding != 0 && encoding != 1) return false;
    if (version != 5) return false;
    if (xmax-xmin+1 < 1 || xmax-xmin+1 > 32000) return false; // why not?
    if (ymax-ymin+1 < 1 || ymax-ymin+1 > 32000) return false; // why not?
    if (color_planes == 1) {
      if (bits_per_pixel != 8 && bits_per_pixel != 24 && bits_per_pixel != 32) return false;
    } else if (color_planes == 3 || color_planes == 4) {
      if (bits_per_pixel != 8) return false;
    } else {
      // invalid number of color planes
      return false;
    }
    if (palette_type != 1 && palette_type != 2) return false;
    if (bytes_per_line != (xmax-xmin+1)*(bits_per_pixel/8)) return false; // this is true for both color plane types
    //k8: don't check reserved, other checks should provide enough reliability
    /*k8: filler *might* be zero-filled, but it is not required by any spec
    for (int i = 0; i < 54; ++i) if (Hdr.filler[i] != 0) return nullptr;
    */
    return true;
  }

  void dump () const {
    GLog.WriteLine("PCX: vendor=0x%02x; version=%u; encoding=%u; bpp=%u", manufacturer, version, encoding, bits_per_pixel);
    GLog.WriteLine("PCX: dims=(%u,%u)-(%u,%u); resolution=%u:%u", xmin, ymin, xmax, ymax, hres, vres);
    GLog.WriteLine("PCX: planes=%u; bpl=%u (expected:%u); palettetype=%u", color_planes, bytes_per_line, xmax-xmin+1, palette_type);
    GLog.WriteLine("PCX: screen=%u:%u", horz_screen_size, vert_screen_size);
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

  if (!Hdr.isValid()) {
    //if (Hdr.manufacturer == 0x0a) { GLog.WriteLine("=== PCX '%s' ===", *Strm.GetName()); Hdr.dump(); }
    return nullptr;
  }

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
  //mFormat = TEXFMT_8Pal;
  if (Hdr.color_planes == 1) {
    mFormat = (Hdr.bits_per_pixel >= 24 ? TEXFMT_RGBA : TEXFMT_8Pal);
  } else if (Hdr.color_planes == 3 || Hdr.color_planes == 4) {
    mFormat = TEXFMT_RGBA;
  } else {
    Sys_Error("PCX '%s' wtf?!", *W_FullLumpName(SourceLump));
  }
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
  // if we already have loaded pixels, return them
  if (Pixels) return Pixels;
  transparent = false;

  // open stream
  VStream *lumpstream = W_CreateLumpReaderNum(SourceLump);
  VCheckedStream Strm(lumpstream);

  int strmSize = Strm.TotalSize();

  // read header
  pcx_t pcx;
  Strm << pcx;

  if (!pcx.isValid()) { pcx.dump(); Sys_Error("invalid PCX file '%s'", *W_FullLumpName(SourceLump)); }

  int currPos = Strm.Tell();
  Width = pcx.xmax-pcx.xmin+1;
  Height = pcx.ymax-pcx.ymin+1;

  bool hasAlpha = false;
  if (pcx.color_planes == 1) {
    mFormat = (pcx.bits_per_pixel >= 24 ? TEXFMT_RGBA : TEXFMT_8Pal);
    hasAlpha = (pcx.bits_per_pixel == 32);
  } else if (pcx.color_planes == 3 || pcx.color_planes == 4) {
    mFormat = TEXFMT_RGBA;
    hasAlpha = (pcx.color_planes == 4);
  } else {
    Sys_Error("PCX '%s' wtf?!", *W_FullLumpName(SourceLump));
  }

  int pixelsPitch = (mFormat == TEXFMT_8Pal ? 1 : 4);
  Pixels = new vuint8[(Width*pixelsPitch)*Height];

  /*
  GLog.WriteLine("=== %s ===", *W_FullLumpName(SourceLump));
  pcx.dump();
  GLog.WriteLine("  pitch=%d", pixelsPitch);
  */

  TArray<vuint8> line;
  line.setLength(pcx.bytes_per_line*pcx.color_planes);

  for (int y = 0; y < Height; ++y) {
    //GLog.WriteLine("  line=%d of %d", y, Height);
    // read one line
    int lpos = 0;
    for (int p = 0; p < pcx.color_planes; ++p) {
      int count = 0;
      vuint8 b;
      for (int n = 0; n < pcx.bytes_per_line; ++n) {
        //GLog.WriteLine("    p=%d; n=%d; count=%d; b=0x%02x", p, n, count, b);
        if (count == 0) {
          // read next byte, do RLE decompression by the way
          if (currPos < strmSize) {
            ++currPos;
            Strm << b;
          } else {
            b = 0;
          }
          if (pcx.encoding) {
            if ((b&0xc0) == 0xc0) {
              count = b&0x3f;
              if (count == 0) Sys_Error("invalid pcx RLE data for '%s'", *W_FullLumpName(SourceLump));
              if (currPos < strmSize) {
                ++currPos;
                Strm << b;
              } else {
                b = 0;
              }
            } else {
              count = 1;
            }
          } else {
            count = 1;
          }
          check(count > 0);
          //GLog.WriteLine("    p=%d; n=%d; count=%d; b=0x%02x; fpos=%d, fsize=%d", p, n, count, b, Strm.Tell(), Strm.TotalSize());
        }
        line[lpos+n] = b;
        --count;
      }
      // allow excessive counts, why not?
      lpos += pcx.bytes_per_line;
    }
    //GLog.WriteLine("  line=%d of %d done", y, Height);

    const vuint8 *src = line.ptr();
    vuint8 *dest = (vuint8 *)(&Pixels[y*(Width*pixelsPitch)]);
    if (mFormat == TEXFMT_8Pal) {
      // paletted, simply copy the data
      memcpy(dest, src, Width);
    } else {
      if (pcx.color_planes != 1) {
        // planar
        for (int x = 0; x < Width; ++x) {
          *dest++ = src[0]; // red
          *dest++ = src[pcx.bytes_per_line]; // green
          *dest++ = src[pcx.bytes_per_line*2]; // blue
          if (hasAlpha) {
            *dest++ = src[pcx.bytes_per_line*3]; // blue
          } else {
            *dest++ = 255; // alpha (opaque)
          }
          ++src;
        }
      } else {
        // flat
        for (int x = 0; x < Width; ++x) {
          *dest++ = *src++; // red
          *dest++ = *src++; // green
          *dest++ = *src++; // blue
          if (hasAlpha) {
            *dest++ = *src++; // alpha
          } else {
            *dest++ = 255; // alpha (opaque)
          }
        }
      }
    }
  }
  //GLog.WriteLine("  PCX DONE!");

  if (mFormat == TEXFMT_8Pal) {
    // if not followed by palette ID, assume palette is at the end of file
    if (Strm.TotalSize()-Strm.Tell() < 769) Sys_Error("invalid pcx palette data for '%s'", *W_FullLumpName(SourceLump));
    vuint8 ch;
    Strm << ch;
    if (ch != 12) Strm.Seek(Strm.TotalSize()-768);

    // read palette
    Palette = new rgba_t[256];
    for (int c = 0; c < 256; ++c) {
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
  } else if (hasAlpha) {
    const vuint8 *s = Pixels;
    for (int count = Width*Height; count--; s += 4) {
      if (s[3] == 0) { transparent = true; break; }
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
  pcx.version = 5; // 256 color
  pcx.encoding = 1; // uncompressed
  pcx.bits_per_pixel = 8; // 256 color
  pcx.xmin = 0;
  pcx.ymin = 0;
  pcx.xmax = width-1;
  pcx.ymax = height-1;
  pcx.hres = width;
  pcx.vres = height;
  memset(pcx.palette, 0, sizeof(pcx.palette));
  pcx.color_planes = (bpp == 8 ? 1 : 3);
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
