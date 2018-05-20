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
VStatement::~VStatement () noexcept(false) {}
void VStatement::Emit (VEmitContext &ec) { DoEmit(ec); }
bool VStatement::IsBreak () { return false; }
bool VStatement::IsContinue () { return false; }
bool VStatement::IsReturn () { return false; }
bool VStatement::IsSwitchCase () { return false; }
bool VStatement::IsSwitchDefault () { return false; }
bool VStatement::IsEndsWithReturn () { return false; }


// ////////////////////////////////////////////////////////////////////////// //
// VEmptyStatement
VEmptyStatement::VEmptyStatement (const TLocation &ALoc) : VStatement(ALoc) {}
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
    ec.AddStatement(OPC_Goto, End);
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
  if (TrueStatement) return TrueStatement->IsEndsWithReturn();
  if (FalseStatement) return FalseStatement->IsEndsWithReturn();
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

  ec.AddStatement(OPC_Goto, ec.LoopStart);
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
  , CondExpr(nullptr)
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
  if (CondExpr) { delete CondExpr; CondExpr = nullptr; }
  for (int i = 0; i < LoopExpr.length(); ++i) {
    if (LoopExpr[i]) { delete LoopExpr[i]; LoopExpr[i] = nullptr; }
  }
  if (Statement) { delete Statement; Statement = nullptr; }
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

  if (CondExpr) {
    CondExpr = CondExpr->ResolveBoolean(ec);
    if (!CondExpr) Ret = false;
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
  if (CondExpr) ec.AddStatement(OPC_Goto, Test);

  // emit embeded statement
  ec.MarkLabel(Loop);
  Statement->Emit(ec);

  // emit per-loop expression statements
  ec.MarkLabel(ec.LoopStart);
  for (int i = 0; i < LoopExpr.length(); ++i) LoopExpr[i]->Emit(ec);

  // loop test
  ec.MarkLabel(Test);
  if (!CondExpr) {
    ec.AddStatement(OPC_Goto, Loop);
  } else {
    CondExpr->EmitBranchable(ec, Loop, true);
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
  ec.AddStatement(OPC_IteratorInit);

  VLabel Loop = ec.DefineLabel();
  ec.LoopStart = ec.DefineLabel();
  ec.LoopEnd = ec.DefineLabel();

  ec.AddStatement(OPC_Goto, ec.LoopStart);
  ec.MarkLabel(Loop);
  Statement->Emit(ec);
  ec.MarkLabel(ec.LoopStart);
  ec.AddStatement(OPC_IteratorNext);
  ec.AddStatement(OPC_IfGoto, Loop);
  ec.MarkLabel(ec.LoopEnd);
  ec.AddStatement(OPC_IteratorPop);

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
      ec.AddStatement(OPC_CaseGotoB, CaseInfo[i].Value, CaseInfo[i].Address);
    } else if (CaseInfo[i].Value >= MIN_VINT16 && CaseInfo[i].Value < MAX_VINT16) {
      ec.AddStatement(OPC_CaseGotoS, CaseInfo[i].Value, CaseInfo[i].Address);
    } else {
      ec.AddStatement(OPC_CaseGoto, CaseInfo[i].Value, CaseInfo[i].Address);
    }
  }
  ec.AddStatement(OPC_Drop);

  // go to default case if we have one, otherwise to the end of switch
  if (HaveDefault) {
    DefaultAddress = ec.DefineLabel();
    ec.AddStatement(OPC_Goto, DefaultAddress);
  } else {
    ec.AddStatement(OPC_Goto, ec.LoopEnd);
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
//  VSwitchDefault::Resolve
//
//==========================================================================
bool VSwitchDefault::Resolve (VEmitContext &) {
  if (Switch->HaveDefault) {
    ParseError(Loc, "Only 1 DEFAULT per switch allowed.");
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
    ParseError(Loc, "Misplaced BREAK statement.");
    return;
  }
  ec.AddStatement(OPC_Goto, ec.LoopEnd);
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
    ParseError(Loc, "Misplaced CONTINUE statement.");
    return;
  }
  ec.AddStatement(OPC_Goto, ec.LoopStart);
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
//  VReturn::Resolve
//
//==========================================================================
bool VReturn::Resolve (VEmitContext &ec) {
  NumLocalsToClear = ec.LocalDefs.length();
  bool Ret = true;
  if (Expr) {
    Expr = (ec.FuncRetType.Type == TYPE_Float ? Expr->ResolveFloat(ec) : Expr->Resolve(ec));
    if (ec.FuncRetType.Type == TYPE_Void) {
      ParseError(Loc, "void function cannot return a value.");
      Ret = false;
    } else if (Expr) {
      Expr->Type.CheckMatch(Expr->Loc, ec.FuncRetType);
    } else {
      Ret = false;
    }
  } else {
    if (ec.FuncRetType.Type != TYPE_Void) {
      ParseError(Loc, "Return value expected.");
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
    ec.EmitClearStrings(0, NumLocalsToClear);
    if (Expr->Type.GetStackSize() == 4) {
      ec.AddStatement(OPC_ReturnL);
    } else if (Expr->Type.Type == TYPE_Vector) {
      ec.AddStatement(OPC_ReturnV);
    } else {
      ParseError(Loc, "Bad return type");
    }
  } else {
    ec.EmitClearStrings(0, NumLocalsToClear);
    ec.AddStatement(OPC_Return);
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
VExpressionStatement::~VExpressionStatement () noexcept(false) {
  if (Expr) { delete Expr; Expr = nullptr; }
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
//  VCompound::Resolve
//
//==========================================================================
bool VCompound::Resolve (VEmitContext &ec) {
  bool Ret = true;
  int NumLocalsOnStart = ec.LocalDefs.length();
  for (int i = 0; i < Statements.length(); ++i) {
    if (!Statements[i]->Resolve(ec)) Ret = false;
  }

  for (int i = NumLocalsOnStart; i < ec.LocalDefs.length(); ++i) {
    ec.LocalDefs[i].Visible = false;
  }

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
