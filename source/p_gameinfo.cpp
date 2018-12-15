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
#include "gamedefs.h"
#include "cl_local.h"
#include "sv_local.h"


IMPLEMENT_CLASS(V, GameInfo)

VGameInfo *GGameInfo;


//==========================================================================
//
//  VGameInfo::VGameInfo
//
//==========================================================================
/*
VGameInfo::VGameInfo ()
  : AcsHelper(E_NoInit)
  , GenericConScript(E_NoInit)
  , PlayerClasses(E_NoInit)
{
}
*/


//==========================================================================
//
//  VGameInfo::IsPaused
//
//==========================================================================
bool VGameInfo::IsPaused () {
  guard(VGameInfo::IsPaused);
  if (NetMode <= NM_TitleMap) return false;
#ifdef CLIENT
  // in single player pause game if in menu or console
  return (Flags&GIF_Paused) || (NetMode == NM_Standalone && (MN_Active() || C_Active()));
#endif
  return !!(Flags&GIF_Paused);
  unguard;
}

IMPLEMENT_FUNCTION(VGameInfo, get_isPaused) {
  P_GET_SELF;
  RET_BOOL(Self ? Self->IsPaused() : false);
}


//==========================================================================
//
//  COMMAND ClearPlayerClasses
//
//==========================================================================
COMMAND(ClearPlayerClasses) {
  guard(COMMAND ClearPlayerClasses);
  if (!ParsingKeyConf) return;
  GGameInfo->PlayerClasses.Clear();
  unguard;
}


//==========================================================================
//
//  COMMAND AddPlayerClass
//
//==========================================================================
COMMAND(AddPlayerClass) {
  guard(COMMAND AddPlayerClass);
  if (!ParsingKeyConf) return;

  if (Args.Num() < 2) {
    GCon->Logf(NAME_Warning, "AddPlayerClass: Player class name missing");
    return;
  }

  VClass *Class = VClass::FindClassNoCase(*Args[1]);
  if (!Class) {
    GCon->Logf(NAME_Warning, "AddPlayerClass: No such class `%s`", *Args[1]);
    return;
  }

  VClass *PPClass = VClass::FindClass("PlayerPawn");
  if (!PPClass) {
    GCon->Logf(NAME_Warning, "AddPlayerClass: Can't find PlayerPawn class");
    return;
  }

  if (!Class->IsChildOf(PPClass)) {
    GCon->Logf(NAME_Warning, "AddPlayerClass: '%s' is not a player pawn class", *Args[1]);
    return;
  }

  GGameInfo->PlayerClasses.Append(Class);
  unguard;
}


//==========================================================================
//
//  COMMAND WeaponSection
//
//==========================================================================
COMMAND(WeaponSection) {
  guard(COMMAND WeaponSection);
  GGameInfo->eventCmdWeaponSection(Args.Num() > 1 ? Args[1] : "");
  unguard;
}


//==========================================================================
//
//  COMMAND SetSlot
//
//==========================================================================
COMMAND(SetSlot) {
  guard(COMMAND SetSlot);
  GGameInfo->eventCmdSetSlot(&Args);
  unguard;
}


//==========================================================================
//
//  COMMAND AddSlotDefault
//
//==========================================================================
COMMAND(AddSlotDefault) {
  guard(COMMAND AddSlotDefault);
  GGameInfo->eventCmdAddSlotDefault(&Args);
  unguard;
}
