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
//**
//**    Build nodes using zdbsp.
//**
//**************************************************************************
//#include "bsp/zdbsp/xs_Float.h"
#include "bsp/zdbsp/nodebuild.h"


static inline int toFix (double val) { return (int)(val*(1<<16)); }
static inline float fromFix (int val) { return (float)((double)val/(double)(1<<16)); }


//==========================================================================
//
//  UploadSectorsZD
//
//==========================================================================
static void UploadSectorsZD (VLevel *Level, ZDBSP::FLevel &zlvl) {
  for (auto &&it : Level->allSectorsIdx()) {
    ZDBSP::IntSector zsec;
    zsec.data.special = it.value()->special;
    zsec.data.tag = it.value()->sectorTag;
    zsec.origindex = it.index();
    zlvl.Sectors.Push(zsec);
  }
}


//==========================================================================
//
//  UploadSidedefsZD
//
//==========================================================================
static void UploadSidedefsZD (VLevel *Level, ZDBSP::FLevel &zlvl) {
  for (auto &&it : Level->allSidesIdx()) {
    ZDBSP::IntSideDef zside;
    zside.sector = (!it.value()->Sector ? ZDBSP::NO_INDEX : (int)(ptrdiff_t)(it.value()->Sector-Level->Sectors));
    zside.origindex = it.index();
    zlvl.Sides.Push(zside);
  }
}


//==========================================================================
//
//  UploadLinedefsZD
//
//==========================================================================
static void UploadLinedefsZD (VLevel *Level, ZDBSP::FLevel &zlvl) {
  int vcount = 0;
  TArray<int> vmap; // index: in `Level->Vertexes`; value: in zlvl
  vmap.setLength(Level->NumVertexes);
  for (int f = 0; f < vmap.length(); ++f) vmap[f] = -1;

  for (auto &&it : Level->allLinesIdx()) {
    const line_t *pSrc = it.value();
    int lv1idx = (int)(ptrdiff_t)(pSrc->v1-Level->Vertexes);
    int lv2idx = (int)(ptrdiff_t)(pSrc->v2-Level->Vertexes);
    if (lv1idx < 0 || lv2idx < 0 || lv1idx >= Level->NumVertexes || lv2idx >= Level->NumVertexes) Sys_Error("invalid linedef vertexes");
    if (vmap[lv1idx] < 0) vmap[lv1idx] = vcount++;
    if (vmap[lv2idx] < 0) vmap[lv2idx] = vcount++;
  }

  check(vcount);
  if (vcount != Level->NumVertexes) GCon->Logf("ZDBSP: dropped %d vertices out of %d", Level->NumVertexes-vcount, Level->NumVertexes);
  //GCon->Logf("ZDBSP: old vertex count is %d, new vertex count is %d", Level->NumVertexes, vcount);

  // copy used vertices
  zlvl.Vertices = new ZDBSP::WideVertex[vcount];
  zlvl.NumVertices = vcount;
  for (auto &&it : Level->allVerticesIdx()) {
    int didx = vmap[it.index()];
    if (didx >= 0) {
      check(didx < vcount);
      ZDBSP::WideVertex *zv = &zlvl.Vertices[didx];
      zv->x = toFix(it.value()->x);
      zv->y = toFix(it.value()->y);
      zv->index = didx;
    }
  }

  for (auto &&it : Level->allLinesIdx()) {
    const line_t *pSrc = it.value();
    if (!pSrc->v1 || !pSrc->v2) Sys_Error("linedef without vertexes");

    ZDBSP::IntLineDef zline;
    zline.origindex = it.index();
    zline.flags = pSrc->flags;
    zline.special = pSrc->special;
    zline.args[0] = pSrc->arg1;
    zline.args[1] = pSrc->arg2;
    zline.args[2] = pSrc->arg3;
    zline.args[3] = pSrc->arg4;
    zline.args[4] = pSrc->arg5;
    zline.sidenum[0] = (pSrc->sidenum[0] >= 0 ? pSrc->sidenum[0] : ZDBSP::NO_INDEX);
    zline.sidenum[1] = (pSrc->sidenum[1] >= 0 ? pSrc->sidenum[1] : ZDBSP::NO_INDEX);

    int lv1idx = (int)(ptrdiff_t)(pSrc->v1-Level->Vertexes);
    int lv2idx = (int)(ptrdiff_t)(pSrc->v2-Level->Vertexes);
    if (lv1idx < 0 || lv2idx < 0 || lv1idx >= Level->NumVertexes || lv2idx >= Level->NumVertexes) Sys_Error("invalid linedef vertexes");
    check(vmap[lv1idx] >= 0);
    check(vmap[lv2idx] >= 0);
    zline.v1 = vmap[lv1idx];
    zline.v2 = vmap[lv2idx];

    zlvl.Lines.Push(zline);
  }
}


//==========================================================================
//
//  UploadThingsZD
//
//==========================================================================
static void UploadThingsZD (VLevel *Level, ZDBSP::FLevel &zlvl) {
  for (auto &&it : Level->allThingsIdx()) {
    const mthing_t *pSrc = it.value();
    ZDBSP::IntThing thing;
    thing.thingid = pSrc->tid;
    thing.x = toFix((double)pSrc->x);
    thing.y = toFix((double)pSrc->y);
    thing.z = 0; //???
    thing.angle = pSrc->angle;
    thing.type = pSrc->type;
    thing.flags = pSrc->options;
    thing.special = pSrc->special;
    thing.args[0] = pSrc->arg1;
    thing.args[1] = pSrc->arg2;
    thing.args[2] = pSrc->arg3;
    thing.args[3] = pSrc->arg4;
    thing.args[4] = pSrc->arg5;
    zlvl.Things.Push(thing);
  }
}


//==========================================================================
//
//  CopyNodeZD
//
//==========================================================================
static void CopyNodeZD (int NodeIndex, const ZDBSP::MapNodeEx &SrcNode, node_t *Node) {
  TVec org = TVec(fromFix(SrcNode.x), fromFix(SrcNode.y), 0);
  TVec dir = TVec(fromFix(SrcNode.dx), fromFix(SrcNode.dy), 0);
  // check if `Length()` and `SetPointDirXY()` are happy
  if (dir.x == 0 && dir.y == 0) {
    //Host_Error("AJBSP: invalid BSP node (zero direction)");
    GCon->Logf("ZDBSP: invalid BSP node #%d (zero direction)", NodeIndex);
    dir.x = 0.001f;
  }
  Node->SetPointDirXY(org, dir);

  Node->sx = SrcNode.x;
  Node->sy = SrcNode.y;
  Node->dx = SrcNode.dx;
  Node->dy = SrcNode.dy;

  Node->bbox[0][0] = SrcNode.bbox[0][ZDBSP::BOXLEFT]; // minx
  Node->bbox[0][1] = SrcNode.bbox[0][ZDBSP::BOXBOTTOM]; // miny
  Node->bbox[0][2] = -32768.0f;
  Node->bbox[0][3] = SrcNode.bbox[0][ZDBSP::BOXRIGHT]; // maxx
  Node->bbox[0][4] = SrcNode.bbox[0][ZDBSP::BOXTOP]; // maxy
  Node->bbox[0][5] = 32768.0f;

  Node->bbox[1][0] = SrcNode.bbox[1][ZDBSP::BOXLEFT]; // minx
  Node->bbox[1][1] = SrcNode.bbox[1][ZDBSP::BOXBOTTOM]; // miny
  Node->bbox[1][2] = -32768.0f;
  Node->bbox[1][3] = SrcNode.bbox[1][ZDBSP::BOXRIGHT]; // maxx
  Node->bbox[1][4] = SrcNode.bbox[1][ZDBSP::BOXTOP]; // maxy
  Node->bbox[1][5] = 32768.0f;

  Node->children[0] = SrcNode.children[0];
  Node->children[1] = SrcNode.children[1];
}


//==========================================================================
//
//  ZDProgress
//
//  total==-1: complete
//
//==========================================================================
void ZDProgress (int curr, int total) {
  //GCon->Logf("BSP: %d/%d", curr, total);
#ifdef CLIENT
  if (total <= 0) {
    R_PBarUpdate("BSP", 42, 42, true); // final update
  } else {
    R_PBarUpdate("BSP", curr, total);
  }
#endif
}


//==========================================================================
//
//  VLevel::BuildNodesZD
//
//==========================================================================
void VLevel::BuildNodesZD () {
  ZDBSP::FLevel zlvl;

  // set up map data from loaded data
  UploadSectorsZD(this, zlvl);
  UploadSidedefsZD(this, zlvl);
  UploadLinedefsZD(this, zlvl);
  UploadThingsZD(this, zlvl);

  /*
  zlvl.RemoveExtraLines();
  zlvl.RemoveExtraSides();
  zlvl.RemoveExtraSectors();
  */

  zlvl.FindMapBounds();

  auto nb = new ZDBSP::FNodeBuilder(zlvl, *MapName);

  ZDBSP::WideVertex *newverts = nullptr;
  int newvertsCount = 0;
  nb->GetVertices(newverts, newvertsCount);

  ZDBSP::MapNodeEx *nodes = nullptr;
  int nodeCount = 0;
  ZDBSP::MapSegGLEx *segs = nullptr;
  int segCount = 0;
  ZDBSP::MapSubsectorEx *ssecs = nullptr;
  int subCount = 0;
  nb->GetGLNodes(nodes, nodeCount, segs, segCount, ssecs, subCount);

  delete nb;

  GCon->Logf("added %d vertices; created %d nodes, %d segs, and %d subsectors", newvertsCount-NumVertexes, nodeCount, segCount, subCount);

  // copy vertices
  {
    delete[] Vertexes;

    NumVertexes = newvertsCount;
    Vertexes = new vertex_t[newvertsCount];
    memset((void *)Vertexes, 0, sizeof(vertex_t)*newvertsCount);

    for (int vidx = 0; vidx < newvertsCount; ++vidx) {
      Vertexes[vidx] = TVec(fromFix(newverts[vidx].x), fromFix(newverts[vidx].y), 0);
    }

    delete[] newverts;
  }

  // fix linedefs
  check(NumLines == (int)zlvl.Lines.Size());
  for (size_t f = 0; f < zlvl.Lines.Size(); ++f) {
    const ZDBSP::IntLineDef &zline = zlvl.Lines[f];
    line_t *nl = &Lines[f];
    nl->v1 = &Vertexes[zline.v1];
    nl->v2 = &Vertexes[zline.v2];
  }

  // copy subsectors
  {
    NumSubsectors = subCount;
    delete[] Subsectors;
    Subsectors = new subsector_t[subCount];
    memset((void *)Subsectors, 0, sizeof(subsector_t)*subCount);
    subsector_t *destss = &Subsectors[0];
    for (int i = 0; i < subCount; ++i, ++destss) {
      ZDBSP::MapSubsectorEx &srcss = ssecs[i];
      destss->numlines = srcss.numlines;
      destss->firstline = srcss.firstline;
    }
    delete[] ssecs;
  }

  // copy segs
  {
    NumSegs = segCount;
    delete[] Segs;
    Segs = new seg_t[segCount];
    memset((void *)Segs, 0, sizeof(seg_t)*segCount);

    seg_t *destseg = &Segs[0];
    for (int i = 0; i < segCount; ++i, ++destseg) {
      const ZDBSP::MapSegGLEx &zseg = segs[i];

      if (zseg.v1 == zseg.v2) {
        GCon->Logf(NAME_Error, "ZDBSP: seg #%d has same vertices (%d)", i, (int)zseg.v1);
      }

      destseg->v1 = &Vertexes[zseg.v1];
      destseg->v2 = &Vertexes[zseg.v2];

      if (zseg.side != 0 && zseg.side != 1) Sys_Error("ZDBSP: invalid seg #%d side (%d)", i, zseg.side);
      destseg->side = zseg.side;

      if (zseg.linedef != ZDBSP::NO_INDEX) {
        if ((int)zseg.linedef < 0 || (int)zseg.linedef >= NumLines) Sys_Error("ZDBSP: invalid seg #%d linedef (%d), max is %d", i, (int)zseg.linedef, NumLines-1);
        destseg->linedef = &Lines[zseg.linedef];
      }

      destseg->partner = (zseg.partner == ZDBSP::NO_INDEX ? nullptr : &Segs[zseg.partner]);
    }
    delete[] segs;
  }

  // copy nodes
  {
    NumNodes = nodeCount;
    delete[] Nodes;
    Nodes = new node_t[NumNodes];
    memset((void *)Nodes, 0, sizeof(node_t)*NumNodes);
    for (int f = 0; f < nodeCount; ++f) CopyNodeZD(f, nodes[f], &Nodes[f]);
    delete[] nodes;
  }

  // clear blockmap (just in case), so it will be recreated by the engine
  delete[] BlockMapLump;
  BlockMapLump = nullptr;
  BlockMapLumpSize = 0;
}
