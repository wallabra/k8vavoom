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
#include <mad.h>

#include "gamedefs.h"
#include "snd_local.h"


class VMp3AudioCodec : public VAudioCodec {
public:
  enum { INPUT_BUFFER_SIZE = 5*8192 };

public:
  VStream *Strm;
  bool FreeStream;
  int BytesLeft;
  bool Initialised;
  // set in `Init()`
  int StreamStart;
  int StreamSize;

  mad_stream Stream;
  mad_frame Frame;
  mad_synth Synth;
  vuint8 InputBuffer[INPUT_BUFFER_SIZE+MAD_BUFFER_GUARD];
  int FramePos;
  bool HaveFrame;

public:
  VMp3AudioCodec (VStream *AStrm, bool AFreeStream);
  virtual ~VMp3AudioCodec () override;

  bool Init ();
  int ReadData ();

  virtual int Decode (vint16 *Data, int NumFrames) override;
  virtual bool Finished () override;
  virtual void Restart () override;

  static VAudioCodec *Create (VStream *InStrm, const vuint8 sign[], int signsize);
};


class VMp3SampleLoader : public VSampleLoader {
public:
  virtual void Load (sfxinfo_t &Sfx, VStream &Stream) override;
};


IMPLEMENT_AUDIO_CODEC(VMp3AudioCodec, "MP3", false); // no signature

VMp3SampleLoader Mp3SampleLoader;


//==========================================================================
//
//  VMp3AudioCodec::VMp3AudioCodec
//
//==========================================================================
VMp3AudioCodec::VMp3AudioCodec (VStream *AStrm, bool AFreeStream)
  : Strm(AStrm)
  , FreeStream(AFreeStream)
  , Initialised(false)
  , StreamStart(0)
  , StreamSize(0)
{
  // seek file to the begining
  StreamSize = BytesLeft = Strm->TotalSize();
  Strm->Seek(0);

  // initialise structures used by libmad
  mad_stream_init(&Stream);
  mad_frame_init(&Frame);
  mad_synth_init(&Synth);
}


//==========================================================================
//
//  VMp3AudioCodec::~VMp3AudioCodec
//
//==========================================================================
VMp3AudioCodec::~VMp3AudioCodec () {
  if (Initialised) {
    // close file only if decoder has been initialised succesfully.
    if (FreeStream) {
      Strm->Close();
      delete Strm;
    }
    Strm = nullptr;
  }
  // clear structs used by libmad
  mad_synth_finish(&Synth);
  mad_frame_finish(&Frame);
  mad_stream_finish(&Stream);
}


//==========================================================================
//
//  VMp3AudioCodec::Init
//
//==========================================================================
bool VMp3AudioCodec::Init () {
  // check for ID3v2 header
  vuint8 Id3Hdr[10];
  int SavedPos = Strm->Tell();
  Strm->Serialise(Id3Hdr, 10);
  if (Id3Hdr[0] == 'I' && Id3Hdr[1] == 'D' && Id3Hdr[2] == '3') {
    // it's a ID3v3 header, skip it
    int HdrSize = Id3Hdr[9]+(Id3Hdr[8]<<7)+(Id3Hdr[7]<<14)+(Id3Hdr[6]<<21);
    if (HdrSize+10 > BytesLeft) return false;
    Strm->Seek(Strm->Tell()+HdrSize);
    BytesLeft -= 10+HdrSize;
  } else {
    // not a ID3v3 header, seek back to saved position
    Strm->Seek(SavedPos);
  }

  if (Strm->IsError()) return false;

  StreamStart = Strm->Tell();
  StreamSize = BytesLeft;
  // read some data
  ReadData();
  if (Strm->IsError()) return false;

  // decode first frame. If this fails we assume it's not a MP3 file
  if (mad_frame_decode(&Frame, &Stream)) {
    // not a valid stream
    return false;
  }

  // we are ready to read data
  mad_synth_frame(&Synth, &Frame);
  FramePos = 0;
  HaveFrame = true;

  // everything's ok
  SampleRate = Frame.header.samplerate;
  Initialised = true;
  return true;
}


//==========================================================================
//
//  VMp3AudioCodec::Decode
//
//==========================================================================
int VMp3AudioCodec::Decode (vint16 *Data, int NumFrames) {
  int CurFrame = 0;
  for (;;) {
    if (HaveFrame) {
      // convert stream from fixed point to vint16
      for (; FramePos < Synth.pcm.length; ++FramePos) {
        // left channel
        vint16 Sample;
        mad_fixed_t Fixed = Synth.pcm.samples[0][FramePos];
             if (Fixed >= MAD_F_ONE) Sample = 0x7fff;
        else if (Fixed <= -MAD_F_ONE) Sample = -0x7fff;
        else Sample = Fixed>>(MAD_F_FRACBITS-15);
        Data[CurFrame*2] = Sample;

        // right channel
        // if the decoded stream is monophonic then the right output channel is the same as the left one
        if (MAD_NCHANNELS(&Frame.header) == 2) {
          Fixed = Synth.pcm.samples[1][FramePos];
               if (Fixed >= MAD_F_ONE) Sample = 0x7fff;
          else if (Fixed <= -MAD_F_ONE) Sample = -0x7fff;
          else Sample = Fixed>>(MAD_F_FRACBITS-15);
        }
        Data[CurFrame*2+1] = Sample;

        ++CurFrame;
        // check if we already have decoded enough
        if (CurFrame >= NumFrames) return CurFrame;
      }
      // we are done with the frame
      HaveFrame = false;
    }

    // fill in input buffer if it becomes empty
    if (Stream.buffer == nullptr || Stream.error == MAD_ERROR_BUFLEN) {
      if (!ReadData()) break;
    }

    // decode the next frame
    if (mad_frame_decode(&Frame, &Stream)) {
      if (MAD_RECOVERABLE(Stream.error) || Stream.error == MAD_ERROR_BUFLEN) continue;
      break;
    }

    // once decoded the frame is synthesized to PCM samples
    mad_synth_frame(&Synth, &Frame);
    FramePos = 0;
    HaveFrame = true;
  }
  return CurFrame;
}


//==========================================================================
//
//  VMp3AudioCodec::ReadData
//
//==========================================================================
int VMp3AudioCodec::ReadData () {
  int ReadSize;
  int Remaining;
  vuint8 *ReadStart;

  // if there are some bytes left, move them to the beginning of the buffer
  if (Stream.next_frame != nullptr) {
    Remaining = Stream.bufend-Stream.next_frame;
    memmove(InputBuffer, Stream.next_frame, Remaining);
    ReadStart = InputBuffer+Remaining;
    ReadSize = INPUT_BUFFER_SIZE-Remaining;
  } else {
    ReadSize = INPUT_BUFFER_SIZE;
    ReadStart = InputBuffer;
    Remaining = 0;
  }

  // fill-in the buffer
  if (ReadSize > BytesLeft) ReadSize = BytesLeft;
  if (!ReadSize) return 0;
  Strm->Serialise(ReadStart, ReadSize);
  BytesLeft -= ReadSize;

  // when decoding the last frame of a file, it must be followed by
  // MAD_BUFFER_GUARD zero bytes if one wants to decode that last frame
  if (!BytesLeft) {
    memset(ReadStart+ReadSize+Remaining, 0, MAD_BUFFER_GUARD);
    ReadSize += MAD_BUFFER_GUARD;
  }

  // pipe the new buffer content to libmad's stream decoder facility
  mad_stream_buffer(&Stream, InputBuffer, ReadSize+Remaining);
  Stream.error = MAD_ERROR_NONE;
  return ReadSize+Remaining;
}


//==========================================================================
//
//  VMp3AudioCodec::Finished
//
//==========================================================================
bool VMp3AudioCodec::Finished () {
  // we are done if there's no more data and last frame has been decoded
  return (!BytesLeft && !HaveFrame);
}


//==========================================================================
//
//  VMp3AudioCodec::Restart
//
//==========================================================================
void VMp3AudioCodec::Restart () {
  // seek to the beginning of the data stream
  Strm->Seek(StreamStart);
  BytesLeft = StreamSize;
}


//==========================================================================
//
//  VMp3AudioCodec::Create
//
//==========================================================================
VAudioCodec *VMp3AudioCodec::Create (VStream *InStrm, const vuint8 sign[], int signsize) {
  VMp3AudioCodec *Codec = new VMp3AudioCodec(InStrm, true);
  if (!Codec->Init()) {
    delete Codec;
    return nullptr;
  }
  return Codec;
}


//==========================================================================
//
//  VMp3SampleLoader::Load
//
//==========================================================================
void VMp3SampleLoader::Load (sfxinfo_t &Sfx, VStream &Stream) {
  VMp3AudioCodec *Codec = new VMp3AudioCodec(&Stream, false);
  if (Codec->Init()) {
    LoadFromAudioCodec(Sfx, Codec);
  }
  delete Codec;
}
