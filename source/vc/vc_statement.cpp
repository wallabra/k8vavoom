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
bool VStatement::IsLabel () const { return false; }
VName VStatement::GetLabelName () const { return NAME_None; }
bool VStatement::IsGoto () const { return false; }
bool VStatement::IsGotoCase () const { return false; }
bool VStatement::HasGotoCaseExpr () const { return false; }
bool VStatement::IsGotoDefault () const { return false; }
bool VStatement::IsBreak () const { return false; }
bool VStatement::IsContinue () const { return false; }
bool VStatement::IsFlowStop () const { return (IsBreak() || IsContinue() || IsGoto()); }
bool VStatement::IsReturn () const { return false; }
bool VStatement::IsSwitchCase () const { return false; }
bool VStatement::IsSwitchDefault () const { return false; }
bool VStatement::IsVarDecl () const { return false; }
bool VStatement::IsEndsWithReturn () const { return IsReturn(); }
bool VStatement::IsProperCaseEnd (bool skipBreak) const { if (IsReturn() || IsGotoCase() || IsGotoDefault()) return true; if (!skipBreak && (IsBreak() || IsContinue())) return true; return false; }
void VStatement::DoSyntaxCopyTo (VStatement *e) { e->Loc = Loc; }
void VStatement::DoFixSwitch (VSwitch *aold, VSwitch *anew) {}
VLabelStmt *VStatement::FindLabel (VName aname) { return (IsLabel() && GetLabelName() == aname ? (VLabelStmt *)this : nullptr); }
bool VStatement::IsGotoInAllowed () const { return true; }
bool VStatement::IsGotoOutAllowed () const { return true; }
bool VStatement::IsJumpOverAllowed (const VStatement *s0, const VStatement *s1) const { return true; }

bool VStatement::BuildPathTo (const VStatement *dest, TArray<VStatement *> &path) {
  if (dest == this) {
    path.append(this);
    return true;
  } else {
    return false;
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// VEmptyStatement
VEmptyStatement::VEmptyStatement (const TLocation &ALoc) : VStatement(ALoc) {}
VStatement *VEmptyStatement::SyntaxCopy () { auto res = new VEmptyStatement(); DoSyntaxCopyTo(res); return res; }
bool VEmptyStatement::Resolve (VEmitContext &) { return true; }
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
bool VIf::IsEndsWithReturn () const {
  if (TrueStatement && FalseStatement) return (TrueStatement->IsEndsWithReturn() && FalseStatement->IsEndsWithReturn());
  return false;
}


//==========================================================================
//
//  VIf::IsProperCaseEnd
//
//==========================================================================
bool VIf::IsProperCaseEnd (bool skipBreak) const {
  if (TrueStatement && FalseStatement) return (TrueStatement->IsProperCaseEnd(skipBreak) && FalseStatement->IsProperCaseEnd(skipBreak));
  return false;
}


//==========================================================================
//
//  VIf::FindLabel
//
//==========================================================================
VLabelStmt *VIf::FindLabel (VName aname) {
  VLabelStmt *lbl = (TrueStatement ? TrueStatement->FindLabel(aname) : nullptr);
  if (!lbl && FalseStatement) lbl = (FalseStatement->FindLabel(aname));
  return lbl;
}


//==========================================================================
//
//  VIf::BuildPathTo
//
//==========================================================================
bool VIf::BuildPathTo (const VStatement *dest, TArray<VStatement *> &path) {
  if (dest == this) { path.append(this); return true; }
  path.append(this);
  if (TrueStatement && TrueStatement->BuildPathTo(dest, path)) return true;
  if (FalseStatement && FalseStatement->BuildPathTo(dest, path)) return true;
  path.removeAt(path.length()-1);
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
bool VWhile::IsEndsWithReturn () const {
  return (Statement && Statement->IsEndsWithReturn());
}


//==========================================================================
//
//  VWhile::IsProperCaseEnd
//
//==========================================================================
bool VWhile::IsProperCaseEnd (bool skipBreak) const {
  return (Statement && Statement->IsProperCaseEnd(true));
}


//==========================================================================
//
//  VWhile::FindLabel
//
//==========================================================================
VLabelStmt *VWhile::FindLabel (VName aname) {
  return (Statement ? Statement->FindLabel(aname) : nullptr);
}


//==========================================================================
//
//  VWhile::BuildPathTo
//
//==========================================================================
bool VWhile::BuildPathTo (const VStatement *dest, TArray<VStatement *> &path) {
  if (dest == this) { path.append(this); return true; }
  path.append(this);
  if (Statement && Statement->BuildPathTo(dest, path)) return true;
  path.removeAt(path.length()-1);
  return false;
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
bool VDo::IsEndsWithReturn () const {
  return (Statement && Statement->IsEndsWithReturn());
}


//==========================================================================
//
//  VDo::IsProperCaseEnd
//
//==========================================================================
bool VDo::IsProperCaseEnd (bool skipBreak) const {
  return (Statement && Statement->IsProperCaseEnd(true));
}


//==========================================================================
//
//  VDo::FindLabel
//
//==========================================================================
VLabelStmt *VDo::FindLabel (VName aname) {
  return (Statement ? Statement->FindLabel(aname) : nullptr);
}


//==========================================================================
//
//  VDo::BuildPathTo
//
//==========================================================================
bool VDo::BuildPathTo (const VStatement *dest, TArray<VStatement *> &path) {
  if (dest == this) { path.append(this); return true; }
  path.append(this);
  if (Statement && Statement->BuildPathTo(dest, path)) return true;
  path.removeAt(path.length()-1);
  return false;
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
bool VFor::IsEndsWithReturn () const {
  //TODO: endless fors should have at least one return instead
  return (Statement && Statement->IsEndsWithReturn());
}


//==========================================================================
//
//  VFor::IsProperCaseEnd
//
//==========================================================================
bool VFor::IsProperCaseEnd (bool skipBreak) const {
  return (Statement && Statement->IsProperCaseEnd(true));
}


//==========================================================================
//
//  VFor::FindLabel
//
//==========================================================================
VLabelStmt *VFor::FindLabel (VName aname) {
  return (Statement ? Statement->FindLabel(aname) : nullptr);
}


//==========================================================================
//
//  VFor::IsGotoInAllowed
//
//==========================================================================
bool VFor::IsGotoInAllowed () const {
  return false;
}


//==========================================================================
//
//  VFor::BuildPathTo
//
//==========================================================================
bool VFor::BuildPathTo (const VStatement *dest, TArray<VStatement *> &path) {
  if (dest == this) { path.append(this); return true; }
  path.append(this);
  if (Statement && Statement->BuildPathTo(dest, path)) return true;
  path.removeAt(path.length()-1);
  return false;
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
bool VForeach::IsEndsWithReturn () const {
  return (Statement && Statement->IsEndsWithReturn());
}


//==========================================================================
//
//  VForeach::IsProperCaseEnd
//
//==========================================================================
bool VForeach::IsProperCaseEnd (bool skipBreak) const {
  return (Statement && Statement->IsProperCaseEnd(true));
}


//==========================================================================
//
//  VForeach::FindLabel
//
//==========================================================================
VLabelStmt *VForeach::FindLabel (VName aname) {
  return (Statement ? Statement->FindLabel(aname) : nullptr);
}


//==========================================================================
//
//  VForeach::IsGotoInAllowed
//
//==========================================================================
bool VForeach::IsGotoInAllowed () const {
  return false;
}


//==========================================================================
//
//  VForeach::IsGotoOutAllowed
//
//==========================================================================
bool VForeach::IsGotoOutAllowed () const {
  return false;
}


//==========================================================================
//
//  VForeach::BuildPathTo
//
//==========================================================================
bool VForeach::BuildPathTo (const VStatement *dest, TArray<VStatement *> &path) {
  if (dest == this) { path.append(this); return true; }
  path.append(this);
  if (Statement && Statement->BuildPathTo(dest, path)) return true;
  path.removeAt(path.length()-1);
  return false;
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
  , reversed(false)
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
  res->reversed = reversed;
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
  if (!statement || !varR || !loR || !hiR) {
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

  // create hidden local for higher bound (if necessary)
  VExpression *limit;
  if ((reversed ? loR : hiR)->IsIntConst()) {
    limit = new VIntLiteral((reversed ? loR : hiR)->GetIntConst(), (reversed ? lo : hi)->Loc);
  } else {
    VLocalVarDef &L = ec.AllocLocal(NAME_None, VFieldType(TYPE_Int), (reversed ? lo : hi)->Loc);
    L.Visible = false; // it is unnamed, and hidden ;-)
    L.ParamFlags = 0;
    limit = new VLocalVar(L.ldindex, L.Loc);
    // initialize hidden local with higher/lower bound
    hiinit = new VAssignment(VAssignment::Assign, limit->SyntaxCopy(), (reversed ? lo : hi)->SyntaxCopy(), L.Loc);
  }

  // we don't need 'em anymore
  delete varR;
  delete loR;
  delete hiR;

  if (hiinit) {
    hiinit = hiinit->Resolve(ec);
    if (!hiinit) { delete limit; return false; }
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
  if (!varinit) return false;

  varnext = varnext->ResolveBoolean(ec);
  if (!varnext) return false;

  var = var->ResolveBoolean(ec);
  if (!var) return false;

  // finally, resolve statement (last, so local reusing will work as expected)
  return statement->Resolve(ec);
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

  VLabel Loop = ec.DefineLabel();

  // emit initialisation expressions
  if (hiinit) hiinit->Emit(ec); // may be absent for iota with literals
  varinit->Emit(ec);

  // do first check
  var->EmitBranchable(ec, ec.LoopEnd, false);

  // emit embeded statement
  ec.MarkLabel(Loop);
  statement->Emit(ec);

  // loop next and test
  ec.MarkLabel(ec.LoopStart); // continue will jump here
  varnext->EmitBranchable(ec, Loop, true);

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
bool VForeachIota::IsEndsWithReturn () const {
  //TODO: endless fors should have at least one return instead
  return (statement && statement->IsEndsWithReturn());
}


//==========================================================================
//
//  VForeachIota::IsProperCaseEnd
//
//==========================================================================
bool VForeachIota::IsProperCaseEnd (bool skipBreak) const {
  return (statement && statement->IsProperCaseEnd(true));
}


//==========================================================================
//
//  VForeachIota::FindLabel
//
//==========================================================================
VLabelStmt *VForeachIota::FindLabel (VName aname) {
  return (statement ? statement->FindLabel(aname) : nullptr);
}


//==========================================================================
//
//  VForeachIota::IsGotoInAllowed
//
//==========================================================================
bool VForeachIota::IsGotoInAllowed () const {
  return false;
}


//==========================================================================
//
//  VForeachIota::BuildPathTo
//
//==========================================================================
bool VForeachIota::BuildPathTo (const VStatement *dest, TArray<VStatement *> &path) {
  if (dest == this) { path.append(this); return true; }
  path.append(this);
  if (statement && statement->BuildPathTo(dest, path)) return true;
  path.removeAt(path.length()-1);
  return false;
}



//==========================================================================
//
//  VForeachArray::VForeachArray
//
//==========================================================================
VForeachArray::VForeachArray (VExpression *aarr, VExpression *aidx, VExpression *avar, bool aVarRef, const TLocation &aloc)
  : VStatement(aloc)
  , idxinit(nullptr)
  , hiinit(nullptr)
  , loopPreCheck(nullptr)
  , loopNext(nullptr)
  , loopLoad(nullptr)
  , varaddr(nullptr)
  , idxvar(aidx)
  , var(avar)
  , arr(aarr)
  , statement(nullptr)
  , reversed(false)
  , isRef(aVarRef)
{
}


//==========================================================================
//
//  VForeachArray::~VForeachArray
//
//==========================================================================
VForeachArray::~VForeachArray () {
  delete idxinit; idxinit = nullptr;
  delete hiinit; hiinit = nullptr;
  delete loopPreCheck; loopPreCheck = nullptr;
  delete loopNext; loopNext = nullptr;
  delete loopLoad; loopLoad = nullptr;
  delete varaddr; varaddr = nullptr;
  delete idxvar; idxvar = nullptr;
  delete var; var = nullptr;
  delete arr; arr = nullptr;
  delete statement; statement = nullptr;
}


//==========================================================================
//
//  VForeachArray::SyntaxCopy
//
//==========================================================================
VStatement *VForeachArray::SyntaxCopy () {
  //if (varinit || varnext || hiinit) FatalError("VC: `VForeachArray::SyntaxCopy()` called on resolved statement");
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
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VForeachArray *)e;
  res->idxvar = (idxvar ? idxvar->SyntaxCopy() : nullptr);
  res->var = (var ? var->SyntaxCopy() : nullptr);
  res->arr = (arr ? arr->SyntaxCopy() : nullptr);
  res->statement = (statement ? statement->SyntaxCopy() : nullptr);
  res->reversed = reversed;
  res->isRef = isRef;
  // no need to copy private data here, as `SyntaxCopy()` should be called only on unresolved things
}


//==========================================================================
//
//  VForeachArray::DoFixSwitch
//
//==========================================================================
void VForeachArray::DoFixSwitch (VSwitch *aold, VSwitch *anew) {
  if (statement) statement->DoFixSwitch(aold, anew);
}


//==========================================================================
//
//  VForeachArray::Resolve
//
//==========================================================================
bool VForeachArray::Resolve (VEmitContext &ec) {
  if (arr && arr->IsAnyInvocation()) FatalError("VC: Internal compiler error (VForeachArray::Resolve)");

  // we will rewrite 'em later
  auto ivarR = (idxvar ? idxvar->SyntaxCopy()->Resolve(ec) : nullptr);
  auto varR = (var ? var->SyntaxCopy()->Resolve(ec) : nullptr);
  auto arrR = (arr ? arr->SyntaxCopy()->Resolve(ec) : nullptr);

  bool wasError = false;
  if (!statement || !varR || !arrR || (idxvar && !ivarR)) wasError = true;

  if (!wasError && ivarR && ivarR->Type.Type != TYPE_Int) {
    ParseError(var->Loc, "Loop variable should be integer (got `%s`)", *ivarR->Type.GetName());
    wasError = true;
  }

  if (!wasError && !arrR->Type.IsAnyArray()) {
    ParseError(var->Loc, "Array variable should be integer (got `%s`)", *varR->Type.GetName());
    wasError = true;
  }

  if (!wasError) {
    if (isRef) {
      wasError = !varR->Type.MakePointerType().CheckMatch(Loc, arrR->Type.GetArrayInnerType().MakePointerType());
    } else {
      wasError = !varR->Type.CheckMatch(Loc, arrR->Type.GetArrayInnerType());
    }
  }

  // generate faster code for static arrays
  bool isStaticArray = (arrR ? arrR->Type.Type == TYPE_Array : false);
  int staticLen = (isStaticArray ? arrR->Type.ArrayDim : 0);

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
  if (wasError) return false;

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
    VLocalVarDef &L = ec.AllocLocal(NAME_None, VFieldType(TYPE_Int), Loc);
    L.Visible = false; // it is unnamed, and hidden ;-)
    L.ParamFlags = 0;
    index = new VLocalVar(L.ldindex, L.Loc);
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
      VLocalVarDef &L = ec.AllocLocal(NAME_None, VFieldType(TYPE_Int), arr->Loc);
      L.Visible = false; // it is unnamed, and hidden ;-)
      L.ParamFlags = 0;
      limit = new VLocalVar(L.ldindex, L.Loc);
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
  // refvar code will be completed in our codegen
  if (isRef) {
    check(indLocalVal >= 0);
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
      auto flg = loopLoad->Flags;
      loopLoad->Flags &= ~FIELD_ReadOnly;
      loopLoad->RequestAddressOf();
      varaddr->Flags = flg;
    }
  } else {
    loopLoad = new VAssignment(VAssignment::Assign, var->SyntaxCopy(), loopLoad, loopLoad->Loc);
    loopLoad = new VDropResult(loopLoad);
    loopLoad = loopLoad->Resolve(ec);
  }

  // we don't need index anymore
  delete index;

  idxinit = idxinit->Resolve(ec);
  if (!idxinit) return false;

  if (hiinit) {
    hiinit = hiinit->Resolve(ec);
    if (!hiinit) return false;
  }

  loopPreCheck = loopPreCheck->ResolveBoolean(ec);
  if (!loopPreCheck) return false;

  loopNext = loopNext->ResolveBoolean(ec);
  if (!loopNext) return false;

  if (!loopLoad) return false;

  // finally, resolve statement (last, so local reusing will work as expected)
  return statement->Resolve(ec);
}


//==========================================================================
//
//  VForeachArray::DoEmit
//
//==========================================================================
void VForeachArray::DoEmit (VEmitContext &ec) {
  // set-up continues and breaks
  VLabel OldStart = ec.LoopStart;
  VLabel OldEnd = ec.LoopEnd;

  // define labels
  ec.LoopStart = ec.DefineLabel();
  ec.LoopEnd = ec.DefineLabel();

  VLabel Loop = ec.DefineLabel();

  // emit initialisation expressions
  if (hiinit) hiinit->Emit(ec); // may be absent for reverse loops
  idxinit->Emit(ec);

  // do first check
  loopPreCheck->EmitBranchable(ec, ec.LoopEnd, false);

  // actual loop
  ec.MarkLabel(Loop);
  // load value
  if (isRef) varaddr->Emit(ec);
  loopLoad->Emit(ec);
  if (isRef) ec.AddStatement(OPC_AssignPtrDrop, Loc);
  // and emit loop body
  statement->Emit(ec);

  // loop next and test
  ec.MarkLabel(ec.LoopStart); // continue will jump here
  loopNext->EmitBranchable(ec, Loop, true);

  // end of loop
  ec.MarkLabel(ec.LoopEnd);

  // restore continue and break state
  ec.LoopStart = OldStart;
  ec.LoopEnd = OldEnd;
}


//==========================================================================
//
//  VForeachArray::IsEndsWithReturn
//
//==========================================================================
bool VForeachArray::IsEndsWithReturn () const {
  //TODO: endless fors should have at least one return instead
  return (statement && statement->IsEndsWithReturn());
}


//==========================================================================
//
//  VForeachArray::IsProperCaseEnd
//
//==========================================================================
bool VForeachArray::IsProperCaseEnd (bool skipBreak) const {
  return (statement && statement->IsProperCaseEnd(true));
}


//==========================================================================
//
//  VForeachArray::FindLabel
//
//==========================================================================
VLabelStmt *VForeachArray::FindLabel (VName aname) {
  return (statement ? statement->FindLabel(aname) : nullptr);
}


//==========================================================================
//
//  VForeachArray::IsGotoInAllowed
//
//==========================================================================
bool VForeachArray::IsGotoInAllowed () const {
  return false;
}


//==========================================================================
//
//  VForeachIota::BuildPathTo
//
//==========================================================================
bool VForeachArray::BuildPathTo (const VStatement *dest, TArray<VStatement *> &path) {
  if (dest == this) { path.append(this); return true; }
  path.append(this);
  if (statement && statement->BuildPathTo(dest, path)) return true;
  path.removeAt(path.length()-1);
  return false;
}



//==========================================================================
//
//  VForeachScripted::VForeachScripted
//
//==========================================================================
VForeachScripted::VForeachScripted (VExpression *aarr, int afeCount, Var *afevars, const TLocation &aloc)
  : VStatement(aloc)
  , isBoolInit(false)
  , ivInit(nullptr)
  , ivNext(nullptr)
  , ivDone(nullptr)
  , arr(aarr)
  , fevarCount(afeCount)
  , statement(nullptr)
  , reversed(false)
{
  if (afeCount < 0 || afeCount > VMethod::MAX_PARAMS) FatalError("VC: internal compiler error (VForeachScripted::VForeachScripted)");
  for (int f = 0; f < afeCount; ++f) fevars[f] = afevars[f];
}


//==========================================================================
//
//  VForeachScripted::~VForeachScripted
//
//==========================================================================
VForeachScripted::~VForeachScripted () {
  delete ivInit; ivInit = nullptr;
  delete ivNext; ivNext = nullptr;
  delete ivDone; ivDone = nullptr;
  delete arr; arr = nullptr;
  for (int f = 0; f < fevarCount; ++f) {
    delete fevars[f].var;
    delete fevars[f].decl;
  }
  fevarCount = 0;
  delete statement; statement = nullptr;
}


//==========================================================================
//
//  VForeachScripted::SyntaxCopy
//
//==========================================================================
VStatement *VForeachScripted::SyntaxCopy () {
  //if (varinit || varnext || hiinit) FatalError("VC: `VForeachScripted::SyntaxCopy()` called on resolved statement");
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
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VForeachScripted *)e;
  res->arr = (arr ? arr->SyntaxCopy() : nullptr);
  for (int f = 0; f < fevarCount; ++f) {
    res->fevars[f] = fevars[f];
    if (fevars[f].var) res->fevars[f].var = fevars[f].var->SyntaxCopy();
    if (fevars[f].decl) res->fevars[f].decl = (VLocalDecl *)fevars[f].decl->SyntaxCopy();
  }
  res->fevarCount = fevarCount;
  res->statement = (statement ? statement->SyntaxCopy() : nullptr);
  res->reversed = reversed;
  // no need to copy private data here, as `SyntaxCopy()` should be called only on unresolved things
}


//==========================================================================
//
//  VForeachScripted::DoFixSwitch
//
//==========================================================================
void VForeachScripted::DoFixSwitch (VSwitch *aold, VSwitch *anew) {
  if (statement) statement->DoFixSwitch(aold, anew);
}


//==========================================================================
//
//  VForeachScripted::Resolve
//
//==========================================================================
bool VForeachScripted::Resolve (VEmitContext &ec) {
  /* if iterator is invocation, rewrite it to:
   *   {
   *     firstargtype it;
   *     mtname_opInit(allargs); // or `mtname_opInitReverse`
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
    return false;
  }

  int itlocidx = -1;
  {
    // create initializer expression
    VStr newName = VStr(*ib->GetMethodName())+"_opInit";
    if (reversed) newName += "Reverse";
    VInvocationBase *einit = (VInvocationBase *)arr->SyntaxCopy();
    einit->SetMethodName(VName(*newName));
    VMethod *minit = einit->GetVMethod(ec);
    if (!minit) {
      //fprintf(stderr, "arr type: `%s` : `%s` (%s)\n", *shitppTypeNameObj(*arr), *shitppTypeNameObj(*einit), *einit->GetMethodName());
      delete einit;
      ParseError(Loc, "Invalid VC iterator (opInit method not found)");
      return false;
    }

    if (einit->NumArgs >= VMethod::MAX_PARAMS) {
      delete einit;
      ParseError(Loc, "Too many arguments to VC iterator opInit method");
      return false;
    }

    // check first arg, and get internal var type
    // should have at least one argument, and it should be `ref`/`out`
    if (minit->NumParams < 1 ||
        (minit->ParamFlags[0]&~(FPARM_Out|FPARM_Ref)) != 0 ||
        (minit->ParamFlags[0]&(FPARM_Out|FPARM_Ref)) == 0)
    {
      delete einit;
      ParseError(Loc, "VC iterator opInit should have at least one arg, and it should be `ref`/`out`");
      return false;
    }

    switch (minit->ReturnType.Type) {
      case TYPE_Void: isBoolInit = false; break;
      case TYPE_Bool: case TYPE_Int: isBoolInit = true; break;
      default:
        delete einit;
        ParseError(Loc, "VC iterator opInit should return `bool` or be `void`");
        return false;
    }

    // create hidden local for `it`
    {
      VLocalVarDef &L = ec.AllocLocal(NAME_None, minit->ParamTypes[0], Loc);
      L.Visible = false; // it is unnamed, and hidden ;-)
      L.ParamFlags = 0;
      itlocidx = L.ldindex;
    }

    // insert hidden local as first init arg
    for (int f = einit->NumArgs; f > 0; --f) einit->Args[f] = einit->Args[f-1];
    einit->Args[0] = new VLocalVar(itlocidx, Loc);
    ++einit->NumArgs;
    // and resolve the call
    ivInit = (isBoolInit ? einit->ResolveBoolean(ec) : einit->Resolve(ec));
    if (!ivInit) return false;
  }

  {
    // create next expression
    VStr newName = VStr(*ib->GetMethodName())+"_opNext";
    VInvocationBase *enext = (VInvocationBase *)arr->SyntaxCopy();
    enext->SetMethodName(VName(*newName));
    VMethod *mnext = enext->GetVMethod(ec);
    if (!mnext) {
      delete enext;
      ParseError(Loc, "Invalid VC iterator (opNext method not found)");
      return false;
    }

    // all "next" args should be `ref`/`out`
    for (int f = 0; f < mnext->NumParams; ++f) {
      if ((mnext->ParamFlags[f]&~(FPARM_Out|FPARM_Ref)) != 0 ||
          (mnext->ParamFlags[f]&(FPARM_Out|FPARM_Ref)) == 0)
      {
        delete enext;
        ParseError(Loc, "VC iterator opNext argument %d is not `ref`/`out`", f+1);
        return false;
      }
    }

    // "next" should return bool
    switch (mnext->ReturnType.Type) {
      case TYPE_Bool: case TYPE_Int: break;
      default:
        delete enext;
        ParseError(Loc, "VC iterator opNext should return `bool`");
        return false;
    }

    // remove all `enext` args, and insert foreach args instead
    for (int f = 0; f < enext->NumArgs; ++f) delete enext->Args[f];
    enext->NumArgs = 1+fevarCount;
    enext->Args[0] = new VLocalVar(itlocidx, Loc);
    for (int f = 0; f < fevarCount; ++f) enext->Args[f+1] = fevars[f].var->SyntaxCopy();

    ivNext = enext->ResolveBoolean(ec);
    if (!ivNext) return false;
  }

  {
    // create done expression
    VStr newName = VStr(*ib->GetMethodName())+"_opDone";
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
        return false;
      }

      if (mdone->ReturnType.Type != TYPE_Void) {
        delete edone;
        ParseError(Loc, "VC iterator opDone should be `void`");
        return false;
      }

      // replace "done" args with hidden `it`
      for (int f = 0; f < edone->NumArgs; ++f) delete edone->Args[f];
      edone->NumArgs = 1;
      edone->Args[0] = new VLocalVar(itlocidx, Loc);

      ivDone = edone->Resolve(ec);
      if (!ivDone) return false;
    } else {
      ivDone = nullptr;
    }
  }

  // finally, resolve statement (last, so local reusing will work as expected)
  return statement->Resolve(ec);
}


//==========================================================================
//
//  VForeachScripted::DoEmit
//
//==========================================================================
void VForeachScripted::DoEmit (VEmitContext &ec) {
  // set-up continues and breaks
  VLabel OldStart = ec.LoopStart;
  VLabel OldEnd = ec.LoopEnd;

  // define labels
  ec.LoopStart = ec.DefineLabel();
  ec.LoopEnd = ec.DefineLabel();

  VLabel LoopExitSkipDtor = ec.DefineLabel();

  // emit initialisation expression
  if (isBoolInit) {
    ivInit->EmitBranchable(ec, LoopExitSkipDtor, false);
  } else {
    ivInit->Emit(ec);
  }

  // push iterator
  ec.AddStatement(OPC_IteratorDtorAt, ec.LoopEnd, Loc);

  // actual loop
  ec.MarkLabel(ec.LoopStart);
  // call next
  ivNext->EmitBranchable(ec, ec.LoopEnd, false);
  // emit loop body
  statement->Emit(ec);
  // again
  ec.AddStatement(OPC_Goto, ec.LoopStart, Loc);

  // end of loop
  ec.MarkLabel(ec.LoopEnd);

  // dtor
  if (ivDone) ivDone->Emit(ec);

  // pop iterator
  ec.AddStatement(OPC_IteratorFinish, Loc);

  ec.MarkLabel(LoopExitSkipDtor);

  // restore continue and break state
  ec.LoopStart = OldStart;
  ec.LoopEnd = OldEnd;
}


//==========================================================================
//
//  VForeachScripted::IsEndsWithReturn
//
//==========================================================================
bool VForeachScripted::IsEndsWithReturn () const {
  //TODO: endless fors should have at least one return instead
  return (statement && statement->IsEndsWithReturn());
}


//==========================================================================
//
//  VForeachScripted::IsProperCaseEnd
//
//==========================================================================
bool VForeachScripted::IsProperCaseEnd (bool skipBreak) const {
  return (statement && statement->IsProperCaseEnd(true));
}


//==========================================================================
//
//  VForeachScripted::FindLabel
//
//==========================================================================
VLabelStmt *VForeachScripted::FindLabel (VName aname) {
  return (statement ? statement->FindLabel(aname) : nullptr);
}


//==========================================================================
//
//  VForeachScripted::IsGotoInAllowed
//
//==========================================================================
bool VForeachScripted::IsGotoInAllowed () const {
  return false;
}


//==========================================================================
//
//  VForeachScripted::IsGotoOutAllowed
//
//==========================================================================
bool VForeachScripted::IsGotoOutAllowed () const {
  return false;
}


//==========================================================================
//
//  VForeachScripted::BuildPathTo
//
//==========================================================================
bool VForeachScripted::BuildPathTo (const VStatement *dest, TArray<VStatement *> &path) {
  if (dest == this) { path.append(this); return true; }
  path.append(this);
  if (statement && statement->BuildPathTo(dest, path)) return true;
  path.removeAt(path.length()-1);
  return false;
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

  // first resolve all cases and default
  for (int i = 0; i < Statements.length(); ++i) {
    VStatement *st = Statements[i];
    if (st->IsSwitchCase() || st->IsSwitchDefault()) {
      if (!st->Resolve(ec)) return false;
    }
  }

  // now resolve other statements
  for (int i = 0; i < Statements.length(); ++i) {
    VStatement *st = Statements[i];
    if (!st->IsSwitchCase() && !st->IsSwitchDefault()) {
      if (!st->Resolve(ec)) Ret = false;
      // dummy last `break`, it is not necessary
      if (Ret) {
        //FIXME: this should be done in separate code optimizer pass
        if (st->IsBreak()) {
          // skip branches without statements
          int n = i+1;
          while (n < Statements.length() && (Statements[n]->IsSwitchCase() || Statements[n]->IsSwitchDefault())) ++n;
          if (n >= Statements.length()) {
            //ParseWarning(st->Loc, "`break;` dummied out");
            ((VBreak *)st)->skipCodegen = true;
          }
        } else if (st->IsGotoCase() && !st->HasGotoCaseExpr()) {
          // jump to next case: dummy it out if next case immediately follows
          if (i+1 < Statements.length() && Statements[i+1]->IsSwitchCase()) {
            //ParseWarning(st->Loc, "`goto case;` dummied out");
            ((VGotoStmt *)st)->skipCodegen = true;
          }
        } else if (st->IsGotoDefault()) {
          // jump to next case: dummy it out if default case immediately follows
          if (i+1 < Statements.length() && HaveDefault && Statements[i+1]->IsSwitchDefault()) {
            //ParseWarning(st->Loc, "`goto default;` dummied out");
            ((VGotoStmt *)st)->skipCodegen = true;
          }
        } else if (st->IsGotoCase() && st->HasGotoCaseExpr()) {
          // jump to next case: dummy it out if next case immediately follows
          VGotoStmt *gs = (VGotoStmt *)st;
          if (gs->CaseValue && gs->CaseValue->IsIntConst()) {
            int v = gs->CaseValue->GetIntConst();
            int n = i+1;
            while (n < Statements.length()) {
              if (!Statements[n]->IsSwitchCase()) { n = Statements.length(); break; }
              VSwitchCase *sc = (VSwitchCase *)Statements[n];
              if (sc->Value == v) break;
              ++n;
            }
            if (n < Statements.length()) {
              //ParseWarning(st->Loc, "`goto case %d;` dummied out", v);
              gs->skipCodegen = true;
            }
          }
        }
      }
    }
  }

  if (Ret) {
    if (!checkProperCaseEnd(true)) Ret = false;
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
bool VSwitch::IsEndsWithReturn () const {
  //FIXME: `goto case` and `goto default`
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
    if (Statements[n]->IsFlowStop()) {
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
//  VSwitch::checkProperCaseEnd
//
//==========================================================================
bool VSwitch::IsProperCaseEnd (bool skipBreak) const {
  return IsEndsWithReturn();
}


//==========================================================================
//
//  VSwitch::checkProperCaseEnd
//
//==========================================================================
bool VSwitch::checkProperCaseEnd (bool reportSwitchCase) const {
  if (Statements.length() == 0) return true;
  bool statementSeen = false;
  bool isEndSeen = false;
  int lastCaseIdx = -1;
  //ParseWarning(Loc, "=========================");
  for (int n = 0; n < Statements.length(); ++n) {
    if (!Statements[n]) return false; //k8: orly?
    // `case` or `default`?
    if (Statements[n]->IsSwitchCase() || Statements[n]->IsSwitchDefault()) {
      if (lastCaseIdx >= 0 && !isEndSeen && statementSeen) {
        //fprintf(stderr, "pidx=%d; n=%d; es=%d; ss=%d\n", lastCaseIdx, n, (int)isEndSeen, (int)statementSeen);
        // oops
        if (reportSwitchCase) ParseError(Statements[lastCaseIdx]->Loc, "`switch` branch doesn't end with `break` or `goto case`");
        return false;
      }
      lastCaseIdx = n;
      isEndSeen = false;
      statementSeen = false;
      continue;
    }
    if (isEndSeen) continue;
    //fprintf(stderr, "  n=%d; type=%s\n", n, *shitppTypeNameObj(*Statements[n]));
    if (Statements[n]->IsLabel()) continue; // nobody cares
    statementSeen = true;
    // proper end?
    if (Statements[n]->IsProperCaseEnd(false)) {
      isEndSeen = true;
      continue;
    }
    // flow break?
    if (Statements[n]->IsFlowStop()) {
      // `break`/`continue`
      isEndSeen = true;
    }
  }
  // last one can omit proper end
  return true;
}


//==========================================================================
//
//  VSwitch::FindLabel
//
//==========================================================================
VLabelStmt *VSwitch::FindLabel (VName aname) {
  for (int f = 0; f < Statements.length(); ++f) {
    VLabelStmt *lbl = (Statements[f] ? Statements[f]->FindLabel(aname) : nullptr);
    if (lbl) return lbl;
  }
  return nullptr;
}


//==========================================================================
//
//  VSwitch::BuildPathTo
//
//==========================================================================
bool VSwitch::BuildPathTo (const VStatement *dest, TArray<VStatement *> &path) {
  if (dest == this) { path.append(this); return true; }
  path.append(this);
  for (int f = 0; f < Statements.length(); ++f) {
    if (Statements[f] && Statements[f]->BuildPathTo(dest, path)) return true;
  }
  path.removeAt(path.length()-1);
  return false;
}


//==========================================================================
//
//  VSwitch::IsJumpOverAllowed
//
//==========================================================================
bool VSwitch::IsJumpOverAllowed (const VStatement *s0, const VStatement *s1) const {
  if (s0 == s1) return true;
  if (!s0 || !s1) return false;
  int idx0 = -1, idx1 = -1;
  for (int f = Statements.length()-1; f >= 0; --f) {
         if (Statements[f] == s0) idx0 = f;
    else if (Statements[f] == s1) idx1 = f;
  }
  if (idx0 < 0 || idx1 < 0) return false; // the thing that should not be
  if (idx0 > idx1) { int tmp = idx0; idx0 = idx1; idx1 = tmp; }
  for (int f = idx0; f <= idx1; ++f) {
    if (Statements[f] && Statements[f]->IsVarDecl()) return false;
  }
  return true;
}


//==========================================================================
//
//  VSwitch::PostProcessGotoCase
//
//==========================================================================
/*
void VSwitch::PostProcessGotoCase () {
  for (int f = Statements.length()-1; f >= 0; --f) {
    if (Statements[f]->IsGoto()) {
      auto gs = (VGotoStmt *)Statements[f];
      // is this `goto case;`?
      if (gs.GotoType == VGotoStmt::Case || gs.GotoType == VGotoStmt::Default) {
        gs->Switch = this;
      }
    }
  }
}
*/



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
  if (Expr) Expr = Expr->ResolveToIntLiteralEx(ec);
  if (!Expr) return false;

  if (!Expr->IsIntConst()) FatalError("VC: internal compiler error (VSwitchCase::Resolve)");

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
  if (gotoLbl.IsDefined()) ec.MarkLabel(gotoLbl);
}


//==========================================================================
//
//  VSwitchCase::IsCase
//
//==========================================================================
bool VSwitchCase::IsSwitchCase () const {
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
bool VSwitchDefault::IsSwitchDefault () const {
  return true;
}



//==========================================================================
//
//  VBreak::VBreak
//
//==========================================================================
VBreak::VBreak (const TLocation &ALoc)
  : VStatement(ALoc)
  , skipCodegen(false)
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
  if (skipCodegen) return;
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
bool VBreak::IsBreak () const {
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
bool VContinue::IsContinue () const {
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
    ec.EmitLocalDtors(0, NumLocalsToClear, Loc);
    if (Expr->Type.GetStackSize() == 4) {
      ec.AddStatement(OPC_ReturnL, Loc);
    } else if (Expr->Type.Type == TYPE_Vector) {
      ec.AddStatement(OPC_ReturnV, Loc);
    } else {
      ParseError(Loc, "Bad return type");
    }
  } else {
    ec.EmitLocalDtors(0, NumLocalsToClear, Loc);
    ec.AddStatement(OPC_Return, Loc);
  }
}


//==========================================================================
//
//  VReturn::IsReturn
//
//==========================================================================
bool VReturn::IsReturn () const {
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
//  VLocalVarStatement::IsVarDecl
//
//==========================================================================
bool VLocalVarStatement::IsVarDecl () const {
  return true;
}



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
//  VDeleteStatement::Resolve
//
//==========================================================================
bool VDeleteStatement::Resolve (VEmitContext &ec) {
  if (!var) return false;

  // build check expression
  checkexpr = var->SyntaxCopy()->ResolveBoolean(ec);
  if (!checkexpr) return false;

  // build delete expression
  delexpr = new VDotInvocation(var->SyntaxCopy(), VName("Destroy"), var->Loc, 0, nullptr);
  delexpr = new VDropResult(delexpr);
  delexpr = delexpr->Resolve(ec);
  if (!delexpr) return false;

  // build clear expression
  assexpr = new VAssignment(VAssignment::Assign, var->SyntaxCopy(), new VNoneLiteral(var->Loc), var->Loc);
  assexpr = new VDropResult(assexpr);
  assexpr = assexpr->Resolve(ec);
  if (!assexpr) return false;

  return true;
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
//  VCompound::VCompound
//
//==========================================================================
VCompound::VCompound (const TLocation &ALoc) : VStatement(ALoc) {
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
bool VCompound::IsEndsWithReturn () const {
  for (int n = 0; n < Statements.length(); ++n) {
    if (!Statements[n]) continue;
    if (Statements[n]->IsEndsWithReturn()) return true;
    if (Statements[n]->IsFlowStop()) break;
  }
  return false;
}


//==========================================================================
//
//  VCompound::IsProperCaseEnd
//
//==========================================================================
bool VCompound::IsProperCaseEnd (bool skipBreak) const {
  for (int n = 0; n < Statements.length(); ++n) {
    if (!Statements[n]) continue;
    if (Statements[n]->IsProperCaseEnd(skipBreak)) return true;
    if (Statements[n]->IsFlowStop()) break;
  }
  return false;
}


//==========================================================================
//
//  VCompound::FindLabel
//
//==========================================================================
VLabelStmt *VCompound::FindLabel (VName aname) {
  for (int f = 0; f < Statements.length(); ++f) {
    VLabelStmt *lbl = (Statements[f] ? Statements[f]->FindLabel(aname) : nullptr);
    if (lbl) return lbl;
  }
  return nullptr;
}


//==========================================================================
//
//  VCompound::BuildPathTo
//
//==========================================================================
bool VCompound::BuildPathTo (const VStatement *dest, TArray<VStatement *> &path) {
  if (dest == this) { path.append(this); return true; }
  path.append(this);
  for (int f = 0; f < Statements.length(); ++f) {
    if (Statements[f] && Statements[f]->BuildPathTo(dest, path)) return true;
  }
  path.removeAt(path.length()-1);
  return false;
}


//==========================================================================
//
//  VCompound::IsJumpOverAllowed
//
//==========================================================================
bool VCompound::IsJumpOverAllowed (const VStatement *s0, const VStatement *s1) const {
  if (s0 == s1) return true;
  if (!s0 || !s1) return false;
  int idx0 = -1, idx1 = -1;
  for (int f = Statements.length()-1; f >= 0; --f) {
         if (Statements[f] == s0) idx0 = f;
    else if (Statements[f] == s1) idx1 = f;
  }
  if (idx0 < 0 || idx1 < 0) return false; // the thing that should not be
  if (idx0 > idx1) { int tmp = idx0; idx0 = idx1; idx1 = tmp; }
  for (int f = idx0; f <= idx1; ++f) {
    if (Statements[f] && Statements[f]->IsVarDecl()) return false;
  }
  return true;
}



//==========================================================================
//
//  VLabelStmt::VLabelStmt
//
//==========================================================================
VLabelStmt::VLabelStmt (VName aname, const TLocation &ALoc)
  : VStatement(ALoc)
  , Name(aname)
{
}


//==========================================================================
//
//  VLabelStmt::VLabelStmt
//
//==========================================================================
VStatement *VLabelStmt::SyntaxCopy () {
  auto res = new VLabelStmt();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VLabelStmt::DoSyntaxCopyTo
//
//==========================================================================
void VLabelStmt::DoSyntaxCopyTo (VStatement *e) {
  VStatement::DoSyntaxCopyTo(e);
  auto res = (VLabelStmt *)e;
  res->Name = Name;
}


//==========================================================================
//
//  VLabelStmt::Resolve
//
//==========================================================================
bool VLabelStmt::Resolve (VEmitContext &ec) {
  return true;
}


//==========================================================================
//
//  VLabelStmt::DoEmit
//
//==========================================================================
void VLabelStmt::DoEmit (VEmitContext &ec) {
  ec.EmitGotoLabel(Name, Loc);
}


//==========================================================================
//
//  VLabelStmt::IsLabel
//
//==========================================================================
bool VLabelStmt::IsLabel () const {
  return true;
}


//==========================================================================
//
//  VLabelStmt::GetLabelName
//
//==========================================================================
VName VLabelStmt::GetLabelName () const {
  return Name;
}



//==========================================================================
//
//  VGotoStmt::VGotoStmt
//
//==========================================================================
VGotoStmt::VGotoStmt (VName aname, const TLocation &ALoc)
  : VStatement(ALoc)
  , casedef(nullptr)
  , gotolbl(nullptr)
  , Name(aname)
  , Switch(nullptr)
  , CaseValue(nullptr)
  , GotoType(Normal)
  , SwitchStNum(-1)
  , skipCodegen(false)
{
}


//==========================================================================
//
//  VGotoStmt::VGotoStmt
//
//==========================================================================
VGotoStmt::VGotoStmt (VSwitch *ASwitch, VExpression *ACaseValue, int ASwitchStNum, bool toDefault, const TLocation &ALoc)
  : VStatement(ALoc)
  , casedef(nullptr)
  , gotolbl(nullptr)
  , Name(NAME_None)
  , Switch(ASwitch)
  , CaseValue(ACaseValue)
  , GotoType(toDefault ? Default : Case)
  , SwitchStNum(ASwitchStNum)
  , skipCodegen(false)
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
  res->Name = Name;
  res->Switch = Switch;
  res->CaseValue = (CaseValue ? CaseValue->SyntaxCopy() : nullptr);
  res->GotoType = GotoType;
}


//==========================================================================
//
//  VGotoStmt::ResolveGoto
//
//==========================================================================
bool VGotoStmt::ResolveGoto (VEmitContext &ec, VStatement *dest) {
  if (!dest) return false;

  // build path to self
  TArray<VStatement *> toself;
  if (!ec.CurrentFunc->Statement->BuildPathTo(this, toself)) {
    ParseError(Loc, "Cannot build path to `goto`");
    return false;
  }

  // build path to label
  TArray<VStatement *> tolabel;
  if (!ec.CurrentFunc->Statement->BuildPathTo(dest, tolabel)) {
    ParseError(Loc, "Cannot build path to label `%s`", *Name);
    return false;
  }

  // find common parent
  VStatement *cpar = nullptr;
  int sidx = -1, lidx = -1;
  for (int f = toself.length()-1; f >= 0; --f) {
    for (int c = tolabel.length()-1; c >= 0; --c) {
      if (tolabel[c] == toself[f]) {
        cpar = tolabel[c];
        sidx = f;
        lidx = c;
        break;
      }
    }
    if (cpar) break;
  }
  if (!cpar) {
    ParseError(Loc, "Cannot find common parent for `goto`, and label `%s`", *Name);
    return false;
  }
  if (sidx == toself.length()-1 || lidx == tolabel.length()-1) FatalError("VC: internal compiler error (VGotoStmt::Resolve)");

  // now go up to parent and down to label, checking if gotos are allowed
  for (int f = toself.length()-1; f >= 0; --f) {
    if (toself[f] == cpar) break;
    if (!toself[f]->IsGotoOutAllowed()) {
      ParseError(Loc, "Cannot jump to label `%s`", *Name);
      return false;
    }
  }

  // down
  bool doCheck = false;
  for (int c = 0; c < tolabel.length(); ++c) {
    if (tolabel[c] == cpar) {
      doCheck = true;
      continue;
    }
    if (!doCheck) continue;
    if (!tolabel[c]->IsGotoInAllowed()) {
      ParseError(Loc, "Cannot jump to label `%s`", *Name);
      return false;
    }
  }

  // check if we can jump over
  if (!cpar->IsJumpOverAllowed(toself[sidx+1], tolabel[lidx+1])) {
    ParseError(Loc, "Cannot jump to label `%s`", *Name);
    return false;
  }

  // ok, it is legal
  return true;
}


//==========================================================================
//
//  VGotoStmt::Resolve
//
//==========================================================================
bool VGotoStmt::Resolve (VEmitContext &ec) {
  if (Switch) {
    // goto case/default
    if (GotoType == Normal) FatalError("VC: internal compiler error (VGotoStmt::Resolve) (0)");
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
      if (!st) { ParseError(Loc, "`goto default;` whithout `default`"); return false; }
    } else {
      // find the case
      if (CaseValue) {
        // case is guaranteed to be parsed, do value search
        CaseValue = CaseValue->ResolveToIntLiteralEx(ec);
        if (!CaseValue) return false; // oops
        if (!CaseValue->IsIntConst()) FatalError("VC: internal compiler error (VGotoStmt::Resolve)");
        int val = CaseValue->GetIntConst();
        for (int f = 0; f < Switch->Statements.length(); ++f) {
          if (Switch->Statements[f]->IsSwitchCase()) {
            VSwitchCase *cc = (VSwitchCase *)(Switch->Statements[f]);
            if (cc->Value == val) {
              st = cc;
              break;
            }
          }
        }
        if (!st) { ParseError(Loc, "case `%d` not found", val); return false; }
      } else {
        // `goto case` without args: find next one
        for (int f = SwitchStNum; f < Switch->Statements.length(); ++f) {
          if (Switch->Statements[f]->IsSwitchCase()) {
            st = Switch->Statements[f];
            break;
          }
        }
        if (!st) { ParseError(Loc, "case for `goto case;` not found"); return false; }
      }
      if (st) {
        VSwitchCase *cc = (VSwitchCase *)st;
        if (!cc->gotoLbl.IsDefined()) cc->gotoLbl = ec.DefineLabel();
      }
    }
    casedef = st;
    return (st ? ResolveGoto(ec, st) : true);
  } else {
    // normal goto
    if (GotoType != Normal) FatalError("VC: internal compiler error (VGotoStmt::Resolve) (1)");
    if (Switch) FatalError("VC: internal compiler error (VGotoStmt::Resolve) (2)");
    // find label
    VLabelStmt *lbl = ec.CurrentFunc->Statement->FindLabel(Name);
    if (!lbl) {
      ParseError(Loc, "Destination label `%s` not found", *Name);
      return false;
    }
    gotolbl = lbl;
    return ResolveGoto(ec, lbl);
  }
}


//==========================================================================
//
//  VGotoStmt::DoEmit
//
//==========================================================================
void VGotoStmt::DoEmit (VEmitContext &ec) {
  if (skipCodegen) return; // nothing to do here
  if (GotoType == Normal) {
    VLabelStmt *lbl = gotolbl; //ec.CurrentFunc->Statement->FindLabel(Name);
    if (!lbl) {
      ParseError(Loc, "Destination label `%s` not found", *Name);
      return;
    }
    ec.EmitGotoTo(Name, Loc);
  } else if (GotoType == Case) {
    if (!casedef) {
      if (!ec.LoopEnd.IsDefined()) ParseError(Loc, "Misplaced `goto case` statement");
      ec.AddStatement(OPC_Goto, ec.LoopEnd, Loc);
      return;
    }
    if (!casedef->IsSwitchCase()) FatalError("VC: internal compiler error (VGotoStmt::DoEmit) (0)");
    VSwitchCase *cc = (VSwitchCase *)casedef;
    ec.AddStatement(OPC_Goto, cc->gotoLbl, Loc);
  } else if (GotoType == Default) {
    if (!casedef) {
      if (!ec.LoopEnd.IsDefined()) ParseError(Loc, "Misplaced `goto default` statement");
      ec.AddStatement(OPC_Goto, ec.LoopEnd, Loc);
      return;
    }
    ec.AddStatement(OPC_Goto, Switch->DefaultAddress, Loc);
  } else {
    FatalError("VC: internal compiler error (VGotoStmt::DoEmit)");
  }
}


//==========================================================================
//
//  VGotoStmt::IsGoto
//
//==========================================================================
bool VGotoStmt::IsGoto () const {
  return true;
}


//==========================================================================
//
//  VGotoStmt::IsGotoCase
//
//==========================================================================
bool VGotoStmt::IsGotoCase () const {
  return (GotoType == Case);
}


//==========================================================================
//
//  VGotoStmt::HasGotoCaseExpr
//
//==========================================================================
bool VGotoStmt::HasGotoCaseExpr () const {
  return (GotoType == Case && !!CaseValue);
}


//==========================================================================
//
//  VGotoStmt::IsGotoDefault
//
//==========================================================================
bool VGotoStmt::IsGotoDefault () const {
  return (GotoType == Default);
}


//==========================================================================
//
//  VGotoStmt::GetLabelName
//
//==========================================================================
VName VGotoStmt::GetLabelName () const {
  return Name;
}
