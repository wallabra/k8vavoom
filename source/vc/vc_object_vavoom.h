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
  // textures
  DECLARE_FUNCTION(CheckTextureNumForName)
  DECLARE_FUNCTION(TextureNumForName)
  DECLARE_FUNCTION(CheckFlatNumForName)
  DECLARE_FUNCTION(FlatNumForName)
  DECLARE_FUNCTION(TextureHeight)
  DECLARE_FUNCTION(GetTextureName)

  // console command functions
  DECLARE_FUNCTION(Cmd_CheckParm)
  DECLARE_FUNCTION(Cmd_GetArgC)
  DECLARE_FUNCTION(Cmd_GetArgV)
  DECLARE_FUNCTION(CmdBuf_AddText)

  DECLARE_FUNCTION(AreStateSpritesPresent)

  // misc
  DECLARE_FUNCTION(Info_ValueForKey)
  DECLARE_FUNCTION(WadLumpPresent)
  DECLARE_FUNCTION(FindAnimDoor)
  DECLARE_FUNCTION(GetLangString)
  DECLARE_FUNCTION(RGB)
  DECLARE_FUNCTION(RGBA)
  DECLARE_FUNCTION(GetLockDef)
  DECLARE_FUNCTION(ParseColour)
  DECLARE_FUNCTION(TextColourString)
  DECLARE_FUNCTION(StartTitleMap)
  DECLARE_FUNCTION(LoadBinaryLump)
  DECLARE_FUNCTION(IsMapPresent)

#ifdef CLIENT
  DECLARE_FUNCTION(KeyNameForNum)
  DECLARE_FUNCTION(IN_GetBindingKeys)
  DECLARE_FUNCTION(IN_SetBinding)
  DECLARE_FUNCTION(StartSearch)
  DECLARE_FUNCTION(GetSlist)

  // graphics
  DECLARE_FUNCTION(SetVirtualScreen)
  DECLARE_FUNCTION(GetVirtualWidth)
  DECLARE_FUNCTION(GetVirtualHeight)
  DECLARE_FUNCTION(GetRealScreenWidth)
  DECLARE_FUNCTION(GetRealScreenHeight)
  DECLARE_FUNCTION(R_RegisterPic)
  DECLARE_FUNCTION(R_RegisterPicPal)
  DECLARE_FUNCTION(R_GetPicInfo)
  DECLARE_FUNCTION(R_DrawPic)
  DECLARE_FUNCTION(R_DrawPicFloat)
  DECLARE_FUNCTION(R_InstallSprite)
  DECLARE_FUNCTION(R_DrawSpritePatch)
  DECLARE_FUNCTION(InstallModel)
  DECLARE_FUNCTION(R_DrawModelFrame)
  DECLARE_FUNCTION(R_FillRect)
  DECLARE_FUNCTION(R_GetAspectRatio)

  // client side sound
  DECLARE_FUNCTION(LocalSound)
  DECLARE_FUNCTION(IsLocalSoundPlaying)
  DECLARE_FUNCTION(StopLocalSounds)

  DECLARE_FUNCTION(TranslateKey)

#include "../neoui/vc_object_neoui.h"
#endif // CLIENT

  DECLARE_FUNCTION(LoadTextLump)

  DECLARE_FUNCTION(IsAnimatedTexture)

  DECLARE_FUNCTION(R_GetBloodTranslation)

  DECLARE_FUNCTION(KBCheatClearAll)
  DECLARE_FUNCTION(KBCheatAppend)

#ifdef SERVER
  DECLARE_FUNCTION(P_GetMapName)
  DECLARE_FUNCTION(P_GetMapIndexByLevelNum)
  DECLARE_FUNCTION(P_GetNumMaps)
  DECLARE_FUNCTION(P_GetMapInfo)
  DECLARE_FUNCTION(P_GetMapLumpName)
  DECLARE_FUNCTION(P_TranslateMap)
  DECLARE_FUNCTION(P_GetNumEpisodes)
  DECLARE_FUNCTION(P_GetEpisodeDef)
  DECLARE_FUNCTION(P_GetNumSkills)
  DECLARE_FUNCTION(P_GetSkillDef)

  DECLARE_FUNCTION(SV_GetSaveString)
  DECLARE_FUNCTION(SV_GetSaveDateString)

  // map utilites
  DECLARE_FUNCTION(SectorClosestPoint)
  DECLARE_FUNCTION(LineOpenings)
  DECLARE_FUNCTION(P_BoxOnLineSide)
  DECLARE_FUNCTION(FindThingGap)
  DECLARE_FUNCTION(FindOpening)
  DECLARE_FUNCTION(PointInRegion)
  DECLARE_FUNCTION(P_GetMidTexturePosition)

  // sound functions
  DECLARE_FUNCTION(SectorStopSound)
  DECLARE_FUNCTION(GetSoundPlayingInfo)
  DECLARE_FUNCTION(GetSoundID)
  DECLARE_FUNCTION(SetSeqTrans)
  DECLARE_FUNCTION(GetSeqTrans)
  DECLARE_FUNCTION(GetSeqSlot)
  DECLARE_FUNCTION(StopAllSounds)

  DECLARE_FUNCTION(SB_Start)
  DECLARE_FUNCTION(TerrainType)
  DECLARE_FUNCTION(GetSplashInfo)
  DECLARE_FUNCTION(GetTerrainInfo)
  DECLARE_FUNCTION(FindClassFromEditorId)
  DECLARE_FUNCTION(FindClassFromScriptId)

  DECLARE_FUNCTION(HasDecal)
#endif // SERVER

  DECLARE_FUNCTION(W_IterateNS)
  DECLARE_FUNCTION(W_IterateFile)

  DECLARE_FUNCTION(W_LumpLength)
  DECLARE_FUNCTION(W_LumpName)
  DECLARE_FUNCTION(W_FullLumpName)
  DECLARE_FUNCTION(W_LumpFile)

  DECLARE_FUNCTION(W_CheckNumForName)
  DECLARE_FUNCTION(W_GetNumForName)
  DECLARE_FUNCTION(W_CheckNumForNameInFile)

  DECLARE_FUNCTION(GetCurrRefDef)
