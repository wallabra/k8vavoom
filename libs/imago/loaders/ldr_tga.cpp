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
struct tgaHeader_t {
  vuint8 id_length;
  vuint8 pal_type;
  vuint8 img_type;
  vuint16 first_colour;
  vuint16 pal_colours;
  vuint8 pal_entry_size;
  vuint16 left;
  vuint16 top;
  vuint16 width;
  vuint16 height;
  vuint8 bpp;
  vuint8 descriptor_bits;

  friend VStream &operator << (VStream &strm, tgaHeader_t &h) {
    return strm << h.id_length << h.pal_type << h.img_type
      << h.first_colour << h.pal_colours << h.pal_entry_size << h.left
      << h.top << h.width << h.height << h.bpp << h.descriptor_bits;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
VImage *imagoLoadTGA (VStream *strm) {
  if (!strm || strm->IsError()) return nullptr;
  strm->Seek(0);
  if (strm->IsError()) return nullptr;
  if (strm->TotalSize() < 18) return nullptr;

  tgaHeader_t hdr;
  *strm << hdr;

  //fprintf(stderr, "trying TGA...\n");

  if ((hdr.pal_type != 0 && hdr.pal_type != 1) || hdr.width <= 0 ||
      hdr.height <= 0 || hdr.width > 32768 || hdr.height > 32768 ||
      (hdr.pal_type == 0 && hdr.bpp != 15 && hdr.bpp != 16 &&
      hdr.bpp != 24 && hdr.bpp != 32) ||
      (hdr.pal_type == 1 && hdr.bpp != 8) ||
      (hdr.pal_type == 0 && hdr.img_type != 2 && hdr.img_type != 10) ||
      (hdr.pal_type == 1 && hdr.img_type != 1 && hdr.img_type != 3 &&
      hdr.img_type != 9 && hdr.img_type != 11) ||
      (hdr.pal_type == 1 && hdr.pal_entry_size != 16 &&
      hdr.pal_entry_size != 24 && hdr.pal_entry_size != 32) ||
      (hdr.descriptor_bits&16) != 0)
  {
    return nullptr;
  }

  //fprintf(stderr, "loading TGA...\n");
  // load texture
  int count;
  int c;

  int Width = hdr.width;
  int Height = hdr.height;

  if (Width < 1 || Height < 1 || Width > 32768 || Height > 32768) return nullptr;

  VImage::RGBA palette[256];
  VImage *res = nullptr;
  memset(palette, 0, sizeof(palette));

  strm->Seek(strm->Tell()+hdr.id_length);

  if (hdr.pal_type == 1) {
    for (int i = 0; i < hdr.pal_colours; ++i) {
      switch (hdr.pal_entry_size) {
        case 16:
          if (i < 256) {
            vuint16 col;
            *strm << col;
            palette[i].r = (col&0x1F)<<3;
            palette[i].g = ((col>>5)&0x1F)<<3;
            palette[i].b = ((col>>10)&0x1F)<<3;
            palette[i].a = 255;
          } else {
            vuint16 col;
            *strm << col;
          }
          break;
        case 24:
          if (i < 256) {
            *strm << palette[i].b << palette[i].g << palette[i].r;
            palette[i].a = 255;
          } else {
            vuint8 r, g, b;
            *strm << b << g << r;
          }
          break;
        case 32:
          if (i < 256) {
            *strm << palette[i].b << palette[i].g << palette[i].r << palette[i].a;
          } else {
            vuint8 r, g, b, a;
            *strm << b << g << r << a;
          }
          break;
      }
    }
  }

  /* Image type:
  *    0 = no image data
  *    1 = uncompressed colour mapped
  *    2 = uncompressed true colour
  *    3 = grayscale
  *    9 = RLE colour mapped
  *   10 = RLE true colour
  *   11 = RLE grayscale
  */
  if (hdr.img_type == 1 || hdr.img_type == 3 || hdr.img_type == 9 || hdr.img_type == 11) {
    res = new VImage(VImage::IT_Pal, Width, Height);
    res->setPalette(palette, 256);
  } else {
    res = new VImage(VImage::IT_RGBA, Width, Height);
  }

  if (hdr.img_type == 1 && hdr.bpp == 8 && hdr.pal_type == 1) {
    // 8-bit, uncompressed
    for (int y = Height; y; --y) {
      int yc = (hdr.descriptor_bits&0x20 ? Height-y : y-1);
      for (int x = 0; x < Width; ++x) {
        vuint8 b;
        *strm << b;
        res->setBytePixel(x, yc, b);
      }
    }
  } else if (hdr.img_type == 2 && hdr.pal_type == 0 && hdr.bpp == 16) {
    // 16-bit uncompressed
    for (int y = Height; y; --y) {
      int yc = (hdr.descriptor_bits&0x20 ? Height-y : y-1);
      for (int x = 0; x < Width; ++x) {
        vuint16 col;
        *strm << col;
        auto dst = VImage::RGBA(
          ((col>>10)&0x1F)<<3,
          ((col>>5)&0x1F)<<3,
          (col&0x1F)<<3,
          255);
        res->setPixel(x, yc, dst);
      }
    }
  } else if (hdr.img_type == 2 && hdr.pal_type == 0 && hdr.bpp == 24) {
    // 24-bit uncompressed
    for (int y = Height; y; --y) {
      int yc = (hdr.descriptor_bits&0x20 ? Height-y : y-1);
      for (int x = 0; x < Width; ++x) {
        VImage::RGBA dst;
        *strm << dst.b << dst.g << dst.r;
        dst.a = 255;
        res->setPixel(x, yc, dst);
      }
    }
  } else if (hdr.img_type == 2 && hdr.pal_type == 0 && hdr.bpp == 32) {
    // 32-bit uncompressed
    for (int y = Height; y; --y) {
      int yc = (hdr.descriptor_bits&0x20 ? Height-y : y-1);
      for (int x = 0; x < Width; ++x) {
        VImage::RGBA dst;
        *strm << dst.b << dst.g << dst.r << dst.a;
        res->setPixel(x, yc, dst);
      }
    }
  } else if (hdr.img_type == 3 && hdr.bpp == 8 && hdr.pal_type == 1) {
    // grayscale uncompressed
    for (int i = 0; i < 256; ++i) {
      palette[i].r = i;
      palette[i].g = i;
      palette[i].b = i;
      palette[i].a = 255;
    }
    res->setPalette(palette, 256);
    for (int y = Height; y; --y) {
      int yc = (hdr.descriptor_bits&0x20 ? Height-y : y-1);
      for (int x = 0; x < Width; ++x) {
        vuint8 b;
        *strm << b;
        res->setBytePixel(x, yc, b);
      }
    }
  } else if (hdr.img_type == 9 && hdr.bpp == 8 && hdr.pal_type == 1) {
    // 8-bit RLE compressed
    for (int y = Height; y; --y) {
      int yc = (hdr.descriptor_bits&0x20 ? Height-y : y-1);
      c = 0;
      int x = 0;
      do {
        count = Streamer<vuint8>(*strm);
        if (count&0x80) {
          count = (count&0x7F)+1;
          c += count;
          vuint8 col;
          *strm << col;
          while (count--) res->setBytePixel(x++, yc, col);
        } else {
          ++count;
          c += count;
          while (count--) {
            vuint8 col;
            *strm << col;
            res->setBytePixel(x++, yc, col);
          }
        }
      } while (c < Width);
    }
  } else if (hdr.img_type == 10 && hdr.pal_type == 0 && hdr.bpp == 16) {
    // 16-bit RLE compressed
    for (int y = Height; y; --y) {
      int yc = (hdr.descriptor_bits&0x20 ? Height-y : y-1);
      c = 0;
      int x = 0;
      do {
        count = Streamer<vuint8>(*strm);
        if (count&0x80) {
          count = (count&0x7F)+1;
          c += count;
          vuint16 col;
          *strm << col;
          while (count--) {
            auto dst = VImage::RGBA(
              ((col>>10)&0x1F)<<3,
              ((col>>5)&0x1F)<<3,
              (col&0x1F)<<3,
              255);
            res->setPixel(x++, yc, dst);
          }
        } else {
          ++count;
          c += count;
          while (count--) {
            vuint16 col;
            *strm << col;
            auto dst = VImage::RGBA(
              ((col>>10)&0x1F)<<3,
              ((col>>5)&0x1F)<<3,
              (col&0x1F)<<3,
              255);
            res->setPixel(x++, yc, dst);
          }
        }
      } while (c < Width);
    }
  } else if (hdr.img_type == 10 && hdr.pal_type == 0 && hdr.bpp == 24) {
    // 24-bit RLE compressed
    for (int y = Height; y; --y) {
      int yc = (hdr.descriptor_bits&0x20 ? Height-y : y-1);
      c = 0;
      int x = 0;
      do {
        count = Streamer<vuint8>(*strm);
        if (count&0x80) {
          count = (count&0x7F)+1;
          c += count;
          VImage::RGBA col;
          *strm << col.b << col.g << col.r;
          col.a = 255;
          while (count--) res->setPixel(x++, yc, col);
        } else {
          ++count;
          c += count;
          while (count--) {
            VImage::RGBA dst;
            *strm << dst.b << dst.g << dst.r;
            dst.a = 255;
            res->setPixel(x++, yc, dst);
          }
        }
      } while (c < Width);
    }
  } else if (hdr.img_type == 10 && hdr.pal_type == 0 && hdr.bpp == 32) {
    // 32-bit RLE compressed
    for (int y = Height; y; --y) {
      int yc = (hdr.descriptor_bits&0x20 ? Height-y : y-1);
      c = 0;
      int x = 0;
      do {
        count = Streamer<vuint8>(*strm);
        if (count&0x80) {
          count = (count&0x7F)+1;
          c += count;
          VImage::RGBA col;
          *strm << col.b << col.g << col.r << col.a;
          while (count--) res->setPixel(x++, yc, col);
        } else {
          ++count;
          c += count;
          while (count--) {
            VImage::RGBA dst;
            *strm << dst.b << dst.g << dst.r << dst.a;
            res->setPixel(x++, yc, dst);
          }
        }
      } while (c < Width);
    }
  } else if (hdr.img_type == 11 && hdr.bpp == 8 && hdr.pal_type == 1) {
    // grayscale RLE compressed
    for (int i = 0; i < 256; ++i) {
      palette[i].r = i;
      palette[i].g = i;
      palette[i].b = i;
      palette[i].a = 255;
    }
    res->setPalette(palette, 256);
    for (int y = Height; y; --y) {
      int yc = (hdr.descriptor_bits&0x20 ? Height-y : y-1);
      c = 0;
      int x = 0;
      do {
        count = Streamer<vuint8>(*strm);
        if (count&0x80) {
          count = (count&0x7F)+1;
          c += count;
          vuint8 col;
          *strm << col;
          while (count--) res->setBytePixel(x++, yc, col);
        } else {
          ++count;
          c += count;
          while (count--) {
            vuint8 b;
            *strm << b;
            res->setBytePixel(x++, yc, b);
          }
        }
      } while (c < Width);
    }
  } else {
    delete res;
    res = nullptr;
  }

  if (strm->IsError()) { delete res; res = nullptr; }

  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
/*
class VTGALoaderReg {
public:
  VTGALoaderReg (int n) { ImagoRegisterLoader("tga", "Targa Image", &loadTGA, 600); } // lesser priority
};


static VTGALoaderReg ldreg(666);
*/
