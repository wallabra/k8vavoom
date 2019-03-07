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
#include "gamedefs.h"
#include "r_local.h"


extern VCvarB r_darken;
extern VCvarI r_ambient;
extern VCvarB r_allow_ambient;
extern VCvarB r_allow_subtractive_lights;
extern VCvarB r_dynamic_clip;
extern VCvarB r_dynamic_clip_more;


// ////////////////////////////////////////////////////////////////////////// //
// private data definitions
// ////////////////////////////////////////////////////////////////////////// //

static subsector_t *r_sub;
static sec_region_t *r_region;

VCvarF r_lights_radius("r_lights_radius", "2048", "Maximum light radius.", CVAR_Archive);
VCvarF r_lights_radius_sight_check("r_lights_radius_sight_check", "1024", "Maximum light radius.", CVAR_Archive);
VCvarI r_max_model_lights("r_max_model_lights", "32", "Maximum model lights.", CVAR_Archive);
VCvarI r_max_model_shadows("r_max_model_shadows", "2", "Maximum model shadows.", CVAR_Archive);
VCvarI r_max_lights("r_max_lights", "64", "Maximum lights.", CVAR_Archive);
VCvarI r_max_shadows("r_max_shadows", "128", "Maximum shadows.", CVAR_Archive);

// not used anymore (was used in grid optimiser)
VCvarI r_hashlight_static_div("r_hashlight_static_div", "8", "Divisor for static light spatial hashing.", CVAR_Archive);
VCvarI r_hashlight_dynamic_div("r_hashlight_dynamic_div", "8", "Divisor for dynamic light spatial hashing.", CVAR_Archive);

VCvarF r_light_filter_static_coeff("r_light_filter_static_coeff", "0.2", "How close static lights should be to be filtered out?\n(0.5-0.7 is usually ok).", CVAR_Archive);

extern VCvarB r_dynamic_clip_more;

static VCvarB dbg_adv_light_notrace_mark("dbg_adv_light_notrace_mark", false, "Mark notrace lights red?", CVAR_PreInit);

static VCvarB r_advlight_opt_trace("r_advlight_opt_trace", true, "Try to skip shadow volumes when a light can cast no shadow.", CVAR_PreInit);
static VCvarB r_advlight_opt_scissor("r_advlight_opt_scissor", true, "Use scissor rectangle to limit light overdraws.", CVAR_PreInit);


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
//  VAdvancedRenderLevel::PushDlights
//
//==========================================================================
void VAdvancedRenderLevel::PushDlights () {
  r_dlightframecount = 1;
}


//==========================================================================
//
//  VAdvancedRenderLevel::LightPoint
//
//==========================================================================
vuint32 VAdvancedRenderLevel::LightPoint (const TVec &p, VEntity *mobj) {
  if (FixedLight) return FixedLight|(FixedLight<<8)|(FixedLight<<16)|(FixedLight<<24);

  float l = 0, lr = 0, lg = 0, lb = 0;

  subsector_t *sub = Level->PointInSubsector(p);
  subregion_t *reg = sub->regions;
  if (reg) {
    while (reg->next) {
      float d = DotProduct(p, reg->floor->secplane->normal)-reg->floor->secplane->dist;
      if (d >= 0.0f) break;
      reg = reg->next;
    }

    // region's base light
    if (r_allow_ambient) {
      l = reg->secregion->params->lightlevel+ExtraLight;
      l = MID(0, l, 255);
      if (r_darken) l = light_remap[(int)l];
      if (l < r_ambient) l = r_ambient;
      l = MID(0, l, 255);
    } else {
      l = 0;
    }
    int SecLightColour = reg->secregion->params->LightColour;
    lr = ((SecLightColour>>16)&255)*l/255.0f;
    lg = ((SecLightColour>>8)&255)*l/255.0f;
    lb = (SecLightColour&255)*l/255.0f;
  }

  // add static lights
  if (r_static_lights) {
    if (!staticLightsFiltered) RefilterStaticLights();
    const vuint8 *dyn_facevis = Level->LeafPVS(sub);
    const bool hasPVS = Level->HasPVS();
    for (int i = 0; i < Lights.Num(); i++) {
      //if (!Lights[i].radius) continue;
      if (!Lights[i].active) continue;

      // check potential visibility
      if (hasPVS && !(dyn_facevis[Lights[i].leafnum>>3]&(1<<(Lights[i].leafnum&7)))) continue;

      const float add = Lights[i].radius-Length(p-Lights[i].origin);
      if (add > 0) {
        if (r_dynamic_clip) {
          if (!RadiusCastRay(p, Lights[i].origin, (mobj ? mobj->Radius : 0), false/*r_dynamic_clip_more*/)) continue;
        }
        l += add;
        lr += add*((Lights[i].colour>>16)&255)/255.0f;
        lg += add*((Lights[i].colour>>8)&255)/255.0f;
        lb += add*(Lights[i].colour&255)/255.0f;
      }
    }
  }

  // add dynamic lights
  if (r_dynamic) {
    const vuint8 *dyn_facevis = Level->LeafPVS(sub);
    const bool hasPVS = Level->HasPVS();
    for (int i = 0; i < MAX_DLIGHTS; i++) {
      const dlight_t &dl = DLights[i];
      if (dl.type == DLTYPE_Subtractive && !r_allow_subtractive_lights) continue;

      if (!dl.radius || dl.die < Level->Time) continue;

      // check potential visibility
      if (r_dynamic_clip && hasPVS) {
        int leafnum = Level->PointInSubsector(dl.origin)-Level->Subsectors;
        if (!(dyn_facevis[leafnum>>3]&(1<<(leafnum&7)))) continue;
      }

      float add = (dl.radius-dl.minlight)-Length(p-dl.origin);
      if (add > 0) {
        if (r_dynamic_clip) {
          if (!RadiusCastRay(p, dl.origin, (mobj ? mobj->Radius : 0), false/*r_dynamic_clip_more*/)) continue;
        }
        if (dl.type == DLTYPE_Subtractive) add = -add;
        l += add;
        lr += add*((dl.colour>>16)&255)/255.0f;
        lg += add*((dl.colour>>8)&255)/255.0f;
        lb += add*(dl.colour&255)/255.0f;
      }
    }
  }

  if (l > 255) l = 255; else if (l < 0) l = 0;
  if (lr > 255) lr = 255; else if (lr < 0) lr = 0;
  if (lg > 255) lg = 255; else if (lg < 0) lg = 0;
  if (lb > 255) lb = 255; else if (lb < 0) lb = 0;

  return ((int)l<<24)|((int)lr<<16)|((int)lg<<8)|((int)lb);
}


//==========================================================================
//
//  VAdvancedRenderLevel::LightPointAmbient
//
//==========================================================================
vuint32 VAdvancedRenderLevel::LightPointAmbient (const TVec &p, VEntity *mobj) {
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
    l = MID(0, l, 255);
    if (r_darken) l = light_remap[(int)l];
    if (l < r_ambient) l = r_ambient;
    l = MID(0, l, 255);
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


#define UPDATE_LIGHTVIS(ssindex)  do { \
  LightVis[(unsigned)(ssindex)>>3] |= 1<<((unsigned)(ssindex)&7); \
  if (LightBspVis[(unsigned)(ssindex)>>3] |= BspVis[(unsigned)(ssindex)>>3]&(1<<((unsigned)(ssindex)&7))) HasLightIntersection = true; \
} while (0)


//==========================================================================
//
//  VAdvancedRenderLevel::BuildLightVis
//
//==========================================================================
void VAdvancedRenderLevel::BuildLightVis (int bspnum, float *bbox) {
  if (LightClip.ClipIsFull()) return;

  if (!LightClip.ClipLightIsBBoxVisible(bbox, CurrLightPos, CurrLightRadius)) return;

  if (bspnum == -1) {
    const unsigned SubNum = 0;
    const subsector_t *Sub = &Level->Subsectors[SubNum];
    if (!Sub->sector->linecount) return; // skip sectors containing original polyobjs

    if (!LightClip.ClipLightCheckSubsector(Sub, CurrLightPos, CurrLightRadius)) return;

    //LightVis[SubNum>>3] |= 1<<(SubNum&7);
    UPDATE_LIGHTVIS(SubNum);
    LightClip.ClipLightAddSubsectorSegs(Sub, CurrLightPos, CurrLightRadius);
    return;
  }

  // found a subsector?
  if (!(bspnum&NF_SUBSECTOR)) {
    node_t *bsp = &Level->Nodes[bspnum];

    // decide which side the view point is on
    const float dist = DotProduct(CurrLightPos, bsp->normal)-bsp->dist;
    if (dist > CurrLightRadius) {
      // light is completely on front side
      BuildLightVis(bsp->children[0], bsp->bbox[0]);
    } else if (dist < -CurrLightRadius) {
      // light is completely on back side
      BuildLightVis(bsp->children[1], bsp->bbox[1]);
    } else {
      int side = bsp->PointOnSide(CurrLightPos);

      // recursively divide front space
      BuildLightVis(bsp->children[side], bsp->bbox[side]);

      // possibly divide back space
      if (!LightClip.ClipLightIsBBoxVisible(bsp->bbox[side^1], CurrLightPos, CurrLightRadius)) return;

      BuildLightVis(bsp->children[side^1], bsp->bbox[side^1]);
    }
    return;
  }

  const unsigned SubNum = (unsigned)(bspnum&(~NF_SUBSECTOR));
  const subsector_t *Sub = &Level->Subsectors[SubNum];
  if (!Sub->sector->linecount) return; // skip sectors containing original polyobjs

  if (!LightClip.ClipLightCheckSubsector(Sub, CurrLightPos, CurrLightRadius)) return;

  //LightVis[SubNum>>3] |= 1<<(SubNum&7);
  UPDATE_LIGHTVIS(SubNum);
  LightClip.ClipLightAddSubsectorSegs(Sub, CurrLightPos, CurrLightRadius);
}


//==========================================================================
//
//  VAdvancedRenderLevel::DrawShadowSurfaces
//
//==========================================================================
void VAdvancedRenderLevel::DrawShadowSurfaces (surface_t *InSurfs, texinfo_t *texinfo,
                                               bool CheckSkyBoxAlways, bool LightCanCross)
{
  surface_t *surfs = InSurfs;
  if (!surfs) return;

  if (texinfo->Tex->Type == TEXTYPE_Null) return;
  if (texinfo->Alpha < 1.0f) return;

  do {
    Drawer->RenderSurfaceShadowVolume(surfs, CurrLightPos, CurrLightRadius, LightCanCross);
    surfs = surfs->next;
  } while (surfs);
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderShadowLine
//
//  Clips the given segment and adds any visible pieces to the line list.
//
//==========================================================================
void VAdvancedRenderLevel::RenderShadowLine (drawseg_t *dseg) {
  seg_t *line = dseg->seg;
  if (!line->linedef) return; // miniseg

  // note: we don't want to filter out shadows that are behind
  const float dist = DotProduct(CurrLightPos, line->normal)-line->dist;
  if (dist < -CurrLightRadius || dist > CurrLightRadius) return; // light is too far away

/*
    k8: i don't know what Janis wanted to accomplish with this, but it actually
        makes clipping WORSE due to limited precision
  // clip sectors that are behind rendered segs
  TVec v1 = *line->v1;
  TVec v2 = *line->v2;
  const TVec r1 = CurrLightPos-v1;
  const TVec r2 = CurrLightPos-v2;
  const float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), CurrLightPos);
  const float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), CurrLightPos);

  // there might be a better method of doing this, but this one works for now...
       if (D1 > CurrLightRadius && D2 < -CurrLightRadius) v2 += (v2-v1)*D1/(D1-D2);
  else if (D2 > CurrLightRadius && D1 < -CurrLightRadius) v1 += (v1-v2)*D2/(D2-D1);

  if (!LightClip.IsRangeVisible(LightClip.PointToClipAngle(v2), LightClip.PointToClipAngle(v1))) return;
*/
  if (!LightClip.IsRangeVisible(*line->v2, *line->v1)) return;

  //line_t *linedef = line->linedef;
  //side_t *sidedef = line->sidedef;

  if (!line->backsector) {
    // single sided line
    DrawShadowSurfaces(dseg->mid->surfs, &dseg->mid->texinfo, false, false);
    DrawShadowSurfaces(dseg->topsky->surfs, &dseg->topsky->texinfo, false, false);
  } else {
    // two sided line
    DrawShadowSurfaces(dseg->top->surfs, &dseg->top->texinfo, false, false);
    DrawShadowSurfaces(dseg->topsky->surfs, &dseg->topsky->texinfo, false, true);
    DrawShadowSurfaces(dseg->bot->surfs, &dseg->bot->texinfo, false, false);
    DrawShadowSurfaces(dseg->mid->surfs, &dseg->mid->texinfo, false, true);

    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      DrawShadowSurfaces(sp->surfs, &sp->texinfo, false, false);
    }
  }
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderShadowSecSurface
//
//==========================================================================
void VAdvancedRenderLevel::RenderShadowSecSurface (sec_surface_t *ssurf, VEntity *SkyBox) {
  sec_plane_t &plane = *ssurf->secplane;

  if (!plane.pic) return;

  // note: we don't want to filter out shadows that are behind
  const float dist = DotProduct(CurrLightPos, plane.normal)-plane.dist;
  if (dist < -CurrLightRadius || dist > CurrLightRadius) return; // light is too far away

  DrawShadowSurfaces(ssurf->surfs, &ssurf->texinfo, true, false);
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderShadowSubRegion
//
//  Determine floor/ceiling planes.
//  Draw one or more line segments.
//
//==========================================================================
void VAdvancedRenderLevel::RenderShadowSubRegion (subregion_t *region) {
  const float dist = DotProduct(CurrLightPos, region->floor->secplane->normal)-region->floor->secplane->dist;

  if (region->next && dist <= -CurrLightRadius) {
    if (!LightClip.ClipLightCheckRegion(region->next, r_sub, CurrLightPos, CurrLightRadius)) return;
    RenderShadowSubRegion(region->next);
  }

  r_region = region->secregion;

  if (r_sub->poly) {
    // render the polyobj in the subsector first
    int polyCount = r_sub->poly->numsegs;
    seg_t **polySeg = r_sub->poly->segs;
    while (polyCount--) {
      RenderShadowLine((*polySeg)->drawsegs);
      ++polySeg;
    }
  }

  int count = r_sub->numlines;
  drawseg_t *ds = region->lines;
  while (count--) {
    RenderShadowLine(ds);
    ++ds;
  }

  RenderShadowSecSurface(region->floor, r_region->floor->SkyBox);
  RenderShadowSecSurface(region->ceil, r_region->ceiling->SkyBox);

  if (region->next && dist > CurrLightRadius) {
    if (!LightClip.ClipLightCheckRegion(region->next, r_sub, CurrLightPos, CurrLightRadius)) return;
    RenderShadowSubRegion(region->next);
  }
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderShadowSubsector
//
//==========================================================================
void VAdvancedRenderLevel::RenderShadowSubsector (int num) {
  subsector_t *Sub = &Level->Subsectors[num];
  r_sub = Sub;

  // don't do this check for shadows
  //if (!(LightBspVis[num>>3]&(1<<(num&7))) || !(BspVis[num>>3]&(1<<(num&7)))) return;

  if (!Sub->sector->linecount) return; // skip sectors containing original polyobjs

  if (!LightClip.ClipLightCheckSubsector(Sub, CurrLightPos, CurrLightRadius)) return;

  RenderShadowSubRegion(Sub->regions);

  // add subsector's segs to the clipper
  // clipping against mirror is done only for vertical mirror planes
  LightClip.ClipLightAddSubsectorSegs(Sub, CurrLightPos, CurrLightRadius);
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderShadowBSPNode
//
//  Renders all subsectors below a given node, traversing subtree
//  recursively. Just call with BSP root.
//
//==========================================================================
void VAdvancedRenderLevel::RenderShadowBSPNode (int bspnum, float *bbox, bool LimitLights) {
  if (LimitLights && r_max_shadows >= 0 && CurrShadowsNumber > r_max_shadows) return;

  if (LightClip.ClipIsFull()) return;

  if (!LightClip.ClipLightIsBBoxVisible(bbox, CurrLightPos, CurrLightRadius)) return;

  if (bspnum == -1) {
    RenderShadowSubsector(0);
    if (LimitLights) ++CurrShadowsNumber;
    return;
  }

  // found a subsector?
  if (!(bspnum&NF_SUBSECTOR)) {
    node_t *bsp = &Level->Nodes[bspnum];

    // decide which side the light is on
    const float dist = DotProduct(CurrLightPos, bsp->normal)-bsp->dist;
    if (dist > CurrLightRadius) {
      // light is completely on front side
      RenderShadowBSPNode(bsp->children[0], bsp->bbox[0], LimitLights ? true : false);
    } else if (dist < -CurrLightRadius) {
      // light is completely on back side
      RenderShadowBSPNode(bsp->children[1], bsp->bbox[1], LimitLights ? true : false);
    } else {
      int side = bsp->PointOnSide(CurrLightPos);
      // recursively divide front space
      RenderShadowBSPNode(bsp->children[side], bsp->bbox[side], false);
      // always divide back space for shadows
      RenderShadowBSPNode(bsp->children[side^1], bsp->bbox[side^1], false);
    }

    //if (LimitLights) CurrShadowsNumber += 1;
    return;
  }

  RenderShadowSubsector(bspnum&(~NF_SUBSECTOR));
  if (LimitLights) ++CurrShadowsNumber;
}


//==========================================================================
//
//  VAdvancedRenderLevel::DrawLightSurfaces
//
//==========================================================================
void VAdvancedRenderLevel::DrawLightSurfaces (surface_t *InSurfs, texinfo_t *texinfo,
                                              VEntity *SkyBox, bool CheckSkyBoxAlways, bool LightCanCross)
{
  if (!InSurfs) return;

  if (texinfo->Tex->Type == TEXTYPE_Null) return;
  if (texinfo->Alpha < 1.0f) return;

  if (SkyBox && (SkyBox->EntityFlags&VEntity::EF_FixedModel)) SkyBox = nullptr;

  if (texinfo->Tex == GTextureManager.getIgnoreAnim(skyflatnum) ||
      (CheckSkyBoxAlways && (SkyBox && SkyBox->eventSkyBoxGetAlways())))
  {
    return;
  }

  surface_t *surfs = InSurfs;
  do {
    Drawer->DrawSurfaceLight(surfs, CurrLightPos, CurrLightRadius, LightCanCross);
    surfs = surfs->next;
  } while (surfs);
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderLightLine
//
//  Clips the given segment and adds any visible pieces to the line list.
//
//==========================================================================
void VAdvancedRenderLevel::RenderLightLine (drawseg_t *dseg) {
  seg_t *line = dseg->seg;

  if (!line->linedef) return; // miniseg

  const float dist = DotProduct(CurrLightPos, line->normal)-line->dist;
  if (dist <= -CurrLightRadius || dist > CurrLightRadius) return; // light sphere is not touching a plane

/*
    k8: i don't know what Janis wanted to accomplish with this, but it actually
        makes clipping WORSE due to limited precision
  // clip sectors that are behind rendered segs
  TVec v1 = *line->v1;
  TVec v2 = *line->v2;
  TVec r1 = CurrLightPos-v1;
  TVec r2 = CurrLightPos-v2;
  float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), CurrLightPos);
  float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), CurrLightPos);

  // there might be a better method of doing this, but this one works for now...
       if (D1 > CurrLightRadius && D2 < -CurrLightRadius) v2 += (v2-v1)*D1/(D1-D2);
  else if (D2 > CurrLightRadius && D1 < -CurrLightRadius) v1 += (v1-v2)*D2/(D2-D1);

  if (!LightClip.IsRangeVisible(LightClip.PointToClipAngle(v2), LightClip.PointToClipAngle(v1))) return;
*/
  if (!LightClip.IsRangeVisible(*line->v2, *line->v1)) return;

  if (!line->backsector) {
    // single sided line
    DrawLightSurfaces(dseg->mid->surfs, &dseg->mid->texinfo, r_region->ceiling->SkyBox, false, false);
    DrawLightSurfaces(dseg->topsky->surfs, &dseg->topsky->texinfo, r_region->ceiling->SkyBox, false, false);
  } else {
    // two sided line
    DrawLightSurfaces(dseg->top->surfs, &dseg->top->texinfo, r_region->ceiling->SkyBox, false, false);
    DrawLightSurfaces(dseg->topsky->surfs, &dseg->topsky->texinfo, r_region->ceiling->SkyBox, false, true);
    DrawLightSurfaces(dseg->bot->surfs, &dseg->bot->texinfo, r_region->ceiling->SkyBox, false, false);
    DrawLightSurfaces(dseg->mid->surfs, &dseg->mid->texinfo, r_region->ceiling->SkyBox, false, true);

    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      DrawLightSurfaces(sp->surfs, &sp->texinfo, r_region->ceiling->SkyBox, false, false);
    }
  }
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderLightSecSurface
//
//==========================================================================
void VAdvancedRenderLevel::RenderLightSecSurface (sec_surface_t *ssurf, VEntity *SkyBox) {
  sec_plane_t &plane = *ssurf->secplane;

  if (!plane.pic) return;

  const float dist = DotProduct(CurrLightPos, plane.normal)-plane.dist;
  if (dist <= -CurrLightRadius || dist > CurrLightRadius) return; // light is in back side or on plane

  DrawLightSurfaces(ssurf->surfs, &ssurf->texinfo, SkyBox, true, false);
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderLightSubRegion
//
//  Determine floor/ceiling planes.
//  Draw one or more line segments.
//
//==========================================================================
void VAdvancedRenderLevel::RenderLightSubRegion (subregion_t *region) {
  const float dist = DotProduct(CurrLightPos, region->floor->secplane->normal)-region->floor->secplane->dist;

  if (region->next && dist <= -CurrLightRadius) {
    if (!LightClip.ClipLightCheckRegion(region->next, r_sub, CurrLightPos, CurrLightRadius)) return;
    RenderLightSubRegion(region->next);
  }

  r_region = region->secregion;

  if (r_sub->poly) {
    // render the polyobj in the subsector first
    int polyCount = r_sub->poly->numsegs;
    seg_t **polySeg = r_sub->poly->segs;
    while (polyCount--) {
      RenderLightLine((*polySeg)->drawsegs);
      ++polySeg;
    }
  }

  int count = r_sub->numlines;
  drawseg_t *ds = region->lines;
  while (count--) {
    RenderLightLine(ds);
    ++ds;
  }

  RenderLightSecSurface(region->floor, r_region->floor->SkyBox);
  RenderLightSecSurface(region->ceil, r_region->ceiling->SkyBox);

  if (region->next && dist > CurrLightRadius) {
    if (!LightClip.ClipLightCheckRegion(region->next, r_sub, CurrLightPos, CurrLightRadius)) return;
    RenderLightSubRegion(region->next);
  }
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderLightSubsector
//
//==========================================================================
void VAdvancedRenderLevel::RenderLightSubsector (int num) {
  subsector_t *Sub = &Level->Subsectors[num];
  r_sub = Sub;

  if (!Sub->sector->linecount) return; // skip sectors containing original polyobjs

  // `LightBspVis` is already an intersection, no need to check `BspVis` here
  //if (!(LightBspVis[num>>3]&(1<<(num&7))) || !(BspVis[num>>3]&(1<<(num&7)))) return;
  if (!(LightBspVis[(unsigned)num>>3]&(1<<((unsigned)num&7)))) return;

  if (!LightClip.ClipLightCheckSubsector(Sub, CurrLightPos, CurrLightRadius)) return;

  RenderLightSubRegion(Sub->regions);

  // add subsector's segs to the clipper
  // clipping against mirror is done only for vertical mirror planes
  LightClip.ClipLightAddSubsectorSegs(Sub, CurrLightPos, CurrLightRadius);
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderLightBSPNode
//
//  Renders all subsectors below a given node, traversing subtree
//  recursively. Just call with BSP root.
//
//==========================================================================
void VAdvancedRenderLevel::RenderLightBSPNode (int bspnum, float *bbox, bool LimitLights) {
  if (LimitLights && r_max_lights >= 0 && CurrLightsNumber > r_max_lights) return;

  if (LightClip.ClipIsFull()) return;

  if (!LightClip.ClipLightIsBBoxVisible(bbox, CurrLightPos, CurrLightRadius)) return;

  if (bspnum == -1) {
    RenderLightSubsector(0);
    if (LimitLights) ++CurrLightsNumber;
    return;
  }

  // found a subsector?
  if (!(bspnum&NF_SUBSECTOR)) {
    node_t *bsp = &Level->Nodes[bspnum];

    // decide which side the light is on
    const float dist = DotProduct(CurrLightPos, bsp->normal)-bsp->dist;
    if (dist > CurrLightRadius) {
      // light is completely on front side
      RenderLightBSPNode(bsp->children[0], bsp->bbox[0], LimitLights ? true : false);
    } else if (dist < -CurrLightRadius) {
      // light is completely on back side
      RenderLightBSPNode(bsp->children[1], bsp->bbox[1], LimitLights ? true : false);
    } else {
      int side = bsp->PointOnSide(CurrLightPos);
      // recursively divide front space
      RenderLightBSPNode(bsp->children[side], bsp->bbox[side], false);
      // possibly divide back space
      if (!LightClip.ClipLightIsBBoxVisible(bsp->bbox[side^1], CurrLightPos, CurrLightRadius)) {
        //if (LimitLights) ++CurrLightsNumber;
        return;
      }
      RenderLightBSPNode(bsp->children[side^1], bsp->bbox[side^1], false);
    }

    //if (LimitLights) ++CurrLightsNumber;
    return;
  }

  RenderLightSubsector(bspnum&(~NF_SUBSECTOR));
  if (LimitLights) ++CurrLightsNumber;
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderLightShadows
//
//==========================================================================
void VAdvancedRenderLevel::RenderLightShadows (const refdef_t *RD, const VViewClipper *Range,
                                               TVec &Pos, float Radius, vuint32 Colour, bool LimitLights)
{
  if (Radius < 2.0f) return;

  CurrLightPos = Pos;
  CurrLightRadius = Radius;
  CurrLightColour = Colour;

  float dummy_bbox[6] = { -99999, -99999, -99999, 99999, 99999, 99999 };

  // k8: create light bbox (actually, it does nothing interesting)
  /*
  dummy_bbox[0] = Pos.x-Radius;
  dummy_bbox[1] = Pos.y-Radius;
  dummy_bbox[2] = Pos.z-Radius;

  dummy_bbox[3] = Pos.x+Radius;
  dummy_bbox[4] = Pos.y+Radius;
  dummy_bbox[5] = Pos.z+Radius;
  */

  bool doShadows = true;

  if (r_advlight_opt_trace && !Level->NeedProperLightTraceAt(Pos, Radius)) {
    //GCon->Log("some light doesn't need shadows");
    //return;
    if (dbg_adv_light_notrace_mark) Colour = 0xffff0000U;
    doShadows = false;
  }

  // build vis data for light
  LightClip.ClearClipNodes(CurrLightPos, Level);
  memset(LightVis, 0, VisSize);
  memset(LightBspVis, 0, VisSize);
  HasLightIntersection = false;
  BuildLightVis(Level->NumNodes-1, dummy_bbox);
  if (!HasLightIntersection) return;

  // create combined light and view visibility
  /*
  bool HaveIntersect = false;
  for (int i = 0; i < VisSize; ++i) {
    LightBspVis[i] = BspVis[i]&LightVis[i];
    if (LightBspVis[i]) HaveIntersect = true;
  }
  if (!HaveIntersect) return;
  */

  // setup light scissor rectangle
  if (r_advlight_opt_scissor) Drawer->SetupLightScissor(Pos, Radius);

  ResetMobjsLightCount(true);
  // do shadow volumes
  Drawer->BeginLightShadowVolumes();
  LightClip.ClearClipNodes(CurrLightPos, Level);
  if (doShadows) {
    RenderShadowBSPNode(Level->NumNodes-1, dummy_bbox, LimitLights);
    Drawer->BeginModelsShadowsPass(CurrLightPos, CurrLightRadius);
    RenderMobjsShadow();
  }
  Drawer->EndLightShadowVolumes();

  ResetMobjsLightCount(false);

  // k8: the question is: why we are rendering surfaces instead
  //     of simply render a light circle? shadow volumes should
  //     take care of masking the area, so simply rendering a
  //     circle should do the trick.

  // draw light
  Drawer->BeginLightPass(CurrLightPos, CurrLightRadius, Colour);
  LightClip.ClearClipNodes(CurrLightPos, Level);
  RenderLightBSPNode(Level->NumNodes-1, dummy_bbox, LimitLights);
  Drawer->BeginModelsLightPass(CurrLightPos, CurrLightRadius, Colour);
  RenderMobjsLight();

  if (r_advlight_opt_scissor) Drawer->ResetScissor();
}
