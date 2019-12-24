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
//**
//**  Refresh of things, i.e. objects represented by sprites.
//**
//**  Sprite rotation 0 is facing the viewer, rotation 1 is one angle turn
//**  CLOCKWISE around the axis. This is not the same as the angle, which
//**  increases counter clockwise (protractor). There was a lot of stuff
//**  grabbed wrong, so I changed it...
//**
//**************************************************************************
#include "gamedefs.h"
#include "r_local.h"
#include "sv_local.h"


extern VCvarB r_chasecam;
extern VCvarB r_draw_mobjs;
extern VCvarB r_model_shadows;
extern VCvarB r_camera_player_shadows;
extern VCvarB r_model_light;
extern VCvarB r_drawfuzz;
extern VCvarF r_transsouls;
extern VCvarI r_max_model_shadows;

static VCvarB r_dbg_thing_dump_vislist("r_dbg_thing_dump_vislist", false, "Dump built list of visible things?", 0);

static VCvarB r_dbg_advthing_dump_actlist("r_dbg_advthing_dump_actlist", false, "Dump built list of active/affected things in advrender?", 0);
static VCvarB r_dbg_advthing_dump_ambient("r_dbg_advthing_dump_ambient", false, "Dump rendered ambient things?", 0);
static VCvarB r_dbg_advthing_dump_textures("r_dbg_advthing_dump_textures", false, "Dump rendered textured things?", 0);

static VCvarB r_dbg_advthing_draw_ambient("r_dbg_advthing_draw_ambient", true, "Draw ambient light for things?", 0);
static VCvarB r_dbg_advthing_draw_texture("r_dbg_advthing_draw_texture", true, "Draw textures for things?", 0);
static VCvarB r_dbg_advthing_draw_light("r_dbg_advthing_draw_light", true, "Draw textures for things?", 0);
static VCvarB r_dbg_advthing_draw_shadow("r_dbg_advthing_draw_shadow", true, "Draw textures for things?", 0);
static VCvarB r_dbg_advthing_draw_fog("r_dbg_advthing_draw_fog", true, "Draw fog for things?", 0);

static VCvarB r_model_advshadow_all("r_model_advshadow_all", false, "Light all alias models, not only those that are in blockmap (slower)?", CVAR_Archive);


//==========================================================================
//
//  CalculateThingAlpha
//
//  returns `false` if object is not need to be rendered
//
//==========================================================================
static inline bool CalculateThingAlpha (const VEntity *ent, int &RendStyle, float &Alpha) {
  int rs = VRenderLevelDrawer::CoerceRenderStyle(ent->RenderStyle);
  float alpha;
  switch (rs) {
    case STYLE_None:
      return false;
    case STYLE_Normal:
      alpha = 1.0f;
      break;
    case STYLE_Translucent:
    case STYLE_Dark:
    case STYLE_Stencil: //???
      alpha = ent->Alpha;
      break;
    case STYLE_Fuzzy:
      alpha = FUZZY_ALPHA;
      break;
    case STYLE_OptFuzzy:
      if (r_drawfuzz) {
        rs = STYLE_Fuzzy;
        alpha = FUZZY_ALPHA;
      } else {
        rs = STYLE_Translucent;
        alpha = ent->Alpha;
      }
      break;
    case STYLE_Add:
    case STYLE_AddStencil:
      alpha = ent->Alpha;
      if (alpha >= 1.0f) alpha = 1.0f-0.002f;
      break;
    case STYLE_SoulTrans:
      rs = STYLE_Translucent;
      alpha = r_transsouls.asFloat();
      if (alpha >= 1.0f) rs = STYLE_Normal;
      break;
    default:
      GCon->Logf(NAME_Error, "Entity `%s` has unknown render style %d; ignored", ent->GetClass()->GetName(), rs);
      rs = STYLE_None;
      return false;
  }
  if (alpha < 0.01f) return false; // no reason to render it, it is invisible
  if (alpha >= 1.0f) alpha = 1.0f;
  RendStyle = rs;
  Alpha = alpha;
  return true;
}


//==========================================================================
//
//  SetupRenderStyle
//
//  returns `false` if object is not need to be rendered
//
//==========================================================================
static inline bool SetupRenderStyleAndTime (const VEntity *ent, int &RendStyle, float &Alpha, bool &Additive, float &TimeFrac) {
  if (!CalculateThingAlpha(ent, RendStyle, Alpha)) return false;
  Additive = VRenderLevelDrawer::IsAdditiveStyle(RendStyle);

  if (ent->State->Time > 0) {
    TimeFrac = 1.0f-(ent->StateTime/ent->State->Time);
    TimeFrac = midval(0.0f, TimeFrac, 1.0f);
  } else {
    TimeFrac = 0;
  }

  return true;
}


//==========================================================================
//
//  VRenderLevelShared::BuildVisibleObjectsList
//
//  this should be called after `RenderWorld()`
//
//  this is not called for "regular" renderer
//
//==========================================================================
void VRenderLevelShared::BuildVisibleObjectsList () {
  visibleObjects.reset();
  visibleAliasModels.reset();
  allShadowModelObjects.reset();

  int RendStyle;
  float Alpha;

  const bool lightAll = r_model_advshadow_all;
  const bool doDump = r_dbg_thing_dump_vislist.asBool();
  bool alphaDone = false;

  if (doDump) GCon->Logf("=== VISIBLE THINGS ===");
  for (TThinkerIterator<VEntity> it(Level); it; ++it) {
    VEntity *ent = *it;

    if (!ent->State || (ent->GetFlags()&(_OF_Destroyed|_OF_DelayedDestroy))) continue;
    if (ent->EntityFlags&(VEntity::EF_NoSector|VEntity::EF_Invisible)) continue;
    if (!ent->SubSector) continue; // just in case

    const bool hasAliasModel = HasEntityAliasModel(ent);

    if (lightAll) {
      // collect all things with models (we'll need them in advrender)
      if (hasAliasModel) {
        alphaDone = true;
        if (!CalculateThingAlpha(ent, RendStyle, Alpha)) continue; // invisible
        // ignore translucent things, they cannot cast a shadow
        if (RendStyle == STYLE_Normal && Alpha >= 1.0f) {
          allShadowModelObjects.append(ent);
          ent->NumRenderedShadows = 0; // for advanced renderer
        }
      } else {
        alphaDone = false;
      }
    }

    // skip things in subsectors that are not visible
    /*
    const unsigned SubIdx = (unsigned)(ptrdiff_t)(ent->SubSector-Level->Subsectors);
    if (!(BspVisThing[SubIdx>>3]&(1<<(SubIdx&7)))) continue;
    */
    if (!IsThingVisible(ent)) continue;

    if (!alphaDone) {
      if (!CalculateThingAlpha(ent, RendStyle, Alpha)) continue; // invisible
    }

    if (doDump) GCon->Logf("  <%s> (%f,%f,%f) 0x%08x", *ent->GetClass()->GetFullName(), ent->Origin.x, ent->Origin.y, ent->Origin.z, ent->EntityFlags);
    // mark as visible, why not?
    // use bsp visibility, to not mark "adjacent" things
    //if (BspVis[SubIdx>>3]&(1<<(SubIdx&7))) ent->FlagsEx |= VEntity::EFEX_Rendered;

    visibleObjects.append(ent);
    if (hasAliasModel) {
      ent->NumRenderedShadows = 0; // for advanced renderer
      visibleAliasModels.append(ent);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::BuildMobjsInCurrLight
//
//==========================================================================
void VRenderLevelShadowVolume::BuildMobjsInCurrLight (bool doShadows) {
  mobjsInCurrLight.reset();
  if (!r_draw_mobjs || !r_models) return;
  // build new list
  // if we won't render thing shadows, don't bother trying invisible things
  if (!doShadows || !r_model_shadows) {
    // we already have a list of visible things built
    useInCurrLightAsLight = true;
    const bool doDump = r_dbg_advthing_dump_actlist;
    if (doDump) GCon->Log("=== counting objects ===");
    for (auto &&ent : visibleAliasModels) {
      if (!IsTouchedByCurrLight(ent)) continue;
      if (doDump) GCon->Logf("  <%s> (%f,%f,%f)", *ent->GetClass()->GetFullName(), ent->Origin.x, ent->Origin.y, ent->Origin.z);
      mobjsInCurrLight.append(ent);
    }
  } else {
    // we need to render shadows, so process all things
    if (r_model_advshadow_all) {
      useInCurrLightAsLight = true;
      for (auto &&ent : allShadowModelObjects) {
        // skip things in subsectors that are not visible by the current light
        const int SubIdx = (int)(ptrdiff_t)(ent->SubSector-Level->Subsectors);
        if (!IsSubsectorLitVis(SubIdx)) continue;
        if (!IsTouchedByCurrLight(ent)) continue;
        //if (r_dbg_advthing_dump_actlist) GCon->Logf("  <%s> (%f,%f,%f)", *ent->GetClass()->GetFullName(), ent->Origin.x, ent->Origin.y, ent->Origin.z);
        mobjsInCurrLight.append(ent);
      }
    } else {
      useInCurrLightAsLight = false;
      const int xl = MapBlock(CurrLightPos.x-CurrLightRadius-Level->BlockMapOrgX-MAXRADIUS);
      const int xh = MapBlock(CurrLightPos.x+CurrLightRadius-Level->BlockMapOrgX+MAXRADIUS);
      const int yl = MapBlock(CurrLightPos.y-CurrLightRadius-Level->BlockMapOrgY-MAXRADIUS);
      const int yh = MapBlock(CurrLightPos.y+CurrLightRadius-Level->BlockMapOrgY+MAXRADIUS);
      int RendStyle;
      float Alpha;
      for (int bx = xl; bx <= xh; ++bx) {
        for (int by = yl; by <= yh; ++by) {
          for (VBlockThingsIterator It(Level, bx, by); It; ++It) {
            VEntity *ent = *It;
            if (!ent->State || (ent->GetFlags()&(_OF_Destroyed|_OF_DelayedDestroy))) continue;
            if (ent->EntityFlags&(VEntity::EF_NoSector|VEntity::EF_Invisible)) continue;
            if (!ent->SubSector) continue; // just in case
            if (ent->GetRenderRadius() < 1) continue;
            // skip things in subsectors that are not visible by the current light
            const int SubIdx = (int)(ptrdiff_t)(ent->SubSector-Level->Subsectors);
            if (!IsSubsectorLitBspVis(SubIdx)) continue;
            if (!IsTouchedByCurrLight(ent)) continue;
            if (!HasEntityAliasModel(ent)) continue;
            if (!CalculateThingAlpha(ent, RendStyle, Alpha)) continue; // invisible
            // ignore translucent things, they cannot cast a shadow
            if (RendStyle == STYLE_Normal && Alpha >= 1.0f) {
              mobjsInCurrLight.append(ent);
            }
          }
        }
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderMobjsShadow
//
//==========================================================================
void VRenderLevelShadowVolume::RenderMobjsShadow (VEntity *owner, vuint32 dlflags) {
  if (!r_draw_mobjs || !r_models || !r_model_shadows) return;
  if (!r_dbg_advthing_draw_shadow) return;
  int RendStyle;
  float Alpha, TimeFrac;
  bool Additive;
  for (auto &&ent : mobjsInCurrLight) {
    if (ent == owner && (dlflags&dlight_t::NoSelfShadow)) continue;
    if (ent->NumRenderedShadows > r_max_model_shadows) continue; // limit maximum shadows for this Entity
    if (!IsShadowAllowedFor(ent)) continue;
    //RenderThingShadow(ent);
    if (SetupRenderStyleAndTime(ent, RendStyle, Alpha, Additive, TimeFrac)) {
      //GCon->Logf("THING SHADOW! (%s)", *ent->GetClass()->GetFullName());
      DrawEntityModel(ent, 0xffffffff, 0, Alpha, Additive, TimeFrac, RPASS_ShadowVolumes);
    }
    ++ent->NumRenderedShadows;
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderMobjsLight
//
//==========================================================================
void VRenderLevelShadowVolume::RenderMobjsLight () {
  if (!r_draw_mobjs || !r_models || !r_model_light) return;
  if (!r_dbg_advthing_draw_light) return;
  int RendStyle;
  float Alpha, TimeFrac;
  bool Additive;
  if (useInCurrLightAsLight) {
    // list is already built
    for (auto &&ent : mobjsInCurrLight) {
      if (ent == ViewEnt && (!r_chasecam || ent != cl->MO)) continue; // don't draw camera actor
      //RenderThingLight(ent);
      if (SetupRenderStyleAndTime(ent, RendStyle, Alpha, Additive, TimeFrac)) {
        DrawEntityModel(ent, 0xffffffff, 0, Alpha, Additive, TimeFrac, RPASS_Light);
      }
    }
  } else {
    for (auto &&ent : visibleAliasModels) {
      if (ent == ViewEnt && (!r_chasecam || ent != cl->MO)) continue; // don't draw camera actor
      // skip things in subsectors that are not visible by the current light
      const int SubIdx = (int)(ptrdiff_t)(ent->SubSector-Level->Subsectors);
      if (!IsSubsectorLitBspVis(SubIdx)) continue;
      if (!IsTouchedByCurrLight(ent)) continue;
      //RenderThingLight(ent);
      if (SetupRenderStyleAndTime(ent, RendStyle, Alpha, Additive, TimeFrac)) {
        DrawEntityModel(ent, 0xffffffff, 0, Alpha, Additive, TimeFrac, RPASS_Light);
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderMobjsAmbient
//
//==========================================================================
void VRenderLevelShadowVolume::RenderMobjsAmbient () {
  if (!r_draw_mobjs || !r_models) return;
  if (!r_dbg_advthing_draw_ambient) return;
  const bool oldLight = (!r_model_light || !r_model_shadows);
  const bool doDump = r_dbg_advthing_dump_ambient.asBool();
  int RendStyle;
  float Alpha, TimeFrac;
  bool Additive;
  if (doDump) GCon->Log("=== ambient ===");
  for (auto &&ent : visibleAliasModels) {
    if (ent == ViewEnt && (!r_chasecam || ent != cl->MO)) continue; // don't draw camera actor
    if (doDump) GCon->Logf("  <%s> (%f,%f,%f)", *ent->GetClass()->GetFullName(), ent->Origin.x, ent->Origin.y, ent->Origin.z);
    //RenderThingAmbient(ent);

    if (SetupRenderStyleAndTime(ent, RendStyle, Alpha, Additive, TimeFrac)) {
      //GCon->Logf("  <%s>", *ent->GetClass()->GetFullName());
      if (Alpha < 1.0f) continue;

      // setup lighting
      vuint32 light;
      if (RendStyle == STYLE_Fuzzy) {
        light = 0;
      } else if ((ent->State->Frame&VState::FF_FULLBRIGHT) ||
                 (ent->EntityFlags&(VEntity::EF_FullBright|VEntity::EF_Bright)))
      {
        light = 0xffffffff;
      } else {
        if (oldLight) {
          // use old way of lighting
          light = LightPoint(ent->Origin, ent->GetRenderRadius(), ent->Height, nullptr, ent->SubSector);
        } else {
          light = LightPointAmbient(ent->Origin, ent->GetRenderRadius(), ent->SubSector);
        }
      }

      DrawEntityModel(ent, light, 0, Alpha, Additive, TimeFrac, RPASS_Ambient);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderMobjsTextures
//
//==========================================================================
void VRenderLevelShadowVolume::RenderMobjsTextures () {
  if (!r_draw_mobjs || !r_models) return;
  if (!r_dbg_advthing_draw_texture) return;
  const bool doDump = r_dbg_advthing_dump_textures.asBool();
  int RendStyle;
  float Alpha, TimeFrac;
  bool Additive;
  if (doDump) GCon->Log("=== textures ===");
  for (auto &&ent : visibleAliasModels) {
    if (ent == ViewEnt && (!r_chasecam || ent != cl->MO)) continue; // don't draw camera actor
    if (doDump) GCon->Logf("  <%s> (%f,%f,%f)", *ent->GetClass()->GetFullName(), ent->Origin.x, ent->Origin.y, ent->Origin.z);
    //RenderThingTextures(ent);
    if (SetupRenderStyleAndTime(ent, RendStyle, Alpha, Additive, TimeFrac)) {
      if (Alpha < 1.0f) continue;
      DrawEntityModel(ent, 0xffffffff, 0, Alpha, Additive, TimeFrac, RPASS_Textures);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderMobjsFog
//
//==========================================================================
void VRenderLevelShadowVolume::RenderMobjsFog () {
  if (!r_draw_mobjs || !r_models) return;
  if (!r_dbg_advthing_draw_fog) return;
  int RendStyle;
  float Alpha, TimeFrac;
  bool Additive;
  for (auto &&ent : visibleAliasModels) {
    if (ent == ViewEnt && (!r_chasecam || ent != cl->MO)) continue; // don't draw camera actor
    //RenderThingFog(ent);
    if (SetupRenderStyleAndTime(ent, RendStyle, Alpha, Additive, TimeFrac)) {
      vuint32 Fade = GetFade(SV_PointRegionLight(ent->Sector, ent->Origin));
      if (Fade || Alpha < 1.0f) {
        DrawEntityModel(ent, 0xffffffff, Fade, Alpha, Additive, TimeFrac, RPASS_Fog);
      }
    }
  }
}
