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
extern VCvarF loader_cache_time_limit;
extern VCvarI loader_cache_max_age_days;
extern VCvarI loader_cache_compression_level;

static VCvarB dbg_always_cache_lightmaps("dbg_always_cache_lightmaps", true, "Always cache lightmaps?", /*CVAR_Archive*/0);


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


// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  CountSurfaces
//
//==========================================================================
static int CountSurfaces (surface_t *s) {
  int res = 0;
  for (; s; s = s->next) ++res;
  return res;
}


//==========================================================================
//
//  CountSegSurfaces
//
//==========================================================================
static int CountSegSurfaces (segpart_t *sp) {
  int res = 0;
  for (; sp; sp = sp->next) res += CountSurfaces(sp->surfs);
  return res;
}


//==========================================================================
//
//  LightSurfaces
//
//==========================================================================
static int LightSurfaces (VRenderLevelLightmap *rdr, surface_t *s) {
  int res = 0;
  for (; s; s = s->next) {
    if (s->drawflags&surface_t::DF_CALC_LMAP) {
      s->drawflags &= ~surface_t::DF_CALC_LMAP;
      rdr->LightFace(s, s->subsector);
    }
    ++res;
  }
  return res;
}


//==========================================================================
//
//  LightSegSurfaces
//
//==========================================================================
static int LightSegSurfaces (VRenderLevelLightmap *rdr, segpart_t *sp) {
  int res = 0;
  for (; sp; sp = sp->next) res += LightSurfaces(rdr, sp->surfs);
  return res;
}


//==========================================================================
//
//  WriteSurfaceLightmaps
//
//==========================================================================
static void WriteSurfaceLightmaps (VLevel *Level, VStream *strm, surface_t *s, subsector_t *sub, seg_t *seg) {
  vuint32 cnt = 0;
  for (surface_t *t = s; t; t = t->next) ++cnt;
  *strm << cnt;
  for (; s; s = s->next) {
    vuint8 flag = 0;
    // monochrome lightmap is always there when rgb lightmap is there
    if (s->lightmap) {
      flag |= 1u<<0;
      vassert(s->lmsize > 0);
      if (s->lightmap_rgb) {
        vassert(s->lmrgbsize > 0);
        flag |= 1u<<1;
      }
      *strm << flag;
      // surface check data
      *strm << s->plane.normal.x << s->plane.normal.y << s->plane.normal.z << s->plane.dist;
      vuint32 ssnum = (s->subsector ? (vuint32)(ptrdiff_t)(s->subsector-&Level->Subsectors[0]) : 0xffffffffu);
      *strm << ssnum;
      vuint32 segnum = (s->seg ? (vuint32)(ptrdiff_t)(s->seg-&Level->Segs[0]) : 0xffffffffu);
      *strm << segnum;
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
static void WriteSegLightmaps (VLevel *Level, VStream *strm, segpart_t *sp, seg_t *seg) {
  vuint32 cnt = 0;
  for (segpart_t *t = sp; t; t = t->next) ++cnt;
  *strm << cnt;
  for (; sp; sp = sp->next) WriteSurfaceLightmaps(Level, strm, sp->surfs, nullptr, seg);
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
  bool doCache = (!Level->cacheFileBase.isEmpty() && loader_cache_data.asBool() && (Level->cacheFlags&VLevel::CacheFlag_Ignore) == 0);
  float tlim = loader_cache_time_limit.asFloat();
  double stt = -Sys_Time();

  bool doPrecalc = (r_precalc_static_lights_override >= 0 ? !!r_precalc_static_lights_override : r_precalc_static_lights);
  VStr ccfname = (Level->cacheFileBase.isEmpty() ? VStr::EmptyString : Level->cacheFileBase+".lmap");
  if (ccfname.isEmpty()) doCache = false;

  if (doCache) {
    GCon->Logf(NAME_Debug, "lightmap cache file: '%s.lmap'", *ccfname);
  }

  if (doPrecalc) {
    R_LdrMsgShowSecondary("CREATING LIGHTMAPS...");
    R_PBarReset();

    // calculate total number of surfaces to process
    int surfCount = 0;
    for (int i = 0; i < Level->NumSubsectors; ++i) {
      subsector_t *sub = &Level->Subsectors[i];
      for (subregion_t *r = sub->regions; r != nullptr; r = r->next) {
        if (r->realfloor != nullptr) surfCount += CountSurfaces(r->realfloor->surfs);
        if (r->realceil != nullptr) surfCount += CountSurfaces(r->realceil->surfs);
        if (r->fakefloor != nullptr) surfCount += CountSurfaces(r->fakefloor->surfs);
        if (r->fakeceil != nullptr) surfCount += CountSurfaces(r->fakeceil->surfs);
      }
    }

    for (int i = 0; i < Level->NumSegs; ++i) {
      seg_t *seg = &Level->Segs[i];
      for (drawseg_t *ds = seg->drawsegs; ds; ds = ds->next) {
        surfCount += CountSegSurfaces(ds->top);
        surfCount += CountSegSurfaces(ds->mid);
        surfCount += CountSegSurfaces(ds->bot);
        surfCount += CountSegSurfaces(ds->topsky);
        surfCount += CountSegSurfaces(ds->extra);
      }
    }

    R_PBarUpdate("Lightmaps", 0, surfCount);
    int surfProcessed = 0;

    for (int i = 0; i < Level->NumSubsectors; ++i) {
      subsector_t *sub = &Level->Subsectors[i];
      for (subregion_t *r = sub->regions; r != nullptr; r = r->next) {
        if (r->realfloor != nullptr) surfProcessed += LightSurfaces(this, r->realfloor->surfs);
        if (r->realceil != nullptr) surfProcessed += LightSurfaces(this, r->realceil->surfs);
        if (r->fakefloor != nullptr) surfProcessed += LightSurfaces(this, r->fakefloor->surfs);
        if (r->fakeceil != nullptr) surfProcessed += LightSurfaces(this, r->fakeceil->surfs);
      }
      R_PBarUpdate("Lightmaps", surfProcessed, surfCount);
    }

    for (int i = 0; i < Level->NumSegs; ++i) {
      seg_t *seg = &Level->Segs[i];
      for (drawseg_t *ds = seg->drawsegs; ds; ds = ds->next) {
        surfProcessed += LightSegSurfaces(this, ds->top);
        surfProcessed += LightSegSurfaces(this, ds->mid);
        surfProcessed += LightSegSurfaces(this, ds->bot);
        surfProcessed += LightSegSurfaces(this, ds->topsky);
        surfProcessed += LightSegSurfaces(this, ds->extra);
      }
      R_PBarUpdate("Lightmaps", surfProcessed, surfCount);
    }

    R_PBarUpdate("Lightmaps", surfCount, surfCount, true);

    // cache
    if (doCache) {
      stt += Sys_Time();
      if (dbg_always_cache_lightmaps || stt >= tlim) {
        VStream *lmc = FL_OpenSysFileWrite(ccfname);
        if (lmc) {
          GCon->Logf("writing lightmap cache to '%s.lmap'...", *ccfname);
          vuint32 ver = 0;
          *lmc << ver;

          *lmc << Level->NumSectors;
          *lmc << Level->NumSubsectors;
          *lmc << Level->NumSegs;

          for (int i = 0; i < Level->NumSubsectors; ++i) {
            vuint32 ssnum = (vuint32)i;
            *lmc << ssnum;
            subsector_t *sub = &Level->Subsectors[i];
            for (subregion_t *r = sub->regions; r != nullptr; r = r->next) {
              if (r->realfloor != nullptr) WriteSurfaceLightmaps(Level, lmc, r->realfloor->surfs, sub, nullptr);
              if (r->realceil != nullptr) WriteSurfaceLightmaps(Level, lmc, r->realceil->surfs, sub, nullptr);
              if (r->fakefloor != nullptr) WriteSurfaceLightmaps(Level, lmc, r->fakefloor->surfs, sub, nullptr);
              if (r->fakeceil != nullptr) WriteSurfaceLightmaps(Level, lmc, r->fakeceil->surfs, sub, nullptr);
            }
          }

          for (int i = 0; i < Level->NumSegs; ++i) {
            vuint32 snum = (vuint32)i;
            *lmc << snum;
            seg_t *seg = &Level->Segs[i];
            for (drawseg_t *ds = seg->drawsegs; ds; ds = ds->next) {
              WriteSegLightmaps(Level, lmc, ds->top, seg);
              WriteSegLightmaps(Level, lmc, ds->mid, seg);
              WriteSegLightmaps(Level, lmc, ds->bot, seg);
              WriteSegLightmaps(Level, lmc, ds->topsky, seg);
              WriteSegLightmaps(Level, lmc, ds->extra, seg);
            }
          }

          lmc->Close();
          bool err = lmc->IsError();
          delete lmc;
          if (err) Sys_FileDelete(ccfname);
        }
      }
    }
  }

  GCon->Logf("%d subdivides", c_subdivides);
  GCon->Logf("%d seg subdivides", c_seg_div);
  GCon->Logf("%dk light mem", light_mem/1024);
}
/*
extern VCvarB loader_cache_data;
extern VCvarF loader_cache_time_limit;
extern VCvarI loader_cache_max_age_days;
extern VCvarI loader_cache_compression_level;
*/


/* surface caching
  inline vuint32 calcHash (const VLevel *level) const noexcept {
    vuint32 hash = 0;
    hash = joaatHashBuf(&plane.normal.x, sizeof(plane.normal.x), hash);
    hash = joaatHashBuf(&plane.normal.y, sizeof(plane.normal.y), hash);
    hash = joaatHashBuf(&plane.normal.z, sizeof(plane.normal.z), hash);
    hash = joaatHashBuf(&plane.dist, sizeof(plane.dist), hash);
    vuint32 ssnum = (subsector ? (vuint32)(ptrdiff_t)(subsector-&level->Subsectors[0]) : 0xffffffffu);
    hash = joaatHashBuf(&ssnum, sizeof(ssnum), hash);
    vuint32 segnum = (seg ? (vuint32)(ptrdiff_t)(seg-&level->Segs[0]) : 0xffffffffu);
    hash = joaatHashBuf(&segnum, sizeof(segnum), hash);
    hash = joaatHashBuf(&count, sizeof(count), hash);
    hash = joaatHashBuf(&texturemins, sizeof(texturemins), hash);
    hash = joaatHashBuf(&extents, sizeof(extents), hash);
    return hash;
  }

  inline bool operator == (const surface_t &b) const noexcept {
    if (&b == this) return true;
    return
      count == b.count &&
      texturemins[0] == b.texturemins[0] &&
      texturemins[1] == b.texturemins[1] &&
      extents[0] == b.extents[0] &&
      extents[1] == b.extents[1] &&
      plane.dist == b.plane.dist &&
      plane.normal == b.plane.normal;
  }
*/
