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
  Connection->ObjMapSent = true;
  //GCon->Logf(NAME_DevNet, "initial objects and names sent!");
}


//==========================================================================
//
//  VObjectMapChannel::Suicide
//
//==========================================================================
void VObjectMapChannel::Suicide () {
  VChannel::Suicide();
  Closing = true; // just in case
  ClearAllQueues();
  if (Index >= 0 && Index < MAX_CHANNELS && Connection) {
    Connection->UnregisterChannel(this);
    Index = -1; // just in case
  }
  //if (!Connection->ObjMapSent) GCon->Logf(NAME_DevNet, "VObjectMapChannel::DONE:%p (#%d)", this, Index);
  Connection->ObjMapSent = true;
}


//==========================================================================
//
//  VObjectMapChannel::ReceivedClosingAck
//
//  some channels may want to set some flags here
//
//  WARNING! don't close/suicide here!
//
//==========================================================================
void VObjectMapChannel::ReceivedClosingAck () {
  //GCon->Logf(NAME_DevNet, "VObjectMapChannel::ReceivedClosingAck:%p (#%d)", this, Index);
  Connection->ObjMapSent = true;
  GCon->Logf(NAME_DevNet, "initial objects and names sent");
}


//==========================================================================
//
//  VObjectMapChannel::Tick
//
//==========================================================================
void VObjectMapChannel::Tick () {
  VChannel::Tick();
  if (OpenedLocally) Update();
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
  if (OutMsg && !OpenAcked) return;

  if (CurrName == Connection->ObjMap->NameLookup.length() && CurrClass == Connection->ObjMap->ClassLookup.length()) {
    // everything has been sent
    Close();
    return;
  }

  // do not overflow queue
  if (CountOutMessages() >= 10) {
    UpdateSendPBar();
    return;
  }

  VMessageOut Msg(this, true/*reliable*/);
  Msg.bOpen = needOpenMessage; // opening message?
  // send counters in the first message
  if (needOpenMessage) {
    needOpenMessage = false;
    RNet_PBarReset();
    GCon->Logf(NAME_DevNet, "opened class/name channel for %s", *Connection->GetAddress());
    // send number of names
    vint32 NumNames = Connection->ObjMap->NameLookup.length();
    Msg.WriteInt(NumNames);
    GCon->Logf(NAME_DevNet, "sending total %d names", NumNames);
    // send number of classes
    vint32 NumClasses = Connection->ObjMap->ClassLookup.length();
    Msg.WriteInt(NumClasses);
    GCon->Logf(NAME_DevNet, "sending total %d classes", NumClasses);
  }

  // send names while we have anything to send
  while (CurrName < Connection->ObjMap->NameLookup.length()) {
    //const VNameEntry *E = VName::GetEntry(CurrName);
    const char *EName = *VName::CreateWithIndex(CurrName);
    int Len = VStr::Length(EName);
    vassert(Len > 0 && Len <= NAME_SIZE);
    // send message if this name will not fit
    if (Msg.GetNumBytes()+1+Len+VBitStreamWriter::CalcIntBits(Len) > OUT_MESSAGE_SIZE/8) {
      SendMessage(&Msg);
      if (!OpenAcked) { UpdateSendPBar(); return; } // if not opened, don't spam with packets yet
      if (CountOutMessages() >= 10) { UpdateSendPBar(); return; } // is queue full?
      // clear message
      if (net_debug_dump_chan_objmap) GCon->Logf(NAME_DevNet, "  ...names: [%d/%d] (%d)", CurrName+1, Connection->ObjMap->NameLookup.length(), CountOutMessages());
      Msg.Setup(this, true/*reliable*/);
      Msg.bOpen = false;
    }
    Msg.WriteInt(Len);
    Msg.Serialise((void *)EName, Len);
    ++CurrName;
    //if (net_debug_dump_chan_objmap) GCon->Logf(NAME_DevNet, "  :name: [%d/%d]: <%s>", CurrName, Connection->ObjMap->NameLookup.length(), EName);
  }

  // send classes while we have anything to send
  while (CurrClass < Connection->ObjMap->ClassLookup.length()) {
    // send message if this class will not fit
    if (Msg.GetNumBytes()+1+VBitStreamWriter::CalcIntBits(VName::GetNumNames()) > OUT_MESSAGE_SIZE/8) {
      SendMessage(&Msg);
      if (!OpenAcked) { UpdateSendPBar(); return; } // if not opened, don't spam with packets yet
      if (CountOutMessages() >= 10) { UpdateSendPBar(); return; } // is queue full?
      // clear message
      if (net_debug_dump_chan_objmap) GCon->Logf(NAME_DevNet, "  ...classes: [%d/%d] (%d)", CurrClass+1, Connection->ObjMap->ClassLookup.length(), CountOutMessages());
      Msg.Setup(this, true/*reliable*/);
      Msg.bOpen = false;
    }
    VName Name = Connection->ObjMap->ClassLookup[CurrClass]->GetVName();
    Connection->ObjMap->SerialiseName(Msg, Name);
    ++CurrClass;
    //if (net_debug_dump_chan_objmap) GCon->Logf(NAME_DevNet, "  :class: [%d/%d]: <%s>", CurrClass, Connection->ObjMap->ClassLookup.length(), *Name);
  }

  // this is the last message
  SendMessage(&Msg);
  Close();

  if (net_fixed_name_set) {
    GCon->Logf(NAME_DevNet, "done writing initial objects (%d) and names (%d)", CurrClass, CurrName);
  } else {
    vassert(CurrName == 1);
    GCon->Logf(NAME_DevNet, "done writing initial objects (%d) and names (%d)", CurrClass, Connection->ObjMap->NewName2Idx.length());
  }
  //Connection->ObjMapSent = true;
}


//==========================================================================
//
//  VObjectMapChannel::ParsePacket
//
//==========================================================================
void VObjectMapChannel::ParsePacket (VMessageIn &Msg) {
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

  const bool wasEmpty = Msg.AtEnd(); // for console messages

  TArray<char> buf;

  // read names
  while (!Msg.AtEnd() && CurrName < Connection->ObjMap->NameLookup.length()) {
    int Len = Msg.ReadInt();
    vassert(Len > 0 && Len <= NAME_SIZE);
    buf.setLength(Len+1, false);
    Msg.Serialise(buf.ptr(), Len);
    buf[Len] = 0;
    VName Name(buf.ptr());
    Connection->ObjMap->ReceivedName(CurrName, Name);
    ++CurrName;
    //if (net_debug_dump_chan_objmap) GCon->Logf(NAME_DevNet, "  :name: [%d/%d]: <%s>", CurrName, Connection->ObjMap->NameLookup.length(), buf.ptr());
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
    //if (net_debug_dump_chan_objmap) GCon->Logf(NAME_DevNet, "  :class: [%d/%d]: <%s : %s>", CurrClass, Connection->ObjMap->ClassLookup.length(), *Name, C->GetName());
  }

  RNet_PBarUpdate("loading names and classes", CurrName+CurrClass, Connection->ObjMap->NameLookup.length()+Connection->ObjMap->ClassLookup.length());

  if (!wasEmpty) {
    if (CurrName >= Connection->ObjMap->NameLookup.length() && CurrClass >= Connection->ObjMap->ClassLookup.length()) {
      GCon->Logf(NAME_DevNet, "received initial names (%d) and classes (%d)", CurrName, CurrClass);
    } else {
      if (net_debug_dump_chan_objmap) GCon->Logf(NAME_DevNet, "...got %d names and %d classes so far", CurrName, CurrClass);
    }
  }
}
