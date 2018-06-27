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
//  VCastExpressionBase
//
//==========================================================================
class VCastExpressionBase : public VExpression {
public:
  VExpression *op;

  VCastExpressionBase (VExpression *AOp);
  VCastExpressionBase (const TLocation &ALoc);
  virtual ~VCastExpressionBase () override;
  virtual VExpression *DoResolve (VEmitContext &) override;

protected:
  VCastExpressionBase () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDelegateToBool
//
//==========================================================================
class VDelegateToBool : public VCastExpressionBase {
public:
  VDelegateToBool (VExpression *AOp);
  virtual VExpression *SyntaxCopy () override;
  virtual void Emit (VEmitContext &) override;

protected:
  VDelegateToBool () {}
};


//==========================================================================
//
//  VStringToBool
//
//==========================================================================
class VStringToBool : public VCastExpressionBase {
public:
  VStringToBool (VExpression *AOp);
  virtual VExpression *SyntaxCopy () override;
  virtual void Emit (VEmitContext &) override;

protected:
  VStringToBool () {}
};


//==========================================================================
//
//  VPointerToBool
//
//==========================================================================
class VPointerToBool : public VCastExpressionBase {
public:
  VPointerToBool (VExpression *AOp);
  virtual VExpression *SyntaxCopy () override;
  virtual void Emit (VEmitContext &) override;

protected:
  VPointerToBool () {}
};


//==========================================================================
//
//  VScalarToFloat
//
//==========================================================================
class VScalarToFloat : public VCastExpressionBase {
public:
  VScalarToFloat (VExpression *AOp);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VScalarToFloat () {}
};


//==========================================================================
//
//  VScalarToInt
//
//==========================================================================
class VScalarToInt : public VCastExpressionBase {
public:
  VScalarToInt (VExpression *AOp);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VScalarToInt () {}
};


//==========================================================================
//
//  VCastToString
//
//==========================================================================
class VCastToString : public VCastExpressionBase {
public:
  VCastToString (VExpression *AOp);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VCastToString () {}
};


//==========================================================================
//
//  VCastToName
//
//==========================================================================
class VCastToName : public VCastExpressionBase {
public:
  VCastToName (VExpression *AOp);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VCastToName () {}
};


//==========================================================================
//
//  VDynamicCast
//
//==========================================================================
class VDynamicCast : public VCastExpressionBase {
public:
  VClass *Class;

  VDynamicCast (VClass *AClass, VExpression *AOp, const TLocation &ALoc);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VDynamicCast () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDynamicClassCast
//
//==========================================================================
class VDynamicClassCast : public VCastExpressionBase {
public:
  VName ClassName;

  VDynamicClassCast (VName, VExpression *, const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VDynamicClassCast () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VStructPtrCast
//
//==========================================================================
class VStructPtrCast : public VCastExpressionBase {
public:
  VExpression *dest;

  VStructPtrCast (VExpression *aop, VExpression *adest, const TLocation &aloc);
  virtual ~VStructPtrCast () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VStructPtrCast () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};
