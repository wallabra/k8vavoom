//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**    $Id$
//**
//**    Copyright (C) 1999-2002 J306nis Legzdi267375
//**
//**    This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**    This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

#include <SDL.h>
#include "gamedefs.h"
#include "drawer.h"

#ifndef MAX_JOYSTICK_BUTTONS
# define MAX_JOYSTICK_BUTTONS  (100)
#endif


// ////////////////////////////////////////////////////////////////////////// //
class VSdlInputDevice : public VInputDevice {
public:
  VSdlInputDevice ();
  ~VSdlInputDevice ();

  virtual void ReadInput () override;
  virtual void RegrabMouse () override; // called by UI when mouse cursor is turned off

private:
  int mouse;
  bool winactive;
  bool firsttime;
  bool uiwasactive;
  bool curHidden;

  int mouse_oldx;
  int mouse_oldy;

  SDL_Joystick *joystick;
  bool joystick_started;
  int joy_num_buttons;
  int joy_x;
  int joy_y;
  int joy_newb[MAX_JOYSTICK_BUTTONS];
  int joy_oldx;
  int joy_oldy;
  int joy_oldb[MAX_JOYSTICK_BUTTONS];

  void StartupJoystick ();
  void PostJoystick ();
};


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB m_filter("m_filter", true, "Filter input?", CVAR_Archive);
static VCvarB ui_mouse("ui_mouse", false, "Allow using mouse in UI?", CVAR_Archive);
static VCvarB ui_active("ui_active", false, "Is UI active (used to stop mouse warping if \"ui_mouse\" is false)?", 0);
static VCvarB m_nograb("m_nograb", false, "Do not grab mouse?", CVAR_Archive);
static VCvarB m_dbg_cursor("m_dbg_cursor", false, "Do not hide (true) mouse cursor on startup?", 0);


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

    case SDLK_BACKQUOTE: return K_BACKQUOTE;
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
  }

  return 0;
}


//==========================================================================
//
//  VSdlInputDevice::VSdlInputDevice
//
//==========================================================================
VSdlInputDevice::VSdlInputDevice ()
  : mouse(0)
  , winactive(false)
  , firsttime(true)
  , uiwasactive(false)
  , curHidden(false)
  , mouse_oldx(0)
  , mouse_oldy(0)
  , joystick(nullptr)
  , joystick_started(false)
  , joy_num_buttons(0)
  , joy_x(0)
  , joy_y(0)
  , joy_oldx(0)
  , joy_oldy(0)
{
  guard(VSdlInputDevice::VSdlInputDevice);
  // always off
  if (!m_dbg_cursor) { curHidden = true; SDL_ShowCursor(0); }
  // mouse and keyboard are setup using SDL's video interface
  mouse = 1;
  if (GArgs.CheckParm("-nomouse")) {
    SDL_EventState(SDL_MOUSEMOTION,     SDL_IGNORE);
    SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_IGNORE);
    SDL_EventState(SDL_MOUSEBUTTONUP,   SDL_IGNORE);
    mouse = 0;
  } else {
    // ignore mouse motion events in any case...
    SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
    SDL_GetMouseState(&mouse_oldx, &mouse_oldy);
    if (Drawer) Drawer->WarpMouseToWindowCenter();
  }

  // initialise joystick
  StartupJoystick();
  unguard;
}


//==========================================================================
//
//  VSdlInputDevice::~VSdlInputDevice
//
//==========================================================================
VSdlInputDevice::~VSdlInputDevice () {
  //guard(VSdlInputDevice::~VSdlInputDevice);
  // on
  SDL_ShowCursor(1);
  if (joystick_started) {
    SDL_JoystickClose(joystick);
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
  }
  //unguard;
}


//==========================================================================
//
//  VSdlInputDevice::RegrabMouse
//
//  Called by UI when mouse cursor is turned off.
//
//==========================================================================
void VSdlInputDevice::RegrabMouse () {
  //FIXME: ignore winactive here, 'cause when mouse is off-window, it may be `false`
  if (mouse /*&& winactive*/) {
    firsttime = true;
    if (Drawer) Drawer->WarpMouseToWindowCenter();
    SDL_GetMouseState(&mouse_oldx, &mouse_oldy);
  }
}

//==========================================================================
//
//  VSdlInputDevice::ReadInput
//
//  Reads input from the input devices.
//
//==========================================================================
void VSdlInputDevice::ReadInput () {
  guard(VSdlInputDevice::ReadInput);
  SDL_Event ev;
  event_t vev;
  //int rel_x = 0, rel_y = 0;
  int mouse_x, mouse_y;
  int normal_value;

  SDL_PumpEvents();
  while (SDL_PollEvent(&ev)) {
    switch (ev.type) {
      case SDL_KEYDOWN:
      case SDL_KEYUP:
        {
          int kk = sdl2TranslateKey(ev.key.keysym.sym);
          if (kk > 0) GInput->KeyEvent(kk, (ev.key.state == SDL_PRESSED) ? 1 : 0);
        }
        break;
      /*
      case SDL_MOUSEMOTION:
        if (!m_oldmode) {
          //fprintf(stderr, "MOTION: x=%d; y=%d\n", ev.motion.xrel, ev.motion.yrel);
          rel_x += ev.motion.xrel;
          rel_y += -ev.motion.yrel;
        }
        break;
      */
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
      case SDL_JOYAXISMOTION:
        normal_value = ev.jaxis.value*127/32767;
             if (ev.jaxis.axis == 0) joy_x = normal_value;
        else if (ev.jaxis.axis == 1) joy_y = normal_value;
        break;
      case SDL_JOYBALLMOTION:
        break;
      case SDL_JOYHATMOTION:
        break;
      case SDL_JOYBUTTONDOWN:
        joy_newb[ev.jbutton.button] = 1;
        break;
      case SDL_JOYBUTTONUP:
        joy_newb[ev.jbutton.button] = 0;
        break;
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
            if (!m_nograb) SDL_CaptureMouse(SDL_TRUE);
            break;
          case SDL_WINDOWEVENT_FOCUS_LOST:
            //fprintf(stderr, "***FOCUS LOST; first=%d; drawer=%p\n", (int)firsttime, Drawer);
            winactive = false;
            firsttime = true;
            SDL_CaptureMouse(SDL_FALSE);
            break;
          //case SDL_WINDOWEVENT_TAKE_FOCUS: Drawer->SDL_SetWindowInputFocus();
        }
        break;
      case SDL_QUIT:
        GCmdBuf << "Quit\n";
        break;
      default:
        break;
    }
  }

  // read mouse separately
  if (mouse && winactive && Drawer) {
    SDL_GetMouseState(&mouse_x, &mouse_y);
    if (!ui_active || ui_mouse) {
      if (!m_dbg_cursor && !curHidden) { curHidden = true; SDL_ShowCursor(0); }
      if (Drawer) Drawer->WarpMouseToWindowCenter();
      int dx = mouse_x-mouse_oldx;
      int dy = mouse_oldy-mouse_y;
      if (dx || dy) {
        if (!firsttime) {
          vev.type = ev_mouse;
          vev.data1 = 0;
          vev.data2 = dx;
          vev.data3 = dy;
          GInput->PostEvent(&vev);
        }
        firsttime = false;
        mouse_oldx = ScreenWidth/2;
        mouse_oldy = ScreenHeight/2;
      }
      uiwasactive = ui_active;
    } else {
      if (ui_active != uiwasactive) {
        uiwasactive = ui_active;
        if (!ui_active) {
          if (!m_nograb) SDL_CaptureMouse(SDL_TRUE);
          firsttime = true;
          if (!m_dbg_cursor && !curHidden) { curHidden = true; SDL_ShowCursor(0); }
        } else {
          SDL_CaptureMouse(SDL_FALSE);
          if (!m_dbg_cursor && curHidden) { curHidden = false; SDL_ShowCursor(1); }
        }
      }
      mouse_oldx = mouse_x;
      mouse_oldy = mouse_y;
    }
  }

  PostJoystick();

  unguard;
}

//**************************************************************************
//**
//**  JOYSTICK
//**
//**************************************************************************

//==========================================================================
//
//  VSdlInputDevice::StartupJoystick
//
//  Initialises joystick
//
//==========================================================================
void VSdlInputDevice::StartupJoystick () {
  guard(VSdlInputDevice::StartupJoystick);
  if (!GArgs.CheckParm("-joystick")) return;

  if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) {
    GCon->Log(NAME_Init, "sdl init joystick failed.");
    return;
  }
  //else {
  //  SDL_JoystickEventState(SDL_IGNORE);
  //  // we are on our own now...
  //}
  joystick = SDL_JoystickOpen(0);
  if (!joystick) return;

  joy_num_buttons = SDL_JoystickNumButtons(joystick);
  joystick_started = true;
  memset(joy_oldb, 0, sizeof(joy_oldb));
  memset(joy_newb, 0, sizeof(joy_newb));
  unguard;
}


//==========================================================================
//
//  VSdlInputDevice::PostJoystick
//
//==========================================================================
void VSdlInputDevice::PostJoystick () {
  guard(VSdlInputDevice::PostJoystick);
  event_t event;

  if (!joystick_started) return;

  if (joy_oldx != joy_x || joy_oldy != joy_y) {
    event.type = ev_joystick;
    event.data1 = 0;
    event.data2 = joy_x;
    event.data3 = joy_y;
    GInput->PostEvent(&event);

    joy_oldx = joy_x;
    joy_oldy = joy_y;
  }

  for (int i = 0; i < joy_num_buttons; ++i) {
    if (joy_newb[i] != joy_oldb[i]) {
      GInput->KeyEvent(K_JOY1+i, joy_newb[i]);
      joy_oldb[i] = joy_newb[i];
    }
  }
  unguard;
}


//**************************************************************************
//**
//**    INPUT
//**
//**************************************************************************

//==========================================================================
//
//  VInputDevice::CreateDevice
//
//==========================================================================
VInputDevice *VInputDevice::CreateDevice () {
  return new VSdlInputDevice();
}
