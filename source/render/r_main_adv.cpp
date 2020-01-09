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
#include "gamedefs.h"
#include "r_local.h"


extern VCvarF r_lights_radius;
extern VCvarI r_max_lights;
//extern VCvarB r_disable_world_update;

static VCvarB r_advlight_sort_static("r_advlight_sort_static", true, "Sort visible static lights, so nearby lights will be rendered first?", CVAR_Archive|CVAR_PreInit);
static VCvarB r_advlight_sort_dynamic("r_advlight_sort_dynamic", true, "Sort visible dynamic lights, so nearby lights will be rendered first?", CVAR_Archive|CVAR_PreInit);
// no need to do this, because light rendering will do it again anyway
// yet it seems to be slightly faster for complex maps with alot of static lights
static VCvarB r_advlight_flood_check("r_advlight_flood_check", true, "Check static light visibility with floodfill before trying to render it?", CVAR_Archive|CVAR_PreInit);

static VCvarB dbg_adv_show_light_count("dbg_adv_show_light_count", false, "Show number of rendered lights?", CVAR_PreInit);
static VCvarB dbg_adv_show_light_seg_info("dbg_adv_show_light_seg_info", false, "Show totals of rendered light/shadow segments?", CVAR_PreInit);

static VCvarI dbg_adv_force_static_lights_radius("dbg_adv_force_static_lights_radius", "0", "Force static light radius.", CVAR_PreInit);
static VCvarI dbg_adv_force_dynamic_lights_radius("dbg_adv_force_dynamic_lights_radius", "0", "Force dynamic light radius.", CVAR_PreInit);


struct StLightInfo {
  VRenderLevelShared::light_t *stlight; // light
  float distSq; // distance
  float zofs; // origin z offset
};

struct DynLightInfo {
  dlight_t *l; // light
  float distSq; // distance
  //float zofs; // origin z offset
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
//  VRenderLevelShadowVolume::VRenderLevelShadowVolume
//
//==========================================================================
VRenderLevelShadowVolume::VRenderLevelShadowVolume (VLevel *ALevel)
  : VRenderLevelShared(ALevel)
{
  mIsShadowVolumeRenderer = true;
  float mt = clampval(r_fade_mult_advanced.asFloat(), 0.0f, 16.0f);
  if (mt <= 0.0f) mt = 1.0f;
  VDrawer::LightFadeMult = mt;
}


//==========================================================================
//
//  VRenderLevelShadowVolume::~VRenderLevelShadowVolume
//
//==========================================================================
VRenderLevelShadowVolume::~VRenderLevelShadowVolume () {
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderScene
//
//==========================================================================
void VRenderLevelShadowVolume::RenderScene (const refdef_t *RD, const VViewClipper *Range) {
  if (!Drawer->SupportsShadowVolumeRendering()) Host_Error("Shadow volume rendering is not supported by your graphics card");

  //r_viewleaf = Level->PointInSubsector(Drawer->vieworg); // moved to `PrepareWorldRender()`

  TransformFrustum();

  Drawer->SetupViewOrg();

#if 0
  {
    VMatrix4 model, proj;
    Drawer->GetModelMatrix(model);
    Drawer->GetProjectionMatrix(proj);

    VMatrix4 comb;
    comb.ModelProjectCombine(model, proj);

    TPlane planes[6];
    //VMatrix4::ExtractFrustum(model, proj, planes);
    comb.ExtractFrustum(planes);


    GCon->Log("=== FRUSTUM ===");
    for (unsigned f = 0; f < 6; ++f) {
      const float len = planes[f].normal.length();
      planes[f].normal /= len;
      planes[f].dist /= len;
      //planes[f].Normalise();
      GCon->Logf("  GL plane #%u: (%9f,%9f,%9f) : %f", f, planes[f].normal.x, planes[f].normal.y, planes[f].normal.z, planes[f].dist);
      GCon->Logf("  MY plane #%u: (%9f,%9f,%9f) : %f", f, Drawer->view_frustum.planes[f].normal.x, Drawer->view_frustum.planes[f].normal.y, Drawer->view_frustum.planes[f].normal.z, Drawer->view_frustum.planes[f].dist);
    }

    // we aren't interested in far plane
    for (unsigned f = 0; f < 4; ++f) {
      Drawer->view_frustum.planes[f] = planes[f];
      Drawer->view_frustum.planes[f].clipflag = 1U<<f;
    }
    // near plane for reverse z is "far"
    if (Drawer->CanUseRevZ()) {
      Drawer->view_frustum.planes[TFrustum::Back] = planes[5];
    } else {
      Drawer->view_frustum.planes[TFrustum::Back] = planes[4];
    }
    Drawer->view_frustum.planes[TFrustum::Back].clipflag = TFrustum::BackBit;
    Drawer->view_frustum.planeCount = 5;
    //vassert(Drawer->view_frustum.planes[4].PointOnSide(Drawer->vieworg));
  }
#endif

  //ClearQueues(); // moved to `PrepareWorldRender()`
  //MarkLeaves(); // moved to `PrepareWorldRender()`
  //if (!MirrorLevel && !r_disable_world_update) UpdateFakeSectors();

  RenderWorld(RD, Range);
  BuildVisibleObjectsList();

  RenderMobjsAmbient();

  //GCon->Log("***************** RenderScene *****************");
  //FIXME: mirrors can use stencils, and advlight too...
  if (!MirrorLevel) {
    Drawer->BeginShadowVolumesPass();

    linetrace_t Trace;
    TVec Delta;

    CurrLightsNumber = 0;
    CurrShadowsNumber = 0;
    AllLightsNumber = 0;
    AllShadowsNumber = 0;

    const float rlightraduisSq = (r_lights_radius < 1 ? 2048*2048 : r_lights_radius*r_lights_radius);
    //const bool hasPVS = Level->HasPVS();

    // no need to do this, because light rendering will do it again anyway
    const bool checkLightVis = r_advlight_flood_check.asBool();

    static TFrustum frustum;
    static TFrustumParam fp;

    TPlane backPlane;
    backPlane.SetPointNormal3D(Drawer->vieworg, Drawer->viewforward);

    LightsRendered = 0;
    DynLightsRendered = 0;
    DynamicLights = false;

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

        TVec lorg = stlight->origin;

        // don't do lights that are too far away
        Delta = lorg-Drawer->vieworg;
        const float distSq = Delta.lengthSquared();

        // if the light is behind a view, drop it if it is further than light radius
        if (distSq >= stlight->radius*stlight->radius) {
          if (distSq > rlightraduisSq || backPlane.PointOnSide(lorg)) continue; // too far away
          if (fp.needUpdate(Drawer->vieworg, Drawer->viewangles)) {
            fp.setup(Drawer->vieworg, Drawer->viewangles, Drawer->viewforward, Drawer->viewright, Drawer->viewup);
            frustum.setup(clip_base, fp, false); //true, r_lights_radius);
          }
          if (!frustum.checkSphere(lorg, stlight->radius)) {
            // out of frustum
            continue;
          }
        }

        // drop lights inside sectors without height
        if (stlight->leafnum < 0 || stlight->leafnum >= Level->NumSubsectors) {
          stlight->leafnum = (int)(ptrdiff_t)(Level->PointInSubsector(stlight->origin)-Level->Subsectors);
        }

        const sector_t *sec = Level->Subsectors[stlight->leafnum].sector;
        if (!CheckValidLightPosRough(lorg, sec)) continue;
        if (checkLightVis && !CheckBSPVisibilityBox(lorg, stlight->radius, &Level->Subsectors[stlight->leafnum])) continue;

        StLightInfo &sli = visstatlights[visstatlightCount++];
        sli.stlight = stlight;
        sli.distSq = distSq;
        sli.zofs = lorg.z-stlight->origin.z;
      }

      // sort lights, so nearby ones will be rendered first
      if (visstatlightCount > 0) {
        if (r_advlight_sort_static) {
          timsort_r(visstatlights.ptr(), visstatlightCount, sizeof(StLightInfo), &stLightCompare, nullptr);
        }
        for (const StLightInfo *sli = visstatlights.ptr(); visstatlightCount--; ++sli) {
          VEntity *own = (sli->stlight->owner && sli->stlight->owner->IsA(VEntity::StaticClass()) ? sli->stlight->owner : nullptr);
          vuint32 flags = (own && R_EntModelNoSelfShadow(own) ? dlight_t::NoSelfShadow : 0);
          //if (own) GCon->Logf("STLOWN: %s", *own->GetClass()->GetFullName());
          TVec lorg = sli->stlight->origin;
          lorg.z += sli->zofs;
          RenderLightShadows(own, flags, RD, Range, lorg, (dbg_adv_force_static_lights_radius > 0 ? dbg_adv_force_static_lights_radius : sli->stlight->radius), 0.0f, sli->stlight->color, true);
        }
      }
    }

    //int rlStatic = LightsRendered;
    DynamicLights = true;

    if (!FixedLight && r_dynamic_lights && r_max_lights != 0) {
      static TArray<DynLightInfo> visdynlights;
      if (visdynlights.length() < MAX_DLIGHTS) visdynlights.setLength(MAX_DLIGHTS);
      unsigned visdynlightCount = 0;

      dlight_t *l = DLights;
      for (int i = MAX_DLIGHTS; i--; ++l) {
        if (l->radius < l->minlight+8 || l->die < Level->Time) continue;

        TVec lorg = l->origin;

        // drop lights inside sectors without height
        /* it is not set here yet; why?! we should calc leafnum!
        const int leafnum = dlinfo[i].leafnum;
        GCon->Logf(NAME_Debug, "dl #%d: lfn=%d", i, leafnum);
        if (leafnum >= 0 && leafnum < Level->NumSubsectors) {
          const sector_t *sec = Level->Subsectors[leafnum].sector;
          if (!CheckValidLightPosRough(lorg, sec)) continue;
        }
        */

        // don't do lights that are too far away
        Delta = lorg-Drawer->vieworg;
        const float distSq = Delta.lengthSquared();

        // if the light is behind a view, drop it if it is further than light radius
        if (distSq >= l->radius*l->radius) {
          if (distSq > rlightraduisSq || backPlane.PointOnSide(lorg)) continue; // too far away
          if (fp.needUpdate(Drawer->vieworg, Drawer->viewangles)) {
            fp.setup(Drawer->vieworg, Drawer->viewangles, Drawer->viewforward, Drawer->viewright, Drawer->viewup);
            frustum.setup(clip_base, fp, false); //true, r_lights_radius);
          }
          if (!frustum.checkSphere(lorg, l->radius)) {
            // out of frustum
            continue;
          }
        }

        DynLightInfo &dli = visdynlights[visdynlightCount++];
        dli.l = l;
        dli.distSq = distSq;
        //dli.zofs = lorg.z-l->origin.z;
      }

      // sort lights, so nearby ones will be rendered first
      if (visdynlightCount > 0) {
        if (r_advlight_sort_dynamic) {
          timsort_r(visdynlights.ptr(), visdynlightCount, sizeof(DynLightInfo), &dynLightCompare, nullptr);
        }
        for (const DynLightInfo *dli = visdynlights.ptr(); visdynlightCount--; ++dli) {
          VEntity *own = (dli->l->Owner && dli->l->Owner->IsA(VEntity::StaticClass()) ? (VEntity *)dli->l->Owner : nullptr);
          if (own && R_EntModelNoSelfShadow(own)) dli->l->flags |= dlight_t::NoSelfShadow;
          //TVec lorg = dli->l->origin;
          //lorg.z += dli->zofs;
          // always render player lights
          const bool forced = (own && own->IsPlayer());
          RenderLightShadows(own, dli->l->flags, RD, Range, /*lorg*/dli->l->origin, (dbg_adv_force_dynamic_lights_radius > 0 ? dbg_adv_force_dynamic_lights_radius : dli->l->radius), dli->l->minlight, dli->l->color, true, dli->l->coneDirection, dli->l->coneAngle, forced);
        }
      }
    }

    if (dbg_adv_show_light_count) {
      GCon->Logf("total lights per frame: %d (%d static, %d dynamic)", LightsRendered, LightsRendered-DynLightsRendered, DynLightsRendered);
    }

    if (dbg_adv_show_light_seg_info) {
      GCon->Logf("rendered %d shadow segs, and %d light segs", AllShadowsNumber, AllLightsNumber);
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

  RenderPortals();
}
