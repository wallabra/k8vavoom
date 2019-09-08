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
#include <stdlib.h>

#include "../imago.h"


// ////////////////////////////////////////////////////////////////////////// //
struct PNGData {
  struct Chunk {
    vuint32 ID;
    vuint32 Offset;
    vuint32 Size;
  };

  vuint8 pal[768]; //RGB
  vuint8 trans[768]; //alpha for palette (color 0 will be transparent for paletted images, as DooM does it this way)
  vuint8 tR, tG, tB; // transparent color for RGB images
  bool hasTrans;

  int width;
  int height;
  unsigned int idatlen;
  vuint8 bitdepth;
  vuint8 colortype;
  vuint8 interlace;
  vuint8 *pixbuf;

  PNGData ();
  ~PNGData ();

  vuint8 getR (int x, int y) const;
  vuint8 getG (int x, int y) const;
  vuint8 getB (int x, int y) const;
  vuint8 getA (int x, int y) const;

private:
  const vuint8 *pixaddr (int x, int y) const;
};


// Verify that a file really is a PNG file. This includes not only checking
// the signature, but also checking for the IEND chunk. CRC checking of
// each chunk is not done. If it is valid, you get a PNGHandle to pass to
// the following functions.
static PNGData *pngOpen (VStream *file);

// The file must be positioned at the start of the first IDAT. It reads
// image data into the provided buffer. Returns true on success.
static bool pngReadIDAT (VStream &file, vuint8 *buffer, int width, int height, int pitch,
                         vuint8 bitdepth, vuint8 colortype, vuint8 interlace, unsigned int idatlen);



// ////////////////////////////////////////////////////////////////////////// //
VImage *imagoLoadPNG (VStream *strm) {
  if (!strm || strm->IsError()) return nullptr;
  strm->Seek(0);
  if (strm->IsError()) return nullptr;
  if (strm->TotalSize() < 29) return nullptr;
  auto png = pngOpen(strm);
  if (!png) return nullptr;
  if (png->width < 1 || png->height < 1) { delete png; return nullptr; }
  VImage *res = nullptr;
  try {
    png->pixbuf = new vuint8[png->width*png->height*4];
    if (!pngReadIDAT(*strm, png->pixbuf, png->width, png->height, png->width*4, png->bitdepth, png->colortype, png->interlace, png->idatlen)) {
      delete png;
      return nullptr;
    }
    res = new VImage(VImage::IT_RGBA, png->width, png->height);
    for (int y = 0; y < png->height; ++y) {
      for (int x = 0; x < png->width; ++x) {
        VImage::RGBA col(png->getR(x, y), png->getG(x, y), png->getB(x, y), png->getA(x, y));
        res->setPixel(x, y, col);
      }
    }
  } catch (...) {
    delete png;
    throw;
  }
  delete png;
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
#ifndef __BIG_ENDIAN__
# define MAKE_ID(a,b,c,d)  ((uint32_t)((a)|((b)<<8)|((c)<<16)|((d)<<24)))
#else
# define MAKE_ID(a,b,c,d)  ((uint32_t)((d)|((c)<<8)|((b)<<16)|((a)<<24)))
#endif


// zlib includes some CRC32 stuff, so just use that
//static inline const uint32_t *GetCRCTable () { return (const uint32_t *)get_crc_table(); }
static inline uint32_t CalcCRC32 (const uint8_t *buf, unsigned int len) { return mz_crc32 (0, buf, len); }
//static inline uint32_t AddCRC32 (uint32_t crc, const uint8_t *buf, unsigned int len) { return crc32 (crc, buf, len); }
//static inline uint32_t CRC1 (uint32_t crc, const uint8_t c, const uint32_t *crcTable) { return crcTable[(crc & 0xff) ^ c] ^ (crc >> 8); }


PNGData::PNGData () : width(0), height(0), idatlen(0), pixbuf(nullptr) {
}


PNGData::~PNGData () {
  delete[] pixbuf;
}


const vuint8 *PNGData::pixaddr (int x, int y) const {
  if (width < 1 || height < 1 || x < 0 || y < 0 || x >= width || y >= height) return nullptr;
  int xmul;
  switch (colortype) {
    case 0: xmul = 1; break; // grayscale
    case 2: xmul = 3; break; // RGB
    case 3: xmul = 1; break; // paletted
    case 4: xmul = 2; break; // grayscale+alpha
    case 6: xmul = 4; break; // RGBA
    default: return nullptr;
  }
  return &pixbuf[y*(width*4)+x*xmul];
}


vuint8 PNGData::getR (int x, int y) const {
  const vuint8 *a = pixaddr(x, y);
  if (!a) return 0;
  if (colortype == 3) return pal[a[0]*3+0]; // paletted
  return a[0]; // other chunks
}

vuint8 PNGData::getG (int x, int y) const {
  const vuint8 *a = pixaddr(x, y);
  if (!a) return 0;
  switch (colortype) {
    case 0: return a[0]; // grayscale
    case 2: return a[1]; // RGB
    case 3: return pal[a[0]*3+1]; // paletted
    case 4: return a[0]; // grayscale+alpha
    case 6: return a[1]; // RGBA
  }
  return 0;
}

vuint8 PNGData::getB (int x, int y) const {
  const vuint8 *a = pixaddr(x, y);
  if (!a) return 0;
  switch (colortype) {
    case 0: return a[0]; // grayscale
    case 2: return a[2]; // RGB
    case 3: return pal[a[0]*3+2]; // paletted
    case 4: return a[0]; // grayscale+alpha
    case 6: return a[2]; // RGBA
  }
  return 0;
}

vuint8 PNGData::getA (int x, int y) const {
  const vuint8 *a = pixaddr(x, y);
  if (!a) return 0;
  switch (colortype) {
    case 0: return (hasTrans ? trans[a[0]] : 255); // grayscale
    case 2: // RGB
      if (hasTrans) {
        if (a[0] == tR && a[1] == tG && a[2] == tB) return 0;
      }
      return 255;
    case 3: return (hasTrans ? trans[a[0]] : 255); // paletted
    case 4: return a[1]; // grayscale+alpha
    case 6: return a[3]; // RGBA
  }
  return 0;
}


static void UnfilterRow (int width, vuint8 *dest, vuint8 *stream, vuint8 *prev, int bpp);
static void UnpackPixels (int width, int bytesPerRow, int bitdepth, const vuint8 *rowin, vuint8 *rowout, bool grayscale);


static bool readI32BE (VStream *filer, int *res) {
  if (res) *res = 0;
  if (!filer) return false;
  vuint32 u = 0;
  for (int f = 3; f >= 0; --f) {
    vuint8 b;
    filer->Serialise(&b, 1);
    if (filer->IsError()) return false;
    u |= ((vuint32)b)<<(f*8);
  }
  if (u > 10000) return false;
  if (res) *res = (int)u;
  return true;
}


static bool readU16BE (VStream *filer, vuint16 *res) {
  if (res) *res = 0;
  if (!filer) return false;
  vuint16 u = 0;
  for (int f = 1; f >= 0; --f) {
    vuint8 b;
    filer->Serialise(&b, 1);
    if (filer->IsError()) return false;
    u |= ((vuint32)b)<<(f*8);
  }
  if (res) *res = u;
  return true;
}


static vuint8 convertBPP (vuint16 v, vuint8 bitdepth) {
  switch (bitdepth) {
    case 1: return (v ? 255 : 0);
    case 2: return (vuint8)((v&3)*255/3);
    case 4: return (vuint8)((v&7)*255/7);
    case 8: return (vuint8)(v&255);
    case 16: return (vuint8)(v*255/0xffff);
  }
  return 255;
}


PNGData *pngOpen (VStream *filer) {
  PNGData::Chunk chunk;
  PNGData *png;
  vuint32 data[2];
  bool sawIDAT = false;

  if (!filer) return nullptr;
  if (filer->IsError()) return nullptr;

  filer->Serialise(&data, 8);
  if (filer->IsError()) return nullptr;
  if (data[0] != MAKE_ID(137,'P','N','G') || data[1] != MAKE_ID(13,10,26,10)) return nullptr; // does not have PNG signature
  filer->Serialise(&data, 8);
  if (filer->IsError()) return nullptr;
  if (data[1] != MAKE_ID('I','H','D','R')) return nullptr; // IHDR must be the first chunk

  // the PNG looks valid so far
  // check the IHDR to make sure it's a type of PNG we support
  int width, height;

  // read width
  if (!readI32BE(filer, &width)) return nullptr;
  if (width < 1) return nullptr;

  // read height
  if (!readI32BE(filer, &height)) return nullptr;
  if (height < 1) return nullptr;

  // it looks like a PNG so far, so start creating a PNGHandle for it
  png = new PNGData();
  png->width = width;
  png->height = height;
  png->tR = png->tG = png->tB = 0;

  vuint8 compression = 0;
  vuint8 filter = 0;

  filer->Serialise(&png->bitdepth, 1);
  if (filer->IsError()) { delete png; return nullptr; }
  filer->Serialise(&png->colortype, 1);
  if (filer->IsError()) { delete png; return nullptr; }
  filer->Serialise(&compression, 1);
  if (filer->IsError()) { delete png; return nullptr; }
  filer->Serialise(&filter, 1);
  if (filer->IsError()) { delete png; return nullptr; }
  filer->Serialise(&png->interlace, 1);
  if (filer->IsError()) { delete png; return nullptr; }

  if (compression != 0 || filter != 0 || png->interlace > 1) { delete png; return nullptr; }
  if (!((1<<png->colortype)&0x5D)) { delete png; return nullptr; }
  if (!((1<<png->bitdepth)&0x116)) { delete png; return nullptr; }

  if (png->bitdepth > 8) { delete png; return nullptr; }

  memset(png->pal, 0, sizeof(png->pal));
  memset(png->trans, 255, sizeof(png->trans));
  //png->trans[0] = 0; // DooM does this
  png->hasTrans = false;

  chunk.ID = data[1];
  chunk.Offset = 16;
  chunk.Size = BigLong((unsigned int)data[0]);
  //png->Chunks.Append(chunk);
  filer->Seek(16);

  int idatPos = -1;

  for (;;) {
    if (filer->TotalSize()-filer->Tell() <= (int)chunk.Size+4) break;
    filer->Seek(filer->Tell()+chunk.Size+4);
    if (filer->TotalSize()-filer->Tell() < 12) break;

    // if the file ended before an IEND was encountered, it's not a PNG
    filer->Serialise(&data, 8);
    if (filer->IsError()) break;
    // an IEND chunk terminates the PNG and must be empty
    if (data[1] == MAKE_ID('I','E','N','D')) {
      if (data[0] == 0 && sawIDAT) {
        filer->Seek(idatPos);
        if (filer->IsError()) break;
        return png;
      }
      break;
    }
    chunk.ID = data[1];
    chunk.Offset = (vuint32)filer->Tell();
    chunk.Size = BigLong((unsigned int)data[0]);
    //png->Chunks.Append(chunk);

    // a PNG must include an IDAT chunk
    if (data[1] == MAKE_ID('I','D','A','T')) {
      if (idatPos < 0) {
        idatPos = chunk.Offset;
        png->idatlen = chunk.Size;
      }
      sawIDAT = true;
    }

    if (data[1] == MAKE_ID('P','L','T','E')) {
      if (chunk.Size%3 != 0) break; // invalid chunk
      if (chunk.Size > 768) break; // invalid chunk
      filer->Serialise(png->pal, (int)chunk.Size);
      if (filer->IsError()) break;
      chunk.Size = 0; // don't try to seek past its contents again
    }

    if (data[1] == MAKE_ID('t','R','N','S')) {
      vuint16 v;
      bool error = false;
      switch (png->colortype) {
        case 0: // grayscale
          if (chunk.Size != 2) { error = true; break; } // invalid chunk
          if (!readU16BE(filer, &v)) { error = true; break; }
          memset(png->trans, 255, sizeof(png->trans));
          png->trans[convertBPP(v, png->bitdepth)] = 0;
          chunk.Size = 0; // don't try to seek past its contents again
          break;
        case 3: // paletted
          png->hasTrans = true;
          if (chunk.Size > 256) { error = true; break; } // invalid chunk
          memset(png->trans, 255, sizeof(png->trans));
          if (chunk.Size) {
            filer->Serialise(png->trans, (int)chunk.Size);
            if (filer->IsError()) { error = true; break; }
          }
          chunk.Size = 0; // don't try to seek past its contents again
          break;
        case 2: // RGB
          if (chunk.Size != 3*2) { error = true; break; } // invalid chunk
          png->hasTrans = true;
          if (!readU16BE(filer, &v)) { error = true; break; }
          png->tR = convertBPP(v, png->bitdepth);
          if (!readU16BE(filer, &v)) { error = true; break; }
          png->tG = convertBPP(v, png->bitdepth);
          if (!readU16BE(filer, &v)) { error = true; break; }
          png->tB = convertBPP(v, png->bitdepth);
          chunk.Size = 0; // don't try to seek past its contents again
          break;
      }
      if (error) break;
    }
  }

  delete png;
  return nullptr;
}


static bool pngReadIDAT (VStream &file, vuint8 *buffer, int width, int height, int pitch,
                         vuint8 bitdepth, vuint8 colortype, vuint8 interlace, unsigned int chunklen)
{
  // uninterlaced images are treated as a conceptual eighth pass by these tables
  static const vuint8 passwidthshift[8] =  { 3, 3, 2, 2, 1, 1, 0, 0 };
  static const vuint8 passheightshift[8] = { 3, 3, 3, 2, 2, 1, 1, 0 };
  static const vuint8 passrowoffset[8] =   { 0, 0, 4, 0, 2, 0, 1, 0 };
  static const vuint8 passcoloffset[8] =   { 0, 4, 0, 2, 0, 1, 0, 0 };

  vuint8 *inputLine, *prev, *curr, *adam7buff[3], *bufferend;
  vuint8 chunkbuffer[4096];
  mz_stream stream;
  int err;
  int i, pass, passbuff, passpitch, passwidth;
  bool lastIDAT;
  int bytesPerRowIn, bytesPerRowOut;
  int bytesPerPixel;
  bool initpass;

  switch (colortype) {
    case 2: bytesPerPixel = 3; break; // RGB
    case 4: bytesPerPixel = 2; break; // LA
    case 6: bytesPerPixel = 4; break; // RGBA
    default: bytesPerPixel = 1; break;
  }

  bytesPerRowOut = width*bytesPerPixel;
  i = 4+bytesPerRowOut*2;
  if (interlace) i += bytesPerRowOut*2;

  inputLine = (vuint8 *)alloca(i);
  adam7buff[0] = inputLine+4+bytesPerRowOut;
  adam7buff[1] = adam7buff[0]+bytesPerRowOut;
  adam7buff[2] = adam7buff[1]+bytesPerRowOut;
  bufferend = buffer+pitch*height;

  stream.next_in = /*Z_NULL*/0;
  stream.avail_in = 0;
  stream.zalloc = /*Z_NULL*/0;
  stream.zfree = /*Z_NULL*/0;
  err = mz_inflateInit (&stream);
  if (err != MZ_OK) return false;

  lastIDAT = false;
  initpass = true;
  pass = (interlace ? 0 : 7);

  // silence GCC warnings
  // due to initpass being true, these will be set before they're used, but it doesn't know that
  curr = prev = 0;
  passwidth = passpitch = bytesPerRowIn = 0;
  passbuff = 0;

  while (err != MZ_STREAM_END && pass < 8-interlace) {
    if (initpass) {
      int rowoffset, coloffset;

      initpass = false;
      --pass;
      do {
        ++pass;
        rowoffset = passrowoffset[pass];
        coloffset = passcoloffset[pass];
      } while ((rowoffset >= height || coloffset >= width) && pass < 7);
      if (pass == 7 && interlace) break;
      passwidth = (width+(1<<passwidthshift[pass])-1-coloffset)>>passwidthshift[pass];
      prev = adam7buff[0];
      passbuff = 1;
      memset (prev, 0, passwidth*bytesPerPixel);
      switch (bitdepth) {
        case 8: bytesPerRowIn = passwidth*bytesPerPixel; break;
        case 4: bytesPerRowIn = (passwidth+1)/2; break;
        case 2: bytesPerRowIn = (passwidth+3)/4; break;
        case 1: bytesPerRowIn = (passwidth+7)/8; break;
        default: return false;
      }
      curr = buffer+rowoffset*pitch+coloffset*bytesPerPixel;
      passpitch = pitch<<passheightshift[pass];
      stream.next_out = inputLine;
      stream.avail_out = bytesPerRowIn+1;
    }
    if (stream.avail_in == 0 && chunklen > 0) {
      stream.next_in = chunkbuffer;
      int rd = (int)sizeof(chunkbuffer);
      if (rd > (int)chunklen) rd = (int)chunklen;
      file.Serialise(chunkbuffer, rd);
      if (file.IsError()) rd = 0;
      stream.avail_in = (vuint32)rd;
      //stream.avail_in = (vuint32)file.Read (chunkbuffer, MIN<vuint32>(chunklen,sizeof(chunkbuffer)));
      chunklen -= stream.avail_in;
    }

    err = mz_inflate (&stream, MZ_SYNC_FLUSH);
    if (err != MZ_OK && err != MZ_STREAM_END) {
      // something unexpected happened
      mz_inflateEnd(&stream);
      return false;
    }

    if (stream.avail_out == 0) {
      if (pass >= 6) {
        // store pixels directly into the output buffer
        UnfilterRow(bytesPerRowIn, curr, inputLine, prev, bytesPerPixel);
        prev = curr;
      } else {
        const vuint8 *in;
        vuint8 *out;
        int colstep, x;

        // store pixels into a temporary buffer
        UnfilterRow(bytesPerRowIn, adam7buff[passbuff], inputLine, prev, bytesPerPixel);
        prev = adam7buff[passbuff];
        passbuff ^= 1;
        in = prev;
        if (bitdepth < 8) {
          UnpackPixels(passwidth, bytesPerRowIn, bitdepth, in, adam7buff[2], colortype == 0);
          in = adam7buff[2];
        }
        // distribute pixels into the output buffer
        out = curr;
        colstep = bytesPerPixel<<passwidthshift[pass];
        switch (bytesPerPixel) {
          case 1:
            for (x = passwidth; x > 0; --x) {
              *out = *in;
              out += colstep;
              in += 1;
            }
            break;
          case 2:
            for (x = passwidth; x > 0; --x) {
              *(vuint16 *)out = *(vuint16 *)in;
              out += colstep;
              in += 2;
            }
            break;
          case 3:
            for (x = passwidth; x > 0; --x) {
              out[0] = in[0];
              out[1] = in[1];
              out[2] = in[2];
              out += colstep;
              in += 3;
            }
            break;
          case 4:
            for (x = passwidth; x > 0; --x) {
              *(vuint32 *)out = *(vuint32 *)in;
              out += colstep;
              in += 4;
            }
            break;
          }
      }
      if ((curr += passpitch) >= bufferend) {
        ++pass;
        initpass = true;
      }
      stream.next_out = inputLine;
      stream.avail_out = bytesPerRowIn+1;
    }

    if (chunklen == 0 && !lastIDAT) {
      vuint32 x[3];
      file.Serialise(x, 12);
           if (file.IsError()) lastIDAT = true;
      else if (x[2] != MAKE_ID('I','D','A','T')) lastIDAT = true;
      else chunklen = BigLong((unsigned int)x[1]);
    }
  }

  mz_inflateEnd(&stream);

  if (bitdepth < 8) {
    // noninterlaced images must be unpacked completely
    // interlaced images only need their final pass unpacked
    passpitch = pitch<<interlace;
    for (curr = buffer+pitch*interlace; curr <= prev; curr += passpitch) {
      UnpackPixels(width, bytesPerRowIn, bitdepth, curr, curr, colortype == 0);
    }
  }
  return true;
}


//==========================================================================
//
// UnfilterRow
//
// Unfilters the given row. Unknown filter types are silently ignored.
// bpp is bytes per pixel, not bits per pixel.
// width is in bytes, not pixels.
//
//==========================================================================
static void UnfilterRow (int width, vuint8 *dest, vuint8 *row, vuint8 *prev, int bpp) {
  int x;

  switch (*row++) {
    case 1: // sub
      x = bpp;
      do { *dest++ = *row++; } while (--x);
      for (x = width-bpp; x > 0; --x) {
        *dest = *row++ + *(dest-bpp);
        ++dest;
      }
      break;
    case 2: // up
      x = width;
      do { *dest++ = *row++ + *prev++; } while (--x);
      break;
    case 3: // average
      x = bpp;
      do { *dest++ = *row++ + (*prev++)/2; } while (--x);
      for (x = width-bpp; x > 0; --x) {
        *dest = *row++ + (vuint8)((unsigned(*(dest-bpp))+unsigned(*prev++))>>1);
        ++dest;
      }
      break;
    case 4: // paeth
      x = bpp;
      do { *dest++ = *row++ + *prev++; } while (--x);
      for (x = width-bpp; x > 0; --x) {
        int a = *(dest-bpp);
        int b = *(prev);
        int c = *(prev-bpp);
        int pa = b-c;
        int pb = a-c;
        int pc = abs(pa+pb);
        pa = abs(pa);
        pb = abs(pb);
        *dest = *row+(vuint8)((pa <= pb && pa <= pc) ? a : (pb <= pc) ? b : c);
        ++dest;
        ++row;
        ++prev;
      }
      break;
    default:  // Treat everything else as filter type 0 (none)
      memcpy(dest, row, width);
      break;
  }
}


//==========================================================================
//
// UnpackPixels
//
// Unpacks a row of pixels whose depth is less than 8 so that each pixel
// occupies a single byte. The outrow must be "width" bytes long.
// "bytesPerRow" is the number of bytes for the packed row. The in and out
// rows may overlap, but only if rowin == rowout.
//
//==========================================================================
static void UnpackPixels (int width, int bytesPerRow, int bitdepth, const vuint8 *rowin, vuint8 *rowout, bool grayscale) {
  const vuint8 *in;
  vuint8 *out;
  vuint8 pack;
  int lastbyte;

  //assert(bitdepth == 1 || bitdepth == 2 || bitdepth == 4);

  out = rowout+width;
  in = rowin+bytesPerRow;

  switch (bitdepth) {
    case 1:
      lastbyte = width&7;
      if (lastbyte != 0) {
        --in;
        pack = *in;
        out -= lastbyte;
        out[0] = (pack>>7)&1;
        if (lastbyte >= 2) out[1] = (pack>>6)&1;
        if (lastbyte >= 3) out[2] = (pack>>5)&1;
        if (lastbyte >= 4) out[3] = (pack>>4)&1;
        if (lastbyte >= 5) out[4] = (pack>>3)&1;
        if (lastbyte >= 6) out[5] = (pack>>2)&1;
        if (lastbyte == 7) out[6] = (pack>>1)&1;
      }
      while (in-- > rowin) {
        pack = *in;
        out -= 8;
        out[0] = (pack>>7)&1;
        out[1] = (pack>>6)&1;
        out[2] = (pack>>5)&1;
        out[3] = (pack>>4)&1;
        out[4] = (pack>>3)&1;
        out[5] = (pack>>2)&1;
        out[6] = (pack>>1)&1;
        out[7] = pack&1;
      }
      break;
    case 2:
      lastbyte = width&3;
      if (lastbyte != 0) {
        --in;
        pack = *in;
        out -= lastbyte;
        out[0] = pack>>6;
        if (lastbyte >= 2) out[1] = (pack>>4)&3;
        if (lastbyte == 3) out[2] = (pack>>2)&3;
      }
      while (in-- > rowin) {
        pack = *in;
        out -= 4;
        out[0] = pack>>6;
        out[1] = (pack>>4)&3;
        out[2] = (pack>>2)&3;
        out[3] = pack&3;
      }
      break;
    case 4:
      lastbyte = width&1;
      if (lastbyte != 0) {
        --in;
        pack = *in;
        out -= lastbyte;
        out[0] = pack>>4;
      }
      while (in-- > rowin) {
        pack = *in;
        out -= 2;
        out[0] = pack>>4;
        out[1] = pack&15;
      }
      break;
  }

  // expand grayscale to 8bpp
  if (grayscale) {
    // put the 2-bit lookup table on the stack, since it's probably already in a cache line
    union {
      vuint32 bits2l;
      vuint8 bits2[4];
    };

    out = rowout+width;
    switch (bitdepth) {
      case 1:
        while (--out >= rowout) {
          // 1 becomes -1 (0xFF), and 0 remains untouched.
          *out = 0-*out;
        }
        break;
      case 2:
        bits2l = MAKE_ID(0x00,0x55,0xAA,0xFF);
        while (--out >= rowout) {
          *out = bits2[*out];
        }
        break;
      case 4:
        while (--out >= rowout) {
          *out |= (*out<<4);
        }
        break;
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
/*
class VPNGLoaderReg {
public:
  VPNGLoaderReg (int n) { ImagoRegisterLoader("png", "Portable Network Graphics", &loadPNG); }
};


static VPNGLoaderReg ldreg(666);
*/
