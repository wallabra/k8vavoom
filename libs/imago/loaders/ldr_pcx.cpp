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
#include "../../core/core.h"
#include "../imago.h"


// ////////////////////////////////////////////////////////////////////////// //
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


// ////////////////////////////////////////////////////////////////////////// //
VImage *imagoLoadPCX (VStream *strm) {
  if (!strm || strm->IsError()) return nullptr;
  strm->Seek(0);
  if (strm->IsError()) return nullptr;
  int strmSize = strm->TotalSize();
  if (strmSize < 128) return nullptr; // too small

  pcx_t pcx;
  strm->Seek(0);
  *strm << pcx;

  if (!pcx.isValid()) return nullptr;

  int currPos = strm->Tell();
  int Width = pcx.xmax-pcx.xmin+1;
  int Height = pcx.ymax-pcx.ymin+1;

  bool formatPaletted = false;
  bool hasAlpha = false;
  if (pcx.color_planes == 1) {
    formatPaletted = (pcx.bits_per_pixel < 24);
    hasAlpha = (pcx.bits_per_pixel == 32);
  } else if (pcx.color_planes == 3 || pcx.color_planes == 4) {
    hasAlpha = (pcx.color_planes == 4);
  } else {
    //Sys_Error("PCX '%s' wtf?!", *strm->GetName());
    return nullptr;
  }

  VImage *res = nullptr;

  int pixelsPitch = (formatPaletted ? 1 : 4);
  TArray<vuint8> PixelsStore;
  PixelsStore.setLength((Width*pixelsPitch)*Height);
  vuint8 *Pixels = PixelsStore.ptr();

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
            *strm << b;
          } else {
            b = 0;
          }
          if (pcx.encoding) {
            if ((b&0xc0) == 0xc0) {
              count = b&0x3f;
              if (count == 0) Sys_Error("invalid pcx RLE data for '%s'", *strm->GetName());
              if (currPos < strmSize) {
                ++currPos;
                *strm << b;
              } else {
                b = 0;
              }
            } else {
              count = 1;
            }
          } else {
            count = 1;
          }
          vassert(count > 0);
          //GLog.WriteLine("    p=%d; n=%d; count=%d; b=0x%02x; fpos=%d, fsize=%d", p, n, count, b, strm->Tell(), strm->TotalSize());
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
    if (formatPaletted) {
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

  if (formatPaletted) {
    // if not followed by palette ID, assume palette is at the end of file
    if (strm->TotalSize()-strm->Tell() > 0) {
      if (strm->TotalSize()-strm->Tell() > 768) {
        vuint8 ch;
        *strm << ch;
        if (ch != 12) {
          if (strm->TotalSize() < 768) Sys_Error("invalid pcx palette data for '%s'", *strm->GetName());
          //if (developer) GLog.Logf(NAME_Dev, "found some other data before palette in '%s'", *strm->GetName());
          //GLog.Logf(NAME_Warning, "found some other data before palette in '%s'", *strm->GetName());
          strm->Seek(strm->TotalSize()-768);
        } else {
          //if (developer) GLog.Logf(NAME_Dev, "palette data with marker in '%s'", *strm->GetName());
          //GLog.Logf(NAME_Warning, "palette data with marker in '%s'", *strm->GetName());
        }
      } else {
        if (strm->TotalSize()-strm->Tell() < 768) {
          GLog.Logf(NAME_Warning, "palette data takes only %d bytes in '%s'", strm->TotalSize()-strm->Tell(), *strm->GetName());
        } else {
          GLog.Logf(NAME_Warning, "palette data without marker in '%s'", *strm->GetName());
        }
      }
    } else {
      Sys_Error("missing pcx palette data for '%s'", *strm->GetName());
    }

    // read palette
    VImage::RGBA palette[256];
    for (int c = 0; c < 256; ++c) {
      if (strm->TotalSize()-strm->Tell() < 3) {
        GLog.Logf(NAME_Warning, "palette data too short in '%s'", *strm->GetName());
        palette[c].r = palette[c].g = palette[c].b = 0;
        palette[c].a = 0;
      } else {
        *strm << palette[c].r << palette[c].g << palette[c].b;
        palette[c].a = 255;
      }
    }

    res = new VImage(VImage::IT_Pal, Width, Height);
    res->setPalette(palette, 256);
    // copy pixels
    const vuint8 *s = Pixels;
    for (int y = 0; y < Height; ++y) {
      for (int x = 0; x < Width; ++x) {
        res->setBytePixel(x, y, *s++);
      }
    }
  } else {
    // rgba
    res = new VImage(VImage::IT_RGBA, Width, Height);
    // copy pixels
    const vuint8 *s = Pixels;
    for (int y = 0; y < Height; ++y) {
      for (int x = 0; x < Width; ++x, s += 4) {
        auto clr = VImage::RGBA(s[0], s[1], s[2], s[3]);
        res->setPixel(x, y, clr);
      }
    }
  }

  return res;
}
