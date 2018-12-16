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
/*
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
*/

#include "vc_local.h"


//==========================================================================
//
//  VFieldType::VFieldType
//
//==========================================================================
VFieldType::VFieldType()
  : Type(TYPE_Void)
  , InnerType(TYPE_Void)
  , ArrayInnerType(TYPE_Void)
  , PtrLevel(0)
  , ArrayDimInternal(0)
  , Class(0)
{
}


//==========================================================================
//
//  VFieldType::VFieldType
//
//==========================================================================
VFieldType::VFieldType(EType Atype)
  : Type(Atype)
  , InnerType(TYPE_Void)
  , ArrayInnerType(TYPE_Void)
  , PtrLevel(0)
  , ArrayDimInternal(0)
  , Class(0)
{
}


//==========================================================================
//
//  VFieldType::VFieldType
//
//==========================================================================
VFieldType::VFieldType (VClass *InClass)
  : Type(TYPE_Reference)
  , InnerType(TYPE_Void)
  , ArrayInnerType(TYPE_Void)
  , PtrLevel(0)
  , ArrayDimInternal(0)
  , Class(InClass)
{
}


//==========================================================================
//
//  VFieldType::VFieldType
//
//==========================================================================
VFieldType::VFieldType (VStruct *InStruct)
  : Type(InStruct->IsVector ? TYPE_Vector : TYPE_Struct)
  , InnerType(TYPE_Void)
  , ArrayInnerType(TYPE_Void)
  , PtrLevel(0)
  , ArrayDimInternal(0)
  , Struct(InStruct)
{
}


//==========================================================================
//
//  operator VStream << FType
//
//==========================================================================
VStream &operator << (VStream &Strm, VFieldType &T) {
  guard(operator VStream << VFieldType);
  Strm << T.Type;
  vuint8 RealType = T.Type;
  if (RealType == TYPE_Array) {
    Strm << T.ArrayInnerType << STRM_INDEX(T.ArrayDimInternal);
    RealType = T.ArrayInnerType;
  } else if (RealType == TYPE_DynamicArray || RealType == TYPE_SliceArray || RealType == TYPE_Dictionary) {
    Strm << T.ArrayInnerType;
    RealType = T.ArrayInnerType;
  }
  if (RealType == TYPE_Pointer) {
    Strm << T.InnerType << T.PtrLevel;
    RealType = T.InnerType;
  }
       if (RealType == TYPE_Reference || RealType == TYPE_Class) Strm << T.Class;
  else if (RealType == TYPE_Struct || RealType == TYPE_Vector) Strm << T.Struct;
  else if (RealType == TYPE_Delegate) Strm << T.Function;
  else if (RealType == TYPE_Bool) Strm << T.BitMask;
  return Strm;
  unguard;
}


//==========================================================================
//
//  VFieldType::Equals
//
//==========================================================================
bool VFieldType::Equals (const VFieldType &Other) const {
  guardSlow(VFieldType::Equals);
  if (Type != Other.Type ||
      InnerType != Other.InnerType ||
      ArrayInnerType != Other.ArrayInnerType ||
      PtrLevel != Other.PtrLevel ||
      ArrayDimInternal != Other.ArrayDimInternal ||
      Class != Other.Class)
  {
    return false;
  }
  return true;
  unguardSlow;
}


//==========================================================================
//
//  VFieldType::IsCompatiblePointerRelaxed
//
//==========================================================================
bool VFieldType::IsCompatiblePointerRelaxed (const VFieldType &other) const {
  if (!IsPointer() || !other.IsPointer()) return false;
  // void is compatible with anything
  if (IsVoidPointer() || other.IsVoidPointer()) return true;
  if (InnerType == TYPE_Struct) {
    if (other.InnerType != TYPE_Struct) return false;
    return (other.Struct->IsA(Struct) || Struct->IsA(other.Struct));
  }
  if (InnerType == TYPE_Class) return (other.InnerType == TYPE_Class); // any two are ok, why not?
  if (InnerType == TYPE_Reference) return (other.InnerType == TYPE_Reference); // any two are ok, why not?
  return Equals(other);
}


//==========================================================================
//
//  VFieldType::MakePointerType
//
//==========================================================================
VFieldType VFieldType::MakePointerType () const {
  guard(VFieldType::MakePointerType);
  VFieldType pointer = *this;
  if (pointer.Type == TYPE_Pointer) {
    ++pointer.PtrLevel;
  } else {
    pointer.InnerType = pointer.Type;
    pointer.Type = TYPE_Pointer;
    pointer.PtrLevel = 1;
  }
  return pointer;
  unguard;
}


//==========================================================================
//
//  VFieldType::GetPointerInnerType
//
//==========================================================================
VFieldType VFieldType::GetPointerInnerType () const {
  guard(VFieldType::GetPointerInnerType);
  if (Type != TYPE_Pointer) {
    FatalError("Not a pointer type");
    return *this;
  }
  VFieldType ret = *this;
  --ret.PtrLevel;
  if (ret.PtrLevel <= 0) {
    ret.Type = InnerType;
    ret.InnerType = TYPE_Void;
  }
  return ret;
  unguard;
}


//==========================================================================
//
//  VFieldType::MakeArrayType
//
//==========================================================================
VFieldType VFieldType::MakeArrayType (int elcount, const TLocation &l) const {
  guard(VFieldType::MakeArrayType);
  if (IsAnyArray()) ParseError(l, "Can't have multi-dimensional arrays");
  if (elcount < 0) ParseError(l, "Can't have arrays with negative size");
  VFieldType array = *this;
  array.ArrayInnerType = Type;
  array.Type = TYPE_Array;
  array.ArrayDimInternal = elcount;
  return array;
  unguard;
}


//==========================================================================
//
//  VFieldType::MakeArray2DType
//
//==========================================================================
VFieldType VFieldType::MakeArray2DType (int d0, int d1, const TLocation &l) const {
  guard(VFieldType::MakeArray2DType);
  if (IsAnyArray()) ParseError(l, "Can't have multi-dimensional 2d arrays");
  if (d0 < 0 || d1 < 0) ParseError(l, "Can't have 2d arrays with negative size");
  if (d0 <= 0 || d1 <= 0) ParseError(l, "Can't have 2d arrays with zero size");
  if (d0 > 0x7fff || d1 > 0x7fff) ParseError(l, "Can't have 2d arrays with dimensions more than 32767");
  VFieldType array = *this;
  array.ArrayInnerType = Type;
  array.Type = TYPE_Array;
  array.ArrayDimInternal = (d0|(d1<<16))|0x80000000;
  return array;
  unguard;
}


//==========================================================================
//
//  VFieldType::MakeDynamicArrayType
//
//==========================================================================
VFieldType VFieldType::MakeDynamicArrayType (const TLocation &l) const {
  guard(VFieldType::MakeDynamicArrayType);
  if (IsAnyArray()) ParseError(l, "Can't have multi-dimensional arrays");
  VFieldType array = *this;
  array.ArrayInnerType = Type;
  array.Type = TYPE_DynamicArray;
  return array;
  unguard;
}


//==========================================================================
//
//  VFieldType::MakeSliceType
//
//==========================================================================
VFieldType VFieldType::MakeSliceType (const TLocation &l) const {
  guard(VFieldType::MakeSliceType);
  if (IsAnyArray()) ParseError(l, "Can't have multi-dimensional slices or slices of arrays");
  VFieldType array = *this;
  array.ArrayInnerType = Type;
  array.Type = TYPE_SliceArray;
  return array;
  unguard;
}


//==========================================================================
//
//  VFieldType::GetArrayInnerType
//
//==========================================================================
VFieldType VFieldType::GetArrayInnerType () const {
  guard(VFieldType::GetArrayInnerType);
  if (Type != TYPE_Array && Type != TYPE_DynamicArray && Type != TYPE_SliceArray) {
    FatalError("Not an array type");
    return *this;
  }
  VFieldType ret = *this;
  ret.Type = ArrayInnerType;
  ret.ArrayInnerType = TYPE_Void;
  ret.ArrayDimInternal = 0;
  return ret;
  unguard;
}


//==========================================================================
//
//  VFieldType::GetDictKeyType
//
//==========================================================================
VFieldType VFieldType::GetDictKeyType () const {
  guard(VFieldType::GetDictKeyType);
  if (Type != TYPE_Dictionary) {
    FatalError("Not a dictionary type");
    return *this;
  }
  VFieldType ret = *this;
  ret.Type = InnerType;
  ret.InnerType = TYPE_Void;
  ret.ArrayInnerType = TYPE_Void;
  ret.ArrayDimInternal = 0;
  return ret;
  unguard;
}


//==========================================================================
//
//  VFieldType::GetDictValueType
//
//==========================================================================
VFieldType VFieldType::GetDictValueType () const {
  guard(VFieldType::GetDictValueType);
  if (Type != TYPE_Dictionary) {
    FatalError("Not a dictionary type");
    return *this;
  }
  VFieldType ret = *this;
  ret.Type = ArrayInnerType;
  ret.InnerType = TYPE_Void;
  ret.ArrayInnerType = TYPE_Void;
  ret.ArrayDimInternal = 0;
  return ret;
  unguard;
}


//==========================================================================
//
//  VFieldType::GetStackSize
//
//==========================================================================
int VFieldType::GetStackSize () const {
  guard(VFieldType::GetStackSize);
  switch (Type) {
    case TYPE_Int: return 4;
    case TYPE_Byte: return 4;
    case TYPE_Bool: return 4;
    case TYPE_Float: return 4;
    case TYPE_Name: return 4;
    case TYPE_String: return 4;
    case TYPE_Pointer: return 4;
    case TYPE_Reference: return 4;
    case TYPE_Class: return 4;
    case TYPE_State: return 4;
    case TYPE_Delegate: return 2*4; // self, funcptr
    case TYPE_Struct: return Struct->StackSize*4;
    case TYPE_Vector: return 3*4; // 3 floats
    case TYPE_Array: return GetArrayDim()*GetArrayInnerType().GetStackSize();
    case TYPE_SliceArray: return 2*4; // ptr and length
    case TYPE_DynamicArray: return 3*4; // 3 fields in VScriptArray
    case TYPE_Dictionary: return 4; // VScriptDict is just a pointer to the underlying implementation class
  }
  return 0;
  unguard;
}


//==========================================================================
//
//  VFieldType::GetSize
//
//==========================================================================
int VFieldType::GetSize () const {
  guard(VFieldType::GetSize);
  switch (Type) {
    case TYPE_Int: return sizeof(vint32);
    case TYPE_Byte: return sizeof(vuint8);
    case TYPE_Bool: return sizeof(vuint32);
    case TYPE_Float: return sizeof(float);
    case TYPE_Name: return sizeof(VName);
    case TYPE_String: return sizeof(VStr);
    case TYPE_Pointer: return sizeof(void *);
    case TYPE_Reference: return sizeof(VObject *);
    case TYPE_Class: return sizeof(VClass *);
    case TYPE_State: return sizeof(VState *);
    case TYPE_Delegate: return sizeof(VObjectDelegate);
    case TYPE_Struct: return (Struct->Size+3)&~3;
    case TYPE_Vector: return sizeof(TVec);
    case TYPE_Array: return GetArrayDim()*GetArrayInnerType().GetSize();
    case TYPE_SliceArray: return sizeof(void *)+sizeof(vint32); // ptr and length
    case TYPE_DynamicArray: return sizeof(VScriptArray);
    case TYPE_Dictionary: return sizeof(VScriptDict);
  }
  return 0;
  unguard;
}


//==========================================================================
//
//  VFieldType::GetAlignment
//
//==========================================================================
int VFieldType::GetAlignment () const {
  guard(VFieldType::GetAlignment);
  switch (Type) {
    case TYPE_Int: return sizeof(vint32);
    case TYPE_Byte: return sizeof(vuint8);
    case TYPE_Bool: return sizeof(vuint32);
    case TYPE_Float: return sizeof(float);
    case TYPE_Name: return sizeof(VName);
    case TYPE_String: return sizeof(char *);
    case TYPE_Pointer: return sizeof(void *);
    case TYPE_Reference: return sizeof(VObject *);
    case TYPE_Class: return sizeof(VClass *);
    case TYPE_State: return sizeof(VState *);
    case TYPE_Delegate: return sizeof(VObject *);
    case TYPE_Struct: return Struct->Alignment;
    case TYPE_Vector: return sizeof(float);
    case TYPE_Array: return GetArrayInnerType().GetAlignment();
    case TYPE_SliceArray: return sizeof(void *);
    case TYPE_DynamicArray: return sizeof(void *);
    case TYPE_Dictionary: return sizeof(void *);
  }
  return 0;
  unguard;
}


//==========================================================================
//
//  VFieldType::CheckPassable
//
//  Check, if type can be pushed into the stack
//
//==========================================================================
bool VFieldType::CheckPassable (const TLocation &l, bool raiseError) const {
  guardSlow(VFieldType::CheckPassable);
  if (GetStackSize() != 4 && Type != TYPE_Vector && Type != TYPE_Delegate && Type != TYPE_SliceArray) {
    if (raiseError) ParseError(l, "Type `%s` is not passable", *GetName());
    return false;
  }
  return true;
  unguardSlow;
}


//==========================================================================
//
//  VFieldType::CheckReturnable
//
//  Check, if type can be pushed into the stack
//
//==========================================================================
bool VFieldType::CheckReturnable (const TLocation &l, bool raiseError) const {
  guardSlow(VFieldType::CheckReturnable);
  if (GetStackSize() != 4 && Type != TYPE_Vector) {
    if (raiseError) ParseError(l, "Type `%s` is not returnable", *GetName());
    return false;
  }
  return true;
  unguardSlow;
}


//==========================================================================
//
//  VFieldType::CheckMatch
//
//  Check, if types are compatible
//
//  t1 - current type
//  t2 - needed type
//
//==========================================================================
bool VFieldType::CheckMatch (bool asRef, const TLocation &l, const VFieldType &Other, bool raiseError) const {
  guard(VFieldType::CheckMatch);
  if (!asRef) {
    if (!CheckPassable(l, raiseError)) return false;
    if (!Other.CheckPassable(l, raiseError)) return false;
  } else {
    // check if `this` is a substruct of `Other`
    if (Type == TYPE_Struct && Other.Type == TYPE_Struct) {
      /*
      if (!Struct || !Other.Struct) {
        if (raiseError) ParseError(l, "struct `%s` is not a substruct of `%s`", *GetName(), *Other.GetName());
        return false;
      }
      */
      if (!Struct->IsA(Other.Struct)) {
        if (raiseError) ParseError(l, "struct `%s` is not a substruct of `%s`", *GetName(), *Other.GetName());
        return false;
      }
      return true;
    }
  }

  if (Equals(Other)) return true;

  if (Type == TYPE_String && Other.Type == TYPE_String) return true;
  if (Type == TYPE_Vector && Other.Type == TYPE_Vector) return true;

  if (Type == TYPE_Pointer && Other.Type == TYPE_Pointer) {
    VFieldType it1 = GetPointerInnerType();
    VFieldType it2 = Other.GetPointerInnerType();
    if (it1.Equals(it2)) return true;
    if (it1.Type == TYPE_Void || it2.Type == TYPE_Void) return true;
    if (it1.Type == TYPE_Struct && it2.Type == TYPE_Struct) {
      if (!it1.Struct->IsA(it2.Struct)) {
        if (raiseError) ParseError(l, "struct `%s` is not a substruct of `%s`", *GetName(), *Other.GetName());
        return false;
      }
      return true;
    }
  }

  if (Type == TYPE_Reference && Other.Type == TYPE_Reference) {
    VClass *c1 = Class;
    VClass *c2 = Other.Class;
    if (!c1 || !c2) return true; // none reference can be assigned to any reference
    if (c1 == c2) return true;
    for (VClass *pc1 = c1->ParentClass; pc1; pc1 = pc1->ParentClass) if (pc1 == c2) return true;
  }

  if (Type == TYPE_Class && Other.Type == TYPE_Class) {
    VClass *c1 = Class;
    VClass *c2 = Other.Class;
    if (!c2) return true; // can assign any class type to generic class type
    if (c1 == c2) return true;
    if (c1) {
      for (VClass *pc1 = c1->ParentClass; pc1; pc1 = pc1->ParentClass) if (pc1 == c2) return true;
    }
  }

  if (Type == TYPE_Int && Other.Type == TYPE_Byte) return true;
  if (Type == TYPE_Int && Other.Type == TYPE_Bool) return true;

  // allow assigning none to states, classes and delegates
  if (Type == TYPE_Reference && Class == nullptr &&
      (Other.Type == TYPE_Class || Other.Type == TYPE_State || Other.Type == TYPE_Delegate))
  {
    return true;
  }

  bool result = true;

  if (Type == TYPE_Delegate && Other.Type == TYPE_Delegate) {
    if (!Function || !Other.Function) return true; // one or both are `none delegate`
    VMethod &F1 = *Function;
    VMethod &F2 = *Other.Function;
    if ((F1.Flags&FUNC_Static) != 0 || (F2.Flags&FUNC_Static) != 0) {
      result = false;
      if (raiseError) ParseError(l, "Can't assign a static function to delegate");
    }
    if (!F1.ReturnType.Equals(F2.ReturnType)) {
      result = false;
      if (raiseError) ParseError(l, "Delegate has different return type (got '%s', expected '%s')", *F1.ReturnType.GetName(), *F2.ReturnType.GetName());
    } else if (F1.NumParams != F2.NumParams) {
      result = false;
      if (raiseError) ParseError(l, "Delegate has different number of arguments");
    } else for (int i = 0; i < F1.NumParams; ++i) {
      if (!F1.ParamTypes[i].Equals(F2.ParamTypes[i])) {
        result = false;
        if (raiseError) ParseError(l, "Delegate argument %d differs", i+1);
      }
    }
    return result;
  }

  if (raiseError) {
    ParseError(l, "Type mismatch, types `%s` and `%s` are not compatible", *GetName(), *Other.GetName());
  }

  return false;
  unguard;
}


//==========================================================================
//
//  VFieldType::GetName
//
//==========================================================================
VStr VFieldType::GetName () const {
  guard(VFieldType::GetName);
  VStr Ret;
  switch (Type) {
    case TYPE_Void: return "void";
    case TYPE_Int: return "int";
    case TYPE_Byte: return "byte";
    case TYPE_Bool: return "bool";
    case TYPE_Float: return "float";
    case TYPE_Name: return "name";
    case TYPE_String: return "string";
    case TYPE_Pointer:
      Ret = GetPointerInnerType().GetName();
      for (int i = 0; i < PtrLevel; ++i) Ret += "*";
      return Ret;
    case TYPE_Reference: return (Class ? *Class->Name : "none");
    case TYPE_Class:
      Ret = "class";
      if (Class) { Ret += "!"; Ret += *Class->Name; }
      return Ret;
    case TYPE_State: return "state";
    case TYPE_Struct: return *Struct->Name;
    case TYPE_Vector: return "vector";
    case TYPE_Array:
      if (ArrayDimInternal < 0) {
        // two-dimensional
        return GetArrayInnerType().GetName()+"["+VStr(GetFirstDim())+", "+VStr(GetSecondDim())+"]";
      } else {
        return GetArrayInnerType().GetName()+"["+VStr(ArrayDimInternal)+"]";
      }
    case TYPE_DynamicArray:
      Ret = GetArrayInnerType().GetName();
      return (Ret.IndexOf('*') < 0 ? VStr("array!")+Ret : VStr("array!(")+Ret+")");
    case TYPE_SliceArray: return GetArrayInnerType().GetName()+"[]";
    case TYPE_Dictionary:
      return VStr("dictionary!(")+GetDictKeyType().GetName()+","+GetDictValueType().GetName()+")";
    case TYPE_Automatic: return "auto";
    case TYPE_Delegate: return "delegate";
    default: return VStr("unknown:")+VStr((vuint32)Type);
  }
  unguard;
}


//==========================================================================
//
//  VFieldType::IsAnyArray
//
//==========================================================================
bool VFieldType::IsAnyArray () const {
  return (Type == TYPE_Array || Type == TYPE_DynamicArray ||
          Type == TYPE_SliceArray || Type == TYPE_Dictionary);
}


//==========================================================================
//
//  VFieldType::CanBeReplaced
//
//==========================================================================
bool VFieldType::IsReusingDisabled () const {
  if (PtrLevel > 0) return false; // pointers are ok
  switch (Type) {
    // simple types
    case TYPE_Void:
    case TYPE_Bool: // don't replace boolean vars
      return true;
    case TYPE_Int:
    case TYPE_Byte:
    case TYPE_Float:
    case TYPE_Name:
    case TYPE_Pointer:
    case TYPE_State:
    case TYPE_Vector:
    case TYPE_Class: // classes has no dtors
    case TYPE_Reference: // reference is something like a pointer
    case TYPE_SliceArray: // slices require no dtors as of yet
      return false;
    case TYPE_Delegate: // delegates need no dtor (yet)
      return true;
    case TYPE_Struct: // struct members can require dtors
      //!if (!Struct) return true; // let's play safe
      //!return Struct->NeedsDestructor();
      return true;
    case TYPE_String: // strings require dtors
      return true;
    case TYPE_Array: // it depends of inner type, so check it
      if (ArrayInnerType == TYPE_Struct) {
        //!if (!Struct) return true; // let's play safe
        //!return Struct->NeedsDestructor();
        return true;
      }
      //return (ArrayInnerType == TYPE_String || ArrayInnerType == TYPE_Array || ArrayInnerType == TYPE_DynamicArray);
      return !(ArrayInnerType == TYPE_Int || ArrayInnerType == TYPE_Float || ArrayInnerType == TYPE_Name);
    case TYPE_DynamicArray: // dynamic arrays should be cleared with dtors
    case TYPE_Dictionary: // dictionaries should be cleared with dtors
    case TYPE_Automatic: // this is something that should not be, so let's play safe
      return true;
    default:
      break;
  }
  return true; // just in case i forgot something
}


//==========================================================================
//
//  VFieldType::IsReplacableWith
//
//==========================================================================
bool VFieldType::IsReplacableWith (const VFieldType &atype) const {
  // same types are always replaceable
  if (Equals(atype) && !IsReusingDisabled()) return true;
  // don't change types
  return false;
}


// ////////////////////////////////////////////////////////////////////////// //
// VScriptArray
// ////////////////////////////////////////////////////////////////////////// //


#if !defined(IN_VCC)

//==========================================================================
//
//  VScriptArray::VScriptArray
//
//==========================================================================
VScriptArray::VScriptArray (const TArray<VStr> &xarr) {
  ArrData = nullptr;
  ArrNum = 0;
  ArrSize = 0;
  if (xarr.Num()) {
    size_t bytesize = xarr.Num()*sizeof(VStr);
    //ArrData = new vuint8[bytesize];
    ArrData = (vuint8 *)Z_Malloc(bytesize);
    memset(ArrData, 0, bytesize);
    VStr **aa = (VStr **)ArrData;
    for (int f = 0; f < xarr.Num(); ++f) *(VStr *)(&aa[f]) = xarr[f];
    ArrSize = ArrNum = xarr.Num();
  }
}


//==========================================================================
//
//  VScriptArray::Clear
//
//==========================================================================
void VScriptArray::Clear (const VFieldType &Type) {
  guard(VScriptArray::Clear);
  if (ArrData) {
    Flatten();
    // don't waste time destructing types without dtors
    if (VField::NeedToDestructField(Type)) {
      // no need to clear the whole array, as any resizes will zero out unused elements
      int InnerSize = Type.GetSize();
      for (int i = 0; i < ArrNum; ++i) VField::DestructField(ArrData+i*InnerSize, Type);
    }
    //delete[] ArrData;
    Z_Free(ArrData);
  }
  ArrData = nullptr;
  ArrNum = 0;
  ArrSize = 0;
  unguard;
}


//==========================================================================
//
//  VScriptArray::Reset
//
//==========================================================================
void VScriptArray::Reset (const VFieldType &Type) {
  guard(VScriptArray::Reset);
  if (ArrData) {
    Flatten();
    // don't waste time destructing types without dtors
    if (VField::NeedToDestructField(Type)) {
      // no need to clear the whole array, as any resizes will zero out unused elements
      int InnerSize = Type.GetSize();
      for (int i = 0; i < ArrNum; ++i) VField::DestructField(ArrData+i*InnerSize, Type);
    }
  }
  //fprintf(stderr, "VScriptArray::Reset: oldnum=%d; oldsize=%d\n", ArrNum, ArrSize);
  ArrNum = 0;
  unguard;
}


//==========================================================================
//
//  VScriptArray::Resize
//
//==========================================================================
void VScriptArray::Resize (int NewSize, const VFieldType &Type) {
  guard(VScriptArray::Resize);
  check(NewSize >= 0);

  if (NewSize <= 0) { Clear(Type); return; }

  Flatten(); // flatten 2d array (anyway)
  if (NewSize == ArrSize) return;

  //k8: this can be used interchangeably with `TArray()`, so use `Z_XXX()` here too
  //    also, moving elements with dtors can be dangerous, but we don't care
  //    FIXME: introduce `postblit()` for data types

  int InnerSize = Type.GetSize();
  vint32 oldSize = ArrSize;
  ArrSize = NewSize;

  if (ArrNum > NewSize) {
    if (VField::NeedToDestructField(Type)) {
      // clear old data
      for (int i = NewSize; i < ArrNum; ++i) VField::DestructField(ArrData+i*InnerSize, Type);
    }
    ArrNum = NewSize;
  }

  ArrData = (vuint8 *)Z_Realloc(ArrData, ArrSize*InnerSize);

  if (NewSize > oldSize) {
    // got some new elements, clear them
    memset(ArrData+oldSize*InnerSize, 0, (NewSize-oldSize)*InnerSize);
  }

#if 0
  vuint8 *OldData = ArrData;
  vint32 OldSize = ArrSize;
  vint32 oldlen = ArrNum;
  ArrSize = NewSize;
  if (ArrNum > NewSize) ArrNum = NewSize;

  int InnerSize = Type.GetSize();
  //ArrData = new vuint8[ArrSize*InnerSize];
  bool needDtor = VField::NeedToDestructField(Type);

  // coincidentally, simple copy is possible for everything that doesn't require destructing
  // for pod (data that doesn't need dtor), use realloc
  if (needDtor) {
    // realloc
    ArrData = (vuint8 *)Z_Realloc(ArrData, ArrSize*InnerSize);
    // clear new data
    if (ArrSize > OldSize) memset(ArrData+OldSize*InnerSize, 0, (ArrSize-OldSize)*InnerSize);
  } else {
    // alloc new buffer, and copy data
    ArrData = (vuint8 *)Z_Malloc(ArrSize*InnerSize);
    // clear new data, 'cause `VField::CopyFieldValue()` assume valid data
    memset(ArrData, 0, ArrSize*InnerSize);
    for (int i = 0; i < ArrNum; ++i) VField::CopyFieldValue(OldData+i*InnerSize, ArrData+i*InnerSize, Type);
    {
      // copy old data
      if (ArrNum > 0) memcpy(ArrData, OldData, ArrNum*InnerSize);
      // clear tail, so further growth will not hit random values
      if (ArrNum < ArrSize) memset(ArrData+ArrNum*InnerSize, 0, (ArrSize-ArrNum)*InnerSize);
    }

    if (OldData) {
      // don't waste time destructing types without dtors
      if (VField::NeedToDestructField(Type)) {
        // no need to clear the whole array, as any resizes will zero out unused elements
        for (int i = 0; i < oldlen; ++i) VField::DestructField(OldData+i*InnerSize, Type);
      }
      delete[] OldData;
    }
  }
#endif

  unguard;
}


//==========================================================================
//
//  VScriptArray::SetSize2D
//
//==========================================================================
void VScriptArray::SetSize2D (int dim1, int dim2, const VFieldType &Type) {
  if (dim1 <= 0 || dim2 <= 0) { Clear(Type); return; }
  // resize array to make exact match
  Resize(dim1*dim2, Type);
  // resize flattened it, convert to 2d
  ArrNum = dim1|0x80000000;
  ArrSize = dim2|0x80000000;
  if (!Is2D()) FatalError("VC: internal error in (VScriptArray::SetSize2D)");
}


//==========================================================================
//
//  VScriptArray::SetNum
//
//==========================================================================
void VScriptArray::SetNum (int NewNum, const VFieldType &Type, bool doShrink) {
  guard(VScriptArray::SetNum);
  check(NewNum >= 0);
  Flatten(); // flatten 2d array
  if (!doShrink && NewNum == 0) {
    if (ArrNum > 0) {
      // clear unused values (so possible array growth will not hit stale data, and strings won't hang it memory)
      int InnerSize = Type.GetSize();
      if (VField::NeedToDestructField(Type)) {
        for (int i = 0; i < ArrNum; ++i) VField::DestructField(ArrData+i*InnerSize, Type);
      } else {
        memset(ArrData, 0, ArrNum*InnerSize);
      }
      ArrNum = 0;
    }
    return;
  }
  // as a special case setting size to 0 should clear the array
       if (NewNum == 0) Clear(Type);
  else if (NewNum > ArrSize) Resize(NewNum+NewNum*3/8+32, Type); // resize will take care of cleanups
  else if (doShrink && ArrSize > 32 && NewNum > 32 && NewNum < ArrSize/3) Resize(NewNum+NewNum/3+8, Type); // resize will take care of cleanups
  else if (NewNum < ArrNum) {
    // clear unused values (so possible array growth will not hit stale data, and strings won't hang in memory)
    int InnerSize = Type.GetSize();
    if (VField::NeedToDestructField(Type)) {
      for (int i = NewNum; i < ArrNum; ++i) VField::DestructField(ArrData+i*InnerSize, Type);
    } else {
      memset(ArrData+NewNum*InnerSize, 0, (ArrNum-NewNum)*InnerSize);
    }
  }
  if (ArrSize < NewNum) FatalError("VC: internal error in (VScriptArray::SetNum)");
  ArrNum = NewNum;
  unguard;
}


//==========================================================================
//
//  VScriptArray::SetNumMinus
//
//==========================================================================
void VScriptArray::SetNumMinus (int NewNum, const VFieldType &Type) {
  guard(VScriptArray::SetNumMinus);
  Flatten(); // flatten 2d array
  if (NewNum <= 0) return;
  if (NewNum > ArrNum) NewNum = ArrNum;
  NewNum = ArrNum-NewNum;
  SetNum(NewNum, Type, false);
  unguard;
}


//==========================================================================
//
//  VScriptArray::SetNumPlus
//
//==========================================================================
void VScriptArray::SetNumPlus (int NewNum, const VFieldType &Type) {
  guard(VScriptArray::SetNumPlus);
  Flatten(); // flatten 2d array
  if (NewNum <= 0) return;
  if (ArrNum >= 0x3fffffff || 0x3fffffff-ArrNum < NewNum) FatalError("out of memory for dynarray");
  NewNum += ArrNum;
  SetNum(NewNum, Type, false);
  unguard;
}


//==========================================================================
//
//  VScriptArray
//
//==========================================================================
void VScriptArray::Insert (int Index, int Count, const VFieldType &Type) {
  guard(VScriptArray::Insert);
  Flatten(); // flatten 2d array
  //check(ArrData != nullptr);
  check(Index >= 0);
  check(Index <= ArrNum);

  if (Count <= 0) return;

  auto oldnum = ArrNum;
  SetNum(ArrNum+Count, Type, false); // don't shrink
  int InnerSize = Type.GetSize();
  // use simple copy if it is possible
  // coincidentally, simple copy is possible for everything that doesn't require destructing
  if (VField::NeedToDestructField(Type)) {
    // copy values to new location
    for (int i = ArrNum-1; i >= Index+Count; --i) VField::CopyFieldValue(ArrData+(i-Count)*InnerSize, ArrData+i*InnerSize, Type);
    // clean inserted elements
    for (int i = Index; i < Index+Count; ++i) VField::DestructField(ArrData+i*InnerSize, Type);
  } else {
    if (Index < oldnum) memmove(ArrData+(Index+Count)*InnerSize, ArrData+Index*InnerSize, (oldnum-Index)*InnerSize);
  }
  memset(ArrData+Index*InnerSize, 0, Count*InnerSize);
  unguard;
}


//==========================================================================
//
//  VScriptArray::Remove
//
//==========================================================================
void VScriptArray::Remove (int Index, int Count, const VFieldType &Type) {
  guard(VScriptArray::Remove);
  Flatten(); // flatten 2d array
  //check(ArrData != nullptr);
  check(Index >= 0);
  check(Index+Count <= ArrNum);

  auto oldnum = ArrNum;
  if (Count > oldnum) Count = oldnum; // just in case
  if (Count <= 0) return;

  if (Count == oldnum) {
    if (Index != 0) FatalError("VC: internal error 0 (VScriptArray::Remove)");
    // array is empty, so just clear it (but don't shrink)
    SetNum(0, Type, false);
    if (ArrNum != 0) FatalError("VC: internal error 1 (VScriptArray::Remove)");
  } else {
    // move elements that are after removed ones
    int InnerSize = Type.GetSize();
    // use simple copy if it is possible
    // coincidentally, simple copy is possible for everything that doesn't require destructing
    if (VField::NeedToDestructField(Type)) {
      for (int i = Index+Count; i < oldnum; ++i) VField::CopyFieldValue(ArrData+i*InnerSize, ArrData+(i-Count)*InnerSize, Type);
    } else {
      if (Index+Count < oldnum) memmove(ArrData+Index*InnerSize, ArrData+(Index+Count)*InnerSize, (oldnum-Index-Count)*InnerSize);
    }
    // now resize it, but don't shrink (this will clear unused values too)
    SetNum(oldnum-Count, Type, false);
  }
  unguard;
}


//==========================================================================
//
//  VScriptArray::SwapElements
//
//==========================================================================
void VScriptArray::SwapElements (int i0, int i1, const VFieldType &Type) {
  if (i0 == i1) return;
  //fprintf(stderr, "VScriptArray::SwapElements: i0=%d; i1=%d\n", i0, i1);
  int InnerSize = Type.GetSize();
  vuint32 *p0 = (vuint32 *)(ArrData+i0*InnerSize);
  vuint32 *p1 = (vuint32 *)(ArrData+i1*InnerSize);
  while (InnerSize >= 4) {
    vuint32 t = *p0;
    *p0 = *p1;
    *p1 = t;
    ++p0;
    ++p1;
    InnerSize -= 4;
  }
  vuint8 *c0 = (vuint8 *)p0;
  vuint8 *c1 = (vuint8 *)p1;
  while (InnerSize--) {
    vuint8 t = *c0;
    *c0 = *c1;
    *c1 = t;
    ++c0;
    ++c1;
  }
}


//==========================================================================
//
//  VScriptArray::CallComparePtr
//
//==========================================================================
int VScriptArray::CallComparePtr (void *p0, void *p1, const VFieldType &Type, VObject *self, VMethod *fnless) {
  if (p0 == p1) return 0; // same indicies are always equal
  // self
  if ((fnless->Flags&FUNC_Static) == 0) P_PASS_REF(self);
  // first arg
  if ((fnless->ParamFlags[0]&(FPARM_Out|FPARM_Ref)) != 0) {
    P_PASS_REF(p0);
  } else {
    switch (Type.Type) {
      case TYPE_Int: P_PASS_INT(*(vint32 *)p0); break;
      case TYPE_Byte: P_PASS_BYTE(*(vuint8 *)p0); break;
      case TYPE_Bool: P_PASS_BOOL(*(vint32 *)p0); break;
      case TYPE_Float: P_PASS_FLOAT(*(float *)p0); break;
      case TYPE_Name: P_PASS_NAME(*(VName *)p0); break;
      case TYPE_String: P_PASS_STR(*(VStr *)p0); break;
      case TYPE_Pointer: P_PASS_PTR(*(void **)p0); break;
      case TYPE_Reference: P_PASS_REF(*(VObject **)p0); break;
      case TYPE_Class: P_PASS_PTR(*(VClass **)p0); break;
      case TYPE_State: P_PASS_PTR(*(VState **)p0); break;
      //case TYPE_Delegate
      //case TYPE_Struct,
      case TYPE_Vector: P_PASS_VEC(*(TVec *)p0); break;
      //case TYPE_Array:
      //case TYPE_DynamicArray:
      //case TYPE_SliceArray: // array consisting of pointer and length, with immutable length
      //case TYPE_Dictionary:
      default: abort(); // the thing that should not be
    }
  }
  // second arg
  if ((fnless->ParamFlags[1]&(FPARM_Out|FPARM_Ref)) != 0) {
    P_PASS_REF(p1);
  } else {
    switch (Type.Type) {
      case TYPE_Int: P_PASS_INT(*(vint32 *)p1); break;
      case TYPE_Byte: P_PASS_BYTE(*(vuint8 *)p1); break;
      case TYPE_Bool: P_PASS_BOOL(*(vint32 *)p1); break;
      case TYPE_Float: P_PASS_FLOAT(*(float *)p1); break;
      case TYPE_Name: P_PASS_NAME(*(VName *)p1); break;
      case TYPE_String: P_PASS_STR(*(VStr *)p1); break;
      case TYPE_Pointer: P_PASS_PTR(*(void **)p1); break;
      case TYPE_Reference: P_PASS_REF(*(VObject **)p1); break;
      case TYPE_Class: P_PASS_PTR(*(VClass **)p1); break;
      case TYPE_State: P_PASS_PTR(*(VState **)p1); break;
      //case TYPE_Delegate
      //case TYPE_Struct,
      case TYPE_Vector: P_PASS_VEC(*(TVec *)p1); break;
      //case TYPE_Array:
      //case TYPE_DynamicArray:
      //case TYPE_SliceArray: // array consisting of pointer and length, with immutable length
      //case TYPE_Dictionary:
      default: abort(); // the thing that should not be
    }
  }
  return VObject::ExecuteFunction(fnless).getInt();
}


//==========================================================================
//
//  VScriptArray::CallCompare
//
//==========================================================================
int VScriptArray::CallCompare (int i0, int i1, const VFieldType &Type, VObject *self, VMethod *fnless) {
  if (i0 == i1) return 0; // same indicies are always equal
  //fprintf(stderr, "VScriptArray::CallCompare: i0=%d; i1=%d\n", i0, i1);
  int InnerSize = Type.GetSize();
  vuint8 *p0 = ArrData+i0*InnerSize;
  vuint8 *p1 = ArrData+i1*InnerSize;
  return CallComparePtr(p0, p1, Type, self, fnless);
}


struct VSASortInfo {
  VScriptArray *arr;
  VFieldType Type;
  VObject *self;
  VMethod *fnless;
  vuint8 *ArrData;
};


extern "C" {
static int vsaCompare (const void *aa, const void *bb, void *udata) {
  VSASortInfo *si = (VSASortInfo *)udata;
  /*
  const int InnerSize = si->Type.GetSize();
  const vuint8 *a = (const vuint8 *)aa;
  const vuint8 *b = (const vuint8 *)bb;
  const int i0 = ((int)(ptrdiff_t)(a-si->ArrData))/InnerSize;
  const int i1 = ((int)(ptrdiff_t)(b-si->ArrData))/InnerSize;
  return si->arr->CallCompare(i0, i1, si->Type, si->self, si->fnless);
  */
  return VScriptArray::CallComparePtr((void *)aa, (void *)bb, si->Type, si->self, si->fnless);
}
}


//==========================================================================
//
//  VScriptArray::Sort
//
//==========================================================================
//#define SRTLOG(fmt,...)  fprintf(stderr, "VScriptArray::Sort: " fmt "\n", __VA_ARGS__)
#define SRTLOG(fmt,...)  (void)((void)fmt, (void)__VA_ARGS__)

//static final void sortIntArray (ref array!int arr, bool delegate (int a, int b) dgLess, optional int count)
bool VScriptArray::Sort (const VFieldType &Type, VObject *self, VMethod *fnless) {
  // check delegate
  if (!fnless) {
    SRTLOG("%s", "delegate is null");
    return false;
  }
  SRTLOG("dgname: `%s`", *fnless->GetFullName());
  if (fnless->NumParams != 2) {
    SRTLOG("%s (%d)", "delegate has invalid number of parameters", fnless->NumParams);
    return false;
  }
  if (fnless->ReturnType.Type != TYPE_Int && fnless->ReturnType.Type != TYPE_Bool) {
    SRTLOG("%s", "delegate has invalid return type");
    return false;
  }
  if (!fnless->ParamTypes[0].Equals(Type) || !fnless->ParamTypes[1].Equals(Type)) {
    SRTLOG("%s", "delegate has invalid parameters type");
    return false;
  }
  // check if type should be passed as ref
  bool requireRef = false;
  if (Type.PtrLevel == 0) {
    switch (Type.Type) {
      case TYPE_Struct:
      case TYPE_Vector: //FIXME
      case TYPE_DynamicArray:
      case TYPE_Dictionary:
        requireRef = true;
        break;
      case TYPE_Delegate:
      case TYPE_Array:
      case TYPE_SliceArray: //FIXME
        return false;
      default:
        break;
    }
  }
  if (requireRef) {
    if ((fnless->ParamFlags[0]&(FPARM_Out|FPARM_Ref)) == 0) {
      SRTLOG("%s", "first delegate parameter is not `ref`");
      return false;
    }
    if ((fnless->ParamFlags[1]&(FPARM_Out|FPARM_Ref)) == 0) {
      SRTLOG("%s", "second delegate parameter is not `ref`");
      return false;
    }
  }
  // no optional args allowed
  if ((fnless->ParamFlags[0]|fnless->ParamFlags[1])&FPARM_Optional) {
    SRTLOG("%s", "some delegate parameters are optional");
    return false;
  }
  // if we have no self, this should be a static method
  if (!self && (fnless->Flags&FUNC_Static) == 0) {
    SRTLOG("%s", "has no self, but delegate is not static");
    return false;
  }
  // check other flags
  if (fnless->Flags&(FUNC_VarArgs|FUNC_Iterator)) {
    SRTLOG("%s", "delegate is iterator or vararg");
    return false;
  }
  // ok, it looks that our delegate is valid

  /*
  if (count < 2 || ArrNum < 2) return; // nothing to do
  if (count > ArrNum) count = ArrNum;
  */

  Flatten();

  if (ArrNum < 2) return true;
  int count = ArrNum;

  /*
  int InnerSize = Type.GetSize();
  QSortInfo nfo;
  nfo.arr = this.
  nfo.Type = Type;

  qsort_r(ArrData, ArrNum, InnerSize, &QComparator, &nfo);
  */

  if (count == 2) {
    if (CallCompare(0, 1, Type, self, fnless) < 0) SwapElements(0, 1, Type);
    return true;
  }

#if 0
  auto end = count-1;

  // heapify
  auto start = (end-1)/2; // parent; always safe, as our array has at least two items
  for (;;) {
    //siftDownCIDSort(arr, start, end, dgLess);
    auto root = start;
    for (;;) {
      auto child = 2*root+1; // left child
      if (child > end) break;
      auto swap = root;
      if (CallCompare(swap, child, Type, self, fnless) < 0) swap = child;
      if (child+1 <= end && CallCompare(swap, child+1, Type, self, fnless) < 0) swap = child+1;
      if (swap == root) break;
      SwapElements(swap, root, Type);
      root = swap;
    }
    if (start-- == 0) break; // as `start` cannot be negative, use this condition
  }

  while (end > 0) {
    SwapElements(0, end, Type);
    --end;
    //siftDownCIDSort(arr, 0, end, dgLess);
    auto root = 0;
    for (;;) {
      auto child = 2*root+1; // left child
      if (child > end) break;
      auto swap = root;
      if (CallCompare(swap, child, Type, self, fnless) < 0) swap = child;
      if (child+1 <= end && CallCompare(swap, child+1, Type, self, fnless) < 0) swap = child+1;
      if (swap == root) break;
      SwapElements(swap, root, Type);
      root = swap;
    }
  }
#else
  VSASortInfo si;
  si.arr = this;
  si.Type = Type;
  si.self = self;
  si.fnless = fnless;
  si.ArrData = ArrData;
  timsort_r(ArrData, (size_t)ArrNum, (size_t)Type.GetSize(), &vsaCompare, &si);
#endif

  return true;
}

#endif // !defined(IN_VCC)
