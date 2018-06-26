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
{
}


//==========================================================================
//
//  VStruct::CompilerShutdown
//
//==========================================================================
void VStruct::CompilerShutdown () {
  VMemberBase::CompilerShutdown();
  AliasList.clear();
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
bool VStruct::NeedsDestructor () const {
  guard(VStruct::NeedsDestructor);
  for (VField *F = Fields; F; F = F->Next) if (F->NeedsDestructor()) return true;
  if (ParentStruct) return ParentStruct->NeedsDestructor();
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
    PrevBool = fi->Type.Type == TYPE_Bool ? fi : nullptr;
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
  DestructorFields = nullptr;
  if (ParentStruct) DestructorFields = ParentStruct->DestructorFields;
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
  for (VField *F = Fields; F; F = F->Next) VField::CopyFieldValue(Src+F->Ofs, Dst+F->Ofs, F->Type);
  unguardf(("(%s)", *Name));
}


//==========================================================================
//
//  VStruct::SerialiseObject
//
//==========================================================================
void VStruct::SerialiseObject (VStream &Strm, vuint8 *Data) {
  guard(VStruct::SerialiseObject);
  // serialise parent struct's fields
  if (ParentStruct) ParentStruct->SerialiseObject(Strm, Data);
  // serialise fields
  for (VField *F = Fields; F; F = F->Next) {
    // skip native and transient fields
    if (F->Flags&(FIELD_Native|FIELD_Transient)) continue;
    VField::SerialiseFieldValue(Strm, Data+F->Ofs, F->Type);
  }
  unguardf(("(%s)", *Name));
}


//==========================================================================
//
//  VStruct::CleanObject
//
//==========================================================================
void VStruct::CleanObject (vuint8 *Data) {
  guard(VStruct::CleanObject);
  for (VField *F = ReferenceFields; F; F = F->NextReference) VField::CleanField(Data+F->Ofs, F->Type);
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


#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
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
