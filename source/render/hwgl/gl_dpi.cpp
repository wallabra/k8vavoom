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
//**  Copyright (C) 2018-2021 Ketmar Dark
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
#include "../../../libs/core/core.h"

#ifdef VAVOOM_CUSTOM_SPECIAL_SDL
# include <SDL.h>
# if defined(USE_XRANDR) && USE_XRANDR == 1
#  include <SDL2/SDL_syswm.h>
# endif
#else
# include <SDL2/SDL.h>
# if defined(USE_XRANDR) && USE_XRANDR == 1
#  include <SDL2/SDL_syswm.h>
# endif
#endif
#if !defined(SDL_MAJOR_VERSION) || SDL_MAJOR_VERSION != 2
# error "SDL2 required!"
#endif


extern bool OS_SDL_IsXRandRAspectAllowed ();


#if defined(USE_XRANDR) && USE_XRANDR == 1
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

static int bXRandRAvail = -1;


//==========================================================================
//
//  isXRandRAvailable
//
//==========================================================================
static bool isXRandRAvailable (Display *display) {
  if (bXRandRAvail == -1) {
    int major = 0, minor = 0, err = 0;
    if (XQueryExtension(display, "RANDR", &major, &minor, &err) && major != 0) {
      bXRandRAvail = 1;
    } else {
      bXRandRAvail = 0;
    }
    GLog.Logf(NAME_Init, "XRandR: %sfound", (bXRandRAvail ? "" : "not "));
  }
  return bXRandRAvail;
}


//==========================================================================
//
//  tryXRandrGetDPI
//
//==========================================================================
static float tryXRandrGetDPI (Display *display, Window window) {
  XRRScreenResources *xr_screen;
  XWindowAttributes attr;
  int x = 0, y = 0;
  Window child;

  /* do not use XRandR if the X11 server does not support it */
  if (!isXRandRAvailable(display)) return 1.0f;
  if (!OS_SDL_IsXRandRAspectAllowed()) return 1.0f;

  memset(&attr,0,sizeof(attr));
  XGetWindowAttributes(display, window, &attr);
  XTranslateCoordinates(display, window, DefaultRootWindow(display), 0, 0, &x, &y, &child );

  attr.x = x-attr.x;
  attr.y = y-attr.y;

  if ((xr_screen = XRRGetScreenResources(display, DefaultRootWindow(display))) != NULL) {
    /* Look for a valid CRTC  */
    for (int f = 0; f < xr_screen->ncrtc; ++f) {
      XRRCrtcInfo *crtcInfo = XRRGetCrtcInfo(display, xr_screen, xr_screen->crtcs[f]);
      if (crtcInfo == NULL) continue;
      if (crtcInfo->width < 1 || crtcInfo->height < 1) continue;

      GLog.Logf(NAME_Init, "XRandR: CRTC %d: pos=(%d,%d) size=(%d,%d) outputs=%d", f, crtcInfo->x, crtcInfo->y, crtcInfo->width, crtcInfo->height, crtcInfo->noutput);

      /* match our window position to the display, use the center */
      int match_x = attr.x+(attr.width/2);
      int match_y = attr.y+(attr.height/2);

      if (match_x >= crtcInfo->x && match_x < (crtcInfo->x+(int)crtcInfo->width) &&
          match_y >= crtcInfo->y && match_y < (crtcInfo->y+(int)crtcInfo->height))
      {
        GLog.Logf(NAME_Init, "XRandR: Main window CRTC display: pos=(%d,%d) size=(%d,%d) match=(%d,%d)", attr.x, attr.y, attr.width, attr.height, match_x, match_y);
        if (crtcInfo->noutput > 0) {
          for (int c = 0; c < crtcInfo->noutput; ++c) {
            XRROutputInfo *outInfo = XRRGetOutputInfo(display, xr_screen, crtcInfo->outputs[c]);
            if (outInfo == NULL) continue;

            VStr outname;
            if (outInfo->nameLen > 0 && outInfo->name != NULL) {
              outname = VStr(outInfo->name, (int)outInfo->nameLen);
            } else {
              outname = "unknown";
            }

            GLog.Logf(NAME_Init, "XRandR:   output %d: name='%s'; size_pix=(%dx%d); size_mm=(%dx%d)", c, *outname, (int)crtcInfo->width, (int)crtcInfo->height, (int)outInfo->mm_width, (int)outInfo->mm_height);

            /* exit both loops */
            if (outInfo->mm_width > 0 && outInfo->mm_height > 0 &&
                crtcInfo->width > 0 && crtcInfo->height > 0)
            {
              const double wdpi = ((double)crtcInfo->width*25.4)/(double)outInfo->mm_width;
              const double hdpi = ((double)crtcInfo->height*25.4)/(double)outInfo->mm_height;
              const float aspect = (float)(hdpi/wdpi);
              GLog.Logf(NAME_Init, "XRandR:   output aspect: %g  (wdpi=%g; hdpi=%g)", aspect, wdpi, hdpi);
              /* get out */
              XRRFreeOutputInfo(outInfo);
              XRRFreeCrtcInfo(crtcInfo);
              XRRFreeScreenResources(xr_screen);
              return aspect;
            }

            XRRFreeOutputInfo(outInfo);
          }
        }
      }

      XRRFreeCrtcInfo(crtcInfo);
    }

    XRRFreeScreenResources(xr_screen);
  }

  return 1.0f;
}
#endif


//==========================================================================
//
//  OS_SDL_GetMainWindowAspect
//
//==========================================================================
float OS_SDL_GetMainWindowAspect (SDL_Window *win) {
  if (!OS_SDL_IsXRandRAspectAllowed()) return 1.0f;

#if defined(USE_XRANDR) && USE_XRANDR == 1
  SDL_SysWMinfo wminfo;
  memset(&wminfo, 0, sizeof(wminfo));
  SDL_VERSION(&wminfo.version);

  if (SDL_GetWindowWMInfo(win, &wminfo) >= 0) {
    if (wminfo.subsystem == SDL_SYSWM_X11 && wminfo.info.x11.display != NULL) {
      return tryXRandrGetDPI(wminfo.info.x11.display, wminfo.info.x11.window);
    }
  }
#endif

  return 1.0f;
}
