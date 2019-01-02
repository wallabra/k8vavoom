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

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"
#include "r_local.h"
#include "sv_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

enum
{
  SPR_VP_PARALLEL_UPRIGHT,
  SPR_FACING_UPRIGHT,
  SPR_VP_PARALLEL,
  SPR_ORIENTED,
  SPR_VP_PARALLEL_ORIENTED,
  SPR_VP_PARALLEL_UPRIGHT_ORIENTED,
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern VCvarB   r_chasecam;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

extern VCvarB     r_draw_mobjs;
extern VCvarB     r_draw_psprites;
extern VCvarB     r_models;
extern VCvarB     r_view_models;
extern VCvarB     r_model_shadows;
extern VCvarB     r_model_light;
extern VCvarB     r_sort_sprites;
extern VCvarB     r_fix_sprite_offsets;
extern VCvarI     r_sprite_fix_delta;
extern VCvarB     r_drawfuzz;
extern VCvarF     r_transsouls;
extern VCvarI     crosshair;
extern VCvarF     crosshair_alpha;
extern VCvarI     r_max_model_lights;
extern VCvarI     r_max_model_shadows;


// CODE --------------------------------------------------------------------

//==========================================================================
//
//  VAdvancedRenderLevel::RenderThingAmbient
//
//==========================================================================

void VAdvancedRenderLevel::RenderThingAmbient(VEntity *mobj)
{
  guard(VAdvancedRenderLevel::RenderThingAmbient);
  int RendStyle = mobj->RenderStyle;
  float Alpha = mobj->Alpha;
  bool Additive = false;

  if (RendStyle == STYLE_SoulTrans)
  {
    RendStyle = STYLE_Translucent;
    Alpha = r_transsouls;
  }
  else if (RendStyle == STYLE_OptFuzzy)
  {
    RendStyle = r_drawfuzz ? STYLE_Fuzzy : STYLE_Translucent;
  }

  switch (RendStyle)
  {
  case STYLE_None:
    return;

  case STYLE_Normal:
    Alpha = 1.0;
    break;

  case STYLE_Fuzzy:
    Alpha = FUZZY_ALPHA;
    break;

  case STYLE_Add:
    if (Alpha == 1.0)
    {
      Alpha -= 0.00001;
    }
    Additive = true;
    break;
  }
  Alpha = MID(0.0, Alpha, 1.0);

  //  Setup lighting
  vuint32 light;
  if (RendStyle == STYLE_Fuzzy)
  {
    light = 0;
  }
  else if ((mobj->State->Frame & VState::FF_FULLBRIGHT) ||
    (mobj->EntityFlags & (VEntity::EF_FullBright | VEntity::EF_Bright)))
  {
    light = 0xffffffff;
  }
  else
  {
    if (!r_model_light || !r_model_shadows)
    {
      // Use old way of lighting
      light = LightPoint(mobj->Origin, mobj);
    }
    else
    {
      light = LightPointAmbient(mobj->Origin, mobj);
    }
  }

  float TimeFrac = 0;
  if (mobj->State->Time > 0)
  {
    TimeFrac = 1.0 - (mobj->StateTime / mobj->State->Time);
    TimeFrac = MID(0.0, TimeFrac, 1.0);
  }

  DrawEntityModel(mobj, light, 0, Alpha, Additive, TimeFrac, RPASS_Ambient);
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::RenderMobjsAmbient
//
//==========================================================================

void VAdvancedRenderLevel::RenderMobjsAmbient()
{
  guard(VAdvancedRenderLevel::RenderMobjsAmbient);
  if (!r_draw_mobjs || !r_models)
  {
    return;
  }

  for (TThinkerIterator<VEntity> Ent(Level); Ent; ++Ent)
  {
    if (*Ent == ViewEnt && (!r_chasecam || ViewEnt != cl->MO))
    {
      //  Don't draw camera actor.
      continue;
    }

    if ((Ent->EntityFlags & VEntity::EF_NoSector) ||
      (Ent->EntityFlags & VEntity::EF_Invisible))
    {
      continue;
    }

    if (!Ent->State)
    {
      continue;
    }

    //  Skip things in subsectors that are not visible.
    int SubIdx = Ent->SubSector - Level->Subsectors;
    if (!(BspVis[SubIdx >> 3] & (1 << (SubIdx & 7))))
    {
      continue;
    }

    RenderThingAmbient(*Ent);
  }
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::RenderThingTextures
//
//==========================================================================

void VAdvancedRenderLevel::RenderThingTextures(VEntity *mobj)
{
  guard(VAdvancedRenderLevel::RenderThingAmbient);
  int RendStyle = mobj->RenderStyle;
  float Alpha = mobj->Alpha;
  bool Additive = false;

  if (RendStyle == STYLE_SoulTrans)
  {
    RendStyle = STYLE_Translucent;
    Alpha = r_transsouls;
  }
  else if (RendStyle == STYLE_OptFuzzy)
  {
    RendStyle = r_drawfuzz ? STYLE_Fuzzy : STYLE_Translucent;
  }

  switch (RendStyle)
  {
  case STYLE_None:
    return;

  case STYLE_Normal:
    Alpha = 1.0;
    break;

  case STYLE_Translucent:
    Alpha = mobj->Alpha;
    break;

  case STYLE_Fuzzy:
    Alpha = FUZZY_ALPHA;
    break;

  case STYLE_Add:
    if (Alpha == 1.0)
    {
      Alpha -= 0.00001;
    }
    Additive = true;
    break;
  }
  Alpha = MID(0.0, Alpha, 1.0);

  float TimeFrac = 0;
  if (mobj->State->Time > 0)
  {
    TimeFrac = 1.0 - (mobj->StateTime / mobj->State->Time);
    TimeFrac = MID(0.0, TimeFrac, 1.0);
  }

  DrawEntityModel(mobj, 0xffffffff, 0, Alpha, Additive, TimeFrac, RPASS_Textures);
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::RenderMobjsTextures
//
//==========================================================================

void VAdvancedRenderLevel::RenderMobjsTextures()
{
  guard(VAdvancedRenderLevel::RenderMobjsTextures);
  if (!r_draw_mobjs || !r_models)
  {
    return;
  }

  for (TThinkerIterator<VEntity> Ent(Level); Ent; ++Ent)
  {
    if (*Ent == ViewEnt && (!r_chasecam || ViewEnt != cl->MO))
    {
      //  Don't draw camera actor.
      continue;
    }

    if ((Ent->EntityFlags & VEntity::EF_NoSector) ||
      (Ent->EntityFlags & VEntity::EF_Invisible))
    {
      continue;
    }

    if (!Ent->State)
    {
      continue;
    }

    //  Skip things in subsectors that are not visible.
    int SubIdx = Ent->SubSector - Level->Subsectors;
    if (!(BspVis[SubIdx >> 3] & (1 << (SubIdx & 7))))
    {
      continue;
    }

    RenderThingTextures(*Ent);
  }
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::IsTouchedByLight
//
//==========================================================================

bool VAdvancedRenderLevel::IsTouchedByLight(VEntity *Ent, bool Count)
{
  guard(VAdvancedRenderLevel::IsTouchedByLight);
  TVec Delta = Ent->Origin - CurrLightPos;
  float Dist = Ent->Radius + CurrLightRadius;
  if (Delta.x > Dist || Delta.y > Dist)
  {
    return false;
  }
  if (Delta.z < -CurrLightRadius)
  {
    return false;
  }
  if (Delta.z > CurrLightRadius + Ent->Height)
  {
    return false;
  }
  Delta.z = 0;
  if (Delta.Length() > Dist)
  {
    return false;
  }
  if (Count)
  {
    Ent->NumTouchingLights += 1;
  }

  return true;
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::RenderThingLight
//
//==========================================================================

void VAdvancedRenderLevel::RenderThingLight(VEntity *mobj)
{
  guard(VAdvancedRenderLevel::RenderThingLight);
  // Use advanced lighting style
  int RendStyle = mobj->RenderStyle;
  float Alpha = mobj->Alpha;
  bool Additive = false;

  if (RendStyle == STYLE_SoulTrans)
  {
    RendStyle = STYLE_Translucent;
    Alpha = r_transsouls;
  }
  else if (RendStyle == STYLE_OptFuzzy)
  {
    RendStyle = r_drawfuzz ? STYLE_Fuzzy : STYLE_Translucent;
  }

  switch (RendStyle)
  {
  case STYLE_None:
    return;

  case STYLE_Normal:
    Alpha = 1.0;
    break;

  case STYLE_Fuzzy:
    Alpha = FUZZY_ALPHA;
    break;

  case STYLE_Add:
    if (Alpha == 1.0)
    {
      Alpha -= 0.00001;
    }
    Additive = true;
    break;
  }
  Alpha = MID(0.0, Alpha, 1.0);

  if (Alpha < 1.0 && RendStyle == STYLE_Fuzzy)
  {
    return;
  }

  float TimeFrac = 0;
  if (mobj->State->Time > 0)
  {
    TimeFrac = 1.0 - (mobj->StateTime / mobj->State->Time);
    TimeFrac = MID(0.0, TimeFrac, 1.0);
  }

  DrawEntityModel(mobj, 0xffffffff, 0, Alpha, Additive, TimeFrac, RPASS_Light);
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::ResetMobjsLightCount
//
//==========================================================================

void VAdvancedRenderLevel::ResetMobjsLightCount()
{
  guard(VAdvancedRenderLevel::RenderMobjsLight);
  if (!r_draw_mobjs || !r_models)
  {
    return;
  }

  for (TThinkerIterator<VEntity> Ent(Level); Ent; ++Ent)
  {
    if (Ent->NumTouchingLights == 0)
    {
      // No need to do anything
      continue;
    }

    if ((Ent->EntityFlags & VEntity::EF_NoSector) ||
      (Ent->EntityFlags & VEntity::EF_Invisible))
    {
      continue;
    }

    if (!Ent->State)
    {
      continue;
    }

    if (!IsTouchedByLight(*Ent, false))
    {
      continue;
    }

    Ent->NumTouchingLights = 0;
  }
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::RenderMobjsLight
//
//==========================================================================

void VAdvancedRenderLevel::RenderMobjsLight()
{
  guard(VAdvancedRenderLevel::RenderMobjsLight);
  if (!r_draw_mobjs || !r_models || !r_model_light)
  {
    return;
  }

  for (TThinkerIterator<VEntity> Ent(Level); Ent; ++Ent)
  {
    if (Ent->NumTouchingLights > r_max_model_lights)
    {
      // Limit maximum lights for this Entity
      continue;
    }

    if (*Ent == ViewEnt && (!r_chasecam || ViewEnt != cl->MO))
    {
      //  Don't draw camera actor.
      continue;
    }

    if ((Ent->EntityFlags & VEntity::EF_NoSector) ||
      (Ent->EntityFlags & VEntity::EF_Invisible))
    {
      continue;
    }

    if (!Ent->State)
    {
      continue;
    }

    //  Skip things in subsectors that are not visible.
    int SubIdx = Ent->SubSector - Level->Subsectors;
    if (!(LightBspVis[SubIdx >> 3] & (1 << (SubIdx & 7))))
    {
      continue;
    }

    if (!IsTouchedByLight(*Ent, true))
    {
      continue;
    }

    RenderThingLight(*Ent);
  }
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::RenderThingShadow
//
//==========================================================================

void VAdvancedRenderLevel::RenderThingShadow(VEntity *mobj)
{
  guard(VAdvancedRenderLevel::RenderThingShadow);
  int RendStyle = mobj->RenderStyle;
  float Alpha = mobj->Alpha;
  bool Additive = false;

  if (RendStyle == STYLE_SoulTrans)
  {
    RendStyle = STYLE_Translucent;
    Alpha = r_transsouls;
  }
  else if (RendStyle == STYLE_OptFuzzy)
  {
    RendStyle = r_drawfuzz ? STYLE_Fuzzy : STYLE_Translucent;
  }

  switch (RendStyle)
  {
  case STYLE_None:
    return;

  case STYLE_Normal:
    Alpha = 1.0;
    break;

  case STYLE_Fuzzy:
    Alpha = FUZZY_ALPHA;
    break;

  case STYLE_Add:
    if (Alpha == 1.0)
    {
      Alpha -= 0.00001;
    }
    Additive = true;
    break;
  }
  Alpha = MID(0.0, Alpha, 1.0);

  float TimeFrac = 0;
  if (mobj->State->Time > 0)
  {
    TimeFrac = 1.0 - (mobj->StateTime / mobj->State->Time);
    TimeFrac = MID(0.0, TimeFrac, 1.0);
  }

  DrawEntityModel(mobj, 0xffffffff, 0, Alpha, Additive, TimeFrac, RPASS_ShadowVolumes);
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::RenderMobjsShadow
//
//==========================================================================

void VAdvancedRenderLevel::RenderMobjsShadow()
{
  guard(VAdvancedRenderLevel::RenderMobjsShadow);
  if (!r_draw_mobjs || !r_models || !r_model_shadows)
  {
    return;
  }

  for (TThinkerIterator<VEntity> Ent(Level); Ent; ++Ent)
  {
    if (Ent->NumTouchingLights > r_max_model_shadows)
    {
      // Limit maximum shadows for this Entity
      continue;
    }

    if ((Ent->EntityFlags & VEntity::EF_NoSector) ||
      (Ent->EntityFlags & VEntity::EF_Invisible))
    {
      continue;
    }

    if (!Ent->State)
    {
      continue;
    }

    //  Skip things in subsectors that are not visible.
    int SubIdx = Ent->SubSector - Level->Subsectors;
    if (!(LightVis[SubIdx >> 3] & (1 << (SubIdx & 7))))
    {
      continue;
    }

    if (!IsTouchedByLight(*Ent, true))
    {
      continue;
    }

    RenderThingShadow(*Ent);
  }
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::RenderThingFog
//
//==========================================================================

void VAdvancedRenderLevel::RenderThingFog(VEntity *mobj)
{
  guard(VAdvancedRenderLevel::RenderThingFog);
  int RendStyle = mobj->RenderStyle;
  float Alpha = mobj->Alpha;
  bool Additive = false;

  if (RendStyle == STYLE_SoulTrans)
  {
    RendStyle = STYLE_Translucent;
    Alpha = r_transsouls;
  }
  else if (RendStyle == STYLE_OptFuzzy)
  {
    RendStyle = r_drawfuzz ? STYLE_Fuzzy : STYLE_Translucent;
  }

  switch (RendStyle)
  {
  case STYLE_None:
    return;

  case STYLE_Normal:
    Alpha = 1.0;
    break;

  case STYLE_Fuzzy:
    Alpha = FUZZY_ALPHA;
    break;

  case STYLE_Add:
    if (Alpha == 1.0)
    {
      Alpha -= 0.00001;
    }
    Additive = true;
    break;
  }
  Alpha = MID(0.0, Alpha, 1.0);

  vuint32 Fade = GetFade(SV_PointInRegion(mobj->Sector, mobj->Origin));
  if (!Fade)
  {
    return;
  }

  float TimeFrac = 0;
  if (mobj->State->Time > 0)
  {
    TimeFrac = 1.0 - (mobj->StateTime / mobj->State->Time);
    TimeFrac = MID(0.0, TimeFrac, 1.0);
  }

  DrawEntityModel(mobj, 0xffffffff, Fade, Alpha, Additive, TimeFrac, RPASS_Fog);
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::RenderMobjsFog
//
//==========================================================================

void VAdvancedRenderLevel::RenderMobjsFog()
{
  guard(VAdvancedRenderLevel::RenderMobjsFog);
  if (!r_draw_mobjs || !r_models)
  {
    return;
  }

  for (TThinkerIterator<VEntity> Ent(Level); Ent; ++Ent)
  {
    if (*Ent == ViewEnt && (!r_chasecam || ViewEnt != cl->MO))
    {
      //  Don't draw camera actor.
      continue;
    }

    if ((Ent->EntityFlags & VEntity::EF_NoSector) ||
      (Ent->EntityFlags & VEntity::EF_Invisible))
    {
      continue;
    }

    if (!Ent->State)
    {
      continue;
    }

    //  Skip things in subsectors that are not visible.
    int SubIdx = Ent->SubSector - Level->Subsectors;
    if (!(BspVis[SubIdx >> 3] & (1 << (SubIdx & 7))))
    {
      continue;
    }

    RenderThingFog(*Ent);
  }
  unguard;
}
