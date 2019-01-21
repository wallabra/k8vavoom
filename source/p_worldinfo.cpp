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
#include "gamedefs.h"
#include "sv_local.h"


IMPLEMENT_CLASS(V, WorldInfo)


//==========================================================================
//
//  VWorldInfo::PostCtor
//
//==========================================================================
void VWorldInfo::PostCtor () {
  Super::PostCtor();
  Acs = new VAcsGlobal;
}


//==========================================================================
//
//  VWorldInfo::SerialiseOther
//
//==========================================================================
void VWorldInfo::SerialiseOther (VStream &Strm) {
  guard(VWorldInfo::Serialise);
  Super::SerialiseOther(Strm);
  vuint8 xver = 0;
  Strm << xver;
  // serialise global script info
  Acs->Serialise(Strm);
  unguard;
}


//==========================================================================
//
//  VWorldInfo::Destroy
//
//==========================================================================
void VWorldInfo::Destroy () {
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
void VWorldInfo::SetSkill (int ASkill) {
  guard(VWorldInfo::SetSkill);
       if (ASkill < 0) GameSkill = 0;
  else if (ASkill >= P_GetNumSkills()) GameSkill = P_GetNumSkills()-1;
  else GameSkill = ASkill;
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
//  VWorldInfo::GetCurrSkillName
//
//==========================================================================
const VStr VWorldInfo::GetCurrSkillName () const {
  const VSkillDef *SDef = P_GetSkillDef(GameSkill);
  return SDef->Name;
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
  RET_INT(Self && Self->Acs ? Self->Acs->GetGlobalVarInt(index) : 0);
}

// native final int GetACSGlobalInt (void *level, int index);
IMPLEMENT_FUNCTION(VWorldInfo, GetACSGlobalStr) {
  P_GET_INT(index);
  P_GET_PTR(VAcsLevel, alvl);
  P_GET_SELF;
  RET_STR(Self && Self->Acs ? Self->Acs->GetGlobalVarStr(alvl, index) : VStr());
}

// native final float GetACSGlobalFloat (int index);
IMPLEMENT_FUNCTION(VWorldInfo, GetACSGlobalFloat) {
  P_GET_INT(index);
  P_GET_SELF;
  RET_FLOAT(Self && Self->Acs ? Self->Acs->GetGlobalVarFloat(index) : 0.0f);
}

// native final void SetACSGlobalInt (int index, int value);
IMPLEMENT_FUNCTION(VWorldInfo, SetACSGlobalInt) {
  P_GET_INT(value);
  P_GET_INT(index);
  P_GET_SELF;
  if (Self && Self->Acs) Self->Acs->SetGlobalVarInt(index, value);
}

// native final void SetACSGlobalFloat (int index, float value);
IMPLEMENT_FUNCTION(VWorldInfo, SetACSGlobalFloat) {
  P_GET_FLOAT(value);
  P_GET_INT(index);
  P_GET_SELF;
  if (Self && Self->Acs) Self->Acs->SetGlobalVarFloat(index, value);
}

// native final int GetACSWorldInt (int index);
IMPLEMENT_FUNCTION(VWorldInfo, GetACSWorldInt) {
  P_GET_INT(index);
  P_GET_SELF;
  RET_INT(Self && Self->Acs ? Self->Acs->GetWorldVarInt(index) : 0);
}

// native final float GetACSWorldFloat (int index);
IMPLEMENT_FUNCTION(VWorldInfo, GetACSWorldFloat) {
  P_GET_INT(index);
  P_GET_SELF;
  RET_FLOAT(Self && Self->Acs ? Self->Acs->GetWorldVarFloat(index) : 0.0f);
}

// native final void SetACSWorldInt (int index, int value);
IMPLEMENT_FUNCTION(VWorldInfo, SetACSWorldInt) {
  P_GET_INT(value);
  P_GET_INT(index);
  P_GET_SELF;
  if (Self && Self->Acs) Self->Acs->SetWorldVarInt(index, value);
}

// native final void SetACSWorldFloat (int index, float value);
IMPLEMENT_FUNCTION(VWorldInfo, SetACSWorldFloat) {
  P_GET_FLOAT(value);
  P_GET_INT(index);
  P_GET_SELF;
  if (Self && Self->Acs) Self->Acs->SetWorldVarFloat(index, value);
}
