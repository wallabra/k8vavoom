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
#include "../sound_private.h"
#include "../sound.h"


#pragma pack(1)
struct FRiffChunkHeader {
  char ID[4];
  vuint32 Size;
};

struct FWavFormatDesc {
  vuint16 Format;
  vuint16 Channels;
  vuint32 Rate;
  vuint32 BytesPerSec;
  vuint16 BlockAlign;
  vuint16 Bits;
};
#pragma pack()


class VWaveSampleLoader : public VSampleLoader {
public:
  virtual void Load (sfxinfo_t &, VStream &) override;
};


class VWavAudioCodec : public VAudioCodec {
public:
  VStream *Strm;
  int SamplesLeft;

  int WavChannels;
  int WavBits;
  int BlockAlign;

  VWavAudioCodec (VStream *InStrm);
  virtual ~VWavAudioCodec () override;
  virtual int Decode (short *Data, int NumSamples) override;
  virtual bool Finished () override;
  virtual void Restart () override;

  static VAudioCodec *Create (VStream *InStrm);
};

static VWaveSampleLoader WaveSampleLoader;

IMPLEMENT_AUDIO_CODEC(VWavAudioCodec, "Wav");


//==========================================================================
//
//  FindChunk
//
//==========================================================================
static int FindRiffChunk (VStream &Strm, const char *ID) {
  Strm.Seek(12);
  int EndPos = Strm.TotalSize();
  while (Strm.Tell()+8 <= EndPos) {
    FRiffChunkHeader ChunkHdr;
    Strm.Serialise(&ChunkHdr, 8);
    int ChunkSize = LittleLong(ChunkHdr.Size);
    if (!memcmp(ChunkHdr.ID, ID, 4)) return ChunkSize; // i found her!
    if (Strm.Tell()+ChunkSize > EndPos) break; // chunk goes beyound end of file
    Strm.Seek(Strm.Tell()+ChunkSize);
  }
  return -1;
}


//==========================================================================
//
//  VWaveSampleLoader::Load
//
//==========================================================================
void VWaveSampleLoader::Load (sfxinfo_t &Sfx, VStream &Strm) {
  // check header to see if it's a wave file
  char Header[12];
  Strm.Seek(0);
  Strm.Serialise(Header, 12);
  if (memcmp(Header, "RIFF", 4) || memcmp(Header+8, "WAVE", 4)) return; // not a WAVE

  // get format settings
  int FmtSize = FindRiffChunk(Strm, "fmt ");
  if (FmtSize < 16) return; // format not found or too small
  FWavFormatDesc Fmt;
  Strm.Serialise(&Fmt, 16);
  if (LittleShort(Fmt.Format) != 1) return; // not a PCM format
  int SampleRate = LittleLong(Fmt.Rate);
  int WavChannels = LittleShort(Fmt.Channels);
  int WavBits = LittleShort(Fmt.Bits);
  int BlockAlign = LittleShort(Fmt.BlockAlign);

  if (WavBits != 8 && WavBits != 16) return;

  // find data chunk
  int DataSize = FindRiffChunk(Strm, "data");
  if (DataSize == -1) return; // data not found

  //if (WavChannels != 1) fprintf(stderr, "WAVE: A stereo sample, taking left channel.\n");

  // fill in sample info and allocate data
  Sfx.sampleRate = SampleRate;
  Sfx.sampleBits = WavBits;
  Sfx.dataSize = (DataSize/BlockAlign)*(WavBits/8);
  Sfx.data = (vuint8 *)Z_Malloc(Sfx.dataSize);

  //  Read wav data.
  void *WavData = Z_Malloc(DataSize);
  Strm.Serialise(WavData, DataSize);

  // copy sample data
  DataSize /= BlockAlign;
  if (WavBits == 8) {
    byte *pSrc = (byte *)WavData;
    byte *pDst = (byte *)Sfx.data;
    for (int i = 0; i < DataSize; i++, pSrc += BlockAlign) {
      // mix it
      int v = 0;
      for (int f = 0; f < WavChannels; ++f) v += (int)pSrc[f];
      v /= WavChannels;
      if (v < -128) v = -128; else if (v > 127) v = 127;
      *pDst++ = (byte)v;
    }
  } else {
    byte *pSrc = (byte *)WavData;
    short *pDst = (short *)Sfx.data;
    for (int i = 0; i < DataSize; i++, pSrc += BlockAlign) {
      // mix it
      int v = 0;
      for (int f = 0; f < WavChannels; ++f) v += (int)(LittleShort(*(((short *)pSrc)+f)));
      v /= WavChannels;
      if (v < -32768) v = -32768; else if (v > 32767) v = 32767;
      *pDst++ = (short)v;
    }
  }
  Z_Free(WavData);
}


//==========================================================================
//
//  VWavAudioCodec::VWavAudioCodec
//
//==========================================================================
VWavAudioCodec::VWavAudioCodec (VStream *InStrm)
  : Strm(InStrm)
  , SamplesLeft(-1)
{
  int FmtSize = FindRiffChunk(*Strm, "fmt ");
  if (FmtSize < 16) return; // format not found or too small
  FWavFormatDesc Fmt;
  Strm->Serialise(&Fmt, 16);
  if (LittleShort(Fmt.Format) != 1) return; // not a PCM format
  SampleRate = LittleLong(Fmt.Rate);
  WavChannels = LittleShort(Fmt.Channels);
  WavBits = LittleShort(Fmt.Bits);
  BlockAlign = LittleShort(Fmt.BlockAlign);

  SamplesLeft = FindRiffChunk(*Strm, "data");
  if (SamplesLeft == -1) return; // data not found
  SamplesLeft /= BlockAlign;
}


//==========================================================================
//
//  VWavAudioCodec::~VWavAudioCodec
//
//==========================================================================
VWavAudioCodec::~VWavAudioCodec () {
  if (SamplesLeft != -1) {
    Strm->Close();
    delete Strm;
    Strm = nullptr;
  }
}


//==========================================================================
//
//  VWavAudioCodec::Decode
//
//==========================================================================
int VWavAudioCodec::Decode (short *Data, int NumSamples) {
  int CurSample = 0;
  byte Buf[1024];
  while (SamplesLeft && CurSample < NumSamples) {
    int ReadSamples = 1024/BlockAlign;
    if (ReadSamples > NumSamples-CurSample) ReadSamples = NumSamples-CurSample;
    if (ReadSamples > SamplesLeft) ReadSamples = SamplesLeft;
    Strm->Serialise(Buf, ReadSamples*BlockAlign);
    for (int i = 0; i < 2; ++i) {
      byte *pSrc = Buf;
      if (i && WavChannels > 1) pSrc += WavBits/8;
      short *pDst = Data+CurSample*2+i;
      if (WavBits == 8) {
        for (int j = 0; j < ReadSamples; ++j, pSrc += BlockAlign, pDst += 2) *pDst = (*pSrc-127)<<8;
      } else {
        for (int j = 0; j < ReadSamples; ++j, pSrc += BlockAlign, pDst += 2) *pDst = LittleShort(*(short *)pSrc);
      }
    }
    SamplesLeft -= ReadSamples;
    CurSample += ReadSamples;
  }
  return CurSample;
}


//==========================================================================
//
//  VWavAudioCodec::Finished
//
//==========================================================================
bool VWavAudioCodec::Finished () {
  return !SamplesLeft;
}


//==========================================================================
//
//  VWavAudioCodec::Restart
//
//==========================================================================
void VWavAudioCodec::Restart () {
  SamplesLeft = FindRiffChunk(*Strm, "data")/BlockAlign;
}


//==========================================================================
//
//  VWavAudioCodec::Create
//
//==========================================================================
VAudioCodec *VWavAudioCodec::Create (VStream *InStrm) {
  char Header[12];
  InStrm->Seek(0);
  InStrm->Serialise(Header, 12);
  if (!memcmp(Header, "RIFF", 4) && !memcmp(Header+8, "WAVE", 4)) {
    // it's a WAVE file
    VWavAudioCodec *Codec = new VWavAudioCodec(InStrm);
    if (Codec->SamplesLeft != -1) return Codec;
    // file seams to be broken
    delete Codec;
    Codec = nullptr;
  }
  return nullptr;
}
