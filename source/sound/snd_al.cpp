//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**  You should have received a copy of the GNU General Public License
//**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//**
//**************************************************************************
#include "gamedefs.h"
#include "snd_local.h"


VCvarF VOpenALDevice::doppler_factor("snd_doppler_factor", "1.0", "OpenAL doppler factor.", 0/*CVAR_Archive*/);
VCvarF VOpenALDevice::doppler_velocity("snd_doppler_velocity", "10000.0", "OpenAL doppler velocity.", 0/*CVAR_Archive*/);
VCvarF VOpenALDevice::rolloff_factor("snd_rolloff_factor", "1.0", "OpenAL rolloff factor.", 0/*CVAR_Archive*/);
//VCvarF VOpenALDevice::reference_distance("snd_reference_distance", "64.0", "OpenAL reference distance.", CVAR_Archive);
//VCvarF VOpenALDevice::max_distance("snd_max_distance", "2024.0", "OpenAL max distance.", CVAR_Archive);
//VCvarF VOpenALDevice::reference_distance("snd_reference_distance", "384.0", "OpenAL reference distance.", 0/*CVAR_Archive*/);
VCvarF VOpenALDevice::reference_distance("snd_reference_distance", "192.0", "OpenAL reference distance.", 0/*CVAR_Archive*/);
//VCvarF VOpenALDevice::max_distance("snd_max_distance", "4096.0", "OpenAL max distance.", 0/*CVAR_Archive*/);
VCvarF VOpenALDevice::max_distance("snd_max_distance", "8192.0", "OpenAL max distance.", 0/*CVAR_Archive*/);

static VCvarB openal_show_extensions("openal_show_extensions", false, "Show available OpenAL extensions?", CVAR_Archive);


//==========================================================================
//
//  VOpenALDevice::VOpenALDevice
//
//==========================================================================
VOpenALDevice::VOpenALDevice ()
  : Device(nullptr)
  , Context(nullptr)
  , Buffers(nullptr)
  , BufferCount(0)
  , StrmSampleRate(0)
  , StrmFormat(0)
  , StrmNumAvailableBuffers(0)
  , StrmSource(0)
{
  memset(StrmBuffers, 0, sizeof(StrmBuffers));
  memset(StrmAvailableBuffers, 0, sizeof(StrmAvailableBuffers));
  memset(StrmDataBuffer, 0, sizeof(StrmDataBuffer));
}


//==========================================================================
//
//  VOpenALDevice::~VOpenALDevice
//
//==========================================================================
VOpenALDevice::~VOpenALDevice () {
  Shutdown(); // just in case
}


//==========================================================================
//
//  VOpenALDevice::Init
//
//  Inits sound
//
//==========================================================================
bool VOpenALDevice::Init () {
  ALenum E;

  Device = nullptr;
  Context = nullptr;
  Buffers = nullptr;
  BufferCount = 0;
  StrmSource = 0;
  StrmNumAvailableBuffers = 0;

  //  Connect to a device.
  Device = alcOpenDevice(nullptr);
  if (!Device) {
    GCon->Log(NAME_Init, "Couldn't open OpenAL device");
    return false;
  }

  if (!alcIsExtensionPresent(Device, "ALC_EXT_thread_local_context")) {
    Sys_Error("OpenAL: 'ALC_EXT_thread_local_context' extension is not present.\n"
              "Please, use OpenAL Soft implementation, and make sure that it is recent.");
  }

  // create a context and make it current
  static const ALCint attrs[] = {
    ALC_STEREO_SOURCES, 1, // get at least one stereo source for music
    ALC_MONO_SOURCES, MAX_VOICES, // this should be audio channels in our game engine
    //ALC_FREQUENCY, 48000, // desired frequency; we don't really need this, let OpenAL choose the best
    0,
  };
  Context = alcCreateContext(Device, attrs);
  if (!Context) Sys_Error("Failed to create OpenAL context");
  alcSetThreadContext(Context);
  E = alGetError();
  if (E != AL_NO_ERROR) Sys_Error("OpenAL error: %s", alGetString(E));

  alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
  alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
  alEnable(AL_SOURCE_DISTANCE_MODEL);

  // clear error code
  alGetError();

  // print some information
  if (openal_show_extensions) {
    GCon->Logf(NAME_Init, "AL_VENDOR: %s", alGetString(AL_VENDOR));
    GCon->Logf(NAME_Init, "AL_RENDERER: %s", alGetString(AL_RENDERER));
    GCon->Logf(NAME_Init, "AL_VERSION: %s", alGetString(AL_VERSION));
    GCon->Log(NAME_Init, "AL_EXTENSIONS:");
    TArray<VStr> Exts;
    VStr((char*)alGetString(AL_EXTENSIONS)).Split(' ', Exts);
    for (int i = 0; i < Exts.Num(); i++) GCon->Log(NAME_Init, VStr("- ")+Exts[i]);
    GCon->Log(NAME_Init, "ALC_EXTENSIONS:");
    VStr((char*)alcGetString(Device, ALC_EXTENSIONS)).Split(' ', Exts);
    for (int i = 0; i < Exts.Num(); i++) GCon->Log(NAME_Init, VStr("- ")+Exts[i]);
  }

  // allocate array for buffers
  /*
  Buffers = new ALuint[GSoundManager->S_sfx.Num()];
  memset(Buffers, 0, sizeof(ALuint) * GSoundManager->S_sfx.Num());
  */

  GCon->Log(NAME_Init, "OpenAL initialized.");
  return true;
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
  int NumChannels = MAX_VOICES;
  if (NumChannels > InNumChannels) NumChannels = InNumChannels;
  return NumChannels;
}


//==========================================================================
//
//  VOpenALDevice::Shutdown
//
//==========================================================================
void VOpenALDevice::Shutdown () {
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
  if (Device) {
    alcCloseDevice(Device);
    Device = nullptr;
  }
}


//==========================================================================
//
//  VOpenALDevice::LoadSound
//
//==========================================================================
bool VOpenALDevice::LoadSound (int sound_id) {
  if (BufferCount < sound_id+1) {
    int newsz = ((sound_id+4)|0xfff)+1;
    ALuint *newbuf = new ALuint[newsz];
    for (int f = BufferCount; f < newsz; ++f) newbuf[f] = 0;
    delete[] Buffers;
    Buffers = newbuf;
    BufferCount = newsz;
  }

  if (Buffers[sound_id]) return true;

  // check, that sound lump is loaded
  if (!GSoundManager->LoadSound(sound_id)) return false; // missing sound

  // clear error code
  alGetError();

  // create buffer
  alGenBuffers(1, &Buffers[sound_id]);
  if (alGetError() != AL_NO_ERROR) {
    GCon->Log(NAME_Dev, "Failed to gen buffer");
    GSoundManager->DoneWithLump(sound_id);
    return false;
  }

  // load buffer data
  alBufferData(Buffers[sound_id],
    (GSoundManager->S_sfx[sound_id].SampleBits == 8 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16),
    GSoundManager->S_sfx[sound_id].Data,
    GSoundManager->S_sfx[sound_id].DataSize,
    GSoundManager->S_sfx[sound_id].SampleRate);
  if (alGetError() != AL_NO_ERROR) {
    GCon->Log(NAME_Dev, "Failed to load buffer data");
    GSoundManager->DoneWithLump(sound_id);
    return false;
  }

  // we don't need to keep lump static
  GSoundManager->DoneWithLump(sound_id);
  return true;
}


//==========================================================================
//
//  VOpenALDevice::PlaySound
//
//  This function adds a sound to the list of currently active sounds, which
//  is maintained as a given number of internal channels.
//
//==========================================================================
int VOpenALDevice::PlaySound (int sound_id, float volume, float pitch, bool Loop) {
  if (!LoadSound(sound_id)) return -1;

  ALuint src;
  alGetError(); // clear error code
  alGenSources(1, &src);
  if (alGetError() != AL_NO_ERROR) {
    GCon->Log(NAME_Dev, "Failed to gen source");
    return -1;
  }

  alSourcei(src, AL_BUFFER, Buffers[sound_id]);

  alSourcef(src, AL_GAIN, volume);
  alSourcef(src, AL_ROLLOFF_FACTOR, rolloff_factor);
  alSourcei(src, AL_SOURCE_RELATIVE, AL_TRUE);
  alSource3f(src, AL_POSITION, 0.0f, 0.0f, -16.0f);
  alSourcef(src, AL_REFERENCE_DISTANCE, reference_distance);
  alSourcef(src, AL_MAX_DISTANCE, max_distance);
  alSourcef(src, AL_PITCH, pitch);
  if (Loop) alSourcei(src, AL_LOOPING, AL_TRUE);
  alSourcePlay(src);
  return src;
}


//==========================================================================
//
//  VOpenALDevice::PlaySound3D
//
//==========================================================================
int VOpenALDevice::PlaySound3D (int sound_id, const TVec &origin, const TVec &velocity,
                                float volume, float pitch, bool Loop)
{
  if (!LoadSound(sound_id)) return -1;

  ALuint src;
  alGetError(); // clear error code
  alGenSources(1, &src);
  if (alGetError() != AL_NO_ERROR) {
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
}


//==========================================================================
//
//  VOpenALDevice::UpdateChannel3D
//
//==========================================================================
void VOpenALDevice::UpdateChannel3D (int Handle, const TVec &Org, const TVec &Vel) {
  if (Handle == -1) return;
  alSource3f(Handle, AL_POSITION, Org.x, Org.y, Org.z);
  alSource3f(Handle, AL_VELOCITY, Vel.x, Vel.y, Vel.z);
}


//==========================================================================
//
//  VOpenALDevice::IsChannelPlaying
//
//==========================================================================
bool VOpenALDevice::IsChannelPlaying (int Handle) {
  if (Handle == -1) return false;
  ALint State;
  alGetSourcei(Handle, AL_SOURCE_STATE, &State);
  return State == AL_PLAYING;
}


//==========================================================================
//
//  VOpenALDevice::StopChannel
//
//  Stop the sound. Necessary to prevent runaway chainsaw, and to stop
//  rocket launches when an explosion occurs.
//  All sounds MUST be stopped;
//
//==========================================================================
void VOpenALDevice::StopChannel (int Handle) {
  if (Handle == -1) return;
  // stop buffer
  alSourceStop(Handle);
  alDeleteSources(1, (ALuint*)&Handle);
}


//==========================================================================
//
//  VOpenALDevice::UpdateListener
//
//==========================================================================
void VOpenALDevice::UpdateListener (const TVec &org, const TVec &vel,
                                    const TVec &fwd, const TVec&, const TVec &up
#if defined(VAVOOM_REVERB)
                                    , VReverbInfo *Env
#endif
)
{
  alListener3f(AL_POSITION, org.x, org.y, org.z);
  alListener3f(AL_VELOCITY, vel.x, vel.y, vel.z);

  ALfloat orient[6] = { fwd.x, fwd.y, fwd.z, up.x, up.y, up.z};
  alListenerfv(AL_ORIENTATION, orient);

  alDopplerFactor(doppler_factor);
  alDopplerVelocity(doppler_velocity);
}


//==========================================================================
//
//  VOpenALDevice::OpenStream
//
//==========================================================================
bool VOpenALDevice::OpenStream (int Rate, int Bits, int Channels) {
  StrmSampleRate = Rate;
  StrmFormat = Channels == 2 ?
    Bits == 8 ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16 :
    Bits == 8 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16;

  alGetError(); // clear error code
  alGenSources(1, &StrmSource);
  if (alGetError() != AL_NO_ERROR) {
    GCon->Log(NAME_Dev, "Failed to gen source");
    return false;
  }
  alSourcei(StrmSource, AL_SOURCE_RELATIVE, AL_TRUE);
  alGenBuffers(NUM_STRM_BUFFERS, StrmBuffers);
  alSourceQueueBuffers(StrmSource, NUM_STRM_BUFFERS, StrmBuffers);
  alSourcePlay(StrmSource);
  StrmNumAvailableBuffers = 0;
  return true;
}


//==========================================================================
//
//  VOpenALDevice::CloseStream
//
//==========================================================================
void VOpenALDevice::CloseStream () {
  if (StrmSource) {
    alDeleteBuffers(NUM_STRM_BUFFERS, StrmBuffers);
    alDeleteSources(1, &StrmSource);
    StrmSource = 0;
  }
}


//==========================================================================
//
//  VOpenALDevice::GetStreamAvailable
//
//==========================================================================
int VOpenALDevice::GetStreamAvailable () {
  if (!StrmSource) return 0;

  ALint NumProc;
  alGetSourcei(StrmSource, AL_BUFFERS_PROCESSED, &NumProc);
  if (NumProc > 0) {
    alSourceUnqueueBuffers(StrmSource, NumProc, StrmAvailableBuffers+StrmNumAvailableBuffers);
    StrmNumAvailableBuffers += NumProc;
  }
  return (StrmNumAvailableBuffers > 0 ? STRM_BUFFER_SIZE : 0);
}


//==========================================================================
//
//  VOpenALDevice::GetStreamBuffer
//
//==========================================================================
short *VOpenALDevice::GetStreamBuffer () {
  return StrmDataBuffer;
}


//==========================================================================
//
//  VOpenALDevice::SetStreamData
//
//==========================================================================
void VOpenALDevice::SetStreamData (short *Data, int Len) {
  ALint State;

  ALuint Buf = StrmAvailableBuffers[StrmNumAvailableBuffers-1];
  --StrmNumAvailableBuffers;
  alBufferData(Buf, StrmFormat, Data, Len*4, StrmSampleRate);
  alSourceQueueBuffers(StrmSource, 1, &Buf);
  alGetSourcei(StrmSource, AL_SOURCE_STATE, &State);
  if (State != AL_PLAYING) {
    if (StrmSource) alSourcePlay(StrmSource);
  }
}


//==========================================================================
//
//  VOpenALDevice::SetStreamVolume
//
//==========================================================================
void VOpenALDevice::SetStreamVolume (float Vol) {
  if (StrmSource) alSourcef(StrmSource, AL_GAIN, Vol);
}


//==========================================================================
//
//  VOpenALDevice::SetStreamPitch
//
//==========================================================================
void VOpenALDevice::SetStreamPitch (float pitch) {
  if (StrmSource) alSourcef(StrmSource, AL_PITCH, pitch);
}


//==========================================================================
//
//  VOpenALDevice::PauseStream
//
//==========================================================================
void VOpenALDevice::PauseStream () {
  if (StrmSource) alSourcePause(StrmSource);
}


//==========================================================================
//
//  VOpenALDevice::ResumeStream
//
//==========================================================================
void VOpenALDevice::ResumeStream () {
  if (StrmSource) alSourcePlay(StrmSource);
}
