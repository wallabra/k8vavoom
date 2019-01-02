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
/*
// native final bool HasFieldByName (name fldname);
IMPLEMENT_FUNCTION(VObject, HasFieldByName) {
  P_GET_NAME(name);
  P_GET_SELF;
  if (!Self || name == NAME_None) { RET_BOOL(false); return; }
  VField *fld = Self->Class->FindField(name);
  RET_BOOL(!!fld);
}


// native final int GetIntFieldByName (name fldname, optional bool defval);
IMPLEMENT_FUNCTION(VObject, GetIntFieldByName) {
  P_GET_BOOL_OPT(defval, false);
  P_GET_NAME(name);
  P_GET_SELF;
  if (!Self || name == NAME_None) { RET_INT(0); return; }
  //VField *fld = Self->Class->FindFieldChecked(name);
  VField *fld = Self->Class->FindField(name);
  if (!fld) Sys_Error("field '%s' not found in class '%s'", *name, Self->Class->GetName());
  if (fld->Type.Type != TYPE_Int) Sys_Error("field '%s' is not int (it is `%s`)", *name, *fld->Type.GetName());
  RET_INT(*(const vint32 *)((defval ? Self->Class->Defaults : (const vuint8 *)Self)+fld->Ofs));
}
*/
