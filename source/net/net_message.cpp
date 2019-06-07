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
#include "gamedefs.h"
#include "network.h"


//==========================================================================
//
//  VMessageIn::VMessageIn
//
//==========================================================================
VMessageIn::VMessageIn (vuint8 *Src, vint32 Length)
  : VBitStreamReader(Src, Length)
  , Next(nullptr)
  , ChanType(0)
  , ChanIndex(0)
  , bReliable(false)
  , bOpen(false)
  , bClose(false)
  , Sequence(0)
{
}


//==========================================================================
//
//  VMessageOut::VMessageOut
//
//==========================================================================
VMessageOut::VMessageOut (VChannel *AChannel, bool aAllowExpand)
  : VBitStreamWriter(OUT_MESSAGE_SIZE, aAllowExpand) // allow expand
  , mChannel(AChannel)
  , Next(nullptr)
  , ChanType(AChannel->Type)
  , ChanIndex(AChannel->Index)
  , bReliable(/*false*/aAllowExpand)
  , bOpen(false)
  , bClose(false)
  , bReceivedAck(false)
  , Sequence(0)
  , Time(0)
  , PacketId(0)
  , markPos(0)
{
}


//==========================================================================
//
//  VMessageOut::Reset
//
//==========================================================================
/*
void VMessageOut::Reset () {
  Sequence = 0;
  Time = 0;
  PacketId = 0;
  Clear();
}
*/


//==========================================================================
//
//  VMessageOut::NeedSplit
//
//==========================================================================
bool VMessageOut::NeedSplit () const {
  if (bError) {
    //GCon->Log(NAME_DevNet, "SHIT!");
    return false;
  }
  //GCon->Logf(NAME_DevNet, "split check: markPos=%d; pos=%d; max=%d; expanded=%d", markPos, Pos, Max, (int)IsExpanded());
  return (markPos > 0 && IsExpanded());
}


//==========================================================================
//
//  VMessageOut::Reset
//
//==========================================================================
void VMessageOut::SendSplitMessage () {
  check(bReliable);
  check(mChannel);
  if (IsError() || !NeedSplit()) return; // just in case
  if (Pos-markPos > OUT_MESSAGE_SIZE) { bError = true; return; } // oops
  check(!bOpen);
  check(!bClose);

  //GCon->Logf(NAME_DevNet, "SPLIT! max=%d, size=%d, markPos=%d, extra=%d", Max, Pos, markPos, Pos-markPos);

  // send message part
  VMessageOut tmp(mChannel);
  tmp.ChanType = ChanType;
  tmp.ChanIndex = ChanIndex;
  tmp.bReliable = bReliable;
  tmp.bOpen = bOpen;
  tmp.bClose = bClose;
  tmp.SerialiseBits(Data.Ptr(), markPos);
  mChannel->SendMessage(&tmp);

  // remove sent bits
  const int bitsLeft = Pos-markPos;
  TArray<vuint8> oldData;
  //oldData.setLength((bitsLeft+7)/8+16);
  //TODO: make this faster
  Pos = markPos;
  vuint8 currByte = 0;
  vuint8 currMask = 1;
  for (int f = 0; f < bitsLeft; ++f) {
    if (ReadBitInternal()) currByte |= currMask;
    currMask <<= 1;
    if (currMask == 0) {
      oldData.append(currByte);
      currByte = 0;
      currMask = 1;
    }
  }
  oldData.append(currByte);

  Data.setLength((Max+7)/8+256);
  memset(Data.ptr(), 0, Data.length());
  Pos = 0;
  for (int f = 0; f < bitsLeft; ++f) {
    WriteBit(!!(oldData[f/8]&(1<<(f%8))));
  }

  markPos = 0;

  //GCon->Logf(NAME_DevNet, "  max=%d, size=%d; left=%d", Max, Pos, bitsLeft);
}
