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

  //static vuint32 GetTypeHash (const seg_t *sg) { return (vuint32)(ptrdiff_t)sg; }
  //static vuint32 GetTypeHash (const vertex_t *vx) { return (vuint32)(ptrdiff_t)vx; }
}


// ////////////////////////////////////////////////////////////////////////// //
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
  if (!nodes_show_warnings) return;

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
  if (!nodes_show_warnings) return;

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
//  UploadVertices
//
//==========================================================================
/*
static void UploadVertices (VLevel *Level) {
  guard(UploadVertices);
  const vertex_t *pSrc = Level->Vertexes;
  for (int i = 0; i < Level->NumVertexes; ++i, ++pSrc) {
    ajbsp::vertex_t *Vert = ajbsp::NewVertex();
    Vert->x = pSrc->x;
    Vert->y = pSrc->y;
    Vert->index = i;
  }
  ajbsp::num_old_vert = ajbsp::num_vertices;
  unguard;
}
*/


//==========================================================================
//
//  UploadSectors
//
//==========================================================================
static void UploadSectors (VLevel *Level) {
  guard(UploadSectors);
  const sector_t *pSrc = Level->Sectors;
  for (int i = 0; i < Level->NumSectors; ++i, ++pSrc) {
    ajbsp::sector_t *sector = ajbsp::NewSector();
    memset(sector, 0, sizeof(*sector));
    sector->coalesce = (pSrc->tag >= 900 && pSrc->tag < 1000 ? 1 : 0);
    // sector indices never change
    sector->index = i;
    sector->warned_facing = -1;
  }
  unguard;
}


//==========================================================================
//
//  UploadSidedefs
//
//==========================================================================
static void UploadSidedefs (VLevel *Level) {
  guard(UploadSidedefs);
  const side_t *pSrc = Level->Sides;
  for (int i = 0; i < Level->NumSides; ++i, ++pSrc) {
    ajbsp::sidedef_t *side = ajbsp::NewSidedef();
    memset(side, 0, sizeof(*side));
    side->sector = (!pSrc->Sector ? nullptr : ajbsp::LookupSector((int)(ptrdiff_t)(pSrc->Sector-Level->Sectors)));
    if (side->sector) side->sector->is_used = 1;
    // sidedef indices never change
    side->index = i;
  }
  unguard;
}


//==========================================================================
//
//  UploadLinedefs
//
//==========================================================================
static void UploadLinedefs (VLevel *Level) {
  guard(UploadLinedefs);

  TArray<int> vertmap;
  vertmap.SetNum(Level->NumVertexes);
  for (int f = 0; f < Level->NumVertexes; ++f) vertmap[f] = -1;

  const line_t *pSrc = Level->Lines;
  for (int i = 0; i < Level->NumLines; ++i, ++pSrc) {
    ajbsp::linedef_t *line = ajbsp::NewLinedef();
    memset(line, 0, sizeof(*line));
    //if (line == nullptr) Sys_Error("AJBSP: out of memory!");

    // upload vertexes
    if (!pSrc->v1 || !pSrc->v2) Sys_Error("VAVOOM: linedef without vertexes");
    int lv1idx = (int)(ptrdiff_t)(pSrc->v1-Level->Vertexes);
    int lv2idx = (int)(ptrdiff_t)(pSrc->v2-Level->Vertexes);
    if (lv1idx < 0 || lv2idx < 0 || lv1idx >= Level->NumVertexes || lv2idx >= Level->NumVertexes) Sys_Error("VAVOOM: invalid linedef vertexes");
    // check if we already have v1
    if (vertmap[lv1idx] < 0) {
      // add new one
      vertmap[lv1idx] = ajbsp::num_vertices;
      ajbsp::vertex_t *Vert = ajbsp::NewVertex();
      memset(Vert, 0, sizeof(*Vert));
      Vert->x = pSrc->v1->x;
      Vert->y = pSrc->v1->y;
      Vert->index = lv1idx;
      Vert->is_new = 0;
      Vert->is_used = 1;
    }
    // check if we already have v2
    if (vertmap[lv2idx] < 0) {
      // add new one
      vertmap[lv2idx] = ajbsp::num_vertices;
      ajbsp::vertex_t *Vert = ajbsp::NewVertex();
      memset(Vert, 0, sizeof(*Vert));
      Vert->x = pSrc->v2->x;
      Vert->y = pSrc->v2->y;
      Vert->index = lv2idx;
      Vert->is_new = 0;
      Vert->is_used = 1;
    }

    line->start = ajbsp::LookupVertex(vertmap[lv1idx]);
    line->end = ajbsp::LookupVertex(vertmap[lv2idx]);
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
  unguard;
}


//==========================================================================
//
//  UploadThings
//
//==========================================================================
static void UploadThings (VLevel *Level) {
  guard(UploadThings);
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
  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //
struct CopyInfo {
  TArray<int> ajseg2vv; // index: ajbsp seg number; value: new vavoom seg number
  TArray<int> ajvx2vv; // index: ajbsp vertex->index; value: new vavoom vertex index
};


//==========================================================================
//
//  CopyGLVerts
//
//==========================================================================
static void CopyGLVerts (VLevel *Level, CopyInfo &nfo) {
  guard(CopyGLVerts);
  // copy new vertices, build linedef vertex translation table
  int oldmaxverts = Level->NumVertexes;
  vertex_t *oldvx = Level->Vertexes;
  //fprintf(stderr, "num_vertices=%d; num_old_vert=%d; num_new_vert=%d\n", ajbsp::num_vertices, ajbsp::num_old_vert, ajbsp::num_new_vert);
  if (ajbsp::num_old_vert+ajbsp::num_new_vert != ajbsp::num_vertices) Sys_Error("AJBSP: vertex count desync");

  int maxverts = ajbsp::num_old_vert+ajbsp::num_new_vert; // same as `ajbsp::num_vertices`
  nfo.ajvx2vv.SetNum(maxverts*2+8);
  for (int f = 0; f < nfo.ajvx2vv.length(); ++f) nfo.ajvx2vv[f] = -1;

  TArray<int> vxmap; // index: old vertex index; value: new vertex index
  vxmap.SetNum(maxverts);
  for (int f = 0; f < maxverts; ++f) vxmap[f] = -1;

  // new vertexes will get new indexes too
  int newindex = maxverts;

  Level->NumVertexes = 0;
  Level->Vertexes = new vertex_t[maxverts]; //k8: this may overallocate, but i don't care
  memset((void *)Level->Vertexes, 0, sizeof(vertex_t)*maxverts);
  int oldcount = 0;
  int newcount = 0;
  for (int i = 0; i < ajbsp::num_vertices; ++i) {
    ajbsp::vertex_t *vert = ajbsp::LookupVertex(i);
    if (!vert->is_used) continue;
    vertex_t *pDst = &Level->Vertexes[Level->NumVertexes];
    *pDst = TVec(vert->x, vert->y, 0);
    if (!vert->is_new) {
      // old
      if (nfo.ajvx2vv[vert->index] >= 0) Sys_Error("AJBSP: invalid old vector index (0)");
      if (vert->index < 0 || vert->index >= oldmaxverts) Sys_Error("AJBSP: invalid old vector index (1)");
      if (vxmap[vert->index] >= 0) Sys_Error("AJBSP: duplicate old vertex index");
      vxmap[vert->index] = Level->NumVertexes;
      ++oldcount;
    } else {
      // new
      vert->index = newindex++;
      if (nfo.ajvx2vv[vert->index] >= 0) Sys_Error("AJBSP: invalid new vector index (0)");
      ++newcount;
    }
    nfo.ajvx2vv[vert->index] = Level->NumVertexes;
    ++pDst;
    ++Level->NumVertexes;
  }
  GCon->Logf("AJBSP: copied %d of %d used vertexes (%d original, %d new)", Level->NumVertexes, maxverts, oldcount, newcount);

  // update pointers to vertexes in lines
  for (int i = 0; i < Level->NumLines; ++i) {
    line_t *ld = &Level->Lines[i];
    int v1idx = (int)(ptrdiff_t)(ld->v1-oldvx);
    int v2idx = (int)(ptrdiff_t)(ld->v2-oldvx);
    if (v1idx < 0 || v1idx >= oldmaxverts || v2idx < 0 || v2idx >= oldmaxverts) Sys_Error("AJBSP: invalid old vertex index");
    if (vxmap[v1idx] < 0 || vxmap[v2idx] < 0) Sys_Error("AJBSP: invalid new vertex index");
    ld->v1 = &Level->Vertexes[vxmap[v1idx]];
    ld->v2 = &Level->Vertexes[vxmap[v2idx]];
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

  Level->NumSegs = 0;
  delete [] Level->Segs;
  Level->Segs = new seg_t[ajbsp::num_segs]; //k8: this may overallocate, but i don't care
  memset((void *)Level->Segs, 0, sizeof(seg_t)*ajbsp::num_segs);

  int *partners = new int[ajbsp::num_segs]; // indicies for `ajbsp::LookupSeg()`

  nfo.ajseg2vv.SetNum(ajbsp::num_segs);
  int firstPartner = -1, lastPartner = -1;

  for (int i = 0; i < ajbsp::num_segs; ++i) {
    nfo.ajseg2vv[i] = -1;

    ajbsp::seg_t *srcseg = ajbsp::LookupSeg(i);
    // ignore degenerate segs (but keep minisegs)
    if (/*!srcseg->linedef ||*/ srcseg->is_degenerate) {
      //GCon->Logf("skipped miniseg #%d", i);
      continue;
    }
    if (srcseg->index != i) Host_Error("AJBSP: seg #%d has invalid index %d", i, srcseg->index);

    const int dsnum = Level->NumSegs++;
    nfo.ajseg2vv[i] = dsnum;

    seg_t *destseg = &Level->Segs[dsnum];
    partners[dsnum] = -1;
    if (srcseg->partner) {
      if (/*!srcseg->partner->linedef ||*/ srcseg->partner->is_degenerate) {
        GCon->Logf("AJBSP: seg #%d has degenerate partner, ignored", i);
      } else {
        partners[dsnum] = srcseg->partner->index;
        if (firstPartner < 0) firstPartner = dsnum;
        lastPartner = dsnum;
      }
    }

    destseg->partner = nullptr;
    destseg->front_sub = nullptr;

    auto v1mp = nfo.ajvx2vv[srcseg->start->index];
    auto v2mp = nfo.ajvx2vv[srcseg->end->index];
    if (v1mp < 0 || v2mp < 0) Sys_Error("AJBSP: vertex not found for seg #%d", i);
    if (v1mp == v2mp) Sys_Error("AJBSP: seg #%d has same start and end vertex (%d:%d) (%d:%d)", i, v1mp, srcseg->start->index, v2mp, srcseg->end->index);
    destseg->v1 = &Level->Vertexes[v1mp];
    destseg->v2 = &Level->Vertexes[v2mp];

    if (srcseg->side != 0 && srcseg->side != 1) Sys_Error("AJBSP: invalid seg #%d side (%d)", i, srcseg->side);

    if (srcseg->linedef) {
      if (srcseg->linedef->index < 0 || srcseg->linedef->index >= Level->NumLines) Sys_Error("AJBSP: invalid seg #%d linedef (%d), max is %d", i, srcseg->linedef->index, Level->NumLines-1);

      line_t *ldef = &Level->Lines[srcseg->linedef->index];
      destseg->linedef = ldef;

      if (ldef->sidenum[srcseg->side] < 0 || ldef->sidenum[srcseg->side] >= Level->NumSides) {
        Sys_Error("AJBSP: seg #%d: ldef=%d; seg->side=%d; sidenum=%d (max sidenum is %d)\n", i, srcseg->linedef->index, srcseg->side, ldef->sidenum[srcseg->side], Level->NumSides-1);
      }

      //fprintf(stderr, "seg #%d: ldef=%d; seg->side=%d; sidenum=%d\n", i, SrcSeg->linedef->index, SrcSeg->side, ldef->sidenum[SrcSeg->side]);
      destseg->sidedef = &Level->Sides[ldef->sidenum[srcseg->side]];
      destseg->frontsector = Level->Sides[ldef->sidenum[srcseg->side]].Sector;

      if (ldef->flags&ML_TWOSIDED) {
        if (ldef->sidenum[srcseg->side^1] < 0 || ldef->sidenum[srcseg->side^1] >= Level->NumSides) Sys_Error("AJBSP: another side of two-sided linedef is fucked");
        destseg->backsector = Level->Sides[ldef->sidenum[srcseg->side^1]].Sector;
      } else if (ldef->sidenum[srcseg->side^1] >= 0) {
        if (ldef->sidenum[srcseg->side^1] >= Level->NumSides) Sys_Error("AJBSP: another side of blocking two-sided linedef is fucked");
        destseg->backsector = Level->Sides[ldef->sidenum[srcseg->side^1]].Sector;
        // not a two-sided, so clear backsector (just in case) -- nope
        //destseg->backsector = nullptr;
      } else {
        destseg->backsector = nullptr;
        ldef->flags &= ~ML_TWOSIDED; // just in case
      }

      if (srcseg->side) {
        destseg->offset = Length(*destseg->v1 - *ldef->v2);
      } else {
        destseg->offset = Length(*destseg->v1 - *ldef->v1);
      }
    }

    destseg->length = Length(*destseg->v2 - *destseg->v1);
    destseg->side = srcseg->side;

    if (destseg->length < 0.0001) Sys_Error("AJBSP: zero-length seg #%d", i);

    // calc seg's plane params
    CalcSeg(destseg);
  }
  GCon->Logf("AJBSP: copied %d of %d used segs", Level->NumSegs, ajbsp::num_segs);

  // setup partners (we need 'em for self-referencing deep water)
  if (firstPartner == -1) {
    check(lastPartner == -1);
    GCon->Logf(NAME_Dev, "AJBSP: no partner segs found");
  } else {
    check(firstPartner >= 0 && firstPartner < Level->NumSegs);
    check(lastPartner >= 0 && lastPartner < Level->NumSegs);
    GCon->Logf(NAME_Dev, "AJBSP: partner segs range: [%d..%d] of %d", firstPartner, lastPartner, Level->NumSegs);
    for (int i = firstPartner; i <= lastPartner; ++i) {
      if (partners[i] == -1) continue; // no partner for this seg
      seg_t *destseg = &Level->Segs[i];
      if (partners[i] >= nfo.ajseg2vv.length()) { GCon->Logf("ERROR: cannot find partner! (%d, max is %d)", partners[i], nfo.ajseg2vv.length()-1); continue; }
      if (partners[i] < 0) Host_Error("ERROR: cannot find partner! (%d)", partners[i]);
      int psidx = nfo.ajseg2vv[partners[i]];
      if (psidx < 0 || psidx >= Level->NumSegs) Host_Error("GLBSP: invalid partner seg index %d (max is %d)", psidx, Level->NumSegs);
      destseg->partner = &Level->Segs[psidx];
    }
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
  for (int i = 0; i < Level->NumSubsectors; ++i) {
    ajbsp::subsec_t *srcss = ajbsp::LookupSubsec(i);
    int ajidx = srcss->seg_list->index;

    subsector_t *destss = &Level->Subsectors[i];
    destss->numlines = srcss->seg_count;

    auto flidx = nfo.ajseg2vv[ajidx];
    if (flidx < 0) Host_Error("AJBSP: subsector #%d starts with miniseg or degenerate seg", i);
    destss->firstline = flidx;
    // check sector numbers
    for (int j = 0; j < destss->numlines; ++j) {
      auto i2idx = nfo.ajseg2vv[ajidx+j];
      if (i2idx < 0) Host_Error("AJBSP: subsector #%d contains miniseg or degenerate seg #%d (%d)", i, ajidx+j, j);
      if (i2idx != destss->firstline+j) Host_Error("AJBSP: subsector #%d contains non-sequential segs", i);
    }

    // setup sector links
    seg_t *seg = &Level->Segs[destss->firstline];
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
    if (!destss->sector) Host_Error("AJBSP: Subsector #%d without sector", i);
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

  if (setcount != Level->NumSegs) GCon->Logf(NAME_Warning, "AJBSP: %d of %d segs has no front_sub!", Level->NumSegs-setcount, Level->NumSegs);
  unguard;
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

  TVec org = TVec(SrcNode->x, SrcNode->y, 0);
  TVec dir = TVec(SrcNode->dx, SrcNode->dy, 0);
  if (SrcNode->too_long) { dir.x /= 2; dir.y /= 2; }
  // check if `Length()` and `SetPointDirXY()` are happy
  if (dir.x == 0 && dir.y == 0) {
    //Host_Error("AJBSP: invalid BSP node (zero direction)");
    GCon->Logf("AJBSP: invalid BSP node (zero direction%s)", (SrcNode->too_long ? "; overlong node" : ""));
    dir.x = 0.001f;
  }
  Node->SetPointDirXY(org, dir);

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
  guard(CopyNodes);
  Level->NumNodes = ajbsp::num_nodes;
  delete [] Level->Nodes;
  Level->Nodes = new node_t[Level->NumNodes];
  memset((void *)Level->Nodes, 0, sizeof(node_t)*Level->NumNodes);
  if (root_node) {
    int NodeIndex = 0;
    CopyNode(NodeIndex, root_node, Level->Nodes);
    if (NodeIndex != ajbsp::num_nodes) Host_Error("AJBSP: invalid total number of nodes (1)");
  }
  unguard;
}


//==========================================================================
//
//  VLevel::BuildNodesAJ
//
//==========================================================================
void VLevel::BuildNodesAJ () {
  guard(VLevel::BuildNodesAJ);
  // set up glBSP build globals
  nodebuildinfo_t nb_info;
  nb_info.fast = nodes_fast_mode;
  nb_info.warnings = true; // not currently used, but meh
  nb_info.do_blockmap = true;
  nb_info.do_reject = true;

  ajbsp::cur_info = &nb_info;

  //lev_doing_normal = false;
  ajbsp::lev_doing_hexen = !!(LevelFlags&LF_Extended);

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
  //UploadVertices(this); // vertices will be uploaded with linedefs
  UploadSectors(this);
  UploadSidedefs(this);
  UploadLinedefs(this);
  UploadThings(this);

  // other data initialisation
  // always prune vertices at end of lump, otherwise all the
  // unused vertices from seg splits would keep accumulating.
  //ajbsp::PruneVerticesAtEnd();
  GCon->Logf("AJBSP: copied %d original vertexes out of %d", ajbsp::num_vertices, NumVertexes);
  GCon->Logf("AJBSP: building nodes (%s mode)", (nodes_fast_mode ? "fast" : "normal"));

  ajbsp::DetectOverlappingVertices();
  ajbsp::DetectOverlappingLines();

  ajbsp::CalculateWallTips();

  if (ajbsp::lev_doing_hexen) ajbsp::DetectPolyobjSectors(); // -JL- Find sectors containing polyobjs

  //if (cur_info->window_fx) ajbsp::DetectWindowEffects();
  ajbsp::InitBlockmap();

  // create initial segs
  ajbsp::superblock_t *seg_list = ajbsp::CreateSegs();
  ajbsp::node_t *root_node;
  ajbsp::subsec_t *root_sub;
  ajbsp::bbox_t seg_bbox;
  ajbsp::FindLimits(seg_list, &seg_bbox);
  build_result_e ret = ajbsp::BuildNodes(seg_list, &root_node, &root_sub, 0, &seg_bbox);
  ajbsp::FreeSuper(seg_list);

  if (ret == build_result_e::BUILD_OK) {
    ajbsp::ClockwiseBspTree();
    ajbsp::CheckLimits();
    ajbsp::SortSegs();

    GCon->Logf("AJBSP: built with %d nodes, %d subsectors, %d segs, %d vertexes", ajbsp::num_nodes, ajbsp::num_subsecs, ajbsp::num_segs, ajbsp::num_vertices);
    if (root_node && root_node->r.node && root_node->l.node) GCon->Logf("AJBSP: heights of subtrees: %d/%d", ajbsp::ComputeBspHeight(root_node->r.node), ajbsp::ComputeBspHeight(root_node->l.node));
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
    // k8: ajbsp blockmap builder (or reader) seems to not work on switch; don't use it at all
    delete [] BlockMapLump;
    BlockMapLump = nullptr;
    BlockMapLumpSize = 0;
    GCon->Logf("AJBSP: creating BLOCKMAP...");
    CreateBlockMap();
    /*
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

      // `true` means that blockmap was overflowed; let VaVoom rebuild it
      if (!ajbsp::PutBlockmap(*xms)) {
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
      } else {
        GCon->Logf("AJBSP: blockmap overflowed, will be rebuilt by VaVoom");
        delete [] BlockMapLump;
        BlockMapLump = nullptr;
        BlockMapLumpSize = 0;
      }

      delete xms;
    }
    */
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
