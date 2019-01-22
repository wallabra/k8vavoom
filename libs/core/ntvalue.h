//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2019 Ketmar Dark
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
//**
//**  typed and named field
//**
//**************************************************************************


struct VNTValue {
public:
  enum {
    T_Invalid, // zero, so we can `memset(0)` this
    T_Int,
    T_Float,
    T_Vec, // 3 floats
    T_Name,
    T_Str,
    T_Class,
    T_Obj, // VObject
    T_XObj, // VSerialisable
    T_Blob, // byte array
  };

public:
  vuint8 type; // T_XXX
  // some values are moved out of union for correct finalisation
  union {
    vint32 ival;
    vuint32 uval;
    float fval;
    float vval[3]; // vector
    VName nval;
    VClass *cval;
    VObject *oval;
    VSerialisable *xoval;
  };
  VStr sval;
  vuint8 *blob;
  vint32 blobSize;

public:
  VNTValue () : type(T_Invalid), sval(), blob(nullptr), blobSize(0) { memset((void *)this, 0, sizeof(VNTValue)); }
};
