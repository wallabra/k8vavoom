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
#include "mod_textread.h"

// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_CLASS(V, TextReader);


// ////////////////////////////////////////////////////////////////////////// //
void VTextReader::Destroy () {
  delete fstream; fstream = nullptr;
  Super::Destroy();
}


// ////////////////////////////////////////////////////////////////////////// //
// returns `none` on error
//native final static TextReader Open (string filename);
IMPLEMENT_FUNCTION(VTextReader, Open) {
  P_GET_STR(fname);
  if (fname.isEmpty()) { RET_REF(nullptr); return; }
  VStream *fs = fsysOpenFile(fname);
  if (!fs) { RET_REF(nullptr); return; }
  VTextReader *res = (VTextReader *)StaticSpawnObject(StaticClass());
  res->fstream = fs;
  RET_REF(res);
}

// closes the file, but won't destroy the object
//native void close ();
IMPLEMENT_FUNCTION(VTextReader, close) {
  P_GET_SELF;
  if (Self && Self->fstream) {
    delete Self->fstream;
    Self->fstream = nullptr;
  }
}

// returns success flag (false means "error")
// whence is one of SeekXXX; default is SeekStart
//native bool seek (int ofs, optional int whence);
IMPLEMENT_FUNCTION(VTextReader, seek) {
  P_GET_INT_OPT(whence, SeekStart);
  P_GET_INT(ofs);
  P_GET_SELF;
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_BOOL(false); return; }
  switch (whence) {
    case SeekCur: ofs += Self->fstream->Tell(); break;
    case SeekEnd: ofs = Self->fstream->TotalSize()+ofs; break;
  }
  if (ofs < 0) ofs = 0;
  Self->fstream->Seek(ofs);
  RET_BOOL(!Self->fstream->IsError());
}

// returns 8-bit char or -1 on EOF/error
//native int getch ();
IMPLEMENT_FUNCTION(VTextReader, getch) {
  P_GET_SELF;
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(-1); return; }
  vuint8 b;
  Self->fstream->Serialize(&b, 1);
  if (Self->fstream->IsError()) { RET_INT(-1); return; }
  RET_INT(b);
}

// reads `size` bytes from file
// always tries to read as max as possible
// returns empty string on EOF/error
// if `exact` is `true`, out of data means "error"
// default is "not exact"
//native string readBuf (int size, optional bool exact);
IMPLEMENT_FUNCTION(VTextReader, readBuf) {
  P_GET_BOOL_OPT(exact, false);
  P_GET_INT(size);
  P_GET_SELF;
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
}

// returns name of the opened file (it may be empty)
//native string fileName { get; }
IMPLEMENT_FUNCTION(VTextReader, get_fileName) {
  P_GET_SELF;
  RET_STR(Self && Self->fstream && !Self->fstream->IsError() ? Self->fstream->GetName() : VStr());
}

// is this object open (if object is error'd, it is not open)
//native bool isOpen { get; }
IMPLEMENT_FUNCTION(VTextReader, get_isOpen) {
  P_GET_SELF;
  RET_BOOL(Self && Self->fstream && !Self->fstream->IsError());
}

// `true` if something was error'd
// there is no reason to continue after an error (and this is UB)
//native bool error { get; }
IMPLEMENT_FUNCTION(VTextReader, get_error) {
  P_GET_SELF;
  RET_BOOL(Self && Self->fstream ? Self->fstream->IsError() : true);
}

// get file size
//native int size { get; }
IMPLEMENT_FUNCTION(VTextReader, get_size) {
  P_GET_SELF;
  RET_INT(Self && Self->fstream && !Self->fstream->IsError() ? Self->fstream->TotalSize() : 0);
}

// get current position
//native int position { get; }
IMPLEMENT_FUNCTION(VTextReader, get_position) {
  P_GET_SELF;
  RET_INT(Self && Self->fstream && !Self->fstream->IsError() ? Self->fstream->Tell() : 0);
}


// convenient functions
IMPLEMENT_FUNCTION(VTextReader, readU8) {
  P_GET_SELF;
  if (!Self || !Self->fstream || Self->fstream->IsError()) { fprintf(stderr, "*** BOOM!\n"); RET_INT(0); return; }
  vuint8 b;
  Self->fstream->Serialize(&b, 1);
  if (Self->fstream->IsError()) { fprintf(stderr, "*** FUCK!\n"); RET_INT(0); return; }
  RET_INT(b);
}

IMPLEMENT_FUNCTION(VTextReader, readU16) {
  P_GET_SELF;
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[2];
  Self->fstream->Serialize(&b[0], 2);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT(b[0]|(b[1]<<8));
}

IMPLEMENT_FUNCTION(VTextReader, readU32) {
  P_GET_SELF;
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[4];
  Self->fstream->Serialize(&b[0], 4);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT(b[0]|(b[1]<<8)|(b[2]<<16)|(b[3]<<24));
}

IMPLEMENT_FUNCTION(VTextReader, readI8) {
  P_GET_SELF;
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vint8 b;
  Self->fstream->Serialize(&b, 1);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT((vint32)b);
}

IMPLEMENT_FUNCTION(VTextReader, readI16) {
  P_GET_SELF;
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[2];
  Self->fstream->Serialize(&b[0], 2);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT((vint32)(vint16)(b[0]|(b[1]<<8)));
}

IMPLEMENT_FUNCTION(VTextReader, readI32) {
  P_GET_SELF;
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[4];
  Self->fstream->Serialize(&b[0], 4);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT(b[0]|(b[1]<<8)|(b[2]<<16)|(b[3]<<24));
}

IMPLEMENT_FUNCTION(VTextReader, readU16BE) {
  P_GET_SELF;
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[2];
  Self->fstream->Serialize(&b[0], 2);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT(b[1]|(b[0]<<8));
}

IMPLEMENT_FUNCTION(VTextReader, readU32BE) {
  P_GET_SELF;
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[4];
  Self->fstream->Serialize(&b[0], 4);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT(b[3]|(b[2]<<8)|(b[1]<<16)|(b[0]<<24));
}

IMPLEMENT_FUNCTION(VTextReader, readI16BE) {
  P_GET_SELF;
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[2];
  Self->fstream->Serialize(&b[0], 2);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT((vint32)(vint16)(b[1]|(b[0]<<8)));
}

IMPLEMENT_FUNCTION(VTextReader, readI32BE) {
  P_GET_SELF;
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[4];
  Self->fstream->Serialize(&b[0], 4);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT(b[3]|(b[2]<<8)|(b[1]<<16)|(b[0]<<24));
}
