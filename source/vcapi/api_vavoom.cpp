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
#include "../gamedefs.h"
#ifdef CLIENT
# include "../drawer.h"
#endif


VStream *VPackage::OpenFileStreamRO (VStr fname) { return FL_OpenFileRead(fname); }

void __attribute__((noreturn)) __declspec(noreturn) VPackage::HostErrorBuiltin (VStr msg) { Host_Error("%s", *msg); }
void __attribute__((noreturn)) __declspec(noreturn) VPackage::SysErrorBuiltin (VStr msg) { Sys_Error("%s", *msg); }
void __attribute__((noreturn)) __declspec(noreturn) VPackage::AssertErrorBuiltin (VStr msg) { Sys_Error("Assertion failure: %s", *msg); }

void __attribute__((noreturn)) __declspec(noreturn) VPackage::ExecutorAbort (const char *msg) { Host_Error("%s", msg); }
void __attribute__((noreturn)) __declspec(noreturn) VPackage::IOError (const char *msg) { Host_Error("I/O Error: %s", msg); }

void __attribute__((noreturn)) __declspec(noreturn) VPackage::CompilerFatalError (const char *msg) { Sys_Error("%s", msg); }
void __attribute__((noreturn)) __declspec(noreturn) VPackage::InternalFatalError (const char *msg) { Sys_Error("%s", msg); }


//==========================================================================
//
//  VPackage::LoadObject
//
//==========================================================================
void VPackage::LoadObject (TLocation l) {
  // main engine
  for (unsigned pidx = 0; ; ++pidx) {
    const char *pif = GetPkgImportFile(pidx);
    if (!pif) break;
    VStr mainVC = va("progs/%s/%s", *Name, pif);
    if (FL_FileExists(*mainVC)) {
      // compile package
      //fprintf(stderr, "Loading package '%s' (%s)\n", *Name, *mainVC);
      VStream *Strm = VPackage::OpenFileStreamRO(*mainVC);
      LoadSourceObject(Strm, mainVC, l);
      return;
    }
  }
  Sys_Error("Progs package %s not found", *Name);
}


IMPLEMENT_FREE_FUNCTION(VObject, CvarUnlatchAll) {
  if (GGameInfo && GGameInfo->NetMode < NM_DedicatedServer) {
    VCvar::Unlatch();
  }
}


// native static final float GetLightMaxDist ();
IMPLEMENT_FREE_FUNCTION(VObject, GetLightMaxDist) {
  #ifdef CLIENT
  RET_FLOAT(VRenderLevelDrawer::GetLightMaxDist());
  #else
  RET_FLOAT(2048); // arbitrary
  #endif
}

// native static final float GetDynLightMaxDist ();
IMPLEMENT_FREE_FUNCTION(VObject, GetDynLightMaxDist) {
  #ifdef CLIENT
  RET_FLOAT(VRenderLevelDrawer::GetLightMaxDistDef());
  #else
  RET_FLOAT(1024); // arbitrary
  #endif
}

// native static final float GetLightMaxDistDef (float defval);
IMPLEMENT_FREE_FUNCTION(VObject, GetLightMaxDistDef) {
  float defval;
  vobjGetParam(defval);
  #ifdef CLIENT
  RET_FLOAT(VRenderLevelDrawer::GetLightMaxDistDef(defval));
  #else
  RET_FLOAT(defval);
  #endif
}


// native static final bool CheckViewOrgDistance (const TVec org, const float dist);
IMPLEMENT_FREE_FUNCTION(VObject, CheckViewOrgDistance) {
  TVec org;
  float dist;
  vobjGetParam(org, dist);
  #ifdef CLIENT
  if (cl) {
    RET_BOOL((cl->ViewOrg-org).lengthSquared() < dist*dist);
  } else {
    // no camera
    RET_BOOL(false);
  }
  #else
  // for server, always failed (there is no camera)
  RET_BOOL(false);
  #endif
}

// native static final bool CheckViewOrgDistance2D (const TVec org, const float dist);
IMPLEMENT_FREE_FUNCTION(VObject, CheckViewOrgDistance2D) {
  TVec org;
  float dist;
  vobjGetParam(org, dist);
  #ifdef CLIENT
  if (cl) {
    RET_BOOL((cl->ViewOrg-org).length2DSquared() < dist*dist);
  } else {
    // no camera
    RET_BOOL(false);
  }
  #else
  // for server, always failed (there is no camera)
  RET_BOOL(false);
  #endif
}


//**************************************************************************
//
//  Texture utils
//
//**************************************************************************
IMPLEMENT_FREE_FUNCTION(VObject, CheckTextureNumForName) {
  P_GET_NAME(name);
  RET_INT(GTextureManager.CheckNumForName(name, TEXTYPE_Wall, true));
}

IMPLEMENT_FREE_FUNCTION(VObject, TextureNumForName) {
  P_GET_NAME(name);
  RET_INT(GTextureManager.NumForName(name, TEXTYPE_Wall, false));
}

IMPLEMENT_FREE_FUNCTION(VObject, CheckFlatNumForName) {
  P_GET_NAME(name);
  RET_INT(GTextureManager.CheckNumForName(name, TEXTYPE_Flat, true));
}

IMPLEMENT_FREE_FUNCTION(VObject, FlatNumForName) {
  P_GET_NAME(name);
  RET_INT(GTextureManager.NumForName(name, TEXTYPE_Flat, false));
}

IMPLEMENT_FREE_FUNCTION(VObject, TextureHeight) {
  P_GET_INT(pic);
  RET_FLOAT(GTextureManager.TextureHeight(pic));
}

IMPLEMENT_FREE_FUNCTION(VObject, GetTextureName) {
  P_GET_INT(Handle);
  RET_NAME(GTextureManager.GetTextureName(Handle));
}


//==========================================================================
//
//  Console command functions
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, Cmd_CheckParm) {
  P_GET_STR(str);
  RET_INT(VCommand::CheckParm(*str));
}

IMPLEMENT_FREE_FUNCTION(VObject, Cmd_GetArgC) {
  RET_INT(VCommand::GetArgC());
}

IMPLEMENT_FREE_FUNCTION(VObject, Cmd_GetArgV) {
  P_GET_INT(idx);
  RET_STR(VCommand::GetArgV(idx));
}

IMPLEMENT_FREE_FUNCTION(VObject, CmdBuf_AddText) {
  GCmdBuf << VObject::PF_FormatString();
}


IMPLEMENT_FREE_FUNCTION(VObject, KBCheatClearAll) {
#ifdef CLIENT
  VInputPublic::KBCheatClearAll();
#endif
}

IMPLEMENT_FREE_FUNCTION(VObject, KBCheatAppend) {
  P_GET_STR(concmd);
  P_GET_STR(keys);
#ifdef CLIENT
  VInputPublic::KBCheatAppend(keys, concmd);
#endif
}


IMPLEMENT_FREE_FUNCTION(VObject, AreStateSpritesPresent) {
  P_GET_PTR(VState, State);
  RET_BOOL(State ? R_AreSpritesPresent(State->SpriteIndex) : false);
}


IMPLEMENT_FREE_FUNCTION(VObject, R_GetBloodTranslation) {
  P_GET_INT(color);
  RET_INT(R_GetBloodTranslation(color));
}


//native static final int R_FindNamedTranslation (string name);
IMPLEMENT_FREE_FUNCTION(VObject, R_FindNamedTranslation) {
  VStr name;
  vobjGetParam(name);
  RET_INT(R_FindTranslationByName(name));
}

// native static final int BoxOnLineSide2D (const TVec bmin, const TVec bmax, const ref GameObject::line_t line);
IMPLEMENT_FREE_FUNCTION(VObject, BoxOnLineSide2D) {
  P_GET_PTR(line_t, ld);
  P_GET_VEC(bmax);
  P_GET_VEC(bmin);
  float tmbox[4];
  tmbox[BOX2D_TOP] = bmax.y;
  tmbox[BOX2D_BOTTOM] = bmin.y;
  tmbox[BOX2D_LEFT] = bmin.x;
  tmbox[BOX2D_RIGHT] = bmax.x;
  RET_INT(P_BoxOnLineSide(tmbox, ld));
}


//==========================================================================
//
//  Misc
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, Info_ValueForKey) {
  P_GET_STR(key);
  P_GET_STR(info);
  RET_STR(Info_ValueForKey(info, key));
}

IMPLEMENT_FREE_FUNCTION(VObject, WadLumpPresent) {
  P_GET_NAME(name);
  //fprintf(stderr, "*** <%s> : %d (%d)\n", *name, W_CheckNumForName(name), W_CheckNumForName(name, WADNS_Graphics));
  RET_BOOL(W_CheckNumForName(name) >= 0 || W_CheckNumForName(name, WADNS_Graphics) >= 0);
}

IMPLEMENT_FREE_FUNCTION(VObject, FindAnimDoor) {
  P_GET_INT(BaseTex);
  RET_PTR(R_FindAnimDoor(BaseTex));
}

IMPLEMENT_FREE_FUNCTION(VObject, IsAnimatedTexture) {
  P_GET_INT(texid);
  RET_BOOL(R_IsAnimatedTexture(texid));
}

IMPLEMENT_FREE_FUNCTION(VObject, HasLangString) {
  P_GET_STR(Id);
  RET_BOOL(GLanguage.HasTranslation(*Id));
}

IMPLEMENT_FREE_FUNCTION(VObject, GetLangString) {
  P_GET_STR(Id);
  RET_STR(GLanguage[*Id]);
}

//native static final string TranslateString (string Id); // *WITH* leading '$'
IMPLEMENT_FREE_FUNCTION(VObject, TranslateString) {
  VStr id;
  vobjGetParam(id);
  RET_STR(GLanguage.Translate(id));
}

IMPLEMENT_FREE_FUNCTION(VObject, GetLockDef) {
  P_GET_INT(Lock);
  RET_PTR(GetLockDef(Lock));
}

IMPLEMENT_FREE_FUNCTION(VObject, ParseColor) {
  P_GET_BOOL_OPT(retZeroIfInvalid, false);
  P_GET_STR(Name);
  RET_INT(M_ParseColor(*Name, retZeroIfInvalid));
}

IMPLEMENT_FREE_FUNCTION(VObject, TextColorString) {
  P_GET_INT(Color);
  char buf[3];
  buf[0] = TEXT_COLOR_ESCAPE;
  buf[1] = (Color < CR_BRICK || Color >= NUM_TEXT_COLORS ? '-' : (char)(Color+'A'));
  buf[2] = 0;
  RET_STR(VStr(buf));
}

IMPLEMENT_FREE_FUNCTION(VObject, StartTitleMap) {
  RET_BOOL(Host_StartTitleMap());
}

IMPLEMENT_FREE_FUNCTION(VObject, IsAutoloadingMapFromCLI) {
  RET_BOOL(Host_IsCLIMapStartFound());
}

IMPLEMENT_FREE_FUNCTION(VObject, LoadBinaryLump) {
  P_GET_PTR(TArray<vuint8>, Array);
  P_GET_NAME(LumpName);
  W_LoadLumpIntoArray(LumpName, *Array);
}

IMPLEMENT_FREE_FUNCTION(VObject, IsMapPresent) {
  P_GET_NAME(MapName);
  RET_BOOL(IsMapPresent(MapName));
}

IMPLEMENT_FREE_FUNCTION(VObject, HasDecal) {
  P_GET_NAME(name);
  RET_BOOL(VDecalDef::hasDecal(name));
}


// native static final int W_IterateNS (int Prev, EWadNamespace NS);
IMPLEMENT_FREE_FUNCTION(VObject, W_IterateNS) {
  P_GET_INT(wadns);
  P_GET_INT(prev);
  RET_INT(W_IterateNS(prev, EWadNamespace(wadns)));
}

// native static final int W_IterateFile (int Prev, string Name);
IMPLEMENT_FREE_FUNCTION(VObject, W_IterateFile) {
  P_GET_STR(name);
  P_GET_INT(prev);
  RET_INT(W_IterateFile(prev, name));
}

// native static final int W_LumpLength (int lump);
IMPLEMENT_FREE_FUNCTION(VObject, W_LumpLength) {
  P_GET_INT(lump);
  RET_INT(W_LumpLength(lump));
}

// native static final name W_LumpName (int lump);
IMPLEMENT_FREE_FUNCTION(VObject, W_LumpName) {
  P_GET_INT(lump);
  RET_NAME(W_LumpName(lump));
}

// native static final string W_FullLumpName (int lump);
IMPLEMENT_FREE_FUNCTION(VObject, W_FullLumpName) {
  P_GET_INT(lump);
  RET_STR(W_FullLumpName(lump));
}

// native static final int W_LumpFile (int lump);
IMPLEMENT_FREE_FUNCTION(VObject, W_LumpFile) {
  P_GET_INT(lump);
  RET_INT(W_LumpFile(lump));
}


// native static final bool W_IsIWADLump (int lump);
IMPLEMENT_FREE_FUNCTION(VObject, W_IsIWADLump) { int lump; vobjGetParam(lump); RET_BOOL(W_IsIWADLump(lump)); }
// native static final bool W_IsIWADFile (int file);
IMPLEMENT_FREE_FUNCTION(VObject, W_IsIWADFile) { int file; vobjGetParam(file); RET_BOOL(W_IsIWADFile(file)); }

// native static final bool W_IsWADLump (int lump);
IMPLEMENT_FREE_FUNCTION(VObject, W_IsWADLump) { int lump; vobjGetParam(lump); RET_BOOL(W_IsWADLump(lump)); }
// native static final bool W_IsWADFile (int file);
IMPLEMENT_FREE_FUNCTION(VObject, W_IsWADFile) { int file; vobjGetParam(file); RET_BOOL(W_IsWADFile(file)); }

// native static final bool W_IsPWADLump (int lump);
IMPLEMENT_FREE_FUNCTION(VObject, W_IsPWADLump) { int lump; vobjGetParam(lump); RET_BOOL(W_IsUserWadLump(lump)); }
// native static final bool W_IsPWADFile (int file);
IMPLEMENT_FREE_FUNCTION(VObject, W_IsPWADFile) { int file; vobjGetParam(file); RET_BOOL(W_IsUserWadFile(file)); }


// native static final int W_CheckNumForName (name Name, optional EWadNamespace NS /*= WADNS_Global*/);
IMPLEMENT_FREE_FUNCTION(VObject, W_CheckNumForName) {
  P_GET_INT_OPT(ns, WADNS_Global);
  P_GET_NAME(Name);
  RET_INT(W_CheckNumForName(Name, EWadNamespace(ns)));
}

// native static final int W_GetNumForName (name Name, optional EWadNamespace NS /*= WADNS_Global*/);
IMPLEMENT_FREE_FUNCTION(VObject, W_GetNumForName) {
  P_GET_INT_OPT(ns, WADNS_Global);
  P_GET_NAME(Name);
  RET_INT(W_GetNumForName(Name, EWadNamespace(ns)));
}

// native static final int W_CheckNumForNameInFile (name Name, int File, optional EWadNamespace NS /*= WADNS_Global*/);
IMPLEMENT_FREE_FUNCTION(VObject, W_CheckNumForNameInFile) {
  P_GET_INT_OPT(ns, WADNS_Global);
  P_GET_INT(File);
  P_GET_NAME(Name);
  RET_INT(W_CheckNumForNameInFile(Name, File, EWadNamespace(ns)));
}


// native static final bool FS_FileExists (string fname);
IMPLEMENT_FREE_FUNCTION(VObject, FS_FileExists) {
  P_GET_STR(fname);
  if (!FL_IsSafeDiskFileName(fname)) { RET_BOOL(false); return; }
  VStr diskName = FL_GetUserDataDir(false)+"/"+fname;
  VStream *st = FL_OpenSysFileRead(*diskName);
  RET_BOOL(!!st);
  delete st;
}

// native static final string FS_ReadFileContents (string fname);
IMPLEMENT_FREE_FUNCTION(VObject, FS_ReadFileContents) {
  P_GET_STR(fname);
  if (!FL_IsSafeDiskFileName(fname)) { RET_STR(VStr::EmptyString); return; }
  VStr diskName = FL_GetUserDataDir(false)+"/"+fname;
  VStream *st = FL_OpenSysFileRead(*diskName);
  if (!st) { RET_STR(VStr::EmptyString); return; }
  VStr s;
  if (st->TotalSize() > 0) {
    s.setLength(st->TotalSize());
    st->Serialise(s.getMutableCStr(), s.length());
    if (st->IsError()) s.clear();
  }
  delete st;
  RET_STR(s);
}

// native static final bool FS_WriteFileContents (string fname, string contents);
IMPLEMENT_FREE_FUNCTION(VObject, FS_WriteFileContents) {
  P_GET_STR(contents);
  P_GET_STR(fname);
  if (!FL_IsSafeDiskFileName(fname)) { RET_BOOL(false); return; }
  VStr diskName = FL_GetUserDataDir(true)+"/"+fname;
  VStream *st = FL_OpenSysFileWrite(*diskName);
  if (!st) { RET_BOOL(false); return; }
  if (contents.length()) st->Serialise(*contents, contents.length());
  bool ok = !st->IsError();
  delete st;
  RET_BOOL(ok);
}


#ifdef CLIENT
//k8: sorry!
extern int screenblocks;
#endif

// native static final int R_GetScreenBlocks ();
IMPLEMENT_FREE_FUNCTION(VObject, R_GetScreenBlocks) {
#ifdef CLIENT
  RET_INT(screenblocks);
#else
  // `13` is "fullscreen, no status bar"
  RET_INT(13);
#endif
}
