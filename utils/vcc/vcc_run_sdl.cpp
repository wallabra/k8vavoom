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

#include "vcc_run_sdl.h"

#ifdef VCCRUN_HAS_SDL
# include <SDL.h>
static SDL_Window* hw_window = nullptr;
static SDL_GLContext hw_glctx = nullptr;
#endif

#ifdef VCCRUN_HAS_OPENGL
# include <GL/gl.h>
#endif


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_CLASS(V, VideoMode);

bool VVideoMode::mInited = false;
int VVideoMode::mWidth = 0;
int VVideoMode::mHeight = 0;


// ////////////////////////////////////////////////////////////////////////// //
bool VVideoMode::canInit () {
#ifdef VCCRUN_HAS_SDL
  return true;
#else
  return false;
#endif
}


bool VVideoMode::hasOpenGL () {
#if defined(VCCRUN_HAS_SDL) && defined(VCCRUN_HAS_OPENGL)
  return true;
#else
  return false;
#endif
}


bool VVideoMode::isInitialized () { return mInited; }
int VVideoMode::getWidth () { return mWidth; }
int VVideoMode::getHeight () { return mHeight; }


// ////////////////////////////////////////////////////////////////////////// //
void VVideoMode::close () {
#if defined(VCCRUN_HAS_SDL) && defined(VCCRUN_HAS_OPENGL)
  if (mInited) {
    if (hw_glctx) {
      if (hw_window) SDL_GL_MakeCurrent(hw_window, hw_glctx);
      SDL_GL_DeleteContext(hw_glctx);
      hw_glctx = nullptr;
    }
    if (hw_window) {
      SDL_DestroyWindow(hw_window);
      hw_window = nullptr;
    }
    mInited = false;
    mWidth = 0;
    mHeight = 0;
  }
#endif
}


bool VVideoMode::open (const VStr &winname, int width, int height) {
#if defined(VCCRUN_HAS_SDL) && defined(VCCRUN_HAS_OPENGL)
  if (!width || !height) {
    width = 800;
    height = 600;
  }

  close();

  Uint32 flags = SDL_WINDOW_OPENGL;
  //if (!Windowed) flags |= SDL_WINDOW_FULLSCREEN;

  //k8: require OpenGL 1.5
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);

  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  //SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, r_vsync);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  /*
  if (r_vsync) {
    if (SDL_GL_SetSwapInterval(-1) == -1) SDL_GL_SetSwapInterval(1);
  } else {
    SDL_GL_SetSwapInterval(0);
  }
  */
  SDL_GL_SetSwapInterval(0);

  hw_window = SDL_CreateWindow((winname.length() ? *winname : "Untitled"), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, flags);
  if (!hw_window) {
    //GCon->Logf("ALAS: cannot create SDL2 window.");
    return false;
  }

  hw_glctx = SDL_GL_CreateContext(hw_window);
  if (!hw_glctx) {
    SDL_DestroyWindow(hw_window);
    hw_window = nullptr;
    return false;
  }

  SDL_GL_MakeCurrent(hw_window, hw_glctx);

  //SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, r_vsync);
  /*
  if (r_vsync) {
    if (SDL_GL_SetSwapInterval(-1) == -1) SDL_GL_SetSwapInterval(1);
  } else {
    SDL_GL_SetSwapInterval(0);
  }
  */
  SDL_GL_SetSwapInterval(0);

  // everything is fine, set some globals and finish
  mWidth = width;
  mHeight = height;
  mInited = true;

  //SDL_DisableScreenSaver();

  glViewport(0, 0, width, height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, width, height, 0, -99999, 99999);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);

  return true;
#else
  return false;
#endif
}


// ////////////////////////////////////////////////////////////////////////// //
void VVideoMode::runEventLoop () {
#if defined(VCCRUN_HAS_SDL) && defined(VCCRUN_HAS_OPENGL)
  if (!mInited) return;

  bool doQuit = false;
  while (!doQuit) {
    SDL_Event ev;
    //event_t vev;

    SDL_PumpEvents();
    while (SDL_PollEvent(&ev)) {
      switch (ev.type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
          /*
          {
            int kk = sdl2TranslateKey(ev.key.keysym.sym);
            if (kk > 0) GInput->KeyEvent(kk, (ev.key.state == SDL_PRESSED) ? 1 : 0);
          }
          */
          break;
        /*
        case SDL_MOUSEMOTION:
          vev.type = ev_mouse;
          vev.data1 = 0;
          vev.data2 = ev.motion.xrel;
          vev.data3 = ev.motion.yrel;
          GInput->PostEvent(&vev);
          break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
          vev.type = (ev.button.state == SDL_PRESSED) ? ev_keydown : ev_keyup;
               if (ev.button.button == SDL_BUTTON_LEFT) vev.data1 = K_MOUSE1;
          else if (ev.button.button == SDL_BUTTON_RIGHT) vev.data1 = K_MOUSE2;
          else if (ev.button.button == SDL_BUTTON_MIDDLE) vev.data1 = K_MOUSE3;
          //else if (ev.button.button == SDL_BUTTON_WHEELUP) vev.data1 = K_MWHEELUP;
          //else if (ev.button.button == SDL_BUTTON_WHEELDOWN) vev.data1 = K_MWHEELDOWN;
          else break;
          vev.data2 = 0;
          vev.data3 = 0;
          if (ui_mouse || !ui_active) GInput->PostEvent(&vev);
          break;
        case SDL_MOUSEWHEEL:
          vev.type = ev_keydown;
               if (ev.wheel.y > 0) vev.data1 = K_MWHEELUP;
          else if (ev.wheel.y < 0) vev.data1 = K_MWHEELDOWN;
          else break;
          vev.data2 = 0;
          vev.data3 = 0;
          if (ui_mouse || !ui_active) GInput->PostEvent(&vev);
          break;
        */
        /*
        case SDL_WINDOWEVENT:
          switch (ev.window.event) {
            case SDL_WINDOWEVENT_FOCUS_GAINED:
              //fprintf(stderr, "***FOCUS GAIN; wa=%d; first=%d; drawer=%p\n", (int)winactive, (int)firsttime, Drawer);
              if (!winactive && mouse) {
                if (Drawer) {
                  Drawer->WarpMouseToWindowCenter();
                  SDL_GetMouseState(&mouse_oldx, &mouse_oldy);
                }
              }
              firsttime = true;
              winactive = true;
              break;
            case SDL_WINDOWEVENT_FOCUS_LOST:
              //fprintf(stderr, "***FOCUS LOST; first=%d; drawer=%p\n", (int)firsttime, Drawer);
              winactive = false;
              firsttime = true;
              break;
            //case SDL_WINDOWEVENT_TAKE_FOCUS: Drawer->SDL_SetWindowInputFocus();
          }
          break;
        */
        case SDL_QUIT:
          doQuit = true;
          break;
        default:
          break;
      }

      // read mouse separately
      /*
      if (mouse && winactive && Drawer) {
        if (!ui_active || ui_mouse) {
          uiwasactive = ui_active;
          SDL_GetMouseState(&mouse_x, &mouse_y);
          int dx = mouse_x-ScreenWidth/2;
          int dy = ScreenHeight/2-mouse_y;
          if (firsttime) {
            Drawer->WarpMouseToWindowCenter();
            SDL_GetMouseState(&mouse_x, &mouse_y);
            if (mouse_x == mouse_oldx && mouse_y == mouse_oldy) dx = dy = 0;
          }
          if (dx || dy) {
            //SDL_GetMouseState(&mouse_oldx, &mouse_oldy);
            mouse_oldx = mouse_x;
            mouse_oldy = mouse_y;
            //fprintf(stderr, "mx=%d; my=%d; dx=%d, dy=%d\n", mouse_x, mouse_y, dx, dy);
            vev.type = ev_mouse;
            vev.data1 = 0;
            vev.data2 = dx;
            vev.data3 = dy;
            GInput->PostEvent(&vev);
            //SDL_WarpMouse(ScreenWidth / 2, ScreenHeight / 2);
            if (Drawer) { firsttime = false; Drawer->WarpMouseToWindowCenter(); }
          }
        } else if (ui_active != uiwasactive) {
          uiwasactive = ui_active;
          if (!ui_active) {
            //SDL_WarpMouse(ScreenWidth / 2, ScreenHeight / 2);
            if (Drawer) {
              firsttime = true;
              Drawer->WarpMouseToWindowCenter();
              SDL_GetMouseState(&mouse_oldx, &mouse_oldy);
            }
          }
        }
      }
      */
    }

    SDL_GL_SwapWindow(hw_window);
  }
#endif
}


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_FUNCTION(VVideoMode, canInit) { RET_BOOL(VVideoMode::canInit()); }
IMPLEMENT_FUNCTION(VVideoMode, hasOpenGL) { RET_BOOL(VVideoMode::hasOpenGL()); }
IMPLEMENT_FUNCTION(VVideoMode, isInitialized) { RET_BOOL(VVideoMode::isInitialized()); }
IMPLEMENT_FUNCTION(VVideoMode, getWidth) { RET_INT(VVideoMode::getWidth()); }
IMPLEMENT_FUNCTION(VVideoMode, getHeight) { RET_INT(VVideoMode::getHeight()); }

IMPLEMENT_FUNCTION(VVideoMode, close) { VVideoMode::close(); }

IMPLEMENT_FUNCTION(VVideoMode, open) {
  P_GET_INT(hgt);
  P_GET_INT(wdt);
  P_GET_STR(wname);
  RET_BOOL(VVideoMode::open(wname, wdt, hgt));
}

IMPLEMENT_FUNCTION(VVideoMode, runEventLoop) { VVideoMode::runEventLoop(); }
