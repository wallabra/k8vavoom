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
#include "gamedefs.h"


IMPLEMENT_CLASS(V, GameObject)


//==========================================================================
//
//  getFieldPtr
//
//==========================================================================
static vuint8 *getFieldPtr (VFieldType *fldtype, VObject *obj, VName fldname, int index, VObject *Self) {
  if (!obj) {
    VObject::VMDumpCallStack();
    if (Self) {
      Sys_Error("cannot find field '%s' in null object, redirected from `%s`", *fldname, *Self->GetClass()->GetFullName());
    } else {
      Sys_Error("cannot find field '%s' in null object", *fldname);
    }
  }
  VField *fld = obj->GetClass()->FindField(fldname);
  if (!fld) {
    VObject::VMDumpCallStack();
    if (Self == obj) {
      Host_Error("uservar '%s' not found in object of class `%s`", *fldname, *obj->GetClass()->GetFullName());
    } else {
      Host_Error("uservar '%s' not found in object of class `%s`, redirected from `%s`", *fldname, *obj->GetClass()->GetFullName(), *Self->GetClass()->GetFullName());
    }
  }
  if (fld->Type.Type == TYPE_Array) {
    if (index < 0 || fld->Type.ArrayDimInternal < 0 || index >= fld->Type.ArrayDimInternal) {
      VObject::VMDumpCallStack();
      if (Self == obj) {
        Host_Error("uservar '%s' array index out of bounds (%d) in object of class `%s`", *fldname, index, *obj->GetClass()->GetFullName());
      } else {
        Host_Error("uservar '%s' array index out of bounds (%d) in object of class `%s`, redirected from `%s`", *fldname, index, *obj->GetClass()->GetFullName(), *Self->GetClass()->GetFullName());
      }
    }
    VFieldType itt = fld->Type.GetArrayInnerType();
    if (fldtype) *fldtype = itt;
    return ((vuint8 *)obj)+fld->Ofs+itt.GetSize()*index;
  } else {
    if (index != 0) {
      VObject::VMDumpCallStack();
      if (Self == obj) {
        Sys_Error("cannot index non-array uservar '%s' in object of class `%s` (index is %d)", *fldname, *obj->GetClass()->GetFullName(), index);
      } else {
        Sys_Error("cannot index non-array uservar '%s' in object of class `%s` (index is %d), redirected from `%s`", *fldname, *obj->GetClass()->GetFullName(), index, *Self->GetClass()->GetFullName());
      }
    }
    if (fldtype) *fldtype = fld->Type;
    return ((vuint8 *)obj)+fld->Ofs;
  }
}


//==========================================================================
//
//  VGameObject::_get_user_var_int
//
//==========================================================================
int VGameObject::_get_user_var_int (VName fldname, int index) {
  VFieldType type;
  vuint8 *dptr = getFieldPtr(&type, (_stateRouteSelf ? _stateRouteSelf : this), fldname, index, this);
  switch (type.Type) {
    case TYPE_Int: return *(const vint32 *)dptr;
    case TYPE_Float: return *(const float *)dptr;
  }
  Sys_Error("cannot get non-int uservar '%s'", *fldname);
  return 0;
}


//==========================================================================
//
//  VGameObject::_get_user_var_float
//
//==========================================================================
float VGameObject::_get_user_var_float (VName fldname, int index) {
  VFieldType type;
  vuint8 *dptr = getFieldPtr(&type, (_stateRouteSelf ? _stateRouteSelf : this), fldname, index, this);
  switch (type.Type) {
    case TYPE_Int: return *(const vint32 *)dptr;
    case TYPE_Float: return *(const float *)dptr;
  }
  Sys_Error("cannot get non-float uservar '%s'", *fldname);
  return 0;
}


//==========================================================================
//
//  VGameObject::_set_user_var_int
//
//==========================================================================
void VGameObject::_set_user_var_int (VName fldname, int value, int index) {
  VFieldType type;
  vuint8 *dptr = getFieldPtr(&type, (_stateRouteSelf ? _stateRouteSelf : this), fldname, index, this);
  switch (type.Type) {
    case TYPE_Int: *(vint32 *)dptr = value; return;
    case TYPE_Float: *(float *)dptr = value; return;
  }
  Sys_Error("cannot set non-int uservar '%s'", *fldname);
}


//==========================================================================
//
//  VGameObject::_set_user_var_float
//
//==========================================================================
void VGameObject::_set_user_var_float (VName fldname, float value, int index) {
  VFieldType type;
  vuint8 *dptr = getFieldPtr(&type, (_stateRouteSelf ? _stateRouteSelf : this), fldname, index, this);
  switch (type.Type) {
    case TYPE_Int: *(vint32 *)dptr = value; return;
    case TYPE_Float: *(float *)dptr = value; return;
  }
  Sys_Error("cannot set non-float uservar '%s'", *fldname);
}


// ////////////////////////////////////////////////////////////////////////// //
//native final int _get_user_var_int (name fldname, optional int index);
IMPLEMENT_FUNCTION(VGameObject, _get_user_var_int) {
  P_GET_INT_OPT(index, 0);
  P_GET_NAME(fldname);
  P_GET_SELF;
  if (!Self) Sys_Error("cannot get field '%s' from null object", *fldname);
  RET_INT(Self->_get_user_var_int(fldname, index));
}

//native final float _get_user_var_float (name fldname, optional int index);
IMPLEMENT_FUNCTION(VGameObject, _get_user_var_float) {
  P_GET_INT_OPT(index, 0);
  P_GET_NAME(fldname);
  P_GET_SELF;
  if (!Self) Sys_Error("cannot get field '%s' from null object", *fldname);
  RET_FLOAT(Self->_get_user_var_float(fldname, index));
}

//native final void _set_user_var_int (name fldname, int value, optional int index);
IMPLEMENT_FUNCTION(VGameObject, _set_user_var_int) {
  P_GET_INT_OPT(index, 0);
  P_GET_INT(value);
  P_GET_NAME(fldname);
  P_GET_SELF;
  if (!Self) Sys_Error("cannot set field '%s' in null object", *fldname);
  Self->_set_user_var_int(fldname, value, index);
}

//native final void _set_user_var_float (name fldname, float value, optional int index);
IMPLEMENT_FUNCTION(VGameObject, _set_user_var_float) {
  P_GET_INT_OPT(index, 0);
  P_GET_FLOAT(value);
  P_GET_NAME(fldname);
  P_GET_SELF;
  if (!Self) Sys_Error("cannot set field '%s' in null object", *fldname);
  Self->_set_user_var_float(fldname, value, index);
}
