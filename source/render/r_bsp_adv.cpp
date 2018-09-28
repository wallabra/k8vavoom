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

//==========================================================================
//
//  VAdvancedRenderLevel::QueueWorldSurface
//
//==========================================================================
void VAdvancedRenderLevel::QueueWorldSurface (seg_t *seg, surface_t *surf) {
  guard(VAdvancedRenderLevel::QueueWorldSurface);
  QueueSimpleSurf(seg, surf);
  unguard;
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderWorld
//
//==========================================================================
void VAdvancedRenderLevel::RenderWorld (const refdef_t *rd, const VViewClipper *Range) {
  guard(VAdvancedRenderLevel::RenderWorld);
  RenderBspWorld(rd, Range);
  Drawer->DrawWorldAmbientPass();
  RenderPortals();
  unguard;
}
