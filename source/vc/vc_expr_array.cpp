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
//  VArrayElement::VArrayElement
//
//==========================================================================
VArrayElement::VArrayElement (VExpression *AOp, VExpression *AInd, const TLocation &ALoc, bool aSkipBounds)
  : VExpression(ALoc)
  , opscopy(nullptr)
  , genStringAssign(false)
  , sval(nullptr)
  , op(AOp)
  , ind(AInd)
  , ind2(nullptr)
  , AddressRequested(false)
  , IsAssign(false)
  , skipBoundsChecking(aSkipBounds)
{
  if (!ind) {
    ParseError(Loc, "Expression expected");
    return;
  }
}


//==========================================================================
//
//  VArrayElement::VArrayElement
//
//==========================================================================
VArrayElement::VArrayElement (VExpression *AOp, VExpression *AInd, VExpression *AInd2, const TLocation &ALoc, bool aSkipBounds)
  : VExpression(ALoc)
  , opscopy(nullptr)
  , genStringAssign(false)
  , sval(nullptr)
  , op(AOp)
  , ind(AInd)
  , ind2(AInd2)
  , AddressRequested(false)
  , IsAssign(false)
  , skipBoundsChecking(aSkipBounds)
{
  if (!ind) {
    ParseError(Loc, "Expression expected");
    return;
  }
}


//==========================================================================
//
//  VArrayElement::~VArrayElement
//
//==========================================================================
VArrayElement::~VArrayElement () {
  opscopy.release();
  if (op) { delete op; op = nullptr; }
  if (ind) { delete ind; ind = nullptr; }
  if (ind2) { delete ind2; ind2 = nullptr; }
  if (sval) { delete sval; sval = nullptr; }
}


//==========================================================================
//
//  VArrayElement::SyntaxCopy
//
//==========================================================================
VExpression *VArrayElement::SyntaxCopy () {
  auto res = new VArrayElement();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VArrayElement::DoSyntaxCopyTo
//
//==========================================================================
void VArrayElement::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VArrayElement *)e;
  res->genStringAssign = genStringAssign;
  res->sval = (sval ? sval->SyntaxCopy() : nullptr);
  res->op = (op ? op->SyntaxCopy() : nullptr);
  res->ind = (ind ? ind->SyntaxCopy() : nullptr);
  res->ind2 = (ind2 ? ind2->SyntaxCopy() : nullptr);
  res->AddressRequested = AddressRequested;
  res->IsAssign = IsAssign;
  res->skipBoundsChecking = skipBoundsChecking;
}


//==========================================================================
//
//  VArrayElement::InternalResolve
//
//==========================================================================
VExpression *VArrayElement::InternalResolve (VEmitContext &ec, bool assTarget) {
  if (!op || !ind) {
    delete this;
    return nullptr;
  }

  // we need a copy for opDollar
  //VExpression *opcopy = op->SyntaxCopy();
  opscopy.assignSyntaxCopy(op);

  op = op->Resolve(ec);

  bool wasInd2 = (ind2 != nullptr);

  if (wasInd2 && op) {
    if (op->Type.Type != TYPE_Array) {
      ParseError(Loc, "Only static arrays can be 2d yet");
      opscopy.release();
      delete this;
      return nullptr;
    }
    if (!op->Type.IsArray2D()) {
      ParseError(Loc, "2d access to 1d array");
      opscopy.release();
      delete this;
      return nullptr;
    }
  } else if (!wasInd2 && op) {
    if (op->Type.Type == TYPE_Array && !op->Type.IsArray1D()) {
      ParseError(Loc, "1d access to 2d array");
      opscopy.release();
      delete this;
      return nullptr;
    }
  }

  VExpression *indcopy = (ind ? ind->SyntaxCopy() : nullptr);
  VExpression *ind2copy = (ind2 ? ind2->SyntaxCopy() : nullptr);

  if (op) {
    // resolve index expression
    auto oldIndArray = ec.SetIndexArray(this);
    resolvingInd2 = false;
    if (ind) ind = ind->Resolve(ec);
    resolvingInd2 = true;
    if (ind2) ind2 = ind2->Resolve(ec);
    ec.SetIndexArray(oldIndArray);
  }

  if (!op || !ind || (wasInd2 && !ind2)) {
    delete indcopy;
    delete ind2copy;
    delete this;
    return nullptr;
  }

  // convert `class[idx]` to `opIndex` funcall
  if (op->Type.Type == TYPE_Reference || op->Type.Type == TYPE_Class) {
    if (ind2) {
      ParseError(Loc, "No `opIndex` support for 2d array access yet");
      delete indcopy;
      delete ind2copy;
      ParseError(Loc, "Cannot find `opIndex`");
      delete this;
      return nullptr;
    }
    if (assTarget) FatalError("VC: internal compiler error (VArrayElement::InternalResolve)");
    VName mtname = VName("opIndex");
    // try typed method first: opIndex<index>
    VStr itname = "opIndex";
    switch (ind->Type.Type) {
      case TYPE_Int: case TYPE_Byte: case TYPE_Bool: itname += "Int"; break;
      case TYPE_Float: itname += "Float"; break;
      case TYPE_Name: itname += "Name"; break;
      case TYPE_String: itname += "String"; break;
      case TYPE_Reference: itname += "Object"; break;
      case TYPE_Class: itname += "Class"; break;
      case TYPE_State: itname += "State"; break;
    }
    if (op->Type.Class->FindAccessibleMethod(*itname, ec.SelfClass)) mtname = VName(*itname);
    if (!op->Type.Class->FindAccessibleMethod(mtname, ec.SelfClass)) mtname = NAME_None;
    if (mtname == NAME_None) {
      delete indcopy;
      delete ind2copy;
      ParseError(Loc, "Cannot find `opIndex`");
      delete this;
      return nullptr;
    }
    VExpression *e = new VDotInvocation(opscopy.get(), mtname, Loc, 1, &indcopy);
    delete ind2copy;
    delete this;
    return e->Resolve(ec);
  }

  // don't need those anymore
  //delete opcopy;
  opscopy.release();
  delete indcopy;
  delete ind2copy;

  if (ind->Type.Type != TYPE_Int || (ind2 && ind2->Type.Type != TYPE_Int)) {
    ParseError(Loc, "Array index must be of integer type");
    delete this;
    return nullptr;
  }

  if (op->Type.IsAnyArray()) {
    // check bounds for static arrays
    if (!skipBoundsChecking && ind->IsIntConst() && (!ind2 || ind2->IsIntConst())) {
      if (ind->GetIntConst() < 0 || (ind2 && ind2->GetIntConst() < 0)) {
        ParseError(Loc, "Negative array index");
        delete this;
        return nullptr;
      }
      if (op->Type.Type == TYPE_Array) {
        if (ind->GetIntConst() >= op->Type.GetFirstDim()) {
          ParseError(Loc, "Array index %d out of bounds (%d)", ind->GetIntConst(), op->Type.GetFirstDim());
          delete this;
          return nullptr;
        }
        if (ind2 && ind2->GetIntConst() >= op->Type.GetSecondDim()) {
          ParseError(Loc, "Secondary array index %d out of bounds (%d)", ind2->GetIntConst(), op->Type.GetSecondDim());
          delete this;
          return nullptr;
        }
        skipBoundsChecking = true;
      }
    }
    Flags = op->Flags;
    Type = op->Type.GetArrayInnerType();
    op->Flags &= ~FIELD_ReadOnly;
    op->RequestAddressOf();
  } else if (op->Type.Type == TYPE_String) {
    // check bounds
    /*k8: it is tolerant to this
    if (ind->IsIntConst() && ind->GetIntConst() < 0) {
      ParseError(Loc, "Negative string index");
      delete this;
      return nullptr;
    }
    */
    if (assTarget) {
      ParseError(Loc, "Strings are immutable (yet)");
      delete this;
      return nullptr;
    } else {
      RealType = op->Type;
      Type = VFieldType(TYPE_Int);
    }
  } else if (op->Type.Type == TYPE_Pointer) {
    Flags = 0;
    Type = op->Type.GetPointerInnerType();
  } else {
    ParseError(Loc, "Bad operation with array");
    delete this;
    return nullptr;
  }

  // check for unsafe pointer access
  if (op->Type.Type == TYPE_Pointer) {
    if (ind->IsIntConst() && ind->GetIntConst() == 0) {
      // the only safe case, do nothing
    } else {
      if (!VMemberBase::unsafeCodeAllowed) {
        ParseError(Loc, "Unsafe pointer access is forbidden");
        delete this;
        return nullptr;
      } else if (VMemberBase::unsafeCodeWarning) {
        ParseWarning(Loc, "Unsafe pointer access");
      }
    }
  }

  RealType = Type;
  if (Type.Type == TYPE_Byte || Type.Type == TYPE_Bool) Type = VFieldType(TYPE_Int);
  return this;
}


//==========================================================================
//
//  VArrayElement::DoResolve
//
//==========================================================================
VExpression *VArrayElement::DoResolve (VEmitContext &ec) {
  return InternalResolve(ec, false);
}


//==========================================================================
//
//  VArrayElement::ResolveAssignmentTarget
//
//==========================================================================
VExpression *VArrayElement::ResolveAssignmentTarget (VEmitContext &ec) {
  IsAssign = true;
  return InternalResolve(ec, true);
}


//==========================================================================
//
//  VArrayElement::ResolveCompleteAssign
//
//  this will be called before actual assign resolving
//  return `nullptr` to indicate error, or consume `val` and set `resolved` to `true` if resolved
//  if `nullptr` is returned, both `this` and `val` should be destroyed
//
//==========================================================================
VExpression *VArrayElement::ResolveCompleteAssign (VEmitContext &ec, VExpression *val, bool &resolved) {
  if (ind2) return this;

  VExpression *rop = op->SyntaxCopy()->Resolve(ec);
  if (!rop) { delete val; delete this; return nullptr; }

  // convert `class[idx]` to `opIndex` funcall
  if (rop->Type.Type == TYPE_Reference || rop->Type.Type == TYPE_Class) {
    resolved = true; // anyway
    VExpression *rval = val->SyntaxCopy()->Resolve(ec);
    VExpression *rind = ind->SyntaxCopy()->Resolve(ec);
    if (!rval || !rind) {
      delete rval;
      delete rind;
      delete rop;
      delete val;
      delete this;
      return nullptr;
    }
    VExpression *argv[2];
    argv[0] = ind;
    argv[1] = val;
    VName mtname = NAME_None;
    // try typed methods first: opIndex<index>Assign<index>
    VStr itname;
    switch (rind->Type.Type) {
      case TYPE_Int: case TYPE_Byte: case TYPE_Bool: itname += "Int"; break;
      case TYPE_Float: itname += "Float"; break;
      case TYPE_Name: itname += "Name"; break;
      case TYPE_String: itname += "String"; break;
      case TYPE_Reference: itname += "Object"; break;
      case TYPE_Class: itname += "Class"; break;
      case TYPE_State: itname += "State"; break;
    }
    VStr vtname;
    switch (rval->Type.Type) {
      case TYPE_Int: case TYPE_Byte: case TYPE_Bool: vtname += "Int"; break;
      case TYPE_Float: vtname += "Float"; break;
      case TYPE_Name: vtname += "Name"; break;
      case TYPE_String: vtname += "String"; break;
      case TYPE_Reference: vtname += "Object"; break;
      case TYPE_Class: vtname += "Class"; break;
      case TYPE_State: vtname += "State"; break;
    }
    //fprintf(stderr, "rind=<%s>; rval=<%s>; itname=<%s>; vtname=<%s>\n", *rind->Type.GetName(), *rval->Type.GetName(), *itname, *vtname);
    if (!itname.isEmpty() && !vtname.isEmpty()) {
      VStr n = VStr("opIndex")+itname+"Assign"+vtname;
      if (rop->Type.Class->FindAccessibleMethod(*n, ec.SelfClass)) mtname = VName(*n);
    }
    // try shorter
    if (mtname == NAME_None && !itname.isEmpty()) {
      VStr n = VStr("opIndex")+itname+"Assign";
      if (rop->Type.Class->FindAccessibleMethod(*n, ec.SelfClass)) mtname = VName(*n);
    }
    if (mtname == NAME_None && !vtname.isEmpty()) {
      VStr n = VStr("opIndexAssign")+vtname;
      if (rop->Type.Class->FindAccessibleMethod(*n, ec.SelfClass)) mtname = VName(*n);
    }
    if (mtname == NAME_None) {
      mtname = VName("opIndexAssign");
      if (!rop->Type.Class->FindAccessibleMethod(mtname, ec.SelfClass)) mtname = NAME_None;
    }
    // don't need those anymore
    delete rval;
    delete rind;
    delete rop;
    if (mtname == NAME_None) {
      ParseError(Loc, "Cannot find `opIndexAssign`");
      delete val;
      delete this;
      return nullptr;
    }
    // create invocation
    VExpression *e = new VDotInvocation(op, mtname, Loc, 2, argv);
    op = nullptr; // it was consumed by invocation
    ind = nullptr; // it was consumed by invocation
    delete this;
    return e->Resolve(ec);
  }

  if (rop->Type.Type != TYPE_String) {
    delete rop;
    return this; // go on with the normal assignment
  }

  resolved = true; // anyway

  // we need a copy for opDollar
  //VExpression *opcopy = op;
  opscopy.assignNoCopy(op);
  op = rop;

  if (op) {
    // resolve index expression
    auto oldIndArray = ec.SetIndexArray(this);
    ind = ind->Resolve(ec);
    ec.SetIndexArray(oldIndArray);
  }
  sval = (val ? val->Resolve(ec) : nullptr);

  // we don't need this anymore
  //delete opcopy;
  opscopy.release();

  if (!op || !ind || !sval) {
    delete this;
    return nullptr;
  }

  if (op->Type.Type != TYPE_String) {
    ParseError(Loc, "Something is *very* wrong with the compiler");
    delete this;
    return nullptr;
  }

  if (ind->Type.Type != TYPE_Int) {
    ParseError(Loc, "String index must be of integer type");
    delete this;
    return nullptr;
  }

  if (sval->Type.Type == TYPE_String && sval->IsStrConst() && sval->GetStrConst(ec.Package).length() == 1) {
    const char *s = *sval->GetStrConst(ec.Package);
    val = new VIntLiteral((vuint8)s[0], sval->Loc);
    delete sval;
    sval = val->Resolve(ec); // will never fail
  } else if (sval->Type.Type == TYPE_Name && sval->IsNameConst() && VStr::length(*sval->GetNameConst()) == 1) {
    const char *s = *sval->GetNameConst();
    val = new VIntLiteral((vuint8)s[0], sval->Loc);
    delete sval;
    sval = val->Resolve(ec); // will never fail
  }

  if (sval->Type.Type != TYPE_Int && sval->Type.Type != TYPE_Byte) {
    ParseError(Loc, "Cannot assign type '%s' to string element", *sval->Type.GetName());
    delete this;
    return nullptr;
  }

  op->RequestAddressOf();

  genStringAssign = true;
  Type = VFieldType(TYPE_Void);
  return this;
}


//==========================================================================
//
//  VArrayElement::RequestAddressOf
//
//==========================================================================
void VArrayElement::RequestAddressOf () {
  if (op->Type.Type == TYPE_String) {
    ParseError(Loc, "Cannot get string element address");
  } else {
    if (Flags&FIELD_ReadOnly) ParseError(op->Loc, "Tried to assign to a read-only field");
  }
  if (AddressRequested) ParseError(Loc, "Multiple address of");
  AddressRequested = true;
}


//==========================================================================
//
//  VArrayElement::Emit
//
//==========================================================================
void VArrayElement::Emit (VEmitContext &ec) {
  if (op) op->Emit(ec);
  if (ind) ind->Emit(ec);
  if (ind2) ind2->Emit(ec);
  if (genStringAssign) {
    sval->Emit(ec);
    ec.AddStatement(OPC_StrSetChar, Loc);
  } else {
    if (op->Type.Type == TYPE_DynamicArray) {
      if (IsAssign) {
        ec.AddStatement(OPC_DynArrayElementGrow, RealType, Loc);
      } else {
        ec.AddStatement(OPC_DynArrayElement, RealType, Loc);
      }
    } else if (op->Type.Type == TYPE_String) {
      if (IsAssign) {
        ParseError(Loc, "Strings are immutable (yet) -- codegen");
      } else {
        ec.AddStatement(OPC_StrGetChar, Loc);
        return;
      }
    } else if (op->Type.Type == TYPE_SliceArray) {
      ec.AddStatement(OPC_SliceElement, RealType, Loc);
    } else {
      // `Resolve()` will set this flag if it did static check
      if (!skipBoundsChecking) {
        ec.AddStatement((ind2 ? OPC_CheckArrayBounds2d : OPC_CheckArrayBounds), op->Type, Loc);
      }
      ec.AddStatement((ind2 ? OPC_ArrayElement2d : OPC_ArrayElement), RealType, Loc);
    }
    if (!AddressRequested) EmitPushPointedCode(RealType, ec);
  }
}


//==========================================================================
//
//  VArrayElement::GetOpSyntaxCopy
//
//==========================================================================
VExpression *VArrayElement::GetOpSyntaxCopy () {
  return opscopy.SyntaxCopy();
}



//==========================================================================
//
//  VSliceOp::VSliceOp
//
//==========================================================================
VSliceOp::VSliceOp (VExpression *aop, VExpression *alo, VExpression *ahi, const TLocation &aloc)
  : VArrayElement(aop, alo, aloc)
  , hi(ahi)
{
  Flags = FIELD_ReadOnly;
}


//==========================================================================
//
//  VSliceOp::~VSliceOp
//
//==========================================================================
VSliceOp::~VSliceOp () {
  delete hi;
}


//==========================================================================
//
//  VSliceOp::SyntaxCopy
//
//==========================================================================
VExpression *VSliceOp::SyntaxCopy () {
  auto res = new VSliceOp();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VSliceOp::DoSyntaxCopyTo
//
//==========================================================================
void VSliceOp::DoSyntaxCopyTo (VExpression *e) {
  VArrayElement::DoSyntaxCopyTo(e);
  auto res = (VSliceOp *)e;
  res->hi = (hi ? hi->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VSliceOp::DoResolve
//
//==========================================================================
VExpression *VSliceOp::DoResolve (VEmitContext &ec) {
  if (!op || !ind || !hi) {
    delete this;
    return nullptr;
  }

  // we need a copy for opDollar
  //VExpression *opcopy = op->SyntaxCopy();
  opscopy.assignSyntaxCopy(op);

  op = op->Resolve(ec);
  if (op) {
    // resolve index expressions
    auto oldIndArray = ec.SetIndexArray(this);
    ind = ind->Resolve(ec);
    hi = hi->Resolve(ec);
    ec.SetIndexArray(oldIndArray);
  }

  // we don't need this anymore
  //delete opcopy;
  opscopy.release();

  if (!op || !ind || !hi) {
    delete this;
    return nullptr;
  }

  if (op->Type.Type != TYPE_String && op->Type.Type != TYPE_SliceArray) {
    ParseError(Loc, "You can slice only string or another slice");
    delete this;
    return nullptr;
  }

  if (ind->Type.Type != TYPE_Int || hi->Type.Type != TYPE_Int) {
    ParseError(Loc, "Slice indicies must be of integer type");
    delete this;
    return nullptr;
  }

  if (op->Type.Type == TYPE_SliceArray) {
    op->Flags &= ~FIELD_ReadOnly;
    op->RequestAddressOf();
  }

  Type = op->Type;
  return this;
}


//==========================================================================
//
//  VSliceOp::ResolveAssignmentTarget
//
//==========================================================================
VExpression *VSliceOp::ResolveAssignmentTarget (VEmitContext &ec) {
  // actually, this is handled in `ResolveCompleteAssign()`
  ParseError(Loc, "Cannot assign to slices");
  return nullptr;
}


//==========================================================================
//
//  VArrayElement::ResolveCompleteAssign
//
//==========================================================================
VExpression *VSliceOp::ResolveCompleteAssign (VEmitContext &ec, VExpression *val, bool &resolved) {
  resolved = true; // anyway

  VExpression *rop = op->SyntaxCopy()->Resolve(ec);
  if (!rop) { delete val; delete this; return nullptr; }

  if (rop->Type.Type != TYPE_String) {
    ParseError(Loc, "Only string slices are allowed in assign operation");
    delete rop;
    delete val;
    delete this;
    return nullptr;
  }

  // we need a copy for opDollar
  //VExpression *opcopy = op;
  opscopy.assignNoCopy(op);
  op = rop;

  {
    // resolve index expression
    auto oldIndArray = ec.SetIndexArray(this);
    ind = ind->Resolve(ec);
    hi = hi->Resolve(ec);
    ec.SetIndexArray(oldIndArray);
  }
  sval = (val ? val->Resolve(ec) : nullptr);

  // we don't need this anymore
  //delete opcopy;
  opscopy.release();

  if (!op || !ind || !hi || !sval) {
    delete this;
    return nullptr;
  }

  if (ind->Type.Type != TYPE_Int || hi->Type.Type != TYPE_Int) {
    ParseError(Loc, "String slice index must be of integer type");
    delete this;
    return nullptr;
  }

  if (sval->Type.Type != TYPE_String) {
    ParseError(Loc, "String slice new value must be of string type");
    delete this;
    return nullptr;
  }

  op->RequestAddressOf();

  genStringAssign = true;
  Type = VFieldType(TYPE_Void);
  return this;
}


//==========================================================================
//
//  VSliceOp::RequestAddressOf
//
//==========================================================================
void VSliceOp::RequestAddressOf () {
  ParseError(Loc, "Cannot get string slice address");
}


//==========================================================================
//
//  VSliceOp::Emit
//
//==========================================================================
void VSliceOp::Emit (VEmitContext &ec) {
  if (!op || !ind || !hi) return;
  op->Emit(ec);
  ind->Emit(ec);
  hi->Emit(ec);
  if (genStringAssign) {
    sval->Emit(ec);
    ec.AddStatement(OPC_StrSliceAssign, Loc);
  } else if (op->Type.Type == TYPE_String) {
    ec.AddStatement(OPC_StrSlice, Loc);
  } else if (op->Type.Type == TYPE_SliceArray) {
    ec.AddStatement(OPC_SliceSlice, op->Type.GetArrayInnerType(), Loc);
  } else {
    FatalError("VC: the thing that should not be (VSliceOp::Emit)");
  }
}


//==========================================================================
//
//  VDynArrayGetNum::VDynArrayGetNum
//
//==========================================================================
VDynArrayGetNum::VDynArrayGetNum (VExpression *AArrayExpr, const TLocation &ALoc)
  : VExpression(ALoc)
  , ArrayExpr(AArrayExpr)
{
  Flags = FIELD_ReadOnly;
}


//==========================================================================
//
//  VDynArrayGetNum::~VDynArrayGetNum
//
//==========================================================================
VDynArrayGetNum::~VDynArrayGetNum () {
  if (ArrayExpr) { delete ArrayExpr; ArrayExpr = nullptr; }
}


//==========================================================================
//
//  VDynArrayGetNum::SyntaxCopy
//
//==========================================================================
VExpression *VDynArrayGetNum::SyntaxCopy () {
  auto res = new VDynArrayGetNum();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDynArrayGetNum::DoRestSyntaxCopyTo
//
//==========================================================================
void VDynArrayGetNum::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDynArrayGetNum *)e;
  res->ArrayExpr = (ArrayExpr ? ArrayExpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDynArrayGetNum::DoResolve
//
//==========================================================================
VExpression *VDynArrayGetNum::DoResolve (VEmitContext &) {
  Type = VFieldType(TYPE_Int);
  return this;
}


//==========================================================================
//
//  VDynArrayGetNum::Emit
//
//==========================================================================
void VDynArrayGetNum::Emit (VEmitContext &ec) {
  ArrayExpr->Emit(ec);
  ec.AddStatement(OPC_DynArrayGetNum, Loc);
}


//==========================================================================
//
//  VDynArraySetNum::VDynArraySetNum
//
//==========================================================================
VDynArraySetNum::VDynArraySetNum (VExpression *AArrayExpr, VExpression *ANumExpr, const TLocation &ALoc)
  : VExpression(ALoc)
  , ArrayExpr(AArrayExpr)
  , NumExpr(ANumExpr)
  , opsign(0)
{
  Type = VFieldType(TYPE_Void);
}


//==========================================================================
//
//  VDynArraySetNum::~VDynArraySetNum
//
//==========================================================================
VDynArraySetNum::~VDynArraySetNum () {
  if (ArrayExpr) { delete ArrayExpr; ArrayExpr = nullptr; }
  if (NumExpr) { delete NumExpr; NumExpr = nullptr; }
}


//==========================================================================
//
//  VDynArraySetNum::SyntaxCopy
//
//==========================================================================
VExpression *VDynArraySetNum::SyntaxCopy () {
  auto res = new VDynArraySetNum();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDynArraySetNum::DoRestSyntaxCopyTo
//
//==========================================================================
void VDynArraySetNum::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDynArraySetNum *)e;
  res->ArrayExpr = (ArrayExpr ? ArrayExpr->SyntaxCopy() : nullptr);
  res->NumExpr = (NumExpr ? NumExpr->SyntaxCopy() : nullptr);
  res->opsign = opsign;
}


//==========================================================================
//
//  VDynArraySetNum::DoResolve
//
//==========================================================================
VExpression *VDynArraySetNum::DoResolve (VEmitContext &) {
  return this;
}


//==========================================================================
//
//  VDynArraySetNum::Emit
//
//==========================================================================
void VDynArraySetNum::Emit (VEmitContext &ec) {
  ArrayExpr->Emit(ec);
  NumExpr->Emit(ec);
  if (opsign == 0) {
    // normal assign
    ec.AddStatement(OPC_DynArraySetNum, ArrayExpr->Type.GetArrayInnerType(), Loc);
  } else if (opsign < 0) {
    // -=
    ec.AddStatement(OPC_DynArraySetNumMinus, ArrayExpr->Type.GetArrayInnerType(), Loc);
  } else {
    // +=
    ec.AddStatement(OPC_DynArraySetNumPlus, ArrayExpr->Type.GetArrayInnerType(), Loc);
  }
}


//==========================================================================
//
//  VDynArraySetNum::IsDynArraySetNum
//
//==========================================================================
bool VDynArraySetNum::IsDynArraySetNum () const {
  return true;
}


//==========================================================================
//
//  VDynArrayInsert::VDynArrayInsert
//
//==========================================================================
VDynArrayInsert::VDynArrayInsert (VExpression *AArrayExpr, VExpression *AIndexExpr, VExpression *ACountExpr, const TLocation &ALoc)
  : VExpression(ALoc)
  , ArrayExpr(AArrayExpr)
  , IndexExpr(AIndexExpr)
  , CountExpr(ACountExpr)
{
}


//==========================================================================
//
//  VDynArrayInsert::~VDynArrayInsert
//
//==========================================================================
VDynArrayInsert::~VDynArrayInsert () {
  if (ArrayExpr) { delete ArrayExpr; ArrayExpr = nullptr; }
  if (IndexExpr) { delete IndexExpr; IndexExpr = nullptr; }
  if (CountExpr) { delete CountExpr; CountExpr = nullptr; }
}


//==========================================================================
//
//  VDynArrayInsert::SyntaxCopy
//
//==========================================================================
VExpression *VDynArrayInsert::SyntaxCopy () {
  auto res = new VDynArrayInsert();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDynArrayInsert::DoRestSyntaxCopyTo
//
//==========================================================================
void VDynArrayInsert::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDynArrayInsert *)e;
  res->ArrayExpr = (ArrayExpr ? ArrayExpr->SyntaxCopy() : nullptr);
  res->IndexExpr = (IndexExpr ? IndexExpr->SyntaxCopy() : nullptr);
  res->CountExpr = (CountExpr ? CountExpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDynArrayInsert::DoResolve
//
//==========================================================================
VExpression *VDynArrayInsert::DoResolve (VEmitContext &ec) {
  if (ArrayExpr) ArrayExpr->RequestAddressOf();
  // resolve arguments
  if (IndexExpr) IndexExpr = IndexExpr->Resolve(ec);
  if (CountExpr) CountExpr = CountExpr->Resolve(ec);
  if (!IndexExpr || !CountExpr) {
    delete this;
    return nullptr;
  }

  // check argument types
  if (IndexExpr->Type.Type != TYPE_Int) {
    ParseError(Loc, "Index must be integer expression");
    delete this;
    return nullptr;
  }

  if (CountExpr->Type.Type != TYPE_Int) {
    ParseError(Loc, "Count must be integer expression");
    delete this;
    return nullptr;
  }

  Type = VFieldType(TYPE_Void);
  return this;
}


//==========================================================================
//
//  VDynArrayInsert::Emit
//
//==========================================================================
void VDynArrayInsert::Emit (VEmitContext &ec) {
  ArrayExpr->Emit(ec);
  IndexExpr->Emit(ec);
  CountExpr->Emit(ec);
  ec.AddStatement(OPC_DynArrayInsert, ArrayExpr->Type.GetArrayInnerType(), Loc);
}


//==========================================================================
//
//  VDynArrayRemove::VDynArrayRemove
//
//==========================================================================
VDynArrayRemove::VDynArrayRemove (VExpression *AArrayExpr, VExpression *AIndexExpr, VExpression *ACountExpr, const TLocation &ALoc)
  : VExpression(ALoc)
  , ArrayExpr(AArrayExpr)
  , IndexExpr(AIndexExpr)
  , CountExpr(ACountExpr)
{
}


//==========================================================================
//
//  VDynArrayRemove::~VDynArrayRemove
//
//==========================================================================
VDynArrayRemove::~VDynArrayRemove () {
  if (ArrayExpr) { delete ArrayExpr; ArrayExpr = nullptr; }
  if (IndexExpr) { delete IndexExpr; IndexExpr = nullptr; }
  if (CountExpr) { delete CountExpr; CountExpr = nullptr; }
}


//==========================================================================
//
//  VDynArrayRemove::SyntaxCopy
//
//==========================================================================
VExpression *VDynArrayRemove::SyntaxCopy () {
  auto res = new VDynArrayRemove();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDynArrayRemove::DoRestSyntaxCopyTo
//
//==========================================================================
void VDynArrayRemove::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDynArrayRemove *)e;
  res->ArrayExpr = (ArrayExpr ? ArrayExpr->SyntaxCopy() : nullptr);
  res->IndexExpr = (IndexExpr ? IndexExpr->SyntaxCopy() : nullptr);
  res->CountExpr = (CountExpr ? CountExpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDynArrayRemove::DoResolve
//
//==========================================================================
VExpression *VDynArrayRemove::DoResolve (VEmitContext &ec) {
  if (ArrayExpr) ArrayExpr->RequestAddressOf();
  // resolve arguments
  if (IndexExpr) IndexExpr = IndexExpr->Resolve(ec);
  if (CountExpr) CountExpr = CountExpr->Resolve(ec);
  if (!IndexExpr || !CountExpr) {
    delete this;
    return nullptr;
  }

  // check argument types
  if (IndexExpr->Type.Type != TYPE_Int) {
    ParseError(Loc, "Index must be integer expression");
    delete this;
    return nullptr;
  }

  if (CountExpr->Type.Type != TYPE_Int) {
    ParseError(Loc, "Count must be integer expression");
    delete this;
    return nullptr;
  }

  Type = VFieldType(TYPE_Void);
  return this;
}


//==========================================================================
//
//  VDynArrayRemove::Emit
//
//==========================================================================
void VDynArrayRemove::Emit (VEmitContext &ec) {
  ArrayExpr->Emit(ec);
  IndexExpr->Emit(ec);
  CountExpr->Emit(ec);
  ec.AddStatement(OPC_DynArrayRemove, ArrayExpr->Type.GetArrayInnerType(), Loc);
}


//==========================================================================
//
//  VStringGetLength::VStringGetLength
//
//==========================================================================
VStringGetLength::VStringGetLength(VExpression *AStrExpr, const TLocation &ALoc)
  : VExpression(ALoc)
  , StrExpr(AStrExpr)
{
  Flags = FIELD_ReadOnly;
}


//==========================================================================
//
//  VStringGetLength::~VStringGetLength
//
//==========================================================================
VStringGetLength::~VStringGetLength () {
  if (StrExpr) {
    delete StrExpr;
    StrExpr = nullptr;
  }
}


//==========================================================================
//
//  VStringGetLength::SyntaxCopy
//
//==========================================================================
VExpression *VStringGetLength::SyntaxCopy () {
  auto res = new VStringGetLength();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VStringGetLength::DoRestSyntaxCopyTo
//
//==========================================================================
void VStringGetLength::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VStringGetLength *)e;
  res->StrExpr = (StrExpr ? StrExpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VStringGetLength::DoResolve
//
//==========================================================================
VExpression *VStringGetLength::DoResolve (VEmitContext &ec) {
  // optimize it for string literals
  if (StrExpr->IsStrConst()) {
    VStr val = StrExpr->GetStrConst(ec.Package);
    VExpression *e = new VIntLiteral(val.Length(), Loc);
    e = e->Resolve(ec);
    delete this;
    return e;
  }
  Type = VFieldType(TYPE_Int);
  return this;
}


//==========================================================================
//
//  VStringGetLength::Emit
//
//==========================================================================
void VStringGetLength::Emit (VEmitContext &ec) {
  StrExpr->Emit(ec);
  ec.AddStatement(OPC_StrLength, Loc);
}



//==========================================================================
//
//  VSliceGetLength::VSliceGetLength
//
//==========================================================================
VSliceGetLength::VSliceGetLength (VExpression *asexpr, const TLocation &aloc)
  : VExpression(aloc)
  , sexpr(asexpr)
{
  Flags = FIELD_ReadOnly;
}


//==========================================================================
//
//  VSliceGetLength::~VSliceGetLength
//
//==========================================================================
VSliceGetLength::~VSliceGetLength () {
  delete sexpr;
}


//==========================================================================
//
//  VSliceGetLength::SyntaxCopy
//
//==========================================================================
VExpression *VSliceGetLength::SyntaxCopy () {
  auto res = new VSliceGetLength();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VSliceGetLength::DoSyntaxCopyTo
//
//==========================================================================
void VSliceGetLength::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VSliceGetLength *)e;
  res->sexpr = (sexpr ? sexpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VSliceGetLength::DoResolve
//
//==========================================================================
VExpression *VSliceGetLength::DoResolve (VEmitContext &ec) {
  Type = VFieldType(TYPE_Int);
  return this;
}


//==========================================================================
//
//  VSliceGetLength::Emit
//
//==========================================================================
void VSliceGetLength::Emit (VEmitContext &ec) {
  sexpr->Emit(ec);
  /*
  ec.AddStatement(OPC_OffsetPtr, (int)sizeof(void *), Loc);
  ec.AddStatement(OPC_PushPointed, Loc);
  */
  // use special instruction, so we can check for null ptr
  ec.AddStatement(OPC_PushPointedSliceLen, Loc);
}



//==========================================================================
//
//  VSliceGetPtr::VSliceGetPtr
//
//==========================================================================
VSliceGetPtr::VSliceGetPtr (VExpression *asexpr, const TLocation &aloc)
  : VExpression(aloc)
  , sexpr(asexpr)
{
  Flags = FIELD_ReadOnly;
}


//==========================================================================
//
//  VSliceGetPtr::~VSliceGetPtr
//
//==========================================================================
VSliceGetPtr::~VSliceGetPtr () {
  delete sexpr;
}


//==========================================================================
//
//  VSliceGetPtr::SyntaxCopy
//
//==========================================================================
VExpression *VSliceGetPtr::SyntaxCopy () {
  auto res = new VSliceGetPtr();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VSliceGetPtr::DoSyntaxCopyTo
//
//==========================================================================
void VSliceGetPtr::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VSliceGetPtr *)e;
  res->sexpr = (sexpr ? sexpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VSliceGetPtr::DoResolve
//
//==========================================================================
VExpression *VSliceGetPtr::DoResolve (VEmitContext &ec) {
  //fprintf(stderr, "setype: <%s>\n", *sexpr->Type.GetName());
  Type = sexpr->Type.GetArrayInnerType();
  //Type = Type.MakePointerType();
  return this;
}


//==========================================================================
//
//  VSliceGetPtr::Emit
//
//==========================================================================
void VSliceGetPtr::Emit (VEmitContext &ec) {
  sexpr->Emit(ec);
  ec.AddStatement(OPC_PushPointedPtr, Loc);
}
