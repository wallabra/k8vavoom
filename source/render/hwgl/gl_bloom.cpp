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
// 2D lighting post process effect: bloom
// this implementation is using code from Alien Arena (including shaders)
#include "gl_local.h"

#define VV_USE_DIRECT_BLIT


// ////////////////////////////////////////////////////////////////////////// //
VCvarB r_bloom("r_bloom", true, "Enable Bloom post-processing effect?", CVAR_Archive);
VCvarF r_bloom_alpha("r_bloom_alpha", "0.2", "Bloom alpha.", CVAR_Archive);
VCvarI r_bloom_diamond_size("r_bloom_diamond_size", "8", "Bloom diamond size.", CVAR_Archive);
VCvarF r_bloom_intensity("r_bloom_intensity", "0.75", "Bloom intensity.", CVAR_Archive);
VCvarF r_bloom_darken("r_bloom_darken", "8", "Bloom darken.", CVAR_Archive);
VCvarF r_bloom_sample_scaledown("r_bloom_sample_scaledown", "2", "Bloom sample scale down.", CVAR_Archive);
VCvarB r_bloom_autoexposure("r_bloom_autoexposure", true, "Use bloom autoexposure?", CVAR_Archive);
VCvarF r_bloom_autoexposure_coeff("r_bloom_autoexposure_coeff", "1.5", "Bloom autoexposure coefficient.", CVAR_Archive);
VCvarB r_bloom_id0_effect("r_bloom_id0_effect", false, "Special bloom effect for id0. ;-)", CVAR_Archive);


//==========================================================================
//
//  Q_log2
//
//==========================================================================
static VVA_OKUNUSED int Q_log2 (int val) {
  int answer = 0;
  while (val >>= 1) ++answer;
  return answer;
}


//==========================================================================
//
//  bloomDebugClear
//
//==========================================================================
static VVA_OKUNUSED void bloomDebugClear () {
  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
}


//==========================================================================
//
//  bloomDrawFSQuad
//
//==========================================================================
static void bloomDrawFSQuad () {
  glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(0, 0);
    glTexCoord2f(1, 1); glVertex2f(1, 0);
    glTexCoord2f(1, 0); glVertex2f(1, 1);
    glTexCoord2f(0, 0); glVertex2f(0, 1);
  glEnd();
}


//==========================================================================
//
//  VOpenGLDrawer::BloomDeinit
//
//==========================================================================
void VOpenGLDrawer::BloomDeinit () {
  //GCon->Logf(NAME_Debug, "OpenGL: deinit bloom");

  if (bloomFullSizeDownsampleRBOid) {
    //GCon->Logf(NAME_Debug, "*** deleting bloom RBO: id=%u", bloomFullSizeDownsampleRBOid);
    p_glDeleteRenderbuffers(1, &bloomFullSizeDownsampleRBOid);
    bloomFullSizeDownsampleRBOid = 0;
  }

  if (bloomFullSizeDownsampleFBOid) {
    //GCon->Logf(NAME_Debug, "*** deleting bloom FBO: id=%u", bloomFullSizeDownsampleFBOid);
    p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    p_glDeleteFramebuffers(1, &bloomFullSizeDownsampleFBOid);
    bloomFullSizeDownsampleFBOid = 0;
  }

  if (bloomColAvgGetterPBOid) {
    //GCon->Logf(NAME_Debug, "*** deleting bloom PBO: id=%u", bloomColAvgGetterPBOid);
    p_glDeleteBuffersARB(1, &bloomColAvgGetterPBOid);
    bloomColAvgGetterPBOid = 0;
  }

  bloomWidth = bloomHeight = bloomMipmapCount = 0;
  bloomscratchFBO.destroy();
  bloomscratch2FBO.destroy();
  bloomeffectFBO.destroy();
  bloomcoloraveragingFBO.destroy();
  bloomColAvgValid = false;
  bloomCurrentFBO = 0;
}


//==========================================================================
//
//  VOpenGLDrawer::BloomAllocRBO
//
//  create a 24-bit RBO with specified size and attach it to an FBO
//
//==========================================================================
void VOpenGLDrawer::BloomAllocRBO (int width, int height, GLuint *RBO, GLuint *FBO) {
  // create the RBO
  GLDRW_RESET_ERROR();
  *RBO = 0;
  p_glGenRenderbuffers(1, RBO);
  if (*RBO == 0) Sys_Error("OpenGL: cannot create bloom renderbuffer storage, error is 0x%04x", (unsigned)glGetError());
  p_glBindRenderbuffer(GL_RENDERBUFFER_EXT, *RBO);
  p_glRenderbufferStorage(GL_RENDERBUFFER_EXT, GL_RGB, width, height);
  p_glBindRenderbuffer(GL_RENDERBUFFER_EXT, 0);

  // create up the FBO
  GLDRW_RESET_ERROR();
  *FBO = 0;
  p_glGenFramebuffers(1, FBO);
  if (*FBO == 0) Sys_Error("OpenGL: cannot create bloom FBO, error is 0x%04x", (unsigned)glGetError());
  p_glBindFramebuffer(GL_FRAMEBUFFER, *FBO);

  // bind the RBO to it
  p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER_EXT, *RBO);

  // check for validity
  GLenum status = p_glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) Sys_Error("OpenGL: bloom framebuffer creation failed (status=0x%04x)", (unsigned)status);

  // clean up
  p_glBindFramebuffer(GL_FRAMEBUFFER, 0);

  //GCon->Logf(NAME_Debug, "*** created bloom FBO/RBO: fboid=%u; rboid=%u", *FBO, *RBO);
}


//==========================================================================
//
//  VOpenGLDrawer::BloomInitEffectTexture
//
//==========================================================================
void VOpenGLDrawer::BloomInitEffectTexture () {
  bloomWidth = bloomScrWdt/max2(1.0f, r_bloom_sample_scaledown.asFloat());
  bloomHeight = bloomScrHgt/max2(1.0f, r_bloom_sample_scaledown.asFloat());
  // we use mipmapping to calculate a 1-pixel average color for auto-exposure
  bloomMipmapCount = Q_log2(bloomWidth > bloomHeight ? bloomWidth : bloomHeight);

  bloomeffectFBO.setLinearFilter(!r_bloom_id0_effect);
  bloomeffectFBO.createTextureOnly(this, bloomWidth, bloomHeight, true);

  bloomcoloraveragingFBO.setLinearFilter(!r_bloom_id0_effect);
  bloomcoloraveragingFBO.createTextureOnly(this, bloomWidth, bloomHeight, true);
  glBindTexture(GL_TEXTURE_2D, bloomcoloraveragingFBO.getColorTid());
  bloomColAvgValid = false;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1000);
  glBindTexture(GL_TEXTURE_2D, 0);

  p_glGenBuffersARB(1, &bloomColAvgGetterPBOid);
  p_glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, bloomColAvgGetterPBOid);
  p_glObjectLabelVA(GL_BUFFER, bloomColAvgGetterPBOid, "BloomAveragerBuffer");
  p_glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, 3, nullptr, GL_STREAM_READ);
  p_glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
}


//==========================================================================
//
//  VOpenGLDrawer::BloomInitTextures
//
//==========================================================================
void VOpenGLDrawer::BloomInitTextures () {
  if (r_bloom_id0_effect != bloomeffectFBO.getLinearFilter()) {
    if (bloomWidth) {
      int bw = bloomScrWdt/max2(1.0f, r_bloom_sample_scaledown.asFloat());
      int bh = bloomScrHgt/max2(1.0f, r_bloom_sample_scaledown.asFloat());
      if (bw == bloomWidth && bh == bloomHeight) return;
    }
  }

  BloomDeinit();

  GLDRW_RESET_ERROR();

  //GCon->Logf(NAME_Debug, "OpenGL: initalize bloom");

  // validate bloom size and init the bloom effect texture
  BloomInitEffectTexture();

  // init the "scratch" texture
  bloomscratchFBO.setLinearFilter(!r_bloom_id0_effect);
  bloomscratchFBO.createTextureOnly(this, bloomWidth, bloomHeight, true);

  bloomscratch2FBO.setLinearFilter(!r_bloom_id0_effect);
  bloomscratch2FBO.createTextureOnly(this, bloomWidth, bloomHeight, true);

  // init the screen-size RBO
  BloomAllocRBO(bloomScrWdt, bloomScrHgt, &bloomFullSizeDownsampleRBOid, &bloomFullSizeDownsampleFBOid);
}


//==========================================================================
//
//  VOpenGLDrawer::BloomDownsampleView
//
//  creates a downscaled, blurred version of the screen, leaving it in the
//  "scratch" and "effect" textures (identical in both).
//
//==========================================================================
void VOpenGLDrawer::BloomDownsampleView (int ax, int ay, int awidth, int aheight) {
  GLDisableBlend();
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

  bloomResetFBOs();

  // downsample
  //p_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  //p_glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  //p_glBlitFramebuffer(0, 0, bloomScrWdt, bloomScrHgt, 0, 0, bloomWidth, bloomHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  //GCon->Logf(NAME_Debug, "ax=%d; ay=%d; awidth=%d; aheight=%d", ax, ay, awidth, aheight);
  //p_glBlitFramebuffer(ax, ay, ax+awidth, ay+aheight, 0, 0, bloomWidth, bloomHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  // passed coords are top-0 based, but FBO coords are bottom-0, so convert
  ay = bloomScrHgt-(ay+aheight);
  #ifdef VV_USE_DIRECT_BLIT
  p_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, bloomeffectFBO.getFBOid());
  p_glBindFramebuffer(GL_READ_FRAMEBUFFER, mainFBO.getFBOid());
  p_glBlitFramebuffer(ax, ay, ax+awidth, ay+aheight, 0, 0, bloomWidth, bloomHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  p_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  p_glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  #else
  mainFBO.blitTo(&bloomeffectFBO, ax, ay, ax+awidth, ay+aheight, 0, 0, bloomWidth, bloomHeight, GL_LINEAR);
  #endif

  // blit the finished downsampled texture onto a second FBO
  // we end up with with two copies, which DoGaussian will take advantage of
  // no, darken pass will do this for us
  /*
  p_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, bloomGetActiveFBO()->getFBOid());
  p_glBindFramebuffer(GL_READ_FRAMEBUFFER, bloomeffectFBO.getFBOid());
  p_glBlitFramebuffer(0, 0, bloomWidth, bloomHeight, 0, 0, bloomWidth, bloomHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  */
}


//==========================================================================
//
//  VOpenGLDrawer::BloomDarken
//
//==========================================================================
void VOpenGLDrawer::BloomDarken () {
  bool activeFBOInited = false;

  // auto-exposure -- adjust the light bloom intensity based on how dark or light the scene is.
  // we use a PBO and give it a whole frame to compute the color average and transfer it into
  // main memory in the background. the extra delay from using last frame's average is harmless.
  if (r_bloom_autoexposure && p_glMapBufferARB && p_glUnmapBufferARB) {
    static constexpr float BrightnessBias = 0.8f;
    static constexpr float BrightnessReverseBias = 0.2f;

    float brightness = 1.0f; // reasonable default

    p_glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, bloomColAvgGetterPBOid);

    // get last frame's color average
    // will only be false just after a vid_restart
    if (bloomColAvgValid) {
      vuint8 *pixel;
      pixel = (vuint8 *)p_glMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY);
      if (pixel != nullptr) {
        brightness = (float)(pixel[0]+pixel[1]+pixel[2])/(256.0f*3.0f);
        p_glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB);
      }
    }

    // start computing this frame's color average so we can use it next frame
    #ifdef VV_USE_DIRECT_BLIT
    p_glBindFramebuffer(GL_READ_FRAMEBUFFER, bloomeffectFBO.getFBOid());
    p_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, bloomcoloraveragingFBO.getFBOid());
    p_glBlitFramebuffer(0, 0, bloomWidth, bloomHeight, 0, 0, bloomWidth, bloomHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    #else
    bloomeffectFBO.blitTo(&bloomcoloraveragingFBO, 0, 0, bloomWidth, bloomHeight, 0, 0, bloomWidth, bloomHeight, GL_NEAREST);
    #endif
    glBindTexture(GL_TEXTURE_2D, bloomcoloraveragingFBO.getColorTid());
    glGetTexImage(GL_TEXTURE_2D, bloomMipmapCount, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    p_glGenerateMipmap(GL_TEXTURE_2D);
    bloomColAvgValid = true; // any time after this, we could read from the rbo
    p_glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);

    //GCon->Logf(NAME_Debug, "000: brightness=%g", brightness);
    // bias upward so that the brighness can occasionally go *above* 1.0 in bright areas
    // that helps kill the bloom completely in bright scenes while leaving some in dark dark scenes
    brightness += BrightnessBias;
    // since the bias will tend to reduce the bloom even in dark
    // scenes, we re-exaggerate the darkness with exponentiation
    brightness = powf(brightness, r_bloom_autoexposure_coeff.asFloat());
    brightness -= BrightnessReverseBias;
    //GCon->Logf(NAME_Debug, "001: brightness=%g", brightness);

    brightness = 1.0f/brightness;
    //GCon->Logf(NAME_Debug, "002: brightness=%g", brightness);

    // apply the color scaling
    glBindTexture(GL_TEXTURE_2D, bloomeffectFBO.getColorTid());
    p_glBindFramebuffer(GL_FRAMEBUFFER, bloomGetActiveFBO()->getFBOid());
    BloomColorScale.Activate();
    BloomColorScale.SetTextureSource(0);
    BloomColorScale.SetScale(brightness, brightness, brightness);
    BloomColorScale.UploadChangedUniforms();
    bloomDrawFSQuad();

    // no need to copy effect FBO again
    activeFBOInited = true;
  }

  // darkening pass
  if (r_bloom_darken.asFloat() > 0.0f) {
    if (!activeFBOInited) {
      // exposure is off, and the thing wasn't copied
      glBindTexture(GL_TEXTURE_2D, bloomeffectFBO.getColorTid());
      p_glBindFramebuffer(GL_FRAMEBUFFER, bloomGetActiveFBO()->getFBOid());
    } else {
      // active FBO is already copied; use inactive FBO, and then simply switch them
      glBindTexture(GL_TEXTURE_2D, bloomGetActiveFBO()->getColorTid());
      p_glBindFramebuffer(GL_FRAMEBUFFER, bloomGetInactiveFBO()->getFBOid());
      // swap FBOs
      bloomSwapFBOs();
    }
    float exp = r_bloom_darken.asFloat(); //+1.0f;
    BloomColorExp.Activate();
    BloomColorExp.SetTextureSource(0);
    BloomColorExp.SetExponent(exp, exp, exp, 1.0f);
    BloomColorExp.UploadChangedUniforms();
    bloomDrawFSQuad();

    // no need to copy effect FBO again
    activeFBOInited = true;
  }

  // if we still didn't copied effect FBO to active FBO, do it now
  if (!activeFBOInited) {
    #ifdef VV_USE_DIRECT_BLIT
    p_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, bloomGetActiveFBO()->getFBOid());
    p_glBindFramebuffer(GL_READ_FRAMEBUFFER, bloomeffectFBO.getFBOid());
    p_glBlitFramebuffer(0, 0, bloomWidth, bloomHeight, 0, 0, bloomWidth, bloomHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    p_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    p_glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    #else
    bloomeffectFBO.blitTo(bloomGetActiveFBO(), 0, 0, bloomWidth, bloomHeight, 0, 0, bloomWidth, bloomHeight, GL_NEAREST);
    #endif
  }
}


//==========================================================================
//
//  VOpenGLDrawer::BloomDoGaussian
//
//==========================================================================
void VOpenGLDrawer::BloomDoGaussian () {
  // set up sample size workspace
  GLSetViewport(0, 0, bloomWidth, bloomHeight);

  //glMatrixMode(GL_PROJECTION);
  //glLoadIdentity();
  SetOrthoProjection(0, 1, 1, 0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  BloomDarken();

  BloomKawase.Activate();
  BloomKawase.SetTextureSource(0);
  GLEnableBlend();

  // compute bloom effect scale and round to nearest odd integer
  int maxscale = r_bloom_diamond_size.asInt()*bloomHeight/384;
  if (maxscale%2 == 0) ++maxscale;
  if (maxscale < 3) maxscale = 3;

  // apply Kawase filters of increasing size until the desired filter size is reached
  // it needs to stay odd, so it must be incremented by an even number each time
  // choosing a higher increment value reduces the quality of the blur but improves performance
  for (int i = 3; i <= maxscale; i += 2) {
    float scale = (float)i/2.0f-1.0f;
    // increasing the repetitions here increases the strength of the blur but hurts performance
    for (int j = 0; j < 2; ++j) {
      glBindTexture(GL_TEXTURE_2D, bloomGetActiveFBO()->getColorTid());
      p_glBindFramebuffer(GL_FRAMEBUFFER, bloomGetInactiveFBO()->getFBOid());
      BloomKawase.SetScaleU(scale/bloomWidth, scale/bloomHeight);
      BloomKawase.UploadChangedUniforms();
      bloomDrawFSQuad();
      // swap FBOs
      bloomSwapFBOs();
    }
  }

  BloomColorScale.Activate();
  BloomColorScale.SetTextureSource(0);
  const float intensity = 6.0f*r_bloom_intensity.asFloat();
  BloomColorScale.SetScale(intensity, intensity, intensity);
  BloomColorScale.UploadChangedUniforms();

  glBlendFunc(GL_ONE, GL_ONE);
  GLEnableBlend();

  glBindTexture(GL_TEXTURE_2D, bloomGetActiveFBO()->getColorTid());
  p_glBindFramebuffer(GL_FRAMEBUFFER, bloomeffectFBO.getFBOid());
  bloomDrawFSQuad();

  //p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


//==========================================================================
//
//  VOpenGLDrawer::BloomDrawEffect
//
//==========================================================================
void VOpenGLDrawer::BloomDrawEffect (int ax, int ay, int awidth, int aheight) {
  SetMainFBO(true); // forced
  DeactivateShader();

  // restore full screen workspace
  GLSetViewport(0, 0, bloomScrWdt, bloomScrHgt);

  //glMatrixMode(GL_PROJECTION);
  //glLoadIdentity();
  SetOrthoProjection(0, bloomScrWdt, bloomScrHgt, 0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  #if 1
  glBindTexture(GL_TEXTURE_2D, bloomeffectFBO.getColorTid());
  GLEnableBlend();
  glBlendFunc(GL_ONE, GL_ONE);
  glColor4f(r_bloom_alpha, r_bloom_alpha, r_bloom_alpha, 1.0f);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(ax,        ay);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(ax,        ay+aheight);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(ax+awidth, ay+aheight);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(ax+awidth, ay);
  glEnd();

  #else
  GLDisableBlend();
  p_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mainFBO.getFBOid());
  //p_glBindFramebuffer(GL_READ_FRAMEBUFFER, /*bloomeffectFBO*//*bloomscratchFBO*/bloomGetActiveFBO()->getFBOid());
  p_glBindFramebuffer(GL_READ_FRAMEBUFFER, bloomeffectFBO.getFBOid());
  //p_glBindFramebuffer(GL_READ_FRAMEBUFFER, bloomGetInactiveFBO()->getFBOid());
  //p_glBlitFramebuffer(0, 0, bloomWidth, bloomHeight, 0, 0, mainFBO.getWidth(), mainFBO.getHeight(), GL_COLOR_BUFFER_BIT, GL_LINEAR/*GL_NEAREST*/);
  ay = bloomScrHgt-(ay+aheight);
  p_glBlitFramebuffer(0, 0, bloomWidth, bloomHeight, ax, ay, ax+awidth, ay+aheight, GL_COLOR_BUFFER_BIT, GL_LINEAR/*GL_NEAREST*/);
  #endif
}


//==========================================================================
//
//  VOpenGLDrawer::renderBloomPosteffect
//
//==========================================================================
void VOpenGLDrawer::Posteffect_Bloom (int ax, int ay, int awidth, int aheight) {
  if (!r_bloom || !canIntoBloomFX) return;
  SetMainFBO(true); // just in case, forced
  if (ScrWdt < 32 || ScrHgt < 32 || ax >= ScrWdt || ay >= ScrHgt) return;
  // normalise coords
  //TODO: check for overflows
  if (ax < 0) {
    if (awidth <= -ax) return;
    awidth += ax;
    ax = 0;
  }
  if (ay < 0) {
    if (aheight <= -ay) return;
    aheight += ay;
    ay = 0;
  }
  if (ax+awidth > ScrWdt) awidth = ScrWdt-ax;
  if (ay+aheight > ScrHgt) aheight = ScrHgt-ay;
  if (awidth < 32 || aheight < 32) return;

  bloomScrWdt = ScrWdt;
  bloomScrHgt = ScrHgt;

  GLint oldbindtex = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldbindtex);
  glPushAttrib(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_ENABLE_BIT|GL_VIEWPORT_BIT|GL_TRANSFORM_BIT);
  const bool oldBlend = blendEnabled;
  const GLint oldCurrDepthMaskState = currDepthMaskState;

  glMatrixMode(GL_MODELVIEW); glPushMatrix();
  glMatrixMode(GL_PROJECTION); glPushMatrix();

  SelectTexture(0);
  DeactivateShader();

  BloomInitTextures(); // it is safe to call this each time
  if (bloomScrWdt >= bloomWidth && bloomScrHgt >= bloomHeight) {
    // set up full screen workspace
    GLSetViewport(0, 0, bloomScrWdt, bloomScrHgt);

    //glMatrixMode(GL_PROJECTION);
    //glLoadIdentity();
    SetOrthoProjection(0, bloomScrWdt, bloomScrHgt, 0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    //glDepthMask(GL_FALSE);
    GLDisableDepthWrite();
    //if (HaveDepthClamp) glDisable(GL_DEPTH_CLAMP);

    GLDisableBlend();
    glEnable(GL_TEXTURE_2D);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    BloomDownsampleView(ax, ay, awidth, aheight);
    BloomDoGaussian();
    BloomDrawEffect(ax, ay, awidth, aheight);
    //BloomDrawEffect(0, 0, bloomScrWdt, bloomScrHgt);

    glColor3f(1.0f, 1.0f, 1.0f);
    GLDisableBlend();
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //glDepthMask(GL_TRUE);
    GLEnableDepthWrite();
  }

  // restore attributes
  glMatrixMode(GL_PROJECTION); glPopMatrix();
  glMatrixMode(GL_MODELVIEW); glPopMatrix();

  glPopAttrib();

  SetMainFBO(true); // just in case, forced
  DeactivateShader();
  glBindTexture(GL_TEXTURE_2D, oldbindtex);

  blendEnabled = oldBlend;
  currDepthMaskState = oldCurrDepthMaskState;
}


//==========================================================================
//
//  VOpenGLDrawer::LevelRendererCreated
//
//==========================================================================
void VOpenGLDrawer::LevelRendererCreated (VRenderLevelPublic *Renderer) {
  //GCon->Logf(NAME_Debug, "LevelRendererCreated");
  BloomDeinit();
}


//==========================================================================
//
//  VOpenGLDrawer::LevelRendererDestroyed
//
//==========================================================================
void VOpenGLDrawer::LevelRendererDestroyed () {
  //GCon->Logf(NAME_Debug, "LevelRendererDestroyed");
  BloomDeinit();
}
