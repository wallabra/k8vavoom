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

static VCvarB r_dbg_thing_dump_vislist("r_dbg_thing_dump_vislist", false, "Dump built list of visible things?", 0);

static VCvarB r_thing_hiframe_use_camera_plane("r_thing_hiframe_use_camera_plane", true, "Use angle to camera plane to select rotation for sprites with detailed rotations?", CVAR_Archive);
static VCvarB r_thing_monster_use_camera_plane("r_thing_monster_use_camera_plane", true, "Use angle to camera plane to select monster rotation?", CVAR_Archive);
static VCvarB r_thing_missile_use_camera_plane("r_thing_missile_use_camera_plane", true, "Use angle to camera plane to select missile rotation?", CVAR_Archive);
static VCvarB r_thing_other_use_camera_plane("r_thing_other_use_camera_plane", true, "Use angle to camera plane to select non-monster rotation?", CVAR_Archive);


enum {
  SPR_VP_PARALLEL_UPRIGHT, // 0 (default)
  SPR_FACING_UPRIGHT, // 1
  SPR_VP_PARALLEL, // 2: parallel to camera visplane
  SPR_ORIENTED, // 3
  SPR_VP_PARALLEL_ORIENTED, // 4 (xy billboard)
  SPR_VP_PARALLEL_UPRIGHT_ORIENTED, // 5
  SPR_ORIENTED_OFS, // 6 (offset slightly by pitch -- for floor/ceiling splats)
};


VCvarB r_sort_sprites("r_sort_sprites", true, "Sprite sorting.", CVAR_Archive);
VCvarB r_draw_mobjs("r_draw_mobjs", true, "Draw mobjs?", /*CVAR_Archive|*/CVAR_PreInit);
VCvarB r_draw_psprites("r_draw_psprites", true, "Draw psprites?", /*CVAR_Archive|*/CVAR_PreInit);
VCvarB r_models("r_models", true, "Allow models?", CVAR_Archive);
VCvarB r_view_models("r_view_models", false, "View models?", CVAR_Archive);
VCvarB r_model_shadows("r_model_shadows", true, "Draw model shadows in advanced renderer?", CVAR_Archive);
VCvarB r_model_light("r_model_light", true, "Draw model light in advanced renderer?", CVAR_Archive);
VCvarB r_fix_sprite_offsets("r_fix_sprite_offsets", true, "Fix sprite offsets?", CVAR_Archive);
VCvarI r_sprite_fix_delta("r_sprite_fix_delta", "-7", "Sprite offset amount.", CVAR_Archive); // -6 seems to be ok for vanilla BFG explosion, and for imp fireball
VCvarB r_drawfuzz("r_drawfuzz", false, "Draw fuzz effect?", CVAR_Archive);
VCvarF r_transsouls("r_transsouls", "1", "Translucent Lost Souls?", CVAR_Archive);

extern VCvarB r_decals_enabled;
extern VCvarB r_decals_wall_masked;
extern VCvarB r_decals_wall_alpha;


//==========================================================================
//
//  R_DrawSpritePatch
//
//==========================================================================
void R_DrawSpritePatch (float x, float y, int sprite, int frame, int rot,
                        int TranslStart, int TranslEnd, int Color, float scale,
                        bool ignoreVScr)
{
  bool flip;
  int lump;

  spriteframe_t *sprframe = &sprites[sprite].spriteframes[frame&VState::FF_FRAMEMASK];
  flip = sprframe->flip[rot];
  lump = sprframe->lump[rot];
  VTexture *Tex = GTextureManager[lump];
  if (!Tex) return; // just in case

  (void)Tex->GetWidth();

  float x1 = x-Tex->SOffset*scale;
  float y1 = y-Tex->TOffset*scale;
  float x2 = x1+Tex->GetWidth()*scale;
  float y2 = y1+Tex->GetHeight()*scale;

  if (!ignoreVScr) {
    x1 *= fScaleX;
    y1 *= fScaleY;
    x2 *= fScaleX;
    y2 *= fScaleY;
  }

  Drawer->DrawSpriteLump(x1, y1, x2, y2, Tex, R_GetCachedTranslation(R_SetMenuPlayerTrans(TranslStart, TranslEnd, Color), nullptr), flip);
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
  #ifdef VVRENDER_FULL_ALIAS_MODEL_SHADOW_LIST
  allShadowModelObjects.reset();
  #endif

  int RendStyle;
  float Alpha;

  if (r_dbg_thing_dump_vislist) GCon->Logf("=== VISIBLE THINGS ===");
  for (TThinkerIterator<VEntity> Ent(Level); Ent; ++Ent) {
    VEntity *mobj = *Ent;
    if (!mobj->State || (mobj->GetFlags()&(_OF_Destroyed|_OF_DelayedDestroy))) continue;
    if (mobj->EntityFlags&(VEntity::EF_NoSector|VEntity::EF_Invisible)) continue;
    (*Ent)->NumRenderedShadows = 0; // for advanced renderer

    #ifdef VVRENDER_FULL_ALIAS_MODEL_SHADOW_LIST
    bool alphaDone = false;
    // collect all things with models (we'll need them in advrender)
    if (HasAliasModel(mobj->GetClass()->Name)) {
      alphaDone = true;
      if (!CalculateThingAlpha(mobj, RendStyle, Alpha)) continue; // invisible
      // ignore translucent things, they cannot cast a shadow
      if (RendStyle == STYLE_Normal && Alpha >= 1.0f) allShadowModelObjects.append(mobj);
    }
    #endif

    // skip things in subsectors that are not visible
    const unsigned SubIdx = (unsigned)(ptrdiff_t)(mobj->SubSector-Level->Subsectors);
    if (!(BspVisThing[SubIdx>>3]&(1<<(SubIdx&7)))) continue;

    #ifdef VVRENDER_FULL_ALIAS_MODEL_SHADOW_LIST
    if (!alphaDone)
    #endif
    {
      if (!CalculateThingAlpha(mobj, RendStyle, Alpha)) continue; // invisible
    }

    if (r_dbg_thing_dump_vislist) GCon->Logf("  <%s> (%f,%f,%f) 0x%08x", *mobj->GetClass()->GetFullName(), mobj->Origin.x, mobj->Origin.y, mobj->Origin.z, mobj->EntityFlags);
    // mark as visible, why not?
    // use bsp visibility, to not mark "adjacent" things
    if (BspVis[SubIdx>>3]&(1<<(SubIdx&7))) mobj->FlagsEx |= VEntity::EFEX_Rendered;
    visibleObjects.append(mobj);
  }

  // shrink list
  if (visibleObjects.capacity() > 16384) {
    if (visibleObjects.capacity()*2 >= visibleObjects.length()) {
      visibleObjects.setLength(visibleObjects.length(), true); // resize
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::QueueSprite
//
//==========================================================================
void VRenderLevelShared::QueueSprite (VEntity *thing, vuint32 light, vuint32 Fade, float Alpha, bool Additive, vuint32 seclight) {
  int spr_type = thing->SpriteType;

  TVec sprorigin = thing->GetDrawOrigin();
  TVec sprforward(0, 0, 0);
  TVec sprright(0, 0, 0);
  TVec sprup(0, 0, 0);

  // HACK: if sprite is additive, move is slightly closer to view
  // this is mostly for things like light flares
  if (Additive) {
    sprorigin -= viewforward*0.2f;
  }

  float dot;
  TVec tvec(0, 0, 0);
  float sr;
  float cr;
  int hangup = 0;
  //spr_type = SPR_ORIENTED;

  switch (spr_type) {
    case SPR_VP_PARALLEL_UPRIGHT:
      // Generate the sprite's axes, with sprup straight up in worldspace,
      // and sprright parallel to the viewplane. This will not work if the
      // view direction is very close to straight up or down, because the
      // cross product will be between two nearly parallel vectors and
      // starts to approach an undefined state, so we don't draw if the two
      // vectors are less than 1 degree apart
      dot = viewforward.z; // same as DotProduct(viewforward, sprup), because sprup is 0, 0, 1
      if (dot > 0.999848f || dot < -0.999848f) return; // cos(1 degree) = 0.999848f
      sprup = TVec(0, 0, 1);
      // CrossProduct(sprup, viewforward)
      sprright = Normalise(TVec(viewforward.y, -viewforward.x, 0));
      // CrossProduct(sprright, sprup)
      sprforward = TVec(-sprright.y, sprright.x, 0);
      break;

    case SPR_FACING_UPRIGHT:
      // Generate the sprite's axes, with sprup straight up in worldspace,
      // and sprright perpendicular to sprorigin. This will not work if the
      // view direction is very close to straight up or down, because the
      // cross product will be between two nearly parallel vectors and
      // starts to approach an undefined state, so we don't draw if the two
      // vectors are less than 1 degree apart
      tvec = Normalise(sprorigin-vieworg);
      dot = tvec.z; // same as DotProduct (tvec, sprup), because sprup is 0, 0, 1
      if (dot > 0.999848f || dot < -0.999848f) return; // cos(1 degree) = 0.999848f
      sprup = TVec(0, 0, 1);
      // CrossProduct(sprup, -sprorigin)
      sprright = Normalise(TVec(tvec.y, -tvec.x, 0));
      // CrossProduct(sprright, sprup)
      sprforward = TVec(-sprright.y, sprright.x, 0);
      break;

    case SPR_VP_PARALLEL:
      // Generate the sprite's axes, completely parallel to the viewplane.
      // There are no problem situations, because the sprite is always in
      // the same position relative to the viewer
      sprup = viewup;
      sprright = viewright;
      sprforward = viewforward;
      break;

    case SPR_ORIENTED:
    case SPR_ORIENTED_OFS:
      // generate the sprite's axes, according to the sprite's world orientation
      AngleVectors(thing->/*Angles*/GetSpriteDrawAngles(), sprforward, sprright, sprup);
      if (spr_type != SPR_ORIENTED) {
        hangup = (sprup.z > 0 ? 1 : sprup.z < 0 ? -1 : 0);
      }
      break;

    case SPR_VP_PARALLEL_ORIENTED:
      // Generate the sprite's axes, parallel to the viewplane, but
      // rotated in that plane around the centre according to the sprite
      // entity's roll angle. So sprforward stays the same, but sprright
      // and sprup rotate
      sr = msin(thing->Angles.roll);
      cr = mcos(thing->Angles.roll);

      sprforward = viewforward;
      sprright = TVec(viewright.x*cr+viewup.x*sr, viewright.y*cr+viewup.y*sr, viewright.z*cr+viewup.z*sr);
      sprup = TVec(viewright.x*(-sr)+viewup.x*cr, viewright.y*(-sr)+viewup.y*cr, viewright.z*(-sr)+viewup.z*cr);
      break;

    case SPR_VP_PARALLEL_UPRIGHT_ORIENTED:
      // Generate the sprite's axes, with sprup straight up in worldspace,
      // and sprright parallel to the viewplane and then rotated in that
      // plane around the centre according to the sprite entity's roll
      // angle. So sprforward stays the same, but sprright and sprup rotate
      // This will not work if the view direction is very close to straight
      // up or down, because the cross product will be between two nearly
      // parallel vectors and starts to approach an undefined state, so we
      // don't draw if the two vectors are less than 1 degree apart
      dot = viewforward.z;  //  same as DotProduct(viewforward, sprup), because sprup is 0, 0, 1
      if ((dot > 0.999848f) || (dot < -0.999848f))  // cos(1 degree) = 0.999848f
        return;

      sr = msin(thing->Angles.roll);
      cr = mcos(thing->Angles.roll);

      //  CrossProduct(TVec(0, 0, 1), viewforward)
      tvec = Normalise(TVec(viewforward.y, -viewforward.x, 0));
      //  CrossProduct(tvec, TVec(0, 0, 1))
      sprforward = TVec(-tvec.y, tvec.x, 0);
      //  Rotate
      sprright = TVec(tvec.x*cr, tvec.y*cr, tvec.z*cr+sr);
      sprup = TVec(tvec.x*(-sr), tvec.y*(-sr), tvec.z*(-sr)+cr);
      break;

    default:
      Sys_Error("QueueSprite: Bad sprite type %d", spr_type);
  }

  spritedef_t *sprdef;
  spriteframe_t *sprframe;

  int SpriteIndex = thing->GetEffectiveSpriteIndex();
  int FrameIndex = thing->GetEffectiveSpriteFrame();
  if (thing->FixedSpriteName != NAME_None) SpriteIndex = VClass::FindSprite(thing->FixedSpriteName);

  // decide which patch to use for sprite relative to player
  if ((unsigned)SpriteIndex >= MAX_SPRITE_MODELS) {
#ifdef PARANOID
    GCon->Logf(NAME_Dev, "Invalid sprite number %d", SpriteIndex);
#endif
    return;
  }

  sprdef = &sprites[SpriteIndex];
  if (FrameIndex >= sprdef->numframes) {
#ifdef PARANOID
    GCon->Logf(NAME_Dev, "Invalid sprite frame %d : %d", SpriteIndex, FrameIndex);
#endif
    return;
  }

  sprframe = &sprdef->spriteframes[FrameIndex];

  int lump;
  bool flip;

  if (sprframe->rotate) {
    // choose a different rotation based on player view
    //FIXME must use sprforward here?
    bool useCameraPlane;
    if (r_thing_hiframe_use_camera_plane && sprframe->lump[0] != sprframe->lump[1]) {
      useCameraPlane = true;
    } else {
           if (thing->IsMonster()) useCameraPlane = r_thing_monster_use_camera_plane;
      else if (thing->IsMissile()) useCameraPlane = r_thing_missile_use_camera_plane;
      else useCameraPlane = r_thing_other_use_camera_plane;
    }
    float ang = (useCameraPlane ?
      matan(sprorigin.y-vieworg.y, sprorigin.x-vieworg.x) :
      matan(sprforward.y+viewforward.y, sprforward.x+viewforward.x));
    const float angadd = (sprframe->lump[0] == sprframe->lump[1] ? 45.0f/2.0f : 45.0f/4.0f); //k8: is this right?
    //const float angadd = (useCameraPlane ? 45.0f/2.0f : 45.0f/4.0f);
    /*
    if (sprframe->lump[0] == sprframe->lump[1]) {
      ang = matan(sprorigin.y-vieworg.y, sprorigin.x-vieworg.x);
      ang = AngleMod(ang-thing->GetSpriteDrawAngles().yaw+180.0f+45.0f/2.0f);
    } else {
      ang = matan(sprforward.y+viewforward.y, sprforward.x+viewforward.x);
      ang = AngleMod(ang-thing->GetSpriteDrawAngles().yaw+180.0f+45.0f/4.0f);
    }
    */
    ang = AngleMod(ang-thing->GetSpriteDrawAngles().yaw+180.0f+angadd);
    vuint32 rot = (vuint32)(ang*16.0f/360.0f)&15;
    lump = sprframe->lump[rot];
    flip = sprframe->flip[rot];
  } else {
    // use single rotation for all views
    lump = sprframe->lump[0];
    flip = sprframe->flip[0];
  }

  if (lump <= 0) {
#ifdef PARANOID
    GCon->Logf(NAME_Dev, "Sprite frame %d : %d, not present", SpriteIndex, FrameIndex);
#endif
    // sprite lump is not present
    return;
  }

  VTexture *Tex = GTextureManager[lump];

  if (!Tex || Tex->Type == TEXTYPE_Null) return; // just in case

  //if (r_brightmaps && r_brightmaps_sprite && Tex->Brightmap && Tex->Brightmap->nofullbright) light = seclight; // disable fullbright
  if (r_brightmaps && r_brightmaps_sprite && Tex->nofullbright) light = seclight; // disable fullbright

  int TexWidth = Tex->GetWidth();
  int TexHeight = Tex->GetHeight();
  int TexSOffset = Tex->SOffset;
  int TexTOffset = Tex->TOffset;

  TVec sv[4];

  TVec start = -TexSOffset*sprright*thing->ScaleX;
  TVec end = (TexWidth-TexSOffset)*sprright*thing->ScaleX;

  if (r_fix_sprite_offsets && TexTOffset < TexHeight && 2*TexTOffset+r_sprite_fix_delta >= TexHeight) TexTOffset = TexHeight;
  TVec topdelta = TexTOffset*sprup*thing->ScaleY;
  TVec botdelta = (TexTOffset-TexHeight)*sprup*thing->ScaleY;

  sv[0] = sprorigin+start+botdelta;
  sv[1] = sprorigin+start+topdelta;
  sv[2] = sprorigin+end+topdelta;
  sv[3] = sprorigin+end+botdelta;

  //if (Fade != FADE_LIGHT) GCon->Logf("<%s>: Fade=0x%08x", *thing->GetClass()->GetFullName(), Fade);

  if (Alpha >= 1.0f && !Additive && Tex->isTranslucent()) Alpha = 0.9999;

  if (Alpha < 1.0f || Additive || r_sort_sprites) {
    int priority = 0;
    if (thing) {
           if (thing->EntityFlags&VEntity::EF_Bright) priority = 200;
      else if (thing->EntityFlags&VEntity::EF_FullBright) priority = 100;
      else if (thing->EntityFlags&(VEntity::EF_Corpse|VEntity::EF_Blasted)) priority = -120;
      else if (thing->Health <= 0) priority = -110;
      else if (thing->EntityFlags&VEntity::EF_NoBlockmap) priority = -200;
    }
    DrawTranslucentPoly(nullptr, sv, 4, lump, Alpha+(thing->RenderStyle == STYLE_Dark ? 1666.0f : 0.0f), Additive,
      thing->Translation, true/*isSprite*/, light, Fade, -sprforward,
      DotProduct(sprorigin, -sprforward), (flip ? -sprright : sprright)/thing->ScaleX,
      -sprup/thing->ScaleY, (flip ? sv[2] : sv[1]), priority
      , true, /*sprorigin*/thing->Origin, thing->GetUniqueId(), hangup);
  } else {
    Drawer->DrawSpritePolygon(sv, /*GTextureManager[lump]*/Tex, Alpha+(thing->RenderStyle == STYLE_Dark ? 1666.0f : 0.0f),
      Additive, GetTranslation(thing->Translation), ColorMap, light,
      Fade, -sprforward, DotProduct(sprorigin, -sprforward),
      (flip ? -sprright : sprright)/thing->ScaleX,
      -sprup/thing->ScaleY, (flip ? sv[2] : sv[1]), hangup);
  }
}


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
    RenderTranslucentAliasModel(mobj, light, Fade, Alpha, Additive, TimeFrac);
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
    const unsigned SubIdx = (unsigned)(ptrdiff_t)(mobj->SubSector-Level->Subsectors);
    if (!(BspVisThing[SubIdx>>3]&(1<<(SubIdx&7)))) return;
    // mark as visible, why not?
    // use bsp visibility, to not mark "adjacent" things
    if (BspVis[SubIdx>>3]&(1<<(SubIdx&7))) mobj->FlagsEx |= VEntity::EFEX_Rendered;
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
    VEntity **ent = visibleObjects.ptr();
    for (int count = visibleObjects.length(); count--; ++ent) {
      RenderThing(*ent, Pass);
    }
  }
}
