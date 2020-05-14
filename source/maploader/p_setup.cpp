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
//**
//**    Do all the WAD I/O, get map description, set up initial state and
//**  misc. LUTs.
//**
//**************************************************************************
#include "../gamedefs.h"


static VCvarB dbg_show_map_hash("dbg_show_map_hash", false, "Show map hash?", CVAR_PreInit|CVAR_Archive);

static VCvarI r_fakecontrast("r_fakecontrast", "1", "Controls fake contrast/smooth lighting for walls (0: disable; 1: fake contrast; 2: smooth lighting)?", CVAR_Archive);
static VCvarB r_fakecontrast_ignore_mapinfo("r_fakecontrast_ignore_mapinfo", false, "Controls fake contrast/smooth lighting for walls (0: disable; 1: fake contrast; 2: smooth lighting)?", CVAR_Archive);

static VCvarB loader_cache_ignore_one("loader_cache_ignore_one", false, "Ignore (and remove) cache for next map loading?", CVAR_PreInit);
static VCvarB loader_cache_rebuild_data("loader_cache_rebuild_data", true, "Cache rebuilt nodes, pvs, blockmap, and so on?", CVAR_Archive);

VCvarB loader_cache_data("loader_cache_data", true, "Cache built level data?", CVAR_Archive);
VCvarF loader_cache_time_limit("loader_cache_time_limit", "3", "Cache data if building took more than this number of seconds.", CVAR_Archive);
VCvarI loader_cache_max_age_days("loader_cache_max_age_days", "7", "Remove cached data older than this number of days (<=0: none).", CVAR_Archive);
VCvarI loader_cache_compression_level("loader_cache_compression_level", "9", "Cache file compression level [0..9]", CVAR_Archive);

//static VCvarB strict_level_errors("strict_level_errors", true, "Strict level errors mode?", 0);
VCvarB build_blockmap("loader_force_blockmap_rebuild", false, "Force blockmap rebuild on map loading?", CVAR_Archive);
//static VCvarB show_level_load_times("show_level_load_times", false, "Show loading times?", CVAR_Archive);

// there seems to be a bug in compressed GL nodes reader, hence the flag
//static VCvarB nodes_allow_compressed_old("nodes_allow_compressed_old", true, "Allow loading v0 compressed GL nodes?", CVAR_Archive);
VCvarB nodes_allow_compressed("nodes_allow_compressed", false, "Allow loading v1+ compressed GL nodes?", CVAR_Archive);

static VCvarB loader_force_nodes_rebuild("loader_force_nodes_rebuild", true, "Force node rebuilding?", CVAR_Archive);


extern VCvarI nodes_builder_type;
extern VCvarB ldr_fix_slope_cracks;
#ifdef CLIENT
extern VCvarI r_max_portal_depth;
extern VCvarI r_max_portal_depth_override;
extern int ldr_extrasamples_override; // -1: no override; 0: disable; 1: enable
extern int r_precalc_static_lights_override; // <0: not set
extern int r_precache_textures_override; // <0: not set
#endif


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


// ////////////////////////////////////////////////////////////////////////// //
static int constexpr cestlen (const char *s, int pos=0) noexcept { return (s && s[pos] ? 1+cestlen(s, pos+1) : 0); }
static constexpr const char *CACHE_DATA_SIGNATURE = "VAVOOM CACHED DATA VERSION 008.\n";
enum { CDSLEN = cestlen(CACHE_DATA_SIGNATURE) };
static_assert(CDSLEN == 32, "oops!");
static bool cacheCleanupComplete = false; // do it only once, not on each map loading
static TMap<VStr, bool> mapTextureWarns;


// ////////////////////////////////////////////////////////////////////////// //
struct AuxiliaryCloser {
public:
  bool doCloseAux;

public:
  VV_DISABLE_COPY(AuxiliaryCloser)
  AuxiliaryCloser () : doCloseAux(false) {}
  ~AuxiliaryCloser () { if (doCloseAux) W_CloseAuxiliary(); doCloseAux = false; }
};


// ////////////////////////////////////////////////////////////////////////// //
struct LoadingTiming {
  const char *name;
  double time;
  int msecs;
};

#define MAX_LOADING_TIMINGS  (64)
static LoadingTiming loadingTimings[MAX_LOADING_TIMINGS];
static unsigned int loadingTimingsCount = 0;


//==========================================================================
//
//  ResetLoadingTimings
//
//==========================================================================
static void ResetLoadingTimings () {
  loadingTimingsCount = 0;
}


//==========================================================================
//
//  AddLoadingTiming
//
//==========================================================================
static void AddLoadingTiming (const char *name, double time) {
  if (!name || !name[0]) return;
  if (loadingTimingsCount == MAX_LOADING_TIMINGS) return;
  LoadingTiming *tm = &loadingTimings[loadingTimingsCount++];
  tm->name = name;
  tm->time = time;
  if (time < 0) {
    tm->msecs = 0;
  } else {
    tm->msecs = (int)(time*1000+0.5);
  }
}


//==========================================================================
//
//  DumpLoadingTimings
//
//==========================================================================
static void DumpLoadingTimings () {
  if (loadingTimingsCount == 0) return;
  size_t maxLabelLength = 0;
  for (unsigned int f = 0; f < loadingTimingsCount; ++f) {
    const LoadingTiming &lt = loadingTimings[f];
    if (lt.msecs == 0) continue;
    size_t sl = strlen(lt.name);
    if (maxLabelLength < sl) maxLabelLength = sl;
  }
  if (maxLabelLength == 0) return; // nothing to do
  if (maxLabelLength > 64) return; // just in case
  GCon->Log("-------");
  char buf[256];
  for (unsigned int f = 0; f < loadingTimingsCount; ++f) {
    const LoadingTiming &lt = loadingTimings[f];
    if (lt.msecs == 0) continue;
    memset(buf, 32, sizeof(buf));
    strcpy(buf, lt.name);
    buf[strlen(lt.name)] = 32;
    snprintf(buf+maxLabelLength+1, sizeof(buf)-maxLabelLength-1, "%3d.%03d", lt.msecs/1000, lt.msecs%1000);
    GCon->Log(buf);
  }
}


//==========================================================================
//
//  VLevel::FixKnownMapErrors
//
//==========================================================================
void VLevel::FixKnownMapErrors () {
  eventKnownMapBugFixer();
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
    if (!fname.endsWithCI(".cache") && !fname.endsWithCI(".cache.lmap")) continue;
    //GCon->Logf(NAME_Debug, "*** FILE: '%s' (%s)", *fname, *cpath);
    VStr shortname = fname;
    fname = cpath+"/"+fname;
    int ftime = Sys_FileTime(fname);
    //GCon->Logf(NAME_Debug, "cache: age=%d for '%s' (%s); days=%d; tm=%d; cmp=%d; age=%d", currtime-ftime, *shortname, *fname, loader_cache_max_age_days.asInt(), 60*60*24*loader_cache_max_age_days, (int)(currtime-ftime > 60*60*24*loader_cache_max_age_days), (currtime-ftime)/(60*60*24));
    if (ftime <= 0) {
      GCon->Logf(NAME_Debug, "cache: deleting invalid file '%s'", *shortname);
      dellist.append(fname);
    } else if (ftime < currtime && currtime-ftime > 60*60*24*loader_cache_max_age_days) {
      GCon->Logf(NAME_Debug, "cache: deleting old cache file '%s'", *shortname);
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
  *strm << n->normal.x << n->normal.y << n->normal.z << n->dist;
}


//==========================================================================
//
//  VLevel::ClearCachedData
//
//  this is also used in dtor
//
//==========================================================================
void VLevel::ClearAllMapData () {
  if (Sectors) {
    for (auto &&sec : allSectors()) {
      sec.DeleteAllRegions();
      sec.moreTags.clear();
    }
    // line buffer is shared, so this correctly deletes it
    delete[] Sectors[0].lines;
    Sectors[0].lines = nullptr;
    delete[] Sectors[0].nbsecs;
    Sectors[0].nbsecs = nullptr;
  }

  if (Segs) {
    for (auto &&seg : allSegs()) {
      while (seg.decalhead) {
        decal_t *c = seg.decalhead;
        seg.removeDecal(c);
        delete c->animator;
        delete c;
      }
    }
  }

  if (Lines) {
    for (auto &&line : allLines()) {
      delete[] line.v1lines;
      delete[] line.v2lines;
      line.moreTags.clear();
    }
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

  delete[] Zones;
  Zones = nullptr;
  NumZones = 0;

  GTextureManager.ResetMapTextures();
}


//==========================================================================
//
//  VLevel::SaveCachedData
//
//==========================================================================
void VLevel::SaveCachedData (VStream *strm) {
  if (!strm) return;

  NET_SendNetworkHeartbeat(true); // forced

  // signature
  strm->Serialize(CACHE_DATA_SIGNATURE, 32);

  vuint8 bspbuilder = GetNodesBuilder();
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
    vint32 sldidx = (n->splitldef ? (int)(ptrdiff_t)(n->splitldef-Lines) : -1);
    *arrstrm << sldidx;
    *arrstrm << n->sx << n->sy << n->dx << n->dy;
    if (f%512 == 0) NET_SendNetworkHeartbeat();
  }

  // vertices
  *arrstrm << NumVertexes;
  GCon->Logf("cache: writing %d vertexes", NumVertexes);
  for (int f = 0; f < NumVertexes; ++f) {
    float x = Vertexes[f].x;
    float y = Vertexes[f].y;
    float z = Vertexes[f].z;
    *arrstrm << x << y << z;
    if (f%512 == 0) NET_SendNetworkHeartbeat();
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
    if (f%512 == 0) NET_SendNetworkHeartbeat();
  }

  // subsectors
  *arrstrm << NumSubsectors;
  GCon->Logf("cache: writing %d subsectors", NumSubsectors);
  for (int f = 0; f < NumSubsectors; ++f) {
    subsector_t *ss = Subsectors+f;
    *arrstrm << ss->numlines;
    *arrstrm << ss->firstline;
    if (f%512 == 0) NET_SendNetworkHeartbeat();
  }

  // sectors
  *arrstrm << NumSectors;
  GCon->Logf("cache: writing %d sectors", NumSectors);
  /* this will be rebuilt
  for (int f = 0; f < NumSectors; ++f) {
    sector_t *sector = &Sectors[f];
    vint32 ssnum = -1;
    if (sector->subsectors) ssnum = (vint32)(ptrdiff_t)(sector->subsectors-Subsectors);
    *arrstrm << ssnum;
  }
  */

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
    if (seg->frontsub) fssnum = (vint32)(ptrdiff_t)(seg->frontsub-Subsectors);
    *arrstrm << fssnum;
    *arrstrm << seg->side;
    *arrstrm << seg->flags;
    if (f%512 == 0) NET_SendNetworkHeartbeat();
  }

  // reject
  NET_SendNetworkHeartbeat(true); // forced
  *arrstrm << RejectMatrixSize;
  if (RejectMatrixSize) {
    GCon->Logf("cache: writing %d bytes of reject table", RejectMatrixSize);
    arrstrm->Serialize(RejectMatrix, RejectMatrixSize);
  }

  // blockmap
  NET_SendNetworkHeartbeat(true); // forced
  *arrstrm << BlockMapLumpSize;
  if (BlockMapLumpSize) {
    GCon->Logf("cache: writing %d cells of blockmap table", BlockMapLumpSize);
    arrstrm->Serialize(BlockMapLump, BlockMapLumpSize*4);
  }

  //FIXME: store visdata size somewhere
  // pvs
  if (VisData) {
    NET_SendNetworkHeartbeat(true); // forced
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

  NET_SendNetworkHeartbeat(true); // forced
  strm->Flush();

  NET_SendNetworkHeartbeat();
}


//==========================================================================
//
//  VLevel::LoadCachedData
//
//==========================================================================
bool VLevel::LoadCachedData (VStream *strm) {
  if (!strm) return false;
  char sign[CDSLEN];

  // signature
  strm->Serialise(sign, CDSLEN);
  if (strm->IsError() || memcmp(sign, CACHE_DATA_SIGNATURE, CDSLEN) != 0) { GCon->Log("invalid cache file signature"); return false; }

  vuint8 bspbuilder = 255;
  *strm << bspbuilder;
  if (bspbuilder != GetNodesBuilder()) { GCon->Log("invalid cache nodes builder"); return false; }

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
    vint32 sldidx = -1;
    *arrstrm << sldidx;
    n->splitldef = (sldidx >= 0 && sldidx < NumLines ? &Lines[sldidx] : nullptr);
    *arrstrm << n->sx << n->sy << n->dx << n->dy;
  }

  delete [] Vertexes;
  *arrstrm << NumVertexes;
  GCon->Logf("cache: reading %d vertexes", NumVertexes);
  Vertexes = new TVec[NumVertexes];
  memset((void *)Vertexes, 0, sizeof(TVec)*NumVertexes);
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
    *arrstrm << ss->numlines;
    *arrstrm << ss->firstline;
  }

  // sectors
  GCon->Logf("cache: reading %d sectors", NumSectors);
  *arrstrm << checkSecNum;
  if (checkSecNum != NumSectors) { delete arrstrm; GCon->Logf("cache file corrupted (sectors)"); return false; }
  /* this will be rebuilt
  for (int f = 0; f < NumSectors; ++f) {
    sector_t *sector = &Sectors[f];
    vint32 ssnum = -1;
    *arrstrm << ssnum;
    sector->subsectors = (ssnum >= 0 ? Subsectors+ssnum : nullptr);
  }
  */

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
    if (v2num < 0 || v2num >= NumVertexes) { delete arrstrm; GCon->Log("cache file corrupted (seg v2)"); return false; }
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
    seg->frontsub = (fssnum >= 0 ? Subsectors+fssnum : nullptr);
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

  for (int f = 0; f < NumSubsectors; ++f) {
    subsector_t *ss = &Subsectors[f];
    if (ss->firstline < 0 || ss->firstline >= NumSegs) { GCon->Log("invalid subsector data (read error)"); return false; }
    ss->firstseg = &Segs[ss->firstline];
  }

  return true;
}


//==========================================================================
//
//  VLevel::LoadMap
//
//==========================================================================
void VLevel::LoadMap (VName AMapName) {
  AuxiliaryCloser auxCloser;

  bool killCache = loader_cache_ignore_one;
  cacheFlags = (loader_cache_ignore_one ? CacheFlag_Ignore : 0);
  loader_cache_ignore_one = false;
  bool AuxiliaryMap = false;
  int lumpnum, xmaplumpnum = -1;
  VName MapLumpName;
  decanimlist = nullptr;
  decanimuid = 0;

  mapTextureWarns.clear();
  // clear automap marks; save loader will restore them from a save
  #ifdef CLIENT
  AM_ClearMarks();
  #endif

  if (csTouched) Z_Free(csTouched);
  csTouchCount = 0;
  csTouched = nullptr;

load_again:
  cacheFileBase.clear();
  ResetLoadingTimings();
  GTextureManager.ResetMapTextures();

  #ifdef CLIENT
  r_max_portal_depth_override = -1;
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
        auxCloser.doCloseAux = true;
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
        lumpnum = W_AddAuxiliaryStream(lstrm, WAuxFileType::VFS_Wad);
        if (lumpnum >= 0) {
          auxCloser.doCloseAux = true;
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
  R_OSDMsgReset(OSD_MapLoading);
  R_OSDMsgShowMain("LOADING");

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
  const VMapInfo &MInfo = P_GetMapInfo(MapName);
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
      if (LName == NAME_None || LName == NAME_textmap) Host_Error("Map %s is not a valid UDMF map", *MapName);
           if (LName == NAME_behavior) BehaviorLump = lumpnum+i;
      else if (LName == NAME_blockmap) BlockmapLumpNum = lumpnum+i;
      else if (LName == NAME_reject) RejectLump = lumpnum+i;
      else if (LName == NAME_dialogue) DialogueLump = lumpnum+i;
      else if (LName == NAME_znodes) {
        if (!loader_cache_rebuild_data && nodes_allow_compressed) {
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

    // verify that it's a valid map
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

  if (nodes_builder_type == 0) {
    const char *nbname;
    switch (GetNodesBuilder()) {
      case BSP_AJ: nbname = "AJBSP"; break;
      case BSP_ZD: nbname = "ZDBSP"; break;
      default: nbname = "<unknown (bug)>"; break;
    }
    GCon->Logf("Selected nodes builder: %s", nbname);
  }

  if (AuxiliaryMap) GCon->Log("loading map from nested wad");

  if (sha224valid) {
    vuint8 sha224hash[SHA224_DIGEST_SIZE];
    sha224_final(&sha224ctx, sha224hash);
    MapHash = VStr::buf2hex(sha224hash, SHA224_DIGEST_SIZE);

    vuint8 md5digest[MD5Context::DIGEST_SIZE];
    md5ctx.Final(md5digest);
    MapHashMD5 = VStr::buf2hex(md5digest, MD5Context::DIGEST_SIZE);

    if (dbg_show_map_hash) {
      GCon->Logf("map hash, md5: %s", *MapHashMD5);
      GCon->Logf("map hash, sha: %s", *MapHash);
    } else if (developer) {
      GCon->Logf(NAME_Dev, "map hash, md5: %s", *MapHashMD5);
      GCon->Logf(NAME_Dev, "map hash, sha: %s", *MapHash);
    }
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
        GCon->Logf("no GL nodes found, k8vavoom will use internal node builder");
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
  double FloodFixTime = 0;
  double SectorListTime = 0;
  double MapHashingTime = 0;

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

  double Lines2Time = -Sys_Time();
  FixKnownMapErrors();
  bool forceNodeRebuildFromFixer = !!(LevelFlags&LF_ForceRebuildNodes);
  Lines2Time += Sys_Time();

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
      ClearAllMapData();
      goto load_again;
      //if (!glNodesFound) NeedNodesBuild = true;
    }
    delete strm;
    if (cachedDataLoaded) {
      forceNodeRebuildFromFixer = false; //k8: is this right?
      // touch cache file, so it will survive longer
      Sys_Touch(cacheFileName);
    }
  }

  double NodesTime = -Sys_Time();
  // and again; sorry!
  if (!cachedDataLoaded || forceNodeRebuildFromFixer) {
    if (NeedNodesBuild || forceNodeRebuildFromFixer) {
      GCon->Logf("building GL nodes");
      //R_OSDMsgShowSecondary("BUILDING NODES");
      BuildNodes();
      saveCachedData = true;
    } else if (UseComprGLNodes) {
      if (!LoadCompressedGLNodes(CompressedGLNodesLump, GLNodesHdr)) {
        GCon->Logf("rebuilding GL nodes");
        //R_OSDMsgShowSecondary("BUILDING NODES");
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

  for (int nidx = 0; nidx < NumNodes; ++nidx) Nodes[nidx].index = nidx;

  // create blockmap
  if (!BlockMapLump) {
    GCon->Logf("creating BLOCKMAP");
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
  for (int i = 0; i < NumSectors; ++i) {
    //GCon->Logf("MINMAX: %d/%d %3d%%", i, NumSectors, 100*i/NumSectors);
    CalcSecMinMaxs(&Sectors[i]);
  }
  MinMaxTime += Sys_Time();

  // fake contrast
  double WallShadesTime = -Sys_Time();
  const int fctype = (r_fakecontrast_ignore_mapinfo || MInfo.FakeContrast == 0 ? r_fakecontrast.asInt() : (MInfo.FakeContrast+1)%3);
  if (fctype > 0 && (MInfo.HorizWallShade|MInfo.VertWallShade) != 0) {
    for (auto &&line : allLines()) {
      const int shadeChange =
        !line.normal.x ? MInfo.HorizWallShade :
        !line.normal.y ? MInfo.VertWallShade :
        0;
      const int smoothChange =
        !line.normal.x ? 0 :
        (int)(MInfo.HorizWallShade+fabs(atanf(line.normal.y/line.normal.x)/1.57079f)*(MInfo.VertWallShade-MInfo.HorizWallShade)); // xs_RoundToInt()
      if (shadeChange || smoothChange) {
        for (int sn = 0; sn < 2; ++sn) {
          const int sidx = line.sidenum[sn];
          if (sidx >= 0) {
            side_t *side = &Sides[sidx];
            if (side->Flags&SDF_NOFAKECTX) continue; // UDMF flag
            if (side->Flags&SDF_SMOOTHLIT) { side->Light += smoothChange; continue; } // UDMF flag
            // apply mapinfo-defined shading
            side->Light += (fctype > 1 ? smoothChange : shadeChange);
          }
        }
      }
    }
  }
  WallShadesTime += Sys_Time();

  double RepBaseTime = -Sys_Time();
  CreateRepBase();
  RepBaseTime += Sys_Time();

  //GCon->Logf("Building Lidedef VV list");
  double LineVVListTime = -Sys_Time();
  BuildDecalsVVList();
  LineVVListTime += Sys_Time();

  // end of map lump processing
  if (AuxiliaryMap || auxCloser.doCloseAux) {
    // close the auxiliary file(s)
    auxCloser.doCloseAux = false;
    W_CloseAuxiliary();
  }

  DecalProcessingTime = -Sys_Time();
  PostProcessForDecals();
  DecalProcessingTime += Sys_Time();

  // do it here, so it won't touch sloped floors
  // it will set `othersec` for sectors too
  // also, it will detect "transparent door" sectors
  FloodFixTime = -Sys_Time();
  DetectHiddenSectors();
  FixTransparentDoors();
  FixDeepWaters();
  FloodFixTime += Sys_Time();

  // this must be called after deepwater fixes
  SectorListTime = -Sys_Time();
  BuildSectorLists();
  SectorListTime += Sys_Time();

  // calculate xxHash32 of various map parts

  // hash of linedefs, sidedefs, sectors (in this order)
  MapHashingTime = -Sys_Time();
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
  MapHashingTime += Sys_Time();


  TotalTime += Sys_Time();

  AddLoadingTiming("Level loaded in", TotalTime);
  AddLoadingTiming("Initialisation", InitTime);
  AddLoadingTiming("Vertexes", VertexTime);
  AddLoadingTiming("Sectors", SectorsTime);
  AddLoadingTiming("Lines", LinesTime);
  AddLoadingTiming("Things", ThingsTime);
  AddLoadingTiming("Translation", TranslTime);
  AddLoadingTiming("Sides", SidesTime);
  AddLoadingTiming("Error fixing", Lines2Time);
  AddLoadingTiming("Nodes", NodesTime);
  AddLoadingTiming("Blockmap", BlockMapTime);
  AddLoadingTiming("ACS loading", AcsTime);
  AddLoadingTiming("Group lines", GroupLinesTime);
  AddLoadingTiming("Flood zones", FloodZonesTime);
  AddLoadingTiming("Conversations", ConvTime);
  AddLoadingTiming("Reject", RejectTime);
  if (BuildPVSTime >= 0.1f) AddLoadingTiming("PVS building", BuildPVSTime);
  AddLoadingTiming("Spawn world", SpawnWorldTime);
  AddLoadingTiming("Polyobjs", InitPolysTime);
  AddLoadingTiming("Sector minmaxs", MinMaxTime);
  AddLoadingTiming("Wall shades", WallShadesTime);
  AddLoadingTiming("Linedef VV list", LineVVListTime);
  AddLoadingTiming("Decal processing", DecalProcessingTime);
  AddLoadingTiming("Sector min/max", MinMaxTime);
  AddLoadingTiming("Floodbug fixing", FloodFixTime);
  AddLoadingTiming("Sector lists", SectorListTime);
  AddLoadingTiming("Map hashing", MapHashingTime);

  DumpLoadingTimings();

  mapTextureWarns.clear();

  RecalcWorldBBoxes();

  cacheFileBase = cacheFileName;

  eventAfterLevelLoaded();
}


//==========================================================================
//
//  texForceLoad
//
//==========================================================================
static int texForceLoad (const char *name, int Type, bool CMap) {
  if (!name || !name[0]) return 0; // just in case
  if (name[0] == '-' && !name[1]) return 0; // just in case
  if (VStr::strEquCI(name, "aashitty") || VStr::strEquCI(name, "aastinky")) return 0;
  int i = -1;

  //GCon->Logf("texForceLoad(*): <%s>", name);

  #if 0
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
  #else
  i = GTextureManager.FindOrLoadFullyNamedTexture(VStr(name), nullptr, Type, /*overload*/true, /*silent*/true);
  #endif

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
//  native int LdrTexNumForName (string name, int Type, optional bool CMap);
//
//==========================================================================
IMPLEMENT_FUNCTION(VLevel, LdrTexNumForName) {
  VStr name;
  int Type;
  VOptParamBool CMap(false);
  vobjGetParamSelf(name, Type, CMap);
  RET_INT(Self->TexNumForName(*name, Type, CMap));
}


//==========================================================================
//
//  VLevel::TexNumForName
//
//  Retrieval, get a texture or flat number for a name.
//
//==========================================================================
int VLevel::TexNumForName (const char *name, int Type, bool CMap) const {
  if (!name || !name[0] || VStr::Cmp(name, "-") == 0) return 0;
  return texForceLoad(name, Type, CMap);
}


//==========================================================================
//
//  VLevel::TexNumOrColor
//
//==========================================================================
int VLevel::TexNumOrColor (const char *name, int Type, bool &GotColor, vuint32 &Col) const {
  if (VStr::strEquCI(name, "WATERMAP")) {
    GotColor = true;
    Col = M_ParseColor("#004FA5");
    Col = (Col&0xffffffU)|0x80000000U;
    return 0;
  }
  VName Name(name, VName::FindLower8);
  int i = (Name != NAME_None ? GTextureManager.CheckNumForName(Name, Type, true) : -1);
  if (i == -1) {
    char tmpname[9];
    strncpy(tmpname, name, 8);
    tmpname[8] = 0;
    Col = M_ParseColor(tmpname, true); // return zero if invalid
    if (Col == 0) {
      if (tmpname[0] == '#') tmpname[7] = 0; else tmpname[6] = 0;
      Col = M_ParseColor(tmpname, true); // return zero if invalid
    }
    GotColor = (Col != 0);
    Col &= 0xffffffU; // so it won't be fullbright
    i = 0;
  } else {
    GotColor = false;
    Col = 0;
  }
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
    B.Top.ScaleX = S.Top.ScaleX;
    B.Top.ScaleY = S.Top.ScaleY;
    B.Bot.ScaleX = S.Bot.ScaleX;
    B.Bot.ScaleY = S.Bot.ScaleY;
    B.Mid.ScaleX = S.Mid.ScaleX;
    B.Mid.ScaleY = S.Mid.ScaleY;
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
    B.Sky = S.Sky;
    B.params = S.params;
  }

  BasePolyObjs = new rep_polyobj_t[NumPolyObjs];
  for (int i = 0; i < NumPolyObjs; ++i) {
    polyobj_t *P = PolyObjs[i];
    rep_polyobj_t &B = BasePolyObjs[i];
    B.startSpot = P->startSpot;
    B.angle = P->angle;
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
