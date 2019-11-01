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
#include "gamedefs.h"
#include "r_local.h"


// ////////////////////////////////////////////////////////////////////////// //
//#define VV_DUMP_LMAP_CACHE_COMPARISONS
#define VV_DUMP_LMAP_CACHE_COMPARISON_FAIL

#define ON_EPSILON      (0.1f)
#define SUBDIVIDE_SIZE  (240)

#define EXTMAX  (32767)
//#define EXTMAX  (65536)
// float mantissa is 24 bits, but let's play safe, and use 20 bits
//#define EXTMAX  (0x100000)


// ////////////////////////////////////////////////////////////////////////// //
static int constexpr cestlen (const char *s) { return (s && *s ? 1+cestlen(s+1) : 0); }
static constexpr const char *LMAP_CACHE_DATA_SIGNATURE = "VAVOOM CACHED LMAP VERSION 000.\n";
enum { CDSLEN = cestlen(LMAP_CACHE_DATA_SIGNATURE) };


// ////////////////////////////////////////////////////////////////////////// //
VCvarB r_precalc_static_lights("r_precalc_static_lights", true, "Precalculate static lights?", CVAR_Archive);
int r_precalc_static_lights_override = -1; // <0: not set

extern VCvarB loader_cache_data;
VCvarF loader_cache_time_limit_lightmap("loader_cache_time_limit_lightmap", "3", "Cache lightmap data if building took more than this number of seconds.", CVAR_Archive);
VCvarI loader_cache_compression_level_lightmap("loader_cache_compression_level_lightmap", "6", "Lightmap cache file compression level [0..9]", CVAR_Archive);

static VCvarB dbg_cache_lightmap_always("dbg_cache_lightmap_always", false, "Always cache lightmaps?", /*CVAR_Archive|*/CVAR_PreInit);
static VCvarB dbg_cache_lightmap_dump_missing("dbg_cache_lightmap_dump_missing", false, "Dump missing lightmaps?", /*CVAR_Archive|*/CVAR_PreInit);


// ////////////////////////////////////////////////////////////////////////// //
extern int light_mem;


// ////////////////////////////////////////////////////////////////////////// //
// pool allocator for split vertices
// ////////////////////////////////////////////////////////////////////////// //
static float *spvPoolDots = nullptr;
static int *spvPoolSides = nullptr;
static TVec *spvPoolV1 = nullptr;
static TVec *spvPoolV2 = nullptr;
static int spvPoolSize = 0;


static inline void spvReserve (int size) {
  if (size < 1) size = 1;
  size = (size|0xfff)+1;
  if (spvPoolSize < size) {
    spvPoolSize = size;
    spvPoolDots = (float *)Z_Realloc(spvPoolDots, spvPoolSize*sizeof(spvPoolDots[0])); if (!spvPoolDots) Sys_Error("OOM!");
    spvPoolSides = (int *)Z_Realloc(spvPoolSides, spvPoolSize*sizeof(spvPoolSides[0])); if (!spvPoolSides) Sys_Error("OOM!");
    spvPoolV1 = (TVec *)Z_Realloc(spvPoolV1, spvPoolSize*sizeof(spvPoolV1[0])); if (!spvPoolV1) Sys_Error("OOM!");
    spvPoolV2 = (TVec *)Z_Realloc(spvPoolV2, spvPoolSize*sizeof(spvPoolV2[0])); if (!spvPoolV2) Sys_Error("OOM!");
  }
}


//==========================================================================
//
//  CalcSurfMinMax
//
//  surface must be valid
//
//==========================================================================
static bool CalcSurfMinMax (surface_t *surf, float &outmins, float &outmaxs, const TVec axis, const float offs=0.0f) {
  float mins = +99999.0f;
  float maxs = -99999.0f;
  const TVec *vt = surf->verts;
  for (int i = surf->count; i--; ++vt) {
    if (!vt->isValid()) {
      GCon->Log(NAME_Warning, "ERROR(SF): invalid surface vertex; THIS IS INTERNAL K8VAVOOM BUG!");
      surf->count = 0;
      outmins = outmaxs = 0.0f;
      return false;
    }
    const float dot = DotProduct(*vt, axis)+offs;
    if (dot < mins) mins = dot;
    if (dot > maxs) maxs = dot;
  }
  outmins = mins;
  outmaxs = maxs;
  return true;
}


//==========================================================================
//
//  VRenderLevelLightmap::InitSurfs
//
//==========================================================================
void VRenderLevelLightmap::InitSurfs (bool recalcStaticLightmaps, surface_t *ASurfs, texinfo_t *texinfo, const TPlane *plane, subsector_t *sub) {
  bool doPrecalc = (r_precalc_static_lights_override >= 0 ? !!r_precalc_static_lights_override : r_precalc_static_lights);

  for (surface_t *surf = ASurfs; surf; surf = surf->next) {
    if (texinfo) surf->texinfo = texinfo;
    if (plane) surf->plane = *plane;

    if (surf->count == 0) {
      GCon->Logf(NAME_Warning, "empty surface at subsector #%d", (int)(ptrdiff_t)(sub-Level->Subsectors));
      surf->texturemins[0] = 16;
      surf->extents[0] = 16;
      surf->texturemins[1] = 16;
      surf->extents[1] = 16;
      surf->subsector = sub;
      surf->drawflags &= ~surface_t::DF_CALC_LMAP; // just in case
    } else if (surf->count < 3) {
      GCon->Logf(NAME_Warning, "degenerate surface with #%d vertices at subsector #%d", surf->count, (int)(ptrdiff_t)(sub-Level->Subsectors));
      surf->texturemins[0] = 16;
      surf->extents[0] = 16;
      surf->texturemins[1] = 16;
      surf->extents[1] = 16;
      surf->subsector = sub;
      surf->drawflags &= ~surface_t::DF_CALC_LMAP; // just in case
    } else {
      /*short*/int old_texturemins[2];
      /*short*/int old_extents[2];

      // to do checking later
      old_texturemins[0] = surf->texturemins[0];
      old_texturemins[1] = surf->texturemins[1];
      old_extents[0] = surf->extents[0];
      old_extents[1] = surf->extents[1];

      float mins, maxs;

      if (!CalcSurfMinMax(surf, mins, maxs, texinfo->saxis, texinfo->soffs)) {
        // bad surface
        surf->drawflags &= ~surface_t::DF_CALC_LMAP; // just in case
        continue;
      }
      int bmins = (int)floor(mins/16);
      int bmaxs = (int)ceil(maxs/16);

      if (bmins < -EXTMAX/16 || bmins > EXTMAX/16 ||
          bmaxs < -EXTMAX/16 || bmaxs > EXTMAX/16 ||
          (bmaxs-bmins) < -EXTMAX/16 ||
          (bmaxs-bmins) > EXTMAX/16)
      {
        GCon->Logf(NAME_Warning, "Subsector %d got too big S surface extents: (%d,%d)", (int)(ptrdiff_t)(sub-Level->Subsectors), bmins, bmaxs);
        surf->texturemins[0] = 0;
        surf->extents[0] = 256;
      } else {
        surf->texturemins[0] = bmins*16;
        surf->extents[0] = (bmaxs-bmins)*16;
      }

      if (!CalcSurfMinMax(surf, mins, maxs, texinfo->taxis, texinfo->toffs)) {
        // bad surface
        surf->drawflags &= ~surface_t::DF_CALC_LMAP; // just in case
        continue;
      }
      bmins = (int)floor(mins/16);
      bmaxs = (int)ceil(maxs/16);

      if (bmins < -EXTMAX/16 || bmins > EXTMAX/16 ||
          bmaxs < -EXTMAX/16 || bmaxs > EXTMAX/16 ||
          (bmaxs-bmins) < -EXTMAX/16 ||
          (bmaxs-bmins) > EXTMAX/16)
      {
        GCon->Logf(NAME_Warning, "Subsector %d got too big T surface extents: (%d,%d)", (int)(ptrdiff_t)(sub-Level->Subsectors), bmins, bmaxs);
        surf->texturemins[1] = 0;
        surf->extents[1] = 256;
        //GCon->Logf("AXIS=(%g,%g,%g)", texinfo->taxis.x, texinfo->taxis.y, texinfo->taxis.z);
      } else {
        surf->texturemins[1] = bmins*16;
        surf->extents[1] = (bmaxs-bmins)*16;
        //GCon->Logf("AXIS=(%g,%g,%g)", texinfo->taxis.x, texinfo->taxis.y, texinfo->taxis.z);
      }

      /*
      if (!doPrecalc && inWorldCreation && !surf->lightmap) {
        surf->drawflags |= surface_t::DF_CALC_LMAP;
      } else {
        surf->drawflags &= ~surface_t::DF_CALC_LMAP; // just in case
        LightFace(surf, sub);
      }
      */

      // reset surface cache only if something was changed
      bool minMaxChanged =
        recalcStaticLightmaps ||
        old_texturemins[0] != surf->texturemins[0] ||
        old_texturemins[1] != surf->texturemins[1] ||
        old_extents[0] != surf->extents[0] ||
        old_extents[1] != surf->extents[1];

      if (minMaxChanged) FlushSurfCaches(surf);

      if (inWorldCreation && doPrecalc) {
        /*
        surf->drawflags &= ~surface_t::DF_CALC_LMAP; // just in case
        LightFace(surf, sub);
        */
        // we'll do it later
        surf->drawflags |= surface_t::DF_CALC_LMAP;
      } else {
        // recalculate lightmap when we'll need to
        if (recalcStaticLightmaps || inWorldCreation) surf->drawflags |= surface_t::DF_CALC_LMAP;
      }
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
struct SClipInfo {
  int vcount[2];
  TVec *verts[2];
};


//==========================================================================
//
//  SplitSurface
//
//  returns `false` if surface cannot be split
//  axis must be valid
//
//==========================================================================
static bool SplitSurface (SClipInfo &clip, surface_t *surf, const TVec &axis) {
  clip.vcount[0] = clip.vcount[1] = 0;
  if (!surf || surf->count < 3) return false; // cannot split

  float mins, maxs;
  if (!CalcSurfMinMax(surf, mins, maxs, axis)) {
    // invalid surface
    surf->count = 0;
    return false;
  }

  if (maxs-mins <= SUBDIVIDE_SIZE) return false;

  TPlane plane;
  plane.normal = axis;
  const float dot0 = Length(plane.normal);
  plane.normal.normaliseInPlace();
  plane.dist = (mins+SUBDIVIDE_SIZE-16)/dot0;

  enum {
    PlaneBack = -1,
    PlaneCoplanar = 0,
    PlaneFront = 1,
  };

  const int surfcount = surf->count;
  spvReserve(surfcount*2+2); //k8: `surf->count+1` is enough, but...

  float *dots = spvPoolDots;
  int *sides = spvPoolSides;
  TVec *verts1 = spvPoolV1;
  TVec *verts2 = spvPoolV2;

  const TVec *vt = surf->verts;

  int backSideCount = 0, frontSideCount = 0;
  for (int i = 0; i < surfcount; ++i, ++vt) {
    const float dot = DotProduct(*vt, plane.normal)-plane.dist;
    dots[i] = dot;
         if (dot < -ON_EPSILON) { ++backSideCount; sides[i] = PlaneBack; }
    else if (dot > ON_EPSILON) { ++frontSideCount; sides[i] = PlaneFront; }
    else sides[i] = PlaneCoplanar;
  }
  dots[surfcount] = dots[0];
  sides[surfcount] = sides[0];

  // completely on one side?
  if (!backSideCount || !frontSideCount) return false;

  TVec mid(0, 0, 0);
  clip.verts[0] = verts1;
  clip.verts[1] = verts2;

  vt = surf->verts;
  for (int i = 0; i < surfcount; ++i) {
    if (sides[i] == PlaneCoplanar) {
      clip.verts[0][clip.vcount[0]++] = vt[i];
      clip.verts[1][clip.vcount[1]++] = vt[i];
      continue;
    }

    unsigned cvidx = (sides[i] == PlaneFront ? 0 : 1);
    clip.verts[cvidx][clip.vcount[cvidx]++] = vt[i];

    if (sides[i+1] == PlaneCoplanar || sides[i] == sides[i+1]) continue;

    // generate a split point
    const TVec &p1 = vt[i];
    const TVec &p2 = vt[(i+1)%surfcount];

    const float dot = dots[i]/(dots[i]-dots[i+1]);
    for (int j = 0; j < 3; ++j) {
      // avoid round off error when possible
           if (plane.normal[j] == 1.0f) mid[j] = plane.dist;
      else if (plane.normal[j] == -1.0f) mid[j] = -plane.dist;
      else mid[j] = p1[j]+dot*(p2[j]-p1[j]);
    }

    clip.verts[0][clip.vcount[0]++] = mid;
    clip.verts[1][clip.vcount[1]++] = mid;
  }

  return (clip.vcount[0] >= 3 && clip.vcount[1] >= 3);
}


//==========================================================================
//
//  VRenderLevelLightmap::SubdivideFace
//
//==========================================================================
surface_t *VRenderLevelLightmap::SubdivideFace (surface_t *surf, const TVec &axis, const TVec *nextaxis) {
  subsector_t *sub = surf->subsector;
  seg_t *seg = surf->seg;
  vassert(sub);

  if (surf->count < 2) {
    //Sys_Error("surface with less than three (%d) vertices)", f->count);
    GCon->Logf(NAME_Warning, "surface with less than two (%d) vertices (divface) (sub=%d; sector=%d)", surf->count, (int)(ptrdiff_t)(sub-Level->Subsectors), (int)(ptrdiff_t)(sub->sector-Level->Sectors));
    return surf;
  }

  // this can happen for wall without texture
  if (!axis.isValid() || axis.isZero()) {
    GCon->Logf(NAME_Warning, "ERROR(SF): invalid axis (%f,%f,%f); THIS IS MAP BUG! (sub=%d; sector=%d)", axis.x, axis.y, axis.z, (int)(ptrdiff_t)(sub-Level->Subsectors), (int)(ptrdiff_t)(sub->sector-Level->Sectors));
    return (nextaxis ? SubdivideFace(surf, *nextaxis, nullptr) : surf);
  }

  SClipInfo clip;
  if (!SplitSurface(clip, surf, axis)) {
    // cannot subdivide, try next axis
    return (nextaxis ? SubdivideFace(surf, *nextaxis, nullptr) : surf);
  }

  vassert(clip.vcount[0] > 2);
  vassert(clip.vcount[1] > 2);

  ++c_subdivides;

  vuint32 drawflags = surf->drawflags;
  vuint32 typeFlags = surf->typeFlags;
  surface_t *next = surf->next;
  Z_Free(surf);

  surface_t *back = (surface_t *)Z_Calloc(sizeof(surface_t)+(clip.vcount[1]-1)*sizeof(TVec));
  back->drawflags = drawflags;
  back->typeFlags = typeFlags;
  back->subsector = sub;
  back->seg = seg;
  back->count = clip.vcount[1];
  memcpy(back->verts, clip.verts[1], back->count*sizeof(TVec));

  surface_t *front = (surface_t *)Z_Calloc(sizeof(surface_t)+(clip.vcount[0]-1)*sizeof(TVec));
  front->drawflags = drawflags;
  front->typeFlags = typeFlags;
  front->subsector = sub;
  front->seg = seg;
  front->count = clip.vcount[0];
  memcpy(front->verts, clip.verts[0], front->count*sizeof(TVec));

  front->next = next;
  back->next = SubdivideFace(front, axis, nextaxis);
  return (nextaxis ? SubdivideFace(back, *nextaxis, nullptr) : back);
}


//==========================================================================
//
//  VRenderLevelLightmap::SubdivideSeg
//
//==========================================================================
surface_t *VRenderLevelLightmap::SubdivideSeg (surface_t *surf, const TVec &axis, const TVec *nextaxis, seg_t *seg) {
  subsector_t *sub = surf->subsector;
  vassert(surf->seg == seg);
  vassert(sub);

  if (surf->count < 2) {
    //Sys_Error("surface with less than three (%d) vertices)", surf->count);
    GCon->Logf(NAME_Warning, "surface with less than two (%d) vertices (divseg) (sub=%d; sector=%d)", surf->count, (int)(ptrdiff_t)(sub-Level->Subsectors), (int)(ptrdiff_t)(sub->sector-Level->Sectors));
    return surf;
  }

  // this can happen for wall without texture
  if (!axis.isValid() || axis.isZero()) {
    GCon->Logf(NAME_Warning, "ERROR(SS): invalid axis (%f,%f,%f); THIS IS MAP BUG! (sub=%d; sector=%d)", axis.x, axis.y, axis.z, (int)(ptrdiff_t)(sub-Level->Subsectors), (int)(ptrdiff_t)(sub->sector-Level->Sectors));
    return (nextaxis ? SubdivideSeg(surf, *nextaxis, nullptr, seg) : surf);
  }

  SClipInfo clip;
  if (!SplitSurface(clip, surf, axis)) {
    // cannot subdivide, try next axis
    return (nextaxis ? SubdivideSeg(surf, *nextaxis, nullptr, seg) : surf);
  }

  vassert(clip.vcount[0] > 2);
  vassert(clip.vcount[1] > 2);

  ++c_seg_div;

  vassert(clip.vcount[1] <= surface_t::MAXWVERTS);
  surf->count = clip.vcount[1];
  memcpy(surf->verts, clip.verts[1], surf->count*sizeof(TVec));

  surface_t *news = NewWSurf();
  news->drawflags = surf->drawflags;
  news->typeFlags = surf->typeFlags;
  news->subsector = sub;
  news->seg = seg;
  news->count = clip.vcount[0];
  memcpy(news->verts, clip.verts[0], news->count*sizeof(TVec));

  news->next = surf->next;
  surf->next = SubdivideSeg(news, axis, nextaxis, seg);
  if (nextaxis) return SubdivideSeg(surf, *nextaxis, nullptr, seg);
  return surf;
}


//**************************************************************************
//
// lightmap cache saving
//
//**************************************************************************

//==========================================================================
//
//  VRenderLevelLightmap::isNeedLightmapCache
//
//==========================================================================
bool VRenderLevelLightmap::isNeedLightmapCache () const noexcept {
  return true;
}


// ////////////////////////////////////////////////////////////////////////// //
struct SurfaceInfoBlock {
  TPlane plane;
  subsector_t *subsector; // owning subsector
  seg_t *seg; // owning seg (can be `nullptr` for floor/ceiling)
  vint32 subidx;
  vint32 segidx;
  vuint32 typeFlags; // TF_xxx
  vuint32 lmsize, lmrgbsize; // to track static lightmap memory
  vuint8 *lightmap;
  rgb_t *lightmap_rgb;
  vint32 count;
  vint32 texturemins[2];
  vint32 extents[2];
  TVec vert0; // dynamic array
  bool lmowned;

  SurfaceInfoBlock () noexcept
    : subsector(nullptr)
    , seg(nullptr)
    , subidx(-1)
    , segidx(-1)
    , typeFlags(0)
    , lmsize(0)
    , lmrgbsize(0)
    , lightmap(nullptr)
    , lightmap_rgb(nullptr)
    , count(0)
    , vert0(0, 0, 0)
    , lmowned(false)
  {
    plane.normal = TVec(0, 0, 0);
    plane.dist = 0;
    texturemins[0] = texturemins[1] = 0;
    extents[0] = extents[1] = 0;
  }

  ~SurfaceInfoBlock () noexcept { clear(); }

  inline bool isValid () const noexcept { return (count > 0); }

  inline void disownLightmaps () noexcept { lmowned = false; }

  void dump () {
    GCon->Log(NAME_Debug, "=== SURFACE INFO ===");
    GCon->Logf(NAME_Debug, "  plane: (%g,%g,%g) : %g", plane.normal.x, plane.normal.y, plane.normal.z, plane.dist);
    GCon->Logf(NAME_Debug, "  subsector/seg: %d / %d", subidx, segidx);
    GCon->Logf(NAME_Debug, "  typeFlags: %u", typeFlags);
    GCon->Logf(NAME_Debug, "  vertices: %d : (%g,%g,%g)", count, vert0.x, vert0.y, vert0.z);
    GCon->Logf(NAME_Debug, "  tmins/extents: (%d,%d) / (%d,%d)", texturemins[0], texturemins[1], extents[0], extents[1]);
  }

  void clear () noexcept {
    if (lmowned) {
      if (lightmap) Z_Free(lightmap);
      if (lightmap_rgb) Z_Free(lightmap_rgb);
    }
    subsector = nullptr;
    seg = nullptr;
    subidx = -1;
    segidx = -1;
    typeFlags = 0;
    lmsize = lmrgbsize = 0;
    lightmap = nullptr;
    lightmap_rgb = nullptr;
    count = 0;
    vert0 = TVec(0, 0, 0);
    plane.normal = TVec(0, 0, 0);
    plane.dist = 0;
    texturemins[0] = texturemins[1] = 0;
    extents[0] = extents[1] = 0;
    lmowned = false;
  }

  inline bool equalTo (const surface_t *sfc) noexcept {
    if (!sfc || count < 1 || count != sfc->count) return false;
    return
      typeFlags == sfc->typeFlags &&
      texturemins[0] == sfc->texturemins[0] &&
      texturemins[1] == sfc->texturemins[1] &&
      extents[0] == sfc->extents[0] &&
      extents[1] == sfc->extents[1] &&
      subsector == sfc->subsector &&
      seg == sfc->seg &&
      plane.normal == sfc->plane.normal &&
      plane.dist == sfc->plane.dist &&
      vert0 == sfc->verts[0];
  }

  void initWith (VLevel *Level, const surface_t *sfc) noexcept {
    vassert(sfc);
    clear();
    plane = sfc->plane;
    subsector = sfc->subsector;
    subidx = (sfc->subsector ? (vint32)(ptrdiff_t)(sfc->subsector-&Level->Subsectors[0]) : -1);
    seg = sfc->seg;
    segidx = (sfc->seg ? (vint32)(ptrdiff_t)(sfc->seg-&Level->Segs[0]) : -1);
    typeFlags = sfc->typeFlags;
    if (sfc->lightmap) {
      lmsize = sfc->lmsize;
      lightmap = sfc->lightmap;
      if (sfc->lightmap_rgb) {
        lmrgbsize = sfc->lmrgbsize;
        lightmap_rgb = sfc->lightmap_rgb;
      } else {
        lmrgbsize = 0;
        lightmap_rgb = nullptr;
      }
    } else {
      lmsize = 0;
      lightmap = nullptr;
      lmrgbsize = 0;
      lightmap_rgb = nullptr;
    }
    count = sfc->count;
    vert0 = (sfc->count > 0 ? sfc->verts[0] : TVec(0, 0, 0));
    texturemins[0] = sfc->texturemins[0];
    texturemins[1] = sfc->texturemins[1];
    extents[0] = sfc->extents[0];
    extents[1] = sfc->extents[1];
    lmowned = false;
  }

  // must be initialised with `initWith()`
  void writeTo (VStream *strm, VLevel *Level) {
    vassert(strm);
    vassert(!strm->IsLoading());
    vuint8 flag = 0;
    // monochrome lightmap is always there when rgb lightmap is there
    if (!lightmap) {
      vassert(!lightmap_rgb);
    } else {
      flag |= 1u;
      vassert(lmsize > 0);
      if (lightmap_rgb) {
        vassert(lmrgbsize > 0);
        flag |= 2u;
      }
    }
    *strm << flag;
    // surface check data
    // plane
    *strm << plane.normal.x << plane.normal.y << plane.normal.z << plane.dist;
    // subsector
    *strm << subidx;
    // seg
    *strm << segidx;
    // type
    *strm << typeFlags;
    // vertices
    *strm << count;
    // extents
    *strm << texturemins[0];
    *strm << texturemins[1];
    *strm << extents[0];
    *strm << extents[1];
    // first vertex
    *strm << vert0.x << vert0.y << vert0.z;
    // write lightmaps
    if (flag&1u) {
      *strm << lmsize;
      strm->Serialise(lightmap, lmsize);
      if (flag&2u) {
        *strm << lmrgbsize;
        strm->Serialise(lightmap_rgb, lmrgbsize);
      }
    }
  }

  // must be initialised with `initWith()`
  bool readFrom (VStream *strm, VLevel *Level) {
    vassert(strm);
    vassert(strm->IsLoading());
    clear();
    lmowned = true;
    vuint8 flag = 0xff;
    *strm << flag;
    if ((flag&~3u) != 0 || flag == 2u) { GCon->Log(NAME_Warning, "invalid lightmap cache surface flags"); return false; }
    //if (!flag) return true; // it is valid, but empty
    // plane
    *strm << plane.normal.x << plane.normal.y << plane.normal.z << plane.dist;
    // subsector
    *strm << subidx;
    // seg
    *strm << segidx;
    // type
    *strm << typeFlags;
    // vertices
    *strm << count;
    // extents
    *strm << texturemins[0];
    *strm << texturemins[1];
    *strm << extents[0];
    *strm << extents[1];
    // first vertex
    *strm << vert0.x << vert0.y << vert0.z;
    // lightmaps
    if (flag&1u) {
      *strm << lmsize;
      if (strm->IsError()) { GCon->Log(NAME_Error, "error reading lightmap cache"); clear(); return false; }
      if (count < 0) { GCon->Log(NAME_Error, "invalid lightmap cache surface vertex count"); clear(); return false; }
      if (lmsize == 0 || lmsize > BLOCK_WIDTH*BLOCK_HEIGHT) { GCon->Log(NAME_Warning, "invalid lightmap cache surface lightmap size"); clear(); return false; }
      lightmap = (vuint8 *)Z_Malloc(lmsize);
      strm->Serialise(lightmap, lmsize);
      if (flag&2u) {
        *strm << lmrgbsize;
        if (strm->IsError()) { GCon->Log(NAME_Error, "error reading lightmap cache"); clear(); return false; }
        if (lmrgbsize == 0 || lmrgbsize > BLOCK_WIDTH*BLOCK_HEIGHT*3) { GCon->Log(NAME_Warning, "invalid lightmap cache surface lightmap size"); clear(); return false; }
        lightmap_rgb = (rgb_t *)Z_Malloc(lmrgbsize);
        strm->Serialise(lightmap_rgb, lmrgbsize);
      }
    }
    if (strm->IsError()) { GCon->Log(NAME_Error, "error reading lightmap cache"); clear(); return false; }
    // link to subsector
         if (subidx == -1) subsector = nullptr;
    else if (subidx < 0 || subidx >= Level->NumSubsectors) { GCon->Log(NAME_Error, "invalid lightmap cache surface subsector"); clear(); return false; }
    else subsector = &Level->Subsectors[subidx];
    // link to seg
         if (segidx == -1) seg = nullptr;
    else if (segidx < 0 || segidx >= Level->NumSegs) { GCon->Log(NAME_Error, "invalid lightmap cache surface seg"); clear(); return false; }
    else seg = &Level->Segs[segidx];
    // done
    return true;
  }
};


//==========================================================================
//
//  WriteSurfaceLightmaps
//
//==========================================================================
static void WriteSurfaceLightmaps (VLevel *Level, VStream *strm, surface_t *s) {
  vuint32 cnt = 0;
  for (surface_t *t = s; t; t = t->next) ++cnt;
  *strm << cnt;
  SurfaceInfoBlock sib;
  for (; s; s = s->next) {
    sib.initWith(Level, s);
    sib.writeTo(strm, Level);
  }
}


//==========================================================================
//
//  WriteSegLightmaps
//
//==========================================================================
static void WriteSegLightmaps (VLevel *Level, VStream *strm, segpart_t *sp) {
  vuint32 cnt = 0;
  for (segpart_t *t = sp; t; t = t->next) ++cnt;
  *strm << cnt;
  for (; sp; sp = sp->next) WriteSurfaceLightmaps(Level, strm, sp->surfs);
}


//==========================================================================
//
//  VRenderLevelLightmap::saveLightmapsInternal
//
//==========================================================================
void VRenderLevelLightmap::saveLightmapsInternal (VStream *strm) {
  if (!strm) return;
  vuint32 surfCount = CountAllSurfaces();

  *strm << Level->NumSectors;
  *strm << Level->NumSubsectors;
  *strm << Level->NumSegs;
  *strm << surfCount;

  /*
  vuint32 atlasW = BLOCK_WIDTH, atlasH = BLOCK_HEIGHT;
  *strm << atlasW << atlasH;
  */

  for (int i = 0; i < Level->NumSubsectors; ++i) {
    subsector_t *sub = &Level->Subsectors[i];
    vuint32 ssnum = (vuint32)i;
    *strm << ssnum;
    // count regions (so we can skip them if necessary)
    vuint32 regcount = 0;
    for (subregion_t *r = sub->regions; r != nullptr; r = r->next) ++regcount;
    *strm << regcount;
    regcount = 0;
    for (subregion_t *r = sub->regions; r != nullptr; r = r->next, ++regcount) {
      *strm << regcount;
      WriteSurfaceLightmaps(Level, strm, (r->realfloor ? r->realfloor->surfs : nullptr));
      WriteSurfaceLightmaps(Level, strm, (r->realceil ? r->realceil->surfs : nullptr));
      WriteSurfaceLightmaps(Level, strm, (r->fakefloor ? r->fakefloor->surfs : nullptr));
      WriteSurfaceLightmaps(Level, strm, (r->fakeceil ? r->fakeceil->surfs : nullptr));
    }
  }

  for (int i = 0; i < Level->NumSegs; ++i) {
    seg_t *seg = &Level->Segs[i];
    vuint32 snum = (vuint32)i;
    *strm << snum;
    // count drawsegs (so we can skip them if necessary)
    vuint32 dscount = 0;
    for (drawseg_t *ds = seg->drawsegs; ds; ds = ds->next) ++dscount;
    *strm << dscount;
    dscount = 0;
    for (drawseg_t *ds = seg->drawsegs; ds; ds = ds->next, ++dscount) {
      *strm << dscount;
      WriteSegLightmaps(Level, strm, ds->top);
      WriteSegLightmaps(Level, strm, ds->mid);
      WriteSegLightmaps(Level, strm, ds->bot);
      WriteSegLightmaps(Level, strm, ds->topsky);
      WriteSegLightmaps(Level, strm, ds->extra);
    }
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::saveLightmaps
//
//==========================================================================
void VRenderLevelLightmap::saveLightmaps (VStream *strm) {
  if (!strm) return;
  strm->Serialise(LMAP_CACHE_DATA_SIGNATURE, CDSLEN);
  VZipStreamWriter *zipstrm = new VZipStreamWriter(strm, (int)loader_cache_compression_level_lightmap);
  saveLightmapsInternal(zipstrm);
  zipstrm->Close();
  delete zipstrm;
}


//**************************************************************************
//
// lightmap cache loading
//
//**************************************************************************

//==========================================================================
//
//  SkipLightSurfaces
//
//==========================================================================
static bool SkipLightSurfaces (VLevel *Level, VStream *strm, vuint32 *number=nullptr) {
  vuint32 rd = 0;
  if (number) {
    rd = *number;
  } else {
    *strm << rd;
    if (strm->IsError()) { GCon->Log(NAME_Error, "error reading lightmap cache"); return false; }
  }
  if (rd > 1024*1024) { GCon->Logf(NAME_Warning, "invalid lightmap cache surface chain count (%u)", rd); return false; }
  SurfaceInfoBlock sib;
  while (rd--) {
    sib.clear();
    if (!sib.readFrom(strm, Level)) return false;
    sib.clear();
  }
  return true;
}


//==========================================================================
//
//  SkipLightSegSurfaces
//
//==========================================================================
static bool SkipLightSegSurfaces (VLevel *Level, VStream *strm, vuint32 *number=nullptr) {
  vuint32 rd = 0;
  if (number) {
    rd = *number;
  } else {
    *strm << rd;
    if (strm->IsError()) { GCon->Log(NAME_Error, "error reading lightmap cache"); return false; }
  }
  while (rd--) {
    if (!SkipLightSurfaces(Level, strm)) return false;
  }
  return true;
}


//==========================================================================
//
//  LoadLightSurfaces
//
//==========================================================================
static bool LoadLightSurfaces (VLevel *Level, VStream *strm, surface_t *s, unsigned &lmcacheUnknownSurfaceCount, bool &missingWarned) {
  vuint32 cnt = 0, rd = 0;
  for (surface_t *t = s; t; t = t->next) ++cnt;
  *strm << rd;
  if (rd != cnt) {
    GCon->Logf(NAME_Warning, "invalid lightmap cache surface chain count (%u instead of %u)", rd, cnt);
    //return false;
  }
  if (rd > 1024*1024) { GCon->Logf(NAME_Warning, "invalid lightmap cache surface chain count (%u)", rd); return false; }
  // just in case
  if (rd == 0) {
    lmcacheUnknownSurfaceCount += cnt;
    return true;
  }
  if (cnt == 0) return SkipLightSurfaces(Level, strm, &rd);
  // load surfaces
  TArray<SurfaceInfoBlock> sibs;
  sibs.setLength((int)rd);
  #ifdef VV_DUMP_LMAP_CACHE_COMPARISONS
  GCon->Logf(NAME_Debug, "============== ON-DISK (%u) ==============", rd);
  #endif
  for (int f = 0; f < (int)rd; ++f) {
    if (!sibs[f].readFrom(strm, Level)) return false;
    #ifdef VV_DUMP_LMAP_CACHE_COMPARISONS
    sibs[f].dump();
    #endif
  }
  #ifdef VV_DUMP_LMAP_CACHE_COMPARISONS
  GCon->Logf(NAME_Debug, "============== MEMORY (%u) ==============", cnt);
  for (surface_t *t = s; t; t = t->next) {
    SurfaceInfoBlock sss;
    sss.initWith(Level, t);
    sss.dump();
  }
  #endif
  // setup surfaces
  for (; s; s = s->next) {
    SurfaceInfoBlock *sp = nullptr;
    for (auto &&sb : sibs) {
      if (sb.isValid() && sb.equalTo(s)) {
             if (sp) { GCon->Log(NAME_Warning, "invalid lightmap cache surface: duplicate info!"); }
        else sp = &sb;
      } else {
        #ifdef VV_DUMP_LMAP_CACHE_COMPARISONS
        GCon->Logf(NAME_Debug, "::: compare ::: count=%d; typeFlags=%d; tm0=%d; tm1=%d; ext0=%d; ext1=%d; sub=%d; seg=%d; pnorm=%d; pdist=%d; vert=%d",
          (int)(s->count == sb.count),
          (int)(s->typeFlags == sb.typeFlags),
          (int)(s->texturemins[0] == sb.texturemins[0]),
          (int)(s->texturemins[1] == sb.texturemins[1]),
          (int)(s->extents[0] == sb.extents[0]),
          (int)(s->extents[1] == sb.extents[1]),
          (int)(s->subsector == sb.subsector),
          (int)(s->seg == sb.seg),
          (int)(s->plane.normal == sb.plane.normal),
          (int)(s->plane.dist == sb.plane.dist),
          (int)(s->verts[0] == sb.vert0));
        #endif
      }
    }
    if (!sp) {
      if (!missingWarned) {
        missingWarned = true;
        GCon->Logf(NAME_Warning, "*** lightmap cache is missing some surface info...");
      }
      ++lmcacheUnknownSurfaceCount;
      #ifdef VV_DUMP_LMAP_CACHE_COMPARISONS
      SurfaceInfoBlock sss;
      sss.initWith(Level, s);
      sss.dump();
      #elif defined(VV_DUMP_LMAP_CACHE_COMPARISON_FAIL)
      if (dbg_cache_lightmap_dump_missing) {
        GCon->Logf(NAME_Debug, "============== ON-DISK (%u) ==============", rd);
        for (auto &&ss : sibs) {
          if (ss.isValid()) {
            ss.dump();
          } else {
            GCon->Log(NAME_Debug, "*** SKIPPED");
          }
        }
        GCon->Log(NAME_Debug, "============== MEMORY (FAIL) ==============");
        SurfaceInfoBlock sss;
        sss.initWith(Level, s);
        sss.dump();
      }
      #endif
    } else {
      #ifdef VV_DUMP_LMAP_CACHE_COMPARISONS
      GCon->Log(NAME_Debug, "*** FOUND!");
      {
        GCon->Log(NAME_Debug, "--- surface ----");
        SurfaceInfoBlock sss;
        sss.initWith(Level, s);
        sss.dump();
        GCon->Log(NAME_Debug, "--- on-disk ----");
        sp->dump();
      }
      #endif
      if (sp->isValid() && sp->lightmap) {
        vassert(!s->lightmap);
        vassert(!s->lightmap_rgb);
        // monochrome
        s->lmsize = sp->lmsize;
        s->lightmap = sp->lightmap;
        light_mem += sp->lmsize;
        // rgb
        if (sp->lightmap_rgb) {
          s->lmrgbsize = sp->lmrgbsize;
          s->lightmap_rgb = sp->lightmap_rgb;
          light_mem += sp->lmrgbsize;
        }
        sp->disownLightmaps();
        sp->clear();
      }
      s->drawflags &= ~surface_t::DF_CALC_LMAP;
    }
  }
  // done
  return !strm->IsError();
}


//==========================================================================
//
//  LoadLightSegSurfaces
//
//==========================================================================
static bool LoadLightSegSurfaces (VLevel *Level, VStream *strm, segpart_t *sp, unsigned &lmcacheUnknownSurfaceCount, bool &missingWarned) {
  vuint32 cnt = 0, rd = 0;
  for (segpart_t *s = sp; s; s = s->next) ++cnt;
  *strm << rd;
  if (rd != cnt) {
    GCon->Logf(NAME_Warning, "invalid lightmap cache segment surface count (%u instead of %u)", rd, cnt);
    lmcacheUnknownSurfaceCount += cnt;
    return SkipLightSegSurfaces(Level, strm, &rd);
  } else {
    for (; sp; sp = sp->next) if (!LoadLightSurfaces(Level, strm, sp->surfs, lmcacheUnknownSurfaceCount, missingWarned)) return false;
    return true;
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::loadLightmapsInternal
//
//==========================================================================
bool VRenderLevelLightmap::loadLightmapsInternal (VStream *strm) {
  if (!strm || strm->IsError()) return false;

  vuint32 surfCount = CountAllSurfaces();

  // load and check header
  vuint32 seccount = 0, sscount = 0, sgcount = 0, sfcount = 0;
  *strm << seccount << sscount << sgcount << sfcount;
  if (strm->IsError()) { GCon->Log(NAME_Error, "error reading lightmap cache"); return false; }
  if ((int)seccount != Level->NumSectors || strm->IsError()) { GCon->Log(NAME_Warning, "invalid lightmap cache sector count"); return false; }
  if ((int)sscount != Level->NumSubsectors || strm->IsError()) { GCon->Log(NAME_Warning, "invalid lightmap cache subsector count"); return false; }
  if ((int)sgcount != Level->NumSegs || strm->IsError()) { GCon->Log(NAME_Warning, "invalid lightmap cache seg count"); return false; }
  if (sfcount != surfCount || strm->IsError()) { GCon->Logf(NAME_Warning, "invalid lightmap cache surface count (%u instead of %u)", sfcount, surfCount); /*return false;*/ }
  GCon->Log(NAME_Debug, "lightmap cache validated, trying to load it...");

  bool missingWarned = false;

  for (int i = 0; i < Level->NumSubsectors; ++i) {
    subsector_t *sub = &Level->Subsectors[i];
    // count regions (so we can skip them if necessary)
    vuint32 regcount = 0;
    for (subregion_t *r = sub->regions; r != nullptr; r = r->next) ++regcount;
    vuint32 snum = 0xffffffffu;
    *strm << snum;
    if ((int)snum != i) { GCon->Log(NAME_Warning, "invalid lightmap cache subsector number"); return false; }
    // check region count
    vuint32 ccregcount = 0xffffffffu;
    *strm << ccregcount;
    if (strm->IsError()) { GCon->Log(NAME_Error, "error reading lightmap cache"); return false; }
    if (ccregcount != regcount) {
      GCon->Logf(NAME_Warning, "lightmap cache subsector #%d region count mismatch (%u instead of %u)", i, ccregcount, regcount);
      if (regcount != 0) {
        for (subregion_t *r = sub->regions; r != nullptr; r = r->next, ++regcount) {
          if (r->realfloor != nullptr) lmcacheUnknownSurfaceCount += CountSurfacesInChain(r->realfloor->surfs);
          if (r->realceil != nullptr) lmcacheUnknownSurfaceCount += CountSurfacesInChain(r->realceil->surfs);
          if (r->fakefloor != nullptr) lmcacheUnknownSurfaceCount += CountSurfacesInChain(r->fakefloor->surfs);
          if (r->fakeceil != nullptr) lmcacheUnknownSurfaceCount += CountSurfacesInChain(r->fakeceil->surfs);
        }
      }
      // skip them
      while (ccregcount--) {
        vuint32 n = 0xffffffffu;
        *strm << n;
        if (strm->IsError()) { GCon->Log(NAME_Error, "error reading lightmap cache"); return false; }
        if (!SkipLightSurfaces(Level, strm)) return false; // realfloor
        if (!SkipLightSurfaces(Level, strm)) return false; // realceil
        if (!SkipLightSurfaces(Level, strm)) return false; // fakefloor
        if (!SkipLightSurfaces(Level, strm)) return false; // fakeceil
      }
    } else {
      regcount = 0;
      for (subregion_t *r = sub->regions; r != nullptr; r = r->next, ++regcount) {
        vuint32 n = 0xffffffffu;
        *strm << n;
        if (strm->IsError()) { GCon->Log(NAME_Error, "error reading lightmap cache"); return false; }
        if (n != regcount) { GCon->Log(NAME_Warning, "invalid lightmap cache region number"); return false; }
        if (!LoadLightSurfaces(Level, strm, (r->realfloor ? r->realfloor->surfs : nullptr), lmcacheUnknownSurfaceCount, missingWarned)) return false;
        if (!LoadLightSurfaces(Level, strm, (r->realceil ? r->realceil->surfs : nullptr), lmcacheUnknownSurfaceCount, missingWarned)) return false;
        if (!LoadLightSurfaces(Level, strm, (r->fakefloor ? r->fakefloor->surfs : nullptr), lmcacheUnknownSurfaceCount, missingWarned)) return false;
        if (!LoadLightSurfaces(Level, strm, (r->fakeceil ? r->fakeceil->surfs : nullptr), lmcacheUnknownSurfaceCount, missingWarned)) return false;
      }
    }
  }

  for (int i = 0; i < Level->NumSegs; ++i) {
    seg_t *seg = &Level->Segs[i];
    // count drawsegs (so we can skip them if necessary)
    vuint32 dscount = 0;
    for (drawseg_t *ds = seg->drawsegs; ds; ds = ds->next) ++dscount;
    vuint32 snum = 0xffffffffu;
    *strm << snum;
    if ((int)snum != i) { GCon->Log(NAME_Warning, "invalid lightmap cache seg number"); return false; }
    // check drawseg count
    vuint32 ccdscount = 0xffffffffu;
    *strm << ccdscount;
    if (strm->IsError()) { GCon->Log(NAME_Error, "error reading lightmap cache"); return false; }
    if (ccdscount != dscount) {
      GCon->Logf(NAME_Warning, "lightmap cache seg #%d drawseg count mismatch (%u instead of %u)", i, ccdscount, dscount);
      if (dscount != 0) {
        for (drawseg_t *ds = seg->drawsegs; ds; ds = ds->next, ++dscount) {
          lmcacheUnknownSurfaceCount += CountSegSurfacesInChain(ds->top);
          lmcacheUnknownSurfaceCount += CountSegSurfacesInChain(ds->mid);
          lmcacheUnknownSurfaceCount += CountSegSurfacesInChain(ds->bot);
          lmcacheUnknownSurfaceCount += CountSegSurfacesInChain(ds->topsky);
          lmcacheUnknownSurfaceCount += CountSegSurfacesInChain(ds->extra);
        }
      }
      // skip them
      while (ccdscount--) {
        vuint32 n = 0xffffffffu;
        *strm << n;
        if (strm->IsError()) { GCon->Log(NAME_Error, "error reading lightmap cache"); return false; }
        if (!SkipLightSegSurfaces(Level, strm)) return false; // top
        if (!SkipLightSegSurfaces(Level, strm)) return false; // mid
        if (!SkipLightSegSurfaces(Level, strm)) return false; // bot
        if (!SkipLightSegSurfaces(Level, strm)) return false; // topsky
        if (!SkipLightSegSurfaces(Level, strm)) return false; // extra
      }
    } else {
      dscount = 0;
      for (drawseg_t *ds = seg->drawsegs; ds; ds = ds->next, ++dscount) {
        vuint32 n = 0xffffffffu;
        *strm << n;
        if (strm->IsError()) { GCon->Log(NAME_Error, "error reading lightmap cache"); return false; }
        if (n != dscount) { GCon->Log(NAME_Warning, "invalid lightmap cache drawseg number"); return false; }
        if (!LoadLightSegSurfaces(Level, strm, ds->top, lmcacheUnknownSurfaceCount, missingWarned)) return false;
        if (!LoadLightSegSurfaces(Level, strm, ds->mid, lmcacheUnknownSurfaceCount, missingWarned)) return false;
        if (!LoadLightSegSurfaces(Level, strm, ds->bot, lmcacheUnknownSurfaceCount, missingWarned)) return false;
        if (!LoadLightSegSurfaces(Level, strm, ds->topsky, lmcacheUnknownSurfaceCount, missingWarned)) return false;
        if (!LoadLightSegSurfaces(Level, strm, ds->extra, lmcacheUnknownSurfaceCount, missingWarned)) return false;
      }
    }
  }

  return !strm->IsError();
}


//==========================================================================
//
//  VRenderLevelLightmap::loadLightmaps
//
//==========================================================================
bool VRenderLevelLightmap::loadLightmaps (VStream *strm) {
  if (!strm || strm->IsError()) {
    lmcacheUnknownSurfaceCount = CountAllSurfaces();
    return false;
  }
  char sign[CDSLEN];
  strm->Serialise(sign, CDSLEN);
  if (strm->IsError() || memcmp(sign, LMAP_CACHE_DATA_SIGNATURE, CDSLEN) != 0) {
    GCon->Logf(NAME_Error, "invalid lightmap cache file signature");
    lmcacheUnknownSurfaceCount = CountAllSurfaces();
    return false;
  }
  lmcacheUnknownSurfaceCount = 0;
  VZipStreamReader *zipstrm = new VZipStreamReader(true, strm, VZipStreamReader::UNKNOWN_SIZE, VZipStreamReader::UNKNOWN_SIZE/*Map->DecompressedSize*/);
  bool ok = loadLightmapsInternal(zipstrm);
  zipstrm->Close();
  if (!ok && !lmcacheUnknownSurfaceCount) lmcacheUnknownSurfaceCount = CountAllSurfaces();
  return ok;
}


//**************************************************************************
//
// calculate static lightmaps
//
//**************************************************************************

//==========================================================================
//
//  LightSurfaces
//
//==========================================================================
static int LightSurfaces (VRenderLevelLightmap *rdr, surface_t *s, bool recalcNow, bool onlyMarked) {
  int res = 0;
  if (recalcNow) {
    for (; s; s = s->next) {
      ++res;
      if (onlyMarked && (s->drawflags&surface_t::DF_CALC_LMAP) == 0) continue;
      s->drawflags &= ~surface_t::DF_CALC_LMAP;
      if (s->count >= 3) rdr->LightFace(s, s->subsector);
    }
  } else {
    for (; s; s = s->next) {
      ++res;
      if (onlyMarked && (s->drawflags&surface_t::DF_CALC_LMAP) == 0) continue;
      if (s->count >= 3) {
        s->drawflags |= surface_t::DF_CALC_LMAP;
      } else {
        s->drawflags &= ~surface_t::DF_CALC_LMAP;
      }
    }
  }
  return res;
}


//==========================================================================
//
//  LightSegSurfaces
//
//==========================================================================
static int LightSegSurfaces (VRenderLevelLightmap *rdr, segpart_t *sp, bool recalcNow, bool onlyMarked) {
  int res = 0;
  for (; sp; sp = sp->next) res += LightSurfaces(rdr, sp->surfs, recalcNow, onlyMarked);
  return res;
}


//==========================================================================
//
//  VRenderLevelLightmap::RelightMap
//
//==========================================================================
void VRenderLevelLightmap::RelightMap (bool recalcNow, bool onlyMarked) {
  vuint32 surfCount = 0;

  if (recalcNow) {
    surfCount = CountAllSurfaces();
    R_PBarReset();
    R_PBarUpdate("Lightmaps", 0, (int)surfCount);
  }

  int processed = 0;
  for (auto &&sub : Level->allSubsectors()) {
    for (subregion_t *r = sub.regions; r != nullptr; r = r->next) {
      if (r->realfloor != nullptr) processed += LightSurfaces(this, r->realfloor->surfs, recalcNow, onlyMarked);
      if (r->realceil != nullptr) processed += LightSurfaces(this, r->realceil->surfs, recalcNow, onlyMarked);
      if (r->fakefloor != nullptr) processed += LightSurfaces(this, r->fakefloor->surfs, recalcNow, onlyMarked);
      if (r->fakeceil != nullptr) processed += LightSurfaces(this, r->fakeceil->surfs, recalcNow, onlyMarked);
    }
    if (recalcNow) R_PBarUpdate("Lightmaps", processed, (int)surfCount);
  }

  for (auto &&seg : Level->allSegs()) {
    for (drawseg_t *ds = seg.drawsegs; ds; ds = ds->next) {
      processed += LightSegSurfaces(this, ds->top, recalcNow, onlyMarked);
      processed += LightSegSurfaces(this, ds->mid, recalcNow, onlyMarked);
      processed += LightSegSurfaces(this, ds->bot, recalcNow, onlyMarked);
      processed += LightSegSurfaces(this, ds->topsky, recalcNow, onlyMarked);
      processed += LightSegSurfaces(this, ds->extra, recalcNow, onlyMarked);
    }
    if (recalcNow) R_PBarUpdate("Lightmaps", processed, (int)surfCount);
  }

  if (recalcNow) R_PBarUpdate("Lightmaps", (int)surfCount, (int)surfCount, true);
}


//==========================================================================
//
//  VRenderLevelLightmap::ResetLightmaps
//
//==========================================================================
void VRenderLevelLightmap::ResetLightmaps (bool recalcNow) {
  RelightMap(recalcNow, false);
}


//==========================================================================
//
//  VRenderLevelLightmap::PreRender
//
//==========================================================================
void VRenderLevelLightmap::PreRender () {
  c_subdivides = 0;
  c_seg_div = 0;
  light_mem = 0;

  CreateWorldSurfaces();

  // lightmapping
  bool doReadCache = (!Level->cacheFileBase.isEmpty() && loader_cache_data.asBool() && (Level->cacheFlags&VLevel::CacheFlag_Ignore) == 0);
  bool doWriteCache = (!Level->cacheFileBase.isEmpty() && loader_cache_data.asBool());
  Level->cacheFlags &= ~VLevel::CacheFlag_Ignore;

  bool doPrecalc = (r_precalc_static_lights_override >= 0 ? !!r_precalc_static_lights_override : r_precalc_static_lights);
  VStr ccfname = (Level->cacheFileBase.isEmpty() ? VStr::EmptyString : Level->cacheFileBase+".lmap");
  if (ccfname.isEmpty()) { doReadCache = doWriteCache = false; }
  if (!doPrecalc) doWriteCache = false;

  if (doReadCache || doWriteCache) GCon->Logf(NAME_Debug, "lightmap cache file: '%s'", *ccfname);

  if (doPrecalc || doReadCache || doWriteCache) {
    R_LdrMsgShowSecondary("CREATING LIGHTMAPS...");

    bool recalcLight = true;
    if (doReadCache) {
      VStream *lmc = FL_OpenSysFileRead(ccfname);
      if (lmc) {
        recalcLight = !loadLightmaps(lmc);
        if (lmc->IsError()) recalcLight = true;
        lmc->Close();
        delete lmc;
        if (recalcLight) Sys_FileDelete(ccfname);
      }
    }

    if (recalcLight || lmcacheUnknownSurfaceCount) {
      if (lmcacheUnknownSurfaceCount) {
        GCon->Logf("calculating static lighting due to lightmap cache inconsistencies (%u out of %u surfaces)...", lmcacheUnknownSurfaceCount, CountAllSurfaces());
      } else {
        GCon->Log("calculating static lighting...");
      }
      double stt = -Sys_Time();
      RelightMap(true, true); // only marked
      // cache
      if (doWriteCache) {
        const float tlim = loader_cache_time_limit_lightmap.asFloat();
        stt += Sys_Time();
        // if our lightmap cache is partially valid, rewrite it unconditionally
        if (dbg_cache_lightmap_always || lmcacheUnknownSurfaceCount || stt >= tlim) {
          VStream *lmc = FL_OpenSysFileWrite(ccfname);
          if (lmc) {
            GCon->Logf("writing lightmap cache to '%s'...", *ccfname);
            saveLightmaps(lmc);
            bool err = lmc->IsError();
            lmc->Close();
            err = (err || lmc->IsError());
            delete lmc;
            if (err) {
              GCon->Logf(NAME_Warning, "removed broken lightmap cache '%s'...", *ccfname);
              Sys_FileDelete(ccfname);
            }
          } else {
            GCon->Logf(NAME_Warning, "cannot create lightmap cache file '%s'...", *ccfname);
          }
        }
      }
    }
  }

  GCon->Logf("%d subdivides", c_subdivides);
  GCon->Logf("%d seg subdivides", c_seg_div);
  GCon->Logf("%dk light mem", light_mem/1024);
}
