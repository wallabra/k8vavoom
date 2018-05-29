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

  // static
  DECLARE_FUNCTION(canInit)
  DECLARE_FUNCTION(hasOpenGL)
  DECLARE_FUNCTION(isInitialized)
  DECLARE_FUNCTION(width)
  DECLARE_FUNCTION(height)

  DECLARE_FUNCTION(open)
  DECLARE_FUNCTION(close)

  DECLARE_FUNCTION(clear)

  DECLARE_FUNCTION(runEventLoop)

  DECLARE_FUNCTION(requestRefresh)
  DECLARE_FUNCTION(requestQuit)
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
  VTexture (VImage *aimg);
  //virtual ~VTexture () overload;

  void Destroy ();

  void clear ();

  bool loadFrom (VStream *st);

  static VTexture *load (const VStr &fname);

  int getWidth () const { return (img ? img->width : 0); }
  int getHeight () const { return (img ? img->height : 0); }

  void blitExt (int dx0, int dy0, int dx1, int dy1, int x0, int y0, int x1, int y1);

public:
  //PropertyRO<int, VTexture> width {this, &VTexture::getWidth};
  //PropertyRO<int, VTexture> height {this, &VTexture::getHeight};

  DECLARE_FUNCTION(Destroy)
  DECLARE_FUNCTION(load) // Texture load (string fname)
  DECLARE_FUNCTION(width)
  DECLARE_FUNCTION(height)
  DECLARE_FUNCTION(blitExt)
};


#endif

#endif
