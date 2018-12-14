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
// this directly included from "vc_decorate.cpp"


static VExpression *ParseExpression (VScriptParser *sc, VClass *Class);


//==========================================================================
//
//  CheckParseSetUserVarExpr
//
//==========================================================================
static VExpression *CheckParseSetUserVarExpr (VScriptParser *sc, VClass *Class, VStr FuncName) {
  if (FuncName.ICmp("A_SetUserVar") == 0 || FuncName.ICmp("A_SetUserVarFloat") == 0) {
    auto stloc = sc->GetLoc();
    sc->Expect("(");
    sc->ExpectString();
    VStr varName = sc->String;
    VStr uvname = sc->String.toLowerCase();
    if (!uvname.startsWith("user_")) sc->Error(va("%s: user variable name in DECORATE must start with `user_`", *sc->GetLoc().toStringNoCol()));
    sc->Expect(",");
    VExpression *val = ParseExpression(sc, Class);
    sc->Expect(")");
    if (!val) sc->Error("invalid assignment");
    VExpression *dest = new VDecorateUserVar(VName(*varName), stloc);
    return new VAssignment(VAssignment::Assign, dest, val, stloc);
  } else if (FuncName.ICmp("A_SetUserArray") == 0 || FuncName.ICmp("A_SetUserArrayFloat") == 0) {
    auto stloc = sc->GetLoc();
    sc->Expect("(");
    sc->ExpectString();
    VStr varName = sc->String;
    VStr uvname = sc->String.toLowerCase();
    if (!uvname.startsWith("user_")) sc->Error(va("%s: user variable name in DECORATE must start with `user_`", *sc->GetLoc().toStringNoCol()));
    sc->Expect(",");
    // index
    VExpression *idx = ParseExpression(sc, Class);
    if (!idx) sc->Error("decorate parsing error");
    sc->Expect(",");
    // value
    VExpression *val = ParseExpression(sc, Class);
    if (!val) sc->Error("decorate parsing error");
    sc->Expect(")");
    VExpression *dest = new VDecorateUserVar(VName(*varName), idx, stloc);
    return new VAssignment(VAssignment::Assign, dest, val, stloc);
  }
  return nullptr;
}


//==========================================================================
//
//  CheckParseSetUserVarStmt
//
//==========================================================================
static VStatement *CheckParseSetUserVarStmt (VScriptParser *sc, VClass *Class, VStr FuncName) {
  VExpression *asse = CheckParseSetUserVarExpr(sc, Class, FuncName);
  if (!asse) return nullptr;
  return new VExpressionStatement(asse);
}


//==========================================================================
//
//  ParseFunCallWithName
//
//==========================================================================
static VMethod *ParseFunCallWithName (VScriptParser *sc, VStr FuncName, VClass *Class, int &NumArgs, VExpression **Args, bool gotParen) {
  // get function name and parse arguments
  VStr FuncNameLower = FuncName.ToLower();
  NumArgs = 0;
  int totalCount = 0;

  //fprintf(stderr, "***8:<%s> %s\n", *sc->String, *sc->GetLoc().toStringNoCol());
  if (!gotParen) gotParen = sc->Check("(");
  if (gotParen) {
    if (!sc->Check(")")) {
      do {
        ++totalCount;
        Args[NumArgs] = ParseExpression(sc, Class);
        if (NumArgs == VMethod::MAX_PARAMS) {
          delete Args[NumArgs];
          Args[NumArgs] = nullptr;
          if (VStr::ICmp(*FuncName, "A_Jump") != 0 && VStr::ICmp(*FuncName, "randompick") != 0 &&
              VStr::ICmp(*FuncName, "decorate_randompick") != 0)
          {
            ParseError(sc->GetLoc(), "Too many arguments to `%s`", *FuncName);
          }
        } else {
          ++NumArgs;
        }
      } while (sc->Check(","));
      sc->Expect(")");
    }
  }
  //fprintf(stderr, "***9:<%s> %s\n", *sc->String, *sc->GetLoc().toStringNoCol());

  VMethod *Func = nullptr;

  // check ignores
  if (!IgnoredDecorateActions.find(VName(*FuncNameLower))) {
    // find the state action method: first check action specials, then state actions
    // hack: `ACS_ExecuteWithResult` has its own method, but it still should be in line specials
    if (FuncName.ICmp("ACS_ExecuteWithResult") != 0) {
      for (int i = 0; i < LineSpecialInfos.Num(); ++i) {
        if (LineSpecialInfos[i].Name == FuncNameLower) {
          Func = Class->FindMethodChecked("A_ExecActionSpecial");
          if (NumArgs > 5) {
            sc->Error(va("Too many arguments to translated action special `%s`", *FuncName));
          } else {
            // add missing arguments
            while (NumArgs < 5) {
              Args[NumArgs] = new VIntLiteral(0, sc->GetLoc());
              ++NumArgs;
            }
            // add action special number argument
            Args[5] = new VIntLiteral(LineSpecialInfos[i].Number, sc->GetLoc());
            ++NumArgs;
          }
          break;
        }
      }
    }

    if (!Func) {
      VDecorateStateAction *Act = Class->FindDecorateStateAction(*FuncNameLower);
      Func = (Act ? Act->Method : nullptr);
    }
  }

  //fprintf(stderr, "<%s>\n", *FuncNameLower);
  if (!Func) {
    //fprintf(stderr, "***8:<%s> %s\n", *FuncName, *sc->GetLoc().toStringNoCol());
  } else {
    if (Func && NumArgs > Func->NumParams &&
        (VStr::ICmp(*FuncName, "A_Jump") == 0 || VStr::ICmp(*FuncName, "randompick") == 0 ||
         VStr::ICmp(*FuncName, "decorate_randompick") == 0))
    {
      ParseWarning(sc->GetLoc(), "Too many arguments to `%s` (%d -- are you nuts?!)", *FuncName, totalCount);
      for (int f = Func->NumParams; f < NumArgs; ++f) { delete Args[f]; Args[f] = nullptr; }
      NumArgs = Func->NumParams;
    }

    // check for "user_" argument for non-string parameter
    for (int f = 0; f < NumArgs; ++f) {
      if (f >= Func->NumParams) break;
      if (!Args[f]) continue;
      if (!Args[f]->IsStrConst()) continue;
      if (Func->ParamTypes[f].Type == TYPE_String) continue;
      const VStr &str = Args[f]->GetStrConst(DecPkg);
      if (!str.startsWithNoCase("user_")) continue;
      auto loc = Args[f]->Loc;
      ParseWarning(loc, "`user_xxx` should not be a string constant, you moron! FIX YOUR BROKEN CODE!");
      if (Class) {
        VName fldn = Class->FindDecorateStateFieldTrans(*str);
        if (fldn == NAME_None) ParseWarning(loc, "`user_xxx` is not a known uservar");
        delete Args[f];
        Args[f] = new VDecorateSingleName(*fldn, loc);
      } else {
        delete Args[f];
        Args[f] = new VDecorateSingleName(str.toLowerCase(), loc);
      }
    }
  }

  return Func;
}


//==========================================================================
//
//  ParseMethodCall
//
//==========================================================================
static VExpression *ParseMethodCall (VScriptParser *sc, VStr Name, TLocation Loc, bool parenEaten=true) {
  VExpression *Args[VMethod::MAX_PARAMS+1];
  int NumArgs = 0;
  VMethod *Func = ParseFunCallWithName(sc, Name, decoClass, NumArgs, Args, parenEaten); // got paren
  /*
  if (Name.ICmp("random") == 0) {
    fprintf(stderr, "*** RANDOM; NumArgs=%d (%s); func=%p (%s, %s)\n", NumArgs, *sc->GetLoc().toStringNoCol(), Func, *Args[0]->toString(), *Args[1]->toString());
  }
  */
  return new VDecorateInvocation((Func ? Func->GetVName() : VName(*Name, VName::AddLower)), Loc, NumArgs, Args);
}


//==========================================================================
//
//  ParseExpressionPriority0
//
//==========================================================================
static VExpression *ParseExpressionPriority0 (VScriptParser *sc) {
  guard(ParseExpressionPriority0);
  TLocation l = sc->GetLoc();

  // check for quoted strings first, since these could also have numbers...
  if (sc->CheckQuotedString()) {
    int Val = DecPkg->FindString(*sc->String);
    return new VStringLiteral(sc->String, Val, l);
  }

  if (sc->CheckNumber()) {
    vint32 Val = sc->Number;
    return new VIntLiteral(Val, l);
  }

  if (sc->CheckFloat()) {
    float Val = sc->Float;
    return new VFloatLiteral(Val, l);
  }

  if (sc->Check("false")) return new VIntLiteral(0, l);
  if (sc->Check("true")) return new VIntLiteral(1, l);

  if (sc->Check("(")) {
    VExpression *op = ParseExpression(sc, decoClass);
    if (!op) ParseError(l, "Expression expected");
    sc->Expect(")");
    return op;
  }

  if (sc->CheckIdentifier()) {
    VStr Name = sc->String;
    if (Name.ICmp("args") == 0) {
      if (sc->GetString()) {
        if (sc->String == "[") {
          Name = VStr("GetArg");
          //fprintf(stderr, "*** ARGS ***\n");
          VExpression *Args[1];
          Args[0] = ParseExpression(sc, decoClass);
          if (!Args[0]) ParseError(l, "`args` index expression expected");
          sc->Expect("]");
          return new VDecorateInvocation(VName(*Name), l, 1, Args);
        }
        sc->UnGet();
      }
    }
    // skip random generator ID
    if ((Name.ICmp("random") == 0 || Name.ICmp("random2") == 0 || Name.ICmp("frandom") == 0 ||
         Name.ICmp("randompick") == 0 || Name.ICmp("frandompick") == 0) && sc->Check("["))
    {
      sc->ExpectString();
      sc->Expect("]");
    }
    // special argument parsing
    /*
    if (Name.ICmp("GetCvar") == 0) {
      sc->Expect("(");
      auto vnloc = sc->GetLoc();
      sc->ExpectString();
      VStr vname = sc->String;
      sc->Expect(")");
      // create expression
      VExpression *Args[1];
      Args[0] = new VNameLiteral(VName(*vname), vnloc);
      return new VCastOrInvocation(VName("GetCvarF"), l, 1, Args);
    }
    */
    if (sc->Check("(")) return ParseMethodCall(sc, Name, l, true); // paren eaten
    if (sc->String.length() > 2 && sc->String[1] == '_' && (sc->String[0] == 'A' || sc->String[0] == 'a')) {
      return ParseMethodCall(sc, Name, l, false); // paren not eaten
    }
    return new VDecorateSingleName(Name, l);
  }

  return nullptr;
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority1
//
//==========================================================================
static VExpression *ParseExpressionPriority1 (VScriptParser *sc) {
  guard(ParseExpressionPriority1);
  VExpression *op = ParseExpressionPriority0(sc);
  //TLocation l = sc->GetLoc();
  if (!op) return nullptr;
  bool done = false;
  do {
    if (sc->Check("[")) {
      VExpression *ind = ParseExpression(sc, decoClass);
      if (!ind) sc->Error("index expression error");
      sc->Expect("]");
      //op = new VArrayElement(op, ind, l);
      if (!op->IsDecorateSingleName()) sc->Error("cannot index non-array");
      VExpression *e = new VDecorateUserVar(*((VDecorateSingleName *)op)->Name, ind, op->Loc);
      delete op;
      op = e;
    } else {
      done = true;
    }
  } while (!done);
  return op;
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority2
//
//==========================================================================
static VExpression *ParseExpressionPriority2 (VScriptParser *sc) {
  guard(ParseExpressionPriority2);
  VExpression *op;
  TLocation l = sc->GetLoc();

  if (sc->Check("+")) {
    op = ParseExpressionPriority2(sc);
    return new VUnary(VUnary::Plus, op, l);
  }

  if (sc->Check("-")) {
    op = ParseExpressionPriority2(sc);
    return new VUnary(VUnary::Minus, op, l);
  }

  if (sc->Check("!")) {
    op = ParseExpressionPriority2(sc);
    return new VUnary(VUnary::Not, op, l);
  }

  if (sc->Check("~")) {
    op = ParseExpressionPriority2(sc);
    return new VUnary(VUnary::BitInvert, op, l);
  }

  return ParseExpressionPriority1(sc);
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority3
//
//==========================================================================
static VExpression *ParseExpressionPriority3 (VScriptParser *sc) {
  guard(ParseExpressionPriority3);
  VExpression *op1 = ParseExpressionPriority2(sc);
  if (!op1) return nullptr;
  bool done = false;
  do {
    TLocation l = sc->GetLoc();
    if (sc->Check("*")) {
      VExpression *op2 = ParseExpressionPriority2(sc);
      op1 = new VBinary(VBinary::Multiply, op1, op2, l);
    } else if (sc->Check("/")) {
      VExpression *op2 = ParseExpressionPriority2(sc);
      op1 = new VBinary(VBinary::Divide, op1, op2, l);
    } else if (sc->Check("%")) {
      VExpression *op2 = ParseExpressionPriority2(sc);
      op1 = new VBinary(VBinary::Modulus, op1, op2, l);
    } else {
      done = true;
    }
  } while (!done);
  return op1;
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority4
//
//==========================================================================
static VExpression *ParseExpressionPriority4 (VScriptParser *sc) {
  guard(ParseExpressionPriority4);
  VExpression *op1 = ParseExpressionPriority3(sc);
  if (!op1) return nullptr;
  bool done = false;
  do {
    TLocation l = sc->GetLoc();
    if (sc->Check("+")) {
      VExpression *op2 = ParseExpressionPriority3(sc);
      op1 = new VBinary(VBinary::Add, op1, op2, l);
    } else if (sc->Check("-")) {
      VExpression *op2 = ParseExpressionPriority3(sc);
      op1 = new VBinary(VBinary::Subtract, op1, op2, l);
    } else {
      done = true;
    }
  } while (!done);
  return op1;
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority5
//
//==========================================================================
static VExpression *ParseExpressionPriority5 (VScriptParser *sc) {
  guard(ParseExpressionPriority5);
  VExpression *op1 = ParseExpressionPriority4(sc);
  if (!op1) return nullptr;
  bool done = false;
  do {
    TLocation l = sc->GetLoc();
    if (sc->Check("<<")) {
      VExpression *op2 = ParseExpressionPriority4(sc);
      op1 = new VBinary(VBinary::LShift, op1, op2, l);
    } else if (sc->Check(">>")) {
      VExpression *op2 = ParseExpressionPriority4(sc);
      op1 = new VBinary(VBinary::RShift, op1, op2, l);
    } else {
      done = true;
    }
  } while (!done);
  return op1;
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority6
//
//==========================================================================
static VExpression *ParseExpressionPriority6 (VScriptParser *sc) {
  guard(ParseExpressionPriority6);
  VExpression *op1 = ParseExpressionPriority5(sc);
  if (!op1) return nullptr;
  bool done = false;
  do {
    TLocation l = sc->GetLoc();
    if (sc->Check("<")) {
      VExpression *op2 = ParseExpressionPriority5(sc);
      op1 = new VBinary(VBinary::Less, op1, op2, l);
    } else if (sc->Check("<=")) {
      VExpression *op2 = ParseExpressionPriority5(sc);
      op1 = new VBinary(VBinary::LessEquals, op1, op2, l);
    } else if (sc->Check(">")) {
      VExpression *op2 = ParseExpressionPriority5(sc);
      op1 = new VBinary(VBinary::Greater, op1, op2, l);
    } else if (sc->Check(">=")) {
      VExpression *op2 = ParseExpressionPriority5(sc);
      op1 = new VBinary(VBinary::GreaterEquals, op1, op2, l);
    } else {
      done = true;
    }
  } while (!done);
  return op1;
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority7
//
//==========================================================================
static VExpression *ParseExpressionPriority7 (VScriptParser *sc) {
  guard(ParseExpressionPriority7);
  VExpression *op1 = ParseExpressionPriority6(sc);
  if (!op1) return nullptr;
  bool done = false;
  do {
    TLocation l = sc->GetLoc();
    if (sc->Check("==")) {
      VExpression *op2 = ParseExpressionPriority6(sc);
      op1 = new VBinary(VBinary::Equals, op1, op2, l);
    } else if (sc->Check("!=")) {
      VExpression *op2 = ParseExpressionPriority6(sc);
      op1 = new VBinary(VBinary::NotEquals, op1, op2, l);
    } else if (sc->Check("=")) {
      GCon->Logf(NAME_Warning, "%s: hey, dumbhead, use `==` for comparisons!", *sc->GetLoc().toStringNoCol());
      VExpression *op2 = ParseExpressionPriority6(sc);
      op1 = new VBinary(VBinary::NotEquals, op1, op2, l);
    } else {
      done = true;
    }
  } while (!done);
  return op1;
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority8
//
//==========================================================================
static VExpression *ParseExpressionPriority8 (VScriptParser *sc) {
  guard(ParseExpressionPriority8);
  VExpression *op1 = ParseExpressionPriority7(sc);
  if (!op1) return nullptr;
  TLocation l = sc->GetLoc();
  while (sc->Check("&")) {
    VExpression *op2 = ParseExpressionPriority7(sc);
    op1 = new VBinary(VBinary::And, op1, op2, l);
    l = sc->GetLoc();
  }
  return op1;
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority9
//
//==========================================================================
static VExpression *ParseExpressionPriority9 (VScriptParser *sc) {
  guard(ParseExpressionPriority9);
  VExpression *op1 = ParseExpressionPriority8(sc);
  if (!op1) return nullptr;
  TLocation l = sc->GetLoc();
  while (sc->Check("^")) {
    VExpression *op2 = ParseExpressionPriority8(sc);
    op1 = new VBinary(VBinary::XOr, op1, op2, l);
    l = sc->GetLoc();
  }
  return op1;
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority10
//
//==========================================================================
static VExpression *ParseExpressionPriority10 (VScriptParser *sc) {
  guard(ParseExpressionPriority10);
  VExpression *op1 = ParseExpressionPriority9(sc);
  if (!op1) return nullptr;
  TLocation l = sc->GetLoc();
  while (sc->Check("|")) {
    VExpression *op2 = ParseExpressionPriority9(sc);
    op1 = new VBinary(VBinary::Or, op1, op2, l);
    l = sc->GetLoc();
  }
  return op1;
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority11
//
//==========================================================================
static VExpression *ParseExpressionPriority11 (VScriptParser *sc) {
  guard(ParseExpressionPriority11);
  VExpression *op1 = ParseExpressionPriority10(sc);
  if (!op1) return nullptr;
  TLocation l = sc->GetLoc();
  while (sc->Check("&&")) {
    VExpression *op2 = ParseExpressionPriority10(sc);
    op1 = new VBinaryLogical(VBinaryLogical::And, op1, op2, l);
    l = sc->GetLoc();
  }
  return op1;
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority12
//
//==========================================================================
static VExpression *ParseExpressionPriority12 (VScriptParser *sc) {
  guard(ParseExpressionPriority12);
  VExpression *op1 = ParseExpressionPriority11(sc);
  if (!op1) return nullptr;
  TLocation l = sc->GetLoc();
  while (sc->Check("||")) {
    VExpression *op2 = ParseExpressionPriority11(sc);
    op1 = new VBinaryLogical(VBinaryLogical::Or, op1, op2, l);
    l = sc->GetLoc();
  }
  return op1;
  unguard;
}


//==========================================================================
//
//  VParser::ParseExpressionPriority13
//
//==========================================================================
static VExpression *ParseExpressionPriority13 (VScriptParser *sc, VClass *Class) {
  guard(ParseExpressionPriority13);
  VClass *olddc = decoClass;
  decoClass = Class;
  VExpression *op = ParseExpressionPriority12(sc);
  if (!op) { decoClass = olddc; return nullptr; }
  TLocation l = sc->GetLoc();
  /*
  if (inCodeBlock && sc->Check("=")) {
    //return new VAssignment(VAssignment::Assign, op1, op2, stloc);
    abort();
  }
  */
  if (sc->Check("?")) {
    VExpression *op1 = ParseExpressionPriority13(sc, Class);
    sc->Expect(":");
    VExpression *op2 = ParseExpressionPriority13(sc, Class);
    op = new VConditional(op, op1, op2, l);
  }
  decoClass = olddc;
  return op;
  unguard;
}


//==========================================================================
//
//  ParseExpression
//
//==========================================================================
static VExpression *ParseExpression (VScriptParser *sc, VClass *Class) {
  guard(ParseExpression);
  return ParseExpressionPriority13(sc, Class);
  unguard;
}


//==========================================================================
//
//  ParseFunCallAsStmt
//
//==========================================================================
static VStatement *ParseFunCallAsStmt (VScriptParser *sc, VClass *Class, VState *State) {
  // get function name and parse arguments
  //auto actionLoc = sc->GetLoc();
  VExpression *Args[VMethod::MAX_PARAMS+1];
  int NumArgs = 0;

  sc->ExpectIdentifier();
  VStr FuncName = sc->String;
  auto stloc = sc->GetLoc();

  VStatement *suvst = CheckParseSetUserVarStmt(sc, Class, FuncName);
  if (suvst) return suvst;
  //fprintf(stderr, "***1:<%s>\n", *sc->String);

  // assign?
  if (inCodeBlock) {
    VExpression *dest = nullptr;
    if (sc->Check("[")) {
      dest = new VDecorateSingleName(FuncName, stloc);
      VExpression *ind = ParseExpression(sc, Class);
      if (!ind) sc->Error("decorate parsing error");
      sc->Expect("]");
      dest = new VArrayElement(dest, ind, stloc);
      sc->Expect("=");
    } else if (sc->Check("=")) {
      dest = new VDecorateSingleName(FuncName, stloc);
      //GCon->Logf("ASS to '%s'...", *FuncName);
    }
    if (dest) {
      // we're supporting assign to array element or simple names
      // convert "single name" to uservar access
      if (dest->IsDecorateSingleName()) {
        VExpression *e = new VDecorateUserVar(*((VDecorateSingleName *)dest)->Name, dest->Loc);
        delete dest;
        dest = e;
      }
      if (!dest->IsDecorateUserVar()) sc->Error("cannot assign to non-field");
      VExpression *val = ParseExpression(sc, Class);
      if (!val) sc->Error("decorate parsing error");
      VExpression *ass = new VAssignment(VAssignment::Assign, dest, val, stloc);
      return new VExpressionStatement(new VDropResult(ass));
    }
  }

  VMethod *Func = ParseFunCallWithName(sc, FuncName, Class, NumArgs, Args, false); // no paren
  //fprintf(stderr, "***2:<%s>\n", *sc->String);

  VExpression *callExpr = nullptr;
  if (!Func) {
    //GCon->Logf("ERROR000: %s: Unknown state action `%s` in `%s` (replaced with NOP)", *actionLoc.toStringNoCol(), *FuncName, Class->GetName());
    //return nullptr;
    callExpr = new VDecorateInvocation(VName(*FuncName/*, VName::AddLower*/), stloc, NumArgs, Args);
  } else {
    VInvocation *Expr = new VInvocation(nullptr, Func, nullptr, false, false, stloc, NumArgs, Args);
    Expr->CallerState = State;
    callExpr = Expr;
  }
  return new VExpressionStatement(new VDropResult(callExpr));
}


//==========================================================================
//
//  ParseActionStatement
//
//==========================================================================
static VStatement *ParseActionStatement (VScriptParser *sc, VClass *Class, VState *State) {
  if (sc->Check("{")) {
    VCompound *stmt = new VCompound(sc->GetLoc());
    while (!sc->Check("}")) {
      if (sc->Check(";")) continue;
      VStatement *st;
      if (sc->Check("{")) sc->UnGet(); // cheat a little
      st = ParseActionStatement(sc, Class, State);
      if (st) stmt->Statements.append(st);
    }
    return stmt;
  }

  if (sc->Check(";")) return nullptr;

  auto stloc = sc->GetLoc();

  // if
  if (sc->Check("if")) {
    sc->Expect("(");
    VExpression *cond = ParseExpression(sc, Class);
    sc->Expect(")");
    if (!cond) sc->Error("invalid `if` expression");
    VStatement *ts = ParseActionStatement(sc, Class, State);
    if (!ts) sc->Error("invalid `if` true branch");
    if (sc->Check("else")) {
      VStatement *fs = ParseActionStatement(sc, Class, State);
      if (fs) return new VIf(cond, ts, fs, stloc);
    }
    return new VIf(cond, ts, stloc);
  }

  // return
  if (sc->Check("return")) {
    if (sc->Check("state")) {
      // return state("name");
      // specials: `state()`, `state(0)`, `state("")`
      sc->Expect("(");
      if (sc->Check(")")) {
        sc->Expect(";");
        return new VReturn(nullptr, stloc);
      }
      VExpression *ste = ParseExpression(sc, Class);
      if (!ste) sc->Error("invalid `return`");
      sc->Expect(")");
      sc->Expect(";");
      // check for specials
      // replace `+number` with `number`
      for (;;) {
        if (ste->IsUnaryMath()) {
          VUnary *un = (VUnary *)ste;
          if (un->op) {
            if (un->Oper == VUnary::Plus && (un->op->IsIntConst() || un->op->IsFloatConst())) {
              VExpression *etmp = un->op;
              un->op = nullptr;
              delete ste;
              ste = etmp;
              continue;
            }
          }
        }
        break;
      }
      if (!ste) sc->Error("invalid `return`"); // just in case
      // `state(0)`?
      if ((ste->IsIntConst() && ste->GetIntConst() == 0) ||
          (ste->IsFloatConst() && ste->GetFloatConst() == 0))
      {
        delete ste;
        return new VReturn(nullptr, stloc);
      }
      // `state("")`?
      if (ste->IsStrConst()) {
        VStr lbl = ste->GetStrConst(DecPkg);
        while (lbl.length() && (vuint8)lbl[0] <= 32) lbl.chopLeft(1);
        while (lbl.length() && (vuint8)lbl[lbl.length()-1] <= 32) lbl.chopRight(1);
        if (lbl.length() == 0) {
          delete ste;
          return new VReturn(nullptr, stloc);
        }
      }
      if (ste->IsDecorateSingleName()) {
        VDecorateSingleName *e = (VDecorateSingleName *)ste;
        if (e->Name.length() == 0) {
          delete ste;
          return new VReturn(nullptr, stloc);
        }
      }
      // call `decorate_A_RetDoJump`
      VExpression *Args[1];
      Args[0] = ste;
      VExpression *einv = new VDecorateInvocation(VName("decorate_A_RetDoJump"), stloc, 1, Args);
      VStatement *stinv = new VExpressionStatement(new VDropResult(einv));
      VCompound *cst = new VCompound(stloc);
      cst->Statements.append(stinv);
      cst->Statements.append(new VReturn(nullptr, stloc));
      return cst;
    } else if (sc->Check(";")) {
      // return;
      return new VReturn(nullptr, stloc);
    } else {
      // return expr;
      // just call `expr`, and do normal return
      VStatement *fs = ParseActionStatement(sc, Class, State);
      if (!fs) return new VReturn(nullptr, stloc);
      // create compound
      VCompound *cst = new VCompound(stloc);
      cst->Statements.append(fs);
      cst->Statements.append(new VReturn(nullptr, stloc));
      return cst;
    }
  }

  if (sc->Check("for")) sc->Error("`for` is not supported");
  if (sc->Check("while")) sc->Error("`while` is not supported");
  if (sc->Check("do")) sc->Error("`do` is not supported");

  VStatement *res = ParseFunCallAsStmt(sc, Class, State);
  sc->Expect(";");
  if (!res) sc->Error("invalid action statement");
  return res;
}


//==========================================================================
//
//  ParseActionBlock
//
//  "{" checked
//
//==========================================================================
static void ParseActionBlock (VScriptParser *sc, VClass *Class, VState *State) {
  bool oldicb = inCodeBlock;
  inCodeBlock = true;
  VCompound *stmt = new VCompound(sc->GetLoc());
  while (!sc->Check("}")) {
    VStatement *st = ParseActionStatement(sc, Class, State);
    if (!st) continue;
    stmt->Statements.append(st);
  }
  inCodeBlock = oldicb;

  if (stmt->Statements.length()) {
    VMethod *M = new VMethod(NAME_None, Class, sc->GetLoc());
    M->Flags = FUNC_Final;
    M->ReturnTypeExpr = new VTypeExprSimple(TYPE_Void, sc->GetLoc());
    M->ReturnType = VFieldType(TYPE_Void);
    M->Statement = stmt;
    M->NumParams = 0;
    Class->AddMethod(M);
    M->Define();
    State->Function = M;
  } else {
    delete stmt;
    State->Function = nullptr;
  }
}


//==========================================================================
//
//  ParseActionCall
//
//  parse single decorate action
//
//==========================================================================
static void ParseActionCall (VScriptParser *sc, VClass *Class, VState *State) {
  // get function name and parse arguments
  //fprintf(stderr, "!!!***000: <%s> (%s)\n", *sc->String, *FuncName);
  //fprintf(stderr, "!!!***000: <%s>\n", *sc->String);
  sc->ExpectIdentifier();
  //fprintf(stderr, "!!!***001: <%s>\n", *sc->String);

  VExpression *Args[VMethod::MAX_PARAMS+1];
  int NumArgs = 0;
  auto actionLoc = sc->GetLoc();
  VStr FuncName = sc->String;
  VMethod *Func = nullptr;

  VStatement *suvst = CheckParseSetUserVarStmt(sc, Class, FuncName);
  if (suvst) {
    VMethod *M = new VMethod(NAME_None, Class, sc->GetLoc());
    M->Flags = FUNC_Final;
    M->ReturnTypeExpr = new VTypeExprSimple(TYPE_Void, sc->GetLoc());
    M->ReturnType = VFieldType(TYPE_Void);
    M->Statement = suvst;
    M->NumParams = 0;
    //M->ParamsSize = 1;
    Class->AddMethod(M);
    M->Define();
    Func = M;
  } else {
    Func = ParseFunCallWithName(sc, FuncName, Class, NumArgs, Args, false); // no paren
    //fprintf(stderr, "<%s>\n", *FuncNameLower);
    if (!Func) {
      GCon->Logf(NAME_Warning, "%s: Unknown state action `%s` in `%s` (replaced with NOP)", *actionLoc.toStringNoCol(), *FuncName, Class->GetName());
      // if function is not found, it means something is wrong
      // in that case we need to free argument expressions
      for (int i = 0; i < NumArgs; ++i) {
        if (Args[i]) {
          delete Args[i];
          Args[i] = nullptr;
        }
      }
    } else if (Func->NumParams || NumArgs /*|| FuncName.ICmp("a_explode") == 0*/) {
      VInvocation *Expr = new VInvocation(nullptr, Func, nullptr, false, false, sc->GetLoc(), NumArgs, Args);
      Expr->CallerState = State;
      VExpressionStatement *Stmt = new VExpressionStatement(new VDropResult(Expr));
      VMethod *M = new VMethod(NAME_None, Class, sc->GetLoc());
      M->Flags = FUNC_Final;
      M->ReturnTypeExpr = new VTypeExprSimple(TYPE_Void, sc->GetLoc());
      M->ReturnType = VFieldType(TYPE_Void);
      M->Statement = Stmt;
      M->NumParams = 0;
      Class->AddMethod(M);
      M->Define();
      Func = M;
    }
  }

  State->Function = Func;
  if (sc->Check(";")) {}
}


