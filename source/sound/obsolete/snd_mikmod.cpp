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

#ifdef _WIN32
#include "winshit/winlocal.h"
#endif
#include <mikmod.h>

#include "gamedefs.h"
#include "snd_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

class VMikModAudioCodec : public VAudioCodec
{
public:
  struct FMikModArchiveReader
  {
    MREADER   Core;
    VStream *Strm;
    bool    AtEof;
  };

  MODULE *Module;

  static bool   MikModInitialised;
  static MDRIVER  Driver;

  //  VAudioCodec interface.
  VMikModAudioCodec(MODULE *InModule);
  ~VMikModAudioCodec();
  virtual int Decode(short *Data, int NumSamples) override;
  virtual bool Finished() override;
  virtual void Restart() override;

  //  Driver functions.
  static BOOL Drv_IsThere();
  static void Drv_Update();
  static BOOL Drv_Reset();

  //  Archive reader functions.
  static BOOL ArchiveReader_Seek(MREADER *rd, long offset, int whence);
  static long ArchiveReader_Tell(MREADER *rd);
  static BOOL ArchiveReader_Read(MREADER *rd, void *dest, size_t length);
  static int ArchiveReader_Get(MREADER *rd);
  static BOOL ArchiveReader_Eof(MREADER *rd);

  static VAudioCodec *Create(VStream *InStrm);
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

IMPLEMENT_AUDIO_CODEC(VMikModAudioCodec, "MikMod");

bool      VMikModAudioCodec::MikModInitialised;
MDRIVER     VMikModAudioCodec::Driver =
{
  nullptr,
  (CHAR*)"k8vavoom",
  (CHAR*)"k8vavoom output driver",
  0,
  255,
#if (LIBMIKMOD_VERSION > 0x030106)
  (CHAR*)"k8vavoom",
#  if (LIBMIKMOD_VERSION >= 0x030200)
  (CHAR*)"",
#  endif  /* libmikmod-3.2.x */
  nullptr,
#endif
  VMikModAudioCodec::Drv_IsThere,
  VC_SampleLoad,
  VC_SampleUnload,
  VC_SampleSpace,
  VC_SampleLength,
  VC_Init,
  VC_Exit,
  VMikModAudioCodec::Drv_Reset,
  VC_SetNumVoices,
  VC_PlayStart,
  VC_PlayStop,
  VMikModAudioCodec::Drv_Update,
  nullptr,
  VC_VoiceSetVolume,
  VC_VoiceGetVolume,
  VC_VoiceSetFrequency,
  VC_VoiceGetFrequency,
  VC_VoiceSetPanning,
  VC_VoiceGetPanning,
  VC_VoicePlay,
  VC_VoiceStop,
  VC_VoiceStopped,
  VC_VoiceGetPosition,
  VC_VoiceRealVolume
};

static VCvarB s_mikmod_hqmixer("snd_mikmod_hqmixer", true, "MikMod: use HQ mixer?", CVAR_Archive);
static VCvarB s_mikmod_float("snd_mikmod_float", true, "MikMod: use floating point?", CVAR_Archive);
static VCvarB s_mikmod_surround("snd_mikmod_surround", false, "MikMod: allow surround?", CVAR_Archive);
static VCvarB s_mikmod_interpolation("snd_mikmod_interpolation", true, "MikMod: interpolation.", CVAR_Archive);
static VCvarB s_mikmod_reverse_stereo("snd_mikmod_reverse_stereo", false, "MikMod: reverse stereo?", CVAR_Archive);
static VCvarB s_mikmod_lowpass("snd_mikmod_lowpass", true, "MikMod: lowpass filter?", CVAR_Archive);

// CODE --------------------------------------------------------------------

//==========================================================================
//
//  VMikModAudioCodec::VMikModAudioCodec
//
//==========================================================================

VMikModAudioCodec::VMikModAudioCodec(MODULE *InModule)
: Module(InModule)
{
}

//==========================================================================
//
//  VMikModAudioCodec::~VMikModAudioCodec
//
//==========================================================================

VMikModAudioCodec::~VMikModAudioCodec()
{
  Player_Stop();
  Player_Free(Module);
  MikMod_Exit();
}

//==========================================================================
//
//  VMikModAudioCodec::Decode
//
//==========================================================================

int VMikModAudioCodec::Decode(short *Data, int NumSamples)
{
  return VC_WriteBytes((SBYTE*)Data, NumSamples * 4) / 4;
}

//==========================================================================
//
//  VMikModAudioCodec::Finished
//
//==========================================================================

bool VMikModAudioCodec::Finished()
{
  return !Player_Active();
}

//==========================================================================
//
//  VMikModAudioCodec::Restart
//
//==========================================================================

void VMikModAudioCodec::Restart()
{
  Player_SetPosition(0);
}

//==========================================================================
//
//  VMikModAudioCodec::Drv_IsThere
//
//==========================================================================

BOOL VMikModAudioCodec::Drv_IsThere()
{
  return 1;
}

//==========================================================================
//
//  VMikModAudioCodec::Drv_Update
//
//==========================================================================

void VMikModAudioCodec::Drv_Update()
{
}

//==========================================================================
//
//  VMikModAudioCodec::Drv_Reset
//
//==========================================================================

BOOL VMikModAudioCodec::Drv_Reset()
{
  VC_Exit();
  return VC_Init();
}

//==========================================================================
//
//  VMikModAudioCodec::ArchiveReader_Seek
//
//==========================================================================

BOOL VMikModAudioCodec::ArchiveReader_Seek(MREADER *rd, long offset, int whence)
{
  VStream *Strm = ((FMikModArchiveReader*)rd)->Strm;
  int NewPos = 0;
  switch (whence)
  {
  case SEEK_SET:
    NewPos = offset;
    break;
  case SEEK_CUR:
    NewPos = Strm->Tell() + offset;
    break;
  case SEEK_END:
    NewPos = Strm->TotalSize() + offset;
    break;
  }
  if (NewPos > Strm->TotalSize())
  {
    return false;
  }
  Strm->Seek(NewPos);
  return !Strm->IsError();
}

//==========================================================================
//
//  VMikModAudioCodec::ArchiveReader_Tell
//
//==========================================================================

long VMikModAudioCodec::ArchiveReader_Tell(MREADER *rd)
{
  VStream *Strm = ((FMikModArchiveReader*)rd)->Strm;
  return Strm->Tell();
}

//==========================================================================
//
//  VMikModAudioCodec::ArchiveReader_Read
//
//==========================================================================

BOOL VMikModAudioCodec::ArchiveReader_Read(MREADER *rd, void *dest, size_t length)
{
  VStream *Strm = ((FMikModArchiveReader*)rd)->Strm;
  if (Strm->Tell() + (int)length > Strm->TotalSize())
  {
    ((FMikModArchiveReader*)rd)->AtEof = true;
    return false;
  }
  Strm->Serialise(dest, length);
  return !Strm->IsError();
}

//==========================================================================
//
//  VMikModAudioCodec::ArchiveReader_Get
//
//==========================================================================

int VMikModAudioCodec::ArchiveReader_Get(MREADER *rd)
{
  VStream *Strm = ((FMikModArchiveReader*)rd)->Strm;
  if (Strm->AtEnd())
  {
    ((FMikModArchiveReader*)rd)->AtEof = true;
    return EOF;
  }
  else
  {
    vuint8 c;
    *Strm << c;
    return c;
  }
}

//==========================================================================
//
//  VMikModAudioCodec::ArchiveReader_Eof
//
//==========================================================================

BOOL VMikModAudioCodec::ArchiveReader_Eof(MREADER *rd)
{
  return ((FMikModArchiveReader*)rd)->AtEof;
}

//==========================================================================
//
//  VMikModAudioCodec::Create
//
//==========================================================================

VAudioCodec *VMikModAudioCodec::Create(VStream *InStrm)
{
  if (snd_mod_player != 0)
  {
    return nullptr;
  }
  if (!MikModInitialised)
  {
    //  Register our driver and all the loaders.
    MikMod_RegisterDriver(&Driver);
    if (!MikMod_InfoLoader())
      MikMod_RegisterAllLoaders();
    MikModInitialised = true;
  }

  //  Set up playback parameters.
  md_mixfreq = 44100;
  md_mode = DMODE_16BITS | DMODE_SOFT_MUSIC | DMODE_STEREO;
  if (s_mikmod_hqmixer)
  {
    md_mode |= DMODE_HQMIXER;
  }
  else
  {
    md_mode &= ~DMODE_HQMIXER;
  }
#ifdef DMODE_FLOAT
  if (s_mikmod_float)
  {
    md_mode |= DMODE_FLOAT;
  }
  else
  {
    md_mode &= ~DMODE_FLOAT;
  }
#endif
  if (s_mikmod_surround)
  {
    md_mode |= DMODE_SURROUND;
  }
  else
  {
    md_mode &= ~DMODE_SURROUND;
  }
  if (s_mikmod_interpolation)
  {
    md_mode |= DMODE_INTERP;
  }
  else
  {
    md_mode &= ~DMODE_INTERP;
  }
  if (s_mikmod_reverse_stereo)
  {
    md_mode |= DMODE_REVERSE;
  }
  else
  {
    md_mode &= ~DMODE_REVERSE;
  }
#ifdef DMODE_NOISEREDUCTION
  if (s_mikmod_lowpass)
  {
    md_mode |= DMODE_NOISEREDUCTION;
  }
  else
  {
    md_mode &= ~DMODE_NOISEREDUCTION;
  }
#endif

  //  Initialise MikMod.
  if (MikMod_Init((CHAR*)""))
  {
    GCon->Logf("MikMod init failed");
    return nullptr;
  }

  //  Create a reader.
  FMikModArchiveReader Reader;
  Reader.Core.Eof  = ArchiveReader_Eof;
  Reader.Core.Read = ArchiveReader_Read;
  Reader.Core.Get  = ArchiveReader_Get;
  Reader.Core.Seek = ArchiveReader_Seek;
  Reader.Core.Tell = ArchiveReader_Tell;
  Reader.Strm = InStrm;
  Reader.AtEof = false;
  InStrm->Seek(0);

  //  Try to load the song.
  MODULE *module = Player_LoadGeneric(&Reader.Core, 256, 0);
  if (!module)
  {
    //  Not a module file.
    MikMod_Exit();
    return nullptr;
  }

  //  Close stream.
  InStrm->Close();
  delete InStrm;
  InStrm = nullptr;

  //  Start playback.
  Player_Start(module);
  return new VMikModAudioCodec(module);
}
