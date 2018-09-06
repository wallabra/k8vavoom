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
#ifndef _WIN32
# define INITGUID
#else
# include "winshit/winlocal.h"
#endif
#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
// linux headers doesn't define this
#ifndef OPENAL
# define OPENAL
#endif

#include "sound/eax.h"

#include "gamedefs.h"
#include "snd_local.h"

class VOpenALDevice : public VSoundDevice
{
private:
  enum { MAX_VOICES = 256 };

  //enum { NUM_STRM_BUFFERS = 8 };
  //enum { STRM_BUFFER_SIZE = 1024 };

  enum { NUM_STRM_BUFFERS = 8*2 };
  enum { STRM_BUFFER_SIZE = 1024*8 };

  ALCdevice *Device;
  ALCcontext *Context;
  ALuint *Buffers;
  vint32 BufferCount;

  bool    supportEAX;
  EAXGet    pEAXGet;
  EAXSet    pEAXSet;

  ALuint StrmSampleRate;
  ALuint StrmFormat;
  ALuint StrmBuffers[NUM_STRM_BUFFERS];
  ALuint StrmAvailableBuffers[NUM_STRM_BUFFERS];
  int StrmNumAvailableBuffers;
  ALuint StrmSource;
  short StrmDataBuffer[STRM_BUFFER_SIZE * 2];

  static VCvarF doppler_factor;
  static VCvarF doppler_velocity;
  static VCvarF rolloff_factor;
  static VCvarF reference_distance;
  static VCvarF max_distance;
  static VCvarI eax_environment;

public:
  //  VSoundDevice interface.
  virtual bool Init () override;
  virtual int SetChannels (int) override;
  virtual void Shutdown () override;
  //virtual void Tick(float);
  virtual int PlaySound (int, float, float, float, bool) override;
  virtual int PlaySound3D (int, const TVec&, const TVec&, float, float, bool) override;
  virtual void UpdateChannel3D (int, const TVec&, const TVec&) override;
  virtual bool IsChannelPlaying (int) override;
  virtual void StopChannel (int) override;
  virtual void UpdateListener (const TVec&, const TVec&, const TVec&, const TVec&, const TVec&, VReverbInfo*) override;

  virtual bool OpenStream (int, int, int) override;
  virtual void CloseStream () override;
  virtual int GetStreamAvailable () override;
  virtual short *GetStreamBuffer () override;
  virtual void SetStreamData (short*, int) override;
  virtual void SetStreamVolume (float) override;
  virtual void SetStreamPitch (float pitch) override;
  virtual void PauseStream () override;
  virtual void ResumeStream () override;

  virtual void AddCurrentThread () override;
  virtual void RemoveCurrentThread () override;

  bool LoadSound(int);
};

IMPLEMENT_SOUND_DEVICE(VOpenALDevice, SNDDRV_OpenAL, "OpenAL", "OpenAL sound device", "-openal");

VCvarF VOpenALDevice::doppler_factor("snd_al_doppler_factor", "1.0", "OpenAL doppler factor.", CVAR_Archive);
VCvarF VOpenALDevice::doppler_velocity("snd_al_doppler_velocity", "10000.0", "OpenAL doppler velocity.", CVAR_Archive);
VCvarF VOpenALDevice::rolloff_factor("snd_al_rolloff_factor", "1.0", "OpenAL rolloff factor.", CVAR_Archive);
VCvarF VOpenALDevice::reference_distance("snd_al_reference_distance", "64.0", "OpenAL reference distance.", CVAR_Archive);
VCvarF VOpenALDevice::max_distance("snd_al_max_distance", "2024.0", "OpenAL max distance.", CVAR_Archive);
VCvarI VOpenALDevice::eax_environment("snd_al_eax_environment", "0", "OpenAL EAX environment id.");

static VCvarB openal_show_extensions("openal_show_extensions", false, "Show available OpenAL extensions?", CVAR_Archive);


//==========================================================================
//
//  VOpenALDevice::Init
//
//  Inits sound
//
//==========================================================================

bool VOpenALDevice::Init()
{
  guard(VOpenALDevice::Init);
  ALenum E;

  Device = nullptr;
  Context = nullptr;
  Buffers = nullptr;
  BufferCount = 0;
  supportEAX = false;
  pEAXGet = nullptr;
  pEAXSet = nullptr;
  StrmSource = 0;
  StrmNumAvailableBuffers = 0;

  //  Connect to a device.
  Device = alcOpenDevice(nullptr);
  if (!Device) {
    GCon->Log(NAME_Init, "Couldn't open OpenAL device");
    return false;
  }
  //  In Linux it's not implemented.
  if (openal_show_extensions) {
#ifdef ALC_DEVICE_SPECIFIER
    GCon->Logf(NAME_Init, "Opened OpenAL device %s", alcGetString(Device, ALC_DEVICE_SPECIFIER));
#endif
  }

  // create a context and make it current
  static const ALCint attrs[] = {
    ALC_STEREO_SOURCES, 1, // get at least one stereo source for music
    ALC_MONO_SOURCES, 1, // this should be audio channels in our game engine
    //ALC_FREQUENCY, 48000, // desired frequency; we don't really need this, let OpenAL choose the best
    0,
  };
  Context = alcCreateContext(Device, attrs);
  if (!Context) Sys_Error("Failed to create OpenAL context");
  //alcMakeContextCurrent(Context);
  alcSetThreadContext(Context);
  E = alGetError();
  if (E != AL_NO_ERROR) Sys_Error("OpenAL error: %s", alGetString(E));

  alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

  //  Print some information.
  if (openal_show_extensions) {
    GCon->Logf(NAME_Init, "AL_VENDOR: %s", alGetString(AL_VENDOR));
    GCon->Logf(NAME_Init, "AL_RENDERER: %s", alGetString(AL_RENDERER));
    GCon->Logf(NAME_Init, "AL_VERSION: %s", alGetString(AL_VERSION));
    GCon->Log(NAME_Init, "AL_EXTENSIONS:");
    TArray<VStr> Exts;
    VStr((char*)alGetString(AL_EXTENSIONS)).Split(' ', Exts);
    for (int i = 0; i < Exts.Num(); i++) GCon->Log(NAME_Init, VStr("- ") + Exts[i]);
    GCon->Log(NAME_Init, "ALC_EXTENSIONS:");
    VStr((char*)alcGetString(Device, ALC_EXTENSIONS)).Split(' ', Exts);
    for (int i = 0; i < Exts.Num(); i++) GCon->Log(NAME_Init, VStr("- ") + Exts[i]);
  }

  if (alIsExtensionPresent((ALchar*)"EAX")) {
    GCon->Log(NAME_Init, "EAX 2.0 supported");
    pEAXSet = (EAXSet)alGetProcAddress((ALchar*)"EAXSet");
    pEAXGet = (EAXGet)alGetProcAddress((ALchar*)"EAXGet");
    supportEAX = true;
  }

  //  Allocate array for buffers.
  /*
  Buffers = new ALuint[GSoundManager->S_sfx.Num()];
  memset(Buffers, 0, sizeof(ALuint) * GSoundManager->S_sfx.Num());
  */

  GCon->Log(NAME_Init, "OpenAL initialized.");
  return true;
  unguard;
}


//==========================================================================
//
//  VOpenALDevice::AddCurrentThread
//
//==========================================================================
void VOpenALDevice::AddCurrentThread () {
  alcSetThreadContext(Context);
}


//==========================================================================
//
//  VOpenALDevice::RemoveCurrentThread
//
//==========================================================================
void VOpenALDevice::RemoveCurrentThread () {
  alcSetThreadContext(nullptr);
}


//==========================================================================
//
//  VOpenALDevice::SetChannels
//
//==========================================================================
int VOpenALDevice::SetChannels (int InNumChannels) {
  guard(VOpenALDevice::SetChannels);
  int NumChannels = MAX_VOICES;
  if (NumChannels > InNumChannels) NumChannels = InNumChannels;
  return NumChannels;
  unguard;
}


//==========================================================================
//
//  VOpenALDevice::Shutdown
//
//==========================================================================
void VOpenALDevice::Shutdown () {
  guard(VOpenALDevice::Shutdown);

  // delete buffers
  if (Buffers) {
    //alDeleteBuffers(GSoundManager->S_sfx.length(), Buffers);
    for (int bidx = 0; bidx < BufferCount; ++bidx) {
      if (Buffers[bidx]) {
        alDeleteBuffers(1, Buffers+bidx);
        Buffers[bidx] = 0;
      }
    }
    delete[] Buffers;
    Buffers = nullptr;
  }
  BufferCount = 0;

  //  Destroy context.
  if (Context) {
    alcSetThreadContext(nullptr);
    alcDestroyContext(Context);
    Context = nullptr;
  }

  //  Disconnect from a device.
  if (Device)
  {
    alcCloseDevice(Device);
    Device = nullptr;
  }
  unguard;
}


//==========================================================================
//
//  VOpenALDevice::LoadSound
//
//==========================================================================

bool VOpenALDevice::LoadSound(int sound_id)
{
  guard(VOpenALDevice::LoadSound);

  if (BufferCount < sound_id+1) {
    int newsz = ((sound_id+4)|0xfff)+1;
    ALuint *newbuf = new ALuint[newsz];
    for (int f = BufferCount; f < newsz; ++f) newbuf[f] = 0;
    delete[] Buffers;
    Buffers = newbuf;
    BufferCount = newsz;
  }

  if (Buffers[sound_id])
  {
    return true;
  }

  //  Check, that sound lump is loaded
  if (!GSoundManager->LoadSound(sound_id))
  {
    //  Missing sound.
    return false;
  }

  //  Clear error code.
  alGetError();

  //  Create buffer.
  alGenBuffers(1, &Buffers[sound_id]);
  if (alGetError() != AL_NO_ERROR)
  {
    GCon->Log(NAME_Dev, "Failed to gen buffer");
    GSoundManager->DoneWithLump(sound_id);
    return false;
  }

  //  Load buffer data.
  alBufferData(Buffers[sound_id],
    GSoundManager->S_sfx[sound_id].SampleBits == 8 ? AL_FORMAT_MONO8 :
    AL_FORMAT_MONO16, GSoundManager->S_sfx[sound_id].Data,
    GSoundManager->S_sfx[sound_id].DataSize,
    GSoundManager->S_sfx[sound_id].SampleRate);
  if (alGetError() != AL_NO_ERROR)
  {
    GCon->Log(NAME_Dev, "Failed to load buffer data");
    GSoundManager->DoneWithLump(sound_id);
    return false;
  }

  //  We don't need to keep lump static
  GSoundManager->DoneWithLump(sound_id);
  return true;
  unguard;
}

//==========================================================================
//
//  VOpenALDevice::PlaySound
//
//  This function adds a sound to the list of currently active sounds, which
// is maintained as a given number of internal channels.
//
//==========================================================================

int VOpenALDevice::PlaySound(int sound_id, float volume, float, float pitch,
  bool Loop)
{
  guard(VOpenALDevice::PlaySound);
  if (!LoadSound(sound_id))
  {
    return -1;
  }

  ALuint src;
  alGetError(); //  Clear error code.
  alGenSources(1, &src);
  if (alGetError() != AL_NO_ERROR)
  {
    GCon->Log(NAME_Dev, "Failed to gen source");
    return -1;
  }

  alSourcei(src, AL_BUFFER, Buffers[sound_id]);

  alSourcef(src, AL_GAIN, volume);
  alSourcef(src, AL_ROLLOFF_FACTOR, rolloff_factor);
  alSourcei(src, AL_SOURCE_RELATIVE, AL_TRUE);
  alSource3f(src, AL_POSITION, 0.0, 0.0, -16.0);
  alSourcef(src, AL_REFERENCE_DISTANCE, reference_distance);
  alSourcef(src, AL_MAX_DISTANCE, max_distance);
  alSourcef(src, AL_PITCH, pitch);
  if (Loop) alSourcei(src, AL_LOOPING, AL_TRUE);
  alSourcePlay(src);
  return src;
  unguard;
}

//==========================================================================
//
//  VOpenALDevice::PlaySound3D
//
//==========================================================================

int VOpenALDevice::PlaySound3D(int sound_id, const TVec &origin,
  const TVec &velocity, float volume, float pitch, bool Loop)
{
  guard(VOpenALDevice::PlaySound3D);
  if (!LoadSound(sound_id))
  {
    return -1;
  }

  ALuint src;
  alGetError(); //  Clear error code.
  alGenSources(1, &src);
  if (alGetError() != AL_NO_ERROR)
  {
    GCon->Log(NAME_Dev, "Failed to gen source");
    return -1;
  }

  alSourcei(src, AL_BUFFER, Buffers[sound_id]);

  alSourcef(src, AL_GAIN, volume);
  alSourcef(src, AL_ROLLOFF_FACTOR, rolloff_factor);
  alSourcei(src, AL_SOURCE_RELATIVE, AL_FALSE); // just in case
  alSource3f(src, AL_POSITION, origin.x, origin.y, origin.z);
  alSource3f(src, AL_VELOCITY, velocity.x, velocity.y, velocity.z);
  alSourcef(src, AL_REFERENCE_DISTANCE, reference_distance);
  alSourcef(src, AL_MAX_DISTANCE, max_distance);
  alSourcef(src, AL_PITCH, pitch);
  if (Loop) alSourcei(src, AL_LOOPING, AL_TRUE);
  alSourcePlay(src);
  return src;
  unguard;
}


//==========================================================================
//
//  VOpenALDevice::UpdateChannel3D
//
//==========================================================================

void VOpenALDevice::UpdateChannel3D(int Handle, const TVec &Org,
  const TVec &Vel)
{
  guard(VOpenALDevice::UpdateChannel3D);
  if (Handle == -1)
  {
    return;
  }
  alSource3f(Handle, AL_POSITION, Org.x, Org.y, Org.z);
  alSource3f(Handle, AL_VELOCITY, Vel.x, Vel.y, Vel.z);
  unguard;
}

//==========================================================================
//
//  VOpenALDevice::IsChannelPlaying
//
//==========================================================================

bool VOpenALDevice::IsChannelPlaying(int Handle)
{
  guard(VOpenALDevice::IsChannelPlaying);
  if (Handle == -1)
  {
    return false;
  }
  ALint State;
  alGetSourcei(Handle, AL_SOURCE_STATE, &State);
  return State == AL_PLAYING;
  unguard;
}

//==========================================================================
//
//  VOpenALDevice::StopChannel
//
//  Stop the sound. Necessary to prevent runaway chainsaw, and to stop
// rocket launches when an explosion occurs.
//  All sounds MUST be stopped;
//
//==========================================================================

void VOpenALDevice::StopChannel(int Handle)
{
  guard(VOpenALDevice::StopChannel);
  if (Handle == -1)
  {
    return;
  }
  //  Stop buffer
  alSourceStop(Handle);
  alDeleteSources(1, (ALuint*)&Handle);
  unguard;
}

//==========================================================================
//
//  VOpenALDevice::UpdateListener
//
//==========================================================================

void VOpenALDevice::UpdateListener(const TVec &org, const TVec &vel,
  const TVec &fwd, const TVec&, const TVec &up, VReverbInfo *Env)
{
  guard(VOpenALDevice::UpdateListener);
  alListener3f(AL_POSITION, org.x, org.y, org.z);
  alListener3f(AL_VELOCITY, vel.x, vel.y, vel.z);

  ALfloat orient[6] = { fwd.x, fwd.y, fwd.z, up.x, up.y, up.z};
  alListenerfv(AL_ORIENTATION, orient);

  alDopplerFactor(doppler_factor);
  alDopplerVelocity(doppler_velocity);

  if (supportEAX)
  {
    EAXLISTENERPROPERTIES Prop;
    Prop.lRoom = Env->Props.Room;
    Prop.lRoomHF = Env->Props.RoomHF;
    Prop.flRoomRolloffFactor = Env->Props.RoomRolloffFactor;
    Prop.flDecayTime = Env->Props.DecayTime;
    Prop.flDecayHFRatio = Env->Props.DecayHFRatio;
    Prop.lReflections = Env->Props.Reflections;
    Prop.flReflectionsDelay = Env->Props.ReflectionsDelay;
    Prop.lReverb = Env->Props.Reverb;
    Prop.flReverbDelay = Env->Props.ReverbDelay;
    Prop.dwEnvironment = Env->Props.Environment;
    Prop.flEnvironmentSize = Env->Props.EnvironmentSize;
    Prop.flEnvironmentDiffusion = Env->Props.EnvironmentDiffusion;
    Prop.flAirAbsorptionHF = Env->Props.AirAbsorptionHF;
    Prop.dwFlags = Env->Props.Flags & 0x3f;
    pEAXSet(&DSPROPSETID_EAX_ListenerProperties,
      DSPROPERTY_EAXLISTENER_ALLPARAMETERS, 0, &Prop, sizeof(Prop));

    if (Env->Id == 1)
    {
      int envId = eax_environment;
      if (envId < 0 || envId >= EAX_ENVIRONMENT_COUNT)
        envId = EAX_ENVIRONMENT_GENERIC;
      pEAXSet(&DSPROPSETID_EAX_ListenerProperties,
        DSPROPERTY_EAXLISTENER_ENVIRONMENT, 0, &envId, sizeof(int));

      float envSize = GAudio->EAX_CalcEnvSize();
      pEAXSet(&DSPROPSETID_EAX_ListenerProperties,
        DSPROPERTY_EAXLISTENER_ENVIRONMENTSIZE, 0, &envSize, sizeof(float));
    }
  }
  unguard;
}

//==========================================================================
//
//  VOpenALDevice::OpenStream
//
//==========================================================================

bool VOpenALDevice::OpenStream(int Rate, int Bits, int Channels)
{
  guard(VOpenALDevice::OpenStream);
  StrmSampleRate = Rate;
  StrmFormat = Channels == 2 ?
    Bits == 8 ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16 :
    Bits == 8 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16;

  alGetError(); //  Clear error code.
  alGenSources(1, &StrmSource);
  if (alGetError() != AL_NO_ERROR)
  {
    GCon->Log(NAME_Dev, "Failed to gen source");
    return false;
  }
  alSourcei(StrmSource, AL_SOURCE_RELATIVE, AL_TRUE);
  alGenBuffers(NUM_STRM_BUFFERS, StrmBuffers);
  alSourceQueueBuffers(StrmSource, NUM_STRM_BUFFERS, StrmBuffers);
  alSourcePlay(StrmSource);
  StrmNumAvailableBuffers = 0;
  return true;
  unguard;
}

//==========================================================================
//
//  VOpenALDevice::CloseStream
//
//==========================================================================

void VOpenALDevice::CloseStream()
{
  guard(VOpenALDevice::CloseStream);
  if (StrmSource)
  {
    alDeleteBuffers(NUM_STRM_BUFFERS, StrmBuffers);
    alDeleteSources(1, &StrmSource);
    StrmSource = 0;
  }
  unguard;
}

//==========================================================================
//
//  VOpenALDevice::GetStreamAvailable
//
//==========================================================================

int VOpenALDevice::GetStreamAvailable()
{
  guard(VOpenALDevice::GetStreamAvailable);
  if (!StrmSource)
    return 0;

  ALint NumProc;
  alGetSourcei(StrmSource, AL_BUFFERS_PROCESSED, &NumProc);
  if (NumProc > 0)
  {
    alSourceUnqueueBuffers(StrmSource, NumProc,
      StrmAvailableBuffers + StrmNumAvailableBuffers);
    StrmNumAvailableBuffers += NumProc;
  }
  return StrmNumAvailableBuffers > 0 ? STRM_BUFFER_SIZE : 0;
  unguard;
}

//==========================================================================
//
//  VOpenALDevice::GetStreamBuffer
//
//==========================================================================

short *VOpenALDevice::GetStreamBuffer()
{
  guard(VOpenALDevice::GetStreamBuffer);
  return StrmDataBuffer;
  unguard;
}

//==========================================================================
//
//  VOpenALDevice::SetStreamData
//
//==========================================================================

void VOpenALDevice::SetStreamData(short *Data, int Len)
{
  guard(VOpenALDevice::SetStreamData);
  ALuint Buf;
  ALint State;

  Buf = StrmAvailableBuffers[StrmNumAvailableBuffers - 1];
  StrmNumAvailableBuffers--;
  alBufferData(Buf, StrmFormat, Data, Len * 4, StrmSampleRate);
  alSourceQueueBuffers(StrmSource, 1, &Buf);
  alGetSourcei(StrmSource, AL_SOURCE_STATE, &State);
  if (State != AL_PLAYING)
  {
    if (StrmSource)
    {
      alSourcePlay(StrmSource);
    }
  }
  unguard;
}

//==========================================================================
//
//  VOpenALDevice::SetStreamVolume
//
//==========================================================================

void VOpenALDevice::SetStreamVolume(float Vol)
{
  guard(VOpenALDevice::SetStreamVolume);
  if (StrmSource)
  {
      alSourcef(StrmSource, AL_GAIN, Vol);
  }
  unguard;
}


//==========================================================================
//
//  VOpenALDevice::SetStreamPitch
//
//==========================================================================
void VOpenALDevice::SetStreamPitch (float pitch) {
  if (StrmSource) {
    alSourcef(StrmSource, AL_PITCH, pitch);
  }
}

//==========================================================================
//
//  VOpenALDevice::PauseStream
//
//==========================================================================

void VOpenALDevice::PauseStream()
{
  guard(VOpenALDevice::PauseStream);
  if (StrmSource)
  {
    alSourcePause(StrmSource);
  }
  unguard;
}

//==========================================================================
//
//  VOpenALDevice::ResumeStream
//
//==========================================================================

void VOpenALDevice::ResumeStream()
{
  guard(VOpenALDevice::ResumeStream);
  if (StrmSource)
  {
    alSourcePlay(StrmSource);
  }
  unguard;
}
