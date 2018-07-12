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


//==========================================================================
//
//  VStreamMusicPlayer::Init
//
//==========================================================================
void VStreamMusicPlayer::Init () {
}


//==========================================================================
//
//  VStreamMusicPlayer::Shutdown
//
//==========================================================================
void VStreamMusicPlayer::Shutdown () {
  Stop();
}


//==========================================================================
//
//  VStreamMusicPlayer::Tick
//
//==========================================================================
void VStreamMusicPlayer::Tick () {
  if (!StrmOpened) return;
  if (Stopping && FinishTime+1.0 < Sys_Time()) {
    // finish playback
    Stop();
    return;
  }
  if (Paused) {
    // pause playback
    return;
  }
  for (int Len = SoundDevice->GetStreamAvailable(); Len; Len = SoundDevice->GetStreamAvailable()) {
    short *Data = SoundDevice->GetStreamBuffer();
    int StartPos = 0;
    while (!Stopping && StartPos < Len) {
      int SamplesDecoded = Codec->Decode(Data+StartPos*2, Len-StartPos);
      StartPos += SamplesDecoded;
      if (Codec->Finished()) {
        // stream ended
        if (CurrLoop) {
          // restart stream
          Codec->Restart();
        } else {
          // we'll wait for 1 second to finish playing
          Stopping = true;
          FinishTime = Sys_Time();
        }
      } else if (StartPos < Len) {
        // should never happen
        fprintf(stderr, "Stream decoded less but is not finished.\n");
        Stopping = true;
        FinishTime = Sys_Time();
      }
    }
    if (Stopping) {
      memset(Data+StartPos*2, 0, (Len-StartPos)*4);
    }
    SoundDevice->SetStreamData(Data, Len);
  }
}


//==========================================================================
//
//  VStreamMusicPlayer::Play
//
//==========================================================================
void VStreamMusicPlayer::Play (VAudioCodec *InCodec, const VStr &InName, bool InLoop) {
  StrmOpened = SoundDevice->OpenStream(InCodec->SampleRate, InCodec->SampleBits, InCodec->NumChannels);
  if (!StrmOpened) return;
  Codec = InCodec;
  CurrSong = InName;
  CurrLoop = InLoop;
  Stopping = false;
  if (Paused) Resume();
}


//==========================================================================
//
//  VStreamMusicPlayer::Pause
//
//==========================================================================
void VStreamMusicPlayer::Pause () {
  if (!StrmOpened) return;
  SoundDevice->PauseStream();
  Paused = true;
}


//==========================================================================
//
//  VStreamMusicPlayer::Resume
//
//==========================================================================
void VStreamMusicPlayer::Resume () {
  if (!StrmOpened) return;
  SoundDevice->ResumeStream();
  Paused = false;
}


//==========================================================================
//
//  VStreamMusicPlayer::Stop
//
//==========================================================================
void VStreamMusicPlayer::Stop () {
  if (!StrmOpened) return;
  delete Codec;
  Codec = nullptr;
  SoundDevice->CloseStream();
  StrmOpened = false;
}


//==========================================================================
//
//  VStreamMusicPlayer::IsPlaying
//
//==========================================================================
bool VStreamMusicPlayer::IsPlaying () {
  if (!StrmOpened) return false;
  return true;
}


//==========================================================================
//
//  VStreamMusicPlayer::SetPitch
//
//==========================================================================
void VStreamMusicPlayer::SetPitch (float pitch) {
  if (!StrmOpened) return;
  SoundDevice->SetStreamPitch(pitch);
}
