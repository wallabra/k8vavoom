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


VCvarF VOpenALDevice::doppler_factor("snd_doppler_factor", "1", "OpenAL doppler factor.", 0/*CVAR_Archive*/);
VCvarF VOpenALDevice::doppler_velocity("snd_doppler_velocity", "10000", "OpenAL doppler velocity.", 0/*CVAR_Archive*/);
VCvarF VOpenALDevice::rolloff_factor("snd_rolloff_factor", "1", "OpenAL rolloff factor.", 0/*CVAR_Archive*/);
//VCvarF VOpenALDevice::reference_distance("snd_reference_distance", "64", "OpenAL reference distance.", CVAR_Archive);
//VCvarF VOpenALDevice::max_distance("snd_max_distance", "2024", "OpenAL max distance.", CVAR_Archive);
//VCvarF VOpenALDevice::reference_distance("snd_reference_distance", "384", "OpenAL reference distance.", 0/*CVAR_Archive*/);
VCvarF VOpenALDevice::reference_distance("snd_reference_distance", "192", "OpenAL reference distance.", 0/*CVAR_Archive*/);
//VCvarF VOpenALDevice::max_distance("snd_max_distance", "4096", "OpenAL max distance.", 0/*CVAR_Archive*/);
VCvarF VOpenALDevice::max_distance("snd_max_distance", "8192", "OpenAL max distance.", 0/*CVAR_Archive*/);

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
  return clampval(InNumChannels, 1, (int)MAX_VOICES);
}


//==========================================================================
//
//  VOpenALDevice::Shutdown
//
//==========================================================================
void VOpenALDevice::Shutdown () {
  if (developer) GLog.Log(NAME_Dev, "VOpenALDevice::Shutdown(): shutting down OpenAL...");

  srcPendingSet.clear();
  srcErrorSet.clear();

  if (activeSourceSet.length()) {
    if (developer) GLog.Logf(NAME_Dev, "VOpenALDevice::Shutdown(): aborting %d active sources...", activeSourceSet.length());
    while (activeSourceSet.length()) {
      auto it = activeSourceSet.first();
      ALuint src = it.getKey();
      if (developer) GLog.Logf(NAME_Dev, "VOpenALDevice::Shutdown():   aborting source %u", src);
      activeSourceSet.del(src);
      alDeleteSources(1, &src);
    }
  }

  // delete buffers
  if (Buffers) {
    if (developer) GLog.Log(NAME_Dev, "VOpenALDevice::Shutdown(): deleting sound buffers...");
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

  // destroy context
  if (Context) {
    if (developer) GLog.Log(NAME_Dev, "VOpenALDevice::Shutdown(): destroying context...");
    alcSetThreadContext(nullptr);
    //alcSetThreadContext(Context);
    alcDestroyContext(Context);
    //alcSetThreadContext(nullptr);
    Context = nullptr;
  }

  // disconnect from a device
  if (Device) {
    if (developer) GLog.Log(NAME_Dev, "VOpenALDevice::Shutdown(): closing device...");
    alcCloseDevice(Device);
    Device = nullptr;
    Context = nullptr;
  }
  if (developer) GLog.Log(NAME_Dev, "VOpenALDevice::Shutdown(): shutdown complete!");
}


//==========================================================================
//
//  VOpenALDevice::AllocSource
//
//==========================================================================
bool VOpenALDevice::AllocSource (ALuint *src) {
  if (!src) return false;
  alGetError(); // clear error code
  alGenSources(1, src);
  if (alGetError() != AL_NO_ERROR) {
    GCon->Log(NAME_Dev, "Failed to gen OpenAL source");
    return false;
  }
  activeSourceSet.put(*src, true);
  return true;
}


//==========================================================================
//
//  VOpenALDevice::LoadSound
//
//  returns VSoundManager::LS_XXX
//  if not errored, sets `src` to new sound source
//
//==========================================================================
int VOpenALDevice::LoadSound (int sound_id, ALuint *src) {
  if (sound_id < 0 || sound_id >= GSoundManager->S_sfx.length()) return VSoundManager::LS_Error;

  if (BufferCount < GSoundManager->S_sfx.length()) {
    int newsz = ((GSoundManager->S_sfx.length()+1)|0xff)+1;
    ALuint *newbuf = new ALuint[newsz];
    if (BufferCount > 0) {
      for (int f = 0; f < BufferCount; ++f) newbuf[f] = Buffers[f];
    }
    for (int f = BufferCount; f < newsz; ++f) newbuf[f] = 0;
    delete[] Buffers;
    Buffers = newbuf;
    BufferCount = newsz;
  }

  /*
  if (BufferCount < sound_id+1) {
    int newsz = ((sound_id+4)|0xfff)+1;
    ALuint *newbuf = new ALuint[newsz];
    for (int f = BufferCount; f < newsz; ++f) newbuf[f] = 0;
    delete[] Buffers;
    Buffers = newbuf;
    BufferCount = newsz;
  }
  */

  if (Buffers[sound_id]) {
    if (AllocSource(src)) return VSoundManager::LS_Ready;
    return VSoundManager::LS_Error;
  }

  // check that sound lump is queued
  auto pss = sourcesPending.find(sound_id);
  if (pss) {
    // pending sound, generate new source, and add it to pending list
    if (!AllocSource(src)) return VSoundManager::LS_Error;
    PendingSrc *psrc = new PendingSrc;
    psrc->src = *src;
    psrc->sound_id = sound_id;
    psrc->next = *pss;
    sourcesPending.put(sound_id, psrc);
    srcPendingSet.put(*src, sound_id);
    return VSoundManager::LS_Pending;
  }

  // check that sound lump is loaded
  int res = GSoundManager->LoadSound(sound_id);
  if (res == VSoundManager::LS_Error) return false; // missing sound

  // generate new source
  if (!AllocSource(src)) {
    GSoundManager->DoneWithLump(sound_id);
    return VSoundManager::LS_Error;
  }

  if (res == VSoundManager::LS_Pending) {
    // pending sound, generate new source, and add it to pending list
    check(!sourcesPending.find(sound_id));
    check(!srcPendingSet.find(*src));
    PendingSrc *psrc = new PendingSrc;
    psrc->src = *src;
    psrc->sound_id = sound_id;
    psrc->next = nullptr;
    sourcesPending.put(sound_id, psrc);
    srcPendingSet.put(*src, sound_id);
    return VSoundManager::LS_Pending;
  }

  // clear error code
  alGetError();

  // create buffer
  alGenBuffers(1, &Buffers[sound_id]);
  if (alGetError() != AL_NO_ERROR) {
    GCon->Log(NAME_Dev, "Failed to gen OpenAL buffer");
    GSoundManager->DoneWithLump(sound_id);
    activeSourceSet.del(*src);
    alDeleteSources(1, src);
    return VSoundManager::LS_Error;
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
    activeSourceSet.del(*src);
    alDeleteSources(1, src);
    return VSoundManager::LS_Error;
  }

  // we don't need to keep lump static
  GSoundManager->DoneWithLump(sound_id);
  return VSoundManager::LS_Ready;
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
  ALuint src;

  int res = LoadSound(sound_id, &src);
  if (res == VSoundManager::LS_Error) return -1;

  alSourcef(src, AL_GAIN, volume);
  alSourcef(src, AL_ROLLOFF_FACTOR, rolloff_factor);
  alSourcei(src, AL_SOURCE_RELATIVE, AL_TRUE);
  alSource3f(src, AL_POSITION, 0.0f, 0.0f, -16.0f);
  alSourcef(src, AL_REFERENCE_DISTANCE, reference_distance);
  alSourcef(src, AL_MAX_DISTANCE, max_distance);
  alSourcef(src, AL_PITCH, pitch);
  if (Loop) alSourcei(src, AL_LOOPING, AL_TRUE);
  if (res == VSoundManager::LS_Ready) {
    alSourcei(src, AL_BUFFER, Buffers[sound_id]);
    alSourcePlay(src);
  }
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
  ALuint src;

  int res = LoadSound(sound_id, &src);
  if (res == VSoundManager::LS_Error) return -1;

  alSourcef(src, AL_GAIN, volume);
  alSourcef(src, AL_ROLLOFF_FACTOR, rolloff_factor);
  alSourcei(src, AL_SOURCE_RELATIVE, AL_FALSE); // just in case
  alSource3f(src, AL_POSITION, origin.x, origin.y, origin.z);
  alSource3f(src, AL_VELOCITY, velocity.x, velocity.y, velocity.z);
  alSourcef(src, AL_REFERENCE_DISTANCE, reference_distance);
  alSourcef(src, AL_MAX_DISTANCE, max_distance);
  alSourcef(src, AL_PITCH, pitch);
  if (Loop) alSourcei(src, AL_LOOPING, AL_TRUE);
  if (res == VSoundManager::LS_Ready) {
    alSourcei(src, AL_BUFFER, Buffers[sound_id]);
    alSourcePlay(src);
  }
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
  // pending sounds are "playing"
  if (srcPendingSet.has((ALuint)Handle)) return true;
  if (srcErrorSet.has((ALuint)Handle)) return false;
  ALint State;
  alGetSourcei((ALuint)Handle, AL_SOURCE_STATE, &State);
  return (State == AL_PLAYING);
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
  ALuint hh = (ALuint)Handle;
  // remove pending sounds
  auto sidp = srcPendingSet.find(hh);
  if (sidp) {
    // remove from pending list
    srcPendingSet.del(hh);
    PendingSrc **pss = sourcesPending.find(*sidp);
    if (pss) {
      PendingSrc *prev = nullptr, *cur = *pss;
      while (cur && cur->src != hh) {
        prev = cur;
        cur = cur->next;
      }
      if (cur) {
        // i found her!
        if (prev) {
          // not first
          prev->next = cur->next;
        } else if (cur->next) {
          // first, not last
          sourcesPending.put(*sidp, cur->next);
        } else {
          // only one
          sourcesPending.del(*sidp);
        }
        // signal manager that we don't want it anymore
        GSoundManager->DoneWithLump(cur->sound_id);
        delete cur;
      }
    }
  } else {
    // stop buffer
    alSourceStop(hh);
  }
  srcErrorSet.del(hh);
  activeSourceSet.del(hh);
  alDeleteSources(1, &hh);
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
//  VOpenALDevice::NotifySoundLoaded
//
// WARNING! this must be called from the main thread, i.e.
//          from the thread that calls `PlaySound*()` API!
//
//==========================================================================
void VOpenALDevice::NotifySoundLoaded (int sound_id, bool success) {
  PendingSrc **pss = sourcesPending.find(sound_id);
  if (!pss) return; // nothing to do
  PendingSrc *cur = *pss;
  while (cur) {
    PendingSrc *next = cur->next;
    check(cur->sound_id == sound_id);
    srcPendingSet.del(cur->src);
    if (success) {
      // play it
      //GCon->Logf("OpenAL: playing source #%u (sound #%d)", cur->src, sound_id);
      if (!Buffers[sound_id]) {
        // clear error code
        alGetError();
        // create buffer
        alGenBuffers(1, &Buffers[sound_id]);
        if (alGetError() != AL_NO_ERROR) {
          GCon->Log(NAME_Dev, "Failed to gen OpenAL buffer");
          srcErrorSet.put(cur->src, true);
          Buffers[sound_id] = 0;
        } else {
          // load buffer data
          alBufferData(Buffers[sound_id],
            (GSoundManager->S_sfx[sound_id].SampleBits == 8 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16),
            GSoundManager->S_sfx[sound_id].Data,
            GSoundManager->S_sfx[sound_id].DataSize,
            GSoundManager->S_sfx[sound_id].SampleRate);

          if (alGetError() != AL_NO_ERROR) {
            GCon->Log(NAME_Dev, "Failed to load buffer data");
            srcErrorSet.put(cur->src, true);
            Buffers[sound_id] = 0;
          } else {
            //GCon->Logf("OpenAL: playing source #%u (sound #%d; sr=%d; bits=%d; size=%d)", cur->src, sound_id, GSoundManager->S_sfx[sound_id].SampleRate, GSoundManager->S_sfx[sound_id].SampleBits, GSoundManager->S_sfx[sound_id].DataSize);
            alSourcei(cur->src, AL_BUFFER, Buffers[sound_id]);
            alSourcePlay(cur->src);
          }
        }
      } else {
        // already buffered, just play it
        //GCon->Logf("OpenAL: playing already buffered source #%u (sound #%d; sr=%d; bits=%d; size=%d)", cur->src, sound_id, GSoundManager->S_sfx[sound_id].SampleRate, GSoundManager->S_sfx[sound_id].SampleBits, GSoundManager->S_sfx[sound_id].DataSize);
        alSourcei(cur->src, AL_BUFFER, Buffers[sound_id]);
        alSourcePlay(cur->src);
      }
    } else {
      // mark as invalid
      srcErrorSet.put(cur->src, true);
    }
    GSoundManager->DoneWithLump(sound_id);
    delete cur;
    cur = next;
  }
  sourcesPending.del(sound_id);
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
  activeSourceSet.put(StrmSource, true);
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
    activeSourceSet.del(StrmSource);
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
