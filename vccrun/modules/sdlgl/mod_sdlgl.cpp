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
static VOpenGLTexture *txHead = nullptr, *txTail = nullptr;
static TMap<VStr, VOpenGLTexture *> txLoaded;

bool VVideo::doGLSwap = false;
bool VVideo::doRefresh = false;
bool VVideo::quitSignal = false;

extern VObject *mainObject;

#ifndef GL_CLAMP_TO_EDGE
# define GL_CLAMP_TO_EDGE  0x812F
#endif


// ////////////////////////////////////////////////////////////////////////// //
struct ScissorRect {
  int x, y, w, h;
  int enabled;
};


// ////////////////////////////////////////////////////////////////////////// //
//native static final int CreateTimer (int intervalms, optional bool oneShot);
IMPLEMENT_FUNCTION(VObject, CreateTimer) {
  P_GET_INT(specifiedOneShot);
  P_GET_BOOL(oneShot);
  P_GET_INT(intervalms);
  if (!specifiedOneShot) oneShot = false; // just in case
  RET_INT(VVideo::CreateTimerWithId(0, intervalms, oneShot));
}

//native static final bool CreateTimerWithId (int id, int intervalms, optional bool oneShot);
IMPLEMENT_FUNCTION(VObject, CreateTimerWithId) {
  P_GET_INT(specifiedOneShot);
  P_GET_BOOL(oneShot);
  P_GET_INT(intervalms);
  P_GET_INT(id);
  if (!specifiedOneShot) oneShot = false; // just in case
  RET_BOOL(VVideo::CreateTimerWithId(id, intervalms, oneShot) > 0);
}

//native static final bool DeleteTimer (int id);
IMPLEMENT_FUNCTION(VObject, DeleteTimer) {
  P_GET_INT(id);
  RET_BOOL(VVideo::DeleteTimer(id));
}

//native static final bool IsTimerExists (int id);
IMPLEMENT_FUNCTION(VObject, IsTimerExists) {
  P_GET_INT(id);
  RET_BOOL(VVideo::IsTimerExists(id));
}

//native static final bool IsTimerOneShot (int id);
IMPLEMENT_FUNCTION(VObject, IsTimerOneShot) {
  P_GET_INT(id);
  RET_BOOL(VVideo::IsTimerOneShot(id));
}

//native static final int GetTimerInterval (int id);
IMPLEMENT_FUNCTION(VObject, GetTimerInterval) {
  P_GET_INT(id);
  RET_INT(VVideo::GetTimerInterval(id));
}

//native static final bool SetTimerInterval (int id, int intervalms);
IMPLEMENT_FUNCTION(VObject, SetTimerInterval) {
  P_GET_INT(intervalms);
  P_GET_INT(id);
  RET_BOOL(VVideo::SetTimerInterval(id, intervalms));
}


//native static final float GetTickCount ();
IMPLEMENT_FUNCTION(VObject, GetTickCount) {
  RET_FLOAT((float)fsysCurrTick());
}


// ////////////////////////////////////////////////////////////////////////// //
// keys and buttons
enum {
  K_SPACE = 32,

  K_N0 = 48,
  K_N1,
  K_N2,
  K_N3,
  K_N4,
  K_N5,
  K_N6,
  K_N7,
  K_N8,
  K_N9,

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
  // extended events for vcc_run
  ev_winfocus, // data1: focused
  ev_timer, // data1: timer id
  ev_closequery, // data1: !=0 -- system shutdown
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
static bool texUpload (VOpenGLTexture *tx) {
  if (!tx) return false;
  if (!tx->img) { tx->tid = 0; return false; }
  if (tx->tid) return true;

  tx->tid = 0;
  glGenTextures(1, &tx->tid);
  if (tx->tid == 0) return false;

  //fprintf(stderr, "uploading texture '%s'\n", *tx->getPath());

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


void unloadAllTextures () {
  if (!hw_glctx) return;
  glBindTexture(GL_TEXTURE_2D, 0);
  for (VOpenGLTexture *tx = txHead; tx; tx = tx->next) {
    if (tx->tid) { glDeleteTextures(1, &tx->tid); tx->tid = 0; }
  }
}


void uploadAllTextures () {
  if (!hw_glctx) return;
  for (VOpenGLTexture *tx = txHead; tx; tx = tx->next) texUpload(tx);
}


// ////////////////////////////////////////////////////////////////////////// //
VOpenGLTexture::VOpenGLTexture (VImage *aimg, const VStr &apath)
  : rc(1)
  , mPath(apath)
  , img(aimg)
  , tid(0)
  , mTransparent(false)
  , mOpaque(false)
  , mOneBitAlpha(false)
  , prev(nullptr)
  , next(nullptr)
{
  analyzeImage();
  if (hw_glctx) texUpload(this);
  registerMe();
}

VOpenGLTexture::~VOpenGLTexture () {
  if (hw_glctx && tid) {
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &tid);
  }
  tid = 0;
  delete img;
  img = nullptr;
  if (mPath.length()) txLoaded.remove(mPath);
  mPath = VStr();
  if (!prev && !next) {
    if (txHead == this) { txHead = txTail = nullptr; }
  } else {
    if (prev) prev->next = next; else txHead = next;
    if (next) next->prev = prev; else txTail = prev;
  }
  prev = next = nullptr;
}


void VOpenGLTexture::registerMe () {
  if (prev || next) return;
  if (txHead == this) return;
  prev = txTail;
  if (txTail) txTail->next = this; else txHead = this;
  txTail = this;
}


void VOpenGLTexture::analyzeImage () {
  if (img) {
    mTransparent = true;
    mOpaque = true;
    mOneBitAlpha = true;
    for (int y = 0; y < img->height; ++y) {
      for (int x = 0; x < img->width; ++x) {
        VImage::RGBA pix = img->getPixel(x, y);
        if (pix.a != 0 && pix.a != 255) {
          mOneBitAlpha = false;
          mTransparent = false;
          mOpaque = false;
          break; // no need to analyze more
        } else {
               if (pix.a != 0) mTransparent = false;
          else if (pix.a != 255) mOpaque = false;
        }
      }
    }
  } else {
    mTransparent = false;
    mOpaque = false;
    mOneBitAlpha = false;
  }
}


void VOpenGLTexture::addRef () {
  ++rc;
}


void VOpenGLTexture::release () {
  if (--rc == 0) delete this;
}


VOpenGLTexture *VOpenGLTexture::Load (const VStr &fname) {
  VStr rname = fsysFileFindAnyExt(fname);
  if (rname.length() == 0) return nullptr;
  VOpenGLTexture **loaded = txLoaded.find(fname);
  if (loaded) {
    (*loaded)->addRef();
    return *loaded;
  }
  VStream *st = fsysOpenFile(rname);
  if (!st) return nullptr;
  VImage *img = VImage::loadFrom(st);
  delete st;
  if (!img) return nullptr;
  VOpenGLTexture *res = new VOpenGLTexture(img, rname);
  txLoaded.put(rname, res);
  //fprintf(stderr, "TXLOADED: '%s' rc=%d, (%p)\n", *res->mPath, res->rc, res);
  return res;
}


void VOpenGLTexture::blitExt (int dx0, int dy0, int dx1, int dy1, int x0, int y0, int x1, int y1) const {
  if (!tid || VVideo::isFullyTransparent() || mTransparent) return;
  if (x1 < 0) x1 = img->width;
  if (y1 < 0) y1 = img->height;
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, tid);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  if (VVideo::getBlendMode() == VVideo::BlendNormal) {
    if (mOpaque && VVideo::isFullyOpaque()) {
      glDisable(GL_BLEND);
    } else {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
  } else {
    VVideo::setupBlending();
  }
  const float fx0 = (float)x0/(float)img->width;
  const float fx1 = (float)x1/(float)img->width;
  const float fy0 = (float)y0/(float)img->height;
  const float fy1 = (float)y1/(float)img->height;
  glBegin(GL_QUADS);
    glTexCoord2f(fx0, fy0); glVertex2f(dx0, dy0);
    glTexCoord2f(fx1, fy0); glVertex2f(dx1, dy0);
    glTexCoord2f(fx1, fy1); glVertex2f(dx1, dy1);
    glTexCoord2f(fx0, fy1); glVertex2f(dx0, dy1);
  glEnd();
}


void VOpenGLTexture::blitExtRep (int dx0, int dy0, int dx1, int dy1, int x0, int y0, int x1, int y1) const {
  if (!tid || VVideo::isFullyTransparent() || mTransparent) return;
  if (x1 < 0) x1 = img->width;
  if (y1 < 0) y1 = img->height;
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, tid);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  if (VVideo::getBlendMode() == VVideo::BlendNormal) {
    if (mOpaque && VVideo::isFullyOpaque()) {
      glDisable(GL_BLEND);
    } else {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
  } else {
    VVideo::setupBlending();
  }
  glBegin(GL_QUADS);
    glTexCoord2i(x0, y0); glVertex2f(dx0, dy0);
    glTexCoord2i(x1, y0); glVertex2f(dx1, dy0);
    glTexCoord2i(x1, y1); glVertex2f(dx1, dy1);
    glTexCoord2i(x0, y1); glVertex2f(dx0, dy1);
  glEnd();
}


void VOpenGLTexture::blitAt (int dx0, int dy0, float scale) const {
  if (!tid || VVideo::isFullyTransparent() || scale <= 0 || mTransparent) return;
  int w = img->width;
  int h = img->height;
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, tid);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  if (VVideo::getBlendMode() == VVideo::BlendNormal) {
    if (mOpaque && VVideo::isFullyOpaque()) {
      glDisable(GL_BLEND);
    } else {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
  } else {
    VVideo::setupBlending();
  }
  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(dx0, dy0);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(dx0+w*scale, dy0);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(dx0+w*scale, dy0+h*scale);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(dx0, dy0+h*scale);
  glEnd();
}


// ////////////////////////////////////////////////////////////////////////// //
static TMap<int, VGLTexture *> vcGLTexMap;
//FIXME: rewrite id management
static TArray<int> vcGLFreeIds;
static int vcGLFreeIdsUsed = 0;
static int vcGLLastUsedId = 0;


static int vcGLAllocId (VGLTexture *obj) {
  int res;
  if (vcGLFreeIdsUsed > 0) {
    res = vcGLFreeIds[--vcGLFreeIdsUsed];
  } else {
    // no free ids
    res = ++vcGLLastUsedId;
  }
  vcGLTexMap.put(res, obj);
  return res;
}


static void vcGLFreeId (int id) {
  if (id < 1 || id > vcGLLastUsedId) return;
  vcGLTexMap.remove(id);
  if (vcGLFreeIdsUsed == vcGLFreeIds.length()) {
    vcGLFreeIds.append(id);
    ++vcGLFreeIdsUsed;
  } else {
    vcGLFreeIds[vcGLFreeIdsUsed++] = id;
  }
}


IMPLEMENT_CLASS(V, GLTexture);


void VGLTexture::Destroy () {
  vcGLFreeId(id);
  //fprintf(stderr, "destroying texture object %p\n", this);
  if (tex) {
    //fprintf(stderr, "  releasing texture '%s'... rc=%d, (%p)\n", *tex->getPath(), tex->getRC(), tex);
    tex->release();
    tex = nullptr;
  }
  Super::Destroy();
}


IMPLEMENT_FUNCTION(VGLTexture, Destroy) {
  P_GET_SELF;
  //if (Self) Self->SetFlags(_OF_DelayedDestroy);
  if (Self) Self->ConditionalDestroy();
  //delete Self;
}


//native final static GLTexture Load (string fname);
IMPLEMENT_FUNCTION(VGLTexture, Load) {
  P_GET_STR(fname);
  VOpenGLTexture *tex = VOpenGLTexture::Load(fname);
  if (tex) {
    VGLTexture *ifile = Spawn<VGLTexture>();
    ifile->tex = tex;
    ifile->id = vcGLAllocId(ifile);
    //fprintf(stderr, "created texture object %p (%p)\n", ifile, ifile->tex);
    RET_REF((VObject *)ifile);
    return;
  }
  RET_REF(nullptr);
}


// native final static GLTexture GetById (int id);
IMPLEMENT_FUNCTION(VGLTexture, GetById) {
  P_GET_INT(id);
  if (id > 0 && id <= vcGLLastUsedId) {
    auto opp = vcGLTexMap.find(id);
    if (opp) RET_REF((VGLTexture *)(*opp)); else RET_REF(nullptr);
  } else {
    RET_REF(nullptr);
  }
}


IMPLEMENT_FUNCTION(VGLTexture, width) {
  P_GET_SELF;
  RET_INT(Self && Self->tex ? Self->tex->width : 0);
}

IMPLEMENT_FUNCTION(VGLTexture, height) {
  P_GET_SELF;
  RET_INT(Self && Self->tex ? Self->tex->height : 0);
}

IMPLEMENT_FUNCTION(VGLTexture, isTransparent) {
  P_GET_SELF;
  RET_BOOL(Self && Self->tex ? Self->tex->isTransparent : true);
}

IMPLEMENT_FUNCTION(VGLTexture, isOpaque) {
  P_GET_SELF;
  RET_BOOL(Self && Self->tex ? Self->tex->isOpaque : false);
}

IMPLEMENT_FUNCTION(VGLTexture, isOneBitAlpha) {
  P_GET_SELF;
  RET_BOOL(Self && Self->tex ? Self->tex->isOneBitAlpha : true);
}


// void blitExt (int dx0, int dy0, int dx1, int dy1, int x0, int y0, optional int x1, optional int y1);
IMPLEMENT_FUNCTION(VGLTexture, blitExt) {
  P_GET_INT(specifiedY1);
  P_GET_INT(y1);
  P_GET_INT(specifiedX1);
  P_GET_INT(x1);
  P_GET_INT(y0);
  P_GET_INT(x0);
  P_GET_INT(dy1);
  P_GET_INT(dx1);
  P_GET_INT(dy0);
  P_GET_INT(dx0);
  P_GET_SELF;
  if (!specifiedX1) x1 = -1;
  if (!specifiedY1) y1 = -1;
  if (Self && Self->tex) Self->tex->blitExt(dx0, dy0, dx1, dy1, x0, y0, x1, y1);
}


// void blitExtRep (int dx0, int dy0, int dx1, int dy1, int x0, int y0, optional int x1, optional int y1);
IMPLEMENT_FUNCTION(VGLTexture, blitExtRep) {
  P_GET_INT(specifiedY1);
  P_GET_INT(y1);
  P_GET_INT(specifiedX1);
  P_GET_INT(x1);
  P_GET_INT(y0);
  P_GET_INT(x0);
  P_GET_INT(dy1);
  P_GET_INT(dx1);
  P_GET_INT(dy0);
  P_GET_INT(dx0);
  P_GET_SELF;
  if (!specifiedX1) x1 = -1;
  if (!specifiedY1) y1 = -1;
  if (Self && Self->tex) Self->tex->blitExtRep(dx0, dy0, dx1, dy1, x0, y0, x1, y1);
}


// void blitAt (int dx0, int dy0, optional float scale);
IMPLEMENT_FUNCTION(VGLTexture, blitAt) {
  P_GET_INT(specifiedScale);
  P_GET_FLOAT(scale);
  P_GET_INT(dy0);
  P_GET_INT(dx0);
  P_GET_SELF;
  if (!specifiedScale) scale = 1;
  if (Self && Self->tex) Self->tex->blitAt(dx0, dy0, scale);
}


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_CLASS(V, Video);

bool VVideo::mInited = false;
int VVideo::mWidth = 0;
int VVideo::mHeight = 0;
bool VVideo::smoothLine = false;
bool VVideo::directMode = false;


struct TimerInfo {
  SDL_TimerID sdlid;
  int id; // script id (0: not used)
  int interval;
  bool oneShot;
};

static TMap<int, TimerInfo> timerMap; // key: timer id; value: timer info
static int timerLastUsedId = 0;


// doesn't insert anything in `timerMap`!
static int timerAllocId () {
  int res = 0;
  if (timerLastUsedId == 0) timerLastUsedId = 1;
  if (timerLastUsedId < 0x7ffffff && timerMap.has(timerLastUsedId)) ++timerLastUsedId;
  if (timerLastUsedId < 0x7ffffff) {
    res = timerLastUsedId++;
  } else {
    for (int f = 1; f < timerLastUsedId; ++f) if (!timerMap.has(f)) { res = f; break; }
  }
  return res;
}


// removes element from `timerMap`!
static void timerFreeId (int id) {
  if (id <= 0) return;
  TimerInfo *ti = timerMap.get(id);
  if (ti) {
    SDL_RemoveTimer(ti->sdlid);
    timerMap.del(id);
  }
  if (id == timerLastUsedId) {
    while (timerLastUsedId > 0 && !timerMap.has(timerLastUsedId)) --timerLastUsedId;
  }
}


static Uint32 sdlTimerCallback (Uint32 interval, void *param) {
  SDL_Event event;
  SDL_UserEvent userevent;

  int id = (int)param;

  TimerInfo *ti = timerMap.get(id);
  if (ti) {
    //fprintf(stderr, "timer cb: id=%d\n", id);
    userevent.type = SDL_USEREVENT;
    userevent.code = 1;
    userevent.data1 = (void *)ti->id;

    event.type = SDL_USEREVENT;
    event.user = userevent;

    SDL_PushEvent(&event);
    // don't delete timer here, 'cause callback is running in separate thread
    return (ti->oneShot ? 0 : ti->interval);
  }

  return 0; // this timer is dead
}


// returns timer id or 0
// if id <= 0, creates new unique timer id
// if interval is < 1, returns with error and won't create timer
int VVideo::CreateTimerWithId (int id, int intervalms, bool oneShot) {
  if (intervalms < 1) return 0;
  if (id <= 0) {
    // get new id
    id = timerAllocId();
  } else {
    if (timerMap.has(id)) return 0;
  }
  //fprintf(stderr, "id=%d; interval=%d; one=%d\n", id, intervalms, (int)oneShot);
  TimerInfo ti;
  ti.sdlid = SDL_AddTimer(intervalms, &sdlTimerCallback, (void *)id);
  if (ti.sdlid == 0) {
    fprintf(stderr, "CANNOT create timer: id=%d; interval=%d; one=%d\n", id, intervalms, (int)oneShot);
    timerFreeId(id);
    return 0;
  }
  ti.id = id;
  ti.interval = intervalms;
  ti.oneShot = oneShot;
  timerMap.put(id, ti);
  return id;
}


// `true`: deleted, `false`: no such timer
bool VVideo::DeleteTimer (int id) {
  if (id <= 0 || !timerMap.has(id)) return false;
  timerFreeId(id);
  return true;
}


bool VVideo::IsTimerExists (int id) {
  return (id > 0 && timerMap.has(id));
}


bool VVideo::IsTimerOneShot (int id) {
  TimerInfo *ti = timerMap.get(id);
  return (ti && ti->oneShot);
}


// 0: no such timer
int VVideo::GetTimerInterval (int id) {
  TimerInfo *ti = timerMap.get(id);
  return (ti ? ti->interval : 0);
}


// returns success flag; won't do anything if interval is < 1
bool VVideo::SetTimerInterval (int id, int intervalms) {
  if (intervalms < 1) return false;
  TimerInfo *ti = timerMap.get(id);
  if (ti) {
    ti->interval = intervalms;
    return true;
  }
  return false;
}


// ////////////////////////////////////////////////////////////////////////// //
bool VVideo::canInit () {
  return true;
}


bool VVideo::hasOpenGL () {
  return true;
}


bool VVideo::isInitialized () { return mInited; }
int VVideo::getWidth () { return mWidth; }
int VVideo::getHeight () { return mHeight; }


// ////////////////////////////////////////////////////////////////////////// //
void VVideo::close () {
  if (mInited) {
    if (hw_glctx) {
      if (hw_window) {
        SDL_GL_MakeCurrent(hw_window, hw_glctx);
        unloadAllTextures();
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
    directMode = false;
  }
}


bool VVideo::open (const VStr &winname, int width, int height) {
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

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  //glDisable(GL_BLEND);
  //glEnable(GL_LINE_SMOOTH);
  if (smoothLine) glEnable(GL_LINE_SMOOTH); else glDisable(GL_LINE_SMOOTH);

  glDisable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
  realiseGLColor(); // this will setup blending
  //glEnable(GL_BLEND);
  //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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


void VVideo::clear () {
  if (!mInited) return;

  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClearDepth(1.0);
  glClearStencil(0);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
}


// ////////////////////////////////////////////////////////////////////////// //
VMethod *VVideo::onDrawVC = nullptr;
VMethod *VVideo::onEventVC = nullptr;
VMethod *VVideo::onNewFrameVC = nullptr;


void VVideo::initMethods () {
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
      ((mmain->ParamTypes[0].Type == TYPE_Pointer &&
        mmain->ParamFlags[0] == 0 &&
        mmain->ParamTypes[0].GetPointerInnerType().Type == TYPE_Struct &&
        mmain->ParamTypes[0].GetPointerInnerType().Struct->Name == "event_t") ||
       ((mmain->ParamFlags[0]&(FPARM_Out|FPARM_Ref)) != 0 &&
        mmain->ParamTypes[0].Type == TYPE_Struct &&
        mmain->ParamTypes[0].Struct->Name == "event_t")))
  {
    //fprintf(stderr, "onevent found\n");
    onEventVC = mmain;
  } else {
    //fprintf(stderr, ":: (%d) %s\n", mmain->ParamFlags[0], *mmain->ParamTypes[0].GetName());
    //abort();
  }

  mmain = mklass->FindMethod("onNewFrame");
  if (mmain && (mmain->Flags&FUNC_VarArgs) == 0 && mmain->ReturnType.Type == TYPE_Void && mmain->NumParams == 0) {
    onNewFrameVC = mmain;
  }
}


void VVideo::onDraw () {
  doRefresh = false;
  if (!hw_glctx || !onDrawVC) return;
  if ((onDrawVC->Flags&FUNC_Static) == 0) P_PASS_REF((VObject *)mainObject);
  VObject::ExecuteFunction(onDrawVC);
  doGLSwap = true;
}


void VVideo::onEvent (event_t &evt) {
  if (!hw_glctx || !onEventVC) return;
  if ((onEventVC->Flags&FUNC_Static) == 0) P_PASS_REF((VObject *)mainObject);
  P_PASS_REF((event_t *)&evt);
  VObject::ExecuteFunction(onEventVC);
}


void VVideo::onNewFrame () {
  if (!hw_glctx || !onNewFrameVC) return;
  if ((onNewFrameVC->Flags&FUNC_Static) == 0) P_PASS_REF((VObject *)mainObject);
  VObject::ExecuteFunction(onNewFrameVC);
}


// ////////////////////////////////////////////////////////////////////////// //
int VVideo::currFrameTime = 0;
int VVideo::prevFrameTime = 0;


int VVideo::getFrameTime () {
  return currFrameTime;
}


void VVideo::setFrameTime (int newft) {
  if (newft < 0) newft = 0;
  if (currFrameTime == newft) return;
  prevFrameTime = 0;
  currFrameTime = newft;
}


bool VVideo::doFrameBusiness (SDL_Event &ev) {
  if (currFrameTime <= 0) {
    SDL_WaitEvent(&ev);
    return true;
  }

  int cticks = SDL_GetTicks();
  if (cticks < 0) { fprintf(stderr, "Tick overflow!"); abort(); }

  if (prevFrameTime == 0) {
    prevFrameTime = cticks;
    onNewFrame();
    cticks = SDL_GetTicks();
  }

  bool gotEvent = false;
  if (prevFrameTime+currFrameTime > cticks) {
    if (SDL_WaitEventTimeout(&ev, prevFrameTime+currFrameTime-cticks)) gotEvent = true;
    cticks = SDL_GetTicks();
  }

  //fprintf(stderr, "pt=%d; nt=%d; ct=%d\n", prevFrameTime, prevFrameTime+currFrameTime, cticks);
  while (prevFrameTime+currFrameTime <= cticks) {
    prevFrameTime += currFrameTime;
    onNewFrame();
  }

  return (gotEvent || SDL_PollEvent(&ev));
}


// ////////////////////////////////////////////////////////////////////////// //
void VVideo::runEventLoop () {
  if (!mInited) return;

  initMethods();

  onDraw();

  bool doQuit = false;
  while (!doQuit && !quitSignal) {
    SDL_Event ev;
    event_t evt;

    SDL_PumpEvents();
    bool gotEvent = doFrameBusiness(ev);

  morevents:
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
          //evt.data2 = ev.motion.xrel;
          //evt.data3 = ev.motion.yrel;
          evt.data2 = ev.motion.x;
          evt.data3 = ev.motion.y;
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
          evt.data2 = ev.button.x;
          evt.data3 = ev.button.y;
          onEvent(evt);
          break;
        case SDL_MOUSEWHEEL:
          evt.type = ev_keydown;
               if (ev.wheel.y > 0) evt.data1 = K_MWHEELUP;
          else if (ev.wheel.y < 0) evt.data1 = K_MWHEELDOWN;
          else break;
          {
            int mx, my;
            //SDL_GetGlobalMouseState(&mx, &my);
            SDL_GetMouseState(&mx, &my);
            evt.data2 = mx;
            evt.data3 = my;
            onEvent(evt);
          }
          break;
        case SDL_WINDOWEVENT:
          switch (ev.window.event) {
            case SDL_WINDOWEVENT_FOCUS_GAINED:
              evt.type = ev_winfocus;
              evt.data1 = 1;
              evt.data2 = 0;
              evt.data3 = 0;
              onEvent(evt);
              break;
            case SDL_WINDOWEVENT_FOCUS_LOST:
              evt.type = ev_winfocus;
              evt.data1 = 0;
              evt.data2 = 0;
              evt.data3 = 0;
              onEvent(evt);
              break;
            case SDL_WINDOWEVENT_EXPOSED:
              onDraw();
              break;
          }
          break;
        case SDL_QUIT:
          //doQuit = true;
          evt.type = ev_closequery;
          evt.data1 = 0; // alas, there is no way to tell why we're quiting; fuck you, sdl!
          evt.data2 = 0;
          evt.data3 = 0;
          onEvent(evt);
          break;
        case SDL_USEREVENT:
          //fprintf(stderr, "SDL: userevent, code=%d\n", ev.user.code);
          if (ev.user.code == 1) {
            TimerInfo *ti = timerMap.get((int)ev.user.data1);
            if (ti) {
              int id = ti->id; // save id
              // remove one-shot timer
              if (ti->oneShot) timerFreeId(ti->id);
              evt.type = ev_timer;
              evt.data1 = id;
              evt.data2 = 0;
              evt.data3 = 0;
              onEvent(evt);
            }
          }
          break;
        default:
          break;
      }
      // if we have no fixed frame time, process more events
      if (currFrameTime <= 0 && SDL_PollEvent(&ev)) goto morevents;
    }

    if (doRefresh) onDraw();

    if (doGLSwap) {
      static double lastCollect = 0.0;
      doGLSwap = false;
      SDL_GL_SwapWindow(hw_window);

      double currTick = fsysCurrTick();
      if (currTick-lastCollect >= 3) {
        lastCollect = currTick;
        VObject::CollectGarbage(); // why not?
        //fprintf(stderr, "objc=%d\n", VObject::GetObjectsCount());
      }
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
vuint32 VVideo::colorARGB = 0xffffff;
int VVideo::mBlendMode = VVideo::BlendNormal;
VFont *VVideo::currFont = nullptr;


void VVideo::setFont (VName fontname) {
  if (currFont && currFont->getName() == fontname) return;
  currFont = VFont::findFont(fontname);
}


void VVideo::drawTextAt (int x, int y, const VStr &text) {
  if (!currFont || isFullyTransparent() || text.isEmpty()) return;
  if (!mInited) return;

  const VOpenGLTexture *tex = currFont->getTexture();
  if (!tex || !tex->tid) return; // oops

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, tex->tid);
  glEnable(GL_BLEND); // font is rarely fully opaque, so don't bother checking
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glBegin(GL_QUADS);
  int sx = x;
  for (int f = 0; f < text.length(); ++f) {
    int ch = (vuint8)text[f];
    if (ch == '\r') { x = sx; continue; }
    if (ch == '\n') { x = sx; y += currFont->getHeight(); continue; }
    auto fc = currFont->getChar(ch);
    if (!fc) continue;
    // draw char
    glTexCoord2f(fc->tx0, fc->ty0); glVertex2f(x, y+fc->topofs);
    glTexCoord2f(fc->tx1, fc->ty0); glVertex2f(x+fc->width, y+fc->topofs);
    glTexCoord2f(fc->tx1, fc->ty1); glVertex2f(x+fc->width, y+fc->topofs+fc->height);
    glTexCoord2f(fc->tx0, fc->ty1); glVertex2f(x, y+fc->topofs+fc->height);
    // advance
    x += fc->advance;
  }
  glEnd();
}


// ////////////////////////////////////////////////////////////////////////// //
void VVideo::sendPing () {
  if (!mInited) return;

  SDL_Event event;
  SDL_UserEvent userevent;

  userevent.type = SDL_USEREVENT;
  userevent.code = 0;

  event.type = SDL_USEREVENT;
  event.user = userevent;

  SDL_PushEvent(&event);
}


IMPLEMENT_FUNCTION(VVideo, canInit) { RET_BOOL(VVideo::canInit()); }
IMPLEMENT_FUNCTION(VVideo, hasOpenGL) { RET_BOOL(VVideo::hasOpenGL()); }
IMPLEMENT_FUNCTION(VVideo, isInitialized) { RET_BOOL(VVideo::isInitialized()); }
IMPLEMENT_FUNCTION(VVideo, screenWidth) { RET_INT(VVideo::getWidth()); }
IMPLEMENT_FUNCTION(VVideo, screenHeight) { RET_INT(VVideo::getHeight()); }

IMPLEMENT_FUNCTION(VVideo, get_frameTime) { RET_BOOL(VVideo::getFrameTime()); }
IMPLEMENT_FUNCTION(VVideo, set_frameTime) { P_GET_INT(newft); VVideo::setFrameTime(newft); VVideo::sendPing(); }

IMPLEMENT_FUNCTION(VVideo, openScreen) {
  P_GET_INT(hgt);
  P_GET_INT(wdt);
  P_GET_STR(wname);
  RET_BOOL(VVideo::open(wname, wdt, hgt));
}

IMPLEMENT_FUNCTION(VVideo, closeScreen) {
  VVideo::close();
  VVideo::sendPing();
}


IMPLEMENT_FUNCTION(VVideo, runEventLoop) { VVideo::runEventLoop(); }

IMPLEMENT_FUNCTION(VVideo, clearScreen) { VVideo::clear(); }


//native final static void setScale (float sx, float sy);
IMPLEMENT_FUNCTION(VVideo, setScale) {
  P_GET_FLOAT(sy);
  P_GET_FLOAT(sx);
  if (mInited) {
    glLoadIdentity();
    glScalef(sx, sy, 1);
  }
}


// aborts if font cannot be loaded
//native final static void loadFont (name fname, string fnameIni, string fnameTexture);
IMPLEMENT_FUNCTION(VVideo, loadFont) {
  /*
  P_GET_STR(fnameTexture);
  P_GET_STR(fnameIni);
  P_GET_NAME(fname);
  */
  VName fname;
  VStr fnameIni;
  VStr fnameTexture;
  vobjGetParam(fname, fnameIni, fnameTexture);
  //fprintf(stderr, "fname=<%s>; ini=<%s>; tx=<%s>\n", *fname, *fnameIni, *fnameTexture);
  if (VFont::findFont(fname)) return;
  new VFont(fname, fnameIni, fnameTexture);
}


IMPLEMENT_FUNCTION(VVideo, requestQuit) {
  if (!VVideo::quitSignal) {
    VVideo::quitSignal = true;
    VVideo::sendPing();
  }
}

IMPLEMENT_FUNCTION(VVideo, resetQuitRequest) {
  VVideo::quitSignal = false;
}

IMPLEMENT_FUNCTION(VVideo, requestRefresh) {
  if (!VVideo::doRefresh) {
    VVideo::doRefresh = true;
    if (VVideo::getFrameTime() <= 0) VVideo::sendPing();
  }
}

IMPLEMENT_FUNCTION(VVideo, forceSwap) {
  if (!mInited) return;
  doGLSwap = false;
  SDL_GL_SwapWindow(hw_window);
}

IMPLEMENT_FUNCTION(VVideo, get_directMode) {
  RET_BOOL(directMode);
}

IMPLEMENT_FUNCTION(VVideo, set_directMode) {
  P_GET_BOOL(m);
  if (!mInited) return;
  if (m != directMode) {
    directMode = m;
    glDrawBuffer(m ? GL_FRONT : GL_BACK);
  }
}

IMPLEMENT_FUNCTION(VVideo, get_scissorEnabled) {
  if (mInited) {
    RET_BOOL((glIsEnabled(GL_SCISSOR_TEST) ? 1 : 0));
  } else {
    RET_BOOL(0);
  }
}

IMPLEMENT_FUNCTION(VVideo, set_scissorEnabled) {
  P_GET_BOOL(v);
  if (mInited) {
    if (v) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
  }
}

IMPLEMENT_FUNCTION(VVideo, copyScissor) {
  P_GET_PTR(ScissorRect, s);
  P_GET_PTR(ScissorRect, d);
  if (d) {
    if (s) {
      *d = *s;
    } else {
      d->x = d->y = 0;
      d->w = mWidth;
      d->h = mHeight;
      d->enabled = 0;
    }
  }
}

IMPLEMENT_FUNCTION(VVideo, getScissor) {
  P_GET_PTR(ScissorRect, sr);
  if (sr) {
    if (!mInited) { sr->x = sr->y = sr->w = sr->h = sr->enabled = 0; return; }
    sr->enabled = (glIsEnabled(GL_SCISSOR_TEST) ? 1 : 0);
    GLint scxywh[4];
    glGetIntegerv(GL_SCISSOR_BOX, scxywh);
    int y0 = mHeight-(scxywh[1]+scxywh[3]);
    int y1 = mHeight-scxywh[1]-1;
    sr->x = scxywh[0];
    sr->y = y0;
    sr->w = scxywh[2];
    sr->h = y1-y0+1;
  }
}

IMPLEMENT_FUNCTION(VVideo, setScissor) {
  P_GET_PTR(ScissorRect, sr);
  if (sr) {
    if (!mInited) return;
    if (sr->enabled) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    //glScissor(sr->x0, mHeight-sr->y0-1, sr->x1, mHeight-sr->y1-1);
    int w = (sr->w > 0 ? sr->w : 0);
    int h = (sr->h > 0 ? sr->h : 0);
    int y1 = mHeight-sr->y-1;
    int y0 = mHeight-(sr->y+h);
    glScissor(sr->x, y0, w, y1-y0+1);
  } else {
    if (mInited) {
      glDisable(GL_SCISSOR_TEST);
      GLint scxywh[4];
      glGetIntegerv(GL_VIEWPORT, scxywh);
      glScissor(scxywh[0], scxywh[1], scxywh[2], scxywh[3]);
    }
  }
}


IMPLEMENT_FUNCTION(VVideo, get_smoothLine) {
  RET_BOOL(smoothLine);
}


IMPLEMENT_FUNCTION(VVideo, set_smoothLine) {
  P_GET_BOOL(v);
  if (smoothLine != v) {
    smoothLine = v;
    if (mInited) {
      if (v) glEnable(GL_LINE_SMOOTH); else glDisable(GL_LINE_SMOOTH);
    }
  }
}


//native final static int getColorARGB ();
IMPLEMENT_FUNCTION(VVideo, get_colorARGB) {
  RET_INT(colorARGB);
}

//native final static void setColorARGB (int v);
IMPLEMENT_FUNCTION(VVideo, set_colorARGB) {
  P_GET_INT(c);
  setColor((vuint32)c);
}

//native final static int getBlendMode ();
IMPLEMENT_FUNCTION(VVideo, get_blendMode) {
  RET_INT(getBlendMode());
}

//native final static void set_blendMode (int v);
IMPLEMENT_FUNCTION(VVideo, set_blendMode) {
  P_GET_INT(c);
  setBlendMode(c);
}

//native final static void setFont (name fontname);
IMPLEMENT_FUNCTION(VVideo, set_fontName) {
  P_GET_NAME(fontname);
  setFont(fontname);
}

//native final static name getFont ();
IMPLEMENT_FUNCTION(VVideo, get_fontName) {
  if (!currFont) {
    RET_NAME(NAME_None);
  } else {
    RET_NAME(currFont->getName());
  }
}

//native final static void fontHeight ();
IMPLEMENT_FUNCTION(VVideo, fontHeight) {
  RET_INT(currFont ? currFont->getHeight() : 0);
}

//native final static int spaceWidth ();
IMPLEMENT_FUNCTION(VVideo, spaceWidth) {
  RET_INT(currFont ? currFont->getSpaceWidth() : 0);
}

//native final static int charWidth (int ch);
IMPLEMENT_FUNCTION(VVideo, charWidth) {
  P_GET_INT(ch);
  RET_INT(currFont ? currFont->charWidth(ch) : 0);
}

//native final static int textWidth (string text);
IMPLEMENT_FUNCTION(VVideo, textWidth) {
  P_GET_STR(text);
  RET_INT(currFont ? currFont->textWidth(text) : 0);
}

//native final static int textHeight (string text);
IMPLEMENT_FUNCTION(VVideo, textHeight) {
  P_GET_STR(text);
  RET_INT(currFont ? currFont->textHeight(text) : 0);
}

//native final static void drawText (int x, int y, string text);
IMPLEMENT_FUNCTION(VVideo, drawTextAt) {
  P_GET_STR(text);
  P_GET_INT(y);
  P_GET_INT(x);
  drawTextAt(x, y, text);
}


//native final static void drawLine (int x0, int y0, int x1, int y1);
IMPLEMENT_FUNCTION(VVideo, drawLine) {
  P_GET_INT(y1);
  P_GET_INT(x1);
  P_GET_INT(y0);
  P_GET_INT(x0);
  if (!mInited || isFullyTransparent()) return;
  setupBlending();
  glDisable(GL_TEXTURE_2D);
  glBegin(GL_LINES);
    glVertex2f(x0+0.5f, y0+0.5f);
    glVertex2f(x1+0.5f, y1+0.5f);
  glEnd();
}


//native final static void drawRect (int x0, int y0, int w, int h);
IMPLEMENT_FUNCTION(VVideo, drawRect) {
  P_GET_INT(h);
  P_GET_INT(w);
  P_GET_INT(y0);
  P_GET_INT(x0);
  if (!mInited || isFullyTransparent() || w < 1 || h < 1) return;
  setupBlending();
  glDisable(GL_TEXTURE_2D);
  glBegin(GL_LINE_LOOP);
    glVertex2f(x0+0+0.5f, y0+0+0.5f);
    glVertex2f(x0+w-0.5f, y0+0+0.5f);
    glVertex2f(x0+w-0.5f, y0+h-0.5f);
    glVertex2f(x0+0+0.5f, y0+h-0.5f);
  glEnd();
}


//native final static void fillRect (int x0, int y0, int w, int h);
IMPLEMENT_FUNCTION(VVideo, fillRect) {
  P_GET_INT(h);
  P_GET_INT(w);
  P_GET_INT(y0);
  P_GET_INT(x0);
  if (!mInited || isFullyTransparent() || w < 1 || h < 1) return;
  setupBlending();
  glDisable(GL_TEXTURE_2D);

  // no need for 0.5f here, or rect will be offset
  glBegin(GL_QUADS);
    glVertex2f(x0+0, y0+0);
    glVertex2f(x0+w, y0+0);
    glVertex2f(x0+w, y0+h);
    glVertex2f(x0+0, y0+h);
  glEnd();
}


// ////////////////////////////////////////////////////////////////////////// //
static VStr readLine (VStream *strm, bool allTrim=true) {
  if (!strm || strm->IsError()) return VStr();
  VStr res;
  while (!strm->AtEnd()) {
    char ch;
    strm->Serialize(&ch, 1);
    if (strm->IsError()) return VStr();
    if (ch == '\r') {
      if (!strm->AtEnd()) {
        strm->Serialize(&ch, 1);
        if (strm->IsError()) return VStr();
        if (ch != '\n') strm->Seek(strm->Tell()-1);
      }
      break;
    }
    if (ch == '\n') break;
    if (ch == 0) ch = ' ';
    res += ch;
  }
  if (allTrim) {
    while (!res.isEmpty() && (vuint8)res[0] <= ' ') res.chopLeft(1);
    while (!res.isEmpty() && (vuint8)res[res.length()-1] <= ' ') res.chopRight(1);
  }
  return res;
}


static VStr getKey (const VStr &s) {
  int epos = s.indexOf('=');
  if (epos < 0) return s;
  VStr res = s.left(epos);
  while (!res.isEmpty() && (vuint8)res[res.length()-1] <= ' ') res.chopRight(1);
  return res;
}


static VStr getValue (const VStr &s) {
  int epos = s.indexOf('=');
  if (epos < 0) return VStr();
  VStr res = s;
  res.chopLeft(epos+1);
  while (!res.isEmpty() && (vuint8)res[0] <= ' ') res.chopLeft(1);
  while (!res.isEmpty() && (vuint8)res[res.length()-1] <= ' ') res.chopRight(1);
  return res;
}


static int getIntValue (const VStr &s) {
  VStr v = getValue(s);
  if (v.isEmpty()) return 0;
  bool neg = v.startsWith("-");
  if (neg) v.chopLeft(1);
  int res = 0;
  for (int f = 0; f < v.length(); ++f) {
    int d = VStr::digitInBase(v[f]);
    if (d < 0) break;
    res = res*10+d;
  }
  if (neg) res = -res;
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
VFont *VFont::fontList;


//==========================================================================
//
//  VFont::findFont
//
//==========================================================================
VFont *VFont::findFont (VName name) {
  for (VFont *cur = fontList; cur; cur = cur->next) if (cur->name == name) return cur;
  return nullptr;
}


//==========================================================================
//
//  VFont::VFont
//
//==========================================================================
VFont::VFont (VName aname, const VStr &fnameIni, const VStr &fnameTexture)
  : spaceWidth(0)
  , fontHeight(0)
{
  guard(VFont::VFont);

  //for (int i = 0; i < 256; ++i) chars1251[f] = -1;
  //firstChar = -1;
  //lastChar = -1;

  tex = VOpenGLTexture::Load(fnameTexture);
  if (!tex) Sys_Error(va("cannot load font '%s' (texture not found)", *aname));

  auto inif = fsysOpenFileAnyExt(fnameIni);
  if (!inif) { tex->release(); tex = nullptr; Sys_Error(va("cannot load font '%s' (description not found)", *aname)); }
  //fprintf(stderr, "*** %d %d %d %d\n", (int)inif->AtEnd(), (int)inif->IsError(), inif->TotalSize(), inif->Tell());

  VStr currSection;

  int cwdt = -1, chgt = -1, kern = 0;
  int xwidth[256];
  memset(xwidth, 0, sizeof(xwidth));

  // parse ini file
  while (!inif->AtEnd()) {
    VStr line = readLine(inif);
    if (inif->IsError()) { delete inif; tex->release(); tex = nullptr; Sys_Error(va("cannot load font '%s' (error loading description)", *aname)); }
    if (line.isEmpty() || line[0] == ';' || line.startsWith("//")) continue;
    if (line[0] == '[') { currSection = line; continue; }
    // fontmap?
    auto key = getKey(line);
    //fprintf(stderr, "line:<%s>; key:<%s>; intval=%d\n", *line, *key, getIntValue(line));
    if (currSection.equ1251CI("[FontMap]")) {
      if (key.equ1251CI("CharWidth")) { cwdt = getIntValue(line); continue; }
      if (key.equ1251CI("CharHeight")) { chgt = getIntValue(line); continue; }
      if (key.equ1251CI("Kerning")) { kern = getIntValue(line); continue; }
      continue;
    }
    if (currSection.length() < 2 || VStr::digitInBase(currSection[1]) < 0 || !currSection.endsWith("]")) continue;
    if (!key.equ1251CI("Width")) continue;
    int cidx = 0;
    for (int f = 1; f < currSection.length(); ++f) {
      int d = VStr::digitInBase(currSection[f]);
      if (d < 0) {
        if (f != currSection.length()-1) cidx = -1;
        break;
      }
      cidx = cidx*10+d;
    }
    if (cidx >= 0 && cidx < 256) {
      int w = getIntValue(line);
      //fprintf(stderr, "cidx=%d; w=%d\n", cidx, w);
      if (w < 0) w = 0;
      xwidth[cidx] = w;
    }
  }

  delete inif;

  if (cwdt < 1 || chgt < 1) { tex->release(); tex = nullptr; Sys_Error(va("cannot load font '%s' (invalid description 00)", *aname)); }
  int xchars = tex->width/cwdt;
  int ychars = tex->height/chgt;
  if (xchars < 1 || ychars < 1 || xchars*ychars < 128) { tex->release(); tex = nullptr; Sys_Error(va("cannot load font '%s' (invalid description 01)", *aname)); }
  chars.setLength(xchars*ychars);

  fontHeight = chgt;

  for (int cx = 0; cx < xchars; ++cx) {
    for (int cy = 0; cy < ychars; ++cy) {
      FontChar &fc = chars[cy*xchars+cx];
      fc.ch = cy*xchars+cx;
      fc.width = cwdt;
      fc.height = chgt;
      fc.advance = xwidth[fc.ch]+kern;
      if (fc.ch == 32) spaceWidth = fc.advance;
      fc.topofs = 0;
      fc.tx0 = (float)(cx*cwdt)/(float)tex->getWidth();
      fc.ty0 = (float)(cy*chgt)/(float)tex->getHeight();
      fc.tx1 = (float)(cx*cwdt+cwdt)/(float)tex->getWidth();
      fc.ty1 = (float)(cy*chgt+chgt)/(float)tex->getHeight();
      fc.tex = tex;
    }
  }

  name = aname;
  next = fontList;
  fontList = this;

  unguard;
}

//==========================================================================
//
//  VFont::~VFont
//
//==========================================================================
VFont::~VFont() {
  tex->release();
  VFont *prev = nullptr, *cur = fontList;
  while (cur && cur != this) { prev = cur; cur = cur->next; }
  if (cur) {
    if (prev) prev->next = next; else fontList = next;
  }
}


//==========================================================================
//
//  VFont::GetChar
//
//==========================================================================
const VFont::FontChar *VFont::getChar (int ch) const {
  if (ch < 0 || ch >= chars.length()) {
    ch = VStr::upcase1251(ch);
    if (ch < 0 || ch >= chars.length()) return nullptr;
  }
  return &chars[ch];
}


//==========================================================================
//
//  VFont::charWidth
//
//==========================================================================
int VFont::charWidth (int ch) const {
  auto fc = getChar(ch);
  return (fc ? fc->width : 0);
}


//==========================================================================
//
//  VFont::textWidth
//
//==========================================================================
int VFont::textWidth (const VStr &s) const {
  int res = 0;
  for (int f = 0; f < s.length(); ++f) {
    auto fc = getChar(vuint8(s[f]));
    if (fc) res += fc->advance;
  }
  return res;
}


//==========================================================================
//
//  VFont::textHeight
//
//==========================================================================
int VFont::textHeight (const VStr &s) const {
  int res = fontHeight;
  for (int f = 0; f < s.length(); ++f) if (s[f] == '\n') res += fontHeight;
  return res;
}


#endif
