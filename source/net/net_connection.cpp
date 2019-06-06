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
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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


static VCvarF net_test_loss("net_test_loss", "0", "Test packet loss code?", CVAR_PreInit);


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
  memset(Channels, 0, sizeof(Channels));
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
  GCon->Logf(NAME_Dev, va("Closing connection %s", *GetAddress()));
  //GCon->Logf("NET: deleting #%d channels...", OpenChannels.length());
  while (OpenChannels.length()) {
    int idx = OpenChannels.length()-1;
    if (OpenChannels[idx]) {
      delete OpenChannels[idx];
      if (OpenChannels.length() == idx+1) {
        GCon->Logf(NAME_DevNet, "channel #%d failed to remove itself, initialing manual remove...", idx);
        OpenChannels[idx] = nullptr;
        OpenChannels.SetNum(idx);
      }
    } else {
      GCon->Logf(NAME_DevNet, "channel #%d is empty", idx);
      OpenChannels.SetNum(idx);
    }
  }
  //GCon->Logf("NET: all channels deleted.");
  if (NetCon) {
    delete NetCon;
    NetCon = nullptr;
  }
  NetCon = nullptr;
  if (Context->ServerConnection) {
    checkSlow(Context->ServerConnection == this);
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
    ret = GetRawPacket(Data);
    if (ret == -1) {
      GCon->Log(NAME_DevNet, "Bad read");
      State = NETCON_Closed;
      return;
    }
    if (ret) {
      if (!IsLocalConnection()) {
        NetCon->LastMessageTime = Driver->NetTime;
             if (ret == 1) ++Driver->MessagesReceived;
        else if (ret == 2) ++Driver->UnreliableMessagesReceived;
      }
      if (Data.Num() > 0) {
        vuint8 LastByte = Data[Data.Num()-1];
        if (LastByte) {
          // find out real length by stepping back until the trailing bit
          vuint32 Length = Data.Num()*8-1;
          for (vuint8 Mask = 0x80; !(LastByte&Mask); Mask >>= 1) --Length;
          VBitStreamReader Packet(Data.Ptr(), Length);
          ReceivedPacket(Packet);
        } else {
          GCon->Logf(NAME_DevNet, "Packet is missing trailing bit");
        }
      } else {
        GCon->Logf(NAME_DevNet, "Packet is too small");
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
  checkSlow(NetCon);
  return NetCon->GetMessage(Data);
}


//==========================================================================
//
//  VNetConnection::ReceivedPacket
//
//==========================================================================
void VNetConnection::ReceivedPacket (VBitStreamReader &Packet) {
  if (Packet.ReadInt(/*256*/) != NETPACKET_DATA) return;
  ++Driver->packetsReceived;

  NeedsUpdate = true;

  vuint32 Sequence;
  Packet << Sequence;
  if (Packet.IsError()) {
    GCon->Log(NAME_DevNet, "Packet is missing packet ID");
    return;
  }
  if (Sequence < UnreliableReceiveSequence) GCon->Log(NAME_DevNet, "Got a stale datagram");
  if (Sequence != UnreliableReceiveSequence) {
    int count = Sequence-UnreliableReceiveSequence;
    Driver->droppedDatagrams += count;
    GCon->Logf(NAME_DevNet, "Dropped %d datagram(s)", count);
  }
  UnreliableReceiveSequence = Sequence+1;

  bool NeedsAck = false;

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
      for (int i = 0; i < OpenChannels.Num(); ++i) {
        for (VMessageOut *Msg = OpenChannels[i]->OutMsg; Msg; Msg = Msg->Next) {
          if (Msg->PacketId == AckSeq) {
            Msg->bReceivedAck = true;
            if (Msg->bOpen) OpenChannels[i]->OpenAcked = true;
          }
        }
      }

      // notify channels that ACK has been received
      for (int i = OpenChannels.Num()-1; i >= 0; --i) OpenChannels[i]->ReceivedAck();
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
      Msg.SetData(Packet, Length);
      if (Packet.IsError()) {
        GCon->Logf(NAME_DevNet, "Packet is missing message data");
        break;
      }

      VChannel *Chan = Channels[Msg.ChanIndex];
      if (!Chan) {
        if (Msg.bOpen) {
          Chan = CreateChannel(Msg.ChanType, Msg.ChanIndex, false);
          Chan->OpenAcked = true;
        } else {
          GCon->Logf(NAME_DevNet, "Channel %d is not open", Msg.ChanIndex);
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
//  VNetConnection::CreateChannel
//
//==========================================================================
VChannel *VNetConnection::CreateChannel (vuint8 Type, vint32 AIndex, vuint8 OpenedLocally) {
  // if channel index is -1, find a free channel slot
  vint32 Index = AIndex;
  if (Index == -1) {
    Index = CHANIDX_ThinkersStart;
    while (Index < MAX_CHANNELS && Channels[Index]) ++Index;
    if (Index == MAX_CHANNELS) return nullptr;
  }

  switch (Type) {
    case CHANNEL_Control: return new VControlChannel(this, Index, OpenedLocally);
    case CHANNEL_Level: return new VLevelChannel(this, Index, OpenedLocally);
    case CHANNEL_Player: return new VPlayerChannel(this, Index, OpenedLocally);
    case CHANNEL_Thinker: return new VThinkerChannel(this, Index, OpenedLocally);
    case CHANNEL_ObjectMap: return new VObjectMapChannel(this, Index, OpenedLocally);
    default:
      GCon->Logf(NAME_DevNet, "Unknown channel type %d for channel %d", Type, Index);
      return nullptr;
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
  if (!Out.GetNumBits() && Driver->NetTime-LastSendTime < 5.0) return;

  // prepare out for keepalive messages
  if (!Out.GetNumBits()) PrepareOut(0);

  // add trailing bit so we can find out how many bits the message has
  Out.WriteBit(true);
  // pad it with zero bits untill byte boundary
  while (Out.GetNumBits()&7) Out.WriteBit(false);

  // send the message
  if (net_test_loss == 0 || Random()*100.0 <= net_test_loss) {
    if (NetCon->SendMessage(Out.GetData(), Out.GetNumBytes()) == -1) State = NETCON_Closed;
  }
  LastSendTime = Driver->NetTime;

  if (!IsLocalConnection()) ++Driver->MessagesSent;
  ++Driver->packetsSent;

  // increment outgoing packet counter
  ++UnreliableSendSequence;

  // clear outgoing packet buffer
  Out = VBitStreamWriter(MAX_MSGLEN*8);
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
//  VNetConnection::Tick
//
//==========================================================================
void VNetConnection::Tick () {
  // for bots and demo playback there's no other end that will send us
  // the ACK so just mark all outgoing messages as ACK-ed
  if (AutoAck) {
    for (int i = OpenChannels.Num()-1; i >= 0; --i) {
      if (!OpenChannels[i]) continue; // k8: just in case
      for (VMessageOut *Msg = OpenChannels[i]->OutMsg; Msg; Msg = Msg->Next) Msg->bReceivedAck = true;
      OpenChannels[i]->OpenAcked = true;
      OpenChannels[i]->ReceivedAck();
    }
  }

  // see if this connection has timed out
  bool connTimedOut = false;

  if (!IsLocalConnection() && (Driver->MessagesSent > 90 || Driver->MessagesReceived > 10)) {
    double currTime = Sys_Time();
    double tout = VNetworkPublic::MessageTimeOut;
    if (tout < 50) tout = 50;
    tout /= 1000.0f;
    if (currTime-Driver->NetTime-NetCon->LastMessageTime > tout) {
      if (State != NETCON_Closed) GCon->Logf(NAME_DevNet, "ERROR: Channel timed out; time delta=%g; sent %d messages (%d packets), received %d messages (%d packets)",
        (currTime-Driver->NetTime-NetCon->LastMessageTime)*1000.0f,
        Driver->MessagesSent, Driver->packetsSent,
        Driver->MessagesReceived, Driver->packetsReceived);
      State = NETCON_Closed;
      connTimedOut = true;
    }
  }

  if (!connTimedOut) {
    // run tick for all of the open channels
    for (int i = OpenChannels.Num()-1; i >= 0; --i) if (OpenChannels[i]) OpenChannels[i]->Tick();
    // if general channel has been closed, then this connection is closed
    if (!Channels[CHANIDX_General]) State = NETCON_Closed;
  }

  // flush any remaining data or send keepalive
  Flush();

  //GCon->Logf(NAME_DevNet, "***: (time delta=%g); sent: %d (%d); recv: %d (%d)", (Sys_Time()-Driver->NetTime-NetCon->LastMessageTime)*1000.0f, Driver->MessagesSent, Driver->packetsSent, Driver->MessagesReceived, Driver->packetsReceived);
}


//==========================================================================
//
//  VNetConnection::SendCommand
//
//==========================================================================
void VNetConnection::SendCommand (VStr Str) {
  VMessageOut Msg(Channels[CHANIDX_General]);
  Msg.bReliable = true;
  Msg << Str;
  Channels[CHANIDX_General]->SendMessage(&Msg);
}


//==========================================================================
//
//  VNetConnection::SetUpFatPVS
//
//==========================================================================
void VNetConnection::SetUpFatPVS () {
  float dummy_bbox[6] = { -99999, -99999, -99999, 99999, 99999, 99999 };
  VLevel *Level = Context->GetLevel();

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
  Clipper.RepSectors = (Channels[CHANIDX_Level] ? ((VLevelChannel *)Channels[CHANIDX_Level])->Sectors : nullptr);
  SetUpPvsNode(Level->NumNodes-1, dummy_bbox);
}


//==========================================================================
//
//  VNetConnection::SetUpPvsNode
//
//==========================================================================
void VNetConnection::SetUpPvsNode (int BspNum, float *BBox) {
  VLevel *Level = Context->GetLevel();
#ifdef VV_CLIPPER_FULL_CHECK
  if (Clipper.ClipIsFull()) return;
#endif
  if (!Clipper.ClipIsBBoxVisible(BBox)) return;

  if (BspNum == -1) {
    int SubNum = 0;
    subsector_t *Sub = &Level->Subsectors[SubNum];
    if (!Sub->sector->linecount) return; // skip sectors containing original polyobjs
    if (LeafPvs && !(LeafPvs[SubNum>>3]&(1<<(SubNum&7)))) return;
    if (!Clipper.ClipCheckSubsector(Sub)) return;
    Clipper.ClipAddSubsectorSegs(Sub);
    UpdatePvs[SubNum>>3] |= 1<<(SubNum&7);
    return;
  }

  // found a subsector?
  if (!(BspNum&NF_SUBSECTOR)) {
    node_t *Bsp = &Level->Nodes[BspNum];
    // decide which side the view point is on
    int Side = Bsp->PointOnSide(Owner->ViewOrg);
    // recursively divide front space
    SetUpPvsNode(Bsp->children[Side], Bsp->bbox[Side]);
    // possibly divide back space
    //if (!Clipper.ClipIsBBoxVisible(Bsp->bbox[Side^1])) return;
    return SetUpPvsNode(Bsp->children[Side^1], Bsp->bbox[Side^1]);
  } else {
    int SubNum = BspNum&~NF_SUBSECTOR;
    subsector_t *Sub = &Level->Subsectors[SubNum];
    if (!Sub->sector->linecount) return; // skip sectors containing original polyobjs
    if (LeafPvs && !(LeafPvs[SubNum>>3]&(1<<(SubNum&7)))) return;
    if (!Clipper.ClipCheckSubsector(Sub)) return;
    Clipper.ClipAddSubsectorSegs(Sub);
    UpdatePvs[SubNum>>3] |= 1<<(SubNum&7);
  }
}


//==========================================================================
//
//  VNetConnection::CheckFatPVS
//
//==========================================================================
int VNetConnection::CheckFatPVS (subsector_t *Subsector) {
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
  if (Th->ThinkerFlags&VThinker::TF_AlwaysRelevant) return true;
  VEntity *Ent = Cast<VEntity>(Th);
  if (!Ent) return false;
  if (Ent->GetTopOwner() == Owner->MO) return true;
  if (Ent->EntityFlags&VEntity::EF_NoSector) return false;
  if (Ent->EntityFlags&VEntity::EF_Invisible) return false;
  if (!CheckFatPVS(Ent->SubSector)) return false;
  return true;
}


//==========================================================================
//
//  VNetConnection::UpdateLevel
//
//==========================================================================
void VNetConnection::UpdateLevel () {
  SetUpFatPVS();

  ((VLevelChannel *)Channels[CHANIDX_Level])->Update();

  // mark all entity channels as not updated in this frame
  for (int i = OpenChannels.Num()-1; i >= 0; --i) {
    VChannel *Chan = OpenChannels[i];
    if (Chan->Type == CHANNEL_Thinker) ((VThinkerChannel *)Chan)->UpdatedThisFrame = false;
  }

  // update mobjs in sight
  for (TThinkerIterator<VThinker> Th(Context->GetLevel()); Th; ++Th) {
    if (!IsRelevant(*Th)) continue;
    VThinkerChannel *Chan = ThinkerChannels.FindPtr(*Th);
    if (!Chan) {
      Chan = (VThinkerChannel *)CreateChannel(CHANNEL_Thinker, -1);
      if (!Chan) continue;
      Chan->SetThinker(*Th);
    }
    Chan->Update();
  }

  // close entity channels that were not updated in this frame
  for (int i = OpenChannels.Num()-1; i >= 0; --i) {
    VChannel *Chan = OpenChannels[i];
    if (Chan->Type == CHANNEL_Thinker && !((VThinkerChannel*)Chan)->UpdatedThisFrame) Chan->Close();
  }
}


//==========================================================================
//
//  VNetConnection::SendServerInfo
//
//==========================================================================
void VNetConnection::SendServerInfo () {
  if (!ObjMapSent) return;

  // this will load level on client side
  ((VLevelChannel *)Channels[CHANIDX_Level])->SetLevel(GLevel);
  ((VLevelChannel *)Channels[CHANIDX_Level])->SendNewLevel();
  LevelInfoSent = true;
}
