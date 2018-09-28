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
//**  Rendering main loop and setup functions, utility functions (BSP,
//**  geometry, trigonometry). See tables.c, too.
//**
//**************************************************************************
#include "gamedefs.h"
#include "r_local.h"


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

  memset(cacheblocks, 0, sizeof(cacheblocks));
  memset(blockbuf, 0, sizeof(blockbuf));

  FlushCaches();

  memset(DLights, 0, sizeof(DLights));
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

#ifdef VAVOOM_RENDER_TIMES
  double stt = -Sys_Time();
#endif
  MarkLeaves();
#ifdef VAVOOM_RENDER_TIMES
  stt += Sys_Time();
  GCon->Logf("  MarkLeaves: %f", stt);
#endif

#ifdef VAVOOM_RENDER_TIMES
  stt = -Sys_Time();
#endif
  UpdateWorld(RD, Range);
#ifdef VAVOOM_RENDER_TIMES
  stt += Sys_Time();
  GCon->Logf("  UpdateWorld: %f", stt);
#endif

#ifdef VAVOOM_RENDER_TIMES
  stt = -Sys_Time();
#endif
  RenderWorld(RD, Range);
#ifdef VAVOOM_RENDER_TIMES
  stt += Sys_Time();
  GCon->Logf("  RenderWorld: %f", stt);
#endif

#ifdef VAVOOM_RENDER_TIMES
  stt = -Sys_Time();
#endif
  RenderMobjs(RPASS_Normal);
#ifdef VAVOOM_RENDER_TIMES
  stt += Sys_Time();
  GCon->Logf("  RenderMobjs: %f", stt);
#endif

  DrawParticles();

  DrawTranslucentPolys();
  unguard;
}
