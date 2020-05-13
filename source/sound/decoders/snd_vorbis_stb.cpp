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
#include "../../gamedefs.h"
#include "../snd_local.h"

/*#define STB_VORBIS_NO_PUSHDATA_API*/
#define STB_VORBIS_NO_PULLDATA_API
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_FAST_SCALED_FLOAT

#define stb_malloc   Z_Malloc
#define stb_realloc  Z_Realloc
#define stb_free     Z_Free

#include "../stbdr/stb_vorbis.c"


class VVorbisAudioCodec : public VAudioCodec {
public:
  VStream *Strm;
  bool FreeStream;
  int BytesLeft;
  stb_vorbis *decoder;
  stb_vorbis_alloc tmpbuf;
  bool eos;
  vuint8 *inbuf;
  int inbufSize;
  int inbufUsed;
  int inbufFilled;
  vint16 *outbuf; // stereo
  int outbufSize; // in shorts
  int outbufUsed; // in shorts
  int outbufFilled; // in shorts
  bool inited;

  VVorbisAudioCodec (VStream *AStrm, bool AFreeStream);
  virtual ~VVorbisAudioCodec () override;
  bool Init ();
  void Cleanup ();
  virtual int Decode (vint16 *Data, int NumFrames) override;
  virtual bool Finished () override;
  virtual void Restart () override;

  static VAudioCodec *Create (VStream *InStrm, const vuint8 sign[], int signsize);

protected:
  void stopFeeding (bool setEOS=false);
  // returns `false` on error or eof
  bool fillInBuffer ();
  // returns `false` on error or eof
  bool decodeFrame ();
};


class VVorbisSampleLoader : public VSampleLoader {
public:
  virtual void Load (sfxinfo_t &, VStream &) override;
};

IMPLEMENT_AUDIO_CODEC(VVorbisAudioCodec, "Vorbis(stb)", true); // with signature

VVorbisSampleLoader VorbisSampleLoader;


//==========================================================================
//
//  VVorbisAudioCodec::VVorbisAudioCodec
//
//==========================================================================
VVorbisAudioCodec::VVorbisAudioCodec (VStream *AStrm, bool AFreeStream)
  : Strm(AStrm)
  , FreeStream(AFreeStream)
  , decoder(nullptr)
  , eos(false)
  , inbuf(nullptr)
  , inbufSize(65536)
  , inbufUsed(0)
  , inbufFilled(0)
  , outbuf(nullptr)
  , outbufSize(0)
  , outbufUsed(0)
  , outbufFilled(0)
  , inited(false)
{
  BytesLeft = Strm->TotalSize();
  Strm->Seek(0);
  inbuf = (vuint8 *)Z_Malloc(inbufSize);
  tmpbuf.alloc_buffer_length_in_bytes = 0;
  tmpbuf.alloc_buffer = nullptr;
}


//==========================================================================
//
//  VVorbisAudioCodec::~VVorbisAudioCodec
//
//==========================================================================
VVorbisAudioCodec::~VVorbisAudioCodec () {
  Cleanup();
  if (inited && FreeStream && Strm) {
    Strm->Close();
    delete Strm;
  }
  Strm = nullptr;
  if (inbuf) Z_Free(inbuf);
  if (outbuf) Z_Free(outbuf);
}


//==========================================================================
//
//  VVorbisAudioCodec::Cleanup
//
//==========================================================================
void VVorbisAudioCodec::Cleanup () {
  if (decoder) { stb_vorbis_close(decoder); decoder = nullptr; }
  inbufUsed = inbufFilled = 0;
  outbufUsed = outbufFilled = 0;
  if (tmpbuf.alloc_buffer) Z_Free(tmpbuf.alloc_buffer);
  tmpbuf.alloc_buffer_length_in_bytes = 0;
  tmpbuf.alloc_buffer = nullptr;
}


//==========================================================================
//
//  VVorbisAudioCodec::stopFeeding
//
//==========================================================================
void VVorbisAudioCodec::stopFeeding (bool setEOS) {
  if (setEOS) eos = true;
  BytesLeft = 0;
  inbufUsed = inbufFilled = 0;
}


//==========================================================================
//
//  VVorbisAudioCodec::fillInBuffer
//
//  returns `false` on error or eof
//
//==========================================================================
bool VVorbisAudioCodec::fillInBuffer () {
  if (BytesLeft == 0 || eos) {
    if (inbufUsed >= inbufFilled) stopFeeding();
    return (inbufUsed < inbufFilled);
  }
  if (inbufUsed > 0) {
    if (inbufUsed < inbufFilled) {
      memmove(inbuf, inbuf+inbufUsed, inbufFilled-inbufUsed);
      inbufFilled -= inbufUsed;
    } else {
      inbufFilled = 0;
    }
    inbufUsed = 0;
  }
  vassert(inbufUsed == 0);
  int rd = min2(BytesLeft, inbufSize-inbufFilled);
  vassert(rd >= 0);
  if (rd == 0) {
    if (inbufFilled > 0) return true;
    if (BytesLeft > 0) GCon->Logf(NAME_Error, "stb_vorbis decoder glitched at '%s'", *Strm->GetName());
    stopFeeding(true);
    return false;
  }
  Strm->Serialise(inbuf+inbufFilled, rd);
  if (Strm->IsError()) { stopFeeding(true); return false; }
  inbufFilled += rd;
  BytesLeft -= rd;
  return true;
}


//==========================================================================
//
//  f2i
//
//==========================================================================
static inline void floatbuf2short (vint16 *dest, const float *src, unsigned length, bool mono2stereo=false) noexcept {
  if (!mono2stereo) {
    while (length--) {
      *dest = (vint16)(clampval(*src, -1.0f, 1.0f)*32767.0f);
      ++src;
      dest += 2;
    }
  } else {
    while (length--) {
      dest[0] = dest[1] = (vint16)(clampval(*src, -1.0f, 1.0f)*32767.0f);
      ++src;
      dest += 2;
    }
  }
}


//==========================================================================
//
//  VVorbisAudioCodec::decodeFrame
//
//  returns `false` on error or eof
//
//==========================================================================
bool VVorbisAudioCodec::decodeFrame () {
  if (outbufUsed < outbufFilled) return true;
  if (eos || !decoder || !Strm || Strm->IsError()) { stopFeeding(true); return false; }
  //float *fltpcm[STB_VORBIS_MAX_CHANNELS];
  float **fltpcm;
  //GCon->Logf(NAME_Debug, "...decodeFrame: '%s' (used=%d; filled=%d; size=%d); oused=%d; ofilled=%d; osize=%d", *Strm->GetName(), inbufUsed, inbufFilled, inbufSize, outbufUsed, outbufFilled, outbufSize);
  for (;;) {
    if (inbufUsed >= inbufFilled) {
      fillInBuffer();
      //GCon->Logf(NAME_Debug, "...read new buffer from '%s' (used=%d; filled=%d; size=%d)", *Strm->GetName(), inbufUsed, inbufFilled, inbufSize);
    }
    //GCon->Logf(NAME_Debug, "...decoding frame from '%s' (used=%d; filled=%d; size=%d)", *Strm->GetName(), inbufUsed, inbufFilled, inbufSize);
    int chans = 0;
    int samples = 0;
    int res = stb_vorbis_decode_frame_pushdata(decoder, (const unsigned char *)(inbuf+inbufUsed), inbufFilled-inbufUsed, &chans, &fltpcm, &samples);
    //GCon->Logf(NAME_Debug, "...decoded frame from '%s' (res=%d; chans=%d; samples=%d)", *Strm->GetName(), res, chans, samples);
    if (res < 0) { stopFeeding(true); return false; } // something's strange in the neighbourhood
    inbufUsed += res;
    // check samples first
    if (samples > 0) {
      if (chans < 1 || chans > 2) { stopFeeding(true); return false; } // why is that?
      // alloc samples for two channels
      if (outbufSize < samples*2) {
        outbufSize = samples*2;
        outbuf = (vint16 *)Z_Realloc(outbuf, outbufSize*2);
      }
      vassert(outbufSize >= samples*2);
      outbufUsed = 0;
      if (chans == 1) {
        // mono, expand to stereo
        floatbuf2short(outbuf, fltpcm[0], (unsigned)samples, true);
      } else {
        // stereo
        floatbuf2short(outbuf, fltpcm[0], (unsigned)samples);
        floatbuf2short(outbuf+1, fltpcm[1], (unsigned)samples);
      }
      outbufFilled = samples*2;
      //GCon->Logf(NAME_Debug, "...DECODED: '%s' (used=%d; filled=%d; size=%d); oused=%d; ofilled=%d; osize=%d", *Strm->GetName(), inbufUsed, inbufFilled, inbufSize, outbufUsed, outbufFilled, outbufSize);
      return true;
    }
    // ok, no samples, we may need more data
    if (res == 0) {
      //GCon->Logf(NAME_Debug, "...needs more data from '%s'", *Strm->GetName());
      // just need more data
      int oldInbufAvail = inbufFilled-inbufUsed;
      fillInBuffer();
      if (inbufFilled-inbufUsed == oldInbufAvail) {
        // cannot get more data, set "end of stream" flag
        //GCon->Logf(NAME_Debug, "cannot get more data from '%s'", *Strm->GetName());
        stopFeeding(true);
        return false;
      }
      //GCon->Logf(NAME_Debug, "...Decoding frame from '%s' (used=%d; filled=%d; size=%d)", *Strm->GetName(), inbufUsed, inbufFilled, inbufSize);
    }
  }
}


//==========================================================================
//
//  VVorbisAudioCodec::Init
//
//==========================================================================
bool VVorbisAudioCodec::Init () {
  Cleanup();
  vassert(!tmpbuf.alloc_buffer);

  tmpbuf.alloc_buffer_length_in_bytes = 1024*1024; // 1MB should be enough
  tmpbuf.alloc_buffer = (char *)Z_Malloc(tmpbuf.alloc_buffer_length_in_bytes);

  eos = false;
  inited = false;
  if (!fillInBuffer()) {
    //GCon->Logf(NAME_Debug, "oops (%s)", *Strm->GetName());
    return false;
  }

  //GCon->Logf(NAME_Debug, "buffer filled (%s) (%d/%d/%d)", *Strm->GetName(), inbufUsed, inbufFilled, inbufSize);
  int usedData = 0;
  int error = 0;
  decoder = stb_vorbis_open_pushdata((const unsigned char *)inbuf, inbufFilled-inbufUsed, &usedData, &error, &tmpbuf);
  if (!decoder || error != 0) {
    //GCon->Logf(NAME_Debug, "stb_vorbis error: %d", error);
    Cleanup();
    return false;
  }
  inbufUsed += usedData;

  stb_vorbis_info info = stb_vorbis_get_info(decoder);

  if (info.sample_rate < 64 || info.sample_rate > 96000*2 || info.channels < 1 || info.channels > 2) {
    //GCon->Logf(NAME_Debug, "stb_vorbis cannot get info (%d : %d)", (int)info.sample_rate, (int)info.channels);
    Cleanup();
    return false;
  }

  SampleRate = info.sample_rate;
  SampleBits = 16;
  NumChannels = 2; // always
  //GCon->Logf(NAME_Debug, "stb_vorbis created (%s); rate=%d; chans=%d", *Strm->GetName(), SampleRate, NumChannels);
  //GCon->Logf(NAME_Debug, "buffer filled (%s) (%d/%d/%d)", *Strm->GetName(), inbufUsed, inbufFilled, inbufSize);
  inited = true;
  return true;
}


//==========================================================================
//
//  VVorbisAudioCodec::Decode
//
//==========================================================================
int VVorbisAudioCodec::Decode (vint16 *Data, int NumFrames) {
  int CurFrame = 0;
  vint16 *dest = Data;
  while (CurFrame < NumFrames) {
    if (outbufUsed >= outbufFilled) {
      if (!decodeFrame()) break;
    }
    while (CurFrame < NumFrames && outbufUsed+2 <= outbufFilled) {
      *dest++ = outbuf[outbufUsed++];
      *dest++ = outbuf[outbufUsed++];
      ++CurFrame;
    }
    if (outbufUsed+2 > outbufFilled) outbufUsed = outbufFilled; // just in case
  }
  return CurFrame;
}


//==========================================================================
//
//  VVorbisAudioCodec::Finished
//
//==========================================================================
bool VVorbisAudioCodec::Finished () {
  return (eos && outbufUsed >= outbufFilled);
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
VAudioCodec *VVorbisAudioCodec::Create (VStream *InStrm, const vuint8 sign[], int signsize) {
  if (sign[0] != 'O' || sign[1] != 'g' || sign[2] != 'g' || sign[3] != 'S') return nullptr;
  VVorbisAudioCodec *Codec = new VVorbisAudioCodec(InStrm, true);
  if (!Codec->Init()) {
    Codec->Cleanup();
    delete Codec;
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
  //GCon->Logf(NAME_Debug, "trying sfx '%s' (VVorbisSampleLoader)", *Stream.GetName());
  if (!Codec->Init()) {
    Codec->Cleanup();
  } else {
    LoadFromAudioCodec(Sfx, Codec);
  }
  delete Codec;
}
