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
//  VConstant::VConstant
//
//==========================================================================
VConstant::VConstant (VName AName, VMemberBase *AOuter, const TLocation &ALoc)
  : VMemberBase(MEMBER_Const, AName, AOuter, ALoc)
  , alreadyDefined(false)
  , Type(TYPE_Unknown)
  , bitconstant(false)
  , Value(0)
  , ValueExpr(nullptr)
  , PrevEnumValue(nullptr)
{
}


//==========================================================================
//
//  VConstant::VConstant
//
//==========================================================================
VConstant::VConstant (VName AEnumName, VName AName, VMemberBase *AOuter, const TLocation &ALoc)
  : VMemberBase(MEMBER_Const, VName(*(VStr(*AEnumName)+" "+(*AName))), AOuter, ALoc)
  , alreadyDefined(false)
  , Type(TYPE_Unknown)
  , bitconstant(false)
  , Value(0)
  , ValueExpr(nullptr)
  , PrevEnumValue(nullptr)
{
}


//==========================================================================
//
//  VConstant::~VConstant
//
//==========================================================================
VConstant::~VConstant () {
  delete ValueExpr; ValueExpr = nullptr;
}


//==========================================================================
//
//  VConstant::CompilerShutdown
//
//==========================================================================
void VConstant::CompilerShutdown () {
  VMemberBase::CompilerShutdown();
  delete ValueExpr; ValueExpr = nullptr;
}


//==========================================================================
//
//  VConstant::Serialise
//
//==========================================================================
void VConstant::Serialise (VStream &Strm) {
  guard(VConstant::Serialise);
  VMemberBase::Serialise(Strm);
  Strm << Type;
  switch (Type) {
    case TYPE_Float: Strm << FloatValue; break;
    case TYPE_Name: Strm << *(VName *)&Value; break;
    default: Strm << STRM_INDEX(Value); break;
  }
  unguard;
}


//==========================================================================
//
//  VConstant::Define
//
//==========================================================================
bool VConstant::Define () {
  guard(VConstant::Define);

  if (alreadyDefined) return true;
  alreadyDefined = true;

  if (PrevEnumValue) {
    if (bitconstant) {
      if (PrevEnumValue->Value == 0) {
        Value = 1;
      } else {
        Value = PrevEnumValue->Value<<1;
      }
    } else {
      Value = PrevEnumValue->Value+1;
    }
    return true;
  }

  if (ValueExpr) {
    if (ValueExpr->IsNoneLiteral()) {
      if (!bitconstant) ParseError(ValueExpr->Loc, "`none` is allowed only for bitenums");
      Value = 0;
      Type = TYPE_Int;
      return true;
    } else {
      VEmitContext ec(this);
      ValueExpr = ValueExpr->Resolve(ec);
    }
  }
  if (!ValueExpr) return false;

  switch (Type) {
    case TYPE_Int:
      if (!ValueExpr->IsIntConst()) {
        ParseError(ValueExpr->Loc, "Integer constant expected");
        return false;
      }
      Value = ValueExpr->GetIntConst();
      if (bitconstant) Value = 1<<Value;
      break;
    case TYPE_Float:
      if (!ValueExpr->IsFloatConst()) {
        ParseError(ValueExpr->Loc, "Float constant expected");
        return false;
      }
      if (bitconstant) ParseError(ValueExpr->Loc, "Integer constant expected");
      FloatValue = ValueExpr->GetFloatConst();
      break;
    default:
      ParseError(Loc, "Unsupported type of constant");
      return false;
  }
  return true;
  unguard;
}


//==========================================================================
//
//  VConstant::Define
//
//==========================================================================
VStr VConstant::toString () const {
  switch (Type) {
    case TYPE_Int: return VStr(Value);
    case TYPE_Float: return VStr(FloatValue);
  }
  return VStr("<{unknown-const}>");
}
