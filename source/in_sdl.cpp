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
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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
#include <SDL.h>
#include "gamedefs.h"
#include "drawer.h"
#include "ui/ui.h"
#include "neoui/neoui.h"

#ifndef MAX_JOYSTICK_BUTTONS
# define MAX_JOYSTICK_BUTTONS  (100)
#endif


static VCvarB ui_control_waiting("ui_control_waiting", false, "Waiting for new control key (pass mouse buttons)?", 0);
VCvarB want_mouse_at_zero("ui_want_mouse_at_zero", false, "Move real mouse cursor to (0,0) when UI activated?", CVAR_Archive);


// ////////////////////////////////////////////////////////////////////////// //
class VSdlInputDevice : public VInputDevice {
public:
  VSdlInputDevice ();
  ~VSdlInputDevice ();

  virtual void ReadInput () override;
  virtual void RegrabMouse () override; // called by UI when mouse cursor is turned off

  virtual void SetClipboardText (const VStr &text) override;
  virtual bool HasClipboardText () override;
  virtual VStr GetClipboardText () override;

private:
  bool mouse;
  bool winactive;
  bool firsttime;
  bool uiwasactive;
  bool uimouselast;
  bool curHidden;

  int mouse_oldx;
  int mouse_oldy;

  vuint32 curmodflags;

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

  void HideRealMouse ();
  void ShowRealMouse ();
};


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB m_filter("m_filter", true, "Filter input?", CVAR_Archive);
static VCvarB ui_mouse("ui_mouse", false, "Allow using mouse in UI?", CVAR_Archive);
static VCvarB ui_active("ui_active", false, "Is UI active (used to stop mouse warping if \"ui_mouse\" is false)?", 0);
static VCvarB m_nograb("m_nograb", false, "Do not grab mouse?", CVAR_Archive);
static VCvarB m_dbg_cursor("m_dbg_cursor", false, "Do not hide (true) mouse cursor on startup?", 0);

//extern VCvarB screen_fsmode;
extern VCvarB gl_current_screen_fsmode;


// ////////////////////////////////////////////////////////////////////////// //
static int sdl2TranslateKey (SDL_Scancode scan) {
  if (scan >= SDL_SCANCODE_A && scan <= SDL_SCANCODE_Z) return (int)(scan-SDL_SCANCODE_A+'a');
  if (scan >= SDL_SCANCODE_1 && scan <= SDL_SCANCODE_9) return (int)(scan-SDL_SCANCODE_1+'1');

  switch (scan) {
    case SDL_SCANCODE_0: return '0';
    case SDL_SCANCODE_SPACE: return ' ';
    case SDL_SCANCODE_MINUS: return '-';
    case SDL_SCANCODE_EQUALS: return '=';
    case SDL_SCANCODE_LEFTBRACKET: return '[';
    case SDL_SCANCODE_RIGHTBRACKET: return ']';
    case SDL_SCANCODE_BACKSLASH: return '\\';
    case SDL_SCANCODE_SEMICOLON: return ';';
    case SDL_SCANCODE_APOSTROPHE: return '\'';
    case SDL_SCANCODE_COMMA: return ',';
    case SDL_SCANCODE_PERIOD: return '.';
    case SDL_SCANCODE_SLASH: return '/';

    case SDL_SCANCODE_UP: return K_UPARROW;
    case SDL_SCANCODE_LEFT: return K_LEFTARROW;
    case SDL_SCANCODE_RIGHT: return K_RIGHTARROW;
    case SDL_SCANCODE_DOWN: return K_DOWNARROW;
    case SDL_SCANCODE_INSERT: return K_INSERT;
    case SDL_SCANCODE_DELETE: return K_DELETE;
    case SDL_SCANCODE_HOME: return K_HOME;
    case SDL_SCANCODE_END: return K_END;
    case SDL_SCANCODE_PAGEUP: return K_PAGEUP;
    case SDL_SCANCODE_PAGEDOWN: return K_PAGEDOWN;

    case SDL_SCANCODE_KP_0: return K_PAD0;
    case SDL_SCANCODE_KP_1: return K_PAD1;
    case SDL_SCANCODE_KP_2: return K_PAD2;
    case SDL_SCANCODE_KP_3: return K_PAD3;
    case SDL_SCANCODE_KP_4: return K_PAD4;
    case SDL_SCANCODE_KP_5: return K_PAD5;
    case SDL_SCANCODE_KP_6: return K_PAD6;
    case SDL_SCANCODE_KP_7: return K_PAD7;
    case SDL_SCANCODE_KP_8: return K_PAD8;
    case SDL_SCANCODE_KP_9: return K_PAD9;

    case SDL_SCANCODE_NUMLOCKCLEAR: return K_NUMLOCK;
    case SDL_SCANCODE_KP_DIVIDE: return K_PADDIVIDE;
    case SDL_SCANCODE_KP_MULTIPLY: return K_PADMULTIPLE;
    case SDL_SCANCODE_KP_MINUS: return K_PADMINUS;
    case SDL_SCANCODE_KP_PLUS: return K_PADPLUS;
    case SDL_SCANCODE_KP_ENTER: return K_PADENTER;
    case SDL_SCANCODE_KP_PERIOD: return K_PADDOT;

    case SDL_SCANCODE_ESCAPE: return K_ESCAPE;
    case SDL_SCANCODE_RETURN: return K_ENTER;
    case SDL_SCANCODE_TAB: return K_TAB;
    case SDL_SCANCODE_BACKSPACE: return K_BACKSPACE;

    case SDL_SCANCODE_GRAVE: return K_BACKQUOTE;
    case SDL_SCANCODE_CAPSLOCK: return K_CAPSLOCK;

    case SDL_SCANCODE_F1: return K_F1;
    case SDL_SCANCODE_F2: return K_F2;
    case SDL_SCANCODE_F3: return K_F3;
    case SDL_SCANCODE_F4: return K_F4;
    case SDL_SCANCODE_F5: return K_F5;
    case SDL_SCANCODE_F6: return K_F6;
    case SDL_SCANCODE_F7: return K_F7;
    case SDL_SCANCODE_F8: return K_F8;
    case SDL_SCANCODE_F9: return K_F9;
    case SDL_SCANCODE_F10: return K_F10;
    case SDL_SCANCODE_F11: return K_F11;
    case SDL_SCANCODE_F12: return K_F12;

    case SDL_SCANCODE_LSHIFT: return K_LSHIFT;
    case SDL_SCANCODE_RSHIFT: return K_RSHIFT;
    case SDL_SCANCODE_LCTRL: return K_LCTRL;
    case SDL_SCANCODE_RCTRL: return K_RCTRL;
    case SDL_SCANCODE_LALT: return K_LALT;
    case SDL_SCANCODE_RALT: return K_RALT;

    case SDL_SCANCODE_LGUI: return K_LWIN;
    case SDL_SCANCODE_RGUI: return K_RWIN;
    case SDL_SCANCODE_MENU: return K_MENU;

    case SDL_SCANCODE_PRINTSCREEN: return K_PRINTSCRN;
    case SDL_SCANCODE_SCROLLLOCK: return K_SCROLLLOCK;
    case SDL_SCANCODE_PAUSE: return K_PAUSE;

    default:
      //if (scan >= ' ' && scan < 127) return (vuint8)scan;
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
  : mouse(false)
  , winactive(false)
  , firsttime(true)
  , uiwasactive(false)
  , uimouselast(false)
  , curHidden(false)
  , mouse_oldx(0)
  , mouse_oldy(0)
  , curmodflags(0)
  , joystick(nullptr)
  , joystick_started(false)
  , joy_num_buttons(0)
  , joy_x(0)
  , joy_y(0)
  , joy_oldx(0)
  , joy_oldy(0)
{
  guard(VSdlInputDevice::VSdlInputDevice);

  // mouse and keyboard are setup using SDL's video interface
  mouse = true;
  if (GArgs.CheckParm("-nomouse")) {
    SDL_EventState(SDL_MOUSEMOTION,     SDL_IGNORE);
    SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_IGNORE);
    SDL_EventState(SDL_MOUSEBUTTONUP,   SDL_IGNORE);
    mouse = false;
  } else {
    // ignore mouse motion events in any case...
    SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
    SDL_GetMouseState(&mouse_oldx, &mouse_oldy);
    if (Drawer) Drawer->WarpMouseToWindowCenter();
  }

  // always off
  HideRealMouse();

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
//  VSdlInputDevice::HideRealMouse
//
//==========================================================================
void VSdlInputDevice::HideRealMouse () {
  if (!curHidden) {
    // real mouse cursor is visible
    if (m_dbg_cursor || !mouse) return;
    curHidden = true;
    SDL_ShowCursor(0);
  } else {
    // real mouse cursor is invisible
    if (m_dbg_cursor || !mouse) {
      curHidden = false;
      SDL_ShowCursor(1);
    }
  }
}


//==========================================================================
//
//  VSdlInputDevice::ShowRealMouse
//
//==========================================================================
void VSdlInputDevice::ShowRealMouse () {
  if (curHidden) {
    // real mouse cursor is invisible
    curHidden = false;
    SDL_ShowCursor(1);
  }
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
  if (mouse) {
    firsttime = true;
    if (Drawer) Drawer->WarpMouseToWindowCenter();
    SDL_GetMouseState(&mouse_oldx, &mouse_oldy);
  }
}


//==========================================================================
//
//  VSdlInputDevice::SetClipboardText
//
//==========================================================================
void VSdlInputDevice::SetClipboardText (const VStr &text) {
  if (text.length() && !text.IsValidUtf8()) {
    VStr s2 = text.Latin1ToUtf8();
    SDL_SetClipboardText(s2.getCStr());
  } else {
    SDL_SetClipboardText(text.getCStr());
  }
}


//==========================================================================
//
//  VSdlInputDevice::HasClipboardText
//
//==========================================================================
bool VSdlInputDevice::HasClipboardText () {
  return !!SDL_HasClipboardText();
}


//==========================================================================
//
//  VSdlInputDevice::GetClipboardText
//
//==========================================================================
VStr VSdlInputDevice::GetClipboardText () {
  char *text = SDL_GetClipboardText();
  if (!text) return VStr::EmptyString;
  VStr str;
  for (const char *p = text; *p; ++p) {
    char ch = *p;
    if (ch <= 0 || ch > 127) {
      ch = '?';
    } else if (ch > 0 && ch < 32) {
           if (ch == '\t') ch = ' ';
      else if (ch != '\n') continue;
    }
    str += ch;
  }
  SDL_free(text);
  return str;
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
    memset((void *)&vev, 0, sizeof(vev));
    vev.modflags = curmodflags;
    switch (ev.type) {
      case SDL_KEYDOWN:
      case SDL_KEYUP:
        {
          int kk = sdl2TranslateKey(ev.key.keysym.scancode);
          if (kk > 0) GInput->PostKeyEvent(kk, (ev.key.state == SDL_PRESSED ? 1 : 0), vev.modflags);
        }
        // now fix flags
        switch (ev.key.keysym.sym) {
          case SDLK_LSHIFT: if (ev.type == SDL_KEYDOWN) curmodflags |= bShiftLeft; else curmodflags &= ~bShiftLeft; break;
          case SDLK_RSHIFT: if (ev.type == SDL_KEYDOWN) curmodflags |= bShiftRight; else curmodflags &= ~bShiftRight; break;
          case SDLK_LCTRL: if (ev.type == SDL_KEYDOWN) curmodflags |= bCtrlLeft; else curmodflags &= ~bCtrlLeft; break;
          case SDLK_RCTRL: if (ev.type == SDL_KEYDOWN) curmodflags |= bCtrlRight; else curmodflags &= ~bCtrlRight; break;
          case SDLK_LALT: if (ev.type == SDL_KEYDOWN) curmodflags |= bAltLeft; else curmodflags &= ~bAltLeft; break;
          case SDLK_RALT: if (ev.type == SDL_KEYDOWN) curmodflags |= bAltRight; else curmodflags &= ~bAltRight; break;
          case SDLK_LGUI: if (ev.type == SDL_KEYDOWN) curmodflags |= bHyper; else curmodflags &= ~bHyper; break;
          case SDLK_RGUI: if (ev.type == SDL_KEYDOWN) curmodflags |= bHyper; else curmodflags &= ~bHyper; break;
          default: break;
        }
        if (curmodflags&(bShiftLeft|bShiftRight)) curmodflags |= bShift; else curmodflags &= ~bShift;
        if (curmodflags&(bCtrlLeft|bCtrlRight)) curmodflags |= bCtrl; else curmodflags &= ~bCtrl;
        if (curmodflags&(bAltLeft|bAltRight)) curmodflags |= bAlt; else curmodflags &= ~bAlt;
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
        vev.type = (ev.button.state == SDL_PRESSED ? ev_keydown : ev_keyup);
             if (ev.button.button == SDL_BUTTON_LEFT) vev.data1 = K_MOUSE1;
        else if (ev.button.button == SDL_BUTTON_RIGHT) vev.data1 = K_MOUSE2;
        else if (ev.button.button == SDL_BUTTON_MIDDLE) vev.data1 = K_MOUSE3;
        else if (ev.button.button == SDL_BUTTON_X1) vev.data1 = K_MOUSE4;
        else if (ev.button.button == SDL_BUTTON_X2) vev.data1 = K_MOUSE5;
        else break;
        if (Drawer) Drawer->GetMousePosition(&vev.data2, &vev.data3);
        if (ui_mouse || !ui_active || ui_control_waiting) VObject::PostEvent(vev);
        // now fix flags
             if (ev.button.button == SDL_BUTTON_LEFT) { if (ev.button.state == SDL_PRESSED) curmodflags |= bLMB; else curmodflags &= ~bLMB; }
        else if (ev.button.button == SDL_BUTTON_RIGHT) { if (ev.button.state == SDL_PRESSED) curmodflags |= bRMB; else curmodflags &= ~bRMB; }
        else if (ev.button.button == SDL_BUTTON_MIDDLE) { if (ev.button.state == SDL_PRESSED) curmodflags |= bMMB; else curmodflags &= ~bMMB; }
        break;
      case SDL_MOUSEWHEEL:
        vev.type = ev_keydown;
             if (ev.wheel.y > 0) vev.data1 = K_MWHEELUP;
        else if (ev.wheel.y < 0) vev.data1 = K_MWHEELDOWN;
        else break;
        if (Drawer) Drawer->GetMousePosition(&vev.data2, &vev.data3);
        if (ui_mouse || !ui_active || ui_control_waiting) VObject::PostEvent(vev);
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
            curmodflags = 0; // just in case
            vev.modflags = 0;
            if (!winactive && mouse) {
              if (Drawer) {
                if (!ui_active || ui_mouse) Drawer->WarpMouseToWindowCenter();
                SDL_GetMouseState(&mouse_oldx, &mouse_oldy);
              }
            }
            firsttime = true;
            winactive = true;
            if (!m_nograb) SDL_CaptureMouse(SDL_TRUE);
            if (cl) cl->ClearInput();
            vev.type = ev_winfocus;
            vev.data1 = 1;
            VObject::PostEvent(vev);
            break;
          case SDL_WINDOWEVENT_FOCUS_LOST:
            //fprintf(stderr, "***FOCUS LOST; first=%d; drawer=%p\n", (int)firsttime, Drawer);
            curmodflags = 0; // just in case
            vev.modflags = 0;
            winactive = false;
            firsttime = true;
            SDL_CaptureMouse(SDL_FALSE);
            if (cl) cl->ClearInput();
            vev.type = ev_winfocus;
            vev.data1 = 0;
            VObject::PostEvent(vev);
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
    // check for UI activity changes
    if (ui_active != uiwasactive) {
      // UI activity changed
      uiwasactive = ui_active;
      uimouselast = ui_mouse;
      if (!ui_active) {
        // ui deactivated
        if (!m_nograb) SDL_CaptureMouse(SDL_TRUE);
        firsttime = true;
        HideRealMouse();
      } else {
        // ui activted
        SDL_CaptureMouse(SDL_FALSE);
        if (!ui_mouse) {
          if (curHidden && want_mouse_at_zero) SDL_WarpMouseGlobal(0, 0);
          ShowRealMouse();
        }
      }
    }
    // check for "allow mouse in UI" changes
    if (ui_mouse != uimouselast) {
      // "allow mouse in UI" changed
      if (ui_active) {
        if (gl_current_screen_fsmode == 0) {
          if (ui_mouse) HideRealMouse(); else ShowRealMouse();
        } else {
          HideRealMouse();
        }
        if (GRoot) GRoot->SetMouse(ui_mouse);
      }
      uimouselast = ui_mouse;
    }
    // hide real mouse in fullscreen mode, show in windowed mode (if necessary)
    if (gl_current_screen_fsmode != 0 && !curHidden) HideRealMouse();
    if (gl_current_screen_fsmode == 0 && curHidden && ui_active && !ui_mouse) ShowRealMouse();
    // generate events
    if (!ui_active || ui_mouse) {
      if (Drawer) Drawer->WarpMouseToWindowCenter();
      int dx = mouse_x-mouse_oldx;
      int dy = mouse_oldy-mouse_y;
      if (dx || dy) {
        if (!firsttime) {
          memset((void *)&vev, 0, sizeof(vev));
          vev.modflags = curmodflags;
          vev.type = ev_mouse;
          vev.data1 = 0;
          vev.data2 = dx;
          vev.data3 = dy;
          VObject::PostEvent(vev);
          vev.type = ev_uimouse;
          vev.data1 = 0;
          vev.data2 = mouse_x;
          vev.data3 = mouse_y;
          VObject::PostEvent(vev);
        }
        firsttime = false;
        mouse_oldx = ScreenWidth/2;
        mouse_oldy = ScreenHeight/2;
      }
    } else {
      mouse_oldx = mouse_x;
      mouse_oldy = mouse_y;
    }
  }

  PostJoystick();

  unguard;
}

#ifdef __SWITCH__
//TEMPORARY: maps joystick buttons to keys for fake kb events
static inline int SwitchJoyToKey(int b) {
  static const int keymap[] = {
      /* KEY_A      */ K_ENTER,
      /* KEY_B      */ K_BACKSPACE,
      /* KEY_X      */ K_RALT,
      /* KEY_Y      */ K_LALT,
      /* KEY_LSTICK */ 0,
      /* KEY_RSTICK */ 0,
      /* KEY_L      */ K_LSHIFT,
      /* KEY_R      */ K_RSHIFT,
      /* KEY_ZL     */ K_SPACE,
      /* KEY_ZR     */ K_LCTRL,
      /* KEY_PLUS   */ K_ESCAPE,
      /* KEY_MINUS  */ K_TAB,
      /* KEY_DLEFT  */ K_LEFTARROW,
      /* KEY_DUP    */ K_UPARROW,
      /* KEY_DRIGHT */ K_RIGHTARROW,
      /* KEY_DDOWN  */ K_DOWNARROW,
  };

  if (b < 8 || b > 15) return 0;
  return keymap[b];
}
#endif

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
#ifndef __SWITCH__
  // always enable joystick on the switch
  if (!GArgs.CheckParm("-joystick")) return;
#endif

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
    VObject::PostEvent(event);

    joy_oldx = joy_x;
    joy_oldy = joy_y;
  }

  for (int i = 0; i < joy_num_buttons; ++i) {
    if (joy_newb[i] != joy_oldb[i]) {
#ifdef __SWITCH__
      //TEMPORARY: also translate some buttons to keys
      int key = SwitchJoyToKey(i);
      if (key) GInput->PostKeyEvent(key, joy_newb[i], curmodflags);
#endif
      GInput->PostKeyEvent(K_JOY1+i, joy_newb[i], curmodflags);
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
