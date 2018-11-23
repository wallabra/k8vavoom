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
  ACS_EXTFUNC(GetActorViewHeight)
  ACS_EXTFUNC(GetChar) // implemented
  ACS_EXTFUNC(GetAirSupply) // ignored
  ACS_EXTFUNC(SetAirSupply) // ignored
  ACS_EXTFUNC(SetSkyScrollSpeed) // ignored
  ACS_EXTFUNC(GetArmorType)
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
  ACS_EXTFUNC(SoundSequenceOnActor)
  ACS_EXTFUNC(SoundSequenceOnSector)
  ACS_EXTFUNC(SoundSequenceOnPolyobj)
  ACS_EXTFUNC(GetPolyobjX)
  ACS_EXTFUNC(GetPolyobjY)
  ACS_EXTFUNC(CheckSight)
  ACS_EXTFUNC(SpawnForced) // implemented
  ACS_EXTFUNC(AnnouncerSound) // skulltag, ignored
  ACS_EXTFUNC(SetPointer) // partially implemented
  ACS_EXTFUNC(ACS_NamedExecute) // implemented
  ACS_EXTFUNC(ACS_NamedSuspend)
  ACS_EXTFUNC(ACS_NamedTerminate)
  ACS_EXTFUNC(ACS_NamedLockedExecute)
  ACS_EXTFUNC(ACS_NamedLockedExecuteDoor)
  ACS_EXTFUNC(ACS_NamedExecuteWithResult) // implemented
  ACS_EXTFUNC(ACS_NamedExecuteAlways) // implemented
  ACS_EXTFUNC(UniqueTID) // implemented
  ACS_EXTFUNC(IsTIDUsed) // implemented
  ACS_EXTFUNC(Sqrt) // implemented
  ACS_EXTFUNC(FixedSqrt) // implemented
  ACS_EXTFUNC(VectorLength) // implemented
  ACS_EXTFUNC(SetHUDClipRect)
  ACS_EXTFUNC(SetHUDWrapWidth)
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
  ACS_EXTFUNC(GetWeapon)
  ACS_EXTFUNC(SoundVolume)
  ACS_EXTFUNC(PlayActorSound)
  ACS_EXTFUNC(SpawnDecal)
  ACS_EXTFUNC(CheckFont)
  ACS_EXTFUNC(DropItem)
  ACS_EXTFUNC(CheckFlag) // implemented
  ACS_EXTFUNC(SetLineActivation)
  ACS_EXTFUNC(GetLineActivation)
  ACS_EXTFUNC(GetActorPowerupTics)
  ACS_EXTFUNC(ChangeActorAngle) // implemented
  ACS_EXTFUNC(ChangeActorPitch) // 80; implemented
  ACS_EXTFUNC(GetArmorInfo)
  ACS_EXTFUNC(DropInventory)
  ACS_EXTFUNC(PickActor) // implemented
  ACS_EXTFUNC(IsPointerEqual)
  ACS_EXTFUNC(CanRaiseActor)
  ACS_EXTFUNC(SetActorTeleFog) // 86
  ACS_EXTFUNC(SwapActorTeleFog)
  ACS_EXTFUNC(SetActorRoll) // implemented
  ACS_EXTFUNC(ChangeActorRoll) // implemented
  ACS_EXTFUNC(GetActorRoll)
  ACS_EXTFUNC(QuakeEx)
  ACS_EXTFUNC(Warp) // 92
  ACS_EXTFUNC(GetMaxInventory)
  ACS_EXTFUNC(SetSectorDamage)
  ACS_EXTFUNC(SetSectorTerrain)
  ACS_EXTFUNC(SpawnParticle)
  ACS_EXTFUNC(SetMusicVolume) // ignored
  ACS_EXTFUNC(CheckProximity)
  ACS_EXTFUNC(CheckActorState) // 99
  /* Zandronum's - these must be skipped when we reach 99!
  -100:ResetMap(0)
  -101 : PlayerIsSpectator(1)
  -102 : ConsolePlayerNumber(0)
  -103 : GetTeamProperty(2)
  -104 : GetPlayerLivesLeft(1)
  -105 : SetPlayerLivesLeft(2)
  -106 : KickFromGame(2)
  */

  ACS_EXTFUNC_NUM(ResetMap_Zadro, 100)
  ACS_EXTFUNC(PlayerIsSpectator_Zadro) // implemented
  ACS_EXTFUNC(ConsolePlayerNumber_Zadro) // implemented
  ACS_EXTFUNC(GetTeamProperty_Zadro) // [Dusk]
  ACS_EXTFUNC(GetPlayerLivesLeft_Zadro)
  ACS_EXTFUNC(SetPlayerLivesLeft_Zadro)
  ACS_EXTFUNC(ForceToSpectate_Zadro)
  ACS_EXTFUNC(GetGamemodeState_Zadro)
  ACS_EXTFUNC(SetDBEntry_Zadro)
  ACS_EXTFUNC(GetDBEntry_Zadro)
  ACS_EXTFUNC(SetDBEntryString_Zadro)
  ACS_EXTFUNC(GetDBEntryString_Zadro)
  ACS_EXTFUNC(IncrementDBEntry_Zadro)
  ACS_EXTFUNC(PlayerIsLoggedIn_Zadro) // ignored
  ACS_EXTFUNC(GetPlayerAccountName_Zadro) // ignored
  ACS_EXTFUNC(SortDBEntries_Zadro)
  ACS_EXTFUNC(CountDBResults_Zadro)
  ACS_EXTFUNC(FreeDBResults_Zadro)
  ACS_EXTFUNC(GetDBResultKeyString_Zadro)
  ACS_EXTFUNC(GetDBResultValueString_Zadro)
  ACS_EXTFUNC(GetDBResultValue_Zadro)
  ACS_EXTFUNC(GetDBEntryRank_Zadro)
  ACS_EXTFUNC(RequestScriptPuke_Zadro) // implemented
  ACS_EXTFUNC(BeginDBTransaction_Zadro)
  ACS_EXTFUNC(EndDBTransaction_Zadro)
  ACS_EXTFUNC(GetDBEntries_Zadro)
  ACS_EXTFUNC(NamedRequestScriptPuke_Zadro) // implemented
  ACS_EXTFUNC(SystemTime_Zadro)
  ACS_EXTFUNC(GetTimeProperty_Zadro)
  ACS_EXTFUNC(Strftime_Zadro)
  ACS_EXTFUNC(SetDeadSpectator_Zadro)
  ACS_EXTFUNC(SetActivatorToPlayer_Zadro)


  ACS_EXTFUNC_NUM(CheckClass, 200)
  ACS_EXTFUNC(DamageActor) // [arookas]
  ACS_EXTFUNC(SetActorFlag) // implemented
  ACS_EXTFUNC(SetTranslation)
  ACS_EXTFUNC(GetActorFloorTexture)
  ACS_EXTFUNC(GetActorFloorTerrain)
  ACS_EXTFUNC(StrArg)
  ACS_EXTFUNC(Floor)
  ACS_EXTFUNC(Round)
  ACS_EXTFUNC(Ceil)
  ACS_EXTFUNC(ScriptCall)
  ACS_EXTFUNC(StartSlideshow)

  // Eternity's
  ACS_EXTFUNC_NUM(GetLineX, 300)
  ACS_EXTFUNC(GetLineY)

  // OpenGL stuff
  ACS_EXTFUNC_NUM(SetSectorGlow, 400)
  ACS_EXTFUNC(SetFogDensity)

  // ZDaemon
  ACS_EXTFUNC_NUM(GetTeamScore, 19620) // (int team)
  ACS_EXTFUNC(SetTeamScore) // (int team, int value)
