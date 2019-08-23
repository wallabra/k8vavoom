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
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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
// this source is directly included from "vc_decorate.cpp"

static VExpression *ParseExpressionNoAssign (VScriptParser *sc, VClass *Class);


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
    VExpression *val = ParseExpressionNoAssign(sc, Class);
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
    VExpression *idx = ParseExpressionNoAssign(sc, Class);
    if (!idx) sc->Error("decorate parsing error");
    sc->Expect(",");
    // value
    VExpression *val = ParseExpressionNoAssign(sc, Class);
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
//  VExpression
//
//==========================================================================
static VExpression *ParseAJump (VScriptParser *sc, VClass *Class, VState *State) {
  VDecorateAJump *jexpr = new VDecorateAJump(sc->GetLoc()); //FIXME: MEMLEAK!
  jexpr->CallerState = State;
  sc->Expect("(");
  VExpression *prob = ParseExpressionNoAssign(sc, Class);
  if (!prob) {
    ParseError(sc->GetLoc(), "`A_Jump` oops (0)!");
    sc->Expect(")");
    return jexpr;
  }
  jexpr->prob = prob;
  if (sc->Check(",")) {
    do {
      VExpression *arg = ParseExpressionNoAssign(sc, Class);
      if (!arg) {
        ParseError(sc->GetLoc(), "`A_Jump` oops (1)!");
        sc->Expect(")");
        return jexpr;
      }
      jexpr->labels.append(arg);
    } while (sc->Check(","));
  }
  sc->Expect(")");
  return jexpr;
}


//==========================================================================
//
//  ParseRandomPick
//
//  paren eated
//
//==========================================================================
static VExpression *ParseRandomPick (VScriptParser *sc, VClass *Class, bool asFloat) {
  VDecorateRndPick *pk = new VDecorateRndPick(asFloat, sc->GetLoc()); //FIXME: MEMLEAK!
  if (sc->Check(")")) {
    ParseError(sc->GetLoc(), "`%srandompick` expects some arguments!", (asFloat ? "f" : ""));
    return pk;
  }
  do {
    VExpression *num = ParseExpressionNoAssign(sc, Class);
    if (!num) {
      ParseError(sc->GetLoc(), "`%srandompick` oops!", (asFloat ? "f" : ""));
      sc->Expect(")");
      return pk;
    }
    pk->numbers.append(num);
  } while (sc->Check(","));
  sc->Expect(")");
  return pk;
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

  auto oldEsc = sc->IsEscape();
  sc->SetEscape(true);

  //fprintf(stderr, "***8:<%s> %s\n", *sc->String, *sc->GetLoc().toStringNoCol());
  if (!gotParen) gotParen = sc->Check("(");
  if (gotParen) {
    if (!sc->Check(")")) {
      do {
        ++totalCount;
        Args[NumArgs] = ParseExpressionNoAssign(sc, Class);
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
  sc->SetEscape(oldEsc);

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
      VStr str = Args[f]->GetStrConst(DecPkg);
      if (!str.startsWithNoCase("user_")) continue;
      auto loc = Args[f]->Loc;
      ParseWarning(loc, "`user_xxx` should not be a string constant, you mo...dder! FIX YOUR BROKEN CODE!");
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
static VExpression *ParseMethodCall (VScriptParser *sc, VClass *Class, VStr Name, TLocation Loc, bool parenEaten=true) {
  VExpression *Args[VMethod::MAX_PARAMS+1];
  int NumArgs = 0;
  VMethod *Func = ParseFunCallWithName(sc, Name, Class, NumArgs, Args, parenEaten); // got paren
  /*
  if (Name.ICmp("random") == 0) {
    fprintf(stderr, "*** RANDOM; NumArgs=%d (%s); func=%p (%s, %s)\n", NumArgs, *sc->GetLoc().toStringNoCol(), Func, *Args[0]->toString(), *Args[1]->toString());
  }
  */
  return new VDecorateInvocation((Func ? Func->GetVName() : VName(*Name, VName::AddLower)), Loc, NumArgs, Args);
}


// ////////////////////////////////////////////////////////////////////////// //
typedef VExpression *(*ExprOpCB) (VScriptParser *sc, VClass *Class, const TLocation &l, VExpression *lhs, VExpression *rhs);

struct MathOpHandler {
  int prio; // negative means "right-associative" (not implemented yet)
  const char *op;
  ExprOpCB cb; // nullptr means "no more"
};

#define DEFOP(prio_,name_) \
  { .prio = prio_, .op = name_, .cb = [](VScriptParser *sc, VClass *Class, const TLocation &l, VExpression *lhs, VExpression *rhs) -> VExpression *

#define ENDOP }

static const MathOpHandler oplist[] = {
  // unaries; rhs has no sense here
  DEFOP(2, "+") { return new VUnary(VUnary::Plus, lhs, l); } ENDOP,
  DEFOP(2, "-") { return new VUnary(VUnary::Minus, lhs, l); } ENDOP,
  DEFOP(2, "!") { return new VUnary(VUnary::Not, lhs, l); } ENDOP,
  DEFOP(2, "~") { return new VUnary(VUnary::BitInvert, lhs, l); } ENDOP,

  // binarues
  DEFOP(3, "*") { return new VBinary(VBinary::Multiply, lhs, rhs, l); } ENDOP,
  DEFOP(3, "/") { return new VBinary(VBinary::Divide, lhs, rhs, l); } ENDOP,
  DEFOP(3, "%") { return new VBinary(VBinary::Modulus, lhs, rhs, l); } ENDOP,

  DEFOP(4, "+") { return new VBinary(VBinary::Add, lhs, rhs, l); } ENDOP,
  DEFOP(4, "-") { return new VBinary(VBinary::Subtract, lhs, rhs, l); } ENDOP,

  DEFOP(5, "<<") { return new VBinary(VBinary::LShift, lhs, rhs, l); } ENDOP,
  DEFOP(5, ">>") { return new VBinary(VBinary::RShift, lhs, rhs, l); } ENDOP,

  DEFOP(6, "<") { return new VBinary(VBinary::Less, lhs, rhs, l); } ENDOP,
  DEFOP(6, "<=") { return new VBinary(VBinary::LessEquals, lhs, rhs, l); } ENDOP,
  DEFOP(6, ">") { return new VBinary(VBinary::Greater, lhs, rhs, l); } ENDOP,
  DEFOP(6, ">=") { return new VBinary(VBinary::GreaterEquals, lhs, rhs, l); } ENDOP,

  DEFOP(7, "==") { return new VBinary(VBinary::Equals, lhs, rhs, l); } ENDOP,
  DEFOP(7, "!=") { return new VBinary(VBinary::NotEquals, lhs, rhs, l); } ENDOP,
  DEFOP(7, "=") {
    GLog.Logf(NAME_Warning, "%s: hey, dumbhead, use `==` for comparisons!", *l.toStringNoCol());
    return new VBinary(VBinary::NotEquals, lhs, rhs, l);
  } ENDOP,

  DEFOP(8, "&") { return new VBinary(VBinary::And, lhs, rhs, l); } ENDOP,
  DEFOP(9, "^") { return new VBinary(VBinary::XOr, lhs, rhs, l); } ENDOP,
  DEFOP(10, "|") { return new VBinary(VBinary::Or, lhs, rhs, l); } ENDOP,

  DEFOP(11, "&&") { return new VBinaryLogical(VBinaryLogical::And, lhs, rhs, l); } ENDOP,
  DEFOP(12, "||") { return new VBinaryLogical(VBinaryLogical::Or, lhs, rhs, l); } ENDOP,

  // 13 is ternary, it has no callback (special case)

  { .prio = 0, .op = nullptr, .cb = nullptr },
};

#define PRIO_TERNARY  (13)
// for now
#define PRIO_MAX        PRIO_TERNARY
// for now
#define PRIO_NO_ASSIGN  PRIO_TERNARY

#undef DEFOP
#undef ENDOP


//==========================================================================
//
//  VParser::ParseExpressionGeneral
//
//==========================================================================
static VExpression *ParseExpressionGeneral (VScriptParser *sc, VClass *Class, int prio) {
  check(prio >= 0);

  // term
  if (prio == 0) {
    // check for quoted strings first, since these could also have numbers...
    if (sc->CheckQuotedString()) {
      int Val = DecPkg->FindString(*sc->String);
      return new VStringLiteral(sc->String, Val, sc->GetLoc());
    }

    // integer?
    if (sc->CheckNumber()) {
      vint32 Val = sc->Number;
      return new VIntLiteral(Val, sc->GetLoc());
    }

    // float?
    if (sc->CheckFloat()) {
      float Val = sc->Float;
      return new VFloatLiteral(Val, sc->GetLoc());
    }

    // booleans?
    if (sc->Check("false")) return new VIntLiteral(0, sc->GetLoc());
    if (sc->Check("true")) return new VIntLiteral(1, sc->GetLoc());

    // subexpression?
    if (sc->Check("(")) {
      const TLocation l = sc->GetLoc();
      VExpression *op = ParseExpressionGeneral(sc, Class, PRIO_NO_ASSIGN);
      if (!op) ParseError(l, "Expression expected");
      sc->Expect(")");
      return new VExprParens(op, l);
    }

    // identifier?
    if (sc->CheckIdentifier()) {
      const TLocation l = sc->GetLoc();
      VStr Name = sc->String;
      if (Name.strEquCI("args")) {
        if (sc->Check("[")) {
          const TLocation lidx = sc->GetLoc();
          Name = VStr("GetArg");
          //fprintf(stderr, "*** ARGS ***\n");
          VExpression *Args[1];
          Args[0] = ParseExpressionGeneral(sc, Class, PRIO_NO_ASSIGN);
          if (!Args[0]) ParseError(lidx, "`args` index expression expected");
          sc->Expect("]");
          return new VDecorateInvocation(VName(*Name), lidx, 1, Args);
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
      if (sc->Check("(")) {
        if (Name.strEquCI("randompick") || Name.strEquCI("frandompick")) {
          return ParseRandomPick(sc, Class, Name.strEquCI("frandompick"));
        }
        return ParseMethodCall(sc, Class, Name, l, true); // paren eaten
      }
      if (sc->String.length() > 2 && sc->String[1] == '_' && (sc->String[0] == 'A' || sc->String[0] == 'a')) {
        return ParseMethodCall(sc, Class, Name, l, false); // paren not eaten
      }
      return new VDecorateSingleName(Name, l);
    }

    // some unknown shit
    return nullptr;
  }

  // indexing
  if (prio == 1) {
    VExpression *op = ParseExpressionGeneral(sc, Class, prio-1);
    if (!op) return nullptr;
    for (;;) {
      if (!sc->Check("[")) break;
      VExpression *ind = ParseExpressionNoAssign(sc, Class);
      if (!ind) sc->Error("index expression error");
      sc->Expect("]");
      //op = new VArrayElement(op, ind, l);
      if (!op->IsDecorateSingleName()) sc->Error("cannot index non-array");
      VExpression *e = new VDecorateUserVar(*((VDecorateSingleName *)op)->Name, ind, op->Loc);
      delete op;
      op = e;
    }
    return op;
  }

  // unaries
  if (prio == 2) {
    for (const MathOpHandler *mop = oplist; mop->op; ++mop) {
      if (mop->prio != prio) continue;
      if (sc->Check(mop->op)) {
        const TLocation l = sc->GetLoc();
        VExpression *lhs = ParseExpressionGeneral(sc, Class, prio); // rassoc
        if (!lhs) return nullptr;
        // as this is right-associative, return here
        return mop->cb(sc, Class, l, lhs, nullptr);
      }
    }
    // not found, try higher priority
    return ParseExpressionGeneral(sc, Class, prio-1);
  }

  // ternary
  if (prio == PRIO_TERNARY) {
    VExpression *cond = ParseExpressionGeneral(sc, Class, prio-1);
    if (!cond) return nullptr;
    if (sc->Check("?")) {
      const TLocation l = sc->GetLoc();
      VExpression *op1 = ParseExpressionGeneral(sc, Class, prio); // rassoc
      if (!op1) { delete cond; return nullptr; }
      sc->Expect(":");
      VExpression *op2 = ParseExpressionGeneral(sc, Class, prio); // rassoc
      if (!op2) { delete op1; delete cond; return nullptr; }
      cond = new VConditional(cond, op1, op2, l);
    }
    return cond;
  }

  // binaries
  VExpression *lhs = ParseExpressionGeneral(sc, Class, prio-1);
  if (!lhs) return nullptr; // some error
  for (;;) {
    const MathOpHandler *mop = oplist;
    // get token, this is slightly faster
    if (!sc->GetString()) break; // no more code
    VStr token = sc->String;
    if (!inCodeBlock && token.strEqu("||")) {
      if (prio == 10) GCon->Logf(NAME_Error, "%s: in decorate, you shall use `|` to combine constants, or you will be executed!", *sc->GetLoc().toStringNoCol());
      token = "|";
    }
    // find math operator
    for (; mop->op; ++mop) {
      if (mop->prio != prio) continue;
      if (token.strEqu(mop->op)) break;
    }
    // not found?
    if (!mop->op) {
      // we're done with this priority level, restore a token
      sc->UnGet();
      break;
    }
    // get rhs
    const TLocation l = sc->GetLoc();
    VExpression *rhs = ParseExpressionGeneral(sc, Class, prio-1); // use `prio` for rassoc, and immediately return (see above)
    if (!rhs) { delete lhs; return nullptr; } // some error
    lhs = mop->cb(sc, Class, l, lhs, rhs);
    if (!lhs) return nullptr; // some error
    // do it all again...
  }
  return lhs;
}


//==========================================================================
//
//  ParseExpressionNoAssign
//
//==========================================================================
static VExpression *ParseExpressionNoAssign (VScriptParser *sc, VClass *Class) {
  return ParseExpressionGeneral(sc, Class, PRIO_NO_ASSIGN);
}


//==========================================================================
//
//  ParseExpression
//
//==========================================================================
static VExpression *ParseExpression (VScriptParser *sc, VClass *Class) {
  if (sc->Check("A_SetUserVar") || sc->Check("A_SetUserVarFloat") ||
      sc->Check("A_SetUserArray") || sc->Check("A_SetUserArrayFloat"))
  {
    VStr FuncName = sc->String;
    VExpression *asse = CheckParseSetUserVarExpr(sc, Class, FuncName);
    if (!asse) sc->Error("error in decorate");
    return asse;
  }

  return ParseExpressionNoAssign(sc, Class);
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
      VExpression *ind = ParseExpressionNoAssign(sc, Class);
      if (!ind) sc->Error("decorate parsing error");
      sc->Expect("]");
      dest = new VArrayElement(dest, ind, stloc);
      sc->Expect("=");
    } else if (sc->Check("=")) {
      dest = new VDecorateSingleName(FuncName, stloc);
      //GLog.Logf("ASS to '%s'...", *FuncName);
    }
    if (dest) {
      // we're supporting assign to array element or simple names
      // convert "single name" to uservar access
      if (dest->IsDecorateSingleName()) {
        VExpression *e = new VDecorateUserVar(*((VDecorateSingleName *)dest)->Name, dest->Loc);
        delete dest;
        dest = e;
      }
      if (!dest->IsDecorateUserVar()) sc->Error(va("cannot assign to non-field `%s`", *FuncName));
      VExpression *val = ParseExpressionNoAssign(sc, Class);
      if (!val) sc->Error("decorate parsing error");
      VExpression *ass = new VAssignment(VAssignment::Assign, dest, val, stloc);
      return new VExpressionStatement(new VDropResult(ass));
    }
  }

  if (FuncName.strEquCI("A_Jump")) {
    VExpression *jexpr = ParseAJump(sc, Class, State);
    return new VExpressionStatement(jexpr);
  } else {
    //k8: THIS IS ABSOLUTELY HORRIBLY HACK! MAKE IT RIGHT IN TERM PARSER INSTEAD!
    auto ll = sc->GetLoc();
    if (sc->Check("++")) {
      if (!FuncName.startsWithCI("user_")) sc->Error(va("cannot assign to non-unservar `%s`", *FuncName));
      //VExpression *dest = new VDecorateSingleName(FuncName, stloc);
      //if (!dest->IsDecorateUserVar()) sc->Error(va("cannot assign to non-field `%s`", *FuncName));
      VExpression *dest = new VDecorateUserVar(*FuncName, stloc);
      VExpression *op1 = dest->SyntaxCopy();
      VExpression *op2 = new VIntLiteral(1, ll);
      VExpression *val = new VBinary(VBinary::Add, op1, op2, ll);
      VExpression *ass = new VAssignment(VAssignment::Assign, dest, val, stloc);
      //fprintf(stderr, "**%s: <%s> **\n", *sc->GetLoc().toStringNoCol(), *ass->toString());
      return new VExpressionStatement(new VDropResult(ass));
    }

    VMethod *Func = ParseFunCallWithName(sc, FuncName, Class, NumArgs, Args, false); // no paren
    //fprintf(stderr, "***2:<%s>\n", *sc->String);

    VExpression *callExpr = nullptr;
    if (!Func) {
      //GLog.Logf("ERROR000: %s: Unknown state action `%s` in `%s` (replaced with NOP)", *actionLoc.toStringNoCol(), *FuncName, Class->GetName());
      //return nullptr;
      callExpr = new VDecorateInvocation(VName(*FuncName/*, VName::AddLower*/), stloc, NumArgs, Args);
    } else {
      VInvocation *Expr = new VInvocation(nullptr, Func, nullptr, false, false, stloc, NumArgs, Args);
      Expr->CallerState = State;
      callExpr = Expr;
    }
    return new VExpressionStatement(new VDropResult(callExpr));
  }
}


//==========================================================================
//
//  CheckUnsafeStatement
//
//==========================================================================
static void CheckUnsafeStatement (VScriptParser *sc, const char *msg) {
  if (GArgs.CheckParm("-decorate-allow-unsafe") || GArgs.CheckParm("--decorate-allow-unsafe")) return;
  GCon->Logf(NAME_Error, "Unsafe decorate statement found.");
  GCon->Logf(NAME_Error, "You can allow unsafe statements with \"-decorate-allow-unsafe\", but it is not recommeded.");
  GCon->Logf(NAME_Error, "The engine can crash or hang with such mods.");
  sc->Error(msg);
}


// forward declaration
static VStatement *ParseActionStatement (VScriptParser *sc, VClass *Class, VState *State);


//==========================================================================
//
//  ParseStatementFor
//
//==========================================================================
static VStatement *ParseStatementFor (VScriptParser *sc, VClass *Class, VState *State) {
  CheckUnsafeStatement(sc, "`for` is not allowed");

  auto stloc = sc->GetLoc();

  sc->Expect("(");

  VFor *forstmt = new VFor(stloc);

  // parse init expr(s)
  if (!sc->Check(";")) {
    for (;;) {
      VExpression *expr = ParseExpression(sc, Class);
      if (!expr) break;
      forstmt->InitExpr.append(new VDropResult(expr));
      // here should be a comma or a semicolon
      if (!sc->Check(",")) break;
    }
    sc->Expect(";");
  }

  // parse cond expr(s)
  VExpression *lastCondExpr = nullptr;
  if (!sc->Check(";")) {
    for (;;) {
      VExpression *expr = ParseExpression(sc, Class);
      if (!expr) break;
      if (lastCondExpr) forstmt->CondExpr.append(new VDropResult(lastCondExpr));
      lastCondExpr = expr;
      // here should be a comma or a semicolon
      if (!sc->Check(",")) break;
    }
    sc->Expect(";");
  }
  if (lastCondExpr) forstmt->CondExpr.append(lastCondExpr);

  // parse loop expr(s)
  if (!sc->Check(")")) {
    for (;;) {
      VExpression *expr = ParseExpression(sc, Class);
      if (!expr) break;
      forstmt->LoopExpr.append(new VDropResult(expr));
      // here should be a comma or a right paren
      if (!sc->Check(",")) break;
    }
    sc->Expect(")");
  }

  VStatement *body = ParseActionStatement(sc, Class, State);
  forstmt->Statement = body;
  return forstmt;
}


//==========================================================================
//
//  ParseStatementWhile
//
//==========================================================================
static VStatement *ParseStatementWhile (VScriptParser *sc, VClass *Class, VState *State) {
  CheckUnsafeStatement(sc, "`while` is not allowed");

  auto stloc = sc->GetLoc();

  sc->Expect("(");
  VExpression *cond = ParseExpression(sc, Class);
  if (!cond) sc->Error("`while` loop expression expected");
  sc->Expect(")");

  VStatement *body = ParseActionStatement(sc, Class, State);

  return new VWhile(cond, body, stloc);
}


//==========================================================================
//
//  ParseStatementDo
//
//==========================================================================
static VStatement *ParseStatementDo (VScriptParser *sc, VClass *Class, VState *State) {
  CheckUnsafeStatement(sc, "`do` is not allowed");

  auto stloc = sc->GetLoc();

  VStatement *body = ParseActionStatement(sc, Class, State);

  sc->Expect("while");
  sc->Expect("(");
  VExpression *cond = ParseExpression(sc, Class);
  if (!cond) sc->Error("`do` loop expression expected");
  sc->Expect(")");

  return new VDo(cond, body, stloc);
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
    auto elseloc = sc->GetLoc();
    if (sc->Check("else")) {
      VStatement *fs = ParseActionStatement(sc, Class, State);
      if (fs) return new VIf(cond, ts, fs, stloc, elseloc, false);
    }
    return new VIf(cond, ts, stloc, false);
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

  if (sc->Check("for")) return ParseStatementFor(sc, Class, State);
  if (sc->Check("while")) return ParseStatementWhile(sc, Class, State);
  if (sc->Check("do")) return ParseStatementDo(sc, Class, State);

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
#if defined(VC_DECORATE_ACTION_BELONGS_TO_STATE)
    VMethod *M = new VMethod(NAME_None, State, sc->GetLoc());
#else
    VMethod *M = new VMethod(NAME_None, Class, sc->GetLoc());
#endif
    M->Flags = FUNC_Final;
    M->ReturnTypeExpr = new VTypeExprSimple(TYPE_Void, sc->GetLoc());
    M->ReturnType = VFieldType(TYPE_Void);
    M->Statement = stmt;
    M->NumParams = 0;
#if !defined(VC_DECORATE_ACTION_BELONGS_TO_STATE)
    Class->AddMethod(M);
    M->Define();
#endif
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
#if defined(VC_DECORATE_ACTION_BELONGS_TO_STATE)
    VMethod *M = new VMethod(NAME_None, State, sc->GetLoc());
#else
    VMethod *M = new VMethod(NAME_None, Class, sc->GetLoc());
#endif
    M->Flags = FUNC_Final;
    M->ReturnTypeExpr = new VTypeExprSimple(TYPE_Void, sc->GetLoc());
    M->ReturnType = VFieldType(TYPE_Void);
    M->Statement = suvst;
    M->NumParams = 0;
#if !defined(VC_DECORATE_ACTION_BELONGS_TO_STATE)
    Class->AddMethod(M);
    M->Define();
#endif
    Func = M;
  } else {
    if (VStr::ICmp(*FuncName, "A_Jump") == 0) {
      VExpression *jexpr = ParseAJump(sc, Class, State);
      VExpressionStatement *Stmt = new VExpressionStatement(jexpr);
#if defined(VC_DECORATE_ACTION_BELONGS_TO_STATE)
      VMethod *M = new VMethod(NAME_None, State, sc->GetLoc());
#else
      VMethod *M = new VMethod(NAME_None, Class, sc->GetLoc());
#endif
      M->Flags = FUNC_Final;
      M->ReturnTypeExpr = new VTypeExprSimple(TYPE_Void, sc->GetLoc());
      M->ReturnType = VFieldType(TYPE_Void);
      M->Statement = Stmt;
      M->NumParams = 0;
#if !defined(VC_DECORATE_ACTION_BELONGS_TO_STATE)
      Class->AddMethod(M);
      M->Define();
#endif
      Func = M;
    } else {
      Func = ParseFunCallWithName(sc, FuncName, Class, NumArgs, Args, false); // no paren
      //fprintf(stderr, "<%s>\n", *FuncNameLower);
      if (!Func) {
        GLog.Logf(NAME_Warning, "%s: Unknown state action `%s` in `%s` (replaced with NOP)", *actionLoc.toStringNoCol(), *FuncName, Class->GetName());
        // if function is not found, it means something is wrong
        // in that case we need to free argument expressions
        for (int i = 0; i < NumArgs; ++i) {
          if (Args[i]) {
            delete Args[i];
            Args[i] = nullptr;
          }
        }
      } else if (NumArgs || Func->Name == NAME_None || !Func->IsGoodStateMethod()) {
        // need to create invocation
        VInvocation *Expr = new VInvocation(nullptr, Func, nullptr, false, false, sc->GetLoc(), NumArgs, Args);
        Expr->CallerState = State;
        VExpressionStatement *Stmt = new VExpressionStatement(new VDropResult(Expr));
#if defined(VC_DECORATE_ACTION_BELONGS_TO_STATE)
        VMethod *M = new VMethod(NAME_None, State, sc->GetLoc());
#else
        VMethod *M = new VMethod(NAME_None, Class, sc->GetLoc());
#endif
        M->Flags = FUNC_Final;
        M->ReturnTypeExpr = new VTypeExprSimple(TYPE_Void, sc->GetLoc());
        M->ReturnType = VFieldType(TYPE_Void);
        M->Statement = Stmt;
        M->NumParams = 0;
#if !defined(VC_DECORATE_ACTION_BELONGS_TO_STATE)
        Class->AddMethod(M);
        M->Define();
#endif
        /*
        if (Func->NumParams == 0 && NumArgs == 0 && (Func->Flags&(FUNC_Final|FUNC_Static)) == FUNC_Final && Func->Name != NAME_None) {
          GCon->Logf(NAME_Debug, "!!! %s: func=`%s` (%s) (params=%d; args=%d; final=%d; static=%d; flags=0x%04x)", Class->GetName(), Func->GetName(), *FuncName, Func->NumParams, NumArgs, (Func->Flags&FUNC_Final ? 1 : 0), (Func->Flags&FUNC_Static ? 1 : 0), Func->Flags);
        }
        */
        Func = M;
      } else {
        //GCon->Logf(NAME_Debug, "*** %s: func=`%s` (%s) (params=%d; args=%d; final=%d; static=%d)", Class->GetName(), Func->GetName(), *FuncName, Func->NumParams, NumArgs, (Func->Flags&FUNC_Final ? 1 : 0), (Func->Flags&FUNC_Static ? 1 : 0));
        State->FunctionName = Func->Name;
        check(State->FunctionName != NAME_None);
        Func = nullptr;
      }
    }
  }

  State->Function = Func;
  if (Func) State->FunctionName = NAME_None;
  if (sc->Check(";")) {}
}
