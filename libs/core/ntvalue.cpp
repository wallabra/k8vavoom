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
//**
//**  typed and named field
//**
//**************************************************************************
#include "core.h"

//#define VNTVALUE_DEBUG_DUMP


//==========================================================================
//
//  VNTValue::VNTValue
//
//==========================================================================
VNTValue::VNTValue (VName aname, const vuint8 *buf, int bufsz, bool doCopyData) {
  vassert(bufsz >= 0);
  zeroSelf();
  type = T_Blob;
  name = aname;
  if (bufsz == 0) { doCopyData = false; buf = nullptr; }
  if (doCopyData) {
    newBlob(bufsz);
    if (bufsz > 0) {
      if (buf) memcpy(blob, buf, bufsz); else memset(blob, 0, bufsz);
    }
  } else {
    blobRC = nullptr;
    blob = (vuint8 *)buf;
    blobSize = bufsz;
  }
}


//==========================================================================
//
//  VNTValue::ReadTypeName
//
//==========================================================================
bool VNTValue::ReadTypeName (VStream &strm, vuint8 *otype, VName *oname) {
  vassert(strm.IsLoading());
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
  vassert(strm.IsLoading());
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
//  VNTValue::serialiseValueInternal
//
//  should be cleared, type should be set
//
//==========================================================================
void VNTValue::serialiseValueInternal (VStream &strm) {
  if (strm.IsError()) { clear(); return; }
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
      if (!strm.IsLoading()) {
        // writing
        vint32 len = blobSize;
        if (len < 0 || len > 1024*1024*64) Sys_Error("invalid blob size (%d)", len);
        strm << STRM_INDEX(len);
        if (len) strm.Serialise(blob, len);
        vassert(len == blobSize);
        //VStr s = va("  writtenblob(%d):", blobSize); for (int f = 0; f < len; ++f) s += va(" %02x", blob[f]); GLog.Logf(NAME_Debug, "%s", *s);
      } else {
        // reading
        vassert(strm.IsLoading());
        vint32 len = -1;
        strm << STRM_INDEX(len);
        if (len < 0 || len > 1024*1024*64) Sys_Error("invalid blob size (%d)", len);
        decRef(); // just in case
        newBlob(len);
        vassert(blobSize == len);
        if (len > 0) strm.Serialise(blob, len);
        //VStr s = va("  readblob(%d):", blobSize); for (int f = 0; f < len; ++f) s += va(" %02x", blob[f]); GLog.Logf(NAME_Debug, "%s", *s);
      }
      break;
  }
  if (strm.IsError()) { clear(); return; }
}


//==========================================================================
//
//  VNTValue::ReadValue
//
//  call this after `ReadTypeName()`
//
//==========================================================================
VNTValue VNTValue::ReadValue (VStream &strm, vuint8 atype, VName aname) {
  vassert(strm.IsLoading());
  if (!isValidType(atype)) return VNTValue();
  if (strm.IsError()) return VNTValue();
  VNTValue res;
  res.type = atype;
  res.name = aname;
  res.serialiseValueInternal(strm);
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
  serialiseValueInternal(strm);
}


//==========================================================================
//
//  VNTValue::Serialise
//
//==========================================================================
void VNTValue::Serialise (VStream &strm) const {
  vassert(!strm.IsLoading());
  strm << type;
  strm << name;
  // sorry for this hack
  ((VNTValue *)this)->serialiseValueInternal(strm);
}


//==========================================================================
//
//  VNTValue::SkipSerialised
//
//==========================================================================
void VNTValue::SkipSerialised (VStream &strm, vuint8 *otype, VName *oname) {
  vassert(strm.IsLoading());
  vuint8 atype = T_Invalid;
  strm << atype;
  if (atype <= T_Invalid || atype > T_Blob) Sys_Error("invalid NTValue type (%d)", atype);
  VName aname;
  strm << aname;
  if (otype) *otype = atype;
  if (oname) *oname = aname;
  SkipValue(strm, atype);
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
  vassert(srcStream);
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
  vassert(srcStream->IsLoading());
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
  //setError(); // nope, this is not an error
  return VNTValue(); // oops
}


//==========================================================================
//
//  VNTValueReader::readInt
//
//==========================================================================
vint32 VNTValueReader::readInt (VName vname, bool *notfound) {
  if (bError) { if (notfound) *notfound = true; return 0; }
  VNTValue res = readValue(vname, VNTValue::T_Int);
  if (notfound) *notfound = !res;
  return (res ? res.getInt() : 0);
}


//==========================================================================
//
//  VNTValueReader::readFloat
//
//==========================================================================
float VNTValueReader::readFloat (VName vname, bool *notfound) {
  if (bError) { if (notfound) *notfound = true; return 0; }
  VNTValue res = readValue(vname, VNTValue::T_Float);
  if (notfound) *notfound = !res;
  return (res ? res.getFloat() : 0.0f);
}


//==========================================================================
//
//  VNTValueReader::readVec
//
//==========================================================================
TVec VNTValueReader::readVec (VName vname, bool *notfound) {
  if (bError) { if (notfound) *notfound = true; return 0; }
  VNTValue res = readValue(vname, VNTValue::T_Vec);
  if (notfound) *notfound = !res;
  return (res ? res.getVec() : TVec(0, 0, 0));
}


//==========================================================================
//
//  VNTValueReader::readName
//
//==========================================================================
VName VNTValueReader::readName (VName vname, bool *notfound) {
  if (bError) { if (notfound) *notfound = true; return 0; }
  VNTValue res = readValue(vname, VNTValue::T_Name);
  if (notfound) *notfound = !res;
  return (res ? res.getVName() : NAME_None);
}


//==========================================================================
//
//  VNTValueReader::readStr
//
//==========================================================================
VStr VNTValueReader::readStr (VName vname, bool *notfound) {
  if (bError) { if (notfound) *notfound = true; return 0; }
  VNTValue res = readValue(vname, VNTValue::T_Str);
  if (notfound) *notfound = !res;
  return (res ? res.getStr() : VStr::EmptyString);
}


//==========================================================================
//
//  VNTValueReader::readClass
//
//==========================================================================
VClass *VNTValueReader::readClass (VName vname, bool *notfound) {
  if (bError) { if (notfound) *notfound = true; return 0; }
  VNTValue res = readValue(vname, VNTValue::T_Class);
  if (notfound) *notfound = !res;
  return (res ? res.getClass() : nullptr);
}


//==========================================================================
//
//  VNTValueReader::readObj
//
//==========================================================================
VObject *VNTValueReader::readObj (VName vname, bool *notfound) {
  if (bError) { if (notfound) *notfound = true; return 0; }
  VNTValue res = readValue(vname, VNTValue::T_Obj);
  if (notfound) *notfound = !res;
  return (res ? res.getObj() : nullptr);
}


//==========================================================================
//
//  VNTValueReader::readXObj
//
//==========================================================================
VSerialisable *VNTValueReader::readXObj (VName vname, bool *notfound) {
  if (bError) { if (notfound) *notfound = true; return 0; }
  VNTValue res = readValue(vname, VNTValue::T_XObj);
  if (notfound) *notfound = !res;
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
  vassert(astrm);
  vassert(!astrm->IsLoading());
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
  vassert(!strm.IsLoading());
  vint32 count = vlist.length();
  strm << STRM_INDEX(count);
  vassert(vlist.length() == count);
  for (vint32 f = 0; f < count; ++f) {
    vlist[f].Serialise(strm);
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
  vassert(astrm);
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
  vassert(!rd);
  vassert(!wr);
  vassert(!bError);
  vassert(astrm);
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
void VNTValueIO::iodef (VName vname, vint32 &v, vint32 defval) {
  if (bError) { v = defval; return; }
  if (rd) {
    bool notfound = false;
    v = rd->readInt(vname, &notfound);
    if (notfound) v = defval;
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
  return rd->readValue(vname, VNTValue::T_Blob);
}


//==========================================================================
//
//  VNTValueIO::writeBlob
//
//==========================================================================
void VNTValueIO::writeBlob (VName vname, const void *buf, int len) {
  if (!wr || bError) return;
  VNTValue v = VNTValue(vname, (const vuint8 *)buf, len, true); // need to copy it, because it isn't written immediately
  if (wr->putValue(v)) bError = true;
}
