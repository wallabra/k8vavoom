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
//**  Copyright (C) 2018-2020 Ketmar Dark
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


//==========================================================================
//
//  NeedDtor
//
//==========================================================================
static inline bool NeedDtor (const VFieldType &Type) {
  if (Type.Type == TYPE_String) return true;
  if (Type.Type == TYPE_DynamicArray) return true;
  if (Type.Type == TYPE_Dictionary) return true;
  if (Type.Type == TYPE_Array) {
    if (Type.ArrayInnerType == TYPE_String) return true;
    if (Type.ArrayInnerType == TYPE_Struct) return Type.Struct->NeedsDestructor();
  }
  if (Type.Type == TYPE_Struct) return Type.Struct->NeedsDestructor();
  return false;
}


//==========================================================================
//
//  VField::VField
//
//==========================================================================
VField::VField (VName AName, VMemberBase *AOuter, TLocation ALoc)
  : VMemberBase(MEMBER_Field, AName, AOuter, ALoc)
  , Next(nullptr)
  , Type(TYPE_Void)
  , Func(nullptr)
  , Flags(0)
  , ReplCond(nullptr)
  , TypeExpr(nullptr)
  , NextReference(0)
  , DestructorLink(0)
  , NextNetField(0)
  , Ofs(0)
  , NetIndex(-1)
{
}


//==========================================================================
//
//  VField::~VField
//
//==========================================================================
VField::~VField () {
  delete TypeExpr; TypeExpr = nullptr;
}


//==========================================================================
//
//  VField::CompilerShutdown
//
//==========================================================================
void VField::CompilerShutdown () {
  VMemberBase::CompilerShutdown();
  delete TypeExpr; TypeExpr = nullptr;
}


//==========================================================================
//
//  VField::Serialise
//
//==========================================================================
void VField::Serialise (VStream &Strm) {
  VMemberBase::Serialise(Strm);
  vuint8 xver = 0; // current version is 0
  Strm << xver;
  Strm << Next
    << Type
    << Func
    << STRM_INDEX(Flags)
    << ReplCond;
}


//==========================================================================
//
//  VField::NeedsDestructor
//
//==========================================================================
bool VField::NeedsDestructor () const {
  return NeedDtor(Type);
}


//==========================================================================
//
//  VField::Define
//
//==========================================================================
bool VField::Define () {
  if (Type.Type == TYPE_Delegate) return Func->Define();

  if (TypeExpr) {
    VEmitContext ec(this);
    TypeExpr = TypeExpr->ResolveAsType(ec);
  }
  if (!TypeExpr) return false;

  if (TypeExpr->Type.Type == TYPE_Void) {
    ParseError(TypeExpr->Loc, "Field cannot have `void` type");
    return false;
  }

  Type = TypeExpr->Type;
  return true;
}


//==========================================================================
//
//  VField::CopyFieldValue
//
//==========================================================================
void VField::CopyFieldValue (const vuint8 *Src, vuint8 *Dst, const VFieldType &Type) {
  if (Src == Dst) return; // nothing to do
  switch (Type.Type) {
    case TYPE_Int: *(vint32 *)Dst = *(const vint32 *)Src; break;
    case TYPE_Byte: *(vuint8 *)Dst = *(const vuint8 *)Src; break;
    case TYPE_Bool:
      if ((*(const vuint32 *)Src)&Type.BitMask) {
        *(vuint32 *)Dst |= Type.BitMask;
      } else {
        *(vuint32 *)Dst &= ~Type.BitMask;
      }
      break;
    case TYPE_Float: *(float *)Dst = *(const float *)Src; break;
    case TYPE_Vector: *(TVec *)Dst = *(const TVec *)Src; break;
    case TYPE_Name: *(VName *)Dst = *(const VName *)Src; break;
    case TYPE_String: *(VStr *)Dst = *(const VStr *)Src; break;
    case TYPE_Pointer: *(void **)Dst = *(void * const *)Src; break;
    case TYPE_Reference: *(VObject **)Dst = *(VObject * const *)Src; break;
    case TYPE_Class: *(VClass **)Dst = *(VClass * const *)Src; break;
    case TYPE_State: *(VState **)Dst = *(VState * const *)Src; break;
    case TYPE_Delegate: *(VObjectDelegate *)Dst = *(const VObjectDelegate *)Src; break;
    case TYPE_Struct: Type.Struct->CopyObject(Src, Dst); break;
    case TYPE_Array:
      {
        VFieldType IntType = Type;
        IntType.Type = Type.ArrayInnerType;
        int InnerSize = IntType.GetSize();
        for (int i = 0; i < Type.GetArrayDim(); ++i) CopyFieldValue(Src+i*InnerSize, Dst+i*InnerSize, IntType);
      }
      break;
    case TYPE_DynamicArray:
      {
        VScriptArray &ASrc = *(VScriptArray *)Src;
        VScriptArray &ADst = *(VScriptArray *)Dst;
        VFieldType IntType = Type;
        IntType.Type = Type.ArrayInnerType;
        int InnerSize = IntType.GetSize();
        ADst.SetNum(ASrc.Num(), IntType);
        for (int i = 0; i < ASrc.Num(); ++i) CopyFieldValue(ASrc.Ptr()+i*InnerSize, ADst.Ptr()+i*InnerSize, IntType);
      }
      break;
    case TYPE_SliceArray:
      memcpy(Dst, Src, (size_t)Type.GetSize());
      break;
    case TYPE_Dictionary:
      {
        const VScriptDict *src = (const VScriptDict *)Src;
        VScriptDict *dst = (VScriptDict *)Dst;
        src->copyTo(dst);
      }
      break;
  }
}


//==========================================================================
//
//  VField::SkipSerialisedType
//
//==========================================================================
void VField::SkipSerialisedType (VStream &Strm) {
  vuint8 tp = 0;
  Strm << tp;
  vint32 tmpi32, n, InnerSize;
  vuint8 tmpu8;
  float tmpf32;
  TVec tmpvec(0, 0, 0);
  VName tmpname, tmpname2;
  VStr tmpstr;
  VObject *tmpobj;
  switch (tp) {
    case TYPE_Int: Strm << tmpi32; break;
    case TYPE_Byte: Strm << tmpu8; break;
    case TYPE_Bool: Strm << tmpu8; break;
    case TYPE_Float: Strm << tmpf32; break;
    case TYPE_Vector: Strm << tmpvec; break;
    case TYPE_Name: Strm << tmpname; break;
    case TYPE_String: Strm << tmpstr; break;
    case TYPE_Pointer: // WARNING! keep in sync with sv_save.cpp!
      Strm << tp; // inner type
      if (tp != TYPE_Struct) {
        //Host_Error("I/O Error: don't know how to skip non-struct pointer type");
      } else {
        Strm << STRM_INDEX(tmpi32);
      }
      break;
    case TYPE_Reference: Strm << tmpobj; break;
    case TYPE_Class: Strm << tmpname; break;
    case TYPE_State: Strm << tmpname << tmpname2; break;
    case TYPE_Delegate: Strm << tmpobj; Strm << tmpname; break;
    case TYPE_Struct: Strm << tmpname; VStruct::SkipSerialisedObject(Strm); break;
    case TYPE_Array:
    case TYPE_DynamicArray:
      Strm << tp; // inner type
      Strm << STRM_INDEX(n);
      Strm << STRM_INDEX(InnerSize);
      if (n < 0 || InnerSize < 0) Host_Error("I/O Error: invalid array size");
      while (n--) SkipSerialisedType(Strm);
      break;
    case TYPE_SliceArray:
      //FIXME:SLICE
      break;
    case TYPE_Dictionary:
      {
        VFieldType t;
        Strm << t;
        VScriptDict::streamSkip(Strm);
      }
      break;
    default:
      Host_Error("I/O Error: unknown data type");
  }
}


//==========================================================================
//
//  VField::SkipSerialisedValue
//
//==========================================================================
void VField::SkipSerialisedValue (VStream &Strm) {
  SkipSerialisedType(Strm);
}


//==========================================================================
//
//  VField::SerialiseFieldValue
//
//==========================================================================
void VField::SerialiseFieldValue (VStream &Strm, vuint8 *Data, const VFieldType &Type) {
  vuint8 tp = Type.Type;
  Strm << tp;
  if (Strm.IsLoading()) {
    // check type
    if (tp != Type.Type) {
      // tolerate some type changes
      switch (tp) {
        case TYPE_Int:
          {
            vint32 v;
            Strm << v;
            switch (Type.Type) {
              case TYPE_Int: *(vint32 *)Data = v; return;
              case TYPE_Byte: *(vuint8 *)Data = v; return;
              case TYPE_Bool:
                if (v) {
                  *(int *)Data |= Type.BitMask;
                } else {
                  *(int *)Data &= ~Type.BitMask;
                }
                return;
              case TYPE_Float: *(float *)Data = v; return;
            }
            break;
          }
        case TYPE_Byte:
          {
            vuint8 v;
            Strm << v;
            switch (Type.Type) {
              case TYPE_Int: *(vint32 *)Data = v; return;
              case TYPE_Byte: *(vuint8 *)Data = v; return;
              case TYPE_Bool:
                if (v) {
                  *(int *)Data |= Type.BitMask;
                } else {
                  *(int *)Data &= ~Type.BitMask;
                }
                return;
              case TYPE_Float: *(float *)Data = v; return;
            }
            break;
          }
        case TYPE_Float:
          {
            float v;
            Strm << v;
            switch (Type.Type) {
              case TYPE_Int: *(vint32 *)Data = (vuint32)v; return;
              case TYPE_Byte: *(vuint8 *)Data = (vuint8)v; return;
              case TYPE_Bool:
                if (v) {
                  *(int *)Data |= Type.BitMask;
                } else {
                  *(int *)Data &= ~Type.BitMask;
                }
                return;
              case TYPE_Float: *(float *)Data = v; return;
            }
            break;
          }
        case TYPE_Bool:
          {
            vuint8 v;
            Strm << v;
            v = (v ? 1 : 0);
            switch (Type.Type) {
              case TYPE_Int: *(vint32 *)Data = (vuint32)v; return;
              case TYPE_Byte: *(vuint8 *)Data = (vuint8)v; return;
              case TYPE_Bool:
                if (v) {
                  *(int *)Data |= Type.BitMask;
                } else {
                  *(int *)Data &= ~Type.BitMask;
                }
                return;
              case TYPE_Float: *(float *)Data = v; return;
            }
            break;
          }
      }
      //Sys_Error("I/O Error: field '%s' should be of type '%s', but it is of type '%s'", *fullname, *Type.GetName(), *VFieldType(EType(tp)).GetName());
      Host_Error("stored data should be of type `%s`, but it is of type `%s`", *Type.GetName(), *VFieldType(EType(tp)).GetName());
    }
  }
  VFieldType IntType;
  vint32 InnerSize;
  vint32 n;
  switch (Type.Type) {
    case TYPE_Int: Strm << *(vint32 *)Data; break;
    case TYPE_Byte: Strm << *Data; break;
    case TYPE_Bool:
      if (Strm.IsLoading()) {
        vuint8 Val;
        Strm << Val;
        if (Val) {
          *(int *)Data |= Type.BitMask;
        } else {
          *(int *)Data &= ~Type.BitMask;
        }
      } else {
        vuint8 Val = !!((*(int *)Data)&Type.BitMask);
        Strm << Val;
      }
      break;
    case TYPE_Float: Strm << *(float *)Data; break;
    case TYPE_Vector: Strm << *(TVec *)Data; break;
    case TYPE_Name: Strm << *(VName *)Data; break;
    case TYPE_String: Strm << *(VStr *)Data; break;
    case TYPE_Pointer:
      tp = Type.InnerType;
      Strm << tp;
      if (Type.InnerType == TYPE_Struct) {
        Strm.SerialiseStructPointer(*(void **)Data, Type.Struct);
      } else {
        GLog.WriteLine(NAME_Warning, "I/O WARNING: don't know how to serialise pointer type `%s`", *Type.GetName());
        /*
        if (Strm.IsLoading()) {
          //Strm << *(int *)Data;
          *(void **)Data = nullptr;
        } else {
          GLog.Log(NAME_Dev, "I/O Error: don't know how to serialise pointer type `%s`", *Type.GetName());
        }
        */
      }
      break;
    case TYPE_Reference: Strm << *(VObject **)Data; break;
    case TYPE_Class:
      if (Strm.IsLoading()) {
        VName CName;
        Strm << CName;
        if (CName != NAME_None) {
          *(VClass **)Data = VClass::FindClass(*CName);
        } else {
          *(VClass **)Data = nullptr;
        }
      } else {
        VName CName = NAME_None;
        if (*(VClass **)Data) CName = (*(VClass **)Data)->GetVName();
        Strm << CName;
      }
      break;
    case TYPE_State:
      if (Strm.IsLoading()) {
        VName CName;
        VName SName;
        Strm << CName << SName;
        if (SName != NAME_None && VStr::ICmp(*SName, "none") != 0) {
          //*(VState **)Data = VClass::FindClass(*CName)->FindStateChecked(SName);
          *(VState **)Data = VClass::FindClass(*CName)->FindState(SName);
          if (*(VState **)Data == nullptr) {
            GLog.WriteLine(NAME_Warning, "I/O: state '%s' not found", *SName);
            //Sys_Error("I/O WARNING: state '%s' not found in '%s'", *SName, *fullname);
          }
        } else {
          *(VState **)Data = nullptr;
        }
      } else {
        VName CName = NAME_None;
        VName SName = NAME_None;
        if (*(VState **)Data) {
          CName = (*(VState **)Data)->Outer->GetVName();
          SName = (*(VState **)Data)->Name;
        }
        Strm << CName << SName;
      }
      break;
    case TYPE_Delegate:
      Strm << ((VObjectDelegate *)Data)->Obj;
      if (Strm.IsLoading()) {
        VName FuncName;
        Strm << FuncName;
        if (((VObjectDelegate *)Data)->Obj) ((VObjectDelegate *)Data)->Func = ((VObjectDelegate *)Data)->Obj->GetVFunction(FuncName);
      } else {
        VName FuncName = NAME_None;
        if (((VObjectDelegate *)Data)->Obj) FuncName = ((VObjectDelegate *)Data)->Func->Name;
        Strm << FuncName;
      }
      break;
    case TYPE_Struct:
      if (Strm.IsLoading()) {
        // check struct name
        VName stname = NAME_None;
        Strm << stname;
        if (Type.Struct->Name != stname) Host_Error("I/O Error: expected struct `%s`, but got struct '%s'", *Type.Struct->Name, *stname);
      } else {
        // save struct name
        VName stname = Type.Struct->Name;
        Strm << stname;
      }
      Type.Struct->SerialiseObject(Strm, Data);
      break;
    case TYPE_Array:
      IntType = Type;
      IntType.Type = Type.ArrayInnerType;
      InnerSize = IntType.GetSize();
      n = Type.GetArrayDim();
      tp = IntType.Type;
      Strm << tp;
      Strm << STRM_INDEX(n);
      if (Strm.IsLoading()) {
        // check inner size
        vint32 isz = -1;
        Strm << STRM_INDEX(isz);
        if (tp != IntType.Type) Host_Error("I/O Error: invalid array element type, expected '%s', got '%s'", *IntType.GetName(), *VFieldType(EType(tp)).GetName());
        // meh, type serialiser will take care of this
        if (isz != /*InnerSize*/IntType.GetStackSize()) {
          GLog.WriteLine("I/O: invalid array element size, expected %d, got %d (this is mostly harmless)", /*InnerSize*/IntType.GetStackSize(), isz);
        }
        for (int i = 0; i < Type.GetArrayDim(); ++i) {
          if (i < n) {
            SerialiseFieldValue(Strm, Data+i*InnerSize, IntType);
          } else {
            SkipSerialisedValue(Strm);
          }
        }
      } else {
        vint32 isz = IntType.GetStackSize();
        Strm << STRM_INDEX(/*InnerSize*/isz);
        for (int i = 0; i < n; ++i) SerialiseFieldValue(Strm, Data+i*InnerSize, IntType);
      }
      break;
    case TYPE_DynamicArray:
      {
        VScriptArray &A = *(VScriptArray *)Data;
        IntType = Type;
        IntType.Type = Type.ArrayInnerType;
        InnerSize = IntType.GetSize();
        vint32 ArrNum = A.Num();
        tp = IntType.Type;
        Strm << tp;
        Strm << STRM_INDEX(ArrNum);
        if (Strm.IsLoading()) {
          // check inner size
          vint32 isz = -1;
          Strm << STRM_INDEX(isz);
          if (tp != IntType.Type) Host_Error("I/O Error: invalid dynarray element type, expected '%s', got '%s'", *IntType.GetName(), *VFieldType(EType(tp)).GetName());
          // meh, type serialiser will take care of this
          if (isz != /*InnerSize*/IntType.GetStackSize()) {
            GLog.WriteLine("I/O: invalid array element size, expected %d, got %d (this is mostly harmless)", /*InnerSize*/IntType.GetStackSize(), isz);
          }
        } else {
          vint32 isz = IntType.GetStackSize();
          Strm << STRM_INDEX(/*InnerSize*/isz);
        }
        if (Strm.IsLoading()) A.SetNum(ArrNum, IntType);
        for (int i = 0; i < A.Num(); ++i) SerialiseFieldValue(Strm, A.Ptr()+i*InnerSize, IntType);
      }
      break;
    case TYPE_SliceArray:
      //FIXME:SLICE
      GLog.Logf(NAME_Dev, "Don't know how to serialise slice type `%s`", *Type.GetName());
      break;
    case TYPE_Dictionary:
      {
        VScriptDict *dc = (VScriptDict *)Data;
        VFieldType t = Type;
        Strm << t;
        if (Strm.IsLoading()) {
          if (!t.Equals(Type)) Host_Error("I/O Error: expected dictionary of type `%s`, but got `%s`", *Type.GetName(), *t.GetName());
        }
        dc->Serialise(Strm, Type);
      }
      break;
    default:
      Host_Error("I/O Error: unknown data type");
  }
}


//==========================================================================
//
//  VField::NeedToCleanField
//
//==========================================================================
bool VField::NeedToCleanField (const VFieldType &Type) {
  VFieldType IntType;
  switch (Type.Type) {
    case TYPE_Reference:
    case TYPE_Delegate:
    case TYPE_Dictionary:
      return true;
    case TYPE_Struct:
      return Type.Struct->NeedToCleanObject();
    case TYPE_Array:
    case TYPE_DynamicArray:
      IntType = Type;
      IntType.Type = Type.ArrayInnerType;
      return NeedToCleanField(IntType);
  }
  return false;
}


//==========================================================================
//
//  VField::CleanField
//
//==========================================================================
bool VField::CleanField (vuint8 *Data, const VFieldType &Type) {
  VFieldType IntType;
  int InnerSize;
  bool res = false;
  switch (Type.Type) {
    case TYPE_Reference:
      if (*(VObject **)Data && ((*(VObject **)Data)->GetFlags()&_OF_CleanupRef) != 0) {
        *(VObject **)Data = nullptr;
        res = true;
      }
      break;
    case TYPE_Delegate:
      if (((VObjectDelegate *)Data)->Obj && (((VObjectDelegate *)Data)->Obj->GetFlags()&_OF_CleanupRef) != 0) {
        ((VObjectDelegate *)Data)->Obj = nullptr;
        ((VObjectDelegate *)Data)->Func = nullptr;
        res = true;
      }
      break;
    case TYPE_Struct:
      return Type.Struct->CleanObject(Data);
    case TYPE_Array:
      IntType = Type;
      IntType.Type = Type.ArrayInnerType;
      if (NeedToCleanField(IntType)) {
        InnerSize = IntType.GetSize();
        for (int i = 0; i < Type.GetArrayDim(); ++i) {
          if (CleanField(Data+i*InnerSize, IntType)) res = true;
        }
      }
      break;
    case TYPE_DynamicArray:
      {
        VScriptArray &A = *(VScriptArray *)Data;
        IntType = Type;
        IntType.Type = Type.ArrayInnerType;
        if (NeedToCleanField(IntType)) {
          InnerSize = IntType.GetSize();
          for (int i = 0; i < A.Num(); ++i) {
            if (CleanField(A.Ptr()+i*InnerSize, IntType)) res = true;
          }
        }
      }
      break;
    case TYPE_Dictionary:
      {
        VScriptDict *dc = (VScriptDict *)Data;
        res = dc->cleanRefs();
      }
      break;
  }
  return res;
}


//==========================================================================
//
//  VField::NeedToDestructField
//
//==========================================================================
bool VField::NeedToDestructField (const VFieldType &atype) {
  return NeedDtor(atype);
}


//==========================================================================
//
//  VField::DestructField
//
//==========================================================================
void VField::DestructField (vuint8 *Data, const VFieldType &Type, bool zeroIt) {
  //if (zeroIt) fprintf(stderr, "***ZERO<%s>: %d\n", *Type.GetName(), (int)NeedDtor(Type));
  if (zeroIt && !NeedDtor(Type)) {
    //fprintf(stderr, "   ZEROED (%d)!\n", Type.GetSize());
    memset(Data, 0, Type.GetSize());
    return;
  }
  VFieldType IntType;
  int InnerSize;
  switch (Type.Type) {
    case TYPE_Reference:
    case TYPE_Pointer:
      *(void **)Data = nullptr;
      break;
    case TYPE_String:
      ((VStr *)Data)->Clean();
      break;
    case TYPE_Struct:
      if (!zeroIt) Type.Struct->DestructObject(Data); else Type.Struct->ZeroObject(Data);
      break;
    case TYPE_Array:
      IntType = Type;
      IntType.Type = Type.ArrayInnerType;
      if (NeedDtor(IntType)) {
        InnerSize = IntType.GetSize();
        for (int i = 0; i < Type.GetArrayDim(); ++i) DestructField(Data+i*InnerSize, IntType, zeroIt);
      } else if (zeroIt && Type.GetArrayDim()) {
        InnerSize = IntType.GetSize();
        memset(Data, 0, Type.GetArrayDim()*InnerSize);
      }
      break;
    case TYPE_DynamicArray:
      IntType = Type;
      IntType.Type = Type.ArrayInnerType;
      ((VScriptArray *)Data)->Clear(IntType);
      break;
    case TYPE_SliceArray:
      memset(Data, 0, Type.GetSize());
      break;
    case TYPE_Dictionary:
      {
        VScriptDict *dc = (VScriptDict *)Data;
        dc->clear();
      }
      break;
  }
}


//==========================================================================
//
//  VField::IdenticalValue
//
//==========================================================================
bool VField::IdenticalValue (const vuint8 *Val1, const vuint8 *Val2, const VFieldType &Type) {
  if (Val1 == Val2) return true; // nothing to do
  VFieldType IntType;
  int InnerSize;
  switch (Type.Type) {
    case TYPE_Int: return (*(const vint32 *)Val1 == *(const vint32 *)Val2);
    case TYPE_Byte: return (*(const vuint8 *)Val1 == *(const vuint8 *)Val2);
    case TYPE_Bool: return (((*(const vuint32 *)Val1)&Type.BitMask) == ((*(const vuint32 *)Val2)&Type.BitMask));
    case TYPE_Float: return (*(const float *)Val1 == *(const float *)Val2);
    case TYPE_Vector: return (*(const TVec *)Val1 == *(const TVec *)Val2);
    case TYPE_Name: return (*(const VName *)Val1 == *(const VName *)Val2);
    case TYPE_String: return (*(const VStr *)Val1 == *(const VStr *)Val2);
    case TYPE_Pointer: return (*(void * const *)Val1 == *(void * const *)Val2);
    case TYPE_Reference: return (*(VObject * const *)Val1 == *(VObject * const *)Val2);
    case TYPE_Class: return (*(VClass * const *)Val1 == *(VClass * const *)Val2);
    case TYPE_State: return (*(VState * const *)Val1 == *(VState * const *)Val2);
    case TYPE_Delegate:
      return ((const VObjectDelegate *)Val1)->Obj == ((const VObjectDelegate *)Val2)->Obj &&
             ((const VObjectDelegate *)Val1)->Func == ((const VObjectDelegate *)Val2)->Func;
    case TYPE_Struct: return Type.Struct->IdenticalObject(Val1, Val2);
    case TYPE_Array:
      IntType = Type;
      IntType.Type = Type.ArrayInnerType;
      InnerSize = IntType.GetSize();
      for (int i = 0; i < Type.GetArrayDim(); ++i) {
        if (!IdenticalValue(Val1+i*InnerSize, Val2+i*InnerSize, IntType)) return false;
      }
      return true;
    case TYPE_DynamicArray:
      {
        VScriptArray &Arr1 = *(VScriptArray *)Val1;
        VScriptArray &Arr2 = *(VScriptArray *)Val2;
        if (Arr1.Num() != Arr2.Num()) return false;
        IntType = Type;
        IntType.Type = Type.ArrayInnerType;
        InnerSize = IntType.GetSize();
        for (int i = 0; i < Type.GetArrayDim(); ++i) {
          if (!IdenticalValue(Arr1.Ptr()+i*InnerSize, Arr2.Ptr()+i*InnerSize, IntType)) return false;
        }
      }
      return true;
    case TYPE_SliceArray:
      return (memcmp(Val1, Val2, Type.GetSize()) == 0);
    case TYPE_Dictionary:
      {
        const VScriptDict *v1 = (const VScriptDict *)Val1;
        const VScriptDict *v2 = (const VScriptDict *)Val2;
        return (v1->map == v2->map);
      }
      break;
  }
  Sys_Error("Bad field type");
  return false;
}


//==========================================================================
//
//  angle2word
//
//==========================================================================
static vuint16 angle2word (float angle) {
  if (angle) {
    angle = fmodf(angle, 360.0f); // signed
    vuint16 v = (vuint16)((double)fabsf(angle)/360.0*32767.0);
    v <<= 1;
    if (v && angle < 0) v |= 1u;
    return v;
  } else {
    return 0;
  }
}


//==========================================================================
//
//  word2angle
//
//==========================================================================
static float word2angle (vuint16 angle) {
  if (angle) {
    const vuint8 sign = angle&1u;
    angle >>= 1;
    float res = (float)((double)angle/32767.0*360.0);
    return (sign ? -res : res);
  } else {
    return 0;
  }
}


//==========================================================================
//
//  VField::NetSerialiseValue
//
//  if `vecprecise` is false, use 16 bits for coords and for angles
//
//==========================================================================
bool VField::NetSerialiseValue (VStream &Strm, VNetObjectsMapBase *Map, vuint8 *Data, const VFieldType &Type, bool vecprecise) {
  VFieldType IntType;
  int InnerSize;
  bool Ret = true;
  switch (Type.Type) {
    case TYPE_Int: Strm << *(vint32 *)Data; break;
    case TYPE_Byte: Strm << *(vuint8 *)Data; break;
    case TYPE_Bool:
      if (Strm.IsLoading()) {
        vuint8 Val;
        Strm.SerialiseBits(&Val, 1);
        if (Val) {
          *(vuint32 *)Data |= Type.BitMask;
        } else {
          *(vuint32 *)Data &= ~Type.BitMask;
        }
      } else {
        vuint8 Val = ((*(vuint32 *)Data)&Type.BitMask ? 1 : 0);
        Strm.SerialiseBits(&Val, 1);
      }
      break;
    case TYPE_Float: Strm << *(float *)Data; break;
    case TYPE_Name: Ret = Map->SerialiseName(Strm, *(VName *)Data); break;
    case TYPE_Vector:
      if (Type.Struct->Name == NAME_TAVec) {
        TAVec *ang = (TAVec *)Data;
        if (Strm.IsLoading()) {
          // load angles
          vuint8 HavePitchRoll = 0;
          Strm.SerialiseBits(&HavePitchRoll, 3);
          if (HavePitchRoll&4u) {
            // precise
            Strm << ang->yaw;
            if (HavePitchRoll&1u) Strm << ang->pitch; else ang->pitch = 0.0f;
            if (HavePitchRoll&2u) Strm << ang->roll; else ang->roll = 0.0f;
          } else {
            // imprecise
            vuint16 wyaw = 0;
            vuint16 wpitch = 0;
            vuint16 wroll = 0;
            Strm << wyaw;
            if (HavePitchRoll&1u) Strm << wpitch;
            if (HavePitchRoll&2u) Strm << wroll;
            ang->yaw = word2angle(wyaw);
            ang->pitch = word2angle(wpitch);
            ang->roll = word2angle(wroll);
          }
        } else {
          // save angles
          if (vecprecise) {
            vuint8 HavePitchRoll =
              (ang->pitch ? 1u : 0u)|
              (ang->roll ? 2u : 0u)|
              4u; // precision
            Strm.SerialiseBits(&HavePitchRoll, 3);
            Strm << ang->yaw;
            if (HavePitchRoll&1u) Strm << ang->pitch;
            if (HavePitchRoll&2u) Strm << ang->roll;
          } else {
            vuint16 wyaw = angle2word(ang->yaw);
            vuint16 wpitch = angle2word(ang->pitch);
            vuint16 wroll = angle2word(ang->roll);
            vuint8 HavePitchRoll =
              (wpitch ? 1u : 0u)|
              (wroll ? 2u : 0u)|
              0u; // precision
            Strm.SerialiseBits(&HavePitchRoll, 3);
            Strm << wyaw;
            if (HavePitchRoll&1u) Strm << wpitch;
            if (HavePitchRoll&2u) Strm << wroll;
          }
        }
      } else {
        TVec *vec = (TVec *)Data;
        if (Strm.IsLoading()) {
          // load position
          vuint8 precision = 0;
          Strm.SerialiseBits(&precision, 1);
          if (precision) {
            Strm << vec->x << vec->y << vec->z;
          } else {
            vint16 x, y, z;
            Strm << x << y << z;
            vec->x = x;
            vec->y = y;
            vec->z = z;
          }
        } else {
          // save position
          if (vecprecise) {
            vuint8 precision = 1;
            Strm.SerialiseBits(&precision, 1);
            Strm << vec->x << vec->y << vec->z;
          } else {
            vuint8 precision = 0;
            Strm.SerialiseBits(&precision, 1);
            vint16 x = mround(vec->x);
            vint16 y = mround(vec->y);
            vint16 z = mround(vec->z);
            Strm << x << y << z;
          }
        }
      }
      break;
    case TYPE_String: Strm << *(VStr *)Data; break;
    case TYPE_Class: Ret = Map->SerialiseClass(Strm, *(VClass **)Data); break;
    case TYPE_State: Ret = Map->SerialiseState(Strm, *(VState **)Data); break;
    case TYPE_Reference: Ret = Map->SerialiseObject(Strm, *(VObject **)Data); break;
    case TYPE_Struct: Ret = Type.Struct->NetSerialiseObject(Strm, Map, Data); break;
    case TYPE_Array:
      IntType = Type;
      IntType.Type = Type.ArrayInnerType;
      InnerSize = IntType.GetSize();
      for (int i = 0; i < Type.GetArrayDim(); ++i) {
        if (!NetSerialiseValue(Strm, Map, Data+i*InnerSize, IntType)) Ret = false;
      }
      break;
    //TODO: dynarrays, slices?
    default:
      Sys_Error("Replication of field type %d is not supported", Type.Type);
  }
  return Ret;
}
