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
#include <limits.h>
#include <float.h>

#include "gamedefs.h"
#include "r_local.h"

#define HORIZON_SURF_SIZE (sizeof(surface_t) + sizeof(TVec) * 3)

//#define VRBSP_DISABLE_SKY_PORTALS


VCvarB r_draw_pobj("r_draw_pobj", true, "Render polyobjects?", CVAR_PreInit);
static VCvarI r_maxmirrors("r_maxmirrors", "1", "Maximum allowed mirrors.", CVAR_Archive);
VCvarI r_max_portal_depth("r_max_portal_depth", "1", "Maximum allowed portal depth (-1: infinite)", CVAR_Archive);
static VCvarB r_allow_horizons("r_allow_horizons", true, "Allow horizon portal rendering?", CVAR_Archive);
static VCvarB r_allow_mirrors("r_allow_mirrors", true, "Allow mirror portal rendering (SLOW)?", CVAR_Archive);

static VCvarB r_disable_sky_portals("r_disable_sky_portals", false, "Disable rendering of sky portals.", 0/*CVAR_Archive*/);

static VCvarB r_steamline_masked_walls("r_steamline_masked_walls", true, "Render masked (two-sided) walls as normal ones.", CVAR_Archive);

static VCvarB dbg_max_portal_depth_warning("dbg_max_portal_depth_warning", false, "Show maximum allowed portal depth warning?", 0/*CVAR_Archive*/);

static VCvarB r_flood_renderer("r_flood_renderer", false, "Use new floodfill renderer?", CVAR_PreInit);

VCvarB VRenderLevelShared::times_render_highlevel("times_render_highlevel", false, "Show high-level render times.", 0/*CVAR_Archive*/);
VCvarB VRenderLevelShared::times_render_lowlevel("times_render_lowlevel", false, "Show low-level render times.", 0/*CVAR_Archive*/);
VCvarB VRenderLevelShared::r_disable_world_update("r_disable_world_update", false, "Disable world updates.", 0/*CVAR_Archive*/);

extern int light_reset_surface_cache; // in r_light_reg.cpp
extern VCvarB r_decals_enabled;
extern VCvarB r_draw_adjacent_subsector_things;
extern VCvarB w_update_in_renderer;
extern VCvarB clip_frustum;
extern VCvarB clip_frustum_bsp;
extern VCvarB clip_frustum_mirror;
extern VCvarB clip_use_1d_clipper;

// to clear portals
static bool oldMirrors = true;
static bool oldHorizons = true;
static int oldMaxMirrors = -666;
static int oldPortalDepth = -666;


//==========================================================================
//
//  VRenderLevelShared::SurfCheckAndQueue
//
//  this checks if surface is not queued twice
//
//==========================================================================
void VRenderLevelShared::SurfCheckAndQueue (TArray<surface_t *> &queue, surface_t *surf) {
  check(surf);
  if (surf->queueframe == currQueueFrame) {
    if (surf->dcseg) {
      Host_Error("subsector %d, seg %d surface queued for rendering twice",
        (int)(ptrdiff_t)(surf->dcseg-Level->Segs),
        (int)(ptrdiff_t)(surf->subsector-Level->Subsectors));
    } else {
      Host_Error("subsector %d surface queued for rendering twice",
        (int)(ptrdiff_t)(surf->subsector-Level->Subsectors));
    }
  }
  surf->queueframe = currQueueFrame;
  queue.append(surf);
  //GCon->Logf("frame %u: queued surface with texinfo %p", currQueueFrame, surf->texinfo);
}


//==========================================================================
//
//  VRenderLevelShared::QueueSimpleSurf
//
//==========================================================================
void VRenderLevelShared::QueueSimpleSurf (seg_t *seg, surface_t *surf) {
  surf->dcseg = seg;
  SurfCheckAndQueue(DrawSurfList, surf);
}


//==========================================================================
//
//  VRenderLevelShared::QueueSkyPortal
//
//==========================================================================
void VRenderLevelShared::QueueSkyPortal (surface_t *surf) {
  surf->dcseg = nullptr;
  SurfCheckAndQueue(DrawSkyList, surf);
}


//==========================================================================
//
//  VRenderLevelShared::QueueHorizonPortal
//
//==========================================================================
void VRenderLevelShared::QueueHorizonPortal (surface_t *surf) {
  surf->dcseg = nullptr;
  SurfCheckAndQueue(DrawHorizonList, surf);
}


//==========================================================================
//
//  VRenderLevelShared::DrawSurfaces
//
//==========================================================================
void VRenderLevelShared::DrawSurfaces (subsector_t *sub, sec_region_t *secregion, seg_t *seg,
  surface_t *InSurfs, texinfo_t *texinfo, VEntity *SkyBox, int LightSourceSector, int SideLight,
  bool AbsSideLight, bool CheckSkyBoxAlways)
{
  surface_t *surfs = InSurfs;
  if (!surfs) return;

  if (texinfo->Tex->Type == TEXTYPE_Null) return;

  sec_params_t *LightParams = (LightSourceSector == -1 ? secregion->params : &Level->Sectors[LightSourceSector].params);
  int lLev = (AbsSideLight ? 0 : LightParams->lightlevel)+SideLight;
  lLev = (FixedLight ? FixedLight : lLev+ExtraLight);
  lLev = MID(0, lLev, 255);
  if (r_darken) lLev = light_remap[lLev];
  vuint32 Fade = GetFade(secregion);

  if (SkyBox && (SkyBox->EntityFlags&VEntity::EF_FixedModel)) SkyBox = nullptr;
  bool IsStack = SkyBox && SkyBox->eventSkyBoxGetAlways();
  if (texinfo->Tex == GTextureManager[skyflatnum] || (IsStack && CheckSkyBoxAlways)) { //k8: i hope that the parens are right here
    VSky *Sky = nullptr;
    if (!SkyBox && (sub->sector->Sky&SKY_FROM_SIDE) != 0) {
      int Tex;
      bool Flip;
      if (sub->sector->Sky == SKY_FROM_SIDE) {
        Tex = Level->LevelInfo->Sky2Texture;
        Flip = true;
      } else {
        side_t *Side = &Level->Sides[(sub->sector->Sky&(SKY_FROM_SIDE-1))-1];
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
        surfs->dlightframe = sub->dlightframe;
        surfs->dlightbits = sub->dlightbits;
        surfs->dcseg = nullptr; // sky cannot have decals anyway
        DrawTranslucentPoly(surfs, surfs->verts, surfs->count,
          0, SkyBox->eventSkyBoxGetPlaneAlpha(), false, 0,
          false, 0, Fade, TVec(), 0, TVec(), TVec(), TVec());
      }
      surfs = surfs->next;
    } while (surfs);
    return;
  } // done skybox rendering

  do {
    if (surfs->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane

    surfs->Light = (lLev<<24)|LightParams->LightColour;
    surfs->Fade = Fade;
    surfs->dlightframe = sub->dlightframe;
    surfs->dlightbits = sub->dlightbits;

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
        0, texinfo->Alpha, texinfo->Additive, 0, false, 0, Fade,
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
void VRenderLevelShared::RenderHorizon (subsector_t *sub, sec_region_t *secregion, subregion_t *subregion, drawseg_t *dseg) {
  seg_t *seg = dseg->seg;

  if (!dseg->HorizonTop) {
    dseg->HorizonTop = (surface_t *)Z_Malloc(HORIZON_SURF_SIZE);
    dseg->HorizonBot = (surface_t *)Z_Malloc(HORIZON_SURF_SIZE);
    memset((void *)dseg->HorizonTop, 0, HORIZON_SURF_SIZE);
    memset((void *)dseg->HorizonBot, 0, HORIZON_SURF_SIZE);
  }

  // horizon is not supported in sectors with slopes, so just use TexZ
  float TopZ = secregion->ceiling->TexZ;
  float BotZ = secregion->floor->TexZ;
  float HorizonZ = vieworg.z;

  // handle top part
  if (TopZ > HorizonZ) {
    sec_surface_t *Ceil = subregion->ceil;

    // calculate light and fade
    sec_params_t *LightParams = Ceil->secplane->LightSourceSector != -1 ?
      &Level->Sectors[Ceil->secplane->LightSourceSector].params :
      secregion->params;
    int lLev = (FixedLight ? FixedLight : MIN(255, LightParams->lightlevel+ExtraLight));
    if (r_darken) lLev = light_remap[lLev];
    vuint32 Fade = GetFade(secregion);

    surface_t *Surf = dseg->HorizonTop;
    Surf->plane = dseg->seg;
    Surf->texinfo = &Ceil->texinfo;
    Surf->HorizonPlane = Ceil->secplane;
    Surf->Light = (lLev << 24) | LightParams->LightColour;
    Surf->Fade = Fade;
    Surf->count = 4;
    TVec *svs = &Surf->verts[0];
    svs[0] = *seg->v1; svs[0].z = MAX(BotZ, HorizonZ);
    svs[1] = *seg->v1; svs[1].z = TopZ;
    svs[2] = *seg->v2; svs[2].z = TopZ;
    svs[3] = *seg->v2; svs[3].z = MAX(BotZ, HorizonZ);
    if (Ceil->secplane->pic == skyflatnum) {
      // if it's a sky, render it as a regular sky surface
      DrawSurfaces(sub, secregion, nullptr, Surf, &Ceil->texinfo, secregion->ceiling->SkyBox, -1,
        seg->sidedef->Light, !!(seg->sidedef->Flags & SDF_ABSLIGHT),
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
    sec_surface_t *Floor = subregion->floor;

    // calculate light and fade
    sec_params_t *LightParams = Floor->secplane->LightSourceSector != -1 ?
      &Level->Sectors[Floor->secplane->LightSourceSector].params :
      secregion->params;
    int lLev = (FixedLight ? FixedLight : MIN(255, LightParams->lightlevel+ExtraLight));
    if (r_darken) lLev = light_remap[lLev];
    vuint32 Fade = GetFade(secregion);

    surface_t *Surf = dseg->HorizonBot;
    Surf->plane = dseg->seg;
    Surf->texinfo = &Floor->texinfo;
    Surf->HorizonPlane = Floor->secplane;
    Surf->Light = (lLev << 24) | LightParams->LightColour;
    Surf->Fade = Fade;
    Surf->count = 4;
    TVec *svs = &Surf->verts[0];
    svs[0] = *seg->v1; svs[0].z = BotZ;
    svs[1] = *seg->v1; svs[1].z = MIN(TopZ, HorizonZ);
    svs[2] = *seg->v2; svs[2].z = MIN(TopZ, HorizonZ);
    svs[3] = *seg->v2; svs[3].z = BotZ;
    if (Floor->secplane->pic == skyflatnum) {
      // if it's a sky, render it as a regular sky surface
      DrawSurfaces(sub, secregion, nullptr, Surf, &Floor->texinfo, secregion->floor->SkyBox, -1,
        seg->sidedef->Light, !!(seg->sidedef->Flags & SDF_ABSLIGHT),
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
void VRenderLevelShared::RenderMirror (subsector_t *sub, sec_region_t *secregion, drawseg_t *dseg) {
  seg_t *seg = dseg->seg;
  if (MirrorLevel < r_maxmirrors && r_allow_mirrors) {
    VPortal *Portal = nullptr;
    for (int i = 0; i < Portals.Num(); ++i) {
      if (Portals[i] && Portals[i]->MatchMirror(seg)) {
        Portal = Portals[i];
        break;
      }
    }
    if (!Portal) {
      Portal = new VMirrorPortal(this, seg);
      Portals.Append(Portal);
    }

    surface_t *surfs = dseg->mid->surfs;
    do {
      Portal->Surfs.Append(surfs);
      surfs = surfs->next;
    } while (surfs);
  } else {
    DrawSurfaces(sub, secregion, seg, dseg->mid->surfs, &dseg->mid->texinfo,
      secregion->ceiling->SkyBox, -1, seg->sidedef->Light,
      !!(seg->sidedef->Flags & SDF_ABSLIGHT), false);
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderLine
//
//  Clips the given segment and adds any visible pieces to the line list.
//
//==========================================================================
void VRenderLevelShared::RenderLine (subsector_t *sub, sec_region_t *secregion, subregion_t *subregion, drawseg_t *dseg) {
  seg_t *seg = dseg->seg;
  line_t *linedef = seg->linedef;

  if (!linedef) return; // miniseg

  if (seg->PointOnSide(vieworg)) return; // viewer is in back side or on plane

  if (MirrorClipSegs && clip_frustum && clip_frustum_mirror && clip_frustum_bsp && view_frustum.planes[5].isValid()) {
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
      RenderHorizon(sub, secregion, subregion, dseg);
    } else if (seg->linedef->special == LNSPEC_LineMirror) {
      RenderMirror(sub, secregion, dseg);
    } else {
      DrawSurfaces(sub, secregion, seg, dseg->mid->surfs, &dseg->mid->texinfo,
        secregion->ceiling->SkyBox, -1, sidedef->Light,
        !!(sidedef->Flags & SDF_ABSLIGHT), false);
    }
    DrawSurfaces(sub, secregion, nullptr, dseg->topsky->surfs, &dseg->topsky->texinfo,
      secregion->ceiling->SkyBox, -1, sidedef->Light,
      !!(sidedef->Flags & SDF_ABSLIGHT), false);
  } else {
    // two sided line
    DrawSurfaces(sub, secregion, seg, dseg->top->surfs, &dseg->top->texinfo,
      secregion->ceiling->SkyBox, -1, sidedef->Light,
      !!(sidedef->Flags&SDF_ABSLIGHT), false);
    DrawSurfaces(sub, secregion, nullptr, dseg->topsky->surfs, &dseg->topsky->texinfo,
      secregion->ceiling->SkyBox, -1, sidedef->Light,
      !!(sidedef->Flags&SDF_ABSLIGHT), false);
    DrawSurfaces(sub, secregion, seg, dseg->bot->surfs, &dseg->bot->texinfo,
      secregion->ceiling->SkyBox, -1, sidedef->Light,
      !!(sidedef->Flags&SDF_ABSLIGHT), false);
    DrawSurfaces(sub, secregion, seg, dseg->mid->surfs, &dseg->mid->texinfo,
      secregion->ceiling->SkyBox, -1, sidedef->Light,
      !!(sidedef->Flags&SDF_ABSLIGHT), false);
    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      DrawSurfaces(sub, secregion, seg, sp->surfs, &sp->texinfo, secregion->ceiling->SkyBox,
        -1, sidedef->Light, !!(sidedef->Flags&SDF_ABSLIGHT), false);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderSecSurface
//
//==========================================================================
void VRenderLevelShared::RenderSecSurface (subsector_t *sub, sec_region_t *secregion, sec_surface_t *ssurf, VEntity *SkyBox) {
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
        sec_params_t *oldRegionLightParams = secregion->params;
        sec_params_t newLight = (plane.LightSourceSector >= 0 ? Level->Sectors[plane.LightSourceSector].params : *oldRegionLightParams);
        newLight.lightlevel = (int)((float)newLight.lightlevel*plane.MirrorAlpha);
        secregion->params = &newLight;
        // take light from `secregion->params`
        DrawSurfaces(sub, secregion, nullptr, ssurf->surfs, &ssurf->texinfo, SkyBox, -1, 0, false, true);
        // and resore rregion
        secregion->params = oldRegionLightParams;
        return;
      }
    }
  }

  DrawSurfaces(sub, secregion, nullptr, ssurf->surfs, &ssurf->texinfo, SkyBox, plane.LightSourceSector, 0, false, true);
}


//==========================================================================
//
//  VRenderLevelShared::RenderSubRegion
//
//  Determine floor/ceiling planes.
//  Draw one or more line segments.
//
//==========================================================================
void VRenderLevelShared::RenderSubRegion (subsector_t *sub, subregion_t *region, bool useClipper) {
  const float d = DotProduct(vieworg, region->floor->secplane->normal)-region->floor->secplane->dist;
  if (region->next && d <= 0.0f) {
    if (useClipper && !ViewClip.ClipCheckRegion(region->next, sub)) return;
    RenderSubRegion(sub, region->next, useClipper);
  }

  check(sub->sector != nullptr);

  subregion_t *subregion = region;
  sec_region_t *secregion = region->secregion;

  if (sub->poly && r_draw_pobj) {
    // render the polyobj in the subsector first
    int polyCount = sub->poly->numsegs;
    seg_t **polySeg = sub->poly->segs;
    while (polyCount--) {
      RenderLine(sub, secregion, subregion, (*polySeg)->drawsegs);
      ++polySeg;
    }
  }

  int count = sub->numlines;
  drawseg_t *ds = region->lines;
  while (count--) {
    RenderLine(sub, secregion, subregion, ds);
    ++ds;
  }

  RenderSecSurface(sub, secregion, region->floor, secregion->floor->SkyBox);
  RenderSecSurface(sub, secregion, region->ceil, secregion->ceiling->SkyBox);

  if (region->next && d > 0.0f) {
    if (useClipper && !ViewClip.ClipCheckRegion(region->next, sub)) return;
    RenderSubRegion(sub, region->next, useClipper);
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderSubsector
//
//==========================================================================
void VRenderLevelShared::RenderSubsector (int num, bool useClipper) {
  subsector_t *sub = &Level->Subsectors[num];
  //r_sub = sub;

  if (Level->HasPVS() && sub->VisFrame != currVisFrame) return;

  if (!sub->sector->linecount) return; // skip sectors containing original polyobjs

  if (useClipper && !ViewClip.ClipCheckSubsector(sub, clip_use_1d_clipper)) return;

  sub->parent->VisFrame = currVisFrame;
  sub->VisFrame = currVisFrame;

  BspVis[((unsigned)num)>>3] |= 1U<<(num&7);
  BspVisThing[((unsigned)num)>>3] |= 1U<<(num&7);

  // mark adjacent subsectors
  if (r_draw_adjacent_subsector_things) {
    int sgcount = sub->numlines;
    if (sgcount) {
      const seg_t *seg = &Level->Segs[sub->firstline];
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
  if (w_update_in_renderer && sub->updateWorldFrame != updateWorldFrame) {
    sub->updateWorldFrame = updateWorldFrame;
    if (!updateWorldCheckVisFrame || !Level->HasPVS() || sub->VisFrame == currVisFrame) {
      //k8: i don't know yet if we have to restore `r_surf_sub`, so let's play safe here
      //auto oldrss = r_surf_sub;
      //r_surf_sub = sub;
      UpdateSubRegion(sub, sub->regions/*, ClipSegs:true*/);
      //r_surf_sub = oldrss;
    }
  }

  RenderSubRegion(sub, sub->regions, useClipper);

  // add subsector's segs to the clipper
  // clipping against mirror is done only for vertical mirror planes
  if (useClipper && clip_use_1d_clipper) {
    ViewClip.ClipAddSubsectorSegs(sub, (MirrorClipSegs && view_frustum.planes[5].isValid() ? &view_frustum.planes[5] : nullptr));
  }
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

  if (bspnum == -1) {
    RenderSubsector(0);
    return;
  }

  unsigned clipflags = AClipflags;
  // cull the clipping planes if not trivial accept
  if (clipflags && clip_frustum && clip_frustum_bsp) {
    //static float newbbox[6];
    //memcpy(newbbox, bbox, sizeof(float)*6);
    //newbbox[2] = -32767.0f;
    //newbbox[5] = +32767.0f;
    const TClipPlane *cp = &view_frustum.planes[0];
    for (unsigned i = view_frustum.planeCount; i--; ++cp) {
      if (!(clipflags&cp->clipflag)) continue; // don't need to clip against it
      //k8: this check is always true, because view origin is outside of frustum (oops)
      //if (cp->PointOnSide(vieworg)) continue; // viewer is in back side or on plane (k8: why check this?)
#if defined(FRUSTUM_BOX_OPTIMISATION)
      // check reject point
      if (cp->PointOnSide(TVec(bbox[cp->pindex[0]], bbox[cp->pindex[1]], bbox[cp->pindex[2]]))) {
        // completely outside of any plane means "invisible"
        check(cp->PointOnSide(TVec(bbox[cp->pindex[3+0]], bbox[cp->pindex[3+1]], bbox[cp->pindex[3+2]])));
        return;
      }
      // is node entirely on screen?
      // k8: don't do this: frustum test are cheap, and we can hit false positive easily
      /*
      if (!cp->PointOnSide(TVec(bbox[cp->pindex[3+0]], bbox[cp->pindex[3+1]], bbox[cp->pindex[3+2]]))) {
        // yes, don't check this plane
        clipflags ^= cp->clipflag;
      }
      */
#elif 0
      int cres = cp->checkBoxEx(bbox);
      if (cres == TFrustum::OUTSIDE) {
        // add subsector to clipper (why not?)
        /*
        if ((bspnum&NF_SUBSECTOR) != 0) {
          if (ViewClip.ClipIsBBoxVisible(bbox)) {
            subsector_t *sub = &Level->Subsectors[bspnum&(~NF_SUBSECTOR)];
            ViewClip.ClipAddAllSubsectorSegs(sub, (MirrorClipSegs && view_frustum.planes[5].isValid() ? &view_frustum.planes[5] : nullptr));
          }
        }
        */
        return;
      }
      // k8: don't do this: frustum test are cheap, and we can hit false positive easily
      //if (cres == TFrustum::INSIDE) clipflags ^= cp->clipflag; // don't check this plane anymore
#else
      if (!cp->checkBox(bbox)) return;
#endif
    }
  }

  if (!ViewClip.ClipIsBBoxVisible(bbox)) return;

  // found a subsector?
  if ((bspnum&NF_SUBSECTOR) == 0) {
    node_t *bsp = &Level->Nodes[bspnum];

    // decide which side the view point is on
    int side = bsp->PointOnSide(vieworg);

    if (bsp->children[side]&NF_SUBSECTOR) bsp->VisFrame = currVisFrame;

    // recursively divide front space (toward the viewer)
    RenderBSPNode(bsp->children[side], bsp->bbox[side], clipflags);

    // possibly divide back space (away from the viewer)
    if (!ViewClip.ClipIsBBoxVisible(bsp->bbox[side^1])) return;

    RenderBSPNode(bsp->children[side^1], bsp->bbox[side^1], clipflags);
  } else {
    RenderSubsector(bspnum&(~NF_SUBSECTOR));
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// returns the closest point to `this` on the line segment from `a` to `b`
static TVec projectOnLine (const TVec &p, const TVec &a, const TVec b) {
  const TVec ab = b-a; // vector from a to b
  // squared distance from a to b
  const float absq = ab.dot(ab);
  if (fabs(absq) < 0.001f) return a; // a and b are the same point (roughly)
  const TVec ap = p-a; // vector from a to p
  const float t = ap.dot(ab)/absq;
  if (t < 0) return a; // "before" a on the line
  if (t > 1) return b; // "after" b on the line
  // projection lies "inbetween" a and b on the line
  return a+t*ab;
}


struct SubInfo {
  subsector_t *sub;
  float minDistSq; // minimum distance to view origin (used to sort subsectors)
  bool seenSeg;
  float bbox[6];

  SubInfo () {}
  SubInfo (ENoInit) {}
  SubInfo (const VLevel *Level, const TVec &origin, subsector_t *asub) {
    sub = asub;
    minDistSq = FLT_MAX;
    seenSeg = false;
    float bestMini = FLT_MAX;
    bool seenMini = false;
    float bestNonMini = FLT_MAX;
    bool seenNonMini = false;
    const seg_t *seg = &Level->Segs[asub->firstline];
    for (unsigned i = asub->numlines; i--; ++seg) {
      const line_t *line = seg->linedef;
      //if (seg->PointOnSide(origin)) continue; // cannot see
      if (!line) {
        // miniseg
        seenSeg = true;
        seenMini = true;
        const TVec proj = projectOnLine(origin, *seg->v1, *seg->v2);
        const float distSq = proj.length2DSquared();
        bestMini = MIN(distSq, bestMini);
      } else {
        // normal seg
        seenSeg = true;
        seenNonMini = true;
        /*
        const TVec v1 = *seg->v1-origin, v2 = seg->v2-origin;
        float distSq = v1.length2DSquared();
        minDistSq = MIN(distSq, minDistSq);
        float distSq = v2.length2DSquared();
        minDistSq = MIN(distSq, minDistSq);
        */
        /*
        const float distSq = fabsf(DotProduct(origin, seg->normal)-seg->dist);
        minDistSq = MIN(distSq, minDistSq);
        */
        const TVec proj = projectOnLine(origin, *seg->v1, *seg->v2);
        const float distSq = proj.length2DSquared();
        bestNonMini = MIN(distSq, bestNonMini);
      }
    }
    if (seenNonMini) {
      minDistSq = bestNonMini;
    } else if (seenMini) {
      minDistSq = bestMini;
    }
    minDistSq = MIN(bestMini, bestNonMini);
  }
};

extern "C" {
  static int subinfoCmp (const void *a, const void *b, void *udata) {
    if (a == b) return 0;
    const SubInfo *aa = (const SubInfo *)a;
    const SubInfo *bb = (const SubInfo *)b;
    if (aa->minDistSq < bb->minDistSq) return -1;
    if (aa->minDistSq > bb->minDistSq) return 1;
    return 0;
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
    if (!clip_frustum_mirror) {
      MirrorClipSegs = false;
      view_frustum.planes[5].clipflag = 0;
    }

    if (r_flood_renderer && Level->NumSubsectors) {
      GCon->Log("============");
      // start from r_viewleaf
      static TArray<SubInfo> queue;
      static TArray<vuint8> addedSubs;
      if (addedSubs.length() != Level->NumSubsectors) addedSubs.setLength(Level->NumSubsectors);
      memset(addedSubs.ptr(), 0, Level->NumSubsectors);
      queue.reset();
      // start from this subsector
      queue.append(SubInfo(Level, vieworg, r_viewleaf));
      addedSubs[(unsigned)(ptrdiff_t)(r_viewleaf-Level->Subsectors)] = 1;
      // add other subsectors
      int queuenum = 0;
      while (queuenum < queue.length()) {
        subsector_t *currsub = queue[queuenum++].sub;
        if (currsub->VisFrame == currVisFrame) continue; // already processed
        currsub->VisFrame = currVisFrame; // mark as processed
        if (!currsub->sector->linecount) continue; // skip sectors containing original polyobjs
        //const unsigned csnum = (unsigned)(ptrdiff_t)(currsub-Level->Subsectors);
        // check clipper
        float bbox[6];
        Level->GetSubsectorBBox(currsub, bbox);
        if (!view_frustum.checkBox(bbox)) continue;
        // travel to other subsectors
        //GCon->Log("...");
        const seg_t *seg = &Level->Segs[currsub->firstline];
        for (unsigned i = currsub->numlines; i--; ++seg) {
          const line_t *line = seg->linedef;
          if (line) {
            // not a miniseg; check for two-sided
            if (!(line->flags&ML_TWOSIDED)) {
              // not a two-sided, nowhere to travel
              continue;
            }
          }
          if (!seg->partner) continue; // just in case
          if (seg->PointOnSide(vieworg)) continue; // cannot see
          // closed door/lift?
          if (seg->backsector && seg->backsector != seg->frontsector &&
              (line->flags&(ML_TWOSIDED|ML_3DMIDTEX)) == ML_TWOSIDED)
          {
            if (VViewClipper::IsSegAClosedSomething(&view_frustum, seg)) continue;
          }
          // it is visible, travel to partner subsector
          subsector_t *ps = seg->partner->front_sub;
          if (!ps || ps == currsub) continue; // just in case
          if (ps->VisFrame == currVisFrame) continue; // already processed
          const unsigned psnum = (unsigned)(ptrdiff_t)(ps-Level->Subsectors);
          if (addedSubs.ptr()[psnum]) continue; // already added
          addedSubs.ptr()[psnum] = 1;
          SubInfo si = SubInfo(Level, vieworg, ps);
          if (si.seenSeg) {
            //GCon->Log(" +++");
            memcpy(si.bbox, bbox, sizeof(float)*6);
            queue.append(si);
          }
        }
      }
      //GCon->Logf("found #%d subsectors", queue.length());

      // sort subsectors
      if (queue.length() > 1) {
        timsort_r(queue.ptr()+1, queue.length()-1, sizeof(SubInfo), &subinfoCmp, nullptr);
      }

      // render subsectors using clipper
      // no need to check frustum, it is already done
      {
        const SubInfo *si = queue.ptr();
        for (int sicount = queue.length(); sicount--; ++si) {
          //if (!ViewClip.ClipIsBBoxVisible(si->bbox)) continue;
          const subsector_t *currsub = si->sub;
          const int snum = (int)(ptrdiff_t)(currsub-Level->Subsectors);
          if (!ViewClip.ClipCheckSubsector(currsub, clip_use_1d_clipper)) {
            GCon->Logf("SKIP SEG #%d", snum);
            continue;
          }
          // render it, and add to clipper
          RenderSubsector((int)(ptrdiff_t)(currsub-Level->Subsectors), false);
          GCon->Logf("RENDER SEG #%d (before)", snum); ViewClip.Dump();
          ViewClip.ClipAddSubsectorSegs(currsub, (MirrorClipSegs && view_frustum.planes[5].isValid() ? &view_frustum.planes[5] : nullptr));
          GCon->Logf("RENDER SEG #%d (after)", snum); ViewClip.Dump();
        }
      }
      //ViewClip.Dump();
    } else {
      // head node is the last node output
      RenderBSPNode(Level->NumNodes-1, dummy_bbox, (MirrorClip ? 0x3f : 0x1f));
    }

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
