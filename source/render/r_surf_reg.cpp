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
#include "gamedefs.h"
#include "r_local.h"

#define USE_FASTER_SUBDIVIDER

#define ON_EPSILON      (0.1f)
#define subdivide_size  (240)

#define MAXWVERTS  (8)
#define WSURFSIZE  (sizeof(surface_t)+sizeof(TVec)*(MAXWVERTS-1))

//  This is used to compare floats like ints which is faster
#define FASI(var) (*(int *)&var)


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB r_precalc_static_lights("r_precalc_static_lights", true, "Precalculate static lights?", CVAR_Archive);
int r_precalc_static_lights_override = -1; // <0: not set


// ////////////////////////////////////////////////////////////////////////// //
extern int light_mem;

extern subsector_t *r_surf_sub;


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
//  VRenderLevel::InitSurfs
//
//==========================================================================
void VRenderLevel::InitSurfs (surface_t *ASurfs, texinfo_t *texinfo, TPlane *plane, subsector_t *sub) {
  guard(VRenderLevel::InitSurfs);
  surface_t *surfs = ASurfs;
  //float dot, mins, maxs;
  //int bmins, bmaxs;

  bool doPrecalc = (r_precalc_static_lights_override >= 0 ? !!r_precalc_static_lights_override : r_precalc_static_lights);

  while (surfs) {
    if (plane) {
      surfs->texinfo = texinfo;
      surfs->plane = plane;
    }

    float mins = 99999.0f;
    float maxs = -99999.0f;
    for (int i = 0; i < surfs->count; ++i) {
      float dot = DotProduct(surfs->verts[i], texinfo->saxis)+texinfo->soffs;
      if (dot < mins) mins = dot;
      if (dot > maxs) maxs = dot;
    }

    int bmins = (int)floor(mins/16);
    int bmaxs = (int)ceil(maxs/16);
    surfs->texturemins[0] = bmins*16;
    surfs->extents[0] = (bmaxs-bmins)*16;
    if (surfs->extents[0] > 256) {
      //Sys_Error(va("Bad extents (0): %d", (int)surfs->extents[0]));
      GCon->Logf("Bad extents (0): %d", (int)surfs->extents[0]);
      surfs->extents[0] = 256;
    }

    mins = 99999.0f;
    maxs = -99999.0f;
    for (int i = 0; i < surfs->count; ++i) {
      float dot = DotProduct(surfs->verts[i], texinfo->taxis)+texinfo->toffs;
      if (dot < mins) mins = dot;
      if (dot > maxs) maxs = dot;
    }
    bmins = (int)floor(mins/16);
    bmaxs = (int)ceil(maxs/16);
    surfs->texturemins[1] = bmins*16;
    surfs->extents[1] = (bmaxs-bmins)*16;
    if (surfs->extents[1] > 256) {
      //Sys_Error(va("Bad extents (1): %d", (int)surfs->extents[1]));
      GCon->Logf("Bad extents (1): %d", (int)surfs->extents[1]);
      surfs->extents[1] = 256;
    }

    //GCon->Logf("***INITSURF***");
    surfs->subsector = sub;
    if (!doPrecalc && showCreateWorldSurfProgress && !surfs->lightmap) {
      surfs->lmapflags |= Lightmap_Required;
      //GCon->Logf("delayed static lightmap for %p (subsector %p)", surfs, sub);
      //LightFace(surfs, sub);
    } else {
      surfs->lmapflags &= ~Lightmap_Required; // just in case
      LightFace(surfs, sub);
    }
    //LightFace(surfs, sub);

    surfs = surfs->next;
  }
  unguard;
}


static __attribute__((unused)) inline void intersectAgainstPlane (TVec &res, const TPlane &plane, const TVec &a, const TVec &b) {
  //const float t = (plane.dist-(plane.normal*a))/(plane.normal*(b-a));
  const float t = (plane.dist-DotProduct(plane.normal, a))/DotProduct(plane.normal, b-a);
  res.x = a.x+(b.x-a.x)*t;
  res.y = a.y+(b.y-a.y)*t;
  res.z = a.z+(b.z-a.z)*t;
}


//==========================================================================
//
//  VRenderLevel::SubdivideFace
//
//==========================================================================
surface_t *VRenderLevel::SubdivideFace (surface_t *InF, const TVec &axis, const TVec *nextaxis) {
  guard(VRenderLevel::SubdivideFace);
  surface_t *f = InF;
  float mins = 99999.0f;
  float maxs = -99999.0f;

  for (int i = 0; i < f->count; ++i) {
    if (!isFiniteF(f->verts[i].x) || !isFiniteF(f->verts[i].y) || !isFiniteF(f->verts[i].z)) {
      GCon->Logf("ERROR: invalid surface vertex %d (%f,%f,%f); axis=(%f,%f,%f); THIS IS INTERNAL VAVOOM BUG!",
        i, f->verts[i].x, f->verts[i].y, f->verts[i].z, axis.x, axis.y, axis.z);
      if (!isFiniteF(f->verts[i].x)) f->verts[i].x = 0;
      if (!isFiniteF(f->verts[i].y)) f->verts[i].y = 0;
      if (!isFiniteF(f->verts[i].z)) f->verts[i].z = 0;
    }
    const float dot = DotProduct(f->verts[i], axis);
    if (dot < mins) mins = dot;
    if (dot > maxs) maxs = dot;
  }

  if (maxs-mins <= subdivide_size) {
    if (nextaxis) return SubdivideFace(f, *nextaxis, nullptr);
    return f;
  }

  ++c_subdivides;

  TPlane plane;

  plane.normal = axis;
  const float dot0 = Length(plane.normal);
  plane.normal = Normalise(plane.normal);
  plane.dist = (mins+subdivide_size-16)/dot0;

  enum {
    PlaneBack = -1,
    PlaneCoplanar = 0,
    PlaneFront = 1,
  };

  spvReserve(f->count*2+2); //k8: `f->count+1` is enough, but...

  float *dots = spvPoolDots;
  int *sides = spvPoolSides;
  TVec *verts1 = spvPoolV1;
  TVec *verts2 = spvPoolV2;

  for (int i = 0; i < f->count; ++i) {
    const float dot = DotProduct(f->verts[i], plane.normal)-plane.dist;
    dots[i] = dot;
         if (dot < -ON_EPSILON) sides[i] = PlaneBack;
    else if (dot > ON_EPSILON) sides[i] = PlaneFront;
    else sides[i] = PlaneCoplanar;
  }
  dots[f->count] = dots[0];
  sides[f->count] = sides[0];

  int count1 = 0;
  int count2 = 0;
  TVec mid(0, 0, 0);

#if defined(USE_FASTER_SUBDIVIDER)
  for (int i = 0; i < f->count; ++i) {
    if (sides[i] == PlaneCoplanar) {
      verts1[count1++] = f->verts[i];
      verts2[count2++] = f->verts[i];
      continue;
    }
    if (sides[i] == PlaneFront) {
      verts1[count1++] = f->verts[i];
    } else {
      verts2[count2++] = f->verts[i];
    }
    if (sides[i+1] == PlaneCoplanar || sides[i] == sides[i+1]) continue;

    // generate a split point
    TVec &p1 = f->verts[i];
    TVec &p2 = f->verts[(i+1)%f->count];

    const float dot = dots[i]/(dots[i]-dots[i+1]);
    for (int j = 0; j < 3; ++j) {
      // avoid round off error when possible
           if (plane.normal[j] == 1) mid[j] = plane.dist;
      else if (plane.normal[j] == -1) mid[j] = -plane.dist;
      else mid[j] = p1[j]+dot*(p2[j]-p1[j]);
      //if (!isFiniteF(mid[j])) GCon->Logf("FUCKED mid #%d (%f)! p1=%f; p2=%f; dot=%f", j, mid[j], p1[j], p2[j], dot);
    }

    verts1[count1++] = mid;
    verts2[count2++] = mid;
  }
#else
  // robust spliting, taken from "Real-Time Collision Detection" book
  for (int i = 0; i < f->count; ++i) {
    const auto atype = sides[i];
    const auto btype = sides[i+1];
    const TVec &va = f->verts[i];
    const TVec &vb = f->verts[(i+1)%f->count];
    if (btype == PlaneFront) {
      if (atype == PlaneBack) {
        // edge (a, b) straddles, output intersection point to both sides
        intersectAgainstPlane(mid, plane, vb, va); // `(b, a)` for robustness; was (a, b)
        // consistently clip edge as ordered going from in front -> behind
        //assert(plane.pointSide(mid.pos) == Plane.Coplanar);
        verts1[count1++] = mid;
        verts2[count2++] = mid;
      }
      // in all three cases, output b to the front side
      verts1[count1++] = vb;
    } else if (btype == PlaneBack) {
      if (atype == PlaneFront) {
        // edge (a, b) straddles plane, output intersection point
        intersectAgainstPlane(mid, plane, va, vb); // `(b, a)` for robustness; was (a, b)
        //assert(plane.pointSide(mid.pos) == Plane.Coplanar);
        verts1[count1++] = mid;
        verts2[count2++] = mid;
      } else if (atype == PlaneCoplanar) {
        // output a when edge (a, b) goes from 'on' to 'behind' plane
        verts2[count2++] = va;
      }
      // in all three cases, output b to the back side
      verts2[count2++] = vb;
    } else {
      // b is on the plane. In all three cases output b to the front side
      verts1[count1++] = vb;
      // in one case, also output b to back side
      if (atype == PlaneBack) {
        verts2[count2++] = vb;
      }
    }
  }
#endif

  /*
  fprintf(stderr, "f->count=%d; count1=%d; count2=%d; axis=(%f,%f,%f)\n", f->count, count1, count2, axis.x, axis.y, axis.z);
  fprintf(stderr, "=== F ===\n"); for (int ff = 0; ff < f->count; ++ff) fprintf(stderr, "  %d: (%f,%f,%f)\n", ff, f->verts[ff].x, f->verts[ff].y, f->verts[ff].z);
  fprintf(stderr, "=== 1 ===\n"); for (int ff = 0; ff < count1; ++ff) fprintf(stderr, "  %d: (%f,%f,%f)\n", ff, verts1[ff].x, verts1[ff].y, verts1[ff].z);
  fprintf(stderr, "=== 2 ===\n"); for (int ff = 0; ff < count2; ++ff) fprintf(stderr, "  %d: (%f,%f,%f)\n", ff, verts2[ff].x, verts2[ff].y, verts2[ff].z);
  */

  surface_t *next = f->next;
  Z_Free(f);

  surface_t *back = (surface_t *)Z_Calloc(sizeof(surface_t)+(count2-1)*sizeof(TVec));
  back->count = count2;
  memcpy(back->verts, verts2, count2*sizeof(TVec));

  surface_t *front = (surface_t *)Z_Calloc(sizeof(surface_t)+(count1-1)*sizeof(TVec));
  front->count = count1;
  memcpy(front->verts, verts1, count1*sizeof(TVec));

  front->next = next;
  back->next = SubdivideFace(front, axis, nextaxis);
  if (nextaxis) back = SubdivideFace(back, *nextaxis, nullptr);
  return back;
  unguard;
}


//==========================================================================
//
//  VRenderLevel::SubdivideSeg
//
//==========================================================================
surface_t *VRenderLevel::SubdivideSeg (surface_t *InSurf, const TVec &axis, const TVec *nextaxis) {
  guard(VRenderLevel::SubdivideSeg);
  surface_t *surf = InSurf;
  float mins = 99999.0f;
  float maxs = -99999.0f;

  for (int i = 0; i < surf->count; ++i) {
    if (!isFiniteF(surf->verts[i].x) || !isFiniteF(surf->verts[i].y) || !isFiniteF(surf->verts[i].z)) {
      GCon->Logf("ERROR: invalid surface vertex %d (%f,%f,%f); axis=(%f,%f,%f); THIS IS INTERNAL VAVOOM BUG!",
        i, surf->verts[i].x, surf->verts[i].y, surf->verts[i].z, axis.x, axis.y, axis.z);
      if (!isFiniteF(surf->verts[i].x)) surf->verts[i].x = 0;
      if (!isFiniteF(surf->verts[i].y)) surf->verts[i].y = 0;
      if (!isFiniteF(surf->verts[i].z)) surf->verts[i].z = 0;
    }
    const float dot = DotProduct(surf->verts[i], axis);
    if (dot < mins) mins = dot;
    if (dot > maxs) maxs = dot;
  }

  if (maxs-mins <= subdivide_size) {
    if (nextaxis) surf = SubdivideSeg(surf, *nextaxis, nullptr);
    return surf;
  }

  ++c_seg_div;

  TPlane plane;

  plane.normal = axis;
  const float dot0 = Length(plane.normal);
  plane.normal = Normalise(plane.normal);
  plane.dist = (mins+subdivide_size-16)/dot0;

  enum {
    PlaneBack = -1,
    PlaneCoplanar = 0,
    PlaneFront = 1,
  };

  float dots[MAXWVERTS+1];
  int sides[MAXWVERTS+1];

  for (int i = 0; i < surf->count; ++i) {
    const float dot = DotProduct(surf->verts[i], plane.normal)-plane.dist;
    dots[i] = dot;
         if (dot < -ON_EPSILON) sides[i] = PlaneBack;
    else if (dot > ON_EPSILON) sides[i] = PlaneFront;
    else sides[i] = PlaneCoplanar;
  }
  dots[surf->count] = dots[0];
  sides[surf->count] = sides[0];

  TVec verts1[MAXWVERTS];
  TVec verts2[MAXWVERTS];
  int count1 = 0;
  int count2 = 0;
  TVec mid(0, 0, 0);

#if defined(USE_FASTER_SUBDIVIDER)
  for (int i = 0; i < surf->count; ++i) {
    if (sides[i] == 0) {
      verts1[count1++] = surf->verts[i];
      verts2[count2++] = surf->verts[i];
      continue;
    }
    if (sides[i] == 1) {
      verts1[count1++] = surf->verts[i];
    } else {
      verts2[count2++] = surf->verts[i];
    }
    if (sides[i+1] == 0 || sides[i] == sides[i+1]) continue;

    // generate a split point
    TVec &p1 = surf->verts[i];
    TVec &p2 = surf->verts[(i+1)%surf->count];

    const float dot = dots[i]/(dots[i]-dots[i+1]);
    for (int j = 0; j < 3; ++j) {
      // avoid round off error when possible
           if (plane.normal[j] == 1) mid[j] = plane.dist;
      else if (plane.normal[j] == -1) mid[j] = -plane.dist;
      else mid[j] = p1[j]+dot*(p2[j]-p1[j]);
    }

    verts1[count1++] = mid;
    verts2[count2++] = mid;
  }
#else
  // robust spliting, taken from "Real-Time Collision Detection" book
  for (int i = 0; i < surf->count; ++i) {
    const auto atype = sides[i];
    const auto btype = sides[i+1];
    const TVec &va = surf->verts[i];
    const TVec &vb = surf->verts[(i+1)%surf->count];
    if (btype == PlaneFront) {
      if (atype == PlaneBack) {
        // edge (a, b) straddles, output intersection point to both sides
        intersectAgainstPlane(mid, plane, vb, va); // `(b, a)` for robustness; was (a, b)
        // consistently clip edge as ordered going from in front -> behind
        //assert(plane.pointSide(mid.pos) == Plane.Coplanar);
        //f.unsafeArrayAppend(mid);
        //b.unsafeArrayAppend(mid);
        verts1[count1++] = mid;
        verts2[count2++] = mid;
      }
      // in all three cases, output b to the front side
      //f.unsafeArrayAppend(vb);
      verts1[count1++] = vb;
    } else if (btype == PlaneBack) {
      if (atype == PlaneFront) {
        // edge (a, b) straddles plane, output intersection point
        intersectAgainstPlane(mid, plane, va, vb); // `(b, a)` for robustness; was (a, b)
        //assert(plane.pointSide(mid.pos) == Plane.Coplanar);
        //f.unsafeArrayAppend(mid);
        //b.unsafeArrayAppend(mid);
        verts1[count1++] = mid;
        verts2[count2++] = mid;
      } else if (atype == PlaneCoplanar) {
        // output a when edge (a, b) goes from 'on' to 'behind' plane
        //b.unsafeArrayAppend(va);
        verts2[count2++] = va;
      }
      // in all three cases, output b to the back side
      //b.unsafeArrayAppend(vb);
      verts2[count2++] = vb;
    } else {
      // b is on the plane. In all three cases output b to the front side
      //f.unsafeArrayAppend(vb);
      verts1[count1++] = vb;
      // in one case, also output b to back side
      if (atype == PlaneBack) {
        //b.unsafeArrayAppend(vb);
        verts2[count2++] = vb;
      }
    }
  }
#endif

  surf->count = count2;
  memcpy(surf->verts, verts2, count2*sizeof(TVec));

  surface_t *news = NewWSurf();
  news->count = count1;
  memcpy(news->verts, verts1, count1*sizeof(TVec));

  news->next = surf->next;
  surf->next = SubdivideSeg(news, axis, nextaxis);
  if (nextaxis)
  {
    surf = SubdivideSeg(surf, *nextaxis, nullptr);
  }
  return surf;
  unguard;
}


//==========================================================================
//
//  VRenderLevel::PreRender
//
//==========================================================================
void VRenderLevel::PreRender () {
  guard(VRenderLevel::PreRender);
  c_subdivides = 0;
  c_seg_div = 0;
  light_mem = 0;

  CreateWorldSurfaces();

  GCon->Logf(NAME_Dev, "%d subdivides", c_subdivides);
  GCon->Logf(NAME_Dev, "%d seg subdivides", c_seg_div);
  GCon->Logf(NAME_Dev, "%dk light mem", light_mem/1024);
  unguard;
}


//==========================================================================
//
//  VRenderLevel::UpdateSubsector
//
//==========================================================================
void VRenderLevel::UpdateSubsector (int num, float *bbox) {
  guard(VRenderLevel::UpdateSubsector);
  r_surf_sub = &Level->Subsectors[num];

  if (r_surf_sub->VisFrame != r_visframecount) return;

  if (!r_surf_sub->sector->linecount) return; // skip sectors containing original polyobjs

  if (!ViewClip.ClipCheckSubsector(r_surf_sub, false)) return;

  bbox[2] = r_surf_sub->sector->floor.minz;
  if (IsSky(&r_surf_sub->sector->ceiling)) {
    bbox[5] = skyheight;
  } else {
    bbox[5] = r_surf_sub->sector->ceiling.maxz;
  }

  UpdateSubRegion(r_surf_sub->regions, true);

  ViewClip.ClipAddSubsectorSegs(r_surf_sub, false);
  unguard;
}


//==========================================================================
//
//  VRenderLevel::UpdateBSPNode
//
//==========================================================================
void VRenderLevel::UpdateBSPNode (int bspnum, float *bbox) {
  guard(VRenderLevel::UpdateBSPNode);
  if (ViewClip.ClipIsFull()) return;

  if (!ViewClip.ClipIsBBoxVisible(bbox, false)) return;

  if (bspnum == -1) {
    UpdateSubsector(0, bbox);
    return;
  }

  // found a subsector?
  if (!(bspnum&NF_SUBSECTOR)) {
    node_t *bsp = &Level->Nodes[bspnum];

    if (bsp->VisFrame != r_visframecount) return;

    // decide which side the view point is on
    int side = bsp->PointOnSide(vieworg);

    UpdateBSPNode(bsp->children[side], bsp->bbox[side]);
    bbox[2] = MIN(bsp->bbox[0][2], bsp->bbox[1][2]);
    bbox[5] = MAX(bsp->bbox[0][5], bsp->bbox[1][5]);
    if (!ViewClip.ClipIsBBoxVisible(bsp->bbox[side^1], false)) return;
    UpdateBSPNode(bsp->children[side^1], bsp->bbox[side^1]);
    return;
  }

  UpdateSubsector(bspnum&(~NF_SUBSECTOR), bbox);
  unguard;
}


//==========================================================================
//
//  VRenderLevel::UpdateWorld
//
//==========================================================================
void VRenderLevel::UpdateWorld (const refdef_t *rd, const VViewClipper *Range) {
  guard(VRenderLevel::UpdateWorld);
  float dummy_bbox[6] = { -99999, -99999, -99999, 99999, 99999, 99999 };

  ViewClip.ClearClipNodes(vieworg, Level);
  ViewClip.ClipInitFrustrumRange(viewangles, viewforward, viewright, viewup, rd->fovx, rd->fovy);
  if (Range) ViewClip.ClipToRanges(*Range); // range contains a valid range, so we must clip away holes in it

  // update fake sectors
  for (int i = 0; i < Level->NumSectors; ++i) {
    sector_t *sec = &Level->Sectors[i];
         if (sec->deepref) UpdateDeepWater(sec);
    else if (sec->heightsec && !(sec->heightsec->SectorFlags&sector_t::SF_IgnoreHeightSec)) UpdateFakeFlats(sec);
    else if (sec->othersecFloor || sec->othersecCeiling) UpdateFloodBug(sec);
  }

  UpdateBSPNode(Level->NumNodes-1, dummy_bbox); // head node is the last node output
  unguard;
}
