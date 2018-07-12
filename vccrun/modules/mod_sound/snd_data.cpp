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


/*
class VRawSampleLoader : public VSampleLoader {
public:
  virtual void Load (sfxinfo_t &, VStream &) override;
};
*/


VSampleLoader *VSampleLoader::List;
VSoundManager *GSoundManager;
static bool sminited = false;

//static VRawSampleLoader RawSampleLoader;
//static TStrSet soundsWarned;

void VSoundManager::StaticInitialize () {
  if (!sminited) {
    if (!GSoundManager) {
      sminited = true;
      GSoundManager = new VSoundManager;
      GSoundManager->Init();
      GAudio = VAudioPublic::Create();
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
//  VSoundManager::VSoundManager
//
//==========================================================================
VSoundManager::VSoundManager ()
  : NumPlayerReserves(0)
  , CurrentChangePitch(0) //7.0/255.0
  , name2idx()
{
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
    //delete[] S_sfx[i].Sounds;
    //S_sfx[i].Sounds = nullptr;
  }
}


//==========================================================================
//
//  VSoundManager::Init
//
//  Loads sound script lump or file, if param -devsnd was specified
//
//==========================================================================
void VSoundManager::Init () {
  // zero slot is reserved
  sfxinfo_t S;
  memset(&S, 0, sizeof(S));
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
  //fprintf(stderr, "id=%d; tag=%s; file=%s\n", id, *TagName, *filename);
  if (!id) {
    sfxinfo_t S;
    memset(&S, 0, sizeof(S));
    S.tagName = TagName;
    S.data = nullptr;
    S.priority = 127;
    S.numChannels = 2;
    S.changePitch = CurrentChangePitch;
    int idx = -1;
    for (int f = S_sfx.length()-1; f > 0; --f) if (S_sfx[f].tagName == NAME_None) { idx = f; break; }
    if (idx < 0) {
      idx = S_sfx.length();
      S_sfx.Append(S);
    } else {
      S_sfx[idx] = S;
    }
    name2idx.put(TagName, idx);
    //fprintf(stderr, "  idx=%d; slen=%d\n", idx, S_sfx.length());
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
//  VSoundManager::LoadSound
//
//==========================================================================
bool VSoundManager::LoadSound (int sound_id, const VStr &filename) {
  //static const char *exts[] = { "flac", "ogg", "wav", nullptr };
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
        //GCon->Logf("sound '%s' is %s", *W_FullLumpName(Lump), typeid(*Ldr).name());
        //fprintf(stderr, "Loaded sound '%s' from file '%s'\n", *S_sfx[sound_id].tagName, *filename);
        break;
      }
    }
    delete Strm;
    if (!S_sfx[sound_id].data) {
      fprintf(stderr, "WARNING: Failed to load sound '%s' from file '%s'\n", *S_sfx[sound_id].tagName, *filename);
      /*
      if (!soundsWarned.put(*S_sfx[sound_id].TagName)) {
        GCon->Logf(NAME_Dev, "Failed to load sound %s", *S_sfx[sound_id].TagName);
      }
      */
      return false;
    }
  }
  return true;
}


//==========================================================================
//
//  VRawSampleLoader::Load
//
//==========================================================================
/*
void VRawSampleLoader::Load(sfxinfo_t &Sfx, VStream &Strm) {
  //  Read header and see if it's a valid raw sample.
  vuint16   Unknown;
  vuint16   SampleRate;
  vuint32   DataSize;

  Strm.Seek(0);
  Strm << Unknown
    << SampleRate
    << DataSize;
  if (Unknown != 3 || (vint32)DataSize != Strm.TotalSize() - 8)
  {
    return;
  }

  Sfx.SampleBits = 8;
  Sfx.SampleRate = SampleRate;
  Sfx.DataSize = DataSize;
  Sfx.Data = Z_Malloc(Sfx.DataSize);
  Strm.Serialise(Sfx.Data, Sfx.DataSize);
}
*/


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_CLASS(V, SoundSystem);


// ////////////////////////////////////////////////////////////////////////// //
// returns `none` on error
// static native final static void Initialize ();
IMPLEMENT_FUNCTION(VSoundSystem, Initialize) {
  VSoundManager::StaticInitialize();
}


// static native final int AddSound (name tagName, string filename); // won't replace
IMPLEMENT_FUNCTION(VSoundSystem, AddSound) {
  VSoundManager::StaticInitialize();
  P_GET_STR(filename);
  P_GET_NAME(tagName);
  int res = 0;
  if (GSoundManager) res = GSoundManager->AddSound(tagName, filename);
  RET_INT(res);
}


// static native final int FindSound (name tagName);
IMPLEMENT_FUNCTION(VSoundSystem, FindSound) {
  VSoundManager::StaticInitialize();
  P_GET_NAME(tagName);
  int res = 0;
  if (GSoundManager) res = GSoundManager->FindSound(tagName);
  RET_INT(res);
}


// static native final int PlaySound (int InSoundId, const TVec origin, const TVec velocity, int origin_id, int channel, float volume, float Attenuation, bool Loop);
IMPLEMENT_FUNCTION(VSoundSystem, PlaySound) {
  VSoundManager::StaticInitialize();
  P_GET_BOOL(loop);
  P_GET_FLOAT(attenuation);
  P_GET_FLOAT(volume);
  P_GET_INT(channel);
  P_GET_INT(origin_id);
  P_GET_VEC(velocity);
  P_GET_VEC(origin);
  P_GET_INT(sndid);
  if (GAudio) GAudio->PlaySound(sndid, origin, velocity, origin_id, channel, volume, attenuation, loop);
}


// static native final void StopSound (int origin_id, int channel);
IMPLEMENT_FUNCTION(VSoundSystem, StopSound) {
  VSoundManager::StaticInitialize();
  P_GET_INT(channel);
  P_GET_INT(origin_id);
  if (GAudio) GAudio->StopSound(origin_id, channel);
}


// static native final void StopAllSound ();
IMPLEMENT_FUNCTION(VSoundSystem, StopAllSound) {
  VSoundManager::StaticInitialize();
  if (GAudio) GAudio->StopAllSound();
}


// static native final bool IsSoundPlaying (int origin_id, int InSoundId);
IMPLEMENT_FUNCTION(VSoundSystem, IsSoundPlaying) {
  VSoundManager::StaticInitialize();
  P_GET_INT(sound_id);
  P_GET_INT(origin_id);
  bool res = false;
  if (GAudio) res = GAudio->IsSoundPlaying(origin_id, sound_id);
  RET_BOOL(res);
}


// static native final void SetSoundPitch (int origin_id, int InSoundId, float pitch);
IMPLEMENT_FUNCTION(VSoundSystem, SetSoundPitch) {
  VSoundManager::StaticInitialize();
  P_GET_FLOAT(pitch);
  P_GET_INT(sound_id);
  P_GET_INT(origin_id);
  if (GAudio) GAudio->SetSoundPitch(origin_id, sound_id, pitch);
}


// static native final void UpdateSounds ();
IMPLEMENT_FUNCTION(VSoundSystem, UpdateSounds) {
  VSoundManager::StaticInitialize();
  if (GAudio) GAudio->UpdateSounds();
}

// static native final void SetListenerOrigin ();
IMPLEMENT_FUNCTION(VSoundSystem, set_ListenerOrigin) {
  VSoundManager::StaticInitialize();
  P_GET_VEC(orig);
  if (GAudio) GAudio->SetListenerOrigin(orig);
}


IMPLEMENT_FUNCTION(VSoundSystem, get_SoundVolume) {
  RET_FLOAT(VAudioPublic::snd_sfx_volume);
}

IMPLEMENT_FUNCTION(VSoundSystem, set_SoundVolume) {
  P_GET_FLOAT(v);
  if (v < 0) v = 0; else if (v > 1) v = 1;
  VAudioPublic::snd_sfx_volume = v;
}

IMPLEMENT_FUNCTION(VSoundSystem, get_MusicVolume) {
  RET_FLOAT(VAudioPublic::snd_music_volume);
}

IMPLEMENT_FUNCTION(VSoundSystem, set_MusicVolume) {
  P_GET_FLOAT(v);
  if (v < 0) v = 0; else if (v > 1) v = 1;
  VAudioPublic::snd_music_volume = v;
}

IMPLEMENT_FUNCTION(VSoundSystem, get_SwapStereo) {
  RET_BOOL(VAudioPublic::snd_swap_stereo);
}

IMPLEMENT_FUNCTION(VSoundSystem, set_SwapStereo) {
  P_GET_BOOL(v);
  VAudioPublic::snd_swap_stereo = v;
}


// static native final bool PlayMusic (string filename, optional bool Loop);
IMPLEMENT_FUNCTION(VSoundSystem, PlayMusic) {
  P_GET_BOOL_OPT(loop, false);
  P_GET_STR(filename);
  VSoundManager::StaticInitialize();
  if (GAudio) {
    RET_BOOL(GAudio->PlayMusic(filename, loop));
  } else {
    RET_BOOL(false);
  }
}

// static native final bool IsMusicPlaying ();
IMPLEMENT_FUNCTION(VSoundSystem, IsMusicPlaying) {
  VSoundManager::StaticInitialize();
  RET_BOOL(GAudio ? GAudio->IsMusicPlaying() : false);
}

// static native final void PauseMusic ();
IMPLEMENT_FUNCTION(VSoundSystem, PauseMusic) {
  VSoundManager::StaticInitialize();
  if (GAudio) GAudio->PauseMusic();
}

// static native final void ResumeMusic ();
IMPLEMENT_FUNCTION(VSoundSystem, ResumeMusic) {
  VSoundManager::StaticInitialize();
  if (GAudio) GAudio->ResumeMusic();
}

// static native final void StopMusic ();
IMPLEMENT_FUNCTION(VSoundSystem, StopMusic) {
  VSoundManager::StaticInitialize();
  if (GAudio) GAudio->StopMusic();
}


// static native final void SetMusicPitch (float pitch);
IMPLEMENT_FUNCTION(VSoundSystem, SetMusicPitch) {
  VSoundManager::StaticInitialize();
  P_GET_FLOAT(pitch);
  if (GAudio) GAudio->SetMusicPitch(pitch);
}
