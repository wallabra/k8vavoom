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
#include "core.h"


//==========================================================================
//
//  VMemoryStreamRO::VMemoryStreamRO
//
//==========================================================================
VMemoryStreamRO::VMemoryStreamRO ()
  : Data(nullptr)
  , Pos(0)
  , DataSize(0)
  , FreeData(false)
{
  bLoading = true;
}


//==========================================================================
//
//  VMemoryStreamRO::VMemoryStreamRO
//
//==========================================================================
VMemoryStreamRO::VMemoryStreamRO (VStr strmName, const void *adata, int adataSize, bool takeOwnership)
  : Data((const vuint8 *)adata)
  , Pos(0)
  , DataSize(adataSize)
  , FreeData(takeOwnership)
  , StreamName(strmName)
{
  vassert(adataSize >= 0);
  vassert(adataSize == 0 || (adataSize > 0 && adata));
  bLoading = true;
}


//==========================================================================
//
//  VMemoryStreamRO::VMemoryStreamRO
//
//==========================================================================
VMemoryStreamRO::VMemoryStreamRO (VStr strmName, VStream *strm)
  : Data(nullptr)
  , Pos(0)
  , DataSize(0)
  , FreeData(false)
  , StreamName()
{
  bLoading = true;
  Setup(strmName, strm);
}


//==========================================================================
//
//  VMemoryStreamRO::VMemoryStreamRO
//
//==========================================================================
VMemoryStreamRO::~VMemoryStreamRO () {
  Close();
}


//==========================================================================
//
//  VMemoryStreamRO::Clear
//
//==========================================================================
void VMemoryStreamRO::Clear () {
  if (FreeData && Data) Z_Free((void *)Data);
  Data = nullptr;
  Pos = 0;
  DataSize = 0;
  FreeData = false;
  bError = false;
  StreamName.clear();
}


//==========================================================================
//
//  VMemoryStreamRO::Setup
//
//==========================================================================
void VMemoryStreamRO::Setup (VStr strmName, const void *adata, int adataSize, bool takeOwnership) {
  vassert(!Data);
  vassert(Pos == 0);
  vassert(DataSize == 0);
  vassert(!bError);
  vassert(adataSize >= 0);
  vassert(adataSize == 0 || (adataSize > 0 && adata));
  vassert(StreamName.length() == 0);
  StreamName = strmName;
  Data = (const vuint8 *)adata;
  DataSize = adataSize;
  FreeData = takeOwnership;
}


//==========================================================================
//
//  VMemoryStreamRO::Setup
//
//  from current position to stream end
//
//==========================================================================
void VMemoryStreamRO::Setup (VStr strmName, VStream *strm) {
  vassert(!Data);
  vassert(Pos == 0);
  vassert(DataSize == 0);
  vassert(!bError);
  vassert(StreamName.length() == 0);
  StreamName = strmName;
  if (strm) {
    vassert(strm->IsLoading());
    int tsz = strm->TotalSize();
    vassert(tsz >= 0);
    int cpos = strm->Tell();
    vassert(cpos >= 0);
    if (cpos < tsz) {
      int len = tsz-cpos;
      void *dta = Z_Malloc(len);
      strm->Serialize(dta, len);
      bError = strm->IsError();
      if (bError) {
        Z_Free(dta);
      } else {
        Data = (const vuint8 *)dta;
        DataSize = len;
        FreeData = true;
      }
    }
  }
}


//==========================================================================
//
//  VMemoryStreamRO::Serialise
//
//==========================================================================
void VMemoryStreamRO::Serialise (void *buf, int Len) {
  vassert(bLoading);
  vassert(Len >= 0);
  if (Len == 0) return;
  vassert(buf);
  if (Pos >= DataSize) {
    bError = true;
    return;
  }
  if (Len > DataSize-Pos) {
    // too much
    Len = DataSize-Pos;
    bError = true;
  }
  if (Len) {
    memcpy(buf, Data+Pos, Len);
    Pos += Len;
  }
}


//==========================================================================
//
//  VMemoryStreamRO::Seek
//
//==========================================================================
void VMemoryStreamRO::Seek (int InPos) {
  if (InPos < 0) {
    bError = true;
    Pos = 0;
  } else if (InPos > DataSize) {
    bError = true;
    Pos = DataSize;
  } else {
    Pos = InPos;
  }
}


//==========================================================================
//
//  VMemoryStreamRO::Tell
//
//==========================================================================
int VMemoryStreamRO::Tell () {
  return Pos;
}


//==========================================================================
//
//  VMemoryStreamRO::TotalSize
//
//==========================================================================
int VMemoryStreamRO::TotalSize () {
  return DataSize;
}


//==========================================================================
//
//  VMemoryStreamRO::GetName
//
//==========================================================================
VStr VMemoryStreamRO::GetName () const {
  return StreamName.cloneUnique();
}



//==========================================================================
//
//  VMemoryStream::VMemoryStream
//
//==========================================================================
VMemoryStream::VMemoryStream ()
  : Pos(0)
  , StreamName()
{
  bLoading = false;
}


//==========================================================================
//
//  VMemoryStream::VMemoryStream
//
//==========================================================================
VMemoryStream::VMemoryStream (VStr strmName)
  : Pos(0)
  , StreamName(strmName)
{
  bLoading = false;
}


//==========================================================================
//
//  VMemoryStream::VMemoryStream
//
//==========================================================================
VMemoryStream::VMemoryStream (VStr strmName, const void *InData, int InLen, bool takeOwnership)
  : Pos(0)
  , StreamName(strmName)
{
  if (InLen < 0) InLen = 0;
  bLoading = true;
  if (!takeOwnership) {
    Array.SetNum(InLen);
    if (InLen) memcpy(Array.Ptr(), InData, InLen);
  } else {
    Array.SetPointerData((void *)InData, InLen);
    vassert(Array.length() == InLen);
  }
}


//==========================================================================
//
//  VMemoryStream::VMemoryStream
//
//==========================================================================
VMemoryStream::VMemoryStream (VStr strmName, const TArray<vuint8> &InArray)
  : Pos(0)
  , StreamName(strmName)
{
  bLoading = true;
  Array = InArray;
}


//==========================================================================
//
//  VMemoryStream::VMemoryStream
//
//==========================================================================
VMemoryStream::VMemoryStream (VStr strmName, VStream *strm)
  : Pos(0)
  , StreamName(strmName)
{
  bLoading = true;
  if (strm) {
    vassert(strm->IsLoading());
    int tsz = strm->TotalSize();
    vassert(tsz >= 0);
    int cpos = strm->Tell();
    vassert(cpos >= 0);
    if (cpos < tsz) {
      int len = tsz-cpos;
      Array.setLength(len);
      strm->Serialize(Array.ptr(), len);
      bError = strm->IsError();
    }
  }
}


//==========================================================================
//
//  ~VMemoryStream::~VMemoryStream
//
//==========================================================================
VMemoryStream::~VMemoryStream () {
  Close();
}


//==========================================================================
//
//  VMemoryStream::Serialise
//
//==========================================================================
void VMemoryStream::Serialise (void *Data, int Len) {
  vassert(Len >= 0);
  if (Len == 0) return;
  const int alen = Array.length();
  if (bLoading) {
    if (Pos >= alen) {
      bError = true;
      return;
    }
    if (Len > alen-Pos) {
      // too much
      Len = alen-Pos;
      bError = true;
    }
    if (Len) {
      memcpy(Data, &Array[Pos], Len);
      Pos += Len;
    }
  } else {
    if (Pos+Len > alen) Array.SetNumWithReserve(Pos+Len);
    memcpy(&Array[Pos], Data, Len);
    Pos += Len;
  }
}


//==========================================================================
//
//  VMemoryStream::Seek
//
//==========================================================================
void VMemoryStream::Seek (int InPos) {
  if (InPos < 0) {
    bError = true;
    Pos = 0;
  } else if (InPos > Array.length()) {
    bError = true;
    Pos = Array.length();
  } else {
    Pos = InPos;
  }
}


//==========================================================================
//
//  VMemoryStream::Tell
//
//==========================================================================
int VMemoryStream::Tell () {
  return Pos;
}


//==========================================================================
//
//  VMemoryStream::TotalSize
//
//==========================================================================
int VMemoryStream::TotalSize () {
  return Array.length();
}


//==========================================================================
//
//  VMemoryStream::GetName
//
//==========================================================================
VStr VMemoryStream::GetName () const {
  return StreamName.cloneUnique();
}



//==========================================================================
//
//  VArrayStream::VArrayStream
//
//==========================================================================
VArrayStream::VArrayStream (VStr strmName, TArray<vuint8>& InArray)
  : Array(InArray)
  , Pos(0)
  , StreamName(strmName)
{
  bLoading = true;
}


//==========================================================================
//
//  VArrayStream::~VArrayStream
//
//==========================================================================
VArrayStream::~VArrayStream () {
  Close();
}


//==========================================================================
//
//  VArrayStream::Serialise
//
//==========================================================================
void VArrayStream::Serialise (void *Data, int Len) {
  vassert(Len >= 0);
  if (Len == 0) return;
  const int alen = Array.length();
  if (bLoading) {
    if (Pos >= alen) {
      bError = true;
      return;
    }
    if (Len > alen-Pos) {
      // too much
      Len = alen-Pos;
      bError = true;
    }
    if (Len) {
      memcpy(Data, &Array[Pos], Len);
      Pos += Len;
    }
  } else {
    if (Pos+Len > alen) Array.SetNumWithReserve(Pos+Len);
    memcpy(&Array[Pos], Data, Len);
    Pos += Len;
  }
}


//==========================================================================
//
//  VArrayStream::Seek
//
//==========================================================================
void VArrayStream::Seek (int InPos) {
  if (InPos < 0) {
    bError = true;
    Pos = 0;
  } else if (InPos > Array.length()) {
    bError = true;
    Pos = Array.length();
  } else {
    Pos = InPos;
  }
}


//==========================================================================
//
//  VArrayStream::Tell
//
//==========================================================================
int VArrayStream::Tell () {
  return Pos;
}


//==========================================================================
//
//  VArrayStream::TotalSize
//
//==========================================================================
int VArrayStream::TotalSize () {
  return Array.length();
}


//==========================================================================
//
//  VArrayStream::GetName
//
//==========================================================================
VStr VArrayStream::GetName () const {
  return StreamName.cloneUnique();
}



//==========================================================================
//
//  VPagedMemoryStream::VPagedMemoryStream
//
//  initialise empty writing stream
//
//==========================================================================
VPagedMemoryStream::VPagedMemoryStream (VStr strmName)
  : first(nullptr)
  , curr(nullptr)
  , pos(0)
  , size(0)
  , StreamName(strmName)
{
  bLoading = false;
}


//==========================================================================
//
//  VPagedMemoryStream::~VPagedMemoryStream
//
//==========================================================================
VPagedMemoryStream::~VPagedMemoryStream () {
  Close();
}


//==========================================================================
//
//  VPagedMemoryStream::Close
//
//==========================================================================
bool VPagedMemoryStream::Close () {
  while (first) {
    vuint8 *next = *(vuint8 **)first;
    Z_Free(first);
    first = next;
  }
  first = curr = nullptr;
  pos = size = 0;
  return !bError;
}


//==========================================================================
//
//  VPagedMemoryStream::GetName
//
//==========================================================================
VStr VPagedMemoryStream::GetName () const {
  return StreamName.cloneUnique();
}


//==========================================================================
//
//  VPagedMemoryStream::Serialise
//
//==========================================================================
void VPagedMemoryStream::Serialise (void *bufp, int count) {
  vassert(count >= 0);
  if (count == 0 || bError) return;
  int leftInPage = DataPerPage-pos%DataPerPage;
  vuint8 *buf = (vuint8 *)bufp;
  if (bLoading) {
    // loading
    if (pos >= size) { bError = true; return; }
    if (count > size-pos) { count = size-pos; bError = true; }
    while (count > 0) {
      if (count <= leftInPage) {
        memcpy(buf, curr+sizeof(vuint8 *)+pos%DataPerPage, count);
        pos += count;
        break;
      }
      memcpy(buf, curr+sizeof(vuint8 *)+pos%DataPerPage, leftInPage);
      pos += leftInPage;
      count -= leftInPage;
      buf += leftInPage;
      vassert(pos%DataPerPage == 0);
      curr = *(vuint8 **)curr; // next page
      leftInPage = DataPerPage; // it is safe to overestimate here
    }
  } else {
    // writing
    while (count > 0) {
      if (leftInPage == DataPerPage) {
        // need new page
        if (first) {
          if (pos != 0) {
            vuint8 *next = *(vuint8 **)curr;
            if (!next) {
              // allocate next page
              next = (vuint8 *)Z_Malloc(FullPageSize);
              *(vuint8 **)next = nullptr; // no next page
              *(vuint8 **)curr = next; // pointer to next page
            }
            curr = next;
          } else {
            curr = first;
          }
        } else {
          // allocate first page
          first = curr = (vuint8 *)Z_Malloc(FullPageSize);
          *(vuint8 **)first = nullptr; // no next page
        }
      }
      if (count <= leftInPage) {
        memcpy(curr+sizeof(vuint8 *)+pos%DataPerPage, buf, count);
        pos += count;
        break;
      }
      memcpy(curr+sizeof(vuint8 *)+pos%DataPerPage, buf, leftInPage);
      pos += leftInPage;
      count -= leftInPage;
      buf += leftInPage;
      vassert(pos%DataPerPage == 0);
      leftInPage = DataPerPage;
    }
    if (size < pos) size = pos;
  }
}


//==========================================================================
//
//  VPagedMemoryStream::Seek
//
//==========================================================================
void VPagedMemoryStream::Seek (int newpos) {
  if (bError) return;
  if (newpos < 0 || newpos > size) { bError = true; return; }
  if (bLoading) {
    // loading
  } else {
    // writing is somewhat special:
    // we don't have to go to next page if pos is at its first byte
    if (newpos <= DataPerPage) {
      // in the first page
      curr = first;
    } else {
      // walk pages
      int pgcount = newpos/DataPerPage;
      // if we are at page boundary, do one step less
      if (newpos%DataPerPage == 0) {
        --pgcount;
        vassert(pgcount > 0);
      }
      curr = first;
      while (pgcount--) curr = *(vuint8 **)curr;
    }
  }
  pos = newpos;
}


//==========================================================================
//
//  VPagedMemoryStream::Tell
//
//==========================================================================
int VPagedMemoryStream::Tell () {
  return pos;
}


//==========================================================================
//
//  VPagedMemoryStream::TotalSize
//
//==========================================================================
int VPagedMemoryStream::TotalSize () {
  return size;
}


void VPagedMemoryStream::CopyTo (VStream *strm) {
  if (!strm) return;
  int left = size;
  vuint8 *cpg = first;
  while (left > 0) {
    int wrt = (left > DataPerPage ? DataPerPage : left);
    strm->Serialise(cpg+sizeof(vuint8 *), wrt);
    if (strm->IsError()) return;
    left -= wrt;
    cpg = *(vuint8 **)cpg; // next page
  }
}



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
//  VBitStreamWriter::WriteBit
//
//==========================================================================
void VBitStreamWriter::WriteBit (bool Bit) noexcept {
  if (bError) return;
  if (Pos+1 > Max) {
    if (!bAllowExpand) { bError = true; return; }
    if ((Pos+7)/8+1 > Data.length()) {
      if (!Expand()) { bError = true; return; }
    }
  }
  if (Bit) Data.ptr()[Pos>>3] |= 1<<(Pos&7);
  ++Pos;
}


//==========================================================================
//
//  VBitStreamWriter::CopyFromWS
//
//==========================================================================
void VBitStreamWriter::CopyFromWS (const VBitStreamWriter &strm) noexcept {
  if (strm.Pos == 0) return;
  const vuint8 *src = (const vuint8 *)strm.Data.ptr();
  for (int pos = 0; pos < strm.Pos; ++pos) {
    WriteBit(src[pos>>3]&(1<<(pos&7)));
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
    int Byte1 = Pos>>3;
    int Byte2 = (Pos+Length-1)>>3;

    vuint8 Val = ((vuint8*)Src)[0]&((1<<Length)-1);
    int Shift = Pos&7;
    if (Byte1 == Byte2) {
      Data[Byte1] |= Val<<Shift;
    } else {
      Data[Byte1] |= Val<<Shift;
      Data[Byte2] |= Val>>(8-Shift);
    }
    Pos += Length;
    return;
  }

  int Bytes = Length>>3;
  if (Bytes) {
    if (Pos&7) {
      vuint8 *pSrc = (vuint8*)Src;
      vuint8 *pDst = (vuint8*)Data.Ptr()+(Pos>>3);
      for (int i = 0; i < Bytes; ++i, ++pSrc, ++pDst) {
        pDst[0] |= *pSrc<<(Pos&7);
        pDst[1] |= *pSrc>>(8-(Pos&7));
      }
    } else {
      memcpy(Data.Ptr()+((Pos+7)>>3), Src, Length>>3);
    }
    Pos += Length&~7;
  }

  if (Length&7) {
    int Byte1 = Pos>>3;
    int Byte2 = (Pos+(Length&7)-1)>>3;
    vuint8 Val = ((vuint8*)Src)[Length>>3]&((1<<(Length&7))-1);
    int Shift = Pos&7;
    if (Byte1 == Byte2) {
      Data[Byte1] |= Val<<Shift;
    } else {
      Data[Byte1] |= Val<<Shift;
      Data[Byte2] |= Val>>(8-Shift);
    }
    Pos += Length&7;
  }
}


//==========================================================================
//
//  VBitStreamWriter::SerialiseInt
//
//==========================================================================
void VBitStreamWriter::SerialiseInt (vuint32 &Value/*, vuint32 Max*/) {
  WriteInt(Value/*, Max*/);
}


//==========================================================================
//
//  VBitStreamWriter::WriteInt
//
//==========================================================================
void VBitStreamWriter::WriteInt (vuint32 Val/*, vuint32 Maximum*/) {
  //vensure(Val < Maximum);
#if 0
  // with maximum of 1 the only possible value is 0
  if (Maximum <= 1) return;

  // check for the case when it will take all 32 bits
  if (Maximum > 0x80000000) {
    *this << Val;
    return;
  }

  for (vuint32 Mask = 1; Mask && Mask < Maximum; Mask <<= 1) {
    if (Pos+1 > Max) {
      bError = true;
      return;
    }
    if (Val&Mask) Data[Pos>>3] |= 1<<(Pos&7);
    ++Pos;
  }
#else
  // sign bit
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
#endif
}


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
}


//==========================================================================
//
//  VBitStreamReader::SetData
//
//==========================================================================
void VBitStreamReader::SetData (VBitStreamReader &Src, int Length) noexcept {
  vassert(Length >= 0);
  Data.SetNum((Length+7)>>3);
  if (Length) Src.SerialiseBits(Data.Ptr(), Length);
  Num = Length;
  Pos = 0;
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

  if (Pos&7) {
    int SrcPos = Pos>>3;
    int Shift1 = Pos&7;
    int Shift2 = 8-Shift1;
    int Count = Length>>3;
    for (int i = 0; i < Count; ++i, ++SrcPos) {
      ((vuint8*)Dst)[i] = (Data[SrcPos]>>Shift1)|Data[SrcPos+1]<<Shift2;
    }
    if (Length&7) {
      if ((Length&7) > Shift2) {
        ((vuint8*)Dst)[Count] = ((Data[SrcPos]>>Shift1)|Data[SrcPos+1]<<Shift2)&((1<<(Length&7))-1);
      } else {
        ((vuint8*)Dst)[Count] = (Data[SrcPos]>>Shift1)&((1<<(Length&7))-1);
      }
    }
  } else {
    int Count = Length>>3;
    memcpy(Dst, Data.Ptr()+(Pos>>3), Count);
    if (Length&7) {
      ((vuint8*)Dst)[Count] = Data[(Pos>>3)+Count]&((1<<(Length&7))-1);
    }
  }
  Pos += Length;
}


//==========================================================================
//
//  VBitStreamReader::SerialiseInt
//
//==========================================================================
void VBitStreamReader::SerialiseInt (vuint32 &Value/*, vuint32 Max*/) {
  Value = ReadInt(/*Max*/);
}


//==========================================================================
//
//  VBitStreamReader::ReadInt
//
//==========================================================================
vuint32 VBitStreamReader::ReadInt (/*vuint32 Maximum*/) {
#if 0
  // with maximum of 1 the only possible value is 0
  if (Maximum <= 1) return 0;

  // check for the case when it will take all 32 bits
  if (Maximum > 0x80000000) return Streamer<vuint32>(*this);

  vuint32 Val = 0;
  for (vuint32 Mask = 1; Mask && Mask < Maximum; Mask <<= 1) {
    if (Pos+1 > Num) {
      bError = true;
      return 0;
    }
    if (Data[Pos>>3]&(1<<(Pos&7))) Val |= Mask;
    ++Pos;
  }
  return Val;
#else
  bool sign = ReadBit();
  vuint32 Val = 0, Mask = 1;
  // bytes
  while (ReadBit()) {
    for (int cnt = 4; cnt > 0; --cnt) {
      vassert(Mask);
      if (ReadBit()) Val |= Mask;
      Mask <<= 1;
    }
  }
  if (sign) Val ^= 0xffffffffu;
  return Val;
#endif
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
//  VStdFileStreamBase::VStdFileStreamBase
//
//==========================================================================
VStdFileStreamBase::VStdFileStreamBase (FILE* afl, VStr aname, bool asWriter)
  : mFl(afl)
  , mName(aname)
  , size(-1)
{
  bLoading = !asWriter;
  if (afl && !asWriter) {
    if (fseek(afl, 0, SEEK_SET)) setError();
  }
}


//==========================================================================
//
//  VStdFileStreamBase::~VStdFileStreamBase
//
//==========================================================================
VStdFileStreamBase::~VStdFileStreamBase () {
  Close();
}


//==========================================================================
//
//  VStdFileStreamBase::Close
//
//==========================================================================
bool VStdFileStreamBase::Close () {
  if (mFl) {
    if (!bLoading) fflush(mFl);
    fclose(mFl);
    mFl = nullptr;
  }
  fflush(stderr);
  mName.clear();
  return !bError;
}


//==========================================================================
//
//  VStdFileStreamBase::setError
//
//==========================================================================
void VStdFileStreamBase::setError () {
  if (mFl) {
    fflush(stderr);
    fclose(mFl);
    mFl = nullptr;
  }
  mName.clear();
  bError = true;
}


//==========================================================================
//
//  VStdFileStreamBase::GetName
//
//==========================================================================
VStr VStdFileStreamBase::GetName () const {
  return mName.cloneUnique();
}


//==========================================================================
//
//  VStdFileStreamBase::Seek
//
//==========================================================================
void VStdFileStreamBase::Seek (int pos) {
  if (!mFl) { setError(); return; }
#ifdef __SWITCH__
  // I don't know how or why this works, but unless you seek to 0 first,
  // fseeking on the Switch seems to set the pointer to an incorrect
  // position, but only sometimes
  fseek(mFl, 0, SEEK_SET);
#endif
  if (fseek(mFl, pos, SEEK_SET)) setError();
}


//==========================================================================
//
//  VStdFileStreamBase::Tell
//
//==========================================================================
int VStdFileStreamBase::Tell () {
  if (mFl && !bError) {
    int res = (int)ftell(mFl);
    if (res < 0) { setError(); return 0; }
    return res;
  } else {
    setError();
    return 0;
  }
}


//==========================================================================
//
//  VStdFileStreamBase::TotalSize
//
//==========================================================================
int VStdFileStreamBase::TotalSize () {
  if (!mFl || bError) { setError(); return 0; }
  if (!IsLoading()) size = -1;
  if (size < 0) {
    auto opos = ftell(mFl);
    fseek(mFl, 0, SEEK_END);
    size = (int)ftell(mFl);
    fseek(mFl, opos, SEEK_SET);
  }
  return size;
}


//==========================================================================
//
//  VStdFileStreamBase::AtEnd
//
//==========================================================================
bool VStdFileStreamBase::AtEnd () {
  return (bError || !mFl || Tell() >= TotalSize());
}


//==========================================================================
//
//  VStdFileStreamBase::Serialise
//
//==========================================================================
void VStdFileStreamBase::Serialise (void *buf, int len) {
  if (bError || !mFl || len < 0) { setError(); return; }
  if (len == 0) return;
  if (bLoading) {
    if (fread(buf, len, 1, mFl) != 1) setError();
  } else {
    if (fwrite(buf, len, 1, mFl) != 1) setError();
  }
}



// ////////////////////////////////////////////////////////////////////////// //
//  VPartialStreamRO::VPartialStreamRO
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VPartialStreamRO::VPartialStreamRO
//
//==========================================================================
VPartialStreamRO::VPartialStreamRO (VStream *ASrcStream, int astpos, int apartlen, bool aOwnSrc)
  : lockptr(nullptr)
  , srcStream(ASrcStream)
  , stpos(astpos)
  , srccurpos(astpos)
  , partlen(apartlen)
  , srcOwned(aOwnSrc)
  , closed(false)
  , myname()
{
  lockptr = &lock;
  mythread_mutex_init(lockptr);
  bLoading = true;
  if (!srcStream) { srcOwned = false; bError = true; return; }
  {
    MyThreadLocker locker(lockptr);
    if (!srcStream->IsLoading()) {
      bError = true;
      partlen = 0;
    } else {
      if (partlen < 0) {
        partlen = srcStream->TotalSize()-stpos;
             if (partlen < 0) { partlen = 0; bError = true; }
        else if (srcStream->IsError()) bError = true;
      }
    }
  }
}


//==========================================================================
//
//  VPartialStreamRO::VPartialStreamRO
//
//==========================================================================
VPartialStreamRO::VPartialStreamRO (VStr aname, VStream *ASrcStream, int astpos, int apartlen, mythread_mutex *alockptr)
  : lockptr(alockptr)
  , srcStream(ASrcStream)
  , stpos(astpos)
  , srccurpos(astpos)
  , partlen(apartlen)
  , srcOwned(false)
  , closed(false)
  , myname(aname)
{
  if (!lockptr) {
    lockptr = &lock;
    mythread_mutex_init(lockptr);
  }
  bLoading = true;
  if (partlen < 0 || !srcStream) { bError = true; return; }
  {
    MyThreadLocker locker(lockptr);
    if (srcStream->IsError() || !srcStream->IsLoading()) { bError = true; return; }
  }
}


//==========================================================================
//
//  VPartialStreamRO::~VPartialStreamRO
//
//==========================================================================
VPartialStreamRO::~VPartialStreamRO () {
  Close();
  if (srcOwned && srcStream) {
    MyThreadLocker locker(lockptr);
    delete srcStream;
  }
  srcOwned = false;
  srcStream = nullptr;
  if (lockptr == &lock) mythread_mutex_destroy(lockptr);
  lockptr = nullptr;
}


//==========================================================================
//
//  VPartialStreamRO::Close
//
//==========================================================================
bool VPartialStreamRO::Close () {
  {
    MyThreadLocker locker(lockptr);
    if (!closed) {
      closed = true;
      myname.clear();
    }
  }
  return !bError;
}


//==========================================================================
//
//  VPartialStreamRO::GetName
//
//==========================================================================
VStr VPartialStreamRO::GetName () const {
  if (!closed) {
    if (!myname.isEmpty()) return myname.cloneUnique();
    if (srcStream) {
      MyThreadLocker locker(lockptr);
      return srcStream->GetName();
    }
  }
  return VStr();
}


//==========================================================================
//
//  VPartialStreamRO::IsError
//
//==========================================================================
bool VPartialStreamRO::IsError () const {
  if (lockptr) {
    MyThreadLocker locker(lockptr);
    return bError;
  } else {
    return bError;
  }
}


//==========================================================================
//
//  VPartialStreamRO::checkValidityCond
//
//==========================================================================
bool VPartialStreamRO::checkValidityCond (bool mustBeTrue) noexcept {
  if (!lockptr) { bError = true; return false; }
  {
    MyThreadLocker locker(lockptr);
    if (!bError) {
      if (!mustBeTrue || closed || !srcStream || srcStream->IsError()) bError = true;
    }
    if (!bError) {
      if (srccurpos < stpos || srccurpos > stpos+partlen) bError = true;
    }
    if (bError) return false;
  }
  return true;
}


//==========================================================================
//
//  VPartialStreamRO::Serialise
//
//==========================================================================
void VPartialStreamRO::Serialise (void *buf, int len) {
  if (!checkValidityCond(len >= 0)) return;
  if (len == 0) return;
  {
    MyThreadLocker locker(lockptr);
    if (stpos+partlen-srccurpos < len) { bError = true; return; }
    if (srcStream->Tell() != srccurpos) srcStream->Seek(srccurpos);
    if (srcStream->IsError()) { bError = true; return; }
    srcStream->Serialise(buf, len);
    if (srcStream->IsError()) { bError = true; return; }
    srccurpos += len;
  }
}


//==========================================================================
//
//  VPartialStreamRO::SerialiseBits
//
//==========================================================================
void VPartialStreamRO::SerialiseBits (void *Data, int Length) {
  if (!checkValidityCond(Length >= 0)) return;
  {
    MyThreadLocker locker(lockptr);
    if (srcStream->Tell() != srccurpos) srcStream->Seek(srccurpos);
    if (srcStream->IsError()) { bError = true; return; }
    srcStream->SerialiseBits(Data, Length);
    int cpos = srcStream->Tell();
    if (srcStream->IsError()) { bError = true; return; }
    if (cpos < stpos) { srccurpos = stpos; bError = true; return; }
    if (cpos > stpos+partlen) { srccurpos = stpos+partlen; bError = true; return; }
    srccurpos = cpos-stpos;
  }
}


void VPartialStreamRO::SerialiseInt (vuint32 &Value/*, vuint32 Max*/) {
  if (!checkValidity()) return;
  {
    MyThreadLocker locker(lockptr);
    if (srcStream->Tell() != srccurpos) srcStream->Seek(srccurpos);
    if (srcStream->IsError()) { bError = true; return; }
    srcStream->SerialiseInt(Value/*, Max*/);
    int cpos = srcStream->Tell();
    if (srcStream->IsError()) { bError = true; return; }
    if (cpos < stpos) { srccurpos = stpos; bError = true; return; }
    if (cpos > stpos+partlen) { srccurpos = stpos+partlen; bError = true; return; }
    srccurpos = cpos-stpos;
  }
}


//==========================================================================
//
//  VPartialStreamRO::Seek
//
//==========================================================================
void VPartialStreamRO::Seek (int pos) {
  if (!checkValidityCond(pos >= 0 && pos <= partlen)) return;
  {
    MyThreadLocker locker(lockptr);
    srccurpos = stpos+pos;
  }
}


//==========================================================================
//
//  VPartialStreamRO::Tell
//
//==========================================================================
int VPartialStreamRO::Tell () {
  if (!checkValidity()) return 0;
  {
    MyThreadLocker locker(lockptr);
    return (srccurpos-stpos);
  }
}


//==========================================================================
//
//  VPartialStreamRO::TotalSize
//
//==========================================================================
int VPartialStreamRO::TotalSize () {
  if (!checkValidity()) return 0;
  {
    MyThreadLocker locker(lockptr);
    return partlen;
  }
}


//==========================================================================
//
//  VPartialStreamRO::AtEnd
//
//==========================================================================
bool VPartialStreamRO::AtEnd () {
  if (!checkValidity()) return true;
  {
    MyThreadLocker locker(lockptr);
    return (srccurpos >= stpos+partlen);
  }
}


//==========================================================================
//
//  VPartialStreamRO::Flush
//
//==========================================================================
void VPartialStreamRO::Flush () {
  if (!checkValidity()) return;
  {
    MyThreadLocker locker(lockptr);
    srcStream->Flush();
    if (srcStream->IsError()) bError = true;
  }
}


#define PARTIAL_DO_IO()  do { \
  if (!checkValidity()) return; \
  { \
    MyThreadLocker locker(lockptr); \
    if (srcStream->Tell() != srccurpos) srcStream->Seek(srccurpos); \
    if (srcStream->IsError()) { bError = true; return; } \
    srcStream->io(v); \
    int cpos = srcStream->Tell(); \
    if (srcStream->IsError()) { bError = true; return; } \
    if (cpos < stpos) { srccurpos = stpos; bError = true; return; } \
    if (cpos > stpos+partlen) { srccurpos = stpos+partlen; bError = true; return; } \
    srccurpos = cpos-stpos; \
  } \
} while (0)


//==========================================================================
//
//  VPartialStreamRO::io
//
//==========================================================================
void VPartialStreamRO::io (VName &v) {
  PARTIAL_DO_IO();
}


//==========================================================================
//
//  VPartialStreamRO::io
//
//==========================================================================
void VPartialStreamRO::io (VStr &v) {
  PARTIAL_DO_IO();
}


//==========================================================================
//
//  VPartialStreamRO::io
//
//==========================================================================
void VPartialStreamRO::io (VObject *&v) {
  PARTIAL_DO_IO();
}


//==========================================================================
//
//  VPartialStreamRO::io
//
//==========================================================================
void VPartialStreamRO::io (VMemberBase *&v) {
  PARTIAL_DO_IO();
}


//==========================================================================
//
//  VPartialStreamRO::io
//
//==========================================================================
void VPartialStreamRO::io (VSerialisable *&v) {
  PARTIAL_DO_IO();
}


//==========================================================================
//
//  VPartialStreamRO::SerialiseStructPointer
//
//==========================================================================
void VPartialStreamRO::SerialiseStructPointer (void *&Ptr, VStruct *Struct) {
  if (!checkValidityCond(!!Ptr && !!Struct)) return;
  {
    MyThreadLocker locker(lockptr);
    if (srcStream->Tell() != srccurpos) srcStream->Seek(srccurpos);
    if (srcStream->IsError()) { bError = true; return; }
    srcStream->SerialiseStructPointer(Ptr, Struct);
    int cpos = srcStream->Tell();
    if (srcStream->IsError()) { bError = true; return; }
    if (cpos < stpos) { srccurpos = stpos; bError = true; return; }
    if (cpos > stpos+partlen) { srccurpos = stpos+partlen; bError = true; return; }
    srccurpos = cpos-stpos;
  }
}
