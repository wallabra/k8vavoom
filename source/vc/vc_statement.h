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

class VStatement
{
public:
  TLocation   Loc;

  VStatement(const TLocation&);
  virtual ~VStatement() noexcept(false);
  virtual bool Resolve(VEmitContext&) = 0;
  virtual void DoEmit(VEmitContext&) = 0;
  void Emit(VEmitContext&);
  virtual bool IsBreak () { return false; }
  virtual bool IsContinue () { return false; }
  virtual bool IsReturn () { return false; }
  virtual bool IsCase () { return false; }
  virtual bool IsDefault () { return false; }
  virtual bool IsEndsWithReturn () { return false; }
};

class VEmptyStatement : public VStatement
{
public:
  VEmptyStatement(const TLocation&);
  bool Resolve(VEmitContext&);
  void DoEmit(VEmitContext&);
  virtual bool IsEndsWithReturn () { return false; }
};

class VIf : public VStatement
{
public:
  VExpression*  Expr;
  VStatement*   TrueStatement;
  VStatement*   FalseStatement;

  VIf(VExpression*, VStatement*, const TLocation&);
  VIf(VExpression*, VStatement*, VStatement*, const TLocation&);
  ~VIf();
  bool Resolve(VEmitContext&);
  void DoEmit(VEmitContext&);
  virtual bool IsEndsWithReturn () {
    if (TrueStatement && FalseStatement) return (TrueStatement->IsEndsWithReturn() && FalseStatement->IsEndsWithReturn());
    if (TrueStatement) return TrueStatement->IsEndsWithReturn();
    if (FalseStatement) return FalseStatement->IsEndsWithReturn();
    return false;
  }
};

class VWhile : public VStatement
{
public:
  VExpression*    Expr;
  VStatement*     Statement;

  VWhile(VExpression*, VStatement*, const TLocation&);
  ~VWhile();
  bool Resolve(VEmitContext&);
  void DoEmit(VEmitContext&);
  virtual bool IsEndsWithReturn () { return (Statement && Statement->IsEndsWithReturn()); }
};

class VDo : public VStatement
{
public:
  VExpression*    Expr;
  VStatement*     Statement;

  VDo(VExpression*, VStatement*, const TLocation&);
  ~VDo();
  bool Resolve(VEmitContext&);
  void DoEmit(VEmitContext&);
  virtual bool IsEndsWithReturn () { return (Statement && Statement->IsEndsWithReturn()); }
};

class VFor : public VStatement
{
public:
  TArray<VExpression*>  InitExpr;
  VExpression*      CondExpr;
  TArray<VExpression*>  LoopExpr;
  VStatement*       Statement;

  VFor(const TLocation&);
  ~VFor();
  bool Resolve(VEmitContext&);
  void DoEmit(VEmitContext&);
  virtual bool IsEndsWithReturn () { return (Statement && Statement->IsEndsWithReturn()); }
};

class VForeach : public VStatement
{
public:
  VExpression*    Expr;
  VStatement*     Statement;

  VForeach(VExpression*, VStatement*, const TLocation&);
  ~VForeach();
  bool Resolve(VEmitContext&);
  void DoEmit(VEmitContext&);
  virtual bool IsEndsWithReturn () { return (Statement && Statement->IsEndsWithReturn()); }
};

class VSwitch : public VStatement
{
public:
  struct VCaseInfo
  {
    vint32      Value;
    VLabel      Address;
  };

  VExpression*    Expr;
  TArray<VCaseInfo> CaseInfo;
  VLabel        DefaultAddress;
  TArray<VStatement*> Statements;
  bool        HaveDefault;

  VSwitch(const TLocation&);
  ~VSwitch();
  bool Resolve(VEmitContext&);
  void DoEmit(VEmitContext&);
  virtual bool IsEndsWithReturn () {
    if (Statements.Num() == 0) return false;
    bool defautSeen = false;
    bool returnSeen = false;
    bool breakSeen = false;
    bool statementSeen = false;
    for (int n = 0; n < Statements.Num(); ++n) {
      if (!Statements[n]) return false;
      // `case` or `default`?
      if (Statements[n]->IsCase() || Statements[n]->IsDefault()) {
        if (!returnSeen && statementSeen) return false; // oops
        if (Statements[n]->IsDefault()) defautSeen = true;
        breakSeen = false;
        statementSeen = true;
        returnSeen = false;
        continue;
      }
      if (breakSeen) continue;
      statementSeen = true;
      if (Statements[n]->IsBreak() || Statements[n]->IsContinue()) {
        // `break`
        if (!returnSeen) return false;
        breakSeen = true;
      } else {
        // normal statement
        if (!returnSeen) returnSeen = Statements[n]->IsEndsWithReturn();
      }
    }
    if (!statementSeen) return false; // just in case
    // without `default` it may fallthru
    return (returnSeen && defautSeen);
  }
};

class VSwitchCase : public VStatement
{
public:
  VSwitch*    Switch;
  VExpression*  Expr;
  vint32      Value;
  vint32      Index;

  VSwitchCase(VSwitch*, VExpression*, const TLocation&);
  ~VSwitchCase();
  bool Resolve(VEmitContext&);
  void DoEmit(VEmitContext&);
  virtual bool IsCase () { return true; }
};

class VSwitchDefault : public VStatement
{
public:
  VSwitch*    Switch;

  VSwitchDefault(VSwitch*, const TLocation&);
  bool Resolve(VEmitContext&);
  void DoEmit(VEmitContext&);
  virtual bool IsDefault () { return true; }
};

class VBreak : public VStatement
{
public:
  VBreak(const TLocation&);
  bool Resolve(VEmitContext&);
  void DoEmit(VEmitContext&);
  virtual bool IsBreak () { return true; }
};

class VContinue : public VStatement
{
public:
  VContinue(const TLocation&);
  bool Resolve(VEmitContext&);
  void DoEmit(VEmitContext&);
  virtual bool IsContinue () { return true; }
};

class VReturn : public VStatement
{
public:
  VExpression*    Expr;
  int         NumLocalsToClear;

  VReturn(VExpression*, const TLocation&);
  ~VReturn();
  bool Resolve(VEmitContext&);
  void DoEmit(VEmitContext&);
  virtual bool IsReturn () { return true; }
  virtual bool IsEndsWithReturn () { return true; }
};

class VExpressionStatement : public VStatement
{
public:
  VExpression*    Expr;

  VExpressionStatement(VExpression*);
  virtual ~VExpressionStatement() noexcept(false);
  bool Resolve(VEmitContext&);
  void DoEmit(VEmitContext&);
};

class VLocalVarStatement : public VStatement
{
public:
  VLocalDecl*   Decl;

  VLocalVarStatement(VLocalDecl*);
  ~VLocalVarStatement();
  bool Resolve(VEmitContext&);
  void DoEmit(VEmitContext&);
};

class VCompound : public VStatement
{
public:
  TArray<VStatement*>   Statements;

  VCompound(const TLocation&);
  ~VCompound();
  bool Resolve(VEmitContext&);
  void DoEmit(VEmitContext&);
  virtual bool IsEndsWithReturn () {
    for (int n = 0; n < Statements.Num(); ++n) {
      if (!Statements[n]) continue;
      if (Statements[n]->IsEndsWithReturn()) return true;
      if (Statements[n]->IsBreak() || Statements[n]->IsContinue()) break;
    }
    return false;
  }
};
