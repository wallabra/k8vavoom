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
//  VLevel::LoadLineDefs1
//
//  For Doom and Heretic
//
//==========================================================================
void VLevel::LoadLineDefs1 (int Lump, int NumBaseVerts, const VMapInfo &MInfo) {
  NumLines = W_LumpLength(Lump)/14;
  Lines = new line_t[NumLines];
  memset((void *)Lines, 0, sizeof(line_t)*NumLines);

  VStream *lumpstream = W_CreateLumpReaderNum(Lump);
  VCheckedStream Strm(lumpstream);
  line_t *ld = Lines;
  for (int i = 0; i < NumLines; ++i, ++ld) {
    vuint16 v1, v2, flags;
    vuint16 special, tag;
    vuint16 side0, side1;
    Strm << v1 << v2 << flags << special << tag << side0 << side1;

    if (v1 >= NumBaseVerts) Host_Error("Bad vertex index %d (00)", v1);
    if (v2 >= NumBaseVerts) Host_Error("Bad vertex index %d (01)", v2);

    ld->flags = flags;
    ld->special = special;
    ld->arg1 = (tag == 0xffff ? -1 : tag);
    ld->v1 = &Vertexes[v1];
    ld->v2 = &Vertexes[v2];
    ld->sidenum[0] = (side0 == 0xffff ? -1 : side0);
    ld->sidenum[1] = (side1 == 0xffff ? -1 : side1);

    ld->alpha = 1.0f;
    ld->lineTag = -1; //k8: this should be zero, i think

    if (MInfo.Flags&VLevelInfo::LIF_ClipMidTex) ld->flags |= ML_CLIP_MIDTEX;
    if (MInfo.Flags&VLevelInfo::LIF_WrapMidTex) ld->flags |= ML_WRAP_MIDTEX;
  }
}


//==========================================================================
//
//  VLevel::LoadLineDefs2
//
//  Hexen format
//
//==========================================================================
void VLevel::LoadLineDefs2 (int Lump, int NumBaseVerts, const VMapInfo &MInfo) {
  NumLines = W_LumpLength(Lump)/16;
  Lines = new line_t[NumLines];
  memset((void *)Lines, 0, sizeof(line_t)*NumLines);

  VStream *lumpstream = W_CreateLumpReaderNum(Lump);
  VCheckedStream Strm(lumpstream);
  line_t *ld = Lines;
  for (int i = 0; i < NumLines; ++i, ++ld) {
    vuint16 v1, v2, flags;
    vuint8 special, arg1, arg2, arg3, arg4, arg5;
    vuint16 side0, side1;
    Strm << v1 << v2 << flags << special << arg1 << arg2 << arg3 << arg4 << arg5 << side0 << side1;

    if (v1 >= NumBaseVerts) Host_Error("Bad vertex index %d (02)", v1);
    if (v2 >= NumBaseVerts) Host_Error("Bad vertex index %d (03)", v2);

    ld->flags = flags&~ML_SPAC_MASK;
    int Spac = (flags&ML_SPAC_MASK)>>ML_SPAC_SHIFT;
    if (Spac == 7) {
      ld->SpacFlags = SPAC_Impact|SPAC_PCross;
    } else {
      ld->SpacFlags = 1<<Spac;
    }

    // new line special info
    ld->special = special;
    ld->arg1 = arg1;
    ld->arg2 = arg2;
    ld->arg3 = arg3;
    ld->arg4 = arg4;
    ld->arg5 = arg5;

    ld->v1 = &Vertexes[v1];
    ld->v2 = &Vertexes[v2];
    ld->sidenum[0] = (side0 == 0xffff ? -1 : side0);
    ld->sidenum[1] = (side1 == 0xffff ? -1 : side1);

    ld->alpha = 1.0f;
    ld->lineTag = -1;

    if (MInfo.Flags&VLevelInfo::LIF_ClipMidTex) ld->flags |= ML_CLIP_MIDTEX;
    if (MInfo.Flags&VLevelInfo::LIF_WrapMidTex) ld->flags |= ML_WRAP_MIDTEX;
  }
}


//==========================================================================
//
//  VLevel::FinaliseLines
//
//==========================================================================
void VLevel::FinaliseLines () {
  line_t *ldef = Lines;
  for (int lleft = NumLines; lleft--; ++ldef) {
    // calculate line's plane, slopetype, etc
    CalcLine(ldef);
    // set up sector references
    ldef->frontsector = Sides[ldef->sidenum[0]].Sector;
    if (ldef->sidenum[1] != -1) {
      ldef->backsector = Sides[ldef->sidenum[1]].Sector;
    } else {
      ldef->backsector = nullptr;
    }
  }
}


//==========================================================================
//
//  VLevel::HashLines
//
//==========================================================================
void VLevel::HashLines () {
  /*
  // clear hash
  for (int i = 0; i < NumLines; ++i) Lines[i].HashFirst = -1;
  // create hash: process lines in backward order so that they get processed in original order
  for (int i = NumLines-1; i >= 0; --i) {
    vuint32 HashIndex = (vuint32)Lines[i].LineTag%(vuint32)NumLines;
    Lines[i].HashNext = Lines[HashIndex].HashFirst;
    Lines[HashIndex].HashFirst = i;
  }
  */
  tagHashClear(lineTags);
  for (int i = 0; i < NumLines; ++i) {
    line_t *ldef = &Lines[i];
    tagHashPut(lineTags, ldef->lineTag, &Lines[i]);
    for (int cc = 0; cc < ldef->moreTags.length(); ++cc) tagHashPut(lineTags, ldef->moreTags[cc], ldef);
  }
}
