/**************************************************************************
 *
 * Coded by Ketmar Dark, 2018
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 **************************************************************************/
#include "mod_socket.h"

// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_CLASS(V, Socket);


// ////////////////////////////////////////////////////////////////////////// //
void VSocket::Destroy () {
  delete fstream; fstream = nullptr;
  Super::Destroy();
}


// ////////////////////////////////////////////////////////////////////////// //
// returns `none` on error
//native final static VSocket OpenTCP (string ipaddr, optional bool listen);
IMPLEMENT_FUNCTION(VSocket, OpenTCP) {
  /*
  P_GET_STR(fname);
  if (fname.isEmpty()) { RET_REF(nullptr); return; }
  VStream *fs = fsysOpenFile(fname);
  if (!fs) { RET_REF(nullptr); return; }
  VSocket *res = (VSocket *)StaticSpawnObject(StaticClass());
  res->fstream = fs;
  RET_REF(res);
  */
}

// closes the file, but won't destroy the object
//native void close ();
IMPLEMENT_FUNCTION(VSocket, close) {
  P_GET_SELF;
  if (Self && Self->sock) {
    delete Self->sock;
    Self->sock = nullptr;
  }
}

// returns 8-bit char or -1 on EOF/error
//native int getch ();
IMPLEMENT_FUNCTION(VSocket, getch) {
  P_GET_SELF;
  RET_INT(0);
}

// reads `size` bytes from file
// always tries to read as max as possible
// returns empty string on EOF/error
// if `exact` is `true`, out of data means "error"
// default is "not exact"
//native string readBuf (int size, optional bool exact);
IMPLEMENT_FUNCTION(VSocket, readBuf) {
  P_GET_BOOL_OPT(exact, false);
  P_GET_INT(size);
  P_GET_SELF;
  /*
  if (!Self || !Self->fstream || Self->fstream->IsError() || size < 1) { RET_STR(VStr()); return; }
  VStr res;
  if (!exact) {
    int left = Self->fstream->TotalSize()-Self->fstream->Tell();
    if (left < 1) { RET_STR(VStr()); return; }
    if (size > left) size = left;
  }
  res.setLength(size);
  Self->fstream->Serialize(res.GetMutableCharPointer(0), size);
  if (Self->fstream->IsError()) {
    fprintf(stderr, "ERRORED!\n");
    RET_STR(VStr());
    return;
  }
  RET_STR(res);
  */
}

// is this object open (if object is error'd, it is not open)
//native bool isOpen { get; }
IMPLEMENT_FUNCTION(VSocket, get_isOpen) {
  P_GET_SELF;
  //RET_BOOL(Self && Self->fstream && !Self->fstream->IsError());
}

// `true` if something was error'd
// there is no reason to continue after an error (and this is UB)
//native bool error { get; }
IMPLEMENT_FUNCTION(VSocket, get_error) {
  P_GET_SELF;
  //RET_BOOL(Self && Self->fstream ? Self->fstream->IsError() : true);
}

// convenient functions
IMPLEMENT_FUNCTION(VSocket, readU8) {
  P_GET_SELF;
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b;
  Self->fstream->Serialize(&b, 1);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT(b);
}

IMPLEMENT_FUNCTION(VSocket, readU16) {
  P_GET_SELF;
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[2];
  Self->fstream->Serialize(&b[0], 2);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT(b[0]|(b[1]<<8));
}

IMPLEMENT_FUNCTION(VSocket, readU32) {
  P_GET_SELF;
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[4];
  Self->fstream->Serialize(&b[0], 4);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT(b[0]|(b[1]<<8)|(b[2]<<16)|(b[3]<<24));
}

IMPLEMENT_FUNCTION(VSocket, readI8) {
  P_GET_SELF;
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vint8 b;
  Self->fstream->Serialize(&b, 1);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT((vint32)b);
}

IMPLEMENT_FUNCTION(VSocket, readI16) {
  P_GET_SELF;
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[2];
  Self->fstream->Serialize(&b[0], 2);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT((vint32)(vint16)(b[0]|(b[1]<<8)));
}

IMPLEMENT_FUNCTION(VSocket, readI32) {
  P_GET_SELF;
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[4];
  Self->fstream->Serialize(&b[0], 4);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT(b[0]|(b[1]<<8)|(b[2]<<16)|(b[3]<<24));
}

IMPLEMENT_FUNCTION(VSocket, readU16BE) {
  P_GET_SELF;
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[2];
  Self->fstream->Serialize(&b[0], 2);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT(b[1]|(b[0]<<8));
}

IMPLEMENT_FUNCTION(VSocket, readU32BE) {
  P_GET_SELF;
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[4];
  Self->fstream->Serialize(&b[0], 4);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT(b[3]|(b[2]<<8)|(b[1]<<16)|(b[0]<<24));
}

IMPLEMENT_FUNCTION(VSocket, readI16BE) {
  P_GET_SELF;
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[2];
  Self->fstream->Serialize(&b[0], 2);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT((vint32)(vint16)(b[1]|(b[0]<<8)));
}

IMPLEMENT_FUNCTION(VSocket, readI32BE) {
  P_GET_SELF;
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[4];
  Self->fstream->Serialize(&b[0], 4);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT(b[3]|(b[2]<<8)|(b[1]<<16)|(b[0]<<24));
}
