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
      FatalError("VC: VTypeExpr::NewTypeExpr: no delegates yet");
    case TYPE_Array:
      return new VFixedArrayType(NewTypeExpr(atype.GetArrayInnerType(), aloc), new VIntLiteral(atype.ArrayDim, aloc), aloc);
    case TYPE_DynamicArray:
      return new VDynamicArrayType(NewTypeExpr(atype.GetArrayInnerType(), aloc), aloc);
    case TYPE_Unknown:
    case TYPE_Automatic: // this is valid only for variable declarations, and will be resolved to actual type
      fprintf(stderr, "VC: VTypeExpr::NewTypeExpr: internal compiler error\n");
      *(int *)0 = 0;
    default: break;
  }
  fprintf(stderr, "VC: VTypeExpr::NewTypeExpr: internal compiler error\n");
  *(int *)0 = 0;
  return nullptr;
};


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

  if (Type.Type == TYPE_Automatic) {
    fprintf(stderr, "VC INTERNAL COMPILER ERROR: unresolved automatic type (0)!\n");
    *(int*)0 = 0;
  }

  if (Type.Type == TYPE_Class) {
    fprintf(stderr, "VC INTERNAL COMPILER ERROR: 19463!\n");
    *(int*)0 = 0;
  }

  return this;
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
VFixedArrayType::VFixedArrayType (VExpression *AExpr, VExpression *ASizeExpr, const TLocation &ALoc)
  : VTypeExpr(TYPE_Unknown, ALoc)
  , SizeExpr(ASizeExpr)
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
}


//==========================================================================
//
//  VFixedArrayType::ResolveAsType
//
//==========================================================================
VTypeExpr *VFixedArrayType::ResolveAsType (VEmitContext &ec) {
  if (Expr) Expr = Expr->ResolveAsType(ec);
  if (SizeExpr) SizeExpr = SizeExpr->Resolve(ec);
  if (!Expr || !SizeExpr) { delete this; return nullptr; }

  if (Expr->IsAnyArrayType()) {
    ParseError(Expr->Loc, "Arrays of arrays are not allowed (yet)");
    delete this;
    return nullptr;
  }

  if (!SizeExpr->IsIntConst()) {
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

  Type = Expr->Type.MakeArrayType(Size, Loc);
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
  if (Expr) Expr = Expr->ResolveAsType(ec);
  if (!Expr) { delete this; return nullptr; }
  Type = Expr->Type;
  //RealType = create delegate type here
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
