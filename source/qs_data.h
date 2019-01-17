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
// quicksave data

class VEntity;


enum QSPhase {
  QSP_None,
  QSP_Load,
  QSP_Save,
};

extern QSPhase qsPhase;


enum QSType {
  QST_Void,
  QST_Int,
  QST_Name,
  QST_Str,
  QST_Float,
};

struct QSValue {
  QSType type;
  VEntity *ent; // owner (nullptr: player)
  VStr name; // value name
  // not a union
  vint32 ival;
  VName nval;
  VStr sval;
  float fval;
  // used to map entities; 0 is player, 1.. -- entities
  vint32 objidx;

  QSValue ()
    : type(QSType::QST_Void)
    , ent(nullptr)
    , name()
    , ival(0)
    , nval(NAME_None)
    , sval()
    , fval(0)
    , objidx(0)
  {}

  QSValue (VEntity *aent, const VStr &aname, QSType atype)
    : type(atype)
    , ent(aent)
    , name(aname)
    , ival(0)
    , nval(NAME_None)
    , sval()
    , fval(0)
    , objidx(0)
  {}

  static inline QSValue CreateInt (VEntity *ent, const VStr &name, vint32 value) { QSValue aval(ent, name, QSType::QST_Int); aval.ival = value; return aval; }
  static inline QSValue CreateName (VEntity *ent, const VStr &name, VName value) { QSValue aval(ent, name, QSType::QST_Name); aval.nval = value; return aval; }
  static inline QSValue CreateStr (VEntity *ent, const VStr &name, const VStr &value) { QSValue aval(ent, name, QSType::QST_Str); aval.sval = value; return aval; }
  static inline QSValue CreateFloat (VEntity *ent, const VStr &name, float value) { QSValue aval(ent, name, QSType::QST_Float); aval.fval = value; return aval; }

  inline bool isEmpty () const { return (type == QSType::QST_Void); }

  void Serialise (VStream &strm);

  VStr toString () const;
};


extern TArray<QSValue> QS_GetCurrentArray ();

extern void QS_StartPhase (QSPhase aphase);
extern void QS_PutValue (const QSValue &val);
extern QSValue QS_GetValue (VEntity *ent, const VStr &name);

// this is used in loader to build qs data
extern void QS_EnterValue (const QSValue &val);
