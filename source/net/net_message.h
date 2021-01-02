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
//**
//**  MESSAGE IO FUNCTIONS
//**
//**    Handles byte ordering and avoids alignment errors
//**
//**************************************************************************

// ////////////////////////////////////////////////////////////////////////// //
class VChannel;


// ////////////////////////////////////////////////////////////////////////// //
class VMessageIn : public VBitStreamReader {
public:
  vuint8 ChanType;
  vuint32 ChanIndex;
  vuint32 ChanSequence;
  // note that in unreliable message, `bOpen` is always equal to `bClose`
  bool bOpen; // open channel message
  bool bClose; // close channel message
  bool bReliable;

  VMessageIn *Next;

public:
  VMessageOut &operator = (const VMessageOut &) = delete;

  inline VMessageIn (const VMessageIn &src)
    : VBitStreamReader() // will be replaced anyway
    , ChanType(src.ChanType)
    , ChanIndex(src.ChanIndex)
    , ChanSequence(src.ChanSequence)
    , bOpen(src.bOpen)
    , bClose(src.bClose)
    , bReliable(src.bReliable)
    , Next(nullptr)
  {
    cloneFrom(&src);
  }

  // this parses message header, and consumes message data
  // first zero bit is already read, though
  inline VMessageIn (VBitStreamReader &srcPacket)
    : VBitStreamReader()
    , ChanType(0)
    , ChanIndex(-1)
    , ChanSequence(0)
    , bOpen(false)
    , bClose(false)
    , bReliable(false)
    , Next(nullptr)
  {
    LoadFrom(srcPacket);
  }

  // this clears the message, parses message header, and consumes message data
  // first zero bit is already read, though
  bool LoadFrom (VBitStreamReader &srcPacket);

  VStr toStringDbg () const noexcept;
};


// ////////////////////////////////////////////////////////////////////////// //
class VMessageOut : public VBitStreamWriter {
public:
  vuint8 ChanType;
  vuint32 ChanIndex;
  vuint32 ChanSequence;
  // the packet (datagram) sequence id in which this message was sent last time
  // it is used to ack all messages in the given packet
  vuint32 PacketId;
  bool bOpen;
  bool bClose;
  bool bReliable;

  VMessageOut *Next;
  // set by the connection object
  double Time; // time when this message was sent (updated with each resending)
  bool bReceivedAck; // packet parser will set this flag, and will call the channel to process acked messages
  int OutEstimated; // channel stores its estimation here, so it can keep its internal accumulator in sync

private:
  // called from ctors
  void SetupWith (vuint8 AChanType, int AChanIndex, bool areliable) noexcept;

public:
  VMessageOut &operator = (const VMessageOut &) = delete;

  // default messages are reliable
  VMessageOut (vuint8 AChanType, int AChanIndex, bool areliable=true);
  VMessageOut (VChannel *AChannel, bool areliable=true);

  // this copies message data, including header
  inline VMessageOut (const VMessageOut &src)
    : VBitStreamWriter(0, false) // it will be overwritten anyway
    , ChanType(src.ChanType)
    , ChanIndex(src.ChanIndex)
    , ChanSequence(src.ChanSequence)
    , PacketId(src.PacketId)
    , bOpen(src.bOpen)
    , bClose(src.bClose)
    , bReliable(src.bReliable)
    , Next(nullptr)
    , Time(src.Time)
    , bReceivedAck(src.bReceivedAck) //???
    , OutEstimated(src.OutEstimated)
  {
    // clone bitstream writer
    cloneFrom(&src);
  }

  void Reset (VChannel *AChannel, bool areliable);

  // this can be called several times
  void WriteHeader (VBitStreamWriter &strm) const;

  // estimate current packet size
  // channel sequence, and packedid doesn't matter
  int EstimateSizeInBits (int addbits=0) const noexcept;

  // returns `true` if appending `strm` will overflow the message
  inline bool WillOverflow (int addbits) const noexcept { return (EstimateSizeInBits(addbits) > MAX_MSG_SIZE_BITS); }
  inline bool WillOverflow (const VBitStreamWriter &strm) const noexcept { return WillOverflow(strm.GetNumBits()); }

  VStr toStringDbg () const noexcept;
};
