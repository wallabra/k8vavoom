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
//**    Do all the WAD I/O, get map description, set up initial state and
//**  misc. LUTs.
//**
//**************************************************************************

#define DEBUG_DEEP_WATERS


// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"
#include "filesys/zipstream.h"
#ifdef SERVER
#include "sv_local.h"
#endif

#ifdef CLIENT
# include "drawer.h"
#endif


static VCvarB dbg_deep_water("dbg_deep_water", false, "Show debug messages in Deep Water processor?", 0/*CVAR_Archive*/);
static VCvarB dbg_use_old_decal_pp("dbg_use_old_decal_pp", false, "Use old decal processor? (for timing)", 0/*CVAR_Archive*/);

VCvarB loader_cache_rebuilt_data("loader_cache_rebuilt_data", true, "Cache rebuilt nodes, pvs, blockmap, and so on?", CVAR_Archive);
static VCvarF loader_cache_time_limit("loader_cache_time_limit", "3.0", "Cache data if building took more than this number of seconds.", CVAR_Archive);
static VCvarI loader_cache_max_age_days("loader_cache_max_age_days", "7", "Remove cached data older than this number of days (<=0: none).", CVAR_Archive);

static VCvarB strict_level_errors("strict_level_errors", true, "Strict level errors mode?", 0);
static VCvarB deepwater_hacks("deepwater_hacks", true, "Apply self-referenced deepwater hacks?", CVAR_Archive);
static VCvarB deepwater_hacks_extra("deepwater_hacks_extra", true, "Apply deepwater hacks to fix some map errors? (not working right yet)", CVAR_Archive);
static VCvarB build_blockmap("build_blockmap", false, "Build blockmap?", CVAR_Archive);
static VCvarB show_level_load_times("show_level_load_times", false, "Show loading times?", CVAR_Archive);

// there seems to be a bug in compressed GL nodes reader, hence the flag
//static VCvarB nodes_allow_compressed_old("nodes_allow_compressed_old", true, "Allow loading v0 compressed GL nodes?", CVAR_Archive);
static VCvarB nodes_allow_compressed("nodes_allow_compressed", false, "Allow loading v1+ compressed GL nodes?", CVAR_Archive);

static VCvarB loader_force_nodes_rebuild("loader_force_nodes_rebuild", false, "Force node rebuilding?", CVAR_Archive);

static VCvarB loader_cache_data("loader_cache_data", false, "Cache built level data?", CVAR_Archive);
static VCvarB loader_cache_ignore_one("loader_cache_ignore_one", false, "Ignore (and remove) cache for next map loading?", 0);
static VCvarI loader_cache_compression_level("loader_cache_compression_level", "9", "Cache file compression level [0..9]", CVAR_Archive);


// MACROS ------------------------------------------------------------------

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

//  Lump order from "GL-Friendly Nodes" specs.
enum
{
  ML_GL_LABEL,  // A separator name, GL_ExMx or GL_MAPxx
  ML_GL_VERT,   // Extra Vertices
  ML_GL_SEGS,   // Segs, from linedefs & minisegs
  ML_GL_SSECT,  // SubSectors, list of segs
  ML_GL_NODES,  // GL BSP nodes
  ML_GL_PVS   // Potentially visible set
};

//  GL-node version identifiers.
#define GL_V2_MAGIC     "gNd2"
#define GL_V3_MAGIC     "gNd3"
#define GL_V5_MAGIC     "gNd5"

//  Indicates a GL-specific vertex.
#define GL_VERTEX     0x8000
#define GL_VERTEX_V3    0x40000000
#define GL_VERTEX_V5    0x80000000

//  GL-seg flags.
#define GL_SEG_FLAG_SIDE  0x0001

//  Old subsector flag.
#define NF_SUBSECTOR_OLD  0x8000

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------
static const char *CACHE_DATA_SIGNATURE = "VAVOOM CACHED DATA VERSION 002.\n";
static bool cacheCleanupComplete = false;


//==========================================================================
//
//  hashLump
//
//==========================================================================
static bool hashLump (sha512_ctx *sha512ctx, int lumpnum) {
  if (lumpnum < 0) return false;
  static vuint8 buf[65536];
  VStream *strm = W_CreateLumpReaderNum(lumpnum);
  if (!strm) return false;
  auto left = strm->TotalSize();
  while (left > 0) {
    int rd = left;
    if (rd > (int)sizeof(buf)) rd = (int)sizeof(buf);
    strm->Serialise(buf, rd);
    if (strm->IsError()) { delete strm; return false; }
    sha512_update(sha512ctx, buf, rd);
    left -= rd;
  }
  delete strm;
  return true;
}


//==========================================================================
//
//  getCacheDir
//
//==========================================================================
static VStr getCacheDir () {
  VStr res;
  if (!loader_cache_data) return res;
#if !defined(_WIN32)
  const char *HomeDir = getenv("HOME");
  if (HomeDir && HomeDir[0]) {
    res = VStr(HomeDir)+"/.vavoom";
    Sys_CreateDirectory(res);
    res += "/.mapcache";
    Sys_CreateDirectory(res);
  }
#endif
  return res;
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
    VStr shortname = fname;
    fname = cpath+"/"+fname;
    int ftime = Sys_FileTime(fname);
    if (ftime <= 0) {
      GCon->Logf("cache: deleting invalid file '%s'", *shortname);
      dellist.append(fname);
    } else if (ftime < currtime && currtime-ftime > 60*60*24*loader_cache_max_age_days) {
      GCon->Logf("cache: deleting old file '%s'", *shortname);
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
  *strm << n->dist << n->type << n->signbits;
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
  if (strm->IsError() || memcmp(sign, CACHE_DATA_SIGNATURE, 32) != 0) return false;

  VZipStreamReader *arrstrm = new VZipStreamReader(true, strm);
  if (arrstrm->IsError()) { delete arrstrm; return false; }

  int vissize = -1;
  int checkSecNum = -1;

  // flags (nothing for now)
  vuint32 flags = 0x29a;
  *arrstrm << flags;
  if (flags != 0) { delete arrstrm; Host_Error("cache file corrupted (flags)"); }

  //TODO: more checks

  // nodes
  *arrstrm << NumNodes;
  GCon->Logf("cache: reading %d nodes", NumNodes);
  if (NumNodes == 0 || NumNodes > 0x1fffffff) { delete arrstrm; delete strm; Host_Error("cache file corrupted (nodes)"); }
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
  if (checkSecNum != NumSectors) { delete arrstrm; delete strm; Host_Error("cache file corrupted (sectors)"); }
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
    if (v1num < 0) { delete arrstrm; delete strm; Host_Error("cache file corrupted (seg v1)"); }
    seg->v1 = Vertexes+v1num;
    vint32 v2num = -1;
    *arrstrm << v2num;
    if (v2num < 0) { delete arrstrm; delete strm; Host_Error("cache file corrupted (seg v2)"); }
    seg->v2 = Vertexes+v2num;
    *arrstrm << seg->offset;
    *arrstrm << seg->length;
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
  }

  // reject
  *arrstrm << RejectMatrixSize;
  if (RejectMatrixSize < 0 || RejectMatrixSize > 0x1fffffff) { delete arrstrm; delete strm; Host_Error("cache file corrupted (reject)"); }
  if (RejectMatrixSize) {
    GCon->Logf("cache: reading %d bytes of reject table", RejectMatrixSize);
    RejectMatrix = new vuint8[RejectMatrixSize];
    arrstrm->Serialize(RejectMatrix, RejectMatrixSize);
  }

  // blockmap
  *arrstrm << BlockMapLumpSize;
  if (BlockMapLumpSize < 0 || BlockMapLumpSize > 0x1fffffff) { delete arrstrm; delete strm; Host_Error("cache file corrupted (blockmap)"); }
  if (BlockMapLumpSize) {
    GCon->Logf("cache: reading %d cells of blockmap table", BlockMapLumpSize);
    BlockMapLump = new vint32[BlockMapLumpSize];
    arrstrm->Serialize(BlockMapLump, BlockMapLumpSize*4);
  }

  // pvs
  *arrstrm << vissize;
  if (vissize < 0 || vissize > 0x6fffffff) { delete arrstrm; delete strm; Host_Error("cache file corrupted (pvs)"); }
  if (vissize > 0) {
    GCon->Logf("cache: reading %d bytes of pvs table", vissize);
    VisData = new vuint8[vissize];
    arrstrm->Serialize(VisData, vissize);
  }

  if (arrstrm->IsError()) { delete arrstrm; delete strm; Host_Error("cache file corrupted (read error)"); }
  delete arrstrm;
  return true;
}


//==========================================================================
//
//  VLevel::LoadMap
//
//==========================================================================
void VLevel::LoadMap (VName AMapName) {
  guard(VLevel::LoadMap);
  bool killCache = loader_cache_ignore_one;
  loader_cache_ignore_one = false;
  bool AuxiliaryMap = false;
  int lumpnum;
  VName MapLumpName;
  decanimlist = nullptr;
  decanimuid = 0;

  double TotalTime = -Sys_Time();
  double InitTime = -Sys_Time();
  MapName = AMapName;
  // If working with a devlopment map, reload it.
  // k8: nope, it doesn't work this way: it looks for "maps/xxx.wad" in zips,
  //     and "complete.pk3" takes precedence over any pwads
  //     so let's do it in a backwards way
  // Find map and GL nodes.
  lumpnum = W_CheckNumForName(MapName);
  MapLumpName = MapName;
  // If there is no map lump, try map wad.
  if (lumpnum < 0) {
    // Check if map wad is here.
    VStr aux_file_name = va("maps/%s.wad", *MapName);
    if (FL_FileExists(aux_file_name)) {
      // Apped map wad to list of wads (it will be deleted later).
      lumpnum = W_OpenAuxiliary(aux_file_name);
      MapLumpName = W_LumpName(lumpnum);
      AuxiliaryMap = true;
    }
  }
  if (lumpnum < 0) Host_Error("Map %s not found\n", *MapName);

#ifdef CLIENT
  if (Drawer && Drawer->IsInited()) {
    T_SetFont(SmallFont);
    Drawer->StartUpdate(false); // don't clear
    T_SetAlign(hcentre, vcentre);
    // slightly off vcenter
    T_DrawText(320, 320, "LOADING...", CR_GOLD);
    Drawer->Update();
  }
#endif

  bool saveCachedData = false;
  int gl_lumpnum = -100;
  int ThingsLump = -1;
  int LinesLump = -1;
  int SidesLump = -1;
  int VertexesLump = -1;
  int SectorsLump = -1;
  int RejectLump = -1;
  int BlockmapLump = -1;
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

  sha512_ctx sha512ctx;
  bool sha512valid = false;
  VStr cacheFileName;
  VStr cacheDir = getCacheDir();

  if (cacheDir.length()) sha512_init(&sha512ctx);

  // check for UDMF map
  if (W_LumpName(lumpnum+1) == NAME_textmap) {
    LevelFlags |= LF_TextMap;
    NeedNodesBuild = true;
    for (int i = 2; true; ++i) {
      VName LName = W_LumpName(lumpnum+i);
      if (LName == NAME_endmap) break;
      if (LName == NAME_None) Host_Error("Map %s is not a valid UDMF map", *MapName);
           if (LName == NAME_behavior) BehaviorLump = lumpnum+i;
      else if (LName == NAME_blockmap) BlockmapLump = lumpnum+i;
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
    if (cacheDir.length()) sha512valid = hashLump(&sha512ctx, lumpnum+1);
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
    if (W_LumpName(LIdx) == NAME_blockmap) BlockmapLump = LIdx++;

    if (cacheDir.length()) sha512valid = hashLump(&sha512ctx, LinesLump);
    if (cacheDir.length()) sha512valid = hashLump(&sha512ctx, SidesLump);
    if (cacheDir.length()) sha512valid = hashLump(&sha512ctx, VertexesLump);
    if (cacheDir.length()) sha512valid = hashLump(&sha512ctx, SectorsLump);

    // determine level format
    if (W_LumpName(LIdx) == NAME_behavior) {
      LevelFlags |= LF_Extended;
      BehaviorLump = LIdx++;
    }

    //  Verify that it's a valid map.
    if (ThingsLump == -1 || LinesLump == -1 || SidesLump == -1 ||
        VertexesLump == -1 || SectorsLump == -1)
    {
      Host_Error("Map %s is not a valid map", *MapName);
    }

    if (SubsectorsLump != -1) {
      VStream *TmpStrm = W_CreateLumpReaderNum(SubsectorsLump);
      if (TmpStrm->TotalSize() > 4) {
        TmpStrm->Serialise(GLNodesHdr, 4);
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
      TmpStrm = nullptr;
    }
  }
  InitTime += Sys_Time();

  bool cachedDataLoaded = false;
  if (sha512valid) {
    vuint8 sha512hash[SHA512_DIGEST_SIZE];
    sha512_final(&sha512ctx, sha512hash);
    cacheFileName = VStr("mapcache_")+VStr::buf2hex(sha512hash, SHA512_DIGEST_SIZE)+".cache";
    cacheFileName = cacheDir+"/"+cacheFileName;
  }

  bool hasCacheFile = false;

  //FIXME: load cache file into temp buffer, and process it later
  if (!loader_force_nodes_rebuild && sha512valid) {
    if (killCache) {
      Sys_FileDelete(cacheFileName);
    } else {
      VStream *strm = FL_OpenSysFileRead(cacheFileName);
      hasCacheFile = !!strm;
      delete strm;
    }
  }

  double NodeBuildTime = -Sys_Time();
  bool glNodesFound = false;

  if (/*cachedDataLoaded*/hasCacheFile) {
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
        glNodesFound = true;
      }
    } else {
      if ((LevelFlags&LF_TextMap) != 0 || !UseComprGLNodes) NeedNodesBuild = true;
    }
  }
  NodeBuildTime += Sys_Time();

  int NumBaseVerts;
  double VertexTime = 0;
  double SectorsTime = 0;
  double LinesTime = 0;
  double ThingsTime = 0;
  double TranslTime = 0;
  double SidesTime = 0;
  double DecalProcessingTime = 0;
  // begin processing map lumps
  if (LevelFlags & LF_TextMap) {
    VertexTime = -Sys_Time();
    LoadTextMap(lumpnum+1, MInfo);
    VertexTime += Sys_Time();
  } else {
    // Note: most of this ordering is important
    VertexTime = -Sys_Time();
    LevelFlags &= ~LF_GLNodesV5;
    LoadVertexes(VertexesLump, gl_lumpnum + ML_GL_VERT, NumBaseVerts);
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
  double Lines2Time = -Sys_Time();
  FinaliseLines();
  Lines2Time += Sys_Time();

  if (hasCacheFile) {
    VStream *strm = FL_OpenSysFileRead(cacheFileName);
    cachedDataLoaded = LoadCachedData(strm);
    if (!cachedDataLoaded) {
      GCon->Logf("cache data is obsolete or in invalid format");
      if (!glNodesFound) NeedNodesBuild = true;
    }
    delete strm;
  }

  double NodesTime = -Sys_Time();
  // and again; sorry!
  if (!cachedDataLoaded) {
    if (NeedNodesBuild) {
      GCon->Logf("building GL nodes...");
      BuildNodes();
      saveCachedData = true;
    } else if (UseComprGLNodes) {
      if (!LoadCompressedGLNodes(CompressedGLNodesLump, GLNodesHdr)) {
        GCon->Logf("rebuilding GL nodes...");
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
  NodesTime += Sys_Time();

  // load blockmap
  double BlockMapTime = -Sys_Time();
  if (!BlockMapLump) {
    LoadBlockMap(BlockmapLump);
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
  if (loader_cache_data && saveCachedData && sha512valid && TotalTime+Sys_Time() > loader_cache_time_limit) {
    VStream *strm = FL_OpenSysFileWrite(cacheFileName);
    SaveCachedData(strm);
    delete strm;
  }
  doCacheCleanup();


  // ACS object code
  double AcsTime = -Sys_Time();
  LoadACScripts(BehaviorLump);
  AcsTime += Sys_Time();

  double GroupLinesTime = -Sys_Time();
  GroupLines();
  GroupLinesTime += Sys_Time();

  double FloodZonesTime = -Sys_Time();
  FloodZones();
  FloodZonesTime += Sys_Time();

  FixDeepWaters();

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
  for (int i = 0; i < NumLines; i++) {
    line_t *Line = Lines+i;
    if (!Line->normal.x) {
      Sides[Line->sidenum[0]].Light = MInfo.HorizWallShade;
      if (Line->sidenum[1] >= 0) {
        Sides[Lines[i].sidenum[1]].Light = MInfo.HorizWallShade;
      }
    } else if (!Line->normal.y) {
      Sides[Line->sidenum[0]].Light = MInfo.VertWallShade;
      if (Line->sidenum[1] >= 0) {
        Sides[Lines[i].sidenum[1]].Light = MInfo.VertWallShade;
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

  //
  // End of map lump processing
  //
  if (AuxiliaryMap) {
    // Close the auxiliary file.
    W_CloseAuxiliary();
  }

  DecalProcessingTime = -Sys_Time();
  PostProcessForDecals();
  DecalProcessingTime += Sys_Time();

  TotalTime += Sys_Time();
  if (true || show_level_load_times) {
    GCon->Logf("-------");
    GCon->Logf("Level loadded in %f", TotalTime);
    //GCon->Logf("Initialisation   %f", InitTime);
    GCon->Logf("Node build       %f", NodeBuildTime);
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

  unguard;
}


struct VectorInfo {
  float xy[2];
  int aidx;
  int lidx; // linedef index
  VectorInfo *next;

  inline bool operator == (const VectorInfo &vi) const { return (xy[0] == vi.xy[0] && xy[1] == vi.xy[1]); }
  inline bool operator != (const VectorInfo &vi) const { return (xy[0] != vi.xy[0] || xy[1] != vi.xy[1]); }
};

//inline vuint32 GetTypeHash (const VectorInfo &vi) { return fnvHashBuf(vi.xy, sizeof(vi.xy)); }
//inline vuint32 GetTypeHash (const VectorInfo *vi) { return fnvHashBuf(vi->xy, sizeof(vi->xy)); }
inline vuint32 GetTypeHash (const VectorInfo &vi) { return joaatHashBuf(vi.xy, sizeof(vi.xy)); }


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
  TMapNC<VectorInfo, int> vmap; // in tarray
  TArray<VectorInfo> va;
  va.SetLength(NumLines*2);
  line_t *ld = Lines;
  for (int i = 0; i < NumLines; ++i, ++ld) {
    ld->decalMark = 0;
    ld->v1linesCount = ld->v2linesCount = 0;
    ld->v1lines = ld->v2lines = nullptr;
    for (int vn = 0; vn < 2; ++vn) {
      VectorInfo *vi = &va[i*2+vn];
      const TVec *vertex = (vn == 0 ? ld->v1 : ld->v2);
      vi->xy[0] = vertex->x;
      vi->xy[1] = vertex->y;
      vi->aidx = i*2+vn;
      vi->lidx = i;
      vi->next = nullptr;
      auto vaidxp = vmap.find(*vi);
      if (vaidxp) {
        VectorInfo *cv = &va[*vaidxp];
        while (cv->next) {
          if (*cv != *vi) Sys_Error("VLevel::BuildDecalsVVList: OOPS(0)!");
          cv = cv->next;
        }
        if (*cv != *vi) Sys_Error("VLevel::BuildDecalsVVList: OOPS(1)!");
        cv->next = vi;
      } else {
        vmap.put(*vi, i*2+vn);
      }
    }
  }

  line_t **wklist = new line_t *[NumLines*2];
  vuint8 *wkhit = new vuint8[NumLines];

  // fill linedef lists
  ld = Lines;
  for (int i = 0; i < NumLines; ++i, ++ld) {
    for (int vn = 0; vn < 2; ++vn) {
      int count = 0;
      memset(wkhit, 0, NumLines*sizeof(wkhit[0]));
      wkhit[i] = 1;
      for (int curvn = 0; curvn < 2; ++curvn) {
        VectorInfo *vi = &va[i*2+curvn];
        auto vaidxp = vmap.find(*vi);
        if (!vaidxp) continue; //Sys_Error("VLevel::BuildDecalsVVList: internal error (0)");
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

int VLevel::FindGLNodes(VName name) const
{
  guard(VLevel::FindGLNodes);
  if (VStr::Length(*name) < 6)
  {
    return W_CheckNumForName(VName(va("gl_%s", *name), VName::AddLower8));
  }

  //  Long map name, check GL_LEVEL lumps.
  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0;
    Lump = W_IterateNS(Lump, WADNS_Global))
  {
    if (W_LumpName(Lump) != NAME_gl_level)
    {
      continue;
    }
    if (W_LumpLength(Lump) < 12)
    {
      //  Lump is too short.
      continue;
    }
    char Buf[16];
    VStream *Strm = W_CreateLumpReaderNum(Lump);
    Strm->Serialise(Buf, Strm->TotalSize() < 16 ? Strm->TotalSize() : 16);
    delete Strm;
    Strm = nullptr;
    if (memcmp(Buf, "LEVEL=", 6))
    {
      //  LEVEL keyword expected, but missing.
      continue;
    }
    for (int i = 11; i < 14; i++)
    {
      if (Buf[i] == '\n' || Buf[i] == '\r')
      {
        Buf[i] = 0;
        break;
      }
    }
    Buf[14] = 0;
    if (!VStr::ICmp(Buf + 6, *name))
    {
      return Lump;
    }
  }
  return -1;
  unguard;
}

//==========================================================================
//
//  VLevel::LoadVertexes
//
//==========================================================================

void VLevel::LoadVertexes(int Lump, int GLLump, int &NumBaseVerts)
{
  guard(VLevel::LoadVertexes);
  int GlFormat = 0;
  if (GLLump >= 0)
  {
    //  Read header of the GL vertexes lump and determinte GL vertex format.
    char Magic[4];
    W_ReadFromLump(GLLump, Magic, 0, 4);
    GlFormat = !VStr::NCmp((char*)Magic, GL_V2_MAGIC, 4) ? 2 :
      !VStr::NCmp((char*)Magic, GL_V5_MAGIC, 4) ? 5 : 1;
    if (GlFormat ==  5)
    {
      LevelFlags |= LF_GLNodesV5;
    }
  }

  //  Determine number of vertexes: total lump length / vertex record length.
  NumBaseVerts = W_LumpLength(Lump) / 4;
  int NumGLVerts = GlFormat == 0 ? 0 : GlFormat == 1 ?
    (W_LumpLength(GLLump) / 4) : ((W_LumpLength(GLLump) - 4) / 8);
  NumVertexes = NumBaseVerts + NumGLVerts;

  //  Allocate memory for vertexes.
  Vertexes = new vertex_t[NumVertexes];
  if (NumVertexes) memset((void *)Vertexes, 0, sizeof(vertex_t)*NumVertexes);

  //  Load base vertexes.
  VStream *Strm = W_CreateLumpReaderNum(Lump);
  vertex_t *pDst = Vertexes;
  for (int i = 0; i < NumBaseVerts; i++, pDst++)
  {
    vint16 x, y;
    *Strm << x << y;
    *pDst = TVec(x, y, 0);
  }
  delete Strm;
  Strm = nullptr;

  if (GLLump >= 0)
  {
    //  Load GL vertexes.
    Strm = W_CreateLumpReaderNum(GLLump);
    if (GlFormat == 1)
    {
      //  GL version 1 vertexes, same as normal ones.
      for (int i = 0; i < NumGLVerts; i++, pDst++)
      {
        vint16 x, y;
        *Strm << x << y;
        *pDst = TVec(x, y, 0);
      }
    }
    else
    {
      //  GL version 2 or greater vertexes, as fixed point.
      Strm->Seek(4);
      for (int i = 0; i < NumGLVerts; i++, pDst++)
      {
        vint32 x, y;
        *Strm << x << y;
        *pDst = TVec(x / 65536.0, y / 65536.0, 0);
      }
    }
    delete Strm;
    Strm = nullptr;
  }
  unguard;
}

//==========================================================================
//
//  VLevel::LoadSectors
//
//==========================================================================

void VLevel::LoadSectors(int Lump)
{
  guard(VLevel::LoadSectors);
  //  Allocate memory for sectors.
  NumSectors = W_LumpLength(Lump) / 26;
  Sectors = new sector_t[NumSectors];
  memset((void *)Sectors, 0, sizeof(sector_t) * NumSectors);

  //  Load sectors.
  VStream *Strm = W_CreateLumpReaderNum(Lump);
  sector_t *ss = Sectors;
  for (int i = 0; i < NumSectors; i++, ss++)
  {
    //  Read data.
    vint16 floorheight, ceilingheight, lightlevel, special, tag;
    char floorpic[9];
    char ceilingpic[9];
    memset(floorpic, 0, sizeof(floorpic));
    memset(ceilingpic, 0, sizeof(ceilingpic));
    *Strm << floorheight << ceilingheight;
    Strm->Serialise(floorpic, 8);
    Strm->Serialise(ceilingpic, 8);
    *Strm << lightlevel << special << tag;

    //  Floor
    ss->floor.Set(TVec(0, 0, 1), floorheight);
    ss->floor.TexZ = floorheight;
    ss->floor.pic = TexNumForName(floorpic, TEXTYPE_Flat);
    ss->floor.xoffs = 0;
    ss->floor.yoffs = 0;
    ss->floor.XScale = 1.0;
    ss->floor.YScale = 1.0;
    ss->floor.Angle = 0.0;
    ss->floor.minz = floorheight;
    ss->floor.maxz = floorheight;
    ss->floor.Alpha = 1.0;
    ss->floor.MirrorAlpha = 1.0;
    ss->floor.LightSourceSector = -1;

    //  Ceiling
    ss->ceiling.Set(TVec(0, 0, -1), -ceilingheight);
    ss->ceiling.TexZ = ceilingheight;
    ss->ceiling.pic = TexNumForName(ceilingpic, TEXTYPE_Flat);
    ss->ceiling.xoffs = 0;
    ss->ceiling.yoffs = 0;
    ss->ceiling.XScale = 1.0;
    ss->ceiling.YScale = 1.0;
    ss->ceiling.Angle = 0.0;
    ss->ceiling.minz = ceilingheight;
    ss->ceiling.maxz = ceilingheight;
    ss->ceiling.Alpha = 1.0;
    ss->ceiling.MirrorAlpha = 1.0;
    ss->ceiling.LightSourceSector = -1;

    //  Params
    ss->params.lightlevel = lightlevel;
    ss->params.LightColour = 0x00ffffff;
    //  Region
    sec_region_t *region = new sec_region_t;
    memset((void *)region, 0, sizeof(*region));
    region->floor = &ss->floor;
    region->ceiling = &ss->ceiling;
    region->params = &ss->params;
    ss->topregion = region;
    ss->botregion = region;

    ss->special = special;
    ss->tag = tag;

    ss->seqType = -1; // default seqType
    ss->Gravity = 1.0;  // default sector gravity of 1.0
    ss->Zone = -1;
  }
  delete Strm;
  Strm = nullptr;
  HashSectors();
  unguard;
}

//==========================================================================
//
//  VLevel::CreateSides
//
//==========================================================================

void VLevel::CreateSides()
{
  guard(VLevel::CreateSides);
  //  Perform side index and two-sided flag checks and count number of
  // sides needed.
  int NumNewSides = 0;
  line_t *Line = Lines;
  for (int i = 0; i < NumLines; i++, Line++)
  {
    if (Line->sidenum[0] == -1)
    {
      if (strict_level_errors)
      {
        Host_Error("Bad WAD: Line %d has no front side", i);
      }
      else
      {
        GCon->Logf("Bad WAD: Line %d has no front side", i);
        Line->sidenum[0] = 0;
      }
    }
    if (Line->sidenum[0] < 0 || Line->sidenum[0] >= NumSides)
    {
      Host_Error("Bad side-def index %d", Line->sidenum[0]);
    }
    ++NumNewSides;

    if (Line->sidenum[1] != -1) {
      // has second side
      if (Line->sidenum[1] < 0 || Line->sidenum[1] >= NumSides) Host_Error("Bad sidedef index %d for linedef #%d", Line->sidenum[1], i);
      // just a warning (and a fix)
      if (!(Line->flags&ML_TWOSIDED)) {
        GCon->Logf("WARNING: linedef #%d marked as two-sided but has no TWO-SIDED flag set", i);
        Line->flags |= ML_TWOSIDED; //k8: we need to set this, or clipper will glitch
      }
      ++NumNewSides;
    } else {
      // no second side, but marked as two-sided
      if (Line->flags&ML_TWOSIDED) {
        //if (strict_level_errors) Host_Error("Bad WAD: Line %d is marked as TWO-SIDED but has only one side", i);
        GCon->Logf("WARNING: linedef #%d is marked as TWO-SIDED but has only one side", i);
        Line->flags &= ~ML_TWOSIDED;
      }
    }
    //fprintf(stderr, "linedef #%d: sides=(%d,%d); two-sided=%s\n", i, Line->sidenum[0], Line->sidenum[1], (Line->flags&ML_TWOSIDED ? "tan" : "ona"));
  }

  //  Allocate memory for side defs.
  Sides = new side_t[NumNewSides+1];
  memset((void *)Sides, 0, sizeof(side_t) * (NumNewSides+1));

  int CurrentSide = 0;
  Line = Lines;
  for (int i = 0; i < NumLines; i++, Line++)
  {
    Sides[CurrentSide].BottomTexture = Line->sidenum[0];
    Sides[CurrentSide].LineNum = i;
    Line->sidenum[0] = CurrentSide;
    CurrentSide++;
    if (Line->sidenum[1] != -1)
    {
      Sides[CurrentSide].BottomTexture = Line->sidenum[1];
      Sides[CurrentSide].LineNum = i;
      Line->sidenum[1] = CurrentSide;
      CurrentSide++;
    }

    //  Assign line specials to sidedefs midtexture and arg1 to toptexture.
    if (Line->special == LNSPEC_StaticInit && Line->arg2 != 1)
    {
      continue;
    }
    Sides[Line->sidenum[0]].MidTexture = Line->special;
    Sides[Line->sidenum[0]].TopTexture = Line->arg1;
  }
  check(CurrentSide == NumNewSides);

  NumSides = NumNewSides;
  unguard;
}

//==========================================================================
//
//  VLevel::LoadSideDefs
//
//==========================================================================

void VLevel::LoadSideDefs(int Lump)
{
  guard(VLevel::LoadSideDefs);
  NumSides = W_LumpLength(Lump) / 30;
  CreateSides();

  //  Load data.
  VStream *Strm = W_CreateLumpReaderNum(Lump);
  side_t *sd = Sides;
  for (int i = 0; i < NumSides; i++, sd++)
  {
    Strm->Seek(sd->BottomTexture * 30);
    vint16 textureoffset;
    vint16 rowoffset;
    char toptexture[9];
    char bottomtexture[9];
    char midtexture[9];
    memset(toptexture, 0, sizeof(toptexture));
    memset(bottomtexture, 0, sizeof(bottomtexture));
    memset(midtexture, 0, sizeof(midtexture));
    vint16 sector;
    *Strm << textureoffset << rowoffset;
    Strm->Serialise(toptexture, 8);
    Strm->Serialise(bottomtexture, 8);
    Strm->Serialise(midtexture, 8);
    *Strm << sector;

    if (sector < 0 || sector >= NumSectors)
    {
      Host_Error("Bad sector index %d", sector);
    }

    sd->TopTextureOffset = textureoffset;
    sd->BotTextureOffset = textureoffset;
    sd->MidTextureOffset = textureoffset;
    sd->TopRowOffset = rowoffset;
    sd->BotRowOffset = rowoffset;
    sd->MidRowOffset = rowoffset;
    sd->Sector = &Sectors[sector];

    switch (sd->MidTexture)
    {
    case LNSPEC_LineTranslucent:
      //  In BOOM midtexture can be translucency table lump name.
      sd->MidTexture = GTextureManager.CheckNumForName(
        VName(midtexture, VName::AddLower8),
        TEXTYPE_Wall, true, true);
      if (sd->MidTexture == -1)
      {
        sd->MidTexture = 0;
      }
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
        int TmpTop = TexNumOrColour(toptexture, TEXTYPE_Wall,
          HaveCol, Col);
        sd->BottomTexture = TexNumOrColour(bottomtexture, TEXTYPE_Wall,
          HaveFade, Fade);
        if (HaveCol || HaveFade)
        {
          for (int j = 0; j < NumSectors; j++)
          {
            if (Sectors[j].tag == sd->TopTexture)
            {
              if (HaveCol)
              {
                Sectors[j].params.LightColour = Col;
              }
              if (HaveFade)
              {
                Sectors[j].params.Fade = Fade;
              }
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
  delete Strm;
  Strm = nullptr;
  unguard;
}

//==========================================================================
//
//  VLevel::LoadLineDefs1
//
//  For Doom and Heretic
//
//==========================================================================

void VLevel::LoadLineDefs1(int Lump, int NumBaseVerts, const mapInfo_t &MInfo)
{
  guard(VLevel::LoadLineDefs1);
  NumLines = W_LumpLength(Lump) / 14;
  Lines = new line_t[NumLines];
  memset((void *)Lines, 0, sizeof(line_t) * NumLines);

  VStream *Strm = W_CreateLumpReaderNum(Lump);
  line_t *ld = Lines;
  for (int i = 0; i < NumLines; i++, ld++)
  {
    vuint16 v1, v2, flags;
    vuint16 special, tag;
    vuint16 side0, side1;
    *Strm << v1 << v2 << flags << special << tag << side0 << side1;

    if (/*v1 < 0 ||*/ v1 >= NumBaseVerts)
    {
      Host_Error("Bad vertex index %d (00)", v1);
    }
    if (/*v2 < 0 ||*/ v2 >= NumBaseVerts)
    {
      Host_Error("Bad vertex index %d (01)", v2);
    }

    ld->flags = flags;
    ld->special = special;
    ld->arg1 = (tag == 0xffff ? -1 : tag);
    ld->v1 = &Vertexes[v1];
    ld->v2 = &Vertexes[v2];
    ld->sidenum[0] = side0 == 0xffff ? -1 : side0;
    ld->sidenum[1] = side1 == 0xffff ? -1 : side1;

    ld->alpha = 1.0;
    ld->LineTag = -1;

    if (MInfo.Flags & MAPINFOF_ClipMidTex)
    {
      ld->flags |= ML_CLIP_MIDTEX;
    }
    if (MInfo.Flags & MAPINFOF_WrapMidTex)
    {
      ld->flags |= ML_WRAP_MIDTEX;
    }
  }
  delete Strm;
  Strm = nullptr;
  unguard;
}

//==========================================================================
//
//  VLevel::LoadLineDefs2
//
//  For Hexen
//
//==========================================================================

void VLevel::LoadLineDefs2(int Lump, int NumBaseVerts, const mapInfo_t &MInfo)
{
  guard(VLevel::LoadLineDefs2);
  NumLines = W_LumpLength(Lump) / 16;
  Lines = new line_t[NumLines];
  memset((void *)Lines, 0, sizeof(line_t) * NumLines);

  VStream *Strm = W_CreateLumpReaderNum(Lump);
  line_t *ld = Lines;
  for (int i = 0; i < NumLines; i++, ld++)
  {
    vuint16 v1, v2, flags;
    vuint8 special, arg1, arg2, arg3, arg4, arg5;
    vuint16 side0, side1;
    *Strm << v1 << v2 << flags << special << arg1 << arg2 << arg3 << arg4
      << arg5 << side0 << side1;

    if (/*v1 < 0 ||*/ v1 >= NumBaseVerts)
    {
      Host_Error("Bad vertex index %d (02)", v1);
    }
    if (/*v2 < 0 ||*/ v2 >= NumBaseVerts)
    {
      Host_Error("Bad vertex index %d (03)", v2);
    }

    ld->flags = flags & ~ML_SPAC_MASK;
    int Spac = (flags & ML_SPAC_MASK) >> ML_SPAC_SHIFT;
    if (Spac == 7)
    {
      ld->SpacFlags = SPAC_Impact | SPAC_PCross;
    }
    else
    {
      ld->SpacFlags = 1 << Spac;
    }

    // New line special info ...
    ld->special = special;
    ld->arg1 = arg1;
    ld->arg2 = arg2;
    ld->arg3 = arg3;
    ld->arg4 = arg4;
    ld->arg5 = arg5;

    ld->v1 = &Vertexes[v1];
    ld->v2 = &Vertexes[v2];
    ld->sidenum[0] = side0 == 0xffff ? -1 : side0;
    ld->sidenum[1] = side1 == 0xffff ? -1 : side1;

    ld->alpha = 1.0;
    ld->LineTag = -1;

    if (MInfo.Flags & MAPINFOF_ClipMidTex)
    {
      ld->flags |= ML_CLIP_MIDTEX;
    }
    if (MInfo.Flags & MAPINFOF_WrapMidTex)
    {
      ld->flags |= ML_WRAP_MIDTEX;
    }
  }
  delete Strm;
  Strm = nullptr;
  unguard;
}

//==========================================================================
//
//  VLevel::FinaliseLines
//
//==========================================================================

void VLevel::FinaliseLines()
{
  guard(VLevel::FinaliseLines);
  line_t *Line = Lines;
  for (int i = 0; i < NumLines; i++, Line++)
  {
    //  Calculate line's plane, slopetype, etc.
    CalcLine(Line);

    //  Set up sector references.
    Line->frontsector = Sides[Line->sidenum[0]].Sector;
    if (Line->sidenum[1] != -1)
    {
      Line->backsector = Sides[Line->sidenum[1]].Sector;
    }
    else
    {
      Line->backsector = nullptr;
    }
  }
  unguard;
}

//==========================================================================
//
//  VLevel::LoadGLSegs
//
//==========================================================================

void VLevel::LoadGLSegs(int Lump, int NumBaseVerts)
{
  guard(VLevel::LoadGLSegs);
  vertex_t *GLVertexes = Vertexes + NumBaseVerts;
  int NumGLVertexes = NumVertexes - NumBaseVerts;

  //  Determine format of the segs data.
  int Format;
  vuint32 GLVertFlag;
  if (LevelFlags & LF_GLNodesV5)
  {
    Format = 5;
    NumSegs = W_LumpLength(Lump) / 16;
    GLVertFlag = GL_VERTEX_V5;
  }
  else
  {
    char Header[4];
    W_ReadFromLump(Lump, Header, 0, 4);
    if (!VStr::NCmp(Header, GL_V3_MAGIC, 4))
    {
      Format = 3;
      NumSegs = (W_LumpLength(Lump) - 4) / 16;
      GLVertFlag = GL_VERTEX_V3;
    }
    else
    {
      Format = 1;
      NumSegs = W_LumpLength(Lump) / 10;
      GLVertFlag = GL_VERTEX;
    }
  }

  //  Allocate memory for segs data.
  Segs = new seg_t[NumSegs];
  memset((void *)Segs, 0, sizeof(seg_t) * NumSegs);

  //  Read data.
  VStream *Strm = W_CreateLumpReaderNum(Lump);
  if (Format == 3)
  {
    Strm->Seek(4);
  }
  seg_t *li = Segs;
  for (int i = 0; i < NumSegs; i++, li++)
  {
    vuint32 v1num;
    vuint32 v2num;
    vint16 linedef; // -1 for minisegs
    vint16 side;
    vint16 partner; // -1 on one-sided walls

    if (Format < 3)
    {
      vuint16 v1, v2;
      *Strm << v1 << v2 << linedef << side << partner;
      v1num = v1;
      v2num = v2;
    }
    else
    {
      vuint32 v1, v2;
      vint16 flags;
      *Strm << v1 << v2 << linedef << flags << partner;
      v1num = v1;
      v2num = v2;
      side = flags & GL_SEG_FLAG_SIDE;
    }

    if (v1num & GLVertFlag)
    {
      v1num ^= GLVertFlag;
      if (v1num >= (vuint32)NumGLVertexes)
        Host_Error("Bad GL vertex index %d", v1num);
      li->v1 = &GLVertexes[v1num];
    }
    else
    {
      if (v1num >= (vuint32)NumVertexes)
        Host_Error("Bad vertex index %d (04)", v1num);
      li->v1 = &Vertexes[v1num];
    }
    if (v2num & GLVertFlag)
    {
      v2num ^= GLVertFlag;
      if (v2num >= (vuint32)NumGLVertexes)
        Host_Error("Bad GL vertex index %d", v2num);
      li->v2 = &GLVertexes[v2num];
    }
    else
    {
      if (v2num >= (vuint32)NumVertexes)
        Host_Error("Bad vertex index %d (05)", v2num);
      li->v2 = &Vertexes[v2num];
    }

    if (linedef >= 0)
    {
      line_t *ldef = &Lines[linedef];
      li->linedef = ldef;
      li->sidedef = &Sides[ldef->sidenum[side]];
      li->frontsector = Sides[ldef->sidenum[side]].Sector;

      if (ldef->flags & ML_TWOSIDED)
      {
        li->backsector = Sides[ldef->sidenum[side ^ 1]].Sector;
      }

      if (side)
      {
        li->offset = Length(*li->v1 - *ldef->v2);
      }
      else
      {
        li->offset = Length(*li->v1 - *ldef->v1);
      }
      li->length = Length(*li->v2 - *li->v1);
      li->side = side;
    }

    //  Assign partner (we need it for self-referencing deep water)
    li->partner = (partner >= 0 && partner < NumSegs ? &Segs[partner] : nullptr);

    //  Calc seg's plane params
    CalcSeg(li);
  }
  delete Strm;
  Strm = nullptr;
  unguard;
}

//==========================================================================
//
//  VLevel::LoadSubsectors
//
//==========================================================================

void VLevel::LoadSubsectors(int Lump)
{
  guard(VLevel::LoadSubsectors);
  //  Determine format of the subsectors data.
  int Format;
  if (LevelFlags & LF_GLNodesV5)
  {
    Format = 5;
    NumSubsectors = W_LumpLength(Lump) / 8;
  }
  else
  {
    char Header[4];
    W_ReadFromLump(Lump, Header, 0, 4);
    if (!VStr::NCmp(Header, GL_V3_MAGIC, 4))
    {
      Format = 3;
      NumSubsectors = (W_LumpLength(Lump) - 4) / 8;
    }
    else
    {
      Format = 1;
      NumSubsectors = W_LumpLength(Lump) / 4;
    }
  }

  //  Allocate memory for subsectors.
  Subsectors = new subsector_t[NumSubsectors];
  memset((void *)Subsectors, 0, sizeof(subsector_t) * NumSubsectors);

  //  Read data.
  VStream *Strm = W_CreateLumpReaderNum(Lump);
  if (Format == 3)
  {
    Strm->Seek(4);
  }
  subsector_t *ss = Subsectors;
  for (int i = 0; i < NumSubsectors; i++, ss++)
  {
    if (Format < 3)
    {
      vuint16 numsegs, firstseg;
      *Strm << numsegs << firstseg;
      ss->numlines = numsegs;
      ss->firstline = firstseg;
    }
    else
    {
      vint32 numsegs, firstseg;
      *Strm << numsegs << firstseg;
      ss->numlines = numsegs;
      ss->firstline = firstseg;
    }

    if (ss->firstline < 0 || ss->firstline >= NumSegs)
      Host_Error("Bad seg index %d", ss->firstline);
    if (ss->numlines <= 0 || ss->firstline + ss->numlines > NumSegs)
      Host_Error("Bad segs range %d %d", ss->firstline, ss->numlines);

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
  for (int f = 0; f < NumSegs; ++f) {
    if (!Segs[f].front_sub) GCon->Logf("Seg %d: front_sub is not set!", f);
  }
  delete Strm;
  Strm = nullptr;
  unguard;
}

//==========================================================================
//
//  VLevel::LoadNodes
//
//==========================================================================

void VLevel::LoadNodes(int Lump)
{
  guard(VLevel::LoadNodes);
  if (LevelFlags & LF_GLNodesV5)
  {
    NumNodes = W_LumpLength(Lump) / 32;
  }
  else
  {
    NumNodes = W_LumpLength(Lump) / 28;
  }
  Nodes = new node_t[NumNodes];
  memset((void *)Nodes, 0, sizeof(node_t) * NumNodes);

  VStream *Strm = W_CreateLumpReaderNum(Lump);
  node_t *no = Nodes;
  for (int i = 0; i < NumNodes; i++, no++)
  {
    vint16 x, y, dx, dy;
    vint16 bbox[2][4];
    vuint32 children[2];
    *Strm << x << y << dx << dy
      << bbox[0][0] << bbox[0][1] << bbox[0][2] << bbox[0][3]
      << bbox[1][0] << bbox[1][1] << bbox[1][2] << bbox[1][3];
    if (LevelFlags & LF_GLNodesV5)
    {
      *Strm << children[0] << children[1];
    }
    else
    {
      vuint16 child0, child1;
      *Strm << child0 << child1;
      children[0] = child0;
      if (children[0] & NF_SUBSECTOR_OLD)
        children[0] ^= NF_SUBSECTOR_OLD | NF_SUBSECTOR;
      children[1] = child1;
      if (children[1] & NF_SUBSECTOR_OLD)
        children[1] ^= NF_SUBSECTOR_OLD | NF_SUBSECTOR;
    }

    if (dx == 0 && dy == 0) {
      //Host_Error("invalid nodes (dir)");
      GCon->Log("invalid BSP node (zero direction)");
      no->SetPointDirXY(TVec(x, y, 0), TVec(0.001f, 0, 0));
    } else {
      no->SetPointDirXY(TVec(x, y, 0), TVec(dx, dy, 0));
    }

    for (int j = 0; j < 2; j++)
    {
      no->children[j] = children[j];
      no->bbox[j][0] = bbox[j][BOXLEFT];
      no->bbox[j][1] = bbox[j][BOXBOTTOM];
      no->bbox[j][2] = -32768.0;
      no->bbox[j][3] = bbox[j][BOXRIGHT];
      no->bbox[j][4] = bbox[j][BOXTOP];
      no->bbox[j][5] = 32768.0;
    }
  }
  delete Strm;
  Strm = nullptr;
  unguard;
}

//==========================================================================
//
//  VLevel::LoadPVS
//
//==========================================================================

void VLevel::LoadPVS(int Lump)
{
  guard(VLevel::LoadPVS);
  if (W_LumpName(Lump) != NAME_gl_pvs || W_LumpLength(Lump) == 0)
  {
    GCon->Logf(NAME_Dev, "Empty or missing PVS lump");
    if (NoVis == nullptr && VisData == nullptr) BuildPVS();
    /*
    VisData = nullptr;
    NoVis = new vuint8[(NumSubsectors + 7) / 8];
    memset(NoVis, 0xff, (NumSubsectors + 7) / 8);
    */
  }
  else
  {
    //if (NoVis == nullptr && VisData == nullptr) BuildPVS();
    byte *VisDataNew = new byte[W_LumpLength(Lump)];
    VStream *Strm = W_CreateLumpReaderNum(Lump);
    Strm->Serialise(VisDataNew, W_LumpLength(Lump));
    if (Strm->IsError() || W_LumpLength(Lump) < ((NumSubsectors+7)>>3)*NumSubsectors) {
      delete [] VisDataNew;
    } else {
      delete [] VisData;
      delete [] NoVis;
      VisData = VisDataNew;
    }
    delete Strm;
  }
  unguard;
}


//==========================================================================
//
//  VLevel::LoadCompressedGLNodes
//
//==========================================================================
bool VLevel::LoadCompressedGLNodes (int Lump, char hdr[4]) {
  guard(VLevel::LoadCompressedGLNodes);
  VStream *BaseStrm = W_CreateLumpReaderNum(Lump);

  // read header
  BaseStrm->Serialise(hdr, 4);
  if (BaseStrm->IsError()) {
    delete BaseStrm;
    GCon->Logf("WARNING: error reading GL nodes (VaVoom will use internal node builder)");
    return false;
  }

  if ((hdr[0] == 'Z' || hdr[0] == 'X') &&
      hdr[1] == 'G' && hdr[2] == 'L' &&
      (hdr[3] == 'N' || hdr[3] == '2' || hdr[3] == '3'))
  {
    // ok
  } else {
    delete BaseStrm;
    GCon->Logf("WARNING: invalid GL nodes signature (VaVoom will use internal node builder)");
    return false;
  }

  // create reader stream for the zipped data
  vuint8 *TmpData = new vuint8[BaseStrm->TotalSize()-4];
  BaseStrm->Serialise(TmpData, BaseStrm->TotalSize()-4);
  if (BaseStrm->IsError()) {
    delete BaseStrm;
    GCon->Logf("WARNING: error reading GL nodes (VaVoom will use internal node builder)");
    return false;
  }
  VStream *DataStrm = new VMemoryStream(TmpData, BaseStrm->TotalSize()-4);
  delete[] TmpData;
  TmpData = nullptr;
  delete BaseStrm;
  BaseStrm = nullptr;

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
      GCon->Logf("WARNING: this obsolete version of GL nodes is disabled (VaVoom will use internal node builder)");
      return false;
  }

  GCon->Logf("NOTE: found %scompressed GL nodes, type %d", (hdr[0] == 'X' ? "un" : ""), type);

  if (!nodes_allow_compressed) {
    delete Strm;
    delete DataStrm;
    GCon->Logf("WARNING: this new version of GL nodes is disabled (VaVoom will use internal node builder)");
    return false;
  }

  // read extra vertex data
  guard(VLevel::LoadCompressedGLNodes::Vertexes);
  vuint32 OrgVerts, NewVerts;
  *Strm << OrgVerts << NewVerts;

  if (Strm->IsError()) {
    delete Strm;
    delete DataStrm;
    GCon->Logf("WARNING: error reading GL nodes (VaVoom will use internal node builder)");
    return false;
  }

  if (OrgVerts != (vuint32)NumVertexes) {
    delete Strm;
    delete DataStrm;
    GCon->Logf("WARNING: error reading GL nodes (got %u vertexes, expected %d vertexes)", OrgVerts, NumVertexes);
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
  unguard;

  // load subsectors
  int FirstSeg = 0;
  guard(VLevel::LoadCompressedGLNodes::Subsectors);
  NumSubsectors = Streamer<vuint32>(*Strm);
  if (NumSubsectors == 0 || NumSubsectors > 0x1fffffff) Host_Error("error reading GL nodes (got %u subsectors)", NumSubsectors);
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
  if (FirstSeg == 0 || FirstSeg > 0x1fffffff) Host_Error("error reading GL nodes (counted %i subsegs)", FirstSeg);
  unguard;

  // load segs
  guard(VLevel::LoadCompressedGLNodes::Segs);
  NumSegs = Streamer<vuint32>(*Strm);
  if (NumSegs != FirstSeg) Host_Error("error reading GL nodes (got %d segs, expected %d segs)", NumSegs, FirstSeg);

  Segs = new seg_t[NumSegs];
  memset((void *)Segs, 0, sizeof(seg_t)*NumSegs);
  /*if (type == 0) {
    seg_t *li = Segs;
    for (int i = 0; i < NumSegs; ++i, ++li) {
      vuint32 v1;
      vuint32 partner;
      vuint16 linedef;
      vuint8 side;

      *Strm << v1 << partner << linedef << side;

      if (v1 >= (vuint32)NumVertexes) Host_Error("Bad vertex index %d (06)", v1);
      li->v1 = &Vertexes[v1];

      // assign partner (we need it for self-referencing deep water)
      li->partner = (partner < (unsigned)NumSegs ? &Segs[partner] : nullptr);

      if (linedef != 0xffff) {
        if (linedef >= NumLines) Host_Error("Bad linedef index %d", linedef);
        if (side > 1) Host_Error("Bad seg side %d", side);

        line_t *ldef = &Lines[linedef];
        li->linedef = ldef;
        li->sidedef = &Sides[ldef->sidenum[side]];
        li->frontsector = Sides[ldef->sidenum[side]].Sector;

        if ((ldef->flags&ML_TWOSIDED) != 0 && ldef->sidenum[side^1]) {
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
      } else {
        / *
        seg->linedef = nullptr;
        seg->sidedef = nullptr;
        seg->frontsector = seg->backsector = (Segs+Subsectors[i].firstline)->frontsector;
        * /
      }
    }
  } else*/
  {
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
          li->sidedef = &Sides[ldef->sidenum[side]];
          li->frontsector = Sides[ldef->sidenum[side]].Sector;

          if ((ldef->flags&ML_TWOSIDED) != 0 && ldef->sidenum[side^1]) {
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
        } else {
          li->linedef = nullptr;
          li->sidedef = nullptr;
          li->frontsector = li->backsector = (Segs+Subsectors[i].firstline)->frontsector;
        }
      }
    }
  }
  unguard;

  // load nodes
  guard(VLevel::LoadCompressedGLNodes::Nodes);
  NumNodes = Streamer<vuint32>(*Strm);
  if (NumNodes == 0 || NumNodes > 0x1fffffff) Host_Error("error reading GL nodes (got %u nodes)", NumNodes);
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
      no->bbox[j][2] = -32768.0;
      no->bbox[j][3] = bbox[j][BOXRIGHT];
      no->bbox[j][4] = bbox[j][BOXTOP];
      no->bbox[j][5] = 32768.0;
    }
  }
  unguard;

  // set v2 of the segs
  guard(VLevel::LoadCompressedGLNodes::Set up seg v2);
  subsector_t *Sub = Subsectors;
  for (int i = 0; i < NumSubsectors; ++i, ++Sub) {
    seg_t *Seg = Segs+Sub->firstline;
    for (int j = 0; j < Sub->numlines-1; ++j, ++Seg) {
      Seg->v2 = Seg[1].v1;
    }
    Seg->v2 = Segs[Sub->firstline].v1;
  }
  unguard;

  guard(VLevel::LoadCompressedGLNodes::Calc segs);
  seg_t *li = Segs;
  for (int i = 0; i < NumSegs; ++i, ++li) {
    // calc seg's plane params
    li->length = Length(*li->v2 - *li->v1);
    CalcSeg(li);
  }
  unguard;

  guard(Calc subsectors);
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
  unguard;

  // create dummy VIS data
  // k8: no need to do this, main loader will take care of it
  /*
  VisData = nullptr;
  NoVis = new vuint8[(NumSubsectors+7)/8];
  memset(NoVis, 0xff, (NumSubsectors+7)/8);
  */

  delete Strm;
  delete DataStrm;

  return true;
  unguard;
}


//==========================================================================
//
//  VLevel::LoadBlockMap
//
//==========================================================================
void VLevel::LoadBlockMap (int Lump) {
  guard(VLevel::LoadBlockMap);
  VStream *Strm = nullptr;

  if (Lump >= 0 && !build_blockmap) Strm = W_CreateLumpReaderNum(Lump);

  if (!Strm || Strm->TotalSize() == 0 || Strm->TotalSize()/2 >= 0x10000) {
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
      BlockMapLump[i] = Tmp == -1 ? -1 : (vuint16)Tmp & 0xffff;
    }
  }

  if (Strm) delete Strm;

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
  unguard;
}

//==========================================================================
//
//  VLevel::CreateBlockMap
//
//==========================================================================

void VLevel::CreateBlockMap()
{
  guard(VLevel::CreateBlockMap);
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

  int Width = MapBlock(MaxX - MinX) + 1;
  int Height = MapBlock(MaxY - MinY) + 1;

  // add all lines to their corresponding blocks
  TArray<vuint16>* BlockLines = new TArray<vuint16>[Width*Height];
  for (int i = 0; i < NumLines; ++i) {
    // determine starting and ending blocks
    line_t &Line = Lines[i];
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
  unguard;
}

//==========================================================================
//
//  VLevel::LoadReject
//
//==========================================================================

void VLevel::LoadReject(int Lump)
{
  guard(VLevel::LoadReject);
  if (Lump < 0)
  {
    return;
  }
  VStream *Strm = W_CreateLumpReaderNum(Lump);
  //  Check for empty reject lump
  if (Strm->TotalSize())
  {
    //  Check if reject lump is required bytes long.
    int NeededSize = (NumSectors * NumSectors + 7) / 8;
    if (Strm->TotalSize() < NeededSize)
    {
      GCon->Logf("Reject data is %d bytes too short",
        NeededSize - Strm->TotalSize());
    }
    else
    {
      //  Read it.
      RejectMatrixSize = Strm->TotalSize();
      RejectMatrix = new vuint8[RejectMatrixSize];
      Strm->Serialise(RejectMatrix, RejectMatrixSize);

      //  Check if it's an all-zeroes lump, in which case it's useless
      // and can be discarded.
      bool Blank = true;
      for (int i = 0; i < NeededSize; i++)
      {
        if (RejectMatrix[i])
        {
          Blank = false;
          break;
        }
      }
      if (Blank)
      {
        delete[] RejectMatrix;
        RejectMatrix = nullptr;
        RejectMatrixSize = 0;
      }
    }
  }
  delete Strm;
  Strm = nullptr;
  unguard;
}

//==========================================================================
//
//  VLevel::LoadThings1
//
//==========================================================================

void VLevel::LoadThings1(int Lump)
{
  guard(VLevel::LoadThings1);
  NumThings = W_LumpLength(Lump) / 10;
  Things = new mthing_t[NumThings];
  memset((void *)Things, 0, sizeof(mthing_t) * NumThings);

  VStream *Strm = W_CreateLumpReaderNum(Lump);
  mthing_t *th = Things;
  for (int i = 0; i < NumThings; i++, th++)
  {
    vint16 x, y, angle, type, options;
    *Strm << x << y << angle << type << options;

    th->x = x;
    th->y = y;
    th->angle = angle;
    th->type = type;
    th->options = options & ~7;
    th->SkillClassFilter = 0xffff0000;
    if (options & 1)
    {
      th->SkillClassFilter |= 0x03;
    }
    if (options & 2)
    {
      th->SkillClassFilter |= 0x04;
    }
    if (options & 4)
    {
      th->SkillClassFilter |= 0x18;
    }
  }
  delete Strm;
  Strm = nullptr;
  unguard;
}

//==========================================================================
//
//  VLevel::LoadThings2
//
//==========================================================================

void VLevel::LoadThings2(int Lump)
{
  guard(VLevel::LoadThings2);
  NumThings = W_LumpLength(Lump) / 20;
  Things = new mthing_t[NumThings];
  memset((void *)Things, 0, sizeof(mthing_t) * NumThings);

  VStream *Strm = W_CreateLumpReaderNum(Lump);
  mthing_t *th = Things;
  for (int i = 0; i < NumThings; i++, th++)
  {
    vint16 tid, x, y, height, angle, type, options;
    vuint8 special, arg1, arg2, arg3, arg4, arg5;
    *Strm << tid << x << y << height << angle << type << options
      << special << arg1 << arg2 << arg3 << arg4 << arg5;

    th->tid = tid;
    th->x = x;
    th->y = y;
    th->height = height;
    th->angle = angle;
    th->type = type;
    th->options = options & ~0xe7;
    th->SkillClassFilter = (options & 0xe0) << 11;
    if (options & 1)
    {
      th->SkillClassFilter |= 0x03;
    }
    if (options & 2)
    {
      th->SkillClassFilter |= 0x04;
    }
    if (options & 4)
    {
      th->SkillClassFilter |= 0x18;
    }
    th->special = special;
    th->arg1 = arg1;
    th->arg2 = arg2;
    th->arg3 = arg3;
    th->arg4 = arg4;
    th->arg5 = arg5;
  }
  delete Strm;
  Strm = nullptr;
  unguard;
}

//==========================================================================
//
//  VLevel::LoadACScripts
//
//==========================================================================

void VLevel::LoadACScripts(int Lump)
{
  guard(VLevel::LoadACScripts);
  Acs = new VAcsLevel(this);

  //  Load level's BEHAVIOR lump if it has one.
  if (Lump >= 0)
  {
    Acs->LoadObject(Lump);
  }

  //  Load ACS helper scripts if needed (for Strife).
  if (GGameInfo->AcsHelper != NAME_None)
  {
    Acs->LoadObject(W_GetNumForName(GGameInfo->AcsHelper,
      WADNS_ACSLibrary));
  }

  //  Load user-specified default ACS libraries.
  for (int ScLump = W_IterateNS(-1, WADNS_Global); ScLump >= 0;
    ScLump = W_IterateNS(ScLump, WADNS_Global))
  {
    if (W_LumpName(ScLump) != NAME_loadacs)
    {
      continue;
    }

    VScriptParser *sc = new VScriptParser(*W_LumpName(ScLump),
      W_CreateLumpReaderNum(ScLump));
    while (!sc->AtEnd())
    {
      sc->ExpectName8();
      int AcsLump = W_CheckNumForName(sc->Name8, WADNS_ACSLibrary);
      if (AcsLump >= 0)
      {
        Acs->LoadObject(AcsLump);
      }
      else
      {
        GCon->Logf("No such autoloaded ACS library %s", *sc->String);
      }
    }
    delete sc;
    sc = nullptr;
  }
  unguard;
}

//==========================================================================
//
//  VLevel::TexNumForName
//
//  Retrieval, get a texture or flat number for a name.
//
//==========================================================================

static TStrSet texNumForNameWarned;

int VLevel::TexNumForName(const char *name, int Type, bool CMap) const
{
  guard(VLevel::TexNumForName);
  VName Name(name, VName::AddLower8);
  int i = GTextureManager.CheckNumForName(Name, Type, true, true);
  if (i == -1)
  {
    if (CMap) return 0;
    if (!texNumForNameWarned.put(*Name)) GCon->Logf("FTNumForName: %s not found", *Name);
    return GTextureManager.DefaultTexture;
  }
  return i;
  unguard;
}

//==========================================================================
//
//  VLevel::TexNumOrColour
//
//==========================================================================

int VLevel::TexNumOrColour(const char *name, int Type, bool &GotColour,
  vuint32 &Col) const
{
  guard(VLevel::TexNumOrColour);
  VName Name(name, VName::AddLower8);
  int i = GTextureManager.CheckNumForName(Name, Type, true, true);
  if (i == -1)
  {
    char TmpName[9];
    memcpy(TmpName, name, 8);
    TmpName[8] = 0;
    char *Stop;
    Col = strtoul(TmpName, &Stop, 16);
    GotColour = (*Stop == 0) && (Stop >= TmpName + 2) &&
      (Stop <= TmpName + 6);
    return 0;
  }
  GotColour = false;
  return i;
  unguard;
}

//==========================================================================
//
//  VLevel::LoadRogueConScript
//
//==========================================================================

void VLevel::LoadRogueConScript(VName LumpName, int ALumpNum,
  FRogueConSpeech *&SpeechList, int &NumSpeeches) const
{
  guard(VLevel::LoadRogueConScript);
  bool teaser = false;
  //  Clear variables.
  SpeechList = nullptr;
  NumSpeeches = 0;

  int LumpNum = ALumpNum;
  if (LumpNum < 0)
  {
    //  Check for empty name.
    if (LumpName == NAME_None)
    {
      return;
    }

    //  Get lump num.
    LumpNum = W_CheckNumForName(LumpName);
    if (LumpNum < 0)
    {
      return; //  Not here.
    }
  }

  //  Load them.

  //  First check the size of the lump, if it's 1516,
  //  we are using a registered strife lump, if it's
  //  1488, then it's a teaser conversation script
  if (W_LumpLength(LumpNum) % 1516 != 0)
  {
    NumSpeeches = W_LumpLength(LumpNum) / 1488;
    teaser = true;
  }
  else
  {
    NumSpeeches = W_LumpLength(LumpNum) / 1516;
  }
  SpeechList = new FRogueConSpeech[NumSpeeches];

  VStream *Strm = W_CreateLumpReaderNum(LumpNum);
  for (int i = 0; i < NumSpeeches; i++)
  {
    char Tmp[324];

    FRogueConSpeech &S = SpeechList[i];
    if (!teaser)
    {
      // Parse non teaser speech
      *Strm << S.SpeakerID << S.DropItem << S.CheckItem1 << S.CheckItem2
        << S.CheckItem3 << S.JumpToConv;

      // Parse NPC name
      Strm->Serialise(Tmp, 16);
      Tmp[16] = 0;
      S.Name = Tmp;

      // Parse sound name (if any)
      Strm->Serialise(Tmp, 8);
      Tmp[8] = 0;
      S.Voice = VName(Tmp, VName::AddLower8);
      if (S.Voice != NAME_None)
      {
        S.Voice = va("svox/%s", *S.Voice);
      }

      // Parse backdrop pics (if any)
      Strm->Serialise(Tmp, 8);
      Tmp[8] = 0;
      S.BackPic = VName(Tmp, VName::AddLower8);

      // Parse speech text
      Strm->Serialise(Tmp, 320);
      Tmp[320] = 0;
      S.Text = Tmp;
    }
    else
    {
      // Parse teaser speech, which doesn't contain all fields
      *Strm << S.SpeakerID << S.DropItem;

      // Parse NPC name
      Strm->Serialise(Tmp, 16);
      Tmp[16] = 0;
      S.Name = Tmp;

      // Parse sound number (if any)
      vint32 Num;
      *Strm << Num;
      if (Num)
      {
        S.Voice = va("svox/voc%d", Num);
      }

      // Also, teaser speeches don't have backdrop pics
      S.BackPic = NAME_None;

      // Parse speech text
      Strm->Serialise(Tmp, 320);
      Tmp[320] = 0;
      S.Text = Tmp;
    }

    // Parse conversation options for PC
    for (int j = 0; j < 5; j++)
    {
      FRogueConChoice &C = S.Choices[j];
      *Strm << C.GiveItem << C.NeedItem1 << C.NeedItem2 << C.NeedItem3
        << C.NeedAmount1 << C.NeedAmount2 << C.NeedAmount3;
      Strm->Serialise(Tmp, 32);
      Tmp[32] = 0;
      C.Text = Tmp;
      Strm->Serialise(Tmp, 80);
      Tmp[80] = 0;
      C.TextOK = Tmp;
      *Strm << C.Next << C.Objectives;
      Strm->Serialise(Tmp, 80);
      Tmp[80] = 0;
      C.TextNo = Tmp;
    }
  }
  delete Strm;
  Strm = nullptr;
  unguard;
}

//==========================================================================
//
//  VLevel::ClearBox
//
//==========================================================================

inline void VLevel::ClearBox(float *box) const
{
  guardSlow(VLevel::ClearBox);
  box[BOXTOP] = box[BOXRIGHT] = -99999.0;
  box[BOXBOTTOM] = box[BOXLEFT] = 99999.0;
  unguardSlow;
}

//==========================================================================
//
//  VLevel::AddToBox
//
//==========================================================================

inline void VLevel::AddToBox(float *box, float x, float y) const
{
  guardSlow(VLevel::AddToBox);
  if (x < box[BOXLEFT])
    box[BOXLEFT] = x;
  else if (x > box[BOXRIGHT])
    box[BOXRIGHT] = x;
  if (y < box[BOXBOTTOM])
    box[BOXBOTTOM] = y;
  else if (y > box[BOXTOP])
    box[BOXTOP] = y;
  unguardSlow;
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
//  Builds sector line lists and subsector sector numbers. Finds block
// bounding boxes for sectors.
//
//==========================================================================

void VLevel::GroupLines() const
{
  guard(VLevel::GroupLines);
  line_t ** linebuffer;
  int i;
  int total;
  line_t *li;
  sector_t *sector;
  float bbox[4];
  int block;

  LinkNode(NumNodes - 1, nullptr);

  // count number of lines in each sector
  li = Lines;
  total = 0;
  for (i = 0; i < NumLines; i++, li++)
  {
    total++;
    li->frontsector->linecount++;

    if (li->backsector && li->backsector != li->frontsector)
    {
      li->backsector->linecount++;
      total++;
    }
  }

  // build line tables for each sector
  linebuffer = new line_t*[total];
  sector = Sectors;
  for (i = 0; i < NumSectors; i++, sector++)
  {
    sector->lines = linebuffer;
    linebuffer += sector->linecount;
  }

  //  Assign lines for each sector.
  int *SecLineCount = new int[NumSectors];
  memset(SecLineCount, 0, sizeof(int) * NumSectors);
  li = Lines;
  for (i = 0; i < NumLines; i++, li++)
  {
    if (li->frontsector)
    {
      li->frontsector->lines[SecLineCount[
        li->frontsector - Sectors]++] = li;
    }
    if (li->backsector && li->backsector != li->frontsector)
    {
      li->backsector->lines[SecLineCount[
        li->backsector - Sectors]++] = li;
    }
  }

  sector = Sectors;
  for (i = 0; i < NumSectors; i++, sector++)
  {
    if (SecLineCount[i] != sector->linecount)
    {
      Sys_Error("GroupLines: miscounted");
    }
    ClearBox(bbox);
    for (int j = 0; j < sector->linecount; j++)
    {
      li = sector->lines[j];
      AddToBox(bbox, li->v1->x, li->v1->y);
      AddToBox(bbox, li->v2->x, li->v2->y);
    }

    // set the soundorg to the middle of the bounding box
    sector->soundorg = TVec((bbox[BOXRIGHT] + bbox[BOXLEFT]) / 2.0,
      (bbox[BOXTOP] + bbox[BOXBOTTOM]) / 2.0, 0);

    // adjust bounding box to map blocks
    block = MapBlock(bbox[BOXTOP] - BlockMapOrgY + MAXRADIUS);
    block = block >= BlockMapHeight ? BlockMapHeight - 1 : block;
    sector->blockbox[BOXTOP] = block;

    block = MapBlock(bbox[BOXBOTTOM] - BlockMapOrgY - MAXRADIUS);
    block = block < 0 ? 0 : block;
    sector->blockbox[BOXBOTTOM] = block;

    block = MapBlock(bbox[BOXRIGHT] - BlockMapOrgX + MAXRADIUS);
    block = block >= BlockMapWidth ? BlockMapWidth - 1 : block;
    sector->blockbox[BOXRIGHT] = block;

    block = MapBlock(bbox[BOXLEFT] - BlockMapOrgX - MAXRADIUS);
    block = block < 0 ? 0 : block;
    sector->blockbox[BOXLEFT] = block;
  }
  delete[] SecLineCount;
  SecLineCount = nullptr;

  unguard;
}

//==========================================================================
//
//  VLevel::LinkNode
//
//==========================================================================

void VLevel::LinkNode(int BSPNum, node_t *pParent) const
{
  guardSlow(LinkNode);
  if (BSPNum & NF_SUBSECTOR)
  {
    int num;

    if (BSPNum == -1)
      num = 0;
    else
      num = BSPNum & (~NF_SUBSECTOR);
    if (num < 0 || num >= NumSubsectors)
      Host_Error("ss %i with numss = %i", num, NumSubsectors);
    Subsectors[num].parent = pParent;
  }
  else
  {
    if (BSPNum < 0 || BSPNum >= NumNodes)
      Host_Error("bsp %i with numnodes = %i", NumNodes, NumNodes);
    node_t *bsp = &Nodes[BSPNum];
    bsp->parent = pParent;

    LinkNode(bsp->children[0], bsp);
    LinkNode(bsp->children[1], bsp);
  }
  unguardSlow;
}

//==========================================================================
//
//  VLevel::CreateRepBase
//
//==========================================================================

void VLevel::CreateRepBase()
{
  guard(VLevel::CreateRepBase);
  BaseLines = new rep_line_t[NumLines];
  for (int i = 0; i < NumLines; i++)
  {
    line_t &L = Lines[i];
    rep_line_t &B = BaseLines[i];
    B.alpha = L.alpha;
  }

  BaseSides = new rep_side_t[NumSides];
  for (int i = 0; i < NumSides; i++)
  {
    side_t &S = Sides[i];
    rep_side_t &B = BaseSides[i];
    B.TopTextureOffset = S.TopTextureOffset;
    B.BotTextureOffset = S.BotTextureOffset;
    B.MidTextureOffset = S.MidTextureOffset;
    B.TopRowOffset = S.TopRowOffset;
    B.BotRowOffset = S.BotRowOffset;
    B.MidRowOffset = S.MidRowOffset;
    B.TopTexture = S.TopTexture;
    B.BottomTexture = S.BottomTexture;
    B.MidTexture = S.MidTexture;
    B.Flags = S.Flags;
    B.Light = S.Light;
  }

  BaseSectors = new rep_sector_t[NumSectors];
  for (int i = 0; i < NumSectors; i++)
  {
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
  for (int i = 0; i < NumPolyObjs; i++)
  {
    polyobj_t &P = PolyObjs[i];
    rep_polyobj_t &B = BasePolyObjs[i];
    B.startSpot = P.startSpot;
    B.angle = P.angle;
  }
  unguard;
}

//==========================================================================
//
//  VLevel::HashSectors
//
//==========================================================================

void VLevel::HashSectors()
{
  guard(VLevel::HashSectors);
  //  Clear hash.
  for (int i = 0; i < NumSectors; i++)
  {
    Sectors[i].HashFirst = -1;
  }
  //  Create hash. Process sectors in backward order so that they get
  // processed in original order.
  for (int i = NumSectors - 1; i >= 0; i--)
  {
    vuint32 HashIndex = (vuint32)Sectors[i].tag % (vuint32)NumSectors;
    Sectors[i].HashNext = Sectors[HashIndex].HashFirst;
    Sectors[HashIndex].HashFirst = i;
  }
  unguard;
}

//==========================================================================
//
//  VLevel::HashLines
//
//==========================================================================

void VLevel::HashLines()
{
  guard(VLevel::HashLines);
  //  Clear hash.
  for (int i = 0; i < NumLines; i++)
  {
    Lines[i].HashFirst = -1;
  }
  //  Create hash. Process lines in backward order so that they get
  // processed in original order.
  for (int i = NumLines - 1; i >= 0; i--)
  {
    vuint32 HashIndex = (vuint32)Lines[i].LineTag % (vuint32)NumLines;
    Lines[i].HashNext = Lines[HashIndex].HashFirst;
    Lines[HashIndex].HashFirst = i;
  }
  unguard;
}

//==========================================================================
//
//  VLevel::FloodZones
//
//==========================================================================

void VLevel::FloodZones()
{
  guard(VLevel::FloodZones);
  for (int i = 0; i < NumSectors; i++)
  {
    if (Sectors[i].Zone == -1)
    {
      FloodZone(&Sectors[i], NumZones);
      NumZones++;
    }
  }

  Zones = new vint32[NumZones];
  for (int i = 0; i < NumZones; i++)
  {
    Zones[i] = 0;
  }
  unguard;
}

//==========================================================================
//
//  VLevel::FloodZone
//
//==========================================================================

void VLevel::FloodZone(sector_t *Sec, int Zone)
{
  guard(VLevel::FloodZone);
  Sec->Zone = Zone;
  for (int i = 0; i < Sec->linecount; i++)
  {
    line_t *Line = Sec->lines[i];
    if (Line->flags & ML_ZONEBOUNDARY)
    {
      continue;
    }
    if (Line->frontsector && Line->frontsector->Zone == -1)
    {
      FloodZone(Line->frontsector, Zone);
    }
    if (Line->backsector && Line->backsector->Zone == -1)
    {
      FloodZone(Line->backsector, Zone);
    }
  }
  unguard;
}

//==========================================================================
//
// VLevel::FixSelfRefDeepWater
//
// This code was taken from Hyper3dge
//
//==========================================================================
void VLevel::FixSelfRefDeepWater () {
  vuint8 *self_subs = new vuint8[NumSubsectors];
  memset(self_subs, 0, NumSubsectors);

  for (int i = 0; i < NumSegs; ++i) {
    const seg_t *seg = &Segs[i];

    //if (seg->miniseg) continue;
    if (!seg->linedef) continue; //k8: miniseg check (i think)
    if (!seg->front_sub) { GCon->Logf("INTERNAL ERROR IN GLBSP LOADER: FRONT SUBSECTOR IS NOT SET!"); return; }

    if (seg->linedef->backsector && seg->linedef->frontsector == seg->linedef->backsector) {
      self_subs[seg->front_sub-Subsectors] |= 1;
    } else {
      self_subs[seg->front_sub-Subsectors] |= 2;
    }
  }

  int pass = 0;

  do {
    ++pass;
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
  } while (pass < 100);

  for (int i = 0; i < NumSubsectors; ++i) {
    subsector_t *sub = &Subsectors[i];
    sector_t *hs = sub->deepref;
    if (!hs) continue;
    //while (hs->deepref && hs->deepref != sub->deepref) hs = hs->deepref;
    //if (hs->deepref == sub->deepref) hs = sub->deepref;
    sector_t *ss = sub->sector;
    if (!ss) { if (dbg_deep_water) GCon->Logf("WTF(0)?!"); continue; }
    if (ss->deepref) {
      if (ss->deepref != hs) { if (dbg_deep_water) GCon->Logf("WTF(1) %d : %d?!", (int)(hs-Sectors), (int)(ss->deepref-Sectors)); continue; }
    } else {
      ss->deepref = hs;
    }
  }

  delete[] self_subs;
}

//==========================================================================
//
// VLevel::IsDeepWater
//
//==========================================================================

// bits: 0: normal, back floor; 1: normal, front ceiling
// this tries to detect and fix bugs like Doom2:MAP04
int VLevel::IsDeepWater (line_t *line) {
  // should have both sectors
  if (!line || !line->frontsector || !line->backsector) return 0;
  // should not be self-referencing
  if (line->frontsector == line->backsector) return 0;
  // should have both sides
  if (line->sidenum[0] < 0 || line->sidenum[1] < 0) return 0;
  // ignore sloped floors
  if (line->frontsector->floor.minz != line->frontsector->floor.maxz) return 0;
  if (line->backsector->floor.minz != line->backsector->floor.maxz) return 0;
  // back sector should have height
  if (line->backsector->floor.minz >= line->backsector->ceiling.minz) return 0;
  // front sector should have height
  if (line->frontsector->floor.minz >= line->frontsector->ceiling.minz) return 0;

  int res = 0;
  // floor: back sidedef should have no texture
  if (Sides[line->sidenum[1]].BottomTexture == 0 && !line->backsector->heightsec &&
      Sides[line->sidenum[0]].TopTexture > 0 && Sides[line->sidenum[0]].MidTexture == 0) {
    // it should be lower than front
    if (line->frontsector->floor.minz > line->backsector->floor.minz) {
      res |= 1;
#ifdef DEBUG_DEEP_WATERS
      if (dbg_deep_water) {
        int lidx = (int)(ptrdiff_t)(line-Lines);
        GCon->Logf("    DEEP WATER; LINEDEF #%d; front_floor_z=%f; back_floor_z=%f", lidx, line->frontsector->floor.minz, line->backsector->floor.minz);
        GCon->Logf("    DEEP WATER; LINEDEF #%d; front_ceiling_z=%f; back_ceiling_z=%f", lidx, line->frontsector->ceiling.minz, line->backsector->ceiling.minz);
        //if (!line->partner) GCon->Logf("  NO PARTNER!");
      }
    }
#endif
  }
  /*
  // ceiling: front sidedef should have no texture
  if (Sides[line->sidenum[1]].TopTexture == 0 && !line->frontsector->heightsec) {
    // it should be higher than front
    if (line->frontsector->ceiling.minz < line->backsector->ceiling.minz) res |= 2;
  }
  */

  // done
  return res;
}

//==========================================================================
//
// VLevel::IsDeepOk
//
// all lines should have the same front/back
//
//==========================================================================

bool VLevel::IsDeepOk (sector_t *sec) {
  if (!sec || sec->linecount < 2) return false;
  int dwt = 0, xidx = -1;
  for (vint32 lidx = 0; lidx < sec->linecount; ++lidx) {
    dwt = IsDeepWater(sec->lines[lidx]);
    if (dwt != 0) { xidx = lidx; break; }
  }
  if (!dwt) return false;
  for (vint32 lidx = 0; lidx < sec->linecount; ++lidx) {
    line_t *ld = sec->lines[lidx];
    if (!ld) return false; // just in case
    int xdwt = IsDeepWater(sec->lines[lidx]);
    if (!xdwt) continue;
    if (xdwt != dwt) return false;
    if (ld->frontsector != sec->lines[xidx]->frontsector) return false;
    if (ld->backsector != sec->lines[xidx]->backsector) return false;
  }
  return true;
}

//==========================================================================
//
// VLevel::FixDeepWater
//
//==========================================================================

void VLevel::FixDeepWater (line_t *line, vint32 lidx) {
  if (!line->frontsector || !line->backsector) return;

  int type = IsDeepWater(line);
  if (type == 0) return; // not a deep water
  if (type == 3) return; //k8: i am not sure, but...

  // mark as deep water
  sector_t *hs;
  if (type != 2) {
    if (line->backsector->heightsec) return; // already processed
    hs = (line->backsector->heightsec = line->frontsector);
  } else {
    if (line->frontsector->heightsec) return; // already processed
    hs = (line->frontsector->heightsec = line->backsector);
  }
#ifdef DEBUG_DEEP_WATERS
  if (dbg_deep_water) GCon->Logf("*** DEEP WATER; LINEDEF #%d; type=%d; fsf=%f; bsf=%f", lidx, type, line->frontsector->floor.minz, line->backsector->floor.minz);
#endif
  hs->SectorFlags &= ~sector_t::SF_IgnoreHeightSec;
  hs->SectorFlags &= ~sector_t::SF_ClipFakePlanes;
  hs->SectorFlags &= ~sector_t::SF_FakeFloorOnly;
  hs->SectorFlags &= ~sector_t::SF_FakeCeilingOnly;
  //hs->SectorFlags |= sector_t::SF_NoFakeLight;
  if (type == 1) {
    hs->SectorFlags |= sector_t::SF_FakeFloorOnly;
  } else if (type == 2) {
    hs->SectorFlags |= sector_t::SF_FakeCeilingOnly;
    //hs->SectorFlags |= sector_t::SF_ClipFakePlanes;
  }
}


void VLevel::FixDeepWaters () {
  for (vint32 sidx = 0; sidx < NumSectors; ++sidx) Sectors[sidx].deepref = nullptr;

  if (deepwater_hacks) {
    FixSelfRefDeepWater();
  }

  if (deepwater_hacks_extra) {
    for (vint32 sidx = 0; sidx < NumSectors; ++sidx) {
      sector_t *sec = &Sectors[sidx];
      if (!IsDeepOk(sec)) continue;
#ifdef DEBUG_DEEP_WATERS
      if (dbg_deep_water) GCon->Logf("DWSEC=%d", sidx);
#endif
      for (vint32 lidx = 0; lidx < sec->linecount; ++lidx) {
        line_t *ld = sec->lines[lidx];
        FixDeepWater(ld, (int)(ld-Lines));
      }
    }
  }
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
