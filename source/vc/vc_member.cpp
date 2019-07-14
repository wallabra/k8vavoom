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
bool VMemberBase::optDeprecatedLaxOverride = false;
bool VMemberBase::optDeprecatedLaxStates = false;
vuint32 VMemberBase::lastUsedMemberId = 0; // monotonically increasing


// ////////////////////////////////////////////////////////////////////////// //
bool VMemberBase::GObjInitialised = false;
bool VMemberBase::GObjShuttingDown = false;
TArray<VMemberBase *> VMemberBase::GMembers;
//static VMemberBase *GMembersHash[4096];
static TMapNC<VName, VMemberBase *> gMembersMap;
static TMapNC<VName, VMemberBase *> gMembersMapLC; // lower-cased names
static TArray<VPackage *> gPackageList;

TArray<VStr> VMemberBase::GPackagePath;
TArray<VPackage *> VMemberBase::GLoadedPackages;
TArray<VClass *> VMemberBase::GDecorateClassImports;

VClass *VMemberBase::GClasses;

TArray<VStr> VMemberBase::incpathlist;
TArray<VStr> VMemberBase::definelist;

bool VMemberBase::doAsmDump = false;

bool VMemberBase::unsafeCodeAllowed = true;
bool VMemberBase::unsafeCodeWarning = true;
bool VMemberBase::koraxCompatibility = false;
bool VMemberBase::koraxCompatibilityWarnings = true;

void *VMemberBase::userdata; // arbitrary pointer, not used by the lexer
VStream *(*VMemberBase::dgOpenFile) (const VStr &filename, void *userdata);


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
  , HashNextLC(nullptr)
{
  if (lastUsedMemberId == 0xffffffffu) Sys_Error("too many VC members");
  mMemberId = ++lastUsedMemberId;
  check(mMemberId != 0);
  if (GObjInitialised) {
    MemberIndex = GMembers.Append(this);
    /*
    int HashIndex = Name.GetIndex()&4095;
    HashNext = GMembersHash[HashIndex];
    GMembersHash[HashIndex] = this;
    */
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
//  VMemberBase::PutToNameHash
//
//==========================================================================
void VMemberBase::PutToNameHash (VMemberBase *self) {
  if (!self || self->Name == NAME_None) return;
  //fprintf(stderr, "REGISTERING: <%s>\n", *self->Name);
  check(self->HashNext == nullptr);
  check(self->HashNextLC == nullptr);
  {
    VMemberBase **mpp = gMembersMap.find(self->Name);
    if (mpp) {
      check(*mpp != self);
      self->HashNext = (*mpp);
      *mpp = self;
    } else {
      self->HashNext = nullptr;
      gMembersMap.put(self->Name, self);
    }
  }
  // case-insensitive search is required only for classes
  if (self->MemberType != MEMBER_Class) return;
  // locase map
  {
    VName lname = VName(*self->Name, VName::AddLower);
    VMemberBase **mpp = gMembersMapLC.find(lname);
    if (mpp) {
      self->HashNextLC = (*mpp);
      *mpp = self;
    } else {
      self->HashNextLC = nullptr;
      gMembersMapLC.put(lname, self);
    }
  }
}


//==========================================================================
//
//  VMemberBase::DumpNameMap
//
//==========================================================================
void VMemberBase::DumpNameMap (TMapNC<VName, VMemberBase *> &map, bool caseSensitive) {
#if !defined(IN_VCC)
  GLog.Logf("=== CASE-%sSENSITIVE NAME MAP ===", (caseSensitive ? "" : "IN"));
  for (auto it = map.first(); it; ++it) {
    GLog.Logf(" --- <%s>", *it.getKey());
    for (VMemberBase *m = it.getValue(); m; m = (caseSensitive ? m->HashNext : m->HashNextLC)) {
      GLog.Logf("  <%s> : <%s>", *m->Name, *m->GetFullName());
    }
  }
#endif
}


//==========================================================================
//
//  VMemberBase::DumpNameMaps
//
//==========================================================================
void VMemberBase::DumpNameMaps () {
#if !defined(IN_VCC)
  if (!GArgs.CheckParm("-dev-dump-name-tables")) return;
  DumpNameMap(gMembersMap, true);
  DumpNameMap(gMembersMapLC, false);
#endif
}


//==========================================================================
//
//  VMemberBase::RemoveFromNameHash
//
//==========================================================================
void VMemberBase::RemoveFromNameHash (VMemberBase *self) {
  if (!self || self->Name == NAME_None) return;
  //fprintf(stderr, "UNREGISTERING: <%s>\n", *self->Name);
  {
    VMemberBase **mpp = gMembersMap.find(self->Name);
    if (mpp) {
      VMemberBase *mprev = nullptr, *m = *mpp;
      while (m && m != self) { mprev = m; m = m->HashNext; }
      if (m) {
        if (mprev) {
          mprev->HashNext = m->HashNext;
        } else {
          if (m->HashNext) *mpp = m->HashNext; else gMembersMap.remove(m->Name);
        }
      }
    }
  }
  // locase map
  {
    VName lname = VName(*self->Name, VName::FindLower);
    if (lname != NAME_None) {
      VMemberBase **mpp = gMembersMapLC.find(lname);
      if (mpp) {
        VMemberBase *mprev = nullptr, *m = *mpp;
        while (m && m != self) { mprev = m; m = m->HashNextLC; }
        if (m) {
          if (mprev) {
            mprev->HashNextLC = m->HashNextLC;
          } else {
            if (m->HashNextLC) *mpp = m->HashNextLC; else gMembersMapLC.remove(lname);
          }
        }
      }
    }
  }
}


//==========================================================================
//
//  VMemberBase::ForEachNamed
//
//  WARNING! don't add/remove ANY named members from callback!
//  return `false` from callback to stop (and return current member)
//
//==========================================================================
VMemberBase *VMemberBase::ForEachNamed (VName aname, FERes (*dg) (VMemberBase *m), bool caseSensitive) {
  if (!dg) return nullptr;
  if (aname == NAME_None) return nullptr; // oops
  if (!caseSensitive) {
    // use lower-case map
    aname = VName(*aname, VName::FindLower);
    if (aname == NAME_None) return nullptr; // no such name, no chance to find a member
    VMemberBase **mpp = gMembersMapLC.find(aname);
    if (!mpp) return nullptr;
    for (VMemberBase *m = *mpp; m; m = m->HashNextLC) {
      if (dg(m) == FERes::FOREACH_STOP) return m;
    }
  } else {
    // use normal map
    VMemberBase **mpp = gMembersMap.find(aname);
    if (!mpp) return nullptr;
    for (VMemberBase *m = *mpp; m; m = m->HashNext) {
      if (dg(m) == FERes::FOREACH_STOP) return m;
    }
  }
  return nullptr;
}


//==========================================================================
//
//  VMemberBase::GetFullName
//
//==========================================================================
VStr VMemberBase::GetFullName () const {
  if (Outer) return Outer->GetFullName()+"."+Name;
  return VStr(Name);
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
  check(!GObjInitialised);
  check(!GObjShuttingDown);
  // add native classes to the list
  for (VClass *C = GClasses; C; C = C->LinkNext) {
    check(C->MemberIndex == -666);
    C->MemberIndex = GMembers.Append(C);
    PutToNameHash(C);
    /*
    int HashIndex = C->Name.GetIndex()&4095;
    C->HashNext = GMembersHash[HashIndex];
    GMembersHash[HashIndex] = C;
    C->HashLowerCased();
    */
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
  check(!GObjShuttingDown);
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
  check(AName != NAME_None);
  // check if already loaded
  for (int i = 0; i < GLoadedPackages.Num(); ++i) if (GLoadedPackages[i]->Name == AName) return GLoadedPackages[i];
  GLog.WriteLine(NAME_Init, "VavoomC: loading package '%s'...", *AName);
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
VMemberBase *VMemberBase::StaticFindMember (VName AName, VMemberBase *AOuter, vuint8 AType, VName EnumName/*, bool caseSensitive*/) {
  //VName realName = AName;
  if (AType == MEMBER_Const && EnumName != NAME_None) {
    // rewrite name
    VStr nn(*EnumName);
    nn += " ";
    nn += *AName;
    AName = VName(*nn);
  }
  /*
  int HashIndex = AName.GetIndex()&4095;
  for (VMemberBase *m = GMembersHash[HashIndex]; m; m = m->HashNext) {
    if (m->Name == AName && (m->Outer == AOuter ||
        (AOuter == ANY_PACKAGE && m->Outer && m->Outer->MemberType == MEMBER_Package)) &&
        (AType == ANY_MEMBER || m->MemberType == AType))
    {
      return m;
    }
  }
  */
  //k8: FUCK YOU, SHITPP!
  /*
  VMemberBase *mm = ForEachNamed(AName, [&](VMemberBase *m) -> FERes {
    if ((m->Outer == AOuter || (AOuter == ANY_PACKAGE && m->Outer && m->Outer->MemberType == MEMBER_Package)) &&
        (AType == ANY_MEMBER || m->MemberType == AType))
    {
      return FERes::FOREACH_STOP;
    }
    return FERes::FOREACH_NEXT;
  }, caseSensitive);
  return mm;
  */
  /*
  if (!caseSensitive) {
    // use lower-case map
    AName = VName(*AName, VName::FindLower);
    if (AName == NAME_None) return nullptr; // no such name, no chance to find a member
    VMemberBase **mpp = gMembersMapLC.find(AName);
    if (!mpp) return nullptr;
    for (VMemberBase *m = *mpp; m; m = m->HashNextLC) {
      if ((m->Outer == AOuter || (AOuter == ANY_PACKAGE && m->Outer && m->Outer->MemberType == MEMBER_Package)) &&
          (AType == ANY_MEMBER || m->MemberType == AType))
      {
        return m;
      }
    }
  } else
  */
  {
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
void VMemberBase::StaticGetClassListNoCase (TArray<VStr> &list, const VStr &prefix, VClass *isaClass) {
  //FIXME: make this faster
#if 0
  int len = GMembers.length();
  for (int f = 0; f < len; ++f) {
    VMemberBase *m = GMembers[f];
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
#else
  // use locase member map, it consists mostly of classes
  for (auto it = gMembersMapLC.first(); it; ++it) {
    for (VMemberBase *m = it.getValue(); m; m = m->HashNextLC) {
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
#endif
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
#if 0
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
#else
  {
    int len = gPackageList.length();
    for (int f = 0; f < len; ++f) {
      VPackage *pkg = gPackageList[f];
      if (pkg->IsKnownEnum(Name)) return VFieldType(TYPE_Int);
    }
  }
#endif

  return VFieldType(TYPE_Unknown);
}


//==========================================================================
//
//  VMemberBase::StaticFindClass
//
//==========================================================================
VClass *VMemberBase::StaticFindClass (VName AName, bool caseSensitive) {
  if (AName == NAME_None) return nullptr;
  //VMemberBase *m = StaticFindMember(AName, ANY_PACKAGE, MEMBER_Class, NAME_None, caseSensitive);
  //if (m) return (VClass *)m;
#if 0
  if (caseSensitive) {
    // use normal map
    VMemberBase **mpp = gMembersMap.find(AName);
    if (!mpp) return nullptr;
    for (VMemberBase *m = *mpp; m; m = m->HashNext) {
      if (m->Outer && m->Outer->MemberType == MEMBER_Package && m->MemberType == MEMBER_Class) {
        return (VClass *)m;
      }
    }
  } else
#endif
  // classes cannot be duplicated, so we can use much smaller lower-case map to find class
  {
    // use lower-case map
    VName loname = VName(*AName, VName::FindLower);
    if (loname == NAME_None) return nullptr; // no such name, no chance to find a member
    VMemberBase **mpp = gMembersMapLC.find(loname);
    if (!mpp) return nullptr;
    for (VMemberBase *m = *mpp; m; m = m->HashNextLC) {
      if (m->Outer && m->Outer->MemberType == MEMBER_Package && m->MemberType == MEMBER_Class) {
        if (caseSensitive && m->Name != AName) continue;
        return (VClass *)m;
      }
    }
  }
  return nullptr;
}


//==========================================================================
//
//  VMemberBase::StaticFindClassNoCase
//
//==========================================================================
/*
VClass *VMemberBase::StaticFindClassNoCase (VName AName) {
  VMemberBase *m = StaticFindMemberNoCase(AName, ANY_PACKAGE, MEMBER_Class);
  if (m) return (VClass *)m;
  return nullptr;
}
*/


/*
//==========================================================================
//
//  VMemberBase::StaticFindMObj
//
//==========================================================================
VClass *VMemberBase::StaticFindMObj (vint32 id, VName pkgname) {
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
}


//==========================================================================
//
//  VMemberBase::StaticFindMObjInfo
//
//==========================================================================
mobjinfo_t *VMemberBase::StaticFindMObjInfo (vint32 id) {
  if (id == 0) return nullptr;
  int len = VClass::GMobjInfos.length();
  for (int f = 0; f < len; ++f) {
    mobjinfo_t *nfo = &VClass::GMobjInfos[f];
    if (nfo->DoomEdNum == id) return nfo;
  }
  return nullptr;
}


//==========================================================================
//
//  VMemberBase::StaticFindScriptId
//
//==========================================================================
VClass *VMemberBase::StaticFindScriptId (vint32 id, VName pkgname) {
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
}
*/


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
void VMemberBase::StaticSplitStateLabel (const VStr &LabelName, TArray<VName> &Parts, bool appendToParts) {
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
static VStream *pkgOpenFileDG (VLexer *self, const VStr &filename) {
  if (VMemberBase::dgOpenFile) return VMemberBase::dgOpenFile(filename, VMemberBase::userdata);
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  return FL_OpenFileRead(filename);
#else
  return fsysOpenFile(filename);
#endif
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
