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
    check(Connection->ObjMap->NameLookup.length() <= NumNames);
    Connection->ObjMap->NameLookup.SetNum(NumNames);
  }

  if (newClasses) {
    vint32 NumClasses = Msg.ReadInt();
    //Msg << NumClasses;
    //if (!Msg.bOpen) GCon->Logf("MORE: classes=%d (%d)", NumClasses, Connection->ObjMap->ClassLookup.length());
    check(Connection->ObjMap->ClassLookup.length() <= NumClasses);
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
    check(LastNameCount == Connection->ObjMap->NameLookup.Num());
    check(LastClassCount == Connection->ObjMap->ClassLookup.Num());
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
      Msg = VMessageOut(this);
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
      Msg = VMessageOut(this);
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
}


//==========================================================================
//
//  VObjectMapChannel::ParsePacket
//
//==========================================================================
void VObjectMapChannel::ParsePacket (VMessageIn &Msg) {
  ReadCounters(Msg);

  // read names
  while (!Msg.AtEnd() && CurrName < Connection->ObjMap->NameLookup.Num()) {
    int Len = Msg.ReadInt(/*NAME_SIZE*/);
    char Buf[NAME_SIZE+1];
    Msg.Serialise(Buf, Len);
    Buf[Len] = 0;
    VName Name = Buf;
    Connection->ObjMap->NameLookup[CurrName] = Name;
    while (Connection->ObjMap->NameMap.Num() <= Name.GetIndex()) {
      Connection->ObjMap->NameMap.Append(-1);
    }
    Connection->ObjMap->NameMap[Name.GetIndex()] = CurrName;
    ++CurrName;
  }

  // read classes
  while (!Msg.AtEnd() && CurrClass < Connection->ObjMap->ClassLookup.Num()) {
    VName Name;
    Connection->ObjMap->SerialiseName(Msg, Name);
    VClass *C = VMemberBase::StaticFindClass(Name);
    check(C);
    Connection->ObjMap->ClassLookup[CurrClass] = C;
    Connection->ObjMap->ClassMap.Set(C, CurrClass);
    ++CurrClass;
  }
}
