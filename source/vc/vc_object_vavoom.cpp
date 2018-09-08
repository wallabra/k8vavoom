//**************************************************************************
//
//  Cvar functions
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, CvarExists) {
  P_GET_NAME(name);
  RET_BOOL(VCvar::HasVar(*name));
}

IMPLEMENT_FUNCTION(VObject, CreateCvar) {
  P_GET_INT(flags);
  P_GET_STR(help);
  P_GET_STR(def);
  P_GET_NAME(name);
  VCvar::CreateNew(*name, def, help, flags);
}

IMPLEMENT_FUNCTION(VObject, GetCvar) {
  P_GET_NAME(name);
  RET_INT(VCvar::GetInt(*name));
}

IMPLEMENT_FUNCTION(VObject, SetCvar) {
  P_GET_INT(value);
  P_GET_NAME(name);
  VCvar::Set(*name, value);
}

IMPLEMENT_FUNCTION(VObject, GetCvarF) {
  P_GET_NAME(name);
  RET_FLOAT(VCvar::GetFloat(*name));
}

IMPLEMENT_FUNCTION(VObject, SetCvarF) {
  P_GET_FLOAT(value);
  P_GET_NAME(name);
  VCvar::Set(*name, value);
}

IMPLEMENT_FUNCTION(VObject, GetCvarS) {
  P_GET_NAME(name);
  RET_STR(VCvar::GetString(*name));
}

IMPLEMENT_FUNCTION(VObject, SetCvarS) {
  P_GET_STR(value);
  P_GET_NAME(name);
  VCvar::Set(*name, value);
}

IMPLEMENT_FUNCTION(VObject, GetCvarB) {
  P_GET_NAME(name);
  RET_BOOL(VCvar::GetBool(*name));
}


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
