//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
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
//  VRenderLevel::QueueWorldSurface
//
//==========================================================================
void VRenderLevel::QueueWorldSurface (seg_t *seg, surface_t *surf) {
  guard(VRenderLevel::QueueWorldSurface);
  bool lightmaped = (surf->lightmap != nullptr || surf->dlightframe == r_dlightframecount);
  surf->dcseg = seg;

  if (lightmaped) {
    if (CacheSurface(surf)) return;
    // cannot do lightmapping, draw as normal surface instead
    // this is ugly, but much better than lost surfaces
  }

  QueueSimpleSurf(seg, surf);
  unguard;
}


//==========================================================================
//
//  VRenderLevel::RenderWorld
//
//==========================================================================
#ifdef VAVOOM_LOWLEVEL_RENDER_TIMES
extern vuint32 glWDPolyTotal;
extern vuint32 glWDVertexTotal;
extern vuint32 glWDTextureChangesTotal;
#endif

void VRenderLevel::RenderWorld (const refdef_t *rd, const VViewClipper *Range) {
  guard(VRenderLevel::RenderWorld);

#ifdef VAVOOM_LOWLEVEL_RENDER_TIMES
  double stt = -Sys_Time();
#endif
  RenderBspWorld(rd, Range);
#ifdef VAVOOM_LOWLEVEL_RENDER_TIMES
  stt += Sys_Time();
  GCon->Logf("   RenderBspWorld: %f", stt);
#endif

#ifdef VAVOOM_LOWLEVEL_RENDER_TIMES
  glWDPolyTotal = 0;
  glWDVertexTotal = 0;
  glWDTextureChangesTotal = 0;
  stt = -Sys_Time();
#endif
  Drawer->WorldDrawing();
#ifdef VAVOOM_LOWLEVEL_RENDER_TIMES
  stt += Sys_Time();
  GCon->Logf("   Drawer->WorldDrawing: %f (%u polys, %u vertices, %u texture changes)", stt, glWDPolyTotal, glWDVertexTotal, glWDTextureChangesTotal);
#endif

  //stt = -Sys_Time();
  RenderPortals();
  //stt += Sys_Time();
  //GCon->Logf("   RenderPortals: %f", stt);

  unguard;
}
