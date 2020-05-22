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
//**  Copyright (C) 2018-2020 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, version 3 of the License ONLY.
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

// ////////////////////////////////////////////////////////////////////////// //
// constant flags
enum {
  CONST_Decorate = 0x0100, // visible from decorate
};


// ////////////////////////////////////////////////////////////////////////// //
class VConstant : public VMemberBase {
private:
  bool alreadyDefined;

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

  vuint32 Flags;

public:
  VConstant (VName AName, VMemberBase *AOuter, const TLocation &ALoc);
  VConstant (VName AEnumName, VName AName, VMemberBase *AOuter, const TLocation &ALoc);
  virtual ~VConstant () override;
  virtual void CompilerShutdown () override;

  bool Define ();

  virtual VStr toString () const;

  friend inline VStream &operator << (VStream &Strm, VConstant *&Obj) { return Strm << *(VMemberBase **)&Obj; }
};
