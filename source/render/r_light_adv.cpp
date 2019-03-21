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
#include <limits.h>
#include <float.h>

#include "gamedefs.h"
#include "r_local.h"


extern VCvarB r_darken;
extern VCvarI r_ambient;
extern VCvarB r_allow_ambient;
extern VCvarB r_allow_subtractive_lights;
extern VCvarB r_dynamic_clip;
extern VCvarB r_dynamic_clip_more;
extern VCvarB r_draw_mobjs;
extern VCvarB r_models;
extern VCvarB r_model_shadows;
extern VCvarB r_draw_pobj;


/*
  possible shadow volume optimisation:
  for things like pillars (i.e. sectors that has solid walls facing outwards),
  we can construct a shadow silhouette. that is, all connected light-facing
  walls can be replaced with one. this will render much less sides.
  that is, we can effectively turn our pillar into cube.

  we can prolly use sector lines to render this (not segs). i thinkg that it
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
  onyl a silhouette. and we can avoid rendering caps if our camera is not
  lie inside any shadow volume (and volume is not clipped by near plane).

  we can easily determine if the camera is inside a volume by checking if
  it lies inside any extruded subsector. as subsectors are convex, this is a
  simple set of point-vs-plane checks. first check if light and camera are
  on a different sides of a surface, and do costly checks only if they are.

  if the camera is outside of shadow volume, we can use faster z-pass method.

  if a light is behing a camera, we can move back frustum plane, so it will
  contain light origin, and clip everything behind it. the same can be done
  for all other frustum planes. or we can build full light-vs-frustum clip.
*/


// ////////////////////////////////////////////////////////////////////////// //
// private data definitions
// ////////////////////////////////////////////////////////////////////////// //

//VCvarI r_max_model_lights("r_max_model_lights", "32", "Maximum lights that can affect one model when we aren't using model shadows.", CVAR_Archive);
VCvarI r_max_model_shadows("r_max_model_shadows", "16", "Maximum number of shadows one model can cast.", CVAR_Archive);

VCvarI r_max_lights("r_max_lights", "64", "Maximum lights.", CVAR_Archive);
static VCvarI r_max_light_segs_all("r_max_light_segs_all", "-1", "Maximum light segments for all lights.", CVAR_Archive);
static VCvarI r_max_light_segs_one("r_max_light_segs_one", "-1", "Maximum light segments for one light.", CVAR_Archive);
// was 128, but with scissored light, there is no sense to limit it anymore
static VCvarI r_max_shadow_segs_all("r_max_shadow_segs_all", "-1", "Maximum shadow segments for all lights.", CVAR_Archive);
static VCvarI r_max_shadow_segs_one("r_max_shadow_segs_one", "-1", "Maximum shadow segments for one light.", CVAR_Archive);

VCvarF r_light_filter_static_coeff("r_light_filter_static_coeff", "0.2", "How close static lights should be to be filtered out?\n(0.5-0.7 is usually ok).", CVAR_Archive);

extern VCvarB r_dynamic_clip_more;

VCvarB dbg_adv_light_notrace_mark("dbg_adv_light_notrace_mark", false, "Mark notrace lights red?", CVAR_PreInit);

//static VCvarB r_advlight_opt_trace("r_advlight_opt_trace", true, "Try to skip shadow volumes when a light can cast no shadow.", CVAR_Archive|CVAR_PreInit);
static VCvarB r_advlight_opt_scissor("r_advlight_opt_scissor", true, "Use scissor rectangle to limit light overdraws.", CVAR_Archive);
// this is wrong for now
static VCvarB r_advlight_opt_frustum_full("r_advlight_opt_frustum_full", false, "Optimise 'light is in frustum' case.", CVAR_Archive);
static VCvarB r_advlight_opt_frustum_back("r_advlight_opt_frustum_back", false, "Optimise 'light is in frustum' case.", CVAR_Archive);

VCvarB r_advlight_opt_optimise_scissor("r_advlight_opt_optimise_scissor", true, "Optimise scissor with lit geometry bounds.", CVAR_Archive);


// ////////////////////////////////////////////////////////////////////////// //
// code
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VAdvancedRenderLevel::RefilterStaticLights
//
//==========================================================================
void VAdvancedRenderLevel::RefilterStaticLights () {
  staticLightsFiltered = true;

  float coeff = r_light_filter_static_coeff;

  int llen = Lights.length();
  int actlights = 0;
  for (int currlidx = 0; currlidx < llen; ++currlidx) {
    light_t &cl = Lights[currlidx];
    if (coeff > 0) {
      cl.active = (cl.radius > 6); // arbitrary limit
    } else {
      cl.active = true;
    }
    if (cl.active) ++actlights;
  }
  if (actlights < 6) return; // arbitrary limit

  if (coeff <= 0) return; // no filtering
  if (coeff > 8) coeff = 8;

  for (int currlidx = 0; currlidx < llen; ++currlidx) {
    light_t &cl = Lights[currlidx];
    if (!cl.active) continue; // already filtered out
    // remove nearby lights with radius less than ours (or ourself if we'll hit bigger light)
    float radsq = (cl.radius*cl.radius)*coeff;
    for (int nlidx = currlidx+1; nlidx < llen; ++nlidx) {
      light_t &nl = Lights[nlidx];
      if (!nl.active) continue; // already filtered out
      const float distsq = length2DSquared(cl.origin-nl.origin);
      if (distsq >= radsq) continue;

      // check potential visibility
      /*
      subsector_t *sub = Level->PointInSubsector(nl.origin);
      const vuint8 *dyn_facevis = Level->LeafPVS(sub);
      if (!(dyn_facevis[nl.leafnum>>3]&(1<<(nl.leafnum&7)))) continue;
      */

      // if we cannot trace a line between two lights, they are prolly divided by a wall or floor
      linetrace_t Trace;
      if (!Level->TraceLine(Trace, nl.origin, cl.origin, SPF_NOBLOCKSIGHT)) continue;

      if (nl.radius <= cl.radius) {
        // deactivate nl
        nl.active = false;
      } else /*if (nl.radius > cl.radius)*/ {
        // deactivate cl
        cl.active = false;
        // there is no sense to continue
        break;
      }
    }
  }

  actlights = 0;
  for (int currlidx = 0; currlidx < llen; ++currlidx) {
    light_t &cl = Lights[currlidx];
    if (cl.active) ++actlights;
  }

  GCon->Logf("ADVRENDERER: filtered %d static lights out of %d (%d left)", llen-actlights, llen, actlights);
}


//==========================================================================
//
//  VAdvancedRenderLevel::LightPoint
//
//==========================================================================
vuint32 VAdvancedRenderLevel::LightPoint (const TVec &p, float radius, const TPlane *surfplane) {
  if (FixedLight) return FixedLight|(FixedLight<<8)|(FixedLight<<16)|(FixedLight<<24);

  float l = 0, lr = 0, lg = 0, lb = 0;

  subsector_t *sub = Level->PointInSubsector(p);
  subregion_t *reg = sub->regions;
  if (reg) {
    while (reg->next) {
      const float d = DotProduct(p, reg->floor->secplane->normal)-reg->floor->secplane->dist;
      if (d >= 0.0f) break;
      reg = reg->next;
    }

    // region's base light
    if (r_allow_ambient) {
      l = reg->secregion->params->lightlevel+ExtraLight;
      l = MID(0.0f, l, 255.0f);
      if (r_darken) l = light_remap[(int)l];
      if (l < r_ambient) l = r_ambient;
      l = MID(0.0f, l, 255.0f);
    } else {
      l = 0;
    }
    int SecLightColour = reg->secregion->params->LightColour;
    lr = ((SecLightColour>>16)&255)*l/255.0f;
    lg = ((SecLightColour>>8)&255)*l/255.0f;
    lb = (SecLightColour&255)*l/255.0f;
  }

  const vuint8 *dyn_facevis = (Level->HasPVS() ? Level->LeafPVS(sub) : nullptr);

  // add static lights
  if (r_static_lights) {
    if (!staticLightsFiltered) RefilterStaticLights();
    const light_t *stl = Lights.Ptr();
    for (int i = Lights.length(); i--; ++stl) {
      //if (!stl->radius) continue;
      if (!stl->active) continue;
      // check potential visibility
      if (dyn_facevis && !(dyn_facevis[stl->leafnum>>3]&(1<<(stl->leafnum&7)))) continue;
      const float distSq = (p-stl->origin).lengthSquared();
      if (distSq >= stl->radius*stl->radius) continue; // too far away
      const float add = stl->radius-sqrtf(distSq);
      if (add > 8) {
        if (r_dynamic_clip) {
          if (surfplane && surfplane->PointOnSide(stl->origin)) continue;
          if (!RadiusCastRay(p, stl->origin, radius, false/*r_dynamic_clip_more*/)) continue;
        }
        l += add;
        lr += add*((stl->colour>>16)&255)/255.0f;
        lg += add*((stl->colour>>8)&255)/255.0f;
        lb += add*(stl->colour&255)/255.0f;
      }
    }
  }

  // add dynamic lights
  if (r_dynamic && sub->dlightframe == currDLightFrame) {
    for (unsigned i = 0; i < MAX_DLIGHTS; ++i) {
      if (!(sub->dlightbits&(1U<<i))) continue;
      if (!dlinfo[i].needTrace) continue;
      // check potential visibility
      if (dyn_facevis) {
        //int leafnum = Level->PointInSubsector(dl.origin)-Level->Subsectors;
        const int leafnum = dlinfo[i].leafnum;
        if (leafnum < 0) continue;
        if (!(dyn_facevis[leafnum>>3]&(1<<(leafnum&7)))) continue;
      }
      const dlight_t &dl = DLights[i];
      if (dl.type == DLTYPE_Subtractive && !r_allow_subtractive_lights) continue;
      //if (!dl.radius || dl.die < Level->Time) continue; // this is not needed here
      const float distSq = (p-dl.origin).lengthSquared();
      if (distSq >= dl.radius*dl.radius) continue; // too far away
      float add = (dl.radius-dl.minlight)-sqrtf(distSq);
      if (add > 8) {
        // check potential visibility
        if (r_dynamic_clip) {
          if (surfplane && surfplane->PointOnSide(dl.origin)) continue;
          if (!RadiusCastRay(p, dl.origin, radius, false/*r_dynamic_clip_more*/)) continue;
        }
        if (dl.type == DLTYPE_Subtractive) add = -add;
        l += add;
        lr += add*((dl.colour>>16)&255)/255.0f;
        lg += add*((dl.colour>>8)&255)/255.0f;
        lb += add*(dl.colour&255)/255.0f;
      }
    }
  }

  /*
  if (l > 255) l = 255; else if (l < 0) l = 0;
  if (lr > 255) lr = 255; else if (lr < 0) lr = 0;
  if (lg > 255) lg = 255; else if (lg < 0) lg = 0;
  if (lb > 255) lb = 255; else if (lb < 0) lb = 0;
  return ((int)l<<24)|((int)lr<<16)|((int)lg<<8)|((int)lb);
  */

  return
    (((vuint32)clampToByte((int)l))<<24)|
    (((vuint32)clampToByte((int)lr))<<16)|
    (((vuint32)clampToByte((int)lg))<<8)|
    ((vuint32)clampToByte((int)lb));
}


//==========================================================================
//
//  VAdvancedRenderLevel::LightPointAmbient
//
//==========================================================================
vuint32 VAdvancedRenderLevel::LightPointAmbient (const TVec &p, float radius) {
  if (FixedLight) return FixedLight|(FixedLight<<8)|(FixedLight<<16)|(FixedLight<<24);

  subsector_t *sub = Level->PointInSubsector(p);
  subregion_t *reg = sub->regions;
  while (reg->next) {
    const float d = DotProduct(p, reg->floor->secplane->normal)-reg->floor->secplane->dist;
    if (d >= 0.0f) break;
    reg = reg->next;
  }

  float l;

  // region's base light
  if (r_allow_ambient) {
    l = reg->secregion->params->lightlevel+ExtraLight;
    l = MID(0.0f, l, 255.0f);
    if (r_darken) l = light_remap[(int)l];
    if (l < r_ambient) l = r_ambient;
    l = MID(0.0f, l, 255.0f);
  } else {
    l = 0;
  }

  int SecLightColour = reg->secregion->params->LightColour;
  float lr = ((SecLightColour>>16)&255)*l/255.0f;
  float lg = ((SecLightColour>>8)&255)*l/255.0f;
  float lb = (SecLightColour&255)*l/255.0f;

  return ((int)l<<24)|((int)lr<<16)|((int)lg<<8)|((int)lb);
}


//==========================================================================
//
//  VAdvancedRenderLevel::BuildLightMap
//
//==========================================================================
void VAdvancedRenderLevel::BuildLightMap (surface_t *surf) {
}


//==========================================================================
//
//  VAdvancedRenderLevel::DrawShadowSurfaces
//
//==========================================================================
void VAdvancedRenderLevel::DrawShadowSurfaces (surface_t *InSurfs, texinfo_t *texinfo,
                                               bool CheckSkyBoxAlways, int LightCanCross)
{
  if (!InSurfs) return;

  if (!texinfo || texinfo->Tex->Type == TEXTYPE_Null) return;
  if (texinfo->Alpha < 1.0f) return;
  if (LightCanCross > 0 && texinfo->Tex->isTransparent()) return; // has holes, don't bother

  // ignore everything that is placed behind player's back
  // we shouldn't have many of those, so check them in the loop below
  // but do this only if the light is behind a player
  bool checkFrustum;
  if (!r_advlight_opt_frustum_back) {
    checkFrustum = false;
  } else {
    checkFrustum = view_frustum.checkSphere(CurrLightPos, CurrLightRadius, TFrustum::NearBit);
  }

  // TODO: if light is behing a camera, we can move back frustum plane, so it will
  //       contain light origin, and clip everything behind it. the same can be done
  //       for all other frustum planes.

  for (surface_t *surf = InSurfs; surf; surf = surf->next) {
    if (surf->count < 3) continue; // just in case

    // for two-sided walls, we want to leave only one surface, otherwise z-fighting will occur
    // also, don't bother with it at all if texture has holes
    if (LightCanCross < 0) {
      // horizon
      // k8: can horizont surfaces block light? i think they shouldn't
      //if (surf->plane->PointOnSide(vieworg)) return; // viewer is in back side or on plane
      continue;
    }

    // leave only surface that light can see (it shouldn't matter for texturing which one we'll use)
    const float dist = DotProduct(CurrLightPos, surf->plane->normal)-surf->plane->dist;
    // k8: use `<=` and `>=` for radius checks, 'cause why not?
    //     light completely fades away at that distance
    if (dist <= 0.0f || dist >= CurrLightRadius) return; // light is too far away

    if (checkFrustum) {
      if (!view_frustum.checkVerts(surf->verts, (unsigned)surf->count, TFrustum::NearBit)) continue;
    }

    Drawer->RenderSurfaceShadowVolume(surf, CurrLightPos, CurrLightRadius);
  }
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderShadowLine
//
//  Clips the given segment and adds any visible pieces to the line list.
//
//==========================================================================
void VAdvancedRenderLevel::RenderShadowLine (sec_region_t *secregion, drawseg_t *dseg) {
  seg_t *seg = dseg->seg;
  if (!seg->linedef) return; // miniseg

  // note that we don't want to filter out shadows that are behind
  // but we are want to filter out surfaces that cannot possibly block light
  // (i.e. back-surfaces with respect to light origin)
  const float dist = DotProduct(CurrLightPos, seg->normal)-seg->dist;
  //if (dist < -CurrLightRadius || dist > CurrLightRadius) return; // light is too far away
  //if (fabsf(dist) >= CurrLightRadius) return;
  if (dist <= 0.0f || dist > CurrLightRadius) return;

/*
    k8: i don't know what Janis wanted to accomplish with this, but it actually
        makes clipping WORSE due to limited precision
  // clip sectors that are behind rendered segs
  TVec v1 = *seg->v1;
  TVec v2 = *seg->v2;
  const TVec r1 = CurrLightPos-v1;
  const TVec r2 = CurrLightPos-v2;
  const float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), CurrLightPos);
  const float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), CurrLightPos);

  // there might be a better method of doing this, but this one works for now...
       if (D1 > CurrLightRadius && D2 < -CurrLightRadius) v2 += (v2-v1)*D1/(D1-D2);
  else if (D2 > CurrLightRadius && D1 < -CurrLightRadius) v1 += (v1-v2)*D2/(D2-D1);

  if (!LightClip.IsRangeVisible(LightClip.PointToClipAngle(v2), LightClip.PointToClipAngle(v1))) return;
*/
  if (!LightClip.IsRangeVisible(*seg->v2, *seg->v1)) return;

  //line_t *linedef = seg->linedef;
  //side_t *sidedef = seg->sidedef;

  if (!seg->backsector) {
    // single sided line
    DrawShadowSurfaces(dseg->mid->surfs, &dseg->mid->texinfo, false, 0);
    DrawShadowSurfaces(dseg->topsky->surfs, &dseg->topsky->texinfo, false, 0);
  } else {
    // two sided line
    DrawShadowSurfaces(dseg->top->surfs, &dseg->top->texinfo, false, 0);
    DrawShadowSurfaces(dseg->topsky->surfs, &dseg->topsky->texinfo, false, -1);
    DrawShadowSurfaces(dseg->bot->surfs, &dseg->bot->texinfo, false, 0);
    DrawShadowSurfaces(dseg->mid->surfs, &dseg->mid->texinfo, false, 1);

    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      DrawShadowSurfaces(sp->surfs, &sp->texinfo, false, 0);
    }
  }
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderShadowSecSurface
//
//  this is used for floor and ceilings
//
//==========================================================================
void VAdvancedRenderLevel::RenderShadowSecSurface (sec_surface_t *ssurf, VEntity *SkyBox) {
  const sec_plane_t &plane = *ssurf->secplane;

  if (!plane.pic) return;

  // note that we don't want to filter out shadows that are behind
  // but we are want to filter out surfaces that cannot possibly block light
  // (i.e. back-surfaces with respect to light origin)
  const float dist = DotProduct(CurrLightPos, plane.normal)-plane.dist;
  //if (dist < -CurrLightRadius || dist > CurrLightRadius) return; // light is too far away
  //if (fabsf(dist) >= CurrLightRadius) return;
  if (dist <= 0.0f || dist > CurrLightRadius) return;

  DrawShadowSurfaces(ssurf->surfs, &ssurf->texinfo, true, 0);
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderShadowSubRegion
//
//  Determine floor/ceiling planes.
//  Draw one or more line segments.
//
//==========================================================================
void VAdvancedRenderLevel::RenderShadowSubRegion (subsector_t *sub, subregion_t *region) {
  const float dist = DotProduct(CurrLightPos, region->floor->secplane->normal)-region->floor->secplane->dist;

  if (region->next && dist <= -CurrLightRadius) {
    if (!LightClip.ClipLightCheckRegion(region->next, sub, CurrLightPos, CurrLightRadius)) return;
    RenderShadowSubRegion(sub, region->next);
  }

  sec_region_t *curreg = region->secregion;

  if (sub->poly && r_draw_pobj) {
    // render the polyobj in the subsector first
    seg_t **polySeg = sub->poly->segs;
    for (int count = sub->poly->numsegs; count--; ++polySeg) {
      RenderShadowLine(curreg, (*polySeg)->drawsegs);
    }
  }

  drawseg_t *ds = region->lines;
  for (int count = sub->numlines; count--; ++ds) {
    RenderShadowLine(curreg, ds);
  }

  RenderShadowSecSurface(region->floor, curreg->floor->SkyBox);
  RenderShadowSecSurface(region->ceil, curreg->ceiling->SkyBox);

  if (region->next && dist > CurrLightRadius) {
    if (!LightClip.ClipLightCheckRegion(region->next, sub, CurrLightPos, CurrLightRadius)) return;
    RenderShadowSubRegion(sub, region->next);
  }
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderShadowSubsector
//
//==========================================================================
void VAdvancedRenderLevel::RenderShadowSubsector (int num) {
  subsector_t *sub = &Level->Subsectors[num];

  // don't do this check for shadows
  //if (!(LightBspVis[num>>3]&(1<<(num&7))) || !(BspVis[num>>3]&(1<<(num&7)))) return;

  if (!sub->sector->linecount) return; // skip sectors containing original polyobjs

  if (!LightClip.ClipLightCheckSubsector(sub, CurrLightPos, CurrLightRadius)) return;

  // if our light is in frustum, out-of-frustum subsectors are not interesting
  //FIXME: pass "need frustum check" flag to other functions
  bool needToRender = true;
  if (CurrLightInFrustum && !(BspVis[num>>3]&(1u<<(num&7)))) {
    // this subsector is invisible, check if it is in frustum
    float bbox[6];
    // min
    bbox[0] = sub->bbox[0];
    bbox[1] = sub->bbox[1];
    bbox[2] = sub->sector->floor.minz;
    // max
    bbox[3] = sub->bbox[2];
    bbox[4] = sub->bbox[3];
    bbox[5] = sub->sector->ceiling.maxz;
    FixBBoxZ(bbox);
    needToRender = view_frustum.checkBox(bbox);
  }

  if (needToRender) RenderShadowSubRegion(sub, sub->regions);

  // add subsector's segs to the clipper
  // clipping against mirror is done only for vertical mirror planes
  LightClip.ClipLightAddSubsectorSegs(sub, CurrLightPos, CurrLightRadius);
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderShadowBSPNode
//
//  Renders all subsectors below a given node, traversing subtree
//  recursively. Just call with BSP root.
//
//==========================================================================
void VAdvancedRenderLevel::RenderShadowBSPNode (int bspnum, const float *bbox, bool LimitLights) {
  if (LimitLights) {
    if (r_max_shadow_segs_all >= 0 && AllShadowsNumber > r_max_shadow_segs_all) return;
    if (r_max_shadow_segs_one >= 0 && CurrShadowsNumber > r_max_shadow_segs_one) return;
  }

  if (LightClip.ClipIsFull()) return;

  if (!LightClip.ClipLightIsBBoxVisible(bbox, CurrLightPos, CurrLightRadius)) return;

  if (bspnum == -1) {
    RenderShadowSubsector(0);
    if (LimitLights) { ++CurrShadowsNumber; ++AllShadowsNumber; }
    return;
  }

  // found a subsector?
  if (!(bspnum&NF_SUBSECTOR)) {
    node_t *bsp = &Level->Nodes[bspnum];
    // decide which side the light is on
    const float dist = DotProduct(CurrLightPos, bsp->normal)-bsp->dist;
    if (dist > CurrLightRadius) {
      // light is completely on front side
      RenderShadowBSPNode(bsp->children[0], bsp->bbox[0], LimitLights);
    } else if (dist < -CurrLightRadius) {
      // light is completely on back side
      RenderShadowBSPNode(bsp->children[1], bsp->bbox[1], LimitLights);
    } else {
      int side = bsp->PointOnSide(CurrLightPos);
      // recursively divide front space
      RenderShadowBSPNode(bsp->children[side], bsp->bbox[side], LimitLights);
      // always divide back space for shadows
      RenderShadowBSPNode(bsp->children[side^1], bsp->bbox[side^1], LimitLights);
    }
  } else {
    RenderShadowSubsector(bspnum&(~NF_SUBSECTOR));
    if (LimitLights) { ++CurrShadowsNumber; ++AllShadowsNumber; }
  }
}


//==========================================================================
//
//  VAdvancedRenderLevel::DrawLightSurfaces
//
//==========================================================================
void VAdvancedRenderLevel::DrawLightSurfaces (surface_t *InSurfs, texinfo_t *texinfo,
                                              VEntity *SkyBox, bool CheckSkyBoxAlways, int LightCanCross)
{
  if (!InSurfs) return;

  if (!texinfo || texinfo->Tex->Type == TEXTYPE_Null) return;
  if (texinfo->Alpha < 1.0f) return;

  if (SkyBox && (SkyBox->EntityFlags&VEntity::EF_FixedModel)) SkyBox = nullptr;

  if (texinfo->Tex == GTextureManager.getIgnoreAnim(skyflatnum) ||
      (CheckSkyBoxAlways && (SkyBox && SkyBox->eventSkyBoxGetAlways())))
  {
    return;
  }

  for (surface_t *surf = InSurfs; surf; surf = surf->next) {
    if (surf->count < 3) continue; // just in case
    if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
    const float dist = DotProduct(CurrLightPos, surf->plane->normal)-surf->plane->dist;
    if (dist <= 0.0f || dist >= CurrLightRadius) continue; // light is too far away, or surface is not lit
    Drawer->DrawSurfaceLight(surf);
  }
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderLightLine
//
//  clips the given segment and adds any visible pieces to the line list
//
//==========================================================================
void VAdvancedRenderLevel::RenderLightLine (sec_region_t *secregion, drawseg_t *dseg) {
  const seg_t *seg = dseg->seg;

  if (!seg->linedef) return; // miniseg

  const float dist = DotProduct(CurrLightPos, seg->normal)-seg->dist;
  //if (dist <= -CurrLightRadius || dist > CurrLightRadius) return; // light sphere is not touching a plane
  if (fabsf(dist) >= CurrLightRadius) return;

/*
    k8: i don't know what Janis wanted to accomplish with this, but it actually
        makes clipping WORSE due to limited precision
  // clip sectors that are behind rendered segs
  TVec v1 = *seg->v1;
  TVec v2 = *seg->v2;
  TVec r1 = CurrLightPos-v1;
  TVec r2 = CurrLightPos-v2;
  float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), CurrLightPos);
  float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), CurrLightPos);

  // there might be a better method of doing this, but this one works for now...
       if (D1 > CurrLightRadius && D2 < -CurrLightRadius) v2 += (v2-v1)*D1/(D1-D2);
  else if (D2 > CurrLightRadius && D1 < -CurrLightRadius) v1 += (v1-v2)*D2/(D2-D1);

  if (!LightClip.IsRangeVisible(LightClip.PointToClipAngle(v2), LightClip.PointToClipAngle(v1))) return;
*/
  if (!LightClip.IsRangeVisible(*seg->v2, *seg->v1)) return;

  if (!seg->backsector) {
    // single sided line
    DrawLightSurfaces(dseg->mid->surfs, &dseg->mid->texinfo, secregion->ceiling->SkyBox, false, 0);
    DrawLightSurfaces(dseg->topsky->surfs, &dseg->topsky->texinfo, secregion->ceiling->SkyBox, false, 0);
  } else {
    // two sided line
    DrawLightSurfaces(dseg->top->surfs, &dseg->top->texinfo, secregion->ceiling->SkyBox, false, 0);
    DrawLightSurfaces(dseg->topsky->surfs, &dseg->topsky->texinfo, secregion->ceiling->SkyBox, false, -1);
    DrawLightSurfaces(dseg->bot->surfs, &dseg->bot->texinfo, secregion->ceiling->SkyBox, false, 0);
    DrawLightSurfaces(dseg->mid->surfs, &dseg->mid->texinfo, secregion->ceiling->SkyBox, false, 1);

    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      DrawLightSurfaces(sp->surfs, &sp->texinfo, secregion->ceiling->SkyBox, false, 0);
    }
  }
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderLightSecSurface
//
//  this is used for floor and ceilings
//
//==========================================================================
void VAdvancedRenderLevel::RenderLightSecSurface (sec_surface_t *ssurf, VEntity *SkyBox) {
  const sec_plane_t &plane = *ssurf->secplane;

  if (!plane.pic) return;

  const float dist = DotProduct(CurrLightPos, plane.normal)-plane.dist;
  //if (dist <= -CurrLightRadius || dist > CurrLightRadius) return; // light is in back side or on plane
  if (fabsf(dist) >= CurrLightRadius) return;

  DrawLightSurfaces(ssurf->surfs, &ssurf->texinfo, SkyBox, true, 0);
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderLightSubRegion
//
//  Determine floor/ceiling planes.
//  Draw one or more line segments.
//
//==========================================================================
void VAdvancedRenderLevel::RenderLightSubRegion (subsector_t *sub, subregion_t *region) {
  const float dist = DotProduct(CurrLightPos, region->floor->secplane->normal)-region->floor->secplane->dist;

  if (region->next && dist <= -CurrLightRadius) {
    if (!LightClip.ClipLightCheckRegion(region->next, sub, CurrLightPos, CurrLightRadius)) return;
    RenderLightSubRegion(sub, region->next);
  }

  sec_region_t *curreg = region->secregion;

  if (sub->poly && r_draw_pobj) {
    // render the polyobj in the subsector first
    seg_t **polySeg = sub->poly->segs;
    for (int count = sub->poly->numsegs; count--; ++polySeg) {
      RenderLightLine(curreg, (*polySeg)->drawsegs);
    }
  }

  int count = sub->numlines;
  drawseg_t *ds = region->lines;
  while (count--) {
    RenderLightLine(curreg, ds);
    ++ds;
  }

  RenderLightSecSurface(region->floor, curreg->floor->SkyBox);
  RenderLightSecSurface(region->ceil, curreg->ceiling->SkyBox);

  if (region->next && dist > CurrLightRadius) {
    if (!LightClip.ClipLightCheckRegion(region->next, sub, CurrLightPos, CurrLightRadius)) return;
    RenderLightSubRegion(sub, region->next);
  }
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderLightSubsector
//
//==========================================================================
void VAdvancedRenderLevel::RenderLightSubsector (int num) {
  subsector_t *sub = &Level->Subsectors[num];

  if (!sub->sector->linecount) return; // skip sectors containing original polyobjs

  // `LightBspVis` is already an intersection, no need to check `BspVis` here
  //if (!(LightBspVis[num>>3]&(1<<(num&7))) || !(BspVis[num>>3]&(1<<(num&7)))) return;
  if (!(LightBspVis[(unsigned)num>>3]&(1<<((unsigned)num&7)))) return;

  if (!LightClip.ClipLightCheckSubsector(sub, CurrLightPos, CurrLightRadius)) return;

  RenderLightSubRegion(sub, sub->regions);

  // add subsector's segs to the clipper
  // clipping against mirror is done only for vertical mirror planes
  LightClip.ClipLightAddSubsectorSegs(sub, CurrLightPos, CurrLightRadius);
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderLightBSPNode
//
//  Renders all subsectors below a given node, traversing subtree
//  recursively. Just call with BSP root.
//
//==========================================================================
void VAdvancedRenderLevel::RenderLightBSPNode (int bspnum, const float *bbox, bool LimitLights) {
  if (LimitLights) {
     if (r_max_light_segs_all >= 0 && AllLightsNumber > r_max_light_segs_all) return;
     if (r_max_light_segs_one >= 0 && CurrLightsNumber > r_max_light_segs_one) return;
  }

  if (LightClip.ClipIsFull()) return;

  if (!LightClip.ClipLightIsBBoxVisible(bbox, CurrLightPos, CurrLightRadius)) return;

  if (bspnum == -1) {
    RenderLightSubsector(0);
    if (LimitLights) { ++CurrLightsNumber; ++AllLightsNumber; }
    return;
  }

  // found a subsector?
  if (!(bspnum&NF_SUBSECTOR)) {
    node_t *bsp = &Level->Nodes[bspnum];

    // decide which side the light is on
    const float dist = DotProduct(CurrLightPos, bsp->normal)-bsp->dist;
    if (dist > CurrLightRadius) {
      // light is completely on front side
      RenderLightBSPNode(bsp->children[0], bsp->bbox[0], LimitLights);
    } else if (dist < -CurrLightRadius) {
      // light is completely on back side
      RenderLightBSPNode(bsp->children[1], bsp->bbox[1], LimitLights);
    } else {
      int side = bsp->PointOnSide(CurrLightPos);
      // recursively divide front space
      RenderLightBSPNode(bsp->children[side], bsp->bbox[side], LimitLights);
      // possibly divide back space
      if (LightClip.ClipLightIsBBoxVisible(bsp->bbox[side^1], CurrLightPos, CurrLightRadius)) {
        RenderLightBSPNode(bsp->children[side^1], bsp->bbox[side^1], LimitLights);
      }
    }
  } else {
    RenderLightSubsector(bspnum&(~NF_SUBSECTOR));
    if (LimitLights) { ++CurrLightsNumber; ++AllLightsNumber; }
  }
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderLightShadows
//
//==========================================================================
void VAdvancedRenderLevel::RenderLightShadows (const refdef_t *RD, const VViewClipper *Range,
                                               TVec &Pos, float Radius, vuint32 Colour, bool LimitLights)
{
  if (r_max_lights >= 0 && LightsRendered >= r_max_lights) return;

  if (!CalcLightVis(Pos, Radius)) return;

  if (r_advlight_opt_optimise_scissor && !LitSurfaces) return; // no lit surfaces, nothing to do
  if (LightVisSubs.length() == 0) return; // just in case

  CurrLightColour = Colour;
  // if our light is in frustum, ignore any out-of-frustum polys
  if (r_advlight_opt_frustum_full) {
    CurrLightInFrustum = view_frustum.checkSphere(Pos, Radius+4.0f);
  } else {
    CurrLightInFrustum = false; // don't do frustum optimisations
  }

  // if we want model shadows, always do full rendering
  if (!doShadows && r_draw_mobjs && r_models && r_model_shadows) doShadows = true;

  if (!doShadows && dbg_adv_light_notrace_mark) {
    //Colour = 0xffff0000U;
    Colour = 0xffff00ffU; // purple; it should be very noticeable
  }

  ++LightsRendered;

  CurrShadowsNumber = 0;
  CurrLightsNumber = 0;

  //  0 if scissor is empty
  // -1 if scissor has no sense (should not be used)
  //  1 if scissor is set
  int hasScissor = 1;
  int scoord[4];

  //GCon->Logf("LBB:(%f,%f,%f)-(%f,%f,%f)", LitBBox[0], LitBBox[1], LitBBox[2], LitBBox[3], LitBBox[4], LitBBox[5]);

  // setup light scissor rectangle
  if (r_advlight_opt_scissor) {
    hasScissor = Drawer->SetupLightScissor(Pos, Radius, scoord, (r_advlight_opt_optimise_scissor && !r_model_shadows ? LitBBox : nullptr));
    if (hasScissor <= 0) {
      // something is VERY wrong (-1), or scissor is empty (0)
      Drawer->ResetScissor();
      if (!hasScissor) return; // empty scissor
      hasScissor = 0;
      scoord[0] = scoord[1] = 0;
      scoord[2] = ScreenWidth;
      scoord[3] = ScreenHeight;
    } else {
      if (scoord[0] == 0 && scoord[1] == 0 && scoord[2] == ScreenWidth && scoord[3] == ScreenHeight) {
        hasScissor = 0;
      }
    }
  }

  // if there are no lit surfaces oriented away from camera, it cannot possibly be in shadow volume
  // nope, it is wrong
  //bool useZPass = !HasBackLit;
  //if (useZPass) GCon->Log("*** ZPASS");
  //useZPass = false;
  bool useZPass = false;

  /* nope
  if ((CurrLightPos-vieworg).lengthSquared() > CurrLightRadius*CurrLightRadius) {
    useZPass = true;
  }
  */

#if 0
  float dummy_bbox[6] = { -99999, -99999, -99999, 99999, 99999, 99999 };
#endif

  ResetMobjsLightCount(true);
  // do shadow volumes
  Drawer->BeginLightShadowVolumes(CurrLightPos, CurrLightRadius, useZPass, hasScissor, scoord);
  LightClip.ClearClipNodes(CurrLightPos, Level);
  if (doShadows && r_max_shadow_segs_all) {
#if 0
    RenderShadowBSPNode(Level->NumNodes-1, dummy_bbox, LimitLights);
#else
    {
      const int *subidx = LightSubs.ptr();
      for (int sscount = LightSubs.length(); sscount--; ++subidx) {
        RenderShadowSubsector(*subidx);
      }
    }
#endif
    Drawer->BeginModelsShadowsPass(CurrLightPos, CurrLightRadius);
    RenderMobjsShadow();
    ResetMobjsLightCount(false);
  }
  Drawer->EndLightShadowVolumes();

  // k8: the question is: why we are rendering surfaces instead
  //     of simply render a light circle? shadow volumes should
  //     take care of masking the area, so simply rendering a
  //     circle should do the trick.

  // draw light
  Drawer->BeginLightPass(CurrLightPos, CurrLightRadius, Colour, doShadows);
  LightClip.ClearClipNodes(CurrLightPos, Level);
#if 0
  RenderLightBSPNode(Level->NumNodes-1, dummy_bbox, LimitLights);
#else
  {
    const int *subidx = LightVisSubs.ptr();
    for (int sscount = LightVisSubs.length(); sscount--; ++subidx) {
      RenderLightSubsector(*subidx);
    }
  }
#endif
  Drawer->BeginModelsLightPass(CurrLightPos, CurrLightRadius, Colour);
  RenderMobjsLight();
  ResetMobjsLightCount(false);

  //if (hasScissor) Drawer->DebugRenderScreenRect(scoord[0], scoord[1], scoord[2], scoord[3], 0x7f007f00);

  /*if (hasScissor)*/ Drawer->ResetScissor();
}
