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
//  VArrayElement
//
//==========================================================================
class VArrayElement : public VExpression {
protected:
  VExpression *opcopy; // valid only in `DoResolve()`

public:
  VExpression *op;
  VExpression *ind;
  bool AddressRequested;
  bool IsAssign;

  VArrayElement (VExpression *AOp, VExpression *AInd, const TLocation &ALoc);
  virtual ~VArrayElement () override;
  VExpression *InternalResolve (VEmitContext &ec, bool assTarget);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual VExpression *ResolveAssignmentTarget (VEmitContext &) override;
  virtual void RequestAddressOf () override;
  virtual void Emit (VEmitContext &) override;

protected:
  VArrayElement () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;

  friend class VDollar; // to access opcopy
};


//==========================================================================
//
//  VStringSlice
//
//==========================================================================
class VStringSlice : public VArrayElement {
public:
  VExpression *hi;

  VStringSlice (VExpression *aop, VExpression *alo, VExpression *ahi, const TLocation &aloc);
  virtual ~VStringSlice () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &ec) override;
  virtual VExpression *ResolveAssignmentTarget (VEmitContext &ec) override;
  virtual void RequestAddressOf () override;
  virtual void Emit (VEmitContext &ec) override;

protected:
  VStringSlice () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDynArrayGetNum
//
//==========================================================================
class VDynArrayGetNum : public VExpression {
public:
  VExpression *ArrayExpr;

  VDynArrayGetNum (VExpression *AArrayExpr, const TLocation &ALoc);
  virtual ~VDynArrayGetNum () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VDynArrayGetNum () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDynArraySetNum
//
//==========================================================================
class VDynArraySetNum : public VExpression {
public:
  VExpression *ArrayExpr;
  VExpression *NumExpr;
  int opsign; // <0: -=; >0: +=; 0: =; fixed in assign expression resolving

  VDynArraySetNum (VExpression *, VExpression *, const TLocation &);
  virtual ~VDynArraySetNum () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsDynArraySetNum () const override;

protected:
  VDynArraySetNum () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDynArrayInsert
//
//==========================================================================
class VDynArrayInsert : public VExpression {
public:
  VExpression *ArrayExpr;
  VExpression *IndexExpr;
  VExpression *CountExpr;

  VDynArrayInsert (VExpression *, VExpression *, VExpression *, const TLocation &);
  virtual ~VDynArrayInsert () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VDynArrayInsert () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDynArrayRemove
//
//==========================================================================
class VDynArrayRemove : public VExpression {
public:
  VExpression *ArrayExpr;
  VExpression *IndexExpr;
  VExpression *CountExpr;

  VDynArrayRemove (VExpression *, VExpression *, VExpression *, const TLocation &);
  virtual ~VDynArrayRemove () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VDynArrayRemove () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VStringGetLength
//
//==========================================================================
class VStringGetLength : public VExpression {
public:
  VExpression *StrExpr;

  VStringGetLength (VExpression *, const TLocation&);
  virtual ~VStringGetLength () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext&) override;
  virtual void Emit (VEmitContext&) override;

protected:
  VStringGetLength () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};
