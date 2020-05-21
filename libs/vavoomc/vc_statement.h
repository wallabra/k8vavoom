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
//**  Copyright (C) 2018-2020 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, version 3 of the License ONLY.
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
class VSwitch;


// ////////////////////////////////////////////////////////////////////////// //
class VStatement {
public:
  TLocation Loc;
  // linked list of "parent" statements
  VStatement *UpScope;
  // set to `true` to skip calling `EmitDtor()`
  bool skipDtorOnLeave;
  // set to `true` to skip calling `EmitFinalizer()`
  bool skipFinalizerOnLeave;
  // break and continue labels, used in break and continue statements
  VLabel breakLabel;
  VLabel contLabel;
  // any statement can have attached label (but it is used only for loops for now)
  VName Label;

protected:
  struct UpScopeGuard {
    VStatement *st;
    VStatement *oldUp;

    UpScopeGuard () = delete;
    UpScopeGuard &operator = (const UpScopeGuard &) = delete;

    inline UpScopeGuard (VStatement *self, VStatement *newUp) noexcept : st(self) {
      vassert(self);
      oldUp = self->UpScope;
      self->UpScope = newUp;
    }

    inline ~UpScopeGuard () noexcept { Restore(); }

    inline void Restore () noexcept {
      if (st) {
        st->UpScope = oldUp;
        st = nullptr;
        oldUp = nullptr;
      }
    }
  };

public:
  VStatement (const TLocation &ALoc);
  virtual ~VStatement ();
  virtual VStatement *SyntaxCopy () = 0;

  virtual VStatement *DoResolve (VEmitContext &ec) = 0;
  virtual void DoEmit (VEmitContext &ec) = 0;

  // dtors will be emited before scope exit
  virtual void EmitDtor (VEmitContext &ec);
  // finalizers will be emited after scope exit
  virtual void EmitFinalizer (VEmitContext &ec);

  // emit dtor, and block automating dtor emiting
  inline void EmitDtorAndBlock (VEmitContext &ec) { EmitDtor(ec); skipDtorOnLeave = true; }

  // emit dtor, and block automating dtor emiting
  inline void EmitFinalizerAndBlock (VEmitContext &ec) { EmitFinalizer(ec); skipFinalizerOnLeave = true; }

  // those will manage `UpScope`, and will call dtor/finalizer emiters
  VStatement *Resolve (VEmitContext &ec, VStatement *aUpScope);
  void Emit (VEmitContext &ec, VStatement *aUpScope);

  // this is used in `scope(exit)` to block `return`
  virtual bool IsReturnAllowed () const noexcept;
  // this is used to find the scope to `break` from
  virtual bool IsBreakScope () const noexcept;
  // this is used to find the scope to `continue` from
  virtual bool IsContinueScope () const noexcept;

  // this returns label attached to loops, to implement labeled break/cont
  virtual VName GetBCScopeLabel () const noexcept;

  virtual bool IsCompound () const noexcept;
  virtual bool IsEmptyStatement () const noexcept;
  virtual bool IsInvalidStatement () const noexcept;
  virtual VName GetLabelName () const noexcept;
  virtual bool IsGoto () const noexcept; // any, including `goto case` and `goto default`
  virtual bool IsFor () const noexcept;
  virtual bool IsGotoCase () const noexcept;
  virtual bool HasGotoCaseExpr () const noexcept;
  virtual bool IsGotoDefault () const noexcept;
  virtual bool IsBreak () const noexcept;
  virtual bool IsContinue () const noexcept;
  virtual bool IsFlowStop () const noexcept; // break, continue, goto...
  virtual bool IsReturn () const noexcept;
  virtual bool IsSwitchCase () const noexcept;
  virtual bool IsSwitchDefault () const noexcept;
  virtual bool IsVarDecl () const noexcept;
  virtual bool IsEndsWithReturn () const noexcept;
  virtual bool IsProperCaseEnd (bool skipBreak) const noexcept; // ends with `return`, `break`, `continue`, `goto case` or `goto default`

  // this checks for `if (...)\nstat;`
  bool CheckCondIndent (const TLocation &condLoc, VStatement *body);

  virtual VStr toString () = 0;

  inline bool IsValid () const noexcept { return !IsInvalidStatement(); }

  // this will do `delete this`
  VStatement *CreateInvalid ();

protected:
  VStatement () {}
  virtual void DoSyntaxCopyTo (VStatement *e);

public:
  virtual void DoFixSwitch (VSwitch *aold, VSwitch *anew);
};


// ////////////////////////////////////////////////////////////////////////// //
class VInvalidStatement : public VStatement {
public:
  VInvalidStatement (const TLocation &);
  virtual VStatement *SyntaxCopy () override;

  virtual VStatement *DoResolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

  virtual bool IsInvalidStatement () const noexcept override;

  virtual VStr toString () override;

protected:
  VInvalidStatement () {}
};


// ////////////////////////////////////////////////////////////////////////// //
class VEmptyStatement : public VStatement {
public:
  VEmptyStatement (const TLocation &);
  virtual VStatement *SyntaxCopy () override;

  virtual VStatement *DoResolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

  virtual bool IsEmptyStatement () const noexcept override;

  virtual VStr toString () override;

protected:
  VEmptyStatement () {}
};


// ////////////////////////////////////////////////////////////////////////// //
class VAssertStatement : public VStatement {
private:
  VExpression *FatalInvoke;

public:
  VExpression *Expr;
  VExpression *Message; // can be `nullptr`

  VAssertStatement (const TLocation &ALoc, VExpression *AExpr, VExpression *AMsg);
  virtual ~VAssertStatement () override;
  virtual VStatement *SyntaxCopy () override;

  virtual VStatement *DoResolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

  virtual VStr toString () override;

protected:
  VAssertStatement () {}
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

  virtual VStatement *DoResolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

  virtual VStr toString () override;

protected:
  VDeleteStatement () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VExpressionStatement : public VStatement {
public:
  VExpression *Expr;

  VExpressionStatement (VExpression *AExpr);
  virtual ~VExpressionStatement () override;
  virtual VStatement *SyntaxCopy () override;

  virtual VStatement *DoResolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

  virtual VStr toString () override;

protected:
  VExpressionStatement () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VIf : public VStatement {
public:
  VExpression *Expr;
  VStatement *TrueStatement;
  VStatement *FalseStatement;
  TLocation ElseLoc;
  bool doIndentCheck;

  VIf (VExpression *AExpr, VStatement *ATrueStatement, const TLocation &ALoc, bool ADoIndentCheck=true);
  VIf (VExpression *AExpr, VStatement *ATrueStatement, VStatement *AFalseStatement, const TLocation &ALoc, const TLocation &AElseLoc, bool ADoIndentCheck=true);
  virtual ~VIf () override;
  virtual VStatement *SyntaxCopy () override;

  virtual VStatement *DoResolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

  virtual bool IsEndsWithReturn () const noexcept override;
  virtual bool IsProperCaseEnd (bool skipBreak) const noexcept override;

  virtual VStr toString () override;

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

  virtual VStatement *DoResolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

  virtual bool IsBreakScope () const noexcept override;
  virtual bool IsContinueScope () const noexcept override;

  virtual bool IsEndsWithReturn () const noexcept override;
  virtual bool IsProperCaseEnd (bool skipBreak) const noexcept override;

  virtual VStr toString () override;

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

  virtual VStatement *DoResolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

  virtual bool IsBreakScope () const noexcept override;
  virtual bool IsContinueScope () const noexcept override;

  virtual bool IsEndsWithReturn () const noexcept override;
  virtual bool IsProperCaseEnd (bool skipBreak) const noexcept override;

  virtual VStr toString () override;

protected:
  VDo () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;

public:
  virtual void DoFixSwitch (VSwitch *aold, VSwitch *anew) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VFor : public VStatement {
public:
  //TArray<VExpression *> InitExpr; // moved to outer scope
  TArray<VExpression *> CondExpr;
  TArray<VExpression *> LoopExpr;
  VStatement *Statement;

  VFor (const TLocation &ALoc);
  virtual ~VFor () override;
  virtual VStatement *SyntaxCopy () override;

  virtual VStatement *DoResolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

  virtual bool IsFor () const noexcept override;

  virtual bool IsBreakScope () const noexcept override;
  virtual bool IsContinueScope () const noexcept override;

  virtual bool IsEndsWithReturn () const noexcept override;
  virtual bool IsProperCaseEnd (bool skipBreak) const noexcept override;

  virtual VStr toString () override;

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

  virtual VStatement *DoResolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

  virtual bool IsBreakScope () const noexcept override;
  virtual bool IsContinueScope () const noexcept override;

  virtual void EmitFinalizer (VEmitContext &ec) override;

  virtual bool IsEndsWithReturn () const noexcept override;
  virtual bool IsProperCaseEnd (bool skipBreak) const noexcept override;

  virtual VStr toString () override;

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

  TArray<int> tempLocals;

public:
  VExpression *var; // loop variable (resolved to first-check expression)
  VExpression *lo; // low bound
  VExpression *hi; // high bound
  VStatement *Statement;
  bool reversed;

  VForeachIota (const TLocation &ALoc);
  virtual ~VForeachIota () override;
  virtual VStatement *SyntaxCopy () override;

  virtual VStatement *DoResolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

  virtual void EmitDtor (VEmitContext &ec) override;

  virtual bool IsBreakScope () const noexcept override;
  virtual bool IsContinueScope () const noexcept override;

  virtual bool IsEndsWithReturn () const noexcept override;
  virtual bool IsProperCaseEnd (bool skipBreak) const noexcept override;

  virtual VStr toString () override;

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

  TArray<int> tempLocals;

private:
  VStatement *DoResolveScriptIter (VEmitContext &ec);

public:
  VExpression *idxvar; // index variable (can be null if hidden)
  VExpression *var; // value variable
  VExpression *arr; // array
  VStatement *Statement;
  bool reversed;
  bool isRef; // if `var` a reference?
  bool isConst; // if `var` a const?

  VForeachArray (VExpression *aarr, VExpression *aidx, VExpression *avar, bool aVarRef, bool aVarConst, const TLocation &aloc);
  virtual ~VForeachArray () override;
  virtual VStatement *SyntaxCopy () override;

  virtual VStatement *DoResolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

  virtual void EmitDtor (VEmitContext &ec) override;

  virtual bool IsBreakScope () const noexcept override;
  virtual bool IsContinueScope () const noexcept override;

  virtual bool IsEndsWithReturn () const noexcept override;
  virtual bool IsProperCaseEnd (bool skipBreak) const noexcept override;

  virtual VStr toString () override;

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
    bool isConst; // if `var` a const?
    VLocalDecl *decl; // used in parser

    Var () : var(nullptr), isRef(false), isConst(false), decl(nullptr) {}
  };

private:
  bool isBoolInit;
  VExpression *ivInit; // invocation, init
  VExpression *ivNext; // invocation, next
  VExpression *ivDone; // invocation, done, can be null

  TArray<int> tempLocals;

public:
  VExpression *arr; // array
  Var fevars[VMethod::MAX_PARAMS];
  int fevarCount;
  VStatement *Statement;
  bool reversed;

  VForeachScripted (VExpression *aarr, int afeCount, Var *afevars, const TLocation &aloc);
  virtual ~VForeachScripted () override;
  virtual VStatement *SyntaxCopy () override;

  virtual VStatement *DoResolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

  virtual bool IsBreakScope () const noexcept override;
  virtual bool IsContinueScope () const noexcept override;

  virtual void EmitDtor (VEmitContext &ec) override;
  virtual void EmitFinalizer (VEmitContext &ec) override;

  virtual bool IsEndsWithReturn () const noexcept override;
  virtual bool IsProperCaseEnd (bool skipBreak) const noexcept override;

  virtual VStr toString () override;

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

  virtual VStatement *DoResolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

  virtual bool IsBreakScope () const noexcept override;

  virtual bool IsEndsWithReturn () const noexcept override;
  virtual bool IsProperCaseEnd (bool skipBreak) const noexcept override;

  virtual VStr toString () override;

protected:
  VSwitch () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;

  bool checkProperCaseEnd (bool reportSwitchCase) const noexcept;

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

  virtual VStatement *DoResolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

  virtual bool IsSwitchCase () const noexcept override;

  virtual VStr toString () override;

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

  virtual VStatement *DoResolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

  virtual bool IsSwitchDefault () const noexcept override;

  virtual VStr toString () override;

protected:
  VSwitchDefault () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;

public:
  virtual void DoFixSwitch (VSwitch *aold, VSwitch *anew) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VBreak : public VStatement {
public:
  VName LoopLabel; // can be empty

  VBreak (const TLocation &ALoc, VName aLoopLabel=NAME_None);
  virtual VStatement *SyntaxCopy () override;

  virtual VStatement *DoResolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

  virtual bool IsBreak () const noexcept override;

  virtual VStr toString () override;

protected:
  VBreak () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VContinue : public VStatement {
public:
  VName LoopLabel; // can be empty

  VContinue (const TLocation &ALoc, VName aLoopLabel=NAME_None);
  virtual VStatement *SyntaxCopy () override;

  virtual VStatement *DoResolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

  virtual bool IsContinue () const noexcept override;

  virtual VStr toString () override;

protected:
  VContinue () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VReturn : public VStatement {
public:
  VExpression *Expr;
  int NumLocalsToClear;

  VReturn (VExpression *AExpr, const TLocation &ALoc);
  virtual ~VReturn () override;
  virtual VStatement *SyntaxCopy () override;

  virtual VStatement *DoResolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

  virtual bool IsReturn () const noexcept override;

  virtual VStr toString () override;

protected:
  VReturn () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;
};


// ////////////////////////////////////////////////////////////////////////// //
// after compound resolving, this will get a scope
class VBaseCompoundStatement : public VStatement {
public:
  TArray<VStatement *> Statements;

  VBaseCompoundStatement (const TLocation &aloc);
  virtual ~VBaseCompoundStatement () override;

  // required for compounds
  virtual bool IsEndsWithReturn () const noexcept override;
  virtual bool IsProperCaseEnd (bool skipBreak) const noexcept override;

  virtual VStr toString () override;

protected:
  VBaseCompoundStatement () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;

  // recursively moves everything after local var decl into its own statement list.
  // WARNING! this is gum solution, i should invent a better way to do this!
  // currently this thing should be manually called before resolving statements.
  void ProcessVarDecls ();

public:
  virtual void DoFixSwitch (VSwitch *aold, VSwitch *anew) override;
};


// ////////////////////////////////////////////////////////////////////////// //
// after compound resolving, this will get a scope
class VLocalVarStatement : public VBaseCompoundStatement {
public:
  VLocalDecl *Decl;
  // `Statements` is filled by compound

  VLocalVarStatement (VLocalDecl *ADecl);
  virtual ~VLocalVarStatement () override;
  virtual VStatement *SyntaxCopy () override;

  virtual VStatement *DoResolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

  virtual void EmitDtor (VEmitContext &ec) override;

  virtual bool IsVarDecl () const noexcept override;

  virtual VStr toString () override;

protected:
  VLocalVarStatement () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VCompound : public VBaseCompoundStatement {
public:
  VCompound (const TLocation &ALoc);
  virtual VStatement *SyntaxCopy () override;

  virtual VStatement *DoResolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

  virtual bool IsCompound () const noexcept override;

protected:
  VCompound () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VCompoundScopeExit : public VCompound {
private:
  // i am too lazy to create proper AST nodes, so let's use this small hack instead
  bool mReturnAllowed;

public:
  VStatement *Body;

  VCompoundScopeExit (VStatement *ABody, const TLocation &ALoc);
  virtual ~VCompoundScopeExit () override;
  virtual VStatement *SyntaxCopy () override;

  virtual VStatement *DoResolve (VEmitContext &ec) override;

  virtual bool IsReturnAllowed () const noexcept override;

  virtual void EmitFinalizer (VEmitContext &ec) override;

  virtual VStr toString () override;

protected:
  VCompoundScopeExit () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VGotoStmt : public VStatement {
private:
  VStatement *casedef;

public:
  enum { Case, Default };

public:
  VSwitch *Switch; // for `goto case;`
  VExpression *CaseValue; // for `goto case n;`
  int GotoType;
  int SwitchStNum;

  VGotoStmt (VSwitch *ASwitch, VExpression *ACaseValue, int ASwitchStNum, bool toDefault, const TLocation &ALoc);
  virtual VStatement *SyntaxCopy () override;

  virtual VStatement *DoResolve (VEmitContext &ec) override;
  virtual void DoEmit (VEmitContext &ec) override;

  virtual bool IsGoto () const noexcept override;
  virtual bool IsGotoCase () const noexcept override;
  virtual bool HasGotoCaseExpr () const noexcept override;
  virtual bool IsGotoDefault () const noexcept override;

  virtual VStr toString () override;

protected:
  VGotoStmt () {}
  virtual void DoSyntaxCopyTo (VStatement *e) override;
};
