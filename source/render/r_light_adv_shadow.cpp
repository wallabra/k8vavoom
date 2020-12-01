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
// directly included from "r_light_adv.cpp"
//**************************************************************************

//==========================================================================
//
//  VRenderLevelShadowVolume::DrawShadowSurfaces
//
//  LightCanCross:
//    -1: two-sided line, top or bottom
//     0: one-sided line, or flat
//     1: two-sided line, middle
//
//==========================================================================
void VRenderLevelShadowVolume::DrawShadowSurfaces (surface_t *InSurfs, texinfo_t *texinfo,
                                                   VEntity *SkyBox, bool CheckSkyBoxAlways, int LightCanCross)
{
  if (!InSurfs) return;

  if (!texinfo || texinfo->Tex->Type == TEXTYPE_Null) return;
  if (texinfo->Alpha < 1.0f || texinfo->Additive) return;
  if (!r_shadowmaps.asBool() || !Drawer->CanRenderShadowMaps()) {
    if (LightCanCross > 0 && texinfo->Tex->isSeeThrough()) return; // has holes, don't bother
  }

  if (SkyBox && SkyBox->IsPortalDirty()) SkyBox = nullptr;

  if (texinfo->Tex == GTextureManager.getIgnoreAnim(skyflatnum) ||
      (CheckSkyBoxAlways && (SkyBox && SkyBox->GetSkyBoxAlways())))
  {
    return;
  }

  // ignore everything that is placed behind camera's back
  // we shouldn't have many of those, so check them in the loop below
  // but do this only if the light is in front of a camera
  //const bool checkFrustum = (r_advlight_opt_frustum_back && Drawer->viewfrustum.checkSphere(CurrLightPos, CurrLightRadius, TFrustum::NearBit));

  const bool smaps = r_shadowmaps.asBool() && Drawer->CanRenderShadowMaps();

  // TODO: if light is behind a camera, we can move back frustum plane, so it will
  //       contain light origin, and clip everything behind it. the same can be done
  //       for all other frustum planes.
  for (surface_t *surf = InSurfs; surf; surf = surf->next) {
    if (surf->count < 3) continue; // just in case

    // check transdoor hacks
    //if (surf->drawflags&surface_t::TF_TOPHACK) continue;

    // floor or ceiling? ignore translucent/masked
    if (LightCanCross < 0 || surf->GetNormalZ()) {
      VTexture *tex = surf->texinfo->Tex;
      if (!tex || tex->Type == TEXTYPE_Null) continue;
      if (surf->texinfo->Alpha < 1.0f || surf->texinfo->Additive) continue;
      if (!smaps && tex->isSeeThrough()) continue; // this is masked texture, shadow volumes cannot process it
    }

    // leave only surface that light can see (it shouldn't matter for texturing which one we'll use)
    const float dist = DotProduct(CurrLightPos, surf->GetNormal())-surf->GetDist();
    // k8: use `<=` and `>=` for radius checks, 'cause why not?
    //     light completely fades away at that distance
    if (dist <= 0.0f || dist >= CurrLightRadius) return; // light is too far away

    /*
    if (checkFrustum) {
      if (!Drawer->viewfrustum.checkVerts(surf->verts, (unsigned)surf->count, TFrustum::NearBit)) continue;
    }
    */

    if (!smaps) {
      Drawer->RenderSurfaceShadowVolume(surf, CurrLightPos, CurrLightRadius);
    } else {
      smapSurfaces.append(surf);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderShadowLine
//
//  Clips the given segment and adds any visible pieces to the line list.
//
//==========================================================================
void VRenderLevelShadowVolume::RenderShadowLine (subsector_t *sub, sec_region_t *secregion, drawseg_t *dseg) {
  seg_t *seg = dseg->seg;
  if (!seg->linedef) return; // miniseg

  // note that we don't want to filter out shadows that are behind
  // but we are want to filter out surfaces that cannot possibly block light
  // (i.e. back-surfaces with respect to light origin)
  const float dist = DotProduct(CurrLightPos, seg->normal)-seg->dist;
  //if (dist < -CurrLightRadius || dist > CurrLightRadius) return; // light is too far away
  //if (fabsf(dist) >= CurrLightRadius) return;
  if (dist <= 0.0f || dist >= CurrLightRadius) return;

  //k8: here we can call `ClipSegToLight()`, but i see no reasons to do so

  // ignore everything that is placed behind camera's back
  // we shouldn't have many of those, so check them in the loop below
  // but do this only if the light is in front of a camera
  if (CurrLightInFront) {
    if (!Drawer->viewfrustum.checkPoint(*seg->v1, TFrustum::NearBit) &&
        !Drawer->viewfrustum.checkPoint(*seg->v2, TFrustum::NearBit))
    {
      return;
    }
  }

  if (!LightClip.IsRangeVisible(*seg->v2, *seg->v1)) return;

  #ifdef VV_CHECK_1S_CAST_SHADOW
  if (!seg->backsector && !CheckCan1SCastShadow(seg->linedef)) {
    return;
  }
  #endif

#if 1
  // k8: this drops some segs that may leak without proper frustum culling
  // k8: this seems to be unnecessary now
  // k8: yet leave it there in the hope that it will reduce GPU overdrawing
  if (!LightClip.CheckSegFrustum(sub, seg)) return;
#endif

  VEntity *skybox = secregion->eceiling.splane->SkyBox;
  if (dseg->mid) DrawShadowSurfaces(dseg->mid->surfs, &dseg->mid->texinfo, skybox, false, (seg->backsector ? 1 : 0));
  if (seg->backsector) {
    // two sided line
    if (dseg->top) DrawShadowSurfaces(dseg->top->surfs, &dseg->top->texinfo, skybox, false, (seg->backsector ? -1 : 0));
    //k8: horizon/sky cannot block light
    //if (dseg->topsky) DrawShadowSurfaces(dseg->topsky->surfs, &dseg->topsky->texinfo, skybox, false, -1);
    if (dseg->bot) DrawShadowSurfaces(dseg->bot->surfs, &dseg->bot->texinfo, skybox, false, (seg->backsector ? -1 : 0));
    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      DrawShadowSurfaces(sp->surfs, &sp->texinfo, skybox, false, (seg->backsector ? -1 : 0));
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderShadowSecSurface
//
//  this is used for floor and ceilings
//
//==========================================================================
void VRenderLevelShadowVolume::RenderShadowSecSurface (sec_surface_t *ssurf, VEntity *SkyBox) {
  //const sec_plane_t &plane = *ssurf->secplane;
  if (!ssurf->esecplane.splane->pic) return;

  // note that we don't want to filter out shadows that are behind
  // but we are want to filter out surfaces that cannot possibly block light
  // (i.e. back-surfaces with respect to light origin)
  //const float dist = DotProduct(CurrLightPos, plane.normal)-plane.dist;
  const float dist = ssurf->PointDist(CurrLightPos);
  //if (dist < -CurrLightRadius || dist > CurrLightRadius) return; // light is too far away
  //if (fabsf(dist) >= CurrLightRadius) return;
  if (dist <= 0.0f || dist >= CurrLightRadius) return;

  // we can do "CurrLightInFront check" here, but meh...

  DrawShadowSurfaces(ssurf->surfs, &ssurf->texinfo, SkyBox, true, 0);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderShadowPolyObj
//
//==========================================================================
void VRenderLevelShadowVolume::RenderShadowPolyObj (subsector_t *sub) {
  if (sub && sub->HasPObjs() && r_draw_pobj) {
    subregion_t *region = sub->regions;
    sec_region_t *secregion = region->secregion;
    for (auto &&it : sub->PObjFirst()) {
      polyobj_t *pobj = it.value();
      seg_t **polySeg = pobj->segs;
      for (int polyCount = pobj->numsegs; polyCount--; ++polySeg) {
        RenderShadowLine(sub, secregion, (*polySeg)->drawsegs);
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderShadowSubRegion
//
//  Determine floor/ceiling planes.
//  Draw one or more line segments.
//
//==========================================================================
void VRenderLevelShadowVolume::RenderShadowSubRegion (subsector_t *sub, subregion_t *region) {
  // no reason to sort flats here
  //const bool nextFirst = NeedToRenderNextSubFirst(region);
  //if (nextFirst) RenderShadowSubRegion(sub, region->next);

  /*
    note that we can throw away main floors and ceilings (i.e. for the base region),
    but only if this subsector either doesn't have any lines (i.e. consists purely of minisegs),
    nope: not any such subregion; just avoid checking neighbouring sectors if shared line
          cannot be touched by the light
    or:
      drop floor if all neighbour floors are higher or equal (we cannot cast any shadow to them),
      drop ceiling if all neighbour ceilings are lower or equal (we cannot cast any shadow to them)

    this is done by `CheckShadowingFlats()`
   */

  for (; region; region = region->next) {
    sec_region_t *secregion = region->secregion;

    if (!clip_adv_regions_shadow || LightClip.ClipLightCheckRegion(region, sub, true)) {
      drawseg_t *ds = region->lines;
      for (int count = sub->numlines; count--; ++ds) RenderShadowLine(sub, secregion, ds);
    }

    {
      sec_surface_t *fsurf[4];
      GetFlatSetToRender(sub, region, fsurf);

      // skip sectors with height transfer for now
      if ((region->secregion->regflags&sec_region_t::RF_BaseRegion) && !sub->sector->heightsec) {
        unsigned disableflag = CheckShadowingFlats(sub);
        if (disableflag&FlatSectorShadowInfo::NoFloor) {
          //GCon->Logf(NAME_Debug, "dropping floor for sector #%d", (int)(ptrdiff_t)(sub->sector-&Level->Sectors[0]));
          fsurf[0] = fsurf[1] = nullptr;
        }
        if (disableflag&FlatSectorShadowInfo::NoCeiling) {
          //GCon->Logf(NAME_Debug, "dropping ceiling for sector #%d", (int)(ptrdiff_t)(sub->sector-&Level->Sectors[0]));
          fsurf[2] = fsurf[3] = nullptr;
        }
      }

      if (fsurf[0]) RenderShadowSecSurface(fsurf[0], secregion->efloor.splane->SkyBox);
      if (fsurf[1]) RenderShadowSecSurface(fsurf[1], secregion->efloor.splane->SkyBox);

      if (fsurf[2]) RenderShadowSecSurface(fsurf[2], secregion->/*efloor*/eceiling.splane->SkyBox);
      if (fsurf[3]) RenderShadowSecSurface(fsurf[3], secregion->eceiling.splane->SkyBox);
    }
  }

  //if (!nextFirst && region->next) return RenderShadowSubRegion(sub, region->next);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderShadowSubsector
//
//==========================================================================
void VRenderLevelShadowVolume::RenderShadowSubsector (int num) {
  subsector_t *sub = &Level->Subsectors[num];

  // don't do this check for shadows
  //if (!IsSubsectorLitBspVis(num) || !(BspVis[num>>3]&(1<<(num&7)))) return;

  if (!sub->sector->linecount) return; // skip sectors containing original polyobjs

  if (LightClip.ClipLightCheckSubsector(sub, true)) {
    // if our light is in frustum, out-of-frustum subsectors are not interesting
    //FIXME: pass "need frustum check" flag to other functions
    bool needToRender = true;
    if (CurrLightInFrustum && !(BspVis[num>>3]&(1u<<(num&7)))) {
      // this subsector is invisible, check if it is in frustum
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
      needToRender = Drawer->viewfrustum.checkBox(bbox);
    }

    if (needToRender) {
      // update world
      if (sub->updateWorldFrame != updateWorldFrame) {
        sub->updateWorldFrame = updateWorldFrame;
        if (!r_disable_world_update) UpdateSubRegion(sub, sub->regions);
      }
      // render the polyobj in the subsector first, and add it to clipper
      // this blocks view with polydoors
      RenderShadowPolyObj(sub);
      AddPolyObjToLightClipper(LightClip, sub, true);
      RenderShadowSubRegion(sub, sub->regions);
    }
  }

  // add subsector's segs to the clipper
  // clipping against mirror is done only for vertical mirror planes
  LightClip.ClipLightAddSubsectorSegs(sub, true);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderShadowBSPNode
//
//  Renders all subsectors below a given node, traversing subtree
//  recursively. Just call with BSP root.
//
//==========================================================================
void VRenderLevelShadowVolume::RenderShadowBSPNode (int bspnum, const float *bbox, bool LimitLights) {
  if (LimitLights) {
    if (r_max_shadow_segs_all >= 0 && AllShadowsNumber > r_max_shadow_segs_all) return;
    if (r_max_shadow_segs_one >= 0 && CurrShadowsNumber > r_max_shadow_segs_one) return;
  }

#ifdef VV_CLIPPER_FULL_CHECK
  if (LightClip.ClipIsFull()) return;
#endif

  if (!LightClip.ClipLightIsBBoxVisible(bbox)) return;

  if (bspnum == -1) {
    if (LimitLights) { ++CurrShadowsNumber; ++AllShadowsNumber; }
    return RenderShadowSubsector(0);
  }

  // found a subsector?
  if (BSPIDX_IS_NON_LEAF(bspnum)) {
    node_t *bsp = &Level->Nodes[bspnum];
    // decide which side the light is on
    const float dist = DotProduct(CurrLightPos, bsp->normal)-bsp->dist;
    if (dist >= CurrLightRadius) {
      // light is completely on front side
      return RenderShadowBSPNode(bsp->children[0], bsp->bbox[0], LimitLights);
    } else if (dist <= -CurrLightRadius) {
      // light is completely on back side
      return RenderShadowBSPNode(bsp->children[1], bsp->bbox[1], LimitLights);
    } else {
      //int side = bsp->PointOnSide(CurrLightPos);
      unsigned side = (unsigned)(dist <= 0.0f);
      // recursively divide front space
      RenderShadowBSPNode(bsp->children[side], bsp->bbox[side], LimitLights);
      // always divide back space for shadows
      side ^= 1;
      return RenderShadowBSPNode(bsp->children[side], bsp->bbox[side], LimitLights);
    }
  } else {
    if (LimitLights) { ++CurrShadowsNumber; ++AllShadowsNumber; }
    return RenderShadowSubsector(BSPIDX_LEAF_SUBSECTOR(bspnum));
  }
}
