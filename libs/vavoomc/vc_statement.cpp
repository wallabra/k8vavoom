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
#include "vc_local.h"


#if 0
#include <string>
#include <cstdlib>
#include <cxxabi.h>

template<typename T> VStr shitppTypeName () {
  VStr tpn(typeid(T).name());
  char *dmn = abi::__cxa_demangle(*tpn, nullptr, nullptr, nullptr);
  if (dmn) {
    tpn = VStr(dmn);
    //Z_Free(dmn);
    // use `free()` here, because it is not allocated via zone allocator
    free(dmn);
  }
  return tpn;
}


template<class T> VStr shitppTypeNameObj (const T &o) {
  VStr tpn(typeid(o).name());
  char *dmn = abi::__cxa_demangle(*tpn, nullptr, nullptr, nullptr);
  if (dmn) {
    tpn = VStr(dmn);
    //Z_Free(dmn);
    // use `free()` here, because it is not allocated via zone allocator
    free(dmn);
  }
  return tpn;
}

# define GET_MY_TYPE()  (VStr(":")+shitppTypeNameObj(*this))
# define GET_OBJ_TYPE(o_)  ((o_) ? VStr("{")+shitppTypeNameObj(*(o_))+"}" : VStr("{null}"))
#else
# define GET_MY_TYPE()     VStr()
# define GET_OBJ_TYPE(o_)  VStr()
#endif



//**************************************************************************
//
// VStatement
//
//**************************************************************************

//==========================================================================
//
//  VStatement::VStatement
//
//==========================================================================
VStatement::VStatement (const TLocation &ALoc, VName aLabel)
  : Loc(ALoc)
  , UpScope(nullptr)
  , Label(aLabel)
  , Statement(nullptr)
{
}


//==========================================================================
//
//  VStatement::~VStatement
//
//==========================================================================
VStatement::~VStatement () {
  delete Statement; Statement = nullptr;
}


// ////////////////////////////////////////////////////////////////////////// //
void VStatement::EmitCtor (VEmitContext &ec) {}
void VStatement::EmitDtor (VEmitContext &ec, bool properLeave) {}
void VStatement::EmitFinalizer (VEmitContext &ec, bool properLeave) {}

// ////////////////////////////////////////////////////////////////////////// //
bool VStatement::IsCompound () const noexcept { return false; }
bool VStatement::IsAnyCompound () const noexcept { return false; }
bool VStatement::IsTryFinally () const noexcept { return false; }
bool VStatement::IsEmptyStatement () const noexcept { return false; }
bool VStatement::IsInvalidStatement () const noexcept { return false; }
bool VStatement::IsFor () const noexcept { return false; }
bool VStatement::IsGoto () const noexcept { return false; }
bool VStatement::IsGotoCase () const noexcept { return false; }
bool VStatement::IsGotoDefault () const noexcept { return false; }
bool VStatement::IsBreak () const noexcept { return false; }
bool VStatement::IsContinue () const noexcept { return false; }
bool VStatement::IsFlowStop () const noexcept { return false; }
bool VStatement::IsReturn () const noexcept { return false; }
bool VStatement::IsSwitchCase () const noexcept { return false; }
bool VStatement::IsSwitchDefault () const noexcept { return false; }
bool VStatement::IsVarDecl () const noexcept { return false; }
bool VStatement::IsInLoop () const noexcept { return false; }
bool VStatement::IsInReturn () const noexcept { return false; }

// ////////////////////////////////////////////////////////////////////////// //
bool VStatement::IsReturnAllowed () const noexcept { return true; }
bool VStatement::IsContBreakAllowed () const noexcept { return true; }
bool VStatement::IsBreakScope () const noexcept { return false; }
bool VStatement::IsContinueScope () const noexcept { return false; }


//==========================================================================
//
//  VStatement::DoSyntaxCopyTo
//
//==========================================================================
void VStatement::DoSyntaxCopyTo (VStatement *e) {
  e->Loc = Loc;
  e->Label = Label;
  e->Statement = (Statement ? Statement->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VStatement::IsEndsWithReturn
//
//==========================================================================
bool VStatement::IsEndsWithReturn () const noexcept {
  return (Statement && Statement->IsEndsWithReturn());
}


//==========================================================================
//
//  VStatement::IsProperCaseEnd
//
//==========================================================================
bool VStatement::IsProperCaseEnd (const VStatement *ASwitch) const noexcept {
  return (Statement && Statement->IsProperCaseEnd(ASwitch));
}


//==========================================================================
//
//  VStatement::DoFixSwitch
//
//==========================================================================
void VStatement::DoFixSwitch (VSwitch *aold, VSwitch *anew) {
  if (Statement) Statement->DoFixSwitch(aold, anew);
}


//==========================================================================
//
//  VStatement::CreateInvalid
//
//  this will do `delete this`
//
//==========================================================================
VStatement *VStatement::CreateInvalid () {
  VStatement *res = new VInvalidStatement(Loc);
  delete this;
  return res;
}


//==========================================================================
//
//  VStatement::CheckCondIndent
//
//  this checks for `if (...)\nstat;`
//
//==========================================================================
bool VStatement::CheckCondIndent (const TLocation &condLoc, VStatement *body) {
  if (!body) return true;
  if (body->IsCompound() || body->IsEmptyStatement()) return true;
  if (condLoc.GetLine() != body->Loc.GetLine()) {
    ParseError(condLoc, "please, use `{}` for multiline statements");
    return false;
  }
  return true;
}


//==========================================================================
//
//  VStatement::Resolve
//
//==========================================================================
VStatement *VStatement::Resolve (VEmitContext &ec, VStatement *aUpScope) {
  UpScopeGuard upguard(this, aUpScope);
  const int inloopInc = (IsInLoop() ? 1 : 0);
  const int inretInc = (IsInReturn() ? 1 : 0);
  ec.InLoop += inloopInc;
  ec.InReturn += inretInc;
  VStatement *res = DoResolve(ec);
  ec.InLoop -= inloopInc;
  ec.InReturn -= inretInc;
  // just in case
  vassert(res);
  return res;
}


//==========================================================================
//
//  VStatement::Emit
//
//==========================================================================
void VStatement::Emit (VEmitContext &ec, VStatement *aUpScope) {
  UpScopeGuard upguard(this, aUpScope);
  EmitCtor(ec);
  const int inloopInc = (IsInLoop() ? 1 : 0);
  const int inretInc = (IsInReturn() ? 1 : 0);
  ec.InLoop += inloopInc;
  ec.InReturn += inretInc;
  DoEmit(ec);
  ec.InLoop -= inloopInc;
  ec.InReturn -= inretInc;
  EmitDtor(ec, true); // proper leaving
  EmitFinalizer(ec, true); // proper leaving
}



//**************************************************************************
//
// VInvalidStatement
//
//**************************************************************************

//==========================================================================
//
//  VInvalidStatement::VInvalidStatement
//
//==========================================================================
VInvalidStatement::VInvalidStatement (const TLocation &ALoc)
   : VStatement(ALoc)
{
}


//==========================================================================
//
//  VInvalidStatement::SyntaxCopy
//
//==========================================================================
VStatement *VInvalidStatement::SyntaxCopy () {
  auto res = new VInvalidStatement();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VInvalidStatement::DoResolve
//
//==========================================================================
VStatement *VInvalidStatement::DoResolve (VEmitContext &) {
  VCFatalError("internal compiler error occured (tried to resolve invalid statement)");
  return this;
}


//==========================================================================
//
//  VInvalidStatement::DoEmit
//
//==========================================================================
void VInvalidStatement::DoEmit (VEmitContext &) {
}


//==========================================================================
//
//  VInvalidStatement::IsInvalidStatement
//
//==========================================================================
bool VInvalidStatement::IsInvalidStatement () const noexcept {
  return true;
}


//==========================================================================
//
//  VInvalidStatement::toString
//
//==========================================================================
VStr VInvalidStatement::toString () {
  return VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/<INVALID>;";
}



//**************************************************************************
//
// VEmptyStatement
//
//**************************************************************************

//==========================================================================
//
//  VEmptyStatement::VEmptyStatement
//
//==========================================================================
VEmptyStatement::VEmptyStatement (const TLocation &ALoc)
  : VStatement(ALoc)
{
}


//==========================================================================
//
//  VEmptyStatement::SyntaxCopy
//
//==========================================================================
VStatement *VEmptyStatement::SyntaxCopy () {
  auto res = new VEmptyStatement();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VEmptyStatement::DoResolve
//
//==========================================================================
VStatement *VEmptyStatement::DoResolve (VEmitContext &) {
  return this;
}


//==========================================================================
//
//  VEmptyStatement::DoEmit
//
//==========================================================================
void VEmptyStatement::DoEmit (VEmitContext &) {
}


//==========================================================================
//
//  VEmptyStatement::IsEmptyStatement
//
//==========================================================================
bool VEmptyStatement::IsEmptyStatement () const noexcept {
  return true;
}


//==========================================================================
//
//  VEmptyStatement::toString
//
//==========================================================================
VStr VEmptyStatement::toString () {
  return VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/;";
}



//**************************************************************************
//
// VAssertStatement
//
//**************************************************************************

//==========================================================================
//
//  VAssertStatement::VAssertStatement
//
//==========================================================================
VAssertStatement::VAssertStatement (const TLocation &ALoc, VExpression *AExpr, VExpression *AMsg)
  : VStatement(ALoc)
  , FatalInvoke(nullptr)
  , Expr(AExpr)
  , Message(AMsg)
{
}


//==========================================================================
//
//  VAssertStatement::~VAssertStatement
//
//==========================================================================
VAssertStatement::~VAssertStatement () {
  delete Expr; Expr = nullptr;
  delete Message; Message = nullptr;
  delete FatalInvoke; FatalInvoke = nullptr;
}


//==========================================================================
//
//  VAssertStatement::SyntaxCopy
//
//==========================================================================
VStatement *VAssertStatement::SyntaxCopy () {
  auto res = new VAssertStatement();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VAssertStatement::DoSyntaxCopyTo
//
//==========================================================================
void VAssertStatement::DoSyntaxCopyTo (VStatement *e) {
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VAssertStatement *)e;
  res->Expr = (Expr ? Expr->SyntaxCopy() : nullptr);
  res->Message = (Message ? Message->SyntaxCopy() : nullptr);
  res->FatalInvoke = (FatalInvoke ? FatalInvoke->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VAssertStatement::DoResolve
//
//==========================================================================
VStatement *VAssertStatement::DoResolve (VEmitContext &ec) {
  vassert(!FatalInvoke);

  bool wasError = false;

  // create message if necessary
  if (!Message) {
    VStr s = (Expr ? Expr->toString() : VStr("wtf"));
    int val = ec.Package->FindString(*s);
    Message = new VStringLiteral(s, val, Loc);
  }

  // find `AssertError()` method
  VMethod *M = ec.SelfClass->FindAccessibleMethod("AssertError", ec.SelfClass, &Loc);
  if (!M) {
    ParseError(Loc, "`AssertError()` method not found");
    return CreateInvalid();
  }

  // check method type: it should be static and final
  if ((M->Flags&(FUNC_Static|FUNC_VarArgs|FUNC_Final)) != (FUNC_Static|FUNC_Final)) {
    ParseError(Loc, "`AssertError()` method should be `static`");
    wasError = true;
  }

  // check method signature: it should return `void`, and have only one string argument
  if (M->ReturnType.Type != TYPE_Void || M->NumParams != 1 || M->ParamTypes[0].Type != TYPE_String) {
    ParseError(Loc, "`AssertError()` method has invalid signature");
    wasError = true;
  }

  // rewrite as invoke
  VExpression *args[1];
  args[0] = Message;
  FatalInvoke = new VInvocation(nullptr, M, nullptr, false/*no self*/, false/*not a base*/, Loc, 1, args);
  Message = nullptr; // it is owned by invoke now

  if (Expr) Expr = Expr->ResolveBoolean(ec);
  if (!Expr) wasError = true;

  if (FatalInvoke) FatalInvoke = FatalInvoke->Resolve(ec);
  if (!FatalInvoke) wasError = true;

  return (wasError ? CreateInvalid() : this);
}


//==========================================================================
//
//  VAssertStatement::DoDoEmit
//
//==========================================================================
void VAssertStatement::DoEmit (VEmitContext &ec) {
  if (!Expr || !FatalInvoke) return; // just in case

  VLabel skipError = ec.DefineLabel();
  // expression
  Expr->EmitBranchable(ec, skipError, true); // jump if true
  // check failed
  FatalInvoke->Emit(ec);
  // done
  ec.MarkLabel(skipError);
}


//==========================================================================
//
//  VAssertStatement::toString
//
//==========================================================================
VStr VAssertStatement::toString () {
  return VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/assert("+VExpression::e2s(Expr)+");";
}



//**************************************************************************
//
// VDeleteStatement
//
//**************************************************************************

//==========================================================================
//
//  VDeleteStatement::VDeleteStatement
//
//==========================================================================
VDeleteStatement::VDeleteStatement (VExpression *avar, const TLocation &aloc)
  : VStatement(aloc)
  , delexpr(nullptr)
  , assexpr(nullptr)
  , checkexpr(nullptr)
  , var(avar)
{
}


//==========================================================================
//
//  VDeleteStatement::~VDeleteStatement
//
//==========================================================================
VDeleteStatement::~VDeleteStatement () {
  delete delexpr; delexpr = nullptr;
  delete assexpr; assexpr = nullptr;
  delete checkexpr; checkexpr = nullptr;
  delete var; var = nullptr;
}


//==========================================================================
//
//  VDeleteStatement::SyntaxCopy
//
//==========================================================================
VStatement *VDeleteStatement::SyntaxCopy () {
  auto res = new VDeleteStatement();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDeleteStatement::DoSyntaxCopyTo
//
//==========================================================================
void VDeleteStatement::DoSyntaxCopyTo (VStatement *e) {
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VDeleteStatement *)e;
  res->var = (var ? var->SyntaxCopy() : nullptr);
  // no need to copy private fields
}


//==========================================================================
//
//  VDeleteStatement::DoResolve
//
//==========================================================================
VStatement *VDeleteStatement::DoResolve (VEmitContext &ec) {
  if (!var) return CreateInvalid();

  // build check expression
  checkexpr = var->SyntaxCopy()->ResolveBoolean(ec);
  if (!checkexpr) return CreateInvalid();

  // build delete expression
  delexpr = new VDotInvocation(var->SyntaxCopy(), VName("Destroy"), var->Loc, 0, nullptr);
  delexpr = new VDropResult(delexpr);
  delexpr = delexpr->Resolve(ec);
  if (!delexpr) return CreateInvalid();

  // build clear expression
  assexpr = new VAssignment(VAssignment::Assign, var->SyntaxCopy(), new VNoneLiteral(var->Loc), var->Loc);
  assexpr = new VDropResult(assexpr);
  assexpr = assexpr->Resolve(ec);
  if (!assexpr) return CreateInvalid();

  return this;
}


//==========================================================================
//
//  VDeleteStatement::DoEmit
//
//==========================================================================
void VDeleteStatement::DoEmit (VEmitContext &ec) {
  if (!checkexpr || !delexpr || !assexpr) return;

  // emit check
  VLabel skipLabel = ec.DefineLabel();
  checkexpr->EmitBranchable(ec, skipLabel, false);

  // emit delete and clear
  delexpr->Emit(ec);
  assexpr->Emit(ec);

  // done
  ec.MarkLabel(skipLabel);
}


//==========================================================================
//
//  VDeleteStatement::toString
//
//==========================================================================
VStr VDeleteStatement::toString () {
  return VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/delete "+VExpression::e2s(var)+";";
}



//**************************************************************************
//
// VExpressionStatement
//
//**************************************************************************

//==========================================================================
//
//  VExpressionStatement::VExpressionStatement
//
//==========================================================================
VExpressionStatement::VExpressionStatement (VExpression *AExpr)
  : VStatement(AExpr->Loc)
  , Expr(AExpr)
{
}


//==========================================================================
//
//  VExpressionStatement::~VExpressionStatement
//
//==========================================================================
VExpressionStatement::~VExpressionStatement () {
  delete Expr; Expr = nullptr;
}


//==========================================================================
//
//  VExpressionStatement::SyntaxCopy
//
//==========================================================================
VStatement *VExpressionStatement::SyntaxCopy () {
  auto res = new VExpressionStatement();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VExpressionStatement::DoSyntaxCopyTo
//
//==========================================================================
void VExpressionStatement::DoSyntaxCopyTo (VStatement *e) {
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VExpressionStatement *)e;
  res->Expr = (Expr ? Expr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VExpressionStatement::DoResolve
//
//==========================================================================
VStatement *VExpressionStatement::DoResolve (VEmitContext &ec) {
  if (Expr) Expr = Expr->Resolve(ec);
  return (Expr ? this : CreateInvalid());
}


//==========================================================================
//
//  VExpressionStatement::DoEmit
//
//==========================================================================
void VExpressionStatement::DoEmit (VEmitContext &ec) {
  Expr->Emit(ec);
}


//==========================================================================
//
//  VExpressionStatement::toString
//
//==========================================================================
VStr VExpressionStatement::toString () {
  return
    VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/ "+
    GET_OBJ_TYPE(Expr)+
    VExpression::e2s(Expr)+";";
}



//**************************************************************************
//
// VIf
//
//**************************************************************************

//==========================================================================
//
//  VIf::VIf
//
//==========================================================================
VIf::VIf (VExpression *AExpr, VStatement *ATrueStatement, const TLocation &ALoc, bool ADoIndentCheck)
  : VStatement(ALoc)
  , Expr(AExpr)
  , TrueStatement(ATrueStatement)
  , FalseStatement(nullptr)
  , ElseLoc(ALoc)
  , doIndentCheck(ADoIndentCheck)
{
}


//==========================================================================
//
//  VIf::VIf
//
//==========================================================================
VIf::VIf (VExpression *AExpr, VStatement *ATrueStatement, VStatement *AFalseStatement,
          const TLocation &ALoc, const TLocation &AElseLoc, bool ADoIndentCheck)
  : VStatement(ALoc)
  , Expr(AExpr)
  , TrueStatement(ATrueStatement)
  , FalseStatement(AFalseStatement)
  , ElseLoc(AElseLoc)
  , doIndentCheck(ADoIndentCheck)
{
}


//==========================================================================
//
//  VIf::~VIf
//
//==========================================================================
VIf::~VIf () {
  delete Expr; Expr = nullptr;
  delete TrueStatement; TrueStatement = nullptr;
  delete FalseStatement; FalseStatement = nullptr;
}


//==========================================================================
//
//  VIf::SyntaxCopy
//
//==========================================================================
VStatement *VIf::SyntaxCopy () {
  auto res = new VIf();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VIf::DoSyntaxCopyTo
//
//==========================================================================
void VIf::DoSyntaxCopyTo (VStatement *e) {
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VIf *)e;
  res->Expr = (Expr ? Expr->SyntaxCopy() : nullptr);
  res->TrueStatement = (TrueStatement ? TrueStatement->SyntaxCopy() : nullptr);
  res->FalseStatement = (FalseStatement ? FalseStatement->SyntaxCopy() : nullptr);
  res->ElseLoc = ElseLoc;
  res->doIndentCheck = doIndentCheck;
}


//==========================================================================
//
//  VIf::DoFixSwitch
//
//==========================================================================
void VIf::DoFixSwitch (VSwitch *aold, VSwitch *anew) {
  if (TrueStatement) TrueStatement->DoFixSwitch(aold, anew);
  if (FalseStatement) FalseStatement->DoFixSwitch(aold, anew);
}


//==========================================================================
//
//  VIf::DoResolve
//
//==========================================================================
VStatement *VIf::DoResolve (VEmitContext &ec) {
  bool wasError = false;

  if (doIndentCheck) {
    // indent check
    if (Expr && TrueStatement && !CheckCondIndent(Expr->Loc, TrueStatement)) wasError = true;
    if (Expr && FalseStatement && !CheckCondIndent(ElseLoc, FalseStatement)) wasError = true;
  }

  // resolve
  if (Expr) Expr = Expr->ResolveBoolean(ec);
  if (!Expr) wasError = true;

  TrueStatement = TrueStatement->Resolve(ec, this);
  if (!TrueStatement->IsValid()) wasError = true;

  if (FalseStatement) {
    FalseStatement = FalseStatement->Resolve(ec, this);
    if (!FalseStatement->IsValid()) wasError = true;
  }

  return (wasError ? CreateInvalid() : this);
}


//==========================================================================
//
//  VIf::DoEmit
//
//==========================================================================
void VIf::DoEmit (VEmitContext &ec) {
  if (!Expr) return; // just in case
  const int bval = Expr->IsBoolLiteral(ec);
  if (bval >= 0) {
    // known
    if (bval) {
      // only true branch
      TrueStatement->Emit(ec, this);
    } else if (FalseStatement) {
      // only false branch
      FalseStatement->Emit(ec, this);
    }
  } else {
    VLabel FalseTarget = ec.DefineLabel();
    // expression
    Expr->EmitBranchable(ec, FalseTarget, false);
    // true statement
    TrueStatement->Emit(ec, this);
    if (FalseStatement) {
      // false statement
      VLabel End = ec.DefineLabel();
      ec.AddStatement(OPC_Goto, End, Loc);
      ec.MarkLabel(FalseTarget);
      FalseStatement->Emit(ec, this);
      ec.MarkLabel(End);
    } else {
      ec.MarkLabel(FalseTarget);
    }
  }
}


//==========================================================================
//
//  VIf::IsEndsWithReturn
//
//==========================================================================
bool VIf::IsEndsWithReturn () const noexcept {
  if (!TrueStatement || !FalseStatement) return false;
  return (TrueStatement->IsEndsWithReturn() && FalseStatement->IsEndsWithReturn());
}


//==========================================================================
//
//  VIf::IsProperCaseEnd
//
//==========================================================================
bool VIf::IsProperCaseEnd (const VStatement *ASwitch) const noexcept {
  if (!TrueStatement || !FalseStatement) return false;
  return (TrueStatement->IsProperCaseEnd(ASwitch) && FalseStatement->IsProperCaseEnd(ASwitch));
}


//==========================================================================
//
//  VIf::toString
//
//==========================================================================
VStr VIf::toString () {
  return
    VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/if ("+VExpression::e2s(Expr)+")\n"+
    (TrueStatement ? TrueStatement->toString() : VStr("<none>;"))+
    (FalseStatement ? VStr("\nelse\n")+FalseStatement->toString() : VStr());
}



//**************************************************************************
//
// VWhile
//
//**************************************************************************

//==========================================================================
//
//  VWhile::VWhile
//
//==========================================================================
VWhile::VWhile (VExpression *AExpr, VStatement *AStatement, const TLocation &ALoc, VName aLabel)
  : VStatement(ALoc, aLabel)
  , Expr(AExpr)
{
  Statement = AStatement;
}


//==========================================================================
//
//  VWhile::~VWhile
//
//==========================================================================
VWhile::~VWhile () {
  delete Expr; Expr = nullptr;
}


//==========================================================================
//
//  VWhile::SyntaxCopy
//
//==========================================================================
VStatement *VWhile::SyntaxCopy () {
  auto res = new VWhile();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VWhile::DoSyntaxCopyTo
//
//==========================================================================
void VWhile::DoSyntaxCopyTo (VStatement *e) {
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VWhile *)e;
  res->Expr = (Expr ? Expr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VWhile::DoResolve
//
//==========================================================================
VStatement *VWhile::DoResolve (VEmitContext &ec) {
  bool wasError = false;

  // indent check
  if (Expr && Statement && !CheckCondIndent(Expr->Loc, Statement)) wasError = true;

  if (Expr) Expr = Expr->ResolveBoolean(ec);
  if (!Expr) wasError = true;
  Statement = Statement->Resolve(ec, this);
  if (!Statement->IsValid()) wasError = true;

  return (wasError ? CreateInvalid() : this);
}


//==========================================================================
//
//  VWhile::DoEmit
//
//==========================================================================
void VWhile::DoEmit (VEmitContext &ec) {
  if (!Expr) return; // just in case
  const int bval = Expr->IsBoolLiteral(ec);
  if (bval == 0) return; // loop is not taken
  ++ec.InLoop;
  // allocate labels
  breakLabel = ec.DefineLabel();
  contLabel = ec.DefineLabel();
  // continue point is here (because `continue` emits all necessary dtors)
  ec.MarkLabel(contLabel);
  // do not emit loop condition check if it is known `true`
  if (bval != 1) {
    // loop condition check
    Expr->EmitBranchable(ec, breakLabel, false);
  }
  // generate loop body
  Statement->Emit(ec, this);
  // jump to loop start
  ec.AddStatement(OPC_Goto, contLabel, Loc);
  // loop breaks here
  ec.MarkLabel(breakLabel);
  --ec.InLoop;
}


//==========================================================================
//
//  VWhile::IsBreakScope
//
//==========================================================================
bool VWhile::IsBreakScope () const noexcept {
  return true;
}


//==========================================================================
//
//  VWhile::IsContinueScope
//
//==========================================================================
bool VWhile::IsContinueScope () const noexcept {
  return true;
}


//==========================================================================
//
//  VWhile::IsInLoop
//
//==========================================================================
bool VWhile::IsInLoop () const noexcept {
  return true;
}


//==========================================================================
//
//  VWhile::toString
//
//==========================================================================
VStr VWhile::toString () {
  return
    VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/while ("+VExpression::e2s(Expr)+")\n"+
    (Statement ? Statement->toString() : VStr("<none>"));
}



//**************************************************************************
//
// VDo
//
//**************************************************************************

//==========================================================================
//
//  VDo::VDo
//
//==========================================================================
VDo::VDo (VExpression *AExpr, VStatement *AStatement, const TLocation &ALoc, VName aLabel)
  : VStatement(ALoc, aLabel)
  , Expr(AExpr)
{
  Statement = AStatement;
}


//==========================================================================
//
//  VDo::~VDo
//
//==========================================================================
VDo::~VDo () {
  delete Expr; Expr = nullptr;
}


//==========================================================================
//
//  VDo::SyntaxCopy
//
//==========================================================================
VStatement *VDo::SyntaxCopy () {
  auto res = new VDo();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDo::DoSyntaxCopyTo
//
//==========================================================================
void VDo::DoSyntaxCopyTo (VStatement *e) {
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VDo *)e;
  res->Expr = (Expr ? Expr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDo::DoResolve
//
//==========================================================================
VStatement *VDo::DoResolve (VEmitContext &ec) {
  bool wasError = false;

  // indent check
  if (Expr && Statement && !CheckCondIndent(Expr->Loc, Statement)) wasError = true;

  if (Expr) Expr = Expr->ResolveBoolean(ec);
  if (!Expr) wasError = true;
  Statement = Statement->Resolve(ec, this);
  if (!Statement->IsValid()) wasError = true;

  return (wasError ? CreateInvalid() : this);
}


//==========================================================================
//
//  VDo::DoEmit
//
//==========================================================================
void VDo::DoEmit (VEmitContext &ec) {
  if (!Expr) return; // just in case
  const int bval = Expr->IsBoolLiteral(ec);
  ++ec.InLoop;
  // allocate labels
  breakLabel = ec.DefineLabel();
  contLabel = ec.DefineLabel();
  // generate loop body
  VLabel loopStart = ec.DefineLabel();
  ec.MarkLabel(loopStart);
  // if loop is endless, generate continue point here
  if (bval == 1) ec.MarkLabel(contLabel);
  // emit loop body
  Statement->Emit(ec, this);
  // continue point is here (because `continue` emits all necessary dtors)
  // but only if loop is not endless
  if (bval != 1) ec.MarkLabel(contLabel);
  if (bval < 0) {
    // emit loop condition branch
    Expr->EmitBranchable(ec, loopStart, true);
  } else if (bval == 1) {
    // emit unconditional jump
    ec.AddStatement(OPC_Goto, loopStart, Loc);
  } else {
    vassert(bval == 0);
    // loop condition is false, no need to emit any branch here
  }
  // loop breaks here
  ec.MarkLabel(breakLabel);
  --ec.InLoop;
}


//==========================================================================
//
//  VDo::IsBreakScope
//
//==========================================================================
bool VDo::IsBreakScope () const noexcept {
  return true;
}


//==========================================================================
//
//  VDo::IsContinueScope
//
//==========================================================================
bool VDo::IsContinueScope () const noexcept {
  return true;
}


//==========================================================================
//
//  VDo::IsInLoop
//
//==========================================================================
bool VDo::IsInLoop () const noexcept {
  return true;
}


//==========================================================================
//
//  VDo::toString
//
//==========================================================================
VStr VDo::toString () {
  return
    VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/do\n"+
    (Statement ? Statement->toString() : VStr("<none>"))+"\nwhile ("+VExpression::e2s(Expr)+");";
}



//**************************************************************************
//
// VFor
//
//**************************************************************************

//==========================================================================
//
//  VFor::VFor
//
//==========================================================================
VFor::VFor (const TLocation &ALoc, VName aLabel)
  : VStatement(ALoc, aLabel)
  , CondExpr()
  , LoopExpr()
{
  Statement = nullptr;
}


//==========================================================================
//
//  VFor::~VFor
//
//==========================================================================
VFor::~VFor () {
  //for (int i = 0; i < InitExpr.length(); ++i) { delete InitExpr[i]; InitExpr[i] = nullptr; }
  for (int i = 0; i < CondExpr.length(); ++i) { delete CondExpr[i]; CondExpr[i] = nullptr; }
  for (int i = 0; i < LoopExpr.length(); ++i) { delete LoopExpr[i]; LoopExpr[i] = nullptr; }
}


//==========================================================================
//
//  VFor::SyntaxCopy
//
//==========================================================================
VStatement *VFor::SyntaxCopy () {
  auto res = new VFor();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VFor::DoSyntaxCopyTo
//
//==========================================================================
void VFor::DoSyntaxCopyTo (VStatement *e) {
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VFor *)e;
  //res->InitExpr.setLength(InitExpr.length());
  //for (int f = 0; f < InitExpr.length(); ++f) res->InitExpr[f] = (InitExpr[f] ? InitExpr[f]->SyntaxCopy() : nullptr);
  res->CondExpr.setLength(CondExpr.length());
  for (int f = 0; f < CondExpr.length(); ++f) res->CondExpr[f] = (CondExpr[f] ? CondExpr[f]->SyntaxCopy() : nullptr);
  res->LoopExpr.setLength(LoopExpr.length());
  for (int f = 0; f < LoopExpr.length(); ++f) res->LoopExpr[f] = (LoopExpr[f] ? LoopExpr[f]->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VFor::DoResolve
//
//==========================================================================
VStatement *VFor::DoResolve (VEmitContext &ec) {
  bool wasError = false;

  // indent check
  if (Statement && !CheckCondIndent(Loc, Statement)) wasError = true;

  for (int i = 0; i < CondExpr.length(); ++i) {
    VExpression *ce = CondExpr[i];
    if (i != CondExpr.length()-1) {
      ce = ce->Resolve(ec);
    } else {
      ce = ce->ResolveBoolean(ec);
    }
    CondExpr[i] = ce;
    if (!ce) wasError = true;
  }

  for (auto &&e : LoopExpr) {
    e = e->Resolve(ec);
    if (!e) wasError = true;
  }

  Statement = Statement->Resolve(ec, this);
  if (!Statement->IsValid()) wasError = true;

  return (wasError ? CreateInvalid() : this);
}


//==========================================================================
//
//  VFor::DoEmit
//
//==========================================================================
void VFor::DoEmit (VEmitContext &ec) {
  const int bval = (CondExpr.length() == 0 ? 1 : CondExpr[CondExpr.length()-1]->IsBoolLiteral(ec));

  // allocate labels
  breakLabel = ec.DefineLabel();
  contLabel = ec.DefineLabel();

  // emit initialisation expressions
  //for (auto &&e : InitExpr) e->Emit(ec);

  VLabel testLbl = ec.DefineLabel();

  // emit loop test (this is also where it jumps after loop expressions
  ec.MarkLabel(testLbl);

  // loop test expressions
  for (int i = 0; i < CondExpr.length()-1; ++i) CondExpr[i]->Emit(ec);
       if (bval < 0) CondExpr[CondExpr.length()-1]->EmitBranchable(ec, breakLabel, false);
  else if (bval == 0) ec.AddStatement(OPC_Goto, breakLabel, Loc);

  // emit embeded statement
  Statement->Emit(ec, this);

  // put continue point here
  ec.MarkLabel(contLabel);

  // emit per-loop expression statements
  for (auto &&e : LoopExpr) e->Emit(ec);

  // jump to loop test
  ec.AddStatement(OPC_Goto, testLbl, Loc);

  // loop breaks here
  ec.MarkLabel(breakLabel);
}


//==========================================================================
//
//  VFor::IsFor
//
//==========================================================================
bool VFor::IsFor () const noexcept {
  return true;
}


//==========================================================================
//
//  VFor::IsBreakScope
//
//==========================================================================
bool VFor::IsBreakScope () const noexcept {
  return true;
}


//==========================================================================
//
//  VFor::IsContinueScope
//
//==========================================================================
bool VFor::IsContinueScope () const noexcept {
  return true;
}


//==========================================================================
//
//  VFor::IsInLoop
//
//==========================================================================
bool VFor::IsInLoop () const noexcept {
  return true;
}


//==========================================================================
//
//  VFor::toString
//
//==========================================================================
VStr VFor::toString () {
  VStr res = VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/for (;";
  //for (auto &&e : InitExpr) { res += VExpression::e2s(e); res += ","; }
  //res += "; ";
  for (auto &&e : CondExpr) { res += VExpression::e2s(e); res += ","; }
  res += "; ";
  for (auto &&e : LoopExpr) { res += VExpression::e2s(e); res += ","; }
  res += ")\n";
  res += (Statement ? Statement->toString() : VStr("<none>"));
  return res;
}



//**************************************************************************
//
// VForeach
//
//**************************************************************************

//==========================================================================
//
//  VForeach::VForeach
//
//==========================================================================
VForeach::VForeach (VExpression *AExpr, VStatement *AStatement, const TLocation &ALoc, VName aLabel)
  : VStatement(ALoc, aLabel)
  , Expr(AExpr)
{
  Statement = AStatement;
}


//==========================================================================
//
//  VForeach::~VForeach
//
//==========================================================================
VForeach::~VForeach () {
  delete Expr; Expr = nullptr;
}


//==========================================================================
//
//  VForeach::SyntaxCopy
//
//==========================================================================
VStatement *VForeach::SyntaxCopy () {
  auto res = new VForeach();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VForeach::DoSyntaxCopyTo
//
//==========================================================================
void VForeach::DoSyntaxCopyTo (VStatement *e) {
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VForeach *)e;
  res->Expr = (Expr ? Expr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VForeach::DoResolve
//
//==========================================================================
VStatement *VForeach::DoResolve (VEmitContext &ec) {
  bool wasError = false;

  // indent check
  if (Statement && !CheckCondIndent(Loc, Statement)) wasError = true;

  if (Expr) Expr = Expr->ResolveIterator(ec);
  if (!Expr) wasError = true;
  Statement = Statement->Resolve(ec, this);
  if (!Statement->IsValid()) wasError = true;

  return (wasError ? CreateInvalid() : this);
}


//==========================================================================
//
//  VForeach::DoEmit
//
//==========================================================================
void VForeach::DoEmit (VEmitContext &ec) {
  Expr->Emit(ec);
  ec.AddStatement(OPC_IteratorInit, Loc);

  // allocate labels
  breakLabel = ec.DefineLabel();
  contLabel = ec.DefineLabel();

  VLabel loopLbl = ec.DefineLabel();

  // jump to "next"
  ec.AddStatement(OPC_Goto, contLabel, Loc);

  // loop body
  ec.MarkLabel(loopLbl);
  Statement->Emit(ec, this);

  // put continue point here
  ec.MarkLabel(contLabel);

  ec.AddStatement(OPC_IteratorNext, Loc);
  ec.AddStatement(OPC_IfGoto, loopLbl, Loc);

  // loop breaks here
  ec.MarkLabel(breakLabel);
}


//==========================================================================
//
//  VForeach::IsBreakScope
//
//==========================================================================
bool VForeach::IsBreakScope () const noexcept {
  return true;
}


//==========================================================================
//
//  VForeach::IsContinueScope
//
//==========================================================================
bool VForeach::IsContinueScope () const noexcept {
  return true;
}


//==========================================================================
//
//  VForeach::IsInLoop
//
//==========================================================================
bool VForeach::IsInLoop () const noexcept {
  return true;
}


//==========================================================================
//
//  VForeach::EmitFinalizer
//
//==========================================================================
void VForeach::EmitFinalizer (VEmitContext &ec, bool properLeave) {
  ec.AddStatement(OPC_IteratorPop, Loc);
}


//==========================================================================
//
//  VForeach::toString
//
//==========================================================================
VStr VForeach::toString () {
  return
    VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/foreach "+VExpression::e2s(Expr)+"\n"+
    (Statement ? Statement->toString() : VStr("<none>"));
}



//**************************************************************************
//
// VLoopStatementWithTempLocals
//
//**************************************************************************

//==========================================================================
//
//  VLoopStatementWithTempLocals::VLoopStatementWithTempLocals
//
//==========================================================================
VLoopStatementWithTempLocals::VLoopStatementWithTempLocals (const TLocation &aloc, VName aLabel)
  : VStatement(aloc, aLabel)
  , tempLocals()
{
}


//==========================================================================
//
//  VLoopStatementWithTempLocals::~VLoopStatementWithTempLocals
//
//==========================================================================
VLoopStatementWithTempLocals::~VLoopStatementWithTempLocals () {
  tempLocals.clear();
}


//==========================================================================
//
//  VLoopStatementWithTempLocals::EmitCtor
//
//==========================================================================
void VLoopStatementWithTempLocals::EmitCtor (VEmitContext &ec) {
  // no need to clear uninited vars, the loop will take care of it
  for (auto &&lv : tempLocals) {
    ec.AllocateLocalSlot(lv);
    VLocalVarDef &loc = ec.GetLocalByIndex(lv);
    if (loc.reused && loc.Type.NeedZeroingOnSlotReuse()) ec.EmitLocalZero(lv, Loc);
  }
}


//==========================================================================
//
//  VLoopStatementWithTempLocals::EmitDtor
//
//==========================================================================
void VLoopStatementWithTempLocals::EmitDtor (VEmitContext &ec, bool properLeave) {
  for (auto &&lv : tempLocals.reverse()) {
    ec.EmitLocalDtor(lv, Loc);
  }
  // if leaving properly, release locals
  if (properLeave) {
    for (auto &&lv : tempLocals) ec.ReleaseLocalSlot(lv);
  }
}



//**************************************************************************
//
// VForeachIota
//
//**************************************************************************

//==========================================================================
//
//  VForeachIota::VForeachIota
//
//==========================================================================
VForeachIota::VForeachIota (const TLocation &ALoc, VName aLabel)
  : VLoopStatementWithTempLocals(ALoc, aLabel)
  , varinit(nullptr)
  , varnext(nullptr)
  , hiinit(nullptr)
  , var(nullptr)
  , lo(nullptr)
  , hi(nullptr)
  , reversed(false)
{
  Statement = nullptr;
}


//==========================================================================
//
//  VForeachIota::~VForeachIota
//
//==========================================================================
VForeachIota::~VForeachIota () {
  tempLocals.clear();
  delete varinit; varinit = nullptr;
  delete varnext; varnext = nullptr;
  delete hiinit; hiinit = nullptr;
  delete var; var = nullptr;
  delete lo; lo = nullptr;
  delete hi; hi = nullptr;
}


//==========================================================================
//
//  VForeachIota::SyntaxCopy
//
//==========================================================================
VStatement *VForeachIota::SyntaxCopy () {
  if (varinit || varnext || hiinit) VCFatalError("VC: `VForeachIota::SyntaxCopy()` called on resolved statement");
  auto res = new VForeachIota();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VForeachIota::DoSyntaxCopyTo
//
//==========================================================================
void VForeachIota::DoSyntaxCopyTo (VStatement *e) {
  VLoopStatementWithTempLocals::DoSyntaxCopyTo(e);
  auto res = (VForeachIota *)e;
  res->var = (var ? var->SyntaxCopy() : nullptr);
  res->lo = (lo ? lo->SyntaxCopy() : nullptr);
  res->hi = (hi ? hi->SyntaxCopy() : nullptr);
  res->reversed = reversed;
  // no need to copy private data here, as `SyntaxCopy()` should be called only on unresolved things
}


//==========================================================================
//
//  VForeachIota::DoResolve
//
//==========================================================================
VStatement *VForeachIota::DoResolve (VEmitContext &ec) {
  bool wasError = false;

  // indent check
  if (Statement && !CheckCondIndent(Loc, Statement)) wasError = true;

  // we will rewrite 'em later
  auto varR = (var ? var->SyntaxCopy()->Resolve(ec) : nullptr);
  auto loR = (lo ? lo->SyntaxCopy()->Resolve(ec) : nullptr);
  auto hiR = (hi ? hi->SyntaxCopy()->Resolve(ec) : nullptr);
  if (!Statement || !varR || !loR || !hiR) {
    wasError = true;
  } else {
    if (varR->Type.Type != TYPE_Int) {
      ParseError(var->Loc, "Loop variable should be integer (got `%s`)", *varR->Type.GetName());
      wasError = true;
    }

    if (loR->Type.Type != TYPE_Int) {
      ParseError(lo->Loc, "Loop lower bound should be integer (got `%s`)", *loR->Type.GetName());
      wasError = true;
    }

    if (hiR->Type.Type != TYPE_Int) {
      ParseError(hi->Loc, "Loop higher bound should be integer (got `%s`)", *hiR->Type.GetName());
      wasError = true;
    }
  }

  if (wasError) {
    delete varR;
    delete loR;
    delete hiR;
    return CreateInvalid();
  }

  // create hidden local for higher bound (if necessary)
  VExpression *limit;
  if ((reversed ? loR : hiR)->IsIntConst()) {
    limit = new VIntLiteral((reversed ? loR : hiR)->GetIntConst(), (reversed ? lo : hi)->Loc);
  } else {
    VLocalVarDef &L = ec.NewLocal(NAME_None, VFieldType(TYPE_Int), (reversed ? lo : hi)->Loc);
    L.Visible = false; // it is unnamed, and hidden ;-)
    tempLocals.append(L.GetIndex());
    limit = new VLocalVar(L.GetIndex(), L.Loc);
    // initialize hidden local with higher/lower bound
    hiinit = new VAssignment(VAssignment::Assign, limit->SyntaxCopy(), (reversed ? lo : hi)->SyntaxCopy(), L.Loc);
  }

  // we don't need 'em anymore
  delete varR;
  delete loR;
  delete hiR;

  if (hiinit) {
    hiinit = hiinit->Resolve(ec);
    if (!hiinit) {
      delete limit;
      return CreateInvalid();
    }
    //GLog.Logf(NAME_Debug, "%s: hiinit=%s; tempLocals.length=%d", *Loc.toStringNoCol(), *hiinit->toString(), tempLocals.length());
  }

  if (!reversed) {
    // normal
    // create initializer expression: `var = lo`
    varinit = new VAssignment(VAssignment::Assign, var->SyntaxCopy(), lo->SyntaxCopy(), hi->Loc);

    // create loop/check expression: `++var < limit`
    varnext = new VUnaryMutator(VUnaryMutator::PreInc, var->SyntaxCopy(), hi->Loc);
    varnext = new VBinary(VBinary::EBinOp::Less, varnext, limit->SyntaxCopy(), hi->Loc);

    // create condition expression: `var < limit`
    var = new VBinary(VBinary::EBinOp::Less, var, limit->SyntaxCopy(), hi->Loc);
  } else {
    // reversed
    // create initializer expression: `var = hi-1`
    VExpression *vminus = new VBinary(VBinary::EBinOp::Subtract, hi->SyntaxCopy(), new VIntLiteral(1, hi->Loc), hi->Loc);
    varinit = new VAssignment(VAssignment::Assign, var->SyntaxCopy(), vminus, hi->Loc);

    // create loop/check expression: `var-- > limit`
    varnext = new VUnaryMutator(VUnaryMutator::PostDec, var->SyntaxCopy(), hi->Loc);
    varnext = new VBinary(VBinary::EBinOp::Greater, varnext, limit->SyntaxCopy(), hi->Loc);

    // create condition expression: `var >= limit`
    var = new VBinary(VBinary::EBinOp::GreaterEquals, var, limit->SyntaxCopy(), hi->Loc);
  }

  delete limit;

  varinit = varinit->Resolve(ec);
  if (!varinit) wasError = true;

  varnext = varnext->ResolveBoolean(ec);
  if (!varnext) wasError = true;

  var = var->ResolveBoolean(ec);
  if (!var) wasError = true;

  // finally, resolve statement
  Statement = Statement->Resolve(ec, this);
  if (!Statement->IsValid()) wasError = true;

  return (wasError ? CreateInvalid() : this);
}


//==========================================================================
//
//  VForeachIota::DoEmit
//
//==========================================================================
void VForeachIota::DoEmit (VEmitContext &ec) {
  // allocate labels
  breakLabel = ec.DefineLabel();
  contLabel = ec.DefineLabel();

  VLabel loopLbl = ec.DefineLabel();

  // emit initialisation expressions
  if (hiinit) hiinit->Emit(ec); // may be absent for iota with literals
  varinit->Emit(ec);

  // do first check
  var->EmitBranchable(ec, breakLabel, false);

  // emit embeded statement
  ec.MarkLabel(loopLbl);

  // emit loop body
  Statement->Emit(ec, this);

  // put continue point here
  ec.MarkLabel(contLabel);

  // loop next and test
  varnext->EmitBranchable(ec, loopLbl, true);

  // loop breaks here
  ec.MarkLabel(breakLabel);
}


//==========================================================================
//
//  VForeachIota::IsBreakScope
//
//==========================================================================
bool VForeachIota::IsBreakScope () const noexcept {
  return true;
}


//==========================================================================
//
//  VForeachIota::IsContinueScope
//
//==========================================================================
bool VForeachIota::IsContinueScope () const noexcept {
  return true;
}


//==========================================================================
//
//  VForeachIota::IsInLoop
//
//==========================================================================
bool VForeachIota::IsInLoop () const noexcept {
  return true;
}


//==========================================================================
//
//  VForeachIota::toString
//
//==========================================================================
VStr VForeachIota::toString () {
  return
    VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/foreach ("+
    VExpression::e2s(var)+"; "+
    VExpression::e2s(lo)+" .. "+
    VExpression::e2s(hi)+(reversed ? "; reversed)\n" : ")\n")+
    (Statement ? Statement->toString() : VStr("<none>"));
}



//**************************************************************************
//
// VForeachArray
//
//**************************************************************************

//==========================================================================
//
//  VForeachArray::VForeachArray
//
//==========================================================================
VForeachArray::VForeachArray (VExpression *aarr, VExpression *aidx, VExpression *avar, bool aVarRef, bool aVarConst, const TLocation &aloc, VName aLabel)
  : VLoopStatementWithTempLocals(aloc, aLabel)
  , idxinit(nullptr)
  , hiinit(nullptr)
  , loopPreCheck(nullptr)
  , loopNext(nullptr)
  , loopLoad(nullptr)
  , varaddr(nullptr)
  , idxvar(aidx)
  , var(avar)
  , arr(aarr)
  , reversed(false)
  , isRef(aVarRef)
  , isConst(aVarConst)
{
  Statement = nullptr;
}


//==========================================================================
//
//  VForeachArray::~VForeachArray
//
//==========================================================================
VForeachArray::~VForeachArray () {
  tempLocals.clear();
  delete idxinit; idxinit = nullptr;
  delete hiinit; hiinit = nullptr;
  delete loopPreCheck; loopPreCheck = nullptr;
  delete loopNext; loopNext = nullptr;
  delete loopLoad; loopLoad = nullptr;
  delete varaddr; varaddr = nullptr;
  delete idxvar; idxvar = nullptr;
  delete var; var = nullptr;
  delete arr; arr = nullptr;
}


//==========================================================================
//
//  VForeachArray::SyntaxCopy
//
//==========================================================================
VStatement *VForeachArray::SyntaxCopy () {
  //if (varinit || varnext || hiinit) VCFatalError("VC: `VForeachArray::SyntaxCopy()` called on resolved statement");
  auto res = new VForeachArray();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VForeachArray::DoSyntaxCopyTo
//
//==========================================================================
void VForeachArray::DoSyntaxCopyTo (VStatement *e) {
  VLoopStatementWithTempLocals::DoSyntaxCopyTo(e);
  auto res = (VForeachArray *)e;
  res->idxvar = (idxvar ? idxvar->SyntaxCopy() : nullptr);
  res->var = (var ? var->SyntaxCopy() : nullptr);
  res->arr = (arr ? arr->SyntaxCopy() : nullptr);
  res->reversed = reversed;
  res->isRef = isRef;
  res->isConst = isConst;
  // no need to copy private data here, as `SyntaxCopy()` should be called only on unresolved things
}


//==========================================================================
//
//  VForeachArray::DoResolve
//
//==========================================================================
VStatement *VForeachArray::DoResolve (VEmitContext &ec) {
  bool wasError = false;

  // indent check
  if (Statement && !CheckCondIndent(Loc, Statement)) wasError = true;

  if (arr && arr->IsAnyInvocation()) VCFatalError("VC: Internal compiler error (VForeachArray::Resolve)");

  // we will rewrite 'em later
  auto ivarR = (idxvar ? idxvar->SyntaxCopy()->Resolve(ec) : nullptr);
  auto varR = (var ? var->SyntaxCopy()->Resolve(ec) : nullptr);
  auto arrR = (arr ? arr->SyntaxCopy()->Resolve(ec) : nullptr);

  if (!Statement || !varR || !arrR || (idxvar && !ivarR)) wasError = true;

  if (ivarR && ivarR->Type.Type != TYPE_Int) {
    ParseError(var->Loc, "Loop variable should be integer (got `%s`)", *ivarR->Type.GetName());
    wasError = true;
  }

  if (arrR && !arrR->Type.IsAnyIndexableArray()) {
    ParseError(var->Loc, "Array variable should be integer (got `%s`)", *varR->Type.GetName());
    wasError = true;
  }

  if (!wasError) {
    if (isRef) {
      wasError = !varR->Type.MakePointerType().CheckMatch(false, Loc, arrR->Type.GetArrayInnerType().MakePointerType());
    } else {
      wasError = !varR->Type.CheckMatch(false, Loc, arrR->Type.GetArrayInnerType());
    }
  }

  // generate faster code for static arrays
  bool isStaticArray = (arrR ? arrR->Type.Type == TYPE_Array : false);
  int staticLen = (isStaticArray ? arrR->Type.GetArrayDim() : 0);

  // find local for ref
  int indLocalVal = -1;
  if (isRef && !wasError) {
    if (!varR->IsLocalVarExpr()) {
      ParseError(var->Loc, "VC: something is very wrong with the compiler (VForeachArray::Resolve)");
      wasError = true;
    } else {
      indLocalVal = ((VLocalVar *)varR)->num;
    }
  }

  // we don't need 'em anymore
  delete ivarR;
  delete varR;
  delete arrR;
  if (wasError) return CreateInvalid();

  /* this will compile to:
   *   ivar = 0;
   *   limit = arr.length;
   *   if (ivar >= limit) goto end;
   *  loop:
   *   var = arr[limit];
   *   body
   *   if (++ivar < limit) goto loop;
   */

  // create hidden local for index (if necessary)
  VExpression *index;
  if (idxvar) {
    index = idxvar->SyntaxCopy();
  } else {
    VLocalVarDef &L = ec.NewLocal(NAME_None, VFieldType(TYPE_Int), Loc);
    L.Visible = false; // it is unnamed, and hidden ;-)
    tempLocals.append(L.GetIndex());
    index = new VLocalVar(L.GetIndex(), L.Loc);
  }
  // initialize index
  if (!reversed) {
    // normal: 0
    idxinit = new VAssignment(VAssignment::Assign, index->SyntaxCopy(), new VIntLiteral(0, index->Loc), index->Loc);
  } else {
    // reversed: $-1
    if (isStaticArray) {
      // for static arrays we know the limit for sure
      idxinit = new VAssignment(VAssignment::Assign, index->SyntaxCopy(), new VIntLiteral(staticLen-1, index->Loc), index->Loc);
    } else {
      VExpression *len = new VDotField(arr->SyntaxCopy(), VName("length"), index->Loc);
      len = new VBinary(VBinary::EBinOp::Subtract, len, new VIntLiteral(1, index->Loc), len->Loc);
      idxinit = new VAssignment(VAssignment::Assign, index->SyntaxCopy(), len, index->Loc);
    }
  }

  // create hidden local for higher bound, and initialize it (for reverse, just use 0)
  VExpression *limit;
  if (!reversed) {
    // normal
    if (isStaticArray) {
      // for static arrays we know the limit for sure
      limit = new VIntLiteral(staticLen, arr->Loc);
    } else {
      VLocalVarDef &L = ec.NewLocal(NAME_None, VFieldType(TYPE_Int), arr->Loc);
      L.Visible = false; // it is unnamed, and hidden ;-)
      tempLocals.append(L.GetIndex());
      limit = new VLocalVar(L.GetIndex(), L.Loc);
      // initialize hidden local with array length
      VExpression *len = new VDotField(arr->SyntaxCopy(), VName("length"), arr->Loc);
      hiinit = new VAssignment(VAssignment::Assign, limit->SyntaxCopy(), len, len->Loc);
    }
  } else {
    limit = new VIntLiteral(0, arr->Loc);
  }

  if (!reversed) {
    // normal
    // create condition expression: `index < limit`
    loopPreCheck = new VBinary(VBinary::EBinOp::Less, index->SyntaxCopy(), limit->SyntaxCopy(), Loc);

    // create loop/check expression: `++index < limit`
    loopNext = new VUnaryMutator(VUnaryMutator::PreInc, index->SyntaxCopy(), Loc);
    loopNext = new VBinary(VBinary::EBinOp::Less, loopNext, limit->SyntaxCopy(), loopNext->Loc);
  } else {
    // reversed
    // create condition expression: `index >= limit`
    loopPreCheck = new VBinary(VBinary::EBinOp::GreaterEquals, index->SyntaxCopy(), limit->SyntaxCopy(), Loc);

    // create loop/check expression: `index-- > limit`
    loopNext = new VUnaryMutator(VUnaryMutator::PostDec, index->SyntaxCopy(), Loc);
    loopNext = new VBinary(VBinary::EBinOp::Greater, loopNext, limit->SyntaxCopy(), loopNext->Loc);
  }

  // we don't need limit anymore
  delete limit;

  loopLoad = new VArrayElement(arr->SyntaxCopy(), index->SyntaxCopy(), Loc, true); // we can skip bounds checking here
  loopLoad->ResetReadOnly();
  // refvar code will be completed in our codegen
  if (isRef) {
    vassert(indLocalVal >= 0);
    varaddr = new VLocalVar(indLocalVal, loopLoad->Loc);
    varaddr = varaddr->Resolve(ec); // should not fail (i hope)
    if (varaddr) {
      varaddr->RequestAddressOf(); // get rid of `ref`
      varaddr->RequestAddressOf(); // and request a real address
    }
    // work around r/o fields
    //loopLoad = new VUnary(VUnary::TakeAddress, loopLoad, loopLoad->Loc);
    loopLoad = loopLoad->Resolve(ec);
    if (loopLoad) {
      loopLoad->RequestAddressOf();
      if (isConst || arr->IsReadOnly()) {
        loopLoad->SetReadOnly();
        if (varaddr) varaddr->SetReadOnly();
      }
    }
  } else {
    auto oldvflags = var->Flags;
    var->ResetReadOnly();
    loopLoad = new VAssignment(VAssignment::Assign, var->SyntaxCopy(), loopLoad, loopLoad->Loc);
    if (loopLoad) {
      loopLoad = new VDropResult(loopLoad);
      loopLoad = loopLoad->Resolve(ec); // should not fail (i hope)
    }
    if (var) var->Flags = oldvflags;
  }
  if (isConst && loopLoad) loopLoad->SetReadOnly();

  // we don't need index anymore
  delete index;

  idxinit = idxinit->Resolve(ec);
  if (!idxinit) wasError = true;

  if (hiinit) {
    hiinit = hiinit->Resolve(ec);
    if (!hiinit) wasError = true;
  }

  loopPreCheck = loopPreCheck->ResolveBoolean(ec);
  if (!loopPreCheck) wasError = true;

  loopNext = loopNext->ResolveBoolean(ec);
  if (!loopNext) wasError = true;

  if (!loopLoad) wasError = true;

  // finally, resolve statement (last, so local reusing will work as expected)
  Statement = Statement->Resolve(ec, this);
  if (!Statement->IsValid()) wasError = true;

  return (wasError ? CreateInvalid() : this);
}


//==========================================================================
//
//  VForeachArray::DoEmit
//
//==========================================================================
void VForeachArray::DoEmit (VEmitContext &ec) {
  // allocate labels
  breakLabel = ec.DefineLabel();
  contLabel = ec.DefineLabel();

  VLabel loopLbl = ec.DefineLabel();

  // emit initialisation expressions
  if (hiinit) hiinit->Emit(ec); // may be absent for reverse loops
  idxinit->Emit(ec);

  // do first check
  loopPreCheck->EmitBranchable(ec, breakLabel, false);

  // actual loop
  ec.MarkLabel(loopLbl);

  // load value
  if (isRef) varaddr->Emit(ec);
  loopLoad->Emit(ec);
  if (isRef) ec.AddStatement(OPC_AssignPtrDrop, Loc);

  // emit loop body
  Statement->Emit(ec, this);

  // put continue point here
  ec.MarkLabel(contLabel);

  // loop next and test
  loopNext->EmitBranchable(ec, loopLbl, true);

  // loop breaks here
  ec.MarkLabel(breakLabel);
}


//==========================================================================
//
//  VForeachArray::IsBreakScope
//
//==========================================================================
bool VForeachArray::IsBreakScope () const noexcept {
  return true;
}


//==========================================================================
//
//  VForeachArray::IsContinueScope
//
//==========================================================================
bool VForeachArray::IsContinueScope () const noexcept {
  return true;
}


//==========================================================================
//
//  VForeachArray::IsInLoop
//
//==========================================================================
bool VForeachArray::IsInLoop () const noexcept {
  return true;
}


//==========================================================================
//
//  VForeachArray::toString
//
//==========================================================================
VStr VForeachArray::toString () {
  return
    VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/foreach ("+
    (isConst ? "const " : "")+
    (isRef ? "ref " : "")+
    VExpression::e2s(idxvar)+", "+
    VExpression::e2s(var)+"; "+
    VExpression::e2s(arr)+(reversed ? "; reversed)\n" : ")\n")+
    (Statement ? Statement->toString() : VStr("<none>"));
}



//**************************************************************************
//
// VForeachScriptedOuter
//
//**************************************************************************

//==========================================================================
//
//  VForeachScriptedOuter::VForeachScriptedOuter
//
//==========================================================================
VForeachScriptedOuter::VForeachScriptedOuter (bool aisBoolInit, VExpression *aivInit, int ainitLocIdx, VStatement *aBody, const TLocation &aloc, VName aLabel)
  : VStatement(aloc, aLabel)
  , isBoolInit(aisBoolInit)
  , ivInit(aivInit)
  , initLocIdx(ainitLocIdx)
{
  vassert(aBody);
  Statement = aBody;
}


//==========================================================================
//
//  VForeachScriptedOuter::~VForeachScriptedOuter
//
//==========================================================================
VForeachScriptedOuter::~VForeachScriptedOuter () {
  delete ivInit; ivInit = nullptr;
}


//==========================================================================
//
//  VForeachScriptedOuter::SyntaxCopy
//
//==========================================================================
VStatement *VForeachScriptedOuter::SyntaxCopy () {
  auto res = new VForeachScriptedOuter();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VForeachScriptedOuter::DoSyntaxCopyTo
//
//==========================================================================
void VForeachScriptedOuter::DoSyntaxCopyTo (VStatement *e) {
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VForeachScriptedOuter *)e;
  res->isBoolInit = isBoolInit;
  res->ivInit = (ivInit ? ivInit->SyntaxCopy() : nullptr);
  res->initLocIdx = initLocIdx;
}


//==========================================================================
//
//  VForeachScriptedOuter::DoResolve
//
//==========================================================================
VStatement *VForeachScriptedOuter::DoResolve (VEmitContext &ec) {
  VCFatalError("VForeachScriptedOuter::DoResolve: it should never be called!");
  return nullptr;
}


//==========================================================================
//
//  VForeachScriptedOuter::DoEmit
//
//==========================================================================
void VForeachScriptedOuter::DoEmit (VEmitContext &ec) {
  // emit initialisation expression
  if (isBoolInit) {
    VLabel LoopExitSkipDtor = ec.DefineLabel();
    ivInit->EmitBranchable(ec, LoopExitSkipDtor, false);
    if (Statement) Statement->Emit(ec, this);
    // jump point for "loop not taken"
    ec.MarkLabel(LoopExitSkipDtor);
  } else {
    ivInit->Emit(ec);
    if (Statement) Statement->Emit(ec, this);
  }
}


//==========================================================================
//
//  VForeachScriptedOuter::EmitDtor
//
//==========================================================================
void VForeachScriptedOuter::EmitDtor (VEmitContext &ec, bool properLeave) {
  if (initLocIdx >= 0) {
    ec.EmitLocalDtor(initLocIdx, Loc);
    // if leaving properly, release locals
    if (properLeave) ec.ReleaseLocalSlot(initLocIdx);
  }
}


//==========================================================================
//
//  VForeachScriptedOuter::toString
//
//==========================================================================
VStr VForeachScriptedOuter::toString () {
  return
    VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/foreachSInit ("+
    VExpression::e2s(ivInit)+")\n"+
    (Statement ? Statement->toString() : VStr("<none>"));
}



//**************************************************************************
//
// VForeachScripted
//
//**************************************************************************

//==========================================================================
//
//  VForeachScripted::VForeachScripted
//
//==========================================================================
VForeachScripted::VForeachScripted (VExpression *aarr, int afeCount, Var *afevars, const TLocation &aloc, VName aLabel)
  : VLoopStatementWithTempLocals(aloc, aLabel)
  , isBoolInit(false)
  , ivInit(nullptr)
  , ivNext(nullptr)
  , ivDone(nullptr)
  , arr(aarr)
  , fevarCount(afeCount)
  , reversed(false)
{
  if (afeCount < 0 || afeCount > VMethod::MAX_PARAMS) VCFatalError("VC: internal compiler error (VForeachScripted::VForeachScripted)");
  for (int f = 0; f < afeCount; ++f) fevars[f] = afevars[f];
  Statement = nullptr;
}


//==========================================================================
//
//  VForeachScripted::~VForeachScripted
//
//==========================================================================
VForeachScripted::~VForeachScripted () {
  tempLocals.clear();
  delete ivInit; ivInit = nullptr;
  delete ivNext; ivNext = nullptr;
  delete ivDone; ivDone = nullptr;
  delete arr; arr = nullptr;
  for (int f = 0; f < fevarCount; ++f) {
    delete fevars[f].var;
    delete fevars[f].decl;
  }
  fevarCount = 0;
}


//==========================================================================
//
//  VForeachScripted::SyntaxCopy
//
//==========================================================================
VStatement *VForeachScripted::SyntaxCopy () {
  //if (varinit || varnext || hiinit) VCFatalError("VC: `VForeachScripted::SyntaxCopy()` called on resolved statement");
  auto res = new VForeachScripted();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VForeachScripted::DoSyntaxCopyTo
//
//==========================================================================
void VForeachScripted::DoSyntaxCopyTo (VStatement *e) {
  VLoopStatementWithTempLocals::DoSyntaxCopyTo(e);
  auto res = (VForeachScripted *)e;
  res->arr = (arr ? arr->SyntaxCopy() : nullptr);
  for (int f = 0; f < fevarCount; ++f) {
    res->fevars[f] = fevars[f];
    if (fevars[f].var) res->fevars[f].var = fevars[f].var->SyntaxCopy();
    if (fevars[f].decl) res->fevars[f].decl = (VLocalDecl *)fevars[f].decl->SyntaxCopy();
  }
  res->fevarCount = fevarCount;
  res->reversed = reversed;
  // no need to copy private data here, as `SyntaxCopy()` should be called only on unresolved things
}


//==========================================================================
//
//  VForeachScripted::DoResolve
//
//==========================================================================
VStatement *VForeachScripted::DoResolve (VEmitContext &ec) {
  bool wasError = false;
  int initLocIdx = -1;

  // indent check
  if (Statement && !CheckCondIndent(Loc, Statement)) wasError = true;

  /* if iterator is invocation, rewrite it to:
   *   {
   *     firstargtype it;
   *     mtname_opIterInit(allargs); // or `mtname_opIterInitReverse`
   *     itsetup(doneaddr);
   *     {
   *       forvars;
   *       while (mtname_Next(it, forvars)) {
   *         <body>
   *       }
   *     }
   *    doneaddr:
   *     mtname_Done(it)
   *     itfinish // used in `return` processing
   *   }
   */

  VInvocationBase *ib = (VInvocationBase *)arr;
  if (!ib->IsMethodNameChangeable()) {
    ParseError(Loc, "Invalid VC iterator");
    return CreateInvalid();
  }

  int itlocidx = -1;
  {
    // create initializer expression
    VStr newName = VStr(ib->GetMethodName())+"_opIterInit";
    if (reversed) newName += "Reverse";
    VInvocationBase *einit = (VInvocationBase *)arr->SyntaxCopy();
    einit->SetMethodName(VName(*newName));
    VMethod *minit = einit->GetVMethod(ec);
    if (!minit) {
      //fprintf(stderr, "arr type: `%s` : `%s` (%s)\n", *shitppTypeNameObj(*arr), *shitppTypeNameObj(*einit), *einit->GetMethodName());
      ParseError(Loc, "Invalid VC iterator (opInit method not found)");
      wasError = true;
    }

    if (einit->NumArgs >= VMethod::MAX_PARAMS) {
      ParseError(Loc, "Too many arguments to VC iterator opInit method");
      wasError = true;
    }

    // check first arg, and get internal var type
    // should have at least one argument, and it should be `ref`/`out`
    if (minit->NumParams < 1 ||
        (minit->ParamFlags[0]&~(FPARM_Out|FPARM_Ref)) != 0 ||
        (minit->ParamFlags[0]&(FPARM_Out|FPARM_Ref)) == 0)
    {
      ParseError(Loc, "VC iterator opInit should have at least one arg, and it should be `ref`/`out`");
      wasError = true;
    }

    switch (minit->ReturnType.Type) {
      case TYPE_Void: isBoolInit = false; break;
      case TYPE_Bool: case TYPE_Int: isBoolInit = true; break;
      default:
        ParseError(Loc, "VC iterator opInit should return `bool` or be `void`");
        wasError = true;
    }

    if (wasError) {
      delete einit;
      return CreateInvalid();
    }

    // create hidden local for `it`
    {
      VLocalVarDef &L = ec.NewLocal(NAME_None, minit->ParamTypes[0], Loc);
      L.Visible = false; // it is unnamed, and hidden ;-)
      //tempLocals.append(L.GetIndex());
      initLocIdx = itlocidx;
      itlocidx = L.GetIndex();
    }

    // insert hidden local as first init arg
    for (int f = einit->NumArgs; f > 0; --f) einit->Args[f] = einit->Args[f-1];
    einit->Args[0] = new VLocalVar(itlocidx, Loc);
    ++einit->NumArgs;
    // and resolve the call
    ivInit = (isBoolInit ? einit->ResolveBoolean(ec) : einit->Resolve(ec));
    if (!ivInit) return CreateInvalid();
  }

  {
    // create next expression
    VStr newName = VStr(ib->GetMethodName())+"_opIterNext";
    VInvocationBase *enext = (VInvocationBase *)arr->SyntaxCopy();
    enext->SetMethodName(VName(*newName));
    VMethod *mnext = enext->GetVMethod(ec);
    if (!mnext) {
      delete enext;
      ParseError(Loc, "Invalid VC iterator (opNext method not found)");
      return CreateInvalid();
    }

    // all "next" args should be `ref`/`out`
    for (int f = 0; f < mnext->NumParams; ++f) {
      if ((mnext->ParamFlags[f]&~(FPARM_Out|FPARM_Ref)) != 0 ||
          (mnext->ParamFlags[f]&(FPARM_Out|FPARM_Ref)) == 0)
      {
        delete enext;
        ParseError(Loc, "VC iterator opNext argument %d is not `ref`/`out`", f+1);
        return CreateInvalid();
      }
    }

    // "next" should return bool
    switch (mnext->ReturnType.Type) {
      case TYPE_Bool: case TYPE_Int: break;
      default:
        delete enext;
        ParseError(Loc, "VC iterator opNext should return `bool`");
        return CreateInvalid();
    }

    // remove all `enext` args, and insert foreach args instead
    for (int f = 0; f < enext->NumArgs; ++f) delete enext->Args[f];
    enext->NumArgs = 1+fevarCount;
    enext->Args[0] = new VLocalVar(itlocidx, Loc);
    for (int f = 0; f < fevarCount; ++f) enext->Args[f+1] = fevars[f].var->SyntaxCopy();

    ivNext = enext->ResolveBoolean(ec);
    if (!ivNext) return CreateInvalid();
  }

  {
    // create done expression
    VStr newName = VStr(ib->GetMethodName())+"_opIterDone";
    VInvocationBase *edone = (VInvocationBase *)arr->SyntaxCopy();
    edone->SetMethodName(VName(*newName));
    VMethod *mdone = edone->GetVMethod(ec);
    // no done is ok
    if (mdone) {
      // has done
      // check first arg, and get internal var type
      // should have at least one argument, and it should be `ref`/`out`
      if (mdone->NumParams != 1 ||
          (mdone->ParamFlags[0]&~(FPARM_Out|FPARM_Ref)) != 0 ||
          (mdone->ParamFlags[0]&(FPARM_Out|FPARM_Ref)) == 0)
      {
        delete edone;
        ParseError(Loc, "VC iterator opDone should have one arg, and it should be `ref`/`out`");
        return CreateInvalid();
      }

      if (mdone->ReturnType.Type != TYPE_Void) {
        delete edone;
        ParseError(Loc, "VC iterator opDone should be `void`");
        return CreateInvalid();
      }

      // replace "done" args with hidden `it`
      for (int f = 0; f < edone->NumArgs; ++f) delete edone->Args[f];
      edone->NumArgs = 1;
      edone->Args[0] = new VLocalVar(itlocidx, Loc);

      ivDone = edone->Resolve(ec);
      if (!ivDone) return CreateInvalid();
    } else {
      delete edone;
      ivDone = nullptr;
    }
  }

  // finally, resolve statement (last, so local reusing will work as expected)
  Statement = Statement->Resolve(ec, this);
  if (!Statement->IsValid()) wasError = true;

  if (wasError) return CreateInvalid();

  // create outer statement
  VStatement *res = new VForeachScriptedOuter(isBoolInit, ivInit, initLocIdx, this, Loc);
  ivInit = nullptr;
  return res;
}


//==========================================================================
//
//  VForeachScripted::DoEmit
//
//==========================================================================
void VForeachScripted::DoEmit (VEmitContext &ec) {
  // allocate labels
  breakLabel = ec.DefineLabel();
  contLabel = ec.DefineLabel();

  // actual loop

  // put continue point here
  ec.MarkLabel(contLabel);

  // call next
  ivNext->EmitBranchable(ec, breakLabel, false);

  // emit loop body
  Statement->Emit(ec, this);

  // again
  ec.AddStatement(OPC_Goto, contLabel, Loc);

  // loop breaks here
  ec.MarkLabel(breakLabel);
}


//==========================================================================
//
//  VForeachScripted::IsBreakScope
//
//==========================================================================
bool VForeachScripted::IsBreakScope () const noexcept {
  return true;
}


//==========================================================================
//
//  VForeachScripted::IsContinueScope
//
//==========================================================================
bool VForeachScripted::IsContinueScope () const noexcept {
  return true;
}


//==========================================================================
//
//  VForeachScripted::IsInLoop
//
//==========================================================================
bool VForeachScripted::IsInLoop () const noexcept {
  return true;
}


//==========================================================================
//
//  VForeachScripted::EmitFinalizer
//
//==========================================================================
void VForeachScripted::EmitFinalizer (VEmitContext &ec, bool properLeave) {
  if (ivDone) ivDone->Emit(ec);
}


//==========================================================================
//
//  VForeachScripted::toString
//
//==========================================================================
VStr VForeachScripted::toString () {
  VStr res = VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/foreachS (";
  res += VExpression::e2s(arr)+"(";
  for (int f = 0; f < fevarCount; ++f) {
    if (fevars[f].isConst) res += "const ";
    if (fevars[f].isRef) res += "ref ";
    res += VExpression::e2s(fevars[f].var);
    res += ",";
  }
  res +=
    VExpression::e2s(arr)+(reversed ? "); reversed)\n" : "))\n")+
    (Statement ? Statement->toString() : VStr("<none>"));
  return res;
}



//**************************************************************************
//
// VSwitch
//
//**************************************************************************

//==========================================================================
//
//  VSwitch::VSwitch
//
//==========================================================================
VSwitch::VSwitch (const TLocation &ALoc, VName aLabel)
  : VStatement(ALoc, aLabel)
  , Expr(nullptr)
  , HaveDefault(false)
{
}


//==========================================================================
//
//  VSwitch::~VSwitch
//
//==========================================================================
VSwitch::~VSwitch () {
  delete Expr; Expr = nullptr;
  for (int i = 0; i < Statements.length(); ++i) {
    delete Statements[i]; Statements[i] = nullptr;
  }
}


//==========================================================================
//
//  VSwitch::SyntaxCopy
//
//==========================================================================
VStatement *VSwitch::SyntaxCopy () {
  auto res = new VSwitch();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VSwitch::DoSyntaxCopyTo
//
//==========================================================================
void VSwitch::DoSyntaxCopyTo (VStatement *e) {
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VSwitch *)e;
  res->Expr = (Expr ? Expr->SyntaxCopy() : nullptr);
  res->CaseInfo.setLength(CaseInfo.length());
  for (int f = 0; f < CaseInfo.length(); ++f) res->CaseInfo[f] = CaseInfo[f];
  res->DefaultAddress = DefaultAddress;
  res->Statements.setLength(Statements.length());
  for (int f = 0; f < Statements.length(); ++f) res->Statements[f] = (Statements[f] ? Statements[f]->SyntaxCopy() : nullptr);
  res->HaveDefault = HaveDefault;
  res->DoFixSwitch(this, res);
}


//==========================================================================
//
//  VSwitch::DoFixSwitch
//
//==========================================================================
void VSwitch::DoFixSwitch (VSwitch *aold, VSwitch *anew) {
  for (auto &&st : Statements) if (st) st->DoFixSwitch(aold, anew);
}


//==========================================================================
//
//  VSwitch::DoResolve
//
//==========================================================================
VStatement *VSwitch::DoResolve (VEmitContext &ec) {
  bool wasError = false;

  if (Expr) Expr = Expr->Resolve(ec);

  if (!Expr ||
      (Expr->Type.Type != TYPE_Byte && Expr->Type.Type != TYPE_Int &&
       Expr->Type.Type != TYPE_Name))
  {
    if (Expr) ParseError(Loc, "Invalid switch expression type (%s)", *Expr->Type.GetName());
    wasError = true;
  }

  // first resolve all cases and default
  for (auto &&st : Statements) {
    if (st->IsVarDecl()) {
      ParseError(st->Loc, "Declaring locals inside switch and without an explicit scope is forbidden");
      wasError = true;
    }
    if (st->IsSwitchCase() || st->IsSwitchDefault()) {
      st = st->Resolve(ec, this);
      if (!st->IsValid()) wasError = true;
    }
  }

  // now resolve other statements
  for (auto &&st : Statements) {
    if (!st->IsSwitchCase() && !st->IsSwitchDefault()) {
      st = st->Resolve(ec, this);
      if (!st->IsValid()) wasError = true;
    }
  }

  if (!checkProperCaseEnd(true)) wasError = true;

  return (wasError ? CreateInvalid() : this);
}


//==========================================================================
//
//  VSwitch::DoEmit
//
//==========================================================================
void VSwitch::DoEmit (VEmitContext &ec) {
  Expr->Emit(ec);

  // allocate labels
  breakLabel = ec.DefineLabel();
  //auto loopEnd = ec.DefineBreak(nullptr);

  const bool isName = (Expr->Type.Type == TYPE_Name);

  // case table
  for (auto &&ci : CaseInfo) {
    ci.Address = ec.DefineLabel();
    if (!isName) {
      // int/byte
      if (ci.Value >= 0 && ci.Value < 256) {
        ec.AddStatement(OPC_CaseGotoB, ci.Value, ci.Address, Loc);
      } else if (ci.Value >= MIN_VINT16 && ci.Value < MAX_VINT16) {
        ec.AddStatement(OPC_CaseGotoS, ci.Value, ci.Address, Loc);
      } else {
        ec.AddStatement(OPC_CaseGoto, ci.Value, ci.Address, Loc);
      }
    } else {
      ec.AddStatement(OPC_CaseGotoN, ci.Value, ci.Address, Loc);
    }
  }
  ec.AddStatement(OPC_Drop, Loc);

  // go to default case if we have one, otherwise to the end of switch
  if (HaveDefault) {
    DefaultAddress = ec.DefineLabel();
    ec.AddStatement(OPC_Goto, DefaultAddress, Loc);
  } else {
    ec.AddStatement(OPC_Goto, breakLabel, Loc);
  }

  // switch statements
  for (auto &&st : Statements) st->Emit(ec, this);

  // loop breaks here
  ec.MarkLabel(breakLabel);
}


//==========================================================================
//
//  VSwitch::IsBreakScope
//
//==========================================================================
bool VSwitch::IsBreakScope () const noexcept {
  return true;
}


//==========================================================================
//
//  VSwitch::IsEndsWithReturn
//
//==========================================================================
bool VSwitch::IsEndsWithReturn () const noexcept {
  //FIXME: `goto case` and `goto default`
  if (Statements.length() == 0) return false;
  bool defautSeen = false;
  bool returnSeen = false;
  bool breakSeen = false;
  bool statementSeen = false;
  for (auto &&st : Statements) {
    if (!st) return false; //k8: orly?
    // `case` or `default`?
    if (st->IsSwitchCase() || st->IsSwitchDefault()) {
      if (!returnSeen && statementSeen) return false; // oops
      if (st->IsSwitchDefault()) defautSeen = true;
      breakSeen = false;
      statementSeen = true;
      returnSeen = false;
      continue;
    }
    if (breakSeen) continue;
    statementSeen = true;
    // always check for returns
    if (!returnSeen) returnSeen = st->IsEndsWithReturn();
    // check for flow break
    if (st->IsFlowStop()) {
      // `break`/`continue`/`return`/etc.
      if (!returnSeen) return false;
      breakSeen = true;
    }
  }
  if (!statementSeen) return false; // just in case
  // without `default` it may fallthru
  return (returnSeen && defautSeen);
}


//==========================================================================
//
//  VSwitch::IsProperCaseEnd
//
//==========================================================================
bool VSwitch::IsProperCaseEnd (const VStatement *ASwitch) const noexcept {
  return IsEndsWithReturn();
}


//==========================================================================
//
//  VSwitch::checkProperCaseEnd
//
//==========================================================================
bool VSwitch::checkProperCaseEnd (bool reportSwitchCase) const noexcept {
  if (Statements.length() == 0) return true;
  bool statementSeen = false;
  bool isEndSeen = false;
  int lastCaseIdx = -1;
  //ParseWarning(Loc, "=========================");
  int stindex = -1;
  for (auto &&st : Statements) {
    ++stindex;
    if (!st) return false; //k8: orly?
    // `case` or `default`?
    if (st->IsSwitchCase() || st->IsSwitchDefault()) {
      if (lastCaseIdx >= 0 && !isEndSeen && statementSeen) {
        //fprintf(stderr, "pidx=%d; n=%d; es=%d; ss=%d\n", lastCaseIdx, n, (int)isEndSeen, (int)statementSeen);
        // oops
        if (reportSwitchCase) ParseError(Statements[lastCaseIdx]->Loc, "`switch` branch doesn't end with `break` or `goto case`");
        return false;
      }
      //lastCaseIdx = n;
      lastCaseIdx = stindex;
      isEndSeen = false;
      statementSeen = false;
      continue;
    }
    if (isEndSeen) continue;
    //fprintf(stderr, "  n=%d; type=%s\n", n, *shitppTypeNameObj(*Statements[n]));
    statementSeen = true;
    // proper end?
    if (st->IsProperCaseEnd(this)) {
      isEndSeen = true;
      continue;
    }
    // flow break?
    if (st->IsFlowStop()) {
      // `break`/`continue`/`return`/etc.
      isEndSeen = true;
      continue;
    }
  }
  // last one can omit proper end
  return true;
}


//==========================================================================
//
//  VSwitch::toString
//
//==========================================================================
VStr VSwitch::toString () {
  VStr res = VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/switch (";
  res += VExpression::e2s(Expr)+") {\n";
  for (auto &&st : Statements) {
    res += st->toString();
    res += "\n";
  }
  res += "}";
  return res;
}



//**************************************************************************
//
// VSwitchCase
//
//**************************************************************************

//==========================================================================
//
//  VSwitchCase::VSwitchCase
//
//==========================================================================
VSwitchCase::VSwitchCase (VSwitch *ASwitch, VExpression *AExpr, const TLocation &ALoc)
  : VStatement(ALoc)
  , Switch(ASwitch)
  , Expr(AExpr)
  , Value(0)
  , Index(0)
  , gotoLbl()
{
}


//==========================================================================
//
//  VSwitchCase::~VSwitchCase
//
//==========================================================================
VSwitchCase::~VSwitchCase () {
  delete Expr; Expr = nullptr;
}


//==========================================================================
//
//  VSwitch::SyntaxCopy
//
//==========================================================================
VStatement *VSwitchCase::SyntaxCopy () {
  auto res = new VSwitchCase();
  DoSyntaxCopyTo(res);
  return res;
}



//==========================================================================
//
//  VSwitchCase::DoSyntaxCopyTo
//
//==========================================================================
void VSwitchCase::DoSyntaxCopyTo (VStatement *e) {
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VSwitchCase *)e;
  res->Switch = Switch;
  res->Expr = (Expr ? Expr->SyntaxCopy() : nullptr);
  res->Value = Value;
  res->Index = Index;
}


//==========================================================================
//
//  VSwitchCase::DoFixSwitch
//
//==========================================================================
void VSwitchCase::DoFixSwitch (VSwitch *aold, VSwitch *anew) {
  if (Switch == aold) Switch = anew;
}


//==========================================================================
//
//  VSwitchCase::DoResolve
//
//==========================================================================
VStatement *VSwitchCase::DoResolve (VEmitContext &ec) {
  if (Switch && Expr && Switch->Expr && Switch->Expr->Type.Type == TYPE_Name) {
    Expr = Expr->Resolve(ec);
    if (!Expr) return CreateInvalid();
    if (!Expr->IsNameConst()) {
      ParseError(Loc, "case condition should be a name literal");
      return CreateInvalid();
    }
    Value = Expr->GetNameConst().GetIndex();
  } else {
    if (Expr) Expr = Expr->ResolveToIntLiteralEx(ec);
    if (!Expr) return CreateInvalid();
    if (!Expr->IsIntConst()) {
      ParseError(Loc, "case condition should be an integer literal");
      return CreateInvalid();
    }
    Value = Expr->GetIntConst();
  }

  for (auto &&ci : Switch->CaseInfo) {
    if (ci.Value == Value) {
      ParseError(Loc, "Duplicate case value");
      return CreateInvalid();
    }
  }

  Index = Switch->CaseInfo.length();
  VSwitch::VCaseInfo &C = Switch->CaseInfo.Alloc();
  C.Value = Value;

  return this;
}


//==========================================================================
//
//  VSwitchCase::DoEmit
//
//==========================================================================
void VSwitchCase::DoEmit (VEmitContext &ec) {
  ec.MarkLabel(Switch->CaseInfo[Index].Address);
  if (gotoLbl.IsDefined()) ec.MarkLabel(gotoLbl);
}


//==========================================================================
//
//  VSwitchCase::IsCase
//
//==========================================================================
bool VSwitchCase::IsSwitchCase () const noexcept {
  return true;
}


//==========================================================================
//
//  VSwitchCase::toString
//
//==========================================================================
VStr VSwitchCase::toString () {
  return
    VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/case "+
    VExpression::e2s(Expr)+":";
}



//**************************************************************************
//
// VSwitchDefault
//
//**************************************************************************

//==========================================================================
//
//  VSwitchDefault::VSwitchDefault
//
//==========================================================================
VSwitchDefault::VSwitchDefault (VSwitch *ASwitch, const TLocation &ALoc)
  : VStatement(ALoc)
  , Switch(ASwitch)
{
}


//==========================================================================
//
//  VSwitchDefault::SyntaxCopy
//
//==========================================================================
VStatement *VSwitchDefault::SyntaxCopy () {
  auto res = new VSwitchDefault();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VSwitchDefault::DoSyntaxCopyTo
//
//==========================================================================
void VSwitchDefault::DoSyntaxCopyTo (VStatement *e) {
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VSwitchDefault *)e;
  res->Switch = Switch;
}


//==========================================================================
//
//  VSwitchDefault::DoFixSwitch
//
//==========================================================================
void VSwitchDefault::DoFixSwitch (VSwitch *aold, VSwitch *anew) {
  if (Switch == aold) Switch = anew;
}


//==========================================================================
//
//  VSwitchDefault::DoResolve
//
//==========================================================================
VStatement *VSwitchDefault::DoResolve (VEmitContext &) {
  if (Switch->HaveDefault) {
    ParseError(Loc, "Only one `default` per switch allowed");
    return CreateInvalid();
  }
  Switch->HaveDefault = true;
  return this;
}


//==========================================================================
//
//  VSwitchDefault::DoEmit
//
//==========================================================================
void VSwitchDefault::DoEmit (VEmitContext &ec) {
  ec.MarkLabel(Switch->DefaultAddress);
}


//==========================================================================
//
//  VSwitchDefault::IsSwitchDefault
//
//==========================================================================
bool VSwitchDefault::IsSwitchDefault () const noexcept {
  return true;
}


//==========================================================================
//
//  VSwitchDefault::toString
//
//==========================================================================
VStr VSwitchDefault::toString () {
  return VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/default:";
}



//**************************************************************************
//
// VBreak
//
//**************************************************************************

//==========================================================================
//
//  VBreak::VBreak
//
//==========================================================================
VBreak::VBreak (const TLocation &ALoc, VName aLoopLabel)
  : VStatement(ALoc)
  , LoopLabel(aLoopLabel)
{
}


//==========================================================================
//
//  VBreak::SyntaxCopy
//
//==========================================================================
VStatement *VBreak::SyntaxCopy () {
  auto res = new VBreak();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VBreak::DoSyntaxCopyTo
//
//==========================================================================
void VBreak::DoSyntaxCopyTo (VStatement *e) {
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VBreak *)e;
  res->LoopLabel = LoopLabel;
}


//==========================================================================
//
//  VBreak::DoResolve
//
//==========================================================================
VStatement *VBreak::DoResolve (VEmitContext &ec) {
  // check if we have a good break scope
  for (VStatement *st = UpScope; st; st = st->UpScope) {
    if (!st->IsContBreakAllowed()) {
      ParseError(Loc, "`break` jumps outside of a restricted scope");
      return CreateInvalid();
    }
    if (st->IsBreakScope()) {
      if (LoopLabel == NAME_None || LoopLabel == st->Label) return this;
    }
    if (!st->IsReturnAllowed()) {
      ParseError(Loc, "`break` jumps outside of a restricted scope");
      return CreateInvalid();
    }
  }
  if (LoopLabel != NAME_None) {
    ParseError(Loc, "`break` label `%s` not found", *LoopLabel);
  } else {
    ParseError(Loc, "`break` without loop or switch");
  }
  return CreateInvalid();
}


//==========================================================================
//
//  VBreak::DoEmit
//
//==========================================================================
void VBreak::DoEmit (VEmitContext &ec) {
  // emit dtors for all scopes
  // emit finalizers for all scopes except the destination one
  for (VStatement *st = this->UpScope; st; st = st->UpScope) {
    st->EmitDtor(ec, false); // abnormal leave
    const bool destReached = (st->IsBreakScope() && (LoopLabel == NAME_None || LoopLabel == st->Label));
    if (destReached) {
      // jump to break destination
      ec.AddStatement(OPC_Goto, st->breakLabel, Loc);
      return;
    }
    st->EmitFinalizer(ec, false); // abnormal leave
  }
  VCFatalError("internal compiler error (break)");
}


//==========================================================================
//
//  VBreak::IsBreak
//
//==========================================================================
bool VBreak::IsBreak () const noexcept {
  return true;
}


//==========================================================================
//
//  VBreak::IsFlowStop
//
//==========================================================================
bool VBreak::IsFlowStop () const noexcept {
  return true;
}


//==========================================================================
//
//  VBreak::IsProperCaseEnd
//
//==========================================================================
bool VBreak::IsProperCaseEnd (const VStatement *ASwitch) const noexcept {
  // check break scopes
  bool res = false;
  for (VStatement *st = UpScope; st; st = st->UpScope) {
    if (st == ASwitch) res = true;
    if (!st->IsContBreakAllowed()) break;
    if (st->IsBreakScope() && (LoopLabel == NAME_None || LoopLabel == st->Label)) break;
    if (!st->IsReturnAllowed()) break;
  }
  return res;
}


//==========================================================================
//
//  VBreak::toString
//
//==========================================================================
VStr VBreak::toString () {
  return VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/break;";
}



//**************************************************************************
//
// VContinue
//
//**************************************************************************

//==========================================================================
//
//  VContinue::VContinue
//
//==========================================================================
VContinue::VContinue (const TLocation &ALoc, VName aLoopLabel)
  : VStatement(ALoc)
  , LoopLabel(aLoopLabel)
{
}


//==========================================================================
//
//  VContinue::SyntaxCopy
//
//==========================================================================
VStatement *VContinue::SyntaxCopy () {
  auto res = new VContinue();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VContinue::DoSyntaxCopyTo
//
//==========================================================================
void VContinue::DoSyntaxCopyTo (VStatement *e) {
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VContinue *)e;
  res->LoopLabel = LoopLabel;
}


//==========================================================================
//
//  VContinue::DoResolve
//
//==========================================================================
VStatement *VContinue::DoResolve (VEmitContext &) {
  // check if we have a good continue scope
  for (VStatement *st = UpScope; st; st = st->UpScope) {
    if (!st->IsContBreakAllowed()) {
      ParseError(Loc, "`continue` jumps outside of a restricted scope");
      return CreateInvalid();
    }
    if (st->IsContinueScope()) {
      if (LoopLabel == NAME_None || LoopLabel == st->Label) return this;
    }
    if (!st->IsReturnAllowed()) {
      ParseError(Loc, "`continue` jumps outside of a restricted scope");
      return CreateInvalid();
    }
  }
  if (LoopLabel != NAME_None) {
    ParseError(Loc, "`continue` label `%s` not found", *LoopLabel);
  } else {
    ParseError(Loc, "`continue` without loop or switch");
  }
  return CreateInvalid();
}


//==========================================================================
//
//  VContinue::DoEmit
//
//==========================================================================
void VContinue::DoEmit (VEmitContext &ec) {
  // emit dtors for all scopes
  // emit finalizers for all scopes except the destination one
  for (VStatement *st = this->UpScope; st; st = st->UpScope) {
    st->EmitDtor(ec, false); // abnormal leave
    const bool destReached = (st->IsContinueScope() && (LoopLabel == NAME_None || LoopLabel == st->Label));
    if (destReached) {
      // jump to conitnue destination
      ec.AddStatement(OPC_Goto, st->contLabel, Loc);
      return;
    }
    st->EmitFinalizer(ec, false); // abnormal leave
  }
  VCFatalError("internal compiler error (break)");
}


//==========================================================================
//
//  VContinue::IsContinue
//
//==========================================================================
bool VContinue::IsContinue () const noexcept {
  return true;
}


//==========================================================================
//
//  VContinue::IsFlowStop
//
//==========================================================================
bool VContinue::IsFlowStop () const noexcept {
  return true;
}


//==========================================================================
//
//  VContinue::IsProperCaseEnd
//
//==========================================================================
bool VContinue::IsProperCaseEnd (const VStatement *ASwitch) const noexcept {
  // check continue scopes
  bool res = false;
  for (VStatement *st = UpScope; st; st = st->UpScope) {
    if (st == ASwitch) res = true;
    if (!st->IsContBreakAllowed()) break;
    if (st->IsContinueScope() && (LoopLabel == NAME_None || LoopLabel == st->Label)) break;
    if (!st->IsReturnAllowed()) break;
  }
  return res;
}


//==========================================================================
//
//  VContinue::toString
//
//==========================================================================
VStr VContinue::toString () {
  return VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/continue;";
}



//**************************************************************************
//
// VReturn
//
//**************************************************************************

//==========================================================================
//
//  VReturn::VReturn
//
//==========================================================================
VReturn::VReturn (VExpression *AExpr, const TLocation &ALoc)
  : VStatement(ALoc)
  , Expr(AExpr)
{
}


//==========================================================================
//
//  VReturn::~VReturn
//
//==========================================================================
VReturn::~VReturn () {
  delete Expr; Expr = nullptr;
}


//==========================================================================
//
//  VReturn::SyntaxCopy
//
//==========================================================================
VStatement *VReturn::SyntaxCopy () {
  auto res = new VReturn();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VReturn::DoSyntaxCopyTo
//
//==========================================================================
void VReturn::DoSyntaxCopyTo (VStatement *e) {
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VReturn *)e;
  res->Expr = (Expr ? Expr->SyntaxCopy() : nullptr);
  res->NumLocalsToClear = NumLocalsToClear;
}


//==========================================================================
//
//  VReturn::DoResolve
//
//==========================================================================
VStatement *VReturn::DoResolve (VEmitContext &ec) {
  // check if we can return from here
  for (VStatement *st = UpScope; st; st = st->UpScope) {
    if (!st->IsReturnAllowed()) {
      ParseError(Loc, "`return` jumps outside of a restricted scope");
      return CreateInvalid();
    }
  }

  bool wasError = false;

  if (Expr) {
    Expr = (ec.FuncRetType.Type == TYPE_Float ? Expr->ResolveFloat(ec) : Expr->Resolve(ec));
    if (!Expr) wasError = true;
    if (!wasError && ec.FuncRetType.Type == TYPE_Void) {
      // allow `return func();` in voids
      if (Expr->Type.Type != TYPE_Void) {
        ParseError(Loc, "void function cannot return a value");
        wasError = true;
      }
    } else if (Expr->Type.GetStackSlotCount() == 1 || Expr->Type.Type == TYPE_Vector) {
      const bool res = Expr->Type.CheckMatch(false, Expr->Loc, ec.FuncRetType);
      if (!res) wasError = true;
    } else {
      ParseError(Loc, "Bad return type");
      wasError = true;
    }
  } else if (ec.FuncRetType.Type != TYPE_Void) {
    ParseError(Loc, "Return value expected");
    wasError = true;
  }

  return (wasError ? CreateInvalid() : this);
}


//==========================================================================
//
//  VReturn::DoEmit
//
//==========================================================================
void VReturn::DoEmit (VEmitContext &ec) {
  if (Expr) {
    // balance stack by dropping old return value
    if (ec.InReturn > 1) {
      //GLog.Logf(NAME_Debug, "%s: return; (%d)", *Loc.toStringNoCol(), ec.InReturn);
      ec.AddStatement(OPC_DropPOD, Loc);
      if (Expr->Type.Type == TYPE_Vector) {
        ec.AddStatement(OPC_DropPOD, Loc);
        ec.AddStatement(OPC_DropPOD, Loc);
      }
    }
    // emit new return value
    Expr->Emit(ec);
  }

  // emit dtors and finalizers for all scopes
  for (VStatement *st = this->UpScope; st; st = st->UpScope) {
    st->EmitDtor(ec, false); // abnormal leave
    st->EmitFinalizer(ec, false); // abnormal leave
  }

  if (Expr) {
         if (Expr->Type.GetStackSlotCount() == 1) ec.AddStatement(OPC_ReturnL, Loc);
    else if (Expr->Type.Type == TYPE_Vector) ec.AddStatement(OPC_ReturnV, Loc);
    else ec.AddStatement(OPC_Return, Loc);
    //else ParseError(Loc, "Bad return type `%s`", *Expr->Type.GetName());
  } else {
    ec.AddStatement(OPC_Return, Loc);
  }
}


//==========================================================================
//
//  VReturn::IsReturn
//
//==========================================================================
bool VReturn::IsReturn () const noexcept {
  return true;
}


//==========================================================================
//
//  VReturn::IsFlowStop
//
//==========================================================================
bool VReturn::IsFlowStop () const noexcept {
  return true;
}


//==========================================================================
//
//  VReturn::IsProperCaseEnd
//
//==========================================================================
bool VReturn::IsProperCaseEnd (const VStatement *ASwitch) const noexcept {
  return true; // always
}


//==========================================================================
//
//  VReturn::IsEndsWithReturn
//
//==========================================================================
bool VReturn::IsEndsWithReturn () const noexcept {
  return true;
}


//==========================================================================
//
//  VReturn::IsInReturn
//
//==========================================================================
bool VReturn::IsInReturn () const noexcept {
  return true;
}


//==========================================================================
//
//  VReturn::toString
//
//==========================================================================
VStr VReturn::toString () {
  return
    VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/return"+
    (Expr ? " (" : "")+
    (Expr ? VExpression::e2s(Expr) : VStr(""))+
    (Expr ? ")" : "")+";";
}



//**************************************************************************
//
// VGotoStmt
//
//**************************************************************************

//==========================================================================
//
//  VGotoStmt::VGotoStmt
//
//==========================================================================
VGotoStmt::VGotoStmt (VSwitch *ASwitch, VExpression *ACaseValue, int ASwitchStNum, bool toDefault, const TLocation &ALoc)
  : VStatement(ALoc)
  , casedef(nullptr)
  , Switch(ASwitch)
  , CaseValue(ACaseValue)
  , GotoType(toDefault ? Default : Case)
  , SwitchStNum(ASwitchStNum)
{
}


//==========================================================================
//
//  VGotoStmt::VGotoStmt
//
//==========================================================================
VStatement *VGotoStmt::SyntaxCopy () {
  auto res = new VGotoStmt();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VGotoStmt::DoSyntaxCopyTo
//
//==========================================================================
void VGotoStmt::DoSyntaxCopyTo (VStatement *e) {
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VGotoStmt *)e;
  res->Switch = Switch;
  res->CaseValue = (CaseValue ? CaseValue->SyntaxCopy() : nullptr);
  res->GotoType = GotoType;
}


//==========================================================================
//
//  VGotoStmt::DoResolve
//
//==========================================================================
VStatement *VGotoStmt::DoResolve (VEmitContext &ec) {
  if (!Switch) VCFatalError("VC: internal compiler error (VGotoStmt::Resolve) (0)");
  // find case or default
  VStatement *st = nullptr;
  if (GotoType == Default) {
    // find the default
    for (int f = Switch->Statements.length()-1; f >= 0; --f) {
      if (Switch->Statements[f]->IsSwitchDefault()) {
        st = Switch->Statements[f];
        break;
      }
    }
    if (!st) { ParseError(Loc, "`goto default;` whithout `default`"); return CreateInvalid(); }
  } else {
    vassert(GotoType == Case);
    // find the case
    if (CaseValue) {
      // case is guaranteed to be parsed, do value search
      int val;
      if (Switch && Switch->Expr && Switch->Expr->Type.Type == TYPE_Name) {
        CaseValue = CaseValue->Resolve(ec);
        if (!CaseValue) return CreateInvalid(); // oops
        if (!CaseValue->IsNameConst()) {
          ParseError(Loc, "`goto case` expects a name literal");
          return CreateInvalid();
        }
        val = CaseValue->GetNameConst().GetIndex();
      } else {
        CaseValue = CaseValue->ResolveToIntLiteralEx(ec);
        if (!CaseValue) return CreateInvalid(); // oops
        if (!CaseValue->IsIntConst()) {
          ParseError(Loc, "`goto case` expects an integer literal");
          return CreateInvalid();
        }
        val = CaseValue->GetIntConst();
      }
      for (int f = 0; f < Switch->Statements.length(); ++f) {
        if (Switch->Statements[f]->IsSwitchCase()) {
          VSwitchCase *cc = (VSwitchCase *)(Switch->Statements[f]);
          if (cc->Value == val) {
            st = cc;
            break;
          }
        }
      }
      if (!st) { ParseError(Loc, "case `%d` not found", val); return CreateInvalid(); }
    } else {
      // `goto case` without args: find next one
      for (int f = SwitchStNum; f < Switch->Statements.length(); ++f) {
        if (Switch->Statements[f]->IsSwitchCase()) {
          st = Switch->Statements[f];
          break;
        }
      }
      if (!st) { ParseError(Loc, "case for `goto case;` not found"); return CreateInvalid(); }
    }
    vassert(st);
    VSwitchCase *cc = (VSwitchCase *)st;
    if (!cc->gotoLbl.IsDefined()) cc->gotoLbl = ec.DefineLabel();
  }
  vassert(st);
  casedef = st;
  // do additional scope check
  for (VStatement *scp = UpScope; scp; scp = scp->UpScope) {
    if (scp == Switch) return this;
    if (!st->IsReturnAllowed()) {
      ParseError(Loc, "`goto` jumps outside of a restricted scope");
      return CreateInvalid();
    }
  }
  VCFatalError("VC: internal compiler error (VGotoStmt::Resolve) (1)");
  return CreateInvalid();
}


//==========================================================================
//
//  VGotoStmt::DoEmit
//
//==========================================================================
void VGotoStmt::DoEmit (VEmitContext &ec) {
  if (!Switch || !casedef) return; // just in case

  // emit cleanups (it is guaranteed to be in the switch)
  for (VStatement *scp = this->UpScope; scp != Switch; scp = scp->UpScope) {
    scp->EmitDtor(ec, false); // abnormal leave
    scp->EmitFinalizer(ec, false); // abnormal leave
  }

  if (GotoType == Case) {
    if (!casedef) {
      ParseError(Loc, "Misplaced `goto case` statement");
      return;
    }
    if (!casedef->IsSwitchCase()) VCFatalError("VC: internal compiler error (VGotoStmt::DoEmit) (0)");
    VSwitchCase *cc = (VSwitchCase *)casedef;
    ec.AddStatement(OPC_Goto, cc->gotoLbl, Loc);
  } else if (GotoType == Default) {
    if (!casedef) {
      ParseError(Loc, "Misplaced `goto default` statement");
      return;
    }
    ec.AddStatement(OPC_Goto, Switch->DefaultAddress, Loc);
  } else {
    VCFatalError("VC: internal compiler error (VGotoStmt::DoEmit)");
  }
}


//==========================================================================
//
//  VGotoStmt::IsGoto
//
//==========================================================================
bool VGotoStmt::IsGoto () const noexcept {
  return true;
}


//==========================================================================
//
//  VGotoStmt::IsGotoCase
//
//==========================================================================
bool VGotoStmt::IsGotoCase () const noexcept {
  return (GotoType == Case);
}


//==========================================================================
//
//  VGotoStmt::IsGotoDefault
//
//==========================================================================
bool VGotoStmt::IsGotoDefault () const noexcept {
  return (GotoType == Default);
}


//==========================================================================
//
//  VGotoStmt::IsFlowStop
//
//==========================================================================
bool VGotoStmt::IsFlowStop () const noexcept {
  return true;
}


//==========================================================================
//
//  VGotoStmt::IsProperCaseEnd
//
//==========================================================================
bool VGotoStmt::IsProperCaseEnd (const VStatement *ASwitch) const noexcept {
  return (Switch == ASwitch);
}


//==========================================================================
//
//  VGotoStmt::DoFixSwitch
//
//==========================================================================
void VGotoStmt::DoFixSwitch (VSwitch *aold, VSwitch *anew) {
  if (Switch == aold) Switch = anew;
}


//==========================================================================
//
//  VGotoStmt::toString
//
//==========================================================================
VStr VGotoStmt::toString () {
  return VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/goto "+
  (GotoType == Default ? VStr("default") :
   (CaseValue ? VStr("case ")+CaseValue->toString() : VStr("case")))+";";
}



//**************************************************************************
//
// VBaseCompoundStatement
//
//**************************************************************************

//==========================================================================
//
//  VBaseCompoundStatement::VBaseCompoundStatement
//
//==========================================================================
VBaseCompoundStatement::VBaseCompoundStatement (const TLocation &aloc)
  : VStatement(aloc)
{
}


//==========================================================================
//
//  VBaseCompoundStatement::~VBaseCompoundStatement
//
//==========================================================================
VBaseCompoundStatement::~VBaseCompoundStatement () {
  for (auto &&st : Statements) delete st;
  Statements.clear();
}


//==========================================================================
//
//  VBaseCompoundStatement::DoSyntaxCopyTo
//
//==========================================================================
void VBaseCompoundStatement::DoSyntaxCopyTo (VStatement *e) {
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VBaseCompoundStatement *)e;
  res->Statements.setLength(Statements.length());
  for (int f = 0; f < Statements.length(); ++f) res->Statements[f] = (Statements[f] ? Statements[f]->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VBaseCompoundStatement::ProcessVarDecls
//
//==========================================================================
void VBaseCompoundStatement::ProcessVarDecls () {
  for (int idx = 0; idx < Statements.length()-1; ++idx) {
    if (Statements[idx]->IsVarDecl()) {
      // i found her!
      VLocalVarStatement *vst = (VLocalVarStatement *)Statements[idx];
      vassert(vst->Statements.length() == 0);
      const int newlen = idx+1; // skip `vst` (i.e. leave it in this compound)
      // move compound tail to `vst`
      vst->Statements.resize(Statements.length()-newlen);
      for (idx = newlen; idx < Statements.length(); ++idx) vst->Statements.append(Statements[idx]);
      // shrink compound list
      Statements.setLength(newlen);
      // recursively process `vst`
      //return vst->ProcessVarDecls();
      // there is no need to recursively process `vst`, because its resolver
      // will call `ProcessVarDecls()` by itself, so we'll be doing excess
      // work for nothing here
      return;
    }
  }
}


//==========================================================================
//
//  VBaseCompoundStatement::BeforeProcessVarDecls
//
//==========================================================================
bool VBaseCompoundStatement::BeforeProcessVarDecls (VEmitContext &ec) {
  return true;
}


//==========================================================================
//
//  VBaseCompoundStatement::BeforeResolveStatements
//
//==========================================================================
bool VBaseCompoundStatement::BeforeResolveStatements (VEmitContext &ec) {
  return true;
}


//==========================================================================
//
//  VBaseCompoundStatement::AfterResolveStatements
//
//==========================================================================
bool VBaseCompoundStatement::AfterResolveStatements (VEmitContext &ec) {
  return true;
}


//==========================================================================
//
//  VBaseCompoundStatement::DoResolve
//
//==========================================================================
VStatement *VBaseCompoundStatement::DoResolve (VEmitContext &ec) {
  bool wasError = false;
  if (!BeforeProcessVarDecls(ec)) wasError = true;
  ProcessVarDecls();
  if (!BeforeResolveStatements(ec)) wasError = true;
  for (auto &&st : Statements) {
    st = st->Resolve(ec, this);
    if (!st->IsValid()) wasError = true;
  }
  if (!AfterResolveStatements(ec)) wasError = true;
  return (wasError ? CreateInvalid() : this);
}


//==========================================================================
//
//  VBaseCompoundStatement::BeforeEmitStatements
//
//==========================================================================
bool VBaseCompoundStatement::BeforeEmitStatements (VEmitContext &ec) {
  return true;
}


//==========================================================================
//
//  VBaseCompoundStatement::AfterEmitStatements
//
//==========================================================================
bool VBaseCompoundStatement::AfterEmitStatements (VEmitContext &ec) {
  return true;
}


//==========================================================================
//
//  VBaseCompoundStatement::DoEmit
//
//==========================================================================
void VBaseCompoundStatement::DoEmit (VEmitContext &ec) {
  if (!BeforeEmitStatements(ec)) return;
  for (auto &&st : Statements) if (st) st->Emit(ec, this);
  if (!AfterEmitStatements(ec)) return;
}


//==========================================================================
//
//  VBaseCompoundStatement::IsAnyCompound
//
//==========================================================================
bool VBaseCompoundStatement::IsAnyCompound () const noexcept {
  return true;
}


//==========================================================================
//
//  VBaseCompoundStatement::IsEndsWithReturn
//
//==========================================================================
bool VBaseCompoundStatement::IsEndsWithReturn () const noexcept {
  for (auto &&st : Statements) {
    if (!st) continue;
    if (st->IsEndsWithReturn()) return true;
    if (st->IsFlowStop()) break;
  }
  return false;
}


//==========================================================================
//
//  VBaseCompoundStatement::IsProperCaseEnd
//
//==========================================================================
bool VBaseCompoundStatement::IsProperCaseEnd (const VStatement *ASwitch) const noexcept {
  for (auto &&st : Statements) {
    if (!st) continue;
    if (st->IsProperCaseEnd(ASwitch)) return true;
    if (st->IsFlowStop()) break;
  }
  return false;
}


//==========================================================================
//
//  VBaseCompoundStatement::DoFixSwitch
//
//==========================================================================
void VBaseCompoundStatement::DoFixSwitch (VSwitch *aold, VSwitch *anew) {
  for (auto &&st : Statements) if (st) st->DoFixSwitch(aold, anew);
}


//==========================================================================
//
//  VBaseCompoundStatement::toString
//
//==========================================================================
VStr VBaseCompoundStatement::toString () {
  VStr res = VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/{";
  if (Statements.length()) {
    for (auto &&st : Statements) {
      if (!st) continue;
      res += "\n";
      res += st->toString();
    }
    res += "\n}";
  } else {
    res += "}";
  }
  return res;
}



//**************************************************************************
//
// VTryFinallyCompound
//
//**************************************************************************

//==========================================================================
//
//  VTryFinallyCompound::VTryFinallyCompound
//
//==========================================================================
VTryFinallyCompound::VTryFinallyCompound (VStatement *aFinally, const TLocation &ALoc)
  : VBaseCompoundStatement(ALoc)
  , Finally(aFinally)
  , retScope(false)
  , returnAllowed(true)
  , breakAllowed(true)
{
}


//==========================================================================
//
//  VTryFinallyCompound::~VTryFinallyCompound
//
//==========================================================================
VTryFinallyCompound::~VTryFinallyCompound () {
  delete Finally; Finally = nullptr;
}


//==========================================================================
//
//  VTryFinallyCompound::SyntaxCopy
//
//==========================================================================
VStatement *VTryFinallyCompound::SyntaxCopy () {
  auto res = new VTryFinallyCompound();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VTryFinallyCompound::DoSyntaxCopyTo
//
//==========================================================================
void VTryFinallyCompound::DoSyntaxCopyTo (VStatement *e) {
  VBaseCompoundStatement::DoSyntaxCopyTo(e);
  auto res = (VTryFinallyCompound *)e;
  res->Finally = (Finally ? Finally->SyntaxCopy() : nullptr);
  res->retScope = retScope;
  // no need to copy `return allowed`
}


//==========================================================================
//
//  VTryFinallyCompound::BeforeResolveStatements
//
//==========================================================================
bool VTryFinallyCompound::BeforeResolveStatements (VEmitContext &ec) {
  bool wasError = false;

  if (Finally) {
    //returnAllowed = false;
    // we cannot break loop from `return` scope
    if (retScope) breakAllowed = false;
    Finally = Finally->Resolve(ec, this);
    if (!Finally->IsValid()) wasError = true;
    if (retScope) breakAllowed = true;
    //returnAllowed = true;
  }

  return !wasError;
}


//==========================================================================
//
//  VTryFinallyCompound::IsTryFinally
//
//==========================================================================
bool VTryFinallyCompound::IsTryFinally () const noexcept {
  return true;
}


//==========================================================================
//
//  VTryFinallyCompound::IsReturnAllowed
//
//==========================================================================
bool VTryFinallyCompound::IsReturnAllowed () const noexcept {
  return returnAllowed;
}


//==========================================================================
//
//  VTryFinallyCompound::IsContBreakAllowed
//
//==========================================================================
bool VTryFinallyCompound::IsContBreakAllowed () const noexcept {
  return breakAllowed;
}


//==========================================================================
//
//  VTryFinallyCompound::EmitDtor
//
//==========================================================================
void VTryFinallyCompound::EmitDtor (VEmitContext &ec, bool properLeave) {
  // `scope(exit)` should be called even on `return`
  if (retScope && !ec.InReturn) return;
  if (Finally) Finally->Emit(ec, this->UpScope); // avoid double return
}


//==========================================================================
//
//  VTryFinallyCompound::toString
//
//==========================================================================
VStr VTryFinallyCompound::toString () {
  return
    VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/try"+(retScope ? "(return)" : "")+"\n"+
    VBaseCompoundStatement::toString()+
    (Finally ? VStr("/*finally*/")+Finally->toString() : VStr());
}



//**************************************************************************
//
// VLocalVarStatement
//
//**************************************************************************

//==========================================================================
//
//  VLocalVarStatement::VLocalVarStatement
//
//==========================================================================
VLocalVarStatement::VLocalVarStatement (VLocalDecl *ADecl)
  : VBaseCompoundStatement(ADecl->Loc)
  , Decl(ADecl)
{
}


//==========================================================================
//
//  VLocalVarStatement::~VLocalVarStatement
//
//==========================================================================
VLocalVarStatement::~VLocalVarStatement () {
  delete Decl; Decl = nullptr;
}


//==========================================================================
//
//  VLocalVarStatement::SyntaxCopy
//
//==========================================================================
VStatement *VLocalVarStatement::SyntaxCopy () {
  auto res = new VLocalVarStatement();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VLocalVarStatement::DoSyntaxCopyTo
//
//==========================================================================
void VLocalVarStatement::DoSyntaxCopyTo (VStatement *e) {
  VBaseCompoundStatement::DoSyntaxCopyTo(e);
  auto res = (VLocalVarStatement *)e;
  res->Decl = (VLocalDecl *)(Decl ? Decl->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VLocalVarStatement::BeforeResolveStatements
//
//==========================================================================
bool VLocalVarStatement::BeforeResolveStatements (VEmitContext &ec) {
  return Decl->Declare(ec);
}


//==========================================================================
//
//  VLocalVarStatement::AfterResolveStatements
//
//==========================================================================
bool VLocalVarStatement::AfterResolveStatements (VEmitContext &ec) {
  Decl->Hide(ec);
  return true;
}


//==========================================================================
//
//  VLocalVarStatement::BeforeEmitStatements
//
//==========================================================================
bool VLocalVarStatement::BeforeEmitStatements (VEmitContext &ec) {
  //GLog.Logf(NAME_Debug, "%s: VLocalVarStatement::DoEmit: %s", *Loc.toStringNoCol(), *toString());
  Decl->Allocate(ec);
  // check if we are in some loop
  bool inloop = false;
  for (VStatement *st = this->UpScope; st; st = st->UpScope) {
    if (st->IsInLoop()) {
      inloop = true;
      break;
    }
  }
  if (inloop && !ec.InLoop) VCFatalError("VLocalVarStatement::BeforeEmitStatements: unbalanced loops");
  Decl->EmitInitialisations(ec, inloop);
  return true;
}


//==========================================================================
//
//  VLocalVarStatement::EmitDtor
//
//==========================================================================
void VLocalVarStatement::EmitDtor (VEmitContext &ec, bool properLeave) {
  Decl->EmitDtors(ec);
  // if leaving properly, release locals
  if (properLeave) Decl->Release(ec);
}


//==========================================================================
//
//  VLocalVarStatement::IsVarDecl
//
//==========================================================================
bool VLocalVarStatement::IsVarDecl () const noexcept {
  return true;
}


//==========================================================================
//
//  VLocalVarStatement::toString
//
//==========================================================================
VStr VLocalVarStatement::toString () {
  VStr res = VStr("/*")+Loc.toStringNoCol()+GET_MY_TYPE()+"*/<LOCDECL>\n";
  if (Decl) {
    for (auto &&v : Decl->Vars) {
      res += VStr("/*")+v.Loc.toStringNoCol()+"*/ ";
      res += VExpression::e2s(v.TypeExpr);
      res += " ";
      res += *v.Name;
      if (v.locIdx) {
        res += VStr("{")+VStr(v.locIdx)+"}";
      }
      if (v.Value) {
        res += "<init:";
        res += VExpression::e2s(v.Value);
        res += ">";
      }
      if (v.TypeOfExpr) {
        res += "<typeof:";
        res += VExpression::e2s(v.TypeOfExpr);
        res += ">";
      }
      res += ";";
      res += "\n";
    }
  }
  res += VBaseCompoundStatement::toString();
  return res;
}



//**************************************************************************
//
// VCompound
//
//**************************************************************************

//==========================================================================
//
//  VCompound::VCompound
//
//==========================================================================
VCompound::VCompound (const TLocation &ALoc) : VBaseCompoundStatement(ALoc) {
}


//==========================================================================
//
//  VCompound::IsCompound
//
//==========================================================================
bool VCompound::IsCompound () const noexcept {
  return true;
}


//==========================================================================
//
//  VCompound::SyntaxCopy
//
//==========================================================================
VStatement *VCompound::SyntaxCopy () {
  auto res = new VCompound();
  DoSyntaxCopyTo(res);
  return res;
}
