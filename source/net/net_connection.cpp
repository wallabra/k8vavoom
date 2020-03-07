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
#include "network.h"
#include "net_message.h"


static VCvarF net_dbg_send_loss("net_dbg_send_loss", "0", "Emulated sent packet loss percentage (randomly skip sending some packets).", CVAR_PreInit);
static VCvarF net_dbg_recv_loss("net_dbg_recv_loss", "0", "Emulated received packet loss percentage (randomly skip sending some packets).", CVAR_PreInit);
static VCvarB net_dbg_conn_show_outdated("net_dbg_conn_show_outdated", false, "Show outdated channel messages?");
static VCvarB net_dbg_conn_show_dgrams("net_dbg_conn_show_dgrams", false, "Show datagram activity?");
static VCvarB net_dbg_conn_show_unreliable("net_dbg_conn_show_unreliable", false, "Show datagram unreliable payload info?");
static VCvarB net_dbg_report_missing_dgrams("net_dbg_report_missing_dgrams", false, "Report missing datagrams (this is mostly useless console spam)?");
static VCvarB net_dbg_report_stats("net_dbg_report_stats", false, "Report some stats to the console?");

VCvarB net_debug_dump_recv_packets("net_debug_dump_recv_packets", false, "Dump received packets?");

//FIXME: autoadjust this according to average ping
VCvarI VNetConnection::net_speed_limit("net_speed_limit", "560000", "Network speed limit, bauds (rough).", 0/*CVAR_Archive*/);

static VCvarF sv_fps("sv_fps", "35", "Server update frame rate (the server will use this to send updates to clients).", 0/*CVAR_Archive*/);


//==========================================================================
//
//  VNetConnection::VNetConnection
//
//==========================================================================
VNetConnection::VNetConnection (VSocketPublic *ANetCon, VNetContext *AContext, VBasePlayer *AOwner)
  : NetCon(ANetCon)
  , Driver(GNet)
  , Context(AContext)
  , Owner(AOwner)
  , State(NETCON_Open) //FIXME: set to invalid?
  , NeedsUpdate(false)
  , AutoAck(false)
  , LastLevelUpdateTime(0)
  , UpdateFrameCounter(0)
  , ObjMapSent(false)
  , LevelInfoSent(false)
  , Out(MAX_DGRAM_SIZE*8+128, false) // cannot grow
  , UpdatePvs(nullptr)
  , UpdatePvsSize(0)
  , LeafPvs(nullptr)
{
  InRate = OutRate = 0;
  InPackets = OutPackets = 0;
  InMessages = OutMessages = 0;
  InLoss = OutLoss = 0;
  InOrder = OutOrder = 0;
  PrevLag = AvgLag = 0;

  LagAcc = PrevLagAcc = 0;
  InLossAcc = OutLossAcc = 0;
  InPktAcc = OutPktAcc = 0;
  InMsgAcc = OutMsgAcc = 0;
  InByteAcc = OutByteAcc = 0;
  InOrdAcc = 0;
  LagCount = 0;
  LastFrameStartTime = FrameDeltaTime = 0;
  CumulativeTime = AverageFrameTime = 0;
  StatsFrameCounter = 0;

  memset((void *)OutLagTime, 0, sizeof(OutLagTime));
  memset((void *)OutLagPacketId, 0, sizeof(OutLagPacketId));

  InPacketId = 0;
  OutPacketId = 1;
  OutAckPacketId = 0;
  LastInPacketIdAck = 0xffffffffu;

  //FIXME: driver time?
  LastReceiveTime = 0;
  LastSendTime = 0;
  LastTickTime = 0;
  LastStatsUpdateTime = 0;

  SaturaDepth = 0;
  LagAcc = 9999;
  PrevLagAcc = 9999;
  PrevLag = 9999;
  AvgLag = 9999;

  ForceFlush = false;

  for (unsigned f = 0; f < (unsigned)MAX_CHANNELS; ++f) {
    Channels[f] = nullptr;
    OutReliable[f] = 0;
    InReliable[f] = 0;
  }

  Out.Reinit(MAX_DGRAM_SIZE*8+128, false); // don't grow

  ObjMap = new VNetObjectsMap(this);

  // open local channels
  CreateChannel(CHANNEL_Control, CHANIDX_General, true);
  CreateChannel(CHANNEL_Player, CHANIDX_Player, true);
  CreateChannel(CHANNEL_Level, CHANIDX_Level, true);
}


//==========================================================================
//
//  VNetConnection::~VNetConnection
//
//==========================================================================
VNetConnection::~VNetConnection () {
  GCon->Logf(NAME_DevNet, "Deleting connection to %s", *GetAddress());
  // remove all open channels
  for (auto &&chan : OpenChannels) {
    chan->Connection = nullptr;
    chan->Closing = true; // just in case
    delete chan;
  }
  OpenChannels.clear();
  ThinkerChannels.clear();
  //GCon->Logf(NAME_DevNet, "...all channels deleted");
  if (NetCon) {
    //GCon->Logf(NAME_DevNet, "...deleting socket (%p)", NetCon);
    NetCon->DumpStats();
    delete NetCon;
  }
  NetCon = nullptr;
  if (IsClient()) {
    vensure(Context->ServerConnection == this);
    Context->ServerConnection = nullptr;
  } else {
    Context->ClientConnections.Remove(this);
  }
  if (UpdatePvs) {
    delete[] UpdatePvs;
    UpdatePvs = nullptr;
  }
  if (ObjMap) {
    delete ObjMap;
    ObjMap = nullptr;
  }
}


//==========================================================================
//
//  VNetConnection::IsClient
//
//==========================================================================
bool VNetConnection::IsClient () noexcept {
  return (Context ? Context->IsClient() : false);
}


//==========================================================================
//
//  VNetConnection::IsServer
//
//==========================================================================
bool VNetConnection::IsServer () noexcept {
  return (Context ? Context->IsServer() : false);
}


//==========================================================================
//
//  VNetConnection::IsKeepAliveExceeded
//
//==========================================================================
bool VNetConnection::IsKeepAliveExceeded () {
  const double kt = clampval(net_keepalive.asFloat(), 0.05f, 1.0f);
  const double ctt = Driver->GetNetTime();
  return (ctt-LastSendTime > kt);
}


//==========================================================================
//
//  VNetConnection::IsTimeoutExceeded
//
//==========================================================================
bool VNetConnection::IsTimeoutExceeded () {
  if (IsClosed()) return false; // no wai
  // for bots and demo playback there's no other end that will send us the ACK
  // so there's no need to check for timeouts
  // `AutoAck == true` means "demo recording"
  if (AutoAck || IsLocalConnection()) return false;
  if (LastReceiveTime < 1) LastReceiveTime = Driver->GetNetTime();
  if (NetCon->LastMessageTime < 1) NetCon->LastMessageTime = Driver->GetNetTime();
  double tout = clampval(net_timeout.asFloat(), 0.4f, 20.0f);
  if (Driver->GetNetTime()-LastReceiveTime <= tout) return false;
  // timeout!
  return true;
}


//==========================================================================
//
//  VNetConnection::Saturate
//
//==========================================================================
void VNetConnection::Saturate () noexcept {
  SaturaDepth = -Out.GetNumBytes();
}


//==========================================================================
//
//  VNetConnection::CanSendData
//
//==========================================================================
bool VNetConnection::CanSendData () const noexcept {
  return (SaturaDepth+Out.GetNumBytes() <= 0);
}


//==========================================================================
//
//  VNetConnection::IsLocalConnection
//
//==========================================================================
bool VNetConnection::IsLocalConnection () const noexcept {
  // for demo playback NetCon can be `nullptr`
  return (NetCon ? NetCon->IsLocalConnection() : true);
}


//==========================================================================
//
//  VNetConnection::ShowTimeoutStats
//
//==========================================================================
void VNetConnection::ShowTimeoutStats () {
  if (IsClosed()) return;
  GCon->Logf(NAME_DevNet, "ERROR: Channel timed out; time delta=%g; sent %d packets (%d datagrams), received %d packets (%d datagrams)",
    (Driver->GetNetTime()-LastReceiveTime)*1000.0f,
    Driver->packetsSent, Driver->UnreliableMessagesSent,
    Driver->packetsReceived, Driver->UnreliableMessagesReceived);
}


//==========================================================================
//
//  VNetConnection::CreateChannel
//
//==========================================================================
VChannel *VNetConnection::CreateChannel (vuint8 Type, vint32 AIndex, vuint8 OpenedLocally) {
  // if channel index is -1, find a free channel slot
  vint32 Index = AIndex;
  if (Index < 0) {
    if (Type == CHANNEL_ObjectMap) {
      vassert(Channels[CHANIDX_ObjectMap] == nullptr);
      Index = CHANIDX_ObjectMap;
    } else {
      vassert(Type == CHANNEL_Thinker);
      for (int f = CHANIDX_ThinkersStart; f < MAX_CHANNELS; ++f) {
        if (!Channels[f]) {
          //if (Index < 0) Index = f;
          //if (GenRandomU31()) {}
          Index = f;
          break;
        }
      }
      if (Index < 0) return nullptr;
      vassert(Index >= CHANIDX_ThinkersStart && Index < MAX_CHANNELS);
    }
  } else if (Type == CHANIDX_ThinkersStart) {
    // this can happen in client (server requested channel id)
    if (Index < CHANIDX_ThinkersStart || Index >= MAX_CHANNELS) Sys_Error("trying to allocate thinker channel with invalid index %d", Index);
    if (Channels[Index]) Sys_Error("trying to allocate already allocated fixed thinker channel with index %d", Index);
  }
  vassert(Index >= 0 && Index < MAX_CHANNELS);

  switch (Type) {
    case CHANNEL_Control: vassert(Index == CHANIDX_General); return new VControlChannel(this, Index, OpenedLocally);
    case CHANNEL_Level: vassert(Index == CHANIDX_Level); return new VLevelChannel(this, Index, OpenedLocally);
    case CHANNEL_Player: vassert(Index == CHANIDX_Player); return new VPlayerChannel(this, Index, OpenedLocally);
    case CHANNEL_Thinker: vassert(Index >= CHANIDX_ThinkersStart); return new VThinkerChannel(this, Index, OpenedLocally);
    case CHANNEL_ObjectMap: vassert(Index == CHANIDX_ObjectMap); return new VObjectMapChannel(this, Index, OpenedLocally);
    default: GCon->Logf(NAME_DevNet, "Unknown channel type %d for channel with index %d", Type, Index); return nullptr;
  }
}


//==========================================================================
//
//  VNetConnection::GetRawPacket
//
//  used in demos
//
//==========================================================================
int VNetConnection::GetRawPacket (void *dest, size_t destSize) {
  vensure(NetCon);
  return NetCon->GetMessage(dest, destSize);
}


//==========================================================================
//
//  VNetConnection::GetMessage
//
//  read and process incoming network datagram
//  returns `false` if no message was processed
//
//==========================================================================
bool VNetConnection::GetMessage () {
  // check for message arrival
  if (IsClosed()) return false;
  vassert(NetCon);

  vuint8 msgdata[MAX_DGRAM_SIZE+4];
  const int msgsize = NetCon->GetMessage(msgdata, sizeof(msgdata));
  if (msgsize == 0) return false;
  if (msgsize < 0) { State = NETCON_Closed; return false; }

  InByteAcc += msgsize;
  ++InPktAcc;

  // received something
  ++Driver->UnreliableMessagesReceived;

  // copy received data to packet stream
  VBitStreamReader Packet;
  // nope, this will be set by the server code
  Packet.SetupFrom(msgdata, msgsize*8, true); // fix the length with the trailing bit
  if (Packet.IsError()) {
    GCon->Logf(NAME_DevNet, "%s: datagram packet is missing trailing bit", *GetAddress());
    State = NETCON_Closed; // close connection due to invalid data
    return false;
  }
  if (net_dbg_conn_show_dgrams) GCon->Logf(NAME_DevNet, "%s: got datagram with a packet (%d bits of data)", *GetAddress(), Packet.GetNumBits());

  ReceivedPacket(Packet);

  return true;
}


//==========================================================================
//
//  VNetConnection::GetMessages
//
//==========================================================================
void VNetConnection::GetMessages () {
  if (IsClosed()) return;

  // process up to 128 packets (why not?)
  for (int f = 0; f < 128 && IsOpen(); ++f) {
    if (!GetMessage()) break;
  }
}


//==========================================================================
//
//  VNetConnection::PacketLost
//
//==========================================================================
void VNetConnection::PacketLost (vuint32 PacketId) {
  for (int f = OpenChannels.length()-1; f >= 0; --f) {
    VChannel *chan = OpenChannels[f];
    if (!chan) continue; // just in case
    chan->PacketLost(PacketId);
  }
}


//==========================================================================
//
//  VNetConnection::ReceivedPacket
//
//==========================================================================
void VNetConnection::ReceivedPacket (VBitStreamReader &Packet) {
  // simulate receiving loss
  const float lossPrc = net_dbg_recv_loss.asFloat();
  if (lossPrc > 0.0f && RandomFull()*100.0f < lossPrc) {
    //GCon->Logf(NAME_Debug, "%s: simulated packet loss!", *GetAddress());
    return;
  }

  // check packet ordering
  vuint32 PacketId = 0;
  Packet << PacketId;
  if (Packet.IsError()) {
    GCon->Logf(NAME_DevNet, "%s: got invalid packet, connection dropped", *GetAddress());
    State = NETCON_Closed;
    return;
  }

  // reset timeout timer
  NetCon->LastMessageTime = LastReceiveTime = Driver->GetNetTime();
  ++Driver->packetsReceived;

  if (net_debug_dump_recv_packets) GCon->Logf(NAME_DevNet, "***!!!*** Network Packet (bitcount=%d; pid=%u; inpid=%u)", Packet.GetNum(), PacketId, InPacketId);

  if (PacketId > InPacketId) {
    InLossAcc += PacketId-InPacketId-1;
    InPacketId = PacketId;
  } else {
    ++InOrdAcc;
  }

  // ack it
  LastInPacketIdAck = PacketId;
  SendPacketAck(PacketId);
  NeedsUpdate = true; // we got *any* activity, update the world!

  while (!Packet.AtEnd() && IsOpen()) {
    if (net_debug_dump_recv_packets) GCon->Logf(NAME_DevNet, "  parsing packet: %d bits eaten of %d", Packet.GetPos(), Packet.GetNumBits());
    //INT StartPos = Reader.GetPosBits();
    // ack?
    if (Packet.ReadBit()) {
      // yep, process it
      vuint32 AckPacketId = 0;
      Packet << STRM_INDEX_U(AckPacketId);

      if (Packet.IsError()) {
        // this is not fatal
        GCon->Logf(NAME_DevNet, "%s: missing ack id", *GetAddress());
        ++Driver->shortPacketCount;
        return;
      }

      if (net_debug_dump_recv_packets) GCon->Logf(NAME_DevNet, "  ack: ackpid=%u; outackpid=%u", AckPacketId, OutAckPacketId);

      // resend any old reliable packets that the receiver hasn't acknowledged
      if (AckPacketId > OutAckPacketId) {
        for (vuint32 LostPacketId = OutAckPacketId+1; LostPacketId < AckPacketId; ++LostPacketId, ++OutLossAcc) {
          PacketLost(LostPacketId);
        }
        OutAckPacketId = AckPacketId;
      } else if (AckPacketId < OutAckPacketId) {
        // this is harmless
      }

      // update lag statistics
      const unsigned arrIndex = AckPacketId&(ARRAY_COUNT(OutLagPacketId)-1);
      if (OutLagPacketId[arrIndex] == AckPacketId) {
        const double NewLag = Driver->GetNetTime()-OutLagTime[arrIndex]-(FrameDeltaTime*0.5);
        LagAcc += NewLag;
        ++LagCount;
      }

      // forward the ack to the respective channel
      for (int f = OpenChannels.length()-1; f >= 0; --f) {
        VChannel *chan = OpenChannels[f];
        for (VMessageOut *outmsg = chan->OutList; outmsg; outmsg = outmsg->Next) {
          if (outmsg->PacketId == AckPacketId) {
            outmsg->bReceivedAck = true;
            if (outmsg->bOpen) chan->OpenAcked = true;
          }
        }
        chan->ReceivedAcks(); // WARNING: channel may delete itself there!
      }
    } else {
      // normal message

      // read message header
      VMessageIn Msg(Packet);
      if (Packet.IsError() || Msg.IsError()) {
        // this is not fatal
        GCon->Logf(NAME_DevNet, "%s: packet is missing message header", *GetAddress());
        ++Driver->shortPacketCount;
        return;
      }
      if (net_debug_dump_recv_packets) GCon->Logf(NAME_DevNet, "  parsed packet message: %d bits eaten of %d", Packet.GetPos(), Packet.GetNumBits());

      if (net_debug_dump_recv_packets) {
        GCon->Logf(NAME_DevNet, "  message (channel %u; chantype=%u; chanseq=%u; pktid=%u; open=%d; close=%d; reliable=%d) (len=%d; pos=%d; num=%d; left=%d)",
          Msg.ChanIndex, Msg.ChanType, Msg.ChanSequence, PacketId, (int)Msg.bOpen, (int)Msg.bClose, (int)Msg.bReliable,
          Msg.GetNumBits(), Packet.GetPos(), Packet.GetNum(), Packet.GetNum()-Packet.GetPos());
      }

      if (Msg.ChanIndex < 0 || Msg.ChanIndex >= MAX_CHANNELS) {
        // this is not fatal
        GCon->Logf(NAME_DevNet, "%s: got message for channel with invalid index %d", *GetAddress(), Msg.ChanIndex);
        continue;
      }

      // ignore already processed reliable packets
      if (Msg.bReliable && Msg.ChanSequence <= InReliable[Msg.ChanIndex]) {
        // this is not fatal
        GCon->Logf(NAME_DevNet, "%s: got outdated message (channel #%d; msgseq=%u; currseq=%u)", *GetAddress(), Msg.ChanIndex, Msg.ChanSequence, InReliable[Msg.ChanIndex]);
        continue;
      }

      // get existing channel
      VChannel *chan = Channels[Msg.ChanIndex];

      // discard unreliable message to closed/inexisting channel
      if (!Msg.bReliable && (!chan || chan->Closing)) {
        // this is not fatal
        GCon->Logf(NAME_DevNet, "%s: got unreliable message before open message (channel #%d; msgseq=%u; currseq=%u)", *GetAddress(), Msg.ChanIndex, Msg.ChanSequence, InReliable[Msg.ChanIndex]);
        continue;
      }

      // create channel if necessary
      if (!chan) {
        if (Msg.ChanType == 0 || Msg.ChanType >= CHANNEL_MAX) {
          // this is not fatal
          GCon->Logf(NAME_DevNet, "%s: got message for channel #%d with invalid channel type %u (msgseq=%u; currseq=%u)", *GetAddress(), Msg.ChanIndex, Msg.ChanType, Msg.ChanSequence, InReliable[Msg.ChanIndex]);
          continue;
        }

        // reliable (either open or later), so create new channel
        chan = CreateChannel(Msg.ChanType, Msg.ChanIndex, false); // opened remotely
        if (!chan) {
          // cannot create channel, send close packet to notify the remote about it
          VMessageOut refmsg(Msg.ChanType, Msg.ChanIndex, true/*reliable*/);
          refmsg.bClose = true;
          chan->SendMessage(&refmsg);
          Flush();
          delete chan;
          vassert(Msg.ChanIndex != 0); // the thing that should not happen
          continue;
        }
      }

      if (Msg.bOpen) chan->OpenAcked = true;

      // let channel process the message
      chan->ReceivedMessage(Msg); // WARNING: channel may delete itself there!
      ++InMsgAcc;
    }
  }
}


//==========================================================================
//
//  VNetConnection::Prepare
//
//  called before sending anything
//
//==========================================================================
void VNetConnection::Prepare (int addBits) {
  vassert(addBits >= 0);

  // flush if not enough space
  if (CalcEstimatedByteSize(addBits) > MAX_DGRAM_SIZE) {
    //GCon->Logf(NAME_DevNet, "*** %s: FLUSHING: %d (%d) (max=%d)", *GetAddress(), Out.GetNumBits()+addBits, CalcFinalisedBitSize(Out.GetNumBits()+addBits), MAX_MSG_SIZE_BITS);
    Flush();
  } else {
    //GCon->Logf(NAME_DevNet, "*** %s: collecting: %d (%d) (max=%d)", *GetAddress(), Out.GetNumBits()+addBits, CalcFinalisedBitSize(Out.GetNumBits()+addBits), MAX_MSG_SIZE_BITS);
  }

  // put packet id for new packet
  if (Out.GetNumBits() == 0) {
    Out << OutPacketId;
    vassert(Out.GetNumBits() <= MAX_PACKET_HEADER_BITS);
  }

  // make sure there's enough space now
  if (CalcEstimatedByteSize(addBits) > MAX_DGRAM_SIZE) {
    //GCon->Logf(NAME_DevNet, "*** %s: ERROR: %d (%d) (max=%d)", *GetAddress(), Out.GetNumBits()+addBits, CalcFinalisedBitSize(Out.GetNumBits()+addBits), MAX_MSG_SIZE_BITS);
    Sys_Error("%s: cannot send packet of size %d+%d (newsize is %d, max size is %d)", *GetAddress(), Out.GetNumBits(), addBits, CalcEstimatedByteSize(Out.GetNumBits()+addBits), MAX_DGRAM_SIZE);
  } else {
    //GCon->Logf(NAME_DevNet, "*** %s: collector: %d (%d) (max=%d)", *GetAddress(), Out.GetNumBits()+addBits, CalcFinalisedBitSize(Out.GetNumBits()+addBits), MAX_MSG_SIZE_BITS);
  }
}


//==========================================================================
//
//  VNetConnection::ResendAcks
//
//==========================================================================
void VNetConnection::ResendAcks (bool allowOutOverflow) {
  if (!AutoAck) {
    for (auto &&ack : AcksToResend) {
      if (allowOutOverflow) {
        Prepare(STRM_INDEX_U_BYTES(ack)*8+1);
      } else {
        if (Out.GetNumBytes()+STRM_INDEX_U_BYTES(ack)+4 >= MAX_DGRAM_SIZE-2) return;
      }
      Out.WriteBit(true); // ack flag
      Out << STRM_INDEX_U(ack);
    }
  }
}


//==========================================================================
//
//  VNetConnection::ForgetResendAcks
//
//==========================================================================
void VNetConnection::ForgetResendAcks () {
  AcksToResend.reset();
}


//==========================================================================
//
//  VNetConnection::SendPacketAck
//
//==========================================================================
void VNetConnection::SendPacketAck (vuint32 AckPacketId) {
  if (AutoAck) { AcksToResend.reset(); return; }
  ResendAcks();
  ForgetResendAcks();
  // queue current ack, and send it
  QueuedAcks.append(AckPacketId);
  // queue current ack
  Prepare(STRM_INDEX_U_BYTES(AckPacketId)*8+1);
  Out.WriteBit(true); // ack flag
  Out << STRM_INDEX_U(AckPacketId);
}


//==========================================================================
//
//  VNetConnection::SendMessage
//
//==========================================================================
void VNetConnection::SendMessage (VMessageOut *Msg) {
  //if (net_dbg_conn_show_dgrams) GCon->Logf(NAME_DevNet, "%s: saving message to outbuf; out=%d; msg=%d", *GetAddress(), Out.GetNumBytes(), Msg.GetNumBytes());
  vassert(Msg);
  vassert(!Msg->IsError());
  vassert(!Msg->bReceivedAck);
  ++OutMsgAcc;
  ForceFlush = true;

  VBitStreamWriter hdr(MAX_MSG_SIZE_BITS+16, false); // no expand
  Msg->PacketId = OutPacketId;
  Msg->WriteHeader(hdr);

  // make sure we have enough room for the message with that header
  Prepare(hdr.GetNumBits()+Msg->GetNumBits());

  // outgoing packet id may change
  if (Msg->PacketId != OutPacketId) {
    hdr.Reinit(MAX_MSG_SIZE_BITS+16, false); // no expand
    Msg->PacketId = OutPacketId;
    Msg->WriteHeader(hdr);
  }

  Msg->Time = Driver->GetNetTime();

  Out.SerialiseBits(hdr.GetData(), hdr.GetNumBits());
  Out.SerialiseBits(Msg->GetData(), Msg->GetNumBits());
}


//==========================================================================
//
//  VNetConnection::Flush
//
//==========================================================================
void VNetConnection::Flush () {
  if (IsClosed()) return;

  if (Out.IsError()) {
    GCon->Logf(NAME_DevNet, "!!! %s: out collector errored! bits=%d", *GetAddress(), Out.GetNumBits());
  }
  vassert(!Out.IsError());
  ForceFlush = false;

  // if there is any pending data to send, send it
  if (Out.GetNumBits() || IsKeepAliveExceeded()) {
    // if sending keepalive packet, still generate header
    if (Out.GetNumBits() == 0) {
      Prepare(0);
      // this looks like keepalive packet, so resend last acks there too
      // this is to avoid client timeout on bad connection
      if (LastInPacketIdAck != 0xffffffffu) {
        Out.WriteBit(true);
        Out << STRM_INDEX_U(LastInPacketIdAck);
        ResendAcks(false); // no overflow
      }
    } else {
      ++Driver->packetsSent;
    }

    // add trailing bit so we can find out how many bits the message has
    Out.WriteTrailingBit();
    vassert(!Out.IsError());

    // send the message
    const float lossPrc = net_dbg_send_loss.asFloat();
    if (lossPrc <= 0.0f || RandomFull()*100.0f >= lossPrc) {
      if (NetCon->SendMessage(Out.GetData(), Out.GetNumBytes()) == -1) {
        GCon->Logf(NAME_DevNet, "%s: error sending datagram", *GetAddress());
        State = NETCON_Closed;
        return;
      }
    }
    LastSendTime = Driver->GetNetTime();

    //if (!IsLocalConnection()) ++Driver->MessagesSent;
    ++Driver->UnreliableMessagesSent;

    const unsigned arrIndex = OutPacketId&(ARRAY_COUNT(OutLagPacketId)-1);
    OutLagPacketId[arrIndex] = OutPacketId;
    OutLagTime[arrIndex] = Driver->GetNetTime();
    ++OutPacketId;
    ++OutPktAcc;
    SaturaDepth += Out.GetNumBytes();
    OutByteAcc += Out.GetNumBytes();

    Out.Reinit(MAX_DGRAM_SIZE*8+128, false); // don't expand
  }

  // move queued acks to resend queue
  for (auto &&ack : QueuedAcks) AcksToResend.append(ack);
  QueuedAcks.reset();
}


//==========================================================================
//
//  VNetConnection::KeepaliveTick
//
//==========================================================================
void VNetConnection::KeepaliveTick () {
  Tick();
}


//==========================================================================
//
//  VNetConnection::Tick
//
//==========================================================================
void VNetConnection::Tick () {
  if (IsClosed()) return;

  // for bots and demo playback there's no other end that will send us
  // the ACK so just mark all outgoing messages as ACK-ed
  // `AutoAck == true` means "demo recording"
  if (AutoAck) {
    LastReceiveTime = Driver->GetNetTime();
    for (int f = OpenChannels.length()-1; f >= 0; --f) {
      VChannel *chan = OpenChannels[f];
      for (VMessageOut *Msg = chan->OutList; Msg; Msg = Msg->Next) Msg->bReceivedAck = true;
      chan->OpenAcked = true;
      chan->ReceivedAcks();
    }
  }

  // get frame time
  double ctt = Sys_Time();
  FrameDeltaTime = ctt-LastFrameStartTime;
  LastFrameStartTime = ctt;
  CumulativeTime += FrameDeltaTime;
  ++StatsFrameCounter;

  if (CumulativeTime > 1.0f) {
    AverageFrameTime = CumulativeTime/StatsFrameCounter;
    CumulativeTime = 0;
    StatsFrameCounter = 0;
  }

  ctt = Driver->GetNetTime();

  // update stats roughly once per second
  if (ctt-LastStatsUpdateTime > 1) {
    const double deltaTime = ctt-LastStatsUpdateTime;
    InRate = InByteAcc/deltaTime;
    OutRate = OutByteAcc/deltaTime;
    InPackets = InPktAcc/deltaTime;
    OutPackets = OutPktAcc/deltaTime;
    InMessages = InMsgAcc/deltaTime;
    OutMessages = OutMsgAcc/deltaTime;
    InOrder = InOrdAcc/deltaTime;
    InLoss = 100.0*InLossAcc/max2(InPackets+InLossAcc, 1.0);
    OutLoss = 100.0*OutLossAcc/max2(OutPackets, 1.0);
    if (LagCount) AvgLag = LagAcc/LagCount;
    PrevLag = AvgLag;

    // if in or out loss is more than... let's say 20, this is high packet loss, show something to the user

    if (net_dbg_report_stats) {
      GCon->Logf("*** %s: lag:(%d,%d) %d; rate:%g/%g; packets:%g/%g; messages:%g/%g; order:%g/%g; loss:%g/%g", *GetAddress(),
        (int)(PrevLag*1000), (int)(AvgLag*1000), (int)((PrevLag+1.2*(max2(InLoss, OutLoss)*0.01))*1000),
        InRate, OutRate, InPackets, OutPackets, InMessages, OutMessages, InOrder, OutOrder, InLoss, OutLoss);
    }

    // init counters
    LagAcc = 0;
    PrevLagAcc = 9999;
    InByteAcc = 0;
    OutByteAcc = 0;
    InPktAcc = 0;
    OutPktAcc = 0;
    InMsgAcc = 0;
    OutMsgAcc = 0;
    InLossAcc = 0;
    OutLossAcc = 0;
    InOrdAcc = 0;
    LagCount = 0;
    LastStatsUpdateTime = ctt;
  }

  // compute time passed since last update
  double DeltaTime = ctt-LastTickTime;
  LastTickTime = ctt;

  // update queued byte count
  double DeltaBytes = (double)GetNetSpeed()*DeltaTime;
  if (DeltaBytes > 0x1fffffff) DeltaBytes = 0x1fffffff;
  SaturaDepth -= (int)DeltaBytes;
  double AllowedLag = DeltaBytes*2;
  if (AllowedLag > 0x3fffffff) AllowedLag = 0x3fffffff;
  if (SaturaDepth < -AllowedLag) SaturaDepth = (int)(-AllowedLag);

  // tick the channels
  for (int f = OpenChannels.length()-1; f >= 0; --f) {
    VChannel *chan = OpenChannels[f];
    chan->Tick();
  }

  // if channel 0 has closed, mark the conection as closed
  if (!Channels[0] && (OutReliable[0]|InReliable[0])) {
    ShowTimeoutStats();
    State = NETCON_Closed;
    return;
  }

  ResendAcks();
  ForgetResendAcks();

  // also, flush if we have no room for more data in outgoing accumulator
  if (ForceFlush || IsKeepAliveExceeded() || CalcEstimatedByteSize() == MAX_MSG_SIZE_BITS) Flush();

  // see if this connection has timed out
  if (IsTimeoutExceeded()) {
    ShowTimeoutStats();
    State = NETCON_Closed;
    return;
  }
}


//==========================================================================
//
//  VNetConnection::SendCommand
//
//==========================================================================
void VNetConnection::SendCommand (VStr Str) {
  VMessageOut msg(GetGeneralChannel());
  msg << Str;
  GetGeneralChannel()->SendMessage(&msg);
}


//==========================================================================
//
//  VNetConnection::SetupFatPVS
//
//==========================================================================
void VNetConnection::SetupFatPVS () {
  float dummy_bbox[6] = { -99999, -99999, -99999, 99999, 99999, 99999 };
  VLevel *Level = Context->GetLevel();
  if (!Level) return;

  //LeafPvs = Level->LeafPVS(Owner->MO->SubSector);
  LeafPvs = nullptr;

  // re-allocate PVS buffer if needed
  if (UpdatePvsSize != (Level->NumSubsectors+7)/8) {
    if (UpdatePvs) {
      delete[] UpdatePvs;
      UpdatePvs = nullptr;
    }
    UpdatePvsSize = (Level->NumSubsectors+7)/8;
    UpdatePvs = new vuint8[UpdatePvsSize];
  }

  // build view PVS using view clipper
  memset(UpdatePvs, 0, UpdatePvsSize);
  //GCon->Logf("FATPVS: view=(%g,%g,%g)", Owner->ViewOrg.x, Owner->ViewOrg.y, Owner->ViewOrg.z);
  Clipper.ClearClipNodes(Owner->ViewOrg, Level);
  //Clipper.check2STextures = false;
  Clipper.RepSectors = (GetLevelChannel() ? GetLevelChannel()->Sectors : nullptr);
  SetupPvsNode(Level->NumNodes-1, dummy_bbox);
}


//==========================================================================
//
//  VNetConnection::SetupPvsNode
//
//==========================================================================
void VNetConnection::SetupPvsNode (int BspNum, float *BBox) {
  VLevel *Level = Context->GetLevel();
  vassert(Level);
#ifdef VV_CLIPPER_FULL_CHECK
  if (Clipper.ClipIsFull()) return;
#endif
  if (!Clipper.ClipIsBBoxVisible(BBox)) return;

  if (BspNum == -1) {
    int SubNum = 0;
    subsector_t *Sub = &Level->Subsectors[SubNum];
    if (!Sub->sector->linecount) return; // skip sectors containing original polyobjs
    if (LeafPvs && !(LeafPvs[SubNum>>3]&(1<<(SubNum&7)))) return;
    if (Clipper.ClipCheckSubsector(Sub)) UpdatePvs[SubNum>>3] |= 1<<(SubNum&7);
    Clipper.ClipAddSubsectorSegs(Sub);
    return;
  }

  // found a subsector?
  if (!(BspNum&NF_SUBSECTOR)) {
    node_t *Bsp = &Level->Nodes[BspNum];
    // decide which side the view point is on
    int Side = Bsp->PointOnSide(Owner->ViewOrg);
    // recursively divide front space
    SetupPvsNode(Bsp->children[Side], Bsp->bbox[Side]);
    // possibly divide back space
    //if (!Clipper.ClipIsBBoxVisible(Bsp->bbox[Side^1])) return;
    return SetupPvsNode(Bsp->children[Side^1], Bsp->bbox[Side^1]);
  } else {
    int SubNum = BspNum&~NF_SUBSECTOR;
    subsector_t *Sub = &Level->Subsectors[SubNum];
    if (!Sub->sector->linecount) return; // skip sectors containing original polyobjs
    if (LeafPvs && !(LeafPvs[SubNum>>3]&(1<<(SubNum&7)))) return;
    if (Clipper.ClipCheckSubsector(Sub)) UpdatePvs[SubNum>>3] |= 1<<(SubNum&7);
    Clipper.ClipAddSubsectorSegs(Sub);
  }
}


//==========================================================================
//
//  VNetConnection::CheckFatPVS
//
//==========================================================================
int VNetConnection::CheckFatPVS (subsector_t *Subsector) {
  VLevel *Level = Context->GetLevel();
  if (!Level) return 0;
  //return true; //k8: this returns "always visible" for sector: more data, no door glitches
  int ss = (int)(ptrdiff_t)(Subsector-Context->GetLevel()->Subsectors);
  return UpdatePvs[ss/8]&(1<<(ss&7));
}


//==========================================================================
//
//  VNetConnection::SecCheckFatPVS
//
//==========================================================================
bool VNetConnection::SecCheckFatPVS (sector_t *Sec) {
  VLevel *Level = Context->GetLevel();
  if (!Level) return false;
  for (subsector_t *Sub = Sec->subsectors; Sub; Sub = Sub->seclink) {
    if (CheckFatPVS(Sub)) return true;
  }
  return false;
}


//==========================================================================
//
//  VNetConnection::IsRelevant
//
//==========================================================================
bool VNetConnection::IsRelevant (VThinker *Th) {
  if (Th->IsGoingToDie()) return false; // anyway
  if (Th->ThinkerFlags&VThinker::TF_AlwaysRelevant) return true;
  // check if this thinker was detached
  if (DetachedThinkers.has(Th)) return false;
  VEntity *Ent = Cast<VEntity>(Th);
  if (!Ent || !Ent->Sector) return false;
  if (Ent->GetTopOwner() == Owner->MO) return true; // inventory
  if (Ent->EntityFlags&(VEntity::EF_NoSector|VEntity::EF_Invisible)) return false;
  //if (Ent->RemoteRole == ROLE_Authority) return false; // this should not end here
  if (!CheckFatPVS(Ent->SubSector)) return false;
  return true;
}


//==========================================================================
//
//  cmpPendingThinkers
//  cmpPendingGoreEnts
//
//==========================================================================
extern "C" {
  static int cmpPendingThinkers (const void *aa, const void *bb, void *ncptr) {
    if (aa == bb) return 0;
    const VThinker *ta = *(const VThinker **)aa;
    const VThinker *tb = *(const VThinker **)bb;
    // entities always wins
    if (!ta->GetClass()->IsChildOf(VEntity::StaticClass())) {
      // a is not an entity
      if (tb->GetClass()->IsChildOf(VEntity::StaticClass())) return 1; // b should come first, a > b
      // both aren't entities, sort by object id
      if (ta->GetUniqueId() < tb->GetUniqueId()) return -1;
      if (ta->GetUniqueId() > tb->GetUniqueId()) return 1;
      return 0;
    } else if (!tb->GetClass()->IsChildOf(VEntity::StaticClass())) {
      // a is entity, b is not; a should come first (a < b)
      return -1;
    }
    // both are entities
    VNetConnection *nc = (VNetConnection *)ncptr;
    const VEntity *ea = (const VEntity *)ta;
    const VEntity *eb = (const VEntity *)tb;
    // the one that is closer to the view origin should come first
    const float distaSq = (ea->Origin-nc->Owner->ViewOrg).length2DSquared();
    const float distbSq = (eb->Origin-nc->Owner->ViewOrg).length2DSquared();
    if (distaSq < distbSq) return -1;
    if (distaSq > distbSq) return 1;
    // by unique id
    if (ta->GetUniqueId() < tb->GetUniqueId()) return -1;
    if (ta->GetUniqueId() > tb->GetUniqueId()) return 1;
    return 0;
  }

  static int cmpPendingGoreEnts (const void *aa, const void *bb, void *ncptr) {
    if (aa == bb) return 0;
    const VEntity *ea = *(const VEntity **)aa;
    const VEntity *eb = *(const VEntity **)bb;
    VNetConnection *nc = (VNetConnection *)ncptr;
    // the one that is closer to the view origin should come first
    const float distaSq = (ea->Origin-nc->Owner->ViewOrg).length2DSquared();
    const float distbSq = (eb->Origin-nc->Owner->ViewOrg).length2DSquared();
    if (distaSq < distbSq) return -1;
    if (distaSq > distbSq) return 1;
    // by unique id
    if (ea->GetUniqueId() < eb->GetUniqueId()) return -1;
    if (ea->GetUniqueId() > eb->GetUniqueId()) return 1;
    return 0;
  }
}


//==========================================================================
//
//  VNetConnection::UpdateThinkers
//
//==========================================================================
void VNetConnection::UpdateThinkers () {
  PendingThinkers.reset();
  PendingGoreEnts.reset();
  AliveGoreChans.reset();

  // use counter trick to mark updated channels
  if ((++UpdateFrameCounter) == 0) {
    // reset all counters
    UpdateFrameCounter = 1;
    for (auto &&chan : OpenChannels) {
      if (chan->IsThinker()) ((VThinkerChannel *)chan)->LastUpdateFrame = 0;
    }
  }

  // update mobjs in sight
  for (TThinkerIterator<VThinker> th(Context->GetLevel()); th; ++th) {
    if (!IsRelevant(*th)) continue;
    VThinkerChannel *chan = ThinkerChannels.FindPtr(*th);
    if (!chan) {
      // add gore entities as last ones
      if (VStr::startsWith(th->GetClass()->GetName(), "K8Gore")) {
        vassert(th->GetClass()->IsChildOf(VEntity::StaticClass()));
        PendingGoreEnts.append((VEntity *)(*th));
        continue;
      }
      // not a gore, remember this thinker; we'll sort them later
      PendingThinkers.append(*th);
      continue;
    }
    if (chan->CanSendData()) {
      chan->Update();
    } else {
      NeedsUpdate = true;
      chan->LastUpdateFrame = UpdateFrameCounter;
    }
  }


  // close entity channels that were not updated in this frame
  // WARNING! `Close()` should not delete a channel!
  for (auto &&chan : OpenChannels) {
    if (chan->IsThinker()) {
      VThinkerChannel *tc = (VThinkerChannel *)chan;
      if (tc->LastUpdateFrame != UpdateFrameCounter) {
        if (!chan->Closing) {
          if (chan->CanSendClose()) {
            chan->Close();
          } else {
            NeedsUpdate = true;
          }
        }
        continue;
      }
      // remember alive gore entities
      if (!tc->Closing && tc->Thinker && VStr::startsWith(tc->Thinker->GetClass()->GetName(), "K8Gore")) {
        AliveGoreChans.append(chan->Index);
      }
    }
  }

  // if we have some pending thinkers, open channels for them
  if (PendingThinkers.length()) {
    static_assert(sizeof(PendingThinkers[0]) == sizeof(VThinker *), "wtf?!");
    // sort them
    timsort_r(PendingThinkers.ptr(), PendingThinkers.length(), sizeof(PendingThinkers[0]), &cmpPendingThinkers, (void *)this);
    // if we have not enough free channels, remove gore entities (they will be removed sometimes in the future)
    if (AliveGoreChans.length() && MAX_CHANNELS-OpenChannels.length() < PendingThinkers.length()) {
      int needChans = PendingThinkers.length()-(MAX_CHANNELS-OpenChannels.length());
      while (AliveGoreChans.length() && needChans-- > 0) {
        // pop index
        int idx = AliveGoreChans[AliveGoreChans.length()-1];
        AliveGoreChans.removeAt(AliveGoreChans.length()-1);
        vassert(idx >= CHANIDX_ThinkersStart && idx < MAX_CHANNELS);
        vassert(Channels[idx]);
        vassert(Channels[idx]->IsThinker());
        // close channel
        VThinkerChannel *tc = (VThinkerChannel *)Channels[idx];
        vassert(tc->Thinker && tc->Thinker->GetClass()->IsChildOf(VEntity::StaticClass()));
        //PendingGoreEnts.append((VEntity *)(tc->Thinker));
        //??? CanSendClose?
        if (tc->CanSendData()) {
          tc->Close();
        } else {
          NeedsUpdate = true;
        }
      }
    }
    // append thinkers (do not check network, we need to do this unconditionally)
    for (auto &&th : PendingThinkers) {
      VThinkerChannel *chan = (VThinkerChannel *)CreateChannel(CHANNEL_Thinker, -1, true); // local channel
      if (!chan) break; // no room
      chan->SetThinker(th);
      chan->Update();
    }
  }

  // append gore entities if we have any free slots
  if (PendingGoreEnts.length() && MAX_CHANNELS-OpenChannels.length() > 0) {
    // sort them
    timsort_r(PendingGoreEnts.ptr(), PendingGoreEnts.length(), sizeof(PendingGoreEnts[0]), &cmpPendingGoreEnts, (void *)this);
    for (auto &&it : PendingGoreEnts) {
      if (!CanSendData()) {
        NeedsUpdate = true;
        return;
      }
      VThinkerChannel *chan = (VThinkerChannel *)CreateChannel(CHANNEL_Thinker, -1, true); // local channel
      if (!chan) break; // no room
      chan->SetThinker(it);
      chan->Update();
    }
  }
}


//==========================================================================
//
//  VNetConnection::UpdateLevel
//
//==========================================================================
void VNetConnection::UpdateLevel () {
  if (IsClient()) return; // client never does this
  if (!GetLevelChannel()->Level) return;
  vassert(IsServer());

  if (!CanSendData()) return;

  const double ctt = Sys_Time();
  if (LastLevelUpdateTime > ctt) return;
  LastLevelUpdateTime = ctt+1.0/(double)clampval(sv_fps.asFloat(), 5.0f, 90.0f);

  NeedsUpdate = false; // note that we already sent an update
  SetupFatPVS();

  GetLevelChannel()->Update();
  // we still need to call this, so the engine can send update for new thinkers
  UpdateThinkers();
  //!GCon->Logf(NAME_DevNet, "%s: UpLevel; %d messages in queue", *GetAddress(), sendQueueSize);
}


//==========================================================================
//
//  VNetConnection::SendServerInfo
//
//==========================================================================
void VNetConnection::SendServerInfo () {
  if (!ObjMapSent) return;

  GCon->Log(NAME_DevNet, "sending server info...");
  // this will load level on client side
  GetLevelChannel()->SetLevel(GLevel);
  GetLevelChannel()->SendNewLevel();
  LevelInfoSent = true;
}


//==========================================================================
//
//  VNetConnection::LoadedNewLevel
//
//==========================================================================
void VNetConnection::LoadedNewLevel () {
  GCon->Log(NAME_DevNet, "LEVEL RESET");
  ObjMapSent = false;
  LevelInfoSent = false;
  // this will load level on client side
  GetLevelChannel()->SetLevel(GLevel);
  GetLevelChannel()->SendNewLevel();
  LevelInfoSent = true;
}


//==========================================================================
//
//  VNetConnection::ResetLevel
//
//==========================================================================
void VNetConnection::ResetLevel () {
  if (!GetLevelChannel()->Level) return;
  // close entity channels
  for (auto &&chan : OpenChannels) {
    if (!chan) continue;
    if (chan->IsThinker()) {
      chan->Close(); //k8: should we close it, or we can simply kill it?
    }
  }
  GetLevelChannel()->ResetLevel();
}


//==========================================================================
//
//  VNetConnection::Intermission
//
//==========================================================================
void VNetConnection::Intermission (bool active) {
  (void)active;
}
