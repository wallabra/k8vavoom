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
#if defined (VCCRUN_HAS_SDL) && defined(VCCRUN_HAS_OPENGL)

#include "mod_sdlgl.h"
#include "../../filesys/fsys.h"

#include <SDL.h>
#include <GL/gl.h>
static SDL_Window *hw_window = nullptr;
static SDL_GLContext hw_glctx = nullptr;
static VTexture *txHead = nullptr, *txTail = nullptr;


// ////////////////////////////////////////////////////////////////////////// //
static bool texUpload (VTexture *tx) {
  if (!tx) return false;
  if (!tx->img) { tx->tid = 0; return false; }

  tx->tid = 0;
  glGenTextures(1, &tx->tid);
  if (tx->tid == 0) return false;

  glBindTexture(GL_TEXTURE_2D, tx->tid);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  if (!tx->img->isTrueColor) {
    VImage *tc = new VImage(VImage::ImageType::IT_RGBA, tx->img->width, tx->img->height);
    for (int y = 0; y < tx->img->height; ++y) {
      for (int x = 0; x < tx->img->width; ++x) {
        tc->setPixel(x, y, tx->img->getPixel(x, y));
      }
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tc->width, tc->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tc->pixels);
    delete tc;
  } else {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tx->img->width, tx->img->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tx->img->pixels);
  }

  return true;
}


void deleteAllTextures () {
  if (!hw_glctx) return;
  glBindTexture(GL_TEXTURE_2D, 0);
  for (VTexture *tx = txHead; tx; tx = tx->next) {
    if (tx->tid) { glDeleteTextures(1, &tx->tid); tx->tid = 0; }
  }
}


void uploadAllTextures () {
  if (!hw_glctx) return;
  for (VTexture *tx = txHead; tx; tx = tx->next) texUpload(tx);
}


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_CLASS(V, Texture);

VTexture::VTexture (VImage *aimg) : img(aimg), tid(0), prev(nullptr), next(nullptr) {
  if (hw_glctx) texUpload(this);
  prev = txTail;
  if (txTail) txTail->next = this; else txHead = this;
  txTail = this;
}


void VTexture::Destroy () {
  if (hw_glctx && tid) {
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &tid);
  }
  delete img;
  if (prev) prev->next = next; else txHead = next;
  if (next) next->prev = prev; else txTail = prev;
  tid = 0;
  img = nullptr;
  Super::Destroy();
}


void VTexture::clear () {
  if (hw_glctx && tid) {
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &tid);
  }
  tid = 0;
  delete img;
  img = nullptr;
}


bool VTexture::loadFrom (VStream *st) {
  clear();
  if (!st || st->IsError()) return false;
  img = VImage::loadFrom(st);
  if (st->IsError()) { clear(); return false; }
  if (!img) return false;
  if (hw_glctx) texUpload(this);
  return true;
}


VTexture *VTexture::load (const VStr &fname) {
  VStr rname = fsysFileFindAnyExt(fname);
  if (rname.length() == 0) return nullptr;
  VStream *st = fsysOpenFile(rname);
  if (!st) return nullptr;
  VImage *img = VImage::loadFrom(st);
  delete st;
  if (!img) return nullptr;
  return new VTexture(img);
}


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_FUNCTION(VTexture, Destroy) {
  P_GET_SELF;
  delete Self;
}


IMPLEMENT_FUNCTION(VTexture, load) {
  P_GET_STR(fname);
  VClass *iclass = VClass::FindClass("Texture");
  if (iclass) {
    VStr rname = fsysFileFindAnyExt(fname);
    if (rname.length() != 0) {
      VStream *st = fsysOpenFile(rname);
      if (st) {
        auto ifileo = VObject::StaticSpawnObject(iclass);
        auto ifile = (VTexture *)ifileo;
        if (!ifile->loadFrom(st)) { delete ifileo; ifileo = nullptr; }
        delete st;
        RET_REF((VObject *)ifileo);
      }
    }
  } else {
    RET_REF(nullptr);
  }
}


IMPLEMENT_FUNCTION(VTexture, width) {
  P_GET_SELF;
  RET_INT(Self ? Self->getWidth() : 0);
}


IMPLEMENT_FUNCTION(VTexture, height) {
  P_GET_SELF;
  RET_INT(Self ? Self->getHeight() : 0);
}


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_CLASS(V, VideoMode);

bool VVideoMode::mInited = false;
int VVideoMode::mWidth = 0;
int VVideoMode::mHeight = 0;


// ////////////////////////////////////////////////////////////////////////// //
bool VVideoMode::canInit () {
  return true;
}


bool VVideoMode::hasOpenGL () {
  return true;
}


bool VVideoMode::isInitialized () { return mInited; }
int VVideoMode::getWidth () { return mWidth; }
int VVideoMode::getHeight () { return mHeight; }


// ////////////////////////////////////////////////////////////////////////// //
void VVideoMode::close () {
  if (mInited) {
    if (hw_glctx) {
      if (hw_window) {
        SDL_GL_MakeCurrent(hw_window, hw_glctx);
        deleteAllTextures();
      }
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
}


bool VVideoMode::open (const VStr &winname, int width, int height) {
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
  uploadAllTextures();

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
}


// ////////////////////////////////////////////////////////////////////////// //
void VVideoMode::runEventLoop () {
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
}


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_FUNCTION(VVideoMode, canInit) { RET_BOOL(VVideoMode::canInit()); }
IMPLEMENT_FUNCTION(VVideoMode, hasOpenGL) { RET_BOOL(VVideoMode::hasOpenGL()); }
IMPLEMENT_FUNCTION(VVideoMode, isInitialized) { RET_BOOL(VVideoMode::isInitialized()); }
IMPLEMENT_FUNCTION(VVideoMode, width) { RET_INT(VVideoMode::getWidth()); }
IMPLEMENT_FUNCTION(VVideoMode, height) { RET_INT(VVideoMode::getHeight()); }

IMPLEMENT_FUNCTION(VVideoMode, close) { VVideoMode::close(); }

IMPLEMENT_FUNCTION(VVideoMode, open) {
  P_GET_INT(hgt);
  P_GET_INT(wdt);
  P_GET_STR(wname);
  RET_BOOL(VVideoMode::open(wname, wdt, hgt));
}

IMPLEMENT_FUNCTION(VVideoMode, runEventLoop) { VVideoMode::runEventLoop(); }


#endif
