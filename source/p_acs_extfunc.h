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
  ACS_EXTFUNC(SetActivator)
  ACS_EXTFUNC(SetActivatorToTarget)
  ACS_EXTFUNC(GetActorViewHeight)
  ACS_EXTFUNC(GetChar)
  ACS_EXTFUNC(GetAirSupply)
  ACS_EXTFUNC(SetAirSupply)
  ACS_EXTFUNC(SetSkyScrollSpeed)
  ACS_EXTFUNC(GetArmorType)
  ACS_EXTFUNC(SpawnSpotForced) // implemented
  ACS_EXTFUNC(SpawnSpotFacingForced)
  ACS_EXTFUNC(CheckActorProperty)
  ACS_EXTFUNC(SetActorVelocity) // implemented
  ACS_EXTFUNC(SetUserVariable)
  ACS_EXTFUNC(GetUserVariable)
  ACS_EXTFUNC(Radius_Quake2)
  ACS_EXTFUNC(CheckActorClass) // implemented
  ACS_EXTFUNC(SetUserArray)
  ACS_EXTFUNC(GetUserArray)
  ACS_EXTFUNC(SoundSequenceOnActor)
  ACS_EXTFUNC(SoundSequenceOnSector)
  ACS_EXTFUNC(SoundSequenceOnPolyobj)
  ACS_EXTFUNC(GetPolyobjX)
  ACS_EXTFUNC(GetPolyobjY)
  ACS_EXTFUNC(CheckSight)
  ACS_EXTFUNC(SpawnForced)
  ACS_EXTFUNC(AnnouncerSound) // skulltag
  ACS_EXTFUNC(SetPointer)
  ACS_EXTFUNC(ACS_NamedExecute) // implemented
  ACS_EXTFUNC(ACS_NamedSuspend)
  ACS_EXTFUNC(ACS_NamedTerminate)
  ACS_EXTFUNC(ACS_NamedLockedExecute)
  ACS_EXTFUNC(ACS_NamedLockedExecuteDoor)
  ACS_EXTFUNC(ACS_NamedExecuteWithResult)
  ACS_EXTFUNC(ACS_NamedExecuteAlways)
  ACS_EXTFUNC(UniqueTID)
  ACS_EXTFUNC(IsTIDUsed)
  ACS_EXTFUNC(Sqrt)
  ACS_EXTFUNC(FixedSqrt)
  ACS_EXTFUNC(VectorLength)
  ACS_EXTFUNC(SetHUDClipRect)
  ACS_EXTFUNC(SetHUDWrapWidth)
  ACS_EXTFUNC(SetCVar)
  ACS_EXTFUNC(GetUserCVar)
  ACS_EXTFUNC(SetUserCVar)
  ACS_EXTFUNC(GetCVarString)
  ACS_EXTFUNC(SetCVarString)
  ACS_EXTFUNC(GetUserCVarString)
  ACS_EXTFUNC(SetUserCVarString)
  ACS_EXTFUNC(LineAttack)
  ACS_EXTFUNC(PlaySound) // implemented
  ACS_EXTFUNC(StopSound) // implemented
  ACS_EXTFUNC(strcmp)
  ACS_EXTFUNC(stricmp)
  ACS_EXTFUNC(StrLeft)
  ACS_EXTFUNC(StrRight)
  ACS_EXTFUNC(StrMid)
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
  ACS_EXTFUNC(ChangeActorAngle)
  ACS_EXTFUNC(ChangeActorPitch) // 80
  ACS_EXTFUNC(GetArmorInfo)
  ACS_EXTFUNC(DropInventory)
  ACS_EXTFUNC(PickActor)
  ACS_EXTFUNC(IsPointerEqual)
  ACS_EXTFUNC(CanRaiseActor)
  ACS_EXTFUNC(SetActorTeleFog) // 86
  ACS_EXTFUNC(SwapActorTeleFog)
  ACS_EXTFUNC(SetActorRoll)
  ACS_EXTFUNC(ChangeActorRoll)
  ACS_EXTFUNC(GetActorRoll)
  ACS_EXTFUNC(QuakeEx)
  ACS_EXTFUNC(Warp) // 92
  ACS_EXTFUNC(GetMaxInventory)
  ACS_EXTFUNC(SetSectorDamage)
  ACS_EXTFUNC(SetSectorTerrain)
  ACS_EXTFUNC(SpawnParticle)
  ACS_EXTFUNC(SetMusicVolume)
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
