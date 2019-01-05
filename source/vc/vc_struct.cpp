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


//==========================================================================
//
//  VStruct::VStruct
//
//==========================================================================
VStruct::VStruct (VName AName, VMemberBase *AOuter, TLocation ALoc)
  : VMemberBase(MEMBER_Struct, AName, AOuter, ALoc)
  , ParentStruct(0)
  , IsVector(false)
  , StackSize(0)
  , Fields(0)
  , ParentStructName(NAME_None)
  , Defined(true)
  , PostLoaded(false)
  , Size(0)
  , Alignment(0)
  , ReferenceFields(0)
  , DestructorFields(0)
  , AliasList()
  , AliasFrameNum(0)
  , cacheNeedDTor(-1)
#if !defined(IN_VCC)
  , cacheNeedCleanup(-1)
#endif
{
}


//==========================================================================
//
//  VStruct::CompilerShutdown
//
//==========================================================================
void VStruct::CompilerShutdown () {
  VMemberBase::CompilerShutdown();
  //AliasList.clear();
}


//==========================================================================
//
//  VStruct::ResolveAlias
//
//  returns `aname` for unknown alias, or `NAME_None` for alias loop
//
//==========================================================================
VName VStruct::ResolveAlias (VName aname) {
  if (aname == NAME_None) return NAME_None;
  if (++AliasFrameNum == 0x7fffffff) {
    for (auto it = AliasList.first(); it; ++it) it.getValue().aframe = 0;
    AliasFrameNum = 1;
  }
  VName res = aname;
  for (;;) {
    auto ai = AliasList.get(aname);
    if (!ai) {
      if (!ParentStruct) return res;
      return ParentStruct->ResolveAlias(res);
    }
    if (ai->aframe == AliasFrameNum) return NAME_None; // loop
    res = ai->origName;
    ai->aframe = AliasFrameNum;
    aname = res;
  }
}


//==========================================================================
//
//  VStruct::Serialise
//
//==========================================================================
void VStruct::Serialise (VStream &Strm) {
  guard(VStruct::Serialise);
  VMemberBase::Serialise(Strm);
  vuint8 xver = 0; // current version is 0
  Strm << xver;
  vuint32 acount = AliasList.count();
  Strm << acount;
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
  Strm << ParentStruct
    << IsVector
    << STRM_INDEX(StackSize)
    << Fields;

  if (Strm.IsLoading()) {
    cacheNeedDTor = -1;
#if !defined(IN_VCC)
    cacheNeedCleanup = -1;
#endif
  }
  unguard;
}


//==========================================================================
//
//  VStruct::AddField
//
//==========================================================================
void VStruct::AddField (VField *f) {
  guard(VStruct::AddField);
  for (VField *Check = Fields; Check; Check = Check->Next) {
    if (f->Name == Check->Name) {
      ParseError(f->Loc, "Redeclared field");
      ParseError(Check->Loc, "Previous declaration here");
    }
  }

  if (!Fields) {
    Fields = f;
  } else {
    VField *Prev = Fields;
    while (Prev->Next) Prev = Prev->Next;
    Prev->Next = f;
  }
  f->Next = nullptr;

  cacheNeedDTor = -1;
#if !defined(IN_VCC)
  cacheNeedCleanup = -1;
#endif
  unguard;
}


//==========================================================================
//
//  VStruct::FindField
//
//==========================================================================
VField *VStruct::FindField (VName FieldName) {
  guard(VStruct::FindField);
  if (FieldName == NAME_None) return nullptr;
  FieldName = ResolveAlias(FieldName);
  for (VField *fi = Fields; fi; fi = fi->Next) if (fi->Name == FieldName) return fi;
  if (ParentStruct) return ParentStruct->FindField(FieldName);
  return nullptr;
  unguard;
}


//==========================================================================
//
//  VStruct::NeedsDestructor
//
//==========================================================================
bool VStruct::NeedsDestructor () {
  guard(VStruct::NeedsDestructor);
  if (cacheNeedDTor >= 0) return (cacheNeedDTor != 0);
  cacheNeedDTor = 1;
  for (VField *F = Fields; F; F = F->Next) if (F->NeedsDestructor()) return true;
  if (ParentStruct && ParentStruct->NeedsDestructor()) return true;
  cacheNeedDTor = 0;
  return false;
  unguard;
}


//==========================================================================
//
//  VStruct::Define
//
//==========================================================================
bool VStruct::Define () {
  guard(VStruct::Define);
  if (ParentStructName != NAME_None) {
    VFieldType type = StaticFindType((Outer->MemberType == MEMBER_Class ? (VClass *)Outer : nullptr), ParentStructName);
    if (type.Type != TYPE_Struct) {
      ParseError(ParentStructLoc, "%s is not a struct type", *ParentStructName);
    } else {
      ParentStruct = type.Struct;
    }
  }

  if (ParentStruct && !ParentStruct->Defined) {
    ParseError(ParentStructLoc, "Parent struct must be declared before");
    return false;
  }

  Defined = true;
  return true;
  unguard;
}


//==========================================================================
//
//  VStruct::DefineMembers
//
//==========================================================================
bool VStruct::DefineMembers () {
  guard(VStruct::DefineMembers);
  bool Ret = true;

  // define fields
  vint32 size = 0;
  if (ParentStruct) size = ParentStruct->StackSize*4;
  VField *PrevBool = nullptr;
  for (VField *fi = Fields; fi; fi = fi->Next) {
    if (!fi->Define()) Ret = false;
    if (fi->Type.Type == TYPE_Bool && PrevBool && PrevBool->Type.BitMask != 0x80000000) {
      fi->Type.BitMask = PrevBool->Type.BitMask<<1;
    } else {
      size += fi->Type.GetStackSize();
    }
    PrevBool = (fi->Type.Type == TYPE_Bool ? fi : nullptr);
  }

  // validate vector type
  if (IsVector) {
    int fc = 0;
    for (VField *f = Fields; f; f = f->Next) {
      if (f->Type.Type != TYPE_Float) {
        ParseError(f->Loc, "Vector can have only float fields");
        Ret = false;
      }
      ++fc;
    }
    if (fc != 3) {
      ParseError(Loc, "Vector must have exactly 3 float fields");
      Ret = false;
    }
  }

  StackSize = (size+3)/4;
  return Ret;
  unguard;
}


//==========================================================================
//
//  VStruct::IsA
//
//==========================================================================
bool VStruct::IsA (const VStruct *s) const {
  if (!s) return false;
  for (const VStruct *me = this; me; me = me->ParentStruct) {
    if (s == me) return true;
  }
  return false;
}


//==========================================================================
//
//  VStruct::PostLoad
//
//==========================================================================
void VStruct::PostLoad () {
  guard(VStruct::PostLoad);
  if (PostLoaded) return; // already done

  // make sure parent struct has been set up
  if (ParentStruct) ParentStruct->PostLoad();

  // calculate field offsets and class size
  CalcFieldOffsets();

  // set up list of reference fields
  InitReferences();

  // set up list of destructor fields
  InitDestructorFields();

  PostLoaded = true;
  unguard;
}


//==========================================================================
//
//  VStruct::CalcFieldOffsets
//
//==========================================================================
void VStruct::CalcFieldOffsets () {
  guard(VStruct::CalcFieldOffsets);
  int size = (ParentStruct ? ParentStruct->Size : 0);
  Alignment = (ParentStruct ? ParentStruct->Alignment : 0);
  VField *PrevField = nullptr;
  for (VField *fi = Fields; fi; fi = fi->Next) {
    if (fi->Type.Type == TYPE_Bool && PrevField &&
        PrevField->Type.Type == TYPE_Bool &&
        PrevField->Type.BitMask != 0x80000000)
    {
      vuint32 bit_mask = PrevField->Type.BitMask << 1;
      if (fi->Type.BitMask != bit_mask) Sys_Error("Wrong bit mask");
      fi->Type.BitMask = bit_mask;
      fi->Ofs = PrevField->Ofs;
    } else {
      if (fi->Type.Type == TYPE_Struct ||
          (fi->Type.Type == TYPE_Array && fi->Type.ArrayInnerType == TYPE_Struct))
      {
        // make sure struct size has been calculated
        fi->Type.Struct->PostLoad();
      }
      // align field offset
      int FldAlign = fi->Type.GetAlignment();
      size = (size+FldAlign-1)&~(FldAlign-1);
      // structure itself has the bigest alignment
      if (Alignment < FldAlign) Alignment = FldAlign;
      fi->Ofs = size;
      size += fi->Type.GetSize();
    }
    PrevField = fi;
  }
  Size = (size+Alignment-1)&~(Alignment-1);
  unguard;
}


//==========================================================================
//
//  VStruct::InitReferences
//
//==========================================================================
void VStruct::InitReferences () {
  guard(VStruct::InitReferences);
  // invalidate caches (just in case)
  cacheNeedDTor = -1;
#if !defined(IN_VCC)
  cacheNeedCleanup = -1;
#endif
  ReferenceFields = nullptr;
  if (ParentStruct) ReferenceFields = ParentStruct->ReferenceFields;
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
  unguard;
}


//==========================================================================
//
//  VStruct::InitDestructorFields
//
//==========================================================================
void VStruct::InitDestructorFields () {
  guard(VStruct::InitDestructorFields);
  // invalidate caches (just in case)
  cacheNeedDTor = -1;
#if !defined(IN_VCC)
  cacheNeedCleanup = -1;
#endif
  DestructorFields = nullptr;
  if (ParentStruct) DestructorFields = ParentStruct->DestructorFields;
  for (VField *F = Fields; F; F = F->Next) {
    switch (F->Type.Type) {
#if !defined(IN_VCC)
      case TYPE_Reference:
      case TYPE_Delegate:
        cacheNeedCleanup = 1;
        break;
#endif
      case TYPE_String:
        cacheNeedDTor = 1; // anyway
        F->DestructorLink = DestructorFields;
        DestructorFields = F;
        break;
      case TYPE_Struct:
        F->Type.Struct->PostLoad();
        if (F->Type.Struct->DestructorFields) {
          cacheNeedDTor = 1; // anyway
          F->DestructorLink = DestructorFields;
          DestructorFields = F;
        }
        break;
      case TYPE_Array:
        if (F->Type.ArrayInnerType == TYPE_String) {
          cacheNeedDTor = 1; // anyway
          F->DestructorLink = DestructorFields;
          DestructorFields = F;
        } else if (F->Type.ArrayInnerType == TYPE_Struct) {
          F->Type.Struct->PostLoad();
          if (F->Type.Struct->DestructorFields) {
            cacheNeedDTor = 1; // anyway
            F->DestructorLink = DestructorFields;
            DestructorFields = F;
          }
        }
        break;
      case TYPE_DynamicArray:
      case TYPE_Dictionary:
        cacheNeedDTor = 1; // anyway
        F->DestructorLink = DestructorFields;
        DestructorFields = F;
        break;
    }
  }
  unguard;
}


#if !defined(IN_VCC)

//==========================================================================
//
//  VStruct::CopyObject
//
//==========================================================================
void VStruct::CopyObject (const vuint8 *Src, vuint8 *Dst) {
  guard(VStruct::CopyObject);
  // copy parent struct's fields
  if (ParentStruct) ParentStruct->CopyObject(Src, Dst);
  // copy fields
  for (VField *F = Fields; F; F = F->Next) {
    if (F->Flags&FIELD_Internal) continue;
    VField::CopyFieldValue(Src+F->Ofs, Dst+F->Ofs, F->Type);
  }
  unguardf(("(%s)", *Name));
}


//==========================================================================
//
//  VStruct::SkipSerialisedObject
//
//==========================================================================
void VStruct::SkipSerialisedObject (VStream &Strm) {
  VName psname = NAME_None;
  Strm << psname;
  if (psname != NAME_None) SkipSerialisedObject(Strm); // skip parent struct
  vint32 fldcount = 0;
  Strm << STRM_INDEX(fldcount);
  while (fldcount--) {
    VName fname = NAME_None;
    Strm << fname;
    VField::SkipSerialisedValue(Strm);
  }
}


//==========================================================================
//
//  VStruct::SerialiseObject
//
//==========================================================================
void VStruct::SerialiseObject (VStream &Strm, vuint8 *Data) {
  guard(VStruct::SerialiseObject);
  // serialise parent struct's fields
  if (Strm.IsLoading()) {
    // load parent struct
    VName psname = NAME_None;
    Strm << psname;
    if (ParentStruct) {
      if (ParentStruct->Name != psname) Sys_Error("I/O ERROR: expected parent struct '%s', got '%s'", *ParentStruct->Name, *psname);
      ParentStruct->SerialiseObject(Strm, Data);
    } else {
      if (psname != NAME_None) Sys_Error("I/O ERROR: expected no parent struct, got '%s'", *psname);
    }
  } else {
    // save parent struct
    VName psname = (ParentStruct ? ParentStruct->Name : NAME_None);
    Strm << psname;
    if (ParentStruct) ParentStruct->SerialiseObject(Strm, Data);
  }
/*
  // serialise fields
  for (VField *F = Fields; F; F = F->Next) {
    // skip native and transient fields
    if (F->Flags&(FIELD_Native|FIELD_Transient)) continue;
    VField::SerialiseFieldValue(Strm, Data+F->Ofs, F->Type);
  }
*/
  // serialise fields
  vint32 fldcount = 0;
  if (Strm.IsLoading()) {
    // load fields
    TMap<VName, VField *> flist;
    for (VField *F = Fields; F; F = F->Next) {
      // skip native and transient fields
      if (F->Flags&(FIELD_Native|FIELD_Transient)) continue;
      flist.put(F->Name, F);
    }
    // number of fields
    Strm << STRM_INDEX(fldcount);
    while (fldcount--) {
      VName fname = NAME_None;
      Strm << fname;
      auto ffp = flist.find(fname);
      if (!ffp) {
#if defined(IN_VCC) || defined(VCC_STANDALONE_EXECUTOR)
        fprintf(stderr, "I/O WARNING: field '%s' not found\n", *fname);
#else
        GCon->Logf("I/O WARNING: field '%s' not found", *fname);
#endif
        VField::SkipSerialisedValue(Strm);
      } else {
        VField *F = *ffp;
        flist.remove(fname);
        VField::SerialiseFieldValue(Strm, Data+F->Ofs, F->Type, F->GetFullName());
      }
    }
    // show missing fields
    while (flist.count()) {
      auto it = flist.first();
      VName fname = it.getKey();
#if defined(IN_VCC) || defined(VCC_STANDALONE_EXECUTOR)
      fprintf(stderr, "I/O WARNING: field '%s' is missing\n", *fname);
#else
      GCon->Logf("I/O WARNING: field '%s' is missing", *fname);
#endif
      flist.remove(fname);
    }
  } else {
    // save fields
    for (VField *F = Fields; F; F = F->Next) {
      // skip native and transient fields
      if (F->Flags&(FIELD_Native|FIELD_Transient)) continue;
      ++fldcount;
    }
    Strm << STRM_INDEX(fldcount);
    for (VField *F = Fields; F; F = F->Next) {
      // skip native and transient fields
      if (F->Flags&(FIELD_Native|FIELD_Transient)) continue;
      Strm << F->Name;
      VField::SerialiseFieldValue(Strm, Data+F->Ofs, F->Type, F->GetFullName());
    }
  }
  unguardf(("(%s)", *Name));
}


//==========================================================================
//
//  VStruct::NeedToCleanObject
//
//==========================================================================
bool VStruct::NeedToCleanObject () {
  if (cacheNeedCleanup >= 0) return (cacheNeedCleanup != 0);
  cacheNeedCleanup = 1;
  //for (VField *F = ReferenceFields; F; F = F->NextReference) if (VField::NeedToCleanField(F->Type)) return true;
  for (VField *F = Fields; F; F = F->Next) if (VField::NeedToCleanField(F->Type)) return true;
  if (ParentStruct && ParentStruct->NeedToCleanObject()) return true;
  cacheNeedCleanup = 0;
  return false;
}


//==========================================================================
//
//  VStruct::CleanObject
//
//==========================================================================
bool VStruct::CleanObject (vuint8 *Data) {
  guard(VStruct::CleanObject);
  bool res = false;
  for (VField *F = ReferenceFields; F; F = F->NextReference) {
    if (VField::CleanField(Data+F->Ofs, F->Type)) res = true;
  }
  return res;
  unguardf(("(%s)", *Name));
}


//==========================================================================
//
//  VStruct::DestructObject
//
//==========================================================================
void VStruct::DestructObject (vuint8 *Data) {
  guard(VStruct::DestructObject);
  for (VField *F = DestructorFields; F; F = F->DestructorLink) VField::DestructField(Data+F->Ofs, F->Type);
  unguardf(("(%s)", *Name));
}


//==========================================================================
//
//  VStruct::ZeroObject
//
//==========================================================================
void VStruct::ZeroObject (vuint8 *Data) {
  for (VField *F = Fields; F; F = F->Next) VField::DestructField(Data+F->Ofs, F->Type, true);
}


//==========================================================================
//
//  VStruct::IdenticalObject
//
//==========================================================================
bool VStruct::IdenticalObject (const vuint8 *Val1, const vuint8 *Val2) {
  guard(VStruct::IdenticalObject);
  // compare parent struct's fields
  if (ParentStruct) {
    if (!ParentStruct->IdenticalObject(Val1, Val2)) return false;
  }
  // compare fields
  for (VField *F = Fields; F; F = F->Next) {
    if (!VField::IdenticalValue(Val1+F->Ofs, Val2+F->Ofs, F->Type)) return false;
  }
  return true;
  unguardf(("(%s)", *Name));
}
#endif //!defined(IN_VCC)


#if !defined(IN_VCC)
//==========================================================================
//
//  VStruct::NetSerialiseObject
//
//==========================================================================
bool VStruct::NetSerialiseObject (VStream &Strm, VNetObjectsMap *Map, vuint8 *Data) {
  guard(VStruct::NetSerialiseObject);
  bool Ret = true;
  // serialise parent struct's fields
  if (ParentStruct) Ret = ParentStruct->NetSerialiseObject(Strm, Map, Data);
  // serialise fields
  for (VField *F = Fields; F; F = F->Next) {
    if (!VField::NetSerialiseValue(Strm, Map, Data+F->Ofs, F->Type)) Ret = false;
  }
  return Ret;
  unguardf(("(%s)", *Name));
}
#endif


//==========================================================================
//
//  VStruct::CreateWrapperStruct
//
//==========================================================================
VStruct *VStruct::CreateWrapperStruct (VExpression *aTypeExpr, VMemberBase *AOuter, TLocation ALoc) {
  check(aTypeExpr);
  check(AOuter);
  VStruct *st = new VStruct(NAME_None, AOuter, ALoc);
  st->Defined = false;
  st->IsVector = false;
  st->Fields = nullptr;

  VField *fi = new VField(VName("__"), st, ALoc);
  fi->TypeExpr = aTypeExpr;
  if (aTypeExpr->IsDelegateType()) {
    fi->Func = ((VDelegateType *)aTypeExpr)->CreateDelegateMethod(st);
    fi->Type = VFieldType(TYPE_Delegate);
    fi->Type.Function = fi->Func;
    fi->TypeExpr = nullptr;
    delete aTypeExpr;
  }
  st->AddField(fi);

  return st;
}
