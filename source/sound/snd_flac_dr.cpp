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

#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_STDIO
#define DR_FLAC_NO_CRC
#define DR_FLAC_NO_SIMD

#define DRFLAC_ASSERT(expression)
#define DRFLAC_MALLOC   Z_Malloc
#define DRFLAC_REALLOC  Z_Realloc
#define DRFLAC_FREE     Z_Free

#include "stbdr/dr_flac.h"


class VFlacAudioCodec : public VAudioCodec {
public:
  drflac *decoder;
  VStream *Strm;
  bool FreeStream;
  size_t BytesLeft;
  bool eos;
  bool inited;
  vint16 *tmpbuf;
  int tmpbufsize;

  VFlacAudioCodec (VStream *AStrm, bool AFreeStream);
  virtual ~VFlacAudioCodec () override;
  bool Init ();
  void Cleanup ();
  virtual int Decode (short *Data, int NumSamples) override;
  virtual bool Finished () override;
  virtual void Restart () override;

  static VAudioCodec *Create (VStream *InStream);

protected:
  static size_t readCB (void *pUserData, void *pBufferOut, size_t bytesToRead);
  static drflac_bool32 seekCB (void *pUserData, int offset, drflac_seek_origin origin);
};


class VFlacSampleLoader : public VSampleLoader {
public:
  virtual void Load (sfxinfo_t &, VStream &) override;
};


VFlacSampleLoader FlacSampleLoader;

IMPLEMENT_AUDIO_CODEC(VFlacAudioCodec, "FLAC(dr)");


//==========================================================================
//
//  VFlacAudioCodec::VFlacAudioCodec
//
//==========================================================================
VFlacAudioCodec::VFlacAudioCodec (VStream *AStrm, bool AFreeStream)
  : decoder(nullptr)
  , Strm(AStrm)
  , FreeStream(AFreeStream)
  , BytesLeft(0)
  , eos(false)
  , inited(false)
  , tmpbuf(nullptr)
  , tmpbufsize(0)
{
  BytesLeft = Strm->TotalSize();
  Strm->Seek(0);
}


//==========================================================================
//
//  VFlacAudioCodec::~VFlacAudioCodec
//
//==========================================================================
VFlacAudioCodec::~VFlacAudioCodec () {
  Cleanup();
  if (inited && FreeStream && Strm) {
    Strm->Close();
    delete Strm;
  }
  Strm = nullptr;
}


//==========================================================================
//
//  VFlacAudioCodec::Cleanup
//
//==========================================================================
void VFlacAudioCodec::Cleanup () {
  if (decoder) { drflac_close(decoder); decoder = nullptr; }
  if (tmpbuf) Z_Free(tmpbuf);
  tmpbuf = nullptr;
  tmpbufsize = 0;
}


//==========================================================================
//
//  VFlacAudioCodec::readCB
//
//==========================================================================
size_t VFlacAudioCodec::readCB (void *pUserData, void *pBufferOut, size_t bytesToRead) {
  if (bytesToRead == 0) return 0; // just in case
  VFlacAudioCodec *self = (VFlacAudioCodec *)pUserData;
  if (!self->Strm || self->BytesLeft == 0 || self->Strm->IsError()) return 0;
  if (bytesToRead > (unsigned)self->BytesLeft) bytesToRead = (unsigned)self->BytesLeft;
  self->Strm->Serialise(pBufferOut, (int)bytesToRead);
  if (self->Strm->IsError()) return 0;
  self->BytesLeft -= (int)bytesToRead;
  return bytesToRead;
}


//==========================================================================
//
//  VFlacAudioCodec::seekCB
//
//==========================================================================
drflac_bool32 VFlacAudioCodec::seekCB (void *pUserData, int offset, drflac_seek_origin origin) {
  VFlacAudioCodec *self = (VFlacAudioCodec *)pUserData;
  if (!self->Strm || self->Strm->IsError()) return DRFLAC_FALSE;
  if (origin == drflac_seek_origin_current) offset += self->Strm->Tell();
  if (offset < 0 || offset > self->Strm->TotalSize()) return DRFLAC_FALSE;
  self->Strm->Seek(offset);
  if (self->Strm->IsError()) return DRFLAC_FALSE;
  self->BytesLeft = self->Strm->TotalSize()-self->Strm->Tell();
  return DRFLAC_TRUE;
}


//==========================================================================
//
//  VFlacAudioCodec::Init
//
//==========================================================================
bool VFlacAudioCodec::Init () {
  Cleanup();

  eos = false;
  inited = false;

  //GCon->Logf(NAME_Debug, "buffer filled (%s) (%d/%d/%d)", *Strm->GetName(), inbufUsed, inbufFilled, inbufSize);
  decoder = drflac_open(&readCB, &seekCB, (void *)this, nullptr);
  if (!decoder) {
    //GCon->Logf(NAME_Debug, "stb_vorbis error: %d", error);
    Cleanup();
    return false;
  }

  if (decoder->sampleRate < 64 || decoder->sampleRate > 96000*2) {
    GCon->Logf(NAME_Warning, "cannot open flac '%s' with sample rate %u", *Strm->GetName(), (unsigned)decoder->sampleRate);
    Cleanup();
    return false;
  }

  if (decoder->channels < 1 || decoder->channels > 2) {
    GCon->Logf(NAME_Warning, "cannot open flac '%s' with %u channels", *Strm->GetName(), (unsigned)decoder->channels);
    Cleanup();
    return false;
  }

  /*
  if (decoder->bitsPerSample != 8 && decoder->bitsPerSample != 16) {
    GCon->Logf(NAME_Warning, "cannot open flac '%s' with %u bits per sample", *Strm->GetName(), (unsigned)decoder->bitsPerSample);
    Cleanup();
    return false;
  }
  */

  SampleRate = decoder->sampleRate;
  SampleBits = /*decoder->bitsPerSample*/16;
  NumChannels = 2; // always
  inited = true;
  return true;
}


//==========================================================================
//
//  VFlacAudioCodec::Decode
//
//==========================================================================
int VFlacAudioCodec::Decode (short *Data, int NumSamples) {
  int CurSample = 0;
  if (eos) return 0;
  while (!eos && CurSample < NumSamples) {
    if (decoder->channels == 2) {
      drflac_uint64 rd = drflac_read_pcm_frames_s16(decoder, (unsigned)(NumSamples-CurSample), (drflac_int16 *)(Data+CurSample));
      if (rd < (unsigned)(NumSamples-CurSample)) eos = true;
      CurSample += (int)rd;
    } else {
      // read mono data
      if (tmpbufsize < NumSamples-CurSample) {
        tmpbufsize = NumSamples-CurSample;
        tmpbuf = (vint16 *)Z_Realloc(tmpbuf, tmpbufsize*2);
      }
      drflac_uint64 rd = drflac_read_pcm_frames_s16(decoder, (unsigned)(NumSamples-CurSample), (drflac_int16 *)tmpbuf);
      // expand it to stereo
      for (int f = 0; f < (int)rd; ++f) {
        Data[(CurSample+f)*2+0] = Data[(CurSample+f)*2+1] = tmpbuf[f];
      }
      if (rd < (unsigned)(NumSamples-CurSample)) eos = true;
      CurSample += (int)rd;
    }
  }
  return CurSample;
}


//==========================================================================
//
//  VFlacAudioCodec::Finished
//
//==========================================================================
bool VFlacAudioCodec::Finished () {
  return (eos);
}


//==========================================================================
//
//  VFlacAudioCodec::Restart
//
//==========================================================================
void VFlacAudioCodec::Restart () {
  Strm->Seek(0);
  BytesLeft = Strm->TotalSize();
  Init();
}


//==========================================================================
//
//  VFlacAudioCodec::Create
//
//==========================================================================
VAudioCodec *VFlacAudioCodec::Create (VStream *InStream) {
  // check if it's a FLAC file
  InStream->Seek(0);
  char Hdr[4];
  InStream->Serialise(Hdr, 4);
  if (InStream->IsError()) return nullptr;
  if ((Hdr[0] != 'f' || Hdr[1] != 'L' || Hdr[2] != 'a' || Hdr[3] != 'C') &&
      (Hdr[0] != 'o' || Hdr[1] != 'g' || Hdr[2] != 'g' || Hdr[3] != 'S'))
  {
    return nullptr;
  }

  VFlacAudioCodec *Codec = new VFlacAudioCodec(InStream, true);
  if (!Codec->Init()) {
    Codec->Cleanup();
    delete Codec;
    return nullptr;
  }
  return Codec;
}


//==========================================================================
//
//  VFlacSampleLoader::Create
//
//==========================================================================
void VFlacSampleLoader::Load (sfxinfo_t &Sfx, VStream &Stream) {
  VFlacAudioCodec *Codec = new VFlacAudioCodec(&Stream, false);
  if (!Codec->Init()) {
    Codec->Cleanup();
  } else {
    LoadFromAudioCodec(Sfx, Codec);
  }
  delete Codec;
}
