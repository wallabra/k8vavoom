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
  ACS_EXTFUNC_NUM(GetLineUDMFInt, 1)
  ACS_EXTFUNC(GetLineUDMFFixed)
  ACS_EXTFUNC(GetThingUDMFInt)
  ACS_EXTFUNC(GetThingUDMFFixed)
  ACS_EXTFUNC(GetSectorUDMFInt)
  ACS_EXTFUNC(GetSectorUDMFFixed)
  ACS_EXTFUNC(GetSideUDMFInt)
  ACS_EXTFUNC(GetSideUDMFFixed)
  ACS_EXTFUNC(GetActorVelX) // implemented
  ACS_EXTFUNC(GetActorVelY) // implemented
  ACS_EXTFUNC(GetActorVelZ) // implemented
  ACS_EXTFUNC(SetActivator) // implemented
  ACS_EXTFUNC(SetActivatorToTarget) // implemented
  ACS_EXTFUNC(GetActorViewHeight) // implemented
  ACS_EXTFUNC(GetChar) // implemented
  ACS_EXTFUNC(GetAirSupply) // ignored
  ACS_EXTFUNC(SetAirSupply) // ignored
  ACS_EXTFUNC(SetSkyScrollSpeed) // ignored
  ACS_EXTFUNC(GetArmorType) // implemented
  ACS_EXTFUNC(SpawnSpotForced) // implemented
  ACS_EXTFUNC(SpawnSpotFacingForced) // implemented
  ACS_EXTFUNC(CheckActorProperty) // implemented
  ACS_EXTFUNC(SetActorVelocity) // implemented
  ACS_EXTFUNC(SetUserVariable) // implemented
  ACS_EXTFUNC(GetUserVariable) // implemented
  ACS_EXTFUNC(Radius_Quake2) // implemented
  ACS_EXTFUNC(CheckActorClass) // implemented
  ACS_EXTFUNC(SetUserArray) // implemented
  ACS_EXTFUNC(GetUserArray) // implemented
  ACS_EXTFUNC(SoundSequenceOnActor) // ignored
  ACS_EXTFUNC(SoundSequenceOnSector) // ignored
  ACS_EXTFUNC(SoundSequenceOnPolyobj) // ignored
  ACS_EXTFUNC(GetPolyobjX) // implemented
  ACS_EXTFUNC(GetPolyobjY) // implemented
  ACS_EXTFUNC(CheckSight) // implemented
  ACS_EXTFUNC(SpawnForced) // implemented
  ACS_EXTFUNC(AnnouncerSound) // skulltag, ignored
  ACS_EXTFUNC(SetPointer) // partially implemented
  ACS_EXTFUNC(ACS_NamedExecute) // implemented
  ACS_EXTFUNC(ACS_NamedSuspend) // implemented
  ACS_EXTFUNC(ACS_NamedTerminate) // implemented
  ACS_EXTFUNC(ACS_NamedLockedExecute) // implemented
  ACS_EXTFUNC(ACS_NamedLockedExecuteDoor) // implemented
  ACS_EXTFUNC(ACS_NamedExecuteWithResult) // implemented
  ACS_EXTFUNC(ACS_NamedExecuteAlways) // implemented
  ACS_EXTFUNC(UniqueTID) // implemented
  ACS_EXTFUNC(IsTIDUsed) // implemented
  ACS_EXTFUNC(Sqrt) // implemented
  ACS_EXTFUNC(FixedSqrt) // implemented
  ACS_EXTFUNC(VectorLength) // implemented
  ACS_EXTFUNC(SetHUDClipRect) // ignored
  ACS_EXTFUNC(SetHUDWrapWidth) // ignored
  ACS_EXTFUNC(SetCVar) // implemented
  ACS_EXTFUNC(GetUserCVar) // implemented
  ACS_EXTFUNC(SetUserCVar) // implemented
  ACS_EXTFUNC(GetCVarString) // implemented
  ACS_EXTFUNC(SetCVarString) // implemented
  ACS_EXTFUNC(GetUserCVarString) // implemented
  ACS_EXTFUNC(SetUserCVarString) // implemented
  ACS_EXTFUNC(LineAttack) // implemented
  ACS_EXTFUNC(PlaySound) // implemented
  ACS_EXTFUNC(StopSound) // implemented
  ACS_EXTFUNC(strcmp) // implemented
  ACS_EXTFUNC(stricmp) // implemented
  ACS_EXTFUNC(StrLeft) // implemented
  ACS_EXTFUNC(StrRight) // implemented
  ACS_EXTFUNC(StrMid) // implemented
  ACS_EXTFUNC(GetActorClass) // implemented
  ACS_EXTFUNC(GetWeapon) // implemented
  ACS_EXTFUNC(SoundVolume) // ignored
  ACS_EXTFUNC(PlayActorSound) // ignored
  ACS_EXTFUNC(SpawnDecal) // ignored
  ACS_EXTFUNC(CheckFont) // implemented partially
  ACS_EXTFUNC(DropItem) // implemented
  ACS_EXTFUNC(CheckFlag) // implemented
  ACS_EXTFUNC(SetLineActivation) // implemented
  ACS_EXTFUNC(GetLineActivation) // implemented
  ACS_EXTFUNC(GetActorPowerupTics) // implemented
  ACS_EXTFUNC(ChangeActorAngle) // implemented
  ACS_EXTFUNC(ChangeActorPitch) // 80; implemented
  ACS_EXTFUNC(GetArmorInfo) // partially implemented
  ACS_EXTFUNC(DropInventory) // implemented
  ACS_EXTFUNC(PickActor) // implemented
  ACS_EXTFUNC(IsPointerEqual) // implemented
  ACS_EXTFUNC(CanRaiseActor) // implemented
  ACS_EXTFUNC(SetActorTeleFog) // 86
  ACS_EXTFUNC(SwapActorTeleFog)
  ACS_EXTFUNC(SetActorRoll) // implemented
  ACS_EXTFUNC(ChangeActorRoll) // implemented
  ACS_EXTFUNC(GetActorRoll) // implemented
  ACS_EXTFUNC(QuakeEx) // routed to `Radius_Quake`
  ACS_EXTFUNC(Warp) // 92 implemented
  ACS_EXTFUNC(GetMaxInventory) // ignored, as does Zandronum 3.0
  ACS_EXTFUNC(SetSectorDamage) // implemented
  ACS_EXTFUNC(SetSectorTerrain) // ignored
  ACS_EXTFUNC(SpawnParticle) // ignored
  ACS_EXTFUNC(SetMusicVolume) // ignored
  ACS_EXTFUNC(CheckProximity) // implemented
  ACS_EXTFUNC(CheckActorState) // implemented 99
  /* Zandronum's - these must be skipped when we reach 99!
  -100:ResetMap(0)
  -101 : PlayerIsSpectator(1)
  -102 : ConsolePlayerNumber(0)
  -103 : GetTeamProperty(2)
  -104 : GetPlayerLivesLeft(1)
  -105 : SetPlayerLivesLeft(2)
  -106 : KickFromGame(2)
  */

  ACS_EXTFUNC_NUM(ResetMap_Zandro, 100)
  ACS_EXTFUNC(PlayerIsSpectator_Zandro) // ignored
  ACS_EXTFUNC(ConsolePlayerNumber_Zandro) // implemented
  ACS_EXTFUNC(GetTeamProperty_Zandro) // [Dusk]
  ACS_EXTFUNC(GetPlayerLivesLeft_Zandro) // ignored
  ACS_EXTFUNC(SetPlayerLivesLeft_Zandro) // ignored
  ACS_EXTFUNC(ForceToSpectate_Zandro)
  ACS_EXTFUNC(GetGamemodeState_Zandro)
  ACS_EXTFUNC(SetDBEntry_Zandro)
  ACS_EXTFUNC(GetDBEntry_Zandro)
  ACS_EXTFUNC(SetDBEntryString_Zandro)
  ACS_EXTFUNC(GetDBEntryString_Zandro)
  ACS_EXTFUNC(IncrementDBEntry_Zandro)
  ACS_EXTFUNC(PlayerIsLoggedIn_Zandro) // ignored
  ACS_EXTFUNC(GetPlayerAccountName_Zandro) // ignored
  ACS_EXTFUNC(SortDBEntries_Zandro)
  ACS_EXTFUNC(CountDBResults_Zandro)
  ACS_EXTFUNC(FreeDBResults_Zandro)
  ACS_EXTFUNC(GetDBResultKeyString_Zandro)
  ACS_EXTFUNC(GetDBResultValueString_Zandro)
  ACS_EXTFUNC(GetDBResultValue_Zandro)
  ACS_EXTFUNC(GetDBEntryRank_Zandro)
  ACS_EXTFUNC(RequestScriptPuke_Zandro) // implemented
  ACS_EXTFUNC(BeginDBTransaction_Zandro)
  ACS_EXTFUNC(EndDBTransaction_Zandro)
  ACS_EXTFUNC(GetDBEntries_Zandro)
  ACS_EXTFUNC(NamedRequestScriptPuke_Zandro) // implemented
  ACS_EXTFUNC(SystemTime_Zandro)
  ACS_EXTFUNC(GetTimeProperty_Zandro)
  ACS_EXTFUNC(Strftime_Zandro)
  ACS_EXTFUNC(SetDeadSpectator_Zandro)
  ACS_EXTFUNC(SetActivatorToPlayer_Zandro)


  ACS_EXTFUNC_NUM(CheckClass, 200) // implemented
  ACS_EXTFUNC(DamageActor) // [arookas]
  ACS_EXTFUNC(SetActorFlag) // implemented
  ACS_EXTFUNC(SetTranslation) // implemented
  ACS_EXTFUNC(GetActorFloorTexture) // implemented
  ACS_EXTFUNC(GetActorFloorTerrain) // implemented
  ACS_EXTFUNC(StrArg) // implemented
  ACS_EXTFUNC(Floor) // implemented
  ACS_EXTFUNC(Round) // implemented
  ACS_EXTFUNC(Ceil) // implemented
  ACS_EXTFUNC(ScriptCall) // ignored
  ACS_EXTFUNC(StartSlideshow)

  // Eternity's
  ACS_EXTFUNC_NUM(GetLineX, 300)
  ACS_EXTFUNC(GetLineY)

  // OpenGL stuff
  ACS_EXTFUNC_NUM(SetSectorGlow, 400) // ignored
  ACS_EXTFUNC(SetFogDensity) // ignored

  // ZDaemon
  ACS_EXTFUNC_NUM(GetTeamScore, 19620) // (int team)
  ACS_EXTFUNC(SetTeamScore) // (int team, int value)
