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
//  VArgMarshall
//
//==========================================================================
class VArgMarshall : public VExpression {
public:
  VExpression *e;
  bool isRef;
  bool isOut;
  bool marshallOpt;

public:
  VArgMarshall (VExpression *ae);
  virtual ~VArgMarshall () override;
  virtual VExpression *DoResolve (VEmitContext &ec) override;
  virtual void Emit (VEmitContext &ec) override;

  virtual VExpression *SyntaxCopy () override;

  virtual bool IsMarshallArg () const override;
  virtual bool IsRefArg () const override;
  virtual bool IsOutArg () const override;
  virtual bool IsOptMarshallArg () const override;

  virtual VStr toString () const override;

protected:
  VArgMarshall () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VInvocationBase
//
//==========================================================================
class VInvocationBase : public VExpression {
public:
  int NumArgs;
  VExpression *Args[VMethod::MAX_PARAMS+1];

  VInvocationBase (int ANumArgs, VExpression **AArgs, const TLocation &ALoc);
  virtual ~VInvocationBase () override;

  virtual bool IsAnyInvocation () const override;

  virtual VMethod *GetVMethod (VEmitContext &ec) = 0;

  virtual bool IsMethodNameChangeable () const = 0;
  virtual VName GetMethodName () const = 0;
  virtual void SetMethodName (VName name) = 0;

protected:
  VInvocationBase () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;

  // generates "(" and ")" too
  VStr args2str () const;
};


//==========================================================================
//
//  VSuperInvocation
//
//==========================================================================
class VSuperInvocation : public VInvocationBase {
public:
  VName Name;

  VSuperInvocation (VName, int, VExpression **, const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VMethod *GetVMethod (VEmitContext &ec) override;

  virtual bool IsMethodNameChangeable () const override;
  virtual VName GetMethodName () const override;
  virtual void SetMethodName (VName aname) override;

  virtual VStr toString () const override;

protected:
  VSuperInvocation () {}
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

  VCastOrInvocation (VName, const TLocation&, int, VExpression**);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual VExpression *ResolveIterator (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VMethod *GetVMethod (VEmitContext &ec) override;

  virtual bool IsMethodNameChangeable () const override;
  virtual VName GetMethodName () const override;
  virtual void SetMethodName (VName aname) override;

  virtual VStr toString () const override;

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

  virtual VMethod *GetVMethod (VEmitContext &ec) override;

  virtual bool IsMethodNameChangeable () const override;
  virtual VName GetMethodName () const override;
  virtual void SetMethodName (VName aname) override;

  virtual VStr toString () const override;

protected:
  VDotInvocation () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VTypeInvocation
//
//  This does properties for typeexprs, like `int.max`
//
//==========================================================================
class VTypeInvocation : public VInvocationBase {
public:
  VExpression *TypeExpr;
  VName MethodName;

  VTypeInvocation (VExpression *aTypeExpr, VName aMethodName, const TLocation &aloc, int argc, VExpression **argv);
  virtual ~VTypeInvocation () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VMethod *GetVMethod (VEmitContext &ec) override;

  virtual bool IsMethodNameChangeable () const override;
  virtual VName GetMethodName () const override;
  virtual void SetMethodName (VName aname) override;

  virtual VStr toString () const override;

protected:
  VTypeInvocation () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VInvocation
//
//==========================================================================
class VInvocation : public VInvocationBase {
private:
  int lcidx[VMethod::MAX_PARAMS];
  bool reused[VMethod::MAX_PARAMS];
  bool optmarshall[VMethod::MAX_PARAMS];

public:
  VExpression *SelfExpr;
  VMethod *Func;
  VField *DelegateField;
  int DelegateLocal;
  bool HaveSelf;
  bool BaseCall;
  VState *CallerState;
  bool MultiFrameState;

  VInvocation (VExpression *ASelfExpr, VMethod *AFunc, VField *ADelegateField,
               bool AHaveSelf, bool ABaseCall, const TLocation &ALoc, int ANumArgs,
               VExpression **AArgs);
  VInvocation (VMethod *AFunc, int ADelegateLocal, const TLocation &ALoc, int ANumArgs, VExpression **AArgs);
  virtual ~VInvocation () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VMethod *GetVMethod (VEmitContext &ec) override;

  virtual bool IsLLInvocation () const override;

  void CheckParams (VEmitContext &ec);
  void CheckDecorateParams (VEmitContext &);

  // arguments should be already resolved (i.e. has type)
  static VMethod *FindMethodWithSignature (VEmitContext &ec, VName name, int argc, VExpression **argv);
  // arguments should be already resolved (i.e. has type)
  static bool IsGoodMethodParams (VEmitContext &ec, VMethod *m, int argc, VExpression **argv);

  virtual bool IsMethodNameChangeable () const override;
  virtual VName GetMethodName () const override;
  virtual void SetMethodName (VName aname) override;

  virtual VStr toString () const override;

protected:
  VInvocation () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;

  // should be called only for functions with `builtinOpc >= 0`
  VExpression *OptimizeBuiltin (VEmitContext &ec);

  // used by `OptimizeBuiltin`; `types` are `TYPE_xxx`
  bool CheckSimpleConstArgs (int argc, const int *types) const;
};


//==========================================================================
//
//  VInvokeWrite
//
//==========================================================================
class VInvokeWrite : public VInvocationBase {
public:
  bool isWriteln;

  VInvokeWrite (bool aIsWriteln, const TLocation &ALoc, int ANumArgs, VExpression **AArgs);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VMethod *GetVMethod (VEmitContext &ec) override;

  virtual bool IsMethodNameChangeable () const override;
  virtual VName GetMethodName () const override;
  virtual void SetMethodName (VName aname) override;

  virtual VStr toString () const override;

protected:
  VInvokeWrite () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};
