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
//**  BSP traversal, handling of LineSegs for rendering.
//**
//**************************************************************************
#include "gamedefs.h"
#include "r_local.h"

#define HORIZON_SURF_SIZE (sizeof(surface_t) + sizeof(TVec) * 3)

//#define VRBSP_DISABLE_SKY_PORTALS


static VCvarI r_maxmirrors("r_maxmirrors", "1", "Maximum allowed mirrors.", CVAR_Archive);
VCvarI r_max_portal_depth("r_max_portal_depth", "1", "Maximum allowed portal depth (-1: infinite)", CVAR_Archive);
static VCvarB r_allow_horizons("r_allow_horizons", true, "Allow horizon portal rendering?", CVAR_Archive);
static VCvarB r_allow_mirrors("r_allow_mirrors", true, "Allow mirror portal rendering (SLOW)?", CVAR_Archive);

static VCvarB r_disable_sky_portals("r_disable_sky_portals", false, "Disable rendering of sky portals.", 0/*CVAR_Archive*/);

static VCvarB r_steamline_masked_walls("r_steamline_masked_walls", true, "Render masked (two-sided) walls as normal ones.", CVAR_Archive);

static VCvarB dbg_max_portal_depth_warning("dbg_max_portal_depth_warning", false, "Show maximum allowed portal depth warning?", 0/*CVAR_Archive*/);

VCvarB VRenderLevelShared::times_render_highlevel("times_render_highlevel", false, "Show high-level render times.", 0/*CVAR_Archive*/);
VCvarB VRenderLevelShared::times_render_lowlevel("times_render_lowlevel", false, "Show low-level render times.", 0/*CVAR_Archive*/);
VCvarB VRenderLevelShared::r_disable_world_update("r_disable_world_update", false, "Disable world updates.", 0/*CVAR_Archive*/);

extern int light_reset_surface_cache; // in r_light_reg.cpp
extern VCvarB r_decals_enabled;
extern VCvarB r_draw_adjacent_subsector_things;
extern VCvarB w_update_in_renderer;
extern VCvarB clip_frustum;
extern VCvarB clip_frustum_bsp;

// to clear portals
static bool oldMirrors = true;
static bool oldHorizons = true;
static int oldMaxMirrors = -666;
static int oldPortalDepth = -666;


//==========================================================================
//
//  VRenderLevelShared::QueueSimpleSurf
//
//==========================================================================
void VRenderLevelShared::QueueSimpleSurf (seg_t *seg, surface_t *surf) {
  surf->dcseg = seg;
  if (SimpleSurfsTail) {
    SimpleSurfsTail->DrawNext = surf;
    SimpleSurfsTail = surf;
  } else {
    SimpleSurfsHead = surf;
    SimpleSurfsTail = surf;
  }
  surf->DrawNext = nullptr;
}


//==========================================================================
//
//  VRenderLevelShared::QueueSkyPortal
//
//==========================================================================
void VRenderLevelShared::QueueSkyPortal (surface_t *surf) {
  surf->dcseg = nullptr;
  if (SkyPortalsTail) {
    SkyPortalsTail->DrawNext = surf;
    SkyPortalsTail = surf;
  } else {
    SkyPortalsHead = surf;
    SkyPortalsTail = surf;
  }
  surf->DrawNext = nullptr;
}


//==========================================================================
//
//  VRenderLevelShared::QueueHorizonPortal
//
//==========================================================================
void VRenderLevelShared::QueueHorizonPortal (surface_t *surf) {
  surf->dcseg = nullptr;
  if (HorizonPortalsTail) {
    HorizonPortalsTail->DrawNext = surf;
    HorizonPortalsTail = surf;
  } else {
    HorizonPortalsHead = surf;
    HorizonPortalsTail = surf;
  }
  surf->DrawNext = nullptr;
}


//==========================================================================
//
//  VRenderLevelShared::DrawSurfaces
//
//==========================================================================
void VRenderLevelShared::DrawSurfaces (seg_t *seg, surface_t *InSurfs, texinfo_t *texinfo,
  VEntity *SkyBox, int LightSourceSector, int SideLight, bool AbsSideLight,
  bool CheckSkyBoxAlways)
{
  surface_t *surfs = InSurfs;
  if (!surfs) return;

  if (texinfo->Tex->Type == TEXTYPE_Null) return;

  sec_params_t *LightParams = (LightSourceSector == -1 ? r_region->params : &Level->Sectors[LightSourceSector].params);
  int lLev = (AbsSideLight ? 0 : LightParams->lightlevel)+SideLight;
  lLev = (FixedLight ? FixedLight : lLev+ExtraLight);
  lLev = MID(0, lLev, 255);
  if (r_darken) lLev = light_remap[lLev];
  vuint32 Fade = GetFade(r_region);

  if (SkyBox && (SkyBox->EntityFlags&VEntity::EF_FixedModel)) SkyBox = nullptr;
  bool IsStack = SkyBox && SkyBox->eventSkyBoxGetAlways();
  if (texinfo->Tex == GTextureManager[skyflatnum] || (IsStack && CheckSkyBoxAlways)) { //k8: i hope that the parens are right here
    VSky *Sky = nullptr;
    if (!SkyBox && (r_sub->sector->Sky&SKY_FROM_SIDE) != 0) {
      int Tex;
      bool Flip;
      if (r_sub->sector->Sky == SKY_FROM_SIDE) {
        Tex = Level->LevelInfo->Sky2Texture;
        Flip = true;
      } else {
        side_t *Side = &Level->Sides[(r_sub->sector->Sky&(SKY_FROM_SIDE-1))-1];
        Tex = Side->TopTexture;
        Flip = !!Level->Lines[Side->LineNum].arg3;
      }
      if (GTextureManager[Tex]->Type != TEXTYPE_Null) {
        for (int i = 0; i < SideSkies.Num(); ++i) {
          if (SideSkies[i]->SideTex == Tex && SideSkies[i]->SideFlip == Flip) {
            Sky = SideSkies[i];
            break;
          }
        }
        if (!Sky) {
          Sky = new VSky;
          Sky->Init(Tex, Tex, 0, 0, false, !!(Level->LevelInfo->LevelInfoFlags&VLevelInfo::LIF_ForceNoSkyStretch), Flip, false);
          SideSkies.Append(Sky);
        }
      }
    }

    if (!Sky && !SkyBox) {
      InitSky();
      Sky = &BaseSky;
    }

    VPortal *Portal = nullptr;
    if (SkyBox) {
      for (int i = 0; i < Portals.Num(); ++i) {
        if (Portals[i] && Portals[i]->MatchSkyBox(SkyBox)) {
          Portal = Portals[i];
          break;
        }
      }
      if (!Portal) {
        if (IsStack) {
          Portal = new VSectorStackPortal(this, SkyBox);
          Portals.Append(Portal);
        } else {
#if !defined(VRBSP_DISABLE_SKY_PORTALS)
          if (!r_disable_sky_portals) {
            Portal = new VSkyBoxPortal(this, SkyBox);
            Portals.Append(Portal);
          }
#endif
        }
      }
    } else {
      for (int i = 0; i < Portals.Num(); ++i) {
        if (Portals[i] && Portals[i]->MatchSky(Sky)) {
          Portal = Portals[i];
          break;
        }
      }
#if !defined(VRBSP_DISABLE_SKY_PORTALS)
      if (!Portal && !r_disable_sky_portals) {
        Portal = new VSkyPortal(this, Sky);
        Portals.Append(Portal);
      }
#endif
    }
    if (!Portal) return;
    //GCon->Log("----");
    int doRenderSurf = -1;
    do {
      if (surfs->plane->PointOnSide(vieworg)) {
        // viewer is in back side or on plane
        //GCon->Logf("  SURF SKIP!");
        continue;
      }
      Portal->Surfs.Append(surfs);
      if (doRenderSurf < 0) {
        doRenderSurf = (IsStack && CheckSkyBoxAlways && SkyBox->eventSkyBoxGetPlaneAlpha() ? 1 : 0);
      }
      //if (IsStack && CheckSkyBoxAlways && SkyBox->eventSkyBoxGetPlaneAlpha())
      if (doRenderSurf) {
        //GCon->Logf("  SURF!");
        surfs->Light = (lLev<<24)|LightParams->LightColour;
        surfs->Fade = Fade;
        surfs->dlightframe = r_sub->dlightframe;
        surfs->dlightbits = r_sub->dlightbits;
        surfs->dcseg = nullptr; // sky cannot have decals anyway
        DrawTranslucentPoly(surfs, surfs->verts, surfs->count,
          0, SkyBox->eventSkyBoxGetPlaneAlpha(), false, 0,
          false, 0, 0, TVec(), 0, TVec(), TVec(), TVec());
      }
      surfs = surfs->next;
    } while (surfs);
    return;
  } // done skybox rendering

  do {
    if (surfs->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane

    surfs->Light = (lLev<<24)|LightParams->LightColour;
    surfs->Fade = Fade;
    surfs->dlightframe = r_sub->dlightframe;
    surfs->dlightbits = r_sub->dlightbits;

    if (texinfo->Alpha > 1.0f || (r_steamline_masked_walls && texinfo->Alpha >= 1.0f)) {
      if (PortalLevel == 0) {
        world_surf_t &S = WorldSurfs.Alloc();
        S.Surf = surfs;
        S.Type = 0;
        surfs->dcseg = seg;
      } else {
        QueueWorldSurface(seg, surfs);
      }
    } else {
      //surfs->dcseg = nullptr;
      surfs->dcseg = seg; // allow decals on masked polys
      DrawTranslucentPoly(surfs, surfs->verts, surfs->count,
        0, texinfo->Alpha, texinfo->Additive, 0, false, 0, 0,
        TVec(), 0, TVec(), TVec(), TVec());
    }
    surfs = surfs->next;
  } while (surfs);
}


//==========================================================================
//
//  VRenderLevelShared::RenderHorizon
//
//==========================================================================
void VRenderLevelShared::RenderHorizon (drawseg_t *dseg) {
  seg_t *Seg = dseg->seg;

  if (!dseg->HorizonTop) {
    dseg->HorizonTop = (surface_t *)Z_Malloc(HORIZON_SURF_SIZE);
    dseg->HorizonBot = (surface_t *)Z_Malloc(HORIZON_SURF_SIZE);
    memset((void *)dseg->HorizonTop, 0, HORIZON_SURF_SIZE);
    memset((void *)dseg->HorizonBot, 0, HORIZON_SURF_SIZE);
  }

  // horizon is not supported in sectors with slopes, so just use TexZ
  float TopZ = r_region->ceiling->TexZ;
  float BotZ = r_region->floor->TexZ;
  float HorizonZ = vieworg.z;

  // handle top part
  if (TopZ > HorizonZ) {
    sec_surface_t *Ceil = r_subregion->ceil;

    // calculate light and fade
    sec_params_t *LightParams = Ceil->secplane->LightSourceSector != -1 ?
      &Level->Sectors[Ceil->secplane->LightSourceSector].params :
      r_region->params;
    int lLev = (FixedLight ? FixedLight : MIN(255, LightParams->lightlevel+ExtraLight));
    if (r_darken) lLev = light_remap[lLev];
    vuint32 Fade = GetFade(r_region);

    surface_t *Surf = dseg->HorizonTop;
    Surf->plane = dseg->seg;
    Surf->texinfo = &Ceil->texinfo;
    Surf->HorizonPlane = Ceil->secplane;
    Surf->Light = (lLev << 24) | LightParams->LightColour;
    Surf->Fade = Fade;
    Surf->count = 4;
    TVec *svs = &Surf->verts[0];
    svs[0] = *Seg->v1; svs[0].z = MAX(BotZ, HorizonZ);
    svs[1] = *Seg->v1; svs[1].z = TopZ;
    svs[2] = *Seg->v2; svs[2].z = TopZ;
    svs[3] = *Seg->v2; svs[3].z = MAX(BotZ, HorizonZ);
    if (Ceil->secplane->pic == skyflatnum) {
      // if it's a sky, render it as a regular sky surface
      DrawSurfaces(nullptr, Surf, &Ceil->texinfo, r_region->ceiling->SkyBox, -1,
        Seg->sidedef->Light, !!(Seg->sidedef->Flags & SDF_ABSLIGHT),
        false);
    } else {
      if (PortalLevel == 0) {
        world_surf_t &S = WorldSurfs.Alloc();
        S.Surf = Surf;
        S.Type = 2;
        Surf->dcseg = nullptr;
      } else {
        QueueHorizonPortal(Surf);
      }
    }
  }

  // handle bottom part
  if (BotZ < HorizonZ) {
    sec_surface_t *Floor = r_subregion->floor;

    // calculate light and fade
    sec_params_t *LightParams = Floor->secplane->LightSourceSector != -1 ?
      &Level->Sectors[Floor->secplane->LightSourceSector].params :
      r_region->params;
    int lLev = (FixedLight ? FixedLight : MIN(255, LightParams->lightlevel+ExtraLight));
    if (r_darken) lLev = light_remap[lLev];
    vuint32 Fade = GetFade(r_region);

    surface_t *Surf = dseg->HorizonBot;
    Surf->plane = dseg->seg;
    Surf->texinfo = &Floor->texinfo;
    Surf->HorizonPlane = Floor->secplane;
    Surf->Light = (lLev << 24) | LightParams->LightColour;
    Surf->Fade = Fade;
    Surf->count = 4;
    TVec *svs = &Surf->verts[0];
    svs[0] = *Seg->v1; svs[0].z = BotZ;
    svs[1] = *Seg->v1; svs[1].z = MIN(TopZ, HorizonZ);
    svs[2] = *Seg->v2; svs[2].z = MIN(TopZ, HorizonZ);
    svs[3] = *Seg->v2; svs[3].z = BotZ;
    if (Floor->secplane->pic == skyflatnum) {
      // if it's a sky, render it as a regular sky surface
      DrawSurfaces(nullptr, Surf, &Floor->texinfo, r_region->floor->SkyBox, -1,
        Seg->sidedef->Light, !!(Seg->sidedef->Flags & SDF_ABSLIGHT),
        false);
    } else {
      if (PortalLevel == 0) {
        world_surf_t &S = WorldSurfs.Alloc();
        S.Surf = Surf;
        S.Type = 2;
        Surf->dcseg = nullptr;
      } else {
        QueueHorizonPortal(Surf);
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderMirror
//
//==========================================================================
void VRenderLevelShared::RenderMirror (drawseg_t *dseg) {
  seg_t *Seg = dseg->seg;
  if (MirrorLevel < r_maxmirrors && r_allow_mirrors) {
    VPortal *Portal = nullptr;
    for (int i = 0; i < Portals.Num(); ++i) {
      if (Portals[i] && Portals[i]->MatchMirror(Seg)) {
        Portal = Portals[i];
        break;
      }
    }
    if (!Portal) {
      Portal = new VMirrorPortal(this, Seg);
      Portals.Append(Portal);
    }

    surface_t *surfs = dseg->mid->surfs;
    do {
      Portal->Surfs.Append(surfs);
      surfs = surfs->next;
    } while (surfs);
  } else {
    DrawSurfaces(Seg, dseg->mid->surfs, &dseg->mid->texinfo,
      r_region->ceiling->SkyBox, -1, Seg->sidedef->Light,
      !!(Seg->sidedef->Flags & SDF_ABSLIGHT), false);
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderLine
//
//  Clips the given segment and adds any visible pieces to the line list.
//
//==========================================================================
void VRenderLevelShared::RenderLine (drawseg_t *dseg) {
  seg_t *seg = dseg->seg;
  line_t *linedef = seg->linedef;

  if (!linedef) return; // miniseg

  if (seg->PointOnSide(vieworg)) return; // viewer is in back side or on plane

  if (MirrorClipSegs && clip_frustum && clip_frustum_bsp && view_frustum.planes[5].isValid()) {
    // clip away segs that are behind mirror
    if (view_frustum.planes[5].PointOnSide(*seg->v1) && view_frustum.planes[5].PointOnSide(*seg->v2)) return; // behind mirror
  }

/*
    k8: i don't know what Janis wanted to accomplish with this, but it actually
        makes clipping WORSE due to limited precision
  if (seg->backsector) {
    // just apply this to sectors without slopes
    if (seg->frontsector->floor.normal.z == 1.0f && seg->backsector->floor.normal.z == 1.0f &&
        seg->frontsector->ceiling.normal.z == -1.0f && seg->backsector->ceiling.normal.z == -1.0f)
    {
      // clip sectors that are behind rendered segs
      TVec v1 = *seg->v1;
      TVec v2 = *seg->v2;
      TVec r1 = vieworg-v1;
      TVec r2 = vieworg-v2;
      float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), vieworg);
      float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), vieworg);

      // there might be a better method of doing this, but this one works for now...
           if (D1 > 0.0f && D2 < 0.0f) v2 += ((v2-v1)*D1)/(D1-D2);
      else if (D2 > 0.0f && D1 < 0.0f) v1 += ((v2-v1)*D1)/(D2-D1);

      if (!ViewClip.IsRangeVisible(ViewClip.PointToClipAngle(v2), ViewClip.PointToClipAngle(v1))) return;
    }
  } else {
    // clip sectors that are behind rendered segs
    TVec v1 = *seg->v1;
    TVec v2 = *seg->v2;
    TVec r1 = vieworg-v1;
    TVec r2 = vieworg-v2;
    float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), vieworg);
    float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), vieworg);

    // there might be a better method of doing this, but this one works for now...
         if (D1 > 0.0f && D2 < 0.0f) v2 += ((v2-v1)*D1)/(D1-D2);
    else if (D2 > 0.0f && D1 < 0.0f) v1 += ((v2-v1)*D1)/(D2-D1);

    if (!ViewClip.IsRangeVisible(ViewClip.PointToClipAngle(v2), ViewClip.PointToClipAngle(v1))) return;
  }
*/
  if (!ViewClip.IsRangeVisible(*seg->v2, *seg->v1)) return;

  // k8: this drops some segs that may leak without proper frustum culling
  if (!ViewClip.CheckSegFrustum(seg)) return;

  //FIXME this marks all lines
  // mark the segment as visible for auto map
  //linedef->flags |= ML_MAPPED;
  if (!(linedef->flags&ML_MAPPED)) {
    // this line is at least partially mapped; let automap drawer do the rest
    linedef->exFlags |= ML_EX_PARTIALLY_MAPPED|ML_EX_CHECK_MAPPED;
  }
  seg->flags |= SF_MAPPED;

  side_t *sidedef = seg->sidedef;

  if (!seg->backsector) {
    // single sided line
    if (seg->linedef->special == LNSPEC_LineHorizon && r_allow_horizons) {
      RenderHorizon(dseg);
    } else if (seg->linedef->special == LNSPEC_LineMirror) {
      RenderMirror(dseg);
    } else {
      DrawSurfaces(seg, dseg->mid->surfs, &dseg->mid->texinfo,
        r_region->ceiling->SkyBox, -1, sidedef->Light,
        !!(sidedef->Flags & SDF_ABSLIGHT), false);
    }
    DrawSurfaces(nullptr, dseg->topsky->surfs, &dseg->topsky->texinfo,
      r_region->ceiling->SkyBox, -1, sidedef->Light,
      !!(sidedef->Flags & SDF_ABSLIGHT), false);
  } else {
    // two sided line
    DrawSurfaces(seg, dseg->top->surfs, &dseg->top->texinfo,
      r_region->ceiling->SkyBox, -1, sidedef->Light,
      !!(sidedef->Flags&SDF_ABSLIGHT), false);
    DrawSurfaces(nullptr, dseg->topsky->surfs, &dseg->topsky->texinfo,
      r_region->ceiling->SkyBox, -1, sidedef->Light,
      !!(sidedef->Flags&SDF_ABSLIGHT), false);
    DrawSurfaces(seg, dseg->bot->surfs, &dseg->bot->texinfo,
      r_region->ceiling->SkyBox, -1, sidedef->Light,
      !!(sidedef->Flags&SDF_ABSLIGHT), false);
    DrawSurfaces(seg, dseg->mid->surfs, &dseg->mid->texinfo,
      r_region->ceiling->SkyBox, -1, sidedef->Light,
      !!(sidedef->Flags&SDF_ABSLIGHT), false);
    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      DrawSurfaces(seg, sp->surfs, &sp->texinfo, r_region->ceiling->SkyBox,
        -1, sidedef->Light, !!(sidedef->Flags&SDF_ABSLIGHT), false);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderSecSurface
//
//==========================================================================
void VRenderLevelShared::RenderSecSurface (sec_surface_t *ssurf, VEntity *SkyBox) {
  sec_plane_t &plane = *ssurf->secplane;

  if (!plane.pic) return;

  if (plane.PointOnSide(vieworg)) return; // viewer is in back side or on plane

  if (r_allow_mirrors && MirrorLevel < r_maxmirrors && plane.MirrorAlpha < 1.0f) {
    VPortal *Portal = nullptr;
    for (int i = 0; i < Portals.Num(); ++i) {
      if (Portals[i] && Portals[i]->MatchMirror(&plane)) {
        Portal = Portals[i];
        break;
      }
    }
    if (!Portal) {
      Portal = new VMirrorPortal(this, &plane);
      Portals.Append(Portal);
    }

    surface_t *surfs = ssurf->surfs;
    do {
      Portal->Surfs.Append(surfs);
      surfs = surfs->next;
    } while (surfs);

    if (plane.MirrorAlpha <= 0.0f) return;
    // k8: is this right?
    ssurf->texinfo.Alpha = plane.MirrorAlpha;
  } else {
    // this is NOT right!
    //ssurf->texinfo.Alpha = 1.0f;
    if (plane.MirrorAlpha < 1.0f) {
      if (ssurf->texinfo.Alpha >= 1.0f) {
        //GCon->Logf("MALPHA=%f", plane.MirrorAlpha);
        // darken it a little to simulate mirror
        sec_params_t *oldRegionLightParams = r_region->params;
        sec_params_t newLight = (plane.LightSourceSector >= 0 ? Level->Sectors[plane.LightSourceSector].params : *oldRegionLightParams);
        newLight.lightlevel = (int)((float)newLight.lightlevel*plane.MirrorAlpha);
        r_region->params = &newLight;
        // take light from `r_region->params`
        DrawSurfaces(nullptr, ssurf->surfs, &ssurf->texinfo, SkyBox, -1, 0, false, true);
        // and resore rregion
        r_region->params = oldRegionLightParams;
        return;
      }
    }
  }

  DrawSurfaces(nullptr, ssurf->surfs, &ssurf->texinfo, SkyBox, plane.LightSourceSector, 0, false, true);
}


//==========================================================================
//
//  VRenderLevelShared::RenderSubRegion
//
//  Determine floor/ceiling planes.
//  Draw one or more line segments.
//
//==========================================================================
void VRenderLevelShared::RenderSubRegion (subregion_t *region) {
  const float d = DotProduct(vieworg, region->floor->secplane->normal)-region->floor->secplane->dist;
  if (region->next && d <= 0.0f) {
    if (!ViewClip.ClipCheckRegion(region->next, r_sub)) return;
    RenderSubRegion(region->next);
  }

  check(r_sub->sector != nullptr);

  r_subregion = region;
  r_region = region->secregion;

  if (r_sub->poly) {
    // render the polyobj in the subsector first
    int polyCount = r_sub->poly->numsegs;
    seg_t **polySeg = r_sub->poly->segs;
    while (polyCount--) {
      RenderLine((*polySeg)->drawsegs);
      ++polySeg;
    }
  }

  int count = r_sub->numlines;
  drawseg_t *ds = region->lines;
  while (count--) {
    RenderLine(ds);
    ++ds;
  }

  RenderSecSurface(region->floor, r_region->floor->SkyBox);
  RenderSecSurface(region->ceil, r_region->ceiling->SkyBox);

  if (region->next && d > 0.0f) {
    if (!ViewClip.ClipCheckRegion(region->next, r_sub)) return;
    RenderSubRegion(region->next);
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderSubsector
//
//==========================================================================
void VRenderLevelShared::RenderSubsector (int num) {
  subsector_t *Sub = &Level->Subsectors[num];
  r_sub = Sub;

  if (Level->HasPVS() && Sub->VisFrame != r_visframecount) return;

  if (!Sub->sector->linecount) return; // skip sectors containing original polyobjs

  if (!ViewClip.ClipCheckSubsector(Sub, true)) return;

  BspVis[((unsigned)num)>>3] |= 1U<<(num&7);
  BspVisThing[((unsigned)num)>>3] |= 1U<<(num&7);

  // mark adjacent subsectors
  if (r_draw_adjacent_subsector_things) {
    int sgcount = Sub->numlines;
    if (sgcount) {
      const seg_t *seg = &Level->Segs[Sub->firstline];
      for (; sgcount--; ++seg) {
        if (seg->linedef && !(seg->linedef->flags&ML_TWOSIDED)) continue; // don't go through solid walls
        const seg_t *pseg = seg->partner;
        if (!pseg || !pseg->front_sub) continue;
        const unsigned psidx = (unsigned)(ptrdiff_t)(pseg->front_sub-Level->Subsectors);
        BspVisThing[psidx>>3] |= 1U<<(psidx&7);
      }
    }
  }

  // update world
  if (w_update_in_renderer && Sub->updateWorldFrame != updateWorldFrame) {
    Sub->updateWorldFrame = updateWorldFrame;
    if (!updateWorldCheckVisFrame || !Level->HasPVS() || Sub->VisFrame == r_visframecount) {
      //k8: i don't know yet if we have to restore `r_surf_sub`, so let's play safe here
      auto oldrss = r_surf_sub;
      r_surf_sub = Sub;
      UpdateSubRegion(Sub->regions/*, ClipSegs:true*/);
      r_surf_sub = oldrss;
    }
  }

  RenderSubRegion(Sub->regions);

  // add subsector's segs to the clipper
  // clipping against mirror is done only for vertical mirror planes
  ViewClip.ClipAddSubsectorSegs(Sub, (MirrorClipSegs && view_frustum.planes[5].isValid() ? &view_frustum.planes[5] : nullptr));
}


//==========================================================================
//
//  VRenderLevelShared::RenderBSPNode
//
//  Renders all subsectors below a given node, traversing subtree
//  recursively. Just call with BSP root.
//
//==========================================================================
void VRenderLevelShared::RenderBSPNode (int bspnum, const float *bbox, unsigned AClipflags) {
  if (ViewClip.ClipIsFull()) return;

  unsigned clipflags = AClipflags;
  // cull the clipping planes if not trivial accept
  if (clipflags && clip_frustum && clip_frustum_bsp) {
    const TClipPlane *cp = &view_frustum.planes[0];
    for (unsigned i = 0; i < 6; ++i, ++cp) {
      if (!(clipflags&cp->clipflag)) continue; // don't need to clip against it
      //k8: this check is always true, because view origin is outside of frustum (oops)
      //if (cp->PointOnSide(vieworg)) continue; // viewer is in back side or on plane (k8: why check this?)
      // check reject point
      if (cp->PointOnSide(TVec(bbox[cp->pindex[0]], bbox[cp->pindex[1]], bbox[cp->pindex[2]]))) {
        // completely outside of any plane means "invisible"
        check(cp->PointOnSide(TVec(bbox[cp->pindex[3+0]], bbox[cp->pindex[3+1]], bbox[cp->pindex[3+2]])));
        return;
      }
      // is node entirely on screen?
      if (!cp->PointOnSide(TVec(bbox[cp->pindex[3+0]], bbox[cp->pindex[3+1]], bbox[cp->pindex[3+2]]))) {
        // yes, don't check this plane
        clipflags ^= view_frustum.planes[i].clipflag;
      }
    }
  }

  if (!ViewClip.ClipIsBBoxVisible(bbox)) return;

  if (bspnum == -1) {
    RenderSubsector(0);
    return;
  }

  // found a subsector?
  if ((bspnum&NF_SUBSECTOR) == 0) {
    node_t *bsp = &Level->Nodes[bspnum];

    if (Level->HasPVS() && bsp->VisFrame != r_visframecount) return;

    // decide which side the view point is on
    int side = bsp->PointOnSide(vieworg);

    // recursively divide front space (toward the viewer)
    RenderBSPNode(bsp->children[side], bsp->bbox[side], clipflags);

    // possibly divide back space (away from the viewer)
    if (!ViewClip.ClipIsBBoxVisible(bsp->bbox[side^1])) return;

    RenderBSPNode(bsp->children[side^1], bsp->bbox[side^1], clipflags);
  } else {
    RenderSubsector(bspnum&(~NF_SUBSECTOR));
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderBspWorld
//
//==========================================================================
void VRenderLevelShared::RenderBspWorld (const refdef_t *rd, const VViewClipper *Range) {
  static const float dummy_bbox[6] = { -99999, -99999, -99999, 99999, 99999, 99999 };

  // if we hit a cache overflow, render everything again, to avoid partial frames
  do {
    if (light_reset_surface_cache) return;

    //view_frustum.setupBoxIndicies(); // done automatically
    ViewClip.ClearClipNodes(vieworg, Level);
    ViewClip.ClipInitFrustumRange(viewangles, viewforward, viewright, viewup, rd->fovx, rd->fovy);
    if (Range) ViewClip.ClipToRanges(*Range); // range contains a valid range, so we must clip away holes in it
    memset(BspVis, 0, VisSize);
    memset(BspVisThing, 0, VisSize);
    if (PortalLevel == 0) {
      if (WorldSurfs.NumAllocated() < 4096) WorldSurfs.Resize(4096);
    }
    MirrorClipSegs = (MirrorClip && !view_frustum.planes[5].normal.z);

    // head node is the last node output
    RenderBSPNode(Level->NumNodes-1, dummy_bbox, (MirrorClip ? 0x3f : 0x1f));

    if (PortalLevel == 0) {
      // draw the most complex sky portal behind the scene first, without the need to use stencil buffer
      VPortal *BestSky = nullptr;
      int BestSkyIndex = -1;
      for (int i = 0; i < Portals.Num(); ++i) {
        if (Portals[i] && Portals[i]->IsSky() && (!BestSky || BestSky->Surfs.Num() < Portals[i]->Surfs.Num())) {
          BestSky = Portals[i];
          BestSkyIndex = i;
        }
      }
      if (BestSky) {
        PortalLevel = 1;
        BestSky->Draw(false);
        delete BestSky;
        BestSky = nullptr;
        Portals.RemoveIndex(BestSkyIndex);
        PortalLevel = 0;
      }

      //fprintf(stderr, "WSL=%d\n", WorldSurfs.Num());
      const int wslen = WorldSurfs.Num();
      for (int i = 0; i < wslen; ++i) {
        switch (WorldSurfs[i].Type) {
          case 0:
            {
              seg_t *dcseg = (WorldSurfs[i].Surf->dcseg ? WorldSurfs[i].Surf->dcseg : nullptr);
              //if (dcseg) printf("world surface! %p\n", dcseg);
              QueueWorldSurface(dcseg, WorldSurfs[i].Surf);
            }
            break;
          case 1:
            QueueSkyPortal(WorldSurfs[i].Surf);
            break;
          case 2:
            QueueHorizonPortal(WorldSurfs[i].Surf);
            break;
        }
      }
      //WorldSurfs.Clear();
      WorldSurfs.resetNoDtor();
    }
  } while (light_reset_surface_cache);
}


//==========================================================================
//
//  VRenderLevelShared::RenderPortals
//
//==========================================================================
void VRenderLevelShared::RenderPortals () {
  if (PortalLevel == 0) {
    if (oldMaxMirrors != r_maxmirrors || oldPortalDepth != r_max_portal_depth ||
        oldHorizons != r_allow_horizons || oldMirrors != r_allow_mirrors)
    {
      //GCon->Logf("portal settings changed, resetting portal info...");
      for (int i = 0; i < Portals.Num(); ++i) {
        if (Portals[i]) {
          delete Portals[i];
          Portals[i] = nullptr;
        }
      }
      Portals.Clear();
      // save cvars
      oldMaxMirrors = r_maxmirrors;
      oldPortalDepth = r_max_portal_depth;
      oldHorizons = r_allow_horizons;
      oldMirrors = r_allow_mirrors;
      return;
    }
  }

  ++PortalLevel;

  if (r_max_portal_depth < 0 || PortalLevel <= r_max_portal_depth) {
    //FIXME: disable decals for portals
    //       i should rewrite decal rendering, so we can skip stencil buffer
    //       (or emulate stencil buffer with texture and shaders)
    bool oldDecalsEnabled = r_decals_enabled;
    r_decals_enabled = false;
    for (int i = 0; i < Portals.Num(); ++i) {
      if (Portals[i] && Portals[i]->Level == PortalLevel) Portals[i]->Draw(true);
    }
    r_decals_enabled = oldDecalsEnabled;
  } else {
    if (dbg_max_portal_depth_warning) GCon->Logf(NAME_Warning, "portal level too deep (%d)", PortalLevel);
  }

  for (int i = 0; i < Portals.Num(); ++i) {
    if (Portals[i] && Portals[i]->Level == PortalLevel) {
      delete Portals[i];
      Portals[i] = nullptr;
    }
  }

  --PortalLevel;
  if (PortalLevel == 0) Portals.Clear();
}
