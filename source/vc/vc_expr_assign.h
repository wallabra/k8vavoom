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

// ////////////////////////////////////////////////////////////////////////// //
class VAssignment : public VExpression {
public:
  enum EAssignOper {
    Assign,
    AddAssign,
    MinusAssign,
    MultiplyAssign,
    DivideAssign,
    ModAssign,
    AndAssign,
    OrAssign,
    XOrAssign,
    LShiftAssign,
    RShiftAssign,
    URShiftAssign,
    CatAssign,
  };

public:
  EAssignOper Oper;
  VExpression *op1;
  VExpression *op2;

public:
  VAssignment (EAssignOper, VExpression*, VExpression*, const TLocation&);
  virtual ~VAssignment () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsAssignExpr () const override;

  virtual VStr toString () const override;

protected:
  VAssignment () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VPropertyAssign : public VInvocation {
public:
  VPropertyAssign (VExpression *ASelfExpr, VMethod *AFunc, bool AHaveSelf, const TLocation &ALoc);
  virtual VExpression *SyntaxCopy () override;
  virtual bool IsPropertyAssign () const override;

protected:
  VPropertyAssign () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};
