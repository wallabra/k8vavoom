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
  // read length (it includes 2 bytes for the length itself)
  vuint16 v = 0;
  for (int f = 0; f < 16; ++f) {
    if (srcPacket.IsError()) { bError = true; return false; }
    v <<= 1;
    if (srcPacket.ReadBit()) v |= 0x01u;
  }
  if (srcPacket.IsError() || IsError() || v < 2*8) { bError = true; return false; }
  v -= 2*8;
  GCon->Logf(NAME_Debug, "*** inmessage size: %u (left %u)", v, srcPacket.GetNumBits()-srcPacket.GetPos());
  if (srcPacket.GetNumBits() < v) {
    GCon->Log(NAME_Debug, "*** inmessage: out of data in packed!");
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
  ChanIndex = (vint32)this->ReadInt();
  GCon->Logf(NAME_Debug, "*** inmessage: cidx=%d", ChanIndex);
  if (ChanIndex < 0) {
    bError = true;
    return false;
  }
  bOpen = ReadBit();
  bClose = ReadBit();
  GCon->Logf(NAME_Debug, "*** inmessage: bOpen=%d; bClose=%d; cidx=%d", (int)bOpen, (int)bClose, ChanIndex);
  if (bOpen) {
    int ctype = ReadInt();
    GCon->Logf(NAME_Debug, "*** inmessage: ctype=%d", ctype);
    if (ctype < 0 || ctype >= CHANNEL_MAX) {
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
//  VMessageIn::ReadMessageSize
//
//  returns -1 on error
//
//==========================================================================
int VMessageIn::ReadMessageSize (VBitStreamReader &strm) {
  if (strm.IsError()) return -1;
  vuint16 len = 0xffffu;
  strm << len;
  if (len > MAX_MSG_SIZE_BITS) return -1;
  return (int)len;
}



//==========================================================================
//
//  VMessageOut::VMessageOut
//
//==========================================================================
VMessageOut::VMessageOut (VChannel *AChannel, unsigned flags)
  : VBitStreamWriter(MAX_MSG_SIZE_BITS+16, false) // no expand
  , hdrSizeBits(0)
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
  // reserve room for size
  for (int f = 0; f < 16; ++f) this->WriteBit(false);
  vassert(GetNumBits() == 16);
  this->WriteInt(AChanIndex);
  this->WriteBit(!!(flags&Open));
  this->WriteBit(!!(flags&Close));
  if (flags&Open) this->WriteInt(AChanType);
  hdrSizeBits = GetNumBits();
}


//==========================================================================
//
//  VMessageOut::setSize
//
//==========================================================================
void VMessageOut::fixSize () {
  vassert(GetNumBits() >= 2*8);
  vassert(GetNumBits() <= MAX_MSG_SIZE_BITS);
  static_assert(MAX_MSG_SIZE_BITS <= 0xffff, "internal message size too big");
  //HACK!
  int savedPos = Pos;
  vuint16 v = (vuint16)savedPos;
  GCon->Logf(NAME_Debug, "*** outmessage size: %u", v);
  Pos = 0;
  for (int f = 0; f < 16; ++f) {
    WriteBit(!!(v&0x8000u));
    v <<= 1;
  }
  vassert(Pos == 2*8);
  Pos = savedPos;
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
