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

VCvarB net_debug_dump_chan_objmap("net_debug_dump_chan_objmap", false, "Dump objectmap communication?");


//==========================================================================
//
//  VObjectMapChannel::VObjectMapChannel
//
//==========================================================================
VObjectMapChannel::VObjectMapChannel (VNetConnection *AConnection, vint32 AIndex, vuint8 AOpenedLocally)
  : VChannel(AConnection, CHANNEL_ObjectMap, AIndex, AOpenedLocally)
  , CurrName(1) // NAME_None is implicit
  , CurrClass(1) // `None` class is implicit
  , needOpenMessage(true)
{
}


//==========================================================================
//
//  VObjectMapChannel::~VObjectMapChannel
//
//==========================================================================
VObjectMapChannel::~VObjectMapChannel () {
  if (Connection) Connection->ObjMapSent = true;
  //GCon->Logf(NAME_DevNet, "initial objects and names sent!");
}


//==========================================================================
//
//  VObjectMapChannel::GetName
//
//==========================================================================
VStr VObjectMapChannel::GetName () const noexcept {
  return VStr(va("ompchan #%d(%s)", Index, GetTypeName()));
}


//==========================================================================
//
//  VObjectMapChannel::IsQueueFull
//
//==========================================================================
int VObjectMapChannel::IsQueueFull () const noexcept {
  return
    OutListBits >= 64000*8 ? -1 : // oversaturated
    OutListBits >= 60000*8 ? 1 : // full
    0; // ok
}


//==========================================================================
//
//  VObjectMapChannel::ReceivedCloseAck
//
//  sets `ObjMapSent` flag
//
//==========================================================================
void VObjectMapChannel::ReceivedCloseAck () {
  if (Connection) Connection->ObjMapSent = true;
  VChannel::ReceivedCloseAck(); // just in case
}


//==========================================================================
//
//  VObjectMapChannel::Tick
//
//==========================================================================
void VObjectMapChannel::Tick () {
  VChannel::Tick();
  if (IsLocalChannel()) Update();
}


//==========================================================================
//
//  VObjectMapChannel::UpdateSendPBar
//
//==========================================================================
void VObjectMapChannel::UpdateSendPBar () {
  RNet_PBarUpdate("sending names and classes", CurrName+CurrClass, Connection->ObjMap->NameLookup.length()+Connection->ObjMap->ClassLookup.length());
}


//==========================================================================
//
//  VObjectMapChannel::Update
//
//==========================================================================
void VObjectMapChannel::Update () {
  //if (!OpenAcked && !needOpenMessage) return; // nothing to do yet (we sent open message, and waiting for the ack)

  if (CurrName >= Connection->ObjMap->NameLookup.length() && CurrClass == Connection->ObjMap->ClassLookup.length()) {
    // everything has been sent
    Close(); // just in case
    return;
  }

  //GCon->Logf(NAME_DevNet, "%s:000: qbytes=%d; outbytes=%d; sum=%d", *GetDebugName(), Connection->SaturaDepth, Connection->Out.GetNumBytes(), Connection->SaturaDepth+Connection->Out.GetNumBytes());

  // do not overflow queue
  if (!CanSendData()) {
    //GCon->Logf(NAME_DevNet, "%s:666: qbytes=%d; outbytes=%d; sum=%d; queued=%d (%d)", *GetDebugName(), Connection->SaturaDepth, Connection->Out.GetNumBytes(), Connection->SaturaDepth+Connection->Out.GetNumBytes(), GetSendQueueSize(), OutListBits);
    UpdateSendPBar();
    return;
  }

  VMessageOut outmsg(this);
  // send counters in the first message
  if (needOpenMessage) {
    needOpenMessage = false;
    outmsg.bOpen = true;
    RNet_PBarReset();
    GCon->Logf(NAME_DevNet, "opened class/name channel for %s", *Connection->GetAddress());
    // send number of names
    vint32 NumNames = Connection->ObjMap->NameLookup.length();
    outmsg.WriteInt(NumNames);
    GCon->Logf(NAME_DevNet, "sending total %d names", NumNames);
    // send number of classes
    vint32 NumClasses = Connection->ObjMap->ClassLookup.length();
    outmsg.WriteInt(NumClasses);
    GCon->Logf(NAME_DevNet, "sending total %d classes", NumClasses);
  }

  VBitStreamWriter strm(MAX_MSG_SIZE_BITS+64, true); // allow expand, why not?

  // send names while we have anything to send
  while (CurrName < Connection->ObjMap->NameLookup.length()) {
    const char *EName = *VName::CreateWithIndex(CurrName);
    int Len = VStr::Length(EName);
    vassert(Len > 0 && Len <= NAME_SIZE);
    strm.WriteInt(Len);
    strm.Serialise((void *)EName, Len);
    // send message if this name will not fit
    if (WillOverflowMsg(&outmsg, strm)) {
      FlushMsg(&outmsg);
      if (net_debug_dump_chan_objmap) GCon->Logf(NAME_DevNet, "  ...names: [%d/%d] (%d)", CurrName+1, Connection->ObjMap->NameLookup.length(), GetSendQueueSize());
      if (!OpenAcked) { UpdateSendPBar(); return; } // if not opened, don't spam with packets yet
      // is queue full?
      if (!CanSendData()) {
        //GCon->Logf(NAME_DevNet, "%s:000: qbytes=%d; outbytes=%d; sum=%d; queued=%d (%d)", *GetDebugName(), Connection->SaturaDepth, Connection->Out.GetNumBytes(), Connection->SaturaDepth+Connection->Out.GetNumBytes(), GetSendQueueSize(), OutListBits);
        UpdateSendPBar();
        return;
      }
    }
    PutStream(&outmsg, strm);
    ++CurrName;
    if (net_debug_dump_chan_objmap) GCon->Logf(NAME_DevNet, "  :name: [%d/%d]: <%s>", CurrName, Connection->ObjMap->NameLookup.length(), EName);
  }

  // send classes while we have anything to send
  while (CurrClass < Connection->ObjMap->ClassLookup.length()) {
    VName Name = Connection->ObjMap->ClassLookup[CurrClass]->GetVName();
    Connection->ObjMap->SerialiseNameNoIntern(strm, Name); // not yet
    // send message if this class will not fit
    if (WillOverflowMsg(&outmsg, strm)) {
      FlushMsg(&outmsg);
      if (net_debug_dump_chan_objmap) GCon->Logf(NAME_DevNet, "  ...classes: [%d/%d] (%d)", CurrClass+1, Connection->ObjMap->ClassLookup.length(), GetSendQueueSize());
      if (!OpenAcked) { UpdateSendPBar(); return; } // if not opened, don't spam with packets yet
      // is queue full?
      if (!CanSendData()) {
        //GCon->Logf(NAME_DevNet, "%s:001: qbytes=%d; outbytes=%d; sum=%d; queued=%d (%d)", *GetDebugName(), Connection->SaturaDepth, Connection->Out.GetNumBytes(), Connection->SaturaDepth+Connection->Out.GetNumBytes(), GetSendQueueSize(), OutListBits);
        UpdateSendPBar();
        return;
      }
    }
    // now internalise it
    Connection->ObjMap->InternName(Name);
    PutStream(&outmsg, strm);
    ++CurrClass;
    if (net_debug_dump_chan_objmap) GCon->Logf(NAME_DevNet, "  :class: [%d/%d]: <%s>", CurrClass, Connection->ObjMap->ClassLookup.length(), *Name);
  }

  // this is the last message
  PutStream(&outmsg, strm);
  FlushMsg(&outmsg);
  Close();

  if (net_fixed_name_set) {
    GCon->Logf(NAME_DevNet, "done writing initial objects (%d) and names (%d)", CurrClass, CurrName);
  } else {
    vassert(CurrName == 1);
    GCon->Logf(NAME_DevNet, "done writing initial objects (%d) and names (%d)", CurrClass, Connection->ObjMap->NewName2Idx.length());
  }
}


//==========================================================================
//
//  VObjectMapChannel::ParseMessage
//
//==========================================================================
void VObjectMapChannel::ParseMessage (VMessageIn &Msg) {
  // if this channel is opened locally, it is used only for sending data
  if (IsLocalChannel()) {
    // if we got ANY message from the remote, something is VERY wrong on the other side!
    if (Msg.bClose) {
      GCon->Logf(NAME_DevNet, "%s: remote closed object map channel, dropping the connection", *GetDebugName());
    } else {
      GCon->Logf(NAME_DevNet, "%s: remote sent something to object map channel, dropping the connection", *GetDebugName());
    }
    Connection->State = NETCON_Closed;
    return;
  }

  // read counters from opening message
  if (Msg.bOpen) {
    RNet_PBarReset();
    RNet_OSDMsgShow("receiving names and classes");

    vint32 NumNames = Msg.ReadInt();
    Connection->ObjMap->SetNumberOfKnownNames(NumNames);
    GCon->Logf(NAME_DevNet, "expecting %d names", NumNames);

    vint32 NumClasses = Msg.ReadInt();
    Connection->ObjMap->ClassLookup.setLength(NumClasses);
    GCon->Logf(NAME_DevNet, "expecting %d classes", NumClasses);
  }

  char buf[NAME_SIZE+1];

  // read names
  if (net_debug_dump_chan_objmap) GCon->Logf(NAME_Debug, "%s: ==== (%d : %d)", *GetDebugName(), CurrName, Connection->ObjMap->NameLookup.length());
  while (!Msg.AtEnd() && CurrName < Connection->ObjMap->NameLookup.length()) {
    int Len = Msg.ReadInt();
    vassert(Len > 0 && Len <= NAME_SIZE);
    Msg.Serialise(buf, Len);
    buf[Len] = 0;
    //GCon->Logf(NAME_Debug, "%s: len=%d (%s)", *GetDebugName(), Len, buf.ptr());
    VName Name(buf);
    Connection->ObjMap->ReceivedName(CurrName, Name);
    ++CurrName;
    if (net_debug_dump_chan_objmap) GCon->Logf(NAME_DevNet, "  :name: [%d/%d]: <%s>", CurrName, Connection->ObjMap->NameLookup.length(), buf);
  }

  // read classes
  while (!Msg.AtEnd() && CurrClass < Connection->ObjMap->ClassLookup.length()) {
    VName Name;
    Connection->ObjMap->SerialiseName(Msg, Name);
    VClass *C = VMemberBase::StaticFindClass(Name);
    vassert(C);
    Connection->ObjMap->ClassLookup[CurrClass] = C;
    Connection->ObjMap->ClassMap.Set(C, CurrClass);
    ++CurrClass;
    if (net_debug_dump_chan_objmap) GCon->Logf(NAME_DevNet, "  :class: [%d/%d]: <%s : %s>", CurrClass, Connection->ObjMap->ClassLookup.length(), *Name, C->GetName());
  }

  if (Msg.bClose) {
    if (CurrName >= Connection->ObjMap->NameLookup.length() && CurrClass >= Connection->ObjMap->ClassLookup.length()) {
      GCon->Logf(NAME_DevNet, "received initial names (%d) and classes (%d)", CurrName, CurrClass);
      Connection->ObjMapSent = true;
    } else {
      GCon->Logf(NAME_DevNet, "...got %d/%d names and %d/%d classes, aborting connection!", CurrName, Connection->ObjMap->NameLookup.length(), CurrClass, Connection->ObjMap->ClassLookup.length());
      Connection->State = NETCON_Closed;
    }
  }

  RNet_PBarUpdate("loading names and classes", CurrName+CurrClass, Connection->ObjMap->NameLookup.length()+Connection->ObjMap->ClassLookup.length(), Msg.bClose/*forced*/);
}
