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
class DummyClass1 {
public:
  void *Pointer;
  vuint8 Byte1;
  virtual ~DummyClass1() {}
  virtual void Dummy () = 0;
};

class DummyClass2 : public DummyClass1 {
public:
  vuint8 Byte2;
};


// ////////////////////////////////////////////////////////////////////////// //
TArray<VName> VClass::GSpriteNames;
//VClass *VClass::GLowerCaseHashTable[VClass::LOWER_CASE_HASH_SIZE];

static TArray<mobjinfo_t> GMobjInfos;
static TArray<mobjinfo_t> GScriptIds;
static TMapNC<vint32, vint32> GMobj2Arr; // key: doomedidx, value: index in GMobjInfos
static TMapNC<vint32, vint32> GSId2Arr; // key: doomedidx, value: index in GScriptIds


#if !defined(IN_VCC)
struct XXMInfo {
  int idx;
  mobjinfo_t nfo;
};

extern "C" {
  static int cmpobjnfo (const void *aa, const void *bb, void *) {
    if (aa == bb) return 0;
    const XXMInfo *a = (const XXMInfo *)aa;
    const XXMInfo *b = (const XXMInfo *)bb;
    if (a->nfo.DoomEdNum < b->nfo.DoomEdNum) return -1;
    if (a->nfo.DoomEdNum > b->nfo.DoomEdNum) return 1;
    if (a->idx < b->idx) return -1;
    if (a->idx > b->idx) return 1;
    return 0;
    //return (a->nfo.DoomEdNum-b->nfo.DoomEdNum);
  }
}
#endif


//==========================================================================
//
//  operator VStream << mobjinfo_t
//
//==========================================================================
/*
VStream &operator << (VStream &Strm, mobjinfo_t &MI) {
  return Strm << STRM_INDEX(MI.DoomEdNum)
    << STRM_INDEX(MI.GameFilter)
    << MI.flags
    << MI.special << MI.args[0] << MI.args[1] << MI.args[2] << MI.args[3] << MI.args[4]
    << MI.Class;
}
*/


//==========================================================================
//
//  VClass::VClass
//
//==========================================================================
VClass::VClass (VName AName, VMemberBase *AOuter, const TLocation &ALoc)
  : VMemberBase(MEMBER_Class, AName, AOuter, ALoc)
  , ParentClass(nullptr)
  , Fields(nullptr)
  , States(nullptr)
  , DefaultProperties(nullptr)
  , ParentClassName(NAME_None)
  , DoesReplacement(ReplaceType::Replace_None)
  , GameExpr(nullptr)
  , MobjInfoExpr(nullptr)
  , ScriptIdExpr(nullptr)
  , Defined(true)
  , DefinedAsDependency(false)
  , dfStateTexList()
  , dfStateTexDir()
  , dfStateTexDirSet(0)
  , ObjectFlags(0)
  , LinkNext(nullptr)
  , ClassSize(0)
  , ClassUnalignedSize(0)
  , ClassFlags(0)
  , ClassVTable(nullptr)
  , ClassConstructor(nullptr)
  , ClassNumMethods(0)
  , ReferenceFields(nullptr)
  , DestructorFields(nullptr)
  , NetFields(nullptr)
  , NetMethods(nullptr)
  , NetStates(nullptr)
  , NumNetFields(0)
  , Defaults(nullptr)
  , Replacement(nullptr)
  , Replacee(nullptr)
  , AliasList()
  , AliasFrameNum(0)
  , KnownEnums()
{
  LinkNext = GClasses;
  GClasses = this;
  ClassGameObjName = NAME_None;
  //HashLowerCased();
}


//==========================================================================
//
//  VClass::VClass
//
//==========================================================================
VClass::VClass (ENativeConstructor, size_t ASize, vuint32 AClassFlags, VClass *AParent, EName AName, void (*ACtor) ())
  : VMemberBase(MEMBER_Class, AName, nullptr, TLocation())
  , ParentClass(AParent)
  , Fields(nullptr)
  , States(nullptr)
  , DefaultProperties(nullptr)
  , ParentClassName(NAME_None)
  , DoesReplacement(ReplaceType::Replace_None)
  , GameExpr(nullptr)
  , MobjInfoExpr(nullptr)
  , ScriptIdExpr(nullptr)
  , Defined(true)
  , DefinedAsDependency(false)
  , ObjectFlags(CLASSOF_Native)
  , LinkNext(nullptr)
  , ClassSize(ASize)
  , ClassUnalignedSize(ASize)
  , ClassFlags(AClassFlags)
  , ClassVTable(nullptr)
  , ClassConstructor(ACtor)
  , ClassNumMethods(0)
  , ReferenceFields(nullptr)
  , DestructorFields(nullptr)
  , NetFields(nullptr)
  , NetMethods(nullptr)
  , NetStates(nullptr)
  , NumNetFields(0)
  , Defaults(nullptr)
  , Replacement(nullptr)
  , Replacee(nullptr)
  , AliasList()
  , AliasFrameNum(0)
  , KnownEnums()
{
  LinkNext = GClasses;
  GClasses = this;
  ClassGameObjName = NAME_None;
}


//==========================================================================
//
//  VClass::~VClass
//
//==========================================================================
VClass::~VClass () {
  delete GameExpr; GameExpr = nullptr;
  delete MobjInfoExpr; MobjInfoExpr = nullptr;
  delete ScriptIdExpr; ScriptIdExpr = nullptr;

  if (ClassVTable) {
    delete[] ClassVTable;
    ClassVTable = nullptr;
  }
#if !defined(IN_VCC)
  if (Defaults) {
    DestructObject((VObject*)Defaults);
    delete[] Defaults;
    Defaults = nullptr;
  }
#endif

  if (!GObjInitialised || GObjShuttingDown) return;

  // unlink from classes list
  if (GClasses == this) {
    GClasses = LinkNext;
  } else {
    VClass *Prev = GClasses;
    while (Prev && Prev->LinkNext != this) Prev = Prev->LinkNext;
    if (Prev) {
      Prev->LinkNext = LinkNext;
    } else {
#if !defined(IN_VCC)
      GLog.Log(NAME_Dev, "VClass Unlink: Class not in list");
#endif
    }
  }
}


//==========================================================================
//
//  VClass::CompilerShutdown
//
//==========================================================================
void VClass::CompilerShutdown () {
  VMemberBase::CompilerShutdown();
  delete GameExpr; GameExpr = nullptr;
  delete MobjInfoExpr; MobjInfoExpr = nullptr;
  delete ScriptIdExpr; ScriptIdExpr = nullptr;
  Structs.clear();
  Constants.clear();
  Properties.clear();
  StateLabelDefs.clear();
  //AliasList.clear();
}


//==========================================================================
//
//  VClass::ResolveAlias
//
//  returns `aname` for unknown alias, or `NAME_None` for alias loop
//
//==========================================================================
VName VClass::ResolveAlias (VName aname, bool nocase) {
  if (aname == NAME_None) return NAME_None;
  if (++AliasFrameNum == 0x7fffffff) {
    for (auto it = AliasList.first(); it; ++it) it.getValue().aframe = 0;
    AliasFrameNum = 1;
  }
  VName res = aname;
  for (;;) {
    if (nocase) {
      bool found = false;
      for (auto it = AliasList.first(); it; ++it) {
        if (VStr::ICmp(*it.getKey(), *aname) == 0) {
          if (it.getValue().aframe == AliasFrameNum) return NAME_None; // loop
          res = it.getValue().origName;
          it.getValue().aframe = AliasFrameNum;
          aname = res;
          found = true;
          break;
        }
      }
      if (!found) {
        if (!ParentClass) return res;
        return ParentClass->ResolveAlias(res, nocase);
      }
    } else {
      auto ai = AliasList.get(aname);
      if (!ai) {
        if (!ParentClass) return res;
        return ParentClass->ResolveAlias(res);
      }
      if (ai->aframe == AliasFrameNum) return NAME_None; // loop
      res = ai->origName;
      ai->aframe = AliasFrameNum;
      aname = res;
    }
  }
}


//==========================================================================
//
//  VClass::IsKnownEnum
//
//==========================================================================
bool VClass::IsKnownEnum (VName EnumName) {
  if (KnownEnums.has(EnumName)) return true;
  if (!ParentClass) return false;
  return ParentClass->IsKnownEnum(EnumName);
}


//==========================================================================
//
//  VClass::AddKnownEnum
//
//==========================================================================
bool VClass::AddKnownEnum (VName EnumName) {
  if (IsKnownEnum(EnumName)) return true;
  KnownEnums.put(EnumName, true);
  return false;
}


//==========================================================================
//
//  VClass::FindClass
//
//==========================================================================
VClass *VClass::FindClass (const char *AName) {
  if (!AName || !AName[0]) return nullptr;
  VName TempName(AName, VName::Find);
  if (TempName == NAME_None) return nullptr; // no such name, no chance to find a class
  return (VClass *)VMemberBase::ForEachNamed(TempName, [](VMemberBase *m) { return (m->MemberType == MEMBER_Class ? FOREACH_STOP : FOREACH_NEXT); });
  /*
  for (VClass *Cls = GClasses; Cls; Cls = Cls->LinkNext) {
    if (Cls->GetVName() == TempName && Cls->MemberType == MEMBER_Class) return Cls;
  }
  return nullptr;
  */
}


//==========================================================================
//
//  VClass::FindClassLowerCase
//
//==========================================================================
VClass *VClass::FindClassLowerCase (VName AName) {
  if (AName == NAME_None) return nullptr;
  /*
  int HashIndex = GetTypeHash(AName)&(LOWER_CASE_HASH_SIZE-1);
  for (VClass *Probe = GLowerCaseHashTable[HashIndex]; Probe; Probe = Probe->LowerCaseHashNext) {
    if (Probe->LowerCaseName == AName) return Probe;
  }
  return nullptr;
  */
  return (VClass *)VMemberBase::ForEachNamedCI(AName, [](VMemberBase *m) { return (m->MemberType == MEMBER_Class ? FOREACH_STOP : FOREACH_NEXT); });
}


//==========================================================================
//
//  VClass::FindClassNoCase
//
//==========================================================================
VClass *VClass::FindClassNoCase (const char *AName) {
  if (!AName || !AName[0]) return nullptr;
  /*
  for (VClass *Cls = GClasses; Cls; Cls = Cls->LinkNext) {
    if (Cls->MemberType == MEMBER_Class && VStr::ICmp(Cls->GetName(), AName) == 0) return Cls;
  }
  return nullptr;
  */
  return (VClass *)VMemberBase::ForEachNamedCI(VName(AName, VName::FindLower), [](VMemberBase *m) { return (m->MemberType == MEMBER_Class ? FOREACH_STOP : FOREACH_NEXT); });
}


//==========================================================================
//
//  VClass::FindSprite
//
//==========================================================================
int VClass::FindSprite (VName Name, bool Append) {
  for (int i = 0; i < GSpriteNames.Num(); ++i) {
    if (GSpriteNames[i] == Name) return i;
  }
  if (!Append) return -1;
  return GSpriteNames.Append(Name);
}


//==========================================================================
//
//  VClass::GetSpriteNames
//
//==========================================================================
void VClass::GetSpriteNames (TArray<FReplacedString> &List) {
  for (int i = 0; i < GSpriteNames.Num(); ++i) {
    FReplacedString &R = List.Alloc();
    R.Index = i;
    R.Replaced = false;
    R.Old = VStr(GSpriteNames[i]).ToUpper();
  }
}


//==========================================================================
//
//  VClass::ReplaceSpriteNames
//
//==========================================================================
void VClass::ReplaceSpriteNames (TArray<FReplacedString> &List) {
  for (int i = 0; i < List.Num(); ++i) {
    if (!List[i].Replaced) continue;
    GSpriteNames[List[i].Index] = *List[i].New.ToLower();
  }
  // update sprite names in states
  for (int i = 0; i < VMemberBase::GMembers.Num(); ++i) {
    if (GMembers[i] && GMembers[i]->MemberType == MEMBER_State) {
      VState *S = (VState *)GMembers[i];
      S->SpriteName = GSpriteNames[S->SpriteIndex];
    }
  }
}


//==========================================================================
//
//  VClass::StaticReinitStatesLookup
//
//==========================================================================
void VClass::StaticReinitStatesLookup () {
  // clear states lookup tables
  for (VClass *C = GClasses; C; C = C->LinkNext) C->StatesLookup.Clear();
  // now init states lookup tables again
  for (VClass *C = GClasses; C; C = C->LinkNext) C->InitStatesLookup();
}


//==========================================================================
//
//  VClass::Serialise
//
//==========================================================================
void VClass::Serialise (VStream &Strm) {
  VMemberBase::Serialise(Strm);
  vuint8 xver = 0; // current version is 0
  Strm << xver;
#if !defined(IN_VCC)
  VClass *PrevParent = ParentClass;
#endif
  // other data
  Strm << ParentClass
    << Fields
    << States
    << Methods
    << DefaultProperties
    << RepInfos
    << StateLabels
    << ClassGameObjName;
#if !defined(IN_VCC)
  if ((ObjectFlags&CLASSOF_Native) != 0 && ParentClass != PrevParent) {
    Sys_Error("Bad parent class, class %s, C++ %s, VavoomC %s)",
      GetName(), PrevParent ? PrevParent->GetName() : "(none)",
      ParentClass ? ParentClass->GetName() : "(none)");
  }
#endif
  // aliases
  vint32 acount = (vint32)AliasList.count();
  Strm << STRM_INDEX(acount);
  if (Strm.IsLoading()) {
    AliasFrameNum = 0;
    AliasList.clear();
    while (acount-- > 0) {
      VName key;
      AliasInfo ai;
      Strm << key << ai.aliasName << ai.origName;
      ai.aframe = 0;
      AliasList.put(key, ai);
    }
  } else {
    for (auto it = AliasList.first(); it; ++it) {
      VName key = it.getKey();
      Strm << key;
      auto ai = it.getValue();
      Strm << ai.aliasName << ai.origName;
    }
  }
  // enums
  acount = (vint32)KnownEnums.count();
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
  // dfstate thingy
  Strm << STRM_INDEX(dfStateTexDirSet) << dfStateTexDir;
  acount = (vint32)dfStateTexList.count();
  Strm << STRM_INDEX(acount);
  if (Strm.IsLoading()) {
    dfStateTexList.clear();
    while (acount-- > 0) {
      VStr tname;
      TextureInfo ti;
      Strm << tname;
      Strm << ti.texImage << ti.frameWidth << ti.frameHeight << ti.frameOfsX << ti.frameOfsY;
      dfStateTexList.put(tname, ti);
    }
  } else {
    for (auto it = dfStateTexList.first(); it; ++it) {
      VStr tname = it.getKey();
      Strm << tname;
      TextureInfo &ti = it.getValue();
      Strm << ti.texImage << ti.frameWidth << ti.frameHeight << ti.frameOfsX << ti.frameOfsY;
    }
  }
  // done
}


//==========================================================================
//
//  VClass::Shutdown
//
//==========================================================================
void VClass::Shutdown () {
  if (ClassVTable) {
    delete[] ClassVTable;
    ClassVTable = nullptr;
  }
#if !defined(IN_VCC)
  if (Defaults) {
    DestructObject((VObject*)Defaults);
    delete[] Defaults;
    Defaults = nullptr;
  }
#endif
  StatesLookup.Clear();
  RepInfos.Clear();
  SpriteEffects.Clear();
  StateLabels.Clear();
  Structs.Clear();
  Constants.Clear();
  Properties.Clear();
  Methods.Clear();
  StateLabelDefs.Clear();
  DecorateStateActions.Clear();
  SpriteEffects.Clear();
  ClassGameObjName = NAME_None;
}


//==========================================================================
//
//  VClass::AddConstant
//
//==========================================================================
void VClass::AddConstant (VConstant *c) {
  Constants.Append(c);
}


//==========================================================================
//
//  VClass::AddField
//
//==========================================================================
void VClass::AddField (VField *f) {
  if (!Fields) {
    Fields = f;
  } else {
    VField *Prev = Fields;
    while (Prev->Next) Prev = Prev->Next;
    Prev->Next = f;
  }
  f->Next = nullptr;
}


//==========================================================================
//
//  VClass::AddProperty
//
//==========================================================================
void VClass::AddProperty (VProperty *p) {
  Properties.Append(p);
}


//==========================================================================
//
//  VClass::AddState
//
//==========================================================================
void VClass::AddState (VState *s) {
  if (!States) {
    States = s;
  } else {
    VState *Prev = States;
    while (Prev->Next) Prev = Prev->Next;
    Prev->Next = s;
  }
  s->Next = nullptr;
}


//==========================================================================
//
//  VClass::AddMethod
//
//==========================================================================
void VClass::AddMethod (VMethod *m) {
  Methods.Append(m);
}


//==========================================================================
//
//  VClass::FindConstant
//
//==========================================================================
VConstant *VClass::FindConstant (VName Name, VName EnumName) {
  if (Name == NAME_None) return nullptr;
  Name = ResolveAlias(Name);
  VMemberBase *m = StaticFindMember(Name, this, MEMBER_Const, EnumName);
  if (m) return (VConstant *)m;
  if (ParentClass) return ParentClass->FindConstant(Name, EnumName);
  return nullptr;
}


//==========================================================================
//
//  VClass::FindField
//
//==========================================================================
VField *VClass::FindField (VName Name, bool bRecursive) {
  if (Name == NAME_None) return nullptr;
  Name = ResolveAlias(Name);
  for (VField *F = Fields; F; F = F->Next) if (Name == F->Name) return F;
  if (bRecursive && ParentClass) return ParentClass->FindField(Name, bRecursive);
  return nullptr;
}


//==========================================================================
//
//  VClass::FindField
//
//==========================================================================
VField *VClass::FindField (VName Name, const TLocation &l, VClass *SelfClass) {
  if (Name == NAME_None) return nullptr;
  for (VClass *cls = this; cls; cls = cls->ParentClass) {
    VField *F = cls->FindField(Name, false); // non-recursive search
    if (F) {
      if ((F->Flags&FIELD_Private) && cls != SelfClass) ParseError(l, "Field `%s` is private", *F->Name);
      if ((F->Flags&FIELD_Protected) && (!SelfClass || !SelfClass->IsChildOf(cls))) ParseError(l, "Field `%s` is protected", *F->Name);
      return F;
    }
  }
  return nullptr;
}


//==========================================================================
//
//  VClass::FindFieldChecked
//
//==========================================================================
VField *VClass::FindFieldChecked (VName AName) {
  VField *F = FindField(AName);
  if (!F) Sys_Error("Field %s not found", *AName);
  return F;
}


//==========================================================================
//
//  VClass::FindProperty
//
//==========================================================================
VProperty *VClass::FindProperty (VName Name) {
  if (Name == NAME_None) return nullptr;
  Name = ResolveAlias(Name);
  VProperty *P = (VProperty*)StaticFindMember(Name, this, MEMBER_Property);
  if (P) return P;
  if (ParentClass) return ParentClass->FindProperty(Name);
  return nullptr;
}


//==========================================================================
//
//  VClass::FindMethod
//
//==========================================================================
VMethod *VClass::FindMethod (VName Name, bool bRecursive) {
  if (Name == NAME_None) return nullptr;
  Name = ResolveAlias(Name);
  VMethod *M = (VMethod *)StaticFindMember(Name, this, MEMBER_Method);
  if (M) return M;
  if (bRecursive && ParentClass) return ParentClass->FindMethod(Name, bRecursive);
  return nullptr;
}


//==========================================================================
//
//  VClass::FindMethodNoCase
//
//==========================================================================
/*
VMethod *VClass::FindMethodNoCase (VName Name, bool bRecursive) {
  if (Name == NAME_None) return nullptr;
  Name = ResolveAlias(Name, true); // nocase
  VMethod *M = (VMethod *)StaticFindMemberNoCase(Name, this, MEMBER_Method);
  if (M) return M;
  if (bRecursive && ParentClass) return ParentClass->FindMethodNoCase(Name, bRecursive);
  return nullptr;
}
*/


//==========================================================================
//
//  VClass::FindAccessibleMethod
//
//==========================================================================
VMethod *VClass::FindAccessibleMethod (VName Name, VClass *self, const TLocation *loc) {
  if (Name == NAME_None) return nullptr;
  //if (self && !loc && self->Name == "Test1") abort();
  Name = ResolveAlias(Name);
  VMethod *M = (VMethod *)StaticFindMember(Name, this, MEMBER_Method);
  if (M) {
    //fprintf(stderr, "FAM: <%s>; self=%s; this=%s; child=%d; loc=%p\n", *Name, (self ? *self->Name : "<none>"), *this->Name, (int)(self ? self->IsChildOf(this) : false), loc);
    if (loc) {
      //fprintf(stderr, "  FAM: <%s>; self=%s; this=%s; child=%d; flags=0x%04x\n", *Name, (self ? *self->Name : "<none>"), *this->Name, (int)(self ? self->IsChildOf(this) : false), M->Flags);
      if ((M->Flags&FUNC_Private) && this != self) ParseError(*loc, "Method `%s` is private", *M->Name);
      if ((M->Flags&FUNC_Protected) && (!self || !self->IsChildOf(this))) ParseError(*loc, "Method `%s` is protected", *M->Name);
      return M;
    } else {
      if (!self) {
        if ((M->Flags&(FUNC_Private|FUNC_Protected)) == 0) return M;
      } else {
        if (M->Flags&FUNC_Private) {
          if (self == this) return M;
        } else if (M->Flags&FUNC_Protected) {
          if (self->IsChildOf(this)) return M;
        } else {
          return M;
        }
      }
    }
  }
  return (ParentClass ? ParentClass->FindAccessibleMethod(Name, self, loc) : nullptr);
}


//==========================================================================
//
//  VClass::FindMethodChecked
//
//==========================================================================
VMethod *VClass::FindMethodChecked (VName AName) {
  VMethod *func = FindMethod(AName);
  if (!func) Sys_Error("Function %s not found", *AName);
  return func;
}


//==========================================================================
//
//  VClass::GetMethodIndex
//
//==========================================================================
int VClass::GetMethodIndex (VName AName) {
  if (AName == NAME_None) return -1;
  //for (int i = 0; i < ClassNumMethods; ++i) if (ClassVTable[i]->Name == AName) return i;
  //return -1;
  auto mptr = MethodMap.find(AName);
  //fprintf(stderr, "%s: %s = %d\n", *GetFullName(), *AName, (mptr ? (*mptr)->VTableIndex : -1));
  return (mptr ? (*mptr)->VTableIndex : -1);
}


//==========================================================================
//
//  VClass::FindState
//
//==========================================================================
VState *VClass::FindState (VName AName) {
  if (AName == NAME_None) return nullptr;
  if (VStr::ICmp(*AName, "none") == 0) return nullptr;
  for (VState *s = States; s; s = s->Next) {
    if (VStr::ICmp(*s->Name, *AName) == 0) return s;
  }
  if (ParentClass) return ParentClass->FindState(AName);
  if (VStr::ICmp(*AName, "null") == 0) return nullptr;
  return nullptr;
}


//==========================================================================
//
//  VClass::FindStateChecked
//
//==========================================================================
VState *VClass::FindStateChecked (VName AName) {
  if (AName == NAME_None) return nullptr;
  VState *s = FindState(AName);
  if (!s) {
    //HACK!
    if (VStr::ICmp(*AName, "none") == 0 || VStr::ICmp(*AName, "null") == 0 || VStr::ICmp(*AName, "nil") == 0) return nullptr;
    Sys_Error("State `%s` not found", *AName);
  }
  return s;
}


//==========================================================================
//
//  VClass::FindStateLabel
//
//==========================================================================
VStateLabel *VClass::FindStateLabel (VName AName, VName SubLabel, bool Exact) {
  if (AName == NAME_None || VStr::ICmp(*AName, "None") == 0 || VStr::ICmp(*AName, "Null") == 0) return nullptr;

  if (SubLabel == NAME_None) {
    // remap old death state labels to proper names
         if (VStr::ICmp(*AName, "XDeath") == 0) { AName = VName("Death"); SubLabel = VName("Extreme"); }
    else if (VStr::ICmp(*AName, "Burn") == 0) { AName = VName("Death"); SubLabel = VName("Fire"); }
    else if (VStr::ICmp(*AName, "Ice") == 0) { AName = VName("Death"); SubLabel = VName("Ice"); }
    else if (VStr::ICmp(*AName, "Disintegrate") == 0) { AName = VName("Death"); SubLabel = VName("Disintegrate"); }
  }

  const char *namestr = *AName;
  if (strchr(namestr, '.') != nullptr || (SubLabel != NAME_None && strchr(*SubLabel, '.') != nullptr)) {
    // oops, has dots; do it slow
    VStr lblstr = VStr(namestr);
    if (SubLabel != NAME_None) { lblstr += '.'; lblstr += *SubLabel; }
    TArray<VName> names;
    if (names.length() == 0) return nullptr;
    StaticSplitStateLabel(lblstr, names);
    return FindStateLabel(names, Exact);
  }

  for (int i = 0; i < StateLabels.Num(); ++i) {
    //fprintf(stderr, "%s:<%s>: i=%d; lname=%s\n", GetName(), *AName, i, *StateLabels[i].Name);
    if (VStr::ICmp(*StateLabels[i].Name, *AName) == 0) {
      if (SubLabel != NAME_None) {
        TArray<VStateLabel> &SubList = StateLabels[i].SubLabels;
        for (int j = 0; j < SubList.Num(); ++j) {
          if (VStr::ICmp(*SubList[j].Name, *SubLabel) == 0) return &SubList[j];
        }
        if (Exact /*&& VStr::ICmp(*SubLabel, "None") != 0*/) return nullptr; //k8:HACK! 'None' is nothing
      }
      //fprintf(stderr, "FOUND: %s:<%s>: i=%d; lname=%s (%s)\n", GetName(), *AName, i, *StateLabels[i].Name, *StateLabels[i].State->Loc.toStringNoCol());
      return &StateLabels[i];
    }
  }

  //if (AName == VName("Missile")) fprintf(stderr, "ERROR: '%s' state for '%s' not found! (parentclass=%s)\n", *AName, *GetFullName(), (ParentClass ? *ParentClass->GetFullName() : "<none>"));
  return nullptr;
}


//==========================================================================
//
//  VClass::FindStateLabel
//
//==========================================================================
VStateLabel *VClass::FindStateLabel (TArray<VName> &Names, bool Exact) {
  if (Names.length() > 0 && (VStr::ICmp(*Names[0], "None") == 0 || VStr::ICmp(*Names[0], "Null") == 0)) return nullptr;
  TArray<VStateLabel> *List = &StateLabels;
  VStateLabel *Best = nullptr;
  for (int ni = 0; ni < Names.Num(); ++ni) {
    if (Names[ni] == NAME_None) continue;
    VStateLabel *Lbl = nullptr;
    for (int i = 0; i < List->Num(); ++i) {
      if (VStr::ICmp(*(*List)[i].Name, *Names[ni]) == 0) {
        Lbl = &(*List)[i];
        break;
      }
    }
    if (!Lbl) {
      if (Exact) return nullptr;
      break;
    } else {
      Best = Lbl;
      List = &Lbl->SubLabels;
    }
  }
  return Best;
}


//==========================================================================
//
//  VClass::FindStateLabelChecked
//
//==========================================================================
/*
VStateLabel *VClass::FindStateLabelChecked (VName AName, VName SubLabel, bool Exact) {
  if (AName == NAME_None || VStr::ICmp(*AName, "None") == 0 || VStr::ICmp(*AName, "Null") == 0) return nullptr;
  VStateLabel *Lbl = FindStateLabel(AName, SubLabel, Exact);
  if (!Lbl) {
    if (Names.length() > 0 && (VStr::ICmp(*Names[0], "None") == 0 || VStr::ICmp(*Names[0], "Null") == 0)) return nullptr;
    VStr FullName = *AName;
    if (SubLabel != NAME_None) {
      FullName += ".";
      FullName += *SubLabel;
    }
    Sys_Error("State %s not found", *FullName);
  }
  return Lbl;
}
*/


//==========================================================================
//
//  VClass::FindStateLabelChecked
//
//==========================================================================
/*
VStateLabel *VClass::FindStateLabelChecked (TArray<VName> &Names, bool Exact) {
  VStateLabel *Lbl = FindStateLabel(Names, Exact);
  if (!Lbl) {
    VStr FullName = *Names[0];
    for (int i = 1; i < Names.Num(); ++i) {
      FullName += ".";
      FullName += *Names[i];
    }
    Sys_Error("State %s not found", *FullName);
  }
  return Lbl;
}
*/


//==========================================================================
//
//  VClass::FindDecorateStateAction
//
//==========================================================================
VDecorateStateAction *VClass::FindDecorateStateAction (VName ActName) {
  for (int i = 0; i < DecorateStateActions.Num(); ++i) {
    if (DecorateStateActions[i].Name == ActName) return &DecorateStateActions[i];
  }
  if (ParentClass) return ParentClass->FindDecorateStateAction(ActName);
  return nullptr;
}


//==========================================================================
//
//  VClass::FindDecorateStateFieldTrans
//
//==========================================================================
VName VClass::FindDecorateStateFieldTrans (VName dcname) {
  auto vp = DecorateStateFieldTrans.find(dcname);
  if (vp) return *vp;
  if (ParentClass) return ParentClass->FindDecorateStateFieldTrans(dcname);
  return NAME_None;
}


//==========================================================================
//
//  VClass::isNonVirtualMethod
//
//==========================================================================
bool VClass::isNonVirtualMethod (VName Name) {
  VMethod *M = FindMethod(Name, false); // don't do recursive search
  if (!M) return false;
  if ((M->Flags&FUNC_Final) == 0) return false; // no way
  if ((M->Flags&FUNC_NonVirtual) != 0) return true; // just in case
  // check if parent class has method with the same name
  if (!ParentClass) return true; // no parent class (why?) -- real final
  return (ParentClass->GetMethodIndex(M->Name) == -1);
}


//==========================================================================
//
//  VClass::FindBestLatestChild
//
//  check inheritance chains, find a child with the longest chain
//  use `ParentClassName`, because some classes may not be defined yet
//
//==========================================================================
VClass *VClass::FindBestLatestChild (VName ignoreThis) {
  int bestChainLen = -1;
  VClass *bestClass = nullptr;

  VMemberBase **mlist = GMembers.ptr();
  for (int count = GMembers.length(); count--; ++mlist) {
    VMemberBase *m = *mlist;
    if (m && m->MemberType == MEMBER_Class) {
      VClass *c = (VClass *)m;
      if (c == this) continue;
      // check inheritance chain
      int chainLen = 0;
      while (c) {
        if (c->Name == ignoreThis) { c = nullptr; break; } // bad chain
        if (c->Name == Name) break;
        if (c->ParentClassName == NAME_None) { c = nullptr; break; } // wtf?!
        ++chainLen;
        c = StaticFindClass(c->ParentClassName);
      }
      if (c) {
        // found child
        if (bestChainLen < chainLen) {
          bestChainLen = chainLen;
          bestClass = (VClass *)m;
        }
      }
    }
  }

  return (bestClass ? bestClass : this);
}


//==========================================================================
//
//  VClass::Define
//
//==========================================================================
bool VClass::Define () {
  // check for duplicates
  /*
  int HashIndex = Name.GetIndex()&4095;
  for (VMemberBase *m = GMembersHash[HashIndex]; m; m = m->HashNext) {
    if (m->Name == Name && m->MemberType == MEMBER_Class && ((VClass *)m)->Defined) {
      if (((VClass *)m)->DefinedAsDependency) return true;
      ParseError(Loc, "Class `%s` already has been declared", *Name);
    }
  }
  */
  if (Name != NAME_None) {
    VClass *cc = StaticFindClass(Name);
    if (cc) {
      if (cc->Defined) {
        if (cc->DefinedAsDependency) return true;
        ParseError(Loc, "Class `%s` already has been declared", *Name);
        return false;
      }
    }
  }

  // mark it as defined
  Defined = true;
  DefinedAsDependency = false;

#if !defined(IN_VCC)
  VClass *PrevParent = ParentClass;
#endif
  if (ParentClassName != NAME_None) {
    ParentClass = StaticFindClass(ParentClassName);
    if (!ParentClass) {
      ParseError(ParentClassLoc, "No such class `%s`", *ParentClassName);
      return false;
    } else if (!ParentClass->Defined) {
      //ParseError(ParentClassLoc, "Parent class must be defined before");
      // recurse, 'cause why not?
      bool xdres = ParentClass->Define();
      ParentClass->DefinedAsDependency = true;
      if (!xdres) return false;
    }
#if defined(VCC_STANDALONE_EXECUTOR)
    // process replacements
    // first get actual replacement
    ParentClass = ParentClass->GetReplacement();
    if (!ParentClass) FatalError("VC Internal Error: VClass::Define: cannot find replacement");
    if (DoesReplacement == ReplaceType::Replace_LatestChild) ParentClass = ParentClass->FindBestLatestChild(Name);
    if (!ParentClass->Defined) {
      bool xdres = ParentClass->Define();
      ParentClass->DefinedAsDependency = true;
      if (!xdres) return false;
    }
    //fprintf(stderr, "VClass::Define: requested parent is `%s`, actual parent is `%s`\n", *ParentClassName, ParentClass->GetName());
    // now set replacemente for the actual replacement (if necessary)
    if (DoesReplacement) {
      /*
      //fprintf(stderr, "VClass::Define: class `%s` tries to replace class `%s` (actual is `%s`)...\n", GetName(), *ParentClassName, ParentClass->GetName());
      VClass *pc = ParentClass->GetReplacement();
      if (!pc) {
        ParseError(ParentClassLoc, "Cannot replace class `%s` (oops)", *ParentClassName);
      } else if (!pc->IsChildOf(ParentClass)) {
        ParseError(ParentClassLoc, "Cannot replace class `%s` (%s)", *ParentClassName, pc->GetName());
      } else {
        //if (!ParentClass->SetReplacement(this)) ParseError(ParentClassLoc, "Cannot replace class `%s`", *ParentClassName);
        fprintf(stderr, "**** %s : %s\n", ParentClass->GetName(), pc->GetName());
        if (!pc->SetReplacement(this)) ParseError(ParentClassLoc, "Cannot replace class `%s`", *ParentClassName);
      }
      */
      if (!ParentClass->SetReplacement(this)) ParseError(ParentClassLoc, "Cannot replace class `%s`", *ParentClassName);
    }
#else
    if (DoesReplacement) {
      if (DoesReplacement == ReplaceType::Replace_LatestChild) {
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
        if (developer) GLog.Logf(NAME_Dev, "VClass::Define: class `%s` tries to replace latest child of class `%s` (actual is `%s`)", GetName(), *ParentClassName, ParentClass->GetName());
#endif
        ParentClass = ParentClass->FindBestLatestChild(Name);
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
        if (developer) GLog.Logf(NAME_Dev, "VClass::Define:   latest child is `%s`", ParentClass->GetName());
#endif
      } else {
        ParentClass = ParentClass->GetReplacement();
      }
      if (!ParentClass) FatalError("VC Internal Error: VClass::Define: cannot find replacement");
      //fprintf(stderr, "VClass::Define: requested parent is `%s`, actual parent is `%s`\n", *ParentClassName, ParentClass->GetName());
      if (!ParentClass->Defined) {
        bool xdres = ParentClass->Define();
        ParentClass->DefinedAsDependency = true;
        if (!xdres) return false;
      }
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
      if (developer) GLog.Logf(NAME_Dev, "VClass::Define: class `%s` tries to replace class `%s` (actual is `%s`)...", GetName(), *ParentClassName, ParentClass->GetName());
#endif
      if (!ParentClass->SetReplacement(this)) {
        ParseError(ParentClassLoc, "Cannot replace class `%s`", *ParentClassName);
      }
    }
#endif
  }
#if !defined(IN_VCC)
  if ((ObjectFlags&CLASSOF_Native) && ParentClass != PrevParent) {
    Sys_Error("Bad parent class, class %s, C++ %s, VavoomC %s)",
      GetName(), PrevParent ? PrevParent->GetName() : "(none)",
      ParentClass ? ParentClass->GetName() : "(none)");
  }
#endif

  // process constants, so if other class will try to use constant it its declaration, that will succeed
  for (int f = 0; f < Constants.length(); ++f) if (!Constants[f]->Define()) return false;

  for (int i = 0; i < Structs.Num(); ++i) if (!Structs[i]->Define()) return false;

  // this can be not postloaded yet, so...
  {
    VName pn = ParentClassName;
    while (pn != NAME_None) {
      VClass *c = StaticFindClass(pn);
      if (!c) break;
      // check fields
      for (VField *F = Fields; F; F = F->Next) {
        if (c->FindField(F->Name)) {
          ParseError(F->Loc, "Field `%s` already defined in parent class `%s`", *F->Name, *pn);
        }
      }
      // check constants
      for (int f = 0; f < Constants.length(); ++f) {
        for (int ff = 0; ff < c->Constants.length(); ++ff) {
          if (c->Constants[ff]->Name == Constants[f]->Name) {
            ParseError(Constants[f]->Loc, "Constant `%s` already defined in parent class `%s`", *Constants[f]->Name, *pn);
          }
        }
      }
      pn = c->ParentClassName;
    }
  }

  return true;
}


//==========================================================================
//
//  VClass::DefineRepInfos
//
//==========================================================================
bool VClass::DefineRepInfos () {
  bool Ret = true;

  for (int ri = 0; ri < RepInfos.Num(); ++ri) {
    if (!RepInfos[ri].Cond->Define()) Ret = false;
    TArray<VRepField> &RepFields = RepInfos[ri].RepFields;
    for (int i = 0; i < RepFields.Num(); ++i) {
      VField *RepField = nullptr;
      for (VField *F = Fields; F; F = F->Next) {
        if (F->Name == RepFields[i].Name) {
          RepField = F;
          break;
        }
      }
      if (RepField) {
        if (RepField->Flags&FIELD_Net) {
          ParseError(RepFields[i].Loc, "Field %s has multiple replication statements", *RepFields[i].Name);
          continue;
        }
        RepField->Flags |= FIELD_Net;
        RepField->ReplCond = RepInfos[ri].Cond;
        RepFields[i].Member = RepField;
        continue;
      }

      VMethod *RepMethod = nullptr;
      for (int mi = 0; mi < Methods.Num(); ++mi) {
        if (Methods[mi]->Name == RepFields[i].Name) {
          RepMethod = Methods[mi];
          break;
        }
      }
      if (RepMethod) {
        if (RepMethod->SuperMethod) {
          ParseError(RepFields[i].Loc, "Method %s is overloaded in this class", *RepFields[i].Name);
          continue;
        }
        if (RepMethod->Flags&FUNC_Net) {
          ParseError(RepFields[i].Loc, "Method %s has multiple replication statements", *RepFields[i].Name);
          continue;
        }
        RepMethod->Flags |= FUNC_Net;
        RepMethod->ReplCond = RepInfos[ri].Cond;
        if (RepInfos[ri].Reliable) RepMethod->Flags |= FUNC_NetReliable;
        RepFields[i].Member = RepMethod;
        continue;
      }

      ParseError(RepFields[i].Loc, "No such field or method %s", *RepFields[i].Name);
    }
  }
  return Ret;
}


//==========================================================================
//
//  VClass::DefineMembers
//
//==========================================================================
bool VClass::DefineMembers () {
  bool Ret = true;

  // moved to `Define()`
  //for (int i = 0; i < Constants.Num(); ++i) if (!Constants[i]->Define()) Ret = false;
  for (int i = 0; i < Structs.Num(); ++i) Structs[i]->DefineMembers();

  VField *PrevBool = nullptr;
  for (VField *fi = Fields; fi; fi = fi->Next) {
    if (!fi->Define()) Ret = false;
    if (fi->Type.Type == TYPE_Bool && PrevBool && PrevBool->Type.BitMask != 0x80000000) {
      fi->Type.BitMask = PrevBool->Type.BitMask<<1;
    }
    PrevBool = (fi->Type.Type == TYPE_Bool ? fi : nullptr);
  }

  for (int i = 0; i < Properties.Num(); ++i) if (!Properties[i]->Define()) Ret = false;
  for (int i = 0; i < Methods.Num(); ++i) if (!Methods[i]->Define()) Ret = false;

  if (!DefaultProperties->Define()) Ret = false;

  for (VState *s = States; s; s = s->Next) if (!s->Define()) Ret = false;

  if (!DefineRepInfos()) Ret = false;

  return Ret;
}


//==========================================================================
//
//  VClass::DecorateDefine
//
//==========================================================================
bool VClass::DecorateDefine () {
  bool Ret = true;
  VField *PrevBool = nullptr;
  for (VField *fi = Fields; fi; fi = fi->Next) {
    if (!fi->Define()) Ret = false;
    if (fi->Type.Type == TYPE_Bool && PrevBool && PrevBool->Type.BitMask != 0x80000000) {
      fi->Type.BitMask = PrevBool->Type.BitMask<<1;
    }
    PrevBool = (fi->Type.Type == TYPE_Bool ? fi : nullptr);
  }
  return Ret;
}


//==========================================================================
//
//  VClass::StaticDumpMObjInfo
//
//==========================================================================
void VClass::StaticDumpMObjInfo () {
#if !defined(IN_VCC)
  TArray<XXMInfo> list;
  for (int f = 0; f < GMobjInfos.length(); ++f) {
    XXMInfo &xn = list.alloc();
    xn.idx = f;
    xn.nfo = GMobjInfos[f];
  }
  timsort_r(list.ptr(), list.length(), sizeof(XXMInfo), &cmpobjnfo, nullptr);
  GLog.Log("=== DOOMED ===");
  for (int f = 0; f < list.length(); ++f) {
    mobjinfo_t *nfo = &list[f].nfo;
    GLog.Logf("  %5d: '%s'; flags:0x%02x; filter:0x%04x", nfo->DoomEdNum, (nfo->Class ? *nfo->Class->GetFullName() : "<none>"), nfo->flags, nfo->GameFilter);
  }
  GLog.Log(" ------");
  for (auto it = GMobj2Arr.first(); it; ++it) {
    GLog.Logf("  ..[DOOMED:%d]..", it.getKey());
    int link = it.getValue();
    while (link != -1) {
      mobjinfo_t *nfo = &GMobjInfos[link];
      GLog.Logf("    #%5d: %5d: '%s'; flags:0x%02x; filter:0x%04x", link, nfo->DoomEdNum, (nfo->Class ? *nfo->Class->GetFullName() : "<none>"), nfo->flags, nfo->GameFilter);
      link = nfo->nextidx;
    }
  }
#endif
}


//==========================================================================
//
//  VClass::StaticDumpScriptIds
//
//==========================================================================
void VClass::StaticDumpScriptIds () {
#if !defined(IN_VCC)
  TArray<XXMInfo> list;
  for (int f = 0; f < GScriptIds.length(); ++f) {
    XXMInfo &xn = list.alloc();
    xn.idx = f;
    xn.nfo = GScriptIds[f];
  }
  timsort_r(list.ptr(), list.length(), sizeof(XXMInfo), &cmpobjnfo, nullptr);
  GLog.Logf("=== SCRIPTID ===");
  for (int f = 0; f < list.length(); ++f) {
    mobjinfo_t *nfo = &list[f].nfo;
    GLog.Logf("  %5d: '%s'; flags:0x%02x; filter:0x%04x", nfo->DoomEdNum, (nfo->Class ? *nfo->Class->GetFullName() : "<none>"), nfo->flags, nfo->GameFilter);
  }
#endif
}


//==========================================================================
//
//  RehashList
//
//==========================================================================
static void RehashList (TArray<mobjinfo_t> &list, TMapNC<vint32, vint32> &map) {
  map.reset();
  for (int f = 0; f < list.length(); ++f) list[f].nextidx = -1;
  for (int idx = 0; idx < list.length(); ++idx) {
    // link to map
    mobjinfo_t *mi = &list[idx];
    int id = mi->DoomEdNum;
    auto prevp = map.find(id);
    if (prevp) {
      mi->nextidx = *prevp;
      *prevp = idx;
      //map.del(id);
      //map.put(id, idx);
    } else {
      map.put(id, idx);
    }
  }
}


//==========================================================================
//
//  AllocateIdFromList
//
//==========================================================================
static mobjinfo_t *AllocateIdFromList (TArray<mobjinfo_t> &list, TMapNC<vint32, vint32> &map, vint32 id, int GameFilter, VClass *cls, const char *msg) {
  if (id <= 0) return nullptr;
#if 0
  mobjinfo_t *mi = nullptr;
  for (int midx = list.length()-1; midx >= 0; --midx) {
    mobjinfo_t *nfo = &list[midx];
    if (nfo->DoomEdNum == id && nfo->GameFilter == GameFilter) {
      mi = nfo;
      break;
    }
  }
  if (mi == nullptr) {
    mi = &list.alloc();
    //fprintf(stderr, "%s: new doomed number #%d, class <%s> (filter=0x%04x)\n", msg, id, *cls->GetFullName(), GameFilter);
  } else {
    //fprintf(stderr, "%s: REPLACED doomed number #%d, class <%s> (old class <%s>) (filter=0x%04x)\n", msg, id, *cls->GetFullName(), (mi->Class ? *mi->Class->GetFullName() : "none"), GameFilter);
  }
  memset(mi, 0, sizeof(*mi));
  mi->DoomEdNum = id;
  mi->GameFilter = GameFilter;
  mi->Class = cls;
#else
  vint32 idx = list.length();
  mobjinfo_t *mi = &list.alloc();
  memset(mi, 0, sizeof(*mi));
  mi->DoomEdNum = id;
  mi->GameFilter = GameFilter;
  mi->Class = cls;
  mi->nextidx = -1;
  // link to map
  auto prevp = map.find(id);
  if (prevp) {
    mi->nextidx = *prevp;
    *prevp = idx;
  } else {
    map.put(id, idx);
  }
  //RehashList(list, map);
#endif
  return mi;
}


//==========================================================================
//
//  FindIdInList
//
//==========================================================================
static mobjinfo_t *FindIdInList (TArray<mobjinfo_t> &list, TMapNC<vint32, vint32> &map, vint32 id, int GameFilter) {
  if (id <= 0) return nullptr;
#if 0
  for (int midx = list.length()-1; midx >= 0; --midx) {
    mobjinfo_t *nfo = &list[midx];
    if (nfo->DoomEdNum == id && (nfo->GameFilter == 0 || (nfo->GameFilter&GameFilter) != 0)) {
      return nfo;
    }
  }
#else
  auto linkp = map.find(id);
  if (linkp) {
    int link = *linkp;
    while (link != -1) {
      mobjinfo_t *nfo = &list[link];
      if (nfo->DoomEdNum == id && (nfo->GameFilter == 0 || (nfo->GameFilter&GameFilter) != 0)) {
        return nfo;
      }
      link = nfo->nextidx;
    }
  }
#endif
  return nullptr;
}


//==========================================================================
//
//  RemoveIdFromList
//
//==========================================================================
static void RemoveIdFromList (TArray<mobjinfo_t> &list, TMapNC<vint32, vint32> &map, vint32 id, int GameFilter) {
  if (id <= 0) return;
  bool removed = false;
  int midx = 0;
  while (midx < list.length()) {
    mobjinfo_t *nfo = &list[midx];
    if (nfo->DoomEdNum == id && (nfo->GameFilter == 0 || (nfo->GameFilter&GameFilter) != 0)) {
      list.removeAt(midx);
      removed = true;
    } else {
      ++midx;
    }
  }
  if (removed) RehashList(list, map);
}


//==========================================================================
//
//  VClass::AllocMObjId
//
//==========================================================================
mobjinfo_t *VClass::AllocMObjId (vint32 id, int GameFilter, VClass *cls) {
  return AllocateIdFromList(GMobjInfos, GMobj2Arr, id, GameFilter, cls, "DOOMED");
}


//==========================================================================
//
//  VClass::AllocScriptId
//
//==========================================================================
mobjinfo_t *VClass::AllocScriptId (vint32 id, int GameFilter, VClass *cls) {
  return AllocateIdFromList(GScriptIds, GSId2Arr, id, GameFilter, cls, "SCRIPTID");
}


//==========================================================================
//
//  VClass::FindMObjId
//
//==========================================================================
mobjinfo_t *VClass::FindMObjId (vint32 id, int GameFilter) {
  return FindIdInList(GMobjInfos, GMobj2Arr, id, GameFilter);
}


//==========================================================================
//
//  VClass::FindScriptId
//
//==========================================================================
mobjinfo_t *VClass::FindScriptId (vint32 id, int GameFilter) {
  return FindIdInList(GScriptIds, GSId2Arr, id, GameFilter);
}


//==========================================================================
//
//  VClass::FindMObjIdByClass
//
//==========================================================================
mobjinfo_t *VClass::FindMObjIdByClass (const VClass *cls, int GameFilter) {
  if (!cls) return nullptr;
  for (int midx = GMobjInfos.length()-1; midx >= 0; --midx) {
    mobjinfo_t *nfo = &GMobjInfos[midx];
    if (nfo->Class == cls && (nfo->GameFilter == 0 || (nfo->GameFilter&GameFilter) != 0)) {
      return &GMobjInfos[midx];
    }
  }
  return nullptr;
}


//==========================================================================
//
//  VClass::RemoveMObjId
//
//==========================================================================
void VClass::RemoveMObjId (vint32 id, int GameFilter) {
  RemoveIdFromList(GMobjInfos, GMobj2Arr, id, GameFilter);
}


//==========================================================================
//
//  VClass::RemoveScriptId
//
//==========================================================================
void VClass::RemoveScriptId (vint32 id, int GameFilter) {
  RemoveIdFromList(GScriptIds, GSId2Arr, id, GameFilter);
}


//==========================================================================
//
//  VClass::RemoveMObjIdByClass
//
//==========================================================================
void VClass::RemoveMObjIdByClass (VClass *cls, int GameFilter) {
  if (!cls) return;
  bool removed = false;
  int midx = 0;
  while (midx < GMobjInfos.length()) {
    mobjinfo_t *nfo = &GMobjInfos[midx];
    if (nfo->Class == cls && (nfo->GameFilter == 0 || (nfo->GameFilter&GameFilter) != 0)) {
      GMobjInfos.removeAt(midx);
      removed = true;
    } else {
      ++midx;
    }
  }
  if (removed) RehashList(GMobjInfos, GMobj2Arr);
}


//==========================================================================
//
//  VClass::Emit
//
//==========================================================================
void VClass::Emit () {
  int GameFilter = 0;
  if (GameExpr) {
    VEmitContext ec(this);
    GameExpr = GameExpr->Resolve(ec);
    if (GameExpr) {
      if (!GameExpr->IsIntConst()) {
        ParseError(GameExpr->Loc, "Integer constant expected");
      } else {
        GameFilter = GameExpr->GetIntConst();
      }
    }
  }

  if (MobjInfoExpr) {
    VEmitContext ec(this);
    MobjInfoExpr = MobjInfoExpr->Resolve(ec);
    if (MobjInfoExpr) {
      if (!MobjInfoExpr->IsIntConst()) {
        ParseError(MobjInfoExpr->Loc, "Integer constant expected");
      } else {
        int id = MobjInfoExpr->GetIntConst();
        if (id != 0) {
          /*mobjinfo_t *mi =*/ AllocMObjId(id, GameFilter, this);
          //if (mi) mi->Class = this;
        }
        /*
        mobjinfo_t &mi = ec.Package->MobjInfo.Alloc();
        mi.DoomEdNum = MobjInfoExpr->GetIntConst();
        mi.GameFilter = GameFilter;
        mi.Class = this;
        mi.flags = 0;
        */
      }
    }
  }

  if (ScriptIdExpr) {
    VEmitContext ec(this);
    ScriptIdExpr = ScriptIdExpr->Resolve(ec);
    if (ScriptIdExpr) {
      if (!ScriptIdExpr->IsIntConst()) {
        ParseError(ScriptIdExpr->Loc, "Integer constant expected");
      } else {
        int id = ScriptIdExpr->GetIntConst();
        if (id != 0) {
          /*mobjinfo_t *mi =*/ AllocScriptId(id, GameFilter, this);
          //if (mi) mi->Class = this;
        }
        /*
        mobjinfo_t &mi = ec.Package->ScriptIds.Alloc();
        mi.DoomEdNum = ScriptIdExpr->GetIntConst();
        mi.GameFilter = GameFilter;
        mi.Class = this;
        mi.flags = 0;
        */
      }
    }
  }

  // emit method code
  for (int i = 0; i < Methods.Num(); ++i) Methods[i]->Emit();

  // build list of state labels, resolve jumps
  EmitStateLabels();

  // emit code of the state methods
  for (VState *s = States; s; s = s->Next) s->Emit();

  // emit code of the network replication conditions
  for (int ri = 0; ri < RepInfos.Num(); ++ri) RepInfos[ri].Cond->Emit();

  DefaultProperties->Emit();
}


//==========================================================================
//
//  VClass::DecorateEmit
//
//==========================================================================
void VClass::DecorateEmit () {
  // emit method code
  for (int i = 0; i < Methods.Num(); ++i) Methods[i]->Emit();
}


//==========================================================================
//
//  VClass::EmitStateLabels
//
//==========================================================================
void VClass::EmitStateLabels () {
  if (ParentClass && (ClassFlags&CLASS_SkipSuperStateLabels) == 0) {
    StateLabels = ParentClass->StateLabels;
  }

  // first add all labels
  for (int i = 0; i < StateLabelDefs.Num(); ++i) {
    VStateLabelDef &Lbl = StateLabelDefs[i];
    //if (VStr::ICmp(GetName(), "SmoothZombieman") == 0) fprintf(stderr, "SMZ: label '%s' at '%s'\n", *Lbl.Name, (Lbl.State ? *Lbl.State->GetFullName() : "???"));
    TArray<VName> Names;
    //if (Lbl.Name[0] == '_') fprintf(stderr, "+++  <%s> -> <%s>\n", *Lbl.Name, Lbl.State->GetName());
    StaticSplitStateLabel(Lbl.Name, Names);
    SetStateLabel(Names, Lbl.State);
  }

  // then resolve state labels that do immediate jumps
  for (int i = 0; i < StateLabelDefs.Num(); ++i) {
    VStateLabelDef &Lbl = StateLabelDefs[i];
    if (Lbl.GotoLabel != NAME_None) {
      //fprintf(stderr, "XXXLABEL:%s: '%s': dest label is '%s', dest offset is %d\n", GetName(), *StateLabelDefs[i].Name, *StateLabelDefs[i].GotoLabel, StateLabelDefs[i].GotoOffset);
      Lbl.State = ResolveStateLabel(Lbl.Loc, Lbl.GotoLabel, Lbl.GotoOffset);
      TArray<VName> Names;
      StaticSplitStateLabel(Lbl.Name, Names);
      SetStateLabel(Names, Lbl.State);
    }
  }
}


//==========================================================================
//
//  VClass::ResolveStateLabel
//
//==========================================================================
VState *VClass::ResolveStateLabel (const TLocation &Loc, VName LabelName, int Offset) {
  VClass *CheckClass = this;
  VStr CheckName = *LabelName;

  int DCol = CheckName.IndexOf("::");
  if (DCol >= 0) {
    VStr ClassNameStr(CheckName, 0, DCol);
    if (ClassNameStr.ICmp("Super") == 0) {
      CheckClass = ParentClass;
    } else {
      VName ClassName(*ClassNameStr);
      CheckClass = StaticFindClassNoCase(ClassName);
      if (!CheckClass) {
        ParseError(Loc, "No such superclass '%s' for class '%s'", *ClassNameStr, *GetFullName());
        return nullptr;
      }
    }
    CheckName = VStr(CheckName, DCol+2, CheckName.Length()-DCol-2);
  }

  if (VStr::ICmp(*CheckName, "Null") == 0) return nullptr;
  if (VStr::ICmp(*CheckName, "None") == 0) return nullptr;

  TArray<VName> Names;
  StaticSplitStateLabel(CheckName, Names);
  VStateLabel *Lbl = CheckClass->FindStateLabel(Names, true);
  if (!Lbl) {
    if (optDeprecatedLaxStates) {
      ParseWarning(Loc, "No such state '%s' in class '%s'", *LabelName, *GetFullName());
    } else {
      ParseError(Loc, "No such state '%s' in class '%s'", *LabelName, *GetFullName());
    }
    return nullptr;
  }

  VState *State = Lbl->State;
  int Count = Offset;
  while (Count--) {
    if (!State || !State->Next) {
      ParseError(Loc, "Bad jump offset (%d, but only %d is allowed); in `ResolveStateLabel` for label '%s' in class '%s'", Offset, Offset-(Count+1), *LabelName, *GetFullName());
      return nullptr;
    }
    if (State->Frame&VState::FF_SKIPOFFS) ++Count;
    State = State->Next;
  }
  return State;
}


//==========================================================================
//
//  VClass::SetStateLabel
//
//==========================================================================
void VClass::SetStateLabel (VName AName, VState *State) {
  for (int i = 0; i < StateLabels.Num(); ++i) {
    if (VStr::ICmp(*StateLabels[i].Name, *AName) == 0) {
      StateLabels[i].State = State;
      return;
    }
  }
  VStateLabel &L = StateLabels.Alloc();
  L.Name = AName;
  L.State = State;
}


//==========================================================================
//
//  VClass::SetStateLabel
//
//==========================================================================
void VClass::SetStateLabel (const TArray<VName> &Names, VState *State) {
  if (!Names.Num()) return;
  TArray<VStateLabel> *List = &StateLabels;
  VStateLabel *Lbl = nullptr;
  for (int ni = 0; ni < Names.Num(); ++ni) {
    Lbl = nullptr;
    for (int i = 0; i < List->Num(); ++i) {
      if (VStr::ICmp(*(*List)[i].Name, *Names[ni]) == 0) {
        Lbl = &(*List)[i];
        break;
      }
    }
    if (Lbl == nullptr) {
      Lbl = &List->Alloc();
      Lbl->Name = Names[ni];
    }
    List = &Lbl->SubLabels;
  }
  if (Lbl != nullptr) {
    //if ((*Names[0])[0] == '_') fprintf(stderr, "*** <%s> is <%s>\n", *Names[0], State->GetName());
    Lbl->State = State;
  }
}


//==========================================================================
//
//  VClass::PostLoad
//
//==========================================================================
void VClass::PostLoad () {
  if (ObjectFlags&CLASSOF_PostLoaded) return; // already set up

  // make sure parent class has been set up
  if (GetSuperClass()) GetSuperClass()->PostLoad();

  NetStates = States;

  // calculate field offsets and class size
  CalcFieldOffsets();

  // initialise reference fields
  InitReferences();

  // initialise destructor fields
  InitDestructorFields();

  // initialise net fields
  InitNetFields();

  // create virtual table
  CreateVTable();

  // set up states lookup table
  InitStatesLookup();

  // set state in-class indexes
  int CurrIndex = 0;
  //VState *prevS = nullptr;
  for (VState *S = States; S; /*prevS = S,*/ S = S->Next) {
    //if (!prevS || (prevS->Frame&(VState::FF_SKIPMODEL|VState::FF_SKIPOFFS)) == 0) ++CurrIndex;
    //fprintf(stderr, "state <%s>: sprite name is '%s', frame is %04x\n", *S->GetFullName(), *S->SpriteName, S->Frame);
    S->InClassIndex = CurrIndex;
    if ((S->Frame&(VState::FF_SKIPMODEL|VState::FF_SKIPOFFS)) == 0) ++CurrIndex;
  }

  ObjectFlags |= CLASSOF_PostLoaded;
}


//==========================================================================
//
//  VClass::DecoratePostLoad
//
//==========================================================================
void VClass::DecoratePostLoad () {
  // compile
  for (int i = 0; i < Methods.Num(); ++i) Methods[i]->PostLoad();
  for (VState *S = States; S; S = S->Next) S->PostLoad();
  NetStates = States;

  // set state in-class indexes
  int CurrIndex = 0;
  for (VState *S = States; S; S = S->Next) {
    S->InClassIndex = CurrIndex;
    if ((S->Frame&(VState::FF_SKIPMODEL|VState::FF_SKIPOFFS)) == 0) ++CurrIndex;
  }

  // calculate indexes of virtual methods
  CalcFieldOffsets();

  /*
  // initialise reference fields
  InitReferences();

  // initialise destructor fields
  InitDestructorFields();
  */

  // recreate virtual table
  CreateVTable();
}


//==========================================================================
//
//  VClass::CalcFieldOffsets
//
//==========================================================================
void VClass::CalcFieldOffsets () {
  // skip this for C++ only classes
  if (!Outer && (ObjectFlags&CLASSOF_Native) != 0) {
    ClassNumMethods = (ParentClass ? ParentClass->ClassNumMethods : 0);
    return;
  }

  int PrevClassNumMethods = ClassNumMethods;
  int numMethods = (ParentClass ? ParentClass->ClassNumMethods : 0);
  for (int i = 0; i < Methods.Num(); ++i) {
    VMethod *M = (VMethod *)Methods[i];
    int MOfs = -1;
    if (ParentClass) MOfs = ParentClass->GetMethodIndex(M->Name);
    if (MOfs == -1 && (M->Flags&FUNC_Final) != 0) M->Flags |= FUNC_NonVirtual;
    if (MOfs == -1 && (M->Flags&FUNC_Final) == 0) MOfs = numMethods++;
    M->VTableIndex = MOfs;
  }
  if (ClassVTable && PrevClassNumMethods != ClassNumMethods) {
    delete[] ClassVTable;
    ClassVTable = nullptr;
  }

  VField *PrevField = nullptr;
  int PrevSize = ClassSize;
  int size = 0;
  if (ParentClass) {
    // GCC has a strange behavior of starting to add fields in subclasses
    // in a class that has virtual methods on unaligned parent size offset.
    // In other cases and in other compilers it starts on aligned parent
    // class size offset.
    if (sizeof(DummyClass1) == sizeof(DummyClass2)) {
      size = ParentClass->ClassUnalignedSize;
    } else {
      size = ParentClass->ClassSize;
    }
  }
  for (VField *fi = Fields; fi; fi = fi->Next) {
    if (fi->Type.Type == TYPE_Bool && PrevField &&
        PrevField->Type.Type == TYPE_Bool &&
        PrevField->Type.BitMask != 0x80000000)
    {
      vuint32 bit_mask = PrevField->Type.BitMask<<1;
      if (fi->Type.BitMask != bit_mask) Sys_Error("Wrong bit mask");
      fi->Type.BitMask = bit_mask;
      fi->Ofs = PrevField->Ofs;
    } else {
      if (fi->Type.Type == TYPE_Struct ||
          (fi->Type.IsAnyIndexableArray() && fi->Type.ArrayInnerType == TYPE_Struct) ||
          (fi->Type.Type == TYPE_Dictionary && (fi->Type.GetDictKeyType().Type == TYPE_Struct || fi->Type.GetDictValueType().Type == TYPE_Struct)))
      {
        // make sure struct size has been calculated
        fi->Type.Struct->PostLoad();
      }
      int FldAlign = fi->Type.GetAlignment();
      //fprintf(stderr, " fldalign for '%s' is %d (%s)\n", fi->GetName(), FldAlign, *fi->Type.GetName());
      size = (size+FldAlign-1)&~(FldAlign-1);
      fi->Ofs = size;
      size += fi->Type.GetSize();
    }
    PrevField = fi;
  }
  ClassUnalignedSize = size;
  size = (size+sizeof(void *)-1)&~(sizeof(void *)-1);
  ClassSize = size;
  ClassNumMethods = numMethods;
  if ((ObjectFlags&CLASSOF_Native) != 0 && ClassSize != PrevSize) {
    Sys_Error("Bad class size for class `%s`: C++: %d, VavoomC: %d", GetName(), PrevSize, ClassSize);
  }
}


//==========================================================================
//
//  VClass::InitNetFields
//
//==========================================================================
void VClass::InitNetFields () {
  if (ParentClass) {
    NetFields = ParentClass->NetFields;
    NetMethods = ParentClass->NetMethods;
    NumNetFields = ParentClass->NumNetFields;
  }

  for (VField *fi = Fields; fi; fi = fi->Next) {
    if ((fi->Flags&FIELD_Net) == 0) continue;
    fi->NetIndex = NumNetFields++;
    fi->NextNetField = NetFields;
    NetFields = fi;
  }

  for (int i = 0; i < Methods.Num(); ++i) {
    VMethod *M = Methods[i];
    if ((M->Flags&FUNC_Net) == 0) continue;
    VMethod *MPrev = nullptr;
    if (ParentClass) MPrev = ParentClass->FindMethod(M->Name);
    if (MPrev) {
      M->NetIndex = MPrev->NetIndex;
    } else {
      M->NetIndex = NumNetFields++;
    }
    M->NextNetMethod = NetMethods;
    NetMethods = M;
  }
}


//==========================================================================
//
//  VClass::InitReferences
//
//==========================================================================
void VClass::InitReferences () {
  ReferenceFields = nullptr;
  if (GetSuperClass()) ReferenceFields = GetSuperClass()->ReferenceFields;
  for (VField *F = Fields; F; F = F->Next) {
    switch (F->Type.Type) {
      case TYPE_Reference:
      case TYPE_Delegate:
        F->NextReference = ReferenceFields;
        ReferenceFields = F;
        break;
      case TYPE_Struct:
        F->Type.Struct->PostLoad();
        if (F->Type.Struct->ReferenceFields) {
          F->NextReference = ReferenceFields;
          ReferenceFields = F;
        }
        break;
      case TYPE_Array:
      case TYPE_DynamicArray:
        if (F->Type.ArrayInnerType == TYPE_Reference) {
          F->NextReference = ReferenceFields;
          ReferenceFields = F;
        } else if (F->Type.ArrayInnerType == TYPE_Struct) {
          F->Type.Struct->PostLoad();
          if (F->Type.Struct->ReferenceFields) {
            F->NextReference = ReferenceFields;
            ReferenceFields = F;
          }
        }
        break;
      case TYPE_Dictionary:
        if (F->Type.GetDictKeyType().Type == TYPE_Reference || F->Type.GetDictValueType().Type == TYPE_Reference) {
          F->NextReference = ReferenceFields;
          ReferenceFields = F;
        } else {
          if (F->Type.GetDictKeyType().Type == TYPE_Struct) {
            F->Type.KStruct->PostLoad();
            if (F->Type.KStruct->ReferenceFields) {
              F->NextReference = ReferenceFields;
              ReferenceFields = F;
              break;
            }
          }
          if (F->Type.GetDictValueType().Type == TYPE_Struct) {
            F->Type.Struct->PostLoad();
            if (F->Type.Struct->ReferenceFields) {
              F->NextReference = ReferenceFields;
              ReferenceFields = F;
              break;
            }
          }
        }
        break;
    }
  }
}


//==========================================================================
//
//  VClass::InitDestructorFields
//
//==========================================================================
void VClass::InitDestructorFields () {
  DestructorFields = nullptr;
  if (GetSuperClass()) DestructorFields = GetSuperClass()->DestructorFields;
  for (VField *F = Fields; F; F = F->Next) {
    switch (F->Type.Type) {
      case TYPE_String:
        F->DestructorLink = DestructorFields;
        DestructorFields = F;
        break;
      case TYPE_Struct:
        F->Type.Struct->PostLoad();
        if (F->Type.Struct->DestructorFields) {
          F->DestructorLink = DestructorFields;
          DestructorFields = F;
        }
        break;
      case TYPE_Array:
        if (F->Type.ArrayInnerType == TYPE_String) {
          F->DestructorLink = DestructorFields;
          DestructorFields = F;
        } else if (F->Type.ArrayInnerType == TYPE_Struct) {
          F->Type.Struct->PostLoad();
          if (F->Type.Struct->DestructorFields) {
            F->DestructorLink = DestructorFields;
            DestructorFields = F;
          }
        }
        break;
      case TYPE_DynamicArray:
        F->DestructorLink = DestructorFields;
        DestructorFields = F;
        break;
      case TYPE_Dictionary:
        F->DestructorLink = DestructorFields;
        DestructorFields = F;
        break;
    }
  }
}


//==========================================================================
//
//  VClass::CreateVTable
//
//==========================================================================
void VClass::CreateVTable () {
  if (!ClassVTable) ClassVTable = new VMethod*[ClassNumMethods];
  if (ParentClass) memcpy(ClassVTable, ParentClass->ClassVTable, ParentClass->ClassNumMethods*sizeof(VMethod *));
  for (int i = 0; i < Methods.Num(); ++i) {
    VMethod *M = Methods[i];
    if (M->VTableIndex == -1) continue;
    ClassVTable[M->VTableIndex] = M;
  }
  CreateMethodMap();
}


//==========================================================================
//
//  IsGoodAC
//
//==========================================================================
static bool IsGoodAC (VMethod *mt) {
  if (!mt) return false;
  if (mt->NumParams != 3) return false;
  if (mt->ReturnType.Type != TYPE_Void) return false;
  // first arg should be `const ref array!string`
  if (mt->ParamFlags[0] != (FPARM_Const|FPARM_Ref)) return false;
  VFieldType tp = mt->ParamTypes[0];
  if (tp.Type != TYPE_DynamicArray) return false;
  tp = tp.GetArrayInnerType();
  if (tp.Type != TYPE_String) return false;
  // second arg should be int (actually, bool, but it is converted to int)
  if (mt->ParamFlags[1]&~FPARM_Const) return false;
  tp = mt->ParamTypes[1];
  if (tp.Type != TYPE_Int && tp.Type != TYPE_Bool) return false;
  // third arg should be `out array!string`
  if (mt->ParamFlags[2] != FPARM_Out) return false;
  tp = mt->ParamTypes[2];
  if (tp.Type != TYPE_DynamicArray) return false;
  tp = tp.GetArrayInnerType();
  if (tp.Type != TYPE_String) return false;
  return true;
}


//==========================================================================
//
//  VClass::CreateMethodMap
//
//==========================================================================
void VClass::CreateMethodMap () {
  // build mehtod map and console command map
  MethodMap.clear();
  ConCmdListMts.clear();
  for (VClass *cls = this; cls; cls = cls->GetSuperClass()) {
    for (int f = 0; f < cls->Methods.length(); ++f) {
      VMethod *mt = cls->Methods[f];
      if (!mt || mt->Name == NAME_None) continue;
      if (!MethodMap.has(mt->Name)) MethodMap.put(mt->Name, mt);
      // check and register console command (including autocompleters)
      if (mt->ReturnType.Type != TYPE_Void) continue;
      const char *mtname = *mt->Name;
      if (!VStr::startsWith(mtname, "Cheat_")) continue;
      if (!mtname[6] || mtname[6] == '_') continue;
      // should not be "special" or networked
      if (!mt->IsNormal() || mt->IsNetwork()) continue;
      if (VStr::endsWithNoCase(mtname, "_AC")) {
        if (!IsGoodAC(mt)) continue;
      } else {
        if (mt->NumParams != 0) continue;
      }
      VStr loname = VStr(mtname+6).toLowerCase();
      if (!ConCmdListMts.has(loname)) ConCmdListMts.put(loname, mt);
    }
  }
}


//==========================================================================
//
//  VBasePlayer::FindConCommandMethodIdx
//
//==========================================================================
VMethod *VClass::FindConCommandMethod (const VStr &name, bool exact) {
  if (name.isEmpty()) return nullptr;
  VStr loname = name.toLowerCase();
  auto mtp = ConCmdListMts.find(loname);
  if (!mtp) return nullptr;
  if (!exact && VStr::endsWithNoCase(*(*mtp)->Name, "_AC")) return nullptr;
  return *mtp;
}


//==========================================================================
//
//  VClass::InitStatesLookup
//
//==========================================================================
void VClass::InitStatesLookup () {
  // this is also called from dehacked parser, so we must do this check
  if (StatesLookup.Num()) return;
  // create states lookup table
  if (GetSuperClass()) {
    GetSuperClass()->InitStatesLookup();
    for (int i = 0; i < GetSuperClass()->StatesLookup.Num(); ++i) {
      StatesLookup.Append(GetSuperClass()->StatesLookup[i]);
    }
  }
  for (VState *S = NetStates; S; S = S->NetNext) {
    S->NetId = StatesLookup.Num();
    StatesLookup.Append(S);
  }
}


#if !defined(IN_VCC)
//==========================================================================
//
//  VClass::CreateDefaults
//
//==========================================================================
void VClass::CreateDefaults () {
  if (Defaults) return;

  if (ParentClass && !ParentClass->Defaults) ParentClass->CreateDefaults();

  // allocate memory
  Defaults = new vuint8[ClassSize];
  memset(Defaults, 0, ClassSize);

  /*
  if (Fields) {
    fprintf(stderr, "=== FIELDS OF '%s' (before:%p) ===\n", GetName());
    for (VField *fi = Fields; fi; fi = fi->Next) {
      if (fi->Type.Type == TYPE_Int) {
        fprintf(stderr, "  %s: %d v=%d\n", fi->GetName(), fi->Ofs, *(vint32 *)(Defaults+fi->Ofs));
      } else if (fi->Type.Type == TYPE_Float) {
        fprintf(stderr, "  %s: %d v=%f\n", fi->GetName(), fi->Ofs, *(float *)(Defaults+fi->Ofs));
      } else {
        //fprintf(stderr, "  %s: %d\n", fi->GetName(), fi->Ofs);
      }
    }
  }
  */

  // copy default properties from the parent class
  if (ParentClass) {
    //fprintf(stderr, "COPYING `%s` to `%s`\n", ParentClass->GetName(), GetName());
    ParentClass->CopyObject(ParentClass->Defaults, Defaults);
  }

  /*
  if (Fields) {
    fprintf(stderr, "=== FIELDS OF '%s' (after) ===\n", GetName());
    for (VField *fi = Fields; fi; fi = fi->Next) {
      if (fi->Type.Type == TYPE_Int) {
        fprintf(stderr, "  %s: %d v=%d\n", fi->GetName(), fi->Ofs, *(vint32 *)(Defaults+fi->Ofs));
      } else if (fi->Type.Type == TYPE_Float) {
        fprintf(stderr, "  %s: %d v=%f\n", fi->GetName(), fi->Ofs, *(float *)(Defaults+fi->Ofs));
      } else {
        //fprintf(stderr, "  %s: %d\n", fi->GetName(), fi->Ofs);
      }
    }
  }
  */

  // call default properties method
  if (DefaultProperties) {
    //fprintf(stderr, "CALLING defprops on `%s`\n", GetName());
    P_PASS_REF((VObject *)Defaults);
    VObject::ExecuteFunction(DefaultProperties);
  }
}


//==========================================================================
//
//  VClass::CopyObject
//
//==========================================================================
void VClass::CopyObject (const vuint8 *Src, vuint8 *Dst) {
  // copy parent class fields
  if (GetSuperClass()) {
    //GLog.Logf(NAME_Dev, "COPYING SUPER fields of `%s` (super is '%s')...", GetName(), GetSuperClass()->GetName());
    GetSuperClass()->CopyObject(Src, Dst);
  }
  // copy fields
  //GLog.Logf(NAME_Dev, "COPYING fields of `%s`...", GetName());
  for (VField *F = Fields; F; F = F->Next) {
    if (F->Flags&FIELD_Internal) {
      //fprintf(stderr, "skipping field '%s' of `%s`... (ofs=%d, type=%s)\n", F->GetName(), GetName(), F->Ofs, *F->Type.GetName());
      continue;
    }
    //GLog.Logf(NAME_Dev, "  COPYING field '%s' of `%s`... (ofs=%d, type=%s)", F->GetName(), GetName(), F->Ofs, *F->Type.GetName());
    VField::CopyFieldValue(Src+F->Ofs, Dst+F->Ofs, F->Type);
  }
  //GLog.Logf(NAME_Dev, "DONE COPYING fields of `%s`...", GetName());
}


//==========================================================================
//
//  VClass::CleanObject
//
//==========================================================================
void VClass::CleanObject (VObject *Obj) {
  if (Obj) {
    for (VField *F = ReferenceFields; F; F = F->NextReference) VField::CleanField((vuint8 *)Obj+F->Ofs, F->Type);
  }
}


//==========================================================================
//
//  VClass::DestructObject
//
//==========================================================================
void VClass::DestructObject (VObject *Obj) {
  if (Obj) {
    for (VField *F = DestructorFields; F; F = F->DestructorLink) VField::DestructField((vuint8 *)Obj+F->Ofs, F->Type);
  }
}


//==========================================================================
//
//  VClass::CreateDerivedClass
//
//==========================================================================
VClass *VClass::CreateDerivedClass (VName AName, VMemberBase *AOuter, TArray<VDecorateUserVarDef> &uvlist, const TLocation &ALoc) {
  VClass *NewClass = nullptr;
  for (int i = 0; i < GDecorateClassImports.Num(); ++i) {
    if (GDecorateClassImports[i]->Name == AName) {
      // this class implements a decorate import class
      NewClass = GDecorateClassImports[i];
      NewClass->MemberType = MEMBER_Class;
      NewClass->Outer = AOuter;
      NewClass->Loc = ALoc;
      // make sure parent class is correct
      VClass *Check = FindClass(*NewClass->ParentClassName);
      if (!Check) Sys_Error("No such class `%s`", *NewClass->ParentClassName);
      if (!IsChildOf(Check)) Sys_Error("`%s` must be a child of `%s`", *AName, *Check->Name);
      GDecorateClassImports.RemoveIndex(i);
      break;
    }
  }
  if (!NewClass) NewClass = new VClass(AName, AOuter, ALoc);
  NewClass->ParentClass = this;

  if (uvlist.length()) {
    // create replication:
    //   replication {
    //     reliable if (Role == ROLE_Authority && bNetOwner)
    //       <fields>;
    //   }

    VRepInfo &RI = NewClass->RepInfos.Alloc();
    RI.Reliable = true;

    // replication condition
    RI.Cond = new VMethod(NAME_None, NewClass, ALoc);
    RI.Cond->ReturnType = VFieldType(TYPE_Bool);
    RI.Cond->ReturnType.BitMask = 1;
    RI.Cond->ReturnTypeExpr = new VTypeExprSimple(RI.Cond->ReturnType, ALoc);
    {
      VExpression *eOwnerField = new VSingleName("bNetOwner", ALoc);
      VExpression *eRoleConst = new VSingleName("ROLE_Authority", ALoc);
      VExpression *eRoleFld = new VSingleName("Role", ALoc);
      VExpression *eCmp = new VBinary(VBinary::EBinOp::Equals, eRoleFld, eRoleConst, ALoc);
      VExpression *eCond = new VBinaryLogical(VBinaryLogical::ELogOp::And, eOwnerField, eCmp, ALoc);
      RI.Cond->Statement = new VReturn(eCond, ALoc);
    }
    NewClass->AddMethod(RI.Cond);

    // replication fields
    for (int f = 0; f < uvlist.length(); ++f) {
      VRepField &F = RI.RepFields.Alloc();
      F.Name = uvlist[f].name;
      F.Loc = ALoc;
      F.Member = nullptr;
    }

    // define user fields
    VField *PrevBool = nullptr;
    for (int f = 0; f < uvlist.length(); ++f) {
      VField *fi = new VField(uvlist[f].name, NewClass, uvlist[f].loc);
      VTypeExpr *te = VTypeExpr::NewTypeExpr(uvlist[f].type, uvlist[f].loc);
      fi->TypeExpr = te;
      fi->Type = uvlist[f].type;
      fi->Flags = 0;
      NewClass->AddField(fi);
      NewClass->DecorateStateFieldTrans.put(uvlist[f].name, uvlist[f].name); // so field name will be case-insensitive
      // process boolean field
      if (!fi->Define()) Sys_Error("cannot define field '%s' in class '%s'", *uvlist[f].name, *AName);
      //fprintf(stderr, "FI: <%s> (%s)\n", *fi->GetFullName(), *fi->Type.GetName());
      if (fi->Type.Type == TYPE_Bool && PrevBool && PrevBool->Type.BitMask != 0x80000000) {
        fi->Type.BitMask = PrevBool->Type.BitMask<<1;
      }
      PrevBool = (fi->Type.Type == TYPE_Bool ? fi : nullptr);
    }

  }

  if (!NewClass->DefineRepInfos()) Sys_Error("cannot post-process replication info for class '%s'", *AName);

  //!DecorateDefine();
  //fprintf(stderr, "*** '%s' : '%s' (%d) ***\n", NewClass->GetName(), GetName(), (NewClass->ObjectFlags&CLASSOF_PostLoaded ? 1 : 0));
  //fprintf(stderr, " class size 0: %d  %d  (%d)\n", NewClass->ClassUnalignedSize, NewClass->ClassSize, ParentClass->ClassSize);
  NewClass->PostLoad();
  //fprintf(stderr, " class size 1: %d  %d  (%d)\n", NewClass->ClassUnalignedSize, NewClass->ClassSize, ParentClass->ClassSize);
  NewClass->CreateDefaults();

  /*
  if (NewClass->Fields) {
    for (VField *fi = NewClass->Fields; fi; fi = fi->Next) {
      if (fi->Type.Type == TYPE_Int) {
        fprintf(stderr, "  %s: %d v=%d\n", fi->GetName(), fi->Ofs, *(vint32 *)(NewClass->Defaults+fi->Ofs));
      } else if (fi->Type.Type == TYPE_Float) {
        fprintf(stderr, "  %s: %d v=%f\n", fi->GetName(), fi->Ofs, *(float *)(NewClass->Defaults+fi->Ofs));
      } else {
        fprintf(stderr, "  %s: %d\n", fi->GetName(), fi->Ofs);
      }
    }
  }
  */

  return NewClass;
}
#endif //!defined(IN_VCC)


//==========================================================================
//
//  VClass::GetReplacement
//
//==========================================================================
VClass *VClass::GetReplacement () {
  check(this);
  if (!Replacement) return this;
  // avoid looping recursion by temporarely nullptr-ing the field
  VClass *Temp = Replacement;
  Replacement = nullptr;
  VClass *Ret = Temp->GetReplacement();
  Replacement = Temp;
  return Ret;
}


//==========================================================================
//
//  VClass::GetReplacee
//
//==========================================================================
VClass *VClass::GetReplacee () {
  if (!Replacee) return this;
  // avoid looping recursion by temporarely nullptr-ing the field
  VClass *Temp = Replacee;
  Replacee = nullptr;
  VClass *Ret = Temp->GetReplacee();
  Replacee = Temp;
  return Ret;
}


//==========================================================================
//
//  VClass::SetReplacement
//
//  assign `cls` as a replacement for this
//
//==========================================================================
bool VClass::SetReplacement (VClass *cls) {
  if (!cls) return (Replacement == nullptr);
  if (cls == this) return false; // cannot replace itself
  if (Replacement == cls) return true; // nothing to do
  if (Replacement) return false; // already set
  // sanity check: `cls` should not be already assigned as some replacement
  if (cls->Replacee) return false;
  // do it
  Replacement = cls;
  cls->Replacee = this;
  return true;
}


//==========================================================================
//
//  VClass::HashLowerCased
//
//==========================================================================
/*
void VClass::HashLowerCased () {
  LowerCaseName = *VStr(Name).ToLower();
  int HashIndex = GetTypeHash(LowerCaseName)&(LOWER_CASE_HASH_SIZE-1);
  LowerCaseHashNext = GLowerCaseHashTable[HashIndex];
  GLowerCaseHashTable[HashIndex] = this;
}
*/


//==========================================================================
//
//  operator<<
//
//==========================================================================
VStream &operator << (VStream &Strm, VStateLabel &Lbl) {
  return Strm << Lbl.Name << Lbl.State << Lbl.SubLabels;
}


//==========================================================================
//
//  VClass::DFStateSetTexDir
//
//==========================================================================
void VClass::DFStateSetTexDir (const VStr &adir) {
  dfStateTexDirSet = true;
  dfStateTexDir = adir;
}


//==========================================================================
//
//  VClass::DFStateGetTexDir
//
//==========================================================================
const VStr &VClass::DFStateGetTexDir () const {
  for (const VClass *c = this; c; c = c->ParentClass) {
    if (c->dfStateTexDirSet) return c->dfStateTexDir;
  }
  // this can be not postloaded yet, so...
  if (!ParentClass) {
    VName pn = ParentClassName;
    while (pn != NAME_None) {
      VClass *c = StaticFindClass(pn);
      if (!c) break;
      if (c->dfStateTexDirSet) return c->dfStateTexDir;
      pn = c->ParentClassName;
    }
  }
  return VStr::EmptyString;
}


//==========================================================================
//
//  VClass::DFStateAddTexture
//
//==========================================================================
void VClass::DFStateAddTexture (const VStr &tname, const TextureInfo &ti) {
  if (tname.isEmpty()) return;
  dfStateTexList.put(tname, ti);
}


//==========================================================================
//
//  VClass::DFStateGetTexture
//
//==========================================================================
bool VClass::DFStateGetTexture (const VStr &tname, TextureInfo &ti) const {
  ti.texImage.clear();
  ti.frameWidth = ti.frameHeight = 0;
  ti.frameOfsX = ti.frameOfsY = 0;
  if (tname.isEmpty()) return false;
  auto xti = dfStateTexList.get(tname);
  if (xti) {
    ti = *xti;
    return true;
  }
  for (const VClass *c = this->ParentClass; c; c = c->ParentClass) {
    if (c->DFStateGetTexture(tname, ti)) return true;
  }
  // this can be not postloaded yet, so...
  if (!ParentClass) {
    VName pn = ParentClassName;
    while (pn != NAME_None) {
      VClass *c = StaticFindClass(pn);
      if (!c) break;
      if (c->DFStateGetTexture(tname, ti)) return true;
      pn = c->ParentClassName;
    }
  }
  return false;
}
