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
#include <limits.h>
#include <float.h>
#include <math.h>

// builtin codes
#define BUILTIN_OPCODE_INFO
#include "../progdefs.h"


//==========================================================================
//
//  VDynCastWithVar::VDynCastWithVar
//
//==========================================================================
VDynCastWithVar::VDynCastWithVar (VExpression *awhat, VExpression *adestclass, const TLocation &aloc)
  : VExpression(aloc)
  , what(awhat)
  , destclass(adestclass)
{
}


//==========================================================================
//
//  VDynCastWithVar::~VDynCastWithVar
//
//==========================================================================
VDynCastWithVar::~VDynCastWithVar () {
  delete what; what = nullptr;
  delete destclass; destclass = nullptr;
}


//==========================================================================
//
//  VDynCastWithVar::SyntaxCopy
//
//==========================================================================
VExpression *VDynCastWithVar::SyntaxCopy () {
  auto res = new VDynCastWithVar();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDynCastWithVar::DoSyntaxCopyTo
//
//==========================================================================
void VDynCastWithVar::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDynCastWithVar *)e;
  res->what = (what ? what->SyntaxCopy() : nullptr);
  res->destclass = (destclass ? destclass->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDynCastWithVar::DoResolve
//
//==========================================================================
VExpression *VDynCastWithVar::DoResolve (VEmitContext &ec) {
  if (what) what = what->Resolve(ec);
  if (destclass) destclass = destclass->Resolve(ec);
  if (!what || !destclass) { delete this; return nullptr; }

  if (destclass->Type.Type != TYPE_Class) {
    ParseError(destclass->Loc, "Dynamic cast destination must be of a class type");
    delete this;
    return nullptr;
  }

  if (what->Type.Type != TYPE_Class && what->Type.Type != TYPE_Reference) {
    ParseError(what->Loc, "Dynamic cast destination must be of a class or an object type");
    delete this;
    return nullptr;
  }

  // result of this is of `what` type with replaced class
  Type = destclass->Type;
  Type.Type = what->Type.Type;
  Type.Class = destclass->Type.Class;
  return this;
}


//==========================================================================
//
//  VDynCastWithVar::Emit
//
//==========================================================================
void VDynCastWithVar::Emit (VEmitContext &ec) {
  if (what) what->Emit(ec);
  if (destclass) destclass->Emit(ec);
  ec.AddStatement((what->Type.Type == TYPE_Class ? OPC_DynamicClassCastIndirect : OPC_DynamicCastIndirect), Loc);
}


//==========================================================================
//
//  VDynCastWithVar::toString
//
//==========================================================================
VStr VDynCastWithVar::toString () const {
  return e2s(destclass)+"("+e2s(what)+")";
}


//==========================================================================
//
//  VArgMarshall::VArgMarshall
//
//==========================================================================
VArgMarshall::VArgMarshall (VExpression *ae)
  : VExpression(ae->Loc)
  , e(ae)
  , isRef(false)
  , isOut(false)
  , marshallOpt(false)
  , argName(NAME_None)
{
}


//==========================================================================
//
//  VArgMarshall::VArgMarshall
//
//==========================================================================
VArgMarshall::VArgMarshall (VName aname, const TLocation &aloc)
  : VExpression(aloc)
  , e(nullptr)
  , isRef(false)
  , isOut(false)
  , marshallOpt(false)
  , argName(aname)
{
}


//==========================================================================
//
//  VArgMarshall::~VArgMarshall
//
//==========================================================================
VArgMarshall::~VArgMarshall () {
  delete e;
}


//==========================================================================
//
//  VArgMarshall::DoResolve
//
//==========================================================================
VExpression *VArgMarshall::DoResolve (VEmitContext &ec) {
  if (e) e = e->Resolve(ec);
  if (!e) { delete this; return nullptr; }
  VExpression *res = e;
  e = nullptr;
  delete this;
  return res;
}


//==========================================================================
//
//  VArgMarshall::SyntaxCopy
//
//==========================================================================
VExpression *VArgMarshall::SyntaxCopy () {
  auto res = new VArgMarshall();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VArgMarshall::DoSyntaxCopyTo
//
//==========================================================================
void VArgMarshall::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VArgMarshall *)e;
  res->e = (this->e ? this->e->SyntaxCopy() : nullptr);
  res->isRef = isRef;
  res->isOut = isOut;
  res->marshallOpt = marshallOpt;
  res->argName = argName;
}


//==========================================================================
//
//  VArgMarshall::Emit
//
//==========================================================================
void VArgMarshall::Emit (VEmitContext &ec) {
  Sys_Error("The thing that should not be (VArgMarshall::Emit)");
}


//==========================================================================
//
//  VArgMarshall::IsMarshallArg
//
//==========================================================================
bool VArgMarshall::IsMarshallArg () const {
  return true;
}


//==========================================================================
//
//  VArgMarshall::IsRefArg
//
//==========================================================================
bool VArgMarshall::IsRefArg () const {
  return isRef;
}


//==========================================================================
//
//  VArgMarshall::IsOutArg
//
//==========================================================================
bool VArgMarshall::IsOutArg () const {
  return isOut;
}


//==========================================================================
//
//  VArgMarshall::IsOptMarshallArg
//
//==========================================================================
bool VArgMarshall::IsOptMarshallArg () const {
  return marshallOpt;
}


//==========================================================================
//
//  VArgMarshall::IsDefaultArg
//
//==========================================================================
bool VArgMarshall::IsDefaultArg () const {
  return (e == nullptr);
}


//==========================================================================
//
//  VArgMarshall::IsNamedArg
//
//==========================================================================
bool VArgMarshall::IsNamedArg () const {
  return (argName != NAME_None);
}


//==========================================================================
//
//  VArgMarshall::GetArgName
//
//==========================================================================
VName VArgMarshall::GetArgName () const {
  return argName;
}


//==========================================================================
//
//  VArgMarshall::toString
//
//==========================================================================
VStr VArgMarshall::toString () const {
  VStr res;
  if (isRef) res += "ref ";
  if (isOut) res += "out ";
  if (argName != NAME_None) { res = *argName; res += ":"; }
  res += (e ? e2s(e) : "default");
  if (marshallOpt) res += "!optional";
  return res;
}



//==========================================================================
//
//  VInvocationBase::VInvocationBase
//
//==========================================================================
VInvocationBase::VInvocationBase (int ANumArgs, VExpression **AArgs, const TLocation &ALoc)
  : VExpression(ALoc)
  , NumArgs(ANumArgs)
{
  memset(Args, 0, sizeof(Args)); // why not
  for (int i = 0; i < NumArgs; ++i) Args[i] = AArgs[i];
}


//==========================================================================
//
//  VInvocationBase::~VInvocationBase
//
//==========================================================================
VInvocationBase::~VInvocationBase () {
  for (int i = 0; i < NumArgs; ++i) { delete Args[i]; Args[i] = nullptr; }
  NumArgs = 0;
}


//==========================================================================
//
//  VInvocationBase::DoSyntaxCopyTo
//
//==========================================================================
void VInvocationBase::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VInvocationBase *)e;
  memset(res->Args, 0, sizeof(res->Args));
  res->NumArgs = NumArgs;
  for (int i = 0; i < NumArgs; ++i) res->Args[i] = (Args[i] ? Args[i]->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VInvocationBase::IsAnyInvocation
//
//==========================================================================
bool VInvocationBase::IsAnyInvocation () const {
  return true;
}


//==========================================================================
//
//  VInvocationBase::IsAnyInvocation
//
//==========================================================================
VStr VInvocationBase::args2str () const {
  VStr res("(");
  for (int f = 0; f < NumArgs; ++f) {
    if (f != 0) res += ", ";
    if (Args[f]) res += Args[f]->toString(); else res += "default";
  }
  res += ")";
  return res;
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

  if (ec.SelfClass && ec.SelfClass->ParentClass) {
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
//  VSuperInvocation::GetVMethod
//
//==========================================================================
VMethod *VSuperInvocation::GetVMethod (VEmitContext &ec) {
  if (!ec.SelfClass || !ec.SelfClass->ParentClass) return nullptr;
  return ec.SelfClass->ParentClass->FindAccessibleMethod(Name, ec.SelfClass);
}


//==========================================================================
//
//  VSuperInvocation::IsMethodNameChangeable
//
//==========================================================================
bool VSuperInvocation::IsMethodNameChangeable () const {
  return true;
}


//==========================================================================
//
//  VSuperInvocation::GetMethodName
//
//==========================================================================
VName VSuperInvocation::GetMethodName () const {
  return Name;
}


//==========================================================================
//
//  VSuperInvocation::SetMethodName
//
//==========================================================================
void VSuperInvocation::SetMethodName (VName aname) {
  Name = aname;
}


//==========================================================================
//
//  VSuperInvocation::toString
//
//==========================================================================
VStr VSuperInvocation::toString () const {
  return VStr("::")+(*Name)+args2str();
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
  // look for delegate-typed local var
  int num = ec.CheckForLocalVar(Name);
  if (num != -1) {
    VFieldType tp = ec.GetLocalVarType(num);
    if (tp.Type != TYPE_Class && tp.Type != TYPE_Delegate) {
      ParseError(Loc, "Cannot call non-delegate");
      delete this;
      return nullptr;
    }
    if (tp.Type == TYPE_Delegate) {
      VInvocation *e = new VInvocation(tp.Function, num, Loc, NumArgs, Args);
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }
    // cast to class variable
    if (NumArgs != 1 || !Args[0] || Args[0]->IsDefaultArg()) {
      ParseError(Loc, "Dynamic cast requires one argument");
      delete this;
      return nullptr;
    }
    VExpression *e = new VDynCastWithVar(Args[0], new VLocalVar(num, Loc), Loc);
    NumArgs = 0;
    delete this;
    return e->Resolve(ec);
  }

  VClass *Class = VMemberBase::StaticFindClass(Name);
  if (Class) {
    if (NumArgs != 1 || !Args[0] || Args[0]->IsDefaultArg()) {
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
      if (M->Flags&FUNC_Iterator) {
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
    if (field != nullptr) {
      if (field->Type.Type == TYPE_Delegate) {
        VInvocation *e = new VInvocation(nullptr, field->Func, field, false, false, Loc, NumArgs, Args);
        NumArgs = 0;
        delete this;
        return e->Resolve(ec);
      } else if (field->Type.Type == TYPE_Class) {
        // cast to class variable
        if (NumArgs != 1 || !Args[0] || Args[0]->IsDefaultArg()) {
          ParseError(Loc, "Dynamic cast requires one argument");
          delete this;
          return nullptr;
        }
        VExpression *e = new VDynCastWithVar(Args[0], new VFieldAccess(new VSelf(Loc), field, Loc, 0), Loc);
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
  }

  ParseError(Loc, "Unknown method `%s`", *Name);
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
//  VCastOrInvocation::GetVMethod
//
//==========================================================================
VMethod *VCastOrInvocation::GetVMethod (VEmitContext &ec) {
  if (!ec.SelfClass) return nullptr;
  return ec.SelfClass->FindAccessibleMethod(Name, ec.SelfClass);
}


//==========================================================================
//
//  VCastOrInvocation::IsMethodNameChangeable
//
//==========================================================================
bool VCastOrInvocation::IsMethodNameChangeable () const {
  return true;
}


//==========================================================================
//
//  VCastOrInvocation::GetMethodName
//
//==========================================================================
VName VCastOrInvocation::GetMethodName () const {
  return Name;
}


//==========================================================================
//
//  VCastOrInvocation::SetMethodName
//
//==========================================================================
void VCastOrInvocation::SetMethodName (VName aname) {
  Name = aname;
}


//==========================================================================
//
//  VCastOrInvocation::toString
//
//==========================================================================
VStr VCastOrInvocation::toString () const {
  return VStr(*Name)+args2str();
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

    // allow `length()` for cpp-addicts
    if (MethodName == NAME_Num || MethodName == NAME_Length || MethodName == NAME_length ||
        MethodName == "length1" || MethodName == "length2")
    {
      if (NumArgs != 0) {
        ParseError(Loc, "Cannot call `%s` with arguments", *MethodName);
        delete this;
        return nullptr;
      }
      VExpression *e = new VDynArrayGetNum(SelfExpr, (MethodName == "length1" ? 1 : MethodName == "length2" ? 2 : 0), Loc);
      SelfExpr = nullptr;
      delete this;
      return e->Resolve(ec);
    }

    // set new array size, 1d or 2d
    if (MethodName == "SetSize" || MethodName == "setSize") {
      if (NumArgs < 1 || NumArgs > 2) {
        ParseError(Loc, "Method `%s` expects one or two arguments", *MethodName);
        delete this;
        return nullptr;
      }
      // for 2d case, defaults are allowed
      if (NumArgs == 1 && (!Args[0] || Args[0]->IsDefaultArg())) {
        ParseError(Loc, "Method `%s` does not accept default arguments", *MethodName);
        delete this;
        return nullptr;
      }
      // check ref/out
      for (int f = 0; f < NumArgs; ++f) {
        if (!Args[f]) continue;
        if (Args[f]->IsRefArg()) {
          ParseError(Loc, "Method `%s` cannot has `ref` argument", *MethodName);
          delete this;
          return nullptr;
        }
        if (Args[f]->IsOutArg()) {
          ParseError(Loc, "Method `%s` cannot has `out` argument", *MethodName);
          delete this;
          return nullptr;
        }
      }
      // ok
      //op->Flags &= ~FIELD_ReadOnly;
      SelfExpr->RequestAddressOf();
      VDynArraySetNum *e = new VDynArraySetNum(SelfExpr, Args[0], Args[1], Loc);
      e->asSetSize = true;
      SelfExpr = nullptr;
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }

    if (MethodName == NAME_Insert || VStr::Cmp(*MethodName, "insert") == 0) {
      if (NumArgs == 1) {
        if (Args[0] && Args[0]->GetArgName() == "count") {
          ParseError(Loc, "`count` without index");
          delete this;
          return nullptr;
        }
        // default count is 1
        Args[1] = new VIntLiteral(1, Loc);
        NumArgs = 2;
      }
      if (NumArgs != 2 || !Args[0] || Args[0]->IsDefaultArg() || !Args[1] || Args[1]->IsDefaultArg()) {
        ParseError(Loc, "Insert requires 1 or 2 arguments");
        delete this;
        return nullptr;
      }
      if (Args[0]->IsRefArg() || Args[0]->IsOutArg()) {
        ParseError(Args[0]->Loc, "Insert cannot has `ref`/`out` argument");
        delete this;
        return nullptr;
      }
      if (Args[1]->IsRefArg() || Args[1]->IsOutArg()) {
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
        if (Args[0] && Args[0]->GetArgName() == "count") {
          ParseError(Loc, "`count` without index");
          delete this;
          return nullptr;
        }
        // default count is 1
        Args[1] = new VIntLiteral(1, Loc);
        NumArgs = 2;
      }
      if (NumArgs != 2 || !Args[0] || Args[0]->IsDefaultArg() || !Args[1] || Args[1]->IsDefaultArg()) {
        ParseError(Loc, "Insert requires 1 or 2 arguments");
        delete this;
        return nullptr;
      }
      if (Args[0]->IsRefArg() || Args[0]->IsOutArg()) {
        ParseError(Args[0]->Loc, "Remove cannot has `ref`/`out` argument");
        delete this;
        return nullptr;
      }
      if (Args[1]->IsRefArg() || Args[1]->IsOutArg()) {
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

    // clear array, but don't reallocate
    if (VStr::Cmp(*MethodName, "clear") == 0) {
      if (NumArgs != 0) {
        ParseError(Loc, "`.clear` requires no arguments");
        delete this;
        return nullptr;
      }
      VExpression *e = new VDynArrayClear(SelfExpr, Loc);
      SelfExpr = nullptr;
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }

    if (MethodName == NAME_Remove || VStr::Cmp(*MethodName, "sort") == 0) {
      if (NumArgs != 1) {
        ParseError(Loc, "`.sort` require delegate argument");
        delete this;
        return nullptr;
      }
      if (Args[0] && Args[0]->GetArgName() != NAME_None && Args[0]->GetArgName() != "lessfn") {
        ParseError(Loc, "`.sort` argument name must be `lessfn`");
        delete this;
        return nullptr;
      }
      if (Args[0]->IsDefaultArg()) {
        ParseError(Loc, "`.sort` argument name must not be default");
        delete this;
        return nullptr;
      }
      VExpression *e = new VDynArraySort(SelfExpr, Args[0], Loc);
      SelfExpr = nullptr;
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }

    if (VStr::Cmp(*MethodName, "swap") == 0) {
      if (NumArgs != 2) {
        ParseError(Loc, "`.swap` requires two arguments");
        delete this;
        return nullptr;
      }
      if (Args[0] && Args[0]->GetArgName() != NAME_None && Args[0]->GetArgName() == "idx0") {
        ParseError(Loc, "`.swap` 1st argument name must be `idx0`");
        delete this;
        return nullptr;
      }
      if (Args[1] && Args[1]->GetArgName() != NAME_None && Args[1]->GetArgName() == "idx1") {
        ParseError(Loc, "`.swap` 1st argument name must be `idx1`");
        delete this;
        return nullptr;
      }
      if (!Args[0] || Args[0]->IsDefaultArg() || !Args[1] || Args[1]->IsDefaultArg()) {
        ParseError(Loc, "`.swap` requires two arguments");
        delete this;
        return nullptr;
      }
      if (Args[0]->IsRefArg() || Args[0]->IsOutArg()) {
        ParseError(Loc, "`.swap` arguments cannot be `ref`");
        delete this;
        return nullptr;
      }
      if (Args[1]->IsRefArg() || Args[1]->IsOutArg()) {
        ParseError(Loc, "`.swap` arguments cannot be `out`");
        delete this;
        return nullptr;
      }
      VExpression *e = new VDynArraySwap1D(SelfExpr, Args[0], Args[1], Loc);
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
    // translate method name for some built-in types
    if (SelfExpr->Type.Type == TYPE_String) {
      static const char *knownStrTrans[] = {
        "mid", "strmid",
        "left", "strleft",
        "right", "strright",
        "toLowerCase", "strlwr",
        "toUpperCase", "strupr",
        //"repeat", "strrepeat",
        nullptr,
      };
      for (const char **tr = knownStrTrans; *tr; tr += 2) {
        if (MethodName == *tr) {
          // i found her!
          MethodName = VName(tr[1]);
          break;
        }
      }
    } else if (SelfExpr->Type.Type == TYPE_Float) {
      if (MethodName == "isnan" || MethodName == "isNan" || MethodName == "isNaN" || MethodName == "isNAN" ||
          MethodName == "isinf" || MethodName == "isInf" || MethodName == "isfinite" || MethodName == "isFinite") {
        if (NumArgs != 0) {
          ParseError(Loc, "`float` builtin `%s` cannot have args", *MethodName);
          delete this;
          return nullptr;
        }
        VExpression *e = new VDotField(selfCopy, MethodName, Loc);
        delete this;
        return e->Resolve(ec);
      }
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
    ParseError(Loc, "Object reference expected at the left side of `.`");
    delete selfCopy;
    delete this;
    return nullptr;
  }

  VMethod *M = SelfExpr->Type.Class->FindAccessibleMethod(MethodName, ec.SelfClass);
  if (M) {
    /*
    if (SelfExpr->Type.Class && SelfExpr->Type.Class != ec.SelfClass && !SelfExpr->Type.Class->Defined) {
      fprintf(stderr, "!!!! <%s>\n", SelfExpr->Type.Class->GetName());
      SelfExpr->Type.Class->Define();
    }
    fprintf(stderr, "!!!! CALL: <%s>\n", *M->GetFullName());
    */
    // don't need it anymore
    delete selfCopy;
    if (M->Flags&FUNC_Iterator) {
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
    ParseError(Loc, "`%s` is not an iterator method", *MethodName);
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
//  VDotInvocation::GetVMethod
//
//==========================================================================
VMethod *VDotInvocation::GetVMethod (VEmitContext &ec) {
  if (!ec.SelfClass || !SelfExpr) return nullptr;

  VGagErrors gag;

  VExpression *eself = SelfExpr->SyntaxCopy()->Resolve(ec);
  if (!eself) return nullptr;

  if (eself->Type.Type == TYPE_Reference || eself->Type.Type == TYPE_Class) {
    VMethod *res = eself->Type.Class->FindAccessibleMethod(MethodName, ec.SelfClass);
    delete eself;
    return res;
  }

  delete eself;
  return nullptr;
}


//==========================================================================
//
//  VDotInvocation::IsMethodNameChangeable
//
//==========================================================================
bool VDotInvocation::IsMethodNameChangeable () const {
  return true;
}


//==========================================================================
//
//  VDotInvocation::GetMethodName
//
//==========================================================================
VName VDotInvocation::GetMethodName () const {
  return MethodName;
}


//==========================================================================
//
//  VDotInvocation::SetMethodName
//
//==========================================================================
void VDotInvocation::SetMethodName (VName aname) {
  MethodName = aname;
}


//==========================================================================
//
//  VDotInvocation::toString
//
//==========================================================================
VStr VDotInvocation::toString () const {
  return e2s(SelfExpr)+"."+VStr(*MethodName)+args2str();
}



//==========================================================================
//
//  VTypeInvocation::VTypeInvocation
//
//==========================================================================
VTypeInvocation::VTypeInvocation (VExpression *aTypeExpr, VName aMethodName, const TLocation &aloc, int argc, VExpression **argv)
  : VInvocationBase(argc, argv, aloc)
  , TypeExpr(aTypeExpr)
  , MethodName(aMethodName)
{
}


//==========================================================================
//
//  VTypeInvocation::~VTypeInvocation
//
//==========================================================================
VTypeInvocation::~VTypeInvocation () {
  delete TypeExpr; TypeExpr = nullptr;
}


//==========================================================================
//
//  VTypeInvocation::SyntaxCopy
//
//==========================================================================
VExpression *VTypeInvocation::SyntaxCopy () {
  auto res = new VTypeInvocation();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VTypeInvocation::~VTypeInvocation
//
//==========================================================================
void VTypeInvocation::DoSyntaxCopyTo (VExpression *e) {
  VInvocationBase::DoSyntaxCopyTo(e);
  auto res = (VTypeInvocation *)e;
  res->TypeExpr = (TypeExpr ? TypeExpr->SyntaxCopy() : nullptr);
  res->MethodName = MethodName;
}


//==========================================================================
//
//  VTypeInvocation::DoResolve
//
//==========================================================================
VExpression *VTypeInvocation::DoResolve (VEmitContext &ec) {
  if (!TypeExpr) return nullptr;
  TypeExpr = TypeExpr->ResolveAsType(ec);
  if (!TypeExpr || !TypeExpr->IsTypeExpr()) {
    ParseError(Loc, "Type expected");
    delete this;
    return nullptr;
  }

  // `int` properties
  if (TypeExpr->Type.Type == TYPE_Int) {
    if (MethodName == "min") {
      if (NumArgs != 0) { ParseError(Loc, "`int.%s` cannot have arguments", *MethodName); delete this; return nullptr; }
      VExpression *e = (new VIntLiteral((int)0x80000000, Loc))->Resolve(ec);
      delete this;
      return e;
    }
    if (MethodName == "max") {
      if (NumArgs != 0) { ParseError(Loc, "`int.%s` cannot have arguments", *MethodName); delete this; return nullptr; }
      VExpression *e = (new VIntLiteral((int)0x7fffffff, Loc))->Resolve(ec);
      delete this;
      return e;
    }
    ParseError(Loc, "invalid `int` property `%s`", *MethodName);
    delete this;
    return nullptr;
  }

  // `float` properties
  if (TypeExpr->Type.Type == TYPE_Float) {
    if (MethodName == "min") {
      if (NumArgs != 0) { ParseError(Loc, "`float.%s` cannot have arguments", *MethodName); delete this; return nullptr; }
      VExpression *e = (new VFloatLiteral(-FLT_MAX, Loc))->Resolve(ec);
      delete this;
      return e;
    }
    if (MethodName == "max") {
      if (NumArgs != 0) { ParseError(Loc, "`float.%s` cannot have arguments", *MethodName); delete this; return nullptr; }
      VExpression *e = (new VFloatLiteral(FLT_MAX, Loc))->Resolve(ec);
      delete this;
      return e;
    }
    if (MethodName == "min_norm" || MethodName == "min_normal" || MethodName == "min_normalized") {
      if (NumArgs != 0) { ParseError(Loc, "`float.%s` cannot have arguments", *MethodName); delete this; return nullptr; }
      VExpression *e = (new VFloatLiteral(FLT_MIN, Loc))->Resolve(ec);
      delete this;
      return e;
    }
    if (MethodName == "nan") {
      if (NumArgs != 0) { ParseError(Loc, "`float.%s` cannot have arguments", *MethodName); delete this; return nullptr; }
      VExpression *e = (new VFloatLiteral(NAN, Loc))->Resolve(ec);
      delete this;
      return e;
    }
    if (MethodName == "inf" || MethodName == "infinity") {
      if (NumArgs != 0) { ParseError(Loc, "`float.%s` cannot have arguments", *MethodName); delete this; return nullptr; }
      VExpression *e = (new VFloatLiteral(INFINITY, Loc))->Resolve(ec);
      delete this;
      return e;
    }
    ParseError(Loc, "invalid `float` property `%s`", *MethodName);
    delete this;
    return nullptr;
  }

  // `string` properties
  if (TypeExpr->Type.Type == TYPE_String) {
    VClass *cls = VClass::FindClass("Object");
    if (cls) {
      const char *newMethod = nullptr;
           if (MethodName == "repeat") newMethod = "strrepeat";
      else if (MethodName == "fromChar") newMethod = "strFromChar";
      else if (MethodName == "fromCharUtf8") newMethod = "strFromCharUtf8";
      else if (MethodName == "fromInt") newMethod = "strFromInt";
      else if (MethodName == "fromFloat") newMethod = "strFromFloat";
      if (newMethod) {
        // convert to invocation
        VExpression *e = new VInvocation(nullptr, cls->FindMethodChecked(newMethod), nullptr, false, false, Loc, NumArgs, Args);
        NumArgs = 0; // don't clear args
        e = e->Resolve(ec);
        delete this;
        return e;
      }
    }
    ParseError(Loc, "invalid `string` property `%s`", *MethodName);
    delete this;
    return nullptr;
  }

  // `name` properties
  if (TypeExpr->Type.Type == TYPE_Name) {
    if (MethodName == "none" || MethodName == "empty") {
      if (NumArgs != 0) { ParseError(Loc, "`name.%s` cannot have arguments", *MethodName); delete this; return nullptr; }
      VExpression *e = (new VNameLiteral(NAME_None, Loc))->Resolve(ec);
      delete this;
      return e;
    }
    ParseError(Loc, "invalid `name` property `%s`", *MethodName);
    delete this;
    return nullptr;
  }

  ParseError(Loc, "invalid `%s` property `%s`", *TypeExpr->Type.GetName(), *MethodName);
  delete this;
  return nullptr;
}


//==========================================================================
//
//  VTypeInvocation::Emit
//
//==========================================================================
void VTypeInvocation::Emit (VEmitContext &) {
  ParseError(Loc, "Should not happen (VTypeInvocation)");
}


//==========================================================================
//
//  VTypeInvocation::GetVMethod
//
//==========================================================================
VMethod *VTypeInvocation::GetVMethod (VEmitContext &ec) {
  return nullptr;
}


//==========================================================================
//
//  VTypeInvocation::IsMethodNameChangeable
//
//==========================================================================
bool VTypeInvocation::IsMethodNameChangeable () const {
  return false;
}


//==========================================================================
//
//  VTypeInvocation::GetMethodName
//
//==========================================================================
VName VTypeInvocation::GetMethodName () const {
  return MethodName;
}


//==========================================================================
//
//  VTypeInvocation::SetMethodName
//
//==========================================================================
void VTypeInvocation::SetMethodName (VName aname) {
  MethodName = aname;
}


//==========================================================================
//
//  VTypeInvocation::toString
//
//==========================================================================
VStr VTypeInvocation::toString () const {
  return e2s(TypeExpr)+"."+VStr(*MethodName)+args2str();
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
  , DelegateLocal(-666)
  , HaveSelf(AHaveSelf)
  , BaseCall(ABaseCall)
  , CallerState(nullptr)
  , MultiFrameState(false)
{
}


//==========================================================================
//
//  VInvocation::VInvocation
//
//==========================================================================
VInvocation::VInvocation (VMethod *AFunc, int ADelegateLocal, const TLocation &ALoc, int ANumArgs, VExpression **AArgs)
  : VInvocationBase(ANumArgs, AArgs, ALoc)
  , SelfExpr(nullptr)
  , Func(AFunc)
  , DelegateField(nullptr)
  , DelegateLocal(ADelegateLocal)
  , HaveSelf(false)
  , BaseCall(false)
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
  // no need to copy private fields
  res->SelfExpr = (SelfExpr ? SelfExpr->SyntaxCopy() : nullptr);
  res->Func = Func;
  res->DelegateField = DelegateField;
  res->DelegateLocal = DelegateLocal;
  res->HaveSelf = HaveSelf;
  res->BaseCall = BaseCall;
  res->CallerState = CallerState;
  res->MultiFrameState = MultiFrameState;
}


//==========================================================================
//
//  VInvocation::RebuildArgs
//
//==========================================================================
bool VInvocation::RebuildArgs () {
  if (NumArgs == 0) return true;
  int maxParams = (Func->Flags&FUNC_VarArgs ? VMethod::MAX_PARAMS-1 : Func->NumParams);
  if (NumArgs > maxParams) {
    ParseError(Loc, "Too many method arguments");
    return false;
  }
  VExpression *newArgs[VMethod::MAX_PARAMS+1];
  bool isSet[VMethod::MAX_PARAMS+1];
  for (int f = 0; f < VMethod::MAX_PARAMS+1; ++f) { newArgs[f] = nullptr; isSet[f] = false; }
  int firstNamed = -1;
  for (int f = 0; f < NumArgs; ++f) {
    if (Args[f] && Args[f]->IsNamedArg()) {
      firstNamed = f;
      break;
    }
    newArgs[f] = Args[f];
    isSet[f] = true;
  }
  if (firstNamed < 0) return true; // nothing to do
  // do actual rebuild
  int newNumArgs = firstNamed;
  int newIdx = -1;
  for (; firstNamed < NumArgs; ++firstNamed) {
    if (Args[firstNamed] && Args[firstNamed]->IsNamedArg()) {
      // named
      int realIdx = Func->FindArgByName(Args[firstNamed]->GetArgName());
      //HACK: ending underscore means "nobody cares"
      if (realIdx < 0 && firstNamed < Func->NumParams) {
        const char *ss = *Func->Params[firstNamed].Name;
        if (ss[0] && ss[strlen(ss)-1] == '_') realIdx = firstNamed;
      }
      if (realIdx < 0) {
        ParseError(Args[firstNamed]->Loc, "no argument named `%s`", *Args[firstNamed]->GetArgName());
        return false;
      }
      if (isSet[realIdx]) {
        ParseError(Args[firstNamed]->Loc, "duplicate argument named `%s`", *Args[firstNamed]->GetArgName());
        return false;
      }
      newIdx = realIdx;
    } else {
      // unnamed
      if (newIdx < 0) FatalError("VC: internal compiler error (VInvocation::RebuildArgs)");
      if (newIdx >= maxParams) {
        ParseError(Loc, "Too many method arguments");
        return false;
      }
      if (isSet[newIdx]) {
        ParseError((Args[firstNamed] ? Args[firstNamed]->Loc : Loc), "duplicate argument #%d", newIdx+1);
        return false;
      }
    }
    isSet[newIdx] = true;
    newArgs[newIdx] = Args[firstNamed];
    ++newIdx;
    if (newIdx > newNumArgs) newNumArgs = newIdx;
  }
  // copy new list
  NumArgs = newNumArgs;
  for (int f = 0; f < newNumArgs; ++f) Args[f] = newArgs[f];
  return true;
}


//==========================================================================
//
//  VInvocation::DoResolve
//
//==========================================================================
VExpression *VInvocation::DoResolve (VEmitContext &ec) {
  guard(VInvocation::DoResolve);

  if (ec.Package->Name == NAME_decorate) CheckDecorateParams(ec);

  if (DelegateLocal >= 0) {
    //FIXME
    const VLocalVarDef &loc = ec.GetLocalByIndex(DelegateLocal);
    if (loc.ParamFlags&(FPARM_Out|FPARM_Ref)) {
      ParseError(Loc, "ref locals arent supported yet (sorry)");
      delete this;
      return nullptr;
    }
  }

  memset(optmarshall, 0, sizeof(optmarshall));

  // resolve arguments
  int requiredParams = Func->NumParams;
  int maxParams = (Func->Flags&FUNC_VarArgs ? VMethod::MAX_PARAMS-1 : Func->NumParams);
  bool ArgsOk = RebuildArgs();
  if (!ArgsOk) { delete this; return nullptr; }
  for (int i = 0; i < NumArgs; ++i) {
    if (i >= maxParams) {
      ParseError((Args[i] ? Args[i]->Loc : Loc), "Too many method arguments");
      ArgsOk = false;
      break;
    }
    if (Args[i] && Args[i]->IsDefaultArg()) {
      // unpack it, why not
      delete Args[i];
      Args[i] = nullptr;
    }
    if (Args[i]) {
      if (i < VMethod::MAX_PARAMS && Args[i]->IsOptMarshallArg() && (Func->ParamFlags[i]&FPARM_Optional) != 0) {
        optmarshall[i] = true;
      }
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
    delete this;
    return nullptr;
  }

  CheckParams(ec);

  Type = Func->ReturnType;
  if (Type.Type == TYPE_Byte || Type.Type == TYPE_Bool) Type = VFieldType(TYPE_Int);
  if (Func->Flags&FUNC_Spawner) Type.Class = Args[0]->Type.Class;

  // we may create new locals, so activate local reuse mechanics
  int compIdx = ec.EnterCompound();

  for (int f = 0; f < VMethod::MAX_PARAMS; ++f) {
    lcidx[f] = -1;
    reused[f] = false;
  }

  // for ommited "optional ref", create temporary locals
  for (int i = 0; i < NumArgs; ++i) {
    if (!Args[i] && i < VMethod::MAX_PARAMS) {
      if ((Func->ParamFlags[i]&(FPARM_Out|FPARM_Ref)) != 0) {
        // create temporary
        VLocalVarDef &L = ec.AllocLocal(NAME_None, Func->ParamTypes[i], Loc);
        L.Visible = false; // it is unnamed, and hidden ;-)
        L.ParamFlags = 0;
        //index = new VLocalVar(L.ldindex, L.Loc);
        lcidx[i] = L.ldindex;
        reused[i] = L.reused;
      }
    }
  }

  ec.ExitCompound(compIdx);

  // some special functions will be converted to builtins, try to const-optimize 'em
  if (Func->builtinOpc >= 0) return OptimizeBuiltin(ec);

  return this;
  unguard;
}


//==========================================================================
//
//  VInvocation::CheckSimpleConstArgs
//
//  used by `OptimizeBuiltin`; `types` are `TYPE_xxx`
//
//==========================================================================
bool VInvocation::CheckSimpleConstArgs (int argc, const int *types) const {
  if (argc != NumArgs) return false;
  for (int f = 0; f < argc; ++f) {
    if (!Args[f]) return false; // cannot omit anything (yet)
    switch (types[f]) {
      case TYPE_Int: if (!Args[f]->IsIntConst()) return false; break;
      case TYPE_Float: if (!Args[f]->IsFloatConst()) return false; break;
      case TYPE_String: if (!Args[f]->IsStrConst()) return false; break;
      case TYPE_Name: if (!Args[f]->IsNameConst()) return false; break;
      case TYPE_Vector: if (!Args[f]->IsConstVectorCtor()) return false; break;
      default: return false; // unknown request
    }
  }
  return true;
}


//==========================================================================
//
//  VInvocation::OptimizeBuiltin
//
//==========================================================================
VExpression *VInvocation::OptimizeBuiltin (VEmitContext &ec) {
  if (!Func || Func->builtinOpc < 0) return this; // sanity check
  TVec v0, v1;
  VExpression *e = nullptr;
  switch (Func->builtinOpc) {
    case OPC_Builtin_IntAbs:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Int})) return this;
      e = new VIntLiteral((Args[0]->GetIntConst() < 0 ? -Args[0]->GetIntConst() : Args[0]->GetIntConst()), Loc);
      break;
    case OPC_Builtin_FloatAbs:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VFloatLiteral((Args[0]->GetFloatConst() < 0 ? -Args[0]->GetFloatConst() : Args[0]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_IntSign:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Int})) return this;
      e = new VIntLiteral((Args[0]->GetIntConst() < 0 ? -1 : Args[0]->GetIntConst() > 0 ? 1 : 0), Loc);
      break;
    case OPC_Builtin_FloatSign:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VFloatLiteral((Args[0]->GetFloatConst() < 0 ? -1.0f : Args[0]->GetFloatConst() > 0 ? 1.0f : 0.0f), Loc);
      break;
    case OPC_Builtin_IntMin:
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Int, TYPE_Int})) return this;
      e = new VIntLiteral((Args[0]->GetIntConst() < Args[1]->GetIntConst() ? Args[0]->GetIntConst() : Args[1]->GetIntConst()), Loc);
      break;
    case OPC_Builtin_IntMax:
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Int, TYPE_Int})) return this;
      e = new VIntLiteral((Args[0]->GetIntConst() > Args[1]->GetIntConst() ? Args[0]->GetIntConst() : Args[1]->GetIntConst()), Loc);
      break;
    case OPC_Builtin_FloatMin:
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Float, TYPE_Float})) return this;
      e = new VFloatLiteral((Args[0]->GetFloatConst() < Args[1]->GetFloatConst() ? Args[0]->GetFloatConst() : Args[1]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_FloatMax:
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Float, TYPE_Float})) return this;
      e = new VFloatLiteral((Args[0]->GetFloatConst() > Args[1]->GetFloatConst() ? Args[0]->GetFloatConst() : Args[1]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_IntClamp: // (val, min, max)
      if (!CheckSimpleConstArgs(3, (const int []){TYPE_Int, TYPE_Int, TYPE_Int})) return this;
      e = new VIntLiteral(
        (Args[0]->GetIntConst() < Args[1]->GetIntConst() ? Args[1]->GetIntConst() :
         Args[0]->GetIntConst() > Args[2]->GetIntConst() ? Args[2]->GetIntConst() :
         Args[0]->GetIntConst()), Loc);
      break;
    case OPC_Builtin_FloatClamp: // (val, min, max)
      if (!CheckSimpleConstArgs(3, (const int []){TYPE_Float, TYPE_Float, TYPE_Float})) return this;
      e = new VFloatLiteral(
        (Args[0]->GetFloatConst() < Args[1]->GetFloatConst() ? Args[1]->GetFloatConst() :
         Args[0]->GetFloatConst() > Args[2]->GetFloatConst() ? Args[2]->GetFloatConst() :
         Args[0]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_FloatIsNaN:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VIntLiteral((isNaNF(Args[0]->GetFloatConst()) ? 1 : 0), Loc);
      break;
    case OPC_Builtin_FloatIsInf:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VIntLiteral((isInfF(Args[0]->GetFloatConst()) ? 1 : 0), Loc);
      break;
    case OPC_Builtin_FloatIsFinite:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VIntLiteral((isFiniteF(Args[0]->GetFloatConst()) ? 1 : 0), Loc);
      break;
    case OPC_Builtin_DegToRad:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VFloatLiteral(DEG2RAD(Args[0]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_RadToDeg:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VFloatLiteral(RAD2DEG(Args[0]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_Sin:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VFloatLiteral(msin(Args[0]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_Cos:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VFloatLiteral(mcos(Args[0]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_Tan:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VFloatLiteral(mtan(Args[0]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_ASin:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VFloatLiteral(masin(Args[0]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_ACos:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VFloatLiteral(acos(Args[0]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_ATan:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VFloatLiteral(RAD2DEG(atan(Args[0]->GetFloatConst())), Loc);
      break;
    case OPC_Builtin_Sqrt:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VFloatLiteral(sqrt(Args[0]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_ATan2:
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Float, TYPE_Float})) return this;
      e = new VFloatLiteral(matan(Args[0]->GetFloatConst(), Args[1]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_VecLength:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Vector})) return this;
      v0 = ((VVector *)Args[0])->GetConstValue();
      e = new VFloatLiteral(v0.length(), Loc);
      break;
    case OPC_Builtin_VecLength2D:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Vector})) return this;
      v0 = ((VVector *)Args[0])->GetConstValue();
      e = new VFloatLiteral(v0.length2D(), Loc);
      break;
    case OPC_Builtin_VecNormalize:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Vector})) return this;
      v0 = ((VVector *)Args[0])->GetConstValue();
      v0 = normalise(v0);
      e = new VVector(v0, Loc);
      break;
    case OPC_Builtin_VecNormalize2D:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Vector})) return this;
      v0 = ((VVector *)Args[0])->GetConstValue();
      v0 = normalise2D(v0);
      e = new VVector(v0, Loc);
      break;
    case OPC_Builtin_VecDot:
      //fprintf(stderr, "op0=<%s>; op1=<%s>\n", *Args[0]->toString(), *Args[1]->toString());
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Vector, TYPE_Vector})) return this;
      v0 = ((VVector *)Args[0])->GetConstValue();
      v1 = ((VVector *)Args[1])->GetConstValue();
      e = new VFloatLiteral(dot(v0, v1), Loc);
      break;
    case OPC_Builtin_VecDot2D:
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Vector, TYPE_Vector})) return this;
      v0 = ((VVector *)Args[0])->GetConstValue();
      v1 = ((VVector *)Args[1])->GetConstValue();
      e = new VFloatLiteral(dot2D(v0, v1), Loc);
      break;
    case OPC_Builtin_VecCross:
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Vector, TYPE_Vector})) return this;
      v0 = ((VVector *)Args[0])->GetConstValue();
      v1 = ((VVector *)Args[1])->GetConstValue();
      v0 = cross(v0, v1);
      e = new VVector(v0, Loc);
      break;
    case OPC_Builtin_VecCross2D:
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Vector, TYPE_Vector})) return this;
      v0 = ((VVector *)Args[0])->GetConstValue();
      v1 = ((VVector *)Args[1])->GetConstValue();
      e = new VFloatLiteral(cross2D(v0, v1), Loc);
      break;
    case OPC_Builtin_RoundF2I:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VIntLiteral((int)roundf(Args[0]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_RoundF2F:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VFloatLiteral(roundf(Args[0]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_TruncF2I:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VIntLiteral((int)truncf(Args[0]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_TruncF2F:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VFloatLiteral(truncf(Args[0]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_FloatCeil:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VFloatLiteral(ceilf(Args[0]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_FloatFloor:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VFloatLiteral(floorf(Args[0]->GetFloatConst()), Loc);
      break;
    // [-3]: a; [-2]: b, [-1]: delta
    case OPC_Builtin_FloatLerp:
      if (!CheckSimpleConstArgs(3, (const int []){TYPE_Float, TYPE_Float, TYPE_Float})) return this;
      e = new VFloatLiteral(Args[0]->GetFloatConst()+(Args[1]->GetFloatConst()-Args[0]->GetFloatConst())*Args[2]->GetFloatConst(), Loc);
      break;
    case OPC_Builtin_IntLerp:
      if (!CheckSimpleConstArgs(3, (const int []){TYPE_Int, TYPE_Int, TYPE_Float})) return this;
      e = new VIntLiteral((int)roundf(Args[0]->GetIntConst()+(Args[1]->GetIntConst()-Args[0]->GetIntConst())*Args[2]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_FloatSmoothStep:
      if (!CheckSimpleConstArgs(3, (const int []){TYPE_Float, TYPE_Float, TYPE_Float})) return this;
      e = new VFloatLiteral(smoothstep(Args[0]->GetFloatConst(), Args[1]->GetFloatConst(), Args[2]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_FloatSmoothStepPerlin:
      if (!CheckSimpleConstArgs(3, (const int []){TYPE_Float, TYPE_Float, TYPE_Float})) return this;
      e = new VFloatLiteral(smoothstepPerlin(Args[0]->GetFloatConst(), Args[1]->GetFloatConst(), Args[2]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_NameToInt:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Name})) return this;
      e = new VIntLiteral(Args[0]->GetNameConst().GetIndex(), Loc);
      break;
    default: break;
  }
  if (e) {
    delete this;
    return e->Resolve(ec);
  }
  return this;
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

  if (DelegateLocal >= 0) {
    //ec.EmitPushNumber(0, Loc); // `self`, will be replaced by executor
    ec.AddStatement(OPC_PushNull, Loc); // `self`, will be replaced by executor
  } else {
    if (Func->Flags&FUNC_Static) {
      if (HaveSelf) ParseError(Loc, "Invalid static function call");
    } else {
      if (!HaveSelf) {
        if (ec.CurrentFunc->Flags&FUNC_Static) ParseError(Loc, "An object is required to call non-static methods");
        ec.AddStatement(OPC_LocalValue0, Loc);
      }
    }
  }

  vint32 SelfOffset = 1;
  for (int i = 0; i < NumArgs; ++i) {
    if (!Args[i]) {
      if (i < Func->NumParams && (Func->ParamFlags[i]&(FPARM_Out|FPARM_Ref)) != 0) {
        // get local address
        if (lcidx[i] < 0) FatalError("VC: Internal compiler error (VInvocation::Emit)");
        // make sure struct / class field offsets have been calculated
        if (Func->ParamTypes[i].Type == TYPE_Struct) {
          Func->ParamTypes[i].Struct->PostLoad();
        }
        if (reused[i]) ec.EmitOneLocalDtor(lcidx[i], Loc, true);
        const VLocalVarDef &loc = ec.GetLocalByIndex(lcidx[i]);
        ec.EmitLocalAddress(loc.Offset, Loc);
        //ec.AddStatement(OPC_ZeroByPtrNoDrop, Func->ParamTypes[i].GetSize(), Loc);
        //ec.EmitLocalAddress(loc.Offset, Loc);
        ++SelfOffset; // pointer
      } else {
        // nonref
        switch (Func->ParamTypes[i].Type) {
          case TYPE_Int:
          case TYPE_Byte:
          case TYPE_Bool:
          case TYPE_Float:
          case TYPE_Name:
            ec.EmitPushNumber(0, Loc);
            ++SelfOffset;
            break;
          case TYPE_String:
          case TYPE_Pointer:
          case TYPE_Reference:
          case TYPE_Class:
          case TYPE_State:
            ec.AddStatement(OPC_PushNull, Loc);
            ++SelfOffset;
            break;
          case TYPE_Vector:
            ec.EmitPushNumber(0, Loc);
            ec.EmitPushNumber(0, Loc);
            ec.EmitPushNumber(0, Loc);
            SelfOffset += 3;
            break;
          case TYPE_Delegate:
            ec.AddStatement(OPC_PushNull, Loc);
            ec.AddStatement(OPC_PushNull, Loc);
            SelfOffset += 2;
            break;
          default:
            // optional?
            ParseError(Loc, "Bad optional parameter type");
            break;
        }
      }
      // omited ref args can be non-optional
      if ((Func->ParamFlags[i]&FPARM_Optional) != 0) {
        ec.EmitPushNumber(0, Loc);
        ++SelfOffset;
      }
    } else {
      Args[i]->Emit(ec);
      if (Func->ParamFlags[i]&(FPARM_Out|FPARM_Ref)) {
        SelfOffset += 1;
      } else {
        //SelfOffset += (Args[i]->Type.Type == TYPE_Vector ? 3 : Args[i]->Type.Type == TYPE_Delegate ? 2 : 1);
        SelfOffset += Args[i]->Type.GetStackSize()/4;
      }
      if (Func->ParamFlags[i]&FPARM_Optional) {
        // marshall "specified_*"?
        if (i < VMethod::MAX_PARAMS && optmarshall[i] && Args[i]->IsLocalVarExpr()) {
          VLocalVar *ve = (VLocalVar *)Args[i];
          const VLocalVarDef &L = ec.GetLocalByIndex(ve->num);
          if (L.Name == NAME_None) {
            // unnamed, no "specified_*"
            ec.EmitPushNumber(1, Loc);
          } else {
            VStr spname = VStr("specified_")+(*L.Name);
            int lidx = ec.CheckForLocalVar(VName(*spname));
            if (lidx < 0) {
              // not found
              ec.EmitPushNumber(1, Loc);
            } else {
              const VLocalVarDef &LL = ec.GetLocalByIndex(lidx);
              if (LL.Type.Type != TYPE_Int) {
                // not int
                ec.EmitPushNumber(1, Loc);
              } else {
                // i found her!
                //HACK: it is safe (and necessary) to resolve here
                VExpression *xlv = new VLocalVar(lidx, ve->Loc);
                xlv = xlv->Resolve(ec);
                if (!xlv) FatalError("VC: internal compiler error (13496)");
                xlv->Emit(ec);
              }
            }
          }
        } else {
          ec.EmitPushNumber(1, Loc);
        }
        ++SelfOffset;
      }
    }
    // push type for varargs (except the last arg, as it is a simple int counter)
    if (Func->printfFmtArgIdx >= 0 && (Func->Flags&FUNC_VarArgs) != 0 && i >= Func->NumParams && i != NumArgs-1) {
      if (Args[i]) {
        ec.AddStatement(OPC_DoPushTypePtr, Args[i]->Type, Loc);
      } else {
        auto vtp = VFieldType(TYPE_Void);
        ec.AddStatement(OPC_DoPushTypePtr, vtp, Loc);
      }
      ++SelfOffset;
    }
  }

  // some special functions will be converted to builtins
  if (Func->builtinOpc >= 0) {
    ec.AddBuiltin(Func->builtinOpc, Loc);
    return;
  }

  if (DirectCall) {
    ec.AddStatement(OPC_Call, Func, SelfOffset, Loc);
  } else if (DelegateField) {
    ec.AddStatement(OPC_DelegateCall, DelegateField, SelfOffset, Loc);
  } else if (DelegateLocal >= 0) {
    // get address of local
    const VLocalVarDef &loc = ec.GetLocalByIndex(DelegateLocal);
    ec.EmitLocalAddress(loc.Offset, Loc);
    ec.AddStatement(OPC_DelegateCallPtr, loc.Type, SelfOffset, Loc);
  } else {
    ec.AddStatement(OPC_VCall, Func, SelfOffset, Loc);
  }
  unguard;
}


//==========================================================================
//
//  VInvocation::IsLLInvocation
//
//==========================================================================
bool VInvocation::IsLLInvocation () const {
  return true;
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

  VGagErrors gag;

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
          if (!aa->Type.CheckMatch(true, aa->Loc, m->ParamTypes[i], false)) { delete aa; return false; }
        }
      } else {
        if (m->ParamTypes[i].Type == TYPE_Float && aa->Type.Type == TYPE_Int) { delete aa; continue; }
        // check, but don't raise any errors
        if (!aa->Type.CheckMatch(false, aa->Loc, m->ParamTypes[i], false)) { delete aa; return false; }
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

  return !gag.wasErrors();
}


//==========================================================================
//
//  VInvocation::CheckParams
//
//  argc/argv: non-resolved argument copies)
//
//==========================================================================
void VInvocation::CheckParams (VEmitContext &ec) {
  guard(VInvocation::CheckParams);

  // determine parameter count
  int argsize = 0;
  int requiredParams = Func->NumParams;
  int maxParams = (Func->Flags&FUNC_VarArgs ? VMethod::MAX_PARAMS-1 : Func->NumParams);

  for (int i = 0; i < NumArgs; ++i) {
    if (i < requiredParams) {
      if (!Args[i]) {
        if ((Func->ParamFlags[i]&(FPARM_Out|FPARM_Ref)) != 0) {
          argsize += 4; // pointer
        } else {
          if (!(Func->ParamFlags[i]&FPARM_Optional)) ParseError(Loc, "Cannot omit non-optional argument #%d", i+1);
          argsize += Func->ParamTypes[i].GetStackSize();
        }
      } else if ((Args[i]->IsNoneLiteral() || Args[i]->IsNullLiteral()) && (Func->ParamFlags[i]&(FPARM_Out|FPARM_Ref)) != 0) {
        // `ref`/`out` arg can be ommited with `none` or `null`
        delete Args[i];
        Args[i] = nullptr;
        argsize += 4; // pointer
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
                Args[i] = (new VScalarToInt(Args[i], true))->Resolve(ec);
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
                Args[i] = (new VScalarToFloat(Args[i], true))->Resolve(ec);
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
              Args[i]->Type.CheckMatch(true, Args[i]->Loc, Func->ParamTypes[i]);
            }
            */
            if (!Args[i]->Type.CheckMatch(true, Args[i]->Loc, Func->ParamTypes[i])) {
              ParseError(Args[i]->Loc, "Out parameter types must be equal for arg #%d (want `%s`, but got `%s`)", i+1, *Func->ParamTypes[i].GetName(), *Args[i]->Type.GetName());
            } else {
              /*
              fprintf(stderr, "PTRPTR: %s (%s : %s); (%u:%u) (%d:%d)\n", *Args[i]->Loc.toString(), *Args[i]->Type.GetName(), *Func->ParamTypes[i].GetName(),
                (unsigned)Args[i]->Type.Type, (unsigned)Func->ParamTypes[i].Type,
                Args[i]->Type.PtrLevel, Func->ParamTypes[i].PtrLevel);
              */
            }
          }
          // now check r/o flags
          if ((Args[i]->Flags&FIELD_ReadOnly) != 0 && (Func->ParamFlags[i]&FPARM_Const) == 0) {
            ParseError(Args[i]->Loc, "Cannot pass const argument #%d as non-const", i+1);
          }
          int oldFlags = Args[i]->Flags;
          Args[i]->Flags &= ~FIELD_ReadOnly;
          Args[i]->RequestAddressOf();
          Args[i]->Flags = oldFlags;
        } else {
          // normal args: do int->float conversion
          if (Func->ParamTypes[i].Type == TYPE_Float) {
            if (Args[i]->IsIntConst()) {
              int val = Args[i]->GetIntConst();
              TLocation Loc = Args[i]->Loc;
              delete Args[i];
              Args[i] = (new VFloatLiteral(val, Loc))->Resolve(ec); // literal's `Reslove()` does nothing, but...
            } else if (Args[i]->Type.Type == TYPE_Int) {
              auto erloc = Args[i]->Loc;
              Args[i] = (new VScalarToFloat(Args[i], true))->Resolve(ec);
              if (!Args[i]) ParseError(erloc, "Cannot convert argument to float for arg #%d", i+1);
            }
          }
          if (!Args[i]->Type.CheckMatch(false, Args[i]->Loc, Func->ParamTypes[i], false)) {
            ParseError(Args[i]->Loc, "Invalid argument #%d type '%s' in method '%s'", i+1, *Args[i]->Type.GetName(), *Func->GetFullName());
            //fprintf(stderr, "  <%s>\n", *Args[i]->toString());
          }
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

  if (NumArgs > maxParams) ParseError(Loc, "Incorrect number of arguments to `%s`, need %d, got %d", *Func->GetFullName(), maxParams, NumArgs);

  while (NumArgs < requiredParams) {
    if (Func->ParamFlags[NumArgs]&FPARM_Optional) {
      Args[NumArgs] = nullptr;
      ++NumArgs;
    } else {
      ParseError(Loc, "Incorrect argument count %d to `%s`, should be %d", NumArgs, *Func->GetFullName(), requiredParams);
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
  if (Func->Flags&FUNC_VarArgs) {
    maxParams = VMethod::MAX_PARAMS-1;
  } else {
    maxParams = Func->NumParams;
  }

  if (NumArgs > maxParams) ParseError(Loc, "Incorrect number of arguments to `%s`, need %d, got %d", *Func->GetFullName(), maxParams, NumArgs);

  for (int i = 0; i < NumArgs; ++i) {
    if (i >= requiredParams) continue;
    if (!Args[i]) continue;

    //FIXME: move this to separate method
    // simplify a little:
    //   replace `+number` with `number`
    {
      VExpression *e = Args[i];
      Args[i] = nullptr;
      for (;;) {
        if (e->IsUnaryMath()) {
          VUnary *un = (VUnary *)e;
          if (un->op) {
            if (un->Oper == VUnary::Plus && (un->op->IsIntConst() || un->op->IsFloatConst())) {
              VExpression *etmp = un->op;
              //fprintf(stderr, "SIMPLIFIED! <%s> -> <%s>\n", *un->toString(), *etmp->toString());
              un->op = nullptr;
              delete e;
              e = etmp;
              continue;
            }
          }
        }
        break;
      }
      Args[i] = e;
      if (!e) continue;
    }

    switch (Func->ParamTypes[i].Type) {
      case TYPE_Name:
        if (Args[i]->IsDecorateSingleName()) {
          VDecorateSingleName *E = (VDecorateSingleName *)Args[i];
          Args[i] = new VNameLiteral(*E->Name, E->Loc);
          delete E;
          E = nullptr;
        } else if (Args[i]->IsStrConst()) {
          const char *Val = Args[i]->GetStrConst(ec.Package);
          TLocation ALoc = Args[i]->Loc;
          delete Args[i];
          Args[i] = new VNameLiteral(Val, ALoc);
        } else if (Args[i]->IsIntConst() && Args[i]->GetIntConst() == 0) {
          // "false" or "0" means "empty"
          TLocation ALoc = Args[i]->Loc;
          ParseWarning(ALoc, "DECORATE: `%s` argument #%d should be string; FIX YOUR BROKEN CODE!", Func->GetName(), i+1);
          delete Args[i];
          Args[i] = new VNameLiteral(NAME_None, ALoc);
        }
        break;
      case TYPE_String:
        if (Args[i]->IsDecorateSingleName()) {
          VDecorateSingleName *E = (VDecorateSingleName *)Args[i];
          Args[i] = new VStringLiteral(VStr(*E->Name), ec.Package->FindString(*E->Name), E->Loc);
          delete E;
          E = nullptr;
        } else if (Args[i]->IsIntConst() && Args[i]->GetIntConst() == 0) {
          // "false" or "0" means "empty"
          TLocation ALoc = Args[i]->Loc;
          ParseWarning(ALoc, "DECORATE: `%s` argument #%d should be string; FIX YOUR BROKEN CODE!", Func->GetName(), i+1);
          delete Args[i];
          Args[i] = new VStringLiteral(VStr(), ec.Package->FindString(""), ALoc);
        }
        break;
      case TYPE_Class:
        if (Args[i]->IsDecorateSingleName()) {
          VDecorateSingleName *E = (VDecorateSingleName *)Args[i];
          Args[i] = new VStringLiteral(VStr(*E->Name), ec.Package->FindString(*E->Name), E->Loc);
          delete E;
          E = nullptr;
        }
        if (Args[i]->IsStrConst()) {
          const char *CName = Args[i]->GetStrConst(ec.Package);
          TLocation ALoc = Args[i]->Loc;
          VClass *Cls = VClass::FindClassNoCase(CName);
          if (!Cls) {
            ParseWarning(ALoc, "No such class `%s`", CName);
            delete Args[i];
            Args[i] = nullptr;
            Args[i] = new VNoneLiteral(ALoc);
          } else if (Func->ParamTypes[i].Class && !Cls->IsChildOf(Func->ParamTypes[i].Class)) {
            ParseWarning(ALoc, "Class `%s` is not a descendant of `%s`", CName, Func->ParamTypes[i].Class->GetName());
            delete Args[i];
            Args[i] = nullptr;
            Args[i] = new VNoneLiteral(ALoc);
          } else {
            delete Args[i];
            Args[i] = nullptr;
            Args[i] = new VClassConstant(Cls, ALoc);
          }
        } else if (Args[i]->IsIntConst() && Args[i]->GetIntConst() == 0) {
          // "false" or "0" means "empty"
          TLocation ALoc = Args[i]->Loc;
          ParseWarning(ALoc, "DECORATE: `%s` argument #%d should be class; FIX YOUR BROKEN CODE!", Func->GetName(), i+1);
          delete Args[i];
          Args[i] = new VNoneLiteral(ALoc);
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
          VStr Lbl = VStr(Args[i]->GetStrConst(ec.Package));
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
//  VInvocation::GetVMethod
//
//==========================================================================
VMethod *VInvocation::GetVMethod (VEmitContext &ec) {
  return nullptr;
}


//==========================================================================
//
//  VInvocation::IsMethodNameChangeable
//
//==========================================================================
bool VInvocation::IsMethodNameChangeable () const {
  return false;
}


//==========================================================================
//
//  VInvocation::GetMethodName
//
//==========================================================================
VName VInvocation::GetMethodName () const {
  return (Func ? Func->Name : NAME_None);
}


//==========================================================================
//
//  VInvocation::SetMethodName
//
//==========================================================================
void VInvocation::SetMethodName (VName aname) {
  FatalError("VC: Internal compiler error: `VInvocation::SetMethodName()` called");
}


//==========================================================================
//
//  VInvocation::toString
//
//==========================================================================
VStr VInvocation::toString () const {
  VStr res;
  if (HaveSelf) {
    res += "(";
    res += e2s(SelfExpr);
    res += ")";
  }
  if (BaseCall) res += "::";
  if (Func) res += "("+Func->GetFullName()+")"; else res += e2s(nullptr);
  //VField *DelegateField;
  //int DelegateLocal;
  //VState *CallerState;
  //bool MultiFrameState;
  res += args2str();
  return res;
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
//  VInvokeWrite::DoSyntaxCopyTo
//
//==========================================================================
void VInvokeWrite::DoSyntaxCopyTo (VExpression *e) {
  VInvocationBase::DoSyntaxCopyTo(e);
  auto res = (VInvokeWrite *)e;
  res->isWriteln = isWriteln;
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
          case TYPE_SliceArray:
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
    //ec.EmitPushNumber(Args[i]->Type.Type, Loc);
    ec.AddStatement(OPC_DoWriteOne, Args[i]->Type, Loc);
  }
  if (isWriteln) ec.AddStatement(OPC_DoWriteFlush, Loc);
  unguard;
}


//==========================================================================
//
//  VInvokeWrite::GetVMethod
//
//==========================================================================
VMethod *VInvokeWrite::GetVMethod (VEmitContext &ec) {
  return nullptr;
}


//==========================================================================
//
//  VInvokeWrite::IsMethodNameChangeable
//
//==========================================================================
bool VInvokeWrite::IsMethodNameChangeable () const {
  return false;
}


//==========================================================================
//
//  VInvokeWrite::GetMethodName
//
//==========================================================================
VName VInvokeWrite::GetMethodName () const {
  return VName(isWriteln ? "writeln" : "write");
}


//==========================================================================
//
//  VInvokeWrite::SetMethodName
//
//==========================================================================
void VInvokeWrite::SetMethodName (VName aname) {
  FatalError("VC: Internal compiler error: `VInvokeWrite::SetMethodName()` called");
}


//==========================================================================
//
//  VInvokeWrite::toString
//
//==========================================================================
VStr VInvokeWrite::toString () const {
  VStr res("write");
  if (isWriteln) res += "ln";
  res += args2str();
  return res;
}
