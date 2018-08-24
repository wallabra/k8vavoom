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

#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
# include "gamedefs.h"
# include "net/network.h"
#else
# if defined(IN_VCC)
#  include "../../utils/vcc/vcc.h"
# elif defined(VCC_STANDALONE_EXECUTOR)
#  include "../../vccrun/vcc_run.h"
# endif
#endif


// register a class at startup time
VClass VObject::PrivateStaticClass (
  EC_NativeConstructor,
  sizeof(VObject),
  VObject::StaticClassFlags,
  nullptr,
  NAME_Object,
  VObject::InternalConstructor
);
VClass *autoclassVObject = VObject::StaticClass();

bool VObject::GObjInitialised = false;
TArray<VObject*> VObject::GObjObjects;
TArray<int> VObject::GObjAvailable;
VObject *VObject::GObjHash[4096];
int VObject::GNumDeleted = 0;
bool VObject::GInGarbageCollection = false;
void *VObject::GNewObject = nullptr;
#ifdef VCC_STANDALONE_EXECUTOR
bool VObject::GImmediadeDelete = true;
#endif
bool VObject::GGCMessagesAllowed = false;
bool (*VObject::onExecuteNetMethodCB) (VObject *obj, VMethod *func) = nullptr; // return `false` to do normal execution


//==========================================================================
//
//  VScriptIterator::Finished
//
//==========================================================================
void VScriptIterator::Finished () {
  delete this;
}


//==========================================================================
//
//  VObject::VObject
//
//==========================================================================
VObject::VObject () {
}


//==========================================================================
//
//  VObject::~VObject
//
//==========================================================================
VObject::~VObject () {
  //guard(VObject::~VObject);

  ConditionalDestroy();
  --GNumDeleted;
  if (!GObjInitialised) return;

  if (!GInGarbageCollection) {
    //fprintf(stderr, "Cleaning up for `%s`\n", *this->GetClass()->Name);
    SetFlags(_OF_CleanupRef);
    for (int i = 0; i < GObjObjects.Num(); ++i) {
      VObject *Obj = GObjObjects[i];
      if (!Obj || (Obj->GetFlags()&_OF_Destroyed)) continue;
      Obj->GetClass()->CleanObject(Obj);
    }
  }

  if (Index == GObjObjects.Num()-1) {
    GObjObjects.RemoveIndex(Index);
  } else {
    GObjObjects[Index] = nullptr;
    GObjAvailable.Append(Index);
  }

  //unguard;
}


//==========================================================================
//
//  VObject::operator new
//
//==========================================================================
void *VObject::operator new (size_t) {
  check(GNewObject);
  return GNewObject;
}

//==========================================================================
//
//  VObject::operator new
//
//==========================================================================
void *VObject::operator new (size_t, const char *, int) {
  check(GNewObject);
  return GNewObject;
}


//==========================================================================
//
//  VObject::operator delete
//
//==========================================================================
void VObject::operator delete (void *Object) {
  Z_Free(Object);
}


//==========================================================================
//
//  VObject::operator delete
//
//==========================================================================
void VObject::operator delete (void *Object, const char *, int) {
  Z_Free(Object);
}


//==========================================================================
//
//  VObject::StaticInit
//
//==========================================================================
void VObject::StaticInit () {
  VMemberBase::StaticInit();
  GObjInitialised = true;
}


//==========================================================================
//
//  VObject::StaticExit
//
//==========================================================================
void VObject::StaticExit () {
  for (int i = 0; i < GObjObjects.Num(); ++i) if (GObjObjects[i]) GObjObjects[i]->ConditionalDestroy();
  CollectGarbage();
  GObjObjects.Clear();
  GObjAvailable.Clear();
  GObjInitialised = false;
  VMemberBase::StaticExit();
}


//==========================================================================
//
//  VObject::StaticSpawnObject
//
//==========================================================================
VObject *VObject::StaticSpawnObject (VClass *AClass, bool skipReplacement) {
  guard(VObject::StaticSpawnObject);
  check(AClass);

  // actually, spawn a replacement
  if (!skipReplacement) AClass = AClass->GetReplacement();

  // allocate memory
  VObject *Obj = (VObject *)Z_Calloc(AClass->ClassSize);

  // copy values from the default object
  check(AClass->Defaults);
  AClass->CopyObject(AClass->Defaults, (vuint8 *)Obj);

  // find native class
  VClass *NativeClass = AClass;
  while (NativeClass != nullptr && !(NativeClass->ObjectFlags&CLASSOF_Native)) {
    NativeClass = NativeClass->GetSuperClass();
  }
  check(NativeClass);

  // call constructor of the native class to set up C++ virtual table
  GNewObject = Obj;
  NativeClass->ClassConstructor();
  GNewObject = nullptr;

  // set up object fields
  Obj->Class = AClass;
  Obj->vtable = AClass->ClassVTable;
  Obj->Register();

  // we're done
  return Obj;
  unguardf(("%s", AClass ? AClass->GetName() : "nullptr"));
}


//==========================================================================
//
//  VObject::Register
//
//==========================================================================
void VObject::Register () {
  guard(VObject::Register);
  if (GObjAvailable.Num()) {
    Index = GObjAvailable[GObjAvailable.Num()-1];
    GObjAvailable.RemoveIndex(GObjAvailable.Num()-1);
    GObjObjects[Index] = this;
  } else {
    Index = GObjObjects.Append(this);
  }
  unguard;
}


//==========================================================================
//
//  VObject::ConditionalDestroy
//
//==========================================================================
bool VObject::ConditionalDestroy () {
  if (!(ObjectFlags&_OF_Destroyed)) {
    ++GNumDeleted;
    SetFlags(_OF_Destroyed);
    Destroy();
  }
  return true;
}


//==========================================================================
//
//  VObject::Destroy
//
//==========================================================================
void VObject::Destroy () {
  Class->DestructObject(this);
}


//==========================================================================
//
//  VObject::IsA
//
//==========================================================================
bool VObject::IsA (VClass *SomeBaseClass) const {
  for (const VClass *c = Class; c; c = c->GetSuperClass()) if (SomeBaseClass == c) return true;
  return false;
}


//==========================================================================
//
//  VObject::GetVFunction
//
//==========================================================================
VMethod *VObject::GetVFunction (VName FuncName) const {
  guardSlow(VObject::GetVFunction);
  return vtable[Class->GetMethodIndex(FuncName)];
  unguardSlow;
}


//==========================================================================
//
//  VObject::ClearReferences
//
//==========================================================================
void VObject::ClearReferences () {
  guard(VObject::ClearReferences);
  GetClass()->CleanObject(this);
  unguard;
}


//==========================================================================
//
//  VObject::CollectGarbage
//
//==========================================================================
void VObject::CollectGarbage (bool destroyDelayed) {
  guard(VObject::CollectGarbage);

  if (!GNumDeleted && !destroyDelayed) return;

  GInGarbageCollection = true;

  // destroy all delayed-destroy objects
  if (destroyDelayed) {
    for (int i = 0; i < GObjObjects.length(); ++i) {
      VObject *Obj = GObjObjects[i];
      if (!Obj) continue;
      if ((Obj->GetFlags()&(_OF_Destroyed|_OF_DelayedDestroy)) == _OF_DelayedDestroy) {
        Obj->ConditionalDestroy();
      }
    }
    if (!GNumDeleted) {
      GInGarbageCollection = false;
      return;
    }
  }

  // mark objects to be cleaned
  for (int i = 0; i < GObjObjects.Num(); ++i) {
    VObject *Obj = GObjObjects[i];
    if (!Obj) continue;
    if (Obj->GetFlags()&_OF_Destroyed) Obj->SetFlags(_OF_CleanupRef);
  }

  // clean references
  for (int i = 0; i < GObjObjects.Num(); ++i) {
    VObject *Obj = GObjObjects[i];
    if (!Obj || (Obj->GetFlags()&_OF_Destroyed)) continue;
    Obj->ClearReferences();
  }

  // now actually delete the objects
  int count = 0;
  for (int i = 0; i < GObjObjects.Num(); ++i) {
    VObject *Obj = GObjObjects[i];
    if (!Obj) continue;
    if (Obj->GetFlags()&_OF_Destroyed) {
      ++count;
      delete Obj;
    }
  }
#if !defined(IN_VCC) || defined(VCC_STANDALONE_EXECUTOR)
  if (GGCMessagesAllowed) {
#if defined(VCC_STANDALONE_EXECUTOR)
    fprintf(stderr, "GC: %d objects deleted\n", count);
#else
    GCon->Logf("GC: %d objects deleted", count);
#endif
  }
#endif

  GInGarbageCollection = false;
  unguard;
}


//==========================================================================
//
//  VObject::GetIndexObject
//
//==========================================================================
VObject *VObject::GetIndexObject (int Index) {
  return GObjObjects[Index];
}


//==========================================================================
//
//  VObject::GetObjectsCount
//
//==========================================================================
int VObject::GetObjectsCount () {
  return GObjObjects.Num();
}


//==========================================================================
//
//  VObject::Serialise
//
//==========================================================================
void VObject::Serialise (VStream &Strm) {
  guard(VObject::Serialise);
  GetClass()->SerialiseObject(Strm, this);
  unguard;
}


//==========================================================================
//
//  VObject::ExecuteNetMethod
//
//==========================================================================
bool VObject::ExecuteNetMethod (VMethod *func) {
  if (onExecuteNetMethodCB) return onExecuteNetMethodCB(this, func);
  return false;
}


// ////////////////////////////////////////////////////////////////////////// //
class VClassesIterator : public VScriptIterator {
private:
  VClass *BaseClass;
  VClass **Out;
  int Index;

public:
  VClassesIterator (VClass *ABaseClass, VClass **AOut) : BaseClass(ABaseClass), Out(AOut), Index(0) {}

  bool GetNext () {
    while (Index < VMemberBase::GMembers.Num()) {
      VMemberBase *Check = VMemberBase::GMembers[Index];
      ++Index;
      if (Check->MemberType == MEMBER_Class && ((VClass*)Check)->IsChildOf(BaseClass)) {
        *Out = (VClass*)Check;
        return true;
      }
    }
    *Out = nullptr;
    return false;
  }
};

// ////////////////////////////////////////////////////////////////////////// //
class VClassStatesIterator : public VScriptIterator {
private:
  VState *curr;
  VState **out;

public:
  VClassStatesIterator (VClass *aclass, VState **aout) : curr(nullptr), out(aout) {
    if (aclass) curr = aclass->States;
  }

  bool GetNext () {
    *out = curr;
    if (curr) {
      curr = curr->Next;
      return true;
    }
    return false;
  }
};

//==========================================================================
//
//  Iterators
//
//==========================================================================
class VObjectsIterator : public VScriptIterator {
private:
  VClass *BaseClass;
  VObject **Out;
  int Index;

public:
  VObjectsIterator (VClass *ABaseClass, VObject **AOut) : BaseClass(ABaseClass), Out(AOut), Index(0) {}

  bool GetNext () {
    while (Index < VObject::GetObjectsCount()) {
      VObject *Check = VObject::GetIndexObject(Index);
      ++Index;
      if (Check != nullptr && !(Check->GetFlags()&_OF_DelayedDestroy) && Check->IsA(BaseClass)) {
        *Out = Check;
        return true;
      }
    }
    *Out = nullptr;
    return false;
  }
};

//==========================================================================
//
//  VObject::NameFromVKey
//
//==========================================================================
VStr VObject::NameFromVKey (int vkey) {
  if (vkey >= K_N0 && vkey <= K_N9) return VStr(char(vkey));
  if (vkey >= K_a && vkey <= K_z) return VStr(char(vkey-32));
  switch (vkey) {
    case K_ESCAPE: return "ESCAPE";
    case K_ENTER: return "ENTER";
    case K_TAB: return "TAB";
    case K_BACKSPACE: return "BACKSPACE";

    case K_SPACE: return "SPACE";

    case K_UPARROW: return "UP";
    case K_LEFTARROW: return "LEFT";
    case K_RIGHTARROW: return "RIGHT";
    case K_DOWNARROW: return "DOWN";
    case K_INSERT: return "INSERT";
    case K_DELETE: return "DELETE";
    case K_HOME: return "HOME";
    case K_PAGEUP: return "PGUP";
    case K_PAGEDOWN: return "PGDOWN";

    case K_PAD0: return "PAD0";
    case K_PAD1: return "PAD1";
    case K_PAD2: return "PAD2";
    case K_PAD3: return "PAD3";
    case K_PAD4: return "PAD4";
    case K_PAD5: return "PAD5";
    case K_PAD6: return "PAD6";
    case K_PAD7: return "PAD7";
    case K_PAD8: return "PAD8";
    case K_PAD9: return "PAD9";

    case K_NUMLOCK: return "NUM";
    case K_PADDIVIDE: return "PAD/";
    case K_PADMULTIPLE: return "PAD*";
    case K_PADMINUS: return "PAD-";
    case K_PADPLUS: return "PAD+";
    case K_PADENTER: return "PADENTER";
    case K_PADDOT: return "PAD.";

    case K_CAPSLOCK: return "CAPS";
    case K_BACKQUOTE: return "BACKQUOTE";

    case K_F1: return "F1";
    case K_F2: return "F2";
    case K_F3: return "F3";
    case K_F4: return "F4";
    case K_F5: return "F5";
    case K_F6: return "F6";
    case K_F7: return "F7";
    case K_F8: return "F8";
    case K_F9: return "F9";
    case K_F10: return "F10";
    case K_F11: return "F11";
    case K_F12: return "F12";

    case K_LSHIFT: return "LSHIFT";
    case K_RSHIFT: return "RSHIFT";
    case K_LCTRL: return "LCTRL";
    case K_RCTRL: return "RCTRL";
    case K_LALT: return "LALT";
    case K_RALT: return "RALT";

    case K_LWIN: return "LWIN";
    case K_RWIN: return "RWIN";
    case K_MENU: return "MENU";

    case K_PRINTSCRN: return "PSCRN";
    case K_SCROLLLOCK: return "SCROLL";
    case K_PAUSE: return "PAUSE";

    case K_MOUSE1: return "MOUSE1";
    case K_MOUSE2: return "MOUSE2";
    case K_MOUSE3: return "MOUSE3";
    case K_MWHEELUP: return "MWHEELUP";
    case K_MWHEELDOWN: return "MWHEELDOWN";

    case K_JOY1: return "JOY1";
    case K_JOY2: return "JOY2";
    case K_JOY3: return "JOY3";
    case K_JOY4: return "JOY4";
    case K_JOY5: return "JOY5";
    case K_JOY6: return "JOY6";
    case K_JOY7: return "JOY7";
    case K_JOY8: return "JOY8";
    case K_JOY9: return "JOY9";
    case K_JOY10: return "JOY10";
    case K_JOY11: return "JOY11";
    case K_JOY12: return "JOY12";
    case K_JOY13: return "JOY13";
    case K_JOY14: return "JOY14";
    case K_JOY15: return "JOY15";
    case K_JOY16: return "JOY16";
  }
  return VStr();
}


//==========================================================================
//
//  VObject::VKeyFromName
//
//==========================================================================
int VObject::VKeyFromName (const VStr &kn) {
  if (kn.isEmpty()) return 0;

  if (kn.length() == 1) {
    char ch = kn[0];
    if (ch >= '0' && ch <= '9') return K_N0+(ch-'0');
    if (ch >= 'A' && ch <= 'Z') return K_a+(ch-'A');
    if (ch >= 'a' && ch <= 'z') return K_a+(ch-'a');
    if (ch == ' ') return K_SPACE;
    if (ch == '`' || ch == '~') return K_BACKQUOTE;
    if (ch == 27) return K_ESCAPE;
    if (ch == 13 || ch == 10) return K_ENTER;
    if (ch == 8) return K_BACKSPACE;
    if (ch == 9) return K_TAB;
  }

  if (kn.ICmp("ESCAPE") == 0) return K_ESCAPE;
  if (kn.ICmp("ENTER") == 0) return K_ENTER;
  if (kn.ICmp("RETURN") == 0) return K_ENTER;
  if (kn.ICmp("TAB") == 0) return K_TAB;
  if (kn.ICmp("BACKSPACE") == 0) return K_BACKSPACE;

  if (kn.ICmp("SPACE") == 0) return K_SPACE;

  if (kn.ICmp("UPARROW") == 0 || kn.ICmp("UP") == 0) return K_UPARROW;
  if (kn.ICmp("LEFTARROW") == 0 || kn.ICmp("LEFT") == 0) return K_LEFTARROW;
  if (kn.ICmp("RIGHTARROW") == 0 || kn.ICmp("RIGHT") == 0) return K_RIGHTARROW;
  if (kn.ICmp("DOWNARROW") == 0 || kn.ICmp("DOWN") == 0) return K_DOWNARROW;
  if (kn.ICmp("INSERT") == 0 || kn.ICmp("INS") == 0) return K_INSERT;
  if (kn.ICmp("DELETE") == 0 || kn.ICmp("DEL") == 0) return K_DELETE;
  if (kn.ICmp("HOME") == 0) return K_HOME;
  if (kn.ICmp("PAGEUP") == 0 || kn.ICmp("PGUP") == 0) return K_PAGEUP;
  if (kn.ICmp("PAGEDOWN") == 0 || kn.ICmp("PGDOWN") == 0 || kn.ICmp("PGDN") == 0) return K_PAGEDOWN;

  if (kn.ICmp("PAD0") == 0 || kn.ICmp("PAD_0") == 0) return K_PAD0;
  if (kn.ICmp("PAD1") == 0 || kn.ICmp("PAD_1") == 0) return K_PAD1;
  if (kn.ICmp("PAD2") == 0 || kn.ICmp("PAD_2") == 0) return K_PAD2;
  if (kn.ICmp("PAD3") == 0 || kn.ICmp("PAD_3") == 0) return K_PAD3;
  if (kn.ICmp("PAD4") == 0 || kn.ICmp("PAD_4") == 0) return K_PAD4;
  if (kn.ICmp("PAD5") == 0 || kn.ICmp("PAD_5") == 0) return K_PAD5;
  if (kn.ICmp("PAD6") == 0 || kn.ICmp("PAD_6") == 0) return K_PAD6;
  if (kn.ICmp("PAD7") == 0 || kn.ICmp("PAD_7") == 0) return K_PAD7;
  if (kn.ICmp("PAD8") == 0 || kn.ICmp("PAD_8") == 0) return K_PAD8;
  if (kn.ICmp("PAD9") == 0 || kn.ICmp("PAD_9") == 0) return K_PAD9;

  if (kn.ICmp("NUMLOCK") == 0 || kn.ICmp("NUM") == 0) return K_NUMLOCK;
  if (kn.ICmp("PADDIVIDE") == 0 || kn.ICmp("PAD_DIVIDE") == 0 || kn.ICmp("PAD/") == 0) return K_PADDIVIDE;
  if (kn.ICmp("PADMULTIPLE") == 0 || kn.ICmp("PAD_MULTIPLE") == 0 || kn.ICmp("PAD*") == 0) return K_PADMULTIPLE;
  if (kn.ICmp("PADMINUS") == 0 || kn.ICmp("PAD_MINUS") == 0 || kn.ICmp("PAD-") == 0) return K_PADMINUS;
  if (kn.ICmp("PADPLUS") == 0 || kn.ICmp("PAD_PLUS") == 0 || kn.ICmp("PAD+") == 0) return K_PADPLUS;
  if (kn.ICmp("PADENTER") == 0 || kn.ICmp("PAD_ENTER") == 0) return K_PADENTER;
  if (kn.ICmp("PADDOT") == 0 || kn.ICmp("PAD_DOT") == 0 || kn.ICmp("PAD.") == 0) return K_PADDOT;

  if (kn.ICmp("CAPSLOCK") == 0 || kn.ICmp("CAPS") == 0) return K_CAPSLOCK;
  if (kn.ICmp("BACKQUOTE") == 0) return K_BACKQUOTE;

  if (kn.ICmp("F1") == 0) return K_F1;
  if (kn.ICmp("F2") == 0) return K_F2;
  if (kn.ICmp("F3") == 0) return K_F3;
  if (kn.ICmp("F4") == 0) return K_F4;
  if (kn.ICmp("F5") == 0) return K_F5;
  if (kn.ICmp("F6") == 0) return K_F6;
  if (kn.ICmp("F7") == 0) return K_F7;
  if (kn.ICmp("F8") == 0) return K_F8;
  if (kn.ICmp("F9") == 0) return K_F9;
  if (kn.ICmp("F10") == 0) return K_F10;
  if (kn.ICmp("F11") == 0) return K_F11;
  if (kn.ICmp("F12") == 0) return K_F12;

  if (kn.ICmp("LSHIFT") == 0) return K_LSHIFT;
  if (kn.ICmp("RSHIFT") == 0) return K_RSHIFT;
  if (kn.ICmp("LCTRL") == 0) return K_LCTRL;
  if (kn.ICmp("RCTRL") == 0) return K_RCTRL;
  if (kn.ICmp("LALT") == 0) return K_LALT;
  if (kn.ICmp("RALT") == 0) return K_RALT;

  if (kn.ICmp("LWIN") == 0) return K_LWIN;
  if (kn.ICmp("RWIN") == 0) return K_RWIN;
  if (kn.ICmp("MENU") == 0) return K_MENU;

  if (kn.ICmp("PRINTSCRN") == 0 || kn.ICmp("PSCRN") == 0) return K_PRINTSCRN;
  if (kn.ICmp("SCROLLLOCK") == 0 || kn.ICmp("SCROLL") == 0) return K_SCROLLLOCK;
  if (kn.ICmp("PAUSE") == 0) return K_PAUSE;

  if (kn.ICmp("MOUSE1") == 0) return K_MOUSE1;
  if (kn.ICmp("MOUSE2") == 0) return K_MOUSE2;
  if (kn.ICmp("MOUSE3") == 0) return K_MOUSE3;
  if (kn.ICmp("MWHEELUP") == 0) return K_MWHEELUP;
  if (kn.ICmp("MWHEELDOWN") == 0) return K_MWHEELDOWN;

  if (kn.ICmp("JOY1") == 0) return K_JOY1;
  if (kn.ICmp("JOY2") == 0) return K_JOY2;
  if (kn.ICmp("JOY3") == 0) return K_JOY3;
  if (kn.ICmp("JOY4") == 0) return K_JOY4;
  if (kn.ICmp("JOY5") == 0) return K_JOY5;
  if (kn.ICmp("JOY6") == 0) return K_JOY6;
  if (kn.ICmp("JOY7") == 0) return K_JOY7;
  if (kn.ICmp("JOY8") == 0) return K_JOY8;
  if (kn.ICmp("JOY9") == 0) return K_JOY9;
  if (kn.ICmp("JOY10") == 0) return K_JOY10;
  if (kn.ICmp("JOY11") == 0) return K_JOY11;
  if (kn.ICmp("JOY12") == 0) return K_JOY12;
  if (kn.ICmp("JOY13") == 0) return K_JOY13;
  if (kn.ICmp("JOY14") == 0) return K_JOY14;
  if (kn.ICmp("JOY15") == 0) return K_JOY15;
  if (kn.ICmp("JOY16") == 0) return K_JOY16;

  return 0;
}


#include "vc_object_common.cpp"

#if defined(VCC_STANDALONE_EXECUTOR)
# include "vc_object_vccrun.cpp"
#elif defined(IN_VCC)
# include "vc_object_vcc.cpp"
#else
# include "vc_object_vavoom.cpp"
#endif
