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


class VSdlOpenGLDrawer : public VOpenGLDrawer {
private:
  bool skipAdaptiveVSync;

public:
  SDL_Window *hw_window;
  SDL_GLContext hw_glctx;

  virtual void Init () override;
  virtual bool SetResolution (int, int, bool) override;
  virtual void *GetExtFuncPtr (const char *) override;
  virtual void Update () override;
  virtual void Shutdown () override;

  virtual void WarpMouseToWindowCenter () override;

  virtual void GetRealWindowSize (int *rw, int *rh) override;

private:
  void SetVSync (bool firstTime);
};


IMPLEMENT_DRAWER(VSdlOpenGLDrawer, DRAWER_OpenGL, "OpenGL", "SDL OpenGL rasteriser device", "-opengl");


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
//  k8: omebody should fix this; i don't care
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
//  Set up the video mode
//
//==========================================================================
bool VSdlOpenGLDrawer::SetResolution (int AWidth, int AHeight, bool Windowed) {
  guard(VSdlOpenGLDrawer::SetResolution);
  int Width = AWidth;
  int Height = AHeight;
  if (!Width || !Height) {
    // set defaults
    Width = 800;
    Height = 600;
  }

  // shut down current mode
  Shutdown();

  Uint32 flags = SDL_WINDOW_OPENGL;
  if (!Windowed) flags |= SDL_WINDOW_FULLSCREEN;

  //k8: require OpenGL 2.1, sorry; non-shader renderer was removed anyway
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  //SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, r_vsync);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  // doing it twice is required for some broken setups. oops.
  SetVSync(true); // first time

  hw_window = SDL_CreateWindow("k8VaVoom", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, Width, Height, flags);
  if (!hw_window) {
    GCon->Logf("ALAS: cannot create SDL2 window.");
    return false;
  }

  hw_glctx = SDL_GL_CreateContext(hw_window);
  if (!hw_glctx) {
    SDL_DestroyWindow(hw_window);
    hw_window = nullptr;
    GCon->Logf("ALAS: cannot initialize OpenGL 2.1 with stencil buffer.");
    return false;
  }

  SDL_GL_MakeCurrent(hw_window, hw_glctx);

  //SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, r_vsync);
  SetVSync(false); // second time
  //SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

  // Everything is fine, set some globals and finish
  ScreenWidth = Width;
  ScreenHeight = Height;

  //SDL_DisableScreenSaver();

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
  unguard;
}
