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

#define HORIZON_SURF_SIZE  (sizeof(surface_t)+sizeof(TVec)*3)

//#define VRBSP_DISABLE_SKY_PORTALS


VCvarB r_draw_pobj("r_draw_pobj", true, "Render polyobjects?", CVAR_PreInit);
static VCvarI r_maxmirrors("r_maxmirrors", "1", "Maximum allowed mirrors.", CVAR_Archive);
VCvarI r_max_portal_depth("r_max_portal_depth", "1", "Maximum allowed portal depth (-1: infinite)", CVAR_Archive);
static VCvarB r_allow_horizons("r_allow_horizons", true, "Allow horizon portal rendering?", CVAR_Archive);
static VCvarB r_allow_mirrors("r_allow_mirrors", false, "Allow mirror portal rendering?", CVAR_Archive);
static VCvarB r_allow_other_portals("r_allow_other_portals", false, "Allow non-mirror portal rendering (SLOW)?", CVAR_Archive);

static VCvarB r_disable_sky_portals("r_disable_sky_portals", false, "Disable rendering of sky portals.", 0/*CVAR_Archive*/);

static VCvarB dbg_max_portal_depth_warning("dbg_max_portal_depth_warning", false, "Show maximum allowed portal depth warning?", 0/*CVAR_Archive*/);

static VCvarB r_ordered_subregions("r_ordered_subregions", true, "Order subregions in renderer?", CVAR_Archive);

//static VCvarB dbg_dump_portal_list("dbg_dump_portal_list", false, "Dump portal list before rendering?", 0/*CVAR_Archive*/);

VCvarB VRenderLevelShared::times_render_highlevel("times_render_highlevel", false, "Show high-level render times.", 0/*CVAR_Archive*/);
VCvarB VRenderLevelShared::times_render_lowlevel("times_render_lowlevel", false, "Show low-level render times.", 0/*CVAR_Archive*/);
VCvarB VRenderLevelShared::r_disable_world_update("r_disable_world_update", false, "Disable world updates.", 0/*CVAR_Archive*/);

VCvarB r_dbg_always_draw_flats("r_dbg_always_draw_flats", false, "Draw flat surfaces even if region is not visible (this is pobj hack)?", 0/*CVAR_Archive*/);

extern int light_reset_surface_cache; // in r_light_reg.cpp
extern VCvarB r_decals_enabled;
extern VCvarB r_draw_adjacent_subsector_things;
#if 0
extern VCvarB w_update_in_renderer;
#endif
extern VCvarB clip_frustum;
extern VCvarB clip_frustum_bsp;
extern VCvarB clip_frustum_mirror;
extern VCvarB clip_use_1d_clipper;
extern VCvarB clip_frustum_bsp_segs;
extern VCvarB r_dbg_always_draw_flats;

// to clear portals
static bool oldMirrors = true;
static bool oldHorizons = true;
static int oldMaxMirrors = -666;
static int oldPortalDepth = -666;


//==========================================================================
//
//  VRenderLevelShared::ChooseFlatSurfaces
//
//==========================================================================
void VRenderLevelShared::ChooseFlatSurfaces (sec_surface_t *&f0, sec_surface_t *&f1, sec_surface_t *flat0, sec_surface_t *flat1) {
  if (!flat0 || !flat1) {
    f0 = flat0;
    f1 = flat1;
    return;
  }

  // check if flat1 is the same as flat0
  if (flat0->esecplane.splane == flat1->esecplane.splane) {
    f0 = flat1; // flat1 is fake, use it, because it may have different texture
    f1 = nullptr;
    return;
  }

  // not the same, check if on the same height
  if (flat0->esecplane.GetNormal() == flat1->esecplane.GetNormal() &&
      flat0->esecplane.GetDist() == flat1->esecplane.GetDist())
  {
    f0 = flat1; // flat1 is fake, use it, because it may have different texture
    f1 = nullptr;
    return;
  }

  // render both
  f0 = flat0;
  f1 = flat1;
}


//==========================================================================
//
//  VRenderLevelShared::GetFlatSetToRender
//
//  this chooses from zero to four subsector floor/ceiling surfaces
//  this is required to properly render Boom transfer heights
//  first two elements are floors, second two elements are ceilings
//  any element can be `nullptr`
//
//==========================================================================
void VRenderLevelShared::GetFlatSetToRender (subsector_t *sub, subregion_t *region, sec_surface_t *surfs[4]) {
  // if "clip fake planes" is not set, don't render real floor/ceiling if they're clipped away
  bool realFloorClipped = false;
  bool realCeilingClipped = false;

  const sector_t *hs = (sub->sector ? sub->sector->heightsec : nullptr);
  if (hs && !(hs->SectorFlags&sector_t::SF_ClipFakePlanes)) {
    // check for clipped real floor and ceiling
    if (sub->sector->floor.minz > hs->floor.minz) realFloorClipped = true;
    if (sub->sector->ceiling.maxz < hs->ceiling.maxz) realCeilingClipped = true;
  }

  if (!realFloorClipped) {
    ChooseFlatSurfaces(surfs[0], surfs[1], region->realfloor, region->fakefloor);
  } else {
    surfs[0] = region->fakefloor;
    surfs[1] = nullptr;
  }

  if (!realCeilingClipped) {
    ChooseFlatSurfaces(surfs[2], surfs[3], region->realceil, region->fakeceil);
  } else {
    surfs[2] = region->fakeceil;
    surfs[3] = nullptr;
  }
}


//==========================================================================
//
//  VRenderLevelShared::SurfCheckAndQueue
//
//  this checks if surface is not queued twice
//
//==========================================================================
void VRenderLevelShared::SurfCheckAndQueue (TArray<surface_t *> &queue, surface_t *surf) {
  if (!surf || !surf->subsector || surf->count < 3) return; // why not?

  VTexture *tex = surf->texinfo->Tex;
  if (!tex || tex->Type == TEXTYPE_Null) return;

  if (surf->queueframe == currQueueFrame) {
    if (surf->seg) {
      //abort();
      GCon->Logf(NAME_Warning, "subsector %d, seg %d surface queued for rendering twice",
        (int)(ptrdiff_t)(surf->subsector-Level->Subsectors),
        (int)(ptrdiff_t)(surf->seg-Level->Segs));
    } else {
      GCon->Logf(NAME_Warning, "subsector %d surface queued for rendering twice",
        (int)(ptrdiff_t)(surf->subsector-Level->Subsectors));
    }
    return;
  }
  surf->queueframe = currQueueFrame;
  surf->plvisible = surf->IsVisible(vieworg);

  /*
  if (!(surf->drawflags&surface_t::DF_MASKED)) {
    // check for non-solid texture
    if (surf->texinfo->Alpha < 1.0f || tex->isTransparent()) surf->drawflags |= surface_t::DF_MASKED;
  }
  */
  // alpha: 1.0 is masked wall, 1.1 is solid wall
  if (surf->texinfo->Alpha < 1.0f || tex->isTransparent()) {
    surf->drawflags |= surface_t::DF_MASKED;
  } else {
    surf->drawflags &= ~surface_t::DF_MASKED;
  }

  queue.append(surf);
  //GCon->Logf("frame %u: queued surface with texinfo %p", currQueueFrame, surf->texinfo);
}


//==========================================================================
//
//  VRenderLevelShared::QueueSimpleSurf
//
//==========================================================================
void VRenderLevelShared::QueueSimpleSurf (surface_t *surf) {
  SurfCheckAndQueue(DrawSurfList, surf);
}


//==========================================================================
//
//  VRenderLevelShared::QueueSkyPortal
//
//==========================================================================
void VRenderLevelShared::QueueSkyPortal (surface_t *surf) {
  SurfCheckAndQueue(DrawSkyList, surf);
}


//==========================================================================
//
//  VRenderLevelShared::QueueHorizonPortal
//
//==========================================================================
void VRenderLevelShared::QueueHorizonPortal (surface_t *surf) {
  SurfCheckAndQueue(DrawHorizonList, surf);
}


//==========================================================================
//
//  VRenderLevelShared::CommonQueueSurface
//
//==========================================================================
void VRenderLevelShared::CommonQueueSurface (surface_t *surf, vuint8 type) {
  if (PortalLevel == 0) {
    world_surf_t &S = WorldSurfs.Alloc();
    S.Surf = surf;
    S.Type = type;
  } else {
    switch (type) {
      case 0: QueueWorldSurface(surf); break;
      case 1: QueueSkyPortal(surf); break;
      case 2: QueueHorizonPortal(surf); break;
      default: Sys_Error("internal renderer error: unknown surface type %d", (int)type);
    }
  }
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

  sec_params_t *LightParams = (LightSourceSector < 0 || LightSourceSector >= Level->NumSectors ? secregion->params : &Level->Sectors[LightSourceSector].params);
  int lLev = (AbsSideLight ? 0 : LightParams->lightlevel)+SideLight;
  lLev = (FixedLight ? FixedLight : lLev+ExtraLight);
  lLev = midval(0, lLev, 255);
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
      for (int i = 0; i < Portals.length(); ++i) {
        if (Portals[i] && Portals[i]->MatchSkyBox(SkyBox)) {
          Portal = Portals[i];
          break;
        }
      }
      if (!Portal) {
        if (IsStack) {
          if (r_allow_other_portals) {
            Portal = new VSectorStackPortal(this, SkyBox);
            Portals.Append(Portal);
          }
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
      for (int i = 0; i < Portals.length(); ++i) {
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
    for (; surfs; surfs = surfs->next) {
      /*k8: ?
      if (!surfs->IsVisible(vieworg)) {
        // viewer is in back side or on plane
        //GCon->Logf("  SURF SKIP!");
        continue;
      }
      */
      Portal->Surfs.Append(surfs);
      if (doRenderSurf < 0) {
        doRenderSurf = (IsStack && CheckSkyBoxAlways && SkyBox->eventSkyBoxGetPlaneAlpha() ? 1 : 0);
      }
      //if (IsStack && CheckSkyBoxAlways && SkyBox->eventSkyBoxGetPlaneAlpha())
      if (doRenderSurf && surfs->queueframe != currQueueFrame) {
        //GCon->Logf("  SURF!");
        surfs->Light = (lLev<<24)|LightParams->LightColor;
        surfs->Fade = Fade;
        surfs->dlightframe = sub->dlightframe;
        surfs->dlightbits = sub->dlightbits;
        surfs->queueframe = currQueueFrame;
        surfs->plvisible = surfs->IsVisible(vieworg);
        if (surfs->plvisible) {
          DrawTranslucentPoly(surfs, surfs->verts, surfs->count,
            0, SkyBox->eventSkyBoxGetPlaneAlpha(), false, 0,
            false, 0, Fade, TVec(), 0, TVec(), TVec(), TVec());
        }
      }
    }
    return;
  } // done skybox rendering

  vuint32 sflight = (lLev<<24)|LightParams->LightColor;

#if 0
  if ((int)(ptrdiff_t)(sub->sector-Level->Sectors) == 40 ||
      (int)(ptrdiff_t)(sub->sector-Level->Sectors) == 0)
  {
    GCon->Logf("#%d: light=%d; ls=%d; sl=%d; asl=%d; lp->llev=%d; fixed=%d; extra=%d; remap=%d; fade=0x%08x; lc=0x%08x; sflight=0x%08x; ta=%g",
      (int)(ptrdiff_t)(sub->sector-Level->Sectors),
      lLev, LightSourceSector, SideLight, (int)AbsSideLight,
      LightParams->lightlevel, (int)FixedLight, ExtraLight, light_remap[Clamp(LightParams->lightlevel, 0, 255)], Fade, (unsigned)LightParams->LightColor, sflight, texinfo->Alpha);
    //lLev = 250;
    //Fade = 0xffffffff;
    /*
    lLev = 255;
    sflight = (lLev<<24)|(0xff7fff);
    */
    sflight = 0x1fffffff;
  }
#endif

  for (; surfs; surfs = surfs->next) {
    //if (!surfs->IsVisible(vieworg)) continue;

    surfs->Light = sflight;
    surfs->Fade = Fade;
    surfs->dlightframe = sub->dlightframe;
    surfs->dlightbits = sub->dlightbits;
    // alpha: 1.0 is masked wall, 1.1 is solid wall
    //if (texinfo->Alpha <= 1.0f) surfs->drawflags |= surface_t::DF_MASKED; else surfs->drawflags &= ~surface_t::DF_MASKED;

    if (texinfo->Alpha >= 1.0f) {
      CommonQueueSurface(surfs, 0);
    } else if (surfs->queueframe != currQueueFrame) {
      surfs->queueframe = currQueueFrame;
      surfs->plvisible = surfs->IsVisible(vieworg);
      if (surfs->plvisible) {
        DrawTranslucentPoly(surfs, surfs->verts, surfs->count,
          0, texinfo->Alpha, texinfo->Additive, 0, false, 0, Fade,
          TVec(), 0, TVec(), TVec(), TVec());
      }
    }
  }
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
  float TopZ = secregion->eceiling.splane->TexZ;
  float BotZ = secregion->efloor.splane->TexZ;
  float HorizonZ = vieworg.z;

  // handle top part
  if (TopZ > HorizonZ) {
    sec_surface_t *Ceil = (subregion->fakeceil ? subregion->fakeceil : subregion->realceil);
    if (Ceil) {
      // calculate light and fade
      sec_params_t *LightParams = Ceil->esecplane.splane->LightSourceSector != -1 ?
        &Level->Sectors[Ceil->esecplane.splane->LightSourceSector].params :
        secregion->params;
      int lLev = (FixedLight ? FixedLight : min2(255, LightParams->lightlevel+ExtraLight));
      if (r_darken) lLev = light_remap[lLev];
      vuint32 Fade = GetFade(secregion);

      surface_t *Surf = dseg->HorizonTop;
      Surf->plane = *(TPlane *)(dseg->seg);
      Surf->texinfo = &Ceil->texinfo;
      Surf->HorizonPlane = Ceil->esecplane.splane; //FIXME: 3dfloor
      Surf->Light = (lLev<<24)|LightParams->LightColor;
      Surf->Fade = Fade;
      Surf->count = 4;
      TVec *svs = &Surf->verts[0];
      svs[0] = *seg->v1; svs[0].z = max2(BotZ, HorizonZ);
      svs[1] = *seg->v1; svs[1].z = TopZ;
      svs[2] = *seg->v2; svs[2].z = TopZ;
      svs[3] = *seg->v2; svs[3].z = max2(BotZ, HorizonZ);
      if (Ceil->esecplane.splane->pic == skyflatnum) {
        // if it's a sky, render it as a regular sky surface
        DrawSurfaces(sub, secregion, nullptr, Surf, &Ceil->texinfo, secregion->eceiling.splane->SkyBox, -1,
          seg->sidedef->Light, !!(seg->sidedef->Flags&SDF_ABSLIGHT),
          false);
      } else {
        CommonQueueSurface(Surf, 2);
      }
    }
  }

  // handle bottom part
  if (BotZ < HorizonZ) {
    sec_surface_t *Floor = (subregion->fakefloor ? subregion->fakefloor : subregion->realfloor);
    if (Floor) {
      // calculate light and fade
      sec_params_t *LightParams = Floor->esecplane.splane->LightSourceSector != -1 ?
        &Level->Sectors[Floor->esecplane.splane->LightSourceSector].params :
        secregion->params;
      int lLev = (FixedLight ? FixedLight : min2(255, LightParams->lightlevel+ExtraLight));
      if (r_darken) lLev = light_remap[lLev];
      vuint32 Fade = GetFade(secregion);

      surface_t *Surf = dseg->HorizonBot;
      Surf->plane = *(TPlane *)(dseg->seg);
      Surf->texinfo = &Floor->texinfo;
      Surf->HorizonPlane = Floor->esecplane.splane; //FIXME: 3dfloor
      Surf->Light = (lLev<<24)|LightParams->LightColor;
      Surf->Fade = Fade;
      Surf->count = 4;
      TVec *svs = &Surf->verts[0];
      svs[0] = *seg->v1; svs[0].z = BotZ;
      svs[1] = *seg->v1; svs[1].z = min2(TopZ, HorizonZ);
      svs[2] = *seg->v2; svs[2].z = min2(TopZ, HorizonZ);
      svs[3] = *seg->v2; svs[3].z = BotZ;
      if (Floor->esecplane.splane->pic == skyflatnum) {
        // if it's a sky, render it as a regular sky surface
        DrawSurfaces(sub, secregion, nullptr, Surf, &Floor->texinfo, secregion->efloor.splane->SkyBox, -1,
          seg->sidedef->Light, !!(seg->sidedef->Flags&SDF_ABSLIGHT),
          false);
      } else {
        CommonQueueSurface(Surf, 2);
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
    if (!dseg->mid) return;

    VPortal *Portal = nullptr;
    for (int i = 0; i < Portals.length(); ++i) {
      if (Portals[i] && Portals[i]->MatchMirror(seg)) {
        Portal = Portals[i];
        break;
      }
    }
    if (!Portal) {
      Portal = new VMirrorPortal(this, seg);
      Portals.Append(Portal);
    }

    for (surface_t *surfs = dseg->mid->surfs; surfs; surfs = surfs->next) Portal->Surfs.Append(surfs);
  } else {
    if (dseg->mid) {
      DrawSurfaces(sub, secregion, seg, dseg->mid->surfs, &dseg->mid->texinfo,
                   secregion->eceiling.splane->SkyBox, -1, seg->sidedef->Light,
                   !!(seg->sidedef->Flags&SDF_ABSLIGHT), false);
    }
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

  if (seg->PointOnSide(vieworg)) {
    // viewer is in back side or on plane
    // gozzo 3d floors should be rendered regardless of orientation
    segpart_t *sp = dseg->extra;
    if (sp && sp->texinfo.Tex && (sp->texinfo.Alpha < 1.0f || sp->texinfo.Tex->isTransparent())) {
      side_t *sidedef = seg->sidedef;
      //GCon->Logf("00: extra for seg #%d (line #%d)", (int)(ptrdiff_t)(seg-Level->Segs), (int)(ptrdiff_t)(linedef-Level->Lines));
      for (; sp; sp = sp->next) {
        DrawSurfaces(sub, secregion, seg, sp->surfs, &sp->texinfo, secregion->eceiling.splane->SkyBox,
          -1, sidedef->Light, !!(sidedef->Flags&SDF_ABSLIGHT), false);
        //GCon->Logf("  extra for seg #%d (%p)", (int)(ptrdiff_t)(seg-Level->Segs), sp);
      }
      //GCon->Logf("01: extra for seg #%d", (int)(ptrdiff_t)(seg-Level->Segs));
    }
    return;
  }

  if (MirrorClipSegs && clip_frustum && clip_frustum_mirror && /*clip_frustum_bsp &&*/ view_frustum.planes[5].isValid()) {
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

      if (!ViewClip.IsRangeVisible(v2, v1)) return;
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

    if (!ViewClip.IsRangeVisible(v2, v1)) return;
  }
*/
  if (!ViewClip.IsRangeVisible(*seg->v2, *seg->v1)) return;

  // k8: this drops some segs that may leak without proper frustum culling
  // k8: this seems to be unnecessary now
  // k8: reenabled, because why not?
  if (clip_frustum_bsp_segs && !ViewClip.CheckSegFrustum(sub, seg)) return;

  // mark the segment as visible for auto map
#if 0
  linedef->flags |= ML_MAPPED;
#else
  if (!(linedef->flags&ML_MAPPED)) {
    // this line is at least partially mapped; let automap drawer do the rest
    linedef->exFlags |= ML_EX_PARTIALLY_MAPPED|ML_EX_CHECK_MAPPED;
  }
  seg->flags |= SF_MAPPED;
#endif

  side_t *sidedef = seg->sidedef;

  VEntity *SkyBox = secregion->eceiling.splane->SkyBox;
  if (!seg->backsector) {
    // single sided line
    if (seg->linedef->special == LNSPEC_LineHorizon && r_allow_horizons) {
      RenderHorizon(sub, secregion, subregion, dseg);
    } else if (seg->linedef->special == LNSPEC_LineMirror) {
      RenderMirror(sub, secregion, dseg);
    } else {
      if (dseg->mid) DrawSurfaces(sub, secregion, seg, dseg->mid->surfs, &dseg->mid->texinfo, SkyBox, -1, sidedef->Light, !!(sidedef->Flags&SDF_ABSLIGHT), false);
    }
    if (dseg->topsky) DrawSurfaces(sub, secregion, nullptr, dseg->topsky->surfs, &dseg->topsky->texinfo, SkyBox, -1, sidedef->Light, !!(sidedef->Flags&SDF_ABSLIGHT), false);
  } else {
    // two sided line
    if (dseg->top) DrawSurfaces(sub, secregion, seg, dseg->top->surfs, &dseg->top->texinfo, SkyBox, -1, sidedef->Light, !!(sidedef->Flags&SDF_ABSLIGHT), false);
    if (dseg->topsky) DrawSurfaces(sub, secregion, nullptr, dseg->topsky->surfs, &dseg->topsky->texinfo, SkyBox, -1, sidedef->Light, !!(sidedef->Flags&SDF_ABSLIGHT), false);
    if (dseg->bot) DrawSurfaces(sub, secregion, seg, dseg->bot->surfs, &dseg->bot->texinfo, SkyBox, -1, sidedef->Light, !!(sidedef->Flags&SDF_ABSLIGHT), false);
    if (dseg->mid) DrawSurfaces(sub, secregion, seg, dseg->mid->surfs, &dseg->mid->texinfo, SkyBox, -1, sidedef->Light, !!(sidedef->Flags&SDF_ABSLIGHT), false);
    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      DrawSurfaces(sub, secregion, seg, sp->surfs, &sp->texinfo, SkyBox, -1, sidedef->Light, !!(sidedef->Flags&SDF_ABSLIGHT), false);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderSecSurface
//
//==========================================================================
void VRenderLevelShared::RenderSecSurface (subsector_t *sub, sec_region_t *secregion, sec_surface_t *ssurf, VEntity *SkyBox) {
  if (!ssurf) return;
  TSecPlaneRef plane(ssurf->esecplane);

  if (!plane.splane->pic) return;

  if (plane.PointOnSide(vieworg)) return; // viewer is in back side or on plane

  if (r_allow_mirrors && MirrorLevel < r_maxmirrors && plane.splane->MirrorAlpha < 1.0f) {
    VPortal *Portal = nullptr;
    for (int i = 0; i < Portals.length(); ++i) {
      if (Portals[i] && Portals[i]->MatchMirror(plane.splane)) {
        Portal = Portals[i];
        break;
      }
    }
    if (!Portal) {
      Portal = new VMirrorPortal(this, plane.splane);
      Portals.Append(Portal);
    }

    for (surface_t *surfs = ssurf->surfs; surfs; surfs = surfs->next) Portal->Surfs.Append(surfs);

    if (plane.splane->MirrorAlpha <= 0.0f) return;
    // k8: is this right?
    ssurf->texinfo.Alpha = plane.splane->MirrorAlpha;
  } else {
    // this is NOT right!
    //ssurf->texinfo.Alpha = 1.0f;
    if (plane.splane->MirrorAlpha < 1.0f) {
      if (ssurf->texinfo.Alpha >= 1.0f) {
        //GCon->Logf("MALPHA=%f", plane.splane->MirrorAlpha);
        // darken it a little to simulate mirror
        sec_params_t *oldRegionLightParams = secregion->params;
        sec_params_t newLight = (plane.splane->LightSourceSector >= 0 ? Level->Sectors[plane.splane->LightSourceSector].params : *oldRegionLightParams);
        newLight.lightlevel = (int)((float)newLight.lightlevel*plane.splane->MirrorAlpha);
        secregion->params = &newLight;
        // take light from `secregion->params`
        DrawSurfaces(sub, secregion, nullptr, ssurf->surfs, &ssurf->texinfo, SkyBox, -1, 0, false, true);
        // and resore rregion
        secregion->params = oldRegionLightParams;
        return;
      }
    }
  }

  DrawSurfaces(sub, secregion, nullptr, ssurf->surfs, &ssurf->texinfo, SkyBox, plane.splane->LightSourceSector, 0, false, true);
}


//==========================================================================
//
//  VRenderLevelShared::NeedToRenderNextSubFirst
//
//  k8: i don't know what Janis wanted to with this
//
//==========================================================================
bool VRenderLevelShared::NeedToRenderNextSubFirst (const subregion_t *region) {
  if (!region->next || !r_ordered_subregions) return false;
  const sec_surface_t *floor = region->fakefloor;
  if (floor) {
    const float d = floor->PointDist(vieworg);
    if (d <= 0.0f) return true;
  }
  floor = region->realfloor;
  if (floor) {
    const float d = floor->PointDist(vieworg);
    if (d <= 0.0f) return true;
  }
  return false;
}


//==========================================================================
//
//  VRenderLevelShared::AddPolyObjToClipper
//
//  we have to do this separately, because for now we have to add
//  invisible segs to clipper too
//  i don't yet know why
//
//==========================================================================
void VRenderLevelShared::AddPolyObjToClipper (VViewClipper &clip, subsector_t *sub) {
  if (sub && sub->poly && r_draw_pobj && clip_use_1d_clipper) {
    seg_t **polySeg = sub->poly->segs;
    for (int polyCount = sub->poly->numsegs; polyCount--; ++polySeg) {
      seg_t *seg = (*polySeg)->drawsegs->seg;
      if (seg->linedef) {
        clip.CheckAddClipSeg(seg, (MirrorClipSegs && view_frustum.planes[5].isValid() ? &view_frustum.planes[5] : nullptr));
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderPolyObj
//
//  render the polyobj in the subsector, and optionally add it to clipper
//  this blocks view with polydoors
//
//==========================================================================
void VRenderLevelShared::RenderPolyObj (subsector_t *sub) {
  if (sub && sub->poly && r_draw_pobj) {
    subregion_t *region = sub->regions;
    sec_region_t *secregion = region->secregion;
    seg_t **polySeg = sub->poly->segs;
    for (int polyCount = sub->poly->numsegs; polyCount--; ++polySeg) {
      RenderLine(sub, secregion, region, (*polySeg)->drawsegs);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderSubRegion
//
//  Determine floor/ceiling planes.
//  Draw one or more line segments.
//
//==========================================================================
void VRenderLevelShared::RenderSubRegion (subsector_t *sub, subregion_t *region) {
  sec_region_t *secregion = region->secregion;

  const bool nextFirst = NeedToRenderNextSubFirst(region);
  if (nextFirst) RenderSubRegion(sub, region->next);

  bool drawFloors = true;
  if (ViewClip.ClipCheckRegion(region, sub)) {
    int count = sub->numlines;
    drawseg_t *ds = region->lines;
    while (count--) {
      RenderLine(sub, secregion, region, ds);
      ++ds;
    }
  } else {
    // if there is no polyobj here, we can skip rendering floors
    if (!sub || !sub->poly || !r_draw_pobj) drawFloors = false;
  }

  if (drawFloors || r_dbg_always_draw_flats) {
    sec_surface_t *fsurf[4];
    GetFlatSetToRender(sub, region, fsurf);

    if (fsurf[0]) RenderSecSurface(sub, secregion, fsurf[0], secregion->efloor.splane->SkyBox);
    if (fsurf[1]) RenderSecSurface(sub, secregion, fsurf[1], secregion->efloor.splane->SkyBox);

    if (fsurf[2]) RenderSecSurface(sub, secregion, fsurf[2], secregion->eceiling.splane->SkyBox);
    if (fsurf[3]) RenderSecSurface(sub, secregion, fsurf[3], secregion->eceiling.splane->SkyBox);
  }

  if (!nextFirst && region->next) return RenderSubRegion(sub, region->next);
}


//==========================================================================
//
//  VRenderLevelShared::RenderMarkAdjSubsectorsThings
//
//  used for "better things rendering"
//
//  TODO: this should be replaced with "touching subsectors" list instead
//
//==========================================================================
void VRenderLevelShared::RenderMarkAdjSubsectorsThings (int num) {
  BspVisThing[((unsigned)num)>>3] |= 1U<<(num&7);
  if (r_draw_adjacent_subsector_things) {
    subsector_t *sub = &Level->Subsectors[num];
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
}


//==========================================================================
//
//  VRenderLevelShared::RenderSubsector
//
//==========================================================================
void VRenderLevelShared::RenderSubsector (int num, bool onlyClip) {
  subsector_t *sub = &Level->Subsectors[num];
  if (!sub->sector->linecount) return; // skip sectors containing original polyobjs

  // render it if we're not in "only clip" mode
  if (!onlyClip) {
    // if we have PVS, `MarkLeaves()` marks potentially visible subsectors
    if (Level->HasPVS() && sub->VisFrame != currVisFrame) {
      if (r_draw_adjacent_subsector_things) {
        if (!clip_use_1d_clipper || ViewClip.ClipCheckSubsector(sub)) {
          RenderMarkAdjSubsectorsThings(num);
          if (clip_use_1d_clipper) ViewClip.ClipAddSubsectorSegs(sub, (MirrorClipSegs && view_frustum.planes[5].isValid() ? &view_frustum.planes[5] : nullptr));
        }
      } else if (clip_use_1d_clipper) {
        ViewClip.ClipAddSubsectorSegs(sub, (MirrorClipSegs && view_frustum.planes[5].isValid() ? &view_frustum.planes[5] : nullptr));
      }
      return;
    }

    // is this subsector potentially visible?
    if (ViewClip.ClipCheckSubsector(sub)) {
      // mark as rendered
      sub->miscFlags |= subsector_t::SSMF_Rendered;

      if (sub->parent) sub->parent->VisFrame = currVisFrame; // for one-sector degenerate maps
      sub->VisFrame = currVisFrame;

      // mark this subsector as rendered
      BspVis[((unsigned)num)>>3] |= 1U<<(num&7);

      // mark thing subsectors
      RenderMarkAdjSubsectorsThings(num);

      // update world
#if 0
      if (w_update_in_renderer && sub->updateWorldFrame != updateWorldFrame) {
        UpdateSubsector(num, nullptr); // trigger BSP updating
      }
#else
      if (sub->updateWorldFrame != updateWorldFrame) {
        sub->updateWorldFrame = updateWorldFrame;
        UpdateSubRegion(sub, sub->regions);
      }
#endif

      // render the polyobj in the subsector first, and add it to clipper
      // this blocks view with polydoors
      RenderPolyObj(sub);
      AddPolyObjToClipper(ViewClip, sub);
      RenderSubRegion(sub, sub->regions);
    }
  }

  // add subsector's segs to the clipper
  // clipping against mirror is done only for vertical mirror planes
  if (clip_use_1d_clipper) {
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
void VRenderLevelShared::RenderBSPNode (int bspnum, const float bbox[6], unsigned clipflags, bool onlyClip) {
 tailcall:
#ifdef VV_CLIPPER_FULL_CHECK
  if (ViewClip.ClipIsFull()) return;
#endif

  if (bspnum == -1) {
    RenderSubsector(0, onlyClip);
    return;
  }

  if (!ViewClip.ClipIsBBoxVisible(bbox)) return;

  if (!onlyClip) {
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
        auto crs = cp->checkBoxEx(bbox);
        if (crs == 1) clipflags ^= cp->clipflag; // if it is on a front side of this plane, don't bother checking with it anymore
        else if (crs == 0) {
          // it is enough to hit at least one "outside" to be completely outside
          // add this box to clipper, why not
          // k8: nope; this glitches
          //if (clip_use_1d_clipper) ViewClip.ClipAddBBox(bbox);
          if (!clip_use_1d_clipper) return;
          onlyClip = true;
          break;
          //return;
        }
      }
    }
  }

  // found a subsector?
  if ((bspnum&NF_SUBSECTOR) == 0) {
    // nope
    node_t *bsp = &Level->Nodes[bspnum];
    // decide which side the view point is on
    const float dist = DotProduct(vieworg, bsp->normal)-bsp->dist;
    unsigned side = (unsigned)(dist <= 0.0f);
    // mark this node as rendered (if we're going to render it)
    if (!onlyClip && (bsp->children[side]&NF_SUBSECTOR)) bsp->VisFrame = currVisFrame;
    // recursively divide front space (towards the viewer)
    RenderBSPNode(bsp->children[side], bsp->bbox[side], clipflags, onlyClip);
    // recursively divide back space (away from the viewer)
    side ^= 1;
    //return RenderBSPNode(bsp->children[side], bsp->bbox[side], clipflags, onlyClip);
    bspnum = bsp->children[side];
    bbox = bsp->bbox[side];
    goto tailcall;
  } else {
    return RenderSubsector(bspnum&(~NF_SUBSECTOR), onlyClip);
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

    // head node is the last node output
    RenderBSPNode(Level->NumNodes-1, dummy_bbox, (MirrorClip ? 0x3f : 0x1f));

    if (PortalLevel == 0) {
      // draw the most complex sky portal behind the scene first, without the need to use stencil buffer
      VPortal *BestSky = nullptr;
      int BestSkyIndex = -1;
      for (int i = 0; i < Portals.length(); ++i) {
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

      world_surf_t *wsurf = WorldSurfs.ptr();
      for (int i = WorldSurfs.length(); i--; ++wsurf) {
        switch (wsurf->Type) {
          case 0: QueueWorldSurface(wsurf->Surf); break;
          case 1: QueueSkyPortal(wsurf->Surf); break;
          case 2: QueueHorizonPortal(wsurf->Surf); break;
          default: Sys_Error("invalid queued 0-level world surface type %d", (int)wsurf->Type);
        }
      }
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
      for (int i = 0; i < Portals.length(); ++i) {
        if (Portals[i]) {
          delete Portals[i];
          Portals[i] = nullptr;
        }
      }
      Portals.reset();
      // save cvars
      oldMaxMirrors = r_maxmirrors;
      oldPortalDepth = r_max_portal_depth;
      oldHorizons = r_allow_horizons;
      oldMirrors = r_allow_mirrors;
      return;
    }
  }

  ++PortalLevel;

#if 0
  if (/*dbg_dump_portal_list &&*/ PortalLevel == 1) {
    for (int f = 0; f < Portals.length(); ++f) {
      if (Portals[f]) GCon->Logf("PORTAL #%d: %s", f, *shitppTypeNameObj(*Portals[f]));
    }
  }
#endif

  if (r_max_portal_depth < 0 || PortalLevel <= r_max_portal_depth) {
    //FIXME: disable decals for portals
    //       i should rewrite decal rendering, so we can skip stencil buffer
    //       (or emulate stencil buffer with texture and shaders)
    bool oldDecalsEnabled = r_decals_enabled;
    r_decals_enabled = false;
    for (int i = 0; i < Portals.length(); ++i) {
      if (Portals[i] && Portals[i]->Level == PortalLevel) {
        if (r_allow_other_portals || Portals[i]->IsMirror()) {
          Portals[i]->Draw(true);
        }
      }
    }
    r_decals_enabled = oldDecalsEnabled;
  } else {
    if (dbg_max_portal_depth_warning) GCon->Logf(NAME_Warning, "portal level too deep (%d)", PortalLevel);
  }

  for (int i = 0; i < Portals.length(); ++i) {
    if (Portals[i] && Portals[i]->Level == PortalLevel) {
      delete Portals[i];
      Portals[i] = nullptr;
    }
  }

  --PortalLevel;
  if (PortalLevel == 0) Portals.reset();
}
