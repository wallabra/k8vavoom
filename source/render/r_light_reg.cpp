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

//#define VV_DEBUG_LMAP_ALLOCATOR
//#define VV_EXPERIMENTAL_LMAP_FILTER
//#define VV_USELESS_SUBTRACTIVE_LIGHT_CODE
//#define VV_DEBUG_BMAP_TRACER


// ////////////////////////////////////////////////////////////////////////// //
extern VCvarI r_ambient_min;
extern VCvarB r_allow_ambient;
extern VCvarB r_dynamic_clip;
extern VCvarB r_dynamic_clip_pvs;
extern VCvarB r_dynamic_clip_more;
extern VCvarB r_allow_subtractive_lights;
extern VCvarB r_glow_flat;

vuint32 gf_dynlights_processed = 0;
vuint32 gf_dynlights_traced = 0;
int ldr_extrasamples_override = -1;

enum { Filter4X = false };

// ////////////////////////////////////////////////////////////////////////// //
static VCvarI r_lmap_filtering("r_lmap_filtering", "3", "Static lightmap filtering (0: none; 1: simple; 2: simple++; 3: extra).", CVAR_Archive);
static VCvarB r_lmap_lowfilter("r_lmap_lowfilter", false, "Filter lightmaps without extra samples?", CVAR_Archive);
static VCvarB r_lmap_overbright_static("r_lmap_overbright_static", true, "Use Quake-like (but gentlier) overbright for static lights?", CVAR_Archive);
static VCvarF r_lmap_specular("r_lmap_specular", "0.1", "Specular light in regular renderer.", CVAR_Archive);
static VCvarI r_lmap_atlas_limit("r_lmap_atlas_limit", "14", "Nuke lightmap cache if it reached this number of atlases.", CVAR_Archive);

VCvarB r_lmap_bsp_trace_static("r_lmap_bsp_trace_static", false, "Trace static lightmaps with BSP tree instead of blockmap?", CVAR_Archive);
VCvarB r_lmap_bsp_trace_dynamic("r_lmap_bsp_trace_dynamic", false, "Trace dynamic lightmaps with BSP tree instead of blockmap?", CVAR_Archive);

extern VCvarB dbg_adv_light_notrace_mark;


// ////////////////////////////////////////////////////////////////////////// //
enum { GridSize = VRenderLevelLightmap::LMapTraceInfo::GridSize };
enum { MaxSurfPoints = VRenderLevelLightmap::LMapTraceInfo::MaxSurfPoints };

static_assert((Filter4X ? (MaxSurfPoints >= GridSize*GridSize*16) : (MaxSurfPoints >= GridSize*GridSize*4)), "invalid grid size");

vuint32 blocklightsr[GridSize*GridSize];
vuint32 blocklightsg[GridSize*GridSize];
vuint32 blocklightsb[GridSize*GridSize];
static vuint32 blockaddlightsr[GridSize*GridSize];
static vuint32 blockaddlightsg[GridSize*GridSize];
static vuint32 blockaddlightsb[GridSize*GridSize];

#ifdef VV_EXPERIMENTAL_LMAP_FILTER
static vuint32 blocklightsrNew[GridSize*GridSize];
static vuint32 blocklightsgNew[GridSize*GridSize];
static vuint32 blocklightsbNew[GridSize*GridSize];
#endif

#ifdef VV_USELESS_SUBTRACTIVE_LIGHT_CODE
// subtractive
static vuint32 blocklightsrS[GridSize*GridSize];
static vuint32 blocklightsgS[GridSize*GridSize];
static vuint32 blocklightsbS[GridSize*GridSize];
#endif

int light_mem = 0;


// ////////////////////////////////////////////////////////////////////////// //
// *4 for extra filtering
static vuint8 lightmapHit[MaxSurfPoints];
static float lightmap[MaxSurfPoints];
static float lightmapr[MaxSurfPoints];
static float lightmapg[MaxSurfPoints];
static float lightmapb[MaxSurfPoints];
// set in lightmap merge code
static bool hasOverbright; // has overbright component?
static bool isColored; // is lightmap colored?


//==========================================================================
//
//  getSurfLightLevelInt
//
//==========================================================================
static inline int getSurfLightLevelInt (const surface_t *surf) {
  if (r_glow_flat && surf && !surf->seg && surf->subsector) {
    const sector_t *sec = surf->subsector->sector;
    //FIXME: check actual view height here
    if (sec && !sec->heightsec) {
      if (sec->floor.pic && surf->GetNormalZ() > 0.0f) {
        VTexture *gtex = GTextureManager(sec->floor.pic);
        if (gtex && gtex->Type != TEXTYPE_Null && gtex->IsGlowFullbright()) return 255;
      }
      if (sec->ceiling.pic && surf->GetNormalZ() < 0.0f) {
        VTexture *gtex = GTextureManager(sec->ceiling.pic);
        if (gtex && gtex->Type != TEXTYPE_Null && gtex->IsGlowFullbright()) return 255;
      }
    }
  }
  if (!surf) return 0;
  if (!r_allow_ambient) return clampToByte(r_ambient_min.asInt());
  int slins = (surf->Light>>24)&0xff;
  slins = max2(slins, r_ambient_min.asInt());
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
    //FIXME: check actual view height here
    if (sec && !sec->heightsec) {
      if (sec->floor.pic && surf->GetNormalZ() > 0.0f) {
        VTexture *gtex = GTextureManager(sec->floor.pic);
        if (gtex && gtex->Type != TEXTYPE_Null && gtex->IsGlowFullbright()) return (surf->Light&0xffffffu)|0xff000000u;
      }
      if (sec->ceiling.pic && surf->GetNormalZ() < 0.0f) {
        VTexture *gtex = GTextureManager(sec->ceiling.pic);
        if (gtex && gtex->Type != TEXTYPE_Null && gtex->IsGlowFullbright()) return (surf->Light&0xffffffu)|0xff000000u;
      }
    }
  }
  if (!surf) return 0;
  int slins = (r_allow_ambient ? (surf->Light>>24)&0xff : r_ambient_min.asInt());
  slins = max2(slins, r_ambient_min.asInt());
  //if (slins > 255) slins = 255;
  return (surf->Light&0xffffffu)|(((vuint32)clampToByte(slins))<<24);
}


//==========================================================================
//
//  VRenderLevelLightmap::CastStaticRay
//
//  Returns the distance between the points, or 0 if blocked
//
//==========================================================================
float VRenderLevelLightmap::CastStaticRay (sector_t *ssector, const TVec &p1, const TVec &p2, float squaredist) {
  const TVec delta = p2-p1;
  const float t = DotProduct(delta, delta);
  if (t >= squaredist) return 0.0f; // too far away
  if (t <= 2.0f) return 1.0f; // at light point

  if (!r_lmap_bsp_trace_static) {
    if (!Level->CastLightRay(ssector, p1, p2)) return 0.0f; // ray was blocked
  } else {
    linetrace_t Trace;
    if (!Level->TraceLine(Trace, p1, p2, SPF_NOBLOCKSIGHT)) return 0.0f; // ray was blocked
  }

  return sqrtf(t);
  //return 1.0f/fastInvSqrtf(t); //k8: not much faster
}


//==========================================================================
//
//  VRenderLevelLightmap::CalcMinMaxs
//
//==========================================================================
void VRenderLevelLightmap::CalcMinMaxs (LMapTraceInfo &lmi, const surface_t *surf) {
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
//  VRenderLevelLightmap::CalcFaceVectors
//
//  fills in texorg, worldtotex, and textoworld
//
//==========================================================================
bool VRenderLevelLightmap::CalcFaceVectors (LMapTraceInfo &lmi, const surface_t *surf) {
  const texinfo_t *tex = surf->texinfo;

  lmi.worldtotex[0] = tex->saxisLM;
  lmi.worldtotex[1] = tex->taxisLM;

  // calculate a normal to the texture axis
  // points can be moved along this without changing their S/T
  TVec texnormal(
    tex->taxisLM.y*tex->saxisLM.z-tex->taxisLM.z*tex->saxisLM.y,
    tex->taxisLM.z*tex->saxisLM.x-tex->taxisLM.x*tex->saxisLM.z,
    tex->taxisLM.x*tex->saxisLM.y-tex->taxisLM.y*tex->saxisLM.x);
  texnormal.normaliseInPlace();
  if (!isFiniteF(texnormal.x)) return false; // no need to check other coords

  // flip it towards plane normal
  float distscale = DotProduct(texnormal, surf->GetNormal());
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
    const float dist = DotProduct(lmi.worldtotex[i], surf->GetNormal())*distscale;
    lmi.textoworld[i] = lmi.worldtotex[i]-texnormal*dist;
    lmi.textoworld[i] = lmi.textoworld[i]*len*len;
    //lmi.textoworld[i] = lmi.textoworld[i]*(1.0f/len)*(1.0f/len);
  }

  // calculate texorg on the texture plane
  for (int i = 0; i < 3; ++i) {
    //lmi.texorg[i] = -tex->soffs*lmi.textoworld[0][i]-tex->toffs*lmi.textoworld[1][i];
    lmi.texorg[i] = 0;
  }

  // project back to the face plane
  {
    const float dist = (DotProduct(lmi.texorg, surf->GetNormal())-surf->GetDist()-1.0f)*distscale;
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
//  VRenderLevelLightmap::CalcPoints
//
//  for each texture aligned grid point, back project onto the plane
//  to get the world xyz value of the sample point
//
//  for dynlights, set `lowres` to `true`
//  setting `lowres` skips point visibility determination, because it is
//  done in `AddDynamicLights()`.
//
//==========================================================================
void VRenderLevelLightmap::CalcPoints (LMapTraceInfo &lmi, const surface_t *surf, bool lowres) {
  int w, h;
  float step;
  float starts, startt;
  linetrace_t Trace;

  bool doExtra = (r_lmap_filtering > 2);
  if (ldr_extrasamples_override >= 0) doExtra = (ldr_extrasamples_override > 0);
  if (!lowres && doExtra) {
    // extra filtering
    if (Filter4X) {
      w = ((surf->extents[0]>>4)+1)*4;
      h = ((surf->extents[1]>>4)+1)*4;
      starts = surf->texturemins[0]-16;
      startt = surf->texturemins[1]-16;
      step = 4;
    } else {
      w = ((surf->extents[0]>>4)+1)*2;
      h = ((surf->extents[1]>>4)+1)*2;
      starts = surf->texturemins[0]-8;
      startt = surf->texturemins[1]-8;
      step = 8;
    }
  } else {
    w = (surf->extents[0]>>4)+1;
    h = (surf->extents[1]>>4)+1;
    starts = surf->texturemins[0];
    startt = surf->texturemins[1];
    step = 16;
  }
  lmi.didExtra = doExtra;

  // fill in surforg
  // the points are biased towards the center of the surface
  // to help avoid edge cases just inside walls
  const float mids = surf->texturemins[0]+surf->extents[0]/2.0f;
  const float midt = surf->texturemins[1]+surf->extents[1]/2.0f;
  const TVec facemid = lmi.texorg+lmi.textoworld[0]*mids+lmi.textoworld[1]*midt;

  lmi.numsurfpt = w*h;
  bool doPointCheck = false;
  // *4 for extra filtering
  if (lmi.numsurfpt > MaxSurfPoints) {
    GCon->Logf(NAME_Warning, "too many points in lightmap tracer");
    lmi.numsurfpt = MaxSurfPoints;
    doPointCheck = true;
  }

  TVec *spt = lmi.surfpt;
  for (int t = 0; t < h; ++t) {
    for (int s = 0; s < w; ++s, ++spt) {
      if (doPointCheck && (int)(ptrdiff_t)(spt-lmi.surfpt) >= MaxSurfPoints) return;
      float us = starts+s*step;
      float ut = startt+t*step;

      // if a line can be traced from surf to facemid, the point is good
      for (int i = 0; i < 6; ++i) {
        // calculate texture point
        //*spt = lmi.texorg+lmi.textoworld[0]*us+lmi.textoworld[1]*ut;
        *spt = lmi.calcTexPoint(us, ut);
        if (lowres) break;
        //const TVec fms = facemid-(*spt);
        //if (length2DSquared(fms) < 0.1f) break; // same point, got it
        if (Level->TraceLine(Trace, facemid, *spt, SPF_NOBLOCKSIGHT)) break; // got it

        if (i&1) {
          CP_FIX_UT(s);
        } else {
          CP_FIX_UT(t);
        }

        // move surf 8 pixels towards the center
        const TVec fms = facemid-(*spt);
        *spt += 8*Normalise(fms);
      }
      //if (i == 2) ++c_bad;
    }
  }
}


//==========================================================================
//
//  FilterLightmap
//
//==========================================================================
template<typename T> void FilterLightmap (T *lmap, const int wdt, const int hgt) {
  if (!r_lmap_lowfilter) return;
  if (!lmap || (wdt < 2 && hgt < 2)) return;
  static T *lmnew = nullptr;
  static int lmnewSize = 0;
  if (wdt*hgt > lmnewSize) {
    lmnewSize = wdt*hgt;
    lmnew = (T *)Z_Realloc(lmnew, lmnewSize*sizeof(T));
  }
  for (int y = 0; y < hgt; ++y) {
    for (int x = 0; x < wdt; ++x) {
      int count = 0;
      float sum = 0;
      for (int dy = -1; dy < 2; ++dy) {
        const int sy = y+dy;
        if (sy < 0 || sy >= hgt) continue;
        T *row = lmap+(sy*wdt);
        for (int dx = -1; dx < 2; ++dx) {
          const int sx = x+dx;
          if (sx < 0 || sx >= wdt) continue;
          if ((dx|dy) == 0) continue;
          ++count;
          sum += row[sx];
        }
      }
      if (!count) continue;
      sum /= (float)count;
      float v = lmap[y*wdt+x];
      sum = (sum+v)*0.5f;
      lmnew[y*wdt+x] = sum;
    }
  }
  memcpy(lmap, lmnew, lmnewSize*sizeof(T));
}


//==========================================================================
//
//  VRenderLevelLightmap::SingleLightFace
//
//  light face with static light
//
//==========================================================================
void VRenderLevelLightmap::SingleLightFace (LMapTraceInfo &lmi, light_t *light, surface_t *surf, const vuint8 *facevis) {
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

  //float orgdist = DotProduct(light->origin, surf->GetNormal())-surf->GetDist();
  const float orgdist = surf->PointDistance(light->origin);
  // don't bother with lights behind the surface, or too far away
  if (orgdist <= -0.1f || orgdist >= light->radius) return;

  TVec lorg = light->origin;

  // drop lights inside sectors without height
  if (light->leafnum >= 0 && light->leafnum < Level->NumSubsectors) {
    const sector_t *sec = Level->Subsectors[light->leafnum].sector;
    if (!CheckValidLightPosRough(lorg, sec)) return;
  }

  lmi.setupSpotlight(light->coneDirection, light->coneAngle);

  // calc points only when surface may be lit by a light
  if (!lmi.pointsCalced) {
    if (!CalcFaceVectors(lmi, surf)) {
      GCon->Logf(NAME_Warning, "cannot calculate lightmap vectors");
      lmi.numsurfpt = 0;
      memset(lightmap, 0, sizeof(lightmap));
      memset(lightmapr, 0, sizeof(lightmapr));
      memset(lightmapg, 0, sizeof(lightmapg));
      memset(lightmapb, 0, sizeof(lightmapb));
      return;
    }

    CalcPoints(lmi, surf, false);
    lmi.pointsCalced = true;
    memset(lightmap, 0, lmi.numsurfpt*sizeof(lightmap[0]));
    memset(lightmapr, 0, lmi.numsurfpt*sizeof(lightmapr[0]));
    memset(lightmapg, 0, lmi.numsurfpt*sizeof(lightmapg[0]));
    memset(lightmapb, 0, lmi.numsurfpt*sizeof(lightmapb[0]));
  }

  // check it for real
  const TVec *spt = lmi.surfpt;
  const float squaredist = light->radius*light->radius;
  const float rmul = ((light->color>>16)&255)/255.0f;
  const float gmul = ((light->color>>8)&255)/255.0f;
  const float bmul = (light->color&255)/255.0f;

  int w = (surf->extents[0]>>4)+1;
  int h = (surf->extents[1]>>4)+1;

  bool doMidFilter = (!lmi.didExtra && r_lmap_filtering > 0);
  if (doMidFilter) memset(lightmapHit, 0, /*w*h*/lmi.numsurfpt);

  bool wasAnyHit = false;
  sector_t *ssector = Level->PointInSubsector(lorg)->sector;
  const TVec lnormal = surf->GetNormal();
  //const TVec lorg = light->origin;

  float attn = 1.0f;
  for (int c = 0; c < lmi.numsurfpt; ++c, ++spt) {
    // check spotlight cone
    if (lmi.spotLight) {
      //spt = lmi.calcTexPoint(starts+s*step, startt+t*step);
      if (length2DSquared((*spt)-lorg) > 2*2) {
        attn = spt->CalcSpotlightAttMult(lorg, lmi.coneDir, lmi.coneAngle);
        if (attn == 0.0f) continue;
      } else {
        attn = 1.0f;
      }
    }

    const float raydist = CastStaticRay(ssector, lorg+lnormal, (*spt)+lnormal, squaredist);
    if (raydist <= 0.0f) {
      // light ray is blocked
      /*
      lightmap[c] += 255.0f;
      lightmapg[c] += 255.0f;
      isColored = true;
      lmi.light_hit = true;
      */
      continue;
    }
    /*
    if (CastRay(Level->PointInSubsector(*spt)->sector, *spt, lorg, squaredist) <= 0.0f) {
      continue;
    }
    */

    TVec incoming = lorg-(*spt);
    if (!incoming.isZero()) {
      incoming.normaliseInPlace();
      if (!incoming.isValid()) {
        lightmap[c] += 255.0f;
        lightmapr[c] += 255.0f;
        isColored = true;
        lmi.light_hit = true;
        continue;
      }
    }

    float angle = DotProduct(incoming, lnormal);
    angle = 0.5f+0.5f*angle;

    float add = (light->radius-raydist)*angle*attn;
    if (add <= 0.0f) continue;
    // without this, lights with huge radius will overbright everything
    if (add > 255.0f) add = 255.0f;

    if (doMidFilter) { wasAnyHit = true; lightmapHit[c] = 1; }

    lightmap[c] += add;
    lightmapr[c] += add*rmul;
    lightmapg[c] += add*gmul;
    lightmapb[c] += add*bmul;
    // ignore really tiny lights
    if (lightmap[c] > 1) {
      lmi.light_hit = true;
      if (light->color != 0xffffffff) isColored = true;
    }
  }

  if (doMidFilter && wasAnyHit) {
    //GCon->Logf("w=%d; h=%d; num=%d; cnt=%d", w, h, w*h, lmi.numsurfpt);
   again:
    const vuint8 *lht = lightmapHit;
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x, ++lht) {
        const int laddr = y*w+x;
        if (laddr >= lmi.numsurfpt) goto doneall;
        if (*lht) continue;
        // check if any neighbour point was hit
        // if not, don't bother to do better tracing
        bool doit = false;
        for (int dy = -1; dy < 2; ++dy) {
          const int sy = y+dy;
          if (sy < 0 || sy >= h) continue;
          const vuint8 *row = lightmapHit+(sy*w);
          for (int dx = -1; dx < 2; ++dx) {
            if ((dx|dy) == 0) continue;
            const int sx = x+dx;
            if (sx < 0 || sx >= w) continue;
            if (row[sx]) { doit = true; goto done; }
          }
        }
       done:
        if (doit) {
          TVec pt = lmi.surfpt[laddr];
          float raydist = 0.0f;
          for (int dy = -1; dy < 2; ++dy) {
            for (int dx = -1; dx < 2; ++dx) {
              for (int dz = -1; dz < 2; ++dz) {
                if ((dx|dy|dz) == 0) continue;
                raydist = CastStaticRay(ssector, lorg+lnormal, pt+TVec(4*dx, 4*dy, 4*dz), squaredist);
                if (raydist > 0.0f) goto donetrace;
              }
            }
          }
          continue;
         donetrace:
          //GCon->Logf("x=%d; y=%d; w=%d; h=%d; raydist=%g", x, y, w, h, raydist);

          TVec incoming = lorg-pt;
          if (!incoming.isZero()) {
            incoming.normaliseInPlace();
            if (!incoming.isValid()) continue;
          }

          float angle = DotProduct(incoming, lnormal);
          angle = 0.5f+0.5f*angle;

          float add = (light->radius-raydist)*angle*0.75f;
          if (add <= 0.0f) continue;
          // without this, lights with huge radius will overbright everything
          if (add > 255.0f) add = 255.0f;

          lightmap[laddr] += add;
          lightmapr[laddr] += add*rmul;
          lightmapg[laddr] += add*gmul;
          lightmapb[laddr] += add*bmul;
          // ignore really tiny lights
          if (lightmap[laddr] > 1) {
            lmi.light_hit = true;
            if (light->color != 0xffffffff) isColored = true;
          }
          lightmapHit[laddr] = 1;
          if (r_lmap_filtering == 2) goto again;
        }
      }
    }
    doneall: (void)0;
  }
}


// ////////////////////////////////////////////////////////////////////////// //
#define FILTER_LMAP_EXTRA(lmv_)  do { \
  if (Filter4X) { \
    total = 0; \
    for (int dy = 0; dy < 4; ++dy) { \
      for (int dx = 0; dx < 4; ++dx) { \
        total += lmv_[(t*4+dy)*w*4+s*4+dx]; \
      } \
    } \
    total *= 1.0f/16.0f; \
  } else { \
    total = lmv_[t*w*4+s*2]+ \
            lmv_[t*2*w*2+s*2+1]+ \
            lmv_[(t*2+1)*w*2+s*2]+ \
            lmv_[(t*2+1)*w*2+s*2+1]; \
    total *= 0.25f; \
  } \
} while (0)


//==========================================================================
//
//  VRenderLevelLightmap::LightFace
//
//==========================================================================
void VRenderLevelLightmap::LightFace (surface_t *surf, subsector_t *leaf) {
  if (surf->count < 3) return; // wtf?!

  LMapTraceInfo lmi;
  //lmi.points_calculated = false;
  vassert(!lmi.pointsCalced);

  const vuint8 *facevis = (leaf && Level->HasPVS() ? Level->LeafPVS(leaf) : nullptr);
  lmi.light_hit = false;
  isColored = false;

  // cast all static lights
  CalcMinMaxs(lmi, surf);
  if (r_static_lights) {
    light_t *stl = Lights.ptr();
    for (int i = Lights.length(); i--; ++stl) SingleLightFace(lmi, stl, surf, facevis);
  }

  if (!lmi.light_hit) {
    // no light hit it
    if (surf->lightmap) {
      light_mem -= surf->lmsize;
      Z_Free(surf->lightmap);
      surf->lightmap = nullptr;
      surf->lmsize = 0;
    }
    if (surf->lightmap_rgb) {
      light_mem -= surf->lmrgbsize;
      Z_Free(surf->lightmap_rgb);
      surf->lightmap_rgb = nullptr;
      surf->lmrgbsize = 0;
    }
    return;
  }

  const int w = (surf->extents[0]>>4)+1;
  const int h = (surf->extents[1]>>4)+1;
  vassert(w > 0);
  vassert(h > 0);

  // if the surface already has a static lightmap, we will reuse it,
  // otherwise we must allocate a new one
  if (isColored) {
    // need colored lightmap
    int sz = w*h*(int)sizeof(surf->lightmap_rgb[0]);
    if (surf->lmrgbsize != sz) {
      light_mem -= surf->lmrgbsize;
      light_mem += sz;
      surf->lmrgbsize = sz;
      surf->lightmap_rgb = (rgb_t *)Z_Realloc(surf->lightmap_rgb, sz);
    }

    if (!lmi.didExtra) {
      if (w*h <= MaxSurfPoints) {
        FilterLightmap(lightmapr, w, h);
        FilterLightmap(lightmapg, w, h);
        FilterLightmap(lightmapb, w, h);
      } else {
        GCon->Logf(NAME_Warning, "skipped filter for lightmap of size %dx%d", w, h);
      }
    }

    //HACK!
    if (w*h > MaxSurfPoints) lmi.didExtra = false;

    int i = 0;
    for (int t = 0; t < h; ++t) {
      for (int s = 0; s < w; ++s, ++i) {
        if (i > MaxSurfPoints) i = MaxSurfPoints-1;
        float total;
        if (lmi.didExtra) {
          // filtered sample
          FILTER_LMAP_EXTRA(lightmapr);
        } else {
          total = lightmapr[i];
        }
        surf->lightmap_rgb[i].r = clampToByte((int)total);

        if (lmi.didExtra) {
          // filtered sample
          FILTER_LMAP_EXTRA(lightmapg);
        } else {
          total = lightmapg[i];
        }
        surf->lightmap_rgb[i].g = clampToByte((int)total);

        if (lmi.didExtra) {
          // filtered sample
          FILTER_LMAP_EXTRA(lightmapb);
        } else {
          total = lightmapb[i];
        }
        surf->lightmap_rgb[i].b = clampToByte((int)total);
      }
    }
  } else {
    // free rgb lightmap
    if (surf->lightmap_rgb) {
      light_mem -= surf->lmrgbsize;
      Z_Free(surf->lightmap_rgb);
      surf->lightmap_rgb = nullptr;
      surf->lmrgbsize = 0;
    }
  }

  {
    // monochrome lightmap
    int sz = w*h*(int)sizeof(surf->lightmap[0]);
    if (surf->lmsize != sz) {
      light_mem -= surf->lmsize;
      light_mem += sz;
      surf->lightmap = (vuint8 *)Z_Realloc(surf->lightmap, sz);
      surf->lmsize = sz;
    }

    if (!lmi.didExtra) {
      if (w*h <= MaxSurfPoints) {
        FilterLightmap(lightmap, w, h);
      } else {
        GCon->Logf(NAME_Warning, "skipped filter for lightmap of size %dx%d", w, h);
      }
    }

    //HACK!
    if (w*h > MaxSurfPoints) lmi.didExtra = false;

    int i = 0;
    for (int t = 0; t < h; ++t) {
      for (int s = 0; s < w; ++s, ++i) {
        if (i > MaxSurfPoints) i = MaxSurfPoints-1;
        float total;
        if (lmi.didExtra) {
          // filtered sample
          FILTER_LMAP_EXTRA(lightmap);
        } else {
          total = lightmap[i];
        }
        surf->lightmap[i] = clampToByte((int)total);
      }
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
//  VRenderLevelLightmap::AddDynamicLights
//
//  dlight frame already checked
//
//==========================================================================
void VRenderLevelLightmap::AddDynamicLights (surface_t *surf) {
  if (surf->count < 3) return; // wtf?!

  //float mids = 0, midt = 0;
  //TVec facemid = TVec(0,0,0);
  LMapTraceInfo lmi;
  vassert(!lmi.pointsCalced);

  int smax = (surf->extents[0]>>4)+1;
  int tmax = (surf->extents[1]>>4)+1;
  if (smax > GridSize) smax = GridSize;
  if (tmax > GridSize) tmax = GridSize;

  const texinfo_t *tex = surf->texinfo;

  /*
  const float starts = surf->texturemins[0];
  const float startt = surf->texturemins[1];
  const float step = 16;
  */

  const bool hasPVS = Level->HasPVS();
  const bool doCheckTrace = (r_dynamic_clip && r_dynamic_clip_more && r_allow_shadows);
  const bool useBSPTrace = r_lmap_bsp_trace_dynamic.asBool();
  linetrace_t Trace;

  for (unsigned lnum = 0; lnum < MAX_DLIGHTS; ++lnum) {
    if (!(surf->dlightbits&(1U<<lnum))) continue; // not lit by this light

    const dlight_t &dl = DLights[lnum];
    //if (dl.type == DLTYPE_Subtractive) GCon->Logf("***SUBTRACTIVE LIGHT!");
    if (dl.type == DLTYPE_Subtractive && !r_allow_subtractive_lights) continue;

    const int xnfo = dlinfo[lnum].needTrace;
    if (!xnfo) continue;

    const TVec dorg = dl.origin;
    float rad = dl.radius;
    float dist = surf->PointDistance(dorg);
    // don't bother with lights behind the surface, or too far away
    if (dist <= -0.1f || dist >= rad) continue; // was with `r_dynamic_clip` check; but there is no reason to not check this
    if (dist < 0.0f) dist = 0.0f; // clamp it

    rad -= dist;
    float minlight = dl.minlight;
    if (rad < minlight) continue;
    minlight = rad-minlight;

    if (hasPVS && r_dynamic_clip_pvs && surf->subsector) {
      const subsector_t *sub = surf->subsector; //Level->PointInSubsector(impact);
      const vuint8 *dyn_facevis = Level->LeafPVS(sub);
      //int leafnum = Level->PointInSubsector(dorg)-Level->Subsectors;
      int leafnum = dlinfo[lnum].leafnum;
      if (leafnum < 0) continue;
      // check potential visibility
      if (!(dyn_facevis[leafnum>>3]&(1<<(leafnum&7)))) continue;
    }

    TVec impact = dorg-surf->GetNormal()*dist;
    //const TVec surfOffs = surf->GetNormal()*4.0f; // don't land exactly on a surface

    //TODO: we can use clipper to check if destination subsector is occluded
    bool needProperTrace = (doCheckTrace && xnfo > 0 && (dl.flags&dlight_t::NoShadow) == 0);

    ++gf_dynlights_processed;
    if (needProperTrace) ++gf_dynlights_traced;

    vuint32 dlcolor = (!needProperTrace && dbg_adv_light_notrace_mark ? 0xffff00ffU : dl.color);

    const float rmul = (dlcolor>>16)&255;
    const float gmul = (dlcolor>>8)&255;
    const float bmul = dlcolor&255;

    TVec local;
    local.x = DotProduct(impact, tex->saxisLM)/*+tex->soffs*/;
    local.y = DotProduct(impact, tex->taxisLM)/*+tex->toffs*/;
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
    sector_t *surfsector = (surf->subsector ? surf->subsector->sector : nullptr);
    float attn = 1.0f;

    const TVec *spt = lmi.surfpt;
    for (int t = 0; t < tmax; ++t) {
      int td = (int)local.y-t*16;
      if (td < 0) td = -td;
      for (int s = 0; s < smax; ++s, ++spt) {
        int sd = (int)local.x-s*16;
        if (sd < 0) sd = -sd;
        const float ptdist = (sd > td ? sd+(td>>1) : td+(sd>>1));
        if (ptdist < minlight) {
          // check spotlight cone
          if (lmi.spotLight) {
            //spt = lmi.calcTexPoint(starts+s*step, startt+t*step);
            if (length2DSquared((*spt)-dorg) > 2*2) {
              attn = spt->CalcSpotlightAttMult(dorg, lmi.coneDir, lmi.coneAngle);
              if (attn == 0.0f) continue;
            } else {
              attn = 1.0f;
            }
          }
          float add = (rad-ptdist)*attn;
          if (add <= 0.0f) continue;
          // without this, lights with huge radius will overbright everything
          if (add > 255.0f) add = 255.0f;
          // do more dynlight clipping
          if (needProperTrace) {
            //if (!lmi.spotLight) spt = lmi.calcTexPoint(starts+s*step, startt+t*step);
            if (length2DSquared((*spt)-dorg) > 2*2) {
              const TVec &p2 = *spt;
              //const TVec p2 = (*spt)+surfOffs;
              if (!useBSPTrace) {
                if (!Level->CastLightRay(Level->Subsectors[dlinfo[lnum].leafnum].sector, dorg, p2, surfsector)) {
                  #ifdef VV_DEBUG_BMAP_TRACER
                  if (!Level->TraceLine(Trace, dorg, p2, SPF_NOBLOCKSIGHT)) continue;
                  GCon->Logf(NAME_Debug, "TRACEvsTRACE: org=(%g,%g,%g); dest=(%g,%g,%g); bmap=BLOCK; bsp=NON-BLOCK", dorg.x, dorg.y, dorg.z, p2.x, p2.y, p2.z);
                  #endif
                  continue;
                }
                #ifdef VV_DEBUG_BMAP_TRACER
                else {
                  if (!Level->TraceLine(Trace, dorg, p2, SPF_NOBLOCKSIGHT)) {
                    GCon->Logf(NAME_Debug, "TRACEvsTRACE: org=(%g,%g,%g); dest=(%g,%g,%g); bmap=NON-BLOCK; bsp=BLOCK", dorg.x, dorg.y, dorg.z, p2.x, p2.y, p2.z);
                    continue;
                  }
                }
                #endif
              } else {
                if (!Level->TraceLine(Trace, dorg, p2, SPF_NOBLOCKSIGHT)) continue; // ray was blocked
              }
            }
          }
          int i = t*smax+s;
          if (dl.type != DLTYPE_Subtractive) {
            //blocklights[i] += (rad-ptdist)*256.0f;
            blocklightsr[i] += rmul*add;
            blocklightsg[i] += gmul*add;
            blocklightsb[i] += bmul*add;
          } else {
            #ifdef VV_USELESS_SUBTRACTIVE_LIGHT_CODE
            //blocklightsS[i] += (rad-ptdist)*256.0f;
            blocklightsrS[i] += rmul*add;
            blocklightsgS[i] += gmul*add;
            blocklightsbS[i] += bmul*add;
            #endif
          }
          if (dlcolor != 0xffffffff) isColored = true;
        }
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::InvalidateSurfacesLMaps
//
//==========================================================================
void VRenderLevelLightmap::InvalidateSurfacesLMaps (const TVec &org, float radius, surface_t *surf) {
  for (; surf; surf = surf->next) {
    if (surf->count < 3) continue; // just in case
    if (!surf->SphereTouches(org, radius)) continue;
    if (!invalidateRelight) {
      if (surf->lightmap || surf->lightmap_rgb) {
        surf->drawflags |= surface_t::DF_CALC_LMAP;
      }
    } else {
      surf->drawflags |= surface_t::DF_CALC_LMAP;
    }
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::InvalidateLineLMaps
//
//==========================================================================
void VRenderLevelLightmap::InvalidateLineLMaps (const TVec &org, float radius, drawseg_t *dseg) {
  const seg_t *seg = dseg->seg;

  if (!seg->linedef) return; // miniseg
  if (!seg->SphereTouches(org, radius)) return;

  if (dseg->mid) InvalidateSurfacesLMaps(org, radius, dseg->mid->surfs);
  if (seg->backsector) {
    // two sided line
    if (dseg->top) InvalidateSurfacesLMaps(org, radius, dseg->top->surfs);
    // no lightmaps on sky anyway
    //if (dseg->topsky) InvalidateSurfacesLMaps(org, radius, dseg->topsky->surfs);
    if (dseg->bot) InvalidateSurfacesLMaps(org, radius, dseg->bot->surfs);
    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      InvalidateSurfacesLMaps(org, radius, sp->surfs);
    }
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::InvalidateSubsectorLMaps
//
//==========================================================================
void VRenderLevelLightmap::InvalidateSubsectorLMaps (const TVec &org, float radius, int num) {
  subsector_t *sub = &Level->Subsectors[num];
  if (!sub->sector->linecount) return; // skip sectors containing original polyobjs
  // polyobj
  if (sub->HasPObjs()) {
    for (auto &&it : sub->PObjFirst()) {
      polyobj_t *pobj = it.value();
      seg_t **polySeg = pobj->segs;
      for (int polyCount = pobj->numsegs; polyCount--; ++polySeg) {
        InvalidateLineLMaps(org, radius, (*polySeg)->drawsegs);
      }
    }
  }
  //TODO: invalidate only relevant segs
  for (subregion_t *subregion = sub->regions; subregion; subregion = subregion->next) {
    drawseg_t *ds = subregion->lines;
    for (int dscount = sub->numlines; dscount--; ++ds) {
      InvalidateLineLMaps(org, radius, ds);
    }
    if (subregion->realfloor) InvalidateSurfacesLMaps(org, radius, subregion->realfloor->surfs);
    if (subregion->realceil) InvalidateSurfacesLMaps(org, radius, subregion->realceil->surfs);
    if (subregion->fakefloor) InvalidateSurfacesLMaps(org, radius, subregion->fakefloor->surfs);
    if (subregion->fakeceil) InvalidateSurfacesLMaps(org, radius, subregion->fakeceil->surfs);
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::InvalidateBSPNodeLMaps
//
//==========================================================================
void VRenderLevelLightmap::InvalidateBSPNodeLMaps (const TVec &org, float radius, int bspnum, const float *bbox) {
  if (bspnum == -1) {
    return InvalidateSubsectorLMaps(org, radius, 0);
  }

#ifdef VV_CLIPPER_FULL_CHECK
  if (LightClip.ClipIsFull()) return;
#endif

  if (!LightClip.ClipLightIsBBoxVisible(bbox)) return;
  if (!CheckSphereVsAABBIgnoreZ(bbox, org, radius)) return;

  // found a subsector?
  if (BSPIDX_IS_NON_LEAF(bspnum)) {
    node_t *bsp = &Level->Nodes[bspnum];
    // decide which side the light is on
    const float dist = DotProduct(org, bsp->normal)-bsp->dist;
    if (dist > radius) {
      // light is completely on front side
      return InvalidateBSPNodeLMaps(org, radius, bsp->children[0], bsp->bbox[0]);
    } else if (dist < -radius) {
      // light is completely on back side
      return InvalidateBSPNodeLMaps(org, radius, bsp->children[1], bsp->bbox[1]);
    } else {
      //int side = bsp->PointOnSide(CurrLightPos);
      unsigned side = (unsigned)(dist <= 0.0f);
      // recursively divide front space
      InvalidateBSPNodeLMaps(org, radius, bsp->children[side], bsp->bbox[side]);
      // possibly divide back space
      side ^= 1;
      return InvalidateBSPNodeLMaps(org, radius, bsp->children[side], bsp->bbox[side]);
    }
  } else {
    subsector_t *sub = &Level->Subsectors[BSPIDX_LEAF_SUBSECTOR(bspnum)];
    if (!LightClip.ClipLightCheckSubsector(sub, false)) return;
    InvalidateSubsectorLMaps(org, radius, BSPIDX_LEAF_SUBSECTOR(bspnum));
    LightClip.ClipLightAddSubsectorSegs(sub, false);
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::InvalidateStaticLightmaps
//
//==========================================================================
void VRenderLevelLightmap::InvalidateStaticLightmaps (const TVec &org, float radius, bool relight) {
  //FIXME: make this faster!
  if (radius < 2.0f) return;
  invalidateRelight = relight;
  float bbox[6];
#if 0
  subsector_t *sub = Level->Subsectors;
  for (int count = Level->NumSubsectors; count--; ++sub) {
    if (!sub->sector->linecount) continue; // skip sectors containing original polyobjs
    Level->GetSubsectorBBox(sub, bbox);
    if (!CheckSphereVsAABBIgnoreZ(bbox, org, radius)) continue;
    //GCon->Logf("invalidating subsector %d", (int)(ptrdiff_t)(sub-Level->Subsectors));
    InvalidateSubsectorLMaps(org, radius, (int)(ptrdiff_t)(sub-Level->Subsectors));
  }
#else
  bbox[0] = bbox[1] = bbox[2] = -999999.0f;
  bbox[3] = bbox[4] = bbox[5] = 999999.0f;
  LightClip.ClearClipNodes(org, Level, radius);
  InvalidateBSPNodeLMaps(org, radius, Level->NumNodes-1, bbox);
#endif
}


//==========================================================================
//
//  xblight
//
//==========================================================================
#ifdef VV_USELESS_SUBTRACTIVE_LIGHT_CODE
static inline int xblight (int add, int sub) {
  enum {
    minlight = 256,
    maxlight = 0xff00,
  };
  int t = 255*256-add+sub;
  //if (sub > 0) t = maxlight;
  if (t < minlight) t = minlight; else if (t > maxlight) t = maxlight;
  return t;
}
#else
static inline int xblight (int add) {
  enum {
    minlight = 256,
    maxlight = 0xff00,
  };
  const int t = 255*256-add;
  return (t < minlight ? minlight : t > maxlight ? maxlight : t);
}
#endif


//==========================================================================
//
//  VRenderLevelLightmap::BuildLightMap
//
//  combine and scale multiple lightmaps into the 8.8 format in blocklights
//
//==========================================================================
void VRenderLevelLightmap::BuildLightMap (surface_t *surf) {
  if (surf->count < 3) {
    surf->drawflags &= ~surface_t::DF_CALC_LMAP;
    return;
  }

  if (surf->drawflags&surface_t::DF_CALC_LMAP) {
    //if (surf->subsector) GCon->Logf("relighting subsector %d", (int)(ptrdiff_t)(surf->subsector-Level->Subsectors));
    surf->drawflags &= ~surface_t::DF_CALC_LMAP;
    //GCon->Logf("%p: Need to calculate static lightmap for subsector %p!", surf, surf->subsector);
    if (surf->subsector) LightFace(surf, surf->subsector);
  }

  isColored = false;
  hasOverbright = false;
  int smax = (surf->extents[0]>>4)+1;
  int tmax = (surf->extents[1]>>4)+1;
  vassert(smax > 0);
  vassert(tmax > 0);
  if (smax > GridSize) smax = GridSize;
  if (tmax > GridSize) tmax = GridSize;
  int size = smax*tmax;
  const vuint8 *lightmap = surf->lightmap;
  const rgb_t *lightmap_rgb = surf->lightmap_rgb;

  // clear to ambient
  int t = getSurfLightLevelInt(surf);
  t <<= 8;
  int tR = ((surf->Light>>16)&255)*t/255;
  int tG = ((surf->Light>>8)&255)*t/255;
  int tB = (surf->Light&255)*t/255;
  if (tR != tG || tR != tB) isColored = true;

  for (int i = 0; i < size; ++i) {
    //blocklights[i] = t;
    blocklightsr[i] = tR;
    blocklightsg[i] = tG;
    blocklightsb[i] = tB;
    blockaddlightsr[i] = blockaddlightsg[i] = blockaddlightsb[i] = 0;
    #ifdef VV_USELESS_SUBTRACTIVE_LIGHT_CODE
    /*blocklightsS[i] =*/ blocklightsrS[i] = blocklightsgS[i] = blocklightsbS[i] = 0;
    #endif
  }

  // sum lightmaps
  const bool overbright = r_lmap_overbright_static.asBool();
  if (lightmap_rgb) {
    if (!lightmap) Sys_Error("RGB lightmap without uncolored lightmap");
    isColored = true;
    for (int i = 0; i < size; ++i) {
      //blocklights[i] += lightmap[i]<<8;
      blocklightsr[i] += lightmap_rgb[i].r<<8;
      blocklightsg[i] += lightmap_rgb[i].g<<8;
      blocklightsb[i] += lightmap_rgb[i].b<<8;
      if (!overbright) {
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
      if (!overbright) {
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
  const float spcoeff = clampval(r_lmap_specular.asFloat(), 0.0f, 16.0f);
  for (unsigned i = 0; i < (unsigned)size; ++i) {
    #ifdef VV_USELESS_SUBTRACTIVE_LIGHT_CODE
    t = blocklightsr[i]-blocklightsrS[i];
    #else
    t = blocklightsr[i];
    #endif
    //if (t < 0) { t = 0; blocklightsr[i] = 0; } // subtractive light fix
    t -= 0x10000;
    if (t > 0) {
      t = int(spcoeff*t);
      if (t > 0xffff) t = 0xffff;
      blockaddlightsr[i] = t;
      hasOverbright = true;
    }

    #ifdef VV_USELESS_SUBTRACTIVE_LIGHT_CODE
    t = blocklightsg[i]-blocklightsgS[i];
    #else
    t = blocklightsg[i];
    #endif
    //if (t < 0) { t = 0; blocklightsg[i] = 0; } // subtractive light fix
    t -= 0x10000;
    if (t > 0) {
      t = int(spcoeff*t);
      if (t > 0xffff) t = 0xffff;
      blockaddlightsg[i] = t;
      hasOverbright = true;
    }

    #ifdef VV_USELESS_SUBTRACTIVE_LIGHT_CODE
    t = blocklightsb[i]-blocklightsbS[i];
    #else
    t = blocklightsb[i];
    #endif
    //if (t < 0) { t = 0; blocklightsb[i] = 0; } // subtractive light fix
    t -= 0x10000;
    if (t > 0) {
      t = int(spcoeff*t);
      if (t > 0xffff) t = 0xffff;
      blockaddlightsb[i] = t;
      hasOverbright = true;
    }
  }

  // bound, invert, and shift
  for (unsigned i = 0; i < (unsigned)size; ++i) {
    //if (blocklightsrS[i]|blocklightsgS[i]|blocklightsbS[i]) fprintf(stderr, "*** SBL: (%d,%d,%d)\n", blocklightsrS[i], blocklightsgS[i], blocklightsbS[i]);
    #ifdef VV_USELESS_SUBTRACTIVE_LIGHT_CODE
    blocklightsr[i] = xblight((int)blocklightsr[i], (int)blocklightsrS[i]);
    blocklightsg[i] = xblight((int)blocklightsg[i], (int)blocklightsgS[i]);
    blocklightsb[i] = xblight((int)blocklightsb[i], (int)blocklightsbS[i]);
    #else
    blocklightsr[i] = xblight((int)blocklightsr[i]);
    blocklightsg[i] = xblight((int)blocklightsg[i]);
    blocklightsb[i] = xblight((int)blocklightsb[i]);
    #endif

    /*
    blocklightsr[i] = 0xff00;
    blocklightsg[i] = 0x0100;
    blocklightsb[i] = 0xff00;
    */
  }

  #ifdef VV_EXPERIMENTAL_LMAP_FILTER
  enum {
    minlight = 256,
    maxlight = 0xff00,
  };

  #define DO_ONE_LMFILTER(lmc_)  do { \
    int v = \
      lmc_[pos-1]+lmc_[pos]+lmc_[pos+1]+ \
      lmc_[pos-1-smax]+lmc_[pos-smax]+lmc_[pos+1-smax]+ \
      lmc_[pos-1+smax]+lmc_[pos+smax]+lmc_[pos+1+smax]; \
    v /= 9; \
    if (v < minlight) v = minlight; else if (v > maxlight) v = maxlight; \
    lmc_ ## New[pos] = v; \
  } while (0)

  if (smax > 2 && tmax > 2) {
    for (int j = 1; j < tmax-1; ++j) {
      unsigned pos = j*smax+1;
      blocklightsrNew[pos-1] = blocklightsr[pos-1];
      blocklightsgNew[pos-1] = blocklightsg[pos-1];
      blocklightsbNew[pos-1] = blocklightsb[pos-1];
      for (int i = 1; i < smax-1; ++i, ++pos) {
        DO_ONE_LMFILTER(blocklightsr);
        DO_ONE_LMFILTER(blocklightsg);
        DO_ONE_LMFILTER(blocklightsb);
      }
      blocklightsrNew[pos] = blocklightsr[pos];
      blocklightsgNew[pos] = blocklightsg[pos];
      blocklightsbNew[pos] = blocklightsb[pos];
    }
    memcpy(blocklightsr+smax, blocklightsrNew+smax, smax*(tmax-2)*sizeof(blocklightsr[0]));
    memcpy(blocklightsg+smax, blocklightsgNew+smax, smax*(tmax-2)*sizeof(blocklightsg[0]));
    memcpy(blocklightsb+smax, blocklightsbNew+smax, smax*(tmax-2)*sizeof(blocklightsb[0]));
  }
  #endif

  //return is_colored;
}


//==========================================================================
//
//  VRenderLevelLightmap::FlushCaches
//
//==========================================================================
void VRenderLevelLightmap::FlushCaches () {
  lmcache.resetAllBlocks();
  lmcache.reset();
  nukeLightmapsOnNextFrame = false;
  advanceCacheFrame(); // reset all chains
}


//==========================================================================
//
//  VRenderLevelLightmap::NukeLightmapCache
//
//==========================================================================
void VRenderLevelLightmap::NukeLightmapCache () {
  //GCon->Logf(NAME_Warning, "nuking lightmap atlases...");
  // nuke all lightmap caches
  FlushCaches();
}


//==========================================================================
//
//  VRenderLevelLightmap::FreeSurfCache
//
//==========================================================================
void VRenderLevelLightmap::FreeSurfCache (surfcache_t *&block) {
  if (block) {
    lmcache.freeBlock((VLMapCache::Item *)block, true);
    block = nullptr;
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::BuildSurfaceLightmap
//
//  returns `false` if cannot allocate lightmap block
//
//==========================================================================
bool VRenderLevelLightmap::BuildSurfaceLightmap (surface_t *surface) {
  // see if the cache holds appropriate data
  //surfcache_t *cache = surface->CacheSurf;
  VLMapCache::Item *cache = (VLMapCache::Item *)surface->CacheSurf;

  const vuint32 srflight = fixSurfLightLevel(surface);

  if (cache) {
    if (cache->lastframe == lmcache.cacheframecount) {
      GCon->Log(NAME_Warning, "duplicate surface caching");
      return true;
    }
    if (!(surface->drawflags&surface_t::DF_CALC_LMAP)) {
      if (!cache->dlight && surface->dlightframe != currDLightFrame && cache->Light == srflight) {
        chainLightmap(cache);
        //GCon->Logf(NAME_Debug, "unchanged lightmap %p for surface %p", cache, surface);
        return true;
      }
    }
  }

  // determine shape of surface
  int smax = (surface->extents[0]>>4)+1;
  int tmax = (surface->extents[1]>>4)+1;
  vassert(smax > 0);
  vassert(tmax > 0);
  if (smax > GridSize) smax = GridSize;
  if (tmax > GridSize) tmax = GridSize;

  // allocate memory if needed
  // if a texture just animated, don't reallocate it
  if (!cache) {
    cache = lmcache.allocBlock(smax, tmax);
    // in rare case of surface cache overflow, just skip the light
    if (!cache) return false; // alas
    surface->CacheSurf = cache;
    cache->owner = (VLMapCache::Item **)&surface->CacheSurf;
    cache->surf = surface;
    //GCon->Logf(NAME_Debug, "new lightmap %p for surface %p (bnum=%u)", cache, surface, cache->blocknum);
  } else {
    //GCon->Logf(NAME_Debug, "old lightmap %p for surface %p (bnum=%u)", cache, surface, cache->blocknum);
    vassert(surface->CacheSurf == cache);
    vassert(cache->surf == surface);
  }

  cache->dlight = (surface->dlightframe == currDLightFrame);
  cache->Light = srflight;

  // calculate the lightings
  BuildLightMap(surface);

  vassert(cache->t >= 0);
  vassert(cache->s >= 0);

  const vuint32 bnum = cache->atlasid;

  // normal lightmap
  rgba_t *lbp = &light_block[bnum][cache->t*BLOCK_WIDTH+cache->s];
  unsigned blpos = 0;
  for (int y = 0; y < tmax; ++y, lbp += BLOCK_WIDTH) {
    rgba_t *dlbp = lbp;
    for (int x = 0; x < smax; ++x, ++dlbp, ++blpos) {
      dlbp->r = 255-clampToByte(blocklightsr[blpos]>>8);
      dlbp->g = 255-clampToByte(blocklightsg[blpos]>>8);
      dlbp->b = 255-clampToByte(blocklightsb[blpos]>>8);
      dlbp->a = 255;
    }
  }
  chainLightmap(cache);
  block_dirty[bnum].addDirty(cache->s, cache->t, smax, tmax);

  // overbrights
  lbp = &add_block[bnum][cache->t*BLOCK_WIDTH+cache->s];
  blpos = 0;
  for (int y = 0; y < tmax; ++y, lbp += BLOCK_WIDTH) {
    rgba_t *dlbp = lbp;
    for (int x = 0; x < smax; ++x, ++dlbp, ++blpos) {
      dlbp->r = clampToByte(blockaddlightsr[blpos]>>8);
      dlbp->g = clampToByte(blockaddlightsg[blpos]>>8);
      dlbp->b = clampToByte(blockaddlightsb[blpos]>>8);
      dlbp->a = 255;
    }
  }

  if (/*hasOverbright*/true) {
    add_block_dirty[bnum].addDirty(cache->s, cache->t, smax, tmax);
  }

  return true;
}


//==========================================================================
//
//  VRenderLevelLightmap::ProcessCachedSurfaces
//
//  this is called after surface queues built, so lightmap
//  renderer can calculate new lightmaps
//
//==========================================================================
void VRenderLevelLightmap::ProcessCachedSurfaces () {
  if (LMSurfList.length() == 0) return; // nothing to do here
  if (nukeLightmapsOnNextFrame) {
    if (dbg_show_lightmap_cache_messages) GCon->Log(NAME_Debug, "LIGHTMAP: *** previous frame requested lightmaps nuking...");
    FlushCaches();
  }
  // first pass, try to perform normal allocation
  bool success = true;
  for (auto &&sfc : LMSurfList) if (!BuildSurfaceLightmap(sfc)) { success = false; break; }
  if (success) {
    const int lim = r_lmap_atlas_limit.asInt();
    if (lim > 0 && lmcache.getAtlasCount() > lim) nukeLightmapsOnNextFrame = true;
    return;
  }
  // second pass, nuke all lightmap caches, and do it all again
  GCon->Log(NAME_Warning, "LIGHTMAP: *** out of surface cache blocks, retrying...");
  FlushCaches();
  for (auto &&sfc : LMSurfList) {
    if (!BuildSurfaceLightmap(sfc)) {
      // render this surface as non-lightmapped: it is better than completely loosing it
      //DrawSurfList.append(sfc);
      if ((sfc->drawflags&surface_t::DF_MASKED) == 0) {
        SurfCheckAndQueue(GetCurrentDLS().DrawSurfListSolid, sfc);
      } else {
        SurfCheckAndQueue(GetCurrentDLS().DrawSurfListMasked, sfc);
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::CacheSurface
//
//==========================================================================
bool VRenderLevelLightmap::CacheSurface (surface_t *surface) {
  // HACK: return `true` for invalid surfaces, so they won't be queued as normal ones
  if (!SurfPrepareForRender(surface)) return true;

  // remember this surface, it will be processed later
  LMSurfList.append(surface);
  return true;
}
