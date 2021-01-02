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
//**  Copyright (C) 2018-2021 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, version 3 of the License ONLY.
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
#include "../gamedefs.h"
#include "snd_local.h"

//#define VCCRUN_SOUND_THREAD_DEBUG

#ifdef VCCRUN_SOUND_THREAD_DEBUG
# define SDLOG(...)  GLog.WriteLine(__VA_ARGS__)
#else
# define SDLOG(...)  do {} while (0)
#endif


extern VCvarF snd_master_volume;


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
    vint16 *Data = strm->SoundDevice->GetStreamBuffer();
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
            GLog.WriteLine("Looped music stream is too short, aborting it");
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
  SDLOG("STP: getting lock for pong sending");
  // we'll aquire lock if another thread is in cond_wait
  mythread_mutex_lock(&stpLockPong);
  SDLOG("STP: releasing lock for pong sending");
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
  SDLOG("MAIN: sending command %u", (unsigned)stpcmd);
  SDLOG("MAIN:   getting lock for ping sending");
  // we'll aquire lock if another thread is in cond_wait
  mythread_mutex_lock(&stpPingLock);
  SDLOG("MAIN:   releasing lock for ping sending");
  // and immediately release it
  mythread_mutex_unlock(&stpPingLock);
  // send signal
  mythread_cond_signal(&stpPingCond);
  SDLOG("MAIN:   ping sent.");
  if (acmd == STP_Quit) {
    SDLOG("MAIN: waiting for streamer thread to stop");
    // wait for it to complete
    mythread_join(stpThread);
    SDLOG("MAIN: streamer thread stopped");
  } else {
    mythread_cond_wait(&stpCondPong, &stpLockPong);
    SDLOG("MAIN:   pong received.");
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
  bool doLoop = false;
  VStr newSongName;
  bool doLoadNewSong = false;
  SDLOG("STP: streaming thread started.");
  for (;;) {
    //SDLOG("STP: streaming thread waiting");
    if (strm->stpThreadWaitPing(100*5)) {
      SDLOG("STP: streaming thread received the command: %u", (unsigned)strm->stpcmd);
      // ping received
      bool doQuit = false;
      switch (strm->stpcmd) {
        case VStreamMusicPlayer::STP_Quit: // stop playing, and quit immediately
        case VStreamMusicPlayer::STP_Stop: // stop current stream
          doQuit = (strm->stpcmd == VStreamMusicPlayer::STP_Quit);
          SDLOG("STP:   %s", (doQuit ? "quit" : "stop"));
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
          SDLOG("STP:   start");
          strm->StrmOpened = true;
          strm->SoundDevice->SetStreamPitch(1.0f);
          strm->SoundDevice->SetStreamVolume(strm->stpNewVolume);
          break;
        case VStreamMusicPlayer::STP_Pause: // pause current stream
          SDLOG("STP:   pause");
          if (strm->StrmOpened) {
            strm->SoundDevice->PauseStream();
            strm->Paused = true;
          }
          break;
        case VStreamMusicPlayer::STP_Resume: // resume current stream
          SDLOG("STP:   resume");
          if (strm->StrmOpened) {
            strm->SoundDevice->ResumeStream();
            strm->Paused = false;
          }
          break;
        case VStreamMusicPlayer::STP_IsPlaying: // check if current stream is playing
          SDLOG("STP:   playing state request");
          strm->stpIsPlaying = strm->StrmOpened;
          break;
        case VStreamMusicPlayer::STP_SetPitch:
          SDLOG("STP:   pitch change");
          if (strm->StrmOpened) {
            strm->SoundDevice->SetStreamPitch(strm->stpNewPitch);
          }
          break;
        case VStreamMusicPlayer::STP_SetVolume:
          SDLOG("STP:   volume change");
          strm->SoundDevice->SetStreamVolume(strm->stpNewVolume);
          break;
        case VStreamMusicPlayer::STP_PlaySongLooped:
        case VStreamMusicPlayer::STP_PlaySong:
          SDLOG("STP:   play song X");
          doLoadNewSong = true;
          doLoop = (strm->stpcmd == VStreamMusicPlayer::STP_PlaySongLooped);
          newSongName = VStr(strm->namebuf);
          break;
      }
      // quit doesn't require pong
      if (doQuit) break;
      // send confirmation
      SDLOG("STP:   sending pong");
      strm->stpThreadSendPong();
      SDLOG("STP:   pong sent.");
    }
    // load new song
    if (doLoadNewSong) {
      doLoadNewSong = false;
      bool wasPlaying;
      // stop current song
      if (strm->StrmOpened) {
        wasPlaying = true;
        // unpause it, just in case
        if (strm->Paused) {
          strm->SoundDevice->ResumeStream();
          strm->Paused = false;
        }
        strm->SoundDevice->CloseStream();
        strm->StrmOpened = false;
        delete strm->Codec;
        strm->Codec = nullptr;
      } else {
        wasPlaying = false;
      }
      // load a new song
      if (!newSongName.isEmpty() && GAudio) {
        // unlock thread, so other song commands won't stall
        // k8: nope, it won't work anyway
        //mythread_mutex_unlock(&stpPingLock);
        //mythread_mutex_lock(&stpPingLock);
        //GLog.Logf("STRM: loading song '%s'", *newSongName);
        VAudioCodec *codec = GAudio->LoadSongInternal(*newSongName, wasPlaying, true); // called from streaming thread
        if (codec) {
          bool xopened = strm->SoundDevice->OpenStream(codec->SampleRate, codec->SampleBits, codec->NumChannels);
          if (!xopened) {
            GLog.WriteLine(NAME_Warning, "cannot' start song '%s'", *newSongName);
          } else {
            //GLog.Logf("STRM: starting song '%s'", *newSongName);
            strm->Codec = codec;
            strm->CurrSong = newSongName;
            strm->CurrLoop = doLoop;
            strm->Stopping = false;
            strm->stpNewVolume = strm->lastVolume;
            strm->stpNewPitch = 1.0f;
            //stpThreadSendCommand(STP_Start);
            strm->StrmOpened = true;
            strm->SoundDevice->SetStreamPitch(1.0f);
            strm->SoundDevice->SetStreamVolume(strm->stpNewVolume);
          }
        }
      }
      newSongName.clear();
    }
    // tick
    if (strm->StrmOpened) {
      //SDLOG("STP: streaming thread ticking");
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
  SDLOG("STP: streaming thread complete.");
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
  SDLOG("MAIN: first pong received.");
  threadInited = true;
}


//==========================================================================
//
//  VStreamMusicPlayer::Shutdown
//
//==========================================================================
void VStreamMusicPlayer::Shutdown () {
  if (SoundDevice) {
    if (developer) GLog.Log(NAME_Dev, "VStreamMusicPlayer::Shutdown(): sending quit command");
    stpThreadSendCommand(STP_Quit); // this joins player thread
    mythread_mutex_unlock(&stpLockPong);

    // threading cleanup
    mythread_mutex_destroy(&stpPingLock);
    mythread_cond_destroy(&stpPingCond);

    mythread_mutex_destroy(&stpLockPong);
    mythread_cond_destroy(&stpCondPong);

    if (developer) GLog.Log(NAME_Dev, "VStreamMusicPlayer::Shutdown(): shutdown complete!");
    SDLOG("MAIN: destroyed mutexes and conds");
    SoundDevice = nullptr;
  }
  threadInited = false;
}


//==========================================================================
//
//  VStreamMusicPlayer::Play
//
//==========================================================================
void VStreamMusicPlayer::Play (VAudioCodec *InCodec, const char *InName, bool InLoop) {
  if (!threadInited) {
    GLog.Logf(NAME_Error, "*** cannot load song to uninited stream player!");
    return;
  }
  stpThreadSendCommand(STP_Stop);
  if (InName && InName[0]) {
    bool xopened = SoundDevice->OpenStream(InCodec->SampleRate, InCodec->SampleBits, InCodec->NumChannels);
    if (!xopened) {
      GLog.WriteLine("WARNING: cannot' start song '%s'", InName);
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
//  VStreamMusicPlayer::LoadAndPlay
//
//==========================================================================
void VStreamMusicPlayer::LoadAndPlay (const char *InName, bool InLoop) {
  if (!threadInited) {
    GLog.Logf(NAME_Error, "*** cannot load song to uninited stream player!");
    return;
  }
  if (!InName) InName = "";
  memset(namebuf, 0, sizeof(namebuf));
  strncpy(namebuf, InName, sizeof(namebuf)-1);
  stpThreadSendCommand(InLoop ? STP_PlaySongLooped : STP_PlaySong);
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
void VStreamMusicPlayer::SetVolume (float volume, bool fromStreamThread) {
  volume = clampval(volume*snd_master_volume.asFloat(), 0.0f, 1.0f);
  if (volume == lastVolume) return;
  lastVolume = volume;
  stpNewVolume = volume;
  if (!fromStreamThread) {
    if (threadInited) stpThreadSendCommand(STP_SetVolume);
  }
}
