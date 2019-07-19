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


static VCvarB snd_fluid_midi_messages("snd_fluid_midi_messages", false, "Show messages from MIDI files?", CVAR_Archive);


//==========================================================================
//
//  MIDIData::MIDIData
//
//==========================================================================
MIDIData::MIDIData ()
  : type(0xff)
  , delta(0)
  , tempo(480000)
  , currtime(0)
  , currtrack(0)
  , midiData(nullptr)
  , dataSize(0)
{
}


//==========================================================================
//
//  MIDIData::~MIDIData
//
//==========================================================================
MIDIData::~MIDIData () {
  clear();
}


//==========================================================================
//
//  MIDIData::clear
//
//==========================================================================
void MIDIData::clear () {
  for (auto &&track : tracks) track.reset();
  tracks.clear();
  type = 0xff;
  delta = 0;
  tempo = 480000;
  currtime = 0;
  currtrack = 0;
  if (midiData) Z_Free(midiData);
  midiData = nullptr;
  dataSize = 0;
}


//==========================================================================
//
//  MIDIData::restart
//
//==========================================================================
void MIDIData::restart () {
  setTempo(480000);
  currtime = 0;
  currtrack = 0;
  for (auto &&track : tracks) {
    check(track.getSong() == this);
    track.reset();
    // perform first advance
    if (!track.isEndOfData()) track.advanceTics(track.getDeltaTic());
  }
}


//==========================================================================
//
//  MIDIData::isMidiStream
//
//==========================================================================
bool MIDIData::isMidiStream (VStream &strm) {
  // check if it's a MIDI file
  char header[4];

  if (strm.IsError()) return false;
  int stpos = strm.Tell();
  strm.Serialise(header, 4);
  if (strm.IsError()) return false;
  if (memcmp(header, "MThd", 4) != 0) { strm.Seek(stpos); return false; }

  vuint32 hdrSize = 0;
  vuint16 type = 0;
  vuint16 ntracks = 0;
  vuint16 divisions = 0;

  strm.SerialiseBigEndian(&hdrSize, 4);
  strm.SerialiseBigEndian(&type, 2);
  strm.SerialiseBigEndian(&ntracks, 2);
  strm.SerialiseBigEndian(&divisions, 2);
  if (strm.IsError()) return false;
  strm.Seek(stpos);

  // MIDIs with invalid header will be considered empty
  if (hdrSize != 6) { GCon->Logf(NAME_Warning, "invalid MIDI header size for '%s'", *strm.GetName()); return true; }
  if (type > 2) { GCon->Logf(NAME_Warning, "invalid MIDI type for '%s'", *strm.GetName()); return true; }
  if (divisions == 0 || divisions >= 0x7fffu) { GCon->Logf(NAME_Warning, "MIDI SMPTE timing is not supported for '%s'", *strm.GetName()); return true; }
  if (ntracks > 16384) { GCon->Logf(NAME_Warning, "too many MIDI tracks for '%s'", *strm.GetName()); return true; }

  return true;
}


//==========================================================================
//
//  MIDIData::parseMem
//
//==========================================================================
bool MIDIData::parseMem () {
  setTempo(480000);

  if (!midiData || dataSize < 14) return false;

  vuint8 *indata = midiData;
  int inleft = dataSize;

  vuint16 ntracks;
  char header[4];
  vuint32 chunksize;

  memcpy(&header[0], indata, 4); indata += 4;
  if (memcmp(header, "MThd", 4) != 0) return false;

  memcpy(&chunksize, indata, 4); indata += 4;
  memcpy(&type, indata, 2); indata += 2;
  memcpy(&ntracks, indata, 2); indata += 2;
  memcpy(&delta, indata, 2); indata += 2;
  inleft -= 4+4+2+2+2;

  chunksize = BigLong(chunksize);
  type = BigShort(type);
  ntracks = BigShort(ntracks);
  delta = BigShort(delta);

  if (chunksize != 6 || type > 2 || delta == 0 || delta > 0x7fffu || ntracks > 16384) {
    // invalid midi file; consider it empty
    clear();
    return true;
  }

  if (ntracks > 0) {
    // collect tracks
    tracks.resize(ntracks);
    while (tracks.length() < ntracks) {
      if (inleft < 8) break;
      memcpy(header, indata, 4); indata += 4;
      memcpy(&chunksize, indata, 4); indata += 4;
      inleft -= 8;
      chunksize = BigLong(chunksize);
      if (chunksize > (vuint32)inleft) break;
      // ignore non-track chunks
      if (memcmp(header, "MTrk", 4) != 0) {
        bool ok = true;
        for (int f = 0; f < 4; ++f) {
          vuint8 ch = (vuint8)header[f];
          if (ch < 32 || ch >= 127) { ok = false; break; }
        }
        if (!ok) break;
      } else {
        MidiTrack &track = tracks.alloc();
        memset((void *)&track, 0, sizeof(MidiTrack));
        track.setup(this, indata, (vint32)chunksize);
#ifdef VV_FLUID_DEBUG_DUMP_TRACKS
        GCon->Logf("  track #%d: %u bytes", tracks.length()-1, track.size());
        for (int f = 0; f < track.size(); ++f) GCon->Logf("    %5d: 0x%02x", f, track[f]);
#endif
      }
      inleft -= chunksize;
      indata += chunksize;
    }
  }

#ifdef VV_FLUID_DEBUG
  GCon->Logf("Fluid: %d tracks in song", tracks.length());
#endif

  return true;
}


//==========================================================================
//
//  MIDIData::parseStream
//
//==========================================================================
bool MIDIData::parseStream (VStream &strm) {
  clear();
  check(!midiData);
  if (strm.IsError()) return false;
  dataSize = strm.TotalSize()-strm.Tell();
  if (dataSize < 14) { dataSize = 0; return false; }
  // load song
  midiData = (vuint8 *)Z_Malloc(dataSize);
  strm.Serialise(midiData, dataSize);
  if (strm.IsError()) {
    clear();
    GCon->Logf(NAME_Warning, "error loading MIDI song '%s'", *strm.GetName());
    return false;
  }

  if (!parseMem()) { clear(); return false; }
  return true;
}




//==========================================================================
//
//  MIDIData::runTrack
//
//  main midi parsing routine
//  returns `false` if the track is complete
//
//==========================================================================
bool MIDIData::runTrack (int tidx, EventCBType cb, void *udata) {
  if (tidx < 0 || tidx >= tracks.length()) return false;

  MidiTrack &track = tracks[tidx];
  check(track.getSong() == this);

  if (currtime < track.nextEventTime()) return true;

  // we are in "post-track pause" now, and it was just expired
  if (track.isEndOfData()) return false;

#ifdef VV_FLUID_DEBUG_TICKS
  GCon->Logf("FLUID: TICK for channel #%d (stime=%g; ntt=%g; eot=%d)", tidx, currtime, track.nextEventTime(), (track.isEndOfData() ? 1 : 0));
#endif

  // keep parsing through midi track until
  // the end is reached or until it reaches next delta time
  while (!track.isEndOfData() && currtime >= track.nextEventTime()) {
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
    GCon->Logf("EVENT: tidx=%d; pos=%d; len=%d; left=%d; event=0x%02x; nt=%g; ct=%g", tidx, track.getPos(), track.size(), track.getLeft(), evcode, track.nextEventTime(), currtime);
#endif

    if (evcode == MIDI_SYSEX) {
      // system exclusive
      // read the length of the message
      vuint32 len = track.readVarLen();
      // check for valid length
      if (len == 0 || len > (vuint32)track.getLeft()) { track.abort(true); return false; }
      if (len > 1 && track[len-1] == MIDI_EOX) {
        // generate event
        if (cb) {
          MidiEvent mev;
          mev.type = MIDI_SYSEX;
          mev.data1 = len-1;
          mev.payload = track.getCurPosPtr();
          cb(track.nextEventTime(), mev, udata);
        }
      } else {
        // oops, incomplete packet, ignore it
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
            setTempo(t);
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
        case MIDI_SEQUENCER_EVENT:
          if (cb) {
            MidiEvent mev;
            mev.type = MIDI_SEQUENCER_EVENT;
            mev.data1 = len-1;
            mev.payload = track.getCurPosPtr();
            cb(track.nextEventTime(), mev, udata);
          }
          break;
        default:
          break;
      }
      track.skipNextMidiBytes(len);
    } else {
      //track.runningStatus = evcode;
      MidiEvent mev;
      mev.type = evcode&0xf0u;
      mev.channel = evcode&0x0fu;
      mev.data1 = track.getNextMidiByte();
      switch (mev.type) {
        case NOTE_OFF:
          mev.data2 = track.getNextMidiByte();
#ifdef VV_FLUID_DEBUG
          GCon->Logf("  (%u):NOTE_OFF: %u %u", mev.channel, mev.data1, mev.data2);
#endif
          break;
        case NOTE_ON:
          mev.data2 = track.getNextMidiByte();
#ifdef VV_FLUID_DEBUG
          GCon->Logf("  (%u):NOTE_N: %u %u", mev.channel, mev.data1, mev.data2);
#endif
          break;
        case KEY_PRESSURE:
          mev.data2 = track.getNextMidiByte();
#ifdef VV_FLUID_DEBUG
          GCon->Logf("  (%u):KEY_PRESSURE: %u %u", mev.channel, mev.data1, mev.data2);
#endif
          break;
        case CONTROL_CHANGE:
          mev.data2 = track.getNextMidiByte();
#ifdef VV_FLUID_DEBUG
          GCon->Logf("  (%u):CONTROL_CHANGE: %u %u", mev.channel, mev.data1, mev.data2);
#endif
          break;
        case PROGRAM_CHANGE:
#ifdef VV_FLUID_DEBUG
          GCon->Logf("  (%u):PROGRAM_CHANGE: %u", mev.channel, mev.data1);
#endif
          break;
        case CHANNEL_PRESSURE:
#ifdef VV_FLUID_DEBUG
          GCon->Logf("  (%u):CHANNEL_PRESSURE: %u", mev.channel, mev.data1);
#endif
          break;
        case PITCH_BEND: // pitch bend
          mev.data2 = track.getNextMidiByte();
          mev.data1 = (mev.data1&0x7f)|(((vuint32)(mev.data2&0x7f))<<7);
          mev.data2 = 0;
#ifdef VV_FLUID_DEBUG
          GCon->Logf("  (%u):PITCH_BEND: %u", channel, mev.data1);
#endif
          break;
        default:
          GCon->Logf(NAME_Warning, "invalid MIDI command encountered, aborting playback");
          //GCon->Logf(NAME_Warning, "  (%u):INVALID COMMAND! (0x%02x)", channel, event);
          track.abort(true);
          return false;
      }
      if (cb) cb(track.nextEventTime(), mev, udata);
    }
    // check for end of the track, otherwise get the next delta time
    if (!track.isEndOfData()) {
      vuint32 dtime = track.getDeltaTic();
#ifdef VV_FLUID_DEBUG
      GCon->Logf("  timedelta: %u (%g) (pos=%d)", dtime, tic2ms(dtime), track.getPos());
#endif
      track.advanceTics(dtime);
    }
  }

  return true;
}


//==========================================================================
//
//  MIDIData::abort
//
//==========================================================================
void MIDIData::abort () {
  for (auto &&track : tracks) track.abort(true);
}


//==========================================================================
//
//  MidiEvent::decodeStep
//
//  <0: error
//  0: done decoding
//  >0: number of frames one can generate until next event
//
//==========================================================================
int MIDIData::decodeStep (EventCBType cb, int SampleRate, void *udata) {
  if (!isValid()) return -1;
  if (SampleRate < 1) {
    for (auto &&track : tracks) track.abort(true);
    return -1;
  }
  for (;;) {
    double nextEventTime = -1;
    if (type != 2) {
      for (int f = 0; f < tracks.length(); ++f) {
        if (runTrack(f, cb, udata)) {
          double ntt = tracks[f].nextEventTime();
#ifdef VV_FLUID_DEBUG
          GCon->Logf("*** TRACK #%d: ntt=%g; mintt=%g (stime=%g)", f, ntt, nextEventTime, currtime);
#endif
          if (nextEventTime < 0 || nextEventTime > ntt) nextEventTime = ntt;
        }
      }
    } else {
      while (currtrack < tracks.length()) {
        if (runTrack(currtrack, cb, udata)) {
          nextEventTime = tracks[currtrack].nextEventTime();
          break;
        }
        currtime = 0;
        nextEventTime = -1;
        ++currtrack;
      }
    }
    if (nextEventTime < 0) {
#ifdef VV_FLUID_DEBUG
      GCon->Logf("FluidSynth: no more active tracks");
#endif
      return 0; // done decoding
    }
    if (nextEventTime <= currtime) {
      GCon->Logf(NAME_Error, "MIDI: internal error in MIDI decoder!");
      //GCon->Logf("FS: currtime=%g; nexttime=%g", currtime, nextEventTime);
      for (auto &&track : tracks) track.abort(true);
      return -1;
    }
    // calculate number of frames to generate before next event
    vint32 framesUntilEvent = (vint32)((nextEventTime-currtime)*SampleRate/1000.0);
    if (framesUntilEvent < 0) framesUntilEvent = 0; // just in case
#ifdef VV_FLUID_DEBUG
    GCon->Logf("FS: currtime=%g; nexttime=%g; delta=%g; frames=%d", currtime, nextEventTime, nextEventTime-currtime, framesUntilEvent);
#endif
    // advance virtual time
    currtime = nextEventTime;
    if (framesUntilEvent != 0) return framesUntilEvent;
    // nothing to generate yet, the step is too small
  }
}


//==========================================================================
//
//  MIDIData::isFinished
//
//==========================================================================
bool MIDIData::isFinished () {
  if (!isValid()) return true;
  if (type != 2) {
    for (auto &&track : tracks) {
      if (!track.isEOT()) return false;
    }
    return true;
  } else {
    return (currtrack >= tracks.length());
  }
}
