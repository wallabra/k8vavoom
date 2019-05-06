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
//**  Copyright (C) 2018 Ketmar Dark
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
#ifndef IMAGO_HEADER_FILE
#define IMAGO_HEADER_FILE

#include "../core/core.h"


class VImage {
public:
  // see https://www.compuphase.com/cmetric.htm
  static inline __attribute__((unused)) vint32 rgbDistanceSquared (vuint8 r0, vuint8 g0, vuint8 b0, vuint8 r1, vuint8 g1, vuint8 b1) {
    const vint32 rmean = ((vint32)r0+(vint32)r1)/2;
    const vint32 r = (vint32)r0-(vint32)r1;
    const vint32 g = (vint32)g0-(vint32)g1;
    const vint32 b = (vint32)b0-(vint32)b1;
    return (((512+rmean)*r*r)/256)+4*g*g+(((767-rmean)*b*b)/256);
  }

  // texture data formats
  enum ImageType {
    IT_Pal,  // paletised
    IT_RGBA, // truecolor
  };

  struct __attribute__((aligned(1), packed)) RGB;
  struct __attribute__((aligned(1), packed)) RGBA;

  struct __attribute__((aligned(1), packed)) RGB {
    vuint8 r, g, b;
    RGB () : r(0), g(0), b(0) {}
    RGB (vuint8 ar, vuint8 ag, vuint8 ab) : r(ar), g(ag), b(ab) {}
    RGB (const RGB &c) : r(c.r), g(c.g), b(c.b) {}
    RGB (const RGBA &c) : r(c.r), g(c.g), b(c.b) {}
    inline bool operator == (const RGB &c) const { return (r == c.r && g == c.g && b == c.b); }
    inline bool operator == (const RGBA &c) const { return (c.a == 255 && r == c.r && g == c.g && b == c.b); }
    //inline int distSq (const RGB &c) const { return (r-c.r)*(r-c.r)+(g-c.g)*(g-c.g)+(b-c.b)*(b-c.b); }
    //inline int distSq (const RGBA &c) const { return (r-c.r)*(r-c.r)+(g-c.g)*(g-c.g)+(b-c.b)*(b-c.b)+(255-c.a)*(255-c.a); }
    inline int distSq (const RGBA &c) const { return rgbDistanceSquared(r, g, b, c.r*c.a/255, c.g*c.a/255, c.b*c.a/255); }
    inline int distSq (const RGB &c) const { return rgbDistanceSquared(r, g, b, c.r, c.g, c.b); }
    inline bool isTransparent () const { return false; }
    inline bool isOpaque () const { return true; }
    // ignores alpha
    inline bool sameColor (const RGB &c) const { return (r == c.r && g == c.g && b == c.b); }
    inline bool sameColor (const RGBA &c) const { return (r == c.r && g == c.g && b == c.b); }
    // ignores alpha
    inline void setColor (const RGB &c) { r = c.r; g = c.g; b = c.b; }
    inline void setColor (const RGBA &c) { r = c.r; g = c.g; b = c.b; }
  };

  struct __attribute__((aligned(1), packed)) RGBA {
    vuint8  r, g, b, a;
    RGBA () : r(0), g(0), b(0), a(0) {}
    RGBA (vuint8 ar, vuint8 ag, vuint8 ab, vuint8 aa=255) : r(ar), g(ag), b(ab), a(aa) {}
    RGBA (const RGB &c) : r(c.r), g(c.g), b(c.b), a(255) {}
    RGBA (const RGBA &c) : r(c.r), g(c.g), b(c.b), a(c.a) {}
    inline bool operator == (const RGB &c) const { return (a == 255 && r == c.r && g == c.g && b == c.b); }
    inline bool operator == (const RGBA &c) const { return (a == c.a && r == c.r && g == c.g && b == c.b); }
    //inline int distSq (const RGB &c) const { return (r-c.r)*(r-c.r)+(g-c.g)*(g-c.g)+(b-c.b)*(b-c.b)+(255-a)*(255-a); }
    //inline int distSq (const RGBA &c) const { return (r-c.r)*(r-c.r)+(g-c.g)*(g-c.g)+(b-c.b)*(b-c.b)+(a-c.a)*(a-c.a); }
    inline int distSq (const RGBA &c) const { return rgbDistanceSquared(r*a/255, g*a/255, b*a/255, c.r*c.a/255, c.g*c.a/255, c.b*c.a/255); }
    inline int distSq (const RGB &c) const { return rgbDistanceSquared(r*a/255, g*a/255, b*a/255, c.r, c.g, c.b); }
    inline bool isTransparent () const { return (a == 0); }
    inline bool isOpaque () const { return (a == 255); }
    // ignores alpha
    inline bool sameColor (const RGB &c) const { return (r == c.r && g == c.g && b == c.b); }
    inline bool sameColor (const RGBA &c) const { return (r == c.r && g == c.g && b == c.b); }
    // ignores alpha
    inline void setColor (const RGB &c) { r = c.r; g = c.g; b = c.b; }
    inline void setColor (const RGBA &c) { r = c.r; g = c.g; b = c.b; }
  };

protected:
  ImageType mFormat;
  int mWidth, mHeight;
  VStr mName;
  VStr mType;

  vuint8 *mPixels;
  RGBA mPalette[256];
  int mPalUsed; // number of used colors in palette

private:
  ImageType getFormat () const { return mFormat; }
  int getWidth () const { return mWidth; }
  int getHeight () const { return mHeight; }
  const VStr &getName () const { return mName; }
  void setName (const VStr &aname) { mName = aname; }
  const VStr &getType () const { return mType; }
  void setType (const VStr &aname) { mType = aname; }

  bool getIsTrueColor () const { return (mFormat == IT_RGBA); }
  bool getHasPalette () const { return (mPalUsed > 0); }
  int getPalUsed () const { return mPalUsed; }
  const RGBA *getPalette () const { return mPalette; }
  const vuint8 *getPixels () const { return mPixels; }

public:
  PropertyRO<ImageType, VImage> format {this, &VImage::getFormat};
  PropertyRO<int, VImage> width {this, &VImage::getWidth};
  PropertyRO<int, VImage> height {this, &VImage::getHeight};
  Property<const VStr &, VImage> name {this, &VImage::getName, &VImage::setName};
  Property<const VStr &, VImage> type {this, &VImage::getType, &VImage::setType};
  PropertyRO<bool, VImage> isTrueColor {this, &VImage::getIsTrueColor};
  PropertyRO<bool, VImage> hasPalette {this, &VImage::getHasPalette}; // note that non-tc images can still be palette-less
  PropertyRO<int, VImage> palUsed {this, &VImage::getPalUsed};
  PropertyRO<const RGBA *, VImage> palette {this, &VImage::getPalette};
  PropertyRO<const vuint8 *, VImage> pixels {this, &VImage::getPixels};

private:
  VImage (const VImage &); // no copies

  vuint8 appendColor (const RGBA &col);

public:
  //VImage ();
  VImage (ImageType atype, int awidth, int aheight); // pixels aren't initialized
  ~VImage ();

  // load image from stream; return `nullptr` on error
  static VImage *loadFrom (VStream *strm, const VStr &name=VStr());

  void checkerFill ();

  void setPalette (const RGBA *pal, int colnum);

  RGBA getPixel (int x, int y) const;
  void setPixel (int x, int y, const RGBA &col);

  // has any sense only for paletted images
  void setBytePixel (int x, int y, vuint8 b);

  // filters edges in RGBA images
  void smoothEdges ();
};


// ////////////////////////////////////////////////////////////////////////// //
typedef VImage *(*VImageLoaderFn) (VStream *);

// `ext` may, or may not include dot
// loaders with higher priority will be tried first
void ImagoRegisterLoader (const char *fmtext, const char *fmtdesc, VImageLoaderFn ldr, int prio=1000);


#endif
