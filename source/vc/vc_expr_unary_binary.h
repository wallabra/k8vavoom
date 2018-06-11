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
//  VUnary
//
//==========================================================================
class VUnary : public VExpression {
public:
  enum EUnaryOp {
    Plus,
    Minus,
    Not,
    BitInvert,
    TakeAddress,
  };

public:
  EUnaryOp Oper;
  VExpression *op;

  VUnary (EUnaryOp, VExpression *, const TLocation &);
  virtual ~VUnary () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual void EmitBranchable (VEmitContext &, VLabel, bool) override;

protected:
  VUnary () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VUnaryMutator
//
//==========================================================================
class VUnaryMutator : public VExpression {
public:
  enum EIncDec {
    PreInc,
    PreDec,
    PostInc,
    PostDec,
    Inc,
    Dec,
  };

public:
  EIncDec Oper;
  VExpression *op;

  VUnaryMutator (EIncDec, VExpression *, const TLocation &);
  virtual ~VUnaryMutator () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool AddDropResult () override;

protected:
  VUnaryMutator () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VBinary
//
//==========================================================================
class VBinary : public VExpression {
public:
  enum EBinOp {
    Add,
    Subtract,
    Multiply,
    Divide,
    Modulus,
    LShift,
    RShift,
    And,
    XOr,
    Or,
    Equals,
    NotEquals,
    Less,
    LessEquals,
    Greater,
    GreaterEquals,
    StrCat,
  };

public:
  EBinOp Oper;
  VExpression *op1;
  VExpression *op2;

  VBinary (EBinOp, VExpression *, VExpression *, const TLocation &);
  virtual ~VBinary () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsBinaryMath () const override;

protected:
  VBinary () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VBinaryLogical
//
//==========================================================================
class VBinaryLogical : public VExpression {
public:
  enum ELogOp {
    And,
    Or,
  };

public:
  ELogOp Oper;
  VExpression *op1;
  VExpression *op2;

  VBinaryLogical (ELogOp, VExpression *, VExpression *, const TLocation &);
  virtual ~VBinaryLogical () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual void EmitBranchable (VEmitContext &, VLabel, bool) override;

protected:
  VBinaryLogical () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};
