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

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"
#include "filesys/fwaddefs.h"
extern "C" {
#define vertex_t    glbsp_vertex_t
#define sector_t    glbsp_sector_t
#define seg_t     glbsp_seg_t
#define node_t      glbsp_node_t
#include "../utils/glbsp/level.h"
#include "../utils/glbsp/blockmap.h"
#include "../utils/glbsp/node.h"
#include "../utils/glbsp/seg.h"
#include "../utils/glbsp/analyze.h"
#undef vertex_t
#undef sector_t
#undef seg_t
#undef node_t
extern boolean_g lev_doing_normal;
extern boolean_g lev_doing_hexen;
};

#include "vector2d.h"


// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// Lump order in a map WAD: each map needs a couple of lumps
// to provide a complete scene geometry description.
enum
{
  ML_LABEL,   // A separator, name, ExMx or MAPxx
  ML_THINGS,    // Monsters, items..
  ML_LINEDEFS,  // LineDefs, from editing
  ML_SIDEDEFS,  // SideDefs, from editing
  ML_VERTEXES,  // Vertices, edited and BSP splits generated
  ML_SEGS,    // LineSegs, from LineDefs split by BSP
  ML_SSECTORS,  // SubSectors, list of LineSegs
  ML_NODES,   // BSP nodes
  ML_SECTORS,   // Sectors, from editing
  ML_REJECT,    // LUT, sector-sector visibility
  ML_BLOCKMAP,  // LUT, motion clipping, walls/grid element
  ML_BEHAVIOR   // ACS scripts
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static void stripNL (char *str) {
  if (!str) return;
  auto slen = strlen(str);
  while (slen > 0 && (str[slen-1] == '\n' || str[slen-1] == '\r')) str[--slen] = '\0';
}

//==========================================================================
//
//  GLBSP_PrintMsg
//
//==========================================================================

static void GLBSP_PrintMsg(const char *str, ...)
{
  static char   message_buf[1024];
  va_list   args;

  va_start(args, str);
  vsnprintf(message_buf, sizeof(message_buf), str, args);
  va_end(args);
  stripNL(message_buf);

  GCon->Logf("GB: %s", message_buf);
}

//==========================================================================
//
//  GLBSP_FatalError
//
//  Terminates the program reporting an error.
//
//==========================================================================

static void GLBSP_FatalError(const char *str, ...)
{
  static char   message_buf[1024];
  va_list   args;

  va_start(args, str);
  vsnprintf(message_buf, sizeof(message_buf), str, args);
  va_end(args);
  stripNL(message_buf);

  Sys_Error("Builing nodes failed: %s\n", message_buf);
}

static void GLBSP_Ticker()
{
}

static boolean_g GLBSP_DisplayOpen(displaytype_e)
{
  return true;
}

static void GLBSP_DisplaySetTitle(const char *)
{
}

static void GLBSP_DisplaySetBarText(int, const char*)
{
}

static void GLBSP_DisplaySetBarLimit(int, int)
{
}

static void GLBSP_DisplaySetBar(int, int)
{
}

static void GLBSP_DisplayClose()
{
}

static const nodebuildfuncs_t build_funcs =
{
  GLBSP_FatalError,
  GLBSP_PrintMsg,
  GLBSP_Ticker,

  GLBSP_DisplayOpen,
  GLBSP_DisplaySetTitle,
  GLBSP_DisplaySetBar,
  GLBSP_DisplaySetBarLimit,
  GLBSP_DisplaySetBarText,
  GLBSP_DisplayClose
};

//==========================================================================
//
//  SetUpVertices
//
//==========================================================================

static void SetUpVertices(VLevel *Level)
{
  guard(SetUpVertices);
  vertex_t *pSrc = Level->Vertexes;
  for (int i = 0; i < Level->NumVertexes; i++, pSrc++)
  {
    glbsp_vertex_t *Vert = NewVertex();
    Vert->x = pSrc->x;
    Vert->y = pSrc->y;
    Vert->index = i;
  }

  num_normal_vert = num_vertices;
  num_gl_vert = 0;
  num_complete_seg = 0;
  unguard;
}

//==========================================================================
//
//  SetUpSectors
//
//==========================================================================

static void SetUpSectors(VLevel *Level)
{
  guard(SetUpSectors);
  sector_t *pSrc = Level->Sectors;
  for (int i = 0; i < Level->NumSectors; i++, pSrc++)
  {
    glbsp_sector_t *Sector = NewSector();
    Sector->coalesce = (pSrc->tag >= 900 && pSrc->tag < 1000) ?
      TRUE : FALSE;
    Sector->index = i;
    Sector->warned_facing = -1;
  }
  unguard;
}

//==========================================================================
//
//  SetUpSidedefs
//
//==========================================================================

static void SetUpSidedefs(VLevel *Level)
{
  guard(SetUpSidedefs);
  side_t *pSrc = Level->Sides;
  for (int i = 0; i < Level->NumSides; i++, pSrc++)
  {
    sidedef_t *Side = NewSidedef();
    Side->sector = !pSrc->Sector ? nullptr :
      LookupSector(pSrc->Sector - Level->Sectors);
    if (Side->sector)
    {
      Side->sector->ref_count++;
    }
    Side->index = i;
  }
  unguard;
}

//==========================================================================
//
//  SetUpLinedefs
//
//==========================================================================

static void SetUpLinedefs(VLevel *Level)
{
  guard(SetUpLinedefs);
  line_t *pSrc = Level->Lines;
  for (int i = 0; i < Level->NumLines; i++, pSrc++)
  {
    linedef_t *Line = NewLinedef();
    if (Line == nullptr)
    {
      continue;
    }
    Line->start = LookupVertex(pSrc->v1 - Level->Vertexes);
    Line->end = LookupVertex(pSrc->v2 - Level->Vertexes);
    Line->start->ref_count++;
    Line->end->ref_count++;
    Line->zero_len = (fabs(Line->start->x - Line->end->x) < DIST_EPSILON) &&
      (fabs(Line->start->y - Line->end->y) < DIST_EPSILON);
    Line->flags = pSrc->flags;
    Line->type = pSrc->special;
    Line->two_sided = (Line->flags & LINEFLAG_TWO_SIDED) ? TRUE : FALSE;
    Line->right = pSrc->sidenum[0] < 0 ? nullptr : LookupSidedef(pSrc->sidenum[0]);
    Line->left = pSrc->sidenum[1] < 0 ? nullptr : LookupSidedef(pSrc->sidenum[1]);
    if (Line->right != nullptr)
    {
      Line->right->ref_count++;
      Line->right->on_special |= (Line->type > 0) ? 1 : 0;
    }
    if (Line->left != nullptr)
    {
      Line->left->ref_count++;
      Line->left->on_special |= (Line->type > 0) ? 1 : 0;
    }
    Line->self_ref = (Line->left != nullptr && Line->right != nullptr &&
      (Line->left->sector == Line->right->sector));
    Line->index = i;
  }
  unguard;
}

//==========================================================================
//
//  SetUpThings
//
//==========================================================================

static void SetUpThings(VLevel *Level)
{
  guard(SetUpThings);
  mthing_t *pSrc = Level->Things;
  for (int i = 0; i < Level->NumThings; i++, pSrc++)
  {
    thing_t *Thing = NewThing();
    Thing->x = (int)pSrc->x;
    Thing->y = (int)pSrc->y;
    Thing->type = pSrc->type;
    Thing->options = pSrc->options;
    Thing->index = i;
  }
  unguard;
}

//==========================================================================
//
//  CopyGLVerts
//
//==========================================================================

static void CopyGLVerts(VLevel *Level, vertex_t *&GLVertexes)
{
  guard(CopyGLVerts);
  int NumBaseVerts = Level->NumVertexes;
  vertex_t *OldVertexes = Level->Vertexes;
  Level->NumVertexes = NumBaseVerts + num_gl_vert;
  Level->Vertexes = new vertex_t[Level->NumVertexes];
  GLVertexes = Level->Vertexes + NumBaseVerts;
  memcpy(Level->Vertexes, OldVertexes, NumBaseVerts * sizeof(vertex_t));
  vertex_t *pDst = GLVertexes;
  for (int i = 0; i < num_vertices; i++)
  {
    glbsp_vertex_t *vert = LookupVertex(i);
    if (!(vert->index & IS_GL_VERTEX))
      continue;
    *pDst = TVec(vert->x, vert->y, 0);
    pDst++;
  }

  //  Update pointers to vertexes in lines.
  for (int i = 0; i < Level->NumLines; i++)
  {
    line_t *ld = &Level->Lines[i];
    ld->v1 = &Level->Vertexes[ld->v1 - OldVertexes];
    ld->v2 = &Level->Vertexes[ld->v2 - OldVertexes];
  }
  delete[] OldVertexes;
  OldVertexes = nullptr;
  unguard;
}

//==========================================================================
//
//  CopySegs
//
//==========================================================================

static void CopySegs(VLevel *Level, vertex_t *GLVertexes)
{
  guard(CopySegs);
  //  Build ordered list of source segs.
  glbsp_seg_t **SrcSegs = new glbsp_seg_t*[num_complete_seg];
  for (int i = 0; i < num_segs; i++)
  {
    glbsp_seg_t *Seg = LookupSeg(i);
    // ignore degenerate segs
    if (Seg->degenerate)
      continue;
    SrcSegs[Seg->index] = Seg;
  }

  Level->NumSegs = num_complete_seg;
  Level->Segs = new seg_t[Level->NumSegs];
  memset((void *)Level->Segs, 0, sizeof(seg_t) * Level->NumSegs);
  seg_t *li = Level->Segs;
  for (int i = 0; i < Level->NumSegs; i++, li++)
  {
    glbsp_seg_t *SrcSeg = SrcSegs[i];
    li->partner = nullptr;
    li->front_sub = nullptr;

    // assign partner (we need it for self-referencing deep water)
    if (SrcSeg->partner) {
      for (int psi = 0; psi < Level->NumSegs; ++psi) {
        if (SrcSegs[psi] == SrcSeg->partner) {
          li->partner = &Level->Segs[psi];
          break;
        }
      }
      if (!li->partner) GCon->Logf("GLBSP: invalid partner (ignored)");
    }

    if (SrcSeg->start->index & IS_GL_VERTEX)
    {
      li->v1 = &GLVertexes[SrcSeg->start->index & ~IS_GL_VERTEX];
    }
    else
    {
      li->v1 = &Level->Vertexes[SrcSeg->start->index];
    }
    if (SrcSeg->end->index & IS_GL_VERTEX)
    {
      li->v2 = &GLVertexes[SrcSeg->end->index & ~IS_GL_VERTEX];
    }
    else
    {
      li->v2 = &Level->Vertexes[SrcSeg->end->index];
    }

    if (SrcSeg->linedef)
    {
      line_t *ldef = &Level->Lines[SrcSeg->linedef->index];
      li->linedef = ldef;
      li->sidedef = &Level->Sides[ldef->sidenum[SrcSeg->side]];
      li->frontsector = Level->Sides[ldef->sidenum[SrcSeg->side]].Sector;

      if (ldef->flags & ML_TWOSIDED)
      {
        li->backsector = Level->Sides[ldef->sidenum[SrcSeg->side ^ 1]].Sector;
      }

      if (SrcSeg->side)
      {
        li->offset = Length(*li->v1 - *ldef->v2);
      }
      else
      {
        li->offset = Length(*li->v1 - *ldef->v1);
      }
      li->length = Length(*li->v2 - *li->v1);
      li->side = SrcSeg->side;
    }

    //  Calc seg's plane params
    CalcSeg(li);
  }

  delete[] SrcSegs;
  SrcSegs = nullptr;
  unguard;
}

//==========================================================================
//
//  CopySubsectors
//
//==========================================================================

static void CopySubsectors(VLevel *Level)
{
  guard(CopySubsectors);
  Level->NumSubsectors = num_subsecs;
  Level->Subsectors = new subsector_t[Level->NumSubsectors];
  memset((void *)Level->Subsectors, 0, sizeof(subsector_t) * Level->NumSubsectors);
  subsector_t *ss = Level->Subsectors;
  for (int i = 0; i < Level->NumSubsectors; i++, ss++)
  {
    subsec_t *SrcSub = LookupSubsec(i);
    ss->numlines = SrcSub->seg_count;
    ss->firstline = SrcSub->seg_list->index;

    //  Look up sector number for each subsector
    seg_t *seg = &Level->Segs[ss->firstline];
    for (int j = 0; j < ss->numlines; j++)
    {
      if (seg[j].linedef)
      {
        ss->sector = seg[j].sidedef->Sector;
        ss->seclink = ss->sector->subsectors;
        ss->sector->subsectors = ss;
        break;
      }
    }
    for (int j = 0; j < ss->numlines; j++) seg[j].front_sub = ss;
    if (!ss->sector)
    {
      Host_Error("Subsector %d without sector", i);
    }
  }
  int setcount = Level->NumSegs;
  for (int f = 0; f < Level->NumSegs; ++f) {
    if (!Level->Segs[f].front_sub) { GCon->Logf("Seg %d: front_sub is not set!", f); --setcount; }
  }
  if (setcount != Level->NumSegs) GCon->Logf("WARNING: %d of %d segs has no front_sub!", Level->NumSegs-setcount, Level->NumSegs);
  unguard;
}

//==========================================================================
//
//  CopyNode
//
//==========================================================================

static void CopyNode(int &NodeIndex, glbsp_node_t *SrcNode, node_t *Nodes)
{
  if (SrcNode->r.node)
  {
    CopyNode(NodeIndex, SrcNode->r.node, Nodes);
  }

  if (SrcNode->l.node)
  {
    CopyNode(NodeIndex, SrcNode->l.node, Nodes);
  }

  SrcNode->index = NodeIndex;

  node_t *Node = &Nodes[NodeIndex];
  Node->SetPointDir(TVec(SrcNode->x, SrcNode->y, 0),
    TVec(SrcNode->dx, SrcNode->dy, 0));

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

  if (SrcNode->r.node)
  {
    Node->children[0] = SrcNode->r.node->index;
  }
  else if (SrcNode->r.subsec)
  {
    Node->children[0] = SrcNode->r.subsec->index | NF_SUBSECTOR;
  }

  if (SrcNode->l.node)
  {
    Node->children[1] = SrcNode->l.node->index;
  }
  else if (SrcNode->l.subsec)
  {
    Node->children[1] = SrcNode->l.subsec->index | NF_SUBSECTOR;
  }

  NodeIndex++;
}

//==========================================================================
//
//  CopyNodes
//
//==========================================================================

static void CopyNodes(VLevel *Level, glbsp_node_t *root_node)
{
  guard(CopyNodes);
  //  Copy nodes.
  Level->NumNodes = num_nodes;
  Level->Nodes = new node_t[Level->NumNodes];
  memset((void *)Level->Nodes, 0, sizeof(node_t) * Level->NumNodes);
  if (root_node)
  {
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

void VLevel::BuildNodes()
{
  guard(VLevel::BuildNodes);
  //  Set up glBSP build globals.
  nodebuildinfo_t nb_info = default_buildinfo;
  nodebuildcomms_t nb_comms = default_buildcomms;
  nb_info.quiet = false;
  nb_info.gwa_mode = true;

  cur_info  = &nb_info;
  cur_funcs = &build_funcs;
  cur_comms = &nb_comms;

  lev_doing_normal = false;
  lev_doing_hexen = !!(LevelFlags & LF_Extended);

  //  Set up map data from loaded data.
  SetUpVertices(this);
  SetUpSectors(this);
  SetUpSidedefs(this);
  SetUpLinedefs(this);
  SetUpThings(this);

  //  Other data initialisation.
  CalculateWallTips();
  if (lev_doing_hexen)
  {
    DetectPolyobjSectors();
  }
  DetectOverlappingLines();
  if (cur_info->window_fx)
  {
    DetectWindowEffects();
  }
  InitBlockmap();

  //  Build nodes.
  superblock_t *SegList = CreateSegs();
  glbsp_node_t *root_node;
  subsec_t *root_sub;
  glbsp_ret_e ret = ::BuildNodes(SegList, &root_node, &root_sub, 0, nullptr);
  FreeSuper(SegList);

  if (ret == GLBSP_E_OK)
  {
    ClockwiseBspTree(root_node);

    //  Copy nodes into internal structures.
    vertex_t *GLVertexes;

    CopyGLVerts(this, GLVertexes);
    CopySegs(this, GLVertexes);
    CopySubsectors(this);
    CopyNodes(this, root_node);
  }

  //  Free any memory used by glBSP.
  FreeLevel();
  FreeQuickAllocCuts();
  FreeQuickAllocSupers();

  cur_info  = nullptr;
  cur_comms = nullptr;
  cur_funcs = nullptr;

  if (ret != GLBSP_E_OK)
  {
    Host_Error("Node build failed");
  }

  /*
  //  Create dummy VIS data.
  VisData = nullptr;
  NoVis = new vuint8[(NumSubsectors + 7) / 8];
  memset(NoVis, 0xff, (NumSubsectors + 7) / 8);
  */
  BuildPVS();
  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //
#define MAX_PORTALS_ON_LEAF   (128)
#define ON_EPSILON  (0.1)


struct winding_t {
  bool original; // don't free, it's part of the portal
  TVec2D points[2];
};

enum vstatus_t { stat_none, stat_working, stat_done };

// normal pointing into neighbor
struct portal_t : TPlane2D {
  int leaf; // neighbor
  winding_t winding;
  //vstatus_t status;
  vuint8 *visbits;
  vuint8 *mightsee;
  int nummightsee;
  int numcansee;
};

struct subsec_extra_t {
  portal_t *portals[MAX_PORTALS_ON_LEAF];
  int numportals;
};

struct PVSInfo {
  int numportals;
  portal_t *portals;
  int c_portalsee;
  int c_leafsee;
  int bitbytes;
  int bitlongs;
  int rowbytes;
  //int *secnums; // NumSubsectors
  int *leaves; // NumSegs
  subsec_extra_t *ssex; // NumSubsectors
  // temp, don't free
  vuint8 *portalsee;
};


//==========================================================================
//
//  VLevel::BuildPVS
//
//==========================================================================
void VLevel::BuildPVS () {
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
      //seg_t *line = &Segs[ln];
      //secnums[i] = -1;
      while (count--) {
        nfo.leaves[ln++] = i;
        /*
        if (secnums[i] == -1 && line->secnum >= 0) {
          ss->secnum = line->secnum;
        } else if (ss->secnum != -1 && line->secnum >= 0 && ss->secnum != line->secnum) {
          Owner.DisplayMessage("Segs from different sectors\n");
        }
        ++line;
        */
      }
      //if (ss->secnum == -1) throw GLVisError("Subsector without sector");
    }
  }

  bool ok = CreatePortals(&nfo);
  if (ok) {
    BasePortalVis(&nfo);
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
  ++nfo->c_leafsee;

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

    nfo->c_portalsee = 0;
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
      ++nfo->c_portalsee;
    }

    nfo->c_leafsee = 0;
    SimpleFlood(p, p->leaf, pvsinfo);
    p->nummightsee = nfo->c_leafsee;

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
