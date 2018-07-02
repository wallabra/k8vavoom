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
//  VCastExpressionBase
//
//==========================================================================
VCastExpressionBase::VCastExpressionBase (VExpression *AOp, bool aOpResolved) : VExpression(AOp->Loc), op(AOp), opResolved(aOpResolved) {}
VCastExpressionBase::VCastExpressionBase (const TLocation &ALoc, bool aOpResolved) : VExpression(ALoc), op(nullptr), opResolved(aOpResolved) {}
VCastExpressionBase::~VCastExpressionBase () { if (op) { delete op; op = nullptr; } }

void VCastExpressionBase::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VCastExpressionBase *)e;
  res->op = (op ? op->SyntaxCopy() : nullptr);
  res->opResolved = opResolved;
}

VExpression *VCastExpressionBase::DoResolve (VEmitContext &ec) {
  if (op && !opResolved) {
    opResolved = true;
    op = op->Resolve(ec);
    if (!op) { delete this; return nullptr; }
  }
  return this;
}


//==========================================================================
//
//  VDelegateToBool::VDelegateToBool
//
//==========================================================================
VDelegateToBool::VDelegateToBool (VExpression *AOp, bool aOpResolved)
  : VCastExpressionBase(AOp, aOpResolved)
{
  Type = TYPE_Int;
  if (aOpResolved) {
    vint32 wasRO = (op->Flags&FIELD_ReadOnly);
    op->Flags &= ~FIELD_ReadOnly;
    op->RequestAddressOf();
    op->Flags |= wasRO;
  }
}


//==========================================================================
//
//  VDelegateToBool::SyntaxCopy
//
//==========================================================================
VExpression *VDelegateToBool::SyntaxCopy () {
  auto res = new VDelegateToBool();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDelegateToBool::Emit
//
//==========================================================================
VExpression *VDelegateToBool::DoResolve (VEmitContext &ec) {
  if (!opResolved) {
    opResolved = true;
    if (op) op = op->Resolve(ec);
    if (op) {
      vint32 wasRO = (op->Flags&FIELD_ReadOnly);
      op->Flags &= ~FIELD_ReadOnly;
      op->RequestAddressOf();
      op->Flags |= wasRO;
    }
  }
  if (!op) { delete this; return nullptr; }
  if (op->Type.Type != TYPE_Delegate) {
    ParseError(Loc, "cannot convert type `%s` to `bool`", *op->Type.GetName());
    delete this;
    return nullptr;
  }
  Type = TYPE_Int;
  return this;
}


//==========================================================================
//
//  VDelegateToBool::Emit
//
//==========================================================================
void VDelegateToBool::Emit (VEmitContext &ec) {
  op->Emit(ec);
  ec.AddStatement(OPC_PushPointedPtr, Loc);
  ec.AddStatement(OPC_PtrToBool, Loc);
}


//==========================================================================
//
//  VDelegateToBool::toString
//
//==========================================================================
VStr VDelegateToBool::toString () const {
  return VStr("bool(")+e2s(op)+")";
}



//==========================================================================
//
//  VStringToBool::VStringToBool
//
//==========================================================================
VStringToBool::VStringToBool (VExpression *AOp, bool aOpResolved)
  : VCastExpressionBase(AOp, aOpResolved)
{
  Type = TYPE_Int;
}


//==========================================================================
//
//  VStringToBool::SyntaxCopy
//
//==========================================================================
VExpression *VStringToBool::SyntaxCopy () {
  auto res = new VStringToBool();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VStringToBool::DoResolve
//
//==========================================================================
VExpression *VStringToBool::DoResolve (VEmitContext &ec) {
  if (!opResolved) { opResolved = true; if (op) op = op->Resolve(ec); }
  if (!op) { delete this; return nullptr; }
  if (op->Type.Type != TYPE_String) {
    ParseError(Loc, "cannot convert type `%s` to `bool`", *op->Type.GetName());
    delete this;
    return nullptr;
  }
  if (op->IsStrConst()) {
    // do it inplace
    VStr s = op->GetStrConst(ec.Package);
    VExpression *e = new VIntLiteral((s.length() ? 1 : 0), Loc);
    delete this;
    return e->Resolve(ec);
  }
  Type = TYPE_Int;
  return this;
}


//==========================================================================
//
//  VStringToBool::Emit
//
//==========================================================================
void VStringToBool::Emit (VEmitContext &ec) {
  op->Emit(ec);
  ec.AddStatement(OPC_StrToBool, Loc);
}


//==========================================================================
//
//  VStringToBool::toString
//
//==========================================================================
VStr VStringToBool::toString () const {
  return VStr("bool(")+e2s(op)+")";
}



//==========================================================================
//
//  VNameToBool::VNameToBool
//
//==========================================================================
VNameToBool::VNameToBool (VExpression *AOp, bool aOpResolved)
  : VCastExpressionBase(AOp, aOpResolved)
{
  Type = TYPE_Int;
}


//==========================================================================
//
//  VNameToBool::SyntaxCopy
//
//==========================================================================
VExpression *VNameToBool::SyntaxCopy () {
  auto res = new VNameToBool();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VNameToBool::DoResolve
//
//==========================================================================
VExpression *VNameToBool::DoResolve (VEmitContext &ec) {
  if (!opResolved) { opResolved = true; if (op) op = op->Resolve(ec); }
  if (!op) { delete this; return nullptr; }
  if (op->Type.Type != TYPE_Name) {
    ParseError(Loc, "cannot convert type `%s` to `bool`", *op->Type.GetName());
    delete this;
    return nullptr;
  }
  if (op->IsNameConst()) {
    // do it inplace
    VName n = op->GetNameConst();
    VExpression *e = new VIntLiteral((n != NAME_None ? 1 : 0), Loc);
    delete this;
    return e->Resolve(ec);
  }
  Type = TYPE_Int;
  return this;
}


//==========================================================================
//
//  VNameToBool::Emit
//
//==========================================================================
void VNameToBool::Emit (VEmitContext &ec) {
  op->Emit(ec);
  // no further conversion required
}


//==========================================================================
//
//  VNameToBool::toString
//
//==========================================================================
VStr VNameToBool::toString () const {
  return VStr("bool(")+e2s(op)+")";
}



//==========================================================================
//
//  VFloatToBool::VFloatToBool
//
//==========================================================================
VFloatToBool::VFloatToBool (VExpression *AOp, bool aOpResolved)
  : VCastExpressionBase(AOp, aOpResolved)
{
  Type = TYPE_Int;
}


//==========================================================================
//
//  VFloatToBool::SyntaxCopy
//
//==========================================================================
VExpression *VFloatToBool::SyntaxCopy () {
  auto res = new VFloatToBool();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VFloatToBool::DoResolve
//
//==========================================================================
VExpression *VFloatToBool::DoResolve (VEmitContext &ec) {
  if (!opResolved) { opResolved = true; if (op) op = op->Resolve(ec); }
  if (!op) { delete this; return nullptr; }
  if (op->Type.Type != TYPE_Float) {
    ParseError(Loc, "cannot convert type `%s` to `bool`", *op->Type.GetName());
    delete this;
    return nullptr;
  }
  if (op->IsFloatConst()) {
    // do it inplace
    VExpression *e = new VIntLiteral((op->GetFloatConst() == 0 ? 0 : 1), Loc);
    delete this;
    return e->Resolve(ec);
  }
  Type = TYPE_Int;
  return this;
}


//==========================================================================
//
//  VFloatToBool::Emit
//
//==========================================================================
void VFloatToBool::Emit (VEmitContext &ec) {
  op->Emit(ec);
  ec.AddStatement(OPC_FloatToBool, Loc);
}


//==========================================================================
//
//  VFloatToBool::toString
//
//==========================================================================
VStr VFloatToBool::toString () const {
  return VStr("bool(")+e2s(op)+")";
}



//==========================================================================
//
//  VVectorToBool::VVectorToBool
//
//==========================================================================
VVectorToBool::VVectorToBool (VExpression *AOp, bool aOpResolved)
  : VCastExpressionBase(AOp, aOpResolved)
{
  Type = TYPE_Int;
}


//==========================================================================
//
//  VVectorToBool::SyntaxCopy
//
//==========================================================================
VExpression *VVectorToBool::SyntaxCopy () {
  auto res = new VVectorToBool();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VVectorToBool::DoResolve
//
//==========================================================================
VExpression *VVectorToBool::DoResolve (VEmitContext &ec) {
  if (!opResolved) { opResolved = true; if (op) op = op->Resolve(ec); }
  if (!op) { delete this; return nullptr; }
  if (op->Type.Type != TYPE_Vector) {
    ParseError(Loc, "cannot convert type `%s` to `bool`", *op->Type.GetName());
    delete this;
    return nullptr;
  }
  if (op->IsConstVectorCtor()) {
    // do it inplace
    TVec v = ((VVector *)op)->GetConstValue();
    VExpression *e = new VIntLiteral((v.x == 0 && v.y == 0 && v.z == 0 ? 0 : 1), Loc);
    delete this;
    return e->Resolve(ec);
  }
  Type = TYPE_Int;
  return this;
}


//==========================================================================
//
//  VVectorToBool::Emit
//
//==========================================================================
void VVectorToBool::Emit (VEmitContext &ec) {
  op->Emit(ec);
  ec.AddStatement(OPC_VectorToBool, Loc);
}


//==========================================================================
//
//  VVectorToBool::toString
//
//==========================================================================
VStr VVectorToBool::toString () const {
  return VStr("bool(")+e2s(op)+")";
}



//==========================================================================
//
//  VPointerToBool::VPointerToBool
//
//==========================================================================
VPointerToBool::VPointerToBool (VExpression *AOp, bool aOpResolved)
  : VCastExpressionBase(AOp, aOpResolved)
{
  Type = TYPE_Int;
}


//==========================================================================
//
//  VPointerToBool::SyntaxCopy
//
//==========================================================================
VExpression *VPointerToBool::SyntaxCopy () {
  auto res = new VPointerToBool();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VPointerToBool::DoResolve
//
//==========================================================================
VExpression *VPointerToBool::DoResolve (VEmitContext &ec) {
  if (!opResolved) { opResolved = true; if (op) op = op->Resolve(ec); }
  if (!op) { delete this; return nullptr; }
  // do it in-place for pointers
  switch (op->Type.Type) {
    case TYPE_Pointer:
      if (op->IsNullLiteral()) {
        VExpression *e = new VIntLiteral(0, Loc);
        delete this;
        return e->Resolve(ec);
      }
      break;
    case TYPE_Reference:
    case TYPE_Class:
    case TYPE_State:
      if (op->IsNoneLiteral()) {
        VExpression *e = new VIntLiteral(0, Loc);
        delete this;
        return e->Resolve(ec);
      }
      break;
    default:
      ParseError(Loc, "cannot convert type `%s` to `bool`", *op->Type.GetName());
      delete this;
      return nullptr;
  }
  Type = TYPE_Int;
  return this;
}


//==========================================================================
//
//  VPointerToBool::Emit
//
//==========================================================================
void VPointerToBool::Emit (VEmitContext &ec) {
  op->Emit(ec);
  ec.AddStatement(OPC_PtrToBool, Loc);
}


//==========================================================================
//
//  VPointerToBool::toString
//
//==========================================================================
VStr VPointerToBool::toString () const {
  return VStr("bool(")+e2s(op)+")";
}



//==========================================================================
//
//  VScalarToFloat::VScalarToFloat
//
//==========================================================================
VScalarToFloat::VScalarToFloat (VExpression *AOp, bool aOpResolved)
  : VCastExpressionBase(AOp, aOpResolved)
{
  Type = TYPE_Float;
}


//==========================================================================
//
//  VScalarToFloat::SyntaxCopy
//
//==========================================================================
VExpression *VScalarToFloat::SyntaxCopy () {
  auto res = new VScalarToFloat();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VScalarToFloat::DoResolve
//
//==========================================================================
VExpression *VScalarToFloat::DoResolve (VEmitContext &ec) {
  if (!opResolved) { opResolved = true; if (op) op = op->Resolve(ec); }
  if (!op) { delete this; return nullptr; }
  switch (op->Type.Type) {
    case TYPE_Int:
    case TYPE_Byte:
    case TYPE_Bool:
      if (op->IsIntConst()) {
        VExpression *e = new VFloatLiteral((float)op->GetIntConst(), op->Loc);
        delete this;
        return e->Resolve(ec);
      }
      break;
    case TYPE_Float:
      if (op->IsFloatConst()) {
        VExpression *e = op;
        op = nullptr;
        delete this;
        return e;
      }
      break;
    default:
      ParseError(Loc, "cannot convert type `%s` to `float`", *op->Type.GetName());
      delete this;
      return nullptr;
  }
  return this;
}


//==========================================================================
//
//  VScalarToFloat::Emit
//
//==========================================================================
void VScalarToFloat::Emit (VEmitContext &ec) {
  op->Emit(ec);
  switch (op->Type.Type) {
    case TYPE_Int:
    case TYPE_Byte:
    case TYPE_Bool:
      ec.AddStatement(OPC_IntToFloat, Loc);
      break;
    case TYPE_Float: // nothing to do
      break;
    default:
      ParseError(Loc, "Internal compiler error (VScalarToFloat::Emit)");
  }
}


//==========================================================================
//
//  VScalarToFloat::toString
//
//==========================================================================
VStr VScalarToFloat::toString () const {
  return VStr("float(")+e2s(op)+")";
}



//==========================================================================
//
//  VScalarToInt::VScalarToInt
//
//==========================================================================
VScalarToInt::VScalarToInt (VExpression *AOp, bool aOpResolved)
  : VCastExpressionBase(AOp, aOpResolved)
{
  Type = TYPE_Int;
}


//==========================================================================
//
//  VScalarToInt::SyntaxCopy
//
//==========================================================================
VExpression *VScalarToInt::SyntaxCopy () {
  auto res = new VScalarToInt();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VScalarToInt::DoResolve
//
//==========================================================================
VExpression *VScalarToInt::DoResolve (VEmitContext &ec) {
  if (!opResolved) { opResolved = true; if (op) op = op->Resolve(ec); }
  if (!op) { delete this; return nullptr; }
  switch (op->Type.Type) {
    case TYPE_Int:
    case TYPE_Byte:
    case TYPE_Bool:
      if (op->IsIntConst()) {
        VExpression *e = op;
        op = nullptr;
        delete this;
        return e;
      }
      break;
    case TYPE_Float:
      if (op->IsFloatConst()) {
        VExpression *e = new VIntLiteral((vint32)op->GetFloatConst(), op->Loc);
        delete this;
        return e->Resolve(ec);
      }
      break;
    default:
      ParseError(Loc, "cannot convert type `%s` to `int`", *op->Type.GetName());
      delete this;
      return nullptr;
  }
  return this;
}


//==========================================================================
//
//  VScalarToInt::Emit
//
//==========================================================================
void VScalarToInt::Emit (VEmitContext &ec) {
  op->Emit(ec);
  switch (op->Type.Type) {
    case TYPE_Int:
    case TYPE_Byte:
    case TYPE_Bool:
      // nothing to do
      break;
    case TYPE_Float:
      ec.AddStatement(OPC_FloatToInt, Loc);
      break;
    default:
      ParseError(Loc, "Internal compiler error (VScalarToInt::Emit)");
      return;
  }
}


//==========================================================================
//
//  VScalarToInt::toString
//
//==========================================================================
VStr VScalarToInt::toString () const {
  return VStr("int(")+e2s(op)+")";
}



//==========================================================================
//
//  VCastToString::VCastToString
//
//==========================================================================
VCastToString::VCastToString (VExpression *AOp, bool aOpResolved)
  : VCastExpressionBase(AOp, aOpResolved)
{
  Type = TYPE_String;
}


//==========================================================================
//
//  VCastToString::SyntaxCopy
//
//==========================================================================
VExpression *VCastToString::SyntaxCopy () {
  auto res = new VCastToString();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VCastToString::DoResolve
//
//==========================================================================
VExpression *VCastToString::DoResolve (VEmitContext &ec) {
  if (!opResolved) { opResolved = true; if (op) op = op->Resolve(ec); }
  if (!op) { delete this; return nullptr; }
  switch (op->Type.Type) {
    case TYPE_String:
      break;
    case TYPE_Name:
      if (op->IsNameConst()) {
        // do it inplace
        VStr ns = VStr(*op->GetNameConst());
        int val = ec.Package->FindString(*ns);
        VExpression *e = (new VStringLiteral(ns, val, Loc))->Resolve(ec);
        delete this;
        return e->Resolve(ec);
      }
      break;
    default:
      ParseError(Loc, "cannot convert type `%s` to `string`", *op->Type.GetName());
      delete this;
      return nullptr;
  }
  return this;
}


//==========================================================================
//
//  VCastToString::Emit
//
//==========================================================================
void VCastToString::Emit (VEmitContext &ec) {
  if (!op) return;
  op->Emit(ec);
  switch (op->Type.Type) {
    case TYPE_String:
      break;
    case TYPE_Name:
      ec.AddStatement(OPC_NameToStr, Loc);
      break;
    default:
      ParseError(Loc, "cannot convert type `%s` to `string` (the thing that should not be)", *op->Type.GetName());
  }
}


//==========================================================================
//
//  VCastToString::toString
//
//==========================================================================
VStr VCastToString::toString () const {
  return VStr("string(")+e2s(op)+")";
}



//==========================================================================
//
//  VCastToName::VCastToName
//
//==========================================================================
VCastToName::VCastToName (VExpression *AOp, bool aOpResolved)
  : VCastExpressionBase(AOp, aOpResolved)
{
  Type = TYPE_Name;
}


//==========================================================================
//
//  VCastToName::SyntaxCopy
//
//==========================================================================
VExpression *VCastToName::SyntaxCopy () {
  auto res = new VCastToName();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VCastToName::DoResolve
//
//==========================================================================
VExpression *VCastToName::DoResolve (VEmitContext &ec) {
  if (!opResolved) { opResolved = true; if (op) op = op->Resolve(ec); }
  if (!op) { delete this; return nullptr; }
  switch (op->Type.Type) {
    case TYPE_String:
      if (op->IsStrConst()) {
        // do it inplace
        VStr s = op->GetStrConst(ec.Package);
        VExpression *e = (new VNameLiteral(VName(*s), Loc))->Resolve(ec);
        delete this;
        return e->Resolve(ec);
      }
      break;
    case TYPE_Name:
      break;
    default:
      ParseError(Loc, "cannot convert type `%s` to `name`", *op->Type.GetName());
      delete this;
      return nullptr;
  }
  return this;
}


//==========================================================================
//
//  VCastToName::Emit
//
//==========================================================================
void VCastToName::Emit (VEmitContext &ec) {
  if (!op) return;
  op->Emit(ec);
  switch (op->Type.Type) {
    case TYPE_String:
      ec.AddStatement(OPC_StrToName, Loc);
      break;
    case TYPE_Name:
      break;
    default:
      ParseError(Loc, "cannot convert type `%s` to `name` (the thing that should not be)", *op->Type.GetName());
  }
}


//==========================================================================
//
//  VCastToName::toString
//
//==========================================================================
VStr VCastToName::toString () const {
  return VStr("name(")+e2s(op)+")";
}



//==========================================================================
//
//  VDynamicCast::VDynamicCast
//
//==========================================================================
VDynamicCast::VDynamicCast (VClass *AClass, VExpression *AOp, const TLocation &ALoc)
  : VCastExpressionBase(ALoc, false)
  , Class(AClass)
{
  op = AOp;
}


//==========================================================================
//
//  VDynamicCast::SyntaxCopy
//
//==========================================================================
VExpression *VDynamicCast::SyntaxCopy () {
  auto res = new VDynamicCast();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDynamicCast::DoRestSyntaxCopyTo
//
//==========================================================================
void VDynamicCast::DoSyntaxCopyTo (VExpression *e) {
  VCastExpressionBase::DoSyntaxCopyTo(e);
  auto res = (VDynamicCast *)e;
  res->Class = Class;
}


//==========================================================================
//
//  VDynamicCast::DoResolve
//
//==========================================================================
VExpression *VDynamicCast::DoResolve (VEmitContext &ec) {
  if (!opResolved) { opResolved = true; if (op) op = op->Resolve(ec); }
  if (!op) { delete this; return nullptr; }
  if (op->Type.Type != TYPE_Reference) {
    ParseError(Loc, "Bad expression, class reference required");
    delete this;
    return nullptr;
  }
  Type = VFieldType(Class);
  return this;
}


//==========================================================================
//
//  VDynamicCast::Emit
//
//==========================================================================
void VDynamicCast::Emit (VEmitContext &ec) {
  op->Emit(ec);
  ec.AddStatement(OPC_DynamicCast, Class, Loc);
}


//==========================================================================
//
//  VDynamicCast::toString
//
//==========================================================================
VStr VDynamicCast::toString () const {
  return (Class ? VStr(*Class->Name) : e2s(nullptr))+"("+e2s(op)+")";
}



//==========================================================================
//
//  VDynamicClassCast::VDynamicClassCast
//
//==========================================================================
VDynamicClassCast::VDynamicClassCast (VName AClassName, VExpression *AOp, const TLocation &ALoc)
  : VCastExpressionBase(ALoc, false)
  , ClassName(AClassName)
{
  op = AOp;
}


//==========================================================================
//
//  VDynamicClassCast::SyntaxCopy
//
//==========================================================================
VExpression *VDynamicClassCast::SyntaxCopy () {
  auto res = new VDynamicClassCast();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDynamicCast::DoRestSyntaxCopyTo
//
//==========================================================================
void VDynamicClassCast::DoSyntaxCopyTo (VExpression *e) {
  VCastExpressionBase::DoSyntaxCopyTo(e);
  auto res = (VDynamicClassCast *)e;
  res->ClassName = ClassName;
}


//==========================================================================
//
//  VDynamicClassCast::DoResolve
//
//==========================================================================
VExpression *VDynamicClassCast::DoResolve (VEmitContext &ec) {
  if (!opResolved) { opResolved = true; if (op) op = op->Resolve(ec); }
  if (!op) { delete this; return nullptr; }

  if (op->Type.Type != TYPE_Class) {
    ParseError(Loc, "Bad expression, class type required");
    delete this;
    return nullptr;
  }

  Type = TYPE_Class;
  Type.Class = VMemberBase::StaticFindClass(ClassName);
  if (!Type.Class) {
    ParseError(Loc, "No such class %s", *ClassName);
    delete this;
    return nullptr;
  }

  return this;
}


//==========================================================================
//
//  VDynamicClassCast::Emit
//
//==========================================================================
void VDynamicClassCast::Emit (VEmitContext &ec) {
  op->Emit(ec);
  ec.AddStatement(OPC_DynamicClassCast, Type.Class, Loc);
}


//==========================================================================
//
//  VDynamicClassCast::toString
//
//==========================================================================
VStr VDynamicClassCast::toString () const {
  return VStr(*ClassName)+"("+e2s(op)+")";
}



//==========================================================================
//
//  VStructPtrCast::VStructPtrCast
//
//==========================================================================
VStructPtrCast::VStructPtrCast (VExpression *aop, VExpression *adest, const TLocation &aloc)
  : VCastExpressionBase(aloc, false)
  , dest(adest)
{
  op = aop;
}


//==========================================================================
//
//  VStructPtrCast::~VStructPtrCast
//
//==========================================================================
VStructPtrCast::~VStructPtrCast () {
  delete dest; dest = nullptr;
}


//==========================================================================
//
//  VStructPtrCast::SyntaxCopy
//
//==========================================================================
VExpression *VStructPtrCast::SyntaxCopy () {
  auto res = new VStructPtrCast();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VStructPtrCast::VStructPtrCast
//
//==========================================================================
void VStructPtrCast::DoSyntaxCopyTo (VExpression *e) {
  VCastExpressionBase::DoSyntaxCopyTo(e);
  auto res = (VStructPtrCast *)e;
  res->dest = (dest ? dest->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VStructPtrCast::DoResolve
//
//==========================================================================
VExpression *VStructPtrCast::DoResolve (VEmitContext &ec) {
  if (!opResolved) { opResolved = true; if (op) op = op->Resolve(ec); }
  if (dest) dest = dest->ResolveAsType(ec);
  if (!op || !dest) { delete this; return nullptr; }
  if (op->Type.Type != TYPE_Pointer || dest->Type.Type != TYPE_Pointer) {
    ParseError(Loc, "Casts are supported only for pointers yet");
    delete this;
    return nullptr;
  }
  //FIXME: this is unsafe!
  Type = dest->Type;
  return this;
}


//==========================================================================
//
//  VStructPtrCast::Emit
//
//==========================================================================
void VStructPtrCast::Emit (VEmitContext &ec) {
  if (op) op->Emit(ec);
}


//==========================================================================
//
//  VStructPtrCast::toString
//
//==========================================================================
VStr VStructPtrCast::toString () const {
  return VStr("cast(")+e2s(dest)+"("+e2s(op)+")";
}
