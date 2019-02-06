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

//#define VNTVALUE_DEBUG_DUMP


//==========================================================================
//
//  operator <<
//
//==========================================================================
VStream &operator << (VStream &strm, const VNTValue &val) {
  val.WriteTo(strm);
  return strm;
}


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
//  VNTValue::ReadTypeName
//
//==========================================================================
bool VNTValue::ReadTypeName (VStream &strm, vuint8 *otype, VName *oname) {
  check(strm.IsLoading());
  vuint8 atype = T_Invalid;
  strm << atype;
  if (atype <= T_Invalid || atype > T_Blob) {
    if (otype) *otype = T_Invalid;
    if (oname) *oname = NAME_None;
    return false;
  }
  VName aname;
  strm << aname;
  if (otype) *otype = atype;
  if (oname) *oname = aname;
  return !strm.IsError();
}


//==========================================================================
//
//  VNTValue::SkipValue
//
//  call this after `ReadTypeName()`
//
//==========================================================================
bool VNTValue::SkipValue (VStream &strm, vuint8 atype) {
  check(strm.IsLoading());
  if (!isValidType(atype)) return false;
  if (strm.IsError()) return false;
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
        if (strm.IsError()) return false;
        if (len) strm.Seek(strm.Tell()+len);
      }
      break;
  }
  return !strm.IsError();
}


//==========================================================================
//
//  VNTValue::ReadValue
//
//  call this after `ReadTypeName()`
//
//==========================================================================
VNTValue VNTValue::ReadValue (VStream &strm, vuint8 atype, VName aname) {
  check(strm.IsLoading());
  if (!isValidType(atype)) return VNTValue();
  if (strm.IsError()) return VNTValue();
  VNTValue res;
  res.type = atype;
  res.name = aname;
  switch (atype) {
    case T_Int: strm << STRM_INDEX(res.ud.ival); break;
    case T_Float: strm << res.ud.fval; break;
    case T_Vec: strm << res.vval.x << res.vval.y << res.vval.z; break;
    case T_Name: strm << res.nval; break;
    case T_Str: strm << res.sval; break;
    case T_Class: strm << res.ud.cval; break;
    case T_Obj: strm << res.ud.oval; break;
    case T_XObj: strm << res.ud.xoval; break;
    case T_Blob:
      {
        // reading
        vint32 len = -1;
        strm << STRM_INDEX(len);
        if (len < 0 || len > 1024*1024*64) Sys_Error("invalid blob size (%d)", len);
        if (len > 0) {
          vuint32 *mem = (vuint32 *)Z_Malloc(len+sizeof(vuint32));
          res.blob = (vuint8 *)(mem+1);
          res.blobSize = len;
          *mem = 1;
          if (len) strm.Serialise(res.blob, len);
        } else {
          res.blob = nullptr;
          res.blobSize = 0;
        }
      }
      break;
  }
  if (strm.IsError()) return VNTValue();
  return res;
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
//  VNTValue::WriteTo
//
//==========================================================================
void VNTValue::WriteTo (VStream &strm) const {
  check(!strm.IsLoading());
  vuint8 atype = type;
  strm << atype;
  VName aname = name;
  strm << aname;
  switch (type) {
    case T_Int: { vint32 v = ud.ival; strm << STRM_INDEX(v); } break;
    case T_Float: { float f = ud.fval; strm << f; } break;
    case T_Vec: { TVec v = vval; strm << v.x << v.y << v.z; } break;
    case T_Name: { aname = nval; strm << aname; } break;
    case T_Str: { VStr s = sval; strm << s; } break;
    case T_Class: { VMemberBase *v = ud.cval; strm << v; } break;
    case T_Obj: { VObject *v = ud.oval; strm << v; } break;
    case T_XObj: { VSerialisable *v = ud.xoval; strm << v; } break;
    case T_Blob:
      {
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
void VNTValue::SkipSerialised (VStream &strm, vuint8 *otype, VName *oname) {
  check(strm.IsLoading());
  vuint8 atype = T_Invalid;
  strm << atype;
  if (atype <= T_Invalid || atype > T_Blob) Sys_Error("invalid NTValue type (%d)", atype);
  VName aname;
  strm << aname;
  if (otype) *otype = atype;
  if (oname) *oname = aname;
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


// ////////////////////////////////////////////////////////////////////////// //
// VNTValueReader
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VNTValueReader::VNTValueReader
//
//  doesn't own passed stream; starts reading from current stream position
//
//==========================================================================
VNTValueReader::VNTValueReader (VStream *ASrcStream)
  : srcStream(ASrcStream)
  , valleft(0)
  , vlist()
  , strmendofs(0)
  , bError(false)
{
  check(srcStream);
  if (srcStream->IsError()) { setError(); return; }
  *srcStream << STRM_INDEX(valleft);
  if (srcStream->IsError()) { setError(); return; }
#ifdef VNTVALUE_DEBUG_DUMP
  fprintf(stderr, "*** %d values\n", valleft);
#endif
}


VNTValueReader::~VNTValueReader () {
  if (srcStream && !bError && !srcStream->IsError()) {
    // skip fields
    do {
      if (strmendofs) {
        srcStream->Seek(strmendofs);
        if (srcStream->IsError()) { setError(); break; }
      }
#ifdef VNTVALUE_DEBUG_DUMP
      fprintf(stderr, "*** rest-skip %d values\n", valleft);
#endif
      VName rdname;
      vuint8 rdtype;
      while (valleft > 0) {
        --valleft;
        if (!VNTValue::ReadTypeName(*srcStream, &rdtype, &rdname)) { setError(); break; }
        if (srcStream->IsError()) { setError(); break; }
#ifdef VNTVALUE_DEBUG_DUMP
        fprintf(stderr, "***   rest-skip: <%s> %u\n", *rdname, (unsigned)rdtype);
#endif
        // skip it
        if (!VNTValue::SkipValue(*srcStream, rdtype)) { setError(); break; }
      }
    } while (0);
    if (srcStream && srcStream->IsError()) setError();
  }
}


//==========================================================================
//
//  VNTValueReader::setError
//
//==========================================================================
void VNTValueReader::setError () {
  srcStream = nullptr;
  bError = true;
  valleft = 0;
  vlist.clear();
}


//==========================================================================
//
//  VNTValueReader::coerceTo
//
//  returns invalid value on error
//  `T_Invalid` `vtype` means "don't coerce"
//
//==========================================================================
VNTValue VNTValueReader::coerceTo (VNTValue v, vuint8 vtype) {
  if (!v.isValid()) return v;
  if (vtype == VNTValue::T_Invalid || v.getType() == vtype) return v;
  // try to convert
  switch (vtype) {
    case VNTValue::T_Int:
      // float can be converted to int
      if (v.isFloat()) {
        float f = v.getFloat();
        if (f < MIN_VINT32 || f > MAX_VINT32) { setError(); return VNTValue(); }
        return VNTValue(v.getName(), (int)f);
      }
      break;
    case VNTValue::T_Float:
      // int can be converted to float
      if (v.isInt()) return VNTValue(v.getName(), (float)v.getInt());
      break;
    case VNTValue::T_Name:
      // string can be converted to name
      if (v.isStr()) {
        VStr s = v.getStr();
        if (s.length() <= NAME_SIZE) return VNTValue(v.getName(), VName(*s));
      }
      break;
    case VNTValue::T_Str:
      // name can be converted to string
      if (v.isName()) return VNTValue(v.getName(), VStr(*v.getName()));
      break;
    // others cannot be converted
  }
  return VNTValue();
}


//==========================================================================
//
//  VNTValueReader::readValue
//
//  if `vtype` is not `T_Invalid`, returns invalid VNTValue if it is
//  not possible to convert
//  returns invalid VNTValue if not found
//
//==========================================================================
VNTValue VNTValueReader::readValue (VName vname, vuint8 vtype) {
  if (!srcStream || srcStream->IsError()) { setError(); return VNTValue(); }
  // continue scan: usually, fields are read in the same order as they are written
  VName rdname;
  vuint8 rdtype;
  const int oldvlen = vlist.length(); // so we won't check already checked fields
  while (valleft > 0) {
    --valleft;
    if (!VNTValue::ReadTypeName(*srcStream, &rdtype, &rdname)) { setError(); return VNTValue(); }
#ifdef VNTVALUE_DEBUG_DUMP
    fprintf(stderr, "***   rd: <%s> %u (%s)\n", *rdname, (unsigned)rdtype, *vname);
#endif
    if (rdname == vname) {
      // i found her!
      VNTValue res = coerceTo(VNTValue::ReadValue(*srcStream, rdtype, rdname), vtype);
      if (!res.isValid()) { setError(); return VNTValue(); }
      strmendofs = srcStream->Tell();
      return res;
    }
    // put into vlist
    ValInfo &vi = vlist.alloc();
    vi.name = rdname;
    vi.type = rdtype;
    vi.ofs = srcStream->Tell();
    if (srcStream->IsError()) { setError(); return VNTValue(); }
    if (!VNTValue::SkipValue(*srcStream, rdtype)) { setError(); return VNTValue(); }
    strmendofs = srcStream->Tell();
  }
  // we're done, try to find in vlist
  for (int f = 0; f < oldvlen; ++f) {
    if (vlist[f].name == vname) {
#ifdef VNTVALUE_DEBUG_DUMP
      fprintf(stderr, "***   found in previous #%d: <%s> %u\n", f, *vlist[f].name, (unsigned)vlist[f].type);
#endif
      srcStream->Seek(vlist[f].ofs);
      if (srcStream->IsError()) { setError(); return VNTValue(); }
      VNTValue res = coerceTo(VNTValue::ReadValue(*srcStream, vlist[f].type, rdname), vtype);
      if (!res.isValid()) { setError(); return VNTValue(); }
      // remove field from list
      vlist.removeAt(f);
      return res;
    }
  }
  setError();
  return VNTValue(); // oops
}


//==========================================================================
//
//  VNTValueReader::readInt
//
//==========================================================================
vint32 VNTValueReader::readInt (VName vname) {
  if (bError) return 0;
  VNTValue res = readValue(vname, VNTValue::T_Int);
  return (res ? res.getInt() : 0);
}


//==========================================================================
//
//  VNTValueReader::readFloat
//
//==========================================================================
float VNTValueReader::readFloat (VName vname) {
  if (bError) return 0;
  VNTValue res = readValue(vname, VNTValue::T_Float);
  return (res ? res.getFloat() : 0.0f);
}


//==========================================================================
//
//  VNTValueReader::readVec
//
//==========================================================================
TVec VNTValueReader::readVec (VName vname) {
  if (bError) return 0;
  VNTValue res = readValue(vname, VNTValue::T_Vec);
  return (res ? res.getVec() : TVec(0, 0, 0));
}


//==========================================================================
//
//  VNTValueReader::readName
//
//==========================================================================
VName VNTValueReader::readName (VName vname) {
  if (bError) return 0;
  VNTValue res = readValue(vname, VNTValue::T_Name);
  return (res ? res.getVName() : NAME_None);
}


//==========================================================================
//
//  VNTValueReader::readStr
//
//==========================================================================
VStr VNTValueReader::readStr (VName vname) {
  if (bError) return 0;
  VNTValue res = readValue(vname, VNTValue::T_Str);
  return (res ? res.getStr() : VStr::EmptyString);
}


//==========================================================================
//
//  VNTValueReader::readClass
//
//==========================================================================
VClass *VNTValueReader::readClass (VName vname) {
  if (bError) return 0;
  VNTValue res = readValue(vname, VNTValue::T_Class);
  return (res ? res.getClass() : nullptr);
}


//==========================================================================
//
//  VNTValueReader::readObj
//
//==========================================================================
VObject *VNTValueReader::readObj (VName vname) {
  if (bError) return 0;
  VNTValue res = readValue(vname, VNTValue::T_Obj);
  return (res ? res.getObj() : nullptr);
}


//==========================================================================
//
//  VNTValueReader::readXObj
//
//==========================================================================
VSerialisable *VNTValueReader::readXObj (VName vname) {
  if (bError) return 0;
  VNTValue res = readValue(vname, VNTValue::T_XObj);
  return (res ? res.getXObj() : nullptr);
}


// ////////////////////////////////////////////////////////////////////////// //
// VNTValueWriter
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VNTValueWriter::VNTValueWriter
//
//==========================================================================
VNTValueWriter::VNTValueWriter (VStream *astrm)
  : strm(astrm)
  , vlist()
{
  check(astrm);
  check(!astrm->IsLoading());
}


//==========================================================================
//
//  VNTValueWriter::~VNTValueWriter
//
//==========================================================================
VNTValueWriter::~VNTValueWriter () {
  WriteTo(*strm);
}


//==========================================================================
//
//  VNTValueWriter::WriteTo
//
//==========================================================================
void VNTValueWriter::WriteTo (VStream &strm) {
  vint32 count = vlist.length();
  strm << STRM_INDEX(count);
  check(vlist.length() == count);
  for (vint32 f = 0; f < count; ++f) {
    vlist[f].WriteTo(strm);
    if (strm.IsError()) return;
  }
}


//==========================================================================
//
//  VNTValueWriter::putValue
//
//  returns `true` if value was replaced
//
//==========================================================================
bool VNTValueWriter::putValue (const VNTValue &v) {
  const int len = vlist.length();
  for (int f = 0; f < len; ++f) {
    if (vlist[f].getName() == v.getName()) {
      vlist[f] = v;
      return true;
    }
  }
  vlist.append(v);
  return false;
}


// ////////////////////////////////////////////////////////////////////////// //
// VNTValueIO
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VNTValueIO::VNTValueIO
//
//==========================================================================
VNTValueIO::VNTValueIO ()
  : rd(nullptr)
  , wr(nullptr)
  , bError(false)
{
}


//==========================================================================
//
//  VNTValueIO::VNTValueIO
//
//==========================================================================
VNTValueIO::VNTValueIO (VStream *astrm)
  : rd(nullptr)
  , wr(nullptr)
  , bError(false)
{
  check(astrm);
  if (astrm->IsLoading()) {
    rd = new VNTValueReader(astrm);
  } else {
    wr = new VNTValueWriter(astrm);
  }
}


//==========================================================================
//
//  VNTValueIO::~VNTValueIO
//
//==========================================================================
VNTValueIO::~VNTValueIO () {
  if (rd) { delete rd; rd = nullptr; }
  if (wr) { delete wr; wr = nullptr; }
}


//==========================================================================
//
//  VNTValueIO::setup
//
//==========================================================================
void VNTValueIO::setup (VStream *astrm) {
  check(!rd);
  check(!wr);
  check(!bError);
  check(astrm);
  if (astrm->IsLoading()) {
    rd = new VNTValueReader(astrm);
  } else {
    wr = new VNTValueWriter(astrm);
  }
}


//==========================================================================
//
//  VNTValueIO::IsError
//
//==========================================================================
bool VNTValueIO::IsError () {
  if (bError) return true;
  if (rd) return rd->IsError();
  return false;
}


//==========================================================================
//
//  VNTValueIO::io
//
//==========================================================================
void VNTValueIO::io (VName vname, vint32 &v) {
  if (bError) return;
  if (rd) {
    v = rd->readInt(vname);
  } else {
    if (wr->putValue(VNTValue(vname, v))) bError = true;
  }
}


//==========================================================================
//
//  VNTValueIO::io
//
//==========================================================================
void VNTValueIO::io (VName vname, vuint32 &v) {
  if (bError) return;
  if (rd) {
    v = (vuint32)rd->readInt(vname);
  } else {
    if (wr->putValue(VNTValue(vname, (vuint32)v))) bError = true;
  }
}


//==========================================================================
//
//  VNTValueIO::io
//
//==========================================================================
void VNTValueIO::io (VName vname, float &v) {
  if (bError) return;
  if (rd) {
    v = rd->readFloat(vname);
  } else {
    if (wr->putValue(VNTValue(vname, v))) bError = true;
  }
}


//==========================================================================
//
//  VNTValueIO::io
//
//==========================================================================
void VNTValueIO::io (VName vname, TVec &v) {
  if (bError) return;
  if (rd) {
    v = rd->readVec(vname);
  } else {
    if (wr->putValue(VNTValue(vname, v))) bError = true;
  }
}


//==========================================================================
//
//  VNTValueIO::io
//
//==========================================================================
void VNTValueIO::io (VName vname, VName &v) {
  if (bError) return;
  if (rd) {
    v = rd->readName(vname);
  } else {
    if (wr->putValue(VNTValue(vname, v))) bError = true;
  }
}


//==========================================================================
//
//  VNTValueIO::io
//
//==========================================================================
void VNTValueIO::io (VName vname, VStr &v) {
  if (bError) return;
  if (rd) {
    v = rd->readStr(vname);
  } else {
    if (wr->putValue(VNTValue(vname, v))) bError = true;
  }
}


//==========================================================================
//
//  VNTValueIO::io
//
//==========================================================================
void VNTValueIO::io (VName vname, VClass *&v) {
  if (bError) return;
  if (rd) {
    v = rd->readClass(vname);
  } else {
    if (wr->putValue(VNTValue(vname, v))) bError = true;
  }
}


//==========================================================================
//
//  VNTValueIO::io
//
//==========================================================================
void VNTValueIO::io (VName vname, VObject *&v) {
  if (bError) return;
  if (rd) {
    v = rd->readObj(vname);
  } else {
    if (wr->putValue(VNTValue(vname, v))) bError = true;
  }
}


//==========================================================================
//
//  VNTValueIO::io
//
//==========================================================================
void VNTValueIO::io (VName vname, VSerialisable *&v) {
  if (bError) return;
  if (rd) {
    v = rd->readXObj(vname);
  } else {
    if (wr->putValue(VNTValue(vname, v))) bError = true;
  }
}


//==========================================================================
//
//  VNTValueIO::readBlob
//
//==========================================================================
VNTValue VNTValueIO::readBlob (VName vname) {
  if (!rd || bError || rd->IsError()) return VNTValue();
  return rd->readValue(vname);
}


//==========================================================================
//
//  VNTValueIO::writeBlob
//
//==========================================================================
void VNTValueIO::writeBlob (VName vname, const void *buf, int len) {
  if (!wr || bError) return;
  VNTValue v = VNTValue(vname, (const vuint8 *)buf, len, false); // don't copy
  if (wr->putValue(v)) bError = true;
}
