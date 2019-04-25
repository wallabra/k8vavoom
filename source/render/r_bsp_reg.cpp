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

static VCvarB r_reg_disable_world("r_reg_disable_world", false, "Disable rendering of world (regular renderer).", 0/*CVAR_Archive*/);
static VCvarB r_reg_disable_portals("r_reg_disable_portals", false, "Disable rendering of portals (regular renderer).", 0/*CVAR_Archive*/);
static VCvarB dbg_show_dlight_trace_info("dbg_show_dlight_trace_info", false, "Show number of properly traced dynlights per frame.", 0/*CVAR_Archive*/);

extern int light_reset_surface_cache; // in r_light_reg.cpp


//==========================================================================
//
//  VRenderLevel::QueueWorldSurface
//
//==========================================================================
void VRenderLevel::QueueWorldSurface (surface_t *surf) {
  bool lightmaped = (surf->lightmap != nullptr || surf->dlightframe == currDLightFrame);

  if (surf->drawflags&surface_t::DF_CALC_LMAP) {
    if (surf->CacheSurf) FreeSurfCache(surf->CacheSurf);
    //if (surf->subsector) GCon->Logf("relighting subsector %d", (int)(ptrdiff_t)(surf->subsector-Level->Subsectors));
    surf->drawflags &= ~surface_t::DF_CALC_LMAP;
    //GCon->Logf("%p: Need to calculate static lightmap for subsector %p!", surf, surf->subsector);
    if (surf->subsector) {
      LightFace(surf, surf->subsector);
      if (!lightmaped && surf->lightmap) lightmaped = true;
    }
  }

  if (lightmaped) {
    if (CacheSurface(surf)) return;
    // cannot do lightmapping, draw as normal surface instead
    // this is ugly, but much better than lost surfaces
  }

  QueueSimpleSurf(surf);
}


//==========================================================================
//
//  VRenderLevel::RenderWorld
//
//==========================================================================
extern vuint32 glWDPolyTotal;
extern vuint32 glWDVertexTotal;
extern vuint32 glWDTextureChangesTotal;

extern vuint32 gf_dynlights_processed;
extern vuint32 gf_dynlights_traced;


void VRenderLevel::RenderWorld (const refdef_t *rd, const VViewClipper *Range) {
  ClearQueues();

  gf_dynlights_processed = 0;
  gf_dynlights_traced = 0;

  double stt = -Sys_Time();
  RenderBspWorld(rd, Range);
  stt += Sys_Time();
  if (times_render_lowlevel) GCon->Logf("RenderBspWorld: %f", stt);
  if (light_reset_surface_cache != 0) return;

  glWDPolyTotal = 0;
  glWDVertexTotal = 0;
  glWDTextureChangesTotal = 0;
  if (!r_reg_disable_world) {
    //GCon->Logf("vfz: %f", viewforward.z);
    stt = -Sys_Time();
    Drawer->WorldDrawing();
    stt += Sys_Time();
    if (times_render_lowlevel) GCon->Logf("Drawer->WorldDrawing: %f (%u polys, %u vertices, %u texture changes)", stt, glWDPolyTotal, glWDVertexTotal, glWDTextureChangesTotal);
  }

  /*
  if (!r_reg_disable_portals) {
    stt = -Sys_Time();
    RenderPortals();
    stt += Sys_Time();
    if (times_render_lowlevel && stt > 0.01) GCon->Logf("   RenderPortals: %f", stt);
  }
  */

  if (dbg_show_dlight_trace_info) GCon->Logf("DYNLIGHT: %u total, %u traced", gf_dynlights_processed, gf_dynlights_traced);
}
