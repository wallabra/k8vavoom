//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
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
#include "core.h"


// ////////////////////////////////////////////////////////////////////////// //
/*
if bit 7 of first byte is not set, this is one-byte number in range [0..127].
if bit 7 is set, this is encoded number in the following format:

  bits 5-6:
    0: this is 13-bit number in range [0..8191] (max: 0x1fff)
    1: this is 21-bit number in range [0..2097151] (max: 0x1fffff)
    2: this is 29-bit number in range [0..536870911] (max: 0x1fffffff)
    3: extended number, next 2 bits are used to specify format; result should be xored with -1

  bit 3-4 for type 3:
    0: this is 11-bit number in range [0..2047] (max: 0x7ff)
    1: this is 19-bit number in range [0..524287] (max: 0x7ffff)
    2: this is 27-bit number in range [0..134217727] (max: 0x7ffffff)
    3: read next 4 bytes as 32 bit number (other bits in this byte should be zero)
*/


// returns number of bytes required to decode full number, in range [1..5]
int decodeVarIntLength (const vuint8 firstByte) {
  if ((firstByte&0x80) == 0) {
    return 1;
  } else {
    // multibyte
    switch (firstByte&0x60) {
      case 0x00: return 2;
      case 0x20: return 3;
      case 0x40: return 4;
      case 0x60: // most complex format
        switch (firstByte&0x18) {
          case 0x00: return 2;
          case 0x08: return 3;
          case 0x10: return 4;
          case 0x18: return 5;
        }
    }
  }
  return -1; // the thing that should not be
}


// returns decoded number; can consume up to 5 bytes
vuint32 decodeVarInt (const void *data) {
  const vuint8 *buf = (const vuint8 *)data;
  if ((buf[0]&0x80) == 0) {
    return buf[0];
  } else {
    // multibyte
    switch (buf[0]&0x60) {
      case 0x00: return ((buf[0]&0x1f)<<8)|buf[1];
      case 0x20: return ((buf[0]&0x1f)<<16)|(buf[1]<<8)|buf[2];
      case 0x40: return ((buf[0]&0x1f)<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3];
      case 0x60: // most complex format
        switch (buf[0]&0x18) {
          case 0x00: return (vuint32)(((buf[0]&0x07)<<8)|buf[1])^0xffffffffU;
          case 0x08: return (vuint32)(((buf[0]&0x07)<<16)|(buf[1]<<8)|buf[2])^0xffffffffU;
          case 0x10: return (vuint32)(((buf[0]&0x07)<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3])^0xffffffffU;
          case 0x18: return (vuint32)((buf[1]<<24)|(buf[2]<<16)|(buf[3]<<8)|buf[4])^0xffffffffU;
        }
    }
  }
  return 0xffffffffU; // the thing that should not be
}


// returns number of used bytes; can consume up to 5 bytes
int encodeVarInt (void *data, vuint32 n) {
  vuint8 *buf = (vuint8 *)data;
  if (n <= 0x1fffffff) {
    // positive
    if (n <= 0x7f) {
      *buf = (vuint8)n;
      return 1;
    }
    if (n <= 0x1fff) {
      *buf++ = (vuint8)(n>>8)|0x80;
      *buf = (vuint8)(n&0xff);
      return 2;
    }
    if (n <= 0x1fffff) {
      *buf++ = (vuint8)(n>>16)|(0x80|0x20);
      *buf++ = (vuint8)((n>>8)&0xff);
      *buf = (vuint8)(n&0xff);
      return 3;
    }
    // invariant: n <= 0x1fffffff
    *buf++ = (vuint8)(n>>24)|(0x80|0x40);
    *buf++ = (vuint8)((n>>16)&0xff);
    *buf++ = (vuint8)((n>>8)&0xff);
    *buf = (vuint8)(n&0xff);
    return 4;
  } else {
    // either negative, or full 32 bits required; format 3
    // first, xor it
    n ^= 0xffffffffU;
    if (n <= 0x7ff) {
      *buf++ = (vuint8)(n>>8)|(0x80|0x60);
      *buf = (vuint8)(n&0xff);
      return 2;
    }
    if (n <= 0x7ffff) {
      *buf++ = (vuint8)(n>>16)|(0x80|0x60|0x08);
      *buf++ = (vuint8)((n>>8)&0xff);
      *buf = (vuint8)(n&0xff);
      return 3;
    }
    if (n <= 0x7ffffff) {
      *buf++ = (vuint8)(n>>24)|(0x80|0x60|0x10);
      *buf++ = (vuint8)((n>>16)&0xff);
      *buf++ = (vuint8)((n>>8)&0xff);
      *buf = (vuint8)(n&0xff);
      return 4;
    }
    // full 32 bits
    *buf++ = (vuint8)(0x80|0x60|0x18);
    *buf++ = (vuint8)((n>>24)&0xff);
    *buf++ = (vuint8)((n>>16)&0xff);
    *buf++ = (vuint8)((n>>8)&0xff);
    *buf = (vuint8)(n&0xff);
    return 5;
  }
}



//==========================================================================
//
//  VSerialisable::~VSerialisable
//
//==========================================================================
VSerialisable::~VSerialisable () {
}



//==========================================================================
//
//  VStream::~VStream
//
//==========================================================================
VStream::~VStream () {
  Close();
}


//==========================================================================
//
//  VStream::GetName
//
//==========================================================================
const VStr &VStream::GetName () const {
  return VStr::EmptyString;
}


//==========================================================================
//
//  VStream::GetVersion
//
//  stream version; usually purely informational
//
//==========================================================================
vint16 VStream::GetVersion () {
  return 0;
}


//==========================================================================
//
//  VStream::Serialise
//
//==========================================================================
void VStream::Serialise (void *, int) {
  //abort(); // k8: nope, we need dummy one
}


//==========================================================================
//
//  VStream::Serialise
//
//==========================================================================
void VStream::Serialise (const void *buf, int len) {
  if (!IsSaving()) Sys_Error("VStream::Serialise(const): expecting writer stream");
  Serialise((void *)buf, len);
}


//==========================================================================
//
//  VStream::SerialiseBits
//
//==========================================================================
void VStream::SerialiseBits (void *Data, int Length) {
  Serialise(Data, (Length+7)>>3);
  if (IsLoading() && (Length&7)) {
    ((vuint8*)Data)[Length>>3] &= (1<<(Length&7))-1;
  }
}


//==========================================================================
//
//  VStream::SerialiseInt
//
//==========================================================================
void VStream::SerialiseInt (vuint32 &Value, vuint32) {
  *this << Value;
}


//==========================================================================
//
//  VStream::Seek
//
//==========================================================================
void VStream::Seek (int) {
}


//==========================================================================
//
//  VStream::Tell
//
//==========================================================================
int VStream::Tell () {
  return -1;
}


//==========================================================================
//
//  VStream::TotalSize
//
//==========================================================================
int VStream::TotalSize () {
  return -1;
}


//==========================================================================
//
//  VStream::AtEnd
//
//==========================================================================
bool VStream::AtEnd () {
  int Pos = Tell();
  return (Pos != -1 && Pos >= TotalSize());
}


//==========================================================================
//
//  VStream::Flush
//
//==========================================================================
void VStream::Flush () {
}


//==========================================================================
//
//  VStream::Close
//
//==========================================================================
bool VStream::Close () {
  return !bError;
}


//==========================================================================
//
//  VStream::operator<<
//
//==========================================================================
VStream &VStream::operator << (VName &) {
  //abort(); // k8: nope, we need dummy one
  return *this;
}


//==========================================================================
//
//  VStream::operator<<
//
//==========================================================================
VStream &VStream::operator << (VStr &s) {
  s.Serialise(*this);
  return *this;
}


//==========================================================================
//
//  VStream::operator<<
//
//==========================================================================
VStream &VStream::operator << (const VStr &s) {
  check(!IsLoading());
  s.Serialise(*this);
  return *this;
}


//==========================================================================
//
//  VStream::operator<<
//
//==========================================================================
VStream &VStream::operator << (VObject *&) {
  //abort(); // k8: nope, we need dummy one
  return *this;
}


//==========================================================================
//
//  VStream::SerialiseStructPointer
//
//==========================================================================
void VStream::SerialiseStructPointer (void *&, VStruct *) {
}


//==========================================================================
//
//  VStream::operator<<
//
//==========================================================================
VStream &VStream::operator << (VMemberBase *&) {
  //abort(); // k8: nope, we need dummy one
  return *this;
}


//==========================================================================
//
//  VStream::operator<<
//
//==========================================================================
VStream &VStream::operator << (VSerialisable *&) {
  abort();
  return *this;
}


//==========================================================================
//
//  VStream::SerialiseLittleEndian
//
//==========================================================================
void VStream::SerialiseLittleEndian (void *Val, int Len) {
  guard(VStream::SerialiseLittleEndian);
#ifdef VAVOOM_BIG_ENDIAN
    // swap byte order
    for (int i = Len-1; i >= 0; --i) Serialise((vuint8*)Val+i, 1);
#else
    // already in correct byte order
    Serialise(Val, Len);
#endif
  unguard;
}


//==========================================================================
//
//  VStream::SerialiseBigEndian
//
//==========================================================================
void VStream::SerialiseBigEndian (void *Val, int Len) {
  guard(VStream::SerialiseBigEndian);
#ifdef VAVOOM_LITTLE_ENDIAN
    // swap byte order
    for (int i = Len - 1; i >= 0; i--) Serialise((vuint8*)Val+i, 1);
#else
    // already in correct byte order
    Serialise(Val, Len);
#endif
  unguard;
}


//==========================================================================
//
//  operator <<
//
//==========================================================================
VStream &operator << (VStream &Strm, VStreamCompactIndex &I) {
  vuint8 buf[5];
  if (Strm.IsLoading()) {
    Strm << buf[0];
    const int length = decodeVarIntLength(buf[0]);
    if (length > 1) Strm.Serialise(buf+1, length-1);
    I.Val = (vint32)decodeVarInt(buf);
  } else {
    const int length = encodeVarInt(buf, (vuint32)I.Val);
    Strm.Serialise(buf, length);
  }
  return Strm;
}


//==========================================================================
//
//  operator <<
//
//==========================================================================
VStream &operator << (VStream &Strm, VStreamCompactIndexU &I) {
  vuint8 buf[5];
  if (Strm.IsLoading()) {
    Strm << buf[0];
    const int length = decodeVarIntLength(buf[0]);
    if (length > 1) Strm.Serialise(buf+1, length-1);
    I.Val = decodeVarInt(buf);
  } else {
    const int length = encodeVarInt(buf, I.Val);
    Strm.Serialise(buf, length);
  }
  return Strm;
}


// ////////////////////////////////////////////////////////////////////////// //
VStream &operator << (VStream &Strm, vint8 &Val) { Strm.Serialise(&Val, 1); return Strm; }
VStream &operator << (VStream &Strm, vuint8 &Val) { Strm.Serialise(&Val, 1); return Strm; }
VStream &operator << (VStream &Strm, vint16 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
VStream &operator << (VStream &Strm, vuint16 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
VStream &operator << (VStream &Strm, vint32 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
VStream &operator << (VStream &Strm, vuint32 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
VStream &operator << (VStream &Strm, float &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
VStream &operator << (VStream &Strm, double &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
