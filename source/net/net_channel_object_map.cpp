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
//  VObjectMapChannel::VObjectMapChannel
//
//==========================================================================
VObjectMapChannel::VObjectMapChannel (VNetConnection *AConnection, vint32 AIndex, vuint8 AOpenedLocally)
  : VChannel(AConnection, CHANNEL_ObjectMap, AIndex, AOpenedLocally)
  , CurrName(0)
  , CurrClass(1)
  , LastNameCount(-1)
  , LastClassCount(-1)
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
  #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
  GCon->Logf(NAME_DevNet, "VObjectMapChannel::Suicide:%p (#%d)", this, Index);
  #endif
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
//  VObjectMapChannel::WriteCounters
//
//==========================================================================
void VObjectMapChannel::WriteCounters (VMessageOut &Msg) {
  bool writeNames;
  bool writeClasses;

  if (Msg.bOpen) {
    writeNames = true;
    writeClasses = true;
  } else {
    writeNames = (LastNameCount != Connection->ObjMap->NameLookup.Num());
    writeClasses = (LastClassCount != Connection->ObjMap->ClassLookup.Num());
    Msg.WriteBit(writeNames);
    Msg.WriteBit(writeClasses);
    if (writeNames) GCon->Logf(NAME_Warning, "+++ %d names", Connection->ObjMap->NameLookup.Num()-LastNameCount);
    if (writeClasses) GCon->Logf(NAME_Warning, "+++ %d classes", Connection->ObjMap->ClassLookup.Num()-LastClassCount);
  }

  if (writeNames) {
    vint32 NumNames = Connection->ObjMap->NameLookup.Num();
    //Msg << NumNames;
    Msg.WriteInt(NumNames);
  }

  if (writeClasses) {
    vint32 NumClasses = Connection->ObjMap->ClassLookup.Num();
    //Msg << NumClasses;
    Msg.WriteInt(NumClasses);
  }

  LastNameCount = Connection->ObjMap->NameLookup.Num();
  LastClassCount = Connection->ObjMap->ClassLookup.Num();
}


//==========================================================================
//
//  VObjectMapChannel::ReadCounters
//
//==========================================================================
void VObjectMapChannel::ReadCounters (VMessageIn &Msg) {
  bool newNames;
  bool newClasses;

  if (Msg.bOpen) {
    newNames = true;
    newClasses = true;
  } else {
    newNames = Msg.ReadBit();
    newClasses = Msg.ReadBit();
  }

  if (newNames) {
    vint32 NumNames = Msg.ReadInt();
    //Msg << NumNames;
    //if (!Msg.bOpen) GCon->Logf("MORE: names=%d (%d)", NumNames, Connection->ObjMap->NameLookup.length());
    vassert(Connection->ObjMap->NameLookup.length() <= NumNames);
    //Connection->ObjMap->NameLookup.SetNum(NumNames);
    Connection->ObjMap->SetNumberOfKnownNames(NumNames);
  }

  if (newClasses) {
    vint32 NumClasses = Msg.ReadInt();
    //Msg << NumClasses;
    //if (!Msg.bOpen) GCon->Logf("MORE: classes=%d (%d)", NumClasses, Connection->ObjMap->ClassLookup.length());
    vassert(Connection->ObjMap->ClassLookup.length() <= NumClasses);
    Connection->ObjMap->ClassLookup.SetNum(NumClasses);
  }

  //if (Msg.bOpen) GCon->Logf("FIRST: names=%d; classes=%d", Connection->ObjMap->NameLookup.length(), Connection->ObjMap->ClassLookup.length());

  LastNameCount = Connection->ObjMap->NameLookup.Num();
  LastClassCount = Connection->ObjMap->ClassLookup.Num();
}


//==========================================================================
//
//  VObjectMapChannel::Update
//
//==========================================================================
void VObjectMapChannel::Update () {
  if (OutMsg && !OpenAcked) return;

  if (CurrName == Connection->ObjMap->NameLookup.Num() && CurrClass == Connection->ObjMap->ClassLookup.Num()) {
    // everything has been sent
    vassert(LastNameCount == Connection->ObjMap->NameLookup.Num());
    vassert(LastClassCount == Connection->ObjMap->ClassLookup.Num());
    return;
  }

  if (CountOutMessages() >= 10) return; // queue is full

  VMessageOut Msg(this);
  Msg.bReliable = true;
  Msg.bOpen = (CurrName == 0); // opening message?

  WriteCounters(Msg);

  // send names while we have anything to send
  while (CurrName < Connection->ObjMap->NameLookup.Num()) {
    //const VNameEntry *E = VName::GetEntry(CurrName);
    const char *EName = *VName::CreateWithIndex(CurrName);
    int Len = VStr::Length(EName);
    // send message if this name will not fit
    if (Msg.GetNumBytes()+1+Len+VBitStreamWriter::CalcIntBits(Len) > OUT_MESSAGE_SIZE/8) {
      SendMessage(&Msg);
      if (!OpenAcked) return;
      if (CountOutMessages() >= 10) return; // queue is full
      //Msg = VMessageOut(this);
      Msg.Setup(this);
      Msg.bReliable = true;
      Msg.bOpen = false;
      WriteCounters(Msg);
    }
    Msg.WriteInt(Len/*, NAME_SIZE*/);
    Msg.Serialise((void *)EName, Len);
    ++CurrName;
  }

  // send classes while we have anything to send
  while (CurrClass < Connection->ObjMap->ClassLookup.Num()) {
    // send message if this class will not fit
    if (Msg.GetNumBytes()+1+VBitStreamWriter::CalcIntBits(VName::GetNumNames()) > OUT_MESSAGE_SIZE/8) {
      SendMessage(&Msg);
      if (CountOutMessages() >= 10) return; // queue is full
      //Msg = VMessageOut(this);
      Msg.Setup(this);
      Msg.bReliable = true;
      Msg.bOpen = false;
      WriteCounters(Msg);
    }
    VName Name = Connection->ObjMap->ClassLookup[CurrClass]->GetVName();
    Connection->ObjMap->SerialiseName(Msg, Name);
    ++CurrClass;
  }

  // this is the last message
  SendMessage(&Msg);
  Close();

  if (net_fixed_name_set) {
    GCon->Logf(NAME_DevNet, "done writing initial objects (%d) and names (%d)", CurrClass, CurrName);
  } else {
    vassert(CurrName == 0);
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
  ReadCounters(Msg);

  TArray<char> buf;

  const bool haveSomeWork =
    CurrName < Connection->ObjMap->NameLookup.Num() ||
    CurrClass < Connection->ObjMap->ClassLookup.Num();

  // read names
  while (!Msg.AtEnd() && CurrName < Connection->ObjMap->NameLookup.Num()) {
    int Len = Msg.ReadInt(/*NAME_SIZE*/);
    buf.setLength(Len+1, false);
    //char buf[NAME_SIZE+1];
    Msg.Serialise(buf.ptr(), Len);
    buf[Len] = 0;
    VName Name(buf.ptr());
    Connection->ObjMap->ReceivedName(CurrName, Name);
    ++CurrName;
  }

  // read classes
  while (!Msg.AtEnd() && CurrClass < Connection->ObjMap->ClassLookup.Num()) {
    VName Name;
    Connection->ObjMap->SerialiseName(Msg, Name);
    VClass *C = VMemberBase::StaticFindClass(Name);
    vassert(C);
    Connection->ObjMap->ClassLookup[CurrClass] = C;
    Connection->ObjMap->ClassMap.Set(C, CurrClass);
    ++CurrClass;
  }

  if (haveSomeWork && CurrName >= Connection->ObjMap->NameLookup.Num() && CurrClass >= Connection->ObjMap->ClassLookup.Num()) {
    GCon->Logf(NAME_DevNet, "received initial names (%d) and classes (%d)", CurrName, CurrClass);
  }
}
