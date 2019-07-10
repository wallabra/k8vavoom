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
//**    Build nodes using ajbsp.
//**
//**************************************************************************
static VCvarB ajbsp_roundoff_tree("__ajbsp_roundoff_tree", false, "Roundoff vertices in AJBSP?", CVAR_PreInit);


namespace ajbsp {
  //extern bool lev_doing_hexen;
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
}


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
  va_list ap;
  va_start(ap, fmt);
  char *res = vavarg(fmt, ap);
  va_end(ap);
  stripNL(res);
  Sys_Error("AJBSP: %s", res);
}


//==========================================================================
//
//  ajbsp_PrintMsg
//
//==========================================================================
__attribute__((format(printf,1,2))) void ajbsp_PrintMsg (const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char *res = vavarg(fmt, ap);
  va_end(ap);
  stripNL(res);
  GCon->Logf("AJBSP: %s", res);
}


//==========================================================================
//
//  ajbsp_PrintVerbose
//
//==========================================================================
__attribute__((format(printf,1,2))) void ajbsp_PrintVerbose (const char *fmt, ...) {
  if (!nodes_show_warnings) return;
  va_list ap;
  va_start(ap, fmt);
  char *res = vavarg(fmt, ap);
  va_end(ap);
  stripNL(res);
  GCon->Logf("AJBSP: %s", res);
}


//==========================================================================
//
//  ajbsp_PrintDetail
//
//==========================================================================
__attribute__((format(printf,1,2))) void ajbsp_PrintDetail (const char *fmt, ...) {
  if (!nodes_show_warnings) return;
  va_list ap;
  va_start(ap, fmt);
  char *res = vavarg(fmt, ap);
  va_end(ap);
  stripNL(res);
  GCon->Logf("AJBSP: %s", res);
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
//  ajbsp_Progress
//
//==========================================================================
void ajbsp_Progress (int curr, int total) {
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
//  ajRoundOffVertex
//
//  round vertex coordinates
//
//==========================================================================
static inline float ajRoundoffVertex (double v) {
  vint32 iv = (vint32)(v*65536.0);
  float res = (((double)iv)/65536.0);
  return res;
}


//==========================================================================
//
//  UploadSectors
//
//==========================================================================
static void UploadSectors (VLevel *Level) {
  const sector_t *pSrc = Level->Sectors;
  for (int i = 0; i < Level->NumSectors; ++i, ++pSrc) {
    ajbsp::sector_t *sector = ajbsp::NewSector();
    memset(sector, 0, sizeof(*sector));
    sector->coalesce = (pSrc->sectorTag >= 900 && pSrc->sectorTag < 1000 ? 1 : 0);
    if (!sector->coalesce) {
      for (int f = 0; f < pSrc->moreTags.length(); ++f) {
        if (pSrc->moreTags[f] >= 900 && pSrc->moreTags[f] < 1000) {
          sector->coalesce = 1;
          break;
        }
      }
    }
    // sector indices never change
    sector->index = i;
    sector->warned_facing = -1;
  }
}


//==========================================================================
//
//  UploadSidedefs
//
//==========================================================================
static void UploadSidedefs (VLevel *Level) {
  const side_t *pSrc = Level->Sides;
  for (int i = 0; i < Level->NumSides; ++i, ++pSrc) {
    ajbsp::sidedef_t *side = ajbsp::NewSidedef();
    memset(side, 0, sizeof(*side));
    side->sector = (!pSrc->Sector ? nullptr : ajbsp::LookupSector((int)(ptrdiff_t)(pSrc->Sector-Level->Sectors)));
    if (side->sector) side->sector->is_used = 1;
    // sidedef indices never change
    side->index = i;
  }
}


// ////////////////////////////////////////////////////////////////////////// //
struct __attribute__((packed)) VertexInfo {
  float xy[2];
  int index; // vertex index in AJBSP

  VertexInfo () {}
  VertexInfo (const TVec &v, int aindex=0) { xy[0] = ajRoundoffVertex(v.x); xy[1] = ajRoundoffVertex(v.y); index = aindex; }

  inline bool operator == (const VertexInfo &vi) const { return (memcmp(xy, &vi.xy, sizeof(xy)) == 0); }
  inline bool operator != (const VertexInfo &vi) const { return (memcmp(xy, &vi.xy, sizeof(xy)) != 0); }
};
static_assert(sizeof(VertexInfo) == sizeof(float)*2+sizeof(int), "oops");

static inline vuint32 GetTypeHash (const VertexInfo &vi) { return joaatHashBuf(vi.xy, sizeof(vi.xy)); }


//==========================================================================
//
//  ajUploadVertex
//
//==========================================================================
static int ajUploadVertex (TMap<VertexInfo, int> &vmap, VLevel *Level, const TVec *v) {
  VertexInfo vi(*v, ajbsp::num_vertices);
  auto pp = vmap.find(vi);
  if (pp) return *pp;
  check(vi.index == ajbsp::num_vertices);
  vmap.put(vi, vi.index);
  ajbsp::vertex_t *vert = ajbsp::NewVertex();
  memset(vert, 0, sizeof(*vert));
  vert->x = vi.xy[0];
  vert->y = vi.xy[1];
  vert->index = (int)(ptrdiff_t)(v-Level->Vertexes);
  vert->is_new = 0;
  vert->is_used = 1;
  return vi.index;
}


//==========================================================================
//
//  UploadLinedefs
//
//  ..and vertices
//
//==========================================================================
static void UploadLinedefs (VLevel *Level) {
  TMap<VertexInfo, int> vmap;
  const line_t *pSrc = Level->Lines;
  for (int i = 0; i < Level->NumLines; ++i, ++pSrc) {
    ajbsp::linedef_t *line = ajbsp::NewLinedef();
    memset(line, 0, sizeof(*line));
    //if (line == nullptr) Sys_Error("AJBSP: out of memory!");
    if (!pSrc->v1 || !pSrc->v2) Sys_Error("linedef without vertexes");
    line->start = ajbsp::LookupVertex(ajUploadVertex(vmap, Level, pSrc->v1));
    line->end = ajbsp::LookupVertex(ajUploadVertex(vmap, Level, pSrc->v2));
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
  ajbsp::num_old_vert = ajbsp::num_vertices;
}


//==========================================================================
//
//  UploadThings
//
//==========================================================================
static void UploadThings (VLevel *Level) {
  const mthing_t *pSrc = Level->Things;
  for (int i = 0; i < Level->NumThings; ++i, ++pSrc) {
    ajbsp::thing_t *Thing = ajbsp::NewThing();
    memset(Thing, 0, sizeof(*Thing));
    Thing->x = (int)pSrc->x;
    Thing->y = (int)pSrc->y;
    Thing->type = pSrc->type;
    Thing->options = pSrc->options;
    Thing->index = i;
  }
}


// ////////////////////////////////////////////////////////////////////////// //
struct CopyInfo {
  TMap<vuint64, int> ajvidx; // for `AJVertexIndex()`, created in `CopyGLVerts()`
};


//==========================================================================
//
//  AJVertexIndex
//
//==========================================================================
static inline int AJVertexIndex (CopyInfo &nfo, const ajbsp::vertex_t *v) {
  check(v != nullptr);
  auto ip = nfo.ajvidx.find((vuint64)(uintptr_t)v);
  if (!ip) Sys_Error("AJBSP: found unknown vertex");
  return *ip;
}


//==========================================================================
//
//  CopyGLVerts
//
//==========================================================================
static void CopyGLVerts (VLevel *Level, CopyInfo &nfo) {
  const int oldvnum = Level->NumVertexes;
  delete[] Level->Vertexes;
  nfo.ajvidx.reset();

  Level->Vertexes = new vertex_t[ajbsp::num_vertices];
  Level->NumVertexes = ajbsp::num_vertices;
  memset((void *)Level->Vertexes, 0, sizeof(vertex_t)*ajbsp::num_vertices);

  for (int i = 0; i < ajbsp::num_vertices; ++i) {
    ajbsp::vertex_t *vert = ajbsp::LookupVertex(i);
    nfo.ajvidx.put((vuint64)(uintptr_t)vert, i); // for `AJVertexIndex()`
    vertex_t *pDst = &Level->Vertexes[i];
    *pDst = TVec(ajRoundoffVertex(vert->x), ajRoundoffVertex(vert->y), 0);
  }

  // fix lines
  for (int i = 0; i < Level->NumLines; ++i) {
    auto ajline = ajbsp::LookupLinedef(i);
    line_t *ld = &Level->Lines[i];

    int v1idx = AJVertexIndex(nfo, ajline->start);
    if (v1idx < 0 || v1idx >= ajbsp::num_vertices) Sys_Error("AJBSP: invalid line #%d v1 index", i);
    ld->v1 = &Level->Vertexes[v1idx];

    int v2idx = AJVertexIndex(nfo, ajline->end);
    if (v2idx < 0 || v2idx >= ajbsp::num_vertices) Sys_Error("AJBSP: invalid line #%d v1 index", i);
    ld->v2 = &Level->Vertexes[v2idx];
  }

  GCon->Logf("AJBSP: copied %d vertices (old number is %d)", Level->NumVertexes, oldvnum);
}


//==========================================================================
//
//  CopySegs
//
//==========================================================================
static void CopySegs (VLevel *Level, CopyInfo &nfo) {
  Level->NumSegs = 0;
  delete[] Level->Segs;
  Level->NumSegs = ajbsp::num_segs;
  Level->Segs = new seg_t[ajbsp::num_segs]; //k8: this may overallocate, but i don't care
  memset((void *)Level->Segs, 0, sizeof(seg_t)*ajbsp::num_segs);

  for (int i = 0; i < ajbsp::num_segs; ++i) {
    ajbsp::seg_t *srcseg = ajbsp::LookupSeg(i);
    if (srcseg->is_degenerate) Sys_Error("AJBSP: seg #%d is degenerate (the thing that should not be!)", i);
    if (srcseg->index != i) Host_Error("AJBSP: seg #%d has invalid index %d", i, srcseg->index);

    seg_t *destseg = &Level->Segs[i];

    destseg->partner = (srcseg->partner ? &Level->Segs[srcseg->partner->index] : nullptr);
    destseg->front_sub = nullptr;

    auto v1mp = AJVertexIndex(nfo, srcseg->start);
    auto v2mp = AJVertexIndex(nfo, srcseg->end);
    if (v1mp < 0 || v2mp < 0) Sys_Error("AJBSP: vertex not found for seg #%d", i);
    if (v1mp == v2mp) Sys_Error("AJBSP: seg #%d has same start and end vertex (%d:%d) (%d:%d)", i, v1mp, srcseg->start->index, v2mp, srcseg->end->index);
    if (v1mp >= Level->NumVertexes || v2mp >= Level->NumVertexes) Sys_Error("AJBSP: invalid vertices not found for seg #%d", i);
    destseg->v1 = &Level->Vertexes[v1mp];
    destseg->v2 = &Level->Vertexes[v2mp];

    if (srcseg->side != 0 && srcseg->side != 1) Sys_Error("AJBSP: invalid seg #%d side (%d)", i, srcseg->side);
    destseg->side = srcseg->side;

    if (srcseg->linedef) {
      if (srcseg->linedef->index < 0 || srcseg->linedef->index >= Level->NumLines) Sys_Error("AJBSP: invalid seg #%d linedef (%d), max is %d", i, srcseg->linedef->index, Level->NumLines-1);
      destseg->linedef = &Level->Lines[srcseg->linedef->index];
    }
  }
  GCon->Logf("AJBSP: copied %d segs", Level->NumSegs);
}


//==========================================================================
//
//  CopySubsectors
//
//==========================================================================
static void CopySubsectors (VLevel *Level, CopyInfo &nfo) {
  Level->NumSubsectors = ajbsp::num_subsecs;
  delete[] Level->Subsectors;
  Level->Subsectors = new subsector_t[Level->NumSubsectors];
  memset((void *)Level->Subsectors, 0, sizeof(subsector_t)*Level->NumSubsectors);
  for (int i = 0; i < Level->NumSubsectors; ++i) {
    ajbsp::subsec_t *srcss = ajbsp::LookupSubsec(i);
    const int ajfsegidx = srcss->seg_list->index; // exactly the same as k8vavoom index

    subsector_t *destss = &Level->Subsectors[i];
    destss->numlines = srcss->seg_count;

    if (destss->numlines < 0) Sys_Error("AJBSP: invalid subsector #%d data (numlines)", i);
    if (destss->numlines == 0) {
      destss->firstline = 0; // doesn't matter
    } else {
      destss->firstline = ajfsegidx;
      // check segments
      for (int j = 0; j < destss->numlines; ++j) {
        const ajbsp::seg_t *xseg = ajbsp::LookupSeg(ajfsegidx+j);
        if (xseg->index != ajfsegidx+j) Host_Error("AJBSP: subsector #%d contains non-sequential segs (expected %d, got %d)", i, ajfsegidx+j, xseg->index);
      }
    }
  }
  GCon->Logf("AJBSP: copied %d subsectors", Level->NumSubsectors);
}


//==========================================================================
//
//  CopyNode
//
//==========================================================================
static void CopyNode (int &NodeIndex, ajbsp::node_t *SrcNode, node_t *Nodes) {
  if (SrcNode->r.node) CopyNode(NodeIndex, SrcNode->r.node, Nodes);
  if (SrcNode->l.node) CopyNode(NodeIndex, SrcNode->l.node, Nodes);

  if (NodeIndex >= ajbsp::num_nodes) Host_Error("AJBSP: invalid total number of nodes (0)");

  SrcNode->index = NodeIndex;
  node_t *Node = &Nodes[NodeIndex];
  ++NodeIndex;

  TVec org = TVec(SrcNode->xs, SrcNode->ys, 0);
  TVec dir = TVec(SrcNode->xe-SrcNode->xs, SrcNode->ye-SrcNode->ys, 0);
  //k8: this seems to be unnecessary
  //if (SrcNode->too_long) { dir.x /= 2.0f; dir.y /= 2.0f; }
  // check if `Length()` and `SetPointDirXY()` are happy
  if (dir.x == 0 && dir.y == 0) {
    //Host_Error("AJBSP: invalid BSP node (zero direction)");
    GCon->Logf("AJBSP: invalid BSP node (zero direction%s)", (SrcNode->too_long ? "; overlong node" : ""));
    dir.x = 0.001f;
  }
  Node->SetPointDirXY(org, dir);

  //TODO: check if i should convert AJBSP bounding boxes to floats
  Node->bbox[0][0] = SrcNode->r.bounds.minx;
  Node->bbox[0][1] = SrcNode->r.bounds.miny;
  Node->bbox[0][2] = -32768.0f;
  Node->bbox[0][3] = SrcNode->r.bounds.maxx;
  Node->bbox[0][4] = SrcNode->r.bounds.maxy;
  Node->bbox[0][5] = 32768.0f;

  Node->bbox[1][0] = SrcNode->l.bounds.minx;
  Node->bbox[1][1] = SrcNode->l.bounds.miny;
  Node->bbox[1][2] = -32768.0f;
  Node->bbox[1][3] = SrcNode->l.bounds.maxx;
  Node->bbox[1][4] = SrcNode->l.bounds.maxy;
  Node->bbox[1][5] = 32768.0f;

  { // fuck you, shitcc!
         if (SrcNode->r.node) Node->children[0] = SrcNode->r.node->index;
    else if (SrcNode->r.subsec) Node->children[0] = SrcNode->r.subsec->index|NF_SUBSECTOR;
    else Host_Error("AJBSP: bad left children in BSP");
  }

  { // fuck you, shitcc!
         if (SrcNode->l.node) Node->children[1] = SrcNode->l.node->index;
    else if (SrcNode->l.subsec) Node->children[1] = SrcNode->l.subsec->index|NF_SUBSECTOR;
    else Host_Error("AJBSP: bad right children in BSP");
  }
}


//==========================================================================
//
//  CopyNodes
//
//==========================================================================
static void CopyNodes (VLevel *Level, ajbsp::node_t *root_node) {
  Level->NumNodes = ajbsp::num_nodes;
  delete[] Level->Nodes;
  Level->Nodes = new node_t[Level->NumNodes];
  memset((void *)Level->Nodes, 0, sizeof(node_t)*Level->NumNodes);
  if (root_node) {
    int NodeIndex = 0;
    CopyNode(NodeIndex, root_node, Level->Nodes);
    if (NodeIndex != ajbsp::num_nodes) Host_Error("AJBSP: invalid total number of nodes (1)");
  }
  GCon->Logf("AJBSP: copied %d nodes", Level->NumNodes);
}


//==========================================================================
//
//  VLevel::BuildNodesAJ
//
//==========================================================================
void VLevel::BuildNodesAJ () {
  // set up glBSP build globals
  nodebuildinfo_t nb_info;
  nb_info.fast = nodes_fast_mode;
  nb_info.warnings = true; // not currently used, but meh
  nb_info.do_blockmap = true;
  nb_info.do_reject = true;

  ajbsp::cur_info = &nb_info;

  //lev_doing_normal = false;
  //!bool lev_doing_hexen = !!(LevelFlags&(LF_Extended|LF_TextMap));

  ajbsp::num_vertices = 0;
  ajbsp::num_linedefs = 0;
  ajbsp::num_sidedefs = 0;
  ajbsp::num_sectors = 0;
  ajbsp::num_things = 0;
  ajbsp::num_segs = 0;
  ajbsp::num_subsecs = 0;
  ajbsp::num_nodes = 0;

  ajbsp::num_old_vert = 0;
  ajbsp::num_new_vert = 0;
  ajbsp::num_complete_seg = 0;
  ajbsp::num_real_lines = 0;

  // set up map data from loaded data
  // vertices will be uploaded with linedefs
  UploadSectors(this);
  UploadSidedefs(this);
  UploadLinedefs(this);
  UploadThings(this);

  // other data initialisation
  // no need to prune vertices, 'cause our vertex uploader will upload only used vertices
  //ajbsp::PruneVerticesAtEnd();
  GCon->Logf("AJBSP: copied %d original vertexes out of %d", ajbsp::num_vertices, NumVertexes);
  GCon->Logf("AJBSP: building nodes (%s mode)", (nodes_fast_mode ? "fast" : "normal"));

  GCon->Logf("AJBSP: detecting overlapped vertices...");
  ajbsp::DetectOverlappingVertices();
  GCon->Logf("AJBSP: detecting overlapped linedefs...");
  ajbsp::DetectOverlappingLines();
  GCon->Logf("AJBSP: caclulating wall tips...");
  ajbsp::CalculateWallTips();

  //k8: always try polyobjects, why not?
  GCon->Logf("AJBSP: detecting polyobjects...");
  /*if (lev_doing_hexen)*/ ajbsp::DetectPolyobjSectors(); // -JL- Find sectors containing polyobjs

  //if (cur_info->window_fx) ajbsp::DetectWindowEffects();
  GCon->Logf("AJBSP: building blockmap...");
  ajbsp::InitBlockmap();

  GCon->Logf("AJBSP: building nodes...");
  // create initial segs
  ajbsp::superblock_t *seg_list = ajbsp::CreateSegs();
  ajbsp::node_t *root_node;
  ajbsp::subsec_t *root_sub;
  ajbsp::bbox_t seg_bbox;
  ajbsp::FindLimits(seg_list, &seg_bbox);
  build_result_e ret = ajbsp::BuildNodes(seg_list, &root_node, &root_sub, 0, &seg_bbox);
  ajbsp::FreeSuper(seg_list);

  if (ret == build_result_e::BUILD_OK) {
    GCon->Log("AJBSP: finalising the tree...");
    ajbsp::ClockwiseBspTree();
    ajbsp::CheckLimits();
    //k8: this seems to be unnecessary for GL nodes
    //ajbsp::NormaliseBspTree(); // remove all the mini-segs
    if (ajbsp_roundoff_tree) {
      GCon->Log("AJBSP: rounding off bsp tree");
      ajbsp::RoundOffBspTree();
    }
    ajbsp::SortSegs();
    ajbsp_Progress(-1, -1);

    GCon->Logf("AJBSP: built with %d nodes, %d subsectors, %d segs, %d vertexes", ajbsp::num_nodes, ajbsp::num_subsecs, ajbsp::num_segs, ajbsp::num_vertices);
    if (root_node && root_node->r.node && root_node->l.node) GCon->Logf("AJBSP: heights of subtrees: %d/%d", ajbsp::ComputeBspHeight(root_node->r.node), ajbsp::ComputeBspHeight(root_node->l.node));

    GCon->Logf("AJBSP: copying built data");
    // copy nodes into internal structures
    CopyInfo nfo;
    CopyGLVerts(this, nfo);
    CopySegs(this, nfo);
    CopySubsectors(this, nfo);
    CopyNodes(this, root_node);

    // reject
    if (ajbsp::cur_info->do_reject) {
      VMemoryStream *xms = new VMemoryStream();
      xms->BeginWrite();
      ajbsp::PutReject(*xms);

      delete[] RejectMatrix;
      RejectMatrix = nullptr;

      RejectMatrixSize = xms->TotalSize();
      if (RejectMatrixSize) {
        TArray<vuint8> &arr = xms->GetArray();
        RejectMatrix = new vuint8[RejectMatrixSize];
        memcpy(RejectMatrix, arr.ptr(), RejectMatrixSize);
        // check if it's an all-zeroes lump, in which case it's useless and can be discarded
        // k8: don't do it, or k8vavoom will try to rebuild/reload it
      }
      delete xms;
    }

    // blockmap
    // k8: ajbsp blockmap builder (or reader) seems to not work on switch; don't use it at all
    delete[] BlockMapLump;
    BlockMapLump = nullptr;
    BlockMapLumpSize = 0;
  }

  // free any memory used by AJBSP
  ajbsp::FreeLevel();
  ajbsp::FreeQuickAllocCuts();
  ajbsp::FreeQuickAllocSupers();

  ajbsp::cur_info  = nullptr;

  if (ret != build_result_e::BUILD_OK) Host_Error("Node build failed");
}
