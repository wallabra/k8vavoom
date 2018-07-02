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
VCastExpressionBase::VCastExpressionBase (VExpression *AOp) : VExpression(AOp->Loc), op(AOp) {}
VCastExpressionBase::VCastExpressionBase (const TLocation &ALoc) : VExpression(ALoc), op(nullptr) {}
VCastExpressionBase::~VCastExpressionBase () { if (op) { delete op; op = nullptr; } }

void VCastExpressionBase::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VCastExpressionBase *)e;
  res->op = (op ? op->SyntaxCopy() : nullptr);
}

VExpression *VCastExpressionBase::DoResolve (VEmitContext &) { return this; }


//==========================================================================
//
//  VDelegateToBool::VDelegateToBool
//
//==========================================================================
VDelegateToBool::VDelegateToBool (VExpression *AOp) : VCastExpressionBase(AOp) {
  Type = TYPE_Int;
  op->RequestAddressOf();
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
void VDelegateToBool::Emit (VEmitContext &ec) {
  op->Emit(ec);
  ec.AddStatement(OPC_PushPointedPtr, Loc);
  ec.AddStatement(OPC_PtrToBool, Loc);
}


//==========================================================================
//
//  VStringToBool::VStringToBool
//
//==========================================================================
VStringToBool::VStringToBool (VExpression *AOp) : VCastExpressionBase(AOp) {
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
  if (!op) return nullptr;
  op = op->Resolve(ec);
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
    delete op;
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
//  VNameToBool::VNameToBool
//
//==========================================================================
VNameToBool::VNameToBool (VExpression *AOp) : VCastExpressionBase(AOp) {
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
  if (!op) return nullptr;
  op = op->Resolve(ec);
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
    delete op;
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
//  VFloatToBool::VFloatToBool
//
//==========================================================================
VFloatToBool::VFloatToBool (VExpression *AOp) : VCastExpressionBase(AOp) {
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
  if (!op) return nullptr;
  op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }
  if (op->Type.Type != TYPE_Float) {
    ParseError(Loc, "cannot convert type `%s` to `bool`", *op->Type.GetName());
    delete this;
    return nullptr;
  }
  if (op->IsFloatConst()) {
    // do it inplace
    VExpression *e = new VIntLiteral((op->GetFloatConst() == 0 ? 0 : 1), Loc);
    delete op;
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
//  VVectorToBool::VVectorToBool
//
//==========================================================================
VVectorToBool::VVectorToBool (VExpression *AOp) : VCastExpressionBase(AOp) {
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
  if (!op) return nullptr;
  op = op->Resolve(ec);
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
    delete op;
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
//  VPointerToBool::VPointerToBool
//
//==========================================================================
VPointerToBool::VPointerToBool (VExpression *AOp) : VCastExpressionBase(AOp) {
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
  if (!op) return nullptr;
  op = op->Resolve(ec);
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
//  VScalarToFloat::VScalarToFloat
//
//==========================================================================
VScalarToFloat::VScalarToFloat (VExpression *AOp) : VCastExpressionBase(AOp) {
  // convert it in-place
  if (op && op->IsIntConst()) {
    //printf("*** IN-PLACE CONVERSION OF %d\n", op->GetIntConst());
    VExpression *lit = new VFloatLiteral((float)op->GetIntConst(), op->Loc);
    delete op;
    op = lit; // op is resolved here, but literal resolves to itself, so it is ok
  }
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
  if (!op) return nullptr;
  op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }
  //fprintf(stderr, "VScalarToFloat::DoResolve! (%s)\n", *shitppTypeNameObj(*op));
  switch (op->Type.Type) {
    case TYPE_Int:
    case TYPE_Byte:
    case TYPE_Bool:
      if (op->IsIntConst()) {
        VExpression *lit = new VFloatLiteral((float)op->GetIntConst(), op->Loc);
        delete op;
        op = lit->Resolve(ec); // just in case
        if (!op) { delete this; return nullptr; }
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
//  VScalarToInt::VScalarToInt
//
//==========================================================================
VScalarToInt::VScalarToInt (VExpression *AOp) : VCastExpressionBase(AOp) {
  // convert it in-place
  if (op && op->IsFloatConst()) {
    VExpression *lit = new VIntLiteral((vint32)op->GetFloatConst(), op->Loc);
    delete op;
    op = lit; // op is resolved here, but literal resolves to itself, so it is ok
  }
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
  //printf("VScalarToInt::DoResolve!\n");
  if (!op) return nullptr;
  op = op->Resolve(ec);
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
        VExpression *lit = new VIntLiteral((vint32)op->GetFloatConst(), op->Loc);
        delete op;
        op = lit->Resolve(ec); // just in case
        if (!op) { delete this; return nullptr; }
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
//  VCastToString::VCastToString
//
//==========================================================================
VCastToString::VCastToString (VExpression *AOp) : VCastExpressionBase(AOp) {
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
  if (!op) return nullptr;
  op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }

  switch (op->Type.Type) {
    case TYPE_String:
      break;
    case TYPE_Name:
      if (op->IsNameConst()) {
        // do it inplace
        int val = ec.Package->FindString(*op->GetNameConst());
        VExpression *e = (new VStringLiteral(val, Loc))->Resolve(ec);
        delete op;
        op = e;
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
//  VCastToName::VCastToName
//
//==========================================================================
VCastToName::VCastToName (VExpression *AOp) : VCastExpressionBase(AOp) {
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
  if (!op) return nullptr;
  op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }

  switch (op->Type.Type) {
    case TYPE_String:
      if (op->IsStrConst()) {
        // do it inplace
        VStr s = op->GetStrConst(ec.Package);
        VExpression *e = (new VNameLiteral(VName(*s), Loc))->Resolve(ec);
        delete op;
        op = e;
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
//  VDynamicCast::VDynamicCast
//
//==========================================================================

VDynamicCast::VDynamicCast (VClass *AClass, VExpression *AOp, const TLocation &ALoc)
  : VCastExpressionBase(ALoc)
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
  if (op) op = op->Resolve(ec);
  if (!op) {
    delete this;
    return nullptr;
  }

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
//  VDynamicClassCast::VDynamicClassCast
//
//==========================================================================

VDynamicClassCast::VDynamicClassCast (VName AClassName, VExpression *AOp, const TLocation &ALoc)
  : VCastExpressionBase(ALoc)
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
  if (op) op = op->Resolve(ec);
  if (!op) {
    delete this;
    return nullptr;
  }

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
//  VStructPtrCast::VStructPtrCast
//
//==========================================================================
VStructPtrCast::VStructPtrCast (VExpression *aop, VExpression *adest, const TLocation &aloc)
  : VCastExpressionBase(aloc)
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
  if (op) op = op->Resolve(ec);
  if (dest) dest = dest->ResolveAsType(ec);
  if (!op || !dest) { delete this; return nullptr; }
  if (op->Type.Type != TYPE_Pointer || dest->Type.Type != TYPE_Pointer) {
    ParseError(Loc, "Casts are supported only for pointers yet");
    delete this;
    return nullptr;
  }
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
