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
  vint32 ChanIndex;
  bool bOpen; // open channel message
  bool bClose; // close channel message

public:
  VV_DISABLE_COPY(VMessageIn)

  inline VMessageIn (VBitStreamReader &srcPacket) : VBitStreamReader(), ChanType(0), ChanIndex(-1), bOpen(false), bClose(false) { LoadFrom(srcPacket); }

  bool LoadFrom (VBitStreamReader &srcPacket);
};


// ////////////////////////////////////////////////////////////////////////// //
class VMessageOut : public VBitStreamWriter {
public:
  // flags for ctor
  enum {
    Unreliable = 1u<<0,
    Open       = 1u<<1,
    Close      = 1u<<2,
    // this flag means nothing, and used only to force sending empty packets
    Keepalive  = 1u<<6,
    //NoHeader   = 1u<<7,
  };

protected:
  int hdrSizeBits;
  unsigned msgflags;

protected:
  void writeHeader (vuint8 AChanType, int AChanIndex, unsigned flags);

  void fixSize ();

public:
  // this creates message with the header for the given existing channel
  VMessageOut (VChannel *AChannel, unsigned flags=0);

  // this creates message with the header for arbitrary channel
  VMessageOut (vuint8 AChanType, int AChanIndex, unsigned flags);

  // this copies message data, including header
  inline VMessageOut (const VMessageOut &src)
    : VBitStreamWriter(0, false) // it will be overwritten anyway
  {
    // clone bitstream writer
    cloneFrom(&src);
  }

  // empty message contains only header
  inline bool IsEmpty () const noexcept { return (GetNumBits() == hdrSizeBits); }

  inline bool IsOpen () const noexcept { return !!(msgflags&Open); }
  inline bool IsClose () const noexcept { return !!(msgflags&Close); }
  inline bool IsReliable () const noexcept { return !(msgflags&Unreliable); }
  inline bool IsUnreliable () const noexcept { return !!(msgflags&Unreliable); }
  inline bool IsKeepalive () const noexcept { return !!(msgflags&Keepalive); }

  inline void MarkKeepalive () noexcept { msgflags |= Keepalive; }

  // we cannot mark the packed as "open" post-factum, but we can mark is as "closing", to save bandwidth
  void MarkClose () noexcept;

  // empty packets with special flags should still be sent
  inline bool NeedToSend () const noexcept { return (GetNumBits() > hdrSizeBits || (msgflags&(Open|Close|Keepalive))); }

  void Reset (VChannel *AChannel, unsigned flags=0);

  // call this before copying packet data
  void Finalise ();

  static inline int CalcFullMsgBitSize (int bitlen) noexcept {
    // message size, one stop bit, rounded to the byte boundary
    vassert(bitlen >= 0);
    return ((bitlen+1+7)>>3)<<3;
  }

  inline int CalcRealMsgBitSize (int addlen=0) const noexcept { return CalcFullMsgBitSize(GetNumBits()+addlen); }
  inline int CalcRealMsgBitSize (const VBitStreamWriter &strm) const noexcept { return CalcFullMsgBitSize(GetNumBits()+strm.GetNumBits()); }

  // will this overflow a datagram?
  inline bool WillOverflow (int moredata) const noexcept {
    vassert(moredata >= 0);
    return (CalcRealMsgBitSize(moredata) > MAX_MSG_SIZE_BITS);
  }

  // will this overflow a datagram?
  inline bool WillOverflow (const VBitStreamWriter &strm) const noexcept {
    return (CalcRealMsgBitSize(strm) > MAX_MSG_SIZE_BITS);
  }
};
