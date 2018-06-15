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
//  VIntLiteral
//
//==========================================================================
class VIntLiteral : public VExpression {
public:
  vint32 Value;

  VIntLiteral (vint32, const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsIntConst () const override;
  virtual vint32 GetIntConst () const override;

protected:
  VIntLiteral () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VFloatLiteral
//
//==========================================================================
class VFloatLiteral : public VExpression {
public:
  float Value;

  VFloatLiteral (float, const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsFloatConst () const override;
  virtual float GetFloatConst () const override;

protected:
  VFloatLiteral () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VNameLiteral
//
//==========================================================================
class VNameLiteral : public VExpression {
public:
  VName Value;

  VNameLiteral (VName, const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsNameConst () const override;
  virtual VName GetNameConst () const;

protected:
  VNameLiteral () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VStringLiteral
//
//==========================================================================
class VStringLiteral : public VExpression {
public:
  vint32 Value;

  VStringLiteral (vint32, const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsStrConst () const override;
  virtual VStr GetStrConst (VPackage *) const override;

protected:
  VStringLiteral () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VSelf
//
//==========================================================================
class VSelf : public VExpression {
public:
  VSelf (const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VSelf () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VSelfClass
//
//==========================================================================
class VSelfClass : public VExpression {
public:
  VSelfClass (const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VSelfClass () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VNoneLiteral
//
//==========================================================================
class VNoneLiteral : public VExpression {
public:
  VNoneLiteral (const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VNoneLiteral () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VNullLiteral
//
//==========================================================================
class VNullLiteral : public VExpression {
public:
  VNullLiteral (const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VNullLiteral () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDollar
//
//==========================================================================
class VDollar : public VExpression {
public:
  VDollar (const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VDollar () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};
