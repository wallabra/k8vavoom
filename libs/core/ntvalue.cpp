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
#include "core.h"


//==========================================================================
//
//  VNTValue::VNTValue
//
//==========================================================================
VNTValue::VNTValue (VName aname, const vuint8 *buf, int bufsz, bool doCopyData) {
  check(bufsz >= 0);
  memset((void *)this, 0, sizeof(VNTValue));
  type = T_Blob;
  name = aname;
  blobSize = bufsz;
  if (doCopyData || (bufsz > 0 && !buf)) {
    vuint32 *mem = (vuint32 *)Z_Malloc(bufsz+sizeof(vuint32));
    blob = (vuint8 *)(mem+1);
    *mem = 1;
    if (bufsz) {
      if (buf) memcpy(blob, buf, bufsz); else memset(blob, 0, bufsz);
    }
  } else {
    blob = (vuint8 *)buf;
  }
}


//==========================================================================
//
//  VNTValue::Serialise
//
//==========================================================================
void VNTValue::Serialise (VStream &strm) {
  if (strm.IsLoading()) clear();

  strm << type;
  if (strm.IsLoading()) {
    if (!isValid()) Sys_Error("invalid NTValue type (%d)", type);
  }
  strm << name;

  switch (type) {
    case T_Int: strm << STRM_INDEX(ud.ival); break;
    case T_Float: strm << ud.fval; break;
    case T_Vec: strm << vval.x << vval.y << vval.z; break;
    case T_Name: strm << nval; break;
    case T_Str: strm << sval; break;
    case T_Class: strm << ud.cval; break;
    case T_Obj: strm << ud.oval; break;
    case T_XObj: strm << ud.xoval; break;
    case T_Blob:
      if (strm.IsLoading()) {
        // reading
        vint32 len = -1;
        strm << STRM_INDEX(len);
        if (len < 0 || len > 1024*1024*64) Sys_Error("invalid blob size (%d)", len);
        if (len > 0) {
          vuint32 *mem = (vuint32 *)Z_Malloc(len+sizeof(vuint32));
          blob = (vuint8 *)(mem+1);
          blobSize = len;
          *mem = 1;
          if (len) strm.Serialise(blob, len);
        } else {
          blob = nullptr;
          blobSize = 0;
        }
      } else {
        // writing
        vint32 len = blobSize;
        if (len < 0 || len > 1024*1024*64) Sys_Error("invalid blob size (%d)", len);
        strm << STRM_INDEX(len);
        if (len) strm.Serialise(blob, len);
      }
      break;
  }
}


//==========================================================================
//
//  VNTValue::SkipSerialised
//
//==========================================================================
void VNTValue::SkipSerialised (VStream &strm) {
  check(strm.IsLoading());
  vuint8 atype = T_Invalid;
  strm << atype;
  if (atype <= T_Invalid || atype > T_Blob) Sys_Error("invalid NTValue type (%d)", atype);
  switch (atype) {
    case T_Int: { vint32 v; strm << STRM_INDEX(v); } break;
    case T_Float: { float v; strm << v; } break;
    case T_Vec: { float x, y, z; strm << x << y << z; } break;
    case T_Name: { VName v; strm << v; } break;
    case T_Str: { VStr v; strm << v; } break;
    case T_Class: { VMemberBase *v; strm << v; } break;
    case T_Obj: { VObject *v; strm << v; } break;
    case T_XObj: { VSerialisable *v; strm << v; } break;
    case T_Blob:
      {
        vint32 len = -1;
        strm << STRM_INDEX(len);
        if (len < 0 || len > 1024*1024*64) Sys_Error("invalid blob size (%d)", len);
        if (strm.IsError()) return;
        if (len) strm.Seek(strm.Tell()+len);
      }
      break;
  }
}
