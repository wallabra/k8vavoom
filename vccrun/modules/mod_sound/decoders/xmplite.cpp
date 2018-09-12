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
#include <xmp.h>

#include "../sound_private.h"
#include "../sound.h"
#include "../../../convars.h"


// ////////////////////////////////////////////////////////////////////////// //
static VCvarF snd_xmp_amp("snd_xmp_amp", "1", "XMP: amplification [0..3]", CVAR_Archive);
static VCvarI snd_xmp_interpolator("snd_xmp_interpolator", "2", "XMP: interpolator (0:nearest; 1:linear; 2:spline)", CVAR_Archive);
static VCvarB snd_xmp_full_dsp("snd_xmp_full_dsp", true, "XMP: turn on all DSP effects?", CVAR_Archive);


// ////////////////////////////////////////////////////////////////////////// //
class VXMPAudioCodec : public VAudioCodec {
public:
  int InitLevel;
  VStream *Strm;
  bool FreeStream;
  vuint8 *modData;
  vint32 modDataSize;
  bool eos;
  bool allowLooping;
  vuint8 frmbuf[XMP_MAX_FRAMESIZE];
  vint32 frmbufPos, frmbufUsed;

  xmp_context xmpctx;

  VXMPAudioCodec (VStream *, bool);
  virtual ~VXMPAudioCodec () override;
  bool Init ();
  void Cleanup ();
  virtual int Decode (short *, int) override;
  virtual bool Finished () override;
  virtual void Restart () override;

  static VAudioCodec *Create (VStream *);

private:
  bool loadModuleData ();
  void unloadModuleData ();
};


class VXMPSampleLoader : public VSampleLoader {
public:
  virtual void Load (sfxinfo_t &, VStream &) override;
};

IMPLEMENT_AUDIO_CODEC(VXMPAudioCodec, "XMP");

VXMPSampleLoader XMPSampleLoader;


//==========================================================================
//
//  VXMPAudioCodec::VXMPAudioCodec
//
//==========================================================================
VXMPAudioCodec::VXMPAudioCodec (VStream *AStrm, bool AFreeStream)
  : Strm(AStrm)
  , FreeStream(AFreeStream)
  , modData(nullptr)
  , modDataSize(0)
  , eos(false)
  , allowLooping(true)
  , frmbufPos(0)
  , frmbufUsed(0)
  , xmpctx(0)
{
  loadModuleData();
}


//==========================================================================
//
//  VXMPAudioCodec::~VXMPAudioCodec
//
//==========================================================================
VXMPAudioCodec::~VXMPAudioCodec () {
  Cleanup();
  if (InitLevel > 0) {
    if (FreeStream) {
      Strm->Close();
      delete Strm;
    }
    Strm = nullptr;
  }
  unloadModuleData();
}


//==========================================================================
//
//  VXMPAudioCodec::loadModuleData
//
//==========================================================================
bool VXMPAudioCodec::loadModuleData () {
  if (modData) return true;
  auto modsz = Strm->TotalSize();
  if (modsz < 32 || modsz > 1024*1024*512) return false; // alas
  Strm->Seek(0);
  modData = (vuint8 *)malloc(modsz);
  if (!modData) return false;
  modDataSize = modsz;
  Strm->Serialize(modData, modsz);
  if (Strm->IsError()) { unloadModuleData(); return false; }
  return true;
}


//==========================================================================
//
//  VXMPAudioCodec::unloadModuleData
//
//==========================================================================
void VXMPAudioCodec::unloadModuleData () {
  if (modData) free(modData);
  modData = nullptr;
  modDataSize = 0;
}


//==========================================================================
//
//  VXMPAudioCodec::Init
//
//==========================================================================
bool VXMPAudioCodec::Init () {
  InitLevel = 0;

  if (!modData) return false;

  // create context
  xmpctx = xmp_create_context();
  if (!xmpctx) return false;

  int amp = (int)(snd_xmp_amp+0.5f);
  if (amp < 0) amp = 0; else if (amp > 3) amp = 3;
  xmp_set_player(xmpctx, XMP_PLAYER_AMP, amp);

  switch (snd_xmp_interpolator) {
    case 0: xmp_set_player(xmpctx, XMP_PLAYER_INTERP, XMP_INTERP_NEAREST); break;
    case 1: default: xmp_set_player(xmpctx, XMP_PLAYER_INTERP, XMP_INTERP_LINEAR); break;
    case 2: xmp_set_player(xmpctx, XMP_PLAYER_INTERP, XMP_INTERP_SPLINE); break;
  }
  xmp_set_player(xmpctx, XMP_PLAYER_DSP, (snd_xmp_full_dsp ? XMP_DSP_ALL : XMP_DSP_LOWPASS));

  // load module
  if (xmp_load_module_from_memory(xmpctx, modData, modDataSize) != 0) return false;

  InitLevel = 1;
  // initialize player
  if (xmp_start_player(xmpctx, 48000, 0) != 0) return false;

  SampleRate = 48000; // always
  SampleBits = 16;
  NumChannels = 2;
  frmbufPos = frmbufUsed = 0;
  InitLevel = 2;

  return true;
}


//==========================================================================
//
//  VXMPAudioCodec::Cleanup
//
//==========================================================================
void VXMPAudioCodec::Cleanup () {
  if (InitLevel >= 2) xmp_end_player(xmpctx);
  if (InitLevel >= 1) xmp_release_module(xmpctx);
  if (xmpctx) { xmp_free_context(xmpctx); xmpctx = 0; }
  InitLevel = 0;
}


//==========================================================================
//
//  VXMPAudioCodec::Decode
//
//  `NumSamples` is number of frames, actually
//
//==========================================================================
int VXMPAudioCodec::Decode (short *Data, int NumSamples) {
  if (!xmpctx) return 0;
  /*struct*/ xmp_frame_info mi;
  int CurSample = 0;
  while (CurSample < NumSamples) {
    if (frmbufPos >= frmbufUsed) {
      if (eos) break;
      int fres = xmp_play_frame(xmpctx);
      if (fres != 0) { eos = true; break; }
      xmp_get_frame_info(xmpctx, &mi);
      if (!allowLooping && mi.loop_count > 0) {
        // exit before looping
        eos = true;
        //if (mi.buffer_size == 0) break;
        //fprintf(stderr, "LOOP DETECTED (finished=%d)\n", (int)Finished());
        break;
      }
      memcpy(frmbuf, mi.buffer, mi.buffer_size);
      frmbufPos = 0;
      frmbufUsed = (vint32)mi.buffer_size;
      //fprintf(stderr, "got %d bytes of data (loopcount: %d)\n", frmbufUsed, (int)mi.loop_count);
    }
    int frames = (frmbufUsed-frmbufPos)/4; // two channels, 16-bit data
    int toread = (NumSamples-CurSample);
    if (toread > frames) toread = frames;
    //fprintf(stderr, "  reading %d frames (%d frames left)\n", toread, NumSamples-CurSample);
    memcpy(Data+CurSample*2, frmbuf+frmbufPos, toread*4);
    frmbufPos += toread*4;
    CurSample += toread;
  }
  if (CurSample == 0) eos = true;
  return CurSample;
}


//==========================================================================
//
//  VXMPAudioCodec::Finished
//
//==========================================================================
bool VXMPAudioCodec::Finished () {
  return (eos && frmbufPos >= frmbufUsed);
}


//==========================================================================
//
//  VXMPAudioCodec::Restart
//
//==========================================================================
void VXMPAudioCodec::Restart () {
  Cleanup();
  Init();
}


//==========================================================================
//
//  VXMPAudioCodec::Create
//
//==========================================================================
VAudioCodec *VXMPAudioCodec::Create (VStream *InStrm) {
  VXMPAudioCodec *Codec = new VXMPAudioCodec(InStrm, true);
  if (!Codec->Init()) {
    Codec->Cleanup();
    delete Codec;
    Codec = nullptr;
    return nullptr;
  }
  return Codec;
}


//==========================================================================
//
//  VXMPAudioCodec::Create
//
//==========================================================================
void VXMPSampleLoader::Load (sfxinfo_t &Sfx, VStream &Stream) {
  VXMPAudioCodec *Codec = new VXMPAudioCodec(&Stream, false);
  Codec->allowLooping = false; // this is sfx, no loops allowed
  if (!Codec->Init()) {
    Codec->Cleanup();
  } else {
    LoadFromAudioCodec(Sfx, Codec);
  }
  delete Codec;
}
