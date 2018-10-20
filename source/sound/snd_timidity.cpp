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
static VCvarS s_timidity_patches("snd_timidity_patches", "\\TIMIDITY", "Path to timidity patches.", CVAR_Archive);
#else
static VCvarS s_timidity_patches("snd_timidity_patches", "/usr/share/timidity", "Path to timidity patches.", CVAR_Archive);
#endif
static VCvarI s_timidity_test_sf2("snd_timidity_test_sf2", "0", "Some timidity crap.", CVAR_Archive);
static VCvarI s_timidity_verbosity("snd_timidity_verbosity", "0", "Some timidity crap.", CVAR_Archive);


//==========================================================================
//
//  VTimidityAudioCodec::VTimidityAudioCodec
//
//==========================================================================
VTimidityAudioCodec::VTimidityAudioCodec(MidiSong *InSong)
  : Song(InSong)
{
  guard(VTimidityAudioCodec::VTimidityAudioCodec);
  Timidity_SetVolume(Song, 100);
  Timidity_Start(Song);
  unguard;
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
  guard(VTimidityAudioCodec::Decode);
  return Timidity_PlaySome(Song, Data, NumSamples);
  unguard;
}


//==========================================================================
//
//  VTimidityAudioCodec::Finished
//
//==========================================================================
bool VTimidityAudioCodec::Finished () {
  guard(VTimidityAudioCodec::Finished);
  return !Timidity_Active(Song);
  unguard;
}


//==========================================================================
//
//  VTimidityAudioCodec::Restart
//
//==========================================================================
void VTimidityAudioCodec::Restart () {
  guard(VTimidityAudioCodec::Restart);
  Timidity_Start(Song);
  unguard;
}


//==========================================================================
//
//  Minimal control mode -- no interaction, just stores messages.
//
//==========================================================================
int VTimidityAudioCodec::ctl_msg (int type, int verbosity_level, const char *fmt, ...) {
  guard(VTimidityAudioCodec::ctl_msg);
  char Buf[1024];
  va_list ap;
  if ((type == CMSG_TEXT || type == CMSG_INFO || type == CMSG_WARNING) && s_timidity_verbosity < verbosity_level) return 0;
  va_start(ap, fmt);
  vsnprintf(Buf, sizeof(Buf), fmt, ap);
  GCon->Log(Buf);
  va_end(ap);
  return 0;
  unguard;
}


//==========================================================================
//
//  VTimidityAudioCodec::Create
//
//==========================================================================
VAudioCodec *VTimidityAudioCodec::Create (VStream *InStrm) {
  guard(VTimidityAudioCodec::Create);
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
  if (Timidity_Init()) {
#ifdef _WIN32
    VStr GMPath = VStr(getenv("WINDIR"))+"/system32/drivers/gm.dls";
    FILE *f = fopen(*GMPath, "rb");
    if (f) {
      patches = Timidity_LoadDLS(f);
      fclose(f);
      if (patches) GCon->Logf("Loaded gm.dls");
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

  Sf2Data *sf2_data = nullptr;
  if (s_timidity_test_sf2) {
    sf2_data = Timidity_LoadSF2("gm.sf2");
    if (sf2_data) GCon->Logf("Loaded SF2");
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
  unguard;
}
