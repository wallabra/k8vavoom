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
#include "gamedefs.h"

//TODO: use hashtable instead?
static TArray<QSValue> qslist;
QSPhase qsPhase = QSPhase::QSP_None;


void QSValue::Serialise (VStream &strm) {
  vuint8 atype = type;
  strm << atype;
  if (strm.IsLoading()) type = (QSType)atype;
  strm << name;

  switch (type) {
    case QSType::QST_Void:
      break;
    case QSType::QST_Int:
      strm << STRM_INDEX(ival);
      break;
    case QSType::QST_Name:
      if (strm.IsLoading()) {
        VStr nv;
        strm << nv;
        if (nv.length()) nval = VName(*nv); else nval = NAME_None;
      } else {
        VStr nv = *nval;
        strm << nv;
      }
      break;
    case QSType::QST_Str:
      strm << sval;
      break;
    case QSType::QST_Float:
      strm << fval;
      break;
    default:
      Host_Error("invalid qsvalue type");
  }

  strm << STRM_INDEX(objidx);
}


VStr QSValue::toString () const {
  VStr res = name+":";
  switch (type) {
    case QSType::QST_Void:
      res += "(void)";
      break;
    case QSType::QST_Int:
      res += "(int)";
      res += VStr(ival);
      break;
    case QSType::QST_Name:
      res += "(name)";
      res += VStr(*nval);
      break;
    case QSType::QST_Str:
      res += "(str)";
      res += sval.quote(true);
      break;
    case QSType::QST_Float:
      res += "(float)";
      res += VStr(fval);
      break;
    default:
      res += "<invalid>";
      break;
  }
  return res;
}



TArray<QSValue> QS_GetCurrentArray () {
  return qslist;
}


void QS_StartPhase (QSPhase aphase) {
  qslist.reset();
  qsPhase = aphase;
}


static int findValue (VEntity *ent, const VStr &name) {
  const int len = qslist.length();
  for (int f = 0; f < len; ++f) {
    if (qslist[f].ent == ent && qslist[f].name.Cmp(name) == 0) return f;
  }
  return -1;
}


void QS_EnterValue (const QSValue &val) {
  if (qsPhase != QSPhase::QSP_Load) Host_Error("cannot use CheckPoint API outside of checkpoint handlers");
  int idx = findValue(val.ent, val.name);
  if (idx >= 0) {
    qslist[idx] = val;
  } else {
    qslist.append(val);
  }
}


void QS_PutValue (const QSValue &val) {
  if (qsPhase != QSPhase::QSP_Save) Host_Error("cannot use CheckPoint API outside of checkpoint handlers");
  int idx = findValue(val.ent, val.name);
  if (idx >= 0) {
    qslist[idx] = val;
  } else {
    qslist.append(val);
  }
}


QSValue QS_GetValue (VEntity *ent, const VStr &name) {
  if (qsPhase != QSPhase::QSP_Load) Host_Error("cannot use CheckPoint API outside of checkpoint handlers");
  int idx = findValue(ent, name);
  if (idx < 0) return QSValue();
  return qslist[idx];
}
