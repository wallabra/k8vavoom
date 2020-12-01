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
//  matDump
//
//==========================================================================
static VVA_OKUNUSED void matDump (const VMatrix4 &mat) {
  GCon->Logf(NAME_Debug, "=======");
  for (int y = 0; y < 4; ++y) {
    VStr s;
    for (int x = 0; x < 4; ++x) {
      s += va(" %g", mat.m[y][x]);
    }
    GCon->Logf(NAME_Debug, "%s", *s);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::SetupLightShadowMap
//
//==========================================================================
void VOpenGLDrawer::SetupLightShadowMap (const TVec &LightPos, const float Radius, const TVec &aconeDir, const float aconeAngle, unsigned int facenum, int swidth, int sheight) {
  //GCon->Logf(NAME_Debug, "--- facenum=%u ---", facenum);
  const TVec viewsCenter[6] = {
    TVec( 1,  0,  0),
    TVec(-1,  0,  0),
    TVec( 0,  1,  0),
    TVec( 0, -1,  0),
    TVec( 0,  0,  1),
    TVec( 0,  0, -1),
  };
  const TVec viewsUp[6] = {
    TVec(0, -1,  0), // or -1
    TVec(0, -1,  0), // or -1
    TVec(0,  0, -1), // or 1
    TVec(0,  0,  1), // or -1
    TVec(0, -1,  0), // or -1
    TVec(0, -1,  0), // or -1
  };

  //glDisable(GL_SCISSOR_TEST);

    // right
    // left
    // top
    // bottom
    // back
    // front

  //VMatrix4 newPrj = VMatrix4::ProjectionZeroOne(90.0f, 1.0f, 1.0f, Radius);
  VMatrix4 newPrj = VMatrix4::ProjectionNegOne(90.0f, 1.0, 1.0f, Radius);
  //VMatrix4 newPrj = VMatrix4::Perspective(90.0f, 1.0f, 1.0f, Radius);
  //matDump(newPrj);
  //matDump(newPrj1);
  /*
  VMatrix4 lview = VMatrix4::TranslateNeg(LightPos);
  VMatrix4 aface = VMatrix4::LookAtGLM(TVec(0, 0, 0), viewsCenter[facenum], viewsUp[facenum]);
  VMatrix4 mvp = newPrj*aface*lview; // *mview
  */
  //VMatrix4 lview = VMatrix4::LookAtGLM(LightPos, viewsCenter[facenum], viewsUp[facenum]);
  //VMatrix4 lview = VMatrix4::LookAt(LightPos, viewsCenter[facenum], viewsUp[facenum]);
  //VMatrix4 lview = VMatrix4::LookAtGLM(TVec(0, 0, 0), viewsCenter[facenum], viewsUp[facenum]);

  //VMatrix4 mvp = newPrj*lview; //*mview

  VMatrix4 ProjMat;
  const float fov = 90.0f;
  const float fovx = tanf(DEG2RADF(fov)/2.0f);
  const float fovy = fovx;//*sheight/swidth/PixelAspect;

  /*
  clip_base.setupViewport(refdef->width, refdef->height, fov, PixelAspect);
  static inline void CalcFovXY (float *outfovx, float *outfovy, const int width, const int height, const float fov, const float pixelAspect=1.0f) noexcept {
    const float fovx = tanf(DEG2RADF(fov)/2.0f);
    if (outfovx) *outfovx = fovx;
    if (outfovy) *outfovy = fovx*height/width/pixelAspect;
  */

  ProjMat.SetZero();
  ProjMat[0][0] = 1.0f/fovx;
  ProjMat[1][1] = 1.0f/fovy;
  ProjMat[2][3] = -1.0f;
  //ProjMat[3][3] = 0.0f;
  float maxdist = Radius;
  if (maxdist < 1.0f || !isFiniteF(maxdist)) maxdist = 32767.0f;
  if (/*DepthZeroOne*/false) {
    ProjMat[2][2] = maxdist/(1.0f-maxdist); // zFar/(zNear-zFar);
    ProjMat[3][2] = -maxdist/(maxdist-1.0f); // -(zFar*zNear)/(zFar-zNear);
  } else {
    ProjMat[2][2] = -(maxdist+1.0f)/(maxdist-1.0f); // -(zFar+zNear)/(zFar-zNear);
    ProjMat[3][2] = -2.0f*maxdist/(maxdist-1.0f); // -(2.0f*zFar*zNear)/(zFar-zNear);
  }

  newPrj = ProjMat;

  /*
  ModelMat.SetIdentity();
  ModelMat *= VMatrix4::RotateX(-90); //glRotatef(-90, 1, 0, 0);
  ModelMat *= VMatrix4::RotateZ(90); //glRotatef(90, 0, 0, 1);
  if (MirrorFlip) ModelMat *= VMatrix4::Scale(TVec(1, -1, 1)); //glScalef(1, -1, 1);
  ModelMat *= VMatrix4::RotateX(-angles.roll); //glRotatef(-viewangles.roll, 1, 0, 0);
  ModelMat *= VMatrix4::RotateY(-angles.pitch); //glRotatef(-viewangles.pitch, 0, 1, 0);
  ModelMat *= VMatrix4::RotateZ(-angles.yaw); //glRotatef(-viewangles.yaw, 0, 0, 1);
  ModelMat *= VMatrix4::Translate(-origin); //glTranslatef(-vieworg.x, -vieworg.y, -vieworg.z);
  */

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
  Drawer->CalcModelMatrix(lview, LightPos, viewAngles[facenum], false);
  //SurfShadowMap.SetLightPos(LightPos);

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

#if 0
  VMatrix4 prjOrtho = VMatrix4::Ortho(0, shadowmapSize, 0, shadowmapSize, -666.0f, 666.0f);
  SurfShadowMapClear.Activate();
  SurfShadowMapClear.SetLightMPV(prjOrtho);
  SurfShadowMapClear.UploadChangedUniforms();

  glDepthMask(GL_TRUE);
  glDisable(GL_DEPTH_TEST);
  glBegin(GL_QUADS);
    glVertex2f(-shadowmapSize,  shadowmapSize);
    glVertex2f( shadowmapSize,  shadowmapSize);
    glVertex2f( shadowmapSize, -shadowmapSize);
    glVertex2f(-shadowmapSize, -shadowmapSize);
  glEnd();
#endif

  glEnable(GL_DEPTH_TEST);
  //glDisable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  //glDepthFunc(GL_GREATER);
  SurfShadowMap.Activate();
  //SurfShadowMap.SetLightMPV(mvp);
  //SurfShadowMap.SetLightProj(newPrj);
  //SurfShadowMap.SetLightView(lview);

  VMatrix4 lview2;
  Drawer->CalcModelMatrix(lview2, TVec(0, 0, 0), TAVec(0, 0, 0), false);
  SurfShadowMap.SetLightView(lview2);

  TVec lpp = lview2*LightPos;

  VMatrix4 lmpv = newPrj*lview;
  SurfShadowMap.SetLightMPV(lmpv);
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
  if (gl_dbg_wireframe) return;
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

  //GCon->Logf(NAME_Debug, "  sfc: 0x%08x", (unsigned)(uintptr_t)surf);

  #if 1
  //glBegin(GL_POLYGON);
  glBegin(GL_TRIANGLE_FAN);
    for (unsigned i = 0; i < vcount; ++i, ++v) glVertex(v->vec());
  glEnd();
  #else
  const float size = 68.0f;
  // top
  glBegin(GL_QUADS);
    glVertex3f(-size, -size, -size);
    glVertex3f( size, -size, -size);
    glVertex3f( size,  size, -size);
    glVertex3f(-size,  size, -size);
  glEnd();
  // bottom
  glBegin(GL_QUADS);
    glVertex3f(-size, -size, size);
    glVertex3f( size, -size, size);
    glVertex3f( size,  size, size);
    glVertex3f(-size,  size, size);
  glEnd();
  // left
  glBegin(GL_QUADS);
    glVertex3f(-size, -size, -size);
    glVertex3f(-size,  size, -size);
    glVertex3f(-size,  size,  size);
    glVertex3f(-size, -size,  size);
  glEnd();
  // right
  glBegin(GL_QUADS);
    glVertex3f(size, -size, -size);
    glVertex3f(size,  size, -size);
    glVertex3f(size,  size,  size);
    glVertex3f(size, -size,  size);
  glEnd();
  // rear
  glBegin(GL_QUADS);
    glVertex3f(-size, -size, -size);
    glVertex3f( size, -size, -size);
    glVertex3f( size, -size,  size);
    glVertex3f(-size, -size,  size);
  glEnd();
  // front
  glBegin(GL_QUADS);
    glVertex3f(-size, size, -size);
    glVertex3f( size, size, -size);
    glVertex3f( size, size,  size);
    glVertex3f(-size, size,  size);
  glEnd();
  #endif
}
