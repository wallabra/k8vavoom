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

//#define VV_FLUID_DEBUG
//#define VV_FLUID_DEBUG_DUMP_TRACKS
//#define VV_FLUID_DEBUG_TICKS

#ifdef BUILTIN_FLUID
# include "../../libs/fluidsynth_lite/include/fluidsynth-lite.h"
# if (FLUIDSYNTH_VERSION_MAJOR != 1) || (FLUIDSYNTH_VERSION_MINOR != 1) || (FLUIDSYNTH_VERSION_MICRO != 6)
#  warning "invalid FluidSynth version"
# endif
#else
# include "fluidsynth.h"
#endif


extern TArray<VStr> midiSynthAllBanks;

extern void forceMidiBanksScan ();
extern void scanForMidiBanks ();


extern VCvarB snd_timidity_autoload_sf2;
extern VCvarS snd_timidity_sf2_file;


// Values are: 0 = FLUID_INTERP_NONE
//             1 = FLUID_INTERP_LINEAR
//             4 = FLUID_INTERP_4THORDER (the FluidSynth default)
//             7 = FLUID_INTERP_7THORDER
static VCvarI snd_fluid_interp("snd_fluid_interp", "2", "FluidSynth interpolation.", CVAR_Archive);

static VCvarI snd_fluid_voices("snd_fluid_voices", "128", "Number of FluidSynth voices.", CVAR_Archive);
static VCvarF snd_fluid_gain("snd_fluid_gain", "1", "FluidSynth global gain.", CVAR_Archive);

static VCvarB snd_fluid_reverb("snd_fluid_reverb", true, "Allow FluidSynth reverb?", CVAR_Archive);
// [0..1.2]
static VCvarF snd_fluid_reverb_roomsize("snd_fluid_reverb_roomsize", "0.61", "FluidSynth reverb room size.", CVAR_Archive);
// [0..1]
static VCvarF snd_fluid_reverb_damping("snd_fluid_reverb_damping", "0.23", "FluidSynth reverb damping.", CVAR_Archive);
// [0..100]
static VCvarF snd_fluid_reverb_width("snd_fluid_reverb_width", "0.76", "FluidSynth reverb width.", CVAR_Archive);
// [0..1]
static VCvarF snd_fluid_reverb_level("snd_fluid_reverb_level", "0.57", "FluidSynth reverb level.", CVAR_Archive);

static VCvarB snd_fluid_chorus("snd_fluid_chorus", false, "Allow FluidSynth chorus?", CVAR_Archive);
// [0..99]
static VCvarI snd_fluid_chorus_voices("snd_fluid_chorus_voices", "3", "Number of FluidSynth chorus voices.", CVAR_Archive);
// [0..1] -- wtf?
static VCvarF snd_fluid_chorus_level("snd_fluid_chorus_level", "1.2", "FluidSynth chorus level.", CVAR_Archive);
// [0.29..5]
static VCvarF snd_fluid_chorus_speed("snd_fluid_chorus_speed", "0.3", "FluidSynth chorus speed.", CVAR_Archive);
// depth is in ms and actual maximum depends on the sample rate
// [0..21]
static VCvarF snd_fluid_chorus_depth("snd_fluid_chorus_depth", "8", "FluidSynth chorus depth.", CVAR_Archive);
// [0..1]
static VCvarI snd_fluid_chorus_type("snd_fluid_chorus_type", "0", "FluidSynth chorus type (0:sine; 1:triangle).", CVAR_Archive);


// ////////////////////////////////////////////////////////////////////////// //
enum {
  MIDI_MAX_CHANNEL = 16,
};

enum /*event type*/ {
  // channel messages
  NOTE_OFF = 0x80,
  NOTE_ON = 0x90,
  KEY_PRESSURE = 0xa0,
  CONTROL_CHANGE = 0xb0,
  PROGRAM_CHANGE = 0xc0,
  CHANNEL_PRESSURE = 0xd0,
  PITCH_BEND = 0xe0,
  // system exclusive
  MIDI_SYSEX = 0xf0,
  MIDI_EOX = 0xf7,
  // meta event
  MIDI_META_EVENT = 0xff,
};

enum /*metaevent*/ {
  MIDI_SEQ_NUM = 0x00,
  MIDI_TEXT = 0x01,
  MIDI_COPYRIGHT = 0x02,
  MIDI_TRACK_NAME = 0x03,
  MIDI_INST_NAME = 0x04,
  MIDI_LYRIC = 0x05,
  MIDI_MARKER = 0x06,
  MIDI_CUE_POINT = 0x07,
  MIDI_CHANNEL = 0x20, // channel for the following meta
  MIDI_EOT = 0x2f,
  MIDI_SET_TEMPO = 0x51,
  MIDI_SMPTE_OFFSET = 0x54,
  MIDI_TIME_SIGNATURE = 0x58,
  MIDI_KEY_SIGNATURE = 0x59,
  MIDI_SEQUENCER_EVENT = 0x7f,
};


// ////////////////////////////////////////////////////////////////////////// //
class VFluidAudioCodec : public VAudioCodec {
public:
  MIDIData *mididata;
  int framesUntilEvent;

public:
  VFluidAudioCodec (MIDIData *amididata); // takes ownership
  virtual ~VFluidAudioCodec () override;

  virtual int Decode (short *Data, int NumSamples) override;
  virtual bool Finished () override;
  virtual void Restart () override;

private:
  static void eventCB (double timemsecs, const MIDIData::MidiEvent &ev, void *);

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
  static int versionPrinted;

  // last values of some cvars
  static VStr sf2Path;
  static bool autoloadSF2;
  static bool needRestart;

protected:
  static bool NeedRestart ();
  static void UpdateCvarCache ();

  static void onCVarChanged (VCvar *cvar, const VStr &oldValue) { needRestart = true; }

public:
  FluidManager () {
    snd_fluid_voices.MeChangedCB = &onCVarChanged;
    snd_fluid_gain.MeChangedCB = &onCVarChanged;
    snd_fluid_reverb.MeChangedCB = &onCVarChanged;
    snd_fluid_chorus.MeChangedCB = &onCVarChanged;
    snd_fluid_interp.MeChangedCB = &onCVarChanged;
    snd_fluid_reverb_roomsize.MeChangedCB = &onCVarChanged;
    snd_fluid_reverb_damping.MeChangedCB = &onCVarChanged;
    snd_fluid_reverb_width.MeChangedCB = &onCVarChanged;
    snd_fluid_reverb_level.MeChangedCB = &onCVarChanged;
    snd_fluid_chorus_voices.MeChangedCB = &onCVarChanged;
    snd_fluid_chorus_level.MeChangedCB = &onCVarChanged;
    snd_fluid_chorus_speed.MeChangedCB = &onCVarChanged;
    snd_fluid_chorus_depth.MeChangedCB = &onCVarChanged;
    snd_fluid_chorus_type.MeChangedCB = &onCVarChanged;
  }

  ~FluidManager () { CloseFluid(); }

  // returns success flag
  static bool InitFluid ();
  // WARNING! song must be freed!
  static void CloseFluid ();

  static void ResetSynth ();
};

static FluidManager fluidManager;


IMPLEMENT_AUDIO_CODEC(VFluidAudioCodec, "FluidSynth");


fluid_settings_t *FluidManager::settings = nullptr;
fluid_synth_t *FluidManager::synth = nullptr;
fluid_audio_driver_t *FluidManager::driver = nullptr;
int FluidManager::sf2id = -1;

int FluidManager::fluidInitialised = -1;
int FluidManager::versionPrinted = -1;
VStr FluidManager::sf2Path = VStr::EmptyString;
bool FluidManager::autoloadSF2 = false;
bool FluidManager::needRestart = false;


// ////////////////////////////////////////////////////////////////////////// //
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


//==========================================================================
//
//  FluidManager::ResetSynth
//
//==========================================================================
void FluidManager::ResetSynth () {
  if (!synth) return;
  fluid_synth_system_reset(synth);
#if 0
  // reset channels
  for (int f = 0; f < MIDI_MAX_CHANNEL; ++f) {
    // volume
    fluid_synth_cc(synth, f, 0x07, 127);
    fluid_synth_cc(synth, f, 0x27, 0);
    // all notes off
    fluid_synth_cc(synth, f, 0x7B, 0);
    // all ctrl off
    fluid_synth_cc(synth, f, 0x79, 0);
    // all sound off
    fluid_synth_cc(synth, f, 0x78, 0);
  }
#endif
}


//==========================================================================
//
//  FluidManager::InitFluid
//
//==========================================================================
bool FluidManager::InitFluid () {
  if (NeedRestart()) CloseFluid();

  if (fluidInitialised >= 0) return (fluidInitialised > 0);

  if (versionPrinted < 0) {
    versionPrinted = 1;
    GCon->Logf("FluidSynth version %s", fluid_version_str());
  }

  // shut the fuck up!
  fluid_set_log_function(FLUID_PANIC, &shutTheFuckUpFluid, nullptr);
  fluid_set_log_function(FLUID_ERR, &shutTheFuckUpFluid, nullptr);
  fluid_set_log_function(FLUID_WARN, &shutTheFuckUpFluid, nullptr);
  fluid_set_log_function(FLUID_INFO, &shutTheFuckUpFluid, nullptr);
  fluid_set_log_function(FLUID_DBG, &shutTheFuckUpFluid, nullptr);

  check(sf2id < 0);

  if (autoloadSF2 != snd_timidity_autoload_sf2.asBool()) forceMidiBanksScan();

  UpdateCvarCache();

  int interp = 0;
  switch (snd_fluid_interp.asInt()) {
    case 0: interp = 0; break; // FLUID_INTERP_NONE
    case 1: interp = 1; break; // FLUID_INTERP_LINEAR
    case 2: interp = 4; break; // FLUID_INTERP_4THORDER
    case 3: interp = 7; break; // FLUID_INTERP_7THORDER
  }


  // alloc settings
  if (!settings) settings = new_fluid_settings();
  fluid_settings_setint(settings, "synth.midi-channels", MIDI_MAX_CHANNEL);
  fluid_settings_setint(settings, "synth.polyphony", clampval(snd_fluid_voices.asInt(), 16, 1024));
  fluid_settings_setint(settings, "synth.cpu-cores", 0); // FluidSynth-Lite cannot work in multithreaded mode (segfault)
  fluid_settings_setnum(settings, "synth.sample-rate", 44100);
  fluid_settings_setnum(settings, "synth.gain", clampval(snd_fluid_gain.asFloat(), 0.0f, 10.0f));
  fluid_settings_setint(settings, "synth.reverb.active", (snd_fluid_reverb.asBool() ? 1 : 0));
  fluid_settings_setint(settings, "synth.chorus.active", (snd_fluid_chorus.asBool() ? 1 : 0));

  fluid_settings_setint(settings, "synth.interpolation", interp);

  // init synth
  if (!synth) synth = new_fluid_synth(settings);
  if (!synth) {
    GCon->Log(NAME_Warning, "FluidSynth: failed to create synthesizer");
    fluidInitialised = 0;
    return false;
  }

  fluid_synth_set_interp_method(synth, -1, interp);
  fluid_synth_set_gain(synth, snd_fluid_gain.asFloat());
  fluid_synth_set_reverb(synth, snd_fluid_reverb_roomsize, snd_fluid_reverb_damping, snd_fluid_reverb_width, snd_fluid_reverb_level);
  fluid_synth_set_reverb_on(synth, (snd_fluid_reverb.asBool() ? 1 : 0));

  fluid_synth_set_chorus(synth, snd_fluid_chorus_voices.asInt(), snd_fluid_chorus_level.asFloat(),
                         snd_fluid_chorus_speed.asFloat(), snd_fluid_chorus_depth.asFloat(), snd_fluid_chorus_type.asInt());


  scanForMidiBanks();

  if (snd_timidity_autoload_sf2) {
    TArray<VStr> failedBanks;
    // try to load a bank
    for (auto &&bfn : midiSynthAllBanks) {
      VStr sf2name = bfn;
      if (sf2name.isEmpty()) continue;
      if (!sf2name.extractFileExtension().strEquCI(".sf2")) continue;
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

  ResetSynth();

  fluidInitialised = 1;
  return true;
}


//==========================================================================
//
//  VFluidAudioCodec::VFluidAudioCodec
//
//==========================================================================
VFluidAudioCodec::VFluidAudioCodec (MIDIData *amididata)
  : mididata(amididata)
{
  FluidManager::ResetSynth();
  Restart();
}


//==========================================================================
//
//  VFluidAudioCodec::~VFluidAudioCodec
//
//==========================================================================
VFluidAudioCodec::~VFluidAudioCodec () {
  delete mididata;
  mididata = nullptr;
}


//==========================================================================
//
//  VFluidAudioCodec::eventCB
//
//==========================================================================
void VFluidAudioCodec::eventCB (double timemsecs, const MIDIData::MidiEvent &ev, void *) {
  switch (ev.type) {
    case MIDIData::NOTE_OFF:
      fluid_synth_noteoff(FluidManager::synth, ev.channel, ev.data1);
      break;
    case MIDIData::NOTE_ON:
      fluid_synth_noteon(FluidManager::synth, ev.channel, ev.data1, ev.data2);
      break;
    case MIDIData::KEY_PRESSURE:
      break;
    case MIDIData::CONTROL_CHANGE:
      fluid_synth_cc(FluidManager::synth, ev.channel, ev.data1, ev.data2);
      break;
    case MIDIData::PROGRAM_CHANGE:
      fluid_synth_program_change(FluidManager::synth, ev.channel, ev.data1);
      break;
    case MIDIData::CHANNEL_PRESSURE:
      fluid_synth_channel_pressure(FluidManager::synth, ev.channel, ev.data1);
      break;
    case MIDIData::PITCH_BEND: // pitch bend
      fluid_synth_pitch_bend(FluidManager::synth, ev.channel, ev.data1);
      break;
    default:
      break;
  }
}


//==========================================================================
//
//  VFluidAudioCodec::Decode
//
//==========================================================================
int VFluidAudioCodec::Decode (short *Data, int NumSamples) {
  if (!mididata) return 0;
  int res = 0;
  // use adaptive stepping
  while (NumSamples > 0) {
    if (framesUntilEvent == 0) {
      framesUntilEvent = mididata->decodeStep(&eventCB, 44100, nullptr);
      if (framesUntilEvent <= 0) {
        // done
        FluidManager::ResetSynth();
        break;
      }
    }
    int rdf = NumSamples;
    if (rdf > framesUntilEvent) rdf = framesUntilEvent;
    if (fluid_synth_write_s16(FluidManager::synth, rdf, Data, 0, 2, Data, 1, 2) != FLUID_OK) {
      GCon->Log(NAME_Error, "FluidSynth: error getting a sample, playback aborted!");
      mididata->abort();
      break;
    }
    //GCon->Logf("FLUID: got a sample (total=%d, left=%d)!", res, NumSamples);
    Data += rdf*2;
    res += rdf*2;
    NumSamples -= rdf;
    framesUntilEvent -= rdf;
    check(framesUntilEvent >= 0);
  }
  if (res == 0) FluidManager::ResetSynth();
  return res;
}


//==========================================================================
//
//  VFluidAudioCodec::Finished
//
//==========================================================================
bool VFluidAudioCodec::Finished () {
  return (mididata ? mididata->isFinished() : true);
}


//==========================================================================
//
//  VFluidAudioCodec::Restart
//
//==========================================================================
void VFluidAudioCodec::Restart () {
  FluidManager::ResetSynth();
  if (mididata) mididata->restart();
  framesUntilEvent = 0;
}


//==========================================================================
//
//  VFluidAudioCodec::Create
//
//==========================================================================
VAudioCodec *VFluidAudioCodec::Create (VStream *InStrm) {
  if (snd_mid_player != 1) return nullptr;
  if (InStrm->IsError()) return nullptr;

  int size = InStrm->TotalSize();
  if (size < 0x0e) return nullptr;

  // check if it's a MIDI file
  InStrm->Seek(0);
  if (!MIDIData::isMidiStream(*InStrm)) return nullptr;
  if (!fluidManager.InitFluid()) return nullptr;

  // load song
  MIDIData *mdata = new MIDIData();
  InStrm->Seek(0);
  if (!mdata->parseStream(*InStrm)) {
    // some very fatal error
    delete mdata;
    return nullptr;
  }
  // ok, we pwned it
  InStrm->Close();
  delete InStrm;

  // create codec
  return new VFluidAudioCodec(mdata);
}
