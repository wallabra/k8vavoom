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

//#define VAVOOM_NET_RECV_DEBUG_EXTRA


static VCvarF net_test_loss("net_test_loss", "0", "Test packet loss code?", CVAR_PreInit);
static VCvarB net_dbg_conn_show_outdated("net_dbg_conn_show_outdated", false, "Show outdated channel messages?");


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
  , NeedsUpdate(false)
  , AutoAck(false)
  , Out(MAX_MSGLEN*8)
  , AckSequence(0)
  , UnreliableSendSequence(0)
  , UnreliableReceiveSequence(0)
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
  /* no need to shuffle it, our allocator will take care of that
  // shuffle
  for (unsigned f = 0; f < ChanFreeIdsUsed; ++f) {
    unsigned sidx = GenRandomU31()%ChanFreeIdsUsed;
    if (sidx != f) {
      const int tmp = ChanFreeIds[f];
      ChanFreeIds[f] = ChanFreeIds[sidx];
      ChanFreeIds[sidx] = tmp;
    }
  }
  */

  memset(InSequence, 0, sizeof(InSequence));
  memset(OutSequence, 0, sizeof(OutSequence));

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
  GCon->Logf(NAME_Dev, "Closing connection %s", *GetAddress());
  //GCon->Logf("NET: deleting #%d channels...", OpenChannels.length());
  // remove all open channels
  while (ChanIdxMap.length()) {
    VChannel *chan = ChanIdxMap.first().getValue();
    vassert(chan);
    chan->Suicide(); // don't send any messages (just in case)
    delete chan;
  }
  ThinkerChannels.reset();
  //GCon->Logf("NET: all channels deleted.");
  if (NetCon) {
    delete NetCon;
    NetCon = nullptr;
  }
  NetCon = nullptr;
  if (Context->IsClient()) {
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
//  VNetConnection::GetMessages
//
//==========================================================================
void VNetConnection::GetMessages () {
  int ret;
  Driver->SetNetTime();
  do {
    TArray<vuint8> Data;
    //  returns  0 if no data is waiting
    //  returns  1 if a packet was received
    //  returns -1 if connection is invalid
    ret = GetRawPacket(Data);
    if (ret == -1) {
      // error
      GCon->Log(NAME_DevNet, "Bad read");
      State = NETCON_Closed;
      return;
    }
    if (ret) {
      NeedsUpdate = true; // we got *any* activity, update the world!
      // received something
      /*
      if (!IsLocalConnection()) {
        NetCon->LastMessageTime = Driver->NetTime;
             if (ret == 1) ++Driver->MessagesReceived;
        else if (ret == 2) ++Driver->UnreliableMessagesReceived; // this seems to never happen
      }
      */
      if (Data.Num() > 0) {
        vuint8 LastByte = Data[Data.Num()-1];
        if (LastByte) {
          // find out real length by stepping back until the trailing bit
          vuint32 Length = Data.Num()*8-1;
          for (vuint8 Mask = 0x80; !(LastByte&Mask); Mask >>= 1) --Length;
          VBitStreamReader Packet(Data.Ptr(), Length);
          //GCon->Logf(NAME_DevNet, "%g: Got network packet; length=%d", Sys_Time(), Length);
          ReceivedPacket(Packet);
        } else {
          GCon->Log(NAME_DevNet, "Packet is missing trailing bit");
        }
      } else {
        GCon->Log(NAME_DevNet, "Packet is too small");
        ++Driver->shortPacketCount;
      }
    }
  } while (ret > 0 && State != NETCON_Closed);
}


//==========================================================================
//
//  VNetConnection::GetRawMessage
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
      #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
      //#ifdef CLIENT
      GCon->Logf(NAME_Debug, ":::%p: removing dead channel #%d", chan, chan->Index);
      //#endif
      #endif
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
  if (Packet.ReadInt(/*256*/) != NETPACKET_DATA) return;
  ++Driver->packetsReceived;

  // update receive time (this is used for keepalive packets)
  if (IsTimeoutExceeded()) {
    ShowTimeoutStats();
    State = NETCON_Closed;
    return;
  }

  Driver->SetNetTime();
  NetCon->LastMessageTime = Driver->NetTime;
  /*
  #ifndef CLIENT
  GCon->Logf(NAME_DevNet, "**** got client packet! *** (interval=%g; tout=%g)", Driver->NetTime-NetCon->LastMessageTime, VNetworkPublic::MessageTimeOut.asFloat());
  #endif
  */

  //NeedsUpdate = true; // this is done elsewhere

  // get unreliable sequence number
  vuint32 Sequence;
  Packet << Sequence;
  if (Packet.IsError()) {
    GCon->Log(NAME_DevNet, "Packet is missing packet ID");
    return;
  }

  // ignore stale datagrams
  // reliable messages will be resent anyway
  if (Sequence < UnreliableReceiveSequence) {
    if (net_dbg_conn_show_outdated) GCon->Log(NAME_DevNet, "Got a stale datagram");
    return;
  }

  // lost some datagrams?
  if (Sequence != UnreliableReceiveSequence) {
    // yeah, record it
    int count = Sequence-UnreliableReceiveSequence;
    Driver->droppedDatagrams += count;
    GCon->Logf(NAME_DevNet, "Dropped %d datagram(s)", count);
  }

  // bump sequence number
  UnreliableReceiveSequence = Sequence+1;

  bool NeedsAck = false;

#ifdef VAVOOM_NET_RECV_DEBUG_EXTRA
  GCon->Logf(NAME_DevNet, "***!!!*** Network Packet (pos=%d; num=%d; seq=%u)", Packet.GetPos(), Packet.GetNum(), Sequence);
#endif
  while (!Packet.AtEnd()) {
    // read a flag to see if it's an ACK or a message
    bool IsAck = Packet.ReadBit();
    if (Packet.IsError()) {
      GCon->Log(NAME_DevNet, "Packet is missing ACK flag");
      return;
    }

    if (IsAck) {
      vuint32 AckSeq;
      Packet << AckSeq;
           if (AckSeq == AckSequence) ++AckSequence;
      else if (AckSeq > AckSequence) AckSequence = AckSeq+1;
      else GCon->Log(NAME_DevNet, "Duplicate ACK received");

      // mark corrresponding messages as ACK-ed
      //for (int i = 0; i < OpenChannels.Num(); ++i) {
      for (auto it = ChanIdxMap.first(); it; ++it) {
        VChannel *chan = it.getValue();
        bool gotAck = false;
        for (VMessageOut *Msg = chan->OutMsg; Msg; Msg = Msg->Next) {
          if (Msg->PacketId == AckSeq) {
            Msg->bReceivedAck = true;
            if (Msg->bOpen) chan->OpenAcked = true;
            gotAck = true;
          }
        }
        if (gotAck) chan->ReceivedAck();
      }
    } else {
      NeedsAck = true;
      VMessageIn Msg;

      // read message header
      Msg.ChanIndex = Packet.ReadInt(/*MAX_CHANNELS*/);
      Msg.bReliable = Packet.ReadBit();
      Msg.bOpen = Packet.ReadBit();
      Msg.bClose = Packet.ReadBit();
      Msg.Sequence = 0;
      Msg.ChanType = 0;
      if (Msg.bReliable) Packet << Msg.Sequence;
      if (Msg.bOpen) Msg.ChanType = Packet.ReadInt(/*CHANNEL_MAX*/);
      if (Packet.IsError()) {
        GCon->Logf(NAME_DevNet, "Packet is missing message header");
        break;
      }

      // read data
      int Length = Packet.ReadInt(/*MAX_MSGLEN*8*/);
      #ifdef VAVOOM_NET_RECV_DEBUG_EXTRA
      GCon->Logf(NAME_DevNet, "SERBITS: len=%d; pos=%d; num=%d; left=%d", Length, Packet.GetPos(), Packet.GetNum(), Packet.GetNum()-Packet.GetPos()-Length);
      #endif
      Msg.SetData(Packet, Length);
      if (Packet.IsError()) {
        GCon->Logf(NAME_DevNet, "Packet (channel %d; open=%d; close=%d; reliable=%d; seq=%d; chantype=%d) is missing message data (len=%d; pos=%d; num=%d)",
          Msg.ChanIndex, (int)Msg.bOpen, (int)Msg.bClose, (int)Msg.bReliable, (int)Msg.Sequence, (int)Msg.ChanType,
          Length, Packet.GetPos(), Packet.GetNum());
        break;
      } else {
        #ifdef VAVOOM_NET_RECV_DEBUG_EXTRA
        GCon->Logf(NAME_DevNet, "*** Packet (channel %d; open=%d; close=%d; reliable=%d; seq=%d; chantype=%d) (len=%d; pos=%d; num=%d; left=%d)",
          Msg.ChanIndex, (int)Msg.bOpen, (int)Msg.bClose, (int)Msg.bReliable, (int)Msg.Sequence, (int)Msg.ChanType,
          Length, Packet.GetPos(), Packet.GetNum(), Packet.GetNum()-Packet.GetPos());
        #endif
      }

      VChannel *Chan = GetChannelByIndex(Msg.ChanIndex);
      if (!Chan) {
        if (Msg.bOpen) {
          Chan = CreateChannel(Msg.ChanType, Msg.ChanIndex, false);
          Chan->OpenAcked = true;
          #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
          #ifdef CLIENT
          if (Chan->IsThinker()) {
            VThinkerChannel *tc = (VThinkerChannel *)Chan;
            GCon->Logf(NAME_Debug, ":::%p: created thinker channel #%d (%s : %u)", Chan, Chan->Index, (tc->Thinker ? tc->Thinker->GetClass()->GetName() : "<none>"), (tc->Thinker ? tc->Thinker->GetUniqueId() : 0u));
          } else {
            GCon->Logf(NAME_Debug, ":::%p: created channel #%d", Chan, Chan->Index);
          }
          #endif
          #endif
        } else {
          if (Msg.ChanIndex < 0 || Msg.ChanIndex >= MAX_CHANNELS) {
            GCon->Logf(NAME_DevNet, "Ignored message for invalid channel %d", Msg.ChanIndex);
          } else if (!Msg.bClose && Msg.bReliable) {
            //k8: we still may receive some resent messages for closed channels; why?
            // ignore outdated messages
            if (Msg.bReliable && Msg.Sequence < InSequence[Msg.ChanIndex]) {
              //++Driver->receivedDuplicateCount;
              if (net_dbg_conn_show_outdated) {
                GCon->Logf(NAME_DevNet, "Ignored outdated message for channel %d (msg seq is %u, conn seq is %u)", Msg.ChanIndex, Msg.Sequence, InSequence[Msg.ChanIndex]);
              }
            } else {
              GCon->Logf(NAME_DevNet, "Channel %d is not open (msg seq is %u, conn seq is %u)", Msg.ChanIndex, Msg.Sequence, InSequence[Msg.ChanIndex]);
            }
          }
          continue;
        }
      }
      Chan->ReceivedRawMessage(Msg);
    }
  }

  if (NeedsAck) SendAck(Sequence);
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
  #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
  #ifdef CLIENT
  if (chan->IsThinker()) {
    VThinkerChannel *tc = (VThinkerChannel *)chan;
    GCon->Logf(NAME_Debug, ":::%p: registered thinker channel #%d (%s : %u)", chan, chan->Index, (tc->Thinker ? tc->Thinker->GetClass()->GetName() : "<none>"), (tc->Thinker ? tc->Thinker->GetUniqueId() : 0u));
  } else {
    GCon->Logf(NAME_Debug, ":::%p: registered channel #%d", chan, chan->Index);
  }
  #endif
  #endif
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
  #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
  #ifdef CLIENT
  if (chan->IsThinker()) {
    VThinkerChannel *tc = (VThinkerChannel *)chan;
    GCon->Logf(NAME_Debug, ":::%p: unregistering thinker channel #%d (%s : %u)", chan, chan->Index, (tc->Thinker ? tc->Thinker->GetClass()->GetName() : "<none>"), (tc->Thinker ? tc->Thinker->GetUniqueId() : 0u));
  } else {
    GCon->Logf(NAME_Debug, ":::%p: unregistering channel #%d", chan, chan->Index);
  }
  #endif
  #endif
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
//  VNetConnection::SendRawMessage
//
//==========================================================================
void VNetConnection::SendRawMessage (VMessageOut &Msg) {
  PrepareOut(MAX_MESSAGE_HEADER_BITS+Msg.GetNumBits());

  Out.WriteBit(false);
  Out.WriteInt(Msg.ChanIndex/*, MAX_CHANNELS*/);
  Out.WriteBit(Msg.bReliable);
  Out.WriteBit(Msg.bOpen);
  Out.WriteBit(Msg.bClose);
  if (Msg.bReliable) Out << Msg.Sequence;
  if (Msg.bOpen) Out.WriteInt(Msg.ChanType/*, CHANNEL_MAX*/);
  Out.WriteInt(Msg.GetNumBits()/*, MAX_MSGLEN*8*/);
  Out.SerialiseBits(Msg.GetData(), Msg.GetNumBits());

  Msg.Time = Driver->NetTime;
  Msg.PacketId = UnreliableSendSequence;
}


//==========================================================================
//
//  VNetConnection::SendAck
//
//==========================================================================
void VNetConnection::SendAck (vuint32 Sequence) {
  if (AutoAck) return;
  PrepareOut(33);
  Out.WriteBit(true);
  Out << Sequence;
}


//==========================================================================
//
//  VNetConnection::PrepareOut
//
//==========================================================================
void VNetConnection::PrepareOut (int Length) {
  // send current packet if new message doesn't fit
  if (Out.GetNumBits()+Length+MAX_PACKET_TRAILER_BITS > MAX_MSGLEN*8) Flush();
  if (Out.GetNumBits() == 0) {
    Out.WriteInt(NETPACKET_DATA/*, 256*/);
    Out << UnreliableSendSequence;
  }
}


//==========================================================================
//
//  VNetConnection::Flush
//
//==========================================================================
void VNetConnection::Flush () {
  Driver->SetNetTime();
  if (State == NETCON_Closed) return;

  if (!Out.GetNumBits()) {
    // do not sent keepalices for autoack (demos) and local connections, nobody cares
    // `AutoAck == true` means "demo recording"
    if (AutoAck || IsLocalConnection()) return;
    double tout = VNetworkPublic::MessageTimeOut;
    if (tout < 0.02) tout = 0.02;
    if (Driver->NetTime-LastSendTime < tout/4.0f) return;
    //GCon->Log(NAME_Debug, "sending keepalive...");
  }

  // prepare out for keepalive messages
  if (!Out.GetNumBits()) PrepareOut(0);

  // add trailing bit so we can find out how many bits the message has
  Out.WriteBit(true);
  // pad it with zero bits until byte boundary
  while (Out.GetNumBits()&7) Out.WriteBit(false);

  // send the message
  if (net_test_loss == 0 || Random()*100.0f <= net_test_loss) {
    if (NetCon->SendMessage(Out.GetData(), Out.GetNumBytes()) == -1) State = NETCON_Closed;
  }
  LastSendTime = Driver->NetTime;

  if (!IsLocalConnection()) ++Driver->MessagesSent;
  ++Driver->packetsSent;

  // increment outgoing packet counter
  ++UnreliableSendSequence;

  // clear outgoing packet buffer
  //Out = VBitStreamWriter(MAX_MSGLEN*8);
  //k8: shout we fully reinit `Out` here?
  Out.Reinit(MAX_MSGLEN*8);
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
  NetCon->LastMessageTime = Driver->NetTime;
  // flush any remaining data or send keepalive
  Flush();
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
    for (auto it = ChanIdxMap.first(); it; ++it) {
      VChannel *chan = it.getValue();
      for (VMessageOut *Msg = chan->OutMsg; Msg; Msg = Msg->Next) Msg->bReceivedAck = true;
      chan->OpenAcked = true;
      if (chan->OutMsg) chan->ReceivedAck();
    }
  }

  // perform channel cleanup
  if (HasDeadChannels) RemoveDeadThinkerChannels();

  // see if this connection has timed out
  bool connTimedOut = false;
  if (IsTimeoutExceeded()) {
    ShowTimeoutStats();
    State = NETCON_Closed;
    connTimedOut = true;
  }

  if (!connTimedOut) {
    // run tick for all open channels
    //for (int i = OpenChannels.Num()-1; i >= 0; --i) if (OpenChannels[i]) OpenChannels[i]->Tick();
    for (auto it = ChanIdxMap.first(); it; ++it) it.getValue()->Tick();
    // perform channel cleanup
    if (HasDeadChannels) RemoveDeadThinkerChannels();
    // if general channel has been closed, then this connection is closed
    if (!GetGeneralChannel()) State = NETCON_Closed;
  }

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
  VMessageOut Msg(GetGeneralChannel());
  Msg.bReliable = true;
  Msg << Str;
  GetGeneralChannel()->SendMessage(&Msg);
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
//  VNetConnection::UpdateLevel
//
//==========================================================================
void VNetConnection::UpdateLevel () {
  if (GetLevelChannel()->Level) {
    SetupFatPVS();

    GetLevelChannel()->Update();
    PendingThinkers.reset();
    PendingGoreEnts.reset();
    AliveGoreChans.reset();
    #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
    #ifndef CLIENT
    GCon->Log(NAME_Debug, "=== VNetConnection::UpdateLevel ===");
    #endif
    #endif

    // mark all entity channels as not updated in this frame, and remove dead channels
    RemoveDeadThinkerChannels(true);

    // update mobjs in sight
    for (TThinkerIterator<VThinker> th(Context->GetLevel()); th; ++th) {
      if (!IsRelevant(*th)) continue;
      VThinkerChannel *chan = ThinkerChannels.FindPtr(*th);
      if (!chan) {
        // add gore entities as last ones
        if (VStr::startsWith(th->GetClass()->GetName(), "K8Gore")) {
          /*
          #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
          #ifndef CLIENT
          GCon->Logf(NAME_Debug, "  thinker '%s':%u is GORE: pending...", th->GetClass()->GetName(), th->GetUniqueId());
          #endif
          #endif
          */
          vassert(th->GetClass()->IsChildOf(VEntity::StaticClass()));
          PendingGoreEnts.append((VEntity *)(*th));
          continue;
        }
        // not a gore
        chan = (VThinkerChannel *)CreateChannel(CHANNEL_Thinker, -1);
        if (!chan) {
          // remember this thinker
          #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
          #ifndef CLIENT
          GCon->Logf(NAME_Debug, "  thinker '%s':%u is pending...", th->GetClass()->GetName(), th->GetUniqueId());
          #endif
          #endif
          PendingThinkers.append(*th);
          continue;
        }
        #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
        GCon->Logf(NAME_Debug, "  thinker '%s':%u is new channel #%d...", th->GetClass()->GetName(), th->GetUniqueId(), chan->Index);
        #endif
        chan->SetThinker(*th);
      } else {
        /*
        #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
        #ifndef CLIENT
        GCon->Logf(NAME_Debug, "  #%d: old thinker '%s':%u", chan->Index, th->GetClass()->GetName(), th->GetUniqueId());
        #endif
        #endif
        */
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
              #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
              GCon->Logf(NAME_Debug, ":::%p: closing thinker channel #%d (%s : %u)", chan, chan->Index, (tc->Thinker ? tc->Thinker->GetClass()->GetName() : "<none>"), (tc->Thinker ? tc->Thinker->GetUniqueId() : 0u));
              #endif
              chan->Close();
            }
          }
          if (chan->IsDead()) {
            // remove it
            #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
            #ifdef CLIENT
            GCon->Logf(NAME_Debug, ":::%p: removing inactive/closed channel #%d", chan, chan->Index);
            #endif
            #endif
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

    #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
    #ifndef CLIENT
    if (PendingThinkers.length()) {
      GCon->Logf(NAME_Debug, "  *** we have %d pending thinkers, and %d free channels...", PendingThinkers.length(), MAX_CHANNELS-ChanIdxMap.length());
    }
    #endif
    #endif

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
        #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
        GCon->Logf(NAME_Debug, "  added pending thinger '%s':%u (%p; #%d)", PendingThinkers[f]->GetClass()->GetName(), PendingThinkers[f]->GetUniqueId(), chan, chan->Index);
        #endif
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
        #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
        GCon->Logf(NAME_Debug, "  added pending gore '%s':%u (%p; #%d)", it->GetClass()->GetName(), it->GetUniqueId(), chan, chan->Index);
        #endif
        chan->Update();
      }
    }
  }
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
      #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
      #ifdef CLIENT
      VThinkerChannel *tc = (VThinkerChannel *)chan;
      GCon->Logf(NAME_Debug, ":::%p: resetlevel: killing thinker channel #%d (%s : %u)", chan, chan->Index, (tc->Thinker ? tc->Thinker->GetClass()->GetName() : "<none>"), (tc->Thinker ? tc->Thinker->GetUniqueId() : 0u));
      #endif
      #endif
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
  /*
  for (int i = OpenChannels.Num()-1; i >= 0; --i) {
    VChannel *Chan = OpenChannels[i];
    if (Chan->Type == CHANNEL_Thinker) Chan->Close();
  }
  */
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
