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
VTypeExpr::VTypeExpr (VFieldType AType, const TLocation &ALoc)
  : VExpression(ALoc)
  , MetaClassName(NAME_None)
{
  Type = AType;
}


//==========================================================================
//
//  VTypeExpr::VTypeExpr
//
//==========================================================================
VTypeExpr::VTypeExpr (VFieldType AType, const TLocation &ALoc, VName AMetaClassName)
  : VExpression(ALoc)
  , MetaClassName(AMetaClassName)
{
  Type = AType;
}


//==========================================================================
//
//  VTypeExpr::SyntaxCopy
//
//==========================================================================
VExpression *VTypeExpr::SyntaxCopy () {
  auto res = new VTypeExpr();
  DoSyntaxCopyTo(res);
  return res;
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
//  VTypeExpr::ResolveAsType
//
//==========================================================================
VTypeExpr *VTypeExpr::ResolveAsType (VEmitContext &) {
  if (Type.Type == TYPE_Unknown) {
    ParseError(Loc, "Bad type");
    delete this;
    return nullptr;
  }

  if (Type.Type == TYPE_Automatic) {
    fprintf(stderr, "VC INTERNAL COMPILER ERROR: unresolved automatic type (0)!\n");
    *(int*)0 = 0;
  }

  if (Type.Type == TYPE_Class && MetaClassName != NAME_None) {
    Type.Class = VMemberBase::StaticFindClass(MetaClassName);
    if (!Type.Class) {
      ParseError(Loc, "No such class %s", *MetaClassName);
      delete this;
      return nullptr;
    }
  }

  return this;
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
//  VPointerType::VPointerType
//
//==========================================================================
VPointerType::VPointerType (VExpression *AExpr, const TLocation &ALoc)
  : VTypeExpr(TYPE_Unknown, ALoc)
  , Expr(AExpr)
{
}


//==========================================================================
//
//  VPointerType::~VPointerType
//
//==========================================================================
VPointerType::~VPointerType () {
  if (Expr) { delete Expr; Expr = nullptr; }
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
//  VPointerType::DoSyntaxCopyTo
//
//==========================================================================
void VPointerType::DoSyntaxCopyTo (VExpression *e) {
  VTypeExpr::DoSyntaxCopyTo(e);
  auto res = (VPointerType *)e;
  res->Expr = (Expr ? Expr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VPointerType::ResolveAsType
//
//==========================================================================
VTypeExpr *VPointerType::ResolveAsType (VEmitContext &ec) {
  if (Expr) Expr = Expr->ResolveAsType(ec);
  if (!Expr) {
    delete this;
    return nullptr;
  }

  Type = Expr->Type.MakePointerType();
  return this;
}


//==========================================================================
//
//  VFixedArrayType::VFixedArrayType
//
//==========================================================================
VFixedArrayType::VFixedArrayType (VExpression *AExpr, VExpression *ASizeExpr, const TLocation &ALoc)
  : VTypeExpr(TYPE_Unknown, ALoc)
  , Expr(AExpr)
  , SizeExpr(ASizeExpr)
{
  if (!SizeExpr) ParseError(Loc, "Array size expected");
}


//==========================================================================
//
//  VFixedArrayType::~VFixedArrayType
//
//==========================================================================
VFixedArrayType::~VFixedArrayType () {
  if (Expr) { delete Expr; Expr = nullptr; }
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
  res->Expr = (Expr ? Expr->SyntaxCopy() : nullptr);
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
  if (!Expr || !SizeExpr) {
    delete this;
    return nullptr;
  }

  if (!SizeExpr->IsIntConst()) {
    ParseError(SizeExpr->Loc, "Integer constant expected");
    delete this;
    return nullptr;
  }

  vint32 Size = SizeExpr->GetIntConst();
  Type = Expr->Type.MakeArrayType(Size, Loc);
  return this;
}


//==========================================================================
//
//  VDynamicArrayType::VDynamicArrayType
//
//==========================================================================
VDynamicArrayType::VDynamicArrayType (VExpression *AExpr, const TLocation &ALoc)
  : VTypeExpr(TYPE_Unknown, ALoc)
  , Expr(AExpr)
{
}


//==========================================================================
//
//  VDynamicArrayType::~VDynamicArrayType
//
//==========================================================================
VDynamicArrayType::~VDynamicArrayType () {
  if (Expr) { delete Expr; Expr = nullptr; }
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
//  VPointerType::DoSyntaxCopyTo
//
//==========================================================================
void VDynamicArrayType::DoSyntaxCopyTo (VExpression *e) {
  VTypeExpr::DoSyntaxCopyTo(e);
  auto res = (VDynamicArrayType *)e;
  res->Expr = (Expr ? Expr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDynamicArrayType::ResolveAsType
//
//==========================================================================
VTypeExpr *VDynamicArrayType::ResolveAsType (VEmitContext &ec) {
  if (Expr) Expr = Expr->ResolveAsType(ec);
  if (!Expr) {
    delete this;
    return nullptr;
  }

  Type = Expr->Type.MakeDynamicArrayType(Loc);
  return this;
}
