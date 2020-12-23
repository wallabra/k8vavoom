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
#include "../gamedefs.h"


static int constexpr cestlen (const char *s, int pos=0) noexcept { return (s && s[pos] ? 1+cestlen(s, pos+1) : 0); }
static constexpr const char *CACHE_DATA_SIGNATURE = "VAVOOM CACHED DATA VERSION 008.\n";
enum { CDSLEN = cestlen(CACHE_DATA_SIGNATURE) };
static_assert(CDSLEN == 32, "oops!");

static bool cacheCleanupComplete = false; // do it only once, not on each map loading


static VCvarI loader_cache_compression_level("loader_cache_compression_level", "9", "Cache file compression level [0..9]", CVAR_Archive);
static VCvarI loader_cache_max_age_days("loader_cache_max_age_days", "7", "Remove cached data older than this number of days (<=0: none).", CVAR_Archive);

extern VCvarB loader_cache_data;


//==========================================================================
//
//  VLevel::getCacheDir
//
//==========================================================================
VStr VLevel::getCacheDir () {
  if (!loader_cache_data) return VStr();
  return FL_GetCacheDir();
}


//==========================================================================
//
//  VLevel::doCacheCleanup
//
//==========================================================================
void VLevel::doCacheCleanup () {
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

  VZLibStreamWriter *arrstrm = new VZLibStreamWriter(strm, (int)loader_cache_compression_level);

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

  VZLibStreamReader *arrstrm = new VZLibStreamReader(true, strm);
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
