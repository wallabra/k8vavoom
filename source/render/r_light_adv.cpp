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

VCvarI r_max_model_lights("r_max_model_lights", "32", "Maximum model lights.", CVAR_Archive);
VCvarI r_max_model_shadows("r_max_model_shadows", "2", "Maximum model shadows.", CVAR_Archive);

VCvarI r_max_lights("r_max_lights", "64", "Maximum lights.", CVAR_Archive);
static VCvarI r_max_light_segs_all("r_max_light_segs_all", "-1", "Maximum light segments for all lights.", CVAR_Archive);
static VCvarI r_max_light_segs_one("r_max_light_segs_one", "-1", "Maximum light segments for one light.", CVAR_Archive);
// was 128, but with scissored light, there is no sense to limit it anymore
static VCvarI r_max_shadow_segs_all("r_max_shadow_segs_all", "-1", "Maximum shadow segments for all lights.", CVAR_Archive);
static VCvarI r_max_shadow_segs_one("r_max_shadow_segs_one", "-1", "Maximum shadow segments for one light.", CVAR_Archive);

VCvarF r_light_filter_static_coeff("r_light_filter_static_coeff", "0.2", "How close static lights should be to be filtered out?\n(0.5-0.7 is usually ok).", CVAR_Archive);

extern VCvarB r_dynamic_clip_more;

static VCvarB dbg_adv_light_notrace_mark("dbg_adv_light_notrace_mark", false, "Mark notrace lights red?", CVAR_PreInit);

static VCvarB r_advlight_opt_trace("r_advlight_opt_trace", true, "Try to skip shadow volumes when a light can cast no shadow.", CVAR_Archive|CVAR_PreInit);
static VCvarB r_advlight_opt_scissor("r_advlight_opt_scissor", true, "Use scissor rectangle to limit light overdraws.", CVAR_Archive|CVAR_PreInit);
static VCvarB r_advlight_opt_separate_vis("r_advlight_opt_separate_vis", false, "Calculate light and render vis intersection as separate step?", CVAR_Archive|CVAR_PreInit);


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
vuint32 VAdvancedRenderLevel::LightPoint (const TVec &p, float radius) {
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
  if (r_dynamic && sub->dlightframe == r_dlightframecount) {
    for (unsigned i = 0; i < MAX_DLIGHTS; ++i) {
      if (!(sub->dlightbits&(1U<<i))) continue;
      // check potential visibility
      if (dyn_facevis) {
        //int leafnum = Level->PointInSubsector(dl.origin)-Level->Subsectors;
        const int leafnum = dlinfo[i].leafnum;
        check(leafnum >= 0);
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

/*
  this also checks if we need to do shadow volume rendering.
  the idea is like in `VLevel::NeedProperLightTraceAt()`, only
  we know (almost) exact sectors light can touch, so we can
  do our checks here.

  checks:
    if sector has more than one region, check if light is crossing
    at least one. if it is, mark this light as shadowing.

    one-sided walls are not interesting: they're blocking everything.
    previous region check will make sure that those walls are really blocking.
    but note the fact that we seen such wall.

    if we have a two-sided wall, check if we can see its backsector
    (we have this info in BspVis). if not, don't bother with this wall anymore.
    this is safe, as we won't see shadows in that area anyway.

    now, we have a two-sided wall that is interesting. check if light can touch
    midtex of this wall. if there is no midtex contact, count this wall as
    one-sided.

    ok, now we have a contact with midtex. if we've seen some one-sided wall,
    assume that this light can cast a shadow (corner, for example), and
    mark this light as shadowing.

    last, check if we have elevation change between sectors. if it is there,
    mark this light as shadowing.
*/

// i'll move these to renderer class later
static bool doShadows; // true: don't do more checks
static bool seen1SWall;
static bool seen2SWall;
static bool hasAnyLitSurfaces;


//==========================================================================
//
//  IsTouchingSectorRegion
//
//==========================================================================
static bool IsTouchingSectorRegion (const sector_t *sector, const TVec &point, const float radius) {
  for (const sec_region_t *gap = sector->botregion; gap; gap = gap->next) {
    // assume that additive floor/ceiling is translucent, and doesn't block
    //FIXME: this is not true now, shadow volume renderer should be fixed
    if (!(gap->floor->flags&SPF_ADDITIVE)) {
      // check if we are crossing the floor
      if (gap->floor->SphereTouches(point, radius)) return true;
    }
    if (!(gap->ceiling->flags&SPF_ADDITIVE)) {
      // check if we are crossing the floor
      if (gap->ceiling->SphereTouches(point, radius)) return true;
    }
  }
  return false;
}


//==========================================================================
//
//  VAdvancedRenderLevel::AddClipSubsector
//
//==========================================================================
void VAdvancedRenderLevel::AddClipSubsector (const subsector_t *sub) {
  //LightClip.ClipLightAddSubsectorSegs(sub, CurrLightPos, CurrLightRadius);
  if (doShadows) return; // already determined

  // check sector regions, if there are more than one
  if (sub->sector->botregion->next) {
    if (IsTouchingSectorRegion(sub->sector, CurrLightPos, CurrLightRadius)) {
      // oops
      doShadows = true;
      return;
    }
  } else if (!hasAnyLitSurfaces) {
    hasAnyLitSurfaces = IsTouchingSectorRegion(sub->sector, CurrLightPos, CurrLightRadius);
  }

  //FIXME: while our BSP renderer is not precise, we have to skip some cheks

  // check walls
  const seg_t *seg = &Level->Segs[sub->firstline];
  for (int count = sub->numlines; count--; ++seg) {
    const line_t *ldef = seg->linedef;
    if (!ldef) continue; // minisegs are boring
    const float dist = DotProduct(CurrLightPos, seg->normal)-seg->dist;
    if (fabsf(dist) >= CurrLightRadius) continue; // totally uninteresting
    if ((ldef->flags&ML_TWOSIDED) == 0) {
      // one-sided wall: if it is not facing light, it can create a shadow
      if (dist <= 0) {
        // oops
        if (LightClip.ClipLightCheckSeg(seg, CurrLightPos, CurrLightRadius)) {
          doShadows = true;
          return;
        }
      }
      // if it is facing light, note it
      if (!seen1SWall) {
        if (!LightClip.ClipLightCheckSeg(seg, CurrLightPos, CurrLightRadius)) continue;
        hasAnyLitSurfaces = true;
        seen1SWall = true;
        if (seen1SWall && seen2SWall) {
          // oops
          doShadows = true;
          return;
        }
      }
    } else {
      // two-sided wall: check if we can see backsector
      if (seg->frontsector == seg->backsector) continue; // deep water; don't know what to do with it yet
      // do partner subsector check
      // we should have partner seg
      if (!seg->partner || seg->partner == seg || seg->partner->front_sub == sub) {
        // dunno
        if (dist <= 0) {
          // oops
          if (LightClip.ClipLightCheckSeg(seg, CurrLightPos, CurrLightRadius)) {
            doShadows = true;
            return;
          }
        } else {
          if (!hasAnyLitSurfaces) hasAnyLitSurfaces = LightClip.ClipLightCheckSeg(seg, CurrLightPos, CurrLightRadius);
        }
        continue;
      }
      // this check is wrong due to... what?!
      // somehow, some lights are visible when they shouldn't be
      unsigned snum = (unsigned)(ptrdiff_t)(seg->partner->front_sub-Level->Subsectors);
      if (!(BspVis[snum>>3]&(1u<<(snum&7)))) {
        // we cannot see anything behind this wall, so don't bother
        // don't mark it as solid too, it doesn't matter
        //FIXME: this causes some glitches, so check if we can see current sector
        //snum = (unsigned)(ptrdiff_t)(sub-Level->Subsectors);
        //if (!(BspVis[snum>>3]&(1u<<(snum&7))))
        {
          continue;
        }
      }
      // check if we can touch midtex
      const sector_t *fsec = seg->frontsector;
      if (CurrLightPos.z+CurrLightRadius <= fsec->botregion->floor->minz ||
          CurrLightPos.z-CurrLightRadius >= fsec->topregion->ceiling->maxz)
      {
        // cannot possibly touch midtex, consider this wall solid
        if (dist <= 0) {
          if (LightClip.ClipLightCheckSeg(seg, CurrLightPos, CurrLightRadius)) {
            // oops
            doShadows = true;
            return;
          }
        } else if (!seen1SWall) {
          if (LightClip.ClipLightCheckSeg(seg, CurrLightPos, CurrLightRadius)) {
            hasAnyLitSurfaces = true;
            seen1SWall = true;
          }
        }
        continue;
      }
      if (!LightClip.ClipLightCheckSeg(seg, CurrLightPos, CurrLightRadius)) continue;
      hasAnyLitSurfaces = true;
      //GCon->Logf("MIDTOUCH! seen1SWall=%d", (int)seen1SWall);
      // if we've seen some one-sided wall, assume shadowing
      if (seen1SWall) {
        doShadows = true;
        return;
      }
      const sector_t *bsec = seg->backsector;
      const sec_region_t *fbotr = fsec->botregion;
      const sec_region_t *bbotr = bsec->botregion;
      // if we have elevation change, and the light is
      // touching any of current region floor/ceiling,
      // or any of back sector floor/ceiling
      // first, elevation change
      if (!fbotr->next && !bbotr->next) {
        // two sectors with one region each, check for change
        // floor
        if (fbotr->floor->minz != bbotr->floor->minz ||
            fbotr->floor->minz != bbotr->floor->maxz ||
            fbotr->floor->maxz != bbotr->floor->minz ||
            fbotr->floor->maxz != bbotr->floor->maxz)
        {
          // floor elevation changed, check if we're touching any floor
          if (fbotr->floor->SphereTouches(CurrLightPos, CurrLightRadius) ||
              bbotr->floor->SphereTouches(CurrLightPos, CurrLightRadius))
          {
            // oops
            doShadows = true;
            return;
          }
        }
        // ceiling
        if (fbotr->ceiling->minz != bbotr->ceiling->minz ||
            fbotr->ceiling->minz != bbotr->ceiling->maxz ||
            fbotr->ceiling->maxz != bbotr->ceiling->minz ||
            fbotr->ceiling->maxz != bbotr->ceiling->maxz)
        {
          // ceiling elevation changed, check if we're touching any ceiling
          if (fbotr->ceiling->SphereTouches(CurrLightPos, CurrLightRadius) ||
              bbotr->ceiling->SphereTouches(CurrLightPos, CurrLightRadius))
          {
            // oops
            doShadows = true;
            return;
          }
        }
      } else {
        // sectors with multiple regions assumed "changed"
        if (IsTouchingSectorRegion(fsec, CurrLightPos, CurrLightRadius) ||
            IsTouchingSectorRegion(bsec, CurrLightPos, CurrLightRadius))
        {
          // oops
          doShadows = true;
          return;
        }
      }
    }
  }
}


//==========================================================================
//
//  VAdvancedRenderLevel::BuildLightVis
//
//==========================================================================
void VAdvancedRenderLevel::BuildLightVis (int bspnum, const float *bbox) {
  if (LightClip.ClipIsFull()) return;

  if (!LightClip.ClipLightIsBBoxVisible(bbox, CurrLightPos, CurrLightRadius)) return;

  if (bspnum == -1) {
    const unsigned SubNum = 0;
    const subsector_t *Sub = &Level->Subsectors[SubNum];
    if (!Sub->sector->linecount) return; // skip sectors containing original polyobjs
    if (!LightClip.ClipLightCheckSubsector(Sub, CurrLightPos, CurrLightRadius)) {
      LightClip.ClipLightAddSubsectorSegs(Sub, CurrLightPos, CurrLightRadius);
      return;
    }
    //LightVis[SubNum>>3] |= 1<<(SubNum&7);
    UPDATE_LIGHTVIS(SubNum);
    AddClipSubsector(Sub);
    LightClip.ClipLightAddSubsectorSegs(Sub, CurrLightPos, CurrLightRadius);
    return;
  }

  // found a subsector?
  if (!(bspnum&NF_SUBSECTOR)) {
    const node_t *bsp = &Level->Nodes[bspnum];
    // decide which side the view point is on
    const float dist = DotProduct(CurrLightPos, bsp->normal)-bsp->dist;
    if (dist > CurrLightRadius) {
      // light is completely on front side
      return BuildLightVis(bsp->children[0], bsp->bbox[0]);
    } else if (dist < -CurrLightRadius) {
      // light is completely on back side
      return BuildLightVis(bsp->children[1], bsp->bbox[1]);
    } else {
      //unsigned side = (unsigned)bsp->PointOnSide(CurrLightPos);
      unsigned side = (unsigned)(dist <= 0); //(unsigned)bsp->PointOnSide(CurrLightPos);
      // recursively divide front space
      BuildLightVis(bsp->children[side], bsp->bbox[side]);
      // possibly divide back space
      side ^= 1;
      return BuildLightVis(bsp->children[side], bsp->bbox[side]);
    }
  } else {
    const unsigned SubNum = (unsigned)(bspnum&(~NF_SUBSECTOR));
    const subsector_t *Sub = &Level->Subsectors[SubNum];
    if (!Sub->sector->linecount) return; // skip sectors containing original polyobjs
    if (!LightClip.ClipLightCheckSubsector(Sub, CurrLightPos, CurrLightRadius)) {
      LightClip.ClipLightAddSubsectorSegs(Sub, CurrLightPos, CurrLightRadius);
      return;
    }
    //LightVis[SubNum>>3] |= 1<<(SubNum&7);
    UPDATE_LIGHTVIS(SubNum);
    AddClipSubsector(Sub);
    LightClip.ClipLightAddSubsectorSegs(Sub, CurrLightPos, CurrLightRadius);
  }
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
  seg_t *seg = dseg->seg;
  if (!seg->linedef) return; // miniseg

  // note: we don't want to filter out shadows that are behind
  const float dist = DotProduct(CurrLightPos, seg->normal)-seg->dist;
  //if (dist < -CurrLightRadius || dist > CurrLightRadius) return; // light is too far away
  if (fabsf(dist) >= CurrLightRadius) return;

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
  const sec_plane_t &plane = *ssurf->secplane;

  if (!plane.pic) return;

  // note: we don't want to filter out shadows that are behind
  const float dist = DotProduct(CurrLightPos, plane.normal)-plane.dist;
  //if (dist < -CurrLightRadius || dist > CurrLightRadius) return; // light is too far away
  if (fabsf(dist) >= CurrLightRadius) return;

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
  const sec_plane_t &plane = *ssurf->secplane;

  if (!plane.pic) return;

  const float dist = DotProduct(CurrLightPos, plane.normal)-plane.dist;
  //if (dist <= -CurrLightRadius || dist > CurrLightRadius) return; // light is in back side or on plane
  if (fabsf(dist) >= CurrLightRadius) return;

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

  if (r_max_lights >= 0 && LightsRendered >= r_max_lights) return;

  /*
  bool doShadows = true;

  if (r_advlight_opt_trace && !Level->NeedProperLightTraceAt(Pos, Radius)) {
    //GCon->Log("some light doesn't need shadows");
    //return;
    if (dbg_adv_light_notrace_mark) Colour = 0xffff0000U;
    doShadows = false;
  }
  */
  doShadows = (Radius < 12.0f); // arbitrary; set "do shadows" flag to skip checks
  seen1SWall = false;
  seen2SWall = false;
  hasAnyLitSurfaces = false;

  // build vis data for light
  LightClip.ClearClipNodes(CurrLightPos, Level);
  memset(LightVis, 0, VisSize);
  if (!r_advlight_opt_separate_vis) memset(LightBspVis, 0, VisSize);
  HasLightIntersection = false;
  BuildLightVis(Level->NumNodes-1, dummy_bbox);
  if (!r_advlight_opt_separate_vis && !HasLightIntersection) return;
  if (Radius < 12.0f) {
    doShadows = false;
  } else {
    if (!doShadows && !hasAnyLitSurfaces) return;
  }

  // create combined light and view visibility
  if (r_advlight_opt_separate_vis) {
    //memset(LightBspVis, 0, VisSize);
    bool HaveIntersect = false;
    for (int i = 0; i < VisSize; ++i) {
      LightBspVis[i] = BspVis[i]&LightVis[i];
      if (LightBspVis[i]) HaveIntersect = true;
    }
    if (!HaveIntersect) return;
  }

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

  // setup light scissor rectangle
  if (r_advlight_opt_scissor) {
    hasScissor = Drawer->SetupLightScissor(Pos, Radius, scoord);
    if (hasScissor <= 0) {
      // something is VERY wrong (0), or scissor is empty (-1)
      Drawer->ResetScissor();
      if (!hasScissor) return; // undefined scissor
      //return;
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

  ResetMobjsLightCount(true);
  // do shadow volumes
  Drawer->BeginLightShadowVolumes(hasScissor, scoord);
  LightClip.ClearClipNodes(CurrLightPos, Level);
  if (doShadows && r_max_shadow_segs_all) {
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

  /*if (hasScissor)*/ Drawer->ResetScissor();
}
