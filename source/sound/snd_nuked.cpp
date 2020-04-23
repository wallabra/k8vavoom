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
//**  Copyright (C) 2018-2020 Ketmar Dark
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
#include "gamedefs.h"
#include "snd_local.h"

#include "nukedopl/musmid.h"


class VNukedOPLAudioCodec : public VAudioCodec {
public:
  TArray<vuint8> songdata;
  TArray<vint16> tempbuf;
  bool eos;

public:
  VNukedOPLAudioCodec (VStream *InStrm);
  virtual ~VNukedOPLAudioCodec () override;

  bool Init ();

  virtual int Decode (short *Data, int NumSamples) override;
  virtual bool Finished () override;
  virtual void Restart () override;

  static VAudioCodec *Create (VStream *InStrm);

private:
  static OPLPlayer musplr;
  static TArray<vuint8> genmidi;
  static int gmlumpTried;
  static void loadGenMIDI ();
};


IMPLEMENT_AUDIO_CODEC(VNukedOPLAudioCodec, "NukedOPL");

static VCvarI snd_nukedopl_type("snd_nukedopl_type", "1", "OPL chip type (0:OPL2; 1:OPL3)", CVAR_Archive);
static VCvarB snd_nukedopl_stereo("snd_nukedopl_stereo", true, "Use stereo mode?", CVAR_Archive);
static VCvarF snd_nukedopl_amplify("snd_nukedopl_amplify", "1", "Sound amplification.", CVAR_Archive);

TArray<vuint8> VNukedOPLAudioCodec::genmidi;
int VNukedOPLAudioCodec::gmlumpTried = 0;
OPLPlayer VNukedOPLAudioCodec::musplr;


//==========================================================================
//
//  VNukedOPLAudioCodec::loadGenMIDI
//
//==========================================================================
void VNukedOPLAudioCodec::loadGenMIDI () {
  if (gmlumpTried) return;
  gmlumpTried = -1;
  int lump = W_CheckNumForName("genmidi");
  if (lump < 0) {
    GCon->Log(NAME_Warning, "\"genmidi\" lump not found, OPL synthesis disabled.");
    return;
  }
  VStream *strm = W_CreateLumpReaderNum(lump);
  if (!strm) return;
  int size = strm->TotalSize();
  if (size < 8 || size > 65536 || strm->IsError()) {
    strm->Close();
    delete strm;
    GCon->Log(NAME_Warning, "\"genmidi\" has invalid size, OPL synthesis disabled.");
    return;
  }
  genmidi.setLength(size);
  strm->Serialise(genmidi.ptr(), size);
  bool err = strm->IsError();
  strm->Close();
  delete strm;
  if (err) {
    genmidi.clear();
    GCon->Log(NAME_Warning, "cannot load \"genmidi\", OPL synthesis disabled.");
    return;
  }

  GCon->Logf("loaded \"genmidi\" from '%s'", *W_FullLumpName(lump));
  gmlumpTried = 1;
}


//==========================================================================
//
//  VNukedOPLAudioCodec::VNukedOPLAudioCodec
//
//==========================================================================
VNukedOPLAudioCodec::VNukedOPLAudioCodec (VStream *InStrm)
  : eos(false)
{
  if (!gmlumpTried) loadGenMIDI();
  if (gmlumpTried < 0) return; // no "genmidi" lump

  // (re)init player
  musplr.sendConfig(48000, (snd_nukedopl_type.asInt() > 0), snd_nukedopl_stereo.asBool());
  if (!musplr.isGenMIDILoaded()) {
    if (!musplr.loadGenMIDI(genmidi.ptr(), (size_t)genmidi.length())) {
      GCon->Log(NAME_Warning, "cannot parse \"genmidi\" lump, OPL synthesis disabled.");
      gmlumpTried = -1; // do not try it anymore
      return;
    }
  }

  // load song data
  InStrm->Seek(0);
  int size = InStrm->TotalSize();
  if (size < 8 || size > 1024*1024*8 || InStrm->IsError()) return;
  songdata.setLength(size);
  InStrm->Serialise(songdata.ptr(), size);
  if (InStrm->IsError()) { songdata.clear(); }
}


//==========================================================================
//
//  VNukedOPLAudioCodec::~VNukedOPLAudioCodec
//
//==========================================================================
VNukedOPLAudioCodec::~VNukedOPLAudioCodec () {
}


//==========================================================================
//
//  VNukedOPLAudioCodec::Init
//
//==========================================================================
bool VNukedOPLAudioCodec::Init () {
  eos = true;
  if (songdata.length() < 8) return false;
  if (!musplr.load(songdata.ptr(), (size_t)songdata.length())) return false;
  if (!musplr.play()) return false;
  eos = false;
  SampleRate = 48000;
  NumChannels = 2;
  return true;
}


//==========================================================================
//
//  VNukedOPLAudioCodec::Decode
//
//==========================================================================
int VNukedOPLAudioCodec::Decode (short *Data, int NumSamples) {
  if (eos || !musplr.isPlaying()) return 0;
  int scount = (int)musplr.generate(Data, NumSamples*2, true); // always generate stereo output
  eos = (scount < NumSamples);
  if (scount) {
    const float amp = clampval(snd_nukedopl_amplify.asFloat(), 0.0f, 8.0f);
    if (amp != 1.0f) {
      short *d = Data;
      for (int f = 0; f < NumSamples*2; ++f) {
        const float v = clampval((*d)/32767.0f*amp, -1.0f, 1.0f);
        *d++ = (short)(v*32767.0f);
      }
    }
  }
  return scount;
}


//==========================================================================
//
//  VNukedOPLAudioCodec::Finished
//
//==========================================================================
bool VNukedOPLAudioCodec::Finished () {
  return eos;
}


//==========================================================================
//
//  VNukedOPLAudioCodec::Restart
//
//==========================================================================
void VNukedOPLAudioCodec::Restart () {
  eos = false;
  musplr.sendConfig(48000, (snd_nukedopl_type.asInt() > 0), snd_nukedopl_stereo.asBool());
  musplr.load(songdata.ptr(), (size_t)songdata.length());
  musplr.play();
}


//==========================================================================
//
//  VNukedOPLAudioCodec::Create
//
//==========================================================================
VAudioCodec *VNukedOPLAudioCodec::Create (VStream *InStrm) {
  if (snd_mid_player != 3) return nullptr; // we are the latest one

  VNukedOPLAudioCodec *codec = new VNukedOPLAudioCodec(InStrm);
  if (!codec->Init()) {
    delete codec;
    return nullptr;
  }

  // delete stream
  InStrm->Close();
  delete InStrm;

  // return codec
  return codec;
}
