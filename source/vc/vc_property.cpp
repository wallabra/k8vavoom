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
//  VProperty::VProperty
//
//==========================================================================
VProperty::VProperty (VName AName, VMemberBase *AOuter, TLocation ALoc)
  : VMemberBase(MEMBER_Property, AName, AOuter, ALoc)
  , Type(TYPE_Void)
  , GetFunc(nullptr)
  , SetFunc(nullptr)
  , DefaultField(nullptr)
  , ReadField(nullptr)
  , WriteField(nullptr)
  , Flags(0)
  , TypeExpr(nullptr)
  , DefaultFieldName(NAME_None)
  , ReadFieldName(NAME_None)
  , WriteFieldName(NAME_None)
{
}


//==========================================================================
//
//  VProperty::~VProperty
//
//==========================================================================
VProperty::~VProperty () {
  delete TypeExpr; TypeExpr = nullptr;
}


//==========================================================================
//
//  VProperty::CompilerShutdown
//
//==========================================================================
void VProperty::CompilerShutdown () {
  VMemberBase::CompilerShutdown();
  delete TypeExpr; TypeExpr = nullptr;
}


//==========================================================================
//
//  VProperty::Serialise
//
//==========================================================================
void VProperty::Serialise (VStream &Strm) {
  guard(VProperty::Serialise);
  VMemberBase::Serialise(Strm);
  vuint8 xver = 0; // current version is 0
  Strm << xver;
  Strm << Type << GetFunc << SetFunc << DefaultField << ReadField << WriteField << Flags;
  unguard;
}


//==========================================================================
//
//  VProperty::Define
//
//==========================================================================
bool VProperty::Define () {
  if (TypeExpr) {
    VEmitContext ec(this);
    TypeExpr = TypeExpr->ResolveAsType(ec);
  }
  if (!TypeExpr) return false;

  if (TypeExpr->Type.Type == TYPE_Void) {
    ParseError(TypeExpr->Loc, "Property cannot have `void` type");
    return false;
  }
  Type = TypeExpr->Type;

  if (DefaultFieldName != NAME_None) {
    DefaultField = ((VClass *)Outer)->FindField(DefaultFieldName, Loc, (VClass *)Outer);
    if (!DefaultField) {
      ParseError(Loc, "No such field `%s`", *DefaultFieldName);
      return false;
    }
  }

  if (ReadFieldName != NAME_None) {
    ReadField = ((VClass *)Outer)->FindField(ReadFieldName, Loc, (VClass *)Outer);
    if (!ReadField) {
      ParseError(Loc, "No such field `%s`", *ReadFieldName);
      return false;
    }
  }

  if (WriteFieldName != NAME_None) {
    WriteField = ((VClass *)Outer)->FindField(WriteFieldName, Loc, (VClass *)Outer);
    if (!WriteField) {
      ParseError(Loc, "No such field `%s`", *WriteFieldName);
      return false;
    }
  }

  VProperty *BaseProp = nullptr;
  if (((VClass *)Outer)->ParentClass) BaseProp = ((VClass*)Outer)->ParentClass->FindProperty(Name);
  if (BaseProp) {
    if (BaseProp->Flags&PROP_Final) ParseError(Loc, "Property alaready has been declared final and cannot be overridden");
    if (!Type.Equals(BaseProp->Type)) ParseError(Loc, "Property redeclared with a different type");
  }

  return true;
}
