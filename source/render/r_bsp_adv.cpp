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
//**  BSP traversal, handling of LineSegs for rendering.
//**
//**************************************************************************
#include "../gamedefs.h"
#include "r_local.h"


//==========================================================================
//
//  VRenderLevelShadowVolume::QueueWorldSurface
//
//==========================================================================
void VRenderLevelShadowVolume::QueueWorldSurface (surface_t *surf) {
  if (!SurfPrepareForRender(surf)) return;
  if (!surf->IsPlVisible()) return;
  QueueSimpleSurf(surf);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::ProcessCachedSurfaces
//
//==========================================================================
void VRenderLevelShadowVolume::ProcessCachedSurfaces () {
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderCollectSurfaces
//
//  this does BSP traversing, and collect world surfaces into various
//  lists to drive GPU rendering
//
//==========================================================================
void VRenderLevelShadowVolume::RenderCollectSurfaces (const refdef_t *rd, const VViewClipper *Range) {
  MiniStopTimer profPrep("PrepareWorldRender", prof_r_world_prepare.asBool());
  PrepareWorldRender(rd, Range);
  profPrep.stopAndReport();

  MiniStopTimer profBSPCollect("RenderBspWorld", prof_r_bsp_collect.asBool());
  RenderBspWorld(rd, Range);
  profBSPCollect.stopAndReport();

  ProcessCachedSurfaces();
}
