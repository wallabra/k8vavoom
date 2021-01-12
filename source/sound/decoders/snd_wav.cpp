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

struct __attribute__((packed)) FRiffChunkHeader {
  char ID[4];
  vuint32 Size;
};
static_assert(sizeof(FRiffChunkHeader) == 8, "invalid size of FRiffChunkHeader");

struct __attribute__((packed)) FWavFormatDesc
{
  vuint16   Format;
  vuint16   Channels;
  vuint32   Rate;
  vuint32   BytesPerSec;
  vuint16   BlockAlign;
  vuint16   Bits;
};
static_assert(sizeof(FWavFormatDesc) == 2+2+4+4+2+2, "invalid size of FWavFormatDesc");


class VWaveSampleLoader : public VSampleLoader {
public:
  VWaveSampleLoader () : VSampleLoader(true) {} // with signature
  virtual void Load (sfxinfo_t &, VStream &) override;
  virtual const char *GetName () const noexcept override;
};


class VWavAudioCodec : public VAudioCodec {
public:
  VStream *Strm;
  int SamplesLeft;
  int WavChannels;
  int WavBits;
  int BlockAlign;
  vuint8 Buf[1024];

public:
  VWavAudioCodec (VStream *InStrm);
  virtual ~VWavAudioCodec () override;

  virtual int Decode (vint16 *Data, int NumFrames) override;
  virtual bool Finished () override;
  virtual void Restart () override;

  static VAudioCodec *Create (VStream *InStrm, const vuint8 sign[], int signsize);
};


static VWaveSampleLoader WaveSampleLoader;

IMPLEMENT_AUDIO_CODEC_EX(VWavAudioCodec, "Wav", 663); // before XMP


//==========================================================================
//
//  FindRiffChunk
//
//==========================================================================
static int FindRiffChunk (VStream &Strm, const char *ID) {
  Strm.Seek(12);
  if (Strm.IsError()) return -1;
  int EndPos = Strm.TotalSize();
  while (Strm.Tell()+8 <= EndPos) {
    if (Strm.IsError()) return -1;
    FRiffChunkHeader ChunkHdr;
    Strm.Serialise(&ChunkHdr, 8);
    int ChunkSize = LittleLong(ChunkHdr.Size);
    if (!memcmp(ChunkHdr.ID, ID, 4)) return ChunkSize; // found chunk
    if (Strm.Tell()+ChunkSize > EndPos) break; // chunk goes beyound end of file
    Strm.Seek(Strm.Tell()+ChunkSize);
    if (Strm.IsError()) return -1;
  }
  return -1;
}


//==========================================================================
//
//  VWaveSampleLoader::GetName
//
//==========================================================================
const char *VWaveSampleLoader::GetName () const noexcept {
  return "wav";
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
  if (memcmp(Header, "RIFF", 4) != 0 || memcmp(Header+8, "WAVE", 4) != 0) return; // not a WAVE

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
  //if (WavChannels != 1) GCon->Logf("A stereo sample, taking left channel");
  if (WavChannels < 1 || WavChannels > 2) return;
  if (SampleRate < 128 || SampleRate > 96000) return;
  if (WavBits != 8 && WavBits != 16) return;

  // find data chunk
  int DataSize = FindRiffChunk(Strm, "data");
  if (DataSize == -1) return; // data not found

  // fill in sample info and allocate data.
  Sfx.SampleRate = SampleRate;
  Sfx.SampleBits = WavBits;
  Sfx.DataSize = (DataSize/BlockAlign)*(WavBits/8);
  Sfx.Data = Z_Malloc(Sfx.DataSize);

  // read wave data
  void *WavData = Z_Malloc(DataSize);
  Strm.Serialise(WavData, DataSize);

  // copy sample data
  DataSize /= BlockAlign;
  if (WavBits == 8) {
    vuint8 *pSrc = (vuint8 *)WavData;
    vuint8 *pDst = (vuint8 *)Sfx.Data;
    for (int i = 0; i < DataSize; ++i, pSrc += BlockAlign) {
      int v = 0;
      for (int f = 0; f < WavChannels; ++f) v += (int)pSrc[f];
      v /= WavChannels;
      if (v < -128) v = -128; else if (v > 127) v = 127;
      *pDst++ = (vuint8)v;
    }
  } else {
    vuint8 *pSrc = (vuint8 *)WavData;
    vint16 *pDst = (vint16 *)Sfx.Data;
    for (int i = 0; i < DataSize; ++i, pSrc += BlockAlign) {
      int v = 0;
      for (int f = 0; f < WavChannels; ++f) v += (int)(LittleShort(*(((vint16 *)pSrc)+f)));
      v /= WavChannels;
      if (v < -32768) v = -32768; else if (v > 32767) v = 32767;
      *pDst++ = (vint16)v;
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
  if (WavBits != 8 && WavBits != 16) return;
  if (WavChannels < 1 || WavChannels > 2) return;
  if (SampleRate < 128 || SampleRate > 96000) return;

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
int VWavAudioCodec::Decode (vint16 *Data, int NumFrames) {
  int CurFrame = 0;
  while (SamplesLeft && CurFrame < NumFrames) {
    int ReadSamples = 1024/BlockAlign;
    if (ReadSamples > NumFrames-CurFrame) ReadSamples = NumFrames-CurFrame;
    if (ReadSamples > SamplesLeft) ReadSamples = SamplesLeft;
    Strm->Serialise(Buf, ReadSamples*BlockAlign);
    for (int i = 0; i < 2; ++i) {
      vuint8 *pSrc = Buf;
      if (i && WavChannels > 1) pSrc += WavBits/8;
      vint16 *pDst = Data+CurFrame*2+i;
      if (WavBits == 8) {
        for (int j = 0; j < ReadSamples; j++, pSrc += BlockAlign, pDst += 2) {
          *pDst = (*pSrc-127)<<8;
        }
      } else {
        for (int j = 0; j < ReadSamples; j++, pSrc += BlockAlign, pDst += 2) {
          *pDst = LittleShort(*(int16_t *)pSrc);
        }
      }
    }
    SamplesLeft -= ReadSamples;
    CurFrame += ReadSamples;
  }
  return CurFrame;
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
VAudioCodec *VWavAudioCodec::Create (VStream *InStrm, const vuint8 sign[], int signsize) {
  if (memcmp(sign, "RIFF", 4) != 0) return nullptr;
  char Header[12];
  if (signsize >= 12) {
    memcpy(Header, sign, 12);
  } else {
    InStrm->Seek(0);
    InStrm->Serialise(Header, 12);
  }
  if (memcmp(Header, "RIFF", 4) != 0 || memcmp(Header+8, "WAVE", 4) != 0) return nullptr;

  // it's a WAVE file
  VWavAudioCodec *Codec = new VWavAudioCodec(InStrm);
  if (Codec->SamplesLeft != -1) return Codec;
  // file seams to be broken
  delete Codec;
  return nullptr;
}
