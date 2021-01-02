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
//**  Copyright (C) 2018-2021 Ketmar Dark
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
#include "vc_local.h"


#define vdlogf(...)  if (VObject::cliShowLoadingMessages) GLog.Logf(NAME_Init, __VA_ARGS__)


TArray<VPackage *> VPackage::PackagesToEmit;


// ////////////////////////////////////////////////////////////////////////// //
struct Lists {
  //TArray<VClass *> &oldList;
  TMapNC<VName, VClass *> nameMap; // because classes aren't defined yet
  TMapNC<VClass *, int> oldIndex;
  TArray<VClass *> newList;
  TMapNC<VClass *, int> newIndex;
};


// ////////////////////////////////////////////////////////////////////////// //
struct PLMember {
  VMemberBase *m;
  PLMember *next;

  inline PLMember () noexcept : m(nullptr), next(nullptr) {}
};


// ////////////////////////////////////////////////////////////////////////// //
struct PLMemberList {
  PLMember *head;
  PLMember *tail;

  inline PLMemberList () noexcept : head(nullptr), tail(nullptr) {}
  inline ~PLMemberList () noexcept { clear(); }

  void clear () noexcept {
    while (head) {
      PLMember *c = head;
      head = head->next;
      delete c;
    }
    head = tail = nullptr;
  }

  void append (PLMember *m) noexcept {
    if (!m) return; // just in case
    vassert(!m->next);
    if (tail) tail->next = m; else { vassert(!head); head = m; }
    tail = m;
  }

  void append (VMemberBase *m) noexcept {
    if (!m) return;
    PLMember *n = new PLMember();
    n->m = m;
    append(n);
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct PLPkgInfo {
  VPackage *pkg;
  PLMemberList structs;
  PLMemberList others;

  inline PLPkgInfo () noexcept : pkg(nullptr), structs(), others() {}
  inline ~PLPkgInfo () noexcept { clear(); }

  void clear () noexcept {
    structs.clear();
    others.clear();
  }
};


//==========================================================================
//
//  PutClassToList
//
//==========================================================================
static void PutClassToList (Lists &l, VClass *cls) {
  if (!cls) return;
  if (!l.oldIndex.has(cls)) return; // not our class
  if (l.newIndex.has(cls)) return; // already processed
  // put parent first
  auto pcp = l.nameMap.find(cls->ParentClassName);
  VClass *parent = (pcp ? *pcp : nullptr);
  //GLog.Logf(NAME_Debug, "CLASS '%s': parent is '%s' (%s)", *cls->GetFullName(), *cls->ParentClassName, (parent ? *parent->GetFullName() : "<none>"));
  vassert(parent != cls);
  if (parent && l.oldIndex.has(parent)) {
    if (!l.newIndex.has(parent)) {
      ParseWarning(cls->Loc, "class `%s` is defined before class `%s` (at %s)", cls->GetName(), parent->GetName(), *parent->Loc.toStringNoCol());
      PutClassToList(l, parent);
    }
  }
  // now put us
  //GLog.Logf(NAME_Debug, "PutClassToList: %s : %s", *cls->GetFullName(), (cls->GetSuperClass() ? *cls->GetSuperClass()->GetFullName() : "FUCK!"));
  //GLog.Logf(NAME_Debug, "PutClassToList: %s : %s", *cls->GetFullName(), (parent ? *parent->GetFullName() : "<none>"));
  l.newIndex.put(cls, l.newList.length());
  l.newList.append(cls);
}


//==========================================================================
//
//  VPackage::GetPkgImportFile
//
//  returns `nullptr` on list end; starts with 0
//
//==========================================================================
const char *VPackage::GetPkgImportFile (unsigned idx) {
  switch (idx) {
    case 0: return "0package.vc";
    case 1: return "package.vc";
    case 2: return "0classes.vc";
    case 3: return "classes.vc";
  }
  return nullptr;
};


//==========================================================================
//
//  VPackage::VPackage
//
//==========================================================================
VPackage::VPackage ()
  : VMemberBase(MEMBER_Package, NAME_None, nullptr, TLocation())
  , StringCount(0)
  , KnownEnums()
  , NumBuiltins(0)
{
  InitStringPool();
}


//==========================================================================
//
//  VPackage::VPackage
//
//==========================================================================
VPackage::VPackage (VName AName)
  : VMemberBase(MEMBER_Package, AName, nullptr, TLocation())
  , KnownEnums()
  , NumBuiltins(0)
{
  InitStringPool();
}


//==========================================================================
//
//  VPackage::~VPackage
//
//==========================================================================
VPackage::~VPackage () {
}


//==========================================================================
//
//  VPackage::~VPackage
//
//==========================================================================
void VPackage::InitStringPool () {
  // strings
  memset(StringLookup, 0, sizeof(StringLookup));
  // 1-st string is empty
  StringInfo.Alloc();
  StringInfo[0].Offs = 0;
  StringInfo[0].Next = 0;
  StringCount = 1;
  StringInfo[0].str = VStr::EmptyString;
}


//==========================================================================
//
//  VPackage::CompilerShutdown
//
//==========================================================================
void VPackage::CompilerShutdown () {
  VMemberBase::CompilerShutdown();
  ParsedConstants.clear();
  ParsedStructs.clear();
  ParsedClasses.clear();
  ParsedDecorateImportClasses.clear();
}


//==========================================================================
//
//  VPackage::StringHashFunc
//
//==========================================================================
vuint32 VPackage::StringHashFunc (const char *str) {
  //return (str && str[0] ? (str[0]^(str[1]<<4))&0xff : 0);
  return (str && str[0] ? joaatHashBuf(str, strlen(str)) : 0)&0x0fffU;
}


//==========================================================================
//
//  VPackage::FindString
//
//  Return offset in strings array.
//
//==========================================================================
int VPackage::FindString (const char *str) {
  if (!str || !str[0]) return 0;

  vuint32 hash = StringHashFunc(str);
  for (int i = StringLookup[hash]; i; i = StringInfo[i].Next) {
    if (StringInfo[i].str.Cmp(str) == 0) return StringInfo[i].Offs;
  }

  // add new string
  int Ofs = StringInfo.length();
  vassert(Ofs == StringCount);
  ++StringCount;
  TStringInfo &SI = StringInfo.Alloc();
  // remember string
  SI.str = VStr(str);
  SI.str.makeImmutable();
  SI.Offs = Ofs;
  SI.Next = StringLookup[hash];
  StringLookup[hash] = StringInfo.length()-1;
  return SI.Offs;
}


//==========================================================================
//
//  VPackage::FindString
//
//  Return offset in strings array.
//
//==========================================================================
int VPackage::FindString (VStr s) {
  if (s.length() == 0) return 0;
  return FindString(*s);
}


//==========================================================================
//
//  VPackage::GetStringByIndex
//
//==========================================================================
const VStr &VPackage::GetStringByIndex (int idx) {
  if (idx < 0 || idx >= StringInfo.length()) return VStr::EmptyString;
  return StringInfo[idx].str;
}


//==========================================================================
//
//  VPackage::IsKnownEnum
//
//==========================================================================
bool VPackage::IsKnownEnum (VName EnumName) {
  return KnownEnums.has(EnumName);
}


//==========================================================================
//
//  VClass::AddKnownEnum
//
//==========================================================================
bool VPackage::AddKnownEnum (VName EnumName) {
  if (IsKnownEnum(EnumName)) return true;
  KnownEnums.put(EnumName, true);
  return false;
}


//==========================================================================
//
//  VPackage::FindConstant
//
//==========================================================================
VConstant *VPackage::FindConstant (VName Name, VName EnumName) {
  VMemberBase *m = StaticFindMember(Name, this, MEMBER_Const, EnumName);
  if (m) return (VConstant *)m;
  return nullptr;
}


//==========================================================================
//
//  VPackage::FindDecorateImportClass
//
//==========================================================================
VClass *VPackage::FindDecorateImportClass (VName AName) const {
  for (int i = 0; i < ParsedDecorateImportClasses.Num(); ++i) {
    if (ParsedDecorateImportClasses[i]->Name == AName) {
      return ParsedDecorateImportClasses[i];
    }
  }
  return nullptr;
}


//==========================================================================
//
//  VPackage::SortParsedClasses
//
//  this tries to sort parsed classes so subclasses will be
//  defined after superclasses
//
//==========================================================================
void VPackage::SortParsedClasses () {
  if (ParsedClasses.length() == 0) return;
  //GLog.Log(NAME_Debug, "Sorting classes");
  // build class map
  Lists l;
  for (auto &&it : ParsedClasses.itemsIdx()) {
    l.nameMap.put(it.value()->Name, it.value());
    l.oldIndex.put(it.value(), it.index());
  }
  // build new list
  for (auto &&cls : ParsedClasses) PutClassToList(l, cls);
  // sanity check
  vassert(l.newList.length() == ParsedClasses.length());
  for (auto &&cls : l.newList) {
    vassert(l.oldIndex.has(cls));
    vassert(l.newIndex.has(cls));
  }
  // update list
  ParsedClasses = l.newList;
}


//==========================================================================
//
//  VPackage::Emit
//
//==========================================================================
void VPackage::Emit () {
  vdlogf("Importing packages for '%s'", *Name);
  for (auto &&pkg : PackagesToLoad) {
    vdlogf("  importing package '%s'", *pkg.Name);
    pkg.Pkg = StaticLoadPackage(pkg.Name, pkg.Loc);
  }

  SortParsedClasses();

  if (vcErrorCount) BailOut();

  vdlogf("Defining constants for '%s'", *Name);
  for (auto &&cc : ParsedConstants) {
    vdlogf("  defining constant '%s'", *cc->Name);
    cc->Define();
  }

  vdlogf("Defining structs for '%s'", *Name);
  for (auto &&st : ParsedStructs) {
    vdlogf("  defining struct '%s'", *st->Name);
    st->Define();
  }

  vdlogf("Defining classes for '%s'", *Name);
  for (auto &&cls : ParsedClasses) {
    vdlogf("  defining class '%s'", *cls->Name);
    cls->Define();
  }

  if (vcErrorCount) BailOut();

  for (auto &&dcl : ParsedDecorateImportClasses) {
    vdlogf("  defining decorate import class '%s'", *dcl->Name);
    dcl->Define();
  }

  if (vcErrorCount) BailOut();

  vdlogf("Defining struct members for '%s'", *Name);
  for (auto &&st : ParsedStructs) {
    vdlogf("  defining members for struct '%s'", *st->Name);
    st->DefineMembers();
  }

  vdlogf("Defining class members for '%s'", *Name);
  for (auto &&cls : ParsedClasses) {
    vdlogf("  defining members for class '%s'", *cls->Name);
    cls->DefineMembers();
  }

  if (vcErrorCount) BailOut();

  /*
  vdlogf("Emiting classes for '%s'", *Name);
  for (auto &&cls : ParsedClasses) {
    vdlogf("  emitting class '%s'", *cls->Name);
    cls->Emit();
  }

  if (vcErrorCount) BailOut();

  vdlogf("Emiting package '%s' complete!", *Name);
  */
  bool found = false;
  for (auto &&pkg : PackagesToEmit) if (pkg == this) { found = true; break; }
  if (!found) PackagesToEmit.append(this);
}


//==========================================================================
//
//  VPackage::StaticEmitPackages
//
//  this emits code for all `PackagesToEmit()`
//
//==========================================================================
void VPackage::StaticEmitPackages () {
  if (PackagesToEmit.length() == 0) return;

  bool wasEngine = false;

  // create two postload lists: structs and others
  // we want to postload structs before emiting code, so struct sizes will be known

  TArray<PLPkgInfo> pinfo;
  pinfo.setLength(PackagesToEmit.length());

  // for faster loopkups: key is package, value is package index
  TMapNC<VPackage *, int> pidMap;
  for (auto &&it : PackagesToEmit.itemsIdx()) {
    wasEngine = wasEngine || (it.value()->Name == NAME_engine);
    vassert(it.value());
    pidMap.put(it.value(), it.index());
    pinfo[it.index()].pkg = it.value();
  }

  // create lists
  for (auto &&mm : GMembers) {
    VPackage *pkg = mm->GetPackageRelaxed();
    if (!pkg) continue;
    auto pp = pidMap.find(pkg);
    if (pp) {
      const int pidx = *pp;
      vassert(pinfo[pidx].pkg == pkg);
      if (mm->isStructMember()) {
        pinfo[pidx].structs.append(mm);
      } else {
        pinfo[pidx].others.append(mm);
      }
    }
  }

  // we don't need the map anymore
  pidMap.clear();

  // postload structs
  for (auto &&pi : pinfo) {
    vassert(pi.pkg);
    //GLog.Logf(NAME_Debug, "postloading structs for package `%s`", *pi.pkg->Name);
    for (PLMember *pm = pi.structs.head; pm; pm = pm->next) {
      //GLog.Logf(NAME_Debug, "  struct `%s`", *pm->m->Name);
      pm->m->PostLoad();
    }
    // we can free struct list now
    pi.structs.clear();
    if (vcErrorCount) BailOut();
  }

  // emit classes
  for (auto &&pkg : PackagesToEmit) {
    if (pkg->ParsedClasses.length() > 0) {
      vdlogf("Emiting %d class%s for '%s'", pkg->ParsedClasses.length(), (pkg->ParsedClasses.length() != 1 ? "es" : ""), *pkg->Name);
      for (auto &&cls : pkg->ParsedClasses) {
        vdlogf("  emitting class '%s' (parent is '%s')", *cls->Name, (cls->ParentClass ? *cls->ParentClass->Name : "none"));
        cls->Emit();
      }
      if (vcErrorCount) BailOut();
    }
  }

  // postload everything except structs
  //if (!VObject::compilerDisablePostloading)
  {
    /*
    for (auto &&pkg : PackagesToEmit) {
      if (VObject::cliShowPackageLoading) GLog.Logf(NAME_Init, "VavoomC: generating code for package '%s'", *pkg->Name);
      for (auto &&mm : GMembers) {
        if (mm->IsIn(pkg)) {
          //vdlogf("  package '%s': calling `PostLoad()` for %s `%s`", *pkg->Name, mm->GetMemberTypeString(), *mm->Name);
          mm->PostLoad();
        }
      }
      if (vcErrorCount) BailOut();
    }
    */
    for (auto &&pi : pinfo) {
      vassert(pi.pkg);
      if (VObject::cliShowPackageLoading) GLog.Logf(NAME_Init, "VavoomC: generating code for package '%s'", *pi.pkg->Name);
      for (PLMember *pm = pi.others.head; pm; pm = pm->next) {
        //GLog.Logf(NAME_Debug, "  `%s`", *pm->m->Name);
        pm->m->PostLoad();
      }
      // we can free others list now
      pi.others.clear();
      if (vcErrorCount) BailOut();
    }

    // create defaultproperties for all classes
    for (auto &&pkg : PackagesToEmit) {
      // create default objects
      for (auto &&cls : pkg->ParsedClasses) {
        cls->CreateDefaults();
        if (!cls->Outer) cls->Outer = pkg;
      }
    }

    // we need to do this, 'cause k8vavoom 'engine' package has some classes w/o definitions (`Acs`, `Button`)
    if (wasEngine && !VObject::engineAllowNotImplementedBuiltins) {
      for (VClass *Cls = GClasses; Cls; Cls = Cls->LinkNext) {
        if (!Cls->Outer && Cls->MemberType == MEMBER_Class) {
          VCFatalError("package `engine` has hidden class `%s`", *Cls->Name);
        }
      }
    }
  }

  PackagesToEmit.clear();
}


//==========================================================================
//
//  VPackage::LoadSourceObject
//
//  will delete `Strm`
//
//==========================================================================
void VPackage::LoadSourceObject (VStream *Strm, VStr filename, TLocation l) {
  if (!Strm) return;

  VLexer Lex;
  VMemberBase::InitLexer(Lex);
  Lex.OpenSource(Strm, filename);
  VParser Parser(Lex, this);
  Parser.Parse();
  Emit();
}
