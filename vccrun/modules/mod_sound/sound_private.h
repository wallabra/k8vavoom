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
#ifndef VCCMOD_SOUND_PRIVATE_HEADER_FILE
#define VCCMOD_SOUND_PRIVATE_HEADER_FILE

#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#ifndef OPENAL
# define OPENAL
#endif

//#include "../../vcc_run.h"
//#include "../../filesys/fsys.h"

//#include "../../vcc_run.h"
#include "../../../libs/core/core.h"


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
// sound device interface
// as we won't support anything except OpenAL Soft, don't bother making this virtual
class VOpenALDevice {
private:
  enum { MAX_VOICES = 256-4 };

  enum { NUM_STRM_BUFFERS = 8*2 };
  enum { STRM_BUFFER_SIZE = 1024*4 };

  ALCdevice *Device;
  ALCcontext *Context;
  ALuint *Buffers;
  vint32 BufferCount;

  ALuint StrmSampleRate;
  ALuint StrmFormat;
  ALuint StrmBuffers[NUM_STRM_BUFFERS];
  ALuint StrmAvailableBuffers[NUM_STRM_BUFFERS];
  int StrmNumAvailableBuffers;
  ALuint StrmSource;
  short StrmDataBuffer[STRM_BUFFER_SIZE*2];

  // for debugging
  //TVec listenerPos;

  bool PrepareSound (int sound_id);

public:
  VOpenALDevice () : Device(nullptr), Context(nullptr), Buffers(nullptr), BufferCount(0) {}

  // VSoundDevice interface
  bool Init ();
  int SetChannels (int InNumChannels);
  void Shutdown ();

  int PlaySound3D (int sound_id, const TVec &origin, const TVec &velocity, float volume, float pitch, bool Loop, bool relative);
  void UpdateChannel3D (int Handle, const TVec &Org, const TVec &Vel, bool relative);
  void UpdateChannelPitch (int Handle, float pitch);
  void UpdateChannelVolume (int Handle, float volume);
  bool IsChannelActive (int Handle);
  void StopChannel (int Handle);
  void PauseChannel (int Handle);
  void ResumeChannel (int Handle);
  void UpdateListener (const TVec &org, const TVec &vel, const TVec &fwd, const TVec &up);

  // OpenAL is thread-safe, so we have nothing special to do here
  bool OpenStream (int Rate, int Bits, int Channels);
  void CloseStream ();
  int GetStreamAvailable ();
  short *GetStreamBuffer ();
  void SetStreamData (short *data, int len);
  void SetStreamVolume (float vol);
  void PauseStream ();
  void ResumeStream ();
  void SetStreamPitch (float pitch);

  void AddCurrentThread ();
  void RemoveCurrentThread ();

  const char *GetDevList ();
  const char *GetAllDevList ();
  const char *GetExtList ();

public:
  // set the following BEFORE initializing sound
  static float doppler_factor;
  static float doppler_velocity;
  static float rolloff_factor;
  static float reference_distance; // The distance under which the volume for the source would normally drop by half (before being influenced by rolloff factor or AL_MAX_DISTANCE)
  static float max_distance; // Used with the Inverse Clamped Distance Model to set the distance where there will no longer be any attenuation of the source
};


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
  VOpenALDevice *SoundDevice;

public:
  VStreamMusicPlayer (VOpenALDevice *InSoundDevice)
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


#endif
