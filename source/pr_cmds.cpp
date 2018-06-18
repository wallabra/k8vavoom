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
//**
//**  Builtins.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
# include "gamedefs.h"
# include "net/network.h"
# include "sv_local.h"
# include "cl_local.h"
# include "sound/snd_local.h"
# include "drawer.h"
#else
# if defined(IN_VCC)
#  include "../utils/vcc/vcc.h"
# elif defined(VCC_STANDALONE_EXECUTOR)
#  include "../vccrun/vcc_run.h"
# endif
#endif

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

VClass *SV_FindClassFromEditorId (int Id, int GameFilter);
VClass *SV_FindClassFromScriptId (int Id, int GameFilter);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//**************************************************************************
//
//  Vararg strings
//
//**************************************************************************

VStr PF_FormatString () {
  guard(PF_FormatString);

  const int MAX_PARAMS = 64;

  VStr Ret;
  VStack params[MAX_PARAMS];
  int pi;

  P_GET_INT(count);
  if (count > MAX_PARAMS) { VObject::VMDumpCallStack(); Sys_Error("Too many arguments to string formatting function"); }
  for (pi = count-1; pi >= 0; --pi) params[pi] = *(--pr_stackPtr);
  P_GET_STR(str);

  const char *src = *str;
  pi = 0;
  while (*src) {
    if (*src == '%') {
      ++src;
      switch (*src) {
        case '%':
          Ret += *src;
          break;
        case 'i':
        case 'd':
          if (pi >= count) { VObject::VMDumpCallStack(); Sys_Error("Out of arguments to string formatting function"); }
          Ret += va("%d", params[pi].i);
          ++pi;
          break;
        case 'x':
          if (pi >= count) { VObject::VMDumpCallStack(); Sys_Error("Out of arguments to string formatting function"); }
          Ret += va("%x", params[pi].i);
          ++pi;
          break;
        case 'f':
          if (pi >= count) { VObject::VMDumpCallStack(); Sys_Error("Out of arguments to string formatting function"); }
          Ret += va("%f", params[pi].f);
          ++pi;
          break;
        case 'n':
          if (pi >= count) { VObject::VMDumpCallStack(); Sys_Error("Out of arguments to string formatting function"); }
          Ret += *(VName*)&params[pi].i;
          ++pi;
          break;
        case 's':
          if (pi >= count) { VObject::VMDumpCallStack(); Sys_Error("Out of arguments to string formatting function"); }
          Ret += *(VStr*)&params[pi].p;
          ((VStr*)&params[pi].p)->Clean();
          ++pi;
          break;
        case 'p':
          if (pi >= count) { VObject::VMDumpCallStack(); Sys_Error("Out of arguments to string formatting function"); }
          Ret += va("%p", params[pi].p);
          ++pi;
          break;
        case 'v':
          if (count-pi < 3) { VObject::VMDumpCallStack(); Sys_Error("Out of arguments to string formatting function"); }
          Ret += va("(%f,%f,%f)", params[pi].f, params[pi+1].f, params[pi+2].f);
          pi += 3;
          break;
        case 'B': // boolean
          if (pi >= count) { VObject::VMDumpCallStack(); Sys_Error("Out of arguments to string formatting function"); }
          Ret += (params[pi].i ? "true" : "false");
          ++pi;
          break;

        default:
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
          GCon->Logf(NAME_Dev, "PF_FormatString: Unknown format identifier '%c'", *src);
#else
          fprintf(stderr, "PF_FormatString: Unknown format identifier '%c'\n", *src);
#endif
          --src;
          Ret += *src;
          break;
      }
    } else {
      Ret += *src;
    }
    ++src;
  }

#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  if (pi < count) GCon->Log(NAME_Dev, "PF_FormatString: Not all params were used");
  if (pi > count) GCon->Log(NAME_Dev, "PF_FormatString: Param count overflow");
#else
  if (pi < count) fprintf(stderr, "PF_FormatString: Not all params were used\n");
  if (pi > count) fprintf(stderr, "PF_FormatString: Param count overflow\n");
#endif

  return Ret;
  unguard;
}

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
IMPLEMENT_FUNCTION(VObject, SectorClosestPoint) {
  P_GET_VEC(point);
  P_GET_PTR(sector_t, sec);
  RET_VEC(P_SectorClosestPoint(sec, point));
}


//==========================================================================
//
//  PF_LineOpenings
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, LineOpenings) {
  P_GET_VEC(point);
  P_GET_PTR(line_t, linedef);
  RET_PTR(SV_LineOpenings(linedef, point, 0xffffffff));
}


//==========================================================================
//
//  PF_P_BoxOnLineSide
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, P_BoxOnLineSide) {
  P_GET_PTR(line_t, ld);
  P_GET_PTR(float, tmbox);
  RET_INT(P_BoxOnLineSide(tmbox, ld));
}


//==========================================================================
//
//  PF_FindThingGap
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, FindThingGap) {
  P_GET_FLOAT(z2);
  P_GET_FLOAT(z1);
  P_GET_VEC(point);
  P_GET_PTR(sec_region_t, gaps);
  RET_PTR(SV_FindThingGap(gaps, point, z1, z2));
}


//==========================================================================
//
//  PF_FindOpening
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, FindOpening) {
  P_GET_FLOAT(z2);
  P_GET_FLOAT(z1);
  P_GET_PTR(opening_t, gaps);
  RET_PTR(SV_FindOpening(gaps, z1, z2));
}


//==========================================================================
//
//  PF_PointInRegion
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, PointInRegion) {
  P_GET_VEC(p);
  P_GET_PTR(sector_t, sector);
  RET_PTR(SV_PointInRegion(sector, p));
}


//**************************************************************************
//
//  Sound functions
//
//**************************************************************************

//==========================================================================
//
//  PF_GetSoundPlayingInfo
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, GetSoundPlayingInfo) {
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
//  PF_GetSoundID
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, GetSoundID) {
  P_GET_NAME(Name);
  RET_INT(GSoundManager->GetSoundID(Name));
}


//==========================================================================
//
//  PF_StopAllSounds
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, StopAllSounds) {
#ifdef CLIENT
  GAudio->StopAllSound();
#endif
}


//==========================================================================
//
//  PF_SetSeqTrans
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, SetSeqTrans) {
  P_GET_INT(SeqType);
  P_GET_INT(Num);
  P_GET_NAME(Name);
  GSoundManager->SetSeqTrans(Name, Num, SeqType);
}


//==========================================================================
//
//  PF_GetSeqTrans
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, GetSeqTrans) {
  P_GET_INT(SeqType);
  P_GET_INT(Num);
  RET_NAME(GSoundManager->GetSeqTrans(Num, SeqType));
}


//==========================================================================
//
//  PF_GetSeqTrans
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, GetSeqSlot) {
  P_GET_NAME(Name);
  RET_NAME(GSoundManager->GetSeqSlot(Name));
}


//==========================================================================
//
//  PF_P_GetThingFloorType
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, TerrainType) {
  P_GET_INT(pic);
  RET_PTR(SV_TerrainType(pic));
}


//==========================================================================
//
//  PF_SB_Start
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, SB_Start) {
#ifdef CLIENT
  SB_Start();
#endif
}


//==========================================================================
//
//  PF_FindClassFromEditorId
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, FindClassFromEditorId) {
  P_GET_INT(GameFilter);
  P_GET_INT(Id);
  RET_PTR(SV_FindClassFromEditorId(Id, GameFilter));
}


//==========================================================================
//
//  PF_FindClassFromScriptId
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, FindClassFromScriptId) {
  P_GET_INT(GameFilter);
  P_GET_INT(Id);
  RET_PTR(SV_FindClassFromScriptId(Id, GameFilter));
}

#endif // SERVER


#ifdef CLIENT

//**************************************************************************
//
//  Graphics
//
//**************************************************************************

//==========================================================================
//
//  PF_SetVirtualScreen
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, SetVirtualScreen) {
  P_GET_INT(Height);
  P_GET_INT(Width);
  SCR_SetVirtualScreen(Width, Height);
}


//==========================================================================
//
//  PF_R_RegisterPic
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, R_RegisterPic) {
  P_GET_NAME(name);
  RET_INT(GTextureManager.AddPatch(name, TEXTYPE_Pic));
}


//==========================================================================
//
//  PF_R_RegisterPicPal
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, R_RegisterPicPal) {
  P_GET_NAME(palname);
  P_GET_NAME(name);
  RET_INT(GTextureManager.AddRawWithPal(name, palname));
}


//==========================================================================
//
//  PF_R_GetPicInfo
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, R_GetPicInfo) {
  P_GET_PTR(picinfo_t, info);
  P_GET_INT(handle);
  GTextureManager.GetTextureInfo(handle, info);
}


//==========================================================================
//
//  PF_R_DrawPic
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, R_DrawPic) {
  P_GET_INT(handle);
  P_GET_INT(y);
  P_GET_INT(x);
  R_DrawPic(x, y, handle);
}


//==========================================================================
//
//  PF_R_InstallSprite
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, R_InstallSprite) {
  P_GET_INT(index);
  P_GET_STR(name);
  R_InstallSprite(*name, index);
}


//==========================================================================
//
//  PF_R_DrawSpritePatch
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, R_DrawSpritePatch) {
  P_GET_INT(Colour);
  P_GET_INT(TranslEnd);
  P_GET_INT(TranslStart);
  P_GET_INT(rot);
  P_GET_INT(frame);
  P_GET_INT(sprite);
  P_GET_INT(y);
  P_GET_INT(x);
  R_DrawSpritePatch(x, y, sprite, frame, rot, TranslStart, TranslEnd, Colour);
}


//==========================================================================
//
//  PF_InstallModel
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, InstallModel) {
  P_GET_STR(name);
  if (FL_FileExists(name)) {
    RET_PTR(Mod_FindName(name));
  } else {
    RET_PTR(0);
  }
}


//==========================================================================
//
//  PF_R_DrawModelFrame
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, R_DrawModelFrame) {
  P_GET_INT(Colour);
  P_GET_INT(TranslEnd);
  P_GET_INT(TranslStart);
  P_GET_STR(skin);
  P_GET_INT(frame);
  P_GET_INT(nextframe);
  P_GET_PTR(VModel, model);
  P_GET_FLOAT(angle);
  P_GET_VEC(origin);
  R_DrawModelFrame(origin, angle, model, frame, nextframe, *skin, TranslStart, TranslEnd, Colour, 0);
}


//==========================================================================
//
//  PF_R_FillRect
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, R_FillRect) {
  P_GET_INT(coulor);
  P_GET_INT(height);
  P_GET_INT(width);
  P_GET_INT(y);
  P_GET_INT(x);
  Drawer->FillRect(x*fScaleX, y*fScaleY, (x+width)*fScaleX, (y+height)*fScaleY, coulor);
}


//**************************************************************************
//
//  Client side sound
//
//**************************************************************************

//==========================================================================
//
//  PF_LocalSound
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, LocalSound) {
  P_GET_NAME(name);
  GAudio->PlaySound(GSoundManager->GetSoundID(name), TVec(0, 0, 0), TVec(0, 0, 0), 0, 0, 1, 0, false);
}


//==========================================================================
//
//  PF_IsLocalSoundPlaying
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, IsLocalSoundPlaying) {
  P_GET_NAME(name);
  RET_BOOL(GAudio->IsSoundPlaying(0, GSoundManager->GetSoundID(name)));
}


//==========================================================================
//
//  PF_StopLocalSounds
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, StopLocalSounds) {
  GAudio->StopSound(0, 0);
}


//==========================================================================
//
//  PF_InputLine_SetValue
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, TranslateKey) {
  P_GET_INT(ch);
  RET_STR(VStr((char)GInput->TranslateKey(ch)));
}


//==========================================================================
//
//  Temporary menu stuff
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, P_GetMapName) {
  P_GET_INT(map);
  RET_STR(P_GetMapName(map));
}


IMPLEMENT_FUNCTION(VObject, P_GetMapIndexByLevelNum) {
  P_GET_INT(map);
  RET_INT(P_GetMapIndexByLevelNum(map));
}


IMPLEMENT_FUNCTION(VObject, P_GetNumMaps) {
  RET_INT(P_GetNumMaps());
}


IMPLEMENT_FUNCTION(VObject, P_GetMapInfo) {
  P_GET_INT(map);
  RET_PTR(P_GetMapInfoPtr(map));
}


IMPLEMENT_FUNCTION(VObject, P_GetMapLumpName) {
  P_GET_INT(map);
  RET_NAME(P_GetMapLumpName(map));
}


IMPLEMENT_FUNCTION(VObject, P_TranslateMap) {
  P_GET_INT(map);
  RET_NAME(P_TranslateMap(map));
}


IMPLEMENT_FUNCTION(VObject, P_GetNumEpisodes) {
  RET_INT(P_GetNumEpisodes());
}


IMPLEMENT_FUNCTION(VObject, P_GetEpisodeDef) {
  P_GET_INT(Index);
  RET_PTR(P_GetEpisodeDef(Index));
}


IMPLEMENT_FUNCTION(VObject, P_GetNumSkills) {
  RET_INT(P_GetNumSkills());
}


IMPLEMENT_FUNCTION(VObject, P_GetSkillDef) {
  P_GET_INT(Index);
  RET_PTR(const_cast<VSkillDef*>(P_GetSkillDef(Index)));
}


IMPLEMENT_FUNCTION(VObject, KeyNameForNum) {
  P_GET_INT(keynum);
  RET_STR(GInput->KeyNameForNum(keynum));
}


IMPLEMENT_FUNCTION(VObject, IN_GetBindingKeys) {
  P_GET_PTR(int, key2);
  P_GET_PTR(int, key1);
  P_GET_STR(name);
  GInput->GetBindingKeys(name, *key1, *key2);
}


IMPLEMENT_FUNCTION(VObject, IN_SetBinding) {
  P_GET_STR(onup);
  P_GET_STR(ondown);
  P_GET_INT(keynum);
  GInput->SetBinding(keynum, ondown, onup);
}


IMPLEMENT_FUNCTION(VObject, SV_GetSaveString) {
  P_GET_PTR(VStr, buf);
  P_GET_INT(i);
#ifdef SERVER
  RET_INT(SV_GetSaveString(i, *buf));
#else
  RET_INT(0);
#endif
}


IMPLEMENT_FUNCTION(VObject, GetSlist) {
  RET_PTR(GNet->GetSlist());
}


IMPLEMENT_FUNCTION(VObject, LoadTextLump) {
  P_GET_NAME(name);
  RET_STR(W_LoadTextLump(name));
}


IMPLEMENT_FUNCTION(VObject, StartSearch) {
  P_GET_BOOL(Master);
  GNet->StartSearch(Master);
}

#endif // CLIENT

static char wrbuffer[1024] = {0};

void PR_WriteOne (const VFieldType &type) {
  char *sptr = (char *)memchr(wrbuffer, 0, sizeof(wrbuffer));
  if (!sptr) { sptr = wrbuffer; wrbuffer[0] = 0; } // just in case
  size_t maxlen = (size_t)(wrbuffer+sizeof(wrbuffer)-sptr);
  switch (type.Type) {
    case TYPE_Int: case TYPE_Byte: snprintf(sptr, maxlen, "%d", PR_Pop()); break;
    case TYPE_Bool: snprintf(sptr, maxlen, "%s", (PR_Pop() ? "true" : "false")); break;
    case TYPE_Float: snprintf(sptr, maxlen, "%f", PR_Popf()); break;
    case TYPE_Name: snprintf(sptr, maxlen, "%s", *PR_PopName()); break;
    case TYPE_String: snprintf(sptr, maxlen, "%s", *PR_PopStr()); break;
    case TYPE_Vector: { TVec v = PR_Popv(); snprintf(sptr, maxlen, "(%f,%f,%f)", v.x, v.y, v.z); } break;
    case TYPE_Pointer: snprintf(sptr, maxlen, "<%s>(%p)", *type.GetName(), PR_PopPtr()); break;
    case TYPE_Class: if (PR_PopPtr()) snprintf(sptr, maxlen, "<%s>", *type.GetName()); else snprintf(sptr, maxlen, "<none>"); break;
    case TYPE_State:
      {
        VState *st = (VState *)PR_PopPtr();
        if (st) {
          snprintf(sptr, maxlen, "<state:%s %d %f>", *st->SpriteName, st->Frame, st->Time);
        } else {
          snprintf(sptr, maxlen, "<state>");
        }
      }
      break;
    case TYPE_Reference: snprintf(sptr, maxlen, "<%s>", (type.Class ? *type.Class->Name : "none")); break;
    case TYPE_Delegate: snprintf(sptr, maxlen, "<%s:%p:%p>", *type.GetName(), PR_PopPtr(), PR_PopPtr()); break;
    case TYPE_Struct: PR_PopPtr(); snprintf(sptr, maxlen, "<%s>", *type.Struct->Name); break;
    case TYPE_Array: PR_PopPtr(); snprintf(sptr, maxlen, "<%s>", *type.GetName()); break;
    case TYPE_DynamicArray:
      {
        VScriptArray *a = (VScriptArray *)PR_PopPtr();
        snprintf(sptr, maxlen, "%s(%d)", *type.GetName(), a->Num());
      }
      break;
    default: Sys_Error("Tried to print something strange...");
  }
}

void PR_WriteFlush () {
  GCon->Logf("%s", wrbuffer);
  wrbuffer[0] = 0;
}

#endif // !VCC_STANDALONE_EXECUTOR
