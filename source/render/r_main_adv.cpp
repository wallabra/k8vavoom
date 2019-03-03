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

//#define RADVLIGHT_GRID_OPTIMIZER


extern VCvarF r_lights_radius;
extern VCvarF r_lights_radius_sight_check;
extern VCvarB r_dynamic_clip_more;
extern VCvarB w_update_in_renderer;


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

  //const float rlightraduisSq = r_lights_radius*r_lights_radius;
  const float rlightraduisSightSq = r_lights_radius_sight_check*r_lights_radius_sight_check;

  const bool hasPVS = Level->HasPVS();

  static TFrustum frustum;

  if (!FixedLight && r_static_lights) {
#ifdef RADVLIGHT_GRID_OPTIMIZER
    static TMapNC<vuint32, int> slmhash;
    int statdiv = r_hashlight_static_div;
    if (statdiv > 0) slmhash.reset();
#endif

    if (!staticLightsFiltered) RefilterStaticLights();

    light_t *stlight = Lights.ptr();
    for (int i = Lights.length(); i--; ++stlight) {
      //if (!Lights[i].radius) continue;
      if (!stlight->active) continue;

      // don't do lights that are too far away
      Delta = stlight->origin-vieworg;

      // if the light is behind a view, drop it if it is further than light radius
      if (Delta.lengthSquared() >= stlight->radius*stlight->radius) {
        frustum.setup(clip_base, TFrustumParam(cl->ViewOrg, cl->ViewAngles), true, r_lights_radius);
        if (!frustum.checkSphere(stlight->origin, stlight->radius)) {
          // out of frustum
          continue;
        }
      } else {
        // already did above
        /*
        // don't add too far-away lights
        Delta.z = 0;
        const float dlenSq = Delta.length2DSquared();
        if (dlenSq > rlightraduisSq) continue;
        */
      }

      // check potential visibility (this should be moved to sight check for precise pvs, but...)
      if (hasPVS) {
        subsector_t *sub = Level->PointInSubsector(stlight->origin);
        const vuint8 *dyn_facevis = Level->LeafPVS(sub);
        if (!(dyn_facevis[stlight->leafnum>>3]&(1<<(stlight->leafnum&7)))) continue;
      }

      if (/*dlenSq*/Delta.length2DSquared() > rlightraduisSightSq) {
        // check some more rays
        if (!RadiusCastRay(stlight->origin, vieworg, stlight->radius, /*true*/r_dynamic_clip_more)) continue;
      }

#ifdef RADVLIGHT_GRID_OPTIMIZER
      // don't render too much lights around one point
      if (statdiv > 0) {
        vuint32 cc = ((((vuint32)stlight->origin.x)/(vuint32)statdiv)&0xffffu)|(((((vuint32)stlight->origin.y)/(vuint32)statdiv)&0xffffu)<<16);
        int *np = slmhash.get(cc);
        if (np) {
          // replace by light with greater radius
          if (Lights[*np].radius < stlight->radius) {
            *np = i;
          }
        } else {
          slmhash.put(cc, i);
        }
      } else
#endif
      {
        RenderLightShadows(RD, Range, stlight->origin, stlight->radius, stlight->colour, true);
      }
    }

#ifdef RADVLIGHT_GRID_OPTIMIZER
    if (statdiv > 0) {
      for (auto it = slmhash.first(); bool(it); ++it) {
        int i = it.getValue();
        RenderLightShadows(RD, Range, stlight->origin, stlight->radius, stlight->colour, true);
      }
    }
#endif
  }

  if (!FixedLight && r_dynamic) {
#ifdef RADVLIGHT_GRID_OPTIMIZER
    static TMapNC<vuint32, dlight_t *> dlmhash;
    int dyndiv = r_hashlight_dynamic_div;
    if (dyndiv > 0) dlmhash.reset();
    int lcount = 0;
    //fprintf(stderr, "=====\n");
#endif

    dlight_t *l = DLights;
    for (int i = MAX_DLIGHTS; i--; ++l) {
      if (!l->radius || l->die < Level->Time) continue;

      // don't do lights that are too far away
      Delta = l->origin-vieworg;

      // if the light is behind a view, drop it if it is further than light radius
      if (Delta.lengthSquared() >= l->radius*l->radius) {
        frustum.setup(clip_base, TFrustumParam(cl->ViewOrg, cl->ViewAngles), true, r_lights_radius);
        if (!frustum.checkSphere(l->origin, l->radius)) {
          // out of frustum
          continue;
        }
      } else {
        // already done above
        /*
        Delta.z = 0;
        const float dlenSq = Delta.length2DSquared();
        if (dlenSq > rlightraduisSq) continue;
        */
      }

      // check potential visibility (this should be moved to sight check for precise pvs, but...)
      if (hasPVS) {
        subsector_t *sub = Level->PointInSubsector(l->origin);
        const vuint8 *dyn_facevis = Level->LeafPVS(sub);
        int leafnum = Level->PointInSubsector(l->origin)-Level->Subsectors;
        if (!(dyn_facevis[leafnum>>3]&(1<<(leafnum&7)))) continue;
      }

      if (/*dlenSq*/Delta.length2DSquared() > rlightraduisSightSq) {
        // check some more rays
        if (!RadiusCastRay(l->origin, vieworg, l->radius, /*true*/r_dynamic_clip_more)) continue;
      }

#ifdef RADVLIGHT_GRID_OPTIMIZER
      // don't render too much lights around one point
      if (dyndiv > 0) {
        vuint32 cc = ((((vuint32)l->origin.x)/(vuint32)dyndiv)&0xffffu)|(((((vuint32)l->origin.y)/(vuint32)dyndiv)&0xffffu)<<16);
        dlight_t **hl = dlmhash.get(cc);
        if (hl) {
          // replace by light with greater radius
          if ((*hl)->radius < l->radius) {
            *hl = l;
            //fprintf(stderr, "  replaced (%f,%f,%f,%f) with (%f,%f,%f,%f)\n", (*hl)->origin.x, (*hl)->origin.y, (*hl)->origin.z, (*hl)->radius, l->origin.x, l->origin.y, l->origin.z, l->radius);
          } else {
            //fprintf(stderr, "  dropped (%f,%f,%f,%f)\n", l->origin.x, l->origin.y, l->origin.z, l->radius);
          }
        } else {
          dlmhash.put(cc, l);
          ++lcount;
        }
      } else
#endif
      {
        RenderLightShadows(RD, Range, l->origin, l->radius, l->colour, true);
      }
    }

#ifdef RADVLIGHT_GRID_OPTIMIZER
    if (dyndiv > 0) {
      for (auto it = dlmhash.first(); bool(it); ++it) {
        dlight_t *dlt = it.getValue();
        RenderLightShadows(RD, Range, dlt->origin, dlt->radius, dlt->colour, true);
        --lcount;
      }
      if (lcount != 0) Sys_Error("unbalanced dlights");
    }
#endif
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
