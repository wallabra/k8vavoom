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
//  VVector
//
//==========================================================================
class VVector : public VExpression {
public:
  VExpression *op1;
  VExpression *op2;
  VExpression *op3;

  VVector (VExpression *, VExpression *, VExpression *, const TLocation &);
  virtual ~VVector () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VVector () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VSingleName
//
//==========================================================================
class VSingleName : public VExpression {
private:
  enum AssType { Normal, AssTarget, AssValue };

public:
  VName Name;

  VSingleName (VName, const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  VExpression *InternalResolve (VEmitContext &ec, AssType assType);
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual VExpression *ResolveAssignmentTarget (VEmitContext &) override;
  virtual VExpression *ResolveAssignmentValue (VEmitContext &) override;
  virtual VTypeExpr *ResolveAsType (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsValidTypeExpression () const override;
  virtual bool IsSingleName () const override;

protected:
  VSingleName () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDoubleName
//
//==========================================================================
class VDoubleName : public VExpression {
public:
  VName Name1;
  VName Name2;

  VDoubleName (VName, VName, const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual VTypeExpr *ResolveAsType (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsValidTypeExpression () const override;

protected:
  VDoubleName () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDefaultObject
//
//==========================================================================
class VDefaultObject : public VExpression {
public:
  VExpression *op;

  VDefaultObject (VExpression *, const TLocation &);
  virtual ~VDefaultObject () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &);
  virtual void Emit (VEmitContext &) override;
  virtual bool IsDefaultObject () const override;

protected:
  VDefaultObject () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VPushPointed
//
//==========================================================================
class VPushPointed : public VExpression {
public:
  VExpression *op;
  bool AddressRequested;

  VPushPointed (VExpression *);
  virtual ~VPushPointed () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void RequestAddressOf () override;
  virtual void Emit (VEmitContext &) override;

protected:
  VPushPointed () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VConditional
//
//==========================================================================
class VConditional : public VExpression {
public:
  VExpression *op;
  VExpression *op1;
  VExpression *op2;

  VConditional (VExpression *, VExpression *, VExpression *, const TLocation &);
  virtual ~VConditional () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VConditional () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDropResult
//
//==========================================================================
class VDropResult : public VExpression {
public:
  VExpression *op;

  VDropResult (VExpression *);
  virtual ~VDropResult () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VDropResult () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VClassConstant
//
//==========================================================================
class VClassConstant : public VExpression {
public:
  VClass *Class;

  VClassConstant (VClass *AClass, const TLocation &ALoc);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VClassConstant () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VStateConstant
//
//==========================================================================
class VStateConstant : public VExpression {
public:
  VState *State;

  VStateConstant (VState *AState, const TLocation &ALoc);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VStateConstant () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VConstantValue
//
//==========================================================================
class VConstantValue : public VExpression {
public:
  VConstant *Const;

  VConstantValue (VConstant *AConst, const TLocation &ALoc);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsIntConst () const override;
  virtual bool IsFloatConst () const override;
  virtual vint32 GetIntConst () const override;
  virtual float GetFloatConst () const override;

protected:
  VConstantValue () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDecorateSingleName
//
//==========================================================================
class VDecorateSingleName : public VExpression {
public:
  VStr Name;

  VDecorateSingleName (const VStr &, const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsDecorateSingleName () const override;

protected:
  VDecorateSingleName ();
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};
