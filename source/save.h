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


extern void SV_SaveGame (int Slot, const VStr &Description);
extern void SV_MapTeleport (VName MapName, int flags=0, int newskill=-1);
extern void SV_LoadGame (int Slot);
extern void SV_InitBaseSlot ();
//extern void SV_UpdateRebornSlot ();
//extern bool SV_LoadQuicksaveSlot ();
//extern void SV_SaveGameToReborn ();
extern bool SV_GetSaveString (int Slot, VStr &Desc);
extern void SV_GetSaveDateString (int Slot, VStr &datestr);
extern void SV_AutoSaveOnLevelExit ();
extern void SV_AutoSave ();
