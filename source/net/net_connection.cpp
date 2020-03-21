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
static VCvarB net_dbg_conn_show_dgrams("net_dbg_conn_show_dgrams", false, "Show datagram activity?");
static VCvarB net_dbg_report_stats("net_dbg_report_stats", false, "Report some stats to the console?");

static VCvarB net_dbg_conn_dump_tick("net_dbg_conn_dump_tick", false, "Dump tick/getmessage calls?");
static VCvarB net_dbg_conn_dump_acks("net_dbg_conn_dump_acks", false, "Show ack info?");

static VCvarB net_dbg_detailed_disconnect_stats("net_dbg_detailed_disconnect_stats", false, "Show channels on disconnect?", 0/*CVAR_Archive*/);

VCvarB net_debug_dump_recv_packets("net_debug_dump_recv_packets", false, "Dump received packets?");

//FIXME: autoadjust this according to average ping
static VCvarI net_speed_limit("net_speed_limit", "560000", "Network speed limit, bauds (rough).", 0/*CVAR_Archive*/);
//static VCvarI net_speed_limit("net_speed_limit", "28000", "Network speed limit, bauds (rough).", 0/*CVAR_Archive*/);
// the network layer will force packet sending after this interval
static VCvarI net_keepalive("net_keepalive", "60", "Network keepalive time, in milliseconds.", 0);
static VCvarF net_timeout("net_timeout", "4", "Network timeout, in seconds.", 0);

// level will be updated twice as more times as this, until i wrote client-side interpolation code
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
  , LastThinkersUpdateTime(0)
  , UpdateFrameCounter(0)
  , UpdateFingerUId(0)
  , ObjMapSent(false)
  , LevelInfoSent(LNFO_UNSENT)
  , Out(MAX_DGRAM_SIZE*8+128, false) // cannot grow
  //, UpdatePvs(nullptr)
  //, UpdatePvsSize(0)
  , LeafPvs(nullptr)
{
  OriginField = VEntity::StaticClass()->FindFieldChecked("Origin");
  DataGameTimeField = VEntity::StaticClass()->FindFieldChecked("DataGameTime");

  memcpy(AuthKey, ANetCon->AuthKey, VNetUtils::ChaCha20KeySize);

  InRate = OutRate = 0;
  InPackets = OutPackets = 0;
  InMessages = OutMessages = 0;
  InLoss = OutLoss = 0;
  InOrder = OutOrder = 0;
  PrevLag = AvgLag = 0;

  LagAcc = 0;
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
  OutLastWrittenAck = 0;

  //FIXME: driver time?
  LastReceiveTime = 0;
  LastSendTime = 0;
  LastTickTime = 0;
  LastStatsUpdateTime = 0;

  SaturaDepth = 0;
  //LagAcc = 9999;
  //PrevLag = 9999;
  //AvgLag = 9999;

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
  /*
  if (UpdatePvs) {
    delete[] UpdatePvs;
    UpdatePvs = nullptr;
  }
  */
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
//  VNetConnection::GetNetSpeed
//
//==========================================================================
int VNetConnection::GetNetSpeed () const noexcept {
  if (AutoAck) return 100000000;
  if (IsLocalConnection()) return 100000000;
  return max2(2400, net_speed_limit.asInt());
}


//==========================================================================
//
//  VNetConnection::IsKeepAliveExceeded
//
//==========================================================================
bool VNetConnection::IsKeepAliveExceeded () {
  if (IsClosed()) return false; // no wai
  if (AutoAck || IsLocalConnection()) return false;
  const double kt = clampval(net_keepalive.asInt(), 5, 1000)/1000.0;
  const double ctt = Driver->GetNetTime();
  #if 1
  return (ctt-LastSendTime > kt);
  #else
  if (ctt-LastSendTime > kt) return true;
  // also, if we're getting no data from the other side for a long time, send keepalive too
  if (LastReceiveTime < 1) LastReceiveTime = Driver->GetNetTime();
  if (NetCon->LastMessageTime < 1) NetCon->LastMessageTime = Driver->GetNetTime();
  const double tout = clampval(net_timeout.asFloat(), 0.4f, 20.0f);
  return (Driver->GetNetTime()-LastReceiveTime >= tout/3.0);
  #endif
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
  const double tout = clampval(net_timeout.asFloat(), 0.4f, 20.0f);
  if (Driver->GetNetTime()-LastReceiveTime <= tout) {
    #if 0
    if (Driver->GetNetTime()-LastReceiveTime > 2.0) {
      GCon->Logf(NAME_DevNet, "%s: *** TIMEOUT IS NEAR! gtime=%g; lastrecv=%g; delta=%g", *GetAddress(), Driver->GetNetTime(), LastReceiveTime, Driver->GetNetTime()-LastReceiveTime);
    }
    #endif
    return false;
  }
  // timeout!
  return true;
}


//==========================================================================
//
//  VNetConnection::IsDangerousTimeout
//
//==========================================================================
bool VNetConnection::IsDangerousTimeout () {
  if (IsClosed()) return false; // no wai
  if (AutoAck || IsLocalConnection()) return false;
  if (LastReceiveTime < 1) LastReceiveTime = Driver->GetNetTime();
  if (NetCon->LastMessageTime < 1) NetCon->LastMessageTime = Driver->GetNetTime();
  //const double tout = clampval(net_timeout.asFloat(), 0.4f, 20.0f);
  return (Driver->GetNetTime()-LastReceiveTime >= 0.6);
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
  return (AutoAck || SaturaDepth+Out.GetNumBytes() <= 0);
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
//  VNetConnection::Close
//
//  this marks the connection as closed, but doesn't destroy anything
//
//==========================================================================
void VNetConnection::Close () {
  if (IsClosed()) return;
  ShowTimeoutStats();
  State = NETCON_Closed;
}


//==========================================================================
//
//  VNetConnection::ShowTimeoutStats
//
//==========================================================================
void VNetConnection::ShowTimeoutStats () {
  if (IsClosed()) return;
  GCon->Logf(NAME_DevNet, "%s: ERROR: Channel timed out; time delta=%g; sent %d packets (%d datagrams), received %d packets (%d datagrams)",
    *GetAddress(),
    (Driver->GetNetTime()-LastReceiveTime)*1000.0f,
    Driver->packetsSent, Driver->UnreliableMessagesSent,
    Driver->packetsReceived, Driver->UnreliableMessagesReceived);
  if (net_dbg_detailed_disconnect_stats) {
    // show extended stats
    GCon->Logf(NAME_DevNet, "  connection saturation: %d", SaturaDepth);
    GCon->Logf(NAME_DevNet, "  active channels: %d", OpenChannels.length());
    for (int f = 0; f < OpenChannels.length(); ++f) {
      VChannel *chan = OpenChannels[f];
      GCon->Logf(NAME_DevNet, "  #%d:%s: %s, open is %s%s, saturation:%d", f, *chan->GetName(),
        (chan->OpenedLocally ? "local" : "remote"),
        (chan->OpenAcked ? "acked" : "not acked"),
        (chan->Closing ? ", closing" : ""),
        chan->IsQueueFull());
      GCon->Logf(NAME_DevNet, "   in packets : %d (estimated bits: %d)", chan->InListCount, chan->InListBits);
      for (VMessageIn *msg = chan->InList; msg; msg = msg->Next) GCon->Logf(NAME_DevNet, "    %s", *msg->toStringDbg());
      GCon->Logf(NAME_DevNet, "   out packets: %d (estimated bits: %d)", chan->OutListCount, chan->OutListBits);
      for (VMessageOut *msg = chan->OutList; msg; msg = msg->Next) GCon->Logf(NAME_DevNet, "    %s", *msg->toStringDbg());
    }
  }
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
//  VNetConnection::AckEverythingEverywhere
//
//  WARNING! this can change channel list!
//
//==========================================================================
void VNetConnection::AckEverythingEverywhere () {
  for (int f = OpenChannels.length()-1; f >= 0; --f) {
    VChannel *chan = OpenChannels[f];
    for (VMessageOut *outmsg = chan->OutList; outmsg; outmsg = outmsg->Next) {
      outmsg->bReceivedAck = true;
      if (outmsg->bOpen) chan->OpenAcked = true;
    }
    chan->ReceivedAcks(); // WARNING: channel may delete itself there!
  }
}


//==========================================================================
//
//  VNetConnection::GetMessage
//
//  read and process incoming network datagram
//  returns `false` if no message was processed
//
//==========================================================================
bool VNetConnection::GetMessage (bool asHearbeat) {
  Driver->UpdateNetTime();

  // check for message arrival
  if (IsClosed()) {
    // ack all outgoing packets, just in case (this is HACK!)
    AckEverythingEverywhere();
    return false;
  }
  vassert(NetCon);

  vuint8 msgdata[MAX_DGRAM_SIZE+4];
  const int msgsize = NetCon->GetMessage(msgdata, sizeof(msgdata));
  if (msgsize == 0) return false;
  if (msgsize < 0) { Close(); return false; }

  InByteAcc += msgsize;
  ++InPktAcc;

  // received something
  ++Driver->UnreliableMessagesReceived;

  // decrypt packet
  if (msgsize < 8) return true; // too small

  vuint32 PacketId = // this is also nonce
    ((vuint32)msgdata[0])|
    (((vuint32)msgdata[1])<<8)|
    (((vuint32)msgdata[2])<<16)|
    (((vuint32)msgdata[3])<<24);

  // decrypt data
  VNetUtils::ChaCha20Ctx cctx;
  VNetUtils::ChaCha20Setup(&cctx, AuthKey, PacketId);
  VNetUtils::ChaCha20XCrypt(&cctx, msgdata+4, msgdata+4, (unsigned)(msgsize-4));

  // check crc32
  vuint32 crc32 =
    ((vuint32)msgdata[4])|
    (((vuint32)msgdata[5])<<8)|
    (((vuint32)msgdata[6])<<16)|
    (((vuint32)msgdata[7])<<24);

  msgdata[4] = msgdata[5] = msgdata[6] = msgdata[7] = 0;

  if (crc32 != VNetUtils::CRC32C(0, msgdata, (unsigned)msgsize)) return true; // invalid crc, ignore this packet

  // copy received data to packet stream
  VBitStreamReader Packet;
  // nope, this will be set by the server code
  Packet.SetupFrom(msgdata, msgsize*8, true); // fix the length with the trailing bit
  if (Packet.IsError()) {
    GCon->Logf(NAME_DevNet, "%s: datagram packet is missing trailing bit", *GetAddress());
    Close(); // close connection due to invalid data
    return false;
  }
  if (net_dbg_conn_show_dgrams || net_dbg_conn_dump_acks) GCon->Logf(NAME_DevNet, "%s: got datagram with a packet (%d bits of data)", *GetAddress(), Packet.GetNumBits());

  if (!asHearbeat) {
    ReceivedPacket(Packet);
  } else {
    NetCon->LastMessageTime = LastReceiveTime = Driver->GetNetTime();
  }

  return true;
}


//==========================================================================
//
//  VNetConnection::GetMessages
//
//==========================================================================
void VNetConnection::GetMessages (bool asHearbeat) {
  if (IsClosed()) {
    // ack all outgoing packets, just in case (this is HACK!)
    AckEverythingEverywhere();
    return;
  }

  if (net_dbg_conn_dump_tick) GCon->Logf(NAME_DevNet, "%s: GetMessages()", *GetAddress());
  #if 0
  if (!GetMessage(asHearbeat)) {
    const struct timespec sleepTime = {0, 10000}; // 1 millisecond
    nanosleep(&sleepTime, nullptr);
  }

  // we can have alot of small packets queued, so process them all
  // without this, everything will be delayed
  if (!GetMessage(asHearbeat)) return;

  //const double ctt = Sys_Time();
  int waitCount = 2;
  for (int f = 0; f < 256 && IsOpen(); ++f) {
    if (!GetMessage(asHearbeat)) {
      if (--waitCount == 0) break;
      const struct timespec sleepTime = {0, 10000/2}; // 0.5 milliseconds
      nanosleep(&sleepTime, nullptr);
      continue;
    }
    //if (Sys_Time()-ctt >= 1.0/1000.0*2.5) break;
  }
  #else
  // we can have alot of small packets queued, so process them all
  // without this, everything will be delayed
  if (!GetMessage(asHearbeat)) return; // nothing's here
  // spend no more than 2 msecs here
  int count = 0;
  const double ctt = Sys_Time();
  for (;;) {
    if (!GetMessage(asHearbeat)) break;
    ++count;
    if (count == 64) {
      count = 0;
      if (Sys_Time()-ctt >= 1.0/1000.0*2.0) break;
    }
  }
  /*
  for (int f = 0; f < 128 && IsOpen(); ++f) {
    if (!GetMessage(asHearbeat)) break;
  }
  */
  #endif
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

    if (net_dbg_conn_dump_acks) {
      for (VMessageOut *Out = chan->OutList; Out; Out = Out->Next) {
        // retransmit reliable messages in the lost packet
        if (Out->PacketId == PacketId && !Out->bReceivedAck) {
          vassert(Out->bReliable);
          GCon->Logf(NAME_DevNet, "%s: LOST: %s", *chan->GetDebugName(), *Out->toStringDbg());
        }
      }
    }

    chan->PacketLost(PacketId);
  }
}


//==========================================================================
//
//  VNetConnection::ReceivedPacket
//
//==========================================================================
void VNetConnection::ReceivedPacket (VBitStreamReader &Packet) {
  Driver->UpdateNetTime();

  // simulate receiving loss
  const float lossPrc = net_dbg_recv_loss.asFloat();
  if (lossPrc > 0.0f && RandomFull()*100.0f < lossPrc) {
    //GCon->Logf(NAME_Debug, "%s: simulated packet loss!", *GetAddress());
    return;
  }

  // read packet id
  vuint32 PacketId = 0;
  Packet << PacketId;
  if (Packet.IsError()) {
    GCon->Logf(NAME_DevNet, "%s: got invalid packet, connection dropped", *GetAddress());
    Close();
    return;
  }

  // get crc32
  vuint32 crc32 = 0xffffffffU;
  Packet << crc32;
  if (Packet.IsError() || crc32 != 0) {
    GCon->Logf(NAME_DevNet, "%s: got invalid packet, connection dropped", *GetAddress());
    Close();
    return;
  }

  // reset timeout timer
  NetCon->LastMessageTime = LastReceiveTime = Driver->GetNetTime();
  ++Driver->packetsReceived;

  if (net_debug_dump_recv_packets) GCon->Logf(NAME_DevNet, "***!!!*** Network Packet (bitcount=%d; pid=%u; inpid=%u)", Packet.GetNum(), PacketId, InPacketId);

  // check packet ordering
  if (PacketId > InPacketId) {
    InLossAcc += PacketId-InPacketId-1;
    InPacketId = PacketId;
  } else {
    ++InOrdAcc;
  }

  // ack it
  if (net_dbg_conn_dump_acks) GCon->Logf(NAME_DevNet, "%s: got packet with pid=%u, sending ack", *GetAddress(), PacketId);
  SendPacketAck(PacketId);
  NeedsUpdate = true; // we got *any* activity, update the world!

  if (Packet.AtEnd()) GCon->Logf(NAME_DevNet, "%s: got empty keepalive packet", *GetAddress());

  vuint32 lastSeenAck = 0;

  while (!Packet.AtEnd() && IsOpen()) {
    if (net_debug_dump_recv_packets) GCon->Logf(NAME_DevNet, "  parsing packet: %d bits eaten of %d", Packet.GetPos(), Packet.GetNumBits());
    //INT StartPos = Reader.GetPosBits();
    // ack?
    if (Packet.ReadBit()) {
      // yep, process it
      vuint32 AckPacketId = 0;
      if (lastSeenAck) AckPacketId = Packet.ReadUInt()+lastSeenAck; else Packet << STRM_INDEX_U(AckPacketId);
      lastSeenAck = AckPacketId;

      if (Packet.IsError()) {
        // this is fatal
        GCon->Logf(NAME_DevNet, "%s: missing ack id", *GetAddress());
        ++Driver->shortPacketCount;
        Close();
        return;
      }

      // resend any old reliable packets that the receiver hasn't acknowledged
      if (AckPacketId > OutAckPacketId) {
        if (net_dbg_conn_dump_acks) GCon->Logf(NAME_DevNet, "  ack: ackpid=%u; outackpid=%u (future)", AckPacketId, OutAckPacketId);
        for (vuint32 LostPacketId = OutAckPacketId+1; LostPacketId < AckPacketId; ++LostPacketId, ++OutLossAcc) {
          if (net_dbg_conn_dump_acks) GCon->Logf(NAME_DevNet, "  ack:   PACKETLOST: %u", LostPacketId);
          PacketLost(LostPacketId);
        }
        OutAckPacketId = AckPacketId;
      } else if (AckPacketId < OutAckPacketId) {
        // this is harmless
        if (net_dbg_conn_dump_acks) GCon->Logf(NAME_DevNet, "  ack: ackpid=%u; outackpid=%u (outdated)", AckPacketId, OutAckPacketId);
      } else {
        if (net_dbg_conn_dump_acks) GCon->Logf(NAME_DevNet, "  ack: ackpid=%u; outackpid=%u (current)", AckPacketId, OutAckPacketId);
      }

      // update lag statistics
      const unsigned arrIndex = AckPacketId&(ARRAY_COUNT(OutLagPacketId)-1);
      if (OutLagPacketId[arrIndex] == AckPacketId) {
        const double NewLag = Driver->GetNetTime()-OutLagTime[arrIndex]-(FrameDeltaTime*0.5);
        LagAcc += NewLag;
        ++LagCount;
      }

      // forward the ack to the respective channel(s)
      for (int f = OpenChannels.length()-1; f >= 0; --f) {
        VChannel *chan = OpenChannels[f];
        for (VMessageOut *outmsg = chan->OutList; outmsg; outmsg = outmsg->Next) {
          if (outmsg->PacketId == AckPacketId) {
            outmsg->bReceivedAck = true;
            if (outmsg->bOpen) chan->OpenAcked = true;
            if (net_dbg_conn_dump_acks) GCon->Logf(NAME_DevNet, "%s: ACKED: %s", *chan->GetDebugName(), *outmsg->toStringDbg());
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
        if (net_debug_dump_recv_packets) GCon->Logf(NAME_DevNet, "%s: got outdated message (channel #%d; msgseq=%u; currseq=%u)", *GetAddress(), Msg.ChanIndex, Msg.ChanSequence, InReliable[Msg.ChanIndex]);
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
    //GCon->Logf(NAME_DevNet, "*** %s: FLUSHING: %d (%d) (bytes=%d (%d); max=%d)", *GetAddress(), Out.GetNumBits(), addBits, CalcEstimatedByteSize(), CalcEstimatedByteSize(addBits), MAX_DGRAM_SIZE);
    Flush();
    //GCon->Logf(NAME_DevNet, "*** %s: FLUSHED: %d (%d) (bytes=%d (%d); max=%d)", *GetAddress(), Out.GetNumBits(), addBits, CalcEstimatedByteSize(), CalcEstimatedByteSize(addBits), MAX_DGRAM_SIZE);
  } else {
    //GCon->Logf(NAME_DevNet, "*** %s: collecting: %d (%d) (bytes=%d (%d); max=%d)", *GetAddress(), Out.GetNumBits(), addBits, CalcEstimatedByteSize(), CalcEstimatedByteSize(addBits), MAX_DGRAM_SIZE);
  }

  // put packet id for new packet
  if (Out.GetNumBits() == 0) {
    vuint32 crc32 = 0; // will be fixed later
    Out << OutPacketId;
    Out << crc32;
    vassert(Out.GetNumBits() == MAX_PACKET_HEADER_BITS);
    OutLastWrittenAck = 0;
  }

  // make sure there's enough space now
  if (CalcEstimatedByteSize(addBits) > MAX_DGRAM_SIZE) {
    Sys_Error("%s: cannot send packet of size %d+%d (newsize is %d, max size is %d)", *GetAddress(), Out.GetNumBits(), addBits, CalcEstimatedByteSize(addBits), MAX_DGRAM_SIZE);
  }
}


//==========================================================================
//
//  VNetConnection::PutOneAck
//
//  returns argument for `Prepare` if putting the ack will
//  overflow the output buffer.
//  i.e. if it returned non-zero, ack is not put.
//  if `forceSend` is `true`, flush the output buffer if
//  necessary (always sends, returns 0).
//
//==========================================================================
int VNetConnection::PutOneAck (vuint32 ackId, bool forceSend) {
  if (AutoAck) return 0;
  if (Out.GetNumBits() == 0) Prepare(0); // put header
  // we cannot ack packets from the future
  vassert(ackId <= InPacketId);
  vassert(ackId >= OutLastWrittenAck);
  // convert ack to delta, it will take much less room (usually just one byte)
  unsigned ackIdDelta = ackId-OutLastWrittenAck;
  int outBits = (OutLastWrittenAck ? BitStreamCalcUIntBits(ackIdDelta) : STRM_INDEX_U_BYTES(ackIdDelta)*8)+1;
  if (CalcEstimatedByteSize(outBits) > MAX_DGRAM_SIZE) {
    if (!forceSend) return outBits;
    Prepare(outBits);
    // recheck, and reconvert
    vassert(ackId <= OutPacketId);
    vassert(OutLastWrittenAck == 0);
    outBits = STRM_INDEX_U_BYTES(ackIdDelta)*8+1;
    vassert(CalcEstimatedByteSize(outBits) <= MAX_DGRAM_SIZE);
    ackIdDelta = ackId;
  }
  if (net_dbg_conn_dump_acks) GCon->Logf(NAME_DevNet, "%s: putting ack with pid=%u (delta=%u)", *GetAddress(), ackId, ackIdDelta);
  Out.WriteBit(true); // ack flag
  if (OutLastWrittenAck) Out.WriteUInt(ackIdDelta); else Out << STRM_INDEX_U(ackIdDelta);
  vassert(CalcEstimatedByteSize(0) <= MAX_DGRAM_SIZE);
  OutLastWrittenAck = ackId; // update current ack
  return 0;
}


extern "C" {
  static int cmpAcks (const void *aa, const void *bb, void *ncptr) {
    const vuint32 a = *(const vuint32 *)aa;
    const vuint32 b = *(const vuint32 *)bb;
    return
      a < b ? -1 :
      a > b ? 1 :
      0;
  }
}


//==========================================================================
//
//  VNetConnection::ResendAcks
//
//==========================================================================
void VNetConnection::ResendAcks () {
  if (!AutoAck) {
    // make sure that the sequence is right
    timsort_r(AcksToResend.ptr(), AcksToResend.length(), sizeof(AcksToResend[0]), &cmpAcks, nullptr);
    for (auto &&ack : AcksToResend) {
      PutOneAckForced(ack);
    }
  }
  AcksToResend.reset();
}


//==========================================================================
//
//  VNetConnection::SendPacketAck
//
//==========================================================================
void VNetConnection::SendPacketAck (vuint32 AckPacketId) {
  if (AutoAck) { AcksToResend.reset(); QueuedAcks.reset(); return; }
  // append current ack to resend queue, so it will be sent too
  AcksToResend.append(AckPacketId);
  // this call will clear resend queue
  ResendAcks();
  // queue current ack, so it will be sent one more time
  QueuedAcks.append(AckPacketId);
}


//==========================================================================
//
//  VNetConnection::SendMessage
//
//==========================================================================
void VNetConnection::SendMessage (VMessageOut *Msg) {
  Driver->UpdateNetTime();

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

  // send it if it is full, why not
  if (CalcEstimatedByteSize(0) == MAX_DGRAM_SIZE) {
    Flush();
    vassert(Out.GetNumBits() == 0);
  }
}


//==========================================================================
//
//  VNetConnection::Flush
//
//==========================================================================
void VNetConnection::Flush () {
  Driver->UpdateNetTime();

  // if the connection is closed, discard the data
  if (IsClosed()) {
    Out.Reinit(MAX_DGRAM_SIZE*8+128, false); // don't expand
    LastSendTime = Driver->GetNetTime();
    return;
  }

  if (Out.IsError()) {
    GCon->Logf(NAME_DevNet, "!!! %s: out collector errored! bits=%d", *GetAddress(), Out.GetNumBits());
  }
  vassert(!Out.IsError());
  ForceFlush = false;

  // if there is any pending data to send, send it
  if (Out.GetNumBits() || (!AutoAck && IsKeepAliveExceeded())) {
    // if sending keepalive packet, still generate header
    if (Out.GetNumBits() == 0) {
      Prepare(0); // write header
      // this looks like keepalive packet, so resend last acks there too
      // this is to avoid client timeout on bad connection
      #if 1
      if (!AutoAck) {
        // only ticker can call this with empty accumulator, and in this case we have no acks to resend
        vassert(AcksToResend.length() == 0);
        if (QueuedAcks.length()) {
          // sort queued acks, just to be sure that the sequence is right
          timsort_r(QueuedAcks.ptr(), QueuedAcks.length(), sizeof(QueuedAcks[0]), &cmpAcks, nullptr);
          while (QueuedAcks.length()) {
            if (PutOneAck(QueuedAcks[0])) break; // no room
            QueuedAcks.removeAt(0);
          }
        } else if (InPacketId) {
          // `InPacketId` is the last highest packet we've seen, so send ack for it, why not
          PutOneAck(InPacketId);
        }
        //GCon->Logf(NAME_DevNet, "%s: created keepalive packet with acks; size is %d bits", *GetAddress(), Out.GetNumBits());
      }
      #endif
    } else {
      /*
      #ifndef CLIENT
      if (!AutoAck && IsKeepAliveExceeded()) {
        GCon->Logf(NAME_DevNet, "%s: keex! bits=%d", *GetAddress(), Out.GetNumBits());
      }
      #endif
      */
      ++Driver->packetsSent;
    }

    // add trailing bit so we can find out how many bits the message has
    Out.WriteTrailingBit();
    vassert(!Out.IsError());

    // send the message
    const float lossPrc = net_dbg_send_loss.asFloat();
    if (lossPrc <= 0.0f || RandomFull()*100.0f >= lossPrc) {
      // fix crc, encrypt the message
      vuint8 *msgdata = Out.GetData();
      unsigned msgsize = Out.GetNumBytes();
      vassert(msgsize >= 4+4);

      const vuint32 nonce =
        ((vuint32)msgdata[0])|
        (((vuint32)msgdata[1])<<8)|
        (((vuint32)msgdata[2])<<16)|
        (((vuint32)msgdata[3])<<24);

      vassert(msgdata[4] == 0);
      vassert(msgdata[5] == 0);
      vassert(msgdata[6] == 0);
      vassert(msgdata[7] == 0);

      // write crc32
      const vuint32 crc32 = VNetUtils::CRC32C(0, msgdata, msgsize);
      msgdata[4] = crc32&0xffU;
      msgdata[5] = (crc32>>8)&0xffU;
      msgdata[6] = (crc32>>16)&0xffU;
      msgdata[7] = (crc32>>24)&0xffU;

      // encrypt data
      VNetUtils::ChaCha20Ctx cctx;
      VNetUtils::ChaCha20Setup(&cctx, AuthKey, nonce);
      VNetUtils::ChaCha20XCrypt(&cctx, msgdata+4, msgdata+4, (unsigned)(msgsize-4));

      int res = NetCon->SendMessage(msgdata, (int)msgsize);
      if (res < 0) {
        GCon->Logf(NAME_DevNet, "%s: error sending datagram", *GetAddress());
        Close();
        return;
      }
      if (res == 0) SaturaDepth = MAX_DGRAM_SIZE; // pause it a little
      if (net_dbg_conn_dump_acks) {
        vassert(Out.GetNumBytes() >= 4);
        //WARNING! invalid for big-endian!
        const vuint32 *pid = (const vuint32 *)Out.GetData();
        GCon->Logf(NAME_DevNet, "%s: sent packet with pid=%u (size: %d bytes)", *GetAddress(), *pid, Out.GetNumBytes());
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
    OutLastWrittenAck = 0; // just in case
  }

  // move queued acks to resend queue
  // this way we will send acks twice, just in case they're lost
  // (first time ack wass sent before it got into queued acks store)
  for (auto &&ack : QueuedAcks) AcksToResend.append(ack);
  QueuedAcks.reset();
}


//==========================================================================
//
//  VNetConnection::KeepaliveTick
//
//==========================================================================
void VNetConnection::KeepaliveTick () {
  GetMessages(true); // update counters
  Tick();
}


//==========================================================================
//
//  VNetConnection::Tick
//
//==========================================================================
void VNetConnection::Tick () {
  Driver->UpdateNetTime();

  if (IsClosed()) {
    // ack all outgoing packets, just in case (this is HACK!)
    AckEverythingEverywhere();
    // don't stop here, though, let channels tick
  }

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

    NetLagChart[NetLagChartPos] = clampval((int)((PrevLag+1.2*(max2(InLoss, OutLoss)*0.01))*1000), 0, 1000);
    NetLagChartPos = (NetLagChartPos+1)%NETLAG_CHART_ITEMS;

    if (net_dbg_report_stats) {
      GCon->Logf("*** %s: lag:(%d,%d) %d; rate:%g/%g; packets:%g/%g; messages:%g/%g; order:%g/%g; loss:%g/%g", *GetAddress(),
        (int)(PrevLag*1000), (int)(AvgLag*1000), (int)((PrevLag+1.2*(max2(InLoss, OutLoss)*0.01))*1000),
        InRate, OutRate, InPackets, OutPackets, InMessages, OutMessages, InOrder, OutOrder, InLoss, OutLoss);
    }

    // init counters
    LagAcc = 0;
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

  if (net_dbg_conn_dump_tick) {
    GCon->Logf(NAME_DevNet, "%s: tick: outbits=%d(ffl=%d); lastrecv=%g(%g); lastsend=%g(%g);", *GetAddress(),
      Out.GetNumBits(), (int)ForceFlush,
      LastReceiveTime, (Driver->GetNetTime()-LastReceiveTime)*1000,
      LastSendTime, (Driver->GetNetTime()-LastSendTime)*1000);
  }

  // see if this connection has timed out
  if (IsTimeoutExceeded()) {
    Close();
    return;
  }

  // tick the channels
  // channel should not delete itself in a ticker, but...
  // we have to do it from the first opened channel, because
  // of update priorities
  for (int f = 0; f < OpenChannels.length(); ++f) {
    VChannel *chan = OpenChannels[f];
    chan->Tick();
    // invariant
    vassert(OpenChannels[f] == chan);
  }

  // if channel 0 has closed, mark the conection as closed
  if (!Channels[0] && (OutReliable[0]|InReliable[0])) {
    Close();
    return;
  }

  ResendAcks();

  // update queued byte count
  // need to be here, because `Flush()` may saturate it
  double DeltaBytes = (double)GetNetSpeed()*DeltaTime;
  if (DeltaBytes > 0x1fffffff) DeltaBytes = 0x1fffffff;
  SaturaDepth -= (int)DeltaBytes;
  double AllowedLag = DeltaBytes*2;
  if (AllowedLag > 0x3fffffff) AllowedLag = 0x3fffffff;
  if (SaturaDepth < -AllowedLag) SaturaDepth = (int)(-AllowedLag);

  // also, flush if we have no room for more data in outgoing accumulator
  if (ForceFlush || IsKeepAliveExceeded() || CalcEstimatedByteSize() == MAX_DGRAM_SIZE) Flush();
}


//==========================================================================
//
//  VNetConnection::AbortChannel
//
//  this is called by channel send/recv methods on fatal queue overflow
//  you can `delete` channel here, as it is guaranteed that call to this
//  method is followed by `return`
//  you CAN call `chan->Close()` here
//
//==========================================================================
void VNetConnection::AbortChannel (VChannel *chan) {
  vassert(chan);
  // if this is some vital channel (control, player, level) -- close connection
  // otherwise, just close this channel, and let world updater deal with it
  if (!chan->IsThinker()) {
    GCon->Logf(NAME_DevNet, "%s: aborting the connection, because vital non-thinker channel %s is oversaturated!", *GetAddress(), *chan->GetDebugName());
    Close();
    return;
  }
  VThinkerChannel *tc = (VThinkerChannel *)chan;
  if (tc->GetThinker() && (tc->GetThinker()->ThinkerFlags&VThinker::TF_AlwaysRelevant)) {
    GCon->Logf(NAME_DevNet, "%s: aborting the connection, because vital thinker channel %s is oversaturated!", *GetAddress(), *chan->GetDebugName());
    Close();
    return;
  }
  // close this channel, and hope that nothing will go wrong
  GCon->Logf(NAME_DevNet, "%s: closing thinker channel %s due oversaturation", *GetAddress(), *chan->GetDebugName());
  chan->Close();
}


//==========================================================================
//
//  VNetConnection::SendCommand
//
//==========================================================================
void VNetConnection::SendCommand (VStr Str) {
  Str = Str.xstrip();
  if (Str.length() == 0) return; // no, really
  if (Str.length() > 1200) {
    GCon->Logf(NAME_Error, "%s: sorry, cannot send command that long (%d bytes): [%s...]", *GetAddress(), Str.length(), *Str.RemoveColors().left(32));
    return;
  }
  //GCon->Logf(NAME_DevNet, "%s: sending command: \"%s\"", *GetAddress(), *Str.quote());
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
  UpdatedSubsectors.reset();
  UpdatedSectors.reset();

  VLevel *Level = Context->GetLevel();
  if (!Level) return;

  //LeafPvs = Level->LeafPVS(Owner->MO->SubSector);
  LeafPvs = nullptr;

  // re-allocate PVS buffer if needed
  /*
  if (UpdatePvsSize != (Level->NumSubsectors+7)/8) {
    if (UpdatePvs) {
      delete[] UpdatePvs;
      UpdatePvs = nullptr;
    }
    UpdatePvsSize = (Level->NumSubsectors+7)/8;
    UpdatePvs = new vuint8[UpdatePvsSize];
  }
  */

  // build view PVS using view clipper
  //memset(UpdatePvs, 0, UpdatePvsSize);
  //GCon->Logf("FATPVS: view=(%g,%g,%g)", Owner->ViewOrg.x, Owner->ViewOrg.y, Owner->ViewOrg.z);
  Clipper.ClearClipNodes(Owner->ViewOrg, Level);
  //Clipper.check2STextures = false;
  Clipper.RepSectors = (GetLevelChannel() ? GetLevelChannel()->Sectors : nullptr);

  float dummy_bbox[6] = { -99999, -99999, -99999, 99999, 99999, 99999 };
  SetupPvsNode(Level->NumNodes-1, dummy_bbox);
}


//==========================================================================
//
//  VNetConnection::PvsMarkExtra
//
//==========================================================================
void VNetConnection::PvsMarkExtra (sector_t *sec) {
  if (sec->othersecFloor) PvsAddSector(sec->othersecFloor);
  if (sec->othersecCeiling) PvsAddSector(sec->othersecCeiling);
  if (sec->heightsec) PvsAddSector(sec->heightsec);
  for (sec_region_t *reg = sec->eregions->next; reg; reg = reg->next) {
    line_t *line = reg->extraline;
    if (!line) continue;
    if (line->frontsector) PvsAddSector(line->frontsector);
    if (line->backsector) PvsAddSector(line->backsector);
  }
}


//==========================================================================
//
//  VNetConnection::PvsAddSector
//
//==========================================================================
void VNetConnection::PvsAddSector (sector_t *sec) {
  VLevel *Level = Context->GetLevel();
  if (UpdatedSectors.put((vint32)(ptrdiff_t)(sec-&Level->Sectors[0]), true)) return;
  PvsMarkExtra(sec);
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
    if (Clipper.ClipCheckSubsector(Sub)) {
      //UpdatePvs[SubNum>>3] |= 1<<(SubNum&7);
      UpdatedSubsectors.put(SubNum, true);
      PvsAddSector(Sub->sector);
    }
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
    if (Clipper.ClipCheckSubsector(Sub)) {
      //UpdatePvs[SubNum>>3] |= 1<<(SubNum&7);
      UpdatedSubsectors.put(SubNum, true);
      PvsAddSector(Sub->sector);
    }
    Clipper.ClipAddSubsectorSegs(Sub);
  }
}


//==========================================================================
//
//  VNetConnection::CheckFatPVS
//
//==========================================================================
bool VNetConnection::CheckFatPVS (const subsector_t *Subsector) {
  VLevel *Level = Context->GetLevel();
  if (!Level) return 0;
  //return true; //k8: this returns "always visible" for sector: more data, no door glitches
  return UpdatedSubsectors.has((vint32)(ptrdiff_t)(Subsector-&Level->Subsectors[0]));
  /*
  int ss = (int)(ptrdiff_t)(Subsector-Context->GetLevel()->Subsectors);
  return UpdatePvs[ss/8]&(1<<(ss&7));
  */
}


//==========================================================================
//
//  VNetConnection::SecCheckFatPVS
//
//==========================================================================
bool VNetConnection::SecCheckFatPVS (const sector_t *Sec) {
  VLevel *Level = Context->GetLevel();
  if (!Level) return false;
  /*
  for (subsector_t *Sub = Sec->subsectors; Sub; Sub = Sub->seclink) {
    if (CheckFatPVS(Sub)) return true;
  }
  return false;
  */
  return UpdatedSectors.has((vint32)(ptrdiff_t)(Sec-&Level->Sectors[0]));
}


//==========================================================================
//
//  VNetConnection::IsRelevant
//
//==========================================================================
bool VNetConnection::IsRelevant (VThinker *th) {
  if (th->IsGoingToDie()) return false; // anyway
  if (th->ThinkerFlags&VThinker::TF_AlwaysRelevant) return true; // always
  if (th->ThinkerFlags&VThinker::TF_ServerSideOnly) return false; // never
  // check if this thinker was detached
  if (DetachedThinkers.has(th)) return false;
  VEntity *Ent = Cast<VEntity>(th);
  if (!Ent) return false;
  if (Ent == Owner->MO || Ent->GetTopOwner() == Owner->MO) return true; // inventory
  if (!Ent->Sector) return false; // just in case
  if (Ent->EntityFlags&(VEntity::EF_NoSector|VEntity::EF_Invisible)) return false;
  // if we're in coop, always transmit other players (we need them for the automap)
  if (!svs.deathmatch && Ent->IsPlayer() && !Ent->IsRealCorpse() &&
      Ent->Player && Ent->Player->MO == Ent && (Ent->Player->PlayerFlags&VBasePlayer::PF_Spawned))
  {
    //GCon->Logf(NAME_DevNet, "%s: client #%d", *GetAddress(), Ent->Player->ClientNum);
    return true;
  }
  //if (Ent->RemoteRole == ROLE_Authority) return false; // this should not end here
  if (CheckFatPVS(Ent->SubSector)) return true;
  // invisible, but simulated are still relevant
  return SimulatedThinkers.has(Ent);
}


//==========================================================================
//
//  VNetConnection::IsAlwaysRelevant
//
//  inventory is always relevant too
//  doesn't check PVS
//  call after `IsRelevant()` returned `true`,
//  because this does much less checks
//
//==========================================================================
bool VNetConnection::IsAlwaysRelevant (VThinker *th) {
  if (th->ThinkerFlags&VThinker::TF_AlwaysRelevant) return true;
  VEntity *Ent = Cast<VEntity>(th);
  if (!Ent) return false;
  // we, inventory, or other player
  return (Ent == Owner->MO || Ent->GetTopOwner() == Owner->MO || Ent->IsPlayer());
}


//==========================================================================
//
//  VNetConnection::ThinkerSortInfo::ThinkerSortInfo
//
//==========================================================================
VNetConnection::ThinkerSortInfo::ThinkerSortInfo (VBasePlayer *Owner) noexcept {
  MO = Owner->MO;
  ViewOrg = Owner->ViewOrg;
  ViewAngles = Owner->ViewAngles;
  TVec fwd;
  #if 0
  AngleVector(ViewAngles, fwd);
  //GCon->Logf(NAME_DevNet, "angles=(%g,%g,%g); fwd=(%g,%g,%g)", ViewAngles.yaw, ViewAngles.pitch, ViewAngles.roll, fwd.x, fwd.y, fwd.z);
  ViewPlane.SetPointNormal3DSafe(ViewOrg, fwd);
  #else
  fwd = AngleVectorYaw(ViewAngles.yaw);
  //GCon->Logf(NAME_Debug, "yaw=%g; fwd=(%g,%g,%g)", ViewAngles.yaw, fwd.x, fwd.y, fwd.z);
  //ViewPlane.SetPointDirXY(ViewOrg+fwd*128, fwd);
  //ViewPlane.SetPointNormal3DSafe(ViewOrg+fwd*2048, fwd);
  // move it back a little, so we can avoid distance check in the other code
  ViewPlane.SetPointNormal3DSafe(ViewOrg-fwd*128, fwd);
  #endif
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
    // "always relewant" first (xor flags to see if they're equal)
    if (((ta->ThinkerFlags|tb->ThinkerFlags)&VThinker::TF_AlwaysRelevant) &&
        ((ta->ThinkerFlags^tb->ThinkerFlags)&VThinker::TF_AlwaysRelevant))
    {
      // only one thinker is "always relevant" here
      vassert((ta->ThinkerFlags&VThinker::TF_AlwaysRelevant) != (tb->ThinkerFlags&VThinker::TF_AlwaysRelevant));
      return (ta->ThinkerFlags&VThinker::TF_AlwaysRelevant ? -1 : 1);
    }
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
    const VNetConnection::ThinkerSortInfo *snfo = (const VNetConnection::ThinkerSortInfo *)ncptr;
    /*const*/ VEntity *ea = (/*const*/ VEntity *)ta;
    /*const*/ VEntity *eb = (/*const*/ VEntity *)tb;

    // player MO always first
    if (ea == snfo->MO) {
      vassert(eb != snfo->MO);
      return -1;
    }
    if (eb == snfo->MO) {
      vassert(ea != snfo->MO);
      return 1;
    }

    // inventory should come first
    if (ea->GetTopOwner() == snfo->MO) {
      // first is in inventory, check second
      if (eb->GetTopOwner() != snfo->MO) return -1; // a should come first, a < b
      // both are inventories, sort by unique it
      if (ta->GetUniqueId() < tb->GetUniqueId()) return -1;
      if (ta->GetUniqueId() > tb->GetUniqueId()) return 1;
      return 0;
    } else {
      // first is not in inventory, check second
      if (eb->GetTopOwner() == snfo->MO) return 1; // b should come first, a > b
      // neither is in inventory, use type/distance sort
    }

    // type sorting
    //TODO: pickups!
    if (((ea->EntityFlags|eb->EntityFlags)&VEntity::EF_IsPlayer)) {
      // at least one is player
      if ((ea->EntityFlags^eb->EntityFlags)&VEntity::EF_IsPlayer) {
        // one is player
        return (ea->IsPlayer() ? -1 : 1);
      }
    }

    // we moved our plane away, no need to check any distance here
    // prefer entities which are before our eyes
    const int sidea = snfo->ViewPlane.PointOnSide(ea->Origin);
    const int sideb = snfo->ViewPlane.PointOnSide(eb->Origin);
    if (sidea^sideb) {
      // different sides, prefer one that is before the camera
      return (sideb ? -1 : 1); // if b is behind our back, a is first (a < b), otherwise b is first (a > b)
    }

    // monsters
    if (((ea->FlagsEx|eb->FlagsEx)&VEntity::EFEX_Monster)) {
      // at least one is monster
      if (((ea->FlagsEx|eb->FlagsEx)^VEntity::EFEX_Monster)) {
        // one is monster
        return (ea->IsMonster() ? -1 : 1);
      }
    }
    // projectiles
    if (((ea->EntityFlags|eb->EntityFlags)&VEntity::EF_Missile)) {
      // at least one is missile
      if ((ea->EntityFlags^eb->EntityFlags)&VEntity::EF_Missile) {
        // one is missile
        return (ea->IsPlayer() ? -1 : 1);
      }
    }
    // solid things
    if (((ea->EntityFlags|eb->EntityFlags)&VEntity::EF_Solid)) {
      // at least one is solid
      if ((ea->EntityFlags^eb->EntityFlags)&VEntity::EF_Solid) {
        // one is solid
        return (ea->IsPlayer() ? -1 : 1);
      }
    }

    // last

    // no interaction
    if (((ea->FlagsEx|eb->FlagsEx)&VEntity::EFEX_NoInteraction)) {
      // at least one is "no interaction" (always last)
      if (((ea->FlagsEx|eb->FlagsEx)^VEntity::EFEX_NoInteraction)) {
        // one is "no interaction" (always last)
        return (ea->IsMonster() ? 1 : -1);
      }
    }
    // pseudocorpse
    if (((ea->FlagsEx|eb->FlagsEx)&VEntity::EFEX_PseudoCorpse)) {
      // at least one is pseudocorpse (corpse decoration, always last)
      if (((ea->FlagsEx|eb->FlagsEx)^VEntity::EFEX_PseudoCorpse)) {
        // one is pseudocorpse (corpse decoration, always last)
        return (ea->IsMonster() ? 1 : -1);
      }
    }
    // corpse
    if (((ea->EntityFlags|eb->EntityFlags)&VEntity::EF_Corpse)) {
      // at least one is corpse
      if ((ea->EntityFlags^eb->EntityFlags)&VEntity::EF_Corpse) {
        // one is corpse (corpses are always last)
        return (ea->IsPlayer() ? 1 : -1);
      }
    }

    // the one that is closer to the view origin should come first
    const float distaSq = (ea->Origin-snfo->ViewOrg).length2DSquared();
    const float distbSq = (eb->Origin-snfo->ViewOrg).length2DSquared();
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
    const VNetConnection::ThinkerSortInfo *snfo = (const VNetConnection::ThinkerSortInfo *)ncptr;
    // check sides
    const int sidea = snfo->ViewPlane.PointOnSide(ea->Origin);
    const int sideb = snfo->ViewPlane.PointOnSide(eb->Origin);
    if (sidea^sideb) {
      // different sides, prefer one that is before the camera
      return (sideb ? -1 : 1); // if b is behind our back, a is first (a < b), otherwise b is first (a > b)
    }
    // the one that is closer to the view origin should come first
    const float distaSq = (ea->Origin-snfo->ViewOrg).length2DSquared();
    const float distbSq = (eb->Origin-snfo->ViewOrg).length2DSquared();
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
//  VNetConnection::CollectAndSortAliveThinkerChans
//
//==========================================================================
void VNetConnection::CollectAndSortAliveThinkerChans (ThinkerSortInfo *snfo) {
  AliveThinkerChans.reset();
  for (auto &&it : ThinkerChannels.first()) {
    VChannel *chan = it.getValue();
    if (!chan || !chan->IsThinker() || chan->Closing) continue;
    VThinkerChannel *tc = (VThinkerChannel *)chan;
    vassert(tc->GetThinker() == it.getKey());
    // never evict "always relevant" ones
    if (tc->GetThinker()->ThinkerFlags&VThinker::TF_AlwaysRelevant) continue;
    VEntity *ent = Cast<VEntity>(it.getKey());
    if (!ent) continue;
    // never evict players or our own MO
    if (ent == Owner->MO || ent->IsPlayer()) continue;
    AliveThinkerChans.append(tc->GetThinker());
  }
  static_assert(sizeof(AliveThinkerChans[0]) == sizeof(VThinker *), "wtf?!");
  // sort them
  timsort_r(AliveThinkerChans.ptr(), AliveThinkerChans.length(), sizeof(AliveThinkerChans[0]), &cmpPendingThinkers, (void *)snfo);
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

  ThinkerSortInfo snfo(Owner);

  // use counter trick to mark updated channels
  if ((++UpdateFrameCounter) == 0) {
    // reset all counters
    UpdateFrameCounter = 1;
    for (auto &&chan : OpenChannels) {
      if (chan->IsThinker()) ((VThinkerChannel *)chan)->LastUpdateFrame = 0;
    }
  }

  /*
   note: updating the closest object may not be the best strategy.
   if our channel is very close to the saturation, we may never update anything that
   is far away. basically, we'll be able to update only one or two thinkers, and each
   time the same. the game is barely playable in this case anyway, but it still may
   be better to "shuffle" things a little based on the last successfull update time.
   that is, we may put a "finger" with entity uid, and always start with it after
   the players and non-entities.
   or we can make LevelInfo the top priority, our MO is next (for predictors), then
   other players, then finger.
   */

  // send updates to the nearest objects first
  // collect all thinkers with channels in `PendingThinkers`, and sort
  // also, use finger to update the object
  vuint32 minUId = 0xffffffffu, nextUId = 0xffffffffu;
  for (auto &&it : ThinkerChannels.first()) {
    if (IsRelevant(it.getKey())) {
      const vuint32 currUId = it.getKey()->GetUniqueId();
      minUId = min2(minUId, currUId);
      // finger check
      VThinkerChannel *chan = it.getValue();
      if (currUId > UpdateFingerUId && nextUId > currUId) nextUId = currUId;
      if (UpdateFingerUId) {
        // update next uid
        if (UpdateFingerUId == currUId && chan->CanSendData()) {
          chan->Update();
          //GCon->Logf(NAME_DevNet, "%s: FINGER UPDATE", *chan->GetDebugName());
          continue;
        }
      }
      PendingThinkers.append(it.getKey());
    }
  }
  if (minUId == 0xffffffffu) minUId = 0;

  // move finger (if we have no next uid, use minimal)
  UpdateFingerUId = (nextUId != 0xffffffffu ? nextUId : minUId);

  // sort and update existing thinkers first
  if (PendingThinkers.length()) {
    timsort_r(PendingThinkers.ptr(), PendingThinkers.length(), sizeof(PendingThinkers[0]), &cmpPendingThinkers, (void *)&snfo);
    for (auto &&th : PendingThinkers) {
      VThinkerChannel *chan = ThinkerChannels.FindPtr(th);
      if (!chan) continue;
      if (chan->CanSendData()) {
        chan->Update();
        continue;
      }
    }
    // don't send them twice, lol
    PendingThinkers.reset();
  }

  // if we are starving on channels, don't try to add entities behind our back
  const bool starvingOnChannels = (OpenChannels.length() > MAX_CHANNELS-32);

  // update mobjs in sight
  for (TThinkerIterator<VThinker> th(Context->GetLevel()); th; ++th) {
    if (!IsRelevant(*th)) continue;
    VThinkerChannel *chan = ThinkerChannels.FindPtr(*th);
    if (!chan) {
      //HACK! add gore entities as last ones
      if (VStr::startsWith(th->GetClass()->GetName(), "K8Gore")) {
        vassert(th->GetClass()->IsChildOf(VEntity::StaticClass()));
        // if we are starving on channels, don't try to add entities behind our back
        if (starvingOnChannels) {
          VEntity *ent = (VEntity *)(*th);
          if (snfo.ViewPlane.PointOnSide(ent->Origin)) continue;
        }
        PendingGoreEnts.append((VEntity *)(*th));
        continue;
      }
      // if we are starving on channels, don't try to add entities behind our back
      if (starvingOnChannels && !IsAlwaysRelevant(*th)) {
        VEntity *ent = Cast<VEntity>(*th);
        if (ent) {
          if (snfo.ViewPlane.PointOnSide(ent->Origin)) continue;
        }
      }
      // not a gore, remember this thinker; we'll sort them later
      PendingThinkers.append(*th);
      continue;
    }
    // skip, if already updated
    if (chan->LastUpdateFrame == UpdateFrameCounter) continue;
    // update if we can, but still mark as updated if we cannot (so we won't drop this object)
    // TODO: replace unupdated object with new ones according to distance?
    if (chan->CanSendData()) {
      chan->Update();
    } else {
      // there's no need to mark for updates here, as client ack will do that for us
      //NeedsUpdate = true;
      chan->LastUpdateFrame = UpdateFrameCounter;
    }
  }

  // close entity channels that were not updated in this frame
  // WARNING! `Close()` should not delete a channel!
  int closedChanCount = 0; // counts closed thinker channels
  for (auto &&chan : OpenChannels) {
    if (chan->IsThinker()) {
      VThinkerChannel *tc = (VThinkerChannel *)chan;
      if (tc->LastUpdateFrame != UpdateFrameCounter) {
        if (!chan->Closing) {
          if (chan->CanSendClose()) {
            chan->Close();
          } else {
            // there's no need to mark for updates here, as client ack will do that for us
            //NeedsUpdate = true;
          }
        }
      }
      if (tc->Closing) ++closedChanCount;
      // remember alive gore entities
      if (!tc->Closing && tc->GetThinker() && VStr::startsWith(tc->GetThinker()->GetClass()->GetName(), "K8Gore")) {
        AliveGoreChans.append(chan->Index);
      }
    }
  }

  // if we have some pending thinkers, open channels for them
  if (PendingThinkers.length()) {
    static_assert(sizeof(PendingThinkers[0]) == sizeof(VThinker *), "wtf?!");
    // sort them
    timsort_r(PendingThinkers.ptr(), PendingThinkers.length(), sizeof(PendingThinkers[0]), &cmpPendingThinkers, (void *)&snfo);

    // if we have not enough free channels, remove gore entities (they will be removed in the future)
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
        vassert(tc->GetThinker() && tc->GetThinker()->GetClass()->IsChildOf(VEntity::StaticClass()));
        //PendingGoreEnts.append((VEntity *)(tc->GetThinker()));
        if (tc->CanSendClose()) {
          tc->Close();
        } else {
          // there's no need to mark for updates here, as client ack will do that for us
          //NeedsUpdate = true;
        }
        if (tc->Closing) ++closedChanCount;
      }
    }

    bool thinkChansReady = false;
    int evictCount = 0;

    // append thinkers (do not check network, we need to do this unconditionally)
    for (auto &&th : PendingThinkers) {
      VThinkerChannel *chan = (VThinkerChannel *)CreateChannel(CHANNEL_Thinker, -1, true); // local channel
      if (!chan) {
        /*
         we have no room for new thinkers. this means that the map is *VERY* crowded (like nuts.wad ;-).
         now, check and close any channels further than the current pending thinker, so next update will be able
         to add our surroundings.
         there's no need to mark for updates here, as client ack will do that for us.
         */
        if (!thinkChansReady) {
          thinkChansReady = true;
          CollectAndSortAliveThinkerChans(&snfo);
        }
        if (AliveThinkerChans.length() == 0) break; // nobody to evict anymore
        // if this is "always relevant" thinker, do no more checks
        if (!(th->ThinkerFlags&VThinker::TF_AlwaysRelevant)) {
          // only for entities; it is guaranteed that all "always relevant" thinkers are already processed
          // also, it is guaranteed that entities are sorted by the distance
          VEntity *ent = Cast<VEntity>(th);
          if (!ent) continue;
          // do not try to stuff in entities that are behind the camera
          // this is not the best way to do it, but we are heavily starving on channels
          if (snfo.ViewPlane.PointOnSide(ent->Origin)) {
            // everything else should be behind our back, ignore
            /*
            int n = 0;
            while (PendingThinkers[n] != th) ++n;
            GCon->Logf(NAME_DevNet, "%s: stopped eviction at %d of %d due to back culling", *GetAddress(), n, PendingThinkers.length());
            */
            break;
          }
          const float maxDistSq = (ent->Origin-Owner->ViewOrg).length2DSquared();
          // evict something that is far away, to make room for this one
          ent = Cast<VEntity>(AliveThinkerChans[AliveThinkerChans.length()-1]);
          vassert(ent);
          const float distSq = (ent->Origin-Owner->ViewOrg).length2DSquared();
          // if this one is further than the nearest one, stop
          if (distSq <= maxDistSq) break;
        }
        // evict furthest thinker
        VThinkerChannel *tc = ThinkerChannels.FindPtr(AliveThinkerChans[AliveThinkerChans.length()-1]);
        vassert(tc);
        tc->Close();
        AliveThinkerChans.removeAt(AliveThinkerChans.length()-1);
        ++evictCount;
        continue;
      }
      chan->SetThinker(th);
      chan->Update();
    }

    if (evictCount) GCon->Logf(NAME_DevNet, "%s: evicted %d thinker channels", *GetAddress(), evictCount);
  }

  // append gore entities if we have any free slots
  if (PendingGoreEnts.length() && MAX_CHANNELS-OpenChannels.length() > 0) {
    // sort them
    timsort_r(PendingGoreEnts.ptr(), PendingGoreEnts.length(), sizeof(PendingGoreEnts[0]), &cmpPendingGoreEnts, (void *)&snfo);
    for (auto &&it : PendingGoreEnts) {
      if (!CanSendData()) {
        // there's no need to mark for updates here, as client ack will do that for us
        //NeedsUpdate = true;
        break;
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

  bool wasAtLeastOneUpdate = false;

  const double ctt = Sys_Time();

  // update level
  if (LastLevelUpdateTime <= ctt) {
    LastLevelUpdateTime = ctt+1.0/(double)clampval(sv_fps.asFloat()*2.0f, 5.0f, 70.0f);
    //const bool oldUpdateFlag = NeedsUpdate;
    NeedsUpdate = false; // note that we already sent an update
    SetupFatPVS();
    GetLevelChannel()->Update();
    wasAtLeastOneUpdate = true;
  }

  if (LastThinkersUpdateTime <= ctt) {
    LastThinkersUpdateTime = ctt+1.0/(double)clampval(sv_fps.asFloat(), 5.0f, 70.0f);
    NeedsUpdate = false; // note that we already sent an update
    if (!wasAtLeastOneUpdate) SetupFatPVS();
    UpdateThinkers();
  }

  /*
  NeedsUpdate = false; // note that we already sent an update
  SetupFatPVS();
  GetLevelChannel()->Update();
  UpdateThinkers();
  */

  //GCon->Logf(NAME_DevNet, "%s: *** UpdatateLevel done, %d active channels", *GetAddress(), OpenChannels.length());
}


//==========================================================================
//
//  VNetConnection::SendServerInfo
//
//==========================================================================
void VNetConnection::SendServerInfo () {
  if (!ObjMapSent) return;

  // do not send server info if we are in the intermission (let client hang)
  if (sv.intermission) return;

  if (GetLevelChannel()->Level != GLevel) {
    GCon->Logf(NAME_DevNet, "Preparing level for %s", *GetAddress());
    GetLevelChannel()->SetLevel(GLevel);
    LevelInfoSent = LNFO_UNSENT;
  }

  if (LevelInfoSent == LNFO_SENT_COMPLETE) return;

  GCon->Logf(NAME_DevNet, "Sending server info for %s", *GetAddress());
  // this will load level on client side
  LevelInfoSent = (GetLevelChannel()->SendLevelData() ? LNFO_SENT_INPROGRESS : LNFO_SENT_COMPLETE);
}


//==========================================================================
//
//  VNetConnection::LoadedNewLevel
//
//  always followed by `SendServerInfo()`
//
//==========================================================================
void VNetConnection::LoadedNewLevel () {
  GCon->Log(NAME_DevNet, "LEVEL RESET");
  LevelInfoSent = LNFO_UNSENT;
  GetLevelChannel()->SetLevel(GLevel);
}


//==========================================================================
//
//  VNetConnection::ResetLevel
//
//==========================================================================
void VNetConnection::ResetLevel () {
  if (!GetLevelChannel()->Level) return;
  // close entity channels
  for (int f = OpenChannels.length()-1; f >= 0; --f) {
    VChannel *chan = OpenChannels[f];
    if (!chan) { OpenChannels.removeAt(f); continue; }
    if (chan->IsThinker()) {
      chan->Close(); //k8: should we close it, or we can simply kill it?
      /*
      // we're going to destroy the level anyway, so just drop all actors, nobody cares
      chan->Closing = true;
      chan->Connection = nullptr;
      delete chan;
      OpenChannels.removeAt(f);
      continue;
      */
    }
  }
  //ThinkerChannels.reset();
  GetLevelChannel()->ResetLevel();
  GetPlayerChannel()->ResetLevel();
}
