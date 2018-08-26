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

//#define DEBUG_CHAN_ALLOC

// ////////////////////////////////////////////////////////////////////////// //
// main audio management class
class VAudio : public VAudioPublic {
public:
  VAudio ();
  ~VAudio ();

  // top level methods
  virtual void Init () override;
  virtual void Shutdown () override;

  // playback of sound effects
  virtual int PlaySound (int sound_id, const TVec &origin, const TVec &velocity, int origin_id, int channel, float volume, float attenuation, float pitch, bool Loop) override;

  virtual bool IsSoundActive (int origin_id, int sound_id) override;
  virtual bool IsSoundPaused (int origin_id, int sound_id) override;
  virtual void StopSound (int origin_id, int sound_id) override;
  virtual void PauseSound (int origin_id, int sound_id) override;
  virtual void ResumeChannel (int origin_id, int channel) override;

  virtual void SetSoundPitch (int origin_id, int sound_id, float pitch) override;
  virtual void SetSoundOrigin (int origin_id, int sound_id, const TVec &origin) override;
  virtual void SetSoundVelocity (int origin_id, int sound_id, const TVec &velocity) override;
  virtual void SetSoundVolume (int origin_id, int sound_id, float volume) override;
  virtual void SetSoundAttenuation (int origin_id, int sound_id, float attenuation) override;

  virtual bool IsChannelActive (int origin_id, int channel) override;
  virtual bool IsChannelPaused (int origin_id, int channel) override;
  virtual void StopChannel (int origin_id, int channel) override;
  virtual void PauseChannel (int origin_id, int channel) override;
  virtual void ResumeSound (int origin_id, int sound_id) override;

  virtual void SetChannelPitch (int origin_id, int channel, float pitch) override;
  virtual void SetChannelOrigin (int origin_id, int channel, const TVec &origin) override;
  virtual void SetChannelVelocity (int origin_id, int channel, const TVec &velocity) override;
  virtual void SetChannelVolume (int origin_id, int channel, float volume) override;
  virtual void SetChannelAttenuation (int origin_id, int channel, float attenuation) override;

  virtual void StopSounds () override;
  virtual void PauseSounds () override;
  virtual void ResumeSounds () override;

  // <0: not found
  virtual int FindInternalChannelForSound (int origin_id, int sound_id) override;
  virtual int FindInternalChannelForChannel (int origin_id, int channel) override;

  virtual bool IsInternalChannelPlaying (int ichannel) override;
  virtual bool IsInternalChannelPaused (int ichannel) override;
  virtual void StopInternalChannel (int ichannel) override;
  virtual void PauseInternalChannel (int ichannel) override;
  virtual void ResumeInternalChannel (int ichannel) override;
  virtual bool IsInternalChannelRelative (int ichannel) override;
  virtual void SetInternalChannelRelative (int ichannel, bool relative) override;

  virtual bool PlayMusic (const VStr &filename, bool Loop) override;
  virtual bool IsMusicPlaying () override;
  virtual void PauseMusic () override;
  virtual void ResumeMusic () override;
  virtual void StopMusic () override;
  virtual void SetMusicPitch (float pitch) override;

  virtual void LockUpdates () override;
  virtual void UnlockUpdates () override;

// public, so i don't have to fuck with "friends"
public:
  enum { MAX_CHANNELS = 256 };

  enum { PRIORITY_MAX_ADJUST = 10 };

  mythread updateThread;
  bool updateThreadStarted;
  mythread_mutex lockListener;

  mythread_mutex stpPingLock;
  mythread_cond stpPingCond;

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

  // hardware devices
  VSoundDevice *SoundDevice;

  // sound effect helpers
  int GetChannel (int sound_id, int origin_id, int channel, int priority, bool loop);
  void StopChannelByNum (int chan_num);
  void PauseChannelByNum (int chan_num);
  void ResumeChannelByNum (int chan_num);
  bool IsChannelPausedByNum (int chan_num);
  void UpdateSfx (float frameDelta);

  void sendQuit ();
};


VAudioPublic *GAudio = nullptr;
FAudioCodecDesc *FAudioCodecDesc::List;


// ////////////////////////////////////////////////////////////////////////// //
float VAudioPublic::snd_sfx_volume = 1.0; //("snd_sfx_volume", "0.5", "Sound effects volume.", CVAR_Archive);
float VAudioPublic::snd_music_volume = 1.0; //("snd_music_volume", "0.5", "Music volume", CVAR_Archive);
bool VAudioPublic::snd_swap_stereo = false; //("snd_swap_stereo", false, "Swap stereo channels?", CVAR_Archive);
int VAudioPublic::snd_channels = 32; //("snd_channels", "128", "Number of sound channels.", CVAR_Archive);
int VAudioPublic::snd_max_distance = 1200;


//==========================================================================
//
//  streamPlayerThread
//
//==========================================================================
static MYTHREAD_RET_TYPE audioUpdateThread (void *aobjptr) {
  VAudio *audio = (VAudio *)aobjptr;
  mythread_mutex_lock(&audio->stpPingLock);
  audio->SoundDevice->AddCurrentThread();
  mythread_condtime ctime;
  double lastUpdateTime = -1;
  for (;;) {
    mythread_condtime_set(&ctime, &audio->stpPingCond, 33); // ~30 FPS
    auto res = mythread_cond_timedwait(&audio->stpPingCond, &audio->stpPingLock, &ctime);
    if (res == 0) {
      // ping received, which means that main thread wants to quit
      break;
    }
    double frameDelta = 0;
    double ctt = fsysCurrTick();
    // update sounds
    if (lastUpdateTime > 0 && lastUpdateTime <= ctt) frameDelta = ctt-lastUpdateTime;
    lastUpdateTime = ctt;
    audio->UpdateSfx(frameDelta);
  }
  audio->SoundDevice->RemoveCurrentThread();
  mythread_mutex_unlock(&audio->stpPingLock);
  return MYTHREAD_RET_VALUE;
}


//==========================================================================
//
//  VAudioPublic::Create
//
//==========================================================================
VAudioPublic *VAudioPublic::Create () {
  return new VAudio();
}


//==========================================================================
//
//  VAudio::VAudio
//
//==========================================================================
VAudio::VAudio()
  : MaxSoundDist(VAudioPublic::snd_max_distance)
  , StreamPlaying(false)
  , StreamMusicPlayer(nullptr)
  , NumChannels(0)
  , SndCount(0)
  , SoundDevice(nullptr)
{
  memset(Channel, 0, sizeof(Channel));
  ListenerForward = TVec(0, 0, -1);
  ListenerUp = TVec(0, 1, 0);
  ListenerOrigin = TVec(0, 0, 0);
  ListenerVelocity = TVec(0, 0, 0);

  updateThreadStarted = false;
  updaterThreadLockFlag = 0;

  mythread_mutex_init(&stpPingLock);
  mythread_cond_init(&stpPingCond);

  mythread_mutex_init(&lockListener);
}


//==========================================================================
//
//  VAudio::~VAudio
//
//==========================================================================
VAudio::~VAudio () {
  Shutdown();
}


//==========================================================================
//
//  VAudio::Init
//
//  Initialises sound stuff, including volume
//  Sets channels, SFX and music volume, allocates channel buffer.
//
//==========================================================================
void VAudio::Init () {
  Shutdown(); // just in case

  // initialize sound device
  if (!SoundDevice) SoundDevice = CreateVSoundDevice();
  if (!SoundDevice->Init()) {
    delete SoundDevice;
    SoundDevice = nullptr;
  }

  // initialise stream music player
  if (SoundDevice) {
    StreamMusicPlayer = new VStreamMusicPlayer(SoundDevice);
    StreamMusicPlayer->Init();
  }

  MaxSoundDist = VAudioPublic::snd_max_distance;

  // free all channels for use
  memset(Channel, 0, sizeof(Channel));
  NumChannels = (SoundDevice ? SoundDevice->SetChannels(snd_channels) : 0);
  for (int f = 0; f < MAX_CHANNELS; ++f) Channel[f].handle = -1;

  if (SoundDevice) {
    if (mythread_create(&updateThread, &audioUpdateThread, this)) Sys_Error("OpenAL driver cannot create audio updater thread");
    updateThreadStarted = true;
  }
}


//==========================================================================
//
//  VAudio::Shutdown
//
//  Shuts down all sound stuff
//
//==========================================================================
void VAudio::Shutdown () {
  sendQuit();
  // stop playback of all sounds
  if (SoundDevice) StopSounds();
  if (StreamMusicPlayer) {
    StreamMusicPlayer->Shutdown();
    delete StreamMusicPlayer;
    StreamMusicPlayer = nullptr;
  }
  if (SoundDevice) {
    SoundDevice->Shutdown();
    delete SoundDevice;
    SoundDevice = nullptr;
  }
}


//==========================================================================
//
//  VAudio::sendQuit
//
//==========================================================================
void VAudio::sendQuit () {
  if (updateThreadStarted) {
    // we'll aquire lock if another thread is in cond_wait
    mythread_mutex_lock(&stpPingLock);
    // and immediately release it
    mythread_mutex_unlock(&stpPingLock);
    // send signal
    mythread_cond_signal(&stpPingCond);
    // wait for it to complete
    mythread_join(updateThread);
    // done
    updateThreadStarted = false;
  }
  updaterThreadLockFlag = 0;
}


//==========================================================================
//
//  VAudio::LockUpdates
//
//==========================================================================
void VAudio::LockUpdates () {
  MyThreadLocker lislock(&lockListener);
  // 256 should be enough for everyone
  if (++updaterThreadLockFlag > 256) Sys_Error("SoundSysmem update locker is not balanced");
}


//==========================================================================
//
//  VAudio::UnlockUpdates
//
//==========================================================================
void VAudio::UnlockUpdates () {
  MyThreadLocker lislock(&lockListener);
  if (--updaterThreadLockFlag < 0) Sys_Error("SoundSysmem update locker is not balanced");
}


static float RandomFloat () {
  vuint32 rn;
  ed25519_randombytes(&rn, sizeof(rn));
  float res = float(rn&0x3ffff)/(float)0x3ffff;
  return res;
}


//==========================================================================
//
//  VAudio::PlaySound
//
//  This function adds a sound to the list of currently active sounds, which
//  is maintained as a given number of internal channels.
//
//  Returns allocated channel or -1
//
//==========================================================================
int VAudio::PlaySound (int sound_id, const TVec &origin,
  const TVec &velocity, int origin_id, int channel, float volume,
  float attenuation, float pitch, bool Loop)
{
  if (!SoundDevice || sound_id < 1 || volume <= 0 || snd_sfx_volume <= 0) return -1;
  if (sound_id >= GSoundManager->S_sfx.length()) return -1;
  if (!GSoundManager->S_sfx[sound_id].data) return -1; // this sound wasn't loaded, don't bother with it

  // check if this sound is emited by the local player
  bool localPlayerSound = (origin_id == 0); //(cl && cl->MO && cl->MO->SoundOriginID == origin_id);

  // calculate the distance before other stuff so that we can throw out sounds that are beyond the hearing range
  int dist = 0;
  if (!localPlayerSound && attenuation > 0) dist = (int)(Length(origin-ListenerOrigin)*attenuation);

  MyThreadLocker lislock(&lockListener);

  MaxSoundDist = VAudioPublic::snd_max_distance;

  // is sound beyond the hearing range?
  if (dist >= MaxSoundDist) return -1;

  // apply sound volume
  if (snd_sfx_volume < 1.0) volume *= snd_sfx_volume;

  // calculate sound priority
  int priority = GSoundManager->S_sfx[sound_id].priority*(PRIORITY_MAX_ADJUST-PRIORITY_MAX_ADJUST*dist/MaxSoundDist);

  int chan = GetChannel(sound_id, origin_id, channel, priority, Loop);
  if (chan == -1) return -1; // no free channels
  bool startIt = (Channel[chan].handle < 0);

  // random pitch?
  if (GSoundManager->S_sfx[sound_id].changePitch) {
    pitch = 1.0+(RandomFloat()-RandomFloat())*GSoundManager->S_sfx[sound_id].changePitch;
  }

  if (startIt) {
    int handle = SoundDevice->PlaySound3D(sound_id, origin, velocity, volume, pitch, Loop, localPlayerSound);
    if (handle < 0) {
      Channel[chan].handle = -1;
      Channel[chan].origin_id = 0;
      Channel[chan].sound_id = 0;
      return -1;
    } else {
      Channel[chan].handle = handle;
      Channel[chan].pitch = pitch;
      Channel[chan].volume = volume;
      Channel[chan].paused = false;
    }
  }

  Channel[chan].origin_id = origin_id;
  Channel[chan].channel = channel;
  Channel[chan].origin = origin;
  Channel[chan].velocity = velocity;
  Channel[chan].sound_id = sound_id;
  Channel[chan].priority = priority;
  Channel[chan].attenuation = attenuation;
  Channel[chan].localPlayerSound = localPlayerSound;
  Channel[chan].loop = Loop;
  Channel[chan].newPitch = pitch;
  Channel[chan].newVolume = volume;

  return chan;
}


//==========================================================================
//
//  VAudio::GetChannel
//
//==========================================================================
int VAudio::GetChannel (int sound_id, int origin_id, int channel, int priority, bool loop) {
  // if it's a looping sound and it's still playing, don't replace it
  // otherwise, abort currently playing sound on the given logical channel
  if (channel >= 0) {
    for (int i = 0; i < NumChannels; ++i) {
      if (Channel[i].handle >= 0 && Channel[i].sound_id == sound_id) {
        // if this sound is singular, and it is already playing, don't start it again
        if (GSoundManager->S_sfx[sound_id].bSingular) return -1;
        if (Channel[i].origin_id != origin_id || Channel[i].channel != channel) continue;
        // don't restart looped sound
        if (!loop || Channel[i].loop != loop) {
#ifdef DEBUG_CHAN_ALLOC
          fprintf(stderr, "aborting sound %d (oid=%d; cid=%d) at ichannel %d\n", sound_id, origin_id, channel, i);
#endif
          StopChannelByNum(i);
        }
        return i;
      }
    }
  }

  // new sound
  const int numchannels = GSoundManager->S_sfx[sound_id].numChannels;

  if (numchannels > 0) {
    int lp = -1; // least priority
    int found = 0;
    auto prior = priority;

    for (int i = 0; i < NumChannels; ++i) {
      if (Channel[i].sound_id == sound_id) {
        ++found; // found one; now, should we replace it?
        if (prior >= Channel[i].priority) {
          // if we're gonna kill one, then this'll be it
          lp = i;
          prior = Channel[i].priority;
        }
      }
    }

    if (found >= numchannels) {
      // other sounds have greater priority?
      if (lp == -1) return -1; // don't replace any sounds
#ifdef DEBUG_CHAN_ALLOC
      fprintf(stderr, "aborting sound %d (oid=%d; cid=%d) at ichannel %d\n", Channel[lp].sound_id, Channel[lp].origin_id, Channel[lp].channel, lp);
#endif
      StopChannelByNum(lp);
    }
  }

  // look for a free channel
  for (int i = 0; i < NumChannels; ++i) {
    if (!Channel[i].sound_id) return i;
  }

  // look for a lower priority sound to replace.
  ++SndCount;
  if (SndCount >= NumChannels) SndCount = 0;

  for (int chan = 0; chan < NumChannels; ++chan) {
    int i = (SndCount+chan)%NumChannels;
    if (priority >= Channel[i].priority) {
      // replace the lower priority sound
#ifdef DEBUG_CHAN_ALLOC
      fprintf(stderr, "aborting sound %d (oid=%d; cid=%d) at ichannel %d\n", Channel[i].sound_id, Channel[i].origin_id, Channel[i].channel, i);
#endif
      StopChannelByNum(i);
      return i;
    }
  }

  // no free channels
  return -1;
}


//==========================================================================
//
//  VAudio::StopChannelByNum
//
//==========================================================================
void VAudio::StopChannelByNum (int chan_num) {
  if (chan_num >= 0 && chan_num < NumChannels && Channel[chan_num].handle >= 0) {
#ifdef DEBUG_CHAN_ALLOC
    fprintf(stderr, "%s ichannel %d (oid=%d; sid=%d; cid=%d)\n",
      (SoundDevice->IsChannelActive(Channel[chan_num].handle) ? "stopping" : "clearing"), chan_num,
      Channel[chan_num].origin_id, Channel[chan_num].sound_id, Channel[chan_num].channel);
#endif
    SoundDevice->StopChannel(Channel[chan_num].handle);
    Channel[chan_num].handle = -1;
    Channel[chan_num].origin_id = 0;
    Channel[chan_num].sound_id = 0;
  }
}


//==========================================================================
//
//  VAudio::PauseChannelByNum
//
//==========================================================================
void VAudio::PauseChannelByNum (int chan_num) {
  if (chan_num >= 0 && chan_num < NumChannels && Channel[chan_num].handle >= 0 && !Channel[chan_num].paused) {
    SoundDevice->PauseChannel(Channel[chan_num].handle);
    Channel[chan_num].paused = true;
  }
}


//==========================================================================
//
//  VAudio::ResumeChannelByNum
//
//==========================================================================
void VAudio::ResumeChannelByNum (int chan_num) {
  if (chan_num >= 0 && chan_num < NumChannels && Channel[chan_num].handle >= 0 && Channel[chan_num].paused) {
    SoundDevice->ResumeChannel(Channel[chan_num].handle);
    Channel[chan_num].paused = false;
  }
}


//==========================================================================
//
//  VAudio::IsChannelPausedByNum
//
//==========================================================================
bool VAudio::IsChannelPausedByNum (int chan_num) {
  return (chan_num >= 0 && chan_num < NumChannels && Channel[chan_num].handle >= 0 ? Channel[chan_num].paused : false);
}


//==========================================================================
//
//  VAudio::PauseSounds
//
//==========================================================================
void VAudio::PauseSounds () {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle < 0) continue;
    if (!SoundDevice->IsChannelActive(Channel[i].handle)) { StopChannelByNum(i); continue; } // free it, why not?
    PauseChannelByNum(i);
  }
}


//==========================================================================
//
//  VAudio::ResumeSounds
//
//==========================================================================
void VAudio::ResumeSounds () {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle < 0) continue;
    if (!SoundDevice->IsChannelActive(Channel[i].handle)) { StopChannelByNum(i); continue; } // free it, why not?
    ResumeChannelByNum(i);
  }
}


//==========================================================================
//
//  VAudio::StopSounds
//
//==========================================================================
void VAudio::StopSounds () {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) StopChannelByNum(i);
}


//==========================================================================
//
//  VAudio::StopChannel
//
//==========================================================================
void VAudio::StopChannel (int origin_id, int channel) {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].origin_id == origin_id && (channel < 0 || Channel[i].channel == channel)) {
      if (!SoundDevice->IsChannelActive(Channel[i].handle)) { StopChannelByNum(i); continue; } // free it, why not?
      StopChannelByNum(i);
    }
  }
}


//==========================================================================
//
//  VAudio::PauseChannel
//
//==========================================================================
void VAudio::PauseChannel (int origin_id, int channel) {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].origin_id == origin_id && (channel < 0 || Channel[i].channel == channel)) {
      if (!SoundDevice->IsChannelActive(Channel[i].handle)) { StopChannelByNum(i); continue; } // free it, why not?
      PauseChannelByNum(i);
    }
  }
}


//==========================================================================
//
//  VAudio::ResumeChannel
//
//==========================================================================
void VAudio::ResumeChannel (int origin_id, int channel) {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].origin_id == origin_id && (channel < 0 || Channel[i].channel == channel)) {
      if (!SoundDevice->IsChannelActive(Channel[i].handle)) { StopChannelByNum(i); continue; } // free it, why not?
      ResumeChannelByNum(i);
    }
  }
}


//==========================================================================
//
//  VAudio::IsChannelActive
//
//==========================================================================
bool VAudio::IsChannelActive (int origin_id, int channel) {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].origin_id == origin_id && (channel < 0 || Channel[i].channel == channel)) {
      if (SoundDevice->IsChannelActive(Channel[i].handle)) return true;
      StopChannelByNum(i); // free it, why not?
    }
  }
  return false;
}


//==========================================================================
//
//  VAudio::IsChannelPaused
//
//==========================================================================
bool VAudio::IsChannelPaused (int origin_id, int channel) {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].origin_id == origin_id && (channel < 0 || Channel[i].channel == channel)) {
      if (IsChannelPausedByNum(i)) return true;
    }
  }
  return false;
}


//==========================================================================
//
//  VAudio::SetChannelPitch
//
//==========================================================================
void VAudio::SetChannelPitch (int origin_id, int channel, float pitch) {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle < 0) continue;
    if ((channel < 0 || Channel[i].channel == channel) && Channel[i].origin_id == origin_id) {
      Channel[i].newPitch = pitch;
    }
  }
}


//==========================================================================
//
//  VAudio::SetChannelOrigin
//
//==========================================================================
void VAudio::SetChannelOrigin (int origin_id, int channel, const TVec &origin) {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle < 0) continue;
    if ((channel < 0 || Channel[i].channel == channel) && Channel[i].origin_id == origin_id) {
      Channel[i].origin = origin;
    }
  }
}


//==========================================================================
//
//  VAudio::SetChannelVelocity
//
//==========================================================================
void VAudio::SetChannelVelocity (int origin_id, int channel, const TVec &velocity) {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle < 0) continue;
    if ((channel < 0 || Channel[i].channel == channel) && Channel[i].origin_id == origin_id) {
      Channel[i].velocity = velocity;
    }
  }
}


//==========================================================================
//
//  VAudio::SetChannelVolume
//
//==========================================================================
void VAudio::SetChannelVolume (int origin_id, int channel, float volume) {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle < 0) continue;
    if ((channel < 0 || Channel[i].channel == channel) && Channel[i].origin_id == origin_id) {
      Channel[i].newVolume = volume;
    }
  }
}


//==========================================================================
//
//  VAudio::SetChannelAttenuation
//
//==========================================================================
void VAudio::SetChannelAttenuation (int origin_id, int channel, float attenuation) {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle < 0) continue;
    if ((channel < 0 || Channel[i].channel == channel) && Channel[i].origin_id == origin_id) {
      Channel[i].attenuation = attenuation;
    }
  }
}


//==========================================================================
//
//  VAudio::PauseSound
//
//==========================================================================
void VAudio::PauseSound (int origin_id, int sound_id) {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle < 0) continue;
    if (Channel[i].sound_id == sound_id && Channel[i].origin_id == origin_id) {
      if (!SoundDevice->IsChannelActive(Channel[i].handle)) { StopChannelByNum(i); continue; } // free it, why not?
      PauseChannelByNum(i);
    }
  }
}


//==========================================================================
//
//  VAudio::ResumeSound
//
//==========================================================================
void VAudio::ResumeSound (int origin_id, int sound_id) {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle < 0) continue;
    if (Channel[i].sound_id == sound_id && Channel[i].origin_id == origin_id) {
      if (!SoundDevice->IsChannelActive(Channel[i].handle)) { StopChannelByNum(i); continue; } // free it, why not?
      ResumeChannelByNum(i);
    }
  }
}


//==========================================================================
//
//  VAudio::StopSound
//
//==========================================================================
void VAudio::StopSound (int origin_id, int sound_id) {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle < 0) continue;
    if (Channel[i].sound_id == sound_id && Channel[i].origin_id == origin_id) {
      StopChannelByNum(i);
    }
  }
}


//==========================================================================
//
//  VAudio::IsSoundActive
//
//==========================================================================
bool VAudio::IsSoundActive (int origin_id, int sound_id) {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle < 0) continue;
    if (Channel[i].sound_id == sound_id && Channel[i].origin_id == origin_id) {
      if (SoundDevice->IsChannelActive(Channel[i].handle)) return true;
      StopChannelByNum(i); // free it, why not?
    }
  }
  return false;
}


//==========================================================================
//
//  VAudio::IsSoundPaused
//
//==========================================================================
bool VAudio::IsSoundPaused (int origin_id, int sound_id) {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle < 0) continue;
    if (Channel[i].sound_id == sound_id && Channel[i].origin_id == origin_id) {
      if (IsChannelPausedByNum(i)) return true;
    }
  }
  return false;
}


//==========================================================================
//
//  VAudio::SetSoundPitch
//
//==========================================================================
void VAudio::SetSoundPitch (int origin_id, int sound_id, float pitch) {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle < 0) continue;
    if (Channel[i].sound_id == sound_id && Channel[i].origin_id == origin_id) {
      Channel[i].newPitch = pitch;
    }
  }
}


//==========================================================================
//
//  VAudio::SetSoundOrigin
//
//==========================================================================
void VAudio::SetSoundOrigin (int origin_id, int sound_id, const TVec &origin) {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle < 0) continue;
    if (Channel[i].sound_id == sound_id && Channel[i].origin_id == origin_id) {
      Channel[i].origin = origin;
    }
  }
}


//==========================================================================
//
//  VAudio::SetSoundVelocity
//
//==========================================================================
void VAudio::SetSoundVelocity (int origin_id, int sound_id, const TVec &velocity) {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle < 0) continue;
    if (Channel[i].sound_id == sound_id && Channel[i].origin_id == origin_id) {
      Channel[i].velocity = velocity;
    }
  }
}


//==========================================================================
//
//  VAudio::SetSoundVolume
//
//==========================================================================
void VAudio::SetSoundVolume (int origin_id, int sound_id, float volume) {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle < 0) continue;
    if (Channel[i].sound_id == sound_id && Channel[i].origin_id == origin_id) {
      Channel[i].newVolume = volume;
    }
  }
}


//==========================================================================
//
//  VAudio::SetSoundAttenuation
//
//==========================================================================
void VAudio::SetSoundAttenuation (int origin_id, int sound_id, float attenuation) {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle < 0) continue;
    if (Channel[i].sound_id == sound_id && Channel[i].origin_id == origin_id) {
      Channel[i].attenuation = attenuation;
    }
  }
}


//==========================================================================
//
//  VAudio::FindInternalChannelForSound
//
//==========================================================================
int VAudio::FindInternalChannelForSound (int origin_id, int sound_id) {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle < 0) continue;
    if (Channel[i].sound_id == sound_id && Channel[i].origin_id == origin_id) {
      if (!SoundDevice->IsChannelActive(Channel[i].handle)) { StopChannelByNum(i); continue; } // free it, why not?
      return i;
    }
  }
  return -1;
}


//==========================================================================
//
//  VAudio::FindInternalChannelForChannel
//
//==========================================================================
int VAudio::FindInternalChannelForChannel (int origin_id, int channel) {
  if (channel < 0) return -1; // don't know what to do
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].origin_id == origin_id && Channel[i].channel == channel) {
      if (!SoundDevice->IsChannelActive(Channel[i].handle)) { StopChannelByNum(i); continue; } // free it, why not?
      return i;
    }
  }
  return -1;
}


//==========================================================================
//
//  VAudio::IsInternalChannelPlaying
//
//==========================================================================
bool VAudio::IsInternalChannelPlaying (int ichannel) {
  MyThreadLocker lislock(&lockListener);
  if (ichannel >= 0 && ichannel < NumChannels && Channel[ichannel].handle >= 0) {
    if (SoundDevice->IsChannelActive(Channel[ichannel].handle)) return true;
    StopChannelByNum(ichannel); // free it, why not?
  }
  return false;
}


//==========================================================================
//
//  VAudio::IsInternalChannelPaused
//
//==========================================================================
bool VAudio::IsInternalChannelPaused (int ichannel) {
  MyThreadLocker lislock(&lockListener);
  return IsChannelPausedByNum(ichannel);
}


//==========================================================================
//
//  VAudio::StopInternalChannel
//
//==========================================================================
void VAudio::StopInternalChannel (int ichannel) {
  MyThreadLocker lislock(&lockListener);
  StopChannelByNum(ichannel);
}


//==========================================================================
//
//  VAudio::PauseInternalChannel
//
//==========================================================================
void VAudio::PauseInternalChannel (int ichannel) {
  MyThreadLocker lislock(&lockListener);
  PauseChannelByNum(ichannel);
}


//==========================================================================
//
//  VAudio::ResumeInternalChannel
//
//==========================================================================
void VAudio::ResumeInternalChannel (int ichannel) {
  MyThreadLocker lislock(&lockListener);
  ResumeChannelByNum(ichannel);
}


//==========================================================================
//
//  VAudio::IsInternalChannelRelative
//
//==========================================================================
bool VAudio::IsInternalChannelRelative (int ichannel) {
  MyThreadLocker lislock(&lockListener);
  return (ichannel >= 0 && ichannel < NumChannels && Channel[ichannel].handle >= 0 ? Channel[ichannel].localPlayerSound : false);
}


//==========================================================================
//
//  VAudio::SetInternalChannelRelative
//
//==========================================================================
void VAudio::SetInternalChannelRelative (int ichannel, bool relative) {
  MyThreadLocker lislock(&lockListener);
  if (ichannel >= 0 && ichannel < NumChannels && Channel[ichannel].handle >= 0) Channel[ichannel].localPlayerSound = relative;
}


//==========================================================================
//
//  VAudio::UpdateSfx
//
//  Update the sound parameters. Used to control volume and pan
//  changes such as when a player turns.
//
//==========================================================================
void VAudio::UpdateSfx (float frameDelta) {
  MyThreadLocker lislock(&lockListener);

  if (updaterThreadLockFlag || !SoundDevice || !NumChannels) return;

  MaxSoundDist = VAudioPublic::snd_max_distance;

  if (StreamMusicPlayer) {
    auto mvol = snd_music_volume;
    if (mvol < 0) mvol = 0; else if (mvol > 1) mvol = 1;
    StreamMusicPlayer->SetVolume(mvol);
  }

  // don't do this, so we can change volume without interrupting sounds
  //if (snd_sfx_volume <= 0) { StopSounds(); return; }

  //fprintf(stderr, "listener position: (%f,%f,%f)\n\n", ListenerOrigin.x, ListenerOrigin.y, ListenerOrigin.z);
  SoundDevice->UpdateListener(ListenerOrigin, ListenerVelocity, ListenerForward, ListenerUp);

  for (int i = 0; i < NumChannels; ++i) {
    if (!Channel[i].sound_id) continue; // nothing on this channel
    if (Channel[i].handle < 0) continue;
    if (!SoundDevice->IsChannelActive(Channel[i].handle)) {
      // playback done
      StopChannelByNum(i);
      continue;
    }

    //if (!Channel[i].origin_id || Channel[i].attenuation <= 0) continue; // full volume sound
    //if (Channel[i].localPlayerSound) continue; // client sound

    // move sound
    if (frameDelta) Channel[i].origin += Channel[i].velocity*frameDelta;

    // calculate the distance before other stuff so that we can throw out sounds that are beyond the hearing range
    int dist = 0;
    if (!Channel[i].localPlayerSound && Channel[i].attenuation > 0) dist = (int)(Length(Channel[i].origin-ListenerOrigin)*Channel[i].attenuation);

    // too far away?
    if (dist >= MaxSoundDist) {
      StopChannelByNum(i);
      continue;
    }

    // update params
    SoundDevice->UpdateChannel3D(Channel[i].handle, Channel[i].origin, Channel[i].velocity, Channel[i].localPlayerSound);

    if (Channel[i].newPitch != Channel[i].pitch) {
      SoundDevice->UpdateChannelPitch(Channel[i].handle, Channel[i].newPitch);
      Channel[i].pitch = Channel[i].newPitch;
    }

    if (Channel[i].newVolume != Channel[i].volume) {
      SoundDevice->UpdateChannelVolume(Channel[i].handle, Channel[i].newVolume);
      Channel[i].volume = Channel[i].newVolume;
    }

    Channel[i].priority = GSoundManager->S_sfx[Channel[i].sound_id].priority*(PRIORITY_MAX_ADJUST-PRIORITY_MAX_ADJUST*dist/MaxSoundDist);
  }
}


//==========================================================================
//
//  VAudio::PlayMusic
//
//==========================================================================
bool VAudio::PlayMusic (const VStr &filename, bool Loop) {
  if (StreamPlaying) StreamMusicPlayer->Stop();
  StreamPlaying = false;

  if (filename.isEmpty()) return false;

  VStream *Strm = fsysOpenFile(filename);
  if (!Strm) return false;

  if (Strm->TotalSize() < 4) {
    delete Strm;
    Strm = nullptr;
    return false;
  }

  /*
  byte Hdr[4];
  Strm->Serialise(Hdr, 4);
  if (!memcmp(Hdr, MUSMAGIC, 4))
  {
    // convert mus to mid with a wonderfull function
    // thanks to S.Bacquet for the source of qmus2mid
    Strm->Seek(0);
    VMemoryStream *MidStrm = new VMemoryStream();
    MidStrm->BeginWrite();
    VQMus2Mid Conv;
    int MidLength = Conv.Run(*Strm, *MidStrm);
    delete Strm;
    Strm = nullptr;
    if (!MidLength)
    {
      delete MidStrm;
      MidStrm = nullptr;
      return;
    }
    MidStrm->Seek(0);
    MidStrm->BeginRead();
    Strm = MidStrm;
  }
  */

  //  Try to create audio codec.
  VAudioCodec *Codec = nullptr;
  for (FAudioCodecDesc *Desc = FAudioCodecDesc::List; Desc && !Codec; Desc = Desc->Next) {
    //GCon->Logf(va("Using %s to open the stream", Desc->Description));
    Codec = Desc->Creator(Strm);
  }

  if (StreamMusicPlayer && Codec) {
    // start playing streamed music
    auto mvol = snd_music_volume;
    if (mvol < 0) mvol = 0; else if (mvol > 1) mvol = 1;
    StreamMusicPlayer->SetVolume(mvol);
    StreamMusicPlayer->Play(Codec, filename, Loop);
    StreamPlaying = true;
    return true;
  } else {
    delete Strm;
    Strm = nullptr;
    return false;
  }
}


//==========================================================================
//
//  VAudio::IsMusicPlaying
//
//==========================================================================
bool VAudio::IsMusicPlaying () {
  if (StreamPlaying && StreamMusicPlayer) return StreamMusicPlayer->IsPlaying();
  return false;
}


//==========================================================================
//
//  VAudio::PauseMusic
//
//==========================================================================
void VAudio::PauseMusic () {
  if (StreamPlaying && StreamMusicPlayer) StreamMusicPlayer->Pause();
}


//==========================================================================
//
//  VAudio::ResumeMusic
//
//==========================================================================
void VAudio::ResumeMusic () {
  if (StreamPlaying && StreamMusicPlayer) StreamMusicPlayer->Resume();
}


//==========================================================================
//
//  VAudio::StopMusic
//
//==========================================================================
void VAudio::StopMusic () {
  if (StreamPlaying && StreamMusicPlayer) StreamMusicPlayer->Stop();
}


//==========================================================================
//
//  VAudio::SetMusicPitch
//
//==========================================================================
void VAudio::SetMusicPitch (float pitch) {
  if (StreamPlaying && StreamMusicPlayer) StreamMusicPlayer->SetPitch(pitch);
}
