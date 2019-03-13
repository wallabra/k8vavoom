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

#include "timidity/timidity.h"

using namespace LibTimidity;


class VTimidityAudioCodec : public VAudioCodec {
public:
  MidiSong *Song;

  static ControlMode  MyControlMode;

  VTimidityAudioCodec (MidiSong *InSong);
  virtual ~VTimidityAudioCodec () override;
  virtual int Decode (short *Data, int NumSamples) override;
  virtual bool Finished () override;
  virtual void Restart () override;

  // control mode functions.
  static int ctl_msg (int, int, const char *, ...);

  static VAudioCodec *Create (VStream *InStrm);
};


IMPLEMENT_AUDIO_CODEC(VTimidityAudioCodec, "Timidity");

ControlMode VTimidityAudioCodec::MyControlMode = {
  VTimidityAudioCodec::ctl_msg,
};

#if defined(_WIN32)
static VCvarS s_timidity_patches("snd_timidity_patches", "\\TIMIDITY", "Path to timidity patches.", CVAR_Archive|CVAR_PreInit);
#else
static VCvarS s_timidity_patches("snd_timidity_patches", "/usr/share/timidity", "Path to timidity patches.", CVAR_Archive|CVAR_PreInit);
#endif
static VCvarS s_timidity_sf2_file("snd_timidity_sf2_file", "", "Some timidity crap.", CVAR_Archive|CVAR_PreInit);
static VCvarI s_timidity_verbosity("snd_timidity_verbosity", "0", "Some timidity crap.", CVAR_Archive|CVAR_PreInit);

static VStr lastSF2Used = VStr();


//==========================================================================
//
//  VTimidityAudioCodec::VTimidityAudioCodec
//
//==========================================================================
VTimidityAudioCodec::VTimidityAudioCodec(MidiSong *InSong)
  : Song(InSong)
{
  Timidity_SetVolume(Song, 100);
  Timidity_Start(Song);
}


//==========================================================================
//
//  VTimidityAudioCodec::~VTimidityAudioCodec
//
//==========================================================================
VTimidityAudioCodec::~VTimidityAudioCodec () {
  Timidity_Stop(Song);
  if (Song->patches) Timidity_FreeDLS(Song->patches);
  if (Song->sf2_font) Timidity_FreeSf2(Song->sf2_font);
  Timidity_FreeSong(Song);
  Timidity_Close();
}


//==========================================================================
//
//  VTimidityAudioCodec::Decode
//
//==========================================================================
int VTimidityAudioCodec::Decode (short *Data, int NumSamples) {
  return Timidity_PlaySome(Song, Data, NumSamples);
}


//==========================================================================
//
//  VTimidityAudioCodec::Finished
//
//==========================================================================
bool VTimidityAudioCodec::Finished () {
  return !Timidity_Active(Song);
}


//==========================================================================
//
//  VTimidityAudioCodec::Restart
//
//==========================================================================
void VTimidityAudioCodec::Restart () {
  Timidity_Start(Song);
}


//==========================================================================
//
//  Minimal control mode -- no interaction, just stores messages.
//
//==========================================================================
int VTimidityAudioCodec::ctl_msg (int type, int verbosity_level, const char *fmt, ...) {
  if (s_timidity_verbosity < 0) return 0;
  char Buf[1024];
  va_list ap;
  if ((type == CMSG_TEXT || type == CMSG_INFO || type == CMSG_WARNING) && s_timidity_verbosity < verbosity_level) return 0;
  va_start(ap, fmt);
  vsnprintf(Buf, sizeof(Buf), fmt, ap);
  size_t slen = strlen(Buf);
  while (slen > 0 && (Buf[slen-1] == '\r' || Buf[slen-1] == '\n')) --slen;
  Buf[slen] = 0;
  GCon->Log(Buf);
  va_end(ap);
  return 0;
}


//==========================================================================
//
//  VTimidityAudioCodec::Create
//
//==========================================================================
VAudioCodec *VTimidityAudioCodec::Create (VStream *InStrm) {
  if (snd_mid_player != 0) return nullptr;
  // check if it's a MIDI file
  char Header[4];
  InStrm->Seek(0);
  InStrm->Serialise(Header, 4);
  if (memcmp(Header, MIDIMAGIC, 4)) return nullptr;

  // register our control mode
  ctl = &MyControlMode;

  // initialise Timidity
  add_to_pathlist(s_timidity_patches);
  DLS_Data *patches = nullptr;

  Timidity_Init();

  Sf2Data *sf2_data = nullptr;
  VStr sf2name = VStr(s_timidity_sf2_file);
  if (sf2name.length()) {
    sf2_data = Timidity_LoadSF2(*sf2name);
    if (lastSF2Used != sf2name) {
      lastSF2Used = sf2name;
      if (sf2_data) {
        GCon->Logf("TIMIDITY: loaded SF2: '%s'", *sf2name);
      } else {
        GCon->Logf("TIMIDITY: SF2 loading failed for '%s'", *sf2name);
      }
    }
  }

#ifdef _WIN32
  // try "!/gzdoom.sf2"
  if (!sf2_data) {
    VStr gzsf2 = VStr(GArgs[0])+"/gzdoom.sf2";
    sf2_data = Timidity_LoadSF2(*gzsf2);
    if (sf2_data && lastSF2Used != gzsf2) {
      GCon->Log("TIMIDITY: loaded SF2: 'gzdoom.sf2'");
      lastSF2Used = gzsf2;
    }
  }
#elif defined(__SWITCH__)
  // try "/switch/vavoom/gzdoom.sf2"
  if (!sf2_data) {
    VStr gzsf2 = "/switch/vavoom/gzdoom.sf2";
    sf2_data = Timidity_LoadSF2(*gzsf2);
    if (sf2_data && lastSF2Used != gzsf2) {
      GCon->Log("TIMIDITY: loaded SF2: 'gzdoom.sf2'");
      lastSF2Used = gzsf2;
    }
  }
#endif

  if (!sf2_data) {
    if (Timidity_ReadConfig() != 0) {
#ifdef _WIN32
      VStr GMPath = VStr(getenv("WINDIR"))+"/system32/drivers/gm.dls";
      FILE *f = fopen(*GMPath, "rb");
      if (f) {
        patches = Timidity_LoadDLS(f);
        fclose(f);
        if (patches) GCon->Logf("TIMIDITY: Loaded gm.dls");
      }
      if (!patches) {
        GCon->Logf("Timidity init failed");
        return nullptr;
      }
#else
      GCon->Logf("Timidity init failed");
      return nullptr;
#endif
    }
  }

  // load song
  int Size = InStrm->TotalSize();
  void *Data = Z_Malloc(Size);
  InStrm->Seek(0);
  InStrm->Serialise(Data, Size);
  MidiSong *Song = Timidity_LoadSongMem(Data, Size, patches, sf2_data);
  Z_Free(Data);
  if (!Song) {
    GCon->Logf("Failed to load MIDI song");
    Timidity_Close();
    return nullptr;
  }
  InStrm->Close();
  delete InStrm;
  InStrm = nullptr;

  // create codec
  return new VTimidityAudioCodec(Song);
}
