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
#include <FLAC++/decoder.h>

#include "../sound_private.h"
#include "../sound.h"


// ////////////////////////////////////////////////////////////////////////// //
class VFlacSampleLoader : public VSampleLoader {
public:
  class FStream : public FLAC::Decoder::Stream {
  public:
    VStream &Strm;
    size_t BytesLeft;
    int SampleBits;
    int SampleRate;
    void *Data;
    size_t DataSize;

    FStream (VStream &InStream);
    //virtual ~FStream () {}
    void StrmWrite (const FLAC__int32 *const Buf[], size_t Offs, size_t Len);

  protected:
    // FLAC decoder callbacks
    virtual ::FLAC__StreamDecoderReadStatus read_callback (FLAC__byte buffer[], size_t *bytes) override;
    virtual ::FLAC__StreamDecoderWriteStatus write_callback (const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[]) override;
    virtual void metadata_callback (const ::FLAC__StreamMetadata *metadata) override;
    virtual void error_callback (::FLAC__StreamDecoderErrorStatus status) override;
  };

  virtual void Load (sfxinfo_t&, VStream&) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VFlacAudioCodec : public VAudioCodec {
public:
  class FStream : public FLAC::Decoder::Stream {
  public:
    VStream *Strm;
    size_t BytesLeft;
    int NumChannels;
    int SampleBits;
    int SampleRate;

    FLAC__int32 *SamplePool[2];
    size_t PoolSize;
    size_t PoolUsed;
    size_t PoolPos;

    short *StrmBuf;
    size_t StrmSize;

    FStream (VStream *InStream);
    virtual ~FStream ();
    void StrmWrite (const FLAC__int32 *const Buf[], size_t Offs, size_t Len);

  protected:
    // FLAC decoder callbacks
    virtual ::FLAC__StreamDecoderReadStatus read_callback (FLAC__byte buffer[], size_t *bytes) override;
    virtual ::FLAC__StreamDecoderWriteStatus write_callback (const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[]) override;
    virtual void metadata_callback (const ::FLAC__StreamMetadata *metadata) override;
    virtual void error_callback (::FLAC__StreamDecoderErrorStatus status) override;
  };

  FStream *Stream;

  VFlacAudioCodec (FStream *InStream);
  virtual ~VFlacAudioCodec () override;

  virtual int Decode (short *Data, int NumSamples) override;
  virtual bool Finished () override;
  virtual void Restart () override;

  static VAudioCodec *Create(VStream *InStream);
};


// ////////////////////////////////////////////////////////////////////////// //
static VFlacSampleLoader FlacSampleLoader;

IMPLEMENT_AUDIO_CODEC(VFlacAudioCodec, "FLAC");


//==========================================================================
//
//  VFlacSampleLoader::Load
//
//==========================================================================
void VFlacSampleLoader::Load (sfxinfo_t &Sfx, VStream &Stream) {
  // check if it's a FLAC file
  Stream.Seek(0);
  char hdr[4];
  Stream.Serialise(hdr, 4);
  if (hdr[0] != 'f' || hdr[1] != 'L' || hdr[2] != 'a' || hdr[3] != 'C') return;

  // create reader sream
  FStream *Strm = new FStream(Stream);
  Strm->Data = Z_Malloc(1);
  Strm->init();
  Strm->process_until_end_of_metadata();
  if (!Strm->SampleRate) {
    Z_Free(Strm->Data);
    Sfx.data = nullptr;
    delete Strm;
    return;
  }
  if (!Strm->process_until_end_of_stream()) {
    fprintf(stderr, "WARNING: failed to process FLAC file.\n");
    Z_Free(Strm->Data);
    Sfx.data = nullptr;
    delete Strm;
    return;
  }
  Sfx.sampleRate = Strm->SampleRate;
  Sfx.sampleBits = Strm->SampleBits;
  Sfx.dataSize = Strm->DataSize;
  Sfx.data = (vuint8 *)Strm->Data;
  delete Strm;
}


//==========================================================================
//
//  VFlacSampleLoader::FStream::FStream
//
//==========================================================================
VFlacSampleLoader::FStream::FStream (VStream &InStream)
  : Strm(InStream)
  , SampleBits(0)
  , SampleRate(0)
  , Data(0)
  , DataSize(0)
{
  Strm.Seek(0);
  BytesLeft = Strm.TotalSize();
}


//==========================================================================
//
//  VFlacSampleLoader::FStream::read_callback
//
//==========================================================================
::FLAC__StreamDecoderReadStatus VFlacSampleLoader::FStream::read_callback (FLAC__byte buffer[], size_t *bytes) {
  if (*bytes > 0) {
    if (!BytesLeft) return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    if (*bytes > BytesLeft) *bytes = BytesLeft;
    Strm.Serialise(buffer, *bytes);
    BytesLeft -= *bytes;
    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
  }
  return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
}


//==========================================================================
//
//  VFlacSampleLoader::FStream::write_callback
//
//==========================================================================
::FLAC__StreamDecoderWriteStatus VFlacSampleLoader::FStream::write_callback (const ::FLAC__Frame *frame, const FLAC__int32 *const buffer[]) {
  void *Temp = Data;
  Data = Z_Malloc(DataSize+frame->header.blocksize*SampleBits/8);
  memcpy(Data, Temp, DataSize);
  Z_Free(Temp);

  const FLAC__int32 *pSrc = buffer[0];
  if (SampleBits == 8) {
    vuint8 *pDst = (vuint8 *)Data+DataSize;
    for (size_t j = 0; j < frame->header.blocksize; ++j, ++pSrc, ++pDst) *pDst = vuint8(*pSrc)^0x80;
  } else {
    vint16 *pDst = (vint16 *)((vuint8 *)Data+DataSize);
    for (size_t j = 0; j < frame->header.blocksize; ++j, ++pSrc, ++pDst) *pDst = vint16(*pSrc);
  }
  DataSize += frame->header.blocksize*SampleBits/8;
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}


//==========================================================================
//
//  VFlacSampleLoader::FStream::metadata_callback
//
//==========================================================================
void VFlacSampleLoader::FStream::metadata_callback (const ::FLAC__StreamMetadata *metadata) {
  if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
    if (metadata->data.stream_info.bits_per_sample != 8 && metadata->data.stream_info.bits_per_sample != 16) {
      fprintf(stderr, "WARNING: only 8 and 16 bit FLAC files are supported.\n");
      return;
    }
    if (metadata->data.stream_info.channels != 1) {
      fprintf(stderr, "WARNING: stereo FLAC, taking left channel.\n");
    }
    SampleRate = metadata->data.stream_info.sample_rate;
    SampleBits = metadata->data.stream_info.bits_per_sample;
  }
}


//==========================================================================
//
//  VFlacSampleLoader::FStream::error_callback
//
//==========================================================================
void VFlacSampleLoader::FStream::error_callback (::FLAC__StreamDecoderErrorStatus) {
}



//==========================================================================
//
//  VFlacAudioCodec::VFlacAudioCodec
//
//==========================================================================
VFlacAudioCodec::VFlacAudioCodec (FStream *InStream) : Stream(InStream) {
  SampleRate = Stream->SampleRate;
}


//==========================================================================
//
//  VFlacAudioCodec::~VFlacAudioCodec
//
//==========================================================================
VFlacAudioCodec::~VFlacAudioCodec () {
  delete Stream;
  Stream = nullptr;
}


//==========================================================================
//
//  VFlacAudioCodec::Decode
//
//==========================================================================
int VFlacAudioCodec::Decode (short *Data, int NumSamples) {
  Stream->StrmBuf = Data;
  Stream->StrmSize = NumSamples;

  if (Stream->PoolUsed > Stream->PoolPos) {
    size_t poolGrab = Stream->PoolUsed-Stream->PoolPos;
    if (poolGrab > (size_t)NumSamples) poolGrab = (size_t)NumSamples;
    Stream->StrmWrite(Stream->SamplePool, Stream->PoolPos, poolGrab);
    Stream->PoolPos += poolGrab;
    if (Stream->PoolPos == Stream->PoolUsed) {
      Stream->PoolPos = 0;
      Stream->PoolUsed = 0;
    }
  }

  while (Stream->StrmSize > 0 && !Finished()) if (!Stream->process_single()) break;
  return NumSamples-Stream->StrmSize;
}


//==========================================================================
//
//  VFlacAudioCodec::Finished
//
//==========================================================================
bool VFlacAudioCodec::Finished () {
  return (Stream->get_state() == FLAC__STREAM_DECODER_END_OF_STREAM);
}


//==========================================================================
//
//  VFlacAudioCodec::Restart
//
//==========================================================================
void VFlacAudioCodec::Restart () {
  Stream->Strm->Seek(0);
  Stream->BytesLeft = Stream->Strm->TotalSize();
  Stream->reset();
}


//==========================================================================
//
//  VFlacAudioCodec::FStream::FStream
//
//==========================================================================
VFlacAudioCodec::FStream::FStream (VStream *InStream)
  : Strm(InStream)
  , NumChannels(0)
  , SampleBits(0)
  , SampleRate(0)
  , PoolSize(0)
  , PoolUsed(0)
  , PoolPos(0)
  , StrmBuf(0)
  , StrmSize(0)
{
  Strm->Seek(0);
  BytesLeft = Strm->TotalSize();
  init();
  process_until_end_of_metadata();
}


//==========================================================================
//
//  VFlacAudioCodec::FStream::~FStream
//
//==========================================================================
VFlacAudioCodec::FStream::~FStream () {
  if (PoolSize > 0 && SamplePool[0] != nullptr) {
    Z_Free(SamplePool[0]);
    SamplePool[0] = nullptr;
    Strm->Close();
    delete Strm;
    Strm = nullptr;
  }
}


//==========================================================================
//
//  VFlacAudioCodec::FStream::StrmWrite
//
//==========================================================================
void VFlacAudioCodec::FStream::StrmWrite (const FLAC__int32 *const Buf[], size_t Offs, size_t Len) {
  for (int i = 0; i < 2; ++i) {
    const FLAC__int32 *pSrc = Buf[NumChannels == 1 ? 0 : i] + Offs;
    short *pDst = StrmBuf+i;
    if (SampleBits == 8) {
      for (size_t j = 0; j < Len; ++j, ++pSrc, pDst += 2) *pDst = char(*pSrc)<<8;
    } else {
      for (size_t j = 0; j < Len; ++j, ++pSrc, pDst += 2) *pDst = short(*pSrc);
    }
  }
  StrmBuf += Len*2;
  StrmSize -= Len;
}


//==========================================================================
//
//  VFlacAudioCodec::FStream::read_callback
//
//==========================================================================
::FLAC__StreamDecoderReadStatus VFlacAudioCodec::FStream::read_callback (FLAC__byte buffer[], size_t *bytes) {
  if (*bytes > 0) {
    if (!BytesLeft) return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    if (*bytes > BytesLeft) *bytes = BytesLeft;
    Strm->Serialise(buffer, *bytes);
    BytesLeft -= *bytes;
    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
  }
  return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
}


//==========================================================================
//
//  VFlacAudioCodec::FStream::write_callback
//
//==========================================================================
::FLAC__StreamDecoderWriteStatus VFlacAudioCodec::FStream::write_callback (const ::FLAC__Frame *frame, const FLAC__int32 *const buffer[]) {
  size_t blockSize = frame->header.blocksize;
  size_t blockGrab = 0;
  size_t blockOfs;

  blockGrab = MIN(StrmSize, blockSize);
  StrmWrite(buffer, 0, blockGrab);
  blockSize -= blockGrab;
  blockOfs = blockGrab;

  if (blockSize > 0) {
    blockGrab = PoolSize-PoolUsed;
    if (blockGrab > blockSize) blockGrab = blockSize;
    memcpy(SamplePool[0]+PoolUsed, buffer[0]+blockOfs, sizeof(*buffer[0])*blockGrab);
    if (NumChannels > 1) memcpy(SamplePool[1]+PoolUsed, buffer[1]+blockOfs, sizeof(*buffer[1])*blockGrab);
    PoolUsed += blockGrab;
  }

  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}


//==========================================================================
//
//  VFlacAudioCodec::FStream::metadata_callback
//
//==========================================================================
void VFlacAudioCodec::FStream::metadata_callback (const ::FLAC__StreamMetadata *metadata) {
  if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO && PoolSize == 0) {
    if (metadata->data.stream_info.bits_per_sample != 8 && metadata->data.stream_info.bits_per_sample != 16) {
      fprintf(stderr, "WARNING: only 8 and 16 bit FLAC files are supported.\n");
      return;
    }
    SampleRate = metadata->data.stream_info.sample_rate;
    NumChannels = MIN((unsigned)2, metadata->data.stream_info.channels);
    SampleBits = metadata->data.stream_info.bits_per_sample;
    PoolSize = metadata->data.stream_info.max_blocksize*2;

    if (metadata->data.stream_info.channels < 1 || metadata->data.stream_info.channels > 2) {
      fprintf(stderr, "WARNING: Only mono and stereo FLACS are supported.\n");
    }

    SamplePool[0] = (FLAC__int32*)Z_Malloc(sizeof(FLAC__int32)*PoolSize*NumChannels);
    SamplePool[1] = SamplePool[0]+PoolSize;
  }
}


//==========================================================================
//
//  VFlacAudioCodec::FStream::error_callback
//
//==========================================================================
void VFlacAudioCodec::FStream::error_callback (::FLAC__StreamDecoderErrorStatus) {
}


//==========================================================================
//
//  VFlacAudioCodec::Create
//
//==========================================================================
VAudioCodec *VFlacAudioCodec::Create (VStream *InStream) {
  // check if it's a FLAC file
  InStream->Seek(0);
  char hdr[4];
  InStream->Serialise(hdr, 4);
  if (hdr[0] != 'f' || hdr[1] != 'L' || hdr[2] != 'a' || hdr[3] != 'C') return nullptr;

  FStream *Strm = new FStream(InStream);
  if (!Strm->SampleRate) {
    delete Strm;
    return nullptr;
  }
  return new VFlacAudioCodec(Strm);
}
