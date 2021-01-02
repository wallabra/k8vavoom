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
#include "loader_common.h"


//==========================================================================
//
//  VLevel::LoadVertexes
//
//==========================================================================
void VLevel::LoadVertexes (int Lump, int GLLump, int &NumBaseVerts) {
  int GlFormat = 0;
  if (GLLump >= 0) {
    // read header of the GL vertexes lump and determinte GL vertex format
    char Magic[4];
    W_ReadFromLump(GLLump, Magic, 0, 4);
    GlFormat = !VStr::NCmp((char *)Magic, GL_V2_MAGIC, 4) ? 2 : !VStr::NCmp((char *)Magic, GL_V5_MAGIC, 4) ? 5 : 1;
    if (GlFormat == 5) LevelFlags |= LF_GLNodesV5;
  }

  // determine number of vertexes: total lump length / vertex record length
  NumBaseVerts = W_LumpLength(Lump)/4;
  if (NumBaseVerts <= 0) Host_Error("Map '%s' has no vertices!", *MapName);

  int NumGLVerts = (GlFormat == 0 ? 0 : GlFormat == 1 ? (W_LumpLength(GLLump)/4) : ((W_LumpLength(GLLump)-4)/8));
  NumVertexes = NumBaseVerts+NumGLVerts;

  // allocate memory for vertexes
  Vertexes = new TVec[NumVertexes];
  if (NumVertexes) memset((void *)Vertexes, 0, sizeof(TVec)*NumVertexes);

  // load base vertexes
  TVec *pDst;
  {
    VStream *lumpstream = W_CreateLumpReaderNum(Lump);
    VCheckedStream Strm(lumpstream);
    pDst = Vertexes;
    for (int i = 0; i < NumBaseVerts; ++i, ++pDst) {
      vint16 x, y;
      Strm << x << y;
      *pDst = TVec(x, y, 0);
    }
  }

  if (GLLump >= 0) {
    // load gl vertexes
    VStream *lumpstream = W_CreateLumpReaderNum(GLLump);
    VCheckedStream Strm(lumpstream);
    if (GlFormat == 1) {
      // gl version 1 vertexes, same as normal ones
      for (int i = 0; i < NumGLVerts; ++i, ++pDst) {
        vint16 x, y;
        Strm << x << y;
        *pDst = TVec(x, y, 0);
      }
    } else {
      // gl version 2 or greater vertexes, as fixed point
      Strm.Seek(4);
      for (int i = 0; i < NumGLVerts; ++i, ++pDst) {
        vint32 x, y;
        Strm << x << y;
        *pDst = TVec(x/65536.0f, y/65536.0f, 0);
      }
    }
  }
}
