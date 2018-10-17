//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
//**
//**  OpenGL driver, main module
//**
//**************************************************************************
#include "gl_local.h"
#include "../r_local.h"


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB gl_enable_floating_zbuffer("gl_enable_floating_zbuffer", true, "Enable using of floating-point depth buffer for OpenGL3+?", CVAR_Archive);
static VCvarB gl_disable_reverse_z("gl_disable_reverse_z", false, "Completely disable reverse z, even if it is available? (not permanent)", 0);
VCvarB VOpenGLDrawer::gl_dbg_adv_reverse_z("gl_dbg_adv_reverse_z", false, "Don't do this.", 0); // force-enable reverse z for advanced renderer

VCvarI VOpenGLDrawer::texture_filter("gl_texture_filter", "0", "Texture interpolation mode.", CVAR_Archive);
VCvarI VOpenGLDrawer::sprite_filter("gl_sprite_filter", "0", "Sprite interpolation mode.", CVAR_Archive);
VCvarI VOpenGLDrawer::model_filter("gl_model_filter", "3", "Model interpolation mode.", CVAR_Archive);
VCvarB VOpenGLDrawer::gl_2d_filtering("gl_2d_filtering", false, "Filter 2D interface.", CVAR_Archive);
VCvarI VOpenGLDrawer::gl_texture_filter_anisotropic("gl_texture_filter_anisotropic", "1", "Texture anisotropic filtering.", CVAR_Archive);
VCvarB VOpenGLDrawer::clear("gl_clear", true, "Clear screen before rendering new frame?", CVAR_Archive);
VCvarB VOpenGLDrawer::blend_sprites("gl_blend_sprites", false, "Alpha-blend sprites?", CVAR_Archive);
VCvarB VOpenGLDrawer::ext_anisotropy("gl_ext_anisotropy", true, "Use OpenGL anisotropy extension (if present)?", CVAR_Archive);
VCvarF VOpenGLDrawer::maxdist("gl_maxdist", "8192.0", "Max view distance (too big values will cause z-buffer issues).", CVAR_Archive);
VCvarB VOpenGLDrawer::model_lighting("gl_model_lighting", true, "Light models?", CVAR_Archive);
VCvarB VOpenGLDrawer::specular_highlights("gl_specular_highlights", true, "Specular highlights type.", CVAR_Archive);
VCvarI VOpenGLDrawer::multisampling_sample("gl_multisampling_sample", "1", "Multisampling mode.", CVAR_Archive);
VCvarB VOpenGLDrawer::gl_smooth_particles("gl_smooth_particles", false, "Draw smooth particles?", CVAR_Archive);

VCvarB VOpenGLDrawer::gl_dump_vendor("gl_dump_vendor", false, "Dump OpenGL vendor?", 0);
VCvarB VOpenGLDrawer::gl_dump_extensions("gl_dump_extensions", false, "Dump available OpenGL extensions?", 0);

// was 0.333
VCvarF gl_alpha_threshold("gl_alpha_threshold", "0.15", "Alpha threshold (less than this will not be drawn).", CVAR_Archive);

static VCvarI gl_max_anisotropy("gl_max_anisotropy", "1", "Maximum anisotropy level (r/o).", CVAR_Rom);


//==========================================================================
//
//  VOpenGLDrawer::VOpenGLDrawer
//
//==========================================================================
VOpenGLDrawer::VOpenGLDrawer ()
  : VDrawer()
  , texturesGenerated(false)
  , lastgamma(0)
  , CurrentFade(0)
{
  MaxTextureUnits = 1;
  HaveTearControl = false;
  useReverseZ = false;

  mainFBO = 0;
  mainFBOColorTid = 0;
  mainFBODepthStencilTid = 0;

  tmpImgBuf0 = nullptr;
  tmpImgBuf1 = nullptr;
  tmpImgBufSize = 0;

  readBackTempBuf = nullptr;
  readBackTempBufSize = 0;
}


//==========================================================================
//
//  VOpenGLDrawer::~VOpenGLDrawer
//
//==========================================================================
VOpenGLDrawer::~VOpenGLDrawer () {
  if (tmpImgBuf0) { Z_Free(tmpImgBuf0); tmpImgBuf0 = nullptr; }
  if (tmpImgBuf1) { Z_Free(tmpImgBuf1); tmpImgBuf1 = nullptr; }
  tmpImgBufSize = 0;
  if (readBackTempBuf) { Z_Free(readBackTempBuf); readBackTempBuf = nullptr; }
  readBackTempBufSize = 0;
}


//==========================================================================
//
//  VOpenGLDrawer::RestoreDepthFunc
//
//==========================================================================
void VOpenGLDrawer::RestoreDepthFunc () {
  // advanced renderer doesn't support reverse z yet
  glDepthFunc(!CanUseRevZ() ? GL_LEQUAL : GL_GEQUAL);
}


//==========================================================================
//
//  VOpenGLDrawer::RestoreDepthFunc
//
//==========================================================================
void VOpenGLDrawer::SetupTextureFiltering (int level) {
  // for anisotropy, we require trilinear filtering
  if (max_anisotropy > 1) {
    if (gl_texture_filter_anisotropic > 1) {
      // turn on trilinear filtering
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      // setup anisotropy level
      glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT),
        (gl_texture_filter_anisotropic > max_anisotropy ? max_anisotropy : gl_texture_filter_anisotropic)
      );
      return;
    }
    // we have anisotropy, but it is turned off
    glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), 1); // 1 is minimum, i.e. "off"
  }
  int mipfilter, maxfilter;
  // setup filtering
  switch (level) {
    case 1: // nearest mipmap
      maxfilter = GL_NEAREST;
      mipfilter = GL_NEAREST_MIPMAP_NEAREST;
      break;
    case 2: // linear nearest
      maxfilter = GL_LINEAR;
      mipfilter = GL_LINEAR_MIPMAP_NEAREST;
      break;
    case 3: // bilinear
      maxfilter = GL_LINEAR;
      mipfilter = GL_LINEAR;
      break;
    case 4: // trilinear
      maxfilter = GL_LINEAR;
      mipfilter = GL_LINEAR_MIPMAP_LINEAR;
      break;
    default: // nearest, no mipmaps
      maxfilter = GL_NEAREST;
      mipfilter = GL_NEAREST;
      break;
  }
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipfilter);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, maxfilter);
}


//==========================================================================
//
//  VOpenGLDrawer::InitResolution
//
//==========================================================================
void VOpenGLDrawer::InitResolution () {
  guard(VOpenGLDrawer::InitResolution);

  GCon->Logf(NAME_Init, "Setting up new resolution: %dx%d", ScreenWidth, ScreenHeight);

  if (gl_dump_vendor) {
    GCon->Logf(NAME_Init, "GL_VENDOR: %s", glGetString(GL_VENDOR));
    GCon->Logf(NAME_Init, "GL_RENDERER: %s", glGetString(GL_RENDERER));
    GCon->Logf(NAME_Init, "GL_VERSION: %s", glGetString (GL_VERSION));
  }

  if (gl_dump_extensions) {
    GCon->Log(NAME_Init, "GL_EXTENSIONS:");
    TArray<VStr> Exts;
    VStr((char *)glGetString(GL_EXTENSIONS)).Split(' ', Exts);
    for (int i = 0; i < Exts.Num(); ++i) GCon->Log(NAME_Init, VStr("- ")+Exts[i]);
  }

  // check the maximum texture size
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
  GCon->Logf(NAME_Init, "Maximum texture size: %d", maxTexSize);
  if (maxTexSize < 1024) maxTexSize = 1024; // 'cmon!

  /*
  if (CheckExtension("ARB_get_proc_address")) {
    GCon->Logf("OpenGL \"ARB_get_proc_address\" extension found");
  }
  */

  HaveTearControl = false;
  if (CheckGLXExtension("GLX_EXT_swap_control_tear")) {
    GCon->Logf(NAME_Init, "OpenGL tear control extension found");
    HaveTearControl = true;
    if (r_vsync && SetAdaptiveSwap()) GCon->Logf(NAME_Init, "GLX: Turned on adaptive swap");
  }

#define _(x)  p_##x = x##_t(GetExtFuncPtr(#x)); if (!p_##x) found = false

  useReverseZ = false;
  GLint major, minor;
  glGetIntegerv(GL_MAJOR_VERSION, &major);
  glGetIntegerv(GL_MINOR_VERSION, &minor);
  GCon->Logf(NAME_Init, "OpenGL v%d.%d found", major, minor);
  if ((major > 4 || (major == 4 && minor >= 5)) || CheckExtension("GL_ARB_clip_control")) {
    //glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
    p_glClipControl = glClipControl_t(GetExtFuncPtr("glClipControl"));
    if (p_glClipControl) {
      GCon->Logf(NAME_Init, "OpenGL: glClipControl found, using reverse z");
      useReverseZ = true;
      if (gl_disable_reverse_z) {
        GCon->Logf(NAME_Init, "OpenGL: oops, user disabled reverse z, i shall obey");
        useReverseZ = false;
      }
    }
  } else {
    p_glClipControl = nullptr;
  }

  // check multi-texture extensions
  if (!CheckExtension("GL_ARB_multitexture")) {
    Sys_Error("OpenGL FATAL: Multitexture extensions not found.");
  } else {
    bool found = true;

    _(glMultiTexCoord2fARB);
    _(glActiveTextureARB);

    if (!found) Sys_Error("OpenGL FATAL: Multitexture extensions not found.");

    GCon->Log(NAME_Init, "Multitexture extensions found.");
    GLint tmp;
    glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max texture units: %d", tmp);
    if (tmp > 1) MaxTextureUnits = tmp;
  }

  // anisotropy extension
  max_anisotropy = 1.0;
  if (ext_anisotropy && CheckExtension("GL_EXT_texture_filter_anisotropic")) {
    glGetFloatv(GLenum(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT), &max_anisotropy);
    if (max_anisotropy < 1) max_anisotropy = 1;
    GCon->Logf(NAME_Init, "Max anisotropy %g", (double)max_anisotropy);
  }
  gl_max_anisotropy = (int)max_anisotropy;

  // clamp to edge extension
  if (CheckExtension("GL_SGIS_texture_edge_clamp") || CheckExtension("GL_EXT_texture_edge_clamp")) {
    GCon->Log(NAME_Init, "Clamp to edge extension found.");
    ClampToEdge = GL_CLAMP_TO_EDGE_SGIS;
  }
  else {
    ClampToEdge = GL_CLAMP;
  }

  // check for shader extensions
  if (CheckExtension("GL_ARB_shader_objects") && CheckExtension("GL_ARB_shading_language_100") &&
      CheckExtension("GL_ARB_vertex_shader") && CheckExtension("GL_ARB_fragment_shader"))
  {
    bool found = true;

    _(glDeleteObjectARB);
    _(glGetHandleARB);
    _(glDetachObjectARB);
    _(glCreateShaderObjectARB);
    _(glShaderSourceARB);
    _(glCompileShaderARB);
    _(glCreateProgramObjectARB);
    _(glAttachObjectARB);
    _(glLinkProgramARB);
    _(glUseProgramObjectARB);
    _(glValidateProgramARB);
    _(glUniform1fARB);
    _(glUniform2fARB);
    _(glUniform3fARB);
    _(glUniform4fARB);
    _(glUniform1iARB);
    _(glUniform2iARB);
    _(glUniform3iARB);
    _(glUniform4iARB);
    _(glUniform1fvARB);
    _(glUniform2fvARB);
    _(glUniform3fvARB);
    _(glUniform4fvARB);
    _(glUniform1ivARB);
    _(glUniform2ivARB);
    _(glUniform3ivARB);
    _(glUniform4ivARB);
    _(glUniformMatrix2fvARB);
    _(glUniformMatrix3fvARB);
    _(glUniformMatrix4fvARB);
    _(glGetObjectParameterfvARB);
    _(glGetObjectParameterivARB);
    _(glGetInfoLogARB);
    _(glGetAttachedObjectsARB);
    _(glGetUniformLocationARB);
    _(glGetActiveUniformARB);
    _(glGetUniformfvARB);
    _(glGetUniformivARB);
    _(glGetShaderSourceARB);

    _(glVertexAttrib1dARB);
    _(glVertexAttrib1dvARB);
    _(glVertexAttrib1fARB);
    _(glVertexAttrib1fvARB);
    _(glVertexAttrib1sARB);
    _(glVertexAttrib1svARB);
    _(glVertexAttrib2dARB);
    _(glVertexAttrib2dvARB);
    _(glVertexAttrib2fARB);
    _(glVertexAttrib2fvARB);
    _(glVertexAttrib2sARB);
    _(glVertexAttrib2svARB);
    _(glVertexAttrib3dARB);
    _(glVertexAttrib3dvARB);
    _(glVertexAttrib3fARB);
    _(glVertexAttrib3fvARB);
    _(glVertexAttrib3sARB);
    _(glVertexAttrib3svARB);
    _(glVertexAttrib4NbvARB);
    _(glVertexAttrib4NivARB);
    _(glVertexAttrib4NsvARB);
    _(glVertexAttrib4NubARB);
    _(glVertexAttrib4NubvARB);
    _(glVertexAttrib4NuivARB);
    _(glVertexAttrib4NusvARB);
    _(glVertexAttrib4bvARB);
    _(glVertexAttrib4dARB);
    _(glVertexAttrib4dvARB);
    _(glVertexAttrib4fARB);
    _(glVertexAttrib4fvARB);
    _(glVertexAttrib4ivARB);
    _(glVertexAttrib4sARB);
    _(glVertexAttrib4svARB);
    _(glVertexAttrib4ubvARB);
    _(glVertexAttrib4uivARB);
    _(glVertexAttrib4usvARB);
    _(glVertexAttribPointerARB);
    _(glEnableVertexAttribArrayARB);
    _(glDisableVertexAttribArrayARB);
    _(glBindAttribLocationARB);
    _(glGetActiveAttribARB);
    _(glGetAttribLocationARB);
    _(glGetVertexAttribdvARB);
    _(glGetVertexAttribfvARB);
    _(glGetVertexAttribivARB);
    _(glGetVertexAttribPointervARB);

    if (!found) Sys_Error("OpenGL FATAL: no shader support");

    GLint tmp;
    GCon->Logf(NAME_Init, "Shading language version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION_ARB));
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max texture image units: %d", tmp);
    if (tmp > 1) MaxTextureUnits = tmp; // this is number of texture *samplers*, but it is ok for our shaders case
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max vertex uniform components: %d", tmp);
    glGetIntegerv(GL_MAX_VARYING_FLOATS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max varying floats: %d", tmp);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max vertex attribs: %d", tmp);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max fragment uniform components: %d", tmp);
  } else {
    Sys_Error("OpenGL FATAL: no shader support");
  }

  {
    bool found = true;
    _(glStencilFuncSeparate);
    _(glStencilOpSeparate);
    if (found) {
      GCon->Log(NAME_Init, "Found OpenGL 2.0 separate stencil methods");
    } else if (CheckExtension("GL_ATI_separate_stencil")) {
      GCon->Log(NAME_Init, "Found GL_ATI_separate_stencil...");
      p_glStencilFuncSeparate = glStencilFuncSeparate_t(GetExtFuncPtr("glStencilFuncSeparateATI"));
      p_glStencilOpSeparate = glStencilOpSeparate_t(GetExtFuncPtr("glStencilOpSeparateATI"));
      if (p_glStencilFuncSeparate && p_glStencilOpSeparate) GCon->Log(NAME_Init, "Separate stencil extensions found");
    }
  }

  if (CheckExtension("GL_ARB_depth_clamp")) {
    GCon->Log(NAME_Init, "Found GL_ARB_depth_clamp...");
    HaveDepthClamp = true;
  } else if (CheckExtension("GL_NV_depth_clamp")) {
    GCon->Log(NAME_Init, "Found GL_NV_depth_clamp...");
    HaveDepthClamp = true;
  } else {
    GCon->Log(NAME_Init, "Symbol not found, depth clamp extensions disabled.");
    HaveDepthClamp = false;
  }

  if (CheckExtension("GL_EXT_stencil_wrap")) {
    GCon->Log(NAME_Init, "Found GL_EXT_stencil_wrap...");
    HaveStencilWrap = true;
  } else {
    GCon->Log(NAME_Init, "Symbol not found, stencil wrap extensions disabled.");
    HaveStencilWrap = false;
  }

  if (!CheckExtension("GL_ARB_vertex_buffer_object")) {
    Sys_Error("OpenGL FATAL: VBO not found.");
  } else {
    bool found = true;

    _(glBindBufferARB);
    _(glDeleteBuffersARB);
    _(glGenBuffersARB);
    _(glIsBufferARB);
    _(glBufferDataARB);
    _(glBufferSubDataARB);
    _(glGetBufferSubDataARB);
    _(glMapBufferARB);
    _(glUnmapBufferARB);
    _(glGetBufferParameterivARB);
    _(glGetBufferPointervARB);

    if (!found) Sys_Error("OpenGL FATAL: VBO not found.");
  }

  if (CheckExtension("GL_EXT_draw_range_elements")) {
    GCon->Log(NAME_Init, "Found GL_EXT_draw_range_elements...");

    bool found = true;
    _(glDrawRangeElementsEXT);

    if (found) {
      GCon->Log(NAME_Init, "Draw range elements extensions found.");
      HaveDrawRangeElements = true;
    } else {
      GCon->Log(NAME_Init, "Symbol not found, draw range elements extensions disabled.");
      HaveDrawRangeElements = false;
    }
  } else {
    HaveDrawRangeElements = false;
  }

#undef _

  glFramebufferTexture2D = (glFramebufferTexture2DFn)GetExtFuncPtr("glFramebufferTexture2D");
  glDeleteFramebuffers = (glDeleteFramebuffersFn)GetExtFuncPtr("glDeleteFramebuffers");
  glGenFramebuffers = (glGenFramebuffersFn)GetExtFuncPtr("glGenFramebuffers");
  glCheckFramebufferStatus = (glCheckFramebufferStatusFn)GetExtFuncPtr("glCheckFramebufferStatus");
  glBindFramebuffer = (glBindFramebufferFn)GetExtFuncPtr("glBindFramebuffer");

  if (!glFramebufferTexture2D || !glDeleteFramebuffers || !glGenFramebuffers ||
      !glCheckFramebufferStatus || !glBindFramebuffer)
  {
    Sys_Error("OpenGL FBO API not found");
  }

  //GCon->Logf("********* %d : %d *********", ScreenWidth, ScreenHeight);

  if (mainFBO) {
    glBindFramebuffer(GL_FRAMEBUFFER, mainFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (mainFBOColorTid) glDeleteTextures(1, &mainFBOColorTid);
    if (mainFBODepthStencilTid) glDeleteTextures(1, &mainFBODepthStencilTid);
    glDeleteFramebuffers(1, &mainFBO);
    mainFBO = 0;
    mainFBOColorTid = 0;
    mainFBODepthStencilTid = 0;
  }

  // allocate FBO object
  glGenFramebuffers(1, &mainFBO);
  if (mainFBO == 0) Sys_Error("OpenGL: cannot create main FBO");
  glBindFramebuffer(GL_FRAMEBUFFER, mainFBO);

  // attach 2D texture to this mainFBO
  glGenTextures(1, &mainFBOColorTid);
  if (mainFBOColorTid == 0) Sys_Error("OpenGL: cannot create RGBA texture for main FBO");
  glBindTexture(GL_TEXTURE_2D, mainFBOColorTid);

  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, /*GL_CLAMP_TO_EDGE*/ClampToEdge);
  //glnvg__checkError(gl, "glnvg__allocFBO: glTexParameterf: GL_TEXTURE_WRAP_S");
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, /*GL_CLAMP_TO_EDGE*/ClampToEdge);

  //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // empty texture
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ScreenWidth, ScreenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mainFBOColorTid, 0);

  // attach stencil texture to this FBO
  glGenTextures(1, &mainFBODepthStencilTid);
  if (mainFBODepthStencilTid == 0) Sys_Error("OpenGL: cannot create stencil texture for main FBO");
  glBindTexture(GL_TEXTURE_2D, mainFBODepthStencilTid);

  (void)glGetError();
  if (!useReverseZ) {
    if (major >= 3 && gl_enable_floating_zbuffer) GCon->Logf(NAME_Init, "OpenGL: using floating-point depth buffer");
    glTexImage2D(GL_TEXTURE_2D, 0, (major >= 3 && gl_enable_floating_zbuffer ? GL_DEPTH32F_STENCIL8 : GL_DEPTH_STENCIL), ScreenWidth, ScreenHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
  } else {
    // reversed z
    glTexImage2D(GL_TEXTURE_2D, 0, (useReverseZ ? GL_DEPTH32F_STENCIL8 : GL_DEPTH_STENCIL), ScreenWidth, ScreenHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
  }
  if (glGetError() != 0) {
    if (useReverseZ) {
      GCon->Log(NAME_Init, "OpenGL: cannot create fp depth buffer, turning off reverse z");
      useReverseZ = false;
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_STENCIL, ScreenWidth, ScreenHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
    } else {
      Sys_Error("OpenGL initialization error");
    }
  }
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, mainFBODepthStencilTid, 0);

  {
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) Sys_Error("OpenGL: framebuffer creation failed");
  }


  glClearColor(0.0, 0.0, 0.0, 0.0); // Black Background
  glClearDepth(!useReverseZ ? 1.0f : 0.0f);
  if (p_glClipControl) p_glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); // actually, this is better even for "normal" cases
  RestoreDepthFunc();
  glDepthRange(0.0, 1.0);

  glClearStencil(0);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

  glBindTexture(GL_TEXTURE_2D, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);


  glClearColor(0.0, 0.0, 0.0, 0.0); // Black Background
  glClearDepth(!useReverseZ ? 1.0f : 0.0f);
  if (p_glClipControl) p_glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); // actually, this is better even for "normal" cases
  RestoreDepthFunc();
  glDepthRange(0.0, 1.0);

  glClearStencil(0);

  glClear(GL_COLOR_BUFFER_BIT);
  Update();
  glClear(GL_COLOR_BUFFER_BIT);

  glEnable(GL_TEXTURE_2D);
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
  GenerateTextures();

  //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glAlphaFunc(GL_GREATER, getAlphaThreshold());
  glShadeModel(GL_FLAT);

  glDisable(GL_POLYGON_SMOOTH);

  GLhandleARB VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/draw_simple.vs");
  GLhandleARB FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/draw_simple.fs");
  DrawSimpleProgram = CreateProgram(VertexShader, FragmentShader);
  DrawSimpleTextureLoc = p_glGetUniformLocationARB(DrawSimpleProgram, "Texture");
  DrawSimpleAlphaLoc = p_glGetUniformLocationARB(DrawSimpleProgram, "Alpha");

  //  Reuses vertex shader.
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/draw_shadow.fs");
  DrawShadowProgram = CreateProgram(VertexShader, FragmentShader);
  DrawShadowTextureLoc = p_glGetUniformLocationARB(DrawShadowProgram, "Texture");
  DrawShadowAlphaLoc = p_glGetUniformLocationARB(DrawShadowProgram, "Alpha");

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/draw_fixed_col.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/draw_fixed_col.fs");
  DrawFixedColProgram = CreateProgram(VertexShader, FragmentShader);
  DrawFixedColColourLoc = p_glGetUniformLocationARB(DrawFixedColProgram, "Colour");

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/draw_automap.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/draw_automap.fs");
  DrawAutomapProgram = CreateProgram(VertexShader, FragmentShader);

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_zbuf.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_zbuf.fs");
  SurfZBufProgram = CreateProgram(VertexShader, FragmentShader);

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_decal_adv.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_decal_adv.fs");
  SurfAdvDecalProgram = CreateProgram(VertexShader, FragmentShader);
  SurfAdvDecalTextureLoc = p_glGetUniformLocationARB(SurfAdvDecalProgram, "Texture");
  SurfAdvDecalSplatColourLoc = p_glGetUniformLocationARB(SurfAdvDecalProgram, "SplatColour");
  SurfAdvDecalSplatAlphaLoc = p_glGetUniformLocationARB(SurfAdvDecalProgram, "SplatAlpha");
  SurfAdvDecalLightLoc = p_glGetUniformLocationARB(SurfAdvDecalProgram, "Light");

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_decal_nolmap.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_decal_nolmap.fs");
  SurfDecalNoLMapProgram = CreateProgram(VertexShader, FragmentShader);
  SurfDecalNoLMapTextureLoc = p_glGetUniformLocationARB(SurfDecalNoLMapProgram, "Texture");
  SurfDecalNoLMapSplatColourLoc = p_glGetUniformLocationARB(SurfDecalNoLMapProgram, "SplatColour");
  SurfDecalNoLMapSplatAlphaLoc = p_glGetUniformLocationARB(SurfDecalNoLMapProgram, "SplatAlpha");
  SurfDecalNoLMapLightLoc = p_glGetUniformLocationARB(SurfDecalNoLMapProgram, "Light");
  SurfDecalNoLMapFogEnabledLoc = p_glGetUniformLocationARB(SurfDecalNoLMapProgram, "FogEnabled");
  SurfDecalNoLMapFogTypeLoc = p_glGetUniformLocationARB(SurfDecalNoLMapProgram, "FogType");
  SurfDecalNoLMapFogColourLoc = p_glGetUniformLocationARB(SurfDecalNoLMapProgram, "FogColour");
  SurfDecalNoLMapFogDensityLoc = p_glGetUniformLocationARB(SurfDecalNoLMapProgram, "FogDensity");
  SurfDecalNoLMapFogStartLoc = p_glGetUniformLocationARB(SurfDecalNoLMapProgram, "FogStart");
  SurfDecalNoLMapFogEndLoc = p_glGetUniformLocationARB(SurfDecalNoLMapProgram, "FogEnd");

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_decal_lmap.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_decal_lmap.fs");
  SurfDecalProgram = CreateProgram(VertexShader, FragmentShader);
  SurfDecalTextureLoc = p_glGetUniformLocationARB(SurfDecalProgram, "Texture");
  SurfDecalSplatColourLoc = p_glGetUniformLocationARB(SurfDecalProgram, "SplatColour");
  SurfDecalSplatAlphaLoc = p_glGetUniformLocationARB(SurfDecalProgram, "SplatAlpha");
  SurfDecalLightLoc = p_glGetUniformLocationARB(SurfDecalProgram, "Light");
  SurfDecalFogEnabledLoc = p_glGetUniformLocationARB(SurfDecalProgram, "FogEnabled");
  SurfDecalFogTypeLoc = p_glGetUniformLocationARB(SurfDecalProgram, "FogType");
  SurfDecalFogColourLoc = p_glGetUniformLocationARB(SurfDecalProgram, "FogColour");
  SurfDecalFogDensityLoc = p_glGetUniformLocationARB(SurfDecalProgram, "FogDensity");
  SurfDecalFogStartLoc = p_glGetUniformLocationARB(SurfDecalProgram, "FogStart");
  SurfDecalFogEndLoc = p_glGetUniformLocationARB(SurfDecalProgram, "FogEnd");

  SurfDecalSAxisLoc = p_glGetUniformLocationARB(SurfDecalProgram, "SAxis");
  SurfDecalTAxisLoc = p_glGetUniformLocationARB(SurfDecalProgram, "TAxis");
  SurfDecalSOffsLoc = p_glGetUniformLocationARB(SurfDecalProgram, "SOffs");
  SurfDecalTOffsLoc = p_glGetUniformLocationARB(SurfDecalProgram, "TOffs");
  SurfDecalTexMinSLoc = p_glGetUniformLocationARB(SurfDecalProgram, "TexMinS");
  SurfDecalTexMinTLoc = p_glGetUniformLocationARB(SurfDecalProgram, "TexMinT");
  SurfDecalCacheSLoc = p_glGetUniformLocationARB(SurfDecalProgram, "CacheS");
  SurfDecalCacheTLoc = p_glGetUniformLocationARB(SurfDecalProgram, "CacheT");
  SurfDecalLightMapLoc = p_glGetUniformLocationARB(SurfDecalProgram, "LightMap");
  SurfDecalSpecularMapLoc = p_glGetUniformLocationARB(SurfDecalProgram, "SpecularMap");

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_simple.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_simple.fs");
  SurfSimpleProgram = CreateProgram(VertexShader, FragmentShader);
  SurfSimpleSAxisLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "SAxis");
  SurfSimpleTAxisLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "TAxis");
  SurfSimpleSOffsLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "SOffs");
  SurfSimpleTOffsLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "TOffs");
  SurfSimpleTexIWLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "TexIW");
  SurfSimpleTexIHLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "TexIH");
  SurfSimpleTextureLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "Texture");
  SurfSimpleLightLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "Light");
  SurfSimpleFogEnabledLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "FogEnabled");
  SurfSimpleFogTypeLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "FogType");
  SurfSimpleFogColourLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "FogColour");
  SurfSimpleFogDensityLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "FogDensity");
  SurfSimpleFogStartLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "FogStart");
  SurfSimpleFogEndLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "FogEnd");

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_lightmap.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_lightmap.fs");
  SurfLightmapProgram = CreateProgram(VertexShader, FragmentShader);
  SurfLightmapSAxisLoc = p_glGetUniformLocationARB(SurfLightmapProgram, "SAxis");
  SurfLightmapTAxisLoc = p_glGetUniformLocationARB(SurfLightmapProgram, "TAxis");
  SurfLightmapSOffsLoc = p_glGetUniformLocationARB(SurfLightmapProgram, "SOffs");
  SurfLightmapTOffsLoc = p_glGetUniformLocationARB(SurfLightmapProgram, "TOffs");
  SurfLightmapTexIWLoc = p_glGetUniformLocationARB(SurfLightmapProgram, "TexIW");
  SurfLightmapTexIHLoc = p_glGetUniformLocationARB(SurfLightmapProgram, "TexIH");
  SurfLightmapTexMinSLoc = p_glGetUniformLocationARB(SurfLightmapProgram, "TexMinS");
  SurfLightmapTexMinTLoc = p_glGetUniformLocationARB(SurfLightmapProgram, "TexMinT");
  SurfLightmapCacheSLoc = p_glGetUniformLocationARB(SurfLightmapProgram, "CacheS");
  SurfLightmapCacheTLoc = p_glGetUniformLocationARB(SurfLightmapProgram, "CacheT");
  SurfLightmapTextureLoc = p_glGetUniformLocationARB(SurfLightmapProgram, "Texture");
  SurfLightmapLightMapLoc = p_glGetUniformLocationARB(SurfLightmapProgram, "LightMap");
  SurfLightmapSpecularMapLoc = p_glGetUniformLocationARB(SurfLightmapProgram, "SpecularMap");
  SurfLightmapFogEnabledLoc = p_glGetUniformLocationARB(SurfLightmapProgram, "FogEnabled");
  SurfLightmapFogTypeLoc = p_glGetUniformLocationARB(SurfLightmapProgram, "FogType");
  SurfLightmapFogColourLoc = p_glGetUniformLocationARB(SurfLightmapProgram, "FogColour");
  SurfLightmapFogDensityLoc = p_glGetUniformLocationARB(SurfLightmapProgram, "FogDensity");
  SurfLightmapFogStartLoc = p_glGetUniformLocationARB(SurfLightmapProgram, "FogStart");
  SurfLightmapFogEndLoc = p_glGetUniformLocationARB(SurfLightmapProgram, "FogEnd");

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_sky.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_sky.fs");
  SurfSkyProgram = CreateProgram(VertexShader, FragmentShader);
  SurfSkyTextureLoc = p_glGetUniformLocationARB(SurfSkyProgram, "Texture");
  SurfSkyBrightnessLoc = p_glGetUniformLocationARB(SurfSkyProgram, "Brightness");
  SurfSkyTexCoordLoc = p_glGetAttribLocationARB(SurfSkyProgram, "TexCoord");

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_dsky.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_dsky.fs");
  SurfDSkyProgram = CreateProgram(VertexShader, FragmentShader);
  SurfDSkyTextureLoc = p_glGetUniformLocationARB(SurfDSkyProgram, "Texture");
  SurfDSkyTexture2Loc = p_glGetUniformLocationARB(SurfDSkyProgram, "Texture2");
  SurfDSkyBrightnessLoc = p_glGetUniformLocationARB(SurfDSkyProgram, "Brightness");
  SurfDSkyTexCoordLoc = p_glGetAttribLocationARB(SurfDSkyProgram, "TexCoord");
  SurfDSkyTexCoord2Loc = p_glGetAttribLocationARB(SurfDSkyProgram, "TexCoord2");

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_masked.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_masked.fs");
  SurfMaskedProgram = CreateProgram(VertexShader, FragmentShader);
  SurfMaskedTextureLoc = p_glGetUniformLocationARB(SurfMaskedProgram, "Texture");
  SurfMaskedLightLoc = p_glGetUniformLocationARB(SurfMaskedProgram, "Light");
  SurfMaskedFogEnabledLoc = p_glGetUniformLocationARB(SurfMaskedProgram, "FogEnabled");
  SurfMaskedFogTypeLoc = p_glGetUniformLocationARB(SurfMaskedProgram, "FogType");
  SurfMaskedFogColourLoc = p_glGetUniformLocationARB(SurfMaskedProgram, "FogColour");
  SurfMaskedFogDensityLoc = p_glGetUniformLocationARB(SurfMaskedProgram, "FogDensity");
  SurfMaskedFogStartLoc = p_glGetUniformLocationARB(SurfMaskedProgram, "FogStart");
  SurfMaskedFogEndLoc = p_glGetUniformLocationARB(SurfMaskedProgram, "FogEnd");
  SurfMaskedAlphaRefLoc = p_glGetUniformLocationARB(SurfMaskedProgram, "AlphaRef");
  SurfMaskedTexCoordLoc = p_glGetAttribLocationARB(SurfMaskedProgram, "TexCoord");

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_model.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_model.fs");
  SurfModelProgram = CreateProgram(VertexShader, FragmentShader);
  SurfModelInterLoc = p_glGetUniformLocationARB(SurfModelProgram, "Inter");
  SurfModelTextureLoc = p_glGetUniformLocationARB(SurfModelProgram, "Texture");
  SurfModelFogEnabledLoc = p_glGetUniformLocationARB(SurfModelProgram, "FogEnabled");
  SurfModelFogTypeLoc = p_glGetUniformLocationARB(SurfModelProgram, "FogType");
  SurfModelFogColourLoc = p_glGetUniformLocationARB(SurfModelProgram, "FogColour");
  SurfModelFogDensityLoc = p_glGetUniformLocationARB(SurfModelProgram, "FogDensity");
  SurfModelFogStartLoc = p_glGetUniformLocationARB(SurfModelProgram, "FogStart");
  SurfModelFogEndLoc = p_glGetUniformLocationARB(SurfModelProgram, "FogEnd");
  SurfModelVert2Loc = p_glGetAttribLocationARB(SurfModelProgram, "Vert2");
  SurfModelTexCoordLoc = p_glGetAttribLocationARB(SurfModelProgram, "TexCoord");
  ShadowsModelAlphaLoc = p_glGetUniformLocationARB(SurfModelProgram, "InAlpha");
  SurfModelLightValLoc = p_glGetAttribLocationARB(SurfModelProgram, "LightVal");
  SurfModelViewOrigin = p_glGetUniformLocationARB(SurfModelProgram, "ViewOrigin");
  SurfModelAllowTransparency = p_glGetUniformLocationARB(SurfModelProgram, "AllowTransparency");

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_part.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_part.fs");
  SurfPartProgram = CreateProgram(VertexShader, FragmentShader);
  SurfPartTexCoordLoc = p_glGetAttribLocationARB(SurfPartProgram, "TexCoord");
  SurfPartLightValLoc = p_glGetAttribLocationARB(SurfPartProgram, "LightVal");
  SurfPartSmoothParticleLoc = p_glGetUniformLocationARB(SurfPartProgram, "SmoothParticle");

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_ambient.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_ambient.fs");
  ShadowsAmbientProgram = CreateProgram(VertexShader, FragmentShader);
  ShadowsAmbientLightLoc = p_glGetUniformLocationARB(ShadowsAmbientProgram, "Light");
  ShadowsAmbientSAxisLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "SAxis");
  ShadowsAmbientTAxisLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "TAxis");
  ShadowsAmbientSOffsLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "SOffs");
  ShadowsAmbientTOffsLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "TOffs");
  ShadowsAmbientTexIWLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "TexIW");
  ShadowsAmbientTexIHLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "TexIH");
  ShadowsAmbientTextureLoc = p_glGetUniformLocationARB(ShadowsAmbientProgram, "Texture");

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_light.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_light.fs");
  ShadowsLightProgram = CreateProgram(VertexShader, FragmentShader);
  ShadowsLightLightPosLoc = p_glGetUniformLocationARB(ShadowsLightProgram, "LightPos");
  ShadowsLightLightRadiusLoc = p_glGetUniformLocationARB(ShadowsLightProgram, "LightRadius");
  ShadowsLightLightColourLoc = p_glGetUniformLocationARB(ShadowsLightProgram, "LightColour");
  ShadowsLightSurfNormalLoc = p_glGetAttribLocationARB(ShadowsLightProgram, "SurfNormal");
  ShadowsLightSurfDistLoc = p_glGetAttribLocationARB(ShadowsLightProgram, "SurfDist");
  ShadowsLightSAxisLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "SAxis");
  ShadowsLightTAxisLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "TAxis");
  ShadowsLightSOffsLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "SOffs");
  ShadowsLightTOffsLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "TOffs");
  ShadowsLightTexIWLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "TexIW");
  ShadowsLightTexIHLoc = p_glGetUniformLocationARB(SurfSimpleProgram, "TexIH");
  ShadowsLightTextureLoc = p_glGetUniformLocationARB(ShadowsAmbientProgram, "Texture");
  ShadowsLightAlphaLoc = p_glGetUniformLocationARB(ShadowsLightProgram, "InAlpha");
  ShadowsLightViewOrigin = p_glGetUniformLocationARB(ShadowsLightProgram, "ViewOrigin");

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_texture.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_texture.fs");
  ShadowsTextureProgram = CreateProgram(VertexShader, FragmentShader);
  ShadowsTextureTexCoordLoc = p_glGetAttribLocationARB(ShadowsTextureProgram, "TexCoord");
  ShadowsTextureTextureLoc = p_glGetUniformLocationARB(ShadowsTextureProgram, "Texture");

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_model_ambient.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_model_ambient.fs");
  ShadowsModelAmbientProgram = CreateProgram(VertexShader, FragmentShader);
  ShadowsModelAmbientInterLoc = p_glGetUniformLocationARB(ShadowsModelAmbientProgram, "Inter");
  ShadowsModelAmbientTextureLoc = p_glGetUniformLocationARB(ShadowsModelAmbientProgram, "Texture");
  ShadowsModelAmbientLightLoc = p_glGetUniformLocationARB(ShadowsModelAmbientProgram, "Light");
  ShadowsModelAmbientModelToWorldMatLoc = p_glGetUniformLocationARB(ShadowsModelAmbientProgram, "ModelToWorldMat");
  ShadowsModelAmbientNormalToWorldMatLoc = p_glGetUniformLocationARB(ShadowsModelAmbientProgram, "NormalToWorldMat");
  ShadowsModelAmbientVert2Loc = p_glGetAttribLocationARB(ShadowsModelAmbientProgram, "Vert2");
  ShadowsModelAmbientVertNormalLoc = p_glGetAttribLocationARB(ShadowsModelAmbientProgram, "VertNormal");
  ShadowsModelAmbientVert2NormalLoc = p_glGetAttribLocationARB(ShadowsModelAmbientProgram, "Vert2Normal");
  ShadowsModelAmbientTexCoordLoc = p_glGetAttribLocationARB(ShadowsModelAmbientProgram, "TexCoord");
  ShadowsModelAmbientAlphaLoc = p_glGetUniformLocationARB(ShadowsModelAmbientProgram, "InAlpha");
  ShadowsModelAmbientViewOrigin = p_glGetUniformLocationARB(ShadowsModelAmbientProgram, "ViewOrigin");
  ShadowsModelAmbientAllowTransparency = p_glGetUniformLocationARB(ShadowsModelAmbientProgram, "AllowTransparency");

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_model_textures.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_model_textures.fs");
  ShadowsModelTexturesProgram = CreateProgram(VertexShader, FragmentShader);
  ShadowsModelTexturesInterLoc = p_glGetUniformLocationARB(ShadowsModelTexturesProgram, "Inter");
  ShadowsModelTexturesTextureLoc = p_glGetUniformLocationARB(ShadowsModelTexturesProgram, "Texture");
  ShadowsModelTexturesModelToWorldMatLoc = p_glGetUniformLocationARB(ShadowsModelTexturesProgram, "ModelToWorldMat");
  ShadowsModelTexturesNormalToWorldMatLoc = p_glGetUniformLocationARB(ShadowsModelTexturesProgram, "NormalToWorldMat");
  ShadowsModelTexturesVert2Loc = p_glGetAttribLocationARB(ShadowsModelTexturesProgram, "Vert2");
  ShadowsModelTexturesTexCoordLoc = p_glGetAttribLocationARB(ShadowsModelTexturesProgram, "TexCoord");
  ShadowsModelTexturesVertNormalLoc = p_glGetAttribLocationARB(ShadowsModelTexturesProgram, "VertNormal");
  ShadowsModelTexturesVert2NormalLoc = p_glGetAttribLocationARB(ShadowsModelTexturesProgram, "Vert2Normal");
  ShadowsModelTexturesAlphaLoc = p_glGetUniformLocationARB(ShadowsModelTexturesProgram, "InAlpha");
  ShadowsModelTexturesViewOrigin = p_glGetUniformLocationARB(ShadowsModelTexturesProgram, "ViewOrigin");
  ShadowsModelTexturesAllowTransparency = p_glGetUniformLocationARB(ShadowsModelTexturesProgram, "AllowTransparency");

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_model_light.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_model_light.fs");
  ShadowsModelLightProgram = CreateProgram(VertexShader, FragmentShader);
  ShadowsModelLightInterLoc = p_glGetUniformLocationARB(ShadowsModelLightProgram, "Inter");
  ShadowsModelLightTextureLoc = p_glGetUniformLocationARB(ShadowsModelLightProgram, "Texture");
  ShadowsModelLightLightPosLoc = p_glGetUniformLocationARB(ShadowsModelLightProgram, "LightPos");
  ShadowsModelLightLightRadiusLoc = p_glGetUniformLocationARB(ShadowsModelLightProgram, "LightRadius");
  ShadowsModelLightLightColourLoc = p_glGetUniformLocationARB(ShadowsModelLightProgram, "LightColour");
  ShadowsModelLightModelToWorldMatLoc = p_glGetUniformLocationARB(ShadowsModelLightProgram, "ModelToWorldMat");
  ShadowsModelLightNormalToWorldMatLoc = p_glGetUniformLocationARB(ShadowsModelLightProgram, "NormalToWorldMat");
  ShadowsModelLightVert2Loc = p_glGetAttribLocationARB(ShadowsModelLightProgram, "Vert2");
  ShadowsModelLightVertNormalLoc = p_glGetAttribLocationARB(ShadowsModelLightProgram, "VertNormal");
  ShadowsModelLightVert2NormalLoc = p_glGetAttribLocationARB(ShadowsModelLightProgram, "Vert2Normal");
  ShadowsModelLightTexCoordLoc = p_glGetAttribLocationARB(ShadowsModelLightProgram, "TexCoord");
  ShadowsModelLightViewOrigin = p_glGetUniformLocationARB(ShadowsModelLightProgram, "ViewOrigin");
  ShadowsModelLightAllowTransparency = p_glGetUniformLocationARB(ShadowsModelLightProgram, "AllowTransparency");

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_model_shadow.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_model_shadow.fs");
  ShadowsModelShadowProgram = CreateProgram(VertexShader, FragmentShader);
  ShadowsModelShadowInterLoc = p_glGetUniformLocationARB(ShadowsModelShadowProgram, "Inter");
  ShadowsModelShadowLightPosLoc = p_glGetUniformLocationARB(ShadowsModelShadowProgram, "LightPos");
  ShadowsModelShadowModelToWorldMatLoc = p_glGetUniformLocationARB(ShadowsModelShadowProgram, "ModelToWorldMat");
  ShadowsModelShadowVert2Loc = p_glGetAttribLocationARB(ShadowsModelShadowProgram, "Vert2");
  ShadowsModelShadowOffsetLoc = p_glGetAttribLocationARB(ShadowsModelShadowProgram, "Offset");
  ShadowsModelShadowViewOrigin = p_glGetUniformLocationARB(ShadowsModelShadowProgram, "ViewOrigin");

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_fog.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_fog.fs");
  ShadowsFogProgram = CreateProgram(VertexShader, FragmentShader);
  ShadowsFogFogTypeLoc = p_glGetUniformLocationARB(ShadowsFogProgram, "FogType");
  ShadowsFogFogColourLoc = p_glGetUniformLocationARB(ShadowsFogProgram, "FogColour");
  ShadowsFogFogDensityLoc = p_glGetUniformLocationARB(ShadowsFogProgram, "FogDensity");
  ShadowsFogFogStartLoc = p_glGetUniformLocationARB(ShadowsFogProgram, "FogStart");
  ShadowsFogFogEndLoc = p_glGetUniformLocationARB(ShadowsFogProgram, "FogEnd");

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_model_fog.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_model_fog.fs");
  ShadowsModelFogProgram = CreateProgram(VertexShader, FragmentShader);
  ShadowsModelFogInterLoc = p_glGetUniformLocationARB(ShadowsModelFogProgram, "Inter");
  ShadowsModelFogModelToWorldMatLoc = p_glGetUniformLocationARB(ShadowsModelFogProgram, "ModelToWorldMat");
  ShadowsModelFogTextureLoc = p_glGetUniformLocationARB(ShadowsModelFogProgram, "Texture");
  ShadowsModelFogFogTypeLoc = p_glGetUniformLocationARB(ShadowsModelFogProgram, "FogType");
  ShadowsModelFogFogColourLoc = p_glGetUniformLocationARB(ShadowsModelFogProgram, "FogColour");
  ShadowsModelFogFogDensityLoc = p_glGetUniformLocationARB(ShadowsModelFogProgram, "FogDensity");
  ShadowsModelFogFogStartLoc = p_glGetUniformLocationARB(ShadowsModelFogProgram, "FogStart");
  ShadowsModelFogFogEndLoc = p_glGetUniformLocationARB(ShadowsModelFogProgram, "FogEnd");
  ShadowsModelFogVert2Loc = p_glGetAttribLocationARB(ShadowsModelFogProgram, "Vert2");
  ShadowsModelFogTexCoordLoc = p_glGetAttribLocationARB(ShadowsModelFogProgram, "TexCoord");
  ShadowsModelFogAlphaLoc = p_glGetUniformLocationARB(ShadowsModelFogProgram, "InAlpha");
  ShadowsModelFogViewOrigin = p_glGetUniformLocationARB(ShadowsModelFogProgram, "ViewOrigin");
  ShadowsModelFogAllowTransparency = p_glGetUniformLocationARB(ShadowsModelFogProgram, "AllowTransparency");

  mInitialized = true;

  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::CheckExtension
//
//==========================================================================
bool VOpenGLDrawer::CheckExtension (const char *ext) {
  guard(VOpenGLDrawer::CheckExtension);
  if (!ext || !ext[0]) return false;
  TArray<VStr> Exts;
  VStr((char*)glGetString(GL_EXTENSIONS)).Split(' ', Exts);
  for (int i = 0; i < Exts.Num(); ++i) if (Exts[i] == ext) return true;
  return false;
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::CheckGLXExtension
//
//==========================================================================
typedef const char *(APIENTRY *glXQueryExtensionsString)(void *dpy, int screen);
typedef void *(APIENTRY *glXGetCurrentDisplay) ();
typedef long (APIENTRY *glXGetCurrentDrawable) ();

bool VOpenGLDrawer::CheckGLXExtension (const char *ext) {
  guard(VOpenGLDrawer::CheckGLXExtension);
  if (!ext || !ext[0]) return false;

  glXQueryExtensionsString gextstr = (glXQueryExtensionsString)GetExtFuncPtr("glXQueryExtensionsString");
  if (!gextstr) return false;
  glXGetCurrentDisplay getdpy = (glXGetCurrentDisplay)GetExtFuncPtr("glXGetCurrentDisplay");
  if (!getdpy) return false;
  if (!getdpy()) return false;
  glXGetCurrentDrawable getdrw = (glXGetCurrentDrawable)GetExtFuncPtr("glXGetCurrentDrawable");
  if (!getdrw) return false;
  if (!getdrw()) return false;

  /*
  GCon->Logf("dpy is %p", getdpy());
  GCon->Logf("drw is %d", (int)getdrw());
  GCon->Logf("extlist is %s", (int)gextstr(getdpy(), 0));
  */

  TArray<VStr> Exts;
  VStr((char *)(gextstr(getdpy(), 0))).Split(' ', Exts);
  //for (int i = 0; i < Exts.Num(); ++i) GCon->Logf("GLX EXTENSION: %s", *Exts[i]);
  for (int i = 0; i < Exts.Num(); ++i) if (Exts[i] == ext) return true;
  return false;
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::SupportsAdvancedRendering
//
//==========================================================================
bool VOpenGLDrawer::SupportsAdvancedRendering () {
  return HaveStencilWrap && p_glStencilFuncSeparate && HaveDrawRangeElements;
}


//==========================================================================
//
//  VOpenGLDrawer::Setup2D
//
//==========================================================================
void VOpenGLDrawer::Setup2D () {
  guard(VOpenGLDrawer::Setup2D);
  glViewport(0, 0, ScreenWidth, ScreenHeight);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, ScreenWidth, ScreenHeight, 0, -99999, 99999);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  if (HaveDepthClamp) glDisable(GL_DEPTH_CLAMP);
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::StartUpdate
//
//==========================================================================
void VOpenGLDrawer::StartUpdate (bool allowClear) {
  guard(VOpenGLDrawer::StartUpdate);
  glFinish();

  VRenderLevelShared::ResetPortalPool();

  if (mainFBO) glBindFramebuffer(GL_FRAMEBUFFER, mainFBO);

  if (allowClear && clear) glClear(GL_COLOR_BUFFER_BIT);

  glBindTexture(GL_TEXTURE_2D, 0);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  // turn off anisotropy
  glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), 1); // 1 is minimum, i.e. "off"

  if (usegamma != lastgamma) {
    FlushTextures();
    lastgamma = usegamma;
  }

  Setup2D();
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::FinishUpdate
//
//==========================================================================
void VOpenGLDrawer::FinishUpdate () {
  if (mainFBO) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, mainFBOColorTid);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glColor4f(1.0, 1.0, 1.0, 1.0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_TEXTURE_2D);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    p_glUseProgramObjectARB(0);

    // copy texture by drawing full quad
    glBegin(GL_QUADS);
      glTexCoord2f(0.0f, 1.0f); glVertex2i(0, 0);
      glTexCoord2f(1.0f, 1.0f); glVertex2i(ScreenWidth, 0);
      glTexCoord2f(1.0f, 0.0f); glVertex2i(ScreenWidth, ScreenHeight);
      glTexCoord2f(0.0f, 0.0f); glVertex2i(0, ScreenHeight);
    glEnd();

    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFlush();
  }
}


//==========================================================================
//
//  VOpenGLDrawer::BeginDirectUpdate
//
//==========================================================================
void VOpenGLDrawer::BeginDirectUpdate () {
  guard(VOpenGLDrawer::BeginDirectUpdate);
  glFinish();
  glDrawBuffer(GL_FRONT);
  if (mainFBO) glBindFramebuffer(GL_FRAMEBUFFER, 0);
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::EndDirectUpdate
//
//==========================================================================
void VOpenGLDrawer::EndDirectUpdate () {
  guard(VOpenGLDrawer::EndDirectUpdate);
  glDrawBuffer(GL_BACK);
  if (mainFBO) glBindFramebuffer(GL_FRAMEBUFFER, mainFBO);
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::SetupView
//
//==========================================================================
void VOpenGLDrawer::SetupView (VRenderLevelDrawer *ARLev, const refdef_t *rd) {
  guard(VOpenGLDrawer::SetupView);
  RendLev = ARLev;

  if (!rd->DrawCamera && rd->drawworld && rd->width != ScreenWidth) {
    // draws the border around the view for different size windows
    R_DrawViewBorder();
  }

  VMatrix4 ProjMat = VMatrix4::Identity;
  if (!CanUseRevZ()) {
    // normal
    glClearDepth(1.0f);
    glDepthFunc(GL_LEQUAL);
    ProjMat[0][0] = 1.0/rd->fovx;
    ProjMat[1][1] = 1.0/rd->fovy;
    ProjMat[2][3] = -1.0;
    ProjMat[3][3] = 0.0;
    if (RendLev && RendLev->NeedsInfiniteFarClip && !HaveDepthClamp) {
      ProjMat[2][2] = -1.0;
      ProjMat[3][2] = -2.0;
    } else {
      ProjMat[2][2] = -(maxdist + 1.0) / (maxdist - 1.0);
      ProjMat[3][2] = -2.0 * maxdist / (maxdist - 1.0);
    }
  } else {
    // reversed
    //GCon->Logf("!!!REV!!!");
    glClearDepth(0.0f);
    glDepthFunc(GL_GEQUAL);
    for (int f = 0; f < 4; ++f) for (int c = 0; c < 4; ++c) ProjMat.m[f][c] = 0;
    ProjMat[0][0] = 1.0/rd->fovx;
    ProjMat[1][1] = 1.0/rd->fovy;
    ProjMat[2][3] = -1.0;
    ProjMat[3][2] = 0.001;
  }
  //RestoreDepthFunc();

  glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

  glViewport(rd->x, ScreenHeight - rd->height - rd->y, rd->width, rd->height);

  glMatrixMode(GL_PROJECTION);    // Select The Projection Matrix
  glLoadMatrixf(ProjMat[0]);

  glMatrixMode(GL_MODELVIEW);     // Select The Modelview Matrix

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_FRONT);

  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_ALPHA_TEST);
  if (RendLev && RendLev->NeedsInfiniteFarClip && HaveDepthClamp) {
    glEnable(GL_DEPTH_CLAMP);
  }

  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::SetupViewOrg
//
//==========================================================================
void VOpenGLDrawer::SetupViewOrg () {
  guard(VOpenGLDrawer::SetupViewOrg);
  glLoadIdentity();
  glRotatef(-90, 1, 0, 0);
  glRotatef(90, 0, 0, 1);
  if (MirrorFlip) {
    glScalef(1, -1, 1);
    glCullFace(GL_BACK);
  } else {
    glCullFace(GL_FRONT);
  }
  glRotatef(-viewangles.roll, 1, 0, 0);
  glRotatef(-viewangles.pitch, 0, 1, 0);
  glRotatef(-viewangles.yaw, 0, 0, 1);
  glTranslatef(-vieworg.x, -vieworg.y, -vieworg.z);

  if (MirrorClip) {
    glEnable(GL_CLIP_PLANE0);
    GLdouble eq[4] = { view_clipplanes[4].normal.x,
      view_clipplanes[4].normal.y, view_clipplanes[4].normal.z,
      -view_clipplanes[4].dist };
    glClipPlane(GL_CLIP_PLANE0, eq);
  } else {
    glDisable(GL_CLIP_PLANE0);
  }

  if (RendLev) {
    memset(RendLev->light_chain, 0, sizeof(RendLev->light_chain));
    memset(RendLev->add_chain, 0, sizeof(RendLev->add_chain));
    RendLev->SimpleSurfsHead = nullptr;
    RendLev->SimpleSurfsTail = nullptr;
    RendLev->SkyPortalsHead = nullptr;
    RendLev->SkyPortalsTail = nullptr;
    RendLev->HorizonPortalsHead = nullptr;
    RendLev->HorizonPortalsTail = nullptr;
  }
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::EndView
//
//==========================================================================
void VOpenGLDrawer::EndView () {
  guard(VOpenGLDrawer::EndView);
  Setup2D();

  if (cl && cl->CShift) {
    p_glUseProgramObjectARB(DrawFixedColProgram);
    p_glUniform4fARB(DrawFixedColColourLoc,
      (float)((cl->CShift >> 16) & 255) / 255.0,
      (float)((cl->CShift >> 8) & 255) / 255.0,
      (float)(cl->CShift & 255) / 255.0,
      (float)((cl->CShift >> 24) & 255) / 255.0);
    glEnable(GL_BLEND);

    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(ScreenWidth, 0);
    glVertex2f(ScreenWidth, ScreenHeight);
    glVertex2f(0, ScreenHeight);
    glEnd();

    glDisable(GL_BLEND);
  }
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::ReadScreen
//
//==========================================================================
void *VOpenGLDrawer::ReadScreen (int *bpp, bool *bot2top) {
  guard(VOpenGLDrawer::ReadScreen);
  if (mainFBO == 0) {
    void *dst = Z_Malloc(ScreenWidth*ScreenHeight*3);
    glReadBuffer(GL_FRONT);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glReadPixels(0, 0, ScreenWidth, ScreenHeight, GL_RGB, GL_UNSIGNED_BYTE, dst);
    *bpp = 24;
    *bot2top = true;
    return dst;
  } else {
    glBindTexture(GL_TEXTURE_2D, mainFBOColorTid);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    void *dst = Z_Malloc(ScreenWidth*ScreenHeight*3);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, dst);
    glBindTexture(GL_TEXTURE_2D, 0);
    *bpp = 24;
    *bot2top = true;
    return dst;
  }
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::ReadBackScreen
//
//==========================================================================
void VOpenGLDrawer::ReadBackScreen (int Width, int Height, rgba_t *Dest) {
  guard(VOpenGLDrawer::ReadBackScreen);

  if (readBackTempBufSize < ScreenWidth*ScreenHeight*4) {
    readBackTempBufSize = ScreenWidth*ScreenHeight*4;
    readBackTempBuf = (vuint8 *)Z_Realloc(readBackTempBuf, readBackTempBufSize);
  }

  if (mainFBO == 0) {
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glReadPixels(0, ScreenHeight-Height, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, Dest);
    //rgba_t *Temp = new rgba_t[Width];
    rgba_t *Temp = (rgba_t *)readBackTempBuf;
    for (int i = 0; i < Height/2; ++i) {
      memcpy(Temp, Dest+i*Width, Width*sizeof(rgba_t));
      memcpy(Dest+i*Width, Dest+(Height-1-i)*Width, Width*sizeof(rgba_t));
      memcpy(Dest+(Height-1-i)*Width, Temp, Width*sizeof(rgba_t));
    }
    //delete[] Temp;
    //Temp = nullptr;
  } else {
    glBindTexture(GL_TEXTURE_2D, mainFBOColorTid);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    //rgba_t *temp = new rgba_t[ScreenWidth*ScreenHeight];
    rgba_t *temp = (rgba_t *)readBackTempBuf;
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, temp);
    glBindTexture(GL_TEXTURE_2D, 0);
    for (int y = 0; y < Height; ++y) {
      memcpy(Dest+y*Width, temp+(ScreenHeight-y-1)*ScreenWidth, Width*sizeof(rgba_t));
    }
    //delete[] temp;
  }
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::SetFade
//
//==========================================================================
void VOpenGLDrawer::SetFade (vuint32 NewFade) {
  guard(VOpenGLDrawer::SetFade);
  if ((vuint32)CurrentFade == NewFade) return;

  if (NewFade) {
    static GLenum fogMode[4] = { GL_LINEAR, GL_LINEAR, GL_EXP, GL_EXP2 };
    float fogColour[4];

    fogColour[0] = float((NewFade >> 16) & 255) / 255.0;
    fogColour[1] = float((NewFade >> 8) & 255) / 255.0;
    fogColour[2] = float(NewFade & 255) / 255.0;
    fogColour[3] = float((NewFade >> 24) & 255) / 255.0;
    glFogi(GL_FOG_MODE, fogMode[r_fog & 3]);
    glFogfv(GL_FOG_COLOR, fogColour);
    if (NewFade == FADE_LIGHT) {
      glFogf(GL_FOG_DENSITY, 0.3);
      glFogf(GL_FOG_START, 1.0);
      glFogf(GL_FOG_END, 1024.0*r_fade_factor);
    } else {
      glFogf(GL_FOG_DENSITY, r_fog_density);
      glFogf(GL_FOG_START, r_fog_start);
      glFogf(GL_FOG_END, r_fog_end);
    }
    glHint(GL_FOG_HINT, r_fog < 4 ? GL_DONT_CARE : GL_NICEST);
    glEnable(GL_FOG);
  } else {
    glDisable(GL_FOG);
  }
  CurrentFade = NewFade;
  unguard;
}


//==========================================================================
//
//  readTextFile
//
//==========================================================================
static VStr readTextFile (const VStr &fname) {
  VStream *strm = FL_OpenFileRead(fname);
  if (!strm) Sys_Error("Failed to open shader '%s'", *fname);
  int size = strm->TotalSize();
  if (size == 0) return VStr();
  VStr res;
  res.setLength(size, 0);
  strm->Serialise(res.GetMutableCharPointer(0), size);
  delete strm;
  return res;
}


//==========================================================================
//
//  isEmptyLine
//
//==========================================================================
static bool isEmptyLine (const VStr &s) {
  int pos = 0;
  while (pos < s.length()) {
    if ((vuint8)s[pos] > ' ') return false;
    ++pos;
  }
  return true;
}


//==========================================================================
//
//  isCommentLine
//
//==========================================================================
static bool isCommentLine (const VStr &s) {
  if (s.length() < 2) return false;
  int pos = 0;
  while (pos+1 < s.length()) {
    if (s[pos] == '/' && s[pos+1] == '/') return true;
    if ((vuint8)s[pos] > ' ') return false;
    ++pos;
  }
  return false;
}


//==========================================================================
//
//  isVersionLine
//
//==========================================================================
static bool isVersionLine (const VStr &s) {
  if (s.length() < 2) return false;
  int pos = 0;
  while (pos < s.length()) {
    if ((vuint8)s[pos] == '#') {
      ++pos;
      while (pos < s.length() && (vuint8)s[pos] <= ' ') ++pos;
      if (pos >= s.length()) return false;
      if (s[pos+0] == 'v' && s[pos+1] == 'e' && s[pos+2] == 'r' && s[pos+3] == 's' &&
          s[pos+4] == 'i' && s[pos+5] == 'o' && s[pos+6] == 'n') return true;
      return false;
    }
    if ((vuint8)s[pos] > ' ') return false;
    ++pos;
  }
  return false;
}


//==========================================================================
//
//  getDirective
//
//==========================================================================
static VStr getDirective (const VStr &s) {
  int pos = 0;
  while (pos+1 < s.length()) {
    if (s[pos] == '$') {
      ++pos;
      while (pos < s.length() && (vuint8)s[pos] <= ' ') ++pos;
      if (pos >= s.length()) return VStr("$");
      int start = pos;
      while (pos < s.length() && (vuint8)s[pos] > ' ') ++pos;
      return s.mid(start, pos-start);
    }
    if ((vuint8)s[pos] > ' ') return VStr();
    ++pos;
  }
  return VStr();
}


//==========================================================================
//
//  getDirectiveArg
//
//==========================================================================
static VStr getDirectiveArg (const VStr &s) {
  int pos = 0;
  while (pos+1 < s.length()) {
    if (s[pos] == '$') {
      ++pos;
      while (pos < s.length() && (vuint8)s[pos] <= ' ') ++pos;
      if (pos >= s.length()) return VStr();
      while (pos < s.length() && (vuint8)s[pos] > ' ') ++pos;
      while (pos < s.length() && (vuint8)s[pos] <= ' ') ++pos;
      if (pos >= s.length()) return VStr();
      if (s[pos] == '"') {
        int start = ++pos;
        while (pos < s.length() && s[pos] != '"') ++pos;
        return s.mid(start, pos-start);
      } else {
        int start = pos;
        while (pos < s.length() && (vuint8)s[pos] > ' ') ++pos;
        return s.mid(start, pos-start);
      }
    }
    if ((vuint8)s[pos] > ' ') return VStr();
    ++pos;
  }
  return VStr();
}


//==========================================================================
//
//  VOpenGLDrawer::LoadShader
//
//==========================================================================
GLhandleARB VOpenGLDrawer::LoadShader (GLenum Type, const VStr &FileName) {
  guard(VOpenGLDrawer::LoadShader);
  // create shader object
  GLhandleARB Shader = p_glCreateShaderObjectARB(Type);
  if (!Shader) Sys_Error("Failed to create shader object");
  CreatedShaderObjects.Append(Shader);

  // load source file
  VStr ssrc = readTextFile(FileName);

  // build source text
  bool needToAddRevZ = CanUseRevZ();
  /*
  if (CanUseRevZ()) {
    if (ssrc.length() && ssrc[0] == '#') {
      // skip first line (this should be "#version")
      int epos = 0;
      while (epos < ssrc.length() && ssrc[epos] != '\n') ++epos;
      if (epos < ssrc.length()) ++epos; // skip eol
      VStr ns = ssrc.mid(0, epos);
      ns += "#define VAVOOM_REVERSE_Z\n";
      ssrc.chopLeft(epos);
      ns += ssrc;
      ssrc = ns;
    } else {
      VStr ns = "#define VAVOOM_REVERSE_Z\n";
      ns += ssrc;
      ssrc = ns;
    }
  }
  */

  // process $include
  //FIXME: nested "$include", and proper directive parsing
  VStr res;
  while (ssrc.length()) {
    // find line end
    int epos = 0;
    while (epos < ssrc.length() && ssrc[epos] != '\n') ++epos;
    if (epos < ssrc.length()) ++epos; // skip "\n"
    // extract line
    VStr line = ssrc.left(epos);
    ssrc.chopLeft(epos);
    if (isEmptyLine(line)) { res += line; continue; }
    if (isCommentLine(line)) { res += line; continue; }
    // add "reverse z" define
    VStr cmd = getDirective(line);
    if (cmd.length() == 0) {
      if (needToAddRevZ) {
        if (isVersionLine(line)) { res += line; continue; }
        res += "#define VAVOOM_REVERSE_Z\n";
        needToAddRevZ = false;
      }
      res += line;
      continue;
    }
    if (cmd != "include") Sys_Error("%s", va("invalid directive \"%s\" in shader '%s'", *cmd, *FileName));
    VStr fname = getDirectiveArg(line);
    if (fname.length() == 0) Sys_Error("%s", va("directive \"%s\" in shader '%s' expects file name", *cmd, *FileName));
    VStr incf = readTextFile(FileName.extractFilePath()+fname);
    if (incf.length() && incf[incf.length()-1] != '\n') incf += '\n';
    incf += ssrc;
    ssrc = incf;
  }
  //fprintf(stderr, "================ %s ================\n%s\n=================================\n", *FileName, *res);
  //vsShaderSrc = res;

  // upload source text
  const GLcharARB *ShaderText = *res;
  p_glShaderSourceARB(Shader, 1, &ShaderText, nullptr);

  // compile it
  p_glCompileShaderARB(Shader);

  // check id it is compiled successfully
  GLint Status;
  p_glGetObjectParameterivARB(Shader, GL_OBJECT_COMPILE_STATUS_ARB, &Status);
  if (!Status) {
    GLcharARB LogText[1024];
    GLsizei LogLen;
    p_glGetInfoLogARB(Shader, sizeof(LogText)-1, &LogLen, LogText);
    LogText[LogLen] = 0;
    fprintf(stderr, "================ %s ================\n%s\n=================================\n%s\b", *FileName, *res, LogText);
    Sys_Error("%s", va("Failed to compile shader %s: %s", *FileName, LogText));
  }
  return Shader;
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::CreateProgram
//
//==========================================================================
GLhandleARB VOpenGLDrawer::CreateProgram (GLhandleARB VertexShader, GLhandleARB FragmentShader) {
  guard(VOpenGLDrawer::CreateProgram);
  // create program object
  GLhandleARB Program = p_glCreateProgramObjectARB();
  if (!Program) Sys_Error("Failed to create program object");
  CreatedShaderObjects.Append(Program);

  // attach shaders
  p_glAttachObjectARB(Program, VertexShader);
  p_glAttachObjectARB(Program, FragmentShader);

  // link program
  p_glLinkProgramARB(Program);

  // check if it was linked successfully
  GLint Status;
  p_glGetObjectParameterivARB(Program, GL_OBJECT_LINK_STATUS_ARB, &Status);
  if (!Status) {
    GLcharARB LogText[1024];
    GLsizei LogLen;
    p_glGetInfoLogARB(Program, sizeof(LogText)-1, &LogLen, LogText);
    LogText[LogLen] = 0;
    Sys_Error("Failed to link program %s", LogText);
  }
  return Program;
  unguard;
}
