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
int r_dlightframecount = 0;


// ////////////////////////////////////////////////////////////////////////// //
VCvarB r_darken("r_darken", true, "Darken level to better match original DooM?", CVAR_Archive);
VCvarI r_ambient("r_ambient", "0", "Minimal ambient light.", CVAR_Archive);
VCvarB r_allow_ambient("r_allow_ambient", true, "Allow ambient lights?", CVAR_Archive);
VCvarB r_dynamic("r_dynamic", true, "Allow dynamic lights?", CVAR_Archive);
VCvarB r_dynamic_clip("r_dynamic_clip", true, "Clip dynamic lights?", CVAR_Archive);
VCvarB r_dynamic_clip_more("r_dynamic_clip_more", true, "Do some extra checks when clipping dynamic lights?", CVAR_Archive);
VCvarB r_static_lights("r_static_lights", true, "Allow static lights?", CVAR_Archive);

VCvarF r_light_filter_dynamic_coeff("r_light_filter_dynamic_coeff", "0.8", "How close dynamic lights should be to be filtered out?\n(0.6-0.9 is usually ok).", CVAR_Archive);
VCvarB r_allow_subtractive_lights("r_allow_subtractive_lights", true, "Are subtractive lights allowed?", /*CVAR_Archive*/0);

extern VCvarF r_lights_radius;
extern VCvarF r_lights_radius_sight_check;


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
// VRenderLevelShared::AllocDlight
//
//==========================================================================
dlight_t *VRenderLevelShared::AllocDlight (VThinker *Owner, const TVec &lorg, float radius, int lightid) {
  guard(VRenderLevelShared::AllocDlight);

  dlight_t *dlowner = nullptr;
  dlight_t *dldying = nullptr;
  dlight_t *dlreplace = nullptr;
  dlight_t *dlbestdist = nullptr;
  float bestdist = length2DSquared(lorg-cl->ViewOrg);
  if (radius < 0) radius = 0;

  float coeff = r_light_filter_dynamic_coeff;
  if (coeff <= 0.1f) coeff = 0.1f; else if (coeff > 1) coeff = 1;

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
  }

  // look for any free slot (or free one if necessary)
  dlight_t *dl = DLights;

  // first try to find owned light to replace
  if (Owner) {
    for (int i = 0; i < MAX_DLIGHTS; ++i, ++dl) {
      if (dl->Owner == Owner) { dlowner = dl; break; }
    }
  }

  // look for any free slot (or free one if necessary)
  if (!dlowner) {
    dl = DLights;
    for (int i = 0; i < MAX_DLIGHTS; ++i, ++dl) {
      // replace dlight of the same owner
      // (but keep looping, 'cause we may want to drop this light altogether)
      /*
      if (!dlowner && Owner && dl->Owner == Owner) {
        dlowner = dl;
        if (isPlr) break;
        continue;
      }
      */
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
        if (dd <= 6*6) {
          if (radius > 0 && dl->radius >= radius) return nullptr;
          dlreplace = dl;
          break; // stop searching, we have a perfect candidate
        } else if (dd < radsq) {
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
// VRenderLevelShared::FreeSurfCache
//
//==========================================================================
void VRenderLevelShared::FreeSurfCache (surfcache_t*) {
}


//==========================================================================
//
// VRenderLevelShared::CacheSurface
//
//==========================================================================
bool VRenderLevelShared::CacheSurface (surface_t*) {
  return false;
}
