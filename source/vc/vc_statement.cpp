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

#include "vc_local.h"


// ////////////////////////////////////////////////////////////////////////// //
// VStatement
VStatement::VStatement (const TLocation &ALoc) : Loc(ALoc) {}
VStatement::~VStatement () {}
void VStatement::Emit (VEmitContext &ec) { DoEmit(ec); }
bool VStatement::IsBreak () { return false; }
bool VStatement::IsContinue () { return false; }
bool VStatement::IsReturn () { return false; }
bool VStatement::IsSwitchCase () { return false; }
bool VStatement::IsSwitchDefault () { return false; }
bool VStatement::IsEndsWithReturn () { return false; }
void VStatement::DoSyntaxCopyTo (VStatement *e) { e->Loc = Loc; }
void VStatement::DoFixSwitch (VSwitch *aold, VSwitch *anew) {}


// ////////////////////////////////////////////////////////////////////////// //
// VEmptyStatement
VEmptyStatement::VEmptyStatement (const TLocation &ALoc) : VStatement(ALoc) {}
VStatement *VEmptyStatement::SyntaxCopy () { auto res = new VEmptyStatement(); DoSyntaxCopyTo(res); return res; }
bool VEmptyStatement::Resolve (VEmitContext &) { return true; }
bool VEmptyStatement::IsEndsWithReturn () { return false; }
void VEmptyStatement::DoEmit (VEmitContext &) {}


//==========================================================================
//
//  VIf::VIf
//
//==========================================================================
VIf::VIf (VExpression *AExpr, VStatement *ATrueStatement, const TLocation &ALoc)
  : VStatement(ALoc)
  , Expr(AExpr)
  , TrueStatement(ATrueStatement)
  , FalseStatement(nullptr)
{
}


//==========================================================================
//
//  VIf::VIf
//
//==========================================================================
VIf::VIf (VExpression *AExpr, VStatement *ATrueStatement, VStatement *AFalseStatement, const TLocation &ALoc)
  : VStatement(ALoc)
  , Expr(AExpr)
  , TrueStatement(ATrueStatement)
  , FalseStatement(AFalseStatement)
{
}


//==========================================================================
//
//  VIf::~VIf
//
//==========================================================================
VIf::~VIf () {
  if (Expr) { delete Expr; Expr = nullptr; }
  if (TrueStatement) { delete TrueStatement; TrueStatement = nullptr; }
  if (FalseStatement) { delete FalseStatement; FalseStatement = nullptr; }
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
//  VIf::Resolve
//
//==========================================================================
bool VIf::Resolve (VEmitContext &ec) {
  bool Ret = true;
  if (Expr) Expr = Expr->ResolveBoolean(ec);
  if (!Expr) Ret = false;
  if (!TrueStatement->Resolve(ec)) Ret = false;
  if (FalseStatement && !FalseStatement->Resolve(ec)) Ret = false;
  return Ret;
}


//==========================================================================
//
//  VIf::DoEmit
//
//==========================================================================
void VIf::DoEmit (VEmitContext &ec) {
  VLabel FalseTarget = ec.DefineLabel();

  // expression
  Expr->EmitBranchable(ec, FalseTarget, false);

  // true statement
  TrueStatement->Emit(ec);
  if (FalseStatement) {
    // false statement
    VLabel End = ec.DefineLabel();
    ec.AddStatement(OPC_Goto, End, Loc);
    ec.MarkLabel(FalseTarget);
    FalseStatement->Emit(ec);
    ec.MarkLabel(End);
  } else {
    ec.MarkLabel(FalseTarget);
  }
}


//==========================================================================
//
//  VIf::IsEndsWithReturn
//
//==========================================================================
bool VIf::IsEndsWithReturn () {
  if (TrueStatement && FalseStatement) return (TrueStatement->IsEndsWithReturn() && FalseStatement->IsEndsWithReturn());
  return false;
}


//==========================================================================
//
//  VWhile::VWhile
//
//==========================================================================
VWhile::VWhile (VExpression *AExpr, VStatement *AStatement, const TLocation &ALoc)
  : VStatement(ALoc)
  , Expr(AExpr)
  , Statement(AStatement)
{
}


//==========================================================================
//
//  VWhile::~VWhile
//
//==========================================================================
VWhile::~VWhile () {
  if (Expr) { delete Expr; Expr = nullptr; }
  if (Statement) { delete Statement; Statement = nullptr; }
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
  res->Statement = (Statement ? Statement->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VWhile::DoFixSwitch
//
//==========================================================================
void VWhile::DoFixSwitch (VSwitch *aold, VSwitch *anew) {
  if (Statement) Statement->DoFixSwitch(aold, anew);
}


//==========================================================================
//
//  VWhile::Resolve
//
//==========================================================================
bool VWhile::Resolve (VEmitContext &ec) {
  bool Ret = true;
  if (Expr) Expr = Expr->ResolveBoolean(ec);
  if (!Expr) Ret = false;
  if (!Statement->Resolve(ec)) Ret = false;
  return Ret;
}


//==========================================================================
//
//  VWhile::DoEmit
//
//==========================================================================
void VWhile::DoEmit (VEmitContext &ec) {
  VLabel OldStart = ec.LoopStart;
  VLabel OldEnd = ec.LoopEnd;

  VLabel Loop = ec.DefineLabel();
  ec.LoopStart = ec.DefineLabel();
  ec.LoopEnd = ec.DefineLabel();

  ec.AddStatement(OPC_Goto, ec.LoopStart, Loc);
  ec.MarkLabel(Loop);
  Statement->Emit(ec);
  ec.MarkLabel(ec.LoopStart);
  Expr->EmitBranchable(ec, Loop, true);
  ec.MarkLabel(ec.LoopEnd);

  ec.LoopStart = OldStart;
  ec.LoopEnd = OldEnd;
}


//==========================================================================
//
//  VWhile::IsEndsWithReturn
//
//==========================================================================
bool VWhile::IsEndsWithReturn () {
  return (Statement && Statement->IsEndsWithReturn());
}


//==========================================================================
//
//  VDo::VDo
//
//==========================================================================
VDo::VDo (VExpression *AExpr, VStatement *AStatement, const TLocation &ALoc)
  : VStatement(ALoc)
  , Expr(AExpr)
  , Statement(AStatement)
{
}


//==========================================================================
//
//  VDo::~VDo
//
//==========================================================================
VDo::~VDo () {
  if (Expr) { delete Expr; Expr = nullptr; }
  if (Statement) { delete Statement; Statement = nullptr; }
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
  res->Statement = (Statement ? Statement->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDo::DoFixSwitch
//
//==========================================================================
void VDo::DoFixSwitch (VSwitch *aold, VSwitch *anew) {
  if (Statement) Statement->DoFixSwitch(aold, anew);
}


//==========================================================================
//
//  VDo::Resolve
//
//==========================================================================
bool VDo::Resolve (VEmitContext &ec) {
  bool Ret = true;
  if (Expr) Expr = Expr->ResolveBoolean(ec);
  if (!Expr) Ret = false;
  if (!Statement->Resolve(ec)) Ret = false;
  return Ret;
}


//==========================================================================
//
//  VDo::DoEmit
//
//==========================================================================
void VDo::DoEmit (VEmitContext &ec) {
  VLabel OldStart = ec.LoopStart;
  VLabel OldEnd = ec.LoopEnd;

  VLabel Loop = ec.DefineLabel();
  ec.LoopStart = ec.DefineLabel();
  ec.LoopEnd = ec.DefineLabel();

  ec.MarkLabel(Loop);
  Statement->Emit(ec);
  ec.MarkLabel(ec.LoopStart);
  Expr->EmitBranchable(ec, Loop, true);
  ec.MarkLabel(ec.LoopEnd);

  ec.LoopStart = OldStart;
  ec.LoopEnd = OldEnd;
}


//==========================================================================
//
//  VDo::IsEndsWithReturn
//
//==========================================================================
bool VDo::IsEndsWithReturn () {
  return (Statement && Statement->IsEndsWithReturn());
}


//==========================================================================
//
//  VFor::VFor
//
//==========================================================================
VFor::VFor (const TLocation &ALoc)
  : VStatement(ALoc)
  , InitExpr()
  , CondExpr()
  , LoopExpr()
  , Statement(nullptr)
{
}


//==========================================================================
//
//  VFor::~VFor
//
//==========================================================================
VFor::~VFor () {
  for (int i = 0; i < InitExpr.length(); ++i) {
    if (InitExpr[i]) { delete InitExpr[i]; InitExpr[i] = nullptr; }
  }
  for (int i = 0; i < CondExpr.length(); ++i) {
    if (CondExpr[i]) { delete CondExpr[i]; CondExpr[i] = nullptr; }
  }
  for (int i = 0; i < LoopExpr.length(); ++i) {
    if (LoopExpr[i]) { delete LoopExpr[i]; LoopExpr[i] = nullptr; }
  }
  if (Statement) { delete Statement; Statement = nullptr; }
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
  res->InitExpr.SetNum(InitExpr.length());
  for (int f = 0; f < InitExpr.length(); ++f) res->InitExpr[f] = (InitExpr[f] ? InitExpr[f]->SyntaxCopy() : nullptr);
  res->CondExpr.SetNum(CondExpr.length());
  for (int f = 0; f < CondExpr.length(); ++f) res->CondExpr[f] = (CondExpr[f] ? CondExpr[f]->SyntaxCopy() : nullptr);
  res->LoopExpr.SetNum(LoopExpr.length());
  for (int f = 0; f < LoopExpr.length(); ++f) res->LoopExpr[f] = (LoopExpr[f] ? LoopExpr[f]->SyntaxCopy() : nullptr);
  res->Statement = (Statement ? Statement->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VFor::DoFixSwitch
//
//==========================================================================
void VFor::DoFixSwitch (VSwitch *aold, VSwitch *anew) {
  if (Statement) Statement->DoFixSwitch(aold, anew);
}


//==========================================================================
//
//  VFor::Resolve
//
//==========================================================================
bool VFor::Resolve (VEmitContext &ec) {
  bool Ret = true;

  for (int i = 0; i < InitExpr.length(); ++i) {
    InitExpr[i] = InitExpr[i]->Resolve(ec);
    if (!InitExpr[i]) Ret = false;
  }

  for (int i = 0; i < CondExpr.length(); ++i) {
    if (i != CondExpr.length()-1) {
      CondExpr[i] = CondExpr[i]->Resolve(ec);
    } else {
      CondExpr[i] = CondExpr[i]->ResolveBoolean(ec);
    }
    if (!CondExpr[i]) Ret = false;
  }

  for (int i = 0; i < LoopExpr.length(); ++i) {
    LoopExpr[i] = LoopExpr[i]->Resolve(ec);
    if (!LoopExpr[i]) Ret = false;
  }

  if (!Statement->Resolve(ec)) Ret = false;

  return Ret;
}


//==========================================================================
//
//  VFor::DoEmit
//
//==========================================================================
void VFor::DoEmit (VEmitContext &ec) {
  // set-up continues and breaks
  VLabel OldStart = ec.LoopStart;
  VLabel OldEnd = ec.LoopEnd;

  // define labels
  ec.LoopStart = ec.DefineLabel();
  ec.LoopEnd = ec.DefineLabel();
  VLabel Test = ec.DefineLabel();
  VLabel Loop = ec.DefineLabel();

  // emit initialisation expressions
  for (int i = 0; i < InitExpr.length(); ++i) InitExpr[i]->Emit(ec);

  // jump to test if it's present
  if (CondExpr.length()) ec.AddStatement(OPC_Goto, Test, Loc);

  // emit embeded statement
  ec.MarkLabel(Loop);
  Statement->Emit(ec);

  // emit per-loop expression statements
  ec.MarkLabel(ec.LoopStart);
  for (int i = 0; i < LoopExpr.length(); ++i) LoopExpr[i]->Emit(ec);

  // loop test
  ec.MarkLabel(Test);
  if (CondExpr.length() == 0) {
    ec.AddStatement(OPC_Goto, Loop, Loc);
  } else {
    for (int i = 0; i < CondExpr.length()-1; ++i) CondExpr[i]->Emit(ec);
    CondExpr[CondExpr.length()-1]->EmitBranchable(ec, Loop, true);
  }

  // end of loop
  ec.MarkLabel(ec.LoopEnd);

  // restore continue and break state
  ec.LoopStart = OldStart;
  ec.LoopEnd = OldEnd;
}


//==========================================================================
//
//  VFor::IsEndsWithReturn
//
//==========================================================================
bool VFor::IsEndsWithReturn () {
  //TODO: endless fors should have at least one return instead
  return (Statement && Statement->IsEndsWithReturn());
}


//==========================================================================
//
//  VForeach::VForeach
//
//==========================================================================
VForeach::VForeach (VExpression *AExpr, VStatement *AStatement, const TLocation &ALoc)
  : VStatement(ALoc)
  , Expr(AExpr)
  , Statement(AStatement)
{
}

//==========================================================================
//
//  VForeach::~VForeach
//
//==========================================================================
VForeach::~VForeach () {
  if (Expr) { delete Expr; Expr = nullptr; }
  if (Statement) { delete Statement; Statement = nullptr; }
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
  res->Statement = (Statement ? Statement->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VForeach::DoFixSwitch
//
//==========================================================================
void VForeach::DoFixSwitch (VSwitch *aold, VSwitch *anew) {
  if (Statement) Statement->DoFixSwitch(aold, anew);
}


//==========================================================================
//
//  VForeach::Resolve
//
//==========================================================================
bool VForeach::Resolve (VEmitContext &ec) {
  bool Ret = true;
  if (Expr) Expr = Expr->ResolveIterator(ec);
  if (!Expr) Ret = false;
  if (!Statement->Resolve(ec)) Ret = false;
  return Ret;
}


//==========================================================================
//
//  VForeach::DoEmit
//
//==========================================================================
void VForeach::DoEmit (VEmitContext &ec) {
  VLabel OldStart = ec.LoopStart;
  VLabel OldEnd = ec.LoopEnd;

  Expr->Emit(ec);
  ec.AddStatement(OPC_IteratorInit, Loc);

  VLabel Loop = ec.DefineLabel();
  ec.LoopStart = ec.DefineLabel();
  ec.LoopEnd = ec.DefineLabel();

  ec.AddStatement(OPC_Goto, ec.LoopStart, Loc);
  ec.MarkLabel(Loop);
  Statement->Emit(ec);
  ec.MarkLabel(ec.LoopStart);
  ec.AddStatement(OPC_IteratorNext, Loc);
  ec.AddStatement(OPC_IfGoto, Loop, Loc);
  ec.MarkLabel(ec.LoopEnd);
  ec.AddStatement(OPC_IteratorPop, Loc);

  ec.LoopStart = OldStart;
  ec.LoopEnd = OldEnd;
}


//==========================================================================
//
//  VForeach::IsEndsWithReturn
//
//==========================================================================
bool VForeach::IsEndsWithReturn () {
  return (Statement && Statement->IsEndsWithReturn());
}


//==========================================================================
//
//  VForeachIota::VForeachIota
//
//==========================================================================
VForeachIota::VForeachIota (const TLocation &ALoc)
  : VStatement(ALoc)
  , varinit(nullptr)
  , varnext(nullptr)
  , hiinit(nullptr)
  , var(nullptr)
  , lo(nullptr)
  , hi(nullptr)
  , statement(nullptr)
{
}


//==========================================================================
//
//  VForeachIota::~VForeachIota
//
//==========================================================================
VForeachIota::~VForeachIota () {
  delete varinit; varinit = nullptr;
  delete varnext; varnext = nullptr;
  delete hiinit; hiinit = nullptr;
  delete var; var = nullptr;
  delete lo; lo = nullptr;
  delete hi; hi = nullptr;
  delete statement; statement = nullptr;
}


//==========================================================================
//
//  VForeachIota::SyntaxCopy
//
//==========================================================================
VStatement *VForeachIota::SyntaxCopy () {
  if (varinit || varnext || hiinit) FatalError("VC: `VForeachIota::SyntaxCopy()` called on resolved statement");
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
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VForeachIota *)e;
  res->var = (var ? var->SyntaxCopy() : nullptr);
  res->lo = (lo ? lo->SyntaxCopy() : nullptr);
  res->hi = (hi ? hi->SyntaxCopy() : nullptr);
  res->statement = (statement ? statement->SyntaxCopy() : nullptr);
  // no need to copy private data here, as `SyntaxCopy()` should be called only on unresolved things
}


//==========================================================================
//
//  VForeachIota::DoFixSwitch
//
//==========================================================================
void VForeachIota::DoFixSwitch (VSwitch *aold, VSwitch *anew) {
  if (statement) statement->DoFixSwitch(aold, anew);
}


//==========================================================================
//
//  VForeachIota::Resolve
//
//==========================================================================
bool VForeachIota::Resolve (VEmitContext &ec) {
  // we will rewrite 'em later
  auto varR = (var ? var->SyntaxCopy()->Resolve(ec) : nullptr);
  auto loR = (lo ? lo->SyntaxCopy()->Resolve(ec) : nullptr);
  auto hiR = (hi ? hi->SyntaxCopy()->Resolve(ec) : nullptr);
  if (!statement->Resolve(ec) || !varR || !loR || !hiR) {
    delete varR;
    delete loR;
    delete hiR;
    return false;
  }

  if (varR->Type.Type != TYPE_Int) {
    ParseError(var->Loc, "Loop variable should be integer (got `%s`)", *varR->Type.GetName());
    delete varR;
    delete loR;
    delete hiR;
    return false;
  }

  if (loR->Type.Type != TYPE_Int) {
    ParseError(lo->Loc, "Loop lower bound should be integer (got `%s`)", *loR->Type.GetName());
    delete varR;
    delete loR;
    delete hiR;
    return false;
  }

  if (hiR->Type.Type != TYPE_Int) {
    ParseError(hi->Loc, "Loop higher bound should be integer (got `%s`)", *hiR->Type.GetName());
    delete varR;
    delete loR;
    delete hiR;
    return false;
  }

  // we don't need 'em anymore
  delete varR;
  delete loR;
  delete hiR;

  // create hidden local for higher bound
  VFieldType Type = VFieldType(TYPE_Int);
  VLocalVarDef &L = ec.AllocLocal(NAME_None, Type, hi->Loc);
  L.Visible = false; // it is unnamed, and hidden ;-)
  L.Reusable = true; // mark it as reusable, as statement is aready resolved
  L.ParamFlags = 0;

  // initialize hidden local with higher bound
  hiinit = new VAssignment(VAssignment::Assign, new VLocalVar(L.ldindex, hi->Loc), hi->SyntaxCopy(), hi->Loc);
  hiinit = hiinit->Resolve(ec);
  if (!hiinit) return false; // oops

  // create initializer expression: `var = lo`
  varinit = new VAssignment(VAssignment::Assign, var->SyntaxCopy(), lo->SyntaxCopy(), hi->Loc);
  varinit = varinit->Resolve(ec);
  if (!varinit) return false; // oops

  // create loop expression: `++var`
  varnext = new VUnaryMutator(VUnaryMutator::PreInc, var->SyntaxCopy(), hi->Loc);
  varnext = new VDropResult(varnext);
  varnext = varnext->Resolve(ec);
  if (!varnext) return false; // oops

  // create condition expression: `var < hivar`
  var = new VBinary(VBinary::EBinOp::Less, var, new VLocalVar(L.ldindex, hi->Loc), hi->Loc);
  var = var->ResolveBoolean(ec);
  if (!var) return false; // oops

  return true;
}


//==========================================================================
//
//  VForeachIota::DoEmit
//
//==========================================================================
void VForeachIota::DoEmit (VEmitContext &ec) {
  // set-up continues and breaks
  VLabel OldStart = ec.LoopStart;
  VLabel OldEnd = ec.LoopEnd;

  // define labels
  ec.LoopStart = ec.DefineLabel();
  ec.LoopEnd = ec.DefineLabel();

  VLabel Test = ec.DefineLabel();
  VLabel Loop = ec.DefineLabel();

  // emit initialisation expressions
  hiinit->Emit(ec);
  varinit->Emit(ec);

  // jump to test
  ec.AddStatement(OPC_Goto, Test, Loc);

  // emit embeded statement
  ec.MarkLabel(Loop);
  statement->Emit(ec);

  // emit per-loop expression statements
  ec.MarkLabel(ec.LoopStart);
  varnext->Emit(ec);

  // loop test
  ec.MarkLabel(Test);
  var->EmitBranchable(ec, Loop, true);

  // end of loop
  ec.MarkLabel(ec.LoopEnd);

  // restore continue and break state
  ec.LoopStart = OldStart;
  ec.LoopEnd = OldEnd;
}


//==========================================================================
//
//  VForeachIota::IsEndsWithReturn
//
//==========================================================================
bool VForeachIota::IsEndsWithReturn () {
  //TODO: endless fors should have at least one return instead
  return (statement && statement->IsEndsWithReturn());
}


//==========================================================================
//
//  VSwitch::VSwitch
//
//==========================================================================
VSwitch::VSwitch (const TLocation &ALoc)
  : VStatement(ALoc)
  , HaveDefault(false)
{
}

//==========================================================================
//
//  VSwitch::~VSwitch
//
//==========================================================================
VSwitch::~VSwitch () {
  if (Expr) { delete Expr; Expr = nullptr; }
  for (int i = 0; i < Statements.length(); ++i) {
    if (Statements[i]) { delete Statements[i]; Statements[i] = nullptr; }
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
  res->CaseInfo.SetNum(CaseInfo.length());
  for (int f = 0; f < CaseInfo.length(); ++f) res->CaseInfo[f] = CaseInfo[f];
  res->DefaultAddress = DefaultAddress;
  res->Statements.SetNum(Statements.length());
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
  for (int f = 0; f < Statements.length(); ++f) if (Statements[f]) Statements[f]->DoFixSwitch(aold, anew);
}


//==========================================================================
//
//  VSwitch::Resolve
//
//==========================================================================
bool VSwitch::Resolve (VEmitContext &ec) {
  bool Ret = true;

  if (Expr) Expr = Expr->Resolve(ec);

  if (!Expr || Expr->Type.Type != TYPE_Int) {
    ParseError(Loc, "Int expression expected");
    Ret = false;
  }

  for (int i = 0; i < Statements.length(); ++i) {
    if (!Statements[i]->Resolve(ec)) Ret = false;
  }

  return Ret;
}


//==========================================================================
//
//  VSwitch::DoEmit
//
//==========================================================================
void VSwitch::DoEmit (VEmitContext &ec) {
  VLabel OldEnd = ec.LoopEnd;

  Expr->Emit(ec);

  ec.LoopEnd = ec.DefineLabel();

  // case table
  for (int i = 0; i < CaseInfo.length(); ++i) {
    CaseInfo[i].Address = ec.DefineLabel();
    if (CaseInfo[i].Value >= 0 && CaseInfo[i].Value < 256) {
      ec.AddStatement(OPC_CaseGotoB, CaseInfo[i].Value, CaseInfo[i].Address, Loc);
    } else if (CaseInfo[i].Value >= MIN_VINT16 && CaseInfo[i].Value < MAX_VINT16) {
      ec.AddStatement(OPC_CaseGotoS, CaseInfo[i].Value, CaseInfo[i].Address, Loc);
    } else {
      ec.AddStatement(OPC_CaseGoto, CaseInfo[i].Value, CaseInfo[i].Address, Loc);
    }
  }
  ec.AddStatement(OPC_Drop, Loc);

  // go to default case if we have one, otherwise to the end of switch
  if (HaveDefault) {
    DefaultAddress = ec.DefineLabel();
    ec.AddStatement(OPC_Goto, DefaultAddress, Loc);
  } else {
    ec.AddStatement(OPC_Goto, ec.LoopEnd, Loc);
  }

  // switch statements
  for (int i = 0; i < Statements.length(); ++i) Statements[i]->Emit(ec);

  ec.MarkLabel(ec.LoopEnd);

  ec.LoopEnd = OldEnd;
}


//==========================================================================
//
//  VSwitch::IsEndsWithReturn
//
//==========================================================================
bool VSwitch::IsEndsWithReturn () {
  if (Statements.length() == 0) return false;
  bool defautSeen = false;
  bool returnSeen = false;
  bool breakSeen = false;
  bool statementSeen = false;
  for (int n = 0; n < Statements.length(); ++n) {
    if (!Statements[n]) return false; //k8: orly?
    // `case` or `default`?
    if (Statements[n]->IsSwitchCase() || Statements[n]->IsSwitchDefault()) {
      if (!returnSeen && statementSeen) return false; // oops
      if (Statements[n]->IsSwitchDefault()) defautSeen = true;
      breakSeen = false;
      statementSeen = true;
      returnSeen = false;
      continue;
    }
    if (breakSeen) continue;
    statementSeen = true;
    if (Statements[n]->IsBreak() || Statements[n]->IsContinue()) {
      // `break`/`continue`
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


//==========================================================================
//
//  VSwitchCase::VSwitchCase
//
//==========================================================================
VSwitchCase::VSwitchCase (VSwitch *ASwitch, VExpression *AExpr, const TLocation &ALoc)
  : VStatement(ALoc)
  , Switch(ASwitch)
  , Expr(AExpr)
{
}


//==========================================================================
//
//  VSwitchCase::~VSwitchCase
//
//==========================================================================
VSwitchCase::~VSwitchCase () {
  if (Expr) { delete Expr; Expr = nullptr; }
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
//  VSwitchCase::Resolve
//
//==========================================================================
bool VSwitchCase::Resolve (VEmitContext &ec) {
  if (Expr) Expr = Expr->Resolve(ec);
  if (!Expr) return false;

  if (!Expr->IsIntConst()) {
    ParseError(Expr->Loc, "Integer constant expected");
    return false;
  }

  Value = Expr->GetIntConst();
  for (int i = 0; i < Switch->CaseInfo.length(); ++i) {
    if (Switch->CaseInfo[i].Value == Value) {
      ParseError(Loc, "Duplicate case value");
      break;
    }
  }

  Index = Switch->CaseInfo.length();
  VSwitch::VCaseInfo &C = Switch->CaseInfo.Alloc();
  C.Value = Value;

  return true;
}


//==========================================================================
//
//  VSwitchCase::DoEmit
//
//==========================================================================
void VSwitchCase::DoEmit (VEmitContext &ec) {
  ec.MarkLabel(Switch->CaseInfo[Index].Address);
}


//==========================================================================
//
//  VSwitchCase::IsCase
//
//==========================================================================
bool VSwitchCase::IsSwitchCase () {
  return true;
}


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
//  VSwitchDefault::Resolve
//
//==========================================================================
bool VSwitchDefault::Resolve (VEmitContext &) {
  if (Switch->HaveDefault) {
    ParseError(Loc, "Only one `default` per switch allowed");
    return false;
  }
  Switch->HaveDefault = true;
  return true;
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
bool VSwitchDefault::IsSwitchDefault () {
  return true;
}


//==========================================================================
//
//  VBreak::VBreak
//
//==========================================================================
VBreak::VBreak (const TLocation &ALoc) : VStatement(ALoc)
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
//  VBreak::Resolve
//
//==========================================================================
bool VBreak::Resolve (VEmitContext &) {
  return true;
}


//==========================================================================
//
//  VBreak::DoEmit
//
//==========================================================================
void VBreak::DoEmit (VEmitContext &ec) {
  if (!ec.LoopEnd.IsDefined()) {
    ParseError(Loc, "Misplaced `break` statement");
    return;
  }
  ec.AddStatement(OPC_Goto, ec.LoopEnd, Loc);
}


//==========================================================================
//
//  VBreak::IsBreak
//
//==========================================================================
bool VBreak::IsBreak () {
  return true;
}


//==========================================================================
//
//  VContinue::VContinue
//
//==========================================================================
VContinue::VContinue (const TLocation &ALoc) : VStatement(ALoc)
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
//  VContinue::Resolve
//
//==========================================================================
bool VContinue::Resolve (VEmitContext &) {
  return true;
}


//==========================================================================
//
//  VContinue::DoEmit
//
//==========================================================================
void VContinue::DoEmit (VEmitContext &ec) {
  if (!ec.LoopStart.IsDefined()) {
    ParseError(Loc, "Misplaced `continue` statement");
    return;
  }
  ec.AddStatement(OPC_Goto, ec.LoopStart, Loc);
}


//==========================================================================
//
//  VContinue::IsContinue
//
//==========================================================================
bool VContinue::IsContinue () {
  return true;
}


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
  if (Expr) { delete Expr; Expr = nullptr; }
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
//  VReturn::Resolve
//
//==========================================================================
bool VReturn::Resolve (VEmitContext &ec) {
  NumLocalsToClear = ec.GetLocalDefCount();
  bool Ret = true;
  if (Expr) {
    Expr = (ec.FuncRetType.Type == TYPE_Float ? Expr->ResolveFloat(ec) : Expr->Resolve(ec));
    if (ec.FuncRetType.Type == TYPE_Void) {
      ParseError(Loc, "void function cannot return a value");
      Ret = false;
    } else if (Expr) {
      Expr->Type.CheckMatch(Expr->Loc, ec.FuncRetType);
    } else {
      Ret = false;
    }
  } else {
    if (ec.FuncRetType.Type != TYPE_Void) {
      ParseError(Loc, "Return value expected");
      Ret = false;
    }
  }
  return Ret;
}


//==========================================================================
//
//  VReturn::DoEmit
//
//==========================================================================
void VReturn::DoEmit (VEmitContext &ec) {
  if (Expr) {
    Expr->Emit(ec);
    ec.EmitClearStrings(0, NumLocalsToClear, Loc);
    if (Expr->Type.GetStackSize() == 4) {
      ec.AddStatement(OPC_ReturnL, Loc);
    } else if (Expr->Type.Type == TYPE_Vector) {
      ec.AddStatement(OPC_ReturnV, Loc);
    } else {
      ParseError(Loc, "Bad return type");
    }
  } else {
    ec.EmitClearStrings(0, NumLocalsToClear, Loc);
    ec.AddStatement(OPC_Return, Loc);
  }
}


//==========================================================================
//
//  VReturn::IsReturn
//
//==========================================================================
bool VReturn::IsReturn () {
  return true;
}


//==========================================================================
//
//  VReturn::IsEndsWithReturn
//
//==========================================================================
bool VReturn::IsEndsWithReturn () {
  return true;
}


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
  if (Expr) { delete Expr; Expr = nullptr; }
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
//  VExpressionStatement::Resolve
//
//==========================================================================
bool VExpressionStatement::Resolve (VEmitContext &ec) {
  bool Ret = true;
  if (Expr) Expr = Expr->Resolve(ec);
  if (!Expr) Ret = false;
  return Ret;
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
//  VLocalVarStatement::VLocalVarStatement
//
//==========================================================================
VLocalVarStatement::VLocalVarStatement (VLocalDecl *ADecl)
  : VStatement(ADecl->Loc)
  , Decl(ADecl)
{
}


//==========================================================================
//
//  VLocalVarStatement::~VLocalVarStatement
//
//==========================================================================
VLocalVarStatement::~VLocalVarStatement () {
  if (Decl) { delete Decl; Decl = nullptr; }
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
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VLocalVarStatement *)e;
  res->Decl = (VLocalDecl *)(Decl ? Decl->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VLocalVarStatement::Resolve
//
//==========================================================================
bool VLocalVarStatement::Resolve (VEmitContext &ec) {
  bool Ret = true;
  Decl->Declare(ec);
  return Ret;
}


//==========================================================================
//
//  VLocalVarStatement::DoEmit
//
//==========================================================================
void VLocalVarStatement::DoEmit (VEmitContext &ec) {
  Decl->EmitInitialisations(ec);
}


//==========================================================================
//
//  VCompound::VCompound
//
//==========================================================================
VCompound::VCompound (const TLocation &ALoc) : VStatement(ALoc)
{
}


//==========================================================================
//
//  VCompound::~VCompound
//
//==========================================================================
VCompound::~VCompound () {
  for (int i = 0; i < Statements.length(); ++i) {
    if (Statements[i]) { delete Statements[i]; Statements[i] = nullptr; }
  }
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


//==========================================================================
//
//  VCompound::DoSyntaxCopyTo
//
//==========================================================================
void VCompound::DoSyntaxCopyTo (VStatement *e) {
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VCompound *)e;
  res->Statements.SetNum(Statements.length());
  for (int f = 0; f < Statements.length(); ++f) res->Statements[f] = (Statements[f] ? Statements[f]->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VCompound::DoFixSwitch
//
//==========================================================================
void VCompound::DoFixSwitch (VSwitch *aold, VSwitch *anew) {
  for (int f = 0; f < Statements.length(); ++f) if (Statements[f]) Statements[f]->DoFixSwitch(aold, anew);
}


//==========================================================================
//
//  VCompound::Resolve
//
//==========================================================================
bool VCompound::Resolve (VEmitContext &ec) {
  bool Ret = true;
  auto cidx = ec.EnterCompound();
  //fprintf(stderr, "ENTERING COMPOUND %d (%s:%d)\n", cidx, *Loc.GetSource(), Loc.GetLine());
  for (int i = 0; i < Statements.length(); ++i) {
    if (!Statements[i]->Resolve(ec)) Ret = false;
  }
  //fprintf(stderr, "EXITING COMPOUND %d (%s:%d)\n", cidx, *Loc.GetSource(), Loc.GetLine());
  ec.ExitCompound(cidx);
  return Ret;
}


//==========================================================================
//
//  VCompound::DoEmit
//
//==========================================================================
void VCompound::DoEmit (VEmitContext &ec) {
  for (int i = 0; i < Statements.length(); ++i) Statements[i]->Emit(ec);
}


//==========================================================================
//
//  VCompound::IsEndsWithReturn
//
//==========================================================================
bool VCompound::IsEndsWithReturn () {
  for (int n = 0; n < Statements.length(); ++n) {
    if (!Statements[n]) continue;
    if (Statements[n]->IsEndsWithReturn()) return true;
    if (Statements[n]->IsBreak() || Statements[n]->IsContinue()) break;
  }
  return false;
}
