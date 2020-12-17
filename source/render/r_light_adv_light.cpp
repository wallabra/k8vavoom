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


static VCvarB clip_adv_regions_light("clip_adv_regions_light", false, "Clip (1D) light regions?", CVAR_PreInit);


//==========================================================================
//
//  VRenderLevelShadowVolume::DrawLightSurfaces
//
//  LightCanCross:
//    -1: two-sided line, top or bottom
//     0: one-sided line, or flat
//     1: two-sided line, middle
//
//==========================================================================
void VRenderLevelShadowVolume::DrawLightSurfaces (surface_t *InSurfs, texinfo_t *texinfo,
                                                  VEntity *SkyBox, bool CheckSkyBoxAlways, int LightCanCross)
{
  if (!InSurfs) return;

  if (!texinfo || texinfo->Tex->Type == TEXTYPE_Null) return;
  if (texinfo->Alpha < 1.0f || texinfo->Additive) return;

  if (SkyBox && SkyBox->IsPortalDirty()) SkyBox = nullptr;

  if (texinfo->Tex == GTextureManager.getIgnoreAnim(skyflatnum) ||
      (CheckSkyBoxAlways && (SkyBox && SkyBox->GetSkyBoxAlways())))
  {
    return;
  }

  for (surface_t *surf = InSurfs; surf; surf = surf->next) {
    if (surf->count < 3) continue; // just in case
    if (!surf->IsPlVisible()) continue; // viewer is in back side or on plane
    const float dist = DotProduct(CurrLightPos, surf->GetNormal())-surf->GetDist();
    if (dist <= 0.0f || dist >= CurrLightRadius) continue; // light is too far away, or surface is not lit
    // ignore masked
    VTexture *tex = surf->texinfo->Tex;
    if (!tex || tex->Type == TEXTYPE_Null) continue;
    if (surf->texinfo->Alpha < 1.0f || surf->texinfo->Additive) continue;
    if (tex->isTranslucent()) continue; // this is translucent texture
    Drawer->DrawSurfaceLight(surf);
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightLine
//
//  clips the given segment and adds any visible pieces to the line list
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightLine (sec_region_t *secregion, drawseg_t *dseg) {
  const seg_t *seg = dseg->seg;
  if (!seg->linedef) return; // miniseg

  const float dist = DotProduct(CurrLightPos, seg->normal)-seg->dist;
  //if (dist <= -CurrLightRadius || dist > CurrLightRadius) return; // light sphere is not touching a plane
  if (fabsf(dist) >= CurrLightRadius) return;

  //k8: here we can call `ClipSegToLight()`, but i see no reasons to do so
  if (!LightClip.IsRangeVisible(*seg->v2, *seg->v1)) return;

  VEntity *skybox = secregion->eceiling.splane->SkyBox;
  if (dseg->mid) DrawLightSurfaces(dseg->mid->surfs, &dseg->mid->texinfo, skybox, false, (seg->backsector ? 1 : 0));
  if (seg->backsector) {
    // two sided line
    if (dseg->top) DrawLightSurfaces(dseg->top->surfs, &dseg->top->texinfo, skybox, false, (seg->backsector ? -1 : 0));
    //k8: horizon/sky cannot block light
    //if (dseg->topsky) DrawLightSurfaces(dseg->topsky->surfs, &dseg->topsky->texinfo, skybox, false, -1);
    if (dseg->bot) DrawLightSurfaces(dseg->bot->surfs, &dseg->bot->texinfo, skybox, false, (seg->backsector ? -1 : 0));
    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      DrawLightSurfaces(sp->surfs, &sp->texinfo, skybox, false, (seg->backsector ? -1 : 0));
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightSecSurface
//
//  this is used for floor and ceilings
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightSecSurface (sec_surface_t *ssurf, VEntity *SkyBox) {
  //const sec_plane_t &plane = *ssurf->secplane;
  if (!ssurf->esecplane.splane->pic) return;

  //const float dist = DotProduct(CurrLightPos, plane.normal)-plane.dist;
  const float dist = ssurf->PointDist(CurrLightPos);
  //if (dist <= -CurrLightRadius || dist > CurrLightRadius) return; // light is in back side or on plane
  if (fabsf(dist) >= CurrLightRadius) return;

  DrawLightSurfaces(ssurf->surfs, &ssurf->texinfo, SkyBox, true, 0);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightPolyObj
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightPolyObj (subsector_t *sub) {
  if (sub && sub->HasPObjs() && r_draw_pobj) {
    subregion_t *region = sub->regions;
    sec_region_t *secregion = region->secregion;
    for (auto &&it : sub->PObjFirst()) {
      polyobj_t *pobj = it.value();
      seg_t **polySeg = pobj->segs;
      for (int polyCount = pobj->numsegs; polyCount--; ++polySeg) {
        RenderLightLine(secregion, (*polySeg)->drawsegs);
      }
    }
  }
}

//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightSubRegion
//
//  Determine floor/ceiling planes.
//  Draw one or more line segments.
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightSubRegion (subsector_t *sub, subregion_t *region) {
  const bool nextFirst = NeedToRenderNextSubFirst(region);
  if (nextFirst) RenderLightSubRegion(sub, region->next);

  sec_region_t *secregion = region->secregion;

  if (!clip_adv_regions_light || LightClip.ClipLightCheckRegion(region, sub, false)) {
    drawseg_t *ds = region->lines;
    for (int count = sub->numlines; count--; ++ds) RenderLightLine(secregion, ds);
  }

  {
    sec_surface_t *fsurf[4];
    GetFlatSetToRender(sub, region, fsurf);

    if (fsurf[0]) RenderLightSecSurface(fsurf[0], secregion->efloor.splane->SkyBox);
    if (fsurf[1]) RenderLightSecSurface(fsurf[1], secregion->efloor.splane->SkyBox);

    if (fsurf[2]) RenderLightSecSurface(fsurf[2], secregion->eceiling.splane->SkyBox);
    if (fsurf[3]) RenderLightSecSurface(fsurf[3], secregion->eceiling.splane->SkyBox);
  }

  if (!nextFirst && region->next) return RenderLightSubRegion(sub, region->next);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightSubsector
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightSubsector (int num) {
  vassert(num >= 0 && num < Level->NumSubsectors);
  subsector_t *sub = &Level->Subsectors[num];

  if (!sub->sector->linecount) return; // skip sectors containing original polyobjs

  // `LightBspVis` is already an intersection, no need to check `BspVis` here
  //if (!IsSubsectorLitBspVis(num) || !(BspVis[num>>3]&(1<<(num&7)))) return;

  if (IsSubsectorLitBspVis(num)) {
    if (LightClip.ClipLightCheckSubsector(sub, false)) {
      // update world
      if (sub->updateWorldFrame != updateWorldFrame) {
        sub->updateWorldFrame = updateWorldFrame;
        if (!r_disable_world_update) UpdateSubRegion(sub, sub->regions);
      }
      // render the polyobj in the subsector first, and add it to clipper
      // this blocks view with polydoors
      RenderLightPolyObj(sub);
      AddPolyObjToLightClipper(LightClip, sub, false);
      RenderLightSubRegion(sub, sub->regions);
    }
  }

  // add subsector's segs to the clipper
  // clipping against mirror is done only for vertical mirror planes
  LightClip.ClipLightAddSubsectorSegs(sub, false);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightBSPNode
//
//  Renders all subsectors below a given node, traversing subtree
//  recursively. Just call with BSP root.
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightBSPNode (int bspnum, const float *bbox) {
#ifdef VV_CLIPPER_FULL_CHECK
  if (LightClip.ClipIsFull()) return;
#endif

  if (!LightClip.ClipLightIsBBoxVisible(bbox)) return;
  //if (!CheckSphereVsAABBIgnoreZ(bbox, CurrLightPos, CurrLightRadius)) return;

  if (bspnum == -1) return RenderLightSubsector(0);

  // found a subsector?
  if (BSPIDX_IS_NON_LEAF(bspnum)) {
    node_t *bsp = &Level->Nodes[bspnum];
    // decide which side the light is on
    const float dist = DotProduct(CurrLightPos, bsp->normal)-bsp->dist;
    if (dist >= CurrLightRadius) {
      // light is completely on front side
      return RenderLightBSPNode(bsp->children[0], bsp->bbox[0]);
    } else if (dist <= -CurrLightRadius) {
      // light is completely on back side
      return RenderLightBSPNode(bsp->children[1], bsp->bbox[1]);
    } else {
      //int side = bsp->PointOnSide(CurrLightPos);
      unsigned side = (unsigned)(dist <= 0.0f);
      // recursively divide front space
      RenderLightBSPNode(bsp->children[side], bsp->bbox[side]);
      // possibly divide back space
      side ^= 1;
      return RenderLightBSPNode(bsp->children[side], bsp->bbox[side]);
    }
  } else {
    return RenderLightSubsector(BSPIDX_LEAF_SUBSECTOR(bspnum));
  }
}
