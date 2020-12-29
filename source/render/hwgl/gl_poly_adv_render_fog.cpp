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
//  VOpenGLDrawer::DrawWorldFogPass
//
//==========================================================================
void VOpenGLDrawer::DrawWorldFogPass () {
  if (gl_dbg_wireframe) return;
  GLEnableBlend();
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // fog is not premultiplied
  //glDepthMask(GL_FALSE); // no z-buffer writes
  GLDisableDepthWrite();

  // draw surfaces
  //ShadowsFog.Activate();
  //ShadowsFog.SetFogType();

  if (!gl_dbg_adv_render_surface_fog) return;

  VRenderLevelDrawer::DrawLists &dls = RendLev->GetCurrentDLS();
  if (dls.DrawSurfListSolid.length() == 0 && dls.DrawSurfListMasked.length() == 0) return;

  /*
  ShadowsFog.SetTexture(0);
  ShadowsFog.SetFogFade(lastFade, 1.0f);
  */

  vboAdvSurf.activate();
  GLuint attribPosition = 0; /* shut up, gcc! */

  int vboCountIdx = 0; // element (counter) index


  texinfo_t lastTexinfo;
  lastTexinfo.initLastUsed();

  bool lastCullFace = true;
  glEnable(GL_CULL_FACE);

  // normal
  if (dls.DrawSurfListSolid.length() != 0) {
    lastTexinfo.resetLastUsed();
    ShadowsFog.Activate();
    ShadowsFog.SetFogFade(0, 1.0f);
    attribPosition = ShadowsFog.loc_Position;
    vboAdvSurf.enableAttrib(attribPosition);
    vuint32 lastFade = 0;
    glDisable(GL_TEXTURE_2D);
    for (auto &&surf : dls.DrawSurfListSolid) {
      if (!surf->Fade) continue;
      if (lastFade != surf->Fade) {
        SADV_FLUSH_VBO();
        lastFade = surf->Fade;
        ShadowsFog.SetFogFade(surf->Fade, 1.0f);
      }
      SADV_DO_RENDER();
    }
    SADV_FLUSH_VBO();
    vboAdvSurf.disableAttrib(attribPosition);
    glEnable(GL_TEXTURE_2D);
  }

  // masked
  if (dls.DrawSurfListMasked.length() != 0) {
    lastTexinfo.resetLastUsed();
    ShadowsFogMasked.Activate();
    ShadowsFogMasked.SetFogFade(0, 1.0f);
    ShadowsFogMasked.SetTexture(0);
    attribPosition = ShadowsFog.loc_Position;
    vboAdvSurf.enableAttrib(attribPosition);
    vuint32 lastFade = 0;
    for (auto &&surf : dls.DrawSurfListMasked) {
      if (!surf->Fade) continue;
      if (lastFade != surf->Fade) {
        SADV_FLUSH_VBO();
        lastFade = surf->Fade;
        ShadowsFogMasked.SetFogFade(surf->Fade, 1.0f);
      }
      SADV_CHECK_TEXTURE(ShadowsFogMasked);
      SADV_DO_RENDER();
    }
    SADV_FLUSH_VBO();
    vboAdvSurf.disableAttrib(attribPosition);
  }

  //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // for premultiplied

  vboAdvSurf.deactivate();
}


//==========================================================================
//
//  VOpenGLDrawer::EndFogPass
//
//==========================================================================
void VOpenGLDrawer::EndFogPass () {
  //GLDisableBlend();
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // for premultiplied
  // back to normal z-buffering
  //glDepthMask(GL_TRUE); // allow z-buffer writes
  GLEnableDepthWrite();
  RestoreDepthFunc();
}
