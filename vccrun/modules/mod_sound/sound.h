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
enum {
  SNDDRV_OpenAL,

  SNDDRV_MAX,
};


// ////////////////////////////////////////////////////////////////////////// //
// SoundFX struct
struct sfxinfo_t {
  VName tagName; // name, by whitch sound is recognised in script

  int priority;  // higher priority takes precendence
  int numChannels; // total number of channels a sound type may occupy
  float changePitch;

  bool bRandomHeader;
  bool bPlayerReserve;
  bool bSingular;

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
  virtual int PlaySound (int sound_id, float volume, float sep, float pitch, bool Loop) = 0;
  virtual int PlaySound3D (int sound_id, const TVec &origin, const TVec &velocity, float volume, float pitch, bool Loop) = 0;
  virtual void UpdateChannel (int Handle, float vol, float sep) = 0;
  virtual void UpdateChannel3D (int Handle, const TVec &Org, const TVec &Vel) = 0;
  virtual void UpdateChannelPitch (int Handle, float pitch) = 0;
  virtual bool IsChannelPlaying (int Handle) = 0;
  virtual void StopChannel (int Handle) = 0;
  virtual void UpdateListener (const TVec &org, const TVec &vel, const TVec &fwd, const TVec&, const TVec &up) = 0;

  virtual bool OpenStream (int Rate, int Bits, int Channels) = 0;
  virtual void CloseStream () = 0;
  virtual int GetStreamAvailable () = 0;
  virtual short *GetStreamBuffer () = 0;
  virtual void SetStreamData (short *data, int len) = 0;
  virtual void SetStreamVolume (float vol) = 0;
  virtual void PauseStream () = 0;
  virtual void ResumeStream () = 0;
  virtual void SetStreamPitch (float pitch) = 0;

  // set the following BEFORE initializing sound
  static float doppler_factor;
  static float doppler_velocity;
  static float rolloff_factor;
  static float reference_distance; // The distance under which the volume for the source would normally drop by half (before being influenced by rolloff factor or AL_MAX_DISTANCE)
  static float max_distance; // Used with the Inverse Clamped Distance Model to set the distance where there will no longer be any attenuation of the source
};


// description of a sound driver
struct FSoundDeviceDesc {
  const char *Name;
  const char *Description;
  const char *CmdLineArg;
  VSoundDevice *(*Creator) ();

  FSoundDeviceDesc (int Type, const char *AName, const char *ADescription, const char *ACmdLineArg, VSoundDevice *(*ACreator)());
};


// sound device registration helper
#define IMPLEMENT_SOUND_DEVICE(TClass, Type, Name, Description, CmdLineArg) \
VSoundDevice *Create##TClass()  { \
  return new TClass(); \
} \
FSoundDeviceDesc TClass##Desc(Type, Name, Description, CmdLineArg, Create##TClass);


// ////////////////////////////////////////////////////////////////////////// //
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
  bool StrmOpened;
  VAudioCodec *Codec;
  // current playing song info
  bool CurrLoop;
  VStr CurrSong;
  bool Stopping;
  bool Paused;
  double FinishTime;
  VSoundDevice *SoundDevice;

  VStreamMusicPlayer (VSoundDevice *InSoundDevice)
    : StrmOpened(false)
    , Codec(nullptr)
    , CurrLoop(false)
    , Stopping(false)
    , Paused(false)
    , SoundDevice(InSoundDevice)
  {}

  ~VStreamMusicPlayer () {}

  void Init ();
  void Shutdown ();
  void Tick ();
  void Play (VAudioCodec *InCodec, const VStr &InName, bool InLoop);
  void Pause ();
  void Resume ();
  void Stop ();
  bool IsPlaying ();
  void SetPitch (float pitch);
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

  static void StaticInitialize ();
  static void StaticShutdown ();

private:
  struct FPlayerSound {
    int ClassId;
    int GenderId;
    int RefId;
    int SoundId;
  };

  enum { NUM_AMBIENT_SOUNDS = 256 };

  struct VMusicVolume {
    VName SongName;
    float Volume;
  };

  int NumPlayerReserves;
  float CurrentChangePitch;
  int SeqTrans[64*3];

  // mapping from sound name to sfx index
  TMap<VName, vint32> name2idx;

  bool LoadSound (int sound_id, const VStr &filename);
};


// ////////////////////////////////////////////////////////////////////////// //
// main audio management class
class VAudioPublic : public VInterface {
public:
  // top level methods
  virtual void Init () = 0;
  virtual void Shutdown () = 0;

  // playback of sound effects
  virtual int PlaySound (int InSoundId, const TVec &origin, const TVec &velocity, int origin_id, int channel, float volume, float Attenuation, bool Loop) = 0;
  virtual void StopSound (int origin_id, int channel) = 0;
  virtual void StopAllSound () = 0;
  virtual bool IsSoundPlaying (int origin_id, int InSoundId) = 0;
  virtual void SetSoundPitch (int origin_id, int InSoundId, float pitch) = 0;

  // general sound control
  virtual void UpdateSounds () = 0;

  // call this before `UpdateSounds()`
  virtual void SetListenerOrigin (const TVec &aorigin) = 0;

  // music playback
  virtual bool PlayMusic (const VStr &filename, bool Loop) = 0;
  virtual bool IsMusicPlaying () = 0;
  virtual void PauseMusic () = 0;
  virtual void ResumeMusic () = 0;
  virtual void StopMusic () = 0;
  virtual void SetMusicPitch (float pitch) = 0;

  static VAudioPublic *Create ();

public:
  static float snd_sfx_volume;
  static float snd_music_volume;
  static bool snd_swap_stereo;
  static int snd_channels;
};

//extern VCvarB snd_mid_player;
//extern VCvarB snd_mod_player;

extern VSoundManager *GSoundManager;
extern VAudioPublic *GAudio;


// ////////////////////////////////////////////////////////////////////////// //
class VSoundSystem : public VObject {
  DECLARE_CLASS(VSoundSystem, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VSoundSystem)

public:
  //virtual void Destroy () override;

public:
  DECLARE_FUNCTION(Initialize)
  //DECLARE_FUNCTION(Destroy)

  DECLARE_FUNCTION(AddSound)
  DECLARE_FUNCTION(FindSound)

  DECLARE_FUNCTION(PlaySound)
  DECLARE_FUNCTION(StopSound)
  DECLARE_FUNCTION(StopAllSound)
  DECLARE_FUNCTION(IsSoundPlaying)
  DECLARE_FUNCTION(SetSoundPitch)

  DECLARE_FUNCTION(UpdateSounds)

  DECLARE_FUNCTION(set_ListenerOrigin)

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

  DECLARE_FUNCTION(get_DopplerFactor)
  DECLARE_FUNCTION(set_DopplerFactor)
  DECLARE_FUNCTION(get_DopplerVelocity)
  DECLARE_FUNCTION(set_DopplerVelocity)
  DECLARE_FUNCTION(get_RolloffFactor)
  DECLARE_FUNCTION(set_RolloffFactor)
  DECLARE_FUNCTION(get_ReferenceDistance)
  DECLARE_FUNCTION(set_ReferenceDistance)
  DECLARE_FUNCTION(get_MaxDistance)
  DECLARE_FUNCTION(set_MaxDistance)
  DECLARE_FUNCTION(get_NumChannels)
  DECLARE_FUNCTION(set_NumChannels)
};


#endif
