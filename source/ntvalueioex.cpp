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
{
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, VThinker *&v) {
  VObject *o = (VObject *)v;
  io(vname, o);
  if (IsLoading()) v = (VThinker *)o;
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, VEntity *&v) {
  VObject *o = (VObject *)v;
  io(vname, o);
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
    io(vname, tname);
    io(VName(va("%s.ttype", *vname)), ttype);
    if (IsError()) {
      if (IsLoading()) v.id = 0;
      return;
    }
    if (tname == NAME_None) {
      //GCon->Log(NAME_Warning, "LOAD: save file is probably broken (empty texture name)");
      v.id = 0;
    } else {
      auto lock = GTextureManager.LockMapLocalTextures();
      /*
      int texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Wall, true, true, false);
      if (texid < 0) texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Flat, true, true, false);
      if (texid < 0) texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Sprite, true, true, false);
      */
      int texid = GTextureManager.CheckNumForName(tname, ttype, true/*overload*/, true/*checkany*/);
      if (texid < 0) texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Wall, true, true, false);
      if (texid < 0) texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Flat, true, true, false);
      if (texid < 0) texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Sprite, true, true, false);
      if (texid < 0) {
        GCon->Logf(NAME_Warning, "LOAD: save file is broken (texture '%s' not found)", *tname);
        texid = GTextureManager.DefaultTexture;
      }
      v.id = texid;
      if (GTextureManager.getIgnoreAnim(v.id)->Type != ttype) {
        GCon->Logf(NAME_Warning, "TXRD<%s>: %5d <%s> (%d)", *vname, v.id, *tname, GTextureManager.getIgnoreAnim(v.id)->Type);
      }
    }
    //GCon->Logf("txrd: <%s> : %d", *vname, v.id);
  } else {
    //GCon->Logf("txwr: <%s> : %d", *vname, v.id);
    // writing
    VName tname = NAME_None;
    int ttype = TEXTYPE_Wall;
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
    io(vname, tname);
    io(VName(va("%s.ttype", *vname)), ttype);
  }
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, vuint8 &v) {
  vuint32 n = v;
  io(vname, n);
  if (IsLoading()) v = clampToByte(n);
}



//==========================================================================
//
//  VCheckedStream::VCheckedStream
//
//==========================================================================
VCheckedStream::VCheckedStream (VStream *ASrcStream) : srcStream(ASrcStream) {
  if (!ASrcStream) Host_Error("source stream is null");
  bLoading = ASrcStream->IsLoading();
  checkError();
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
//  VCheckedStream::SetStream
//
//==========================================================================
void VCheckedStream::SetStream (VStream *ASrcStream) {
  Close();
  bError = false; // just in case
  if (!ASrcStream) Host_Error("source stream is null");
  srcStream = ASrcStream;
  bLoading = ASrcStream->IsLoading();
  checkError();
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
      delete srcStream;
      srcStream = nullptr;
      Host_Error("error %s '%s'", (bLoading ? "loading from" : "writing to"), *name);
    } else {
      Host_Error("error %s", (bLoading ? "loading" : "writing"));
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
const VStr &VCheckedStream::GetName () const {
  return (srcStream ? srcStream->GetName() : VStr::EmptyString);
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
    if (!mustBeTrue || !srcStream || srcStream->IsError()) bError = true;
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


void VCheckedStream::SerialiseInt (vuint32 &Value, vuint32 Max) {
  checkValidity();
  srcStream->SerialiseInt(Value, Max);
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
