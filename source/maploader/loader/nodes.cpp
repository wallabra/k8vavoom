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
#include "../../gamedefs.h"


extern VCvarB nodes_allow_compressed;


// old subsector flag
#define NF_SUBSECTOR_OLD  (0x8000)


//==========================================================================
//
//  VLevel::LinkNode
//
//==========================================================================
void VLevel::LinkNode (int BSPNum, node_t *pParent) {
  if (BSPNum&NF_SUBSECTOR) {
    int num = (BSPNum == -1 ? 0 : BSPNum&(~NF_SUBSECTOR));
    if (num < 0 || num >= NumSubsectors) Host_Error("ss %i with numss = %i", num, NumSubsectors);
    Subsectors[num].parent = pParent;
    if (pParent) {
      Subsectors[num].parentChild = ((int)pParent->children[0] == BSPNum ? 0 : 1);
    } else {
      Subsectors[num].parentChild = -1;
    }
  } else {
    if (BSPNum < 0 || BSPNum >= NumNodes) Host_Error("bsp %i with numnodes = %i", NumNodes, NumNodes);
    node_t *bsp = &Nodes[BSPNum];
    bsp->parent = pParent;
    LinkNode(bsp->children[0], bsp);
    LinkNode(bsp->children[1], bsp);
  }
}


//==========================================================================
//
//  VLevel::FindGLNodes
//
//==========================================================================
int VLevel::FindGLNodes (VName name) const {
  if (VStr::Length(*name) < 6) return W_CheckNumForName(VName(va("gl_%s", *name), VName::AddLower8));

  // long map name, check GL_LEVEL lumps
  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) != NAME_gl_level) continue;
    if (W_LumpLength(Lump) < 12) continue; // lump is too short
    char Buf[16];
    VStream *lumpstream = W_CreateLumpReaderNum(Lump);
    {
      VCheckedStream Strm(lumpstream);
      Strm.Serialise(Buf, Strm.TotalSize() < 16 ? Strm.TotalSize() : 16);
    }
    if (memcmp(Buf, "LEVEL=", 6)) continue; // "LEVEL" keyword expected, but missing
    for (int i = 11; i < 14; ++i) {
      if (Buf[i] == '\n' || Buf[i] == '\r') {
        Buf[i] = 0;
        break;
      }
    }
    Buf[14] = 0;
    if (!VStr::ICmp(Buf+6, *name)) return Lump;
  }
  return -1;
}


//==========================================================================
//
//  VLevel::LoadNodes
//
//==========================================================================
void VLevel::LoadNodes (int Lump) {
  if (LevelFlags&LF_GLNodesV5) {
    NumNodes = W_LumpLength(Lump)/32;
  } else {
    NumNodes = W_LumpLength(Lump)/28;
  }
  Nodes = new node_t[NumNodes];
  memset((void *)Nodes, 0, sizeof(node_t)*NumNodes);

  VStream *lumpstream = W_CreateLumpReaderNum(Lump);
  VCheckedStream Strm(lumpstream);
  node_t *no = Nodes;
  for (int i = 0; i < NumNodes; ++i, ++no) {
    vint16 x, y, dx, dy;
    vint16 bbox[2][4];
    vuint32 children[2];
    Strm << x << y << dx << dy
      << bbox[0][0] << bbox[0][1] << bbox[0][2] << bbox[0][3]
      << bbox[1][0] << bbox[1][1] << bbox[1][2] << bbox[1][3];
    if (LevelFlags&LF_GLNodesV5) {
      Strm << children[0] << children[1];
    } else {
      vuint16 child0, child1;
      Strm << child0 << child1;
      children[0] = child0;
      if (children[0]&NF_SUBSECTOR_OLD) children[0] ^= NF_SUBSECTOR_OLD|NF_SUBSECTOR;
      children[1] = child1;
      if (children[1]&NF_SUBSECTOR_OLD) children[1] ^= NF_SUBSECTOR_OLD|NF_SUBSECTOR;
    }

    if (dx == 0 && dy == 0) {
      //Host_Error("invalid nodes (dir)");
      GCon->Log("invalid BSP node (zero direction)");
      no->SetPointDirXY(TVec(x, y, 0), TVec(0.001f, 0, 0));
    } else {
      no->SetPointDirXY(TVec(x, y, 0), TVec(dx, dy, 0));
    }

    no->sx = x<<16;
    no->sy = y<<16;
    no->dx = dx<<16;
    no->dy = dy<<16;

    for (int j = 0; j < 2; ++j) {
      no->children[j] = children[j];
      no->bbox[j][0] = bbox[j][BOX2D_LEFT];
      no->bbox[j][1] = bbox[j][BOX2D_BOTTOM];
      no->bbox[j][2] = -32768.0f;
      no->bbox[j][3] = bbox[j][BOX2D_RIGHT];
      no->bbox[j][4] = bbox[j][BOX2D_TOP];
      no->bbox[j][5] = 32768.0f;
    }
  }
}


//==========================================================================
//
//  VLevel::LoadCompressedGLNodes
//
//==========================================================================
bool VLevel::LoadCompressedGLNodes (int Lump, char hdr[4]) {
  VStream *BaseStrm = W_CreateLumpReaderNum(Lump);

  // read header
  BaseStrm->Serialise(hdr, 4);
  if (BaseStrm->IsError()) {
    delete BaseStrm;
    GCon->Logf(NAME_Warning, "error reading GL nodes (k8vavoom will use internal node builder)");
    return false;
  }

  if ((hdr[0] == 'Z' || hdr[0] == 'X') &&
      hdr[1] == 'G' && hdr[2] == 'L' &&
      (hdr[3] == 'N' || hdr[3] == '2' || hdr[3] == '3'))
  {
    // ok
  } else {
    delete BaseStrm;
    GCon->Logf(NAME_Warning, "invalid GL nodes signature (k8vavoom will use internal node builder)");
    return false;
  }

  // create reader stream for the zipped data
  //vuint8 *TmpData = new vuint8[BaseStrm->TotalSize()-4];
  vuint8 *TmpData = (vuint8 *)Z_Calloc(BaseStrm->TotalSize()-4);
  BaseStrm->Serialise(TmpData, BaseStrm->TotalSize()-4);
  if (BaseStrm->IsError()) {
    delete BaseStrm;
    GCon->Logf(NAME_Warning, "error reading GL nodes (k8vavoom will use internal node builder)");
    return false;
  }

  VStream *DataStrm = new VMemoryStream(W_FullLumpName(Lump), TmpData, BaseStrm->TotalSize()-4, true);
  //delete[] TmpData;
  TmpData = nullptr;
  delete BaseStrm;

  VStream *Strm;
  if (hdr[0] == 'X') {
    Strm = DataStrm;
    DataStrm = nullptr;
  } else {
    Strm = new VZLibStreamReader(DataStrm);
  }

  int type;
  switch (hdr[3]) {
    //case 'D': type = 0; break;
    case 'N': type = 1; break;
    case '2': type = 2; break;
    case '3': type = 3; break;
    default:
      delete Strm;
      delete DataStrm;
      GCon->Logf(NAME_Warning, "this obsolete version of GL nodes is disabled (k8vavoom will use internal node builder)");
      return false;
  }

  GCon->Logf("NOTE: found %scompressed GL nodes, type %d", (hdr[0] == 'X' ? "un" : ""), type);

  if (!nodes_allow_compressed) {
    delete Strm;
    delete DataStrm;
    GCon->Logf(NAME_Warning, "this new version of GL nodes is disabled (k8vavoom will use internal node builder)");
    return false;
  }

  // read extra vertex data
  {
    vuint32 OrgVerts, NewVerts;
    *Strm << OrgVerts << NewVerts;

    if (Strm->IsError()) {
      delete Strm;
      delete DataStrm;
      GCon->Logf(NAME_Warning, "error reading GL nodes (k8vavoom will use internal node builder)");
      return false;
    }

    if (OrgVerts != (vuint32)NumVertexes) {
      delete Strm;
      delete DataStrm;
      GCon->Logf(NAME_Warning, "error reading GL nodes (got %u vertexes, expected %d vertexes)", OrgVerts, NumVertexes);
      return false;
    }

    if (OrgVerts+NewVerts != (vuint32)NumVertexes) {
      TVec *OldVerts = Vertexes;
      NumVertexes = OrgVerts+NewVerts;
      Vertexes = new TVec[NumVertexes];
      if (NumVertexes) memset((void *)Vertexes, 0, sizeof(TVec)*NumVertexes);
      if (OldVerts) memcpy((void *)Vertexes, (void *)OldVerts, OrgVerts*sizeof(TVec));
      // fix up vertex pointers in linedefs
      for (int i = 0; i < NumLines; ++i) {
        line_t &L = Lines[i];
        int v1 = L.v1-OldVerts;
        int v2 = L.v2-OldVerts;
        L.v1 = &Vertexes[v1];
        L.v2 = &Vertexes[v2];
      }
      delete[] OldVerts;
      OldVerts = nullptr;
    }

    // read new vertexes
    TVec *DstVert = Vertexes+OrgVerts;
    for (vuint32 i = 0; i < NewVerts; ++i, ++DstVert) {
      vint32 x, y;
      *Strm << x << y;
      *DstVert = TVec(x/65536.0f, y/65536.0f, 0.0f);
    }
  }

  // load subsectors
  int FirstSeg = 0;
  {
    NumSubsectors = Streamer<vuint32>(*Strm);
    if (NumSubsectors == 0 || NumSubsectors > 0x1fffffff || Strm->IsError()) Host_Error("error reading GL nodes (got %u subsectors)", NumSubsectors);
    Subsectors = new subsector_t[NumSubsectors];
    memset((void *)Subsectors, 0, sizeof(subsector_t)*NumSubsectors);
    subsector_t *ss = Subsectors;

    for (int i = 0; i < NumSubsectors; ++i, ++ss) {
      vuint32 NumSubSegs;
      *Strm << NumSubSegs;
      ss->numlines = NumSubSegs;
      ss->firstline = FirstSeg;
      FirstSeg += NumSubSegs;
    }
    if (FirstSeg == 0 || FirstSeg > 0x1fffffff || Strm->IsError()) Host_Error("error reading GL nodes (counted %i subsegs)", FirstSeg);
  }

  // load segs
  {
    NumSegs = Streamer<vuint32>(*Strm);
    if (NumSegs != FirstSeg || Strm->IsError()) Host_Error("error reading GL nodes (got %d segs, expected %d segs)", NumSegs, FirstSeg);

    Segs = new seg_t[NumSegs];
    memset((void *)Segs, 0, sizeof(seg_t)*NumSegs);
    for (int i = 0; i < NumSubsectors; ++i) {
      for (int j = 0; j < Subsectors[i].numlines; ++j) {
        vuint32 v1, partner, linedef;
        *Strm << v1 << partner;

        if (type >= 2) {
          *Strm << linedef;
        } else {
          vuint16 l16;
          *Strm << l16;
          linedef = l16;
          if (linedef == 0xffff) linedef = 0xffffffffu;
        }
        vuint8 side;
        *Strm << side;

        seg_t *seg = Segs+Subsectors[i].firstline+j;

        // assign partner (we need it for self-referencing deep water)
        seg->partner = (partner < (unsigned)NumSegs ? &Segs[partner] : nullptr);

        seg->v1 = &Vertexes[v1];
        // v2 will be set later

        if (linedef != 0xffffffffu) {
          if (linedef >= (vuint32)NumLines) Host_Error("Bad linedef index %u (ss=%d; nl=%d)", linedef, i, j);
          if (side > 1) Host_Error("Bad seg side %d", side);
          seg->linedef = &Lines[linedef];
          seg->side = side;
        } else {
          seg->linedef = nullptr;
        }
      }
    }
  }

  // load nodes
  {
    NumNodes = Streamer<vuint32>(*Strm);
    if (NumNodes == 0 || NumNodes > 0x1fffffff || Strm->IsError()) Host_Error("error reading GL nodes (got %u nodes)", NumNodes);
    Nodes = new node_t[NumNodes];
    memset((void *)Nodes, 0, sizeof(node_t)*NumNodes);
    node_t *no = Nodes;
    for (int i = 0; i < NumNodes; ++i, ++no) {
      vuint32 children[2];
      vint16 bbox[2][4];
      if (type < 3) {
        vint16 xx, yy, dxx, dyy;
        *Strm << xx << yy << dxx << dyy;
        vint32 x, y, dx, dy;
        x = xx;
        y = yy;
        dx = dxx;
        dy = dyy;
        if (dx == 0 && dy == 0) {
          //Host_Error("invalid nodes (dir)");
          GCon->Log("invalid BSP node (zero direction)");
          no->SetPointDirXY(TVec(x, y, 0), TVec(0.001f, 0, 0));
        } else {
          no->SetPointDirXY(TVec(x, y, 0), TVec(dx, dy, 0));
        }
        no->sx = x<<16;
        no->sy = y<<16;
        no->dx = dx<<16;
        no->dy = dy<<16;
      } else {
        vint32 x, y, dx, dy;
        *Strm << x << y << dx << dy;
        if (dx == 0 && dy == 0) {
          GCon->Log("invalid BSP node (zero direction)");
          no->SetPointDirXY(TVec(x/65536.0f, y/65536.0f, 0), TVec(0.001f, 0, 0));
        } else {
          no->SetPointDirXY(TVec(x/65536.0f, y/65536.0f, 0), TVec(dx/65536.0f, dy/65536.0f, 0));
        }
        no->sx = x;
        no->sy = y;
        no->dx = dx;
        no->dy = dy;
      }

      *Strm << bbox[0][0] << bbox[0][1] << bbox[0][2] << bbox[0][3]
            << bbox[1][0] << bbox[1][1] << bbox[1][2] << bbox[1][3]
            << children[0] << children[1];

      for (int j = 0; j < 2; ++j) {
        no->children[j] = children[j];
        no->bbox[j][0] = bbox[j][BOX2D_LEFT];
        no->bbox[j][1] = bbox[j][BOX2D_BOTTOM];
        no->bbox[j][2] = -32768.0f;
        no->bbox[j][3] = bbox[j][BOX2D_RIGHT];
        no->bbox[j][4] = bbox[j][BOX2D_TOP];
        no->bbox[j][5] = 32768.0f;
      }
    }
  }

  // set v2 of the segs
  {
    subsector_t *Sub = Subsectors;
    for (int i = 0; i < NumSubsectors; ++i, ++Sub) {
      seg_t *Seg = Segs+Sub->firstline;
      for (int j = 0; j < Sub->numlines-1; ++j, ++Seg) {
        Seg->v2 = Seg[1].v1;
      }
      Seg->v2 = Segs[Sub->firstline].v1;
    }
  }

  // `PostLoadSegs()` and `PostLoadSubsectors()` will do the rest

  // create dummy VIS data
  // k8: no need to do this, main loader will take care of it

  bool wasError = Strm->IsError();

  delete Strm;
  delete DataStrm;

  if (wasError) Host_Error("error reading GL Nodes (turn on forced node rebuilder in options to load this map)");

  return true;
}
