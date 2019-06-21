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
//#include "zdbsp/xs_Float.h"
#include "zdbsp/nodebuild.h"


static inline int toFix (double val) {
  return (int)(val*(1<<16));
}


static inline float fromFix (int val) {
  return (float)((double)val/(double)(1<<16));
}


//==========================================================================
//
//  CopyName8ZD
//
//==========================================================================
/*
static void CopyName8ZD (char *dest, VName name) {
  const char *s = *name;
  if (!s || !s[0]) s = "-";
  memset(dest, 0, 8);
  for (int f = 0; f < 8 && s[f]; ++f) dest[f] = s[f];
}
*/


//==========================================================================
//
//  UploadSectorsZD
//
//==========================================================================
static void UploadSectorsZD (VLevel *Level, ZDBSP::FLevel &zlvl) {
  const sector_t *pSrc = Level->Sectors;
  for (int i = 0; i < Level->NumSectors; ++i, ++pSrc) {
    ZDBSP::IntSector zsec;
    //zsec.data.floorheight = pSrc->floor.minz;
    //zsec.data.ceilingheight = pSrc->ceiling.minz;
    //CopyName8ZD(zsec.data.floorpic, GTextureManager.GetTextureName(pSrc->floor.pic));
    //CopyName8ZD(zsec.data.ceilingpic, GTextureManager.GetTextureName(pSrc->ceiling.pic));
    //zsec.data.lightlevel = pSrc->params.lightlevel;
    zsec.data.special = pSrc->special;
    zsec.data.tag = pSrc->sectorTag;
    zsec.origindex = i;
    zlvl.Sectors.Push(zsec);
  }
}


//==========================================================================
//
//  UploadSidedefsZD
//
//==========================================================================
static void UploadSidedefsZD (VLevel *Level, ZDBSP::FLevel &zlvl) {
  const side_t *pSrc = Level->Sides;
  for (int i = 0; i < Level->NumSides; ++i, ++pSrc) {
    ZDBSP::IntSideDef zside;
    //zside.textureoffset = 0; //k8:FIXME
    //zside.rowoffset = 0; //k8:FIXME
    //CopyName8ZD(zside.toptexture, GTextureManager.GetTextureName(pSrc->TopTexture));
    //CopyName8ZD(zside.bottomtexture, GTextureManager.GetTextureName(pSrc->BottomTexture));
    //CopyName8ZD(zside.midtexture, GTextureManager.GetTextureName(pSrc->MidTexture));
    zside.sector = (!pSrc->Sector ? ZDBSP::NO_INDEX : (int)(ptrdiff_t)(pSrc->Sector-Level->Sectors));
    zside.origindex = i;
    zlvl.Sides.Push(zside);
  }
}


//==========================================================================
//
//  UploadLinedefsZD
//
//==========================================================================
static void UploadLinedefsZD (VLevel *Level, ZDBSP::FLevel &zlvl) {
/*
  zlvl.Vertices = new ZDBSP::WideVertex[Level->NumVertexes];
  zlvl.NumVertices = Level->NumVertexes;

  for (int f = 0; f < Level->NumVertexes; ++f) {
    //vertmap[f] = -1;
    zlvl.Vertices[f].x = toFix((double)Level->Vertexes[f].x);
    zlvl.Vertices[f].y = toFix((double)Level->Vertexes[f].y);
    zlvl.Vertices[f].index = f;
  }
*/

  int vcount = 0;
  TArray<int> vmap; // index: in `Level->Vertexes`; value: in zlvl
  vmap.setLength(Level->NumVertexes);
  for (int f = 0; f < vmap.length(); ++f) vmap[f] = -1;
  const line_t *pSrc = Level->Lines;
  for (int i = 0; i < Level->NumLines; ++i, ++pSrc) {
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
  for (int f = 0; f < Level->NumVertexes; ++f) {
    int didx = vmap[f];
    if (didx >= 0) {
      ZDBSP::WideVertex *zv = &zlvl.Vertices[didx];
      zv->x = toFix((double)Level->Vertexes[f].x);
      zv->y = toFix((double)Level->Vertexes[f].y);
      zv->index = didx;
    }
  }

  pSrc = Level->Lines;
  for (int i = 0; i < Level->NumLines; ++i, ++pSrc) {
    if (!pSrc->v1 || !pSrc->v2) Sys_Error("linedef without vertexes");

    ZDBSP::IntLineDef zline;
    zline.origindex = i;
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
  const mthing_t *pSrc = Level->Things;
  for (int i = 0; i < Level->NumThings; ++i, ++pSrc) {
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
/*
  int   x,y,dx,dy;
  short bbox[2][4];
  DWORD children[2];

  NFX_SUBSECTOR
*/

  TVec org = TVec(fromFix(SrcNode.x), fromFix(SrcNode.y), 0);
  TVec dir = TVec(fromFix(SrcNode.dx), fromFix(SrcNode.dy), 0);
  // check if `Length()` and `SetPointDirXY()` are happy
  if (dir.x == 0 && dir.y == 0) {
    //Host_Error("AJBSP: invalid BSP node (zero direction)");
    GCon->Logf("ZDBSP: invalid BSP node #%d (zero direction)", NodeIndex);
    dir.x = 0.001f;
  }
  Node->SetPointDirXY(org, dir);

  /*
  GCon->Logf("#%d: pos=(%f,%f); (%d,%d)-(%d,%d) : (%d,%d)-(%d,%d) : d=(%f,%f)",
    NodeIndex,
    fromFix(SrcNode.x), fromFix(SrcNode.y),
    SrcNode.bbox[0][ZDBSP::BOXLEFT],
    SrcNode.bbox[0][ZDBSP::BOXBOTTOM],
    SrcNode.bbox[0][ZDBSP::BOXRIGHT],
    SrcNode.bbox[0][ZDBSP::BOXTOP],
    SrcNode.bbox[1][ZDBSP::BOXLEFT],
    SrcNode.bbox[1][ZDBSP::BOXBOTTOM],
    SrcNode.bbox[1][ZDBSP::BOXRIGHT],
    SrcNode.bbox[1][ZDBSP::BOXTOP],
    fromFix(SrcNode.dx), fromFix(SrcNode.dy)
  );
  */

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

  /*
  if ((int)zlvl.Lines.Size() != NumLines) {
    // some lines were removed, reupload lines and vertices
    GCon->Logf("ZDBSP: rebuilding vertex array...");
    delete [] Vertexes;
    NumVertexes = 0;
    TArray<int> vmap; // index: zlbl vertex index; value: new vertex index
    vmap.setLength(zlvl.NumVertices);
    for (int f = 0; f < zlvl.NumVertices; ++f) vmap[f] = -1;
    for (size_t f = 0; f < zlvl.Lines.Size(); ++f) {
      const ZDBSP::IntLineDef &line = zlvl.Lines[f];
      {
        int vidx = vmap[(int)line.v1];
        if (vidx < 0) {
          vidx = NumVertexes++;
          vmap[(int)line.v1] = vidx;
        }
      }
      {
        int vidx = vmap[(int)line.v2];
        if (vidx < 0) {
          vidx = NumVertexes++;
          vmap[(int)line.v2] = vidx;
        }
      }
    }
    GCon->Logf("ZDBSP: new vertex array size is %d (%d vertices dropped)", NumVertexes, zlvl.NumVertices-NumVertexes);
    // create new vertex array
    Vertexes = new vertex_t[NumVertexes];
    memset((void *)Vertexes, 0, sizeof(vertex_t)*NumVertexes);
    for (int f = 0; f < vmap.length(); ++f) {
      int didx = vmap[f];
      if (didx >= 0) {
        check(didx < NumVertexes);
        Vertexes[didx] = TVec(fromFix(zlvl.Vertices[f].x), fromFix(zlvl.Vertices[f].y), 0);
      }
    }
    // remap linedef vertices
    for (size_t f = 0; f < zlvl.Lines.Size(); ++f) {
      ZDBSP::IntLineDef &line = zlvl.Lines[f];
      check(vmap[line.v1] >= 0);
      check(vmap[line.v2] >= 0);
      line.v1 = vmap[line.v1];
      line.v2 = vmap[line.v2];
    }
    // put new vertex array to `FLevel`
    delete [] zlvl.Vertices;
    zlvl.NumVertices = NumVertexes;
    zlvl.Vertices = new ZDBSP::WideVertex[NumVertexes];
    for (int f = 0; f < NumVertexes; ++f) {
      zlvl.Vertices[f].x = toFix((double)Vertexes[f].x);
      zlvl.Vertices[f].y = toFix((double)Vertexes[f].y);
      zlvl.Vertices[f].index = f;
    }
  }
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
    delete [] Vertexes;

    NumVertexes = newvertsCount;
    Vertexes = new vertex_t[newvertsCount];
    memset((void *)Vertexes, 0, sizeof(vertex_t)*newvertsCount);

    for (int vidx = 0; vidx < newvertsCount; ++vidx) {
      Vertexes[vidx] = TVec(fromFix(newverts[vidx].x), fromFix(newverts[vidx].y), 0);
    }

    delete [] newverts;
  }

/*
  // build old->new line map
  TArray<int> ldmap; // index: old index; value: new index
  ldmap.setLength(NumLines);
  for (int f = 0; f < ldmap.length(); ++f) ldmap[f] = -1;

  // build old->new side map
  TArray<int> sdmap; // index: old index; value: new index
  sdmap.setLength(NumSides);
  for (int f = 0; f < sdmap.length(); ++f) sdmap[f] = -1;
  for (size_t f = 0; f < zlvl.Sides.Size(); ++f) {
    check(sdmap[zlvl.Sides[f].origindex] == -1);
    sdmap[zlvl.Sides[f].origindex] = (int)f;
  }
  //GCon->Logf("old side count is %d; new side count is %d", NumSides, (int)zlvl.Sides.Size());

  // copy linedefs
  {
    line_t *newlines = new line_t[zlvl.Lines.Size()];
    for (size_t f = 0; f < zlvl.Lines.Size(); ++f) {
      const ZDBSP::IntLineDef &zline = zlvl.Lines[f];
      line_t *nl = &newlines[f];
      ldmap[zline.origindex] = (int)f;
      *nl = Lines[zline.origindex];
      nl->v1 = &Vertexes[zline.v1];
      nl->v2 = &Vertexes[zline.v2];
      for (int sidx = 0; sidx < 2; ++sidx) {
        if (nl->sidenum[sidx] >= 0) {
          //if (sdmap[nl->sidenum[sidx]] < 0) GCon->Logf("FUCK: sidx=%d; old side is %d; new side is %d", sidx, nl->sidenum[sidx], sdmap[nl->sidenum[sidx]]);
          nl->sidenum[sidx] = sdmap[nl->sidenum[sidx]];
          check(nl->sidenum[sidx] >= 0);
          check(nl->sidenum[sidx] < (int)zlvl.Sides.Size());
        }
      }
    }
    delete [] Lines;
    NumLines = (int)zlvl.Lines.Size();
    Lines = newlines;
  }
*/
  // fix linedefs
  check(NumLines == (int)zlvl.Lines.Size());
  for (size_t f = 0; f < zlvl.Lines.Size(); ++f) {
    const ZDBSP::IntLineDef &zline = zlvl.Lines[f];
    line_t *nl = &Lines[f];
    nl->v1 = &Vertexes[zline.v1];
    nl->v2 = &Vertexes[zline.v2];
  }

  /*
  // copy sectors
  {
    sector_t *newsecs = new sector_t[zlvl.Sectors.Size()];
    memset((void *)newsecs, 0, sizeof(sector_t)*zlvl.Sectors.Size());
    for (size_t f = 0; f < zlvl.Sectors.Size(); ++f) {
      newsecs[f] = Sectors[zlvl.Sectors[f].origindex];
      check(!newsecs[f].subsectors);
    }
    delete [] Sectors;
    NumSectors = (int)zlvl.Sectors.Size();
    Sectors = newsecs;
  }
  */

  // copy sides
  /*
  {
    side_t *newsides = new side_t[zlvl.Sides.Size()];
    memset((void *)newsides, 0, sizeof(side_t)*zlvl.Sides.Size());
    for (size_t f = 0; f < zlvl.Sides.Size(); ++f) {
      const ZDBSP::IntSideDef &zside = zlvl.Sides[f];
      side_t *nside = &newsides[f];
      *nside = Sides[zside.origindex];
      check(zside.sector >= 0 && zside.sector < NumSectors);
      nside->Sector = &Sectors[zside.sector];
      //!nside->LineNum = ldmap[nside->LineNum];
      check(nside->LineNum >= 0);
    }
    delete [] Sides;
    NumSides = (int)zlvl.Sides.Size();
    Sides = newsides;
  }
  */

  // copy subsectors
  {
    NumSubsectors = subCount;
    delete [] Subsectors;
    Subsectors = new subsector_t[subCount];
    memset((void *)Subsectors, 0, sizeof(subsector_t)*subCount);
    subsector_t *destss = &Subsectors[0];
    for (int i = 0; i < subCount; ++i, ++destss) {
      ZDBSP::MapSubsectorEx &srcss = ssecs[i];
      destss->numlines = srcss.numlines;
      destss->firstline = srcss.firstline;
    }
    delete [] ssecs;
  }

  // copy segs
  {
    NumSegs = segCount;
    delete [] Segs;
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
    delete [] segs;
  }

  // copy nodes
  {
    NumNodes = nodeCount;
    delete [] Nodes;
    Nodes = new node_t[NumNodes];
    memset((void *)Nodes, 0, sizeof(node_t)*NumNodes);
    for (int f = 0; f < nodeCount; ++f) CopyNodeZD(f, nodes[f], &Nodes[f]);
    delete [] nodes;
  }

  // clear blockmap (just in case), so it will be recreated by the engine
  delete [] BlockMapLump;
  BlockMapLump = nullptr;
  BlockMapLumpSize = 0;
}
