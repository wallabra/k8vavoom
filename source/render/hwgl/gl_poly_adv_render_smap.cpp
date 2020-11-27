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
//  VOpenGLDrawer::BeginLightShadowVolumes
//
//==========================================================================
void VOpenGLDrawer::BeginLightShadowMaps (const TVec &LightPos, const float Radius, const TVec &aconeDir, const float aconeAngle) {
  glDepthMask(GL_TRUE); // due to shadow volumes pass settings
  p_glBindFramebuffer(GL_FRAMEBUFFER, cubeFBO);

  glClearDepth(0.0f);
  //if (p_glClipControl) p_glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); // actually, this is better even for "normal" cases
  glDepthRange(0.0f, 1.0f);
  glClear(GL_DEPTH_BUFFER_BIT);
  glDepthFunc(GL_LESS);

  glClearDepth(!useReverseZ ? 1.0f : 0.0f);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

  glDisable(GL_CULL_FACE);

  SurfShadowMap.Activate();
}


//==========================================================================
//
//  VOpenGLDrawer::EndLightShadowMaps
//
//==========================================================================
void VOpenGLDrawer::EndLightShadowMaps () {
  currentActiveFBO = nullptr;
  mainFBO.activate();
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  RestoreDepthFunc();
}


//==========================================================================
//
//  VOpenGLDrawer::SetupLightShadowMap
//
//==========================================================================
void VOpenGLDrawer::SetupLightShadowMap (const TVec &LightPos, const float Radius, const TVec &aconeDir, const float aconeAngle, unsigned int facenum) {
  const TVec viewsCenter[6] = {
    TVec( 1,  0,  0),
    TVec(-1,  0,  0),
    TVec( 0,  1,  0),
    TVec( 0, -1,  0),
    TVec( 0,  0,  1),
    TVec( 0,  0, -1),
  };
  const TVec viewsUp[6] = {
    TVec(0, 1,  0), // or -1
    TVec(0, 1,  0), // or -1
    TVec(0, 0, -1), // or 1
    TVec(0, 0,  1), // or -1
    TVec(0, 1,  0), // or -1
    TVec(0, 1,  0), // or -1
  };
  VMatrix4 newPrj = VMatrix4::ProjectionZeroOne(90.0f, 1.0f, 2.0f, Radius);
  VMatrix4 lview = VMatrix4::TranslateNeg(LightPos);
  VMatrix4 aface = VMatrix4::LookAtGLM(TVec(0, 0, 0), viewsCenter[facenum], viewsUp[facenum]);
  VMatrix4 mvp = newPrj*aface*lview;
  //uniforms->get_uniform("mvp") = light_projection * active_face * light_view * state.model;
  //program->merge_uniforms(*uniforms);
  //SurfShadowMap.SetLightPos(LightPos);
  SurfShadowMap.SetLightMPV(mvp);
  SurfShadowMap.UploadChangedUniforms();
}


//==========================================================================
//
//  VOpenGLDrawer::RenderSurfaceShadowMap
//
//==========================================================================
void VOpenGLDrawer::RenderSurfaceShadowMap (const surface_t *surf, const TVec &LightPos, float Radius) {
  if (gl_dbg_wireframe) return;
  if (surf->count < 3) return; // just in case

  //if (gl_smart_reject_shadows && !AdvRenderCanSurfaceCastShadow(surf, LightPos, Radius)) return;

  const unsigned vcount = (unsigned)surf->count;
  const SurfVertex *sverts = surf->verts;
  const SurfVertex *v = sverts;

  currentActiveShader->UploadChangedUniforms();
  //currentActiveShader->UploadChangedAttrs();

  //glBegin(GL_POLYGON);
  glBegin(GL_TRIANGLE_FAN);
    for (unsigned i = 0; i < vcount; ++i, ++v) glVertex(v->vec());
  glEnd();
}
