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

bool VObject::GObjInitialised;
TArray<VObject*> VObject::GObjObjects;
TArray<int> VObject::GObjAvailable;
VObject *VObject::GObjHash[4096];
int VObject::GNumDeleted;
bool VObject::GInGarbageCollection;
void *VObject::GNewObject;


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
    SetFlags(_OF_CleanupRef);
    for (int i = 0; i < GObjObjects.Num(); ++i) {
      if (!GObjObjects[i] || (GObjObjects[i]->GetFlags()&_OF_Destroyed)) continue;
      GObjObjects[i]->GetClass()->CleanObject(GObjObjects[i]);
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
VObject *VObject::StaticSpawnObject (VClass *AClass) {
  guard(VObject::StaticSpawnObject);

  check(AClass);
  // allocate memory
  VObject *Obj = (VObject*)Z_Calloc(AClass->ClassSize);

  // copy values from the default object
  check(AClass->Defaults);
  AClass->CopyObject(AClass->Defaults, (vuint8*)Obj);

  // find native class
  VClass *NativeClass = AClass;
  while (NativeClass != nullptr && !(NativeClass->ObjectFlags & CLASSOF_Native)) {
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
  if (!(ObjectFlags & _OF_Destroyed)) {
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
void VObject::CollectGarbage () {
  guard(VObject::CollectGarbage);

  if (!GNumDeleted) return;

  GInGarbageCollection = true;
  // mark objects to be cleaned
  for (int i = 0; i < GObjObjects.Num(); ++i) {
    if (!GObjObjects[i]) continue;
    VObject *Obj = GObjObjects[i];
    if (Obj->GetFlags()&_OF_Destroyed) Obj->SetFlags(_OF_CleanupRef);
  }

  // clean references
  for (int i = 0; i < GObjObjects.Num(); ++i) {
    if (!GObjObjects[i] || (GObjObjects[i]->GetFlags()&_OF_Destroyed)) continue;
    GObjObjects[i]->ClearReferences();
  }

  // now actually delete the objects
  for (int i = 0; i < GObjObjects.Num(); ++i) {
    if (!GObjObjects[i]) continue;
    VObject *Obj = GObjObjects[i];
    if (Obj->GetFlags()&_OF_Destroyed) {
      delete Obj;
      Obj = nullptr;
    }
  }
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
bool VObject::ExecuteNetMethod (VMethod *) {
  return false;
}


//**************************************************************************
//
//  Basic functions
//
//**************************************************************************

//==========================================================================
//
//  Object.Destroy
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, Destroy) {
  P_GET_SELF;
  delete Self;
  Self = nullptr;
}


//==========================================================================
//
//  Object.IsA
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, IsA) {
  P_GET_NAME(SomeName);
  P_GET_SELF;
  bool Ret = false;
  for (const VClass *c = Self->Class; c; c = c->GetSuperClass()) {
    if (c->GetVName() == SomeName) { Ret = true; break; }
  }
  RET_BOOL(Ret);
}


//==========================================================================
//
//  Object.IsDestroyed
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, IsDestroyed) {
  P_GET_SELF;
  RET_BOOL(Self->GetFlags() & _OF_DelayedDestroy);
}


//**************************************************************************
//
//  Error functions
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, Error) {
  Host_Error("%s", *PF_FormatString());
}

IMPLEMENT_FUNCTION(VObject, FatalError) {
  Sys_Error("%s", *PF_FormatString());
}


#ifndef VCC_STANDALONE_EXECUTOR
//**************************************************************************
//
//  Cvar functions
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, CvarExists) {
  P_GET_NAME(name);
  RET_BOOL(VCvar::HasVar(*name));
}

IMPLEMENT_FUNCTION(VObject, CreateCvar) {
  P_GET_INT(flags);
  P_GET_STR(help);
  P_GET_STR(def);
  P_GET_NAME(name);
  VCvar::CreateNew(*name, def, help, flags);
}

IMPLEMENT_FUNCTION(VObject, GetCvar) {
  P_GET_NAME(name);
  RET_INT(VCvar::GetInt(*name));
}

IMPLEMENT_FUNCTION(VObject, SetCvar) {
  P_GET_INT(value);
  P_GET_NAME(name);
  VCvar::Set(*name, value);
}

IMPLEMENT_FUNCTION(VObject, GetCvarF) {
  P_GET_NAME(name);
  RET_FLOAT(VCvar::GetFloat(*name));
}

IMPLEMENT_FUNCTION(VObject, SetCvarF) {
  P_GET_FLOAT(value);
  P_GET_NAME(name);
  VCvar::Set(*name, value);
}

IMPLEMENT_FUNCTION(VObject, GetCvarS) {
  P_GET_NAME(name);
  RET_STR(VCvar::GetString(*name));
}

IMPLEMENT_FUNCTION(VObject, SetCvarS) {
  P_GET_STR(value);
  P_GET_NAME(name);
  VCvar::Set(*name, value);
}

IMPLEMENT_FUNCTION(VObject, GetCvarB) {
  P_GET_NAME(name);
  RET_BOOL(VCvar::GetBool(*name));
}
#endif // !VCC_STANDALONE_EXECUTOR


//**************************************************************************
//
//  Math functions
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, AngleMod360) {
  P_GET_FLOAT(an);
  RET_FLOAT(AngleMod(an));
}

IMPLEMENT_FUNCTION(VObject, AngleMod180) {
  P_GET_FLOAT(an);
  RET_FLOAT(AngleMod180(an));
}

IMPLEMENT_FUNCTION(VObject, deg2rad) {
  P_GET_FLOAT(an);
  RET_FLOAT(DEG2RAD(an));
}

IMPLEMENT_FUNCTION(VObject, rad2deg) {
  P_GET_FLOAT(an);
  RET_FLOAT(RAD2DEG(an));
}

IMPLEMENT_FUNCTION(VObject, abs) {
  P_GET_INT(i);
  RET_INT(abs(i));
}

IMPLEMENT_FUNCTION(VObject, fabs) {
  P_GET_FLOAT(i);
  RET_FLOAT(fabs(i));
}

IMPLEMENT_FUNCTION(VObject, Min) {
  P_GET_INT(v2);
  P_GET_INT(v1);
  RET_INT(MIN(v1, v2));
}

IMPLEMENT_FUNCTION(VObject, min) {
  P_GET_INT(v2);
  P_GET_INT(v1);
  RET_INT(MIN(v1, v2));
}

IMPLEMENT_FUNCTION(VObject, FMin) {
  P_GET_FLOAT(v2);
  P_GET_FLOAT(v1);
  RET_FLOAT(MIN(v1, v2));
}

IMPLEMENT_FUNCTION(VObject, fmin) {
  P_GET_FLOAT(v2);
  P_GET_FLOAT(v1);
  RET_FLOAT(MIN(v1, v2));
}

IMPLEMENT_FUNCTION(VObject, Max) {
  P_GET_INT(v2);
  P_GET_INT(v1);
  RET_INT(MAX(v1, v2));
}

IMPLEMENT_FUNCTION(VObject, max) {
  P_GET_INT(v2);
  P_GET_INT(v1);
  RET_INT(MAX(v1, v2));
}

IMPLEMENT_FUNCTION(VObject, FMax) {
  P_GET_FLOAT(v2);
  P_GET_FLOAT(v1);
  RET_FLOAT(MAX(v1, v2));
}

IMPLEMENT_FUNCTION(VObject, fmax) {
  P_GET_FLOAT(v2);
  P_GET_FLOAT(v1);
  RET_FLOAT(MAX(v1, v2));
}

IMPLEMENT_FUNCTION(VObject, Clamp) {
  P_GET_INT(Max);
  P_GET_INT(Min);
  P_GET_INT(Val);
  RET_INT(MID(Min, Val, Max));
}

IMPLEMENT_FUNCTION(VObject, clamp) {
  P_GET_INT(Max);
  P_GET_INT(Min);
  P_GET_INT(Val);
  RET_INT(MID(Min, Val, Max));
}

IMPLEMENT_FUNCTION(VObject, FClamp) {
  P_GET_FLOAT(Max);
  P_GET_FLOAT(Min);
  P_GET_FLOAT(Val);
  RET_FLOAT(MID(Min, Val, Max));
}

IMPLEMENT_FUNCTION(VObject, fclamp) {
  P_GET_FLOAT(Max);
  P_GET_FLOAT(Min);
  P_GET_FLOAT(Val);
  RET_FLOAT(MID(Min, Val, Max));
}

IMPLEMENT_FUNCTION(VObject, sin) {
  P_GET_FLOAT(an);
  RET_FLOAT(msin(an));
}

IMPLEMENT_FUNCTION(VObject, cos) {
  P_GET_FLOAT(an);
  RET_FLOAT(mcos(an));
}

IMPLEMENT_FUNCTION(VObject, tan) {
  P_GET_FLOAT(an);
  RET_FLOAT(mtan(an));
}

IMPLEMENT_FUNCTION(VObject, asin) {
  P_GET_FLOAT(x);
  RET_FLOAT(masin(x));
}

IMPLEMENT_FUNCTION(VObject, acos) {
  P_GET_FLOAT(x);
  RET_FLOAT(acos(x));
}

IMPLEMENT_FUNCTION(VObject, atan) {
  P_GET_FLOAT(slope);
  RET_FLOAT(RAD2DEG(atan(slope)));
}

IMPLEMENT_FUNCTION(VObject, atan2) {
  P_GET_FLOAT(x);
  P_GET_FLOAT(y);
  RET_FLOAT(matan(y, x));
}

IMPLEMENT_FUNCTION(VObject, sqrt) {
  P_GET_FLOAT(x);
  RET_FLOAT(sqrt(x));
}

IMPLEMENT_FUNCTION(VObject, Length) {
  P_GET_VEC(vec);
  RET_FLOAT(Length(vec));
}

IMPLEMENT_FUNCTION(VObject, length) {
  P_GET_VEC(vec);
  RET_FLOAT(Length(vec));
}

IMPLEMENT_FUNCTION(VObject, Length2D) {
  P_GET_VEC(vec);
  RET_FLOAT(Length2D(vec));
}

IMPLEMENT_FUNCTION(VObject, length2D) {
  P_GET_VEC(vec);
  RET_FLOAT(length2D(vec));
}

IMPLEMENT_FUNCTION(VObject, Normalise) {
  P_GET_VEC(vec);
  RET_VEC(Normalise(vec));
}

IMPLEMENT_FUNCTION(VObject, normalise) {
  P_GET_VEC(vec);
  RET_VEC(Normalise(vec));
}

IMPLEMENT_FUNCTION(VObject, DotProduct) {
  P_GET_VEC(v2);
  P_GET_VEC(v1);
  RET_FLOAT(DotProduct(v1, v2));
}

IMPLEMENT_FUNCTION(VObject, dot) {
  P_GET_VEC(v2);
  P_GET_VEC(v1);
  RET_FLOAT(DotProduct(v1, v2));
}

IMPLEMENT_FUNCTION(VObject, CrossProduct) {
  P_GET_VEC(v2);
  P_GET_VEC(v1);
  RET_VEC(CrossProduct(v1, v2));
}

IMPLEMENT_FUNCTION(VObject, cross) {
  P_GET_VEC(v2);
  P_GET_VEC(v1);
  RET_VEC(CrossProduct(v1, v2));
}

IMPLEMENT_FUNCTION(VObject, AngleVectors) {
  P_GET_PTR(TVec, vup);
  P_GET_PTR(TVec, vright);
  P_GET_PTR(TVec, vforward);
  P_GET_PTR(TAVec, angles);
  AngleVectors(*angles, *vforward, *vright, *vup);
}

IMPLEMENT_FUNCTION(VObject, AngleVector) {
  P_GET_PTR(TVec, vec);
  P_GET_PTR(TAVec, angles);
  AngleVector(*angles, *vec);
}

IMPLEMENT_FUNCTION(VObject, VectorAngles) {
  P_GET_PTR(TAVec, angles);
  P_GET_PTR(TVec, vec);
  VectorAngles(*vec, *angles);
}

IMPLEMENT_FUNCTION(VObject, GetPlanePointZ) {
  P_GET_VEC(point);
  P_GET_PTR(TPlane, plane);
  RET_FLOAT(plane->GetPointZ(point));
}

IMPLEMENT_FUNCTION(VObject, PointOnPlaneSide) {
  P_GET_PTR(TPlane, plane);
  P_GET_VEC(point);
  RET_INT(plane->PointOnSide(point));
}

IMPLEMENT_FUNCTION(VObject, RotateDirectionVector) {
  P_GET_AVEC(rot);
  P_GET_VEC(vec);

  TAVec angles;
  TVec out;

  VectorAngles(vec, angles);
  angles.pitch += rot.pitch;
  angles.yaw += rot.yaw;
  angles.roll += rot.roll;
  AngleVector(angles, out);
  RET_VEC(out);
}

IMPLEMENT_FUNCTION(VObject, VectorRotateAroundZ) {
  P_GET_FLOAT(angle);
  P_GET_PTR(TVec, vec);

  float dstx = vec->x * mcos(angle) - vec->y * msin(angle);
  float dsty = vec->x * msin(angle) + vec->y * mcos(angle);

  vec->x = dstx;
  vec->y = dsty;
}

IMPLEMENT_FUNCTION(VObject, RotateVectorAroundVector) {
  P_GET_FLOAT(Angle);
  P_GET_VEC(Axis);
  P_GET_VEC(Vector);
  RET_VEC(RotateVectorAroundVector(Vector, Axis, Angle));
}


//**************************************************************************
//
//  String functions
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, strlen) {
  P_GET_STR(s);
  RET_INT(s.Utf8Length());
}

IMPLEMENT_FUNCTION(VObject, strcmp) {
  P_GET_STR(s2);
  P_GET_STR(s1);
  RET_INT(s1.Cmp(s2));
}

IMPLEMENT_FUNCTION(VObject, stricmp) {
  P_GET_STR(s2);
  P_GET_STR(s1);
  RET_INT(s1.ICmp(s2));
}

IMPLEMENT_FUNCTION(VObject, strcat) {
  P_GET_STR(s2);
  P_GET_STR(s1);
  RET_STR(s1 + s2);
}

IMPLEMENT_FUNCTION(VObject, strlwr) {
  P_GET_STR(s);
  RET_STR(s.ToLower());
}

IMPLEMENT_FUNCTION(VObject, strupr) {
  P_GET_STR(s);
  RET_STR(s.ToUpper());
}

IMPLEMENT_FUNCTION(VObject, substr) {
  P_GET_INT(Len);
  P_GET_INT(Start);
  P_GET_STR(Str);
  RET_STR(Str.Utf8Substring(Start, Len));
}

//  Creates one-char non-utf8 string from the given char code&0xff; 0 is allowed
//native static final string strFromChar (int ch);
IMPLEMENT_FUNCTION(VObject, strFromChar) {
  P_GET_INT(ch);
  ch &= 0xff;
  VStr s((char)ch);
  RET_STR(s);
}

//  Creates one-char utf8 string from the given char code (or empty string if char code is invalid); 0 is allowed
//native static final string strFromCharUtf8 (int ch);
IMPLEMENT_FUNCTION(VObject, strFromCharUtf8) {
  P_GET_INT(ch);
  VStr s;
  if (ch >= 0 && ch <= 0x10FFFF) s.utf8Append((vuint32)ch);
  RET_STR(s);
}

IMPLEMENT_FUNCTION(VObject, va) {
  RET_STR(PF_FormatString());
}

IMPLEMENT_FUNCTION(VObject, atoi) {
  P_GET_STR(str);
  RET_INT(atoi(*str));
}

IMPLEMENT_FUNCTION(VObject, atof) {
  P_GET_STR(str);
  RET_FLOAT(atof(*str));
}

IMPLEMENT_FUNCTION(VObject, StrStartsWith) {
  P_GET_STR(Check);
  P_GET_STR(Str);
  RET_BOOL(Str.StartsWith(Check));
}

IMPLEMENT_FUNCTION(VObject, StrEndsWith) {
  P_GET_STR(Check);
  P_GET_STR(Str);
  RET_BOOL(Str.EndsWith(Check));
}

IMPLEMENT_FUNCTION(VObject, StrReplace) {
  P_GET_STR(Replacement);
  P_GET_STR(Search);
  P_GET_STR(Str);
  RET_STR(Str.Replace(Search, Replacement));
}


//**************************************************************************
//
//  Random numbers
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, Random) {
  RET_FLOAT(Random());
}

IMPLEMENT_FUNCTION(VObject, P_Random) {
  RET_INT(rand() & 0xff);
}


#ifndef VCC_STANDALONE_EXECUTOR
//**************************************************************************
//
//  Texture utils
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, CheckTextureNumForName) {
  P_GET_NAME(name);
  RET_INT(GTextureManager.CheckNumForName(name, TEXTYPE_Wall, true, false));
}

IMPLEMENT_FUNCTION(VObject, TextureNumForName) {
  P_GET_NAME(name);
  RET_INT(GTextureManager.NumForName(name, TEXTYPE_Wall, true, false));
}

IMPLEMENT_FUNCTION(VObject, CheckFlatNumForName) {
  P_GET_NAME(name);
  RET_INT(GTextureManager.CheckNumForName(name, TEXTYPE_Flat, true, false));
}

IMPLEMENT_FUNCTION(VObject, FlatNumForName) {
  P_GET_NAME(name);
  RET_INT(GTextureManager.NumForName(name, TEXTYPE_Flat, true, false));
}

IMPLEMENT_FUNCTION(VObject, TextureHeight) {
  P_GET_INT(pic);
  RET_FLOAT(GTextureManager.TextureHeight(pic));
}

IMPLEMENT_FUNCTION(VObject, GetTextureName) {
  P_GET_INT(Handle);
  RET_NAME(GTextureManager.GetTextureName(Handle));
}
#endif // !VCC_STANDALONE_EXECUTOR


//==========================================================================
//
//  Printing in console
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, print) {
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  GCon->Log(PF_FormatString());
#else
  fprintf(stdout, "%s\n", *PF_FormatString());
#endif
}

IMPLEMENT_FUNCTION(VObject, dprint) {
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  GCon->Log(NAME_Dev, PF_FormatString());
#else
  fprintf(stderr, "%s\n", *PF_FormatString());
#endif
}


//==========================================================================
//
//  Type conversions
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, itof) {
  P_GET_INT(x);
  RET_FLOAT((float)x);
}

IMPLEMENT_FUNCTION(VObject, ftoi) {
  P_GET_FLOAT(x);
  RET_INT((vint32)x);
}

IMPLEMENT_FUNCTION(VObject, StrToName) {
  P_GET_STR(str);
  RET_NAME(VName(*str));
}

IMPLEMENT_FUNCTION(VObject, NameToStr) {
  P_GET_NAME(Name);
  RET_STR(*Name);
}


#ifndef VCC_STANDALONE_EXECUTOR
//==========================================================================
//
//  Console command functions
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, Cmd_CheckParm) {
  P_GET_STR(str);
  RET_INT(VCommand::CheckParm(*str));
}

IMPLEMENT_FUNCTION(VObject, Cmd_GetArgC) {
  RET_INT(VCommand::GetArgC());
}

IMPLEMENT_FUNCTION(VObject, Cmd_GetArgV) {
  P_GET_INT(idx);
  RET_STR(VCommand::GetArgV(idx));
}

IMPLEMENT_FUNCTION(VObject, CmdBuf_AddText) {
  GCmdBuf << PF_FormatString();
}
#endif // !VCC_STANDALONE_EXECUTOR


//==========================================================================
//
//  Class methods
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, FindClass) {
  P_GET_NAME(Name);
  RET_PTR(VClass::FindClass(*Name));
}

IMPLEMENT_FUNCTION(VObject, FindClassLowerCase) {
  P_GET_NAME(Name);
  RET_PTR(VClass::FindClassLowerCase(Name));
}

IMPLEMENT_FUNCTION(VObject, ClassIsChildOf) {
  P_GET_PTR(VClass, BaseClass);
  P_GET_PTR(VClass, SomeClass);
  RET_BOOL(SomeClass->IsChildOf(BaseClass));
}

IMPLEMENT_FUNCTION(VObject, GetClassName) {
  P_GET_PTR(VClass, SomeClass);
  RET_NAME(SomeClass->Name);
}

IMPLEMENT_FUNCTION(VObject, GetClassParent) {
  P_GET_PTR(VClass, SomeClass);
  RET_PTR(SomeClass->ParentClass);
}

IMPLEMENT_FUNCTION(VObject, GetClassReplacement) {
  P_GET_PTR(VClass, SomeClass);
  RET_PTR(SomeClass->GetReplacement());
}

IMPLEMENT_FUNCTION(VObject, GetClassReplacee) {
  P_GET_PTR(VClass, SomeClass);
  RET_PTR(SomeClass->GetReplacee());
}

IMPLEMENT_FUNCTION(VObject, FindClassState) {
  P_GET_NAME(StateName);
  P_GET_PTR(VClass, Cls);
  VStateLabel *Lbl = Cls->FindStateLabel(StateName);
  RET_PTR(Lbl ? Lbl->State : nullptr);
}

IMPLEMENT_FUNCTION(VObject, GetClassNumOwnedStates) {
  P_GET_PTR(VClass, Cls);
  int Ret = 0;
  for (VState *S = Cls->States; S; S = S->Next) ++Ret;
  RET_INT(Ret);
}

IMPLEMENT_FUNCTION(VObject, GetClassFirstState) {
  P_GET_PTR(VClass, Cls);
  RET_PTR(Cls->States);
}


//==========================================================================
//
//  State methods
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, StateIsInRange) {
  P_GET_INT(MaxDepth);
  P_GET_PTR(VState, End);
  P_GET_PTR(VState, Start);
  P_GET_PTR(VState, State);
  RET_BOOL(State ? State->IsInRange(Start, End, MaxDepth) : false);
}

IMPLEMENT_FUNCTION(VObject, StateIsInSequence) {
  P_GET_PTR(VState, Start);
  P_GET_PTR(VState, State);
  RET_BOOL(State ? State->IsInSequence(Start) : false);
}

IMPLEMENT_FUNCTION(VObject, GetStateSpriteName) {
  P_GET_PTR(VState, State);
  RET_NAME(State ? State->SpriteName : NAME_None);
}

IMPLEMENT_FUNCTION(VObject, GetStateSpriteFrame) {
  P_GET_PTR(VState, State);
  RET_INT(State ? State->Frame : 0);
}

IMPLEMENT_FUNCTION(VObject, GetStateSpriteFrameWidth) {
  P_GET_PTR(VState, State);
  RET_INT(State ? State->frameWidth : 0);
}

IMPLEMENT_FUNCTION(VObject, GetStateSpriteFrameHeight) {
  P_GET_PTR(VState, State);
  RET_INT(State ? State->frameHeight : 0);
}

IMPLEMENT_FUNCTION(VObject, GetStateSpriteFrameSize) {
  P_GET_REF(int, h);
  P_GET_REF(int, w);
  P_GET_PTR(VState, State);
  if (State) {
    *w = State->frameWidth;
    *h = State->frameHeight;
  } else {
    *w = 0;
    *h = 0;
  }
}

IMPLEMENT_FUNCTION(VObject, GetStateDuration) {
  P_GET_PTR(VState, State);
  RET_FLOAT(State ? State->Time : 0.0);
}

IMPLEMENT_FUNCTION(VObject, GetStatePlus) {
  P_GET_BOOL_OPT(IgnoreJump, false);
  P_GET_INT(Offset);
  P_GET_PTR(VState, State);
  RET_PTR(State ? State->GetPlus(Offset, IgnoreJump) : nullptr);
}

IMPLEMENT_FUNCTION(VObject, StateHasAction) {
  P_GET_PTR(VState, State);
  RET_BOOL(State ? !!State->Function : false);
}

IMPLEMENT_FUNCTION(VObject, CallStateAction) {
  P_GET_PTR(VState, State);
  P_GET_PTR(VObject, obj);
  if (State && State->Function) {
    P_PASS_REF(obj);
    ExecuteFunction(State->Function);
  }
}

IMPLEMENT_FUNCTION(VObject, GetNextState) {
  P_GET_PTR(VState, State);
  RET_PTR(State ? State->NextState : nullptr);
}

IMPLEMENT_FUNCTION(VObject, GetNextStateInProg) {
  P_GET_PTR(VState, State);
  RET_PTR(State ? State->Next : nullptr);
}

IMPLEMENT_FUNCTION(VObject, GetStateSpriteFrameOfsX) {
  P_GET_PTR(VState, State);
  RET_INT(State ? State->frameOfsX : 0);
}

IMPLEMENT_FUNCTION(VObject, GetStateSpriteFrameOfsY) {
  P_GET_PTR(VState, State);
  RET_INT(State ? State->frameOfsY : 0);
}

IMPLEMENT_FUNCTION(VObject, GetStateSpriteFrameOffset) {
  P_GET_REF(int, dy);
  P_GET_REF(int, dx);
  P_GET_PTR(VState, State);
  if (State) {
    *dx = State->frameOfsX;
    *dy = State->frameOfsY;
  } else {
    *dx = 0;
    *dy = 0;
  }
}

IMPLEMENT_FUNCTION(VObject, GetStateMisc1) {
  P_GET_PTR(VState, State);
  RET_INT(State ? State->Misc1 : 0);
}

IMPLEMENT_FUNCTION(VObject, GetStateMisc2) {
  P_GET_PTR(VState, State);
  RET_INT(State ? State->Misc2 : 0);
}

IMPLEMENT_FUNCTION(VObject, SetStateMisc1) {
  P_GET_INT(v);
  P_GET_PTR(VState, State);
  if (State) State->Misc1 = v;
}

IMPLEMENT_FUNCTION(VObject, SetStateMisc2) {
  P_GET_INT(v);
  P_GET_PTR(VState, State);
  if (State) State->Misc2 = v;
}

#ifndef VCC_STANDALONE_EXECUTOR
IMPLEMENT_FUNCTION(VObject, AreStateSpritesPresent) {
  P_GET_PTR(VState, State);
  RET_BOOL(State ? R_AreSpritesPresent(State->SpriteIndex) : false);
}
#endif


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

IMPLEMENT_FUNCTION(VObject, AllObjects) {
  P_GET_PTR(VObject*, Obj);
  P_GET_PTR(VClass, BaseClass);
  RET_PTR(new VObjectsIterator(BaseClass, Obj));
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

IMPLEMENT_FUNCTION(VObject, AllClasses) {
  P_GET_PTR(VClass*, Class);
  P_GET_PTR(VClass, BaseClass);
  RET_PTR(new VClassesIterator(BaseClass, Class));
}


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

IMPLEMENT_FUNCTION(VObject, AllClassStates) {
  P_GET_PTR(VState *, aout);
  P_GET_PTR(VClass, BaseClass);
  RET_PTR(new VClassStatesIterator(BaseClass, aout));
}


#ifndef VCC_STANDALONE_EXECUTOR
//==========================================================================
//
//  Misc
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, Info_ValueForKey) {
  P_GET_STR(key);
  P_GET_STR(info);
  RET_STR(Info_ValueForKey(info, key));
}

IMPLEMENT_FUNCTION(VObject, WadLumpPresent) {
  P_GET_NAME(name);
  RET_BOOL(W_CheckNumForName(name) >= 0);
}
#endif

IMPLEMENT_FUNCTION(VObject, SpawnObject) {
  P_GET_PTR(VClass, Class);
  RET_REF(VObject::StaticSpawnObject(Class));
}

#ifndef VCC_STANDALONE_EXECUTOR
IMPLEMENT_FUNCTION(VObject, FindAnimDoor) {
  P_GET_INT(BaseTex);
  RET_PTR(R_FindAnimDoor(BaseTex));
}

IMPLEMENT_FUNCTION(VObject, GetLangString) {
  P_GET_NAME(Id);
  RET_STR(GLanguage[Id]);
}
#endif // !VCC_STANDALONE_EXECUTOR

#ifndef VCC_STANDALONE_EXECUTOR
IMPLEMENT_FUNCTION(VObject, RGB) {
  P_GET_BYTE(b);
  P_GET_BYTE(g);
  P_GET_BYTE(r);
  RET_INT(0xff000000+(r<<16)+(g<<8)+b);
}

IMPLEMENT_FUNCTION(VObject, RGBA) {
  P_GET_BYTE(a);
  P_GET_BYTE(b);
  P_GET_BYTE(g);
  P_GET_BYTE(r);
  RET_INT((a<<24)+(r<<16)+(g<<8)+b);
}

IMPLEMENT_FUNCTION(VObject, GetLockDef) {
  P_GET_INT(Lock);
  RET_PTR(GetLockDef(Lock));
}

IMPLEMENT_FUNCTION(VObject, ParseColour) {
  P_GET_STR(Name);
  RET_INT(M_ParseColour(Name));
}

IMPLEMENT_FUNCTION(VObject, TextColourString) {
  P_GET_INT(Colour);
  VStr Ret;
  Ret += TEXT_COLOUR_ESCAPE;
  Ret += (Colour < CR_BRICK || Colour >= NUM_TEXT_COLOURS ? '-' : (char)(Colour+'A'));
  RET_STR(Ret);
}

IMPLEMENT_FUNCTION(VObject, StartTitleMap) {
  RET_BOOL(Host_StartTitleMap());
}

IMPLEMENT_FUNCTION(VObject, LoadBinaryLump) {
  P_GET_PTR(TArray<vuint8>, Array);
  P_GET_NAME(LumpName);
  W_LoadLumpIntoArray(LumpName, *Array);
}

IMPLEMENT_FUNCTION(VObject, IsMapPresent) {
  P_GET_NAME(MapName);
  RET_BOOL(IsMapPresent(MapName));
}

/*
IMPLEMENT_FUNCTION(VObject, Clock) {
  P_GET_INT(Idx);
  if (Idx < 0) ++host_cycles[-Idx]; else clock_cycle(host_cycles[Idx]);
}

IMPLEMENT_FUNCTION(VObject, Unclock) {
  P_GET_INT(Idx);
  unclock_cycle(host_cycles[Idx]);
}
*/

IMPLEMENT_FUNCTION(VObject, HasDecal) {
  P_GET_NAME(name);
  RET_BOOL(VDecalDef::hasDecal(name));
}
#endif // !VCC_STANDALONE_EXECUTOR
