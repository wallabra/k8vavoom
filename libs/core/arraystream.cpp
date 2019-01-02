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
#include "core.h"


//==========================================================================
//
//  VArrayStream::VArrayStream
//
//==========================================================================
VArrayStream::VArrayStream (TArray<vuint8>& InArray)
  : Array(InArray)
  , Pos(0)
{
  bLoading = true;
}


//==========================================================================
//
//  VArrayStream::Serialise
//
//==========================================================================
void VArrayStream::Serialise (void *Data, int Len) {
  guard(VArrayStream::Serialise);
  if (bLoading) {
    if (Pos+Len > Array.Num()) {
      bError = true;
      if (Pos < Array.Num()) {
        memcpy(Data, &Array[Pos], Array.Num()-Pos);
        Pos = Array.Num();
      }
    } else if (Len) {
      memcpy(Data, &Array[Pos], Len);
      Pos += Len;
    }
  } else {
    if (Pos+Len > Array.Num()) Array.SetNumWithReserve(Pos+Len);
    memcpy(&Array[Pos], Data, Len);
    Pos += Len;
  }
  unguard;
}


//==========================================================================
//
//  VArrayStream::Seek
//
//==========================================================================
void VArrayStream::Seek (int InPos) {
  guard(VArrayStream::Seek);
  if (InPos < 0 || InPos > Array.Num()) {
    bError = true;
  } else {
    Pos = InPos;
  }
  unguard;
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
  return Array.Num();
}
