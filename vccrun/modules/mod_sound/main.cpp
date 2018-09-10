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
#include "sound_private.h"
#include "sound.h"

//#define DEBUG_CHAN_ALLOC

#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>


// ////////////////////////////////////////////////////////////////////////// //
VSampleLoader *VSampleLoader::List;
VSoundManager *GSoundManager;
static bool sminited = false;


// ////////////////////////////////////////////////////////////////////////// //
void VSoundManager::StaticInitialize () {
  if (!sminited) {
    if (!GSoundManager) {
      sminited = true;
      GSoundManager = new VSoundManager;
      GSoundManager->Init();
      GAudio = new VAudioPublic();
      GAudio->Init();
    }
  }
}


void VSoundManager::StaticShutdown () {
  if (GAudio) GAudio->Shutdown();
  delete GAudio;
  //if (GSoundManager) GSoundManager->Shutdown();
  delete GSoundManager;
  GAudio = nullptr;
  GSoundManager = nullptr;
  sminited = false;
}


//==========================================================================
//
//  VSampleLoader::LoadFromAudioCodec
//
//  codec must be initialized, and it will not be owned
//
//==========================================================================
void VSampleLoader::LoadFromAudioCodec (sfxinfo_t &Sfx, VAudioCodec *Codec) {
  if (!Codec) return;
  //if (Codec->NumChannels != 1 && Codec->NumChannels != 2) return;
  //fprintf(stderr, "loading from audio codec; chans=%d; rate=%d; bits=%d\n", Codec->NumChannels, Codec->SampleRate, Codec->SampleBits);
  const int MAX_FRAMES = 65536;

  TArray<short> Data;
  short *buf = (short *)malloc(MAX_FRAMES*2*2);
  if (!buf) return; // oops
  do {
    int SamplesDecoded = Codec->Decode(buf, MAX_FRAMES);
    if (SamplesDecoded > 0) {
      int oldlen = Data.length();
      Data.SetNumWithReserve(oldlen+SamplesDecoded);
      // downmix stereo to mono
      const short *src = buf;
      short *dst = ((short *)Data.Ptr())+oldlen;
      for (int i = 0; i < SamplesDecoded; ++i, src += 2) {
        int v = (src[0]+src[1])/2;
        if (v < -32768) v = -32768; else if (v > 32767) v = 32767;
        *dst++ = (short)v;
      }
    } else {
      break;
    }
  } while (!Codec->Finished());
  free(buf);

  int realLen = Data.length();
  if (realLen < 1) return;
/*
  if (Data.length() < 1) return;
  // we don't care about timing, so trim trailing silence
  int realLen = Data.length()-1;
  while (realLen >= 0 && Data[realLen] > -64 && Data[realLen] < 64) --realLen;
  ++realLen;
  if (realLen < 1) realLen = 1;
*/

  // copy parameters
  Sfx.sampleRate = Codec->SampleRate;
  Sfx.sampleBits = Codec->SampleBits;

  // copy data
  Sfx.dataSize = realLen*2;
  Sfx.data = (vuint8 *)Z_Malloc(realLen*2);
  memcpy(Sfx.data, Data.Ptr(), realLen*2);
}


//==========================================================================
//
//  VSoundManager::VSoundManager
//
//==========================================================================
VSoundManager::VSoundManager () : name2idx() {
}


//==========================================================================
//
//  VSoundManager::~VSoundManager
//
//==========================================================================
VSoundManager::~VSoundManager () {
  for (int i = 0; i < S_sfx.length(); ++i) {
    if (S_sfx[i].data) {
      Z_Free(S_sfx[i].data);
      S_sfx[i].data = nullptr;
    }
    S_sfx.clear();
  }
}


//==========================================================================
//
//  VSoundManager::Init
//
//==========================================================================
void VSoundManager::Init () {
  // zero slot is reserved
  sfxinfo_t S;
  memset((void *)(&S), 0, sizeof(S));
  S_sfx.Append(S);
}


//==========================================================================
//
//  VSoundManager::AddSound
//
//==========================================================================
int VSoundManager::AddSound (VName TagName, const VStr &filename) {
  if (TagName == NAME_None) return 0;
  int id = FindSound(TagName);
  if (!id) {
    sfxinfo_t S;
    memset((void *)(&S), 0, sizeof(S));
    S.tagName = TagName;
    S.priority = 127;
    S.numChannels = 2;
    S.changePitch = 0;
    // we can't unload sounds, so don't bother searching for a free slot
    int idx = S_sfx.length();
    S_sfx.Append(S);
    name2idx.put(TagName, idx);
    LoadSound(idx, filename);
    return idx;
  }
  return id;
}


//==========================================================================
//
//  VSoundManager::FindSound
//
//==========================================================================
int VSoundManager::FindSound (VName tagName) {
  auto ii = name2idx.find(tagName);
  if (ii) return *ii;
  return 0;
}


//==========================================================================
//
//  VSoundManager::GetSoundPriority
//
//==========================================================================
int VSoundManager::GetSoundPriority (VName tagName) {
  auto ii = name2idx.find(tagName);
  return (ii ? S_sfx[*ii].priority : 127);
}


//==========================================================================
//
//  VSoundManager::SetSoundPriority
//
//==========================================================================
void VSoundManager::SetSoundPriority (VName tagName, int value) {
  auto ii = name2idx.find(tagName);
  if (ii) S_sfx[*ii].priority = value;
}


//==========================================================================
//
//  VSoundManager::GetSoundChannels
//
//==========================================================================
int VSoundManager::GetSoundChannels (VName tagName) {
  auto ii = name2idx.find(tagName);
  return (ii ? S_sfx[*ii].numChannels : 2);
}


//==========================================================================
//
//  VSoundManager::SetSoundChannels
//
//==========================================================================
void VSoundManager::SetSoundChannels (VName tagName, int value) {
  auto ii = name2idx.find(tagName);
  if (ii) S_sfx[*ii].numChannels = value;
}


//==========================================================================
//
//  VSoundManager::GetSoundRandomPitch
//
//==========================================================================
float VSoundManager::GetSoundRandomPitch (VName tagName) {
  auto ii = name2idx.find(tagName);
  return (ii ? S_sfx[*ii].changePitch : 0);
}


//==========================================================================
//
//  VSoundManager::SetSoundRandomPitch
//
//==========================================================================
void VSoundManager::SetSoundRandomPitch (VName tagName, float value) {
  auto ii = name2idx.find(tagName);
  if (ii) S_sfx[*ii].changePitch = value;
}


//==========================================================================
//
//  VSoundManager::GetSoundSingular
//
//==========================================================================
bool VSoundManager::GetSoundSingular (VName tagName) {
  auto ii = name2idx.find(tagName);
  return (ii ? S_sfx[*ii].bSingular : false);
}


//==========================================================================
//
//  VSoundManager::SetSoundSingular
//
//==========================================================================
void VSoundManager::SetSoundSingular (VName tagName, bool value) {
  auto ii = name2idx.find(tagName);
  if (ii) S_sfx[*ii].bSingular = value;
}


//==========================================================================
//
//  VSoundManager::LoadSound
//
//==========================================================================
bool VSoundManager::LoadSound (int sound_id, const VStr &filename) {
  if (!S_sfx[sound_id].data) {
    VStream *Strm = fsysOpenFile(filename);
    if (!Strm) {
      fprintf(stderr, "WARNING: Can't open sound '%s' from file '%s'\n", *S_sfx[sound_id].tagName, *filename);
      return false;
    }
    for (VSampleLoader *Ldr = VSampleLoader::List; Ldr && !S_sfx[sound_id].data; Ldr = Ldr->Next) {
      Strm->Seek(0);
      Ldr->Load(S_sfx[sound_id], *Strm);
      if (S_sfx[sound_id].data) {
        //fprintf(stderr, "Loaded sound '%s' from file '%s'\n", *S_sfx[sound_id].tagName, *filename);
        break;
      }
    }
    delete Strm;
    if (!S_sfx[sound_id].data) {
      fprintf(stderr, "WARNING: Failed to load sound '%s' from file '%s'\n", *S_sfx[sound_id].tagName, *filename);
      return false;
    }
  }
  return true;
}


// ////////////////////////////////////////////////////////////////////////// //
// main audio management class
// ////////////////////////////////////////////////////////////////////////// //
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
  VAudioPublic *audio = (VAudioPublic *)aobjptr;
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
//  VAudioPublic::VAudioPublic
//
//==========================================================================
VAudioPublic::VAudioPublic()
  : SoundDevice(nullptr)
  , MaxSoundDist(VAudioPublic::snd_max_distance)
  , StreamPlaying(false)
  , StreamMusicPlayer(nullptr)
  , NumChannels(0)
  , SndCount(0)
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
//  VAudioPublic::~VAudioPublic
//
//==========================================================================
VAudioPublic::~VAudioPublic () {
  Shutdown();
}


//==========================================================================
//
//  VAudioPublic::Init
//
//  Initialises sound stuff, including volume
//  Sets channels, SFX and music volume, allocates channel buffer.
//
//==========================================================================
void VAudioPublic::Init () {
  Shutdown(); // just in case

  // initialize sound device
  if (!SoundDevice) SoundDevice = new VOpenALDevice();
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
//  VAudioPublic::Shutdown
//
//  Shuts down all sound stuff
//
//==========================================================================
void VAudioPublic::Shutdown () {
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
//  VAudioPublic::sendQuit
//
//==========================================================================
void VAudioPublic::sendQuit () {
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
//  VAudioPublic::LockUpdates
//
//==========================================================================
void VAudioPublic::LockUpdates () {
  MyThreadLocker lislock(&lockListener);
  // 256 should be enough for everyone
  if (++updaterThreadLockFlag > 256) Sys_Error("SoundSysmem update locker is not balanced");
}


//==========================================================================
//
//  VAudioPublic::UnlockUpdates
//
//==========================================================================
void VAudioPublic::UnlockUpdates () {
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
//  VAudioPublic::PlaySound
//
//  This function adds a sound to the list of currently active sounds, which
//  is maintained as a given number of internal channels.
//
//  Returns allocated channel or -1
//
//==========================================================================
int VAudioPublic::PlaySound (int sound_id, const TVec &origin,
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
//  VAudioPublic::GetChannel
//
//==========================================================================
int VAudioPublic::GetChannel (int sound_id, int origin_id, int channel, int priority, bool loop) {
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
//  VAudioPublic::StopChannelByNum
//
//==========================================================================
void VAudioPublic::StopChannelByNum (int chan_num) {
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
//  VAudioPublic::PauseChannelByNum
//
//==========================================================================
void VAudioPublic::PauseChannelByNum (int chan_num) {
  if (chan_num >= 0 && chan_num < NumChannels && Channel[chan_num].handle >= 0 && !Channel[chan_num].paused) {
    SoundDevice->PauseChannel(Channel[chan_num].handle);
    Channel[chan_num].paused = true;
  }
}


//==========================================================================
//
//  VAudioPublic::ResumeChannelByNum
//
//==========================================================================
void VAudioPublic::ResumeChannelByNum (int chan_num) {
  if (chan_num >= 0 && chan_num < NumChannels && Channel[chan_num].handle >= 0 && Channel[chan_num].paused) {
    SoundDevice->ResumeChannel(Channel[chan_num].handle);
    Channel[chan_num].paused = false;
  }
}


//==========================================================================
//
//  VAudioPublic::IsChannelPausedByNum
//
//==========================================================================
bool VAudioPublic::IsChannelPausedByNum (int chan_num) {
  return (chan_num >= 0 && chan_num < NumChannels && Channel[chan_num].handle >= 0 ? Channel[chan_num].paused : false);
}


//==========================================================================
//
//  VAudioPublic::PauseSounds
//
//==========================================================================
void VAudioPublic::PauseSounds () {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle < 0) continue;
    if (!SoundDevice->IsChannelActive(Channel[i].handle)) { StopChannelByNum(i); continue; } // free it, why not?
    PauseChannelByNum(i);
  }
}


//==========================================================================
//
//  VAudioPublic::ResumeSounds
//
//==========================================================================
void VAudioPublic::ResumeSounds () {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle < 0) continue;
    if (!SoundDevice->IsChannelActive(Channel[i].handle)) { StopChannelByNum(i); continue; } // free it, why not?
    ResumeChannelByNum(i);
  }
}


//==========================================================================
//
//  VAudioPublic::StopSounds
//
//==========================================================================
void VAudioPublic::StopSounds () {
  MyThreadLocker lislock(&lockListener);
  for (int i = 0; i < NumChannels; ++i) StopChannelByNum(i);
}


//==========================================================================
//
//  VAudioPublic::StopChannel
//
//==========================================================================
void VAudioPublic::StopChannel (int origin_id, int channel) {
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
//  VAudioPublic::PauseChannel
//
//==========================================================================
void VAudioPublic::PauseChannel (int origin_id, int channel) {
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
//  VAudioPublic::ResumeChannel
//
//==========================================================================
void VAudioPublic::ResumeChannel (int origin_id, int channel) {
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
//  VAudioPublic::IsChannelActive
//
//==========================================================================
bool VAudioPublic::IsChannelActive (int origin_id, int channel) {
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
//  VAudioPublic::IsChannelPaused
//
//==========================================================================
bool VAudioPublic::IsChannelPaused (int origin_id, int channel) {
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
//  VAudioPublic::SetChannelPitch
//
//==========================================================================
void VAudioPublic::SetChannelPitch (int origin_id, int channel, float pitch) {
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
//  VAudioPublic::SetChannelOrigin
//
//==========================================================================
void VAudioPublic::SetChannelOrigin (int origin_id, int channel, const TVec &origin) {
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
//  VAudioPublic::SetChannelVelocity
//
//==========================================================================
void VAudioPublic::SetChannelVelocity (int origin_id, int channel, const TVec &velocity) {
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
//  VAudioPublic::SetChannelVolume
//
//==========================================================================
void VAudioPublic::SetChannelVolume (int origin_id, int channel, float volume) {
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
//  VAudioPublic::SetChannelAttenuation
//
//==========================================================================
void VAudioPublic::SetChannelAttenuation (int origin_id, int channel, float attenuation) {
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
//  VAudioPublic::PauseSound
//
//==========================================================================
void VAudioPublic::PauseSound (int origin_id, int sound_id) {
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
//  VAudioPublic::ResumeSound
//
//==========================================================================
void VAudioPublic::ResumeSound (int origin_id, int sound_id) {
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
//  VAudioPublic::StopSound
//
//==========================================================================
void VAudioPublic::StopSound (int origin_id, int sound_id) {
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
//  VAudioPublic::IsSoundActive
//
//==========================================================================
bool VAudioPublic::IsSoundActive (int origin_id, int sound_id) {
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
//  VAudioPublic::IsSoundPaused
//
//==========================================================================
bool VAudioPublic::IsSoundPaused (int origin_id, int sound_id) {
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
//  VAudioPublic::SetSoundPitch
//
//==========================================================================
void VAudioPublic::SetSoundPitch (int origin_id, int sound_id, float pitch) {
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
//  VAudioPublic::SetSoundOrigin
//
//==========================================================================
void VAudioPublic::SetSoundOrigin (int origin_id, int sound_id, const TVec &origin) {
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
//  VAudioPublic::SetSoundVelocity
//
//==========================================================================
void VAudioPublic::SetSoundVelocity (int origin_id, int sound_id, const TVec &velocity) {
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
//  VAudioPublic::SetSoundVolume
//
//==========================================================================
void VAudioPublic::SetSoundVolume (int origin_id, int sound_id, float volume) {
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
//  VAudioPublic::SetSoundAttenuation
//
//==========================================================================
void VAudioPublic::SetSoundAttenuation (int origin_id, int sound_id, float attenuation) {
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
//  VAudioPublic::FindInternalChannelForSound
//
//==========================================================================
int VAudioPublic::FindInternalChannelForSound (int origin_id, int sound_id) {
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
//  VAudioPublic::FindInternalChannelForChannel
//
//==========================================================================
int VAudioPublic::FindInternalChannelForChannel (int origin_id, int channel) {
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
//  VAudioPublic::IsInternalChannelPlaying
//
//==========================================================================
bool VAudioPublic::IsInternalChannelPlaying (int ichannel) {
  MyThreadLocker lislock(&lockListener);
  if (ichannel >= 0 && ichannel < NumChannels && Channel[ichannel].handle >= 0) {
    if (SoundDevice->IsChannelActive(Channel[ichannel].handle)) return true;
    StopChannelByNum(ichannel); // free it, why not?
  }
  return false;
}


//==========================================================================
//
//  VAudioPublic::IsInternalChannelPaused
//
//==========================================================================
bool VAudioPublic::IsInternalChannelPaused (int ichannel) {
  MyThreadLocker lislock(&lockListener);
  return IsChannelPausedByNum(ichannel);
}


//==========================================================================
//
//  VAudioPublic::StopInternalChannel
//
//==========================================================================
void VAudioPublic::StopInternalChannel (int ichannel) {
  MyThreadLocker lislock(&lockListener);
  StopChannelByNum(ichannel);
}


//==========================================================================
//
//  VAudioPublic::PauseInternalChannel
//
//==========================================================================
void VAudioPublic::PauseInternalChannel (int ichannel) {
  MyThreadLocker lislock(&lockListener);
  PauseChannelByNum(ichannel);
}


//==========================================================================
//
//  VAudioPublic::ResumeInternalChannel
//
//==========================================================================
void VAudioPublic::ResumeInternalChannel (int ichannel) {
  MyThreadLocker lislock(&lockListener);
  ResumeChannelByNum(ichannel);
}


//==========================================================================
//
//  VAudioPublic::IsInternalChannelRelative
//
//==========================================================================
bool VAudioPublic::IsInternalChannelRelative (int ichannel) {
  MyThreadLocker lislock(&lockListener);
  return (ichannel >= 0 && ichannel < NumChannels && Channel[ichannel].handle >= 0 ? Channel[ichannel].localPlayerSound : false);
}


//==========================================================================
//
//  VAudioPublic::SetInternalChannelRelative
//
//==========================================================================
void VAudioPublic::SetInternalChannelRelative (int ichannel, bool relative) {
  MyThreadLocker lislock(&lockListener);
  if (ichannel >= 0 && ichannel < NumChannels && Channel[ichannel].handle >= 0) Channel[ichannel].localPlayerSound = relative;
}


//==========================================================================
//
//  VAudioPublic::UpdateSfx
//
//  Update the sound parameters. Used to control volume and pan
//  changes such as when a player turns.
//
//==========================================================================
void VAudioPublic::UpdateSfx (float frameDelta) {
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
//  VAudioPublic::PlayMusic
//
//==========================================================================
bool VAudioPublic::PlayMusic (const VStr &filename, bool Loop) {
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
//  VAudioPublic::IsMusicPlaying
//
//==========================================================================
bool VAudioPublic::IsMusicPlaying () {
  if (StreamPlaying && StreamMusicPlayer) return StreamMusicPlayer->IsPlaying();
  return false;
}


//==========================================================================
//
//  VAudioPublic::PauseMusic
//
//==========================================================================
void VAudioPublic::PauseMusic () {
  if (StreamPlaying && StreamMusicPlayer) StreamMusicPlayer->Pause();
}


//==========================================================================
//
//  VAudioPublic::ResumeMusic
//
//==========================================================================
void VAudioPublic::ResumeMusic () {
  if (StreamPlaying && StreamMusicPlayer) StreamMusicPlayer->Resume();
}


//==========================================================================
//
//  VAudioPublic::StopMusic
//
//==========================================================================
void VAudioPublic::StopMusic () {
  if (StreamPlaying && StreamMusicPlayer) StreamMusicPlayer->Stop();
}


//==========================================================================
//
//  VAudioPublic::SetMusicPitch
//
//==========================================================================
void VAudioPublic::SetMusicPitch (float pitch) {
  if (StreamPlaying && StreamMusicPlayer) StreamMusicPlayer->SetPitch(pitch);
}


//==========================================================================
//
//  VAudioPublic::GetDevList
//
//==========================================================================
const char *VAudioPublic::GetDevList () {
  if (SoundDevice) return SoundDevice->GetDevList();
  (void)alcGetError(nullptr);
  const ALCchar *list = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
  ALCenum err = alcGetError(nullptr);
  return (err == ALC_NO_ERROR ? list : nullptr);
}


//==========================================================================
//
//  VAudioPublic::GetAllDevList
//
//==========================================================================
const char *VAudioPublic::GetAllDevList () {
  if (SoundDevice) return SoundDevice->GetAllDevList();
  (void)alcGetError(nullptr);
  const ALCchar *list = alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);
  ALCenum err = alcGetError(nullptr);
  return (err == ALC_NO_ERROR ? list : nullptr);
}


//==========================================================================
//
//  VAudioPublic::GetExtList
//
//==========================================================================
const char *VAudioPublic::GetExtList () {
  if (SoundDevice) return SoundDevice->GetExtList();
  (void)alcGetError(nullptr);
  const ALCchar *list = alcGetString(NULL, ALC_EXTENSIONS);
  ALCenum err = alcGetError(nullptr);
  return (err == ALC_NO_ERROR ? list : nullptr);
}
