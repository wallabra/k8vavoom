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
class VVideo : public VObject {
  DECLARE_ABSTRACT_CLASS(VVideo, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VVideo)

private:
  static bool mInited;
  static int mWidth, mHeight;
  static bool doGLSwap;
  static bool doRefresh;
  static bool quitSignal;
  static VMethod *onDrawVC;
  static VMethod *onEventVC;

  static int colorR;
  static int colorG;
  static int colorB;
  static int colorA;
  static VFont *currFont;
  friend class VGLTexture;

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
  static void clearColored (int r, int g, int b);

  static void runEventLoop ();

  static void setFont (VName fontname);
  static void setColor (int r, int g, int b);
  static void setAlpha (int a);

  static void drawTextAt (int x, int y, const VStr &text);

  // returns timer id or 0
  // if id <= 0, creates new unique timer id
  // if interval is < 1, returns with error and won't create timer
  static int CreateTimerWithId (int id, int intervalms, bool oneshot=false);
  static bool DeleteTimer (int id); // `true`: deleted, `false`: no such timer
  static bool IsTimerExists (int id);
  static bool IsTimerOneShot (int id);
  static int GetTimerInterval (int id); // 0: no such timer
  // returns success flag; won't do anything if interval is < 1
  static bool SetTimerInterval (int id, int intervalms);

  static void sendPing ();

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

  DECLARE_FUNCTION(getScissorEnabled)
  DECLARE_FUNCTION(setScissorEnabled)
  DECLARE_FUNCTION(getScissor)
  DECLARE_FUNCTION(setScissor)
  DECLARE_FUNCTION(copyScissor)

  DECLARE_FUNCTION(clearScreen)

  DECLARE_FUNCTION(setColor)
  DECLARE_FUNCTION(setAlpha)

  DECLARE_FUNCTION(setSmoothLine)

  DECLARE_FUNCTION(colorR)
  DECLARE_FUNCTION(colorG)
  DECLARE_FUNCTION(colorB)
  DECLARE_FUNCTION(colorA)

  DECLARE_FUNCTION(setFont)
  DECLARE_FUNCTION(fontHeight)
  DECLARE_FUNCTION(charWidth)
  DECLARE_FUNCTION(spaceWidth)
  DECLARE_FUNCTION(textWidth)
  DECLARE_FUNCTION(textHeight)
  DECLARE_FUNCTION(drawTextAt)

  DECLARE_FUNCTION(drawLine)
  DECLARE_FUNCTION(drawRect)
  DECLARE_FUNCTION(fillRect)
};


// ////////////////////////////////////////////////////////////////////////// //
// refcounted object
class VGLTexture {
private:
  int rc;
  VStr mPath;

public:
  const VStr &getPath () const { return mPath; }
  int getRC () const { return rc; }

public:
  VImage *img;
  GLuint tid; // !0: texture loaded
  VGLTexture *prev;
  VGLTexture *next;

private:
  void registerMe ();

public:
  VGLTexture ();
  VGLTexture (VImage *aimg, const VStr &apath);
  ~VGLTexture (); // don't call this manually!

  void addRef ();
  void release (); //WARNING: can delete `this`!

  static VGLTexture *Load (const VStr &fname);

  int getWidth () const { return (img ? img->width : 0); }
  int getHeight () const { return (img ? img->height : 0); }

  PropertyRO<const VStr &, VGLTexture> path {this, &VGLTexture::getPath};
  PropertyRO<int, VGLTexture> width {this, &VGLTexture::getWidth};
  PropertyRO<int, VGLTexture> height {this, &VGLTexture::getHeight};

  void blitExt (int dx0, int dy0, int dx1, int dy1, int x0, int y0, int x1, int y1) const;
  void blitAt (int dx0, int dy0, float scale=1) const;
};


// ////////////////////////////////////////////////////////////////////////// //
// VaVoom C wrapper
class VTexture : public VObject {
  DECLARE_CLASS(VTexture, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VTexture)

private:
  VGLTexture *tex;

public:
  virtual void Destroy () override;

public:
  DECLARE_FUNCTION(Destroy)
  DECLARE_FUNCTION(Load) // Texture load (string fname)
  DECLARE_FUNCTION(width)
  DECLARE_FUNCTION(height)
  DECLARE_FUNCTION(blitExt)
  DECLARE_FUNCTION(blitAt)
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
    VGLTexture *tex; // don't destroy this!
  };

protected:
  VName name;
  VFont *next;
  VGLTexture *tex;

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
  inline const VGLTexture *getTexture () const { return tex; }

public:
  static VFont *findFont (VName name);
};


#endif

#endif
