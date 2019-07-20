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
#include <time.h>

#include "zip.h"
#include "cmdlib.h"
#include "wadlib.h"
#include "scrlib.h"
#include "imglib.h"

#include "../../libs/core/core.h"

using namespace VavoomUtils;


// posts are runs of non masked source pixels
struct post_t {
  vuint8 topdelta; // -1 is the last post in a column
  vuint8 length; // length data bytes follows
};

// column_t is a list of 0 or more post_t, (byte)-1 terminated
typedef post_t column_t;

// Patches.
// A patch holds one or more columns.
// Patches are used for sprites and all masked pictures, and we compose
// textures from the TEXTURE1/2 lists of patches.
struct patch_t {
  short width; // bounding box size
  short height;
  short leftoffset; // pixels to the left of origin
  short topoffset; // pixels below the origin
  int columnofs[8]; // only [width] used
  // the [0] is &columnofs[width]
};

struct vpic_t {
  char magic[4];
  short width;
  short height;
  vuint8 bpp;
  vuint8 reserved[7];
};

struct RGB_MAP {
  vuint8 data[32][32][32];
};

struct fon1_header_t {
  char Id[4];
  vuint16 Width;
  vuint16 Height;
};

struct fon2_header_t {
  char Id[4];
  vuint16 Height;
  vuint8 FirstChar;
  vuint8 LastChar;
  vuint8 FixedWidth;
  vuint8 RescalePalette;
  vuint8 ActiveColors;
  vuint8 KerningFlag;
};

struct fon2_char_t {
  int Width;
  int Height;
  vuint8 *Data;
  vint8 *ComprData;
  int ComprDataSize;
};


static TOWadFile outwad;
static zipFile Zip;

static char basepath[256];
static char srcpath[256];
static char destpath[256];

static RGB_MAP rgb_table;
static bool rgb_table_created = false;

static char lumpname[256];
static char destfile[1024];

static bool Fon2ColorsUsed[256] = {0};


//==========================================================================
//
//  makecol8
//
//==========================================================================
static inline int makecol8 (int r, int g, int b) {
  return rgb_table.data[r>>3][g>>3][b>>3];
}


//==========================================================================
//
//  rgbDistanceSquared
//
//  see https://www.compuphase.com/cmetric.htm
//
//==========================================================================
static inline __attribute__((unused)) vint32 rgbDistanceSquared (vuint8 r0, vuint8 g0, vuint8 b0, vuint8 r1, vuint8 g1, vuint8 b1) {
  const vint32 rmean = ((vint32)r0+(vint32)r1)/2;
  const vint32 r = (vint32)r0-(vint32)r1;
  const vint32 g = (vint32)g0-(vint32)g1;
  const vint32 b = (vint32)b0-(vint32)b1;
  return (((512+rmean)*r*r)/256)+4*g*g+(((767-rmean)*b*b)/256);
}


//==========================================================================
//
//  my_bestfit_color
//
//==========================================================================
static int my_bestfit_color (int r, int g, int b) {
  int best_color = 0;
  int best_dist = 0x7fffffff;

  if (r < 0) r = 0; else if (r > 255) r = 255;
  if (g < 0) g = 0; else if (g > 255) g = 255;
  if (b < 0) b = 0; else if (b > 255) b = 255;

  for (int i = 1; i < 256; ++i) {
    /*
    int dist = (ImgPal[i].r-r)*(ImgPal[i].r-r)+
      (ImgPal[i].g-g)*(ImgPal[i].g-g)+
      (ImgPal[i].b-b)*(ImgPal[i].b-b);
    */
    int dist = rgbDistanceSquared(r, g, b, ImgPal[i].r, ImgPal[i].g, ImgPal[i].b);
    if (dist < best_dist) {
      best_color = i;
      best_dist = dist;
      if (!dist) break;
    }
  }
  return best_color;
}


//==========================================================================
//
//  SetupRGBTable
//
//==========================================================================
static void SetupRGBTable () {
  if (rgb_table_created) return;
  for (int r = 0; r < 32; ++r) {
    for (int g = 0; g < 32; ++g) {
      for (int b = 0; b < 32; ++b) {
        rgb_table.data[r][g][b] = my_bestfit_color(
          (int)(r*255.0/31.0+0.5),
          (int)(g*255.0/31.0+0.5),
          (int)(b*255.0/31.0+0.5));
      }
    }
  }
  rgb_table_created = true;
}


//==========================================================================
//
//  fn
//
//==========================================================================
static char *fn (const char *name) {
  static char filename[8192];
  if (name[0] == '/' || name[0] == '\\' || name[1] == ':') {
    // absolute path
    strcpy(filename, name);
  } else if (destpath[0]) {
    snprintf(filename, sizeof(filename)-256, "%s%s", destpath, name);
    FILE *f = fopen(filename, "rb");
    if (f) {
      fclose(f);
    } else {
      snprintf(filename, sizeof(filename)-256, "%s%s", srcpath, name);
    }
  } else {
    snprintf(filename, sizeof(filename)-256, "%s%s", srcpath, name);
  }
  return filename;
}


//==========================================================================
//
//  AddToZip
//
//==========================================================================
static void AddToZip (const char *Name, void *Data, size_t Size) {
  zip_fileinfo zi;
  memset(&zi, 0, sizeof(zi));

  // set file time to current time
  time_t CurTime = 0;
  time(&CurTime);
  struct tm* LTime = localtime(&CurTime);
  if (LTime) {
    zi.tmz_date.tm_sec = LTime->tm_sec;
    zi.tmz_date.tm_min = LTime->tm_min;
    zi.tmz_date.tm_hour = LTime->tm_hour;
    zi.tmz_date.tm_mday = LTime->tm_mday;
    zi.tmz_date.tm_mon = LTime->tm_mon;
    zi.tmz_date.tm_year = LTime->tm_year;
  }

  // open file
  if (zipOpenNewFileInZip(Zip, Name, &zi, nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED, Z_BEST_COMPRESSION) != ZIP_OK) {
    Error("Failed to open file in ZIP");
  }

  // write to it
  if (zipWriteInFileInZip(Zip, Data, Size) != ZIP_OK) {
    Error("Failed to write file in ZIP");
  }

  // close it
  if (zipCloseFileInZip(Zip) != ZIP_OK) {
    Error("Failed to close file in ZIP");
  }
}


//==========================================================================
//
//  GetPixel
//
//==========================================================================
static inline vuint8 GetPixel (int x, int y) {
  return ImgData[x+y*ImgWidth];
}


//==========================================================================
//
//  GetPixelRGB
//
//==========================================================================
static inline rgba_t &GetPixelRGB (int x, int y) {
  return ((rgba_t *)ImgData)[x+y*ImgWidth];
}


//==========================================================================
//
//  AddWadFile
//
//==========================================================================
static void AddWadFile (const char *name) {
  TIWadFile inwad;
  inwad.Open(name);
  for (int i = 0; i < inwad.numlumps; ++i) {
    void *data = inwad.GetLump(i);
    outwad.AddLump(inwad.LumpName(i), data, inwad.LumpSize(i));
    Z_Free(data);
  }
  inwad.Close();
}


//==========================================================================
//
//  AddWad
//
//  $WAD filename
//
//==========================================================================
static void AddWad (const char *name) {
  if (Zip) Error("$wad cannot be used for ZIP file");
  char *filename = fn(name);
  DefaultExtension(filename, 256, ".wad"); // arbitrary limit
  AddWadFile(filename);
}


//==========================================================================
//
//  AddMap
//
//  $MAP filename
//
//==========================================================================
static void AddMap (const char *name) {
  if (Zip) Error("$map cannot be used for ZIP file");

  char *filename = fn(name);
  DefaultExtension(filename, 256, ".wad"); // arbitrary limit
  AddWadFile(filename);

  StripExtension(filename);
  strcat(filename, ".gwa");
  FILE *ff = fopen(filename, "rb");
  if (ff) {
    fclose(ff);
    AddWadFile(filename);
  }
}


//==========================================================================
//
//  LoadImage
//
//  $LOAD image
//
//==========================================================================
static void LoadImage () {
  SC_MustGetString();
  DestroyImage();
  LoadImage(fn(sc_String));
  rgb_table_created = false;
}


//==========================================================================
//
//  GrabRGBTable
//
//==========================================================================
static void GrabRGBTable () {
  vuint8 tmp[32*32*32+4];

  SetupRGBTable();
  memcpy(tmp, &rgb_table, 32*32*32);
  tmp[32*32*32] = 0;
  if (Zip) {
    AddToZip(lumpname, tmp, 32*32*32+1);
  } else {
    outwad.AddLump(lumpname, tmp, 32*32*32+1);
  }
}


//==========================================================================
//
//  GrabTranslucencyTable
//
//  lumpname TINTTAB amount
//
//==========================================================================
static void GrabTranslucencyTable () {
  vuint8 table[256*256];
  vuint8 temp[768];

  SC_MustGetNumber();
  int transluc = sc_Number;

  SetupRGBTable();

  vuint8 *p = table;
  for (int i = 0; i < 256; ++i) {
    temp[i*3+0] = ImgPal[i].r*transluc/100;
    temp[i*3+1] = ImgPal[i].g*transluc/100;
    temp[i*3+2] = ImgPal[i].b*transluc/100;
  }
  for (int i = 0; i < 256; ++i) {
    int r = ImgPal[i].r*(100-transluc)/100;
    int g = ImgPal[i].g*(100-transluc)/100;
    int b = ImgPal[i].b*(100-transluc)/100;
    vuint8 *q = temp;
    for (int j = 0; j < 256; ++j) {
      *(p++) = makecol8(r+q[0], g+q[1], b+q[2]);
      q += 3;
    }
  }

  if (Zip) {
    AddToZip(lumpname, table, 256*256);
  } else {
    outwad.AddLump(lumpname, table, 256*256);
  }
}


//==========================================================================
//
//  GrabScaleMap
//
//  lumpname SCALEMAP r g b
//
//==========================================================================
static void GrabScaleMap () {
  vuint8 map[256];

  SC_MustGetFloat();
  double r = sc_Float;
  SC_MustGetFloat();
  double g = sc_Float;
  SC_MustGetFloat();
  double b = sc_Float;

  SetupRGBTable();
  for (int i = 0; i < 256; ++i) {
    double col = (ImgPal[i].r*0.3+ImgPal[i].g*0.5+ImgPal[i].b*0.2);
    map[i] = makecol8((int)(r*col), (int)(g*col), (int)(b*col));
  }

  if (Zip) {
    AddToZip(lumpname, map, 256);
  } else {
    outwad.AddLump(lumpname, map, 256);
  }
}


//==========================================================================
//
//  GrabRaw
//
//  lumpname RAW x y width height
//
//==========================================================================
static void GrabRaw () {
  SC_MustGetNumber();
  int x1 = sc_Number;
  SC_MustGetNumber();
  int y1 = sc_Number;
  SC_MustGetNumber();
  int w = sc_Number;
  SC_MustGetNumber();
  int h = sc_Number;
  int x2 = x1+w;
  int y2 = y1+h;

  vuint8 *data = (vuint8 *)Z_Malloc(w*h);
  vuint8 *dst = data;
  for (int y = y1; y < y2; ++y) {
    for (int x = x1; x < x2; ++x) {
      *dst++ = GetPixel(x, y);
    }
  }

  if (Zip) {
    AddToZip(lumpname, data, w*h);
  } else {
    outwad.AddLump(lumpname, data, w*h);
  }

  Z_Free(data);
}


//==========================================================================
//
//  GrabPatch
//
//  lumpname PATCH x y width height leftoffset topoffset
//
//==========================================================================
static void GrabPatch () {
  SC_MustGetNumber();
  int x1 = sc_Number;
  SC_MustGetNumber();
  int y1 = sc_Number;
  SC_MustGetNumber();
  int w = sc_Number;
  SC_MustGetNumber();
  int h = sc_Number;
  SC_MustGetNumber();
  int leftoffset = sc_Number;
  SC_MustGetNumber();
  int topoffset = sc_Number;

  int TransColor = 0;
  patch_t *Patch = (patch_t *)Z_Malloc(8+4*w+w*h*4);
  Patch->width = LittleShort(w);
  Patch->height = LittleShort(h);
  Patch->leftoffset = LittleShort(leftoffset);
  Patch->topoffset = LittleShort(topoffset);
  column_t *Col = (column_t *)&Patch->columnofs[w];

  for (int x = 0; x < w; ++x) {
    Patch->columnofs[x] = LittleLong((vuint8*)Col-(vuint8*)Patch);
    int y = 0;
    int PrevTop = -1;
    while (y < h) {
      // skip transparent pixels
      if (GetPixel(x1+x, y1+y) == TransColor) {
        ++y;
        continue;
      }
      // grab a post
      if (y < 255) {
        Col->topdelta = y;
      } else {
        // tall patch
        while (y-PrevTop > 254) {
          // insert empty post
          Col->topdelta = 254;
          Col = (column_t*)((vuint8*)Col+4);
          if (PrevTop < 254) {
            PrevTop = 254;
          } else {
            PrevTop += 254;
          }
        }
        Col->topdelta = y-PrevTop;
      }
      PrevTop = y;
      vuint8* Pixels = (vuint8*)Col+3;
      while (y < h && Col->length < 255 && GetPixel(x1+x, y1+y) != TransColor) {
        Pixels[Col->length] = GetPixel(x1+x, y1+y);
        ++Col->length;
        ++y;
      }
      Col = (column_t *)((vuint8 *)Col+Col->length+4);
    }

    // add terminating post
    Col->topdelta = 0xff;
    Col = (column_t *)((vuint8 *)Col+4);
  }

  if (Zip) {
    AddToZip(lumpname, Patch, (vuint8*)Col-(vuint8*)Patch);
  } else {
    outwad.AddLump(lumpname, Patch, (vuint8*)Col-(vuint8*)Patch);
  }

  Z_Free(Patch);
}


//==========================================================================
//
//  GrabPic
//
//  lumpname PIC x y width height
//
//==========================================================================
static void GrabPic () {
  SC_MustGetNumber();
  int x1 = sc_Number;
  SC_MustGetNumber();
  int y1 = sc_Number;
  SC_MustGetNumber();
  int w = sc_Number;
  SC_MustGetNumber();
  int h = sc_Number;
  int x2 = x1+w;
  int y2 = y1+h;

  vpic_t *pic = (vpic_t *)Z_Malloc(sizeof(vpic_t)+w*h);
  memcpy(pic->magic, "VPIC", 4);
  pic->width = LittleShort(w);
  pic->height = LittleShort(h);
  pic->bpp = 8;
  vuint8 *dst = (vuint8 *)(pic+1);
  for (int y = y1; y < y2; ++y) {
    for (int x = x1; x < x2; ++x) {
      *dst++ = GetPixel(x, y);
    }
  }

  if (Zip) {
    AddToZip(lumpname, pic, sizeof(vpic_t)+w*h);
  } else {
    outwad.AddLump(lumpname, pic, sizeof(vpic_t)+w*h);
  }

  Z_Free(pic);
}


//==========================================================================
//
//  GrabPic15
//
//  lumpname PIC15 x y width height
//
//==========================================================================
static void GrabPic15 () {
  SC_MustGetNumber();
  int x1 = sc_Number;
  SC_MustGetNumber();
  int y1 = sc_Number;
  SC_MustGetNumber();
  int w = sc_Number;
  SC_MustGetNumber();
  int h = sc_Number;
  int x2 = x1+w;
  int y2 = y1+h;

  ConvertImageTo32Bit();
  vpic_t *pic = (vpic_t *)Z_Malloc(sizeof(vpic_t)+w*h*2);
  memcpy(pic->magic, "VPIC", 4);
  pic->width = LittleShort(w);
  pic->height = LittleShort(h);
  pic->bpp = 15;
  vuint8 *dst = (vuint8 *)(pic+1);
  for (int y = y1; y < y2; ++y) {
    for (int x = x1; x < x2; ++x) {
      const rgba_t &p = GetPixelRGB(x, y);
      int c;
      if (p.r == 0 && (p.g&0xf8) == 0xf8 && (p.b&0xf8) == 0xf8) {
        c = 0x8000;
      } else {
        c = ((p.r<<7)&0x7c00)|((p.g<<2)&0x03e0)|((p.b>>3)&0x001f);
      }
      dst[0] = vuint8(c);
      dst[1] = vuint8(c>>8);
      dst += 2;
    }
  }

  if (Zip) {
    AddToZip(lumpname, pic, sizeof(vpic_t)+w*h*2);
  } else {
    outwad.AddLump(lumpname, pic, sizeof(vpic_t)+w*h*2);
  }

  Z_Free(pic);
}


//==========================================================================
//
//  CompressChar
//
//==========================================================================
static vint8 *CompressChar (vuint8 *Src, vint8 *Dst, int Size) {
  vuint8 *SrcEnd = Src+Size;
  do {
    if (SrcEnd-Src < 2) {
      // write last pixel
      *Dst++ = 0;
      *Dst++ = *Src++;
    } else if (Src[0] != Src[1]) {
      // write different pixels
      int Len = 1;
      while (Len < 128 && Len < SrcEnd-Src && (Len+3 > SrcEnd-Src || Src[Len] != Src[Len+1] || Src[Len] != Src[Len+2])) ++Len;
      *Dst++ = Len-1;
      for (int i = 0; i < Len; ++i) *Dst++ = *Src++;
    } else {
      // write equal pixels
      int Len = 2;
      while (Len < 128 && Len < SrcEnd-Src && Src[Len-1] == Src[Len]) ++Len;
      *Dst++ = -Len+1;
      *Dst++ = *Src;
      Src += Len;
    }
  } while (Src < SrcEnd);
  return Dst;
}


//==========================================================================
//
//  GrabFon1
//
//  lumpname FON1
//
//==========================================================================
static void GrabFon1 () {
  // dimensions of a character
  int CharW = ImgWidth/16;
  int CharH = ImgHeight/16;
  // calculate maximum size needed for a single character
  int MaxCharBytes = CharW*CharH+(CharW*CharH+127)/128;

  // allocate memory and fill in header
  fon1_header_t *Font = (fon1_header_t *)Z_Malloc(sizeof(fon1_header_t)+MaxCharBytes*256);
  Font->Id[0] = 'F';
  Font->Id[1] = 'O';
  Font->Id[2] = 'N';
  Font->Id[3] = '1';
  Font->Width = LittleShort(CharW);
  Font->Height = LittleShort(CharH);

  // process characters
  vuint8* CharBuf = (vuint8 *)Z_Malloc(CharW*CharH);
  vint8* pDst = (vint8 *)(Font+1);
  for (int i = 0; i < 256; ++i) {
    int StartX = (i%16)*CharW;
    int StartY = (i/16)*CharH;
    for (int ch = 0; ch < CharH; ++ch) {
      for (int cw = 0; cw < CharW; ++cw) {
        CharBuf[ch*CharW+cw] = GetPixel(StartX+cw, StartY+ch);
      }
    }
    pDst = CompressChar(CharBuf, pDst, CharW*CharH);
  }

  // write lump and free memory
  if (Zip) {
    AddToZip(lumpname, Font, pDst-(vint8*)Font);
  } else {
    outwad.AddLump(lumpname, Font, pDst-(vint8*)Font);
  }

  Z_Free(Font);
  Z_Free(CharBuf);
}


//==========================================================================
//
//  Fon2PalCmp
//
//==========================================================================
static int Fon2PalCmp (const void *v1, const void *v2) {
  int Idx1 = *(const int *)v1;
  int Idx2 = *(const int *)v2;
  // move unused colors to the end
  if (!Fon2ColorsUsed[Idx1]) return 1;
  if (!Fon2ColorsUsed[Idx2]) return -1;
  float Lum1 = ImgPal[Idx1].r*0.299+ImgPal[Idx1].g*0.587+ImgPal[Idx1].b*0.114;
  float Lum2 = ImgPal[Idx2].r*0.299+ImgPal[Idx2].g*0.587+ImgPal[Idx2].b*0.114;
  if (Lum1 < Lum2) return -1;
  if (Lum1 > Lum2) return 1;
  return 0;
}


//==========================================================================
//
//  GrabFon2
//
//  lumpname FON2
//
//==========================================================================
static void GrabFon2 () {
  // process characters
  fon2_char_t *Chars[256];
  memset(Chars, 0, sizeof(Chars));
  memset(Fon2ColorsUsed, 0, sizeof(Fon2ColorsUsed));
  Fon2ColorsUsed[0] = true;
  int CharNum = ' ';
  for (int StartY = 1; StartY < ImgHeight; ) {
    //  Find bottom line.
    int EndY = StartY+1;
    while (EndY < ImgHeight && GetPixel(1, EndY) != 255) ++EndY;
    if (EndY >= ImgHeight) break; // reached end of the image
    for (int StartX = 1; StartX < ImgWidth; ) {
      // find right line
      int EndX = StartX;
      while (EndX < ImgWidth && GetPixel(EndX, StartY) != 255) ++EndX;
      if (EndX >= ImgWidth) break; // reached end of the image
      if (EndX != StartX) {
        fon2_char_t *Chr = new fon2_char_t;
        Chars[CharNum] = Chr;
        Chr->Width = EndX-StartX;
        Chr->Height = EndY-StartY;
        Chr->Data = (vuint8 *)Z_Malloc(Chr->Width*Chr->Height);
        vuint8 *pData = Chr->Data;
        for (int ch = StartY; ch < EndY; ++ch) {
          for (int cw = StartX; cw < EndX; ++cw) {
            *pData = GetPixel(cw, ch);
            Fon2ColorsUsed[*pData] = true;
            ++pData;
          }
        }
      }
      ++CharNum;
      StartX = EndX+1;
    }
    StartY = EndY+1;
  }

  // sort colors by their luminosity
  int SortIdx[256];
  for (int i = 0; i < 256; ++i) SortIdx[i] = i;
  qsort(SortIdx+1, 255, sizeof(int), Fon2PalCmp);

  // build palette containing only used colors
  rgb_t Pal[256];
  vuint8 Remap[256];
  memset(Remap, 0, sizeof(Remap)); //k8: why not?
  int NumColors = 0;
  for (int i = 0; i < 256; ++i) {
    int Idx = SortIdx[i];
    if (Fon2ColorsUsed[Idx]) {
      Pal[NumColors] = ImgPal[Idx];
      Remap[Idx] = NumColors;
      ++NumColors;
    }
  }

  // remap image data to new palette
  for (int i = 0; i < 256; ++i) {
    fon2_char_t* Chr = Chars[i];
    if (!Chr) continue;
    for (int j = 0; j < Chr->Width*Chr->Height; ++j) {
      Chr->Data[j] = Remap[(vuint8)(Chr->Data[j]&0xff)];
    }
  }

  // find maximal height of a character
  int FontHeight = 0;
  for (int i = 0; i < 256; ++i) {
    fon2_char_t* Chr = Chars[i];
    if (Chr && Chr->Height > FontHeight) FontHeight = Chr->Height;
  }

  // add extra transparent pixels for characters that are shorter
  for (int i = 0; i < 256; ++i) {
    fon2_char_t* Chr = Chars[i];
    if (!Chr || Chr->Height == FontHeight) continue;
    vuint8 *NewData = (vuint8*)Z_Malloc(Chr->Width*FontHeight);
    memset(NewData, 0, Chr->Width*(FontHeight-Chr->Height));
    memcpy(NewData+Chr->Width*(FontHeight-Chr->Height), Chr->Data, Chr->Width*Chr->Height);
    Z_Free(Chr->Data);
    Chr->Data = NewData;
    Chr->Height = FontHeight;
  }

  // compress character data
  int DataSize = 0;
  for (int i = 0; i < 256; ++i) {
    fon2_char_t *Chr = Chars[i];
    if (!Chr) continue;
    Chr->ComprData = (vint8*)Z_Malloc(Chr->Width*Chr->Height+(Chr->Width*Chr->Height+127)/128);
    vint8* pEnd = CompressChar(Chr->Data, Chr->ComprData, Chr->Width*Chr->Height);
    Chr->ComprDataSize = pEnd-Chr->ComprData;
    DataSize += Chr->ComprDataSize;
  }

  // allocate memory and fill in header
  int NumChars = CharNum-' ';
  fon2_header_t *Font = (fon2_header_t*)Z_Malloc(sizeof(fon2_header_t)+NumColors*3+NumChars*2+DataSize);
  Font->Id[0] = 'F';
  Font->Id[1] = 'O';
  Font->Id[2] = 'N';
  Font->Id[3] = '2';
  Font->Height = LittleShort(FontHeight);
  Font->FirstChar = ' ';
  Font->LastChar = CharNum-1;
  Font->FixedWidth = 0;
  Font->RescalePalette = 1;
  Font->ActiveColors = NumColors-1;
  Font->KerningFlag = 0;

  // write widths
  vuint16 *Widths = (vuint16*)(Font+1);
  for (int i = ' '; i < CharNum; ++i) {
    fon2_char_t *Chr = Chars[i];
    Widths[i-' '] = LittleShort(Chr ? Chr->Width : 0);
  }

  // write palette
  rgb_t *pPal = (rgb_t *)(Widths+NumChars);
  memcpy(pPal, Pal, NumColors*3);

  // write data
  vint8 *pDst = (vint8 *)(pPal+NumColors);
  for (int i = ' '; i < CharNum; ++i) {
    fon2_char_t *Chr = Chars[i];
    if (Chr) {
      memcpy(pDst, Chr->ComprData, Chr->ComprDataSize);
      pDst += Chr->ComprDataSize;
    }
  }

  // write lump and free memory
  if (Zip) {
    AddToZip(lumpname, Font, pDst-(vint8*)Font);
  } else {
    outwad.AddLump(lumpname, Font, pDst-(vint8*)Font);
  }

  Z_Free(Font);

  for (int i = ' '; i < CharNum; ++i) {
    fon2_char_t *Chr = Chars[i];
    if (Chr) {
      Z_Free(Chr->Data);
      Z_Free(Chr->ComprData);
      delete Chr;
    }
  }
}


struct DiskFile {
  VStr diskName;
  VStr zipName;
};


//==========================================================================
//
//  CreateFileList
//
//==========================================================================
static void CreateFileList (TArray<DiskFile> &flist, VStr diskpath, VStr destpath, bool recurse, VStr ext) {
  auto dir = Sys_OpenDir(diskpath, recurse); // want directories?
  if (!dir) return;
  if (destpath.length()) {
    for (char *cp = destpath.GetMutableCharPointer(0); *cp; ++cp) if (*cp == '\\') *cp = '/';
    if (destpath[destpath.length()-1] != '/') destpath += "/";
  }
  if (diskpath.length()) {
    for (char *cp = diskpath.GetMutableCharPointer(0); *cp; ++cp) if (*cp == '\\') *cp = '/';
    if (diskpath[diskpath.length()-1] != '/') diskpath += "/";
  }
  TArray<VStr> dirnames;
  for (;;) {
    VStr fname = Sys_ReadDir(dir);
    if (fname.isEmpty()) break;
    if (fname[fname.length()-1] == '/') {
      //CreateFileList(flist, diskpath+fname, destpath+fname, recurse, ext);
      dirnames.append(fname);
    } else {
      if (ext.length()) {
        if (fname.length() < ext.length()) continue;
        if (ext.ICmp(*fname+fname.length()-ext.length()) != 0) continue;
      }
      DiskFile &dfl = flist.alloc();
      dfl.diskName = diskpath+fname;
      dfl.zipName = destpath+fname;
    }
  }
  Sys_CloseDir(dir);

  for (auto &&fname : dirnames) {
    CreateFileList(flist, diskpath+fname, destpath+fname, recurse, ext);
  }
}


extern "C" {
  static int DFICompare (const void *a, const void *b, void *) {
    DiskFile *fa = (DiskFile *)a;
    DiskFile *fb = (DiskFile *)b;
    return fa->zipName.ICmp(fb->zipName);
  }
}

//==========================================================================
//
//  SortFileList
//
//==========================================================================
static void SortFileList (TArray<DiskFile> &flist) {
  if (flist.length() < 2) return;
  timsort_r(flist.ptr(), (size_t)flist.length(), (size_t)sizeof(DiskFile), &DFICompare, nullptr);
}


//==========================================================================
//
//  ParseScript
//
//==========================================================================
static void ParseScript (const char *name) {
  ExtractFilePath(name, basepath, sizeof(basepath));
  if (strlen(basepath)+1 > sizeof(srcpath)) Error("Path too long");
  strcpy(srcpath, basepath);

  SC_Open(name);

  if (!destfile[0]) {
    strcpy(destfile, name);
    StripExtension(destfile);
    destpath[0] = 0;
  } else {
    ExtractFilePath(destfile, destpath, sizeof(destpath));
  }

  bool OutputOpened = false;
  bool GrabMode = false;

  while (SC_GetString()) {
    if (SC_Compare("$dest")) {
      if (OutputOpened) SC_ScriptError("Output already opened");
      SC_MustGetString();
      strcpy(destfile, fn(sc_String));
      continue;
    }

    if (SC_Compare("$srcdir")) {
      SC_MustGetString();
      sprintf(srcpath, "%s%s", basepath, sc_String);
      if (srcpath[strlen(srcpath)-1] != '/') strcat(srcpath, "/");
      continue;
    }

    if (SC_Compare("$load")) {
      LoadImage();
      GrabMode = true;
      continue;
    }

    if (SC_Compare("$files")) {
      GrabMode = false;
      continue;
    }

    if (!OutputOpened) {
      DefaultExtension(destfile, sizeof(destfile), ".wad");
      char Ext[64];
      ExtractFileExtension(destfile, Ext, sizeof(Ext));
      if (!stricmp(Ext, ".zip") || !stricmp(Ext, ".pk3")) {
        Zip = zipOpen(destfile, APPEND_STATUS_CREATE);
        if (!Zip) Error("Failed to open ZIP file");
      } else {
        outwad.Open(destfile, "PWAD");
      }
      OutputOpened = true;
    }

    if (SC_Compare("$wad")) {
      SC_MustGetString();
      AddWad(sc_String);
      continue;
    }

    if (SC_Compare("$map")) {
      SC_MustGetString();
      AddMap(sc_String);
      continue;
    }

    if (SC_Compare("$label")) {
      if (Zip) Error("$label cannot be used for ZIP file");
      SC_MustGetString();
      outwad.AddLump(sc_String, nullptr, 0);
      continue;
    }

    bool isScanDir = false;
    bool isScanDirRecurse = false;
    bool isScanDirWithExt = false;

         if (SC_Compare("$recursedir")) { isScanDir = true; isScanDirRecurse = true; isScanDirWithExt = false; }
    else if (SC_Compare("$recursedirext")) { isScanDir = true; isScanDirRecurse = true; isScanDirWithExt = true; }
    else if (SC_Compare("$onlydir")) { isScanDir = true; isScanDirRecurse = false; isScanDirWithExt = false; }
    else if (SC_Compare("$onlydirext")) { isScanDir = true; isScanDirRecurse = false; isScanDirWithExt = true; }

    if (isScanDir) {
      // $recursedir zippath diskpath
      // $recursedirext zippath diskpath ext
      if (!Zip) Error("$recursedir cannot be used outsize of ZIP");
      bool gotExt = false;
      VStr ext;
      if (isScanDirWithExt && SC_Check("|")) {
        gotExt = true;
        SC_MustGetString();
        ext = VStr(sc_String);
        SC_MustGetStringName("|");
      }
      // zippath
      SC_MustGetString();
      VStr zippath = VStr(sc_String);
      // diskpath
      SC_MustGetString();
      VStr diskpath = VStr(sc_String);
      // extension
      if (isScanDirWithExt && !gotExt) {
        SC_MustGetString();
        ext = VStr(sc_String);
      }
      if (isScanDirWithExt) {
        if (ext.length() == 0) SC_ScriptError("empty extension\n");
        if (ext[0] != '.') SC_ScriptError("extension must start with a dot\n");
        for (const char *s = *ext; *s; ++s) {
          if (*s == '<' || *s == '>' || *s == '?' || *s == '*' || *s == '[' || *s == ']') SC_ScriptError("invalid extension character\n");
        }
      }
      //fprintf(stderr, "ZIP: dest=<%s>; disk=<%s>; ext=<%s>\n", *zippath, *diskpath, *ext);
      TArray<DiskFile> flist;
      CreateFileList(flist, diskpath, zippath, isScanDirRecurse, ext);
      SortFileList(flist);
      for (int f = 0; f < flist.length(); ++f) {
        //fprintf(stderr, "  <%s> -> <%s>\n", *flist[f].diskName, *flist[f].zipName);
        void *data;
        int size = LoadFile(*flist[f].diskName, &data);
        AddToZip(*flist[f].zipName, data, size);
        Z_Free(data);
      }
      continue;
    }

    if (GrabMode) {
      if (!Zip && strlen(sc_String) > 8) SC_ScriptError("Lump name is too long.");
      memset(lumpname, 0, sizeof(lumpname));
      strcpy(lumpname, sc_String);

      SC_MustGetString();
           if (SC_Compare("rgbtable")) GrabRGBTable();
      else if (SC_Compare("tinttab")) GrabTranslucencyTable();
      else if (SC_Compare("scalemap")) GrabScaleMap();
      else if (SC_Compare("raw")) GrabRaw();
      else if (SC_Compare("patch")) GrabPatch();
      else if (SC_Compare("pic")) GrabPic();
      else if (SC_Compare("pic15")) GrabPic15();
      else if (SC_Compare("fon1")) GrabFon1();
      else if (SC_Compare("fon2")) GrabFon2();
      else SC_ScriptError(va("Unknown command: \"%s\"", sc_String));
    } else if (Zip) {
      strcpy(lumpname, sc_String);
      SC_MustGetString();
      void *data;
      int size = LoadFile(fn(sc_String), &data);
      AddToZip(lumpname, data, size);
      Z_Free(data);
    } else {
      ExtractFileBase(sc_String, lumpname, sizeof(lumpname));
      if (strlen(lumpname) > 8) SC_ScriptError("Lump name too long");
      void *data;
      int size = LoadFile(fn(sc_String), &data);
      outwad.AddLump(lumpname, data, size);
      Z_Free(data);
    }
  }

  DestroyImage();
  if (outwad.handle) outwad.Close();
  if (Zip) {
    if (zipClose(Zip, nullptr) != ZIP_OK) Error("Failed to close ZIP file");
  }
  SC_Close();

  destfile[0] = 0;
}


//==========================================================================
//
//  main
//
//==========================================================================
int main (int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: vlumpy <script1> [<script2> ...]\n");
    return 1;
  }

  try {
    for (int i = 1; i < argc; ++i) {
      if (argv[i][0] == '-' && argv[i][1] == 'D') {
        strcpy(destfile, argv[i]+2);
      } else {
        ParseScript(argv[i]);
      }
    }
  } catch (WadLibError &E) {
    Error("%s", E.message);
  }

  Z_ShuttingDown();
  return 0;
}
