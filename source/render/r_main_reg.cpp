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
//**  Rendering main loop and setup functions, utility functions (BSP,
//**  geometry, trigonometry). See tables.c, too.
//**
//**************************************************************************
#include "gamedefs.h"
#include "r_local.h"

static VCvarB r_reg_disable_things("r_reg_disable_things", false, "Disable rendering of things (regular renderer).", 0/*CVAR_Archive*/);
extern VCvarB w_update_in_renderer;

extern int light_reset_surface_cache; // in r_light_reg.cpp


//==========================================================================
//
//  VRenderLevel::VRenderLevel
//
//==========================================================================
VRenderLevel::VRenderLevel (VLevel *ALevel)
  : VRenderLevelShared(ALevel)
  , c_subdivides(0)
  , c_seg_div(0)
  , freeblocks(nullptr)
{
  NeedsInfiniteFarClip = false;
  mIsAdvancedRenderer = false;
  showCreateWorldSurfProgress = true; // we want it
  updateWorldCheckVisFrame = true; // we want it

  memset(cacheblocks, 0, sizeof(cacheblocks));
  memset(blockbuf, 0, sizeof(blockbuf));

  FlushCaches();
}


//==========================================================================
//
//  VRenderLevel::RenderScene
//
//==========================================================================
void VRenderLevel::RenderScene (const refdef_t *RD, const VViewClipper *Range) {
  guard(VRenderLevel::RenderScene);
  r_viewleaf = Level->PointInSubsector(vieworg);

  TransformFrustum();

  Drawer->SetupViewOrg();

  if (times_render_highlevel) GCon->Log("========= RenderScene =========");

  double stt = -Sys_Time();
  MarkLeaves();
  stt += Sys_Time();
  if (times_render_highlevel) GCon->Logf("MarkLeaves: %f", stt);

  if (!r_disable_world_update) {
    stt = -Sys_Time();
    UpdateWorld(RD, Range);
    stt += Sys_Time();
    if (times_render_highlevel) GCon->Logf("UpdateWorld: %f", stt);
  }

  stt = -Sys_Time();
  RenderWorld(RD, Range);
  if (light_reset_surface_cache != 0) return;
  stt += Sys_Time();
  if (times_render_highlevel) GCon->Logf("RenderWorld: %f", stt);

  stt = -Sys_Time();
  if (!r_reg_disable_things) RenderMobjs(RPASS_Normal);
  stt += Sys_Time();
  if (times_render_highlevel) GCon->Logf("RenderMobjs: %f", stt);

  DrawParticles();

  DrawTranslucentPolys();
  unguard;
}
