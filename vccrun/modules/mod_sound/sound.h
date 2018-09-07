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
#ifndef VCCMOD_SOUND_HEADER_FILE
#define VCCMOD_SOUND_HEADER_FILE

#include "../../vcc_run.h"
#include "../../filesys/fsys.h"

#include "sound_private.h"


// ////////////////////////////////////////////////////////////////////////// //
// handles list of registered sound and sound sequences
class VSoundManager {
public:
  // the complete set of sound effects
  // WARNING! DON'T MODIFY!
  TArray<sfxinfo_t> S_sfx;

  VSoundManager ();
  ~VSoundManager ();

  void Init ();

  int AddSound (VName TagName, const VStr &filename); // won't replace
  // 0: not found
  int FindSound (VName tagName);

  // higher priority takes precendence (default: 0)
  int GetSoundPriority (VName tagName);
  void SetSoundPriority (VName tagName, int value);

  // total number of channels a sound type may occupy (0 is unlimited, default)
  int GetSoundChannels (VName tagName);
  void SetSoundChannels (VName tagName, int value);

  // if not zero, do pitch randomisation
  // random value between [-1..1] will be multiplied by this, and added to default pitch (1.0)
  float GetSoundRandomPitch (VName tagName);
  void SetSoundRandomPitch (VName tagName, float value);

  // if true, only one instance of this sound may play
  bool GetSoundSingular (VName tagName);
  void SetSoundSingular (VName tagName, bool value);

  static void StaticInitialize ();
  static void StaticShutdown ();

private:
  // mapping from sound name to sfx index
  TMap<VName, vint32> name2idx;

  bool LoadSound (int sound_id, const VStr &filename);
};


// ////////////////////////////////////////////////////////////////////////// //
// main audio management class
class VAudioPublic {
public:
  // lock updates before changing this!
  // TODO: wrap in API
  TVec ListenerForward;
  TVec ListenerUp;
  TVec ListenerOrigin;
  TVec ListenerVelocity;

public:
  VAudioPublic ();
  ~VAudioPublic ();

  // top level methods
  void Init ();
  void Shutdown ();

  // playback of sound effects
  int PlaySound (int sound_id, const TVec &origin, const TVec &velocity, int origin_id, int channel,
                         float volume, float attenuation, float pitch, bool Loop);

  bool IsSoundActive (int origin_id, int sound_id);
  bool IsSoundPaused (int origin_id, int sound_id);
  void StopSound (int origin_id, int sound_id);
  void PauseSound (int origin_id, int sound_id);
  void ResumeChannel (int origin_id, int channel);

  void SetSoundPitch (int origin_id, int sound_id, float pitch);
  void SetSoundOrigin (int origin_id, int sound_id, const TVec &origin);
  void SetSoundVelocity (int origin_id, int sound_id, const TVec &velocity);
  void SetSoundVolume (int origin_id, int sound_id, float volume);
  void SetSoundAttenuation (int origin_id, int sound_id, float attenuation);

  bool IsChannelActive (int origin_id, int channel);
  bool IsChannelPaused (int origin_id, int channel);
  void StopChannel (int origin_id, int channel);
  void PauseChannel (int origin_id, int channel);
  void ResumeSound (int origin_id, int sound_id);

  void SetChannelPitch (int origin_id, int channel, float pitch);
  void SetChannelOrigin (int origin_id, int channel, const TVec &origin);
  void SetChannelVelocity (int origin_id, int channel, const TVec &velocity);
  void SetChannelVolume (int origin_id, int channel, float volume);
  void SetChannelAttenuation (int origin_id, int channel, float attenuation);

  void StopSounds ();
  void PauseSounds ();
  void ResumeSounds ();

  // <0: not found
  int FindInternalChannelForSound (int origin_id, int sound_id);
  int FindInternalChannelForChannel (int origin_id, int channel);

  bool IsInternalChannelPlaying (int ichannel);
  bool IsInternalChannelPaused (int ichannel);
  void StopInternalChannel (int ichannel);
  void PauseInternalChannel (int ichannel);
  void ResumeInternalChannel (int ichannel);
  bool IsInternalChannelRelative (int ichannel);
  void SetInternalChannelRelative (int ichannel, bool relative);

  // music playback
  bool PlayMusic (const VStr &filename, bool Loop);
  bool IsMusicPlaying ();
  void PauseMusic ();
  void ResumeMusic ();
  void StopMusic ();
  void SetMusicPitch (float pitch);

  // balance this!
  void LockUpdates ();
  void UnlockUpdates ();

  const char *GetDevList ();
  const char *GetAllDevList ();
  const char *GetExtList ();

public:
  static float snd_sfx_volume;
  static float snd_music_volume;
  static bool snd_swap_stereo;
  static int snd_channels;
  static int snd_max_distance;

// k8: it is public to free me from fuckery with `friends`
public:
  mythread_mutex stpPingLock;
  mythread_cond stpPingCond;

  // hardware devices
  VOpenALDevice *SoundDevice;

  void UpdateSfx (float frameDelta);

private:
  enum { MAX_CHANNELS = 256-4 };
  enum { PRIORITY_MAX_ADJUST = 10 };

  mythread updateThread;
  bool updateThreadStarted;
  mythread_mutex lockListener;

  int updaterThreadLockFlag; // !0: updater thread is not updating anything

  // info about sounds currently playing
  struct FChannel {
    int origin_id;
    int channel;
    TVec origin;
    TVec velocity;
    int sound_id;
    int priority;
    float volume;
    float attenuation;
    int handle;
    bool localPlayerSound;
    bool loop;
    bool paused;
    float pitch;
    //
    float newPitch;
    float newVolume;
  };

  // used to determine sound priority, and to cut off too far-away sounds
  int MaxSoundDist;

  // stream music player
  bool StreamPlaying;
  VStreamMusicPlayer *StreamMusicPlayer;

  // list of currently playing sounds
  FChannel Channel[MAX_CHANNELS];
  int NumChannels;
  int SndCount;

  // sound effect helpers
  int GetChannel (int sound_id, int origin_id, int channel, int priority, bool loop);
  void StopChannelByNum (int chan_num);
  void PauseChannelByNum (int chan_num);
  void ResumeChannelByNum (int chan_num);
  bool IsChannelPausedByNum (int chan_num);

  void sendQuit ();
};


extern VSoundManager *GSoundManager;
extern VAudioPublic *GAudio;


// ////////////////////////////////////////////////////////////////////////// //
#include "vcapi.h"


#endif
