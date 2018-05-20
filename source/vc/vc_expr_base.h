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

class VTypeExpr;


// ////////////////////////////////////////////////////////////////////////// //
class VExpression {
public:
  VFieldType Type;
  VFieldType RealType;
  int Flags;
  TLocation Loc;

  VExpression (const TLocation &ALoc);
  virtual ~VExpression () noexcept(false);
  virtual VExpression *DoResolve (VEmitContext &ec) = 0;
  VExpression *Resolve (VEmitContext &ec);
  VExpression *ResolveBoolean (VEmitContext &ec); // actually, *to* boolean
  VExpression *ResolveFloat (VEmitContext &ec); // actually, *to* float
  VExpression *CoerceToFloat (); // expression MUST be already resolved
  virtual VTypeExpr *ResolveAsType (VEmitContext &ec);
  virtual VExpression *ResolveAssignmentTarget (VEmitContext &ec);
  virtual VExpression *ResolveIterator (VEmitContext &ec);
  virtual void RequestAddressOf ();
  virtual void Emit (VEmitContext &ec) = 0;
  virtual void EmitBranchable (VEmitContext &ec, VLabel Lbl, bool OnTrue);
  void EmitPushPointedCode (VFieldType type, VEmitContext &ec);
  virtual bool IsValidTypeExpression ();
  virtual bool IsIntConst () const;
  virtual bool IsFloatConst () const;
  virtual bool IsStrConst () const;
  virtual vint32 GetIntConst () const;
  virtual float GetFloatConst () const;
  virtual VStr GetStrConst (VPackage *) const;
  virtual bool IsDefaultObject () const;
  virtual bool IsPropertyAssign () const;
  virtual bool IsDynArraySetNum () const;
  virtual VExpression *CreateTypeExprCopy ();
  virtual bool AddDropResult ();
  virtual bool IsDecorateSingleName () const;
  virtual bool IsLocalVarDecl () const;
  virtual bool IsLocalVarExpr () const;
  virtual bool IsAssignExpr () const;
};
