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


#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
# define vdlogf(...)  if (VObject::cliShowLoadingMessages) GLog.Logf(NAME_Init, __VA_ARGS__)
#else
# define vdlogf(...)  do {} while (0)
#endif


#include "vc_package_vcc.cpp"


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
//  VPackage::Serialise
//
//==========================================================================
void VPackage::Serialise (VStream &Strm) {
  VMemberBase::Serialise(Strm);
  vuint8 xver = 0; // current version is 0
  Strm << xver;
  // enums
  vint32 acount = (vint32)KnownEnums.count();
  Strm << STRM_INDEX(acount);
  if (Strm.IsLoading()) {
    KnownEnums.clear();
    while (acount-- > 0) {
      VName ename;
      Strm << ename;
      KnownEnums.put(ename, true);
    }
  } else {
    for (auto it = KnownEnums.first(); it; ++it) {
      VName ename = it.getKey();
      Strm << ename;
    }
  }
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
//  VPackage::Emit
//
//==========================================================================
void VPackage::Emit () {
  vdlogf("Importing packages for '%s'...", *Name);
  for (auto &&pkg : PackagesToLoad) {
    vdlogf("  importing package '%s'...", *pkg.Name);
    pkg.Pkg = StaticLoadPackage(pkg.Name, pkg.Loc);
  }

  if (vcErrorCount) BailOut();

  vdlogf("Defining constants for '%s'...", *Name);
  for (auto &&cc : ParsedConstants) {
    vdlogf("  defining constant '%s'...", *cc->Name);
    cc->Define();
  }

  vdlogf("Defining structs for '%s'...", *Name);
  for (auto &&st : ParsedStructs) {
    vdlogf("  defining struct '%s'...", *st->Name);
    st->Define();
  }

  vdlogf("Defining classes for '%s'...", *Name);
  for (auto &&cls : ParsedClasses) {
    vdlogf("  defining class '%s'...", *cls->Name);
    cls->Define();
  }

  if (vcErrorCount) BailOut();

  for (auto &&dcl : ParsedDecorateImportClasses) {
    vdlogf("  defining decorate import class '%s'...", *dcl->Name);
    dcl->Define();
  }

  if (vcErrorCount) BailOut();

  vdlogf("Defining struct members for '%s'...", *Name);
  for (auto &&st : ParsedStructs) {
    vdlogf("  defining members for struct '%s'...", *st->Name);
    st->DefineMembers();
  }

  vdlogf("Defining class members for '%s'...", *Name);
  for (auto &&cls : ParsedClasses) {
    vdlogf("  defining members for class '%s'...", *cls->Name);
    cls->DefineMembers();
  }

  if (vcErrorCount) BailOut();

  vdlogf("Emiting classes for '%s'...", *Name);
  for (auto &&cls : ParsedClasses) {
    vdlogf("  emitting class '%s'...", *cls->Name);
    cls->Emit();
  }

  if (vcErrorCount) BailOut();

  vdlogf("Emiting package '%s' complete!", *Name);
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

#if !defined(IN_VCC)
  //vdlogf("VPackage::LoadSourceObject: package '%s'...", *Name);

  #if !defined(VCC_STANDALONE_EXECUTOR)
  GLog.Logf(NAME_Init, "VavoomC: generating code for package '%s'...", *Name);
  #endif
  for (auto &&mm : GMembers) {
    if (mm->IsIn(this)) {
      //vdlogf("  package '%s': calling `PostLoad()` for %s `%s`", *Name, mm->GetMemberTypeString(), *mm->Name);
      mm->PostLoad();
    }
  }

  // create default objects
  for (auto &&cls : ParsedClasses) {
    cls->CreateDefaults();
    if (!cls->Outer) cls->Outer = this;
  }

  #if !defined(VCC_STANDALONE_EXECUTOR)
  // we need to do this, 'cause k8vavoom 'engine' package has some classes w/o definitions (`Acs`, `Button`)
  if (Name == NAME_engine) {
    for (VClass *Cls = GClasses; Cls; Cls = Cls->LinkNext) {
      if (!Cls->Outer && Cls->MemberType == MEMBER_Class) {
        Sys_Error("package `engine` has hidden class `%s`", *Cls->Name);
        /*k8: this wasn't fatal, but now it is
        Cls->PostLoad();
        Cls->CreateDefaults();
        Cls->Outer = this;
        */
      }
    }
  }
  #endif
#endif
}


//==========================================================================
//
// VPackage::LoadObject
//
//==========================================================================
void VPackage::LoadObject (TLocation l) {
  static const char *pkgImportFiles[] = {
    "0package.vc",
    "package.vc",
    "0classes.vc",
    "classes.vc",
    nullptr
  };

#if defined(IN_VCC)
  vdlogf("Loading package %s", *Name);

  // load PROGS from a specified file
  VStream *f = fsysOpenFileSimple(va("%s.dat", *Name));
  if (f) { LoadBinaryObject(f, va("%s.dat", *Name), l); return; }

  for (int i = 0; i < GPackagePath.length(); ++i) {
    for (const char **pif = pkgImportFiles; *pif; ++pif) {
      //VStr mainVC = va("%s/progs/%s/%s", *GPackagePath[i], *Name, *pif);
      VStr mainVC = va("%s/%s/%s", *GPackagePath[i], *Name, *pif);
      //GLog.Logf(": <%s>", *mainVC);
      VStream *Strm = fsysOpenFileSimple(*mainVC);
      if (Strm) {
        LoadSourceObject(Strm, mainVC, l);
        return;
      }
    }
  }

  ParseError(l, "Can't find package %s", *Name);

#elif defined(VCC_STANDALONE_EXECUTOR)
  vdlogf("Loading package '%s'...", *Name);

  for (int i = 0; i < GPackagePath.length(); ++i) {
    for (const char **pif = pkgImportFiles; *pif; ++pif) {
      VStr mainVC = GPackagePath[i]+"/"+Name+"/"+(*pif);
      vdlogf("  <%s>", *mainVC);
      VStream *Strm = fsysOpenFileSimple(mainVC);
      if (Strm) { vdlogf("  '%s'", *mainVC); LoadSourceObject(Strm, mainVC, l); return; }
    }
  }

  // if no package pathes specified, try "packages/"
  if (GPackagePath.length() == 0) {
    for (const char **pif = pkgImportFiles; *pif; ++pif) {
      VStr mainVC = VStr("packages/")+Name+"/"+(*pif);
      VStream *Strm = fsysOpenFileSimple(mainVC);
      if (Strm) { vdlogf("  '%s'", *mainVC); LoadSourceObject(Strm, mainVC, l); return; }
    }
  }

  ParseError(l, "Can't find package %s", *Name);
  BailOut();

#else
  // main engine
  for (const char **pif = pkgImportFiles; *pif; ++pif) {
    VStr mainVC = va("progs/%s/%s", *Name, *pif);
    if (FL_FileExists(*mainVC)) {
      // compile package
      //fprintf(stderr, "Loading package '%s' (%s)...\n", *Name, *mainVC);
      VStream *Strm = FL_OpenFileRead(*mainVC);
      LoadSourceObject(Strm, mainVC, l);
      return;
    }
  }
  Sys_Error("Progs package %s not found", *Name);
#endif
}
