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


//==========================================================================
//
//  VInvocationBase
//
//==========================================================================
class VInvocationBase : public VExpression {
public:
  int NumArgs;
  VExpression *Args[VMethod::MAX_PARAMS+1];

  VInvocationBase (int ANumArgs, VExpression **AArgs, const TLocation& ALoc);
  virtual ~VInvocationBase () override;

protected:
  VInvocationBase () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VBaseInvocation
//
//==========================================================================
class VBaseInvocation : public VInvocationBase {
public:
  VName Name;

  VBaseInvocation(VName, int, VExpression**, const TLocation&);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VBaseInvocation () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VCastOrInvocation
//
//==========================================================================
class VCastOrInvocation : public VInvocationBase {
public:
  VName Name;
  bool FirstOpIsResolved;

  VCastOrInvocation (VName, const TLocation&, int, VExpression**);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual VExpression *ResolveIterator (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VCastOrInvocation () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDotInvocation
//
//==========================================================================
class VDotInvocation : public VInvocationBase {
public:
  VExpression *SelfExpr;
  VName MethodName;

  VDotInvocation (VExpression *, VName, const TLocation &, int, VExpression **);
  virtual ~VDotInvocation () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual VExpression *ResolveIterator (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VDotInvocation () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VInvocation
//
//==========================================================================
class VInvocation : public VInvocationBase {
public:
  VExpression *SelfExpr;
  VMethod *Func;
  VField *DelegateField;
  bool HaveSelf;
  bool BaseCall;
  VState *CallerState;
  bool MultiFrameState;
  bool FirstOpIsResolved;

  VInvocation (VExpression *ASelfExpr, VMethod *AFunc, VField *ADelegateField,
               bool AHaveSelf, bool ABaseCall, const TLocation &ALoc, int ANumArgs,
               VExpression **AArgs);
  virtual ~VInvocation () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  void CheckParams (VEmitContext &);
  void CheckDecorateParams (VEmitContext &);

protected:
  VInvocation () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};
