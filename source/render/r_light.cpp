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
VCvarB r_darken("r_darken", true, "Darken level to better match original DooM?", CVAR_Archive);
VCvarI r_ambient("r_ambient", "0", "Minimal ambient light.", CVAR_Archive);
VCvarB r_allow_ambient("r_allow_ambient", true, "Allow ambient lights?", CVAR_Archive);
VCvarB r_dynamic("r_dynamic", true, "Allow dynamic lights?", CVAR_Archive);
VCvarB r_dynamic_clip("r_dynamic_clip", true, "Clip dynamic lights?", CVAR_Archive);
VCvarB r_dynamic_clip_more("r_dynamic_clip_more", true, "Do some extra checks when clipping dynamic lights?", CVAR_Archive);
VCvarB r_static_lights("r_static_lights", true, "Allow static lights?", CVAR_Archive);

VCvarF r_light_filter_dynamic_coeff("r_light_filter_dynamic_coeff", "0.2", "How close dynamic lights should be to be filtered out?\n(0.6-0.9 is usually ok).", CVAR_Archive);
VCvarB r_allow_subtractive_lights("r_allow_subtractive_lights", true, "Are subtractive lights allowed?", /*CVAR_Archive*/0);

extern VCvarF r_lights_radius;


#define RL_CLEAR_DLIGHT(_dl)  do { \
  (_dl)->radius = 0; \
  (_dl)->flags = 0; \
  if ((_dl)->Owner) { \
    dlowners.del((_dl)->Owner->GetUniqueId()); \
    (_dl)->Owner = nullptr; \
  } \
} while (0)



//==========================================================================
//
//  VRenderLevelShared::AddStaticLight
//
//==========================================================================
void VRenderLevelShared::AddStaticLight (const TVec &origin, float radius, vuint32 colour) {
  staticLightsFiltered = false;
  light_t &L = Lights.Alloc();
  L.origin = origin;
  L.radius = radius;
  L.colour = colour;
  L.leafnum = Level->PointInSubsector(origin)-Level->Subsectors;
  L.active = true;
}


//==========================================================================
//
//  VRenderLevelShared::MarkLights
//
//==========================================================================
void VRenderLevelShared::MarkLights (dlight_t *light, vuint32 bit, int bspnum, int lleafnum) {
  if (bspnum&NF_SUBSECTOR) {
    const int num = (bspnum != -1 ? bspnum&(~NF_SUBSECTOR) : 0);
    subsector_t *ss = &Level->Subsectors[num];

    if (r_dynamic_clip && Level->HasPVS()) {
      const vuint8 *dyn_facevis = Level->LeafPVS(ss);
      //int leafnum = Level->PointInSubsector(light->origin)-Level->Subsectors;
      // check potential visibility
      if (!(dyn_facevis[lleafnum>>3]&(1<<(lleafnum&7)))) return;
    }

    if (ss->dlightframe != r_dlightframecount) {
      ss->dlightbits = 0;
      ss->dlightframe = r_dlightframecount;
    }
    ss->dlightbits |= bit;
  } else {
    node_t *node = &Level->Nodes[bspnum];
    const float dist = DotProduct(light->origin, node->normal)-node->dist;
    if (dist > -light->radius+light->minlight) MarkLights(light, bit, node->children[0], lleafnum);
    if (dist < light->radius-light->minlight) MarkLights(light, bit, node->children[1], lleafnum);
  }
}


//==========================================================================
//
//  VRenderLevelShared::PushDlights
//
//==========================================================================
void VRenderLevelShared::PushDlights () {
  //???:if (GGameInfo->IsPaused() || (Level->LevelInfo->LevelInfoFlags2&VLevelInfo::LIF2_Frozen)) return;
  if ((++r_dlightframecount) == 0x7fffffff) {
    r_dlightframecount = 1;
    for (unsigned idx = 0; idx < (unsigned)Level->NumSubsectors; ++idx) Level->Subsectors[idx].dlightframe = 0;
  }

  if (!r_dynamic) return;

  dlight_t *l = DLights;
  for (int i = 0; i < MAX_DLIGHTS; ++i, ++l) {
    if (l->radius < 1.0f || l->die < Level->Time) {
      dlinfo[i].needTrace = 0;
      dlinfo[i].leafnum = -1;
      continue;
    }
    dlinfo[i].leafnum = (int)(ptrdiff_t)(Level->PointInSubsector(l->origin)-Level->Subsectors);
    dlinfo[i].needTrace = (r_dynamic_clip && r_dynamic_clip_more && Level->NeedProperLightTraceAt(l->origin, l->radius) ? 1 : -1);
    MarkLights(l, 1U<<i, Level->NumNodes-1, dlinfo[i].leafnum);
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

  float bestdist = lengthSquared(lorg-cl->ViewOrg);

  float coeff = r_light_filter_dynamic_coeff;
  if (coeff <= 0.1f) coeff = 0.1f; else if (coeff > 1) coeff = 1;

  float radsq = (radius < 1 ? 64*64 : radius*radius*coeff);
  if (radsq < 32*32) radsq = 32*32;
  const float radsqhalf = radsq*0.25;

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
    // if the light is behind a view, drop it if it is further than light radius
    if ((radius > 0 && bestdist >= radius*radius) || (!radius && bestdist >= 64*64)) {
      static TFrustum frustum;
      static TFrustumParam fp;
      if (fp.needUpdate(cl->ViewOrg, cl->ViewAngles)) {
        fp.setup(cl->ViewOrg, cl->ViewAngles);
        frustum.setup(clip_base, fp, true, r_lights_radius);
      }
      if (!frustum.checkSphere(lorg, (radius > 0 ? radius : 64))) {
        //GCon->Logf("  DROPPED; radius=%f; dist=%f", radius, sqrtf(bestdist));
        return nullptr;
      }
    } else {
      // don't add too far-away lights
      // this checked above
      //!if (bestdist/*lengthSquared(cl->ViewOrg-lorg)*/ >= r_lights_radius*r_lights_radius) return nullptr;
    }
    if (r_dynamic_clip && Level->HasPVS()) {
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
    auto idxp = dlowners.find(Owner->GetUniqueId());
    if (idxp) {
      dlowner = &DLights[*idxp];
      check(dlowner->Owner == Owner);
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
    check(dlowner->Owner == Owner);
    dl = dlowner;
  } else {
    dl = dlreplace;
    if (!dl) { dl = dldying; if (!dl) { dl = dlbestdist; if (!dl) return nullptr; } }
  }

  if (dl->Owner && dl->Owner != Owner) dlowners.del(dl->Owner->GetUniqueId());

  // clean new light, and return it
  memset((void *)dl, 0, sizeof(*dl));
  dl->Owner = Owner;
  dl->origin = lorg;
  dl->radius = radius;
  dl->type = DLTYPE_Point;
  if (isPlr) dl->flags |= dlight_t::PlayerLight;

  if (!dlowner && dl->Owner) dlowners.put(dl->Owner->GetUniqueId(), (vuint32)(ptrdiff_t)(dl-&DLights[0]));

  return dl;
}


//==========================================================================
//
//  VRenderLevelShared::DecayLights
//
//==========================================================================
void VRenderLevelShared::DecayLights (float time) {
  TFrustum frustum;
  int frustumState = 0; // <0: don't check; >1: inited
  if (!cl) frustumState = -1;
  dlight_t *dl = DLights;
  for (int i = 0; i < MAX_DLIGHTS; ++i, ++dl) {
    if (dl->radius <= 0.0f || dl->die < Level->Time) {
      RL_CLEAR_DLIGHT(dl);
      continue;
    }
    dl->radius -= time*dl->decay;
    // remove small lights too
    if (dl->radius < 2.0f) {
      RL_CLEAR_DLIGHT(dl);
    } else {
      // check if light is out of frustum, and remove it if it is invisible
      if (frustumState == 0) {
        TClipBase cb(refdef.fovx, refdef.fovy);
        if (cb.isValid()) {
          frustum.setup(cb, TFrustumParam(cl->ViewOrg, cl->ViewAngles), true, r_lights_radius);
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
//  VRenderLevelShared::RemoveOwnedLight
//
//==========================================================================
void VRenderLevelShared::RemoveOwnedLight (VThinker *Owner) {
  if (!Owner) return;
  auto idxp = dlowners.find(Owner->GetUniqueId());
  if (idxp) {
    dlight_t *dl = &DLights[*idxp];
    check(dl->Owner == Owner);
    dl->radius = 0;
    dl->flags = 0;
    dl->Owner = nullptr;
    dlowners.del(Owner->GetUniqueId());
  }
}


//==========================================================================
//
//  VRenderLevelShared::FreeSurfCache
//
//==========================================================================
void VRenderLevelShared::FreeSurfCache (surfcache_t *) {
}


//==========================================================================
//
//  VRenderLevelShared::CacheSurface
//
//==========================================================================
bool VRenderLevelShared::CacheSurface (surface_t *) {
  return false;
}


#undef RL_CLEAR_DLIGHT
