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
#include "gamedefs.h"
#include "r_local.h"


//==========================================================================
//
//  VAdvancedRenderLevel::InitSurfs
//
//==========================================================================
void VAdvancedRenderLevel::InitSurfs (surface_t *surfs, texinfo_t *texinfo, TPlane *plane, subsector_t *sub) {
  guard(VAdvancedRenderLevel::InitSurfs);
  // it's always one surface
  if (surfs && plane) {
    surfs->texinfo = texinfo;
    surfs->plane = plane;
  }
  unguard;
}


//==========================================================================
//
//  VAdvancedRenderLevel::SubdivideFace
//
//==========================================================================
surface_t *VAdvancedRenderLevel::SubdivideFace (surface_t *f, const TVec&, const TVec *, subsector_t *) {
  // advanced renderer can draw whole surface
  return f;
}


//==========================================================================
//
//  VAdvancedRenderLevel::SubdivideSeg
//
//==========================================================================
surface_t *VAdvancedRenderLevel::SubdivideSeg (surface_t *surf, const TVec &, const TVec *, seg_t *, subsector_t *) {
  // advanced renderer can draw whole surface
  return surf;
}


//==========================================================================
//
//  VAdvancedRenderLevel::PreRender
//
//==========================================================================
void VAdvancedRenderLevel::PreRender () {
  guard(VAdvancedRenderLevel::PreRender);
  CreateWorldSurfaces();
  unguard;
}
