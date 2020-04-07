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
//**  Copyright (C) 2018-2020 Ketmar Dark
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
#include "../core.h"


//**************************************************************************
//
// VBitStreamWriter
//
//**************************************************************************

//==========================================================================
//
//  VBitStreamWriter::VBitStreamWriter
//
//==========================================================================
VBitStreamWriter::VBitStreamWriter (vint32 AMax, bool allowExpand)
  : Max(AMax)
  , Pos(0)
  , bAllowExpand(allowExpand)
{
  bLoading = false;
  int sz = (AMax+7)/8+(allowExpand ? 256 : 0);
  Data.SetNum(sz);
  if (sz > 0) memset(Data.Ptr(), 0, sz);
}


//==========================================================================
//
//  VBitStreamWriter::~VBitStreamWriter
//
//==========================================================================
VBitStreamWriter::~VBitStreamWriter () {
  Close();
}


//==========================================================================
//
//  VBitStreamWriter::Reinit
//
//==========================================================================
void VBitStreamWriter::Reinit (vint32 AMax, bool allowExpand) {
  Max = AMax;
  Pos = 0;
  bAllowExpand = allowExpand;
  bLoading = false;
  int sz = (AMax+7)/8+(allowExpand ? 256 : 0);
  Data.SetNum(sz);
  if (sz > 0) memset(Data.Ptr(), 0, sz);
  bError = false;
}


//==========================================================================
//
//  VBitStreamWriter::cloneFrom
//
//==========================================================================
void VBitStreamWriter::cloneFrom (const VBitStreamWriter *wr) {
  if (!wr || wr == this) return;
  Data = wr->Data;
  Max = wr->Max;
  Pos = wr->Pos;
  bAllowExpand = wr->bAllowExpand;
}


//==========================================================================
//
//  VBitStreamWriter::CopyFromWS
//
//==========================================================================
void VBitStreamWriter::CopyFromWS (const VBitStreamWriter &strm) noexcept {
  if (strm.Pos == 0) return;
  const vuint8 *src = (const vuint8 *)strm.Data.ptr();
  vuint8 mask = 0x01u;
  for (int pos = 0; pos < strm.Pos; ++pos) {
    WriteBit(src[0]&mask);
    if ((mask<<=1) == 0) {
      mask = 0x01u;
      ++src;
    }
  }
}


//==========================================================================
//
//  VBitStreamWriter::Expand
//
//==========================================================================
bool VBitStreamWriter::Expand () noexcept {
  if (!bAllowExpand) return false;
  auto oldSize = Data.length();
  Data.SetNum(oldSize+1024);
  memset(((vuint8 *)(Data.Ptr()))+oldSize, 0, Data.length()-oldSize);
  return true;
}


//==========================================================================
//
//  VBitStreamWriter::Serialise
//
//==========================================================================
void VBitStreamWriter::Serialise (void *data, int length) {
  SerialiseBits(data, length<<3);
}


//==========================================================================
//
//  VBitStreamWriter::SerialiseBits
//
//==========================================================================
void VBitStreamWriter::SerialiseBits (void *Src, int Length) {
  if (!Length) return;
  vassert(Length > 0);

  #if 1
  if (Pos+Length > Max) {
    if (!bAllowExpand) { bError = true; return; }
    // do it slow
    const vuint8 *sb = (const vuint8 *)Src;
    while (Length > 0) {
      vuint8 currByte = *sb++;
      for (int f = 0; f < 8 && Length > 0; ++f, --Length) {
        WriteBit(!!(currByte&0x80));
        currByte <<= 1;
      }
    }
    return;
  }

  if (Length <= 8) {
    const unsigned Byte1 = ((unsigned)Pos)>>3;
    const unsigned Byte2 = ((unsigned)(Pos+Length-1))>>3;

    const vuint8 Val = ((const vuint8 *)Src)[0]&((1u<<(Length&0x1f))-1);
    const unsigned Shift = Pos&7;
    if (Byte1 == Byte2) {
      Data[Byte1] |= Val<<Shift;
    } else {
      Data[Byte1] |= Val<<Shift;
      Data[Byte2] |= Val>>(8-Shift);
    }
    Pos += Length;
    return;
  }

  const unsigned Bytes = ((unsigned)Length)>>3;
  if (Bytes) {
    if (Pos&7) {
      const vuint8 *pSrc = (const vuint8 *)Src;
      vuint8 *pDst = (vuint8*)Data.Ptr()+(Pos>>3);
      for (unsigned i = 0; i < Bytes; ++i, ++pSrc, ++pDst) {
        pDst[0] |= *pSrc<<(Pos&7);
        pDst[1] |= *pSrc>>(8-(Pos&7));
      }
    } else {
      memcpy(Data.Ptr()+((Pos+7)>>3), Src, Length>>3);
    }
    Pos += Length&~7;
  }

  if (Length&7) {
    const unsigned Byte1 = ((unsigned)Pos)>>3;
    const unsigned Byte2 = ((unsigned)(Pos+(Length&7)-1))>>3;
    const vuint8 Val = ((const vuint8 *)Src)[Length>>3]&((1<<(Length&7))-1);
    const unsigned Shift = Pos&7;
    if (Byte1 == Byte2) {
      Data[Byte1] |= Val<<Shift;
    } else {
      Data[Byte1] |= Val<<Shift;
      Data[Byte2] |= Val>>(8-Shift);
    }
    Pos += Length&7;
  }
  #else
  // do it slow
  const vuint8 *sb = (const vuint8 *)Src;
  while (Length > 0) {
    const vuint8 currByte = *sb++;
    vuint8 mask = 1u;
    while (mask && Length-- > 0) {
      WriteBit(currByte&mask);
      mask <<= 1;
    }
  }
  #endif
}


//==========================================================================
//
//  VBitStreamWriter::SerialiseInt
//
//==========================================================================
void VBitStreamWriter::SerialiseInt (vuint32 &Value) {
  WriteInt(Value);
}


//==========================================================================
//
//  VBitStreamWriter::WriteInt
//
//==========================================================================
void VBitStreamWriter::WriteInt (vint32 IVal) {
  // sign bit
  vuint32 Val = (vuint32)IVal;
  if (Val&0x80000000u) {
    WriteBit(true);
    Val ^= 0xffffffffu;
  } else {
    WriteBit(false);
  }
  // bytes
  while (Val) {
    WriteBit(true);
    for (int cnt = 4; cnt > 0; --cnt) {
      WriteBit(!!(Val&0x01));
      Val >>= 1;
    }
  }
  WriteBit(false); // stop bit
}


//==========================================================================
//
//  VBitStreamWriter::WriteUInt
//
//==========================================================================
void VBitStreamWriter::WriteUInt (vuint32 Val) {
  // bytes
  while (Val) {
    WriteBit(true);
    for (int cnt = 4; cnt > 0; --cnt) {
      WriteBit(!!(Val&0x01));
      Val >>= 1;
    }
  }
  WriteBit(false); // stop bit
}


//**************************************************************************
//
// VBitStreamReader
//
//**************************************************************************

//==========================================================================
//
//  VBitStreamReader::VBitStreamReader
//
//==========================================================================
VBitStreamReader::VBitStreamReader (vuint8 *Src, vint32 Length)
  : Num(Length)
  , Pos(0)
{
  bLoading = true;
  Data.SetNum((Length+7)>>3);
  if (Src) memcpy(Data.Ptr(), Src, (Length+7)>>3);
}


//==========================================================================
//
//  VBitStreamReader::~VBitStreamReader
//
//==========================================================================
VBitStreamReader::~VBitStreamReader () {
  Close();
}


//==========================================================================
//
//  VBitStreamReader::cloneFrom
//
//==========================================================================
void VBitStreamReader::cloneFrom (const VBitStreamReader *rd) {
  if (!rd || rd == this) return;
  Data = rd->Data;
  Num = rd->Num;
  Pos = rd->Pos;
  bError = false;
}


//==========================================================================
//
//  VBitStreamReader::SetData
//
//==========================================================================
void VBitStreamReader::SetData (VBitStreamReader &Src, int Length) noexcept {
  vassert(Length >= 0);
  if (Src.IsError() || Src.GetNumBits()-Src.Pos < Length) { bError = true; return; }
  bError = false;
  Pos = 0;
  Data.SetNum((Length+7)>>3);
  if (Data.length()) memset(Data.ptr(), 0, Data.length());
  Num = Length;
  if (Length) Src.SerialiseBits(Data.ptr(), Length);
  if (Src.IsError()) bError = true;
  /*
  if (Src.Pos&7) {
    // slower, alas
    const vuint8 *s = (const vuint8 *)Src.Data.ptr();
    s += Src.Pos>>3;
    vuint8 smask = 1u<<(Src.Pos&7);
    vuint8 *d = (vuint8 *)Data.ptr();
    vuint8 dmask = 1u;
    while (Length--) {
      if (s[0]&smask) d[0] |= dmask;
      if ((smask<<=1) == 0) { smask = 1u; ++s; }
      if ((dmask<<=1) == 0) { dmask = 1u; ++d; }
    }
    Src.Pos += Length;
  } else {
    // faster
    if (Length) Src.SerialiseBits(Data.Ptr()+(Src.Pos>>3), Length);
  }
  */
}


//==========================================================================
//
//  VBitStreamReader::Serialise
//
//==========================================================================
void VBitStreamReader::Serialise (void *AData, int ALen) {
  SerialiseBits(AData, ALen<<3);
}


//==========================================================================
//
//  VBitStreamReader::SerialiseBits
//
//==========================================================================
void VBitStreamReader::SerialiseBits (void *Dst, int Length) {
  if (!Length) return;

  if (Pos+Length > Num) {
    bError = true;
    memset(Dst, 0, (Length+7)>>3);
    return;
  }

  #if 1
  if (Pos&7) {
    unsigned SrcPos = Pos>>3;
    const unsigned Shift1 = Pos&7;
    const unsigned Shift2 = 8-Shift1;
    const unsigned Count = ((unsigned)Length)>>3;
    for (unsigned i = 0; i < Count; ++i, ++SrcPos) {
      ((vuint8 *)Dst)[i] = (Data[SrcPos]>>Shift1)|Data[SrcPos+1]<<Shift2;
    }
    if (Length&7) {
      if ((Length&7) > Shift2) {
        ((vuint8 *)Dst)[Count] = ((Data[SrcPos]>>Shift1)|Data[SrcPos+1]<<Shift2)&((1<<(Length&7))-1);
      } else {
        ((vuint8 *)Dst)[Count] = (Data[SrcPos]>>Shift1)&((1<<(Length&7))-1);
      }
    }
  } else {
    unsigned Count = ((unsigned)Length)>>3;
    memcpy(Dst, Data.Ptr()+(Pos>>3), Count);
    if (Length&7) {
      ((vuint8 *)Dst)[Count] = Data[(Pos>>3)+Count]&((1<<(Length&7))-1);
    }
  }
  Pos += Length;
  #else
  // do it slow
  vuint8 *db = (vuint8 *)Dst;
  while (Length > 0) {
    vuint8 currByte = 0;
    vuint8 mask = 1u;
    while (mask && Length-- > 0) {
      if (ReadBit()) currByte |= mask;
      mask <<= 1;
    }
    *db++ = currByte;
  }
  #endif
}


//==========================================================================
//
//  VBitStreamReader::SerialiseInt
//
//==========================================================================
void VBitStreamReader::SerialiseInt (vuint32 &Value) {
  Value = ReadInt();
}


//==========================================================================
//
//  VBitStreamReader::ReadInt
//
//==========================================================================
vint32 VBitStreamReader::ReadInt () {
  bool sign = ReadBit();
  vuint32 Val = 0, Mask = 1u;
  // bytes
  while (ReadBit()) {
    for (int cnt = 4; cnt > 0; --cnt) {
      vassert(Mask);
      if (ReadBit()) Val |= Mask;
      Mask <<= 1;
    }
  }
  if (sign) Val ^= 0xffffffffu;
  return (vint32)Val;
}


//==========================================================================
//
//  VBitStreamReader::ReadUInt
//
//==========================================================================
vuint32 VBitStreamReader::ReadUInt () {
  vuint32 Val = 0, Mask = 1u;
  // bytes
  while (ReadBit()) {
    for (int cnt = 4; cnt > 0; --cnt) {
      vassert(Mask);
      if (ReadBit()) Val |= Mask;
      Mask <<= 1;
    }
  }
  return Val;
}


//==========================================================================
//
//  VBitStreamReader::AtEnd
//
//==========================================================================
bool VBitStreamReader::AtEnd () {
  return (bError || Pos >= Num);
}


//==========================================================================
//
//  VBitStreamReader::SetupFrom
//
//  if `FixWithTrailingBit` is true, shrink with the last
//  trailing bit (including it)
//
//==========================================================================
void VBitStreamReader::SetupFrom (const vuint8 *data, vint32 len, bool FixWithTrailingBit) noexcept {
  vassert(len >= 0);
  bError = false;
  Num = len;
  Pos = 0;
  const int byteLen = (len+7)>>3;
  Data.setLength(byteLen);
  if (byteLen) {
    if (data) memcpy(Data.ptr(), data, byteLen); else memset(Data.ptr(), 0, byteLen);
  }
  if (len > 0 && FixWithTrailingBit) {
    vassert(data);
    vuint8 b = data[byteLen-1];
    if (!b) { bError = true; return; } // oops
    --Num;
    while ((b&0x80u) == 0) { --Num; b <<= 1; }
    if (Num < 0) { Num = 0; bError = true; return; } // oops
  }
}


//==========================================================================
//
//  VBitStreamReader::CopyFromBuffer
//
//==========================================================================
void VBitStreamReader::CopyFromBuffer (const vuint8 *buf, int bitLength) noexcept {
  vassert(bitLength >= 0);
  if (!bitLength) return;
  vassert(buf);
  int oldByteLen = (Num+7)>>3;
  int newByteLen = (Num+bitLength+7)>>3;
  // make buffer bigger
  if (Data.length() < newByteLen) {
    Data.setLength(newByteLen);
    if (newByteLen > oldByteLen) memset(Data.ptr()+oldByteLen, 0, newByteLen-oldByteLen);
  }
  // destination
  vuint8 *d = Data.ptr()+(Num>>3);
  vuint8 dmask = 1u<<(Num&7);
  Num += bitLength;
  // source
  vuint8 smask = 1u;
  // copy
  while (bitLength-- > 0) {
    if (buf[0]&smask) d[0] |= dmask; else d[0] &= ~dmask;
    if ((smask<<=1) == 0) { smask = 1u; ++buf; }
    if ((dmask<<=1) == 0) { dmask = 1u; ++d; }
  }
}
