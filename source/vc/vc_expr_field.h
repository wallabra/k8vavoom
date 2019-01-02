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
//  VFieldBase
//
//==========================================================================
class VFieldBase : public VExpression {
public:
  VExpression *op;
  VName FieldName;

public:
  VFieldBase (VExpression *AOp, VName AFieldName, const TLocation &ALoc);
  virtual ~VFieldBase () override;

protected:
  VFieldBase () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VPointerField
//
//==========================================================================
class VPointerField : public VFieldBase {
public:
  VPointerField (VExpression *AOp, VName AFieldName, const TLocation &ALoc);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

protected:
  VPointerField () {}

  // return result of this unconditionally
  // this uses or deletes `opcopy`
  VExpression *TryUFCS (VEmitContext &ec, VExpression *opcopy, const char *errdatatype, VMemberBase *mb);
};


//==========================================================================
//
//  VDotField
//
//==========================================================================
class VDotField : public VFieldBase {
private:
  enum AssType { Normal, AssTarget, AssValue };

  int builtin; // >0: generate builtin

private:
  // `Prop` must not be null!
  VExpression *DoPropertyResolve (VEmitContext &ec, VProperty *Prop, AssType assType);

public:
  VDotField (VExpression *AOp, VName AFieldName, const TLocation &ALoc);

  VExpression *InternalResolve (VEmitContext &ec, AssType assType);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual VExpression *ResolveAssignmentTarget (VEmitContext &) override;
  virtual VExpression *ResolveAssignmentValue (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsDotField () const override;

  virtual VStr toString () const override;

protected:
  VDotField () {}
};


//==========================================================================
//
//  VFieldAccess
//
//==========================================================================
class VFieldAccess : public VExpression {
public:
  VExpression *op;
  VField *field;
  bool AddressRequested;

  VFieldAccess (VExpression *AOp, VField *AField, const TLocation &ALoc, int ExtraFlags);
  virtual ~VFieldAccess () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void RequestAddressOf () override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

protected:
  VFieldAccess () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDelegateVal
//
//==========================================================================
class VDelegateVal : public VExpression {
public:
  VExpression *op;
  VMethod *M;

  VDelegateVal (VExpression *AOp, VMethod *AM, const TLocation &ALoc);
  virtual ~VDelegateVal () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

protected:
  VDelegateVal () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};
