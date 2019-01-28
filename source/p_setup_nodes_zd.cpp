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
    zsec.data.tag = pSrc->tag;
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
    zlvl.Sides.Push(zside);
  }
}


//==========================================================================
//
//  UploadLinedefsZD
//
//==========================================================================
static void UploadLinedefsZD (VLevel *Level, ZDBSP::FLevel &zlvl) {
  zlvl.Vertices = new ZDBSP::WideVertex[Level->NumVertexes];
  zlvl.NumVertices = Level->NumVertexes;

  for (int f = 0; f < Level->NumVertexes; ++f) {
    //vertmap[f] = -1;
    zlvl.Vertices[f].x = toFix((double)Level->Vertexes[f].x);
    zlvl.Vertices[f].y = toFix((double)Level->Vertexes[f].y);
    zlvl.Vertices[f].index = f;
  }

  const line_t *pSrc = Level->Lines;
  for (int i = 0; i < Level->NumLines; ++i, ++pSrc) {
    if (!pSrc->v1 || !pSrc->v2) Sys_Error("VAVOOM: linedef without vertexes");

    ZDBSP::IntLineDef line;
    line.flags = pSrc->flags;
    line.special = pSrc->special;
    line.args[0] = pSrc->arg1;
    line.args[1] = pSrc->arg2;
    line.args[2] = pSrc->arg3;
    line.args[3] = pSrc->arg4;
    line.args[4] = pSrc->arg5;
    line.sidenum[0] = (pSrc->sidenum[0] >= 0 ? pSrc->sidenum[0] : ZDBSP::NO_INDEX);
    line.sidenum[1] = (pSrc->sidenum[1] >= 0 ? pSrc->sidenum[1] : ZDBSP::NO_INDEX);

    int lv1idx = (int)(ptrdiff_t)(pSrc->v1-Level->Vertexes);
    int lv2idx = (int)(ptrdiff_t)(pSrc->v2-Level->Vertexes);
    if (lv1idx < 0 || lv2idx < 0 || lv1idx >= Level->NumVertexes || lv2idx >= Level->NumVertexes) Sys_Error("VAVOOM: invalid linedef vertexes");
    line.v1 = lv1idx;
    line.v2 = lv2idx;

    zlvl.Lines.Push(line);
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
  Node->bbox[0][2] = -32768.0;
  Node->bbox[0][3] = SrcNode.bbox[0][ZDBSP::BOXRIGHT]; // maxx
  Node->bbox[0][4] = SrcNode.bbox[0][ZDBSP::BOXTOP]; // maxy
  Node->bbox[0][5] = 32768.0;

  Node->bbox[1][0] = SrcNode.bbox[1][ZDBSP::BOXLEFT]; // minx
  Node->bbox[1][1] = SrcNode.bbox[1][ZDBSP::BOXBOTTOM]; // miny
  Node->bbox[1][2] = -32768.0;
  Node->bbox[1][3] = SrcNode.bbox[1][ZDBSP::BOXRIGHT]; // maxx
  Node->bbox[1][4] = SrcNode.bbox[1][ZDBSP::BOXTOP]; // maxy
  Node->bbox[1][5] = 32768.0;

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
  if (total <= 0) {
    R_PBarUpdate("BSP", 42, 42, true); // final update
  } else {
    R_PBarUpdate("BSP", curr, total);
  }
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

  // copy vertices, fix linedefs
  {
    //const int oldmaxverts = NumVertexes;
    //vertex_t *oldvx = Vertexes;
    delete [] Vertexes;

    NumVertexes = newvertsCount;
    Vertexes = new vertex_t[newvertsCount];
    memset((void *)Vertexes, 0, sizeof(vertex_t)*newvertsCount);

    for (int vidx = 0; vidx < newvertsCount; ++vidx) {
      Vertexes[vidx].x = fromFix(newverts[vidx].x);
      Vertexes[vidx].y = fromFix(newverts[vidx].y);
      Vertexes[vidx].z = 0;
    }

    for (int lidx = 0; lidx < NumLines; ++lidx) {
      Lines[lidx].v1 = &Vertexes[zlvl.Lines[lidx].v1];
      Lines[lidx].v2 = &Vertexes[zlvl.Lines[lidx].v2];
    }
  }

  // copy nodes
  {
    NumNodes = nodeCount;
    delete [] Nodes;
    Nodes = new node_t[NumNodes];
    memset((void *)Nodes, 0, sizeof(node_t)*NumNodes);
    for (int f = 0; f < nodeCount; ++f) CopyNodeZD(f, nodes[f], &Nodes[f]);
  }

  // copy segs
  {
    NumSegs = 0;
    delete [] Segs;
    Segs = new seg_t[segCount];
    memset((void *)Segs, 0, sizeof(seg_t)*segCount);

    for (int i = 0; i < segCount; ++i) {
      //nfo.ajseg2vv[i] = -1;
      const ZDBSP::MapSegGLEx &zseg = segs[i];

      const int dsnum = NumSegs++;
      //nfo.ajseg2vv[i] = dsnum;

      seg_t *destseg = &Segs[dsnum];

      destseg->partner = nullptr;
      destseg->front_sub = nullptr;

      if (zseg.v1 == zseg.v2) Sys_Error("ZDBSP: seg #%d has same vertices (%d)", i, (int)zseg.v1);

      destseg->v1 = &Vertexes[zseg.v1];
      destseg->v2 = &Vertexes[zseg.v2];

      if (zseg.side != 0 && zseg.side != 1) Sys_Error("ZDBSP: invalid seg #%d side (%d)", i, zseg.side);

      if (zseg.linedef != ZDBSP::NO_INDEX) {
        if ((int)zseg.linedef < 0 || (int)zseg.linedef >= NumLines) Sys_Error("ZDBSP: invalid seg #%d linedef (%d), max is %d", i, zseg.linedef, NumLines-1);

        line_t *ldef = &Lines[zseg.linedef];
        destseg->linedef = ldef;

        if (ldef->sidenum[zseg.side] < 0 || ldef->sidenum[zseg.side] >= NumSides) {
          Sys_Error("ZDBSP: seg #%d: ldef=%d; seg->side=%d; sidenum=%d (max sidenum is %d)\n", i, zseg.linedef, zseg.side, ldef->sidenum[zseg.side], NumSides-1);
        }

        destseg->sidedef = &Sides[ldef->sidenum[zseg.side]];
        destseg->frontsector = Sides[ldef->sidenum[zseg.side]].Sector;

        if (ldef->flags&ML_TWOSIDED) {
          if (ldef->sidenum[zseg.side^1] < 0 || ldef->sidenum[zseg.side^1] >= NumSides) Sys_Error("ZDBSP: another side of two-sided linedef is fucked");
          destseg->backsector = Sides[ldef->sidenum[zseg.side^1]].Sector;
        } else if (ldef->sidenum[zseg.side^1] >= 0) {
          if (ldef->sidenum[zseg.side^1] >= NumSides) Sys_Error("ZDBSP: another side of blocking two-sided linedef is fucked");
          destseg->backsector = Sides[ldef->sidenum[zseg.side^1]].Sector;
          // not a two-sided, so clear backsector (just in case) -- nope
          //destseg->backsector = nullptr;
        } else {
          destseg->backsector = nullptr;
          ldef->flags &= ~ML_TWOSIDED; // just in case
        }

        if (zseg.side) {
          destseg->offset = Length(*destseg->v1 - *ldef->v2);
        } else {
          destseg->offset = Length(*destseg->v1 - *ldef->v1);
        }
      }

      destseg->length = Length(*destseg->v2 - *destseg->v1);
      destseg->side = zseg.side;

      if (destseg->length < 0.000001f) Sys_Error("ZDBSP: zero-length seg #%d (%d:%d) (%f,%f)(%f,%f)", i, zseg.v1, zseg.v2, destseg->v1->x, destseg->v1->y, destseg->v2->x, destseg->v2->y);

      destseg->partner = (zseg.partner == ZDBSP::NO_INDEX ? nullptr : &Segs[zseg.partner]);

      // calc seg's plane params
      CalcSeg(destseg);
    }
  }

  // copy subsectors
  {
    NumSubsectors = subCount;
    delete [] Subsectors;
    Subsectors = new subsector_t[subCount];
    memset((void *)Subsectors, 0, sizeof(subsector_t)*subCount);
    for (int i = 0; i < subCount; ++i) {
      ZDBSP::MapSubsectorEx &srcss = ssecs[i];

      subsector_t *destss = &Subsectors[i];
      destss->numlines = srcss.numlines;
      destss->firstline = srcss.firstline;

      // check sector numbers
      /*
      for (int j = 0; j < destss->numlines; ++j) {
        auto i2idx = nfo.ajseg2vv[ajidx+j];
        if (i2idx < 0) Host_Error("AJBSP: subsector #%d contains miniseg or degenerate seg #%d (%d)", i, ajidx+j, j);
        if (i2idx != destss->firstline+j) Host_Error("AJBSP: subsector #%d contains non-sequential segs", i);
      }
      */

      // setup sector links
      seg_t *seg = &Segs[destss->firstline];
      for (int j = 0; j < destss->numlines; ++j) {
        if (seg[j].linedef) {
          destss->sector = seg[j].sidedef->Sector;
          destss->seclink = destss->sector->subsectors;
          destss->sector->subsectors = destss;
          break;
        }
      }

      // setup front_sub
      for (int j = 0; j < destss->numlines; j++) seg[j].front_sub = destss;
      if (!destss->sector) Host_Error("ZDBSP: Subsector #%d without sector", i);
    }

    int setcount = NumSegs;
    for (int f = 0; f < NumSegs; ++f) {
      if (!Segs[f].front_sub) { GCon->Logf("ZDBSP: Seg %d: front_sub is not set!", f); --setcount; }
      if (Segs[f].sidedef &&
          ((ptrdiff_t)Segs[f].sidedef < (ptrdiff_t)Sides ||
           (ptrdiff_t)(Segs[f].sidedef-Sides) >= NumSides))
      {
        Sys_Error("ZDBSP: seg %d has invalid sidedef (%d)", f, (int)(ptrdiff_t)(Segs[f].sidedef-Sides));
      }
    }

    if (setcount != NumSegs) GCon->Logf(NAME_Warning, "ZDBSP: %d of %d segs has no front_sub!", NumSegs-setcount, NumSegs);
  }

  delete [] ssecs;
  delete [] segs;
  delete [] nodes;
  delete [] newverts;


  {
    // blockmap
    delete [] BlockMapLump;
    BlockMapLump = nullptr;
    BlockMapLumpSize = 0;
    GCon->Logf("ZDBSP: creating BLOCKMAP...");
    CreateBlockMap();
  }
}
