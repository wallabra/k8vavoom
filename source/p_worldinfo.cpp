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

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"
#include "sv_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

IMPLEMENT_CLASS(V, WorldInfo)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
//  VWorldInfo::VWorldInfo
//
//==========================================================================

VWorldInfo::VWorldInfo()
{
  Acs = new VAcsGlobal;
}

//==========================================================================
//
//  VWorldInfo::Serialise
//
//==========================================================================

void VWorldInfo::Serialise(VStream &Strm)
{
  guard(VWorldInfo::Serialise);
  vuint8 xver = 0;
  Strm << xver;
  // serialise global script info
  Acs->Serialise(Strm);
  Super::Serialise(Strm);
  unguard;
}

//==========================================================================
//
//  VWorldInfo::Destroy
//
//==========================================================================

void VWorldInfo::Destroy()
{
  guard(VWorldInfo::Destroy);
  delete Acs;
  Acs = nullptr;

  Super::Destroy();
  unguard;
}

//==========================================================================
//
//  VWorldInfo::SetSkill
//
//==========================================================================

void VWorldInfo::SetSkill(int ASkill)
{
  guard(VWorldInfo::SetSkill);
  if (ASkill < 0)
  {
    GameSkill = 0;
  }
  else if (ASkill >= P_GetNumSkills())
  {
    GameSkill = P_GetNumSkills() - 1;
  }
  else
  {
    GameSkill = ASkill;
  }
  const VSkillDef *SDef = P_GetSkillDef(GameSkill);

  SkillAmmoFactor = SDef->AmmoFactor;
  SkillDoubleAmmoFactor = SDef->DoubleAmmoFactor;
  SkillDamageFactor = SDef->DamageFactor;
  SkillRespawnTime = SDef->RespawnTime;
  SkillRespawnLimit = SDef->RespawnLimit;
  SkillAggressiveness = SDef->Aggressiveness;
  SkillSpawnFilter = SDef->SpawnFilter;
  SkillAcsReturn = SDef->AcsReturn;
  Flags = (Flags & 0xffffff00) | (SDef->Flags & 0x0000000f);
  if (SDef->Flags&SKILLF_SlowMonsters) Flags |= WIF_SkillSlowMonsters;
  unguard;
}


//==========================================================================
//
//  VWorldInfo
//
//==========================================================================
IMPLEMENT_FUNCTION(VWorldInfo, SetSkill) {
  P_GET_INT(Skill);
  P_GET_SELF;
  Self->SetSkill(Skill);
}

// native final int GetACSGlobalInt (int index);
IMPLEMENT_FUNCTION(VWorldInfo, GetACSGlobalInt) {
  P_GET_INT(index);
  P_GET_SELF;
  RET_INT(Self && Self->Acs ? Self->Acs->GetGVarInt(index) : 0);
}

// native final int GetACSGlobalInt (void *level, int index);
IMPLEMENT_FUNCTION(VWorldInfo, GetACSGlobalStr) {
  P_GET_INT(index);
  P_GET_PTR(VAcsLevel, alvl);
  P_GET_SELF;
  RET_STR(Self && Self->Acs ? Self->Acs->GetGVarStr(alvl, index) : VStr());
}

// native final float GetACSGlobalFloat (int index);
IMPLEMENT_FUNCTION(VWorldInfo, GetACSGlobalFloat) {
  P_GET_INT(index);
  P_GET_SELF;
  RET_FLOAT(Self && Self->Acs ? Self->Acs->GetGVarFloat(index) : 0.0f);
}

// native final void SetACSGlobalInt (int index, int value);
IMPLEMENT_FUNCTION(VWorldInfo, SetACSGlobalInt) {
  P_GET_INT(value);
  P_GET_INT(index);
  P_GET_SELF;
  if (Self && Self->Acs) Self->Acs->SetGVarInt(index, value);
}

// native final void SetACSGlobalFloat (int index, float value);
IMPLEMENT_FUNCTION(VWorldInfo, SetACSGlobalFloat) {
  P_GET_FLOAT(value);
  P_GET_INT(index);
  P_GET_SELF;
  if (Self && Self->Acs) Self->Acs->SetGVarFloat(index, value);
}

// native final int GetACSWorldInt (int index);
IMPLEMENT_FUNCTION(VWorldInfo, GetACSWorldInt) {
  P_GET_INT(index);
  P_GET_SELF;
  RET_INT(Self && Self->Acs ? Self->Acs->GetWVarInt(index) : 0);
}

// native final float GetACSWorldFloat (int index);
IMPLEMENT_FUNCTION(VWorldInfo, GetACSWorldFloat) {
  P_GET_INT(index);
  P_GET_SELF;
  RET_FLOAT(Self && Self->Acs ? Self->Acs->GetWVarFloat(index) : 0.0f);
}

// native final void SetACSWorldInt (int index, int value);
IMPLEMENT_FUNCTION(VWorldInfo, SetACSWorldInt) {
  P_GET_INT(value);
  P_GET_INT(index);
  P_GET_SELF;
  if (Self && Self->Acs) Self->Acs->SetWVarInt(index, value);
}

// native final void SetACSWorldFloat (int index, float value);
IMPLEMENT_FUNCTION(VWorldInfo, SetACSWorldFloat) {
  P_GET_FLOAT(value);
  P_GET_INT(index);
  P_GET_SELF;
  if (Self && Self->Acs) Self->Acs->SetWVarFloat(index, value);
}
