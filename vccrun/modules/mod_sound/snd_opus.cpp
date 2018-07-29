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

#include <opus/opusfile.h>

#include "sound.h"


class VOpusAudioCodec : public VAudioCodec {
public:
  int InitLevel;
  VStream *Strm;
  bool FreeStream;
  int BytesLeft;
  bool eos;

  OggOpusFile *opus; // nullptr means "open failed"
  OpusHead head;

  VOpusAudioCodec (VStream *, bool);
  virtual ~VOpusAudioCodec () override;
  bool Init ();
  void Cleanup ();
  virtual int Decode (short *, int) override;
  virtual bool Finished () override;
  virtual void Restart () override;

  static VAudioCodec *Create (VStream *);
};


class VOpusSampleLoader : public VSampleLoader {
public:
  virtual void Load (sfxinfo_t &, VStream &) override;
};

IMPLEMENT_AUDIO_CODEC(VOpusAudioCodec, "Opus");

VOpusSampleLoader OpusSampleLoader;


// ////////////////////////////////////////////////////////////////////////// //
static int opusStreamReadFn (void *astrm, unsigned char *_ptr, int _nbytes) {
  VStream *strm = (VStream *)astrm;
  if (strm->IsError()) return -1;
  auto left = strm->TotalSize()-strm->Tell();
  if (left < 1) return 0;
  if (_nbytes > left) _nbytes = left;
  strm->Serialize(_ptr, _nbytes);
  return (strm->IsError() ? -1 : _nbytes);
}


static int opusStreamSeekFn (void *astrm, opus_int64 _offset, int _whence) {
  VStream *strm = (VStream *)astrm;
  if (strm->IsError()) return -1;
  switch (_whence) {
    case 0: // SEEK_SET
      break;
    case 1: // SEEK_CUR
      _offset += strm->Tell();
      break;
    case 2: // SEEK_END
      _offset += strm->TotalSize();
      break;
    default: return -1;
  }
  if (_offset < 0) _offset = 0;
  if (_offset > strm->TotalSize()) _offset = strm->TotalSize();
  strm->Seek((int)_offset);
  return (strm->IsError() ? -1 : 0);
}


static opus_int64 opusStreamTellFn (void *astrm) {
  VStream *strm = (VStream *)astrm;
  return (strm->IsError() ? 0 : strm->Tell());
}


static const OpusFileCallbacks opusStreamCB = {
  .read = &opusStreamReadFn,
  .seek = &opusStreamSeekFn,
  .tell = &opusStreamTellFn,
  .close = nullptr,
};


//==========================================================================
//
//  VVorbisAudioCodec::VVorbisAudioCodec
//
//==========================================================================
VOpusAudioCodec::VOpusAudioCodec (VStream *AStrm, bool AFreeStream)
  : Strm(AStrm)
  , FreeStream(AFreeStream)
  , BytesLeft(0)
  , eos(false)
  , opus(nullptr)
{
  BytesLeft = Strm->TotalSize();
  Strm->Seek(0);
}


//==========================================================================
//
//  VVorbisAudioCodec::~VVorbisAudioCodec
//
//==========================================================================
VOpusAudioCodec::~VOpusAudioCodec () {
  if (opus) { op_free(opus); opus = nullptr; }
  if (InitLevel > 0) {
    Cleanup();
    if (FreeStream) {
      Strm->Close();
      delete Strm;
    }
    Strm = nullptr;
  }
}


//==========================================================================
//
//  VVorbisAudioCodec::Init
//
//==========================================================================
bool VOpusAudioCodec::Init () {
  //if (eos || BytesLeft == 0) return false;
  InitLevel = 0;

  //opus = op_test_callbacks(Strm, &opusStreamCB, nullptr, 0, nullptr);
  vuint8 buf[512];
  if (BytesLeft < 57 || Strm->IsError()) { BytesLeft = 0; eos = true; return false; }
  int tord = (BytesLeft < 512 ? BytesLeft : 512);
  Strm->Serialize(buf, tord);
  if (Strm->IsError()) { BytesLeft = 0; eos = true; return false; }
  if (op_test(&head, buf, tord) != 0) { BytesLeft = 0; eos = true; return false; }

  if (head.channel_count < 1 || head.channel_count > 2) return false;
  //fprintf(stderr, "channels: %d\n", head.channel_count);

  Strm->Seek(0);
  if (Strm->IsError()) { BytesLeft = 0; eos = true; return false; }
  opus = op_open_callbacks(Strm, &opusStreamCB, nullptr, 0, nullptr);
  if (!opus) { BytesLeft = 0; eos = true; return false; }

  SampleRate = 48000; // always
  SampleBits = 16;
  NumChannels = 2; //head.channel_count;
  InitLevel = 1;

  return true;
}


//==========================================================================
//
//  VVorbisAudioCodec::Cleanup
//
//==========================================================================
void VOpusAudioCodec::Cleanup () {
  if (opus) { op_free(opus); opus = nullptr; }
  InitLevel = 0;
}


//==========================================================================
//
//  VVorbisAudioCodec::Decode
//
//  `NumSamples` is number of frames, actually
//
//==========================================================================
int VOpusAudioCodec::Decode (short *Data, int NumSamples) {
  if (!opus) return 0;
  int CurSample = 0;
  while (!eos && CurSample < NumSamples) {
    int toread = (NumSamples-CurSample);
    auto rdsmp = op_read_stereo(opus, Data+CurSample*2, toread*2); // we always has two channels, and a room for this
    if (rdsmp == 0) { eos = true; break; }
    //fprintf(stderr, "toread: %d; rdsmp: %d\n", toread, rdsmp);
    CurSample += rdsmp;
  }
  if (!eos) eos = (Strm->IsError() || Strm->Tell() >= BytesLeft);
  return CurSample;
}


//==========================================================================
//
//  VVorbisAudioCodec::Finished
//
//==========================================================================
bool VOpusAudioCodec::Finished () {
  return eos;
}


//==========================================================================
//
//  VVorbisAudioCodec::Restart
//
//==========================================================================
void VOpusAudioCodec::Restart () {
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
VAudioCodec *VOpusAudioCodec::Create (VStream *InStrm) {
  VOpusAudioCodec *Codec = new VOpusAudioCodec(InStrm, true);
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
void VOpusSampleLoader::Load (sfxinfo_t &Sfx, VStream &Stream) {
  VOpusAudioCodec *Codec = new VOpusAudioCodec(&Stream, false);
  if (!Codec->Init()) {
    Codec->Cleanup();
    delete Codec;
    Codec = nullptr;
    return;
  }

  TArray<short> Data;
  do {
    short Buf[16*2048];
    int SamplesDecoded = Codec->Decode(Buf, 16*1024);
    if (SamplesDecoded > 0) {
      int OldPos = Data.length();
      Data.SetNumWithReserve(Data.length()+SamplesDecoded);
      for (int i = 0; i < SamplesDecoded; ++i) {
        // mix it
        int v = Buf[i*2]+Buf[i*2+1];
        if (v < -32768) v = -32768; else if (v > 32767) v = 32767;
        Data[OldPos+i] = v;
      }
    }
  } while (!Codec->Finished());

  if (!Data.length()) {
    delete Codec;
    Codec = nullptr;
    return;
  }

  // copy parameters
  Sfx.sampleRate = Codec->SampleRate;
  Sfx.sampleBits = Codec->SampleBits;

  // copy data
  Sfx.dataSize = Data.length()*2;
  Sfx.data = (vuint8 *)Z_Malloc(Data.length()*2);
  memcpy(Sfx.data, Data.Ptr(), Data.length()*2);

  delete Codec;
  Codec = nullptr;
}
