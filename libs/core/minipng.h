#ifndef MINI_PNG_H
#define MINI_PNG_H
/*
** m_png.h
**
**---------------------------------------------------------------------------
** Copyright 2002-2005 Randy Heit
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

//#include <stdio.h>


extern int png_level;
extern float png_gamma;


class VStream;
// PNG Writing --------------------------------------------------------------

// Screenshot buffer image data types
enum ESSType {
  SS_PAL,
  SS_RGB,
  SS_BGRA,
  SS_RGBA,
};

struct PalEntry {
  PalEntry () {}
  PalEntry (vuint32 argb) { d = argb; }

  inline operator vuint32 () const { return d; }

  inline void SetRGB (PalEntry other) { d = other.d & 0xffffff; }

  inline PalEntry Modulate (PalEntry other) const {
    if (isWhite()) {
      return other;
    } else if (other.isWhite()) {
      return *this;
    } else {
      other.r = (r*other.r)/255;
      other.g = (g*other.g)/255;
      other.b = (b*other.b)/255;
      return other;
    }
  }

  inline int Luminance () const { return (r*77+g*143+b*37)>>8; }

  // this for 'nocoloredspritelighting' and not the same as desaturation.
  // the normal formula results in a value that's too dark.
  inline void Decolorize () {
    int v = (r+g+b);
    r = g = b = ((255*3)+v+v)/9;
  }

  inline bool isBlack() const { return (d & 0xffffff) == 0; }

  inline bool isWhite() const { return (d & 0xffffff) == 0xffffff; }

  inline PalEntry &operator = (vuint32 other) { d = other; return *this; }
  inline PalEntry InverseColor() const { PalEntry nc; nc.a = a; nc.r = 255-r; nc.g = 255-g; nc.b = 255-b; return nc; }
#ifdef __BIG_ENDIAN__
  PalEntry (vuint8 ir, vuint8 ig, vuint8 ib) : a(0), r(ir), g(ig), b(ib) {}
  PalEntry (vuint8 ia, vuint8 ir, vuint8 ig, vuint8 ib) : a(ia), r(ir), g(ig), b(ib) {}
  union {
    struct {
      vuint8 a, r, g, b;
    };
    vuint32 d;
  };
#else
  PalEntry (vuint8 ir, vuint8 ig, vuint8 ib) : b(ib), g(ig), r(ir), a(0) {}
  PalEntry (vuint8 ia, vuint8 ir, vuint8 ig, vuint8 ib) : b(ib), g(ig), r(ir), a(ia) {}
  union {
    struct {
      vuint8 b, g, r, a;
    };
    vuint32 d;
  };
#endif
};

/*
inline int Luminance(int r, int g, int b)
{
  return (r * 77 + g * 143 + b * 37) >> 8;
}
*/


// Start writing an 8-bit palettized PNG file.
// The passed file should be a newly created file.
// This function writes the PNG signature and the IHDR, gAMA, PLTE, and IDAT
// chunks.
// if height < 0, the image is from bottom to top
bool M_CreatePNG (VStream *file, const vuint8 *buffer, const PalEntry *pal,
                  ESSType color_type, int width, int height, int pitch, float gamma);

// Creates a grayscale 1x1 PNG file. Used for savegames without savepics.
bool M_CreateDummyPNG (VStream *file);

// Appends any chunk to a PNG file started with M_CreatePNG.
bool M_AppendPNGChunk (VStream *file, vuint32 chunkID, const vuint8 *chunkData, vuint32 len);

// Adds a tEXt chunk to a PNG file started with M_CreatePNG.
bool M_AppendPNGText (VStream *file, const char *keyword, const char *text);

// Appends the IEND chunk to a PNG file.
bool M_FinishPNG (VStream *file);

// if height < 0, the image is from bottom to top
bool M_SaveBitmap (const vuint8 *from, ESSType color_type, int width, int height, int pitch, VStream *file);


// PNG Reading --------------------------------------------------------------
struct PNGHandle {
  struct Chunk {
    vuint32 ID;
    vuint32 Offset;
    vuint32 Size;
  };

  VStream *File; // doesn't own
  TArray<Chunk> Chunks;
#ifdef MINIPNG_LOAD_TEXT_CHUNKS
  TArray<char *> TextChunks;
#endif
  vuint8 pal[768]; //RGB
  vuint8 trans[768]; //alpha for palette (color 0 will be transparent for paletted images, as DooM does it this way)
  vuint8 tR, tG, tB; // transparent color for RGB images
  bool hasTrans;
  unsigned int ChunkPt;

  int width;
  int height;
  vuint8 bitdepth;
  vuint8 colortype;
  vuint8 interlace;
  vuint8 *pixbuf;

  PNGHandle (VStream *file);
  ~PNGHandle ();

  bool loadIDAT ();

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
PNGHandle *M_VerifyPNG (VStream *file);

// Finds a chunk in a PNG file. The file pointer will be positioned at the
// beginning of the chunk data, and its length will be returned. A return
// value of 0 indicates the chunk was either not present or had 0 length.
unsigned int M_FindPNGChunk (PNGHandle *png, vuint32 chunkID);

// Finds a chunk in the PNG file, starting its search at whatever chunk
// the file pointer is currently positioned at.
unsigned int M_NextPNGChunk (PNGHandle *png, vuint32 chunkID);

bool M_FindPNGIDAT (PNGHandle *png, vuint32 *chunkLen);

#ifdef MINIPNG_LOAD_TEXT_CHUNKS
// Finds a PNG text chunk with the given signature and returns a pointer
// to a nullptr-terminated string if present. Returns nullptr on failure.
// (Note: tEXt, not zTXt.)
char *M_GetPNGText (PNGHandle *png, const char *keyword);
bool M_GetPNGText (PNGHandle *png, const char *keyword, char *buffer, size_t buffsize);
#endif

// The file must be positioned at the start of the first IDAT. It reads
// image data into the provided buffer. Returns true on success.
bool M_ReadIDAT (VStream &file, vuint8 *buffer, int width, int height, int pitch,
                 vuint8 bitdepth, vuint8 colortype, vuint8 interlace, unsigned int idatlen);


#endif
