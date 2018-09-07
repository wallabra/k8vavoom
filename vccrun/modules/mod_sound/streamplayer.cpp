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
//#include "sound.h"

//#define VCCRUN_SOUND_THREAD_DEBUG
//#define VCCRUN_SOUND_THREAD_DUMMY


//==========================================================================
//
//  doTick
//
//  returns `true` if caller should perform stopping
//
//==========================================================================
static bool doTick (VStreamMusicPlayer *strm) {
  if (!strm->StrmOpened) return false;
  if (strm->Stopping && strm->FinishTime+1.0 < Sys_Time()) {
    // finish playback
    //Stop();
    return true;
  }
  if (strm->Paused) {
    // pause playback
    return false;
  }
  for (int Len = strm->SoundDevice->GetStreamAvailable(); Len; Len = strm->SoundDevice->GetStreamAvailable()) {
    short *Data = strm->SoundDevice->GetStreamBuffer();
    int StartPos = 0;
    while (!strm->Stopping && StartPos < Len) {
      int SamplesDecoded = strm->Codec->Decode(Data+StartPos*2, Len-StartPos);
      StartPos += SamplesDecoded;
      if (strm->Codec->Finished()) {
        // stream ended
        if (strm->CurrLoop) {
          // restart stream
          strm->Codec->Restart();
        } else {
          // we'll wait for 1 second to finish playing
          strm->Stopping = true;
          strm->FinishTime = Sys_Time();
        }
      } else if (StartPos < Len) {
        // should never happen
        fprintf(stderr, "Stream decoded less but is not finished.\n");
        strm->Stopping = true;
        strm->FinishTime = Sys_Time();
      }
    }
    if (strm->Stopping) memset(Data+StartPos*2, 0, (Len-StartPos)*4);
    strm->SoundDevice->SetStreamData(Data, Len);
  }
  return false;
}


//==========================================================================
//
//  VStreamMusicPlayer::stpThreadWaitPing
//
//  use this in streamer thread to check if there was a ping.
//  returns `true` if ping was received.
//
//==========================================================================
bool VStreamMusicPlayer::stpThreadWaitPing (unsigned int msecs) {
  if (msecs < 1) msecs = 1;
  mythread_condtime ctime;
  mythread_condtime_set(&ctime, &stpPingCond, msecs);
  auto res = mythread_cond_timedwait(&stpPingCond, &stpPingLock, &ctime);
  return (res == 0);
}


//==========================================================================
//
//  VStreamMusicPlayer::stpThreadSendPong
//
//  use this in streamer thread to notify main thead that it can go on
//
//==========================================================================
void VStreamMusicPlayer::stpThreadSendPong () {
#ifdef VCCRUN_SOUND_THREAD_DEBUG
  fprintf(stderr, "STP: getting lock for pong sending...\n");
#endif
  // we'll aquire lock if another thread is in cond_wait
  mythread_mutex_lock(&stpLockPong);
#ifdef VCCRUN_SOUND_THREAD_DEBUG
  fprintf(stderr, "STP: releasing lock for pong sending...\n");
#endif
  // and immediately release it
  mythread_mutex_unlock(&stpLockPong);
  // send signal
  mythread_cond_signal(&stpCondPong);
}


//==========================================================================
//
//  VStreamMusicPlayer::stpThreadSendCommand
//
//  send command to streamer thread, wait for it to be processed
//
//==========================================================================
void VStreamMusicPlayer::stpThreadSendCommand (STPCommand acmd) {
  stpcmd = acmd;
#ifdef VCCRUN_SOUND_THREAD_DEBUG
  fprintf(stderr, "MAIN: sending command %u...\n", (unsigned)stpcmd);
#endif
#ifdef VCCRUN_SOUND_THREAD_DEBUG
  fprintf(stderr, "MAIN:   getting lock for ping sending...\n");
#endif
  // we'll aquire lock if another thread is in cond_wait
  mythread_mutex_lock(&stpPingLock);
#ifdef VCCRUN_SOUND_THREAD_DEBUG
  fprintf(stderr, "MAIN:   releasing lock for ping sending...\n");
#endif
  // and immediately release it
  mythread_mutex_unlock(&stpPingLock);
  // send signal
  mythread_cond_signal(&stpPingCond);
#ifdef VCCRUN_SOUND_THREAD_DEBUG
  fprintf(stderr, "MAIN:   ping sent.\n");
#endif
  if (acmd == STP_Quit) {
#ifdef VCCRUN_SOUND_THREAD_DEBUG
    fprintf(stderr, "MAIN: waiting for streamer thread to stop\n");
#endif
    // wait for it to complete
    mythread_join(stpThread);
#ifdef VCCRUN_SOUND_THREAD_DEBUG
    fprintf(stderr, "MAIN: streamer thread stopped\n");
#endif
  } else {
    mythread_cond_wait(&stpCondPong, &stpLockPong);
#ifdef VCCRUN_SOUND_THREAD_DEBUG
    fprintf(stderr, "MAIN:   pong received.\n");
#endif
  }
}


//==========================================================================
//
//  streamPlayerThread
//
//==========================================================================
static MYTHREAD_RET_TYPE streamPlayerThread (void *adevobj) {
  VStreamMusicPlayer *strm = (VStreamMusicPlayer *)adevobj;
  mythread_mutex_lock(&strm->stpPingLock);
  // set sound device context for this thread
  strm->SoundDevice->AddCurrentThread();
  strm->SoundDevice->SetStreamPitch(strm->stpNewPitch);
  strm->SoundDevice->SetStreamVolume(strm->stpNewVolume);
  // send "we are ready" signal
  strm->stpThreadSendPong();
#ifdef VCCRUN_SOUND_THREAD_DEBUG
  fprintf(stderr, "STP: streaming thread started.\n");
#endif
  for (;;) {
#ifdef VCCRUN_SOUND_THREAD_DEBUG
    //fprintf(stderr, "STP: streaming thread waiting...\n");
#endif
    if (strm->stpThreadWaitPing(100*5)) {
#ifdef VCCRUN_SOUND_THREAD_DEBUG
      fprintf(stderr, "STP: streaming thread received the command: %u\n", (unsigned)strm->stpcmd);
#endif
      // ping received
      bool doQuit = false;
      switch (strm->stpcmd) {
        case VStreamMusicPlayer::STP_Quit: // stop playing, and quit immediately
        case VStreamMusicPlayer::STP_Stop: // stop current stream
          doQuit = (strm->stpcmd == VStreamMusicPlayer::STP_Quit);
#ifdef VCCRUN_SOUND_THREAD_DEBUG
          fprintf(stderr, "STP:   %s\n", (doQuit ? "quit" : "stop"));
#endif
          if (strm->StrmOpened) {
            // unpause it, just in case
            if (strm->Paused) {
              strm->SoundDevice->ResumeStream();
              strm->Paused = false;
            }
            strm->SoundDevice->CloseStream();
            strm->StrmOpened = false;
            delete strm->Codec;
            strm->Codec = nullptr;
          }
          break;
        case VStreamMusicPlayer::STP_Start: // start playing current stream
#ifdef VCCRUN_SOUND_THREAD_DEBUG
          fprintf(stderr, "STP:   start\n");
#endif
          strm->StrmOpened = true;
          strm->SoundDevice->SetStreamPitch(1.0f);
          strm->SoundDevice->SetStreamVolume(strm->stpNewVolume);
          break;
        case VStreamMusicPlayer::STP_Pause: // pause current stream
#ifdef VCCRUN_SOUND_THREAD_DEBUG
          fprintf(stderr, "STP:   pause\n");
#endif
          if (strm->StrmOpened) {
            strm->SoundDevice->PauseStream();
            strm->Paused = true;
          }
          break;
        case VStreamMusicPlayer::STP_Resume: // resume current stream
#ifdef VCCRUN_SOUND_THREAD_DEBUG
          fprintf(stderr, "STP:   resume\n");
#endif
          if (strm->StrmOpened) {
            strm->SoundDevice->ResumeStream();
            strm->Paused = false;
          }
          break;
        case VStreamMusicPlayer::STP_IsPlaying: // check if current stream is playing
#ifdef VCCRUN_SOUND_THREAD_DEBUG
          fprintf(stderr, "STP:   playing state request\n");
#endif
          strm->stpIsPlaying = strm->StrmOpened;
          break;
        case VStreamMusicPlayer::STP_SetPitch:
#ifdef VCCRUN_SOUND_THREAD_DEBUG
          fprintf(stderr, "STP:   pitch change\n");
#endif
          if (strm->StrmOpened) {
            strm->SoundDevice->SetStreamPitch(strm->stpNewPitch);
          }
          break;
        case VStreamMusicPlayer::STP_SetVolume:
#ifdef VCCRUN_SOUND_THREAD_DEBUG
          fprintf(stderr, "STP:   volume change\n");
#endif
          strm->SoundDevice->SetStreamVolume(strm->stpNewVolume);
          break;
      }
      // quit doesn't require pong
      if (doQuit) break;
      // send confirmation
#ifdef VCCRUN_SOUND_THREAD_DEBUG
      fprintf(stderr, "STP:   sending pong...\n");
#endif
      strm->stpThreadSendPong();
#ifdef VCCRUN_SOUND_THREAD_DEBUG
      fprintf(stderr, "STP:   pong sent.\n");
#endif
    }
    if (strm->StrmOpened) {
#ifdef VCCRUN_SOUND_THREAD_DEBUG
      //fprintf(stderr, "STP: streaming thread ticking...\n");
#endif
      // advance playing stream
#ifndef VCCRUN_SOUND_THREAD_DUMMY
      if (doTick(strm)) {
        // unpause it, just in case
        if (strm->Paused) {
          strm->SoundDevice->ResumeStream();
          strm->Paused = false;
        }
        strm->SoundDevice->CloseStream();
        strm->StrmOpened = false;
        delete strm->Codec;
        strm->Codec = nullptr;
      }
#endif
    }
  }
  strm->SoundDevice->RemoveCurrentThread();
  mythread_mutex_unlock(&strm->stpPingLock);
#ifdef VCCRUN_SOUND_THREAD_DEBUG
  fprintf(stderr, "STP: streaming thread complete.\n");
#endif
  return MYTHREAD_RET_VALUE;
}


//==========================================================================
//
//  VStreamMusicPlayer::Init
//
//==========================================================================
void VStreamMusicPlayer::Init () {
  mythread_mutex_init(&stpPingLock);
  mythread_cond_init(&stpPingCond);

  mythread_mutex_init(&stpLockPong);
  mythread_cond_init(&stpCondPong);

  // init for the first pong
  mythread_mutex_lock(&stpLockPong);

  // create stream player thread
  if (mythread_create(&stpThread, &streamPlayerThread, this)) Sys_Error("OpenAL driver cannot create streaming thread");
  // wait for the first pong
  mythread_cond_wait(&stpCondPong, &stpLockPong);
#ifdef VCCRUN_SOUND_THREAD_DEBUG
  fprintf(stderr, "MAIN: first pong received.\n");
#endif
  threadInited = true;
}


//==========================================================================
//
//  VStreamMusicPlayer::Shutdown
//
//==========================================================================
void VStreamMusicPlayer::Shutdown () {
  stpThreadSendCommand(STP_Quit);
  mythread_mutex_unlock(&stpLockPong);

  // threading cleanup
  mythread_mutex_destroy(&stpPingLock);
  mythread_cond_destroy(&stpPingCond);

  mythread_mutex_destroy(&stpLockPong);
  mythread_cond_destroy(&stpCondPong);

#ifdef VCCRUN_SOUND_THREAD_DEBUG
  fprintf(stderr, "MAIN: destroyed mutexes and conds\n");
#endif
}


//==========================================================================
//
//  VStreamMusicPlayer::Play
//
//==========================================================================
void VStreamMusicPlayer::Play (VAudioCodec *InCodec, const VStr &InName, bool InLoop) {
  if (!threadInited) Sys_Error("FATAL: cannot load song to uninited stream player");
  stpThreadSendCommand(STP_Stop);
  bool xopened = SoundDevice->OpenStream(InCodec->SampleRate, InCodec->SampleBits, InCodec->NumChannels);
  if (!xopened) {
    fprintf(stderr, "WARNING: cannot' start song '%s'...\n", *InName);
    return;
  }
  Codec = InCodec;
  CurrSong = InName;
  CurrLoop = InLoop;
  Stopping = false;
  stpNewVolume = lastVolume;
  stpNewPitch = 1.0f;
  stpThreadSendCommand(STP_Start);
}


//==========================================================================
//
//  VStreamMusicPlayer::Pause
//
//==========================================================================
void VStreamMusicPlayer::Pause () {
  if (threadInited) stpThreadSendCommand(STP_Pause);
}


//==========================================================================
//
//  VStreamMusicPlayer::Resume
//
//==========================================================================
void VStreamMusicPlayer::Resume () {
  if (threadInited) stpThreadSendCommand(STP_Resume);
}


//==========================================================================
//
//  VStreamMusicPlayer::Stop
//
//==========================================================================
void VStreamMusicPlayer::Stop () {
  if (threadInited) stpThreadSendCommand(STP_Stop);
}


//==========================================================================
//
//  VStreamMusicPlayer::IsPlaying
//
//==========================================================================
bool VStreamMusicPlayer::IsPlaying () {
  if (!threadInited) return false;
  stpThreadSendCommand(STP_IsPlaying);
  return stpIsPlaying;
}


//==========================================================================
//
//  VStreamMusicPlayer::SetPitch
//
//==========================================================================
void VStreamMusicPlayer::SetPitch (float pitch) {
  stpNewPitch = pitch;
  if (threadInited) stpThreadSendCommand(STP_SetPitch);
}


//==========================================================================
//
//  VStreamMusicPlayer::SetVolume
//
//==========================================================================
void VStreamMusicPlayer::SetVolume (float volume) {
  if (volume == lastVolume) return;
  lastVolume = volume;
  stpNewVolume = volume;
  if (threadInited) stpThreadSendCommand(STP_SetVolume);
}
