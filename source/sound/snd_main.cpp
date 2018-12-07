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
#include "gamedefs.h"
#include "cl_local.h"
#include "snd_local.h"


// ////////////////////////////////////////////////////////////////////////// //
class VSoundSeqNode {
public:
  vint32 Sequence;
  vint32 *SequencePtr;
  vint32 OriginId;
  TVec Origin;
  vint32 CurrentSoundID;
  float DelayTime;
  float Volume;
  float Attenuation;
  vint32 StopSound;
  vuint32 DidDelayOnce;
  TArray<vint32> SeqChoices;
  vint32 ModeNum;
  VSoundSeqNode *Prev;
  VSoundSeqNode *Next;
  VSoundSeqNode *ParentSeq;
  VSoundSeqNode *ChildSeq;

  VSoundSeqNode (int, const TVec &, int, int);
  ~VSoundSeqNode ();
  void Update (float);
  void Serialise (VStream &);
};


// ////////////////////////////////////////////////////////////////////////// //
// main audio management class
class VAudio : public VAudioPublic {
public:
  // sound sequence list
  int ActiveSequences;
  VSoundSeqNode *SequenceListHead;

  VAudio ();
  virtual ~VAudio () override;

  // top level methods
  virtual void Init () override;
  virtual void Shutdown () override;

  // playback of sound effects
  virtual void PlaySound (int InSoundId, const TVec &origin, const TVec &velocity,
                        int origin_id, int channel, float volume, float Attenuation, bool Loop) override;
  virtual void StopSound (int origin_id, int channel) override;
  virtual void StopAllSound () override;
  virtual bool IsSoundPlaying (int origin_id, int InSoundId) override;

  // music and general sound control
  virtual void StartSong (VName song, bool loop) override;
  virtual void PauseSound () override;
  virtual void ResumeSound () override;
  virtual void Start () override;
  virtual void MusicChanged () override;
  virtual void UpdateSounds () override;

  // sound sequences
  virtual void StartSequence (int OriginId, const TVec &Origin, VName Name, int ModeNum) override;
  virtual void AddSeqChoice (int OriginId, VName Name) override;
  virtual void StopSequence (int origin_id) override;
  virtual void UpdateActiveSequences (float DeltaTime) override;
  virtual void StopAllSequences () override;
  virtual void SerialiseSounds (VStream &Strm) override;

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
    float Attenuation;
    int handle;
    bool is3D;
    bool LocalPlayerSound;
    bool Loop;
  };

  // sound curve
  int MaxSoundDist;

  // map's music lump and CD track
  VName MapSong;
  float MusicVolumeFactor;

  // stream music player
  bool MusicEnabled;
  bool StreamPlaying;
  VStreamMusicPlayer *StreamMusicPlayer;

  // list of currently playing sounds
  FChannel Channel[MAX_CHANNELS];
  int NumChannels;
  int ChanUsed;
  vuint32 *ChanBitmap; // used bitmap

  // maximum volume for sound
  float MaxVolume;

  // listener orientation
  TVec ListenerForward;
  TVec ListenerRight;
  TVec ListenerUp;

  // hardware devices
  VOpenALDevice *SoundDevice;

  // console variables
  static VCvarF snd_sfx_volume;
  static VCvarF snd_music_volume;
  //static VCvarB snd_swap_stereo;
  static VCvarI snd_channels;
  static VCvarB snd_external_music;

  // friends
  friend class TCmdMusic;

  // sound effect helpers
  int GetChannel (int, int, int, int);
  void StopChannel (int cidx, bool freeit);
  void UpdateSfx ();

  // music playback
  void StartMusic ();
  void PlaySong (const char *, bool);

  // execution of console commands
  void CmdMusic (const TArray<VStr>&);

  // don't shutdown, just reset
  void ResetAllChannels ();
  int AllocChannel (); // -1: no more
  void DeallocChannel (int cidx);

  inline int ChanFirstUsed () const { return ChanNextUsed(-1, true); }

  inline int ChanNextUsed (int cidx, bool wantFirst=false) const {
    if (!wantFirst && cidx < 0) return -1;
    ++cidx;
    while (cidx < NumChannels) {
      const int bidx = cidx/32;
      const vuint32 mask = 0xffffffffu>>(cidx%32);
      const vuint32 cbv = ChanBitmap[bidx];
      if (cbv&mask) {
        // has some used channels
        for (;;) {
          if (cbv&(0x80000000u>>(cidx%32))) return cidx;
          ++cidx;
        }
      }
      cidx = (cidx|0x1f)+1;
    }
    return -1;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
FAudioCodecDesc *FAudioCodecDesc::List;
VAudioPublic *GAudio;

VCvarF VAudio::snd_sfx_volume("snd_sfx_volume", "0.5", "Sound effects volume.", CVAR_Archive);
VCvarF VAudio::snd_music_volume("snd_music_volume", "0.5", "Music volume", CVAR_Archive);
//VCvarB VAudio::snd_swap_stereo("snd_swap_stereo", false, "Swap stereo channels?", CVAR_Archive);
VCvarI VAudio::snd_channels("snd_channels", "128", "Number of sound channels.", CVAR_Archive);
VCvarB VAudio::snd_external_music("snd_external_music", true, "Allow external music remapping?", CVAR_Archive);

static VCvarF snd_random_pitch("snd_random_pitch", "0.27", "Random pitch all sounds (0: none, otherwise max change).", CVAR_Archive);
static VCvarF snd_random_pitch_boost("snd_random_pitch_boost", "1", "Random pitch will be multiplied by this value.", CVAR_Archive);

VCvarI snd_mid_player("snd_mid_player", "0", "MIDI player type", CVAR_Archive);
VCvarI snd_mod_player("snd_mod_player", "2", "Module player type", CVAR_Archive);


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
VAudio::VAudio ()
  : MaxSoundDist(4096)
  , MapSong(NAME_None)
  , MusicEnabled(true)
  , StreamPlaying(false)
  , StreamMusicPlayer(nullptr)
  , NumChannels(0)
  , ChanUsed(0)
  , MaxVolume(0)
  , SoundDevice(nullptr)
{
  ActiveSequences = 0;
  SequenceListHead = nullptr;
  ChanBitmap = (vuint32 *)Z_Calloc(((MAX_CHANNELS+31)/32)*4);
  ResetAllChannels();
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
//  VAudio::ResetAllChannels
//
//==========================================================================
void VAudio::ResetAllChannels () {
  memset(Channel, 0, sizeof(Channel));
  ChanUsed = 0;
  for (int f = 0; f < MAX_CHANNELS; ++f) Channel[f].handle = -1;
  memset(ChanBitmap, 0, ((MAX_CHANNELS+31)/32)*4);
}


//==========================================================================
//
//  VAudio::AllocChannel
//
//  -1: no more
//
//==========================================================================
int VAudio::AllocChannel () {
  if (ChanUsed >= NumChannels) return -1;
  int cidx = -1;
  for (int bidx = 0; bidx < (MAX_CHANNELS+31)/32; ++bidx) {
    vuint32 cbv = ChanBitmap[bidx];
    if (!cbv) {
      ChanBitmap[bidx] |= 0x80000000u;
      cidx = bidx*32;
      break;
    }
    // has some free channels?
    if (cbv != 0xffffffffu) {
      vuint32 mask = 0x80000000u;
      cidx = bidx*32;
      while (mask) {
        if ((cbv&mask) == 0) {
          ChanBitmap[bidx] |= mask;
          break;
        }
        ++cidx;
        mask >>= 1;
      }
      check(mask);
      break;
    }
  }
  check(cidx >= 0);
  ++ChanUsed;
  //memset((void *)&Channel[cidx], 0, sizeof(FChannel));
  //Channel[cidx].handle = -1;
  return cidx;
}


//==========================================================================
//
//  VAudio::DeallocChannel
//
//==========================================================================
void VAudio::DeallocChannel (int cidx) {
  if (ChanUsed == 0) return; // wtf?!
  if (cidx < 0 || cidx >= MAX_CHANNELS) return; // oops
  const int bidx = cidx/32;
  const vuint32 mask = 0x80000000u>>(cidx%32);
  const vuint32 cbv = ChanBitmap[bidx];
  if (cbv&mask) {
    // allocated channel, free it
    ChanBitmap[bidx] ^= mask;
    --ChanUsed;
  }
}


//==========================================================================
//
//  VAudio::Init
//
//  initialises sound stuff, including volume
//  sets channels, SFX and music volume, allocates channel buffer
//
//==========================================================================
void VAudio::Init () {
  guard(VAudio::Init);

  // initialise sound driver
  if (!GArgs.CheckParm("-nosound") && !GArgs.CheckParm("-nosfx")) {
    SoundDevice = new VOpenALDevice();
    if (!SoundDevice->Init()) {
      delete SoundDevice;
      SoundDevice = nullptr;
    }
  }

  // initialise stream music player
  if (SoundDevice && !GArgs.CheckParm("-nomusic")) {
    StreamMusicPlayer = new VStreamMusicPlayer(SoundDevice);
    StreamMusicPlayer->Init();
  }

  MaxSoundDist = 4096;
  MaxVolume = -1;

  // free all channels for use
  ResetAllChannels();
  NumChannels = (SoundDevice ? SoundDevice->SetChannels(snd_channels) : 0);
  unguard;
}


//==========================================================================
//
//  VAudio::Shutdown
//
//  Shuts down all sound stuff
//
//==========================================================================
void VAudio::Shutdown () {
  guard(VAudio::Shutdown);
  // stop playback of all sounds
  StopAllSequences();
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
  unguard;
}


//==========================================================================
//
//  VAudio::PlaySound
//
//  this function adds a sound to the list of currently active sounds, which
//  is maintained as a given number of internal channels
//
//  channel 0 is "CHAN_AUTO"
//
//==========================================================================
void VAudio::PlaySound (int InSoundId, const TVec &origin, const TVec &velocity,
                        int origin_id, int channel, float volume, float Attenuation, bool Loop)
{
  guard(VAudio::PlaySound);
  if (!SoundDevice || !InSoundId || !MaxVolume || !volume || NumChannels < 1) return;

  // find actual sound ID to use
  int sound_id = GSoundManager->ResolveSound(InSoundId);

  if (sound_id < 0 || sound_id >= GSoundManager->S_sfx.length()) return; // k8: just in case
  if (GSoundManager->S_sfx[sound_id].VolumeAmp <= 0) return; // nothing to see here, come along

  // if it's a looping sound and it's still playing, then continue playing the existing one
  for (int i = ChanFirstUsed(); i >= 0; i = ChanNextUsed(i)) {
    if (Channel[i].origin_id == origin_id && Channel[i].channel == channel &&
        Channel[i].sound_id == sound_id && Channel[i].Loop)
    {
      return;
    }
  }

  // apply sound volume
  volume *= MaxVolume;
  // apply $volume
  volume *= GSoundManager->S_sfx[sound_id].VolumeAmp;
  if (volume <= 0) return; // nothing to see here, come along

  // check if this sound is emited by the local player
  bool LocalPlayerSound = (origin_id == -666 || origin_id == 0 || (cl && cl->MO && cl->MO->SoundOriginID == origin_id));

  // calculate the distance before other stuff so that we can throw out
  // sounds that are beyond the hearing range
  int dist = 0;
  if (origin_id && !LocalPlayerSound && Attenuation > 0 && cl) dist = (int)(Length(origin-cl->ViewOrg)*Attenuation);
  //GCon->Logf("DISTANCE=%d", dist);
  if (dist >= MaxSoundDist) {
    //GCon->Logf("  too far away (%d)", MaxSoundDist);
    return; // sound is beyond the hearing range...
  }

  int priority = GSoundManager->S_sfx[sound_id].Priority*(PRIORITY_MAX_ADJUST-PRIORITY_MAX_ADJUST*dist/MaxSoundDist);

  int chan = GetChannel(sound_id, origin_id, channel, priority);
  if (chan == -1) return; // no free channels

  if (developer) {
    static int checked = -1;
    if (checked < 0) checked = (GArgs.CheckParm("-debug-sound") ? 1 : 0);
    if (checked > 0) GCon->Logf(NAME_Dev, "PlaySound: sound(%d)='%s'; origin_id=%d; channel=%d; chan=%d", sound_id, *GSoundManager->S_sfx[sound_id].TagName, origin_id, channel, chan);
  }

  float pitch = 1.0f;
  if (GSoundManager->S_sfx[sound_id].ChangePitch) {
    pitch = 1.0f+(RandomFull()-RandomFull())*(GSoundManager->S_sfx[sound_id].ChangePitch*snd_random_pitch_boost);
    //fprintf(stderr, "SND0: randompitched to %f\n", pitch);
  } else if (!LocalPlayerSound) {
    float rpt = snd_random_pitch;
    if (rpt > 0) {
      if (rpt > 1) rpt = 1;
      pitch = 1.0f+(RandomFull()-RandomFull())*(rpt*snd_random_pitch_boost);
      //fprintf(stderr, "SND1: randompitched to %f\n", pitch);
    }
  }

  int handle;
  bool is3D;
  if (!origin_id || LocalPlayerSound || Attenuation <= 0) {
    // local sound
    handle = SoundDevice->PlaySound(sound_id, volume, pitch, Loop);
    is3D = false;
  } else {
    handle = SoundDevice->PlaySound3D(sound_id, origin, velocity, volume, pitch, Loop);
    is3D = true;
  }
  Channel[chan].origin_id = origin_id;
  Channel[chan].channel = channel;
  Channel[chan].origin = origin;
  Channel[chan].velocity = velocity;
  Channel[chan].sound_id = sound_id;
  Channel[chan].priority = priority;
  Channel[chan].volume = volume;
  Channel[chan].Attenuation = Attenuation;
  Channel[chan].handle = handle;
  Channel[chan].is3D = is3D;
  Channel[chan].LocalPlayerSound = LocalPlayerSound;
  Channel[chan].Loop = Loop;
  unguard;
}


//==========================================================================
//
//  VAudio::GetChannel
//
//  channel 0 is "CHAN_AUTO"
//
//==========================================================================
int VAudio::GetChannel (int sound_id, int origin_id, int channel, int priority) {
  guard(VAudio::GetChannel);
  int lp; // least priority
  int found;
  int prior;
  int numchannels = GSoundManager->S_sfx[sound_id].NumChannels;

  // first, look if we want to replace sound on some channel
  if (channel != 0) {
    for (int i = ChanFirstUsed(); i >= 0; i = ChanNextUsed(i)) {
      if (Channel[i].origin_id == origin_id && Channel[i].channel == channel) {
        StopChannel(i, false); // don't deallocate
        return i;
      }
    }
  }

  if (numchannels > 0) {
    lp = -1; // denote the argument sound_id
    found = 0;
    prior = priority;
    for (int i = ChanFirstUsed(); i >= 0; i = ChanNextUsed(i)) {
      if (Channel[i].sound_id == sound_id) {
        if (GSoundManager->S_sfx[sound_id].bSingular) {
          // this sound is already playing, so don't start it again.
          return -1;
        }
        ++found; // found one; now, should we replace it?
        if (prior >= Channel[i].priority) {
          // if we're gonna kill one, then this will be it
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
      StopChannel(lp, false); // don't deallocate
      return lp;
    }
  }

  //  Mobjs can have only one sound
  /*
  if (origin_id && channel) {
    for (int i = ChanFirstUsed(); i >= 0; i = ChanNextUsed(i)) {
      if (Channel[i].origin_id == origin_id && Channel[i].channel == channel) {
        // only allow other mobjs one sound
        StopChannel(i);
        return i;
      }
    }
  }
  */

  // get a free channel, if there is any
  if (ChanUsed < NumChannels) return AllocChannel();
  if (NumChannels < 1) return -1;

  // look for a lower priority sound to replace
  for (int i = ChanFirstUsed(); i >= 0; i = ChanNextUsed(i)) {
    if (priority >= Channel[i].priority) {
      // replace the lower priority sound
      StopChannel(i, false); // don't deallocate
      return i;
    }
  }

  // no free channels
  return -1;
  unguard;
}


//==========================================================================
//
//  VAudio::StopChannel
//
//==========================================================================
void VAudio::StopChannel (int cidx, bool freeit) {
  guard(VAudio::StopChannel);
  if (cidx < 0 || cidx >= NumChannels) return;
  if (Channel[cidx].sound_id) {
    SoundDevice->StopChannel(Channel[cidx].handle);
    Channel[cidx].handle = -1;
    Channel[cidx].origin_id = 0;
    Channel[cidx].sound_id = 0;
  }
  if (freeit) DeallocChannel(cidx);
  unguard;
}


//==========================================================================
//
//  VAudio::StopSound
//
//==========================================================================
void VAudio::StopSound (int origin_id, int channel) {
  guard(VAudio::StopSound);
  for (int i = ChanFirstUsed(); i >= 0; i = ChanNextUsed(i)) {
    if (Channel[i].origin_id == origin_id && (!channel || Channel[i].channel == channel)) {
      StopChannel(i, true); // deallocate
    }
  }
  unguard;
}


//==========================================================================
//
//  VAudio::StopAllSound
//
//==========================================================================
void VAudio::StopAllSound () {
  guard(VAudio::StopAllSound);
  // stop all sounds
  for (int i = ChanFirstUsed(); i >= 0; i = ChanNextUsed(i)) StopChannel(i, false);
  ResetAllChannels();
  unguard;
}


//==========================================================================
//
//  VAudio::IsSoundPlaying
//
//==========================================================================
bool VAudio::IsSoundPlaying (int origin_id, int InSoundId) {
  guard(VAudio::IsSoundPlaying);
  int sound_id = GSoundManager->ResolveSound(InSoundId);
  for (int i = ChanFirstUsed(); i >= 0; i = ChanNextUsed(i)) {
    if (Channel[i].sound_id == sound_id &&
        Channel[i].origin_id == origin_id &&
        SoundDevice->IsChannelPlaying(Channel[i].handle))
    {
      return true;
    }
  }
  return false;
  unguard;
}


//==========================================================================
//
//  VAudio::StartSequence
//
//==========================================================================
void VAudio::StartSequence (int OriginId, const TVec &Origin, VName Name, int ModeNum) {
  guard(VAudio::StartSequence);
  int Idx = GSoundManager->FindSequence(Name);
  if (Idx != -1) {
    StopSequence(OriginId); // stop any previous sequence
    new VSoundSeqNode(OriginId, Origin, Idx, ModeNum);
  }
  unguard;
}


//==========================================================================
//
//  VAudio::AddSeqChoice
//
//==========================================================================
void VAudio::AddSeqChoice (int OriginId, VName Name) {
  guard(VAudio::AddSeqChoice);
  int Idx = GSoundManager->FindSequence(Name);
  if (Idx == -1) return;
  for (VSoundSeqNode *node = SequenceListHead; node; node = node->Next) {
    if (node->OriginId == OriginId) {
      node->SeqChoices.Append(Idx);
      return;
    }
  }
  unguard;
}


//==========================================================================
//
//  VAudio::StopSequence
//
//==========================================================================
void VAudio::StopSequence (int origin_id) {
  guard(VAudio::StopSequence);
  VSoundSeqNode *node = SequenceListHead;
  while (node) {
    VSoundSeqNode *next = node->Next;
    if (node->OriginId == origin_id) delete node; // this should exclude node from list
    node = next;
  }
  unguard;
}


//==========================================================================
//
//  VAudio::UpdateActiveSequences
//
//==========================================================================
void VAudio::UpdateActiveSequences (float DeltaTime) {
  guard(VAudio::UpdateActiveSequences);
  if (!ActiveSequences || GGameInfo->IsPaused() || !cl) {
    // no sequences currently playing/game is paused or there's no player in the map
    return;
  }
  //k8: no simple loop, 'cause sequence can delete itself
  VSoundSeqNode *node = SequenceListHead;
  while (node) {
    VSoundSeqNode *next = node->Next;
    node->Update(DeltaTime);
    node = next;
  }
  unguard;
}


//==========================================================================
//
//  VAudio::StopAllSequences
//
//==========================================================================
void VAudio::StopAllSequences () {
  guard(VAudio::StopAllSequences);
  //k8: no simple loop
  VSoundSeqNode *node = SequenceListHead;
  while (node) {
    VSoundSeqNode *next = node->Next;
    node->StopSound = 0; // don't play any stop sounds
    delete node;
    node = next;
  }
  unguard;
}


//==========================================================================
//
//  VAudio::SerialiseSounds
//
//==========================================================================
void VAudio::SerialiseSounds (VStream &Strm) {
  guard(VAudio::SerialiseSounds);
  if (Strm.IsLoading()) {
    // reload and restart all sound sequences
    vint32 numSequences = Streamer<vint32>(Strm);
    for (int i = 0; i < numSequences; ++i) {
      new VSoundSeqNode(0, TVec(0, 0, 0), -1, 0);
    }
    VSoundSeqNode *node = SequenceListHead;
    for (int i = 0; i < numSequences; i++, node = node->Next) {
      node->Serialise(Strm);
    }
  } else {
    // save the sound sequences
    Strm << ActiveSequences;
    for (VSoundSeqNode *node = SequenceListHead; node; node = node->Next) {
      node->Serialise(Strm);
    }
  }
  unguard;
}


//==========================================================================
//
//  VAudio::UpdateSfx
//
//  update the sound parameters
//  used to control volume and pan changes such as when a player turns
//
//==========================================================================
void VAudio::UpdateSfx () {
  guard(VAudio::UpdateSfx);
  if (!SoundDevice || NumChannels <= 0) return;

  if (snd_sfx_volume != MaxVolume) {
    MaxVolume = snd_sfx_volume;
    if (!MaxVolume) StopAllSound();
  }

  if (!MaxVolume) return; // silence

  if (cl) AngleVectors(cl->ViewAngles, ListenerForward, ListenerRight, ListenerUp);

  for (int i = ChanFirstUsed(); i >= 0; i = ChanNextUsed(i)) {
    // active channel?
    if (!Channel[i].sound_id) {
      DeallocChannel(i);
      continue;
    }

    // still playing?
    if (!SoundDevice->IsChannelPlaying(Channel[i].handle)) {
      StopChannel(i, true); // deallocate it
      continue;
    }

    // full volume sound?
    if (!Channel[i].origin_id || Channel[i].Attenuation <= 0) continue;

    // client sound?
    if (Channel[i].LocalPlayerSound) continue;

    // move sound
    Channel[i].origin += Channel[i].velocity*host_frametime;

    if (!cl) continue;

    int dist = (int)(Length(Channel[i].origin-cl->ViewOrg)*Channel[i].Attenuation);
    if (dist >= MaxSoundDist) {
      // too far away
      StopChannel(i, true); // deallocate
      continue;
    }

    // update params
    if (Channel[i].is3D) SoundDevice->UpdateChannel3D(Channel[i].handle, Channel[i].origin, Channel[i].velocity);
    Channel[i].priority = GSoundManager->S_sfx[Channel[i].sound_id].Priority*(PRIORITY_MAX_ADJUST-PRIORITY_MAX_ADJUST*dist/MaxSoundDist);
  }

  if (cl) {
    SoundDevice->UpdateListener(cl->ViewOrg, TVec(0, 0, 0),
      ListenerForward, ListenerRight, ListenerUp
#if defined(VAVOOM_REVERB)
      , GSoundManager->FindEnvironment(cl->SoundEnvironment)
#endif
      );
  }

  //SoundDevice->Tick(host_frametime);
  unguard;
}


//==========================================================================
//
//  VAudio::StartSong
//
//==========================================================================
void VAudio::StartSong (VName song, bool loop) {
  guard(VAudio::StartSong);
  if (loop) {
    GCmdBuf << "Music Loop " << *VStr(*song).quote() << "\n";
  } else {
    GCmdBuf << "Music Play " << *VStr(*song).quote() << "\n";
  }
  unguard;
}


//==========================================================================
//
//  VAudio::PauseSound
//
//==========================================================================
void VAudio::PauseSound () {
  guard(VAudio::PauseSound);
  GCmdBuf << "Music Pause\n";
  unguard;
}


//==========================================================================
//
//  VAudio::ResumeSound
//
//==========================================================================
void VAudio::ResumeSound () {
  guard(VAudio::ResumeSound);
  GCmdBuf << "Music resume\n";
  unguard;
}


//==========================================================================
//
//  VAudio::StartMusic
//
//==========================================================================
void VAudio::StartMusic () {
  StartSong(MapSong, true);
}


//==========================================================================
//
//  VAudio::Start
//
//  per level startup code
//  kills playing sounds at start of level, determines music if any,
//  changes music
//
//==========================================================================
void VAudio::Start () {
  guard(VAudio::Start);
  StopAllSequences();
  StopAllSound();
  unguard;
}


//==========================================================================
//
//  VAudio::MusicChanged
//
//==========================================================================
void VAudio::MusicChanged () {
  guard(VAudio::MusicChanged);
  MapSong = GClLevel->LevelInfo->SongLump;
  StartMusic();
  unguard;
}


//==========================================================================
//
//  VAudio::UpdateSounds
//
//  Updates music & sounds
//
//==========================================================================
void VAudio::UpdateSounds () {
  guard(VAudio::UpdateSounds);

  // check sound volume
  if (snd_sfx_volume < 0.0f) snd_sfx_volume = 0.0f;
  if (snd_sfx_volume > 1.0f) snd_sfx_volume = 1.0f;

  // check music volume
  if (snd_music_volume < 0.0f) snd_music_volume = 0.0f;
  if (snd_music_volume > 1.0f) snd_music_volume = 1.0f;

  // update any Sequences
  UpdateActiveSequences(host_frametime);

  UpdateSfx();
  if (StreamMusicPlayer) {
    SoundDevice->SetStreamVolume(snd_music_volume*MusicVolumeFactor);
    //StreamMusicPlayer->Tick(host_frametime);
  }
  unguard;
}


//==========================================================================
//
//  VAudio::PlaySong
//
//==========================================================================
void VAudio::PlaySong (const char *Song, bool Loop) {
  guard(VAudio::PlaySong);
  static const char *Exts[] = {
    "ogg", "opus", "flac", "mp3", "wav",
    "mid", "mus",
    "mod", "xm", "it", "s3m", "stm",
    //"669", "amf", "dsm", "far", "gdm", "imf", "it", "m15", "med", "mod", "mtm",
    //"okt", "s3m", "stm", "stx", "ult", "uni", "xm", "flac", "ay", "gbs",
    //"gym", "hes", "kss", "nsf", "nsfe", "sap", "sgc", "spc", "vgm",
    nullptr
  };
  static const char *ExtraExts[] = { "ogg", "opus", "flac", "mp3", nullptr };

  if (!Song || !Song[0]) return;

  if (StreamPlaying) StreamMusicPlayer->Stop();
  StreamPlaying = false;

  // get music volume for this song
  MusicVolumeFactor = GSoundManager->GetMusicVolume(Song);
  if (StreamMusicPlayer) SoundDevice->SetStreamVolume(snd_music_volume * MusicVolumeFactor);

  // find the song
  int Lump = -1;
  if (snd_external_music) {
    // check external music definition file
    //TODO: cache this!
    VStream *XmlStrm = FL_OpenFileRead("extras/music/remap.xml");
    if (XmlStrm) {
      VXmlDocument *Doc = new VXmlDocument();
      Doc->Parse(*XmlStrm, "extras/music/remap.xml");
      delete XmlStrm;
      XmlStrm = nullptr;
      for (VXmlNode *N = Doc->Root.FirstChild; N; N = N->NextSibling) {
        if (N->Name != "song") continue;
        if (N->GetAttribute("name") != Song) continue;
        Lump = W_CheckNumForFileName(N->GetAttribute("file"));
        if (Lump >= 0) break;
      }
      delete Doc;
    }
    // also try OGG or MP3 directly
    if (Lump < 0) Lump = W_FindLumpByFileNameWithExts(va("extras/music/%s", Song), ExtraExts);
  }

  if (Lump < 0) {
    Lump = W_FindLumpByFileNameWithExts(va("music/%s", Song), Exts);
    if (Lump < 0) Lump = W_CheckNumForFileName(Song);
    if (Lump < 0) Lump = W_CheckNumForName(VName(Song, VName::AddLower8), WADNS_Music);
    //if (Lump >= 0) GCon->Logf("loaded music file '%s'", *W_FullLumpName(Lump));
  }

  if (Lump < 0) {
    GCon->Logf("Can't find song \"%s\"", Song);
    return;
  }

  VStream *Strm = W_CreateLumpReaderNum(Lump);
  if (Strm->TotalSize() < 4) {
    delete Strm;
    Strm = nullptr;
    return;
  }

  byte Hdr[4];
  Strm->Serialise(Hdr, 4);
  if (!memcmp(Hdr, MUSMAGIC, 4)) {
    // convert mus to mid with a wonderfull function
    // thanks to S.Bacquet for the source of qmus2mid
    Strm->Seek(0);
    VMemoryStream *MidStrm = new VMemoryStream();
    MidStrm->BeginWrite();
    VQMus2Mid Conv;
    int MidLength = Conv.Run(*Strm, *MidStrm);
    delete Strm;
    if (!MidLength) {
      delete MidStrm;
      return;
    }
    MidStrm->Seek(0);
    MidStrm->BeginRead();
    Strm = MidStrm;
  }

  // try to create audio codec
  VAudioCodec *Codec = nullptr;
  for (FAudioCodecDesc *Desc = FAudioCodecDesc::List; Desc && !Codec; Desc = Desc->Next) {
    //GCon->Logf(va("Using %s to open the stream", Desc->Description));
    Codec = Desc->Creator(Strm);
  }

  if (StreamMusicPlayer && Codec) {
    // start playing streamed music
    StreamMusicPlayer->Play(Codec, Song, Loop);
    StreamPlaying = true;
  } else {
    delete Strm;
  }
  unguard;
}


//==========================================================================
//
//  VAudio::CmdMusic
//
//==========================================================================
void VAudio::CmdMusic (const TArray<VStr> &Args) {
  guard(VAudio::CmdMusic);
  if (!StreamMusicPlayer) return;

  if (Args.Num() < 2) return;

  VStr command = Args[1];

  if (command.ICmp("on") == 0) {
    MusicEnabled = true;
    return;
  }

  if (command.ICmp("off") == 0) {
    if (StreamMusicPlayer) StreamMusicPlayer->Stop();
    MusicEnabled = false;
    return;
  }

  if (!MusicEnabled) return;

  if (command.ICmp("play") == 0) {
    if (Args.Num() < 3) {
      GCon->Log("Please enter name of the song.");
      return;
    }
    PlaySong(*Args[2].ToLower(), false);
    return;
  }

  if (command.ICmp("loop") == 0) {
    if (Args.Num() < 3) {
      GCon->Log("Please enter name of the song.");
      return;
    }
    PlaySong(*Args[2].ToLower(), true);
    return;
  }

  if (command.ICmp("pause") == 0) {
    if (StreamPlaying) StreamMusicPlayer->Pause();
    return;
  }

  if (command.ICmp("resume") == 0) {
    if (StreamPlaying) StreamMusicPlayer->Resume();
    return;
  }

  if (command.ICmp("stop") == 0) {
    if (StreamPlaying) StreamMusicPlayer->Stop();
    return;
  }

  if (command.ICmp("info") == 0) {
    if (StreamPlaying && StreamMusicPlayer->IsPlaying()) {
      GCon->Logf("Currently %s %s.", (StreamMusicPlayer->CurrLoop ? "looping" : "playing"), *StreamMusicPlayer->CurrSong);
    } else {
      GCon->Log("No song currently playing");
    }
    return;
  }
  unguard;
}


//==========================================================================
//
//  VSoundSeqNode::VSoundSeqNode
//
//==========================================================================
VSoundSeqNode::VSoundSeqNode (int AOriginId, const TVec &AOrigin, int ASequence, int AModeNum)
  : Sequence(ASequence)
  , OriginId(AOriginId)
  , Origin(AOrigin)
  , CurrentSoundID(0)
  , DelayTime(0.0f)
  , Volume(1.0f) // Start at max volume
  , Attenuation(1.0f)
  , DidDelayOnce(0)
  , ModeNum(AModeNum)
  , Prev(nullptr)
  , Next(nullptr)
  , ParentSeq(nullptr)
  , ChildSeq(nullptr)
{
  if (Sequence >= 0) {
    SequencePtr = GSoundManager->SeqInfo[Sequence].Data;
    StopSound = GSoundManager->SeqInfo[Sequence].StopSound;
  }
  // add to the list of sound sequences
  if (!((VAudio *)GAudio)->SequenceListHead) {
    ((VAudio *)GAudio)->SequenceListHead = this;
  } else {
    ((VAudio *)GAudio)->SequenceListHead->Prev = this;
    Next = ((VAudio *)GAudio)->SequenceListHead;
    ((VAudio *)GAudio)->SequenceListHead = this;
  }
  ++((VAudio *)GAudio)->ActiveSequences;
}


//==========================================================================
//
//  VSoundSeqNode::~VSoundSeqNode
//
//==========================================================================
VSoundSeqNode::~VSoundSeqNode () {
  if (ParentSeq && ParentSeq->ChildSeq == this) {
    // re-activate parent sequence
    ++ParentSeq->SequencePtr;
    ParentSeq->ChildSeq = nullptr;
    ParentSeq = nullptr;
  }

  if (ChildSeq) {
    delete ChildSeq;
    ChildSeq = nullptr;
  }

  // play stop sound
  if (StopSound >= 0) ((VAudio *)GAudio)->StopSound(OriginId, 0);
  if (StopSound >= 1) ((VAudio *)GAudio)->PlaySound(StopSound, Origin, TVec(0, 0, 0), OriginId, 1, Volume, Attenuation, false);

  // remove from the list of active sound sequences
  if (((VAudio*)GAudio)->SequenceListHead == this) ((VAudio *)GAudio)->SequenceListHead = Next;
  if (Prev) Prev->Next = Next;
  if (Next) Next->Prev = Prev;

  --((VAudio *)GAudio)->ActiveSequences;
}


//==========================================================================
//
//  VSoundSeqNode::Update
//
//==========================================================================
void VSoundSeqNode::Update (float DeltaTime) {
  guard(VSoundSeqNode::Update);
  if (DelayTime) {
    DelayTime -= DeltaTime;
    if (DelayTime <= 0.0f) DelayTime = 0.0f;
    return;
  }

  bool sndPlaying = GAudio->IsSoundPlaying(OriginId, CurrentSoundID);
  switch (*SequencePtr) {
    case SSCMD_None:
      ++SequencePtr;
      break;
    case SSCMD_Play:
      if (!sndPlaying) {
        CurrentSoundID = SequencePtr[1];
        GAudio->PlaySound(CurrentSoundID, Origin, TVec(0, 0, 0), OriginId, 1, Volume, Attenuation, false);
      }
      SequencePtr += 2;
      break;
    case SSCMD_WaitUntilDone:
      if (!sndPlaying) {
        ++SequencePtr;
        CurrentSoundID = 0;
      }
      break;
    case SSCMD_PlayRepeat:
      if (!sndPlaying) {
        CurrentSoundID = SequencePtr[1];
        GAudio->PlaySound(CurrentSoundID, Origin, TVec(0, 0, 0), OriginId, 1, Volume, Attenuation, false);
      }
      break;
    case SSCMD_PlayLoop:
      CurrentSoundID = SequencePtr[1];
      GAudio->PlaySound(CurrentSoundID, Origin, TVec(0, 0, 0), OriginId, 1, Volume, Attenuation, false);
      DelayTime = SequencePtr[2]/35.0f;
      break;
    case SSCMD_Delay:
      DelayTime = SequencePtr[1]/35.0f;
      SequencePtr += 2;
      CurrentSoundID = 0;
      break;
    case SSCMD_DelayOnce:
      if (!(DidDelayOnce&(1<<SequencePtr[2]))) {
        DidDelayOnce |= 1<<SequencePtr[2];
        DelayTime = SequencePtr[1]/35.0f;
        CurrentSoundID = 0;
      }
      SequencePtr += 3;
      break;
    case SSCMD_DelayRand:
      DelayTime = (SequencePtr[1]+rand()%(SequencePtr[2]-SequencePtr[1]))/35.0f;
      SequencePtr += 3;
      CurrentSoundID = 0;
      break;
    case SSCMD_Volume:
      Volume = SequencePtr[1]/10000.0f;
      SequencePtr += 2;
      break;
    case SSCMD_VolumeRel:
      Volume += SequencePtr[1]/10000.0f;
      SequencePtr += 2;
      break;
    case SSCMD_VolumeRand:
      Volume = (SequencePtr[1]+rand()%(SequencePtr[2]-SequencePtr[1]))/10000.0f;
      SequencePtr += 3;
      break;
    case SSCMD_Attenuation:
      Attenuation = SequencePtr[1];
      SequencePtr += 2;
      break;
    case SSCMD_RandomSequence:
      if (SeqChoices.Num() == 0) {
        ++SequencePtr;
      } else if (!ChildSeq) {
        int Choice = rand()%SeqChoices.Num();
        ChildSeq = new VSoundSeqNode(OriginId, Origin, SeqChoices[Choice], ModeNum);
        ChildSeq->ParentSeq = this;
        ChildSeq->Volume = Volume;
        ChildSeq->Attenuation = Attenuation;
        return;
      } else {
        // waiting for child sequence to finish
        return;
      }
      break;
    case SSCMD_Branch:
      SequencePtr -= SequencePtr[1];
      break;
    case SSCMD_Select:
      {
        // transfer sequence to the one matching the ModeNum
        int NumChoices = SequencePtr[1];
        int i;
        for (i = 0; i < NumChoices; ++i) {
          if (SequencePtr[2+i*2] == ModeNum) {
            int Idx = GSoundManager->FindSequence(*(VName *)&SequencePtr[3+i*2]);
            if (Idx != -1) {
              Sequence = Idx;
              SequencePtr = GSoundManager->SeqInfo[Sequence].Data;
              StopSound = GSoundManager->SeqInfo[Sequence].StopSound;
              break;
            }
          }
        }
        if (i == NumChoices) SequencePtr += 2+NumChoices; // not found
      }
      break;
    case SSCMD_StopSound:
      // wait until something else stops the sequence
      break;
    case SSCMD_End:
      delete this;
      break;
    default:
      break;
  }
  unguard;
}


//==========================================================================
//
//  VSoundSeqNode::Serialise
//
//==========================================================================
void VSoundSeqNode::Serialise (VStream &Strm) {
  guard(VSoundSeqNode::Serialise);
  vuint8 xver = 0; // current version is 0
  Strm << xver;
  Strm << STRM_INDEX(Sequence)
    << STRM_INDEX(OriginId)
    << Origin
    << STRM_INDEX(CurrentSoundID)
    << DelayTime
    << STRM_INDEX(DidDelayOnce)
    << Volume
    << Attenuation
    << STRM_INDEX(ModeNum);

  if (Strm.IsLoading()) {
    vint32 Offset;
    Strm << STRM_INDEX(Offset);
    SequencePtr = GSoundManager->SeqInfo[Sequence].Data+Offset;
    StopSound = GSoundManager->SeqInfo[Sequence].StopSound;

    vint32 Count;
    Strm << STRM_INDEX(Count);
    for (int i = 0; i < Count; ++i) {
      VName SeqName;
      Strm << SeqName;
      SeqChoices.Append(GSoundManager->FindSequence(SeqName));
    }

    vint32 ParentSeqIdx;
    vint32 ChildSeqIdx;
    Strm << STRM_INDEX(ParentSeqIdx) << STRM_INDEX(ChildSeqIdx);
    if (ParentSeqIdx != -1 || ChildSeqIdx != -1) {
      int i = 0;
      for (VSoundSeqNode *n = ((VAudio*)GAudio)->SequenceListHead; n; n = n->Next, ++i) {
        if (ParentSeqIdx == i) ParentSeq = n;
        if (ChildSeqIdx == i) ChildSeq = n;
      }
    }
  } else {
    vint32 Offset = SequencePtr - GSoundManager->SeqInfo[Sequence].Data;
    Strm << STRM_INDEX(Offset);

    vint32 Count = SeqChoices.Num();
    Strm << STRM_INDEX(Count);
    for (int i = 0; i < SeqChoices.Num(); ++i) Strm << GSoundManager->SeqInfo[SeqChoices[i]].Name;

    vint32 ParentSeqIdx = -1;
    vint32 ChildSeqIdx = -1;
    if (ParentSeq || ChildSeq) {
      int i = 0;
      for (VSoundSeqNode *n = ((VAudio*)GAudio)->SequenceListHead; n; n = n->Next, ++i) {
        if (ParentSeq == n) ParentSeqIdx = i;
        if (ChildSeq == n) ChildSeqIdx = i;
      }
    }
    Strm << STRM_INDEX(ParentSeqIdx) << STRM_INDEX(ChildSeqIdx);
  }
  unguard;
}


//==========================================================================
//
//  COMMAND Music
//
//==========================================================================
COMMAND(Music) {
  guard(COMMAND Music);
  if (GAudio) ((VAudio *)GAudio)->CmdMusic(Args);
  unguard;
}


//==========================================================================
//
//  COMMAND CD
//
//==========================================================================
COMMAND(CD) {
}
