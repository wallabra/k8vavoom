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
  NameMap.SetNum(VName::GetNumNames());
  NameLookup.SetNum(VName::GetNumNames());
  for (int i = 0; i < VName::GetNumNames(); ++i) {
    NameMap[i] = i;
    NameLookup[i] = VName::CreateWithIndex(i);
  }

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
  if (Strm.IsLoading()) {
    vuint32 NameIndex;
    Strm.SerialiseInt(NameIndex/*, NameLookup.Num()+1*/);
    if ((vint32)NameIndex >= NameLookup.Num()) {
      Name = NAME_None;
    } else {
      Name = NameLookup[NameIndex];
    }
    return true;
  } else {
    const int namecount = VName::GetNumNames();
    check(namecount > 0 && namecount < 1024*1024);
    // update tables
    /*
    if (NameMap.length() < namecount) {
      const int oldcount = NameMap.length();
      GCon->Logf(NAME_Warning, "*** got %d new names", namecount-NameMap.length());
      check(NameMap.length() == NameLookup.length());
      NameMap.SetNum(namecount);
      NameLookup.SetNum(namecount);
      for (int i = oldcount; i < namecount; ++i) {
        NameMap[i] = i;
        NameLookup[i] = VName::CreateWithIndex(i);
      }
    }
    */
    vuint32 NameIndex = (Name.GetIndex() < NameMap.Num() ? NameMap[Name.GetIndex()] : NameLookup.Num());
    Strm.SerialiseInt(NameIndex/*, NameLookup.Num()+1*/);
    return ((vint32)NameIndex != NameLookup.Num());
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
      VChannel *Chan = Connection->Channels[Index];
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
      vuint32 *ClassId = ClassMap.Find((VClass*)State->Outer);
      vuint32 StateId = State->NetId;
      checkSlow(ClassId);
      Strm.SerialiseInt(*ClassId/*, ClassLookup.Num()*/);
      Strm.SerialiseInt(StateId/*, ((VClass *)State->Outer)->StatesLookup.Num()*/);
    } else {
      vuint32 NoClass = 0;
      Strm.SerialiseInt(NoClass/*, ClassLookup.Num()*/);
    }
  }
  return true;
}
