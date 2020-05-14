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


//==========================================================================
//
//  VLevel::LoadReject
//
//==========================================================================
void VLevel::LoadReject (int Lump) {
  if (Lump < 0) return;
  VStream *lumpstream = W_CreateLumpReaderNum(Lump);
  VCheckedStream Strm(lumpstream);
  // check for empty reject lump
  if (Strm.TotalSize()) {
    // check if reject lump is required bytes long
    int NeededSize = (NumSectors*NumSectors+7)/8;
    if (Strm.TotalSize() < NeededSize) {
      GCon->Logf("Reject data is %d bytes too short", NeededSize-Strm.TotalSize());
    } else {
      // read it
      RejectMatrixSize = Strm.TotalSize();
      RejectMatrix = new vuint8[RejectMatrixSize];
      Strm.Serialise(RejectMatrix, RejectMatrixSize);

      // check if it's an all-zeroes lump, in which case it's useless and can be discarded
      bool Blank = true;
      for (int i = 0; i < NeededSize; ++i) {
        if (RejectMatrix[i]) {
          Blank = false;
          break;
        }
      }
      if (Blank) {
        delete[] RejectMatrix;
        RejectMatrix = nullptr;
        RejectMatrixSize = 0;
      }
    }
  }
}
