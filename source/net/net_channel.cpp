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


//==========================================================================
//
//  VChannel::VChannel
//
//==========================================================================
VChannel::VChannel (VNetConnection *AConnection, EChannelType AType, vint32 AIndex, vuint8 AOpenedLocally)
  : Connection(AConnection)
  , Index(AIndex)
  , Type(AType)
  , OpenedLocally(AOpenedLocally)
  , OpenAcked(false/*AIndex < CHANIDX_ThinkersStart*/)
  , Closing(false)
  , NoMoreIncoming(false)
  , InMsg(nullptr)
  , OutMsg(nullptr)
  , bAllowPrematureClose(false)
{
  vassert(Index >= 0 && Index < MAX_CHANNELS);
  Connection->RegisterChannel(this);
}


//==========================================================================
//
//  VChannel::~VChannel
//
//==========================================================================
VChannel::~VChannel () {
  Closing = true; // just in case
  ClearAllQueues();
  if (Index >= 0 && Index < MAX_CHANNELS && Connection) {
    Connection->UnregisterChannel(this);
    Index = -1; // just in case
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
  return (Connection ? Connection->GetAddress() : VStr("<noip>"))+":"+GetName();
}


//==========================================================================
//
//  VChannel::Suicide
//
//==========================================================================
void VChannel::ClearAllQueues () {
  ClearInQueue();
  ClearOutQueue();
}


//==========================================================================
//
//  VChannel::ClearInQueue
//
//==========================================================================
void VChannel::ClearInQueue () {
  for (VMessageIn *Msg = InMsg; Msg; ) {
    VMessageIn *Next = Msg->Next;
    delete Msg;
    Msg = Next;
  }
  InMsg = nullptr;
}


//==========================================================================
//
//  VChannel::ClearOutQueue
//
//==========================================================================
void VChannel::ClearOutQueue () {
  for (VMessageOut *Msg = OutMsg; Msg; ) {
    VMessageOut *Next = Msg->Next;
    delete Msg;
    Msg = Next;
  }
  OutMsg = nullptr;
}


//==========================================================================
//
//  VChannel::Suicide
//
//==========================================================================
void VChannel::Suicide () {
  Closing = true;
  NoMoreIncoming = true;
  ClearOutQueue(); // it is safe to destroy it
  if (Connection) Connection->MarkChannelsDirty();
}


//==========================================================================
//
//  VChannel::Close
//
//==========================================================================
void VChannel::Close () {
  if (Closing) return; // already in closing state
  // send close message
  VMessageOut Msg(this, true/*reliable*/);
  Msg.bClose = true;
  SendMessage(&Msg);
  // enter closing state
  Closing = true;
}


//==========================================================================
//
//  VChannel::CountInMessages
//
//==========================================================================
int VChannel::CountInMessages () const noexcept {
  int res = 0;
  for (VMessageIn *M = InMsg; M; M = M->Next) ++res;
  return res;
}


//==========================================================================
//
//  VChannel::CountOutMessages
//
//==========================================================================
int VChannel::CountOutMessages () const noexcept {
  int res = 0;
  for (VMessageOut *M = OutMsg; M; M = M->Next) ++res;
  return res;
}


//==========================================================================
//
//  VChannel::ReceivedRawMessage
//
//==========================================================================
void VChannel::ReceivedRawMessage (VMessageIn &Msg) {
  if (NoMoreIncoming) {
    GCon->Logf(NAME_DevNet, "<<< %s: rejected %sreliable message for in-closed channel; seq=%u (curr=%u)", *GetDebugName(), (Msg.bReliable ? "" : "un"), Msg.Sequence, Connection->InSequence[Index]);
    return;
  }

  GCon->Logf(NAME_DevNet, "<<< %s: received %sreliable message; seq=%u (curr=%u)", *GetDebugName(), (Msg.bReliable ? "" : "un"), Msg.Sequence, Connection->InSequence[Index]);

  // drop outdated messages
  if (Msg.bReliable && Msg.Sequence < Connection->InSequence[Index]) {
    GCon->Logf(NAME_DevNet, "<<< %s: dropped duplicate message %u (%u)", *GetDebugName(), Msg.Sequence, Connection->InSequence[Index]);
    ++Connection->Driver->receivedDuplicateCount;
    return;
  }

  // process unreliable message
  if (!Msg.bReliable) {
    // pretend that unreliable messages never reached closed channel
    if (!Closing) {
      GCon->Logf(NAME_DevNet, "<<< %s: processing unreliable message", *GetDebugName());
      ParsePacket(Msg);
    }
    return;
  }

  // this message must be reliable
  vassert(Msg.bReliable);

  // don't overoptimise yet; just copy and insert new message into the queue
  if (Msg.Sequence >= Connection->InSequence[Index]) {
    // message from the future
    // insert it in the processing queue (but don't replace the existing one)
    VMessageIn *prev = nullptr, *curr = InMsg;
    while (curr && curr->Sequence <= Msg.Sequence) {
      if (curr->Sequence == Msg.Sequence) {
        // duplicate message, drop it
        GCon->Logf(NAME_DevNet, "<<< %s: dropped duplicate message; seq=%u (curr=%u)", *GetDebugName(), Msg.Sequence, Connection->InSequence[Index]);
        ++Connection->Driver->receivedDuplicateCount;
        return;
      }
      prev = curr;
      curr = curr->Next;
    }
    // insert *before* `curr`
    VMessageIn *Copy = new VMessageIn(Msg);
    if (prev) prev->Next = Copy; else InMsg = Copy;
    Copy->Next = curr;
    GCon->Logf(NAME_DevNet, "<<< %s: queued sequence message %u", *GetDebugName(), Copy->Sequence);
  }

  if (!InMsg) {
    GCon->Logf(NAME_DevNet, "<<< %s: NO QUEUED MESSAGES, WTF?!", *GetDebugName());
    return;
  }

  GCon->Logf(NAME_DevNet, "<<< %s: first queued message seq=%u (curr=%u)", *GetDebugName(), InMsg->Sequence, Connection->InSequence[Index]);

  // parse all ordered messages
  while (InMsg && InMsg->Sequence == Connection->InSequence[Index]) {
    VMessageIn *OldMsg = InMsg;
    InMsg = OldMsg->Next;
    ++Connection->InSequence[Index];
    const bool isCloseMsg = OldMsg->bClose;
    const vuint32 seq = OldMsg->Sequence;
    GCon->Logf(NAME_DevNet, "<<< %s: processing sequence message %u", *GetDebugName(), OldMsg->Sequence);
    ParsePacket(*OldMsg);
    delete OldMsg;
    if (isCloseMsg) {
      GCon->Logf(NAME_DevNet, "<<< %s: closing channel due to closing queued message %u", *GetDebugName(), seq);
      // we got "close" message from the other side; it means that the other side called `Close()` on this channel, and
      // don't want to send more data here; also, it means that we processed all the data it fed to us.
      // if we have close message to resend on our side, we can safely remove this channel, it is dead.
      // the easy way to check if we have such outgoing message is to check `Closing` flag: it will be
      // set in `Close()`, where the corresponding message is queued.
      // if the channel is going to suicide, it doesn't matter at all.
      if (Closing) {
        // ok, we'll be dead anyway, so perform suicide here
        Suicide();
      } else {
        // queue "close ack" packet
        Close();
      }
      // it is safe to clear incoming queue here (and stop accepting incoming messages)
      ClearInQueue();
      NoMoreIncoming = true;
    }
  }

  if (InMsg) {
    GCon->Logf(NAME_DevNet, "<<< %s: done queue scan; first queued message seq=%u (curr=%u)", *GetDebugName(), InMsg->Sequence, Connection->InSequence[Index]);
  } else {
    GCon->Logf(NAME_DevNet, "<<< %s: done queue scan; inqueue is empty (curr=%u)", *GetDebugName(), Connection->InSequence[Index]);
  }
}


//==========================================================================
//
//  VChannel::SendMessage
//
//==========================================================================
void VChannel::SendMessage (VMessageOut *AMsg) {
  VMessageOut *Msg = AMsg;

  if (Msg->IsError()) {
    GCon->Logf(NAME_Error, "Overflowed message (len=%d bits, %d bytes)", Msg->GetNum(), (Msg->GetNum()+7)/8);
    GCon->Logf(NAME_Error, "  %s: reliable=%d, open=%d; closed=%d; rack=%d; seq=%d; time=%g; pid=%u",
      *GetDebugName(),
      (int)Msg->bReliable, (int)Msg->bOpen, (int)Msg->bClose, (int)Msg->bReceivedAck, Msg->Sequence, Msg->Time, Msg->PacketId);
    return;
  }

  // put reliable message into resend queue
  if (Msg->bReliable) {
    Msg->Sequence = Connection->OutSequence[Index];

    VMessageOut *Copy = new VMessageOut(*Msg);
    Copy->Next = nullptr;
    VMessageOut **pNext = &OutMsg;
    while (*pNext) pNext = &(*pNext)->Next;
    *pNext = Copy;
    Msg = Copy;

    ++Connection->OutSequence[Index];

    GCon->Logf(NAME_DevNet, ">>> %s: sending and queueing %smessage %u", *GetDebugName(), (Msg->bClose ? "CLOSING " : ""), Msg->Sequence);
  }

  Connection->SendRawMessage(*Msg);
}


//==========================================================================
//
//  VChannel::SpecialAck
//
//==========================================================================
void VChannel::SpecialAck (VMessageOut *msg) {
}


//==========================================================================
//
//  VChannel::ReceivedClosingAck
//
//  some channels may want to set some flags here
//
//  WARNING! don't close/suicide here!
//
//==========================================================================
void VChannel::ReceivedClosingAck () {
}


//==========================================================================
//
//  VChannel::ReceivedAck
//
//  returns `true` if channel is closed (the caller should delete it)
//
//==========================================================================
void VChannel::ReceivedAck () {
  // clean up messages that have been ACK-ed
  // only the first ones are deleted so that close message doesn't
  // get handled while there's still messages that are not ACK-ed
  //bool CloseAcked = false;

  // remove *all* acked messages from resend queue, to not spam the network
  // but don't suicide until closed message is acked (if `bAllowPrematureClose` is not set)
  VMessageOut *prev = nullptr, *curr = OutMsg;
  while (curr) {
    // acked?
    if (!curr->bReceivedAck) {
      // no, move to the next message
      prev = curr;
      curr = curr->Next;
      continue;
    }
    // yes, remove this message from resend queue (but check for premature close)
    if (!prev && curr->udata) SpecialAck(curr); // callback if this message is first acked in queue
    // acked close message?
    if (curr->bClose) {
      //CloseAcked = true;
      // if premature closing is allowed, clear the whole queue
      if (bAllowPrematureClose || !prev) {
        if (!Closing) {
          Closing = true;
          ReceivedClosingAck();
          GCon->Logf(NAME_DevNet, ">>> %s: closing die to close ACK seq=%u", *GetDebugName(), curr->Sequence);
        }
        ClearOutQueue();
        // nothing to do if the queue is cleared
        break;
      }
      vassert(prev);
      // move to the next message
      prev = curr;
      curr = curr->Next;
      continue;
    }
    // remove this message from queue
    if (prev) prev->Next = curr->Next; else OutMsg = curr->Next;
    VMessageOut *msg = curr;
    curr = curr->Next;
    delete msg;
  }

  // if we received ACK for close message, mark is as closing
  // if you need to perform something special on close, use `ReceivedClosingAck()`
  //if (CloseAcked) Closing = true;
}


//==========================================================================
//
//  VChannel::Tick
//
//==========================================================================
void VChannel::Tick () {
  // resend timed out messages
  // bomb the network with messages, because why not
  //TODO: messages like player movement should be marked as "urgent" and we
  //      should spam with them without delays
  //!if (OutMsg) GCon->Logf(NAME_DevNet, "VChannel::Tick(%d:%s): nettime=%g; msgtime=%g; tout=%g", Index, GetTypeName(), Connection->Driver->NetTime, OutMsg->Time, Connection->Driver->NetTime-OutMsg->Time);
  for (VMessageOut *Msg = OutMsg; Msg; Msg = Msg->Next) {
    if (!Msg->bReceivedAck) {
      if (Connection->Driver->NetTime-Msg->Time > 1.0/35.0) {
        Connection->SendRawMessage(*Msg);
        ++Connection->Driver->packetsReSent;
        GCon->Logf(NAME_DevNet, ">>> %s: sending %squeued raw message seq=%u (curr=%u); pid=%u", *GetDebugName(), (Msg->bClose ? "CLOSING " : ""), Msg->Sequence, Connection->OutSequence[Index], Msg->PacketId);
      } else {
        //GCon->Logf(NAME_DevNet, ">>> %s: skipped raw message seq=%u (curr=%u) (to=%g : %g); pid=%u", *GetDebugName(), Msg->Sequence, Connection->OutSequence[Index], Connection->Driver->NetTime-Msg->Time, 1.0/35.0, Msg->PacketId);
      }
    } else {
      GCon->Logf(NAME_DevNet, ">>> %s: skipped acked %squeued raw message seq=%u (curr=%u); pid=%u", *GetDebugName(), (Msg->bClose ? "CLOSING " : ""), Msg->Sequence, Connection->OutSequence[Index], Msg->PacketId);
    }
    if (Msg->bClose) break; // there's no need to sand anything after close message
  }
}


//==========================================================================
//
//  VChannel::SendRpc
//
//==========================================================================
void VChannel::SendRpc (VMethod *Func, VObject *Owner) {
  VMessageOut Msg(this, !!(Func->Flags&FUNC_NetReliable)/*reliable*/);
  Msg.WriteInt(Func->NetIndex/*, Owner->GetClass()->NumNetFields*/);

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
        Sys_Error("Bad method argument type %d", Func->ParamTypes[i].Type);
    }
    if (Func->ParamFlags[i]&FPARM_Optional) {
      Msg.WriteBit(!!Param->i);
      ++Param;
    }
  }

  // send it
  SendMessage(&Msg);
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
        Sys_Error("Bad method argument type `%s` for RPC method call `%s`", *Func->ParamTypes[i].GetName(), *Func->GetFullName());
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
