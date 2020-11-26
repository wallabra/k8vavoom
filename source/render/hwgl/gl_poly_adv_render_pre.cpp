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
#include "gl_local.h"
#include "gl_poly_adv_render.h"


//==========================================================================
//
//  VOpenGLDrawer::BeforeDrawWorldSV
//
//  populate VBO with world surfaces
//
//==========================================================================
void VOpenGLDrawer::BeforeDrawWorldSV () {
  VRenderLevelDrawer::DrawLists &dls = RendLev->GetCurrentDLS();

  if (dls.DrawSurfListSolid.length() == 0 && dls.DrawSurfListMasked.length() == 0) return;

  // reserve room for max number of elements in VBO, because why not?
  int maxEls = 0;

  // precalculate various surface info
  for (int f = 0; f < dls.DrawSurfListSolid.length(); ) {
    surface_t *surf = dls.DrawSurfListSolid.ptr()[f];
    /*
    surf->gp.clear();
    surf->shaderClass = -1; // so they will float up
    */
    if (!surf->IsPlVisible()) { dls.DrawSurfListSolid.removeAt(f); continue; }; // viewer is in back side or on plane
    if (surf->count < 3) { dls.DrawSurfListSolid.removeAt(f); continue; }
    if (surf->drawflags&surface_t::DF_MASKED) { dls.DrawSurfListSolid.removeAt(f); continue; } // this should not end up here

    // don't render translucent surfaces
    // they should not end up here, but...
    const texinfo_t *currTexinfo = surf->texinfo;
    if (!currTexinfo || currTexinfo->isEmptyTexture()) { dls.DrawSurfListSolid.removeAt(f); continue; } // just in case
    if (currTexinfo->Alpha < 1.0f || currTexinfo->Additive) { dls.DrawSurfListSolid.removeAt(f); continue; } // just in case

    CalcGlow(surf->gp, surf);
    surf->shaderClass = ClassifySurfaceShader(surf);
    maxEls += surf->count;
    ++f;
  }

  for (int f = 0; f < dls.DrawSurfListMasked.length(); ) {
    surface_t *surf = dls.DrawSurfListMasked.ptr()[f];
    /*
    surf->gp.clear();
    surf->shaderClass = -1; // so they will float up
    */
    if (!surf->IsPlVisible()) { dls.DrawSurfListMasked.removeAt(f); continue; } // viewer is in back side or on plane
    if (surf->count < 3) { dls.DrawSurfListMasked.removeAt(f); continue; }
    if ((surf->drawflags&surface_t::DF_MASKED) == 0) { dls.DrawSurfListMasked.removeAt(f); continue; } // this should not end up here

    // don't render translucent surfaces
    // they should not end up here, but...
    const texinfo_t *currTexinfo = surf->texinfo;
    if (!currTexinfo || currTexinfo->isEmptyTexture()) { dls.DrawSurfListMasked.removeAt(f); continue; } // just in case
    if (currTexinfo->Alpha < 1.0f || currTexinfo->Additive) { dls.DrawSurfListMasked.removeAt(f); continue; } // just in case

    CalcGlow(surf->gp, surf);
    surf->shaderClass = ClassifySurfaceShader(surf);
    maxEls += surf->count;
    ++f;
  }

  if (dls.DrawSurfListSolid.length() == 0 && dls.DrawSurfListMasked.length() == 0) return;

  vassert(maxEls > 0);

  // put into VBO
  vboAdvSurf.ensure(maxEls);
  int vboIdx = 0;
  TVec *dest = vboAdvSurf.data.ptr();
  // solid surfaces
  for (auto &&surf : dls.DrawSurfListSolid) {
    surf->firstIndex = vboIdx;
    const unsigned len = (unsigned)surf->count;
    for (unsigned i = 0; i < len; ++i) *dest++ = surf->verts[i].vec();
    vboIdx += surf->count;
  }
  // masked surfaces
  for (auto &&surf : dls.DrawSurfListMasked) {
    surf->firstIndex = vboIdx;
    const unsigned len = (unsigned)surf->count;
    for (unsigned i = 0; i < len; ++i) *dest++ = surf->verts[i].vec();
    vboIdx += surf->count;
  }
  vassert(vboIdx <= vboAdvSurf.capacity());

  // upload data
  vboAdvSurf.uploadData(vboIdx);

  // turn off VBO for now
  vboAdvSurf.deactivate();

  const int counterLength = dls.DrawSurfListSolid.length()+dls.DrawSurfListMasked.length()+4;
  if (vboCounters.length() < counterLength) {
    vboCounters.setLength(counterLength);
    vboStartInds.setLength(counterLength);
  }

  if (gl_dbg_vbo_adv_ambient) GCon->Logf(NAME_Debug, "=== ambsurface VBO: maxEls=%d; maxcnt=%d ===", maxEls, counterLength);
}
