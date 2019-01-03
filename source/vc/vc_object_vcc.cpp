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

//**************************************************************************
//
//  Cvar functions
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, CvarExists) {
  P_GET_NAME(name);
  RET_BOOL(false);
}

IMPLEMENT_FUNCTION(VObject, CreateCvar) {
  P_GET_INT(flags);
  P_GET_STR(help);
  P_GET_STR(def);
  P_GET_NAME(name);
}

IMPLEMENT_FUNCTION(VObject, GetCvar) {
  P_GET_NAME(name);
  RET_INT(0);
}

IMPLEMENT_FUNCTION(VObject, SetCvar) {
  P_GET_INT(value);
  P_GET_NAME(name);
}

IMPLEMENT_FUNCTION(VObject, GetCvarF) {
  P_GET_NAME(name);
  RET_FLOAT(0);
}

IMPLEMENT_FUNCTION(VObject, SetCvarF) {
  P_GET_FLOAT(value);
  P_GET_NAME(name);
}

IMPLEMENT_FUNCTION(VObject, GetCvarS) {
  P_GET_NAME(name);
  RET_STR(VStr());
}

IMPLEMENT_FUNCTION(VObject, SetCvarS) {
  P_GET_STR(value);
  P_GET_NAME(name);
}

IMPLEMENT_FUNCTION(VObject, GetCvarB) {
  P_GET_NAME(name);
  RET_BOOL(false);
}

IMPLEMENT_FUNCTION(VObject, GetCvarHelp) {
  P_GET_NAME(name);
  RET_STR(VStr());
}


//**************************************************************************
//
//  Texture utils
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, CheckTextureNumForName) {
  P_GET_NAME(name);
  RET_INT(0);
}

IMPLEMENT_FUNCTION(VObject, TextureNumForName) {
  P_GET_NAME(name);
  RET_INT(0);
}

IMPLEMENT_FUNCTION(VObject, CheckFlatNumForName) {
  P_GET_NAME(name);
  RET_INT(0);
}

IMPLEMENT_FUNCTION(VObject, FlatNumForName) {
  P_GET_NAME(name);
  RET_INT(0);
}

IMPLEMENT_FUNCTION(VObject, TextureHeight) {
  P_GET_INT(pic);
  RET_FLOAT(0);
}

IMPLEMENT_FUNCTION(VObject, GetTextureName) {
  P_GET_INT(Handle);
  RET_NAME(NAME_None);
}


//==========================================================================
//
//  Console command functions
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, Cmd_CheckParm) {
  P_GET_STR(str);
  RET_INT(VCommand::CheckParm(*str));
}

IMPLEMENT_FUNCTION(VObject, Cmd_GetArgC) {
  RET_INT(VCommand::GetArgC());
}

IMPLEMENT_FUNCTION(VObject, Cmd_GetArgV) {
  P_GET_INT(idx);
  RET_STR(VCommand::GetArgV(idx));
}

IMPLEMENT_FUNCTION(VObject, CmdBuf_AddText) {
  GCmdBuf << PF_FormatString();
}


IMPLEMENT_FUNCTION(VObject, AreStateSpritesPresent) {
  P_GET_PTR(VState, State);
  RET_BOOL(false);
}


//==========================================================================
//
//  Misc
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, Info_ValueForKey) {
  P_GET_STR(key);
  P_GET_STR(info);
  RET_STR(VStr());
}

IMPLEMENT_FUNCTION(VObject, WadLumpPresent) {
  P_GET_NAME(name);
  RET_BOOL(false);
}

IMPLEMENT_FUNCTION(VObject, FindAnimDoor) {
  P_GET_INT(BaseTex);
  RET_PTR(nullptr);
}

IMPLEMENT_FUNCTION(VObject, GetLangString) {
  P_GET_NAME(Id);
  RET_STR(VStr());
}

IMPLEMENT_FUNCTION(VObject, RGB) {
  P_GET_BYTE(b);
  P_GET_BYTE(g);
  P_GET_BYTE(r);
  RET_INT(0xff000000+(r<<16)+(g<<8)+b);
}

IMPLEMENT_FUNCTION(VObject, RGBA) {
  P_GET_BYTE(a);
  P_GET_BYTE(b);
  P_GET_BYTE(g);
  P_GET_BYTE(r);
  RET_INT((a<<24)+(r<<16)+(g<<8)+b);
}

IMPLEMENT_FUNCTION(VObject, GetLockDef) {
  P_GET_INT(Lock);
  RET_PTR(nullptr);
}

IMPLEMENT_FUNCTION(VObject, ParseColour) {
  P_GET_STR(Name);
  RET_INT(M_ParseColour(Name));
}

IMPLEMENT_FUNCTION(VObject, TextColourString) {
  P_GET_INT(Colour);
  VStr Ret;
  Ret += TEXT_COLOUR_ESCAPE;
  Ret += (Colour < CR_BRICK || Colour >= NUM_TEXT_COLOURS ? '-' : (char)(Colour+'A'));
  RET_STR(Ret);
}

IMPLEMENT_FUNCTION(VObject, StartTitleMap) {
  RET_BOOL(false);
}

IMPLEMENT_FUNCTION(VObject, LoadBinaryLump) {
  P_GET_PTR(TArray<vuint8>, Array);
  P_GET_NAME(LumpName);
}

IMPLEMENT_FUNCTION(VObject, IsMapPresent) {
  P_GET_NAME(MapName);
  RET_BOOL(false);
}

IMPLEMENT_FUNCTION(VObject, HasDecal) {
  P_GET_NAME(name);
  RET_BOOL(false);
}
