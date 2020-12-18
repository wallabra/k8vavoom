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
//**
//**  Refresh of things, i.e. objects represented by sprites.
//**
//**  Sprite rotation 0 is facing the viewer, rotation 1 is one angle turn
//**  CLOCKWISE around the axis. This is not the same as the angle, which
//**  increases counter clockwise (protractor). There was a lot of stuff
//**  grabbed wrong, so I changed it...
//**
//**************************************************************************
#include "../gamedefs.h"
#include "r_local.h"
#include "../server/sv_local.h"

//#define VV_THING_COLLECTOR_STATS


extern VCvarB r_chasecam;
extern VCvarB r_brightmaps;
extern VCvarB r_brightmaps_sprite;

extern VCvarB r_fake_shadows_alias_models;
extern VCvarB r_model_advshadow_all;

static VCvarB r_dbg_thing_dump_vislist("r_dbg_thing_dump_vislist", false, "Dump built list of visible things?", 0);

static VCvarB r_thing_faster_collect("r_thing_faster_collect", false, "Use faster by-sector thing collector? (This may miss some visible things in invisible sectors)", CVAR_Archive);

VCvarB r_draw_mobjs("r_draw_mobjs", true, "Draw mobjs?", /*CVAR_Archive|*/CVAR_PreInit);
VCvarB r_draw_psprites("r_draw_psprites", true, "Draw psprites?", /*CVAR_Archive|*/CVAR_PreInit);
VCvarI r_drawfuzz("r_drawfuzz", "1", "Fuzz effect (0:alpha; 1:shadow)", CVAR_Archive);
VCvarF r_fuzzalpha("r_fuzzalpha", "0.34", "Alpha for fuzzy sprites.", CVAR_Archive);
VCvarF r_transsouls("r_transsouls", "1", "Lost Souls alpha.", CVAR_Archive);

VCvarB r_models("r_models", true, "Allow 3d models?", CVAR_Archive);
VCvarB r_model_light("r_model_light", true, "Draw model light in advanced renderer?", CVAR_Archive);
VCvarB r_models_view("r_models_view", true, "Allow HUD weapon models?", CVAR_Archive);
VCvarB r_models_strict("r_models_strict", true, "Strict 3D model->class search?", CVAR_Archive);

VCvarB r_models_monsters("r_models_monsters", true, "Render 3D models for monsters?", CVAR_Archive);
VCvarB r_models_corpses("r_models_corpses", true, "Render 3D models for corpses?", CVAR_Archive);
VCvarB r_models_missiles("r_models_missiles", true, "Render 3D models for projectiles?", CVAR_Archive);
VCvarB r_models_pickups("r_models_pickups", true, "Render 3D models for pickups?", CVAR_Archive);
VCvarB r_models_decorations("r_models_decorations", true, "Render 3D models for decorations (any solid things)?", CVAR_Archive);
VCvarB r_models_other("r_models_other", true, "Render 3D models for things with unidentified types?", CVAR_Archive);
VCvarB r_models_players("r_models_players", true, "Render 3D models for players?", CVAR_Archive);

VCvarB r_model_shadows("r_model_shadows", true, "Draw model shadows in advanced renderer?", CVAR_Archive);
VCvarB r_camera_player_shadows("r_camera_player_shadows", false, "Draw camera model shadows in advanced renderer?", CVAR_Archive);
VCvarB r_shadows_monsters("r_shadows_monsters", true, "Render shadows for monsters?", CVAR_Archive);
VCvarB r_shadows_corpses("r_shadows_corpses", true, "Render shadows for corpses?", CVAR_Archive);
VCvarB r_shadows_missiles("r_shadows_missiles", true, "Render shadows for projectiles?", CVAR_Archive);
VCvarB r_shadows_pickups("r_shadows_pickups", true, "Render shadows for pickups?", CVAR_Archive);
VCvarB r_shadows_decorations("r_shadows_decorations", true, "Render shadows for decorations (any solid things)?", CVAR_Archive);
VCvarB r_shadows_other("r_shadows_other", false, "Render shadows for things with unidentified types?", CVAR_Archive);
VCvarB r_shadows_players("r_shadows_players", true, "Render shadows for players?", CVAR_Archive);

static VCvarB r_draw_adjacent_sector_things("r_draw_adjacent_sector_things", true, "Draw things in sectors adjacent to visible sectors (can fix disappearing things, but somewhat slow)?", CVAR_Archive);


//==========================================================================
//
//  VRenderLevelDrawer::CalculateRenderStyleInfo
//
//  returns `false` if there's no need to render the object
//  sets `stencilColor`, `additive`, and `alpha` (only if the result is `true`)
//
//==========================================================================
bool VRenderLevelDrawer::CalculateRenderStyleInfo (RenderStyleInfo &ri, int RenderStyle, float Alpha, vuint32 StencilColor) noexcept {
  const float a = min2(1.0f, Alpha);
  if (a < 0.004f) { // ~1.02
    if (RenderStyle != STYLE_Shadow) return false;
    // a doesn't matter for `STYLE_Shadow`
  }

  switch (RenderStyle) {
    case STYLE_None: // do not draw
      return false;
    case STYLE_Normal: // just copy the image to the screen
      ri.stencilColor = 0u;
      ri.translucency = RenderStyleInfo::Normal;
      ri.alpha = 1.0f;
      return true;
    case STYLE_Fuzzy: // draw silhouette using "fuzz" effect
    case STYLE_OptFuzzy: // draw as fuzzy or translucent, based on user preference
      if (r_drawfuzz) {
        // shadow
        /*
        ri.stencilColor = 0xff000000u;
        ri.translucency = RenderStyleInfo::Translucent;
        ri.alpha = 0.4f; // was 0.3f
        */
        ri.stencilColor = 0u;
        ri.translucency = RenderStyleInfo::Fuzzy;
        ri.alpha = 0.98f; // just in case, to mark it as translucent for sorters
      } else {
        ri.stencilColor = 0u;
        ri.translucency = RenderStyleInfo::Translucent;
        //if (RenderStyle == STYLE_OptFuzzy) ri.alpha = (r_drawfuzz ? FUZZY_ALPHA : a); else ri.alpha = FUZZY_ALPHA;
        ri.alpha = clampval(r_fuzzalpha.asFloat(), 0.02f, 1.0f);
        if (ri.alpha >= 1.0f) ri.translucency = RenderStyleInfo::Normal;
      }
      return true;
    case STYLE_SoulTrans: // draw translucent with amount in r_transsouls
      ri.stencilColor = 0u;
      ri.translucency = RenderStyleInfo::Translucent;
      ri.alpha = clampval(r_transsouls.asFloat(), 0.02f, 1.0f);
      if (ri.alpha >= 1.0f) ri.translucency = RenderStyleInfo::Normal;
      return true;
    case STYLE_Stencil: // solid color
    case STYLE_TranslucentStencil: // seems to be the same as stencil anyway
      ri.stencilColor = (vuint32)StencilColor|0xff000000u;
      ri.translucency = RenderStyleInfo::Translucent;
      ri.alpha = a;
      return true;
    case STYLE_Translucent: // draw translucent
      ri.stencilColor = 0u;
      ri.translucency = RenderStyleInfo::Translucent;
      ri.alpha = a;
      return true;
    case STYLE_Add: // draw additive
      ri.stencilColor = 0u;
      ri.translucency = RenderStyleInfo::Additive;
      ri.alpha = a;
      return true;
    case STYLE_Shaded: // treats 8-bit indexed images as an alpha map. Index 0 = fully transparent, index 255 = fully opaque. This is how decals are drawn. Use StencilColor property to colorize the resulting sprite.
      ri.stencilColor = (vuint32)StencilColor|0xff000000u;
      ri.translucency = RenderStyleInfo::Shaded;
      ri.alpha = a;
      return true;
    case STYLE_Shadow:
      ri.stencilColor = 0xff000000u;
      ri.translucency = RenderStyleInfo::Translucent;
      ri.alpha = 0.4f; // was 0.3f
      return true;
    case STYLE_Subtract:
      ri.stencilColor = 0u;
      ri.translucency = RenderStyleInfo::Subtractive;
      ri.alpha = a;
      return true;
    case STYLE_AddStencil:
      ri.stencilColor = (vuint32)StencilColor|0xff000000u;
      ri.translucency = RenderStyleInfo::Additive;
      ri.alpha = a;
      return true;
    case STYLE_AddShaded: // treats 8-bit indexed images as an alpha map. Index 0 = fully transparent, index 255 = fully opaque. This is how decals are drawn. Use StencilColor property to colorize the resulting sprite.
      ri.stencilColor = (vuint32)StencilColor|0xff000000u;
      ri.translucency = RenderStyleInfo::AddShaded;
      ri.alpha = a;
      return true;
    case STYLE_Dark:
      ri.stencilColor = 0u;
      ri.translucency = RenderStyleInfo::DarkTrans;
      ri.alpha = a;
      return true;
    default:
      break;
  }
  GCon->Logf(NAME_Error, "unknown render style %d", RenderStyle);
  ri.stencilColor = 0u;
  ri.translucency = (a < 1.0f ? RenderStyleInfo::Translucent : RenderStyleInfo::Normal);
  ri.alpha = a;
  return true;
}


//==========================================================================
//
//  VRenderLevelShared::IsThingVisible
//
//  entity must not be `nullptr`, and must have `SubSector` set
//  also, `viewfrustum` should be valid here
//  this is usually called once for each entity, but try to keep it
//  reasonably fast anyway
//
//==========================================================================
bool VRenderLevelShared::IsThingVisible (VEntity *ent) const noexcept {
  const unsigned SubIdx = (unsigned)(ptrdiff_t)(ent->SubSector-Level->Subsectors);
  if (BspVis[SubIdx>>3]&(1u<<(SubIdx&7))) return true;
  // check if the sector is visible
  const unsigned SecIdx = (unsigned)(ptrdiff_t)(ent->Sector-Level->Sectors);
  // check if it is in visible sector
  if (BspVisSector[SecIdx>>3]&(1u<<(SecIdx&7))) {
    // in visible sector; check frustum, because sector shapes can be quite bizarre
    return Drawer->viewfrustum.checkSphere(ent->Origin, ent->GetRenderRadius());
  } else if (r_draw_adjacent_sector_things) {
    // in invisible sector, and we have to perform adjacency check
    // do frustum check first
    if (Drawer->viewfrustum.checkSphere(ent->Origin, ent->GetRenderRadius())) {
      // check if this thing is touching any visible sector
      for (msecnode_t *mnode = ent->TouchingSectorList; mnode; mnode = mnode->TNext) {
        const unsigned snum = (unsigned)(ptrdiff_t)(mnode->Sector-Level->Sectors);
        if (BspVisSector[snum>>3]&(1u<<(snum&7))) return true;
      }
    }
  }
  return false;
}


//==========================================================================
//
//  VRenderLevelShared::RenderAliasModel
//
//==========================================================================
bool VRenderLevelShared::RenderAliasModel (VEntity *mobj, const RenderStyleInfo &ri, ERenderPass Pass) {
  if (!r_models) return false;
  if (!IsAliasModelAllowedFor(mobj)) return false;

  float TimeFrac = 0;
  if (mobj->State->Time > 0) {
    TimeFrac = 1.0f-(mobj->StateTime/mobj->State->Time);
    TimeFrac = midval(0.0f, TimeFrac, 1.0f);
  }

  // draw it
  if (ri.isTranslucent()) {
    if (!CheckAliasModelFrame(mobj, TimeFrac)) return false;
    QueueTranslucentAliasModel(mobj, ri, TimeFrac);
    return true;
  } else {
    return DrawEntityModel(mobj, ri, TimeFrac, Pass);
  }
}


//==========================================================================
//
//  VRenderLevelShared::SetupRIThingLighting
//
//==========================================================================
void VRenderLevelShared::SetupRIThingLighting (VEntity *ent, RenderStyleInfo &ri, bool asAmbient, bool allowBM) {
  if (ent->RenderStyle == STYLE_Fuzzy) {
    ri.light = ri.seclight = 0;
  } else if ((ent->State->Frame&VState::FF_FULLBRIGHT) ||
             (ent->EntityFlags&(VEntity::EF_FullBright|VEntity::EF_Bright)))
  {
    ri.light = ri.seclight = 0xffffffff;
    if (allowBM && r_brightmaps && r_brightmaps_sprite) ri.seclight = LightPoint(ent, ent->Origin, ent->GetRenderRadius(), ent->Height, ent->SubSector);
  } else {
    if (!asAmbient) {
      // use old way of lighting (i.e. calculate rough lighting from all light sources)
      ri.light = ri.seclight = LightPoint(ent, ent->Origin, ent->GetRenderRadius(), ent->Height, ent->SubSector);
    } else {
      // use only ambient lighting, lighting from light sources will be added later
      // this is used in advrender
      ri.light = ri.seclight = LightPointAmbient(ent, ent->Origin, ent->GetRenderRadius(), ent->Height, ent->SubSector);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderThing
//
//==========================================================================
void VRenderLevelShared::RenderThing (VEntity *mobj, ERenderPass Pass) {
  if (mobj == ViewEnt && (!r_chasecam || ViewEnt != cl->MO)) return; // don't draw camera actor

  /*
  if (Pass == RPASS_Normal) {
    // this is called only in regular renderer, and only once
    // so we can skip building visible things list, and perform a direct check here
    if (!mobj->IsRenderable()) return;
  }
  */

  RenderStyleInfo ri;
  if (!CalculateRenderStyleInfo(ri, mobj->RenderStyle, mobj->Alpha, mobj->StencilColor)) return;
  //if (VStr::strEquCI(mobj->GetClass()->GetName(), "FlashSG")) GCon->Logf(NAME_Debug, "%s: %s", mobj->GetClass()->GetName(), ri.toCString());

  /*
  if (Pass == RPASS_Normal) {
    // this is called only in regular renderer, and only once
    // so we can skip building visible things list, and perform a direct check here
    // skip things in subsectors that are not visible
    if (!IsThingVisible(mobj)) return;
  }
  */

  SetupRIThingLighting(mobj, ri, false/*asAmbient*/, true/*allowBM*/);
  ri.fade = GetFade(SV_PointRegionLight(mobj->Sector, mobj->Origin));

  // try to draw a model
  // if it's a script and it doesn't specify model for this frame, draw sprite instead
  if (!RenderAliasModel(mobj, ri, Pass)) {
    QueueSprite(mobj, ri);
  } else if (r_fake_shadows_alias_models) {
    QueueSprite(mobj, ri, true); // only shadow
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderMobjs
//
//==========================================================================
void VRenderLevelShared::RenderMobjs (ERenderPass Pass) {
  if (!r_draw_mobjs) return;
  // "normal" pass has no object list
  /*
  if (Pass == RPASS_Normal) {
    // render things
    for (TThinkerIterator<VEntity> Ent(Level); Ent; ++Ent) {
      RenderThing(*Ent, RPASS_Normal);
    }
  } else
  */
  {
    // we have a list here
    for (auto &&ent : visibleObjects) RenderThing(ent, Pass);
  }
}


//==========================================================================
//
//  VRenderLevelShared::BuildVisibleObjectsList
//
//  this should be called after `RenderCollectSurfaces()`
//
//==========================================================================
void VRenderLevelShared::BuildVisibleObjectsList (bool doShadows) {
  visibleObjects.reset();
  visibleAliasModels.reset();
  visibleSprites.reset();
  allShadowModelObjects.reset();

  if (!r_draw_mobjs) return;

  const bool lightAll = (doShadows && r_model_advshadow_all);
  const bool doDump = r_dbg_thing_dump_vislist.asBool();
  bool alphaDone = false;

  RenderStyleInfo ri;

  if (doDump) GCon->Logf("=== VISIBLE THINGS ===");
  MiniStopTimer profCollect("BuildVisibleObjectsList", prof_r_bsp_mobj_collect.asBool());

  if (r_thing_faster_collect) {
    if (renderedSectors.length() == 0) return;

    /*
    const VObject::GCStats &stats = VObject::GetGCStats();
    int rtmlen = renderedThingMarks.length();
    if (rtmlen < stats.alive) {
      renderedThingMarks.setLength(stats.alive+8192);
      for (int f = rtmlen; f < renderedThingMarks.length(); ++f) renderedThingMarks[f] = 0;
      rtmlen = renderedThingMarks.length();
    }
    */

    #ifdef VV_THING_COLLECTOR_STATS
    int checkedCount = 0;
    #endif
    renderedThingMarks.reset();
    const int visCount = Level->nextVisitedCount();
    for (auto &&secnum : renderedSectors) {
      for (msecnode_t *n = Level->Sectors[secnum].TouchingThingList; n; n = n->SNext) {
        if (n->Visited == visCount) continue;
        n->Visited = visCount;
        VEntity *ent = n->Thing;
        if (!ent || !ent->IsRenderable()) continue;
        if (renderedThingMarks.put(ent, true)) continue; // already collected

        #ifdef VV_THING_COLLECTOR_STATS
        ++checkedCount;
        #endif

        const bool hasAliasModel = HasEntityAliasModel(ent);

        if (lightAll) {
          // collect all things with models (we'll need them in advrender)
          if (hasAliasModel) {
            alphaDone = true;
            if (!CalculateRenderStyleInfo(ri, ent->RenderStyle, ent->Alpha, ent->StencilColor)) continue; // invisible
            // ignore translucent things, they cannot cast a shadow
            if (!ri.isTranslucent()) {
              allShadowModelObjects.append(ent);
              ent->NumRenderedShadows = 0; // for advanced renderer
            }
          } else {
            alphaDone = false;
          }
        }

        // skip things in subsectors that are not visible
        if (!IsThingVisible(ent)) continue;

        if (!alphaDone) {
          if (!CalculateRenderStyleInfo(ri, ent->RenderStyle, ent->Alpha, ent->StencilColor)) continue; // invisible
        }

        if (doDump) GCon->Logf("  <%s> (%f,%f,%f) 0x%08x", *ent->GetClass()->GetFullName(), ent->Origin.x, ent->Origin.y, ent->Origin.z, ent->EntityFlags);
        // mark as visible, why not?
        // use bsp visibility, to not mark "adjacent" things
        //if (BspVis[SubIdx>>3]&(1<<(SubIdx&7))) ent->FlagsEx |= VEntity::EFEX_Rendered;

        visibleObjects.append(ent);
        if (hasAliasModel) {
          ent->NumRenderedShadows = 0; // for advanced renderer
          visibleAliasModels.append(ent);
        } else {
          visibleSprites.append(ent);
        }
      }
    }

    #ifdef VV_THING_COLLECTOR_STATS
    int totalCount = 0;
    for (TThinkerIterator<VEntity> it(Level); it; ++it) {
      VEntity *ent = *it;
      if (!ent->IsRenderable()) continue;
      ++totalCount;
    }
    GCon->Logf(NAME_Debug, "checked %d things out of %d", checkedCount, totalCount);
    #endif
  } else {
    for (TThinkerIterator<VEntity> it(Level); it; ++it) {
      VEntity *ent = *it;

      if (!ent->IsRenderable()) continue;

      const bool hasAliasModel = HasEntityAliasModel(ent);

      if (lightAll) {
        // collect all things with models (we'll need them in advrender)
        if (hasAliasModel) {
          alphaDone = true;
          if (!CalculateRenderStyleInfo(ri, ent->RenderStyle, ent->Alpha, ent->StencilColor)) continue; // invisible
          // ignore translucent things, they cannot cast a shadow
          if (!ri.isTranslucent()) {
            allShadowModelObjects.append(ent);
            ent->NumRenderedShadows = 0; // for advanced renderer
          }
        } else {
          alphaDone = false;
        }
      }

      // skip things in subsectors that are not visible
      if (!IsThingVisible(ent)) continue;

      if (!alphaDone) {
        if (!CalculateRenderStyleInfo(ri, ent->RenderStyle, ent->Alpha, ent->StencilColor)) continue; // invisible
      }

      if (doDump) GCon->Logf("  <%s> (%f,%f,%f) 0x%08x", *ent->GetClass()->GetFullName(), ent->Origin.x, ent->Origin.y, ent->Origin.z, ent->EntityFlags);
      // mark as visible, why not?
      // use bsp visibility, to not mark "adjacent" things
      //if (BspVis[SubIdx>>3]&(1<<(SubIdx&7))) ent->FlagsEx |= VEntity::EFEX_Rendered;

      visibleObjects.append(ent);
      if (hasAliasModel) {
        ent->NumRenderedShadows = 0; // for advanced renderer
        visibleAliasModels.append(ent);
      } else {
        visibleSprites.append(ent);
      }
    }
  }

  profCollect.stopAndReport();
}
