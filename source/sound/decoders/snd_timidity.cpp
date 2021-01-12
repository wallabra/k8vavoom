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
//**  Copyright (C) 2018-2021 Ketmar Dark
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
#include "../../gamedefs.h"
#include "../snd_local.h"

#include "timidity/timidity.h"


using namespace LibTimidity;


class VTimidityAudioCodec : public VAudioCodec {
public:
  MidiSong *Song;

public:
  VTimidityAudioCodec (MidiSong *InSong);
  virtual ~VTimidityAudioCodec () override;
  virtual int Decode (vint16 *Data, int NumFrames) override;
  virtual bool Finished () override;
  virtual void Restart () override;

  static VAudioCodec *Create (VStream *InStrm, const vuint8 sign[], int signsize);

};


class TimidityManager {
public:
  static Sf2Data *sf2_data;
  static DLS_Data *patches;

public:
  static ControlMode MyControlMode;

  static int timidityInitialised; // <0: not yet; >0: ok; 0: failed

  // last values of some cvars
  static VStr patchesPath;
  static VStr sf2Path;
  static bool autoloadSF2;
  static bool needRestart;

protected:
  static bool NeedRestart ();
  static void UpdateCvarCache ();

  // control mode functions.
  static int ctl_msg (int, int, const char *, ...);

public:
  TimidityManager () {}
  ~TimidityManager () { CloseTimidity(); }

  // returns success flag
  static bool InitTimidity ();
  // WARNING! song must be freed!
  static void CloseTimidity ();
};

static TimidityManager timidityManager;


IMPLEMENT_AUDIO_CODEC(VTimidityAudioCodec, "Timidity");

ControlMode TimidityManager::MyControlMode = {
  TimidityManager::ctl_msg,
};


#if defined(_WIN32)
static VCvarS snd_timidity_patches("snd_timidity_patches", "\\TIMIDITY", "Path to timidity patches.", CVAR_Archive|CVAR_PreInit);
#else
static VCvarS snd_timidity_patches("snd_timidity_patches", "/usr/share/timidity", "Path to timidity patches.", CVAR_Archive|CVAR_PreInit);
#endif
static VCvarI snd_timidity_verbosity("snd_timidity_verbosity", "0", "Some timidity crap.", CVAR_Archive);

Sf2Data *TimidityManager::sf2_data = nullptr;
DLS_Data *TimidityManager::patches = nullptr;

int TimidityManager::timidityInitialised = -1;
VStr TimidityManager::patchesPath = VStr::EmptyString;
VStr TimidityManager::sf2Path = VStr::EmptyString;
bool TimidityManager::autoloadSF2 = false;
bool TimidityManager::needRestart = false;


//==========================================================================
//
//  TimidityManager::ctl_msg
//
//  minimal control mode -- no interaction, just stores messages
//
//==========================================================================
int TimidityManager::ctl_msg (int type, int verbosity_level, const char *fmt, ...) {
  if (snd_timidity_verbosity < 0) return 0;
  char Buf[1024];
  va_list ap;
  if ((type == CMSG_TEXT || type == CMSG_INFO || type == CMSG_WARNING) && snd_timidity_verbosity < verbosity_level) return 0;
  va_start(ap, fmt);
  vsnprintf(Buf, sizeof(Buf), fmt, ap);
  size_t slen = strlen(Buf);
  while (slen > 0 && (Buf[slen-1] == '\r' || Buf[slen-1] == '\n')) --slen;
  Buf[slen] = 0;
  if (Buf[0]) GCon->Log(Buf);
  va_end(ap);
  return 0;
}


//==========================================================================
//
//  TimidityManager::NeedRestart
//
//==========================================================================
bool TimidityManager::NeedRestart () {
  return
    needRestart ||
    patchesPath != snd_timidity_patches.asStr() ||
    sf2Path != snd_sf2_file.asStr() ||
    autoloadSF2 != snd_sf2_autoload.asBool();
}


//==========================================================================
//
//  TimidityManager::UpdateCvarCache
//
//==========================================================================
void TimidityManager::UpdateCvarCache () {
  patchesPath = snd_timidity_patches.asStr();
  sf2Path = snd_sf2_file.asStr();
  autoloadSF2 = snd_sf2_autoload.asBool();
  needRestart = false;
}


//==========================================================================
//
//  TimidityManager::CloseTimidity
//
//==========================================================================
void TimidityManager::CloseTimidity () {
  if (timidityInitialised == 0) {
    if (patches) Timidity_FreeDLS(patches);
    if (sf2_data) Timidity_FreeSf2(sf2_data);
    Timidity_Close();
    patches = nullptr;
    sf2_data = nullptr;
  }
  timidityInitialised = -1;
}


//==========================================================================
//
//  TimidityManager::InitTimidity
//
//==========================================================================
bool TimidityManager::InitTimidity () {
  if (NeedRestart()) CloseTimidity();

  if (timidityInitialised >= 0) return (timidityInitialised > 0);

  vassert(!patches);
  vassert(!sf2_data);

  if (autoloadSF2 != snd_sf2_autoload.asBool()) SF2_SetDiskScanned(false);

  UpdateCvarCache();

  // register our control mode
  ctl = &MyControlMode;

  // initialise Timidity
  add_to_pathlist(snd_timidity_patches);
  Timidity_Init();

  // load sf2
  VStr sf2name = snd_sf2_file.asStr();
  if (sf2name.length()) {
    sf2_data = Timidity_LoadSF2(*sf2name);
    if (sf2_data) {
      GCon->Logf("TIMIDITY: loaded SF2: '%s'", *sf2name);
    } else {
      GCon->Logf("TIMIDITY: SF2 loading failed for '%s'", *sf2name);
    }
  }

  SF2_ScanDiskBanks();

  // try to find sf2 in binary dir
  if (!sf2_data && snd_sf2_autoload) {
    TArray<VStr> failedBanks;
    // try to load a bank
    for (auto &&bfn : sf2FileList) {
      sf2name = bfn;
      if (sf2name.isEmpty()) break;
      if (sf2name.extractFileExtension().strEquCI(".sf2")) {
        // sf2
        sf2_data = Timidity_LoadSF2(*sf2name);
        if (sf2_data) {
          GCon->Logf("TIMIDITY: autoloaded SF2: '%s'", *sf2name);
          break;
        }
      } else {
        // dls
        FILE *fl = fopen(*sf2name, "rb");
        if (fl) {
          patches = Timidity_LoadDLS(fl);
          fclose(fl);
          if (patches) {
            GCon->Logf("TIMIDITY: autoloaded DLS: '%s'", *sf2name);
            break;
          }
        }
      }
      // oops
      failedBanks.append(sf2name);
      sf2name.clear();
    }

    if (!sf2_data && !patches && failedBanks.length()) {
      for (auto &&bfn : failedBanks) GCon->Logf("TIMIDITY: autoloading failed for '%s'", *bfn);
    }
  }

  // load patches if no sf2 was loaded
  if (!sf2_data && !patches) {
    if (Timidity_ReadConfig() != 0) {
      bool doLoading = false;
      for (auto &&gmpath : sf2FileList) {
        if (!doLoading) {
          if (gmpath.isEmpty()) doLoading = true;
        }
        if (!doLoading || gmpath.isEmpty()) continue;
        FILE *f = fopen(*gmpath, "rb");
        if (f) {
          patches = Timidity_LoadDLS(f);
          fclose(f);
          if (patches) {
            GCon->Logf("TIMIDITY: loaded '%s'", *gmpath.extractFileName());
            break;
          }
        }
      }

      if (!patches) {
        GCon->Logf(NAME_Warning, "Timidity init failed");
        Timidity_Close();
        timidityInitialised = 0;
        return false;
      }
    } else {
      needRestart = true;
    }
    //k8: dunno if we need a restart for DLS patchsets
  }

  timidityInitialised = 1;
  return true;
}


//==========================================================================
//
//  VTimidityAudioCodec::VTimidityAudioCodec
//
//==========================================================================
VTimidityAudioCodec::VTimidityAudioCodec (MidiSong *InSong)
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
  if (Song) {
    Timidity_Stop(Song);
    Timidity_FreeSong(Song);
    Song = nullptr;
  }
  /* this is not required anymore
  if (Song->patches) Timidity_FreeDLS(Song->patches);
  if (Song->sf2_font) Timidity_FreeSf2(Song->sf2_font);
  Timidity_FreeSong(Song);
  Timidity_Close();
  */
}


//==========================================================================
//
//  VTimidityAudioCodec::Decode
//
//==========================================================================
int VTimidityAudioCodec::Decode (vint16 *Data, int NumFrames) {
  return Timidity_PlaySome(Song, Data, NumFrames);
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
//  VTimidityAudioCodec::Create
//
//==========================================================================
VAudioCodec *VTimidityAudioCodec::Create (VStream *InStrm, const vuint8 sign[], int signsize) {
  if (snd_midi_player != 2) return nullptr;

  // check if it's a MIDI file
  if (memcmp(sign, MIDIMAGIC, 4)) return nullptr;

  int Size = InStrm->TotalSize();
  if (Size < 0x0e) {
    GCon->Logf(NAME_Warning, "Failed to load MIDI song");
    return nullptr;
  }

  if (!timidityManager.InitTimidity()) return nullptr;

  // load song
  void *Data = Z_Malloc(Size);
  InStrm->Seek(0);
  InStrm->Serialise(Data, Size);
  if (InStrm->IsError()) {
    GCon->Logf(NAME_Warning, "Failed to load MIDI song");
    return nullptr;
  }
  MidiSong *Song = Timidity_LoadSongMem(Data, Size, timidityManager.patches, timidityManager.sf2_data);
  Z_Free(Data);
  if (!Song) {
    GCon->Logf(NAME_Warning, "Failed to load MIDI song");
    //Timidity_Close(); // not needed anymore
    return nullptr;
  }
  InStrm->Close();
  delete InStrm;

  // create codec
  return new VTimidityAudioCodec(Song);
}
