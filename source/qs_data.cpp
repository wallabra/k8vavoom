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


// ////////////////////////////////////////////////////////////////////////// //
struct QLItem {
  QSValue value;
  int next; // next for this entity, -1 means "no more"
};
static TArray<QLItem> qslist;
static TMapNC<VEntity *, int> qsmap; // value is index in `qslist`


QSPhase qsPhase = QSPhase::QSP_None;


//==========================================================================
//
//  QSValue::Serialise
//
//==========================================================================
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


//==========================================================================
//
//  QSValue::toString
//
//==========================================================================
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



// ////////////////////////////////////////////////////////////////////////// //
TArray<QSValue> QS_GetCurrentArray () {
  TArray<QSValue> res;
  for (auto it = qsmap.first(); it; ++it) {
    int idx = it.getValue();
    while (idx >= 0) {
      res.append(qslist[idx].value);
      idx = qslist[idx].next;
    }
  }
  return res;
}


void QS_StartPhase (QSPhase aphase) {
  qslist.reset();
  qsmap.reset();
  qsPhase = aphase;
}


static void putValue (const QSValue &val) {
  auto xptr = qsmap.find(val.ent);
  if (!xptr) {
    int idx = qslist.length();
    QLItem &li = qslist.alloc();
    li.value = val;
    li.next = -1;
    qsmap.put(val.ent, idx);
  } else {
    int idx = *xptr, prev = -1;
    while (idx >= 0) {
      check(qslist[idx].value.ent == val.ent);
      if (qslist[idx].value.name.Cmp(val.name) == 0) {
        qslist[idx].value = val;
        return;
      }
      prev = idx;
      idx = qslist[idx].next;
    }
    check(prev != -1);
    int nidx = qslist.length();
    QLItem &li = qslist.alloc();
    li.value = val;
    li.next = -1;
    check(qslist[prev].next == -1);
    qslist[prev].next = nidx;
  }
}


void QS_EnterValue (const QSValue &val) {
  if (qsPhase != QSPhase::QSP_Load) Host_Error("cannot use CheckPoint API outside of checkpoint handlers");
  putValue(val);
}


void QS_PutValue (const QSValue &val) {
  if (qsPhase != QSPhase::QSP_Save) Host_Error("cannot use CheckPoint API outside of checkpoint handlers");
  putValue(val);
}


QSValue QS_GetValue (VEntity *ent, const VStr &name) {
  if (qsPhase != QSPhase::QSP_Load) Host_Error("cannot use CheckPoint API outside of checkpoint handlers");
  auto xptr = qsmap.find(ent);
  if (!xptr) return QSValue();
  for (int idx = *xptr; idx >= 0; idx = qslist[idx].next) {
    check(qslist[idx].value.ent == ent);
    if (qslist[idx].value.name.Cmp(name) == 0) return qslist[idx].value;
  }
  return QSValue();
}
