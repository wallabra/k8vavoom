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
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
# include "gamedefs.h"
# include "net/network.h"
# include "sv_local.h"
# include "cl_local.h"
# include "drawer.h"
#else
# if defined(IN_VCC)
#  include "../utils/vcc/vcc.h"
# elif defined(VCC_STANDALONE_EXECUTOR)
#  include "../vccrun/vcc_run_vc.h"
#  include "vc/vc_public.h"
# endif
#endif

VClass *SV_FindClassFromEditorId (int Id, int GameFilter);
VClass *SV_FindClassFromScriptId (int Id, int GameFilter);


#ifndef VCC_STANDALONE_EXECUTOR

#ifdef SERVER

//**************************************************************************
//
//  Map utilites
//
//**************************************************************************

//==========================================================================
//
//  P_SectorClosestPoint
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, SectorClosestPoint) {
  P_GET_VEC(point);
  P_GET_PTR(sector_t, sec);
  RET_VEC(P_SectorClosestPoint(sec, point));
}


//==========================================================================
//
//  LineOpenings
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, LineOpenings) {
  P_GET_BOOL_OPT(do3dmidtex, false);
  P_GET_INT_OPT(blockmask, SPF_NOBLOCKING);
  P_GET_PTR(TVec, point);
  P_GET_PTR(line_t, linedef);
  RET_PTR(SV_LineOpenings(linedef, *point, blockmask, do3dmidtex));
}


//==========================================================================
//
//  P_GetMidTexturePosition
//
// native static final bool P_GetMidTexturePosition (const line_t *line, int sideno, out float ptextop, out float ptexbot);
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, P_GetMidTexturePosition) {
  P_GET_PTR(float, ptexbot);
  P_GET_PTR(float, ptextop);
  P_GET_INT(sideno);
  P_GET_PTR(line_t, ld);
  RET_BOOL(P_GetMidTexturePosition(ld, sideno, ptextop, ptexbot));
}


//==========================================================================
//
//  FindThingGap
//
//==========================================================================
/*
IMPLEMENT_FREE_FUNCTION(VObject, FindThingGap) {
  P_GET_FLOAT(height);
  P_GET_PTR(TVec, point);
  P_GET_PTR(sector_t, sector);
  if (sector) {
    RET_PTR(SV_FindThingGap(sector, *point, height));
  } else {
    RET_PTR(nullptr);
  }
}
*/


//==========================================================================
//
//  FindOpening
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, FindOpening) {
  P_GET_FLOAT(z2);
  P_GET_FLOAT(z1);
  P_GET_PTR(opening_t, gaps);
  RET_PTR(SV_FindOpening(gaps, z1, z2));
}


//==========================================================================
//
//  PointInRegion
//
//==========================================================================
/*
IMPLEMENT_FREE_FUNCTION(VObject, PointInRegion) {
  P_GET_VEC(p);
  P_GET_PTR(sector_t, sector);
  RET_PTR(SV_PointInRegion(sector, p));
}
*/


//native static final void GetSectorGapCoords (const GameObject::sector_t *sector, const ref TVec point, out float floorz, out float ceilz);
IMPLEMENT_FREE_FUNCTION(VObject, GetSectorGapCoords) {
  P_GET_PTR(float, ceilz);
  P_GET_PTR(float, floorz);
  P_GET_PTR(TVec, point);
  P_GET_PTR(sector_t, sector);
  SV_GetSectorGapCoords(sector, *point, *floorz, *ceilz);
}


//**************************************************************************
//
//  Sound functions
//
//**************************************************************************

//==========================================================================
//
//  GetSoundPlayingInfo
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, GetSoundPlayingInfo) {
  P_GET_INT(id);
  P_GET_REF(VEntity, mobj);
#ifdef CLIENT
  RET_BOOL(GAudio->IsSoundPlaying(mobj->SoundOriginID, id));
#else
  (void)id;
  (void)mobj;
  RET_BOOL(false);
#endif
}


//==========================================================================
//
//  GetSoundID
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, GetSoundID) {
  P_GET_NAME(Name);
  RET_INT(GSoundManager->GetSoundID(Name));
}


//==========================================================================
//
//  StopAllSounds
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, StopAllSounds) {
#ifdef CLIENT
  GAudio->StopAllSound();
#endif
}


//==========================================================================
//
//  SetSeqTrans
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, SetSeqTrans) {
  P_GET_INT(SeqType);
  P_GET_INT(Num);
  P_GET_NAME(Name);
  GSoundManager->SetSeqTrans(Name, Num, SeqType);
}


//==========================================================================
//
//  GetSeqTrans
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, GetSeqTrans) {
  P_GET_INT(SeqType);
  P_GET_INT(Num);
  RET_NAME(GSoundManager->GetSeqTrans(Num, SeqType));
}


//==========================================================================
//
//  GetSeqTrans
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, GetSeqSlot) {
  P_GET_NAME(Name);
  RET_NAME(GSoundManager->GetSeqSlot(Name));
}


//==========================================================================
//
//  P_GetThingFloorType
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, TerrainType) {
  P_GET_INT(pic);
  RET_PTR(SV_TerrainType(pic));
}


//==========================================================================
//
//  SB_Start
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, SB_Start) {
#ifdef CLIENT
  SB_Start();
#endif
}


//==========================================================================
//
//  FindClassFromEditorId
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, FindClassFromEditorId) {
  P_GET_INT(GameFilter);
  P_GET_INT(Id);
  RET_PTR(SV_FindClassFromEditorId(Id, GameFilter));
}


//==========================================================================
//
//  FindClassFromScriptId
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, FindClassFromScriptId) {
  P_GET_INT(GameFilter);
  P_GET_INT(Id);
  RET_PTR(SV_FindClassFromScriptId(Id, GameFilter));
}


//==========================================================================
//
//  various
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, P_GetMapName) {
  P_GET_INT(map);
  RET_STR(P_GetMapName(map));
}


IMPLEMENT_FREE_FUNCTION(VObject, P_GetMapIndexByLevelNum) {
  P_GET_INT(map);
  RET_INT(P_GetMapIndexByLevelNum(map));
}


IMPLEMENT_FREE_FUNCTION(VObject, P_GetNumMaps) {
  RET_INT(P_GetNumMaps());
}


IMPLEMENT_FREE_FUNCTION(VObject, P_GetMapInfo) {
  P_GET_INT(map);
  RET_PTR(P_GetMapInfoPtr(map));
}


IMPLEMENT_FREE_FUNCTION(VObject, P_GetMapLumpName) {
  P_GET_INT(map);
  RET_NAME(P_GetMapLumpName(map));
}


IMPLEMENT_FREE_FUNCTION(VObject, P_TranslateMap) {
  P_GET_INT(map);
  RET_NAME(P_TranslateMap(map));
}


IMPLEMENT_FREE_FUNCTION(VObject, P_GetNumEpisodes) {
  RET_INT(P_GetNumEpisodes());
}


IMPLEMENT_FREE_FUNCTION(VObject, P_GetEpisodeDef) {
  P_GET_INT(Index);
  RET_PTR(P_GetEpisodeDef(Index));
}


IMPLEMENT_FREE_FUNCTION(VObject, P_GetNumSkills) {
  RET_INT(P_GetNumSkills());
}


IMPLEMENT_FREE_FUNCTION(VObject, P_GetSkillDef) {
  P_GET_INT(Index);
  RET_PTR(const_cast<VSkillDef*>(P_GetSkillDef(Index)));
}


IMPLEMENT_FREE_FUNCTION(VObject, SV_GetSaveString) {
  P_GET_PTR(VStr, buf);
  P_GET_INT(i);
  if (!buf) { RET_INT(0); return; }
#ifdef SERVER
  RET_INT(SV_GetSaveString(i, *buf));
#else
  RET_INT(0);
#endif
}


IMPLEMENT_FREE_FUNCTION(VObject, SV_GetSaveDateString) {
  P_GET_PTR(VStr, buf);
  P_GET_INT(i);
  if (!buf) return;
#ifdef SERVER
  SV_GetSaveDateString(i, *buf);
#else
  *buf = VStr("UNKNOWN");
#endif
}


IMPLEMENT_FREE_FUNCTION(VObject, LoadTextLump) {
  P_GET_NAME(name);
  RET_STR(W_LoadTextLump(name));
}


#endif // SERVER


#ifdef CLIENT

#include "neoui/vc_object_neoui.cpp"

//**************************************************************************
//
//  Graphics
//
//**************************************************************************

//==========================================================================
//
//  SetVirtualScreen
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, SetVirtualScreen) {
  P_GET_INT(Height);
  P_GET_INT(Width);
  SCR_SetVirtualScreen(Width, Height);
}

IMPLEMENT_FREE_FUNCTION(VObject, GetVirtualWidth) { RET_INT(VirtualWidth); }
IMPLEMENT_FREE_FUNCTION(VObject, GetVirtualHeight) { RET_INT(VirtualHeight); }

IMPLEMENT_FREE_FUNCTION(VObject, GetRealScreenWidth) { RET_INT(ScreenWidth); }
IMPLEMENT_FREE_FUNCTION(VObject, GetRealScreenHeight) { RET_INT(ScreenHeight); }


//==========================================================================
//
//  R_RegisterPic
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, R_RegisterPic) {
  P_GET_NAME(name);
  RET_INT(GTextureManager.AddPatch(name, TEXTYPE_Pic));
}


//==========================================================================
//
//  R_RegisterPicPal
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, R_RegisterPicPal) {
  P_GET_NAME(palname);
  P_GET_NAME(name);
  RET_INT(GTextureManager.AddRawWithPal(name, palname));
}


//==========================================================================
//
//  R_RegisterPicPath
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, R_RegisterPicPath) {
  P_GET_NAME(path);
  RET_INT(GTextureManager.AddFileTextureChecked(path, TEXTYPE_Pic));
}


//==========================================================================
//
//  R_GetPicInfo
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, R_GetPicInfo) {
  P_GET_PTR(picinfo_t, info);
  P_GET_INT(handle);
  GTextureManager.GetTextureInfo(handle, info);
}


//==========================================================================
//
//  R_DrawPic
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, R_DrawPic) {
  P_GET_FLOAT_OPT(alpha, 1.0f);
  P_GET_INT(handle);
  P_GET_INT(y);
  P_GET_INT(x);
  R_DrawPic(x, y, handle, alpha);
}


//==========================================================================
//
//  R_DrawPicFloat
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, R_DrawPicFloat) {
  P_GET_FLOAT_OPT(alpha, 1.0f);
  P_GET_INT(handle);
  P_GET_FLOAT(y);
  P_GET_FLOAT(x);
  R_DrawPicFloat(x, y, handle, alpha);
}


//==========================================================================
//
//  R_DrawPicPart
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, R_DrawPicPart) {
  P_GET_FLOAT_OPT(alpha, 1.0f);
  P_GET_INT(handle);
  P_GET_FLOAT(phgt);
  P_GET_FLOAT(pwdt);
  P_GET_INT(y);
  P_GET_INT(x);
  R_DrawPicPart(x, y, pwdt, phgt, handle, alpha);
}


//==========================================================================
//
//  R_DrawPicFloatPart
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, R_DrawPicFloatPart) {
  P_GET_FLOAT_OPT(alpha, 1.0f);
  P_GET_INT(handle);
  P_GET_FLOAT(phgt);
  P_GET_FLOAT(pwdt);
  P_GET_FLOAT(y);
  P_GET_FLOAT(x);
  R_DrawPicFloatPart(x, y, pwdt, phgt, handle, alpha);
}


//==========================================================================
//
//  R_DrawPicPartEx
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, R_DrawPicPartEx) {
  P_GET_FLOAT_OPT(alpha, 1.0f);
  P_GET_INT(handle);
  P_GET_FLOAT(ty1);
  P_GET_FLOAT(tx1);
  P_GET_FLOAT(ty0);
  P_GET_FLOAT(tx0);
  P_GET_INT(y);
  P_GET_INT(x);
  R_DrawPicPartEx(x, y, tx0, ty0, tx1, ty1, handle, alpha);
}


//==========================================================================
//
//  R_DrawPicFloatPartEx
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, R_DrawPicFloatPartEx) {
  P_GET_FLOAT_OPT(alpha, 1.0f);
  P_GET_INT(handle);
  P_GET_FLOAT(ty1);
  P_GET_FLOAT(tx1);
  P_GET_FLOAT(ty0);
  P_GET_FLOAT(tx0);
  P_GET_FLOAT(y);
  P_GET_FLOAT(x);
  R_DrawPicFloatPartEx(x, y, tx0, ty0, tx1, ty1, handle, alpha);
}


//==========================================================================
//
//  R_InstallSprite
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, R_InstallSprite) {
  P_GET_INT(index);
  P_GET_STR(name);
  R_InstallSprite(*name, index);
}


//==========================================================================
//
//  R_InstallSpriteComplete
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, R_InstallSpriteComplete) {
  R_InstallSpriteComplete();
}


//==========================================================================
//
//  R_DrawSpritePatch
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, R_DrawSpritePatch) {
  P_GET_FLOAT_OPT(Scale, 1.0f);
  P_GET_INT_OPT(Color, 0);
  P_GET_INT_OPT(TranslEnd, 0);
  P_GET_INT_OPT(TranslStart, 0);
  P_GET_INT_OPT(rot, 0);
  P_GET_INT_OPT(frame, 0);
  P_GET_INT(sprite);
  P_GET_FLOAT(y);
  P_GET_FLOAT(x);
  R_DrawSpritePatch(x, y, sprite, frame, rot, TranslStart, TranslEnd, Color, Scale);
}


//==========================================================================
//
//  InstallModel
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, InstallModel) {
  P_GET_STR(name);
  if (FL_FileExists(name)) {
    RET_PTR(Mod_FindName(name));
  } else {
    RET_PTR(0);
  }
}


//==========================================================================
//
//  R_DrawModelFrame
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, R_DrawModelFrame) {
  P_GET_INT(Color);
  P_GET_INT(TranslEnd);
  P_GET_INT(TranslStart);
  P_GET_STR(skin);
  P_GET_INT(frame);
  P_GET_INT(nextframe);
  P_GET_PTR(VModel, model);
  P_GET_FLOAT(angle);
  P_GET_VEC(origin);
  R_DrawModelFrame(origin, angle, model, frame, nextframe, *skin, TranslStart, TranslEnd, Color, 0);
}


//==========================================================================
//
//  R_FillRect
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, R_FillRect) {
  P_GET_FLOAT_OPT(alpha, 1.0f);
  P_GET_INT(color);
  P_GET_FLOAT(height);
  P_GET_FLOAT(width);
  P_GET_FLOAT(y);
  P_GET_FLOAT(x);
  if (Drawer) Drawer->FillRect(x*fScaleX, y*fScaleY, (x+width)*fScaleX, (y+height)*fScaleY, color, alpha);
}


//==========================================================================
//
//  R_ShadeRect
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, R_ShadeRect) {
  float x, y, width, height;
  float darken;
  vobjGetParam(x, y, width, height, darken);
  if (Drawer) Drawer->ShadeRect(x*fScaleX, y*fScaleY, (x+width)*fScaleX, (y+height)*fScaleY, darken);
}


//==========================================================================
//
//  R_DrawRect
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, R_DrawRect) {
  float x, y, width, height;
  vuint32 color;
  VOptParamFloat alpha(1.0f);
  vobjGetParam(x, y, width, height, color, alpha);
  if (Drawer) Drawer->DrawRect(x*fScaleX, y*fScaleY, (x+width)*fScaleX, (y+height)*fScaleY, color, alpha);
}


//==========================================================================
//
//  R_DrawLine
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, R_DrawLine) {
  float x1, y1, x2, y2;
  vuint32 color;
  VOptParamFloat alpha(1.0f);
  vobjGetParam(x1, y1, x2, y2, color, alpha);
  if (Drawer) Drawer->DrawLine(x1*fScaleX, y1*fScaleY, x2*fScaleX, y2*fScaleY, color, alpha);
}


//==========================================================================
//
//  R_GetAspectRatio
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, R_GetAspectRatio) {
  RET_FLOAT(R_GetAspectRatio());
}


//==========================================================================
//
//  R_SupportsShadowVolumeRendering
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, R_SupportsShadowVolumeRendering) {
  if (Drawer) {
    RET_BOOL(Drawer->SupportsShadowVolumeRendering());
  } else {
    // be conservative
    RET_BOOL(false);
  }
}


//**************************************************************************
//
//  Client side sound
//
//**************************************************************************

//==========================================================================
//
//  LocalSound
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, LocalSound) {
  P_GET_NAME(name);
  GAudio->PlaySound(GSoundManager->GetSoundID(name), TVec(0, 0, 0), TVec(0, 0, 0), 0, 0, 1, 0, false);
}


//==========================================================================
//
//  IsLocalSoundPlaying
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, IsLocalSoundPlaying) {
  P_GET_NAME(name);
  RET_BOOL(GAudio->IsSoundPlaying(0, GSoundManager->GetSoundID(name)));
}


//==========================================================================
//
//  StopLocalSounds
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, StopLocalSounds) {
  GAudio->StopSound(0, 0);
}


//==========================================================================
//
//  TranslateKey
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, TranslateKey) {
  P_GET_INT(ch);
  if (!GInput) { RET_STR(VStr::EmptyString); return; }
  ch = GInput->TranslateKey(ch);
  //FIXME: i18n
  if (ch < 1 || ch > 127) { RET_STR(VStr::EmptyString); return; }
  RET_STR(VStr((char)ch));
}


IMPLEMENT_FREE_FUNCTION(VObject, KeyNameForNum) {
  P_GET_INT(keynum);
  RET_STR(GInput->KeyNameForNum(keynum));
}


IMPLEMENT_FREE_FUNCTION(VObject, IN_GetBindingKeys) {
  P_GET_PTR(int, isActive);
  P_GET_STR(modsection);
  P_GET_INT(strifemode);
  P_GET_PTR(int, key2);
  P_GET_PTR(int, key1);
  P_GET_STR(name);
  GInput->GetBindingKeys(name, *key1, *key2, modsection, strifemode, isActive);
}


//native static final void IN_GetDefaultModBindingKeys (string cmd, int *key1, int *key2, string modSection);
IMPLEMENT_FREE_FUNCTION(VObject, IN_GetDefaultModBindingKeys) {
  P_GET_STR(modsection);
  P_GET_PTR(int, key2);
  P_GET_PTR(int, key1);
  P_GET_STR(name);
  GInput->GetDefaultModBindingKeys(name, *key1, *key2, modsection);
}


IMPLEMENT_FREE_FUNCTION(VObject, IN_SetBinding) {
  P_GET_STR(modsection);
  P_GET_INT(strifemode);
  P_GET_STR(onup);
  P_GET_STR(ondown);
  P_GET_INT(keynum);
  GInput->SetBinding(keynum, ondown, onup, modsection, strifemode);
}


IMPLEMENT_FREE_FUNCTION(VObject, GetSlist) {
  RET_PTR(GNet->GetSlist());
}


IMPLEMENT_FREE_FUNCTION(VObject, StartSearch) {
  P_GET_BOOL(Master);
  GNet->StartSearch(Master);
}

#endif // CLIENT

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

#endif // !VCC_STANDALONE_EXECUTOR
