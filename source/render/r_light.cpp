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
#include "../sv_local.h"


// ////////////////////////////////////////////////////////////////////////// //
VCvarB r_darken("r_darken", true, "Darken level to better match original DooM?", CVAR_Archive);
VCvarI r_ambient_min("r_ambient_min", "0", "Minimal ambient light.", CVAR_Archive);
VCvarB r_allow_ambient("r_allow_ambient", true, "Allow ambient lights?", CVAR_Archive);
VCvarB r_dynamic_lights("r_dynamic_lights", true, "Allow dynamic lights?", CVAR_Archive);
VCvarB r_dynamic_clip("r_dynamic_clip", true, "Clip dynamic lights?", CVAR_Archive);
VCvarB r_dynamic_clip_pvs("r_dynamic_clip_pvs", false, "Clip dynamic lights with PVS?", CVAR_Archive);
VCvarB r_dynamic_clip_more("r_dynamic_clip_more", true, "Do some extra checks when clipping dynamic lights?", CVAR_Archive);
VCvarB r_static_lights("r_static_lights", true, "Allow static lights?", CVAR_Archive);
VCvarB r_light_opt_shadow("r_light_opt_shadow", false, "Check if light can potentially cast a shadow.", CVAR_Archive);

VCvarF r_light_filter_dynamic_coeff("r_light_filter_dynamic_coeff", "0.2", "How close dynamic lights should be to be filtered out?\n(0.2-0.4 is usually ok).", CVAR_Archive);
VCvarB r_allow_dynamic_light_filter("r_allow_dynamic_light_filter", true, "Allow filtering of dynamic lights?", CVAR_Archive);

static VCvarB r_dynamic_light_better_vis_check("r_dynamic_light_better_vis_check", true, "Do better (but slower) dynlight visibility checking on spawn?", CVAR_Archive);

extern VCvarB r_glow_flat;


#define RL_CLEAR_DLIGHT(_dl)  do { \
  (_dl)->radius = 0; \
  (_dl)->flags = 0; \
  /* lights with lightid aren't added to the ownership map, because we may have many of them for one owner */ \
  /* note that they will not be deleted when their owner is going away */ \
  if ((_dl)->ownerUId && !(_dl)->lightid) dlowners.del((_dl)->ownerUId); \
  (_dl)->lightid = 0; \
  (_dl)->ownerUId = 0; \
} while (0)


//==========================================================================
//
//  VRenderLevelShared::RefilterStaticLights
//
//==========================================================================
void VRenderLevelShared::RefilterStaticLights () {
}


//==========================================================================
//
//  VRenderLevelShared::ResetStaticLights
//
//==========================================================================
void VRenderLevelShared::ResetStaticLights () {
  StOwners.reset();
  Lights.reset();
  //GCon->Log(NAME_Debug, "VRenderLevelShared::ResetStaticLights");
}


//==========================================================================
//
//  VRenderLevelShared::AddStaticLightRGB
//
//==========================================================================
void VRenderLevelShared::AddStaticLightRGB (vuint32 OwnerUId, const TVec &origin, float radius, vuint32 color, TVec coneDirection, float coneAngle) {
  staticLightsFiltered = false;
  light_t &L = Lights.Alloc();
  L.origin = origin;
  L.radius = radius;
  L.color = color;
  //L.dynowner = nullptr;
  L.ownerUId = OwnerUId;
  L.leafnum = (int)(ptrdiff_t)(Level->PointInSubsector(origin)-Level->Subsectors);
  L.active = true;
  L.coneDirection = coneDirection;
  L.coneAngle = coneAngle;
  if (OwnerUId) {
    auto osp = StOwners.find(OwnerUId);
    if (osp) Lights[*osp].ownerUId = 0;
    StOwners.put(OwnerUId, Lights.length()-1);
  }
  //GCon->Logf(NAME_Debug, "VRenderLevelShared::AddStaticLightRGB: count=%d", Lights.length());
}


//==========================================================================
//
//  VRenderLevelShared::MoveStaticLightByOwner
//
//==========================================================================
void VRenderLevelShared::MoveStaticLightByOwner (vuint32 OwnerUId, const TVec &origin) {
  if (!OwnerUId) return;
  auto stp = StOwners.get(OwnerUId);
  if (!stp) return;
  light_t &sl = Lights[*stp];
  if (fabs(sl.origin.x-origin.x) <= 4 &&
      fabs(sl.origin.y-origin.y) <= 4 &&
      fabs(sl.origin.z-origin.z) <= 4)
  {
    return;
  }
  //if (sl.origin == origin) return;
  if (sl.active) InvalidateStaticLightmaps(sl.origin, sl.radius, false);
  sl.origin = origin;
  //sl.leafnum = (int)(ptrdiff_t)((Owner->SubSector ? Owner->SubSector : Level->PointInSubsector(sl.origin))-Level->Subsectors);
  sl.leafnum = (int)(ptrdiff_t)(Level->PointInSubsector(sl.origin)-Level->Subsectors);
  if (sl.active) InvalidateStaticLightmaps(sl.origin, sl.radius, true);
}


//==========================================================================
//
//  VRenderLevelShared::InvalidateStaticLightmaps
//
//==========================================================================
void VRenderLevelShared::InvalidateStaticLightmaps (const TVec &org, float radius, bool relight) {
}


//==========================================================================
//
//  VRenderLevelShared::ClearReferences
//
//==========================================================================
void VRenderLevelShared::ClearReferences () {
  // no need to do anything here, the renderer will be notified about thinker add/remove events
  // dynlights
  /*
  dlight_t *l = DLights;
  for (unsigned i = 0; i < MAX_DLIGHTS; ++i, ++l) {
    if (l->die < Level->Time || l->radius < 1.0f) {
      RL_CLEAR_DLIGHT(l);
      continue;
    }
    if (l->Owner && l->Owner->IsRefToCleanup()) {
      RL_CLEAR_DLIGHT(l);
    }
  }
  */
}


//==========================================================================
//
//  VRenderLevelShared::MarkLights
//
//==========================================================================
/*
void VRenderLevelShared::MarkLights (dlight_t *light, vuint32 bit, int bspnum, int lleafnum) {
  if (BSPIDX_IS_LEAF(bspnum)) {
    const int num = (bspnum != -1 ? BSPIDX_LEAF_SUBSECTOR(bspnum) : 0);
    subsector_t *ss = &Level->Subsectors[num];

    if (r_dynamic_clip_pvs && Level->HasPVS()) {
      const vuint8 *dyn_facevis = Level->LeafPVS(ss);
      //int leafnum = Level->PointInSubsector(light->origin)-Level->Subsectors;
      // check potential visibility
      if (!(dyn_facevis[lleafnum>>3]&(1<<(lleafnum&7)))) return;
    }

    if (ss->dlightframe != currDLightFrame) {
      ss->dlightbits = bit;
      ss->dlightframe = currDLightFrame;
    } else {
      ss->dlightbits |= bit;
    }
  } else {
    node_t *node = &Level->Nodes[bspnum];
    const float dist = DotProduct(light->origin, node->normal)-node->dist;
    if (dist > -light->radius+light->minlight) MarkLights(light, bit, node->children[0], lleafnum);
    if (dist < light->radius-light->minlight) MarkLights(light, bit, node->children[1], lleafnum);
  }
}
*/


//==========================================================================
//
//  VRenderLevelShared::PushDlights
//
//==========================================================================
void VRenderLevelShared::PushDlights () {
  //???:if (GGameInfo->IsPaused() || (Level->LevelInfo->LevelInfoFlags2&VLevelInfo::LIF2_Frozen)) return;
  (void)IncDLightFrameCount();

  if (!r_dynamic_lights) return;

  dlight_t *l = DLights;
  for (unsigned i = 0; i < MAX_DLIGHTS; ++i, ++l) {
    if (l->radius < 1.0f || l->die < Level->Time) {
      dlinfo[i].needTrace = 0;
      dlinfo[i].leafnum = -1;
      continue;
    }
    l->origin = l->origOrigin;
    //if (l->Owner && l->Owner->IsA(VEntity::StaticClass())) l->origin += ((VEntity *)l->Owner)->GetDrawDelta();
    if (l->ownerUId) {
      auto ownpp = suid2ent.find(l->ownerUId);
      if (ownpp) l->origin += (*ownpp)->GetDrawDelta();
    }
    if (dlinfo[i].leafnum < 0) dlinfo[i].leafnum = (int)(ptrdiff_t)(Level->PointInSubsector(l->origin)-Level->Subsectors);
    //dlinfo[i].needTrace = (r_dynamic_clip && r_dynamic_clip_more && Level->NeedProperLightTraceAt(l->origin, l->radius) ? 1 : -1);
    //MarkLights(l, 1U<<i, Level->NumNodes-1, dlinfo[i].leafnum);
    //FIXME: this has one frame latency; meh for now
    LitCalcBBox = false; // we don't need any lists
    if (CalcLightVis(l->origin, l->radius, 1U<<i)) {
      dlinfo[i].needTrace = (doShadows ? 1 : -1);
    } else {
      // this one is invisible
      dlinfo[i].needTrace = 0;
      dlinfo[i].leafnum = -1;
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::AllocDlight
//
//==========================================================================
dlight_t *VRenderLevelShared::AllocDlight (VThinker *Owner, const TVec &lorg, float radius, int lightid) {
  dlight_t *dlowner = nullptr;
  dlight_t *dldying = nullptr;
  dlight_t *dlreplace = nullptr;
  dlight_t *dlbestdist = nullptr;

  if (radius <= 0) radius = 0; else if (radius < 2) return nullptr; // ignore too small lights
  if (lightid < 0) lightid = 0;

  float bestdist = lengthSquared(lorg-cl->ViewOrg);

  float coeff = r_light_filter_dynamic_coeff;
  if (coeff <= 0.1f) coeff = 0.1f; else if (coeff > 1) coeff = 1;
  if (!r_allow_dynamic_light_filter) coeff = 0.02f; // filter them anyway

  float radsq = (radius < 1 ? 64*64 : radius*radius*coeff);
  if (radsq < 32*32) radsq = 32*32;
  const float radsqhalf = radsq*0.25f;

  // if this is player's dlight, never drop it
  bool isPlr = false;
  if (Owner && Owner->IsA(VEntity::StaticClass())) {
    isPlr = ((VEntity *)Owner)->IsPlayer();
  }

  int leafnum = -1;

  if (!isPlr) {
    // if the light is behind a view, drop it if it is further than the light radius
    if ((radius > 0 && bestdist >= radius*radius) || (!radius && bestdist >= 64*64)) {
      static TFrustum frustum;
      static TFrustumParam fp;
      if (fp.needUpdate(cl->ViewOrg, cl->ViewAngles)) {
        fp.setup(cl->ViewOrg, cl->ViewAngles);
        frustum.setup(clip_base, fp, true, GetLightMaxDistDef(1024));
      }
      if (!frustum.checkSphere(lorg, (radius > 0 ? radius : 64))) {
        //GCon->Logf("  DROPPED; radius=%f; dist=%f", radius, sqrtf(bestdist));
        return nullptr;
      }
    } else {
      // don't add too far-away lights
      // this checked above
      //!if (bestdist/*lengthSquared(cl->ViewOrg-lorg)*/ >= r_lights_radius*r_lights_radius) return nullptr;
      //const float rsqx = r_lights_radius+radius;
      //if (bestdist >= rsqx*rsqx) return nullptr;
    }

    // pvs check
    if (r_dynamic_clip_pvs && Level->HasPVS()) {
      subsector_t *sub = lastDLightViewSub;
      if (!sub || lastDLightView.x != cl->ViewOrg.x || lastDLightView.y != cl->ViewOrg.y /*|| lastDLightView.z != cl->ViewOrg.z*/) {
        lastDLightView = cl->ViewOrg;
        lastDLightViewSub = sub = Level->PointInSubsector(cl->ViewOrg);
      }
      const vuint8 *dyn_facevis = Level->LeafPVS(sub);
      leafnum = (int)(ptrdiff_t)(Level->PointInSubsector(lorg)-Level->Subsectors);
      // check potential visibility
      if (!(dyn_facevis[leafnum>>3]&(1<<(leafnum&7)))) {
        //fprintf(stderr, "DYNLIGHT rejected by PVS\n");
        return nullptr;
      }
    }

    // floodfill visibility check
    if (/*!IsShadowVolumeRenderer() &&*/ r_dynamic_light_better_vis_check) {
      if (leafnum < 0) leafnum = (int)(ptrdiff_t)(Level->PointInSubsector(lorg)-Level->Subsectors);
      if (!CheckBSPVisibilityBox(lorg, (radius > 0 ? radius : 64), &Level->Subsectors[leafnum])) {
        //GCon->Logf("DYNAMIC DROP: visibility check");
        return nullptr;
      }
    }
  } else {
    // test
    /*
    static TFrustum frustum1;
    frustum1.update(clip_base, cl->ViewOrg, cl->ViewAngles, true, 128);
    if (!frustum1.checkSphere(lorg, 64)) {
      //GCon->Logf("  DROPPED; radius=%f; dist=%f", radius, sqrtf(bestdist));
      return nullptr;
    }
    */
  }

  // look for any free slot (or free one if necessary)
  dlight_t *dl;

  // first try to find owned light to replace
  if (Owner) {
    if (lightid == 0) {
      auto idxp = dlowners.find(Owner->ServerUId);
      if (idxp) {
        dlowner = &DLights[*idxp];
        vassert(dlowner->ownerUId == Owner->ServerUId);
      }
    } else {
      //FIXME: make this faster!
      dl = DLights;
      for (int i = 0; i < MAX_DLIGHTS; ++i, ++dl) {
        if (dl->ownerUId == Owner->ServerUId && dl->lightid == lightid /*&& dl->die >= Level->Time && dl->radius > 0.0f*/) {
          dlowner = dl;
          break;
        }
      }
    }
  }

  // look for any free slot (or free one if necessary)
  if (!dlowner) {
    dl = DLights;
    for (int i = 0; i < MAX_DLIGHTS; ++i, ++dl) {
      // remove dead lights (why not?)
      if (dl->die < Level->Time) dl->radius = 0;
      // unused light?
      if (dl->radius < 2.0f) {
        RL_CLEAR_DLIGHT(dl);
        if (!dldying) dldying = dl;
        continue;
      }
      // don't replace player's lights
      if (dl->flags&dlight_t::PlayerLight) continue;
      // replace furthest light
      float dist = lengthSquared(dl->origin-cl->ViewOrg);
      if (dist > bestdist) {
        bestdist = dist;
        dlbestdist = dl;
      }
      // check if we already have dynamic light around new origin
      if (!isPlr) {
        const float dd = lengthSquared(dl->origin-lorg);
        if (dd <= 6*6) {
          if (radius > 0 && dl->radius >= radius) return nullptr;
          dlreplace = dl;
          break; // stop searching, we have a perfect candidate
        } else if (dd < radsqhalf) {
          // if existing light radius is greater than new radius, drop new light, 'cause
          // we have too much lights around one point (prolly due to several things at one place)
          if (radius > 0 && dl->radius >= radius) return nullptr;
          // otherwise, replace this light
          dlreplace = dl;
          //break; // stop searching, we have a perfect candidate
        }
      }
    }
  }

  if (dlowner) {
    // remove replaced light
    //if (dlreplace && dlreplace != dlowner) memset((void *)dlreplace, 0, sizeof(*dlreplace));
    vassert(dlowner->ownerUId == Owner->ServerUId);
    dl = dlowner;
  } else {
    dl = dlreplace;
    if (!dl) {
      dl = dldying;
      if (!dl) {
        dl = dlbestdist;
        if (!dl) return nullptr;
      }
    }
  }

  // tagged lights are not in the map
  if (dl->ownerUId && !dl->lightid) dlowners.del(dl->ownerUId);

  // clean new light, and return it
  memset((void *)dl, 0, sizeof(*dl));
  dl->ownerUId = (Owner ? Owner->ServerUId : 0);
  dl->origin = lorg;
  dl->radius = radius;
  dl->type = DLTYPE_Point;
  dl->lightid = lightid;
  if (isPlr) dl->flags |= dlight_t::PlayerLight;

  dlinfo[(ptrdiff_t)(dl-DLights)].leafnum = leafnum;

  // tagged lights are not in the map
  if (!lightid && dl->ownerUId) dlowners.put(dl->ownerUId, (vuint32)(ptrdiff_t)(dl-&DLights[0]));

  dl->origOrigin = lorg;
  if (Owner && Owner->IsA(VEntity::StaticClass())) {
    dl->origin += ((VEntity *)Owner)->GetDrawDelta();
  }

  return dl;
}


//==========================================================================
//
//  VRenderLevelShared::FindDlightById
//
//==========================================================================
dlight_t *VRenderLevelShared::FindDlightById (int lightid) {
  if (lightid <= 0) return nullptr;
  dlight_t *dl = DLights;
  for (int i = MAX_DLIGHTS; i--; ++dl) {
    if (dl->die < Level->Time || dl->radius <= 0.0f) continue;
    if (dl->lightid == lightid) return dl;
  }
  return nullptr;
}


//==========================================================================
//
//  VRenderLevelShared::DecayLights
//
//==========================================================================
void VRenderLevelShared::DecayLights (float timeDelta) {
  TFrustum frustum;
  int frustumState = 0; // <0: don't check; >1: inited
  if (!cl) frustumState = -1;
  dlight_t *dl = DLights;
  for (int i = 0; i < MAX_DLIGHTS; ++i, ++dl) {
    if (dl->radius <= 0.0f || dl->die < Level->Time) {
      RL_CLEAR_DLIGHT(dl);
      continue;
    }
    //dl->radius -= timeDelta*(dl->decay/1000.0f);
    dl->radius -= timeDelta*dl->decay;
    // remove small lights too
    if (dl->radius < 2.0f) {
      RL_CLEAR_DLIGHT(dl);
    } else {
      // check if light is out of frustum, and remove it if it is invisible
      if (frustumState == 0) {
        TClipBase cb(refdef.fovx, refdef.fovy);
        if (cb.isValid()) {
          frustum.setup(cb, TFrustumParam(cl->ViewOrg, cl->ViewAngles), true, GetLightMaxDistDef(1024));
          frustumState = (frustum.isValid() ? 1 : -1);
        } else {
          frustumState = -1;
        }
      }
      if (frustumState > 0 && !frustum.checkSphere(dl->origin, dl->radius)) {
        RL_CLEAR_DLIGHT(dl);
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::ThinkerAdded
//
//==========================================================================
void VRenderLevelShared::ThinkerAdded (VThinker *Owner) {
  if (!Owner) return;
  if (!Owner->IsA(VEntity::StaticClass())) return;
  suid2ent.put(Owner->ServerUId, (VEntity *)Owner);
}


//==========================================================================
//
//  VRenderLevelShared::ThinkerDestroyed
//
//==========================================================================
void VRenderLevelShared::ThinkerDestroyed (VThinker *Owner) {
  if (!Owner) return;
  auto idxp = dlowners.find(Owner->ServerUId);
  if (idxp) {
    dlight_t *dl = &DLights[*idxp];
    vassert(dl->ownerUId == Owner->ServerUId);
    dl->radius = 0;
    dl->flags = 0;
    dl->ownerUId = 0;
    dlowners.del(Owner->ServerUId);
  }
  auto stxp = StOwners.find(Owner->ServerUId);
  if (stxp) {
    //Lights[*stxp].dynowner = nullptr;
    Lights[*stxp].ownerUId = 0;
    Lights[*stxp].active = false;
    StOwners.del(Owner->ServerUId);
  }
  suid2ent.remove(Owner->ServerUId);
}


//==========================================================================
//
//  VRenderLevelShared::FreeSurfCache
//
//==========================================================================
void VRenderLevelShared::FreeSurfCache (surfcache_t *&) {
}


//==========================================================================
//
//  VRenderLevelShared::CacheSurface
//
//==========================================================================
bool VRenderLevelShared::CacheSurface (surface_t *) {
  return false;
}


//==========================================================================
//
//  VRenderLevelShared::ProcessCachedSurfaces
//
//==========================================================================
void VRenderLevelShared::ProcessCachedSurfaces () {
}


//==========================================================================
//
//  VRenderLevelShared::CheckLightPointCone
//
//==========================================================================
float VRenderLevelShared::CheckLightPointCone (const TVec &p, const float radius, const float height, const TVec &coneOrigin, const TVec &coneDir, const float coneAngle) {
  TPlane pl;
  pl.SetPointNormal3D(coneOrigin, coneDir);

  if ((p-coneOrigin).lengthSquared() <= 8.0f) return 1.0f;

  //if (checkSpot && dl.coneAngle > 0.0f && dl.coneAngle < 360.0f)
  if (radius == 0.0f) {
    if (height == 0.0f) {
      if (pl.PointOnSide(p)) return 0.0f;
      return p.CalcSpotlightAttMult(coneOrigin, coneDir, coneAngle);
    } else {
      const TVec p1(p.x, p.y, p.z+height);
      if (pl.PointOnSide(p)) {
        if (pl.PointOnSide(p1)) return 0.0f;
        return p1.CalcSpotlightAttMult(coneOrigin, coneDir, coneAngle);
      } else {
        const float att0 = p.CalcSpotlightAttMult(coneOrigin, coneDir, coneAngle);
        if (att0 == 1.0f || pl.PointOnSide(p1)) return att0;
        const float att1 = p1.CalcSpotlightAttMult(coneOrigin, coneDir, coneAngle);
        return max2(att0, att1);
      }
    }
  }

  float bbox[6];
  bbox[0+0] = p.x-radius*0.4f;
  bbox[0+1] = p.y-radius*0.4f;
  bbox[0+2] = p.z;
  bbox[3+0] = p.x+radius*0.4f;
  bbox[3+1] = p.y+radius*0.4f;
  bbox[3+2] = p.z+height;
  if (!pl.checkBox(bbox)) return 0.0f;
  float res = p.CalcSpotlightAttMult(coneOrigin, coneDir, coneAngle);
  if (res == 1.0f) return res;
  CONST_BBoxVertexIndex;
  for (unsigned bi = 0; bi < 8; ++bi) {
    const TVec vv(bbox[BBoxVertexIndex[bi][0]], bbox[BBoxVertexIndex[bi][1]], bbox[BBoxVertexIndex[bi][2]]);
    const float attn = vv.CalcSpotlightAttMult(coneOrigin, coneDir, coneAngle);
    if (attn > res) {
      res = attn;
      if (res == 1.0f) return 1.0f; // it can't be higher than this
    }
  }
  // check box midpoint
  {
    const float attn = TVec((bbox[0+0]+bbox[3+0])/2.0f, (bbox[0+1]+bbox[3+1])/2.0f, (bbox[0+2]+bbox[3+2])/2.0f).CalcSpotlightAttMult(coneOrigin, coneDir, coneAngle);
    res = max2(res, attn);
  }
  return res;
}


//==========================================================================
//
//  VRenderLevelShared::CalculateDynLightSub
//
//  this is common code for light point calculation
//  pass light values from ambient pass
//
//==========================================================================
void VRenderLevelShared::CalculateDynLightSub (float &l, float &lr, float &lg, float &lb, const subsector_t *sub, const TVec &p, float radius, float height, const TPlane *surfplane) {
  if (r_dynamic_lights && sub->dlightframe == currDLightFrame) {
    const vuint8 *dyn_facevis = (Level->HasPVS() ? Level->LeafPVS(sub) : nullptr);
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
      if (dl.type&DLTYPE_Subtractive) continue;
      //if (!dl.radius || dl.die < Level->Time) continue; // this is not needed here
      const float distSq = (p-dl.origin).lengthSquared();
      if (distSq >= dl.radius*dl.radius) continue; // too far away
      float add = (dl.radius-dl.minlight)-sqrtf(distSq);
      if (add > 0.0f) {
        // check potential visibility
        if (r_dynamic_clip) {
          if (surfplane && surfplane->PointOnSide(dl.origin)) continue;
          if (dl.coneAngle > 0.0f && dl.coneAngle < 360.0f) {
            const float attn = CheckLightPointCone(p, radius, height, dl.origin, dl.coneDirection, dl.coneAngle);
            add *= attn;
            if (add <= 1.0f) continue;
          }
          if ((dl.flags&dlight_t::NoShadow) == 0 && !RadiusCastRay(sub->sector, p, dl.origin, radius, false/*r_dynamic_clip_more*/)) continue;
        } else {
          if (dl.coneAngle > 0.0f && dl.coneAngle < 360.0f) {
            const float attn = CheckLightPointCone(p, radius, height, dl.origin, dl.coneDirection, dl.coneAngle);
            add *= attn;
            if (add <= 1.0f) continue;
          }
        }
        if (dl.type&DLTYPE_Subtractive) add = -add;
        l += add;
        lr += add*((dl.color>>16)&255)/255.0f;
        lg += add*((dl.color>>8)&255)/255.0f;
        lb += add*(dl.color&255)/255.0f;
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::CalculateSubStatic
//
//  calculate subsector's light from static light sources
//  (light variables must be initialized)
//
//==========================================================================
void VRenderLevelShared::CalculateSubStatic (float &l, float &lr, float &lg, float &lb, const subsector_t *sub, const TVec &p, float radius, const TPlane *surfplane) {
  if (r_static_lights) {
    if (!staticLightsFiltered) RefilterStaticLights();
    const vuint8 *dyn_facevis = (Level->HasPVS() ? Level->LeafPVS(sub) : nullptr);
    const light_t *stl = Lights.Ptr();
    for (int i = Lights.length(); i--; ++stl) {
      //if (!stl->radius) continue;
      if (!stl->active) continue;
      // check potential visibility
      if (dyn_facevis && !(dyn_facevis[stl->leafnum>>3]&(1<<(stl->leafnum&7)))) continue;
      const float distSq = (p-stl->origin).lengthSquared();
      if (distSq >= stl->radius*stl->radius) continue; // too far away
      const float add = stl->radius-sqrtf(distSq);
      if (add > 0.0f) {
        if (surfplane && surfplane->PointOnSide(stl->origin)) continue;
        if (r_dynamic_clip) {
          if (!RadiusCastRay(sub->sector, p, stl->origin, radius, false/*r_dynamic_clip_more*/)) continue;
        }
        l += add;
        lr += add*((stl->color>>16)&255)/255.0f;
        lg += add*((stl->color>>8)&255)/255.0f;
        lb += add*(stl->color&255)/255.0f;
      }
    }
  }
}


//==========================================================================
//
//  getFPlaneDist
//
//==========================================================================
static inline float getFPlaneDist (const sec_surface_t *floor, const TVec &p) {
  //const float d = floor->PointDist(p);
  if (floor) {
    const float z = floor->esecplane.GetPointZClamped(p);
    if (floor->esecplane.GetNormalZ() > 0.0f) {
      // floor
      if (p.z < z) {
        GCon->Logf(NAME_Debug, "skip floor: p.z=%g; fz=%g", p.z, z);
        return -1.0f;
      }
      GCon->Logf(NAME_Debug, "floor: p.z=%g; fz=%g; d=%g", p.z, z, p.z-z);
      return p.z-z;
    } else {
      // ceiling
      if (p.z >= z) {
        GCon->Logf(NAME_Debug, "skip ceiling: p.z=%g; cz=%g", p.z, z);
        return -1.0f;
      }
      GCon->Logf(NAME_Debug, "floor: p.z=%g; cz=%g; d=%g", p.z, z, z-p.z);
      return z-p.z;
    }
  } else {
    return -1.0f;
  }
}


//==========================================================================
//
//  SV_DebugFindNearestFloor
//
//==========================================================================
sec_surface_t *SV_DebugFindNearestFloor (subsector_t *sub, const TVec &p) {
  sec_surface_t *rfloor = nullptr;
  //reg = sub->regions;
  float bestdist = 999999.0f;
  for (subregion_t *r = sub->regions; r; r = r->next) {
    //const float d = DotProduct(p, reg->floor->secplane->normal)-reg->floor->secplane->dist;
    // floors
    {
      const float d = getFPlaneDist(r->fakefloor, p);
      if (d >= 0.0f && d < bestdist) {
        bestdist = d;
        rfloor = r->fakefloor;
        GCon->Log(NAME_Debug, "  HIT!");
      }
    }
    {
      const float d = getFPlaneDist(r->realfloor, p);
      if (d >= 0.0f && d < bestdist) {
        bestdist = d;
        rfloor = r->realfloor;
        GCon->Log(NAME_Debug, "  HIT!");
      }
    }
    // ceilings
    {
      const float d = getFPlaneDist(r->fakeceil, p);
      if (d >= 0.0f && d < bestdist) {
        bestdist = d;
        rfloor = r->fakeceil;
        GCon->Log(NAME_Debug, "  HIT!");
      }
    }
    {
      const float d = getFPlaneDist(r->realceil, p);
      if (d >= 0.0f && d < bestdist) {
        bestdist = d;
        rfloor = r->realceil;
        GCon->Log(NAME_Debug, "  HIT!");
      }
    }
  }

  if (rfloor /*&& !IsShadowVolumeRenderer()*/) {
    //int s = (int)(DotProduct(p, rfloor->texinfo.saxis)+rfloor->texinfo.soffs);
    //int t = (int)(DotProduct(p, rfloor->texinfo.taxis)+rfloor->texinfo.toffs);
    int s = (int)(DotProduct(p, rfloor->texinfo.saxisLM));
    int t = (int)(DotProduct(p, rfloor->texinfo.taxisLM));
    int ds, dt;
    for (surface_t *surf = rfloor->surfs; surf; surf = surf->next) {
      if (surf->lightmap == nullptr) continue;
      if (surf->count < 3) continue; // wtf?!
      //if (s < surf->texturemins[0] || t < surf->texturemins[1]) continue;

      ds = s-surf->texturemins[0];
      dt = t-surf->texturemins[1];

      if (ds < 0 || dt < 0 || ds > surf->extents[0] || dt > surf->extents[1]) continue;

      GCon->Logf(NAME_Debug, "  lightmap hit! (%d,%d)", ds, dt);
      if (surf->lightmap_rgb) {
        //l += surf->lightmap[(ds>>4)+(dt>>4)*((surf->extents[0]>>4)+1)];
        const rgb_t *rgbtmp = &surf->lightmap_rgb[(ds>>4)+(dt>>4)*((surf->extents[0]>>4)+1)];
        GCon->Logf(NAME_Debug, "    (%d,%d,%d)", rgbtmp->r, rgbtmp->g, rgbtmp->b);
      } else {
        int ltmp = surf->lightmap[(ds>>4)+(dt>>4)*((surf->extents[0]>>4)+1)];
        GCon->Logf(NAME_Debug, "    (%d)", ltmp);
      }
      break;
    }
  }

  return rfloor;
}


//==========================================================================
//
//  VRenderLevelShared::GetNearestFloor
//
//  used to find a lightmap
//  slightly wrong (we should process ceilings too, and use nearest lmap)
//  also note that to process floors/ceilings it is better to use
//  sprite height center
//
//==========================================================================
sec_surface_t *VRenderLevelShared::GetNearestFloor (const subsector_t *sub, const TVec &p) {
  if (!sub) return nullptr;
  sec_surface_t *rfloor = nullptr;
  float bestdist = 999999.0f;
  for (subregion_t *r = sub->regions; r; r = r->next) {
    sec_surface_t *floor;
    // floors
    floor = r->fakefloor;
    if (floor && floor->esecplane.GetNormalZ() > 0.0f) {
      const float z = floor->esecplane.GetPointZClamped(p);
      const float d = p.z-z;
      if (d >= 0.0f && d <= bestdist) {
        bestdist = d;
        rfloor = floor;
      }
    }
    floor = r->realfloor;
    if (floor && floor->esecplane.GetNormalZ() > 0.0f) {
      const float z = floor->esecplane.GetPointZClamped(p);
      const float d = p.z-z;
      if (d >= 0.0f && d <= bestdist) {
        bestdist = d;
        rfloor = floor;
      }
    }
    // ceilings
    floor = r->fakeceil;
    if (floor && floor->esecplane.GetNormalZ() > 0.0f) {
      const float z = floor->esecplane.GetPointZClamped(p);
      const float d = p.z-z;
      if (d >= 0.0f && d <= bestdist) {
        bestdist = d;
        rfloor = floor;
      }
    }
    floor = r->realceil;
    if (floor && floor->esecplane.GetNormalZ() > 0.0f) {
      const float z = floor->esecplane.GetPointZClamped(p);
      const float d = p.z-z;
      if (d >= 0.0f && d <= bestdist) {
        bestdist = d;
        rfloor = floor;
      }
    }
  }
  return rfloor;
}


//==========================================================================
//
//  VRenderLevelShared::CalculateSubAmbient
//
//  calculate subsector's ambient light
//  (light variables must be initialized)
//
//==========================================================================
void VRenderLevelShared::CalculateSubAmbient (float &l, float &lr, float &lg, float &lb, const subsector_t *sub, const TVec &p, float radius, const TPlane *surfplane) {
  bool skipAmbient = false;
  bool glowAllowed = true;

  //FIXME: this is slightly wrong (and slow)
  if (!skipAmbient && sub->regions) {
    sec_region_t *regbase;
    sec_region_t *reglight = SV_PointRegionLightSub((subsector_t *)sub, p, &regbase);

    // allow glow only for bottom regions
    //FIXME: this is not right, we should calculate glow for translucent/transparent floors too!
    glowAllowed = !!(regbase->regflags&sec_region_t::RF_BaseRegion);

    // region's base light
    if (r_allow_ambient) {
      l = reglight->params->lightlevel+ExtraLight;
      l = midval(0.0f, l, 255.0f);
      if (r_darken) l = light_remap[(int)l];
      if (l < r_ambient_min) l = r_ambient_min;
      l = midval(0.0f, l, 255.0f);
    } else {
      l = midval(0.0f, (float)r_ambient_min.asInt(), 255.0f);
    }

    int SecLightColor = reglight->params->LightColor;
    lr = ((SecLightColor>>16)&255)*l/255.0f;
    lg = ((SecLightColor>>8)&255)*l/255.0f;
    lb = (SecLightColor&255)*l/255.0f;

    do {
      if (!IsShadowVolumeRenderer()) {
        // light from floor's lightmap
        sec_surface_t *rfloor = GetNearestFloor(sub, p);
        if (!rfloor) break; // outer `do`
        //int s = (int)(DotProduct(p, rfloor->texinfo.saxis)+rfloor->texinfo.soffs);
        //int t = (int)(DotProduct(p, rfloor->texinfo.taxis)+rfloor->texinfo.toffs);
        int s = (int)(DotProduct(p, rfloor->texinfo.saxisLM));
        int t = (int)(DotProduct(p, rfloor->texinfo.taxisLM));
        int ds, dt;
        for (surface_t *surf = rfloor->surfs; surf; surf = surf->next) {
          if (surf->lightmap == nullptr) continue;
          if (surf->count < 3) continue; // wtf?!
          //if (s < surf->texturemins[0] || t < surf->texturemins[1]) continue;

          ds = s-surf->texturemins[0];
          dt = t-surf->texturemins[1];

          if (ds < 0 || dt < 0 || ds > surf->extents[0] || dt > surf->extents[1]) continue;

          if (surf->lightmap_rgb) {
            l += surf->lightmap[(ds>>4)+(dt>>4)*((surf->extents[0]>>4)+1)];
            const rgb_t *rgbtmp = &surf->lightmap_rgb[(ds>>4)+(dt>>4)*((surf->extents[0]>>4)+1)];
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
    } while (0);
  }

  //TODO: glow height
  #define MIX_FLAT_GLOW  \
    if (hgt >= 0.0f && hgt < 120.0f) { \
      /* if (lr == l && lg == l && lb == l) lr = lg = lb = 255; l = 255; */ \
      /*return gtex->glowing|0xff000000u;*/ \
      /*skipAmbient = true;*/ \
      /*glowL = 255.0f;*/ \
      float glowL = (120.0f-hgt)*255.0f/120.0f; \
      float glowR = (gtex->glowing>>16)&0xff; \
      float glowG = (gtex->glowing>>8)&0xff; \
      float glowB = gtex->glowing&0xff; \
      /* mix with glow */ \
      /*glowL *= 0.8f;*/ \
      if (glowL > 1.0f) { \
        /*l *= 0.8f;*/ \
        const float llfrac = (l/255.0f)*0.8f; \
        const float glfrac = (glowL/255.0f)*0.8f; \
        lr = clampval(lr*llfrac+glowR*glfrac, 0.0f, 255.0f); \
        lg = clampval(lg*llfrac+glowG*glfrac, 0.0f, 255.0f); \
        lb = clampval(lb*llfrac+glowB*glfrac, 0.0f, 255.0f); \
        l = clampval(l+glowL, 0.0f, 255.0f); \
      } \
    }


  // glowing flats
  if (glowAllowed && r_glow_flat && sub->sector) {
    const sector_t *sec = sub->sector;
    // fuckin' pasta!
    if (sec->floor.pic) {
      VTexture *gtex = GTextureManager(sec->floor.pic);
      if (gtex && gtex->Type != TEXTYPE_Null && gtex->glowing) {
        const float hgt = p.z-sub->sector->floor.GetPointZClamped(p);
        MIX_FLAT_GLOW
      }
    }
    if (sec->ceiling.pic) {
      VTexture *gtex = GTextureManager(sec->ceiling.pic);
      if (gtex && gtex->Type != TEXTYPE_Null && gtex->glowing) {
        const float hgt = sub->sector->ceiling.GetPointZClamped(p)-p.z;
        MIX_FLAT_GLOW
      }
    }
  }


  #undef MIX_FLAT_GLOW
}


//==========================================================================
//
//  VRenderLevelShared::LightPoint
//
//==========================================================================
vuint32 VRenderLevelShared::LightPoint (const TVec &p, float radius, float height, const TPlane *surfplane, const subsector_t *psub) {
  if (FixedLight) return FixedLight|(FixedLight<<8)|(FixedLight<<16)|(FixedLight<<24);

  const subsector_t *sub = (psub ? psub : Level->PointInSubsector(p));

  float l = 0.0f, lr = 0.0f, lg = 0.0f, lb = 0.0f;

  // calculate ambient light level
  CalculateSubAmbient(l, lr, lg, lb, sub, p, radius, surfplane);

  // add static lights
  if (IsShadowVolumeRenderer()) CalculateSubStatic(l, lr, lg, lb, sub, p, radius, surfplane);

  // add dynamic lights
  CalculateDynLightSub(l, lr, lg, lb, sub, p, radius, height, surfplane);

  return
    (((vuint32)clampToByte((int)l))<<24)|
    (((vuint32)clampToByte((int)lr))<<16)|
    (((vuint32)clampToByte((int)lg))<<8)|
    ((vuint32)clampToByte((int)lb));
}


//==========================================================================
//
//  VRenderLevelShared::LightPointAmbient
//
//==========================================================================
vuint32 VRenderLevelShared::LightPointAmbient (const TVec &p, float radius, const subsector_t *psub) {
  if (FixedLight) return FixedLight|(FixedLight<<8)|(FixedLight<<16)|(FixedLight<<24);

  const subsector_t *sub = (psub ? psub : Level->PointInSubsector(p));
  float l = 0.0f, lr = 0.0f, lg = 0.0f, lb = 0.0f;
  CalculateSubAmbient(l, lr, lg, lb, sub, p, radius, nullptr);

  return
    (((vuint32)clampToByte((int)l))<<24)|
    (((vuint32)clampToByte((int)lr))<<16)|
    (((vuint32)clampToByte((int)lg))<<8)|
    ((vuint32)clampToByte((int)lb));
}


#undef RL_CLEAR_DLIGHT
