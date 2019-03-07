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
//**  Rendering main loop and setup functions, utility functions (BSP,
//**  geometry, trigonometry). See tables.c, too.
//**
//**************************************************************************
#include "gamedefs.h"
#include "r_local.h"


extern VCvarF r_lights_radius;
extern VCvarF r_lights_radius_sight_check;
extern VCvarB r_dynamic_clip_more;
extern VCvarB w_update_in_renderer;
extern VCvarI r_max_lights;

static VCvarB r_advlight_sort_static("r_advlight_sort_static", true, "Sort visible static lights, so nearby lights will be rendered first?", CVAR_Archive|CVAR_PreInit);
static VCvarB r_advlight_sort_dynamic("r_advlight_sort_dynamic", true, "Sort visible dynamic lights, so nearby lights will be rendered first?", CVAR_Archive|CVAR_PreInit);


struct StLightInfo {
  VRenderLevelShared::light_t *stlight; // light
  float distSq; // distance
};

struct DynLightInfo {
  dlight_t *l; // light
  float distSq; // distance
};


extern "C" {
  static int stLightCompare (const void *aa, const void *bb, void *udata) {
    if (aa == bb) return 0;
    const StLightInfo *a = (const StLightInfo *)aa;
    const StLightInfo *b = (const StLightInfo *)bb;
    //TODO: consider radius too?
    if (a->distSq < b->distSq) return -1;
    if (a->distSq > b->distSq) return 1;
    return 0;
  }

  static int dynLightCompare (const void *aa, const void *bb, void *udata) {
    if (aa == bb) return 0;
    const DynLightInfo *a = (const DynLightInfo *)aa;
    const DynLightInfo *b = (const DynLightInfo *)bb;
    //TODO: consider radius too?
    if (a->distSq < b->distSq) return -1;
    if (a->distSq > b->distSq) return 1;
    return 0;
  }
}


//==========================================================================
//
//  VAdvancedRenderLevel::VAdvancedRenderLevel
//
//==========================================================================
VAdvancedRenderLevel::VAdvancedRenderLevel (VLevel *ALevel)
  : VRenderLevelShared(ALevel)
  , LightVis(nullptr)
{
  guard(VAdvancedRenderLevel::VAdvancedRenderLevel);
  NeedsInfiniteFarClip = true;
  mIsAdvancedRenderer = true;
  showCreateWorldSurfProgress = false; // just in case
  updateWorldCheckVisFrame = false; // we don't want it

  LightVis = new vuint8[VisSize];
  LightBspVis = new vuint8[VisSize];
  unguard;
}


//==========================================================================
//
//  VAdvancedRenderLevel::~VAdvancedRenderLevel
//
//==========================================================================
VAdvancedRenderLevel::~VAdvancedRenderLevel () {
  delete[] LightVis;
  LightVis = nullptr;
  delete[] LightBspVis;
  LightBspVis = nullptr;
}


//==========================================================================
//
//  VRenderLevelShared::NewBSPVisibilityFrame
//
//==========================================================================
void VRenderLevelShared::NewBSPVisibilityFrame () {
  if (bspVisRadius) {
    if (++bspVisRadiusFrame == 0) {
      bspVisRadiusFrame = 1;
      memset(bspVisRadius, 0, sizeof(bspVisRadius[0])*Level->NumSubsectors);
    }
  } else {
    bspVisRadiusFrame = 0;
  }
}


//==========================================================================
//
//  isCircleTouchingLine
//
//==========================================================================
static inline bool isCircleTouchingLine (const TVec &corg, const float radiusSq, const TVec &v0, const TVec &v1) {
  const TVec s0qp = corg-v0;
  if (s0qp.length2DSquared() <= radiusSq) return true;
  if ((corg-v1).length2DSquared() <= radiusSq) return true;
  const TVec s0s1 = v1-v0;
  const float a = s0s1.dot2D(s0s1);
  if (!a) return false; // if you haven't zero-length segments omit this, as it would save you 1 _mm_comineq_ss() instruction and 1 memory fetch
  const float b = s0s1.dot2D(s0qp);
  const float t = b/a; // length of projection of s0qp onto s0s1
  if (t >= 0.0f && t <= 1.0f) {
    const float c = s0qp.dot2D(s0qp);
    const float r2 = c-a*t*t;
    //print("a=%s; t=%s; r2=%s; rsq=%s", a, t, r2, radiusSq);
    return (r2 <= radiusSq); // true if collides
  }
  return false;
}


//==========================================================================
//
//  VRenderLevelShared::CheckBSPVisibilitySub
//
//==========================================================================
bool VRenderLevelShared::CheckBSPVisibilitySub (const TVec &org, float radiusSq, const subsector_t *currsub) {
  const unsigned csubidx = (unsigned)(ptrdiff_t)(currsub-Level->Subsectors);
  // rendered means "visible"
  if (BspVis[csubidx>>3]&(1<<(csubidx&7))) return true;
  // if we came into already visited subsector, abort flooding (and return failure)
  if (bspVisRadius[csubidx].framecount == bspVisRadiusFrame) return false;
  // recurse into neighbour subsectors
  bspVisRadius[csubidx].framecount = bspVisRadiusFrame; // mark as visited
  if (currsub->numlines == 0) return false;
  const seg_t *seg = &Level->Segs[currsub->firstline];
  for (int count = currsub->numlines; count--; ++seg) {
    // skip non-portals
    const line_t *ldef = seg->linedef;
    if (ldef) {
      // not a miniseg; check if linedef is passable
      if (!(ldef->flags&(ML_TWOSIDED|ML_3DMIDTEX))) continue; // solid line
    } // minisegs are portals
    // we should have partner seg
    if (!seg->partner || seg->partner == seg || seg->partner->front_sub == currsub) continue;
    // check if this seg is touching our sphere
    if (!isCircleTouchingLine(org, radiusSq, *seg->v1, *seg->v2)) continue;
    // ok, it is touching, recurse
    if (CheckBSPVisibilitySub(org, radiusSq, seg->partner->front_sub)) {
      //GCon->Logf("RECURSE HIT!");
      return true;
    }
  }
  return false;
}


//==========================================================================
//
//  VRenderLevelShared::CheckBSPVisibility
//
//==========================================================================
bool VRenderLevelShared::CheckBSPVisibility (const TVec &org, float radius, const subsector_t *sub) {
  if (!Level) return false; // just in case
  if (!sub) {
    sub = Level->PointInSubsector(org);
    if (!sub) return false;
  }
  const unsigned subidx = (unsigned)(ptrdiff_t)(sub-Level->Subsectors);
  // check potential visibility
  /*
  if (hasPVS) {
    const vuint8 *dyn_facevis = Level->LeafPVS(sub);
    const unsigned leafnum = Level->PointInSubsector(l->origin)-Level->Subsectors;
    if (!(dyn_facevis[leafnum>>3]&(1<<(leafnum&7)))) continue;
  }
  */
/*
  // already checked?
  if (bspVisRadius[subidx].framecount == bspVisRadiusFrame) {
    if (bspVisRadius[subidx].radius <= radius) return !!bspVisRadius[subidx].vis;
  }
  // mark as "checked"
  bspVisRadius[subidx].framecount = bspVisRadiusFrame;
  bspVisRadius[subidx].radius = radius;
  // rendered means "visible"
  if (BspVis[subidx>>3]&(1<<(subidx&7))) {
    bspVisRadius[subidx].radius = 1e12; // big!
    bspVisRadius[subidx].vis = BSPVisInfo::VISIBLE;
    return true;
  }
*/
  // rendered means "visible"
  if (BspVis[subidx>>3]&(1<<(subidx&7))) return true;

  // use floodfill to determine (rough) potential visibility
  NewBSPVisibilityFrame();
  if (!bspVisRadius) {
    bspVisRadiusFrame = 1;
    bspVisRadius = new BSPVisInfo[Level->NumSubsectors];
    memset(bspVisRadius, 0, sizeof(bspVisRadius[0])*Level->NumSubsectors);
  }
  return CheckBSPVisibilitySub(org, radius*radius, sub);
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderScene
//
//==========================================================================
void VAdvancedRenderLevel::RenderScene (const refdef_t *RD, const VViewClipper *Range) {
  guard(VAdvancedRenderLevel::RenderScene);
  if (!Drawer->SupportsAdvancedRendering()) Host_Error("Advanced rendering not supported by graphics card");

  r_viewleaf = Level->PointInSubsector(vieworg);

  TransformFrustum();

  Drawer->SetupViewOrg();

  MarkLeaves();

  if (!r_disable_world_update) UpdateWorld(RD, Range);

  RenderWorld(RD, Range);
  BuildVisibleObjectsList();

  RenderMobjsAmbient();

  Drawer->BeginShadowVolumesPass();

  linetrace_t Trace;
  TVec Delta;

  CurrLightsNumber = 0;
  CurrShadowsNumber = 0;

  const float rlightraduisSq = (r_lights_radius < 1 ? 2048*2048 : r_lights_radius*r_lights_radius);
  //const float rlightraduisSightSq = (r_lights_radius_sight_check < 1 ? 0 : r_lights_radius_sight_check*r_lights_radius_sight_check);
  //const bool hasPVS = Level->HasPVS();

  static TFrustum frustum;
  static TFrustumParam fp;

  TPlane backPlane;
  backPlane.SetPointNormal3D(vieworg, viewforward);


  if (!FixedLight && r_static_lights && r_max_lights != 0) {
    if (!staticLightsFiltered) RefilterStaticLights();

    // sort lights by distance to player, so faraway lights won't disable nearby ones
    static TArray<StLightInfo> visstatlights;
    if (visstatlights.length() < Lights.length()) visstatlights.setLength(Lights.length());
    unsigned visstatlightCount = 0;

    light_t *stlight = Lights.ptr();
    for (int i = Lights.length(); i--; ++stlight) {
      //if (!Lights[i].radius) continue;
      if (!stlight->active || stlight->radius < 8) continue;

      // don't do lights that are too far away
      Delta = stlight->origin-vieworg;
      const float distSq = Delta.lengthSquared();

      // if the light is behind a view, drop it if it is further than light radius
      if (distSq >= stlight->radius*stlight->radius) {
        if (distSq > rlightraduisSq || backPlane.PointOnBackTh(stlight->origin)) continue; // too far away
        if (fp.needUpdate(vieworg, viewangles)) {
          fp.setup(vieworg, viewangles, viewforward, viewright, viewup);
          frustum.setup(clip_base, fp, false); //true, r_lights_radius);
        }
        if (!frustum.checkSphere(stlight->origin, stlight->radius)) {
          // out of frustum
          continue;
        }
      }

      if (!CheckBSPVisibility(stlight->origin, stlight->radius)) {
        //GCon->Logf("STATIC DROP: visibility check");
        continue;
      }
      /*
      // check potential visibility (this should be moved to sight check for precise pvs, but...)
      if (hasPVS) {
        subsector_t *sub = Level->PointInSubsector(stlight->origin);
        const vuint8 *dyn_facevis = Level->LeafPVS(sub);
        if (!(dyn_facevis[stlight->leafnum>>3]&(1<<(stlight->leafnum&7)))) continue;
      }

      if (rlightraduisSightSq) {
        if (/ *dlenSq* /Delta.length2DSquared() > rlightraduisSightSq) {
          // check some more rays
          if (!RadiusCastRay(stlight->origin, vieworg, stlight->radius, / *true* /r_dynamic_clip_more)) continue;
        }
      }
      */

      if (r_advlight_sort_static) {
        StLightInfo &sli = visstatlights[visstatlightCount++];
        sli.stlight = stlight;
        sli.distSq = distSq;
      } else {
        RenderLightShadows(RD, Range, stlight->origin, stlight->radius, stlight->colour, true);
      }
    }

    // sort lights, so nearby ones will be rendered first
    if (visstatlightCount > 0) {
      timsort_r(visstatlights.ptr(), visstatlightCount, sizeof(StLightInfo), &stLightCompare, nullptr);
      for (const StLightInfo *sli = visstatlights.ptr(); visstatlightCount--; ++sli) {
        RenderLightShadows(RD, Range, sli->stlight->origin, sli->stlight->radius, sli->stlight->colour, true);
      }
    }
  }

  if (!FixedLight && r_dynamic) {
    static TArray<DynLightInfo> visdynlights;
    if (visdynlights.length() < MAX_DLIGHTS) visdynlights.setLength(MAX_DLIGHTS);
    unsigned visdynlightCount = 0;

    dlight_t *l = DLights;
    for (int i = MAX_DLIGHTS; i--; ++l) {
      if (l->radius < 8 || l->die < Level->Time) continue;

      // don't do lights that are too far away
      Delta = l->origin-vieworg;
      const float distSq = Delta.lengthSquared();

      // if the light is behind a view, drop it if it is further than light radius
      if (distSq >= l->radius*l->radius) {
        if (distSq > rlightraduisSq || backPlane.PointOnBackTh(l->origin)) continue; // too far away
        if (fp.needUpdate(vieworg, viewangles)) {
          fp.setup(vieworg, viewangles, viewforward, viewright, viewup);
          frustum.setup(clip_base, fp, false); //true, r_lights_radius);
        }
        if (!frustum.checkSphere(l->origin, l->radius)) {
          // out of frustum
          continue;
        }
      }

      if (!CheckBSPVisibility(l->origin, l->radius)) {
        //GCon->Logf("DYNAMIC DROP: visibility check");
        continue;
      }
      /*
      if (rlightraduisSightSq) {
        if (/ *dlenSq* /Delta.length2DSquared() > rlightraduisSightSq) {
          // check some more rays
          if (!RadiusCastRay(l->origin, vieworg, l->radius, / *true* /r_dynamic_clip_more)) continue;
        }
      }
      */

      if (r_advlight_sort_dynamic) {
        DynLightInfo &dli = visdynlights[visdynlightCount++];
        dli.l = l;
        dli.distSq = distSq;
      } else {
        RenderLightShadows(RD, Range, l->origin, l->radius, l->colour, true);
      }
    }

    // sort lights, so nearby ones will be rendered first
    if (visdynlightCount > 0) {
      timsort_r(visdynlights.ptr(), visdynlightCount, sizeof(DynLightInfo), &dynLightCompare, nullptr);
      for (const DynLightInfo *dli = visdynlights.ptr(); visdynlightCount--; ++dli) {
        RenderLightShadows(RD, Range, dli->l->origin, dli->l->radius, dli->l->colour, true);
      }
    }
  }

  Drawer->DrawWorldTexturesPass();
  RenderMobjsTextures();

  Drawer->DrawWorldFogPass();
  RenderMobjsFog();
  Drawer->EndFogPass();

  RenderMobjs(RPASS_NonShadow);

  DrawParticles();

  DrawTranslucentPolys();
  unguard;
}
