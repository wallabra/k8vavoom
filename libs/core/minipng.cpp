/*
** m_png.cpp
** Routines for manipulating PNG files.
**
**---------------------------------------------------------------------------
** Copyright 2002-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <stdlib.h>
#ifdef USE_INTERNAL_ZLIB
# include "../zlib/zlib.h"
#else
# include <zlib.h>
#endif

/*
#include "m_crc32.h"
#include "m_swap.h"
#include "c_cvars.h"
#include "r_defs.h"
#include "v_video.h"
#include "m_png.h"
*/
#include "core.h"


#ifndef __BIG_ENDIAN__
# define MAKE_ID(a,b,c,d)  ((uint32_t)((a)|((b)<<8)|((c)<<16)|((d)<<24)))
#else
# define MAKE_ID(a,b,c,d)  ((uint32_t)((d)|((c)<<8)|((b)<<16)|((a)<<24)))
#endif


// zlib includes some CRC32 stuff, so just use that

static inline const uint32_t *GetCRCTable () { return (const uint32_t *)get_crc_table(); }
static inline uint32_t CalcCRC32 (const uint8_t *buf, unsigned int len) { return crc32 (0, buf, len); }
static inline uint32_t AddCRC32 (uint32_t crc, const uint8_t *buf, unsigned int len) { return crc32 (crc, buf, len); }
static inline uint32_t CRC1 (uint32_t crc, const uint8_t c, const uint32_t *crcTable) { return crcTable[(crc & 0xff) ^ c] ^ (crc >> 8); }


// The maximum size of an IDAT chunk ZDoom will write. This is also the
// size of the compression buffer it allocates on the stack.
#define PNG_WRITE_SIZE  (32768)


struct IHDR {
  vuint32 Width;
  vuint32 Height;
  vuint8 BitDepth;
  vuint8 ColorType;
  vuint8 Compression;
  vuint8 Filter;
  vuint8 Interlace;
};


PNGHandle::PNGHandle (VStream *file) : ChunkPt(0), width(0), height(0), pixbuf(nullptr) {
  File = file;
}


PNGHandle::~PNGHandle () {
  delete[] pixbuf;
#ifdef MINIPNG_LOAD_TEXT_CHUNKS
  for (int i = 0; i < TextChunks.length(); ++i) {
    delete[] TextChunks[i];
    TextChunks[i] = nullptr;
  }
#endif
}


bool PNGHandle::loadIDAT () {
  if (!File || width < 1 || height < 1) return false;
  ChunkPt = 0;
  vuint32 fidlen = 0;
  for (; ChunkPt < (vuint32)Chunks.length(); ++ChunkPt) {
    if (Chunks[ChunkPt].ID == MAKE_ID('I','D','A','T')) {
      // found the chunk
      File->Seek(Chunks[ChunkPt++].Offset);
      fidlen = Chunks[ChunkPt-1].Size;
      break;
    }
  }
  if (fidlen == 0) return false;
  delete[] pixbuf;
  pixbuf = new vuint8[width*height*4];
  return M_ReadIDAT(*File, pixbuf, width, height, width*4, bitdepth, colortype, interlace, fidlen);
}


const vuint8 *PNGHandle::pixaddr (int x, int y) const {
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


vuint8 PNGHandle::getR (int x, int y) const {
  const vuint8 *a = pixaddr(x, y);
  if (!a) return 0;
  if (colortype == 3) return pal[a[0]*3+0]; // paletted
  return a[0]; // other chunks
}

vuint8 PNGHandle::getG (int x, int y) const {
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

vuint8 PNGHandle::getB (int x, int y) const {
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

vuint8 PNGHandle::getA (int x, int y) const {
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


static inline void MakeChunk (void *where, vuint32 type, size_t len);
static inline void StuffPalette (const PalEntry *from, vuint8 *to);
static bool WriteIDAT (VStream *file, const vuint8 *data, int len);
static void UnfilterRow (int width, vuint8 *dest, vuint8 *stream, vuint8 *prev, int bpp);
static void UnpackPixels (int width, int bytesPerRow, int bitdepth, const vuint8 *rowin, vuint8 *rowout, bool grayscale);

int png_level = 9;
float png_gamma = 0;

/*
CUSTOM_CVAR(Int, png_level, 5, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
  if (self < 0)
    self = 0;
  else if (self > 9)
    self = 9;
}
CVAR(Float, png_gamma, 0.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
*/


//==========================================================================
//
// M_CreatePNG
//
// Passed a newly-created file, writes the PNG signature and IHDR, gAMA, and
// PLTE chunks. Returns true if everything went as expected.
//
//==========================================================================
bool M_CreatePNG (VStream *file, const vuint8 *buffer, const PalEntry *palette,
                  ESSType color_type, int width, int height, int pitch, float gamma)
{
  vuint8 work[8+   // signature
        12+2*4+5+  // IHDR
        12+4+      // gAMA
        12+256*3]; // PLTE
  vuint32 *const sig = (vuint32 *)&work[0];
  IHDR *const ihdr = (IHDR *)&work[8+8];
  vuint32 *const gama = (vuint32 *)((vuint8 *)ihdr+2*4+5+12);
  vuint8 *const plte = (vuint8 *)gama+4+12;
  size_t work_len;

  sig[0] = MAKE_ID(137,'P','N','G');
  sig[1] = MAKE_ID(13,10,26,10);

  ihdr->Width = BigLong(width);
  ihdr->Height = BigLong((height < 0 ? -height : height));
  ihdr->BitDepth = 8;
  ihdr->ColorType = (color_type == SS_PAL ? 3 : color_type == SS_RGB ? 2 : 6);
  ihdr->Compression = 0;
  ihdr->Filter = 0;
  ihdr->Interlace = 0;
  MakeChunk(ihdr, MAKE_ID('I','H','D','R'), 2*4+5);

  // assume a display exponent of 2.2 (100000/2.2 ~= 45454.5)
  *gama = BigLong(int(45454.5f*(png_gamma == 0.0f ? gamma : png_gamma)));
  MakeChunk(gama, MAKE_ID('g','A','M','A'), 4);

  if (color_type == SS_PAL) {
    StuffPalette(palette, plte);
    MakeChunk(plte, MAKE_ID('P','L','T','E'), 256*3);
    work_len = sizeof(work);
  } else {
    work_len = sizeof(work)-(12+256*3);
  }

  file->Serialise(work, work_len);
  if (file->IsError()) return false;

  return M_SaveBitmap(buffer, color_type, width, height, pitch, file);
}


//==========================================================================
//
// M_CreateDummyPNG
//
// Like M_CreatePNG, but the image is always a grayscale 1x1 black square.
//
//==========================================================================
bool M_CreateDummyPNG (VStream *file) {
  static const vuint8 dummyPNG[] = {
    137,'P','N','G',13,10,26,10,
    0,0,0,13,'I','H','D','R',
    0,0,0,1,0,0,0,1,8,0,0,0,0,0x3a,0x7e,0x9b,0x55,
    0,0,0,10,'I','D','A','T',
    104,222,99,96,0,0,0,2,0,1,0x9f,0x65,0x0e,0x18
  };
  file->Serialise((void *)dummyPNG, (int)sizeof(dummyPNG));
  return !file->IsError();
}


//==========================================================================
//
// M_FinishPNG
//
// Writes an IEND chunk to a PNG file. The file is left opened.
//
//==========================================================================
bool M_FinishPNG (VStream *file) {
  static const vuint8 iend[12] = { 0,0,0,0,73,69,78,68,174,66,96,130 };
  file->Serialise((void *)iend, 12);
  return !file->IsError();
}


//==========================================================================
//
// M_AppendPNGChunk
//
// Writes a PNG-compliant chunk to the file.
//
//==========================================================================
bool M_AppendPNGChunk (VStream *file, vuint32 chunkID, const vuint8 *chunkData, vuint32 len) {
  vuint32 head[2] = { (vuint32)BigLong((vuint32)len), chunkID };
  vuint32 crc;

  file->Serialise(head, 8);
  if (file->IsError()) return false;
  if (len != 0) {
    file->Serialise((void *)chunkData, len);
    if (file->IsError()) return false;
  }
  crc = CalcCRC32((vuint8 *)&head[1], 4);
  if (len != 0) crc = AddCRC32(crc, chunkData, len);
  crc = BigLong((unsigned int)crc);
  file->Serialise(&crc, 4);
  return !file->IsError();
}


//==========================================================================
//
// M_AppendPNGText
//
// Appends a PNG tEXt chunk to the file
//
//==========================================================================
bool M_AppendPNGText (VStream *file, const char *keyword, const char *text) {
  struct { vuint32 len, id; char key[80]; } head;
  size_t len = strlen (text);
  size_t keylen = strlen(keyword);
  if (keylen > 79) keylen = 79;
  vuint32 crc;

  head.len = BigLong(len+keylen+1);
  head.id = MAKE_ID('t','E','X','t');
  memset (&head.key, 0, sizeof(head.key));
  //strncpy (head.key, keyword, keylen);
  strncpy (head.key, keyword, sizeof(head.key)-1);
  head.key[keylen] = 0;

  file->Serialise(&head, keylen+9);
  if (file->IsError()) return false;

  if (len) {
    file->Serialise((void *)text, len);
    if (file->IsError()) return false;
  }

  crc = CalcCRC32 ((vuint8 *)&head+4, keylen+5);
  if (len != 0) crc = AddCRC32 (crc, (vuint8 *)text, len);
  crc = BigLong(crc);
  file->Serialise(&crc, 4);
  return !file->IsError();
}


//==========================================================================
//
// M_FindPNGChunk
//
// Finds a chunk in a PNG file. The file pointer will be positioned at the
// beginning of the chunk data, and its length will be returned. A return
// value of 0 indicates the chunk was either not present or had 0 length.
// This means there is no way to conclusively determine if a chunk is not
// present in a PNG file with this function, but since we're only
// interested in chunks with content, that's okay. The file pointer will
// be left sitting at the start of the chunk's data if it was found.
//
//==========================================================================
unsigned int M_FindPNGChunk (PNGHandle *png, vuint32 id) {
  png->ChunkPt = 0;
  return M_NextPNGChunk(png, id);
}


//==========================================================================
//
// M_NextPNGChunk
//
// Like M_FindPNGChunk, but it starts it search at the current chunk.
//
//==========================================================================
unsigned int M_NextPNGChunk (PNGHandle *png, vuint32 id) {
  for (; png->ChunkPt < (vuint32)png->Chunks.length(); ++png->ChunkPt) {
    if (png->Chunks[png->ChunkPt].ID == id) {
      // Found the chunk
      png->File->Seek(png->Chunks[png->ChunkPt++].Offset);
      return png->Chunks[png->ChunkPt-1].Size;
    }
  }
  return 0;
}


//==========================================================================
//
// M_FindPNGIDAT
//
//==========================================================================
bool M_FindPNGIDAT (PNGHandle *png, vuint32 *chunkLen) {
  if (chunkLen) *chunkLen = 0;
  if (!png) return false;
  auto idx = M_FindPNGChunk(png, MAKE_ID('I','D','A','T'));
  if (idx == 0) return false;
  if (chunkLen) *chunkLen = png->Chunks[idx].Size;
  return true;
}


#ifdef MINIPNG_LOAD_TEXT_CHUNKS
//==========================================================================
//
// M_GetPNGText
//
// Finds a PNG text chunk with the given signature and returns a pointer
// to a nullptr-terminated string if present. Returns nullptr on failure.
//
//==========================================================================
char *M_GetPNGText (PNGHandle *png, const char *keyword) {
  size_t keylen, textlen;
  for (int i = 0; i < png->TextChunks.length(); ++i) {
    if (strncmp(keyword, png->TextChunks[i], 80) == 0) {
      // Woo! A match was found!
      keylen = strlen(keyword)+1;
      if (keylen > 80) keylen = 80;
      textlen = strlen(png->TextChunks[i]+keylen)+1;
      char *str = new char[textlen];
      strcpy(str, png->TextChunks[i]+keylen);
      return str;
    }
  }
  return nullptr;
}


// This version copies it to a supplied buffer instead of allocating a new one.
bool M_GetPNGText (PNGHandle *png, const char *keyword, char *buffer, size_t buffsize) {
  size_t keylen;
  for (int i = 0; i < png->TextChunks.length(); ++i) {
    if (strncmp(keyword, png->TextChunks[i], 80) == 0) {
      // Woo! A match was found!
      keylen = strlen(keyword)+1;
      if (keylen > 80) keylen = 80;
      strncpy(buffer, png->TextChunks[i]+keylen, buffsize);
      return true;
    }
  }
  return false;
}
#endif


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


//==========================================================================
//
// M_VerifyPNG
//
// Returns a PNGHandle if the file is a PNG or nullptr if not. CRC checking of
// chunks is not done in order to save time.
//
//==========================================================================
PNGHandle *M_VerifyPNG (VStream *filer) {
  PNGHandle::Chunk chunk;
  PNGHandle *png;
  vuint32 data[2];
  bool sawIDAT = false;

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
  png = new PNGHandle(filer);
  png->width = width;
  png->height = height;
  png->tR = png->tG = png->tB = 0;

  vuint8 compression = 0;
  vuint8 filter = 0;

  png->File->Serialise(&png->bitdepth, 1);
  if (png->File->IsError()) { delete png; return nullptr; }
  png->File->Serialise(&png->colortype, 1);
  if (png->File->IsError()) { delete png; return nullptr; }
  png->File->Serialise(&compression, 1);
  if (png->File->IsError()) { delete png; return nullptr; }
  png->File->Serialise(&filter, 1);
  if (png->File->IsError()) { delete png; return nullptr; }
  png->File->Serialise(&png->interlace, 1);
  if (png->File->IsError()) { delete png; return nullptr; }

  if (compression != 0 || filter != 0 || png->interlace > 1) { delete png; return nullptr; }
  if (!((1<<png->colortype)&0x5D)) { delete png; return nullptr; }
  if (!((1<<png->bitdepth)&0x116)) { delete png; return nullptr; }

  if (png->bitdepth > 8) { delete png; return nullptr; }

  memset(png->pal, 0, sizeof(png->pal));
  memset(png->trans, 255, sizeof(png->trans));
  png->trans[0] = 0; // DooM does this
  png->hasTrans = false;

  chunk.ID = data[1];
  chunk.Offset = 16;
  chunk.Size = BigLong((unsigned int)data[0]);
  png->Chunks.Append(chunk);
  png->File->Seek(16);

  for (;;) {
    if (png->File->TotalSize()-png->File->Tell() <= (int)chunk.Size+4) break;
    png->File->Seek(png->File->Tell()+chunk.Size+4);
    //if (png->File->AtEnd()) break;
    if (png->File->TotalSize()-png->File->Tell() < 12) break;

    // if the file ended before an IEND was encountered, it's not a PNG
    png->File->Serialise(&data, 8);
    if (png->File->IsError()) break;
    // an IEND chunk terminates the PNG and must be empty
    if (data[1] == MAKE_ID('I','E','N','D')) {
      if (data[0] == 0 && sawIDAT) return png;
      break;
    }
    // a PNG must include an IDAT chunk
    if (data[1] == MAKE_ID('I','D','A','T')) sawIDAT = true;
    chunk.ID = data[1];
    chunk.Offset = (vuint32)png->File->Tell();
    chunk.Size = BigLong((unsigned int)data[0]);
    png->Chunks.Append(chunk);

    if (data[1] == MAKE_ID('P','L','T','E')) {
      if (chunk.Size%3 != 0) break; // invalid chunk
      if (chunk.Size > 768) break; // invalid chunk
      png->File->Serialise(png->pal, (int)chunk.Size);
      if (png->File->IsError()) break;
      chunk.Size = 0; // don't try to seek past its contents again
    }

    if (data[1] == MAKE_ID('t','R','N','S')) {
      vuint16 v;
      bool error = false;
      switch (png->colortype) {
        case 0: // grayscale
          if (chunk.Size != 2) { error = true; break; } // invalid chunk
          if (!readU16BE(png->File, &v)) { error = true; break; }
          memset(png->trans, 255, sizeof(png->trans));
          png->trans[convertBPP(v, png->bitdepth)] = 0;
          chunk.Size = 0; // don't try to seek past its contents again
          break;
        case 3: // paletted
          png->hasTrans = true;
          if (chunk.Size > 256) { error = true; break; } // invalid chunk
          memset(png->trans, 255, sizeof(png->trans));
          if (chunk.Size) {
            png->File->Serialise(png->trans, (int)chunk.Size);
            if (png->File->IsError()) { error = true; break; }
          }
          chunk.Size = 0; // don't try to seek past its contents again
          break;
        case 2: // RGB
          if (chunk.Size != 3*2) { error = true; break; } // invalid chunk
          png->hasTrans = true;
          if (!readU16BE(png->File, &v)) { error = true; break; }
          png->tR = convertBPP(v, png->bitdepth);
          if (!readU16BE(png->File, &v)) { error = true; break; }
          png->tG = convertBPP(v, png->bitdepth);
          if (!readU16BE(png->File, &v)) { error = true; break; }
          png->tB = convertBPP(v, png->bitdepth);
          chunk.Size = 0; // don't try to seek past its contents again
          break;
      }
      if (error) break;
    }

    // if this is a text chunk, also record its contents
#ifdef MINIPNG_LOAD_TEXT_CHUNKS
    if (data[1] == MAKE_ID('t','E','X','t')) {
      char *str = new char[chunk.Size+1];
      if (chunk.Size) {
        png->File->Serialise(str, (int)chunk.Size);
        if (png->File->IsError()) {
          delete[] str;
          break;
        }
      }
      str[chunk.Size] = 0;
      png->TextChunks.Append(str);
      chunk.Size = 0; // don't try to seek past its contents again
    }
#endif
  }

  //filer = std::move(png->File); // need to get the reader back if this function failed.
  delete png;
  return nullptr;
}


//==========================================================================
//
// M_FreePNG
//
// Just deletes the PNGHandle. The file is not closed.
//
//==========================================================================
void M_FreePNG (PNGHandle *png) {
  delete png;
}


//==========================================================================
//
// ReadIDAT
//
// Reads image data out of a PNG
//
//==========================================================================
bool M_ReadIDAT (VStream &file, vuint8 *buffer, int width, int height, int pitch,
                 vuint8 bitdepth, vuint8 colortype, vuint8 interlace, unsigned int chunklen)
{
  // uninterlaced images are treated as a conceptual eighth pass by these tables
  static const vuint8 passwidthshift[8] =  { 3, 3, 2, 2, 1, 1, 0, 0 };
  static const vuint8 passheightshift[8] = { 3, 3, 3, 2, 2, 1, 1, 0 };
  static const vuint8 passrowoffset[8] =   { 0, 0, 4, 0, 2, 0, 1, 0 };
  static const vuint8 passcoloffset[8] =   { 0, 4, 0, 2, 0, 1, 0, 0 };

  Byte *inputLine, *prev, *curr, *adam7buff[3], *bufferend;
  Byte chunkbuffer[4096];
  z_stream stream;
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

  inputLine = (Byte *)alloca(i);
  adam7buff[0] = inputLine+4+bytesPerRowOut;
  adam7buff[1] = adam7buff[0]+bytesPerRowOut;
  adam7buff[2] = adam7buff[1]+bytesPerRowOut;
  bufferend = buffer+pitch*height;

  stream.next_in = Z_NULL;
  stream.avail_in = 0;
  stream.zalloc = Z_NULL;
  stream.zfree = Z_NULL;
  err = inflateInit (&stream);
  if (err != Z_OK) return false;

  lastIDAT = false;
  initpass = true;
  pass = (interlace ? 0 : 7);

  // silence GCC warnings
  // due to initpass being true, these will be set before they're used, but it doesn't know that
  curr = prev = 0;
  passwidth = passpitch = bytesPerRowIn = 0;
  passbuff = 0;

  while (err != Z_STREAM_END && pass < 8-interlace) {
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
      stream.avail_in = (uInt)rd;
      //stream.avail_in = (uInt)file.Read (chunkbuffer, MIN<vuint32>(chunklen,sizeof(chunkbuffer)));
      chunklen -= stream.avail_in;
    }

    err = inflate (&stream, Z_SYNC_FLUSH);
    if (err != Z_OK && err != Z_STREAM_END) {
      // something unexpected happened
      inflateEnd(&stream);
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

  inflateEnd(&stream);

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
// MakeChunk
//
// Prepends the chunk length and type and appends the chunk's CRC32.
// There must be 8 bytes available before the chunk passed and 4 bytes
// after the chunk.
//
//==========================================================================
static inline void MakeChunk (void *where, vuint32 type, size_t len) {
  vuint8 *const data = (vuint8 *)where;
  *(vuint32 *)(data-8) = BigLong ((unsigned int)len);
  *(vuint32 *)(data-4) = type;
  *(vuint32 *)(data+len) = BigLong ((unsigned int)CalcCRC32 (data-4, (unsigned int)(len+4)));
}


//==========================================================================
//
// StuffPalette
//
// Converts 256 4-byte palette entries to 3 bytes each.
//
//==========================================================================
static void StuffPalette (const PalEntry *from, vuint8 *to) {
  for (int i = 256; i > 0; --i) {
    to[0] = from->r;
    to[1] = from->g;
    to[2] = from->b;
    from += 1;
    to += 3;
  }
}


//==========================================================================
//
// M_SaveBitmap
//
// Given a bitmap, creates one or more IDAT chunks in the given file.
// Returns true on success.
//
//==========================================================================
//#define MAXWIDTH  (12000)
bool M_SaveBitmap (const vuint8 *from, ESSType color_type, int width, int heightOrig, int pitch, VStream *file) {
  //Byte temprow[1][1+MAXWIDTH*4];
  Byte buffer[PNG_WRITE_SIZE];
  z_stream stream;
  int err;
  int y;

  Byte *temprow = new Byte[width*4+8];
  int height = (heightOrig < 0 ? -heightOrig : heightOrig);

  if (heightOrig < 0) from += pitch*(height-1);

  stream.next_in = Z_NULL;
  stream.avail_in = 0;
  stream.zalloc = Z_NULL;
  stream.zfree = Z_NULL;
  err = deflateInit(&stream, png_level);

  if (err != Z_OK) return false;

  y = height;
  stream.next_out = buffer;
  stream.avail_out = sizeof(buffer);

  temprow[0] = 0; // always use filter type 0

  while (y-- > 0 && err == Z_OK) {
    switch (color_type) {
      case SS_PAL:
        memcpy(&temprow[1], from, width);
        stream.next_in = temprow;
        stream.avail_in = width+1;
        break;
      case SS_RGB:
        memcpy(&temprow[1], from, width*3);
        stream.next_in = temprow;
        stream.avail_in = width*3+1;
        break;
      case SS_BGRA:
        for (int x = 0; x < width; ++x) {
          temprow[1+x*4+0] = from[x*4+2];
          temprow[1+x*4+1] = from[x*4+1];
          temprow[1+x*4+2] = from[x*4+0];
          temprow[1+x*4+3] = from[x*4+3];
        }
        stream.next_in = temprow;
        stream.avail_in = width*4+1;
        break;
      case SS_RGBA:
        for (int x = 0; x < width; ++x) {
          temprow[1+x*4+0] = from[x*4+0];
          temprow[1+x*4+1] = from[x*4+1];
          temprow[1+x*4+2] = from[x*4+2];
          temprow[1+x*4+3] = from[x*4+3];
        }
        stream.next_in = temprow;
        stream.avail_in = width*4+1;
        break;
    }

    if (heightOrig > 0) from += pitch; else from -= pitch;

    err = deflate(&stream, /*(y == 0) ? Z_FINISH :*/ 0);
    if (err != Z_OK) break;
    while (stream.avail_out == 0) {
      if (!WriteIDAT(file, buffer, sizeof(buffer))) return false;
      stream.next_out = buffer;
      stream.avail_out = sizeof(buffer);
      if (stream.avail_in != 0) {
        err = deflate(&stream, /*(y == 0) ? Z_FINISH :*/ 0);
        if (err != Z_OK) break;
      }
    }
  }

  while (err == Z_OK) {
    err = deflate(&stream, Z_FINISH);
    if (err != Z_OK) break;
    if (stream.avail_out == 0) {
      if (!WriteIDAT(file, buffer, sizeof(buffer))) return false;
      stream.next_out = buffer;
      stream.avail_out = sizeof(buffer);
    }
  }

  deflateEnd(&stream);

  if (err != Z_STREAM_END) return false;

  if (!WriteIDAT(file, buffer, sizeof(buffer)-stream.avail_out)) return false;

  // write IEND
  vuint32 foo[2], crc;

  foo[0] = 0;
  foo[1] = MAKE_ID('I','E','N','D');
  crc = CalcCRC32((vuint8 *)&foo[1], 4);
  crc = BigLong(crc);

  file->Serialise(foo, 8);
  if (file->IsError()) return false;
  file->Serialise(&crc, 4);
  return !file->IsError();
}


//==========================================================================
//
// WriteIDAT
//
// Writes a single IDAT chunk to the file. Returns true on success.
//
//==========================================================================
static bool WriteIDAT (VStream *file, const vuint8 *data, int len) {
  vuint32 foo[2], crc;

  foo[0] = BigLong(len);
  foo[1] = MAKE_ID('I','D','A','T');
  crc = CalcCRC32((vuint8 *)&foo[1], 4);
  crc = BigLong((unsigned int)AddCRC32(crc, data, len));

  file->Serialise(foo, 8);
  if (file->IsError()) return false;
  if (len) {
    file->Serialise(data, (int)len);
    if (file->IsError()) return false;
  }
  file->Serialise(&crc, 4);
  return !file->IsError();
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
