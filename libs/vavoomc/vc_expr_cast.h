//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**  You should have received a copy of the GNU General Public License
//**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
  bool opResolved;

  VCastExpressionBase (VExpression *AOp, bool aOpResolved);
  VCastExpressionBase (const TLocation &ALoc, bool aOpResolved);
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
  VDelegateToBool (VExpression *AOp, bool aOpResolved);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

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
  VStringToBool (VExpression *AOp, bool aOpResolved);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

protected:
  VStringToBool () {}
};


//==========================================================================
//
//  VNameToBool
//
//==========================================================================
class VNameToBool : public VCastExpressionBase {
public:
  VNameToBool (VExpression *AOp, bool aOpResolved);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

protected:
  VNameToBool () {}
};


//==========================================================================
//
//  VFloatToBool
//
//==========================================================================
class VFloatToBool : public VCastExpressionBase {
public:
  VFloatToBool (VExpression *AOp, bool aOpResolved);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

protected:
  VFloatToBool () {}
};


//==========================================================================
//
//  VVectorToBool
//
//==========================================================================
class VVectorToBool : public VCastExpressionBase {
public:
  VVectorToBool (VExpression *AOp, bool aOpResolved);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

protected:
  VVectorToBool () {}
};


//==========================================================================
//
//  VPointerToBool
//
//==========================================================================
class VPointerToBool : public VCastExpressionBase {
public:
  VPointerToBool (VExpression *AOp, bool aOpResolved);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

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
  VScalarToFloat (VExpression *AOp, bool aOpResolved);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

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
  VScalarToInt (VExpression *AOp, bool aOpResolved);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

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
  VCastToString (VExpression *AOp, bool aOpResolved);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

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
  VCastToName (VExpression *AOp, bool aOpResolved);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

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

  virtual VStr toString () const override;

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

  virtual VStr toString () const override;

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

  virtual VStr toString () const override;

protected:
  VStructPtrCast () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};
