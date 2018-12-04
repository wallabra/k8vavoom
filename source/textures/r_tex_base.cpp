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

#include "gamedefs.h"
#include "r_tex.h"


// ////////////////////////////////////////////////////////////////////////// //
typedef VTexture *(*VTexCreateFunc) (VStream &, int);


//==========================================================================
//
//  VTexture::CreateTexture
//
//==========================================================================
VTexture *VTexture::CreateTexture (int Type, int LumpNum) {
  guard(VTexture::CreateTexture);
  static const struct {
    VTexCreateFunc  Create;
    int Type;
  } TexTable[] = {
    { VImgzTexture::Create,     TEXTYPE_Any },
    { VPngTexture::Create,      TEXTYPE_Any },
    { VJpegTexture::Create,     TEXTYPE_Any },
    { VPcxTexture::Create,      TEXTYPE_Any },
    { VTgaTexture::Create,      TEXTYPE_Any },
    { VFlatTexture::Create,     TEXTYPE_Flat },
    { VRawPicTexture::Create,   TEXTYPE_Pic },
    { VPatchTexture::Create,    TEXTYPE_Any },
    { VAutopageTexture::Create, TEXTYPE_Autopage },
  };

  if (LumpNum < 0) return nullptr;
  VStream *Strm = W_CreateLumpReaderNum(LumpNum);

  for (size_t i = 0; i < ARRAY_COUNT(TexTable); ++i) {
    if (TexTable[i].Type == Type || TexTable[i].Type == TEXTYPE_Any || Type == TEXTYPE_Any) {
      VTexture *Tex = TexTable[i].Create(*Strm, LumpNum);
      if (Tex) {
        Tex->Type = Type;
        delete Strm;
        return Tex;
      }
    }
  }

  delete Strm;
  return nullptr;
  unguard;
}


//==========================================================================
//
//  VTexture::VTexture
//
//==========================================================================
VTexture::VTexture ()
  : Type(TEXTYPE_Any)
  , Format(TEXFMT_8)
  , Name(NAME_None)
  , Width(0)
  , Height(0)
  , SOffset(0)
  , TOffset(0)
  , bNoRemap0(false)
  , bWorldPanning(false)
  , bIsCameraTexture(false)
  , WarpType(0)
  , SScale(1)
  , TScale(1)
  , TextureTranslation(0)
  , SourceLump(-1)
  , noDecals(false)
  , staticNoDecals(false)
  , animNoDecals(false)
  , animated(false)
  , DriverData(0)
  , Pixels8Bit(0)
  , Pixels8BitA(0)
  , HiResTexture(0)
  , Pixels8BitValid(false)
  , Pixels8BitAValid(false)
  , shadeColor(-1)
{
  needFBO = false;
  mFBO = 0;
  mFBOColorTid = 0;
  mFBODepthStencilTid = 0;
  SavedDriverHandle = 0;
}


//==========================================================================
//
//  VTexture::~VTexture
//
//==========================================================================
VTexture::~VTexture () {
  if (Pixels8Bit) {
    delete[] Pixels8Bit;
    Pixels8Bit = nullptr;
  }
  if (Pixels8BitA) {
    delete[] Pixels8BitA;
    Pixels8BitA = nullptr;
  }
  if (HiResTexture) {
    delete HiResTexture;
    HiResTexture = nullptr;
  }
}


//==========================================================================
//
//  VTexture::SetFrontSkyLayer
//
//==========================================================================
void VTexture::SetFrontSkyLayer () {
  guardSlow(VTexture::SetFrontSkyLayer);
  bNoRemap0 = true;
  unguardSlow;
}


//==========================================================================
//
//  VTexture::CheckModified
//
//==========================================================================
bool VTexture::CheckModified () {
  return false;
}


//==========================================================================
//
//  VTexture::GetPixels8
//
//==========================================================================
vuint8 *VTexture::GetPixels8 () {
  guard(VTexture::GetPixels8);
  // if already have converted version, then just return it
  if (Pixels8Bit && Pixels8BitValid) return Pixels8Bit;

  vuint8 *Pixels = GetPixels();
  if (Format == TEXFMT_8Pal) {
    // remap to game palette
    int NumPixels = Width*Height;
    rgba_t *Pal = GetPalette();
    vuint8 Remap[256];
    Remap[0] = 0;
    for (int i = 1; i < 256; ++i) Remap[i] = r_rgbtable[((Pal[i].r<<7)&0x7c00)+((Pal[i].g<<2)&0x3e0)+((Pal[i].b>>3)&0x1f)];
    if (!Pixels8Bit) Pixels8Bit = new vuint8[NumPixels];
    vuint8 *pSrc = Pixels;
    vuint8 *pDst = Pixels8Bit;
    for (int i = 0; i < NumPixels; ++i, ++pSrc, ++pDst) *pDst = Remap[*pSrc];
    Pixels8BitValid = true;
    return Pixels8Bit;
  } else if (Format == TEXFMT_RGBA) {
    int NumPixels = Width*Height;
    if (!Pixels8Bit) Pixels8Bit = new vuint8[NumPixels];
    rgba_t *pSrc = (rgba_t *)Pixels;
    vuint8 *pDst = Pixels8Bit;
    for (int i = 0; i < NumPixels; ++i, ++pSrc, ++pDst) {
      if (pSrc->a < 128) {
        *pDst = 0;
      } else {
        *pDst = r_rgbtable[((pSrc->r<<7)&0x7c00)+((pSrc->g<<2)&0x3e0)+((pSrc->b>>3)&0x1f)];
      }
    }
    Pixels8BitValid = true;
    return Pixels8Bit;
  }
  return Pixels;
  unguard;
}


//==========================================================================
//
//  VTexture::GetPixels8A
//
//==========================================================================
vuint8 *VTexture::GetPixels8A () {
  // if already have converted version, then just return it
  if (Pixels8BitA && Pixels8BitAValid) return Pixels8BitA;

  int NumPixels = Width*Height;
  if (!Pixels8BitA) Pixels8BitA = new vuint8[NumPixels*2];
  vuint8 *pDst = Pixels8BitA;

  if (Format == TEXFMT_8Pal || Format == TEXFMT_8) {
    // remap to game palette
    vuint8 remap[256];
    if (Format == TEXFMT_8Pal) {
      // own palette, remap
      remap[0] = 0;
      const rgba_t *pal = GetPalette();
      for (int i = 1; i < 256; ++i) remap[i] = r_rgbtable[((pal[i].r<<7)&0x7c00)+((pal[i].g<<2)&0x3e0)+((pal[i].b>>3)&0x1f)];
    } else {
      // game palette, no remap
      for (int i = 0; i < 256; ++i) remap[i] = i;
    }
    const vuint8 *pSrc = GetPixels();
    for (int i = 0; i < NumPixels; ++i, ++pSrc, pDst += 2) {
      pDst[0] = remap[*pSrc];
      pDst[1] = (*pSrc ? 255 : 0);
    }
  } else if (Format == TEXFMT_RGBA) {
    const rgba_t *pSrc = (const rgba_t *)GetPixels();
    for (int i = 0; i < NumPixels; ++i, ++pSrc, pDst += 2) {
      pDst[0] = r_rgbtable[((pSrc->r<<7)&0x7c00)+((pSrc->g<<2)&0x3e0)+((pSrc->b>>3)&0x1f)];
      pDst[1] = pSrc->a;
    }
  } else {
    Sys_Error("invalid texture format in `VTexture::GetPixels8A()`");
  }

  Pixels8BitAValid = true;
  return Pixels8BitA;
}


//==========================================================================
//
//  VTexture::GetPalette
//
//==========================================================================
rgba_t *VTexture::GetPalette () {
  guardSlow(VTexture::GetPalette);
  return r_palette;
  unguardSlow;
}


//==========================================================================
//
//  VTexture::GetHighResolutionTexture
//
// Return high-resolution version of this texture, or self if it doesn't
// exist.
//
//==========================================================================
VTexture *VTexture::GetHighResolutionTexture() {
  guard(VTexture::GetHighResolutionTexture);
#ifdef CLIENT
  if (!r_hirestex) return nullptr;
  // if high resolution texture is already created, then just return it
  if (HiResTexture) return HiResTexture;

  // determine directory name depending on type.
  const char *DirName;
  switch (Type) {
    case TEXTYPE_Wall: DirName = "walls"; break;
    case TEXTYPE_Flat: DirName = "flats"; break;
    case TEXTYPE_Overload: DirName = "textures"; break;
    case TEXTYPE_Sprite: DirName = "sprites"; break;
    case TEXTYPE_Pic: case TEXTYPE_Autopage: DirName = "graphics"; break;
    default: return nullptr;
  }

  // try to find it
  static const char *Exts[] = { "png", "jpg", "tga", nullptr };
  int LumpNum = W_FindLumpByFileNameWithExts(VStr("hirestex/")+DirName+"/"+*Name, Exts);
  if (LumpNum >= 0) {
    // create new high-resolution texture
    HiResTexture = CreateTexture(Type, LumpNum);
    HiResTexture->Name = Name;
    return HiResTexture;
  }
#endif
  // no hi-res texture found
  return nullptr;
  unguard;
}


//==========================================================================
//
//  VTexture::FixupPalette
//
//==========================================================================
void VTexture::FixupPalette (vuint8 *Pixels, rgba_t *Palette) {
  guard(VTexture::FixupPalette);
  // find black colour for remaping
  int black = 0;
  int best_dist = 0x10000;
  for (int i = 1; i < 256; ++i) {
    int dist = Palette[i].r*Palette[i].r+Palette[i].g*Palette[i].g+Palette[i].b*Palette[i].b;
    if (dist < best_dist && Palette[i].a == 255) {
      black = i;
      best_dist = dist;
    }
  }
  for (int i = 0; i < Width*Height; ++i) {
    if (Palette[Pixels[i]].a == 0) {
      Pixels[i] = 0;
    } else if (!Pixels[i]) {
      Pixels[i] = black;
    }
  }
  Palette[0].r = 0;
  Palette[0].g = 0;
  Palette[0].b = 0;
  Palette[0].a = 0;
  unguard;
}


//==========================================================================
//
//  VTexture::FindDriverTrans
//
//==========================================================================
VTexture::VTransData *VTexture::FindDriverTrans (VTextureTranslation *TransTab, int CMap) {
  guard(VTexture::FindDriverTrans);
  for (int i = 0; i < DriverTranslated.Num(); ++i) {
    if (DriverTranslated[i].Trans == TransTab && DriverTranslated[i].ColourMap == CMap) {
      return &DriverTranslated[i];
    }
  }
  return nullptr;
  unguard;
}


  // prepare temp buffer (for fringe removing)
  /*
  //  WARNING! not thred-safe! (using static buffer)
  //
  static vuint8 *tempbuf = nullptr;
  static vint32 tbsize = 0;
  if (tbsize < (w+2)*(h+2)*4) {
    tbsize = (((w+2)*(h+2)*4)|0xffff)+1;
    tempbuf = (vuint8 *)Z_Realloc(tempbuf, tbsize);
  }

  const int tw = w+2;
  const int th = h+2;

  // copy image to temp buffer, with transparent border
  memset(tempbuf, 0, tw*4); // top row
  for (int y = 1; y <= h; ++y) {
    *(vuint32)(tempbuf+(y*tw)*4) = 0; // left border
    memcpy(tempbuf+(y*tw)*4+4, data+((y-1)*w)*4, w*4);
    *(vuint32)(tempbuf+(y*tw)*4+(w+1)*4) = 0; // right border
  }
  memset(tempbuf+((h+1)*tw)*4, 0, tw*4); // bottom row
  */

  // remove fringes, by setting transparent pixel colors to average of
  // surrounding non-transparent ones

// 0: transparent

//==========================================================================
//
//  VTexture::PremultiplyRGBAInPlace
//
//==========================================================================
void VTexture::PremultiplyRGBAInPlace (void *databuff, int w, int h) {
  if (w < 1 || h < 1) return;
  vuint8 *data = (vuint8 *)databuff;
  // premultiply original image
  for (int count = w*h; count > 0; --count, data += 4) {
    int a = data[3];
    if (a == 0) {
      data[0] = data[1] = data[2] = 0;
    } else if (a != 255) {
      data[0] = data[0]*a/255;
      data[1] = data[1]*a/255;
      data[2] = data[2]*a/255;
    }
  }
}


//==========================================================================
//
//  VTexture::PremultiplyRGBA
//
//==========================================================================
void VTexture::PremultiplyRGBA (void *dest, const void *src, int w, int h) {
  if (w < 1 || h < 1) return;
  const vuint8 *s = (const vuint8 *)src;
  vuint8 *d = (vuint8 *)dest;
  // premultiply image
  for (int count = w*h; count > 0; --count, s += 4, d += 4) {
    int a = s[3];
    if (a == 0) {
      *(vuint32 *)d = 0;
    } else if (a == 255) {
      *(vuint32 *)d = *(const vuint32 *)s;
    } else {
      d[0] = s[0]*a/255;
      d[1] = s[1]*a/255;
      d[2] = s[2]*a/255;
      d[3] = a;
    }
  }
}


//==========================================================================
//
//  VTexture::AdjustGamma
//
//  non-premultiplied
//
//==========================================================================
void VTexture::AdjustGamma (rgba_t *data, int size) {
#ifdef CLIENT
  guard(VTexture::AdjustGamma);
  const vuint8 *gt = getGammaTable(usegamma); //gammatable[usegamma];
  for (int i = 0; i < size; ++i) {
    data[i].r = gt[data[i].r];
    data[i].g = gt[data[i].g];
    data[i].b = gt[data[i].b];
  }
  unguard;
#endif
}


//==========================================================================
//
//  VTexture::SmoothEdges
//
//  This one comes directly from GZDoom
//
//  non-premultiplied
//
//==========================================================================
#define CHKPIX(ofs) (l1[(ofs)*4+MSB] == 255 ? (( ((vuint32*)l1)[0] = ((vuint32*)l1)[ofs]&SOME_MASK), trans = true ) : false)

void VTexture::SmoothEdges (vuint8 *buffer, int w, int h) {
  int x, y;
  // why the fuck you would use 0 on big endian here?
  int MSB = 3;
  vuint32 SOME_MASK = (GBigEndian ? 0xffffff00 : 0x00ffffff);

  // if I set this to false here the code won't detect textures that only contain transparent pixels
  bool trans = (buffer[MSB] == 0);
  vuint8 *l1;

  if (h <= 1 || w <= 1) return; // makes (a) no sense and (b) doesn't work with this code!
  l1 = buffer;

  if (l1[MSB] == 0 && !CHKPIX(1)) {
    CHKPIX(w);
  }
  l1 += 4;

  for (x = 1; x < w-1; ++x, l1 += 4) {
    if (l1[MSB] == 0 && !CHKPIX(-1) && !CHKPIX(1)) {
      CHKPIX(w);
    }
  }
  if (l1[MSB] == 0 && !CHKPIX(-1)) {
    CHKPIX(w);
  }
  l1 += 4;

  for (y = 1; y < h-1; ++y) {
    if (l1[MSB] == 0 && !CHKPIX(-w) && !CHKPIX(1)) {
      CHKPIX(w);
    }
    l1 += 4;

    for (x = 1; x < w-1; ++x, l1 += 4) {
      if (l1[MSB] == 0 && !CHKPIX(-w) && !CHKPIX(-1) && !CHKPIX(1) && !CHKPIX(-w-1) && !CHKPIX(-w+1) && !CHKPIX(w-1) && !CHKPIX(w+1)) {
        CHKPIX(w);
      }
    }
    if (l1[MSB] == 0 && !CHKPIX(-w) && !CHKPIX(-1)) {
      CHKPIX(w);
    }
    l1 += 4;
  }

  if (l1[MSB] == 0 && !CHKPIX(-w)) {
    CHKPIX(1);
  }
  l1 += 4;
  for (x = 1; x < w-1; ++x, l1 += 4) {
    if (l1[MSB] == 0 && !CHKPIX(-w) && !CHKPIX(-1)) {
      CHKPIX(1);
    }
  }
  if (l1[MSB] == 0 && !CHKPIX(-w)) {
    CHKPIX(-1);
  }
}

#undef CHKPIX


//==========================================================================
//
//  VTexture::ResampleTexture
//
//  Resizes texture.
//  This is a simplified version of gluScaleImage from sources of MESA 3.0
//  non-premultiplied
//
//==========================================================================
void VTexture::ResampleTexture (int widthin, int heightin, const vuint8 *datain, int widthout, int heightout, vuint8 *dataout, int sampling_type) {
  guard(VTexture::ResampleTexture);
  int i, j, k;
  float sx, sy;

  if (widthout > 1) {
    sx = float(widthin-1)/float(widthout-1);
  } else {
    sx = float(widthin-1);
  }
  if (heightout > 1) {
    sy = float(heightin-1)/float(heightout-1);
  } else {
    sy = float(heightin-1);
  }

  if (sampling_type == 1) {
    // use point sample
    for (i = 0; i < heightout; ++i) {
      int ii = int(i*sy);
      for (j = 0; j < widthout; ++j) {
        int jj = int(j*sx);

        const vuint8 *src = datain+(ii*widthin+jj)*4;
        vuint8 *dst = dataout+(i*widthout+j)*4;

        for (k = 0; k < 4; ++k) *dst++ = *src++;
      }
    }
  } else {
    // use weighted sample
    if (sx <= 1.0 && sy <= 1.0) {
      // magnify both width and height: use weighted sample of 4 pixels
      int i0, i1, j0, j1;
      float alpha, beta;
      const vuint8 *src00, *src01, *src10, *src11;
      float s1, s2;
      vuint8 *dst;

      for (i = 0; i < heightout; ++i) {
        i0 = int(i*sy);
        i1 = i0+1;
        if (i1 >= heightin) i1 = heightin-1;
        alpha = i*sy-i0;
        for (j = 0; j < widthout; ++j) {
          j0 = int(j*sx);
          j1 = j0+1;
          if (j1 >= widthin) j1 = widthin-1;
          beta = j*sx-j0;

          // compute weighted average of pixels in rect (i0,j0)-(i1,j1)
          src00 = datain+(i0*widthin+j0)*4;
          src01 = datain+(i0*widthin+j1)*4;
          src10 = datain+(i1*widthin+j0)*4;
          src11 = datain+(i1*widthin+j1)*4;

          dst = dataout+(i*widthout+j)*4;

          for (k = 0; k < 4; ++k) {
            s1 = *src00++ *(1.0-beta)+ *src01++ *beta;
            s2 = *src10++ *(1.0-beta)+ *src11++ *beta;
            *dst++ = vuint8(s1*(1.0-alpha)+s2*alpha);
          }
        }
      }
    } else {
      // shrink width and/or height: use an unweighted box filter
      int i0, i1;
      int j0, j1;
      int ii, jj;
      int sum;
      vuint8 *dst;

      for (i = 0; i < heightout; ++i) {
        i0 = int(i*sy);
        i1 = i0+1;
        if (i1 >= heightin) i1 = heightin-1;
        for (j = 0; j < widthout; ++j) {
          j0 = int(j*sx);
          j1 = j0+1;
          if (j1 >= widthin) j1 = widthin-1;

          dst = dataout+(i*widthout+j)*4;

          // compute average of pixels in the rectangle (i0,j0)-(i1,j1)
          for (k = 0; k < 4; ++k) {
            sum = 0;
            for (ii = i0; ii <= i1; ++ii) {
              for (jj = j0; jj <= j1; ++jj) {
                sum += *(datain+(ii*widthin+jj)*4+k);
              }
            }
            sum /= (j1-j0+1)*(i1-i0+1);
            *dst++ = vuint8(sum);
          }
        }
      }
    }
  }
  unguard;
}


//==========================================================================
//
//  VTexture::MipMap
//
//  Scales image down for next mipmap level, operates in place
//  non-premultiplied
//
//==========================================================================
void VTexture::MipMap (int width, int height, vuint8 *InIn) {
  guard(VTexture::MipMap);
  vuint8 *in = InIn;
  int i, j;
  vuint8 *out = in;

  if (width == 1 || height == 1) {
    // special case when only one dimension is scaled
    int total = width*height/2;
    for (i = 0; i < total; ++i, in += 8, out += 4) {
      out[0] = vuint8((in[0]+in[4])>>1);
      out[1] = vuint8((in[1]+in[5])>>1);
      out[2] = vuint8((in[2]+in[6])>>1);
      out[3] = vuint8((in[3]+in[7])>>1);
    }
    return;
  }

  // scale down in both dimensions
  width <<= 2;
  height >>= 1;
  for (i = 0; i < height; ++i, in += width) {
    for (j = 0; j < width; j += 8, in += 8, out += 4) {
      out[0] = vuint8((in[0]+in[4]+in[width+0]+in[width+4])>>2);
      out[1] = vuint8((in[1]+in[5]+in[width+1]+in[width+5])>>2);
      out[2] = vuint8((in[2]+in[6]+in[width+2]+in[width+6])>>2);
      out[3] = vuint8((in[3]+in[7]+in[width+3]+in[width+7])>>2);
    }
  }
  unguard;
}


//==========================================================================
//
//  VTexture::getPixel
//
//==========================================================================
rgba_t VTexture::getPixel (int x, int y) {
  rgba_t col = rgba_t(0, 0, 0, 0);
  if (x < 0 || y < 0 || x >= Width || y >= Height) return col;

  const vuint8 *data = (const vuint8 *)GetPixels();
  if (!data) return col;

  int pitch = 0;
  switch (Format) {
    case TEXFMT_8: case TEXFMT_8Pal: pitch = 1; break;
    case TEXFMT_RGBA: pitch = 4; break;
    default: return col;
  }

  data += y*(Width*pitch)+x*pitch;
  switch (Format) {
    case TEXFMT_8: col = r_palette[*data]; break;
    case TEXFMT_8Pal: col = (GetPalette() ? GetPalette()[*data] : r_palette[*data]); break;
    case TEXFMT_RGBA: col = *((const rgba_t *)data); break;
    default: return col;
  }

  return col;
}


//==========================================================================
//
//  VTexture::shadePixelsRGBA
//
//  use image as alpha-map
//
//==========================================================================
void VTexture::shadePixelsRGBA (vuint8 *pic, int wdt, int hgt, int shadeColor) {
  if (!pic || wdt < 1 || hgt < 1 || shadeColor < 0) return;
  //vuint8 *picbuf = pic;
  vuint8 *oldpic = new vuint8[wdt*hgt*4];
  memcpy(oldpic, pic, wdt*hgt*4);
  float shadeR = (shadeColor>>16)&0xff;
  float shadeG = (shadeColor>>8)&0xff;
  float shadeB = (shadeColor)&0xff;
  for (int y = 0; y < hgt; ++y) {
    for (int x = 0; x < wdt; ++x) {
      int addr = y*(wdt*4)+x*4;
      int intensity = pic[addr]; // use red as intensity
      /*
      if (intensity < 3) {
        int r = 0;
        int count = 0;
        for (int dy = -1; dy < 2; ++dy) {
          for (int dx = -1; dx < 2; ++dx) {
            int px = x+dx, py = y+dy;
            if (px >= 0 && py >= 0 && px < wdt && py < hgt) {
              ++count;
              r += oldpic[py*(wdt*4)+px*4+0];
            }
          }
        }
        float v = (float)r/float(count);
        if (v < intensity) v = intensity;
        pic[addr+0] = clampToByte(v*shadeR/255.0f);
        pic[addr+1] = clampToByte(v*shadeG/255.0f);
        pic[addr+2] = clampToByte(v*shadeB/255.0f);
      } else {
        pic[addr+0] = clampToByte((float)intensity*shadeR/255.0f);
        pic[addr+1] = clampToByte((float)intensity*shadeG/255.0f);
        pic[addr+2] = clampToByte((float)intensity*shadeB/255.0f);
      }
      */
      pic[addr+0] = clampToByte(shadeR);
      pic[addr+1] = clampToByte(shadeG);
      pic[addr+2] = clampToByte(shadeB);
      pic[addr+3] = intensity;
    }
  }
  delete oldpic;
  //SmoothEdges(picbuf, wdt, hgt);
}


//==========================================================================
//
//  sRGBungamma
//
//  inverse of sRGB "gamma" function. (approx 2.2)
//
//==========================================================================
static double sRGBungamma (int ic) {
  const double c = ic/255.0;
  if (c <= 0.04045) return c/12.92;
  return pow((c+0.055)/1.055, 2.4);
}


//==========================================================================
//
//  sRGBungamma
//
//  sRGB "gamma" function (approx 2.2)
//
//==========================================================================
static int sRGBgamma (double v) {
  if (v <= 0.0031308) v *= 12.92; else v = 1.055*pow(v, 1.0/2.4)-0.055;
  return int(v*255+0.5);
}


//==========================================================================
//
//  colorIntensity
//
//==========================================================================
static vuint8 colorIntensity (int r, int g, int b) {
  // sRGB luminance(Y) values
  const double rY = 0.212655;
  const double gY = 0.715158;
  const double bY = 0.072187;
  return clampToByte(sRGBgamma(rY*sRGBungamma(r)+gY*sRGBungamma(g)+bY*sRGBungamma(b)));
}


//==========================================================================
//
//  VTexture::stencilPixelsRGBA
//
//==========================================================================
void VTexture::stencilPixelsRGBA (vuint8 *pic, int wdt, int hgt, int shadeColor) {
  if (!pic || wdt < 1 || hgt < 1 || shadeColor < 0) return;
  //vuint8 *picbuf = pic;
  vuint8 *oldpic = new vuint8[wdt*hgt*4];
  memcpy(oldpic, pic, wdt*hgt*4);
  float shadeR = (shadeColor>>16)&0xff;
  float shadeG = (shadeColor>>8)&0xff;
  float shadeB = (shadeColor)&0xff;
  for (int y = 0; y < hgt; ++y) {
    for (int x = 0; x < wdt; ++x) {
      int addr = y*(wdt*4)+x*4;
      float intensity = colorIntensity(pic[addr+0], pic[addr+1], pic[addr+2])/255.0f;
      pic[addr+0] = clampToByte(intensity*shadeR);
      pic[addr+1] = clampToByte(intensity*shadeG);
      pic[addr+2] = clampToByte(intensity*shadeB);
    }
  }
  delete oldpic;
  //SmoothEdges(picbuf, wdt, hgt);
}


//==========================================================================
//
//  VTexture::Shade
//
//==========================================================================
void VTexture::Shade (int shade) {
  shadeColor = shade;
}


//==========================================================================
//
//  VTexture::checkerFill8
//
//==========================================================================
void VTexture::checkerFill8 (vuint8 *dest, int width, int height) {
  if (!dest || width < 1 || height < 1) return;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      *dest++ = (((x/8)^(y/8))&1 ? r_white_colour : r_black_colour);
    }
  }
}


//==========================================================================
//
//  VTexture::checkerFillRGB
//
//==========================================================================
void VTexture::checkerFillRGB (vuint8 *dest, int width, int height) {
  if (!dest || width < 1 || height < 1) return;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      vuint8 v = (((x/8)^(y/8))&1 ? 255 : 0);
      *dest++ = v;
      *dest++ = v;
      *dest++ = v;
    }
  }
}


//==========================================================================
//
//  VTexture::checkerFillRGBA
//
//==========================================================================
void VTexture::checkerFillRGBA (vuint8 *dest, int width, int height) {
  if (!dest || width < 1 || height < 1) return;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      vuint8 v = (((x/8)^(y/8))&1 ? 255 : 0);
      *dest++ = v;
      *dest++ = v;
      *dest++ = v;
      *dest++ = 255;
    }
  }
}


//==========================================================================
//
//  VTexture::checkerFillColumn8
//
//  `dest` points at column, `x` is used only to build checker
//
//==========================================================================
void VTexture::checkerFillColumn8 (vuint8 *dest, int x, int pitch, int height) {
  if (!dest || pitch < 1 || height < 1) return;
  for (int y = 0; y < height; ++y) {
    *dest = (((x/8)^(y/8))&1 ? r_white_colour : r_black_colour);
    dest += pitch;
  }
}


//==========================================================================
//
//  VDummyTexture::VDummyTexture
//
//==========================================================================
VDummyTexture::VDummyTexture () {
  Type = TEXTYPE_Null;
  Format = TEXFMT_8;
}


//==========================================================================
//
//  VDummyTexture::GetPixels
//
//==========================================================================
vuint8 *VDummyTexture::GetPixels () {
  return nullptr;
}


//==========================================================================
//
//  VDummyTexture::Unload
//
//==========================================================================
void VDummyTexture::Unload () {
}
