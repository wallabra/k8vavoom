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
#include "../gamedefs.h"
#include "network.h"

static VCvarB net_debug_name_io("net_debug_name_io", false, "Dump name i/o?");
static VCvarB net_debug_send_initial_names("net_debug_send_initial_names", true, "If false, no initial names will be sent.");


//==========================================================================
//
//  VNetObjectsMap::VNetObjectsMap
//
//==========================================================================
VNetObjectsMap::VNetObjectsMap ()
  : Connection(nullptr)
{
}


//==========================================================================
//
//  VNetObjectsMap::VNetObjectsMap
//
//==========================================================================
VNetObjectsMap::VNetObjectsMap (VNetConnection *AConnection)
  : Connection(AConnection)
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
  // 0 is reserved for ''
  // it is possible to send no names at all, but i see no reason to do that
  // 200-300 kb is nothing for the current connections (and mobile connections sux anyway)
  const int nameCount = (net_debug_send_initial_names ? VName::GetNumNames() : min2(16, VName::GetNumNames()));
  NameMap.setLength(nameCount);
  NameLookup.setLength(nameCount);
  for (int i = 0; i < nameCount; ++i) {
    NameMap[i] = i;
    NameLookup[i] = VName::CreateWithIndex(i);
  }
  vassert(NameLookup[0] == NAME_None);

  // collect all possible thinker classes
  ClassLookup.Clear();
  ClassLookup.Append(nullptr); // `None` class
  for (auto &&mbr : VMemberBase::GMembers) {
    if (mbr->MemberType == MEMBER_Class) {
      VClass *C = static_cast<VClass *>(mbr);
      if (C->IsChildOf(VThinker::StaticClass())) {
        ClassMap.Set(C, ClassLookup.length());
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
}


//==========================================================================
//
//  VNetObjectsMap::ReceivedName
//
//  called on reading
//
//==========================================================================
void VNetObjectsMap::ReceivedName (int index, VName Name) {
  if (index < 1) return;
  if (index > 1024*1024) Host_Error(va("invalid received name index %d", index)); // arbitrary limit
  if (index >= NameLookup.length()) {
    vassert(index == NameLookup.length());
    NameLookup.setLength(index+1);
  }
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
//  VNetObjectsMap::SerialiseNameNoIntern
//
//==========================================================================
bool VNetObjectsMap::SerialiseNameNoIntern (VStream &Strm, VName &Name) {
  vassert(!Strm.IsLoading());
  if (Name == NAME_None) {
    // special for empty name
    vuint32 NameIndex = 0;
    Strm << STRM_INDEX_U(NameIndex);
    if (net_debug_name_io) GCon->Logf(NAME_Debug, "sent empty name");
    return true;
  }
  // let's hope that we won't get so much such names
  if (Name.GetIndex() >= NameMap.length()) {
    // new name
    vuint32 NameIndex = 0xffffffffu;
    Strm << STRM_INDEX_U(NameIndex);
    // new name
    const char *EName = *Name;
    vuint32 Len = VStr::Length(EName);
    if (Len == 0 || Len > NAME_SIZE) Sys_Error(va("name '%s' too long!", EName));
    Strm << STRM_INDEX_U(Len);
    Strm.Serialise((void *)EName, Len);
    if (net_debug_name_io) GCon->Logf(NAME_Debug, "sent new name '%s'", EName);
    return true;
  } else {
    // old name
    vassert(Name.GetIndex() < NameMap.length());
    vuint32 NameIndex = NameMap[Name.GetIndex()];
    Strm << STRM_INDEX_U(NameIndex);
    if (net_debug_name_io) GCon->Logf(NAME_Debug, "sent old name '%s' (%d)", *Name, (int)NameIndex);
    return true;
  }
}


//==========================================================================
//
//  VNetObjectsMap::AckNameWithIndex
//
//==========================================================================
void VNetObjectsMap::AckNameWithIndex (int index) {
  if (index < 1 || index >= VName::GetNumNames()) return; // just in case
  const int oldlen = NameMap.length();
  if (index < oldlen) return; // known name
  NameMap.setLength(index+1);
  NameLookup.setLength(index+1);
  for (int i = oldlen; i <= index; ++i) {
    vassert(index < VName::GetNumNames()); // just in case
    NameMap[i] = i;
    NameLookup[i] = VName::CreateWithIndex(i);
  }
}


//==========================================================================
//
//  VNetObjectsMap::SerialiseName
//
//==========================================================================
bool VNetObjectsMap::SerialiseName (VStream &Strm, VName &Name) {
  if (Strm.IsLoading()) {
    // reading name
    vuint32 NameIndex;
    Strm << STRM_INDEX_U(NameIndex);
    if (net_debug_name_io) GCon->Logf(NAME_Debug, "::: got name index %u (0x%08x)", NameIndex, NameIndex);
    // reserved?
    if (NameIndex == 0) {
      Name = NAME_None;
      return true;
    }
    // special?
    if (NameIndex == 0xffffffffu) {
      // new name
      TArray<char> buf;
      vuint32 Len = 0;
      Strm << STRM_INDEX_U(Len);
      if (Len == 0 || Len > NAME_SIZE) Sys_Error("invalid name length: %u", Len);
      buf.setLength(Len+1, false);
      Strm.Serialise(buf.ptr(), Len);
      buf[Len] = 0;
      VName NewName(buf.ptr());
      Name = NewName;
      if (net_debug_name_io) GCon->Logf(NAME_Debug, "got new name '%s'", *NewName);
    } else if (NameIndex >= (vuint32)NameLookup.length()) {
      Name = NAME_None;
      GCon->Logf(NAME_Error, "got invalid name index %d", (int)NameIndex);
    } else {
      Name = NameLookup[NameIndex];
    }
    return true;
  } else {
    // writing name
    const bool res = SerialiseNameNoIntern(Strm, Name);
    return res;
  }
}


//==========================================================================
//
//  VNetObjectsMap::SerialiseObject
//
//==========================================================================
bool VNetObjectsMap::SerialiseObject (VStream &Strm, VObject *&Obj) {
  if (Strm.IsLoading()) {
    // reading
    Obj = nullptr;
    vuint8 IsThinker = 0;
    Strm.SerialiseBits(&IsThinker, 1);
    if (IsThinker) {
      // it's a thinker that has an open channel
      vuint32 Index;
      Strm.SerialiseInt(Index/*, MAX_CHANNELS*/);
      VChannel *Chan = Connection->GetChannelByIndex(Index);
      if (Chan && Chan->IsThinker() && !Chan->Closing) {
        Obj = ((VThinkerChannel *)Chan)->GetThinker();
      }
    }
    return true;
  } else {
    // writing
    VThinker *Thinker = Cast<VThinker>(Obj);
    vuint8 IsThinker = (Thinker ? 1 : 0);
    Strm.SerialiseBits(&IsThinker, 1);
    if (Thinker) {
      // it's a thinker. if it has an open channel we can use it's
      // channel number to identify it, otherwise we can't serialise it
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
    Strm.SerialiseInt(ClassId/*, ClassLookup.length()*/);
    //GCon->Logf(NAME_DevNet, "VNetObjectsMap::SerialiseClass: classid=%u (%d total)", ClassId, ClassLookup.length());
    if (ClassId) {
      Class = ClassLookup[ClassId];
      /*
      if (Class) {
        GCon->Logf(NAME_DevNet, "VNetObjectsMap::SerialiseClass: classid=%u; class is '%s'", ClassId, Class->GetName());
      } else {
        GCon->Logf(NAME_DevNet, "VNetObjectsMap::SerialiseClass: classid=%u; NO SUCH CLASS", ClassId);
      }
      */
    } else {
      Class = nullptr;
    }
  } else {
    if (Class) {
      vuint32 *ClassId = ClassMap.Find(Class);
      Strm.SerialiseInt(*ClassId/*, ClassLookup.length()*/);
    } else {
      vuint32 NoClass = 0;
      Strm.SerialiseInt(NoClass/*, ClassLookup.length()*/);
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
    Strm.SerialiseInt(ClassId/*, ClassLookup.length()*/);
    if (ClassId) {
      vuint32 StateId;
      Strm.SerialiseInt(StateId/*, ClassLookup[ClassId]->StatesLookup.length()*/);
      State = ClassLookup[ClassId]->StatesLookup[StateId];
    } else {
      State = nullptr;
    }
  } else {
    if (State) {
      vuint32 *ClassId = ClassMap.Find((VClass *)State->Outer);
      vuint32 StateId = State->NetId;
      vensure(ClassId);
      Strm.SerialiseInt(*ClassId/*, ClassLookup.length()*/);
      Strm.SerialiseInt(StateId/*, ((VClass *)State->Outer)->StatesLookup.length()*/);
    } else {
      vuint32 NoClass = 0;
      Strm.SerialiseInt(NoClass/*, ClassLookup.length()*/);
    }
  }
  return true;
}
