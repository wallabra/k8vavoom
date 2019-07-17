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


//==========================================================================
//
//  doTick
//
//  returns `true` if caller should perform stopping
//
//==========================================================================
static bool doTick (VStreamMusicPlayer *strm) {
  if (!strm->StrmOpened) return false;
  if (strm->Stopping && strm->FinishTime+0.5 < Sys_Time()) {
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
    int decodedFromLoop = 0, loopCount = 0;
    while (!strm->Stopping && StartPos < Len) {
      int SamplesDecoded = strm->Codec->Decode(Data+StartPos*2, Len-StartPos);
      if (SamplesDecoded < 0) SamplesDecoded = 0;
      StartPos += SamplesDecoded;
      decodedFromLoop += SamplesDecoded;
      if (strm->Codec->Finished() || SamplesDecoded <= 0) {
        // stream ended
        if (strm->CurrLoop) {
          // restart stream
          strm->Codec->Restart();
          ++loopCount;
          if (loopCount == 1) decodedFromLoop = 0;
          if (loopCount == 3 && decodedFromLoop < 256) {
            GLog.WriteLine("Looped music stream is too short, aborting it...");
            strm->CurrLoop = false;
            strm->Stopping = true;
            strm->FinishTime = Sys_Time();
          }
        } else {
          // we'll wait for some time to finish playing
          strm->Stopping = true;
          strm->FinishTime = Sys_Time();
        }
      } else if (StartPos < Len) {
        // should never happen
        GLog.WriteLine("Stream decoded less but is not finished.");
        strm->Stopping = true;
        strm->FinishTime = Sys_Time();
      }
    }
    if (strm->Stopping && StartPos < Len) memset(Data+StartPos*2, 0, (Len-StartPos)*4);
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
  GLog.WriteLine("STP: getting lock for pong sending...");
#endif
  // we'll aquire lock if another thread is in cond_wait
  mythread_mutex_lock(&stpLockPong);
#ifdef VCCRUN_SOUND_THREAD_DEBUG
  GLog.WriteLine("STP: releasing lock for pong sending...");
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
  GLog.WriteLine("MAIN: sending command %u...", (unsigned)stpcmd);
#endif
#ifdef VCCRUN_SOUND_THREAD_DEBUG
  GLog.WriteLine("MAIN:   getting lock for ping sending...");
#endif
  // we'll aquire lock if another thread is in cond_wait
  mythread_mutex_lock(&stpPingLock);
#ifdef VCCRUN_SOUND_THREAD_DEBUG
  GLog.WriteLine("MAIN:   releasing lock for ping sending...");
#endif
  // and immediately release it
  mythread_mutex_unlock(&stpPingLock);
  // send signal
  mythread_cond_signal(&stpPingCond);
#ifdef VCCRUN_SOUND_THREAD_DEBUG
  GLog.WriteLine("MAIN:   ping sent.");
#endif
  if (acmd == STP_Quit) {
#ifdef VCCRUN_SOUND_THREAD_DEBUG
    GLog.WriteLine("MAIN: waiting for streamer thread to stop");
#endif
    // wait for it to complete
    mythread_join(stpThread);
#ifdef VCCRUN_SOUND_THREAD_DEBUG
    GLog.WriteLine("MAIN: streamer thread stopped");
#endif
  } else {
    mythread_cond_wait(&stpCondPong, &stpLockPong);
#ifdef VCCRUN_SOUND_THREAD_DEBUG
    GLog.WriteLine("MAIN:   pong received.");
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
  GLog.WriteLine("STP: streaming thread started.");
#endif
  for (;;) {
#ifdef VCCRUN_SOUND_THREAD_DEBUG
    //GLog.WriteLine("STP: streaming thread waiting...");
#endif
    if (strm->stpThreadWaitPing(100*5)) {
#ifdef VCCRUN_SOUND_THREAD_DEBUG
      GLog.WriteLine("STP: streaming thread received the command: %u", (unsigned)strm->stpcmd);
#endif
      // ping received
      bool doQuit = false;
      switch (strm->stpcmd) {
        case VStreamMusicPlayer::STP_Quit: // stop playing, and quit immediately
        case VStreamMusicPlayer::STP_Stop: // stop current stream
          doQuit = (strm->stpcmd == VStreamMusicPlayer::STP_Quit);
#ifdef VCCRUN_SOUND_THREAD_DEBUG
          GLog.WriteLine("STP:   %s", (doQuit ? "quit" : "stop"));
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
          GLog.WriteLine("STP:   start");
#endif
          strm->StrmOpened = true;
          strm->SoundDevice->SetStreamPitch(1.0f);
          strm->SoundDevice->SetStreamVolume(strm->stpNewVolume);
          break;
        case VStreamMusicPlayer::STP_Pause: // pause current stream
#ifdef VCCRUN_SOUND_THREAD_DEBUG
          GLog.WriteLine("STP:   pause");
#endif
          if (strm->StrmOpened) {
            strm->SoundDevice->PauseStream();
            strm->Paused = true;
          }
          break;
        case VStreamMusicPlayer::STP_Resume: // resume current stream
#ifdef VCCRUN_SOUND_THREAD_DEBUG
          GLog.WriteLine("STP:   resume");
#endif
          if (strm->StrmOpened) {
            strm->SoundDevice->ResumeStream();
            strm->Paused = false;
          }
          break;
        case VStreamMusicPlayer::STP_IsPlaying: // check if current stream is playing
#ifdef VCCRUN_SOUND_THREAD_DEBUG
          GLog.WriteLine("STP:   playing state request");
#endif
          strm->stpIsPlaying = strm->StrmOpened;
          break;
        case VStreamMusicPlayer::STP_SetPitch:
#ifdef VCCRUN_SOUND_THREAD_DEBUG
          GLog.WriteLine("STP:   pitch change");
#endif
          if (strm->StrmOpened) {
            strm->SoundDevice->SetStreamPitch(strm->stpNewPitch);
          }
          break;
        case VStreamMusicPlayer::STP_SetVolume:
#ifdef VCCRUN_SOUND_THREAD_DEBUG
          GLog.WriteLine("STP:   volume change");
#endif
          strm->SoundDevice->SetStreamVolume(strm->stpNewVolume);
          break;
      }
      // quit doesn't require pong
      if (doQuit) break;
      // send confirmation
#ifdef VCCRUN_SOUND_THREAD_DEBUG
      GLog.WriteLine("STP:   sending pong...");
#endif
      strm->stpThreadSendPong();
#ifdef VCCRUN_SOUND_THREAD_DEBUG
      GLog.WriteLine("STP:   pong sent.");
#endif
    }
    if (strm->StrmOpened) {
#ifdef VCCRUN_SOUND_THREAD_DEBUG
      //GLog.WriteLine("STP: streaming thread ticking...");
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
  GLog.WriteLine("STP: streaming thread complete.");
#endif
  Z_ThreadDone();
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
  GLog.WriteLine("MAIN: first pong received.");
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
  GLog.WriteLine("MAIN: destroyed mutexes and conds");
#endif
}


//==========================================================================
//
//  VStreamMusicPlayer::Play
//
//==========================================================================
void VStreamMusicPlayer::Play (VAudioCodec *InCodec, const char *InName, bool InLoop) {
  if (!threadInited) Sys_Error("FATAL: cannot load song to uninited stream player");
  stpThreadSendCommand(STP_Stop);
  if (InName && InName[0]) {
    bool xopened = SoundDevice->OpenStream(InCodec->SampleRate, InCodec->SampleBits, InCodec->NumChannels);
    if (!xopened) {
      GLog.WriteLine("WARNING: cannot' start song '%s'...", InName);
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
