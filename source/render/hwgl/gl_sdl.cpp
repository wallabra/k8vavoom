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
#include <SDL.h>
#include "gl_local.h"


extern VCvarB want_mouse_at_zero;


class VSdlOpenGLDrawer : public VOpenGLDrawer {
private:
  bool skipAdaptiveVSync;

public:
  SDL_Window *hw_window;
  SDL_GLContext hw_glctx;

  virtual void Init () override;
  virtual bool SetResolution (int, int, int) override;
  virtual void *GetExtFuncPtr (const char *) override;
  virtual void Update () override;
  virtual void Shutdown () override;

  virtual void WarpMouseToWindowCenter () override;
  virtual void GetMousePosition (int *mx, int *my) override;

  virtual void GetRealWindowSize (int *rw, int *rh) override;

private:
  void SetVSync (bool firstTime);
};


IMPLEMENT_DRAWER(VSdlOpenGLDrawer, DRAWER_OpenGL, "OpenGL", "SDL OpenGL rasteriser device", "-opengl");

VCvarI gl_current_screen_fsmode("gl_current_screen_fsmode", "0", "Video mode: windowed(0), fullscreen scaled(1), fullscreen real(2)", CVAR_Rom);


//==========================================================================
//
//  VSdlOpenGLDrawer::Init
//
//  Determine the hardware configuration
//
//==========================================================================
void VSdlOpenGLDrawer::Init () {
  hw_window = nullptr;
  hw_glctx = nullptr;
  mInitialized = false; // just in case
  skipAdaptiveVSync = false;
}


//==========================================================================
//
//  VSdlOpenGLDrawer::WarpMouseToWindowCenter
//
//  k8: somebody should fix this; i don't care
//
//==========================================================================
void VSdlOpenGLDrawer::WarpMouseToWindowCenter () {
  if (!hw_window) return;
  /*
  if (SDL_GetMouseFocus() == hw_window) {
    SDL_WarpMouseInWindow(hw_window, ScreenWidth/2, ScreenHeight/2);
  }
  */
  //int wx, wy;
  //SDL_GetWindowPosition(hw_window, &wx, &wy);
  //SDL_WarpMouseGlobal(wx+ScreenWidth/2, wy+ScreenHeight/2);
  SDL_WarpMouseInWindow(hw_window, ScreenWidth/2, ScreenHeight/2);
}


//==========================================================================
//
//  VSdlOpenGLDrawer::GetMousePosition
//
//==========================================================================
void VSdlOpenGLDrawer::GetMousePosition (int *mx, int *my) {
  int xp = 0, yp = 0;
  if (hw_window) {
    SDL_GetGlobalMouseState(&xp, &yp);
    int wx, wy;
    SDL_GetWindowPosition(hw_window, &wx, &wy);
    xp -= wx;
    yp -= wy;
  }
  if (mx) *mx = xp;
  if (my) *my = yp;
}


//==========================================================================
//
//  VSdlOpenGLDrawer::GetRealWindowSize
//
//==========================================================================
void VSdlOpenGLDrawer::GetRealWindowSize (int *rw, int *rh) {
  if (!rw && !rh) return;
  int realw = ScreenWidth, realh = ScreenHeight;
  if (hw_window) SDL_GL_GetDrawableSize(hw_window, &realw, &realh);
  if (rw) *rw = realw;
  if (rh) *rh = realh;
}


//==========================================================================
//
//  VSdlOpenGLDrawer::SetVSync
//
//==========================================================================
void VSdlOpenGLDrawer::SetVSync (bool firstTime) {
  if (r_vsync) {
    if (r_vsync_adaptive && !skipAdaptiveVSync) {
      if (SDL_GL_SetSwapInterval(-1) == -1) {
        if (!firstTime) {
          GCon->Log("OpenGL: adaptive vsync failed, falling back to normal vsync");
          skipAdaptiveVSync = true;
        }
        SDL_GL_SetSwapInterval(1);
      } else {
        GCon->Log("OpenGL: using adaptive vsync");
      }
    } else {
      SDL_GL_SetSwapInterval(1);
    }
  } else {
    SDL_GL_SetSwapInterval(0);
  }
}


//==========================================================================
//
//  VSdlOpenGLDrawer::SetResolution
//
//  set up the video mode
//  fsmode:
//    0: windowed
//    1: scaled FS
//    2: real FS
//
//==========================================================================
bool VSdlOpenGLDrawer::SetResolution (int AWidth, int AHeight, int fsmode) {
  guard(VSdlOpenGLDrawer::SetResolution);
  int Width = AWidth;
  int Height = AHeight;
  if (Width < 320 || Height < 200) {
    // set defaults
    Width = 800;
    Height = 600;
  }

  if (fsmode < 0 || fsmode > 2) fsmode = 0;

  // shut down current mode
  Shutdown();

  Uint32 flags = SDL_WINDOW_OPENGL;
       if (fsmode == 1) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
  else if (fsmode == 2) flags |= SDL_WINDOW_FULLSCREEN;

  //k8: require OpenGL 2.1, sorry; non-shader renderer was removed anyway
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#ifdef __SWITCH__
  //fgsfds: libdrm_nouveau requires this, or else shit will be trying to use GLES
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
#endif

  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  // doing it twice is required for some broken setups. oops.
  SetVSync(true); // first time

  hw_window = SDL_CreateWindow("k8VaVoom", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, Width, Height, flags);
  if (!hw_window) {
    GCon->Logf("SDL2: cannot create SDL2 window.");
    return false;
  }

  hw_glctx = SDL_GL_CreateContext(hw_window);
  if (!hw_glctx) {
    SDL_DestroyWindow(hw_window);
    hw_window = nullptr;
    GCon->Logf("SDL2: cannot initialize OpenGL 2.1 with stencil buffer.");
    return false;
  }

  SDL_GL_MakeCurrent(hw_window, hw_glctx);
#ifdef USE_GLAD
  GCon->Logf("Loading GL procs using GLAD");
  if (!gladLoadGLLoader(SDL_GL_GetProcAddress)) GCon->Logf("GLAD failed to load GL procs!\n");
#endif

  SetVSync(false); // second time
  //SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

  // Everything is fine, set some globals and finish
  ScreenWidth = Width;
  ScreenHeight = Height;

  //SDL_DisableScreenSaver();

  gl_current_screen_fsmode = fsmode;

  callICB(VCB_InitVideo);

  return true;
  unguard;
}


//==========================================================================
//
//  VSdlOpenGLDrawer::GetExtFuncPtr
//
//==========================================================================
void *VSdlOpenGLDrawer::GetExtFuncPtr (const char *name) {
  guard(VSdlOpenGLDrawer::GetExtFuncPtr);
  return SDL_GL_GetProcAddress(name);
  unguard;
}


//==========================================================================
//
//  VSdlOpenGLDrawer::Update
//
//  Blit to the screen / Flip surfaces
//
//==========================================================================
void VSdlOpenGLDrawer::Update () {
  guard(VSdlOpenGLDrawer::Update);
  if (mInitialized && hw_window && hw_glctx) callICB(VCB_FinishUpdate);
  FinishUpdate();
  if (hw_window) SDL_GL_SwapWindow(hw_window);
  unguard;
}


//==========================================================================
//
//  VSdlOpenGLDrawer::Shutdown
//
//  Close the graphics
//
//==========================================================================
void VSdlOpenGLDrawer::Shutdown() {
  guard(VSdlOpenGLDrawer::Shutdown);
  if (hw_glctx && mInitialized) callICB(VCB_DeinitVideo);
  DeleteTextures();
  if (hw_glctx) {
    if (hw_window) SDL_GL_MakeCurrent(hw_window, hw_glctx);
    SDL_GL_DeleteContext(hw_glctx);
    hw_glctx = nullptr;
  }
  if (hw_window) {
    SDL_DestroyWindow(hw_window);
    hw_window = nullptr;
  }
  mInitialized = false;
  if (want_mouse_at_zero) SDL_WarpMouseGlobal(0, 0);
  unguard;
}
