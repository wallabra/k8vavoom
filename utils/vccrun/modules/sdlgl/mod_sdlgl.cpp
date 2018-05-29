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

bool VVideoMode::doGLSwap = false;
bool VVideoMode::doRefresh = false;
bool VVideoMode::quitSignal = false;

extern VObject *mainObject;


// ////////////////////////////////////////////////////////////////////////// //
// keys and buttons
enum {
  K_SPACE = 32,

  K_a = 97,
  K_b,
  K_c,
  K_d,
  K_e,
  K_f,
  K_g,
  K_h,
  K_i,
  K_j,
  K_k,
  K_l,
  K_m,
  K_n,
  K_o,
  K_p,
  K_q,
  K_r,
  K_s,
  K_t,
  K_u,
  K_v,
  K_w,
  K_x,
  K_y,
  K_z,

  K_UPARROW = 0x80,
  K_LEFTARROW,
  K_RIGHTARROW,
  K_DOWNARROW,
  K_INSERT,
  K_DELETE,
  K_HOME,
  K_END,
  K_PAGEUP,
  K_PAGEDOWN,

  K_PAD0,
  K_PAD1,
  K_PAD2,
  K_PAD3,
  K_PAD4,
  K_PAD5,
  K_PAD6,
  K_PAD7,
  K_PAD8,
  K_PAD9,

  K_NUMLOCK,
  K_PADDIVIDE,
  K_PADMULTIPLE,
  K_PADMINUS,
  K_PADPLUS,
  K_PADENTER,
  K_PADDOT,

  K_ESCAPE,
  K_ENTER,
  K_TAB,
  K_BACKSPACE,
  K_CAPSLOCK,

  K_F1,
  K_F2,
  K_F3,
  K_F4,
  K_F5,
  K_F6,
  K_F7,
  K_F8,
  K_F9,
  K_F10,
  K_F11,
  K_F12,

  K_LSHIFT,
  K_RSHIFT,
  K_LCTRL,
  K_RCTRL,
  K_LALT,
  K_RALT,

  K_LWIN,
  K_RWIN,
  K_MENU,

  K_PRINTSCRN,
  K_SCROLLLOCK,
  K_PAUSE,

  K_ABNT_C1,
  K_YEN,
  K_KANA,
  K_CONVERT,
  K_NOCONVERT,
  K_AT,
  K_CIRCUMFLEX,
  K_COLON2,
  K_KANJI,

  K_MOUSE1,
  K_MOUSE2,
  K_MOUSE3,

  K_MOUSED1,
  K_MOUSED2,
  K_MOUSED3,

  K_MWHEELUP,
  K_MWHEELDOWN,

  K_JOY1,
  K_JOY2,
  K_JOY3,
  K_JOY4,
  K_JOY5,
  K_JOY6,
  K_JOY7,
  K_JOY8,
  K_JOY9,
  K_JOY10,
  K_JOY11,
  K_JOY12,
  K_JOY13,
  K_JOY14,
  K_JOY15,
  K_JOY16,

  KEY_COUNT,

  SCANCODECOUNT = KEY_COUNT-0x80
};

// input event types
enum {
  ev_keydown,
  ev_keyup,
  ev_mouse,
  ev_joystick,
};

// event structure
struct event_t {
  int type; // event type
  int data1; // keys / mouse / joystick buttons
  int data2; // mouse / joystick x move
  int data3; // mouse / joystick y move
};


// ////////////////////////////////////////////////////////////////////////// //
static vuint8 sdl2TranslateKey (SDL_Keycode ksym) {
  if (ksym >= 'a' && ksym <= 'z') return (vuint8)ksym;
  if (ksym >= '0' && ksym <= '9') return (vuint8)ksym;

  switch (ksym) {
    case SDLK_UP: return K_UPARROW;
    case SDLK_LEFT: return K_LEFTARROW;
    case SDLK_RIGHT: return K_RIGHTARROW;
    case SDLK_DOWN: return K_DOWNARROW;
    case SDLK_INSERT: return K_INSERT;
    case SDLK_DELETE: return K_DELETE;
    case SDLK_HOME: return K_HOME;
    case SDLK_END: return K_END;
    case SDLK_PAGEUP: return K_PAGEUP;
    case SDLK_PAGEDOWN: return K_PAGEDOWN;

    case SDLK_KP_0: return K_PAD0;
    case SDLK_KP_1: return K_PAD1;
    case SDLK_KP_2: return K_PAD2;
    case SDLK_KP_3: return K_PAD3;
    case SDLK_KP_4: return K_PAD4;
    case SDLK_KP_5: return K_PAD5;
    case SDLK_KP_6: return K_PAD6;
    case SDLK_KP_7: return K_PAD7;
    case SDLK_KP_8: return K_PAD8;
    case SDLK_KP_9: return K_PAD9;

    case SDLK_NUMLOCKCLEAR: return K_NUMLOCK;
    case SDLK_KP_DIVIDE: return K_PADDIVIDE;
    case SDLK_KP_MULTIPLY: return K_PADMULTIPLE;
    case SDLK_KP_MINUS: return K_PADMINUS;
    case SDLK_KP_PLUS: return K_PADPLUS;
    case SDLK_KP_ENTER: return K_PADENTER;
    case SDLK_KP_PERIOD: return K_PADDOT;

    case SDLK_ESCAPE: return K_ESCAPE;
    case SDLK_RETURN: return K_ENTER;
    case SDLK_TAB: return K_TAB;
    case SDLK_BACKSPACE: return K_BACKSPACE;
    case SDLK_CAPSLOCK: return K_CAPSLOCK;

    case SDLK_F1: return K_F1;
    case SDLK_F2: return K_F2;
    case SDLK_F3: return K_F3;
    case SDLK_F4: return K_F4;
    case SDLK_F5: return K_F5;
    case SDLK_F6: return K_F6;
    case SDLK_F7: return K_F7;
    case SDLK_F8: return K_F8;
    case SDLK_F9: return K_F9;
    case SDLK_F10: return K_F10;
    case SDLK_F11: return K_F11;
    case SDLK_F12: return K_F12;

    case SDLK_LSHIFT: return K_LSHIFT;
    case SDLK_RSHIFT: return K_RSHIFT;
    case SDLK_LCTRL: return K_LCTRL;
    case SDLK_RCTRL: return K_RCTRL;
    case SDLK_LALT: return K_LALT;
    case SDLK_RALT: return K_RALT;

    case SDLK_LGUI: return K_LWIN;
    case SDLK_RGUI: return K_RWIN;
    case SDLK_MENU: return K_MENU;

    case SDLK_PRINTSCREEN: return K_PRINTSCRN;
    case SDLK_SCROLLLOCK: return K_SCROLLLOCK;
    case SDLK_PAUSE: return K_PAUSE;

    default:
      if (ksym >= ' ' && ksym < 127) return (vuint8)ksym;
      break;

    /*
    case SDLK_ABNT_C1: return K_ABNT_C1;
    case SDLK_YEN: return K_YEN;
    case SDLK_KANA: return K_KANA;
    case SDLK_CONVERT: return K_CONVERT;
    case SDLK_NOCONVERT: return K_NOCONVERT;
    case SDLK_AT: return K_AT;
    case SDLK_CIRCUMFLEX: return K_CIRCUMFLEX;
    case SDLK_COLON2: return K_COLON2;
    case SDLK_KANJI: return K_KANJI;
    */
  }

  return 0;
}


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

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tx->img->width, tx->img->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr); // this creates texture

  if (!tx->img->isTrueColor) {
    VImage *tc = new VImage(VImage::ImageType::IT_RGBA, tx->img->width, tx->img->height);
    for (int y = 0; y < tx->img->height; ++y) {
      for (int x = 0; x < tx->img->width; ++x) {
        tc->setPixel(x, y, tx->img->getPixel(x, y));
      }
    }
    delete tx->img;
    tx->img = tc;
  }
  //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tc->width, tc->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tc->pixels);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0/*x*/, 0/*y*/, tx->img->width, tx->img->height, GL_RGBA, GL_UNSIGNED_BYTE, tx->img->pixels); // this updates texture

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
  registerMe();
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


void VTexture::registerMe () {
  if (prev || next) return;
  if (txHead == this) return;
  prev = txTail;
  if (txTail) txTail->next = this; else txHead = this;
  txTail = this;
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


void VTexture::blitExt (int dx0, int dy0, int dx1, int dy1, int x0, int y0, int x1, int y1) {
  if (!tid) return;
  if (x1 < 0) x1 = img->width;
  if (y1 < 0) y1 = img->height;
  //fprintf(stderr, "blitext!\n");
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, tid);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBegin(GL_QUADS);
    glTexCoord2f((float)x0/(float)img->width, (float)y0/(float)img->height); glVertex2f(dx0, dy0);
    glTexCoord2f((float)x1/(float)img->width, (float)y0/(float)img->height); glVertex2f(dx1, dy0);
    glTexCoord2f((float)x1/(float)img->width, (float)y1/(float)img->height); glVertex2f(dx1, dy1);
    glTexCoord2f((float)x0/(float)img->width, (float)y1/(float)img->height); glVertex2f(dx0, dy1);
  glEnd();
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
        ifile->registerMe();
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


// void blitExt (int dx0, int dy0, int dx1, int dy1, int x0, int y0, int x1, int y1);
IMPLEMENT_FUNCTION(VTexture, blitExt) {
  P_GET_INT(y1);
  P_GET_INT(x1);
  P_GET_INT(y0);
  P_GET_INT(x0);
  P_GET_INT(dy1);
  P_GET_INT(dx1);
  P_GET_INT(dy0);
  P_GET_INT(dx0);
  P_GET_SELF;
  if (Self) Self->blitExt(dx0, dy0, dx1, dy1, x0, y0, x1, y1);
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
  if (width < 1 || height < 1) {
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


  glViewport(0, 0, width, height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, width, height, 0, -99999, 99999);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

  clear();

  return true;
}


void VVideoMode::clear () {
  if (!mInited) return;

  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClearDepth(1.0);
  glClearStencil(0);
  glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
}


// ////////////////////////////////////////////////////////////////////////// //
VMethod *VVideoMode::onDrawVC = nullptr;
VMethod *VVideoMode::onEventVC = nullptr;

void VVideoMode::initMethods () {
  onDrawVC = nullptr;
  onEventVC = nullptr;

  VClass *mklass = VClass::FindClass("Main");
  if (!mklass) return;

  VMethod *mmain = mklass->FindMethod("onDraw");
  if (mmain && (mmain->Flags&FUNC_VarArgs) == 0 && mmain->ReturnType.Type == TYPE_Void && mmain->NumParams == 0) {
    onDrawVC = mmain;
  }

  mmain = mklass->FindMethod("onEvent");
  if (mmain && (mmain->Flags&FUNC_VarArgs) == 0 && mmain->ReturnType.Type == TYPE_Void && mmain->NumParams == 1 &&
      mmain->ParamTypes[0].Type == TYPE_Pointer &&
      mmain->ParamTypes[0].GetPointerInnerType().Type == TYPE_Struct &&
      mmain->ParamTypes[0].GetPointerInnerType().Struct->Name == "event_t")
  {
    onEventVC = mmain;
  }
}


void VVideoMode::onDraw () {
  doRefresh = false;
  if (!hw_glctx || !onDrawVC) return;
  if ((onDrawVC->Flags&FUNC_Static) == 0) P_PASS_REF((VObject *)mainObject);
  VObject::ExecuteFunction(onDrawVC);
  doGLSwap = true;
}


void VVideoMode::onEvent (event_t &evt) {
  if ((onEventVC->Flags&FUNC_Static) == 0) P_PASS_REF((VObject *)mainObject);
  P_PASS_REF((event_t *)&evt);
  VObject::ExecuteFunction(onEventVC);
}


void VVideoMode::runEventLoop () {
  if (!mInited) return;

  initMethods();

  onDraw();

  bool doQuit = false;
  while (!doQuit && !quitSignal) {
    SDL_Event ev;
    event_t evt;

    //SDL_PumpEvents();
    bool gotEvent = SDL_PollEvent(&ev);
    if (!gotEvent) {
      if (!SDL_WaitEvent(&ev)) break;
      gotEvent = true;
    }
    if (gotEvent) {
      switch (ev.type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
          {
            int kk = sdl2TranslateKey(ev.key.keysym.sym);
            if (kk > 0) {
              evt.type = (ev.type == SDL_KEYDOWN ? ev_keydown : ev_keyup);
              evt.data1 = kk;
              evt.data2 = 0;
              evt.data3 = 0;
              onEvent(evt);
            }
          }
          break;
        case SDL_MOUSEMOTION:
          evt.type = ev_mouse;
          evt.data1 = 0;
          evt.data2 = ev.motion.xrel;
          evt.data3 = ev.motion.yrel;
          onEvent(evt);
          break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
          evt.type = (ev.button.state == SDL_PRESSED ? ev_keydown : ev_keyup);
               if (ev.button.button == SDL_BUTTON_LEFT) evt.data1 = K_MOUSE1;
          else if (ev.button.button == SDL_BUTTON_RIGHT) evt.data1 = K_MOUSE2;
          else if (ev.button.button == SDL_BUTTON_MIDDLE) evt.data1 = K_MOUSE3;
          //else if (ev.button.button == SDL_BUTTON_WHEELUP) evt.data1 = K_MWHEELUP;
          //else if (ev.button.button == SDL_BUTTON_WHEELDOWN) evt.data1 = K_MWHEELDOWN;
          else break;
          evt.data2 = 0;
          evt.data3 = 0;
          onEvent(evt);
          break;
        case SDL_MOUSEWHEEL:
          evt.type = ev_keydown;
               if (ev.wheel.y > 0) evt.data1 = K_MWHEELUP;
          else if (ev.wheel.y < 0) evt.data1 = K_MWHEELDOWN;
          else break;
          evt.data2 = 0;
          evt.data3 = 0;
          onEvent(evt);
          break;
        case SDL_WINDOWEVENT:
          switch (ev.window.event) {
            /*
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
            */
            case SDL_WINDOWEVENT_EXPOSED:
              onDraw();
              break;
          }
          break;
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
            evt.type = ev_mouse;
            evt.data1 = 0;
            evt.data2 = dx;
            evt.data3 = dy;
            GInput->PostEvent(&evt);
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

    if (doRefresh) onDraw();

    if (doGLSwap) {
      doGLSwap = false;
      SDL_GL_SwapWindow(hw_window);
    }
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

IMPLEMENT_FUNCTION(VVideoMode, clear) { VVideoMode::clear(); }

IMPLEMENT_FUNCTION(VVideoMode, requestQuit) { VVideoMode::quitSignal = true; }
IMPLEMENT_FUNCTION(VVideoMode, requestRefresh) { VVideoMode::doRefresh = true; }


#endif
