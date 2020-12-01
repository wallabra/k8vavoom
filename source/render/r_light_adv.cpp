//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš, dj_jl
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
#include <limits.h>
#include <float.h>

#include "../gamedefs.h"
#include "r_local.h"

// with this defined, it glitches
// you can see it on map01, for example, by firing imp fireball
//#define VV_LADV_CLIPCHECK_REGIONS_LIGHT
//#define VV_LADV_CLIPCHECK_REGIONS_SHADOW

#define VV_LADV_STRANGE_REGION_SORTING


/*
  possible shadow volume optimisations:

  for things like pillars (i.e. sectors that has solid walls facing outwards),
  we can construct a shadow silhouette. that is, all connected light-facing
  walls can be replaced with one. this will render much less sides.
  that is, we can effectively turn our pillar into cube.

  we can prolly use sector lines to render this (not segs). i think that it
  will be better to use sector lines to render shadow volumes in any case,
  'cause we don't really need to render one line in several segments.
  that is, one line split by BSP gives us two surfaces, which adds two
  unnecessary polygons to shadow volume. by using linedefs instead, we can
  avoid this. there is no need to create texture coordinates and surfaces
  at all: we can easily calculate all required vertices.

  note that we cannot do the same with floors and ceilings: they can have
  arbitrary shape.

  actually, we can solve this in another way. note that we need to extrude
  only silhouette edges. so we can collect surfaces from connected subsectors
  into "shape", and run silhouette extraction on it. this way we can extrude
  only a silhouette. and we can avoid rendering caps if our camera is not
  lie inside any shadow volume (and volume is not clipped by near plane).

  we can easily determine if the camera is inside a volume by checking if
  it lies inside any extruded subsector. as subsectors are convex, this is a
  simple set of point-vs-plane checks. first check if light and camera are
  on a different sides of a surface, and do costly checks only if they are.

  if the camera is outside of shadow volume, we can use faster z-pass method.

  if a light is behind a camera, we can move back frustum plane, so it will
  contain light origin, and clip everything behind it. the same can be done
  for all other frustum planes. or we can build full light-vs-frustum clip.
*/


#include "r_light_adv_cvars.cpp"
#include "r_light_adv_fltopt.cpp"
#include "r_light_adv_shadow.cpp"
#include "r_light_adv_light.cpp"


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightShadows
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightShadows (VEntity *ent, vuint32 dlflags, const refdef_t *RD,
                                                   const VViewClipper *Range,
                                                   TVec &Pos, float Radius, float LightMin, vuint32 Color,
                                                   bool LimitLights, TVec coneDir, float coneAngle, bool forceRender)
{
  if (Radius <= LightMin || gl_dbg_wireframe) return;

  if (!forceRender) {
    // check limits
    if (!DynamicLights) {
      // static lights
      if (r_max_lights >= 0 && LightsRendered >= r_max_lights) return;
    } else {
      // dynamic lights
      if (r_max_lights >= 0 && LightsRendered >= r_max_lights) {
        // enforce dynlight limit
        if (r_dynlight_minimum >= 0 && DynLightsRendered >= r_dynlight_minimum) return;
      }
    }
  }

  //TODO: we can reuse collected surfaces in next passes
  LitCalcBBox = true;
  if (!CalcLightVis(Pos, Radius-LightMin)) return;

  if (!LitVisSubHit) return; // something is wrong, light didn't hit any subsector at all

  if (!LitSurfaceHit /*&& !r_models*/) return; // no lit surfaces/subsectors, and no need to light models, so nothing to do

  // if our light is in frustum, ignore any out-of-frustum polys
  CurrLightInFrustum = (r_advlight_opt_frustum_full && Drawer->viewfrustum.checkSphere(Pos, Radius /*-LightMin+4.0f*/));
  CurrLightInFront = (r_advlight_opt_frustum_back && Drawer->viewfrustum.checkSphere(Pos, Radius, TFrustum::NearBit));

  bool allowShadows = doShadows;

  // if we want model shadows, always do full rendering
  //FIXME: we shoud check if we have any model that can cast shadow instead
  //FIXME: also, models can be enabled, but we may not have any models loaded
  //FIXME: for now, `doShadows` only set to false for very small lights (radius<8), so it doesn't matter
  if (!allowShadows && r_draw_mobjs && r_models && r_model_shadows) allowShadows = true;

  if (dlflags&dlight_t::NoShadow) allowShadows = false;

  if (!r_allow_shadows) allowShadows = false;

  // if we don't need shadows, and no visible subsectors were hit, we have nothing to do here
  if (!allowShadows && (!HasLightIntersection || !LitSurfaceHit)) return;

  if (!allowShadows && dbg_adv_light_notrace_mark) {
    //Color = 0xffff0000U;
    Color = 0xffff00ffU; // purple; it should be very noticeable
  }

  ++LightsRendered;
  if (DynamicLights) ++DynLightsRendered;

  CurrShadowsNumber = 0;
  CurrLightsNumber = 0;
  CurrLightRadius = Radius; // we need full radius, not modified
  CurrLightColor = Color;

  const bool optimiseScissor = r_advlight_opt_scissor.asBool();

  //  0 if scissor is empty
  // -1 if scissor has no sense (should not be used)
  //  1 if scissor is set
  int hasScissor = 1;
  int scoord[4];
  bool checkModels = false;
  float dummyBBox[6];

  //GCon->Logf("LBB:(%f,%f,%f)-(%f,%f,%f)", LitBBox[0], LitBBox[1], LitBBox[2], LitBBox[3], LitBBox[4], LitBBox[5]);

  const bool useShadowMaps = (r_shadowmaps.asBool() && Drawer->CanRenderShadowMaps());

  if (!useShadowMaps) {
    // setup light scissor rectangle
    if (optimiseScissor) {
      hasScissor = Drawer->SetupLightScissor(Pos, Radius-LightMin, scoord, (r_advlight_opt_optimise_scissor ? LitBBox : nullptr));
      if (hasScissor <= 0) {
        // something is VERY wrong (-1), or scissor is empty (0)
        Drawer->ResetScissor();
        if (!hasScissor && r_advlight_opt_optimise_scissor && !r_models) return; // empty scissor
        checkModels = r_advlight_opt_optimise_scissor;
        hasScissor = 0;
        scoord[0] = scoord[1] = 0;
        scoord[2] = Drawer->getWidth();
        scoord[3] = Drawer->getHeight();
      } else {
        if (scoord[0] == 0 && scoord[1] == 0 && scoord[2] == Drawer->getWidth() && scoord[3] == Drawer->getHeight()) {
          hasScissor = 0;
        }
      }
    }
  } else {
    Drawer->ResetScissor();
  }

  // if there are no lit surfaces oriented away from camera, it cannot possibly be in shadow volume
  // nope, it is wrong
  //bool useZPass = !HasBackLit;
  //if (useZPass) GCon->Log("*** ZPASS");
  //useZPass = false;
  bool useZPass = false;

  /* nope
  if ((CurrLightPos-Drawer->vieworg).lengthSquared() > CurrLightRadius*CurrLightRadius) {
    useZPass = true;
  }
  */

  if (r_max_light_segs_all < 0 && r_max_light_segs_one < 0) LimitLights = false;

  BuildMobjsInCurrLight(allowShadows);

  // if we want to scissor on geometry, check if any lit model is out of our light bbox.
  // stop right here! say, is there ANY reason to not limit light box with map geometry?
  // after all, most of the things that can receive light is contained inside a map.
  // still, we may miss some lighting on models from flying lights that cannot touch
  // any geometry at all. to somewhat ease this case, rebuild light box when the light
  // didn't touched anything.
  if (optimiseScissor && allowShadows && checkModels && !useShadowMaps) {
    if (mobjsInCurrLight.length() == 0) return; // nothing to do, as it is guaranteed that light cannot touch map geometry
    float xbbox[6] = {0};
    /*
    xbbox[0+0] = LitBBox[0].x;
    xbbox[0+1] = LitBBox[0].y;
    xbbox[0+2] = LitBBox[0].z;
    xbbox[3+0] = LitBBox[1].x;
    xbbox[3+1] = LitBBox[1].y;
    xbbox[3+2] = LitBBox[1].z;
    */
    bool wasHit = false;
    for (auto &&ment : mobjsInCurrLight) {
      if (ment == ViewEnt && (!r_chasecam || ViewEnt != cl->MO)) continue; // don't draw camera actor
      // skip things in subsectors that are not visible
      const int SubIdx = (int)(ptrdiff_t)(ment->SubSector-Level->Subsectors);
      if (!IsSubsectorLitBspVis(SubIdx)) continue;
      const float eradius = ment->GetRenderRadius();
      if (eradius < 1) continue;
      if (!HasEntityAliasModel(ment)) continue;
      // assume that it is not bigger than its radius
      float zup, zdown;
      if (ment->Height < 2) {
        //GCon->Logf("  <%s>: height=%f; radius=%f", *ment->GetClass()->GetFullName(), ment->Height, ment->Radius);
        zup = eradius;
        zdown = eradius;
      } else {
        zup = ment->Height;
        zdown = 0;
      }
      if (wasHit) {
        xbbox[0+0] = min2(xbbox[0+0], ment->Origin.x-eradius);
        xbbox[0+1] = min2(xbbox[0+1], ment->Origin.y-eradius);
        xbbox[0+2] = min2(xbbox[0+2], ment->Origin.z-zup);
        xbbox[3+0] = max2(xbbox[3+0], ment->Origin.x+eradius);
        xbbox[3+1] = max2(xbbox[3+1], ment->Origin.y+eradius);
        xbbox[3+2] = max2(xbbox[3+2], ment->Origin.z+zdown);
      } else {
        wasHit = true;
        xbbox[0+0] = ment->Origin.x-eradius;
        xbbox[0+1] = ment->Origin.y-eradius;
        xbbox[0+2] = ment->Origin.z-zup;
        xbbox[3+0] = ment->Origin.x+eradius;
        xbbox[3+1] = ment->Origin.y+eradius;
        xbbox[3+2] = ment->Origin.z+zdown;
      }
    }
    if (wasHit &&
        (xbbox[0+0] < LitBBox[0].x || xbbox[0+1] < LitBBox[0].y || xbbox[0+2] < LitBBox[0].z ||
         xbbox[3+0] > LitBBox[1].x || xbbox[3+1] > LitBBox[1].y || xbbox[3+2] > LitBBox[1].z))
    {
      /*
      GCon->Logf("fixing light bbox; old=(%f,%f,%f)-(%f,%f,%f); new=(%f,%f,%f)-(%f,%f,%f)",
        LitBBox[0].x, LitBBox[0].y, LitBBox[0].z,
        LitBBox[1].x, LitBBox[1].y, LitBBox[1].z,
        xbbox[0], xbbox[1], xbbox[2], xbbox[3], xbbox[4], xbbox[5]);
      */
      LitBBox[0].x = min2(xbbox[0+0], LitBBox[0].x);
      LitBBox[0].y = min2(xbbox[0+1], LitBBox[0].y);
      LitBBox[0].z = min2(xbbox[0+2], LitBBox[0].z);
      LitBBox[1].x = max2(xbbox[3+0], LitBBox[1].x);
      LitBBox[1].y = max2(xbbox[3+1], LitBBox[1].y);
      LitBBox[1].z = max2(xbbox[3+2], LitBBox[1].z);
      hasScissor = Drawer->SetupLightScissor(Pos, Radius-LightMin, scoord, LitBBox);
      if (hasScissor <= 0) {
        // something is VERY wrong (-1), or scissor is empty (0)
        Drawer->ResetScissor();
        if (!hasScissor) return; // empty scissor
        hasScissor = 0;
        scoord[0] = scoord[1] = 0;
        scoord[2] = Drawer->getWidth();
        scoord[3] = Drawer->getHeight();
      } else {
        if (scoord[0] == 0 && scoord[1] == 0 && scoord[2] == Drawer->getWidth() && scoord[3] == Drawer->getHeight()) {
          hasScissor = 0;
        }
      }
    }
  }

  // do shadow volumes
  if (useShadowMaps) {
    Drawer->BeginLightShadowMaps(CurrLightPos, CurrLightRadius, coneDir, coneAngle, refdef.width, refdef.height);
    if (allowShadows) {
      (void)fsecCounterGen(); // for checker
      if (r_max_shadow_segs_all) {
        smapSurfaces.reset();
        LightClip.ClearClipNodes(CurrLightPos, Level, CurrLightRadius);
        dummyBBox[0] = dummyBBox[1] = dummyBBox[2] = -99999;
        dummyBBox[3] = dummyBBox[4] = dummyBBox[5] = +99999;
        RenderShadowBSPNode(Level->NumNodes-1, dummyBBox, LimitLights);
        for (unsigned fc = 0; fc < 6; ++fc) {
          Drawer->SetupLightShadowMap(fc);
          for (auto &&surf : smapSurfaces) Drawer->RenderSurfaceShadowMap(surf);
        }
      }
    }
    Drawer->EndLightShadowMaps();
  } else {
    LightClip.ClearClipNodes(CurrLightPos, Level, CurrLightRadius);
    Drawer->BeginLightShadowVolumes(CurrLightPos, CurrLightRadius, useZPass, hasScissor, scoord, coneDir, coneAngle);
    if (allowShadows) {
      (void)fsecCounterGen(); // for checker
      if (r_shadowvol_use_pofs) {
        // pull forward
        Drawer->GLPolygonOffsetEx(r_shadowvol_pslope, -r_shadowvol_pofs);
      }
      if (r_max_shadow_segs_all) {
        dummyBBox[0] = dummyBBox[1] = dummyBBox[2] = -99999;
        dummyBBox[3] = dummyBBox[4] = dummyBBox[5] = +99999;
        RenderShadowBSPNode(Level->NumNodes-1, dummyBBox, LimitLights);
      }
      Drawer->BeginModelsShadowsPass(CurrLightPos, CurrLightRadius);
      RenderMobjsShadow(ent, dlflags);
      if (r_shadowvol_use_pofs) {
        Drawer->GLDisableOffset();
      }
      Drawer->EndModelsShadowsPass();
    }
    Drawer->EndLightShadowVolumes();
  }

  // k8: the question is: why we are rendering surfaces instead
  //     of simply render a light circle? shadow volumes should
  //     take care of masking the area, so simply rendering a
  //     circle should do the trick.
  // k8: answering to the silly younger ketmar: because we cannot
  //     read depth info, and we need normals to calculate light
  //     intensity, and so on.

  // draw light
  Drawer->BeginLightPass(CurrLightPos, CurrLightRadius, LightMin, Color, allowShadows);
  LightClip.ClearClipNodes(CurrLightPos, Level, CurrLightRadius);
  dummyBBox[0] = dummyBBox[1] = dummyBBox[2] = -99999;
  dummyBBox[3] = dummyBBox[4] = dummyBBox[5] = +99999;
  RenderLightBSPNode(Level->NumNodes-1, dummyBBox, LimitLights);
  Drawer->EndLightPass();

  Drawer->BeginModelsLightPass(CurrLightPos, CurrLightRadius, LightMin, Color, coneDir, coneAngle);
  RenderMobjsLight(ent);
  Drawer->EndModelsLightPass();

  //if (hasScissor) Drawer->DebugRenderScreenRect(scoord[0], scoord[1], scoord[2], scoord[3], 0x7f007f00);

  /*if (hasScissor)*/ Drawer->ResetScissor();
}
