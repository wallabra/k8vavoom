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
//**  Builtins.
//**
//**************************************************************************
#include "../gamedefs.h"
#include "../net/network.h" /* for server list */
#include "../server/sv_local.h"
#include "../client/cl_local.h"
#include "../drawer.h"


VClass *SV_FindClassFromEditorId (int Id, int GameFilter);
VClass *SV_FindClassFromScriptId (int Id, int GameFilter);


#ifdef SERVER

//**************************************************************************
//
//  Map utilites
//
//**************************************************************************

IMPLEMENT_FREE_FUNCTION(VObject, SectorClosestPoint) {
  sector_t *sec;
  TVec point;
  vobjGetParam(sec, point);
  RET_VEC(P_SectorClosestPoint(sec, point));
}


//==========================================================================
//
//  LineOpenings
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, LineOpenings) {
  line_t *linedef;
  TVec *point;
  VOptParamInt blockmask(SPF_NOBLOCKING);
  VOptParamBool do3dmidtex(false);
  vobjGetParam(linedef, point, blockmask, do3dmidtex);
  RET_PTR(SV_LineOpenings(linedef, *point, blockmask, do3dmidtex));
}


// native static final bool P_GetMidTexturePosition (const line_t *line, int sideno, out float ptextop, out float ptexbot);
IMPLEMENT_FREE_FUNCTION(VObject, P_GetMidTexturePosition) {
  line_t *ld;
  int sideno;
  float *ptextop;
  float *ptexbot;
  vobjGetParam(ld, sideno, ptextop, ptexbot);
  RET_BOOL(P_GetMidTexturePosition(ld, sideno, ptextop, ptexbot));
}


//native static final void GetSectorGapCoords (const GameObject::sector_t *sector, const ref TVec point, out float floorz, out float ceilz);
IMPLEMENT_FREE_FUNCTION(VObject, GetSectorGapCoords) {
  sector_t *sector;
  TVec *point;
  float *floorz;
  float *ceilz;
  vobjGetParam(sector, point, floorz, ceilz);
  SV_GetSectorGapCoords(sector, *point, *floorz, *ceilz);
}


IMPLEMENT_FREE_FUNCTION(VObject, FindOpening) {
  opening_t *gaps;
  float z1, z2;
  vobjGetParam(gaps, z1, z2);
  RET_PTR(SV_FindOpening(gaps, z1, z2));
}


//**************************************************************************
//
//  Sound functions
//
//**************************************************************************

IMPLEMENT_FREE_FUNCTION(VObject, GetSoundPlayingInfo) {
  VEntity *mobj;
  int id;
  vobjGetParam(mobj, id);
#ifdef CLIENT
  RET_BOOL(GAudio->IsSoundPlaying(mobj->SoundOriginID, id));
#else
  RET_BOOL(false);
#endif
}

IMPLEMENT_FREE_FUNCTION(VObject, GetSoundID) {
  VName Name;
  vobjGetParam(Name);
  RET_INT(GSoundManager->GetSoundID(Name));
}

IMPLEMENT_FREE_FUNCTION(VObject, StopAllSounds) {
#ifdef CLIENT
  GAudio->StopAllSound();
#endif
}

IMPLEMENT_FREE_FUNCTION(VObject, SetSeqTrans) {
  VName Name;
  int Num, SeqType;
  vobjGetParam(Name, Num, SeqType);
  GSoundManager->SetSeqTrans(Name, Num, SeqType);
}

IMPLEMENT_FREE_FUNCTION(VObject, GetSeqTrans) {
  int Num, SeqType;
  vobjGetParam(Num, SeqType);
  RET_NAME(GSoundManager->GetSeqTrans(Num, SeqType));
}

IMPLEMENT_FREE_FUNCTION(VObject, GetSeqSlot) {
  VName Name;
  vobjGetParam(Name);
  RET_NAME(GSoundManager->GetSeqSlot(Name));
}


IMPLEMENT_FREE_FUNCTION(VObject, TerrainType) {
  int pic;
  vobjGetParam(pic);
  RET_PTR(SV_TerrainType(pic));
}


IMPLEMENT_FREE_FUNCTION(VObject, SB_Start) {
#ifdef CLIENT
  SB_Start();
#endif
}


IMPLEMENT_FREE_FUNCTION(VObject, FindClassFromEditorId) {
  int Id, GameFilter;
  vobjGetParam(Id, GameFilter);
  RET_PTR(SV_FindClassFromEditorId(Id, GameFilter));
}


//==========================================================================
//
//  FindClassFromScriptId
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, FindClassFromScriptId) {
  int Id, GameFilter;
  vobjGetParam(Id, GameFilter);
  RET_PTR(SV_FindClassFromScriptId(Id, GameFilter));
}


//==========================================================================
//
//  various
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, P_GetMapName) {
  int map;
  vobjGetParam(map);
  RET_STR(P_GetMapName(map));
}


IMPLEMENT_FREE_FUNCTION(VObject, P_GetMapIndexByLevelNum) {
  int map;
  vobjGetParam(map);
  RET_INT(P_GetMapIndexByLevelNum(map));
}


IMPLEMENT_FREE_FUNCTION(VObject, P_GetNumMaps) {
  RET_INT(P_GetNumMaps());
}


IMPLEMENT_FREE_FUNCTION(VObject, P_GetMapInfo) {
  int map;
  vobjGetParam(map);
  RET_PTR(P_GetMapInfoPtr(map));
}


IMPLEMENT_FREE_FUNCTION(VObject, P_GetMapLumpName) {
  int map;
  vobjGetParam(map);
  RET_NAME(P_GetMapLumpName(map));
}


IMPLEMENT_FREE_FUNCTION(VObject, P_TranslateMap) {
  int map;
  vobjGetParam(map);
  RET_NAME(P_TranslateMap(map));
}


IMPLEMENT_FREE_FUNCTION(VObject, P_GetNumEpisodes) {
  RET_INT(P_GetNumEpisodes());
}


IMPLEMENT_FREE_FUNCTION(VObject, P_GetEpisodeDef) {
  int Index;
  vobjGetParam(Index);
  RET_PTR(P_GetEpisodeDef(Index));
}


IMPLEMENT_FREE_FUNCTION(VObject, P_GetNumSkills) {
  RET_INT(P_GetNumSkills());
}


IMPLEMENT_FREE_FUNCTION(VObject, P_GetSkillDef) {
  int Index;
  vobjGetParam(Index);
  RET_PTR(const_cast<VSkillDef*>(P_GetSkillDef(Index)));
}


IMPLEMENT_FREE_FUNCTION(VObject, SV_GetSaveString) {
  int i;
  VStr *buf;
  vobjGetParam(i, buf);
  if (!buf) { RET_INT(0); return; }
#ifdef SERVER
  RET_INT(SV_GetSaveString(i, *buf));
#else
  RET_INT(0);
#endif
}


IMPLEMENT_FREE_FUNCTION(VObject, SV_GetSaveDateString) {
  int i;
  VStr *buf;
  vobjGetParam(i, buf);
  if (!buf) return;
#ifdef SERVER
  SV_GetSaveDateString(i, *buf);
#else
  *buf = VStr("UNKNOWN");
#endif
}


IMPLEMENT_FREE_FUNCTION(VObject, LoadTextLump) {
  VName name;
  vobjGetParam(name);
  RET_STR(W_LoadTextLump(name));
}
#endif // SERVER


#ifdef CLIENT
# define CVC_CALC_EXPR(expr_)  expr_
#else
# define CVC_CALC_EXPR(expr_)  0
#endif

//**************************************************************************
//
//  Graphics
//
//**************************************************************************

IMPLEMENT_FREE_FUNCTION(VObject, SetVirtualScreen) {
  int Width, Height;
  vobjGetParam(Width, Height);
  #ifdef CLIENT
  SCR_SetVirtualScreen(Width, Height);
  #endif
}

IMPLEMENT_FREE_FUNCTION(VObject, GetVirtualWidth) { RET_INT(CVC_CALC_EXPR(VirtualWidth)); }
IMPLEMENT_FREE_FUNCTION(VObject, GetVirtualHeight) { RET_INT(CVC_CALC_EXPR(VirtualHeight)); }

IMPLEMENT_FREE_FUNCTION(VObject, GetRealScreenWidth) { RET_INT(CVC_CALC_EXPR(ScreenWidth)); }
IMPLEMENT_FREE_FUNCTION(VObject, GetRealScreenHeight) { RET_INT(CVC_CALC_EXPR(ScreenHeight)); }


IMPLEMENT_FREE_FUNCTION(VObject, R_RegisterPic) {
  VName name;
  vobjGetParam(name);
  RET_INT(GTextureManager.AddPatch(name, TEXTYPE_Pic));
}


IMPLEMENT_FREE_FUNCTION(VObject, R_RegisterPicPal) {
  VName name, palname;
  vobjGetParam(name, palname);
  RET_INT(GTextureManager.AddRawWithPal(name, palname));
}


IMPLEMENT_FREE_FUNCTION(VObject, R_RegisterPicPath) {
  VName path;
  vobjGetParam(path);
  RET_INT(GTextureManager.AddFileTextureChecked(path, TEXTYPE_Pic));
}


IMPLEMENT_FREE_FUNCTION(VObject, R_GetPicInfo) {
  int handle;
  picinfo_t *info;
  vobjGetParam(handle, info);
  GTextureManager.GetTextureInfo(handle, info);
}


//native static final void R_GetPicRealDimensions (int handle, out int x0, out int y0, out int width, out int height, optional bool unscaled);
IMPLEMENT_FREE_FUNCTION(VObject, R_GetPicRealDimensions) {
  int handle;
  int *x0, *y0, *width, *height;
  VOptParamBool unscaled(false);
  vobjGetParam(handle, x0, y0, width, height, unscaled);
  VTexture *tx = GTextureManager.getIgnoreAnim(handle);
  if (tx) {
    if (x0) {
      int v = tx->GetRealX0();
      if (!unscaled) v = v/tx->SScale;
      *x0 = v;
    }
    if (y0) {
      int v = tx->GetRealY0();
      if (!unscaled) v = v/tx->TScale;
      *y0 = v;
    }
    if (width) {
      int v = tx->GetRealWidth();
      if (!unscaled) v = v/tx->SScale;
      *width = v;
    }
    if (height) {
      int v = tx->GetRealHeight();
      if (!unscaled) v = v/tx->TScale;
      *height = v;
    }
  } else {
    if (x0) *x0 = 0;
    if (y0) *y0 = 0;
    if (width) *width = 0;
    if (height) *height = 0;
  }
}


IMPLEMENT_FREE_FUNCTION(VObject, R_DrawPic) {
  int x, y, handle;
  VOptParamFloat alpha(1.0f);
  vobjGetParam(x, y, handle, alpha);
  #ifdef CLIENT
  R_DrawPic(x, y, handle, alpha);
  #endif
}


IMPLEMENT_FREE_FUNCTION(VObject, R_DrawPicFloat) {
  float x, y;
  int handle;
  VOptParamFloat alpha(1.0f);
  vobjGetParam(x, y, handle, alpha);
  #ifdef CLIENT
  R_DrawPicFloat(x, y, handle, alpha);
  #endif
}


IMPLEMENT_FREE_FUNCTION(VObject, R_DrawPicPart) {
  int x, y;
  float pwdt, phgt;
  int handle;
  VOptParamFloat alpha(1.0f);
  vobjGetParam(x, y, pwdt, phgt, handle, alpha);
  #ifdef CLIENT
  R_DrawPicPart(x, y, pwdt, phgt, handle, alpha);
  #endif
}


IMPLEMENT_FREE_FUNCTION(VObject, R_DrawPicFloatPart) {
  float x, y;
  float pwdt, phgt;
  int handle;
  VOptParamFloat alpha(1.0f);
  vobjGetParam(x, y, pwdt, phgt, handle, alpha);
  #ifdef CLIENT
  R_DrawPicFloatPart(x, y, pwdt, phgt, handle, alpha);
  #endif
}


IMPLEMENT_FREE_FUNCTION(VObject, R_DrawPicPartEx) {
  int x, y;
  float tx0, ty0;
  float tx1, ty1;
  int handle;
  VOptParamFloat alpha(1.0f);
  vobjGetParam(x, y, tx0, ty0, tx1, ty1, handle, alpha);
  #ifdef CLIENT
  R_DrawPicPartEx(x, y, tx0, ty0, tx1, ty1, handle, alpha);
  #endif
}


IMPLEMENT_FREE_FUNCTION(VObject, R_DrawPicFloatPartEx) {
  float x, y;
  float tx0, ty0;
  float tx1, ty1;
  int handle;
  VOptParamFloat alpha(1.0f);
  vobjGetParam(x, y, tx0, ty0, tx1, ty1, handle, alpha);
  #ifdef CLIENT
  R_DrawPicFloatPartEx(x, y, tx0, ty0, tx1, ty1, handle, alpha);
  #endif
}


IMPLEMENT_FREE_FUNCTION(VObject, R_InstallSprite) {
  VStr name;
  int index;
  vobjGetParam(name, index);
  R_InstallSprite(*name, index);
}


IMPLEMENT_FREE_FUNCTION(VObject, R_InstallSpriteComplete) {
  R_InstallSpriteComplete();
}


IMPLEMENT_FREE_FUNCTION(VObject, R_DrawSpritePatch) {
  float x, y;
  int sprite;
  VOptParamInt frame(0);
  VOptParamInt rot(0);
  VOptParamInt TranslStart(0);
  VOptParamInt TranslEnd(0);
  VOptParamInt Color(0);
  VOptParamFloat Scale(1.0f);
  vobjGetParam(x, y, sprite, frame, rot, TranslStart, TranslEnd, Color, Scale);
  #ifdef CLIENT
  R_DrawSpritePatch(x, y, sprite, frame, rot, TranslStart, TranslEnd, Color, Scale);
  #endif
}


IMPLEMENT_FREE_FUNCTION(VObject, InstallModel) {
  VStr name;
  vobjGetParam(name);
  #ifdef CLIENT
  if (FL_FileExists(name)) {
    RET_PTR(Mod_FindName(name));
  } else {
    RET_PTR(nullptr);
  }
  #else
  RET_PTR(nullptr);
  #endif
}


IMPLEMENT_FREE_FUNCTION(VObject, R_DrawModelFrame) {
  TVec origin;
  float angle;
  VModel *model;
  int nextframe;
  int frame;
  VStr skin;
  int TranslStart;
  int TranslEnd;
  int Color;
  vobjGetParam(origin, angle, model, frame, nextframe, skin, TranslStart, TranslEnd, Color);
  #ifdef CLIENT
  R_DrawModelFrame(origin, angle, model, frame, nextframe, *skin, TranslStart, TranslEnd, Color, 0);
  #endif
}


IMPLEMENT_FREE_FUNCTION(VObject, R_FillRect) {
  float x, y, width, height;
  vuint32 color;
  VOptParamFloat alpha(1.0f);
  vobjGetParam(x, y, width, height, color, alpha);
  #ifdef CLIENT
  if (Drawer) Drawer->FillRect(x*fScaleX, y*fScaleY, (x+width)*fScaleX, (y+height)*fScaleY, color, alpha);
  #endif
}


IMPLEMENT_FREE_FUNCTION(VObject, R_ShadeRect) {
  float x, y, width, height;
  float darken;
  vobjGetParam(x, y, width, height, darken);
  #ifdef CLIENT
  if (Drawer) Drawer->ShadeRect(x*fScaleX, y*fScaleY, (x+width)*fScaleX, (y+height)*fScaleY, darken);
  #endif
}


IMPLEMENT_FREE_FUNCTION(VObject, R_DrawRect) {
  float x, y, width, height;
  vuint32 color;
  VOptParamFloat alpha(1.0f);
  vobjGetParam(x, y, width, height, color, alpha);
  #ifdef CLIENT
  if (Drawer) Drawer->DrawRect(x*fScaleX, y*fScaleY, (x+width)*fScaleX, (y+height)*fScaleY, color, alpha);
  #endif
}


IMPLEMENT_FREE_FUNCTION(VObject, R_DrawLine) {
  float x1, y1, x2, y2;
  vuint32 color;
  VOptParamFloat alpha(1.0f);
  vobjGetParam(x1, y1, x2, y2, color, alpha);
  #ifdef CLIENT
  if (Drawer) Drawer->DrawLine(x1*fScaleX, y1*fScaleY, x2*fScaleX, y2*fScaleY, color, alpha);
  #endif
}


IMPLEMENT_FREE_FUNCTION(VObject, R_GetAspectRatio) {
  #ifdef CLIENT
  RET_FLOAT(R_GetAspectRatio());
  #else
  RET_FLOAT(1.2f);
  #endif
}

IMPLEMENT_FREE_FUNCTION(VObject, R_GetAspectRatioValue) {
  #ifdef CLIENT
  RET_FLOAT(R_GetAspectRatioValue());
  #else
  RET_FLOAT(1.2f);
  #endif
}

// native static final int R_GetAspectRatioCount ();
IMPLEMENT_FREE_FUNCTION(VObject, R_GetAspectRatioCount) {
  RET_INT(CVC_CALC_EXPR(R_GetAspectRatioCount()));
}

// native static final int R_GetAspectRatioHoriz (int idx);
IMPLEMENT_FREE_FUNCTION(VObject, R_GetAspectRatioHoriz) {
  int idx;
  vobjGetParam(idx);
  RET_INT(CVC_CALC_EXPR(R_GetAspectRatioHoriz(idx)));
}

// native static final int R_GetAspectRatioVert (int idx);
IMPLEMENT_FREE_FUNCTION(VObject, R_GetAspectRatioVert) {
  int idx;
  vobjGetParam(idx);
  RET_INT(CVC_CALC_EXPR(R_GetAspectRatioVert(idx)));
}

// native static final string R_GetAspectRatioDsc (int idx);
IMPLEMENT_FREE_FUNCTION(VObject, R_GetAspectRatioDsc) {
  int idx;
  vobjGetParam(idx);
  #ifdef CLIENT
  RET_STR(VStr(R_GetAspectRatioDsc(idx)));
  #else
  RET_STR(VStr("dummy"));
  #endif
}


IMPLEMENT_FREE_FUNCTION(VObject, R_SupportsShadowVolumeRendering) {
  #ifdef CLIENT
  if (Drawer) {
    RET_BOOL(Drawer->SupportsShadowVolumeRendering());
  } else {
    // be conservative
    RET_BOOL(false);
  }
  #else
  RET_BOOL(false);
  #endif
}


//**************************************************************************
//
//  Client side sound
//
//**************************************************************************

IMPLEMENT_FREE_FUNCTION(VObject, LocalSound) {
  VName name;
  vobjGetParam(name);
  #ifdef CLIENT
  GAudio->PlaySound(GSoundManager->GetSoundID(name), TVec(0, 0, 0), TVec(0, 0, 0), 0, 0, 1, 0, false);
  #endif
}

IMPLEMENT_FREE_FUNCTION(VObject, IsLocalSoundPlaying) {
  VName name;
  vobjGetParam(name);
  #ifdef CLIENT
  RET_BOOL(GAudio->IsSoundPlaying(0, GSoundManager->GetSoundID(name)));
  #else
  RET_BOOL(false);
  #endif
}

IMPLEMENT_FREE_FUNCTION(VObject, StopLocalSounds) {
  #ifdef CLIENT
  GAudio->StopSound(0, 0);
  #endif
}


//==========================================================================
//
//  TranslateKey
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, TranslateKey) {
  int ch;
  vobjGetParam(ch);
  #ifdef CLIENT
  if (!GInput) { RET_STR(VStr::EmptyString); return; }
  ch = GInput->TranslateKey(ch);
  //FIXME: i18n
  if (ch < 1 || ch > 127) { RET_STR(VStr::EmptyString); return; }
  RET_STR(VStr((char)ch));
  #else
  if (ch < 1 || ch > 127) { RET_STR(VStr::EmptyString); return; }
  RET_STR(VStr((char)ch));
  #endif
}


IMPLEMENT_FREE_FUNCTION(VObject, KeyNameForNum) {
  int keynum;
  vobjGetParam(keynum);
  #ifdef CLIENT
  RET_STR(GInput->KeyNameForNum(keynum));
  #else
  RET_STR(VStr::EmptyString);
  #endif
}


IMPLEMENT_FREE_FUNCTION(VObject, IN_GetBindingKeys) {
  VStr name;
  int *key1;
  int *key2;
  int strifemode;
  VStr modsection;
  int *isActive;
  vobjGetParam(name, key1, key2, strifemode, modsection, isActive);
  #ifdef CLIENT
  GInput->GetBindingKeys(name, *key1, *key2, modsection, strifemode, isActive);
  #else
  if (key1) *key1 = 0;
  if (key2) *key2 = 0;
  if (isActive) *isActive = false;
  #endif
}


//native static final void IN_GetDefaultModBindingKeys (string cmd, int *key1, int *key2, string modSection);
IMPLEMENT_FREE_FUNCTION(VObject, IN_GetDefaultModBindingKeys) {
  VStr name;
  int *key1;
  int *key2;
  VStr modsection;
  vobjGetParam(name, key1, key2, modsection);
  #ifdef CLIENT
  GInput->GetDefaultModBindingKeys(name, *key1, *key2, modsection);
  #else
  if (key1) *key1 = 0;
  if (key2) *key2 = 0;
  #endif
}


IMPLEMENT_FREE_FUNCTION(VObject, IN_SetBinding) {
  int keynum;
  VStr ondown;
  VStr onup;
  int strifemode;
  VStr modsection;
  vobjGetParam(keynum, ondown, onup, strifemode, modsection);
  #ifdef CLIENT
  GInput->SetBinding(keynum, ondown, onup, modsection, strifemode);
  #endif
}


IMPLEMENT_FREE_FUNCTION(VObject, GetSlist) {
  RET_PTR(GNet->GetSlist());
}


IMPLEMENT_FREE_FUNCTION(VObject, StartSearch) {
  bool Master;
  vobjGetParam(Master);
  GNet->StartSearch(Master);
}


//native static bool IsLineTagEqual (const line_t *line, int tag);
IMPLEMENT_FREE_FUNCTION(VObject, IsLineTagEqual) {
  line_t *line;
  int tag;
  vobjGetParam(line, tag);
  if (!line) { RET_BOOL(false); return; }
  RET_BOOL(line->IsTagEqual(tag));
}


//native static bool IsSectorTagEqual (const sector_t *sector, int tag);
IMPLEMENT_FREE_FUNCTION(VObject, IsSectorTagEqual) {
  sector_t *sector;
  int tag;
  vobjGetParam(sector, tag);
  if (!sector) { RET_BOOL(false); return; }
  RET_BOOL(sector->IsTagEqual(tag));
}
