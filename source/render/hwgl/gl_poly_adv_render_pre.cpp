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
#include "gl_local.h"
#include "gl_poly_adv_render.h"


//==========================================================================
//
//  VOpenGLDrawer::vboAdvAppendSurface
//
//==========================================================================
void VOpenGLDrawer::vboAdvAppendSurface (surface_t *surf) {
  CalcGlow(surf->gp, surf);
  surf->shaderClass = ClassifySurfaceShader(surf);
  surf->firstIndex = vboAdvSurf.dataUsed();
  const SurfVertex *svt = surf->verts;
  for (int f = surf->count; f--; ++svt) {
    TVec *v = vboAdvSurf.allocPtrSafe();
    *v = svt->vec();
  }
}


//==========================================================================
//
//  VOpenGLDrawer::BeforeDrawWorldSV
//
//  populate VBO with world surfaces
//
//==========================================================================
void VOpenGLDrawer::BeforeDrawWorldSV () {
  VRenderLevelDrawer::DrawLists &dls = RendLev->GetCurrentDLS();

  const int counterLength = dls.DrawSurfListSolid.length()+dls.DrawSurfListMasked.length();
  if (counterLength == 0) return;

  // there is no need to check various conditions here,
  // surface collectors SHOULD make sure that everything is ok

  vboAdvSurf.ensureDataSize(counterLength, 1024);
  vboAdvSurf.allocReset();

  // solid surfaces
  for (auto &&surf : dls.DrawSurfListSolid) vboAdvAppendSurface(surf);
  // masked surfaces
  for (auto &&surf : dls.DrawSurfListMasked) vboAdvAppendSurface(surf);
  // upload data
  vboAdvSurf.uploadData();
  // and turn off VBO for now
  vboAdvSurf.deactivate();

  if (vboCounters.length() < counterLength) {
    vboCounters.setLength(counterLength+1024);
    vboStartInds.setLength(counterLength+1024);
  }

  if (gl_dbg_vbo_adv_ambient) GCon->Logf(NAME_Debug, "=== ambsurface VBO: %d/%d ===", vboAdvSurf.dataUsed(), vboAdvSurf.capacity());
}
