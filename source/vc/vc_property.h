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


// ////////////////////////////////////////////////////////////////////////// //
// property flags
enum {
  PROP_Native    = 0x0001, // Native get and set methods
  PROP_Final     = 0x0002, // Final version of a proeprty
  PROP_Private   = 0x0004,
  PROP_Protected = 0x0008,
};


// ////////////////////////////////////////////////////////////////////////// //
class VProperty : public VMemberBase {
public:
  // persistent fields
  VFieldType Type;
  VMethod *GetFunc;
  VMethod *SetFunc;
  VField *DefaultField;
  VField *ReadField; // !null: property read will use this field
  VField *WriteField; // !null: property write will use this field
  vuint32 Flags;

  // compiler fields
  VExpression *TypeExpr;
  VName DefaultFieldName;
  VName ReadFieldName;
  VName WriteFieldName;

  VProperty (VName, VMemberBase *, TLocation);
  virtual ~VProperty () override;

  virtual void Serialise (VStream &) override;
  bool Define();

  friend inline VStream &operator << (VStream &Strm, VProperty *&Obj) { return Strm << *(VMemberBase **)&Obj; }
};
