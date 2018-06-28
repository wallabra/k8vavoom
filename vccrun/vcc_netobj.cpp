//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

#include "vcc_run.h"
//#include "gamedefs.h"
//#include "network.h"


//==========================================================================
//
//  VNetObjectsMap::VNetObjectsMap
//
//==========================================================================
VNetObjectsMap::VNetObjectsMap () : Connection(nullptr) {
}


//==========================================================================
//
//  VNetObjectsMap::VNetObjectsMap
//
//==========================================================================
VNetObjectsMap::VNetObjectsMap (VNetConnection *AConnection) : Connection(AConnection) {
}


//==========================================================================
//
//  VNetObjectsMap::SetUpClassLookup
//
//==========================================================================
void VNetObjectsMap::SetUpClassLookup () {
  guard(VNetObjectsMap::SetUpClassLookup);
  NameMap.SetNum(VName::GetNumNames());
  NameLookup.SetNum(VName::GetNumNames());
  for (int i = 0; i < VName::GetNumNames(); ++i) {
    NameMap[i] = i;
    NameLookup[i] = *(VName *)&i;
  }

  ClassLookup.clear();
  ClassLookup.append(nullptr);
  for (int i = 0; i < VMemberBase::GMembers.length(); ++i) {
    if (VMemberBase::GMembers[i]->MemberType == MEMBER_Class) {
      VClass *cls = static_cast<VClass *>(VMemberBase::GMembers[i]);
      if (cls->IsChildOf(/*VThinker*/VObject::StaticClass())) {
        ClassMap.Set(cls, ClassLookup.length());
        ClassLookup.Append(cls);
      }
    }
  }
  unguard;
}


//==========================================================================
//
//  VNetObjectsMap::CanSerialiseObject
//
//==========================================================================
bool VNetObjectsMap::CanSerialiseObject (VObject *obj) {
  //VThinker *Thinker = Cast<VThinker>(Obj);
  VObject *Thinker = obj;
  if (Thinker) {
    // thinker can be serialised only if it has an open channel
    //return !!Connection->ThinkerChannels.FindPtr(Thinker);
    return true; //FIXME
  } else {
    // we can always serialise nullptr object
    return !obj;
  }
}


//==========================================================================
//
//  VNetObjectsMap::SerialiseName
//
//==========================================================================
bool VNetObjectsMap::SerialiseName (VStream &Strm, VName &Name) {
  guard(VNetObjectsMap::SerialiseName);
  if (Strm.IsLoading()) {
    vuint32 NameIndex;
    Strm.SerialiseInt(NameIndex, NameLookup.length()+1);
    if ((vint32)NameIndex == NameLookup.length()) {
      Name = NAME_None;
    } else {
      Name = NameLookup[NameIndex];
    }
    return true;
  } else {
    vuint32 NameIndex = (Name.GetIndex() < NameMap.length() ? NameMap[Name.GetIndex()] : NameLookup.length());
    Strm.SerialiseInt(NameIndex, NameLookup.length()+1);
    return ((vint32)NameIndex != NameLookup.length());
  }
  unguard;
}


//==========================================================================
//
//  VNetObjectsMap::SerialiseObject
//
//==========================================================================
bool VNetObjectsMap::SerialiseObject (VStream &Strm, VObject *&obj) {
  guard(VNetObjectsMap::SerialiseObject);
#if 0
  if (Strm.IsLoading()) {
    obj = nullptr;
    vuint8 IsThinker = 0;
    Strm.SerialiseBits(&IsThinker, 1);
    if (IsThinker) {
      // it's a thinker that has an open channel
      vuint32 Index;
      Strm.SerialiseInt(Index, MAX_CHANNELS);
      VChannel *Chan = Connection->Channels[Index];
      if (Chan && Chan->Type == CHANNEL_Thinker && !Chan->Closing) {
        obj = ((VThinkerChannel *)Chan)->Thinker;
      }
    }
    return true;
  } else {
    VThinker *Thinker = Cast<VThinker>(obj);
    vuint8 IsThinker = !!Thinker;
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
      Strm.SerialiseInt(Index, MAX_CHANNELS);
      return Ret;
    }
    return !obj;
  }
#else
  obj = nullptr;
  return false;
#endif
  unguard;
}


//==========================================================================
//
//  VNetObjectsMap::SerialiseClass
//
//==========================================================================
bool VNetObjectsMap::SerialiseClass (VStream &Strm, VClass *&Class) {
  guard(VNetObjectsMap::SerialiseClass);
  if (Strm.IsLoading()) {
    vuint32 ClassId;
    Strm.SerialiseInt(ClassId, ClassLookup.length());
    if (ClassId) {
      Class = ClassLookup[ClassId];
    } else {
      Class = nullptr;
    }
  } else {
    if (Class) {
      vuint32 *ClassId = ClassMap.Find(Class);
      Strm.SerialiseInt(*ClassId, ClassLookup.length());
    } else {
      vuint32 NoClass = 0;
      Strm.SerialiseInt(NoClass, ClassLookup.length());
    }
  }
  return true;
  unguard;
}


//==========================================================================
//
//  VNetObjectsMap::SerialiseState
//
//==========================================================================
bool VNetObjectsMap::SerialiseState (VStream &Strm, VState *&State) {
  guard(VNetObjectsMap::SerialiseState);
  if (Strm.IsLoading()) {
    vuint32 ClassId;
    Strm.SerialiseInt(ClassId, ClassLookup.length());
    if (ClassId) {
      vuint32 StateId;
      Strm.SerialiseInt(StateId, ClassLookup[ClassId]->StatesLookup.length());
      State = ClassLookup[ClassId]->StatesLookup[StateId];
    } else {
      State = nullptr;
    }
  } else {
    if (State) {
      vuint32 *ClassId = ClassMap.Find((VClass*)State->Outer);
      vuint32 StateId = State->NetId;
      checkSlow(ClassId);
      Strm.SerialiseInt(*ClassId, ClassLookup.length());
      Strm.SerialiseInt(StateId, ((VClass *)State->Outer)->StatesLookup.length());
    } else {
      vuint32 NoClass = 0;
      Strm.SerialiseInt(NoClass, ClassLookup.length());
    }
  }
  return true;
  unguard;
}
