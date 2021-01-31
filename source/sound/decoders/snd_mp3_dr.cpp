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

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#define DR_MP3_NO_SIMD

#define DRMP3_ASSERT(expression)
#define DRMP3_MALLOC   Z_Malloc
#define DRMP3_REALLOC  Z_Realloc
#define DRMP3_FREE     Z_Free

#include "../stbdr/dr_mp3.h"


class VMP3AudioCodec : public VAudioCodec {
public:
  drmp3 decoder;
  VStream *Strm;
  bool FreeStream;
  size_t BytesLeft;
  bool eos;
  bool inited;
  bool decinited;
  vint16 *tmpbuf;
  int tmpbufsize;

  VMP3AudioCodec (VStream *AStrm, bool AFreeStream);
  virtual ~VMP3AudioCodec () override;
  bool Init ();
  void Cleanup ();
  virtual int Decode (vint16 *Data, int NumFrames) override;
  virtual bool Finished () override;
  virtual void Restart () override;

  static VAudioCodec *Create (VStream *InStrm, const vuint8 sign[], int signsize);

protected:
  static size_t readCB (void *pUserData, void *pBufferOut, size_t bytesToRead);
  static drmp3_bool32 seekCB (void *pUserData, int offset, drmp3_seek_origin origin);
};


/*
class VMP3SampleLoader : public VSampleLoader {
public:
  VMP3SampleLoader () : VSampleLoader(AUDIO_NO_SIGNATURE) {}
  virtual void Load (sfxinfo_t &, VStream &) override;
  virtual const char *GetName () const noexcept override;
};

VMP3SampleLoader MP3SampleLoader;
*/

IMPLEMENT_AUDIO_CODEC_EX(VMP3AudioCodec, "MP3(dr)", AUDIO_NO_SIGNATURE);


//==========================================================================
//
//  VMP3AudioCodec::VMP3AudioCodec
//
//==========================================================================
VMP3AudioCodec::VMP3AudioCodec (VStream *AStrm, bool AFreeStream)
  : Strm(AStrm)
  , FreeStream(AFreeStream)
  , BytesLeft(0)
  , eos(false)
  , inited(false)
  , decinited(false)
  , tmpbuf(nullptr)
  , tmpbufsize(0)
{
  memset((void *)&decoder, 0, sizeof(decoder));
  BytesLeft = Strm->TotalSize();
  Strm->Seek(0);
}


//==========================================================================
//
//  VMP3AudioCodec::~VMP3AudioCodec
//
//==========================================================================
VMP3AudioCodec::~VMP3AudioCodec () {
  Cleanup();
  if (inited && FreeStream && Strm) {
    Strm->Close();
    delete Strm;
  }
  Strm = nullptr;
}


//==========================================================================
//
//  VMP3AudioCodec::Cleanup
//
//==========================================================================
void VMP3AudioCodec::Cleanup () {
  if (decinited) { drmp3_uninit(&decoder); decinited = false; }
  if (tmpbuf) Z_Free(tmpbuf);
  tmpbuf = nullptr;
  tmpbufsize = 0;
}


//==========================================================================
//
//  VMP3AudioCodec::readCB
//
//==========================================================================
size_t VMP3AudioCodec::readCB (void *pUserData, void *pBufferOut, size_t bytesToRead) {
  if (bytesToRead == 0) return 0; // just in case
  VMP3AudioCodec *self = (VMP3AudioCodec *)pUserData;
  if (!self->Strm || self->BytesLeft == 0 || self->Strm->IsError()) return 0;
  if (bytesToRead > (unsigned)self->BytesLeft) bytesToRead = (unsigned)self->BytesLeft;
  self->Strm->Serialise(pBufferOut, (int)bytesToRead);
  if (self->Strm->IsError()) return 0;
  self->BytesLeft -= (int)bytesToRead;
  return bytesToRead;
}


//==========================================================================
//
//  VMP3AudioCodec::seekCB
//
//==========================================================================
drmp3_bool32 VMP3AudioCodec::seekCB (void *pUserData, int offset, drmp3_seek_origin origin) {
  VMP3AudioCodec *self = (VMP3AudioCodec *)pUserData;
  if (!self->Strm || self->Strm->IsError()) return DRMP3_FALSE;
  if (origin == drmp3_seek_origin_current) offset += self->Strm->Tell();
  if (offset < 0 || offset > self->Strm->TotalSize()) return DRMP3_FALSE;
  self->Strm->Seek(offset);
  if (self->Strm->IsError()) return DRMP3_FALSE;
  self->BytesLeft = self->Strm->TotalSize()-self->Strm->Tell();
  return DRMP3_TRUE;
}


//==========================================================================
//
//  VMP3AudioCodec::Init
//
//==========================================================================
bool VMP3AudioCodec::Init () {
  Cleanup();

  eos = false;
  inited = false;
  vassert(!decinited);

  //GCon->Logf(NAME_Debug, "buffer filled (%s) (%d/%d/%d)", *Strm->GetName(), inbufUsed, inbufFilled, inbufSize);
  memset((void *)&decoder, 0, sizeof(decoder));
  drmp3_bool32 ok = drmp3_init(&decoder, &readCB, &seekCB, (void *)this, nullptr);
  if (!ok) {
    //GCon->Logf(NAME_Debug, "stb_vorbis error: %d", error);
    Cleanup();
    return false;
  }
  decinited = true;

  if (decoder.sampleRate < 64 || decoder.sampleRate > 96000*2) {
    GCon->Logf(NAME_Warning, "cannot open mp3 '%s' with sample rate %u", *Strm->GetName(), (unsigned)decoder.sampleRate);
    Cleanup();
    return false;
  }

  if (decoder.channels < 1 || decoder.channels > 2) {
    GCon->Logf(NAME_Warning, "cannot open mp3 '%s' with %u channels", *Strm->GetName(), (unsigned)decoder.channels);
    Cleanup();
    return false;
  }

  SampleRate = decoder.sampleRate;
  SampleBits = 16;
  NumChannels = 2; // always
  inited = true;
  return true;
}


//==========================================================================
//
//  VMP3AudioCodec::Decode
//
//==========================================================================
int VMP3AudioCodec::Decode (vint16 *Data, int NumFrames) {
  int CurFrame = 0;
  if (eos) return 0;
  while (!eos && CurFrame < NumFrames) {
    if (decoder.channels == 2) {
      drmp3_uint64 rd = drmp3_read_pcm_frames_s16(&decoder, (unsigned)(NumFrames-CurFrame), (drmp3_int16 *)(Data+CurFrame));
      if (rd < (unsigned)(NumFrames-CurFrame)) eos = true;
      CurFrame += (int)rd;
    } else {
      // read mono data
      if (tmpbufsize < NumFrames-CurFrame) {
        tmpbufsize = NumFrames-CurFrame;
        tmpbuf = (vint16 *)Z_Realloc(tmpbuf, tmpbufsize*2);
      }
      drmp3_uint64 rd = drmp3_read_pcm_frames_s16(&decoder, (unsigned)(NumFrames-CurFrame), (drmp3_int16 *)tmpbuf);
      // expand it to stereo
      for (int f = 0; f < (int)rd; ++f) {
        Data[(CurFrame+f)*2+0] = Data[(CurFrame+f)*2+1] = tmpbuf[f];
      }
      if (rd < (unsigned)(NumFrames-CurFrame)) eos = true;
      CurFrame += (int)rd;
    }
  }
  return CurFrame;
}


//==========================================================================
//
//  VMP3AudioCodec::Finished
//
//==========================================================================
bool VMP3AudioCodec::Finished () {
  return (eos);
}


//==========================================================================
//
//  VMP3AudioCodec::Restart
//
//==========================================================================
void VMP3AudioCodec::Restart () {
  Strm->Seek(0);
  BytesLeft = Strm->TotalSize();
  Init();
}


//==========================================================================
//
//  VMP3AudioCodec::Create
//
//==========================================================================
VAudioCodec *VMP3AudioCodec::Create (VStream *InStream, const vuint8 sign[], int signsize) {
  VMP3AudioCodec *Codec = new VMP3AudioCodec(InStream, true);
  if (!Codec->Init()) {
    Codec->Cleanup();
    delete Codec;
    return nullptr;
  }
  return Codec;
}



/*
//==========================================================================
//
//  VMP3SampleLoader::Load
//
//==========================================================================
void VMP3SampleLoader::Load (sfxinfo_t &Sfx, VStream &Stream) {
  VMP3AudioCodec *Codec = new VMP3AudioCodec(&Stream, false);
  if (!Codec->Init()) {
    Codec->Cleanup();
  } else {
    LoadFromAudioCodec(Sfx, Codec);
  }
  delete Codec;
}


//==========================================================================
//
//  VMP3SampleLoader::GetName
//
//==========================================================================
const char *VMP3SampleLoader::GetName () const noexcept {
  return "mp3/dr";
}
*/
