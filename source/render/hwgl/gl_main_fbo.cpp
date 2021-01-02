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
//**
//**  OpenGL driver, main module
//**
//**************************************************************************
#include <limits.h>
#include <float.h>
#include <stdarg.h>

#include "gl_local.h"
#include "../r_local.h" /* for VRenderLevelShared */


static ColorCV letterboxColor(&gl_letterbox_color);
static VCvarB gl_enable_fp_zbuffer("gl_enable_fp_zbuffer", false, "Enable using of floating-point depth buffer for OpenGL3+?", CVAR_Archive|CVAR_PreInit);



//**************************************************************************
//
//  VOpenGLDrawer::FBO
//
//**************************************************************************

//==========================================================================
//
//  VOpenGLDrawer::FBO::FBO
//
//==========================================================================
VOpenGLDrawer::FBO::FBO ()
  : mOwner(nullptr)
  , mFBO(0)
  , mColorTid(0)
  , mDepthStencilRBO(0)
  , mWidth(0)
  , mHeight(0)
  , mLinearFilter(false)
  , scrScaled(false)
{
}


//==========================================================================
//
//  VOpenGLDrawer::FBO::~FBO
//
//==========================================================================
VOpenGLDrawer::FBO::~FBO () {
  destroy();
}


//==========================================================================
//
//  VOpenGLDrawer::FBO::destroy
//
//==========================================================================
void VOpenGLDrawer::FBO::destroy () {
  if (!mOwner) return;
  //GCon->Logf(NAME_Debug, "*** destroying FBO with id #%u (mColorTid=%u; mDepthStencilRBO=%u)", mFBO, mColorTid, mDepthStencilRBO);
  // detach everything from FBO, and destroy it
  mOwner->p_glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
  mOwner->p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
  glDeleteTextures(1, &mColorTid);
  if (mDepthStencilRBO) {
    mOwner->p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
    mOwner->p_glDeleteRenderbuffers(1, &mDepthStencilRBO);
  }
  mOwner->p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
  mOwner->p_glDeleteFramebuffers(1, &mFBO);
  // clear object
  mFBO = 0;
  mColorTid = 0;
  mDepthStencilRBO = 0;
  mWidth = 0;
  mHeight = 0;
  mLinearFilter = false;
  if (mOwner->currentActiveFBO == this) mOwner->currentActiveFBO = nullptr;
  mOwner->ReactivateCurrentFBO();
  mOwner = nullptr;
}


//==========================================================================
//
//  VOpenGLDrawer::FBO::createInternal
//
//==========================================================================
void VOpenGLDrawer::FBO::createInternal (VOpenGLDrawer *aowner, int awidth, int aheight, bool createDepthStencil, bool mirroredRepeat) {
  destroy();
  vassert(aowner);
  vassert(awidth > 0);
  vassert(aheight > 0);
  vassert(aowner->currentActiveFBO != this);

  GLint oldbindtex = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldbindtex);
  glBindTexture(GL_TEXTURE_2D, 0);

  // allocate FBO object
  GLDRW_RESET_ERROR();
  aowner->p_glGenFramebuffers(1, &mFBO);
  if (mFBO == 0) Sys_Error("OpenGL: cannot create FBO: error is %s", VGetGLErrorStr(glGetError()));
  aowner->p_glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
  aowner->p_glObjectLabelVA(GL_FRAMEBUFFER, mFBO, "FBO(%u)", mFBO);
  GLDRW_CHECK_ERROR("FBO: glBindFramebuffer");

  // attach 2D texture to this FBO
  glGenTextures(1, &mColorTid);
  if (mColorTid == 0) Sys_Error("OpenGL: cannot create RGBA texture for FBO: error is %s", VGetGLErrorStr(glGetError()));
  glBindTexture(GL_TEXTURE_2D, mColorTid);
  GLDRW_CHECK_ERROR("FBO: glBindTexture");
  aowner->p_glObjectLabelVA(GL_TEXTURE, mColorTid, "FBO(%u) color texture", mFBO);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

  if (mirroredRepeat) {
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
  } else {
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, aowner->ClampToEdge);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, aowner->ClampToEdge);
  }

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (mLinearFilter ? GL_LINEAR : GL_NEAREST));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (mLinearFilter ? GL_LINEAR : GL_NEAREST));
  if (aowner->anisotropyExists) glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), 1.0f); // 1 is minimum, i.e. "off"

  // empty texture
  GLDRW_RESET_ERROR();
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, awidth, aheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  GLDRW_CHECK_ERROR("FBO: glTexImage2D");
  aowner->p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColorTid, 0);
  GLDRW_CHECK_ERROR("FBO: glFramebufferTexture2D");

  // attach stencil texture to this FBO
  if (createDepthStencil) {
    vassert(GL_DEPTH_STENCIL_EXT == GL_DEPTH_STENCIL);
    vassert(GL_DEPTH_STENCIL_EXT == 0x84F9);
    vassert(GL_UNSIGNED_INT_24_8 == 0x84FA);
    vassert(GL_DEPTH24_STENCIL8 == 0x88F0);

    if (!aowner->CheckExtension("GL_EXT_packed_depth_stencil")) Sys_Error("OpenGL error: GL_EXT_packed_depth_stencil is not supported!");

    GLint depthStencilFormat = GL_DEPTH24_STENCIL8;

    // there is (almost) no reason to use fp depth buffer without reverse z
    // also, reverse z is perfectly working with int24 depth buffer, see http://www.reedbeta.com/blog/depth-precision-visualized/
    if (gl_enable_fp_zbuffer) {
      GLint major, minor;
      glGetIntegerv(GL_MAJOR_VERSION, &major);
      glGetIntegerv(GL_MINOR_VERSION, &minor);
      if (major >= 3) {
        depthStencilFormat = GL_DEPTH32F_STENCIL8;
        GCon->Log(NAME_Init, "OpenGL: using floating-point depth buffer");
      }
    }

    //GCon->Log(NAME_Init, "OpenGL: using combined depth/stencil renderbuffer for FBO");

    // create a render buffer object for the depth/stencil buffer
    aowner->p_glGenRenderbuffers(1, &mDepthStencilRBO);
    if (mDepthStencilRBO == 0) Sys_Error("OpenGL: cannot create depth/stencil render buffer for FBO: error is %s", VGetGLErrorStr(glGetError()));

    // bind the texture
    GLDRW_RESET_ERROR();
    aowner->p_glBindRenderbuffer(GL_RENDERBUFFER, mDepthStencilRBO);
    GLDRW_CHECK_ERROR("FBO: glBindRenderbuffer (0)");

    // create the render buffer in the GPU
    aowner->p_glRenderbufferStorage(GL_RENDERBUFFER, depthStencilFormat, awidth, aheight);
    GLDRW_CHECK_ERROR("create depth/stencil renderbuffer storage");

    #ifndef GL4ES_HACKS
    // unbind the render buffer
    aowner->p_glBindRenderbuffer(GL_RENDERBUFFER, 0);
    GLDRW_CHECK_ERROR("FBO: glBindRenderbuffer (1)");
    #endif

    // bind it to FBO
    aowner->p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mDepthStencilRBO);
    GLDRW_CHECK_ERROR("bind depth/stencil renderbuffer storage");
  }

  {
    GLenum status = aowner->p_glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) Sys_Error("OpenGL: framebuffer creation failed (status=0x%04x)", (unsigned)status);
  }

  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glReadBuffer(GL_COLOR_ATTACHMENT0);

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // black background
  glClearStencil(0);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

  mWidth = awidth;
  mHeight = aheight;
  mOwner = aowner;

  glBindTexture(GL_TEXTURE_2D, oldbindtex);

  //GCon->Logf(NAME_Debug, "*** created FBO with id #%u (ds=%d; mColorTid=%u; mDepthStencilRBO=%u)", mFBO, (int)createDepthStencil, mColorTid, mDepthStencilRBO);

  mOwner->ReactivateCurrentFBO();
  GLDRW_RESET_ERROR();
}


//==========================================================================
//
//  VOpenGLDrawer::FBO::createTextureOnly
//
//==========================================================================
void VOpenGLDrawer::FBO::createTextureOnly (VOpenGLDrawer *aowner, int awidth, int aheight, bool mirroredRepeat) {
  createInternal(aowner, awidth, aheight, false, mirroredRepeat);
}


//==========================================================================
//
//  VOpenGLDrawer::FBO::createDepthStencil
//
//==========================================================================
void VOpenGLDrawer::FBO::createDepthStencil (VOpenGLDrawer *aowner, int awidth, int aheight, bool mirroredRepeat) {
  createInternal(aowner, awidth, aheight, true, mirroredRepeat);
}


//==========================================================================
//
//  VOpenGLDrawer::FBO::activate
//
//==========================================================================
void VOpenGLDrawer::FBO::activate () {
  if (mOwner && mOwner->currentActiveFBO != this) {
    mOwner->currentActiveFBO = this;
    mOwner->ReactivateCurrentFBO();
  }
}


//==========================================================================
//
//  VOpenGLDrawer::FBO::deactivate
//
//==========================================================================
void VOpenGLDrawer::FBO::deactivate () {
  if (mOwner && mOwner->currentActiveFBO != nullptr) {
    mOwner->currentActiveFBO = nullptr;
    mOwner->ReactivateCurrentFBO();
  }
}


//==========================================================================
//
//  VOpenGLDrawer::FBO::blitTo
//
//  this blits only color info
//
//==========================================================================
void VOpenGLDrawer::FBO::blitTo (FBO *dest, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                                 GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLenum filter)
{
  if (!mOwner || !dest || !dest->mOwner) return;

  if (mOwner->p_glBlitFramebuffer && !gl_dbg_fbo_blit_with_texture) {
    mOwner->p_glBindFramebuffer(GL_READ_FRAMEBUFFER, mFBO);
    mOwner->p_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dest->mFBO);
    mOwner->p_glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, GL_COLOR_BUFFER_BIT, filter);
    mOwner->p_glBindFramebuffer(GL_READ_FRAMEBUFFER, mFBO);
    mOwner->p_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mFBO);
  } else {
    GLint oldbindtex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldbindtex);
    glPushAttrib(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_ENABLE_BIT|GL_VIEWPORT_BIT|GL_TRANSFORM_BIT);
    const bool oldBlend = mOwner->blendEnabled;
    const GLint oldCurrDepthMaskState = mOwner->currDepthMaskState;

    mOwner->p_glBindFramebuffer(GL_FRAMEBUFFER, dest->mFBO);
    glBindTexture(GL_TEXTURE_2D, mColorTid);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    mOwner->GLDisableBlend();
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_TEXTURE_2D);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    mOwner->p_glUseProgramObjectARB(0);
    mOwner->currentActiveShader = nullptr;

    const float mywf = (float)mWidth;
    const float myhf = (float)mHeight;

    const float mytx0 = (float)srcX0/mywf;
    const float myty0 = (float)srcY0/myhf;
    const float mytx1 = (float)srcX1/mywf;
    const float myty1 = (float)srcY1/myhf;

    mOwner->SetOrthoProjection(0, dest->mWidth, dest->mHeight, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, /*GL_LINEAR*/filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, /*GL_LINEAR*/filter);
    glBegin(GL_QUADS);
      glTexCoord2f(mytx0, myty1); glVertex2i(dstX0, dstY0);
      glTexCoord2f(mytx1, myty1); glVertex2i(dstX1, dstY0);
      glTexCoord2f(mytx1, myty0); glVertex2i(dstX1, dstY1);
      glTexCoord2f(mytx0, myty0); glVertex2i(dstX0, dstY1);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glPopAttrib();
    glBindTexture(GL_TEXTURE_2D, oldbindtex);

    mOwner->blendEnabled = oldBlend;
    mOwner->currDepthMaskState = oldCurrDepthMaskState;
  }
  mOwner->ReactivateCurrentFBO();
}


//==========================================================================
//
//  VOpenGLDrawer::FBO::blitToScreen
//
//==========================================================================
void VOpenGLDrawer::FBO::blitToScreen () {
  if (!mOwner) return;

  mOwner->p_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // screen FBO

  // do letterboxing if necessary
  int realw, realh;
  mOwner->GetRealWindowSize(&realw, &realh);
  int scaledWidth = realw, scaledHeight = realh;
  int blitOfsX = 0, blitOfsY = 0;

  const int calcWidth = mWidth;
  const int calcHeight = mHeight;
  //GCon->Logf(NAME_Debug, "VOpenGLDrawer::FBO::blitToScreen: scrScaled=%d; size=(%d,%d); calcSize=(%d,%d); realSize=(%d,%d)", (int)scrScaled, mWidth, mHeight, calcWidth, calcHeight, realw, realh);

  if ((gl_letterbox || scrScaled) && (realw != calcWidth || realh != calcHeight)) {
    //const float aspect = R_GetAspectRatio();
    const float llscale = clampval(gl_letterbox_scale.asFloat(), 0.0f, 1.0f);
    const float aspect = 1.0f;
    const float scaleX = float(realw)/float(calcWidth);
    const float scaleY = float(realh*aspect)/float(calcHeight);
    const float scale = (scaleX <= scaleY ? scaleX : scaleY)*(llscale ? llscale : 1.0f);
    scaledWidth = int(calcWidth*scale);
    scaledHeight = int(calcHeight/aspect*scale);
    blitOfsX = (realw-scaledWidth)/2;
    blitOfsY = (realh-scaledHeight)/2;
    //GCon->Logf(NAME_Debug, "letterbox: size=(%d,%d); real=(%d,%d); scaled=(%d,%d); offset=(%d,%d); scale=(%g,%g)", calcWidth, calcHeight, realw, realh, scaledWidth, scaledHeight, blitOfsX, blitOfsY, scaleX, scaleY);
    // clear stripes
    //glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // black background
    //glClear(GL_COLOR_BUFFER_BIT);
    glPushAttrib(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_ENABLE_BIT|GL_VIEWPORT_BIT|GL_TRANSFORM_BIT);
    const bool oldBlend = mOwner->blendEnabled;
    const GLint oldCurrDepthMaskState = mOwner->currDepthMaskState;
    mOwner->GLSetViewport(0, 0, realw, realh);
    glBindTexture(GL_TEXTURE_2D, 0);
    //glMatrixMode(GL_PROJECTION);
    //glLoadIdentity();
    mOwner->SetOrthoProjection(0, realw, realh, 0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    mOwner->GLDisableBlend();
    mOwner->DrawFixedCol.Activate();
    mOwner->DrawFixedCol.SetColor(letterboxColor.getFloatR(), letterboxColor.getFloatG(), letterboxColor.getFloatB(), 1);
    mOwner->DrawFixedCol.UploadChangedUniforms();
    glBegin(GL_QUADS);
    if (blitOfsX > 0) {
      // left
      glVertex2f(0, 0);
      glVertex2f(0, realh);
      glVertex2f(blitOfsX, realh);
      glVertex2f(blitOfsX, 0);
      // right
      int rx = realw-blitOfsX;
      glVertex2f(rx, 0);
      glVertex2f(rx, realh);
      glVertex2f(realw, realh);
      glVertex2f(realw, 0);
    }
    if (blitOfsY > 0) {
      int rx = realw-blitOfsX;
      // top
      glVertex2f(blitOfsX, 0);
      glVertex2f(blitOfsX, blitOfsY);
      glVertex2f(rx, blitOfsY);
      glVertex2f(rx, 0);
      // bottom
      int ry = realh-blitOfsY;
      glVertex2f(blitOfsX, ry);
      glVertex2f(blitOfsX, realh);
      glVertex2f(rx, realh);
      glVertex2f(rx, ry);
    }
    glEnd();
    glPopAttrib();
    mOwner->blendEnabled = oldBlend;
    mOwner->currDepthMaskState = oldCurrDepthMaskState;
  }

  if (mOwner->p_glBlitFramebuffer && !gl_dbg_fbo_blit_with_texture) {
    glBindTexture(GL_TEXTURE_2D, 0);
    mOwner->p_glBindFramebuffer(GL_READ_FRAMEBUFFER, mFBO);
    if (mWidth == realw && mHeight == realh) {
      mOwner->p_glBlitFramebuffer(0, 0, mWidth, mHeight, 0, 0, mWidth, mHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    } else {
      //mOwner->p_glBlitFramebuffer(0, 0, mWidth, mHeight, 0, 0, realw, realh, GL_COLOR_BUFFER_BIT, GL_LINEAR);
      mOwner->p_glBlitFramebuffer(0, 0, mWidth, mHeight, blitOfsX, blitOfsY, blitOfsX+scaledWidth, blitOfsY+scaledHeight, GL_COLOR_BUFFER_BIT, (gl_letterbox_filter.asInt() ? GL_LINEAR : GL_NEAREST));
    }
  } else {
    GLint oldbindtex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldbindtex);
    glPushAttrib(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_ENABLE_BIT|GL_VIEWPORT_BIT|GL_TRANSFORM_BIT);
    const bool oldBlend = mOwner->blendEnabled;
    const GLint oldCurrDepthMaskState = mOwner->currDepthMaskState;

    mOwner->GLSetViewport(0, 0, realw, realh);

    glBindTexture(GL_TEXTURE_2D, mColorTid);

    //glMatrixMode(GL_PROJECTION);
    //glLoadIdentity();
    mOwner->SetOrthoProjection(0, realw, realh, 0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    mOwner->GLDisableBlend();
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_TEXTURE_2D);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    mOwner->p_glUseProgramObjectARB(0);
    mOwner->currentActiveShader = nullptr;

    if (calcWidth == realw && calcHeight == realh) {
      // copy texture by drawing full quad
      //mOwner->SetOrthoProjection(0, mWidth, mHeight, 0);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex2i(0, 0);
        glTexCoord2f(1.0f, 1.0f); glVertex2i(calcWidth, 0);
        glTexCoord2f(1.0f, 0.0f); glVertex2i(calcWidth, calcHeight);
        glTexCoord2f(0.0f, 0.0f); glVertex2i(0, calcHeight);
      glEnd();
    } else {
      //mOwner->SetOrthoProjection(0, realw, realh, 0, -99999, 99999);
      //glClear(GL_COLOR_BUFFER_BIT); // just in case
      if (gl_letterbox_filter.asInt()) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      }
      glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex2i(blitOfsX, blitOfsY);
        glTexCoord2f(1.0f, 1.0f); glVertex2i(blitOfsX+scaledWidth, blitOfsY);
        glTexCoord2f(1.0f, 0.0f); glVertex2i(blitOfsX+scaledWidth, blitOfsY+scaledHeight);
        glTexCoord2f(0.0f, 0.0f); glVertex2i(blitOfsX, blitOfsY+scaledHeight);
      glEnd();
    }

    glPopAttrib();
    glBindTexture(GL_TEXTURE_2D, oldbindtex);

    mOwner->blendEnabled = oldBlend;
    mOwner->currDepthMaskState = oldCurrDepthMaskState;
  }

  mOwner->ReactivateCurrentFBO();
}



//**************************************************************************
//
//  VOpenGLDrawer::CameraFBOInfo
//
//**************************************************************************

//==========================================================================
//
//  CameraFBOInfo::CameraFBOInfo
//
//==========================================================================
VOpenGLDrawer::CameraFBOInfo::CameraFBOInfo ()
  : fbo()
  , texnum(-1)
  , camwidth(1)
  , camheight(1)
  , index(-1)
{}


//==========================================================================
//
//  CameraFBOInfo::~CameraFBOInfo
//
//==========================================================================
VOpenGLDrawer::CameraFBOInfo::~CameraFBOInfo () {
  //GCon->Logf(NAME_Debug, "*** destroying FBO for camera fbo, texnum=%d; index=%d; fboid=%u", texnum, index, fbo.getFBOid());
  fbo.destroy();
  vassert(!fbo.isValid());
  vassert(fbo.getFBOid() == 0);
  vassert(fbo.getDSRBTid() == 0);
  texnum = -1;
  camwidth = camheight = 1;
  index = -1;
}



//**************************************************************************
//
//  VOpenGLDrawer
//
//**************************************************************************

//==========================================================================
//
//  VOpenGLDrawer::SetMainFBO
//
//==========================================================================
void VOpenGLDrawer::SetMainFBO (bool forced) {
  if (forced || currMainFBO != -1) {
    currMainFBO = -1;
    if (forced) currentActiveFBO = nullptr;
    mainFBO.activate();
    stencilBufferDirty = true;
  }
}


//==========================================================================
//
//  VOpenGLDrawer::ReactivateCurrentFBO
//
//==========================================================================
void VOpenGLDrawer::ReactivateCurrentFBO () {
  if (currentActiveFBO) {
    p_glBindFramebuffer(GL_FRAMEBUFFER, currentActiveFBO->getFBOid());
    ScrWdt = currentActiveFBO->getWidth();
    ScrHgt = currentActiveFBO->getHeight();
  } else {
    p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ScrWdt = ScreenWidth;
    ScrHgt = ScreenHeight;
  }
}


//==========================================================================
//
//  VOpenGLDrawer::DestroyCameraFBOList
//
//==========================================================================
void VOpenGLDrawer::DestroyCameraFBOList () {
  for (auto &&cf : cameraFBOList) { delete cf; cf = nullptr; }
  cameraFBOList.clear();
}


//==========================================================================
//
//  VOpenGLDrawer::ClearCameraFBOs
//
//==========================================================================
void VOpenGLDrawer::ClearCameraFBOs () {
  if (cameraFBOList.length()) GCon->Logf(NAME_Debug, "deleting #%d camera FBO%s", cameraFBOList.length(), (cameraFBOList.length() != 1 ? "s" : ""));
  DestroyCameraFBOList();
}


//==========================================================================
//
//  VOpenGLDrawer::GetCameraFBO
//
//  returns index or -1; (re)creates FBO if necessary
//
//==========================================================================
int VOpenGLDrawer::GetCameraFBO (int texnum, int width, int height) {
  if (width < 1) width = 1;
  if (height < 1) height = 1;

  int cfidx = cameraFBOList.length();

  for (auto &&cf : cameraFBOList) {
    if (cf->texnum == texnum) {
      if (cf->camwidth == width && cf->camheight == height) return cf->index; // nothing to do
      // recreate
      GCon->Logf(NAME_Debug, "recreating camera FBO #%d for texture #%d (old size is %dx%d, new size is %dx%d)", cf->index, texnum, cf->camwidth, cf->camheight, width, height);
      cfidx = cf->index;
      break;
    }
  }

  if (cfidx < cameraFBOList.length()) {
    p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    cameraFBOList[cfidx]->fbo.destroy();
  }

  if (cfidx >= cameraFBOList.length()) {
    cfidx = cameraFBOList.length();
    GCon->Logf(NAME_Debug, "creating new camera FBO #%d for texture #%d (new size is %dx%d)", cfidx, texnum, width, height);
    CameraFBOInfo *cin = new CameraFBOInfo();
    cin->index = cfidx;
    cameraFBOList.append(cin);
    vassert(cameraFBOList.length()-1 == cfidx);
  }

  CameraFBOInfo *ci = cameraFBOList[cfidx];
  vassert(ci->index == cfidx);
  ci->texnum = texnum;
  ci->camwidth = width;
  ci->camheight = height;
  ci->fbo.createDepthStencil(this, width, height);
  //GCon->Logf(NAME_Debug, "*** FBO for camera fbo, texnum=%d; index=%d; fboid=%u", ci->texnum, ci->index, ci->fbo.getFBOid());

  p_glBindFramebuffer(GL_FRAMEBUFFER, ci->fbo.getFBOid());
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // black background
  glClearDepth(!useReverseZ ? 1.0f : 0.0f);
  if (p_glClipControl) p_glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); // actually, this is better even for "normal" cases
  RestoreDepthFunc();
  glDepthRange(0.0f, 1.0f);

  glClearStencil(0);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

  ReactivateCurrentFBO();

  //GCon->Logf(NAME_Debug, "  FBO #%d tid=%u", cfidx, ci->fbo.getColorTid());

  return cfidx;
}


//==========================================================================
//
//  VOpenGLDrawer::FindCameraFBO
//
//  returns index or -1
//
//==========================================================================
int VOpenGLDrawer::FindCameraFBO (int texnum) {
  if (texnum < 0) return -1;
  for (auto &&cf : cameraFBOList) if (cf->texnum == texnum) return cf->index;
  return -1;
}


//==========================================================================
//
//  VOpenGLDrawer::SetCameraFBO
//
//==========================================================================
void VOpenGLDrawer::SetCameraFBO (int cfboindex) {
  if (cfboindex < 0 || cfboindex >= cameraFBOList.length()) return;
  if (currMainFBO != cfboindex) {
    currMainFBO = cfboindex;
    cameraFBOList[cfboindex]->fbo.activate();
    stencilBufferDirty = true;
  }
}


//==========================================================================
//
//  VOpenGLDrawer::GetCameraFBOTextureId
//
//  returns 0 if cfboindex is invalid
//
//==========================================================================
GLuint VOpenGLDrawer::GetCameraFBOTextureId (int cfboindex) {
  if (cfboindex < 0 || cfboindex >= cameraFBOList.length()) return 0;
  return cameraFBOList[cfboindex]->fbo.getColorTid();
  //glBindTexture(GL_TEXTURE_2D, cameraFBOList[cfboindex]->fbo.getColorTid());
  //glBindTexture(GL_TEXTURE_2D, 10);
}


//==========================================================================
//
//  VOpenGLDrawer::ActivateMainFBO
//
//==========================================================================
void VOpenGLDrawer::ActivateMainFBO () {
  if (currMainFBO < 0) {
    mainFBO.activate();
  } else {
    cameraFBOList[currMainFBO]->fbo.activate();
  }
}


//==========================================================================
//
//  VOpenGLDrawer::GetMainFBO
//
//==========================================================================
VOpenGLDrawer::FBO *VOpenGLDrawer::GetMainFBO () {
  return (currMainFBO < 0 ? &mainFBO : &cameraFBOList[currMainFBO]->fbo);
}


//==========================================================================
//
//  VOpenGLDrawer::ReadFBOPixels
//
//==========================================================================
void VOpenGLDrawer::ReadFBOPixels (FBO *srcfbo, int Width, int Height, rgba_t *Dest) {
  if (!srcfbo || Width < 1 || Height < 1 || !Dest) return;

  const int fbowidth = srcfbo->getWidth();
  const int fboheight = srcfbo->getHeight();

  if (fbowidth < 1 || fboheight < 1) {
    memset((void *)Dest, 0, Width*Height*sizeof(rgba_t));
    return;
  }

  if (readBackTempBufSize < fbowidth*fboheight*4) {
    readBackTempBufSize = fbowidth*fboheight*4;
    readBackTempBuf = (vuint8 *)Z_Realloc(readBackTempBuf, readBackTempBufSize);
  }

  GLint oldbindtex = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldbindtex);

  glBindTexture(GL_TEXTURE_2D, srcfbo->getColorTid());
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  rgba_t *temp = (rgba_t *)readBackTempBuf;
  vassert(temp);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, temp);
  glBindTexture(GL_TEXTURE_2D, oldbindtex);

  if (Width <= fbowidth) {
    size_t blen = Width*sizeof(rgba_t);
    for (int y = 0; y < Height; ++y) memcpy(Dest+y*Width, temp+(fboheight-y-1)*fbowidth, blen);
  } else {
    size_t blen = fbowidth*sizeof(rgba_t);
    size_t restlen = Width*sizeof(rgba_t)-blen;
    for (int y = 0; y < Height; ++y) {
      memcpy(Dest+y*Width, temp+(fboheight-y-1)*fbowidth, blen);
      memset((void *)(Dest+y*Width+fbowidth), 0, restlen);
    }
  }
}
