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
//**  Copyright (C) 2018-2021 Ketmar Dark
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
#include "../gamedefs.h"
#include "r_local.h"


extern VCvarB r_chasecam;
extern VCvarB r_brightmaps;
extern VCvarB r_brightmaps_sprite;
extern VCvarI r_shadowmap_sprshadows;
extern VCvarI gl_release_ram_textures_mode;
extern VCvarB gl_crop_sprites;

VCvarI r_fix_sprite_offsets("r_fix_sprite_offsets", "2", "Sprite offset fixing algorithm (0:don't fix; 1:old; 2:new).", CVAR_Archive);
VCvarB r_fix_sprite_offsets_missiles("r_fix_sprite_offsets_missiles", false, "Fix sprite offsets for projectiles?", CVAR_Archive);
VCvarB r_fix_sprite_offsets_smart_corpses("r_fix_sprite_offsets_smart_corpses", true, "Let corpses sink a little?", CVAR_Archive);
VCvarI r_sprite_fix_delta("r_sprite_fix_delta", "-7", "Sprite offset amount.", CVAR_Archive); // -6 seems to be ok for vanilla BFG explosion, and for imp fireball
VCvarB r_use_real_sprite_offset("r_use_real_sprite_offset", true, "Use real picture height instead of texture height for sprite offset fixes (only for old aglorithm)?", CVAR_Archive);
VCvarB r_use_sprofs_lump("r_use_sprofs_lump", true, "Use 'sprofs' lump for some hard-coded sprite offsets (only for the new algorithm)?", CVAR_Archive);

static VCvarB r_sprite_use_pofs("r_sprite_use_pofs", false, "Use PolygonOffset with sprite sorting to reduce sprite flickering?", CVAR_Archive);
static VCvarF r_sprite_pofs("r_sprite_pofs", "128", "DEBUG");
static VCvarF r_sprite_pslope("r_sprite_pslope", "-1.0", "DEBUG");

static VCvarB r_thing_hiframe_use_camera_plane("r_thing_hiframe_use_camera_plane", true, "Use angle to camera plane to select rotation for sprites with detailed rotations?", CVAR_Archive);
static VCvarB r_thing_monster_use_camera_plane("r_thing_monster_use_camera_plane", true, "Use angle to camera plane to select monster rotation?", CVAR_Archive);
static VCvarB r_thing_missile_use_camera_plane("r_thing_missile_use_camera_plane", true, "Use angle to camera plane to select missile rotation?", CVAR_Archive);
static VCvarB r_thing_other_use_camera_plane("r_thing_other_use_camera_plane", true, "Use angle to camera plane to select non-monster rotation?", CVAR_Archive);

VCvarB r_fake_shadows_alias_models("r_fake_shadows_alias_models", false, "Render shadows from alias models (based on sprite frame)?", CVAR_Archive);
static VCvarI r_fake_sprite_shadows("r_fake_sprite_shadows", "2", "Render fake sprite shadows (0:no; 1:2d; 2:pseudo-3d)?", CVAR_Archive);
static VCvarF r_fake_shadow_translucency("r_fake_shadow_translucency", "0.5", "Fake sprite shadows translucency multiplier.", CVAR_Archive);
static VCvarF r_fake_shadow_scale("r_fake_shadow_scale", "0.1", "Fake sprite shadows height multiplier.", CVAR_Archive);
static VCvarB r_fake_shadow_ignore_offset_fix("r_fake_shadow_ignore_offset_fix", false, "Should fake sprite shadows ignore sprite offset fix?", CVAR_Archive);

static VCvarB r_fake_shadows_monsters("r_fake_shadows_monsters", true, "Render fake sprite shadows for monsters?", CVAR_Archive);
static VCvarB r_fake_shadows_corpses("r_fake_shadows_corpses", false, "Render fake sprite shadows for corpses?", CVAR_Archive);
static VCvarB r_fake_shadows_missiles("r_fake_shadows_missiles", true, "Render fake sprite shadows for projectiles?", CVAR_Archive);
static VCvarB r_fake_shadows_pickups("r_fake_shadows_pickups", true, "Render fake sprite shadows for pickups?", CVAR_Archive);
static VCvarB r_fake_shadows_decorations("r_fake_shadows_decorations", true, "Render fake sprite shadows for decorations?", CVAR_Archive);
static VCvarB r_fake_shadows_players("r_fake_shadows_players", true, "Render fake sprite shadows for players?", CVAR_Archive);

static VCvarB r_fake_shadows_additive_missiles("r_fake_shadows_additive_missiles", false, "Render shadows from additive projectiles?", CVAR_Archive);
static VCvarB r_fake_shadows_additive_monsters("r_fake_shadows_additive_monsters", false, "Render shadows from additive monsters?", CVAR_Archive);

static VCvarF r_fake_3dshadow_scale("r_fake_3dshadow_scale", "0.4", "Fake sprite shadows height multiplier for pseudo-3d mode.", CVAR_Archive);
static VCvarI r_fake_3dshadow_mode("r_fake_3dshadow_mode", "0", "Fake pseudo-3d shadow mode (0:direction to the thing; 1:camera yaw).", CVAR_Archive);

static VCvarB dbg_disable_sprite_sorting("dbg_disable_sprite_sorting", false, "Disable sprite sorting (this WILL glitch renderer)?", /*CVAR_Archive|*/CVAR_PreInit);

static VCvarB dbg_disable_translucent_polys("dbg_disable_translucent_polys", false, "Disable rendering of translucent polygons?", CVAR_PreInit);


// ////////////////////////////////////////////////////////////////////////// //
extern "C" {
  static inline VVA_OKUNUSED int compareSurfacesByTexture (const surface_t *sa, const surface_t *sb) {
    if (sa == sb) return 0;
    const texinfo_t *ta = sa->texinfo;
    const texinfo_t *tb = sb->texinfo;
    if ((uintptr_t)ta->Tex < (uintptr_t)ta->Tex) return -1;
    if ((uintptr_t)tb->Tex > (uintptr_t)tb->Tex) return 1;
    return ((int)ta->ColorMap)-((int)tb->ColorMap);
  }

  static VVA_OKUNUSED int drawListItemCmpByTexture (const void *a, const void *b, void *udata) {
    return compareSurfacesByTexture(*(const surface_t **)a, *(const surface_t **)b);
  }
}


//==========================================================================
//
//  R_GetSpriteTextureInfo
//
//  returns `false` if no texture found
//
//==========================================================================
bool R_GetSpriteTextureInfo (SpriteTexInfo *nfo, int sprindex, int sprframeidx) {
  // decide which patch to use for sprite relative to player
  if ((unsigned)sprindex >= (unsigned)sprites.length()) {
    if (nfo) memset((void *)nfo, 0, sizeof(*nfo));
    return false;
  }

  const spritedef_t *sprdef = &sprites.ptr()[sprindex];
  if (sprframeidx >= sprdef->numframes) {
    if (nfo) memset((void *)nfo, 0, sizeof(*nfo));
    return false;
  }

  const spriteframe_t *sprframe = &sprdef->spriteframes[sprframeidx];

  // use single rotation for all views
  if (nfo) {
    nfo->texid = sprframe->lump[0];
    nfo->flags = (sprframe->flip[0] ? SpriteTexInfo::Flipped : 0u);
  }

  return true;
}


//==========================================================================
//
//  VRenderLevelShared::QueueTranslucentSurface
//
//==========================================================================
void VRenderLevelShared::QueueTranslucentSurface (surface_t *surf, const RenderStyleInfo &ri) {
  if (!surf || surf->count < 3 || ri.alpha < 0.004f) return;
  const SurfVertex *sv = surf->verts;
  const int count = surf->count;

#if 0
  TVec mid(0, 0, 0);
  for (int i = 0; i < count; ++i) mid += sv[i].vec();
  mid /= count;
  //const float dist = fabsf(DotProduct(mid-Drawer->vieworg, Drawer->viewforward));
  const float dist = LengthSquared(mid-Drawer->vieworg);
#else
  // select nearest vertex
  //float dist = LengthSquared(sv[0].vec()-Drawer->vieworg);
  //const TClipPlane *viewplane = &Drawer->viewfrustum.planes[TFrustum::Near];
  //float dist = viewplane->PointDistance(sv[0].vec());
  const TVec vfwd = Drawer->viewforward;
  const TVec vorg = Drawer->vieworg;
  float dist = /*fabsf*/(DotProduct(sv[0].vec()-vorg, vfwd));
  for (int i = 1; i < count; ++i) {
    //const float nd = LengthSquared(sv[i].vec()-Drawer->vieworg);
    //const float nd = viewplane->PointDistance(sv[i].vec());
    const float nd = /*fabsf*/(DotProduct(sv[i].vec()-vorg, vfwd));
    if (dist > nd) dist = nd;
  }
#endif

  trans_sprite_t *spr = AllocTransSpr(ri);
  spr->dist = dist;
  spr->surf = surf;
  spr->type = TSP_Wall;
  //spr->origin = sprOrigin;
  spr->rstyle = ri;
  // used in sorter
  if (spr->surf->drawflags&surface_t::DF_MIRROR) {
    // disable depth write too, just in case
    spr->rstyle.flags |= RenderStyleInfo::FlagMirror/*|RenderStyleInfo::FlagNoDepthWrite*/;
  }
  // set ceiling/floor flags
  const float ssz = spr->surf->plane.normal.z;
  if (ssz != 0.0f) {
    // disable depth write too, just in case
    spr->rstyle.flags |= (ssz < 0.0f ? RenderStyleInfo::FlagCeiling : RenderStyleInfo::FlagFloor)|RenderStyleInfo::FlagNoDepthWrite;
  }
}


//==========================================================================
//
//  VRenderLevelShared::QueueSpritePoly
//
//==========================================================================
void VRenderLevelShared::QueueSpritePoly (const TVec *sv, int lump, const RenderStyleInfo &ri, int translation,
                                          const TVec &normal, float pdist, const TVec &saxis, const TVec &taxis,
                                          const TVec &texorg, int priority, const TVec &sprOrigin, vuint32 objid)
{
  if (ri.alpha < 0.004f) return;

  const float dist = /*fabsf*/(DotProduct(sprOrigin-Drawer->vieworg, Drawer->viewforward));
  //const float dist = LengthSquared(sprOrigin-Drawer->vieworg);

  //trans_sprite_t &spr = GetCurrentDLS().DrawSpriteList.alloc();
  trans_sprite_t *spr = AllocTransSpr(ri);
  memcpy(spr->Verts, sv, sizeof(TVec)*4);
  spr->dist = dist;
  spr->lump = lump;
  spr->normal = normal;
  spr->pdist = pdist;
  spr->saxis = saxis;
  spr->taxis = taxis;
  spr->texorg = texorg;
  spr->surf = nullptr;
  spr->translation = translation;
  spr->type = TSP_Sprite;
  spr->objid = objid;
  spr->prio = priority;
  //spr->origin = sprOrigin;
  spr->rstyle = ri;
}


//==========================================================================
//
//  VRenderLevelShared::QueueTranslucentAliasModel
//
//==========================================================================
void VRenderLevelShared::QueueTranslucentAliasModel (VEntity *mobj, const RenderStyleInfo &ri/*vuint32 light, vuint32 Fade, float Alpha, bool Additive*/, float TimeFrac) {
  if (!mobj) return; // just in case
  if (ri.alpha < 0.004f) return;
  if (ri.flags&RenderStyleInfo::FlagShadow) return;

  const float dist = /*fabsf*/(DotProduct(mobj->Origin-Drawer->vieworg, Drawer->viewforward));
  //const float dist = LengthSquared(mobj->Origin-Drawer->vieworg);

  //trans_sprite_t &spr = trans_sprites[traspUsed++];
  //trans_sprite_t &spr = GetCurrentDLS().DrawSpriteList.alloc();
  trans_sprite_t *spr = AllocTransSpr(ri);
  spr->Ent = mobj;
  spr->rstyle = ri;
  spr->dist = dist;
  spr->type = TSP_Model;
  spr->TimeFrac = TimeFrac;
  spr->lump = -1; // has no sense
  spr->objid = mobj->ServerUId;
  spr->prio = 0; // normal priority
  //spr->origin = mobj->Origin;
}


//==========================================================================
//
//  VRenderLevelShared::FixSpriteOffset
//
//==========================================================================
void VRenderLevelShared::FixSpriteOffset (int fixAlgo, VEntity *thing, VTexture *Tex, const int TexHeight, const float scaleY, int &TexTOffset, int &origTOffset) {
  origTOffset = TexTOffset = (fixAlgo > 1 && Tex->bForcedSpriteOffset && r_use_sprofs_lump ? Tex->TOffsetFix : Tex->TOffset);
  //if (thing) GCon->Logf(NAME_Debug, "*** CLASS '%s': scaleY=%g; TOfs=%d; hgt=%d; dofix=%d", thing->GetClass()->GetName(), scaleY, TexTOffset, TexHeight, (TexTOffset < TexHeight && 2*TexTOffset+r_sprite_fix_delta >= TexHeight ? 1 : 0));
  // don't bother with projectiles, they're usually flying anyway
  if (fixAlgo && !r_fix_sprite_offsets_missiles && thing->IsMissile()) fixAlgo = 0;
  // do not fix offset for flying monsters (but fix flying corpses, just in case)
  if (fixAlgo && thing->IsAnyAerial()) {
    if (thing->IsAnyCorpse()) {
      // don't fix if it is not on a floor
      if (thing->Origin.z != thing->FloorZ) fixAlgo = 0;
    } else {
      fixAlgo = 0;
    }
  }
  if (fixAlgo > 1) {
    // new algo
    const int allowedDelta = -r_sprite_fix_delta.asInt();
    if (allowedDelta > 0) {
      const int sph = Tex->GetRealHeight();
      if (sph > 0 && TexHeight > 0) {
        const int spbot = sph-TexTOffset; // pixels under "hotspot"
        if (spbot > 0) {
          int botofs = (int)(spbot*scaleY);
          //GCon->Logf(NAME_Debug, "%s: height=%d; realheight=%d; ofs=%d; spbot=%d; botofs=%d; tofs=%d; adelta=%d", thing->GetClass()->GetName(), TexHeight, sph, TexTOffset, spbot, botofs, TexTOffset, allowedDelta);
          if (botofs > 0 && botofs <= allowedDelta) {
            //GCon->Logf(NAME_Debug, "%s: height=%d; realheight=%d; ofs=%d; spbot=%d; botofs=%d; tofs=%d", thing->GetClass()->GetName(), TexHeight, sph, TexTOffset, spbot, botofs, TexTOffset);
            // sink corpses a little
            if (thing->IsAnyCorpse() && r_fix_sprite_offsets_smart_corpses) {
              const float clipFactor = 1.8f;
              const float ratio = clampval((float)botofs*clipFactor/(float)sph, 0.5f, 1.0f);
              botofs = (int)((float)botofs*ratio);
              if (botofs < 0 || botofs > allowedDelta) botofs = 0;
            }
            TexTOffset += botofs/scaleY;
          }
        }
      }
    }
  } else if (fixAlgo > 0) {
    // old algo
    const int sph = (r_use_real_sprite_offset ? Tex->GetRealHeight() : TexHeight);
    //if (thing) GCon->Logf(NAME_Debug, "THING '%s': sph=%d; height=%d", thing->GetClass()->GetName(), sph, TexHeight);
    if (TexTOffset < /*TexHeight*/sph && 2*TexTOffset+r_sprite_fix_delta >= /*TexHeight*/sph && scaleY > 0.6f && scaleY < 1.6f) {
      TexTOffset = /*TexHeight*/sph;
    }
    /*
    if (Tex->bForcedSpriteOffset && r_use_sprofs_lump) {
      TexSOffset += Tex->SOffset-Tex->SOffsetFix;
      TexTOffset += Tex->TOffset-Tex->TOffsetFix;
    }
    */
  }
}


//==========================================================================
//
//  VRenderLevelShared::QueueSprite
//
//  this uses `seclight` from ri
//  this can modify `ri`!
//
//==========================================================================
void VRenderLevelShared::QueueSprite (VEntity *thing, RenderStyleInfo &ri, bool onlyShadow) {
  const int sprtype = thing->SpriteType;
  TVec sprorigin = thing->GetDrawOrigin();

  bool renderShadow =
    sprtype == SPR_VP_PARALLEL_UPRIGHT &&
    !ri.isShadow() && !ri.isShaded() /*yep*/ && !ri.isFuzzy() &&
    r_fake_sprite_shadows.asInt() &&
    (r_fake_shadow_scale.asFloat() > 0.0f);

  bool doCheckFrames = false;
  if (renderShadow) {
    if (r_shadowmaps.asBool() && Drawer->CanRenderShadowMaps()) {
      // this is what shadowmaping does
      const int sss = r_shadowmap_sprshadows.asInt();
           if (sss > 1) renderShadow = false;
      else if (sss == 1) doCheckFrames = true;
    }
    if (thing == ViewEnt && (!r_chasecam || ViewEnt != cl->MO)) {
      // don't draw camera actor shadow (just in case, it should not come here anyway)
      renderShadow = false;
    }
    if (renderShadow && ri.isAdditive()) {
      if (thing->IsMissile()) {
        renderShadow = (r_fake_shadows_missiles.asBool() && r_fake_shadows_additive_missiles.asBool());
      } else if (thing->IsMonster()) {
        renderShadow = (r_fake_shadows_monsters.asBool() && r_fake_shadows_additive_monsters.asBool());
      } else {
        renderShadow = false;
      }
    }
  }

  const VEntity::EType tclass = thing->Classify();

  if (renderShadow) {
    switch (tclass) {
      case VEntity::EType::ET_Unknown: renderShadow = false; break;
      case VEntity::EType::ET_Player: renderShadow = r_fake_shadows_players.asBool(); break;
      case VEntity::EType::ET_Missile: renderShadow = r_fake_shadows_missiles.asBool(); break;
      case VEntity::EType::ET_Corpse: renderShadow = r_fake_shadows_corpses.asBool(); break;
      case VEntity::EType::ET_Monster: renderShadow = r_fake_shadows_monsters.asBool(); break;
      case VEntity::EType::ET_Decoration: renderShadow = r_fake_shadows_decorations.asBool(); break;
      case VEntity::EType::ET_Pickup: renderShadow = r_fake_shadows_pickups.asBool(); break;
      default: abort();
    }
    // do not render shadow if floor surface is higher than the camera, or if the sprite is under the floor
    if (renderShadow) {
      if (thing->FloorZ >= Drawer->vieworg.z) {
        // the floor is higher
        renderShadow = false;
      } else if (sprorigin.z+8 < thing->FloorZ || sprorigin.z+thing->Height <= thing->FloorZ) {
        // check origin (+8 for "floatbob")
        renderShadow = false;
      }
    }
  }

  if (onlyShadow && !renderShadow) return;

  spritedef_t *sprdef;
  spriteframe_t *sprframe;

  int SpriteIndex = thing->GetEffectiveSpriteIndex();
  int FrameIndex = thing->GetEffectiveSpriteFrame();
  if (thing->FixedSpriteName != NAME_None) SpriteIndex = VClass::FindSprite(thing->FixedSpriteName, false); // don't append

  if ((unsigned)SpriteIndex >= (unsigned)sprites.length()) {
    #ifdef PARANOID
    GCon->Logf(NAME_Dev, "Invalid sprite number %d", SpriteIndex);
    #endif
    return;
  }

  // decide which patch to use for sprite relative to player
  sprdef = &sprites.ptr()[SpriteIndex];
  if (FrameIndex >= sprdef->numframes) {
    #ifdef PARANOID
    GCon->Logf(NAME_Dev, "Invalid sprite frame %d : %d", SpriteIndex, FrameIndex);
    #endif
    return;
  }

  sprframe = &sprdef->spriteframes[FrameIndex];

  if (renderShadow && doCheckFrames) {
    // this is what shadowmaping does
    renderShadow = false;
    if (sprframe->rotate) {
      for (unsigned int f = 1; f < 16; ++f) if (sprframe->lump[0] != sprframe->lump[f]) { renderShadow = true; break; }
    }
    if (!renderShadow && onlyShadow) return;
  }


  TVec sprforward(0, 0, 0);
  TVec sprright(0, 0, 0);
  TVec sprup(0, 0, 0);

  //const TVec vfwd = Drawer->viewforward;
  const TVec vfwd = AngleVectorYaw(Drawer->viewangles.yaw);

  if (sprtype == SPR_VP_PARALLEL_UPRIGHT) {
    // HACK: if sprite is additive, move is slightly closer to view
    // this is mostly for things like light flares
    if (ri.isAdditive()) sprorigin -= vfwd*0.22f;

    // move various things forward/backward a little
    switch (tclass) {
      case VEntity::EType::ET_Player: sprorigin -= vfwd*(0.12f*1.8f); break; // forward
      case VEntity::EType::ET_Corpse: sprorigin += vfwd*(0.12f*1.8f); break; // backward
      case VEntity::EType::ET_Pickup: sprorigin -= vfwd*(0.16f*1.8f); break; // forward
      default:
             if (thing->IsNoInteraction() || thing->IsFloatBob()) sprorigin -= vfwd*(0.18f*1.8f);
        else if (thing->EntityFlags&(VEntity::EF_Bright|VEntity::EF_FullBright)) sprorigin -= vfwd*(0.13f*1.8f);
        break;
    }
  }

  float dot;
  TVec tvec(0, 0, 0);
  float sr;
  float cr;
  //FIXME: is this right?
  // this also disables shadow
  const bool ignoreSpriteFix = (sprtype != SPR_VP_PARALLEL_UPRIGHT);
  if (ignoreSpriteFix) renderShadow = false;

  switch (sprtype) {
    case SPR_VP_PARALLEL_UPRIGHT:
      // Generate the sprite's axes, with sprup straight up in worldspace,
      // and sprright parallel to the viewplane. This will not work if the
      // view direction is very close to straight up or down, because the
      // cross product will be between two nearly parallel vectors and
      // starts to approach an undefined state, so we don't draw if the two
      // vectors are less than 1 degree apart
      dot = Drawer->viewforward.z; // same as DotProduct(Drawer->viewforward, sprup), because sprup is 0, 0, 1
      if (dot > 0.999848f || dot < -0.999848f) return; // cos(1 degree) = 0.999848f
      sprup = TVec(0, 0, 1);
      // CrossProduct(sprup, Drawer->viewforward)
      sprright = Normalise(TVec(Drawer->viewforward.y, -Drawer->viewforward.x, 0));
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
      tvec = Normalise(sprorigin-Drawer->vieworg);
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
      sprup = Drawer->viewup;
      sprright = Drawer->viewright;
      sprforward = Drawer->viewforward;
      break;

    case SPR_ORIENTED:
    case SPR_ORIENTED_OFS:
      // generate the sprite's axes, according to the sprite's world orientation
      AngleVectors(thing->/*Angles*/GetSpriteDrawAngles(), sprforward, sprright, sprup);
      if (sprtype != SPR_ORIENTED) ri.flags |= RenderStyleInfo::FlagOriented|RenderStyleInfo::FlagNoDepthWrite|RenderStyleInfo::FlagOffset;
      break;

    case SPR_VP_PARALLEL_ORIENTED:
      // Generate the sprite's axes, parallel to the viewplane, but
      // rotated in that plane around the center according to the sprite
      // entity's roll angle. So sprforward stays the same, but sprright
      // and sprup rotate
      sr = msin(thing->Angles.roll);
      cr = mcos(thing->Angles.roll);

      sprforward = Drawer->viewforward;
      sprright = TVec(Drawer->viewright.x*cr+Drawer->viewup.x*sr, Drawer->viewright.y*cr+Drawer->viewup.y*sr, Drawer->viewright.z*cr+Drawer->viewup.z*sr);
      sprup = TVec(Drawer->viewright.x*(-sr)+Drawer->viewup.x*cr, Drawer->viewright.y*(-sr)+Drawer->viewup.y*cr, Drawer->viewright.z*(-sr)+Drawer->viewup.z*cr);
      break;

    case SPR_VP_PARALLEL_UPRIGHT_ORIENTED:
      // Generate the sprite's axes, with sprup straight up in worldspace,
      // and sprright parallel to the viewplane and then rotated in that
      // plane around the center according to the sprite entity's roll
      // angle. So sprforward stays the same, but sprright and sprup rotate
      // This will not work if the view direction is very close to straight
      // up or down, because the cross product will be between two nearly
      // parallel vectors and starts to approach an undefined state, so we
      // don't draw if the two vectors are less than 1 degree apart
      dot = Drawer->viewforward.z; // same as DotProduct(viewforward, sprup), because sprup is 0, 0, 1
      if (dot > 0.999848f || dot < -0.999848f) return; // cos(1 degree) = 0.999848f

      sr = msin(thing->Angles.roll);
      cr = mcos(thing->Angles.roll);

      // CrossProduct(TVec(0, 0, 1), Drawer->viewforward)
      tvec = Normalise(TVec(Drawer->viewforward.y, -Drawer->viewforward.x, 0));
      // CrossProduct(tvec, TVec(0, 0, 1))
      sprforward = TVec(-tvec.y, tvec.x, 0);
      // Rotate
      sprright = TVec(tvec.x*cr, tvec.y*cr, tvec.z*cr+sr);
      sprup = TVec(tvec.x*(-sr), tvec.y*(-sr), tvec.z*(-sr)+cr);
      break;

    case SPR_FLAT: // offset slightly by pitch -- for floor/ceiling splats; ignore roll angle
      {
        TAVec angs = thing->GetSpriteDrawAngles();
        // this is what makes the sprite looks like in GZDoom
        angs.pitch = AngleMod(angs.pitch-90.0f);
        // roll is meaningfull too
        angs.roll = AngleMod(angs.roll+180.0f);
        // generate the sprite's axes, according to the sprite's world orientation
        AngleVectors(angs, sprforward, sprright, sprup);
        ri.flags |= RenderStyleInfo::FlagFlat|RenderStyleInfo::FlagOffset|RenderStyleInfo::FlagNoCull;
      }
      break;

    case SPR_WALL: // offset slightly by yaw -- for wall splats; ignore pitch and roll angle
      {
        TAVec angs = thing->GetSpriteDrawAngles();
        // dunno if roll should be kept here
        angs.pitch = 0;
        angs.roll = 0;
        // this is what makes the sprite looks like in GZDoom
        angs.yaw = AngleMod(angs.yaw+180.0f);
        // generate the sprite's axes, according to the sprite's world orientation
        AngleVectors(angs, sprforward, sprright, sprup);
        ri.flags |= RenderStyleInfo::FlagWall|RenderStyleInfo::FlagOffset|RenderStyleInfo::FlagNoCull;
      }
      break;

    default:
      Sys_Error("QueueSprite: Bad sprite type %d", sprtype);
  }

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
      matan(sprorigin.y-Drawer->vieworg.y, sprorigin.x-Drawer->vieworg.x) :
      matan(sprforward.y+Drawer->viewforward.y, sprforward.x+Drawer->viewforward.x));
    const float angadd = (sprframe->lump[0] == sprframe->lump[1] ? 45.0f/2.0f : 45.0f/4.0f); //k8: is this right?
    //const float angadd = (useCameraPlane ? 45.0f/2.0f : 45.0f/4.0f);
    /*
    if (sprframe->lump[0] == sprframe->lump[1]) {
      ang = matan(sprorigin.y-Drawer->vieworg.y, sprorigin.x-Drawer->vieworg.x);
      ang = AngleMod(ang-thing->GetSpriteDrawAngles().yaw+180.0f+45.0f/2.0f);
    } else {
      ang = matan(sprforward.y+Drawer->viewforward.y, sprforward.x+Drawer->viewforward.x);
      ang = AngleMod(ang-thing->GetSpriteDrawAngles().yaw+180.0f+45.0f/4.0f);
    }
    */
    ang = AngleMod(ang-thing->GetSpriteDrawAngles().yaw+180.0f+angadd);
    const unsigned rot = (unsigned)(ang*16.0f/360.0f)&15;
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

  const int oldRelease = gl_release_ram_textures_mode.asInt();
  const bool cropIt = (oldRelease < 2 && gl_crop_sprites.asBool());
  if (cropIt) {
    gl_release_ram_textures_mode = 0;
    Tex->CropTexture();
  }

  // make sprites with translucent textures translucent
  if (Tex->isTranslucent() && !ri.isTranslucent()) ri.translucency = RenderStyleInfo::Translucent;

  if (cropIt) gl_release_ram_textures_mode = oldRelease;

  //if (r_brightmaps && r_brightmaps_sprite && Tex->Brightmap && Tex->Brightmap->nofullbright) light = seclight; // disable fullbright
  // ignore brightmap flags for stencil style
  if (!ri.isStenciled() && r_brightmaps && r_brightmaps_sprite && Tex->nofullbright) ri.light = ri.seclight; // disable fullbright

  int fixAlgo = (ignoreSpriteFix ? 0 : r_fix_sprite_offsets.asInt());
  if (fixAlgo < 0 || thing->IsFloatBob()) fixAlgo = 0; // just in case

  int TexWidth = Tex->GetWidth();
  int TexHeight = Tex->GetHeight();
  int TexSOffset; // TexTOffset;

  if (fixAlgo > 1 && Tex->bForcedSpriteOffset && r_use_sprofs_lump) {
    TexSOffset = Tex->SOffsetFix;
    //TexTOffset = Tex->TOffsetFix;
  } else {
    TexSOffset = Tex->SOffset;
    //TexTOffset = Tex->TOffset;
  }

  /*
  if (Tex->SScale != 1.0f || Tex->TScale != 1.0f) {
    GCon->Logf(NAME_Debug, "%s: scale=(%g,%g); size=(%d,%d); scaledsize=(%d,%d)", *Tex->Name, Tex->SScale, Tex->TScale, Tex->GetWidth(), Tex->GetHeight(), Tex->GetScaledWidth(), Tex->GetScaledHeight());
  }
  */

  //float scaleX = max2(0.001f, thing->ScaleX/Tex->SScale);
  float scaleX = thing->ScaleX;
  if (thing->IsSpriteFlipped()) scaleX = -scaleX;
  if (scaleX < 0.0f) { scaleX = -scaleX; flip = !flip; }
  scaleX /= Tex->SScale;

  //float scaleY = max2(0.001f, thing->ScaleY/Tex->TScale);
  float scaleY = fabsf(thing->ScaleY); // sorry, vertical flip is not working yet
  scaleY /= Tex->TScale;

  if (!isFiniteF(scaleX) || scaleX == 0.0f) return;
  if (!isFiniteF(scaleY) || scaleY == 0.0f) return;

  TVec sv[4];

  TVec start = -TexSOffset*sprright*scaleX;
  TVec end = (TexWidth-TexSOffset)*sprright*scaleX;

  //const int origTOffset = TexTOffset;
  int TexTOffset, origTOffset;
  FixSpriteOffset(fixAlgo, thing, Tex, TexHeight, scaleY, /*out*/TexTOffset, /*out*/origTOffset);

  TVec topdelta = TexTOffset*sprup*scaleY;
  TVec botdelta = (TexTOffset-TexHeight)*sprup*scaleY;

  sv[0] = sprorigin+start+botdelta;
  sv[1] = sprorigin+start+topdelta;
  sv[2] = sprorigin+end+topdelta;
  sv[3] = sprorigin+end+botdelta;

  //if (Fade != FADE_LIGHT) GCon->Logf("<%s>: Fade=0x%08x", *thing->GetClass()->GetFullName(), Fade);

  //if (!ri.isTranslucent() && Tex->isTranslucent()) Alpha = 0.9999f;

  //if (ri.isTranslucent() || r_sort_sprites || Tex->isTranslucent())
  {
    // add sprite
    int priority = 0;
    if (thing) {
           if (thing->EntityFlags&VEntity::EF_Bright) priority = 200;
      else if (thing->EntityFlags&VEntity::EF_FullBright) priority = 100;
      else if (thing->EntityFlags&(VEntity::EF_Corpse|VEntity::EF_Blasted)) priority = -120;
      else if (thing->Health <= 0) priority = -110;
      else if (thing->EntityFlags&VEntity::EF_NoBlockmap) priority = -200;
      //else if (tclass == VEntity::EType::ET_Pickup) priority = 50;
    }
    if (!onlyShadow) {
      #if 0
      Drawer->DrawSpritePolygon((Level ? Level->Time : 0.0f), sv, Tex, ri,
        GetTranslation(thing->Translation), ColorMap,
        -sprforward, DotProduct(sprorigin, -sprforward),
        (flip ? -sprright : sprright)/scaleX,
        -sprup/scaleY, (flip ? sv[2] : sv[1]));
      #else
      QueueSpritePoly(sv, lump, ri, thing->Translation,
        -sprforward, DotProduct(sprorigin, -sprforward),
        (flip ? -sprright : sprright)/scaleX, -sprup/scaleY,
        (flip ? sv[2] : sv[1]), priority, thing->Origin, thing->ServerUId);
      #endif
    }
    // add shadow
    if (renderShadow) {
      float Alpha = ri.alpha*r_fake_shadow_translucency.asFloat();
      if (Alpha >= 0.012f) {
        // move it slightly further, because they're translucent, and should be further to not overlay the sprite itself
        sprorigin += /*Drawer->viewforward*/vfwd*0.5f;
        // check origin (+12 for "floatbob")
        /*if (sprorigin.z+12 >= thing->FloorZ)*/
        if (r_fake_sprite_shadows.asInt() == 1) {
          // 2d
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

          ri.alpha = Alpha;
          ri.flags = RenderStyleInfo::FlagShadow|RenderStyleInfo::FlagNoDepthWrite;
          ri.stencilColor = 0xff000000u; // shadows are black-stenciled
          ri.translucency = RenderStyleInfo::Translucent;
          QueueSpritePoly(sv, lump, ri, /*translation*/0,
            -sprforward, DotProduct(sprorigin, -sprforward),
            (flip ? -sprright : sprright)/scaleX, -sprup/scaleY,
            (flip ? sv[2] : sv[1]), priority, thing->Origin, thing->ServerUId);
        } else if (r_fake_sprite_shadows.asInt() == 2) {
          // pseudo3d
          sprorigin.z = thing->FloorZ+0.2f;

          Alpha = min2(1.0f, Alpha);
          scaleY *= r_fake_3dshadow_scale.asFloat();

          TAVec angs = thing->GetSpriteDrawAngles();
          angs.yaw = 0;
          // lay it on the floor
          angs.pitch = -90;
          if (r_fake_3dshadow_mode.asInt() == 0) {
            // direction to the thing
            TVec spdir = TVec(sprorigin.x-Drawer->vieworg.x, sprorigin.y-Drawer->vieworg.y).normalised();
            const float ato = VectorAngleYaw(spdir);
            angs.roll = AngleMod(ato+180.0f);
          } else {
            // camera yaw
            angs.roll = AngleMod(Drawer->viewangles.yaw+180.0f);
          }
          // generate the shadow's axes
          AngleVectors(angs, sprforward, sprright, sprup);

          //TexTOffset = origTOffset;

          start = -TexSOffset*sprright*scaleX;
          end = (TexWidth-TexSOffset)*sprright*scaleX;

          topdelta = TexTOffset*sprup*scaleY;
          botdelta = (TexTOffset-TexHeight)*sprup*scaleY;

          sv[0] = sprorigin+start+botdelta;
          sv[1] = sprorigin+start+topdelta;
          sv[2] = sprorigin+end+topdelta;
          sv[3] = sprorigin+end+botdelta;

          ri.alpha = Alpha;
          ri.flags = RenderStyleInfo::FlagShadow|RenderStyleInfo::FlagNoDepthWrite;
          ri.flags |= RenderStyleInfo::FlagFlat|RenderStyleInfo::FlagOffset|RenderStyleInfo::FlagNoCull;
          ri.stencilColor = 0xff000000u; // shadows are black-stenciled
          ri.translucency = RenderStyleInfo::Translucent;
          flip = !flip;
          QueueSpritePoly(sv, lump, ri, /*translation*/0,
            -sprforward, DotProduct(sprorigin, -sprforward),
            (flip ? -sprright : sprright)/scaleX, -sprup/scaleY,
            (flip ? sv[2] : sv[1]), priority, thing->Origin, thing->ServerUId);
        }
      }
    }
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

    // sort by distance to view origin (nearest last)
    const float d0 = ta->dist;
    const float d1 = tb->dist;
    if (d0 < d1) return 1; // a is nearer, so it is last (a is greater)
    if (d0 > d1) return -1; // b is nearer, so it is last (a is lesser)

    // non-translucent objects should come first
    #if 0
    // translucent/additive
    const unsigned aTrans = (unsigned)(ta->rstyle.alpha < 1.0f || (ta->rstyle.isTranslucent() && !ta->rstyle.isStenciled()));
    const unsigned bTrans = (unsigned)(tb->rstyle.alpha < 1.0f || (tb->rstyle.isTranslucent() && !tb->rstyle.isStenciled()));

    // if both has that bit set, the result of xoring will be 0
    if (aTrans^bTrans) {
      // only one is translucent
      vassert(aTrans == 0 || bTrans == 0);
      return (aTrans ? 1/*a is translucent, b is not; b first (a is greater)*/ : -1/*a is not translucent, b is translucent; a first (a is lesser)*/);
    }
    #endif

    #if 0
    // mirrors come first, then comes floor/ceiling surfaces
    if ((ta->rstyle.flags^tb->rstyle.flags)&(RenderStyleInfo::FlagCeiling|RenderStyleInfo::FlagFloor|RenderStyleInfo::FlagMirror)) {
      if ((ta->rstyle.flags^tb->rstyle.flags)&RenderStyleInfo::FlagMirror) {
        return (ta->rstyle.flags&RenderStyleInfo::FlagMirror ? -1 : 1);
      }
      return (ta->rstyle.flags&(RenderStyleInfo::FlagCeiling|RenderStyleInfo::FlagFloor) ? -1 : 1);
    }

    // shadows come first
    // oriented comes next
    const unsigned taspec = ta->rstyle.flags&(RenderStyleInfo::FlagShadow|RenderStyleInfo::FlagOriented);
    const unsigned tbspec = tb->rstyle.flags&(RenderStyleInfo::FlagShadow|RenderStyleInfo::FlagOriented);
    // if both has the same bits set, the result of xoring will be 0
    if (taspec^tbspec) {
      // one is shadow?
      if ((taspec^tbspec)&RenderStyleInfo::FlagShadow) {
        return (taspec&RenderStyleInfo::FlagShadow ? -1/*a first (a is lesser)*/ : 1/*b first (a is greater)*/);
      }
      // one is oriented?
      if ((taspec^tbspec)&RenderStyleInfo::FlagOriented) {
        return (taspec&RenderStyleInfo::FlagOriented ? -1/*a first (a is lesser)*/ : 1/*b first (a is greater)*/);
      }
    }
    #endif

    #if 0
    // sort by special type
    const unsigned tahang = ta->rstyle.flags&~RenderStyleInfo::FlagOptionsMask;
    const unsigned tbhang = tb->rstyle.flags&~RenderStyleInfo::FlagOptionsMask;

    if (tahang|tbhang) {
      if (tahang != tbhang) return (tahang > tbhang ? -1/*a first (a is lesser) */ : 1/*b first (a is greater)*/);
    }
    #endif

    // priority check
    // higher priority comes first
    if (ta->prio < tb->prio) return 1; // a has lower priority, it should come last (a > b)
    if (ta->prio > tb->prio) return -1; // a has higher priority, it should come first (a < b)

    #if 0
    // sort by object type
    // first masked polys, then sprites, then alias models
    // type 0: masked polys
    // type 1: sprites
    // type 2: alias models
    const int typediff = (int)ta->type-(int)tb->type;
    if (typediff) return typediff;

    /*
    // priority check
    // higher priority comes first
    if (ta->prio < tb->prio) return 1; // a has lower priority, it should come last (a > b)
    if (ta->prio > tb->prio) return -1; // a has higher priority, it should come first (a < b)
    */

    if (ta->objid < tb->objid) return -1;
    if (ta->objid > tb->objid) return 1;

    // sort sprites by lump number, why not
    if (ta->type == TSP_Sprite) {
      if (ta->lump < tb->lump) return -1;
      if (ta->lump > tb->lump) return 1;
    }
    #endif

    // nothing to check anymore, consider equal
    return 0;
  }
}


//==========================================================================
//
//  TSNextPOfs
//
//==========================================================================
static inline void TSNextPOfs (VRenderLevelShared::TransPolyState &transSprState) noexcept {
  // switch to next transSprState.pofs
  //if (++transSprState.pofs == /*MAX_POFS*/10) transSprState.pofs = 0;
  ++transSprState.pofs;
  Drawer->GLPolygonOffsetEx(r_sprite_pslope, -(transSprState.pofs*r_sprite_pofs)); // pull forward
  transSprState.pofsEnabled = true;
}


//==========================================================================
//
//  TSDisablePOfs
//
//==========================================================================
static inline void TSDisablePOfs (VRenderLevelShared::TransPolyState &transSprState) noexcept {
  if (transSprState.pofsEnabled) { Drawer->GLDisableOffset(); transSprState.pofsEnabled = false; }
}


//==========================================================================
//
//  VRenderLevelShared::DrawTransSpr
//
//==========================================================================
void VRenderLevelShared::DrawTransSpr (trans_sprite_t &spr) {
  //GCon->Logf("  #%d: type=%d; alpha=%g; additive=%d; light=0x%08x; fade=0x%08x", f, spr.type, spr.Alpha, (int)spr.Additive, spr.light, spr.Fade);
  switch (spr.type) {
    case TSP_Wall:
      // masked polygon
      // non-translucent and non-additive polys should not end up here
      vassert(spr.surf);
      TSDisablePOfs(transSprState);
      if (transSprState.allowTransPolys) {
        //if (spr.surf->drawflags&surface_t::DF_MIRROR)
        {
          Drawer->DrawMaskedPolygon(spr.surf, spr.rstyle.alpha, spr.rstyle.isAdditive(), !(spr.rstyle.flags&RenderStyleInfo::FlagNoDepthWrite));
        }
      }
      break;
    case TSP_Sprite:
      // sprite
      if (transSprState.sortWithOfs && transSprState.lastpdist == spr.pdist) {
        if (!transSprState.pofsEnabled) TSNextPOfs(transSprState);
      } else if (transSprState.sortWithOfs && spr.prio == 50) {
        // pickup
        //transSprState.pofs = 0;
        //TSNextPOfs(transSprState);
        //Drawer->GLPolygonOffsetEx(r_sprite_pslope, (transSprState.pofs*r_sprite_pofs)); // pull forward
        //transSprState.pofsEnabled = true;
      } else {
        TSDisablePOfs(transSprState);
        // reset transSprState.pofs
        transSprState.pofs = 0;
      }
      transSprState.lastpdist = spr.pdist;
      Drawer->DrawSpritePolygon((Level ? Level->Time : 0.0f), spr.Verts, GTextureManager[spr.lump],
                                spr.rstyle, GetTranslation(spr.translation),
                                ColorMap, spr.normal, spr.pdist,
                                spr.saxis, spr.taxis, spr.texorg);
      break;
    case TSP_Model:
      // alias model
      TSDisablePOfs(transSprState);
      DrawEntityModel(spr.Ent, spr.rstyle/*spr.light, spr.Fade, spr.Alpha, spr.Additive*/, spr.TimeFrac, RPASS_Normal);
      break;
    default: Sys_Error("VRenderLevelShared::DrawTransSpr: invalid sprite type (%d)", spr.type);
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

  transSprState.reset();
  transSprState.allowTransPolys = !dbg_disable_translucent_polys.asBool();
  transSprState.sortWithOfs = r_sprite_use_pofs.asBool();

  // render sprite shadpows first (there is no need to sort them)
  if (dls.DrawSpriteShadowsList.length() > 0) {
    // there is no need to sort solid sprites
    //if (!dbg_disable_sprite_sorting) timsort_r(dls.DrawSpriteList.ptr(), dls.DrawSpriteList.length(), sizeof(dls.DrawSpriteList[0]), &transCmp, nullptr);
    for (auto &&spr : dls.DrawSpriteShadowsList) DrawTransSpr(spr);
  }

  if (dls.DrawSpriteList.length() > 0) {
    // there is no need to sort solid sprites
    //if (!dbg_disable_sprite_sorting) timsort_r(dls.DrawSpriteList.ptr(), dls.DrawSpriteList.length(), sizeof(dls.DrawSpriteList[0]), &transCmp, nullptr);
    for (auto &&spr : dls.DrawSpriteList) DrawTransSpr(spr);
  }

  if (dls.DrawSurfListAlpha.length()) {
    if (!dbg_disable_sprite_sorting) timsort_r(dls.DrawSurfListAlpha.ptr(), dls.DrawSurfListAlpha.length(), sizeof(dls.DrawSurfListAlpha[0]), &traspCmp, nullptr);
    for (auto &&spr : dls.DrawSurfListAlpha) DrawTransSpr(spr);
  }

  if (transSprState.pofsEnabled) Drawer->GLDisableOffset();
}
