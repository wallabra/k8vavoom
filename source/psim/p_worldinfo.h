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

class VAcsGlobal;


class VWorldInfo : public VGameObject {
  DECLARE_CLASS(VWorldInfo, VGameObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VWorldInfo)

  VAcsGlobal *Acs;

  vuint8 GameSkill;
  float SkillAmmoFactor;
  float SkillDoubleAmmoFactor;
  float SkillDamageFactor;
  float SkillRespawnTime;
  vint32 SkillRespawnLimit;
  float SkillAggressiveness;
  vint32 SkillSpawnFilter;
  vint32 SkillAcsReturn;

  enum {
    WIF_SkillFastMonsters  = 0x00000001,
    WIF_SkillDisableCheats = 0x00000002,
    WIF_SkillEasyBossBrain = 0x00000004,
    WIF_SkillAutoUseHealth = 0x00000008,
    WIF_SkillSlowMonsters  = 0x00000010,
  };
  vuint32 Flags;

  // monster replacements for each skill
  // WARNING! currently it works only for predefined map spawns!
  TArray<VSkillMonsterReplacement> SkillReplacements;

public:
  //VWorldInfo ();
  virtual void PostCtor () override;

  virtual void SerialiseOther (VStream &Strm) override;
  virtual void Destroy () override;

  void SetSkill (int);
  const VStr GetCurrSkillName () const;

  DECLARE_FUNCTION(SetSkill)

  DECLARE_FUNCTION(GetACSGlobalStr)

  DECLARE_FUNCTION(GetACSGlobalInt)
  DECLARE_FUNCTION(GetACSGlobalFloat)
  DECLARE_FUNCTION(SetACSGlobalInt)
  DECLARE_FUNCTION(SetACSGlobalFloat)

  DECLARE_FUNCTION(GetACSWorldInt)
  DECLARE_FUNCTION(GetACSWorldFloat)
  DECLARE_FUNCTION(SetACSWorldInt)
  DECLARE_FUNCTION(SetACSWorldFloat)
};
