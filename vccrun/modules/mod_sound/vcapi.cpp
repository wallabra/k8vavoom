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
#include "sound.h"
#include "sound_private.h"


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_CLASS(V, SoundSFXManager);


// ////////////////////////////////////////////////////////////////////////// //
// static native final int AddSound (name tagName, string filename); // won't replace
IMPLEMENT_FUNCTION(VSoundSFXManager, AddSound) {
  P_GET_STR(filename);
  P_GET_NAME(tagName);
  int res = 0;
  if (GSoundManager) res = GSoundManager->AddSound(tagName, filename);
  RET_INT(res);
}


// static native final int FindSound (name tagName);
IMPLEMENT_FUNCTION(VSoundSFXManager, FindSound) {
  P_GET_NAME(tagName);
  int res = 0;
  if (GSoundManager) res = GSoundManager->FindSound(tagName);
  RET_INT(res);
}


#define IMPLEMENT_VSS_SND_PROPERTY(atype,name) \
  IMPLEMENT_FUNCTION(VSoundSFXManager, SetSound##name) { \
    P_GET_##atype(v); \
    P_GET_NAME(tagName); \
    if (GSoundManager) GSoundManager->SetSound##name(tagName, v); \
  } \
  \
  IMPLEMENT_FUNCTION(VSoundSFXManager, GetSound##name) { \
    P_GET_NAME(tagName); \
    if (GSoundManager) RET_##atype(GSoundManager->GetSound##name(tagName)); else RET_##atype(0); \
  }

IMPLEMENT_VSS_SND_PROPERTY(INT, Priority)
IMPLEMENT_VSS_SND_PROPERTY(INT, Channels)
IMPLEMENT_VSS_SND_PROPERTY(FLOAT, RandomPitch)
IMPLEMENT_VSS_SND_PROPERTY(BOOL, Singular)

#undef IMPLEMENT_VSS_SND_PROPERTY


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_CLASS(V, SoundSystem);


// ////////////////////////////////////////////////////////////////////////// //
// returns `none` on error
// static native final static void Initialize ();
IMPLEMENT_FUNCTION(VSoundSystem, Initialize) {
  VSoundManager::StaticInitialize();
}


// static native final static void Shutdown ();
IMPLEMENT_FUNCTION(VSoundSystem, Shutdown) {
  VSoundManager::StaticShutdown();
}


// static native final static void IsInitialized ();
IMPLEMENT_FUNCTION(VSoundSystem, get_IsInitialized) {
  RET_BOOL(!!GAudio);
}


// static native final int PlaySound (int sound_id, const TVec origin, optional const TVec velocity,
//   int origin_id, optional int channel, optional float volume, optional float attenuation, optional float pitch,
//    optional bool loop);
IMPLEMENT_FUNCTION(VSoundSystem, PlaySound) {
  P_GET_BOOL_OPT(loop, false);
  P_GET_FLOAT_OPT(pitch, 1.0);
  P_GET_FLOAT_OPT(attenuation, 1.0);
  P_GET_FLOAT_OPT(volume, 1.0);
  P_GET_INT_OPT(channel, -1);
  P_GET_INT(origin_id);
  P_GET_VEC_OPT(velocity, TVec(0, 0, 0));
  P_GET_VEC(origin);
  P_GET_INT(sndid);
  int res = -1;
  if (GAudio) res = GAudio->PlaySound(sndid, origin, velocity, origin_id, channel, volume, attenuation, pitch, loop);
  RET_INT(res);
}


// static native final void StopChannel (int origin_id, int channel);
IMPLEMENT_FUNCTION(VSoundSystem, StopChannel) {
  P_GET_INT(channel);
  P_GET_INT(origin_id);
  if (GAudio) GAudio->StopChannel(origin_id, channel);
}


// static native final void StopSound (int origin_id, int sound_id);
IMPLEMENT_FUNCTION(VSoundSystem, StopSound) {
  P_GET_INT(sound_id);
  P_GET_INT(origin_id);
  if (GAudio) GAudio->StopSound(origin_id, sound_id);
}


// static native final void StopSounds ();
IMPLEMENT_FUNCTION(VSoundSystem, StopSounds) {
  if (GAudio) GAudio->StopSounds();
}


// static native final bool IsSoundActive (int origin_id, int sound_id);
IMPLEMENT_FUNCTION(VSoundSystem, IsSoundActive) {
  P_GET_INT(sound_id);
  P_GET_INT(origin_id);
  bool res = false;
  if (GAudio) res = GAudio->IsSoundActive(origin_id, sound_id);
  RET_BOOL(res);
}

// static native final bool IsSoundPaused (int origin_id, int sound_id);
IMPLEMENT_FUNCTION(VSoundSystem, IsSoundPaused) {
  P_GET_INT(sound_id);
  P_GET_INT(origin_id);
  bool res = false;
  if (GAudio) res = GAudio->IsSoundPaused(origin_id, sound_id);
  RET_BOOL(res);
}

// static native final void PauseSound (int origin_id, int sound_id);
IMPLEMENT_FUNCTION(VSoundSystem, PauseSound) {
  P_GET_INT(sound_id);
  P_GET_INT(origin_id);
  if (GAudio) GAudio->PauseSound(origin_id, sound_id);
}

// static native final void ResumeSound (int origin_id, int sound_id);
IMPLEMENT_FUNCTION(VSoundSystem, ResumeSound) {
  P_GET_INT(sound_id);
  P_GET_INT(origin_id);
  if (GAudio) GAudio->ResumeSound(origin_id, sound_id);
}


// static native final bool IsChannelActive (int origin_id, int channel);
IMPLEMENT_FUNCTION(VSoundSystem, IsChannelActive) {
  P_GET_INT(channel);
  P_GET_INT(origin_id);
  bool res = false;
  if (GAudio) res = GAudio->IsChannelActive(origin_id, channel);
  RET_BOOL(res);
}

// static native final bool IsChannelPaused (int origin_id, int channel);
IMPLEMENT_FUNCTION(VSoundSystem, IsChannelPaused) {
  P_GET_INT(channel);
  P_GET_INT(origin_id);
  bool res = false;
  if (GAudio) res = GAudio->IsChannelPaused(origin_id, channel);
  RET_BOOL(res);
}

// static native final void PauseChannel (int origin_id, int channel);
IMPLEMENT_FUNCTION(VSoundSystem, PauseChannel) {
  P_GET_INT(channel);
  P_GET_INT(origin_id);
  if (GAudio) GAudio->PauseChannel(origin_id, channel);
}
// static native final void ResumeChannel (int origin_id, int channel);
IMPLEMENT_FUNCTION(VSoundSystem, ResumeChannel) {
  P_GET_INT(channel);
  P_GET_INT(origin_id);
  if (GAudio) GAudio->ResumeChannel(origin_id, channel);
}


// static native final void PauseSounds ();
IMPLEMENT_FUNCTION(VSoundSystem, PauseSounds) {
  if (GAudio) GAudio->PauseSounds();
}

// static native final void ResumeSounds ();
IMPLEMENT_FUNCTION(VSoundSystem, ResumeSounds) {
  if (GAudio) GAudio->ResumeSounds();
}


#define IMPLEMENT_VSS_SETSC_PROPERTY(atype,name) \
IMPLEMENT_FUNCTION(VSoundSystem, SetSound##name) { P_GET_##atype(v); P_GET_INT(sc); P_GET_INT(oid); if (GAudio) GAudio->SetSound##name(oid, sc, v); } \
IMPLEMENT_FUNCTION(VSoundSystem, SetChannel##name) { P_GET_##atype(v); P_GET_INT(sc); P_GET_INT(oid); if (GAudio) GAudio->SetChannel##name(oid, sc, v); }

IMPLEMENT_VSS_SETSC_PROPERTY(FLOAT, Pitch)
IMPLEMENT_VSS_SETSC_PROPERTY(VEC, Origin)
IMPLEMENT_VSS_SETSC_PROPERTY(VEC, Velocity)
IMPLEMENT_VSS_SETSC_PROPERTY(FLOAT, Volume)
IMPLEMENT_VSS_SETSC_PROPERTY(FLOAT, Attenuation)

#undef IMPLEMENT_VSS_SETSC_PROPERTY


// static native final int FindInternalChannelForSound (int origin_id, int sound_id) = 0;
IMPLEMENT_FUNCTION(VSoundSystem, FindInternalChannelForSound) {
  P_GET_INT(sid);
  P_GET_INT(oid);
  int res = -1;
  if (GAudio) res = GAudio->FindInternalChannelForSound(oid, sid);
  RET_INT(res);
}

// static native final int FindInternalChannelForChannel (int origin_id, int channel) = 0;
IMPLEMENT_FUNCTION(VSoundSystem, FindInternalChannelForChannel) {
  P_GET_INT(cid);
  P_GET_INT(oid);
  int res = -1;
  if (GAudio) res = GAudio->FindInternalChannelForChannel(oid, cid);
  RET_INT(res);
}

// static native final bool IsInternalChannelPlaying (int ichannel); // paused channels considered "playing"
IMPLEMENT_FUNCTION(VSoundSystem, IsInternalChannelPlaying) {
  P_GET_INT(ichannel);
  bool res = false;
  if (GAudio) res = GAudio->IsInternalChannelPlaying(ichannel);
  RET_BOOL(res);
}

// static native final bool IsInternalChannelPaused (int ichannel);
IMPLEMENT_FUNCTION(VSoundSystem, IsInternalChannelPaused) {
  P_GET_INT(ichannel);
  bool res = false;
  if (GAudio) res = GAudio->IsInternalChannelPaused(ichannel);
  RET_BOOL(res);
}

// static native final void StopInternalChannel (int ichannel);
IMPLEMENT_FUNCTION(VSoundSystem, StopInternalChannel) {
  P_GET_INT(ichannel);
  if (GAudio) GAudio->StopInternalChannel(ichannel);
}

// static native final void PauseInternalChannel (int ichannel);
IMPLEMENT_FUNCTION(VSoundSystem, PauseInternalChannel) {
  P_GET_INT(ichannel);
  if (GAudio) GAudio->PauseInternalChannel(ichannel);
}

// static native final void ResumeInternalChannel (int ichannel);
IMPLEMENT_FUNCTION(VSoundSystem, ResumeInternalChannel) {
  P_GET_INT(ichannel);
  if (GAudio) GAudio->ResumeInternalChannel(ichannel);
}

// static native final bool IsInternalChannelRelative (int ichannel);
IMPLEMENT_FUNCTION(VSoundSystem, IsInternalChannelRelative) {
  P_GET_INT(ichannel);
  bool res = false;
  if (GAudio) res = GAudio->IsInternalChannelRelative(ichannel);
  RET_BOOL(res);
}

// static native final void SetInternalChannelRelative (int ichannel, bool relative);
IMPLEMENT_FUNCTION(VSoundSystem, SetInternalChannelRelative) {
  P_GET_BOOL(relative);
  P_GET_INT(ichannel);
  if (GAudio) GAudio->SetInternalChannelRelative(ichannel, relative);
}


IMPLEMENT_FUNCTION(VSoundSystem, LockUpdates) {
  if (GAudio) GAudio->LockUpdates();
}

IMPLEMENT_FUNCTION(VSoundSystem, UnlockUpdates) {
  if (GAudio) GAudio->UnlockUpdates();
}


#define IMPLEMENT_VSS_LISTENER_PROPERTY(name) \
  IMPLEMENT_FUNCTION(VSoundSystem, set_Listener##name) { \
    P_GET_VEC(v); \
    if (GAudio) { \
      GAudio->LockUpdates(); \
      GAudio->Listener##name = v; \
      GAudio->UnlockUpdates(); \
    } \
  } \
  \
  IMPLEMENT_FUNCTION(VSoundSystem, get_Listener##name) { \
    if (GAudio) RET_VEC(GAudio->Listener##name); else RET_VEC(TVec(0, 0, 0)); \
  }

IMPLEMENT_VSS_LISTENER_PROPERTY(Origin)
IMPLEMENT_VSS_LISTENER_PROPERTY(Velocity)
IMPLEMENT_VSS_LISTENER_PROPERTY(Forward)
IMPLEMENT_VSS_LISTENER_PROPERTY(Up)

#undef IMPLEMENT_VSS_LISTENER_PROPERTY


IMPLEMENT_FUNCTION(VSoundSystem, get_SoundVolume) {
  RET_FLOAT(VAudioPublic::snd_sfx_volume);
}

IMPLEMENT_FUNCTION(VSoundSystem, set_SoundVolume) {
  P_GET_FLOAT(v);
  if (v < 0) v = 0; else if (v > 1) v = 1;
  if (GAudio) GAudio->LockUpdates();
  VAudioPublic::snd_sfx_volume = v;
  if (GAudio) GAudio->UnlockUpdates();
}

IMPLEMENT_FUNCTION(VSoundSystem, get_MusicVolume) {
  RET_FLOAT(VAudioPublic::snd_music_volume);
}

IMPLEMENT_FUNCTION(VSoundSystem, set_MusicVolume) {
  P_GET_FLOAT(v);
  if (v < 0) v = 0; else if (v > 1) v = 1;
  if (GAudio) GAudio->LockUpdates();
  VAudioPublic::snd_music_volume = v;
  if (GAudio) GAudio->UnlockUpdates();
}

IMPLEMENT_FUNCTION(VSoundSystem, get_SwapStereo) {
  RET_BOOL(VAudioPublic::snd_swap_stereo);
}

IMPLEMENT_FUNCTION(VSoundSystem, set_SwapStereo) {
  P_GET_BOOL(v);
  if (GAudio) GAudio->LockUpdates();
  VAudioPublic::snd_swap_stereo = v;
  if (GAudio) GAudio->UnlockUpdates();
}


// static native final bool PlayMusic (string filename, optional bool Loop);
IMPLEMENT_FUNCTION(VSoundSystem, PlayMusic) {
  P_GET_BOOL_OPT(loop, false);
  P_GET_STR(filename);
  if (GAudio) {
    RET_BOOL(GAudio->PlayMusic(filename, loop));
  } else {
    RET_BOOL(false);
  }
}

// static native final bool IsMusicPlaying ();
IMPLEMENT_FUNCTION(VSoundSystem, IsMusicPlaying) {
  RET_BOOL(GAudio ? GAudio->IsMusicPlaying() : false);
}

// static native final void PauseMusic ();
IMPLEMENT_FUNCTION(VSoundSystem, PauseMusic) {
  if (GAudio) GAudio->PauseMusic();
}

// static native final void ResumeMusic ();
IMPLEMENT_FUNCTION(VSoundSystem, ResumeMusic) {
  if (GAudio) GAudio->ResumeMusic();
}

// static native final void StopMusic ();
IMPLEMENT_FUNCTION(VSoundSystem, StopMusic) {
  if (GAudio) GAudio->StopMusic();
}


// static native final void SetMusicPitch (float pitch);
IMPLEMENT_FUNCTION(VSoundSystem, SetMusicPitch) {
  P_GET_FLOAT(pitch);
  if (GAudio) GAudio->SetMusicPitch(pitch);
}


#define IMPLEMENT_VSS_PROPERTY(atype,name,varname) \
IMPLEMENT_FUNCTION(VSoundSystem, get_##name) { RET_##atype(varname); } \
IMPLEMENT_FUNCTION(VSoundSystem, set_##name) { P_GET_##atype(v); varname = v; }

IMPLEMENT_VSS_PROPERTY(FLOAT, DopplerFactor, VOpenALDevice::doppler_factor)
IMPLEMENT_VSS_PROPERTY(FLOAT, DopplerVelocity, VOpenALDevice::doppler_velocity)
IMPLEMENT_VSS_PROPERTY(FLOAT, RolloffFactor, VOpenALDevice::rolloff_factor)
IMPLEMENT_VSS_PROPERTY(FLOAT, ReferenceDistance, VOpenALDevice::reference_distance)
IMPLEMENT_VSS_PROPERTY(FLOAT, MaxDistance, VOpenALDevice::max_distance)

#undef IMPLEMENT_VSS_PROPERTY

IMPLEMENT_FUNCTION(VSoundSystem, get_NumChannels) { RET_INT(VAudioPublic::snd_channels); }

IMPLEMENT_FUNCTION(VSoundSystem, set_NumChannels) {
  P_GET_INT(v);
  if (GAudio || v < 1 || v > 256) return;
  VAudioPublic::snd_channels = v;
}


IMPLEMENT_FUNCTION(VSoundSystem, get_MaxHearingDistance) { RET_INT(VAudioPublic::snd_max_distance); }

IMPLEMENT_FUNCTION(VSoundSystem, set_MaxHearingDistance) {
  P_GET_INT(v);
  if (GAudio || v < 0) return;
  if (v > 0x000fffff) v = 0x000fffff;
  if (GAudio) GAudio->LockUpdates();
  VAudioPublic::snd_max_distance = v;
  if (GAudio) GAudio->UnlockUpdates();
}


// ////////////////////////////////////////////////////////////////////////// //
static void buildDevList (VScriptArray *arr, const ALCchar *list) {
  auto type = VFieldType(TYPE_String);
  if (!list || !list[0]) {
    arr->Clear(type);
  } else {
    int count = 0;
    const ALCchar *tmp = list;
    while (*tmp) {
      ++count;
      while (*tmp) ++tmp;
      ++tmp;
    }
    arr->SetNum(count, type);
    VStr *aptr = (VStr *)arr->Ptr();
    tmp = list;
    while (*tmp) {
      *aptr++ = VStr(tmp);
      while (*tmp) ++tmp;
      ++tmp;
    }
  }
}


static void buildExtList (VScriptArray *arr, const ALCchar *list) {
  auto type = VFieldType(TYPE_String);
  if (!list || !list[0]) {
    arr->Clear(type);
  } else {
    int count = 0;
    const ALCchar *tmp = list;
    while (*tmp) {
      while ((vuint8)*tmp <= 32) ++tmp;
      if (!tmp[0]) break;
      ++count;
      while ((vuint8)*tmp > 32) ++tmp;
      if (*tmp) ++tmp;
    }
    arr->SetNum(count, type);
    VStr *aptr = (VStr *)arr->Ptr();
    tmp = list;
    while (*tmp) {
      while ((vuint8)*tmp <= 32) ++tmp;
      if (!tmp[0]) break;
      auto end = tmp+1;
      while ((vuint8)*end > 32) ++end;
      *aptr++ = VStr(tmp, (int)(intptr_t)(end-tmp));
      tmp = end;
      if (*tmp) ++tmp;
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// static native final void getDeviceList (out array!string list);
IMPLEMENT_FUNCTION(VSoundSystem, getDeviceList) {
  P_GET_PTR(VScriptArray, arr);
  const char *list;
  if (GAudio) {
    list = GAudio->GetDevList();
  } else {
    (void)alcGetError(nullptr);
    list = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
    ALCenum err = alcGetError(nullptr);
    if (err != ALC_NO_ERROR) list = nullptr;
  }
  buildDevList(arr, list);
}


// static native final void getPhysDeviceList (out array!string list);
IMPLEMENT_FUNCTION(VSoundSystem, getPhysDeviceList) {
  P_GET_PTR(VScriptArray, arr);
  const char *list;
  if (GAudio) {
    list = GAudio->GetAllDevList();
  } else {
    (void)alcGetError(nullptr);
    list = alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);
    ALCenum err = alcGetError(nullptr);
    if (err != ALC_NO_ERROR) list = nullptr;
  }
  buildDevList(arr, list);
}


// static native final void getExtensionsList (out array!string list);
IMPLEMENT_FUNCTION(VSoundSystem, getExtensionsList) {
  P_GET_PTR(VScriptArray, arr);
  const char *list;
  if (GAudio) {
    list = GAudio->GetExtList();
  } else {
    (void)alcGetError(nullptr);
    list = alcGetString(NULL, ALC_EXTENSIONS);
    ALCenum err = alcGetError(nullptr);
    if (err != ALC_NO_ERROR) list = nullptr;
  }
  buildExtList(arr, list);
}
