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

// ////////////////////////////////////////////////////////////////////////// //
class VStatement {
public:
  TLocation Loc;

  VStatement (const TLocation &);
  virtual ~VStatement() noexcept(false);
  virtual bool Resolve (VEmitContext &) = 0;
  virtual void DoEmit (VEmitContext &) = 0;
  void Emit (VEmitContext &);
  virtual bool IsBreak ();
  virtual bool IsContinue ();
  virtual bool IsReturn ();
  virtual bool IsSwitchCase ();
  virtual bool IsSwitchDefault ();
  virtual bool IsEndsWithReturn ();
};


// ////////////////////////////////////////////////////////////////////////// //
class VEmptyStatement : public VStatement {
public:
  VEmptyStatement (const TLocation &);
  virtual bool Resolve (VEmitContext&) override;
  virtual void DoEmit (VEmitContext&) override;
  virtual bool IsEndsWithReturn () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VIf : public VStatement {
public:
  VExpression *Expr;
  VStatement *TrueStatement;
  VStatement *FalseStatement;

  VIf (VExpression *AExpr, VStatement *ATrueStatement, const TLocation &ALoc);
  VIf (VExpression *AExpr, VStatement *ATrueStatement, VStatement *AFalseStatement, const TLocation &ALoc);
  virtual ~VIf () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsEndsWithReturn () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VWhile : public VStatement {
public:
  VExpression *Expr;
  VStatement *Statement;

  VWhile (VExpression *AExpr, VStatement *AStatement, const TLocation &ALoc);
  virtual ~VWhile () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsEndsWithReturn () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VDo : public VStatement {
public:
  VExpression *Expr;
  VStatement *Statement;

  VDo (VExpression *AExpr, VStatement *AStatement, const TLocation &ALoc);
  virtual ~VDo () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsEndsWithReturn () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VFor : public VStatement {
public:
  TArray<VExpression *> InitExpr;
  VExpression *CondExpr;
  TArray<VExpression *> LoopExpr;
  VStatement *Statement;

  VFor (const TLocation &ALoc);
  virtual ~VFor () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsEndsWithReturn () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VForeach : public VStatement {
public:
  VExpression *Expr;
  VStatement *Statement;

  VForeach (VExpression *AExpr, VStatement *AStatement, const TLocation &ALoc);
  virtual ~VForeach () override;
  virtual bool Resolve (VEmitContext&) override;
  virtual void DoEmit (VEmitContext&) override;
  virtual bool IsEndsWithReturn () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VSwitch : public VStatement {
public:
  struct VCaseInfo {
    vint32 Value;
    VLabel Address;
  };

  VExpression *Expr;
  TArray<VCaseInfo> CaseInfo;
  VLabel DefaultAddress;
  TArray<VStatement *> Statements;
  bool HaveDefault;

  VSwitch (const TLocation &ALoc);
  virtual ~VSwitch () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsEndsWithReturn () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VSwitchCase : public VStatement {
public:
  VSwitch *Switch;
  VExpression *Expr;
  vint32 Value;
  vint32 Index;

  VSwitchCase (VSwitch *ASwitch, VExpression *AExpr, const TLocation &ALoc);
  virtual ~VSwitchCase () override;
  virtual bool Resolve (VEmitContext&) override;
  virtual void DoEmit (VEmitContext&) override;
  virtual bool IsSwitchCase () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VSwitchDefault : public VStatement {
public:
  VSwitch *Switch;

  VSwitchDefault (VSwitch *ASwitch, const TLocation &ALoc);
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsSwitchDefault () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VBreak : public VStatement {
public:
  VBreak (const TLocation &ALoc);
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsBreak () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VContinue : public VStatement {
public:
  VContinue (const TLocation &ALoc);
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsContinue () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VReturn : public VStatement {
public:
  VExpression *Expr;
  int NumLocalsToClear;

  VReturn (VExpression *AExpr, const TLocation &ALoc);
  virtual ~VReturn () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsReturn () override;
  virtual bool IsEndsWithReturn () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VExpressionStatement : public VStatement {
public:
  VExpression *Expr;

  VExpressionStatement (VExpression *AExpr);
  virtual ~VExpressionStatement () noexcept(false) override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VLocalVarStatement : public VStatement {
public:
  VLocalDecl *Decl;

  VLocalVarStatement (VLocalDecl *ADecl);
  virtual ~VLocalVarStatement () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VCompound : public VStatement {
public:
  TArray<VStatement *> Statements;

  VCompound (const TLocation &ALoc);
  virtual ~VCompound () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsEndsWithReturn () override;
};
