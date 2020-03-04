//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
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
#include "gamedefs.h"
#include "network.h"
#include "net_message.h"

//#define VAVOOM_NET_DEBUG_INMESSAGE
//#define VAVOOM_NET_DEBUG_OUTMESSAGE


//==========================================================================
//
//  VMessageIn::LoadFrom
//
//==========================================================================
bool VMessageIn::LoadFrom (VBitStreamReader &srcPacket) {
  // clear stream
  Data.reset();
  Num = Pos = 0;
  bError = false;
  // read 14-bit length (it includes 2 bytes for the length itself)
  vuint16 v = 0;
  for (int f = 0; f < 14; ++f) {
    if (srcPacket.IsError()) { bError = true; return false; }
    v <<= 1;
    if (srcPacket.ReadBit()) v |= 0x01u;
  }
  if (srcPacket.IsError() || IsError() || v < 14) { bError = true; return false; }
  v -= 14;
  #ifdef VAVOOM_NET_DEBUG_INMESSAGE
  GCon->Logf(NAME_Debug, "*** inmessage size: %u (left %u)", v, srcPacket.GetNumBits()-srcPacket.GetPos());
  #endif
  if (srcPacket.GetNumBits() < v) {
    GCon->Log(NAME_Debug, "*** inmessage: out of data in packet!");
    bError = true;
    return false;
  }
  SetData(srcPacket, v);
  if (srcPacket.IsError() || IsError()) {
    GCon->Logf(NAME_Debug, "*** inmessage: error reading data from packet (%d : %d)!", (int)srcPacket.IsError(), (int)IsError());
    bError = true;
    return false;
  }
  // parse header
  // message flags
  bOpen = ReadBit();
  bClose = ReadBit();
  // channel index
  *this << STRM_INDEX(ChanIndex);
  #ifdef VAVOOM_NET_DEBUG_INMESSAGE
  GCon->Logf(NAME_Debug, "*** inmessage: cidx=%d", ChanIndex);
  #endif
  if (ChanIndex < 0) {
    bError = true;
    return false;
  }
  #ifdef VAVOOM_NET_DEBUG_INMESSAGE
  GCon->Logf(NAME_Debug, "*** inmessage: bOpen=%d; bClose=%d; cidx=%d", (int)bOpen, (int)bClose, ChanIndex);
  #endif
  // optional channel type
  if (bOpen) {
    vuint32 ctype = ReadUInt();
    #ifdef VAVOOM_NET_DEBUG_INMESSAGE
    GCon->Logf(NAME_Debug, "*** inmessage: ctype=%u", ctype);
    #endif
    if (ctype >= CHANNEL_MAX) {
      bError = true;
      return false;
    }
    ChanType = ctype;
  } else {
    ChanType = 0; // unknown
  }
  return !IsError();
}



//==========================================================================
//
//  VMessageOut::VMessageOut
//
//==========================================================================
VMessageOut::VMessageOut (VChannel *AChannel, unsigned flags)
  : VBitStreamWriter(MAX_MSG_SIZE_BITS+16, false) // no expand
  , hdrSizeBits(0)
  , msgflags(0)
{
  vassert(AChannel);
  writeHeader(AChannel->Type, AChannel->Index, flags);
}


//==========================================================================
//
//  VMessageOut::VMessageOut
//
//==========================================================================
VMessageOut::VMessageOut (vuint8 AChanType, int AChanIndex, unsigned flags)
  : VBitStreamWriter(MAX_MSG_SIZE_BITS+16, false) // no expand
  , hdrSizeBits(0)
  , msgflags(0)
{
  writeHeader(AChanType, AChanIndex, flags);
}


//==========================================================================
//
//  VMessageOut::Reset
//
//==========================================================================
void VMessageOut::Reset (VChannel *AChannel, unsigned flags) {
  vassert(AChannel);
  Clear();
  hdrSizeBits = 0;
  writeHeader(AChannel->Type, AChannel->Index, flags);
}


//==========================================================================
//
//  VMessageOut::writeHeader
//
//==========================================================================
void VMessageOut::writeHeader (vuint8 AChanType, int AChanIndex, unsigned flags) {
  vassert(hdrSizeBits == 0);
  //if (flags&NoHeader) return;
  vassert(GetNumBits() == 0);
  vassert(AChanIndex >= 0);
  // reserve room for size (14 bits)
  for (int f = 0; f < 14; ++f) this->WriteBit(false);
  vassert(GetNumBits() == 14);
  // message flags
  this->WriteBit(!!(flags&Open));
  vassert(Pos == 15);
  this->WriteBit(!!(flags&Close));
  // message index
  *this << STRM_INDEX(AChanIndex);
  // optional channel type
  if (flags&Open) this->WriteUInt(AChanType);
  hdrSizeBits = GetNumBits();
  msgflags = flags;
}


//==========================================================================
//
//  VMessageOut::MarkClose
//
//==========================================================================
void VMessageOut::MarkClose () noexcept {
  vassert(hdrSizeBits > 18);
  msgflags |= Close;
  // fix header
  this->ForceBitAt(15, true);
}


//==========================================================================
//
//  VMessageOut::fixSize
//
//  this fixes message size in the header
//
//==========================================================================
void VMessageOut::fixSize () {
  vassert(GetNumBits() >= 16);
  vassert(GetNumBits() <= MAX_MSG_SIZE_BITS);
  static_assert(MAX_MSG_SIZE_BITS <= 0xffff, "internal message size too big");
  vuint16 v = (vuint16)GetNumBits();
  const int chk = GetNumBits();
  vassert(v <= 0x4000);
  #ifdef VAVOOM_NET_DEBUG_OUTMESSAGE
  GCon->Logf(NAME_Debug, "*** outmessage size: %u", v);
  #endif
  // 14 bits is enough
  v <<= 2; // remove hight two bits, they are zero anyway
  for (int f = 0; f < 14; ++f) {
    ForceBitAt(f, v&0x8000u);
    v <<= 1;
  }
  vassert(GetNumBits() == chk);
}


//==========================================================================
//
//  VMessageOut::Finalise
//
//  call this before copying packet data
//
//==========================================================================
void VMessageOut::Finalise () {
  fixSize();
}
