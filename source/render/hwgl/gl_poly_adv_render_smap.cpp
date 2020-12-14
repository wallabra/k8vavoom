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

//#define VV_SMAP_STRICT_ERROR_CHECKS


#ifdef VV_SMAP_STRICT_ERROR_CHECKS
# define GLSMAP_CLEAR_ERR  GLDRW_RESET_ERROR
# define GLSMAP_ERR        GLDRW_CHECK_ERROR
#else
# define GLSMAP_CLEAR_ERR(...)
# define GLSMAP_ERR(...)
#endif


extern VCvarI gl_shadowmap_size;
extern VCvarB gl_shadowmap_precision;
extern VCvarI gl_shadowmap_ray_points;

// saved shadowmap parameters
static int saved_gl_shadowmap_size = -666;
static bool saved_gl_shadowmap_precision = false;
static int saved_gl_shadowmap_ray_points = -666;


//==========================================================================
//
//  VOpenGLDrawer::PrepareShadowMapsInternal
//
//==========================================================================
void VOpenGLDrawer::PrepareShadowMapsInternal (const float Radius) {
  if (!IsAnyShadowMapDirty()) return;
  GLSMAP_CLEAR_ERR();
  glClearColor(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);
  for (unsigned int fc = 0; fc < 6; ++fc) {
    if (IsShadowMapDirty(fc)) {
      //p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowCube[smapCurrent].cubeDepthTexId[fc], 0);
      p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, shadowCube[smapCurrent].cubeDepthRBId[fc]);
      GLSMAP_ERR("set framebuffer depth texture");
      p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X+fc, shadowCube[smapCurrent].cubeTexId, 0);
      GLSMAP_ERR("set cube FBO face");
      glDrawBuffer(GL_COLOR_ATTACHMENT0);
      GLSMAP_ERR("set cube FBO draw buffer");
      glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
      GLSMAP_ERR("clear cube FBO");
    }
  }
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // black background
  MarkAllShadowMapsClear();
}


//==========================================================================
//
//  VOpenGLDrawer::PrepareShadowMaps
//
//==========================================================================
void VOpenGLDrawer::PrepareShadowMaps (const float Radius) {
  if (!r_shadowmaps.asBool() || !CanRenderShadowMaps()) return;
  for (unsigned int index = 0; index < 2; ++index) {
    smapCurrent = index;
    if (!shadowCube[smapCurrent].cubeFBO || !IsAnyShadowMapDirty()) continue;
    GLSMAP_CLEAR_ERR();
    p_glBindFramebuffer(GL_FRAMEBUFFER, shadowCube[index].cubeFBO);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    PushDepthMask();
    glEnableDepthWrite();
    glClearDepth(1.0f);
    if (p_glClipControl) p_glClipControl(GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE); // restore "normal" depth control
    glDepthRange(0.0f, 1.0f);
    //!if (gl_shadowmap_gbuffer) glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE); else
    glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // black background
    PrepareShadowMapsInternal(Radius);
    ReactivateCurrentFBO();
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    RestoreDepthFunc();
    if (p_glClipControl) p_glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); // actually, this is better even for "normal" cases
    glClearDepth(!useReverseZ ? 1.0f : 0.0f);
    PopDepthMask();
  }
}


//==========================================================================
//
//  VOpenGLDrawer::ClearAllShadowMaps
//
//==========================================================================
void VOpenGLDrawer::ClearAllShadowMaps () {
  //FIXME: this should be changed, but we don't have any cascades yet anyway
  PrepareShadowMaps(999999.0f); // use HUGE radius to clear all cascades
}


//==========================================================================
//
//  VOpenGLDrawer::PrepareShadowCube
//
//  activate cube (create it if necessary); calculate matrices
//  won't clear faces
//
//==========================================================================
void VOpenGLDrawer::PrepareShadowCube (const TVec &LightPos, const float Radius, unsigned int index) noexcept {
  if (index > CUBE32) index = CUBE32;
  EnsureShadowMapCube();
  if (index > 0 && !shadowCube[index].cubeFBO) index = 0;
  smapCurrent = index;
  CalcShadowMapProjectionMatrix(shadowCube[index].proj, Radius/*, swidth, sheight, PixelAspect*/);
  for (unsigned int facenum = 0; facenum < 6; ++facenum) {
    VMatrix4 lview;
    CalcSpotLightFaceView(lview, LightPos, facenum);
    shadowCube[index].lmpv[facenum] = shadowCube[index].proj*lview;
  }
}


//==========================================================================
//
//  VOpenGLDrawer::ActivateShadowMapFace
//
//  also sets is as current
//
//==========================================================================
void VOpenGLDrawer::ActivateShadowMapFace (unsigned int facenum) noexcept {
  vassert(facenum >= 0 && facenum <= 5);

  //p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowCube[smapCurrent].cubeDepthTexId[facenum], 0);
  p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, shadowCube[smapCurrent].cubeDepthRBId[facenum]);
  GLSMAP_ERR("set framebuffer depth texture");

  //!p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X+smapCurrentFace, shadowCube[smapCurrent].cubeTexId, 0);
  //!GLSMAP_ERR("set cube FBO face");

  p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X+facenum, shadowCube[smapCurrent].cubeTexId, 0);
  GLSMAP_ERR("set cube FBO face");
  //glDrawBuffer(GL_COLOR_ATTACHMENT0);
  //GLSMAP_ERR("set cube FBO draw buffer");

  SetCurrentShadowMapFace(facenum);
}


//==========================================================================
//
//  VOpenGLDrawer::BeginLightShadowVolumes
//
//==========================================================================
void VOpenGLDrawer::BeginLightShadowMaps (const TVec &LightPos, const float Radius, const TVec &aconeDir, const float aconeAngle) {
  if (gl_shadowmap_size.asInt() != saved_gl_shadowmap_size ||
      gl_shadowmap_precision.asBool() != saved_gl_shadowmap_precision ||
      gl_shadowmap_ray_points.asInt() != saved_gl_shadowmap_ray_points)
  {
    DestroyShadowCube();
    saved_gl_shadowmap_size = gl_shadowmap_size.asInt();
    saved_gl_shadowmap_precision = gl_shadowmap_precision.asBool();
    saved_gl_shadowmap_ray_points = gl_shadowmap_ray_points.asInt();
  }

  PrepareShadowCube(LightPos, Radius, (Radius < 1000.0f ? CUBE16 : CUBE32));

  GLSMAP_CLEAR_ERR();
  const bool flt = gl_dev_shadowmap_filter.asBool();
  if (flt != cubemapLinearFiltering) {
    cubemapLinearFiltering = flt;
    glBindTexture(GL_TEXTURE_CUBE_MAP, shadowCube[smapCurrent].cubeTexId);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, (cubemapLinearFiltering ? GL_LINEAR : GL_NEAREST));
    GLSMAP_ERR("set shadowmap mag filter");
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, (cubemapLinearFiltering ? GL_LINEAR : GL_NEAREST));
    GLSMAP_ERR("set shadowmap min filter");
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  }

  GLSMAP_CLEAR_ERR();
  p_glBindFramebuffer(GL_FRAMEBUFFER, shadowCube[smapCurrent].cubeFBO);
  GLSMAP_ERR("set cube FBO");
  /*
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  GLSMAP_ERR("set cube FBO draw buffer");
  glReadBuffer(GL_NONE);
  GLSMAP_ERR("set cube FBO read buffer");
  */

  ScrWdt = shadowmapSize;
  ScrHgt = shadowmapSize;

  smapLastTexinfo.initLastUsed();
  smapLastSprTexinfo.initLastUsed();

  // temp (it should be already disabled)
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_SCISSOR_TEST);
  GLDisableOffset();
  GLDisableBlend();

  //glDepthMask(GL_TRUE); // due to shadow volumes pass settings
  glEnableDepthWrite();
  glEnable(GL_DEPTH_TEST);
  glClearDepth(1.0f);
  if (p_glClipControl) p_glClipControl(GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE); // restore "normal" depth control
  glDepthRange(0.0f, 1.0f);
  glDepthFunc(GL_LESS);

  //glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // black background
  glClearColor(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);

  //glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  //glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
  //glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
  //!if (gl_shadowmap_gbuffer) glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE); else
  glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);

  glEnable(GL_CULL_FACE);
  glEnable(GL_TEXTURE_2D);

  //glGetIntegerv(GL_VIEWPORT, savedSMVPort);
  GLGetViewport(savedSMVPort);
  GLSetViewport(0, 0, shadowmapSize, shadowmapSize);

  PrepareShadowMapsInternal(Radius);

  setupSpotLight(LightPos, Radius, aconeDir, aconeAngle);

  //SurfShadowMap.Activate();
  SurfShadowMap.SetLightPos(LightPos);
  SurfShadowMap.SetLightRadius(Radius);

  //SurfShadowMapTex.Activate();
  SurfShadowMapTex.SetLightPos(LightPos);
  SurfShadowMapTex.SetLightRadius(Radius);
  SurfShadowMapTex.SetTexture(0);

  //SurfShadowMapSpr.Activate();
  SurfShadowMapSpr.SetLightPos(LightPos);
  SurfShadowMapSpr.SetLightRadius(Radius);
  SurfShadowMapSpr.SetTexture(0);

  //glDisable(GL_CULL_FACE);
  GLSMAP_ERR("finish cube FBO setup");
}


//==========================================================================
//
//  VOpenGLDrawer::EndLightShadowMaps
//
//==========================================================================
void VOpenGLDrawer::EndLightShadowMaps () {
  p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
  GLSMAP_ERR("reset cube FBO");
  ReactivateCurrentFBO();
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  RestoreDepthFunc();
  if (p_glClipControl) p_glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); // actually, this is better even for "normal" cases
  glClearDepth(!useReverseZ ? 1.0f : 0.0f);
  GLSetViewport(savedSMVPort[0], savedSMVPort[1], savedSMVPort[2], savedSMVPort[3]);
  glEnable(GL_CULL_FACE);
  //glDepthMask(GL_FALSE);
  glDisableDepthWrite();
  glEnable(GL_DEPTH_TEST);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // black background
  glDisable(GL_TEXTURE_2D);
}


//==========================================================================
//
//  VOpenGLDrawer::SetupLightShadowMap
//
//==========================================================================
void VOpenGLDrawer::SetupLightShadowMap (unsigned int facenum) {
  ActivateShadowMapFace(facenum);
  vassert(shadowCube[smapCurrent].smapCurrentFace == facenum);

  if (IsCurrentShadowMapDirty()) {
    //glClearColor(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    GLSMAP_ERR("clear cube FBO");
    //glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // black background
    MarkCurrentShadowMapClean();
  }

  SurfShadowMap.SetLightMPV(shadowCube[smapCurrent].lmpv[facenum]);
  SurfShadowMapTex.SetLightMPV(shadowCube[smapCurrent].lmpv[facenum]);
  SurfShadowMapSpr.SetLightMPV(shadowCube[smapCurrent].lmpv[facenum]);

  // required for proper sprite shadow rendering
  smapLastSprTexinfo.resetLastUsed();
}


//==========================================================================
//
//  VOpenGLDrawer::RenderSurfaceShadowMap
//
//==========================================================================
void VOpenGLDrawer::RenderSurfaceShadowMap (const surface_t *surf) {
  if (gl_dbg_wireframe) return;
  if (surf->count < 3) return; // just in case

  if (spotLight && !isSurfaceInSpotlight(surf)) return;

  const unsigned vcount = (unsigned)surf->count;
  const SurfVertex *sverts = surf->verts;
  const SurfVertex *v = sverts;

  const texinfo_t *currTexinfo = surf->texinfo;
  if (currTexinfo->Tex->isTransparent()) {
    SurfShadowMapTex.Activate();
    const bool textureChanged = smapLastTexinfo.needChange(*currTexinfo, updateFrame);
    if (textureChanged) {
      smapLastTexinfo.updateLastUsed(*currTexinfo);
      //SetTexture(currTexinfo->Tex, currTexinfo->ColorMap);
      SetShadowTexture(currTexinfo->Tex);
      SurfShadowMapTex.SetTex(currTexinfo);
    }
  } else {
    SurfShadowMap.Activate();
  }

  //if (gl_smart_reject_shadows && !AdvRenderCanSurfaceCastShadow(surf, LightPos, Radius)) return;

  currentActiveShader->UploadChangedUniforms();

  //glBegin(GL_POLYGON);
  glBegin(GL_TRIANGLE_FAN);
    for (unsigned i = 0; i < vcount; ++i, ++v) glVertex(v->vec());
  glEnd();

  MarkCurrentShadowMapDirty();
}


//==========================================================================
//
//  VOpenGLDrawer::DrawSpriteShadowMap
//
//==========================================================================
void VOpenGLDrawer::DrawSpriteShadowMap (const TVec *cv, VTexture *Tex, const TVec &sprnormal,
                                         const TVec &saxis, const TVec &taxis, const TVec &texorg)
{
  if (gl_dbg_wireframe) return;
  if (!Tex || Tex->Type == TEXTYPE_Null) return; // just in case

  if (spotLight && !isSpriteInSpotlight(cv)) return;

  if (Tex->isTransparent()) {
    // create fake texinfo
    texinfo_t currTexinfo;
    currTexinfo.saxis = saxis;
    currTexinfo.soffs = 0;
    currTexinfo.taxis = taxis;
    currTexinfo.toffs = 0;
    currTexinfo.saxisLM = currTexinfo.taxisLM = TVec(0, 0, 0);
    currTexinfo.Tex = Tex;
    currTexinfo.noDecals = 0;
    currTexinfo.Alpha = 1.1f;
    currTexinfo.Additive = 0;
    currTexinfo.ColorMap = 0;

    SurfShadowMapSpr.Activate();
    // activate shader, check for texture change
    const bool textureChanged = smapLastSprTexinfo.needChange(currTexinfo, updateFrame);
    if (true || textureChanged) {
      smapLastSprTexinfo.updateLastUsed(currTexinfo);
      // required for proper wall shadow rendering
      smapLastTexinfo.resetLastUsed();
      SetShadowTexture(Tex); //FIXME: this should be "no-repeat"
    }
    SurfShadowMapSpr.SetSpriteTex(texorg, saxis, taxis, tex_iw, tex_ih);
  } else {
    SurfShadowMap.Activate();
  }

  currentActiveShader->UploadChangedUniforms();

  glBegin(GL_TRIANGLE_FAN);
    glVertex(cv[0]);
    glVertex(cv[1]);
    glVertex(cv[2]);
    glVertex(cv[3]);
  glEnd();

  MarkCurrentShadowMapDirty();
}
