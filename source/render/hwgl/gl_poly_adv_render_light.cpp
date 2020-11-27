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

extern VCvarB r_shadowmaps;


#define SETUP_LIGHT_SHADER(shad_)  do { \
  (shad_).Activate(); \
  (shad_).SetLightPos(LightPos); \
  (shad_).SetLightRadius(Radius); \
  (shad_).SetViewOrigin(vieworg.x, vieworg.y, vieworg.z); \
  (shad_).SetTexture(0); \
  if (!gl_dbg_advlight_debug) { \
    (shad_).SetLightMin(LightMin); \
  } else { \
    Color = gl_dbg_advlight_color; \
  } \
  (shad_).SetLightColor(((Color>>16)&255)/255.0f, ((Color>>8)&255)/255.0f, (Color&255)/255.0f); \
} while (0)


//==========================================================================
//
//  VOpenGLDrawer::BeginLightPass
//
//  setup rendering parameters for lighted surface rendering
//
//==========================================================================
void VOpenGLDrawer::BeginLightPass (const TVec &LightPos, float Radius, float LightMin, vuint32 Color, bool doShadow) {
  if (gl_dbg_wireframe) return;
  RestoreDepthFunc();
  glDepthMask(GL_FALSE); // no z-buffer writes
  glDisable(GL_TEXTURE_2D);

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  GLEnableBlend();
  //glBlendFunc(GL_SRC_COLOR, GL_DST_COLOR);
  //p_glBlendEquation(GL_MAX_EXT);

  glDepthFunc(GL_EQUAL);

  if (r_shadowmaps) {
    SETUP_LIGHT_SHADER(ShadowsLightSMap);
    glDisable(GL_STENCIL_TEST);
  } else {
    // do not use stencil test if we rendered no shadow surfaces
    if (doShadow && IsStencilBufferDirty()/*wasRenderedShadowSurface*/) {
      if (gl_dbg_use_zpass > 1) {
        glStencilFunc(GL_EQUAL, 0x1, 0xff);
      } else {
        glStencilFunc(GL_EQUAL, 0x0, 0xff);
      }
      glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
      glEnable(GL_STENCIL_TEST);
    } else {
      glDisable(GL_STENCIL_TEST);
    }

    /*
    if (doShadow && !wasRenderedShadowSurface) {
      Color = 0xffff0000u;
    }
    */

    if (spotLight) {
      if (!gl_dbg_advlight_debug) {
        SETUP_LIGHT_SHADER(ShadowsLightSpot);
        ShadowsLightSpot.SetConeDirection(coneDir);
        ShadowsLightSpot.SetConeAngle(coneAngle);
      } else {
        SETUP_LIGHT_SHADER(ShadowsLightSpotDbg);
        ShadowsLightSpotDbg.SetConeDirection(coneDir);
        ShadowsLightSpotDbg.SetConeAngle(coneAngle);
      }
    } else {
      if (!gl_dbg_advlight_debug) {
        SETUP_LIGHT_SHADER(ShadowsLight);
      } else {
        SETUP_LIGHT_SHADER(ShadowsLightDbg);
      }
    }
  }
}


//==========================================================================
//
//  VOpenGLDrawer::DrawSurfaceLight
//
//  this blends surfaces from light sources to ambient map.
//
//  `LightCanCross` means that light can span over this surface
//  light can span over two-sided midtex, for example, but not over
//  one-sided wall
//    <0: horizon
//    >0: two-sided wall
//    =0: one-sided wall
//
//  most checks are done in caller
//
//==========================================================================
void VOpenGLDrawer::DrawSurfaceLight (surface_t *surf) {
  if (gl_dbg_wireframe) return;
  if (!surf->IsPlVisible()) return; // viewer is in back side or on plane
  if (surf->count < 3) return;

  const texinfo_t *tex = surf->texinfo;
  SetTexture(tex->Tex, tex->ColorMap);

  if (r_shadowmaps) {
    ShadowsLightSMap.SetTex(tex);
    ShadowsLightSMap.SetSurfNormal(surf->GetNormal());
    ShadowsLightSMap.SetSurfDist(surf->GetDist());
  } else {
    if (spotLight) {
      if (!gl_dbg_advlight_debug) {
        ShadowsLightSpot.SetTex(tex);
        ShadowsLightSpot.SetSurfNormal(surf->GetNormal());
        ShadowsLightSpot.SetSurfDist(surf->GetDist());
      } else {
        ShadowsLightSpotDbg.SetTex(tex);
        ShadowsLightSpotDbg.SetSurfNormal(surf->GetNormal());
        ShadowsLightSpotDbg.SetSurfDist(surf->GetDist());
      }
    } else {
      if (!gl_dbg_advlight_debug) {
        ShadowsLight.SetTex(tex);
        ShadowsLight.SetSurfNormal(surf->GetNormal());
        ShadowsLight.SetSurfDist(surf->GetDist());
      } else {
        ShadowsLightDbg.SetTex(tex);
        ShadowsLightDbg.SetSurfNormal(surf->GetNormal());
        ShadowsLightDbg.SetSurfDist(surf->GetDist());
      }
    }
  }

  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
  //glBegin(GL_POLYGON);
  currentActiveShader->UploadChangedUniforms();
  glBegin(GL_TRIANGLE_FAN);
    for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i].vec());
  glEnd();
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);
}
