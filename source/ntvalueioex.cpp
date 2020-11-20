//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2019-2020 Ketmar Dark
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
//**  extended NTValue-based I/O
//**
//**************************************************************************
#include "gamedefs.h"


//==========================================================================
//
//  VNTValueIOEx::VNTValueIOEx
//
//==========================================================================
VNTValueIOEx::VNTValueIOEx (VStream *astrm)
  : VNTValueIO(astrm)
  , prefix(VStr::EmptyString)
{
}


//==========================================================================
//
//  VNTValueIOEx::transformName
//
//==========================================================================
VName VNTValueIOEx::transformName (VName vname) const {
  if (prefix.length() == 0) return vname;
  return VName(*(prefix+(*vname)));
}


//==========================================================================
//
//  VNTValueIOEx::iodef
//
//==========================================================================
void VNTValueIOEx::iodef (VName vname, vint32 &v, vint32 defval) {
  VNTValueIO::iodef(transformName(vname), v, defval);
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, vint32 &v) {
  VNTValueIO::io(transformName(vname), v);
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, vuint32 &v) {
  VNTValueIO::io(transformName(vname), v);
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, float &v) {
  VNTValueIO::io(transformName(vname), v);
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, TVec &v) {
  VNTValueIO::io(transformName(vname), v);
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, VName &v) {
  VNTValueIO::io(transformName(vname), v);
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, VStr &v) {
  VNTValueIO::io(transformName(vname), v);
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, VClass *&v) {
  VNTValueIO::io(transformName(vname), v);
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, VObject *&v) {
  VNTValueIO::io(transformName(vname), v);
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, VSerialisable *&v) {
  VNTValueIO::io(transformName(vname), v);
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, VThinker *&v) {
  VObject *o = (VObject *)v;
  io(transformName(vname), o);
  if (IsLoading()) v = (VThinker *)o;
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, VEntity *&v) {
  VObject *o = (VObject *)v;
  io(transformName(vname), o);
  if (IsLoading()) v = (VEntity *)o;
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, VTextureID &v) {
  if (IsError()) {
    if (IsLoading()) v.id = 0;
    return;
  }

  if (IsLoading()) {
    // reading
    VName tname;
    int ttype;
    io(transformName(vname), tname);
    io(VName(va("%s.ttype", *transformName(vname))), ttype);
    if (IsError()) {
      if (IsLoading()) v.id = 0;
      return;
    }
    if (ttype == -666 && tname == NAME_None) {
      // new version
      auto blob = readBlob(VName(va("%s.blobv0", *transformName(vname))));
      if (!blob.isValid() || !blob.isBlob()) {
        GCon->Log(NAME_Warning, "LOAD: save file is probably broken (no texture blob)");
        v.id = 0;
        return;
      }
      //GCon->Logf(NAME_Debug, "LDBLOB: size=%d", blob.getBlobSize());
      //VStr s = "   "; for (int f = 0; f < blob.getBlobSize(); ++f) s += va(" %02x", blob.getBlobPtr()[f]); GCon->Logf(NAME_Debug, "%s", *s);
      VMemoryStreamRO rst;
      rst.Setup("::texture", blob.getBlobPtr(), blob.getBlobSize());
      v.Serialise(rst);
    } else if (tname == NAME_None) {
      //GCon->Log(NAME_Warning, "LOAD: save file is probably broken (empty texture name)");
      v.id = 0;
    } else {
      auto lock = GTextureManager.LockMapLocalTextures();
      /*
      int texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Wall, true, false);
      if (texid < 0) texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Flat, true, false);
      if (texid < 0) texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Sprite, true, false);
      */
      int texid = GTextureManager.CheckNumForName(tname, ttype, true/*overload*/);
      if (texid < 0) texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Wall, true, false);
      if (texid < 0) texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Flat, true, false);
      if (texid < 0) texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Sprite, true, false);
      if (texid < 0) texid = GTextureManager.CheckNumForName(tname, TEXTYPE_Pic, true/*overload*/);
      //if (texid < 0) texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Pic, true, false);
      if (texid < 0) {
        GCon->Logf(NAME_Warning, "LOAD: save file is broken (texture '%s' not found)", *tname);
        texid = GTextureManager.DefaultTexture;
      }
      v.id = texid;
      if (GTextureManager.getIgnoreAnim(v.id)->Type != ttype) {
        GCon->Logf(NAME_Warning, "TXRD<%s>: %5d <%s> (%d)", *transformName(vname), v.id, *tname, GTextureManager.getIgnoreAnim(v.id)->Type);
      }
    }
    //GCon->Logf("txrd: <%s> : %d", *vname, v.id);
  } else {
    //GCon->Logf("txwr: <%s> : %d", *vname, v.id);
    // writing (new version)
    VName tname = NAME_None;
    int ttype = -666;
    io(transformName(vname), tname);
    io(VName(va("%s.ttype", *transformName(vname))), ttype);

    VMemoryStream mst;
    mst.BeginWrite();
    v.Serialise(mst);
    TArray<vuint8> &arr = mst.GetArray();
    //GCon->Logf(NAME_Debug, "WRBLOB: size=%d", arr.length());
    writeBlob(VName(va("%s.blobv0", *transformName(vname))), arr.ptr(), arr.length());
    //VStr s = "   "; for (int f = 0; f < arr.length(); ++f) s += va(" %02x", arr[f]); GCon->Logf(NAME_Debug, "%s", *s);

    /* (old version)
    if (v.id > 0) {
      if (!GTextureManager.getIgnoreAnim(v.id)) {
        GCon->Logf(NAME_Warning, "SAVE: trying to save inexisting texture with id #%d", v.id);
        tname = VName(" ! ");
      } else {
        tname = GTextureManager.GetTextureName(v.id);
        ttype = GTextureManager.getIgnoreAnim(v.id)->Type;
        //GCon->Logf("TXWR<%s>: %5d <%s> (%d)", *vname, v.id, *tname, GTextureManager.getIgnoreAnim(v.id)->Type);
      }
    }
    io(transformName(vname), tname);
    io(VName(va("%s.ttype", *transformName(vname))), ttype);
    */
  }
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, vuint8 &v) {
  vuint32 n = v;
  io(transformName(vname), n);
  if (IsLoading()) v = clampToByte(n);
}



//**************************************************************************
//
// VCheckedStream
//
//**************************************************************************

//==========================================================================
//
//  VCheckedStream::VCheckedStream
//
//==========================================================================
VCheckedStream::VCheckedStream (VStream *ASrcStream) : srcStream(ASrcStream), useSysError(false) {
  if (!ASrcStream) Host_Error("source stream is null");
  bLoading = ASrcStream->IsLoading();
  bError = ASrcStream->IsError();
  checkError();
}


//==========================================================================
//
//  VCheckedStream::openStreamAndCopy
//
//==========================================================================
void VCheckedStream::openStreamAndCopy (VStream *st, bool doCopy) {
  if (!st) { if (useSysError) Sys_Error("source stream is null"); else Host_Error("source stream is null"); }
  if (!st->IsLoading()) { st->Close(); delete st; if (useSysError) Sys_Error("source stream must be for reading"); else Host_Error("source stream must be for reading"); }
  const int ssize = st->TotalSize();
  if (!doCopy || ssize <= 0 || ssize > 1024*1024*64) {
    // direct
    srcStream = st;
    bError = st->IsError();
  } else {
    // load to memory stream
    VMemoryStream *ms = new VMemoryStream(st->GetName());
    TArray<vuint8> &arr = ms->GetArray();
    arr.setLength(ssize);
    st->Serialise(arr.ptr(), ssize);
    const bool err = st->IsError();
    st->Close();
    delete st;
    srcStream = ms;
    ms->BeginRead();
    bError = err;
  }
  bLoading = true;
  checkError();
}


//==========================================================================
//
//  VCheckedStream::VCheckedStream
//
//==========================================================================
VCheckedStream::VCheckedStream (VStream *ASrcStream, bool doCopy) : srcStream(nullptr), useSysError(false) {
  openStreamAndCopy(ASrcStream, doCopy);
}


//==========================================================================
//
//  VCheckedStream::VCheckedStream
//
//==========================================================================
VCheckedStream::VCheckedStream (int LumpNum, bool aUseSysError) : srcStream(nullptr), useSysError(aUseSysError) {
  if (LumpNum < 0) {
    if (useSysError) Sys_Error("invalid lump number (%d) in VCheckedStream::VCheckedStream", LumpNum);
    else Host_Error("invalid lump number (%d) in VCheckedStream::VCheckedStream", LumpNum);
  }
  VStream *lst = W_CreateLumpReaderNum(LumpNum);
  if (!lst) {
    if (useSysError) Sys_Error("cannot read lump (%d) in VCheckedStream::VCheckedStream", LumpNum);
    Host_Error("cannot read lump (%d) in VCheckedStream::VCheckedStream", LumpNum);
  }
  openStreamAndCopy(lst, true);
}


//==========================================================================
//
//  VCheckedStream::~VCheckedStream
//
//==========================================================================
VCheckedStream::~VCheckedStream () {
  Close();
}


//==========================================================================
//
//  VCheckedStream::SetSysErrorMode
//
//==========================================================================
void VCheckedStream::SetSysErrorMode () {
  useSysError = true;
}


//==========================================================================
//
//  VCheckedStream::checkError
//
//==========================================================================
void VCheckedStream::checkError () const {
  if (bError) {
    if (srcStream) {
      VStr name = srcStream->GetName();
      srcStream->Close();
      delete srcStream;
      srcStream = nullptr;
      if (useSysError) Sys_Error("error %s '%s'", (bLoading ? "loading from" : "writing to"), *name);
      else Host_Error("error %s '%s'", (bLoading ? "loading from" : "writing to"), *name);
    } else {
      if (useSysError) Sys_Error("error %s", (bLoading ? "loading" : "writing"));
      else Host_Error("error %s", (bLoading ? "loading" : "writing"));
    }
  }
}


//==========================================================================
//
//  VCheckedStream::Close
//
//==========================================================================
bool VCheckedStream::Close () {
  if (srcStream) {
    checkError();
    srcStream->Close();
    delete srcStream;
    srcStream = nullptr;
  }
  return true;
}


//==========================================================================
//
//  VCheckedStream::GetName
//
//==========================================================================
VStr VCheckedStream::GetName () const {
  return (srcStream ? srcStream->GetName() : VStr());
}


//==========================================================================
//
//  VCheckedStream::IsError
//
//==========================================================================
bool VCheckedStream::IsError () const {
  checkError();
  return false;
}


//==========================================================================
//
//  VCheckedStream::checkValidityCond
//
//==========================================================================
void VCheckedStream::checkValidityCond (bool mustBeTrue) {
  if (!bError) {
    if (!mustBeTrue || !srcStream || srcStream->IsError()) SetError();
  }
  if (bError) checkError();
}


//==========================================================================
//
//  VCheckedStream::Serialise
//
//==========================================================================
void VCheckedStream::Serialise (void *buf, int len) {
  checkValidityCond(len >= 0);
  if (len == 0) return;
  srcStream->Serialise(buf, len);
  checkError();
}


//==========================================================================
//
//  VCheckedStream::SerialiseBits
//
//==========================================================================
void VCheckedStream::SerialiseBits (void *Data, int Length) {
  checkValidityCond(Length >= 0);
  srcStream->SerialiseBits(Data, Length);
  checkError();
}


void VCheckedStream::SerialiseInt (vuint32 &Value/*, vuint32 Max*/) {
  checkValidity();
  srcStream->SerialiseInt(Value/*, Max*/);
  checkError();
}


//==========================================================================
//
//  VCheckedStream::Seek
//
//==========================================================================
void VCheckedStream::Seek (int pos) {
  checkValidityCond(pos >= 0);
  srcStream->Seek(pos);
  checkError();
}


//==========================================================================
//
//  VCheckedStream::Tell
//
//==========================================================================
int VCheckedStream::Tell () {
  checkValidity();
  int res = srcStream->Tell();
  checkError();
  return res;
}


//==========================================================================
//
//  VCheckedStream::TotalSize
//
//==========================================================================
int VCheckedStream::TotalSize () {
  checkValidity();
  int res = srcStream->TotalSize();
  checkError();
  return res;
}


//==========================================================================
//
//  VCheckedStream::AtEnd
//
//==========================================================================
bool VCheckedStream::AtEnd () {
  checkValidity();
  bool res = srcStream->AtEnd();
  checkError();
  return res;
}


//==========================================================================
//
//  VCheckedStream::Flush
//
//==========================================================================
void VCheckedStream::Flush () {
  checkValidity();
  srcStream->Flush();
  checkError();
}


#define PARTIAL_DO_IO()  do { \
  checkValidity(); \
  srcStream->io(v); \
  checkError(); \
} while (0)


//==========================================================================
//
//  VCheckedStream::io
//
//==========================================================================
void VCheckedStream::io (VName &v) {
  PARTIAL_DO_IO();
}


//==========================================================================
//
//  VCheckedStream::io
//
//==========================================================================
void VCheckedStream::io (VStr &v) {
  PARTIAL_DO_IO();
}


//==========================================================================
//
//  VCheckedStream::io
//
//==========================================================================
void VCheckedStream::io (VObject *&v) {
  PARTIAL_DO_IO();
}


//==========================================================================
//
//  VCheckedStream::io
//
//==========================================================================
void VCheckedStream::io (VMemberBase *&v) {
  PARTIAL_DO_IO();
}


//==========================================================================
//
//  VCheckedStream::io
//
//==========================================================================
void VCheckedStream::io (VSerialisable *&v) {
  PARTIAL_DO_IO();
}


//==========================================================================
//
//  VCheckedStream::SerialiseStructPointer
//
//==========================================================================
void VCheckedStream::SerialiseStructPointer (void *&Ptr, VStruct *Struct) {
  checkValidityCond(!!Ptr && !!Struct);
  srcStream->SerialiseStructPointer(Ptr, Struct);
  checkError();
}
