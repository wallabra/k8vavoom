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
//** masked polys and sprites queue
//**
//**************************************************************************
#include "gamedefs.h"
#include "r_local.h"
#include "sv_local.h"


enum {
  SPR_VP_PARALLEL_UPRIGHT, // 0 (default)
  SPR_FACING_UPRIGHT, // 1
  SPR_VP_PARALLEL, // 2: parallel to camera visplane
  SPR_ORIENTED, // 3
  SPR_VP_PARALLEL_ORIENTED, // 4 (xy billboard)
  SPR_VP_PARALLEL_UPRIGHT_ORIENTED, // 5
  SPR_ORIENTED_OFS, // 6 (offset slightly by pitch -- for floor/ceiling splats)
};


extern VCvarB r_chasecam;
extern VCvarB r_sort_sprites;
extern VCvarB r_brightmaps;
extern VCvarB r_brightmaps_sprite;

static VCvarB r_fix_sprite_offsets("r_fix_sprite_offsets", true, "Fix sprite offsets?", CVAR_Archive);
static VCvarB r_fix_sprite_offsets_missiles("r_fix_sprite_offsets_missiles", false, "Fix sprite offsets for projectiles?", CVAR_Archive);
static VCvarI r_sprite_fix_delta("r_sprite_fix_delta", "-7", "Sprite offset amount.", CVAR_Archive); // -6 seems to be ok for vanilla BFG explosion, and for imp fireball
static VCvarB r_use_real_sprite_offset("r_use_real_sprite_offset", true, "Use real picture height instead of texture height for sprite offset fixes?", CVAR_Archive);

static VCvarB r_sprite_use_pofs("r_sprite_use_pofs", true, "Use PolygonOffset with sprite sorting to reduce sprite flickering?", CVAR_Archive);
static VCvarF r_sprite_pofs("r_sprite_pofs", "128", "DEBUG");
static VCvarF r_sprite_pslope("r_sprite_pslope", "-1.0", "DEBUG");

static VCvarB r_thing_hiframe_use_camera_plane("r_thing_hiframe_use_camera_plane", true, "Use angle to camera plane to select rotation for sprites with detailed rotations?", CVAR_Archive);
static VCvarB r_thing_monster_use_camera_plane("r_thing_monster_use_camera_plane", true, "Use angle to camera plane to select monster rotation?", CVAR_Archive);
static VCvarB r_thing_missile_use_camera_plane("r_thing_missile_use_camera_plane", true, "Use angle to camera plane to select missile rotation?", CVAR_Archive);
static VCvarB r_thing_other_use_camera_plane("r_thing_other_use_camera_plane", true, "Use angle to camera plane to select non-monster rotation?", CVAR_Archive);

VCvarB r_fake_shadows_alias_models("r_fake_shadows_alias_models", false, "Render shadows from alias models (based on sprite frame)?", CVAR_Archive);
static VCvarB r_fake_sprite_shadows("r_fake_sprite_shadows", true, "Render fake sprite shadows?", CVAR_Archive);
static VCvarF r_fake_shadow_translucency("r_fake_shadow_translucency", "0.5", "Fake sprite shadows translucency multiplier.", CVAR_Archive);
static VCvarF r_fake_shadow_scale("r_fake_shadow_scale", "0.1", "Fake sprite shadows height multiplier.", CVAR_Archive);
static VCvarB r_fake_shadow_ignore_offset_fix("r_fake_shadow_ignore_offset_fix", false, "Should fake sprite shadows ignore sprite offset fix?", CVAR_Archive);

static VCvarB r_fake_shadows_monsters("r_fake_shadows_monsters", true, "Render fake sprite shadows for monsters?", CVAR_Archive);
static VCvarB r_fake_shadows_corpses("r_fake_shadows_corpses", true, "Render fake sprite shadows for corpses?", CVAR_Archive);
static VCvarB r_fake_shadows_missiles("r_fake_shadows_missiles", true, "Render fake sprite shadows for projectiles?", CVAR_Archive);
static VCvarB r_fake_shadows_pickups("r_fake_shadows_pickups", true, "Render fake sprite shadows for pickups?", CVAR_Archive);
static VCvarB r_fake_shadows_decorations("r_fake_shadows_decorations", true, "Render fake sprite shadows for decorations?", CVAR_Archive);
static VCvarB r_fake_shadows_players("r_fake_shadows_players", true, "Render fake sprite shadows for players?", CVAR_Archive);

static VCvarB r_fake_shadows_additive_missiles("r_fake_shadows_additive_missiles", true, "Render shadows from additive projectiles?", CVAR_Archive);
static VCvarB r_fake_shadows_additive_monsters("r_fake_shadows_additive_monsters", true, "Render shadows from additive monsters?", CVAR_Archive);

static VCvarB dbg_disable_sprite_sorting("dbg_disable_sprite_sorting", false, "Disable sprite sorting (this WILL glitch renderer)?", /*CVAR_Archive|*/CVAR_PreInit);


// ////////////////////////////////////////////////////////////////////////// //
extern "C" {
  static inline __attribute__((unused)) int compareSurfacesByTexture (const surface_t *sa, const surface_t *sb) {
    if (sa == sb) return 0;
    const texinfo_t *ta = sa->texinfo;
    const texinfo_t *tb = sb->texinfo;
    if ((uintptr_t)ta->Tex < (uintptr_t)ta->Tex) return -1;
    if ((uintptr_t)tb->Tex > (uintptr_t)tb->Tex) return 1;
    return ((int)ta->ColorMap)-((int)tb->ColorMap);
  }

  static __attribute__((unused)) int drawListItemCmpByTexture (const void *a, const void *b, void *udata) {
    return compareSurfacesByTexture(*(const surface_t **)a, *(const surface_t **)b);
  }
}


//==========================================================================
//
//  VRenderLevelShared::QueueTranslucentPoly
//
//  hangup:
//    0: normal
//   -1: no z-buffer write, slightly offset (used for flat-aligned sprites)
//  666: fake sprite shadow
//
//==========================================================================
void VRenderLevelShared::QueueTranslucentPoly (surface_t *surf, TVec *sv,
  int count, int lump, float Alpha, bool Additive, int translation,
  bool isSprite, vuint32 light, vuint32 Fade, const TVec &normal, float pdist,
  const TVec &saxis, const TVec &taxis, const TVec &texorg, int priority,
  bool useSprOrigin, const TVec &sprOrigin, vuint32 objid, int hangup)
{
  vassert(count >= 0);
  if (count == 0 || Alpha < 0.01f) return;

  // make room
  /*
  if (traspUsed == traspSize) {
    if (traspSize >= 0xfffffff) Sys_Error("Too many translucent entities");
    traspSize += 0x10000;
    trans_sprites = (trans_sprite_t *)Z_Realloc(trans_sprites, traspSize*sizeof(trans_sprites[0]));
  }
  */

  float dist;
  if (useSprOrigin) {
    TVec mid = sprOrigin;
    //dist = fabsf(DotProduct(mid-vieworg, viewforward));
    dist = LengthSquared(mid-vieworg);
  } else {
#if 0
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

  //trans_sprite_t &spr = trans_sprites[traspUsed++];
  trans_sprite_t &spr = GetCurrentDLS().DrawSpriteList.alloc();
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
//  VRenderLevelShared::QueueTranslucentAliasModel
//
//==========================================================================
void VRenderLevelShared::QueueTranslucentAliasModel (VEntity *mobj, vuint32 light, vuint32 Fade, float Alpha, bool Additive, float TimeFrac) {
  if (!mobj) return; // just in case

  // make room
  /*
  if (traspUsed == traspSize) {
    if (traspSize >= 0xfffffff) Sys_Error("Too many translucent entities");
    traspSize += 0x10000;
    trans_sprites = (trans_sprite_t *)Z_Realloc(trans_sprites, traspSize*sizeof(trans_sprites[0]));
  }
  */

  //const float dist = fabsf(DotProduct(mobj->Origin-vieworg, viewforward));
  const float dist = LengthSquared(mobj->Origin-vieworg);

  //trans_sprite_t &spr = trans_sprites[traspUsed++];
  trans_sprite_t &spr = GetCurrentDLS().DrawSpriteList.alloc();
  spr.Ent = mobj;
  spr.light = light;
  spr.Fade = Fade;
  spr.Alpha = Alpha;
  spr.Additive = Additive;
  spr.dist = dist;
  spr.type = 2;
  spr.TimeFrac = TimeFrac;
  spr.lump = -1; // has no sense
  spr.objid = mobj->GetUniqueId();
  spr.prio = 0; // normal priority
  spr.hangup = 0;
}


//==========================================================================
//
//  VRenderLevelShared::QueueSprite
//
//==========================================================================
void VRenderLevelShared::QueueSprite (VEntity *thing, vuint32 light, vuint32 Fade, float Alpha, bool Additive,
                                      vuint32 seclight, bool onlyShadow) {
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
  bool renderShadow =
    /*!Additive &&*/ thing && spr_type == SPR_VP_PARALLEL_UPRIGHT &&
    r_fake_sprite_shadows.asBool() &&
    r_sort_sprites.asBool() &&
    (r_fake_shadow_scale.asFloat() > 0.0f);

  if (onlyShadow && !renderShadow) return;

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
  if ((unsigned)SpriteIndex >= (unsigned)sprites.length()) {
    #ifdef PARANOID
    GCon->Logf(NAME_Dev, "Invalid sprite number %d", SpriteIndex);
    #endif
    return;
  }

  sprdef = &sprites.ptr()[SpriteIndex];
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

  float scaleX = max2(0.001f, thing->ScaleX);
  float scaleY = max2(0.001f, thing->ScaleY);

  if (renderShadow) {
    if (thing == ViewEnt && (!r_chasecam || ViewEnt != cl->MO)) {
      // don't draw camera actor shadow (just in case, it should not come here anyway)
      renderShadow = false;
    }
    if (renderShadow && Additive) {
      if (thing->IsMissile()) {
        renderShadow = (r_fake_shadows_missiles.asBool() && r_fake_shadows_additive_missiles.asBool());
      } else if (thing->IsMonster()) {
        renderShadow = (r_fake_shadows_monsters.asBool() && r_fake_shadows_additive_monsters.asBool());
      } else {
        renderShadow = false;
      }
    }
  }

  // only for monsters
  if (renderShadow) {
           if (thing->IsPlayer()) { renderShadow = r_fake_shadows_players.asBool();
    } else if (thing->IsMissile()) { renderShadow = r_fake_shadows_missiles.asBool();
    } else if (thing->IsCorpse()) { renderShadow = r_fake_shadows_corpses.asBool();
    } else if (thing->IsMonster()) { renderShadow = r_fake_shadows_monsters.asBool();
    } else if (thing->IsSolid()) { renderShadow = r_fake_shadows_decorations.asBool();
    } else if (r_fake_shadows_pickups.asBool()) {
      // check for pickup
      // inventory class
      static VClass *invCls = nullptr;
      static bool invClsInited = false;
      if (!invClsInited) {
        invClsInited = true;
        invCls = VMemberBase::StaticFindClass("Inventory");
      }
      // random spawner class
      /* no, it should never end up on spawned by itself
      static VClass *rspCls = nullptr;
      static bool rspClsInited = false;
      if (!rspClsInited) {
        rspClsInited = true;
        rspCls = VMemberBase::StaticFindClass("RandomSpawner");
      }
      renderShadow =
        (invCls && thing->IsA(invCls)) ||
        (rspCls && thing->IsA(rspCls));
      */
      renderShadow = (invCls && thing->IsA(invCls));
    } else {
      renderShadow = false;
    }
  }

  if (onlyShadow && !renderShadow) return;

  TVec sv[4];

  TVec start = -TexSOffset*sprright*scaleX;
  TVec end = (TexWidth-TexSOffset)*sprright*scaleX;

  const int origTOffset = TexTOffset;
  //if (thing) GCon->Logf(NAME_Debug, "*** CLASS '%s': scaleY=%g; TOfs=%d; hgt=%d; dofix=%d", thing->GetClass()->GetName(), scaleY, TexTOffset, TexHeight, (TexTOffset < TexHeight && 2*TexTOffset+r_sprite_fix_delta >= TexHeight ? 1 : 0));
  //k8: scale is arbitrary here
  //    also, don't bother with projectiles, they're usually flying anyway
  bool doFixSpriteOffset = r_fix_sprite_offsets;
  if (doFixSpriteOffset && !r_fix_sprite_offsets_missiles && thing->IsMissile()) doFixSpriteOffset = false;
  if (doFixSpriteOffset) {
    const int sph = (r_use_real_sprite_offset ? Tex->GetRealHeight() : TexHeight);
    //if (thing) GCon->Logf(NAME_Debug, "THING '%s': sph=%d; height=%d", thing->GetClass()->GetName(), sph, TexHeight);
    if (TexTOffset < /*TexHeight*/sph && 2*TexTOffset+r_sprite_fix_delta >= /*TexHeight*/sph && scaleY > 0.6f && scaleY < 1.6f) TexTOffset = /*TexHeight*/sph;
  }

  TVec topdelta = TexTOffset*sprup*scaleY;
  TVec botdelta = (TexTOffset-TexHeight)*sprup*scaleY;

  sv[0] = sprorigin+start+botdelta;
  sv[1] = sprorigin+start+topdelta;
  sv[2] = sprorigin+end+topdelta;
  sv[3] = sprorigin+end+botdelta;

  //if (Fade != FADE_LIGHT) GCon->Logf("<%s>: Fade=0x%08x", *thing->GetClass()->GetFullName(), Fade);

  if (Alpha >= 1.0f && !Additive && Tex->isTranslucent()) Alpha = 0.9999f;

  if (Alpha < 1.0f || Additive || r_sort_sprites) {
    // add sprite
    int priority = 0;
    if (thing) {
           if (thing->EntityFlags&VEntity::EF_Bright) priority = 200;
      else if (thing->EntityFlags&VEntity::EF_FullBright) priority = 100;
      else if (thing->EntityFlags&(VEntity::EF_Corpse|VEntity::EF_Blasted)) priority = -120;
      else if (thing->Health <= 0) priority = -110;
      else if (thing->EntityFlags&VEntity::EF_NoBlockmap) priority = -200;
    }
    if (!onlyShadow) {
      QueueTranslucentPoly(nullptr, sv, 4, lump, Alpha+(thing->RenderStyle == STYLE_Dark ? 1666.0f : 0.0f), Additive,
        thing->Translation, true/*isSprite*/, light, Fade, -sprforward,
        DotProduct(sprorigin, -sprforward), (flip ? -sprright : sprright)/scaleX,
        -sprup/scaleY, (flip ? sv[2] : sv[1]), priority
        , true, /*sprorigin*/thing->Origin, thing->GetUniqueId(), hangup);
    }
    // add shadow
    if (renderShadow) {
      Alpha *= r_fake_shadow_translucency.asFloat();
      if (Alpha >= 0.012f) {
        // check origin
        if (sprorigin.z >= thing->FloorZ) {
          sprorigin.z = thing->FloorZ;

          Alpha = min2(1.0f, Alpha);
          scaleY *= r_fake_shadow_scale.asFloat();

          //start = -TexSOffset*sprright*scaleX;
          end = (TexWidth-TexSOffset)*sprright*scaleX;

          // undo sprite offset
          if (r_fake_shadow_ignore_offset_fix) TexTOffset = origTOffset;

          topdelta = TexTOffset*sprup*scaleY;
          botdelta = (TexTOffset-TexHeight)*sprup*scaleY;

          sv[0] = sprorigin+start+botdelta;
          sv[1] = sprorigin+start+topdelta;
          sv[2] = sprorigin+end+topdelta;
          sv[3] = sprorigin+end+botdelta;

          QueueTranslucentPoly(nullptr, sv, 4, lump, Alpha, /*Additive*/false,
            /*thing->Translation*/0, true/*isSprite*/, light, Fade, -sprforward,
            DotProduct(sprorigin, -sprforward), (flip ? -sprright : sprright)/scaleX,
            -sprup/scaleY, (flip ? sv[2] : sv[1]), priority
            , true, /*sprorigin*/thing->Origin, thing->GetUniqueId(), 666/*fakeshadow type*/);
        }
      }
    }
  } else {
    Drawer->DrawSpritePolygon(sv, /*GTextureManager[lump]*/Tex, Alpha+(thing->RenderStyle == STYLE_Dark ? 1666.0f : 0.0f),
      Additive, GetTranslation(thing->Translation), ColorMap, light,
      Fade, -sprforward, DotProduct(sprorigin, -sprforward),
      (flip ? -sprright : sprright)/scaleX,
      -sprup/scaleY, (flip ? sv[2] : sv[1]), hangup);
  }
}


//==========================================================================
//
//  traspCmp
//
//==========================================================================
extern "C" {
  static int traspCmp (const void *a, const void *b, void *) {
    if (a == b) return 0;
    const VRenderLevelShared::trans_sprite_t *ta = (const VRenderLevelShared::trans_sprite_t *)a;
    const VRenderLevelShared::trans_sprite_t *tb = (const VRenderLevelShared::trans_sprite_t *)b;

    const int tahang = ta->hangup;
    const int tbhang = tb->hangup;

    // "hangup" sprites comes before other sprites
    if (tahang == -1 || tbhang == -1) {
      if (tahang == -1 && tbhang == -1) {
        // do normal checks here
      } else if (tahang == -1) {
        // a is hangup, b is not, (a < b)
        return -1;
      } else {
        // a is not hangup, b is hangup, (a > b)
        return 1;
      }
    }

    // fake sprite shadows comes first
    if (tahang == 666 || tbhang == 666) {
      if (tahang == 666 && tbhang == 666) {
        // do normal checks here
      } else if (tahang == 666) {
        // a is shadow, b is not, (a < b)
        return -1;
      } else {
        // a is not shadow, b is shadow, (a > b)
        return 1;
      }
    }

    // non-translucent objects should come first
    bool didDistanceCheck = false;

    // translucent/additive
    const bool aTrans = (ta->Additive || ta->Alpha < 1.0f);
    const bool bTrans = (tb->Additive || tb->Alpha < 1.0f);

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
  VRenderLevelDrawer::DrawLists &dls = GetCurrentDLS();

  //FIXME: we should use proper sort order instead, because with separate lists additive sprites
  //       are broken by translucent surfaces (additive sprite rendered first even if it is nearer
  //       than the surface)

  if (dls.DrawSpriteList.length() > 0) {
    //GCon->Logf("DrawTranslucentPolys: first=%u; used=%u; count=%u", traspFirst, traspUsed, traspUsed-traspFirst);

    // sort 'em
    if (!dbg_disable_sprite_sorting) {
      timsort_r(dls.DrawSpriteList.ptr(), dls.DrawSpriteList.length(), sizeof(dls.DrawSpriteList[0]), &traspCmp, nullptr);
    }

#define MAX_POFS  (10)
    bool pofsEnabled = false;
    int pofs = 0;
    float lastpdist = -1e12f; // for sprites: use polyofs for the same dist
    bool firstSprite = true;

    // other
    for (auto &&spr : dls.DrawSpriteList) {
      //GCon->Logf("  #%d: type=%d; alpha=%g; additive=%d; light=0x%08x; fade=0x%08x", f, spr.type, spr.Alpha, (int)spr.Additive, spr.light, spr.Fade);
      if (spr.type == 2) {
        // alias model
        if (pofsEnabled) { glDisable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(0, 0); pofsEnabled = false; }
        DrawEntityModel(spr.Ent, spr.light, spr.Fade, spr.Alpha, spr.Additive, spr.TimeFrac, RPASS_Normal);
      } else if (spr.type) {
        // sprite
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
        Drawer->DrawSpritePolygon(spr.Verts, GTextureManager[spr.lump],
                                  spr.Alpha, spr.Additive, GetTranslation(spr.translation),
                                  ColorMap, spr.light, spr.Fade, spr.normal, spr.pdist,
                                  spr.saxis, spr.taxis, spr.texorg, spr.hangup);
      } else {
        // masked polygon
        // non-translucent and non-additive polys should not end up here
        vassert(spr.surf);
        if (pofsEnabled) { glDisable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(0, 0); pofsEnabled = false; }
        Drawer->DrawMaskedPolygon(spr.surf, spr.Alpha, spr.Additive);
      }
    }
#undef MAX_POFS

    if (pofsEnabled) { glDisable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(0, 0); }
  }

  //GCon->Logf(NAME_Debug, "add=%d; alp=%d", DrawSurfListAdditive.length(), DrawSurfListAlpha.length());
  // additive (order doesn't matter, so sort by texture)
  if (GetCurrentDLS().DrawSurfListAdditive.length() != 0) {
    //timsort_r(DrawSurfListAdditive.ptr(), DrawSurfListAdditive.length(), sizeof(surface_t *), &drawListItemCmpByTexture, nullptr);
    // back-to-front
    for (int f = GetCurrentDLS().DrawSurfListAdditive.length()-1; f >= 0; --f) {
      surface_t *sfc = GetCurrentDLS().DrawSurfListAdditive[f];
      Drawer->DrawMaskedPolygon(sfc, sfc->texinfo->Alpha, /*sfc->texinfo->Additive*/true);
    }
  }

  // translucent (order does matter, no sorting)
  if (GetCurrentDLS().DrawSurfListAlpha.length() != 0) {
    // back-to-front
    for (int f = GetCurrentDLS().DrawSurfListAlpha.length()-1; f >= 0; --f) {
      surface_t *sfc = GetCurrentDLS().DrawSurfListAlpha[f];
      Drawer->DrawMaskedPolygon(sfc, sfc->texinfo->Alpha, /*sfc->texinfo->Additive*/false);
    }
  }
}
