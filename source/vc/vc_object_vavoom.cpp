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
#include "../drawer.h"
#ifdef CLIENT
extern refdef_t refdef;
#endif


//**************************************************************************
//
//  Texture utils
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, CheckTextureNumForName) {
  P_GET_NAME(name);
  RET_INT(GTextureManager.CheckNumForName(name, TEXTYPE_Wall, true, false));
}

IMPLEMENT_FUNCTION(VObject, TextureNumForName) {
  P_GET_NAME(name);
  RET_INT(GTextureManager.NumForName(name, TEXTYPE_Wall, true, false));
}

IMPLEMENT_FUNCTION(VObject, CheckFlatNumForName) {
  P_GET_NAME(name);
  RET_INT(GTextureManager.CheckNumForName(name, TEXTYPE_Flat, true, false));
}

IMPLEMENT_FUNCTION(VObject, FlatNumForName) {
  P_GET_NAME(name);
  RET_INT(GTextureManager.NumForName(name, TEXTYPE_Flat, true, false));
}

IMPLEMENT_FUNCTION(VObject, TextureHeight) {
  P_GET_INT(pic);
  RET_FLOAT(GTextureManager.TextureHeight(pic));
}

IMPLEMENT_FUNCTION(VObject, GetTextureName) {
  P_GET_INT(Handle);
  RET_NAME(GTextureManager.GetTextureName(Handle));
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


IMPLEMENT_FUNCTION(VObject, KBCheatClearAll) {
#ifdef CLIENT
  VInputPublic::KBCheatClearAll();
#endif
}

IMPLEMENT_FUNCTION(VObject, KBCheatAppend) {
  P_GET_STR(concmd);
  P_GET_STR(keys);
#ifdef CLIENT
  VInputPublic::KBCheatAppend(keys, concmd);
#endif
}


IMPLEMENT_FUNCTION(VObject, AreStateSpritesPresent) {
  P_GET_PTR(VState, State);
  RET_BOOL(State ? R_AreSpritesPresent(State->SpriteIndex) : false);
}


//==========================================================================
//
//  Misc
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, Info_ValueForKey) {
  P_GET_STR(key);
  P_GET_STR(info);
  RET_STR(Info_ValueForKey(info, key));
}

IMPLEMENT_FUNCTION(VObject, WadLumpPresent) {
  P_GET_NAME(name);
  //fprintf(stderr, "*** <%s> : %d (%d)\n", *name, W_CheckNumForName(name), W_CheckNumForName(name, WADNS_Graphics));
  RET_BOOL(W_CheckNumForName(name) >= 0 || W_CheckNumForName(name, WADNS_Graphics) >= 0);
}

IMPLEMENT_FUNCTION(VObject, FindAnimDoor) {
  P_GET_INT(BaseTex);
  RET_PTR(R_FindAnimDoor(BaseTex));
}

IMPLEMENT_FUNCTION(VObject, GetLangString) {
  P_GET_NAME(Id);
  RET_STR(GLanguage[Id]);
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
  RET_PTR(GetLockDef(Lock));
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
  RET_BOOL(Host_StartTitleMap());
}

IMPLEMENT_FUNCTION(VObject, LoadBinaryLump) {
  P_GET_PTR(TArray<vuint8>, Array);
  P_GET_NAME(LumpName);
  W_LoadLumpIntoArray(LumpName, *Array);
}

IMPLEMENT_FUNCTION(VObject, IsMapPresent) {
  P_GET_NAME(MapName);
  RET_BOOL(IsMapPresent(MapName));
}

IMPLEMENT_FUNCTION(VObject, HasDecal) {
  P_GET_NAME(name);
  RET_BOOL(VDecalDef::hasDecal(name));
}


// native static final int W_IterateNS (int Prev, EWadNamespace NS);
IMPLEMENT_FUNCTION(VObject, W_IterateNS) {
  P_GET_INT(wadns);
  P_GET_INT(prev);
  RET_INT(W_IterateNS(prev, EWadNamespace(wadns)));
}

// native static final int W_IterateFile (int Prev, string Name);
IMPLEMENT_FUNCTION(VObject, W_IterateFile) {
  P_GET_STR(name);
  P_GET_INT(prev);
  RET_INT(W_IterateFile(prev, name));
}

// native static final int W_LumpLength (int lump);
IMPLEMENT_FUNCTION(VObject, W_LumpLength) {
  P_GET_INT(lump);
  RET_INT(W_LumpLength(lump));
}

// native static final name W_LumpName (int lump);
IMPLEMENT_FUNCTION(VObject, W_LumpName) {
  P_GET_INT(lump);
  RET_NAME(W_LumpName(lump));
}

// native static final string W_FullLumpName (int lump);
IMPLEMENT_FUNCTION(VObject, W_FullLumpName) {
  P_GET_INT(lump);
  RET_STR(W_FullLumpName(lump));
}

// native static final int W_LumpFile (int lump);
IMPLEMENT_FUNCTION(VObject, W_LumpFile) {
  P_GET_INT(lump);
  RET_INT(W_LumpFile(lump));
}

// native static final int W_CheckNumForName (name Name, optional EWadNamespace NS /*= WADNS_Global*/);
IMPLEMENT_FUNCTION(VObject, W_CheckNumForName) {
  P_GET_INT_OPT(ns, WADNS_Global);
  P_GET_NAME(Name);
  RET_INT(W_CheckNumForName(Name, EWadNamespace(ns)));
}

// native static final int W_GetNumForName (name Name, optional EWadNamespace NS /*= WADNS_Global*/);
IMPLEMENT_FUNCTION(VObject, W_GetNumForName) {
  P_GET_INT_OPT(ns, WADNS_Global);
  P_GET_NAME(Name);
  RET_INT(W_GetNumForName(Name, EWadNamespace(ns)));
}

// native static final int W_CheckNumForNameInFile (name Name, int File, optional EWadNamespace NS /*= WADNS_Global*/);
IMPLEMENT_FUNCTION(VObject, W_CheckNumForNameInFile) {
  P_GET_INT_OPT(ns, WADNS_Global);
  P_GET_INT(File);
  P_GET_NAME(Name);
  RET_INT(W_CheckNumForNameInFile(Name, File, EWadNamespace(ns)));
}

// native static final void GetCurrRefDef (out refdef_t rd);
IMPLEMENT_FUNCTION(VObject, GetCurrRefDef) {
  P_GET_PTR(refdef_t, rd);
#ifdef CLIENT
  if (rd) *rd = refdef;
#else
  if (rd) memset(rd, 0, sizeof(refdef_t));
#endif
}
