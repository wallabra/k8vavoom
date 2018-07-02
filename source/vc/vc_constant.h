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

class VConstant : public VMemberBase {
public:
  // persistent fields
  vuint8 Type;
  bool bitconstant; // used in `Define`
  union {
    vint32 Value;
    float FloatValue;
  };

  // compiler fields
  VExpression *ValueExpr;
  VConstant *PrevEnumValue;

  VConstant (VName AName, VMemberBase *AOuter, const TLocation &ALoc);
  VConstant (VName AEnumName, VName AName, VMemberBase *AOuter, const TLocation &ALoc);
  virtual ~VConstant () override;
  virtual void CompilerShutdown () override;

  virtual void Serialise (VStream &) override;
  bool Define ();

  virtual VStr toString () const;

  friend inline VStream &operator << (VStream &Strm, VConstant *&Obj) { return Strm << *(VMemberBase **)&Obj; }
};
