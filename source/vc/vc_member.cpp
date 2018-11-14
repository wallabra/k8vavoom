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

#include "vc_local.h"


// ////////////////////////////////////////////////////////////////////////// //
bool VMemberBase::GObjInitialised;
TArray<VMemberBase *> VMemberBase::GMembers;
VMemberBase *VMemberBase::GMembersHash[4096];

TArray<VStr> VMemberBase::GPackagePath;
TArray<VPackage *> VMemberBase::GLoadedPackages;
TArray<VClass *> VMemberBase::GDecorateClassImports;

VClass *VMemberBase::GClasses;

TArray<VStr> VMemberBase::incpathlist;
TArray<VStr> VMemberBase::definelist;

bool VMemberBase::doAsmDump = false;

bool VMemberBase::unsafeCodeAllowed = true;
bool VMemberBase::unsafeCodeWarning = true;


//==========================================================================
//
//  VProgsImport::VProgsImport
//
//==========================================================================
VProgsImport::VProgsImport (VMemberBase *InObj, vint32 InOuterIndex)
  : Type(InObj->MemberType)
  , Name(InObj->Name)
  , OuterIndex(InOuterIndex)
  , Obj(InObj)
{
}


//==========================================================================
//
//  VProgsExport::VProgsExport
//
//==========================================================================
VProgsExport::VProgsExport (VMemberBase *InObj)
  : Type(InObj->MemberType)
  , Name(InObj->Name)
  , Obj(InObj)
{
}


//==========================================================================
//
//  VMemberBase::VMemberBase
//
//==========================================================================
VMemberBase::VMemberBase (vuint8 AMemberType, VName AName, VMemberBase *AOuter, const TLocation &ALoc)
  : MemberType(AMemberType)
  , Name(AName)
  , Outer(AOuter)
  , Loc(ALoc)
{
  if (GObjInitialised) {
    MemberIndex = GMembers.Append(this);
    int HashIndex = Name.GetIndex()&4095;
    HashNext = GMembersHash[HashIndex];
    GMembersHash[HashIndex] = this;
  }
}


//==========================================================================
//
//  VMemberBase::~VMemberBase
//
//==========================================================================
VMemberBase::~VMemberBase () {
}


//==========================================================================
//
//  VMemberBase::CompilerShutdown
//
//==========================================================================
void VMemberBase::CompilerShutdown () {
}


//==========================================================================
//
//  VMemberBase::GetFullName
//
//==========================================================================
VStr VMemberBase::GetFullName () const {
  guardSlow(VMemberBase::GetFullName);
  if (Outer) return Outer->GetFullName()+"."+Name;
  return VStr(Name);
  unguardSlow;
}


//==========================================================================
//
//  VMemberBase::GetPackage
//
//==========================================================================
VPackage *VMemberBase::GetPackage () const {
  guard(VMemberBase::GetPackage);
  for (const VMemberBase *p = this; p; p = p->Outer) if (p->MemberType == MEMBER_Package) return (VPackage *)p;
  Sys_Error("Member object %s not in a package", *GetFullName());
  return nullptr;
  unguard;
}


//==========================================================================
//
//  VMemberBase::IsIn
//
//==========================================================================
bool VMemberBase::IsIn (VMemberBase *SomeOuter) const {
  guardSlow(VMemberBase::IsIn);
  for (const VMemberBase *Tst = Outer; Tst; Tst = Tst->Outer) if (Tst == SomeOuter) return true;
  return !SomeOuter;
  unguardSlow;
}


//==========================================================================
//
//  VMemberBase::Serialise
//
//==========================================================================
void VMemberBase::Serialise (VStream &Strm) {
  Strm << Outer;
}


//==========================================================================
//
//  VMemberBase::PostLoad
//
//==========================================================================
void VMemberBase::PostLoad () {
}


//==========================================================================
//
//  VMemberBase::Shutdown
//
//==========================================================================
void VMemberBase::Shutdown () {
}


//==========================================================================
//
//  VMemberBase::StaticInit
//
//==========================================================================
void VMemberBase::StaticInit () {
  guard(VMemberBase::StaticInit);
  // add native classes to the list.
  for (VClass *C = GClasses; C; C = C->LinkNext) {
    C->MemberIndex = GMembers.Append(C);
    int HashIndex = C->Name.GetIndex()&4095;
    C->HashNext = GMembersHash[HashIndex];
    GMembersHash[HashIndex] = C;
    C->HashLowerCased();
  }

  // sprite TNT1 is always 0, ---- is always 1
  VClass::GSpriteNames.Append("tnt1");
  VClass::GSpriteNames.Append("----");

  GObjInitialised = true;
  unguard;
}


//==========================================================================
//
//  VMemberBase::StaticExit
//
//==========================================================================
void VMemberBase::StaticExit () {
  for (int i = 0; i < GMembers.Num(); ++i) {
    if (GMembers[i]->MemberType != MEMBER_Class || (((VClass *)GMembers[i])->ObjectFlags&CLASSOF_Native) == 0) {
      delete GMembers[i];
      GMembers[i] = nullptr;
    } else {
      GMembers[i]->Shutdown();
    }
  }
  GMembers.Clear();
  GPackagePath.Clear();
  GLoadedPackages.Clear();
  GDecorateClassImports.Clear();
  VClass::GMobjInfos.Clear();
  VClass::GScriptIds.Clear();
  VClass::GSpriteNames.Clear();
  GObjInitialised = false;
}


//==========================================================================
//
//  VMemberBase::StaticCompilerShutdown
//
//==========================================================================
void VMemberBase::StaticCompilerShutdown () {
  VExpression::InCompilerCleanup = true;
  for (int i = 0; i < GMembers.length(); ++i) GMembers[i]->CompilerShutdown();
  VExpression::InCompilerCleanup = false;
}


//==========================================================================
//
//  VMemberBase::StaticAddPackagePath
//
//==========================================================================
void VMemberBase::StaticAddPackagePath (const char *Path) {
  if (Path && Path[0]) GPackagePath.Append(Path);
}


//==========================================================================
//
//  VMemberBase::StaticLoadPackage
//
//==========================================================================
VPackage *VMemberBase::StaticLoadPackage (VName AName, const TLocation &l) {
  guard(VMemberBase::StaticLoadPackage);
  // check if already loaded
  for (int i = 0; i < GLoadedPackages.Num(); ++i) if (GLoadedPackages[i]->Name == AName) return GLoadedPackages[i];
  VPackage *Pkg = new VPackage(AName);
  GLoadedPackages.Append(Pkg);
  Pkg->LoadObject(l);
  return Pkg;
  unguard;
}


//==========================================================================
//
//  VMemberBase::StaticFindMember
//
//==========================================================================
VMemberBase *VMemberBase::StaticFindMember (VName AName, VMemberBase *AOuter, vuint8 AType, VName EnumName) {
  guard(VMemberBase::StaticFindMember);
  //VName realName = AName;
  if (AType == MEMBER_Const && EnumName != NAME_None) {
    // rewrite name
    VStr nn(*EnumName);
    nn += " ";
    nn += *AName;
    AName = VName(*nn);
  }
  int HashIndex = AName.GetIndex()&4095;
  for (VMemberBase *m = GMembersHash[HashIndex]; m; m = m->HashNext) {
    if (m->Name == AName && (m->Outer == AOuter ||
        (AOuter == ANY_PACKAGE && m->Outer && m->Outer->MemberType == MEMBER_Package)) &&
        (AType == ANY_MEMBER || m->MemberType == AType))
    {
      return m;
    }
  }
  return nullptr;
  unguard;
}


//==========================================================================
//
//  VMemberBase::StaticFindMemberNoCase
//
//==========================================================================
VMemberBase *VMemberBase::StaticFindMemberNoCase (VName AName, VMemberBase *AOuter, vuint8 AType, VName EnumName) {
  guard(VMemberBase::StaticFindMemberNoCase);
  //VName realName = AName;
  if (AType == MEMBER_Const && EnumName != NAME_None) {
    // rewrite name
    VStr nn(*EnumName);
    nn += " ";
    nn += *AName;
    AName = VName(*nn);
  }
  //FIXME: make this faster
  int len = GMembers.length();
  for (int f = 0; f < len; ++f) {
    VMemberBase *m = GMembers[f];
    if (VStr::ICmp(*m->Name, *AName) == 0 && (m->Outer == AOuter ||
        (AOuter == ANY_PACKAGE && m->Outer && m->Outer->MemberType == MEMBER_Package)) &&
        (AType == ANY_MEMBER || m->MemberType == AType))
    {
      return m;
    }
  }
  return nullptr;
  unguard;
}


//==========================================================================
//
//  VMemberBase::StaticFindType
//
//==========================================================================
VFieldType VMemberBase::StaticFindType (VClass *AClass, VName Name) {
  guard(VMemberBase::StaticFindType);
  if (Name == NAME_None) return VFieldType(TYPE_Unknown);

  // enum in a class
  if (AClass && AClass->IsKnownEnum(Name)) return VFieldType(TYPE_Int);

  // class name
  VMemberBase *m = StaticFindMember(Name, ANY_PACKAGE, MEMBER_Class);
  if (m) return VFieldType((VClass *)m);

  // struct name
  m = StaticFindMember(Name, (AClass ? (VMemberBase *)AClass : (VMemberBase *)ANY_PACKAGE), MEMBER_Struct);
  if (m) return VFieldType((VStruct *)m);

  // type in parent class
  if (AClass) {
    VFieldType tres = StaticFindType(AClass->ParentClass, Name);
    if (tres.Type != TYPE_Unknown) return tres;
  }

  // package enum
  //FIXME: make this faster
  {
    int len = GMembers.length();
    for (int f = 0; f < len; ++f) {
      if (GMembers[f] && GMembers[f]->MemberType == MEMBER_Package) {
        VPackage *pkg = (VPackage *)GMembers[f];
        if (pkg->IsKnownEnum(Name)) return VFieldType(TYPE_Int);
      }
    }
  }

  return VFieldType(TYPE_Unknown);
  unguard;
}


//==========================================================================
//
//  VMemberBase::StaticFindClass
//
//==========================================================================
VClass *VMemberBase::StaticFindClass (VName Name) {
  guard(VMemberBase::StaticFindClass);
  VMemberBase *m = StaticFindMember(Name, ANY_PACKAGE, MEMBER_Class);
  if (m) return (VClass *)m;
  return nullptr;
  unguard;
}


/*
//==========================================================================
//
//  VMemberBase::StaticFindMObj
//
//==========================================================================
VClass *VMemberBase::StaticFindMObj (vint32 id, VName pkgname) {
  guard(VMemberBase::StaticFindMObj);
  if (pkgname != NAME_None) {
    VMemberBase *pkg = StaticFindMember(pkgname, nullptr, MEMBER_Package);
    if (!pkg) return nullptr;
    return ((VPackage *)pkg)->FindMObj(id);
  } else {
    int len = GMembers.length();
    for (int f = 0; f < len; ++f) {
      if (GMembers[f] && GMembers[f]->MemberType == MEMBER_Package) {
        VClass *c = ((VPackage *)GMembers[f])->FindMObj(id);
        if (c) return c;
      }
    }
  }
  return nullptr;
  unguard;
}


//==========================================================================
//
//  VMemberBase::StaticFindMObjInfo
//
//==========================================================================
mobjinfo_t *VMemberBase::StaticFindMObjInfo (vint32 id) {
  if (id == 0) return nullptr;
  guard(VMemberBase::StaticFindMObjInfo);
  int len = VClass::GMobjInfos.length();
  for (int f = 0; f < len; ++f) {
    mobjinfo_t *nfo = &VClass::GMobjInfos[f];
    if (nfo->DoomEdNum == id) return nfo;
  }
  return nullptr;
  unguard;
}
*/


//==========================================================================
//
//  VMemberBase::StaticDumpMObjInfo
//
//==========================================================================
void VMemberBase::StaticDumpMObjInfo () {
  guard(VMemberBase::StaticDumpMObjInfo);
  int len = VClass::GMobjInfos.length();
  fprintf(stderr, "=== DOOMED ===\n");
  for (int f = 0; f < len; ++f) {
    mobjinfo_t *nfo = &VClass::GMobjInfos[f];
    fprintf(stderr, "  %4d: '%s'; flags:0x%02x; filter:0x%04x\n", nfo->DoomEdNum, (nfo->Class ? *nfo->Class->GetFullName() : "<none>"), nfo->flags, nfo->GameFilter);
  }
  unguard;
}


//==========================================================================
//
//  VMemberBase::StaticDumpScriptIds
//
//==========================================================================
void VMemberBase::StaticDumpScriptIds () {
  guard(VMemberBase::StaticDumpMObjInfo);
  int len = VClass::GScriptIds.length();
  fprintf(stderr, "=== SCRIPTID ===\n");
  for (int f = 0; f < len; ++f) {
    mobjinfo_t *nfo = &VClass::GScriptIds[f];
    fprintf(stderr, "  %4d: '%s'; flags:0x%02x; filter:0x%04x\n", nfo->DoomEdNum, (nfo->Class ? *nfo->Class->GetFullName() : "<none>"), nfo->flags, nfo->GameFilter);
  }
  unguard;
}


/*
//==========================================================================
//
//  VMemberBase::StaticFindScriptId
//
//==========================================================================
VClass *VMemberBase::StaticFindScriptId (vint32 id, VName pkgname) {
  guard(VMemberBase::StaticFindMObj);
  if (pkgname != NAME_None) {
    VMemberBase *pkg = StaticFindMember(pkgname, nullptr, MEMBER_Package);
    if (!pkg) return nullptr;
    return ((VPackage *)pkg)->FindScriptId(id);
  } else {
    int len = GMembers.length();
    for (int f = 0; f < len; ++f) {
      if (GMembers[f] && GMembers[f]->MemberType == MEMBER_Package) {
        VClass *c = ((VPackage *)GMembers[f])->FindScriptId(id);
        if (c) return c;
      }
    }
  }
  return nullptr;
  unguard;
}
*/


//==========================================================================
//
//  VMemberBase::StaticFindClassByGameObjName
//
//==========================================================================
VClass *VMemberBase::StaticFindClassByGameObjName (VName aname, VName pkgname) {
  guard(VMemberBase::StaticFindClassByGameObjName);
  if (aname == NAME_None) return nullptr;
  VMemberBase *pkg = nullptr;
  if (pkgname != NAME_None) {
    pkg = StaticFindMember(pkgname, nullptr, MEMBER_Package);
    if (!pkg) return nullptr;
  }
  int len = GMembers.length();
  for (int f = 0; f < len; ++f) {
    if (GMembers[f] && GMembers[f]->MemberType == MEMBER_Class) {
      VClass *c = (VClass *)GMembers[f];
      if (c->ClassGameObjName == aname) {
        if (pkgname == nullptr) return c;
        // check package
        for (const VMemberBase *p = c; p; p = p->Outer) {
          if (p->MemberType == MEMBER_Package && p == pkg) return c;
        }
      }
    }
  }
  return nullptr;
  unguard;
}


//==========================================================================
//
//  VMemberBase::StaticSplitStateLabel
//
//==========================================================================
void VMemberBase::StaticSplitStateLabel (const VStr &LabelName, TArray<VName> &Parts) {
  guard(VMemberBase::StaticSplitStateLabel);
  TArray<VStr> StrParts;
  LabelName.Split(".", StrParts);
  Parts.Clear();
  // remap old death state labels to proper names
  if (StrParts[0] == "XDeath") {
    Parts.Append("Death");
    Parts.Append("Extreme");
  } else if (StrParts[0] == "Burn") {
    Parts.Append("Death");
    Parts.Append("Fire");
  } else if (StrParts[0] == "Ice") {
    Parts.Append("Death");
    Parts.Append("Ice");
  } else if (StrParts[0] == "Disintegrate") {
    Parts.Append("Death");
    Parts.Append("Disintegrate");
  } else {
    Parts.Append(*StrParts[0]);
  }
  for (int i = 1; i < StrParts.Num(); ++i) Parts.Append(*StrParts[i]);
  unguard;
}


//==========================================================================
//
//  VMemberBase::StaticAddIncludePath
//
//==========================================================================
void VMemberBase::StaticAddIncludePath (const char *s) {
  if (!s || !s[0]) return;
  incpathlist.Append(VStr(s));
}


//==========================================================================
//
//  VMemberBase::StaticAddDefine
//
//==========================================================================
void VMemberBase::StaticAddDefine (const char *s) {
  if (!s || !s[0]) return;
  VStr str(s);
  for (int f = 0; f < definelist.length(); ++f) if (definelist[f] == str) return;
  definelist.Append(str);
}


//==========================================================================
//
//  VMemberBase::InitLexer
//
//==========================================================================
void VMemberBase::InitLexer (VLexer &lex) {
  for (int f = 0; f < incpathlist.length(); ++f) lex.AddIncludePath(*incpathlist[f]);
  for (int f = 0; f < definelist.length(); ++f) lex.AddDefine(*definelist[f], false); // no warnings
}
