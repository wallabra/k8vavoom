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
#include "p_setup_load_common.h"


//==========================================================================
//
//  VLevel::LoadGLSegs
//
//==========================================================================
void VLevel::LoadGLSegs (int Lump, int NumBaseVerts) {
  TVec *GLVertexes = Vertexes+NumBaseVerts;
  int NumGLVertexes = NumVertexes-NumBaseVerts;

  // determine format of the segs data
  int Format;
  vuint32 GLVertFlag;
  if (LevelFlags&LF_GLNodesV5) {
    Format = 5;
    NumSegs = W_LumpLength(Lump)/16;
    GLVertFlag = GL_VERTEX_V5;
  } else {
    char Header[4];
    W_ReadFromLump(Lump, Header, 0, 4);
    if (memcmp(Header, GL_V3_MAGIC, 4) == 0) {
      Format = 3;
      NumSegs = (W_LumpLength(Lump)-4)/16;
      GLVertFlag = GL_VERTEX_V3;
    } else {
      Format = 1;
      NumSegs = W_LumpLength(Lump)/10;
      GLVertFlag = GL_VERTEX;
    }
  }

  // allocate memory for segs data
  Segs = new seg_t[NumSegs];
  memset((void *)Segs, 0, sizeof(seg_t)*NumSegs);

  // read data
  VStream *lumpstream = W_CreateLumpReaderNum(Lump);
  VCheckedStream Strm(lumpstream);
  if (Format == 3) Strm.Seek(4);
  seg_t *seg = Segs;
  for (int i = 0; i < NumSegs; ++i, ++seg) {
    vuint32 v1num;
    vuint32 v2num;
    vuint16 linedef; // -1 for minisegs
    vuint16 side;
    vuint16 partner; // -1 on one-sided walls

    if (Format < 3) {
      vuint16 v1, v2;
      Strm << v1 << v2 << linedef << side << partner;
      v1num = v1;
      v2num = v2;
      if (side > 1) Host_Error("Bad GL vertex side %u", side);
    } else {
      vuint32 v1, v2;
      vint16 flags;
      Strm << v1 << v2 << linedef << flags << partner;
      v1num = v1;
      v2num = v2;
      side = flags&GL_SEG_FLAG_SIDE;
    }

    if (v1num&GLVertFlag) {
      v1num ^= GLVertFlag;
      if (v1num >= (vuint32)NumGLVertexes) Host_Error("Bad GL vertex index %u", v1num);
      seg->v1 = &GLVertexes[v1num];
    } else {
      if (v1num >= (vuint32)NumVertexes) Host_Error("Bad vertex index %u (04)", v1num);
      seg->v1 = &Vertexes[v1num];
    }

    if (v2num&GLVertFlag) {
      v2num ^= GLVertFlag;
      if (v2num >= (vuint32)NumGLVertexes) Host_Error("Bad GL vertex index %u", v2num);
      seg->v2 = &GLVertexes[v2num];
    } else {
      if (v2num >= (vuint32)NumVertexes) Host_Error("Bad vertex index %u (05)", v2num);
      seg->v2 = &Vertexes[v2num];
    }

    if (linedef != 0xffffu) {
      if (linedef >= NumLines) Host_Error("Bad GL vertex linedef index %u", linedef);
      line_t *ldef = &Lines[linedef];
      seg->linedef = ldef;
      seg->side = side;
    }

    // assign partner (we need it for self-referencing deep water)
    seg->partner = (partner != 0xffffu && partner < NumSegs ? &Segs[partner] : nullptr);

    // `PostLoadSegs()` will do the rest
  }
}


//==========================================================================
//
//  VLevel::PostLoadSegs
//
//  vertices, side, linedef and partner should be set
//
//==========================================================================
void VLevel::PostLoadSegs () {
  for (auto &&it : allSegsIdx()) {
    int i = it.index();
    seg_t *seg = it.value();
    int dside = seg->side;
    if (dside != 0 && dside != 1) Sys_Error("invalid seg #%d side (%d)", i, dside);

    if (seg->linedef) {
      line_t *ldef = seg->linedef;

      if (ldef->sidenum[dside] < 0 || ldef->sidenum[dside] >= NumSides) {
        Host_Error("seg #%d, ldef=%d: invalid sidenum %d (%d) (max sidenum is %d)\n", i, (int)(ptrdiff_t)(ldef-Lines), dside, ldef->sidenum[dside], NumSides-1);
      }

      seg->sidedef = &Sides[ldef->sidenum[dside]];
      seg->frontsector = Sides[ldef->sidenum[dside]].Sector;

      if (ldef->flags&ML_TWOSIDED) {
        if (ldef->sidenum[dside^1] < 0 || ldef->sidenum[dside^1] >= NumSides) Host_Error("another side of two-sided linedef is fucked");
        seg->backsector = Sides[ldef->sidenum[dside^1]].Sector;
      } else if (ldef->sidenum[dside^1] >= 0) {
        if (ldef->sidenum[dside^1] >= NumSides) Host_Error("another side of blocking two-sided linedef is fucked");
        seg->backsector = Sides[ldef->sidenum[dside^1]].Sector;
        // not a two-sided, so clear backsector (just in case) -- nope
        //destseg->backsector = nullptr;
      } else {
        seg->backsector = nullptr;
        ldef->flags &= ~ML_TWOSIDED; // just in case
      }
    } else {
      // minisegs should have front and back sectors set too, because why not?
      // but this will be fixed in `PostLoadSubsectors()`
    }

    CalcSegLenOfs(seg);
    CalcSeg(seg); // this will check for zero-length segs
  }
}
