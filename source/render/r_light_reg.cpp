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


// ////////////////////////////////////////////////////////////////////////// //
extern VCvarI r_ambient;
extern VCvarB r_allow_ambient;
extern VCvarB r_dynamic_clip;
extern VCvarB r_dynamic_clip_pvs;
extern VCvarB r_dynamic_clip_more;
extern VCvarB r_allow_subtractive_lights;
extern VCvarB r_glow_flat;

vuint32 gf_dynlights_processed = 0;
vuint32 gf_dynlights_traced = 0;
int ldr_extrasamples_override = -1;


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB r_extrasamples("r_extrasamples", true, "Do static lightmap filtering?", CVAR_Archive);
static VCvarB r_static_add("r_static_add", true, "Are static lights additive in regular renderer?", CVAR_Archive);
static VCvarF r_specular("r_specular", "0.1", "Specular light in regular renderer.", CVAR_Archive);

extern VCvarB dbg_adv_light_notrace_mark;


// ////////////////////////////////////////////////////////////////////////// //
static bool r_light_add;

enum { GridSize = VRenderLevel::LMapTraceInfo::GridSize };

vuint32 blocklightsr[GridSize*GridSize];
vuint32 blocklightsg[GridSize*GridSize];
vuint32 blocklightsb[GridSize*GridSize];
static vuint32 blockaddlightsr[GridSize*GridSize];
static vuint32 blockaddlightsg[GridSize*GridSize];
static vuint32 blockaddlightsb[GridSize*GridSize];

// subtractive
static vuint32 blocklightsrS[GridSize*GridSize];
static vuint32 blocklightsgS[GridSize*GridSize];
static vuint32 blocklightsbS[GridSize*GridSize];

vuint8 light_remap[256];
int light_mem = 0;

int light_reset_surface_cache = 0;


// ////////////////////////////////////////////////////////////////////////// //
 // *4 for extra filtering
static float lightmap[GridSize*GridSize*4];
static float lightmapr[GridSize*GridSize*4];
static float lightmapg[GridSize*GridSize*4];
static float lightmapb[GridSize*GridSize*4];
//static bool light_hit;
static bool is_coloured;


//==========================================================================
//
//  getSurfLightLevelInt
//
//==========================================================================
static inline int getSurfLightLevelInt (const surface_t *surf) {
  if (r_glow_flat && surf && !surf->seg && surf->subsector) {
    const sector_t *sec = surf->subsector->sector;
    if (sec->floor.pic && surf->plane->normal.z > 0.0f) {
      VTexture *gtex = GTextureManager(sec->floor.pic);
      if (gtex && gtex->Type != TEXTYPE_Null && gtex->glowing) return 255;
    }
    if (sec->ceiling.pic && surf->plane->normal.z < 0.0f) {
      VTexture *gtex = GTextureManager(sec->ceiling.pic);
      if (gtex && gtex->Type != TEXTYPE_Null && gtex->glowing) return 255;
    }
  }
  if (!surf || !r_allow_ambient) return 0;
  int slins = (surf->Light>>24)&0xff;
  slins = MAX(slins, r_ambient.asInt());
  //if (slins > 255) slins = 255;
  return clampToByte(slins);
}


//==========================================================================
//
//  fixSurfLightLevel
//
//==========================================================================
static inline vuint32 fixSurfLightLevel (const surface_t *surf) {
  if (r_glow_flat && surf && !surf->seg && surf->subsector) {
    const sector_t *sec = surf->subsector->sector;
    if (sec->floor.pic && surf->plane->normal.z > 0.0f) {
      VTexture *gtex = GTextureManager(sec->floor.pic);
      if (gtex && gtex->Type != TEXTYPE_Null && gtex->glowing) return (surf->Light&0xffffffu)|0xff000000u;
    }
    if (sec->ceiling.pic && surf->plane->normal.z < 0.0f) {
      VTexture *gtex = GTextureManager(sec->ceiling.pic);
      if (gtex && gtex->Type != TEXTYPE_Null && gtex->glowing) return (surf->Light&0xffffffu)|0xff000000u;
    }
  }
  if (!surf || !r_allow_ambient) return 0;
  int slins = (surf->Light>>24)&0xff;
  slins = MAX(slins, r_ambient.asInt());
  //if (slins > 255) slins = 255;
  return (surf->Light&0xffffffu)|(((vuint32)clampToByte(slins))<<24);
}


//==========================================================================
//
//  VRenderLevel::CastRay
//
//  Returns the distance between the points, or 0 if blocked
//
//==========================================================================
float VRenderLevel::CastRay (const TVec &p1, const TVec &p2, float squaredist) {
  const TVec delta = p2-p1;
  const float t = DotProduct(delta, delta);
  if (t >= squaredist) return 0.0f; // too far away
  if (t <= 2.0f) return 1.0f; // at light point

  linetrace_t Trace;
  if (!Level->TraceLine(Trace, p1, p2, SPF_NOBLOCKSIGHT)) return 0.0f; // ray was blocked

  //if (t == 0) t = 1; // don't blow up...
  return sqrtf(t);
}


//==========================================================================
//
// VRenderLevel::CalcMinMaxs
//
//==========================================================================
void VRenderLevel::CalcMinMaxs (LMapTraceInfo &lmi, const surface_t *surf) {
  TVec smins(999999.0f, 999999.0f, 999999.0f);
  TVec smaxs(-999999.0f, -999999.0f, -999999.0f);
  const TVec *v = &surf->verts[0];
  for (int i = surf->count; i--; ++v) {
    if (smins.x > v->x) smins.x = v->x;
    if (smins.y > v->y) smins.y = v->y;
    if (smins.z > v->z) smins.z = v->z;
    if (smaxs.x < v->x) smaxs.x = v->x;
    if (smaxs.y < v->y) smaxs.y = v->y;
    if (smaxs.z < v->z) smaxs.z = v->z;
  }
  lmi.smins = smins;
  lmi.smaxs = smaxs;
}


//==========================================================================
//
//  VRenderLevel::CalcFaceVectors
//
//  fills in texorg, worldtotex, and textoworld
//
//==========================================================================
bool VRenderLevel::CalcFaceVectors (LMapTraceInfo &lmi, const surface_t *surf) {
  const texinfo_t *tex = surf->texinfo;

  lmi.worldtotex[0] = tex->saxis;
  lmi.worldtotex[1] = tex->taxis;

  // calculate a normal to the texture axis
  // points can be moved along this without changing their S/T
  TVec texnormal(
    tex->taxis.y*tex->saxis.z-tex->taxis.z*tex->saxis.y,
    tex->taxis.z*tex->saxis.x-tex->taxis.x*tex->saxis.z,
    tex->taxis.x*tex->saxis.y-tex->taxis.y*tex->saxis.x);
  texnormal.normaliseInPlace();
  if (!isFiniteF(texnormal.x)) return false; // no need to check other coords

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
  if (!isFiniteF(distscale)) return false;

  for (int i = 0; i < 2; ++i) {
    const float len = 1.0f/lmi.worldtotex[i].length();
    //float len = lmi.worldtotex[i].length();
    if (!isFiniteF(len)) return false; // just in case
    const float dist = DotProduct(lmi.worldtotex[i], surf->plane->normal)*distscale;
    lmi.textoworld[i] = lmi.worldtotex[i]-texnormal*dist;
    lmi.textoworld[i] = lmi.textoworld[i]*len*len;
    //lmi.textoworld[i] = lmi.textoworld[i]*(1.0f/len)*(1.0f/len);
  }

  // calculate texorg on the texture plane
  for (int i = 0; i < 3; ++i) lmi.texorg[i] = -tex->soffs*lmi.textoworld[0][i]-tex->toffs*lmi.textoworld[1][i];

  // project back to the face plane
  {
    const float dist = (DotProduct(lmi.texorg, surf->plane->normal)-surf->plane->dist-1.0f)*distscale;
    lmi.texorg = lmi.texorg-texnormal*dist;
  }

  return true;
}


#define CP_FIX_UT(uort_)  do { \
  if (u##uort_ > mid##uort_) { \
    u##uort_ -= 8; \
    if (u##uort_ < mid##uort_) u##uort_ = mid##uort_; \
  } else { \
    u##uort_ += 8; \
    if (u##uort_ > mid##uort_) u##uort_ = mid##uort_; \
  } \
} while (0) \


//==========================================================================
//
//  VRenderLevel::CalcPoints
//
//  for each texture aligned grid point, back project onto the plane
//  to get the world xyz value of the sample point
//
//  for dynlights, set `lowres` to `true`
//
//==========================================================================
void VRenderLevel::CalcPoints (LMapTraceInfo &lmi, const surface_t *surf, bool lowres) {
  int w, h;
  float step;
  float starts, startt;
  linetrace_t Trace;

  bool doExtra = r_extrasamples;
  if (ldr_extrasamples_override >= 0) doExtra = (ldr_extrasamples_override > 0);
  if (!lowres && doExtra) {
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
  lmi.didExtra = doExtra;

  // fill in surforg
  // the points are biased towards the centre of the surface
  // to help avoid edge cases just inside walls
  const float mids = surf->texturemins[0]+surf->extents[0]/2.0f;
  const float midt = surf->texturemins[1]+surf->extents[1]/2.0f;
  const TVec facemid = lmi.texorg+lmi.textoworld[0]*mids+lmi.textoworld[1]*midt;

  lmi.numsurfpt = w*h;
  bool doPointCheck = false;
  // *4 for extra filtering
  if (lmi.numsurfpt > LMapTraceInfo::GridSize*LMapTraceInfo::GridSize*4) {
    GCon->Logf(NAME_Warning, "too many points in lightmap tracer");
    lmi.numsurfpt = LMapTraceInfo::GridSize*LMapTraceInfo::GridSize*4;
    doPointCheck = true;
  }

  TVec *spt = lmi.surfpt;
  for (int t = 0; t < h; ++t) {
    for (int s = 0; s < w; ++s, ++spt) {
      if (doPointCheck && (int)(ptrdiff_t)(spt-lmi.surfpt) >= LMapTraceInfo::GridSize*LMapTraceInfo::GridSize*4) return;
      float us = starts+s*step;
      float ut = startt+t*step;

      // if a line can be traced from surf to facemid, the point is good
      for (int i = 0; i < 6; ++i) {
        // calculate texture point
        //*spt = lmi.texorg+lmi.textoworld[0]*us+lmi.textoworld[1]*ut;
        *spt = lmi.calcPoint(us, ut);
        if (lowres) break;
        //const TVec fms = facemid-(*spt);
        //if (length2DSquared(fms) < 0.1f) break; // same point, got it
        if (Level->TraceLine(Trace, facemid, *spt, SPF_NOBLOCKSIGHT)) break; // got it

        if (i&1) {
          CP_FIX_UT(s);
        } else {
          CP_FIX_UT(t);
        }

        // move surf 8 pixels towards the centre
        const TVec fms = facemid-(*spt);
        *spt += 8*Normalise(fms);
      }
      //if (i == 2) ++c_bad;
    }
  }
}


//==========================================================================
//
//  VRenderLevel::SingleLightFace
//
//==========================================================================
void VRenderLevel::SingleLightFace (LMapTraceInfo &lmi, light_t *light, surface_t *surf, const vuint8 *facevis) {
  if (surf->count < 3) return; // wtf?!

  // check potential visibility
  if (facevis) {
    if (!(facevis[light->leafnum>>3]&(1<<(light->leafnum&7)))) return;
  }

  // check bounding box
  if (light->origin.x+light->radius < lmi.smins.x ||
      light->origin.x-light->radius > lmi.smaxs.x ||
      light->origin.y+light->radius < lmi.smins.y ||
      light->origin.y-light->radius > lmi.smaxs.y ||
      light->origin.z+light->radius < lmi.smins.z ||
      light->origin.z-light->radius > lmi.smaxs.z)
  {
    return;
  }

  float dist = DotProduct(light->origin, surf->plane->normal)-surf->plane->dist;

  // don't bother with lights behind the surface
  if (dist <= -0.1f) return;

  // don't bother with light too far away
  if (dist > light->radius) return;

  // calc points only when surface may be lit by a light
  if (!lmi.pointsCalced) {
    if (!CalcFaceVectors(lmi, surf)) {
      lmi.numsurfpt = 0;
      memset(lightmap, 0, GridSize*GridSize*4*sizeof(float));
      memset(lightmapr, 0, GridSize*GridSize*4*sizeof(float));
      memset(lightmapg, 0, GridSize*GridSize*4*sizeof(float));
      memset(lightmapb, 0, GridSize*GridSize*4*sizeof(float));
      return;
    }

    CalcPoints(lmi, surf, false);
    lmi.pointsCalced = true;
    memset(lightmap, 0, lmi.numsurfpt*sizeof(float));
    memset(lightmapr, 0, lmi.numsurfpt*sizeof(float));
    memset(lightmapg, 0, lmi.numsurfpt*sizeof(float));
    memset(lightmapb, 0, lmi.numsurfpt*sizeof(float));
  }

  // check it for real
  const TVec *spt = lmi.surfpt;
  const float squaredist = light->radius*light->radius;
  const float rmul = ((light->colour>>16)&255)/255.0f;
  const float gmul = ((light->colour>>8)&255)/255.0f;
  const float bmul = (light->colour&255)/255.0f;
  for (int c = 0; c < lmi.numsurfpt; ++c, ++spt) {
    dist = CastRay(light->origin, *spt, squaredist);
    if (!dist) continue; // light ray is blocked

    TVec incoming = light->origin-(*spt);
    incoming.normaliseInPlace();
    if (!incoming.isValid()) continue;
    float angle = DotProduct(incoming, surf->plane->normal);

    angle = 0.5f+0.5f*angle;
    const float add = (light->radius-dist)*angle;
    if (add < 0) continue;

    lightmap[c] += add;
    lightmapr[c] += add*rmul;
    lightmapg[c] += add*gmul;
    lightmapb[c] += add*bmul;
    // ignore really tiny lights
    if (lightmap[c] > 1) {
      lmi.light_hit = true;
      if (light->colour != 0xffffffff) is_coloured = true;
    }
  }
}


//==========================================================================
//
//  VRenderLevel::LightFace
//
//==========================================================================
void VRenderLevel::LightFace (surface_t *surf, subsector_t *leaf) {
  if (surf->count < 3) return; // wtf?!

  LMapTraceInfo lmi;
  //lmi.points_calculated = false;
  check(!lmi.pointsCalced);

  const vuint8 *facevis = (leaf && Level->HasPVS() ? Level->LeafPVS(leaf) : nullptr);
  lmi.light_hit = false;
  is_coloured = false;

  // cast all static lights
  CalcMinMaxs(lmi, surf);
  if (r_static_lights) {
    light_t *stl = Lights.ptr();
    for (int i = Lights.length(); i--; ++stl) SingleLightFace(lmi, stl, surf, facevis);
  }

  if (!lmi.light_hit) {
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

  const int w = (surf->extents[0]>>4)+1;
  const int h = (surf->extents[1]>>4)+1;

  // if the surface already has a lightmap, we will reuse it,
  // otherwise we must allocate a new block
  if (is_coloured) {
    /*
    if (surf->lightmap_rgb) Z_Free(surf->lightmap_rgb);
    surf->lightmap_rgb = (rgb_t *)Z_Malloc(w*h*3);
    */
    // use realloc, just in case
    if (!surf->lightmap_rgb) light_mem += w*h*3;
    surf->lightmap_rgb = (rgb_t *)Z_Realloc(surf->lightmap_rgb, w*h*3);

    int i = 0;
    for (int t = 0; t < h; ++t) {
      for (int s = 0; s < w; ++s, ++i) {
        float total;
        if (lmi.didExtra) {
          // filtered sample
          total = lightmapr[t*w*4+s*2]+
                  lightmapr[t*2*w*2+s*2+1]+
                  lightmapr[(t*2+1)*w*2+s*2]+
                  lightmapr[(t*2+1)*w*2+s*2+1];
          total *= 0.25f;
        } else {
          total = lightmapr[i];
        }
        surf->lightmap_rgb[i].r = clampToByte((int)total);

        if (lmi.didExtra) {
          // filtered sample
          total = lightmapg[t*w*4+s*2]+
                  lightmapg[t*2*w*2+s*2+1]+
                  lightmapg[(t*2+1)*w*2+s*2]+
                  lightmapg[(t*2+1)*w*2+s*2+1];
          total *= 0.25f;
        } else {
          total = lightmapg[i];
        }
        surf->lightmap_rgb[i].g = clampToByte((int)total);

        if (lmi.didExtra) {
          // filtered sample
          total = lightmapb[t*w*4+s*2]+
                  lightmapb[t*2*w*2+s*2+1]+
                  lightmapb[(t*2+1)*w*2+s*2]+
                  lightmapb[(t*2+1)*w*2+s*2+1];
          total *= 0.25f;
        } else {
          total = lightmapb[i];
        }
        surf->lightmap_rgb[i].b = clampToByte((int)total);
      }
    }
  } else {
    if (surf->lightmap_rgb) {
      Z_Free(surf->lightmap_rgb);
      surf->lightmap_rgb = nullptr;
    }
  }

  if (!surf->lightmap) light_mem += w*h;
  surf->lightmap = (vuint8 *)Z_Realloc(surf->lightmap, w*h);
  //if (surf->lightmap) Z_Free(surf->lightmap);
  //surf->lightmap = (vuint8 *)Z_Malloc(w*h);

  int i = 0;
  for (int t = 0; t < h; ++t) {
    for (int s = 0; s < w; ++s, ++i) {
      float total;
      if (lmi.didExtra) {
        // filtered sample
        total = lightmap[t*w*4+s*2]+
                lightmap[t*2*w*2+s*2+1]+
                lightmap[(t*2+1)*w*2+s*2]+
                lightmap[(t*2+1)*w*2+s*2+1];
        total *= 0.25f;
      } else {
        total = lightmap[i];
      }
      surf->lightmap[i] = clampToByte((int)total);
    }
  }
}


//**************************************************************************
//**
//**  DYNAMIC LIGHTS
//**
//**************************************************************************


/*
NOTES:
we should do several things to speed it up:
first, cache all dynlights, so we'd be able to reuse info from previous tracing
second, store all dynlights affecting a surface, and calculated traceinfo for them

by storing traceinfo, we can reuse it when light radius changed, instead of
tracing a light again and again.

actually, what we are interested in is not a light per se, but light origin.
if we have a light with the same origin, we can reuse it's traceinfo (and possibly
extend it if new light has bigger radius).

thus, we can go with "light cachemap" instead, and store all relevant info there.
also, keep info in cache for several seconds, as player is likely to move around
the light. do cachemap housekeeping once in 2-3 seconds, for example. it doesn't
really matter if we'll accumulate alot of lights there.

also, with proper cache implementation, we can drop "static lights" at all.
just trace and cache "static lights" at level start, and mark 'em as "persistent".
this way, when level geometry changed, we can re-trace static lights too.
*/


//==========================================================================
//
//  VRenderLevel::AddDynamicLights
//
//  dlight frame already checked
//
//==========================================================================
void VRenderLevel::AddDynamicLights (surface_t *surf) {
  if (surf->count < 3) return; // wtf?!

  //float mids = 0, midt = 0;
  //TVec facemid = TVec(0,0,0);
  LMapTraceInfo lmi;
  check(!lmi.pointsCalced);

  int smax = (surf->extents[0]>>4)+1;
  int tmax = (surf->extents[1]>>4)+1;
  if (smax > LMapTraceInfo::GridSize) smax = LMapTraceInfo::GridSize;
  if (tmax > LMapTraceInfo::GridSize) tmax = LMapTraceInfo::GridSize;

  const texinfo_t *tex = surf->texinfo;

  /*
  const float starts = surf->texturemins[0];
  const float startt = surf->texturemins[1];
  const float step = 16;
  */

  const bool hasPVS = Level->HasPVS();

  bool doCheckTrace = (r_dynamic_clip && r_dynamic_clip_more);

  for (unsigned lnum = 0; lnum < MAX_DLIGHTS; ++lnum) {
    if (!(surf->dlightbits&(1U<<lnum))) continue; // not lit by this light

    const dlight_t &dl = DLights[lnum];
    //if (dl.type == DLTYPE_Subtractive) GCon->Logf("***SUBTRACTIVE LIGHT!");
    if (dl.type == DLTYPE_Subtractive && !r_allow_subtractive_lights) continue;

    const int xnfo = dlinfo[lnum].needTrace;
    if (!xnfo) continue;

    float rad = dl.radius;
    float dist = DotProduct(dl.origin, surf->plane->normal)-surf->plane->dist;
    if (r_dynamic_clip) {
      if (dist <= -0.1f) continue;
    }

    rad -= fabsf(dist);
    float minlight = dl.minlight;
    if (rad < minlight) continue;
    minlight = rad-minlight;

    TVec impact = dl.origin-surf->plane->normal*dist;

    if (hasPVS && r_dynamic_clip_pvs) {
      const subsector_t *sub = Level->PointInSubsector(impact);
      const vuint8 *dyn_facevis = Level->LeafPVS(sub);
      //int leafnum = Level->PointInSubsector(dl.origin)-Level->Subsectors;
      int leafnum = dlinfo[lnum].leafnum;
      if (leafnum < 0) continue;
      // check potential visibility
      if (!(dyn_facevis[leafnum>>3]&(1<<(leafnum&7)))) continue;
    }

    //TODO: we can use clipper to check if destination subsector is occluded
    bool needProperTrace = (doCheckTrace && xnfo > 0);
    if (dl.flags&dlight_t::NoShadow) needProperTrace = false;

    ++gf_dynlights_processed;
    if (needProperTrace) ++gf_dynlights_traced;

    vuint32 dlcolor = (!needProperTrace && dbg_adv_light_notrace_mark ? 0xffff00ffU : dl.colour);

    const float rmul = (dlcolor>>16)&255;
    const float gmul = (dlcolor>>8)&255;
    const float bmul = dlcolor&255;

    TVec local;
    local.x = DotProduct(impact, tex->saxis)+tex->soffs;
    local.y = DotProduct(impact, tex->taxis)+tex->toffs;
    local.z = 0;

    local.x -= /*starts*/surf->texturemins[0];
    local.y -= /*startt*/surf->texturemins[1];

    lmi.setupSpotlight(dl.coneDirection, dl.coneAngle);

    if (!lmi.pointsCalced && (needProperTrace || lmi.spotLight)) {
      if (!CalcFaceVectors(lmi, surf)) return;
      CalcPoints(lmi, surf, true);
      lmi.pointsCalced = true;
    }

    //TVec spt(0.0f, 0.0f, 0.0f);
    float attn = 1.0f;

    const TVec *spt = lmi.surfpt;
    for (int t = 0; t < tmax; ++t) {
      int td = (int)local.y-t*16;
      if (td < 0) td = -td;
      for (int s = 0; s < smax; ++s, ++spt) {
        int sd = (int)local.x-s*16;
        if (sd < 0) sd = -sd;
        dist = (sd > td ? sd+(td>>1) : td+(sd>>1));
        if (dist < minlight) {
          // check spotlight cone
          if (lmi.spotLight) {
            //spt = lmi.calcPoint(starts+s*step, startt+t*step);
            if (length2DSquared((*spt)-dl.origin) > 2*2) {
              attn = spt->CalcSpotlightAttMult(dl.origin, lmi.coneDir, lmi.coneAngle);
              if (attn == 0.0f) continue;
            }
          }
          float add = (rad-dist)*attn;
          if (add <= 0.0f) continue;
          // do more dynlight clipping
          if (needProperTrace) {
            //if (!lmi.spotLight) spt = lmi.calcPoint(starts+s*step, startt+t*step);
            if (length2DSquared((*spt)-dl.origin) > 2*2) {
              if (!Level->CastCanSee(dl.origin, *spt, 0)) continue;
            }
          }
          int i = t*smax+s;
          if (dl.type == DLTYPE_Subtractive) {
            //blocklightsS[i] += (rad-dist)*256.0f;
            blocklightsrS[i] += rmul*add;
            blocklightsgS[i] += gmul*add;
            blocklightsbS[i] += bmul*add;
          } else {
            //blocklights[i] += (rad-dist)*256.0f;
            blocklightsr[i] += rmul*add;
            blocklightsg[i] += gmul*add;
            blocklightsb[i] += bmul*add;
          }
          if (dlcolor != 0xffffffff) is_coloured = true;
        }
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevel::InvalidateSurfacesLMaps
//
//==========================================================================
void VRenderLevel::InvalidateSurfacesLMaps (surface_t *surf) {
  for (; surf; surf = surf->next) {
    if (surf->count < 3) continue; // just in case
    if (surf->CacheSurf) { FreeSurfCache(surf->CacheSurf); surf->CacheSurf = nullptr; }
    if (surf->lightmap) { Z_Free(surf->lightmap); surf->lightmap = nullptr; }
    if (surf->lightmap_rgb) { Z_Free(surf->lightmap_rgb); surf->lightmap_rgb = nullptr; }
    //if (surf->drawflags&surface_t::DF_CALC_LMAP) continue;
    /*if (surf->lightmap || surf->lightmap_rgb)*/
    {
      surf->drawflags |= surface_t::DF_CALC_LMAP;
    }
  }
}


//==========================================================================
//
//  VRenderLevel::InvalidateLineLMaps
//
//==========================================================================
void VRenderLevel::InvalidateLineLMaps (drawseg_t *dseg) {
  const seg_t *seg = dseg->seg;

  if (!seg->linedef) return; // miniseg

  InvalidateSurfacesLMaps(dseg->mid->surfs);
  if (seg->backsector) {
    // two sided line
    InvalidateSurfacesLMaps(dseg->top->surfs);
    // no lightmaps on sky anyway
    //InvalidateSurfacesLMaps(dseg->topsky->surfs);
    InvalidateSurfacesLMaps(dseg->bot->surfs);
    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      InvalidateSurfacesLMaps(sp->surfs);
    }
  }
}


//==========================================================================
//
//  VRenderLevel::InvalidateStaticLightmaps
//
//==========================================================================
void VRenderLevel::InvalidateStaticLightmaps (const TVec &org, float radius) {
  //FIXME: make this faster!
  if (radius < 2.0f) return;
  float bbox[6];
  subsector_t *sub = Level->Subsectors;
  for (int count = Level->NumSubsectors; count--; ++sub) {
    if (!sub->sector->linecount) continue; // skip sectors containing original polyobjs
    Level->GetSubsectorBBox(sub, bbox);
    if (!CheckSphereVsAABBIgnoreZ(bbox, org, radius)) continue;
    //GCon->Logf("invalidating subsector %d", (int)(ptrdiff_t)(sub-Level->Subsectors));
    // polyobj
    if (sub->poly) {
      int polyCount = sub->poly->numsegs;
      seg_t **polySeg = sub->poly->segs;
      while (polyCount--) {
        InvalidateLineLMaps((*polySeg)->drawsegs);
        ++polySeg;
      }
    }
    //TODO: invalidate only relevant segs
    for (subregion_t *subregion = sub->regions; subregion; subregion = subregion->next) {
      drawseg_t *ds = subregion->lines;
      for (int dscount = sub->numlines; dscount--; ++ds) {
        InvalidateLineLMaps(ds);
      }
      InvalidateSurfacesLMaps(subregion->floor->surfs);
      InvalidateSurfacesLMaps(subregion->ceil->surfs);
    }
  }
}


//==========================================================================
//
//  xblight
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
//  VRenderLevel::BuildLightMap
//
//  combine and scale multiple lightmaps into the 8.8 format in blocklights
//
//==========================================================================
void VRenderLevel::BuildLightMap (surface_t *surf) {
  if (surf->drawflags&surface_t::DF_CALC_LMAP) {
    surf->drawflags &= ~surface_t::DF_CALC_LMAP;
    //GCon->Logf("%p: Need to calculate static lightmap for subsector %p!", surf, surf->subsector);
    if (surf->subsector) LightFace(surf, surf->subsector);
  }
  if (surf->count < 3) return; // wtf?!

  is_coloured = false;
  r_light_add = false;
  int smax = (surf->extents[0]>>4)+1;
  int tmax = (surf->extents[1]>>4)+1;
  if (smax > LMapTraceInfo::GridSize) smax = LMapTraceInfo::GridSize;
  if (tmax > LMapTraceInfo::GridSize) tmax = LMapTraceInfo::GridSize;
  int size = smax*tmax;
  vuint8 *lightmap = surf->lightmap;
  rgb_t *lightmap_rgb = surf->lightmap_rgb;

  // clear to ambient
  int t = getSurfLightLevelInt(surf);
  t <<= 8;
  int tR = ((surf->Light>>16)&255)*t/255;
  int tG = ((surf->Light>>8)&255)*t/255;
  int tB = (surf->Light&255)*t/255;
  if (tR != tG || tR != tB) is_coloured = true;

  for (int i = 0; i < size; ++i) {
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
    for (int i = 0; i < size; ++i) {
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
    for (int i = 0; i < size; ++i) {
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
  if (surf->dlightframe == currDLightFrame) AddDynamicLights(surf);

  // calc additive light
  // this must be done before lightmap procesing because it will clamp all lights
  for (int i = 0; i < size; ++i) {
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
  for (int i = 0; i < size; ++i) {
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
}


//==========================================================================
//
//  VRenderLevel::FlushCaches
//
//==========================================================================
void VRenderLevel::FlushCaches () {
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
}


//==========================================================================
//
//  VRenderLevel::FlushOldCaches
//
//==========================================================================
void VRenderLevel::FlushOldCaches () {
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
}


//==========================================================================
//
//  VRenderLevel::GentlyFlushAllCaches
//
//==========================================================================
void VRenderLevel::GentlyFlushAllCaches () {
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
}


//==========================================================================
//
//  VRenderLevel::AllocBlock
//
//==========================================================================
surfcache_t *VRenderLevel::AllocBlock (int width, int height) {
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
        other->height = block->height-height;
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
      other->s = block->s+width;
      other->t = block->t;
      other->width = block->width-width;
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
}


//==========================================================================
//
//  VRenderLevel::FreeBlock
//
//==========================================================================
surfcache_t *VRenderLevel::FreeBlock (surfcache_t *block, bool check_lines) {
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
}


//==========================================================================
//
//  VRenderLevel::FreeSurfCache
//
//==========================================================================
void VRenderLevel::FreeSurfCache (surfcache_t *block) {
  FreeBlock(block, true);
}


//==========================================================================
//
//  VRenderLevel::CacheSurface
//
//==========================================================================
bool VRenderLevel::CacheSurface (surface_t *surface) {
  if (surface->count < 3) return false; // wtf?!

  int bnum;

  // see if the cache holds appropriate data
  surfcache_t *cache = surface->CacheSurf;

  const vuint32 srflight = fixSurfLightLevel(surface);

  if (cache && !cache->dlight && surface->dlightframe != currDLightFrame && cache->Light == srflight) {
    bnum = cache->blocknum;
    cache->chain = light_chain[bnum];
    light_chain[bnum] = cache;
    cache->lastframe = cacheframecount;
    if (!(surface->drawflags&surface_t::DF_CALC_LMAP)) return true;
  }

  // determine shape of surface
  int smax = (surface->extents[0]>>4)+1;
  int tmax = (surface->extents[1]>>4)+1;
  check(smax > 0);
  check(tmax > 0);

  // allocate memory if needed
  // if a texture just animated, don't reallocate it
  if (!cache) {
    cache = AllocBlock(smax, tmax);
    // in rare case of surface cache overflow, just skip the light
    if (!cache) return false; // alas
    surface->CacheSurf = cache;
    cache->owner = &surface->CacheSurf;
    cache->surf = surface;
  }

  if (surface->dlightframe == currDLightFrame) {
    cache->dlight = 1;
  } else {
    cache->dlight = 0;
  }
  cache->Light = srflight;

  // calculate the lightings
  BuildLightMap(surface/*, 0*/);
  bnum = cache->blocknum;
  block_changed[bnum] = true;

  check(cache->t >= 0);
  check(cache->s >= 0);

  for (int j = 0; j < tmax; ++j) {
    for (int i = 0; i < smax; ++i) {
      rgba_t &lb = light_block[bnum][(j+cache->t)*BLOCK_WIDTH+i+cache->s];
      lb.r = 255-clampToByte(blocklightsr[j*smax+i]>>8);
      lb.g = 255-clampToByte(blocklightsg[j*smax+i]>>8);
      lb.b = 255-clampToByte(blocklightsb[j*smax+i]>>8);
      lb.a = 255;
    }
  }
  cache->chain = light_chain[bnum];
  light_chain[bnum] = cache;
  cache->lastframe = cacheframecount;

  // specular highlights
  for (int j = 0; j < tmax; ++j) {
    for (int i = 0; i < smax; ++i) {
      rgba_t &lb = add_block[bnum][(j+cache->t)*BLOCK_WIDTH+i+cache->s];
      lb.r = clampToByte(blockaddlightsr[j*smax+i]>>8);
      lb.g = clampToByte(blockaddlightsg[j*smax+i]>>8);
      lb.b = clampToByte(blockaddlightsb[j*smax+i]>>8);
      lb.a = 255;
    }
  }
  if (r_light_add) {
    cache->addchain = add_chain[bnum];
    add_chain[bnum] = cache;
    add_changed[bnum] = true;
  }

  return true;
}
