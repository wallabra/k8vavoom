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
#include "vc_local.h"


// ////////////////////////////////////////////////////////////////////////// //
int VMemberBase::optDeprecatedLaxOverride = 0;
int VMemberBase::optDeprecatedLaxStates = 0;
vuint32 VMemberBase::lastUsedMemberId = 0; // monotonically increasing


// ////////////////////////////////////////////////////////////////////////// //
bool VMemberBase::GObjInitialised = false;
bool VMemberBase::GObjShuttingDown = false;
TArray<VMemberBase *> VMemberBase::GMembers;
//static VMemberBase *GMembersHash[4096];
TMapNC<VName, VMemberBase *> VMemberBase::gMembersMap;
TMapNC<VName, VMemberBase *> VMemberBase::gMembersMapAnyLC; // lower-cased names
TMapNC<VName, VMemberBase *> VMemberBase::gMembersMapClassLC; // lower-cased class names
TMapNC<VName, VMemberBase *> VMemberBase::gMembersMapPropLC; // lower-cased property names
TMapNC<VName, VMemberBase *> VMemberBase::gMembersMapConstLC; // lower-cased constant names
TArray<VPackage *> VMemberBase::gPackageList;

TArray<VStr> VMemberBase::GPackagePath;
TArray<VPackage *> VMemberBase::GLoadedPackages;
TArray<VClass *> VMemberBase::GDecorateClassImports;

VClass *VMemberBase::GClasses;

TArray<VStr> VMemberBase::incpathlist;
TArray<VStr> VMemberBase::definelist;

bool VMemberBase::doAsmDump = false;

int VMemberBase::unsafeCodeAllowed = 1;
int VMemberBase::unsafeCodeWarning = 1;
int VMemberBase::koraxCompatibility = 0;
int VMemberBase::koraxCompatibilityWarnings = 1;

void *VMemberBase::userdata; // arbitrary pointer, not used by the lexer
VStream *(*VMemberBase::dgOpenFile) (VStr filename, void *userdata);


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
  , HashNext(nullptr)
  , HashNextAnyLC(nullptr)
  , HashNextClassLC(nullptr)
  , HashNextPropLC(nullptr)
  , HashNextConstLC(nullptr)
{
  if (lastUsedMemberId == 0xffffffffu) Sys_Error("too many VC members");
  mMemberId = ++lastUsedMemberId;
  vassert(mMemberId != 0);
  if (GObjInitialised) {
    MemberIndex = GMembers.Append(this);
    PutToNameHash(this);
  } else {
    MemberIndex = -666;
  }
  if (AMemberType == MEMBER_Package) gPackageList.append((VPackage *)this);
}


//==========================================================================
//
//  VMemberBase::~VMemberBase
//
//==========================================================================
VMemberBase::~VMemberBase () {
  // you should never delete members
  // but they can be deleted on shutdown, and at that time
  // there is no reason to do this anyway
  //k8: no member should be removed ever (except on shutdown), so skip this
  //if (!GObjShuttingDown) RemoveFromNameHash(this);
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
//  VMemberBase::AddToHashMC
//
//==========================================================================
#define AddToHashMC(xname,map,HNext)  do { \
  VMemberBase **mpp = map.find(xname); \
  if (mpp) { \
    self->HNext = (*mpp); \
    *mpp = self; \
  } else { \
    self->HNext = nullptr; \
    map.put(xname, self); \
  } \
} while (0)


//==========================================================================
//
//  VMemberBase::PutToNameHash
//
//==========================================================================
void VMemberBase::PutToNameHash (VMemberBase *self) {
  if (!self || self->Name == NAME_None) return;
  //fprintf(stderr, "REGISTERING: <%s>\n", *self->Name);
  vassert(self->HashNext == nullptr);
  vassert(self->HashNextAnyLC == nullptr);
  vassert(self->HashNextClassLC == nullptr);
  vassert(self->HashNextPropLC == nullptr);
  vassert(self->HashNextConstLC == nullptr);
  AddToHashMC(self->Name, gMembersMap, HashNext);
  // case-insensitive search
  VName lname = VName(*self->Name, VName::AddLower);
  AddToHashMC(lname, gMembersMapAnyLC, HashNextAnyLC);
  if (self->MemberType == MEMBER_Class) AddToHashMC(lname, gMembersMapClassLC, HashNextClassLC);
  if (self->MemberType == MEMBER_Property) AddToHashMC(lname, gMembersMapPropLC, HashNextPropLC);
  if (self->MemberType == MEMBER_Const) AddToHashMC(lname, gMembersMapConstLC, HashNextConstLC);
}


//==========================================================================
//
//  VMemberBase::DumpNameMap
//
//==========================================================================
void VMemberBase::DumpNameMap (TMapNC<VName, VMemberBase *> &map, bool caseSensitive) {
  GLog.Logf("=== CASE-%sSENSITIVE NAME MAP ===", (caseSensitive ? "" : "IN"));
  for (auto it = map.first(); it; ++it) {
    GLog.Logf(" --- <%s>", *it.getKey());
    for (VMemberBase *m = it.getValue(); m; m = (caseSensitive ? m->HashNext : m->HashNextAnyLC)) {
      GLog.Logf("  <%s> : <%s>", *m->Name, *m->GetFullName());
    }
  }
}


//==========================================================================
//
//  VMemberBase::DumpNameMaps
//
//==========================================================================
void VMemberBase::DumpNameMaps () {
  if (!VObject::cliDumpNameTables) return;
  DumpNameMap(gMembersMap, true);
  DumpNameMap(gMembersMapAnyLC, false);
}


//==========================================================================
//
//  VMemberBase::DelFromHashMC
//
//==========================================================================
#define DelFromHashMC(xname,map,HNext)  do { \
  if (xname == NAME_None) { \
    vassert(self->HNext == nullptr); \
    break; \
  } \
  VMemberBase **mpp = map.find(xname); \
  if (!mpp) { \
    vassert(self->HNext == nullptr); \
    break; \
  } \
  VMemberBase *mprev = nullptr, *m = *mpp; \
  while (m && m != self) { mprev = m; m = m->HNext; } \
  if (m) { \
    if (mprev) { \
      mprev->HNext = m->HNext; \
    } else { \
      if (m->HNext) *mpp = m->HNext; else map.remove(xname); \
    } \
  } \
} while (0)


//==========================================================================
//
//  VMemberBase::RemoveFromNameHash
//
//==========================================================================
void VMemberBase::RemoveFromNameHash (VMemberBase *self) {
  if (!self || self->Name == NAME_None) return;
  //fprintf(stderr, "UNREGISTERING: <%s>\n", *self->Name);
  DelFromHashMC(self->Name, gMembersMap, HashNext);
  VName lname = VName(*self->Name, VName::FindLower);
  DelFromHashMC(lname, gMembersMapAnyLC, HashNextAnyLC);
  if (self->MemberType == MEMBER_Class) DelFromHashMC(lname, gMembersMapClassLC, HashNextClassLC);
  if (self->MemberType == MEMBER_Property) DelFromHashMC(lname, gMembersMapPropLC, HashNextPropLC);
  if (self->MemberType == MEMBER_Const) DelFromHashMC(lname, gMembersMapConstLC, HashNextConstLC);
}


//==========================================================================
//
//  VMemberBase::GetFullName
//
//==========================================================================
VStr VMemberBase::GetFullName () const {
  if (Outer) return Outer->GetFullName()+"."+Name.getCStr();
  if (Name.isValid()) return VStr(Name);
  return VStr(Name.getCStr());
}


//==========================================================================
//
//  VMemberBase::GetPackage
//
//==========================================================================
VPackage *VMemberBase::GetPackage () const {
  for (const VMemberBase *p = this; p; p = p->Outer) if (p->MemberType == MEMBER_Package) return (VPackage *)p;
  Sys_Error("Member object %s not in a package", *GetFullName());
  return nullptr;
}


//==========================================================================
//
//  VMemberBase::IsIn
//
//==========================================================================
bool VMemberBase::IsIn (VMemberBase *SomeOuter) const {
  for (const VMemberBase *Tst = Outer; Tst; Tst = Tst->Outer) if (Tst == SomeOuter) return true;
  return !SomeOuter;
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
//  called from `VObject::StaticInit()`
//
//==========================================================================
void VMemberBase::StaticInit () {
  vassert(!GObjInitialised);
  vassert(!GObjShuttingDown);
  // add native classes to the list
  for (VClass *C = GClasses; C; C = C->LinkNext) {
    vassert(C->MemberIndex == -666);
    C->MemberIndex = GMembers.Append(C);
    PutToNameHash(C);
  }

  // sprite TNT1 is always 0, ---- is always 1
  VClass::GSpriteNames.Append("tnt1");
  VClass::GSpriteNames.Append("----");

  GObjInitialised = true;
}


//==========================================================================
//
//  VMemberBase::StaticExit
//
//  called from `VObject::StaticInit()`
//
//==========================================================================
void VMemberBase::StaticExit () {
  vassert(!GObjShuttingDown);
  /*
  for (int i = 0; i < GMembers.Num(); ++i) {
    if (!GMembers[i]) continue;
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
  //VClass::GMobjInfos.Clear();
  //VClass::GScriptIds.Clear();
  VClass::GSpriteNames.Clear();
  gMembersMap.clear();
  gMembersMapLC.clear();
  */
  GObjInitialised = false;
  GObjShuttingDown = true;
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
  vassert(AName != NAME_None);
  // check if already loaded
  for (int i = 0; i < GLoadedPackages.Num(); ++i) if (GLoadedPackages[i]->Name == AName) return GLoadedPackages[i];
  if (VObject::cliShowPackageLoading) GLog.WriteLine(NAME_Init, "VavoomC: loading package '%s'...", *AName);
  VPackage *Pkg = new VPackage(AName);
  GLoadedPackages.Append(Pkg);
  Pkg->LoadObject(l);
  return Pkg;
}


//==========================================================================
//
//  VMemberBase::StaticFindMember
//
//==========================================================================
VMemberBase *VMemberBase::StaticFindMember (VName AName, VMemberBase *AOuter, vuint8 AType, VName EnumName) {
  //VName realName = AName;
  if (AType == MEMBER_Const && EnumName != NAME_None) {
    // rewrite name
    VStr nn(*EnumName);
    nn += " ";
    nn += *AName;
    AName = VName(*nn);
  }
  // use normal map
  VMemberBase **mpp = gMembersMap.find(AName);
  if (!mpp) return nullptr;
  for (VMemberBase *m = *mpp; m; m = m->HashNext) {
    //if (AName == "TVec") fprintf(stderr, "V: <%s> %d : %d (anypkg=%d); outerpkg=%d\n", *m->GetFullName(), m->MemberType, AType, (AOuter == ANY_PACKAGE), (m->Outer && m->Outer->MemberType == MEMBER_Package));
    if ((m->Outer == AOuter || (AOuter == ANY_PACKAGE && m->Outer && m->Outer->MemberType == MEMBER_Package)) &&
        (AType == ANY_MEMBER || m->MemberType == AType))
    {
      //if (AName == "TVec") fprintf(stderr, "  FOUND: V: <%s> %d : %d\n", *m->GetFullName(), m->MemberType, AType);
      return m;
    }
  }
  return nullptr;
}


//==========================================================================
//
//  VMemberBase::StaticFindMemberNoCase
//
//==========================================================================
VMemberBase *VMemberBase::StaticFindMemberNoCase (VStr AName, VMemberBase *AOuter, vuint8 AType, VName EnumName) {
  if (AName.isEmpty()) return nullptr;
  // rewrite name, if necessary
  if (AType == MEMBER_Const && EnumName != NAME_None) {
    VStr nn(*EnumName);
    nn += " ";
    nn += AName;
    AName = nn;
  }
  VName lname = VName(*AName, VName::FindLower);
  if (lname == NAME_None) return nullptr;
  // locase map
  VMemberBase **mpp = nullptr;
       if (AType == MEMBER_Class) mpp = gMembersMapClassLC.find(lname);
  else if (AType == MEMBER_Property) mpp = gMembersMapPropLC.find(lname);
  else if (AType == MEMBER_Const) mpp = gMembersMapConstLC.find(lname);
  else mpp = gMembersMapAnyLC.find(lname);
  if (!mpp) return nullptr;
  VMemberBase *m = *mpp;
  while (m) {
    if ((m->Outer == AOuter || (AOuter == ANY_PACKAGE && m->Outer && m->Outer->MemberType == MEMBER_Package)) &&
        (AType == ANY_MEMBER || m->MemberType == AType))
    {
      if (AName.strEquCI(*m->Name)) return m;
    }
         if (AType == MEMBER_Class) m = m->HashNextClassLC;
    else if (AType == MEMBER_Property) m = m->HashNextPropLC;
    else if (AType == MEMBER_Const) m = m->HashNextConstLC;
    else m = m->HashNextAnyLC;
  }
  return nullptr;
}


//==========================================================================
//
//  VMemberBase::StaticGetClassListNoCase
//
//  will not clear `list`
//
//==========================================================================
void VMemberBase::StaticGetClassListNoCase (TArray<VStr> &list, VStr prefix, VClass *isaClass) {
  //FIXME: make this faster
  // use locase class member map
  for (auto it = gMembersMapClassLC.first(); it; ++it) {
    for (VMemberBase *m = it.getValue(); m; m = m->HashNextClassLC) {
      if (m->MemberType == MEMBER_Class && m->Name != NAME_None) {
        VClass *cls = (VClass *)m;
        if (isaClass && !cls->IsChildOf(isaClass)) continue;
        VStr n = *m->Name;
        if (prefix.length()) {
          if (n.length() < prefix.length()) continue;
          if (!n.startsWithNoCase(prefix)) continue;
        }
        list.append(n);
      }
    }
  }
}


//==========================================================================
//
//  VMemberBase::StaticFindType
//
//==========================================================================
VFieldType VMemberBase::StaticFindType (VClass *AClass, VName Name) {
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
  int len = gPackageList.length();
  for (int f = 0; f < len; ++f) {
    VPackage *pkg = gPackageList[f];
    if (pkg->IsKnownEnum(Name)) return VFieldType(TYPE_Int);
  }

  return VFieldType(TYPE_Unknown);
}


//==========================================================================
//
//  VMemberBase::StaticFindClass
//
//==========================================================================
VClass *VMemberBase::StaticFindClass (VName AName, bool caseSensitive) {
  if (AName == NAME_None) return nullptr;
  if (!caseSensitive) return StaticFindClass(*AName, false); // use slightly slower search
  vassert(caseSensitive);
  VMemberBase **mpp = gMembersMap.find(AName);
  if (!mpp) return nullptr;
  for (VMemberBase *m = *mpp; m; m = m->HashNext) {
    if (m->Outer && m->Outer->MemberType == MEMBER_Package && m->MemberType == MEMBER_Class) {
      if (m->Name != AName) continue;
      return (VClass *)m;
    }
  }
  return nullptr;
}


//==========================================================================
//
//  VMemberBase::StaticFindClass
//
//==========================================================================
VClass *VMemberBase::StaticFindClass (const char *AName, bool caseSensitive) {
  if (!AName || !AName[0]) return nullptr;
  if (caseSensitive) return StaticFindClass(VName(AName, VName::Find), true); // use slightly faster search-by-name
  vassert(!caseSensitive);
  // use lower-case map
  VName loname = VName(AName, VName::FindLower);
  if (loname == NAME_None) return nullptr; // no such name, no chance to find a member
  VMemberBase **mpp = gMembersMapClassLC.find(loname);
  if (!mpp) return nullptr;
  for (VMemberBase *m = *mpp; m; m = m->HashNextClassLC) {
    if (m->Outer && m->Outer->MemberType == MEMBER_Package && m->MemberType == MEMBER_Class) {
      if (!VStr::strEquCI(*m->Name, AName)) continue;
      return (VClass *)m;
    }
  }
  return nullptr;
}


//==========================================================================
//
//  VMemberBase::StaticFindClassByGameObjName
//
//==========================================================================
VClass *VMemberBase::StaticFindClassByGameObjName (VName aname, VName pkgname) {
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
}


//==========================================================================
//
//  VMemberBase::StaticSplitStateLabel
//
//==========================================================================
void VMemberBase::StaticSplitStateLabel (VStr LabelName, TArray<VName> &Parts, bool appendToParts) {
  TArray<VStr> StrParts;
  LabelName.Split(".", StrParts);
  if (!appendToParts) Parts.Clear();
  while (StrParts.length() && StrParts[0].length() == 0) StrParts.removeAt(0);
  if (StrParts.length() > 0) {
    // remap old death state labels to proper names
    if (!appendToParts && StrParts[0].ICmp("XDeath") == 0) {
      Parts.Append("Death");
      Parts.Append("Extreme");
    } else if (!appendToParts && StrParts[0].ICmp("Burn") == 0) {
      Parts.Append("Death");
      Parts.Append("Fire");
    } else if (!appendToParts && StrParts[0].ICmp("Ice") == 0) {
      Parts.Append("Death");
      Parts.Append("Ice");
    } else if (!appendToParts && StrParts[0].ICmp("Disintegrate") == 0) {
      Parts.Append("Death");
      Parts.Append("Disintegrate");
    } else {
      Parts.Append(*StrParts[0]);
    }
  }
  for (int i = 1; i < StrParts.Num(); ++i) {
    if (StrParts[i].length()) Parts.Append(*StrParts[i]);
  }
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
//  pkgOpenFileDG
//
//==========================================================================
static VStream *pkgOpenFileDG (VLexer *self, VStr filename) {
  if (VMemberBase::dgOpenFile) return VMemberBase::dgOpenFile(filename, VMemberBase::userdata);
  return vc_OpenFile(filename);
}


//==========================================================================
//
//  VMemberBase::InitLexer
//
//==========================================================================
void VMemberBase::InitLexer (VLexer &lex) {
  for (int f = 0; f < incpathlist.length(); ++f) lex.AddIncludePath(*incpathlist[f]);
  for (int f = 0; f < definelist.length(); ++f) lex.AddDefine(*definelist[f], false); // no warnings
  lex.dgOpenFile = &pkgOpenFileDG;
}
