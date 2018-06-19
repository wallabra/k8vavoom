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


//==========================================================================
//
//  VParser::ParseArgList
//
//  `(` already eaten
//
//==========================================================================
int VParser::ParseArgList (const TLocation &stloc, VExpression **argv) {
  guard(VParser::ParseArgList);
  int count = 0;
  if (!Lex.Check(TK_RParen)) {
    do {
      bool isRef = false, isOut = false;
           if (Lex.Check(TK_Ref)) isRef = true;
      else if (Lex.Check(TK_Out)) isOut = true;
      VExpression *arg;
      if (Lex.Token == TK_Default && Lex.peekNextNonBlankChar() != '.') {
             if (isRef) ParseError(Lex.Location, "`ref` is not allowed for `default` arg");
        else if (isOut) ParseError(Lex.Location, "`out` is not allowed for `default` arg");
        Lex.Expect(TK_Default);
        arg = nullptr;
      } else {
        arg = ParseExpressionPriority13();
        if (arg) {
               if (isRef) arg = new VRefArg(arg);
          else if (isOut) arg = new VOutArg(arg);
        }
      }
      if (count == VMethod::MAX_PARAMS) {
        ParseError(stloc, "Too many arguments");
        delete arg;
      } else {
        argv[count++] = arg;
      }
    } while (Lex.Check(TK_Comma));
    Lex.Expect(TK_RParen, ERR_MISSING_RPAREN);
  }
  return count;
  unguard;
}


//==========================================================================
//
//  VParser::ParseDotMethodCall
//
//==========================================================================
VExpression *VParser::ParseDotMethodCall (VExpression *SelfExpr, VName MethodName, const TLocation &Loc) {
  guard(VParser::ParseDotMethodCall);
  VExpression *Args[VMethod::MAX_PARAMS+1];
  int NumArgs = ParseArgList(Loc, Args);
  return new VDotInvocation(SelfExpr, MethodName, Loc, NumArgs, Args);
  unguard;
}


//==========================================================================
//
//  VParser::ParseBaseMethodCall
//
//==========================================================================
VExpression *VParser::ParseBaseMethodCall (VName Name, const TLocation &Loc) {
  guard(VParser::ParseBaseMethodCall);
  VExpression *Args[VMethod::MAX_PARAMS+1];
  int NumArgs = ParseArgList(Loc, Args);
  return new VSuperInvocation(Name, NumArgs, Args, Loc);
  unguard;
}


//==========================================================================
//
//  VParser::ParseMethodCallOrCast
//
//==========================================================================
VExpression *VParser::ParseMethodCallOrCast (VName Name, const TLocation &Loc) {
  guard(VParser::ParseMethodCallOrCast);
  VExpression *Args[VMethod::MAX_PARAMS+1];
  int NumArgs = ParseArgList(Loc, Args);
  return new VCastOrInvocation(Name, Loc, NumArgs, Args);
  unguard;
}


//==========================================================================
//
//  VParser::ParseLocalVar
//
//==========================================================================
VLocalDecl *VParser::ParseLocalVar (VExpression *TypeExpr, bool requireInit) {
  guard(VParser::ParseLocalVar);
  VLocalDecl *Decl = new VLocalDecl(Lex.Location);
  bool isFirstVar = true;
  bool wasNewArray = false;
  do {
    VLocalEntry e;
    e.TypeExpr = TypeExpr->SyntaxCopy();
    e.TypeExpr = ParseTypePtrs(e.TypeExpr);
    // check for `type[size] arr` syntax
    if (Lex.Check(TK_LBracket)) {
      // arrays cannot be initialized (it seems), so they cannot be automatic
      if (TypeExpr->Type.Type == TYPE_Automatic) {
        ParseError(Lex.Location, "Automatic variable requires initializer");
        continue;
      }
      if (!isFirstVar) {
        ParseError(Lex.Location, "Only one array can be declared with `type[size] name` syntex");
        continue;
      }
      isFirstVar = false;
      // size
      TLocation SLoc = Lex.Location;
      VExpression *SE = ParseExpression();
      Lex.Expect(TK_RBracket, ERR_MISSING_RFIGURESCOPE);
      // name
      if (Lex.Token != TK_Identifier) {
        ParseError(Lex.Location, "Invalid identifier, variable name expected");
        continue;
      }
      e.Loc = Lex.Location;
      e.Name = Lex.Name;
      Lex.NextToken();
      // create it
      e.TypeExpr = new VFixedArrayType(e.TypeExpr, SE, SLoc);
      wasNewArray = true;
      if (requireInit) {
        ParseError(Lex.Location, "Initializer required, but arrays doesn't support initialization");
        continue;
      }
    } else {
      // normal (and old-style array) syntax
      if (wasNewArray) {
        ParseError(Lex.Location, "Only one array can be declared with `type[size] name` syntex");
        break;
      }
      isFirstVar = false;

      if (Lex.Token != TK_Identifier) {
        ParseError(Lex.Location, "Invalid identifier, variable name expected");
        continue;
      }
      e.Loc = Lex.Location;
      e.Name = Lex.Name;
      Lex.NextToken();

      if (requireInit && Lex.Token != TK_Assign) {
        ParseError(Lex.Location, "Initializer required");
        continue;
      }

      if (Lex.Check(TK_LBracket)) {
        // arrays cannot be initialized (it seems), so they cannot be automatic
        if (TypeExpr->Type.Type == TYPE_Automatic) {
          ParseError(Lex.Location, "Automatic variable requires initializer");
          continue;
        }
        TLocation SLoc = Lex.Location;
        VExpression *SE = ParseExpression();
        Lex.Expect(TK_RBracket, ERR_MISSING_RFIGURESCOPE);
        e.TypeExpr = new VFixedArrayType(e.TypeExpr, SE, SLoc);
      }
      // Initialisation
      else if (Lex.Check(TK_Assign)) {
        e.Value = ParseExpressionPriority13();
      }
      else {
        // automatic type cannot be declared without initializer
        if (TypeExpr->Type.Type == TYPE_Automatic) {
          ParseError(Lex.Location, "Automatic variable requires initializer");
          continue;
        }
      }
    }
    Decl->Vars.Append(e);
  } while (Lex.Check(TK_Comma));
  delete TypeExpr;
  TypeExpr = nullptr;
  return Decl;
  unguard;
}


//==========================================================================
//
// VParser::ParseExpressionPriority0
//
// term
//
//==========================================================================
VExpression *VParser::ParseExpressionPriority0 () {
  guard(VParser::ParseExpressionPriority0);
  bool bLocals = CheckForLocal;
  CheckForLocal = false;
  TLocation l = Lex.Location;
  switch (Lex.Token) {
    case TK_IntLiteral:
      {
        vint32 Val = Lex.Number;
        Lex.NextToken();
        return new VIntLiteral(Val, l);
      }
    case TK_FloatLiteral:
      {
        float Val = Lex.Float;
        Lex.NextToken();
        return new VFloatLiteral(Val, l);
      }
    case TK_NameLiteral:
      {
        VName Val = Lex.Name;
        Lex.NextToken();
        return new VNameLiteral(Val, l);
      }
    case TK_StringLiteral:
      {
        int Val = Package->FindString(Lex.String);
        Lex.NextToken();
        return new VStringLiteral(Val, l);
      }
    case TK_Self: Lex.NextToken(); return new VSelf(l);
    case TK_None: Lex.NextToken(); return new VNoneLiteral(l);
    case TK_Null: Lex.NextToken(); return new VNullLiteral(l);
    case TK_False: Lex.NextToken(); return new VIntLiteral(0, l);
    case TK_True: Lex.NextToken(); return new VIntLiteral(1, l);
    case TK_Dollar: Lex.NextToken(); return new VDollar(l);
    case TK_Vector:
      {
        Lex.NextToken();
        Lex.Expect(TK_LParen, ERR_MISSING_LPAREN);
        VExpression *op1 = ParseExpressionPriority13();
        Lex.Expect(TK_Comma);
        VExpression *op2 = ParseExpressionPriority13();
        // third is optional
        VExpression *op3;
        if (Lex.Check(TK_Comma)) {
          op3 = ParseExpressionPriority13();
        } else {
          op3 = new VFloatLiteral(0, l);
        }
        Lex.Expect(TK_RParen, ERR_MISSING_RPAREN);
        return new VVector(op1, op2, op3, l);
      }
    case TK_LParen:
      {
        Lex.NextToken();
        VExpression *op = ParseExpressionPriority13();
        //VExpression *op = ParseExpressionPriority14(false);
        if (!op) ParseError(l, "Expression expected");
        Lex.Expect(TK_RParen, ERR_MISSING_RPAREN);
        return op;
      }
    case TK_DColon:
      {
        Lex.NextToken();
        if (Lex.Token != TK_Identifier) { ParseError(l, "Method name expected"); break; }
        l = Lex.Location;
        VName Name = Lex.Name;
        Lex.NextToken();
        Lex.Expect(TK_LParen, ERR_MISSING_LPAREN);
        return ParseBaseMethodCall(Name, l);
      }
    case TK_Identifier:
      {
        VName Name = Lex.Name;
        Lex.NextToken();
        if (Lex.Check(TK_LParen)) return ParseMethodCallOrCast(Name, l);

        if (Lex.Check(TK_DColon)) {
          if (Lex.Token != TK_Identifier) { ParseError(Lex.Location, "Identifier expected"); break; }
          VName Name2 = Lex.Name;
          Lex.NextToken();
          if (bLocals && Lex.Token == TK_Asterisk) return ParseLocalVar(new VDoubleName(Name, Name2, l));
          return new VDoubleName(Name, Name2, l);
        }

        if (bLocals && Lex.Token == TK_Asterisk) return ParseLocalVar(new VSingleName(Name, l));

        return new VSingleName(Name, l);
      }
    case TK_Default:
      {
        VExpression *Expr = new VDefaultObject(new VSelfClass(l), l);
        Lex.NextToken();
        Lex.Expect(TK_Dot);
        if (Lex.Token != TK_Identifier) ParseError(Lex.Location, "Invalid identifier, field name expected");
        VName FieldName = Lex.Name;
        TLocation Loc = Lex.Location;
        Lex.NextToken();
        if (Lex.Check(TK_LParen)) ParseError(Lex.Location, "Tried to call method on a default object");
        return new VDotField(Expr, FieldName, Loc);
      }
    case TK_Class:
      {
        Lex.NextToken();
        VName ClassName = NAME_None;
        if (Lex.Check(TK_Not)) {
          // class!type
          if (Lex.Token != TK_Identifier) { ParseError(Lex.Location, "Identifier expected"); break; }
          ClassName = Lex.Name;
          Lex.NextToken();
        } else {
          // class<type>
          Lex.Expect(TK_Less);
          if (Lex.Token != TK_Identifier) { ParseError(Lex.Location, "Identifier expected"); break; }
          ClassName = Lex.Name;
          Lex.NextToken();
          Lex.Expect(TK_Greater);
        }
        Lex.Expect(TK_LParen);
        VExpression *Expr = ParseExpressionPriority13();
        if (!Expr) ParseError(Lex.Location, "Expression expected");
        Lex.Expect(TK_RParen);
        return new VDynamicClassCast(ClassName, Expr, l);
      }
    // int(val) --> convert bool/int/float to int
    case TK_Int:
      {
        Lex.NextToken();
        Lex.Expect(TK_LParen);
        VExpression *op = ParseExpressionPriority13(); //k8:???
        if (!op) ParseError(l, "Expression expected");
        Lex.Expect(TK_RParen, ERR_MISSING_RPAREN);
        return new VScalarToInt(op);
      }
    // float(val) --> convert bool/int/float to float
    case TK_Float:
      {
        Lex.NextToken();
        Lex.Expect(TK_LParen);
        VExpression *op = ParseExpressionPriority13(); //k8:???
        if (!op) ParseError(l, "Expression expected");
        Lex.Expect(TK_RParen, ERR_MISSING_RPAREN);
        return new VScalarToFloat(op);
      }
    // string(val) --> convert name to string
    case TK_String:
      {
        Lex.NextToken();
        Lex.Expect(TK_LParen);
        VExpression *op = ParseExpressionPriority13(); //k8:???
        if (!op) ParseError(l, "Expression expected");
        Lex.Expect(TK_RParen, ERR_MISSING_RPAREN);
        return new VCastToString(op);
      }
    // name(val) --> convert string to name
    case TK_Name:
      {
        Lex.NextToken();
        Lex.Expect(TK_LParen);
        VExpression *op = ParseExpressionPriority13(); //k8:???
        if (!op) ParseError(l, "Expression expected");
        Lex.Expect(TK_RParen, ERR_MISSING_RPAREN);
        return new VCastToName(op);
      }
    default:
      break;
  }

  return nullptr;
  unguard;
}


//==========================================================================
//
// VParser::ParseExpressionPriority1
//
// `->`, `.`, `[]`
//
//==========================================================================
VExpression *VParser::ParseExpressionPriority1 () {
  guard(VParser::ParseExpressionPriority1);
  VExpression *op = ParseExpressionPriority0();
  if (!op) return nullptr;
  for (;;) {
    TLocation l = Lex.Location;
    if (Lex.Check(TK_Arrow)) {
      if (Lex.Token != TK_Identifier) {
        ParseError(Lex.Location, "Invalid identifier, field name expected");
      } else {
        op = new VPointerField(op, Lex.Name, Lex.Location);
        Lex.NextToken();
      }
    } else if (Lex.Check(TK_Dot)) {
      if (Lex.Check(TK_Default)) {
        Lex.Expect(TK_Dot);
        op = new VDefaultObject(op, l);
      }
      if (Lex.Token != TK_Identifier) {
        ParseError(Lex.Location, "Invalid identifier, field name expected");
      } else {
        VName FieldName = Lex.Name;
        TLocation Loc = Lex.Location;
        Lex.NextToken();
        if (Lex.Check(TK_LParen)) {
          if (op->IsDefaultObject()) ParseError(Lex.Location, "Tried to call method on a default object");
          op = ParseDotMethodCall(op, FieldName, Loc);
        } else {
          op = new VDotField(op, FieldName, Loc);
        }
      }
    } else if (Lex.Check(TK_LBracket)) {
      VExpression *ind = ParseExpressionPriority13();
      // slice?
      if (Lex.Check(TK_DotDot)) {
        VExpression *hi = ParseExpressionPriority13();
        Lex.Expect(TK_RBracket, ERR_BAD_ARRAY);
        op = new VStringSlice(op, ind, hi, l);
      } else {
        Lex.Expect(TK_RBracket, ERR_BAD_ARRAY);
        op = new VArrayElement(op, ind, l);
      }
    } else {
      break;
    }
  }
  return op;
  unguard;
}


//==========================================================================
//
// VParser::ParseExpressionPriority2
//
// unary: `+`, `-`, `!`, `~`, `&`, `*`
// prefix: `++`, `--`
// postfix: `++`, `--`
//
//==========================================================================
VExpression *VParser::ParseExpressionPriority2 () {
  guard(VParser::ParseExpressionPriority2);
  VExpression *op;
  TLocation l = Lex.Location;

  if (Lex.Check(TK_Plus)) { op = ParseExpressionPriority2(); return new VUnary(VUnary::Plus, op, l); }
  if (Lex.Check(TK_Minus)) { op = ParseExpressionPriority2(); return new VUnary(VUnary::Minus, op, l); }
  if (Lex.Check(TK_Not)) { op = ParseExpressionPriority2(); return new VUnary(VUnary::Not, op, l); }
  if (Lex.Check(TK_Tilde)) { op = ParseExpressionPriority2(); return new VUnary(VUnary::BitInvert, op, l); }
  if (Lex.Check(TK_And)) { op = ParseExpressionPriority1(); return new VUnary(VUnary::TakeAddress, op, l); }
  if (Lex.Check(TK_Asterisk)) { op = ParseExpressionPriority2(); return new VPushPointed(op); }
  if (Lex.Check(TK_Inc)) { op = ParseExpressionPriority2(); return new VUnaryMutator(VUnaryMutator::PreInc, op, l); }
  if (Lex.Check(TK_Dec)) { op = ParseExpressionPriority2(); return new VUnaryMutator(VUnaryMutator::PreDec, op, l); }

  op = ParseExpressionPriority1();
  if (!op) return nullptr;

  l = Lex.Location;

  if (Lex.Check(TK_Inc)) return new VUnaryMutator(VUnaryMutator::PostInc, op, l);
  if (Lex.Check(TK_Dec)) return new VUnaryMutator(VUnaryMutator::PostDec, op, l);

  return op;
  unguard;
}


//==========================================================================
//
// VParser::ParseExpressionPriority3
//
// binary: `*`, `/`, `%`
//
//==========================================================================
VExpression *VParser::ParseExpressionPriority3 () {
  guard(VParser::ParseExpressionPriority3);
  VExpression *op1 = ParseExpressionPriority2();
  if (!op1) return nullptr;
  for (;;) {
    TLocation l = Lex.Location;
    if (Lex.Check(TK_Asterisk)) {
      VExpression *op2 = ParseExpressionPriority2();
      op1 = new VBinary(VBinary::Multiply, op1, op2, l);
    } else if (Lex.Check(TK_Slash)) {
      VExpression *op2 = ParseExpressionPriority2();
      op1 = new VBinary(VBinary::Divide, op1, op2, l);
    } else if (Lex.Check(TK_Percent)) {
      VExpression *op2 = ParseExpressionPriority2();
      op1 = new VBinary(VBinary::Modulus, op1, op2, l);
    } else {
      break;
    }
  }
  return op1;
  unguard;
}


//==========================================================================
//
// VParser::ParseExpressionPriority4
//
// binary: `+`, `-`, `~`
//
//==========================================================================
VExpression *VParser::ParseExpressionPriority4 () {
  guard(VParser::ParseExpressionPriority4);
  VExpression *op1 = ParseExpressionPriority3();
  if (!op1) return nullptr;
  for (;;) {
    TLocation l = Lex.Location;
    if (Lex.Check(TK_Plus)) {
      VExpression *op2 = ParseExpressionPriority3();
      op1 = new VBinary(VBinary::Add, op1, op2, l);
    } else if (Lex.Check(TK_Minus)) {
      VExpression *op2 = ParseExpressionPriority3();
      op1 = new VBinary(VBinary::Subtract, op1, op2, l);
    } else if (Lex.Check(TK_Tilde)) {
      VExpression *op2 = ParseExpressionPriority3();
      op1 = new VBinary(VBinary::StrCat, op1, op2, l);
    } else {
      break;
    }
  }
  return op1;
  unguard;
}


//==========================================================================
//
// VParser::ParseExpressionPriority5
//
// binary: `<<`, `>>`
//
//==========================================================================
VExpression *VParser::ParseExpressionPriority5 () {
  guard(VParser::ParseExpressionPriority5);
  VExpression *op1 = ParseExpressionPriority4();
  if (!op1) return nullptr;
  for (;;) {
    TLocation l = Lex.Location;
    if (Lex.Check(TK_LShift)) {
      VExpression *op2 = ParseExpressionPriority4();
      op1 = new VBinary(VBinary::LShift, op1, op2, l);
    } else if (Lex.Check(TK_RShift)) {
      VExpression *op2 = ParseExpressionPriority4();
      op1 = new VBinary(VBinary::RShift, op1, op2, l);
    } else {
      break;
    }
  }
  return op1;
  unguard;
}


//==========================================================================
//
// VParser::ParseExpressionPriority6
//
// binary: `<`, `<=`, `>`, `>=`
//
//==========================================================================
VExpression *VParser::ParseExpressionPriority6 () {
  guard(VParser::ParseExpressionPriority6);
  VExpression *op1 = ParseExpressionPriority5();
  if (!op1) return nullptr;
  for (;;) {
    TLocation l = Lex.Location;
    if (Lex.Check(TK_Less)) {
      VExpression *op2 = ParseExpressionPriority5();
      op1 = new VBinary(VBinary::Less, op1, op2, l);
    } else if (Lex.Check(TK_LessEquals)) {
      VExpression *op2 = ParseExpressionPriority5();
      op1 = new VBinary(VBinary::LessEquals, op1, op2, l);
    } else if (Lex.Check(TK_Greater)) {
      VExpression *op2 = ParseExpressionPriority5();
      op1 = new VBinary(VBinary::Greater, op1, op2, l);
    } else if (Lex.Check(TK_GreaterEquals)) {
      VExpression *op2 = ParseExpressionPriority5();
      op1 = new VBinary(VBinary::GreaterEquals, op1, op2, l);
    } else {
      break;
    }
  }
  return op1;
  unguard;
}


//==========================================================================
//
// VParser::ParseExpressionPriority7
//
// binary: `==`, `!=`
//
//==========================================================================
VExpression *VParser::ParseExpressionPriority7 () {
  guard(VParser::ParseExpressionPriority7);
  VExpression *op1 = ParseExpressionPriority6();
  if (!op1) return nullptr;
  for (;;) {
    TLocation l = Lex.Location;
    if (Lex.Check(TK_Equals)) {
      VExpression *op2 = ParseExpressionPriority6();
      op1 = new VBinary(VBinary::Equals, op1, op2, l);
    } else if (Lex.Check(TK_NotEquals)) {
      VExpression *op2 = ParseExpressionPriority6();
      op1 = new VBinary(VBinary::NotEquals, op1, op2, l);
    } else {
      break;
    }
  }
  return op1;
  unguard;
}


//==========================================================================
//
// VParser::ParseExpressionPriority8
//
// binary: `&`
//
//==========================================================================
VExpression *VParser::ParseExpressionPriority8 () {
  guard(VParser::ParseExpressionPriority8);
  VExpression *op1 = ParseExpressionPriority7();
  if (!op1) return nullptr;
  TLocation l = Lex.Location;
  while (Lex.Check(TK_And)) {
    VExpression *op2 = ParseExpressionPriority7();
    op1 = new VBinary(VBinary::And, op1, op2, l);
    l = Lex.Location;
  }
  return op1;
  unguard;
}


//==========================================================================
//
// VParser::ParseExpressionPriority9
//
// binary: `^`
//
//==========================================================================
VExpression *VParser::ParseExpressionPriority9 () {
  guard(VParser::ParseExpressionPriority9);
  VExpression *op1 = ParseExpressionPriority8();
  if (!op1) return nullptr;
  TLocation l = Lex.Location;
  while (Lex.Check(TK_XOr)) {
    VExpression *op2 = ParseExpressionPriority8();
    op1 = new VBinary(VBinary::XOr, op1, op2, l);
    l = Lex.Location;
  }
  return op1;
  unguard;
}


//==========================================================================
//
// VParser::ParseExpressionPriority10
//
// binary: `|`
//
//==========================================================================
VExpression *VParser::ParseExpressionPriority10 () {
  guard(VParser::ParseExpressionPriority10);
  VExpression *op1 = ParseExpressionPriority9();
  if (!op1) return nullptr;
  TLocation l = Lex.Location;
  while (Lex.Check(TK_Or)) {
    VExpression *op2 = ParseExpressionPriority9();
    op1 = new VBinary(VBinary::Or, op1, op2, l);
    l = Lex.Location;
  }
  return op1;
  unguard;
}


//==========================================================================
//
// VParser::ParseExpressionPriority11
//
// binary: `&&`
//
//==========================================================================
VExpression *VParser::ParseExpressionPriority11 () {
  guard(VParser::ParseExpressionPriority11);
  VExpression *op1 = ParseExpressionPriority10();
  if (!op1) return nullptr;
  TLocation l = Lex.Location;
  while (Lex.Check(TK_AndLog)) {
    VExpression *op2 = ParseExpressionPriority10();
    op1 = new VBinaryLogical(VBinaryLogical::And, op1, op2, l);
    l = Lex.Location;
  }
  return op1;
  unguard;
}


//==========================================================================
//
// VParser::ParseExpressionPriority12
//
// binary: `||`
//
//==========================================================================
VExpression *VParser::ParseExpressionPriority12 () {
  guard(VParser::ParseExpressionPriority12);
  VExpression *op1 = ParseExpressionPriority11();
  if (!op1) return nullptr;
  TLocation l = Lex.Location;
  while (Lex.Check(TK_OrLog)) {
    VExpression *op2 = ParseExpressionPriority11();
    op1 = new VBinaryLogical(VBinaryLogical::Or, op1, op2, l);
    l = Lex.Location;
  }
  return op1;
  unguard;
}


//==========================================================================
//
// VParser::ParseExpressionPriority13
//
// ternary: `?:`
//
//==========================================================================
VExpression *VParser::ParseExpressionPriority13 () {
  guard(VParser::ParseExpressionPriority13);
  VExpression *op = ParseExpressionPriority12();
  if (!op) return nullptr;
  TLocation l = Lex.Location;
  if (Lex.Check(TK_Quest)) {
    // check for `?:`, and duplicate op
    if (Lex.Check(TK_Colon)) {
      VExpression *op2 = ParseExpressionPriority13();
      op = new VConditional(op, op->SyntaxCopy(), op2, l);
    } else {
      VExpression *op1 = ParseExpressionPriority13();
      Lex.Expect(TK_Colon, ERR_MISSING_COLON);
      VExpression *op2 = ParseExpressionPriority13();
      op = new VConditional(op, op1, op2, l);
    }
  }
  return op;
  unguard;
}


//==========================================================================
//
// VParser::ParseExpressionPriority14
//
// assignmnets: `=`, and various `op=`
//
//==========================================================================
VExpression *VParser::ParseExpressionPriority14 (bool allowAssign) {
  guard(VParser::ParseExpressionPriority14);
  VExpression *op1 = ParseExpressionPriority13();
  if (!op1) return nullptr;
  TLocation l = Lex.Location;
  VAssignment::EAssignOper oper = VAssignment::Assign;
       if (Lex.Check(TK_Assign)) oper = VAssignment::Assign;
  else if (Lex.Check(TK_AddAssign)) oper = VAssignment::AddAssign;
  else if (Lex.Check(TK_MinusAssign)) oper = VAssignment::MinusAssign;
  else if (Lex.Check(TK_MultiplyAssign)) oper = VAssignment::MultiplyAssign;
  else if (Lex.Check(TK_DivideAssign)) oper = VAssignment::DivideAssign;
  else if (Lex.Check(TK_ModAssign)) oper = VAssignment::ModAssign;
  else if (Lex.Check(TK_AndAssign)) oper = VAssignment::AndAssign;
  else if (Lex.Check(TK_OrAssign)) oper = VAssignment::OrAssign;
  else if (Lex.Check(TK_XOrAssign)) oper = VAssignment::XOrAssign;
  else if (Lex.Check(TK_LShiftAssign)) oper = VAssignment::LShiftAssign;
  else if (Lex.Check(TK_RShiftAssign)) oper = VAssignment::RShiftAssign;
  else return op1;
  // parse `n = delegate ...`
  if (oper == VAssignment::Assign && Lex.Check(TK_Delegate)) {
    VExpression *op2 = ParseLambda();
    op1 = new VAssignment(oper, op1, op2, l);
  } else {
    VExpression *op2 = ParseExpressionPriority13();
    op1 = new VAssignment(oper, op1, op2, l);
  }
  if (!allowAssign) ParseError(l, "assignment is not allowed here");
  return op1;
  unguard;
}


//==========================================================================
//
// VParser::ParseExpression
//
// general expression parser
//
//==========================================================================
VExpression *VParser::ParseExpression (bool allowAssign) {
  guard(VParser::ParseExpression);
  CheckForLocal = false;
  if (!allowAssign && Lex.Token == TK_LParen) allowAssign = true;
  return ParseExpressionPriority14(allowAssign);
  unguard;
}


//==========================================================================
//
//  VParser::ParseStatement
//
//==========================================================================
VStatement *VParser::ParseStatement () {
  guard(VParser::ParseStatement);
  TLocation l = Lex.Location;
  switch(Lex.Token) {
    case TK_EOF:
      ParseError(Lex.Location, ERR_UNEXPECTED_EOF);
      return nullptr;
    case TK_If:
      {
        Lex.NextToken();
        Lex.Expect(TK_LParen, ERR_MISSING_LPAREN);
        VExpression *e = ParseExpression();
        if (!e) ParseError(Lex.Location, "If expression expected");
        Lex.Expect(TK_RParen, ERR_MISSING_RPAREN);
        VStatement *STrue = ParseStatement();
        if (Lex.Check(TK_Else)) {
          VStatement *SFalse = ParseStatement();
          return new VIf(e, STrue, SFalse, l);
        } else {
          return new VIf(e, STrue, l);
        }
      }
    case TK_While:
      {
        Lex.NextToken();
        Lex.Expect(TK_LParen, ERR_MISSING_LPAREN);
        VExpression *Expr = ParseExpression();
        if (!Expr) ParseError(Lex.Location, "Wile loop expression expected");
        Lex.Expect(TK_RParen, ERR_MISSING_RPAREN);
        VStatement *Statement = ParseStatement();
        return new VWhile(Expr, Statement, l);
      }
    case TK_Do:
      {
        Lex.NextToken();
        VStatement *Statement = ParseStatement();
        Lex.Expect(TK_While, ERR_BAD_DO_STATEMENT);
        Lex.Expect(TK_LParen, ERR_MISSING_LPAREN);
        VExpression *Expr = ParseExpression();
        if (!Expr) ParseError(Lex.Location, "Do loop expression expected");
        Lex.Expect(TK_RParen, ERR_MISSING_RPAREN);
        Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
        return new VDo(Expr, Statement, l);
      }
    case TK_For:
      {
        // to hide inline `for` variable declarations, we need to wrap `for` into compound statement
        bool needCompound = false;

        Lex.NextToken();
        VFor *For = new VFor(l);
        Lex.Expect(TK_LParen, ERR_MISSING_LPAREN);

        // allow local declaration here
        switch (Lex.Token) {
          case TK_Bool:
          case TK_Byte:
          //case TK_Class: //k8:???
          case TK_Float:
          case TK_Int:
          case TK_Name:
          case TK_State:
          case TK_String:
          case TK_Auto:
           do_for_init_decls:
            {
              needCompound = true; // wrap it
              // indirections are processed in `ParseLocalVar()`, 'cause they belongs to vars
              VExpression *TypeExpr = ParseType(true);
              do {
                VLocalDecl *Decl = ParseLocalVar(TypeExpr, true);
                if (!Decl) break;
                For->InitExpr.Append(new VDropResult(Decl));
              } while (Lex.Check(TK_Comma));
            }
            break;
          case TK_Identifier: // this can be something like `Type var = ...`, so check for it
            {
              int ofs = 1;
              //fprintf(stderr, "TT0: %s; TT1: %s\n", VLexer::TokenNames[Lex.peekTokenType(ofs)], VLexer::TokenNames[Lex.peekTokenType(ofs+1)]);
              while (Lex.peekTokenType(ofs) == TK_Asterisk) ++ofs;
              if (Lex.peekTokenType(ofs) == TK_Identifier && Lex.peekTokenType(ofs+1) == TK_Assign) {
                // yep, declarations
                goto do_for_init_decls;
              }
            }
            /* fallthrough */
          default:
            do {
              VExpression *Expr = ParseExpression(true);
              if (!Expr) break;
              For->InitExpr.Append(new VDropResult(Expr));
            } while (Lex.Check(TK_Comma));
            break;
        }

        Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
        For->CondExpr = ParseExpression();

        Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
        do {
          VExpression *Expr = ParseExpression(true);
          if (!Expr) break;
          For->LoopExpr.Append(new VDropResult(Expr));
        } while (Lex.Check(TK_Comma));
        Lex.Expect(TK_RParen, ERR_MISSING_RPAREN);

        VStatement *Statement = ParseStatement();
        For->Statement = Statement;
        // wrap statement if necessary
        if (needCompound) {
          VCompound *Comp = new VCompound(For->Loc);
          Comp->Statements.Append(For);
          return Comp;
        } else {
          return For;
        }
      }
    case TK_Foreach:
      {
        Lex.NextToken();
        VExpression *Expr = ParseExpression();
        if (!Expr) ParseError(Lex.Location, "Iterator expression expected");
        VStatement *Statement = ParseStatement();
        return new VForeach(Expr, Statement, l);
      }
    case TK_Break:
      Lex.NextToken();
      Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
      return new VBreak(l);
    case TK_Continue:
      Lex.NextToken();
      Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
      return new VContinue(l);
    case TK_Return:
      {
        Lex.NextToken();
        VExpression *Expr = ParseExpression();
        Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
        return new VReturn(Expr, l);
      }
    case TK_Switch:
      {
        Lex.NextToken();
        VSwitch *Switch = new VSwitch(l);
        Lex.Expect(TK_LParen, ERR_MISSING_LPAREN);
        Switch->Expr = ParseExpression();
        if (!Switch->Expr) ParseError(Lex.Location, "Switch expression expected");
        Lex.Expect(TK_RParen, ERR_MISSING_RPAREN);

        Lex.Expect(TK_LBrace, ERR_MISSING_LBRACE);
        do {
          l = Lex.Location;
          if (Lex.Check(TK_Case)) {
            VExpression *Expr = ParseExpression();
            if (!Expr) ParseError(Lex.Location, "Case value expected");
            Lex.Expect(TK_Colon, ERR_MISSING_COLON);
            Switch->Statements.Append(new VSwitchCase(Switch, Expr, l));
          } else if (Lex.Check(TK_Default)) {
            Lex.Expect(TK_Colon, ERR_MISSING_COLON);
            Switch->Statements.Append(new VSwitchDefault(Switch, l));
          } else {
            VStatement *Statement = ParseStatement();
            Switch->Statements.Append(Statement);
          }
        } while (!Lex.Check(TK_RBrace));
        return Switch;
      }
    case TK_LBrace:
      Lex.NextToken();
      return ParseCompoundStatement();
    case TK_Bool:
    case TK_Byte:
    case TK_Class:
    case TK_Float:
    case TK_Int:
    case TK_Name:
    case TK_State:
    case TK_String:
    case TK_Void:
    case TK_Array:
    case TK_Auto:
      {
        // indirections are processed in `ParseLocalVar()`, 'cause they belongs to vars
        VExpression *TypeExpr = ParseType(true);
        VLocalDecl *Decl = ParseLocalVar(TypeExpr);
        Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
        return new VLocalVarStatement(Decl);
      }
    default:
      CheckForLocal = true;
      VExpression *Expr = ParseExpressionPriority14(true);
      if (!Expr) {
        if (!Lex.Check(TK_Semicolon)) {
          ParseError(l, "Token %s makes no sense here", VLexer::TokenNames[Lex.Token]);
          Lex.NextToken();
        }
        return new VEmptyStatement(l);
      } else if (Expr->IsValidTypeExpression() && Lex.Token == TK_Identifier) {
        VLocalDecl *Decl = ParseLocalVar(Expr);
        Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
        return new VLocalVarStatement(Decl);
      } else {
        Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
        return new VExpressionStatement(new VDropResult(Expr));
      }
  }
  unguard;
}


//==========================================================================
//
// VParser::ParseCompoundStatement
//
//==========================================================================
VCompound *VParser::ParseCompoundStatement () {
  guard(VParser::ParseCompoundStatement);
  VCompound *Comp = new VCompound(Lex.Location);
  while (!Lex.Check(TK_RBrace)) Comp->Statements.Append(ParseStatement());
  return Comp;
  unguard;
}


//==========================================================================
//
// VParser::ParsePrimitiveType
//
// this won't parse `type*` and delegates
//
//==========================================================================
VExpression *VParser::ParsePrimitiveType () {
  guard(VParser::ParsePrimitiveType);
  TLocation l = Lex.Location;
  switch (Lex.Token) {
    case TK_Void: Lex.NextToken(); return new VTypeExprSimple(TYPE_Void, l);
    case TK_Auto: Lex.NextToken(); return new VTypeExprSimple(TYPE_Automatic, l);
    case TK_Int: Lex.NextToken(); return new VTypeExprSimple(TYPE_Int, l);
    case TK_Byte: Lex.NextToken(); return new VTypeExprSimple(TYPE_Byte, l);
    case TK_Float: Lex.NextToken(); return new VTypeExprSimple(TYPE_Float, l);
    case TK_Name: Lex.NextToken(); return new VTypeExprSimple(TYPE_Name, l);
    case TK_String: Lex.NextToken(); return new VTypeExprSimple(TYPE_String, l);
    case TK_State: Lex.NextToken(); return new VTypeExprSimple(TYPE_State, l);
    case TK_Bool:
      {
        Lex.NextToken();
        VFieldType ret(TYPE_Bool);
        ret.BitMask = 1;
        return new VTypeExprSimple(ret, l);
      }
    case TK_Class:
      {
        Lex.NextToken();
        VName MetaClassName = NAME_None;
        if (Lex.Check(TK_Not)) {
          // `class!type` or `class!(type)`
          int parenCount = 0;
          while (Lex.Check(TK_LParen)) ++parenCount;
          if (Lex.Token != TK_Identifier) {
            ParseError(Lex.Location, "Invalid identifier, class name expected");
          } else {
            MetaClassName = Lex.Name;
            Lex.NextToken();
          }
          while (parenCount-- > 0) {
            if (!Lex.Check(TK_RParen)) {
              ParseError(Lex.Location, "')' expected");
              break;
            }
          }
        } else if (Lex.Check(TK_Less)) {
          // class<type>
          if (Lex.Token != TK_Identifier) {
            ParseError(Lex.Location, "Invalid identifier, class name expected");
          } else {
            MetaClassName = Lex.Name;
            Lex.NextToken();
          }
          Lex.Expect(TK_Greater);
        }
        return new VTypeExprClass(MetaClassName, l);
      }
    case TK_Identifier:
      {
        VName Name = Lex.Name;
        Lex.NextToken();
        if (Lex.Check(TK_DColon)) {
          if (Lex.Token != TK_Identifier) {
            ParseError(Lex.Location, "Identifier expected");
            return new VSingleName(Name, l);
          }
          VName Name2 = Lex.Name;
          Lex.NextToken();
          return new VDoubleName(Name, Name2, l);
        }
        return new VSingleName(Name, l);
      }
    case TK_Array:
      {
        Lex.NextToken();
        VExpression *Inner = nullptr;
        if (Lex.Check(TK_Not)) {
          // `array!type` or `array!(type)`
          int parenCount = 0;
          while (Lex.Check(TK_LParen)) ++parenCount;
          Inner = ParseType(); // no delegates yet
          if (!Inner) ParseError(Lex.Location, "Inner type declaration expected");
          while (parenCount-- > 0) {
            Inner = ParseTypePtrs(Inner);
            if (!Lex.Check(TK_RParen)) {
              ParseError(Lex.Location, "')' expected");
              break;
            }
          }
        } else {
          // array<type>
          Lex.Expect(TK_Less);
          Inner = ParseTypeWithPtrs(true);
          if (!Inner) ParseError(Lex.Location, "Inner type declaration expected");
          Lex.Expect(TK_Greater);
        }
        return new VDynamicArrayType(Inner, l);
      }
    default:
      return nullptr;
  }
  unguard;
}


//==========================================================================
//
// VParser::ParseType
//
//==========================================================================
VExpression *VParser::ParseType (bool allowDelegates) {
  guard(VParser::ParseType);
  auto stloc = Lex.Location;
  auto Type = ParsePrimitiveType();
  if (!Type) return nullptr; // either error, or not a type

  // skip `*`
  int ofs = 0;
  while (Lex.peekTokenType(ofs) == TK_Asterisk) ++ofs;

  // check for slice: `type[]`
  if (Lex.peekTokenType(ofs) == TK_LBracket && Lex.peekTokenType(ofs+1) == TK_RBracket) {
    Type = ParseTypePtrs(Type);
    Lex.Expect(TK_LBracket);
    Lex.Expect(TK_RBracket);
    Type = new VSliceType(Type, true, stloc);
  }

  if (allowDelegates) {
    ofs = 0;
    while (Lex.peekTokenType(ofs) == TK_Asterisk) ++ofs;
    if (Lex.peekTokenType(ofs) == TK_Delegate) {
      // this must be `type delegate (args)`
      Type = ParseTypePtrs(Type);
      if (!Lex.Check(TK_Delegate)) {
        ParseError(Lex.Location, "Invalid delegate syntax (parser is confused)");
        delete Type;
        return nullptr;
      }
      if (!Lex.Check(TK_LParen)) {
        ParseError(Lex.Location, "Missing delegate arguments");
        delete Type;
        return nullptr;
      }
      // parse delegate args
      VDelegateType *dg = new VDelegateType(Type, stloc);
      do {
        VMethodParam &P = dg->Params[dg->NumParams];
        int ParmModifiers = TModifiers::Parse(Lex);
        dg->ParamFlags[dg->NumParams] = TModifiers::ParmAttr(TModifiers::Check(ParmModifiers, TModifiers::Optional|TModifiers::Out|TModifiers::Ref, Lex.Location));
        P.TypeExpr = ParseTypeWithPtrs(true);
        if (!P.TypeExpr && dg->NumParams == 0) break;
        if (Lex.Token == TK_Identifier) {
          P.Name = Lex.Name;
          P.Loc = Lex.Location;
          Lex.NextToken();
        }
        if (dg->NumParams == VMethod::MAX_PARAMS) {
          ParseError(Lex.Location, "Delegate parameters overflow");
        } else {
          ++dg->NumParams;
        }
      } while (Lex.Check(TK_Comma));
      Type = dg;
      Lex.Expect(TK_RParen, ERR_MISSING_RPAREN);
    }
  }

  return Type;
  unguard;
}


//==========================================================================
//
// VParser::ParseTypePtrs
//
// call this after `ParseType` to parse asterisks
//
//==========================================================================
VExpression *VParser::ParseTypePtrs (VExpression *type) {
  guard(VParser::ParseTypePtrs);
  if (!type) return nullptr;
  TLocation l = Lex.Location;
  while (Lex.Check(TK_Asterisk)) {
    type = new VPointerType(type, l);
    l = Lex.Location;
  }
  return type;
  unguard;
}


//==========================================================================
//
// VParser::ParseTypeWithPtrs
//
// convenient wrapper
//
//==========================================================================
VExpression *VParser::ParseTypeWithPtrs (bool allowDelegates) {
  guard(VParser::ParseTypePtrs);
  return ParseTypePtrs(ParseType(allowDelegates));
  unguard;
}


//==========================================================================
//
// VParser::ParseMethodDef
//
//==========================================================================
void VParser::ParseMethodDef (VExpression *RetType, VName MName, const TLocation &MethodLoc,
                              VClass *InClass, vint32 Modifiers, bool Iterator)
{
  guard(VParser::ParseMethodDef);
  if (InClass->FindMethod(MName, false)) ParseError(MethodLoc, "Redeclared method %s.%s", *InClass->Name, *MName);

  VMethod *Func = new VMethod(MName, InClass, MethodLoc);
  Func->Flags = TModifiers::MethodAttr(TModifiers::Check(Modifiers,
    TModifiers::Native|TModifiers::Static|TModifiers::Final|
    TModifiers::Spawner|TModifiers::Override|
    TModifiers::Private|TModifiers::Protected, MethodLoc));
  Func->ReturnTypeExpr = RetType;
  if (Iterator) Func->Flags |= FUNC_Iterator;
  InClass->AddMethod(Func);

  do {
    if (Lex.Check(TK_VarArgs)) {
      Func->Flags |= FUNC_VarArgs;
      break;
    }

    VMethodParam &P = Func->Params[Func->NumParams];

    int ParmModifiers = TModifiers::Parse(Lex);
    Func->ParamFlags[Func->NumParams] = TModifiers::ParmAttr(TModifiers::Check(ParmModifiers, TModifiers::Optional|TModifiers::Out|TModifiers::Ref, Lex.Location));

    P.TypeExpr = ParseTypeWithPtrs(true);
    if (!P.TypeExpr && Func->NumParams == 0) break;
    if (Lex.Token == TK_Identifier) {
      P.Name = Lex.Name;
      P.Loc = Lex.Location;
      Lex.NextToken();
    }
    if (Func->NumParams == VMethod::MAX_PARAMS) {
      ParseError(Lex.Location, "Method parameters overflow");
      continue;
    }
    ++Func->NumParams;
  } while (Lex.Check(TK_Comma));
  Lex.Expect(TK_RParen, ERR_MISSING_RPAREN);

  // parse attributes
  while (Lex.Check(TK_LBracket)) {
    // for now, there is only one attribute: `[printf,idx]`, so check its prerequisites
    if (Lex.Token != TK_Identifier) {
      ParseError(Lex.Location, "Attribute name expected");
    } else {
      if (Lex.Name == "printf") {
        if ((Func->Flags&FUNC_Native) == 0) ParseError(Func->Loc, "Non-native methods can't have attributes");
        if ((Func->Flags&FUNC_VarArgs) == 0) ParseError(Func->Loc, "Non-vararg methods can't have attributes");
        Lex.NextToken();
        Lex.Expect(TK_Comma, ERR_MISSING_COMMA);
        if (Lex.Token != TK_IntLiteral) {
          ParseError(Func->Loc, "Argument number expected for 'printf' attribute");
        } else {
          if (Lex.Number < 1 || Lex.Number > Func->NumParams) {
            ParseError(Func->Loc, "Invalid argument number for 'printf' attribute (did you forget that counter is 1-based?)");
          } else {
            Func->printfFmtArgIdx = Lex.Number-1;
            if (Func->ParamFlags[Func->printfFmtArgIdx] != 0 ||
                !Func->Params[Func->printfFmtArgIdx].TypeExpr ||
                !Func->Params[Func->printfFmtArgIdx].TypeExpr->IsSimpleType() ||
                ((VTypeExprSimple *)Func->Params[Func->printfFmtArgIdx].TypeExpr)->Type.Type != TYPE_String ||
                Func->printfFmtArgIdx != Func->NumParams-1)
            {
              ParseError(Func->Loc, "printf-like format argument must be `string`");
            }
            //fprintf(stderr, "`%s`: printf, fmtidx=%d\n", *MName, Func->printfFmtArgIdx);
          }
          Lex.NextToken();
        }
      } else {
        ParseError(Lex.Location, "Unknown attribute '%s'", *Lex.Name);
      }
    }
    Lex.Expect(TK_RBracket, ERR_MISSING_RFIGURESCOPE);
  }

  if (Lex.Check(TK_Semicolon)) {
    if ((Func->Flags&FUNC_Native) == 0) ParseError(Func->Loc, "Non-native methods should have a body");
    ++Package->NumBuiltins;
  } else {
    // self type specifier
    // func self(type) -- wtf?!
    if (Lex.Check(TK_Self)) {
      Lex.Expect(TK_LParen);
      if (Lex.Token != TK_Identifier) {
        ParseError(Lex.Location, "Class name expected");
      } else {
        Func->SelfTypeName = Lex.Name;
        Lex.NextToken();
      }
      Lex.Expect(TK_RParen);
    }
    Lex.Expect(TK_LBrace, ERR_MISSING_LBRACE);
    auto oldcf = currFunc;
    currFunc = Func;
    Func->Statement = ParseCompoundStatement();
    currFunc = oldcf;
  }
  unguard;
}


//==========================================================================
//
// VParser::ParseDelegate
//
//==========================================================================
void VParser::ParseDelegate (VExpression *RetType, VField *Delegate) {
  guard(VParser::ParseDelegate);
  VMethod *Func = new VMethod(NAME_None, Delegate, Delegate->Loc);
  Func->ReturnTypeExpr = RetType;
  Lex.Expect(TK_LParen, ERR_MISSING_LPAREN);
  do {
    VMethodParam &P = Func->Params[Func->NumParams];
    int ParmModifiers = TModifiers::Parse(Lex);
    Func->ParamFlags[Func->NumParams] = TModifiers::ParmAttr(TModifiers::Check(ParmModifiers, TModifiers::Optional|TModifiers::Out|TModifiers::Ref, Lex.Location));
    P.TypeExpr = ParseTypeWithPtrs(true);
    if (!P.TypeExpr && Func->NumParams == 0) break;
    if (Lex.Token == TK_Identifier) {
      P.Name = Lex.Name;
      P.Loc = Lex.Location;
      Lex.NextToken();
    }
    if (Func->NumParams == VMethod::MAX_PARAMS) {
      ParseError(Lex.Location, "Method parameters overflow");
      continue;
    }
    ++Func->NumParams;
  } while (Lex.Check(TK_Comma));
  Lex.Expect(TK_RParen, ERR_MISSING_RPAREN);
  Delegate->Func = Func;
  Delegate->Type = VFieldType(TYPE_Delegate);
  Delegate->Type.Function = Func;
  unguard;
}


//==========================================================================
//
// VParser::ParseLambda
//
//==========================================================================
VExpression *VParser::ParseLambda () {
  guard(VParser::ParseLambda);

  TLocation stl = Lex.Location;

  if (!currFunc) { ParseError(stl, "Lambda outside of method"); return new VNullLiteral(stl); }
  if (!currClass) { ParseError(stl, "Lambda outside of class"); return new VNullLiteral(stl); }

  VExpression *Type = ParseTypeWithPtrs(true);
  if (!Type) { ParseError(Lex.Location, "Return type expected"); return new VNullLiteral(stl); }

  if (Lex.Token != TK_LParen) { ParseError(Lex.Location, "Argument list"); delete Type; return new VNullLiteral(stl); }

  VStr newname = VStr(*currFunc->Name)+"-lambda-"+VStr(currFunc->lmbCount++);
  VName lname = VName(*newname);
  //fprintf(stderr, "*** LAMBDA: <%s>\n", *lname);

  VMethod *Func = new VMethod(lname, currClass, stl);
  Func->Flags = currFunc->Flags&(FUNC_Static|FUNC_Final);
  Func->ReturnTypeExpr = Type;
  currClass->AddMethod(Func);

  Lex.Expect(TK_LParen, ERR_MISSING_LPAREN);
  if (Lex.Token != TK_RParen) {
    for (;;) {
      VMethodParam &P = Func->Params[Func->NumParams];
      P.TypeExpr = ParseTypeWithPtrs(true);
      if (!P.TypeExpr) break;
      if (Lex.Token == TK_Identifier) {
        P.Name = Lex.Name;
        P.Loc = Lex.Location;
        Lex.NextToken();
      }
      if (Func->NumParams == VMethod::MAX_PARAMS) {
        delete P.TypeExpr;
        P.TypeExpr = nullptr;
        ParseError(Lex.Location, "Method parameters overflow");
      } else {
        ++Func->NumParams;
      }
      if (Lex.Token == TK_RParen) break;
      Lex.Expect(TK_Comma, ERR_MISSING_RPAREN);
    }
  }
  Lex.Expect(TK_RParen, ERR_MISSING_RPAREN);
  if (Lex.Token != TK_LBrace) { Lex.Expect(TK_LBrace, ERR_MISSING_LBRACE); return new VNullLiteral(stl); }
  auto oldcfn = currFunc;
  currFunc = Func;
  Func->Statement = ParseStatement();
  currFunc = oldcfn;

  if ((currFunc->Flags&FUNC_Static) != 0) {
    ParseError(stl, "Lambdas aren't allowed in static methods");
  } else if ((currFunc->Flags&(FUNC_Final|FUNC_Override)) == FUNC_Final) {
    ParseError(stl, "Lambdas aren't allowed in non-virtual methods");
  }

  return new VDelegateVal(new VSelf(stl), Func, stl);
  unguard;
}


//==========================================================================
//
// VParser::ParseDefaultProperties
//
//==========================================================================
void VParser::ParseDefaultProperties (VClass *InClass, bool doparse, int defcount, VStatement **stats) {
  guard(VParser::ParseDefaultProperties);
  VMethod *Func = new VMethod(NAME_None, InClass, Lex.Location);
  Func->ReturnTypeExpr = new VTypeExprSimple(TYPE_Void, Lex.Location);
  Func->ReturnType = VFieldType(TYPE_Void);
  InClass->DefaultProperties = Func;
  if (doparse) {
    Lex.Expect(TK_LBrace, ERR_MISSING_LBRACE);
    Func->Statement = ParseCompoundStatement();
    if (defcount > 0) {
      VCompound *cst = new VCompound(Func->Statement->Loc);
      for (int f = 0; f < defcount; ++f) cst->Statements.Append(stats[f]);
      // this should be last
      cst->Statements.Append(Func->Statement);
      Func->Statement = cst;
    }
  } else {
    // if we have no 'defaultproperties', create empty compound statement
    Func->Statement = new VCompound(Lex.Location);
    for (int f = 0; f < defcount; ++f) ((VCompound *)Func->Statement)->Statements.Append(stats[f]);
  }
  unguard;
}


//==========================================================================
//
//  VParser::ParseStruct
//
//==========================================================================
void VParser::ParseStruct (VClass *InClass, bool IsVector) {
  guard(VParser::ParseStruct);

  bool globalRO = Lex.Check(TK_ReadOnly);

  VName Name = Lex.Name;
  TLocation StrLoc = Lex.Location;
  if (Lex.Token != TK_Identifier) {
    ParseError(Lex.Location, "Struct name expected");
    Name = NAME_None;
  } else {
    Lex.NextToken();
  }

  // new struct
  VStruct *Struct = new VStruct(Name, (InClass ? (VMemberBase *)InClass : (VMemberBase *)Package), StrLoc);
  Struct->Defined = false;
  Struct->IsVector = IsVector;
  Struct->Fields = nullptr;

  if (!IsVector && Lex.Check(TK_Colon)) {
    if (Lex.Token != TK_Identifier) {
      ParseError(Lex.Location, "Parent struct name expected");
    } else {
      Struct->ParentStructName = Lex.Name;
      Struct->ParentStructLoc = Lex.Location;
      Lex.NextToken();
    }
  }

  Lex.Expect(TK_LBrace, ERR_MISSING_LBRACE);
  while (!Lex.Check(TK_RBrace)) {
    // do `alias new = old;`
    if (Lex.Check(TK_Alias)) {
      bool error = false;
      for (;;) {
        if (Lex.Token != TK_Identifier) { ParseError(Lex.Location, "Identifier name expected"); error = true; break; }
        VName aliasName = Lex.Name;
        TLocation aliasLoc = Lex.Location;
        Lex.NextToken();
        if (!Lex.Check(TK_Assign)) { ParseError(Lex.Location, "`=` expected"); break; }
        if (Lex.Token != TK_Identifier) { ParseError(Lex.Location, "Identifier name expected"); error = true; break; }
        VName origName = Lex.Name;
        Lex.NextToken();
        auto ainfo = Struct->AliasList.get(aliasName);
        if (ainfo) {
          ParseError(Lex.Location, "alias '%s' redeclaration; previous declaration at %s:%d", *aliasName, *ainfo->loc.GetSource(), ainfo->loc.GetLine());
        } else {
          VStruct::AliasInfo ai;
          ai.aliasName = aliasName;
          ai.origName = origName;
          ai.loc = aliasLoc;
          ai.aframe = Struct->AliasFrameNum;
          Struct->AliasList.put(aliasName, ai);
        }
        if (Lex.Check(TK_Semicolon)) { error = false; break; }
        Lex.Expect(TK_Comma, ERR_MISSING_SEMICOLON);
        if (Lex.Check(TK_Semicolon)) { error = false; break; } // alias a = b,; is allowed, 'cause why not?
      }
      if (error && Lex.Token != TK_EOF) {
        if (Lex.Check(TK_Semicolon)) break;
        Lex.NextToken();
      }
      continue;
    }

    vint32 Modifiers = TModifiers::Parse(Lex);
    if (globalRO) Modifiers |= TModifiers::ReadOnly;

    VExpression *Type = ParseType(true); // delegates allowed
    if (!Type) {
      ParseError(Lex.Location, "Field type expected");
      Lex.NextToken();
      continue;
    }

    do {
      VExpression *FieldType = Type->SyntaxCopy();
      FieldType = ParseTypePtrs(FieldType);

      VName FieldName(NAME_None);
      TLocation FieldLoc = Lex.Location;
      if (Lex.Token != TK_Identifier) {
        ParseError(Lex.Location, "Field name expected");
      } else {
        FieldName = Lex.Name;
      }
      Lex.NextToken();
      // array?
      if (Lex.Check(TK_LBracket)) {
        TLocation SLoc = Lex.Location;
        VExpression *e = ParseExpression();
        Lex.Expect(TK_RBracket, ERR_MISSING_RFIGURESCOPE);
        FieldType = new VFixedArrayType(FieldType, e, SLoc);
      }
      // create field
      VField *fi = new VField(FieldName, Struct, FieldLoc);
      fi->TypeExpr = FieldType;
      fi->Flags = TModifiers::FieldAttr(TModifiers::Check(Modifiers,
        TModifiers::Native|TModifiers::Private|TModifiers::Protected|
        TModifiers::ReadOnly|TModifiers::Transient, FieldLoc));
      // delegate?
      if (FieldType->IsDelegateType()) {
        fi->Func = ((VDelegateType *)FieldType)->CreateDelegateMethod(Struct);
        fi->Type = VFieldType(TYPE_Delegate);
        fi->Type.Function = fi->Func;
        fi->TypeExpr = nullptr;
        delete FieldType;
      }
      Struct->AddField(fi);
    } while (Lex.Check(TK_Comma));
    delete Type;
    Type = nullptr;
    Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
  } while (Lex.Check(TK_Semicolon)) {}
  //Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);

  if (InClass) {
    InClass->Structs.Append(Struct);
  } else {
    Package->ParsedStructs.Append(Struct);
  }
  unguard;
}


//==========================================================================
//
//  VParser::ParseStateString
//
//==========================================================================
VName VParser::ParseStateString () {
  guard(VParser::ParseStateString);
  VStr StateStr;

  if (Lex.Token != TK_Identifier && Lex.Token != TK_StringLiteral) {
    ParseError(Lex.Location, "Identifier expected");
    return NAME_None;
  }
  StateStr = Lex.String;
  Lex.NextToken();

  if (Lex.Check(TK_DColon)) {
    if (Lex.Token != TK_Identifier) {
      ParseError(Lex.Location, "Identifier expected");
      return NAME_None;
    }
    StateStr += "::";
    StateStr += *Lex.Name;
    Lex.NextToken();
  }

  if (Lex.Check(TK_Dot)) {
    if (Lex.Token != TK_Identifier) {
      ParseError(Lex.Location, "Identifier expected");
      return NAME_None;
    }
    StateStr += ".";
    StateStr += *Lex.Name;
    Lex.NextToken();
  }

  return *StateStr;
  unguard;
}


//==========================================================================
//
//  VParser::ParseStates
//
//==========================================================================
void VParser::ParseStates (VClass *InClass) {
  guard(VParser::ParseStates);
  if (Lex.Token == TK_Identifier && VStr::ICmp(*Lex.Name, "d2df") == 0) {
    Lex.NextToken();
    ParseStatesNewStyle(InClass);
    return;
  }
  Lex.Expect(TK_LBrace, ERR_MISSING_LBRACE);
  int StateIdx = 0;
  VState *PrevState = nullptr;
  VState *LoopStart = nullptr;
  int NewLabelsStart = InClass->StateLabelDefs.Num();
  while (!Lex.Check(TK_RBrace)) {
    TLocation TmpLoc = Lex.Location;
    VName TmpName = ParseStateString();

    // goto command
    if (TmpName == NAME_Goto) {
      VName GotoLabel = ParseStateString();
      int GotoOffset = 0;
      if (Lex.Check(TK_Plus)) {
        if (Lex.Token != TK_IntLiteral) {
          ParseError(Lex.Location, "Number expected");
          continue;
        }
        GotoOffset = Lex.Number;
        Lex.NextToken();
      }
      if (!PrevState && NewLabelsStart == InClass->StateLabelDefs.Num()) {
        ParseError(Lex.Location, "Goto before first state");
        continue;
      }
      if (PrevState) {
        PrevState->GotoLabel = GotoLabel;
        PrevState->GotoOffset = GotoOffset;
      }
      for (int i = NewLabelsStart; i < InClass->StateLabelDefs.Num(); ++i) {
        InClass->StateLabelDefs[i].GotoLabel = GotoLabel;
        InClass->StateLabelDefs[i].GotoOffset = GotoOffset;
      }
      NewLabelsStart = InClass->StateLabelDefs.Num();
      PrevState = nullptr;
      continue;
    }

    // stop command
    if (TmpName == NAME_Stop) {
      if (!PrevState && NewLabelsStart == InClass->StateLabelDefs.Num()) {
        ParseError(Lex.Location, "Stop before first state");
        continue;
      }
      if (PrevState) PrevState->NextState = nullptr;
      for (int i = NewLabelsStart; i < InClass->StateLabelDefs.Num(); ++i) {
        InClass->StateLabelDefs[i].State = nullptr;
      }
      NewLabelsStart = InClass->StateLabelDefs.Num();
      PrevState = nullptr;
      continue;
    }

    // wait command
    if (TmpName == NAME_Wait || TmpName == NAME_Fail) {
      if (!PrevState) {
        ParseError(Lex.Location, "%s before first state", *TmpName);
        continue;
      }
      PrevState->NextState = PrevState;
      PrevState = nullptr;
      continue;
    }

    // loop command
    if (TmpName == NAME_Loop) {
      if (!PrevState) {
        ParseError(Lex.Location, "Loop before first state");
        continue;
      }
      PrevState->NextState = LoopStart;
      PrevState = nullptr;
      continue;
    }

    // check for label
    if (Lex.Check(TK_Colon)) {
      VStateLabelDef &Lbl = InClass->StateLabelDefs.Alloc();
      Lbl.Loc = TmpLoc;
      Lbl.Name = *TmpName;
      continue;
    }

    char StateName[16];
    snprintf(StateName, sizeof(StateName), "S_%d", StateIdx);
    VState *s = new VState(StateName, InClass, TmpLoc);
    InClass->AddState(s);

    // sprite name
    char SprName[8];
    SprName[0] = 0;
    if (VStr::Length(*TmpName) != 4) {
      ParseError(Lex.Location, "Invalid sprite name");
    } else {
      SprName[0] = VStr::ToLower((*TmpName)[0]);
      SprName[1] = VStr::ToLower((*TmpName)[1]);
      SprName[2] = VStr::ToLower((*TmpName)[2]);
      SprName[3] = VStr::ToLower((*TmpName)[3]);
      SprName[4] = 0;
    }
    s->SpriteName = SprName;

    // frame
    VName FramesString(NAME_None);
    TLocation FramesLoc;
    if (Lex.Token != TK_Identifier && Lex.Token != TK_StringLiteral) ParseError(Lex.Location, "Identifier expected");
    char FChar = VStr::ToUpper(Lex.String[0]);
    if (FChar < '0' || FChar < 'A' || FChar > ']') ParseError(Lex.Location, "Frames must be 0-9, A-Z, [, \\ or ]");
    s->Frame = FChar-'A';
    FramesString = Lex.String;
    FramesLoc = Lex.Location;
    Lex.NextToken();

    // tics
    bool Neg = Lex.Check(TK_Minus);
    if (Lex.Token == TK_IntLiteral) {
      s->Time = (Neg ? -float(Lex.Number) : float(Lex.Number)/35.0f);
      Lex.NextToken();
    } else if (Lex.Token == TK_FloatLiteral) {
      s->Time = (Neg ? -float(Lex.Float) : float(Lex.Float)/35.0f);
      Lex.NextToken();
    } else {
      ParseError(Lex.Location, "State duration expected");
    }

    // options
    while (Lex.Token == TK_Identifier && !Lex.NewLine) {
      if (Lex.Name == NAME_Bright) {
        s->Frame |= VState::FF_FULLBRIGHT;
        Lex.NextToken();
        continue;
      }
      if (Lex.Name == NAME_Offset) {
        Lex.NextToken();
        Lex.Expect(TK_LParen);
        Neg = Lex.Check(TK_Minus);
        if (Lex.Token != TK_IntLiteral) ParseError(Lex.Location, "Integer expected");
        s->Misc1 = Lex.Number*(Neg ? -1 : 1);
        Lex.NextToken();
        Lex.Expect(TK_Comma);
        Neg = Lex.Check(TK_Minus);
        if (Lex.Token != TK_IntLiteral) ParseError(Lex.Location, "Integer expected");
        s->Misc2 = Lex.Number*(Neg ? -1 : 1);
        Lex.NextToken();
        Lex.Expect(TK_RParen);
        continue;
      }
      break;
    }

    // code
    if (Lex.Check(TK_LBrace)) {
      if (VStr::Length(*FramesString) > 1) ParseError(Lex.Location, "Only states with single frame can have code block");
      s->Function = new VMethod(NAME_None, s, s->Loc);
      s->Function->ReturnTypeExpr = new VTypeExprSimple(TYPE_Void, Lex.Location);
      s->Function->ReturnType = VFieldType(TYPE_Void);
      s->Function->Statement = ParseCompoundStatement();
    } else if (!Lex.NewLine) {
      if (Lex.Token != TK_Identifier) {
        ParseError(Lex.Location, "State method name expected");
      } else {
        s->FunctionName = Lex.Name;
        Lex.NextToken();
      }
    }

    // Link previous state
    if (PrevState) PrevState->NextState = s;

    // assign state to the labels
    for (int i = NewLabelsStart; i < InClass->StateLabelDefs.Num(); ++i) {
      InClass->StateLabelDefs[i].State = s;
      LoopStart = s;
    }
    NewLabelsStart = InClass->StateLabelDefs.Num();
    PrevState = s;
    ++StateIdx;

    for (size_t i = 1; i < VStr::Length(*FramesString); ++i) {
      char FSChar = VStr::ToUpper((*FramesString)[i]);
      if (FSChar < 'A' || FSChar > ']') ParseError(Lex.Location, "Frames must be A-Z, [, \\ or ]");
      // create a new state
      snprintf(StateName, sizeof(StateName), "S_%d", StateIdx);
      VState *s2 = new VState(StateName, InClass, TmpLoc);
      InClass->AddState(s2);
      s2->SpriteName = s->SpriteName;
      s2->Frame = (s->Frame & VState::FF_FULLBRIGHT)|(FSChar-'A');
      s2->Time = s->Time;
      s2->Misc1 = s->Misc1;
      s2->Misc2 = s->Misc2;
      s2->FunctionName = s->FunctionName;
      // link previous state
      PrevState->NextState = s2;
      PrevState = s2;
      ++StateIdx;
    }
  }

  // make sure all state labels have corresponding states
  if (NewLabelsStart != InClass->StateLabelDefs.Num()) ParseError(Lex.Location, "State label at the end of state block");
  if (PrevState) ParseError(Lex.Location, "State block not ended");

  unguard;
}


//==========================================================================
//
//  VParser::ParseStatesNewStyle
//
//==========================================================================
void VParser::ParseStatesNewStyle (VClass *inClass) {
  guard(VParser::ParseStatesNewStyle);

  Lex.Expect(TK_LBrace, ERR_MISSING_LBRACE);

  int stateIdx = 0;
  VState *prevState = nullptr;
  VState *loopStart = nullptr;
  int newLabelsStart = inClass->StateLabelDefs.Num();
  bool stateSeen = false;
  bool optOptionsSeen = false;

  struct TextureInfo {
    VStr texImage;
    int frameWidth;
    int frameHeight;
    int frameOfsX;
    int frameOfsY;
  };
  TMapDtor<VStr, TextureInfo> texlist;

  VStr texDir;
  float tickTime = 28.0f*2/1000.0f; //DF does it once per two frames

  while (!Lex.Check(TK_RBrace)) {
    TLocation tmpLoc = Lex.Location;

    // "texture"
    if (!stateSeen && Lex.Token == TK_Identifier && VStr::Cmp(*Lex.Name, "texture") == 0) {
      auto stloc = Lex.Location;
      Lex.NextToken();
      VStr txname;
      // name
           if (Lex.Token == TK_Identifier || Lex.Token == TK_NameLiteral) { txname = VStr(*Lex.Name); Lex.NextToken(); }
      else if (Lex.Token == TK_StringLiteral) { txname = Lex.String; Lex.NextToken(); }
      else ParseError(Lex.Location, "Texture name expected");
      txname = txname.toLowerCase();
      // subblock
      if (!Lex.Check(TK_LBrace)) {
        Lex.Expect(TK_LBrace, ERR_MISSING_LBRACE);
        continue;
      }
      TextureInfo ti;
      ti.texImage = VStr();
      ti.frameWidth = -1;
      ti.frameHeight = -1;
      ti.frameOfsX = 0;
      ti.frameOfsY = 0;
      while (!Lex.Check(TK_RBrace)) {
        if (Lex.Token != TK_Identifier) {
          ParseError(Lex.Location, "Texture option expected");
          break;
        }
        if (VStr::ICmp(*Lex.Name, "image") == 0) {
          Lex.NextToken();
               if (Lex.Token == TK_Identifier || Lex.Token == TK_NameLiteral) { ti.texImage = VStr(*Lex.Name); Lex.NextToken(); }
          else if (Lex.Token == TK_StringLiteral) { ti.texImage = Lex.String; Lex.NextToken(); }
          else ParseError(Lex.Location, "String expected");
          Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
        } else if (VStr::ICmp(*Lex.Name, "frame_width") == 0) {
          Lex.NextToken();
          if (Lex.Token == TK_IntLiteral) { ti.frameWidth = Lex.Number; Lex.NextToken(); }
          else ParseError(Lex.Location, "Integer expected");
          Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
        } else if (VStr::ICmp(*Lex.Name, "frame_height") == 0) {
          Lex.NextToken();
          if (Lex.Token == TK_IntLiteral) { ti.frameHeight = Lex.Number; Lex.NextToken(); }
          else ParseError(Lex.Location, "Integer expected");
          Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
        } else if (VStr::ICmp(*Lex.Name, "frame_offset_x") == 0) {
          Lex.NextToken();
          bool neg = Lex.Check(TK_Minus);
          if (!neg) Lex.Check(TK_Plus);
          if (Lex.Token == TK_IntLiteral) { ti.frameOfsX = Lex.Number*(neg ? -1 : 1); Lex.NextToken(); }
          else ParseError(Lex.Location, "Integer expected");
          Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
        } else if (VStr::ICmp(*Lex.Name, "frame_offset_y") == 0) {
          Lex.NextToken();
          bool neg = Lex.Check(TK_Minus);
          if (!neg) Lex.Check(TK_Plus);
          if (Lex.Token == TK_IntLiteral) { ti.frameOfsY = Lex.Number*(neg ? -1 : 1); Lex.NextToken(); }
          else ParseError(Lex.Location, "Integer expected");
          Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
        } else {
          ParseError(Lex.Location, "Uknown texture option '%s'", *Lex.Name);
          Lex.NextToken();
        }
      }
      if (ti.texImage.isEmpty()) {
        ParseError(stloc, "No texture image specified");
      } else if (txname.isEmpty()) {
        ParseError(stloc, "No texture name specified");
      } else {
        ti.texImage = ti.texImage.toLowerCase();
        texlist.remove(txname);
        texlist.put(txname, ti);
      }
      continue;
    }

    // "options"
    if (!stateSeen && Lex.Token == TK_Identifier && VStr::Cmp(*Lex.Name, "options") == 0) {
      if (optOptionsSeen) ParseError(Lex.Location, "Duplicate `options` subblock"); else optOptionsSeen = true;
      Lex.NextToken();
      if (!Lex.Check(TK_LBrace)) {
        Lex.Expect(TK_LBrace, ERR_MISSING_LBRACE);
        continue;
      }
      while (!Lex.Check(TK_RBrace)) {
        if (Lex.Token != TK_Identifier) {
          ParseError(Lex.Location, "Option name expected");
          break;
        }
        if (VStr::ICmp(*Lex.Name, "texture_dir") == 0) {
          Lex.NextToken();
               if (Lex.Token == TK_Identifier || Lex.Token == TK_NameLiteral) { texDir = VStr(*Lex.Name); Lex.NextToken(); }
          else if (Lex.Token == TK_StringLiteral) { texDir = Lex.String; Lex.NextToken(); }
          else ParseError(Lex.Location, "String expected");
          Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
        } else if (VStr::ICmp(*Lex.Name, "tick_time") == 0) {
          Lex.NextToken();
               if (Lex.Token == TK_IntLiteral) { tickTime = Lex.Number; Lex.NextToken(); }
          else if (Lex.Token == TK_FloatLiteral) { tickTime = Lex.Float; Lex.NextToken(); }
          else ParseError(Lex.Location, "Float expected");
          Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
        } else {
          ParseError(Lex.Location, "Uknown option '%s'", *Lex.Name);
          Lex.NextToken();
        }
      }
      if (!texDir.isEmpty() && !texDir.endsWith("/")) texDir += "/";
      continue;
    }

    stateSeen = true;

    // parse first token
    VStr tmpName;

    if (Lex.Token != TK_Identifier && Lex.Token != TK_StringLiteral && Lex.Token != TK_NameLiteral) {
      if (Lex.Token == TK_None) {
        tmpName = VStr("none");
        Lex.NextToken();
      } else {
        ParseError(Lex.Location, "Identifier expected");
      }
    } else {
      bool wasString = (Lex.Token == TK_StringLiteral);
      tmpName = (Lex.Token == TK_StringLiteral ? Lex.String : VStr(*Lex.Name));
      Lex.NextToken();
      if (!wasString && Lex.Check(TK_Dot)) {
        if (Lex.Token != TK_Identifier) {
          ParseError(Lex.Location, "Identifier after dot expected");
          tmpName.clear();
        } else {
          tmpName += ".";
          tmpName += *Lex.Name;
          Lex.NextToken();
        }
      }
    }

    // goto command
    if (VStr::ICmp(*tmpName, "goto") == 0) {
      VName gotoLabel; // = ParseStateString();

      VStr tmpStateStr;
      if (Lex.Token != TK_Identifier && Lex.Token != TK_StringLiteral) {
        ParseError(Lex.Location, "Identifier expected in `goto`");
        //tmpStateStr NAME_None;
      } else {
        tmpStateStr = Lex.String;
        Lex.NextToken();

        if (Lex.Check(TK_DColon)) {
          if (Lex.Token != TK_Identifier) {
            ParseError(Lex.Location, "Identifier after `::` expected in `goto`");
            tmpStateStr.clear();
          } else {
            tmpStateStr += "::";
            tmpStateStr += *Lex.Name;
            Lex.NextToken();
            if (Lex.Token != TK_Identifier) {
              ParseError(Lex.Location, "Identifier expected in `goto`");
              tmpStateStr.clear();
            } else {
              tmpStateStr += *Lex.Name;
              Lex.NextToken();
            }
          }
        }

        if (Lex.Check(TK_Dot)) {
          if (Lex.Token != TK_Identifier) {
            ParseError(Lex.Location, "Identifier after dot expected in `goto`");
            tmpStateStr.clear();
          } else {
            if (!tmpStateStr.isEmpty()) {
              tmpStateStr += ".";
              tmpStateStr += *Lex.Name;
            }
            Lex.NextToken();
          }
        }

        gotoLabel = VName(*tmpStateStr);
      }
      // no offsets allowed

      if (!prevState && newLabelsStart == inClass->StateLabelDefs.Num()) {
        ParseError(Lex.Location, "`goto` before first state");
        continue;
      }
      if (prevState) {
        prevState->GotoLabel = gotoLabel;
        prevState->GotoOffset = 0;
      }
      for (int i = newLabelsStart; i < inClass->StateLabelDefs.Num(); ++i) {
        inClass->StateLabelDefs[i].GotoLabel = gotoLabel;
        inClass->StateLabelDefs[i].GotoOffset = 0;
      }
      newLabelsStart = inClass->StateLabelDefs.Num();
      prevState = nullptr;

      Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
      while (Lex.Check(TK_Semicolon)) {}
      continue;
    }

    // stop command
    if (VStr::ICmp(*tmpName, "stop") == 0) {
      if (!prevState && newLabelsStart == inClass->StateLabelDefs.Num()) {
        ParseError(Lex.Location, "`stop` before first state");
        continue;
      }
      if (prevState) prevState->NextState = nullptr;
      for (int i = newLabelsStart; i < inClass->StateLabelDefs.Num(); ++i) {
        inClass->StateLabelDefs[i].State = nullptr;
      }
      newLabelsStart = inClass->StateLabelDefs.Num();
      prevState = nullptr;

      Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
      while (Lex.Check(TK_Semicolon)) {}
      continue;
    }

    // wait command
    if (VStr::ICmp(*tmpName, "wait") == 0) {
      if (!prevState) {
        ParseError(Lex.Location, "`wait` before first state");
        continue;
      }
      prevState->NextState = prevState;
      prevState = nullptr;

      Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
      while (Lex.Check(TK_Semicolon)) {}
      continue;
    }

    // loop command
    if (VStr::ICmp(*tmpName, "loop") == 0) {
      if (!prevState) {
        ParseError(Lex.Location, "`loop` before first state");
        continue;
      }
      prevState->NextState = loopStart;
      prevState = nullptr;

      Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
      while (Lex.Check(TK_Semicolon)) {}
      continue;
    }

    //fprintf(stderr, "*** <%s>\n", *tmpName);

    // check for label
    if (Lex.Check(TK_Colon)) {
      VStateLabelDef &Lbl = inClass->StateLabelDefs.Alloc();
      Lbl.Loc = tmpLoc;
      Lbl.Name = tmpName;
      continue;
    }

    // create new state
    char StateName[64];
    snprintf(StateName, sizeof(StateName), "S_%d", stateIdx);
    VState *s = new VState(StateName, inClass, tmpLoc);
    inClass->AddState(s);

    // sprite name
    if (!tmpName.isEmpty() && tmpName.ICmp("none") != 0) {
      VStr sprName = tmpName.toLowerCase();
      if (texlist.has(sprName)) {
        TextureInfo *ti = texlist.get(sprName);
        sprName = ti->texImage;
        s->frameWidth = ti->frameWidth;
        s->frameHeight = ti->frameHeight;
        s->frameOfsX = ti->frameOfsX;
        s->frameOfsY = ti->frameOfsY;
      }
      if (!texDir.isEmpty()) sprName = texDir+sprName;
      s->SpriteName = VName(*sprName);
    } else {
      s->SpriteName = NAME_None;
    }

    // frames
    int frameArr[512];
    int frameUsed = 0;

    // [framelist]
    if (Lex.Check(TK_LBracket)) {
      bool wasError = false;
      for (;;) {
        if (Lex.Token != TK_IntLiteral) {
          if (!wasError) ParseError(Lex.Location, "Integer expected"); else wasError = true;
          break;
        }
        int frm = Lex.Number;
        Lex.NextToken();
        int frmEnd = frm;
        if (frm >= 32768) {
          if (!wasError) ParseError(Lex.Location, "Frame number too big"); else wasError = true;
          frm = 0;
        }
        // check for '..'
        if (Lex.Check(TK_DotDot)) {
          if (Lex.Token != TK_IntLiteral) {
            if (!wasError) ParseError(Lex.Location, "Integer expected"); else wasError = true;
            break;
          }
          frmEnd = Lex.Number;
          Lex.NextToken();
          if (frmEnd >= 32768) {
            if (!wasError) ParseError(Lex.Location, "Frame number too big"); else wasError = true;
            frmEnd = 0;
          }
          if (frmEnd < frm) {
            if (!wasError) ParseError(Lex.Location, "Invalid frame range"); else wasError = true;
          } else if (frmEnd-frm > 512-frameUsed) {
            if (!wasError) ParseError(Lex.Location, "Frame range too long"); else wasError = true;
            frmEnd = -666;
          }
        }
        if (!wasError) {
          while (frm <= frmEnd) {
            if (frameUsed >= 512) {
              ParseError(Lex.Location, "Too many frames");
              break;
            }
            frameArr[frameUsed++] = frm++;
          }
        }
        if (Lex.Token != TK_Comma) break;
        Lex.NextToken();
        if (Lex.Token == TK_RBracket) break;
      }
      if (Lex.Token != TK_RBracket) {
        if (!wasError) ParseError(Lex.Location, "`]` expected");
      } else {
        Lex.NextToken();
      }
      if (wasError) frameUsed = 0; // it doesn't matter
    }

    if (frameUsed < 1) {
      frameArr[0] = 0;
      frameUsed = 1;
    }

    s->Frame = frameArr[0];

    // tics
    bool neg = Lex.Check(TK_Minus);
    if (Lex.Token == TK_IntLiteral) {
      s->Time = Lex.Number*tickTime;
      Lex.NextToken();
    } else if (Lex.Token == TK_FloatLiteral) {
      s->Time = Lex.Float;
      Lex.NextToken();
    } else {
      ParseError(Lex.Location, "State duration expected");
    }
    if (neg) s->Time = -s->Time;

    // options
    while (Lex.Token == TK_Identifier) {
      if (Lex.Name == "_ofs") {
        Lex.NextToken();
        if (!Lex.Check(TK_Assign) && !Lex.Check(TK_Colon)) ParseError(Lex.Location, "`:` expected");
        Lex.Expect(TK_LParen, ERR_MISSING_LPAREN);
        for (int f = 0; f < 2; ++f) {
          bool neg = Lex.Check(TK_Minus);
          if (!neg) Lex.Check(TK_Plus);
          if (Lex.Token == TK_IntLiteral) {
            if (neg) Lex.Number = -Lex.Number;
            if (f == 0) s->frameOfsX += Lex.Number; else s->frameOfsY += Lex.Number;
            Lex.NextToken();
          } else {
            ParseError(Lex.Location, "Integer expected");
          }
          if (f == 0) {
            if (!Lex.Check(TK_Comma)) ParseError(Lex.Location, "`,` expected");
          }
        }
        Lex.Expect(TK_RParen, ERR_MISSING_LPAREN);
        continue;
      }
      break;
    }

    /*
    {
      fprintf(stderr, "state: sprite=<%s>; time=%f; frame", *s->SpriteName, s->Time);
      if (frameUsed == 1) {
        fprintf(stderr, "=%d\n", frameArr[0]);
      } else {
        fprintf(stderr, "s=[");
        for (int f = 0; f < frameUsed; ++f) {
          if (f) fprintf(stderr, ",");
          fprintf(stderr, "%d", frameArr[f]);
        }
        fprintf(stderr, "]\n");
      }
    }
    */

    //Lex.NewLine

    // code
    if (Lex.Check(TK_LBrace)) {
      //if (frameUsed != 1) ParseError(Lex.Location, "Only states with single frame can have code block");
      s->Function = new VMethod(NAME_None, s, s->Loc);
      s->Function->ReturnTypeExpr = new VTypeExprSimple(TYPE_Void, Lex.Location);
      s->Function->ReturnType = VFieldType(TYPE_Void);
      s->Function->Statement = ParseCompoundStatement();
    } else if (!Lex.NewLine) {
      auto stloc = Lex.Location;
      // function call
      if (Lex.Token != TK_Identifier) {
        ParseError(Lex.Location, "State method name expected");
      } else {
        s->FunctionName = Lex.Name;
        Lex.NextToken();
        VExpression *Args[VMethod::MAX_PARAMS+1];
        int NumArgs = 0;
        // function call?
        if (Lex.Check(TK_LParen)) {
          NumArgs = ParseArgList(Lex.Location, Args);
          if (NumArgs > 0) {
            VExpression *e = new VCastOrInvocation(s->FunctionName, stloc, NumArgs, Args);
            auto cst = new VCompound(stloc);
            cst->Statements.Append(new VExpressionStatement(e));
            // create function
            s->Function = new VMethod(NAME_None, s, s->Loc);
            s->Function->ReturnTypeExpr = new VTypeExprSimple(TYPE_Void, Lex.Location);
            s->Function->ReturnType = VFieldType(TYPE_Void);
            s->Function->Statement = cst;
            s->FunctionName = NAME_None;
          }
        }
      }
      Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
    }
    while (Lex.Check(TK_Semicolon)) {}

    // link to previous state
    if (prevState) prevState->NextState = s;

    // assign state to the labels
    for (int i = newLabelsStart; i < inClass->StateLabelDefs.Num(); ++i) {
      inClass->StateLabelDefs[i].State = s;
      loopStart = s;
    }
    newLabelsStart = inClass->StateLabelDefs.Num();
    prevState = s;
    ++stateIdx;

    // create states for frames
    for (int f = 1; f < frameUsed; ++f) {
      // create a new state
      snprintf(StateName, sizeof(StateName), "S_%d", stateIdx);
      VState *s2 = new VState(StateName, inClass, tmpLoc);
      inClass->AddState(s2);
      s2->SpriteName = s->SpriteName;
      s2->Frame = frameArr[f]; //(s->Frame & VState::FF_FULLBRIGHT)|(FSChar-'A');
      s2->Time = s->Time;
      s2->Misc1 = s->Misc1;
      s2->Misc2 = s->Misc2;
      s2->FunctionName = s->FunctionName;
      if (s->Function) {
        s2->Function = new VMethod(NAME_None, s2, s->Loc);
        s2->Function->ReturnTypeExpr = s->Function->ReturnTypeExpr->SyntaxCopy();
        s2->Function->ReturnType = VFieldType(TYPE_Name);
        s2->Function->Statement = s->Function->Statement->SyntaxCopy();
      }
      s2->frameWidth = s->frameWidth;
      s2->frameHeight = s->frameHeight;
      s2->frameOfsX = s->frameOfsX;
      s2->frameOfsY = s->frameOfsY;
      // link to previous state
      prevState->NextState = s2;
      prevState = s2;
      ++stateIdx;
    }
  }

  // make sure all state labels have corresponding states
  if (newLabelsStart != inClass->StateLabelDefs.Num()) ParseError(Lex.Location, "State label at the end of state block");
  if (prevState) ParseError(Lex.Location, "State block not ended");

  unguard;
}


//==========================================================================
//
// VParser::ParseReplication
//
//==========================================================================
void VParser::ParseReplication (VClass *Class) {
  guard(VParser::ParseReplication);
  Lex.Expect(TK_LBrace);
  while (!Lex.Check(TK_RBrace)) {
    VRepInfo &RI = Class->RepInfos.Alloc();

    // reliable or unreliable flag, currently unused.
         if (Lex.Check(TK_Reliable)) RI.Reliable = true;
    else if (Lex.Check(TK_Unreliable)) RI.Reliable = false;
    else ParseError(Lex.Location, "Expected reliable or unreliable");

    // replication condition
    RI.Cond = new VMethod(NAME_None, Class, Lex.Location);
    RI.Cond->ReturnType = VFieldType(TYPE_Bool);
    RI.Cond->ReturnType.BitMask = 1;
    RI.Cond->ReturnTypeExpr = new VTypeExprSimple(RI.Cond->ReturnType, Lex.Location);
    Lex.Expect(TK_If);
    Lex.Expect(TK_LParen, ERR_MISSING_LPAREN);
    VExpression *e = ParseExpression();
    if (!e) ParseError(Lex.Location, "If expression expected");
    Lex.Expect(TK_RParen, ERR_MISSING_RPAREN);
    RI.Cond->Statement = new VReturn(e, RI.Cond->Loc);

    // fields
    do {
      if (Lex.Token != TK_Identifier) {
        ParseError(Lex.Location, "Field name expected");
      } else {
        VRepField &F = RI.RepFields.Alloc();
        F.Name = Lex.Name;
        F.Loc = Lex.Location;
        F.Member = nullptr;
        Lex.NextToken();
      }
    } while (Lex.Check(TK_Comma));
    Lex.Expect(TK_Semicolon);
  }
  unguard;
}


//==========================================================================
//
//  VParser::ParseClass
//
//==========================================================================
void VParser::ParseClass () {
  guard(VParser::ParseClass);

  VName ClassName = Lex.Name;
  TLocation ClassLoc = Lex.Location;
  VClass *ExistingClass = nullptr;

  if (Lex.Token != TK_Identifier) {
    ParseError(Lex.Location, "Class name expected");
    ClassName = NAME_None;
  } else {
    ExistingClass = VMemberBase::StaticFindClass(Lex.Name);
  }
  Lex.NextToken();

  VName ParentClassName = NAME_None;
  TLocation ParentClassLoc;

  if (Lex.Check(TK_Colon)) {
    if (Lex.Token != TK_Identifier) {
      ParseError(Lex.Location, "Parent class name expected");
    } else {
      ParentClassName = Lex.Name;
      ParentClassLoc = Lex.Location;
      Lex.NextToken();
    }
  } else if (ClassName != NAME_Object) {
    ParseError(Lex.Location, "Parent class expected");
  }

  if (Lex.Check(TK_Decorate)) {
    Lex.Expect(TK_Semicolon);

    if (ExistingClass) return;

#if !defined(IN_VCC)
    // check if it already exists n DECORATE imports
    for (int i = 0; i < VMemberBase::GDecorateClassImports.Num(); ++i) {
      if (VMemberBase::GDecorateClassImports[i]->Name == ClassName) {
        Package->ParsedDecorateImportClasses.Append(VMemberBase::GDecorateClassImports[i]);
        return;
      }
    }
#endif

    // new class
    VClass *Class = new VClass(ClassName, Package, ClassLoc);
    Class->Defined = false;

    if (ParentClassName != NAME_None) {
      Class->ParentClassName = ParentClassName;
      Class->ParentClassLoc = ParentClassLoc;
    }

    // this class is not IN this package
    Class->MemberType = MEMBER_DecorateClass;
    Class->Outer = nullptr;
    Package->ParsedDecorateImportClasses.Append(Class);
#if !defined(IN_VCC)
    VMemberBase::GDecorateClassImports.Append(Class);
#endif
    return;
  }

  // for engine package use native class objects (k8: not necessary anymore)
  VClass *Class;
#if !defined(IN_VCC)
  Class = nullptr;
  /*if (Package->Name == NAME_engine)*/ Class = VClass::FindClass(*ClassName);
  if (Class) {
    // if `Defined `is not set, and this is not a native class -- it's a duplicate
    if (Class->ClassFlags&CLASS_Native) {
      if (!Class->Defined) {
        ParseError(ClassLoc, "duplicate class declaration `%s` (%d)", *ClassName, Class->ClassFlags);
      } else {
        //fprintf(stderr, "BOOO! <%s>\n", *ClassName);
      }
      //check(Class->Defined);
      Class->Outer = Package;
    } else {
      ParseError(ClassLoc, "duplicate class declaration `%s`", *ClassName);
    }
  }
  else
#endif
  {
    // new class
    Class = new VClass(ClassName, Package, ClassLoc);
  }
  Class->Defined = false;

  if (ParentClassName != NAME_None) {
    Class->ParentClassName = ParentClassName;
    Class->ParentClassLoc = ParentClassLoc;
  }

  // we will collect default field values, and insert 'em in `defaultproperties` block
  VStatement **defstats = nullptr;
  int defcount = 0;
  int defallot = 0;

  int ClassAttr = TModifiers::ClassAttr(TModifiers::Check(TModifiers::Parse(Lex), TModifiers::Native|TModifiers::Abstract, Lex.Location));
  Class->ClassFlags = ClassAttr;
  // parse class attributes
  do {
    if (Lex.Check(TK_MobjInfo)) {
      Lex.Expect(TK_LParen, ERR_MISSING_LPAREN);
      VExpression *e = ParseExpression();
      if (!e) {
        ParseError(Lex.Location, "Constant expression expected");
      } else if (Class->MobjInfoExpr) {
        ParseError(Lex.Location, "Only one Editor ID allowed");
      } else {
        Class->MobjInfoExpr = e;
      }
      Lex.Expect(TK_RParen, ERR_MISSING_RPAREN);
    } else if (Lex.Check(TK_ScriptId)) {
      Lex.Expect(TK_LParen, ERR_MISSING_LPAREN);
      VExpression *e = ParseExpression();
      if (!e) {
        ParseError(Lex.Location, "Constant expression expected");
      } else if (Class->ScriptIdExpr) {
        ParseError(Lex.Location, "Only one script ID allowed");
      } else {
        Class->ScriptIdExpr = e;
      }
      Lex.Expect(TK_RParen, ERR_MISSING_RPAREN);
    } else if (Lex.Check(TK_Game)) {
      Lex.Expect(TK_LParen, ERR_MISSING_LPAREN);
      VExpression *e = ParseExpression();
      if (!e) {
        ParseError(Lex.Location, "Constant expression expected");
      } else if (Class->GameExpr) {
        ParseError(Lex.Location, "Only one game expression allowed");
      } else {
        Class->GameExpr = e;
      }
      Lex.Expect(TK_RParen, ERR_MISSING_RPAREN);
    } else {
      break;
    }
  } while (1);
  Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);

  // parse class definitions
  auto oldcc = currClass;
  currClass = Class;

  bool skipDefaultProperties = false;
  while (!Lex.Check(TK_DefaultProperties)) {
    if (Lex.Check(TK_EOF)) { skipDefaultProperties = true; break; }

    // another class?
    if (Lex.Token == TK_Class) {
      char nch = Lex.peekNextNonBlankChar();
      // identifier?
      if ((nch >= 'A' && nch <= 'Z') || (nch >= 'a' && nch <= 'z') || nch == '_') { skipDefaultProperties = true; break; }
    }

    if (Lex.Check(TK_States)) {
      ParseStates(Class);
      continue;
    }

    if (Lex.Token == TK_Enum || Lex.Token == TK_BitEnum) {
      bool bitconst = (Lex.Token == TK_BitEnum);
      Lex.NextToken();
      VConstant *PrevValue = nullptr;
      // check for `enum const = val;`
      if (Lex.Token == TK_Identifier) {
        if (Class->FindConstant(Lex.Name)) ParseError(Lex.Location, "Redefined identifier `%s`", *Lex.Name);
        VConstant *cDef = new VConstant(Lex.Name, Class, Lex.Location);
        cDef->Type = TYPE_Int;
        cDef->bitconstant = bitconst;
        Class->AddConstant(cDef);
        Lex.NextToken();
        if (Lex.Check(TK_Assign)) {
          cDef->ValueExpr = ParseExpression();
          Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
        } else {
          ParseError(Lex.Location, "`=` expected");
        }
      } else {
        Lex.Expect(TK_LBrace, ERR_MISSING_LBRACE);
        for (;;) {
          if (Lex.Token != TK_Identifier) {
            ParseError(Lex.Location, "Identifier expected");
            break;
          }
          if (Class->FindConstant(Lex.Name)) ParseError(Lex.Location, "Redefined identifier `%s`", *Lex.Name);
          VConstant *cDef = new VConstant(Lex.Name, Class, Lex.Location);
          cDef->bitconstant = bitconst;
          cDef->Type = TYPE_Int;
          Lex.NextToken();
               if (Lex.Check(TK_Assign)) cDef->ValueExpr = ParseExpression();
          else if (PrevValue) cDef->PrevEnumValue = PrevValue;
          else cDef->ValueExpr = new VIntLiteral(0, Lex.Location);
          PrevValue = cDef;
          Class->AddConstant(cDef);
          // get comma
          if (!Lex.Check(TK_Comma)) break;
          // this can be last "orphan" comma
          if (Lex.Token == TK_RBrace) break;
        }
        Lex.Expect(TK_RBrace, ERR_MISSING_RBRACE);
      }
      while (Lex.Check(TK_Semicolon)) {}
      //Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
      continue;
    }

    if (Lex.Check(TK_Const)) {
      int Type = TYPE_Unknown;
           if (Lex.Check(TK_Int)) Type = TYPE_Int;
      else if (Lex.Check(TK_Float)) Type = TYPE_Float;
      else if (Lex.Check(TK_Name)) Type = TYPE_Name;
      else if (Lex.Check(TK_String)) Type = TYPE_String;
      else { ParseError(Lex.Location, "Bad constant type"); Lex.NextToken(); }
      do {
        if (Lex.Token != TK_Identifier) {
          ParseError(Lex.Location, "Const name expected");
          Lex.NextToken();
          continue;
        }
        if (Class->FindConstant(Lex.Name)) ParseError(Lex.Location, "Redefined identifier %s", *Lex.Name);
        VConstant *cDef = new VConstant(Lex.Name, Class, Lex.Location);
        cDef->Type = Type;
        Lex.NextToken();
        if (!Lex.Check(TK_Assign)) ParseError(Lex.Location, "Assignement operator expected");
        cDef->ValueExpr = ParseExpression();
        Class->AddConstant(cDef);
      } while (Lex.Check(TK_Comma));
      Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
      continue;
    }

    if (Lex.Check(TK_Struct)) {
      ParseStruct(Class, false);
      continue;
    }

    if (Lex.Check(TK_Vector)) {
      ParseStruct(Class, true);
      continue;
    }

    // old-style delegate syntax
    if (Lex.Check(TK_Delegate)) {
      VExpression *Type = ParseTypeWithPtrs();
      if (!Type) {
        ParseError(Lex.Location, "Field type expected");
        continue;
      }
      if (Lex.Token != TK_Identifier) {
        ParseError(Lex.Location, "Field name expected");
        continue;
      }
      VField *fi = new VField(Lex.Name, Class, Lex.Location);
      if (Class->FindField(Lex.Name) || Class->FindMethod(Lex.Name)) ParseError(Lex.Location, "Redeclared field `%s`", *Lex.Name);
      Lex.NextToken();
      Class->AddField(fi);
      ParseDelegate(Type, fi);
      Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
      continue;
    }

    if (Lex.Check(TK_Replication)) {
      ParseReplication(Class);
      continue;
    }

    if (Lex.Check(TK_Alias)) {
      if (!currClass) ParseError(Lex.Location, "cannot create aliases outside of class");
      for (;;) {
        if (Lex.Token != TK_Identifier) { ParseError(Lex.Location, "Identifier name expected"); break; }
        VName aliasName = Lex.Name;
        TLocation aliasLoc = Lex.Location;
        Lex.NextToken();
        if (!Lex.Check(TK_Assign)) { ParseError(Lex.Location, "`=` expected"); break; }
        if (Lex.Token != TK_Identifier) { ParseError(Lex.Location, "Identifier name expected"); break; }
        VName origName = Lex.Name;
        Lex.NextToken();
        auto ainfo = currClass->AliasList.get(aliasName);
        if (ainfo) {
          ParseError(Lex.Location, "alias '%s' redeclaration; previous declaration at %s:%d", *aliasName, *ainfo->loc.GetSource(), ainfo->loc.GetLine());
        } else {
          VClass::AliasInfo ai;
          ai.aliasName = aliasName;
          ai.origName = origName;
          ai.loc = aliasLoc;
          ai.aframe = currClass->AliasFrameNum;
          currClass->AliasList.put(aliasName, ai);
        }
        if (Lex.Check(TK_Semicolon)) break;
        Lex.Expect(TK_Comma, ERR_MISSING_SEMICOLON);
        if (Lex.Check(TK_Semicolon)) break; // alias a = b,; is allowed, 'cause why not?
      }
      continue;
    }

    int Modifiers = TModifiers::Parse(Lex);

    if (Lex.Check(TK_Iterator)) {
      if (Lex.Token != TK_Identifier) ParseError(Lex.Location, "Method name expected");
      VName FieldName = Lex.Name;
      TLocation FieldLoc = Lex.Location;
      Lex.NextToken();
      Lex.Expect(TK_LParen, ERR_MISSING_LPAREN);
      //!!!
      ParseMethodDef(VTypeExpr::NewTypeExpr(VFieldType(TYPE_Void).MakePointerType(), Lex.Location), FieldName, FieldLoc, Class, Modifiers, true);
      continue;
    }

    VExpression *Type = ParseType(true);
    if (!Type) {
      ParseError(Lex.Location, "Field type expected");
      Lex.NextToken();
      continue;
    }

    bool need_semicolon = true;
    bool firstField = true;
    do {
      VExpression *FieldType = Type->SyntaxCopy();
      FieldType = ParseTypePtrs(FieldType);

      // `type[size] name` declaration
      if (firstField && Lex.Check(TK_LBracket)) {
        if (Type->IsDelegateType()) ParseError(Lex.Location, "No arrays of delegates are allowed (yet)");
        firstField = false; // it is safe to reset it here
        TLocation SLoc = Lex.Location;
        VExpression *e = ParseExpression();
        Lex.Expect(TK_RBracket, ERR_MISSING_RFIGURESCOPE);

        if (Lex.Token != TK_Identifier) {
          delete e;
          ParseError(Lex.Location, "Field name expected");
          continue;
        }

        VName FieldName = Lex.Name;
        TLocation FieldLoc = Lex.Location;
        Lex.NextToken();

        if (Class->FindField(FieldName)) {
          delete e;
          ParseError(Lex.Location, "Redeclared field `%s`", *FieldName);
          continue;
        }

        FieldType = new VFixedArrayType(FieldType, e, SLoc);

        VField *fi = new VField(FieldName, Class, FieldLoc);
        fi->TypeExpr = FieldType;
        fi->Flags = TModifiers::FieldAttr(TModifiers::Check(Modifiers,
          TModifiers::Native|TModifiers::Private|TModifiers::Protected|
          TModifiers::ReadOnly|TModifiers::Transient, FieldLoc));
        Class->AddField(fi);
        //Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
        need_semicolon = true;
        break;
      }

      firstField = false; // it is safe to reset it here

      if (Lex.Token != TK_Identifier) {
        ParseError(Lex.Location, "Field name expected");
        continue;
      }
      VName FieldName = Lex.Name;
      TLocation FieldLoc = Lex.Location;
      Lex.NextToken();

      if (Class->FindField(FieldName)) {
        ParseError(Lex.Location, "Redeclared field `%s`", *FieldName);
        continue;
      }

      // property?
      if (Lex.Check(TK_LBrace)) {
        Modifiers = TModifiers::Check(Modifiers, TModifiers::Native|TModifiers::Final|TModifiers::Private|TModifiers::Protected|TModifiers::Static, FieldLoc);
        VProperty *Prop = new VProperty(FieldName, Class, FieldLoc);
        Prop->TypeExpr = FieldType;
        Prop->Flags = TModifiers::PropAttr(Modifiers);
        if ((Modifiers&(TModifiers::Static|TModifiers::Final)) == TModifiers::Static) {
          ParseError(FieldLoc, "static properties must be final for now");
        }
        do {
          if (Lex.Check(TK_Get)) {
            // `get fldname;`?
            if (Lex.Token == TK_Identifier) {
              if (Prop->GetFunc) {
                ParseError(FieldLoc, "Property already has a get method");
                ParseError(Prop->GetFunc->Loc, "Previous get method here");
              }
              if (Prop->ReadFieldName != NAME_None) {
                ParseError(FieldLoc, "Property already has a get field");
              } else {
                Prop->ReadFieldName = Lex.Name;
              }
              Lex.NextToken();
              Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
            } else {
              char TmpName[NAME_SIZE];
              sprintf(TmpName, "get_%s", *FieldName);
              VMethod *Func = new VMethod(TmpName, Class, Lex.Location);
              Func->Flags = TModifiers::MethodAttr(Modifiers);
              Func->ReturnTypeExpr = FieldType->SyntaxCopy();

              if (Modifiers&TModifiers::Native) {
                Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
                ++Package->NumBuiltins;
              } else {
                Lex.Expect(TK_LBrace, ERR_MISSING_LBRACE);
                Func->Statement = ParseCompoundStatement();
              }

              if (Prop->GetFunc) {
                ParseError(FieldLoc, "Property already has a get method");
                ParseError(Prop->GetFunc->Loc, "Previous get method here");
              } else if (Prop->ReadFieldName != NAME_None) {
                ParseError(FieldLoc, "Property already has a get field");
              }
              Prop->GetFunc = Func;
              Class->AddMethod(Func);
            }
          } else if (Lex.Check(TK_Set)) {
            // `set fldname;`?
            if (Lex.Token == TK_Identifier) {
              if (Prop->SetFunc) {
                ParseError(FieldLoc, "Property already has a set method");
                ParseError(Prop->SetFunc->Loc, "Previous set method here");
              }
              if (Prop->WriteFieldName != NAME_None) {
                ParseError(FieldLoc, "Property already has a get field");
              } else {
                Prop->WriteFieldName = Lex.Name;
              }
              Lex.NextToken();
              Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
            } else {
              char TmpName[NAME_SIZE];
              sprintf(TmpName, "set_%s", *FieldName);
              VMethod *Func = new VMethod(TmpName, Class, Lex.Location);
              Func->Flags = TModifiers::MethodAttr(Modifiers);
              Func->ReturnTypeExpr = new VTypeExprSimple(TYPE_Void, Lex.Location);

              VMethodParam &P = Func->Params[Func->NumParams];
              P.TypeExpr = FieldType->SyntaxCopy();
              P.Name = "value";
              P.Loc = Lex.Location;
              // support for `set(valname)` syntax
              if (Lex.Check(TK_LParen)) {
                if (Lex.Token != TK_Identifier) {
                  ParseError(Lex.Location, "value name expected");
                } else {
                  P.Name = Lex.Name;
                  Lex.NextToken();
                }
                Lex.Expect(TK_RParen, ERR_MISSING_RPAREN);
              }
              Func->ParamFlags[Func->NumParams] = 0;
              ++Func->NumParams;

              if (Modifiers&TModifiers::Native) {
                Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
                ++Package->NumBuiltins;
              } else {
                Lex.Expect(TK_LBrace, ERR_MISSING_LBRACE);
                Func->Statement = ParseCompoundStatement();
              }

              if (Prop->SetFunc) {
                ParseError(FieldLoc, "Property already has a set method");
                ParseError(Prop->SetFunc->Loc, "Previous set method here");
              } else if (Prop->WriteFieldName != NAME_None) {
                ParseError(FieldLoc, "Property already has a set field");
              }
              Prop->SetFunc = Func;
              Class->AddMethod(Func);
            }
          } else if (Lex.Check(TK_Default)) {
            if (Lex.Token != TK_Identifier) {
              ParseError(Lex.Location, "Default field name expected");
            } else {
              if (Prop->DefaultFieldName != NAME_None) ParseError(Lex.Location, "Property already has default field defined");
              Prop->DefaultFieldName = Lex.Name;
              Lex.NextToken();
            }
            Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
          } else {
            ParseError(Lex.Location, "Invalid declaration");
            Lex.NextToken();
          }
        } while (!Lex.Check(TK_RBrace));
        Class->AddProperty(Prop);
        need_semicolon = false;
        break;
      }

      // method?
      if (Lex.Check(TK_LParen)) {
        if (Type->IsDelegateType()) ParseError(Lex.Location, "Invalid delegate declaration");
        ParseMethodDef(FieldType, FieldName, FieldLoc, Class, Modifiers, false);
        need_semicolon = false;
        break;
      }

      VExpression *initr = nullptr; // field initializer

      // `type name[size]` declaration
      if (Lex.Check(TK_LBracket)) {
        if (Type->IsDelegateType()) ParseError(Lex.Location, "No arrays of delegates are allowed (yet)");
        TLocation SLoc = Lex.Location;
        VExpression *e = ParseExpression();
        Lex.Expect(TK_RBracket, ERR_MISSING_RFIGURESCOPE);
        FieldType = new VFixedArrayType(FieldType, e, SLoc);
      } else if (!Type->IsDelegateType() && Lex.Check(TK_Assign)) {
        // not an array, and has initialiser
        initr = ParseExpression();
      }

      VField *fi = new VField(FieldName, Class, FieldLoc);
      fi->TypeExpr = FieldType;
      fi->Flags = TModifiers::FieldAttr(TModifiers::Check(Modifiers,
        TModifiers::Native|TModifiers::Private|TModifiers::Protected|
        TModifiers::ReadOnly|TModifiers::Transient, FieldLoc));
      Class->AddField(fi);

      // new-style delegate syntax: `type delegate (args) name;`
      if (FieldType->IsDelegateType()) {
        fi->Func = ((VDelegateType *)FieldType)->CreateDelegateMethod(fi);
        fi->Type = VFieldType(TYPE_Delegate);
        fi->Type.Function = fi->Func;
        delete FieldType;
        fi->TypeExpr = nullptr;
      }

      // append initializer
      if (initr) {
        // convert to `field = value`
        initr = new VAssignment(VAssignment::Assign, new VSingleName(FieldName, initr->Loc), initr, initr->Loc);
        // convert to statement
        VStatement *st = new VExpressionStatement(initr);
        // append it to the list
        if (defallot == defcount) {
          defallot += 1024;
          defstats = (VStatement **)realloc(defstats, defallot*sizeof(VStatement *));
          if (!defstats) Sys_Error("VC: out of memory!");
        }
        defstats[defcount++] = st;
      }
    } while (Lex.Check(TK_Comma));

    delete Type;
    Type = nullptr;
    if (need_semicolon) {
      Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
      while (Lex.Check(TK_Semicolon)) {}
    }
  }

  ParseDefaultProperties(Class, !skipDefaultProperties, defcount, defstats);
  if (defstats) free(defstats);

  currClass = oldcc;

  Package->ParsedClasses.Append(Class);
  unguard;
}


//==========================================================================
//
//  VParser::Parse
//
//==========================================================================
void VParser::Parse () {
  guard(VParser::Parse);
  dprintf("Parsing\n");
  Lex.NextToken();
  bool done = false;
  while (!done) {
    switch (Lex.Token) {
      case TK_EOF:
        done = true;
        break;
      case TK_Import:
        {
          Lex.NextToken();
          if (Lex.Token != TK_NameLiteral) ParseError(Lex.Location, "Package name expected");
          VImportedPackage &I = Package->PackagesToLoad.Alloc();
          I.Name = Lex.Name;
          I.Loc = Lex.Location;
          Lex.NextToken();
          Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
          break;
        }
      case TK_Enum:
        {
          Lex.NextToken();
          VConstant *PrevValue = nullptr;
          Lex.Expect(TK_LBrace, ERR_MISSING_LBRACE);
          for (;;) {
            if (Lex.Token != TK_Identifier) ParseError(Lex.Location, "Expected IDENTIFIER");
            if (Package->FindConstant(Lex.Name)) ParseError(Lex.Location, "Redefined identifier %s", *Lex.Name);
            VConstant *cDef = new VConstant(Lex.Name, Package, Lex.Location);
            cDef->Type = TYPE_Int;
            Lex.NextToken();
                 if (Lex.Check(TK_Assign)) cDef->ValueExpr = ParseExpression();
            else if (PrevValue) cDef->PrevEnumValue = PrevValue;
            else cDef->ValueExpr = new VIntLiteral(0, Lex.Location);
            PrevValue = cDef;
            Package->ParsedConstants.Append(cDef);
            // get comma
            if (!Lex.Check(TK_Comma)) break;
            // this can be last "orphan" comma
            if (Lex.Token == TK_RBrace) break;
          }
          Lex.Expect(TK_RBrace, ERR_MISSING_RBRACE);
          while (Lex.Check(TK_Semicolon)) {}
          //Lex.Expect(TK_Semicolon, ERR_MISSING_SEMICOLON);
          break;
        }
      case TK_Class:
        Lex.NextToken();
        ParseClass();
        break;
      default:
        ParseError(Lex.Location, "Invalid token \"%s\"", VLexer::TokenNames[Lex.Token]);
        Lex.NextToken();
        break;
      }
  }
  if (vcErrorCount) BailOut();
  unguard;
}
