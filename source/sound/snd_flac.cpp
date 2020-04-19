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
#include <FLAC/stream_decoder.h>

#include "gamedefs.h"
#include "snd_local.h"


class VFlacSampleLoader : public VSampleLoader {
public:
  class FStream {
  public:
    FLAC__StreamDecoder *decoder;
    VStream &Strm;
    size_t BytesLeft;
    int SampleBits;
    int SampleRate;
    void *Data;
    size_t DataSize;
    vuint32 lastSeenFrame;
    bool loopDetected;

    FStream (VStream &InStream);
    ~FStream ();

    bool Init ();
    void StrmWrite (const FLAC__int32 *const Buf[], size_t Offs, size_t Len);
    bool ProcessAll ();

  protected:
    // flac decoder callbacks
    static FLAC__StreamDecoderReadStatus read_callback (const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
    static FLAC__StreamDecoderWriteStatus write_callback (const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
    static void metadata_callback (const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
    static void error_callback (const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);
  };

  virtual void Load (sfxinfo_t &, VStream &) override;
};


class VFlacAudioCodec : public VAudioCodec {
public:
  class FStream {
  public:
    FLAC__StreamDecoder *decoder;
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
    ~FStream ();

    bool Init ();
    void StrmWrite (const FLAC__int32 *const Buf[], size_t Offs, size_t Len);
    bool ProcessSingle ();
    bool IsEndOfStream ();

  protected:
    // flac decoder callbacks
    static FLAC__StreamDecoderReadStatus read_callback (const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
    static FLAC__StreamDecoderWriteStatus write_callback (const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
    static void metadata_callback (const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
    static void error_callback (const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);
  };

  FStream *Stream;

  VFlacAudioCodec (FStream *InStream);
  virtual ~VFlacAudioCodec () override;
  virtual int Decode (short *Data, int NumSamples) override;
  virtual bool Finished () override;
  virtual void Restart () override;
  static VAudioCodec *Create (VStream *InStream);
};


VFlacSampleLoader FlacSampleLoader;

IMPLEMENT_AUDIO_CODEC(VFlacAudioCodec, "FLAC");


//==========================================================================
//
//  VFlacSampleLoader::Load
//
//==========================================================================
void VFlacSampleLoader::Load (sfxinfo_t &Sfx, VStream &Stream) {
  // check if it's a flac file
  Stream.Seek(0);
  char Hdr[4];
  Stream.Serialise(Hdr, 4);
  if (Hdr[0] != 'f' || Hdr[1] != 'L' || Hdr[2] != 'a' || Hdr[3] != 'C') return;
  // create reader sream
  FStream *Strm = new FStream(Stream);
  Strm->Data = Z_Malloc(1);
  if (!Strm->SampleRate) {
    Z_Free(Strm->Data);
    Sfx.Data = nullptr;
    delete Strm;
    return;
  }
  if (!Strm->ProcessAll()) {
    if (!Strm->loopDetected || Strm->DataSize == 0) {
      GCon->Logf("Failed to process FLAC file");
      Z_Free(Strm->Data);
      Sfx.Data = nullptr;
      delete Strm;
      return;
    }
  }
  Sfx.SampleRate = Strm->SampleRate;
  Sfx.SampleBits = Strm->SampleBits;
  Sfx.DataSize = Strm->DataSize;
  Sfx.Data = Strm->Data;
  delete Strm;
}


//==========================================================================
//
//  VFlacSampleLoader::FStream::FStream
//
//==========================================================================
VFlacSampleLoader::FStream::FStream (VStream &InStream)
  : decoder(nullptr)
  , Strm(InStream)
  , SampleBits(0)
  , SampleRate(0)
  , Data(0)
  , DataSize(0)
  , lastSeenFrame(0)
  , loopDetected(0)
{
  Strm.Seek(0);
  BytesLeft = Strm.TotalSize();
  Init();
}


//==========================================================================
//
//  VFlacSampleLoader::FStream::~FStream
//
//==========================================================================
VFlacSampleLoader::FStream::~FStream () {
  if (decoder) {
    FLAC__stream_decoder_delete(decoder);
    decoder = nullptr;
  }
}


//==========================================================================
//
//  VFlacSampleLoader::FStream::Init
//
//==========================================================================
bool VFlacSampleLoader::FStream::Init () {
  if (decoder) FLAC__stream_decoder_delete(decoder);
  decoder = FLAC__stream_decoder_new();
  if (!decoder) return false;
  FLAC__stream_decoder_set_md5_checking(decoder, false);
  //init_status = FLAC__stream_decoder_init_file(decoder, argv[1], write_callback, metadata_callback, error_callback, /*client_data=*/fout);
  FLAC__StreamDecoderInitStatus init_status = FLAC__stream_decoder_init_stream(
    decoder,
    &read_callback,
    nullptr/*seek_callback*/,
    nullptr/*tell_callback*/,
    nullptr/*length_callback*/,
    nullptr/*eof_callback*/,
    &write_callback,
    &metadata_callback,
    &error_callback, (void *)this);
  if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
    FLAC__stream_decoder_delete(decoder);
    decoder = nullptr;
    return false;
  }
  FLAC__stream_decoder_process_until_end_of_metadata(decoder);
  return (SampleRate != 0);
}


//==========================================================================
//
//  VFlacSampleLoader::FStream::ProcessAll
//
//==========================================================================
bool VFlacSampleLoader::FStream::ProcessAll () {
  if (!decoder) return false;
  if (!FLAC__stream_decoder_process_until_end_of_stream(decoder)) return false;
  return true;
}


//==========================================================================
//
//  VFlacSampleLoader::FStream::read_callback
//
//==========================================================================
FLAC__StreamDecoderReadStatus VFlacSampleLoader::FStream::read_callback (const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data) {
  VFlacSampleLoader::FStream *self = (VFlacSampleLoader::FStream *)client_data;
  if (*bytes > 0) {
    if (!self->BytesLeft) return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    if (self->loopDetected) return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    if (*bytes > self->BytesLeft) *bytes = self->BytesLeft;
    self->Strm.Serialise(buffer, *bytes);
    self->BytesLeft -= *bytes;
    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
  } else {
    return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
  }
}


//==========================================================================
//
//  VFlacSampleLoader::FStream::write_callback
//
//==========================================================================
FLAC__StreamDecoderWriteStatus VFlacSampleLoader::FStream::write_callback (const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data) {
  VFlacSampleLoader::FStream *self = (VFlacSampleLoader::FStream *)client_data;
  if ((vuint32)frame->header.number.frame_number+1 <= self->lastSeenFrame) {
    GCon->Logf(NAME_Warning, "FLAC: looped sample detected (%u:%u : %u) (%s)", (vuint32)frame->header.number.frame_number, self->lastSeenFrame, (vuint32)frame->header.number.sample_number, *self->Strm.GetName());
    self->loopDetected = true;
    //return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }
  self->lastSeenFrame = (vuint32)frame->header.number.frame_number+1;

  void *Temp = self->Data;
  self->Data = Z_Malloc(self->DataSize+frame->header.blocksize*self->SampleBits/8);
  memcpy(self->Data, Temp, self->DataSize);
  Z_Free(Temp);

  const FLAC__int32 *pSrc = buffer[0];
  if (self->SampleBits == 8) {
    vuint8 *pDst = (vuint8 *)self->Data+self->DataSize;
    for (size_t j = 0; j < frame->header.blocksize; ++j, ++pSrc, ++pDst) *pDst = vuint8(*pSrc)^0x80;
  } else {
    vint16 *pDst = (vint16 *)((vuint8 *)self->Data+self->DataSize);
    for (size_t j = 0; j < frame->header.blocksize; ++j, ++pSrc, ++pDst) *pDst = vint16(*pSrc);
  }
  self->DataSize += frame->header.blocksize*self->SampleBits/8;
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}


//==========================================================================
//
//  VFlacSampleLoader::FStream::metadata_callback
//
//==========================================================================
void VFlacSampleLoader::FStream::metadata_callback (const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data) {
  VFlacSampleLoader::FStream *self = (VFlacSampleLoader::FStream *)client_data;
  if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
    if (metadata->data.stream_info.bits_per_sample != 8 &&
        metadata->data.stream_info.bits_per_sample != 16)
    {
      GCon->Log("Only 8 and 16 bit FLAC files are supported");
      return;
    }
    //if (metadata->data.stream_info.channels != 1) GCon->Log("Stereo FLAC, taking left channel");
    self->SampleRate = metadata->data.stream_info.sample_rate;
    self->SampleBits = metadata->data.stream_info.bits_per_sample;
  }
}


//==========================================================================
//
//  VFlacSampleLoader::FStream::error_callback
//
//==========================================================================
void VFlacSampleLoader::FStream::error_callback (const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data) {
}


//==========================================================================
//
//  VFlacAudioCodec::VFlacAudioCodec
//
//==========================================================================
VFlacAudioCodec::VFlacAudioCodec (FStream *InStream)
  : Stream(InStream)
{
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

  while (Stream->StrmSize > 0 && !Finished()) {
    if (!Stream->ProcessSingle()) break;
  }

  return NumSamples-Stream->StrmSize;
}


//==========================================================================
//
//  VFlacAudioCodec::Finished
//
//==========================================================================
bool VFlacAudioCodec::Finished () {
  //return (Stream->get_state() == FLAC__STREAM_DECODER_END_OF_STREAM);
  return Stream->IsEndOfStream();
}


//==========================================================================
//
//  VFlacAudioCodec::Restart
//
//==========================================================================
void VFlacAudioCodec::Restart () {
  Stream->Strm->Seek(0);
  Stream->BytesLeft = Stream->Strm->TotalSize();
  //Stream->reset();
  Stream->Init();
}


//==========================================================================
//
//  VFlacAudioCodec::FStream::FStream
//
//==========================================================================
VFlacAudioCodec::FStream::FStream (VStream *InStream)
  : decoder(nullptr)
  , Strm(InStream)
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
  Init();
}


//==========================================================================
//
//  VFlacAudioCodec::FStream::~FStream
//
//==========================================================================
VFlacAudioCodec::FStream::~FStream () {
  if (decoder) {
    FLAC__stream_decoder_delete(decoder);
    decoder = nullptr;
  }
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
//  VFlacAudioCodec::FStream::Init
//
//==========================================================================
bool VFlacAudioCodec::FStream::Init () {
  if (decoder) FLAC__stream_decoder_delete(decoder);
  decoder = FLAC__stream_decoder_new();
  if (!decoder) return false;
  FLAC__stream_decoder_set_md5_checking(decoder, false);
  //init_status = FLAC__stream_decoder_init_file(decoder, argv[1], write_callback, metadata_callback, error_callback, /*client_data=*/fout);
  FLAC__StreamDecoderInitStatus init_status = FLAC__stream_decoder_init_stream(
    decoder,
    &read_callback,
    nullptr/*seek_callback*/,
    nullptr/*tell_callback*/,
    nullptr/*length_callback*/,
    nullptr/*eof_callback*/,
    &write_callback,
    &metadata_callback,
    &error_callback, (void *)this);
  if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
    //GCon->Logf(NAME_Debug, "cannot init flac decoder for '%s'", *Strm->GetName());
    FLAC__stream_decoder_delete(decoder);
    decoder = nullptr;
    return false;
  }
  FLAC__stream_decoder_process_until_end_of_metadata(decoder);
  return (SampleRate != 0);
}


//==========================================================================
//
//  VFlacAudioCodec::FStream::ProcessSingle
//
//==========================================================================
bool VFlacAudioCodec::FStream::ProcessSingle () {
  if (!decoder) return false;
  if (!FLAC__stream_decoder_process_single(decoder)) return false;
  return true;
}


//==========================================================================
//
//  VFlacAudioCodec::FStream::IsEndOfStream
//
//==========================================================================
bool VFlacAudioCodec::FStream::IsEndOfStream () {
  if (!decoder) return true;
  return (FLAC__stream_decoder_get_state(decoder) == FLAC__STREAM_DECODER_END_OF_STREAM);
}


//==========================================================================
//
//  VFlacAudioCodec::FStream::StrmWrite
//
//==========================================================================
void VFlacAudioCodec::FStream::StrmWrite (const FLAC__int32 *const Buf[], size_t Offs, size_t Len) {
  for (int i = 0; i < 2; ++i) {
    const FLAC__int32 *pSrc = Buf[NumChannels == 1 ? 0 : i]+Offs;
    short *pDst = StrmBuf+i;
    if (SampleBits == 8) {
      for (size_t j = 0; j < Len; j++, pSrc++, pDst += 2) *pDst = char(*pSrc) << 8;
    } else {
      for (size_t j = 0; j < Len; j++, pSrc++, pDst += 2) *pDst = short(*pSrc);
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
FLAC__StreamDecoderReadStatus VFlacAudioCodec::FStream::read_callback (const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data) {
  VFlacAudioCodec::FStream *self = (VFlacAudioCodec::FStream *)client_data;
  if (*bytes > 0) {
    if (!self->BytesLeft) return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    if (*bytes > self->BytesLeft) *bytes = self->BytesLeft;
    self->Strm->Serialise(buffer, *bytes);
    self->BytesLeft -= *bytes;
    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
  } else {
    return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
  }
}


//==========================================================================
//
//  VFlacAudioCodec::FStream::write_callback
//
//==========================================================================
FLAC__StreamDecoderWriteStatus VFlacAudioCodec::FStream::write_callback (const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data) {
  VFlacAudioCodec::FStream *self = (VFlacAudioCodec::FStream *)client_data;
  size_t blockSize = frame->header.blocksize;
  size_t blockGrab = 0;
  size_t blockOfs;

  blockGrab = min2(self->StrmSize, blockSize);
  self->StrmWrite(buffer, 0, blockGrab);
  blockSize -= blockGrab;
  blockOfs = blockGrab;

  if (blockSize > 0) {
    blockGrab = self->PoolSize-self->PoolUsed;
    if (blockGrab > blockSize) blockGrab = blockSize;
    memcpy(self->SamplePool[0]+self->PoolUsed, buffer[0]+blockOfs, sizeof(*buffer[0])*blockGrab);
    if (self->NumChannels > 1) {
      memcpy(self->SamplePool[1]+self->PoolUsed, buffer[1]+blockOfs, sizeof(*buffer[1])*blockGrab);
    }
    self->PoolUsed += blockGrab;
  }

  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}


//==========================================================================
//
//  VFlacAudioCodec::FStream::metadata_callback
//
//==========================================================================
void VFlacAudioCodec::FStream::metadata_callback (const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data) {
  VFlacAudioCodec::FStream *self = (VFlacAudioCodec::FStream *)client_data;
  if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO && self->PoolSize == 0) {
    if (metadata->data.stream_info.bits_per_sample != 8 &&
        metadata->data.stream_info.bits_per_sample != 16)
    {
      GCon->Log("Only 8 and 16 bit FLAC files are supported");
      return;
    }
    self->SampleRate = metadata->data.stream_info.sample_rate;
    self->NumChannels = min2((unsigned)2, metadata->data.stream_info.channels);
    self->SampleBits = metadata->data.stream_info.bits_per_sample;
    self->PoolSize = metadata->data.stream_info.max_blocksize*2;

    self->SamplePool[0] = (FLAC__int32 *)Z_Malloc(sizeof(FLAC__int32)*self->PoolSize*self->NumChannels);
    self->SamplePool[1] = self->SamplePool[0]+self->PoolSize;
  }
}


//==========================================================================
//
//  VFlacAudioCodec::FStream::error_callback
//
//==========================================================================
void VFlacAudioCodec::FStream::error_callback (const FLAC__StreamDecoder * /*decoder*/, FLAC__StreamDecoderErrorStatus /*status*/, void * /*client_data*/) {
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
  if (Hdr[0] != 'f' || Hdr[1] != 'L' || Hdr[2] != 'a' || Hdr[3] != 'C') return nullptr;

  FStream *Strm = new FStream(InStream);
  if (!Strm->SampleRate) {
    delete Strm;
    Strm = nullptr;
    return nullptr;
  }
  return new VFlacAudioCodec(Strm);
}
