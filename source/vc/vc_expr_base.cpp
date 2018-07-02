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
//  VExpression::VExpression
//
//==========================================================================
VExpression::VExpression (const TLocation &ALoc)
  : Type(TYPE_Void)
  , RealType(TYPE_Void)
  , Flags(0)
  , Loc(ALoc)
{
}


//==========================================================================
//
//  VExpression::~VExpression
//
//==========================================================================
VExpression::~VExpression () {
}


//==========================================================================
//
//  VExpression::DoRestSyntaxCopyTo
//
//==========================================================================
//#include <typeinfo>
void VExpression::DoSyntaxCopyTo (VExpression *e) {
  //fprintf(stderr, "  ***VExpression::DoSyntaxCopyTo for `%s`\n", typeid(*e).name());
  e->Type = Type;
  e->RealType = RealType;
  e->Flags = Flags;
  e->Loc = Loc;
}


//==========================================================================
//
//  VExpression::Resolve
//
//==========================================================================
VExpression *VExpression::Resolve (VEmitContext &ec) {
  VExpression *e = DoResolve(ec);
  return e;
}


//==========================================================================
//
//  VExpression::ResolveBoolean
//
//==========================================================================
VExpression *VExpression::ResolveBoolean (VEmitContext &ec) {
  VExpression *e = Resolve(ec);
  if (!e) return nullptr;
  switch (e->Type.Type) {
    case TYPE_Int:
    case TYPE_Byte:
    case TYPE_Bool:
      break;
    case TYPE_Float:
      e = (new VFloatToBool(e, true))->Resolve(ec);
      break;
    case TYPE_Name:
      e = (new VNameToBool(e, true))->Resolve(ec);
      break;
    case TYPE_Pointer:
    case TYPE_Reference:
    case TYPE_Class:
    case TYPE_State:
      e = (new VPointerToBool(e, true))->Resolve(ec);
      break;
    case TYPE_String:
      e = (new VStringToBool(e, true))->Resolve(ec);
      break;
    case TYPE_Delegate:
      e = (new VDelegateToBool(e, true))->Resolve(ec);
      break;
    case TYPE_Vector:
      e = (new VVectorToBool(e, true))->Resolve(ec);
      break;
    default:
      ParseError(e->Loc, "Expression type mismatch, boolean expression expected");
      delete e;
      return nullptr;
  }
  return e;
}


//==========================================================================
//
//  VExpression::ResolveFloat
//
//==========================================================================
VExpression *VExpression::ResolveFloat (VEmitContext &ec) {
  VExpression *e = Resolve(ec);
  if (!e) return nullptr;
  switch (e->Type.Type) {
    case TYPE_Int:
    case TYPE_Byte:
    //case TYPE_Bool:
      e = (new VScalarToFloat(e, true))->Resolve(ec);
      break;
    case TYPE_Float:
      break;
    default:
      ParseError(e->Loc, "Expression type mismatch, float expression expected");
      delete e;
      return nullptr;
  }
  return e;
}


//==========================================================================
//
//  VExpression::CoerceToFloat
//
//  Expression MUST be already resolved here.
//
//==========================================================================
VExpression *VExpression::CoerceToFloat () {
  if (Type.Type == TYPE_Float) return this; // nothing to do
  if (Type.Type == TYPE_Int || Type.Type == TYPE_Byte) {
    if (IsIntConst()) {
      VExpression *e = new VFloatLiteral((float)GetIntConst(), Loc);
      delete this;
      return e; // no need to resolve it
    }
    //HACK: `VScalarToFloat()` resolver does nothing special (except constant folding),
    //      so we can skip resolving here
    return new VScalarToFloat(this, true);
  }
  ParseError(Loc, "Expression type mismatch, float expression expected");
  delete this;
  return nullptr;
}


//==========================================================================
//
//  VExpression::ResolveAsType
//
//==========================================================================
VTypeExpr *VExpression::ResolveAsType (VEmitContext &) {
  ParseError(Loc, "Invalid type expression");
  delete this;
  return nullptr;
}


//==========================================================================
//
//  VExpression::ResolveAssignmentTarget
//
//==========================================================================
VExpression *VExpression::ResolveAssignmentTarget (VEmitContext &ec) {
  return Resolve(ec);
}


//==========================================================================
//
//  VExpression::ResolveAssignmentValue
//
//==========================================================================
VExpression *VExpression::ResolveAssignmentValue (VEmitContext &ec) {
  return Resolve(ec);
}


//==========================================================================
//
//  VExpression::ResolveIterator
//
//==========================================================================
VExpression *VExpression::ResolveIterator (VEmitContext &) {
  ParseError(Loc, "Iterator method expected");
  delete this;
  return nullptr;
}


//==========================================================================
//
//  VExpression::ResolveCompleteAssign
//
//==========================================================================
VExpression *VExpression::ResolveCompleteAssign (VEmitContext &ec, VExpression *val, bool &resolved) {
  // do nothing
  return this;
}


//==========================================================================
//
//  VExpression::RequestAddressOf
//
//==========================================================================
void VExpression::RequestAddressOf () {
  ParseError(Loc, "Bad address operation");
}


//==========================================================================
//
//  VExpression::EmitBranchable
//
//==========================================================================
void VExpression::EmitBranchable (VEmitContext &ec, VLabel Lbl, bool OnTrue) {
  Emit(ec);
  if (OnTrue) {
    ec.AddStatement(OPC_IfGoto, Lbl, Loc);
  } else {
    ec.AddStatement(OPC_IfNotGoto, Lbl, Loc);
  }
}


//==========================================================================
//
//  VExpression::EmitPushPointedCode
//
//==========================================================================
void VExpression::EmitPushPointedCode (VFieldType type, VEmitContext &ec) {
  ec.EmitPushPointedCode(type, Loc);
}


//==========================================================================
//
//  VExpression::ResolveToIntLiteralEx
//
//  this resolves one-char strings and names to int literals too
//
//==========================================================================
VExpression *VExpression::ResolveToIntLiteralEx (VEmitContext &ec, bool allowFloatTrunc) {
  VExpression *res = Resolve(ec);
  if (!res) return nullptr; // we are dead anyway

  // easy case
  if (res->IsIntConst()) return res;

  // truncate floats?
  if (allowFloatTrunc && res->IsFloatConst()) {
    VExpression *e = new VIntLiteral((int)res->GetFloatConst(), res->Loc);
    delete res;
    return e->Resolve(ec);
  }

  // one-char string?
  if (res->IsStrConst()) {
    VStr s = res->GetStrConst(ec.Package);
    if (s.length() == 1) {
      VExpression *e = new VIntLiteral((vuint8)s[0], res->Loc);
      delete res;
      return e->Resolve(ec);
    }
  }

  // one-char name?
  if (res->IsNameConst()) {
    VStr s(*res->GetNameConst());
    if (s.length() == 1) {
      VExpression *e = new VIntLiteral((vuint8)s[0], res->Loc);
      delete res;
      return e->Resolve(ec);
    }
  }

  ParseError(res->Loc, "Integer constant expected");
  delete res;
  return nullptr;
}


// ////////////////////////////////////////////////////////////////////////// //
// IsXXX
bool VExpression::AddDropResult () { return false; }
bool VExpression::IsValidTypeExpression () const { return false; }
bool VExpression::IsIntConst () const { return false; }
bool VExpression::IsFloatConst () const { return false; }
bool VExpression::IsStrConst () const { return false; }
bool VExpression::IsNameConst () const { return false; }
vint32 VExpression::GetIntConst () const { ParseError(Loc, "Integer constant expected"); return 0; }
float VExpression::GetFloatConst () const { ParseError(Loc, "Float constant expected"); return 0.0; }
VStr VExpression::GetStrConst (VPackage *) const { ParseError(Loc, "String constant expected"); return VStr(); }
VName VExpression::GetNameConst () const { ParseError(Loc, "Name constant expected"); return NAME_None; }
bool VExpression::IsNoneLiteral () const { return false; }
bool VExpression::IsNullLiteral () const { return false; }
bool VExpression::IsDefaultObject () const { return false; }
bool VExpression::IsPropertyAssign () const { return false; }
bool VExpression::IsDynArraySetNum () const { return false; }
bool VExpression::IsDecorateSingleName () const { return false; }
bool VExpression::IsLocalVarDecl () const { return false; }
bool VExpression::IsLocalVarExpr () const { return false; }
bool VExpression::IsAssignExpr () const { return false; }
bool VExpression::IsBinaryMath () const { return false; }
bool VExpression::IsSingleName () const { return false; }
bool VExpression::IsDoubleName () const { return false; }
bool VExpression::IsDotField () const { return false; }
bool VExpression::IsMarshallArg () const { return false; }
bool VExpression::IsRefArg () const { return false; }
bool VExpression::IsOutArg () const { return false; }
bool VExpression::IsOptMarshallArg () const { return false; }
bool VExpression::IsAnyInvocation () const { return false; }
bool VExpression::IsLLInvocation () const { return false; }
bool VExpression::IsTypeExpr () const { return false; }
bool VExpression::IsAutoTypeExpr () const { return false; }
bool VExpression::IsSimpleType () const { return false; }
bool VExpression::IsReferenceType () const { return false; }
bool VExpression::IsClassType () const { return false; }
bool VExpression::IsPointerType () const { return false; }
bool VExpression::IsAnyArrayType () const { return false; }
bool VExpression::IsStaticArrayType () const { return false; }
bool VExpression::IsDynamicArrayType () const { return false; }
bool VExpression::IsDelegateType () const { return false; }
bool VExpression::IsSliceType () const { return false; }
bool VExpression::IsVectorCtor () const { return false; }
bool VExpression::IsConstVectorCtor () const { return false; }

VStr VExpression::toString () const { return VStr("<VExpression::")+shitppTypeNameObj(*this)+":no-toString>"; }


// ////////////////////////////////////////////////////////////////////////// //
// memory allocation
vuint32 VExpression::TotalMemoryUsed = 0;
vuint32 VExpression::CurrMemoryUsed = 0;
vuint32 VExpression::PeakMemoryUsed = 0;
vuint32 VExpression::TotalMemoryFreed = 0;
bool VExpression::InCompilerCleanup = false;


void *VExpression::operator new (size_t size) {
  //if (size == 0) size = 1;
  size_t *res = (size_t *)malloc(size+sizeof(size_t));
  if (!res) { fprintf(stderr, "\nFATAL: OUT OF MEMORY!\n"); *(int *)0 = 0; }
  *res = size;
  ++res;
  if (size) memset(res, 0, size);
  TotalMemoryUsed += (vuint32)size;
  CurrMemoryUsed += (vuint32)size;
  if (PeakMemoryUsed < CurrMemoryUsed) PeakMemoryUsed = CurrMemoryUsed;
  return res;
}


void *VExpression::operator new[] (size_t size) {
  //if (size == 0) size = 1;
  size_t *res = (size_t *)malloc(size+sizeof(size_t));
  if (!res) { fprintf(stderr, "\nFATAL: OUT OF MEMORY!\n"); *(int *)0 = 0; }
  *res = size;
  ++res;
  if (size) memset(res, 0, size);
  TotalMemoryUsed += (vuint32)size;
  CurrMemoryUsed += (vuint32)size;
  if (PeakMemoryUsed < CurrMemoryUsed) PeakMemoryUsed = CurrMemoryUsed;
  return res;
}


void VExpression::operator delete (void *p) {
  if (p) {
    if (InCompilerCleanup) TotalMemoryFreed += (vuint32)*((size_t *)p-1);
    CurrMemoryUsed -= (vuint32)*((size_t *)p-1);
    free(((size_t *)p-1));
  }
}


void VExpression::operator delete[] (void *p) {
  if (p) {
    if (InCompilerCleanup) TotalMemoryFreed += (vuint32)*((size_t *)p-1);
    CurrMemoryUsed -= (vuint32)*((size_t *)p-1);
    free(((size_t *)p-1));
  }
}
