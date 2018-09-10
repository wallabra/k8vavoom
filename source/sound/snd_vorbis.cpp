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
#include <vorbis/codec.h>

#include "gamedefs.h"
#include "snd_local.h"


class VVorbisAudioCodec : public VAudioCodec {
public:
  int InitLevel;
  VStream *Strm;
  bool FreeStream;
  int BytesLeft;

  ogg_sync_state oy;
  ogg_stream_state os;
  vorbis_info vi;
  vorbis_comment vc;
  vorbis_dsp_state vd;
  vorbis_block vb;

  bool eos;

  VVorbisAudioCodec (VStream *, bool);
  virtual ~VVorbisAudioCodec () override;
  bool Init ();
  void Cleanup ();
  virtual int Decode (short *, int) override;
  int ReadData ();
  virtual bool Finished () override;
  virtual void Restart () override;

  static VAudioCodec *Create (VStream *);
};


class VVorbisSampleLoader : public VSampleLoader {
public:
  virtual void Load (sfxinfo_t &, VStream &) override;
};

IMPLEMENT_AUDIO_CODEC(VVorbisAudioCodec, "Vorbis");

VVorbisSampleLoader VorbisSampleLoader;


//==========================================================================
//
//  VVorbisAudioCodec::VVorbisAudioCodec
//
//==========================================================================
VVorbisAudioCodec::VVorbisAudioCodec (VStream *AStrm, bool AFreeStream)
  : Strm(AStrm)
  , FreeStream(AFreeStream)
{
  BytesLeft = Strm->TotalSize();
  Strm->Seek(0);
  ogg_sync_init(&oy);
}


//==========================================================================
//
//  VVorbisAudioCodec::~VVorbisAudioCodec
//
//==========================================================================
VVorbisAudioCodec::~VVorbisAudioCodec () {
  if (InitLevel > 0) {
    Cleanup();
    if (FreeStream) {
      Strm->Close();
      delete Strm;
    }
    Strm = nullptr;
  }
  ogg_sync_clear(&oy);
}


//==========================================================================
//
//  VVorbisAudioCodec::Init
//
//==========================================================================
bool VVorbisAudioCodec::Init () {
  ogg_page og;
  ogg_packet op;

  eos = false;
  InitLevel = 0;

  // read some data
  ReadData();

  // get the first page
  if (ogg_sync_pageout(&oy, &og) != 1) return false; // not a Vorbis file

  ogg_stream_init(&os, ogg_page_serialno(&og));
  vorbis_info_init(&vi);
  vorbis_comment_init(&vc);
  InitLevel = 1;

  if (ogg_stream_pagein(&os, &og) < 0) return false; // stream version mismatch perhaps
  if (ogg_stream_packetout(&os, &op) != 1) return false; // no page? must not be vorbis
  if (vorbis_synthesis_headerin(&vi, &vc, &op) < 0) return false; // not a vorbis header

  // we need 2 more headers
  int i = 0;
  while (i < 2) {
    int result = ogg_stream_packetout(&os, &op);
    if (result < 0) return false; // corrupt header
    if (result > 0) {
      if (vorbis_synthesis_headerin(&vi, &vc, &op)) return false; // corrupt header
      ++i;
    } else if (ogg_sync_pageout(&oy, &og) > 0) {
      ogg_stream_pagein(&os, &og);
    } else if (ReadData() == 0 && i < 2) {
      // out of data while reading headers
      return false;
    }
  }

  // parsed all three headers; initialise the Vorbis decoder
  vorbis_synthesis_init(&vd, &vi);
  vorbis_block_init(&vd, &vb);
  SampleRate = vi.rate;
  InitLevel = 2;
  return true;
}


//==========================================================================
//
//  VVorbisAudioCodec::Cleanup
//
//==========================================================================
void VVorbisAudioCodec::Cleanup () {
  if (InitLevel > 0) ogg_stream_clear(&os);
  if (InitLevel > 1) { vorbis_block_clear(&vb); vorbis_dsp_clear(&vd); }
  if (InitLevel > 0) { vorbis_comment_clear(&vc); vorbis_info_clear(&vi); }
  InitLevel = 0;
}


//==========================================================================
//
//  VVorbisAudioCodec::Decode
//
//==========================================================================
int VVorbisAudioCodec::Decode (short *Data, int NumSamples) {
  ogg_page og;
  ogg_packet op;
  int CurSample = 0;

  while (!eos) {
    // while we have data ready, read it
    float **pcm;
    int samples = vorbis_synthesis_pcmout(&vd, &pcm);
    if (samples > 0) {
      int bout = NumSamples-CurSample;
      if (bout > samples) bout = samples;
      for (int i = 0; i < 2; ++i) {
        short *dst = Data+CurSample*2+i;
        float *src = pcm[vi.channels > 1 ? i : 0];
        for (int j = 0; j < bout; ++j, dst += 2) {
          int val = int(src[j]*32767.0f);
          // clipping
               if (val > 32767) val = 32767;
          else if (val < -32768) val = -32768;
          *dst = val;
        }
      }
      // tell libvorbis how many samples we actually consumed
      vorbis_synthesis_read(&vd, bout);
      CurSample += bout;
      if (CurSample == NumSamples) return CurSample;
    }

    if (ogg_stream_packetout(&os, &op) > 0) {
      // we have a packet: decode it
      if (vorbis_synthesis(&vb, &op) == 0) vorbis_synthesis_blockin(&vd, &vb);
    } else if (ogg_sync_pageout(&oy, &og) > 0) {
      ogg_stream_pagein(&os, &og);
    } else if (ReadData() == 0) {
      eos = true;
    }
  }
  return CurSample;
}


//==========================================================================
//
//  VVorbisAudioCodec::ReadData
//
//==========================================================================
int VVorbisAudioCodec::ReadData () {
  if (!BytesLeft) return 0;
  char *buffer = ogg_sync_buffer(&oy, 4096);
  int bytes = 4096;
  if (bytes > BytesLeft) bytes = BytesLeft;
  Strm->Serialise(buffer, bytes);
  ogg_sync_wrote(&oy, bytes);
  BytesLeft -= bytes;
  return bytes;
}


//==========================================================================
//
//  VVorbisAudioCodec::Finished
//
//==========================================================================
bool VVorbisAudioCodec::Finished () {
  return eos;
}


//==========================================================================
//
//  VVorbisAudioCodec::Restart
//
//==========================================================================
void VVorbisAudioCodec::Restart () {
  Cleanup();
  Strm->Seek(0);
  BytesLeft = Strm->TotalSize();
  Init();
}


//==========================================================================
//
//  VVorbisAudioCodec::Create
//
//==========================================================================
VAudioCodec *VVorbisAudioCodec::Create (VStream *InStrm) {
  VVorbisAudioCodec *Codec = new VVorbisAudioCodec(InStrm, true);
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
//  VVorbisAudioCodec::Create
//
//==========================================================================
void VVorbisSampleLoader::Load (sfxinfo_t &Sfx, VStream &Stream) {
  VVorbisAudioCodec *Codec = new VVorbisAudioCodec(&Stream, false);
  if (!Codec->Init()) {
    Codec->Cleanup();
  } else {
    LoadFromAudioCodec(Sfx, Codec);
  }
  delete Codec;
}
