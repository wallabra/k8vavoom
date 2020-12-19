//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš, dj_jl
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
#include "r_light_adv.h"

// dunno if it does anything interesting
#define VV_SMAP_PAPETHIN_FIX

static VCvarB clip_advlight_regions("clip_advlight_regions", false, "Clip (1D) light regions?", CVAR_PreInit);

// this is because the other side has flipped texture, so if
// the player stands behind it, the shadow is wrong
static VCvarB r_shadowmap_flip_surfaces("r_shadowmap_flip_surfaces", true, "Flip two-sided surfaces for shadowmapping?", CVAR_Archive);


// ////////////////////////////////////////////////////////////////////////// //
enum {
  FlagAsLight = 0x01u,
  FlagAsShadow = 0x02u,
  FlagAsBoth = FlagAsLight|FlagAsShadow,
};


//==========================================================================
//
//  VRenderLevelShadowVolume::CollectLightShadowSurfaces
//
//==========================================================================
void VRenderLevelShadowVolume::CollectLightShadowSurfaces () {
  LightClip.ClearClipNodes(CurrLightPos, Level, CurrLightRadius);
  LightShadowClip.ClearClipNodes(CurrLightPos, Level, CurrLightRadius);
  shadowSurfacesSolid.resetNoDtor();
  shadowSurfacesMasked.resetNoDtor();
  lightSurfacesSolid.resetNoDtor();
  lightSurfacesMasked.resetNoDtor();
  collectorForShadowMaps = (r_shadowmaps.asBool() && Drawer->CanRenderShadowMaps());
  collectorShadowType = (collectorForShadowMaps && r_shadowmap_flip_surfaces.asBool() ? VViewClipper::AsShadowMap : VViewClipper::AsShadow);
  CollectAdvLightBSPNode(Level->NumNodes-1, nullptr);
}


//==========================================================================
//
//  ClipSegToLight
//
//  this (theoretically) should clip segment to light bounds
//  tbh, i don't think that there is a real reason to do this
//
//==========================================================================
/*
static VVA_OKUNUSED inline void ClipSegToLight (TVec &v1, TVec &v2, const TVec &pos, const float radius) {
  const TVec r1 = pos-v1;
  const TVec r2 = pos-v2;
  const float d1 = DotProduct(Normalise(CrossProduct(r1, r2)), pos);
  const float d2 = DotProduct(Normalise(CrossProduct(r2, r1)), pos);
  // there might be a better method of doing this, but this one works for now...
       if (d1 > radius && d2 < -radius) v2 += (v2-v1)*d1/(d1-d2);
  else if (d2 > radius && d1 < -radius) v1 += (v1-v2)*d2/(d2-d1);
}
*/


//==========================================================================
//
//  VRenderLevelShadowVolume::AddPolyObjToLightClipper
//
//  we have to do this separately, because for now we have to add
//  invisible segs to clipper too
//  i don't yet know why
//
//==========================================================================
void VRenderLevelShadowVolume::AddPolyObjToLightClipper (VViewClipper &clip, subsector_t *sub, int asShadow) {
  if (sub && sub->HasPObjs() && r_draw_pobj && clip_use_1d_clipper) {
    for (auto &&it : sub->PObjFirst()) {
      polyobj_t *pobj = it.value();
      seg_t **polySeg = pobj->segs;
      for (int polyCount = pobj->numsegs; polyCount--; ++polySeg) {
        seg_t *seg = (*polySeg)->drawsegs->seg;
        if (seg->linedef) {
          clip.CheckAddClipSeg(seg, nullptr/*mirror*/, asShadow);
        }
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::CollectAdvLightSurfaces
//
//  LightCanCross:
//    -1: two-sided line, top or bottom
//     0: one-sided line, or flat
//     1: two-sided line, middle
//
//==========================================================================
void VRenderLevelShadowVolume::CollectAdvLightSurfaces (surface_t *InSurfs, texinfo_t *texinfo,
                                                        VEntity *SkyBox, bool CheckSkyBoxAlways, int LightCanCross,
                                                        unsigned int ssflag, const bool paperThin)
{
  if (!InSurfs) return;
  if (!(ssflag&FlagAsBoth)) return;

  if (!texinfo || !texinfo->Tex || texinfo->Tex->Type == TEXTYPE_Null) return;
  if (texinfo->Alpha < 1.0f || texinfo->Additive) return;

  if (SkyBox && SkyBox->IsPortalDirty()) SkyBox = nullptr;

  if (texinfo->Tex == GTextureManager.getIgnoreAnim(skyflatnum) ||
      (CheckSkyBoxAlways && (SkyBox && SkyBox->GetSkyBoxAlways())))
  {
    return;
  }

  const bool smaps = collectorForShadowMaps;
  const bool doflip = r_shadowmap_flip_surfaces.asBool();

  for (surface_t *surf = InSurfs; surf; surf = surf->next) {
    if (surf->count < 3) continue; // just in case
    if (!isSurfaceInSpotlight(surf)) continue;

    // check transdoor hacks
    //if (surf->drawflags&surface_t::TF_TOPHACK) continue;

    // ignore translucent
    VTexture *tex = surf->texinfo->Tex;
    if (!tex || tex->Type == TEXTYPE_Null) continue;
    if (surf->texinfo->Alpha < 1.0f || surf->texinfo->Additive) continue;
    if (tex->isTranslucent()) continue; // this is translucent texture

    const float dist = DotProduct(CurrLightPos, surf->GetNormal())-surf->GetDist();
    if (!smaps && dist <= 0.0f) continue;
    if (fabsf(dist) >= CurrLightRadius) continue; // was for light

    // light
    if (ssflag&FlagAsLight) {
      if (dist > 0.0f && surf->IsPlVisible()) {
        // viewer is in front
        if (tex->isTransparent()) lightSurfacesMasked.append(surf); else lightSurfacesSolid.append(surf);
      }
    }

    // shadow
    if (ssflag&FlagAsShadow) {
      if (!smaps && (dist <= 0.0f || tex->isSeeThrough())) continue; // this is masked texture, shadow volumes cannot process it
      if (tex->isTransparent()) {
        // we need to flip it if the player is behind it
        vassert(smaps);
        if (doflip) {
          #ifdef VV_SMAP_PAPETHIN_FIX
          // this is for flats: when the camera is almost on a flat, it's shadow disappears
          // this is because we cannot see neither up, nor down surface
          // in this case, leave down one
          const float sdist = surf->plane.PointDistance(Drawer->vieworg);
          if (sdist <= 0.0f) {
            if (!paperThin || surf->plane.normal.z >= 0.0f) continue;
            // paper-thin surface, ceiling: leave it if it is almost invisible
            if (sdist < -0.1f) continue;
          }
          if (dist <= 0.0f) surf->drawflags |= surface_t::DF_SMAP_FLIP; else surf->drawflags &= ~surface_t::DF_SMAP_FLIP;
          #else
          if (surf->plane.PointOnSide(Drawer->vieworg)) continue; // if the camera cannot see it, no need to render it
          // flip if the light cannot see it
          if (dist <= 0.0f) surf->drawflags |= surface_t::DF_SMAP_FLIP; else surf->drawflags &= ~surface_t::DF_SMAP_FLIP;
          #endif
        } else {
          if (dist <= 0.0f) continue; // light cannot see it
          surf->drawflags &= ~surface_t::DF_SMAP_FLIP;
        }
        shadowSurfacesMasked.append(surf);
      } else {
        if (dist > 0.0f) shadowSurfacesSolid.append(surf);
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::CollectAdvLightLine
//
//  clips the given segment and adds any visible pieces to the line list
//
//==========================================================================
void VRenderLevelShadowVolume::CollectAdvLightLine (subsector_t *sub, sec_region_t *secregion, drawseg_t *dseg, unsigned int ssflag) {
  const seg_t *seg = dseg->seg;
  if (!seg->linedef) return; // miniseg

  const float dist = DotProduct(CurrLightPos, seg->normal)-seg->dist;
  //if (dist <= -CurrLightRadius || dist > CurrLightRadius) return; // light sphere is not touching a plane
  if (!collectorForShadowMaps && dist <= 0.0f) return;
  if (fabsf(dist) >= CurrLightRadius) return; // was for light

  //k8: here we can call `ClipSegToLight()`, but i see no reasons to do so
  if (ssflag&FlagAsLight) {
    if (dist <= 0.0f || !LightClip.IsRangeVisible(*seg->v2, *seg->v1)) {
      if ((ssflag &= ~FlagAsLight) == 0) return;
    }
  }
  if (ssflag&FlagAsShadow) {
    const bool isVis = (dist > 0.0f ? LightShadowClip.IsRangeVisible(*seg->v2, *seg->v1) : LightShadowClip.IsRangeVisible(*seg->v1, *seg->v2));
    if (!isVis) {
      if ((ssflag &= ~FlagAsShadow) == 0) return;
    }
  }

  #ifdef VV_CHECK_1S_CAST_SHADOW
  if ((ssflag&FlagAsShadow) && !seg->backsector && !CheckCan1SCastShadow(seg->linedef)) {
    //return;
    if ((ssflag &= ~FlagAsShadow) == 0) return;
  }
  #endif

#if 0
  // k8: this drops some segs that may leak without proper frustum culling
  // k8: this seems to be unnecessary now
  // k8: yet leave it there in the hope that it will reduce GPU overdrawing
  if (!LightClip.CheckSegFrustum(sub, seg)) return;
#endif

  VEntity *skybox = secregion->eceiling.splane->SkyBox;
  if (dseg->mid) CollectAdvLightSurfaces(dseg->mid->surfs, &dseg->mid->texinfo, skybox, false, (seg->backsector ? 1 : 0), ssflag);
  if (seg->backsector) {
    // two sided line
    if (dseg->top) CollectAdvLightSurfaces(dseg->top->surfs, &dseg->top->texinfo, skybox, false, (seg->backsector ? -1 : 0), ssflag);
    //k8: horizon/sky cannot block light
    //if (dseg->topsky) CollectAdvLightSurfaces(dseg->topsky->surfs, &dseg->topsky->texinfo, skybox, false, -1, ssflag);
    if (dseg->bot) CollectAdvLightSurfaces(dseg->bot->surfs, &dseg->bot->texinfo, skybox, false, (seg->backsector ? -1 : 0), ssflag);
    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      CollectAdvLightSurfaces(sp->surfs, &sp->texinfo, skybox, false, (seg->backsector ? -1 : 0), ssflag);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::CollectAdvLightSecSurface
//
//  this is used for floor and ceilings
//
//==========================================================================
void VRenderLevelShadowVolume::CollectAdvLightSecSurface (sec_surface_t *ssurf, VEntity *SkyBox, unsigned int ssflag, const bool paperThin) {
  //const sec_plane_t &plane = *ssurf->secplane;
  if (!ssurf->esecplane.splane->pic) return;

  //const float dist = DotProduct(CurrLightPos, plane.normal)-plane.dist;
  const float dist = ssurf->PointDist(CurrLightPos);
  //if (dist <= -CurrLightRadius || dist > CurrLightRadius) return; // light is in back side or on plane
  if (!collectorForShadowMaps && dist <= 0.0f) return;
  if (fabsf(dist) >= CurrLightRadius) return; // was for light
  if ((ssflag&FlagAsShadow) == 0 && dist <= 0.0f) return;

  CollectAdvLightSurfaces(ssurf->surfs, &ssurf->texinfo, SkyBox, true, 0, ssflag, paperThin);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::CollectAdvLightPolyObj
//
//==========================================================================
void VRenderLevelShadowVolume::CollectAdvLightPolyObj (subsector_t *sub, unsigned int ssflag) {
  if (sub && sub->HasPObjs() && r_draw_pobj) {
    subregion_t *region = sub->regions;
    sec_region_t *secregion = region->secregion;
    for (auto &&it : sub->PObjFirst()) {
      polyobj_t *pobj = it.value();
      seg_t **polySeg = pobj->segs;
      for (int polyCount = pobj->numsegs; polyCount--; ++polySeg) {
        CollectAdvLightLine(sub, secregion, (*polySeg)->drawsegs, ssflag);
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::CollectAdvLightSubRegion
//
//  Determine floor/ceiling planes.
//  Draw one or more line segments.
//
//==========================================================================
void VRenderLevelShadowVolume::CollectAdvLightSubRegion (subsector_t *sub, subregion_t *region, unsigned int ssflag) {
  const bool nextFirst = NeedToRenderNextSubFirst(region);
  if (nextFirst) CollectAdvLightSubRegion(sub, region->next, ssflag);

  sec_region_t *secregion = region->secregion;

  unsigned int ssflagreg = ssflag;
  if (clip_advlight_regions) {
    if ((ssflagreg&FlagAsLight) && !LightClip.ClipLightCheckRegion(region, sub, VViewClipper::AsLight)) ssflagreg &= ~FlagAsLight;
    if ((ssflagreg&FlagAsShadow) && !LightShadowClip.ClipLightCheckRegion(region, sub, collectorShadowType)) ssflagreg &= ~FlagAsShadow;
  }

  if (ssflagreg) {
    drawseg_t *ds = region->lines;
    for (int count = sub->numlines; count--; ++ds) CollectAdvLightLine(sub, secregion, ds, ssflagreg);
  }

  {
    sec_surface_t *fsurf[4];
    GetFlatSetToRender(sub, region, fsurf);
    //bool skipFloor = false;
    //bool skipCeiling = false;
    unsigned int floorFlag = FlagAsBoth;
    unsigned int ceilingFlag = FlagAsBoth;

    // skip sectors with height transfer for now
    if ((ssflag&FlagAsShadow) && (region->secregion->regflags&sec_region_t::RF_BaseRegion) && !sub->sector->heightsec) {
      unsigned disableflag = CheckShadowingFlats(sub);
      if (disableflag&FlatSectorShadowInfo::NoFloor) {
        //GCon->Logf(NAME_Debug, "dropping floor for sector #%d", (int)(ptrdiff_t)(sub->sector-&Level->Sectors[0]));
        //fsurf[0] = fsurf[1] = nullptr;
        //skipFloor = true;
        floorFlag = FlagAsLight;
      }
      if (disableflag&FlatSectorShadowInfo::NoCeiling) {
        //GCon->Logf(NAME_Debug, "dropping ceiling for sector #%d", (int)(ptrdiff_t)(sub->sector-&Level->Sectors[0]));
        //fsurf[2] = fsurf[3] = nullptr;
        //skipCeiling = true;
        ceilingFlag = FlagAsLight;
      }
    }

    // paper-thin surface shadow may disappear; workaround it
    #ifdef VV_SMAP_PAPETHIN_FIX
    bool paperThin = false;
    if (((ssflag&floorFlag)&FlagAsShadow) && fsurf[0] && fsurf[2]) {
      if (secregion->efloor.GetRealDist() == secregion->eceiling.GetRealDist()) {
        paperThin = true;
      }
    }
    #else
    # define paperThin false
    #endif

    if (fsurf[0]) CollectAdvLightSecSurface(fsurf[0], secregion->efloor.splane->SkyBox, ssflag&floorFlag, paperThin);
    if (fsurf[1]) CollectAdvLightSecSurface(fsurf[1], secregion->efloor.splane->SkyBox, ssflag&floorFlag, paperThin);

    if (fsurf[2]) CollectAdvLightSecSurface(fsurf[2], secregion->eceiling.splane->SkyBox, ssflag&ceilingFlag, paperThin);
    if (fsurf[3]) CollectAdvLightSecSurface(fsurf[3], secregion->eceiling.splane->SkyBox, ssflag&ceilingFlag, paperThin);
  }

  if (!nextFirst && region->next) return CollectAdvLightSubRegion(sub, region->next, ssflag);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::CollectAdvLightSubsector
//
//==========================================================================
void VRenderLevelShadowVolume::CollectAdvLightSubsector (int num) {
  vassert(num >= 0 && num < Level->NumSubsectors);
  subsector_t *sub = &Level->Subsectors[num];

  if (!sub->sector->linecount) return; // skip sectors containing original polyobjs

  // `LightBspVis` is already an intersection, no need to check `BspVis` here
  //if (!IsSubsectorLitBspVis(num) || !(BspVis[num>>3]&(1<<(num&7)))) return;

  unsigned int ssflagmask = FlagAsBoth;
  if (!LightClip.ClipLightCheckSubsector(sub, VViewClipper::AsLight)) ssflagmask &= ~FlagAsLight;
  if (!LightShadowClip.ClipLightCheckSubsector(sub, collectorShadowType)) ssflagmask &= ~FlagAsShadow;

  if (ssflagmask) {
    // update world
    if (sub->updateWorldFrame != updateWorldFrame) {
      sub->updateWorldFrame = updateWorldFrame;
      if (!r_disable_world_update) UpdateSubRegion(sub, sub->regions);
    }

    // if our light is in frustum, out-of-frustum subsectors are not interesting
    //FIXME: pass "need frustum check" flag to other functions
    unsigned int ssflag = (IsSubsectorLitBspVis(num) ? FlagAsBoth : FlagAsShadow)&ssflagmask;
    if ((ssflag&FlagAsShadow) && CurrLightInFrustum && !(BspVis[num>>3]&(1u<<(num&7)))) {
      // this subsector is invisible, check if it is in frustum (this was originally done for shadow)
      float bbox[6];
      // min
      bbox[0] = sub->bbox2d[BOX2D_LEFT];
      bbox[1] = sub->bbox2d[BOX2D_BOTTOM];
      bbox[2] = sub->sector->floor.minz;
      // max
      bbox[3] = sub->bbox2d[BOX2D_RIGHT];
      bbox[4] = sub->bbox2d[BOX2D_TOP];
      bbox[5] = sub->sector->ceiling.maxz;
      FixBBoxZ(bbox);
      if (!Drawer->viewfrustum.checkBox(bbox)) ssflag &= ~FlagAsShadow;
    }

    // render the polyobj in the subsector first, and add it to clipper
    // this blocks view with polydoors
    if (ssflag) {
      CollectAdvLightPolyObj(sub, ssflag);
      AddPolyObjToLightClipper(LightClip, sub, VViewClipper::AsLight);
      AddPolyObjToLightClipper(LightShadowClip, sub, collectorShadowType);
      CollectAdvLightSubRegion(sub, sub->regions, ssflag);
    }

    // add subsector's segs to the clipper
    // clipping against mirror is done only for vertical mirror planes
    if (ssflagmask&FlagAsLight) LightClip.ClipLightAddSubsectorSegs(sub, VViewClipper::AsLight);
    if (ssflagmask&FlagAsShadow) LightShadowClip.ClipLightAddSubsectorSegs(sub, collectorShadowType);
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::CollectAdvLightBSPNode
//
//  Renders all subsectors below a given node, traversing subtree
//  recursively. Just call with BSP root.
//
//==========================================================================
void VRenderLevelShadowVolume::CollectAdvLightBSPNode (int bspnum, const float *bbox) {
#ifdef VV_CLIPPER_FULL_CHECK
  if (LightClip.ClipIsFull() || LightShadowClip.ClipIsFull()) return;
#endif

  if (bbox && !LightClip.ClipLightIsBBoxVisible(bbox) && !LightShadowClip.ClipLightIsBBoxVisible(bbox)) return;
  //if (bbox && !CheckSphereVsAABBIgnoreZ(bbox, CurrLightPos, CurrLightRadius)) return;

  if (bspnum == -1) return CollectAdvLightSubsector(0);

  // found a subsector?
  if (BSPIDX_IS_NON_LEAF(bspnum)) {
    node_t *bsp = &Level->Nodes[bspnum];
    // decide which side the light is on
    const float dist = DotProduct(CurrLightPos, bsp->normal)-bsp->dist;
    if (dist >= CurrLightRadius) {
      // light is completely on front side
      return CollectAdvLightBSPNode(bsp->children[0], bsp->bbox[0]);
    } else if (dist <= -CurrLightRadius) {
      // light is completely on back side
      return CollectAdvLightBSPNode(bsp->children[1], bsp->bbox[1]);
    } else {
      //int side = bsp->PointOnSide(CurrLightPos);
      unsigned side = (unsigned)(dist <= 0.0f);
      // recursively divide front space
      CollectAdvLightBSPNode(bsp->children[side], bsp->bbox[side]);
      // possibly divide back space
      side ^= 1;
      return CollectAdvLightBSPNode(bsp->children[side], bsp->bbox[side]);
    }
  } else {
    return CollectAdvLightSubsector(BSPIDX_LEAF_SUBSECTOR(bspnum));
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderShadowSurfaceList
//
//  this is used only for shadow volumes
//
//==========================================================================
void VRenderLevelShadowVolume::RenderShadowSurfaceList () {
  // non-solid surfaces cannot cast shadows with shadow volumes
  for (auto &&surf : shadowSurfacesSolid) {
    Drawer->RenderSurfaceShadowVolume(surf, CurrLightPos, CurrLightRadius);
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightSurfaceList
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightSurfaceList () {
  Drawer->RenderSolidLightSurfaces(lightSurfacesSolid);
  Drawer->RenderMaskeLightSurfaces(lightSurfacesMasked);
}
