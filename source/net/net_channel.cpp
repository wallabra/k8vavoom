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

extern VCvarB net_debug_dump_recv_packets;


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
  , CloseAcked(false)
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
  CloseAcked = true; // just in case
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
  return (Connection ? Connection->GetAddress() : VStr("<noip>"))+":"+(IsLocalChannel() ? "l:" : "r:")+GetName();
}


//==========================================================================
//
//  VChannel::Suicide
//
//==========================================================================
void VChannel::Suicide () {
  Closing = true;
  CloseAcked = true;
  if (Connection) Connection->MarkChannelsDirty();
}


//==========================================================================
//
//  VChannel::Close
//
//==========================================================================
void VChannel::Close (VMessageOut *msg) {
  if (Closing) return; // already in closing state
  // send close message
  if (net_debug_dump_recv_packets) GCon->Logf(NAME_DevNet, "%s: sending CLOSE %s", *GetDebugName(), (IsLocalChannel() ? "request" : "ack"));
  if (msg) {
    msg->MarkClose();
    SendMessage(*msg);
  } else {
    VMessageOut CloseMsg(this, VMessageOut::Close);
    SendMessage(CloseMsg);
  }
  // enter closing state
  Closing = true;
}


//==========================================================================
//
//  VChannel::WillFlushMsg
//
//==========================================================================
bool VChannel::WillFlushMsg (VMessageOut &msg, VBitStreamWriter &strm) {
  //GCon->Logf(NAME_DevNet, "%s: msglen=%d; strmlen=%d; bothlen=%d; maxlen=%d", *GetDebugName(), msg.GetNumBits(), strm.GetNumBits(), msg.CalcRealMsgBitSize(strm), MAX_MSG_SIZE_BITS);
  if (strm.GetNumBits() == 0) return false;
  return msg.WillOverflow(strm);
}


//==========================================================================
//
//  VChannel::PutStream
//
//  moves steam to msg (sending previous msg if necessary)
//
//==========================================================================
void VChannel::PutStream (VMessageOut &msg, VBitStreamWriter &strm) {
  if (strm.GetNumBits() == 0) return;
  if (msg.WillOverflow(strm)) {
    SendMessage(msg);
    msg.Reset(this);
  }
  vassert(!msg.WillOverflow(strm));
  msg.CopyFromWS(strm);
  strm.Clear();
}


//==========================================================================
//
//  VChannel::FlushMsg
//
//  sends message if it is not empty
//
//==========================================================================
void VChannel::FlushMsg (VMessageOut &msg) {
  SendMessage(msg);
  msg.Reset(this);
}


//==========================================================================
//
//  VChannel::SendMessage
//
//==========================================================================
void VChannel::SendMessage (VMessageOut &Msg) {
  if (Connection && Msg.NeedToSend()) Connection->SendMessage(Msg);
}


//==========================================================================
//
//  VChannel::ReceivedRawMessage
//
//==========================================================================
void VChannel::ReceivedRawMessage (VMessageIn &Msg) {
  if (!OpenAcked && !Msg.bOpen) {
    GCon->Logf(NAME_DevNet, "<<< %s: rejected message for not open-acked channel", *GetDebugName());
    return;
  }

  if (CloseAcked) {
    GCon->Logf(NAME_DevNet, "<<< %s: rejected message for in-closed channel", *GetDebugName());
    return;
  }

  /* no, we still may receive something from that side
  if (Closing && !Msg.bClose) {
    GCon->Logf(NAME_DevNet, "<<< %s: rejected non-close message for closing channel", *GetDebugName());
    return;
  }
  */

  GCon->Logf(NAME_DevNet, "<<< %s: received message", *GetDebugName());

  const bool isClosing = Msg.bClose;
  ParsePacket(Msg);
  // process closing message
  if (isClosing) {
    if (!Closing) ReceivedClosingAck();
    if (Closing) {
      // if we are in "closing" state, and we received closing packed, this is "close ack"
      // ok, we'll be dead anyway, so perform suicide here
      if (net_debug_dump_recv_packets) GCon->Logf(NAME_DevNet, "%s: got CLOSE ACK, performing suicide", *GetDebugName());
      Suicide();
    } else {
      // queue "close ack" or "close request" packet
      Close();
      // if this is not a local channel, kill it for good, becase network layer will take care of ack delivering
      if (!IsLocalChannel()) {
        if (net_debug_dump_recv_packets) GCon->Logf(NAME_DevNet, "%s: sent CLOSE ACK, performing suicide", *GetDebugName());
        Suicide();
      }
    }
  }
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
//  VChannel::Tick
//
//==========================================================================
void VChannel::Tick () {
}


//==========================================================================
//
//  VChannel::SendRpc
//
//==========================================================================
void VChannel::SendRpc (VMethod *Func, VObject *Owner) {
  VMessageOut Msg(this, (Func->Flags&FUNC_NetReliable ? 0u : VMessageOut::Unreliable));
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
  SendMessage(Msg);
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
