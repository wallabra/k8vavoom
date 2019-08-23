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
// this is old code for expression parsing, it is not used anymore

//==========================================================================
//
//  ParseExpressionTerm
//
//==========================================================================
static VExpression *ParseExpressionTerm (VScriptParser *sc) {
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
    return new VExprParens(op, l);
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
    if (sc->Check("(")) {
      if (Name.strEquCI("randompick") || Name.strEquCI("frandompick")) {
        return ParseRandomPick(sc, decoClass, Name.strEquCI("frandompick"));
      }
      return ParseMethodCall(sc, Name, l, true); // paren eaten
    }
    if (sc->String.length() > 2 && sc->String[1] == '_' && (sc->String[0] == 'A' || sc->String[0] == 'a')) {
      return ParseMethodCall(sc, Name, l, false); // paren not eaten
    }
    return new VDecorateSingleName(Name, l);
  }

  return nullptr;
}


//==========================================================================
//
//  ParseExpressionPriority1
//
//==========================================================================
static VExpression *ParseExpressionPriority1 (VScriptParser *sc) {
  VExpression *op = ParseExpressionTerm(sc);
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
}


//==========================================================================
//
//  ParseExpressionPriority2
//
//==========================================================================
static VExpression *ParseExpressionPriority2 (VScriptParser *sc) {
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
}


//==========================================================================
//
//  ParseExpressionPriority3
//
//==========================================================================
static VExpression *ParseExpressionPriority3 (VScriptParser *sc) {
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
}


//==========================================================================
//
//  ParseExpressionPriority4
//
//==========================================================================
static VExpression *ParseExpressionPriority4 (VScriptParser *sc) {
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
}


//==========================================================================
//
//  ParseExpressionPriority5
//
//==========================================================================
static VExpression *ParseExpressionPriority5 (VScriptParser *sc) {
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
}


//==========================================================================
//
//  ParseExpressionPriority6
//
//==========================================================================
static VExpression *ParseExpressionPriority6 (VScriptParser *sc) {
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
}


//==========================================================================
//
//  ParseExpressionPriority7
//
//==========================================================================
static VExpression *ParseExpressionPriority7 (VScriptParser *sc) {
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
      GLog.Logf(NAME_Warning, "%s: hey, dumbhead, use `==` for comparisons!", *sc->GetLoc().toStringNoCol());
      VExpression *op2 = ParseExpressionPriority6(sc);
      op1 = new VBinary(VBinary::NotEquals, op1, op2, l);
    } else {
      done = true;
    }
  } while (!done);
  return op1;
}


//==========================================================================
//
//  ParseExpressionPriority8
//
//==========================================================================
static VExpression *ParseExpressionPriority8 (VScriptParser *sc) {
  VExpression *op1 = ParseExpressionPriority7(sc);
  if (!op1) return nullptr;
  TLocation l = sc->GetLoc();
  while (sc->Check("&")) {
    VExpression *op2 = ParseExpressionPriority7(sc);
    op1 = new VBinary(VBinary::And, op1, op2, l);
    l = sc->GetLoc();
  }
  return op1;
}


//==========================================================================
//
//  ParseExpressionPriority9
//
//==========================================================================
static VExpression *ParseExpressionPriority9 (VScriptParser *sc) {
  VExpression *op1 = ParseExpressionPriority8(sc);
  if (!op1) return nullptr;
  TLocation l = sc->GetLoc();
  while (sc->Check("^")) {
    VExpression *op2 = ParseExpressionPriority8(sc);
    op1 = new VBinary(VBinary::XOr, op1, op2, l);
    l = sc->GetLoc();
  }
  return op1;
}


//==========================================================================
//
//  ParseExpressionPriority10
//
//==========================================================================
static VExpression *ParseExpressionPriority10 (VScriptParser *sc) {
  VExpression *op1 = ParseExpressionPriority9(sc);
  if (!op1) return nullptr;
  TLocation l = sc->GetLoc();
  while (sc->Check("|")) {
    VExpression *op2 = ParseExpressionPriority9(sc);
    op1 = new VBinary(VBinary::Or, op1, op2, l);
    l = sc->GetLoc();
  }
  return op1;
}


//==========================================================================
//
//  ParseExpressionPriority11
//
//==========================================================================
static VExpression *ParseExpressionPriority11 (VScriptParser *sc) {
  VExpression *op1 = ParseExpressionPriority10(sc);
  if (!op1) return nullptr;
  TLocation l = sc->GetLoc();
  while (sc->Check("&&")) {
    VExpression *op2 = ParseExpressionPriority10(sc);
    op1 = new VBinaryLogical(VBinaryLogical::And, op1, op2, l);
    l = sc->GetLoc();
  }
  return op1;
}


//==========================================================================
//
//  ParseExpressionPriority12
//
//==========================================================================
static VExpression *ParseExpressionPriority12 (VScriptParser *sc) {
  VExpression *op1 = ParseExpressionPriority11(sc);
  if (!op1) return nullptr;
  TLocation l = sc->GetLoc();
  while (sc->Check("||")) {
    VExpression *op2 = ParseExpressionPriority11(sc);
    op1 = new VBinaryLogical(VBinaryLogical::Or, op1, op2, l);
    l = sc->GetLoc();
  }
  return op1;
}


//==========================================================================
//
//  VParser::ParseExpressionPriority13
//
//==========================================================================
static VExpression *ParseExpressionPriority13 (VScriptParser *sc, VClass *Class) {
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
  return ParseExpressionPriority13(sc, Class);
}
