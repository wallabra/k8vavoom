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
  , OpenAcked(AIndex < CHANIDX_ThinkersStart)
  , Closing(false)
  , InMsg(nullptr)
  , OutMsg(nullptr)
{
  checkSlow(Index >= 0);
  checkSlow(Index < MAX_CHANNELS);
  checkSlow(!Connection->Channels[Index]);
  Connection->Channels[Index] = this;
  Connection->OpenChannels.Append(this);
}


//==========================================================================
//
//  VChannel::~VChannel
//
//==========================================================================
VChannel::~VChannel () {
  for (VMessageIn *Msg = InMsg; Msg; ) {
    VMessageIn *Next = Msg->Next;
    delete Msg;
    Msg = (Next ? Next : nullptr);
  }
  for (VMessageOut *Msg = OutMsg; Msg; ) {
    VMessageOut *Next = Msg->Next;
    delete Msg;
    Msg = (Next ? Next : nullptr);
  }
  if (Index != -1 && Connection->Channels[Index] == this) {
    Connection->Channels[Index] = nullptr;
  }
  Connection->OpenChannels.Remove(this);
}


//==========================================================================
//
//  VChannel::CountInMessages
//
//==========================================================================
int VChannel::CountInMessages () const {
  int res = 0;
  for (VMessageIn *M = InMsg; M; M = M->Next) ++res;
  return res;
}


//==========================================================================
//
//  VChannel::CountOutMessages
//
//==========================================================================
int VChannel::CountOutMessages () const {
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
  guard(VChannel::ReceivedRawMessage);
  // drop outdated messages
  if (Msg.bReliable && Msg.Sequence < Connection->InSequence[Index]) {
    ++Connection->Driver->receivedDuplicateCount;
    return;
  }

  if (Msg.bReliable && Msg.Sequence > Connection->InSequence[Index]) {
    VMessageIn **pNext = &InMsg;
    while (*pNext && (*pNext)->Sequence <= Msg.Sequence) {
      if ((*pNext)->Sequence == Msg.Sequence) {
        ++Connection->Driver->receivedDuplicateCount;
        return;
      }
      pNext = &(*pNext)->Next;
    }
    VMessageIn *Copy = new VMessageIn(Msg);
    Copy->Next = *pNext;
    *pNext = Copy;
    return;
  }
  if (Msg.bReliable) ++Connection->InSequence[Index];

  if (!Closing) ParsePacket(Msg);
  if (Msg.bClose) {
    delete this;
    return;
  }

  while (InMsg && InMsg->Sequence == Connection->InSequence[Index]) {
    VMessageIn *OldMsg = InMsg;
    InMsg = OldMsg->Next;
    ++Connection->InSequence[Index];
    if (!Closing) ParsePacket(*OldMsg);
    bool Closed = false;
    if (OldMsg->bClose) {
      delete this;
      Closed = true;
    }
    delete OldMsg;
    OldMsg = nullptr;
    if (Closed) return;
  }
  unguard;
}


//==========================================================================
//
//  VChannel::SendMessage
//
//==========================================================================
void VChannel::SendMessage (VMessageOut *AMsg) {
  guard(VChannel::SendMessage);
  VMessageOut *Msg = AMsg;
  if (Msg->IsError()) {
    GCon->Logf(NAME_DevNet, "Overflowed message");
    //abort();
    return;
  }
  if (Msg->bReliable) {
    Msg->Sequence = Connection->OutSequence[Index];

    VMessageOut *Copy = new VMessageOut(*Msg);
    Copy->Next = nullptr;
    VMessageOut **pNext = &OutMsg;
    while (*pNext) pNext = &(*pNext)->Next;
    *pNext = Copy;
    Msg = Copy;

    ++Connection->OutSequence[Index];
  }

  Connection->SendRawMessage(*Msg);
  unguard;
}


//==========================================================================
//
//  VChannel::ReceivedAck
//
//==========================================================================
void VChannel::ReceivedAck () {
  guard(VChannel::ReceivedAck);
  // clean up messages that have been ACK-ed
  // only the first ones are deleted so that close message doesn't
  // get handled while there's still messages that are not ACK-ed
  bool CloseAcked = false;
  while (OutMsg && OutMsg->bReceivedAck) {
    VMessageOut *Msg = OutMsg;
    OutMsg = Msg->Next;
    if (Msg->bClose) CloseAcked = true;
    delete Msg;
    Msg = nullptr;
  }

  // if we received ACK for close message then delete this channel
  if (CloseAcked) delete this;
  unguard;
}


//==========================================================================
//
//  VChannel::Close
//
//==========================================================================
void VChannel::Close () {
  guard(VChannel::Close);
  if (Closing) return; // already in closing state

  // send close message
  VMessageOut Msg(this);
  Msg.bReliable = true;
  Msg.bClose = true;
  SendMessage(&Msg);

  // enter closing state
  Closing = true;
  unguard;
}


//==========================================================================
//
//  VChannel::Tick
//
//==========================================================================
void VChannel::Tick () {
  guard(VChannel::Tick);
  // resend timed out messages
  for (VMessageOut *Msg = OutMsg; Msg; Msg = Msg->Next) {
    if (!Msg->bReceivedAck && Connection->Driver->NetTime-Msg->Time > 1.0) {
      Connection->SendRawMessage(*Msg);
      ++Connection->Driver->packetsReSent;
    }
  }
  unguard;
}


//==========================================================================
//
//  VChannel::SendRpc
//
//==========================================================================
void VChannel::SendRpc (VMethod *Func, VObject *Owner) {
  guard(VChannel::SendRpc);
  VMessageOut Msg(this);
  Msg.bReliable = !!(Func->Flags&FUNC_NetReliable);

  Msg.WriteInt(Func->NetIndex/*, Owner->GetClass()->NumNetFields*/);

  // serialise arguments
  guard(VChannel::SendRpc::SerialiseArguments);
  VStack *Param = pr_stackPtr-Func->ParamsSize+1; // skip self
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
  unguard;

  // send it
  SendMessage(&Msg);
  unguard;
}


//==========================================================================
//
//  VChannel::ReadRpc
//
//==========================================================================
bool VChannel::ReadRpc (VMessageIn &Msg, int FldIdx, VObject *Owner) {
  guard(VChannel::ReadRpc);
  VMethod *Func = nullptr;
  for (VMethod *CM = Owner->GetClass()->NetMethods; CM; CM = CM->NextNetMethod) {
    if (CM->NetIndex == FldIdx) {
      Func = CM;
      break;
    }
  }
  if (!Func) return false;

  memset(pr_stackPtr, 0, Func->ParamsSize*sizeof(VStack));
  // push self pointer
  PR_PushPtr(Owner);
  // get arguments
  for (int i = 0; i < Func->NumParams; ++i) {
    switch (Func->ParamTypes[i].Type) {
      case TYPE_Int:
      case TYPE_Byte:
      case TYPE_Bool:
      case TYPE_Name:
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&pr_stackPtr->i, Func->ParamTypes[i]);
        ++pr_stackPtr;
        break;
      case TYPE_Float:
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&pr_stackPtr->f, Func->ParamTypes[i]);
        ++pr_stackPtr;
        break;
      case TYPE_String:
        pr_stackPtr->p = nullptr;
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&pr_stackPtr->p, Func->ParamTypes[i]);
        ++pr_stackPtr;
        break;
      case TYPE_Pointer:
      case TYPE_Reference:
      case TYPE_Class:
      case TYPE_State:
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&pr_stackPtr->p, Func->ParamTypes[i]);
        ++pr_stackPtr;
        break;
      case TYPE_Vector:
        {
          TVec Vec;
          VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&Vec, Func->ParamTypes[i]);
          PR_Pushv(Vec);
        }
        break;
      default:
        Sys_Error("Bad method argument type %d", Func->ParamTypes[i].Type);
    }
    if (Func->ParamFlags[i]&FPARM_Optional) {
      pr_stackPtr->i = Msg.ReadBit();
      ++pr_stackPtr;
    }
  }
  // execute it
  VObject::ExecuteFunction(Func);
  return true;
  unguard;
}
