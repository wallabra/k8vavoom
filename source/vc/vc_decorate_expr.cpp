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

// forward declarations
static VExpression *ParseExpressionNoAssign (VScriptParser *sc, VClass *Class);
static VStatement *ParseActionStatement (VScriptParser *sc, VClass *Class, VState *State);


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

  if (!Func) {
    //GCon->Logf(NAME_Debug, "***8:<%s> %s", *FuncName, *sc->GetLoc().toStringNoCol());
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
  return new VDecorateInvocation((Func ? Func->GetVName() : VName(*Name, VName::AddLower)), Loc, NumArgs, Args);
}


// ////////////////////////////////////////////////////////////////////////// //
enum MathOpType {
  MOP_Unary, // new VUnary((VUnary::EUnaryOp)mop->opcode, lhs, l);
  MOP_Binary, // new VBinary((VBinary::EBinOp)mop->opcode, lhs, rhs, l);
  MOP_Logical, // new VBinaryLogical((VBinaryLogical::ELogOp)mop->opcode, lhs, rhs, l);
};

struct MathOpHandler {
  MathOpType type;
  int prio; // negative means "right-associative" (not implemented yet)
  const char *op; // `nullptr` means "i am the last one, The Omega of it all!"
  int opcode;
};

#define DEFOP(type_,prio_,name_,opcode_) \
  { .type = type_, .prio = prio_, .op = name_, .opcode = (int)opcode_ }

static const MathOpHandler oplist[] = {
  // unaries; rhs has no sense here
  DEFOP(MOP_Unary, 2, "+", VUnary::Plus),
  DEFOP(MOP_Unary, 2, "-", VUnary::Minus),
  DEFOP(MOP_Unary, 2, "!", VUnary::Not),
  DEFOP(MOP_Unary, 2, "~", VUnary::BitInvert),

  // binaries
  DEFOP(MOP_Binary, 3, "*", VBinary::Multiply),
  DEFOP(MOP_Binary, 3, "/", VBinary::Divide),
  DEFOP(MOP_Binary, 3, "%", VBinary::Modulus),

  DEFOP(MOP_Binary, 4, "+", VBinary::Add),
  DEFOP(MOP_Binary, 4, "-", VBinary::Subtract),

  DEFOP(MOP_Binary, 5, "<<", VBinary::LShift),
  DEFOP(MOP_Binary, 5, ">>", VBinary::RShift),

  DEFOP(MOP_Binary, 6, "<", VBinary::Less),
  DEFOP(MOP_Binary, 6, "<=", VBinary::LessEquals),
  DEFOP(MOP_Binary, 6, ">", VBinary::Greater),
  DEFOP(MOP_Binary, 6, ">=", VBinary::GreaterEquals),

  DEFOP(MOP_Binary, 7, "==", VBinary::Equals),
  DEFOP(MOP_Binary, 7, "!=", VBinary::NotEquals),

  DEFOP(MOP_Binary, 8, "&", VBinary::And),
  DEFOP(MOP_Binary, 9, "^", VBinary::XOr),
  DEFOP(MOP_Binary, 10, "|", VBinary::Or),

  DEFOP(MOP_Logical, 11, "&&", VBinaryLogical::And),
  DEFOP(MOP_Logical, 12, "||", VBinaryLogical::Or),

  // 13 is ternary, it has no callback (special case)

  { .type = MOP_Unary, .prio = 0, .op = nullptr, .opcode = 0 },
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
//  ParseConvertToUserVar
//
//==========================================================================
static VExpression *ParseConvertToUserVar (VScriptParser *sc, VClass *Class, VExpression *lhs) {
  if (!lhs || !lhs->IsDecorateSingleName()) return lhs;
  // decorate uservar should resolve to the special thing
  VExpression *e = new VDecorateUserVar(*((VDecorateSingleName *)lhs)->Name, lhs->Loc);
  //!GCon->Logf(NAME_Debug, "%s: CVT: %s -> %s", *lhs->Loc.toStringNoCol(), *lhs->toString(), *e->toString());
  delete lhs;
  return e;
}


//==========================================================================
//
//  ParsePostfixIncDec
//
//==========================================================================
static VExpression *ParsePostfixIncDec (VScriptParser *sc, VClass *Class, VExpression *lhs) {
  int opc = 0;
       if (sc->Check("++")) opc = 1;
  else if (sc->Check("++")) opc = -1;
  else return lhs; // nothing to do

  lhs = ParseConvertToUserVar(sc, Class, lhs);

  // build assignment
  VExpression *op1 = lhs->SyntaxCopy();
  VExpression *op2 = new VIntLiteral(opc, lhs->Loc);
  VExpression *val = new VBinary(VBinary::Add, op1, op2, lhs->Loc);
  VExpression *ass = new VAssignment(VAssignment::Assign, lhs->SyntaxCopy(), val, sc->GetLoc());

  // build resulting operator
  return new VCommaExprRetOp0(lhs, ass, lhs->Loc);
}


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
    sc->Error("expression expected");
    return nullptr;
  }

  // indexing
  if (prio == 1) {
    VExpression *op = ParseExpressionGeneral(sc, Class, prio-1);
    if (!op) return nullptr;
    if (sc->Check("[")) {
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
    // get token, this is slightly faster
    if (!sc->GetString()) { sc->Error("expression expected"); return nullptr; } // no more code, wtf?
    VStr token = sc->String;
    // prefix increment
    if (inCodeBlock) {
      if (token.strEqu("++") || token.strEqu("--")) {
        const TLocation l = sc->GetLoc();
        VExpression *lhs = ParseExpressionGeneral(sc, Class, prio); // rassoc
        if (!lhs) return nullptr;
        lhs = ParseConvertToUserVar(sc, Class, lhs);
        //GCon->Logf(NAME_Debug, "%s: `%s`: %s", *l.toStringNoCol(), *token, *lhs->toString());
        VExpression *op2 = new VIntLiteral((token[0] == '+' ? 1 : -1), lhs->Loc);
        VExpression *val = new VBinary(VBinary::Add, lhs->SyntaxCopy(), op2, lhs->Loc);
        return new VAssignment(VAssignment::Assign, lhs, val, l);
      }
    }
    for (const MathOpHandler *mop = oplist; mop->op; ++mop) {
      if (mop->prio != prio) continue;
      if (token.strEqu(mop->op)) {
        check(mop->type == MOP_Unary);
        const TLocation l = sc->GetLoc();
        VExpression *lhs = ParseExpressionGeneral(sc, Class, prio); // rassoc
        if (!lhs) return nullptr;
        // as this is right-associative, return here
        lhs = new VUnary((VUnary::EUnaryOp)mop->opcode, lhs, l);
        if (inCodeBlock) lhs = ParsePostfixIncDec(sc, Class, lhs);
        return lhs;
      }
    }
    sc->UnGet(); // return token back
    // not found, try higher priority
    VExpression *lhs = ParseExpressionGeneral(sc, Class, prio-1);
    if (lhs && inCodeBlock) lhs = ParsePostfixIncDec(sc, Class, lhs);
    return lhs;
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
    // hacks for some dumbfucks
    if (!inCodeBlock && token.strEqu("||")) {
      if (prio == 10) GCon->Logf(NAME_Error, "%s: Spanish Inquisition says: in decorate, you shall use `|` to combine constants, or you will be executed!", *sc->GetLoc().toStringNoCol());
      token = "|";
    }
    if (!inCodeBlock && token.strEqu("=")) {
      if (prio == 7) GLog.Logf(NAME_Warning, "%s: Spanish Inquisition says: in decorate, you shall use `==` for comparisons, or you will be executed!", *sc->GetLoc().toStringNoCol());
      token = "==";
    }
    // find math operator
    for (; mop->op; ++mop) {
      if (mop->prio != prio) continue;
      if (token.strEqu(mop->op)) break;
    }
    // not found?
    if (!mop->op) {
      // we're done with this priority level, return a token, and get out
      sc->UnGet();
      break;
    }
    // get rhs
    const TLocation l = sc->GetLoc();
    VExpression *rhs = ParseExpressionGeneral(sc, Class, prio-1); // use `prio` for rassoc, and immediately return (see above)
    if (!rhs) { delete lhs; return nullptr; } // some error
    switch (mop->type) {
      case MOP_Binary: lhs = new VBinary((VBinary::EBinOp)mop->opcode, lhs, rhs, l); break;
      case MOP_Logical: lhs = new VBinaryLogical((VBinaryLogical::ELogOp)mop->opcode, lhs, rhs, l); break;
      default: Sys_Error("internal decorate compiler error 697306");
    }
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

  if (!inCodeBlock) return ParseExpressionNoAssign(sc, Class);

  // this can be assign
  VExpression *lhs = ParseExpressionNoAssign(sc, Class);
  if (!lhs) return nullptr;

  // easy case?
  if (sc->Check("=")) {
    const TLocation l = sc->GetLoc();
    //!GCon->Logf(NAME_Debug, "ASSIGN(%s): %s", *sc->String, *lhs->toString());
    lhs = ParseConvertToUserVar(sc, Class, lhs);
    VExpression *rhs = ParseExpression(sc, Class); // rassoc
    if (!rhs) { delete lhs; return nullptr; }
    return new VAssignment(VAssignment::Assign, lhs, rhs, l);
  } else {
    VBinary::EBinOp opc = VBinary::Add;
         if (sc->Check("+=")) opc = VBinary::Add;
    else if (sc->Check("-=")) opc = VBinary::Subtract;
    else if (sc->Check("*=")) opc = VBinary::Multiply;
    else if (sc->Check("/=")) opc = VBinary::Divide;
    else if (sc->Check("%=")) opc = VBinary::Modulus;
    else if (sc->Check("&=")) opc = VBinary::And;
    else if (sc->Check("|=")) opc = VBinary::Or;
    else if (sc->Check("^=")) opc = VBinary::XOr;
    else if (sc->Check("<<=")) opc = VBinary::LShift;
    else if (sc->Check(">>=")) opc = VBinary::RShift;
    else if (sc->Check(">>>=")) opc = VBinary::URShift;
    else return lhs;
    //!GCon->Logf(NAME_Debug, "ASSIGN(%s): %s", *sc->String, *lhs->toString());
    const TLocation l = sc->GetLoc();
    lhs = ParseConvertToUserVar(sc, Class, lhs);
    // get new value
    VExpression *rhs = ParseExpression(sc, Class); // rassoc
    if (!rhs) { delete lhs; return nullptr; }
    // we cannot get address of a uservar, so let's build this horrible AST instead
    VExpression *val = new VBinary(opc, lhs->SyntaxCopy(), rhs, lhs->Loc);
    return new VAssignment(VAssignment::Assign, lhs, val, l);
  }
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

  if (inCodeBlock) {
    if (sc->Check("++") || sc->Check("--") || sc->Check("(") || sc->Check("[")) {
      // this looks like an expression
      sc->UnGet(); // return it
      VExpression *expr = ParseExpression(sc, Class);
      return new VExpressionStatement(new VDropResult(expr));
    }
  }

  sc->ExpectIdentifier();
  VStr FuncName = sc->String;
  auto stloc = sc->GetLoc();

  //GCon->Logf(NAME_Debug, "%s: %s", *stloc.toStringNoCol(), *FuncName);

  if (inCodeBlock) {
    // if it starts with `user_`, it is an expression too
    if (FuncName.startsWithCI("user_")) {
      // this looks like an expression
      sc->UnGet(); // return it
      VExpression *expr = ParseExpression(sc, Class);
      return new VExpressionStatement(new VDropResult(expr));
    }
  }

  VStatement *suvst = CheckParseSetUserVarStmt(sc, Class, FuncName);
  if (suvst) return suvst;

  if (FuncName.strEquCI("A_Jump")) {
    VExpression *jexpr = ParseAJump(sc, Class, State);
    return new VExpressionStatement(jexpr);
  } else {
    VMethod *Func = ParseFunCallWithName(sc, FuncName, Class, NumArgs, Args, false); // no paren
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

  //GCon->Logf(NAME_Debug, "%s: %s", *sc->GetLoc().toStringNoCol(), *sc->String);
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
  sc->ExpectIdentifier();

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
