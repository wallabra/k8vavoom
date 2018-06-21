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
//  VLocalEntry
//
//==========================================================================
class VLocalEntry {
public:
  VExpression *TypeExpr;
  VName Name;
  TLocation Loc;
  VExpression *Value;
  VExpression *TypeOfExpr; // for automatic vars w/o initializer, resolve this, and use its type
  bool isRef; // for range foreach

  VLocalEntry () : TypeExpr(nullptr), Name(NAME_None), Value(nullptr), TypeOfExpr(nullptr), isRef(false) {}
};


//==========================================================================
//
//  VLocalDecl
//
//==========================================================================
class VLocalDecl : public VExpression {
public:
  TArray<VLocalEntry> Vars;

  VLocalDecl (const TLocation &);
  virtual ~VLocalDecl () override;

  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  void Declare (VEmitContext &);
  void EmitInitialisations (VEmitContext &);
  virtual bool IsLocalVarDecl () const override;

protected:
  VLocalDecl () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VLocalVar
//
//==========================================================================
class VLocalVar : public VExpression {
public:
  int num;
  bool AddressRequested;
  bool PushOutParam;

  VLocalVar (int ANum, const TLocation &ALoc);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void RequestAddressOf () override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsLocalVarExpr () const override;

protected:
  VLocalVar () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};
