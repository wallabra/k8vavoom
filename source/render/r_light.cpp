//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

#include "gamedefs.h"
#include "r_local.h"


// ////////////////////////////////////////////////////////////////////////// //
int r_dlightframecount;
bool r_light_add;

int light_reset_surface_cache = 0;

vuint32 blocklightsr[18*18];
vuint32 blocklightsg[18*18];
vuint32 blocklightsb[18*18];
static vuint32 blockaddlightsr[18*18];
static vuint32 blockaddlightsg[18*18];
static vuint32 blockaddlightsb[18*18];

// subtractive
static vuint32 blocklightsrS[18*18];
static vuint32 blocklightsgS[18*18];
static vuint32 blocklightsbS[18*18];

byte light_remap[256];
int light_mem;

VCvarB r_darken("r_darken", true, "Darken level to better match original DooM?", CVAR_Archive);
VCvarI r_ambient("r_ambient", "0", "Minimal ambient light.", CVAR_Archive);
VCvarB r_allow_ambient("r_allow_ambient", true, "Allow ambient lights?", CVAR_Archive);
VCvarB r_extrasamples("r_extrasamples", false, "Do static lightmap filtering?", CVAR_Archive);
VCvarB r_dynamic("r_dynamic", true, "Allow dynamic lights?", CVAR_Archive);
VCvarB r_dynamic_clip("r_dynamic_clip", true, "Clip dynamic lights?", CVAR_Archive);
VCvarB r_dynamic_clip_more("r_dynamic_clip_more", true, "Do some extra checks when clipping dynamic lights?", CVAR_Archive);
VCvarB r_static_lights("r_static_lights", true, "Allow static lights?", CVAR_Archive);
VCvarB r_static_add("r_static_add", true, "Are static lights additive?", CVAR_Archive);
VCvarF r_specular("r_specular", "0.1", "Specular light.", CVAR_Archive);

VCvarF r_light_filter_dynamic_coeff("r_light_filter_dynamic_coeff", "0.8", "How close dynamic lights should be to be filtered out?\n(0.6-0.9 is usually ok).", CVAR_Archive);

VCvarB r_allow_subtractive_lights("r_allow_subtractive_lights", true, "Are subtractive lights allowed?", /*CVAR_Archive*/0);

extern VCvarF r_lights_radius;
extern VCvarF r_lights_radius_sight_check;


// ////////////////////////////////////////////////////////////////////////// //
static TVec smins, smaxs;
static TVec worldtotex[2];
static TVec textoworld[2];
static TVec texorg;
static TVec surfpt[18 * 18 * 4];
static int  numsurfpt;
static bool points_calculated;
static float lightmap[18 * 18 * 4];
static float lightmapr[18 * 18 * 4];
static float lightmapg[18 * 18 * 4];
static float lightmapb[18 * 18 * 4];
static bool light_hit;
static const vuint8 *facevis;
static bool is_coloured;

static int c_bad;


//==========================================================================
//
//  getSurfLightLevelInt
//
//==========================================================================
static inline int getSurfLightLevelInt (const surface_t *surf) {
  if (!surf || !r_allow_ambient) return 0;
  int slins = (surf->Light>>24)&0xff;
  slins = MAX(slins, r_ambient);
  if (slins > 255) slins = 255;
  return slins;
}


//==========================================================================
//
//  getSurfLightLevelInt
//
//==========================================================================
static inline vuint32 fixSurfLightLevel (const surface_t *surf) {
  if (!surf || !r_allow_ambient) return 0;
  int slins = (surf->Light>>24)&0xff;
  slins = MAX(slins, r_ambient);
  if (slins > 255) slins = 255;
  return (surf->Light&0xffffff)|(slins<<24);
}


//==========================================================================
//
// VRenderLevelShared::AddStaticLight
//
//==========================================================================
void VRenderLevelShared::AddStaticLight (const TVec &origin, float radius, vuint32 colour) {
  guard(VRenderLevelShared::AddStaticLight);
  staticLightsFiltered = false;
  light_t &L = Lights.Alloc();
  L.origin = origin;
  L.radius = radius;
  L.colour = colour;
  L.leafnum = Level->PointInSubsector(origin) - Level->Subsectors;
  L.active = true;
  unguard;
}


//==========================================================================
//
// VRenderLevel::CalcMinMaxs
//
//==========================================================================
void VRenderLevel::CalcMinMaxs (surface_t *surf) {
  guard(VRenderLevel::CalcMinMaxs);
  smins = TVec(99999.0, 99999.0, 99999.0);
  smaxs = TVec(-999999.0, -999999.0, -999999.0);
  for (int i = 0; i < surf->count; ++i) {
    TVec &v = surf->verts[i];
    if (smins.x > v.x) smins.x = v.x;
    if (smins.y > v.y) smins.y = v.y;
    if (smins.z > v.z) smins.z = v.z;
    if (smaxs.x < v.x) smaxs.x = v.x;
    if (smaxs.y < v.y) smaxs.y = v.y;
    if (smaxs.z < v.z) smaxs.z = v.z;
  }
  unguard;
}


//==========================================================================
//
//  VRenderLevel::CastRay
//
//  Returns the distance between the points, or -1 if blocked
//
//==========================================================================
float VRenderLevel::CastRay (const TVec &p1, const TVec &p2, float squaredist) {
  guard(VRenderLevel::CastRay);
  linetrace_t Trace;

  TVec delta = p2 - p1;
  float t = DotProduct(delta, delta);
  if (t > squaredist) return -1; // too far away

  if (!Level->TraceLine(Trace, p1, p2, SPF_NOBLOCKSIGHT)) return -1; // ray was blocked

  if (t == 0) t = 1; // don't blow up...
  return sqrt(t);
  unguard;
}


//==========================================================================
//
// VRenderLevel::CalcFaceVectors
//
// fills in texorg, worldtotex. and textoworld
//
//==========================================================================
void VRenderLevel::CalcFaceVectors (surface_t *surf) {
  guard(VRenderLevel::CalcFaceVectors);
  TVec texnormal;

  texinfo_t *tex = surf->texinfo;

  // convert from float to vec_t
  worldtotex[0] = tex->saxis;
  worldtotex[1] = tex->taxis;

  // calculate a normal to the texture axis
  // points can be moved along this without changing their S/T
  texnormal.x = tex->taxis.y*tex->saxis.z-tex->taxis.z*tex->saxis.y;
  texnormal.y = tex->taxis.z*tex->saxis.x-tex->taxis.x*tex->saxis.z;
  texnormal.z = tex->taxis.x*tex->saxis.y-tex->taxis.y*tex->saxis.x;
  texnormal = Normalise(texnormal);

  // flip it towards plane normal
  float distscale = DotProduct(texnormal, surf->plane->normal);
  if (!distscale) Host_Error("Texture axis perpendicular to face");
  if (distscale < 0) {
    distscale = -distscale;
    texnormal = -texnormal;
  }

  // distscale is the ratio of the distance along the texture normal to
  // the distance along the plane normal
  distscale = 1.0f/distscale;

  for (int i = 0; i < 2; ++i) {
    float len = Length(worldtotex[i]);
    float dist = DotProduct(worldtotex[i], surf->plane->normal);
    dist *= distscale;
    textoworld[i] = worldtotex[i]-dist*texnormal;
    textoworld[i] = textoworld[i]*(1.0f/len)*(1.0f/len);
  }

  // calculate texorg on the texture plane
  for (int i = 0; i < 3; ++i) texorg[i] = -tex->soffs*textoworld[0][i]-tex->toffs*textoworld[1][i];

  // project back to the face plane
  {
    float dist = DotProduct(texorg, surf->plane->normal)-surf->plane->dist-1;
    dist *= distscale;
    texorg = texorg-dist*texnormal;
  }
  unguard;
}


//==========================================================================
//
//  VRenderLevel::CalcPoints
//
// for each texture aligned grid point, back project onto the plane
// to get the world xyz value of the sample point
//
//==========================================================================
void VRenderLevel::CalcPoints (surface_t *surf) {
  guard(VRenderLevel::CalcPoints);
  int i;
  int s, t;
  int w, h;
  int step;
  float starts, startt, us, ut;
  TVec *spt;
  TVec facemid;
  linetrace_t Trace;

  // fill in surforg
  // the points are biased towards the centre of the surface
  // to help avoid edge cases just inside walls
  spt = surfpt;
  float mids = surf->texturemins[0]+surf->extents[0]/2.0f;
  float midt = surf->texturemins[1]+surf->extents[1]/2.0f;

  facemid = texorg+textoworld[0]*mids+textoworld[1]*midt;

  if (r_extrasamples) {
    // extra filtering
    w = ((surf->extents[0]>>4)+1)*2;
    h = ((surf->extents[1]>>4)+1)*2;
    starts = surf->texturemins[0]-8;
    startt = surf->texturemins[1]-8;
    step = 8;
  } else {
    w = (surf->extents[0]>>4)+1;
    h = (surf->extents[1]>>4)+1;
    starts = surf->texturemins[0];
    startt = surf->texturemins[1];
    step = 16;
  }

  numsurfpt = w*h;
  for (t = 0; t < h; ++t) {
    for (s = 0; s < w; ++s, ++spt) {
      us = starts+s*step;
      ut = startt+t*step;

      // if a line can be traced from surf to facemid, the point is good
      for (i = 0; i < 6; ++i) {
        // calculate texture point
        *spt = texorg+textoworld[0]*us+textoworld[1]*ut;
        const TVec fms = facemid-(*spt);
        if (length2DSquared(fms) < 0.001) break; // same point, got it
        if (Level->TraceLine(Trace, facemid, *spt, SPF_NOBLOCKSIGHT)) break; // got it
        if (i&1) {
          if (us > mids) {
            us -= 8;
            if (us < mids) us = mids;
          } else {
            us += 8;
            if (us > mids) us = mids;
          }
        } else {
          if (ut > midt) {
            ut -= 8;
            if (ut < midt) ut = midt;
          } else {
            ut += 8;
            if (ut > midt) ut = midt;
          }
        }

        // move surf 8 pixels towards the centre
        *spt += 8*Normalise(fms);
      }
      if (i == 2) ++c_bad;
    }
  }
  unguard;
}


//==========================================================================
//
// VRenderLevel::SingleLightFace
//
//==========================================================================
void VRenderLevel::SingleLightFace (light_t *light, surface_t *surf) {
  guard(VRenderLevel::SingleLightFace);
  float dist;
  TVec incoming;
  float angle;
  float add;
  TVec *spt;
  int c;
  float squaredist;
  float rmul, gmul, bmul;

  // check potential visibility
  if (!(facevis[light->leafnum>>3]&(1<<(light->leafnum&7)))) return;

  // check bounding box
  if (light->origin.x+light->radius < smins.x ||
      light->origin.x-light->radius > smaxs.x ||
      light->origin.y+light->radius < smins.y ||
      light->origin.y-light->radius > smaxs.y ||
      light->origin.z+light->radius < smins.z ||
      light->origin.z-light->radius > smaxs.z)
  {
    return;
  }

  dist = DotProduct(light->origin, surf->plane->normal) - surf->plane->dist;

  // don't bother with lights behind the surface
  if (dist <= -0.1) return;

  // don't bother with light too far away
  if (dist > light->radius) return;

  // calc points only when surface may be lit by a light
  if (!points_calculated) {
    CalcFaceVectors(surf);
    CalcPoints(surf);

    memset(lightmap, 0, numsurfpt*4);
    memset(lightmapr, 0, numsurfpt*4);
    memset(lightmapg, 0, numsurfpt*4);
    memset(lightmapb, 0, numsurfpt*4);
    points_calculated = true;
  }

  // check it for real
  spt = surfpt;
  squaredist = light->radius*light->radius;
  rmul = ((light->colour>>16)&255)/255.0;
  gmul = ((light->colour>>8)&255)/255.0;
  bmul = (light->colour & 255)/255.0;
  for (c = 0; c < numsurfpt; ++c, ++spt) {
    dist = CastRay(light->origin, *spt, squaredist);
    if (dist < 0) continue; // light doesn't reach

    incoming = NormaliseSafe(light->origin-(*spt));
    angle = DotProduct(incoming, surf->plane->normal);

    angle = 0.5+0.5*angle;
    add = light->radius-dist;
    add *= angle;
    if (add < 0) continue;
    lightmap[c] += add;
    lightmapr[c] += add*rmul;
    lightmapg[c] += add*gmul;
    lightmapb[c] += add*bmul;
    // ignore really tiny lights
    if (lightmap[c] > 1) {
      light_hit = true;
      if (light->colour != 0xffffffff) is_coloured = true;
    }
  }
  unguard;
}


//==========================================================================
//
// VRenderLevel::LightFace
//
//==========================================================================
void VRenderLevel::LightFace (surface_t *surf, subsector_t *leaf) {
  guard(VRenderLevel::LightFace);
  int i, s, t, w, h;
  float total;

  facevis = Level->LeafPVS(leaf);
  points_calculated = false;
  light_hit = false;
  is_coloured = false;

  // cast all lights
  CalcMinMaxs(surf);
  if (r_static_lights) {
    for (i = 0; i < Lights.Num(); ++i) SingleLightFace(&Lights[i], surf);
  }

  if (!light_hit) {
    // no light hitting it
    if (surf->lightmap) {
      Z_Free(surf->lightmap);
      surf->lightmap = nullptr;
    }
    if (surf->lightmap_rgb) {
      Z_Free(surf->lightmap_rgb);
      surf->lightmap_rgb = nullptr;
    }
    return;
  }

  w = (surf->extents[0] >> 4) + 1;
  h = (surf->extents[1] >> 4) + 1;

  // if the surface already has a lightmap, we will reuse it,
  // otherwiese we must allocate a new block
  if (is_coloured) {
    if (surf->lightmap_rgb) Z_Free(surf->lightmap_rgb);
    surf->lightmap_rgb = (rgb_t *)Z_Malloc(w*h*3);
    light_mem += w*h*3;

    i = 0;
    for (t = 0; t < h; ++t) {
      for (s = 0; s < w; ++s, ++i) {
        if (r_extrasamples) {
          // filtered sample
          total = lightmapr[t*w*4+s*2]+
              lightmapr[t*2*w*2+s*2+1]+
              lightmapr[(t*2+1)*w*2+s*2]+
              lightmapr[(t*2+1)*w*2+s*2+1];
          total *= 0.25;
        } else {
          total = lightmapr[i];
        }
        if (total > 255) total = 255;
        if (total < 0) Sys_Error("light < 0");
        surf->lightmap_rgb[i].r = byte(total);

        if (r_extrasamples) {
          // filtered sample
          total = lightmapg[t*w*4+s*2]+
              lightmapg[t*2*w*2+s*2+1]+
              lightmapg[(t*2+1)*w*2+s*2]+
              lightmapg[(t*2+1)*w*2+s*2+1];
          total *= 0.25;
        } else {
          total = lightmapg[i];
        }
        if (total > 255) total = 255;
        if (total < 0) Sys_Error("light < 0");
        surf->lightmap_rgb[i].g = byte(total);

        if (r_extrasamples) {
          // filtered sample
          total = lightmapb[t*w*4+s*2]+
              lightmapb[t*2*w*2+s*2+1]+
              lightmapb[(t*2+1)*w*2+s*2]+
              lightmapb[(t*2+1)*w*2+s*2+1];
          total *= 0.25;
        } else {
          total = lightmapb[i];
        }
        if (total > 255) total = 255;
        if (total < 0) Sys_Error("light < 0");
        surf->lightmap_rgb[i].b = byte(total);
      }
    }
  } else {
    if (surf->lightmap_rgb) {
      Z_Free(surf->lightmap_rgb);
      surf->lightmap_rgb = nullptr;
    }
  }

  if (surf->lightmap) Z_Free(surf->lightmap);
  surf->lightmap = (byte *)Z_Malloc(w*h);
  light_mem += w*h;

  i = 0;
  for (t = 0; t < h; ++t) {
    for (s = 0; s < w; ++s, ++i) {
      if (r_extrasamples) {
        // filtered sample
        total = lightmap[t*w*4+s*2]+
            lightmap[t*2*w*2+s*2+1]+
            lightmap[(t*2+1)*w*2+s*2]+
            lightmap[(t*2+1)*w*2+s*2+1];
        total *= 0.25;
      } else {
        total = lightmap[i];
      }
      if (total > 255) total = 255;
      if (total < 0) Sys_Error("light < 0");
      surf->lightmap[i] = byte(total);
    }
  }
  unguard;
}


//**************************************************************************
//**
//**  DYNAMIC LIGHTS
//**
//**************************************************************************

//==========================================================================
//
// VRenderLevelShared::AllocDlight
//
//==========================================================================
dlight_t *VRenderLevelShared::AllocDlight (VThinker *Owner, const TVec &lorg, float radius) {
  guard(VRenderLevelShared::AllocDlight);

  dlight_t *dlowner = nullptr;
  dlight_t *dldying = nullptr;
  dlight_t *dlreplace = nullptr;
  dlight_t *dlbestdist = nullptr;
  float bestdist = length2DSquared(lorg-cl->ViewOrg);
  if (radius < 0) radius = 0;

  float coeff = r_light_filter_dynamic_coeff;
  if (coeff <= 0.1) coeff = 0.1; else if (coeff > 1) coeff = 1;

  float radsq = (radius < 1 ? 32*32 : radius*radius*coeff);
  if (radsq < 32*32) radsq = 32*32;

  // if this is player's dlight, never drop it
  bool isPlr = false;
  if (Owner) {
    static VClass *eclass = nullptr;
    if (!eclass) eclass = VClass::FindClass("Entity");
    if (eclass && Owner->IsA(eclass)) {
      VEntity *e = (VEntity *)Owner;
      isPlr = ((e->EntityFlags&VEntity::EF_IsPlayer) != 0);
    }
  }

  if (!isPlr) {
    // don't add too far-away lights
    if (length2DSquared(cl->ViewOrg-lorg) >= r_lights_radius*r_lights_radius) return nullptr;
    if (r_dynamic_clip && Level->VisData) {
      subsector_t *sub = lastDLightViewSub;
      if (!sub || lastDLightView.x != cl->ViewOrg.x || lastDLightView.y != cl->ViewOrg.y || lastDLightView.z != cl->ViewOrg.z) {
        lastDLightView = cl->ViewOrg;
        lastDLightViewSub = sub = Level->PointInSubsector(cl->ViewOrg);
      }
      const vuint8 *dyn_facevis = Level->LeafPVS(sub);
      auto leafnum = Level->PointInSubsector(lorg)-Level->Subsectors;
      // check potential visibility
      if (!(dyn_facevis[leafnum>>3]&(1<<(leafnum&7)))) {
        //fprintf(stderr, "DYNLIGHT rejected by PVS\n");
        return nullptr;
      }
    }
  }

  // look for any free slot (or free one if necessary)
  dlight_t *dl = DLights;
  for (int i = 0; i < MAX_DLIGHTS; ++i, ++dl) {
    // replace dlight of the same owner
    // (but keep looping, 'cause we may want to drop this light altogether)
    if (!dlowner && Owner && dl->Owner == Owner) {
      dlowner = dl;
      if (isPlr) break;
      continue;
    }
    // remove dead lights
    if (dl->die < Level->Time) dl->radius = 0;
    // unused light?
    if (dl->radius <= 0) {
      dl->flags = 0;
      if (!dldying) dldying = dl;
      continue;
    }
    // don't replace player's lights
    if (dl->flags&dlight_t::PlayerLight) continue;
    // replace furthest light
    float dist = length2DSquared(dl->origin-cl->ViewOrg);
    if (dist > bestdist) {
      bestdist = dist;
      dlbestdist = dl;
    }
    // check if we already have dynamic light around new origin
    if (!isPlr) {
      float dd = length2DSquared(dl->origin-lorg);
      if (dd <= 6) {
        if (radius > 0 && dl->radius > radius) return nullptr;
        dlreplace = dl;
        //break; // stop searching, we have a perfect candidate
      } else if (dd < radsq) {
        // if existing light radius is greater than new radius, drop new light, 'cause
        // we have too much lights around one point (prolly due to several things at one place)
        if (radius > 0 && dl->radius > radius) return nullptr;
        // otherwise, replace this light
        dlreplace = dl;
        //break; // stop searching, we have a perfect candidate
      }
    }
  }

  if (dlowner) {
    // remove replaced light
    if (dlreplace && dlreplace != dlowner) memset((void *)dlreplace, 0, sizeof(*dlreplace));
    dl = dlowner;
  } else {
    dl = dlreplace;
    if (!dl) { dl = dldying; if (!dl) { dl = dlbestdist; if (!dl) return nullptr; } }
  }

  // clean new light, and return it
  memset((void *)dl, 0, sizeof(*dl));
  dl->Owner = Owner;
  dl->origin = lorg;
  dl->radius = radius;
  dl->type = DLTYPE_Point;
  if (isPlr) dl->flags |= dlight_t::PlayerLight;
  return dl;

  unguard;
}


//==========================================================================
//
// VRenderLevelShared::DecayLights
//
//==========================================================================
void VRenderLevelShared::DecayLights (float time) {
  guard(VRenderLevelShared::DecayLights);
  dlight_t *dl = DLights;
  for (int i = 0; i < MAX_DLIGHTS; ++i, ++dl) {
    if (!dl->radius || dl->die < Level->Time) continue;
    dl->radius -= time*dl->decay;
    if (dl->radius <= 0) {
      dl->radius = 0;
      dl->flags = 0;
    }
  }
  unguard;
}


//==========================================================================
//
//  VRenderLevel::MarkLights
//
//==========================================================================
void VRenderLevel::MarkLights (dlight_t *light, int bit, int bspnum) {
  guard(VRenderLevel::MarkLights);
  if (bspnum&NF_SUBSECTOR) {
    int num;

    if (bspnum == -1) {
      num = 0;
    } else {
      num = bspnum&(~NF_SUBSECTOR);
    }
    subsector_t *ss = &Level->Subsectors[num];

    if (r_dynamic_clip && Level->VisData) {
      const vuint8 *dyn_facevis = Level->LeafPVS(ss);
      int leafnum = Level->PointInSubsector(light->origin)-Level->Subsectors;
      // check potential visibility
      if (!(dyn_facevis[leafnum >> 3] & (1 << (leafnum & 7)))) return;
    }

    if (ss->dlightframe != r_dlightframecount) {
      ss->dlightbits = 0;
      ss->dlightframe = r_dlightframecount;
    }
    ss->dlightbits |= bit;
  } else {
    node_t *node = &Level->Nodes[bspnum];
    float dist = DotProduct(light->origin, node->normal)-node->dist;
    if (dist > -light->radius+light->minlight) MarkLights(light, bit, node->children[0]);
    if (dist < light->radius-light->minlight) MarkLights(light, bit, node->children[1]);
  }
  unguard;
}


//==========================================================================
//
// VRenderLevel::PushDlights
//
//==========================================================================
void VRenderLevel::PushDlights () {
  guard(VRenderLevel::PushDlights);
  if (GGameInfo->IsPaused() || (Level->LevelInfo->LevelInfoFlags2 & VLevelInfo::LIF2_Frozen)) return;
  ++r_dlightframecount;

  if (!r_dynamic) return;

  dlight_t *l = DLights;
  for (int i = 0; i < MAX_DLIGHTS; ++i, ++l) {
    if (!l->radius || l->die < Level->Time) continue;
    MarkLights(l, 1 << i, Level->NumNodes-1);
  }
  unguard;
}


//==========================================================================
//
//  VRenderLevel::LightPoint
//
//==========================================================================
vuint32 VRenderLevel::LightPoint (const TVec &p, VEntity *mobj) {
  guard(VRenderLevel::LightPoint);
  subsector_t *sub;
  subregion_t *reg;
  float l = 0, lr = 0, lg = 0, lb = 0;
  surface_t *surf;
  rgb_t *rgbtmp;

  if (FixedLight) return FixedLight|(FixedLight<<8)|(FixedLight<<16)|(FixedLight<<24);

  sub = Level->PointInSubsector(p);
  reg = sub->regions;
  if (reg) {
    while (reg->next) {
      float d = DotProduct(p, reg->floor->secplane->normal) - reg->floor->secplane->dist;
      if (d >= 0.0) break;
      reg = reg->next;
    }

    // region's base light
    if (r_allow_ambient) {
      l = reg->secregion->params->lightlevel+ExtraLight;
      if (r_darken) l = light_remap[MIN(255, (int)l)];
      if (l < r_ambient) l = r_ambient;
      l = MIN(255, l);
    } else {
      l = 0;
    }
    int SecLightColour = reg->secregion->params->LightColour;
    lr = ((SecLightColour>>16)&255)*l/255.0;
    lg = ((SecLightColour>>8)&255)*l/255.0;
    lb = (SecLightColour&255)*l/255.0;

    // light from floor's lightmap
    int s = (int)(DotProduct(p, reg->floor->texinfo.saxis)+reg->floor->texinfo.soffs);
    int t = (int)(DotProduct(p, reg->floor->texinfo.taxis)+reg->floor->texinfo.toffs);
    int ds, dt;
    for (surf = reg->floor->surfs; surf; surf = surf->next) {
      if (!surf->lightmap) continue;
      if (s < surf->texturemins[0] || t < surf->texturemins[1]) continue;

      ds = s - surf->texturemins[0];
      dt = t - surf->texturemins[1];

      if (ds > surf->extents[0] || dt > surf->extents[1]) continue;

      if (surf->lightmap_rgb) {
        l += surf->lightmap[(ds>>4)+(dt>>4)*((surf->extents[0]>>4)+1)];
        rgbtmp = &surf->lightmap_rgb[(ds>>4)+(dt>>4)*((surf->extents[0]>>4)+1)];
        lr += rgbtmp->r;
        lg += rgbtmp->g;
        lb += rgbtmp->b;
      } else {
        int ltmp = surf->lightmap[(ds>>4)+(dt>>4)*((surf->extents[0]>>4)+1)];
        l += ltmp;
        lr += ltmp;
        lg += ltmp;
        lb += ltmp;
      }
      break;
    }
  }

  // add dynamic lights
  if (sub->dlightframe == r_dlightframecount) {
    for (int i = 0; i < MAX_DLIGHTS; ++i) {
      if (!(sub->dlightbits&(1<<i))) continue;
      const dlight_t &dl = DLights[i];
      if (dl.type == DLTYPE_Subtractive && !r_allow_subtractive_lights) continue;
      if (r_dynamic_clip && Level->VisData) {
        const vuint8 *dyn_facevis = Level->LeafPVS(sub);
        int leafnum = Level->PointInSubsector(dl.origin)-Level->Subsectors;
        // check potential visibility
        if (!(dyn_facevis[leafnum>>3]&(1<<(leafnum&7)))) continue;
      }
      float add = (dl.radius-dl.minlight)-Length(p-dl.origin);
      if (add > 0) {
        if (r_dynamic_clip) {
          if (!RadiusCastRay(p, dl.origin, (mobj ? mobj->Radius : 0), false/*r_dynamic_clip_more*/)) continue;
        }
        if (dl.type == DLTYPE_Subtractive) add = -add;
        l += add;
        lr += add*((dl.colour>>16)&255)/255.0;
        lg += add*((dl.colour>>8)&255)/255.0;
        lb += add*(dl.colour&255)/255.0;
      }
    }
  }

  if (l > 255) l = 255; else if (l < 0) l = 0;
  if (lr > 255) lr = 255; else if (lr < 0) lr = 0;
  if (lg > 255) lg = 255; else if (lg < 0) lg = 0;
  if (lb > 255) lb = 255; else if (lb < 0) lb = 0;

  return ((int)l<<24)|((int)lr<<16)|((int)lg<<8)|((int)lb);
  unguard;
}


//==========================================================================
//
// VRenderLevel::AddDynamicLights
//
//==========================================================================
void VRenderLevel::AddDynamicLights (surface_t *surf) {
  guard(VRenderLevel::AddDynamicLights);
  float dist, rad, minlight, rmul, gmul, bmul;
  TVec impact, local;
  int smax, tmax;
  texinfo_t *tex;
  subsector_t *sub;
  int leafnum;
  float mids=0, midt=0;
  TVec facemid = TVec(0,0,0);
  bool pointsCalced = false;

  smax = (surf->extents[0]>>4)+1;
  tmax = (surf->extents[1]>>4)+1;
  tex = surf->texinfo;

  const float starts = surf->texturemins[0];
  const float startt = surf->texturemins[1];
  const float step = 16;

  for (int lnum = 0; lnum < MAX_DLIGHTS; ++lnum) {
    if (!(surf->dlightbits&(1<<lnum))) continue; // not lit by this light

    const dlight_t &dl = DLights[lnum];
    //if (dl.type == DLTYPE_Subtractive) GCon->Logf("***SUBTRACTIVE LIGHT!");
    if (dl.type == DLTYPE_Subtractive && !r_allow_subtractive_lights) continue;

    rad = dl.radius;
    dist = DotProduct(dl.origin, surf->plane->normal)-surf->plane->dist;
    if (r_dynamic_clip) {
      if (dist <= -0.1) continue;
    }

    rad -= fabs(dist);
    minlight = dl.minlight;
    if (rad < minlight) continue;
    minlight = rad-minlight;

    impact = dl.origin-surf->plane->normal*dist;

    if (r_dynamic_clip && Level->VisData) {
      sub = Level->PointInSubsector(impact);
      const vuint8 *dyn_facevis = Level->LeafPVS(sub);
      leafnum = Level->PointInSubsector(dl.origin)-Level->Subsectors;
      // check potential visibility
      if (!(dyn_facevis[leafnum>>3]&(1<<(leafnum&7)))) continue;
    }

    rmul = (dl.colour>>16)&255;
    gmul = (dl.colour>>8)&255;
    bmul = dl.colour&255;

    local.x = DotProduct(impact, tex->saxis)+tex->soffs;
    local.y = DotProduct(impact, tex->taxis)+tex->toffs;

    local.x -= starts;
    local.y -= startt;

    if (!pointsCalced && r_dynamic_clip && r_dynamic_clip_more) {
      pointsCalced = true;
      CalcFaceVectors(surf);
      mids = starts+surf->extents[0]/2.0f;
      midt = startt+surf->extents[1]/2.0f;
      facemid = texorg+textoworld[0]*mids+textoworld[1]*midt;
    }

    for (int t = 0; t < tmax; ++t) {
      int td = (int)local.y-t*16;
      if (td < 0) td = -td;
      for (int s = 0; s < smax; ++s) {
        // do more dynlight clipping
        if (r_dynamic_clip && r_dynamic_clip_more) {
          linetrace_t Trace;
          float us = starts+s*step;
          float ut = startt+t*step;
          TVec spt = texorg+textoworld[0]*us+textoworld[1]*ut;
          if (length2DSquared(spt-dl.origin) > 1) {
            if (!Level->TraceLine(Trace, dl.origin, spt, SPF_NOBLOCKSIGHT)) continue;
          }
        }
        //
        int sd = (int)local.x-s*16;
        if (sd < 0) sd = -sd;
        if (sd > td) {
          dist = sd+(td>>1);
        } else {
          dist = td+(sd>>1);
        }
        if (dist < minlight) {
          int i = t*smax+s;
          if (dl.type == DLTYPE_Subtractive) {
            //blocklightsS[i] += (rad-dist)*256.0;
            blocklightsrS[i] += (rad-dist)*rmul;
            blocklightsgS[i] += (rad-dist)*gmul;
            blocklightsbS[i] += (rad-dist)*bmul;
          } else {
            //blocklights[i] += (rad-dist)*256.0;
            blocklightsr[i] += (rad-dist)*rmul;
            blocklightsg[i] += (rad-dist)*gmul;
            blocklightsb[i] += (rad-dist)*bmul;
          }
          if (dl.colour != 0xffffffff) is_coloured = true;
        }
      }
    }
  }
  unguard;
}


//==========================================================================
//
// xblight
//
//==========================================================================
static inline int xblight (int add, int sub) {
  const int minlight = 256;
  const int maxlight = 0xff00;
  int t = 255*256-add+sub;
  //if (sub > 0) t = maxlight;
  if (t < minlight) t = minlight; else if (t > maxlight) t = maxlight;
  return t;
}


//==========================================================================
//
// VRenderLevel::BuildLightMap
//
// Combine and scale multiple lightmaps into the 8.8 format in blocklights
//
//==========================================================================
void VRenderLevel::BuildLightMap (surface_t *surf) {
  guard(VRenderLevel::BuildLightMap);
  int smax, tmax;
  int t;
  int i, size;
  byte *lightmap;
  rgb_t *lightmap_rgb;

  is_coloured = false;
  r_light_add = false;
  smax = (surf->extents[0]>>4)+1;
  tmax = (surf->extents[1]>>4)+1;
  size = smax*tmax;
  lightmap = surf->lightmap;
  lightmap_rgb = surf->lightmap_rgb;

  // clear to ambient
  t = getSurfLightLevelInt(surf);
  t <<= 8;
  int tR = ((surf->Light>>16)&255)*t/255;
  int tG = ((surf->Light>>8)&255)*t/255;
  int tB = (surf->Light&255)*t/255;
  if (tR != tG || tR != tB) is_coloured = true;

  for (i = 0; i < size; ++i) {
    //blocklights[i] = t;
    blocklightsr[i] = tR;
    blocklightsg[i] = tG;
    blocklightsb[i] = tB;
    blockaddlightsr[i] = blockaddlightsg[i] = blockaddlightsb[i] = 0;
    /*blocklightsS[i] =*/ blocklightsrS[i] = blocklightsgS[i] = blocklightsbS[i] = 0;
  }

  // add lightmap
  if (lightmap_rgb) {
    if (!lightmap) Sys_Error("RGB lightmap without uncoloured lightmap");
    is_coloured = true;
    for (i = 0; i < size; ++i) {
      //blocklights[i] += lightmap[i]<<8;
      blocklightsr[i] += lightmap_rgb[i].r<<8;
      blocklightsg[i] += lightmap_rgb[i].g<<8;
      blocklightsb[i] += lightmap_rgb[i].b<<8;
      if (!r_static_add) {
        if (blocklightsr[i] > 0xffff) blocklightsr[i] = 0xffff;
        if (blocklightsg[i] > 0xffff) blocklightsg[i] = 0xffff;
        if (blocklightsb[i] > 0xffff) blocklightsb[i] = 0xffff;
      }
    }
  } else if (lightmap) {
    for (i = 0; i < size; ++i) {
      t = lightmap[i]<<8;
      //blocklights[i] += t;
      blocklightsr[i] += t;
      blocklightsg[i] += t;
      blocklightsb[i] += t;
      if (!r_static_add) {
        if (blocklightsr[i] > 0xffff) blocklightsr[i] = 0xffff;
        if (blocklightsg[i] > 0xffff) blocklightsg[i] = 0xffff;
        if (blocklightsb[i] > 0xffff) blocklightsb[i] = 0xffff;
      }
    }
  }

  // add all the dynamic lights
  if (surf->dlightframe == r_dlightframecount) AddDynamicLights(surf);

  // calc additive light
  // this must be done before lightmap procesing because it will clamp all lights
  for (i = 0; i < size; ++i) {
    t = blocklightsr[i]-blocklightsrS[i];
    //if (t < 0) { t = 0; blocklightsr[i] = 0; } // subtractive light fix
    t -= 0x10000;
    if (t > 0) {
      t = int(r_specular*t);
      if (t > 0xffff) t = 0xffff;
      blockaddlightsr[i] = t;
      r_light_add = true;
    }

    t = blocklightsg[i]-blocklightsgS[i];
    //if (t < 0) { t = 0; blocklightsg[i] = 0; } // subtractive light fix
    t -= 0x10000;
    if (t > 0) {
      t = int(r_specular*t);
      if (t > 0xffff) t = 0xffff;
      blockaddlightsg[i] = t;
      r_light_add = true;
    }

    t = blocklightsb[i]-blocklightsbS[i];
    //if (t < 0) { t = 0; blocklightsb[i] = 0; } // subtractive light fix
    t -= 0x10000;
    if (t > 0) {
      t = int(r_specular*t);
      if (t > 0xffff) t = 0xffff;
      blockaddlightsb[i] = t;
      r_light_add = true;
    }
  }

  // bound, invert, and shift
  for (i = 0; i < size; ++i) {
    //if (blocklightsrS[i]|blocklightsgS[i]|blocklightsbS[i]) fprintf(stderr, "*** SBL: (%d,%d,%d)\n", blocklightsrS[i], blocklightsgS[i], blocklightsbS[i]);
    blocklightsr[i] = xblight((int)blocklightsr[i], (int)blocklightsrS[i]);
    blocklightsg[i] = xblight((int)blocklightsg[i], (int)blocklightsgS[i]);
    blocklightsb[i] = xblight((int)blocklightsb[i], (int)blocklightsbS[i]);

    /*
    blocklightsr[i] = 0xff00;
    blocklightsg[i] = 0x0100;
    blocklightsb[i] = 0xff00;
    */
  }

  //return is_coloured;
  unguard;
}


//==========================================================================
//
// VRenderLevel::FlushCaches
//
//==========================================================================
void VRenderLevel::FlushCaches () {
  guard(VRenderLevel::FlushCaches);
  memset(blockbuf, 0, sizeof(blockbuf));
  freeblocks = nullptr;
  for (int i = 0; i < NUM_CACHE_BLOCKS; ++i) {
    blockbuf[i].chain = freeblocks;
    freeblocks = &blockbuf[i];
  }
  for (int i = 0; i < NUM_BLOCK_SURFS; ++i) {
    cacheblocks[i] = freeblocks;
    freeblocks = freeblocks->chain;
    cacheblocks[i]->width = BLOCK_WIDTH;
    cacheblocks[i]->height = BLOCK_HEIGHT;
    cacheblocks[i]->blocknum = i;
  }
  light_reset_surface_cache = 0;
  unguard;
}


//==========================================================================
//
// VRenderLevel::FlushOldCaches
//
//==========================================================================
void VRenderLevel::FlushOldCaches () {
  guard(VRenderLevel::FlushOldCaches);
  for (int i = 0; i < NUM_BLOCK_SURFS; ++i) {
    for (surfcache_t *blines = cacheblocks[i]; blines; blines = blines->bnext) {
      for (surfcache_t *block = blines; block; block = block->lnext) {
        if (block->owner && cacheframecount != block->lastframe) block = FreeBlock(block, false);
      }
      if (!blines->owner && !blines->lprev && !blines->lnext) blines = FreeBlock(blines, true);
    }
  }
  if (!freeblocks) Sys_Error("No more free blocks");
  GCon->Logf("Surface cache overflow, old caches flushed");
  unguard;
}


//==========================================================================
//
// VRenderLevel::GentlyFlushAllCaches
//
//==========================================================================
void VRenderLevel::GentlyFlushAllCaches () {
  guard(VRenderLevel::GentlyFlushAllCaches);
  light_reset_surface_cache = 0;
  for (int i = 0; i < NUM_BLOCK_SURFS; ++i) {
    for (surfcache_t *blines = cacheblocks[i]; blines; blines = blines->bnext) {
      for (surfcache_t *block = blines; block; block = block->lnext) {
        if (block->owner) block = FreeBlock(block, false);
      }
      if (!blines->owner && !blines->lprev && !blines->lnext) blines = FreeBlock(blines, true);
    }
  }
  if (!freeblocks) Sys_Error("No more free blocks");
  unguard;
}


//==========================================================================
//
// VRenderLevel::AllocBlock
//
//==========================================================================
surfcache_t *VRenderLevel::AllocBlock (int width, int height) {
  guard(VRenderLevel::AllocBlock);
  surfcache_t *blines;
  surfcache_t *block;
  surfcache_t *other;

  for (int i = 0; i < NUM_BLOCK_SURFS; ++i) {
    for (blines = cacheblocks[i]; blines; blines = blines->bnext) {
      if (blines->height != height) continue;
      for (block = blines; block; block = block->lnext) {
        if (block->owner) continue;
        if (block->width < width) continue;
        if (block->width > width) {
          if (!freeblocks) FlushOldCaches();
          other = freeblocks;
          freeblocks = other->chain;
          other->s = block->s+width;
          other->t = block->t;
          other->width = block->width-width;
          other->height = block->height;
          other->lnext = block->lnext;
          if (other->lnext) other->lnext->lprev = other;
          block->lnext = other;
          other->lprev = block;
          block->width = width;
          other->owner = nullptr;
          other->blocknum = i;
        }
        return block;
      }
    }
  }

  for (int i = 0; i < NUM_BLOCK_SURFS; ++i) {
    for (blines = cacheblocks[i]; blines; blines = blines->bnext) {
      if (blines->height < height) continue;
      if (blines->lnext) continue;
      block = blines;
      if (block->height > height) {
        if (!freeblocks) FlushOldCaches();
        other = freeblocks;
        freeblocks = other->chain;
        other->s = 0;
        other->t = block->t+height;
        other->width = block->width;
        other->height = block->height - height;
        other->lnext = nullptr;
        other->lprev = nullptr;
        other->bnext = block->bnext;
        if (other->bnext) other->bnext->bprev = other;
        block->bnext = other;
        other->bprev = block;
        block->height = height;
        other->owner = nullptr;
        other->blocknum = i;
      }

      if (!freeblocks) FlushOldCaches();
      other = freeblocks;
      freeblocks = other->chain;
      other->s = block->s + width;
      other->t = block->t;
      other->width = block->width - width;
      other->height = block->height;
      other->lnext = nullptr;
      block->lnext = other;
      other->lprev = block;
      block->width = width;
      other->owner = nullptr;
      other->blocknum = i;

      return block;
    }
  }

  //Sys_Error("Surface cache overflow");
  if (!light_reset_surface_cache) {
    GCon->Logf("ERROR! ERROR! ERROR! Surface cache overflow!");
    light_reset_surface_cache = 1;
  }
  return nullptr;
  unguard;
}


//==========================================================================
//
// VRenderLevel::FreeBlock
//
//==========================================================================
surfcache_t *VRenderLevel::FreeBlock (surfcache_t *block, bool check_lines) {
  guard(VRenderLevel::FreeBlock);
  surfcache_t *other;

  if (block->owner) {
    *block->owner = nullptr;
    block->owner = nullptr;
  }
  if (block->lnext && !block->lnext->owner) {
    other = block->lnext;
    block->width += other->width;
    block->lnext = other->lnext;
    if (block->lnext) block->lnext->lprev = block;
    other->chain = freeblocks;
    freeblocks = other;
  }
  if (block->lprev && !block->lprev->owner) {
    other = block;
    block = block->lprev;
    block->width += other->width;
    block->lnext = other->lnext;
    if (block->lnext) block->lnext->lprev = block;
    other->chain = freeblocks;
    freeblocks = other;
  }

  if (block->lprev || block->lnext || !check_lines) return block;

  if (block->bnext && !block->bnext->lnext) {
    other = block->bnext;
    block->height += other->height;
    block->bnext = other->bnext;
    if (block->bnext) block->bnext->bprev = block;
    other->chain = freeblocks;
    freeblocks = other;
  }
  if (block->bprev && !block->bprev->lnext) {
    other = block;
    block = block->bprev;
    block->height += other->height;
    block->bnext = other->bnext;
    if (block->bnext) block->bnext->bprev = block;
    other->chain = freeblocks;
    freeblocks = other;
  }
  return block;
  unguard;
}


//==========================================================================
//
// VRenderLevel::FreeSurfCache
//
//==========================================================================
void VRenderLevel::FreeSurfCache (surfcache_t *block) {
  guard(VRenderLevel::FreeSurfCache);
  FreeBlock(block, true);
  unguard;
}


//==========================================================================
//
// VRenderLevelShared::FreeSurfCache
//
//==========================================================================
void VRenderLevelShared::FreeSurfCache (surfcache_t*) {
}


//==========================================================================
//
//  VRenderLevel::CacheSurface
//
//==========================================================================
void VRenderLevel::CacheSurface (surface_t *surface) {
  guard(VRenderLevel::CacheSurface);
  surfcache_t *cache;
  int smax, tmax;
  int i, j, bnum;

  // see if the cache holds appropriate data
  cache = surface->CacheSurf;

  const vuint32 srflight = fixSurfLightLevel(surface);

  if (cache && !cache->dlight && surface->dlightframe != r_dlightframecount && cache->Light == srflight) {
    bnum = cache->blocknum;
    cache->chain = light_chain[bnum];
    light_chain[bnum] = cache;
    cache->lastframe = cacheframecount;
    return;
  }

  // determine shape of surface
  smax = (surface->extents[0]>>4)+1;
  tmax = (surface->extents[1]>>4)+1;

  // allocate memory if needed
  // if a texture just animated, don't reallocate it
  if (!cache) {
    cache = AllocBlock(smax, tmax);
    // in rare case of surface cache overflow, just skip the light
    if (!cache) return; // alas
    surface->CacheSurf = cache;
    cache->owner = &surface->CacheSurf;
    cache->surf = surface;
  }

  if (surface->dlightframe == r_dlightframecount) {
    cache->dlight = 1;
  } else {
    cache->dlight = 0;
  }
  cache->Light = srflight;

  // calculate the lightings
  BuildLightMap(surface/*, 0*/);
  bnum = cache->blocknum;
  block_changed[bnum] = true;

  for (j = 0; j < tmax; ++j) {
    for (i = 0; i < smax; ++i) {
      rgba_t &lb = light_block[bnum][(j+cache->t)*BLOCK_WIDTH+i+cache->s];
      lb.r = 255-byte(blocklightsr[j*smax+i]>>8);
      lb.g = 255-byte(blocklightsg[j*smax+i]>>8);
      lb.b = 255-byte(blocklightsb[j*smax+i]>>8);
      lb.a = 255;
    }
  }
  cache->chain = light_chain[bnum];
  light_chain[bnum] = cache;
  cache->lastframe = cacheframecount;

  // specular highlights
  for (j = 0; j < tmax; ++j) {
    for (i = 0; i < smax; ++i) {
      rgba_t &lb = add_block[bnum][(j+cache->t)*BLOCK_WIDTH+i+cache->s];
      lb.r = byte(blockaddlightsr[j*smax+i]>>8);
      lb.g = byte(blockaddlightsg[j*smax+i]>>8);
      lb.b = byte(blockaddlightsb[j*smax+i]>>8);
      lb.a = 255;
    }
  }
  if (r_light_add) {
    cache->addchain = add_chain[bnum];
    add_chain[bnum] = cache;
    add_changed[bnum] = true;
  }
  unguard;
}
