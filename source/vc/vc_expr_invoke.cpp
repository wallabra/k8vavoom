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
//  VRefOutArg::VRefOutArg
//
//==========================================================================
VRefOutArg::VRefOutArg (VExpression *ae)
  : VExpression(ae->Loc)
  , e(ae)
{
}


//==========================================================================
//
//  VRefOutArg::~VRefOutArg
//
//==========================================================================
VRefOutArg::~VRefOutArg () {
  delete e;
}


//==========================================================================
//
//  VRefOutArg::DoSyntaxCopyTo
//
//==========================================================================
void VRefOutArg::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VRefOutArg *)e;
  res->e = (this->e ? this->e->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VRefOutArg::DoSyntaxCopyTo
//
//==========================================================================
VExpression *VRefOutArg::DoResolve (VEmitContext &ec) {
  if (e) e = e->Resolve(ec);
  if (!e) { delete this; return nullptr; }
  VExpression *res = e;
  e = nullptr;
  delete this;
  return res;
}


//==========================================================================
//
//  VRefOutArg::DoSyntaxCopyTo
//
//==========================================================================
void VRefOutArg::Emit (VEmitContext &ec) {
  Sys_Error("The thing that should not be (VRefOutArg::Emit)");
}


//==========================================================================
//
//  VRefArg::VRefArg
//
//==========================================================================
VRefArg::VRefArg (VExpression *ae) : VRefOutArg(ae) {
}


//==========================================================================
//
//  VRefArg::IsRefArg
//
//==========================================================================
bool VRefArg::IsRefArg () const {
  return true;
}


//==========================================================================
//
//  VRefArg::SyntaxCopy
//
//==========================================================================
VExpression *VRefArg::SyntaxCopy () {
  auto res = new VRefArg();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VOutArg::VOutArg
//
//==========================================================================
VOutArg::VOutArg (VExpression *ae) : VRefOutArg(ae) {
}


//==========================================================================
//
//  VOutArg::VOutArg
//
//==========================================================================
bool VOutArg::IsOutArg () const {
  return true;
}


//==========================================================================
//
//  VOutArg::SyntaxCopy
//
//==========================================================================
VExpression *VOutArg::SyntaxCopy () {
  auto res = new VOutArg();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VSuperInvocation::VSuperInvocation
//
//==========================================================================
VInvocationBase::VInvocationBase (int ANumArgs, VExpression **AArgs, const TLocation &ALoc)
  : VExpression(ALoc)
  , NumArgs(ANumArgs)
{
  memset(Args, 0, sizeof(Args)); // why not
  for (int i = 0; i < NumArgs; ++i) Args[i] = AArgs[i];
}

VInvocationBase::~VInvocationBase () {
  for (int i = 0; i < NumArgs; ++i) { delete Args[i]; Args[i] = nullptr; }
  NumArgs = 0;
}

void VInvocationBase::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VInvocationBase *)e;
  memset(res->Args, 0, sizeof(res->Args));
  res->NumArgs = NumArgs;
  for (int i = 0; i < NumArgs; ++i) res->Args[i] = (Args[i] ? Args[i]->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VSuperInvocation::VSuperInvocation
//
//==========================================================================
VSuperInvocation::VSuperInvocation(VName AName, int ANumArgs, VExpression **AArgs, const TLocation &ALoc)
  : VInvocationBase(ANumArgs, AArgs, ALoc)
  , Name(AName)
{
}


//==========================================================================
//
//  VSuperInvocation::SyntaxCopy
//
//==========================================================================
VExpression *VSuperInvocation::SyntaxCopy () {
  auto res = new VSuperInvocation();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VSuperInvocation::DoSyntaxCopyTo
//
//==========================================================================
void VSuperInvocation::DoSyntaxCopyTo (VExpression *e) {
  VInvocationBase::DoSyntaxCopyTo(e);
  auto res = (VSuperInvocation *)e;
  res->Name = Name;
}


//==========================================================================
//
//  VSuperInvocation::DoResolve
//
//==========================================================================
VExpression *VSuperInvocation::DoResolve (VEmitContext &ec) {
  guard(VSuperInvocation::DoResolve);

  if (ec.SelfClass) {
    VMethod *Func = ec.SelfClass->ParentClass->FindAccessibleMethod(Name, ec.SelfClass);
    if (Func) {
      VInvocation *e = new VInvocation(nullptr, Func, nullptr, false, true, Loc, NumArgs, Args);
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }
  }

  if (VStr::Cmp(*Name, "write") == 0 || VStr::Cmp(*Name, "writeln") == 0) {
    VExpression *e = new VInvokeWrite((VStr::Cmp(*Name, "writeln") == 0), Loc, NumArgs, Args);
    NumArgs = 0;
    delete this;
    return e->Resolve(ec);
  }

  if (!ec.SelfClass) {
    ParseError(Loc, ":: not in method");
    delete this;
    return nullptr;
  }

  ParseError(Loc, "No such method `%s`", *Name);
  delete this;
  return nullptr;
  unguard;
}


//==========================================================================
//
//  VSuperInvocation::Emit
//
//==========================================================================
void VSuperInvocation::Emit (VEmitContext &) {
  guard(VSuperInvocation::Emit);
  ParseError(Loc, "Should not happen (VSuperInvocation)");
  unguard;
}


//==========================================================================
//
//  VCastOrInvocation::VCastOrInvocation
//
//==========================================================================
VCastOrInvocation::VCastOrInvocation (VName AName, const TLocation &ALoc, int ANumArgs, VExpression **AArgs)
  : VInvocationBase(ANumArgs, AArgs, ALoc)
  , Name(AName)
{
}


//==========================================================================
//
//  VCastOrInvocation::SyntaxCopy
//
//==========================================================================
VExpression *VCastOrInvocation::SyntaxCopy () {
  auto res = new VCastOrInvocation();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VCastOrInvocation::DoSyntaxCopyTo
//
//==========================================================================
void VCastOrInvocation::DoSyntaxCopyTo (VExpression *e) {
  VInvocationBase::DoSyntaxCopyTo(e);
  auto res = (VCastOrInvocation *)e;
  res->Name = Name;
}


//==========================================================================
//
//  VCastOrInvocation::DoResolve
//
//==========================================================================
VExpression *VCastOrInvocation::DoResolve (VEmitContext &ec) {
  VClass *Class = VMemberBase::StaticFindClass(Name);
  if (Class) {
    if (NumArgs != 1 || !Args[0]) {
      ParseError(Loc, "Dynamic cast requires 1 argument");
      delete this;
      return nullptr;
    }
    VExpression *e = new VDynamicCast(Class, Args[0], Loc);
    NumArgs = 0;
    delete this;
    return e->Resolve(ec);
  }

  if (ec.SelfClass) {
    VMethod *M = ec.SelfClass->FindAccessibleMethod(Name, ec.SelfClass);
    if (M) {
      if (M->Flags & FUNC_Iterator) {
        ParseError(Loc, "Iterator methods can only be used in foreach statements");
        delete this;
        return nullptr;
      }
      VInvocation *e = new VInvocation(nullptr, M, nullptr, false, false, Loc, NumArgs, Args);
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }

    VField *field = ec.SelfClass->FindField(Name, Loc, ec.SelfClass);
    if (field != nullptr && field->Type.Type == TYPE_Delegate) {
      VInvocation *e = new VInvocation(nullptr, field->Func, field, false, false, Loc, NumArgs, Args);
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }
  }

  if (VStr::Cmp(*Name, "write") == 0 || VStr::Cmp(*Name, "writeln") == 0) {
    VExpression *e = new VInvokeWrite((VStr::Cmp(*Name, "writeln") == 0), Loc, NumArgs, Args);
    NumArgs = 0;
    delete this;
    return e->Resolve(ec);
  }

  ParseError(Loc, "Unknown method %s", *Name);
  delete this;
  return nullptr;
}


//==========================================================================
//
//  VCastOrInvocation::ResolveIterator
//
//==========================================================================
VExpression *VCastOrInvocation::ResolveIterator (VEmitContext &ec) {
  VMethod *M = ec.SelfClass->FindAccessibleMethod(Name, ec.SelfClass);
  if (!M) {
    ParseError(Loc, "Unknown method %s", *Name);
    delete this;
    return nullptr;
  }
  if ((M->Flags&FUNC_Iterator) == 0) {
    ParseError(Loc, "%s is not an iterator method", *Name);
    delete this;
    return nullptr;
  }

  VInvocation *e = new VInvocation(nullptr, M, nullptr, false, false, Loc, NumArgs, Args);
  NumArgs = 0;
  delete this;
  return e->Resolve(ec);
}


//==========================================================================
//
//  VCastOrInvocation::Emit
//
//==========================================================================
void VCastOrInvocation::Emit (VEmitContext &) {
  ParseError(Loc, "Should not happen (VCastOrInvocation)");
}


//==========================================================================
//
//  VDotInvocation::VDotInvocation
//
//==========================================================================
VDotInvocation::VDotInvocation (VExpression *ASelfExpr, VName AMethodName, const TLocation &ALoc, int ANumArgs, VExpression **AArgs)
  : VInvocationBase(ANumArgs, AArgs, ALoc)
  , SelfExpr(ASelfExpr)
  , MethodName(AMethodName)
{
}


//==========================================================================
//
//  VDotInvocation::~VDotInvocation
//
//==========================================================================
VDotInvocation::~VDotInvocation () {
  if (SelfExpr) { delete SelfExpr; SelfExpr = nullptr; }
}


//==========================================================================
//
//  VDotInvocation::SyntaxCopy
//
//==========================================================================
VExpression *VDotInvocation::SyntaxCopy () {
  auto res = new VDotInvocation();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDotInvocation::DoSyntaxCopyTo
//
//==========================================================================
void VDotInvocation::DoSyntaxCopyTo (VExpression *e) {
  VInvocationBase::DoSyntaxCopyTo(e);
  auto res = (VDotInvocation *)e;
  res->SelfExpr = (SelfExpr ? SelfExpr->SyntaxCopy() : nullptr);
  res->MethodName = MethodName;
}


//==========================================================================
//
//  VDotInvocation::DoResolve
//
//==========================================================================
VExpression *VDotInvocation::DoResolve (VEmitContext &ec) {
  VExpression *selfCopy = (SelfExpr ? SelfExpr->SyntaxCopy() : nullptr);

  if (SelfExpr) SelfExpr = SelfExpr->Resolve(ec);
  if (!SelfExpr) {
    delete selfCopy;
    delete this;
    return nullptr;
  }

  if (SelfExpr->Type.Type == TYPE_DynamicArray) {
    delete selfCopy;
    if (MethodName == NAME_Insert || VStr::Cmp(*MethodName, "insert") == 0) {
      if (NumArgs == 1) {
        // default count is 1
        Args[1] = new VIntLiteral(1, Loc);
        NumArgs = 2;
      }
      if (NumArgs != 2) {
        ParseError(Loc, "Insert requires 1 or 2 arguments");
        delete this;
        return nullptr;
      }
      if (Args[0] && (Args[0]->IsRefArg() || Args[0]->IsOutArg())) {
        ParseError(Args[0]->Loc, "Insert cannot has `ref`/`out` argument");
        delete this;
        return nullptr;
      }
      if (Args[1] && (Args[1]->IsRefArg() || Args[1]->IsOutArg())) {
        ParseError(Args[1]->Loc, "Insert cannot has `ref`/`out` argument");
        delete this;
        return nullptr;
      }
      VExpression *e = new VDynArrayInsert(SelfExpr, Args[0], Args[1], Loc);
      SelfExpr = nullptr;
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }

    if (MethodName == NAME_Remove || VStr::Cmp(*MethodName, "remove") == 0) {
      if (NumArgs == 1) {
        // default count is 1
        Args[1] = new VIntLiteral(1, Loc);
        NumArgs = 2;
      }
      if (NumArgs != 2) {
        ParseError(Loc, "Insert requires 1 or 2 arguments");
        delete this;
        return nullptr;
      }
      if (Args[0] && (Args[0]->IsRefArg() || Args[0]->IsOutArg())) {
        ParseError(Args[0]->Loc, "Remove cannot has `ref`/`out` argument");
        delete this;
        return nullptr;
      }
      if (Args[1] && (Args[1]->IsRefArg() || Args[1]->IsOutArg())) {
        ParseError(Args[1]->Loc, "Remove cannot has `ref`/`out` argument");
        delete this;
        return nullptr;
      }
      VExpression *e = new VDynArrayRemove(SelfExpr, Args[0], Args[1], Loc);
      SelfExpr = nullptr;
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }

    ParseError(Loc, "Invalid operation on dynamic array");
    delete this;
    return nullptr;
  }

  // Class.Method -- for static methods
  if (SelfExpr->Type.Type == TYPE_Class) {
    delete selfCopy;
    if (!SelfExpr->Type.Class) {
      ParseError(Loc, "Class name expected at the left side of `.`");
      delete this;
      return nullptr;
    }
    VMethod *M = SelfExpr->Type.Class->FindAccessibleMethod(MethodName, ec.SelfClass);
    if (!M) {
      ParseError(Loc, "Method `%s` not found in class `%s`", *MethodName, SelfExpr->Type.Class->GetName());
      delete this;
      return nullptr;
    }
    //fprintf(stderr, "TYPE: %s\n", *SelfExpr->Type.GetName());
    if (M->Flags&FUNC_Iterator) {
      ParseError(Loc, "Iterator methods can only be used in foreach statements");
      delete this;
      return nullptr;
    }
    if ((M->Flags&FUNC_Static) == 0) {
      ParseError(Loc, "Only static methods can be called with this syntax");
      delete this;
      return nullptr;
    }
    // statics has no self
    VExpression *e = new VInvocation(nullptr, M, nullptr, false, false, Loc, NumArgs, Args);
    NumArgs = 0;
    delete this;
    return e->Resolve(ec);
  }

  if (SelfExpr->Type.Type != TYPE_Reference) {
    // try UFCS
    if (NumArgs+1 <= VMethod::MAX_PARAMS) {
      int newArgC = NumArgs+1;
      VExpression *ufcsArgs[VMethod::MAX_PARAMS+1];
      for (int f = 0; f < NumArgs; ++f) ufcsArgs[f+1] = Args[f];
      ufcsArgs[0] = selfCopy;
      if (VInvocation::FindMethodWithSignature(ec, MethodName, newArgC, ufcsArgs)) {
        VCastOrInvocation *call = new VCastOrInvocation(MethodName, Loc, newArgC, ufcsArgs);
        // don't delete `selfCopy`, it is used
        NumArgs = 0; // also, don't delete args
        delete this;
        return call->Resolve(ec);
      }
    }
    ParseError(Loc, "Object reference expected at the left side of `.`");
    delete selfCopy;
    delete this;
    return nullptr;
  }

  VMethod *M = SelfExpr->Type.Class->FindAccessibleMethod(MethodName, ec.SelfClass);
  if (M) {
    // don't need it anymore
    delete selfCopy;
    if (M->Flags & FUNC_Iterator) {
      ParseError(Loc, "Iterator methods can only be used in foreach statements");
      delete this;
      return nullptr;
    }
    VExpression *e = new VInvocation(SelfExpr, M, nullptr, true, false, Loc, NumArgs, Args);
    SelfExpr = nullptr;
    NumArgs = 0;
    delete this;
    return e->Resolve(ec);
  }

  VField *field = SelfExpr->Type.Class->FindField(MethodName, Loc, ec.SelfClass);
  if (field && field->Type.Type == TYPE_Delegate) {
    // don't need it anymore
    delete selfCopy;
    VExpression *e = new VInvocation(SelfExpr, field->Func, field, true, false, Loc, NumArgs, Args);
    SelfExpr = nullptr;
    NumArgs = 0;
    delete this;
    return e->Resolve(ec);
  }

  // try UFCS
  if (NumArgs+1 <= VMethod::MAX_PARAMS) {
    int newArgC = NumArgs+1;
    VExpression *ufcsArgs[VMethod::MAX_PARAMS+1];
    for (int f = 0; f < NumArgs; ++f) ufcsArgs[f+1] = Args[f];
    ufcsArgs[0] = selfCopy;
    if (VInvocation::FindMethodWithSignature(ec, MethodName, newArgC, ufcsArgs)) {
      VCastOrInvocation *call = new VCastOrInvocation(MethodName, Loc, newArgC, ufcsArgs);
      // don't delete `selfCopy`, it is used
      NumArgs = 0; // also, don't delete args
      delete this;
      return call->Resolve(ec);
    }
  }

  // don't need it anymore
  delete selfCopy;

  ParseError(Loc, "No such method %s", *MethodName);
  delete this;
  return nullptr;
}


//==========================================================================
//
//  VDotInvocation::ResolveIterator
//
//==========================================================================
VExpression *VDotInvocation::ResolveIterator (VEmitContext &ec) {
  if (SelfExpr) SelfExpr = SelfExpr->Resolve(ec);
  if (!SelfExpr) {
    delete this;
    return nullptr;
  }

  if (SelfExpr->Type.Type != TYPE_Reference) {
    ParseError(Loc, "Object reference expected at the left side of `.`");
    delete this;
    return nullptr;
  }

  VMethod *M = SelfExpr->Type.Class->FindAccessibleMethod(MethodName, ec.SelfClass);
  if (!M) {
    ParseError(Loc, "No such method %s", *MethodName);
    delete this;
    return nullptr;
  }
  if ((M->Flags&FUNC_Iterator) == 0) {
    ParseError(Loc, "%s is not an iterator method", *MethodName);
    delete this;
    return nullptr;
  }

  VExpression *e = new VInvocation(SelfExpr, M, nullptr, true, false, Loc, NumArgs, Args);
  SelfExpr = nullptr;
  NumArgs = 0;
  delete this;
  return e->Resolve(ec);
}


//==========================================================================
//
//  VDotInvocation::Emit
//
//==========================================================================
void VDotInvocation::Emit (VEmitContext &) {
  ParseError(Loc, "Should not happen (VDotInvocation)");
}


//==========================================================================
//
//  VInvocation::VInvocation
//
//==========================================================================

VInvocation::VInvocation (VExpression *ASelfExpr, VMethod *AFunc, VField *ADelegateField,
                          bool AHaveSelf, bool ABaseCall, const TLocation &ALoc, int ANumArgs,
                          VExpression **AArgs)
  : VInvocationBase(ANumArgs, AArgs, ALoc)
  , SelfExpr(ASelfExpr)
  , Func(AFunc)
  , DelegateField(ADelegateField)
  , HaveSelf(AHaveSelf)
  , BaseCall(ABaseCall)
  , CallerState(nullptr)
  , MultiFrameState(false)
{
}


//==========================================================================
//
//  VInvocation::~VInvocation
//
//==========================================================================
VInvocation::~VInvocation() {
  if (SelfExpr) { delete SelfExpr; SelfExpr = nullptr; }
}


//==========================================================================
//
//  VInvocation::SyntaxCopy
//
//==========================================================================
VExpression *VInvocation::SyntaxCopy () {
  auto res = new VInvocation();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VCastOrInvocation::DoSyntaxCopyTo
//
//==========================================================================
void VInvocation::DoSyntaxCopyTo (VExpression *e) {
  VInvocationBase::DoSyntaxCopyTo(e);
  auto res = (VInvocation *)e;
  res->SelfExpr = (SelfExpr ? SelfExpr->SyntaxCopy() : nullptr);
  res->Func = Func;
  res->DelegateField = DelegateField;
  res->HaveSelf = HaveSelf;
  res->BaseCall = BaseCall;
  res->CallerState = CallerState;
  res->MultiFrameState = MultiFrameState;
}


//==========================================================================
//
//  VInvocation::DoResolve
//
//==========================================================================
VExpression *VInvocation::DoResolve (VEmitContext &ec) {
  guard(VInvocation::DoResolve);
  if (ec.Package->Name == NAME_decorate) CheckDecorateParams(ec);

  int argc = (NumArgs > 0 ? NumArgs : 0);
  VExpression **argv = (argc > 0 ? new VExpression *[argc] : nullptr);
  for (int f = 0; f < argc; ++f) argv[f] = nullptr;

  // resolve arguments
  int requiredParams = Func->NumParams;
  int maxParams = (Func->Flags&FUNC_VarArgs ? VMethod::MAX_PARAMS-1 : Func->NumParams);
  bool ArgsOk = true;
  for (int i = 0; i < NumArgs; ++i) {
    if (i >= maxParams) {
      ParseError((Args[i] ? Args[i]->Loc : Loc), "Too many method arguments");
      ArgsOk = false;
      break;
    }
    if (Args[i]) {
      // check for `ref`/`out` validness
      if (Args[i]->IsRefArg() && (i >= requiredParams || (Func->ParamFlags[i]&FPARM_Ref) == 0)) {
        ParseError(Args[i]->Loc, "`ref` argument for non-ref parameter #%d", i+1);
        ArgsOk = false;
        break;
      }
      if (Args[i]->IsOutArg() && (i >= requiredParams || (Func->ParamFlags[i]&FPARM_Out) == 0)) {
        ParseError(Args[i]->Loc, "`out` argument for non-ref parameter #%d", i+1);
        ArgsOk = false;
        break;
      }
      argv[i] = Args[i]->SyntaxCopy(); // save it for checker
      Args[i] = Args[i]->Resolve(ec);
      if (!Args[i]) { ArgsOk = false; break; }
    } else {
      /*
      if (!(Func->ParamFlags[i]&FPARM_Optional)) {
        ParseError(Loc, "Cannot omit non-optional argument");
        ArgsOk = false;
        break;
      }
      */
    }
  }

  if (!ArgsOk) {
    for (int f = 0; f < argc; ++f) delete argv[f];
    delete[] argv;
    delete this;
    return nullptr;
  }

  CheckParams(ec, argc, argv);

  Type = Func->ReturnType;
  if (Type.Type == TYPE_Byte || Type.Type == TYPE_Bool) Type = VFieldType(TYPE_Int);
  if (Func->Flags&FUNC_Spawner) Type.Class = Args[0]->Type.Class;

  for (int f = 0; f < argc; ++f) delete argv[f];
  delete[] argv;
  return this;
  unguard;
}


//==========================================================================
//
//  VInvocation::Emit
//
//==========================================================================
void VInvocation::Emit (VEmitContext &ec) {
  guard(VInvocation::Emit);
  if (SelfExpr) SelfExpr->Emit(ec);

  bool DirectCall = (BaseCall || (Func->Flags&FUNC_Final) != 0);

  if (Func->Flags&FUNC_Static) {
    if (HaveSelf) ParseError(Loc, "Invalid static function call");
  } else {
    if (!HaveSelf) {
      if (ec.CurrentFunc->Flags & FUNC_Static) ParseError(Loc, "An object is required to call non-static methods");
      ec.AddStatement(OPC_LocalValue0);
    }
  }

  vint32 SelfOffset = 1;
  for (int i = 0; i < NumArgs; ++i) {
    if (!Args[i]) {
      switch (Func->ParamTypes[i].Type) {
        case TYPE_Int:
        case TYPE_Byte:
        case TYPE_Bool:
        case TYPE_Float:
        case TYPE_Name:
          ec.EmitPushNumber(0);
          ++SelfOffset;
          break;
        case TYPE_String:
        case TYPE_Pointer:
        case TYPE_Reference:
        case TYPE_Class:
        case TYPE_State:
          ec.AddStatement(OPC_PushNull);
          ++SelfOffset;
          break;
        case TYPE_Vector:
          ec.EmitPushNumber(0);
          ec.EmitPushNumber(0);
          ec.EmitPushNumber(0);
          SelfOffset += 3;
          break;
        default:
          ParseError(Loc, "Bad optional parameter type");
          break;
      }
      ec.EmitPushNumber(0);
      ++SelfOffset;
    } else {
      Args[i]->Emit(ec);
      SelfOffset += (Args[i]->Type.Type == TYPE_Vector ? 3 : 1);
      if (Func->ParamFlags[i]&FPARM_Optional) {
        ec.EmitPushNumber(1);
        ++SelfOffset;
      }
    }
  }

  // some special functions will be converted to builtins
  if ((Func->Flags&(FUNC_Native|FUNC_Static)) == (FUNC_Native|FUNC_Static) && NumArgs == 1 && Func->NumParams == 1) {
    if (Func->ParamTypes[0].Type == TYPE_Name && Func->ReturnType.Type == TYPE_String && Func->GetVName() == VName("NameToStr")) {
      ec.AddStatement(OPC_NameToStr);
      return;
    }
    if (Func->ParamTypes[0].Type == TYPE_String && Func->ReturnType.Type == TYPE_Name && Func->GetVName() == VName("StrToName")) {
      ec.AddStatement(OPC_StrToName);
      return;
    }
    if (Func->ParamTypes[0].Type == TYPE_Float && Func->ReturnType.Type == TYPE_Int && Func->GetVName() == VName("ftoi")) {
      ec.AddStatement(OPC_FloatToInt);
      return;
    }
    if (Func->ParamTypes[0].Type == TYPE_Int && Func->ReturnType.Type == TYPE_Float && Func->GetVName() == VName("itof")) {
      ec.AddStatement(OPC_IntToFloat);
      return;
    }
    if (Func->ParamTypes[0].Type == TYPE_Int && Func->ReturnType.Type == TYPE_Int && Func->GetVName() == VName("abs")) {
      ec.AddStatement(OPC_IntAbs);
      return;
    }
    if (Func->ParamTypes[0].Type == TYPE_Float && Func->ReturnType.Type == TYPE_Float && Func->GetVName() == VName("fabs")) {
      ec.AddStatement(OPC_FloatAbs);
      return;
    }
  }

  if ((Func->Flags&(FUNC_Native|FUNC_Static)) == (FUNC_Native|FUNC_Static) && NumArgs == 2 && Func->NumParams == 2) {
    if (Func->ParamTypes[0].Type == TYPE_Int && Func->ParamTypes[1].Type == TYPE_Int && Func->ReturnType.Type == TYPE_Int) {
      if (Func->GetVName() == VName("Min") || Func->GetVName() == VName("min")) { ec.AddStatement(OPC_IntMin); return; }
      if (Func->GetVName() == VName("Max") || Func->GetVName() == VName("max")) { ec.AddStatement(OPC_IntMax); return; }
    }
    if (Func->ParamTypes[0].Type == TYPE_Float && Func->ParamTypes[1].Type == TYPE_Float && Func->ReturnType.Type == TYPE_Float) {
      if (Func->GetVName() == VName("FMin") || Func->GetVName() == VName("fmin")) { ec.AddStatement(OPC_FloatMin); return; }
      if (Func->GetVName() == VName("FMax") || Func->GetVName() == VName("fmax")) { ec.AddStatement(OPC_FloatMax); return; }
    }
  }

  if ((Func->Flags&(FUNC_Native|FUNC_Static)) == (FUNC_Native|FUNC_Static) && NumArgs == 3 && Func->NumParams == 3) {
    if (Func->ParamTypes[0].Type == TYPE_Int && Func->ParamTypes[1].Type == TYPE_Int && Func->ParamTypes[2].Type == TYPE_Int &&
        Func->ReturnType.Type == TYPE_Int)
    {
      if (Func->GetVName() == VName("Clamp") || Func->GetVName() == VName("clamp")) { ec.AddStatement(OPC_IntClamp); return; }
    }
    if (Func->ParamTypes[0].Type == TYPE_Float && Func->ParamTypes[1].Type == TYPE_Float && Func->ParamTypes[2].Type == TYPE_Float &&
        Func->ReturnType.Type == TYPE_Float)
    {
      if (Func->GetVName() == VName("FClamp") || Func->GetVName() == VName("fclamp")) { ec.AddStatement(OPC_FloatClamp); return; }
    }
  }

       if (DirectCall) ec.AddStatement(OPC_Call, Func);
  else if (DelegateField) ec.AddStatement(OPC_DelegateCall, DelegateField, SelfOffset);
  else ec.AddStatement(OPC_VCall, Func, SelfOffset);
  unguard;
}


//==========================================================================
//
//  VInvocation::FindMethodWithSignature
//
//==========================================================================
VMethod *VInvocation::FindMethodWithSignature (VEmitContext &ec, VName name, int argc, VExpression **argv) {
  if (argc < 0 || argc > VMethod::MAX_PARAMS) return nullptr;
  if (!ec.SelfClass) return nullptr;
  VMethod *m = ec.SelfClass->FindAccessibleMethod(name, ec.SelfClass);
  if (!m) return nullptr;
  if (!IsGoodMethodParams(ec, m, argc, argv)) return nullptr;
  return m;
}


//==========================================================================
//
//  VInvocation::IsGoodMethodParams
//
//  argv are not resolved, but should be resolvable without errors
//
//==========================================================================
bool VInvocation::IsGoodMethodParams (VEmitContext &ec, VMethod *m, int argc, VExpression **argv) {
  if (argc < 0 || argc > VMethod::MAX_PARAMS) return false;
  if (!m) return false;

  // determine parameter count
  int requiredParams = m->NumParams;
  int maxParams = (m->Flags&FUNC_VarArgs ? VMethod::MAX_PARAMS-1 : m->NumParams);

  if (argc > maxParams) return false;

  for (int i = 0; i < argc; ++i) {
    if (i < requiredParams) {
      if (!argv[i]) {
        if (!(m->ParamFlags[i]&FPARM_Optional)) return false; // ommited non-optional
        continue;
      }
      // check for `ref`/`out` validness
      if (argv[i]->IsRefArg() && (m->ParamFlags[i]&FPARM_Ref) == 0) return false;
      if (argv[i]->IsOutArg() && (m->ParamFlags[i]&FPARM_Out) == 0) return false;
      // resolve it
      VExpression *aa = argv[i]->SyntaxCopy()->Resolve(ec);
      if (!aa) return false;
      // other checks
      if (ec.Package->Name == NAME_decorate) {
        switch (m->ParamTypes[i].Type) {
          case TYPE_Int:
          case TYPE_Float:
            if (aa->Type.Type == TYPE_Float || aa->Type.Type == TYPE_Int) { delete aa; continue; }
            break;
        }
      }
      if (m->ParamFlags[i]&(FPARM_Out|FPARM_Ref)) {
        if (!aa->Type.Equals(m->ParamTypes[i])) {
          // check, but don't raise any errors
          if (!aa->Type.CheckMatch(aa->Loc, m->ParamTypes[i], false)) { delete aa; return false; }
        }
      } else {
        if (m->ParamTypes[i].Type == TYPE_Float && aa->Type.Type == TYPE_Int) { delete aa; continue; }
        // check, but don't raise any errors
        if (!aa->Type.CheckMatch(aa->Loc, m->ParamTypes[i], false)) { delete aa; return false; }
      }
      delete aa;
    } else {
      // vararg
      if (!argv[i]) return false;
    }
  }

  while (argc < requiredParams) {
    if (!(m->ParamFlags[argc]&FPARM_Optional)) return false;
    ++argc;
  }

  return true;
}


//==========================================================================
//
//  VInvocation::CheckParams
//
//  argc/argv: non-resolved argument copies)
//
//==========================================================================
void VInvocation::CheckParams (VEmitContext &ec, int argc, VExpression **argv) {
  guard(VInvocation::CheckParams);

  // determine parameter count
  int argsize = 0;
  int requiredParams = Func->NumParams;
  int maxParams = (Func->Flags&FUNC_VarArgs ? VMethod::MAX_PARAMS-1 : Func->NumParams);

  for (int i = 0; i < NumArgs; ++i) {
    if (i < requiredParams) {
      if (!Args[i]) {
        if (!(Func->ParamFlags[i]&FPARM_Optional)) ParseError(Loc, "Cannot omit non-optional argument");
        argsize += Func->ParamTypes[i].GetStackSize();
      } else {
        // check for `ref`/`out` validness
        //if (Args[i]->IsRefArg() && (Func->ParamFlags[i]&FPARM_Ref) == 0) ParseError(Args[i]->Loc, "`ref` argument for non-ref parameter");
        //if (Args[i]->IsOutArg() && (Func->ParamFlags[i]&FPARM_Out) == 0) ParseError(Args[i]->Loc, "`out` argument for non-ref parameter");
        if (ec.Package->Name == NAME_decorate) {
          switch (Func->ParamTypes[i].Type) {
            case TYPE_Int:
              if (Args[i]->IsFloatConst()) {
                int Val = (int)(Args[i]->GetFloatConst());
                TLocation Loc = Args[i]->Loc;
                delete Args[i];
                Args[i] = nullptr;
                Args[i] = new VIntLiteral(Val, Loc);
                Args[i] = Args[i]->Resolve(ec);
              } else if (Args[i]->Type.Type == TYPE_Float) {
                Args[i] = (new VScalarToInt(Args[i]))->Resolve(ec);
              }
              break;
            case TYPE_Float:
              if (Args[i]->IsIntConst()) {
                int Val = Args[i]->GetIntConst();
                TLocation Loc = Args[i]->Loc;
                delete Args[i];
                Args[i] = nullptr;
                Args[i] = new VFloatLiteral(Val, Loc);
                Args[i] = Args[i]->Resolve(ec);
              } else if (Args[i]->Type.Type == TYPE_Int) {
                Args[i] = (new VScalarToFloat(Args[i]))->Resolve(ec);
              }
              break;
          }
        }
        // ref/out args: no int->float conversion allowed
        if (Func->ParamFlags[i]&(FPARM_Out|FPARM_Ref)) {
          if (!Args[i]->Type.Equals(Func->ParamTypes[i])) {
            //FIXME: This should be error
            /*
            if (!(Func->ParamFlags[NumArgs]&FPARM_Optional)) {
              Args[i]->Type.CheckMatch(Args[i]->Loc, Func->ParamTypes[i]);
            }
            */
            if (!Args[i]->Type.CheckMatch(Args[i]->Loc, Func->ParamTypes[i])) {
              ParseError(Args[i]->Loc, "Out parameter types must be equal for arg #%d (want `%s`, but got `%s`)", i+1, *Func->ParamTypes[i].GetName(), *Args[i]->Type.GetName());
            }
          }
          Args[i]->RequestAddressOf();
        } else {
          // normal args: do int->float conversion
          if (Func->ParamTypes[i].Type == TYPE_Float) {
            if (Args[i]->IsIntConst()) {
              int val = Args[i]->GetIntConst();
              TLocation Loc = Args[i]->Loc;
              delete Args[i];
              Args[i] = (new VFloatLiteral(val, Loc))->Resolve(ec); // literal's `Reslove()` does nothing, but...
            } else if (Args[i]->Type.Type == TYPE_Int) {
              VExpression *e = (new VScalarToFloat(argv[i]->SyntaxCopy()))->Resolve(ec);
              if (!e) {
                ParseError(argv[i]->Loc, "Cannot convert argument to float for arg #%d (want `%s`, but got `%s`)", i+1, *Func->ParamTypes[i].GetName(), *Args[i]->Type.GetName());
              } else {
                delete Args[i];
                Args[i] = e;
              }
            }
          }
          Args[i]->Type.CheckMatch(Args[i]->Loc, Func->ParamTypes[i]);
        }
        argsize += Args[i]->Type.GetStackSize();
      }
    } else if (!Args[i]) {
           if (Func->Flags&FUNC_VarArgs) ParseError(Loc, "Cannot omit arguments for vararg function");
      else if (i >= Func->NumParams) ParseError(Loc, "Cannot omit extra arguments for vararg function");
      else ParseError(Loc, "Cannot omit argument (for some reason)");
    } else {
      argsize += Args[i]->Type.GetStackSize();
    }
  }

  if (NumArgs > maxParams) ParseError(Loc, "Incorrect number of arguments, need %d, got %d.", maxParams, NumArgs);

  while (NumArgs < requiredParams) {
    if (Func->ParamFlags[NumArgs]&FPARM_Optional) {
      Args[NumArgs] = nullptr;
      ++NumArgs;
    } else {
      ParseError(Loc, "Incorrect argument count %d, should be %d", NumArgs, requiredParams);
      break;
    }
  }

  if (Func->Flags&FUNC_VarArgs) {
    Args[NumArgs++] = new VIntLiteral(argsize/4-requiredParams, Loc);
  }

  unguard;
}


//==========================================================================
//
//  VInvocation::CheckDecorateParams
//
//==========================================================================
void VInvocation::CheckDecorateParams (VEmitContext &ec) {
  guard(VInvocation::CheckDecorateParams);

  int maxParams;
  int requiredParams = Func->NumParams;
  if (Func->Flags & FUNC_VarArgs) {
    maxParams = VMethod::MAX_PARAMS-1;
  } else {
    maxParams = Func->NumParams;
  }

  if (NumArgs > maxParams) ParseError(Loc, "Incorrect number of arguments, need %d, got %d.", maxParams, NumArgs);

  for (int i = 0; i < NumArgs; ++i) {
    if (i >= requiredParams) continue;
    if (!Args[i]) continue;
    switch (Func->ParamTypes[i].Type) {
      case TYPE_Name:
        if (Args[i]->IsDecorateSingleName()) {
          VDecorateSingleName *E = (VDecorateSingleName *)Args[i];
          Args[i] = new VNameLiteral(*E->Name, E->Loc);
          delete E;
          E = nullptr;
        } else if (Args[i]->IsStrConst()) {
          VStr Val = Args[i]->GetStrConst(ec.Package);
          TLocation ALoc = Args[i]->Loc;
          delete Args[i];
          Args[i] = nullptr;
          Args[i] = new VNameLiteral(*Val, ALoc);
        }
        break;
      case TYPE_String:
        if (Args[i]->IsDecorateSingleName()) {
          VDecorateSingleName *E = (VDecorateSingleName *)Args[i];
          Args[i] = new VStringLiteral(ec.Package->FindString(*E->Name), E->Loc);
          delete E;
          E = nullptr;
        }
        break;
      case TYPE_Class:
        if (Args[i]->IsDecorateSingleName()) {
          VDecorateSingleName *E = (VDecorateSingleName *)Args[i];
          Args[i] = new VStringLiteral(ec.Package->FindString(*E->Name), E->Loc);
          delete E;
          E = nullptr;
        }
        if (Args[i]->IsStrConst()) {
          VStr CName = Args[i]->GetStrConst(ec.Package);
          TLocation ALoc = Args[i]->Loc;
          VClass *Cls = VClass::FindClassNoCase(*CName);
          if (!Cls) {
            ParseWarning(ALoc, "No such class %s", *CName);
            delete Args[i];
            Args[i] = nullptr;
            Args[i] = new VNoneLiteral(ALoc);
          } else if (Func->ParamTypes[i].Class && !Cls->IsChildOf(Func->ParamTypes[i].Class)) {
            ParseWarning(ALoc, "Class %s is not a descendant of %s", *CName, Func->ParamTypes[i].Class->GetName());
            delete Args[i];
            Args[i] = nullptr;
            Args[i] = new VNoneLiteral(ALoc);
          } else {
            delete Args[i];
            Args[i] = nullptr;
            Args[i] = new VClassConstant(Cls, ALoc);
          }
        }
        break;
      case TYPE_State:
        if (Args[i]->IsIntConst()) {
          int Offs = Args[i]->GetIntConst();
          TLocation ALoc = Args[i]->Loc;
          if (Offs < 0) {
            ParseError(ALoc, "Negative state jumps are not allowed");
          } else if (Offs == 0) {
            // 0 means no state
            delete Args[i];
            Args[i] = nullptr;
            Args[i] = new VNoneLiteral(ALoc);
          } else {
            check(CallerState);
            VState *S = CallerState->GetPlus(Offs, true);
            if (!S) {
              ParseError(ALoc, "Bad state jump offset");
            } else {
              delete Args[i];
              Args[i] = nullptr;
              Args[i] = new VStateConstant(S, ALoc);
            }
          }
        } else if (Args[i]->IsStrConst()) {
          VStr Lbl = Args[i]->GetStrConst(ec.Package);
          TLocation ALoc = Args[i]->Loc;
          int DCol = Lbl.IndexOf("::");
          if (DCol >= 0) {
            // jump to a specific parent class state, resolve it and pass value directly
            VStr ClassName(Lbl, 0, DCol);
            VClass *CheckClass;
            if (ClassName.ICmp("Super")) {
              CheckClass = ec.SelfClass->ParentClass;
            } else {
              CheckClass = VClass::FindClassNoCase(*ClassName);
              if (!CheckClass) {
                ParseError(ALoc, "No such class %s", *ClassName);
              } else if (!ec.SelfClass->IsChildOf(CheckClass)) {
                ParseError(ALoc, "%s is not a subclass of %s", ec.SelfClass->GetName(), CheckClass->GetName());
                CheckClass = nullptr;
              }
            }
            if (CheckClass) {
              VStr LblName(Lbl, DCol+2, Lbl.Length()-DCol-2);
              TArray<VName> Names;
              VMemberBase::StaticSplitStateLabel(LblName, Names);
              VStateLabel *StLbl = CheckClass->FindStateLabel(Names, true);
              if (!StLbl) {
                ParseError(ALoc, "No such state %s", *Lbl);
              } else {
                delete Args[i];
                Args[i] = nullptr;
                Args[i] = new VStateConstant(StLbl->State, ALoc);
              }
            }
          } else {
            // it's a virtual state jump
            VExpression *TmpArgs[1];
            TmpArgs[0] = Args[i];
            Args[i] = new VInvocation(nullptr, ec.SelfClass->FindMethodChecked("FindJumpState"), nullptr, false, false, Args[i]->Loc, 1, TmpArgs);
          }
        }
        break;
    }
  }
  unguard;
}


//==========================================================================
//
//  VInvokeWrite::VInvokeWrite
//
//==========================================================================
VInvokeWrite::VInvokeWrite (bool aIsWriteln, const TLocation &ALoc, int ANumArgs, VExpression **AArgs)
  : VInvocationBase(ANumArgs, AArgs, ALoc)
  , isWriteln(aIsWriteln)
{
}


//==========================================================================
//
//  VInvokeWrite::SyntaxCopy
//
//==========================================================================
VExpression *VInvokeWrite::SyntaxCopy () {
  auto res = new VInvokeWrite();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VInvokeWrite::DoResolve
//
//==========================================================================
VExpression *VInvokeWrite::DoResolve (VEmitContext &ec) {
  guard(VInvokeWrite::DoResolve);

  // resolve arguments
  bool ArgsOk = true;
  for (int i = 0; i < NumArgs; ++i) {
    if (Args[i]) {
      Args[i] = Args[i]->Resolve(ec);
      if (!Args[i]) {
        ArgsOk = false;
      } else {
        switch (Args[i]->Type.Type) {
          case TYPE_Int:
          case TYPE_Byte:
          case TYPE_Bool:
          case TYPE_Float:
          case TYPE_Name:
          case TYPE_String:
          case TYPE_Pointer:
          case TYPE_Vector:
          case TYPE_Class:
          case TYPE_State:
          case TYPE_Reference:
          case TYPE_Delegate:
            break;
          case TYPE_Struct:
          case TYPE_Array:
          case TYPE_DynamicArray:
            Args[i]->RequestAddressOf();
            break;
          case TYPE_Void:
          case TYPE_Unknown:
          case TYPE_Automatic:
          default:
            ParseError(Args[i]->Loc, "Cannot write type `%s`", *Args[i]->Type.GetName());
            ArgsOk = false;
            break;


        }
      }
    }
  }

  if (!ArgsOk) {
    delete this;
    return nullptr;
  }

  Type = VFieldType(TYPE_Void);

  return this;
  unguard;
}


//==========================================================================
//
//  VInvokeWrite::Emit
//
//==========================================================================
void VInvokeWrite::Emit (VEmitContext &ec) {
  guard(VInvokeWrite::Emit);
  for (int i = 0; i < NumArgs; ++i) {
    if (!Args[i]) continue;
    Args[i]->Emit(ec);
    //ec.EmitPushNumber(Args[i]->Type.Type);
    ec.AddStatement(OPC_DoWriteOne, Args[i]->Type);
  }
  if (isWriteln) ec.AddStatement(OPC_DoWriteFlush);
  unguard;
}


//==========================================================================
//
//  VInvokeWrite::DoSyntaxCopyTo
//
//==========================================================================
void VInvokeWrite::DoSyntaxCopyTo (VExpression *e) {
  VInvocationBase::DoSyntaxCopyTo(e);
  auto res = (VInvokeWrite *)e;
  res->isWriteln = isWriteln;
}
