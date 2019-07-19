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
static VCvarI snd_fluid_interp("snd_fluid_interp", "1", "FluidSynth interpolation.", CVAR_Archive);

static VCvarI snd_fluid_voices("snd_fluid_voices", "128", "Number of FluidSynth voices.", CVAR_Archive);
static VCvarF snd_fluid_gain("snd_fluid_gain", "1", "FluidSynth global gain.", CVAR_Archive);

static VCvarB snd_fluid_reverb("snd_fluid_reverb", false, "Allow FluidSynth reverb?", CVAR_Archive);
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

static VCvarB snd_fluid_midi_messages("snd_fluid_midi_messages", false, "Show messages from MIDI files?", CVAR_Archive);


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
  // midi data definitions
  struct song_t;

  // these data should not be modified outside the
  // audio thread unless they're being initialized
  struct track_t {
  private:
    vint32 datasize;
    const vuint8 *tkdata;
    const vuint8 *pos;
    song_t *song;
  public:
    vuint8 runningStatus;
    vuint8 lastMetaChannel; // 0xff: all
    vuint32 nexttic;
    vuint32 starttic;
    // after last track command, wait a little
    double fadeoff;

    VStr copyright; // track copyright
    VStr tname; // track name
    VStr iname; // instrument name

  public:
    track_t () : datasize(0), tkdata(nullptr), pos(nullptr), song(nullptr), runningStatus(0), lastMetaChannel(0xff), nexttic(0), starttic(0), fadeoff(0) {}

    void setup (song_t *asong, const vuint8 *adata, vint32 alen) {
      if (alen <= 0) adata = nullptr;
      if (!adata) alen = 0;
      datasize = alen;
      tkdata = adata;
      song = asong;
      reset();
    }

    inline void abort (bool full) { if (tkdata) pos = tkdata+datasize; if (full) fadeoff = 0; }

    inline void reset () {
      pos = tkdata;
      runningStatus = 0;
      lastMetaChannel = 0xff;
      nexttic = 0;
      starttic = 0;
      fadeoff = 0;
      copyright.clear();
      tname.clear();
      iname.clear();
    }

    inline bool isEOT () const { return (!tkdata || pos == tkdata+datasize); }

    inline int getPos () const { return (tkdata ? (int)(ptrdiff_t)(pos-tkdata) : 0); }
    inline int getLeft () const { return (tkdata ? (int)(ptrdiff_t)(tkdata+datasize-pos) : 0); }

    inline const char *getCurPosPtr () const { return (const char *)pos; }

    inline int size () const { return datasize; }
    inline vuint8 dataAt (int pos) const { return (tkdata && datasize > 0 && pos >= 0 && pos < datasize ? tkdata[pos] : 0); }
    inline vuint8 operator [] (int ofs) const {
      if (!tkdata || !datasize) return 0;
      if (ofs >= 0) {
        const int left = getLeft();
        if (ofs >= left) return 0;
        return pos[ofs];
      } else {
        if (ofs == MIN_VINT32) return 0;
        ofs = -ofs;
        const int pos = getLeft();
        if (ofs > pos) return 0;
        return tkdata[pos-ofs];
      }
    }

    inline song_t *getSong () { return song; }
    inline const song_t *getSong () const { return song; }

    inline vuint8 peekNextMidiByte () {
      if (isEOT()) return 0;
      return *pos;
    }

    inline vuint8 getNextMidiByte () {
      if (isEOT()) return 0;
      return *pos++;
    }

    inline void skipNextMidiBytes (int len) {
      while (len-- > 0) (void)getNextMidiByte();
    }

    // reads a variable-length SMF number
    vuint32 readVarLen () {
      vuint32 res = 0;
      int left = 4;
      for (;;) {
        if (left == 0) { abort(true); return 0; }
        --left;
        vuint8 t = getNextMidiByte();
        res = (res<<7)|(t&0x7fu);
        if ((t&0x80) == 0) break;
      }
      return res;
    }

    inline vuint32 getDeltaTic () {
      return readVarLen();
      //return (starttic+(vuint32)((double)tic*song->timediv));
    }
  };

  struct song_t {
    vuint16 type;
    vuint16 delta;
    TArray<track_t> tracks;
    vuint32 tempo;
    //double timediv;
    vuint32 curtime;

    inline void setTempo (vint32 atempo) {
      if (atempo <= 0) atempo = 480000;
      tempo = atempo;
      //timediv = getTimeDivision();
    }

    //inline double getTimeDivision () const { return (double)tempo/(double)delta/1000.0; }

    inline vuint32 tic2ms (vuint32 tic) const { return (vuint32)(((double)tic*tempo)/(delta*1000.0)); }
    inline vuint32 ms2tic (vuint32 msec) const { return (vuint32)((msec*delta*1000.0)/(double)tempo); }
  };

public:
  vuint8 *MidiData;
  int SongSize;
  vint32 nextFrameTic;
  int currtrack; // for midi type 2
  song_t midisong;

public:
  VFluidAudioCodec (vuint8 *InSong, int aSongSize);
  virtual ~VFluidAudioCodec () override;
  virtual int Decode (short *Data, int NumSamples) override;
  virtual bool Finished () override;
  virtual void Restart () override;

public:
  // returns `false` if the track is complete
  bool runTrack (int tidx);

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
VFluidAudioCodec::VFluidAudioCodec (vuint8 *InSong, int aSongSize)
  : MidiData(InSong)
  , SongSize(aSongSize)
  , nextFrameTic(0)
  , currtrack(0)
{
  FluidManager::ResetSynth();
  memset((void *)&midisong, 0, sizeof(midisong));

  vuint16 ntracks = 0;
  char header[4] = {0};
  vuint32 chunksize = 0;

  memcpy(&header[0], InSong, 4); InSong += 4;

  memcpy(&chunksize, InSong, 4); InSong += 4;
  chunksize = BigLong(chunksize);
  check(chunksize >= 6);

  memcpy(&midisong.type, InSong, 2); InSong += 2;
  midisong.type = BigShort(midisong.type);

  memcpy(&ntracks, InSong, 2); InSong += 2;
  ntracks = BigShort(ntracks);

  memcpy(&midisong.delta, InSong, 2); InSong += 2;
  midisong.delta = BigShort(midisong.delta);

  midisong.setTempo(480000);

  if (ntracks > 0) {
    // collect tracks
    vuint8 *data = InSong; //song.data+0x0e;
    int left = aSongSize-(vint32)chunksize-4*2;
    while (midisong.tracks.length() < ntracks) {
      if (left < 8) break;
      memcpy(header, data, 4);
      memcpy(&chunksize, data+4, 4);
      left -= 8;
      data += 8;
      chunksize = BigLong(chunksize);
      if (chunksize > (vuint32)left) break;
      // ignore non-track chunks
      if (memcmp(header, "MTrk", 4) != 0) {
        bool ok = true;
        for (int f = 0; f < 4; ++f) {
          vuint8 ch = (vuint8)header[f];
          if (ch < 32 || ch >= 127) { ok = false; break; }
        }
        if (!ok) break;
      } else {
        track_t &track = midisong.tracks.alloc();
        memset((void *)&track, 0, sizeof(track_t));
        track.setup(&midisong, data, (vint32)chunksize);
#ifdef VV_FLUID_DEBUG_DUMP_TRACKS
        GCon->Logf("  track #%d: %u bytes", midisong.tracks.length()-1, track.size());
        for (int f = 0; f < track.size(); ++f) GCon->Logf("    %5d: 0x%02x", f, track[f]);
#endif
      }
      left -= chunksize;
      data += chunksize;
    }
  }

#ifdef VV_FLUID_DEBUG
  GCon->Logf("Fluid: %d tracks in song", midisong.tracks.length());
#endif
  Restart();
}


//==========================================================================
//
//  VFluidAudioCodec::~VFluidAudioCodec
//
//==========================================================================
VFluidAudioCodec::~VFluidAudioCodec () {
  midisong.tracks.clear();
  Z_Free(MidiData);
}


//==========================================================================
//
//  VFluidAudioCodec::runTrack
//
//  main midi parsing routine
//  returns `false` if the track is complete
//
//==========================================================================
bool VFluidAudioCodec::runTrack (int tidx) {
  if (tidx < 0 || tidx >= midisong.tracks.length()) return false;

  track_t &track = midisong.tracks[tidx];
  song_t *song = track.getSong();

  if (song->curtime < track.nexttic) return true;

  // last fade?
  if (track.isEOT()) return (song->curtime < track.fadeoff);

#ifdef VV_FLUID_DEBUG_TICKS
  GCon->Logf("FLUID: TICK for channel #%d (stime=%u; stt=%u; ntt=%u; eot=%d)", tidx, song->curtime, track.starttic, track.nexttic, (track.isEOT() ? 1 : 0));
#endif

  // keep parsing through midi track until
  // the end is reached or until it reaches next delta time
  while (!track.isEOT() && song->curtime >= track.nexttic) {
    track.starttic = track.nexttic;

    vuint8 evcode = track.peekNextMidiByte();
    // for invalid status byte: use the running status instead
    if ((evcode&0x80) == 0) {
      evcode = track.runningStatus;
    } else {
      evcode = track.getNextMidiByte();
      track.runningStatus = evcode; //k8: dunno, it seems that only normal midi events should do this, but...
    }

    // still invalid?
    if ((evcode&0x80) == 0) { track.abort(true); return false; }

#ifdef VV_FLUID_DEBUG_TICKS
    GCon->Logf("EVENT: tidx=%d; pos=%d; len=%d; left=%d; event=0x%02x; nt=%d; ct=%d", tidx, track.getPos(), track.size(), track.getLeft(), evcode, track.nexttic, song->curtime);
#endif

    if (evcode == MIDI_SYSEX) {
      // system exclusive
      // read the length of the message
      vuint32 len = track.readVarLen();
      // check for valid length
      if (len == 0 || len > (vuint32)track.getLeft()) { track.abort(true); return false; }
      if (track[len-1] == MIDI_EOX) {
        fluid_synth_sysex(FluidManager::synth, track.getCurPosPtr(), len-1, nullptr, nullptr, nullptr, 0);
      } else {
        // oops, incomplete packed, ignore it
      }
      track.skipNextMidiBytes(len);
    } else if (evcode == MIDI_EOX) {
      vuint32 len = track.readVarLen();
      // check for valid length
      if (len == 0 || len > (vuint32)track.getLeft()) { track.abort(true); return false; }
      track.skipNextMidiBytes(len);
    } else if (evcode == MIDI_META_EVENT) {
      evcode = track.getNextMidiByte();
      vuint32 len = track.readVarLen();
      // check for valid length
      if (len > (vuint32)track.getLeft()) { track.abort(true); return false; }
#ifdef VV_FLUID_DEBUG
      GCon->Logf("META: tidx=%d; meta=0x%02x; len=%u", tidx, evcode, len);
#endif
      switch (evcode) {
        case MIDI_EOT:
#ifdef VV_FLUID_DEBUG
          GCon->Log("  END-OF-TRACK");
#endif
          track.abort(false);
          break;
        case MIDI_SET_TEMPO:
          if (len == 3) {
            vint32 t = (((vuint32)track[0])<<16)|(((vuint32)track[1])<<8)|((vuint32)track[2]);
#ifdef VV_FLUID_DEBUG
            GCon->Logf("  tempo: %u", t);
#endif
            song->setTempo(t);
          }
          break;
        case MIDI_CHANNEL: // channel for the following meta
          if (len == 1) {
            track.lastMetaChannel = track[0];
            if (track.lastMetaChannel >= MIDI_MAX_CHANNEL) track.lastMetaChannel = 0xff;
          }
          break;
        // texts
        case MIDI_COPYRIGHT:
        case MIDI_TRACK_NAME:
        case MIDI_INST_NAME:
          {
            VStr data;
            if (len > 0) {
              // collect text
              VStr currLine;
              bool prevLineWasEmpty = true;
              for (vuint32 f = 0; f < len; ++f) {
                vuint8 ch = track[f];
                if (ch == '\r' && (f == len-1 || track[f+1] != '\n')) ch = '\n';
                if (ch == '\n') {
                  while (currLine.length() && (vuint8)(currLine[currLine.length()-1]) <= ' ') currLine.chopRight(1);
                  if (currLine.length() != 0) {
                    if (data.length()) data += '\n';
                    data += currLine;
                    prevLineWasEmpty = false;
                  } else {
                    if (!prevLineWasEmpty) {
                      if (data.length()) data += '\n';
                      data += currLine;
                    }
                    prevLineWasEmpty = true;
                  }
                  currLine.clear();
                } else {
                  if (ch < 32 || ch >= 127) ch = ' ';
                  if (ch == ' ') {
                    if (currLine.length() > 0 && currLine[currLine.length()-1] != ' ') currLine += (char)ch;
                  } else {
                    currLine += (char)ch;
                  }
                }
              }
              while (currLine.length() && (vuint8)(currLine[currLine.length()-1]) <= ' ') currLine.chopRight(1);
              if (currLine.length()) {
                if (data.length()) data += '\n';
                data += currLine;
              }
            }
            const char *mstype = nullptr;
            switch (evcode) {
              case MIDI_COPYRIGHT: mstype = "copyright"; track.copyright = data; break;
              case MIDI_TRACK_NAME: mstype = "name"; track.tname = data; break;
              case MIDI_INST_NAME: mstype = "instrument"; track.iname = data; break;
            }
            if (snd_fluid_midi_messages && data.length() != 0 && mstype) {
              TArray<VStr> lines;
              data.split('\n', lines);
              check(lines.length() > 0);
              const char *pfx = "";
              for (auto &&ls : lines) {
                GCon->Logf("FluidSynth: MIDI track #%d %s: %s%s", tidx, mstype, pfx, *ls);
                pfx = "  ";
              }
            }
          }
          break;
        default:
          break;
      }
      track.skipNextMidiBytes(len);
    } else {
      //track.runningStatus = evcode;

      vuint8 event = evcode&0xf0u;
      vuint8 channel = evcode&0x0fu;

      // all channel message have at least 1 byte of associated data
      vuint8 data1 = track.getNextMidiByte();
      vuint8 data2;

      switch (event) {
        case NOTE_OFF:
          data2 = track.getNextMidiByte();
#ifdef VV_FLUID_DEBUG
          GCon->Logf("  (%u):NOTE_OFF: %u %u", channel, data1, data2);
#endif
          fluid_synth_noteoff(FluidManager::synth, channel, data1);
          break;
        case NOTE_ON:
          data2 = track.getNextMidiByte();
#ifdef VV_FLUID_DEBUG
          GCon->Logf("  (%u):NOTE_N: %u %u", channel, data1, data2);
#endif
          fluid_synth_noteon(FluidManager::synth, channel, data1, data2);
          break;
        case KEY_PRESSURE:
          data2 = track.getNextMidiByte();
#ifdef VV_FLUID_DEBUG
          GCon->Logf("  (%u):KEY_PRESSURE: %u %u", channel, data1, data2);
#endif
          break;
        case CONTROL_CHANGE:
          data2 = track.getNextMidiByte();
#ifdef VV_FLUID_DEBUG
          GCon->Logf("  (%u):CONTROL_CHANGE: %u %u", channel, data1, data2);
#endif
          fluid_synth_cc(FluidManager::synth, channel, data1, data2);
          break;
        case PROGRAM_CHANGE:
#ifdef VV_FLUID_DEBUG
          GCon->Logf("  (%u):PROGRAM_CHANGE: %u", channel, data1);
#endif
          fluid_synth_program_change(FluidManager::synth, channel, data1);
          break;
        case CHANNEL_PRESSURE:
#ifdef VV_FLUID_DEBUG
          GCon->Logf("  (%u):CHANNEL_PRESSURE: %u", channel, data1);
#endif
          fluid_synth_channel_pressure(FluidManager::synth, channel, data1);
          break;
        case PITCH_BEND: // pitch bend
          data2 = track.getNextMidiByte();
#ifdef VV_FLUID_DEBUG
          GCon->Logf("  (%u):PITCH_BEND: %u", channel, (data1&0x7f)|(((vuint32)(data2&0x7f))<<7));
#endif
          fluid_synth_pitch_bend(FluidManager::synth, channel, (data1&0x7f)|(((vuint32)(data2&0x7f))<<7));
          break;
        default:
          GCon->Logf("  (%u):INVALID COMMAND! (0x%02x)", channel, event);
          track.abort(true);
          return false;
      }
    }
    // check for end of the track, otherwise get the next delta time
    if (!track.isEOT()) {
      vuint32 dtime = track.getDeltaTic();
#ifdef VV_FLUID_DEBUG
      GCon->Logf("  timedelta: %u (%u) (pos=%d)", dtime, song->tic2ms(dtime), track.getPos());
#endif
      track.nexttic = track.starttic+song->tic2ms(dtime);
    }
  }

  if (track.isEOT()) track.fadeoff = (song->type != 2 ? track.nexttic+200 : 0);
  return true;
}


//==========================================================================
//
//  VFluidAudioCodec::Decode
//
//==========================================================================
int VFluidAudioCodec::Decode (short *Data, int NumSamples) {
  //k8: this code is total crap, but idc for now
  fluid_synth_t *synth = FluidManager::synth;
  int res = 0;
  // step by 10 msecs
  static const int stepmsec = 10;
  static const int stepframes = 44100*stepmsec/1000;
  while (NumSamples > 0) {
    if (nextFrameTic == 0) {
      bool hasActiveTracks = false;
      midisong.curtime += stepmsec;
#ifdef VV_FLUID_DEBUG_TICKS
      //GCon->Logf("FLUID: TICK (total=%d, left=%d)!", res, NumSamples);
#endif
      if (midisong.type != 2) {
        for (int f = 0; f < midisong.tracks.length(); ++f) {
          if (runTrack(f)) hasActiveTracks = true;
        }
      } else {
        while (currtrack < midisong.tracks.length()) {
          if (runTrack(currtrack)) { hasActiveTracks = true; break; }
          midisong.curtime = stepmsec;
          ++currtrack;
        }
      }
      if (!hasActiveTracks) {
#ifdef VV_FLUID_DEBUG
        GCon->Logf("FluidSynth: no more active tracks");
#endif
        FluidManager::ResetSynth();
        break;
      }
      // update next update time
      nextFrameTic = stepframes;
    }
    int rdf = NumSamples;
    if (rdf > nextFrameTic) rdf = nextFrameTic;
    if (fluid_synth_write_s16(synth, rdf, Data, 0, 2, Data, 1, 2) != FLUID_OK) {
      GCon->Log(NAME_Error, "FluidSynth: error getting a sample, playback aborted!");
      for (int f = 0; f < midisong.tracks.length(); ++f) midisong.tracks[f].abort(true);
      break;
    }
    //GCon->Logf("FLUID: got a sample (total=%d, left=%d)!", res, NumSamples);
    Data += rdf*2;
    res += rdf*2;
    NumSamples -= rdf;
    nextFrameTic -= rdf;
    check(nextFrameTic >= 0);
  }
  if (res == 0) {
    FluidManager::ResetSynth();
#ifdef VV_FLUID_DEBUG
    GCon->Logf("FluidSynth: decode complete!");
#endif
  }
  return res;
}


//==========================================================================
//
//  VFluidAudioCodec::Finished
//
//==========================================================================
bool VFluidAudioCodec::Finished () {
  if (midisong.type != 2) {
    for (int f = 0; f < midisong.tracks.length(); ++f) {
      track_t &track = midisong.tracks[f];
      if (!track.isEOT()) return false;
      if (track.fadeoff > midisong.curtime) return false;
    }
    return true;
  } else {
    return (currtrack >= midisong.tracks.length());
  }
}


//==========================================================================
//
//  VFluidAudioCodec::Restart
//
//==========================================================================
void VFluidAudioCodec::Restart () {
  FluidManager::ResetSynth();
  midisong.setTempo(480000);
  midisong.curtime = 0;
  nextFrameTic = 0;
  for (int f = 0; f < midisong.tracks.length(); ++f) {
    track_t &track = midisong.tracks[f];
    check(track.getSong() == &midisong);
    track.reset();
    if (!track.isEOT()) track.nexttic = track.getSong()->tic2ms(track.getDeltaTic());
  }
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
  char Header[4];
  InStrm->Seek(0);
  InStrm->Serialise(Header, 4);
  if (InStrm->IsError() || memcmp(Header, "MThd", 4)) return nullptr;

  vuint32 hdrSize = 0;
  vuint16 type = 0;
  vuint16 ntracks = 0;
  vuint16 divisions = 0;

  InStrm->SerialiseBigEndian(&hdrSize, 4);
  InStrm->SerialiseBigEndian(&type, 2);
  InStrm->SerialiseBigEndian(&ntracks, 2);
  InStrm->SerialiseBigEndian(&divisions, 2);
  if (InStrm->IsError()) return nullptr;
  if (hdrSize != 6) { GCon->Logf(NAME_Warning, "invalid midi header size for '%s'", *InStrm->GetName()); return nullptr; }
  if (type > 2) { GCon->Logf(NAME_Warning, "invalid midi type for '%s'", *InStrm->GetName()); return nullptr; }
  if (divisions == 0 || divisions >= 0x7fffu) { GCon->Logf(NAME_Warning, "midi SMPTE timing is not supported for '%s'", *InStrm->GetName()); return nullptr; }

  if (!fluidManager.InitFluid()) return nullptr;

  // load song
  vuint8 *data = (vuint8 *)Z_Malloc(size);
  InStrm->Seek(0);
  InStrm->Serialise(data, size);
  if (InStrm->IsError()) {
    Z_Free(data);
    GCon->Logf(NAME_Warning, "Failed to load MIDI song '%s'", *InStrm->GetName());
    return nullptr;
  }

  InStrm->Close();
  delete InStrm;

  // create codec
  return new VFluidAudioCodec(data, size);
}
