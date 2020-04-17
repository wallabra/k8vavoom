//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**  Copyright (C) 2018-2020 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, version 3 of the License ONLY.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**  You should have received a copy of the GNU General Public License
//**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//**
//**************************************************************************
#ifdef VAVOOM_ARCH_LINUX_SPECIAL_SDL
# include <SDL2/SDL.h>
#else
# include <SDL.h>
#endif
#include "gl_local.h"
#include "../../icondata/k8vavomicondata.c"
#include "splashlogo.inc"
#include "splashfont.inc"

// if not defined, texture will be recreated on each line render
//#define VV_USE_CONFONT_ATLAS_TEXTURE
#define VV_SPLASH_PARTIAL_UPDATES


extern VCvarB ui_want_mouse_at_zero;


class VSdlOpenGLDrawer : public VOpenGLDrawer {
  friend struct SplashDtor;

private:
  enum {
    #if SDL_BYTEORDER == SDL_BIG_ENDIAN
      rmask = 0xff000000u,
      gmask = 0x00ff0000u,
      bmask = 0x0000ff00u,
      amask = 0x000000ffu,
    #else
      rmask = 0x000000ffu,
      gmask = 0x0000ff00u,
      bmask = 0x00ff0000u,
      amask = 0xff000000u,
    #endif
  };

private:
  bool skipAdaptiveVSync;

protected:
  enum { IMG_TEXT_MAXLEN = 80u };
  enum { IMG_TEXT_LINES = 3u };

  SDL_Window *winsplash;
  SDL_Renderer *rensplash;
  SDL_Texture *imgsplash;
  #ifdef VV_USE_CONFONT_ATLAS_TEXTURE
  SDL_Texture *imgsplashfont;
  #endif
  int imgtx, imgty; // where text starts
  int imgtxend; // text box end
  double imgtlastupdate;
  char imgtext[IMG_TEXT_MAXLEN*IMG_TEXT_LINES];
  unsigned imgtextpos;

  // used to cleanup partially created splash window
  struct SplashDtor {
    VSdlOpenGLDrawer *drw;
    vuint8 *pixels;
    PNGHandle *png;
    inline SplashDtor (VSdlOpenGLDrawer *adrw) noexcept : drw(adrw), pixels(nullptr), png(nullptr) {}
    inline ~SplashDtor () {
      if (png) { delete png; png = nullptr; }
      if (pixels) { delete[] pixels; pixels = nullptr; }
      if (drw) {
        #ifdef VV_USE_CONFONT_ATLAS_TEXTURE
        if (drw->imgsplashfont) { SDL_DestroyTexture(drw->imgsplashfont); drw->imgsplashfont = nullptr; }
        #endif
        if (drw->imgsplash) { SDL_DestroyTexture(drw->imgsplash); drw->imgsplash = nullptr; }
        if (drw->rensplash) { SDL_DestroyRenderer(drw->rensplash); drw->rensplash = nullptr; }
        if (drw->winsplash) { SDL_DestroyWindow(drw->winsplash); drw->winsplash = nullptr; }
        drw = nullptr;
      }
    }
    inline void Success () noexcept { drw = nullptr; }
    inline void FreePixels () { if (pixels) { delete[] pixels; pixels = nullptr; } }
    inline void FreePng () { if (png) { delete png; png = nullptr; } }
  };

  inline void clearImgText () noexcept {
    for (unsigned f = 0; f < IMG_TEXT_LINES; ++f) imgtext[f*IMG_TEXT_MAXLEN] = 0;
    imgtextpos = 0;
  }

  static inline unsigned imgTextAdvanceLine (unsigned lpos) noexcept {
    return (lpos+IMG_TEXT_MAXLEN >= IMG_TEXT_MAXLEN*IMG_TEXT_LINES ? 0u : lpos+IMG_TEXT_MAXLEN);
  }

  static inline unsigned imgTextPrevLine (unsigned lpos) noexcept {
    return (lpos ? lpos-IMG_TEXT_MAXLEN : IMG_TEXT_MAXLEN*(IMG_TEXT_LINES-1));
  }

public:
  SDL_Window *hw_window;
  SDL_GLContext hw_glctx;

public:
  VSdlOpenGLDrawer ();

  virtual bool ShowLoadingSplashScreen () override;
  virtual bool IsLoadingSplashActive () override;
  // this can be called regardless of splash screen availability, and even after splash was hidden
  virtual void DrawLoadingSplashText (const char *text, int len=-1) override;
  virtual void HideSplashScreens () override;

  virtual void Init () override;
  virtual bool SetResolution (int, int, int) override;
  virtual void *GetExtFuncPtr (const char *name) override;
  virtual void Update (bool fullUpdate=true) override;
  virtual void Shutdown () override;

  virtual void WarpMouseToWindowCenter () override;
  virtual void GetMousePosition (int *mx, int *my) override;

  virtual void GetRealWindowSize (int *rw, int *rh) override;

private:
  void SetVSync (bool firstTime);

  // this is required for both normal and splash
  static void SetupSDLRequirements ();
  static void SetWindowIcon (SDL_Window *win);

  static void conPutCharAt (vuint8 *pixels, int pixwdt, int pixhgt, int x0, int y0, char ch, unsigned lnum);
};


IMPLEMENT_DRAWER(VSdlOpenGLDrawer, DRAWER_OpenGL, "OpenGL", "SDL OpenGL rasteriser device", "-opengl");

VCvarI gl_current_screen_fsmode("gl_current_screen_fsmode", "0", "Video mode: windowed(0), fullscreen scaled(1), fullscreen real(2)", CVAR_Rom);


//==========================================================================
//
//  VSdlOpenGLDrawer::VSdlOpenGLDrawer
//
//==========================================================================
VSdlOpenGLDrawer::VSdlOpenGLDrawer ()
  : VOpenGLDrawer()
  , skipAdaptiveVSync(false)
  , winsplash(nullptr)
  , rensplash(nullptr)
  , imgsplash(nullptr)
  #ifdef VV_USE_CONFONT_ATLAS_TEXTURE
  , imgsplashfont(nullptr)
  #endif
  , imgtlastupdate(0)
  , imgtextpos(0)
  , hw_window(nullptr)
  , hw_glctx(nullptr)
{
  memset(imgtext, 0, sizeof(imgtext));
}


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
//  VSdlOpenGLDrawer::SetWindowIcon
//
//==========================================================================
void VSdlOpenGLDrawer::SetWindowIcon (SDL_Window *win) {
  enum {
  #if SDL_BYTEORDER == SDL_BIG_ENDIAN
    rmask = 0x0000ff00u,
    gmask = 0x00ff0000u,
    bmask = 0xff000000u,
    amask = 0x000000ffu,
  #else
    rmask = 0x00ff0000u,
    gmask = 0x0000ff00u,
    bmask = 0x000000ffu,
    amask = 0xff000000u,
  #endif
  };
  if (!win) return;
  SDL_Surface *icosfc = SDL_CreateRGBSurfaceFrom(k8vavoomicondata, 32, 32, 32, 32*4, rmask, gmask, bmask, amask);
  if (!icosfc) return;
  SDL_SetWindowIcon(win, icosfc);
  SDL_FreeSurface(icosfc);
}


//==========================================================================
//
//  VSdlOpenGLDrawer::WarpMouseToWindowCenter
//
//==========================================================================
void VSdlOpenGLDrawer::WarpMouseToWindowCenter () {
  if (!hw_window) return;
  int realw = ScreenWidth, realh = ScreenHeight;
  SDL_GL_GetDrawableSize(hw_window, &realw, &realh);
  #if 1
  // k8: better stick with global mouse positioning here
  int wx, wy;
  SDL_GetWindowPosition(hw_window, &wx, &wy);
  SDL_WarpMouseGlobal(wx+realw/2, wy+realh/2);
  #else
  SDL_WarpMouseInWindow(hw_window, realw/2, realh/2);
  #endif
}


//==========================================================================
//
//  VSdlOpenGLDrawer::GetMousePosition
//
//  we have to use `SDL_GetGlobalMouseState()` here, because
//  `SDL_GetMouseState()` is updated in event loop, not immediately
//
//==========================================================================
void VSdlOpenGLDrawer::GetMousePosition (int *mx, int *my) {
  if (!mx && !my) return;
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
          GCon->Log(NAME_Init, "OpenGL: adaptive vsync failed, falling back to normal vsync");
          skipAdaptiveVSync = true;
        }
        SDL_GL_SetSwapInterval(1);
      } else {
        GCon->Log(NAME_Init, "OpenGL: using adaptive vsync");
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
//  VSdlOpenGLDrawer::SetupSDLRequirements
//
//==========================================================================
void VSdlOpenGLDrawer::SetupSDLRequirements () {
  SDL_GL_ResetAttributes(); // just in case
  //k8: require OpenGL 2.1, sorry; non-shader renderer was removed anyway
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
  //#ifdef __SWITCH__
  //fgsfds: libdrm_nouveau requires this, or else shit will be trying to use GLES
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
  //#endif

  // as we are doing rendering to FBO, there is no need to create depth and stencil buffers for FB
  // but shitty intel may require this, so...
  //SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  //SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
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
  HideSplashScreens();

  int Width = AWidth;
  int Height = AHeight;
  if (Width < 320 || Height < 200) {
    // set defaults
    /*
    Width = 800;
    Height = 600;
    */
    //k8: 'cmon, this is silly! let's set something better!
    Width = 1024;
    Height = 768;
  }

  if (fsmode < 0 || fsmode > 2) fsmode = 0;

  // shut down current mode
  Shutdown();

  Uint32 flags = SDL_WINDOW_OPENGL;
       if (fsmode == 1) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
  else if (fsmode == 2) flags |= SDL_WINDOW_FULLSCREEN;

  SetupSDLRequirements();
  // doing it twice is required for some broken setups. oops.
  SetVSync(true); // first time

  hw_window = SDL_CreateWindow("k8vavoom", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, Width, Height, flags);
  if (!hw_window) {
    GCon->Logf("SDL2: cannot create SDL2 window: %s.", SDL_GetError());
    return false;
  }
  SetWindowIcon(hw_window);

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
  if (!gladLoadGLLoader(SDL_GL_GetProcAddress))
    GCon->Logf("GLAD failed to load GL procs!");
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
}


//==========================================================================
//
//  VSdlOpenGLDrawer::GetExtFuncPtr
//
//==========================================================================
void *VSdlOpenGLDrawer::GetExtFuncPtr (const char *name) {
  return SDL_GL_GetProcAddress(name);
}


//==========================================================================
//
//  VSdlOpenGLDrawer::Update
//
//  Blit to the screen / Flip surfaces
//
//==========================================================================
void VSdlOpenGLDrawer::Update (bool fullUpdate) {
  if (fullUpdate) {
    if (mInitialized && hw_window && hw_glctx) callICB(VCB_FinishUpdate);
    FinishUpdate();
  }
  if (hw_window) SDL_GL_SwapWindow(hw_window);
}


//==========================================================================
//
//  VSdlOpenGLDrawer::Shutdown
//
//  Close the graphics
//
//==========================================================================
void VSdlOpenGLDrawer::Shutdown () {
  HideSplashScreens();
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
  if (ui_want_mouse_at_zero) SDL_WarpMouseGlobal(0, 0);
}


//==========================================================================
//
//  VSdlOpenGLDrawer::conPutCharAt
//
//==========================================================================
void VSdlOpenGLDrawer::conPutCharAt (vuint8 *pixels, int pixwdt, int pixhgt, int x0, int y0, char ch, unsigned lnum) {
  const unsigned colors[3] = { 0xff7f00u, 0xdf5f00u, 0xbf3f00u };
  const unsigned conColor = colors[lnum%3];
  int r = (conColor>>16)&0xff;
  int g = (conColor>>8)&0xff;
  int b = conColor&0xff;
  const int rr = r, gg = g, bb = b;
  for (int y = CONFONT_HEIGHT-1; y >= 0; --y) {
    if (y0+y < 0 || y0+y >= pixhgt) break;
    vuint16 v = glConFont10[(ch&0xff)*10+y];
    //immutable uint cc = (b<<16)|(g<<8)|r|0xff000000;
    //const vuint32 cc = (r<<16)|(g<<8)|b|0xff000000u;
    //SDL_SetRenderDrawColor(rdr, clampToByte(r), clampToByte(g), clampToByte(b), SDL_ALPHA_OPAQUE);
    for (int x = 0; x < CONFONT_WIDTH; ++x) {
      if (x0+x < 0) continue;
      if (x0+x >= pixwdt) break;
      if (v&0x8000) {
        vuint8 *dest = pixels+(pixwdt<<2)*(y0+y)+((x0+x)<<2);
        //vsetPixel(conDrawX+x, conDrawY+y, cc);
        //SDL_RenderDrawPoint(rdr, x0+x, y0+y);
        *dest++ = clampToByte(r);
        *dest++ = clampToByte(g);
        *dest++ = clampToByte(b);
        *dest++ = SDL_ALPHA_OPAQUE;
      }
      v <<= 1;
    }
    if ((r -= 7) < 0) r = rr;
    if ((g -= 7) < 0) g = gg;
    if ((b -= 7) < 0) b = bb;
  }
}


//==========================================================================
//
//  VSdlOpenGLDrawer::IsLoadingSplashActive
//
//==========================================================================
bool VSdlOpenGLDrawer::IsLoadingSplashActive () {
  return !!winsplash;
}


//==========================================================================
//
//  VSdlOpenGLDrawer::HideSplashScreens
//
//==========================================================================
void VSdlOpenGLDrawer::HideSplashScreens () {
  if (winsplash) {
    #ifdef VV_USE_CONFONT_ATLAS_TEXTURE
    if (imgsplashfont) { SDL_DestroyTexture(imgsplashfont); imgsplashfont = nullptr; }
    #endif
    if (imgsplash) { SDL_DestroyTexture(imgsplash); imgsplash = nullptr; }
    if (rensplash) { SDL_DestroyRenderer(rensplash); rensplash = nullptr; }
    SDL_DestroyWindow(winsplash);
    winsplash = nullptr;
  }
}


//==========================================================================
//
//  VSdlOpenGLDrawer::ShowLoadingSplashScreen
//
//==========================================================================
bool VSdlOpenGLDrawer::ShowLoadingSplashScreen () {
  //GCon->Log(NAME_Debug, "*** SPLASH ***");
  if (winsplash) return true; // just in case

  VMemoryStreamRO logostream("splashlogo.png", splashlogo, (int)sizeof(splashlogo));

  SplashDtor spdtor(this);

  spdtor.png = M_VerifyPNG(&logostream);
  if (!spdtor.png) return false;

  if (spdtor.png->width < 1 || spdtor.png->width > 1024 || spdtor.png->height < 1 || spdtor.png->height > 768) {
    GCon->Logf(NAME_Warning, "invalid splash logo image dimensions (%dx%d)", spdtor.png->width, spdtor.png->height);
    return false;
  }

  if (!spdtor.png->loadIDAT()) {
    GCon->Log(NAME_Warning, "error decoding splash logo image");
    return false;
  }

  if (logostream.IsError()) {
    GCon->Log(NAME_Warning, "error decoding splash logo image");
    return false;
  }

  int splashWidth = spdtor.png->width;
  int splashHeight = spdtor.png->height;
  //GCon->Logf(NAME_Debug, "splash logo image dimensions (%dx%d)", splashWidth, splashHeight);
  // text coords
  imgtx = 155;
  imgty = 122-9;
  imgtxend = splashWidth-8;

  spdtor.pixels = new vuint8[spdtor.png->width*spdtor.png->height*4];
  vuint8 *dest = spdtor.pixels;
  for (int y = 0; y < spdtor.png->height; ++y) {
    for (int x = 0; x < spdtor.png->width; ++x) {
      auto clr = spdtor.png->getPixel(x, y); // unmultiplied
      *dest++ = clr.r;
      *dest++ = clr.g;
      *dest++ = clr.b;
      *dest++ = clr.a;
    }
  }
  spdtor.FreePng();

  // create window
  Uint32 flags = SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN|SDL_WINDOW_BORDERLESS|SDL_WINDOW_SKIP_TASKBAR|/*SDL_WINDOW_POPUP_MENU*/SDL_WINDOW_TOOLTIP;
  SetupSDLRequirements();
  // turn off vsync
  SDL_GL_SetSwapInterval(0);
  winsplash = SDL_CreateWindow("k8vavoom_splash", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, splashWidth, splashHeight, flags);
  if (!winsplash) {
    GCon->Logf(NAME_Warning, "Cannot create splash window");
    return false;
  }
  SetWindowIcon(winsplash);

  rensplash = SDL_CreateRenderer(winsplash, -1, SDL_RENDERER_SOFTWARE);
  if (!rensplash) {
    GCon->Logf(NAME_Warning, "Cannot create splash renderer");
    return false;
  }

  SDL_Surface *imgsfc = SDL_CreateRGBSurfaceFrom(spdtor.pixels, splashWidth, splashHeight, 32, splashWidth*4, rmask, gmask, bmask, amask);
  if (!imgsfc) {
    GCon->Logf(NAME_Warning, "Cannot create splash image surface");
    return false;
  }

  imgsplash = SDL_CreateTextureFromSurface(rensplash, imgsfc);
  SDL_FreeSurface(imgsfc);
  // we don't need pixels anymore
  spdtor.FreePixels();

  if (!imgsplash) {
    GCon->Logf(NAME_Warning, "Cannot create splash image texture");
    return false;
  }
  SDL_SetTextureBlendMode(imgsplash, SDL_BLENDMODE_NONE);

  #ifdef VV_USE_CONFONT_ATLAS_TEXTURE
  // create texture font
  {
    int fwdt = CONFONT_WIDTH*16;
    int fhgt = CONFONT_HEIGHT*16;
    vuint8 *fpix = new vuint8[(fwdt*4)*fhgt];
    // clear it to transparent
    memset(fpix, 0, (fwdt*4)*fhgt);
    // render chars
    for (int y = 0; y < 16; ++y) {
      for (int x = 0; x < 16; ++x) {
        conPutCharAt(fpix, fwdt, fhgt, x*CONFONT_WIDTH, y*CONFONT_HEIGHT, y*16+x);
      }
    }
    imgsfc = SDL_CreateRGBSurfaceFrom(fpix, fwdt, fhgt, 32, fwdt*4, rmask, gmask, bmask, amask);
    // we don't need pixels anymore
    delete[] fpix;
    if (!imgsfc) {
      GCon->Logf(NAME_Warning, "Cannot create splash image surface");
      return false;
    }
    imgsplashfont = SDL_CreateTextureFromSurface(rensplash, imgsfc);
    SDL_FreeSurface(imgsfc);
    if (imgsplashfont) {
      SDL_SetTextureBlendMode(imgsplashfont, SDL_BLENDMODE_BLEND);
    }
  }
  #endif

  // turn off vsync again
  SDL_GL_SetSwapInterval(0);
  // show it
  SDL_ShowWindow(winsplash);
  SDL_SetWindowPosition(winsplash, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  // turn off vsync once more
  SDL_GL_SetSwapInterval(0);

  // render image
  #ifdef VV_SPLASH_PARTIAL_UPDATES
  SDL_SetRenderDrawColor(rensplash, 0, 0, 0, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(rensplash);
  SDL_RenderCopy(rensplash, imgsplash, NULL, NULL);
  #endif
  DrawLoadingSplashText("Loading...", -1);

  #if 0
  // need to do this to show the window (nope)
  {
    GCon->Logf(NAME_Debug, "splash event loop (enter)");
    SDL_Event ev;
    SDL_PumpEvents();
    GCon->Logf(NAME_Debug, "splash event loop (pumped)");
    while (SDL_PollEvent(&ev)) {
      GCon->Logf(NAME_Debug, "splash event loop (event)");
      if (ev.type == SDL_QUIT) {
        SDL_PushEvent(&ev);
        break;
      }
      /*
      SDL_RenderClear(rensplash);
      SDL_RenderCopy(rensplash, imgtex, NULL, NULL);
      SDL_RenderPresent(rensplash);
      */
    }
    GCon->Logf(NAME_Debug, "splash event loop (leave)");
  }
  //SDL_SetWindowPosition(winsplash, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  #endif

  spdtor.Success();
  return true;
}


//==========================================================================
//
//  VSdlOpenGLDrawer::DrawLoadingSplashText
//
//==========================================================================
void VSdlOpenGLDrawer::DrawLoadingSplashText (const char *text, int len) {
  if (!winsplash) { clearImgText(); return; }
  #ifdef VV_USE_CONFONT_ATLAS_TEXTURE
  if (!imgsplashfont) { clearImgText(); return; }
  #endif
  // copy text line
  if (len < 0) len = (text && text[0] ? (int)strlen(text) : 0);
  unsigned dpos = 0;
  while (len && dpos < IMG_TEXT_MAXLEN && text[0]) {
    // skip colors
    if (text[0] == TEXT_COLOR_ESCAPE) {
      ++text;
      --len;
      if (!len) break;
      const bool sq = (text[0] == '[');
      ++text;
      --len;
      if (!len) break;
      if (sq) {
        while (len && text[0] != ']') { ++text; --len; }
        if (len) {
          ++text;
          --len;
          if (!len) break;
        }
      }
      continue;
    }
    imgtext[imgtextpos+dpos] = text[0];
    ++dpos;
    ++text;
    --len;
  }
  if (dpos < IMG_TEXT_MAXLEN) imgtext[imgtextpos+dpos] = 0;
  // advance line
  imgtextpos = imgTextAdvanceLine(imgtextpos);
  // limit updates
  const double ctt = Sys_Time();
  if (imgtlastupdate > 0 && ctt-imgtlastupdate < 1.0/10.0) return;
  imgtlastupdate = ctt;
  // render image
  {
    #ifdef VV_SPLASH_PARTIAL_UPDATES
    // erase old text
    SDL_Rect srectu;
    srectu.x = imgtx;
    srectu.y = imgty;
    srectu.w = (imgtxend-imgtx);
    srectu.h = CONFONT_HEIGHT*(int)IMG_TEXT_LINES;
    SDL_Rect drectu;
    drectu = srectu;
    SDL_RenderCopy(rensplash, imgsplash, &srectu, &drectu);
    #else
    // copy whole image
    //SDL_SetRenderDrawColor(rensplash, 0, 0, 0, SDL_ALPHA_OPAQUE);
    //SDL_RenderClear(rensplash);
    SDL_RenderCopy(rensplash, imgsplash, NULL, NULL);
    #endif
  }
  // render text
  #ifdef VV_USE_CONFONT_ATLAS_TEXTURE
    SDL_Rect srect;
    srect.w = CONFONT_WIDTH;
    srect.h = CONFONT_HEIGHT;
    SDL_Rect drect;
    drect.w = srect.w;
    drect.h = srect.h;
  #else
    int fwdt = imgtxend-imgtx;
    int fhgt = CONFONT_HEIGHT*(int)IMG_TEXT_LINES;
    vuint8 *fpix = new vuint8[(fwdt*4)*fhgt];
    // clear it to transparent
    memset(fpix, 0, (fwdt*4)*fhgt);
  #endif
  // render lines
  unsigned stpos = imgtextpos;
  int ty = imgty+CONFONT_HEIGHT*(int)(IMG_TEXT_LINES-1);
  for (unsigned f = 0; f < IMG_TEXT_LINES; ++f) {
    stpos = imgTextPrevLine(stpos);
    int tx = imgtx;
    for (unsigned dp = 0; dp < IMG_TEXT_MAXLEN; ++dp) {
      unsigned char fch = (unsigned char)(imgtext[stpos+dp]&0xffu);
      if (!fch) break; // no more
      #ifdef VV_USE_CONFONT_ATLAS_TEXTURE
        srect.x = (fch&0x0f)*srect.w;
        srect.y = (fch>>4)*srect.h;
        drect.x = tx;
        drect.y = ty;
        SDL_RenderCopy(rensplash, imgsplashfont, &srect, &drect);
      #else
        conPutCharAt(fpix, fwdt, fhgt, tx-imgtx, ty-imgty, (char)fch, f);
      #endif
      tx += CONFONT_WIDTH;
      if (tx > imgtxend-CONFONT_WIDTH) break;
    }
    // advance line
    ty -= CONFONT_HEIGHT;
  }
  #ifndef VV_USE_CONFONT_ATLAS_TEXTURE
  // create texture with text
  SDL_Surface *imgsfc = SDL_CreateRGBSurfaceFrom(fpix, fwdt, fhgt, 32, fwdt*4, rmask, gmask, bmask, amask);
  SDL_Texture *ttx = nullptr;
  if (imgsfc) {
    ttx = SDL_CreateTextureFromSurface(rensplash, imgsfc);
    SDL_FreeSurface(imgsfc);
    if (ttx) {
      SDL_SetTextureBlendMode(ttx, SDL_BLENDMODE_BLEND);
      SDL_Rect drect;
      drect.x = imgtx;
      drect.y = imgty;
      drect.w = fwdt;
      drect.h = fhgt;
      SDL_RenderCopy(rensplash, ttx, NULL, &drect);
      SDL_DestroyTexture(ttx);
    }
  }
  // we don't need pixels anymore
  delete[] fpix;
  #endif
  // show it
  SDL_RenderPresent(rensplash);
}
