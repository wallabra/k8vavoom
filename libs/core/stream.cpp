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


const VStr VStream::mEmptyName = VStr();


//==========================================================================
//
//  VStream::~VStream
//
//==========================================================================
VStream::~VStream () {
}


//==========================================================================
//
//  VStream::GetName
//
//==========================================================================
const VStr &VStream::GetName () const {
  return mEmptyName;
}


//==========================================================================
//
//  VStream::Serialise
//
//==========================================================================
void VStream::Serialise (void *, int) {
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
  guardSlow(VStream::SerialiseBits);
  Serialise(Data, (Length+7)>>3);
  if (IsLoading() && (Length&7)) {
    ((vuint8*)Data)[Length>>3] &= (1<<(Length&7))-1;
  }
  unguardSlow;
}


//==========================================================================
//
//  VStream::SerialiseInt
//
//==========================================================================
void VStream::SerialiseInt (vuint32 &Value, vuint32) {
  guardSlow(VStream::SerialiseInt);
  *this << Value;
  unguardSlow;
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
  return *this;
}


//==========================================================================
//
//  VStream::operator<<
//
//==========================================================================
VStream &VStream::operator << (VObject *&) {
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
  guard(operator VStream << VStreamCompactIndex);
  if (Strm.IsLoading()) {
    vuint8 b;
    Strm << b;
    bool Neg = !!(b&0x40);
    vint32 Val = b&0x3f;
    if (b&0x80) {
      Strm << b;
      Val |= (b&0x7f)<<6;
      if (b&0x80) {
        Strm << b;
        Val |= (b&0x7f)<<13;
        if (b&0x80) {
          Strm << b;
          Val |= (b&0x7f)<<20;
          if (b & 0x80) {
            Strm << b;
            Val |= (b&0x7f)<<27;
          }
        }
      }
    }
    if (Neg) Val = -Val;
    I.Val = Val;
  } else {
    vint32 Val = I.Val;
    if (Val < 0) Val = -Val;
    vuint8 b = Val&0x3f;
    if (I.Val < 0) b |= 0x40;
    if (Val & 0xffffffc0) b |= 0x80;
    Strm << b;
    if (Val&0xffffffc0) {
      b = (Val>>6)&0x7f;
      if (Val&0xffffe000) b |= 0x80;
      Strm << b;
      if (Val&0xffffe000) {
        b = (Val>>13)&0x7f;
        if (Val&0xfff00000) b |= 0x80;
        Strm << b;
        if (Val&0xfff00000) {
          b = (Val>>20)&0x7f;
          if (Val&0xf8000000) b |= 0x80;
          Strm << b;
          if (Val&0xf8000000) {
            b = (Val>>27)&0x7f;
            Strm << b;
          }
        }
      }
    }
  }
  return Strm;
  unguard;
}


//==========================================================================
//
//  operator <<
//
//==========================================================================
VStream &operator << (VStream &Strm, VStreamCompactIndexU &I) {
  guard(operator VStream << VStreamCompactIndexU);
  if (Strm.IsLoading()) {
    vuint8 b;
    Strm << b;
    vuint32 Val = b&0x7f;
    if (b&0x80) {
      Strm << b;
      Val |= (b&0x7f)<<7;
      if (b&0x80) {
        Strm << b;
        Val |= (b&0x7f)<<14;
        if (b&0x80) {
          Strm << b;
          Val |= (b&0x7f)<<21;
          if (b & 0x80) {
            Strm << b;
            Val |= (b&0x0f)<<28;
          }
        }
      }
    }
    I.Val = Val;
  } else {
    vuint32 Val = I.Val;
    vuint8 b = Val&0x7f;
    if (Val&0xffffff80) b |= 0x80;
    Strm << b;
    if (Val&0xffffff80) {
      b = (Val>>7)&0x7f;
      if (Val&0xffffc000) b |= 0x80;
      Strm << b;
      if (Val&0xffffc000) {
        b = (Val>>14)&0x7f;
        if (Val&0xffe00000) b |= 0x80;
        Strm << b;
        if (Val&0xffe00000) {
          b = (Val>>21)&0x7f;
          if (Val&0xf0000000)b |= 0x80;
          Strm << b;
          if (Val&0xf0000000) {
            b = (Val>>28)&0x7f;
            Strm << b;
          }
        }
      }
    }
  }
  return Strm;
  unguard;
}
