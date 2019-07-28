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
//** masked polys and sprites queue
//**
//**************************************************************************
#include "gamedefs.h"
#include "r_local.h"
#include "sv_local.h"


extern VCvarB r_sort_sprites;
static VCvarB r_sprite_use_pofs("r_sprite_use_pofs", true, "Use PolygonOffset with sprite sorting to reduce sprite flickering?", CVAR_Archive);

static VCvarF r_sprite_pofs("r_sprite_pofs", "128", "DEBUG");
static VCvarF r_sprite_pslope("r_sprite_pslope", "-1.0", "DEBUG");


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
  if (count == 0 || Alpha < 0.01f) return;

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
  if (!mobj) return; // just in case

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
  spr.objid = mobj->GetUniqueId();
  spr.prio = 0; // normal priority
  spr.hangup = 0;
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

    check(!ta->Additive);
    check(!tb->Additive);

    // translucent
    const bool aTrans = (ta->Alpha < 1.0f);
    const bool bTrans = (tb->Alpha < 1.0f);
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
      if (!IsAdvancedRenderer()) {
        check(spr.surf);
        if (pofsEnabled) { glDisable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(0, 0); pofsEnabled = false; }
        Drawer->DrawMaskedPolygon(spr.surf, spr.Alpha, spr.Additive);
      }
    }
  }
#undef MAX_POFS

  if (pofsEnabled) { glDisable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(0, 0); }

  // reset list
  traspUsed = traspFirst;
}
