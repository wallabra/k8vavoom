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
#include "gamedefs.h"
#include "sv_local.h"


VCvarB compat_shorttex("compat_shorttex", false, "Compatibility: shorttex", 0);
VCvarB compat_stairs("compat_stairs", false, "Compatibility: stairs", 0);
VCvarI compat_limitpain("compat_limitpain", "0", "Compatibility: limit number of skulls from Pain Elementals? (0:map default; 1:always; 2: never)", CVAR_Archive);
VCvarI compat_nopassover("compat_nopassover", "0", "Compatibility: infinitely tall monsters? (0:map default; 1:always; 2: never)", CVAR_Archive);
VCvarI compat_notossdrops("compat_notossdrops", "0", "Compatibility: toss dropped items? (0:map default; 1:always; 2: never)", CVAR_Archive);
VCvarB compat_useblocking("compat_useblocking", false, "Compatibility: useblocking", 0);
VCvarB compat_nodoorlight("compat_nodoorlight", false, "Compatibility: nodoorlight", 0);
VCvarB compat_ravenscroll("compat_ravenscroll", false, "Compatibility: ravenscroll", 0);
VCvarB compat_soundtarget("compat_soundtarget", false, "Compatibility: soundtarget", 0);
VCvarB compat_dehhealth("compat_dehhealth", false, "Compatibility: dehhealth", 0);
VCvarB compat_trace("compat_trace", false, "Compatibility: trace", 0);
VCvarB compat_dropoff("compat_dropoff", false, "Compatibility: dropoff", 0);
VCvarB compat_boomscroll("compat_boomscroll", false, "Compatibility: boomscroll", 0);
VCvarB compat_invisibility("compat_invisibility", false, "Compatibility: invisibility", 0);


IMPLEMENT_CLASS(V, LevelInfo)

static VCvarF sv_gravity("sv_gravity", "800", "Gravity value.", 0/*CVAR_ServerInfo*/);
static VCvarF sv_aircontrol("sv_aircontrol", "0.00390625", "Air control value.", 0/*CVAR_ServerInfo*/);


//==========================================================================
//
//  VLevelInfo::PostCtor
//
//==========================================================================
void VLevelInfo::PostCtor () {
  Super::PostCtor();
  Level = this;
  //GCon->Logf("** falling damage: flags=0x%08x", (unsigned)LevelInfoFlags);
}


//==========================================================================
//
//  VLevelInfo::SetMapInfo
//
//==========================================================================
void VLevelInfo::SetMapInfo (const mapInfo_t &Info) {
  const VClusterDef *CInfo = P_GetClusterDef(Info.Cluster);

  LevelName = Info.Name;
  LevelNum = Info.LevelNum;
  Cluster = Info.Cluster;

  NextMap = Info.NextMap;
  SecretMap = Info.SecretMap;

  ParTime = Info.ParTime;
  SuckTime = Info.SuckTime;

  Sky1Texture = Info.Sky1Texture;
  Sky2Texture = Info.Sky2Texture;
  Sky1ScrollDelta = Info.Sky1ScrollDelta;
  Sky2ScrollDelta = Info.Sky2ScrollDelta;
  SkyBox = Info.SkyBox;

  FadeTable = Info.FadeTable;
  Fade = Info.Fade;
  OutsideFog = Info.OutsideFog;

  SongLump = Info.SongLump;

  Gravity = (Info.Gravity ? Info.Gravity : sv_gravity)*DEFAULT_GRAVITY/800.0f;
  AirControl = (Info.AirControl ? Info.AirControl : sv_aircontrol);

  Infighting = Info.Infighting;
  SpecialActions = Info.SpecialActions;

  // copy flags from mapinfo
  //GCon->Logf("*** level info flags: 0x%08x", (unsigned)Info.Flags);
  LevelInfoFlags = Info.Flags;
  LevelInfoFlags2 = Info.Flags2;

  // doom format maps use strict monster activation by default
  if (!(XLevel->LevelFlags&VLevel::LF_Extended) && !(LevelInfoFlags2&LIF2_HaveMonsterActivation)) {
    LevelInfoFlags2 &= ~LIF2_LaxMonsterActivation;
  }

  if (CInfo->Flags&CLUSTERF_Hub) LevelInfoFlags2 |= LIF2_ClusterHub;

  // no auto sequences flag sets all sectors to use sequence 0 by default
  if (Info.Flags&VLevelInfo::LIF_NoAutoSndSeq) {
    for (int i = 0; i < XLevel->NumSectors; ++i) XLevel->Sectors[i].seqType = 0;
  }

  eventAfterSetMapInfo();
}


//==========================================================================
//
//  VLevelInfo::SectorStartSound
//
//==========================================================================
void VLevelInfo::SectorStartSound (const sector_t *Sector, int SoundId,
                                   int Channel, float Volume, float Attenuation)
{
  if (Sector) {
    if (Sector->SectorFlags&sector_t::SF_Silent) return;
    StartSound(Sector->soundorg, (Sector-XLevel->Sectors)+(SNDORG_Sector<<24), SoundId, Channel, Volume, Attenuation, false);
  } else {
    StartSound(TVec(0, 0, 0), 0, SoundId, Channel, Volume, Attenuation, false);
  }
}


//==========================================================================
//
//  VLevelInfo::SectorStopSound
//
//==========================================================================
void VLevelInfo::SectorStopSound (const sector_t *sector, int channel) {
  if (sector) StopSound((sector-XLevel->Sectors)+(SNDORG_Sector<<24), channel);
}


//==========================================================================
//
//  VLevelInfo::SectorStartSequence
//
//==========================================================================
void VLevelInfo::SectorStartSequence (const sector_t *Sector, VName Name, int ModeNum) {
  if (Sector) {
    if (Sector->SectorFlags&sector_t::SF_Silent) return;
    StartSoundSequence(Sector->soundorg, (Sector-XLevel->Sectors)+(SNDORG_Sector<<24), Name, ModeNum);
  } else {
    StartSoundSequence(TVec(0, 0, 0), 0, Name, ModeNum);
  }
}


//==========================================================================
//
//  VLevelInfo::SectorStopSequence
//
//==========================================================================
void VLevelInfo::SectorStopSequence (const sector_t *sector) {
  if (sector) StopSoundSequence((int)(ptrdiff_t)(sector-XLevel->Sectors)+(SNDORG_Sector<<24));
}


//==========================================================================
//
//  VLevelInfo::PolyobjStartSequence
//
//==========================================================================
void VLevelInfo::PolyobjStartSequence (const polyobj_t *Poly, VName Name, int ModeNum) {
  if (!Poly || !Poly->GetSubsector() || !Poly->GetSubsector()->sector) return;
  if (Poly->GetSubsector() && Poly->GetSubsector()->sector) {
    if (Poly->GetSubsector()->sector->SectorFlags&sector_t::SF_Silent) return;
  }
  StartSoundSequence(Poly->startSpot, Poly->index+(SNDORG_PolyObj<<24), Name, ModeNum);
}


//==========================================================================
//
//  VLevelInfo::PolyobjStopSequence
//
//==========================================================================
void VLevelInfo::PolyobjStopSequence (const polyobj_t *poly) {
  if (!poly) return;
  StopSoundSequence(poly->index+(SNDORG_PolyObj<<24));
}


//==========================================================================
//
//  VLevelInfo::ExitLevel
//
//==========================================================================
void VLevelInfo::ExitLevel (int Position) {
  LeavePosition = Position;
  completed = true;
}


//==========================================================================
//
//  VLevelInfo::SecretExitLevel
//
//==========================================================================
void VLevelInfo::SecretExitLevel (int Position) {
  if (SecretMap == NAME_None) {
    // no secret map, use normal exit
    ExitLevel(Position);
    return;
  }

  LeavePosition = Position;
  completed = true;

  NextMap = SecretMap; // go to secret level

  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (Game->Players[i]) Game->Players[i]->PlayerFlags |= VBasePlayer::PF_DidSecret;
  }
}


//==========================================================================
//
//  VLevelInfo::Completed
//
//  starts intermission routine, which is used only during hub exits,
//  and DeathMatch games.
//
//==========================================================================
void VLevelInfo::Completed (int InMap, int InPosition, int SaveAngle) {
  int Map = InMap;
  int Position = InPosition;
  if (Map == -1 && Position == -1) {
    if (!deathmatch) {
      // if we have cluster exit text, process with the normal intermission sequence
      const mapInfo_t &old_info = P_GetMapInfo(GLevel->MapName);
      const VClusterDef *ClusterD = P_GetClusterDef(old_info.Cluster);
      if (ClusterD && !ClusterD->ExitText.xstrip().isEmpty()) {
        LeavePosition = Position; // just in case
        completed = true;
        return;
      }
      // otherwise, jump straight into intermission
      for (int i = 0; i < svs.max_clients; ++i) {
        if (Game->Players[i]) Game->Players[i]->eventClientFinale(VStr(NextMap).StartsWithCI("EndGame") ? *NextMap : "");
      }
      sv.intermission = 2;
      return;
    }
    Map = 1;
    Position = 0;
  }
  NextMap = P_GetMapLumpNameByLevelNum(Map);

  LeavePosition = Position;
  completed = true;
}


//==========================================================================
//
//  VLevelInfo::FindMobjFromTID
//
//==========================================================================
VEntity *VLevelInfo::FindMobjFromTID (int tid, VEntity *Prev) {
  for (VEntity *E = (Prev ? Prev->TIDHashNext : TIDHash[tid&(TID_HASH_SIZE-1)]); E; E = E->TIDHashNext) {
    if (E->TID == tid) return E;
  }
  return nullptr;
}


//==========================================================================
//
//  VLevelInfo::ChangeMusic
//
//==========================================================================
void VLevelInfo::ChangeMusic (VName SongName) {
  SongLump = SongName;
}


//==========================================================================
//
//  VLevelInfo::FindFreeTID
//
//  see `UniqueTID`
//
//==========================================================================
int VLevelInfo::FindFreeTID (int tidstart, int limit) const {
  if (tidstart <= 0) tidstart = 0;
  if (limit < 0) return 0;
  if (limit == 0) limit = 0x7fffffff;
  if (tidstart == 0) {
    // do several random hits, then linear search
    for (int rndtry = 1024; rndtry; --rndtry) {
      do { tidstart = GenRandomU31()&0x7fff; } while (tidstart == 0);
      if (!IsTIDUsed(tidstart)) return tidstart;
    }
    // fallback to linear search
    tidstart = 1;
  } else {
    tidstart = 1; // 0 is used
  }
  // linear search
  while (limit-- > 0) {
    if (!IsTIDUsed(tidstart)) return tidstart;
    ++tidstart;
    if (tidstart == 0x1fffffff) return 0; // arbitrary limit
    //if (tidstart == 0x8000) return 0; // arbitrary limit
  }
  return 0;
}


//==========================================================================
//
//  VLevelInfo::FindFreeTID
//
//==========================================================================
bool VLevelInfo::IsTIDUsed (int tid) const {
  if (tid == 0) return true; // this is "self"
  VEntity *E = Level->TIDHash[tid&(TID_HASH_SIZE-1)];
  while (E) {
    if (E->TID == tid) return true;
    E = E->TIDHashNext;
  }
  return false;
}


//==========================================================================
//
//  VLevelInfo natives
//
//==========================================================================
// native final void AddStaticLight (Entity ent, TVec origin, float radius, optional TVec coneDirection, optional float coneAngle);
IMPLEMENT_FUNCTION(VLevelInfo, AddStaticLight) {
  VEntity *Ent;
  TVec Origin;
  float Radius;
  VOptParamVec ConeDir(TVec(0, 0, 0));
  VOptParamFloat ConeAngle(0);
  vobjGetParamSelf(Ent, Origin, Radius, ConeDir, ConeAngle);
  Self->XLevel->AddStaticLightRGB(Ent, Origin, Radius, 0xffffffffu, ConeDir, ConeAngle);
}

// native final void AddStaticLightRGB (Entity ent, TVec origin, float radius, int color, optional TVec coneDirection, optional float coneAngle);
IMPLEMENT_FUNCTION(VLevelInfo, AddStaticLightRGB) {
  VEntity *Ent;
  TVec Origin;
  float Radius;
  vint32 Color;
  VOptParamVec ConeDir(TVec(0, 0, 0));
  VOptParamFloat ConeAngle(0);
  vobjGetParamSelf(Ent, Origin, Radius, Color, ConeDir, ConeAngle);
  Self->XLevel->AddStaticLightRGB(Ent, Origin, Radius, (vuint32)Color, ConeDir, ConeAngle);
}

IMPLEMENT_FUNCTION(VLevelInfo, MoveStaticLightByOwner) {
  P_GET_VEC(Origin);
  P_GET_REF(VEntity, Ent);
  P_GET_SELF;
  Self->XLevel->MoveStaticLightByOwner(Ent, Origin);
}

IMPLEMENT_FUNCTION(VLevelInfo, SectorStartSequence) {
  P_GET_INT(ModeNum);
  P_GET_NAME(name);
  P_GET_PTR(sector_t, sec);
  P_GET_SELF;
  Self->SectorStartSequence(sec, name, ModeNum);
}

IMPLEMENT_FUNCTION(VLevelInfo, SectorStopSequence) {
  P_GET_PTR(sector_t, sec);
  P_GET_SELF;
  Self->SectorStopSequence(sec);
}

IMPLEMENT_FUNCTION(VLevelInfo, PolyobjStartSequence) {
  P_GET_INT(ModeNum);
  P_GET_NAME(name);
  P_GET_PTR(polyobj_t, poly);
  P_GET_SELF;
  Self->PolyobjStartSequence(poly, name, ModeNum);
}

IMPLEMENT_FUNCTION(VLevelInfo, PolyobjStopSequence) {
  P_GET_PTR(polyobj_t, poly);
  P_GET_SELF;
  Self->PolyobjStopSequence(poly);
}

IMPLEMENT_FUNCTION(VLevelInfo, ExitLevel) {
  P_GET_INT(Position);
  P_GET_SELF;
  Self->ExitLevel(Position);
}

IMPLEMENT_FUNCTION(VLevelInfo, SecretExitLevel) {
  P_GET_INT(Position);
  P_GET_SELF;
  Self->SecretExitLevel(Position);
}

IMPLEMENT_FUNCTION(VLevelInfo, Completed) {
  P_GET_INT(SaveAngle);
  P_GET_INT(pos);
  P_GET_INT(map);
  P_GET_SELF;
  Self->Completed(map, pos, SaveAngle);
}

IMPLEMENT_FUNCTION(VLevelInfo, ChangeSwitchTexture) {
  P_GET_PTR(vuint8, pQuest);
  P_GET_NAME(DefaultSound);
  P_GET_BOOL(useAgain);
  P_GET_INT(SideNum);
  P_GET_SELF;
  bool Quest;
  bool Ret = Self->ChangeSwitchTexture(SideNum, useAgain, DefaultSound, Quest);
  if (pQuest) *pQuest = Quest;
  RET_BOOL(Ret);
}

IMPLEMENT_FUNCTION(VLevelInfo, FindMobjFromTID) {
  P_GET_REF(VEntity, Prev);
  P_GET_INT(tid);
  P_GET_SELF;
  RET_REF(Self->FindMobjFromTID(tid, Prev));
}

IMPLEMENT_FUNCTION(VLevelInfo, AutoSave) {
  P_GET_BOOL_OPT(checkpoint, false);
  P_GET_SELF;
  if (Self->Game->NetMode == NM_Standalone) SV_AutoSave(checkpoint);
}

IMPLEMENT_FUNCTION(VLevelInfo, ChangeMusic) {
  P_GET_NAME(SongName);
  P_GET_SELF;
  Self->ChangeMusic(SongName);
}

IMPLEMENT_FUNCTION(VLevelInfo, FindFreeTID) {
  P_GET_INT_OPT(limit, 0);
  P_GET_INT_OPT(tidstart, 0);
  P_GET_SELF;
  RET_INT(Self->FindFreeTID(tidstart, limit));
}

IMPLEMENT_FUNCTION(VLevelInfo, IsTIDUsed) {
  P_GET_INT(tid);
  P_GET_SELF;
  RET_BOOL(Self->IsTIDUsed(tid));
}

// compat getters
IMPLEMENT_FUNCTION(VLevelInfo, get_CompatShortTex)     { vobjGetParamSelf(); RET_BOOL(Self ? (!!(Self->LevelInfoFlags2&LIF2_CompatShortTex)     || compat_shorttex.asBool()) : false); }
IMPLEMENT_FUNCTION(VLevelInfo, get_CompatStairs)       { vobjGetParamSelf(); RET_BOOL(Self ? (!!(Self->LevelInfoFlags2&LIF2_CompatStairs)       || compat_stairs.asBool()) : false); }
IMPLEMENT_FUNCTION(VLevelInfo, get_CompatLimitPain)    { vobjGetParamSelf(); RET_BOOL(Self ? Self->GetLimitPain() : false); }
IMPLEMENT_FUNCTION(VLevelInfo, get_CompatNoPassOver)   { vobjGetParamSelf(); RET_BOOL(Self ? Self->GetNoPassOver() : false); }
IMPLEMENT_FUNCTION(VLevelInfo, get_CompatNoTossDrops)  { vobjGetParamSelf(); RET_BOOL(Self ? Self->GetNoTossDropts() : false); }
IMPLEMENT_FUNCTION(VLevelInfo, get_CompatUseBlocking)  { vobjGetParamSelf(); RET_BOOL(Self ? (!!(Self->LevelInfoFlags2&LIF2_CompatUseBlocking)  || compat_useblocking.asBool()) : false); }
IMPLEMENT_FUNCTION(VLevelInfo, get_CompatNoDoorLight)  { vobjGetParamSelf(); RET_BOOL(Self ? (!!(Self->LevelInfoFlags2&LIF2_CompatNoDoorLight)  || compat_nodoorlight.asBool()) : false); }
IMPLEMENT_FUNCTION(VLevelInfo, get_CompatRavenScroll)  { vobjGetParamSelf(); RET_BOOL(Self ? (!!(Self->LevelInfoFlags2&LIF2_CompatRavenScroll)  || compat_ravenscroll.asBool()) : false); }
IMPLEMENT_FUNCTION(VLevelInfo, get_CompatSoundTarget)  { vobjGetParamSelf(); RET_BOOL(Self ? (!!(Self->LevelInfoFlags2&LIF2_CompatSoundTarget)  || compat_soundtarget.asBool()) : false); }
IMPLEMENT_FUNCTION(VLevelInfo, get_CompatDehHealth)    { vobjGetParamSelf(); RET_BOOL(Self ? (!!(Self->LevelInfoFlags2&LIF2_CompatDehHealth)    || compat_dehhealth.asBool()) : false); }
IMPLEMENT_FUNCTION(VLevelInfo, get_CompatTrace)        { vobjGetParamSelf(); RET_BOOL(Self ? (!!(Self->LevelInfoFlags2&LIF2_CompatTrace)        || compat_trace.asBool()) : false); }
IMPLEMENT_FUNCTION(VLevelInfo, get_CompatDropOff)      { vobjGetParamSelf(); RET_BOOL(Self ? (!!(Self->LevelInfoFlags2&LIF2_CompatDropOff)      || compat_dropoff.asBool()) : false); }
IMPLEMENT_FUNCTION(VLevelInfo, get_CompatBoomScroll)   { vobjGetParamSelf(); RET_BOOL(Self ? (!!(Self->LevelInfoFlags2&LIF2_CompatBoomScroll)   || compat_boomscroll.asBool()) : false); }
IMPLEMENT_FUNCTION(VLevelInfo, get_CompatInvisibility) { vobjGetParamSelf(); RET_BOOL(Self ? (!!(Self->LevelInfoFlags2&LIF2_CompatInvisibility) || compat_invisibility.asBool()) : false); }
