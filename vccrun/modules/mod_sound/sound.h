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


// ////////////////////////////////////////////////////////////////////////// //
// SoundFX struct
struct sfxinfo_t {
  VName tagName; // name, by whitch sound is recognised in script

  int priority; // higher priority takes precendence (can be modified by user)
  int numChannels; // total number of channels a sound type may occupy (can be modified by user)
  float changePitch; // if not zero, do pitch randomisation
  bool bSingular; // if true, only one instance of this sound may play

  vuint32 sampleRate;
  int sampleBits;
  vuint32 dataSize;
  vuint8 *data;
};


// ////////////////////////////////////////////////////////////////////////// //
// sound device interface: this class implements dummy driver
class VSoundDevice : public VInterface {
public:
  VSoundDevice () {}

  // VSoundDevice interface
  virtual bool Init () = 0;
  virtual int SetChannels (int InNumChannels) = 0;
  virtual void Shutdown () = 0;
  virtual int PlaySound3D (int sound_id, const TVec &origin, const TVec &velocity, float volume, float pitch, bool Loop, bool relative) = 0;
  virtual void UpdateChannel3D (int Handle, const TVec &Org, const TVec &Vel, bool relative) = 0;
  virtual void UpdateChannelPitch (int Handle, float pitch) = 0;
  virtual void UpdateChannelVolume (int Handle, float volume) = 0;
  virtual bool IsChannelActive (int Handle) = 0;
  virtual void StopChannel (int Handle) = 0;
  virtual void PauseChannel (int Handle) = 0;
  virtual void ResumeChannel (int Handle) = 0;
  virtual void UpdateListener (const TVec &org, const TVec &vel, const TVec &fwd, const TVec &up) = 0;

  // all stream functions should be thread-safe
  virtual bool OpenStream (int Rate, int Bits, int Channels) = 0;
  virtual void CloseStream () = 0;
  virtual int GetStreamAvailable () = 0;
  virtual short *GetStreamBuffer () = 0;
  virtual void SetStreamData (short *data, int len) = 0;
  virtual void SetStreamVolume (float vol) = 0;
  virtual void PauseStream () = 0;
  virtual void ResumeStream () = 0;
  virtual void SetStreamPitch (float pitch) = 0;

  virtual void AddCurrentThread () = 0;
  virtual void RemoveCurrentThread () = 0;

  virtual const char *GetDevList () = 0;
  virtual const char *GetAllDevList () = 0;
  virtual const char *GetExtList () = 0;

  // set the following BEFORE initializing sound
  static float doppler_factor;
  static float doppler_velocity;
  static float rolloff_factor;
  static float reference_distance; // The distance under which the volume for the source would normally drop by half (before being influenced by rolloff factor or AL_MAX_DISTANCE)
  static float max_distance; // Used with the Inverse Clamped Distance Model to set the distance where there will no longer be any attenuation of the source
};


VSoundDevice *CreateVSoundDevice ();


// ////////////////////////////////////////////////////////////////////////// //
class VAudioCodec;

// loader of sound samples
class VSampleLoader : public VInterface {
public:
  VSampleLoader *Next;

  static VSampleLoader *List;

  VSampleLoader () {
    Next = List;
    List = this;
  }

  virtual void Load (sfxinfo_t &, VStream &) = 0;

  // codec must be initialized, and it will not be owned
  void LoadFromAudioCodec (sfxinfo_t &Sfx, VAudioCodec *Codec);
};


// ////////////////////////////////////////////////////////////////////////// //
// streamed audio decoder interface
class VAudioCodec : public VInterface {
public:
  int SampleRate;
  int SampleBits;
  int NumChannels;

  VAudioCodec () : SampleRate(44100), SampleBits(16), NumChannels(2) {}

  virtual int Decode (short*, int) = 0;
  virtual bool Finished () = 0;
  virtual void Restart () = 0;
};


// description of an audio codec
struct FAudioCodecDesc {
  const char *Description;
  VAudioCodec *(*Creator)(VStream*);
  FAudioCodecDesc *Next;

  static FAudioCodecDesc *List;

  FAudioCodecDesc (const char *InDescription, VAudioCodec *(*InCreator)(VStream*))
    : Description(InDescription)
    , Creator(InCreator)
  {
    Next = List;
    List = this;
  }
};

// audio codec registration helper
#define IMPLEMENT_AUDIO_CODEC(TClass, Description) \
FAudioCodecDesc TClass##Desc(Description, TClass::Create);


// ////////////////////////////////////////////////////////////////////////// //
class VStreamMusicPlayer {
public:
  // stream player is using a separate thread
  mythread stpThread;
  mythread_mutex stpPingLock;
  mythread_cond stpPingCond;
  mythread_mutex stpLockPong;
  mythread_cond stpCondPong;
  float lastVolume;
  bool threadInited;

public:
  bool StrmOpened;
  VAudioCodec *Codec;
  // current playing song info
  bool CurrLoop;
  VStr CurrSong;
  bool Stopping;
  bool Paused;
  double FinishTime;
  VSoundDevice *SoundDevice;

public:
  VStreamMusicPlayer (VSoundDevice *InSoundDevice)
    : lastVolume(1.0)
    , threadInited(false)
    , StrmOpened(false)
    , Codec(nullptr)
    , CurrLoop(false)
    , Stopping(false)
    , Paused(false)
    , SoundDevice(InSoundDevice)
    , stpIsPlaying(false)
    , stpNewPitch(1.0)
    , stpNewVolume(1.0)
  {}

  ~VStreamMusicPlayer () {}

  void Init ();
  void Shutdown ();
  void Play (VAudioCodec *InCodec, const VStr &InName, bool InLoop);
  void Pause ();
  void Resume ();
  void Stop ();
  bool IsPlaying ();
  void SetPitch (float pitch);
  void SetVolume (float volume);

  // streamer thread ping/pong bussiness
  // k8: it is public to free me from fuckery with `friends`
  enum STPCommand {
    STP_Quit, // stop playing, and quit immediately
    STP_Start, // start playing current stream
    STP_Stop, // stop current stream
    STP_Pause, // pause current stream
    STP_Resume, // resume current stream
    STP_IsPlaying, // check if current stream is playing
    STP_SetPitch, // change stream pitch
    STP_SetVolume,
  };
  volatile STPCommand stpcmd;
  volatile bool stpIsPlaying; // it will return `STP_IsPlaying` result here
  volatile float stpNewPitch;
  volatile float stpNewVolume;

  bool stpThreadWaitPing (unsigned int msecs);
  void stpThreadSendPong ();

  void stpThreadSendCommand (STPCommand acmd);
};


// ////////////////////////////////////////////////////////////////////////// //
// handles list of registered sound and sound sequences
class VSoundManager {
public:
  // the complete set of sound effects
  // WARNING! DON'T MODIFY!
  TArray<sfxinfo_t> S_sfx;

  VSoundManager ();
  virtual ~VSoundManager ();

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
class VAudioPublic : public VInterface {
public:
  // lock updates before changing this!
  // TODO: wrap in API
  TVec ListenerForward;
  TVec ListenerUp;
  TVec ListenerOrigin;
  TVec ListenerVelocity;

public:
  // top level methods
  virtual void Init () = 0;
  virtual void Shutdown () = 0;

  // playback of sound effects
  virtual int PlaySound (int sound_id, const TVec &origin, const TVec &velocity, int origin_id, int channel,
                         float volume, float attenuation, float pitch, bool Loop) = 0;

  virtual bool IsSoundActive (int origin_id, int sound_id) = 0;
  virtual bool IsSoundPaused (int origin_id, int sound_id) = 0;
  virtual void StopSound (int origin_id, int sound_id) = 0;
  virtual void PauseSound (int origin_id, int sound_id) = 0;
  virtual void ResumeChannel (int origin_id, int channel) = 0;

  virtual void SetSoundPitch (int origin_id, int sound_id, float pitch) = 0;
  virtual void SetSoundOrigin (int origin_id, int sound_id, const TVec &origin) = 0;
  virtual void SetSoundVelocity (int origin_id, int sound_id, const TVec &velocity) = 0;
  virtual void SetSoundVolume (int origin_id, int sound_id, float volume) = 0;
  virtual void SetSoundAttenuation (int origin_id, int sound_id, float attenuation) = 0;

  virtual bool IsChannelActive (int origin_id, int channel) = 0;
  virtual bool IsChannelPaused (int origin_id, int channel) = 0;
  virtual void StopChannel (int origin_id, int channel) = 0;
  virtual void PauseChannel (int origin_id, int channel) = 0;
  virtual void ResumeSound (int origin_id, int sound_id) = 0;

  virtual void SetChannelPitch (int origin_id, int channel, float pitch) = 0;
  virtual void SetChannelOrigin (int origin_id, int channel, const TVec &origin) = 0;
  virtual void SetChannelVelocity (int origin_id, int channel, const TVec &velocity) = 0;
  virtual void SetChannelVolume (int origin_id, int channel, float volume) = 0;
  virtual void SetChannelAttenuation (int origin_id, int channel, float attenuation) = 0;

  virtual void StopSounds () = 0;
  virtual void PauseSounds () = 0;
  virtual void ResumeSounds () = 0;

  // <0: not found
  virtual int FindInternalChannelForSound (int origin_id, int sound_id) = 0;
  virtual int FindInternalChannelForChannel (int origin_id, int channel) = 0;

  virtual bool IsInternalChannelPlaying (int ichannel) = 0;
  virtual bool IsInternalChannelPaused (int ichannel) = 0;
  virtual void StopInternalChannel (int ichannel) = 0;
  virtual void PauseInternalChannel (int ichannel) = 0;
  virtual void ResumeInternalChannel (int ichannel) = 0;
  virtual bool IsInternalChannelRelative (int ichannel) = 0;
  virtual void SetInternalChannelRelative (int ichannel, bool relative) = 0;

  // music playback
  virtual bool PlayMusic (const VStr &filename, bool Loop) = 0;
  virtual bool IsMusicPlaying () = 0;
  virtual void PauseMusic () = 0;
  virtual void ResumeMusic () = 0;
  virtual void StopMusic () = 0;
  virtual void SetMusicPitch (float pitch) = 0;

  // balance this!
  virtual void LockUpdates () = 0;
  virtual void UnlockUpdates () = 0;

  virtual const char *GetDevList () = 0;
  virtual const char *GetAllDevList () = 0;
  virtual const char *GetExtList () = 0;

  static VAudioPublic *Create ();

public:
  static float snd_sfx_volume;
  static float snd_music_volume;
  static bool snd_swap_stereo;
  static int snd_channels;
  static int snd_max_distance;
};

//extern VCvarB snd_mid_player;
//extern VCvarB snd_mod_player;

extern VSoundManager *GSoundManager;
extern VAudioPublic *GAudio;


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


#endif
