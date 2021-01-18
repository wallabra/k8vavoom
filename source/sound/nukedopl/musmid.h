//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2020-2021 Ketmar Dark
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
#ifndef VV_MUSMID
#define VV_MUSMID

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

#include "opl3.h"

typedef _opl3_chip  OPL3Chip;


// ////////////////////////////////////////////////////////////////////////// //
// GenMidi lump structure
struct __attribute__((packed)) GenMidi {
public:
  struct __attribute__((packed)) Operator {
    uint8_t mult; /* Tremolo / vibrato / sustain / KSR / multi */
    uint8_t attack; /* Attack rate / decay rate */
    uint8_t sustain; /* Sustain level / release rate */
    uint8_t wave; /* Waveform select */
    uint8_t ksl; /* Key scale level */
    uint8_t level; /* Output level */
    uint8_t feedback; /* Feedback for modulator, unused for carrier */
  };
  static_assert(sizeof(Operator) == 7, "invalid GenMidi::Operator size");

  struct __attribute__((packed)) Voice {
    Operator mod; /* modulator */
    Operator car; /* carrier */
    /* Base note offset. This is used to offset the MIDI note values.
       Several of the GENMIDI instruments have a base note offset of -12,
       causing all notes to be offset down by one octave. */
    int16_t offset;
  };
  static_assert(sizeof(Voice) == 7*2+2, "invalid GenMidi::Voice size");

  struct __attribute__((packed)) Patch {
  public:
    enum Flag /*: uint16_t*/ {
      Fixed     = 0x01u,
      DualVoice = 0x04u,
    };

  public:
    /* bit 0: Fixed pitch - Instrument always plays the same note.
              Most MIDI instruments play a note that is specified in the MIDI "key on" event,
              but some (most notably percussion instruments) always play the same fixed note.
       bit 1: Unknown - used in instrument #65 of the Doom GENMIDI lump.
       bit 2: Double voice - Play two voices simultaneously. This is used even on an OPL2 chip.
              If this is not set, only the first voice is played. If it is set, the fine tuning
              field (see below) can be used to adjust the pitch of the second voice relative to
              the first.
    */
    uint16_t flags;
    /* Fine tuning - This normally has a value of 128, but can be adjusted to adjust the tuning of
       the instrument. This field only applies to the second voice of double-voice instruments;
       for single voice instruments it has no effect. The offset values are similar to MIDI pitch
       bends; for example, a value of 82 (hex) in this field is equivalent to a MIDI pitch bend of +256.
     */
    uint8_t finetune;
    /* Note number used for fixed pitch instruments */
    uint8_t note;
    Voice voice[2];
  };
  static_assert(sizeof(Patch) == 2+1+1+(7*2+2)*2, "invalid GenMidi::Patch size");

public:
  enum { PatchCount = 175 };
  Patch patch[PatchCount];
};
static_assert(sizeof(GenMidi) == (2+1+1+(7*2+2)*2)*GenMidi::PatchCount, "invalid GenMidi::Patch size");


// ////////////////////////////////////////////////////////////////////////// //
// simple DooM MUS / Midi player
struct OPLPlayer {
public:
  enum { DefaultTempo = 140 };

private:
  struct SynthMidiChannel {
    uint8_t volume;
    uint16_t volume_t;
    uint8_t pan;
    uint8_t reg_pan;
    uint8_t pitch;
    const GenMidi::Patch* patch;
    bool drum;
  };

  struct SynthVoice {
    uint8_t bank;

    uint8_t op_base;
    uint8_t ch_base;

    uint32_t freq;

    uint8_t tl[2];
    uint8_t additive;

    bool voice_dual;
    const GenMidi::Voice* voice_data;
    const GenMidi::Patch* patch;

    SynthMidiChannel* chan;

    uint8_t velocity;
    uint8_t key;
    uint8_t note;

    int32_t finetune;

    uint8_t pan;
  };

  struct __attribute__((packed)) MidiHeader {
    char header[4];
    uint32_t length;
    uint16_t format;
    uint16_t count;
    uint16_t time;
    uint8_t data[0];
  };

  struct __attribute__((packed)) MidiTrack {
    char header[4];
    uint32_t length;
    uint8_t data[0];
  };

  struct __attribute__((packed)) Track {
    const uint8_t* data;
    const uint8_t* pointer;
    uint32_t length;
    uint32_t time;
    uint8_t lastevent;
    bool finish;
    uint32_t num;
  };

  struct __attribute__((packed)) MusHeader {
    char header[4];
    uint16_t length;
    uint16_t offset;
  };

private:
  // config
  uint32_t mSampleRate;
  bool mOPL2Mode;
  bool mStereo;

  // genmidi lump
  GenMidi mGenMidi;
  bool mGenMidiLoaded;

  // OPL3 emulator
  OPL3Chip chip;

  SynthMidiChannel mSynthMidiChannels[16];
  SynthVoice mSynthVoices[18];
  uint32_t mSynthVoiceNum;
  SynthVoice* mSynthVoicesAllocated[18];
  uint32_t mSynthVoicesAllocatedNum;
  SynthVoice* mSynthVoicesFree[18];
  uint32_t mSynthVoicesFreeNum;

  Track *mMidiTracks;
  uint32_t mMidiTracksCount;
  uint32_t mMidiCount;
  uint32_t mMidiTimebase;
  uint32_t mMidiCallrate;
  uint32_t mMidiTimer;
  uint32_t mMidiTimechange;
  uint32_t mMidiRate;
  uint32_t mMidiFinished;
  uint8_t mMidiChannels[16];
  uint8_t mMidiChannelcnt;

  const uint8_t* mMusData;
  const uint8_t* mMusPointer;
  uint16_t mMusLength;
  uint32_t mMusTimer;
  uint32_t mMusTimeend;
  uint8_t mMusChanVelo[16];

  uint32_t mSongTempo;
  bool mPlayerActive;
  bool mPlayLooped;

  enum DataFormat {
    Unknown,
    Midi,
    Mus,
  };
  DataFormat mDataFormat = DataFormat::Unknown;

  uint32_t mOPLCounter;

  uint8_t *songdata;
  uint32_t songlen;

private:
  static inline uint16_t MISC_Read16LE (uint16_t n) noexcept {
    const uint8_t* m = (const uint8_t *)&n;
    return (uint16_t)((uint32_t)m[0]|((uint32_t)m[1]<<8));
  }

  static inline uint16_t MISC_Read16BE (uint16_t n) noexcept {
    const uint8_t* m = (const uint8_t *)&n;
    return (uint16_t)((uint32_t)m[1]|((uint32_t)m[0]<<8));
  }

  static inline uint32_t MISC_Read32LE (uint32_t n) noexcept {
    const uint8_t* m = (const uint8_t *)&n;
    return (uint32_t)m[0]|((uint32_t)m[1]<<8)|((uint32_t)m[2]<<16)|((uint32_t)m[3]<<24);
  }

  static inline uint32_t MISC_Read32BE (uint32_t n) noexcept {
    const uint8_t* m = (const uint8_t *)&n;
    return (uint32_t)m[3]|((uint32_t)m[2]<<8)|((uint32_t)m[1]<<16)|((uint32_t)m[0]<<24);
  }

  // ////////////////////////////////////////////////////////////////////// //
  // synth
  enum SynthCmd /*: uint8_t*/ {
    NoteOff,
    NoteOn,
    PitchBend,
    Patch,
    Control,
  };

  enum SynthCtl /*: uint8_t*/ {
    Bank,
    Modulation,
    Volume,
    Pan,
    Expression,
    Reverb,
    Chorus,
    Sustain,
    Soft,
    AllNoteOff,
    MonoMode,
    PolyMode,
    Reset,
  };

  void SynthResetVoice (SynthVoice &voice);
  void SynthResetChip (OPL3Chip &chip);
  void SynthResetMidi (SynthMidiChannel &channel);
  void SynthInit ();
  void SynthWriteReg (uint32_t bank, uint16_t reg, uint8_t data);
  void SynthVoiceOff (SynthVoice* voice);
  void SynthVoiceFreq (SynthVoice* voice);
  void SynthVoiceVolume (SynthVoice* voice);
  uint8_t SynthOperatorSetup (uint32_t bank, uint32_t base, const GenMidi::Operator* op, bool volume);
  void SynthVoiceOn (SynthMidiChannel* channel, const GenMidi::Patch* patch, bool dual, uint8_t key, uint8_t velocity);
  void SynthKillVoice ();
  void SynthNoteOff (SynthMidiChannel* channel, uint8_t note);
  void SynthNoteOn (SynthMidiChannel* channel, uint8_t note, uint8_t velo);
  void SynthPitchBend (SynthMidiChannel* channel, uint8_t pitch);
  void SynthUpdatePatch (SynthMidiChannel* channel, uint8_t patch);
  void SynthUpdatePan (SynthMidiChannel* channel, uint8_t pan);
  void SynthUpdateVolume (SynthMidiChannel* channel, uint8_t volume);
  void SynthNoteOffAll (SynthMidiChannel* channel);
  void SynthEventReset (SynthMidiChannel* channel);
  void SynthReset ();
  void SynthWrite (uint8_t command, uint8_t data1, uint8_t data2);

  // ////////////////////////////////////////////////////////////////////// //
  // MIDI
  uint32_t MIDI_ReadDelay (const uint8_t** data);
  bool MIDI_LoadSong ();
  bool MIDI_StartSong ();
  void MIDI_StopSong ();
  Track* MIDI_NextTrack ();
  uint8_t MIDI_GetChannel (uint8_t chan);
  void MIDI_Command (const uint8_t** datap, uint8_t evnt);
  void MIDI_FinishTrack (Track* trck);
  void MIDI_AdvanceTrack (Track* trck);
  void MIDI_Callback ();

  // ////////////////////////////////////////////////////////////////////// //
  // MUS
  void MUS_Callback ();
  bool MUS_LoadSong ();
  bool MUS_StartSong ();
  void MUS_StopSong ();

  // ////////////////////////////////////////////////////////////////////// //
  void PlayerInit ();

  void clearSong ();

public:
  OPLPlayer (int32_t asamplerate=48000, bool aopl3mode=true, bool astereo=true);

  // WARNING! this will unload current song!
  void sendConfig (int32_t asamplerate, bool aopl3mode, bool astereo);

  // load "genmidi" lump first
  // data will be copied, so it can be freed after calling this
  bool loadGenMIDI (const void *data, size_t datasize);

  // data will be copied, so it can be freed after calling this
  // this will unload current song
  bool load (const void *data, size_t datasize);

  inline bool isGenMIDILoaded () const noexcept { return mGenMidiLoaded; }

  inline void setTempo (uint32_t atempo) noexcept {
    if (atempo < 1) atempo = 1; else if (atempo > 255) atempo = 255;
    mSongTempo = atempo;
  }
  inline uint32_t getTempo () const noexcept { return mSongTempo; }

  inline void setLooped (bool loop) noexcept { mPlayLooped = loop; }
  inline bool isLooped () const noexcept { return mPlayLooped; }

  inline bool isLoaded () const noexcept { return (mDataFormat != DataFormat::Unknown); }
  inline bool isPlaying () const noexcept { return mPlayerActive; }

  inline void setStereo (bool v) noexcept { mStereo = v; }
  inline bool isStereo () const noexcept { return mStereo; }

  // returns `false` if song cannot be started (or if it is already playing)
  bool play ();
  void stop ();

  // return number of generated *frames*
  // returns 0 if song is complete (and player is not looped)
  // if `bufIsStereo`, always generate interleaved stereo
  // `buflen` is in samples, not in frames
  uint32_t generate (int16_t *buffer, size_t buflen, bool bufIsStereo=false);
};


#endif
