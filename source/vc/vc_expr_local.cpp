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
//  VLocalDecl::VLocalDecl
//
//==========================================================================
VLocalDecl::VLocalDecl (const TLocation &ALoc) : VExpression(ALoc) {
}


//==========================================================================
//
//  VLocalDecl::~VLocalDecl
//
//==========================================================================
VLocalDecl::~VLocalDecl () {
  for (int i = 0; i < Vars.length(); ++i) {
    if (Vars[i].TypeExpr) { delete Vars[i].TypeExpr; Vars[i].TypeExpr = nullptr; }
    if (Vars[i].Value) { delete Vars[i].Value; Vars[i].Value = nullptr; }
    if (Vars[i].TypeOfExpr) { delete Vars[i].TypeOfExpr; Vars[i].TypeOfExpr = nullptr; }
  }
}


//==========================================================================
//
//  VLocalDecl::SyntaxCopy
//
//==========================================================================
VExpression *VLocalDecl::SyntaxCopy () {
  auto res = new VLocalDecl();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VLocalDecl::DoSyntaxCopyTo
//
//==========================================================================
void VLocalDecl::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VLocalDecl *)e;
  res->Vars.SetNum(Vars.Num());
  for (int f = 0; f < Vars.Num(); ++f) {
    res->Vars[f] = Vars[f];
    if (res->Vars[f].TypeExpr) res->Vars[f].TypeExpr = Vars[f].TypeExpr->SyntaxCopy();
    if (res->Vars[f].Value) res->Vars[f].Value = Vars[f].Value->SyntaxCopy();
    if (res->Vars[f].TypeOfExpr) res->Vars[f].TypeOfExpr = Vars[f].TypeOfExpr->SyntaxCopy();
  }
}


//==========================================================================
//
//  VLocalDecl::DoResolve
//
//==========================================================================
VExpression *VLocalDecl::DoResolve (VEmitContext &ec) {
  Declare(ec);
  return this;
}


//==========================================================================
//
//  VLocalDecl::Emit
//
//==========================================================================
void VLocalDecl::Emit (VEmitContext &ec) {
  EmitInitialisations(ec);
}


//==========================================================================
//
//  VLocalDecl::Declare
//
//==========================================================================
//#include <typeinfo>
void VLocalDecl::Declare (VEmitContext &ec) {
  for (int i = 0; i < Vars.length(); ++i) {
    VLocalEntry &e = Vars[i];

    if (ec.CheckForLocalVar(e.Name) != -1) {
      //VLocalVarDef &loc = ec.GetLocalByIndex(ec.CheckForLocalVar(e.Name));
      //fprintf(stderr, "duplicate '%s'(%d) (old(%d) is at %s:%d)\n", *e.Name, ec.GetCurrCompIndex(), loc.GetCompIndex(), *loc.Loc.GetSource(), loc.Loc.GetLine());
      ParseError(e.Loc, "Redefined identifier `%s`", *e.Name);
    } else {
      //fprintf(stderr, "NEW '%s'(%d) (%s:%d)\n", *e.Name, ec.GetCurrCompIndex(), *e.Loc.GetSource(), e.Loc.GetLine());
    }

    // resolve automatic type
    if (e.TypeExpr->Type.Type == TYPE_Automatic) {
      VExpression *te = (e.Value ? e.Value : e.TypeOfExpr);
      if (!te) { fprintf(stderr, "VC INTERNAL COMPILER ERROR: automatic type without initializer!\n"); *(int*)0 = 0; }
      // resolve type
      auto res = te->SyntaxCopy()->Resolve(ec);
      if (!res) {
        ParseError(e.Loc, "Cannot resolve type for identifier `%s`", *e.Name);
        delete e.TypeExpr; // delete old `automatic` type
        e.TypeExpr = new VTypeExprSimple(TYPE_Void, te->Loc);
      } else {
        //fprintf(stderr, "*** automatic type resolved to `%s`\n", *(res->Type.GetName()));
        delete e.TypeExpr; // delete old `automatic` type
        e.TypeExpr = VTypeExpr::NewTypeExpr(res->Type, te->Loc);
        delete res;
      }
    }

    e.TypeExpr = e.TypeExpr->ResolveAsType(ec);
    if (!e.TypeExpr) continue;

    VFieldType Type = e.TypeExpr->Type;
    if (Type.Type == TYPE_Void || Type.Type == TYPE_Automatic) ParseError(e.TypeExpr->Loc, "Bad variable type for variable `%s`", *e.Name);

    VLocalVarDef &L = ec.AllocLocal(e.Name, Type, e.Loc);
    L.ParamFlags = (e.isRef ? FPARM_Ref : 0);
    //if (e.isRef) fprintf(stderr, "*** <%s:%d> is REF\n", *e.Name, L.ldindex);

    // resolve initialisation
    if (e.Value) {
      L.Visible = false; // hide from initializer expression
      VExpression *op1 = new VLocalVar(L.ldindex, e.Loc);
      e.Value = new VAssignment(VAssignment::Assign, op1, e.Value, e.Loc);
      e.Value = e.Value->Resolve(ec);
      L.Visible = true; // and make it visible again
    }
  }
}


//==========================================================================
//
//  VLocalDecl::EmitInitialisations
//
//==========================================================================
void VLocalDecl::EmitInitialisations (VEmitContext &ec) {
  for (int i = 0; i < Vars.length(); ++i) {
    if (Vars[i].Value) Vars[i].Value->Emit(ec);
  }
}


//==========================================================================
//
//  VLocalDecl::IsLocalVarDecl
//
//==========================================================================
bool VLocalDecl::IsLocalVarDecl () const {
  return true;
}


//==========================================================================
//
//  VLocalVar::VLocalVar
//
//==========================================================================
VLocalVar::VLocalVar (int ANum, const TLocation &ALoc)
  : VExpression(ALoc)
  , num(ANum)
  , AddressRequested(false)
  , PushOutParam(false)
  , locSavedFlags(0)
{
}


//==========================================================================
//
//  VLocalVar::SyntaxCopy
//
//==========================================================================
VExpression *VLocalVar::SyntaxCopy () {
  auto res = new VLocalVar();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VLocalVar::DoSyntaxCopyTo
//
//==========================================================================
void VLocalVar::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VLocalVar *)e;
  res->num = num;
  res->AddressRequested = AddressRequested;
  res->PushOutParam = PushOutParam;
}


//==========================================================================
//
//  VLocalVar::DoResolve
//
//==========================================================================
VExpression *VLocalVar::DoResolve (VEmitContext &ec) {
  VLocalVarDef &loc = ec.GetLocalByIndex(num);
  locSavedFlags = loc.ParamFlags;
  Type = loc.Type;
  RealType = loc.Type;
  if (Type.Type == TYPE_Byte || Type.Type == TYPE_Bool) Type = VFieldType(TYPE_Int);
  PushOutParam = !!(locSavedFlags&(FPARM_Out|FPARM_Ref));
  if (locSavedFlags&FPARM_Const) Flags |= FIELD_ReadOnly;
  return this;
}


//==========================================================================
//
//  VLocalVar::RequestAddressOf
//
//==========================================================================
void VLocalVar::RequestAddressOf () {
  if (Flags&FIELD_ReadOnly) ParseError(Loc, "Tried to assign to a read-only variable");
  if (PushOutParam) {
    PushOutParam = false;
    return;
  }
  if (AddressRequested) ParseError(Loc, "Multiple address of");
  AddressRequested = true;
}


//==========================================================================
//
//  VLocalVar::genLocalValue
//
//==========================================================================
void VLocalVar::genLocalValue (VEmitContext &ec, VLocalVarDef &loc, int xofs) {
  int Ofs = loc.Offset+xofs;
  if (Ofs < 256) {
         if (Ofs == 0) ec.AddStatement(OPC_LocalValue0, Loc);
    else if (Ofs == 1) ec.AddStatement(OPC_LocalValue1, Loc);
    else if (Ofs == 2) ec.AddStatement(OPC_LocalValue2, Loc);
    else if (Ofs == 3) ec.AddStatement(OPC_LocalValue3, Loc);
    else if (Ofs == 4) ec.AddStatement(OPC_LocalValue4, Loc);
    else if (Ofs == 5) ec.AddStatement(OPC_LocalValue5, Loc);
    else if (Ofs == 6) ec.AddStatement(OPC_LocalValue6, Loc);
    else if (Ofs == 7) ec.AddStatement(OPC_LocalValue7, Loc);
    else ec.AddStatement(OPC_LocalValueB, Ofs, Loc);
  } else {
    ec.EmitLocalAddress(Ofs, Loc);
    ec.AddStatement(OPC_PushPointedPtr, Loc);
  }
}


//==========================================================================
//
//  VLocalVar::Emit
//
//==========================================================================
void VLocalVar::Emit (VEmitContext &ec) {
  VLocalVarDef &loc = ec.GetLocalByIndex(num);
  if (AddressRequested) {
    ec.EmitLocalAddress(loc.Offset, Loc);
  } else if (locSavedFlags&(FPARM_Out|FPARM_Ref)) {
    genLocalValue(ec, loc);
    if (PushOutParam) EmitPushPointedCode(loc.Type, ec);
  } else if (loc.Offset < 256) {
    if (loc.Type.Type == TYPE_Bool && loc.Type.BitMask != 1) ParseError(Loc, "Strange local bool mask");
    switch (loc.Type.Type) {
      case TYPE_Int:
      case TYPE_Byte:
      case TYPE_Bool:
      case TYPE_Float:
      case TYPE_Name:
      case TYPE_Pointer:
      case TYPE_Reference:
      case TYPE_Class:
      case TYPE_State:
        genLocalValue(ec, loc, 0);
        break;
      case TYPE_Delegate:
        genLocalValue(ec, loc, 0);
        genLocalValue(ec, loc, 1);
        break;
      case TYPE_Vector:
        ec.AddStatement(OPC_VLocalValueB, loc.Offset, Loc);
        break;
      case TYPE_String:
        ec.AddStatement(OPC_StrLocalValueB, loc.Offset, Loc);
        break;
      default:
        ParseError(Loc, "Invalid operation of this variable type");
        break;
    }
  } else {
    ec.EmitLocalAddress(loc.Offset, Loc);
    EmitPushPointedCode(loc.Type, ec);
  }
}


//==========================================================================
//
//  VLocalVar::IsLocalVarExpr
//
//==========================================================================
bool VLocalVar::IsLocalVarExpr () const {
  return true;
}
