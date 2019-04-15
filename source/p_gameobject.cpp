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
      Host_Error("cannot find field '%s' in null object, redirected from `%s`", *fldname, *Self->GetClass()->GetFullName());
    } else {
      Host_Error("cannot find field '%s' in null object", *fldname);
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
        Host_Error("cannot index non-array uservar '%s' in object of class `%s` (index is %d)", *fldname, *obj->GetClass()->GetFullName(), index);
      } else {
        Host_Error("cannot index non-array uservar '%s' in object of class `%s` (index is %d), redirected from `%s`", *fldname, *obj->GetClass()->GetFullName(), index, *Self->GetClass()->GetFullName());
      }
    }
    if (fldtype) *fldtype = fld->Type;
    return ((vuint8 *)obj)+fld->Ofs;
  }
}


//==========================================================================
//
//  VGameObject::getRedirection
//
//==========================================================================
static VObject *getRedirection (VName fldname, VGameObject *gobj) {
  if (!gobj) {
    VObject::VMDumpCallStack();
    Host_Error("cannot redirect field '%s' in none object", *fldname);
  }
  if (gobj->GetFlags()&(_OF_Destroyed)) {
    VObject::VMDumpCallStack();
    Host_Error("cannot redirect field '%s' in dead object", *fldname);
  }
  if (!gobj->_stateRouteSelf) return gobj;
  if (gobj->_stateRouteSelf->GetFlags()&(_OF_Destroyed)) {
    VObject::VMDumpCallStack();
    Host_Error("cannot redirect field '%s' in dead object, from '%s'", *fldname, *gobj->GetClass()->GetFullName());
  }
  return gobj->_stateRouteSelf;
}


//==========================================================================
//
//  VGameObject::_get_user_var_int
//
//==========================================================================
int VGameObject::_get_user_var_int (VName fldname, int index) {
  VObject *xobj = getRedirection(fldname, this);
  VFieldType type;
  vuint8 *dptr = getFieldPtr(&type, xobj, fldname, index, this);
  switch (type.Type) {
    case TYPE_Int: return *(const vint32 *)dptr;
    case TYPE_Float: return *(const float *)dptr;
  }
  Host_Error("cannot get non-int uservar '%s'", *fldname);
  return 0;
}


//==========================================================================
//
//  VGameObject::_get_user_var_float
//
//==========================================================================
float VGameObject::_get_user_var_float (VName fldname, int index) {
  VObject *xobj = getRedirection(fldname, this);
  VFieldType type;
  vuint8 *dptr = getFieldPtr(&type, xobj, fldname, index, this);
  switch (type.Type) {
    case TYPE_Int: return *(const vint32 *)dptr;
    case TYPE_Float: return *(const float *)dptr;
  }
  Host_Error("cannot get non-float uservar '%s'", *fldname);
  return 0;
}


//==========================================================================
//
//  VGameObject::_set_user_var_int
//
//==========================================================================
void VGameObject::_set_user_var_int (VName fldname, int value, int index) {
  VObject *xobj = getRedirection(fldname, this);
  VFieldType type;
  vuint8 *dptr = getFieldPtr(&type, xobj, fldname, index, this);
  switch (type.Type) {
    case TYPE_Int: *(vint32 *)dptr = value; return;
    case TYPE_Float: *(float *)dptr = value; return;
  }
  VObject::VMDumpCallStack();
  Host_Error("cannot set non-int uservar '%s'", *fldname);
}


//==========================================================================
//
//  VGameObject::_set_user_var_float
//
//==========================================================================
void VGameObject::_set_user_var_float (VName fldname, float value, int index) {
  VObject *xobj = getRedirection(fldname, this);
  VFieldType type;
  vuint8 *dptr = getFieldPtr(&type, xobj, fldname, index, this);
  switch (type.Type) {
    case TYPE_Int: *(vint32 *)dptr = value; return;
    case TYPE_Float: *(float *)dptr = value; return;
  }
  Host_Error("cannot set non-float uservar '%s'", *fldname);
}


//==========================================================================
//
//  VGameObject::_get_user_var_type
//
//==========================================================================
VGameObject::UserVarFieldType VGameObject::_get_user_var_type (VName fldname) {
  VObject *xobj = getRedirection(fldname, this);
  VField *fld = xobj->GetClass()->FindField(fldname);
  if (!fld) return UserVarFieldType::None;
  if (fld->Type.Type == TYPE_Array) {
    if (fld->Type.IsArray2D()) return UserVarFieldType::None; // invalid
    switch (fld->Type.GetArrayInnerType().Type) {
      case TYPE_Int: return UserVarFieldType::IntArray;
      case TYPE_Float: return UserVarFieldType::FloatArray;
    }
  } else {
    switch (fld->Type.Type) {
      case TYPE_Int: return UserVarFieldType::Int;
      case TYPE_Float: return UserVarFieldType::Float;
    }
  }
  return UserVarFieldType::None; // invalid
}


//==========================================================================
//
//  VGameObject::_get_user_var_dim
//
//  array dimension; -1: not an array, or absent
//
//==========================================================================
int VGameObject::_get_user_var_dim (VName fldname) {
  VObject *xobj = getRedirection(fldname, this);
  VField *fld = xobj->GetClass()->FindField(fldname);
  if (!fld) return -1;
  if (fld->Type.Type == TYPE_Array) {
    if (fld->Type.IsArray2D()) return -1; // invalid
    int dim = fld->Type.ArrayDimInternal;
    if (dim < 0) return -1;
    return dim;
  }
  return -1; // invalid
}


// ////////////////////////////////////////////////////////////////////////// //
//native final int _get_user_var_int (name fldname, optional int index);
IMPLEMENT_FUNCTION(VGameObject, _get_user_var_int) {
  P_GET_INT_OPT(index, 0);
  P_GET_NAME(fldname);
  P_GET_SELF;
  if (!Self) { VObject::VMDumpCallStack(); Host_Error("cannot get field '%s' from null object", *fldname); }
  RET_INT(Self->_get_user_var_int(fldname, index));
}

//native final float _get_user_var_float (name fldname, optional int index);
IMPLEMENT_FUNCTION(VGameObject, _get_user_var_float) {
  P_GET_INT_OPT(index, 0);
  P_GET_NAME(fldname);
  P_GET_SELF;
  if (!Self) { VObject::VMDumpCallStack(); Host_Error("cannot get field '%s' from null object", *fldname); }
  RET_FLOAT(Self->_get_user_var_float(fldname, index));
}

//native final void _set_user_var_int (name fldname, int value, optional int index);
IMPLEMENT_FUNCTION(VGameObject, _set_user_var_int) {
  P_GET_INT_OPT(index, 0);
  P_GET_INT(value);
  P_GET_NAME(fldname);
  P_GET_SELF;
  if (!Self) { VObject::VMDumpCallStack(); Host_Error("cannot set field '%s' in null object", *fldname); }
  Self->_set_user_var_int(fldname, value, index);
}

//native final void _set_user_var_float (name fldname, float value, optional int index);
IMPLEMENT_FUNCTION(VGameObject, _set_user_var_float) {
  P_GET_INT_OPT(index, 0);
  P_GET_FLOAT(value);
  P_GET_NAME(fldname);
  P_GET_SELF;
  if (!Self) { VObject::VMDumpCallStack(); Host_Error("cannot set field '%s' in null object", *fldname); }
  Self->_set_user_var_float(fldname, value, index);
}

// native final UserVarFieldType _get_user_var_type (name fldname);
IMPLEMENT_FUNCTION(VGameObject, _get_user_var_type) {
  P_GET_NAME(fldname);
  P_GET_SELF;
  if (!Self) { VObject::VMDumpCallStack(); Host_Error("cannot check field '%s' in null object", *fldname); }
  RET_INT(Self->_get_user_var_type(fldname));
}

// native final int _get_user_var_dim (name fldname); // array dimension; -1: not an array, or absent
IMPLEMENT_FUNCTION(VGameObject, _get_user_var_dim) {
  P_GET_NAME(fldname);
  P_GET_SELF;
  if (!Self) { VObject::VMDumpCallStack(); Host_Error("cannot check field '%s' in null object", *fldname); }
  RET_INT(Self->_get_user_var_dim(fldname));
}


//native static final TVec spGetNormal (const ref TSecPlaneRef sp);
IMPLEMENT_FUNCTION(VGameObject, spGetNormal) {
  P_GET_PTR(TSecPlaneRef, sp);
  RET_VEC(sp->GetNormal());
}

//native static final float spGetNormalZ (const ref TSecPlaneRef sp);
IMPLEMENT_FUNCTION(VGameObject, spGetNormalZ) {
  P_GET_PTR(TSecPlaneRef, sp);
  RET_FLOAT(sp->GetNormalZ());
}

//native static final float spGetDist (const ref TSecPlaneRef sp);
IMPLEMENT_FUNCTION(VGameObject, spGetDist) {
  P_GET_PTR(TSecPlaneRef, sp);
  RET_FLOAT(sp->GetDist());
}

//native static final float spGetPointZ (const ref TSecPlaneRef sp, const ref TVec p);
IMPLEMENT_FUNCTION(VGameObject, spGetPointZ) {
  P_GET_PTR(TVec, point);
  P_GET_PTR(TSecPlaneRef, sp);
  RET_FLOAT(sp->GetPointZ(point->x, point->y));
}

//native static final float spDotPoint (const ref TSecPlaneRef sp, const ref TVec point);
IMPLEMENT_FUNCTION(VGameObject, spDotPoint) {
  P_GET_PTR(TVec, point);
  P_GET_PTR(TSecPlaneRef, sp);
  RET_FLOAT(sp->DotPoint(*point));
}

//native static final float spDotPointDist (const ref TSecPlaneRef sp, const ref TVec point);
IMPLEMENT_FUNCTION(VGameObject, spDotPointDist) {
  P_GET_PTR(TVec, point);
  P_GET_PTR(TSecPlaneRef, sp);
  RET_FLOAT(sp->DotPointDist(*point));
}

//native static final int spPointOnSide (const ref TSecPlaneRef sp, const ref TVec point);
IMPLEMENT_FUNCTION(VGameObject, spPointOnSide) {
  P_GET_PTR(TVec, point);
  P_GET_PTR(TSecPlaneRef, sp);
  RET_INT(sp->PointOnSide(*point));
}

//native static final int spPointOnSideThreshold (const ref TSecPlaneRef sp, const ref TVec point);
IMPLEMENT_FUNCTION(VGameObject, spPointOnSideThreshold) {
  P_GET_PTR(TVec, point);
  P_GET_PTR(TSecPlaneRef, sp);
  RET_INT(sp->PointOnSideThreshold(*point));
}

//native static final int spPointOnSideFri (const ref TSecPlaneRef sp, const ref TVec point);
IMPLEMENT_FUNCTION(VGameObject, spPointOnSideFri) {
  P_GET_PTR(TVec, point);
  P_GET_PTR(TSecPlaneRef, sp);
  RET_INT(sp->PointOnSideFri(*point));
}

//native static final int spPointOnSide2 (const ref TSecPlaneRef sp, const ref TVec point);
IMPLEMENT_FUNCTION(VGameObject, spPointOnSide2) {
  P_GET_PTR(TVec, point);
  P_GET_PTR(TSecPlaneRef, sp);
  RET_INT(sp->PointOnSide2(*point));
}

//native static final int spSphereOnSide (const ref TSecPlaneRef sp, const ref TVec center, float radius);
IMPLEMENT_FUNCTION(VGameObject, SphereOnSide) {
  P_GET_FLOAT(radius);
  P_GET_PTR(TVec, center);
  P_GET_PTR(TSecPlaneRef, sp);
  RET_INT(sp->SphereOnSide(*center, radius));
}

//native static final bool spSphereTouches (const ref TSecPlaneRef sp, const ref TVec center, float radius);
IMPLEMENT_FUNCTION(VGameObject, spSphereTouches) {
  P_GET_FLOAT(radius);
  P_GET_PTR(TVec, center);
  P_GET_PTR(TSecPlaneRef, sp);
  RET_BOOL(sp->SphereTouches(*center, radius));
}

//native static final int spSphereOnSide2 (const ref TSecPlaneRef sp, const ref TVec center, float radius);
IMPLEMENT_FUNCTION(VGameObject, spSphereOnSide2) {
  P_GET_FLOAT(radius);
  P_GET_PTR(TVec, center);
  P_GET_PTR(TSecPlaneRef, sp);
  RET_INT(sp->SphereOnSide2(*center, radius));
}
