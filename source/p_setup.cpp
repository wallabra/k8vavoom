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
//**    Do all the WAD I/O, get map description, set up initial state and
//**  misc. LUTs.
//**
//**************************************************************************
#define DEBUG_DEEP_WATERS

#include <stdlib.h>
#include <string.h>
#include "../libs/core/hashfunc.h"

struct VectorInfo {
  float xy[2];
  unsigned aidx;
  unsigned lidx; // linedef index
  VectorInfo *next;

  //inline bool operator == (const VectorInfo &vi) const { return (xy[0] == vi.xy[0] && xy[1] == vi.xy[1]); }
  //inline bool operator != (const VectorInfo &vi) const { return (xy[0] != vi.xy[0] || xy[1] != vi.xy[1]); }

  inline bool operator == (const VectorInfo &vi) const { return (memcmp(xy, &vi.xy, sizeof(xy)) == 0); }
  inline bool operator != (const VectorInfo &vi) const { return (memcmp(xy, &vi.xy, sizeof(xy)) != 0); }
};

static inline vuint32 GetTypeHash (const VectorInfo &vi) { return joaatHashBuf(vi.xy, sizeof(vi.xy)); }


// ////////////////////////////////////////////////////////////////////////// //
#include "gamedefs.h"
#ifdef SERVER
#include "sv_local.h"
#endif

#include "drawer.h"
#include "render/r_local.h"


static VCvarB dbg_deep_water("dbg_deep_water", false, "Show debug messages in Deep Water processor?", CVAR_PreInit/*|CVAR_Archive*/);
static VCvarB dbg_use_old_decal_pp("dbg_use_old_decal_pp", false, "Use old decal processor? (for timing)", CVAR_PreInit/*|CVAR_Archive*/);

static VCvarB dbg_show_map_hash("dbg_show_map_hash", false, "Show map hash?", CVAR_PreInit/*|CVAR_Archive*/);

static VCvarB loader_cache_rebuilt_data("loader_cache_rebuilt_data", true, "Cache rebuilt nodes, pvs, blockmap, and so on?", CVAR_Archive);
static VCvarF loader_cache_time_limit("loader_cache_time_limit", "3", "Cache data if building took more than this number of seconds.", CVAR_Archive);
static VCvarI loader_cache_max_age_days("loader_cache_max_age_days", "7", "Remove cached data older than this number of days (<=0: none).", CVAR_Archive);

//static VCvarB strict_level_errors("strict_level_errors", true, "Strict level errors mode?", 0);
static VCvarB deepwater_hacks("deepwater_hacks", true, "Apply self-referenced deepwater hacks?", CVAR_Archive);
static VCvarB deepwater_hacks_floor("deepwater_hacks_floor", true, "Apply deepwater hacks to fix some map errors? (not working right yet)", CVAR_Archive);
static VCvarB deepwater_hacks_ceiling("deepwater_hacks_ceiling", true, "Apply deepwater hacks to fix some map errors? (not working right yet)", CVAR_Archive);
static VCvarB build_blockmap("loader_force_blockmap_rebuild", false, "Force blockmap rebuild on map loading?", CVAR_Archive);
//static VCvarB show_level_load_times("show_level_load_times", false, "Show loading times?", CVAR_Archive);

// there seems to be a bug in compressed GL nodes reader, hence the flag
//static VCvarB nodes_allow_compressed_old("nodes_allow_compressed_old", true, "Allow loading v0 compressed GL nodes?", CVAR_Archive);
static VCvarB nodes_allow_compressed("nodes_allow_compressed", false, "Allow loading v1+ compressed GL nodes?", CVAR_Archive);

static VCvarB loader_force_nodes_rebuild("loader_force_nodes_rebuild", false, "Force node rebuilding?", CVAR_Archive);

static VCvarB loader_cache_data("loader_cache_data", false, "Cache built level data?", CVAR_Archive);
static VCvarB loader_cache_ignore_one("loader_cache_ignore_one", false, "Ignore (and remove) cache for next map loading?", CVAR_PreInit);
static VCvarI loader_cache_compression_level("loader_cache_compression_level", "9", "Cache file compression level [0..9]", CVAR_Archive);

static VCvarB loader_force_fix_2s("loader_force_fix_2s", false, "Force-fix invalid two-sided flags? (non-persistent)", CVAR_PreInit/*|CVAR_Archive*/);

//static VCvarB r_udmf_allow_extra_textures("r_udmf_allow_extra_textures", false, "Allow force-loading UDMF textures? (WARNING: savegames WILL crash!)", CVAR_Archive);


extern VCvarI nodes_builder;
extern VCvarI r_max_portal_depth;
extern int pobj_allow_several_in_subsector_override; // <0: disable; >0: enable
#ifdef CLIENT
extern int ldr_extrasamples_override; // -1: no override; 0: disable; 1: enable
extern int r_precalc_static_lights_override; // <0: not set
extern int r_precache_textures_override; // <0: not set
#endif
extern VCvarI nodes_builder;


// lump order in a map WAD: each map needs a couple of lumps
// to provide a complete scene geometry description
enum {
  ML_LABEL,    // a separator, name, ExMx or MAPxx
  ML_THINGS,   // monsters, items
  ML_LINEDEFS, // linedefs, from editing
  ML_SIDEDEFS, // sidedefs, from editing
  ML_VERTEXES, // vertices, edited and BSP splits generated
  ML_SEGS,     // linesegs, from linedefs split by BSP
  ML_SSECTORS, // subsectors, list of linesegs
  ML_NODES,    // BSP nodes
  ML_SECTORS,  // sectors, from editing
  ML_REJECT,   // LUT, sector-sector visibility
  ML_BLOCKMAP, // LUT, motion clipping, walls/grid element
  ML_BEHAVIOR, // ACS scripts
};

// lump order from "GL-Friendly Nodes" specs
enum {
  ML_GL_LABEL, // a separator name, GL_ExMx or GL_MAPxx
  ML_GL_VERT,  // extra Vertices
  ML_GL_SEGS,  // segs, from linedefs & minisegs
  ML_GL_SSECT, // subsectors, list of segs
  ML_GL_NODES, // gl bsp nodes
  ML_GL_PVS,   // potentially visible set
};

// gl-node version identifiers
#define GL_V2_MAGIC  "gNd2"
#define GL_V3_MAGIC  "gNd3"
#define GL_V5_MAGIC  "gNd5"

//  Indicates a GL-specific vertex.
#define GL_VERTEX     (0x8000)
#define GL_VERTEX_V3  (0x40000000)
#define GL_VERTEX_V5  (0x80000000)

// gl-seg flags
#define GL_SEG_FLAG_SIDE  (0x0001)

// old subsector flag
#define NF_SUBSECTOR_OLD  (0x8000)


static const char *CACHE_DATA_SIGNATURE = "VAVOOM CACHED DATA VERSION 006.\n";
static bool cacheCleanupComplete = false;
static TMap<VStr, bool> mapTextureWarns;


//==========================================================================
//
//  VLevel::FixKnownMapErrors
//
//==========================================================================
void VLevel::FixKnownMapErrors () {
  eventKnownMapBugFixer();

  if (LevelFlags&LF_ForceAllowSeveralPObjInSubsector) pobj_allow_several_in_subsector_override = 1;
#ifdef CLIENT
  if (LevelFlags&LF_ForceNoTexturePrecache) r_precache_textures_override = 0;
  if (LevelFlags&LF_ForceNoPrecalcStaticLights) r_precalc_static_lights_override = 0;
#endif
}


//==========================================================================
//
//  hashLump
//
//==========================================================================
static bool hashLump (sha224_ctx *sha224ctx, MD5Context *md5ctx, int lumpnum) {
  if (lumpnum < 0) return false;
  static vuint8 buf[65536];
  VStream *strm = W_CreateLumpReaderNum(lumpnum);
  if (!strm) return false;
  VCheckedStream st(strm);
  auto left = st.TotalSize();
  while (left > 0) {
    int rd = left;
    if (rd > (int)sizeof(buf)) rd = (int)sizeof(buf);
    st.Serialise(buf, rd);
    if (st.IsError()) { delete strm; return false; }
    if (sha224ctx) sha224_update(sha224ctx, buf, rd);
    if (md5ctx) md5ctx->Update(buf, (unsigned)rd);
    //if (xxhash) XXH32_update(xxhash, buf, (unsigned)rd);
    left -= rd;
  }
  return true;
}


//==========================================================================
//
//  getCacheDir
//
//==========================================================================
static VStr getCacheDir () {
  if (!loader_cache_data) return VStr();
  return FL_GetCacheDir();
}


//==========================================================================
//
//  doCacheCleanup
//
//==========================================================================
static void doCacheCleanup () {
  if (cacheCleanupComplete) return;
  cacheCleanupComplete = true;
  if (loader_cache_max_age_days <= 0) return;
  int currtime = Sys_CurrFileTime();
  VStr cpath = getCacheDir();
  if (cpath.length() == 0) return;
  TArray<VStr> dellist;
  auto dh = Sys_OpenDir(cpath);
  if (!dh) return;
  for (;;) {
    VStr fname = Sys_ReadDir(dh);
    if (fname.length() == 0) break;
    if (fname.extractFileExtension().ICmp("cache") != 0) continue;
    VStr shortname = fname;
    fname = cpath+"/"+fname;
    int ftime = Sys_FileTime(fname);
    if (ftime <= 0) {
      GCon->Logf("cache: deleting invalid file '%s'", *shortname);
      dellist.append(fname);
    } else if (ftime < currtime && currtime-ftime > 60*60*24*loader_cache_max_age_days) {
      GCon->Logf("cache: deleting old file '%s'", *shortname);
      dellist.append(fname);
    } else {
      //GCon->Logf("cache: age=%d for '%s'", currtime-ftime, *shortname);
    }
  }
  Sys_CloseDir(dh);
  for (int f = 0; f < dellist.length(); ++f) {
    Sys_FileDelete(dellist[f]);
  }
}


//==========================================================================
//
//  doPlaneIO
//
//==========================================================================
static void doPlaneIO (VStream *strm, TPlane *n) {
  *strm << n->normal.x << n->normal.y << n->normal.z;
  *strm << n->dist /* << n->type << n->signbits */;
}


//==========================================================================
//
//  VLevel::ClearCachedData
//
//==========================================================================
void VLevel::ClearAllLevelData () {
  if (Sectors) {
    for (int i = 0; i < NumSectors; ++i) Sectors[i].DeleteAllRegions();
    // line buffer is shared, so this correctly deletes it
    delete[] Sectors[0].lines;
    Sectors[0].lines = nullptr;
  }

  for (int f = 0; f < NumLines; ++f) {
    line_t *ld = Lines+f;
    delete[] ld->v1lines;
    delete[] ld->v2lines;
  }

  delete[] Vertexes;
  Vertexes = nullptr;
  NumVertexes = 0;

  delete[] Sectors;
  Sectors = nullptr;
  NumSectors = 0;

  delete[] Sides;
  Sides = nullptr;
  NumSides = 0;

  delete[] Lines;
  Lines = nullptr;
  NumLines = 0;

  delete[] Segs;
  Segs = nullptr;
  NumSegs = 0;

  delete[] Subsectors;
  Subsectors = nullptr;
  NumSubsectors = 0;

  delete[] Nodes;
  Nodes = nullptr;
  NumNodes = 0;

  if (VisData) delete[] VisData; else delete[] NoVis;
  VisData = nullptr;
  NoVis = nullptr;

  delete[] BlockMapLump;
  BlockMapLump = nullptr;
  BlockMapLumpSize = 0;

  delete[] BlockLinks;
  BlockLinks = nullptr;

  delete[] RejectMatrix;
  RejectMatrix = nullptr;
  RejectMatrixSize = 0;

  delete[] Things;
  Things = nullptr;
  NumThings = 0;

  GTextureManager.ResetMapTextures();
}


//==========================================================================
//
//  VLevel::SaveCachedData
//
//==========================================================================
void VLevel::SaveCachedData (VStream *strm) {
  if (!strm) return;

  // signature
  strm->Serialize(CACHE_DATA_SIGNATURE, 32);

  vuint8 bspbuilder = nodes_builder;
  *strm << bspbuilder;

  VZipStreamWriter *arrstrm = new VZipStreamWriter(strm, (int)loader_cache_compression_level);

  // flags (nothing for now)
  vuint32 flags = 0;
  *arrstrm << flags;

  // nodes
  *arrstrm << NumNodes;
  GCon->Logf("cache: writing %d nodes", NumNodes);
  for (int f = 0; f < NumNodes; ++f) {
    node_t *n = Nodes+f;
    doPlaneIO(arrstrm, n);
    for (int bbi0 = 0; bbi0 < 2; ++bbi0) {
      for (int bbi1 = 0; bbi1 < 6; ++bbi1) {
        *arrstrm << n->bbox[bbi0][bbi1];
      }
    }
    for (int cci = 0; cci < 2; ++cci) *arrstrm << n->children[cci];
  }

  // vertices
  *arrstrm << NumVertexes;
  GCon->Logf("cache: writing %d vertexes", NumVertexes);
  for (int f = 0; f < NumVertexes; ++f) {
    float x = Vertexes[f].x;
    float y = Vertexes[f].y;
    float z = Vertexes[f].z;
    *arrstrm << x << y << z;
  }

  // write vertex indicies in linedefs
  int lncount = NumLines;
  *arrstrm << lncount;
  GCon->Logf("cache: writing %d linedef vertices", NumLines);
  for (int f = 0; f < NumLines; ++f) {
    line_t &L = Lines[f];
    vint32 v1 = (vint32)(ptrdiff_t)(L.v1-Vertexes);
    vint32 v2 = (vint32)(ptrdiff_t)(L.v2-Vertexes);
    *arrstrm << v1 << v2;
  }

  // subsectors
  *arrstrm << NumSubsectors;
  GCon->Logf("cache: writing %d subsectors", NumSubsectors);
  for (int f = 0; f < NumSubsectors; ++f) {
    subsector_t *ss = Subsectors+f;
    vint32 snum = -1;
    if (ss->sector) snum = (vint32)(ptrdiff_t)(ss->sector-Sectors);
    *arrstrm << snum;
    vint32 slinknum = -1;
    if (ss->seclink) slinknum = (vint32)(ptrdiff_t)(ss->seclink-Subsectors);
    *arrstrm << slinknum;
    *arrstrm << ss->numlines;
    *arrstrm << ss->firstline;
  }

  // sectors
  *arrstrm << NumSectors;
  GCon->Logf("cache: writing %d sectors", NumSectors);
  for (int f = 0; f < NumSectors; ++f) {
    sector_t *sector = &Sectors[f];
    vint32 ssnum = -1;
    if (sector->subsectors) ssnum = (vint32)(ptrdiff_t)(sector->subsectors-Subsectors);
    *arrstrm << ssnum;
  }

  // segs
  *arrstrm << NumSegs;
  GCon->Logf("cache: writing %d segs", NumSegs);
  for (int f = 0; f < NumSegs; ++f) {
    seg_t *seg = Segs+f;
    doPlaneIO(arrstrm, seg);
    vint32 v1num = -1;
    if (seg->v1) v1num = (vint32)(ptrdiff_t)(seg->v1-Vertexes);
    *arrstrm << v1num;
    vint32 v2num = -1;
    if (seg->v2) v2num = (vint32)(ptrdiff_t)(seg->v2-Vertexes);
    *arrstrm << v2num;
    *arrstrm << seg->offset;
    *arrstrm << seg->length;
    *arrstrm << seg->dir;
    vint32 sidedefnum = -1;
    if (seg->sidedef) sidedefnum = (vint32)(ptrdiff_t)(seg->sidedef-Sides);
    *arrstrm << sidedefnum;
    vint32 linedefnum = -1;
    if (seg->linedef) linedefnum = (vint32)(ptrdiff_t)(seg->linedef-Lines);
    *arrstrm << linedefnum;
    vint32 snum = -1;
    if (seg->frontsector) snum = (vint32)(ptrdiff_t)(seg->frontsector-Sectors);
    *arrstrm << snum;
    snum = -1;
    if (seg->backsector) snum = (vint32)(ptrdiff_t)(seg->backsector-Sectors);
    *arrstrm << snum;
    vint32 partnum = -1;
    if (seg->partner) partnum = (vint32)(ptrdiff_t)(seg->partner-Segs);
    *arrstrm << partnum;
    vint32 fssnum = -1;
    if (seg->front_sub) fssnum = (vint32)(ptrdiff_t)(seg->front_sub-Subsectors);
    *arrstrm << fssnum;
    *arrstrm << seg->side;
    *arrstrm << seg->flags;
  }

  // reject
  *arrstrm << RejectMatrixSize;
  if (RejectMatrixSize) {
    GCon->Logf("cache: writing %d bytes of reject table", RejectMatrixSize);
    arrstrm->Serialize(RejectMatrix, RejectMatrixSize);
  }

  // blockmap
  *arrstrm << BlockMapLumpSize;
  if (BlockMapLumpSize) {
    GCon->Logf("cache: writing %d cells of blockmap table", BlockMapLumpSize);
    arrstrm->Serialize(BlockMapLump, BlockMapLumpSize*4);
  }

  //FIXME: store visdata size somewhere
  // pvs
  if (VisData) {
    int rowbytes = (NumSubsectors+7)>>3;
    int vissize = rowbytes*NumSubsectors;
    GCon->Logf("cache: writing %d bytes of pvs table", vissize);
    *arrstrm << vissize;
    if (vissize) arrstrm->Serialize(VisData, vissize);
  } else {
    int vissize = 0;
    *arrstrm << vissize;
  }

  delete arrstrm;

  strm->Flush();
}


//==========================================================================
//
//  VLevel::LoadCachedData
//
//==========================================================================
bool VLevel::LoadCachedData (VStream *strm) {
  if (!strm) return false;
  char sign[32];

  // signature
  strm->Serialise(sign, 32);
  if (strm->IsError() || memcmp(sign, CACHE_DATA_SIGNATURE, 32) != 0) { GCon->Log("invalid cache file signature"); return false; }

  vuint8 bspbuilder = 255;
  *strm << bspbuilder;
  if (bspbuilder != nodes_builder) { GCon->Log("invalid cache nodes builder"); return false; }

  VZipStreamReader *arrstrm = new VZipStreamReader(true, strm);
  if (arrstrm->IsError()) { delete arrstrm; GCon->Log("cannot create cache decompressor"); return false; }

  int vissize = -1;
  int checkSecNum = -1;

  // flags (nothing for now)
  vuint32 flags = 0x29a;
  *arrstrm << flags;
  if (flags != 0) { delete arrstrm; GCon->Log("cache file corrupted (flags)"); return false; }

  //TODO: more checks

  // nodes
  *arrstrm << NumNodes;
  GCon->Logf("cache: reading %d nodes", NumNodes);
  if (NumNodes == 0 || NumNodes > 0x1fffffff) { delete arrstrm; GCon->Log("cache file corrupted (nodes)"); return false; }
  Nodes = new node_t[NumNodes];
  memset((void *)Nodes, 0, NumNodes*sizeof(node_t));
  for (int f = 0; f < NumNodes; ++f) {
    node_t *n = &Nodes[f];
    doPlaneIO(arrstrm, n);
    for (int bbi0 = 0; bbi0 < 2; ++bbi0) {
      for (int bbi1 = 0; bbi1 < 6; ++bbi1) {
        *arrstrm << n->bbox[bbi0][bbi1];
      }
    }
    for (int cci = 0; cci < 2; ++cci) *arrstrm << n->children[cci];
  }

  delete [] Vertexes;
  *arrstrm << NumVertexes;
  GCon->Logf("cache: reading %d vertexes", NumVertexes);
  Vertexes = new vertex_t[NumVertexes];
  memset((void *)Vertexes, 0, sizeof(vertex_t)*NumVertexes);
  for (int f = 0; f < NumVertexes; ++f) {
    float x, y, z;
    *arrstrm << x << y << z;
    Vertexes[f].x = x;
    Vertexes[f].y = y;
    Vertexes[f].z = z;
  }

  // fix up vertex pointers in linedefs
  int lncount = -1;
  *arrstrm << lncount;
  if (lncount != NumLines) { delete arrstrm; GCon->Logf("cache file corrupted (linedefs: got %d, want %d)", lncount, NumLines); return false; }
  GCon->Logf("cache: reading %d linedef vertices", NumLines);
  for (int f = 0; f < NumLines; ++f) {
    line_t &L = Lines[f];
    vint32 v1 = 0, v2 = 0;
    *arrstrm << v1 << v2;
    L.v1 = &Vertexes[v1];
    L.v2 = &Vertexes[v2];
  }

  // subsectors
  *arrstrm << NumSubsectors;
  GCon->Logf("cache: reading %d subsectors", NumSubsectors);
  delete [] Subsectors;
  Subsectors = new subsector_t[NumSubsectors];
  memset((void *)Subsectors, 0, NumSubsectors*sizeof(subsector_t));
  for (int f = 0; f < NumSubsectors; ++f) {
    subsector_t *ss = &Subsectors[f];
    vint32 snum = -1;
    *arrstrm << snum;
    ss->sector = (snum >= 0 ? Sectors+snum : nullptr);
    vint32 slinknum = -1;
    *arrstrm << slinknum;
    ss->seclink = (slinknum >= 0 ? Subsectors+slinknum : nullptr);
    *arrstrm << ss->numlines;
    *arrstrm << ss->firstline;
  }

  // sectors
  GCon->Logf("cache: reading %d sectors", NumSectors);
  *arrstrm << checkSecNum;
  if (checkSecNum != NumSectors) { delete arrstrm; GCon->Logf("cache file corrupted (sectors)"); return false; }
  for (int f = 0; f < NumSectors; ++f) {
    sector_t *sector = &Sectors[f];
    vint32 ssnum = -1;
    *arrstrm << ssnum;
    sector->subsectors = (ssnum >= 0 ? Subsectors+ssnum : nullptr);
  }

  // segs
  *arrstrm << NumSegs;
  GCon->Logf("cache: reading %d segs", NumSegs);
  delete [] Segs;
  Segs = new seg_t[NumSegs];
  memset((void *)Segs, 0, NumSegs*sizeof(seg_t));
  for (int f = 0; f < NumSegs; ++f) {
    seg_t *seg = Segs+f;
    doPlaneIO(arrstrm, seg);
    vint32 v1num = -1;
    *arrstrm << v1num;
    if (v1num < 0 || v1num >= NumVertexes) { delete arrstrm; GCon->Log("cache file corrupted (seg v1)"); return false; }
    seg->v1 = Vertexes+v1num;
    vint32 v2num = -1;
    *arrstrm << v2num;
    if (v2num < 0 || v1num >= NumVertexes) { delete arrstrm; GCon->Log("cache file corrupted (seg v2)"); return false; }
    seg->v2 = Vertexes+v2num;
    *arrstrm << seg->offset;
    *arrstrm << seg->length;
    *arrstrm << seg->dir;
    vint32 sidedefnum = -1;
    *arrstrm << sidedefnum;
    seg->sidedef = (sidedefnum >= 0 ? Sides+sidedefnum : nullptr);
    vint32 linedefnum = -1;
    *arrstrm << linedefnum;
    seg->linedef = (linedefnum >= 0 ? Lines+linedefnum : nullptr);
    vint32 snum = -1;
    *arrstrm << snum;
    seg->frontsector = (snum >= 0 ? Sectors+snum : nullptr);
    snum = -1;
    *arrstrm << snum;
    seg->backsector = (snum >= 0 ? Sectors+snum : nullptr);
    vint32 partnum = -1;
    *arrstrm << partnum;
    seg->partner = (partnum >= 0 ? Segs+partnum : nullptr);
    vint32 fssnum = -1;
    *arrstrm << fssnum;
    seg->front_sub = (fssnum >= 0 ? Subsectors+fssnum : nullptr);
    *arrstrm << seg->side;
    *arrstrm << seg->flags;
  }

  // reject
  *arrstrm << RejectMatrixSize;
  if (RejectMatrixSize < 0 || RejectMatrixSize > 0x1fffffff) { delete arrstrm; GCon->Log("cache file corrupted (reject)"); return false; }
  if (RejectMatrixSize) {
    GCon->Logf("cache: reading %d bytes of reject table", RejectMatrixSize);
    RejectMatrix = new vuint8[RejectMatrixSize];
    arrstrm->Serialize(RejectMatrix, RejectMatrixSize);
  }

  // blockmap
  *arrstrm << BlockMapLumpSize;
  if (BlockMapLumpSize < 0 || BlockMapLumpSize > 0x1fffffff) { delete arrstrm; GCon->Log("cache file corrupted (blockmap)"); return false; }
  if (BlockMapLumpSize) {
    GCon->Logf("cache: reading %d cells of blockmap table", BlockMapLumpSize);
    BlockMapLump = new vint32[BlockMapLumpSize];
    arrstrm->Serialize(BlockMapLump, BlockMapLumpSize*4);
  }

  // pvs
  *arrstrm << vissize;
  if (vissize < 0 || vissize > 0x6fffffff) { delete arrstrm; GCon->Log("cache file corrupted (pvs)"); return false; }
  if (vissize > 0) {
    GCon->Logf("cache: reading %d bytes of pvs table", vissize);
    VisData = new vuint8[vissize];
    arrstrm->Serialize(VisData, vissize);
  }

  if (arrstrm->IsError()) { delete arrstrm; GCon->Log("cache file corrupted (read error)"); return false; }
  delete arrstrm;
  return true;
}


//==========================================================================
//
//  VLevel::SetupThingsFromMapinfo
//
//==========================================================================
void VLevel::SetupThingsFromMapinfo () {
  // use hashmap to avoid schlemiel's lookups
  TMapNC<int, mobjinfo_t *> id2nfo;
  for (int tidx = 0; tidx < NumThings; ++tidx) {
    mthing_t *th = &Things[tidx];
    if (th->type == 0) continue;
    mobjinfo_t *nfo = nullptr;
    auto fp = id2nfo.find(th->type);
    if (fp) {
      nfo = *fp;
    } else {
      nfo = VClass::FindMObjId(th->type, GGameInfo->GameFilterFlag);
      id2nfo.put(th->type, nfo);
    }
    if (nfo) {
      // allskills
      if (nfo->flags&mobjinfo_t::FlagNoSkill) {
        //GCon->Logf("*** THING %d got ALLSKILLS", th->type);
        th->SkillClassFilter |= 0x03;
        th->SkillClassFilter |= 0x04;
        th->SkillClassFilter |= 0x18;
      }
      // special
      if (nfo->flags&mobjinfo_t::FlagSpecial) {
        th->special = nfo->special;
        th->arg1 = nfo->args[0];
        th->arg2 = nfo->args[1];
        th->arg3 = nfo->args[2];
        th->arg4 = nfo->args[3];
        th->arg5 = nfo->args[4];
      }
    }
  }
}


//==========================================================================
//
//  VLevel::LoadMap
//
//==========================================================================
void VLevel::LoadMap (VName AMapName) {
  bool killCache = loader_cache_ignore_one;
  loader_cache_ignore_one = false;
  bool AuxiliaryMap = false;
  int lumpnum, xmaplumpnum;
  VName MapLumpName;
  decanimlist = nullptr;
  decanimuid = 0;

  mapTextureWarns.clear();

  if (csTouched) Z_Free(csTouched);
  csTouchCount = 0;
  csTouched = nullptr;

load_again:
  GTextureManager.ResetMapTextures();

  pobj_allow_several_in_subsector_override = 0;
#ifdef CLIENT
  ldr_extrasamples_override = -1;
  r_precalc_static_lights_override = -1;
  r_precache_textures_override = -1;
#endif

  double TotalTime = -Sys_Time();
  double InitTime = -Sys_Time();
  MapName = AMapName;
  MapHash = VStr();
  MapHashMD5 = VStr();
  // If working with a devlopment map, reload it.
  // k8: nope, it doesn't work this way: it looks for "maps/xxx.wad" in zips,
  //     and "complete.pk3" takes precedence over any pwads
  //     so let's do it backwards
  // Find map and GL nodes.
  lumpnum = W_CheckNumForName(MapName);
  MapLumpName = MapName;
  int wadlumpnum = W_CheckNumForFileName(va("maps/%s.wad", *MapName));
  if (wadlumpnum > lumpnum) lumpnum = -1;
  // if there is no map lump, try map wad
  if (lumpnum < 0) {
    // check if map wad is here
    VStr aux_file_name = va("maps/%s.wad", *MapName);
    if (FL_FileExists(aux_file_name)) {
      // append map wad to list of wads (it will be deleted later)
      xmaplumpnum = W_CheckNumForFileName(va("maps/%s.wad", *MapName));
      lumpnum = W_OpenAuxiliary(aux_file_name);
      if (lumpnum >= 0) {
        MapLumpName = W_LumpName(lumpnum);
        AuxiliaryMap = true;
      }
    }
  } else {
    xmaplumpnum = lumpnum;
  }
  if (lumpnum < 0) Host_Error("Map \"%s\" not found", *MapName);

  // some idiots embeds wads into wads
  if (!AuxiliaryMap && lumpnum >= 0 && W_LumpLength(lumpnum) > 128 && W_LumpLength(lumpnum) < 1024*1024) {
    VStream *lstrm = W_CreateLumpReaderNum(lumpnum);
    if (lstrm) {
      char sign[4];
      lstrm->Serialise(sign, 4);
      if (!lstrm->IsError() && memcmp(sign, "PWAD", 4) == 0) {
        lstrm->Seek(0);
        xmaplumpnum = lumpnum;
        lumpnum = W_AddAuxiliaryStream(lstrm, WAuxFileType::Wad);
        if (lumpnum >= 0) {
          MapLumpName = W_LumpName(lumpnum);
          AuxiliaryMap = true;
        } else {
          Host_Error("cannot open pwad for \"%s\"", *MapName);
        }
      } else {
        delete lstrm;
      }
    }
  }

  //FIXME: reload saved background screen from FBO
  R_LdrMsgReset();
  R_LdrMsgShowMain("LOADING...");

  bool saveCachedData = false;
  int gl_lumpnum = -100;
  int ThingsLump = -1;
  int LinesLump = -1;
  int SidesLump = -1;
  int VertexesLump = -1;
  int SectorsLump = -1;
  int RejectLump = -1;
  int BlockmapLumpNum = -1;
  int BehaviorLump = -1;
  int DialogueLump = -1;
  int CompressedGLNodesLump = -1;
  bool UseComprGLNodes = false;
  bool NeedNodesBuild = false;
  char GLNodesHdr[4];
  const mapInfo_t &MInfo = P_GetMapInfo(MapName);
  memset(GLNodesHdr, 0, sizeof(GLNodesHdr));

  VisData = nullptr;
  NoVis = nullptr;

  sha224_ctx sha224ctx;
  MD5Context md5ctx;

  sha224_init(&sha224ctx);
  md5ctx.Init();

  bool sha224valid = false;
  VStr cacheFileName;
  VStr cacheDir = getCacheDir();

  // check for UDMF map
  if (W_LumpName(lumpnum+1) == NAME_textmap) {
    LevelFlags |= LF_TextMap;
    NeedNodesBuild = true;
    for (int i = 2; true; ++i) {
      VName LName = W_LumpName(lumpnum+i);
      if (LName == NAME_endmap) break;
      if (LName == NAME_None) Host_Error("Map %s is not a valid UDMF map", *MapName);
           if (LName == NAME_behavior) BehaviorLump = lumpnum+i;
      else if (LName == NAME_blockmap) BlockmapLumpNum = lumpnum+i;
      else if (LName == NAME_reject) RejectLump = lumpnum+i;
      else if (LName == NAME_dialogue) DialogueLump = lumpnum+i;
      else if (LName == NAME_znodes) {
        if (!loader_cache_rebuilt_data && nodes_allow_compressed) {
          CompressedGLNodesLump = lumpnum+i;
          UseComprGLNodes = true;
          NeedNodesBuild = false;
        }
      }
    }
    sha224valid = hashLump(&sha224ctx, &md5ctx, lumpnum+1);
  } else {
    // find all lumps
    int LIdx = lumpnum+1;
    int SubsectorsLump = -1;
    if (W_LumpName(LIdx) == NAME_things) ThingsLump = LIdx++;
    if (W_LumpName(LIdx) == NAME_linedefs) LinesLump = LIdx++;
    if (W_LumpName(LIdx) == NAME_sidedefs) SidesLump = LIdx++;
    if (W_LumpName(LIdx) == NAME_vertexes) VertexesLump = LIdx++;
    if (W_LumpName(LIdx) == NAME_segs) LIdx++;
    if (W_LumpName(LIdx) == NAME_ssectors) SubsectorsLump = LIdx++;
    if (W_LumpName(LIdx) == NAME_nodes) LIdx++;
    if (W_LumpName(LIdx) == NAME_sectors) SectorsLump = LIdx++;
    if (W_LumpName(LIdx) == NAME_reject) RejectLump = LIdx++;
    if (W_LumpName(LIdx) == NAME_blockmap) BlockmapLumpNum = LIdx++;

    sha224valid = hashLump(nullptr, &md5ctx, lumpnum); // md5
    if (sha224valid) sha224valid = hashLump(nullptr, &md5ctx, ThingsLump); // md5

    if (sha224valid) sha224valid = hashLump(&sha224ctx, &md5ctx, LinesLump);
    if (sha224valid) sha224valid = hashLump(&sha224ctx, &md5ctx, SidesLump);
    if (sha224valid) sha224valid = hashLump(&sha224ctx, nullptr, VertexesLump); // not in md5
    if (sha224valid) sha224valid = hashLump(&sha224ctx, &md5ctx, SectorsLump);

    // determine level format
    if (W_LumpName(LIdx) == NAME_behavior) {
      LevelFlags |= LF_Extended;
      BehaviorLump = LIdx++;
      if (sha224valid) sha224valid = hashLump(nullptr, &md5ctx, BehaviorLump); // md5
    }

    //  Verify that it's a valid map.
    if (ThingsLump == -1 || LinesLump == -1 || SidesLump == -1 ||
        VertexesLump == -1 || SectorsLump == -1)
    {
      VStr nf = "missing lumps:";
      if (ThingsLump == -1) nf += " things";
      if (LinesLump == -1) nf += " lines";
      if (SidesLump == -1) nf += " sides";
      if (VertexesLump == -1) nf += " vertexes";
      if (SectorsLump == -1) nf += " sectors";
      Host_Error("Map '%s' is not a valid map (%s), %s", *MapName, *W_FullLumpName(lumpnum), *nf);
    }

    if (SubsectorsLump != -1) {
      VStream *TmpStrm = W_CreateLumpReaderNum(SubsectorsLump);
      if (TmpStrm->TotalSize() > 4) {
        TmpStrm->Serialise(GLNodesHdr, 4);
        if (TmpStrm->IsError()) GLNodesHdr[0] = 0;
        if ((GLNodesHdr[0] == 'Z' || GLNodesHdr[0] == 'X') &&
            GLNodesHdr[1] == 'G' && GLNodesHdr[2] == 'L' &&
            (GLNodesHdr[3] == 'N' || GLNodesHdr[3] == '2' || GLNodesHdr[3] == '3'))
        {
          UseComprGLNodes = true;
          CompressedGLNodesLump = SubsectorsLump;
        } /*else if ((GLNodesHdr[0] == 'Z' || GLNodesHdr[0] == 'X') &&
                    GLNodesHdr[1] == 'N' && GLNodesHdr[2] == 'O' && GLNodesHdr[3] == 'D')
        {
          UseComprGLNodes = true;
          CompressedGLNodesLump = SubsectorsLump;
        }*/
      }
      delete TmpStrm;
    }
  }
  InitTime += Sys_Time();

  if (sha224valid) {
    vuint8 sha224hash[SHA224_DIGEST_SIZE];
    sha224_final(&sha224ctx, sha224hash);
    MapHash = VStr::buf2hex(sha224hash, SHA224_DIGEST_SIZE);

    vuint8 md5digest[MD5Context::DIGEST_SIZE];
    md5ctx.Final(md5digest);
    MapHashMD5 = VStr::buf2hex(md5digest, MD5Context::DIGEST_SIZE);

    if (dbg_show_map_hash) GCon->Logf("MAP HASH MD5: %s", *MapHashMD5);
    else if (developer) GCon->Logf(NAME_Dev, "MAP HASH MD5: %s", *MapHashMD5);
  }

  bool cachedDataLoaded = false;
  if (sha224valid && cacheDir.length()) {
    cacheFileName = VStr("mapcache_")+MapHash.left(32)+".cache"; // yeah, truncated
    cacheFileName = cacheDir+"/"+cacheFileName;
  } else {
    sha224valid = false;
  }

  bool hasCacheFile = false;

  //FIXME: load cache file into temp buffer, and process it later
  if (sha224valid) {
    if (killCache) {
      Sys_FileDelete(cacheFileName);
    } else {
      VStream *strm = FL_OpenSysFileRead(cacheFileName);
      hasCacheFile = !!strm;
      delete strm;
    }
  }

  //bool glNodesFound = false;

  if (hasCacheFile) {
    UseComprGLNodes = false;
    CompressedGLNodesLump = -1;
    NeedNodesBuild = false;
  } else {
    if (!loader_force_nodes_rebuild && !(LevelFlags&LF_TextMap) && !UseComprGLNodes) {
      gl_lumpnum = FindGLNodes(MapLumpName);
      if (gl_lumpnum < lumpnum) {
        GCon->Logf("no GL nodes found, VaVoom will use internal node builder");
        NeedNodesBuild = true;
      } else {
        //glNodesFound = true;
      }
    } else {
      if ((LevelFlags&LF_TextMap) != 0 || !UseComprGLNodes) NeedNodesBuild = true;
    }
  }


  int NumBaseVerts;
  double VertexTime = 0;
  double SectorsTime = 0;
  double LinesTime = 0;
  double ThingsTime = 0;
  double TranslTime = 0;
  double SidesTime = 0;
  double DecalProcessingTime = 0;

  {
    auto texLock = GTextureManager.LockMapLocalTextures();

    // begin processing map lumps
    if (LevelFlags&LF_TextMap) {
      VertexTime = -Sys_Time();
      LoadTextMap(lumpnum+1, MInfo);
      VertexTime += Sys_Time();
    } else {
      // Note: most of this ordering is important
      VertexTime = -Sys_Time();
      LevelFlags &= ~LF_GLNodesV5;
      LoadVertexes(VertexesLump, gl_lumpnum+ML_GL_VERT, NumBaseVerts);
      VertexTime += Sys_Time();
      SectorsTime = -Sys_Time();
      LoadSectors(SectorsLump);
      SectorsTime += Sys_Time();
      LinesTime = -Sys_Time();
      if (!(LevelFlags&LF_Extended)) {
        LoadLineDefs1(LinesLump, NumBaseVerts, MInfo);
        LinesTime += Sys_Time();
        ThingsTime = -Sys_Time();
        LoadThings1(ThingsLump);
      } else {
        LoadLineDefs2(LinesLump, NumBaseVerts, MInfo);
        LinesTime += Sys_Time();
        ThingsTime = -Sys_Time();
        LoadThings2(ThingsLump);
      }
      ThingsTime += Sys_Time();

      TranslTime = -Sys_Time();
      if (!(LevelFlags&LF_Extended)) {
        // translate level to Hexen format
        GGameInfo->eventTranslateLevel(this);
      }
      TranslTime += Sys_Time();
      // set up textures after loading lines because for some Boom line
      // specials there can be special meaning of some texture names
      SidesTime = -Sys_Time();
      LoadSideDefs(SidesLump);
      SidesTime += Sys_Time();
    }
  }

  //double Lines2Time = -Sys_Time();
  FixKnownMapErrors();
  bool forceNodeRebuildFromFixer = !!(LevelFlags&LF_ForceRebuildNodes);
  //Lines2Time += Sys_Time();

  //HACK! fix things skill settings
  SetupThingsFromMapinfo();

  if (hasCacheFile) {
    //GCon->Logf("using cache file: %s", *cacheFileName);
    VStream *strm = FL_OpenSysFileRead(cacheFileName);
    cachedDataLoaded = LoadCachedData(strm);
    if (!cachedDataLoaded) {
      GCon->Logf("cache data is obsolete or in invalid format");
      delete strm;
      Sys_FileDelete(cacheFileName);
      ClearAllLevelData();
      goto load_again;
      //if (!glNodesFound) NeedNodesBuild = true;
    }
    delete strm;
    if (cachedDataLoaded) forceNodeRebuildFromFixer = false; //k8: is this right?
  }

  double NodesTime = -Sys_Time();
  // and again; sorry!
  if (!cachedDataLoaded || forceNodeRebuildFromFixer) {
    if (NeedNodesBuild || forceNodeRebuildFromFixer) {
      GCon->Logf("building GL nodes...");
      //R_LdrMsgShowSecondary("BUILDING NODES...");
      BuildNodes();
      saveCachedData = true;
    } else if (UseComprGLNodes) {
      if (!LoadCompressedGLNodes(CompressedGLNodesLump, GLNodesHdr)) {
        GCon->Logf("rebuilding GL nodes...");
        //R_LdrMsgShowSecondary("BUILDING NODES...");
        BuildNodes();
        saveCachedData = true;
      }
    } else {
      LoadGLSegs(gl_lumpnum+ML_GL_SEGS, NumBaseVerts);
      LoadSubsectors(gl_lumpnum+ML_GL_SSECT);
      LoadNodes(gl_lumpnum+ML_GL_NODES);
      LoadPVS(gl_lumpnum+ML_GL_PVS);
    }
  }

  HashSectors();
  HashLines();
  FinaliseLines();

  PostLoadSegs();
  PostLoadSubsectors();

  // create blockmap
  if (!BlockMapLump) {
    GCon->Logf("creating BLOCKMAP...");
    CreateBlockMap();
  }

  NodesTime += Sys_Time();

  // load blockmap
  if ((build_blockmap || forceNodeRebuildFromFixer) && BlockMapLump) {
    delete[] BlockMapLump;
    BlockMapLump = nullptr;
    BlockMapLumpSize = 0;
  }

  double BlockMapTime = -Sys_Time();
  if (!BlockMapLump) {
    LoadBlockMap(forceNodeRebuildFromFixer || NeedNodesBuild ? -1 : BlockmapLumpNum);
    saveCachedData = true;
  }
  {
    BlockMapOrgX = BlockMapLump[0];
    BlockMapOrgY = BlockMapLump[1];
    BlockMapWidth = BlockMapLump[2];
    BlockMapHeight = BlockMapLump[3];
    BlockMap = BlockMapLump+4;

    // clear out mobj chains
    int count = BlockMapWidth*BlockMapHeight;
    delete [] BlockLinks;
    BlockLinks = new VEntity *[count];
    memset(BlockLinks, 0, sizeof(VEntity *)*count);
  }
  BlockMapTime += Sys_Time();

  // rebuild PVS if we have none (just in case)
  // cached data loader took care of this
  double BuildPVSTime = -1;
  if (NoVis == nullptr && VisData == nullptr) {
    BuildPVSTime = -Sys_Time();
    BuildPVS();
    BuildPVSTime += Sys_Time();
    if (VisData) saveCachedData = true;
  }

  // load reject table
  double RejectTime = -Sys_Time();
  if (!RejectMatrix) {
    LoadReject(RejectLump);
    saveCachedData = true;
  }
  RejectTime += Sys_Time();


  // update cache
  if (loader_cache_data && saveCachedData && sha224valid && TotalTime+Sys_Time() > loader_cache_time_limit) {
    VStream *strm = FL_OpenSysFileWrite(cacheFileName);
    SaveCachedData(strm);
    delete strm;
  }
  doCacheCleanup();


  // ACS object code
  double AcsTime = -Sys_Time();
  LoadACScripts(BehaviorLump, xmaplumpnum);
  AcsTime += Sys_Time();

  double GroupLinesTime = -Sys_Time();
  GroupLines();
  GroupLinesTime += Sys_Time();

  double FloodZonesTime = -Sys_Time();
  FloodZones();
  FloodZonesTime += Sys_Time();

  double ConvTime = -Sys_Time();
  // load conversations
  LoadRogueConScript(GGameInfo->GenericConScript, -1, GenericSpeeches, NumGenericSpeeches);
  if (DialogueLump >= 0) {
    LoadRogueConScript(NAME_None, DialogueLump, LevelSpeeches, NumLevelSpeeches);
  } else {
    LoadRogueConScript(GGameInfo->eventGetConScriptName(MapName), -1, LevelSpeeches, NumLevelSpeeches);
  }
  ConvTime += Sys_Time();

  // set up polyobjs, slopes, 3D floors and some other static stuff
  double SpawnWorldTime = -Sys_Time();
  GGameInfo->eventSpawnWorld(this);
  // hash it all again, 'cause spawner may change something
  HashSectors();
  HashLines();
  SpawnWorldTime += Sys_Time();

  double InitPolysTime = -Sys_Time();
  InitPolyobjs(); // Initialise the polyobjs
  InitPolysTime += Sys_Time();

  double MinMaxTime = -Sys_Time();
  // we need this for client
  for (int i = 0; i < NumSectors; i++) CalcSecMinMaxs(&Sectors[i]);
  MinMaxTime += Sys_Time();

  double WallShadesTime = -Sys_Time();
  if (MInfo.HorizWallShade|MInfo.VertWallShade) {
    line_t *Line = Lines;
    for (int i = NumLines; i--; ++Line) {
      int shadeChange =
        !Line->normal.x ? MInfo.HorizWallShade :
        !Line->normal.y ? MInfo.VertWallShade :
        0;
      if (shadeChange) {
        for (int sn = 0; sn < 2; ++sn) {
          const int sidx = Line->sidenum[sn];
          if (sidx >= 0) {
            side_t *side = &Sides[sidx];
            side->Light += shadeChange;
          }
        }
      }
    }
  }
  WallShadesTime += Sys_Time();

  double RepBaseTime = -Sys_Time();
  CreateRepBase();
  RepBaseTime += Sys_Time();

  //GCon->Logf("Building Lidedef VV list...");
  double LineVVListTime = -Sys_Time();
  if (dbg_use_old_decal_pp) {
    BuildDecalsVVListOld();
  } else {
    BuildDecalsVVList();
  }
  LineVVListTime += Sys_Time();

  // end of map lump processing
  if (AuxiliaryMap) {
    // close the auxiliary file(s)
    W_CloseAuxiliary();
  }

  DecalProcessingTime = -Sys_Time();
  PostProcessForDecals();
  DecalProcessingTime += Sys_Time();

  // do it here, so it won't touch sloped floors
  // it will set `othersec` for sectors too
  FixDeepWaters();

  // this must be called after deepwater fixes
  BuildSectorLists();

  // calculate xxHash32 of various map parts

  // hash of linedefs, sidedefs, sectors (in this order)
  {
    //GCon->Logf("*** LSSHash: 0x%08x (%d:%d:%d)", LSSHash, NumLines, NumSides, NumSectors);
    XXH32_state_t *lssXXHash = XXH32_createState();
    XXH32_reset(lssXXHash, (unsigned)(NumLines+NumSides+NumSectors));
    for (int f = 0; f < NumLines; ++f) xxHashLinedef(lssXXHash, Lines[f]);
    for (int f = 0; f < NumSides; ++f) xxHashSidedef(lssXXHash, Sides[f]);
    for (int f = 0; f < NumSectors; ++f) xxHashSectordef(lssXXHash, Sectors[f]);
    LSSHash = XXH32_digest(lssXXHash);
    XXH32_freeState(lssXXHash);
    //GCon->Logf("*** LSSHash: 0x%08x", LSSHash);
  }

  // hash of segs
  {
    //GCon->Logf("*** SegHash: 0x%08x (%d)", SegHash, NumSegs);
    XXH32_state_t *segXXHash = XXH32_createState();
    XXH32_reset(segXXHash, (unsigned)NumSegs);
    for (int f = 0; f < NumSegs; ++f) xxHashSegdef(segXXHash, Segs[f]);
    SegHash = XXH32_digest(segXXHash);
    XXH32_freeState(segXXHash);
    //GCon->Logf("*** SegHash: 0x%08x", SegHash);
  }


  TotalTime += Sys_Time();
  if (true /*|| show_level_load_times*/) {
    GCon->Logf("-------");
    GCon->Logf("Level loadded in %f", TotalTime);
    //GCon->Logf("Initialisation   %f", InitTime);
    //GCon->Logf("Vertexes         %f", VertexTime);
    //GCon->Logf("Sectors          %f", SectorsTime);
    //GCon->Logf("Lines            %f", LinesTime);
    //GCon->Logf("Things           %f", ThingsTime);
    //GCon->Logf("Translation      %f", TranslTime);
    //GCon->Logf("Sides            %f", SidesTime);
    //GCon->Logf("Lines 2          %f", Lines2Time);
    GCon->Logf("Nodes            %f", NodesTime);
    GCon->Logf("Block map        %f", BlockMapTime);
    GCon->Logf("Reject           %f", RejectTime);
    if (BuildPVSTime >= 0.1) GCon->Logf("PVS build        %f", BuildPVSTime);
    //GCon->Logf("ACS              %f", AcsTime);
    //GCon->Logf("Group lines      %f", GroupLinesTime);
    //GCon->Logf("Flood zones      %f", FloodZonesTime);
    //GCon->Logf("Conversations    %f", ConvTime);
    GCon->Logf("Spawn world      %f", SpawnWorldTime);
    GCon->Logf("Polyobjs         %f", InitPolysTime);
    GCon->Logf("Sector minmaxs   %f", MinMaxTime);
    GCon->Logf("Wall shades      %f", WallShadesTime);
    GCon->Logf("Linedef VV list  %f", LineVVListTime);
    GCon->Logf("Decal processing %f", DecalProcessingTime);
    //GCon->Logf("%s", ""); // shut up, gcc!
  }

  mapTextureWarns.clear();

  RecalcWorldBBoxes();
}


//==========================================================================
//
//  VLevel::BuildDecalsVVList
//
//  build v1 and v2 lists (for decals)
//
//==========================================================================
void VLevel::BuildDecalsVVList () {
  if (NumLines < 1) return; // just in case

  // build hashes and lists
  TMapNC<VectorInfo, unsigned> vmap; // value: index in in tarray
  TArray<VectorInfo> va;
  va.SetLength(NumLines*2);
  line_t *ld = Lines;
  for (unsigned i = 0; i < (unsigned)NumLines; ++i, ++ld) {
    ld->decalMark = 0;
    ld->v1linesCount = ld->v2linesCount = 0;
    ld->v1lines = ld->v2lines = nullptr;
    for (unsigned vn = 0; vn < 2; ++vn) {
      const unsigned aidx = i*2+vn;
      VectorInfo *vi = &va[aidx];
      const TVec *vertex = (vn == 0 ? ld->v1 : ld->v2);
      vi->xy[0] = vertex->x;
      vi->xy[1] = vertex->y;
      vi->aidx = aidx;
      vi->lidx = i;
      vi->next = nullptr;
      auto vaidxp = vmap.find(*vi);
      if (vaidxp) {
        check(*vaidxp != vi->aidx);
        VectorInfo *cv = &va[*vaidxp];
        while (cv->next) {
          check(cv->aidx < aidx);
          if (*cv != *vi) Sys_Error("VLevel::BuildDecalsVVList: OOPS(0)!");
          cv = cv->next;
        }
        if (*cv != *vi) Sys_Error("VLevel::BuildDecalsVVList: OOPS(1)!");
        cv->next = vi;
      } else {
        vmap.put(*vi, vi->aidx);
      }
    }
  }

  line_t **wklist = new line_t *[NumLines*2];
  vuint8 *wkhit = new vuint8[NumLines];

  // fill linedef lists
  ld = Lines;
  for (unsigned i = 0; i < (unsigned)NumLines; ++i, ++ld) {
    for (unsigned vn = 0; vn < 2; ++vn) {
      unsigned count = 0;
      memset(wkhit, 0, NumLines*sizeof(wkhit[0]));
      wkhit[i] = 1;
      /*
      for (int curvn = 0; curvn < 2; ++curvn) {
        VectorInfo *vi = &va[i*2+curvn];
        auto vaidxp = vmap.find(*vi);
        if (!vaidxp) Sys_Error("VLevel::BuildDecalsVVList: internal error (0)");
        VectorInfo *cv = &va[*vaidxp];
        while (cv) {
          if (!wkhit[cv->lidx]) {
            if (*cv != *vi) Sys_Error("VLevel::BuildDecalsVVList: OOPS(2)!");
            wkhit[cv->lidx] = 1;
            wklist[count++] = Lines+cv->lidx;
          }
          cv = cv->next;
        }
      }
      */
      {
        VectorInfo *vi = &va[i*2+vn];
        auto vaidxp = vmap.find(*vi);
        if (!vaidxp) Sys_Error("VLevel::BuildDecalsVVList: internal error (0)");
        VectorInfo *cv = &va[*vaidxp];
        while (cv) {
          if (!wkhit[cv->lidx]) {
            if (*cv != *vi) Sys_Error("VLevel::BuildDecalsVVList: OOPS(2)!");
            wkhit[cv->lidx] = 1;
            wklist[count++] = Lines+cv->lidx;
          }
          cv = cv->next;
        }
      }
      if (count > 0) {
        line_t **list = new line_t *[count];
        memcpy(list, wklist, count*sizeof(wklist[0]));
        if (vn == 0) {
          ld->v1linesCount = count;
          ld->v1lines = list;
        } else {
          ld->v2linesCount = count;
          ld->v2lines = list;
        }
      }
    }
  }

  delete [] wkhit;
  delete [] wklist;
}


//==========================================================================
//
//  VLevel::BuildDecalsVVListOld
//
//  build v1 and v2 lists (for decals)
//
//==========================================================================
void VLevel::BuildDecalsVVListOld () {
  for (int i = 0; i < NumLines; ++i) {
    line_t *ld = Lines+i;
    ld->decalMark = 0;
    ld->v1linesCount = ld->v2linesCount = 0;
    ld->v1lines = ld->v2lines = nullptr;
    for (int vn = 0; vn < 2; ++vn) {
      // count number of lines
      TVec v = *(vn == 0 ? ld->v1 : ld->v2);
      v.z = 0;
      int count = 0;
      for (int f = 0; f < NumLines; ++f) {
        if (f == i) continue;
        line_t *l2 = Lines+f;
        TVec l2v1 = *l2->v1, l2v2 = *l2->v2;
        l2v1.z = l2v2.z = 0;
        if (l2v1 == v || l2v2 == v) ++count;
      }
      if (count) {
        line_t **list = new line_t *[count];
        count = 0;
        for (int f = 0; f < NumLines; ++f) {
          if (f == i) continue;
          line_t *l2 = Lines+f;
          TVec l2v1 = *l2->v1, l2v2 = *l2->v2;
          l2v1.z = l2v2.z = 0;
          if (l2v1 == v || l2v2 == v) list[count++] = l2;
        }
        if (vn == 0) {
          ld->v1linesCount = count;
          ld->v1lines = list;
        } else {
          ld->v2linesCount = count;
          ld->v2lines = list;
        }
      }
    }
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
//  VLevel::LoadVertexes
//
//==========================================================================
void VLevel::LoadVertexes (int Lump, int GLLump, int &NumBaseVerts) {
  int GlFormat = 0;
  if (GLLump >= 0) {
    // read header of the GL vertexes lump and determinte GL vertex format
    char Magic[4];
    W_ReadFromLump(GLLump, Magic, 0, 4);
    GlFormat = !VStr::NCmp((char*)Magic, GL_V2_MAGIC, 4) ? 2 : !VStr::NCmp((char*)Magic, GL_V5_MAGIC, 4) ? 5 : 1;
    if (GlFormat ==  5) LevelFlags |= LF_GLNodesV5;
  }

  // determine number of vertexes: total lump length / vertex record length
  NumBaseVerts = W_LumpLength(Lump)/4;
  int NumGLVerts = GlFormat == 0 ? 0 : GlFormat == 1 ? (W_LumpLength(GLLump)/4) : ((W_LumpLength(GLLump)-4)/8);
  NumVertexes = NumBaseVerts+NumGLVerts;

  // allocate memory for vertexes
  Vertexes = new vertex_t[NumVertexes];
  if (NumVertexes) memset((void *)Vertexes, 0, sizeof(vertex_t)*NumVertexes);

  // load base vertexes
  vertex_t *pDst;
  {
    VStream *lumpstream = W_CreateLumpReaderNum(Lump);
    VCheckedStream Strm(lumpstream);
    pDst = Vertexes;
    for (int i = 0; i < NumBaseVerts; ++i, ++pDst) {
      vint16 x, y;
      Strm << x << y;
      *pDst = TVec(x, y, 0);
    }
  }

  if (GLLump >= 0) {
    // load gl vertexes
    VStream *lumpstream = W_CreateLumpReaderNum(GLLump);
    VCheckedStream Strm(lumpstream);
    if (GlFormat == 1) {
      // gl version 1 vertexes, same as normal ones
      for (int i = 0; i < NumGLVerts; ++i, ++pDst) {
        vint16 x, y;
        Strm << x << y;
        *pDst = TVec(x, y, 0);
      }
    } else {
      // gl version 2 or greater vertexes, as fixed point
      Strm.Seek(4);
      for (int i = 0; i < NumGLVerts; ++i, ++pDst) {
        vint32 x, y;
        Strm << x << y;
        *pDst = TVec(x/65536.0f, y/65536.0f, 0);
      }
    }
  }
}


//==========================================================================
//
//  VLevel::LoadSectors
//
//==========================================================================
void VLevel::LoadSectors (int Lump) {
  // allocate memory for sectors
  NumSectors = W_LumpLength(Lump)/26;
  Sectors = new sector_t[NumSectors];
  memset((void *)Sectors, 0, sizeof(sector_t)*NumSectors);

  // load sectors
  VStream *lumpstream = W_CreateLumpReaderNum(Lump);
  VCheckedStream Strm(lumpstream);
  sector_t *ss = Sectors;
  for (int i = 0; i < NumSectors; ++i, ++ss) {
    // read data
    vint16 floorheight, ceilingheight, lightlevel, special, tag;
    char floorpic[9];
    char ceilingpic[9];
    memset(floorpic, 0, sizeof(floorpic));
    memset(ceilingpic, 0, sizeof(ceilingpic));
    Strm << floorheight << ceilingheight;
    Strm.Serialise(floorpic, 8);
    Strm.Serialise(ceilingpic, 8);
    Strm << lightlevel << special << tag;

    // floor
    ss->floor.Set(TVec(0, 0, 1), floorheight);
    ss->floor.TexZ = floorheight;
    ss->floor.pic = TexNumForName(floorpic, TEXTYPE_Flat);
    ss->floor.xoffs = 0;
    ss->floor.yoffs = 0;
    ss->floor.XScale = 1.0f;
    ss->floor.YScale = 1.0f;
    ss->floor.Angle = 0.0f;
    ss->floor.minz = floorheight;
    ss->floor.maxz = floorheight;
    ss->floor.Alpha = 1.0f;
    ss->floor.MirrorAlpha = 1.0f;
    ss->floor.LightSourceSector = -1;

    // ceiling
    ss->ceiling.Set(TVec(0, 0, -1), -ceilingheight);
    ss->ceiling.TexZ = ceilingheight;
    ss->ceiling.pic = TexNumForName(ceilingpic, TEXTYPE_Flat);
    ss->ceiling.xoffs = 0;
    ss->ceiling.yoffs = 0;
    ss->ceiling.XScale = 1.0f;
    ss->ceiling.YScale = 1.0f;
    ss->ceiling.Angle = 0.0f;
    ss->ceiling.minz = ceilingheight;
    ss->ceiling.maxz = ceilingheight;
    ss->ceiling.Alpha = 1.0f;
    ss->ceiling.MirrorAlpha = 1.0f;
    ss->ceiling.LightSourceSector = -1;

    // params
    ss->params.lightlevel = lightlevel;
    ss->params.LightColour = 0x00ffffff;

    ss->special = special;
    ss->tag = tag;

    ss->seqType = -1; // default seqType
    ss->Gravity = 1.0f;  // default sector gravity of 1.0
    ss->Zone = -1;

    ss->CreateBaseRegion();
  }
  //HashSectors(); //k8: do it later, 'cause map fixer can change loaded map
}


//==========================================================================
//
//  VLevel::CreateSides
//
//==========================================================================
void VLevel::CreateSides () {
  // perform side index and two-sided flag checks and count number of sides needed
  int NumNewSides = 0;
  line_t *Line = Lines;
  for (int i = 0; i < NumLines; ++i, ++Line) {
    if (Line->sidenum[0] == -1) {
      GCon->Logf("Bad WAD: Line %d has no front side", i);
      // let it glitch...
      //Line->sidenum[0] = 0;
    } else {
      if (Line->sidenum[0] < 0 || Line->sidenum[0] >= NumSides) Host_Error("Bad side-def index %d", Line->sidenum[0]);
      ++NumNewSides;
    }

    if (Line->sidenum[1] != -1) {
      // has second side
      if (Line->sidenum[1] < 0 || Line->sidenum[1] >= NumSides) Host_Error("Bad sidedef index %d for linedef #%d", Line->sidenum[1], i);
      // just a warning (and a fix)
      if ((Line->flags&ML_TWOSIDED) == 0) {
        if (loader_force_fix_2s) {
          GCon->Logf(NAME_Warning, "linedef #%d marked as two-sided but has no TWO-SIDED flag set", i);
          Line->flags |= ML_TWOSIDED; //k8: we need to set this, or clipper will glitch
        }
      }
      ++NumNewSides;
    } else {
      // no second side, but marked as two-sided
      if (Line->flags&ML_TWOSIDED) {
        //if (strict_level_errors) Host_Error("Bad WAD: Line %d is marked as TWO-SIDED but has only one side", i);
        GCon->Logf(NAME_Warning, "linedef #%d is marked as TWO-SIDED but has only one side", i);
        Line->flags &= ~ML_TWOSIDED;
      }
    }
    //fprintf(stderr, "linedef #%d: sides=(%d,%d); two-sided=%s\n", i, Line->sidenum[0], Line->sidenum[1], (Line->flags&ML_TWOSIDED ? "tan" : "ona"));
  }

  // allocate memory for side defs
  Sides = new side_t[NumNewSides+1];
  memset((void *)Sides, 0, sizeof(side_t)*(NumNewSides+1));

  for (int f = 0; f < NumNewSides; ++f) {
    Sides[f].Top.ScaleX = Sides[f].Top.ScaleY = 1.0f;
    Sides[f].Bot.ScaleX = Sides[f].Bot.ScaleY = 1.0f;
    Sides[f].Mid.ScaleX = Sides[f].Mid.ScaleY = 1.0f;
  }

  int CurrentSide = 0;
  Line = Lines;
  for (int i = 0; i < NumLines; ++i, ++Line) {
    Sides[CurrentSide].BottomTexture = Line->sidenum[0]; //k8: this is for UDMF
    Sides[CurrentSide].LineNum = i;
    bool skipInit0 = false;
    if (Line->sidenum[0] == -1) {
      // let it glitch...
      Line->sidenum[0] = 0;
      skipInit0 = true;
    } else {
      Line->sidenum[0] = CurrentSide++;
    }
    if (Line->sidenum[1] != -1) {
      Sides[CurrentSide].BottomTexture = Line->sidenum[1]; //k8: this is for UDMF
      Sides[CurrentSide].LineNum = i;
      Line->sidenum[1] = CurrentSide++;
    }

    // assign line specials to sidedefs midtexture and arg1 to toptexture
    if (Line->special == LNSPEC_StaticInit && Line->arg2 != 1) continue;
    if (!skipInit0) {
      Sides[Line->sidenum[0]].MidTexture = Line->special;
      Sides[Line->sidenum[0]].TopTexture = Line->arg1;
    }
  }
  check(CurrentSide == NumNewSides);

  NumSides = NumNewSides;
}


//==========================================================================
//
//  VLevel::LoadSideDefs
//
//==========================================================================
void VLevel::LoadSideDefs (int Lump) {
  NumSides = W_LumpLength(Lump)/30;
  CreateSides();

  // load data
  VStream *lumpstream = W_CreateLumpReaderNum(Lump);
  VCheckedStream Strm(lumpstream);
  side_t *sd = Sides;
  for (int i = 0; i < NumSides; ++i, ++sd) {
    Strm.Seek(sd->BottomTexture*30);
    vint16 textureoffset;
    vint16 rowoffset;
    char toptexture[9];
    char bottomtexture[9];
    char midtexture[9];
    memset(toptexture, 0, sizeof(toptexture));
    memset(bottomtexture, 0, sizeof(bottomtexture));
    memset(midtexture, 0, sizeof(midtexture));
    vint16 sector;
    Strm << textureoffset << rowoffset;
    Strm.Serialise(toptexture, 8);
    Strm.Serialise(bottomtexture, 8);
    Strm.Serialise(midtexture, 8);
    Strm << sector;

    if (sector < 0 || sector >= NumSectors) Host_Error("Bad sector index %d", sector);

    sd->Top.TextureOffset = textureoffset;
    sd->Bot.TextureOffset = textureoffset;
    sd->Mid.TextureOffset = textureoffset;
    sd->Top.RowOffset = rowoffset;
    sd->Bot.RowOffset = rowoffset;
    sd->Mid.RowOffset = rowoffset;
    sd->Sector = &Sectors[sector];

    switch (sd->MidTexture) {
      case LNSPEC_LineTranslucent:
        // in BOOM midtexture can be translucency table lump name
        sd->MidTexture = GTextureManager.CheckNumForName(VName(midtexture, VName::AddLower8), TEXTYPE_Wall, true);
        if (sd->MidTexture == -1) sd->MidTexture = 0;
        sd->TopTexture = TexNumForName(toptexture, TEXTYPE_Wall);
        sd->BottomTexture = TexNumForName(bottomtexture, TEXTYPE_Wall);
        break;

      case LNSPEC_TransferHeights:
        sd->MidTexture = TexNumForName(midtexture, TEXTYPE_Wall, true);
        sd->TopTexture = TexNumForName(toptexture, TEXTYPE_Wall, true);
        sd->BottomTexture = TexNumForName(bottomtexture, TEXTYPE_Wall, true);
        break;

      case LNSPEC_StaticInit:
        {
          bool HaveCol;
          bool HaveFade;
          vuint32 Col;
          vuint32 Fade;
          sd->MidTexture = TexNumForName(midtexture, TEXTYPE_Wall);
          int TmpTop = TexNumOrColour(toptexture, TEXTYPE_Wall, HaveCol, Col);
          sd->BottomTexture = TexNumOrColour(bottomtexture, TEXTYPE_Wall, HaveFade, Fade);
          if (HaveCol || HaveFade) {
            for (int j = 0; j < NumSectors; ++j) {
              if (Sectors[j].tag == sd->TopTexture) {
                if (HaveCol) Sectors[j].params.LightColour = Col;
                if (HaveFade) Sectors[j].params.Fade = Fade;
              }
            }
          }
          sd->TopTexture = TmpTop;
        }
        break;

      default:
        sd->MidTexture = TexNumForName(midtexture, TEXTYPE_Wall);
        sd->TopTexture = TexNumForName(toptexture, TEXTYPE_Wall);
        sd->BottomTexture = TexNumForName(bottomtexture, TEXTYPE_Wall);
        break;
      }
  }
}


//==========================================================================
//
//  VLevel::LoadLineDefs1
//
//  For Doom and Heretic
//
//==========================================================================
void VLevel::LoadLineDefs1 (int Lump, int NumBaseVerts, const mapInfo_t &MInfo) {
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
    ld->sidenum[0] = side0 == 0xffff ? -1 : side0;
    ld->sidenum[1] = side1 == 0xffff ? -1 : side1;

    ld->alpha = 1.0f;
    ld->LineTag = -1;

    if (MInfo.Flags&MAPINFOF_ClipMidTex) ld->flags |= ML_CLIP_MIDTEX;
    if (MInfo.Flags&MAPINFOF_WrapMidTex) ld->flags |= ML_WRAP_MIDTEX;
  }
}


//==========================================================================
//
//  VLevel::LoadLineDefs2
//
//  Hexen format
//
//==========================================================================
void VLevel::LoadLineDefs2 (int Lump, int NumBaseVerts, const mapInfo_t &MInfo) {
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
    ld->LineTag = -1;

    if (MInfo.Flags&MAPINFOF_ClipMidTex) ld->flags |= ML_CLIP_MIDTEX;
    if (MInfo.Flags&MAPINFOF_WrapMidTex) ld->flags |= ML_WRAP_MIDTEX;
  }
}


//==========================================================================
//
//  VLevel::FinaliseLines
//
//==========================================================================
void VLevel::FinaliseLines () {
  line_t *Line = Lines;
  for (int i = 0; i < NumLines; ++i, ++Line) {
    // calculate line's plane, slopetype, etc
    CalcLine(Line);
    // set up sector references
    Line->frontsector = Sides[Line->sidenum[0]].Sector;
    if (Line->sidenum[1] != -1) {
      Line->backsector = Sides[Line->sidenum[1]].Sector;
    } else {
      Line->backsector = nullptr;
    }
  }
}


//==========================================================================
//
//  VLevel::LoadGLSegs
//
//==========================================================================
void VLevel::LoadGLSegs (int Lump, int NumBaseVerts) {
  vertex_t *GLVertexes = Vertexes+NumBaseVerts;
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
  seg_t *li = Segs;
  for (int i = 0; i < NumSegs; ++i, ++li) {
    vuint32 v1num;
    vuint32 v2num;
    vint16 linedef; // -1 for minisegs
    vint16 side;
    vint16 partner; // -1 on one-sided walls

    if (Format < 3) {
      vuint16 v1, v2;
      Strm << v1 << v2 << linedef << side << partner;
      v1num = v1;
      v2num = v2;
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
      if (v1num >= (vuint32)NumGLVertexes) Host_Error("Bad GL vertex index %d", v1num);
      li->v1 = &GLVertexes[v1num];
    } else {
      if (v1num >= (vuint32)NumVertexes) Host_Error("Bad vertex index %d (04)", v1num);
      li->v1 = &Vertexes[v1num];
    }
    if (v2num&GLVertFlag) {
      v2num ^= GLVertFlag;
      if (v2num >= (vuint32)NumGLVertexes) Host_Error("Bad GL vertex index %d", v2num);
      li->v2 = &GLVertexes[v2num];
    } else {
      if (v2num >= (vuint32)NumVertexes) Host_Error("Bad vertex index %d (05)", v2num);
      li->v2 = &Vertexes[v2num];
    }

    if (linedef >= 0) {
      line_t *ldef = &Lines[linedef];
      li->linedef = ldef;
      li->sidedef = &Sides[ldef->sidenum[side]];
      li->frontsector = Sides[ldef->sidenum[side]].Sector;

      //if (ldef->flags&ML_TWOSIDED) li->backsector = Sides[ldef->sidenum[side^1]].Sector;
      if (/*(ldef->flags&ML_TWOSIDED) != 0 &&*/ ldef->sidenum[side^1] >= 0) {
        li->backsector = Sides[ldef->sidenum[side^1]].Sector;
      } else {
        li->backsector = nullptr;
        ldef->flags &= ~ML_TWOSIDED;
      }

      if (side) {
        li->offset = li->v1->DistanceTo2D(*ldef->v2);
      } else {
        li->offset = li->v1->DistanceTo2D(*ldef->v1);
      }
      li->length = li->v2->DistanceTo2D(*li->v1);
      if (li->length < 0.001f) Sys_Error("zero-length seg #%d", i);
      li->side = side;
    }

    // assign partner (we need it for self-referencing deep water)
    li->partner = (partner >= 0 && partner < NumSegs ? &Segs[partner] : nullptr);

    // calc seg's plane params
    CalcSeg(li);
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
  seg_t *seg = &Segs[0];
  for (int i = 0; i < NumSegs; ++i, ++seg) {
    int dside = seg->side;
    if (dside != 0 && dside != 1) Sys_Error("invalid seg #%d side (%d)", i, dside);

    if (seg->linedef) {
      line_t *ldef = seg->linedef;

      if (ldef->sidenum[dside] < 0 || ldef->sidenum[dside] >= NumSides) {
        Sys_Error("seg #%d, ldef=%d: invalid sidenum %d (%d) (max sidenum is %d)\n", i, (int)(ptrdiff_t)(ldef-Lines), dside, ldef->sidenum[dside], NumSides-1);
      }

      seg->sidedef = &Sides[ldef->sidenum[dside]];
      seg->frontsector = Sides[ldef->sidenum[dside]].Sector;

      if (ldef->flags&ML_TWOSIDED) {
        if (ldef->sidenum[dside^1] < 0 || ldef->sidenum[dside^1] >= NumSides) Sys_Error("another side of two-sided linedef is fucked");
        seg->backsector = Sides[ldef->sidenum[dside^1]].Sector;
      } else if (ldef->sidenum[dside^1] >= 0) {
        if (ldef->sidenum[dside^1] >= NumSides) Sys_Error("another side of blocking two-sided linedef is fucked");
        seg->backsector = Sides[ldef->sidenum[dside^1]].Sector;
        // not a two-sided, so clear backsector (just in case) -- nope
        //destseg->backsector = nullptr;
      } else {
        seg->backsector = nullptr;
        ldef->flags &= ~ML_TWOSIDED; // just in case
      }

      if (dside) {
        seg->offset = seg->v1->DistanceTo2D(*ldef->v2);
      } else {
        seg->offset = seg->v1->DistanceTo2D(*ldef->v1);
      }
    }

    seg->length = seg->v2->DistanceTo2D(*seg->v1);
    if (seg->length < 0.0001f) {
      GCon->Logf(NAME_Warning, "ZERO-LENGTH %sseg #%d (flags: 0x%04x)", (seg->linedef ? "" : "mini"), i, (unsigned)seg->flags);
      GCon->Logf(NAME_Warning, "  verts: (%g,%g,%g)-(%g,%g,%g)", seg->v1->x, seg->v1->y, seg->v1->z, seg->v2->x, seg->v2->y, seg->v2->z);
      GCon->Logf(NAME_Warning, "  offset: %g", seg->offset);
      GCon->Logf(NAME_Warning, "  length: %g", seg->length);
      if (seg->linedef) {
        GCon->Logf(NAME_Warning, "  linedef: %d", (int)(ptrdiff_t)(seg->linedef-Lines));
        GCon->Logf(NAME_Warning, "  sidedef: %d (side #%d)", (int)(ptrdiff_t)(seg->sidedef-Sides), seg->side);
        GCon->Logf(NAME_Warning, "  front sector: %d", (int)(ptrdiff_t)(seg->frontsector-Sectors));
        GCon->Logf(NAME_Warning, "  back sector: %d", (int)(ptrdiff_t)(seg->backsector-Sectors));
      }
      if (seg->partner) GCon->Logf(NAME_Warning, "  partner: %d", (int)(ptrdiff_t)(seg->partner-Segs));
      if (seg->front_sub) GCon->Logf(NAME_Warning, "  frontsub: %d", (int)(ptrdiff_t)(seg->front_sub-Subsectors));
      //Sys_Error("zero-length seg #%d", i);
      if (seg->partner) {
        if (seg->partner->partner) {
          check(seg->partner->partner == seg);
          seg->partner->partner = nullptr;
        }
        seg->partner = nullptr;
      }
      seg->offset = 0.0f;
      seg->length = 0.0001f;
      // setup fake seg's plane params
      seg->normal = TVec(1.0f, 0.0f, 0.0f);
      seg->dist = 0.0f;
      seg->dir = TVec(1.0f, 0.0f, 0.0f); // arbitrary
    } else {
      // calc seg's plane params
      CalcSeg(seg);
    }
  }
}


//==========================================================================
//
//  VLevel::PostLoadSubsectors
//
//==========================================================================
void VLevel::PostLoadSubsectors () {
  subsector_t *ss = Subsectors;
  for (int i = 0; i < NumSubsectors; ++i, ++ss) {
    if (ss->firstline < 0 || ss->firstline >= NumSegs) Host_Error("Bad seg index %d", ss->firstline);
    if (ss->numlines <= 0 || ss->firstline+ss->numlines > NumSegs) Host_Error("Bad segs range %d %d", ss->firstline, ss->numlines);

    // look up sector number for each subsector
    seg_t *seg = &Segs[ss->firstline];
    for (int j = 0; j < ss->numlines; ++j) {
      if (seg[j].linedef) {
        ss->sector = seg[j].sidedef->Sector;
        ss->seclink = ss->sector->subsectors;
        ss->sector->subsectors = ss;
        break;
      }
    }
    // calculate bounding box
    ss->bbox[0] = ss->bbox[1] = 999999.0f;
    ss->bbox[2] = ss->bbox[3] = -999999.0f;
    for (int j = 0; j < ss->numlines; j++) {
      seg[j].front_sub = ss;
      // min
      ss->bbox[0] = min2(ss->bbox[0], seg[j].v1->x);
      ss->bbox[0] = min2(ss->bbox[0], seg[j].v2->x);
      ss->bbox[1] = min2(ss->bbox[1], seg[j].v1->y);
      ss->bbox[1] = min2(ss->bbox[1], seg[j].v2->y);
      // max
      ss->bbox[2] = max2(ss->bbox[2], seg[j].v1->x);
      ss->bbox[2] = max2(ss->bbox[2], seg[j].v2->x);
      ss->bbox[3] = max2(ss->bbox[3], seg[j].v1->y);
      ss->bbox[3] = max2(ss->bbox[3], seg[j].v2->y);
    }
    if (!ss->sector) Host_Error("Subsector %d without sector", i);
  }
  for (int f = 0; f < NumSegs; ++f) {
    if (!Segs[f].front_sub) GCon->Logf("Seg %d: front_sub is not set!", f);
  }
}


//==========================================================================
//
//  VLevel::LoadSubsectors
//
//==========================================================================
void VLevel::LoadSubsectors (int Lump) {
  // determine format of the subsectors data
  int Format;
  if (LevelFlags&LF_GLNodesV5) {
    Format = 5;
    NumSubsectors = W_LumpLength(Lump)/8;
  } else {
    char Header[4];
    W_ReadFromLump(Lump, Header, 0, 4);
    if (memcmp(Header, GL_V3_MAGIC, 4) == 0) {
      Format = 3;
      NumSubsectors = (W_LumpLength(Lump)-4)/8;
    } else {
      Format = 1;
      NumSubsectors = W_LumpLength(Lump)/4;
    }
  }

  // allocate memory for subsectors
  Subsectors = new subsector_t[NumSubsectors];
  memset((void *)Subsectors, 0, sizeof(subsector_t)*NumSubsectors);

  // read data
  VStream *lumpstream = W_CreateLumpReaderNum(Lump);
  VCheckedStream Strm(lumpstream);
  if (Format == 3) Strm.Seek(4);
  subsector_t *ss = Subsectors;
  for (int i = 0; i < NumSubsectors; ++i, ++ss) {
    if (Format < 3) {
      vuint16 numsegs, firstseg;
      Strm << numsegs << firstseg;
      ss->numlines = numsegs;
      ss->firstline = firstseg;
    } else {
      vint32 numsegs, firstseg;
      Strm << numsegs << firstseg;
      ss->numlines = numsegs;
      ss->firstline = firstseg;
    }
  }
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

    for (int j = 0; j < 2; ++j) {
      no->children[j] = children[j];
      no->bbox[j][0] = bbox[j][BOXLEFT];
      no->bbox[j][1] = bbox[j][BOXBOTTOM];
      no->bbox[j][2] = -32768.0f;
      no->bbox[j][3] = bbox[j][BOXRIGHT];
      no->bbox[j][4] = bbox[j][BOXTOP];
      no->bbox[j][5] = 32768.0f;
    }
  }
}


//==========================================================================
//
//  VLevel::LoadPVS
//
//==========================================================================
void VLevel::LoadPVS (int Lump) {
  if (W_LumpName(Lump) != NAME_gl_pvs || W_LumpLength(Lump) == 0) {
    GCon->Logf(NAME_Dev, "Empty or missing PVS lump");
    if (NoVis == nullptr && VisData == nullptr) BuildPVS();
    /*
    VisData = nullptr;
    NoVis = new vuint8[(NumSubsectors + 7) / 8];
    memset(NoVis, 0xff, (NumSubsectors + 7) / 8);
    */
  } else {
    //if (NoVis == nullptr && VisData == nullptr) BuildPVS();
    vuint8 *VisDataNew = new vuint8[W_LumpLength(Lump)];
    VStream *lumpstream = W_CreateLumpReaderNum(Lump);
    VCheckedStream Strm(lumpstream);
    Strm.Serialise(VisDataNew, W_LumpLength(Lump));
    if (Strm.IsError() || W_LumpLength(Lump) < ((NumSubsectors+7)>>3)*NumSubsectors) {
      delete [] VisDataNew;
    } else {
      delete [] VisData;
      delete [] NoVis;
      VisData = VisDataNew;
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
    GCon->Logf(NAME_Warning, "error reading GL nodes (VaVoom will use internal node builder)");
    return false;
  }

  if ((hdr[0] == 'Z' || hdr[0] == 'X') &&
      hdr[1] == 'G' && hdr[2] == 'L' &&
      (hdr[3] == 'N' || hdr[3] == '2' || hdr[3] == '3'))
  {
    // ok
  } else {
    delete BaseStrm;
    GCon->Logf(NAME_Warning, "invalid GL nodes signature (VaVoom will use internal node builder)");
    return false;
  }

  // create reader stream for the zipped data
  //vuint8 *TmpData = new vuint8[BaseStrm->TotalSize()-4];
  vuint8 *TmpData = (vuint8 *)Z_Calloc(BaseStrm->TotalSize()-4);
  BaseStrm->Serialise(TmpData, BaseStrm->TotalSize()-4);
  if (BaseStrm->IsError()) {
    delete BaseStrm;
    GCon->Logf(NAME_Warning, "error reading GL nodes (VaVoom will use internal node builder)");
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
    Strm = new VZipStreamReader(DataStrm);
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
      GCon->Logf(NAME_Warning, "this obsolete version of GL nodes is disabled (VaVoom will use internal node builder)");
      return false;
  }

  GCon->Logf("NOTE: found %scompressed GL nodes, type %d", (hdr[0] == 'X' ? "un" : ""), type);

  if (!nodes_allow_compressed) {
    delete Strm;
    delete DataStrm;
    GCon->Logf(NAME_Warning, "this new version of GL nodes is disabled (VaVoom will use internal node builder)");
    return false;
  }

  // read extra vertex data
  {
    vuint32 OrgVerts, NewVerts;
    *Strm << OrgVerts << NewVerts;

    if (Strm->IsError()) {
      delete Strm;
      delete DataStrm;
      GCon->Logf(NAME_Warning, "error reading GL nodes (VaVoom will use internal node builder)");
      return false;
    }

    if (OrgVerts != (vuint32)NumVertexes) {
      delete Strm;
      delete DataStrm;
      GCon->Logf(NAME_Warning, "error reading GL nodes (got %u vertexes, expected %d vertexes)", OrgVerts, NumVertexes);
      return false;
    }

    if (OrgVerts+NewVerts != (vuint32)NumVertexes) {
      vertex_t *OldVerts = Vertexes;
      NumVertexes = OrgVerts+NewVerts;
      Vertexes = new vertex_t[NumVertexes];
      if (NumVertexes) memset((void *)Vertexes, 0, sizeof(vertex_t)*NumVertexes);
      if (OldVerts) memcpy((void *)Vertexes, (void *)OldVerts, OrgVerts*sizeof(vertex_t));
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
    vertex_t *DstVert = Vertexes+OrgVerts;
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

        seg_t *li = Segs+Subsectors[i].firstline+j;

        // assign partner (we need it for self-referencing deep water)
        li->partner = (partner < (unsigned)NumSegs ? &Segs[partner] : nullptr);

        li->v1 = &Vertexes[v1];
        // v2 will be set later

        if (linedef != 0xffffffffu) {
          if (linedef >= (vuint32)NumLines) Host_Error("Bad linedef index %u (ss=%d; nl=%d)", linedef, i, j);
          if (side > 1) Host_Error("Bad seg side %d", side);

          line_t *ldef = &Lines[linedef];

          li->linedef = ldef;
          /*
          li->sidedef = &Sides[ldef->sidenum[side]];
          li->frontsector = Sides[ldef->sidenum[side]].Sector;

          if (/ *(ldef->flags&ML_TWOSIDED) != 0 &&* / ldef->sidenum[side^1] >= 0) {
            li->backsector = Sides[ldef->sidenum[side^1]].Sector;
          } else {
            li->backsector = nullptr;
            ldef->flags &= ~ML_TWOSIDED;
          }

          if (side) {
            check(li);
            check(li->v1);
            check(ldef->v2);
            li->offset = Length(*li->v1 - *ldef->v2);
          } else {
            check(li);
            check(li->v1);
            check(ldef->v1);
            li->offset = Length(*li->v1 - *ldef->v1);
          }
          li->side = side;
          */
        } else {
          li->linedef = nullptr;
          li->sidedef = nullptr;
          li->frontsector = li->backsector = (Segs+Subsectors[i].firstline)->frontsector;
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
      } else {
        vint32 x, y, dx, dy;
        *Strm << x << y << dx << dy;
        if (dx == 0 && dy == 0) {
          GCon->Log("invalid BSP node (zero direction)");
          no->SetPointDirXY(TVec(x/65536.0f, y/65536.0f, 0), TVec(0.001f, 0, 0));
        } else {
          no->SetPointDirXY(TVec(x/65536.0f, y/65536.0f, 0), TVec(dx/65536.0f, dy/65536.0f, 0));
        }
      }

      *Strm << bbox[0][0] << bbox[0][1] << bbox[0][2] << bbox[0][3]
            << bbox[1][0] << bbox[1][1] << bbox[1][2] << bbox[1][3]
            << children[0] << children[1];

      for (int j = 0; j < 2; ++j) {
        no->children[j] = children[j];
        no->bbox[j][0] = bbox[j][BOXLEFT];
        no->bbox[j][1] = bbox[j][BOXBOTTOM];
        no->bbox[j][2] = -32768.0f;
        no->bbox[j][3] = bbox[j][BOXRIGHT];
        no->bbox[j][4] = bbox[j][BOXTOP];
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

  /*
  {
    seg_t *li = Segs;
    for (int i = 0; i < NumSegs; ++i, ++li) {
      // calc seg's plane params
      li->length = Length(*li->v2 - *li->v1);
      CalcSeg(li);
    }
  }

  {
    subsector_t *ss = Subsectors;
    for (int i = 0; i < NumSubsectors; ++i, ++ss) {
      // look up sector number for each subsector
      seg_t *seg = &Segs[ss->firstline];
      for (int j = 0; j < ss->numlines; ++j) {
        if (seg[j].linedef) {
          ss->sector = seg[j].sidedef->Sector;
          ss->seclink = ss->sector->subsectors;
          ss->sector->subsectors = ss;
          break;
        }
      }
      for (int j = 0; j < ss->numlines; j++) seg[j].front_sub = ss;
      if (!ss->sector) Host_Error("Subsector %d without sector", i);
    }
  }
  */

  // create dummy VIS data
  // k8: no need to do this, main loader will take care of it

  bool wasError = Strm->IsError();

  delete Strm;
  delete DataStrm;

  if (wasError) Host_Error("error reading GL Nodes (turn on forced node rebuilder in options to load this map)");

  return true;
}


//==========================================================================
//
//  VLevel::LoadBlockMap
//
//==========================================================================
void VLevel::LoadBlockMap (int Lump) {
  VStream *Strm = nullptr;

  if (build_blockmap) {
    Lump = -1;
  } else {
    if (Lump >= 0 && !build_blockmap) Strm = W_CreateLumpReaderNum(Lump);
  }

  if (!Strm || Strm->TotalSize() == 0 || Strm->TotalSize()/2 >= 0x10000) {
    delete Strm;
    GCon->Logf("Creating BLOCKMAP");
    CreateBlockMap();
  } else {
    // killough 3/1/98: Expand wad blockmap into larger internal one,
    // by treating all offsets except -1 as unsigned and zero-extending
    // them. This potentially doubles the size of blockmaps allowed,
    // because Doom originally considered the offsets as always signed.

    // allocate memory for blockmap
    int count = Strm->TotalSize()/2;
    BlockMapLump = new vint32[count];
    BlockMapLumpSize = count;

    // read data
    BlockMapLump[0] = Streamer<vint16>(*Strm);
    BlockMapLump[1] = Streamer<vint16>(*Strm);
    BlockMapLump[2] = Streamer<vuint16>(*Strm);
    BlockMapLump[3] = Streamer<vuint16>(*Strm);
    for (int i = 4; i < count; i++) {
      vint16 Tmp;
      *Strm << Tmp;
      BlockMapLump[i] = Tmp == -1 ? -1 : (vuint16)Tmp&0xffff;
    }

    bool wasError = Strm->IsError();
    delete Strm;

    if (wasError) {
      GCon->Logf(NAME_Warning, "error loading BLOCKMAP, it will be rebuilt");
      delete BlockMapLump;
      BlockMapLump = nullptr;
      BlockMapLumpSize = 0;
      CreateBlockMap();
    }
  }

  // read blockmap origin and size
  /*
  BlockMapOrgX = BlockMapLump[0];
  BlockMapOrgY = BlockMapLump[1];
  BlockMapWidth = BlockMapLump[2];
  BlockMapHeight = BlockMapLump[3];
  BlockMap = BlockMapLump + 4;

  // clear out mobj chains
  int Count = BlockMapWidth * BlockMapHeight;
  BlockLinks = new VEntity*[Count];
  memset(BlockLinks, 0, sizeof(VEntity*) * Count);
  */
}


//==========================================================================
//
//  VLevel::CreateBlockMap
//
//==========================================================================
void VLevel::CreateBlockMap () {
  // determine bounds of the map
  float MinX = Vertexes[0].x;
  float MaxX = MinX;
  float MinY = Vertexes[0].y;
  float MaxY = MinY;
  for (int i = 0; i < NumVertexes; ++i) {
    if (MinX > Vertexes[i].x) MinX = Vertexes[i].x;
    if (MaxX < Vertexes[i].x) MaxX = Vertexes[i].x;
    if (MinY > Vertexes[i].y) MinY = Vertexes[i].y;
    if (MaxY < Vertexes[i].y) MaxY = Vertexes[i].y;
  }

  // they should be integers, but just in case round them
  MinX = floor(MinX);
  MinY = floor(MinY);
  MaxX = ceil(MaxX);
  MaxY = ceil(MaxY);

  int Width = MapBlock(MaxX-MinX)+1;
  int Height = MapBlock(MaxY-MinY)+1;

  // add all lines to their corresponding blocks
  // but skip zero-length lines
  TArray<vuint16> *BlockLines = new TArray<vuint16>[Width*Height];
  for (int i = 0; i < NumLines; ++i) {
    // determine starting and ending blocks
    line_t &Line = Lines[i];

    float ssq = Length2DSquared(*Line.v2 - *Line.v1);
    if (ssq < 1.0f) continue;
    ssq = Length2D(*Line.v2 - *Line.v1);
    if (ssq < 1.0f) continue;

    int X1 = MapBlock(Line.v1->x-MinX);
    int Y1 = MapBlock(Line.v1->y-MinY);
    int X2 = MapBlock(Line.v2->x-MinX);
    int Y2 = MapBlock(Line.v2->y-MinY);

    if (X1 > X2) {
      int Tmp = X2;
      X2 = X1;
      X1 = Tmp;
    }
    if (Y1 > Y2) {
      int Tmp = Y2;
      Y2 = Y1;
      Y1 = Tmp;
    }

    if (X1 == X2 && Y1 == Y2) {
      // line is inside a single block
      BlockLines[X1+Y1*Width].Append(i);
    } else if (Y1 == Y2) {
      // horisontal line of blocks
      for (int x = X1; x <= X2; x++) {
        BlockLines[x+Y1*Width].Append(i);
      }
    } else if (X1 == X2) {
      // vertical line of blocks
      for (int y = Y1; y <= Y2; y++) {
        BlockLines[X1+y*Width].Append(i);
      }
    }
    else {
      // diagonal line
      for (int x = X1; x <= X2; ++x) {
        for (int y = Y1; y <= Y2; ++y) {
          // check if line crosses the block
          if (Line.slopetype == ST_POSITIVE) {
            int p1 = Line.PointOnSide(TVec(MinX+x*128, MinY+(y+1)*128, 0));
            int p2 = Line.PointOnSide(TVec(MinX+(x+1)*128, MinY+y*128, 0));
            if (p1 == p2) continue;
          } else {
            int p1 = Line.PointOnSide(TVec(MinX+x*128, MinY+y*128, 0));
            int p2 = Line.PointOnSide(TVec(MinX+(x+1)*128, MinY+(y+1)*128, 0));
            if (p1 == p2) continue;
          }
          BlockLines[x+y*Width].Append(i);
        }
      }
    }
  }

  // build blockmap lump
  TArray<vint32> BMap;
  BMap.SetNum(4+Width*Height);
  BMap[0] = (int)MinX;
  BMap[1] = (int)MinY;
  BMap[2] = Width;
  BMap[3] = Height;
  for (int i = 0; i < Width*Height; ++i) {
    // write offset
    BMap[i+4] = BMap.Num();
    TArray<vuint16> &Block = BlockLines[i];
    // add dummy start marker
    BMap.Append(0);
    // add lines in this block
    for (int j = 0; j < Block.Num(); ++j) {
      BMap.Append(Block[j]);
    }
    // add terminator marker
    BMap.Append(-1);
  }

  // copy data
  BlockMapLump = new vint32[BMap.Num()];
  BlockMapLumpSize = BMap.Num();
  memcpy(BlockMapLump, BMap.Ptr(), BMap.Num()*sizeof(vint32));

  delete[] BlockLines;
  BlockLines = nullptr;
}


//==========================================================================
//
//  VLevel::LoadReject
//
//==========================================================================
void VLevel::LoadReject (int Lump) {
  if (Lump < 0) return;
  VStream *lumpstream = W_CreateLumpReaderNum(Lump);
  VCheckedStream Strm(lumpstream);
  // check for empty reject lump
  if (Strm.TotalSize()) {
    // check if reject lump is required bytes long
    int NeededSize = (NumSectors*NumSectors+7)/8;
    if (Strm.TotalSize() < NeededSize) {
      GCon->Logf("Reject data is %d bytes too short", NeededSize-Strm.TotalSize());
    } else {
      // read it
      RejectMatrixSize = Strm.TotalSize();
      RejectMatrix = new vuint8[RejectMatrixSize];
      Strm.Serialise(RejectMatrix, RejectMatrixSize);

      // check if it's an all-zeroes lump, in which case it's useless and can be discarded
      bool Blank = true;
      for (int i = 0; i < NeededSize; ++i) {
        if (RejectMatrix[i]) {
          Blank = false;
          break;
        }
      }
      if (Blank) {
        delete[] RejectMatrix;
        RejectMatrix = nullptr;
        RejectMatrixSize = 0;
      }
    }
  }
}


//==========================================================================
//
//  VLevel::LoadThings1
//
//==========================================================================
void VLevel::LoadThings1 (int Lump) {
  NumThings = W_LumpLength(Lump)/10;
  Things = new mthing_t[NumThings];
  memset((void *)Things, 0, sizeof(mthing_t)*NumThings);

  VStream *lumpstream = W_CreateLumpReaderNum(Lump);
  VCheckedStream Strm(lumpstream);
  mthing_t *th = Things;
  for (int i = 0; i < NumThings; ++i, ++th) {
    vint16 x, y, angle, type, options;
    Strm << x << y << angle << type << options;

    th->x = x;
    th->y = y;
    th->angle = angle;
    th->type = type;
    th->options = options&~7;
    th->SkillClassFilter = 0xffff0000;
    if (options&1) th->SkillClassFilter |= 0x03;
    if (options&2) th->SkillClassFilter |= 0x04;
    if (options&4) th->SkillClassFilter |= 0x18;
  }
}


//==========================================================================
//
//  VLevel::LoadThings2
//
//==========================================================================
void VLevel::LoadThings2 (int Lump) {
  NumThings = W_LumpLength(Lump)/20;
  Things = new mthing_t[NumThings];
  memset((void *)Things, 0, sizeof(mthing_t)*NumThings);

  VStream *lumpstream = W_CreateLumpReaderNum(Lump);
  VCheckedStream Strm(lumpstream);
  mthing_t *th = Things;
  for (int i = 0; i < NumThings; ++i, ++th) {
    vint16 tid, x, y, height, angle, type, options;
    vuint8 special, arg1, arg2, arg3, arg4, arg5;
    Strm << tid << x << y << height << angle << type << options
      << special << arg1 << arg2 << arg3 << arg4 << arg5;

    th->tid = tid;
    th->x = x;
    th->y = y;
    th->height = height;
    th->angle = angle;
    th->type = type;
    th->options = options&~0xe7;
    th->SkillClassFilter = (options&0xe0)<<11;
    if (options&1) th->SkillClassFilter |= 0x03;
    if (options&2) th->SkillClassFilter |= 0x04;
    if (options&4) th->SkillClassFilter |= 0x18;
    th->special = special;
    th->arg1 = arg1;
    th->arg2 = arg2;
    th->arg3 = arg3;
    th->arg4 = arg4;
    th->arg5 = arg5;
  }
}


//==========================================================================
//
//  VLevel::LoadACScripts
//
//  load libraries from 'loadacs'
//
//==========================================================================
void VLevel::LoadLoadACS (int lacsLump, int XMapLump) {
  if (lacsLump < 0) return;
  GCon->Logf("Loading ACS libraries from '%s'", *W_FullLumpName(lacsLump));
  VScriptParser *sc = new VScriptParser(W_FullLumpName(lacsLump), W_CreateLumpReaderNum(lacsLump));
  while (!sc->AtEnd()) {
    //sc->ExpectName8();
    //int AcsLump = W_CheckNumForName(sc->Name8, WADNS_ACSLibrary);
    //sc->ExpectName();
    sc->ExpectString();
    int AcsLump = W_FindACSObjectInFile(sc->String, W_LumpFile(lacsLump));
    /*
    int AcsLump = W_CheckNumForNameInFile(sc->Name, W_LumpFile(lacsLump), WADNS_ACSLibrary);
    if (AcsLump < 0 && VStr::length(*sc->Name) > 8) {
      VName n8 = VName(*sc->Name, VName::AddLower8);
      AcsLump = W_CheckNumForNameInFile(n8, W_LumpFile(lacsLump), WADNS_ACSLibrary);
      if (AcsLump >= 0) GCon->Logf(NAME_Dev, "ACS: '%s' found as '%s'", *sc->Name, *n8);
    }
    */
    if (AcsLump >= 0) {
      //GCon->Logf(NAME_Dev, "ACS: loading script from '%s'", *W_FullLumpName(AcsLump));
      GCon->Logf("  loading ACS script from '%s'", *W_FullLumpName(AcsLump));
      Acs->LoadObject(AcsLump);
    } else {
      GCon->Logf(NAME_Warning, "ACS script '%s' not found", *sc->String);
    }
  }
  delete sc;
}


//==========================================================================
//
//  VLevel::LoadACScripts
//
//==========================================================================
void VLevel::LoadACScripts (int Lump, int XMapLump) {
  Acs = new VAcsLevel(this);

  GCon->Logf(NAME_Dev, "ACS: BEHAVIOR lump: %d", Lump);

  // load level's BEHAVIOR lump if it has one
  if (Lump >= 0 && W_LumpLength(Lump) > 0) Acs->LoadObject(Lump);

  // load ACS helper scripts if needed (for Strife)
  if (GGameInfo->AcsHelper != NAME_None) {
    GCon->Logf(NAME_Dev, "ACS: looking for helper script from '%s'", *GGameInfo->AcsHelper);
    int hlump = W_GetNumForName(GGameInfo->AcsHelper, WADNS_ACSLibrary);
    if (hlump >= 0) {
      GCon->Logf(NAME_Dev, "ACS: loading helper script from '%s'", *W_FullLumpName(hlump));
      Acs->LoadObject(hlump);
    }
  }

  // load user-specified default ACS libraries
  // first load all from map file and further, then all before map file
  // this is done so autoloaded acs won't interfere with libraries
  if (XMapLump >= 0) {
    // from map file and further
    for (int ScLump = W_IterateNS(W_StartIterationFromLumpFile(W_LumpFile(XMapLump)), WADNS_Global); ScLump >= 0; ScLump = W_IterateNS(ScLump, WADNS_Global)) {
      if (W_LumpName(ScLump) != NAME_loadacs) continue;
      LoadLoadACS(ScLump, XMapLump);
    }
  }
  // before map file
  for (int ScLump = W_IterateNS(-1, WADNS_Global); ScLump >= 0; ScLump = W_IterateNS(ScLump, WADNS_Global)) {
    if (XMapLump >= 0 && W_LumpFile(ScLump) >= W_LumpFile(XMapLump)) break;
    if (W_LumpName(ScLump) != NAME_loadacs) continue;
    LoadLoadACS(ScLump, XMapLump);
  }
}


//==========================================================================
//
//  texForceLoad
//
//==========================================================================
static int texForceLoad (const char *name, int Type, bool CMap, bool allowForceLoad) {
  if (!name || !name[0]) return (CMap ? 0 : GTextureManager.DefaultTexture); // just in case
  if (name[0] == '-' && !name[1]) return 0; // just in case
  int i = -1;

  //GCon->Logf("texForceLoad(*): <%s>", name);

  VName loname = NAME_None;
  // try filename if slash is found
  const char *slash = strchr(name, '/');
  const char *dot = nullptr;
  for (int f = 0; name[f]; ++f) if (name[f] == '.') dot = name+f;
  if (slash && slash[1]) {
    loname = VName(slash+1, VName::AddLower);
    //GCon->Logf("texForceLoad(**): <%s>", *loname);
    //i = GTextureManager.AddFileTextureChecked(loname, Type);
  } else if (!dot) {
    loname = VName(name, VName::AddLower);
    //GCon->Logf("texForceLoad(***): <%s>", *loname);
    //i = GTextureManager.AddFileTextureChecked(loname, Type);
  } else if (dot) {
    loname = VName(dot+1, VName::AddLower);
  }

  if (loname != NAME_None) {
    i = GTextureManager.CheckNumForName(loname, Type, true);
    if (i >= 0) return i;
    if (CMap) return 0;
    if (!allowForceLoad) return GTextureManager.DefaultTexture;
    //i = GTextureManager.AddFileTextureChecked(loname, Type);
    //if (i != -1) GCon->Logf("texForceLoad(0): <%s><%s> (%d)", *loname, name, i);
  }

  if (i == -1) {
    VName loname8((dot ? dot+1 : slash ? slash+1 : name), VName::AddLower8);
    i = GTextureManager.CheckNumForName(loname8, Type, true);
    //if (i != -1) GCon->Logf("texForceLoad(1): <%s><%s> (%d)", *loname8, name, i);
    if (i == -1 && CMap) return 0;
  }

  //if (i == -1) i = GTextureManager.CheckNumForName(VName(name, VName::AddLower), Type, true, true);
  //if (i == -1 && VStr::length(name) > 8) i = GTextureManager.AddFileTexture(VName(name, VName::AddLower), Type);

  if (i == -1 /*&& !slash*/ && allowForceLoad) {
    //GCon->Logf("texForceLoad(x): <%s>", name);
    /*
    if (!slash && loname != NAME_None) {
      if (loname == "ftub3") {
        GCon->Log("===========");
        i = GTextureManager.CheckNumForName(loname, Type, true);
        //i = GTextureManager.CheckNumForName(loname, TEXTYPE_Flat, false);
        GCon->Logf("*********** FTUB3 (%s)! i=%d", VTexture::TexTypeToStr(Type), i);
      }
    }
    */
    if (i == -1) i = GTextureManager.AddFileTextureChecked(VName(name, VName::AddLower), Type);
    //if (i != -1) GCon->Logf("texForceLoad(2): <%s> (%d)", name, i);
    if (i == -1 && loname != NAME_None) {
      i = GTextureManager.AddFileTextureChecked(loname, Type);
      //if (i != -1) GCon->Logf("texForceLoad(3): <%s> (%d)", name, i);
    }
  }

  if (i == -1) {
    VStr nn = VStr(name);
    if (!mapTextureWarns.has(nn)) {
      mapTextureWarns.put(nn, true);
      GCon->Logf(NAME_Warning, "MAP TEXTURE NOT FOUND: '%s'", name);
    }
    i = (CMap ? 0 : GTextureManager.DefaultTexture);
  }
  return i;
}


//==========================================================================
//
//  LdrTexNumForName
//
//  native int LdrTexNumForName (string name, int Type, optional bool CMap, optional bool fromUDMF);
//
//==========================================================================
IMPLEMENT_FUNCTION(VLevel, LdrTexNumForName) {
  P_GET_BOOL_OPT(fromUDMF, false);
  P_GET_BOOL_OPT(CMap, false);
  P_GET_INT(Type);
  P_GET_STR(name);
  P_GET_SELF;
  RET_INT(Self->TexNumForName(*name, Type, CMap, fromUDMF));
}


//==========================================================================
//
//  VLevel::TexNumForName
//
//  Retrieval, get a texture or flat number for a name.
//
//==========================================================================
int VLevel::TexNumForName (const char *name, int Type, bool CMap, bool fromUDMF) const {
  if (!name || !name[0] || VStr::Cmp(name, "-") == 0) return 0;
  return texForceLoad(name, Type, CMap, /*(fromUDMF ? r_udmf_allow_extra_textures : false)*/true);
/*
  int i = -1;
  // try filename if slash is found
  const char *slash = strchr(name, '/');
  if (slash && slash[1] && fromUDMF && r_udmf_allow_extra_textures) {
    VName loname = VName(name, VName::AddLower);
    i = GTextureManager.AddFileTextureChecked(loname, Type);
    if (i != -1) return i;
  } else if (strchr(name, '.')) {
    VName loname = VName(name, VName::AddLower);
    i = GTextureManager.AddFileTextureChecked(loname, Type);
    if (i != -1) return i;
  }
  VName Name(name, VName::AddLower8);
  i = GTextureManager.CheckNumForName(Name, Type, true, true);
  //if (i == -1) i = GTextureManager.CheckNumForName(VName(name, VName::AddLower), Type, true, true);
  //if (i == -1 && VStr::length(name) > 8) i = GTextureManager.AddFileTexture(VName(name, VName::AddLower), Type);
  if (i == -1) {
    static TStrSet texNumForNameWarned;
    if (CMap) return 0;
    VName loname = VName(name, VName::AddLower);
    if (!texNumForNameWarned.put(*loname)) GCon->Logf(NAME_Warning, "VLevel::TexNumForName: '%s' not found", *loname);
    if (fromUDMF && r_udmf_allow_extra_textures) {
      if (!slash) {
        i = GTextureManager.AddFileTextureChecked(loname, Type);
        if (i != -1) {
          GCon->Logf(NAME_Warning, "VLevel::TexNumForName: force-loaded '%s'", *loname);
          return i;
        }
      }
    }
    return GTextureManager.DefaultTexture;
  } else {
    //static TStrSet texReported;
    //if (!texReported.put(name)) GCon->Logf("TEXTURE: '%s' (%s) is %d", name, *Name, i);
  }
  return i;
*/
}


//==========================================================================
//
//  VLevel::TexNumForName2
//
//==========================================================================
int VLevel::TexNumForName2 (const char *name, int Type, bool fromUDMF) const {
  if (!name || !name[0]) return 0; //GTextureManager.DefaultTexture; // just in case
  //int res = GTextureManager.CheckNumForName(VName(name, VName::AddLower), Type, /*bOverload*/true, /*bCheckAny*/true);
  //if (!fromUDMF) return res;
  return texForceLoad(name, Type, /*CMap*/false, /*r_udmf_allow_extra_textures*/true);
}


//==========================================================================
//
//  VLevel::TexNumOrColour
//
//==========================================================================
int VLevel::TexNumOrColour (const char *name, int Type, bool &GotColour, vuint32 &Col) const {
  VName Name(name, VName::AddLower8);
  int i = GTextureManager.CheckNumForName(Name, Type, true);
  if (i == -1) {
    char TmpName[9];
    memcpy(TmpName, name, 8);
    TmpName[8] = 0;
    char *Stop;
    Col = strtoul(TmpName, &Stop, 16);
    GotColour = (*Stop == 0) && (Stop >= TmpName+2) && (Stop <= TmpName+6);
    return 0;
  }
  GotColour = false;
  Col = 0;
  return i;
}


//==========================================================================
//
//  VLevel::LoadRogueConScript
//
//==========================================================================
void VLevel::LoadRogueConScript (VName LumpName, int ALumpNum, FRogueConSpeech *&SpeechList, int &NumSpeeches) const {
  bool teaser = false;
  // clear variables
  SpeechList = nullptr;
  NumSpeeches = 0;

  int LumpNum = ALumpNum;
  if (LumpNum < 0) {
    // check for empty name
    if (LumpName == NAME_None) return;
    // get lump num
    LumpNum = W_CheckNumForName(LumpName);
    if (LumpNum < 0) return; // not here
  }

  // load them

  // first check the size of the lump, if it's 1516,
  // we are using a registered strife lump, if it's
  // 1488, then it's a teaser conversation script
  if (W_LumpLength(LumpNum)%1516 != 0) {
    NumSpeeches = W_LumpLength(LumpNum)/1488;
    teaser = true;
  } else {
    NumSpeeches = W_LumpLength(LumpNum)/1516;
  }

  SpeechList = new FRogueConSpeech[NumSpeeches];

  VStream *lumpstream = W_CreateLumpReaderNum(LumpNum);
  VCheckedStream Strm(lumpstream);
  for (int i = 0; i < NumSpeeches; ++i) {
    char Tmp[324];

    FRogueConSpeech &S = SpeechList[i];
    if (!teaser) {
      // parse non teaser speech
      Strm << S.SpeakerID << S.DropItem << S.CheckItem1 << S.CheckItem2
        << S.CheckItem3 << S.JumpToConv;

      // parse NPC name
      Strm.Serialise(Tmp, 16);
      Tmp[16] = 0;
      S.Name = Tmp;

      // parse sound name (if any)
      Strm.Serialise(Tmp, 8);
      Tmp[8] = 0;
      S.Voice = VName(Tmp, VName::AddLower8);
      if (S.Voice != NAME_None) S.Voice = va("svox/%s", *S.Voice);

      // parse backdrop pics (if any)
      Strm.Serialise(Tmp, 8);
      Tmp[8] = 0;
      S.BackPic = VName(Tmp, VName::AddLower8);

      // parse speech text
      Strm.Serialise(Tmp, 320);
      Tmp[320] = 0;
      S.Text = Tmp;
    } else {
      // parse teaser speech, which doesn't contain all fields
      Strm << S.SpeakerID << S.DropItem;

      // parse NPC name
      Strm.Serialise(Tmp, 16);
      Tmp[16] = 0;
      S.Name = Tmp;

      // parse sound number (if any)
      vint32 Num;
      Strm << Num;
      if (Num) S.Voice = va("svox/voc%d", Num);

      // also, teaser speeches don't have backdrop pics
      S.BackPic = NAME_None;

      // parse speech text
      Strm.Serialise(Tmp, 320);
      Tmp[320] = 0;
      S.Text = Tmp;
    }

    // parse conversation options for PC
    for (int j = 0; j < 5; ++j) {
      FRogueConChoice &C = S.Choices[j];
      Strm << C.GiveItem << C.NeedItem1 << C.NeedItem2 << C.NeedItem3
        << C.NeedAmount1 << C.NeedAmount2 << C.NeedAmount3;
      Strm.Serialise(Tmp, 32);
      Tmp[32] = 0;
      C.Text = Tmp;
      Strm.Serialise(Tmp, 80);
      Tmp[80] = 0;
      C.TextOK = Tmp;
      Strm << C.Next << C.Objectives;
      Strm.Serialise(Tmp, 80);
      Tmp[80] = 0;
      C.TextNo = Tmp;
    }
  }
}


//==========================================================================
//
//  VLevel::ClearBox
//
//==========================================================================
inline void VLevel::ClearBox (float *box) const {
  box[BOXTOP] = box[BOXRIGHT] = -99999.0f;
  box[BOXBOTTOM] = box[BOXLEFT] = 99999.0f;
}


//==========================================================================
//
//  VLevel::AddToBox
//
//==========================================================================
inline void VLevel::AddToBox (float *box, float x, float y) const {
       if (x < box[BOXLEFT]) box[BOXLEFT] = x;
  else if (x > box[BOXRIGHT]) box[BOXRIGHT] = x;
       if (y < box[BOXBOTTOM]) box[BOXBOTTOM] = y;
  else if (y > box[BOXTOP]) box[BOXTOP] = y;
}


//==========================================================================
//
//  VLevel::PostProcessForDecals
//
//==========================================================================
void VLevel::PostProcessForDecals () {
  GCon->Logf(NAME_Dev, "postprocessing level for faster decals...");

  for (int i = 0; i < NumLines; ++i) Lines[i].firstseg = nullptr;

  GCon->Logf(NAME_Dev, "postprocessing level for faster decals: assigning segs...");
  // collect segments, so we won't go thru the all segs in decal spawner
  for (int sidx = 0; sidx < NumSegs; ++sidx) {
    seg_t *seg = &Segs[sidx];
    line_t *li = seg->linedef;
    if (!li) continue;
    seg->lsnext = li->firstseg;
    li->firstseg = seg;
  }
}


//==========================================================================
//
//  VLevel::GroupLines
//
//  builds sector line lists and subsector sector numbers
//  finds block bounding boxes for sectors
//
//==========================================================================
void VLevel::GroupLines () const {
  line_t ** linebuffer;
  int total;
  line_t *li;
  sector_t *sector;
  float bbox[4];
  int block;

  LinkNode(NumNodes-1, nullptr);

  // count number of lines in each sector
  li = Lines;
  total = 0;
  for (int i = 0; i < NumLines; ++i, ++li) {
    ++total;
    ++li->frontsector->linecount;
    if (li->backsector && li->backsector != li->frontsector) {
      ++li->backsector->linecount;
      ++total;
    }
  }

  // build line tables for each sector
  linebuffer = new line_t*[total];
  sector = Sectors;
  for (int i = 0; i < NumSectors; ++i, ++sector) {
    sector->lines = linebuffer;
    linebuffer += sector->linecount;
  }

  // assign lines for each sector
  int *SecLineCount = new int[NumSectors];
  memset(SecLineCount, 0, sizeof(int)*NumSectors);
  li = Lines;
  for (int i = 0; i < NumLines; ++i, ++li) {
    if (li->frontsector) {
      li->frontsector->lines[SecLineCount[li->frontsector-Sectors]++] = li;
    }
    if (li->backsector && li->backsector != li->frontsector) {
      li->backsector->lines[SecLineCount[li->backsector-Sectors]++] = li;
    }
  }

  sector = Sectors;
  for (int i = 0; i < NumSectors; ++i, ++sector) {
    if (SecLineCount[i] != sector->linecount) Sys_Error("GroupLines: miscounted");
    ClearBox(bbox);
    for (int j = 0; j < sector->linecount; ++j) {
      li = sector->lines[j];
      AddToBox(bbox, li->v1->x, li->v1->y);
      AddToBox(bbox, li->v2->x, li->v2->y);
    }

    // set the soundorg to the middle of the bounding box
    sector->soundorg = TVec((bbox[BOXRIGHT]+bbox[BOXLEFT])/2.0f, (bbox[BOXTOP]+bbox[BOXBOTTOM])/2.0f, 0);

    // adjust bounding box to map blocks
    block = MapBlock(bbox[BOXTOP]-BlockMapOrgY+MAXRADIUS);
    block = block >= BlockMapHeight ? BlockMapHeight-1 : block;
    sector->blockbox[BOXTOP] = block;

    block = MapBlock(bbox[BOXBOTTOM]-BlockMapOrgY-MAXRADIUS);
    block = block < 0 ? 0 : block;
    sector->blockbox[BOXBOTTOM] = block;

    block = MapBlock(bbox[BOXRIGHT]-BlockMapOrgX+MAXRADIUS);
    block = block >= BlockMapWidth ? BlockMapWidth-1 : block;
    sector->blockbox[BOXRIGHT] = block;

    block = MapBlock(bbox[BOXLEFT]-BlockMapOrgX-MAXRADIUS);
    block = block < 0 ? 0 : block;
    sector->blockbox[BOXLEFT] = block;
  }
  delete[] SecLineCount;
  SecLineCount = nullptr;
}


//==========================================================================
//
//  VLevel::LinkNode
//
//==========================================================================
void VLevel::LinkNode (int BSPNum, node_t *pParent) const {
  if (BSPNum&NF_SUBSECTOR) {
    int num = (BSPNum == -1 ? 0 : BSPNum&(~NF_SUBSECTOR));
    if (num < 0 || num >= NumSubsectors) Host_Error("ss %i with numss = %i", num, NumSubsectors);
    Subsectors[num].parent = pParent;
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
//  VLevel::CreateRepBase
//
//==========================================================================
void VLevel::CreateRepBase () {
  BaseLines = new rep_line_t[NumLines];
  for (int i = 0; i < NumLines; ++i) {
    line_t &L = Lines[i];
    rep_line_t &B = BaseLines[i];
    B.alpha = L.alpha;
  }

  BaseSides = new rep_side_t[NumSides];
  for (int i = 0; i < NumSides; ++i) {
    side_t &S = Sides[i];
    rep_side_t &B = BaseSides[i];
    B.Top.TextureOffset = S.Top.TextureOffset;
    B.Bot.TextureOffset = S.Bot.TextureOffset;
    B.Mid.TextureOffset = S.Mid.TextureOffset;
    B.Top.RowOffset = S.Top.RowOffset;
    B.Bot.RowOffset = S.Bot.RowOffset;
    B.Mid.RowOffset = S.Mid.RowOffset;
    B.TopTexture = S.TopTexture;
    B.BottomTexture = S.BottomTexture;
    B.MidTexture = S.MidTexture;
    B.Flags = S.Flags;
    B.Light = S.Light;
  }

  BaseSectors = new rep_sector_t[NumSectors];
  for (int i = 0; i < NumSectors; ++i) {
    sector_t &S = Sectors[i];
    rep_sector_t &B = BaseSectors[i];
    B.floor_pic = S.floor.pic;
    B.floor_dist = S.floor.dist;
    B.floor_xoffs = S.floor.xoffs;
    B.floor_yoffs = S.floor.yoffs;
    B.floor_XScale = S.floor.XScale;
    B.floor_YScale = S.floor.YScale;
    B.floor_Angle = S.floor.Angle;
    B.floor_BaseAngle = S.floor.BaseAngle;
    B.floor_BaseYOffs = S.floor.BaseYOffs;
    B.floor_SkyBox = nullptr;
    B.floor_MirrorAlpha = S.floor.MirrorAlpha;
    B.ceil_pic = S.ceiling.pic;
    B.ceil_dist = S.ceiling.dist;
    B.ceil_xoffs = S.ceiling.xoffs;
    B.ceil_yoffs = S.ceiling.yoffs;
    B.ceil_XScale = S.ceiling.XScale;
    B.ceil_YScale = S.ceiling.YScale;
    B.ceil_Angle = S.ceiling.Angle;
    B.ceil_BaseAngle = S.ceiling.BaseAngle;
    B.ceil_BaseYOffs = S.ceiling.BaseYOffs;
    B.ceil_SkyBox = nullptr;
    B.ceil_MirrorAlpha = S.ceiling.MirrorAlpha;
    B.lightlevel = S.params.lightlevel;
    B.Fade = S.params.Fade;
    B.Sky = S.Sky;
  }

  BasePolyObjs = new rep_polyobj_t[NumPolyObjs];
  for (int i = 0; i < NumPolyObjs; ++i) {
    polyobj_t &P = PolyObjs[i];
    rep_polyobj_t &B = BasePolyObjs[i];
    B.startSpot = P.startSpot;
    B.angle = P.angle;
  }
}


//==========================================================================
//
//  VLevel::HashSectors
//
//==========================================================================
void VLevel::HashSectors () {
  // clear hash; count number of sectors with fake something
  for (int i = 0; i < NumSectors; ++i) Sectors[i].HashFirst = Sectors[i].HashNext = -1;
  // create hash: process sectors in backward order so that they get processed in original order
  for (int i = NumSectors-1; i >= 0; --i) {
    if (Sectors[i].tag) {
      vuint32 HashIndex = (vuint32)Sectors[i].tag%(vuint32)NumSectors;
      Sectors[i].HashNext = Sectors[HashIndex].HashFirst;
      Sectors[HashIndex].HashFirst = i;
    }
  }
}


//==========================================================================
//
//  VLevel::BuildSectorLists
//
//==========================================================================
void VLevel::BuildSectorLists () {
  // count number of fake and tagged sectors
  int fcount = 0, tcount = 0;
  const int scount = NumSectors;

  TArray<int> interesting;
  int intrcount = 0;
  interesting.setLength(scount);

  const sector_t *sec = Sectors;
  for (int i = 0; i < scount; ++i, ++sec) {
    bool intr = false;
    // tagged?
    if (sec->tag) { ++tcount; intr = true; }
    // with fakes?
         if (sec->deepref) { ++fcount; intr = true; }
    else if (sec->heightsec && !(sec->heightsec->SectorFlags&sector_t::SF_IgnoreHeightSec)) { ++fcount; intr = true; }
    else if (sec->othersecFloor || sec->othersecCeiling) { ++fcount; intr = true; }
    // register "interesting" sector
    if (intr) interesting[intrcount++] = i;
  }

  GCon->Logf("%d tagged sectors, %d sectors with fakes, %d total sectors", tcount, fcount, scount);

  FakeFCSectors.setLength(fcount);
  TaggedSectors.setLength(tcount);
  fcount = tcount = 0;

  for (int i = 0; i < intrcount; ++i) {
    const int idx = interesting[i];
    sec = &Sectors[idx];
    // tagged?
    if (sec->tag) TaggedSectors[tcount++] = idx;
    // with fakes?
         if (sec->deepref) FakeFCSectors[fcount++] = idx;
    else if (sec->heightsec && !(sec->heightsec->SectorFlags&sector_t::SF_IgnoreHeightSec)) FakeFCSectors[fcount++] = idx;
    else if (sec->othersecFloor || sec->othersecCeiling) FakeFCSectors[fcount++] = idx;
  }
}


//==========================================================================
//
//  VLevel::HashLines
//
//==========================================================================
void VLevel::HashLines () {
  // clear hash
  for (int i = 0; i < NumLines; ++i) Lines[i].HashFirst = -1;
  // create hash: process lines in backward order so that they get processed in original order
  for (int i = NumLines-1; i >= 0; --i) {
    vuint32 HashIndex = (vuint32)Lines[i].LineTag%(vuint32)NumLines;
    Lines[i].HashNext = Lines[HashIndex].HashFirst;
    Lines[HashIndex].HashFirst = i;
  }
}


//==========================================================================
//
//  VLevel::FloodZones
//
//==========================================================================
void VLevel::FloodZones () {
  for (int i = 0; i < NumSectors; ++i) {
    if (Sectors[i].Zone == -1) {
      FloodZone(&Sectors[i], NumZones);
      ++NumZones;
    }
  }

  Zones = new vint32[NumZones];
  for (int i = 0; i < NumZones; ++i) Zones[i] = 0;
}


//==========================================================================
//
//  VLevel::FloodZone
//
//==========================================================================
void VLevel::FloodZone (sector_t *Sec, int Zone) {
  Sec->Zone = Zone;
  for (int i = 0; i < Sec->linecount; ++i) {
    line_t *Line = Sec->lines[i];
    if (Line->flags&ML_ZONEBOUNDARY) continue;
    if (Line->frontsector && Line->frontsector->Zone == -1) FloodZone(Line->frontsector, Zone);
    if (Line->backsector && Line->backsector->Zone == -1) FloodZone(Line->backsector, Zone);
  }
}


//==========================================================================
//
// VLevel::FixSelfRefDeepWater
//
// This code was taken from Hyper3dge
//
//==========================================================================
void VLevel::FixSelfRefDeepWater () {
  TArray<vuint8> self_subs;
  self_subs.setLength(NumSubsectors);
  memset(self_subs.ptr(), 0, NumSubsectors);

  for (int i = 0; i < NumSegs; ++i) {
    const seg_t *seg = &Segs[i];

    if (!seg->linedef) continue; // miniseg?
    if (!seg->front_sub) { GCon->Logf("INTERNAL ERROR IN GLBSP LOADER: FRONT SUBSECTOR IS NOT SET!"); return; }

    const int fsnum = (int)(ptrdiff_t)(seg->front_sub-Subsectors);

    sector_t *fs = seg->linedef->frontsector;
    sector_t *bs = seg->linedef->backsector;

    // slopes aren't interesting
    if (bs && fs == bs &&
        (fs->SectorFlags&sector_t::SF_ExtrafloorSource) == 0 &&
        fs->floor.normal.z == 1.0f /*&& fs->ceiling.normal.z == -1.0f*/ &&
        bs->floor.normal.z == 1.0f /*&& bs->ceiling.normal.z == -1.0f*/)
    {
      self_subs[fsnum] |= 1;
    } else {
      self_subs[fsnum] |= 2;
    }
  }

  for (int pass = 0; pass < 100; ++pass) {
    int count = 0;

    for (int j = 0; j < NumSubsectors; ++j) {
      subsector_t *sub = &Subsectors[j];
      seg_t *seg;

      if (self_subs[j] != 1) continue;
#ifdef DEBUG_DEEP_WATERS
      if (dbg_deep_water) GCon->Logf("Subsector [%d] sec %d --> %d", j, (int)(sub->sector-Sectors), self_subs[j]);
#endif
      seg_t *Xseg = 0;

      for (int ssi = 0; ssi < sub->numlines; ++ssi) {
        // this is how back_sub set
        seg = &Segs[sub->firstline+ssi];
        subsector_t *back_sub = (seg->partner ? seg->partner->front_sub : nullptr);
        if (!back_sub) { GCon->Logf("INTERNAL ERROR IN GLBSP LOADER: BACK SUBSECTOR IS NOT SET!"); return; }

        int k = (int)(back_sub-Subsectors);
#ifdef DEBUG_DEEP_WATERS
        if (dbg_deep_water) GCon->Logf("  Seg [%d] back_sub %d (back_sect %d)", (int)(seg-Segs), k, (int)(back_sub->sector-Sectors));
#endif
        if (self_subs[k]&2) {
          if (!Xseg) Xseg = seg;
        }
      }

      if (Xseg) {
        //sub->deep_ref = Xseg->back_sub->deep_ref ? Xseg->back_sub->deep_ref : Xseg->back_sub->sector;
        subsector_t *Xback_sub = (Xseg->partner ? Xseg->partner->front_sub : nullptr);
        if (!Xback_sub) { GCon->Logf("INTERNAL ERROR IN GLBSP LOADER: BACK SUBSECTOR IS NOT SET!"); return; }
        sub->deepref = (Xback_sub->deepref ? Xback_sub->deepref : Xback_sub->sector);
#ifdef DEBUG_DEEP_WATERS
        if (dbg_deep_water) GCon->Logf("  Updating (from seg %d) --> SEC %d", (int)(Xseg-Segs), (int)(sub->deepref-Sectors));
#endif
        self_subs[j] = 3;

        ++count;
      }
    }

    if (count == 0) break;
  }

  for (int i = 0; i < NumSubsectors; ++i) {
    subsector_t *sub = &Subsectors[i];
    sector_t *hs = sub->deepref;
    if (!hs) continue;
    sector_t *ss = sub->sector;
    // if deepref is the same as the source sector, this is pointless
    if (hs == ss) { sub->deepref = nullptr; continue; }
    if (!ss) { if (dbg_deep_water) GCon->Logf("WTF(0)?!"); continue; }
    if (ss->deepref) {
      if (ss->deepref != hs) { if (dbg_deep_water) GCon->Logf("WTF(1) %d : %d?!", (int)(hs-Sectors), (int)(ss->deepref-Sectors)); continue; }
    } else {
      ss->deepref = hs;
    }
    // also, add deepref PVS info to the current one
    // this is unoptimal, but required for zdbsp, until
    // i'll fix PVS calculations for it.
    // the easiest way to see why it is required is to
    // comment this out, and do:
    //   -tnt +map map02 +notarget +noclip +warpto 866 1660
    // you will immediately see rendering glitches.
    // this also affects sight calculations, 'cause PVS is
    // used for fast rejects there.
    if (VisData && nodes_builder) {
      vuint8 *vis = VisData+(((NumSubsectors+7)>>3)*i);
      for (subsector_t *s2 = hs->subsectors; s2; s2 = s2->seclink) {
        if (s2 == sub) continue; // just in case
        // or vis data
        vuint8 *v2 = VisData+(((NumSubsectors+7)>>3)*((int)(ptrdiff_t)(s2-Subsectors)));
        vuint8 *dest = vis;
        for (int cc = (NumSubsectors+7)/8; cc > 0; --cc, ++dest, ++v2) *dest |= *v2;
      }
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
enum {
  FFBugFloor = 0x01U,
  FFBugCeiling = 0x02U,
};


//==========================================================================
//
//  VLevel::IsFloodBugSector
//
//==========================================================================
vuint32 VLevel::IsFloodBugSector (sector_t *sec) {
  if (!sec) return 0;
  if (sec->linecount == 0 || sec->deepref) return 0;
  if (sec->floor.minz >= sec->ceiling.minz) return 0;
  if (sec->floor.normal.z != 1.0f || sec->ceiling.normal.z != -1.0f) return 0;
  int res = (deepwater_hacks_floor ? FFBugFloor : 0)|(deepwater_hacks_ceiling ? FFBugCeiling : 0); // not yet
  // don't fix alphas
  if (sec->floor.Alpha != 1.0f) res &= ~FFBugFloor;
  if (sec->ceiling.Alpha != 1.0f) res &= ~FFBugCeiling;
  int myside = -1;
  for (int f = 0; f < sec->linecount; ++f) {
    if (!res) return 0;
    line_t *line = sec->lines[f];
    if (!(!!line->frontsector && !!line->backsector)) continue;
    //if (!line->frontsector || !line->backsector) return 0;
    sector_t *bs;
    if (line->frontsector == sec) {
      // back
      bs = line->backsector;
      if (myside == 1) continue;
      myside = 0;
    } else if (line->backsector == sec) {
      // front
      bs = line->frontsector;
      if (myside == 0) continue;
      myside = 1;
    } else {
      return 0; // something's strange in the neighbourhood
    }
    if (bs == sec) return 0; // this is self-referenced sector, nothing to see here, come along
    if (bs->floor.minz >= bs->ceiling.minz) return 0; // this looks like a door, don't "fix" anything
    // check for possible floor floodbug
    do {
      if (res&FFBugFloor) {
        // line has no bottom texture?
        if (Sides[line->sidenum[myside]].BottomTexture != 0) { res &= ~FFBugFloor; break; }
        // slope?
        if (bs->floor.normal.z != 1.0f) { res &= ~FFBugFloor; break; }
        // height?
        if (bs->floor.minz <= sec->floor.minz) { res &= ~FFBugFloor; break; }
        //if (/*line->special != 0 &&*/ bs->floor.minz == sec->floor.minz) { res &= ~FFBugFloor; continue; }
      }
    } while (0);
    // check for possible ceiling floodbug
    do {
      //TODO: here we should ignore lifts
      if (res&FFBugCeiling) {
        /*
        int ssnum = (int)(ptrdiff_t)(sec-Sectors);
        if (ssnum == 314) {
          int fsnum = (int)(ptrdiff_t)(line->frontsector-Sectors);
          int bsnum = (int)(ptrdiff_t)(line->backsector-Sectors);
          GCon->Logf("fs: %d; bs: %d", fsnum, bsnum);
        } else {
          //GCon->Logf("ss: %d", ssnum);
        }
        */
        // line has no top texture?
        if (Sides[line->sidenum[myside]].TopTexture != 0) { res &= ~FFBugCeiling; break; }
        // slope?
        if (bs->ceiling.normal.z != -1.0f) { res &= ~FFBugCeiling; break; }
        // height?
        if (bs->ceiling.minz >= sec->ceiling.minz) { res &= ~FFBugCeiling; break; }
        //if (line->special != 0 && bs->ceiling.minz == sec->ceiling.minz) { res &= ~FFBugCeiling; continue; }
      }
    } while (0);
  }
  //if (res&FFBugCeiling) GCon->Logf("css: %d", (int)(ptrdiff_t)(sec-Sectors));
  return res;
}


//==========================================================================
//
//  VLevel::FindGoodFloodSector
//
//  try to find a sector to borrow a fake surface
//  we'll try each neighbour sector until we'll find a sector without
//  flood bug
//
//==========================================================================
sector_t *VLevel::FindGoodFloodSector (sector_t *sec, bool wantFloor) {
  if (!sec) return nullptr;
  TArray<sector_t *> good;
  TArray<sector_t *> seen;
  TArray<sector_t *> sameBug;
  seen.append(sec);
  vuint32 bugMask = (wantFloor ? FFBugFloor : FFBugCeiling);
  //int ssnum = (int)(ptrdiff_t)(sec-Sectors);
  for (;;) {
    for (int f = 0; f < sec->linecount; ++f) {
      line_t *line = sec->lines[f];
      if (!(!!line->frontsector && !!line->backsector)) continue;
      sector_t *fs;
      sector_t *bs;
      if (line->frontsector == sec) {
        // back
        fs = line->frontsector;
        bs = line->backsector;
      } else if (line->backsector == sec) {
        // front
        fs = line->backsector;
        bs = line->frontsector;
      } else {
        return nullptr; // something's strange in the neighbourhood
      }
      // bs is possible sector to move to
      bool wasSeen = false;
      for (int c = seen.length(); c > 0; --c) if (seen[c-1] == bs) { wasSeen = true; break; }
      if (wasSeen) continue; // we already rejected this sector
      seen.append(bs);
      if (fs == bs) continue;
      //vuint32 xxbug = IsFloodBugSector(fs);
      vuint32 ffbug = IsFloodBugSector(bs);
      //ffbug |= xxbug;
      //if (ssnum == 981) GCon->Logf("xxx: %d (%d); xxbug=%d; ffbug=%d", ssnum, (int)(ptrdiff_t)(bs-Sectors), xxbug, ffbug);
      //if (ssnum == 981) GCon->Logf("xxx: %d (%d); ffbug=%d", ssnum, (int)(ptrdiff_t)(bs-Sectors), ffbug);
      if (ffbug) {
        if ((ffbug&bugMask) != 0) {
          sameBug.append(bs);
          continue;
        }
      }
      // we found a sector without floodbug, check if it is a good one
      // sloped?
      if (wantFloor && bs->floor.normal.z != 1.0f) continue;
      if (!wantFloor && bs->ceiling.normal.z != -1.0f) continue;
      // check height
      if (wantFloor && bs->floor.minz < sec->floor.minz) continue;
      if (!wantFloor && bs->ceiling.minz > sec->ceiling.minz) continue;
      // possible good sector, remember it
      good.append(bs);
    }
    // if we have no good sectors, try neighbour sector with the same bug
    if (good.length() != 0) break;
    //if (good.length() == 0) return nullptr; // oops
    /*
    sec = good[0];
    good.removeAt(0);
    */
    if (sameBug.length() == 0) return nullptr;
    sec = sameBug[0];
    sameBug.removeAt(0);
  }
  // here we should have some good sectors
  if (good.length() == 0) return nullptr; // sanity check
  if (good.length() == 1) return good[0];
  // we have several good sectors; check if they have the same height, and the same flat texture
  //if (ssnum == 981) GCon->Logf("xxx: %d; len=%d", ssnum, good.length());
  sector_t *res = good[0];
  //sector_t *best = (IsFloodBugSector(res) ? nullptr : res);
  for (int f = 1; f < good.length(); ++f) {
    sec = good[f];
    //if (ssnum == 981) GCon->Logf("yyy(%d): %d; res=%d; ff=%d", f, ssnum, (int)(ptrdiff_t)(sec-Sectors), IsFloodBugSector(sec));
    //!if (sec->params.lightlevel != res->params.lightlevel) return nullptr; //k8: ignore this?
    //!if (sec->params.LightColour != res->params.LightColour) return nullptr; //k8: ignore this?
    if (wantFloor) {
      // floor
      if (sec->floor.minz != res->floor.minz) return nullptr;
      if (sec->floor.pic != res->floor.pic) return nullptr;
    } else {
      // ceiling
      if (sec->ceiling.minz != (/*best ? best :*/ res)->ceiling.minz) {
        //GCon->Logf("000: %d (%d)", ssnum, (int)(ptrdiff_t)(sec-Sectors));
        //if (IsFloodBugSector(sec)) continue;
        //GCon->Logf("000: %d; %d (%d)  %f : %f", ssnum, (int)(ptrdiff_t)(res-Sectors), (int)(ptrdiff_t)(sec-Sectors), sec->ceiling.minz, (best ? best : res)->ceiling.minz);
        return nullptr;
      }
      if (sec->ceiling.pic != res->ceiling.pic) {
        //if (IsFloodBugSector(sec)) continue;
        //GCon->Logf("001: %d (%d)", ssnum, (int)(ptrdiff_t)(sec-Sectors));
        return nullptr;
      }
    }
    //if (!best && !IsFloodBugSector(sec)) best = sec;
  }
  //if (!best) return nullptr;
  //if (best) res = best;
  //if (ssnum == 981) GCon->Logf("zzz: %d; res=%d", ssnum, (int)(ptrdiff_t)(res-Sectors));
  return res;
}


//==========================================================================
//
// VLevel::FixDeepWaters
//
//==========================================================================
void VLevel::FixDeepWaters () {
  if (NumSectors == 0) return;

  for (vint32 sidx = 0; sidx < NumSectors; ++sidx) {
    sector_t *sec = &Sectors[sidx];
    sec->deepref = nullptr;
    sec->othersecFloor = nullptr;
    sec->othersecCeiling = nullptr;
  }

  if (deepwater_hacks && !(LevelFlags&LF_ForceNoDeepwaterFix)) FixSelfRefDeepWater();

  bool oldFixFloor = deepwater_hacks_floor;
  bool oldFixCeiling = deepwater_hacks_ceiling;

  if (LevelFlags&LF_ForceNoFloorFloodfillFix) deepwater_hacks_floor = false;
  if (LevelFlags&LF_ForceNoCeilingFloodfillFix) deepwater_hacks_ceiling = false;

  if (deepwater_hacks_floor || deepwater_hacks_ceiling) {
    // fix "floor holes"
    for (int sidx = 0; sidx < NumSectors; ++sidx) {
      sector_t *sec = &Sectors[sidx];
      if (sec->linecount == 0 || sec->deepref) continue;
      if (sec->SectorFlags&sector_t::SF_UnderWater) continue; // this is special sector, skip it
      if ((sec->SectorFlags&(sector_t::SF_HasExtrafloors|sector_t::SF_ExtrafloorSource|sector_t::SF_TransferSource|sector_t::SF_UnderWater))) {
        if (!(sec->SectorFlags&sector_t::SF_IgnoreHeightSec)) continue; // this is special sector, skip it
      }
      // slopes aren't interesting
      if (sec->floor.normal.z != 1.0f || sec->ceiling.normal.z != -1.0f) continue;
      if (sec->floor.minz >= sec->ceiling.minz) continue;
      vuint32 bugFlags = IsFloodBugSector(sec);
      if (bugFlags == 0) continue;
      sector_t *fsecFloor = nullptr, *fsecCeiling = nullptr;
      if (bugFlags&FFBugFloor) fsecFloor = FindGoodFloodSector(sec, true);
      if (bugFlags&FFBugCeiling) fsecCeiling = FindGoodFloodSector(sec, false);
      if (fsecFloor == sec) fsecFloor = nullptr;
      if (fsecCeiling == sec) fsecCeiling = nullptr;
      if (!fsecFloor && !fsecCeiling) continue;
      GCon->Logf("FLATFIX: found illusiopit at sector #%d (floor:%s; ceiling:%s)", sidx, (fsecFloor ? "tan" : "ona"), (fsecCeiling ? "tan" : "ona"));
      sec->othersecFloor = fsecFloor;
      sec->othersecCeiling = fsecCeiling;
      // allocate fakefloor data (engine require it to complete setup)
      if (!sec->fakefloors) sec->fakefloors = new fakefloor_t;
      fakefloor_t *ff = sec->fakefloors;
      memset((void *)ff, 0, sizeof(fakefloor_t));
      ff->floorplane = (fsecFloor ? fsecFloor : sec)->floor;
      ff->ceilplane = (fsecCeiling ? fsecCeiling : sec)->ceiling;
      ff->params = sec->params;
      //sec->SectorFlags = (fsecFloor ? SF_FakeFloorOnly : 0)|(fsecCeiling ? SF_FakeCeilingOnly : 0);
    }
  }

  deepwater_hacks_floor = oldFixFloor;
  deepwater_hacks_ceiling = oldFixCeiling;
}


// ////////////////////////////////////////////////////////////////////////// //
COMMAND(pvs_rebuild) {
  if (GClLevel) {
    delete [] GClLevel->VisData;
    GClLevel->VisData = nullptr;
    delete [] GClLevel->NoVis;
    GClLevel->NoVis = nullptr;
    GClLevel->BuildPVS();
  }
}


COMMAND(pvs_reset) {
  if (GClLevel) {
    delete [] GClLevel->VisData;
    GClLevel->VisData = nullptr;
    delete [] GClLevel->NoVis;
    GClLevel->NoVis = nullptr;
    GClLevel->VisData = nullptr;
    GClLevel->NoVis = new vuint8[(GClLevel->NumSubsectors+7)/8];
    memset(GClLevel->NoVis, 0xff, (GClLevel->NumSubsectors+7)/8);
  }
}
