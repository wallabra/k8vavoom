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
//  VSampleLoader::LoadFromAudioCodec
//
//  codec must be initialized, and it will not be owned
//
//==========================================================================
void VSampleLoader::LoadFromAudioCodec (sfxinfo_t &Sfx, VAudioCodec *Codec) {
  if (!Codec) return;
  if (Codec->NumChannels != 1 && Codec->NumChannels != 2) return;
  //fprintf(stderr, "loading from audio codec; chans=%d; rate=%d; bits=%d\n", Codec->NumChannels, Codec->SampleRate, Codec->SampleBits);

  TArray<short> Data;
  do {
    short Buf[16*2048];
    int SamplesDecoded = Codec->Decode(Buf, 16*1024);
    if (SamplesDecoded > 0) {
      int OldPos = Data.length();
      Data.SetNumWithReserve(Data.length()+SamplesDecoded);
      if (Codec->NumChannels == 2) {
        // stereo
        for (int i = 0; i < SamplesDecoded; ++i) {
          // mix it
          int v = Buf[i*2]+Buf[i*2+1];
          if (v < -32768) v = -32768; else if (v > 32767) v = 32767;
          Data[OldPos+i] = v;
        }
      } else {
        // mono
        for (int i = 0; i < SamplesDecoded; ++i) Data[OldPos+i] = Buf[i];
      }
    }
  } while (!Codec->Finished());

  if (!Data.length()) return;

  // copy parameters
  Sfx.sampleRate = Codec->SampleRate;
  Sfx.sampleBits = Codec->SampleBits;

  // copy data
  Sfx.dataSize = Data.length()*2;
  Sfx.data = (vuint8 *)Z_Malloc(Data.length()*2);
  memcpy(Sfx.data, Data.Ptr(), Data.length()*2);
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
  if (!id) {
    sfxinfo_t S;
    memset(&S, 0, sizeof(S));
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


#include "snd_vcapi.cpp"
