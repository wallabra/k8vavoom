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

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"
#include "cl_local.h"
#include "snd_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

class VSoundSeqNode
{
public:
  vint32      Sequence;
  vint32 *SequencePtr;
  vint32      OriginId;
  TVec      Origin;
  vint32      CurrentSoundID;
  float     DelayTime;
  float     Volume;
  float     Attenuation;
  vint32      StopSound;
  vuint32     DidDelayOnce;
  TArray<vint32>  SeqChoices;
  vint32      ModeNum;
  VSoundSeqNode *Prev;
  VSoundSeqNode *Next;
  VSoundSeqNode *ParentSeq;
  VSoundSeqNode *ChildSeq;

  VSoundSeqNode(int, const TVec&, int, int);
  ~VSoundSeqNode();
  void Update(float);
  void Serialise(VStream&);
};

//
//  VAudio
//
//  Main audio management class.
//
class VAudio : public VAudioPublic
{
public:
  //  Sound sequence list
  int         ActiveSequences;
  VSoundSeqNode *SequenceListHead;

  //  Structors.
  VAudio();
  ~VAudio();

  //  Top level methods.
  void Init();
  void Shutdown();

  //  Playback of sound effects
  void PlaySound(int, const TVec&, const TVec&, int, int, float, float,
    bool);
  void StopSound(int, int);
  void StopAllSound();
  bool IsSoundPlaying(int, int);

  //  Music and general sound control
  void StartSong(VName, int, bool);
  void PauseSound();
  void ResumeSound();
  void Start();
  void MusicChanged();
  void UpdateSounds();

  //  Sound sequences
  void StartSequence(int, const TVec&, VName, int);
  void AddSeqChoice(int, VName);
  void StopSequence(int);
  void UpdateActiveSequences(float);
  void StopAllSequences();
  void SerialiseSounds(VStream&);

private:
  enum { MAX_CHANNELS = 256 };

  enum { PRIORITY_MAX_ADJUST = 10 };

  //  Info about sounds currently playing.
  struct FChannel
  {
    int     origin_id;
    int     channel;
    TVec    origin;
    TVec    velocity;
    int     sound_id;
    int     priority;
    float   volume;
    float   Attenuation;
    int     handle;
    bool    is3D;
    bool    LocalPlayerSound;
    bool    Loop;
  };

  //  Sound curve
  int         MaxSoundDist;

  //  Map's music lump and CD track
  VName       MapSong;
  int         MapCDTrack;
  float       MusicVolumeFactor;

  //  Stream music player
  bool        MusicEnabled;
  bool        StreamPlaying;
  VStreamMusicPlayer *StreamMusicPlayer;

  //  List of currently playing sounds
  FChannel      Channel[MAX_CHANNELS];
  int         NumChannels;
  int         SndCount;

  // maximum volume for sound
  float       MaxVolume;

  //  Listener orientation
  TVec        ListenerForward;
  TVec        ListenerRight;
  TVec        ListenerUp;

  //  Hardware devices
  VOpenALDevice *SoundDevice;

  //  Console variables
  static VCvarF   snd_sfx_volume;
  static VCvarF   snd_music_volume;
  static VCvarB   snd_swap_stereo;
  static VCvarI   snd_channels;
  static VCvarB   snd_external_music;

  //  Friends
  friend class TCmdMusic;
  friend class TCmdCD;

  //  Sound effect helpers
  int GetChannel(int, int, int, int);
  void StopChannel(int);
  void UpdateSfx();

  //  Music playback
  void StartMusic();
  void PlaySong(const char*, bool);

  //  Execution of console commands
  void CmdMusic(const TArray<VStr>&);
  void CmdCD(const TArray<VStr>&);
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

VAudioPublic *GAudio;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

VCvarF        VAudio::snd_sfx_volume("snd_sfx_volume", "0.5", "Sound effects volume.", CVAR_Archive);
VCvarF        VAudio::snd_music_volume("snd_music_volume", "0.5", "Music volume", CVAR_Archive);
VCvarB        VAudio::snd_swap_stereo("snd_swap_stereo", false, "Swap stereo channels?", CVAR_Archive);
VCvarI        VAudio::snd_channels("snd_channels", "128", "Number of sound channels.", CVAR_Archive);
VCvarB        VAudio::snd_external_music("snd_external_music", true, "Allow external music remapping?", CVAR_Archive);

//  Public CVars
#if defined(DJGPP) || defined(_WIN32)
VCvarB        snd_mid_player("snd_mid_player", false, "Allow MIDI?", CVAR_Archive);
#else
VCvarB        snd_mid_player("snd_mid_player", true, "Allow MIDI?", CVAR_Archive);
#endif
VCvarB        snd_mod_player("snd_mod_player", true, "Allow music modules?", CVAR_Archive);

FAudioCodecDesc *FAudioCodecDesc::List;


//==========================================================================
//
//  VAudioPublic::Create
//
//==========================================================================

VAudioPublic *VAudioPublic::Create()
{
  return new VAudio();
}

//==========================================================================
//
//  VAudio::VAudio
//
//==========================================================================

VAudio::VAudio()
: MaxSoundDist(4096)
, MapSong(NAME_None)
, MapCDTrack(0)
, MusicEnabled(true)
, StreamPlaying(false)
, StreamMusicPlayer(nullptr)
, NumChannels(0)
, SndCount(0)
, MaxVolume(0)
, SoundDevice(nullptr)
{
  ActiveSequences = 0;
  SequenceListHead = nullptr;
  memset(Channel, 0, sizeof(Channel));
}

//==========================================================================
//
//  VAudio::~VAudio
//
//==========================================================================

VAudio::~VAudio()
{
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
  guard(VAudio::Init);

  // initialise sound driver
  if (!GArgs.CheckParm("-nosound") && !GArgs.CheckParm("-nosfx")) {
    SoundDevice = new VOpenALDevice();
    if (!SoundDevice->Init()) {
      delete SoundDevice;
      SoundDevice = nullptr;
    }
  }

  //  Initialise stream music player.
  if (SoundDevice && !GArgs.CheckParm("-nomusic")) {
    StreamMusicPlayer = new VStreamMusicPlayer(SoundDevice);
    StreamMusicPlayer->Init();
  }

  MaxSoundDist = 4096;
  MaxVolume = -1;

  //  Free all channels for use.
  memset(Channel, 0, sizeof(Channel));
  NumChannels = SoundDevice ? SoundDevice->SetChannels(snd_channels) : 0;
  unguard;
}

//==========================================================================
//
//  VAudio::Shutdown
//
//  Shuts down all sound stuff
//
//==========================================================================

void VAudio::Shutdown()
{
  guard(VAudio::Shutdown);
  //  Stop playback of all sounds.
  StopAllSequences();
  StopAllSound();

  if (StreamMusicPlayer)
  {
    StreamMusicPlayer->Shutdown();
    delete StreamMusicPlayer;
    StreamMusicPlayer = nullptr;
  }
  if (SoundDevice)
  {
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
//  This function adds a sound to the list of currently active sounds, which
// is maintained as a given number of internal channels.
//
//==========================================================================

void VAudio::PlaySound(int InSoundId, const TVec &origin,
  const TVec &velocity, int origin_id, int channel, float volume,
  float Attenuation, bool Loop)
{
  guard(VAudio::PlaySound);
  if (!SoundDevice || !InSoundId || !MaxVolume || !volume)
  {
    return;
  }

  //  Find actual sound ID to use.
  int sound_id = GSoundManager->ResolveSound(InSoundId);

  if (sound_id < 0 || sound_id >= GSoundManager->S_sfx.length()) return; // k8: just in case
  if (GSoundManager->S_sfx[sound_id].VolumeAmp <= 0) return; // nothing to see here, come along

  //  If it's a looping sound and it's still playing, then continue
  // playing the existing one.
  for (int i = 0; i < NumChannels; i++)
  {
    if (Channel[i].origin_id == origin_id &&
      Channel[i].channel == channel &&
      Channel[i].sound_id == sound_id && Channel[i].Loop)
    {
      return;
    }
  }

  //  Apply sound volume.
  volume *= MaxVolume;

  // apply $volume
  volume *= GSoundManager->S_sfx[sound_id].VolumeAmp;
  if (volume <= 0) return; // nothing to see here, come along

  //  Check if this sound is emited by the local player.
  bool LocalPlayerSound = (origin_id == -666 || origin_id == 0 || (cl && cl->MO && cl->MO->SoundOriginID == origin_id));

  // calculate the distance before other stuff so that we can throw out
  // sounds that are beyond the hearing range.
  int dist = 0;
  if (origin_id && !LocalPlayerSound && Attenuation > 0 && cl)
  {
    dist = (int)(Length(origin - cl->ViewOrg) * Attenuation);
  }
  //GCon->Logf("DISTANCE=%d", dist);
  if (dist >= MaxSoundDist)
  {
    //GCon->Logf("  too far away (%d)", MaxSoundDist);
    return; // sound is beyond the hearing range...
  }

  int priority = GSoundManager->S_sfx[sound_id].Priority *
    (PRIORITY_MAX_ADJUST - PRIORITY_MAX_ADJUST * dist / MaxSoundDist);

  int chan = GetChannel(sound_id, origin_id, channel, priority);
  if (chan == -1)
  {
    return; //no free channels.
  }

  float pitch = 1.0;
  if (GSoundManager->S_sfx[sound_id].ChangePitch)
  {
    pitch = 1.0 + (Random() - Random()) *
      GSoundManager->S_sfx[sound_id].ChangePitch;
  }

  int handle;
  bool is3D;
  if (!origin_id || LocalPlayerSound || Attenuation <= 0)
  {
    //  Local sound
    handle = SoundDevice->PlaySound(sound_id, volume, 0, pitch, Loop);
    is3D = false;
  }
  else
  {
    handle = SoundDevice->PlaySound3D(sound_id, origin, velocity,
      volume, pitch, Loop);
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
//==========================================================================

int VAudio::GetChannel(int sound_id, int origin_id, int channel, int priority)
{
  guard(VAudio::GetChannel);
  int     chan;
  int     i;
  int     lp; //least priority
  int     found;
  int     prior;
  int numchannels = GSoundManager->S_sfx[sound_id].NumChannels;

  if (numchannels > 0)
  {
    lp = -1; //denote the argument sound_id
    found = 0;
    prior = priority;
    for (i = 0; i < NumChannels; i++)
    {
      if (Channel[i].sound_id == sound_id)
      {
        if (GSoundManager->S_sfx[sound_id].bSingular)
        {
          // This sound is already playing, so don't start it again.
          return -1;
        }
        found++; //found one.  Now, should we replace it??
        if (prior >= Channel[i].priority)
        {
          // if we're gonna kill one, then this'll be it
          lp = i;
          prior = Channel[i].priority;
        }
      }
    }

    if (found >= numchannels)
    {
      if (lp == -1)
      {// other sounds have greater priority
        return -1; // don't replace any sounds
      }
      StopChannel(lp);
    }
  }

  //  Mobjs can have only one sound
/*  if (origin_id && channel)
    {
    for (i = 0; i < NumChannels; i++)
    {
      if (Channel[i].origin_id == origin_id &&
        Channel[i].channel == channel)
      {
        // only allow other mobjs one sound
        StopChannel(i);
        return i;
      }
    }
  }*/

  //  Look for a free channel
  for (i = 0; i < NumChannels; i++)
  {
    if (!Channel[i].sound_id)
    {
      return i;
    }
  }

  //  Look for a lower priority sound to replace.
  SndCount++;
  if (SndCount >= NumChannels)
  {
    SndCount = 0;
  }

  for (chan = 0; chan < NumChannels; chan++)
  {
    i = (SndCount + chan) % NumChannels;
    if (priority >= Channel[i].priority)
    {
      //replace the lower priority sound.
      StopChannel(i);
      return i;
    }
  }

    //  no free channels.
  return -1;
  unguard;
}

//==========================================================================
//
//  VAudio::StopChannel
//
//==========================================================================

void VAudio::StopChannel(int chan_num)
{
  guard(VAudio::StopChannel);
  if (Channel[chan_num].sound_id)
  {
    SoundDevice->StopChannel(Channel[chan_num].handle);
    Channel[chan_num].handle = -1;
    Channel[chan_num].origin_id = 0;
    Channel[chan_num].sound_id = 0;
  }
  unguard;
}

//==========================================================================
//
//  VAudio::StopSound
//
//==========================================================================

void VAudio::StopSound(int origin_id, int channel)
{
  guard(VAudio::StopSound);
  for (int i = 0; i < NumChannels; i++)
  {
    if (Channel[i].origin_id == origin_id &&
      (!channel || Channel[i].channel == channel))
    {
      StopChannel(i);
    }
  }
  unguard;
}

//==========================================================================
//
//  VAudio::StopAllSound
//
//==========================================================================

void VAudio::StopAllSound()
{
  guard(VAudio::StopAllSound);
  //  stop all sounds
  for (int i = 0; i < NumChannels; i++)
  {
    StopChannel(i);
  }
  unguard;
}

//==========================================================================
//
//  VAudio::IsSoundPlaying
//
//==========================================================================

bool VAudio::IsSoundPlaying(int origin_id, int InSoundId)
{
  guard(VAudio::IsSoundPlaying);
  int sound_id = GSoundManager->ResolveSound(InSoundId);
  for (int i = 0; i < NumChannels; i++)
  {
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

void VAudio::StartSequence(int OriginId, const TVec &Origin, VName Name,
  int ModeNum)
{
  guard(VAudio::StartSequence);
  int Idx = GSoundManager->FindSequence(Name);
  if (Idx != -1)
  {
    StopSequence(OriginId); // Stop any previous sequence
    new VSoundSeqNode(OriginId, Origin, Idx, ModeNum);
  }
  unguard;
}

//==========================================================================
//
//  VAudio::AddSeqChoice
//
//==========================================================================

void VAudio::AddSeqChoice(int OriginId, VName Name)
{
  guard(VAudio::AddSeqChoice);
  int Idx = GSoundManager->FindSequence(Name);
  if (Idx == -1)
  {
    return;
  }
  for (VSoundSeqNode *node = SequenceListHead; node; node = node->Next)
  {
    if (node->OriginId == OriginId)
    {
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

void VAudio::StopSequence(int origin_id)
{
  guard(VAudio::StopSequence);
  for (VSoundSeqNode *node = SequenceListHead; node; node = node->Next)
  {
    if (node->OriginId == origin_id)
    {
      delete node;
    }
  }
  unguard;
}

//==========================================================================
//
//  VAudio::UpdateActiveSequences
//
//==========================================================================

void VAudio::UpdateActiveSequences(float DeltaTime)
{
  guard(VAudio::UpdateActiveSequences);
  if (!ActiveSequences || GGameInfo->IsPaused() || !cl)
  {
    //  No sequences currently playing/game is paused
    //  or there's no player in the map
    return;
  }
  for (VSoundSeqNode *node = SequenceListHead; node; node = node->Next)
  {
    node->Update(DeltaTime);
  }
  unguard;
}

//==========================================================================
//
//  VAudio::StopAllSequences
//
//==========================================================================

void VAudio::StopAllSequences()
{
  guard(VAudio::StopAllSequences);
  for (VSoundSeqNode *node = SequenceListHead; node; node = node->Next)
  {
    node->StopSound = 0; // don't play any stop sounds
    delete node;
  }
  unguard;
}

//==========================================================================
//
//  VAudio::SerialiseSounds
//
//==========================================================================

void VAudio::SerialiseSounds(VStream &Strm)
{
  guard(VAudio::SerialiseSounds);
  if (Strm.IsLoading())
  {
    // Reload and restart all sound sequences
    vint32 numSequences = Streamer<vint32>(Strm);
    for (int i = 0; i < numSequences; i++)
    {
      new VSoundSeqNode(0, TVec(0, 0, 0), -1, 0);
    }
    VSoundSeqNode *node = SequenceListHead;
    for (int i = 0; i < numSequences; i++, node = node->Next)
    {
      node->Serialise(Strm);
    }
  }
  else
  {
    // Save the sound sequences
    Strm << ActiveSequences;
    for (VSoundSeqNode *node = SequenceListHead; node; node = node->Next)
    {
      node->Serialise(Strm);
    }
  }
  unguard;
}

//==========================================================================
//
//  VAudio::UpdateSfx
//
//  Update the sound parameters. Used to control volume and pan
// changes such as when a player turns.
//
//==========================================================================

void VAudio::UpdateSfx()
{
  guard(VAudio::UpdateSfx);
  if (!SoundDevice || !NumChannels)
  {
    return;
  }

  if (snd_sfx_volume != MaxVolume)
    {
      MaxVolume = snd_sfx_volume;
    if (!MaxVolume)
    {
      StopAllSound();
    }
    }

  if (!MaxVolume)
  {
    //  Silence
    return;
  }

  if (cl)
  {
    AngleVectors(cl->ViewAngles, ListenerForward, ListenerRight, ListenerUp);
  }

  for (int i = 0; i < NumChannels; i++)
  {
    if (!Channel[i].sound_id)
    {
      //  Nothing on this channel
      continue;
    }
    if (!SoundDevice->IsChannelPlaying(Channel[i].handle))
    {
      //  Playback done
      StopChannel(i);
      continue;
    }
    if (!Channel[i].origin_id || Channel[i].Attenuation <= 0)
    {
      //  Full volume sound
      continue;
    }

    if (Channel[i].LocalPlayerSound)
    {
      //  Client sound
      continue;
    }

    //  Move sound
    Channel[i].origin += Channel[i].velocity * host_frametime;

    if (!cl)
    {
      continue;
    }

    int dist = (int)(Length(Channel[i].origin - cl->ViewOrg) *
      Channel[i].Attenuation);
    if (dist >= MaxSoundDist)
    {
      //  Too far away
      StopChannel(i);
      continue;
    }

    //  Update params
    if (Channel[i].is3D)
    {
      SoundDevice->UpdateChannel3D(Channel[i].handle,
        Channel[i].origin, Channel[i].velocity);
    }
    Channel[i].priority = GSoundManager->S_sfx[Channel[i].sound_id].Priority *
      (PRIORITY_MAX_ADJUST - PRIORITY_MAX_ADJUST * dist / MaxSoundDist);
  }

  if (cl)
  {
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

void VAudio::StartSong(VName song, int track, bool loop)
{
  guard(VAudio::StartSong);
  if (loop)
    GCmdBuf << "Music Loop " << *song << "\n";
  else
    GCmdBuf << "Music Play " << *song << "\n";
  unguard;
}

//==========================================================================
//
//  VAudio::PauseSound
//
//==========================================================================

void VAudio::PauseSound()
{
  guard(VAudio::PauseSound);
  GCmdBuf << "Music Pause\n";
  unguard;
}

//==========================================================================
//
//  VAudio::ResumeSound
//
//==========================================================================

void VAudio::ResumeSound()
{
  guard(VAudio::ResumeSound);
  GCmdBuf << "Music resume\n";
  unguard;
}

//==========================================================================
//
//  VAudio::StartMusic
//
//==========================================================================

void VAudio::StartMusic()
{
  StartSong(MapSong, MapCDTrack, true);
}

//==========================================================================
//
//  VAudio::Start
//
//  Per level startup code. Kills playing sounds at start of level,
// determines music if any, changes music.
//
//==========================================================================

void VAudio::Start()
{
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

void VAudio::MusicChanged()
{
  guard(VAudio::MusicChanged);
  MapSong = GClLevel->LevelInfo->SongLump;
  MapCDTrack = GClLevel->LevelInfo->CDTrack;

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

void VAudio::UpdateSounds()
{
  guard(VAudio::UpdateSounds);

  //  Check sound volume.
  if (snd_sfx_volume < 0.0)
  {
    snd_sfx_volume = 0.0;
  }
  if (snd_sfx_volume > 1.0)
  {
    snd_sfx_volume = 1.0;
  }

  //  Check music volume.
  if (snd_music_volume < 0.0)
  {
    snd_music_volume = 0.0;
  }
  if (snd_music_volume > 1.0)
  {
    snd_music_volume = 1.0;
  }

  // Update any Sequences
  UpdateActiveSequences(host_frametime);

  UpdateSfx();
  if (StreamMusicPlayer)
  {
    SoundDevice->SetStreamVolume(snd_music_volume * MusicVolumeFactor);
    //StreamMusicPlayer->Tick(host_frametime);
  }
  unguard;
}

//==========================================================================
//
//  VAudio::PlaySong
//
//==========================================================================

void VAudio::PlaySong(const char *Song, bool Loop)
{
  guard(VAudio::PlaySong);
  static const char *Exts[] = { "ogg", "mp3", "wav", "mid", "mus", "669",
    "amf", "dsm", "far", "gdm", "imf", "it", "m15", "med", "mod", "mtm",
    "okt", "s3m", "stm", "stx", "ult", "uni", "xm", "flac", "ay", "gbs",
    "gym", "hes", "kss", "nsf", "nsfe", "sap", "sgc", "spc", "vgm", nullptr };
  static const char *ExtraExts[] = { "ogg", "mp3", nullptr };

  if (!Song || !Song[0])
  {
    return;
  }

  if (StreamPlaying)
  {
    StreamMusicPlayer->Stop();
  }
  StreamPlaying = false;

  //  Get music volume for this song.
  MusicVolumeFactor = GSoundManager->GetMusicVolume(Song);
  if (StreamMusicPlayer)
  {
    SoundDevice->SetStreamVolume(snd_music_volume * MusicVolumeFactor);
  }

  //  Find the song.
  int Lump = -1;
  if (snd_external_music)
  {
    //  Check external music definition file.
    VStream *XmlStrm = FL_OpenFileRead("extras/music/remap.xml");
    if (XmlStrm)
    {
      VXmlDocument *Doc = new VXmlDocument();
      Doc->Parse(*XmlStrm, "extras/music/remap.xml");
      delete XmlStrm;
      XmlStrm = nullptr;
      for (VXmlNode *N = Doc->Root.FirstChild; N; N = N->NextSibling)
      {
        if (N->Name != "song")
        {
          continue;
        }
        if (N->GetAttribute("name") != Song)
        {
          continue;
        }
        Lump = W_CheckNumForFileName(N->GetAttribute("file"));
        if (Lump >= 0)
        {
          break;
        }
      }
      delete Doc;
      Doc = nullptr;
    }
    //  Also try OGG or MP3 directly.
    if (Lump < 0)
    {
      Lump = W_FindLumpByFileNameWithExts(va("extras/music/%s", Song),
        ExtraExts);
    }
  }
  if (Lump < 0)
  {
    int FileIdx = W_FindLumpByFileNameWithExts(va("music/%s", Song), Exts);
    int LumpIdx = W_CheckNumForName(VName(Song, VName::AddLower8), WADNS_Music);
    Lump = MAX(FileIdx, LumpIdx);
  }
  if (Lump < 0)
  {
    GCon->Logf("Can't find song %s", Song);
    return;
  }
  VStream *Strm = W_CreateLumpReaderNum(Lump);

  if (Strm->TotalSize() < 4)
  {
    delete Strm;
    Strm = nullptr;
    return;
  }

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

  //  Try to create audio codec.
  VAudioCodec *Codec = nullptr;
  for (FAudioCodecDesc *Desc = FAudioCodecDesc::List; Desc && !Codec; Desc = Desc->Next)
  {
//    GCon->Logf(va("Using %s to open the stream", Desc->Description));
    Codec = Desc->Creator(Strm);
  }

  if (StreamMusicPlayer && Codec)
  {
    //  Start playing streamed music.
    StreamMusicPlayer->Play(Codec, Song, Loop);
    StreamPlaying = true;
  }
  else
  {
    delete Strm;
    Strm = nullptr;
  }
  unguard;
}

//==========================================================================
//
//  VAudio::CmdMusic
//
//==========================================================================

void VAudio::CmdMusic(const TArray<VStr>& Args)
{
  guard(VAudio::CmdMusic);
  if (!StreamMusicPlayer)
  {
    return;
  }

  if (Args.Num() < 2)
  {
    return;
  }

  VStr command = Args[1].ToLower();

  if (command == "on")
  {
    MusicEnabled = true;
    return;
  }

  if (command == "off")
  {
    if (StreamMusicPlayer)
    {
      StreamMusicPlayer->Stop();
    }
    MusicEnabled = false;
    return;
  }

  if (!MusicEnabled)
  {
    return;
  }

  if (command == "play")
  {
    if (Args.Num() < 3)
    {
      GCon->Log("Please enter name of the song.");
      return;
    }
    PlaySong(*Args[2].ToLower(), false);
    return;
  }

  if (command == "loop")
  {
    if (Args.Num() < 3)
    {
      GCon->Log("Please enter name of the song.");
      return;
    }
    PlaySong(*Args[2].ToLower(), true);
    return;
  }

  if (command == "pause")
  {
    if (StreamPlaying)
    {
      StreamMusicPlayer->Pause();
    }
    return;
  }

  if (command == "resume")
  {
    if (StreamPlaying)
    {
      StreamMusicPlayer->Resume();
    }
    return;
  }

  if (command == "stop")
  {
    if (StreamPlaying)
    {
      StreamMusicPlayer->Stop();
    }
    return;
  }

  if (command == "info")
  {
    if (StreamPlaying && StreamMusicPlayer->IsPlaying())
    {
      GCon->Logf("Currently %s %s.", StreamMusicPlayer->CurrLoop ?
        "looping" : "playing", *StreamMusicPlayer->CurrSong);
    }
    else
    {
      GCon->Log("No song currently playing");
    }
    return;
  }
  unguard;
}

//==========================================================================
//
//  VAudio::CmdCD
//
//==========================================================================

void VAudio::CmdCD(const TArray<VStr>& Args)
{
}

//==========================================================================
//
//  VSoundSeqNode::VSoundSeqNode
//
//==========================================================================

VSoundSeqNode::VSoundSeqNode(int AOriginId, const TVec &AOrigin,
  int ASequence, int AModeNum)
: Sequence(ASequence)
, OriginId(AOriginId)
, Origin(AOrigin)
, CurrentSoundID(0)
, DelayTime(0.0)
, Volume(1.0) // Start at max volume
, Attenuation(1.0)
, DidDelayOnce(0)
, ModeNum(AModeNum)
, Prev(nullptr)
, Next(nullptr)
, ParentSeq(nullptr)
, ChildSeq(nullptr)
{
  if (Sequence >= 0)
  {
    SequencePtr = GSoundManager->SeqInfo[Sequence].Data;
    StopSound = GSoundManager->SeqInfo[Sequence].StopSound;
  }

  //  Add to the list of sound sequences.
  if (!((VAudio*)GAudio)->SequenceListHead)
  {
    ((VAudio*)GAudio)->SequenceListHead = this;
  }
  else
  {
    ((VAudio*)GAudio)->SequenceListHead->Prev = this;
    Next = ((VAudio*)GAudio)->SequenceListHead;
    ((VAudio*)GAudio)->SequenceListHead = this;
  }
  ((VAudio*)GAudio)->ActiveSequences++;
}

//==========================================================================
//
//  VSoundSeqNode::~VSoundSeqNode
//
//==========================================================================

VSoundSeqNode::~VSoundSeqNode()
{
  if (ParentSeq && ParentSeq->ChildSeq == this)
  {
    //  Re-activate parent sequence.
    ParentSeq->SequencePtr++;
    ParentSeq->ChildSeq = nullptr;
    ParentSeq = nullptr;
  }

  if (ChildSeq)
  {
    delete ChildSeq;
    ChildSeq = nullptr;
  }

  //  Play stop sound.
  if (StopSound >= 0)
  {
    ((VAudio*)GAudio)->StopSound(OriginId, 0);
  }
  if (StopSound >= 1)
  {
    ((VAudio*)GAudio)->PlaySound(StopSound, Origin, TVec(0, 0, 0),
      OriginId, 1, Volume, Attenuation, false);
  }

  //  Remove from the list of active sound sequences.
  if (((VAudio*)GAudio)->SequenceListHead == this)
  {
    ((VAudio*)GAudio)->SequenceListHead = Next;
  }
  if (Prev)
  {
    Prev->Next = Next;
  }
  if (Next)
  {
    Next->Prev = Prev;
  }
  ((VAudio*)GAudio)->ActiveSequences--;
}

//==========================================================================
//
//  VSoundSeqNode::Update
//
//==========================================================================

void VSoundSeqNode::Update(float DeltaTime)
{
  guard(VSoundSeqNode::Update);
  if (DelayTime)
  {
    DelayTime -= DeltaTime;
    if (DelayTime <= 0.0)
    {
      DelayTime = 0.0;
    }
    return;
  }

  bool sndPlaying = GAudio->IsSoundPlaying(OriginId, CurrentSoundID);
  switch (*SequencePtr)
  {
  case SSCMD_None:
    SequencePtr++;
    break;

  case SSCMD_Play:
    if (!sndPlaying)
    {
      CurrentSoundID = SequencePtr[1];
      GAudio->PlaySound(CurrentSoundID, Origin, TVec(0, 0, 0),
        OriginId, 1, Volume, Attenuation, false);
    }
    SequencePtr += 2;
    break;

  case SSCMD_WaitUntilDone:
    if (!sndPlaying)
    {
      SequencePtr++;
      CurrentSoundID = 0;
    }
    break;

  case SSCMD_PlayRepeat:
    if (!sndPlaying)
    {
      CurrentSoundID = SequencePtr[1];
      GAudio->PlaySound(CurrentSoundID, Origin, TVec(0, 0, 0),
        OriginId, 1, Volume, Attenuation, false);
    }
    break;

  case SSCMD_PlayLoop:
    CurrentSoundID = SequencePtr[1];
    GAudio->PlaySound(CurrentSoundID, Origin, TVec(0, 0, 0), OriginId, 1,
      Volume, Attenuation, false);
    DelayTime = SequencePtr[2] / 35.0;
    break;

  case SSCMD_Delay:
    DelayTime = SequencePtr[1] / 35.0;
    SequencePtr += 2;
    CurrentSoundID = 0;
    break;

  case SSCMD_DelayOnce:
    if (!(DidDelayOnce & (1 << SequencePtr[2])))
    {
      DidDelayOnce |= 1 << SequencePtr[2];
      DelayTime = SequencePtr[1] / 35.0;
      CurrentSoundID = 0;
    }
    SequencePtr += 3;
    break;

  case SSCMD_DelayRand:
    DelayTime = (SequencePtr[1] + rand() % (SequencePtr[2] -
      SequencePtr[1])) / 35.0;
    SequencePtr += 3;
    CurrentSoundID = 0;
    break;

  case SSCMD_Volume:
    Volume = SequencePtr[1] / 10000.0;
    SequencePtr += 2;
    break;

  case SSCMD_VolumeRel:
    Volume += SequencePtr[1] / 10000.0;
    SequencePtr += 2;
    break;

  case SSCMD_VolumeRand:
    Volume = (SequencePtr[1] + rand() % (SequencePtr[2] -
      SequencePtr[1])) / 10000.0;
    SequencePtr += 3;
    break;

  case SSCMD_Attenuation:
    Attenuation = SequencePtr[1];
    SequencePtr += 2;
    break;

  case SSCMD_RandomSequence:
    if (SeqChoices.Num() == 0)
    {
      SequencePtr++;
    }
    else if (!ChildSeq)
    {
      int Choice = rand() % SeqChoices.Num();
      ChildSeq = new VSoundSeqNode(OriginId, Origin, SeqChoices[Choice],
        ModeNum);
      ChildSeq->ParentSeq = this;
      ChildSeq->Volume = Volume;
      ChildSeq->Attenuation = Attenuation;
      return;
    }
    else
    {
      //  Waiting for child sequence to finish.
      return;
    }
    break;

  case SSCMD_Branch:
    SequencePtr -= SequencePtr[1];
    break;

  case SSCMD_Select:
    {
      //  Transfer sequence to the one matching the ModeNum.
      int NumChoices = SequencePtr[1];
      int i;
      for (i = 0; i < NumChoices; i++)
      {
        if (SequencePtr[2 + i * 2] == ModeNum)
        {
          int Idx = GSoundManager->FindSequence(
            *(VName*)&SequencePtr[3 + i * 2]);
          if (Idx != -1)
          {
            Sequence = Idx;
            SequencePtr = GSoundManager->SeqInfo[Sequence].Data;
            StopSound = GSoundManager->SeqInfo[Sequence].StopSound;
            break;
          }
        }
      }
      if (i == NumChoices)
      {
        //  Not found.
        SequencePtr += 2 + NumChoices;
      }
    }
    break;

  case SSCMD_StopSound:
    // Wait until something else stops the sequence
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

void VSoundSeqNode::Serialise(VStream &Strm)
{
  guard(VSoundSeqNode::Serialise);
  Strm << STRM_INDEX(Sequence)
    << STRM_INDEX(OriginId)
    << Origin
    << STRM_INDEX(CurrentSoundID)
    << DelayTime
    << STRM_INDEX(DidDelayOnce)
    << Volume
    << Attenuation
    << STRM_INDEX(ModeNum);

  if (Strm.IsLoading())
  {
    vint32 Offset;
    Strm << STRM_INDEX(Offset);
    SequencePtr = GSoundManager->SeqInfo[Sequence].Data + Offset;
    StopSound = GSoundManager->SeqInfo[Sequence].StopSound;

    vint32 Count;
    Strm << STRM_INDEX(Count);
    for (int i = 0; i < Count; i++)
    {
      VName SeqName;
      Strm << SeqName;
      SeqChoices.Append(GSoundManager->FindSequence(SeqName));
    }

    vint32 ParentSeqIdx;
    vint32 ChildSeqIdx;
    Strm << STRM_INDEX(ParentSeqIdx)
      << STRM_INDEX(ChildSeqIdx);
    if (ParentSeqIdx != -1 || ChildSeqIdx != -1)
    {
      int i = 0;
      for (VSoundSeqNode *n = ((VAudio*)GAudio)->SequenceListHead;
        n; n = n->Next, i++)
      {
        if (ParentSeqIdx == i)
        {
          ParentSeq = n;
        }
        if (ChildSeqIdx == i)
        {
          ChildSeq = n;
        }
      }
    }
  }
  else
  {
    vint32 Offset = SequencePtr - GSoundManager->SeqInfo[Sequence].Data;
    Strm << STRM_INDEX(Offset);

    vint32 Count = SeqChoices.Num();
    Strm << STRM_INDEX(Count);
    for (int i = 0; i < SeqChoices.Num(); i++)
      Strm << GSoundManager->SeqInfo[SeqChoices[i]].Name;

    vint32 ParentSeqIdx = -1;
    vint32 ChildSeqIdx = -1;
    if (ParentSeq || ChildSeq)
    {
      int i = 0;
      for (VSoundSeqNode *n = ((VAudio*)GAudio)->SequenceListHead;
        n; n = n->Next, i++)
      {
        if (ParentSeq == n)
        {
          ParentSeqIdx = i;
        }
        if (ChildSeq == n)
        {
          ChildSeqIdx = i;
        }
      }
    }
    Strm << STRM_INDEX(ParentSeqIdx)
      << STRM_INDEX(ChildSeqIdx);
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
  ((VAudio*)GAudio)->CmdMusic(Args);
  unguard;
}


//==========================================================================
//
//  COMMAND CD
//
//==========================================================================
COMMAND(CD) {
  guard(COMMAND CD);
  ((VAudio*)GAudio)->CmdCD(Args);
  unguard;
}
