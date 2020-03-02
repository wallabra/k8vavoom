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


struct ClientServerInfo {
  VStr mapname;
  VStr maphash;
  vuint32 modhash;
  VStr sinfo;
  int maxclients;
  int deathmatch;
};


// ////////////////////////////////////////////////////////////////////////// //
class VMessageIn : public VBitStreamReader {
public:
  VMessageIn *Next;
  vuint8 ChanType;
  vint32 ChanIndex;
  bool bReliable; // reliable message
  bool bOpen; // open channel message
  bool bClose; // close channel message
  vuint32 Sequence; // reliable message sequence ID

public:
  inline VMessageIn (vuint8 *Src=nullptr, vint32 Length=0)
    : VBitStreamReader(Src, Length)
    , Next(nullptr)
    , ChanType(0)
    , ChanIndex(0)
    , bReliable(false)
    , bOpen(false)
    , bClose(false)
    , Sequence(0)
  {}
  inline VMessageIn (const VMessageIn &src) noexcept
    : VBitStreamReader(nullptr, 0)
    , Next(nullptr)
    , ChanType(src.ChanType)
    , ChanIndex(src.ChanIndex)
    , bReliable(src.bReliable)
    , bOpen(src.bOpen)
    , bClose(src.bClose)
    , Sequence(src.Sequence)
  {
    // clone bitstream reader
    cloneFrom(&src);
  }
};


// ////////////////////////////////////////////////////////////////////////// //
class VMessageOut : public VBitStreamWriter {
public:
  VChannel *mChannel;
  VMessageOut *Next;
  vuint8 ChanType;
  vint32 ChanIndex;
  bool bReliable; // needs ACK or not
  bool bOpen;
  bool bClose;
  bool bReceivedAck;
  vuint32 Sequence; // reliable message sequence ID
  double Time; // time this message has been sent
  vuint32 PacketId; // packet in which this message was sent
  int markPos;
  // user data; can be used in ack processor
  int udata;
  // packet ids we sent this message in; used to ack with old messages (FUCKIN' HACK, DON'T DO THAT, KETMAR!)
  TMapNC<vuint32, bool> AckPIds;

public:
  // cannot be inlined, sorry
  VMessageOut (VChannel *AChannel, bool AReliable, bool aAllowExpand=true);

  inline VMessageOut (const VMessageOut &src)
    : VBitStreamWriter(0, false) // it will be overwritten anyway
    , mChannel(src.mChannel)
    , Next(nullptr)
    , ChanType(src.ChanType)
    , ChanIndex(src.ChanIndex)
    , bReliable(src.bReliable)
    , bOpen(src.bOpen)
    , bClose(src.bClose)
    , bReceivedAck(src.bReceivedAck)
    , Sequence(src.Sequence)
    , Time(src.Time)
    , PacketId(src.PacketId)
    , markPos(src.markPos)
    , udata(src.udata)
  {
    // clone bitstream writer
    cloneFrom(&src);
    // there is no need to clone ack pids, tho
  }

  void Setup (VChannel *AChannel, bool AReliable, bool aAllowExpand=true);

  inline void SetMark () { markPos = GetNumBits(); }

  inline bool IsGoodAckId (vuint32 pid) const noexcept { return AckPIds.has(pid); }
  inline void AppendAckId (vuint32 pid) noexcept { AckPIds.put(pid, true); }

  bool NeedSplit () const;
  void SendSplitMessage ();
};
