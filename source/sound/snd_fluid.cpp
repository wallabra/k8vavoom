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

#include "fluidsynth.h"

extern TArray<VStr> midiSynthAllBanks;

extern void forceMidiBanksScan ();
extern void scanForMidiBanks ();


extern VCvarB snd_timidity_autoload_sf2;
extern VCvarS snd_timidity_sf2_file;


// Values are: 0 = FLUID_INTERP_NONE
//             1 = FLUID_INTERP_LINEAR
//             4 = FLUID_INTERP_4THORDER (the FluidSynth default)
//             7 = FLUID_INTERP_7THORDER
static VCvarI snd_fluid_interp("snd_fluid_interp", "1", "FluidSynth interpolation.", CVAR_Archive);


#define MIDI_CHANNELS   (64)
#define MIDI_MESSAGE    (0x07)
#define MIDI_END        (0x2f)
#define MIDI_SET_TEMPO  (0x51)
#define MIDI_SEQUENCER  (0x7f)


// ////////////////////////////////////////////////////////////////////////// //
class VFluidAudioCodec : public VAudioCodec {
public:
  // midi data definitions
  //
  // these data should not be modified outside the
  // audio thread unless they're being initialized
  struct track_t {
    char header[4];
    vint32 length;
    vuint8 *data;
    vuint8 RunningStatus;
  };

  struct song_t {
    char header[4];
    vuint32 chunksize;
    vuint16 type;
    vuint16 ntracks;
    vuint16 delta;
    //vuint8 *data;
    //vuint32 length;
    track_t *tracks;
    vuint32 tempo;
    double timediv;
  };

  struct chan_t {
    song_t *song;
    track_t *track;
    int id;
    int depth;
    const vuint8 *pos;
    const vuint8 *jump;
    vuint32 tics;
    vuint32 nexttic;
    vuint32 lasttic;
    vuint32 starttic;
    vuint32 starttime;
    vuint32 curtime;
    bool ended;
  };

public:
  vuint8 *MidiData;
  int SongSize;
  vint32 nextFrameTic;

  // these should never be modified unless they're initialized
  song_t midisong;
  chan_t chans[MIDI_CHANNELS];
  int activeChanIdx[MIDI_CHANNELS]; // -1: not active
  int activeChanCount;

public:
  VFluidAudioCodec (vuint8 *InSong, int aSongSize);
  virtual ~VFluidAudioCodec () override;
  virtual int Decode (short *Data, int NumSamples) override;
  virtual bool Finished () override;
  virtual void Restart () override;

  void StopSound ();

public:
  vuint8 getNextMidiByte (chan_t *chan);
  void skipNextMidiBytes (chan_t *chan, int len);
  bool checkTrackEnd (chan_t *chan);
  vuint32 getNextTick (chan_t *chan);
  vuint32 readVarLen (chan_t *chan);

  const vuint8 *getChanExData (chan_t *chan, int &len);

  void runChannel (chan_t *chan, vuint32 msecs);

public:
  static VAudioCodec *Create (VStream *InStrm);
};


// ////////////////////////////////////////////////////////////////////////// //
class FluidManager {
public:
  // library specific stuff. should never
  // be modified after initialization
  static fluid_settings_t *settings;
  static fluid_synth_t *synth;
  static fluid_audio_driver_t *driver;
  static int sf2id;

protected:
  static int fluidInitialised; // <0: not yet; >0: ok; 0: failed

  // last values of some cvars
  static VStr sf2Path;
  static bool autoloadSF2;
  static bool needRestart;

protected:
  static bool NeedRestart ();
  static void UpdateCvarCache ();

public:
  FluidManager () {}
  ~FluidManager () { CloseFluid(); }

  // returns success flag
  static bool InitFluid ();
  // WARNING! song must be freed!
  static void CloseFluid ();

  static double Song_GetTimeDivision (const VFluidAudioCodec::song_t *song);
};

static FluidManager fluidManager;


IMPLEMENT_AUDIO_CODEC(VFluidAudioCodec, "Fluid");


fluid_settings_t *FluidManager::settings = nullptr;
fluid_synth_t *FluidManager::synth = nullptr;
fluid_audio_driver_t *FluidManager::driver = nullptr;
int FluidManager::sf2id = -1;

int FluidManager::fluidInitialised = -1;
VStr FluidManager::sf2Path = VStr::EmptyString;
bool FluidManager::autoloadSF2 = false;
bool FluidManager::needRestart = false;


//==========================================================================
//
//  FluidManager::Song_GetTimeDivision
//
//==========================================================================
double FluidManager::Song_GetTimeDivision (const VFluidAudioCodec::song_t *song) {
  return (double)song->tempo/(double)song->delta/1000.0;
}


//==========================================================================
//
//  VFluidAudioCodec::getNextMidiByte
//
//  gets the next vuint8 in a midi track
//
//==========================================================================
vuint8 VFluidAudioCodec::getNextMidiByte (chan_t *chan) {
  if (chan->ended) return 0;
  if ((ptrdiff_t)(chan->pos-MidiData) >= SongSize) {
    GCon->Log(NAME_Error, "getNextMidiByte: Unexpected end of track");
    chan->ended = true;
    return 0;
  }
  return *chan->pos++;
}


//==========================================================================
//
//  VFluidAudioCodec::skipNextMidiBytes
//
//==========================================================================
void VFluidAudioCodec::skipNextMidiBytes (chan_t *chan, int len) {
  if (len <= 0) return; // just in case
  if (checkTrackEnd(chan)) return;
  int left = (int)(ptrdiff_t)(chan->pos-MidiData);
  if (len > left) {
    chan->ended = true;
  } else {
    chan->pos += len;
  }
}


//==========================================================================
//
//  VFluidAudioCodec::readVarLen
//
//  reads a variable-length SMF number
//
//==========================================================================
vuint32 VFluidAudioCodec::readVarLen (chan_t *chan) {
  vuint32 time = 0, t = 0x80;
  while ((t&0x80) != 0 && !checkTrackEnd(chan)) {
    t = getNextMidiByte(chan);
    time = (time<<7)|(t&127);
  }
  return time;
}


//==========================================================================
//
//  VFluidAudioCodec::getChanExData
//
//==========================================================================
const vuint8 *VFluidAudioCodec::getChanExData (chan_t *chan, int &len) {
  len = 0;
  if (checkTrackEnd(chan)) return nullptr;
  const vuint8 *stpos = chan->pos;
  len = readVarLen(chan);
  if (len < 0 || checkTrackEnd(chan)) { chan->ended = true; len = 0; return nullptr; }
  int left = (int)(ptrdiff_t)(chan->pos-MidiData);
  if (len > left) { chan->ended = true; len = 0; return nullptr; }
  const vuint8 *cdt = chan->pos;
  chan->pos += len;
  len = (int)(ptrdiff_t)(stpos-MidiData);
  return cdt;
}


//==========================================================================
//
//  VFluidAudioCodec::checkTrackEnd
//
//  checks if the midi reader has reached the end
//
//==========================================================================
bool VFluidAudioCodec::checkTrackEnd (chan_t *chan) {
  return (chan->ended || ((ptrdiff_t)(chan->pos-MidiData) >= SongSize));
}


//==========================================================================
//
//  VFluidAudioCodec::Chan_GetNextTick
//
//  read the midi track to get the next delta time
//
//==========================================================================
vuint32 VFluidAudioCodec::getNextTick (chan_t *chan) {
  vuint32 tic = getNextMidiByte(chan);
  //GCon->Logf("FLUID: channel=%d; ntt=%u", (int)(ptrdiff_t)(chan-&chans[0]), tic);
  if (tic&0x80) {
    vuint8 mb;
    tic &= 0x7f;
    // the N64 version loops infinitely but since the
    // delta time can only be four bytes long, just loop
    // for the remaining three bytes..
    for (int i = 0; i < 3; ++i) {
      mb = getNextMidiByte(chan);
      tic = (mb&0x7f)+(tic<<7);
      if (!(mb&0x80)) break;
    }
  }
  return (chan->starttic+(vuint32)((double)tic*chan->song->timediv));
}


/*
#define MIDI_META_TEMPO ((vuint8)0x51)
#define MIDI_META_EOT ((vuint8)0x2F)    // End-of-track
#define MIDI_META_SSPEC ((vuint8)0x7F)    // System-specific event

#define MIDI_NOTEOFF  ((vuint8)0x80)    // + note + velocity
#define MIDI_NOTEON   ((vuint8)0x90)    // + note + velocity
#define MIDI_POLYPRESS  ((vuint8)0xA0)    // + pressure (2 bytes)
#define MIDI_CTRLCHANGE ((vuint8)0xB0)    // + ctrlr + value
#define MIDI_PRGMCHANGE ((vuint8)0xC0)    // + new patch
#define MIDI_CHANPRESS  ((vuint8)0xD0)    // + pressure (1 byte)
#define MIDI_PITCHBEND  ((vuint8)0xE0)    // + pitch bend (2 bytes)
*/

#define MIDI_SYSEX     ((vuint8)0xF0) /* SysEx begin */
#define MIDI_SYSEXEND  ((vuint8)0xF7) /* SysEx end */
#define MIDI_META      ((vuint8)0xFF) /* Meta event begin */


//==========================================================================
//
//  Event_Meta
//
//==========================================================================
static void Event_Meta (VFluidAudioCodec *codec, vuint8 evcode, VFluidAudioCodec::chan_t *chan) {
  // SysEx events could potentially not have enough room in the buffer...
  if (evcode == MIDI_SYSEX || evcode == MIDI_SYSEXEND) {
    int len = 0;
    const vuint8 *cdt = codec->getChanExData(chan, len);
    if (len > 0) {
      fluid_synth_sysex(FluidManager::synth, (const char *)cdt, len, nullptr, nullptr, nullptr, 0);
    }
  } else if (evcode == MIDI_META) {
    char string[256];
    int meta = codec->getNextMidiByte(chan);
    int len = codec->readVarLen(chan);
#ifdef VV_FLUID_DEBUG
    GCon->Logf("META: meta=0x%02x; len=%d", meta, len);
#endif
    switch (meta) {
      // mostly for debugging/logging
      case MIDI_MESSAGE:
        for (int i = 0; i < len; ++i) string[i] = codec->getNextMidiByte(chan);
        string[len] = 0;
        for (int i = 0; i < len; ++i) {
          if (string[i] == '\n' || string[i] == '\r') string[i] = ' ';
          else if (string[i] < 32) string[i] = '_';
        }
        GCon->Logf("Midi message: %s", string);
        len = 0;
        break;
      case MIDI_END:
        codec->skipNextMidiBytes(chan, len);
        len = 0;
        chan->ended = true;
        //!!Chan_RemoveTrackFromPlaylist(seq, chan);
        break;
      case MIDI_SET_TEMPO:
        if (len == 3) {
          vuint32 b0 = codec->getNextMidiByte(chan);
          vuint32 b1 = codec->getNextMidiByte(chan);
          vuint32 b2 = codec->getNextMidiByte(chan);
          chan->song->tempo = (b0<<16)|(b1<<8)|(b2&0xff);
#ifdef VV_FLUID_DEBUG
          GCon->Logf("  tempo: %u", chan->song->tempo);
#endif
          if (chan->song->tempo == 0) return;
          chan->song->timediv = FluidManager::Song_GetTimeDivision(chan->song);
          chan->starttime = chan->curtime;
          len = 0;
        }
        break;
      // game-specific midi event
      case MIDI_SEQUENCER:
        if (len > 0) {
          int b = codec->getNextMidiByte(chan); // manufacturer (should be 0)
          --len;
          if (!b && len > 0) {
            b = codec->getNextMidiByte(chan);
            --len;
            if (b == 0x23) {
              // set jump position
              chan->jump = chan->pos;
            } else if (b == 0x20) {
              if (len >= 2) {
                b = codec->getNextMidiByte(chan);
                b = codec->getNextMidiByte(chan);
                len -= 2;
                // goto jump position
                if (chan->jump) chan->pos = chan->jump;
              }
            }
          }
        }
        break;
      default:
#ifdef VV_FLUID_DEBUG
        GCon->Logf("FLUID: channel=%p; UNKNOWN metacmd=0x%02x; metalen=%d", chan, meta, len);
#endif
        break;
    }
    codec->skipNextMidiBytes(chan, len);
  }
}


//==========================================================================
//
//  VFluidAudioCodec::runChannel
//
//  main midi parsing routine
//
//==========================================================================
void VFluidAudioCodec::runChannel (chan_t *chan, vuint32 msecs) {
  if (chan->ended) return;

  chan->curtime += msecs;
  vuint32 tics = chan->curtime;

  // keep parsing through midi track until
  // the end is reached or until it reaches next delta time
  while (!chan->ended) {
    //GCon->Logf("FLUID: channel=%d; tics=%u; next=%u; diff=%d", (int)(ptrdiff_t)(chan-&chans[0]), tics, chan->nexttic, (int)chan->nexttic-(int)tics);

    // not ready to execute events yet
    if (tics < chan->nexttic) {
#ifdef VV_FLUID_DEBUG
      GCon->Logf("FLUID(!!!): channel=%d; tics=%u; next=%u; diff=%d", (int)(ptrdiff_t)(chan-&chans[0]), tics, chan->nexttic, (int)chan->nexttic-(int)tics);
#endif
      break;
    }

    chan->starttic = chan->nexttic;

    vuint8 evcode = getNextMidiByte(chan);
#ifdef VV_FLUID_DEBUG
    GCon->Logf("FLUID: channel=%d; tics=%u; next=%u; diff=%d; cmd=0x%02x", (int)(ptrdiff_t)(chan-&chans[0]), tics, chan->nexttic, (int)chan->nexttic-(int)tics, (unsigned)evcode);
#endif

    if (evcode != MIDI_SYSEX && evcode != MIDI_META && evcode != MIDI_SYSEXEND) {
      static const vuint8 MIDI_EventLengths[7] = { 2, 2, 2, 2, 1, 1, 2 };
      static const vuint8 MIDI_CommonLengths[15] = { 0, 1, 2, 1, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0 };

      // normal short event
      vuint8 data1 = 0, data2 = 0;
      if ((evcode&0xF0) == 0xF0) {
        if (MIDI_CommonLengths[evcode&15] > 0) {
          data1 = getNextMidiByte(chan);
          if (MIDI_CommonLengths[evcode&15] > 1) {
            data2 = getNextMidiByte(chan);
          }
        }
      } else if ((evcode&0x80) == 0) {
        data1 = evcode;
        evcode = chan->track->RunningStatus;
      } else {
        chan->track->RunningStatus = evcode;
        data1 = getNextMidiByte(chan);
      }

      if (MIDI_EventLengths[(evcode&0x70)>>4] == 2) {
        data2 = getNextMidiByte(chan);
      }

      vuint8 event = (evcode>>4)&0x07;
      vuint8 channel = evcode&0x0f;
      switch (event) {
        case 0: // note off
          fluid_synth_noteoff(FluidManager::synth, channel, data1);
          break;
        case 1: // note on
          //fluid_synth_cc(FluidManager::synth, chan->id, 0x5B/*EFFECTS_DEPTH1*/, chan->depth);
          fluid_synth_noteon(FluidManager::synth, channel, data1, data2);
          break;
        case 2: // poly press
          break;
        case 3: // control change
          fluid_synth_cc(FluidManager::synth, channel, data1, data2);
          break;
        case 4: // program change
          fluid_synth_program_change(FluidManager::synth, channel, data1);
          break;
        case 5: // channel pressure
          fluid_synth_channel_pressure(FluidManager::synth, channel, data1);
          break;
        case 6: // pitch bend
          fluid_synth_pitch_bend(FluidManager::synth, channel, (data1&0x7f)|((data2&0x7f)<<7));
          break;
        case 7: // channel volume (msb)
          fluid_synth_cc(FluidManager::synth, channel, 0x07, data2);
          break;
        case 0x27: // channel volume (lsb)
          fluid_synth_cc(FluidManager::synth, channel, 0x27, data2);
          break;
        default: break;
      }
    } else {
      Event_Meta(this, evcode, chan);
    }

    // check for end of the track, otherwise get
    // the next delta time
    if (!chan->ended) {
      if (checkTrackEnd(chan)) {
        chan->ended = true;
      } else {
        chan->nexttic = getNextTick(chan);
        //GCon->Logf("  NTT=%u", chan->nexttic);
      }
    }
  }
}


//==========================================================================
//
//  FluidManager::NeedRestart
//
//==========================================================================
bool FluidManager::NeedRestart () {
  return
    needRestart ||
    sf2Path != snd_timidity_sf2_file.asStr() ||
    autoloadSF2 != snd_timidity_autoload_sf2.asBool();
}


//==========================================================================
//
//  FluidManager::UpdateCvarCache
//
//==========================================================================
void FluidManager::UpdateCvarCache () {
  sf2Path = snd_timidity_sf2_file.asStr();
  autoloadSF2 = snd_timidity_autoload_sf2.asBool();
  needRestart = false;
}


//==========================================================================
//
//  FluidManager::CloseFluid
//
//==========================================================================
void FluidManager::CloseFluid () {
  if (synth) delete_fluid_synth(synth);
  synth = nullptr;
  if (settings) delete_fluid_settings(settings);
  settings = nullptr;
  sf2id = -1;
  fluidInitialised = -1;
}


extern "C" {
  static void shutTheFuckUpFluid (int level, char *message, void *data) {
    switch (level) {
      case FLUID_PANIC:
        GCon->Logf(NAME_Error, "FluidSynth PANIC: %s", message);
        break;
      case FLUID_ERR:
        GCon->Logf(NAME_Error, "FluidSynth: %s", message);
        break;
      case FLUID_WARN:
        //GCon->Logf(NAME_Warning, "FluidSynth: %s", message);
        break;
      case FLUID_INFO:
        //GCon->Logf("FluidSynth: %s", message);
        break;
      case FLUID_DBG:
        //GCon->Logf("FLUID DEBUG: %s", message);
        break;
    }
  }
}


//==========================================================================
//
//  FluidManager::InitFluid
//
//==========================================================================
bool FluidManager::InitFluid () {
  if (NeedRestart()) CloseFluid();

  if (fluidInitialised >= 0) return (fluidInitialised > 0);

  // shut the fuck up!
  fluid_set_log_function(FLUID_PANIC, &shutTheFuckUpFluid, nullptr);
  fluid_set_log_function(FLUID_ERR, &shutTheFuckUpFluid, nullptr);
  fluid_set_log_function(FLUID_WARN, &shutTheFuckUpFluid, nullptr);
  fluid_set_log_function(FLUID_INFO, &shutTheFuckUpFluid, nullptr);
  fluid_set_log_function(FLUID_DBG, &shutTheFuckUpFluid, nullptr);

  check(sf2id < 0);

  if (autoloadSF2 != snd_timidity_autoload_sf2.asBool()) forceMidiBanksScan();

  UpdateCvarCache();

  // alloc settings
  if (!settings) settings = new_fluid_settings();
  fluid_settings_setint(settings, "synth.midi-channels", 0x10+MIDI_CHANNELS);
  fluid_settings_setint(settings, "synth.polyphony", 128);
  fluid_settings_setnum(settings, "synth.sample-rate", 44100);
  fluid_settings_setint(settings, "synth.cpu-cores", 2);
  fluid_settings_setint(settings, "synth.chorus.active", 0);

  fluid_settings_setint(settings, "synth.interpolation", snd_fluid_interp.asInt());


  // init synth
  if (!synth) synth = new_fluid_synth(settings);
  if (!synth) {
    GCon->Log(NAME_Warning, "FluidSynth: failed to create synthesizer");
    fluidInitialised = 0;
    return false;
  }

  fluid_synth_set_interp_method(synth, -1, snd_fluid_interp.asInt());
  fluid_synth_set_gain(synth, 1.0f);
  fluid_synth_set_reverb(synth, 0.61f/*size*/, 0.23f/*damp*/, 0.76f/*width*/, 0.57f/*level*/);
  fluid_synth_set_reverb_on(synth, 0);


  scanForMidiBanks();

  if (snd_timidity_autoload_sf2) {
    TArray<VStr> failedBanks;
    // try to load a bank
    for (auto &&bfn : midiSynthAllBanks) {
      VStr sf2name = bfn;
      if (sf2name.isEmpty()) continue;
      sf2id = fluid_synth_sfload(synth, *sf2name, 1);
      if (sf2id != FLUID_FAILED) {
        GCon->Logf("FluidSynth: autoloaded SF2: '%s'", *sf2name);
        break;
      }
      // oops
      sf2id = -1;
      failedBanks.append(sf2name);
    }
  }

  if (sf2id < 0) {
    GCon->Log(NAME_Warning, "FluidSynth: failed to find SF2");
    fluidInitialised = 0;
    return false;
  }

  /*
    vol = (int)((chan->volume * this->soundvolume) / 127.0f);
    pan = chan->pan;

    fluid_synth_cc(this->synth, chan->id, 0x07, vol);
    fluid_synth_cc(this->synth, chan->id, 0x0A, pan);
  */

  fluidInitialised = 1;
  return true;
}


//==========================================================================
//
//  VFluidAudioCodec::VFluidAudioCodec
//
//==========================================================================
VFluidAudioCodec::VFluidAudioCodec (vuint8 *InSong, int aSongSize)
  : MidiData(InSong)
  , SongSize(aSongSize)
  , nextFrameTic(0)
  , activeChanCount(0)
{
  memcpy(&midisong.header[0], InSong, 4); InSong += 4;
  memcpy(&midisong.chunksize, InSong, 4); InSong += 4;
  memcpy(&midisong.type, InSong, 2); InSong += 2;
  memcpy(&midisong.ntracks, InSong, 2); InSong += 2;
  memcpy(&midisong.delta, InSong, 2); InSong += 2;

  midisong.chunksize = BigLong(midisong.chunksize);
  midisong.ntracks = BigShort(midisong.ntracks);
  midisong.delta = BigShort(midisong.delta);
  midisong.type = BigShort(midisong.type);
  midisong.tempo = 480000;
  midisong.timediv = FluidManager::Song_GetTimeDivision(&midisong);

  if (midisong.ntracks > MIDI_CHANNELS) midisong.ntracks = MIDI_CHANNELS;

  // register tracks
  midisong.tracks = (track_t *)Z_Calloc(sizeof(track_t)*midisong.ntracks);
  vuint8 *data = InSong; //song.data+0x0e;
  for (int i = 0; i < midisong.ntracks; ++i) {
    track_t *track = &midisong.tracks[i];
    //memcpy(track, data, 8);
    memcpy(track->header, data, 4);
    if (memcmp(track->header, "MTrk", 4) != 0) {
      midisong.ntracks = i;
      break;
    }
    memcpy(&track->length, data+4, 4);
    data = data+8;

    track->length = BigLong(track->length);
    track->data = data;

#ifdef VV_FLUID_DEBUG
    GCon->Logf("  track #%d: %u bytes", i, track->length);
#endif

    track->RunningStatus = 0;

    data += track->length;
  }

  if (midisong.ntracks == 0) {
    //pos = MidiData+SongSize;
    //ended = true;
    for (int f = 0; f < MIDI_CHANNELS; ++f) chans[f].ended = true;
    return;
  }

#ifdef VV_FLUID_DEBUG
  GCon->Logf("Fluid: %d tracks in song", midisong.ntracks);
#endif
  Restart();
}


//==========================================================================
//
//  VFluidAudioCodec::~VFluidAudioCodec
//
//==========================================================================
VFluidAudioCodec::~VFluidAudioCodec () {
  Z_Free(midisong.tracks);
  Z_Free(MidiData);
}


//==========================================================================
//
//  VFluidAudioCodec::Decode
//
//==========================================================================
int VFluidAudioCodec::Decode (short *Data, int NumSamples) {
  if (activeChanCount == 0) {
    StopSound();
    return 0;
  }
  //k8: this code is total crap, but idc for now
  fluid_synth_t *synth = FluidManager::synth;
  int res = 0;
  // step by 10 msecs
  static const int stepmsec = 10;
  static const int stepframes = 44100*stepmsec/1000;
  while (NumSamples > 0 && activeChanCount > 0) {
    if (nextFrameTic == 0) {
#ifdef VV_FLUID_DEBUG
      GCon->Logf("FLUID: TICK (total=%d, left=%d)!", res, NumSamples);
#endif
      for (int f = 0; f < activeChanCount; ++f) {
#ifdef VV_FLUID_DEBUG
        GCon->Logf("FLUID: TICK for channel #%d (#%d)", f, activeChanIdx[f]);
#endif
        runChannel(&chans[activeChanIdx[f]], stepmsec);
      }
      // remove dead channels, and compact channel list
      int lastCPos = 0;
      for (int f = 0; f < activeChanCount; ++f) {
        if (chans[activeChanIdx[f]].ended) {
#ifdef VV_FLUID_DEBUG
          GCon->Logf("FLUID(!!!): channel #%d (#%d) is DEAD", f, activeChanIdx[f]);
#endif
          activeChanIdx[f] = -1;
        } else {
          activeChanIdx[lastCPos++] = activeChanIdx[f];
        }
      }
      activeChanCount = lastCPos;
      // update next update time
      nextFrameTic = stepframes;
    }
    int rdf = NumSamples;
    if (rdf > nextFrameTic) rdf = nextFrameTic;
    if (fluid_synth_write_s16(synth, rdf, Data, 0, 2, Data, 1, 2) != FLUID_OK) {
#ifdef VV_FLUID_DEBUG
      GCon->Logf("FLUID: ERROR getting a sample (total=%d, left=%d)!", res, NumSamples);
#endif
      activeChanCount = 0;
      break;
    }
    //GCon->Logf("FLUID: got a sample (total=%d, left=%d)!", res, NumSamples);
    Data += rdf*2;
    res += rdf*2;
    NumSamples -= rdf;
    nextFrameTic -= rdf;
    check(nextFrameTic >= 0);
  }
  if (res == 0 || activeChanCount == 0) StopSound();
  return res;
}


//==========================================================================
//
//  VFluidAudioCodec::Finished
//
//==========================================================================
bool VFluidAudioCodec::Finished () {
  return (activeChanCount == 0);
}


void VFluidAudioCodec::StopSound () {
  if (!FluidManager::synth) return;
  // reset channels
  for (int f = 0; f < MIDI_CHANNELS; ++f) {
    // volume
    fluid_synth_cc(FluidManager::synth, f, 0x07, 0);
    fluid_synth_cc(FluidManager::synth, f, 0x27, 0);
    // all notes off
    fluid_synth_cc(FluidManager::synth, f, 0x7B, 0);
    // all ctrl off
    fluid_synth_cc(FluidManager::synth, f, 0x79, 0);
    // all sound off
    fluid_synth_cc(FluidManager::synth, f, 0x78, 0);
  }
}


//==========================================================================
//
//  VFluidAudioCodec::Restart
//
//==========================================================================
void VFluidAudioCodec::Restart () {
  StopSound();
  activeChanCount = 0;
  memset((void *)&chans[0], 0, sizeof(chans));
  for (int f = 0; f < MIDI_CHANNELS; ++f) chans[f].ended = true;
  // setup defaults
  for (int cidx = 0; cidx < midisong.ntracks; ++cidx) {
    chan_t *chan = &chans[cidx];
    track_t *track = &midisong.tracks[cidx];
    chan->id = cidx;
    chan->song = &midisong;
    chan->track = track;
    chan->tics = 0;
    chan->lasttic = 0;
    chan->starttic = 0;
    chan->pos = track->data;
    chan->jump = nullptr;
    chan->ended = false;
    chan->starttime = 0;
    chan->curtime = 0;
    fluid_synth_cc(FluidManager::synth, cidx, 0x07/*volume_msb*/, 127);
    // immediately start reading the midi track
    chan->nexttic = getNextTick(chan);
    activeChanIdx[activeChanCount++] = cidx;
  }
}


//==========================================================================
//
//  VFluidAudioCodec::Create
//
//==========================================================================
VAudioCodec *VFluidAudioCodec::Create (VStream *InStrm) {
  if (snd_mid_player != 1) return nullptr;

  int size = InStrm->TotalSize();
  if (size < 0x0e) {
    GCon->Logf(NAME_Warning, "Failed to load MIDI song");
    return nullptr;
  }

  // check if it's a MIDI file
  char Header[4];
  InStrm->Seek(0);
  InStrm->Serialise(Header, 4);
  if (InStrm->IsError() || memcmp(Header, "MThd", 4)) return nullptr;

  if (!fluidManager.InitFluid()) return nullptr;

  // load song
  vuint8 *data = (vuint8 *)Z_Malloc(size);
  InStrm->Seek(0);
  InStrm->Serialise(data, size);
  if (InStrm->IsError()) {
    GCon->Logf(NAME_Warning, "Failed to load MIDI song");
    return nullptr;
  }

  InStrm->Close();
  delete InStrm;

  // create codec
  return new VFluidAudioCodec(data, size);
}
