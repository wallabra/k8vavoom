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
extern VCvarB r_brightmaps;
extern VCvarB r_brightmaps_sprite;

extern VCvarB r_fake_shadows_alias_models;

VCvarB r_sort_sprites("r_sort_sprites", true, "Sprite sorting.", CVAR_Archive);
VCvarB r_draw_mobjs("r_draw_mobjs", true, "Draw mobjs?", /*CVAR_Archive|*/CVAR_PreInit);
VCvarB r_draw_psprites("r_draw_psprites", true, "Draw psprites?", /*CVAR_Archive|*/CVAR_PreInit);
VCvarB r_models("r_models", true, "Allow models?", CVAR_Archive);
VCvarB r_view_models("r_view_models", false, "View models?", CVAR_Archive);
VCvarB r_model_shadows("r_model_shadows", true, "Draw model shadows in advanced renderer?", CVAR_Archive);
VCvarB r_model_light("r_model_light", true, "Draw model light in advanced renderer?", CVAR_Archive);
VCvarB r_drawfuzz("r_drawfuzz", false, "Draw fuzz effect?", CVAR_Archive);
VCvarF r_transsouls("r_transsouls", "1", "Translucent Lost Souls?", CVAR_Archive);


//==========================================================================
//
//  VRenderLevelShared::RenderAliasModel
//
//==========================================================================
bool VRenderLevelShared::RenderAliasModel (VEntity *mobj, vuint32 light,
  vuint32 Fade, float Alpha, bool Additive, ERenderPass Pass)
{
  if (!r_models) return false;

  float TimeFrac = 0;
  if (mobj->State->Time > 0) {
    TimeFrac = 1.0f-(mobj->StateTime/mobj->State->Time);
    TimeFrac = midval(0.0f, TimeFrac, 1.0f);
  }

  // draw it
  if (Alpha < 1.0f || Additive) {
    if (!CheckAliasModelFrame(mobj, TimeFrac)) return false;
    QueueTranslucentAliasModel(mobj, light, Fade, Alpha, Additive, TimeFrac);
    return true;
  } else {
    return DrawEntityModel(mobj, light, Fade, 1.0f, false, TimeFrac, Pass);
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderThing
//
//==========================================================================
void VRenderLevelShared::RenderThing (VEntity *mobj, ERenderPass Pass) {
  if (mobj == ViewEnt && (!r_chasecam || ViewEnt != cl->MO)) return; // don't draw camera actor

  if (Pass == RPASS_Normal) {
    //if ((mobj->EntityFlags&VEntity::EF_NoSector) || (mobj->EntityFlags&VEntity::EF_Invisible)) return;
    if (!mobj->State || (mobj->GetFlags()&(_OF_Destroyed|_OF_DelayedDestroy))) return;
    if (mobj->EntityFlags&(VEntity::EF_NoSector|VEntity::EF_Invisible)) return;
  }

  int RendStyle = mobj->RenderStyle;
  if (RendStyle == STYLE_None) return;

  if (Pass == RPASS_Normal) {
    // skip things in subsectors that are not visible
    /*
    const unsigned SubIdx = (unsigned)(ptrdiff_t)(mobj->SubSector-Level->Subsectors);
    if (!(BspVisThing[SubIdx>>3]&(1<<(SubIdx&7)))) return;
    // mark as visible, why not?
    // use bsp visibility, to not mark "adjacent" things
    //if (BspVis[SubIdx>>3]&(1<<(SubIdx&7))) mobj->FlagsEx |= VEntity::EFEX_Rendered;
    */
    if (!IsThingVisible(mobj)) return;
  }

  float Alpha = mobj->Alpha;
  bool Additive = false;

  if (RendStyle == STYLE_SoulTrans) {
    RendStyle = STYLE_Translucent;
    Alpha = r_transsouls;
  } else if (RendStyle == STYLE_OptFuzzy) {
    RendStyle = (r_drawfuzz ? STYLE_Fuzzy : STYLE_Translucent);
  }

  switch (RendStyle) {
    case STYLE_None: return;
    case STYLE_Normal: Alpha = 1.0f; break;
    case STYLE_Fuzzy: Alpha = FUZZY_ALPHA; break;
    case STYLE_Add: Additive = true; break;
    case STYLE_Stencil: break;
    case STYLE_AddStencil: Additive = true; break;
  }
  if (Alpha <= 0.01f) return; // no reason to render it, it is invisible
  if (Alpha > 1.0f) Alpha = 1.0f;

  // setup lighting
  vuint32 light, seclight;

  if (RendStyle == STYLE_Fuzzy) {
    light = seclight = 0;
  } else if ((mobj->State->Frame&VState::FF_FULLBRIGHT) ||
             (mobj->EntityFlags&(VEntity::EF_FullBright|VEntity::EF_Bright))) {
    light = 0xffffffff;
    seclight = (r_brightmaps && r_brightmaps_sprite ? LightPoint(mobj->Origin, mobj->Radius, mobj->Height, nullptr, mobj->SubSector) : light);
  } else {
    light = seclight = LightPoint(mobj->Origin, mobj->Radius, mobj->Height, nullptr, mobj->SubSector);
    //GCon->Logf("%s: radius=%f; height=%f", *mobj->GetClass()->GetFullName(), mobj->Radius, mobj->Height);
  }

  //FIXME: fake "solid color" with colored light for now
  if (RendStyle == STYLE_Stencil || RendStyle == STYLE_AddStencil) {
    light = (light&0xff000000)|(mobj->StencilColor&0xffffff);
    seclight = (seclight&0xff000000)|(mobj->StencilColor&0xffffff);
  }

  vuint32 Fade = GetFade(SV_PointRegionLight(mobj->Sector, mobj->Origin));

  // try to draw a model
  // if it's a script and it doesn't specify model for this frame, draw sprite instead
  if (!RenderAliasModel(mobj, light, Fade, Alpha, Additive, Pass)) {
    QueueSprite(mobj, light, Fade, Alpha, Additive, seclight);
  } else if (r_fake_shadows_alias_models) {
    QueueSprite(mobj, light, Fade, Alpha, Additive, seclight, true); // only shadow
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
  if (Pass == RPASS_Normal) {
    // render things
    for (TThinkerIterator<VEntity> Ent(Level); Ent; ++Ent) {
      RenderThing(*Ent, RPASS_Normal);
    }
  } else {
    // we have a list here
    for (auto &&ent : visibleObjects) RenderThing(ent, Pass);
  }
}
