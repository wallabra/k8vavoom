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
void VOpenGLDrawer::BeginLightShadowMaps (const TVec &LightPos, const float Radius, const TVec &aconeDir, const float aconeAngle, int swidth, int sheight) {
  glDepthMask(GL_TRUE); // due to shadow volumes pass settings
  p_glBindFramebuffer(GL_FRAMEBUFFER, cubeFBO);

  glClearDepth(1.0f);
  if (p_glClipControl) p_glClipControl(GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE); // restore "normal" depth control
  glDepthRange(0.0f, 1.0f);
  glDepthFunc(GL_LESS);

  //glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // black background
  glClearColor(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);

  //glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  glDisable(GL_CULL_FACE);
  //glEnable(GL_CULL_FACE);
  //!glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

  SurfShadowMap.Activate();

  glGetIntegerv(GL_VIEWPORT, savedSMVPort);
  glViewport(0, 0, shadowmapSize, shadowmapSize);

  glDepthMask(GL_FALSE); // due to shadow volumes pass settings
  glDisable(GL_DEPTH_TEST);

  glDepthMask(GL_TRUE);
  glEnable(GL_DEPTH_TEST);
  //glClearDepth(1.0f);
  //glDepthFunc(GL_GREATER);

  glDisable(GL_TEXTURE_2D);
  GLDRW_RESET_ERROR();

  glEnable(GL_TEXTURE_2D);
  glEnable(GL_CULL_FACE);
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
  if (p_glClipControl) p_glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); // actually, this is better even for "normal" cases
  glClearDepth(!useReverseZ ? 1.0f : 0.0f);
  glViewport(savedSMVPort[0], savedSMVPort[1], savedSMVPort[2], savedSMVPort[3]);
  glEnable(GL_CULL_FACE);
  glDepthMask(GL_FALSE);
  glEnable(GL_DEPTH_TEST);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // black background
  glDisable(GL_TEXTURE_2D);
}


//==========================================================================
//
//  VOpenGLDrawer::SetupLightShadowMap
//
//==========================================================================
void VOpenGLDrawer::SetupLightShadowMap (const TVec &LightPos, const float Radius, const TVec &aconeDir, const float aconeAngle, unsigned int facenum, int swidth, int sheight) {
  p_glBindFramebuffer(GL_FRAMEBUFFER, cubeFBO);
  GLDRW_CHECK_ERROR("set cube FBO");

  //!p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X+facenum, cubeTexId, 0);
  //!GLDRW_CHECK_ERROR("set cube FBO face");

  p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, cubeDepthTexId, 0);
  GLDRW_CHECK_ERROR("set framebuffer depth texture");
  p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X+facenum, cubeTexId, 0);
  GLDRW_CHECK_ERROR("set cube FBO face");
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  GLDRW_CHECK_ERROR("set cube FBO draw buffer");
  glReadBuffer(GL_NONE);
  GLDRW_CHECK_ERROR("set cube FBO read buffer");

  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  GLDRW_CHECK_ERROR("clear cube FBO");

  glEnable(GL_DEPTH_TEST);
  //glDisable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  //glDepthFunc(GL_GREATER);

  SurfShadowMap.Activate();

  VMatrix4 newPrj;
  CalcShadowMapProjectionMatrix(newPrj, Radius, swidth, sheight, PixelAspect);

  const TAVec viewAngles[6] = {
    //    pitch    yaw   roll
    TAVec(  0.0f, -90.0f,   0.0f), // right
    TAVec(  0.0f,  90.0f,   0.0f), // left
    TAVec( 90.0f,   0.0f,   0.0f), // top
    TAVec(-90.0f,   0.0f,   0.0f), // bottom
    TAVec(  0.0f,   0.0f,   0.0f), // back
    TAVec(  0.0f, 180.0f,   0.0f), // front
  };
  VMatrix4 lview;
  CalcModelMatrix(lview, LightPos, viewAngles[facenum], false);
  VMatrix4 lmpv = newPrj*lview;
  SurfShadowMap.SetLightMPV(lmpv);

  VMatrix4 lview2;
  CalcModelMatrix(lview2, TVec(0, 0, 0), TAVec(0, 0, 0), false);
  SurfShadowMap.SetLightView(lview2);
  TVec lpp = lview2*LightPos;
  SurfShadowMap.SetLightPos(lpp);

  SurfShadowMap.SetLightRadius(Radius);
  SurfShadowMap.SetTexture(0);
  SurfShadowMap.UploadChangedUniforms();
  GLDRW_CHECK_ERROR("update cube FBO shader");

  coneDir = aconeDir;
  coneAngle = (aconeAngle <= 0.0f || aconeAngle >= 360.0f ? 0.0f : aconeAngle);

  if (coneAngle && aconeDir.isValid() && !aconeDir.isZero()) {
    spotLight = true;
    coneDir.normaliseInPlace();
  } else {
    spotLight = false;
  }
}


//==========================================================================
//
//  VOpenGLDrawer::RenderSurfaceShadowMap
//
//==========================================================================
void VOpenGLDrawer::RenderSurfaceShadowMap (const surface_t *surf, const TVec &LightPos, float Radius) {
  //if (gl_dbg_wireframe) return;
  if (surf->count < 3) return; // just in case

  const texinfo_t *tex = surf->texinfo;
  SetTexture(tex->Tex, tex->ColorMap);
  SurfShadowMap.SetTex(tex);

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
