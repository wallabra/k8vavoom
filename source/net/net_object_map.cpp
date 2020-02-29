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
//  VNetObjectsMap::VNetObjectsMap
//
//==========================================================================
VNetObjectsMap::VNetObjectsMap ()
  : NewNameFirstIndex(0)
  , Connection(nullptr)
{
}


//==========================================================================
//
//  VNetObjectsMap::VNetObjectsMap
//
//==========================================================================
VNetObjectsMap::VNetObjectsMap (VNetConnection *AConnection)
  : NewNameFirstIndex(0)
  , Connection(AConnection)
{
}


//==========================================================================
//
//  VNetObjectsMap::~VNetObjectsMap
//
//==========================================================================
VNetObjectsMap::~VNetObjectsMap () {
}


//==========================================================================
//
//  VNetObjectsMap::SetupClassLookup
//
//==========================================================================
void VNetObjectsMap::SetupClassLookup () {
  if (net_fixed_name_set) {
    NameMap.setLength(VName::GetNumNames());
    NameLookup.setLength(VName::GetNumNames());
    for (int i = 0; i < VName::GetNumNames(); ++i) {
      NameMap[i] = i;
      NameLookup[i] = VName::CreateWithIndex(i);
    }
  } else {
    NameMap.setLength(0);
    NameLookup.setLength(0);
  }
  int size = 0;
  for (int i = 0; i < VName::GetNumNames(); ++i) size += VStr::length(*VName::CreateWithIndex(i))+1;
  GCon->Logf(NAME_DevNet, "DEBUG: NAME TABLE SIZE (rough): %d", size);

  // 0 is reserved for ''
  NewNameFirstIndex = NameMap.length()+1;

  ClassLookup.Clear();
  ClassLookup.Append(nullptr);
  for (int i = 0; i < VMemberBase::GMembers.Num(); ++i) {
    if (VMemberBase::GMembers[i]->MemberType == MEMBER_Class) {
      VClass *C = static_cast<VClass*>(VMemberBase::GMembers[i]);
      if (C->IsChildOf(VThinker::StaticClass())) {
        ClassMap.Set(C, ClassLookup.Num());
        ClassLookup.Append(C);
      }
    }
  }
}


//==========================================================================
//
//  VNetObjectsMap::SetNumberOfKnownNames
//
//  called on name receiving
//
//==========================================================================
void VNetObjectsMap::SetNumberOfKnownNames (int newlen) {
  vassert(newlen >= 0);
  //NameMap.setLength(newlen);
  NameMap.setLength(0);
  NameLookup.setLength(newlen);
  // 0 is reserved for ''
  NewNameFirstIndex = NameMap.length()+1;
}


//==========================================================================
//
//  VNetObjectsMap::ReceivedName
//
//  called on reading
//
//==========================================================================
void VNetObjectsMap::ReceivedName (int index, VName Name) {
  if (index < 0 || index >= NameLookup.length()) return;
  NameLookup[index] = Name;
  while (NameMap.length() <= Name.GetIndex()) NameMap.append(0);
  NameMap[Name.GetIndex()] = index;
}


//==========================================================================
//
//  VNetObjectsMap::CanSerialiseObject
//
//==========================================================================
bool VNetObjectsMap::CanSerialiseObject (VObject *Obj) {
  VThinker *Thinker = Cast<VThinker>(Obj);
  if (Thinker) {
    // thinker can be serialised only if it has an open channel
    return !!Connection->ThinkerChannels.FindPtr(Thinker);
  } else {
    // we can always serialise nullptr object
    return !Obj;
  }
}


//==========================================================================
//
//  VNetObjectsMap::SerialiseName
//
//==========================================================================
bool VNetObjectsMap::SerialiseName (VStream &Strm, VName &Name) {
  vassert(NewNameFirstIndex > 0);
  if (Strm.IsLoading()) {
    // reading name
    vuint32 NameIndex;
    Strm.SerialiseInt(NameIndex);
    // reserved?
    if (NameIndex == 0) {
      Name = NAME_None;
      return true;
    }
    // special?
    if (NameIndex == 0xffffffffu) {
      // new index
      Strm.SerialiseInt(NameIndex);
      // new name
      TArray<char> buf;
      vuint32 Len = 0;
      Strm.SerialiseInt(Len);
      buf.setLength(Len+1, false);
      //char buf[NAME_SIZE+1];
      Strm.Serialise(buf.ptr(), Len);
      buf[Len] = 0;
      VName NewName(buf.ptr());
      Name = NewName;
      // append it
      NewName2Idx.put(Name, NameIndex);
      NewIdx2Name.put(NameIndex, Name);
      if (net_debug_fixed_name_set) GCon->Logf(NAME_Debug, "got new name '%s' (%d)", *NewName, (int)NameIndex);
    } else if ((vint32)NameIndex >= NameLookup.Num()) {
      // try known "new name"
      auto nip = NewIdx2Name.find(NameIndex);
      if (nip) {
        Name = *nip;
        if (net_debug_fixed_name_set) GCon->Logf(NAME_Debug, "got old new name '%s' (%d)", *Name, (int)NameIndex);
      } else {
        Name = NAME_None;
        GCon->Logf(NAME_Error, "got invalid name index %d", (int)NameIndex);
      }
    } else {
      Name = NameLookup[NameIndex];
    }
    return true;
  } else {
    // writing name
    if (Name == NAME_None) {
      // special for empty name
      vuint32 NameIndex = 0;
      Strm.SerialiseInt(NameIndex);
      return true;
    }
    //const int namecount = VName::GetNumNames();
    //vassert(namecount > 0 && namecount < 1024*1024);
    // note that code can create names on the fly, and we need to transmit them
    if (Name.GetIndex() >= NameMap.length()) {
      // new name?
      auto nip = NewName2Idx.find(Name);
      if (nip) {
        // already seen
        vuint32 NameIndex = (vuint32)(*nip);
        Strm.SerialiseInt(NameIndex);
        if (net_debug_fixed_name_set) GCon->Logf(NAME_Debug, "sent old new name '%s' (%d)", *Name, (int)NameIndex);
      } else {
        // new name
        int newIndex = NewNameFirstIndex+NewName2Idx.length();
        NewName2Idx.put(Name, newIndex);
        NewIdx2Name.put(newIndex, Name);
        vuint32 special = 0xffffffffu;
        Strm.SerialiseInt(special);
        // new index
        special = (vuint32)newIndex;
        Strm.SerialiseInt(special);
        // new name
        const char *EName = *Name;
        vuint32 Len = VStr::Length(EName);
        Strm.SerialiseInt(Len);
        Strm.Serialise((void *)EName, Len);
        if (net_debug_fixed_name_set) GCon->Logf(NAME_Debug, "sent new name '%s' (%d)", EName, newIndex);
      }
      return true;
    } else {
      // old name
      vassert(Name.GetIndex() < NameMap.length());
      vuint32 NameIndex = (Name.GetIndex() < NameMap.Num() ? NameMap[Name.GetIndex()] : /*NameLookup.Num()*/0);
      Strm.SerialiseInt(NameIndex/*, NameLookup.Num()+1*/);
      return ((vint32)NameIndex != NameLookup.Num());
    }
  }
}


//==========================================================================
//
//  VNetObjectsMap::SerialiseObject
//
//==========================================================================
bool VNetObjectsMap::SerialiseObject (VStream &Strm, VObject *&Obj) {
  if (Strm.IsLoading()) {
    Obj = nullptr;
    vuint8 IsThinker = 0;
    Strm.SerialiseBits(&IsThinker, 1);
    if (IsThinker) {
      // it's a thinker that has an open channel
      vuint32 Index;
      Strm.SerialiseInt(Index/*, MAX_CHANNELS*/);
      //VChannel *Chan = Connection->Channels[Index];
      VChannel *Chan = Connection->GetChannelByIndex(Index);
      if (Chan && Chan->Type == CHANNEL_Thinker && !Chan->Closing) {
        Obj = ((VThinkerChannel *)Chan)->Thinker;
      }
    }
    return true;
  } else {
    VThinker *Thinker = Cast<VThinker>(Obj);
    vuint8 IsThinker = (Thinker ? 1 : 0);
    Strm.SerialiseBits(&IsThinker, 1);
    if (Thinker) {
      // it's a thinker. if it has an open channel we can use it's
      // channel number to identify it, otherwise we can't serialise it.
      bool Ret = false;
      vuint32 Index = 0;
      VThinkerChannel *Chan = Connection->ThinkerChannels.FindPtr(Thinker);
      if (Chan) {
        Index = Chan->Index;
        Ret = (Chan->OpenAcked ? true : false);
      }
      Strm.SerialiseInt(Index/*, MAX_CHANNELS*/);
      return Ret;
    }
    return !Obj;
  }
}


//==========================================================================
//
//  VNetObjectsMap::SerialiseClass
//
//==========================================================================
bool VNetObjectsMap::SerialiseClass (VStream &Strm, VClass *&Class) {
  if (Strm.IsLoading()) {
    vuint32 ClassId;
    Strm.SerialiseInt(ClassId/*, ClassLookup.Num()*/);
    //GCon->Logf("classid=%u (%d)", ClassId, ClassLookup.Num());
    if (ClassId) {
      Class = ClassLookup[ClassId];
    } else {
      Class = nullptr;
    }
  } else {
    if (Class) {
      vuint32 *ClassId = ClassMap.Find(Class);
      Strm.SerialiseInt(*ClassId/*, ClassLookup.Num()*/);
    } else {
      vuint32 NoClass = 0;
      Strm.SerialiseInt(NoClass/*, ClassLookup.Num()*/);
    }
  }
  return true;
}


//==========================================================================
//
//  VNetObjectsMap::SerialiseState
//
//==========================================================================
bool VNetObjectsMap::SerialiseState (VStream &Strm, VState *&State) {
  if (Strm.IsLoading()) {
    vuint32 ClassId;
    Strm.SerialiseInt(ClassId/*, ClassLookup.Num()*/);
    if (ClassId) {
      vuint32 StateId;
      Strm.SerialiseInt(StateId/*, ClassLookup[ClassId]->StatesLookup.Num()*/);
      State = ClassLookup[ClassId]->StatesLookup[StateId];
    } else {
      State = nullptr;
    }
  } else {
    if (State) {
      vuint32 *ClassId = ClassMap.Find((VClass *)State->Outer);
      vuint32 StateId = State->NetId;
      vensure(ClassId);
      Strm.SerialiseInt(*ClassId/*, ClassLookup.Num()*/);
      Strm.SerialiseInt(StateId/*, ((VClass *)State->Outer)->StatesLookup.Num()*/);
    } else {
      vuint32 NoClass = 0;
      Strm.SerialiseInt(NoClass/*, ClassLookup.Num()*/);
    }
  }
  return true;
}
