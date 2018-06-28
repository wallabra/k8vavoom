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
class VSwitch;
class VLabelStmt;


// ////////////////////////////////////////////////////////////////////////// //
class VStatement {
public:
  TLocation Loc;

  VStatement (const TLocation &);
  virtual ~VStatement();
  virtual VStatement *SyntaxCopy () = 0;
  virtual bool Resolve (VEmitContext &) = 0;
  virtual void DoEmit (VEmitContext &) = 0;
  void Emit (VEmitContext &);

  virtual bool IsLabel () const;
  virtual VName GetLabelName () const;
  virtual bool IsGoto () const; // any, including `goto case` and `goto default`
  virtual bool IsGotoCase () const;
  virtual bool HasGotoCaseExpr () const;
  virtual bool IsGotoDefault () const;
  virtual bool IsBreak () const;
  virtual bool IsContinue () const;
  virtual bool IsFlowStop () const; // break, continue, goto...
  virtual bool IsReturn () const;
  virtual bool IsSwitchCase () const;
  virtual bool IsSwitchDefault () const;
  virtual bool IsVarDecl () const;
  virtual bool IsEndsWithReturn () const;
  virtual bool IsProperCaseEnd (bool skipBreak) const; // ends with `return`, `break`, `continue`, `goto case` or `goto default`

  virtual VLabelStmt *FindLabel (VName aname);
  virtual bool IsGotoInAllowed () const;
  virtual bool IsGotoOutAllowed () const;
  virtual bool IsJumpOverAllowed (const VStatement *s0, const VStatement *s1) const;

  // returns `false` if statement not found (and `path` is not modified)
  virtual bool BuildPathTo (const VStatement *dest, TArray<VStatement *> &path);

protected:
  VStatement () {}
  virtual void DoSyntaxCopyTo (VStatement *e);

public:
  virtual void DoFixSwitch (VSwitch *aold, VSwitch *anew);
};


// ////////////////////////////////////////////////////////////////////////// //
class VEmptyStatement : public VStatement {
public:
  VEmptyStatement (const TLocation &);
  virtual VStatement *SyntaxCopy () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;

protected:
  VEmptyStatement () {}
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
  virtual VStatement *SyntaxCopy () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsEndsWithReturn () const override;
  virtual bool IsProperCaseEnd (bool skipBreak) const override;

  virtual VLabelStmt *FindLabel (VName aname) override;

  virtual bool BuildPathTo (const VStatement *dest, TArray<VStatement *> &path) override;

protected:
  VIf () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;
public:
  virtual void DoFixSwitch (VSwitch *aold, VSwitch *anew) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VWhile : public VStatement {
public:
  VExpression *Expr;
  VStatement *Statement;

  VWhile (VExpression *AExpr, VStatement *AStatement, const TLocation &ALoc);
  virtual ~VWhile () override;
  virtual VStatement *SyntaxCopy () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsEndsWithReturn () const override;
  virtual bool IsProperCaseEnd (bool skipBreak) const override;

  virtual VLabelStmt *FindLabel (VName aname) override;

  virtual bool BuildPathTo (const VStatement *dest, TArray<VStatement *> &path) override;

protected:
  VWhile () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;

public:
  virtual void DoFixSwitch (VSwitch *aold, VSwitch *anew) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VDo : public VStatement {
public:
  VExpression *Expr;
  VStatement *Statement;

  VDo (VExpression *AExpr, VStatement *AStatement, const TLocation &ALoc);
  virtual ~VDo () override;
  virtual VStatement *SyntaxCopy () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsEndsWithReturn () const override;
  virtual bool IsProperCaseEnd (bool skipBreak) const override;

  virtual VLabelStmt *FindLabel (VName aname) override;

  virtual bool BuildPathTo (const VStatement *dest, TArray<VStatement *> &path) override;

protected:
  VDo () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;

public:
  virtual void DoFixSwitch (VSwitch *aold, VSwitch *anew) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VFor : public VStatement {
public:
  TArray<VExpression *> InitExpr;
  TArray<VExpression *> CondExpr;
  TArray<VExpression *> LoopExpr;
  VStatement *Statement;

  VFor (const TLocation &ALoc);
  virtual ~VFor () override;
  virtual VStatement *SyntaxCopy () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsEndsWithReturn () const override;
  virtual bool IsProperCaseEnd (bool skipBreak) const override;

  virtual VLabelStmt *FindLabel (VName aname) override;
  virtual bool IsGotoInAllowed () const override;

  virtual bool BuildPathTo (const VStatement *dest, TArray<VStatement *> &path) override;

protected:
  VFor () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;

public:
  virtual void DoFixSwitch (VSwitch *aold, VSwitch *anew) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VForeach : public VStatement {
public:
  VExpression *Expr;
  VStatement *Statement;

  VForeach (VExpression *AExpr, VStatement *AStatement, const TLocation &ALoc);
  virtual ~VForeach () override;
  virtual VStatement *SyntaxCopy () override;
  virtual bool Resolve (VEmitContext&) override;
  virtual void DoEmit (VEmitContext&) override;
  virtual bool IsEndsWithReturn () const override;
  virtual bool IsProperCaseEnd (bool skipBreak) const override;

  virtual VLabelStmt *FindLabel (VName aname) override;
  virtual bool IsGotoInAllowed () const override;
  virtual bool IsGotoOutAllowed () const override;

  virtual bool BuildPathTo (const VStatement *dest, TArray<VStatement *> &path) override;

protected:
  VForeach () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;

public:
  virtual void DoFixSwitch (VSwitch *aold, VSwitch *anew) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VForeachIota : public VStatement {
private:
  VExpression *varinit; // var initializer expression
  VExpression *varnext; // loop/check expression (++var < hi)
  VExpression *hiinit; // hivar initializer

public:
  VExpression *var; // loop variable (resolved to first-check expression)
  VExpression *lo; // low bound
  VExpression *hi; // high bound
  VStatement *statement;
  bool reversed;

  VForeachIota (const TLocation &ALoc);
  virtual ~VForeachIota () override;
  virtual VStatement *SyntaxCopy () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsEndsWithReturn () const override;
  virtual bool IsProperCaseEnd (bool skipBreak) const override;

  virtual VLabelStmt *FindLabel (VName aname) override;
  virtual bool IsGotoInAllowed () const override;

  virtual bool BuildPathTo (const VStatement *dest, TArray<VStatement *> &path) override;

protected:
  VForeachIota () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;

public:
  virtual void DoFixSwitch (VSwitch *aold, VSwitch *anew) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VForeachArray : public VStatement {
private:
  VExpression *idxinit;
  VExpression *hiinit;
  VExpression *loopPreCheck;
  VExpression *loopNext;
  VExpression *loopLoad;
  VExpression *varaddr;

private:
  bool DoResolveScriptIter (VEmitContext &ec);

public:
  VExpression *idxvar; // index variable (can be null if hidden)
  VExpression *var; // value variable
  VExpression *arr; // array
  VStatement *statement;
  bool reversed;
  bool isRef; // if `var` a reference?

  VForeachArray (VExpression *aarr, VExpression *aidx, VExpression *avar, bool aVarRef, const TLocation &aloc);
  virtual ~VForeachArray () override;
  virtual VStatement *SyntaxCopy () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsEndsWithReturn () const override;
  virtual bool IsProperCaseEnd (bool skipBreak) const override;

  virtual VLabelStmt *FindLabel (VName aname) override;
  virtual bool IsGotoInAllowed () const override;

  virtual bool BuildPathTo (const VStatement *dest, TArray<VStatement *> &path) override;

protected:
  VForeachArray () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;

public:
  virtual void DoFixSwitch (VSwitch *aold, VSwitch *anew) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VForeachScripted : public VStatement {
public:
  struct Var {
    VExpression *var;
    bool isRef;
    VLocalDecl *decl; // used in parser

    Var () : var(nullptr), isRef(false), decl(nullptr) {}
  };

private:
  bool isBoolInit;
  VExpression *ivInit; // invocation, init
  VExpression *ivNext; // invocation, next
  VExpression *ivDone; // invocation, done, can be null

public:
  VExpression *arr; // array
  Var fevars[VMethod::MAX_PARAMS];
  int fevarCount;
  VStatement *statement;
  bool reversed;

  VForeachScripted (VExpression *aarr, int afeCount, Var *afevars, const TLocation &aloc);
  virtual ~VForeachScripted () override;
  virtual VStatement *SyntaxCopy () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsEndsWithReturn () const override;
  virtual bool IsProperCaseEnd (bool skipBreak) const override;

  virtual VLabelStmt *FindLabel (VName aname) override;
  virtual bool IsGotoInAllowed () const override;
  virtual bool IsGotoOutAllowed () const override;

  virtual bool BuildPathTo (const VStatement *dest, TArray<VStatement *> &path) override;

protected:
  VForeachScripted () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;

public:
  virtual void DoFixSwitch (VSwitch *aold, VSwitch *anew) override;
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
  virtual VStatement *SyntaxCopy () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsEndsWithReturn () const override;
  virtual bool IsProperCaseEnd (bool skipBreak) const override;

  virtual VLabelStmt *FindLabel (VName aname) override;

  virtual bool BuildPathTo (const VStatement *dest, TArray<VStatement *> &path) override;

  virtual bool IsJumpOverAllowed (const VStatement *s0, const VStatement *s1) const override;

  //void PostProcessGotoCase ();

protected:
  VSwitch () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;

  bool checkProperCaseEnd (bool reportSwitchCase) const;

public:
  virtual void DoFixSwitch (VSwitch *aold, VSwitch *anew) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VSwitchCase : public VStatement {
public:
  VSwitch *Switch;
  VExpression *Expr;
  vint32 Value;
  vint32 Index;
  // the following need not to be copied by `SyntaxCopy()`
  VLabel gotoLbl; // if `gotoLbl.IsDefined()` is `true`, need to emit it; set by `goto case;`

  VSwitchCase (VSwitch *ASwitch, VExpression *AExpr, const TLocation &ALoc);
  virtual ~VSwitchCase () override;
  virtual VStatement *SyntaxCopy () override;
  virtual bool Resolve (VEmitContext&) override;
  virtual void DoEmit (VEmitContext&) override;
  virtual bool IsSwitchCase () const override;

protected:
  VSwitchCase () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;

public:
  virtual void DoFixSwitch (VSwitch *aold, VSwitch *anew) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VSwitchDefault : public VStatement {
public:
  VSwitch *Switch;

  VSwitchDefault (VSwitch *ASwitch, const TLocation &ALoc);
  virtual VStatement *SyntaxCopy () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsSwitchDefault () const override;

protected:
  VSwitchDefault () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;

public:
  virtual void DoFixSwitch (VSwitch *aold, VSwitch *anew) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VBreak : public VStatement {
public:
  // need not to be copied (set in `switch` resolver)
  bool skipCodegen;

  VBreak (const TLocation &ALoc);
  virtual VStatement *SyntaxCopy () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsBreak () const override;

protected:
  VBreak () {}
};


// ////////////////////////////////////////////////////////////////////////// //
class VContinue : public VStatement {
public:
  VContinue (const TLocation &ALoc);
  virtual VStatement *SyntaxCopy () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsContinue () const override;

protected:
  VContinue () {}
};


// ////////////////////////////////////////////////////////////////////////// //
class VReturn : public VStatement {
public:
  VExpression *Expr;
  int NumLocalsToClear;

  VReturn (VExpression *AExpr, const TLocation &ALoc);
  virtual ~VReturn () override;
  virtual VStatement *SyntaxCopy () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsReturn () const override;

protected:
  VReturn () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VExpressionStatement : public VStatement {
public:
  VExpression *Expr;

  VExpressionStatement (VExpression *AExpr);
  virtual ~VExpressionStatement () override;
  virtual VStatement *SyntaxCopy () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;

protected:
  VExpressionStatement () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VLocalVarStatement : public VStatement {
public:
  VLocalDecl *Decl;

  VLocalVarStatement (VLocalDecl *ADecl);
  virtual ~VLocalVarStatement () override;
  virtual VStatement *SyntaxCopy () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;

  virtual bool IsVarDecl () const override;

protected:
  VLocalVarStatement () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VDeleteStatement : public VStatement {
private:
  // resolved expressions
  VExpression *delexpr; // var.Destroy()
  VExpression *assexpr; // var = none;
  VExpression *checkexpr; // bool(var)

public:
  VExpression *var;

  VDeleteStatement (VExpression *avar, const TLocation &aloc);
  virtual ~VDeleteStatement () override;
  virtual VStatement *SyntaxCopy () override;
  virtual bool Resolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

protected:
  VDeleteStatement () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VCompound : public VStatement {
public:
  TArray<VStatement *> Statements;

  VCompound (const TLocation &ALoc);
  virtual ~VCompound () override;
  virtual VStatement *SyntaxCopy () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;
  virtual bool IsEndsWithReturn () const override;
  virtual bool IsProperCaseEnd (bool skipBreak) const override;

  virtual VLabelStmt *FindLabel (VName aname) override;

  virtual bool BuildPathTo (const VStatement *dest, TArray<VStatement *> &path) override;

  virtual bool IsJumpOverAllowed (const VStatement *s0, const VStatement *s1) const override;

protected:
  VCompound () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;

public:
  virtual void DoFixSwitch (VSwitch *aold, VSwitch *anew) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VLabelStmt : public VStatement {
public:
  VName Name;

  VLabelStmt (VName aname, const TLocation &ALoc);
  virtual VStatement *SyntaxCopy () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;

  virtual bool IsLabel () const override;
  virtual VName GetLabelName () const override;

protected:
  VLabelStmt () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VGotoStmt : public VStatement {
private:
  VStatement *casedef;
  VLabelStmt *gotolbl;

public:
  enum { Normal, Case, Default };

public:
  VName Name; // destination (NAME_None for `goto case;`)
  VSwitch *Switch; // for `goto case;`
  VExpression *CaseValue; // for `goto case n;`
  int GotoType;
  int SwitchStNum;
  // need not to be copied (set in `switch` resolver)
  bool skipCodegen;

  VGotoStmt (VName aname, const TLocation &ALoc);
  VGotoStmt (VSwitch *ASwitch, VExpression *ACaseValue, int ASwitchStNum, bool toDefault, const TLocation &ALoc);
  virtual VStatement *SyntaxCopy () override;
  virtual bool Resolve (VEmitContext &) override;
  virtual void DoEmit (VEmitContext &) override;

  virtual bool IsGoto () const override;
  virtual bool IsGotoCase () const override;
  virtual bool HasGotoCaseExpr () const override;
  virtual bool IsGotoDefault () const override;
  virtual VName GetLabelName () const;

protected:
  VGotoStmt () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;

  bool ResolveGoto (VEmitContext &ec, VStatement *dest);
};
