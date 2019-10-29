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

#define ON_EPSILON      (0.1f)
#define SUBDIVIDE_SIZE  (240)

#define EXTMAX  (32767)
//#define EXTMAX  (65536)
// float mantissa is 24 bits, but let's play safe, and use 20 bits
//#define EXTMAX  (0x100000)


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB r_precalc_static_lights("r_precalc_static_lights", true, "Precalculate static lights?", CVAR_Archive);
int r_precalc_static_lights_override = -1; // <0: not set

extern VCvarB loader_cache_data;
VCvarF loader_cache_time_limit_lightmap("loader_cache_time_limit_lightmap", "3", "Cache lightmap data if building took more than this number of seconds.", CVAR_Archive);
VCvarI loader_cache_compression_level_lightmap("loader_cache_compression_level_lightmap", "6", "Lightmap cache file compression level [0..9]", CVAR_Archive);

static VCvarB dbg_always_cache_lightmaps("dbg_always_cache_lightmaps", false, "Always cache lightmaps?", /*CVAR_Archive*/0);


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


//==========================================================================
//
//  WriteSurfaceLightmaps
//
//==========================================================================
static void WriteSurfaceLightmaps (VLevel *Level, VStream *strm, surface_t *s) {
  vuint32 cnt = 0;
  for (surface_t *t = s; t; t = t->next) ++cnt;
  *strm << cnt;
  for (; s; s = s->next) {
    vuint8 flag = 0;
    // monochrome lightmap is always there when rgb lightmap is there
    if (s->lightmap) {
      flag |= 1u;
      vassert(s->lmsize > 0);
      if (s->lightmap_rgb) {
        vassert(s->lmrgbsize > 0);
        flag |= 2u;
      }
      *strm << flag;
      // surface check data
      // plane
      *strm << s->plane.normal.x << s->plane.normal.y << s->plane.normal.z << s->plane.dist;
      // subsector
      vuint32 ssnum = (s->subsector ? (vuint32)(ptrdiff_t)(s->subsector-&Level->Subsectors[0]) : 0xffffffffu);
      *strm << ssnum;
      // seg
      vuint32 segnum = (s->seg ? (vuint32)(ptrdiff_t)(s->seg-&Level->Segs[0]) : 0xffffffffu);
      *strm << segnum;
      // vertices
      vuint32 vcount = (vuint32)s->count;
      *strm << vcount;
      //??? write vertices too?
      vint32 t;
      t = s->texturemins[0]; *strm << t;
      t = s->texturemins[1]; *strm << t;
      t = s->extents[0]; *strm << t;
      t = s->extents[1]; *strm << t;
      // write lightmaps
      vuint32 lmsize;
      lmsize = (vuint32)s->lmsize;
      *strm << lmsize;
      strm->Serialise(s->lightmap, s->lmsize);
      if (s->lightmap_rgb) {
        lmsize = (vuint32)s->lmrgbsize;
        *strm << lmsize;
        strm->Serialise(s->lightmap_rgb, s->lmrgbsize);
      }
    } else {
      vassert(!s->lightmap_rgb);
      *strm << flag;
    }
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
  vuint32 surfCount = countAllSurfaces();

  vuint32 ver = 0;
  *strm << ver;

  *strm << surfCount;
  *strm << Level->NumSectors;
  *strm << Level->NumSubsectors;
  *strm << Level->NumSegs;

  /*
  vuint32 atlasW = BLOCK_WIDTH, atlasH = BLOCK_HEIGHT;
  *strm << atlasW << atlasH;
  */

  for (int i = 0; i < Level->NumSubsectors; ++i) {
    vuint32 ssnum = (vuint32)i;
    *strm << ssnum;
    subsector_t *sub = &Level->Subsectors[i];
    for (subregion_t *r = sub->regions; r != nullptr; r = r->next) {
      if (r->realfloor != nullptr) WriteSurfaceLightmaps(Level, strm, r->realfloor->surfs);
      if (r->realceil != nullptr) WriteSurfaceLightmaps(Level, strm, r->realceil->surfs);
      if (r->fakefloor != nullptr) WriteSurfaceLightmaps(Level, strm, r->fakefloor->surfs);
      if (r->fakeceil != nullptr) WriteSurfaceLightmaps(Level, strm, r->fakeceil->surfs);
    }
  }

  for (int i = 0; i < Level->NumSegs; ++i) {
    vuint32 snum = (vuint32)i;
    *strm << snum;
    seg_t *seg = &Level->Segs[i];
    for (drawseg_t *ds = seg->drawsegs; ds; ds = ds->next) {
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
//  LoadLightSurfaces
//
//==========================================================================
static bool LoadLightSurfaces (VRenderLevelLightmap *rdr, VLevel *Level, VStream *strm, surface_t *s) {
  vuint32 cnt = 0, rd = 0;
  for (surface_t *t = s; t; t = t->next) ++cnt;
  *strm << rd;
  if (rd != cnt) { GCon->Log(NAME_Warning, "invalid lightmap cache surface chain count"); return false; }
  // load lightmaps
  for (; s; s = s->next) {
    vuint8 flag = 0xffu;
    *strm << flag;
    if ((flag&~3u) != 0 || flag == 2u) { GCon->Log(NAME_Warning, "invalid lightmap cache surface flags"); return false; }
    if (flag) {
      // load check data
      // plane
      TPlane pl;
      *strm << pl.normal.x << pl.normal.y << pl.normal.z << pl.dist;
      if (pl.normal != s->plane.normal || pl.dist != s->plane.dist) { GCon->Log(NAME_Warning, "invalid lightmap cache surface plane"); return false; }
      // indicies
      vuint32 ut;
      // subsector
      ut = 0; *strm << ut;
      vuint32 ssnum = (s->subsector ? (vuint32)(ptrdiff_t)(s->subsector-&Level->Subsectors[0]) : 0xffffffffu);
      if (ssnum != ut) { GCon->Log(NAME_Warning, "invalid lightmap cache surface subsector index"); return false; }
      // seg
      ut = 0; *strm << ut;
      vuint32 segnum = (s->seg ? (vuint32)(ptrdiff_t)(s->seg-&Level->Segs[0]) : 0xffffffffu);
      if (segnum != ut) { GCon->Log(NAME_Warning, "invalid lightmap cache surface segment index"); return false; }
      // vertices
      ut = 0; *strm << ut;
      if ((vuint32)s->count != ut) { GCon->Logf(NAME_Warning, "invalid lightmap cache surface vertex count (%d instead of %d)", (int)ut, s->count); return false; }
      //??? check vertices too?
      vint32 ti32;
      *strm << ti32; if (ti32 != s->texturemins[0]) { GCon->Log(NAME_Warning, "invalid lightmap cache surface tmins[0]"); return false; }
      *strm << ti32; if (ti32 != s->texturemins[1]) { GCon->Log(NAME_Warning, "invalid lightmap cache surface tmins[1]"); return false; }
      *strm << ti32; if (ti32 != s->extents[0]) { GCon->Log(NAME_Warning, "invalid lightmap cache surface extents[0]"); return false; }
      *strm << ti32; if (ti32 != s->extents[1]) { GCon->Log(NAME_Warning, "invalid lightmap cache surface extents[1]"); return false; }
      // read lightmaps
      vuint32 lmsize = 0;
      *strm << lmsize;
      if (lmsize == 0 || lmsize > BLOCK_WIDTH*BLOCK_HEIGHT) { GCon->Log(NAME_Warning, "invalid lightmap cache surface lightmap size"); return false; }
      s->lightmap = (vuint8 *)Z_Malloc(lmsize);
      light_mem += lmsize;
      s->lmsize = lmsize;
      strm->Serialise(s->lightmap, s->lmsize);
      if (strm->IsError()) return false;
      if (flag&2u) {
        lmsize = 0;
        *strm << lmsize;
        if (lmsize == 0 || lmsize > BLOCK_WIDTH*BLOCK_HEIGHT*3) { GCon->Log(NAME_Warning, "invalid lightmap cache surface lightmap size"); return false; }
        s->lightmap_rgb = (rgb_t *)Z_Malloc(lmsize);
        light_mem += lmsize;
        s->lmrgbsize = lmsize;
        strm->Serialise(s->lightmap_rgb, s->lmrgbsize);
        if (strm->IsError()) return false;
      }
      //GCon->Log(NAME_Debug, "loaded surface lightmap");
    }
    s->drawflags &= ~surface_t::DF_CALC_LMAP;
  }
  // done
  return !strm->IsError();
}


//==========================================================================
//
//  LoadLightSegSurfaces
//
//==========================================================================
static bool LoadLightSegSurfaces (VRenderLevelLightmap *rdr, VLevel *Level, VStream *strm, segpart_t *sp) {
  vuint32 cnt = 0, rd = 0;
  for (segpart_t *s = sp; s; s = s->next) ++cnt;
  *strm << rd;
  if (rd != cnt) { GCon->Log(NAME_Warning, "invalid lightmap cache segment surface count"); return false; }
  for (; sp; sp = sp->next) if (!LoadLightSurfaces(rdr, Level, strm, sp->surfs)) return false;
  return true;
}


//==========================================================================
//
//  VRenderLevelLightmap::loadLightmapsInternal
//
//==========================================================================
bool VRenderLevelLightmap::loadLightmapsInternal (VStream *strm) {
  if (!strm || strm->IsError()) return false;

  vuint32 surfCount = countAllSurfaces();

  // load and check header
  vuint32 ver = 0xffffffffu, seccount = 0, sscount = 0, sgcount = 0, sfcount = 0;
  *strm << ver << sfcount << seccount << sscount << sgcount;
  if (ver != 0 || strm->IsError()) { GCon->Logf(NAME_Warning, "invalid lightmap cache version (%u)", ver); return false; }
  if (sfcount != surfCount || strm->IsError()) { GCon->Log(NAME_Warning, "invalid lightmap cache surface count"); return false; }
  if ((int)seccount != Level->NumSectors || strm->IsError()) { GCon->Log(NAME_Warning, "invalid lightmap cache sector count"); return false; }
  if ((int)sscount != Level->NumSubsectors || strm->IsError()) { GCon->Log(NAME_Warning, "invalid lightmap cache subsector count"); return false; }
  if ((int)sgcount != Level->NumSegs || strm->IsError()) { GCon->Log(NAME_Warning, "invalid lightmap cache seg count"); return false; }
  GCon->Log(NAME_Debug, "trying to use lightmap cache...");

  for (int i = 0; i < Level->NumSubsectors; ++i) {
    vuint32 snum = 0xffffffffu;
    *strm << snum;
    if ((int)snum != i) { GCon->Log(NAME_Warning, "invalid lightmap cache subsector number"); return false; }
    subsector_t *sub = &Level->Subsectors[i];
    for (subregion_t *r = sub->regions; r != nullptr; r = r->next) {
      if (r->realfloor != nullptr) if (!LoadLightSurfaces(this, Level, strm, r->realfloor->surfs)) return false;
      if (r->realceil != nullptr) if (!LoadLightSurfaces(this, Level, strm, r->realceil->surfs)) return false;
      if (r->fakefloor != nullptr) if (!LoadLightSurfaces(this, Level, strm, r->fakefloor->surfs)) return false;
      if (r->fakeceil != nullptr) if (!LoadLightSurfaces(this, Level, strm, r->fakeceil->surfs)) return false;
    }
  }

  for (int i = 0; i < Level->NumSegs; ++i) {
    vuint32 snum = 0xffffffffu;
    *strm << snum;
    if ((int)snum != i) { GCon->Log(NAME_Warning, "invalid lightmap cache seg number"); return false; }
    seg_t *seg = &Level->Segs[i];
    for (drawseg_t *ds = seg->drawsegs; ds; ds = ds->next) {
      if (!LoadLightSegSurfaces(this, Level, strm, ds->top)) return false;
      if (!LoadLightSegSurfaces(this, Level, strm, ds->mid)) return false;
      if (!LoadLightSegSurfaces(this, Level, strm, ds->bot)) return false;
      if (!LoadLightSegSurfaces(this, Level, strm, ds->topsky)) return false;
      if (!LoadLightSegSurfaces(this, Level, strm, ds->extra)) return false;
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
  if (!strm || strm->IsError()) return false;
  VZipStreamReader *zipstrm = new VZipStreamReader(strm, VZipStreamReader::UNKNOWN_SIZE, VZipStreamReader::UNKNOWN_SIZE/*Map->DecompressedSize*/);
  bool ok = loadLightmapsInternal(zipstrm);
  zipstrm->Close();
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
static int LightSurfaces (VRenderLevelLightmap *rdr, surface_t *s, bool recalcNow) {
  int res = 0;
  if (recalcNow) {
    for (; s; s = s->next) {
      s->drawflags &= ~surface_t::DF_CALC_LMAP;
      if (s->count >= 3) rdr->LightFace(s, s->subsector);
      ++res;
    }
  } else {
    for (; s; s = s->next) {
      ++res;
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
static int LightSegSurfaces (VRenderLevelLightmap *rdr, segpart_t *sp, bool recalcNow) {
  int res = 0;
  for (; sp; sp = sp->next) res += LightSurfaces(rdr, sp->surfs, recalcNow);
  return res;
}


//==========================================================================
//
//  VRenderLevelLightmap::ResetLightmaps
//
//==========================================================================
void VRenderLevelLightmap::ResetLightmaps (bool recalcNow) {
  vuint32 surfCount = 0;

  if (recalcNow) {
    surfCount = countAllSurfaces();
    R_PBarReset();
    R_PBarUpdate("Lightmaps", 0, (int)surfCount);
  }

  int processed = 0;
  for (auto &&sub : Level->allSubsectors()) {
    for (subregion_t *r = sub.regions; r != nullptr; r = r->next) {
      if (r->realfloor != nullptr) processed += LightSurfaces(this, r->realfloor->surfs, recalcNow);
      if (r->realceil != nullptr) processed += LightSurfaces(this, r->realceil->surfs, recalcNow);
      if (r->fakefloor != nullptr) processed += LightSurfaces(this, r->fakefloor->surfs, recalcNow);
      if (r->fakeceil != nullptr) processed += LightSurfaces(this, r->fakeceil->surfs, recalcNow);
    }
    if (recalcNow) R_PBarUpdate("Lightmaps", processed, (int)surfCount);
  }

  for (auto &&seg : Level->allSegs()) {
    for (drawseg_t *ds = seg.drawsegs; ds; ds = ds->next) {
      processed += LightSegSurfaces(this, ds->top, recalcNow);
      processed += LightSegSurfaces(this, ds->mid, recalcNow);
      processed += LightSegSurfaces(this, ds->bot, recalcNow);
      processed += LightSegSurfaces(this, ds->topsky, recalcNow);
      processed += LightSegSurfaces(this, ds->extra, recalcNow);
    }
    if (recalcNow) R_PBarUpdate("Lightmaps", processed, (int)surfCount);
  }

  if (recalcNow) R_PBarUpdate("Lightmaps", (int)surfCount, (int)surfCount, true);
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

    if (recalcLight && doPrecalc) {
      GCon->Log("calculating static lighting...");
      double stt = -Sys_Time();
      ResetLightmaps(true);
      // cache
      if (doWriteCache) {
        const float tlim = loader_cache_time_limit_lightmap.asFloat();
        stt += Sys_Time();
        if (dbg_always_cache_lightmaps || stt >= tlim) {
          VStream *lmc = FL_OpenSysFileWrite(ccfname);
          if (lmc) {
            GCon->Logf("writing lightmap cache to '%s'...", *ccfname);
            saveLightmaps(lmc);
            lmc->Close();
            bool err = lmc->IsError();
            delete lmc;
            if (err) Sys_FileDelete(ccfname);
          }
        }
      }
    }
  }

  GCon->Logf("%d subdivides", c_subdivides);
  GCon->Logf("%d seg subdivides", c_seg_div);
  GCon->Logf("%dk light mem", light_mem/1024);
}
