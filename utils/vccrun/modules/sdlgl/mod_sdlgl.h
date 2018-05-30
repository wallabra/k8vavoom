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
#ifndef VCCMOD_SDLGL_HEADER_FILE
#define VCCMOD_SDLGL_HEADER_FILE

#include "../../../../libs/imago/imago.h"
#include "../../vcc_run.h"

#if defined (VCCRUN_HAS_SDL) && defined(VCCRUN_HAS_OPENGL)
#include <GL/gl.h>


// ////////////////////////////////////////////////////////////////////////// //
struct event_t;
class VFont;


// ////////////////////////////////////////////////////////////////////////// //
class VVideoMode : public VObject {
  DECLARE_CLASS(VVideoMode, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VVideoMode)

private:
  static bool mInited;
  static int mWidth, mHeight;
  static bool doGLSwap;
  static bool doRefresh;
  static bool quitSignal;
  static VMethod *onDrawVC;
  static VMethod *onEventVC;

  static int fontR;
  static int fontG;
  static int fontB;
  static int fontA;
  static VFont *currFont;

private:
  static void initMethods ();
  static void onDraw ();
  static void onEvent (event_t &evt);

public:
  static bool canInit ();
  static bool hasOpenGL ();
  static bool isInitialized ();
  static int getWidth ();
  static int getHeight ();

  static bool open (const VStr &winname, int width, int height);
  static void close ();

  static void clear ();

  static void runEventLoop ();

  static void setFont (VName fontname);
  static void setTextColor (int r, int g, int b);
  static void setTextAlpha (int a);

  static void drawTextAt (int x, int y, const VStr &text);

  // static
  DECLARE_FUNCTION(canInit)
  DECLARE_FUNCTION(hasOpenGL)
  DECLARE_FUNCTION(isInitialized)
  DECLARE_FUNCTION(screenWidth)
  DECLARE_FUNCTION(screenHeight)

  DECLARE_FUNCTION(openScreen)
  DECLARE_FUNCTION(closeScreen)

  DECLARE_FUNCTION(loadFont)

  DECLARE_FUNCTION(runEventLoop)

  DECLARE_FUNCTION(requestRefresh)
  DECLARE_FUNCTION(requestQuit)

  DECLARE_FUNCTION(clearScreen)

  DECLARE_FUNCTION(setTextFont)
  DECLARE_FUNCTION(setTextColor)
  DECLARE_FUNCTION(setTextAlpha)
  DECLARE_FUNCTION(fontHeight)
  DECLARE_FUNCTION(charWidth)
  DECLARE_FUNCTION(spaceWidth)
  DECLARE_FUNCTION(textWidth)
  DECLARE_FUNCTION(textHeight)
  DECLARE_FUNCTION(drawTextAt)
};


// ////////////////////////////////////////////////////////////////////////// //
class VTexture : public VObject {
  DECLARE_CLASS(VTexture, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VTexture)

public:
  VImage *img;
  GLuint tid; // !0: texture loaded
  VTexture *prev;
  VTexture *next;

private:
  void registerMe ();

public:
  //VTexture (VImage *aimg);
  //virtual ~VTexture () overload;

  void Destroy ();

  void clear ();

  bool loadFrom (VStream *st);

  static VTexture *load (const VStr &fname);
  static VTexture *createFromImage (VImage *aimg);

  int getWidth () const { return (img ? img->width : 0); }
  int getHeight () const { return (img ? img->height : 0); }

  void blitExt (int dx0, int dy0, int dx1, int dy1, int x0, int y0, int x1, int y1) const;

public:
  //PropertyRO<int, VTexture> width {this, &VTexture::getWidth};
  //PropertyRO<int, VTexture> height {this, &VTexture::getHeight};

  DECLARE_FUNCTION(Destroy)
  DECLARE_FUNCTION(load) // Texture load (string fname)
  DECLARE_FUNCTION(width)
  DECLARE_FUNCTION(height)
  DECLARE_FUNCTION(blitExt)
};


// ////////////////////////////////////////////////////////////////////////// //
// base class for fonts
class VFont {
protected:
  static VFont *fontList;

public:
  struct FontChar {
    int ch;
    int width, height; // height may differ from font height
    int advance; // horizontal advance to print next char
    int topofs; // offset from font top (i.e. y+topofs should be used to draw char)
    float tx0, ty0; // texture coordinates, [0..1)
    float tx1, ty1; // texture coordinates, [0..1) -- cached for convenience
    VTexture *tex; // don't destroy this!
  };

protected:
  VName name;
  VFont *next;
  VTexture *tex;

  // font characters (cp1251)
  TArray<FontChar> chars;
  int spaceWidth; // width of the space character
  int fontHeight; // height of the font

  // fast look-up for 1251 encoding characters
  //int chars1251[256];
  // range of available characters
  //int firstChar;
  //int lastChar;

public:
  //VFont ();
  VFont (VName aname, const VStr &fnameIni, const VStr &fnameTexture);
  ~VFont ();

  const FontChar *getChar (int ch) const;
  int charWidth (int ch) const;
  int textWidth (const VStr &s) const;
  int textHeight (const VStr &s) const;
  // will clear lines; returns maximum text width
  //int splitTextWidth (const VStr &text, TArray<VSplitLine> &lines, int maxWidth) const;

  inline VName getName () const { return name; }
  inline int getSpaceWidth () const { return spaceWidth; }
  inline int getHeight () const { return fontHeight; }
  inline const VTexture *getTexture () const { return tex; }

public:
  static VFont *findFont (VName name);
};


#endif

#endif
