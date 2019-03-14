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
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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
#include "gl_local.h"
#include "../r_local.h"


// ////////////////////////////////////////////////////////////////////////// //
VCvarB gl_pic_filtering("gl_pic_filtering", false, "Filter interface pictures.", CVAR_Archive);
VCvarB gl_font_filtering("gl_font_filtering", false, "Filter 2D interface.", CVAR_Archive);

static VCvarB gl_enable_floating_zbuffer("gl_enable_floating_zbuffer", true, "Enable using of floating-point depth buffer for OpenGL3+?", CVAR_Archive|CVAR_PreInit);
static VCvarB gl_disable_reverse_z("gl_disable_reverse_z", false, "Completely disable reverse z, even if it is available?", CVAR_Archive|CVAR_PreInit);
static VCvarB gl_dbg_force_reverse_z("gl_dbg_force_reverse_z", false, "Force-enable reverse z when fp depth buffer is not available.", CVAR_PreInit);
static VCvarB gl_dbg_ignore_gpu_blacklist("gl_dbg_ignore_gpu_blacklist", false, "Ignore GPU blacklist, and don't turn off features?", CVAR_PreInit);
static VCvarB gl_dbg_force_gpu_blacklisting("gl_dbg_force_gpu_blacklisting", false, "Force GPU to be blacklisted.", CVAR_PreInit);

VCvarI VOpenGLDrawer::texture_filter("gl_texture_filter", "0", "Texture interpolation mode.", CVAR_Archive);
VCvarI VOpenGLDrawer::sprite_filter("gl_sprite_filter", "0", "Sprite interpolation mode.", CVAR_Archive);
VCvarI VOpenGLDrawer::model_filter("gl_model_filter", "3", "Model interpolation mode.", CVAR_Archive);
VCvarI VOpenGLDrawer::gl_texture_filter_anisotropic("gl_texture_filter_anisotropic", "1", "Texture anisotropic filtering (<=1 is off).", CVAR_Archive);
VCvarB VOpenGLDrawer::clear("gl_clear", true, "Clear screen before rendering new frame?", CVAR_Archive);
VCvarB VOpenGLDrawer::blend_sprites("gl_blend_sprites", false, "Alpha-blend sprites?", CVAR_Archive);
VCvarB VOpenGLDrawer::ext_anisotropy("gl_ext_anisotropy", true, "Use OpenGL anisotropy extension (if present)?", CVAR_Archive|CVAR_PreInit);
VCvarF VOpenGLDrawer::maxdist("gl_maxdist", "8192", "Max view distance (too big values will cause z-buffer issues).", CVAR_Archive);
//VCvarB VOpenGLDrawer::model_lighting("gl_model_lighting", true, "Light models?", CVAR_Archive); //k8: this doesn't work with shaders, alas
VCvarB VOpenGLDrawer::specular_highlights("gl_specular_highlights", true, "Specular highlights type.", CVAR_Archive);
VCvarI VOpenGLDrawer::multisampling_sample("gl_multisampling_sample", "1", "Multisampling mode.", CVAR_Archive);
VCvarB VOpenGLDrawer::gl_smooth_particles("gl_smooth_particles", false, "Draw smooth particles?", CVAR_Archive);

VCvarB VOpenGLDrawer::gl_dump_vendor("gl_dump_vendor", false, "Dump OpenGL vendor?", CVAR_PreInit);
VCvarB VOpenGLDrawer::gl_dump_extensions("gl_dump_extensions", false, "Dump available OpenGL extensions?", CVAR_PreInit);

// was 0.333
VCvarF gl_alpha_threshold("gl_alpha_threshold", "0.15", "Alpha threshold (less than this will not be drawn).", CVAR_Archive);

static VCvarI gl_max_anisotropy("gl_max_anisotropy", "1", "Maximum anisotropy level (r/o).", CVAR_Rom);
static VCvarB gl_is_shitty_gpu("gl_is_shitty_gpu", true, "Is shitty GPU detected (r/o)?", CVAR_Rom);

static VCvarB gl_enable_depth_bounds("gl_enable_depth_bounds", false, "Use depth bounds extension if found?", CVAR_Archive);
static VCvarB gl_use_optimised_scissor_calculation("gl_use_optimised_scissor_calculation", true, "Use optimised shadow volume scissor calculation?", CVAR_Archive);


//==========================================================================
//
//  CheckVendorString
//
//  both strings should be lower-cased
//  `vs` is what we got from OpenGL
//  `fuckedName` is what we are looking for
//
//==========================================================================
static bool CheckVendorString (VStr vs, const char *fuckedName) {
  if (vs.length() == 0) return false;
  if (!fuckedName || !fuckedName[0]) return false;
  const int fnlen = (int)strlen(fuckedName);
  //GCon->Logf(NAME_Init, "VENDOR: <%s>", *vs);
  while (vs.length()) {
    auto idx = vs.indexOf(fuckedName);
    if (idx < 0) break;
    bool startok = (idx == 0 || !VStr::isAlphaAscii(vs[idx-1]));
    bool endok = (idx+fnlen >= vs.length() || !VStr::isAlphaAscii(vs[idx+fnlen]));
    if (startok && endok) return true;
    vs.chopLeft(idx+fnlen);
    //GCon->Logf(NAME_Init, "  XXX: <%s>", *vs);
  }
  return false;
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShaderCommonLocs::setupProg
//
//  call this first!
//
//==========================================================================
void VOpenGLDrawer::VGLShaderCommonLocs::setupProg (VOpenGLDrawer *aowner, GLhandleARB aprog) {
  //check(!owner);
  //check(!prog);
  check(aowner);
  check(aprog);
  owner = aowner;
  prog = aprog;
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShaderCommonLocs::setupNormal
//
//  setup texture variables
//
//==========================================================================
void VOpenGLDrawer::VGLShaderCommonLocs::setupTexture () {
  check(prog);
  locSAxis = owner->p_glGetUniformLocationARB(prog, "SAxis");
  locTAxis = owner->p_glGetUniformLocationARB(prog, "TAxis");
  locSOffs = owner->p_glGetUniformLocationARB(prog, "SOffs");
  locTOffs = owner->p_glGetUniformLocationARB(prog, "TOffs");
  locTexIW = owner->p_glGetUniformLocationARB(prog, "TexIW");
  locTexIH = owner->p_glGetUniformLocationARB(prog, "TexIH");
  locTexture = owner->p_glGetUniformLocationARB(prog, "Texture");
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShaderCommonLocs::setupLMapOnly
//
//  setup lightmap variables for shader without texture params (decals)
//
//==========================================================================
void VOpenGLDrawer::VGLShaderCommonLocs::setupLMapOnly () {
  locSAxis = owner->p_glGetUniformLocationARB(prog, "SAxis");
  locTAxis = owner->p_glGetUniformLocationARB(prog, "TAxis");
  locSOffs = owner->p_glGetUniformLocationARB(prog, "SOffs");
  locTOffs = owner->p_glGetUniformLocationARB(prog, "TOffs");
  locTexMinS = owner->p_glGetUniformLocationARB(prog, "TexMinS");
  locTexMinT = owner->p_glGetUniformLocationARB(prog, "TexMinT");
  locCacheS = owner->p_glGetUniformLocationARB(prog, "CacheS");
  locCacheT = owner->p_glGetUniformLocationARB(prog, "CacheT");
  locLightMap = owner->p_glGetUniformLocationARB(prog, "LightMap");
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShaderCommonLocs::setupLMap
//
//  setup lightmap variables
//
//==========================================================================
void VOpenGLDrawer::VGLShaderCommonLocs::setupLMap () {
  check(prog);
  locTexMinS = owner->p_glGetUniformLocationARB(prog, "TexMinS");
  locTexMinT = owner->p_glGetUniformLocationARB(prog, "TexMinT");
  locCacheS = owner->p_glGetUniformLocationARB(prog, "CacheS");
  locCacheT = owner->p_glGetUniformLocationARB(prog, "CacheT");
  locLightMap = owner->p_glGetUniformLocationARB(prog, "LightMap");
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShaderCommonLocs::setupFog
//
//  setup fog variables
//
//==========================================================================
void VOpenGLDrawer::VGLShaderCommonLocs::setupFog () {
  check(prog);
  locFogEnabled = owner->p_glGetUniformLocationARB(prog, "FogEnabled");
  locFogType = owner->p_glGetUniformLocationARB(prog, "FogType");
  locFogColour = owner->p_glGetUniformLocationARB(prog, "FogColour");
  locFogDensity = owner->p_glGetUniformLocationARB(prog, "FogDensity");
  locFogStart = owner->p_glGetUniformLocationARB(prog, "FogStart");
  locFogEnd = owner->p_glGetUniformLocationARB(prog, "FogEnd");
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShaderCommonLocs::storeTexture
//
//==========================================================================
void VOpenGLDrawer::VGLShaderCommonLocs::storeTexture (GLint tid) {
  check(prog);
  owner->p_glUniform1iARB(locTexture, tid);
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShaderCommonLocs::storeTextureParams
//
//  `SetTexture()` must be called! it sets `tex_iw` and `tex_ih`
//
//==========================================================================
void VOpenGLDrawer::VGLShaderCommonLocs::storeTextureParams (const texinfo_t *textr) {
  check(prog);
  check(textr);
  owner->p_glUniform3fvARB(locSAxis, 1, &textr->saxis.x);
  owner->p_glUniform1fARB(locSOffs, textr->soffs);
  owner->p_glUniform1fARB(locTexIW, owner->tex_iw);
  owner->p_glUniform3fvARB(locTAxis, 1, &textr->taxis.x);
  owner->p_glUniform1fARB(locTOffs, textr->toffs);
  owner->p_glUniform1fARB(locTexIH, owner->tex_ih);
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShaderCommonLocs::storeLMap
//
//==========================================================================
void VOpenGLDrawer::VGLShaderCommonLocs::storeLMap (GLint tid) {
  check(prog);
  owner->p_glUniform1iARB(locLightMap, tid);
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShaderCommonLocs::storeLMapParams
//
//==========================================================================
void VOpenGLDrawer::VGLShaderCommonLocs::storeLMapParams (const surface_t *surf, const surfcache_t *cache) {
  check(prog);
  check(surf);
  check(cache);
  owner->p_glUniform1fARB(locTexMinS, surf->texturemins[0]);
  owner->p_glUniform1fARB(locTexMinT, surf->texturemins[1]);
  owner->p_glUniform1fARB(locCacheS, cache->s);
  owner->p_glUniform1fARB(locCacheT, cache->t);
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShaderCommonLocs::storeLMapOnlyParams
//
//==========================================================================
void VOpenGLDrawer::VGLShaderCommonLocs::storeLMapOnlyParams (const texinfo_t *textr, const surface_t *surf, const surfcache_t *cache) {
  check(prog);
  check(surf);
  check(cache);
  owner->p_glUniform3fvARB(locSAxis, 1, &textr->saxis.x);
  owner->p_glUniform1fARB(locSOffs, textr->soffs);
  owner->p_glUniform3fvARB(locTAxis, 1, &textr->taxis.x);
  owner->p_glUniform1fARB(locTOffs, textr->toffs);
  owner->p_glUniform1fARB(locTexMinS, surf->texturemins[0]);
  owner->p_glUniform1fARB(locTexMinT, surf->texturemins[1]);
  owner->p_glUniform1fARB(locCacheS, cache->s);
  owner->p_glUniform1fARB(locCacheT, cache->t);
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShaderCommonLocs::storeTextureLMapParams
//
//  `SetTexture()` must be called! it sets `tex_iw` and `tex_ih`
//
//==========================================================================
void VOpenGLDrawer::VGLShaderCommonLocs::storeTextureLMapParams (const texinfo_t *textr, const surface_t *surf, const surfcache_t *cache) {
  storeTextureParams(textr);
  storeLMapParams(surf, cache);
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShaderCommonLocs::storeFogType
//
//==========================================================================
void VOpenGLDrawer::VGLShaderCommonLocs::storeFogType () {
  check(prog);
  owner->p_glUniform1iARB(locFogType, r_fog&3);
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShaderCommonLocs::storeFogFade
//
//==========================================================================
void VOpenGLDrawer::VGLShaderCommonLocs::storeFogFade (vuint32 Fade, float Alpha) {
  check(prog);
  if (Fade) {
    owner->p_glUniform1iARB(locFogEnabled, GL_TRUE);
    owner->p_glUniform4fARB(locFogColour,
      ((Fade>>16)&255)/255.0f,
      ((Fade>>8)&255)/255.0f,
      (Fade&255)/255.0f, Alpha);
    owner->p_glUniform1fARB(locFogDensity, Fade == FADE_LIGHT ? 0.3f : r_fog_density);
    owner->p_glUniform1fARB(locFogStart, Fade == FADE_LIGHT ? 1.0f : r_fog_start);
    owner->p_glUniform1fARB(locFogEnd, Fade == FADE_LIGHT ? 1024.0f*r_fade_factor : r_fog_end);
  } else {
    owner->p_glUniform1iARB(locFogEnabled, GL_FALSE);
  }
}



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
  useReverseZ = false;
  hasNPOT = false;
  hasBoundsTest = false;

  mainFBO = 0;
  mainFBOColorTid = 0;
  mainFBODepthStencilTid = 0;

  secondFBO = 0;
  secondFBOColorTid = 0;

  ambLightFBO = 0;
  ambLightFBOColorTid = 0;

  tmpImgBuf0 = nullptr;
  tmpImgBuf1 = nullptr;
  tmpImgBufSize = 0;

  readBackTempBuf = nullptr;
  readBackTempBufSize = 0;

  decalUsedStencil = false;
  decalStcVal = 255; // next value for stencil buffer (clear on the first use, and clear on each wrap)
  maskedDecalsStarted = false;
  stencilBufferDirty = true; // just in case
  isShittyGPU = true; // let's play safe
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
  glDepthFunc(!CanUseRevZ() ? GL_LEQUAL : GL_GEQUAL);
}


//==========================================================================
//
//  VOpenGLDrawer::RestoreDepthFunc
//
//==========================================================================
void VOpenGLDrawer::SetupTextureFiltering (int level) {
  // for anisotropy, we require trilinear filtering
  if (anisotropyExists) {
    /*
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
    //glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), 1); // 1 is minimum, i.e. "off"
    */
    // but newer OpenGL versions allows anisotropy filtering even for "nearest" mode,
    // so setup it in any case
    glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT),
      (gl_texture_filter_anisotropic > max_anisotropy ? max_anisotropy : gl_texture_filter_anisotropic)
    );
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
  GCon->Logf(NAME_Init, "Setting up new resolution: %dx%d", ScreenWidth, ScreenHeight);

  if (gl_dump_vendor) {
    GCon->Logf(NAME_Init, "GL_VENDOR: %s", glGetString(GL_VENDOR));
    GCon->Logf(NAME_Init, "GL_RENDERER: %s", glGetString(GL_RENDERER));
    GCon->Logf(NAME_Init, "GL_VERSION: %s", glGetString(GL_VERSION));
  }

  if (gl_dump_extensions) {
    GCon->Log(NAME_Init, "GL_EXTENSIONS:");
    TArray<VStr> Exts;
    VStr((char *)glGetString(GL_EXTENSIONS)).Split(' ', Exts);
    for (int i = 0; i < Exts.Num(); ++i) GCon->Log(NAME_Init, VStr("- ")+Exts[i]);
  }

  isShittyGPU = false;
  {
    const char *vcstr = (const char *)glGetString(GL_VENDOR);
    VStr vs = VStr(vcstr).toLowerCase();
    isShittyGPU = CheckVendorString(vs, "intel");
    if (isShittyGPU) {
      GCon->Log(NAME_Init, "Sorry, but your GPU seems to be in my glitchy list; turning off some advanced features");
      GCon->Logf(NAME_Init, "GPU Vendor: %s", vcstr);
      if (gl_dbg_ignore_gpu_blacklist) {
        GCon->Log(NAME_Init, "User command is to ignore blacklist; I shall obey!");
        isShittyGPU = false;
      }
    }
  }

  if (!isShittyGPU && gl_dbg_force_gpu_blacklisting) {
    GCon->Log(NAME_Init, "User command is to blacklist GPU; I shall obey!");
    isShittyGPU = true;
  }

  gl_is_shitty_gpu = isShittyGPU;

  // check the maximum texture size
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
  GCon->Logf(NAME_Init, "Maximum texture size: %d", maxTexSize);
  if (maxTexSize < 1024) maxTexSize = 1024; // 'cmon!

  hasNPOT = CheckExtension("GL_ARB_texture_non_power_of_two") || CheckExtension("GL_OES_texture_npot");
  hasBoundsTest = CheckExtension("GL_EXT_depth_bounds_test");

#define _(x)  p_##x = x##_t(GetExtFuncPtr(#x)); if (!p_##x) found = false

  useReverseZ = false;
  GLint major, minor;
  glGetIntegerv(GL_MAJOR_VERSION, &major);
  glGetIntegerv(GL_MINOR_VERSION, &minor);
  GCon->Logf(NAME_Init, "OpenGL v%d.%d found", major, minor);
  if (!isShittyGPU) {
    // normal GPUs
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
  } else {
    GCon->Logf(NAME_Init, "OpenGL: reverse z is turned off for your GPU");
    p_glClipControl = nullptr;
  }

  // check multi-texture extensions
  if (!CheckExtension("GL_ARB_multitexture")) {
    Sys_Error("OpenGL FATAL: Multitexture extensions not found.");
  } else {
    bool found = true;

    //_(glMultiTexCoord2fARB);
    _(glActiveTextureARB);

    if (!found) Sys_Error("OpenGL FATAL: Multitexture extensions not found.");

    GCon->Log(NAME_Init, "Multitexture extensions found.");
    GLint tmp;
    glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max texture units: %d", tmp);
    if (tmp > 1) MaxTextureUnits = tmp;
  }

  // check main stencil buffer
  // this is purely informative, as we are using FBO to render things anyway
  {
    GLint stencilBits = 0;
    glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
    GCon->Logf(NAME_Init, "Main stencil buffer depth: %d", stencilBits);
  }

  // anisotropy extension
  max_anisotropy = 1.0f;
  if (ext_anisotropy && CheckExtension("GL_EXT_texture_filter_anisotropic")) {
    glGetFloatv(GLenum(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT), &max_anisotropy);
    if (max_anisotropy < 1) max_anisotropy = 1;
    GCon->Logf(NAME_Init, "Max anisotropy: %g", (double)max_anisotropy);
  }
  gl_max_anisotropy = (int)max_anisotropy;
  anisotropyExists = (gl_max_anisotropy > 1);

  // clamp to edge extension
  if (CheckExtension("GL_SGIS_texture_edge_clamp") || CheckExtension("GL_EXT_texture_edge_clamp")) {
    GCon->Log(NAME_Init, "Clamp to edge extension found.");
    ClampToEdge = GL_CLAMP_TO_EDGE_SGIS;
  } else {
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

    if (hasBoundsTest) {
      check(found);
      _(glDepthBoundsEXT);
      if (!found) {
        hasBoundsTest = false;
        GCon->Logf(NAME_Init, "OpenGL: GL_EXT_depth_bounds_test found, but no `glDepthBoundsEXT()` exported");
      }
    }


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

  if (hasBoundsTest) GCon->Logf(NAME_Init, "Found GL_EXT_depth_bounds_test...");

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

  // delete old FBOs
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

  if (secondFBO) {
    glBindFramebuffer(GL_FRAMEBUFFER, secondFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (secondFBOColorTid) glDeleteTextures(1, &secondFBOColorTid);
    glDeleteFramebuffers(1, &secondFBO);
    secondFBO = 0;
    secondFBOColorTid = 0;
  }

  if (ambLightFBO) {
    glBindFramebuffer(GL_FRAMEBUFFER, ambLightFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (ambLightFBOColorTid) glDeleteTextures(1, &ambLightFBOColorTid);
    glDeleteFramebuffers(1, &ambLightFBO);
    ambLightFBO = 0;
    ambLightFBOColorTid = 0;
  }


  // allocate main FBO object
  glGenFramebuffers(1, &mainFBO);
  if (mainFBO == 0) Sys_Error("OpenGL: cannot create main FBO");
  glBindFramebuffer(GL_FRAMEBUFFER, mainFBO);

  // attach 2D texture to this FBO
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

  GLint depthStencilFormat = GL_DEPTH24_STENCIL8;

  if (major >= 3 && gl_enable_floating_zbuffer) {
    if (isShittyGPU) {
      GCon->Logf(NAME_Init, "OpenGL: fp depth buffer is turned off for your GPU");
      useReverseZ = false; // just in case
    } else {
      depthStencilFormat = GL_DEPTH32F_STENCIL8;
      GCon->Logf(NAME_Init, "OpenGL: using floating-point depth buffer");
    }
  } else {
    if (useReverseZ) {
      if (gl_dbg_force_reverse_z) {
        GCon->Log(NAME_Init, "OpenGL: cannot use fp depth buffer, but \"reverse z\" is forced by user");
      } else {
        GCon->Log(NAME_Init, "OpenGL: cannot use fp depth buffer, turning off \"reverse z\"");
        useReverseZ = false;
      }
    }
  }

  (void)glGetError();
  /*
  if (!useReverseZ) {
    if (major >= 3 && gl_enable_floating_zbuffer) GCon->Logf(NAME_Init, "OpenGL: using floating-point depth buffer");
    //glTexImage2D(GL_TEXTURE_2D, 0, (major >= 3 && gl_enable_floating_zbuffer ? GL_DEPTH32F_STENCIL8 : GL_DEPTH_STENCIL), ScreenWidth, ScreenHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
    glTexImage2D(GL_TEXTURE_2D, 0, depthStencilFormat, ScreenWidth, ScreenHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
  } else {
    // reversed z
    //glTexImage2D(GL_TEXTURE_2D, 0, (useReverseZ ? GL_DEPTH32F_STENCIL8 : GL_DEPTH_STENCIL), ScreenWidth, ScreenHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
    glTexImage2D(GL_TEXTURE_2D, 0, depthStencilFormat, ScreenWidth, ScreenHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
  }
  */
  glTexImage2D(GL_TEXTURE_2D, 0, depthStencilFormat, ScreenWidth, ScreenHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
  if (glGetError() != 0) {
    if (depthStencilFormat == GL_DEPTH32F_STENCIL8) {
      if (useReverseZ) {
        if (gl_dbg_force_reverse_z) {
          GCon->Log(NAME_Init, "OpenGL: cannot create fp depth buffer, but \"reverse z\" is forced by user");
        } else {
          GCon->Log(NAME_Init, "OpenGL: cannot create fp depth buffer, turning off \"reverse z\"");
          useReverseZ = false;
        }
      } else {
        GCon->Log(NAME_Init, "OpenGL: cannot create fp depth buffer, trying 24-bit one");
      }
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, ScreenWidth, ScreenHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
      if (glGetError() != 0) Sys_Error("OpenGL initialization error");
    } else {
      Sys_Error("OpenGL initialization error");
    }
  }
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, mainFBODepthStencilTid, 0);

  {
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) Sys_Error("OpenGL: framebuffer creation failed");
  }


  glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // Black Background
  glClearDepth(!useReverseZ ? 1.0f : 0.0f);
  if (p_glClipControl) p_glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); // actually, this is better even for "normal" cases
  RestoreDepthFunc();
  glDepthRange(0.0f, 1.0f);

  glClearStencil(0);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
  stencilBufferDirty = false;


  // allocate ambient FBO object
  glGenFramebuffers(1, &ambLightFBO);
  if (ambLightFBO == 0) Sys_Error("OpenGL: cannot create ambient FBO");
  glBindFramebuffer(GL_FRAMEBUFFER, ambLightFBO);

  // attach 2D texture to this FBO
  glGenTextures(1, &ambLightFBOColorTid);
  if (ambLightFBOColorTid == 0) Sys_Error("OpenGL: cannot create RGBA texture for main FBO");
  glBindTexture(GL_TEXTURE_2D, ambLightFBOColorTid);

  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, /*GL_CLAMP_TO_EDGE*/ClampToEdge);
  //glnvg__checkError(gl, "glnvg__allocFBO: glTexParameterf: GL_TEXTURE_WRAP_S");
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, /*GL_CLAMP_TO_EDGE*/ClampToEdge);

  //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // empty texture
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ScreenWidth, ScreenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ambLightFBOColorTid, 0);



  glBindTexture(GL_TEXTURE_2D, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);


  glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // Black Background
  glClearDepth(!useReverseZ ? 1.0f : 0.0f);
  if (p_glClipControl) p_glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); // actually, this is better even for "normal" cases
  RestoreDepthFunc();
  glDepthRange(0.0f, 1.0f);

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
  DrawSimple_Program = CreateProgram(VertexShader, FragmentShader);
  DrawSimple_TextureLoc = p_glGetUniformLocationARB(DrawSimple_Program, "Texture");
  DrawSimple_AlphaLoc = p_glGetUniformLocationARB(DrawSimple_Program, "Alpha");


  // reuses vertex shader
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/draw_shadow.fs");
  DrawShadow_Program = CreateProgram(VertexShader, FragmentShader);
  DrawShadow_TextureLoc = p_glGetUniformLocationARB(DrawShadow_Program, "Texture");
  DrawShadow_AlphaLoc = p_glGetUniformLocationARB(DrawShadow_Program, "Alpha");

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/draw_fixed_col.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/draw_fixed_col.fs");
  DrawFixedCol_Program = CreateProgram(VertexShader, FragmentShader);
  DrawFixedCol_ColourLoc = p_glGetUniformLocationARB(DrawFixedCol_Program, "Colour");

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/draw_automap.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/draw_automap.fs");
  DrawAutomap_Program = CreateProgram(VertexShader, FragmentShader);

  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_zbuf.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_zbuf.fs");
  SurfZBuf_Program = CreateProgram(VertexShader, FragmentShader);


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_decal_adv.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_decal_adv.fs");
  SurfAdvDecal_Program = CreateProgram(VertexShader, FragmentShader);
  SurfAdvDecal_TextureLoc = p_glGetUniformLocationARB(SurfAdvDecal_Program, "Texture");
  SurfAdvDecal_AmbLightTextureLoc = p_glGetUniformLocationARB(SurfAdvDecal_Program, "AmbLightTexture");
  SurfAdvDecal_SplatAlphaLoc = p_glGetUniformLocationARB(SurfAdvDecal_Program, "SplatAlpha");
  SurfAdvDecal_FullBright = p_glGetUniformLocationARB(SurfAdvDecal_Program, "FullBright");
  SurfAdvDecal_ScreenSize = p_glGetUniformLocationARB(SurfAdvDecal_Program, "ScreenSize");


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_decal_nolmap.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_decal_nolmap.fs");
  SurfDecalNoLMap_Program = CreateProgram(VertexShader, FragmentShader);
  SurfDecalNoLMap_TextureLoc = p_glGetUniformLocationARB(SurfDecalNoLMap_Program, "Texture");
  SurfDecalNoLMap_SplatAlphaLoc = p_glGetUniformLocationARB(SurfDecalNoLMap_Program, "SplatAlpha");
  SurfDecalNoLMap_LightLoc = p_glGetUniformLocationARB(SurfDecalNoLMap_Program, "Light");
  SurfDecalNoLMap_Locs.setupProg(this, SurfDecalNoLMap_Program);
  SurfDecalNoLMap_Locs.setupFog();


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_decal_lmap.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_decal_lmap.fs");
  SurfDecalLMap_Program = CreateProgram(VertexShader, FragmentShader);
  SurfDecalLMap_TextureLoc = p_glGetUniformLocationARB(SurfDecalLMap_Program, "Texture");
  SurfDecalLMap_SplatAlphaLoc = p_glGetUniformLocationARB(SurfDecalLMap_Program, "SplatAlpha");
  SurfDecalLMap_LightLoc = p_glGetUniformLocationARB(SurfDecalLMap_Program, "Light");
  SurfDecalLMap_SpecularMapLoc = p_glGetUniformLocationARB(SurfDecalLMap_Program, "SpecularMap");
  SurfDecalLMap_Locs.setupProg(this, SurfDecalLMap_Program);
  SurfDecalLMap_Locs.setupLMapOnly();
  SurfDecalLMap_Locs.setupFog();


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_simple.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_simple.fs");
  SurfSimple_Program = CreateProgram(VertexShader, FragmentShader);
  SurfSimple_LightLoc = p_glGetUniformLocationARB(SurfSimple_Program, "Light");
  SurfSimple_Locs.setupProg(this, SurfSimple_Program);
  SurfSimple_Locs.setupTexture();
  SurfSimple_Locs.setupFog();


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_lightmap.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_lightmap.fs");
  SurfLightmap_Program = CreateProgram(VertexShader, FragmentShader);
  SurfLightmap_SpecularMapLoc = p_glGetUniformLocationARB(SurfLightmap_Program, "SpecularMap");
  SurfLightmap_Locs.setupProg(this, SurfLightmap_Program);
  SurfLightmap_Locs.setupTexture();
  SurfLightmap_Locs.setupLMap();
  SurfLightmap_Locs.setupFog();


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_sky.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_sky.fs");
  SurfSky_Program = CreateProgram(VertexShader, FragmentShader);
  SurfSky_TextureLoc = p_glGetUniformLocationARB(SurfSky_Program, "Texture");
  SurfSky_BrightnessLoc = p_glGetUniformLocationARB(SurfSky_Program, "Brightness");
  SurfSky_TexCoordLoc = p_glGetAttribLocationARB(SurfSky_Program, "TexCoord");


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_dsky.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_dsky.fs");
  SurfDSky_Program = CreateProgram(VertexShader, FragmentShader);
  SurfDSky_TextureLoc = p_glGetUniformLocationARB(SurfDSky_Program, "Texture");
  SurfDSky_Texture2Loc = p_glGetUniformLocationARB(SurfDSky_Program, "Texture2");
  SurfDSky_BrightnessLoc = p_glGetUniformLocationARB(SurfDSky_Program, "Brightness");
  SurfDSky_TexCoordLoc = p_glGetAttribLocationARB(SurfDSky_Program, "TexCoord");
  SurfDSky_TexCoord2Loc = p_glGetAttribLocationARB(SurfDSky_Program, "TexCoord2");


#define GLSL_LOADLOC(lcname_)  SurfMasked_ ## lcname_ ## Loc  = p_glGetUniformLocationARB(SurfMasked_Program, "" #lcname_)
#define GLSL_LOADATR(lcname_)  SurfMasked_ ## lcname_ ## Loc  = p_glGetAttribLocationARB(SurfMasked_Program, "" #lcname_)
  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_masked.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_masked.fs");
  SurfMasked_Program = CreateProgram(VertexShader, FragmentShader);
  GLSL_LOADLOC(Texture);
  GLSL_LOADLOC(Light);
  GLSL_LOADLOC(AlphaRef);
  GLSL_LOADATR(TexCoord);
  SurfMasked_Locs.setupProg(this, SurfMasked_Program);
  SurfMasked_Locs.setupFog();
#undef GLSL_LOADATR
#undef GLSL_LOADLOC


#define GLSL_LOADLOC(lcname_)  SurfModel_ ## lcname_ ## Loc  = p_glGetUniformLocationARB(SurfModel_Program, "" #lcname_)
#define GLSL_LOADATR(lcname_)  SurfModel_ ## lcname_ ## Loc  = p_glGetAttribLocationARB(SurfModel_Program, "" #lcname_)
  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_model.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_model.fs");
  SurfModel_Program = CreateProgram(VertexShader, FragmentShader);
  GLSL_LOADLOC(Inter);
  GLSL_LOADLOC(Texture);
  GLSL_LOADATR(Vert2);
  GLSL_LOADATR(TexCoord);
  GLSL_LOADLOC(Alpha);
  GLSL_LOADATR(LightVal);
  GLSL_LOADLOC(ViewOrigin);
  GLSL_LOADLOC(AllowTransparency);
  SurfModel_Locs.setupProg(this, SurfModel_Program);
  SurfModel_Locs.setupFog();
#undef GLSL_LOADATR
#undef GLSL_LOADLOC


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/particle.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/particle_sq.fs");
  SurfPartSq_Program = CreateProgram(VertexShader, FragmentShader);
  SurfPartSq_TexCoordLoc = p_glGetAttribLocationARB(SurfPartSq_Program, "TexCoord");
  SurfPartSq_LightValLoc = p_glGetAttribLocationARB(SurfPartSq_Program, "LightVal");


  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/particle_sm.fs");
  SurfPartSm_Program = CreateProgram(VertexShader, FragmentShader);
  SurfPartSm_TexCoordLoc = p_glGetAttribLocationARB(SurfPartSm_Program, "TexCoord");
  SurfPartSm_LightValLoc = p_glGetAttribLocationARB(SurfPartSm_Program, "LightVal");


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_surf_ambient.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_surf_ambient.fs");
  ShadowsAmbient_Program = CreateProgram(VertexShader, FragmentShader);
  ShadowsAmbient_LightLoc = p_glGetUniformLocationARB(ShadowsAmbient_Program, "Light");
  ShadowsAmbient_Locs.setupProg(this, ShadowsAmbient_Program);
  ShadowsAmbient_Locs.setupTexture();


#define GLSL_LOADLOC(lcname_)  ShadowsLight_ ## lcname_ ## Loc  = p_glGetUniformLocationARB(ShadowsLight_Program, "" #lcname_)
#define GLSL_LOADATR(lcname_)  ShadowsLight_ ## lcname_ ## Loc  = p_glGetAttribLocationARB(ShadowsLight_Program, "" #lcname_)
  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_surf_light.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_surf_light.fs");
  ShadowsLight_Program = CreateProgram(VertexShader, FragmentShader);
  GLSL_LOADLOC(LightPos);
  GLSL_LOADLOC(LightRadius);
  GLSL_LOADLOC(LightColour);
  GLSL_LOADATR(SurfNormal);
  GLSL_LOADATR(SurfDist);
  GLSL_LOADLOC(Alpha);
  GLSL_LOADLOC(ViewOrigin);
  ShadowsLight_Locs.setupProg(this, ShadowsLight_Program);
  ShadowsLight_Locs.setupTexture();
#undef GLSL_LOADATR
#undef GLSL_LOADLOC


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_surf_texture.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_surf_texture.fs");
  ShadowsTexture_Program = CreateProgram(VertexShader, FragmentShader);
  ShadowsTexture_Locs.setupProg(this, ShadowsTexture_Program);
  ShadowsTexture_Locs.setupTexture();


#define GLSL_LOADLOC(lcname_)  ShadowsModelAmbient_ ## lcname_ ## Loc  = p_glGetUniformLocationARB(ShadowsModelAmbient_Program, "" #lcname_)
#define GLSL_LOADATR(lcname_)  ShadowsModelAmbient_ ## lcname_ ## Loc  = p_glGetAttribLocationARB(ShadowsModelAmbient_Program, "" #lcname_)
  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_model_ambient.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_model_ambient.fs");
  ShadowsModelAmbient_Program = CreateProgram(VertexShader, FragmentShader);
  GLSL_LOADLOC(Inter);
  GLSL_LOADLOC(Texture);
  GLSL_LOADLOC(Light);
  GLSL_LOADLOC(ModelToWorldMat);
  GLSL_LOADLOC(NormalToWorldMat);
  GLSL_LOADATR(Vert2);
  GLSL_LOADATR(VertNormal);
  GLSL_LOADATR(Vert2Normal);
  GLSL_LOADATR(TexCoord);
  GLSL_LOADLOC(Alpha);
  GLSL_LOADLOC(ViewOrigin);
  GLSL_LOADLOC(AllowTransparency);
#undef GLSL_LOADATR
#undef GLSL_LOADLOC


#define GLSL_LOADLOC(lcname_)  ShadowsModelTextures_ ## lcname_ ## Loc  = p_glGetUniformLocationARB(ShadowsModelTextures_Program, "" #lcname_)
#define GLSL_LOADATR(lcname_)  ShadowsModelTextures_ ## lcname_ ## Loc  = p_glGetAttribLocationARB(ShadowsModelTextures_Program, "" #lcname_)
  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_model_textures.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_model_textures.fs");
  ShadowsModelTextures_Program = CreateProgram(VertexShader, FragmentShader);
  GLSL_LOADLOC(Inter);
  GLSL_LOADLOC(Texture);
  GLSL_LOADLOC(ModelToWorldMat);
  GLSL_LOADLOC(NormalToWorldMat);
  GLSL_LOADATR(Vert2);
  GLSL_LOADATR(TexCoord);
  GLSL_LOADATR(VertNormal);
  GLSL_LOADATR(Vert2Normal);
  GLSL_LOADLOC(Alpha);
  GLSL_LOADLOC(ViewOrigin);
  GLSL_LOADLOC(AllowTransparency);
  GLSL_LOADLOC(AmbLightTexture);
  GLSL_LOADLOC(ScreenSize);
#undef GLSL_LOADATR
#undef GLSL_LOADLOC

#define GLSL_LOADLOC(lcname_)  ShadowsModelLight_ ## lcname_ ## Loc  = p_glGetUniformLocationARB(ShadowsModelLight_Program, "" #lcname_)
#define GLSL_LOADATR(lcname_)  ShadowsModelLight_ ## lcname_ ## Loc  = p_glGetAttribLocationARB(ShadowsModelLight_Program, "" #lcname_)
  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_model_light.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_model_light.fs");
  ShadowsModelLight_Program = CreateProgram(VertexShader, FragmentShader);
  GLSL_LOADLOC(Inter);
  GLSL_LOADLOC(Texture);
  GLSL_LOADLOC(LightPos);
  GLSL_LOADLOC(LightRadius);
  GLSL_LOADLOC(LightColour);
  GLSL_LOADLOC(ModelToWorldMat);
  GLSL_LOADLOC(NormalToWorldMat);
  GLSL_LOADATR(Vert2);
  GLSL_LOADATR(VertNormal);
  GLSL_LOADATR(Vert2Normal);
  GLSL_LOADATR(TexCoord);
  GLSL_LOADLOC(ViewOrigin);
  GLSL_LOADLOC(AllowTransparency);
#undef GLSL_LOADATR
#undef GLSL_LOADLOC


#define GLSL_LOADLOC(lcname_)  ShadowsModelShadow_ ## lcname_ ## Loc  = p_glGetUniformLocationARB(ShadowsModelShadow_Program, "" #lcname_)
#define GLSL_LOADATR(lcname_)  ShadowsModelShadow_ ## lcname_ ## Loc  = p_glGetAttribLocationARB(ShadowsModelShadow_Program, "" #lcname_)
  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_model_shadow.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_model_shadow.fs");
  ShadowsModelShadow_Program = CreateProgram(VertexShader, FragmentShader);
  GLSL_LOADLOC(Inter);
  GLSL_LOADLOC(LightPos);
  GLSL_LOADLOC(ModelToWorldMat);
  GLSL_LOADATR(Vert2);
  GLSL_LOADATR(Offset);
  GLSL_LOADLOC(ViewOrigin);
#undef GLSL_LOADATR
#undef GLSL_LOADLOC


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_surf_fog.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_surf_fog.fs");
  ShadowsFog_Program = CreateProgram(VertexShader, FragmentShader);
  ShadowsFog_Locs.setupProg(this, ShadowsFog_Program);
  ShadowsFog_Locs.setupFog();


#define GLSL_LOADLOC(lcname_)  ShadowsModelFog_ ## lcname_ ## Loc  = p_glGetUniformLocationARB(ShadowsModelFog_Program, "" #lcname_)
#define GLSL_LOADATR(lcname_)  ShadowsModelFog_ ## lcname_ ## Loc  = p_glGetAttribLocationARB(ShadowsModelFog_Program, "" #lcname_)
  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_model_fog.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_model_fog.fs");
  ShadowsModelFog_Program = CreateProgram(VertexShader, FragmentShader);
  GLSL_LOADLOC(Inter);
  GLSL_LOADLOC(ModelToWorldMat);
  GLSL_LOADLOC(Texture);
  GLSL_LOADLOC(FogType);
  GLSL_LOADLOC(FogColour);
  GLSL_LOADLOC(FogDensity);
  GLSL_LOADLOC(FogStart);
  GLSL_LOADLOC(FogEnd);
  GLSL_LOADATR(Vert2);
  GLSL_LOADATR(TexCoord);
  GLSL_LOADLOC(Alpha);
  GLSL_LOADLOC(ViewOrigin);
  GLSL_LOADLOC(AllowTransparency);
#undef GLSL_LOADATR
#undef GLSL_LOADLOC


  if (glGetError() != 0) Sys_Error("OpenGL initialization error");


  mInitialized = true;

  callICB(VCB_InitResolution);
}


//==========================================================================
//
//  VOpenGLDrawer::CheckExtension
//
//==========================================================================
bool VOpenGLDrawer::CheckExtension (const char *ext) {
  if (!ext || !ext[0]) return false;
  TArray<VStr> Exts;
  VStr((char*)glGetString(GL_EXTENSIONS)).Split(' ', Exts);
  for (int i = 0; i < Exts.Num(); ++i) if (Exts[i] == ext) return true;
  return false;
}


//==========================================================================
//
//  VOpenGLDrawer::SupportsAdvancedRendering
//
//==========================================================================
bool VOpenGLDrawer::SupportsAdvancedRendering () {
  return (HaveStencilWrap && p_glStencilFuncSeparate && HaveDrawRangeElements);
}


//==========================================================================
//
//  VOpenGLDrawer::Setup2D
//
//==========================================================================
void VOpenGLDrawer::Setup2D () {
  glViewport(0, 0, ScreenWidth, ScreenHeight);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, ScreenWidth, ScreenHeight, 0, -666, 666);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  if (HaveDepthClamp) glDisable(GL_DEPTH_CLAMP);
}


//==========================================================================
//
//  VOpenGLDrawer::StartUpdate
//
//==========================================================================
void VOpenGLDrawer::StartUpdate (bool allowClear) {
  //glFinish();

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

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_TEXTURE_2D);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    p_glUseProgramObjectARB(0);

    int realw, realh;
    GetRealWindowSize(&realw, &realh);

    if (realw == ScreenWidth && realh == ScreenHeight) {
      // copy texture by drawing full quad
      //glViewport(0, 0, ScreenWidth, ScreenHeight);
      glOrtho(0, ScreenWidth, ScreenHeight, 0, -666, 666);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex2i(0, 0);
        glTexCoord2f(1.0f, 1.0f); glVertex2i(ScreenWidth, 0);
        glTexCoord2f(1.0f, 0.0f); glVertex2i(ScreenWidth, ScreenHeight);
        glTexCoord2f(0.0f, 0.0f); glVertex2i(0, ScreenHeight);
      glEnd();
    } else {
      glViewport(0, 0, realw, realh);
      glOrtho(0, realw, realh, 0, -99999, 99999);
      glClear(GL_COLOR_BUFFER_BIT); // just in case

      if (texture_filter > 0) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      }

      // scale it properly
      float scaleX = float(realw)/float(ScreenWidth);
      float scaleY = float(realh)/float(ScreenHeight);
      float scale = (scaleX <= scaleY ? scaleX : scaleY);
      int newWidth = (int)(ScreenWidth*scale);
      int newHeight = (int)(ScreenHeight*scale);
      int x0 = (realw-newWidth)/2;
      int y0 = (realh-newHeight)/2;
      int x1 = x0+newWidth;
      int y1 = y0+newHeight;
      //fprintf(stderr, "scaleX=%f; scaleY=%f; scale=%f; real=(%d,%d); screen=(%d,%d); new=(%d,%d); rect:(%d,%d)-(%d,%d)\n", scaleX, scaleY, scale, realw, realh, ScreenWidth, ScreenHeight, newWidth, newHeight, x0, y0, x1, y1);
      glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex2i(x0, y0);
        glTexCoord2f(1.0f, 1.0f); glVertex2i(x1, y0);
        glTexCoord2f(1.0f, 0.0f); glVertex2i(x1, y1);
        glTexCoord2f(0.0f, 0.0f); glVertex2i(x0, y1);
      glEnd();

      glViewport(0, 0, ScreenWidth, ScreenHeight);
      glOrtho(0, ScreenWidth, ScreenHeight, 0, -666, 666);
    }

    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    //glFlush();
  }
}


//==========================================================================
//
//  VOpenGLDrawer::BeginDirectUpdate
//
//==========================================================================
/*
void VOpenGLDrawer::BeginDirectUpdate () {
  glFinish();
  glDrawBuffer(GL_FRONT);
  if (mainFBO) glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
*/


//==========================================================================
//
//  VOpenGLDrawer::EndDirectUpdate
//
//==========================================================================
/*
void VOpenGLDrawer::EndDirectUpdate () {
  glDrawBuffer(GL_BACK);
  if (mainFBO) glBindFramebuffer(GL_FRAMEBUFFER, mainFBO);
}
*/


//==========================================================================
//
//  VOpenGLDrawer::GetProjectionMatrix
//
//==========================================================================
void VOpenGLDrawer::GetProjectionMatrix (VMatrix4 &mat) {
  glGetFloatv(GL_PROJECTION_MATRIX, mat[0]);
}


//==========================================================================
//
//  VOpenGLDrawer::GetModelMatrix
//
//==========================================================================
void VOpenGLDrawer::GetModelMatrix (VMatrix4 &mat) {
  glGetFloatv(GL_MODELVIEW_MATRIX, mat[0]);
}


//==========================================================================
//
//  glhProjectf
//
//==========================================================================
static inline bool glhProjectf (const TVec &point, const VMatrix4 &modelview, const VMatrix4 &projection, const int *viewport, float *windowCoordinate) {
#if 0
  TVec inworld = point;
  const float iww = modelview.Transform2InPlace(inworld);
  if (inworld.z >= 0.0f) return false; // the w value
  TVec proj = projection.Transform2(inworld, iww);
#else
  // our `w` is always 1
  TVec inworld = modelview.Transform2(point);
  if (inworld.z >= 0.0f) return false; // the w value
  TVec proj = projection.Transform2OnlyXY(inworld); // we don't care about z here
#endif
  const float pjw = -1.0f/inworld.z;
  proj.x *= pjw;
  proj.y *= pjw;
  windowCoordinate[0] = (proj.x*0.5f+0.5f)*viewport[2]+viewport[0];
  windowCoordinate[1] = (proj.y*0.5f+0.5f)*viewport[3]+viewport[1];
  return true;
}


//==========================================================================
//
//  glhProjectfZ
//
//==========================================================================
static inline float glhProjectfZ (const TVec &point, const float zofs, const VMatrix4 &modelview, const VMatrix4 &projection) {
#if 0
  TVec inworld = point;
  const float iww = modelview.Transform2InPlace(inworld);
  if (inworld.z >= 0.0f) return 0.0f;
  TVec proj = projection.Transform2(inworld, iww);
#else
  // our `w` is always 1
  TVec inworld = modelview.Transform2(point);
  inworld.z += zofs;
  //if (inworld.z >= 0.0f) return 0.0f;
  if (inworld.z > -0.001f) inworld.z = -0.001f;
  float projz = projection.Transform2OnlyZ(inworld);
#endif
  const float pjw = -1.0f/inworld.z;
  projz *= pjw;
  return (1.0f+projz)*0.5f;
}


//==========================================================================
//
//  VOpenGLDrawer::SetupLightScissor
//
//  returns:
//   0 if scissor is empty
//  -1 if scissor has no sense (should not be used)
//   1 if scissor is set
//
//==========================================================================
int VOpenGLDrawer::SetupLightScissor (const TVec &org, float radius, int scoord[4]) {
  int tmpscoord[4];
  VMatrix4 pmat, mmat;
  glGetFloatv(GL_PROJECTION_MATRIX, pmat[0]);
  glGetFloatv(GL_MODELVIEW_MATRIX, mmat[0]);

  if (!scoord) scoord = tmpscoord;

/*TODO:
    transform light origin into world coordinates
    build bounding cube in world coordinates (clamping z)
    use bounding cube vertices to do projection
    this way we can still use scissoring for lights behind our back
    also, we can use only 1-3 planes of a cube (but this doesn't look worthy,
    at the worst case we'll drop only one vertex)
*/

  glEnable(GL_SCISSOR_TEST);

  // usually, light completely fades away at edges, so we can safely shrink our scissor box
  // even such small shrinking can win one-two FPS on light-heavy scenes
  //radius -= 6;
  if (radius < 4) {
    scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
    //glScissor(0, 0, 0, 0);
    return 0;
  }

  if (hasBoundsTest && gl_enable_depth_bounds) {
    /*
    // transform light to camera space
    TVec lcorg = mmat.Transform2(org);
    // project nearest and furthest points
    TVec lcnear = TVec(lcorg.x, lcorg.y, lcorg.z-radius);
    TVec lcfar = TVec(lcorg.x, lcorg.y, lcorg.z+radius);
    */

    const float z0 = glhProjectfZ(org, +radius+radius*0.5f, mmat[0], pmat[0]);
    const float z1 = glhProjectfZ(org, -radius-radius*0.5f, mmat[0], pmat[0]);

    //GCon->Logf("radius=%f; z0=%f; z1=%f", radius, z0, z1);

    p_glDepthBoundsEXT(z0, z1);
    glEnable(GL_DEPTH_BOUNDS_TEST_EXT);
  }

  float minx = ScreenWidth*4, miny = ScreenHeight*4;
  float maxx = -ScreenWidth*4, maxy = -ScreenHeight*4;

  if (gl_use_optimised_scissor_calculation) {
    // transform light center to camera space
    TVec lcorg = mmat.Transform2(org);

    // usually, light completely fades away at edges, so we can safely shrink our scissor box
    // even such small shrinking can win one-two FPS on light-heavy scenes
    /*
         if (radius >= 256) radius -= 16;
    else if (radius >= 128) radius -= 8;
    else if (radius >= 64) radius -= 6;
    else if (radius >= 16) radius -= 2;
    */
    //radius /= 2;

    // the thing that should not be
    if (lcorg.z-radius > -0.001f) {
      scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
      //glScissor(0, 0, 0, 0);
      //GCon->Logf("FUCK! (%f) radius=%f; z=%f; z0=%f; z1=%f", org.y, radius, lcorg.z, lcorg.z-radius, lcorg.z+radius);
      return -1;
    }

    // create light bbox
    float bbox[6];
    bbox[0+0] = lcorg.x-radius;
    bbox[0+1] = lcorg.y-radius;
    bbox[0+2] = lcorg.z-radius;

    bbox[3+0] = lcorg.x+radius;
    bbox[3+1] = lcorg.y+radius;
    bbox[3+2] = lcorg.z+radius;

    if (bbox[3+2] > -0.001f) bbox[3+2] = -0.001f;

    check(bbox[0+2] <= -0.001f);

    const float scrw = ScreenWidth;
    const float scrh = ScreenHeight;

    // transform points, get min and max
    for (unsigned f = 0; f < 8; ++f) {
      TVec inworld = TVec(bbox[BBoxVertexIndex[f][0]], bbox[BBoxVertexIndex[f][1]], bbox[BBoxVertexIndex[f][2]]);
      TVec proj = pmat.Transform2OnlyXY(inworld); // we don't care about z here
      const float pjw = -1.0f/inworld.z;
      proj.x *= pjw;
      proj.y *= pjw;
      float winx = (proj.x*0.5f+0.5f)*scrw;
      float winy = (proj.y*0.5f+0.5f)*scrh;

      if (minx > winx) minx = winx;
      if (miny > winy) miny = winy;
      if (maxx < winx) maxx = winx;
      if (maxy < winy) maxy = winy;
    }
  } else {
    // my original code
    const int viewport[4] = { 0, 0, ScreenWidth, ScreenHeight };

    // create light bbox
    float bbox[6];
    bbox[0+0] = org.x-radius;
    bbox[0+1] = org.y-radius;
    bbox[0+2] = org.z-radius;

    bbox[3+0] = org.x+radius;
    bbox[3+1] = org.y+radius;
    bbox[3+2] = org.z+radius;

    // create 8 bbox points
    TVec bbp[8];
    bbp[0] = TVec(bbox[0+0], bbox[0+1], bbox[0+2]);
    bbp[1] = TVec(bbox[3+0], bbox[0+1], bbox[0+2]);
    bbp[2] = TVec(bbox[0+0], bbox[3+1], bbox[0+2]);
    bbp[3] = TVec(bbox[3+0], bbox[3+1], bbox[0+2]);
    bbp[4] = TVec(bbox[0+0], bbox[0+1], bbox[3+2]);
    bbp[5] = TVec(bbox[3+0], bbox[0+1], bbox[3+2]);
    bbp[6] = TVec(bbox[0+0], bbox[3+1], bbox[3+2]);
    bbp[7] = TVec(bbox[3+0], bbox[3+1], bbox[3+2]);

    //unsigned badCount = 0;
    //float minx = ScreenWidth*4, miny = ScreenHeight*4;
    //float maxx = -ScreenWidth*4, maxy = -ScreenHeight*4;
    // transform points, get min and max
    for (unsigned f = 0; f < 8; ++f) {
      float wc[2];
      if (!glhProjectf(bbp[f], mmat[0], pmat[0], viewport, wc)) {
        scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
        //glScissor(0, 0, 0, 0);
        return -1;
        //++badCount;
        //continue;
      }
      //GCon->Logf("f=%u; org=(%f,%f,%f); radius=%f; wc=(%f,%f)", f, bbp[f].x, bbp[f].y, bbp[f].z, radius, wc[0], wc[1]);
      if (minx > wc[0]) minx = wc[0];
      if (miny > wc[1]) miny = wc[1];
      if (maxx < wc[0]) maxx = wc[0];
      if (maxy < wc[1]) maxy = wc[1];
    }

    /*
    if (badCount == 8) {
      scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
      //glScissor(0, 0, 0, 0);
      GCon->Logf("completely away, wtf?!");
      return 0;
    }
    if (badCount) GCon->Logf("bc=%u; org=(%f,%f,%f); radius=%f; scissor=(%f,%f)-(%f,%f)", badCount, org.x, org.y, org.z, radius, minx, miny, maxx, maxy);
    */
  }


  if (minx >= ScreenWidth || miny >= ScreenHeight || maxx < 0 || maxy < 0) {
    scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
    glScissor(0, 0, 0, 0);
    return 0;
  }

  if (minx < 0) minx = 0; else if (minx > ScreenWidth) minx = ScreenWidth;
  if (miny < 0) miny = 0; else if (miny > ScreenHeight) miny = ScreenHeight;
  if (maxx < 0) maxx = 0; else if (maxx > ScreenWidth) maxx = ScreenWidth;
  if (maxy < 0) maxy = 0; else if (maxy > ScreenHeight) maxy = ScreenHeight;

  if (maxx <= minx || maxy <= miny) {
    scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
    //glScissor(0, 0, 0, 0);
    return 0;
  }

  //GCon->Logf("radius=%f; box=(%d,%d)-(%d,%d)", radius, (int)minx, (int)miny, (int)maxx, (int)maxy);

  glScissor(minx, miny, maxx-minx+1, maxy-miny+1);
  scoord[0] = minx;
  scoord[1] = miny;
  scoord[2] = maxx;
  scoord[3] = maxy;
  /*
  if (scoord[0] == 0 && scoord[1] == 0 && scoord[2] == ScreenWidth-1 && scoord[3] == ScreenHeight-1) {
  } else {
    GCon->Logf("org=(%f,%f,%f); radius=%f; scissor=(%f,%f)-(%f,%f)", org.x, org.y, org.z, radius, minx, miny, maxx, maxy);
  }
  */

  //GCon->Logf("org=(%f,%f,%f); radius=%f; bbox=(%f,%f,%f)-(%f,%f,%f)", org.x, org.y, org.z, radius, bbox[0], bbox[1], bbox[2], bbox[3], bbox[4], bbox[5]);
  //GCon->Logf("  trbbox=(%f,%f,%f)-(%f,%f,%f); prbbox=(%f,%f,%f)-(%f,%f,%f)", trbb[0].x, trbb[0].y, trbb[0].z, trbb[1].x, trbb[1].y, trbb[1].z, prbb[0].x, prbb[0].y, prbb[0].z, prbb[1].x, prbb[1].y, prbb[1].z);
  return 1;
}


//==========================================================================
//
//  VOpenGLDrawer::ResetScissor
//
//==========================================================================
void VOpenGLDrawer::ResetScissor () {
  glScissor(0, 0, ScreenWidth, ScreenHeight);
  glDisable(GL_SCISSOR_TEST);
  if (hasBoundsTest) glDisable(GL_DEPTH_BOUNDS_TEST_EXT);
}


//==========================================================================
//
//  VOpenGLDrawer::SetupView
//
//==========================================================================
void VOpenGLDrawer::SetupView (VRenderLevelDrawer *ARLev, const refdef_t *rd) {
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
    ProjMat[0][0] = 1.0f/rd->fovx;
    ProjMat[1][1] = 1.0f/rd->fovy;
    ProjMat[2][3] = -1.0f;
    ProjMat[3][3] = 0.0f;
    if (RendLev && RendLev->NeedsInfiniteFarClip && !HaveDepthClamp) {
      ProjMat[2][2] = -1.0f;
      ProjMat[3][2] = -2.0f;
    } else {
      ProjMat[2][2] = -(maxdist+1.0f)/(maxdist-1.0f);
      ProjMat[3][2] = -2.0f*maxdist/(maxdist-1.0f);
    }
  } else {
    // reversed
    //GCon->Logf("!!!REV!!!");
    glClearDepth(0.0f);
    glDepthFunc(GL_GEQUAL);
    for (int f = 0; f < 4; ++f) for (int c = 0; c < 4; ++c) ProjMat.m[f][c] = 0;
    ProjMat[0][0] = 1.0f/rd->fovx;
    ProjMat[1][1] = 1.0f/rd->fovy;
    ProjMat[2][3] = -1.0f;
    ProjMat[3][2] = 0.001f;
  }
  //RestoreDepthFunc();

  glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
  stencilBufferDirty = false;

  glViewport(rd->x, ScreenHeight-rd->height-rd->y, rd->width, rd->height);

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
}


//==========================================================================
//
//  VOpenGLDrawer::SetupViewOrg
//
//==========================================================================
void VOpenGLDrawer::SetupViewOrg () {
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

  if (MirrorClip && view_frustum.planes[5].isValid()) {
    glEnable(GL_CLIP_PLANE0);
    const GLdouble eq[4] = {
      view_frustum.planes[5].normal.x, view_frustum.planes[5].normal.y, view_frustum.planes[5].normal.z,
      -view_frustum.planes[5].dist
    };
    glClipPlane(GL_CLIP_PLANE0, eq);
  } else {
    glDisable(GL_CLIP_PLANE0);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::EndView
//
//==========================================================================
void VOpenGLDrawer::EndView () {
  Setup2D();

  if (cl && cl->CShift) {
    p_glUseProgramObjectARB(DrawFixedCol_Program);
    p_glUniform4fARB(DrawFixedCol_ColourLoc,
      (float)((cl->CShift>>16)&255)/255.0f,
      (float)((cl->CShift>>8)&255)/255.0f,
      (float)(cl->CShift&255)/255.0f,
      (float)((cl->CShift>>24)&255)/255.0f);
    glEnable(GL_BLEND);

    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(ScreenWidth, 0);
    glVertex2f(ScreenWidth, ScreenHeight);
    glVertex2f(0, ScreenHeight);
    glEnd();

    glDisable(GL_BLEND);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::ReadScreen
//
//==========================================================================
void *VOpenGLDrawer::ReadScreen (int *bpp, bool *bot2top) {
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
}


//==========================================================================
//
//  VOpenGLDrawer::ReadBackScreen
//
//==========================================================================
void VOpenGLDrawer::ReadBackScreen (int Width, int Height, rgba_t *Dest) {
  if (Width < 1 || Height < 1) return;
  //check(Width > 0);
  //check(Height > 0);
  check(Dest);

  if (ScreenWidth < 1 || ScreenHeight < 1) {
    memset((void *)Dest, 0, Width*Height*sizeof(rgba_t));
    return;
  }

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
    check(temp);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, temp);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (Width <= ScreenWidth) {
      size_t blen = Width*sizeof(rgba_t);
      for (int y = 0; y < Height; ++y) memcpy(Dest+y*Width, temp+(ScreenHeight-y-1)*ScreenWidth, blen);
    } else {
      size_t blen = ScreenWidth*sizeof(rgba_t);
      size_t restlen = Width*sizeof(rgba_t)-blen;
      for (int y = 0; y < Height; ++y) {
        memcpy(Dest+y*Width, temp+(ScreenHeight-y-1)*ScreenWidth, blen);
        memset((void *)(Dest+y*Width+ScreenWidth), 0, restlen);
      }
    }
    //delete[] temp;
  }
}


//==========================================================================
//
//  VOpenGLDrawer::SetFade
//
//==========================================================================
void VOpenGLDrawer::SetFade (vuint32 NewFade) {
  if ((vuint32)CurrentFade == NewFade) return;

  if (NewFade) {
    static GLenum fogMode[4] = { GL_LINEAR, GL_LINEAR, GL_EXP, GL_EXP2 };
    float fogColour[4];

    fogColour[0] = float((NewFade>>16)&255)/255.0f;
    fogColour[1] = float((NewFade>>8)&255)/255.0f;
    fogColour[2] = float(NewFade&255)/255.0f;
    fogColour[3] = float((NewFade>>24)&255)/255.0f;
    glFogi(GL_FOG_MODE, fogMode[r_fog&3]);
    glFogfv(GL_FOG_COLOR, fogColour);
    if (NewFade == FADE_LIGHT) {
      glFogf(GL_FOG_DENSITY, 0.3f);
      glFogf(GL_FOG_START, 1.0f);
      glFogf(GL_FOG_END, 1024.0f*r_fade_factor);
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
}


//==========================================================================
//
//  VOpenGLDrawer::CopyToSecondaryFBO
//
//  copy main FBO texture to secondary FBO
//
//==========================================================================
void VOpenGLDrawer::CopyToSecondaryFBO () {
  if (!mainFBO) return;

  if (!secondFBO) {
    // allocate secondary FBO object
    glGenFramebuffers(1, &secondFBO);
    if (secondFBO == 0) Sys_Error("OpenGL: cannot create secondary FBO");
    glBindFramebuffer(GL_FRAMEBUFFER, secondFBO);

    // attach 2D texture to this FBO
    glGenTextures(1, &secondFBOColorTid);
    if (secondFBOColorTid == 0) Sys_Error("OpenGL: cannot create RGBA texture for main FBO");
    glBindTexture(GL_TEXTURE_2D, secondFBOColorTid);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, /*GL_CLAMP_TO_EDGE*/ClampToEdge);
    //glnvg__checkError(gl, "glnvg__allocFBO: glTexParameterf: GL_TEXTURE_WRAP_S");
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, /*GL_CLAMP_TO_EDGE*/ClampToEdge);

    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // empty texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ScreenWidth, ScreenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, secondFBOColorTid, 0);
  }


  glPushAttrib(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_ENABLE_BIT|GL_VIEWPORT_BIT|GL_TRANSFORM_BIT);

  glBindFramebuffer(GL_FRAMEBUFFER, secondFBO);
  glBindTexture(GL_TEXTURE_2D, mainFBOColorTid);

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();

  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  //glDisable(GL_STENCIL_TEST);
  glDisable(GL_SCISSOR_TEST);
  glEnable(GL_TEXTURE_2D);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE/*GL_TRUE*/);
  p_glUseProgramObjectARB(0);

  glOrtho(0, ScreenWidth, ScreenHeight, 0, -666, 666);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex2i(0, 0);
    glTexCoord2f(1.0f, 1.0f); glVertex2i(ScreenWidth, 0);
    glTexCoord2f(1.0f, 0.0f); glVertex2i(ScreenWidth, ScreenHeight);
    glTexCoord2f(0.0f, 0.0f); glVertex2i(0, ScreenHeight);
  glEnd();
  glBindTexture(GL_TEXTURE_2D, 0);

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();

  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  glBindFramebuffer(GL_FRAMEBUFFER, mainFBO);
  glPopAttrib();
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
}


//==========================================================================
//
//  VOpenGLDrawer::CreateProgram
//
//==========================================================================
GLhandleARB VOpenGLDrawer::CreateProgram (GLhandleARB VertexShader, GLhandleARB FragmentShader) {
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
}
