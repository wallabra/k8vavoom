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
// main audio management class
class VAudio : public VAudioPublic {
public:
  VAudio ();
  ~VAudio ();

  // top level methods
  virtual void Init () override;
  virtual void Shutdown () override;

  // playback of sound effects
  virtual int PlaySound (int InSoundId, const TVec &origin, const TVec &velocity, int origin_id, int channel, float volume, float Attenuation, bool Loop) override;
  virtual void StopSound (int origin_id, int channel) override;
  virtual void StopAllSound () override;
  virtual bool IsSoundPlaying (int origin_id, int InSoundId) override;
  virtual void SetSoundPitch (int origin_id, int InSoundId, float pitch) override;

  // music and general sound control
  virtual void UpdateSounds () override;

  virtual void SetListenerOrigin (const TVec &aorigin) override;

  virtual bool PlayMusic (const VStr &filename, bool Loop) override;
  virtual bool IsMusicPlaying () override;
  virtual void PauseMusic () override;
  virtual void ResumeMusic () override;
  virtual void StopMusic () override;
  virtual void SetMusicPitch (float pitch) override;

private:
  enum { MAX_CHANNELS = 256 };

  enum { PRIORITY_MAX_ADJUST = 10 };

  // info about sounds currently playing
  struct FChannel {
    int origin_id;
    int channel;
    TVec origin;
    TVec velocity;
    int sound_id;
    int priority;
    float volume;
    float attenuation;
    int handle;
    bool is3D;
    bool localPlayerSound;
    bool loop;
    float pitch;
    float newPitch;
  };

  // sound curve
  vuint8 *SoundCurve;
  int MaxSoundDist;

  // stream music player
  bool StreamPlaying;
  VStreamMusicPlayer *StreamMusicPlayer;

  // list of currently playing sounds
  FChannel Channel[MAX_CHANNELS];
  int NumChannels;
  int SndCount;

  // maximum volume for sound
  //float MaxVolume;

  // listener orientation
  TVec ListenerForward;
  TVec ListenerRight;
  TVec ListenerUp;
  TVec ListenerOrigin;

  // hardware devices
  VSoundDevice *SoundDevice;

  // friends
  //friend class TCmdMusic;

  // sound effect helpers
  int GetChannel (int sound_id, int origin_id, int channel, int priority);
  void StopChannel (int chan_num);
  void UpdateSfx ();
};


VAudioPublic *GAudio;


// ////////////////////////////////////////////////////////////////////////// //
float VAudioPublic::snd_sfx_volume = 1.0; //("snd_sfx_volume", "0.5", "Sound effects volume.", CVAR_Archive);
float VAudioPublic::snd_music_volume = 1.0; //("snd_music_volume", "0.5", "Music volume", CVAR_Archive);
bool VAudioPublic::snd_swap_stereo = false; //("snd_swap_stereo", false, "Swap stereo channels?", CVAR_Archive);
int VAudioPublic::snd_channels = 64; //("snd_channels", "128", "Number of sound channels.", CVAR_Archive);


// ////////////////////////////////////////////////////////////////////////// //
// Public CVars
//bool snd_mod_player = true; //("snd_mod_player", true, "Allow music modules?", CVAR_Archive);

FAudioCodecDesc *FAudioCodecDesc::List;


// ////////////////////////////////////////////////////////////////////////// //
static FSoundDeviceDesc *SoundDeviceList[SNDDRV_MAX];


//==========================================================================
//
//  VAudioPublic::Create
//
//==========================================================================
VAudioPublic *VAudioPublic::Create () {
  return new VAudio();
}


//==========================================================================
//
//  VAudio::VAudio
//
//==========================================================================
VAudio::VAudio()
  : SoundCurve(nullptr)
  , MaxSoundDist(0)
  , StreamPlaying(false)
  , StreamMusicPlayer(nullptr)
  , NumChannels(0)
  , SndCount(0)
  //, MaxVolume(0)
  , SoundDevice(nullptr)
{
  memset(Channel, 0, sizeof(Channel));
  ListenerForward = TVec(0, 0, -1);
  ListenerUp = TVec(0, 1, 0);
  ListenerRight = TVec(1, 0, 0);
  ListenerOrigin = TVec(0, 0, 0);
}


//==========================================================================
//
//  VAudio::~VAudio
//
//==========================================================================
VAudio::~VAudio () {
  Shutdown();
}


//==========================================================================
//
//  VAudio::Init
//
//  Initialises sound stuff, including volume
//  Sets channels, SFX and music volume, allocates channel buffer.
//
//==========================================================================
void VAudio::Init () {
  // initialise sound driver
  int SIdx = 0;
  if (SIdx != -1) {
    //GCon->Logf(NAME_Init, "Selected %s", SoundDeviceList[SIdx]->Description);
    SoundDevice = SoundDeviceList[SIdx]->Creator();
    if (!SoundDevice->Init()) {
      delete SoundDevice;
      SoundDevice = nullptr;
    }
  }

  // initialise stream music player
  if (SoundDevice) {
    StreamMusicPlayer = new VStreamMusicPlayer(SoundDevice);
    StreamMusicPlayer->Init();
  }

  MaxSoundDist = 1200;
  SoundCurve = new vuint8[MaxSoundDist];
  for (int i = 0; i < MaxSoundDist; i++) SoundCurve[i] = MIN(127, (MaxSoundDist-i)*127/(MaxSoundDist-160));
  //MaxVolume = -1;

  // free all channels for use
  memset(Channel, 0, sizeof(Channel));
  NumChannels = (SoundDevice ? SoundDevice->SetChannels(snd_channels) : 0);
  for (int f = 0; f < NumChannels; ++f) Channel[f].handle = -1;
}


//==========================================================================
//
//  VAudio::Shutdown
//
//  Shuts down all sound stuff
//
//==========================================================================
void VAudio::Shutdown () {
  // stop playback of all sounds
  StopAllSound();
  if (StreamMusicPlayer) {
    StreamMusicPlayer->Shutdown();
    delete StreamMusicPlayer;
    StreamMusicPlayer = nullptr;
  }
  if (SoundDevice) {
    SoundDevice->Shutdown();
    delete SoundDevice;
    SoundDevice = nullptr;
  }
  if (SoundCurve) {
    delete[] SoundCurve;
    SoundCurve = nullptr;
  }
}


//==========================================================================
//
//  VAudio::SetListenerOrigin
//
//==========================================================================
void VAudio::SetListenerOrigin (const TVec &aorigin) {
  ListenerOrigin = aorigin;
}


//==========================================================================
//
//  VAudio::PlaySound
//
//  This function adds a sound to the list of currently active sounds, which
//  is maintained as a given number of internal channels.
//
//  Returns allocated channel or -1
//
//==========================================================================
int VAudio::PlaySound (int InSoundId, const TVec &origin,
  const TVec &velocity, int origin_id, int channel, float volume,
  float Attenuation, bool Loop)
{
  //fprintf(stderr, "InSoundId: %d; maxvol=%f; vol=%f\n", InSoundId, (double)MaxVolume, (double)volume);
  if (!SoundDevice || InSoundId < 1 || /*MaxVolume <= 0 ||*/ volume <= 0 || snd_sfx_volume <= 0) return -1;

  // find actual sound ID to use
  int sound_id = InSoundId; //GSoundManager->ResolveSound(InSoundId);

  // if it's a looping sound and it's still playing, then continue playing the existing one
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle >= 0 && Channel[i].origin_id == origin_id &&
        Channel[i].channel == channel &&
        Channel[i].sound_id == sound_id && Channel[i].loop)
    {
      return i;
    }
  }

  // apply sound volume
  volume *= snd_sfx_volume; // MaxVolume;

  // check if this sound is emited by the local player
  bool LocalPlayerSound = (origin_id == 0); //(cl && cl->MO && cl->MO->SoundOriginID == origin_id);

  // calculate the distance before other stuff so that we can throw out
  // sounds that are beyond the hearing range
  int dist = 0;
  if (!LocalPlayerSound && Attenuation > 0 /*&& cl*/) {
    dist = (int)(Length(origin-ListenerOrigin)*Attenuation);
  }
  if (dist >= MaxSoundDist) return -1; // sound is beyond the hearing range

  int priority = GSoundManager->S_sfx[sound_id].priority*(PRIORITY_MAX_ADJUST-PRIORITY_MAX_ADJUST*dist/MaxSoundDist);

  int chan = GetChannel(sound_id, origin_id, channel, priority);
  if (chan == -1) return -1; // no free channels

  float pitch = 1.0;
  if (GSoundManager->S_sfx[sound_id].changePitch) {
    pitch = 1.0+(Random()-Random())*GSoundManager->S_sfx[sound_id].changePitch;
  }

  int handle;
  bool is3D;
  if (LocalPlayerSound || Attenuation <= 0) {
    // local sound
    handle = SoundDevice->PlaySound(sound_id, volume, 0, pitch, Loop);
    is3D = false;
  } else if (false) {
    float vol = SoundCurve[dist]/127.0*volume;
    float sep = DotProduct(origin-ListenerOrigin, ListenerRight)/MaxSoundDist;
    if (snd_swap_stereo) sep = -sep;
    handle = SoundDevice->PlaySound(sound_id, vol, sep, pitch, Loop);
    is3D = false;
  } else {
    handle = SoundDevice->PlaySound3D(sound_id, origin, velocity, volume, pitch, Loop);
    is3D = true;
  }
  if (handle >= 0) {
    //fprintf(stderr, "CHAN: %d; handle: %d\n", chan, handle);
    Channel[chan].origin_id = origin_id;
    Channel[chan].channel = channel;
    Channel[chan].origin = origin;
    Channel[chan].velocity = velocity;
    Channel[chan].sound_id = sound_id;
    Channel[chan].priority = priority;
    Channel[chan].volume = volume;
    Channel[chan].attenuation = Attenuation;
    Channel[chan].handle = handle;
    Channel[chan].is3D = is3D;
    Channel[chan].localPlayerSound = LocalPlayerSound;
    Channel[chan].loop = Loop;
    Channel[chan].pitch = pitch;
    Channel[chan].newPitch = pitch;
    return chan;
  } else {
    Channel[chan].handle = -1;
    Channel[chan].origin_id = 0;
    Channel[chan].sound_id = 0;
  }
  return -1;
}


//==========================================================================
//
//  VAudio::GetChannel
//
//==========================================================================
int VAudio::GetChannel (int sound_id, int origin_id, int channel, int priority) {
  int chan;
  int i;
  int lp; //least priority
  int found;
  int prior;
  int numchannels = GSoundManager->S_sfx[sound_id].numChannels;

  if (numchannels > 0) {
    lp = -1; // denote the argument sound_id
    found = 0;
    prior = priority;
    for (i = 0; i < NumChannels; ++i) {
      if (Channel[i].sound_id == sound_id) {
        if (GSoundManager->S_sfx[sound_id].bSingular) {
          // this sound is already playing, so don't start it again
          return -1;
        }
        ++found; // found one; now, should we replace it?
        if (prior >= Channel[i].priority) {
          // if we're gonna kill one, then this'll be it
          lp = i;
          prior = Channel[i].priority;
        }
      }
    }

    if (found >= numchannels) {
      if (lp == -1) {
        // other sounds have greater priority
        return -1; // don't replace any sounds
      }
      StopChannel(lp);
    }
  }

  // look for a free channel
  for (i = 0; i < NumChannels; ++i) {
    if (!Channel[i].sound_id) return i;
  }

  // look for a lower priority sound to replace.
  ++SndCount;
  if (SndCount >= NumChannels) SndCount = 0;

  for (chan = 0; chan < NumChannels; ++chan) {
    i = (SndCount+chan)%NumChannels;
    if (priority >= Channel[i].priority) {
      // replace the lower priority sound
      StopChannel(i);
      return i;
    }
  }

  // no free channels
  return -1;
}


//==========================================================================
//
//  VAudio::StopChannel
//
//==========================================================================
void VAudio::StopChannel (int chan_num) {
  if (Channel[chan_num].sound_id) {
    SoundDevice->StopChannel(Channel[chan_num].handle);
    Channel[chan_num].handle = -1;
    Channel[chan_num].origin_id = 0;
    Channel[chan_num].sound_id = 0;
  }
}


//==========================================================================
//
//  VAudio::StopSound
//
//==========================================================================
void VAudio::StopSound (int origin_id, int channel) {
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].origin_id == origin_id && (!channel || Channel[i].channel == channel)) {
      StopChannel(i);
    }
  }
}


//==========================================================================
//
//  VAudio::StopAllSound
//
//==========================================================================
void VAudio::StopAllSound () {
  for (int i = 0; i < NumChannels; ++i) StopChannel(i);
}


//==========================================================================
//
//  VAudio::IsSoundPlaying
//
//==========================================================================
bool VAudio::IsSoundPlaying (int origin_id, int InSoundId) {
  int sound_id = InSoundId; //GSoundManager->ResolveSound(InSoundId);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle < 0) continue;
    //fprintf(stderr, "i=%d; sid=%d; oid=%d (%d:%d)\n", i, Channel[i].sound_id, Channel[i].origin_id, sound_id, origin_id);
    if (Channel[i].sound_id == sound_id && Channel[i].origin_id == origin_id) {
      //fprintf(stderr, "FOUND! %d\n", (int)SoundDevice->IsChannelPlaying(Channel[i].handle));
      if (SoundDevice->IsChannelPlaying(Channel[i].handle)) {
        return true;
      }
    }
  }
  return false;
}


//==========================================================================
//
//  VAudio::SetSoundPitch
//
//==========================================================================
void VAudio::SetSoundPitch (int origin_id, int InSoundId, float pitch) {
  int sound_id = InSoundId; //GSoundManager->ResolveSound(InSoundId);
  for (int i = 0; i < NumChannels; ++i) {
    if (Channel[i].handle < 0) continue;
    //fprintf(stderr, "i=%d; sid=%d; oid=%d (%d:%d)\n", i, Channel[i].sound_id, Channel[i].origin_id, sound_id, origin_id);
    if (Channel[i].sound_id == sound_id && Channel[i].origin_id == origin_id) {
      Channel[i].newPitch = pitch;
    }
  }
}


//==========================================================================
//
//  VAudio::UpdateSfx
//
//  Update the sound parameters. Used to control volume and pan
//  changes such as when a player turns.
//
//==========================================================================
void VAudio::UpdateSfx () {
  if (!SoundDevice || !NumChannels) return;

  /*
  if (snd_sfx_volume != MaxVolume) {
    MaxVolume = snd_sfx_volume;
    if (MaxVolume <= 0) StopAllSound();
  }

  if (MaxVolume <= 0) return; // silence
  */
  if (snd_sfx_volume <= 0) {
    StopAllSound();
    return;
  }

  /*
  if (cl) {
    AngleVectors(cl->ViewAngles, ListenerForward, ListenerRight, ListenerUp);
  }
  */

  for (int i = 0; i < NumChannels; ++i) {
    if (!Channel[i].sound_id) continue; // nothing on this channel
    if (Channel[i].handle < 0) continue;
    if (!SoundDevice->IsChannelPlaying(Channel[i].handle)) {
      // playback done
      StopChannel(i);
      continue;
    }
    if (!Channel[i].origin_id || Channel[i].attenuation <= 0) continue; // full volume sound

    if (Channel[i].localPlayerSound) continue; // client sound

    // move sound
    //!Channel[i].origin += Channel[i].velocity*host_frametime;

    //if (!cl) continue;

    int dist = (int)(Length(Channel[i].origin-ListenerOrigin)*Channel[i].attenuation);
    if (dist >= MaxSoundDist) {
      // too far away
      StopChannel(i);
      continue;
    }

    // update params
    if (!Channel[i].is3D) {
      float vol = SoundCurve[dist]/127.0*Channel[i].volume;
      float sep = DotProduct(Channel[i].origin-ListenerOrigin, ListenerRight)/MaxSoundDist;
      if (snd_swap_stereo) sep = -sep;
      SoundDevice->UpdateChannel(Channel[i].handle, vol, sep);
    } else {
      SoundDevice->UpdateChannel3D(Channel[i].handle, Channel[i].origin, Channel[i].velocity);
    }
    if (Channel[i].newPitch != Channel[i].pitch) {
      SoundDevice->UpdateChannelPitch(Channel[i].handle, Channel[i].newPitch);
      Channel[i].pitch = Channel[i].newPitch;
    }
    Channel[i].priority = GSoundManager->S_sfx[Channel[i].sound_id].priority*(PRIORITY_MAX_ADJUST-PRIORITY_MAX_ADJUST*dist/MaxSoundDist);
  }

  if (true) {
    SoundDevice->UpdateListener(ListenerOrigin, TVec(0, 0, 0), ListenerForward, ListenerRight, ListenerUp);
  }

  //SoundDevice->Tick();
}


//==========================================================================
//
//  VAudio::UpdateSounds
//
//  Updates music & sounds
//
//==========================================================================
void VAudio::UpdateSounds () {
  // check sound volume
  if (snd_sfx_volume < 0.0) snd_sfx_volume = 0.0;
  if (snd_sfx_volume > 1.0) snd_sfx_volume = 1.0;

  // check music volume
  if (snd_music_volume < 0.0) snd_music_volume = 0.0;
  if (snd_music_volume > 1.0) snd_music_volume = 1.0;

  UpdateSfx();
  if (StreamMusicPlayer) {
    SoundDevice->SetStreamVolume(snd_music_volume/* *MusicVolumeFactor*/);
    StreamMusicPlayer->Tick();
  }
}


//==========================================================================
//
//  VAudio::PlayMusic
//
//==========================================================================
bool VAudio::PlayMusic (const VStr &filename, bool Loop) {
#if 0
  static const char *Exts[] = {
    "ogg", "flac",
    /*
    "mp3", "wav", "mid", "mus", "669",
    "amf", "dsm", "far", "gdm", "imf", "it", "m15", "med", "mod", "mtm",
    "okt", "s3m", "stm", "stx", "ult", "uni", "xm",
    //"ay", "gbs", "gym", "hes", "kss", "nsf", "nsfe", "sap", "sgc", "spc", "vgm",
    */
    nullptr
  };
  static const char *ExtraExts[] = { "ogg", "mp3", nullptr };
#endif

  if (StreamPlaying) StreamMusicPlayer->Stop();
  StreamPlaying = false;

  if (filename.isEmpty()) return false;

  // get music volume for this song
  //MusicVolumeFactor = GSoundManager->GetMusicVolume(Song);
  if (StreamMusicPlayer) SoundDevice->SetStreamVolume(snd_music_volume/* *MusicVolumeFactor*/);

  VStream *Strm = fsysOpenFile(filename);
  if (!Strm) return false;

  if (Strm->TotalSize() < 4) {
    delete Strm;
    Strm = nullptr;
    return false;
  }

  /*
  byte Hdr[4];
  Strm->Serialise(Hdr, 4);
  if (!memcmp(Hdr, MUSMAGIC, 4))
  {
    // convert mus to mid with a wonderfull function
    // thanks to S.Bacquet for the source of qmus2mid
    Strm->Seek(0);
    VMemoryStream *MidStrm = new VMemoryStream();
    MidStrm->BeginWrite();
    VQMus2Mid Conv;
    int MidLength = Conv.Run(*Strm, *MidStrm);
    delete Strm;
    Strm = nullptr;
    if (!MidLength)
    {
      delete MidStrm;
      MidStrm = nullptr;
      return;
    }
    MidStrm->Seek(0);
    MidStrm->BeginRead();
    Strm = MidStrm;
  }
  */

  //  Try to create audio codec.
  VAudioCodec *Codec = nullptr;
  for (FAudioCodecDesc *Desc = FAudioCodecDesc::List; Desc && !Codec; Desc = Desc->Next) {
    //GCon->Logf(va("Using %s to open the stream", Desc->Description));
    Codec = Desc->Creator(Strm);
  }

  if (StreamMusicPlayer && Codec) {
    // start playing streamed music
    StreamMusicPlayer->Play(Codec, filename, Loop);
    StreamPlaying = true;
    return true;
  } else {
    delete Strm;
    Strm = nullptr;
    return false;
  }
}


//==========================================================================
//
//  VAudio::IsMusicPlaying
//
//==========================================================================
bool VAudio::IsMusicPlaying () {
  if (StreamPlaying && StreamMusicPlayer) return StreamMusicPlayer->IsPlaying();
  return false;
}


//==========================================================================
//
//  VAudio::PauseMusic
//
//==========================================================================
void VAudio::PauseMusic () {
  if (StreamPlaying && StreamMusicPlayer) StreamMusicPlayer->Pause();
}


//==========================================================================
//
//  VAudio::ResumeMusic
//
//==========================================================================
void VAudio::ResumeMusic () {
  if (StreamPlaying && StreamMusicPlayer) StreamMusicPlayer->Resume();
}


//==========================================================================
//
//  VAudio::StopMusic
//
//==========================================================================
void VAudio::StopMusic () {
  if (StreamPlaying && StreamMusicPlayer) StreamMusicPlayer->Stop();
}


//==========================================================================
//
//  VAudio::SetMusicPitch
//
//==========================================================================
void VAudio::SetMusicPitch (float pitch) {
  if (StreamPlaying && StreamMusicPlayer) StreamMusicPlayer->SetPitch(pitch);
}


//==========================================================================
//
//  FSoundDeviceDesc::FSoundDeviceDesc
//
//==========================================================================
FSoundDeviceDesc::FSoundDeviceDesc (int Type, const char *AName, const char *ADescription, const char *ACmdLineArg, VSoundDevice *(*ACreator)())
  : Name(AName)
  , Description(ADescription)
  , CmdLineArg(ACmdLineArg)
  , Creator(ACreator)
{
  SoundDeviceList[Type] = this;
}
