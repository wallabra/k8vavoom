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
//**
//**  BSP traversal, handling of LineSegs for rendering.
//**
//**************************************************************************
#include "gamedefs.h"
#include "r_local.h"

//==========================================================================
//
//  VRenderLevelLightmap::QueueWorldSurface
//
//==========================================================================
void VRenderLevelLightmap::QueueWorldSurface (surface_t *surf) {
  if (!SurfPrepareForRender(surf)) return;
  if (!surf->IsPlVisible()) return;

  bool lightmaped = (surf->lightmap != nullptr || surf->dlightframe == currDLightFrame);

  // check if static lightmap recalc requested
  if (surf->drawflags&surface_t::DF_CALC_LMAP) {
    //GCon->Logf("%p: Need to calculate static lightmap for subsector %p!", surf, surf->subsector);
    LightFaceTimeCheckedFreeCaches(surf);
    lightmaped = (surf->lightmap != nullptr || surf->dlightframe == currDLightFrame);
  }

  if (lightmaped) {
    if (QueueLMapSurface(surf)) return;
    // cannot do lightmapping, draw as normal surface instead
    // this is ugly, but much better than lost surfaces
  }

  QueueSimpleSurf(surf);
}


//==========================================================================
//
//  VRenderLevelLightmap::RenderCollectSurfaces
//
//  this does BSP traversing, and collect world surfaces into various
//  lists to drive GPU rendering
//
//==========================================================================
void VRenderLevelLightmap::RenderCollectSurfaces (const refdef_t *rd, const VViewClipper *Range) {
  //GCon->Logf(NAME_Debug, "::: VRenderLevelLightmap::RenderCollectSurfaces ::: (%d)", currQueueFrame);

  MiniStopTimer profPrep("PrepareWorldRender", prof_r_world_prepare.asBool());
  PrepareWorldRender(rd, Range);
  profPrep.stopAndReport();

  MiniStopTimer profBSPCollect("RenderBspWorld", prof_r_bsp_collect.asBool());
  RenderBspWorld(rd, Range);
  profBSPCollect.stopAndReport();

  ProcessCachedSurfaces();
}
