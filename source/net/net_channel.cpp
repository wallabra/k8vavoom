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
#include "../gamedefs.h"
#include "network.h"
#include "net_message.h"

extern VCvarB net_debug_dump_recv_packets;
static VCvarB net_debug_rpc("net_debug_rpc", false, "Dump RPC info?");


//==========================================================================
//
//  VChannel::VChannel
//
//==========================================================================
VChannel::VChannel (VNetConnection *AConnection, EChannelType AType, vint32 AIndex, bool AOpenedLocally)
  : Connection(AConnection)
  , Index(AIndex)
  , Type(AType)
  , OpenedLocally(AOpenedLocally)
  , OpenAcked(AIndex >= 0 && AIndex < CHANIDX_ObjectMap) // those channels are automatically opened
  , Closing(false)
  , InListCount(0)
  , InListBits(0)
  , OutListCount(0)
  , OutListBits(0)
  , InList(nullptr)
  , OutList(nullptr)
  , bSentAnyMessages(false)
{
  vassert(Index >= 0 && Index < MAX_CHANNELS);
  vassert(!Connection->Channels[Index]);
  Connection->Channels[Index] = this;
  Connection->OpenChannels.append(this);
}


//==========================================================================
//
//  VChannel::~VChannel
//
//==========================================================================
VChannel::~VChannel () {
  // free outgoung queue
  while (OutList) {
    VMessageOut *curr = OutList;
    OutList = curr->Next;
    delete curr;
  }
  // free incoming queue
  while (InList) {
    VMessageIn *curr = InList;
    InList = curr->Next;
    delete curr;
  }

  // remove from connection's channel table
  if (Connection && Index >= 0 && Index < MAX_CHANNELS) {
    vassert(Connection->Channels[Index] == this);
    Connection->OpenChannels.Remove(this);
    Connection->Channels[Index] = nullptr;
    Connection = nullptr;
  }
}


//==========================================================================
//
//  VChannel::GetName
//
//==========================================================================
VStr VChannel::GetName () const noexcept {
  return VStr(va("chan #%d(%s)", Index, GetTypeName()));
}


//==========================================================================
//
//  VChannel::GetDebugName
//
//==========================================================================
VStr VChannel::GetDebugName () const noexcept {
  return (Connection ? Connection->GetAddress() : VStr("<noip>"))+":"+(IsLocalChannel() ? "l:" : "r:")+GetName();
}


//==========================================================================
//
//  VChannel::GetLastOutSeq
//
//==========================================================================
vuint32 VChannel::GetLastOutSeq () const noexcept {
  return (Connection ? Connection->OutReliable[Index] : 0);
}


//==========================================================================
//
//  VChannel::IsQueueFull
//
//  returns:
//   -1: oversaturated
//    0: ok
//    1: full
//
//==========================================================================
int VChannel::IsQueueFull () noexcept {
  //if (OutListCount >= MAX_RELIABLE_BUFFER-2) return false;
  //return (OutListBits >= (MAX_RELIABLE_BUFFER-(forClose ? 1 : 2))*MAX_MSG_SIZE_BITS);
  #if 0
  return
    OutListBits >= /*(MAX_RELIABLE_BUFFER+13)*MAX_MSG_SIZE_BITS*/14000*8 ? -1 : // oversaturated
    OutListBits >= /*MAX_RELIABLE_BUFFER*MAX_MSG_SIZE_BITS*/12000*8 ? 1 : // full
    0; // ok
  #endif
  return
    OutListCount >= MAX_RELIABLE_BUFFER+8 ? -1 : // oversaturated
    OutListCount >= MAX_RELIABLE_BUFFER-1 ? 1 : // full
    0; // ok
  //return (OutListBits >= 33000*8);
}


//==========================================================================
//
//  VChannel::CanSendData
//
//==========================================================================
bool VChannel::CanSendData () noexcept {
  // keep some space for close message
  if (IsQueueFull()) return false;
  return (Connection ? Connection->CanSendData() : false);
}


//==========================================================================
//
//  VChannel::CanSendClose
//
//==========================================================================
bool VChannel::CanSendClose () noexcept {
  //return !IsQueueFull(true);
  return true; // always
}


//==========================================================================
//
//  VChannel::SetClosing
//
//==========================================================================
void VChannel::SetClosing () {
  Closing = true;
}


//==========================================================================
//
//  VChannel::ReceivedCloseAck
//
//  this is called from `ReceivedAcks()`
//  the channel will be automatically closed and destroyed, so don't do it here
//
//==========================================================================
void VChannel::ReceivedCloseAck () {
}


//==========================================================================
//
//  VChannel::OutMessageAcked
//
//  called by `ReceivedAcks()`, strictly in sequence
//
//==========================================================================
void VChannel::OutMessageAcked (VMessageOut &Msg) {
}


//==========================================================================
//
//  VChannel::Close
//
//==========================================================================
void VChannel::Close () {
  // if this channel is already closing, do nothing
  if (Closing) return;
  // if the connection is dead, simply set the flag and get out
  if (!Connection || Connection->IsClosed()) {
    SetClosing();
    return;
  }
  vassert(Connection->Channels[Index] == this);
  vassert(!Closing);
  if (net_debug_dump_recv_packets) GCon->Logf(NAME_DevNet, "%s: sending CLOSE %s", *GetDebugName(), (IsLocalChannel() ? "request" : "ack"));
  // send a close notify, and wait for the ack
  // we should not have any closing message in the queue (sanity check)
  for (VMessageOut *Out = OutList; Out; Out = Out->Next) {
    if (Out->bClose) GCon->Logf(NAME_DevNet, "%s: close flag is not set, yet we already have CLOSE message in queue (pid=%u; stime=%g; time=%g)", *GetDebugName(), Out->PacketId, Out->Time, Sys_Time());
    vassert(!Out->bClose);
  }
  //FIXME!
  if (!bSentAnyMessages && !OpenAcked) GCon->Logf(NAME_DevNet, "WARNING: trying to close the channel %s that wasn't used for anything!", *GetDebugName());
  // send closing message
  SendCloseMessageForced();
  // WARNING! make sure that `SetClosing()` sets `Closing` first, and then does any cleanup!
  // failing to do so may cause recursive call to `Close()` (in thinker channel, for example)
  SetClosing();
  // closing flag should be set, check it
  vassert(Closing);
}


//==========================================================================
//
//  VChannel::PacketLost
//
//==========================================================================
void VChannel::PacketLost (vuint32 PacketId) {
  for (VMessageOut *Out = OutList; Out; Out = Out->Next) {
    // retransmit reliable messages in the lost packet
    if (Out->PacketId == PacketId && !Out->bReceivedAck) {
      vassert(Out->bReliable);
      Connection->SendMessage(Out);
    }
  }
}


//==========================================================================
//
//  VChannel::ReceivedAcks
//
//==========================================================================
void VChannel::ReceivedAcks () {
  vassert(Connection->Channels[Index] == this);

  // sanity check
  for (VMessageOut *Out = OutList; Out && Out->Next; Out = Out->Next) vassert(Out->Next->ChanSequence > Out->ChanSequence);

  // release all acknowledged outgoing queued messages
  bool doClose = false;
  while (OutList && OutList->bReceivedAck) {
    doClose = (doClose || OutList->bClose);
    VMessageOut *curr = OutList;
    OutList = OutList->Next;
    OutListBits -= curr->OutEstimated;
    vassert(OutListBits >= 0);
    OutMessageAcked(*curr);
    delete curr;
    --OutListCount;
    vassert(OutListCount >= 0);
  }

  // if a close has been acknowledged in sequence, we're done
  if (doClose) {
    // `OutList` can still contain some packets here for some reason
    // it looks like a bug in my netcode
    if (OutList) {
      GCon->Logf(NAME_DevNet, "!!!! %s: acked close message, but contains some other unacked messages (%d) !!!!", *GetDebugName(), OutListCount);
      for (VMessageOut *Out = OutList; Out; Out = Out->Next) {
        vassert(!Out->bReceivedAck);
        GCon->Logf(NAME_DevNet, "  pid=%u; csq=%u; cidx=%u; ctype=%u; open=%d; close=%d; reliable=%d; size=%d",
          Out->PacketId, Out->ChanSequence, Out->ChanType, Out->ChanIndex, (int)Out->bOpen, (int)Out->bClose,
          (int)Out->bReliable, Out->GetNumBits());
      }
    }
    vassert(!OutList);
    ReceivedCloseAck();
    delete this;
  }
}


//==========================================================================
//
//  VChannel::SendCloseMessageForced
//
//  this unconditionally adds "close" message to the
//  queue, and marks the channel for closing
//
//  WARNING! DOES NO CHECKS!
//
//==========================================================================
void VChannel::SendCloseMessageForced () {
  if (!Connection) return;
  VMessageOut cnotmsg(this, true); // reliable
  cnotmsg.bClose = true;
  // this should not happen, but...
  if (OpenedLocally && !bSentAnyMessages) cnotmsg.bOpen = true;
  // put into queue without any checks
  cnotmsg.Next = nullptr;
  cnotmsg.ChanSequence = ++Connection->OutReliable[Index];
  VMessageOut *OutMsg = new VMessageOut(cnotmsg);
  VMessageOut **OutLink;
  for (OutLink = &OutList; *OutLink; OutLink = &(*OutLink)->Next) {}
  *OutLink = OutMsg;
  OutMsg->OutEstimated = OutMsg->EstimateSizeInBits();
  OutListBits += OutMsg->OutEstimated;
  ++OutListCount;
  // send the raw message
  OutMsg->bReceivedAck = false;
  Connection->SendMessage(OutMsg);
}


//==========================================================================
//
//  VChannel::SendMessage
//
//==========================================================================
void VChannel::SendMessage (VMessageOut *Msg) {
  vassert(Msg);
  vassert(!Closing);
  vassert(Connection->Channels[Index] == this);
  vassert(!Msg->IsError());

  // set some additional message flags
  if (OpenedLocally && !bSentAnyMessages) {
    // first message must be reliable
    vassert(Msg->bReliable);
    Msg->bOpen = true;
  }
  bSentAnyMessages = true;

  if (Msg->bReliable) {
    // put outgoing message into send queue
    //vassert(OutListCount < MAX_RELIABLE_BUFFER-1+(Msg->bClose ? 1 : 0));
    const int satur = IsQueueFull();
    if (satur) {
      if (satur < 0) {
        GCon->Logf(NAME_DevNet, "NETWORK ERROR: channel %s is highly oversaturated!", *GetDebugName());
        Connection->AbortChannel(this);
        return;
      } else {
        GCon->Logf(NAME_DevNet, "NETWORK ERROR: channel %s is oversaturated!", *GetDebugName());
      }
    }
    Msg->Next = nullptr;
    Msg->ChanSequence = ++Connection->OutReliable[Index];
    VMessageOut *OutMsg = new VMessageOut(*Msg);
    VMessageOut **OutLink;
    for (OutLink = &OutList; *OutLink; OutLink = &(*OutLink)->Next) {}
    *OutLink = OutMsg;
    Msg = OutMsg; // use this new message for sending
    Msg->OutEstimated = Msg->EstimateSizeInBits();
    OutListBits += Msg->OutEstimated;
    ++OutListCount;
  }

  // send the raw message
  Msg->bReceivedAck = false;
  Connection->SendMessage(Msg);
  // if we're closing the channel, mark this channel as dying, so we can reject any new data
  // note that we can still have some fragments of the data in incoming queue, and it will be
  // processed normally
  if (Msg->bClose) SetClosing();
}


//==========================================================================
//
//  VChannel::ProcessInMessage
//
//==========================================================================
bool VChannel::ProcessInMessage (VMessageIn &Msg) {
  // fix channel incoming sequence
  if (Msg.bReliable) Connection->InReliable[Index] = Msg.ChanSequence;

  // parse a message
  const bool isCloseMsg = Msg.bClose;
  if (!Closing) ParseMessage(Msg);

  // handle a close notify
  if (isCloseMsg) {
    if (InList) Sys_Error("ERROR: %s: closing channel #%d with unprocessed incoming queue", *GetDebugName(), Index);
    delete this;
    return true;
  }
  return false;
}


//==========================================================================
//
//  VChannel::ReceivedMessage
//
//  process a raw, possibly out-of-sequence message
//  either queue it or dispatch it
//  the message won't be discarded
//
//==========================================================================
void VChannel::ReceivedMessage (VMessageIn &Msg) {
  vassert(Connection->Channels[Index] == this);

  if (Msg.bReliable && Msg.ChanSequence != Connection->InReliable[Index]+1) {
    // if this message is not in a sqeuence, buffer it
    // out-of-sequence message cannot be open message
    // actually, we should show channel error, and block all further messaging on it
    // (or even close the connection, as it looks like broken/malicious)
    vassert(!Msg.bOpen);

    // invariant
    vassert(Msg.ChanSequence > Connection->InReliable[Index]);

    // put this into incoming queue, keeping the queue ordered
    VMessageIn *prev = nullptr, *curr = InList;
    while (curr) {
      if (Msg.ChanSequence == curr->ChanSequence) return; // duplicate message, ignore it
      if (Msg.ChanSequence < curr->ChanSequence) break; // insert before `curr`
      prev = curr;
      curr = curr->Next;
    }
    // copy message
    VMessageIn *newmsg = new VMessageIn(Msg);
    if (prev) prev->Next = newmsg; else InList = newmsg;
    newmsg->Next = curr;
    InListBits += newmsg->GetNumBits();
    ++InListCount;
    // just in case
    for (VMessageIn *m = InList; m && m->Next; m = m->Next) vassert(m->ChanSequence < m->Next->ChanSequence);
    //vassert(InListCount <= MAX_RELIABLE_BUFFER); //FIXME: signal error here!
    if (InListBits > /*(MAX_RELIABLE_BUFFER+18)*MAX_MSG_SIZE_BITS*/128000*8) {
      GCon->Logf(NAME_DevNet, "NETWORK ERROR: channel %s incoming queue overflowed!", *GetDebugName());
      Connection->AbortChannel(this);
      return;
    }
  } else {
    // this is "in sequence" message, process it
    bool removed = ProcessInMessage(Msg);
    if (removed) return;

    // dispatch any waiting messages
    while (InList) {
      if (InList->ChanSequence != Connection->InReliable[Index]+1) break;
      VMessageIn *curr = InList;
      InList = InList->Next;
      InListBits -= curr->GetNumBits();
      --InListCount;
      vassert(InListCount >= 0);
      vassert(InListBits >= 0);
      removed = ProcessInMessage(*curr);
      delete curr;
      if (removed) return;
    }
  }
}


//==========================================================================
//
//  VChannel::Tick
//
//==========================================================================
void VChannel::Tick () {
}


//==========================================================================
//
//  VChannel::WillOverflowMsg
//
//==========================================================================
bool VChannel::WillOverflowMsg (const VMessageOut *msg, int addbits) const noexcept {
  vassert(msg);
  return msg->WillOverflow(addbits);
}


//==========================================================================
//
//  VChannel::WillOverflowMsg
//
//==========================================================================
bool VChannel::WillOverflowMsg (const VMessageOut *msg, const VBitStreamWriter &strm) const noexcept {
  vassert(msg);
  return msg->WillOverflow(strm);
}


//==========================================================================
//
//  VChannel::PutStream
//
//  moves steam to msg (sending previous msg if necessary)
//
//  returns `true` if something was flushed
//
//==========================================================================
bool VChannel::PutStream (VMessageOut *msg, VBitStreamWriter &strm) {
  vassert(msg);
  if (strm.GetNumBits() == 0) return false;
  bool res = false;
  if (WillOverflowMsg(msg, strm)) {
    res = true;
    SendMessage(msg);
    msg->Reset(this, msg->bReliable);
  }
  vassert(!WillOverflowMsg(msg, strm));
  msg->CopyFromWS(strm);
  strm.Clear();
  return res;
}


//==========================================================================
//
//  VChannel::FlushMsg
//
//  sends message if it is not empty
//
//  returns `true` if something was flushed
//
//==========================================================================
bool VChannel::FlushMsg (VMessageOut *msg) {
  vassert(msg);
  if (msg->GetNumBits() || msg->bOpen || msg->bClose) {
    SendMessage(msg);
    msg->Reset(this, msg->bReliable);
    return true;
  }
  return false;
}


//==========================================================================
//
//  VChannel::SendRpc
//
//==========================================================================
void VChannel::SendRpc (VMethod *Func, VObject *Owner) {
  // we cannot simply get out of here, because we need to pop function arguments

  //const bool blockSend = !CanSendData();
  const bool blockSend = (Closing || IsQueueFull() < 0);
  bool serverSide = Closing; // abuse the flag

  // check for server-side only
  if (!serverSide && IsThinker()) {
    VThinkerChannel *tc = (VThinkerChannel *)this;
    VThinker *th = tc->GetThinker();
    if (th && (th->ThinkerFlags&(VThinker::TF_AlwaysRelevant|VThinker::TF_ServerSideOnly)) == VThinker::TF_ServerSideOnly) serverSide = true;
  }

  VMessageOut Msg(this, !!(Func->Flags&FUNC_NetReliable));
  //GCon->Logf(NAME_DevNet, "%s: creating RPC: %s", *GetDebugName(), *Func->GetFullName());
  Msg.WriteUInt((unsigned)Func->NetIndex);

  // serialise arguments
  VStack *Param = VObject::VMGetStackPtr()-Func->ParamsSize+1; // skip self
  for (int i = 0; i < Func->NumParams; ++i) {
    switch (Func->ParamTypes[i].Type) {
      case TYPE_Int:
      case TYPE_Byte:
      case TYPE_Bool:
      case TYPE_Name:
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&Param->i, Func->ParamTypes[i]);
        ++Param;
        break;
      case TYPE_Float:
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&Param->f, Func->ParamTypes[i]);
        ++Param;
        break;
      case TYPE_String:
      case TYPE_Pointer:
      case TYPE_Reference:
      case TYPE_Class:
      case TYPE_State:
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&Param->p, Func->ParamTypes[i]);
        ++Param;
        break;
      case TYPE_Vector:
        {
          TVec Vec;
          Vec.x = Param[0].f;
          Vec.y = Param[1].f;
          Vec.z = Param[2].f;
          VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&Vec, Func->ParamTypes[i]);
          Param += 3;
        }
        break;
      default:
        Host_Error(va("%s: Bad method argument type %d", *GetDebugName(), Func->ParamTypes[i].Type));
    }
    if (Func->ParamFlags[i]&FPARM_Optional) {
      Msg.WriteBit(!!Param->i);
      ++Param;
    }
  }

  if (serverSide) return; // nothing to do

  // send it
  if (blockSend) {
    if (!(Func->Flags&FUNC_NetReliable)) return; // nobody cares
    // alas, cannot send reliable RPC, close the channel, and get out of here
    // if this is non-thinker channel, it is fatal
    // if this is thinker channel, but it has "always relevant", it is fatal
    if (!IsThinker()) {
      GCon->Logf(NAME_DevNet, "%s: cannot send reliable RPC (%s), closing connection (queue: depth=%d; bitsize=%d)", *GetDebugName(), *Func->GetFullName(), OutListCount, OutListBits);
      Connection->Close();
      return;
    }
    // thinker
    VThinkerChannel *tc = (VThinkerChannel *)this;
    if (tc->GetThinker() && (tc->GetThinker()->ThinkerFlags&VThinker::TF_AlwaysRelevant)) {
      GCon->Logf(NAME_DevNet, "%s: cannot send reliable thinker RPC (%s), closing connection (queue: depth=%d; bitsize=%d)", *GetDebugName(), *Func->GetFullName(), OutListCount, OutListBits);
      Connection->Close();
      return;
    }
    GCon->Logf(NAME_DevNet, "%s: cannot send reliable thinker RPC (%s), closing channel (queue: depth=%d; bitsize=%d)", *GetDebugName(), *Func->GetFullName(), OutListCount, OutListBits);
    Close();
    return;
  }

  SendMessage(&Msg);
  if (net_debug_rpc) GCon->Logf(NAME_DevNet, "%s: created %s RPC: %s (%d bits) (queue: depth=%d; bitsize=%d)", *GetDebugName(), (Func->Flags&FUNC_NetReliable ? "reliable" : "unreliable"), *Func->GetFullName(), Msg.GetNumBits(), OutListCount, OutListBits);
}


//==========================================================================
//
//  VChannel::ReadRpc
//
//==========================================================================
bool VChannel::ReadRpc (VMessageIn &Msg, int FldIdx, VObject *Owner) {
  VMethod *Func = nullptr;
  for (VMethod *CM = Owner->GetClass()->NetMethods; CM; CM = CM->NextNetMethod) {
    if (CM->NetIndex == FldIdx) {
      Func = CM;
      break;
    }
  }
  if (!Func) return false;
  if (net_debug_rpc) GCon->Logf(NAME_DevNet, "%s: ...received RPC (%s); method %s", *GetDebugName(), (Connection->IsClient() ? "client" : "server"), *Func->GetFullName());

  //memset(pr_stackPtr, 0, Func->ParamsSize*sizeof(VStack));
  VObject::VMCheckAndClearStack(Func->ParamsSize);
  // push self pointer
  VObject::PR_PushPtr(Owner);
  // get arguments
  for (int i = 0; i < Func->NumParams; ++i) {
    switch (Func->ParamTypes[i].Type) {
      case TYPE_Int:
      case TYPE_Byte:
      case TYPE_Bool:
      case TYPE_Name:
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&VObject::VMGetStackPtr()->i, Func->ParamTypes[i]);
        VObject::VMIncStackPtr();
        break;
      case TYPE_Float:
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&VObject::VMGetStackPtr()->f, Func->ParamTypes[i]);
        VObject::VMIncStackPtr();
        break;
      case TYPE_String:
        VObject::VMGetStackPtr()->p = nullptr;
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&VObject::VMGetStackPtr()->p, Func->ParamTypes[i]);
        VObject::VMIncStackPtr();
        break;
      case TYPE_Pointer:
      case TYPE_Reference:
      case TYPE_Class:
      case TYPE_State:
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&VObject::VMGetStackPtr()->p, Func->ParamTypes[i]);
        VObject::VMIncStackPtr();
        break;
      case TYPE_Vector:
        {
          TVec Vec;
          VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&Vec, Func->ParamTypes[i]);
          VObject::PR_Pushv(Vec);
        }
        break;
      default:
        Host_Error(va("%s: Bad method argument type `%s` for RPC method call `%s`", *GetDebugName(), *Func->ParamTypes[i].GetName(), *Func->GetFullName()));
    }
    if (Func->ParamFlags[i]&FPARM_Optional) {
      VObject::VMGetStackPtr()->i = Msg.ReadBit();
      VObject::VMIncStackPtr();
    }
  }
  // execute it
  (void)VObject::ExecuteFunction(Func);
  return true;
}
