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


extern VCvarB gl_pic_filtering;
extern VCvarF fov;
extern VCvarB r_chasecam;

extern refdef_t refdef;


enum {
  SPR_VP_PARALLEL_UPRIGHT, // 0 (default)
  SPR_FACING_UPRIGHT, // 1
  SPR_VP_PARALLEL, // 2: parallel to camera visplane
  SPR_ORIENTED, // 3
  SPR_VP_PARALLEL_ORIENTED, // 4 (xy billboard)
  SPR_VP_PARALLEL_UPRIGHT_ORIENTED, // 5
  SPR_ORIENTED_OFS, // 6 (offset slightly by pitch -- for floor/ceiling splats)
};


VCvarB r_draw_mobjs("r_draw_mobjs", true, "Draw mobjs?", /*CVAR_Archive|*/CVAR_PreInit);
VCvarB r_draw_psprites("r_draw_psprites", true, "Draw psprites?", /*CVAR_Archive|*/CVAR_PreInit);
VCvarB r_models("r_models", true, "Allow models?", CVAR_Archive);
VCvarB r_view_models("r_view_models", false, "View models?", CVAR_Archive);
VCvarB r_model_shadows("r_model_shadows", false, "Draw model shadows in advanced renderer?", CVAR_Archive);
VCvarB r_model_light("r_model_light", true, "Draw model light in advanced renderer?", CVAR_Archive);
VCvarB r_sort_sprites("r_sort_sprites", false, "Sprite sorting.", CVAR_Archive);
VCvarB r_sprite_use_pofs("r_sprite_use_pofs", true, "Use PolygonOffset with sprite sorting to reduce sprite flickering?", CVAR_Archive);
VCvarB r_fix_sprite_offsets("r_fix_sprite_offsets", true, "Fix sprite offsets?", CVAR_Archive);
VCvarI r_sprite_fix_delta("r_sprite_fix_delta", "-7", "Sprite offset amount.", CVAR_Archive); // -6 seems to be ok for vanilla BFG explosion, and for imp fireball
VCvarB r_drawfuzz("r_drawfuzz", false, "Draw fuzz effect?", CVAR_Archive);
VCvarF r_transsouls("r_transsouls", "1", "Translucent Lost Souls?", CVAR_Archive);
VCvarI crosshair("crosshair", "2", "Crosshair type (0-2).", CVAR_Archive);
VCvarF crosshair_alpha("crosshair_alpha", "0.6", "Crosshair opacity.", CVAR_Archive);

static VCvarI r_crosshair_yofs("r_crosshair_yofs", "0", "Crosshair y offset (>0: down).", CVAR_Archive);

static VCvarF r_sprite_pofs("r_sprite_pofs", "128", "DEBUG");
static VCvarF r_sprite_pslope("r_sprite_pslope", "-1.0", "DEBUG");

VCvarB r_draw_adjacent_subsector_things("r_draw_adjacent_subsector_things", true, "Draw things subsectors adjacent to visible subsectors (can fix disappearing things)?", CVAR_Archive);

extern VCvarB r_decals_enabled;
extern VCvarB r_decals_wall_masked;
extern VCvarB r_decals_wall_alpha;


//==========================================================================
//
//  VRenderLevelShared::DrawTranslucentPoly
//
//==========================================================================
void VRenderLevelShared::DrawTranslucentPoly (surface_t *surf, TVec *sv,
  int count, int lump, float Alpha, bool Additive, int translation,
  bool isSprite, vuint32 light, vuint32 Fade, const TVec &normal, float pdist,
  const TVec &saxis, const TVec &taxis, const TVec &texorg, int priority,
  bool useSprOrigin, const TVec &sprOrigin, vuint32 objid, int hangup)
{
  check(count >= 0);
  if (count == 0 || Alpha < 0.0002f) return;

  // make room
  if (traspUsed == traspSize) {
    if (traspSize >= 0xfffffff) Sys_Error("Too many translucent entities");
    traspSize += 0x10000;
    trans_sprites = (trans_sprite_t *)Z_Realloc(trans_sprites, traspSize*sizeof(trans_sprites[0]));
  }

  float dist;
  if (useSprOrigin) {
    TVec mid = sprOrigin;
    //dist = fabsf(DotProduct(mid-vieworg, viewforward));
    dist = LengthSquared(mid-vieworg);
  } else {
#if 1
    TVec mid(0, 0, 0);
    for (int i = 0; i < count; ++i) mid += sv[i];
    mid /= count;
    //dist = fabsf(DotProduct(mid-vieworg, viewforward));
    dist = LengthSquared(mid-vieworg);
#else
    // select nearest vertex
    dist = LengthSquared(sv[0]-vieworg);
    for (int i = 1; i < count; ++i) {
      const float nd = LengthSquared(sv[i]-vieworg);
      if (dist > nd) dist = nd;
    }
#endif
  }

  //const float dist = fabsf(DotProduct(mid-vieworg, viewforward));
  //float dist = Length(mid-vieworg);

  trans_sprite_t &spr = trans_sprites[traspUsed++];
  if (isSprite) memcpy(spr.Verts, sv, sizeof(TVec)*4);
  spr.dist = dist;
  spr.lump = lump;
  spr.normal = normal;
  spr.pdist = pdist;
  spr.saxis = saxis;
  spr.taxis = taxis;
  spr.texorg = texorg;
  spr.surf = surf;
  spr.Alpha = Alpha;
  spr.Additive = Additive;
  spr.translation = translation;
  spr.type = (isSprite ? 1 : 0);
  spr.light = light;
  spr.objid = objid;
  spr.hangup = hangup;
  spr.Fade = Fade;
  spr.prio = priority;
}


//==========================================================================
//
//  VRenderLevelShared::RenderTranslucentAliasModel
//
//==========================================================================
void VRenderLevelShared::RenderTranslucentAliasModel (VEntity *mobj, vuint32 light, vuint32 Fade, float Alpha, bool Additive, float TimeFrac) {
  // make room
  if (traspUsed == traspSize) {
    if (traspSize >= 0xfffffff) Sys_Error("Too many translucent entities");
    traspSize += 0x10000;
    trans_sprites = (trans_sprite_t *)Z_Realloc(trans_sprites, traspSize*sizeof(trans_sprites[0]));
  }

  //const float dist = fabsf(DotProduct(mobj->Origin-vieworg, viewforward));
  const float dist = LengthSquared(mobj->Origin-vieworg);

  trans_sprite_t &spr = trans_sprites[traspUsed++];
  spr.Ent = mobj;
  spr.light = light;
  spr.Fade = Fade;
  spr.Alpha = Alpha;
  spr.Additive = Additive;
  spr.dist = dist;
  spr.type = 2;
  spr.TimeFrac = TimeFrac;
  spr.lump = -1; // has no sense
  spr.objid = (mobj ? mobj->GetUniqueId() : 0);
  spr.prio = 0; // normal priority
  spr.hangup = 0;
}


//==========================================================================
//
//  VRenderLevelShared::RenderSprite
//
//==========================================================================
void VRenderLevelShared::RenderSprite (VEntity *thing, vuint32 light, vuint32 Fade, float Alpha, bool Additive) {
  int spr_type = thing->SpriteType;

  TVec sprorigin = thing->Origin;
  sprorigin.z -= thing->FloorClip;
  TVec sprforward(0, 0, 0);
  TVec sprright(0, 0, 0);
  TVec sprup(0, 0, 0);

  // HACK: if sprite is additive, move is slightly closer to view
  // this is mostly for things like light flares
  if (Additive) {
    sprorigin -= viewforward*0.2;
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
      AngleVectors(thing->Angles, sprforward, sprright, sprup);
      if (spr_type != SPR_ORIENTED) {
        /*
        const float pitch = thing->Angles.pitch;
        if (pitch == 90.0f) {
          // floor
          sprorigin.z += 0.01f;
        } else if (pitch == 180.0f) {
          // ceiling
          sprorigin.z -= 0.01f;
        } else {
          // slope
          TVec vofs;
          AngleVectorPitch(thing->Angles.pitch, vofs);
          //GCon->Logf("vofs: (%f,%f,%f); pitch=%f", vofs.x, vofs.y, vofs.z, pitch);
          sprorigin -= vofs*0.01f;
        }
        */
        /*
        {
          TVec vofs;
          AngleVectorPitch(pitch, vofs);
          GCon->Logf("vofs: (%f,%f,%f); pitch=%f", vofs.x, vofs.y, vofs.z, pitch);
        }
        */
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
      Sys_Error("RenderSprite: Bad sprite type %d", spr_type);
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
    float ang = matan(sprorigin.y-vieworg.y, sprorigin.x-vieworg.x);
    if (sprframe->lump[0] == sprframe->lump[1]) {
      ang = AngleMod(ang-thing->Angles.yaw+180.0f+45.0f/2.0f);
    } else {
      ang = matan(sprforward.y+viewforward.y, sprforward.x+viewforward.x);
      ang = AngleMod(ang-thing->Angles.yaw+180.0f+45.0f/4.0f);
    }
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

  //FIXME: k8: i don't know why yet, but it doesn't work with sorting
  /*
  if (hangup) {
    Drawer->DrawSpritePolygon(sv, GTextureManager[lump], Alpha,
      Additive, GetTranslation(thing->Translation), ColourMap, light,
      Fade, -sprforward, DotProduct(sprorigin, -sprforward),
      (flip ? -sprright : sprright)/thing->ScaleX,
      -sprup/thing->ScaleY, (flip ? sv[2] : sv[1]), hangup);
    return;
  }
  */

  if (Alpha < 1.0f || Additive || r_sort_sprites) {
    int priority = 0;
    if (thing) {
           if (thing->EntityFlags&VEntity::EF_Bright) priority = 200;
      else if (thing->EntityFlags&VEntity::EF_FullBright) priority = 100;
      else if (thing->EntityFlags&(VEntity::EF_Corpse|VEntity::EF_Blasted)) priority = -120;
      else if (thing->Health <= 0) priority = -110;
      else if (thing->EntityFlags&VEntity::EF_NoBlockmap) priority = -200;
    }
    DrawTranslucentPoly(nullptr, sv, 4, lump, Alpha, Additive,
      thing->Translation, true/*isSprite*/, light, Fade, -sprforward,
      DotProduct(sprorigin, -sprforward), (flip ? -sprright : sprright)/thing->ScaleX,
      -sprup/thing->ScaleY, (flip ? sv[2] : sv[1]), priority
      , true, /*sprorigin*/thing->Origin, thing->GetUniqueId(), hangup);
  } else {
    Drawer->DrawSpritePolygon(sv, GTextureManager[lump], Alpha,
      Additive, GetTranslation(thing->Translation), ColourMap, light,
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
    TimeFrac = MID(0.0f, TimeFrac, 1.0f);
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

  if (Pass == RPASS_Normal) {
    if (RendStyle == STYLE_None) return;

    // skip things in subsectors that are not visible
    //TODO: for advanced renderer, we may need to render things several times, so
    //      it is good place to cache them for the given frame
    const unsigned SubIdx = (unsigned)(ptrdiff_t)(mobj->SubSector-Level->Subsectors);
    if (!(BspVisThing[SubIdx>>3]&(1<<(SubIdx&7)))) return;
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
  if (Alpha <= 0.0002f) return; // no reason to render it, it is invisible
  if (Alpha > 1.0f) Alpha = 1.0f;

  //Alpha = MID(0.0f, Alpha, 1.0f);
  //if (!Alpha) return; // never make a vissprite when MF2_DONTDRAW is flagged

  // setup lighting
  vuint32 light;

  if (RendStyle == STYLE_Fuzzy) {
    light = 0;
  } else if ((mobj->State->Frame&VState::FF_FULLBRIGHT) ||
             (mobj->EntityFlags&(VEntity::EF_FullBright|VEntity::EF_Bright))) {
    light = 0xffffffff;
  } else {
    light = LightPoint(mobj->Origin, mobj->Radius);
  }

  //FIXME: fake "solid color" with colored light for now
  if (RendStyle == STYLE_Stencil || RendStyle == STYLE_AddStencil) {
    light = (light&0xff000000)|(mobj->StencilColour&0xffffff);
  }

  vuint32 Fade = GetFade(SV_PointInRegion(mobj->Sector, mobj->Origin));

  // try to draw a model
  // if it's a script and it doesn't specify model for this frame, draw sprite instead
  if (!RenderAliasModel(mobj, light, Fade, Alpha, Additive, Pass)) {
    RenderSprite(mobj, light, Fade, Alpha, Additive);
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderMobjs
//
//==========================================================================
void VRenderLevelShared::RenderMobjs (ERenderPass Pass) {
  if (!r_draw_mobjs) return;

#if 0
  if (r_draw_adjacent_subsector_things) {
    //int zzcount = 0;
    for (unsigned sidx = 0; sidx < (unsigned)Level->NumSubsectors; ++sidx) {
      if ((sidx&7) == 0) BspVisThing[sidx>>3] |= BspVis[sidx>>3];
      if (BspVis[sidx>>3]&(1U<<(sidx&7))) {
        subsector_t *sub = &Level->Subsectors[sidx];
        int sgcount = sub->numlines;
        if (sgcount) {
          seg_t *seg = &Level->Segs[sub->firstline];
          for (; sgcount--; ++seg) {
            if (seg->linedef && !(seg->linedef->flags&ML_TWOSIDED)) continue; // don't go through solid walls
            seg_t *pseg = seg->partner;
            if (!pseg || !pseg->front_sub) continue;
            unsigned psidx = (unsigned)(ptrdiff_t)(pseg->front_sub-Level->Subsectors);
            //if (!(BspVisThing[psidx>>3]&(1U<<(psidx&7)))) ++zzcount;
            BspVisThing[psidx>>3] |= 1U<<(psidx&7);
          }
        }
      }
    }
    //if (zzcount) GCon->Logf("additional thing subsectors: %d", zzcount);
  }
#endif

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


//==========================================================================
//
//  VRenderLevelShared::BuildVisibleObjectsList
//
//  this should be called after `RenderWorld()`
//
//==========================================================================
void VRenderLevelShared::BuildVisibleObjectsList () {
  visibleObjects.reset();

  for (TThinkerIterator<VEntity> Ent(Level); Ent; ++Ent) {
    VEntity *mobj = *Ent;
    if (!mobj->State || (mobj->GetFlags()&(_OF_Destroyed|_OF_DelayedDestroy))) continue;
    if (mobj->EntityFlags&(VEntity::EF_NoSector|VEntity::EF_Invisible)) continue;

    int RendStyle = mobj->RenderStyle;
    if (RendStyle == STYLE_None) continue;

    // skip things in subsectors that are not visible
    const unsigned SubIdx = (unsigned)(ptrdiff_t)(mobj->SubSector-Level->Subsectors);
    if (!(BspVisThing[SubIdx>>3]&(1<<(SubIdx&7)))) continue;

    float Alpha = mobj->Alpha;

    if (RendStyle == STYLE_SoulTrans) {
      RendStyle = STYLE_Translucent;
      Alpha = r_transsouls;
    } else if (RendStyle == STYLE_OptFuzzy) {
      RendStyle = (r_drawfuzz ? STYLE_Fuzzy : STYLE_Translucent);
    } else if (RendStyle == STYLE_Normal) {
      Alpha = 1.0f;
    }
    if (RendStyle == STYLE_Fuzzy) Alpha = FUZZY_ALPHA;

    if (Alpha <= 0.0002f) continue; // no reason to render it, it is invisible

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
//  traspCmp
//
//==========================================================================
extern "C" {
  static int traspCmp (const void *a, const void *b, void */*udata*/) {
    if (a == b) return 0;
    const VRenderLevelShared::trans_sprite_t *ta = (const VRenderLevelShared::trans_sprite_t *)a;
    const VRenderLevelShared::trans_sprite_t *tb = (const VRenderLevelShared::trans_sprite_t *)b;

    /*
    {
      const float d0 = ta->dist;
      const float d1 = tb->dist;
      if (d0 < d1) return -1; // a is nearer, so it is last
      if (d0 > d1) return 1; // b is nearer, so it is last
    }
    */

    // first masked polys, then sprites, then alias models
    // type 0: masked polys
    // type 1: sprites
    // type 2: alias models
    //const int typediff = (int)ta->type-(int)tb->type;
    //if (typediff) return typediff;
    //if (ta->type < tb->type) return -1;
    //if (ta->type > tb->type) return 1;

    // non-translucent objects should come first, and
    // additive ones should come last
    bool didDistanceCheck = false;

    // additive
    if (ta->Additive && tb->Additive) {
      // both additive, sort by distance to view origin (order doesn't matter, as long as it is consistent)
      // but additive models should be sorted back-to-front, so use back-to-front, as with translucents
      const float d0 = ta->dist;
      const float d1 = tb->dist;
      if (d0 < d1) return 1;
      if (d0 > d1) return -1;
      // same distance, do other checks
      didDistanceCheck = true;
    } else if (ta->Additive) {
      // a is additive, b is not additive, so b should come first (a > b)
      return 1;
    } else if (tb->Additive) {
      // a is not additive, b is additive, so a should come first (a < b)
      return -1;
    }

    // translucent
    const bool aTrans = (!ta->Additive && ta->Alpha < 1.0f);
    const bool bTrans = (!tb->Additive && tb->Alpha < 1.0f);
    if (aTrans && bTrans) {
      // both translucent, sort by distance to view origin (nearest last)
      const float d0 = ta->dist;
      const float d1 = tb->dist;
      if (d0 < d1) return 1; // a is nearer, so it is last (a > b)
      if (d0 > d1) return -1; // b is nearer, so it is last (a < b)
      // same distance, do other checks
      didDistanceCheck = true;
    } else if (aTrans) {
      // a is translucent, b is not translucent; b first (a > b)
      return 1;
    } else if (bTrans) {
      // a is not translucent, b is translucent; a first (a < b)
      return -1;
    }

    // sort by object type
    // first masked polys, then sprites, then alias models
    // type 0: masked polys
    // type 1: sprites
    // type 2: alias models
    const int typediff = (int)ta->type-(int)tb->type;
    if (typediff) return typediff;

    // distance again
    if (!didDistanceCheck) {
      // do nearest first here, so z-buffer will do some culling for us
      //const float d0 = (ta->type == 1 ? ta->pdist : ta->dist);
      //const float d1 = (tb->type == 1 ? tb->pdist : tb->dist);
      const float d0 = ta->dist;
      const float d1 = tb->dist;
      if (d0 < d1) return -1; // a is nearest, so it is first (a < b)
      if (d0 > d1) return 1; // b is nearest, so it is first (a > b)
    }

    // priority check
    // higher priority comes first
    if (ta->prio < tb->prio) return 1; // a has lower priority, it should come last (a > b)
    if (ta->prio > tb->prio) return -1; // a has higher priority, it should come first (a < b)

    if (ta->objid < tb->objid) return -1;
    if (ta->objid > tb->objid) return 1;

    // sort sprites by lump number, why not
    if (ta->type == 1) {
      if (ta->lump < tb->lump) return -1;
      if (ta->lump > tb->lump) return 1;
    }

    // nothing to check anymore, consider equal
    return 0;
  }
}


//==========================================================================
//
//  VRenderLevelShared::DrawTranslucentPolys
//
//==========================================================================
void VRenderLevelShared::DrawTranslucentPolys () {
  if (traspUsed <= traspFirst) return; // nothing to do

  // sort 'em
  timsort_r(trans_sprites+traspFirst, traspUsed-traspFirst, sizeof(trans_sprites[0]), &traspCmp, nullptr);

#define MAX_POFS  (10)
  bool pofsEnabled = false;
  int pofs = 0;
  float lastpdist = -1e12f; // for sprites: use polyofs for the same dist
  bool firstSprite = true;

  // render 'em
  for (int f = traspFirst; f < traspUsed; ++f) {
    trans_sprite_t &spr = trans_sprites[f];
    if (spr.type == 2) {
      // alias model
      if (r_decals_enabled) Drawer->FinishMaskedDecals();
      if (pofsEnabled) { glDisable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(0, 0); pofsEnabled = false; }
      DrawEntityModel(spr.Ent, spr.light, spr.Fade, spr.Alpha, spr.Additive, spr.TimeFrac, RPASS_Normal);
    } else if (spr.type) {
      // sprite
      if (r_decals_enabled) Drawer->FinishMaskedDecals();
      if (r_sort_sprites && r_sprite_use_pofs && (firstSprite || lastpdist == spr.pdist)) {
        lastpdist = spr.pdist;
        if (!firstSprite) {
          if (!pofsEnabled) {
            // switch to next pofs
            //if (++pofs == MAX_POFS) pofs = 0;
            ++pofs;
            glEnable(GL_POLYGON_OFFSET_FILL);
            //glPolygonOffset(((float)pofs)/(float)MAX_POFS, -4);
            glPolygonOffset(r_sprite_pslope, -(pofs*r_sprite_pofs)); // pull forward
            pofsEnabled = true;
          }
        } else {
          firstSprite = false;
        }
      } else {
        lastpdist = spr.pdist;
        if (pofsEnabled) { glDisable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(0, 0); pofsEnabled = false; }
        // reset pofs
        pofs = 0;
      }
      /*
      GLint odf = GL_LEQUAL;
      glGetIntegerv(GL_DEPTH_FUNC, &odf);
      //glDepthFunc(((VOpenGLDrawer *)Drawer)->CanUseRevZ() ? GL_GREATER : GL_LESS);
      glDepthFunc(GL_GREATER);
      */
      //if (spr.noDepthChange) GCon->Logf("!!! %u", spr.objid);
      Drawer->DrawSpritePolygon(spr.Verts, GTextureManager[spr.lump],
                                spr.Alpha, spr.Additive, GetTranslation(spr.translation),
                                ColourMap, spr.light, spr.Fade, spr.normal, spr.pdist,
                                spr.saxis, spr.taxis, spr.texorg, spr.hangup);
      /*
      glDepthFunc(odf);
      */
    } else {
      // masked polygon
      check(spr.surf);
      if (pofsEnabled) { glDisable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(0, 0); pofsEnabled = false; }
      Drawer->DrawMaskedPolygon(spr.surf, spr.Alpha, spr.Additive);
    }
  }
#undef MAX_POFS

  if (r_decals_enabled) Drawer->FinishMaskedDecals();
  if (pofsEnabled) { glDisable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(0, 0); }

  // reset list
  traspUsed = traspFirst;
}


//==========================================================================
//
//  VRenderLevelShared::RenderPSprite
//
//==========================================================================
void VRenderLevelShared::RenderPSprite (VViewState *VSt, const VAliasModelFrameInfo mfi,
  float PSP_DIST, vuint32 light, vuint32 Fade, float Alpha, bool Additive)
{
  spritedef_t *sprdef;
  spriteframe_t *sprframe;
  int lump;
  bool flip;

  if (Alpha <= 0.0002f) return; // no reason to render it, it is invisible
  if (Alpha > 1.0f) Alpha = 1.0f;

  // decide which patch to use
  if ((vuint32)mfi.spriteIndex/*VSt->State->SpriteIndex*/ >= MAX_SPRITE_MODELS) {
#ifdef PARANOID
    GCon->Logf("R_ProjectSprite: invalid sprite number %d", /*VSt->State->SpriteIndex*/mfi.spriteIndex);
#endif
    return;
  }
  sprdef = &sprites[mfi.spriteIndex/*VSt->State->SpriteIndex*/];
  if (mfi.frame/*(VSt->State->Frame & VState::FF_FRAMEMASK)*/ >= sprdef->numframes) {
#ifdef PARANOID
    GCon->Logf("R_ProjectSprite: invalid sprite frame %d : %d", mfi.spriteIndex/*VSt->State->SpriteIndex*/, mfi.frame/*VSt->State->Frame*/);
#endif
    return;
  }
  sprframe = &sprdef->spriteframes[mfi.frame/*VSt->State->Frame & VState::FF_FRAMEMASK*/];

  lump = sprframe->lump[0];
  if (lump < 0) {
    //GCon->Logf("R_ProjectSprite: invalid sprite texture id %d in frame %d : %d", lump, mfi.spriteIndex/*VSt->State->SpriteIndex*/, mfi.frame/*VSt->State->Frame*/);
    return;
  }
  flip = sprframe->flip[0];
  VTexture *Tex = GTextureManager[lump];
  if (!Tex) {
    GCon->Logf("R_ProjectSprite: invalid sprite texture id %d in frame %d : %d (the thing that should not be)", lump, mfi.spriteIndex/*VSt->State->SpriteIndex*/, mfi.frame/*VSt->State->Frame*/);
    return;
  }

  int TexWidth = Tex->GetWidth();
  int TexHeight = Tex->GetHeight();
  int TexSOffset = Tex->SOffset;
  int TexTOffset = Tex->TOffset;

  TVec dv[4];

  float PSP_DISTI = 1.0f/PSP_DIST;
  TVec sprorigin = vieworg+PSP_DIST*viewforward;

  float sprx = 160.0f-VSt->SX+TexSOffset;
  float spry = 100.0f-VSt->SY*R_GetAspectRatio()+TexTOffset;

  spry -= cl->PSpriteSY;
  //k8: this is not right, but meh...
  if (fov > 90) spry -= (refdef.fovx-1.0f)*(aspect_ratio != 0 ? 100.0f : 110.0f);

  //  1 / 160 = 0.00625f
  TVec start = sprorigin-(sprx*PSP_DIST*0.00625f)*viewright;
  TVec end = start+(TexWidth*PSP_DIST*0.00625f)*viewright;

  //  1 / 160.0f * 120 / 100 = 0.0075f
  const float symul = 1.0f/160.0f*120.0f/100.0f;
  TVec topdelta = (spry*PSP_DIST*symul)*viewup;
  TVec botdelta = topdelta-(TexHeight*PSP_DIST*symul)*viewup;
  if (aspect_ratio != 1) {
    topdelta *= 100.0f/120.0f;
    botdelta *= 100.0f/120.0f;
  }

  dv[0] = start+botdelta;
  dv[1] = start+topdelta;
  dv[2] = end+topdelta;
  dv[3] = end+botdelta;

  TVec saxis(0, 0, 0);
  TVec taxis(0, 0, 0);
  TVec texorg(0, 0, 0);
  if (flip) {
    saxis = -(viewright*160*PSP_DISTI);
    texorg = dv[2];
  } else {
    saxis = viewright*160*PSP_DISTI;
    texorg = dv[1];
  }
       if (aspect_ratio == 0) taxis = -(viewup*160*PSP_DISTI);
  else if (aspect_ratio == 1) taxis = -(viewup*100*4/3*PSP_DISTI);
  else if (aspect_ratio == 2) taxis = -(viewup*100*16/9*PSP_DISTI);
  else if (aspect_ratio > 2) taxis = -(viewup*100*16/10*PSP_DISTI);

  Drawer->DrawSpritePolygon(dv, GTextureManager[lump], Alpha, Additive,
    0, ColourMap, light, Fade, -viewforward,
    DotProduct(dv[0], -viewforward), saxis, taxis, texorg, false);
}


//==========================================================================
//
//  VRenderLevelShared::RenderViewModel
//
//  FIXME: this doesn't work with "----" and "####" view states
//
//==========================================================================
bool VRenderLevelShared::RenderViewModel (VViewState *VSt, vuint32 light,
                                          vuint32 Fade, float Alpha, bool Additive)
{
  if (!r_view_models) return false;

  TVec origin = vieworg+(VSt->SX-1.0f)*viewright/8.0f-(VSt->SY-32.0f)*viewup/6.0f;

  float TimeFrac = 0;
  if (VSt->State->Time > 0) {
    TimeFrac = 1.0f-(VSt->StateTime/VSt->State->Time);
    TimeFrac = MID(0.0f, TimeFrac, 1.0f);
  }

  return DrawAliasModel(VSt->State->Outer->Name, origin, cl->ViewAngles, 1.0f, 1.0f,
    VSt->State->getMFI(), (VSt->State->NextState ? VSt->State->NextState->getMFI() : VSt->State->getMFI()),
    nullptr, 0, light, Fade, Alpha, Additive, true, TimeFrac, r_interpolate_frames,
    RPASS_Normal);
}


//==========================================================================
//
//  VRenderLevelShared::DrawPlayerSprites
//
//==========================================================================
void VRenderLevelShared::DrawPlayerSprites () {
  if (!r_draw_psprites || r_chasecam) return;

  int RendStyle = STYLE_Normal;
  float Alpha = 1.0f;
  bool Additive = false;

  cl->MO->eventGetViewEntRenderParams(Alpha, RendStyle);

  if (RendStyle == STYLE_SoulTrans) {
    RendStyle = STYLE_Translucent;
    Alpha = r_transsouls;
  } else if (RendStyle == STYLE_OptFuzzy) {
    RendStyle = r_drawfuzz ? STYLE_Fuzzy : STYLE_Translucent;
  }

  switch (RendStyle) {
    case STYLE_None: return;
    case STYLE_Normal: Alpha = 1.0f; break;
    case STYLE_Fuzzy: Alpha = FUZZY_ALPHA; break;
    case STYLE_Add: Additive = true; break;
    case STYLE_Stencil: break;
    case STYLE_AddStencil: Additive = true; break;
  }
  //Alpha = MID(0.0f, Alpha, 1.0f);
  if (Alpha <= 0.0002f) return; // no reason to render it, it is invisible
  if (Alpha > 1.0f) Alpha = 1.0f;

  // add all active psprites
  for (int i = 0; i < NUMPSPRITES; ++i) {
    if (!cl->ViewStates[i].State) continue;

    vuint32 light;
         if (RendStyle == STYLE_Fuzzy) light = 0;
    else if (cl->ViewStates[i].State->Frame&VState::FF_FULLBRIGHT) light = 0xffffffff;
    else light = LightPoint(vieworg, cl->MO->Radius);

    //FIXME: fake "solid color" with colored light for now
    if (RendStyle == STYLE_Stencil || RendStyle == STYLE_AddStencil) {
      light = (light&0xff000000)|(cl->MO->StencilColour&0xffffff);
    }

    vuint32 Fade = GetFade(SV_PointInRegion(r_viewleaf->sector, cl->ViewOrg));

    if (!RenderViewModel(&cl->ViewStates[i], light, Fade, Alpha, Additive)) {
      RenderPSprite(&cl->ViewStates[i], cl->getMFI(i), 3-i, light, Fade, Alpha, Additive);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::DrawCrosshair
//
//==========================================================================
void VRenderLevelShared::DrawCrosshair () {
  static int prevCH = -666;
  int ch = (int)crosshair;
  if (ch > 0 && ch < 10 && crosshair_alpha > 0.0f) {
    static int handle = 0;
    if (!handle || prevCH != ch) {
      prevCH = ch;
      handle = GTextureManager.AddPatch(VName(va("CROSHAI%i", ch), VName::AddLower8), TEXTYPE_Pic);
      if (handle < 0) handle = 0;
    }
    if (handle > 0) {
      //if (crosshair_alpha < 0.0f) crosshair_alpha = 0.0f;
      if (crosshair_alpha > 1.0f) crosshair_alpha = 1.0f;
      int cy = (screenblocks < 11 ? (VirtualHeight-sb_height)/2 : VirtualHeight/2);
      cy += r_crosshair_yofs;
      bool oldflt = gl_pic_filtering;
      gl_pic_filtering = false;
      R_DrawPic(VirtualWidth/2, cy, handle, crosshair_alpha);
      gl_pic_filtering = oldflt;
    }
  }
}


//==========================================================================
//
//  R_DrawSpritePatch
//
//==========================================================================
void R_DrawSpritePatch (int x, int y, int sprite, int frame, int rot,
                        int TranslStart, int TranslEnd, int Colour)
{
  bool flip;
  int lump;

  spriteframe_t *sprframe = &sprites[sprite].spriteframes[frame&VState::FF_FRAMEMASK];
  flip = sprframe->flip[rot];
  lump = sprframe->lump[rot];
  VTexture *Tex = GTextureManager[lump];
  if (!Tex) return; // just in case

  Tex->GetWidth();

  float x1 = x-Tex->SOffset;
  float y1 = y-Tex->TOffset;
  float x2 = x1+Tex->GetWidth();
  float y2 = y1+Tex->GetHeight();

  x1 *= fScaleX;
  y1 *= fScaleY;
  x2 *= fScaleX;
  y2 *= fScaleY;

  Drawer->DrawSpriteLump(x1, y1, x2, y2, Tex, R_GetCachedTranslation(R_SetMenuPlayerTrans(TranslStart, TranslEnd, Colour), nullptr), flip);
}
