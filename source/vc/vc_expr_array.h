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
  AutoCopy opscopy;
  bool genStringAssign;
  VExpression *sval;
  bool resolvingInd2; // for opDollar

public:
  VExpression *op;
  VExpression *ind;
  VExpression *ind2; // for 2d access; null for 1d
  bool AddressRequested;
  bool IsAssign;
  bool skipBoundsChecking; // in range foreach, we can skip this

  VArrayElement (VExpression *AOp, VExpression *AInd, const TLocation &ALoc, bool aSkipBounds=false);
  VArrayElement (VExpression *AOp, VExpression *AInd, VExpression *AInd2, const TLocation &ALoc, bool aSkipBounds=false);
  virtual ~VArrayElement () override;
  VExpression *InternalResolve (VEmitContext &ec, bool assTarget);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual VExpression *ResolveAssignmentTarget (VEmitContext &) override;
  virtual VExpression *ResolveCompleteAssign (VEmitContext &ec, VExpression *val, bool &resolved) override;
  virtual void RequestAddressOf () override;
  virtual void Emit (VEmitContext &) override;

  inline bool Is2D () const { return (ind2 != nullptr); }

  VExpression *GetOpSyntaxCopy ();

protected:
  VArrayElement () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;

  friend class VDollar; // to access opcopy
};


//==========================================================================
//
//  VSliceOp
//
//==========================================================================
class VSliceOp : public VArrayElement {
public:
  VExpression *hi;

  VSliceOp (VExpression *aop, VExpression *alo, VExpression *ahi, const TLocation &aloc);
  virtual ~VSliceOp () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &ec) override;
  virtual VExpression *ResolveAssignmentTarget (VEmitContext &ec) override;
  virtual VExpression *ResolveCompleteAssign (VEmitContext &ec, VExpression *val, bool &resolved) override;
  virtual void RequestAddressOf () override;
  virtual void Emit (VEmitContext &ec) override;

protected:
  VSliceOp () {}
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
  int dimNumber; // 0: total size

  VDynArrayGetNum (VExpression *AArrayExpr, int aDimNumber, const TLocation &ALoc);
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
  VExpression *NumExpr2;
  int opsign; // <0: -=; >0: +=; 0: =; fixed in assign expression resolving
  bool asSetSize;

  VDynArraySetNum (VExpression *AArrayExpr, VExpression *ANumExpr, VExpression *ANumExpr2, const TLocation &ALoc);
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
//  VDynArrayClear
//
//==========================================================================
class VDynArrayClear : public VExpression {
public:
  VExpression *ArrayExpr;

  VDynArrayClear (VExpression *, const TLocation &);
  virtual ~VDynArrayClear () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VDynArrayClear () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDynArraySort
//
//==========================================================================
class VDynArraySort : public VExpression {
public:
  VExpression *ArrayExpr;
  VExpression *DgExpr;

  VDynArraySort (VExpression *, VExpression *, const TLocation &);
  virtual ~VDynArraySort () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VDynArraySort () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;

private:
  bool checkDelegateType (VMethod *dg);
};


//==========================================================================
//
//  VStringGetLength
//
//==========================================================================
class VStringGetLength : public VExpression {
public:
  VExpression *StrExpr;

  // `AStrExpr` should be already resolved
  VStringGetLength (VExpression *AStrExpr, const TLocation &ALoc);
  virtual ~VStringGetLength () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VStringGetLength () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VSliceGetLength
//
//==========================================================================
class VSliceGetLength : public VExpression {
public:
  VExpression *sexpr;

  // `asexpr` should be already resolved
  VSliceGetLength (VExpression *asexpr, const TLocation &aloc);
  virtual ~VSliceGetLength () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VSliceGetLength () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VSliceGetPtr
//
//==========================================================================
class VSliceGetPtr : public VExpression {
public:
  VExpression *sexpr;

  // `asexpr` should be already resolved
  VSliceGetPtr (VExpression *asexpr, const TLocation &aloc);
  virtual ~VSliceGetPtr () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VSliceGetPtr () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};
