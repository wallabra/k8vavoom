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
//**  Copyright (C) 2018-2021 Ketmar Dark
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
#include "../../gamedefs.h"


extern VCvarB build_blockmap;


//==========================================================================
//
//  VLevel::LoadBlockMap
//
//==========================================================================
void VLevel::LoadBlockMap (int Lump) {
  VStream *Strm = nullptr;

  if (build_blockmap) {
    Lump = -1;
  } else {
    if (Lump >= 0 && !build_blockmap) Strm = W_CreateLumpReaderNum(Lump);
  }

  if (!Strm || Strm->TotalSize() == 0 || Strm->TotalSize()/2 >= 0x10000) {
    delete Strm;
    GCon->Logf("Creating BLOCKMAP");
    CreateBlockMap();
  } else {
    // killough 3/1/98: Expand wad blockmap into larger internal one,
    // by treating all offsets except -1 as unsigned and zero-extending
    // them. This potentially doubles the size of blockmaps allowed,
    // because Doom originally considered the offsets as always signed.

    // allocate memory for blockmap
    int count = Strm->TotalSize()/2;
    BlockMapLump = new vint32[count];
    BlockMapLumpSize = count;

    // read data
    BlockMapLump[0] = Streamer<vint16>(*Strm);
    BlockMapLump[1] = Streamer<vint16>(*Strm);
    BlockMapLump[2] = Streamer<vuint16>(*Strm);
    BlockMapLump[3] = Streamer<vuint16>(*Strm);
    for (int i = 4; i < count; i++) {
      vint16 Tmp;
      *Strm << Tmp;
      BlockMapLump[i] = Tmp == -1 ? -1 : (vuint16)Tmp&0xffff;
    }

    bool wasError = Strm->IsError();
    delete Strm;

    if (wasError) {
      GCon->Logf(NAME_Warning, "error loading BLOCKMAP, it will be rebuilt");
      delete BlockMapLump;
      BlockMapLump = nullptr;
      BlockMapLumpSize = 0;
      CreateBlockMap();
    }
  }

  // read blockmap origin and size
  /*
  BlockMapOrgX = BlockMapLump[0];
  BlockMapOrgY = BlockMapLump[1];
  BlockMapWidth = BlockMapLump[2];
  BlockMapHeight = BlockMapLump[3];
  BlockMap = BlockMapLump + 4;

  // clear out mobj chains
  int Count = BlockMapWidth * BlockMapHeight;
  BlockLinks = new VEntity*[Count];
  memset(BlockLinks, 0, sizeof(VEntity *)*Count);
  */
}


//==========================================================================
//
//  VLevel::CreateBlockMap
//
//==========================================================================
void VLevel::CreateBlockMap () {
  // determine bounds of the map
  float MinX = Vertexes[0].x;
  float MaxX = MinX;
  float MinY = Vertexes[0].y;
  float MaxY = MinY;
  for (int i = 0; i < NumVertexes; ++i) {
    if (MinX > Vertexes[i].x) MinX = Vertexes[i].x;
    if (MaxX < Vertexes[i].x) MaxX = Vertexes[i].x;
    if (MinY > Vertexes[i].y) MinY = Vertexes[i].y;
    if (MaxY < Vertexes[i].y) MaxY = Vertexes[i].y;
  }

  // they should be integers, but just in case round them
  MinX = floorf(MinX);
  MinY = floorf(MinY);
  MaxX = ceilf(MaxX);
  MaxY = ceilf(MaxY);

  int Width = MapBlock(MaxX-MinX)+1;
  int Height = MapBlock(MaxY-MinY)+1;

  // add all lines to their corresponding blocks
  // but skip zero-length lines
  TArray<vuint16> *BlockLines = new TArray<vuint16>[Width*Height];
  for (int i = 0; i < NumLines; ++i) {
    // determine starting and ending blocks
    line_t &Line = Lines[i];

    float ssq = Length2DSquared(*Line.v2 - *Line.v1);
    if (ssq < 1.0f) continue;
    ssq = Length2D(*Line.v2 - *Line.v1);
    if (ssq < 1.0f) continue;

    int X1 = MapBlock(Line.v1->x-MinX);
    int Y1 = MapBlock(Line.v1->y-MinY);
    int X2 = MapBlock(Line.v2->x-MinX);
    int Y2 = MapBlock(Line.v2->y-MinY);

    if (X1 > X2) {
      int Tmp = X2;
      X2 = X1;
      X1 = Tmp;
    }
    if (Y1 > Y2) {
      int Tmp = Y2;
      Y2 = Y1;
      Y1 = Tmp;
    }

    if (X1 == X2 && Y1 == Y2) {
      // line is inside a single block
      BlockLines[X1+Y1*Width].Append(i);
    } else if (Y1 == Y2) {
      // horisontal line of blocks
      for (int x = X1; x <= X2; x++) {
        BlockLines[x+Y1*Width].Append(i);
      }
    } else if (X1 == X2) {
      // vertical line of blocks
      for (int y = Y1; y <= Y2; y++) {
        BlockLines[X1+y*Width].Append(i);
      }
    }
    else {
      // diagonal line
      for (int x = X1; x <= X2; ++x) {
        for (int y = Y1; y <= Y2; ++y) {
          // check if line crosses the block
          if (Line.slopetype == ST_POSITIVE) {
            int p1 = Line.PointOnSide(TVec(MinX+x*128, MinY+(y+1)*128, 0));
            int p2 = Line.PointOnSide(TVec(MinX+(x+1)*128, MinY+y*128, 0));
            if (p1 == p2) continue;
          } else {
            int p1 = Line.PointOnSide(TVec(MinX+x*128, MinY+y*128, 0));
            int p2 = Line.PointOnSide(TVec(MinX+(x+1)*128, MinY+(y+1)*128, 0));
            if (p1 == p2) continue;
          }
          BlockLines[x+y*Width].Append(i);
        }
      }
    }
  }

  // build blockmap lump
  TArray<vint32> BMap;
  BMap.SetNum(4+Width*Height);
  BMap[0] = (int)MinX;
  BMap[1] = (int)MinY;
  BMap[2] = Width;
  BMap[3] = Height;
  for (int i = 0; i < Width*Height; ++i) {
    // write offset
    BMap[i+4] = BMap.Num();
    TArray<vuint16> &Block = BlockLines[i];
    // add dummy start marker
    BMap.Append(0);
    // add lines in this block
    for (int j = 0; j < Block.Num(); ++j) {
      BMap.Append(Block[j]);
    }
    // add terminator marker
    BMap.Append(-1);
  }

  // copy data
  BlockMapLump = new vint32[BMap.Num()];
  BlockMapLumpSize = BMap.Num();
  memcpy(BlockMapLump, BMap.Ptr(), BMap.Num()*sizeof(vint32));

  delete[] BlockLines;
  BlockLines = nullptr;
}
