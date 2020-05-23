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
#include "../gamedefs.h"
#include "network.h"
#include "net_message.h"


//==========================================================================
//
//  VMessageIn::toStringDbg
//
//==========================================================================
VStr VMessageIn::toStringDbg () const noexcept {
  return
    va("bits:%5d; open=%d; close=%d; reliable=%d; seq=%u",
      GetNumBits(), (int)bOpen, (int)bClose, (int)bReliable, ChanSequence);
}


//==========================================================================
//
//  VMessageOut::VMessageOut
//
//==========================================================================
VMessageOut::VMessageOut (VChannel *AChannel, bool areliable)
  : VBitStreamWriter(MAX_MSG_SIZE_BITS+16, false) // no expand
  , ChanType(AChannel ? AChannel->Type : 0)
  , ChanIndex(AChannel ? AChannel->Index : -1)
  , ChanSequence(0)
  , PacketId(0)
  , bOpen(false)
  , bClose(false)
  , bReliable(areliable)
  , Next(nullptr)
  , Time(0)
  , bReceivedAck(false)
  , OutEstimated(0)
{
  SetupWith((AChannel ? AChannel->Type : 0), (AChannel ? AChannel->Index : ~0u), areliable);
}


//==========================================================================
//
//  VMessageOut::VMessageOut
//
//==========================================================================
VMessageOut::VMessageOut (vuint8 AChanType, int AChanIndex, bool areliable)
  : VBitStreamWriter(MAX_MSG_SIZE_BITS+16, false) // no expand
{
  SetupWith(AChanType, (vuint32)AChanIndex, areliable);
}


//==========================================================================
//
//  VMessageOut::SetupWith
//
//==========================================================================
void VMessageOut::SetupWith (vuint8 AChanType, int AChanIndex, bool areliable) noexcept {
  ChanType = AChanType;
  ChanIndex = AChanIndex;
  ChanSequence = 0;
  PacketId = 0;
  bOpen = false;
  bClose = false;
  bReliable = areliable;
  Next = nullptr;
  Time = 0;
  bReceivedAck = false;
  OutEstimated = 0;
}


//==========================================================================
//
//  VMessageOut::Reset
//
//==========================================================================
void VMessageOut::Reset (VChannel *AChannel, bool areliable) {
  Clear();
  SetupWith((AChannel ? AChannel->Type : 0), (AChannel ? AChannel->Index : ~0u), areliable);
}



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

  bReliable = srcPacket.ReadBit();
  bOpen = (bReliable ? srcPacket.ReadBit() : false);
  bClose = (bReliable ? srcPacket.ReadBit() : false);
  srcPacket << STRM_INDEX_U(ChanIndex);
  ChanSequence = 0;
  if (bReliable) srcPacket << STRM_INDEX_U(ChanSequence);
  ChanType = (bReliable || bOpen ? srcPacket.ReadUInt() : 0);
  vuint32 dataBits = 0;
  srcPacket << STRM_INDEX_U(dataBits);

  if (dataBits >= (unsigned)srcPacket.GetNumBits()) {
    GCon->Log(NAME_DevNet, "*** inmessage: invalid number of data bits");
    srcPacket.Clear(true);
    (void)srcPacket.ReadBit(); // this sets error on srcpacket
    bError = true;
    return false;
  }

  if (srcPacket.IsError()) {
    GCon->Log(NAME_DevNet, "*** inmessage: incomplete message header");
    bError = true;
    return false;
  }

  SetData(srcPacket, dataBits);
  if (srcPacket.IsError() || IsError()) {
    GCon->Logf(NAME_Debug, "*** inmessage: error reading data from packet (%d : %d)!", (int)srcPacket.IsError(), (int)IsError());
    bError = true;
    return false;
  }

  return !IsError();
}



//==========================================================================
//
//  VMessageOut::WriteHeader
//
//==========================================================================
void VMessageOut::WriteHeader (VBitStreamWriter &strm) const {
  vassert(ChanIndex >= 0 && ChanIndex < MAX_CHANNELS);
  // "normal message" flag
  strm.WriteBit(false);
  strm.WriteBit(bReliable);
  if (bReliable) {
    strm.WriteBit(bOpen);
    strm.WriteBit(bClose);
  } else {
    vassert(!bOpen && !bClose);
  }
  strm << STRM_INDEX_U(ChanIndex);
  // for reliable, write channel sequence
  if (bReliable) strm << STRM_INDEX_U(ChanSequence);
  // for reliable or open, write channel type
  // this is because non-open reliable message can arrive first, and we need to create a channel anyway
  if (bReliable || bOpen) strm.WriteUInt(ChanType);
  vuint32 dataBits = (vuint32)GetNumBits();
  strm << STRM_INDEX_U(dataBits);
}


//==========================================================================
//
//  VMessageOut::EstimateSize
//
//  estimate current packet size
//  channel sequence, and packedid doesn't matter
//
//==========================================================================
int VMessageOut::EstimateSizeInBits (int addbits) const noexcept {
  vassert(addbits >= 0);
  // this is wrong, because the message should know about communication layer internals; but meh
  const int bitsize =
    // header
    1+ // zero
    1+ // reliable flag
    (bReliable ? 2 : 0)+ // open/close flags
    calcVarIntLength(ChanIndex)*8+ // channel index
    (bReliable ? MaxVarIntLength*8 : 0)+ // channel sequence (unknown yet)
    (bReliable || bOpen ? BitStreamCalcUIntBits(ChanType) : 0)+ // channel type
    calcVarIntLength(/*MAX_MSG_SIZE_BITS*/GetNumBits()+addbits)*8+ // size field
    // data
    GetNumBits()+addbits+
    // stop bit
    1;
  // get size in bytes
  const int bytesize = (bitsize+7)>>3;
  // return resulting bits
  return (bytesize<<3);
}


//==========================================================================
//
//  VMessageOut::toStringDbg
//
//==========================================================================
VStr VMessageOut::toStringDbg () const noexcept {
  return
    va("bits:%5d; pid=%u; seq=%u; open=%d; close=%d; reliable=%d; gotack=%d; time=%u (est:%d)",
      GetNumBits(), PacketId, ChanSequence, (int)bOpen, (int)bClose, (int)bReliable,
          (int)bReceivedAck, (unsigned)(Time*1000), OutEstimated);
}
