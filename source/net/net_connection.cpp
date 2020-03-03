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


static VCvarF net_test_loss("net_test_loss", "0", "Emulated packet loss percentage (randomly skip sending some packets).", CVAR_PreInit);
static VCvarB net_dbg_conn_show_outdated("net_dbg_conn_show_outdated", false, "Show outdated channel messages?");
static VCvarB net_dbg_conn_show_dgrams("net_dbg_conn_show_dgrams", false, "Show datagram activity?");
static VCvarB net_dbg_report_missing_dgrams("net_dbg_report_missing_dgrams", false, "Report missing datagrams (this is mostly useless console spam)?");

VCvarB net_debug_dump_recv_packets("net_debug_dump_recv_packets", false, "Dump received packets?");


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
  , State(NETCON_Open)
  , LastSendTime(0)
  , NextUpdateTimeThinkers(0)
  , NextUpdateTimeLevel(0)
  , NeedsUpdate(false)
  , AutoAck(false)
  , Out(MAX_MSG_SIZE_BITS+16)
  , ObjMapSent(false)
  , LevelInfoSent(false)
  , UpdatePvs(nullptr)
  , UpdatePvsSize(0)
  , LeafPvs(nullptr)
{
  //memset(Channels, 0, sizeof(Channels));
  //vassert(OpenChannels.length() == 0);
  for (unsigned f = 0; f < (unsigned)CHANIDX_ThinkersStart; ++f) KnownChannels[f] = nullptr;
  memset(ChanFreeBitmap, 0, ((MAX_CHANNELS+31)/32)*4);
  vassert(ChanIdxMap.length() == 0);
  HasDeadChannels = false;

  // fill free thinker ids
  memset(ChanFreeIds, 0, sizeof(ChanFreeIds));
  ChanFreeIdsUsed = MAX_CHANNELS-CHANIDX_ThinkersStart;
  for (int f = 0; f < (int)ChanFreeIdsUsed; ++f) ChanFreeIds[f] = f+CHANIDX_ThinkersStart;
  // no need to shuffle it, our allocator will take care of that

  sendQueueHead = sendQueueTail = nullptr;

  relSendDataSize = 0; // no reliable message yet

  incoming_sequence = 0;
  incoming_acknowledged = 0;
  incoming_reliable_acknowledged = 0;
  incoming_reliable_sequence = 0;

  outgoing_sequence = 0;
  reliable_sequence = 0;
  last_reliable_sequence = 0;
  AllowMessageSend = false;

  Out.Reinit(MAX_MSG_SIZE_BITS+16);

  ObjMap = new VNetObjectsMap(this);

  CreateChannel(CHANNEL_Control, CHANIDX_General);
  CreateChannel(CHANNEL_Player, CHANIDX_Player);
  CreateChannel(CHANNEL_Level, CHANIDX_Level);
}


//==========================================================================
//
//  VNetConnection::~VNetConnection
//
//==========================================================================
VNetConnection::~VNetConnection () {
  GCon->Logf(NAME_DevNet, "Deleting connection to %s", *GetAddress());
  // remove all open channels
  while (ChanIdxMap.length()) {
    VChannel *chan = ChanIdxMap.first().getValue();
    vassert(chan);
    chan->Suicide(); // don't send any messages (just in case)
    delete chan;
  }
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
//  VNetConnection::GetRandomThinkerChannelId
//
//==========================================================================
int VNetConnection::GetRandomThinkerChannelId () {
  vassert(ChanFreeIdsUsed > 0);
  // select random element; swap it with the last one; shrink array
  if (ChanFreeIdsUsed > 1) {
    const unsigned idx = GenRandomU31()%ChanFreeIdsUsed;
    // swap idx and the last one
    if (idx != ChanFreeIdsUsed-1) {
      const int tmp = ChanFreeIds[idx];
      ChanFreeIds[idx] = ChanFreeIds[ChanFreeIdsUsed-1];
      ChanFreeIds[ChanFreeIdsUsed-1] = tmp;
    }
  }
  // return the last one
  int res = ChanFreeIds[--ChanFreeIdsUsed];
  //GCon->Logf(NAME_Debug, "*** ALLOCATED THID %d (%u thids left)", res, ChanFreeIdsUsed);
  vassert(ChanFreeIdsUsed >= 0);
  vassert(res >= CHANIDX_ThinkersStart && res < MAX_CHANNELS);
  return res;
}


//==========================================================================
//
//  VNetConnection::ReleaseThinkerChannelId
//
//==========================================================================
void VNetConnection::ReleaseThinkerChannelId (int idx) {
  //GCon->Logf(NAME_Debug, "*** FREEING THID %d (%u thids in pool)", idx, ChanFreeIdsUsed);
  vassert(idx >= CHANIDX_ThinkersStart && idx < MAX_CHANNELS);
  vassert(ChanFreeIdsUsed < (unsigned)(MAX_CHANNELS-CHANIDX_ThinkersStart));
  // select random element; remember its value, and replace it with idx; append replaced value to the array
  if (ChanFreeIdsUsed > 1) {
    const unsigned arridx = GenRandomU31()%ChanFreeIdsUsed;
    const int tmp = ChanFreeIds[arridx];
    ChanFreeIds[arridx] = idx;
    idx = tmp;
  }
  ChanFreeIds[ChanFreeIdsUsed++] = idx;
}


//==========================================================================
//
//  VNetConnection::AllocThinkerChannelId
//
//  can return -1 if there are no free thinker channels
//
//==========================================================================
int VNetConnection::AllocThinkerChannelId () {
  // early exit
  if (ChanIdxMap.length() >= MAX_CHANNELS) return -1;
  if (ChanFreeIdsUsed == 0) return -1; // just in case
  const int idx = GetRandomThinkerChannelId();
  if (idx < 0) return -1; // just in case
  return idx;
  /* old code, sequential
  // ok, we may have some free channels, look for a free one in the bitmap
  const uint32_t oldbmp0 = ChanFreeBitmap[0];
  ChanFreeBitmap[0] |= CHANIDX_KnownBmpMask; // temporarily
  for (unsigned idx = 0; idx < (MAX_CHANNELS+31)/32; ++idx) {
    const uint32_t slot = ChanFreeBitmap[idx];
    if (slot != 0xffffffffu) {
      // we have a free chan in this slot
      for (unsigned ofs = 0; ofs < 32; ++ofs) {
        if (!(slot&(1u<<ofs))) {
          ChanFreeBitmap[0] = oldbmp0;
          return (int)(idx*32u+ofs);
        }
      }
      abort(); // the thing that should not be
    }
  }
  ChanFreeBitmap[0] = oldbmp0;
  return -1; // no free slots
  */
}


//==========================================================================
//
//  VNetConnection::RegisterChannel
//
//==========================================================================
void VNetConnection::RegisterChannel (VChannel *chan) {
  if (!chan) return;
  const vint32 idx = chan->Index;
  vassert(idx >= 0 && idx < MAX_CHANNELS);
  const unsigned bmpIdx = ((unsigned)idx)/32u;
  const unsigned bmpOfs = ((unsigned)idx)&0x1fu;
  if (ChanFreeBitmap[bmpIdx]&(1u<<bmpOfs)) Sys_Error("trying to register already registered channel %d", idx);
  ChanFreeBitmap[bmpIdx] |= 1u<<bmpOfs;
  if (idx < CHANIDX_ThinkersStart) {
    if (KnownChannels[idx]) Sys_Error("trying to register already registered known channel %d", idx);
    KnownChannels[idx] = chan;
  }
  ChanIdxMap.put(idx, chan);
  //!GCon->Logf(NAME_DevNet, "   VNetConnection (%s): registered channel %d (%s) ('%s')", *GetAddress(), chan->Index, chan->GetTypeName(), *shitppTypeNameObj(*chan));
}


//==========================================================================
//
//  VNetConnection::UnregisterChannel
//
//==========================================================================
void VNetConnection::UnregisterChannel (VChannel *chan, bool touchMap) {
  if (!chan) return;
  const vint32 idx = chan->Index;
  vassert(idx >= 0 && idx < MAX_CHANNELS);
  const unsigned bmpIdx = ((unsigned)idx)/32u;
  const unsigned bmpOfs = ((unsigned)idx)&0x1fu;
  if (!(ChanFreeBitmap[bmpIdx]&(1u<<bmpOfs))) Sys_Error("trying to unregister non-registered channel %d", idx);
  ChanFreeBitmap[bmpIdx] &= ~(1u<<bmpOfs);
  if (idx < CHANIDX_ThinkersStart) {
    if (!KnownChannels[idx]) Sys_Error("trying to unregister non-registered known channel %d", idx);
    KnownChannels[idx] = nullptr;
  } else {
    ReleaseThinkerChannelId(idx);
  }
  if (touchMap) ChanIdxMap.remove(idx);
  // for thinker channel, remove it from thinker map (and from the game)
  if (chan->IsThinker()) {
    vassert(chan->Closing);
    VThinkerChannel *tc = (VThinkerChannel *)chan;
    //tc->SetThinker(nullptr);
    tc->RemoveThinkerFromGame();
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
  if (Index == -1) {
    if (Type == CHANNEL_ObjectMap) {
      vassert(KnownChannels[CHANIDX_ObjectMap] == nullptr);
      Index = CHANIDX_ObjectMap;
    } else {
      vassert(Type == CHANNEL_Thinker);
      Index = AllocThinkerChannelId();
      if (Index < 0) return nullptr;
      vassert(Index >= CHANIDX_ThinkersStart && Index < MAX_CHANNELS);
    }
  } else if (Type == CHANIDX_ThinkersStart) {
    // this can happen in client (server requested channel id)
    //FIXME: make this faster!
    //GCon->Logf(NAME_Debug, "trying to allocate fixed thinker channel #%d", Index);
    unsigned xidx = 0;
    while (xidx < ChanFreeIdsUsed && ChanFreeIds[xidx] != Index) ++xidx;
    if (xidx >= ChanFreeIdsUsed) Sys_Error("trying to allocate already allocated fixed thinker channel #%d", Index);
    // swap with last element
    if (xidx != ChanFreeIdsUsed-1) {
      const int tmp = ChanFreeIds[xidx];
      ChanFreeIds[xidx] = ChanFreeIds[ChanFreeIdsUsed-1];
      ChanFreeIds[ChanFreeIdsUsed-1] = tmp;
    }
    vassert(ChanFreeIds[ChanFreeIdsUsed-1] == Index);
    --ChanFreeIdsUsed;
  }
  vassert(Index >= 0 && Index < MAX_CHANNELS);

  switch (Type) {
    case CHANNEL_Control: vassert(Index == CHANIDX_General); return new VControlChannel(this, Index, OpenedLocally);
    case CHANNEL_Level: vassert(Index == CHANIDX_Level); return new VLevelChannel(this, Index, OpenedLocally);
    case CHANNEL_Player: vassert(Index == CHANIDX_Player); return new VPlayerChannel(this, Index, OpenedLocally);
    case CHANNEL_Thinker: vassert(Index >= CHANIDX_ThinkersStart); return new VThinkerChannel(this, Index, OpenedLocally);
    case CHANNEL_ObjectMap: vassert(Index == CHANIDX_ObjectMap); return new VObjectMapChannel(this, Index, OpenedLocally);
    default: GCon->Logf(NAME_DevNet, "Unknown channel type %d for channel %d", Type, Index); return nullptr;
  }
}


//==========================================================================
//
//  VNetConnection::ProcessSendQueue
//
//==========================================================================
void VNetConnection::ProcessSendQueue () {
  if (IsServer()) {
    if (!AllowMessageSend) return; // got nothing from client, don't send anything back yet
    AllowMessageSend = false;
  }

  // if the remote side dropped the last reliable message, resend it
  vuint32 send_reliable = (incoming_acknowledged > last_reliable_sequence && incoming_reliable_acknowledged != reliable_sequence);

  // if the reliable transmit buffer is empty, copy the current message out
  if (relSendDataSize == 0 && sendQueueHead) {
    // invariants
    vassert(sendQueueHead);
    vassert(sendQueueTail);
    QMessage *qm = sendQueueHead;
    vassert(qm->dataSize);
    vassert(qm->dataSize+MSG_HEADER_SIZE <= MAX_DGRAM_SIZE);
    memcpy(relSendData, qm->data, qm->dataSize);
    relSendDataSize = qm->dataSize;
    reliable_sequence ^= 1;
    send_reliable = 1;
    // pop queue head (the data will be in `relSendData` from the now on)
    sendQueueHead = qm->next;
    if (!sendQueueHead) { vassert(sendQueueTail); vassert(!sendQueueTail->next); sendQueueTail = nullptr; }
    delete qm;
    vassert(sendQueueSize > 0);
    --sendQueueSize;
    if (net_dbg_conn_show_dgrams) GCon->Logf(NAME_DevNet, "%s: took new reliable datagram from queue (%d bytes); %d left in queue", *GetAddress(), relSendDataSize, sendQueueSize);
  }

  vuint32 sendDataSize; // put it here, so it won't be overwritten accidentally (and i hope the compiler won't move it)
  vuint8 sendData[MAX_DGRAM_SIZE];

  // write the packet header
  vuint32 *w1 = (vuint32 *)(sendData+0);
  vuint32 *w2 = (vuint32 *)(sendData+4);
  // data starts at `sendData+8`
  static_assert(MSG_HEADER_SIZE == 8, "invalid MSG_HEADER_SIZE");

  *w1 = outgoing_sequence|(send_reliable<<31);
  *w2 = incoming_sequence|(incoming_reliable_sequence<<31);

  ++outgoing_sequence;

  // copy the reliable message to the packet (if there is any)
  sendDataSize = MSG_HEADER_SIZE;

  if (send_reliable) {
    if (relSendDataSize) {
      sendData[sendDataSize++] = NETPACKET_DATA; // packet type
      memcpy(sendData+sendDataSize, relSendData, relSendDataSize);
      sendDataSize += relSendDataSize;
      vassert(sendDataSize <= MAX_DGRAM_SIZE);
    }
    last_reliable_sequence = outgoing_sequence;
    if (net_dbg_conn_show_dgrams) GCon->Logf(NAME_DevNet, "%s: sending reliable datagram (%d bytes; packet: %d bytes); w1=%08x; w2=%08x", *GetAddress(), relSendDataSize, sendDataSize, *w1, *w2);
  } else {
    if (net_dbg_conn_show_dgrams) GCon->Logf(NAME_DevNet, "%s: sending empty datagram (%d bytes); w1=%08x; w2=%08x", *GetAddress(), sendDataSize, *w1, *w2);
  }

  // send the message
  const float lossPrc = net_test_loss.asFloat();
  if (lossPrc <= 0.0f || RandomFull()*100.0f >= net_test_loss) {
    if (NetCon->SendMessage(sendData, sendDataSize) == -1) {
      State = NETCON_Closed;
      return;
    }
  }

  LastSendTime = Driver->NetTime;

  if (!IsLocalConnection()) ++Driver->MessagesSent;
  ++Driver->packetsSent;
}


//==========================================================================
//
//  VNetConnection::ProcessRecvQueue
//
//  returns `true` if message was received
//
//==========================================================================
bool VNetConnection::ProcessRecvQueue (VBitStreamReader &Packet) {
  // check for message arrival
  vassert(NetCon);
  TArray<vuint8> msgdata;
  int res = NetCon->GetMessage(msgdata);
  if (res == 0) return false;
  if (res < 0) { State = NETCON_Closed; return false; }

  if (msgdata.length() < MSG_HEADER_SIZE) {
    GCon->Logf(NAME_DevNet, ">>> got too small packet from %s", *GetAddress());
    return false;
  }

  if (msgdata.length() > MAX_DGRAM_SIZE) {
    GCon->Logf(NAME_DevNet, ">>> got too big packet from %s", *GetAddress());
    return false;
  }

  // copy data to recv buffer
  vuint8 *recvData = msgdata.ptr();
  int recvDataSize = msgdata.length();

  // get sequence numbers
  vuint32 sequence = *(const vuint32 *)(recvData+0);
  vuint32 sequence_ack = *(const vuint32 *)(recvData+4);
  // data starts at `sendData+8`
  static_assert(MSG_HEADER_SIZE == 8, "invalid MSG_HEADER_SIZE");

  if (net_dbg_conn_show_dgrams) GCon->Logf(NAME_DevNet, "%s: got datagram with %d bytes of payload; w1=%08x; w2=%08x", *GetAddress(), recvDataSize, sequence, sequence_ack);

  vuint32 reliable_message = sequence>>31;
  vuint32 reliable_ack = sequence_ack>>31;

  sequence &= ~(1u<<31);
  sequence_ack &= ~(1u<<31);

  // discard stale or duplicated packets
  if (sequence <= incoming_sequence) {
    //++Driver->droppedDatagrams;
    GCon->Logf(NAME_DevNet, "Got stale datagram (urseq=%u; seq=%u)", incoming_sequence, sequence);
    return false;
  }

  // received something
  NetCon->LastMessageTime = Driver->NetTime;
  ++Driver->UnreliableMessagesReceived;

  // dropped packets don't keep the message from being used
  int ndrop = (int)(sequence-(incoming_sequence+1));
  // yeah, record it
  if (ndrop) {
    vassert(ndrop > 0);
    // this datagram is in the future, looks like older datagrams are lost
    Driver->droppedDatagrams += ndrop;
    if (net_dbg_report_missing_dgrams) GCon->Logf(NAME_DevNet, "Missing %d datagram%s (urseq=%u; seq=%u)", ndrop, (ndrop != 1 ? "s" : ""), incoming_sequence, sequence);
  }

  if (net_dbg_conn_show_dgrams) {
    GCon->Logf(NAME_DevNet, "%s: got datagram with %d bytes of data (urseq=%u; seq=%u; urseqA=%u; seqA=%u(%u); rmsg=%u)", *GetAddress(), recvDataSize,
      incoming_sequence, sequence, incoming_acknowledged, sequence_ack, reliable_ack, reliable_message);
  }

  // if the current outgoing reliable message has been acknowledged
  // clear the buffer to make way for the next
  if (reliable_ack == reliable_sequence) relSendDataSize = 0; // it has been received

  // if this message contains a reliable message, bump incoming reliable sequence
  incoming_sequence = sequence;
  incoming_acknowledged = sequence_ack;
  incoming_reliable_acknowledged = reliable_ack;
  if (reliable_message) incoming_reliable_sequence ^= 1;

  // copy received data to packet stream
  Packet.Clear();
  // nope, this will be set by the server code
  //NeedsUpdate = true; // we got *any* activity, update the world!
  if (recvDataSize > MSG_HEADER_SIZE) {
    // check packet type
    if (recvData[MSG_HEADER_SIZE] != NETPACKET_DATA) {
      GCon->Logf(NAME_DevNet, "%s: datagram packet has invalid type; expected 0x%02x, got 0x%02x", *GetAddress(), NETPACKET_DATA, recvData[MSG_HEADER_SIZE]);
      return false;
    }
    int length = (int)((recvDataSize-MSG_HEADER_SIZE-1)*8);
    vassert(length >= 0);
    if (length == 0) {
      GCon->Log(NAME_DevNet, "%s: datagram packet has no data; this should not happen!");
      return false;
    }
    Packet.SetupFrom(recvData+MSG_HEADER_SIZE+1, length, true); // fix the length with the trailing bit
    if (Packet.IsError()) {
      GCon->Logf(NAME_DevNet, "%s: datagram packet is missing trailing bit", *GetAddress());
      return false;
    }
    if (net_dbg_conn_show_dgrams) GCon->Logf(NAME_DevNet, "%s: got datagram with a packet (%d bits of data)", *GetAddress(), Packet.GetNumBits());
  }

  return true;
}


//==========================================================================
//
//  VNetConnection::GetMessages
//
//==========================================================================
void VNetConnection::GetMessages () {
  Driver->SetNetTime();
  if (State == NETCON_Closed) return;

  // check timeout
  if (IsTimeoutExceeded()) {
    ShowTimeoutStats();
    State = NETCON_Closed;
    return;
  }

  VBitStreamReader Packet;
  if (!ProcessRecvQueue(Packet)) return;
  // nope, this will be set by the server code
  //NeedsUpdate = true; // we got *any* activity, update the world! (valid for server)
  AllowMessageSend = true; // allow sending client messages (valid for server)
  if (IsServer()) {
    // make sure the reply sequence number matches the incoming sequence number
    if (incoming_sequence >= outgoing_sequence) {
      outgoing_sequence = incoming_sequence;
    } else {
      AllowMessageSend = false; // don't reply, sequences have slipped
    }
  }
  if (Packet.GetNumBits()) ReceivedPacket(Packet);
}


//==========================================================================
//
//  VNetConnection::GetRawPacket
//
//  used in demos
//
//==========================================================================
int VNetConnection::GetRawPacket (TArray<vuint8> &Data) {
  vensure(NetCon);
  return NetCon->GetMessage(Data);
}


//==========================================================================
//
//  VNetConnection::RemoveDeadThinkerChannels
//
//==========================================================================
void VNetConnection::RemoveDeadThinkerChannels (bool resetUpdated) {
  HasDeadChannels = false;
  auto it = ChanIdxMap.first();
  while (it) {
    VChannel *chan = it.getValue();
    vassert(chan);
    if (chan->IsDead()) {
      // this channel is closed, and should be removed
      if (chan->Index >= 0) UnregisterChannel(chan, false); // leave it in hash
      chan->Index = -666; // channel should not unregister itself, we'll take care of it
      delete chan;
      it.removeCurrent();
      continue;
    }
    if (resetUpdated && chan->IsThinker()) {
      ((VThinkerChannel *)chan)->UpdatedThisFrame = false;
    }
    ++it;
  }
}


//==========================================================================
//
//  VNetConnection::ReceivedPacket
//
//==========================================================================
void VNetConnection::ReceivedPacket (VBitStreamReader &Packet) {
  /*
  vuint8 packetType = 0;
  Packet << packetType;
  if (packetType != NETPACKET_DATA) {
    GCon->Logf(NAME_DevNet, "%s: got invalid packet with type 0x%02x (expected 0x%02x)", *GetAddress(), packetType, NETPACKET_DATA);
    return;
  }
  */
  ++Driver->packetsReceived;

  if (net_debug_dump_recv_packets) GCon->Logf(NAME_DevNet, "***!!!*** Network Packet (pos=%d; num=%d (%d))", Packet.GetPos(), Packet.GetNum(), (Packet.GetNum()+7)/8);
  while (!Packet.AtEnd()) {
    if (net_debug_dump_recv_packets) GCon->Logf(NAME_DevNet, "  parsing packet: %d bits eaten of %d", Packet.GetPos(), Packet.GetNumBits());
    // read message header
    VMessageIn Msg(Packet);
    if (Packet.IsError() || Msg.IsError()) {
      GCon->Logf(NAME_DevNet, "Packet is missing message header");
      State = NETCON_Closed;
      return;
    }
    if (net_debug_dump_recv_packets) GCon->Logf(NAME_DevNet, "  parsed packet message: %d bits eaten of %d", Packet.GetPos(), Packet.GetNumBits());

    if (net_debug_dump_recv_packets) {
      GCon->Logf(NAME_DevNet, "  packet (channel %d; open=%d; close=%d; chantype=%d) (len=%d; pos=%d; num=%d; left=%d)",
        Msg.ChanIndex, (int)Msg.bOpen, (int)Msg.bClose, (int)Msg.ChanType,
        Msg.GetNumBits(), Packet.GetPos(), Packet.GetNum(), Packet.GetNum()-Packet.GetPos());
    }

    VChannel *Chan = GetChannelByIndex(Msg.ChanIndex);
    if (!Chan) {
      // possible new channel
      if (Msg.bOpen) {
        // client cannot create thinkers on the server (safeguard)
        if (Msg.ChanType == CHANNEL_Thinker && IsServer()) {
          GCon->Logf(NAME_DevNet, "%s: client requested thinker creation, disconnecting possibly evil client", *GetAddress());
          State = NETCON_Closed;
          return;
        }
        //TODO: check for invalid index too
        // channel opening message, do it
        Chan = CreateChannel(Msg.ChanType, Msg.ChanIndex, false);
        if (!Chan) {
          GCon->Logf(NAME_DevNet, "  !!! CANNOT create new channel #%d (%s)", Msg.ChanIndex, VChannel::GetChanTypeName(Msg.ChanType));
          continue;
        }
        Chan->OpenAcked = true;
        //!GCon->Logf(NAME_DevNet, "  !!! created new channel #%d (%s)", Chan->Index, Chan->GetTypeName());
        // immediately send "channel open ack" message (the channel itself should do it, but...)
        {
          if (net_debug_dump_recv_packets) GCon->Logf(NAME_DevNet, "%s: sending OPEN ACK for %s", *GetAddress(), *Chan->GetDebugName());
          VMessageOut ackmsg(Chan, VMessageOut::Open);
          SendMessage(ackmsg);
        }
      } else {
        // ignore messages for unopened channels
        if (Msg.ChanIndex < 0 || Msg.ChanIndex >= MAX_CHANNELS) {
          GCon->Logf(NAME_DevNet, "Ignored message for invalid channel %d", Msg.ChanIndex);
        } else {
          GCon->Logf(NAME_DevNet, "Message for closed channel %d", Msg.ChanIndex);
        }
        continue;
      }
    } else {
      // ack channel open for local channels
      if (Msg.bOpen && Chan->OpenedLocally) {
        //!GCon->Logf(NAME_DevNet, "%s: got OPEN ACK for %s", *GetAddress(), *Chan->GetDebugName());
        Chan->OpenAcked = true;
      }
    }
    Chan->ReceivedRawMessage(Msg);
  }
}


//==========================================================================
//
//  CalcFullMsgBitSize
//
//==========================================================================
static inline int CalcFullMsgBitSize (const VBitStreamWriter &strm, int addnum=0) {
  return VMessageOut::CalcFullMsgBitSize(strm.GetNumBits()+addnum);
}


//==========================================================================
//
//  VNetConnection::PutOutToSendQueue
//
//==========================================================================
void VNetConnection::PutOutToSendQueue () {
  if (Out.GetNumBits() == 0) return; // nothing to do
  if (CalcFullMsgBitSize(Out) > MAX_MSG_SIZE_BITS) {
    Sys_Error("outgoing message too long: %d bits, but only %d bits allowed (psq)", CalcFullMsgBitSize(Out), MAX_MSG_SIZE_BITS);
  }
  //!GCon->Logf(NAME_DevNet, "%s: PutOutToSendQueue:000: qlen=%d; outlen=%d bits", *GetAddress(), sendQueueSize, Out.GetNumBits());
  // add trailing bit so we can find out how many bits the message has
  Out.WriteTrailingBit();
  vassert(Out.GetNumBits() <= MAX_MSG_SIZE_BITS);
  //!GCon->Logf(NAME_DevNet, "%s: PutOutToSendQueue:001: qlen=%d; outlen=%d bits", *GetAddress(), sendQueueSize, Out.GetNumBits());
  // create new message
  QMessage *qm = new QMessage;
  qm->dataSize = 0;
  qm->next = nullptr;
  // copy data
  vassert(Out.GetNumBytes() > 0 && Out.GetNumBytes() <= MAX_MSG_SIZE_BITS/8);
  memcpy(qm->data, Out.GetData(), Out.GetNumBytes());
  qm->dataSize = (unsigned)Out.GetNumBytes();
  Out.Reinit(MAX_MSG_SIZE_BITS+16);
  vassert(Out.GetNumBits() == 0);
  // append data to queue
  if (sendQueueTail) sendQueueTail->next = qm; else sendQueueHead = qm;
  sendQueueTail = qm;
  ++sendQueueSize;
  //!GCon->Logf(NAME_DevNet, "%s: PutOutToSendQueue:002: qlen=%d; outlen=%d bits", *GetAddress(), sendQueueSize, Out.GetNumBits());
}


//==========================================================================
//
//  VNetConnection::SendMessage
//
//  actually, add message to send queue
//
//==========================================================================
void VNetConnection::SendMessage (VMessageOut &msg) {
  if (msg.CalcRealMsgBitSize() > MAX_MSG_SIZE_BITS) {
    Sys_Error("%s: outgoing message too long: %d bits, but only %d bits allowed (smg)", *GetAddress(), msg.CalcRealMsgBitSize(), MAX_MSG_SIZE_BITS);
  }

  // do we need to allocate a new message?
  int newsz = msg.CalcRealMsgBitSize(Out);
  if (newsz > MAX_MSG_SIZE_BITS) {
    if (net_dbg_conn_show_dgrams) GCon->Logf(NAME_DevNet, "%s: pushed current output buffer to queue; out=%d; msg=%d", *GetAddress(), Out.GetNumBytes(), msg.GetNumBytes());
    PutOutToSendQueue();
  }

  vassert(msg.CalcRealMsgBitSize(Out) <= MAX_MSG_SIZE_BITS);
  msg.Finalise();

  if (net_dbg_conn_show_dgrams) GCon->Logf(NAME_DevNet, "%s: saving message to outbuf; out=%d; msg=%d", *GetAddress(), Out.GetNumBytes(), msg.GetNumBytes());
  Out.SerialiseBits(msg.GetData(), msg.GetNumBits());
  if (net_dbg_conn_show_dgrams) GCon->Logf(NAME_DevNet, "%s: saved message to outbuf; out=%d; msg=%d", *GetAddress(), Out.GetNumBytes(), msg.GetNumBytes());
}


//==========================================================================
//
//  VNetConnection::Flush
//
//==========================================================================
void VNetConnection::Flush (bool asKeepalive) {
  Driver->SetNetTime();
  if (State == NETCON_Closed) return;

  // put current queued data to send queue
  PutOutToSendQueue();

  // create keepalive packet if send queue is empty
  if (!sendQueueHead && asKeepalive) {
    // do not sent keepalives for autoack (demos) and local connections, nobody cares
    // `AutoAck == true` means "demo recording"
    if (AutoAck || IsLocalConnection()) return;
    double tout = VNetworkPublic::MessageTimeOut;
    if (tout < 0.02) tout = 0.02;
    if (Driver->NetTime-LastSendTime < tout/4.0f) return;
    // it is ok to send a keepalive; it will contain no data
  }

  ProcessSendQueue();
}


//==========================================================================
//
//  VNetConnection::IsLocalConnection
//
//==========================================================================
bool VNetConnection::IsLocalConnection () {
  // for demo playback NetCon can be nullptr.
  return (NetCon ? NetCon->IsLocalConnection() : true);
}


//==========================================================================
//
//  VNetConnection::IsTimeoutExceeded
//
//==========================================================================
bool VNetConnection::IsTimeoutExceeded () {
  if (State == NETCON_Closed) return false; // no wai
  // for bots and demo playback there's no other end that will send us the ACK
  // so there's no need to check for timeouts
  // `AutoAck == true` means "demo recording"
  if (AutoAck || IsLocalConnection()) return false;
  if (!GetLevelChannel()->Level) {
    // wtf?!
    NetCon->LastMessageTime = Driver->NetTime;
    return false;
  }
  double tout = VNetworkPublic::MessageTimeOut;
  if (tout < 0.02) tout = 0.02;
  if (Driver->NetTime-NetCon->LastMessageTime <= tout) return false;
  // timeout!
  return true;
}


//==========================================================================
//
//  VNetConnection::ShowTimeoutStats
//
//==========================================================================
void VNetConnection::ShowTimeoutStats () {
  if (State == NETCON_Closed) return;
  GCon->Logf(NAME_DevNet, "ERROR: Channel timed out; time delta=%g; sent %d messages (%d packets), received %d messages (%d packets)",
    (Driver->NetTime-NetCon->LastMessageTime)*1000.0f,
    Driver->MessagesSent, Driver->packetsSent,
    Driver->MessagesReceived, Driver->packetsReceived);
  /*
  if (GetGeneralChannel() && Owner) {
    if (Owner->PlayerFlags&VBasePlayer::PF_Spawned) {
      GCon->Log(NAME_DevNet, "*** TIMEOUT: PLAYER IS SPAWNED");
    } else {
      GCon->Log(NAME_DevNet, "*** TIMEOUT: PLAYER IS *NOT* SPAWNED");
    }
  }
  */
}


//==========================================================================
//
//  VNetConnection::KeepaliveTick
//
//==========================================================================
void VNetConnection::KeepaliveTick () {
  if (State == NETCON_Closed) return;
  // reset timeout
  Driver->SetNetTime();
  NetCon->LastMessageTime = Driver->NetTime; // presume that we got something from the server
  // flush any remaining data or send keepalive
  Flush(true); // as keepalive
}


//==========================================================================
//
//  VNetConnection::Tick
//
//==========================================================================
void VNetConnection::Tick () {
  if (State == NETCON_Closed) return;

  // for bots and demo playback there's no other end that will send us
  // the ACK so just mark all outgoing messages as ACK-ed
  // `AutoAck == true` means "demo recording"
  if (AutoAck) {
    //FIXME
    Sys_Error("demos are not implemeted yet (0)");
    /*
    for (auto it = ChanIdxMap.first(); it; ++it) {
      VChannel *chan = it.getValue();
      for (VMessageOut *Msg = chan->OutMsg; Msg; Msg = Msg->Next) Msg->bReceivedAck = true;
      chan->OpenAcked = true;
      if (chan->OutMsg) chan->ReceivedAck();
    }
    */
  }

  // perform channel cleanup
  if (HasDeadChannels) RemoveDeadThinkerChannels();

  // see if this connection has timed out
  if (IsTimeoutExceeded()) {
    ShowTimeoutStats();
    State = NETCON_Closed;
    return;
  }

  // run tick for all open channels
  for (auto it = ChanIdxMap.first(); it; ++it) {
    //!GCon->Logf(NAME_DevNet, "   VNetConnection (%s): channel %d (%s) is ticking...", *GetAddress(), it.getValue()->Index, it.getValue()->GetTypeName());
    it.getValue()->Tick();
  }
  // perform channel cleanup
  if (HasDeadChannels) RemoveDeadThinkerChannels();
  // if general channel has been closed, then this connection is closed
  if (!GetGeneralChannel()) { State = NETCON_Closed; return; }

  // flush any remaining data or send keepalive
  Flush();

  //GCon->Logf(NAME_DevNet, "***: (time delta=%g); sent: %d (%d); recv: %d (%d)", (/*Sys_Time()-*/Driver->NetTime-NetCon->LastMessageTime)*1000.0f, Driver->MessagesSent, Driver->packetsSent, Driver->MessagesReceived, Driver->packetsReceived);
}


//==========================================================================
//
//  VNetConnection::SendCommand
//
//==========================================================================
void VNetConnection::SendCommand (VStr Str) {
  VMessageOut msg(GetGeneralChannel());
  msg << Str;
  GetGeneralChannel()->SendMessage(msg);
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

  // mark all entity channels as not updated in this frame, and remove dead channels
  RemoveDeadThinkerChannels(true);

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
      // not a gore
      chan = (VThinkerChannel *)CreateChannel(CHANNEL_Thinker, -1);
      if (!chan) {
        // remember this thinker
        PendingThinkers.append(*th);
        continue;
      }
      chan->SetThinker(*th);
    }
    chan->Update();
  }

  // close entity channels that were not updated in this frame
  {
    HasDeadChannels = false;
    auto it = ChanIdxMap.first();
    while (it) {
      VChannel *chan = it.getValue();
      if (chan->IsThinker()) {
        VThinkerChannel *tc = (VThinkerChannel *)chan;
        if (!tc->UpdatedThisFrame) {
          if (!chan->Closing) {
            chan->Close();
          }
        }
        if (chan->IsDead()) {
          // remove it
          UnregisterChannel(chan, false); // leave it in hash
          chan->Index = -667; // channel should not unregister itself, we'll take care of it
          delete chan;
          it.removeCurrent();
          continue;
        }
        // remember gore entities
        if (!tc->Closing && tc->Thinker && VStr::startsWith(tc->Thinker->GetClass()->GetName(), "K8Gore")) {
          AliveGoreChans.append(chan->Index);
        }
      } else if (chan->IsDead()) {
        HasDeadChannels = true;
      }
      ++it;
    }
  }

  // if we have some pending thinkers, open channels for them
  if (PendingThinkers.length()) {
    static_assert(sizeof(PendingThinkers[0]) == sizeof(VThinker *), "wtf?!");
    // sort them
    timsort_r(PendingThinkers.ptr(), PendingThinkers.length(), sizeof(PendingThinkers[0]), &cmpPendingThinkers, (void *)this);
    // if we have not enough free channels, remove gore entities
    if (AliveGoreChans.length() && MAX_CHANNELS-ChanIdxMap.length() < PendingThinkers.length()) {
      int needChans = PendingThinkers.length()-(MAX_CHANNELS-ChanIdxMap.length());
      while (AliveGoreChans.length() && needChans-- > 0) {
        // pop index
        vint32 idx = AliveGoreChans[AliveGoreChans.length()-1];
        AliveGoreChans.removeAt(AliveGoreChans.length()-1);
        // close channel
        VChannel **chanp = ChanIdxMap.find(idx);
        if (chanp) {
          VChannel *chan = *chanp;
          vassert(chan->IsThinker());
          VThinkerChannel *tc = (VThinkerChannel *)chan;
          vassert(tc->Thinker && tc->Thinker->GetClass()->IsChildOf(VEntity::StaticClass()));
          //PendingGoreEnts.append((VEntity *)(tc->Thinker));
          chan->Close();
        }
      }
    }
    // append thinkers
    for (int f = 0; f < PendingThinkers.length(); ++f) {
      VThinkerChannel *chan = (VThinkerChannel *)CreateChannel(CHANNEL_Thinker, -1);
      if (!chan) break; // no room
      chan->SetThinker(PendingThinkers[f]);
      chan->Update();
    }
  }

  // append gore entities if we have any free slots
  if (PendingGoreEnts.length() && MAX_CHANNELS-ChanIdxMap.length() > 0) {
    // sort them
    timsort_r(PendingGoreEnts.ptr(), PendingGoreEnts.length(), sizeof(PendingGoreEnts[0]), &cmpPendingGoreEnts, (void *)this);
    for (auto &&it : PendingGoreEnts) {
      VThinkerChannel *chan = (VThinkerChannel *)CreateChannel(CHANNEL_Thinker, -1);
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

  // limit thinker update rate
  // level updated 60 times per second
  // thinkers updated 35 times per second
  //NeedsUpdate = false; // don't reset it, we'll do rate limiting here

  double ctt = Sys_Time();
  bool needUpdateLevel = false;
  bool needUpdateThinkers = false;

  if (NextUpdateTimeLevel <= ctt) {
    needUpdateLevel = true;
    NextUpdateTimeLevel = ctt+1.0/60.0;
  }

  if (NextUpdateTimeThinkers <= ctt) {
    needUpdateThinkers = true;
    NextUpdateTimeThinkers = ctt+1.0/35.0;
  }

  if (!needUpdateLevel && !needUpdateThinkers) return;

  SetupFatPVS();

  if (needUpdateLevel) GetLevelChannel()->Update();
  if (needUpdateThinkers) UpdateThinkers();
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
  auto it = ChanIdxMap.first();
  while (it) {
    VChannel *chan = it.getValue();
    vassert(chan);
    if (chan->IsThinker()) {
      chan->Close(); //k8: should we close it, or we can simply kill it?
      /*
      chan->Suicide();
      // this channel is closed, and should be removed
      UnregisterChannel(chan, false); // leave it in hash
      chan->Index = -668; // channel should not unregister itself, we'll take care of it
      delete chan;
      it.removeCurrent();
      continue;
      */
    }
    ++it;
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
