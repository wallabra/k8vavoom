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


extern VCvarB r_darken;
extern VCvarB r_draw_mobjs;
extern VCvarB r_model_shadows;
extern VCvarB r_draw_pobj;
extern VCvarB r_chasecam;
extern VCvarB r_glow_flat;
extern VCvarB clip_use_1d_clipper;
extern VCvarB r_disable_world_update;

extern VCvarB gl_dbg_wireframe;

static VCvarB clip_adv_regions_shadow("clip_adv_regions_shadow", false, "Clip (1D) shadow regions?", CVAR_PreInit);
static VCvarB clip_adv_regions_light("clip_adv_regions_light", false, "Clip (1D) light regions?", CVAR_PreInit);

static VCvarB r_shadowvol_use_pofs("r_shadowvol_use_pofs", true, "Use PolygonOffset for shadow volumes to reduce some flickering (WARNING: BUGGY!)?", CVAR_Archive);
static VCvarF r_shadowvol_pofs("r_shadowvol_pofs", "20", "DEBUG");
static VCvarF r_shadowvol_pslope("r_shadowvol_pslope", "-0.2", "DEBUG");

static VCvarB r_shadowvol_optimise_flats("r_shadowvol_optimise_flats", true, "Drop some floors/ceilings that can't possibly cast shadow?", CVAR_Archive);
#ifdef VV_CHECK_1S_CAST_SHADOW
static VCvarB r_shadowvol_optimise_lines_1s("r_shadowvol_optimise_lines_1s", true, "Drop some 1s walls that can't possibly cast shadow? (glitchy)");
#endif


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


// ////////////////////////////////////////////////////////////////////////// //
// private data definitions
// ////////////////////////////////////////////////////////////////////////// //

//VCvarI r_max_model_lights("r_max_model_lights", "32", "Maximum lights that can affect one model when we aren't using model shadows.", CVAR_Archive);
VCvarI r_max_model_shadows("r_max_model_shadows", "16", "Maximum number of shadows one model can cast.", CVAR_Archive);

VCvarI r_max_lights("r_max_lights", "256", "Total maximum lights for shadow volume renderer.", CVAR_Archive);
VCvarI r_dynlight_minimum("r_dynlight_minimum", "6", "Render at least this number of dynamic lights, regardless of total limit.", CVAR_Archive);

static VCvarI r_max_light_segs_all("r_max_light_segs_all", "-1", "Maximum light segments for all lights.", CVAR_Archive);
static VCvarI r_max_light_segs_one("r_max_light_segs_one", "-1", "Maximum light segments for one light.", CVAR_Archive);
// was 128, but with scissored light, there is no sense to limit it anymore
static VCvarI r_max_shadow_segs_all("r_max_shadow_segs_all", "-1", "Maximum shadow segments for all lights.", CVAR_Archive);
static VCvarI r_max_shadow_segs_one("r_max_shadow_segs_one", "-1", "Maximum shadow segments for one light.", CVAR_Archive);

VCvarF r_light_filter_static_coeff("r_light_filter_static_coeff", "0.2", "How close static lights should be to be filtered out?\n(0.1-0.3 is usually ok).", CVAR_Archive);
VCvarB r_allow_static_light_filter("r_allow_static_light_filter", true, "Allow filtering of static lights?", CVAR_Archive);
VCvarI r_static_light_filter_mode("r_static_light_filter_mode", "0", "Filter only decorations(0), or all lights(1)?", CVAR_Archive);

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
//  ClipSegToLight
//
//  this (theoretically) should clip segment to light bounds
//  tbh, i don't think that there is a real reason to do this
//
//==========================================================================
static VVA_OKUNUSED inline void ClipSegToLight (TVec &v1, TVec &v2, const TVec &pos, const float radius) {
  const TVec r1 = pos-v1;
  const TVec r2 = pos-v2;
  const float d1 = DotProduct(Normalise(CrossProduct(r1, r2)), pos);
  const float d2 = DotProduct(Normalise(CrossProduct(r2, r1)), pos);
  // there might be a better method of doing this, but this one works for now...
       if (d1 > radius && d2 < -radius) v2 += (v2-v1)*d1/(d1-d2);
  else if (d2 > radius && d1 < -radius) v1 += (v1-v2)*d2/(d2-d1);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RefilterStaticLights
//
//==========================================================================
void VRenderLevelShadowVolume::RefilterStaticLights () {
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

  if (!r_allow_static_light_filter) return; // no filtering
  if (coeff <= 0) return; // no filtering
  if (coeff > 8) coeff = 8;

  const bool onlyDecor = (r_static_light_filter_mode.asInt() == 0);

  for (int currlidx = 0; currlidx < llen; ++currlidx) {
    light_t &cl = Lights[currlidx];
    if (!cl.active) continue; // already filtered out
    if (onlyDecor && !cl.ownerUId) continue;
    // remove nearby lights with radius less than ours (or ourself if we'll hit bigger light)
    float radsq = (cl.radius*cl.radius)*coeff;
    for (int nlidx = currlidx+1; nlidx < llen; ++nlidx) {
      light_t &nl = Lights[nlidx];
      if (!nl.active) continue; // already filtered out
      if (onlyDecor && !nl.ownerUId) continue;
      const float distsq = length2DSquared(cl.origin-nl.origin);
      if (distsq >= radsq) continue;

      // check potential visibility
      /*
      subsector_t *sub = Level->PointInSubsector(nl.origin);
      const vuint8 *dyn_facevis = Level->LeafPVS(sub);
      if (!(dyn_facevis[nl.leafnum>>3]&(1<<(nl.leafnum&7)))) continue;
      */

      // if we cannot trace a line between two lights, they are prolly divided by a wall or a flat
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
    if (cl.active) {
      ++actlights;
    } else {
      //if (cl.owner) GCon->Logf(NAME_Debug, "ADVR: filtered static light from `%s`; org=(%g,%g,%g); radius=%g", cl.owner->GetClass()->GetName(), cl.origin.x, cl.origin.y, cl.origin.z, cl.radius);
    }
    CalcStaticLightTouchingSubs(currlidx, cl);
  }

  if (actlights < llen) GCon->Logf("filtered %d static lights out of %d (%d left)", llen-actlights, llen, actlights);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::BuildLightMap
//
//==========================================================================
void VRenderLevelShadowVolume::BuildLightMap (surface_t *surf) {
}


//==========================================================================
//
//  VRenderLevelShadowVolume::DrawShadowSurfaces
//
//  LightCanCross:
//    -1: two-sided line, top or bottom
//     0: one-sided line, or flat
//     1: two-sided line, middle
//
//==========================================================================
void VRenderLevelShadowVolume::DrawShadowSurfaces (surface_t *InSurfs, texinfo_t *texinfo,
                                                   VEntity *SkyBox, bool CheckSkyBoxAlways, int LightCanCross)
{
  if (!InSurfs) return;

  if (!texinfo || texinfo->Tex->Type == TEXTYPE_Null) return;
  if (texinfo->Alpha < 1.0f || texinfo->Additive) return;
  if (LightCanCross > 0 && (!r_shadowmaps.asBool() && texinfo->Tex->isSeeThrough())) return; // has holes, don't bother

  if (SkyBox && SkyBox->IsPortalDirty()) SkyBox = nullptr;

  if (texinfo->Tex == GTextureManager.getIgnoreAnim(skyflatnum) ||
      (CheckSkyBoxAlways && (SkyBox && SkyBox->GetSkyBoxAlways())))
  {
    return;
  }

  // ignore everything that is placed behind camera's back
  // we shouldn't have many of those, so check them in the loop below
  // but do this only if the light is in front of a camera
  //const bool checkFrustum = (r_advlight_opt_frustum_back && Drawer->viewfrustum.checkSphere(CurrLightPos, CurrLightRadius, TFrustum::NearBit));

  if (r_shadowmaps) {
    for (surface_t *surf = InSurfs; surf; surf = surf->next) {
      if (surf->count < 3) continue; // just in case

      // floor or ceiling? ignore masked
      if (LightCanCross < 0 || surf->GetNormalZ()) {
        VTexture *tex = surf->texinfo->Tex;
        if (!tex || tex->Type == TEXTYPE_Null) continue;
        if (surf->texinfo->Alpha < 1.0f || surf->texinfo->Additive) continue;
      }

      // leave only surface that light can see (it shouldn't matter for texturing which one we'll use)
      const float dist = DotProduct(CurrLightPos, surf->GetNormal())-surf->GetDist();
      // k8: use `<=` and `>=` for radius checks, 'cause why not?
      //     light completely fades away at that distance
      if (dist <= 0.0f || dist >= CurrLightRadius) return; // light is too far away

      Drawer->RenderSurfaceShadowMap(surf, CurrLightPos, CurrLightRadius);
    }
  } else {
    // TODO: if light is behind a camera, we can move back frustum plane, so it will
    //       contain light origin, and clip everything behind it. the same can be done
    //       for all other frustum planes.
    for (surface_t *surf = InSurfs; surf; surf = surf->next) {
      if (surf->count < 3) continue; // just in case

      // check transdoor hacks
      //if (surf->drawflags&surface_t::TF_TOPHACK) continue;

      // floor or ceiling? ignore masked
      if (LightCanCross < 0 || surf->GetNormalZ()) {
        VTexture *tex = surf->texinfo->Tex;
        if (!tex || tex->Type == TEXTYPE_Null) continue;
        if (surf->texinfo->Alpha < 1.0f || surf->texinfo->Additive) continue;
        if (tex->isSeeThrough()) continue; // this is masked texture
      }

      // leave only surface that light can see (it shouldn't matter for texturing which one we'll use)
      const float dist = DotProduct(CurrLightPos, surf->GetNormal())-surf->GetDist();
      // k8: use `<=` and `>=` for radius checks, 'cause why not?
      //     light completely fades away at that distance
      if (dist <= 0.0f || dist >= CurrLightRadius) return; // light is too far away

      /*
      if (checkFrustum) {
        if (!Drawer->viewfrustum.checkVerts(surf->verts, (unsigned)surf->count, TFrustum::NearBit)) continue;
      }
      */

      Drawer->RenderSurfaceShadowVolume(surf, CurrLightPos, CurrLightRadius);
    }
  }
}


#ifdef VV_CHECK_1S_CAST_SHADOW
//==========================================================================
//
//  VRenderLevelShadowVolume::CheckCan1SCastShadow
//
//==========================================================================
bool VRenderLevelShadowVolume::CheckCan1SCastShadow (line_t *line) {
  if (!r_shadowvol_optimise_lines_1s) return true;
  const int lidx = (int)(ptrdiff_t)(line-&Level->Lines[0]);
  Line1SShadowInfo &nfo = flineCheck[lidx];
  if (nfo.frametag != fsecCounter) {
    nfo.frametag = fsecCounter; // mark as processed
    // check if all adjacent walls to this one are in front of it
    // as this is one-sided wall, we can assume that if everything is in front,
    // there is no passage that we can cast shadow into

    // check lines at v1
    line_t **llist = line->v1lines;
    for (int f = line->v1linesCount; f--; ++llist) {
      line_t *l2 = *llist;
      if ((l2->flags&ML_TWOSIDED) != 0) return (nfo.canShadow = true); // has two-sided line as a neighbour, oops
      TVec v = (*l2->v1 == *line->v1 ? *l2->v2 : *l2->v1);
      const int side = line->PointOnSide2(v);
      if (side == 1) return (nfo.canShadow = true); // this point is behind, can cast a shadow
      // perform recursive check for coplanar lines
      if (side == 2) {
        // check if the light can touch it
        if (fabsf(l2->PointDistance(CurrLightPos)) < CurrLightRadius) {
          if (CheckCan1SCastShadow(l2)) return (nfo.canShadow = true); // there is a turn, oops
        }
      }
    }

    // check lines at v2
    llist = line->v2lines;
    for (int f = line->v2linesCount; f--; ++llist) {
      line_t *l2 = *llist;
      if ((l2->flags&ML_TWOSIDED) != 0) return (nfo.canShadow = true); // has two-sided line as a neighbour, oops
      TVec v = (*l2->v1 == *line->v2 ? *l2->v2 : *l2->v1);
      const int side = line->PointOnSide2(v);
      if (side == 1) return (nfo.canShadow = true); // this point is behind, can cast a shadow
      // perform recursive check for coplanar lines
      if (side == 2) {
        // check if the light can touch it
        if (fabsf(l2->PointDistance(CurrLightPos)) < CurrLightRadius) {
          if (CheckCan1SCastShadow(l2)) return (nfo.canShadow = true); // there is a turn, oops
        }
      }
    }

    // no vertices are on the back side, shadow casting is impossible
    nfo.canShadow = false;
  }
  return nfo.canShadow;
}
#endif


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderShadowLine
//
//  Clips the given segment and adds any visible pieces to the line list.
//
//==========================================================================
void VRenderLevelShadowVolume::RenderShadowLine (subsector_t *sub, sec_region_t *secregion, drawseg_t *dseg) {
  seg_t *seg = dseg->seg;
  if (!seg->linedef) return; // miniseg

  // note that we don't want to filter out shadows that are behind
  // but we are want to filter out surfaces that cannot possibly block light
  // (i.e. back-surfaces with respect to light origin)
  const float dist = DotProduct(CurrLightPos, seg->normal)-seg->dist;
  //if (dist < -CurrLightRadius || dist > CurrLightRadius) return; // light is too far away
  //if (fabsf(dist) >= CurrLightRadius) return;
  if (dist <= 0.0f || dist >= CurrLightRadius) return;

  //k8: here we can call `ClipSegToLight()`, but i see no reasons to do so

  // ignore everything that is placed behind camera's back
  // we shouldn't have many of those, so check them in the loop below
  // but do this only if the light is in front of a camera
  if (CurrLightInFront) {
    if (!Drawer->viewfrustum.checkPoint(*seg->v1, TFrustum::NearBit) &&
        !Drawer->viewfrustum.checkPoint(*seg->v2, TFrustum::NearBit))
    {
      return;
    }
  }

  if (!LightClip.IsRangeVisible(*seg->v2, *seg->v1)) return;

  #ifdef VV_CHECK_1S_CAST_SHADOW
  if (!seg->backsector && !CheckCan1SCastShadow(seg->linedef)) {
    return;
  }
  #endif

#if 1
  // k8: this drops some segs that may leak without proper frustum culling
  // k8: this seems to be unnecessary now
  // k8: yet leave it there in the hope that it will reduce GPU overdrawing
  if (!LightClip.CheckSegFrustum(sub, seg)) return;
#endif

  VEntity *skybox = secregion->eceiling.splane->SkyBox;
  if (dseg->mid) DrawShadowSurfaces(dseg->mid->surfs, &dseg->mid->texinfo, skybox, false, (seg->backsector ? 1 : 0));
  if (seg->backsector) {
    // two sided line
    if (dseg->top) DrawShadowSurfaces(dseg->top->surfs, &dseg->top->texinfo, skybox, false, (seg->backsector ? -1 : 0));
    //k8: horizon/sky cannot block light
    //if (dseg->topsky) DrawShadowSurfaces(dseg->topsky->surfs, &dseg->topsky->texinfo, skybox, false, -1);
    if (dseg->bot) DrawShadowSurfaces(dseg->bot->surfs, &dseg->bot->texinfo, skybox, false, (seg->backsector ? -1 : 0));
    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      DrawShadowSurfaces(sp->surfs, &sp->texinfo, skybox, false, (seg->backsector ? -1 : 0));
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderShadowSecSurface
//
//  this is used for floor and ceilings
//
//==========================================================================
void VRenderLevelShadowVolume::RenderShadowSecSurface (sec_surface_t *ssurf, VEntity *SkyBox) {
  //const sec_plane_t &plane = *ssurf->secplane;
  if (!ssurf->esecplane.splane->pic) return;

  // note that we don't want to filter out shadows that are behind
  // but we are want to filter out surfaces that cannot possibly block light
  // (i.e. back-surfaces with respect to light origin)
  //const float dist = DotProduct(CurrLightPos, plane.normal)-plane.dist;
  const float dist = ssurf->PointDist(CurrLightPos);
  //if (dist < -CurrLightRadius || dist > CurrLightRadius) return; // light is too far away
  //if (fabsf(dist) >= CurrLightRadius) return;
  if (dist <= 0.0f || dist >= CurrLightRadius) return;

  // we can do "CurrLightInFront check" here, but meh...

  DrawShadowSurfaces(ssurf->surfs, &ssurf->texinfo, SkyBox, true, 0);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::AddPolyObjToLightClipper
//
//  we have to do this separately, because for now we have to add
//  invisible segs to clipper too
//  i don't yet know why
//
//==========================================================================
void VRenderLevelShadowVolume::AddPolyObjToLightClipper (VViewClipper &clip, subsector_t *sub, bool asShadow) {
  if (sub && sub->HasPObjs() && r_draw_pobj && clip_use_1d_clipper) {
    for (auto &&it : sub->PObjFirst()) {
      polyobj_t *pobj = it.value();
      seg_t **polySeg = pobj->segs;
      for (int polyCount = pobj->numsegs; polyCount--; ++polySeg) {
        seg_t *seg = (*polySeg)->drawsegs->seg;
        if (seg->linedef) {
          clip.CheckAddClipSeg(seg, nullptr/*mirror*/, asShadow);
        }
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderShadowPolyObj
//
//==========================================================================
void VRenderLevelShadowVolume::RenderShadowPolyObj (subsector_t *sub) {
  if (sub && sub->HasPObjs() && r_draw_pobj) {
    subregion_t *region = sub->regions;
    sec_region_t *secregion = region->secregion;
    for (auto &&it : sub->PObjFirst()) {
      polyobj_t *pobj = it.value();
      seg_t **polySeg = pobj->segs;
      for (int polyCount = pobj->numsegs; polyCount--; ++polySeg) {
        RenderShadowLine(sub, secregion, (*polySeg)->drawsegs);
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::CheckShadowingFlats
//
//==========================================================================
unsigned VRenderLevelShadowVolume::CheckShadowingFlats (subsector_t *sub) {
  //if (floorz > ceilingz) return 0;
  sector_t *sector = sub->sector; // our main sector
  int sidx = (int)(ptrdiff_t)(sector-&Level->Sectors[0]);
  FlatSectorShadowInfo &nfo = fsecCheck[sidx];
  // check if we need to calculate info
  if (nfo.frametag != fsecCounter) {
    // yeah, calculate it
    nfo.frametag = fsecCounter; // mark as updated
    unsigned allowed = 0u; // set bits means "allowed"
    /*
    if (CurrLightPos.z > sector->floor.minz && CurrLightPos.z-CurrLightRadius < sector->floor.maxz) allowed |= FlatSectorShadowInfo::NoFloor; // too high or too low
    if (CurrLightPos.z < sector->ceiling.maxz && CurrLightPos.z+CurrLightRadius > sector->ceiling.minz) allowed |= FlatSectorShadowInfo::NoCeiling; // too high or too low
    */
    float dist = sector->floor.PointDistance(CurrLightPos);
    if (dist > 0.0f && dist < CurrLightRadius) allowed |= FlatSectorShadowInfo::NoFloor; // light can touch
    dist = sector->ceiling.PointDistance(CurrLightPos);
    if (dist > 0.0f && dist < CurrLightRadius) allowed |= FlatSectorShadowInfo::NoCeiling; // light can touch
    //allowed = (FlatSectorShadowInfo::NoFloor|FlatSectorShadowInfo::NoCeiling);
    if (!r_shadowvol_optimise_flats) {
      // no checks, return inverted `allowed`
      return (nfo.renderFlag = allowed^(FlatSectorShadowInfo::NoFloor|FlatSectorShadowInfo::NoCeiling));
    }
    if (!allowed) {
      // nothing is allowed, oops
      return (nfo.renderFlag = (FlatSectorShadowInfo::NoFloor|FlatSectorShadowInfo::NoCeiling));
    }
    // check all 2s walls of this sector
    // note that polyobjects are not interested, because they're always as high as their sector
    // TODO: optimise this by checking walls only once?
    // calculate blockmap coordinates
    bool checkFloor = (allowed&FlatSectorShadowInfo::NoFloor);
    bool checkCeiling = (allowed&FlatSectorShadowInfo::NoCeiling);
    bool renderFloor = false;
    bool renderCeiling = false;
    // check blockmap
    const int lpx = (int)CurrLightPos.x;
    const int lpy = (int)CurrLightPos.y;
    const int lrad = (int)CurrLightRadius;
    const int bmapx0 = (int)Level->BlockMapOrgX;
    const int bmapy0 = (int)Level->BlockMapOrgY;
    const int bmapw = (int)Level->BlockMapWidth;
    const int bmaph = (int)Level->BlockMapHeight;
    int bmx0 = (lpx-lrad-bmapx0)/MAPBLOCKUNITS;
    int bmy0 = (lpy-lrad-bmapy0)/MAPBLOCKUNITS;
    int bmx1 = (lpx+lrad-bmapx0)/MAPBLOCKUNITS;
    int bmy1 = (lpy+lrad-bmapy0)/MAPBLOCKUNITS;
    // check if we're inside a blockmap
    if (bmx1 < 0 || bmy1 < 0 || bmx0 >= bmapw || bmy0 >= bmaph) {
      // nothing is allowed, oops
      return (nfo.renderFlag = (FlatSectorShadowInfo::NoFloor|FlatSectorShadowInfo::NoCeiling));
    }
    // at least partially inside, perform checks
    if (bmx0 < 0) bmx0 = 0;
    if (bmy0 < 0) bmy0 = 0;
    if (bmx1 >= bmapw) bmx1 = bmapw-1;
    if (bmy1 >= bmaph) bmy1 = bmaph-1;
    const unsigned secSeenTag = fsecSeenSectorsGen();
    for (int by = bmy0; by <= bmy1; ++by) {
      for (int bx = bmx0; bx <= bmx1; ++bx) {
        int offset = by*bmapw+bx;
        offset = *(Level->BlockMap+offset);
        for (const vint32 *list = Level->BlockMapLump+offset+1; *list != -1; ++list) {
          line_t *l = &Level->Lines[*list];
          // ignore one-sided lines
          if ((l->flags&ML_TWOSIDED) == 0) continue;
          // ignore lines that don't touch our sector
          if (l->frontsector != sector && l->backsector != sector) continue;
          // get other sector
          sector_t *other = (l->frontsector == sector ? l->backsector : l->frontsector);
          if (!other) continue; // just in case
          const int othersecidx = (int)(ptrdiff_t)(other-&Level->Sectors[0]);
          if (fsecSeenSectors[othersecidx] == secSeenTag) continue; // already checked
          // ignore lines that cannot be lit
          if (fabsf(l->PointDistance(CurrLightPos)) >= CurrLightRadius) continue;
          // mark as checked
          fsecSeenSectors[othersecidx] = secSeenTag;
          // check other sector floor
          if (checkFloor && other->floor.minz < sector->floor.maxz) {
            // other sector floor is lower, cannot drop our
            renderFloor = true;
            if (renderCeiling || !checkCeiling) goto lhackdone; // no need to perform any more checks
            checkFloor = false;
          }
          // check other sector ceiling
          if (checkCeiling && other->ceiling.maxz > sector->ceiling.minz) {
            // other sector ceiling is higher, cannot drop our
            renderCeiling = true;
            if (renderFloor || !checkFloor) goto lhackdone; // no need to perform any more checks
            checkCeiling = false;
          }
        }
      }
    }
    lhackdone:
    // set flag
    if ((allowed&FlatSectorShadowInfo::NoFloor) == 0) renderFloor = false;
    if ((allowed&FlatSectorShadowInfo::NoCeiling) == 0) renderCeiling = false;
    nfo.renderFlag =
      (renderFloor ? 0u : FlatSectorShadowInfo::NoFloor)|
      (renderCeiling ? 0u : FlatSectorShadowInfo::NoCeiling);
  }
  return nfo.renderFlag;
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderShadowSubRegion
//
//  Determine floor/ceiling planes.
//  Draw one or more line segments.
//
//==========================================================================
void VRenderLevelShadowVolume::RenderShadowSubRegion (subsector_t *sub, subregion_t *region) {
  // no reason to sort flats here
  //const bool nextFirst = NeedToRenderNextSubFirst(region);
  //if (nextFirst) RenderShadowSubRegion(sub, region->next);

  /*
    note that we can throw away main floors and ceilings (i.e. for the base region),
    but only if this subsector either doesn't have any lines (i.e. consists purely of minisegs),
    nope: not any such subregion; just avoid checking neighbouring sectors if shared line
          cannot be touched by the light
    or:
      drop floor if all neighbour floors are higher or equal (we cannot cast any shadow to them),
      drop ceiling if all neighbour ceilings are lower or equal (we cannot cast any shadow to them)

    this is done by `CheckShadowingFlats()`
   */

  for (; region; region = region->next) {
    sec_region_t *secregion = region->secregion;

    if (!clip_adv_regions_shadow || LightClip.ClipLightCheckRegion(region, sub, true)) {
      drawseg_t *ds = region->lines;
      for (int count = sub->numlines; count--; ++ds) RenderShadowLine(sub, secregion, ds);
    }

    {
      sec_surface_t *fsurf[4];
      GetFlatSetToRender(sub, region, fsurf);

      // skip sectors with height transfer for now
      if ((region->secregion->regflags&sec_region_t::RF_BaseRegion) && !sub->sector->heightsec) {
        unsigned disableflag = CheckShadowingFlats(sub);
        if (disableflag&FlatSectorShadowInfo::NoFloor) {
          //GCon->Logf(NAME_Debug, "dropping floor for sector #%d", (int)(ptrdiff_t)(sub->sector-&Level->Sectors[0]));
          fsurf[0] = fsurf[1] = nullptr;
        }
        if (disableflag&FlatSectorShadowInfo::NoCeiling) {
          //GCon->Logf(NAME_Debug, "dropping ceiling for sector #%d", (int)(ptrdiff_t)(sub->sector-&Level->Sectors[0]));
          fsurf[2] = fsurf[3] = nullptr;
        }
      }

      if (fsurf[0]) RenderShadowSecSurface(fsurf[0], secregion->efloor.splane->SkyBox);
      if (fsurf[1]) RenderShadowSecSurface(fsurf[1], secregion->efloor.splane->SkyBox);

      if (fsurf[2]) RenderShadowSecSurface(fsurf[2], secregion->/*efloor*/eceiling.splane->SkyBox);
      if (fsurf[3]) RenderShadowSecSurface(fsurf[3], secregion->eceiling.splane->SkyBox);
    }
  }

  //if (!nextFirst && region->next) return RenderShadowSubRegion(sub, region->next);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderShadowSubsector
//
//==========================================================================
void VRenderLevelShadowVolume::RenderShadowSubsector (int num) {
  subsector_t *sub = &Level->Subsectors[num];

  // don't do this check for shadows
  //if (!IsSubsectorLitBspVis(num) || !(BspVis[num>>3]&(1<<(num&7)))) return;

  if (!sub->sector->linecount) return; // skip sectors containing original polyobjs

  if (LightClip.ClipLightCheckSubsector(sub, true)) {
    // if our light is in frustum, out-of-frustum subsectors are not interesting
    //FIXME: pass "need frustum check" flag to other functions
    bool needToRender = true;
    if (CurrLightInFrustum && !(BspVis[num>>3]&(1u<<(num&7)))) {
      // this subsector is invisible, check if it is in frustum
      float bbox[6];
      // min
      bbox[0] = sub->bbox2d[BOX2D_LEFT];
      bbox[1] = sub->bbox2d[BOX2D_BOTTOM];
      bbox[2] = sub->sector->floor.minz;
      // max
      bbox[3] = sub->bbox2d[BOX2D_RIGHT];
      bbox[4] = sub->bbox2d[BOX2D_TOP];
      bbox[5] = sub->sector->ceiling.maxz;
      FixBBoxZ(bbox);
      needToRender = Drawer->viewfrustum.checkBox(bbox);
    }

    if (needToRender) {
      // update world
      if (sub->updateWorldFrame != updateWorldFrame) {
        sub->updateWorldFrame = updateWorldFrame;
        if (!r_disable_world_update) UpdateSubRegion(sub, sub->regions);
      }
      // render the polyobj in the subsector first, and add it to clipper
      // this blocks view with polydoors
      RenderShadowPolyObj(sub);
      AddPolyObjToLightClipper(LightClip, sub, true);
      RenderShadowSubRegion(sub, sub->regions);
    }
  }

  // add subsector's segs to the clipper
  // clipping against mirror is done only for vertical mirror planes
  LightClip.ClipLightAddSubsectorSegs(sub, true);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderShadowBSPNode
//
//  Renders all subsectors below a given node, traversing subtree
//  recursively. Just call with BSP root.
//
//==========================================================================
void VRenderLevelShadowVolume::RenderShadowBSPNode (int bspnum, const float *bbox, bool LimitLights) {
  if (LimitLights) {
    if (r_max_shadow_segs_all >= 0 && AllShadowsNumber > r_max_shadow_segs_all) return;
    if (r_max_shadow_segs_one >= 0 && CurrShadowsNumber > r_max_shadow_segs_one) return;
  }

#ifdef VV_CLIPPER_FULL_CHECK
  if (LightClip.ClipIsFull()) return;
#endif

  if (!LightClip.ClipLightIsBBoxVisible(bbox)) return;

  if (bspnum == -1) {
    if (LimitLights) { ++CurrShadowsNumber; ++AllShadowsNumber; }
    return RenderShadowSubsector(0);
  }

  // found a subsector?
  if (BSPIDX_IS_NON_LEAF(bspnum)) {
    node_t *bsp = &Level->Nodes[bspnum];
    // decide which side the light is on
    const float dist = DotProduct(CurrLightPos, bsp->normal)-bsp->dist;
    if (dist >= CurrLightRadius) {
      // light is completely on front side
      return RenderShadowBSPNode(bsp->children[0], bsp->bbox[0], LimitLights);
    } else if (dist <= -CurrLightRadius) {
      // light is completely on back side
      return RenderShadowBSPNode(bsp->children[1], bsp->bbox[1], LimitLights);
    } else {
      //int side = bsp->PointOnSide(CurrLightPos);
      unsigned side = (unsigned)(dist <= 0.0f);
      // recursively divide front space
      RenderShadowBSPNode(bsp->children[side], bsp->bbox[side], LimitLights);
      // always divide back space for shadows
      side ^= 1;
      return RenderShadowBSPNode(bsp->children[side], bsp->bbox[side], LimitLights);
    }
  } else {
    if (LimitLights) { ++CurrShadowsNumber; ++AllShadowsNumber; }
    return RenderShadowSubsector(BSPIDX_LEAF_SUBSECTOR(bspnum));
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::DrawLightSurfaces
//
//  LightCanCross:
//    -1: two-sided line, top or bottom
//     0: one-sided line, or flat
//     1: two-sided line, middle
//
//==========================================================================
void VRenderLevelShadowVolume::DrawLightSurfaces (surface_t *InSurfs, texinfo_t *texinfo,
                                                  VEntity *SkyBox, bool CheckSkyBoxAlways, int LightCanCross)
{
  if (!InSurfs) return;

  if (!texinfo || texinfo->Tex->Type == TEXTYPE_Null) return;
  if (texinfo->Alpha < 1.0f || texinfo->Additive) return;

  if (SkyBox && SkyBox->IsPortalDirty()) SkyBox = nullptr;

  if (texinfo->Tex == GTextureManager.getIgnoreAnim(skyflatnum) ||
      (CheckSkyBoxAlways && (SkyBox && SkyBox->GetSkyBoxAlways())))
  {
    return;
  }

  for (surface_t *surf = InSurfs; surf; surf = surf->next) {
    if (surf->count < 3) continue; // just in case
    if (!surf->IsPlVisible()) continue; // viewer is in back side or on plane
    const float dist = DotProduct(CurrLightPos, surf->GetNormal())-surf->GetDist();
    if (dist <= 0.0f || dist >= CurrLightRadius) continue; // light is too far away, or surface is not lit
    // ignore masked
    VTexture *tex = surf->texinfo->Tex;
    if (!tex || tex->Type == TEXTYPE_Null) continue;
    if (surf->texinfo->Alpha < 1.0f || surf->texinfo->Additive) continue;
    if (tex->isTranslucent()) continue; // this is translucent texture
    Drawer->DrawSurfaceLight(surf);
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightLine
//
//  clips the given segment and adds any visible pieces to the line list
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightLine (sec_region_t *secregion, drawseg_t *dseg) {
  const seg_t *seg = dseg->seg;

  if (!seg->linedef) return; // miniseg

  const float dist = DotProduct(CurrLightPos, seg->normal)-seg->dist;
  //if (dist <= -CurrLightRadius || dist > CurrLightRadius) return; // light sphere is not touching a plane
  if (fabsf(dist) >= CurrLightRadius) return;

  //k8: here we can call `ClipSegToLight()`, but i see no reasons to do so
  if (!LightClip.IsRangeVisible(*seg->v2, *seg->v1)) return;

  VEntity *skybox = secregion->eceiling.splane->SkyBox;
  if (dseg->mid) DrawLightSurfaces(dseg->mid->surfs, &dseg->mid->texinfo, skybox, false, (seg->backsector ? 1 : 0));
  if (seg->backsector) {
    // two sided line
    if (dseg->top) DrawLightSurfaces(dseg->top->surfs, &dseg->top->texinfo, skybox, false, (seg->backsector ? -1 : 0));
    //k8: horizon/sky cannot block light
    //if (dseg->topsky) DrawLightSurfaces(dseg->topsky->surfs, &dseg->topsky->texinfo, skybox, false, -1);
    if (dseg->bot) DrawLightSurfaces(dseg->bot->surfs, &dseg->bot->texinfo, skybox, false, (seg->backsector ? -1 : 0));
    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      DrawLightSurfaces(sp->surfs, &sp->texinfo, skybox, false, (seg->backsector ? -1 : 0));
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightSecSurface
//
//  this is used for floor and ceilings
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightSecSurface (sec_surface_t *ssurf, VEntity *SkyBox) {
  //const sec_plane_t &plane = *ssurf->secplane;
  if (!ssurf->esecplane.splane->pic) return;

  //const float dist = DotProduct(CurrLightPos, plane.normal)-plane.dist;
  const float dist = ssurf->PointDist(CurrLightPos);
  //if (dist <= -CurrLightRadius || dist > CurrLightRadius) return; // light is in back side or on plane
  if (fabsf(dist) >= CurrLightRadius) return;

  DrawLightSurfaces(ssurf->surfs, &ssurf->texinfo, SkyBox, true, 0);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightPolyObj
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightPolyObj (subsector_t *sub) {
  if (sub && sub->HasPObjs() && r_draw_pobj) {
    subregion_t *region = sub->regions;
    sec_region_t *secregion = region->secregion;
    for (auto &&it : sub->PObjFirst()) {
      polyobj_t *pobj = it.value();
      seg_t **polySeg = pobj->segs;
      for (int polyCount = pobj->numsegs; polyCount--; ++polySeg) {
        RenderLightLine(secregion, (*polySeg)->drawsegs);
      }
    }
  }
}

//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightSubRegion
//
//  Determine floor/ceiling planes.
//  Draw one or more line segments.
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightSubRegion (subsector_t *sub, subregion_t *region) {
  const bool nextFirst = NeedToRenderNextSubFirst(region);
  if (nextFirst) RenderLightSubRegion(sub, region->next);

  sec_region_t *secregion = region->secregion;

  if (!clip_adv_regions_light || LightClip.ClipLightCheckRegion(region, sub, false)) {
    drawseg_t *ds = region->lines;
    for (int count = sub->numlines; count--; ++ds) RenderLightLine(secregion, ds);
  }

  {
    sec_surface_t *fsurf[4];
    GetFlatSetToRender(sub, region, fsurf);

    if (fsurf[0]) RenderLightSecSurface(fsurf[0], secregion->efloor.splane->SkyBox);
    if (fsurf[1]) RenderLightSecSurface(fsurf[1], secregion->efloor.splane->SkyBox);

    if (fsurf[2]) RenderLightSecSurface(fsurf[2], secregion->eceiling.splane->SkyBox);
    if (fsurf[3]) RenderLightSecSurface(fsurf[3], secregion->eceiling.splane->SkyBox);
  }

  if (!nextFirst && region->next) return RenderLightSubRegion(sub, region->next);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightSubsector
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightSubsector (int num) {
  vassert(num >= 0 && num < Level->NumSubsectors);
  subsector_t *sub = &Level->Subsectors[num];

  if (!sub->sector->linecount) return; // skip sectors containing original polyobjs

  // `LightBspVis` is already an intersection, no need to check `BspVis` here
  //if (!IsSubsectorLitBspVis(num) || !(BspVis[num>>3]&(1<<(num&7)))) return;

  if (IsSubsectorLitBspVis(num)) {
    if (LightClip.ClipLightCheckSubsector(sub, false)) {
      // update world
      if (sub->updateWorldFrame != updateWorldFrame) {
        sub->updateWorldFrame = updateWorldFrame;
        if (!r_disable_world_update) UpdateSubRegion(sub, sub->regions);
      }
      // render the polyobj in the subsector first, and add it to clipper
      // this blocks view with polydoors
      RenderLightPolyObj(sub);
      AddPolyObjToLightClipper(LightClip, sub, false);
      RenderLightSubRegion(sub, sub->regions);
    }
  }

  // add subsector's segs to the clipper
  // clipping against mirror is done only for vertical mirror planes
  LightClip.ClipLightAddSubsectorSegs(sub, false);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightBSPNode
//
//  Renders all subsectors below a given node, traversing subtree
//  recursively. Just call with BSP root.
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightBSPNode (int bspnum, const float *bbox, bool LimitLights) {
  if (LimitLights) {
     if (r_max_light_segs_all >= 0 && AllLightsNumber > r_max_light_segs_all) return;
     if (r_max_light_segs_one >= 0 && CurrLightsNumber > r_max_light_segs_one) return;
  }

#ifdef VV_CLIPPER_FULL_CHECK
  if (LightClip.ClipIsFull()) return;
#endif

  if (!LightClip.ClipLightIsBBoxVisible(bbox)) return;
  //if (!CheckSphereVsAABBIgnoreZ(bbox, CurrLightPos, CurrLightRadius)) return;

  if (bspnum == -1) {
    if (LimitLights) { ++CurrLightsNumber; ++AllLightsNumber; }
    return RenderLightSubsector(0);
  }

  // found a subsector?
  if (BSPIDX_IS_NON_LEAF(bspnum)) {
    node_t *bsp = &Level->Nodes[bspnum];
    // decide which side the light is on
    const float dist = DotProduct(CurrLightPos, bsp->normal)-bsp->dist;
    if (dist >= CurrLightRadius) {
      // light is completely on front side
      return RenderLightBSPNode(bsp->children[0], bsp->bbox[0], LimitLights);
    } else if (dist <= -CurrLightRadius) {
      // light is completely on back side
      return RenderLightBSPNode(bsp->children[1], bsp->bbox[1], LimitLights);
    } else {
      //int side = bsp->PointOnSide(CurrLightPos);
      unsigned side = (unsigned)(dist <= 0.0f);
      // recursively divide front space
      RenderLightBSPNode(bsp->children[side], bsp->bbox[side], LimitLights);
      // possibly divide back space
      side ^= 1;
      return RenderLightBSPNode(bsp->children[side], bsp->bbox[side], LimitLights);
    }
  } else {
    if (LimitLights) { ++CurrLightsNumber; ++AllLightsNumber; }
    return RenderLightSubsector(BSPIDX_LEAF_SUBSECTOR(bspnum));
  }
}


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

  //  0 if scissor is empty
  // -1 if scissor has no sense (should not be used)
  //  1 if scissor is set
  int hasScissor = 1;
  int scoord[4];
  bool checkModels = false;
  float dummyBBox[6];

  //GCon->Logf("LBB:(%f,%f,%f)-(%f,%f,%f)", LitBBox[0], LitBBox[1], LitBBox[2], LitBBox[3], LitBBox[4], LitBBox[5]);

  // setup light scissor rectangle
  if (r_advlight_opt_scissor) {
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
  if (allowShadows && checkModels) {
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
  LightClip.ClearClipNodes(CurrLightPos, Level, CurrLightRadius);
  if (r_shadowmaps) {
    Drawer->BeginLightShadowMaps(CurrLightPos, CurrLightRadius, coneDir, coneAngle);
    if (allowShadows) {
      (void)fsecCounterGen(); // for checker
      if (r_max_shadow_segs_all) {
        //VMatrix4 oldPrj;
        //Drawer->GetProjectionMatrix(oldPrj);
        for (unsigned fc = 0; fc < 6; ++fc) {
          Drawer->SetupLightShadowMap(CurrLightPos, CurrLightRadius, coneDir, coneAngle, fc);
          dummyBBox[0] = dummyBBox[1] = dummyBBox[2] = -99999;
          dummyBBox[3] = dummyBBox[4] = dummyBBox[5] = +99999;
          RenderShadowBSPNode(Level->NumNodes-1, dummyBBox, LimitLights);
        }
        //Drawer->SetProjectionMatrix(oldPrj);
      }
    }
    Drawer->EndLightShadowMaps();
  } else {
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

  Drawer->BeginModelsLightPass(CurrLightPos, CurrLightRadius, LightMin, Color, coneDir, coneAngle);
  RenderMobjsLight(ent);
  Drawer->EndModelsLightPass();

  //if (hasScissor) Drawer->DebugRenderScreenRect(scoord[0], scoord[1], scoord[2], scoord[3], 0x7f007f00);

  /*if (hasScissor)*/ Drawer->ResetScissor();
}
