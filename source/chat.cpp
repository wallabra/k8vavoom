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
#include "gamedefs.h"
#include "iline.h"


bool chatmodeon;


static TILine w_chat(80);
static VCvarS ChatMacro0("Chatmacro0", "No", "Chat macro #0.", CVAR_Archive);
static VCvarS ChatMacro1("Chatmacro1", "I'm ready to kick butt!", "Chat macro #1.", CVAR_Archive);
static VCvarS ChatMacro2("Chatmacro2", "I'm OK.", "Chat macro #2.", CVAR_Archive);
static VCvarS ChatMacro3("Chatmacro3", "I'm not looking too good!", "Chat macro #3.", CVAR_Archive);
static VCvarS ChatMacro4("Chatmacro4", "Help!", "Chat macro #4.", CVAR_Archive);
static VCvarS ChatMacro5("Chatmacro5", "You suck!", "Chat macro #5.", CVAR_Archive);
static VCvarS ChatMacro6("Chatmacro6", "Next time, scumbag...", "Chat macro #6.", CVAR_Archive);
static VCvarS ChatMacro7("Chatmacro7", "Come here!", "Chat macro #7.", CVAR_Archive);
static VCvarS ChatMacro8("Chatmacro8", "I'll take care of it.", "Chat macro #8.", CVAR_Archive);
static VCvarS ChatMacro9("Chatmacro9", "Yes", "Chat macro #9.", CVAR_Archive);
static VCvarS *chat_macros[10] = {
  &ChatMacro0,
  &ChatMacro1,
  &ChatMacro2,
  &ChatMacro3,
  &ChatMacro4,
  &ChatMacro5,
  &ChatMacro6,
  &ChatMacro7,
  &ChatMacro8,
  &ChatMacro9,
};


//===========================================================================
//
//  CT_Init
//
//  Initialise chat mode data
//
//===========================================================================
void CT_Init () {
  w_chat.SetVisChars(42);
  chatmodeon = false;
}


//===========================================================================
//
//  CT_Stop
//
//===========================================================================
static void CT_Stop () {
  chatmodeon = false;
}


//===========================================================================
//
// CT_Responder
//
//===========================================================================
bool CT_Responder (event_t *ev) {
  if (!chatmodeon) return false;

  if (GInput->AltDown) {
    if (ev->type != ev_keyup) return true;
    if (ev->keycode >= '0' && ev->keycode <= '9') {
      GCmdBuf << "Say " << VStr(chat_macros[ev->keycode-'0']->asStr()).quote() << "\n";
      CT_Stop();
      return true;
    }
  }

  if (ev->keycode == K_ENTER || ev->keycode == K_PADENTER) {
    if (ev->type != ev_keyup) return true;
    if (w_chat.length() != 0) {
      GCmdBuf << "Say " << VStr(w_chat.getCStr()).quote(true) << "\n";
    }
    CT_Stop();
    return true;
  }

  if (ev->keycode == K_ESCAPE) {
    if (ev->type != ev_keyup) return true;
    CT_Stop();
    return true;
  }

  if (ev->type != ev_keydown) return true;
  return w_chat.Key(*ev);
}


//==========================================================================
//
//  COMMAND ChatMode
//
//==========================================================================
COMMAND(ChatMode) {
  w_chat.Init();
  chatmodeon = true;
}


//===========================================================================
//
// CT_Drawer
//
//===========================================================================
void CT_Drawer () {
  if (chatmodeon) {
    T_SetFont(SmallFont);
    T_SetAlign(hleft, vtop);
    w_chat.DrawAt(25, 92, /*CR_UNTRANSLATED*/CR_GREEN);
  }
}
