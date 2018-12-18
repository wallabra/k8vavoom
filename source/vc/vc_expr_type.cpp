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
#if defined(IN_VCC) //&& !defined(VCC_STANDALONE_EXECUTOR)
# include "vc_object.h"
#endif


//==========================================================================
//
//  VTypeExpr::VTypeExpr
//
//==========================================================================
VTypeExpr::VTypeExpr (VFieldType atype, const TLocation &aloc)
  : VExpression(aloc)
  , Expr(nullptr)
  , MetaClassName(NAME_None)
{
  Type = atype;
}


//==========================================================================
//
//  VTypeExpr::NewTypeExpr
//
//==========================================================================
VTypeExpr *VTypeExpr::NewTypeExpr (VFieldType atype, const TLocation &aloc) {
  switch (atype.Type) {
    case TYPE_Void:
    case TYPE_Int:
    case TYPE_Byte:
    case TYPE_Bool:
    case TYPE_Float:
    case TYPE_Name:
    case TYPE_String:
    case TYPE_Reference:
    case TYPE_State:
    case TYPE_Struct:
    case TYPE_Vector:
      return new VTypeExprSimple(atype, aloc);
    case TYPE_Pointer:
      return new VPointerType(NewTypeExpr(atype.GetPointerInnerType(), aloc), aloc);
    case TYPE_Class:
      return new VTypeExprClass((atype.Class ? atype.Class->Name : NAME_None), aloc);
    case TYPE_Delegate:
      /*
      fprintf(stderr, "<%s>\n", *atype.GetName());
      abort();
      FatalError("VC: VTypeExpr::NewTypeExpr: no delegates yet");
      */
      return new VDelegateType(atype, aloc);
    case TYPE_Array:
      if (atype.IsArray1D()) {
        return new VFixedArrayType(NewTypeExpr(atype.GetArrayInnerType(), aloc),
          new VIntLiteral(atype.GetArrayDim(), aloc), nullptr, aloc);
      } else {
        return new VFixedArrayType(NewTypeExpr(atype.GetArrayInnerType(), aloc),
          new VIntLiteral(atype.GetFirstDim(), aloc), new VIntLiteral(atype.GetSecondDim(), aloc), aloc);
      }
    case TYPE_DynamicArray:
      return new VDynamicArrayType(NewTypeExpr(atype.GetArrayInnerType(), aloc), aloc);
    case TYPE_SliceArray:
      return new VSliceType(NewTypeExpr(atype.GetArrayInnerType(), aloc), aloc);
    case TYPE_Dictionary:
      return new VDictType(NewTypeExpr(atype.GetDictKeyType(), aloc), NewTypeExpr(atype.GetDictValueType(), aloc), aloc);
    case TYPE_Unknown:
    case TYPE_Automatic: // this is valid only for variable declarations, and will be resolved to actual type
      Sys_Error("VC: VTypeExpr::NewTypeExpr: internal compiler error");
      break;
    default: break;
  }
  Sys_Error("VC: VTypeExpr::NewTypeExpr: internal compiler error");
  return nullptr;
};


//==========================================================================
//
//  VTypeExpr::NewTypeExprFromAuto
//
//  this one resolves `TYPE_Vector` to `TVec` struct
//
//==========================================================================
VTypeExpr *VTypeExpr::NewTypeExprFromAuto (VFieldType atype, const TLocation &aloc) {
  // convert vector to `TVec` struct
  if (atype.Type == TYPE_Vector || (atype.Type == TYPE_Pointer && atype.InnerType == TYPE_Vector)) {
    if (!atype.Struct) {
#if defined(IN_VCC) //&& !defined(VCC_STANDALONE_EXECUTOR)
      VClass *ocls = (VClass *)VMemberBase::StaticFindMember("Object", ANY_PACKAGE, MEMBER_Class);
      check(ocls);
      VStruct *tvs = (VStruct *)VMemberBase::StaticFindMember("TVec", ocls, MEMBER_Struct);
#else
      VStruct *tvs = (VStruct *)VMemberBase::StaticFindMember("TVec", /*ANY_PACKAGE*/VObject::StaticClass(), MEMBER_Struct);
#endif
      if (!tvs) {
        ParseError(aloc, "Cannot find `TVec` struct for vector type");
        return NewTypeExpr(atype, aloc);
      }
      VFieldType stt = VFieldType(tvs);
      return new VTypeExprSimple(stt, aloc);
    }
  }
  return NewTypeExpr(atype, aloc);
}


//==========================================================================
//
//  VTypeExpr::~VTypeExpr
//
//==========================================================================
VTypeExpr::~VTypeExpr () {
  delete Expr;
}


//==========================================================================
//
//  VTypeExpr::DoSyntaxCopyTo
//
//==========================================================================
void VTypeExpr::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VTypeExpr *)e;
  res->MetaClassName = MetaClassName;
  res->Expr = (Expr ? Expr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VTypeExpr::DoResolve
//
//==========================================================================
VExpression *VTypeExpr::DoResolve (VEmitContext &ec) {
  return ResolveAsType(ec);
}


//==========================================================================
//
//  VTypeExpr::Emit
//
//==========================================================================
void VTypeExpr::Emit (VEmitContext &) {
  ParseError(Loc, "Should not happen (VTypeExpr)");
}


//==========================================================================
//
//  VTypeExpr::GetName
//
//==========================================================================
VStr VTypeExpr::GetName () const {
  return Type.GetName();
}


//==========================================================================
//
//  VTypeExpr::IsTypeExpr
//
//==========================================================================
bool VTypeExpr::IsTypeExpr () const {
  return true;
}


//==========================================================================
//
//  VTypeExpr::toString
//
//==========================================================================
VStr VTypeExpr::toString () const {
  return GetName();
}



//==========================================================================
//
//  VTypeExprSimple::VTypeExprSimple
//
//==========================================================================
VTypeExprSimple::VTypeExprSimple (EType atype, const TLocation &aloc)
  : VTypeExpr(VFieldType(atype), aloc)
{
}


//==========================================================================
//
//  VTypeExprSimple::VTypeExprSimple
//
//==========================================================================
VTypeExprSimple::VTypeExprSimple (VFieldType atype, const TLocation &aloc)
  : VTypeExpr(atype, aloc)
{
}


//==========================================================================
//
//  VTypeExprSimple::SyntaxCopy
//
//==========================================================================
VExpression *VTypeExprSimple::SyntaxCopy () {
  auto res = new VTypeExprSimple();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VTypeExprSimple::ResolveAsType
//
//==========================================================================
VTypeExpr *VTypeExprSimple::ResolveAsType (VEmitContext &) {
  if (Type.Type == TYPE_Unknown) {
    ParseError(Loc, "Bad type");
    delete this;
    return nullptr;
  }

  if (Type.Type == TYPE_Automatic) Sys_Error("VC INTERNAL COMPILER ERROR: unresolved automatic type (0)!");
  if (Type.Type == TYPE_Class) Sys_Error("VC INTERNAL COMPILER ERROR: 19463!");

  return this;
}


//==========================================================================
//
//  VTypeExprSimple::IsAutoTypeExpr
//
//==========================================================================
bool VTypeExprSimple::IsAutoTypeExpr () const {
  return (Type.Type == TYPE_Automatic);
}


//==========================================================================
//
//  VTypeExprSimple::IsSimpleType
//
//==========================================================================
bool VTypeExprSimple::IsSimpleType () const {
  return true;
}



//==========================================================================
//
//  VTypeExprClass::VTypeExprClass
//
//==========================================================================
VTypeExprClass::VTypeExprClass (VName AMetaClassName, const TLocation &aloc)
  : VTypeExpr(TYPE_Class, aloc)
{
  MetaClassName = AMetaClassName;
}


//==========================================================================
//
//  VTypeExprClass::SyntaxCopy
//
//==========================================================================
VExpression *VTypeExprClass::SyntaxCopy () {
  auto res = new VTypeExprClass();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VTypeExprClass::ResolveAsType
//
//==========================================================================
VTypeExpr *VTypeExprClass::ResolveAsType (VEmitContext &) {
  if (MetaClassName != NAME_None) {
    Type.Class = VMemberBase::StaticFindClass(MetaClassName);
    if (!Type.Class) {
      ParseError(Loc, "No such class `%s`", *MetaClassName);
      delete this;
      return nullptr;
    }
  }

  return this;
}


//==========================================================================
//
//  VTypeExprClass::IsClassType
//
//==========================================================================
bool VTypeExprClass::IsClassType () const {
  return true;
}



//==========================================================================
//
//  VPointerType::VPointerType
//
//==========================================================================
VPointerType::VPointerType (VExpression *AExpr, const TLocation &ALoc)
  : VTypeExpr(TYPE_Unknown, ALoc)
{
  Expr = AExpr;
}


//==========================================================================
//
//  VPointerType::SyntaxCopy
//
//==========================================================================
VExpression *VPointerType::SyntaxCopy () {
  auto res = new VPointerType();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VPointerType::ResolveAsType
//
//==========================================================================
VTypeExpr *VPointerType::ResolveAsType (VEmitContext &ec) {
  if (Expr) Expr = Expr->ResolveAsType(ec);
  if (!Expr) { delete this; return nullptr; }
  Type = Expr->Type.MakePointerType();
  return this;
}


//==========================================================================
//
//  VPointerType::IsPointerType
//
//==========================================================================
bool VPointerType::IsPointerType () const {
  return true;
}



//==========================================================================
//
//  VFixedArrayType::VFixedArrayType
//
//==========================================================================
VFixedArrayType::VFixedArrayType (VExpression *AExpr, VExpression *ASizeExpr, VExpression *ASizeExpr2, const TLocation &ALoc)
  : VTypeExpr(TYPE_Unknown, ALoc)
  , SizeExpr(ASizeExpr)
  , SizeExpr2(ASizeExpr2)
{
  Expr = AExpr;
  if (!SizeExpr) ParseError(Loc, "Array size expected");
}


//==========================================================================
//
//  VFixedArrayType::~VFixedArrayType
//
//==========================================================================
VFixedArrayType::~VFixedArrayType () {
  if (SizeExpr) { delete SizeExpr; SizeExpr = nullptr; }
  if (SizeExpr2) { delete SizeExpr2; SizeExpr2 = nullptr; }
}


//==========================================================================
//
//  VFixedArrayType::SyntaxCopy
//
//==========================================================================
VExpression *VFixedArrayType::SyntaxCopy () {
  auto res = new VFixedArrayType();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VFixedArrayType::DoSyntaxCopyTo
//
//==========================================================================
void VFixedArrayType::DoSyntaxCopyTo (VExpression *e) {
  VTypeExpr::DoSyntaxCopyTo(e);
  auto res = (VFixedArrayType *)e;
  res->SizeExpr = (SizeExpr ? SizeExpr->SyntaxCopy() : nullptr);
  res->SizeExpr2 = (SizeExpr2 ? SizeExpr2->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VFixedArrayType::ResolveAsType
//
//==========================================================================
VTypeExpr *VFixedArrayType::ResolveAsType (VEmitContext &ec) {
  if (Expr) Expr = Expr->ResolveAsType(ec);
  if (SizeExpr) SizeExpr = SizeExpr->Resolve(ec);
  if (SizeExpr2) {
    SizeExpr2 = SizeExpr2->Resolve(ec);
    if (!SizeExpr2) { delete this; return nullptr; }
  }
  if (!Expr || !SizeExpr) { delete this; return nullptr; }

  if (Expr->IsAnyArrayType()) {
    ParseError(Expr->Loc, "Arrays of arrays are not allowed (yet)");
    delete this;
    return nullptr;
  }

  if (!SizeExpr->IsIntConst() || (SizeExpr2 && !SizeExpr2->IsIntConst())) {
    ParseError(SizeExpr->Loc, "Integer constant expected");
    delete this;
    return nullptr;
  }

  vint32 Size = SizeExpr->GetIntConst();
  if (Size < 0) {
    ParseError(SizeExpr->Loc, "Static array cannot be of negative size");
    delete this;
    return nullptr;
  }

  if (SizeExpr2) {
    if (Size < 1 || SizeExpr2->GetIntConst() < 1) {
      ParseError(SizeExpr2->Loc, "Static 2d array cannot be of negative or empty size");
      delete this;
      return nullptr;
    }
    Type = Expr->Type.MakeArray2DType(Size, SizeExpr2->GetIntConst(), Loc);
  } else {
    Type = Expr->Type.MakeArrayType(Size, Loc);
  }

  return this;
}


//==========================================================================
//
//  VFixedArrayType::IsAnyArrayType
//
//==========================================================================
bool VFixedArrayType::IsAnyArrayType () const {
  return true;
}


//==========================================================================
//
//  VFixedArrayType::IsStaticArrayType
//
//==========================================================================
bool VFixedArrayType::IsStaticArrayType () const {
  return true;
}



//==========================================================================
//
//  VDynamicArrayType::VDynamicArrayType
//
//==========================================================================
VDynamicArrayType::VDynamicArrayType (VExpression *AExpr, const TLocation &ALoc)
  : VTypeExpr(TYPE_Unknown, ALoc)
{
  Expr = AExpr;
}


//==========================================================================
//
//  VDynamicArrayType::SyntaxCopy
//
//==========================================================================
VExpression *VDynamicArrayType::SyntaxCopy () {
  auto res = new VDynamicArrayType();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDynamicArrayType::ResolveAsType
//
//==========================================================================
VTypeExpr *VDynamicArrayType::ResolveAsType (VEmitContext &ec) {
  if (Expr) Expr = Expr->ResolveAsType(ec);
  if (!Expr) { delete this; return nullptr; }

  if (Expr->IsAnyArrayType()) {
    ParseError(Expr->Loc, "Arrays of arrays are not allowed (yet)");
    delete this;
    return nullptr;
  }

  Type = Expr->Type.MakeDynamicArrayType(Loc);
  return this;
}


//==========================================================================
//
//  VDynamicArrayType::IsAnyArrayType
//
//==========================================================================
bool VDynamicArrayType::IsAnyArrayType () const {
  return true;
}


//==========================================================================
//
//  VDynamicArrayType::IsDynamicArrayType
//
//==========================================================================
bool VDynamicArrayType::IsDynamicArrayType () const {
  return true;
}



//==========================================================================
//
//  VSliceType::VSliceType
//
//==========================================================================
VSliceType::VSliceType (VExpression *AExpr, const TLocation &ALoc)
  : VTypeExpr(TYPE_Unknown, ALoc)
{
  Expr = AExpr;
}


//==========================================================================
//
//  VSliceType::SyntaxCopy
//
//==========================================================================
VExpression *VSliceType::SyntaxCopy () {
  auto res = new VSliceType();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VSliceType::ResolveAsType
//
//==========================================================================
VTypeExpr *VSliceType::ResolveAsType (VEmitContext &ec) {
  if (Expr) Expr = Expr->ResolveAsType(ec);
  if (!Expr) { delete this; return nullptr; }

  if (Expr->IsAnyArrayType()) {
    ParseError(Expr->Loc, "Arrays of arrays are not allowed (yet)");
    delete this;
    return nullptr;
  }

  Type = Expr->Type.MakeSliceType(Loc);
  return this;
}


//==========================================================================
//
//  VSliceType::IsAnyArrayType
//
//==========================================================================
bool VSliceType::IsAnyArrayType () const {
  return true;
}


//==========================================================================
//
//  VSliceType::IsSliceType
//
//==========================================================================
bool VSliceType::IsSliceType () const {
  return true;
}



//==========================================================================
//
//  VDelegateType::VDelegateType
//
//==========================================================================
VDelegateType::VDelegateType (VExpression *aexpr, const TLocation &aloc)
  : VTypeExpr(TYPE_Unknown, aloc)
  , Flags(0)
  , NumParams(0)
{
  Expr = aexpr;
  memset(Params, 0, sizeof(Params));
  memset(ParamFlags, 0, sizeof(ParamFlags));
}


//==========================================================================
//
//  VDelegateType::VDelegateType
//
//==========================================================================
VDelegateType::VDelegateType (VFieldType atype, const TLocation &aloc)
  : VTypeExpr(TYPE_Unknown, aloc)
  , Flags(0)
  , NumParams(0)
{
  Expr = nullptr;
  memset(Params, 0, sizeof(Params));
  memset(ParamFlags, 0, sizeof(ParamFlags));
  //FatalError("VC: VDelegateType::VDelegateType: no `auto` delegates yet, use the full declaration");
}


//==========================================================================
//
//  VDelegateType::~VDelegateType
//
//==========================================================================
VDelegateType::~VDelegateType () {
  for (int f = 0; f < NumParams; ++f) {
    delete Params[f].TypeExpr;
    Params[f].TypeExpr = nullptr;
  }
  NumParams = 0;
}


//==========================================================================
//
//  VDelegateType::SyntaxCopy
//
//==========================================================================
VExpression *VDelegateType::SyntaxCopy () {
  auto res = new VDelegateType();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDelegateType::DoSyntaxCopyTo
//
//==========================================================================
void VDelegateType::DoSyntaxCopyTo (VExpression *e) {
  VTypeExpr::DoSyntaxCopyTo(e);
  auto res = (VDelegateType *)e;
  res->Flags = Flags;
  res->NumParams = NumParams;
  for (int f = 0; f < NumParams; ++f) {
    res->ParamFlags[f] = ParamFlags[f];
    res->Params[f].TypeExpr = (Params[f].TypeExpr ? Params[f].TypeExpr->SyntaxCopy() : nullptr);
    res->Params[f].Name = Params[f].Name;
    res->Params[f].Loc = Params[f].Loc;
  }
}


//==========================================================================
//
//  VDelegateType::ResolveAsType
//
//==========================================================================
VTypeExpr *VDelegateType::ResolveAsType (VEmitContext &ec) {
  if (!Expr) {
    ParseError(Loc, "VC: VDelegateType::VDelegateType: no `auto` delegates yet, use the full declaration");
    delete this;
    return nullptr;
  }
  VMethod *Func = CreateDelegateMethod(ec.CurrentFunc);
  Func->Define();
  Type = VFieldType(TYPE_Delegate);
  Type.Function = Func;
  return this;
}


VMethod *VDelegateType::CreateDelegateMethod (VMemberBase *aowner) {
  VMethod *Func = new VMethod(NAME_None, aowner, Loc);
  Func->ReturnTypeExpr = Expr->SyntaxCopy();
  Func->Flags = Flags;
  Func->NumParams = NumParams;
  // copy params
  for (int f = 0; f < NumParams; ++f) {
    Func->ParamFlags[f] = ParamFlags[f];
    Func->Params[f].TypeExpr = (Params[f].TypeExpr ? Params[f].TypeExpr->SyntaxCopy() : nullptr);
    Func->Params[f].Name = Params[f].Name;
    Func->Params[f].Loc = Params[f].Loc;
  }
  return Func;
}


//==========================================================================
//
//  VDelegateType::IsDelegateType
//
//==========================================================================
bool VDelegateType::IsDelegateType () const {
  return true;
}



//==========================================================================
//
//  VDictType::VDictType
//
//==========================================================================
VDictType::VDictType (VExpression *AKExpr, VExpression *AVExpr, const TLocation &ALoc)
  : VTypeExpr(TYPE_Unknown, ALoc)
{
  Expr = AKExpr;
  VExpr = AVExpr;
}


//==========================================================================
//
//  VDictType::~VDictType
//
//==========================================================================
VDictType::~VDictType () {
  delete VExpr; VExpr = nullptr;
}


//==========================================================================
//
//  VDictType::SyntaxCopy
//
//==========================================================================
VExpression *VDictType::SyntaxCopy () {
  auto res = new VDictType();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDictType::DoSyntaxCopyTo
//
//==========================================================================
void VDictType::DoSyntaxCopyTo (VExpression *e) {
  VTypeExpr::DoSyntaxCopyTo(e);
  auto res = (VDictType *)e;
  res->VExpr = (VExpr ? VExpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDictType::ResolveAsType
//
//==========================================================================
VTypeExpr *VDictType::ResolveAsType (VEmitContext &ec) {
  if (!Expr || !VExpr) {
    ParseError(Loc, "no auto dictionaries allowed");
    delete this;
    return nullptr;
  }

  if (Expr) Expr = Expr->ResolveAsType(ec);
  if (VExpr) VExpr = VExpr->ResolveAsType(ec);
  if (!Expr || !VExpr) { delete this; return nullptr; }

  Type = VFieldType::MakeDictType(Expr->Type, VExpr->Type, Loc);
  return this;
}


//==========================================================================
//
//  VDictType::IsDictType
//
//==========================================================================
bool VDictType::IsDictType () const {
  return true;
}
