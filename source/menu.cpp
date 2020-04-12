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
//**
//**  DOOM selection menu, options, episode etc.
//**  Sliders and icons. Kinda widget stuff.
//**
//**************************************************************************
#include "gamedefs.h"
#include "cl_local.h"
#include "ui/ui.h"
#ifdef VAVOOM_NEOUI
# include "neoui/neoui.h"
#endif


//==========================================================================
//
//  MN_Init
//
//==========================================================================
void MN_Init () {
#ifdef SERVER
  GClGame->ClientFlags |= VClientGameBase::CF_LocalServer;
#else
  GClGame->ClientFlags &= ~VClientGameBase::CF_LocalServer;
#endif
  VRootWidget::StaticInit();
  GClGame->eventRootWindowCreated();
}


//==========================================================================
//
//  MN_ActivateMenu
//
//==========================================================================
void MN_ActivateMenu () {
  // intro might call this repeatedly
  if (!MN_Active()) GClGame->eventSetMenu("Main");
}


//==========================================================================
//
//  MN_DeactivateMenu
//
//==========================================================================
void MN_DeactivateMenu () {
  GClGame->eventDeactivateMenu();
}


//==========================================================================
//
//  MN_CheckStartupWarning
//
//==========================================================================
void MN_CheckStartupWarning () {
  if (!GClGame) return;
  if (flWarningMessage.isEmpty()) return;
  GClGame->eventMessageBoxShowWarning(flWarningMessage);
  flWarningMessage.clear();
}


//==========================================================================
//
//  MN_Responder
//
//==========================================================================
bool MN_Responder (event_t *ev) {
  if (GClGame->eventMessageBoxResponder(ev)) return true;

  // show menu?
  if (!MN_Active() && ev->type == ev_keydown && !C_Active() &&
      (!cl || cls.demoplayback || GGameInfo->NetMode == NM_TitleMap))
  {
    bool doActivate = (ev->data1 < K_F1 || ev->data1 > K_F12);
    if (doActivate) {
      VStr down, up;
      GInput->GetBinding(ev->data1, down, up);
      if (down.ICmp("ToggleConsole") == 0) doActivate = false;
    }
    if (doActivate) {
      MN_ActivateMenu();
      return true;
    }
  }

  return GClGame->eventMenuResponder(ev);
}


//==========================================================================
//
//  MN_Active
//
//==========================================================================
bool MN_Active () {
  return (GClGame->eventMenuActive() || GClGame->eventMessageBoxActive());
}


//==========================================================================
//
//  DoMenuCompletions
//
//==========================================================================
static VStr DoMenuCompletions (const TArray<VStr> &args, int aidx, int mode) {
  TArray<VStr> list;
  VStr prefix = (aidx < args.length() ? args[aidx] : VStr());
  if (aidx == 1) {
    GClGame->eventGetAllMenuNames(list, mode);
    return VCommand::AutoCompleteFromListCmd(prefix, list);
  } else {
    return VStr::EmptyString;
  }
}


//==========================================================================
//
//  COMMAND_WITH_AC SetMenu
//
//==========================================================================
COMMAND_WITH_AC(SetMenu) {
  GClGame->eventSetMenu(Args.Num() > 1 ? *Args[1] : "");
}


//==========================================================================
//
//  COMMAND_AC SetMenu
//
//==========================================================================
COMMAND_AC(SetMenu) {
  return DoMenuCompletions(args, aidx, 0);
}


//==========================================================================
//
//  COMMAND_WITH_AC OpenMenu
//
//==========================================================================
COMMAND_WITH_AC(OpenMenu) {
  GClGame->eventSetMenu(Args.Num() > 1 ? *Args[1] : "");
}


//==========================================================================
//
//  COMMAND_AC OpenMenu
//
//==========================================================================
COMMAND_AC(OpenMenu) {
  return DoMenuCompletions(args, aidx, 1);
}


//==========================================================================
//
//  COMMAND_WITH_AC OpenGZMenu
//
//==========================================================================
COMMAND_WITH_AC(OpenGZMenu) {
  GClGame->eventSetMenu(Args.Num() > 1 ? *Args[1] : "");
}


//==========================================================================
//
//  COMMAND_AC OpenGZMenu
//
//==========================================================================
COMMAND_AC(OpenGZMenu) {
  return DoMenuCompletions(args, aidx, -1);
}
