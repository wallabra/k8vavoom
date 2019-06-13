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
extern VCvarB r_draw_psprites;
extern VCvarB r_models;
extern VCvarB r_view_models;
extern VCvarB r_model_shadows;
extern VCvarB r_model_light;
extern VCvarB r_sort_sprites;
extern VCvarB r_fix_sprite_offsets;
extern VCvarI r_sprite_fix_delta;
extern VCvarB r_drawfuzz;
extern VCvarF r_transsouls;
extern VCvarI crosshair;
extern VCvarF crosshair_alpha;
//extern VCvarI r_max_model_lights;
extern VCvarI r_max_model_shadows;

static VCvarB r_dbg_advthing_dump_actlist("r_dbg_advthing_dump_actlist", false, "Dump built list of active/affected things in advrender?", 0);
static VCvarB r_dbg_advthing_dump_ambient("r_dbg_advthing_dump_ambient", false, "Dump rendered ambient things?", 0);
static VCvarB r_dbg_advthing_dump_textures("r_dbg_advthing_dump_textures", false, "Dump rendered textured things?", 0);

static VCvarB r_dbg_advthing_draw_ambient("r_dbg_advthing_draw_ambient", true, "Draw ambient light for things?", 0);
static VCvarB r_dbg_advthing_draw_texture("r_dbg_advthing_draw_texture", true, "Draw textures for things?", 0);
static VCvarB r_dbg_advthing_draw_light("r_dbg_advthing_draw_light", true, "Draw textures for things?", 0);
static VCvarB r_dbg_advthing_draw_shadow("r_dbg_advthing_draw_shadow", true, "Draw textures for things?", 0);
static VCvarB r_dbg_advthing_draw_fog("r_dbg_advthing_draw_fog", true, "Draw fog for things?", 0);


//==========================================================================
//
//  SetupRenderStyle
//
//  returns `false` if object is not need to be rendered
//
//==========================================================================
static inline bool SetupRenderStyleAndTime (const VEntity *mobj, int &RendStyle, float &Alpha, bool &Additive, float &TimeFrac) {
  RendStyle = mobj->RenderStyle;
  Alpha = mobj->Alpha;
  Additive = false;
  TimeFrac = 0;

  switch (RendStyle) {
    case STYLE_None:
      return false;
    case STYLE_Normal:
      Alpha = 1.0f;
      break;
    case STYLE_Translucent:
    case STYLE_Dark:
      //Alpha = mobj->Alpha;
      break;
    case STYLE_Fuzzy:
      Alpha = FUZZY_ALPHA;
      break;
    case STYLE_OptFuzzy:
      if (r_drawfuzz) {
        RendStyle = STYLE_Fuzzy;
        Alpha = FUZZY_ALPHA;
      } else {
        RendStyle = STYLE_Translucent;
        Alpha = mobj->Alpha;
      }
      break;
    case STYLE_Add:
      if (Alpha == 1.0f) Alpha -= 0.002f;
      Additive = true;
      break;
    case STYLE_SoulTrans:
      RendStyle = STYLE_Translucent;
      Alpha = r_transsouls;
      break;
    default: abort();
  }

  Alpha = midval(0.0f, Alpha, 1.0f);
  if (Alpha < 0.01f) return false;

  if (mobj->State->Time > 0) {
    TimeFrac = 1.0f-(mobj->StateTime/mobj->State->Time);
    TimeFrac = midval(0.0f, TimeFrac, 1.0f);
  }

  return true;
}


//==========================================================================
//
//  VAdvancedRenderLevel::IsTouchedByLight
//
//==========================================================================
bool VAdvancedRenderLevel::IsTouchedByLight (VEntity *ent) {
  const TVec Delta = CurrLightPos-ent->Origin;
  const float Dist = ent->Radius+CurrLightRadius;
  if (Delta.LengthSquared() >= Dist*Dist) return false;
  // if light is higher than thing height, assume that the thing is not touched
  if (Delta.z >= CurrLightRadius+ent->Height) return false;
  return true;
}



//==========================================================================
//
//  VAdvancedRenderLevel::ResetMobjsLightCount
//
//==========================================================================
void VAdvancedRenderLevel::ResetMobjsLightCount (bool first, bool doShadows) {
  if (!r_draw_mobjs || !r_models) {
    mobjAffected.reset();
    return;
  }
  if (first) {
    // first time, build new list
    mobjAffected.reset();
    // if we won't render thing shadows, don't bother trying invisible things
    if (!doShadows || !r_model_shadows) {
      // we already have a list of visible things built
      if (r_dbg_advthing_dump_actlist) GCon->Log("=== counting objects ===");
      VEntity **ent = visibleObjects.ptr();
      for (int count = visibleObjects.length(); count--; ++ent) {
        //if ((*ent)->EntityFlags&(VEntity::EF_NoSector|VEntity::EF_Invisible)) continue;
        //if (!(*ent)->State) continue;
        if (!HasAliasModel((*ent)->GetClass()->Name)) continue;
        if (!IsTouchedByLight(*ent)) continue;
        if (r_dbg_advthing_dump_actlist) GCon->Logf("  <%s> (%f,%f,%f)", *(*ent)->GetClass()->GetFullName(), (*ent)->Origin.x, (*ent)->Origin.y, (*ent)->Origin.z);
        mobjAffected.append(*ent);
      }
    } else {
      // we need to render shadows, so process all things
      //TODO: optimise this by build an unified visibility for all lights
      /*
      for (TThinkerIterator<VEntity> ent(Level); ent; ++ent) {
        //if (ent->EntityFlags&(VEntity::EF_NoSector|VEntity::EF_Invisible)) continue;
        //if (!ent->State) continue;
        if (!HasAliasModel((*ent)->GetClass()->Name)) continue;
        ent->NumRenderedShadows = 0;
        if (!IsTouchedByLight(*ent)) continue;
        mobjAffected.append(*ent);
      }
      */
      VEntity **ent = allModelObjects.ptr();
      for (int count = allModelObjects.length(); count--; ++ent) {
        //if ((*ent)->EntityFlags&(VEntity::EF_NoSector|VEntity::EF_Invisible)) continue;
        //if (!(*ent)->State) continue;
        //if (!HasAliasModel((*ent)->GetClass()->Name)) continue;
        if (!IsTouchedByLight(*ent)) continue;
        //if (r_dbg_advthing_dump_actlist) GCon->Logf("  <%s> (%f,%f,%f)", *(*ent)->GetClass()->GetFullName(), (*ent)->Origin.x, (*ent)->Origin.y, (*ent)->Origin.z);
        mobjAffected.append(*ent);
      }
    }
  } else {
    // no need to do anything here, as the list will be reset for each new light
    /*
    int count = mobjAffected.length();
    if (!count) return;
    VEntity **entp = mobjAffected.ptr();
    for (; count--; ++entp) {
      VEntity *ent = *entp;
      if (ent->NumRenderedShadows == 0) continue; // no need to do anything
      if (ent->EntityFlags&(VEntity::EF_NoSector|VEntity::EF_Invisible)) continue;
      if (!ent->State) continue;
      if (!IsTouchedByLight(ent)) continue;
      ent->NumRenderedShadows = 0;
    }
    */
  }
}



//==========================================================================
//
//  VAdvancedRenderLevel::RenderThingAmbient
//
//==========================================================================
void VAdvancedRenderLevel::RenderThingAmbient (VEntity *mobj) {
  int RendStyle;
  float Alpha, TimeFrac;
  bool Additive;
  if (!SetupRenderStyleAndTime(mobj, RendStyle, Alpha, Additive, TimeFrac)) return;

  //GCon->Logf("  <%s>", *mobj->GetClass()->GetFullName());

  // setup lighting
  vuint32 light;
  if (RendStyle == STYLE_Fuzzy) {
    light = 0;
  } else if ((mobj->State->Frame&VState::FF_FULLBRIGHT) ||
             (mobj->EntityFlags&(VEntity::EF_FullBright|VEntity::EF_Bright)))
  {
    light = 0xffffffff;
  } else {
    if (!r_model_light || !r_model_shadows) {
      // use old way of lighting
      light = LightPoint(mobj->Origin, mobj->Radius, mobj->Height, nullptr, mobj->SubSector);
    } else {
      light = LightPointAmbient(mobj->Origin, mobj->Radius, mobj->SubSector);
    }
  }

  DrawEntityModel(mobj, light, 0, Alpha, Additive, TimeFrac, RPASS_Ambient);
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderMobjsAmbient
//
//==========================================================================
void VAdvancedRenderLevel::RenderMobjsAmbient () {
  if (!r_draw_mobjs || !r_models) return;
  if (!r_dbg_advthing_draw_ambient) return;
  VEntity **ent = visibleObjects.ptr();
  if (r_dbg_advthing_dump_ambient) GCon->Log("=== ambient ===");
  for (int count = visibleObjects.length(); count--; ++ent) {
    if (*ent == ViewEnt && (!r_chasecam || ViewEnt != cl->MO)) continue; // don't draw camera actor
    if (!HasAliasModel((*ent)->GetClass()->Name)) continue;
    if (r_dbg_advthing_dump_ambient) GCon->Logf("  <%s> (%f,%f,%f)", *(*ent)->GetClass()->GetFullName(), (*ent)->Origin.x, (*ent)->Origin.y, (*ent)->Origin.z);
    RenderThingAmbient(*ent);
  }
}



//==========================================================================
//
//  VAdvancedRenderLevel::RenderThingTextures
//
//==========================================================================
void VAdvancedRenderLevel::RenderThingTextures (VEntity *mobj) {
  int RendStyle;
  float Alpha, TimeFrac;
  bool Additive;
  if (!SetupRenderStyleAndTime(mobj, RendStyle, Alpha, Additive, TimeFrac)) return;

  DrawEntityModel(mobj, 0xffffffff, 0, Alpha, Additive, TimeFrac, RPASS_Textures);
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderMobjsTextures
//
//==========================================================================
void VAdvancedRenderLevel::RenderMobjsTextures () {
  if (!r_draw_mobjs || !r_models) return;
  if (!r_dbg_advthing_draw_texture) return;
  VEntity **ent = visibleObjects.ptr();
  if (r_dbg_advthing_dump_textures) GCon->Log("=== textures ===");
  for (int count = visibleObjects.length(); count--; ++ent) {
    if (*ent == ViewEnt && (!r_chasecam || ViewEnt != cl->MO)) continue; // don't draw camera actor
    if (!HasAliasModel((*ent)->GetClass()->Name)) continue;
    if (r_dbg_advthing_dump_textures) GCon->Logf("  <%s> (%f,%f,%f)", *(*ent)->GetClass()->GetFullName(), (*ent)->Origin.x, (*ent)->Origin.y, (*ent)->Origin.z);
    RenderThingTextures(*ent);
  }
}



//==========================================================================
//
//  VAdvancedRenderLevel::RenderThingLight
//
//==========================================================================
void VAdvancedRenderLevel::RenderThingLight (VEntity *mobj) {
  // use advanced lighting style
  int RendStyle;
  float Alpha, TimeFrac;
  bool Additive;
  if (!SetupRenderStyleAndTime(mobj, RendStyle, Alpha, Additive, TimeFrac)) return;

  DrawEntityModel(mobj, 0xffffffff, 0, Alpha, Additive, TimeFrac, RPASS_Light);
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderMobjsLight
//
//  can use `mobjAffected`
//
//==========================================================================
void VAdvancedRenderLevel::RenderMobjsLight () {
  if (!r_draw_mobjs || !r_models || !r_model_light) return;
  if (!r_dbg_advthing_draw_light) return;
  int count = mobjAffected.length();
  if (!count) return;
  VEntity **entp = mobjAffected.ptr();
  for (; count--; ++entp) {
    VEntity *ent = *entp;
    if (ent == ViewEnt && (!r_chasecam || ViewEnt != cl->MO)) continue; // don't draw camera actor
    //if (ent->EntityFlags&(VEntity::EF_NoSector|VEntity::EF_Invisible)) continue;
    //if (!ent->State) continue;
    // skip things in subsectors that are not visible
    const int SubIdx = (int)(ptrdiff_t)(ent->SubSector-Level->Subsectors);
    if (!(LightBspVis[SubIdx>>3]&(1<<(SubIdx&7)))) continue;
    //if (!IsTouchedByLight(ent)) continue;
    RenderThingLight(ent);
  }
}



//==========================================================================
//
//  VAdvancedRenderLevel::RenderThingShadow
//
//==========================================================================
void VAdvancedRenderLevel::RenderThingShadow (VEntity *mobj) {
  int RendStyle;
  float Alpha, TimeFrac;
  bool Additive;
  if (!SetupRenderStyleAndTime(mobj, RendStyle, Alpha, Additive, TimeFrac)) return;

  //GCon->Logf("THING SHADOW! (%s)", *mobj->GetClass()->GetFullName());
  DrawEntityModel(mobj, 0xffffffff, 0, Alpha, Additive, TimeFrac, RPASS_ShadowVolumes);
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderMobjsShadow
//
//  can use `mobjAffected`
//
//==========================================================================
void VAdvancedRenderLevel::RenderMobjsShadow (VEntity *owner, vuint32 dlflags) {
  if (!r_draw_mobjs || !r_models || !r_model_shadows) return;
  if (!r_dbg_advthing_draw_shadow) return;
  int count = mobjAffected.length();
  //GCon->Logf("THING SHADOWS: %d", count);
  if (!count) return;
  VEntity **entp = mobjAffected.ptr();
  for (; count--; ++entp) {
    VEntity *ent = *entp;
    if (ent == owner && (dlflags&dlight_t::NoSelfShadow)) continue;
    if (ent->NumRenderedShadows > r_max_model_shadows) continue; // limit maximum shadows for this Entity
    //if (ent->EntityFlags&(VEntity::EF_NoSector|VEntity::EF_Invisible)) continue;
    //if (!ent->State) continue;
    // skip things in subsectors that are not visible
    const int SubIdx = (int)(ptrdiff_t)(ent->SubSector-Level->Subsectors);
    if (!(LightVis[SubIdx>>3]&(1<<(SubIdx&7)))) continue;
    //if (!IsTouchedByLight(ent)) continue;
    RenderThingShadow(ent);
    ++ent->NumRenderedShadows;
  }
}



//==========================================================================
//
//  VAdvancedRenderLevel::RenderThingFog
//
//==========================================================================
void VAdvancedRenderLevel::RenderThingFog (VEntity *mobj) {
  int RendStyle;
  float Alpha, TimeFrac;
  bool Additive;
  if (!SetupRenderStyleAndTime(mobj, RendStyle, Alpha, Additive, TimeFrac)) return;

  vuint32 Fade = GetFade(SV_PointRegionLight(mobj->Sector, mobj->Origin));
  if (!Fade) return;

  DrawEntityModel(mobj, 0xffffffff, Fade, Alpha, Additive, TimeFrac, RPASS_Fog);
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderMobjsFog
//
//==========================================================================
void VAdvancedRenderLevel::RenderMobjsFog () {
  if (!r_draw_mobjs || !r_models) return;
  if (!r_dbg_advthing_draw_fog) return;
  VEntity **ent = visibleObjects.ptr();
  for (int count = visibleObjects.length(); count--; ++ent) {
    if (*ent == ViewEnt && (!r_chasecam || ViewEnt != cl->MO)) continue; // don't draw camera actor
    if (!HasAliasModel((*ent)->GetClass()->Name)) continue;
    RenderThingFog(*ent);
  }
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderTranslucentWallsAmbient
//
//  translucency simply "taints" (aka "shades") lighting, so we can
//  (and should) overlay them on ambient light buffer
//
//  dynamic light should be modified by translucent surfaces too, but
//  i'll do it later (maybe)
//
//==========================================================================
void VAdvancedRenderLevel::RenderTranslucentWallsAmbient () {
  if (traspFirst >= traspUsed) return;
  trans_sprite_t *twi = &trans_sprites[traspFirst];
  Drawer->BeginTranslucentPolygonAmbient();
  for (int f = traspFirst; f < traspUsed; ++f, ++twi) {
    if (twi->type) continue; // not a wall
    if (twi->Alpha >= 1.0f) continue; // not a translucent
    //GCon->Log("!!!");
    check(twi->surf);
    Drawer->DrawTranslucentPolygonAmbient(twi->surf, twi->Alpha, twi->Additive);
  }
  //Drawer->EndTranslucentPolygonAmbient();
  // we don't need to render translucent walls anymore
  //traspUsed = traspFirst;
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderTranslucentWallsDecals
//
//==========================================================================
void VAdvancedRenderLevel::RenderTranslucentWallsDecals () {
  if (traspFirst >= traspUsed) return;
  trans_sprite_t *twi = &trans_sprites[traspFirst];
  //Drawer->BeginTranslucentPolygonAmbient();
  for (int f = traspFirst; f < traspUsed; ++f, ++twi) {
    if (twi->type) continue; // not a wall
    if (twi->Alpha >= 1.0f) continue; // not a translucent
    //GCon->Log("!!!");
    check(twi->surf);
    Drawer->DrawTranslucentPolygonDecals(twi->surf, twi->Alpha, twi->Additive);
  }
  //Drawer->EndTranslucentPolygonAmbient();
  // we don't need to render translucent walls anymore
  //traspUsed = traspFirst;
}
