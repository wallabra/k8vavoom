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
    case TYPE_Float:
    case TYPE_Name:
      break;
    case TYPE_Pointer:
    case TYPE_Reference:
    case TYPE_Class:
    case TYPE_State:
      e = new VPointerToBool(e);
      break;
    case TYPE_String:
      e = new VStringToBool(e);
      break;
    case TYPE_Delegate:
      e = new VDelegateToBool(e);
      break;
    default:
      ParseError(Loc, "Expression type mismatch, boolean expression expected");
      delete e;
      e = nullptr;
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
      e = new VScalarToFloat(e);
      break;
    case TYPE_Float:
      break;
    default:
      ParseError(Loc, "Expression type mismatch, float expression expected");
      delete e;
      e = nullptr;
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
  if (Type.Type == TYPE_Int || Type.Type == TYPE_Byte) return new VScalarToFloat(this);
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
  switch (type.Type) {
    case TYPE_Int:
    case TYPE_Float:
    case TYPE_Name:
      ec.AddStatement(OPC_PushPointed, Loc);
      break;
    case TYPE_Byte:
      ec.AddStatement(OPC_PushPointedByte, Loc);
      break;
    case TYPE_Bool:
           if (type.BitMask&0x000000ff) ec.AddStatement(OPC_PushBool0, (int)(type.BitMask), Loc);
      else if (type.BitMask&0x0000ff00) ec.AddStatement(OPC_PushBool1, (int)(type.BitMask>>8), Loc);
      else if (type.BitMask&0x00ff0000) ec.AddStatement(OPC_PushBool2, (int)(type.BitMask>>16), Loc);
      else ec.AddStatement(OPC_PushBool3, (int)(type.BitMask>>24), Loc);
      break;
    case TYPE_Pointer:
    case TYPE_Reference:
    case TYPE_Class:
    case TYPE_State:
      ec.AddStatement(OPC_PushPointedPtr, Loc);
      break;
    case TYPE_Vector:
      ec.AddStatement(OPC_VPushPointed, Loc);
      break;
    case TYPE_String:
      ec.AddStatement(OPC_PushPointedStr, Loc);
      break;
    case TYPE_Delegate:
      ec.AddStatement(OPC_PushPointedDelegate, Loc);
      break;
    case TYPE_SliceArray:
      ec.AddStatement(OPC_PushPointedSlice, Loc);
      break;
    default:
      ParseError(Loc, "Bad push pointed");
      break;
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// IsXXX
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
bool VExpression::AddDropResult () { return false; }
bool VExpression::IsDecorateSingleName () const { return false; }
bool VExpression::IsLocalVarDecl () const { return false; }
bool VExpression::IsLocalVarExpr () const { return false; }
bool VExpression::IsAssignExpr () const { return false; }
bool VExpression::IsBinaryMath () const { return false; }
bool VExpression::IsSingleName () const { return false; }
bool VExpression::IsDotField () const { return false; }
bool VExpression::IsRefArg () const { return false; }
bool VExpression::IsOutArg () const { return false; }
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
