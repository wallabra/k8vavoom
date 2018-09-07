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
// included from "sound.h"


// ////////////////////////////////////////////////////////////////////////// //
class VSoundSFXManager : public VObject {
  DECLARE_CLASS(VSoundSFXManager, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VSoundSFXManager)

public:
  DECLARE_FUNCTION(AddSound)
  DECLARE_FUNCTION(FindSound)

  DECLARE_FUNCTION(GetSoundPriority)
  DECLARE_FUNCTION(SetSoundPriority)

  DECLARE_FUNCTION(GetSoundChannels)
  DECLARE_FUNCTION(SetSoundChannels)

  DECLARE_FUNCTION(GetSoundRandomPitch)
  DECLARE_FUNCTION(SetSoundRandomPitch)

  DECLARE_FUNCTION(GetSoundSingular)
  DECLARE_FUNCTION(SetSoundSingular)
};


// ////////////////////////////////////////////////////////////////////////// //
class VSoundSystem : public VObject {
  DECLARE_CLASS(VSoundSystem, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VSoundSystem)

public:
  //virtual void Destroy () override;

public:
  DECLARE_FUNCTION(Initialize)
  DECLARE_FUNCTION(Shutdown)
  DECLARE_FUNCTION(get_IsInitialized)
  //DECLARE_FUNCTION(Destroy)

  DECLARE_FUNCTION(get_MaxHearingDistance)
  DECLARE_FUNCTION(set_MaxHearingDistance)

  DECLARE_FUNCTION(IsSoundActive)
  DECLARE_FUNCTION(IsSoundPaused)
  DECLARE_FUNCTION(PlaySound)
  DECLARE_FUNCTION(StopSound)
  DECLARE_FUNCTION(PauseSound)
  DECLARE_FUNCTION(ResumeSound)

  DECLARE_FUNCTION(SetSoundPitch)
  DECLARE_FUNCTION(SetSoundOrigin)
  DECLARE_FUNCTION(SetSoundVelocity)
  DECLARE_FUNCTION(SetSoundVolume)
  DECLARE_FUNCTION(SetSoundAttenuation)

  DECLARE_FUNCTION(IsChannelActive)
  DECLARE_FUNCTION(IsChannelPaused)
  DECLARE_FUNCTION(PauseChannel)
  DECLARE_FUNCTION(ResumeChannel)

  DECLARE_FUNCTION(SetChannelPitch)
  DECLARE_FUNCTION(SetChannelOrigin)
  DECLARE_FUNCTION(SetChannelVelocity)
  DECLARE_FUNCTION(SetChannelVolume)
  DECLARE_FUNCTION(SetChannelAttenuation)

  DECLARE_FUNCTION(StopChannel)

  DECLARE_FUNCTION(StopSounds)
  DECLARE_FUNCTION(PauseSounds)
  DECLARE_FUNCTION(ResumeSounds)

  DECLARE_FUNCTION(FindInternalChannelForSound)
  DECLARE_FUNCTION(FindInternalChannelForChannel)

  DECLARE_FUNCTION(IsInternalChannelPlaying)
  DECLARE_FUNCTION(IsInternalChannelPaused)
  DECLARE_FUNCTION(StopInternalChannel)
  DECLARE_FUNCTION(PauseInternalChannel)
  DECLARE_FUNCTION(ResumeInternalChannel)
  DECLARE_FUNCTION(IsInternalChannelRelative)
  DECLARE_FUNCTION(SetInternalChannelRelative)

  DECLARE_FUNCTION(LockUpdates)
  DECLARE_FUNCTION(UnlockUpdates)

  DECLARE_FUNCTION(get_ListenerOrigin)
  DECLARE_FUNCTION(get_ListenerVelocity)
  DECLARE_FUNCTION(get_ListenerForward)
  DECLARE_FUNCTION(get_ListenerUp)

  DECLARE_FUNCTION(set_ListenerOrigin)
  DECLARE_FUNCTION(set_ListenerVelocity)
  DECLARE_FUNCTION(set_ListenerForward)
  DECLARE_FUNCTION(set_ListenerUp)

  DECLARE_FUNCTION(get_SoundVolume)
  DECLARE_FUNCTION(set_SoundVolume)

  DECLARE_FUNCTION(get_MusicVolume)
  DECLARE_FUNCTION(set_MusicVolume)

  DECLARE_FUNCTION(get_SwapStereo)
  DECLARE_FUNCTION(set_SwapStereo)

  DECLARE_FUNCTION(PlayMusic)
  DECLARE_FUNCTION(IsMusicPlaying)
  DECLARE_FUNCTION(PauseMusic)
  DECLARE_FUNCTION(ResumeMusic)
  DECLARE_FUNCTION(StopMusic)
  DECLARE_FUNCTION(SetMusicPitch)

  DECLARE_FUNCTION(get_RolloffFactor)
  DECLARE_FUNCTION(set_RolloffFactor)
  DECLARE_FUNCTION(get_ReferenceDistance)
  DECLARE_FUNCTION(set_ReferenceDistance)
  DECLARE_FUNCTION(get_MaxDistance)
  DECLARE_FUNCTION(set_MaxDistance)

  DECLARE_FUNCTION(get_DopplerFactor)
  DECLARE_FUNCTION(set_DopplerFactor)
  DECLARE_FUNCTION(get_DopplerVelocity)
  DECLARE_FUNCTION(set_DopplerVelocity)

  DECLARE_FUNCTION(get_NumChannels)
  DECLARE_FUNCTION(set_NumChannels)

  DECLARE_FUNCTION(getDeviceList)
  DECLARE_FUNCTION(getPhysDeviceList)
  DECLARE_FUNCTION(getExtensionsList)
};
