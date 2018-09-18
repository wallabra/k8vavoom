//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**    $Id$
//**
//**    Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**    This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**    This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
//**
//**    Build nodes using glBSP.
//**
//**************************************************************************
#include "gamedefs.h"
#include "filesys/fwaddefs.h"
#include "ajbsp/bsp.h"

#define glbsp_vertex_t  ajbsp::vertex_t
#define glbsp_sector_t  ajbsp::sector_t
#define glbsp_subsec_t  ajbsp::subsec_t
#define glbsp_seg_t  ajbsp::seg_t
#define glbsp_node_t  ajbsp::node_t
#define glbsp_sidedef_t  ajbsp::sidedef_t
#define glbsp_linedef_t  ajbsp::linedef_t
#define glbsp_thing_t  ajbsp::thing_t
//extern boolean_g lev_doing_normal;
//extern boolean_g lev_doing_hexen;
namespace ajbsp {
  extern bool lev_doing_hexen;
  extern int num_old_vert;
  extern int num_new_vert;
  extern int num_complete_seg;
  extern int num_real_lines;
  extern int num_vertices;
  extern int num_linedefs;
  extern int num_sidedefs;
  extern int num_sectors;
  extern int num_things;
  extern int num_segs;
  extern int num_subsecs;
  extern int num_nodes;

  static vuint32 GetTypeHash (const seg_t *sg) { return (vuint32)(ptrdiff_t)sg; }
  //static vuint32 GetTypeHash (const vertex_t *vx) { return (vuint32)(ptrdiff_t)vx; }
}

// for pvs
#include "vector2d.h"


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB loader_build_pvs("loader_build_pvs", false, "Build simple PVS on node rebuilding?", CVAR_Archive);
static VCvarI loader_pvs_builder_threads("loader_pvs_builder_threads", "3", "Number of threads to use in PVS builder.", CVAR_Archive);

//static VCvarB nodes_detect_window_fx("nodes_detect_window_fx", false, "Use \"window fx\" glBSP detector?", CVAR_Archive);
static VCvarB nodes_fast_and_bad("nodes_fast_and_bad", false, "Do faster rebuild, but generate worser BSP tree?", CVAR_Archive);

static char message_buf[1024];


// Lump order in a map WAD: each map needs a couple of lumps
// to provide a complete scene geometry description.
enum {
  ML_LABEL, // A separator, name, ExMx or MAPxx
  ML_THINGS,  // Monsters, items..
  ML_LINEDEFS, // LineDefs, from editing
  ML_SIDEDEFS, // SideDefs, from editing
  ML_VERTEXES, // Vertices, edited and BSP splits generated
  ML_SEGS, // LineSegs, from LineDefs split by BSP
  ML_SSECTORS, // SubSectors, list of LineSegs
  ML_NODES, // BSP nodes
  ML_SECTORS, // Sectors, from editing
  ML_REJECT, // LUT, sector-sector visibility
  ML_BLOCKMAP, // LUT, motion clipping, walls/grid element
  ML_BEHAVIOR, // ACS scripts
};


//==========================================================================
//
//  stripNL
//
//==========================================================================
static void stripNL (char *str) {
  if (!str) return;
  auto slen = strlen(str);
  while (slen > 0 && (str[slen-1] == '\n' || str[slen-1] == '\r')) str[--slen] = '\0';
}


//==========================================================================
//
//  ajbsp_FatalError
//
//==========================================================================
__attribute__((noreturn)) __attribute__((format(printf,1,2))) void ajbsp_FatalError (const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  vsnprintf(message_buf, sizeof(message_buf), fmt, args);
  va_end(args);
  stripNL(message_buf);

  Sys_Error("AJBSP: %s", message_buf);
}


//==========================================================================
//
//  ajbsp_PrintMsg
//
//==========================================================================
__attribute__((format(printf,1,2))) void ajbsp_PrintMsg (const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  vsnprintf(message_buf, sizeof(message_buf), fmt, args);
  va_end(args);
  stripNL(message_buf);

  GCon->Logf("AJBSP: %s", message_buf);
}


//==========================================================================
//
//  ajbsp_PrintVerbose
//
//==========================================================================
__attribute__((format(printf,1,2))) void ajbsp_PrintVerbose (const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  vsnprintf(message_buf, sizeof(message_buf), fmt, args);
  va_end(args);
  stripNL(message_buf);

  GCon->Logf("AJBSP: %s", message_buf);
}


//==========================================================================
//
//  ajbsp_PrintDetail
//
//==========================================================================
__attribute__((format(printf,1,2))) void ajbsp_PrintDetail (const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  vsnprintf(message_buf, sizeof(message_buf), fmt, args);
  va_end(args);
  stripNL(message_buf);

  GCon->Logf("AJBSP: %s", message_buf);
}


//==========================================================================
//
//  ajbsp_DebugPrintf
//
//==========================================================================
__attribute__((format(printf,1,2))) void ajbsp_DebugPrintf (const char *fmt, ...) {
}


//==========================================================================
//
//  ajbsp_PrintMapName
//
//==========================================================================
void ajbsp_PrintMapName (const char *name) {
}


//==========================================================================
//
//  SetUpVertices
//
//==========================================================================
static void SetUpVertices (VLevel *Level) {
  guard(SetUpVertices);
  ajbsp::num_old_vert = 0;
  ajbsp::num_new_vert = 0;
  ajbsp::num_complete_seg = 0;
  ajbsp::num_real_lines = 0;
  ajbsp::num_vertices = 0;
  ajbsp::num_linedefs = 0;
  ajbsp::num_sidedefs = 0;
  ajbsp::num_sectors = 0;
  ajbsp::num_things = 0;
  ajbsp::num_segs = 0;
  ajbsp::num_subsecs = 0;
  ajbsp::num_nodes = 0;
  vertex_t *pSrc = Level->Vertexes;
  for (int i = 0; i < Level->NumVertexes; ++i, ++pSrc) {
    glbsp_vertex_t *Vert = ajbsp::NewVertex();
    Vert->x = pSrc->x;
    Vert->y = pSrc->y;
    Vert->index = i;
  }
  ajbsp::num_old_vert = ajbsp::num_vertices;
  unguard;
}


//==========================================================================
//
//  SetUpSectors
//
//==========================================================================
static void SetUpSectors (VLevel *Level) {
  guard(SetUpSectors);
  sector_t *pSrc = Level->Sectors;
  for (int i = 0; i < Level->NumSectors; ++i, ++pSrc) {
    glbsp_sector_t *sector = ajbsp::NewSector();
    sector->coalesce = (pSrc->tag >= 900 && pSrc->tag < 1000 ? 1 : 0);
    // sector indices never change
    sector->index = i;
    sector->warned_facing = -1;
  }
  unguard;
}


//==========================================================================
//
//  SetUpSidedefs
//
//==========================================================================
static void SetUpSidedefs (VLevel *Level) {
  guard(SetUpSidedefs);
  side_t *pSrc = Level->Sides;
  for (int i = 0; i < Level->NumSides; ++i, ++pSrc) {
    glbsp_sidedef_t *side = ajbsp::NewSidedef();
    side->sector = (!pSrc->Sector ? nullptr : ajbsp::LookupSector((int)(ptrdiff_t)(pSrc->Sector-Level->Sectors)));
    if (side->sector) side->sector->is_used = 1;
    // sidedef indices never change
    side->index = i;
  }
  unguard;
}


//==========================================================================
//
//  SetUpLinedefs
//
//==========================================================================
static void SetUpLinedefs (VLevel *Level) {
  guard(SetUpLinedefs);
  line_t *pSrc = Level->Lines;
  for (int i = 0; i < Level->NumLines; ++i, ++pSrc) {
    glbsp_linedef_t *line = ajbsp::NewLinedef();
    if (line == nullptr) Sys_Error("AJBSP: out of memory!");
    line->start = ajbsp::LookupVertex((int)(ptrdiff_t)(pSrc->v1-Level->Vertexes));
    line->end = ajbsp::LookupVertex((int)(ptrdiff_t)(pSrc->v2-Level->Vertexes));
    line->start->is_used = 1;
    line->end->is_used = 1;
    line->zero_len = (fabs(line->start->x-line->end->x) < DIST_EPSILON) && (fabs(line->start->y-line->end->y) < DIST_EPSILON);
    line->flags = pSrc->flags;
    line->type = pSrc->special;
    line->two_sided = (pSrc->flags&ML_TWOSIDED ? 1 : 0);
    line->is_precious = (pSrc->arg1 >= 900 && pSrc->arg1 < 1000 ? 1 : 0); // arg1 is tag
    line->right = (pSrc->sidenum[0] < 0 ? nullptr : ajbsp::LookupSidedef(pSrc->sidenum[0]));
    line->left = (pSrc->sidenum[1] < 0 ? nullptr : ajbsp::LookupSidedef(pSrc->sidenum[1]));
    if (line->right) {
      line->right->is_used = 1;
      line->right->on_special |= (line->type > 0 ? 1 : 0);
    }
    if (line->left) {
      line->left->is_used = 1;
      line->left->on_special |= (line->type > 0 ? 1 : 0);
    }
    if (line->right || line->left) ++ajbsp::num_real_lines;
    line->self_ref = (line->left && line->right && line->left->sector == line->right->sector);
    line->index = i;
  }
  unguard;
}


//==========================================================================
//
//  SetUpThings
//
//==========================================================================
static void SetUpThings (VLevel *Level) {
  guard(SetUpThings);
  mthing_t *pSrc = Level->Things;
  for (int i = 0; i < Level->NumThings; ++i, ++pSrc) {
    glbsp_thing_t *Thing = ajbsp::NewThing();
    Thing->x = (int)pSrc->x;
    Thing->y = (int)pSrc->y;
    Thing->type = pSrc->type;
    Thing->options = pSrc->options;
    Thing->index = i;
  }
  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //
struct CopyInfo {
  TMapNC<glbsp_seg_t *, int> glsegptr2idx;
  TMapNC<int, int> ajseg2vaseg; // key: ajbsp seg number; value: vavoom seg number
  TMapNC<int, int> ajvx2vavx; // key: ajbsp vertex index; value: vavoom vertex index
};


//==========================================================================
//
//  CopyGLVerts
//
//==========================================================================
static void CopyGLVerts (VLevel *Level, CopyInfo &nfo) {
  guard(CopyGLVerts);

  // copy new vertices, build linedef vertex translation table
  TMapNC<int, int> vxmap; // key: linedef index; value: ajbsp vertex index
  vertex_t *oldvx = Level->Vertexes;
  //fprintf(stderr, "num_vertices=%d; num_old_vert=%d; num_new_vert=%d\n", ajbsp::num_vertices, ajbsp::num_old_vert, ajbsp::num_new_vert);
  //Level->NumVertexes = ajbsp::num_old_vert+ajbsp::num_new_vert; // same as `ajbsp::num_vertices`
  int maxverts = ajbsp::num_old_vert+ajbsp::num_new_vert; // same as `ajbsp::num_vertices`
  Level->NumVertexes = 0;
  Level->Vertexes = new vertex_t[maxverts]; //k8: this overallocates, but i don't care
  memset((void *)Level->Vertexes, 0, sizeof(vertex_t)*maxverts);
  vertex_t *pDst = Level->Vertexes;
  for (int i = 0; i < ajbsp::num_vertices; ++i) {
    glbsp_vertex_t *vert = ajbsp::LookupVertex(i);
    if (!vert->is_used) continue;
    nfo.ajvx2vavx.put(i, Level->NumVertexes);
    *pDst = TVec(vert->x, vert->y, 0);
    if (!vert->is_new) {
      if (vxmap.has(vert->index)) Sys_Error("AJBSP: duplicate old vertex index");
      vxmap.put(vert->index, Level->NumVertexes);
    }
    ++pDst;
    ++Level->NumVertexes;
  }
  GCon->Logf("AJBSP: copied %d of %d used vertexes", Level->NumVertexes, maxverts);

  // update pointers to vertexes in lines
  for (int i = 0; i < Level->NumLines; ++i) {
    line_t *ld = &Level->Lines[i];
    int v1idx = (int)(ptrdiff_t)(ld->v1-oldvx);
    int v2idx = (int)(ptrdiff_t)(ld->v2-oldvx);
    auto v1mp = vxmap.find(v1idx);
    auto v2mp = vxmap.find(v2idx);
    if (!v1mp) Sys_Error("AJBSP: old vertex not found");
    if (!v2mp) Sys_Error("AJBSP: old vertex not found");
    ld->v1 = &Level->Vertexes[*v1mp];
    ld->v2 = &Level->Vertexes[*v2mp];
  }
  delete[] oldvx;
  unguard;
}


//==========================================================================
//
//  CopySegs
//
//==========================================================================
static void CopySegs (VLevel *Level, CopyInfo &nfo) {
  guard(CopySegs);

  // build ordered list of source segs
  Level->NumSegs = 0;
  delete [] Level->Segs;
  Level->Segs = new seg_t[ajbsp::num_segs]; //k8: this overallocates, but i don't care
  memset((void *)Level->Segs, 0, sizeof(seg_t)*ajbsp::num_segs);

  glbsp_seg_t **partners = new glbsp_seg_t *[ajbsp::num_segs];
  memset((void *)partners, 0, sizeof(glbsp_seg_t *)*ajbsp::num_segs);

  for (int i = 0; i < ajbsp::num_segs; ++i) {
    glbsp_seg_t *srcseg = ajbsp::LookupSeg(i);
    // ignore minisegs and degenerate segs
    if (!srcseg->linedef || srcseg->is_degenerate) continue;
    if (srcseg->index != i) Host_Error("AJBSP: seg #%d has invalid index %d", i, srcseg->index);

    seg_t *destseg = Level->Segs+Level->NumSegs;
    partners[Level->NumSegs] = srcseg->partner;
    nfo.glsegptr2idx.put(srcseg, Level->NumSegs); // for partner setup
    nfo.ajseg2vaseg.put(i, Level->NumSegs);
    ++Level->NumSegs;
    destseg->partner = nullptr;
    destseg->front_sub = nullptr;

    auto v1mp = nfo.ajvx2vavx.find(srcseg->start->index);
    auto v2mp = nfo.ajvx2vavx.find(srcseg->end->index);
    if (!v1mp) Sys_Error("AJBSP: vertex not found");
    if (!v2mp) Sys_Error("AJBSP: vertex not found");
    destseg->v1 = &Level->Vertexes[*v1mp];
    destseg->v2 = &Level->Vertexes[*v2mp];

    if (srcseg->side != 0 && srcseg->side != 1) Sys_Error("AJBSP: invalid seg #%d side (%d)", i, srcseg->side);

    if (srcseg->linedef->index < 0 || srcseg->linedef->index >= Level->NumLines) Sys_Error("AJBSP: invalid seg #%d linedef (%d), max is %d", i, srcseg->linedef->index, Level->NumLines-1);

    line_t *ldef = &Level->Lines[srcseg->linedef->index];
    destseg->linedef = ldef;

    if (ldef->sidenum[srcseg->side] < 0 || ldef->sidenum[srcseg->side] >= Level->NumSides) {
      if (srcseg->side == 1) {
        if ((ldef->flags&ML_TWOSIDED) != 0) {
          GCon->Logf("ERROR: linedef #%d is marked as a two-sided, but has no second side!", srcseg->linedef->index);
        } else {
          GCon->Logf("ERROR: linedef #%d is not a two-sided, but has seg for the second side!", srcseg->linedef->index);
        }
      }
      Sys_Error("AJBSP: seg #%d: ldef=%d; seg->side=%d; sidenum=%d (max sidenum is %d)\n", i, srcseg->linedef->index, srcseg->side, ldef->sidenum[srcseg->side], Level->NumSides-1);
    }

    //fprintf(stderr, "seg #%d: ldef=%d; seg->side=%d; sidenum=%d\n", i, SrcSeg->linedef->index, SrcSeg->side, ldef->sidenum[SrcSeg->side]);
    destseg->sidedef = &Level->Sides[ldef->sidenum[srcseg->side]];
    destseg->frontsector = Level->Sides[ldef->sidenum[srcseg->side]].Sector;

    if (ldef->flags&ML_TWOSIDED) {
      destseg->backsector = Level->Sides[ldef->sidenum[srcseg->side^1]].Sector;
    }

    if (srcseg->side) {
      destseg->offset = Length(*destseg->v1 - *ldef->v2);
    } else {
      destseg->offset = Length(*destseg->v1 - *ldef->v1);
    }
    destseg->length = Length(*destseg->v2 - *destseg->v1);
    destseg->side = srcseg->side;

    // calc seg's plane params
    CalcSeg(destseg);
  }

  // setup partners (we need 'em for self-referencing deep water)
  for (int i = 0; i < Level->NumSegs; ++i) {
    if (!partners[i]) continue; // no partner for this seg
    seg_t *destseg = Level->Segs+i;
    auto sidp = nfo.glsegptr2idx.find(partners[i]);
    if (sidp) {
      if (*sidp < 0 || *sidp >= Level->NumSegs) Sys_Error("AJBSP: invalid partner seg");
      destseg->partner = &Level->Segs[*sidp];
    }
    if (!destseg->partner) GCon->Logf("GLBSP: invalid partner (ignored)");
  }

  delete[] partners;
  unguard;
}


//==========================================================================
//
//  CopySubsectors
//
//==========================================================================
static void CopySubsectors (VLevel *Level, CopyInfo &nfo) {
  guard(CopySubsectors);
  Level->NumSubsectors = ajbsp::num_subsecs;
  delete [] Level->Subsectors;
  Level->Subsectors = new subsector_t[Level->NumSubsectors];
  memset((void *)Level->Subsectors, 0, sizeof(subsector_t)*Level->NumSubsectors);
  subsector_t *ss = Level->Subsectors;
  for (int i = 0; i < Level->NumSubsectors; ++i, ++ss) {
    glbsp_subsec_t *SrcSub = ajbsp::LookupSubsec(i);
    ss->numlines = SrcSub->seg_count;
    int ajidx = SrcSub->seg_list->index;
    auto flidxp = nfo.ajseg2vaseg.find(ajidx);
    if (!flidxp) Host_Error("AJBSP: subsector #%d starts with miniseg or degenerate seg", i);
    ss->firstline = *flidxp;
    // setup sector links
    seg_t *seg = &Level->Segs[ss->firstline];
    for (int j = 0; j < ss->numlines; ++j, ++ajidx) {
      auto ip2 = nfo.ajseg2vaseg.find(ajidx);
      if (!ip2) Host_Error("AJBSP: subsector #%d contains miniseg or degenerate seg", i);
      if (*ip2 != ss->firstline+j) Host_Error("AJBSP: subsector #%d contains non-sequential segs", i);
      if (seg[j].linedef) {
        ss->sector = seg[j].sidedef->Sector;
        ss->seclink = ss->sector->subsectors;
        ss->sector->subsectors = ss;
        break;
      }
    }
    // setup front_sub
    for (int j = 0; j < ss->numlines; j++) seg[j].front_sub = ss;
    if (!ss->sector) Host_Error("AJBSP: Subsector #%d without sector", i);
  }

  int setcount = Level->NumSegs;
  for (int f = 0; f < Level->NumSegs; ++f) {
    if (!Level->Segs[f].front_sub) { GCon->Logf("AJBSP: Seg %d: front_sub is not set!", f); --setcount; }
    if (Level->Segs[f].sidedef &&
        ((ptrdiff_t)Level->Segs[f].sidedef < (ptrdiff_t)Level->Sides ||
         (ptrdiff_t)(Level->Segs[f].sidedef-Level->Sides) >= Level->NumSides))
    {
      Sys_Error("AJBSP: seg %d has invalid sidedef (%d)", f, (int)(ptrdiff_t)(Level->Segs[f].sidedef-Level->Sides));
    }
  }

  if (setcount != Level->NumSegs) GCon->Logf("AJBSP: WARNING: %d of %d segs has no front_sub!", Level->NumSegs-setcount, Level->NumSegs);
  unguard;
}


//==========================================================================
//
//  CopyNode
//
//==========================================================================
static void CopyNode (int &NodeIndex, glbsp_node_t *SrcNode, node_t *Nodes) {
  if (SrcNode->r.node) CopyNode(NodeIndex, SrcNode->r.node, Nodes);
  if (SrcNode->l.node) CopyNode(NodeIndex, SrcNode->l.node, Nodes);

  SrcNode->index = NodeIndex;

  node_t *Node = &Nodes[NodeIndex];
  //Node->SetPointDir(TVec(SrcNode->x, SrcNode->y, 0), TVec(SrcNode->dx, SrcNode->dy, 0));
  if (SrcNode->too_long) {
    Node->SetPointDir(TVec(SrcNode->x, SrcNode->y, 0), TVec(SrcNode->dx, SrcNode->dy, 0));
  } else {
    Node->SetPointDir(TVec(SrcNode->x, SrcNode->y, 0), TVec(SrcNode->dx/2, SrcNode->dy/2, 0));
  }

  Node->bbox[0][0] = SrcNode->r.bounds.minx;
  Node->bbox[0][1] = SrcNode->r.bounds.miny;
  Node->bbox[0][2] = -32768.0;
  Node->bbox[0][3] = SrcNode->r.bounds.maxx;
  Node->bbox[0][4] = SrcNode->r.bounds.maxy;
  Node->bbox[0][5] = 32768.0;

  Node->bbox[1][0] = SrcNode->l.bounds.minx;
  Node->bbox[1][1] = SrcNode->l.bounds.miny;
  Node->bbox[1][2] = -32768.0;
  Node->bbox[1][3] = SrcNode->l.bounds.maxx;
  Node->bbox[1][4] = SrcNode->l.bounds.maxy;
  Node->bbox[1][5] = 32768.0;

  if (SrcNode->r.node) {
    Node->children[0] = SrcNode->r.node->index;
  } else if (SrcNode->r.subsec) {
    Node->children[0] = SrcNode->r.subsec->index|NF_SUBSECTOR;
  }

  if (SrcNode->l.node) {
    Node->children[1] = SrcNode->l.node->index;
  } else if (SrcNode->l.subsec) {
    Node->children[1] = SrcNode->l.subsec->index|NF_SUBSECTOR;
  }

  ++NodeIndex;
}


//==========================================================================
//
//  CopyNodes
//
//==========================================================================
static void CopyNodes (VLevel *Level, glbsp_node_t *root_node) {
  guard(CopyNodes);
  Level->NumNodes = ajbsp::num_nodes;
  delete [] Level->Nodes;
  Level->Nodes = new node_t[Level->NumNodes];
  memset((void *)Level->Nodes, 0, sizeof(node_t)*Level->NumNodes);
  if (root_node) {
    int NodeIndex = 0;
    CopyNode(NodeIndex, root_node, Level->Nodes);
  }
  unguard;
}


//==========================================================================
//
//  VLevel::BuildNodes
//
//==========================================================================
void VLevel::BuildNodes () {
  guard(VLevel::BuildNodes);
  // set up glBSP build globals
  nodebuildinfo_t nb_info;
  nb_info.fast = nodes_fast_and_bad;
  nb_info.warnings = true; // not currently used, but meh
  nb_info.do_blockmap = true;
  nb_info.do_reject = true;

  ajbsp::cur_info = &nb_info;

  //lev_doing_normal = false;
  ajbsp::lev_doing_hexen = !!(LevelFlags&LF_Extended);

  // set up map data from loaded data
  SetUpVertices(this);
  SetUpSectors(this);
  SetUpSidedefs(this);
  SetUpLinedefs(this);
  SetUpThings(this);

  // other data initialisation
  // always prune vertices at end of lump, otherwise all the
  // unused vertices from seg splits would keep accumulating.
  ajbsp::PruneVerticesAtEnd();

  ajbsp::DetectOverlappingVertices();
  ajbsp::DetectOverlappingLines();

  ajbsp::CalculateWallTips();

  if (ajbsp::lev_doing_hexen) ajbsp::DetectPolyobjSectors(); // -JL- Find sectors containing polyobjs

  //if (cur_info->window_fx) ajbsp::DetectWindowEffects();
  ajbsp::InitBlockmap();

  // create initial segs
  ajbsp::superblock_t *seg_list = ajbsp::CreateSegs();
  glbsp_node_t *root_node;
  ajbsp::subsec_t *root_sub;
  ajbsp::bbox_t seg_bbox;
  ajbsp::FindLimits(seg_list, &seg_bbox);
  build_result_e ret = ajbsp::BuildNodes(seg_list, &root_node, &root_sub, 0, &seg_bbox);
  ajbsp::FreeSuper(seg_list);

  if (ret == build_result_e::BUILD_OK) {
    ajbsp::ClockwiseBspTree();
    ajbsp::CheckLimits();
    ajbsp::SortSegs();

    GCon->Logf("AJBSP: copying built data");
    // copy nodes into internal structures
    CopyInfo nfo;
    CopyGLVerts(this, nfo);
    CopySegs(this, nfo);
    CopySubsectors(this, nfo);

    ajbsp::NormaliseBspTree(); // remove all the mini-segs
    CopyNodes(this, root_node);

    // reject
    if (ajbsp::cur_info->do_reject) {
      VMemoryStream *xms = new VMemoryStream();
      xms->BeginWrite();
      ajbsp::PutReject(*xms);

      delete [] RejectMatrix;
      RejectMatrix = nullptr;

      RejectMatrixSize = xms->TotalSize();
      if (RejectMatrixSize) {
        TArray<vuint8> &arr = xms->GetArray();
        RejectMatrix = new vuint8[RejectMatrixSize];
        memcpy(RejectMatrix, arr.ptr(), RejectMatrixSize);
        // check if it's an all-zeroes lump, in which case it's useless and can be discarded
        // k8: don't do it, or VaVoom will try to rebuild/reload it
        /*
        bool blank = true;
        for (int i = 0; i < RejectMatrixSize; ++i) if (RejectMatrix[i]) { blank = false; break; }
        if (Blank) {
          RejectMatrixSize = 0;
          delete [] RejectMatrix;
          RejectMatrix = nullptr;
        }
        */
      }
      delete xms;
    }

    // blockmap
    //FIXME: remove pasta (see p_setup.cpp:LoadBlockMap())
    if (ajbsp::cur_info->do_blockmap) {
      // killough 3/1/98: Expand wad blockmap into larger internal one,
      // by treating all offsets except -1 as unsigned and zero-extending
      // them. This potentially doubles the size of blockmaps allowed,
      // because Doom originally considered the offsets as always signed.

      delete [] BlockMapLump;
      BlockMapLump = nullptr;
      BlockMapLumpSize = 0;

      VMemoryStream *xms = new VMemoryStream();
      xms->BeginWrite();
      ajbsp::PutBlockmap(*xms);

      // allocate memory for blockmap
      int count = xms->TotalSize()/2;
      xms->Seek(0);
      xms->BeginRead();

      BlockMapLump = new vint32[count];
      BlockMapLumpSize = count;

      // read data
      BlockMapLump[0] = Streamer<vint16>(*xms);
      BlockMapLump[1] = Streamer<vint16>(*xms);
      BlockMapLump[2] = Streamer<vuint16>(*xms);
      BlockMapLump[3] = Streamer<vuint16>(*xms);
      for (int i = 4; i < count; ++i) {
        vint16 tmp;
        *xms << tmp;
        BlockMapLump[i] = (tmp == -1 ? -1 : (vuint16)tmp&0xffff);
      }

      delete xms;

      // read blockmap origin and size
      BlockMapOrgX = BlockMapLump[0];
      BlockMapOrgY = BlockMapLump[1];
      BlockMapWidth = BlockMapLump[2];
      BlockMapHeight = BlockMapLump[3];
      BlockMap = BlockMapLump+4;

      // clear out mobj chains
      count = BlockMapWidth*BlockMapHeight;
      delete [] BlockLinks;
      BlockLinks = new VEntity *[count];
      memset(BlockLinks, 0, sizeof(VEntity *)*count);
    }

    //ajbsp::PutBlockmap();
    //ajbsp::PutReject();
  }

  // free any memory used by glBSP
  ajbsp::FreeLevel();
  ajbsp::FreeQuickAllocCuts();
  ajbsp::FreeQuickAllocSupers();

  ajbsp::cur_info  = nullptr;

  if (ret != build_result_e::BUILD_OK) Host_Error("Node build failed");

  /*
  //  Create dummy VIS data.
  VisData = nullptr;
  NoVis = new vuint8[(NumSubsectors + 7) / 8];
  memset(NoVis, 0xff, (NumSubsectors + 7) / 8);
  */
  //BuildPVS();
  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //
// simple PVS
#define MAX_PORTALS_ON_LEAF   (512)
#define ON_EPSILON  (0.1)


struct winding_t {
  bool original; // don't free, it's part of the portal
  TVec2D points[2];
};

// normal pointing into neighbor
struct portal_t : TPlane2D {
  int leaf; // neighbor
  winding_t winding;
  vuint8 *visbits;
  vuint8 *mightsee;
};

struct subsec_extra_t {
  portal_t *portals[MAX_PORTALS_ON_LEAF];
  int numportals;
};

struct PVSInfo {
  int numportals;
  portal_t *portals;
  int bitbytes;
  int bitlongs;
  int rowbytes;
  int *leaves; // NumSegs
  subsec_extra_t *ssex; // NumSubsectors
  // temp, don't free
  vuint8 *portalsee;

  PVSInfo ()
    : numportals(0)
    , portals(nullptr)
    , bitbytes(0)
    , bitlongs(0)
    , rowbytes(0)
    , leaves(nullptr)
    , ssex(nullptr)
    , portalsee(nullptr)
  {}

  PVSInfo (const PVSInfo &a)
    : numportals(a.numportals)
    , portals(a.portals)
    , bitbytes(a.bitbytes)
    , bitlongs(a.bitlongs)
    , rowbytes(a.rowbytes)
    , leaves(a.leaves)
    , ssex(a.ssex)
    , portalsee(nullptr)
  {}

  inline void operator = (const PVSInfo &a) {
    numportals = a.numportals;
    portals = a.portals;
    bitbytes = a.bitbytes;
    bitlongs = a.bitlongs;
    rowbytes = a.rowbytes;
    leaves = a.leaves;
    ssex = a.ssex;
    portalsee = nullptr;
  }
};


enum { PVSThreadMax = 16 }; // one is reserved for music

struct PVSThreadInfo {
  PVSInfo nfo;
  mythread trd;
  bool created;
};

static PVSThreadInfo pvsTreadList[PVSThreadMax];
static int pvsNextPortal, pvsMaxPortals, pvsLastReport; // next portal to process
static mythread_mutex pvsNPLock;
static double pvsLastReportTime;


static int getNextPortalNum () {
  mythread_mutex_lock(&pvsNPLock);
  int res = pvsNextPortal++;
  if (res-pvsLastReport >= 512) {
    pvsLastReport = res;
    const double tt = Sys_Time();
    if (tt-pvsLastReportTime >= 2.5) {
      pvsLastReportTime = tt;
      int cur = res;
      if (cur > pvsMaxPortals) cur = pvsMaxPortals;
      int prc = cur*100/pvsMaxPortals;
      GCon->Logf("PVS: %02d%% done (%d of %d)", prc, cur-1, pvsMaxPortals);
    }
  }
  mythread_mutex_unlock(&pvsNPLock);
  return res;
}


extern "C" {
static void SimpleFlood (portal_t *srcportal, int leafnum, PVSInfo *nfo) {
  if (srcportal->mightsee[leafnum>>3]&(1<<(leafnum&7))) return;
  srcportal->mightsee[leafnum>>3] |= (1<<(leafnum&7));

  subsec_extra_t *leaf = &nfo->ssex[leafnum];
  for (int i = 0; i < leaf->numportals; ++i) {
    portal_t *p = leaf->portals[i];
    if (!nfo->portalsee[p-nfo->portals]) continue;
    SimpleFlood(srcportal, p->leaf, nfo);
  }
}


static MYTHREAD_RET_TYPE pvsThreadWorker (void *aarg) {
  PVSInfo *nfo = (PVSInfo *)aarg;
  nfo->portalsee = new vuint8[nfo->numportals];
  for (;;) {
    int pnum = getNextPortalNum();
    if (pnum >= nfo->numportals) break;
    portal_t *p = &nfo->portals[pnum];

    p->mightsee = new vuint8[nfo->bitbytes];
    memset(p->mightsee, 0, nfo->bitbytes);

    memset(nfo->portalsee, 0, nfo->numportals);
    portal_t *tp = nfo->portals;
    for (int j = 0; j < nfo->numportals; ++j, ++tp) {
      if (j == pnum) continue;
      winding_t *w = &tp->winding;
      int k;
      for (k = 0; k < 2; ++k) {
        const double d = DotProduct(w->points[k], p->normal)-p->dist;
        if (d > ON_EPSILON) break;
      }
      if (k == 2) continue; // no points on front

      w = &p->winding;
      for (k = 0; k < 2; ++k) {
        const double d = DotProduct(w->points[k], tp->normal)-tp->dist;
        if (d < -ON_EPSILON) break;
      }
      if (k == 2) continue; // no points on front

      nfo->portalsee[j] = 1;
    }

    SimpleFlood(p, p->leaf, nfo);

    // fastvis just uses mightsee for a very loose bound
    p->visbits = p->mightsee;
  }
  delete[] nfo->portalsee;
  return MYTHREAD_RET_VALUE;
}
}


static void pvsStartThreads (const PVSInfo &anfo) {
  pvsNextPortal = 0;
  pvsLastReport = 0;
  pvsMaxPortals = anfo.numportals;
  int pvsThreadsToUse = loader_pvs_builder_threads;
  if (pvsThreadsToUse < 1) pvsThreadsToUse = 1; else if (pvsThreadsToUse > PVSThreadMax) pvsThreadsToUse = PVSThreadMax;
  mythread_mutex_init(&pvsNPLock);
  int ccount = 0;
  pvsLastReportTime = Sys_Time();
  for (int f = 0; f < pvsThreadsToUse; ++f) {
    pvsTreadList[f].nfo = anfo;
    pvsTreadList[f].created = (mythread_create(&pvsTreadList[f].trd, &pvsThreadWorker, &pvsTreadList[f].nfo) == 0);
    if (pvsTreadList[f].created) ++ccount;
  }
  if (ccount == 0) Sys_Error("Cannot create PVS worker threads");
  for (int f = 0; f < pvsThreadsToUse; ++f) {
    if (pvsTreadList[f].created) mythread_join(pvsTreadList[f].trd);
  }
  mythread_mutex_destroy(&pvsNPLock);
  if (pvsNextPortal < anfo.numportals) Sys_Error("PVS worker threads gone ape");
}


//==========================================================================
//
//  VLevel::BuildPVS
//
//==========================================================================
void VLevel::BuildPVS () {
  if (!loader_build_pvs) {
    VisData = nullptr;
    NoVis = new vuint8[(NumSubsectors+7)/8];
    memset(NoVis, 0xff, (NumSubsectors+7)/8);
    return;
  }

  GCon->Logf("building PVS...");
  PVSInfo nfo;
  memset((void *)&nfo, 0, sizeof(nfo));

  nfo.bitbytes = ((NumSubsectors+63)&~63)>>3;
  nfo.bitlongs = nfo.bitbytes/sizeof(long);
  nfo.rowbytes = (NumSubsectors+7)>>3;

  //nfo.secnums = new int[NumSubsectors+1];
  nfo.leaves = new int[NumSegs+1];
  nfo.ssex = new subsec_extra_t[NumSubsectors+1];

  {
    subsector_t *ss = Subsectors;
    for (int i = 0; i < NumSubsectors; ++i, ++ss) {
      nfo.ssex[i].numportals = 0;
      // set seg subsector links
      int count = ss->numlines;
      int ln = ss->firstline;
      while (count--) nfo.leaves[ln++] = i;
    }
  }

  bool ok = CreatePortals(&nfo);

  if (ok) {
    if (loader_pvs_builder_threads > 1) {
      pvsStartThreads(nfo);
    } else {
      BasePortalVis(&nfo);
    }
    // assemble the leaf vis lists by oring and compressing the portal lists
    //totalvis = 0;
    int vissize = nfo.rowbytes*NumSubsectors;
    //vis = new vuint8[vissize];
    VisData = new vuint8[vissize];
    memset(VisData, 0, vissize);
    for (int i = 0; i < NumSubsectors; ++i) {
      if (!LeafFlow(i, &nfo)) { ok = false; break; }
    }
    NoVis = nullptr;
    if (!ok) {
      delete VisData;
      VisData = nullptr;
    }
  }

  if (!ok) {
    GCon->Logf("PVS building failed.");
    VisData = nullptr;
    NoVis = new vuint8[(NumSubsectors+7)/8];
    memset(NoVis, 0xff, (NumSubsectors+7)/8);
  } else {
    GCon->Logf("PVS building (rough) complete.");
  }

  for (int i = 0; i < nfo.numportals; ++i) {
    delete [] nfo.portals[i].mightsee;
  }
  //delete [] nfo.secnums;
  delete [] nfo.leaves;
  delete [] nfo.ssex;
  delete [] nfo.portals;
}


//==========================================================================
//
//  VLevel::CreatePortals
//
//==========================================================================
bool VLevel::CreatePortals (void *pvsinfo) {
  PVSInfo *nfo = (PVSInfo *)pvsinfo;

  nfo->numportals = 0;
  for (int f = 0; f < NumSegs; ++f) {
    if (Segs[f].partner) ++nfo->numportals;
  }
  if (nfo->numportals == 0) { GCon->Logf("PVS: no possible portals found"); return false; }

  nfo->portals = new portal_t[nfo->numportals];
  for (int i = 0; i < nfo->numportals; ++i) {
    nfo->portals[i].visbits = nullptr;
    nfo->portals[i].mightsee = nullptr;
  }

  portal_t *p = nfo->portals;
  for (int i = 0; i < NumSegs; ++i) {
    seg_t *line = &Segs[i];
    //subsector_t *sub = &Subsectors[line->leaf];
    //subsector_t *sub = &Subsectors[nfo->leaves[i]];
    subsec_extra_t *sub = &nfo->ssex[nfo->leaves[i]];
    if (line->partner) {
      int pnum = (int)(ptrdiff_t)(line->partner-Segs);

      // skip self-referencing subsector segs
      if (/*line->leaf == line->partner->leaf*/nfo->leaves[i] == nfo->leaves[pnum]) {
        //GCon->Logf("Self-referencing subsector detected");
        --nfo->numportals;
        continue;
      }

      // create portal
      if (sub->numportals == MAX_PORTALS_ON_LEAF) {
        //throw GLVisError("Leaf with too many portals");
        GCon->Logf("PVS: Leaf with too many portals!");
        return false;
      }
      sub->portals[sub->numportals] = p;
      ++sub->numportals;

      p->winding.original = true;
      p->winding.points[0] = *line->v1;
      p->winding.points[1] = *line->v2;
      p->normal = line->partner->normal;
      p->dist = line->partner->dist;
      //p->leaf = line->partner->leaf;
      p->leaf = nfo->leaves[pnum];
      ++p;
    }
  }
  GCon->Logf("PVS: %d portals found", nfo->numportals);
  //if (p-portals != numportals) throw GLVisError("Portals miscounted");
  return (nfo->numportals > 0);
}


//==========================================================================
//
//  VLevel::SimpleFlood
//
//==========================================================================
void VLevel::SimpleFlood (/*portal_t*/void *srcportalp, int leafnum, void *pvsinfo) {
  PVSInfo *nfo = (PVSInfo *)pvsinfo;
  portal_t *srcportal = (portal_t *)srcportalp;

  if (srcportal->mightsee[leafnum>>3]&(1<<(leafnum&7))) return;
  srcportal->mightsee[leafnum>>3] |= (1<<(leafnum&7));
  //++nfo->c_leafsee;

  //leaf_t *leaf = &subsectors[leafnum];
  subsec_extra_t *leaf = &nfo->ssex[leafnum];
  for (int i = 0; i < leaf->numportals; ++i) {
    portal_t *p = leaf->portals[i];
    if (!nfo->portalsee[p-nfo->portals]) continue;
    SimpleFlood(srcportal, p->leaf, pvsinfo);
  }
}


//==========================================================================
//
//  VLevel::BasePortalVis
//
//  This is a rough first-order aproximation that is used to trivially
//  reject some of the final calculations.
//
//==========================================================================
void VLevel::BasePortalVis (void *pvsinfo) {
  int i, j, k;
  portal_t *tp, *p;
  double d;
  winding_t *w;

  PVSInfo *nfo = (PVSInfo *)pvsinfo;

  nfo->portalsee = new vuint8[nfo->numportals];
  for (i = 0, p = nfo->portals; i < nfo->numportals; ++i, ++p) {
    //Owner.DisplayBaseVisProgress(i, numportals);

    p->mightsee = new vuint8[nfo->bitbytes];
    memset(p->mightsee, 0, nfo->bitbytes);

    //nfo->c_portalsee = 0;
    memset(nfo->portalsee, 0, nfo->numportals);

    for (j = 0, tp = nfo->portals; j < nfo->numportals; ++j, ++tp) {
      if (j == i) continue;
      w = &tp->winding;
      for (k = 0; k < 2; ++k) {
        d = DotProduct(w->points[k], p->normal) - p->dist;
        if (d > ON_EPSILON) break;
      }
      if (k == 2) continue; // no points on front

      w = &p->winding;
      for (k = 0; k < 2; ++k) {
        d = DotProduct(w->points[k], tp->normal) - tp->dist;
        if (d < -ON_EPSILON) break;
      }
      if (k == 2) continue; // no points on front

      nfo->portalsee[j] = 1;
      //++nfo->c_portalsee;
    }

    //nfo->c_leafsee = 0;
    SimpleFlood(p, p->leaf, pvsinfo);
    //p->nummightsee = nfo->c_leafsee;

    // fastvis just uses mightsee for a very loose bound
    p->visbits = p->mightsee;
    //p->status = stat_done;
  }
  //Owner.DisplayBaseVisProgress(numportals, numportals);
  delete[] nfo->portalsee;
}


//==========================================================================
//
//  VLevel::LeafFlow
//
//  Builds the entire visibility list for a leaf
//
//==========================================================================
bool VLevel::LeafFlow (int leafnum, void *pvsinfo) {
  //leaf_t *leaf;
  vuint8 *outbuffer;
  //int numvis;

  PVSInfo *nfo = (PVSInfo *)pvsinfo;

  // flow through all portals, collecting visible bits
  outbuffer = VisData+leafnum*nfo->rowbytes;
  //leaf = &subsectors[leafnum];
  subsec_extra_t *leaf = &nfo->ssex[leafnum];
  for (int i = 0; i < leaf->numportals; ++i) {
    portal_t *p = leaf->portals[i];
    if (p == nullptr) continue;
    //if (p->status != stat_done) throw GLVisError("portal %d not done", (int)(p - portals));
    for (int j = 0; j < nfo->rowbytes; ++j) {
      if (p->visbits[j] == 0) continue;
      outbuffer[j] |= p->visbits[j];
    }
    //delete[] p->visbits;
    //p->visbits = nullptr;
  }

  if (outbuffer[leafnum>>3]&(1<<(leafnum&7))) {
    GCon->Logf("Leaf portals saw into leaf");
    return false;
  }

  outbuffer[leafnum>>3] |= (1<<(leafnum&7));

  /*
  numvis = 0;
  for (int i = 0; i < numsubsectors; i++) {
    if (outbuffer[i>>3]&(1<<(i&3))) ++numvis;
  }
  totalvis += numvis;
  */

  //if (Owner.verbose) Owner.DisplayMessage("leaf %4i : %4i visible\n", leafnum, numvis);
  return true;
}
