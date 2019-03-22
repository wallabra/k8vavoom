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
#include <limits.h>
#include <float.h>
#include <stdarg.h>

#include "gl_local.h"
#include "../r_local.h"


// ////////////////////////////////////////////////////////////////////////// //
VCvarB gl_pic_filtering("gl_pic_filtering", false, "Filter interface pictures.", CVAR_Archive);
VCvarB gl_font_filtering("gl_font_filtering", false, "Filter 2D interface.", CVAR_Archive);

static VCvarB gl_enable_fp_zbuffer("gl_enable_fp_zbuffer", false, "Enable using of floating-point depth buffer for OpenGL3+?", CVAR_Archive|CVAR_PreInit);
static VCvarB gl_enable_reverse_z("gl_enable_reverse_z", true, "Allow using \"reverse z\" trick?", CVAR_Archive|CVAR_PreInit);
static VCvarB gl_enable_clip_control("gl_enable_clip_control", true, "Allow using `glClipControl()`?", CVAR_Archive|CVAR_PreInit);
static VCvarB gl_dbg_force_reverse_z("gl_dbg_force_reverse_z", false, "Force-enable reverse z when fp depth buffer is not available.", CVAR_PreInit);
static VCvarB gl_dbg_ignore_gpu_blacklist("gl_dbg_ignore_gpu_blacklist", false, "Ignore GPU blacklist, and don't turn off features?", CVAR_PreInit);
static VCvarB gl_dbg_force_gpu_blacklisting("gl_dbg_force_gpu_blacklisting", false, "Force GPU to be blacklisted.", CVAR_PreInit);
static VCvarB gl_dbg_disable_depth_clamp("gl_dbg_disable_depth_clamp", false, "Disable depth clamping.", CVAR_PreInit);

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

VCvarB gl_enable_depth_bounds("gl_enable_depth_bounds", true, "Use depth bounds extension if found?", CVAR_Archive);

VCvarB gl_sort_textures("gl_sort_textures", true, "Sort surfaces by their textures (slightly faster on huge levels)?", CVAR_Archive|CVAR_PreInit);

VCvarB r_decals_wall_masked("r_decals_wall_masked", true, "Render decals on masked walls?", CVAR_Archive);
VCvarB r_decals_wall_alpha("r_decals_wall_alpha", true, "Render decals on translucent walls?", CVAR_Archive);

VCvarB r_adv_masked_wall_vertex_light("r_adv_masked_wall_vertex_light", true, "Estimate lighting of masked wall using its vertices?", CVAR_Archive);

VCvarB gl_decal_debug_nostencil("gl_decal_debug_nostencil", false, "Don't touch this!", 0);
VCvarB gl_decal_debug_noalpha("gl_decal_debug_noalpha", false, "Don't touch this!", 0);
VCvarB gl_decal_dump_max("gl_decal_dump_max", false, "Don't touch this!", 0);
VCvarB gl_decal_reset_max("gl_decal_reset_max", false, "Don't touch this!", 0);

VCvarB gl_dbg_adv_render_textures_surface("gl_dbg_adv_render_textures_surface", true, "Render surface textures in advanced renderer?", CVAR_PreInit);
// this makes shadows glitch for some reason with fp z-buffer (investigate!)
VCvarB gl_dbg_adv_render_offset_shadow_volume("gl_dbg_adv_render_offset_shadow_volume", false, "Offset shadow volumes?", CVAR_PreInit);
VCvarB gl_dbg_adv_render_never_offset_shadow_volume("gl_dbg_adv_render_never_offset_shadow_volume", false, "Never offseting shadow volumes?", CVAR_Archive|CVAR_PreInit);

VCvarB gl_dbg_render_stack_portal_bounds("gl_dbg_render_stack_portal_bounds", false, "Render sector stack portal bounds.", 0);

VCvarB gl_use_stencil_quad_clear("gl_use_stencil_quad_clear", false, "Draw quad to clear stencil buffer instead of 'glClear'?", CVAR_Archive|CVAR_PreInit);

// 1: normal; 2: 1-skewed
VCvarI gl_dbg_use_zpass("gl_dbg_use_zpass", "0", "DO NOT USE!", CVAR_PreInit);

VCvarB gl_dbg_advlight_debug("gl_dbg_advlight_debug", false, "Draw non-fading lights?", CVAR_PreInit);
VCvarI gl_dbg_advlight_color("gl_dbg_advlight_color", "0xff7f7f", "Color for debug lights (only dec/hex).", CVAR_PreInit);


//==========================================================================
//
//  MSA
//
//==========================================================================
static __attribute__((sentinel)) TArray<VStr> MSA (const char *first, ...) {
  TArray<VStr> res;
  res.append(VStr(first));
  va_list ap;
  va_start(ap, first);
  for (;;) {
    const char *str = va_arg(ap, const char *);
    if (!str) break;
    res.append(VStr(str));
  }
  return res;
}


//==========================================================================
//
//  CheckVendorString
//
//  both strings should be lower-cased
//  `vs` is what we got from OpenGL
//  `fuckedName` is what we are looking for
//
//==========================================================================
static __attribute__((unused)) bool CheckVendorString (VStr vs, const char *fuckedName) {
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
//  VOpenGLDrawer::glGetUniLoc
//
//==========================================================================
GLint VOpenGLDrawer::glGetUniLoc (const char *prog, GLhandleARB pid, const char *name) {
  check(name);
  if (!pid) Sys_Error("shader program '%s' not loaded", prog);
  (void)glGetError(); // reset error flag
  GLint res = p_glGetUniformLocationARB(pid, name);
  //if (glGetError() != 0 || res == -1) Sys_Error("shader program '%s' has no uniform '%s'", prog, name);
  if (glGetError() != 0 || res == -1) GCon->Logf(NAME_Error, "shader program '%s' has no uniform '%s'", prog, name);
  return res;
}


//==========================================================================
//
//  VOpenGLDrawer::glGetAttrLoc
//
//==========================================================================
GLint VOpenGLDrawer::glGetAttrLoc (const char *prog, GLhandleARB pid, const char *name) {
  check(name);
  if (!pid) Sys_Error("shader program '%s' not loaded", prog);
  (void)glGetError(); // reset error flag
  GLint res = p_glGetAttribLocationARB(pid, name);
  //if (glGetError() != 0 || res == -1) Sys_Error("shader program '%s' has no attribute '%s'", prog, name);
  if (glGetError() != 0 || res == -1) GCon->Logf(NAME_Error, "shader program '%s' has no attribute '%s'", prog, name);
  return res;
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShaderCommonLocs::setupProg
//
//  call this first!
//
//==========================================================================
void VOpenGLDrawer::VGLShaderCommonLocs::setupProg (VOpenGLDrawer *aowner, const char *aprogname, GLhandleARB aprog) {
  //check(!owner);
  //check(!prog);
  check(aowner);
  check(aprog);
  owner = aowner;
  prog = aprog;
  progname = aprogname;
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
  locSAxis = owner->glGetUniLoc(progname, prog, "SAxis");
  locTAxis = owner->glGetUniLoc(progname, prog, "TAxis");
  locSOffs = owner->glGetUniLoc(progname, prog, "SOffs");
  locTOffs = owner->glGetUniLoc(progname, prog, "TOffs");
  locTexIW = owner->glGetUniLoc(progname, prog, "TexIW");
  locTexIH = owner->glGetUniLoc(progname, prog, "TexIH");
  locTexture = owner->glGetUniLoc(progname, prog, "Texture");
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShaderCommonLocs::setupLMapOnly
//
//  setup lightmap variables for shader without texture params (decals)
//
//==========================================================================
void VOpenGLDrawer::VGLShaderCommonLocs::setupLMapOnly () {
  locSAxis = owner->glGetUniLoc(progname, prog, "SAxis");
  locTAxis = owner->glGetUniLoc(progname, prog, "TAxis");
  locSOffs = owner->glGetUniLoc(progname, prog, "SOffs");
  locTOffs = owner->glGetUniLoc(progname, prog, "TOffs");
  locTexMinS = owner->glGetUniLoc(progname, prog, "TexMinS");
  locTexMinT = owner->glGetUniLoc(progname, prog, "TexMinT");
  locCacheS = owner->glGetUniLoc(progname, prog, "CacheS");
  locCacheT = owner->glGetUniLoc(progname, prog, "CacheT");
  locLightMap = owner->glGetUniLoc(progname, prog, "LightMap");
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
  locTexMinS = owner->glGetUniLoc(progname, prog, "TexMinS");
  locTexMinT = owner->glGetUniLoc(progname, prog, "TexMinT");
  locCacheS = owner->glGetUniLoc(progname, prog, "CacheS");
  locCacheT = owner->glGetUniLoc(progname, prog, "CacheT");
  locLightMap = owner->glGetUniLoc(progname, prog, "LightMap");
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShaderCommonLocs::setupFog
//
//  setup fog variables
//
//==========================================================================
void VOpenGLDrawer::VGLShaderCommonLocs::setupFog (bool hasFogEnabled) {
  check(prog);
  if (hasFogEnabled) {
    locFogEnabled = owner->glGetUniLoc(progname, prog, "FogEnabled");
  } else {
    locFogEnabled = -1;
  }
  //locFogType = owner->glGetUniLoc(progname, prog, "FogType");
  locFogColour = owner->glGetUniLoc(progname, prog, "FogColour");
  //locFogDensity = owner->glGetUniLoc(progname, prog, "FogDensity");
  locFogStart = owner->glGetUniLoc(progname, prog, "FogStart");
  locFogEnd = owner->glGetUniLoc(progname, prog, "FogEnd");
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
/*
void VOpenGLDrawer::VGLShaderCommonLocs::storeFogType () {
  check(prog);
  owner->p_glUniform1iARB(locFogType, r_fog&3);
}
*/


//==========================================================================
//
//  VOpenGLDrawer::VGLShaderCommonLocs::storeFogFade
//
//==========================================================================
void VOpenGLDrawer::VGLShaderCommonLocs::storeFogFade (vuint32 Fade, float Alpha) {
  check(prog);
  if (Fade) {
    if (locFogEnabled >= 0) owner->p_glUniform1iARB(locFogEnabled, GL_TRUE);
    owner->p_glUniform4fARB(locFogColour,
      ((Fade>>16)&255)/255.0f,
      ((Fade>>8)&255)/255.0f,
      (Fade&255)/255.0f, Alpha);
    //owner->p_glUniform1fARB(locFogDensity, Fade == FADE_LIGHT ? 0.3f : r_fog_density);
    owner->p_glUniform1fARB(locFogStart, Fade == FADE_LIGHT ? 1.0f : r_fog_start);
    owner->p_glUniform1fARB(locFogEnd, Fade == FADE_LIGHT ? 1024.0f*r_fade_factor : r_fog_end);
  } else {
    if (locFogEnabled >= 0) owner->p_glUniform1iARB(locFogEnabled, GL_FALSE);
  }
}



//==========================================================================
//
//  VOpenGLDrawer::VGLShader::Setup
//
//==========================================================================
void VOpenGLDrawer::VGLShader::MainSetup (VOpenGLDrawer *aowner, const char *aprogname, const char *avssrcfile, const char *afssrcfile) {
  next = nullptr;
  owner = aowner;
  progname = aprogname;
  vssrcfile = avssrcfile;
  fssrcfile = afssrcfile;
  prog = -1;
  owner->registerShader(this);
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShader::Activate
//
//==========================================================================
void VOpenGLDrawer::VGLShader::Activate () {
  owner->p_glUseProgramObjectARB(prog);
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShader::Deactivate
//
//==========================================================================
void VOpenGLDrawer::VGLShader::Deactivate () {
  owner->p_glUseProgramObjectARB(0);
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShader::Compile
//
//==========================================================================
void VOpenGLDrawer::VGLShader::Compile () {
  GCon->Logf(NAME_Init, "compiling shader '%s'...", progname);
  GLhandleARB VertexShader = owner->LoadShader(GL_VERTEX_SHADER_ARB, vssrcfile, defines);
  GLhandleARB FragmentShader = owner->LoadShader(GL_FRAGMENT_SHADER_ARB, fssrcfile, defines);
  prog = owner->CreateProgram(progname, VertexShader, FragmentShader);
  LoadUniforms();
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShader::Unload
//
//==========================================================================
void VOpenGLDrawer::VGLShader::Unload () {
  GCon->Logf(NAME_Init, "unloading shader '%s'...", progname);
}


#include "glz_shaddef.ci"


//==========================================================================
//
//  VOpenGLDrawer::VOpenGLDrawer
//
//==========================================================================
VOpenGLDrawer::VOpenGLDrawer ()
  : VDrawer()
  , shaderHead(nullptr)
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
  stencilBufferDirty = true; // just in case
  isShittyGPU = true; // let's play safe

  surfList = nullptr;
  surfListUsed = surfListSize = 0;
}


//==========================================================================
//
//  VOpenGLDrawer::~VOpenGLDrawer
//
//==========================================================================
VOpenGLDrawer::~VOpenGLDrawer () {
  if (surfList) Z_Free(surfList);
  surfList = nullptr;
  surfListUsed = surfListSize = 0;

  if (tmpImgBuf0) { Z_Free(tmpImgBuf0); tmpImgBuf0 = nullptr; }
  if (tmpImgBuf1) { Z_Free(tmpImgBuf1); tmpImgBuf1 = nullptr; }
  tmpImgBufSize = 0;
  if (readBackTempBuf) { Z_Free(readBackTempBuf); readBackTempBuf = nullptr; }
  readBackTempBufSize = 0;
}


//==========================================================================
//
//  VOpenGLDrawer::registerShader
//
//==========================================================================
void VOpenGLDrawer::registerShader (VGLShader *shader) {
  if (developer) GCon->Logf(NAME_Dev, "registering shader '%s'...", shader->progname);
  shader->owner = this;
  shader->next = shaderHead;
  shaderHead = shader;
}


//==========================================================================
//
//  VOpenGLDrawer::CompileShaders
//
//==========================================================================
void VOpenGLDrawer::CompileShaders () {
  for (VGLShader *shad = shaderHead; shad; shad = shad->next) shad->Compile();
}


void VOpenGLDrawer::DestroyShaders () {
  for (VGLShader *shad = shaderHead; shad; shad = shad->next) shad->Unload();
  shaderHead = nullptr;
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
//  VOpenGLDrawer::DeinitResolution
//
//==========================================================================
void VOpenGLDrawer::DeinitResolution () {
  // unload shaders
  DestroyShaders();

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
  /*
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
  */

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

  p_glClipControl = nullptr;
  if ((major > 4 || (major == 4 && minor >= 5)) || CheckExtension("GL_ARB_clip_control")) {
    p_glClipControl = glClipControl_t(GetExtFuncPtr("glClipControl"));
  }
  if (p_glClipControl) {
    if (gl_enable_clip_control) {
      GCon->Logf(NAME_Init, "OpenGL: `glClipControl()` found");
    } else {
      p_glClipControl = nullptr;
      GCon->Logf(NAME_Init, "OpenGL: `glClipControl()` found, but disabled by user; i shall obey");
    }
  }

  p_glBlitFramebuffer = glBlitFramebuffer_t(GetExtFuncPtr("glBlitFramebuffer"));
  if (p_glBlitFramebuffer) GCon->Logf(NAME_Init, "OpenGL: `glBlitFramebuffer()` found");

  if (!isShittyGPU && p_glClipControl) {
    // normal GPUs
    useReverseZ = true;
    if (!gl_enable_reverse_z) {
      GCon->Logf(NAME_Init, "OpenGL: oops, user disabled reverse z, i shall obey");
      useReverseZ = false;
    }
  } else {
    GCon->Logf(NAME_Init, "OpenGL: reverse z is turned off for your GPU");
    useReverseZ = false;
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
  /*
  {
    GLint stencilBits = 0;
    glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
    GCon->Logf(NAME_Init, "Main stencil buffer depth: %d", stencilBits);
  }
  */

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

  if (!gl_dbg_disable_depth_clamp && CheckExtension("GL_ARB_depth_clamp")) {
    GCon->Log(NAME_Init, "Found GL_ARB_depth_clamp...");
    HaveDepthClamp = true;
  } else if (!gl_dbg_disable_depth_clamp && CheckExtension("GL_NV_depth_clamp")) {
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

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  if (anisotropyExists) glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), 1.0f); // 1 is minimum, i.e. "off"

  // empty texture
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ScreenWidth, ScreenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mainFBOColorTid, 0);

  // attach stencil texture to this FBO
  glGenTextures(1, &mainFBODepthStencilTid);
  if (mainFBODepthStencilTid == 0) Sys_Error("OpenGL: cannot create stencil texture for main FBO");
  glBindTexture(GL_TEXTURE_2D, mainFBODepthStencilTid);

  GLint depthStencilFormat = GL_DEPTH24_STENCIL8;
  // there is (almost) no reason to use fp depth buffer without reverse z
  // besides, stenciled shadows are glitchy for "forward" fp depth buffer (i don't know why, and too lazy to investigate)
  // also, reverse z is perfectly working with int24 depth buffer, see http://www.reedbeta.com/blog/depth-precision-visualized/
  if (major >= 3 && gl_enable_fp_zbuffer) {
    depthStencilFormat = GL_DEPTH32F_STENCIL8;
    GCon->Logf(NAME_Init, "OpenGL: using floating-point depth buffer");
  }

  (void)glGetError();
  /*
  if (!useReverseZ) {
    if (major >= 3 && gl_enable_fp_zbuffer) GCon->Logf(NAME_Init, "OpenGL: using floating-point depth buffer");
    //glTexImage2D(GL_TEXTURE_2D, 0, (major >= 3 && gl_enable_fp_zbuffer ? GL_DEPTH32F_STENCIL8 : GL_DEPTH_STENCIL), ScreenWidth, ScreenHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
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
      GCon->Log(NAME_Init, "OpenGL: cannot create fp depth buffer, trying 24-bit one");
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, ScreenWidth, ScreenHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
      if (glGetError() != 0) Sys_Error("OpenGL initialization error");
    } else {
      Sys_Error("OpenGL initialization error");
    }
  }
  GCon->Logf(NAME_Init, "OpenGL: reverse z is %s", (useReverseZ ? "enabled" : "disabled"));
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

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  if (anisotropyExists) glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), 1.0f); // 1 is minimum, i.e. "off"

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


  // shaders
  shaderHead = nullptr; // just in case

  DrawFixedCol.Setup(this);
  DrawSimple.Setup(this);
  ShadowsModelAmbient.Setup(this);

  CompileShaders();

  GLhandleARB VertexShader, FragmentShader;

  /*
  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/draw_fixed_col.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/draw_fixed_col.fs");
  DrawFixedCol_Program = CreateProgram("DrawFixedCol", VertexShader, FragmentShader);
  DrawFixedCol_ColourLoc = glGetUniLoc("DrawFixedCol", DrawFixedCol_Program, "Colour");
  */


  /*
  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/draw_simple.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/draw_simple.fs");
  DrawSimple_Program = CreateProgram("DrawSimple", VertexShader, FragmentShader);
  DrawSimple_TextureLoc = glGetUniLoc("DrawSimple", DrawSimple_Program, "Texture");
  DrawSimple_AlphaLoc = glGetUniLoc("DrawSimple", DrawSimple_Program, "Alpha");
  */


  // reuses vertex shader
  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/draw_simple.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/draw_shadow.fs");
  DrawShadow_Program = CreateProgram("DrawShadow", VertexShader, FragmentShader);
  DrawShadow_TextureLoc = glGetUniLoc("DrawShadow", DrawShadow_Program, "Texture");
  DrawShadow_AlphaLoc = glGetUniLoc("DrawShadow", DrawShadow_Program, "Alpha");


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/draw_automap.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/draw_automap.fs");
  DrawAutomap_Program = CreateProgram("DrawAutomap", VertexShader, FragmentShader);


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_zbuf.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_zbuf.fs");
  SurfZBuf_Program = CreateProgram("SurfZBuf", VertexShader, FragmentShader);


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_svol.vs");
  // reuse fragment shader
  //FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_svol.fs");
  SurfShadowVolume_Program = CreateProgram("SurfShadowVolume", VertexShader, FragmentShader);
  SurfShadowVolume_LightPosLoc = glGetUniLoc("SurfShadowVolume", SurfShadowVolume_Program, "LightPos");


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_decal_adv.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_decal_adv.fs");
  SurfAdvDecal_Program = CreateProgram("SurfAdvDecal", VertexShader, FragmentShader);
  SurfAdvDecal_TextureLoc = glGetUniLoc("SurfAdvDecal", SurfAdvDecal_Program, "Texture");
  SurfAdvDecal_AmbLightTextureLoc = glGetUniLoc("SurfAdvDecal", SurfAdvDecal_Program, "AmbLightTexture");
  SurfAdvDecal_SplatAlphaLoc = glGetUniLoc("SurfAdvDecal", SurfAdvDecal_Program, "SplatAlpha");
  SurfAdvDecal_FullBright = glGetUniLoc("SurfAdvDecal", SurfAdvDecal_Program, "FullBright");
  SurfAdvDecal_ScreenSize = glGetUniLoc("SurfAdvDecal", SurfAdvDecal_Program, "ScreenSize");


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_decal_nolmap.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_decal_nolmap.fs");
  SurfDecalNoLMap_Program = CreateProgram("SurfDecalNoLMap", VertexShader, FragmentShader);
  SurfDecalNoLMap_TextureLoc = glGetUniLoc("SurfDecalNoLMap", SurfDecalNoLMap_Program, "Texture");
  SurfDecalNoLMap_SplatAlphaLoc = glGetUniLoc("SurfDecalNoLMap", SurfDecalNoLMap_Program, "SplatAlpha");
  SurfDecalNoLMap_LightLoc = glGetUniLoc("SurfDecalNoLMap", SurfDecalNoLMap_Program, "Light");
  SurfDecalNoLMap_Locs.setupProg(this, "SurfDecalNoLMap", SurfDecalNoLMap_Program);
  SurfDecalNoLMap_Locs.setupFog();


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_decal_lmap.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_decal_lmap.fs");
  SurfDecalLMap_Program = CreateProgram("SurfDecalLMap", VertexShader, FragmentShader);
  SurfDecalLMap_TextureLoc = glGetUniLoc("SurfDecalLMap", SurfDecalLMap_Program, "Texture");
  SurfDecalLMap_SplatAlphaLoc = glGetUniLoc("SurfDecalLMap", SurfDecalLMap_Program, "SplatAlpha");
  //!SurfDecalLMap_LightLoc = glGetUniLoc("SurfDecalLMap", SurfDecalLMap_Program, "Light");
  SurfDecalLMap_SpecularMapLoc = glGetUniLoc("SurfDecalLMap", SurfDecalLMap_Program, "SpecularMap");
  SurfDecalLMap_Locs.setupProg(this, "SurfDecalLMap", SurfDecalLMap_Program);
  SurfDecalLMap_Locs.setupLMapOnly();
  SurfDecalLMap_Locs.setupFog();


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_simple.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_simple.fs");
  SurfSimple_Program = CreateProgram("SurfSimple", VertexShader, FragmentShader);
  SurfSimple_LightLoc = glGetUniLoc("SurfSimple", SurfSimple_Program, "Light");
  SurfSimple_Locs.setupProg(this, "SurfSimple", SurfSimple_Program);
  SurfSimple_Locs.setupTexture();
  SurfSimple_Locs.setupFog();


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_lightmap.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_lightmap.fs");
  SurfLightmap_Program = CreateProgram("SurfLightmap", VertexShader, FragmentShader);
  SurfLightmap_SpecularMapLoc = glGetUniLoc("SurfLightmap", SurfLightmap_Program, "SpecularMap");
  SurfLightmap_Locs.setupProg(this, "SurfLightmap", SurfLightmap_Program);
  SurfLightmap_Locs.setupTexture();
  SurfLightmap_Locs.setupLMap();
  SurfLightmap_Locs.setupFog();


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_sky.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_sky.fs");
  SurfSky_Program = CreateProgram("SurfSky", VertexShader, FragmentShader);
  SurfSky_TextureLoc = glGetUniLoc("SurfSky", SurfSky_Program, "Texture");
  SurfSky_BrightnessLoc = glGetUniLoc("SurfSky", SurfSky_Program, "Brightness");
  SurfSky_TexCoordLoc = glGetAttrLoc("SurfSky", SurfSky_Program, "TexCoord");


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_dsky.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_dsky.fs");
  SurfDSky_Program = CreateProgram("SurfDSky", VertexShader, FragmentShader);
  SurfDSky_TextureLoc = glGetUniLoc("SurfDSky", SurfDSky_Program, "Texture");
  SurfDSky_Texture2Loc = glGetUniLoc("SurfDSky", SurfDSky_Program, "Texture2");
  SurfDSky_BrightnessLoc = glGetUniLoc("SurfDSky", SurfDSky_Program, "Brightness");
  SurfDSky_TexCoordLoc = glGetAttrLoc("SurfDSky", SurfDSky_Program, "TexCoord");
  SurfDSky_TexCoord2Loc = glGetAttrLoc("SurfDSky", SurfDSky_Program, "TexCoord2");


#define GLSL_LOADLOC(lcname_)  SurfMasked_ ## lcname_ ## Loc  = glGetUniLoc("SurfMasked", SurfMasked_Program, "" #lcname_)
#define GLSL_LOADATR(lcname_)  SurfMasked_ ## lcname_ ## Loc  = glGetAttrLoc("SurfMasked", SurfMasked_Program, "" #lcname_)
  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_masked.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_masked.fs");
  SurfMasked_Program = CreateProgram("SurfMasked", VertexShader, FragmentShader);
  GLSL_LOADLOC(Texture);
  GLSL_LOADLOC(Light);
  GLSL_LOADLOC(AlphaRef);
  GLSL_LOADATR(TexCoord);
  SurfMasked_Locs.setupProg(this, "SurfMasked", SurfMasked_Program);
  SurfMasked_Locs.setupFog();
#undef GLSL_LOADATR
#undef GLSL_LOADLOC


#define GLSL_LOADLOC(lcname_)  SurfModel_ ## lcname_ ## Loc  = glGetUniLoc("SurfModel", SurfModel_Program, "" #lcname_)
#define GLSL_LOADATR(lcname_)  SurfModel_ ## lcname_ ## Loc  = glGetAttrLoc("SurfModel", SurfModel_Program, "" #lcname_)
  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/surf_model.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/surf_model.fs");
  SurfModel_Program = CreateProgram("SurfModel", VertexShader, FragmentShader);
  GLSL_LOADLOC(Inter);
  GLSL_LOADLOC(Texture);
  GLSL_LOADATR(Vert2);
  GLSL_LOADATR(TexCoord);
  GLSL_LOADLOC(InAlpha);
  GLSL_LOADATR(LightVal);
  GLSL_LOADLOC(ViewOrigin);
  GLSL_LOADLOC(AllowTransparency);
  SurfModel_Locs.setupProg(this, "SurfModel", SurfModel_Program);
  SurfModel_Locs.setupFog();
#undef GLSL_LOADATR
#undef GLSL_LOADLOC


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/particle.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/particle_sq.fs");
  SurfPartSq_Program = CreateProgram("SurfPartSq", VertexShader, FragmentShader);
  SurfPartSq_TexCoordLoc = glGetAttrLoc("SurfPartSq", SurfPartSq_Program, "TexCoord");
  SurfPartSq_LightValLoc = glGetAttrLoc("SurfPartSq", SurfPartSq_Program, "LightVal");


  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/particle_sm.fs");
  SurfPartSm_Program = CreateProgram("SurfPartSm", VertexShader, FragmentShader);
  SurfPartSm_TexCoordLoc = glGetAttrLoc("SurfPartSm", SurfPartSm_Program, "TexCoord");
  SurfPartSm_LightValLoc = glGetAttrLoc("SurfPartSm", SurfPartSm_Program, "LightVal");


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_surf_ambient.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_surf_ambient.fs");
  ShadowsAmbient_Program = CreateProgram("ShadowsAmbient", VertexShader, FragmentShader);
  ShadowsAmbient_LightLoc = glGetUniLoc("ShadowsAmbient", ShadowsAmbient_Program, "Light");
  ShadowsAmbient_Locs.setupProg(this, "ShadowsAmbient", ShadowsAmbient_Program);
  ShadowsAmbient_Locs.setupTexture();


#define GLSL_LOADLOC(lcname_)  ShadowsLight_ ## lcname_ ## Loc  = glGetUniLoc("ShadowsLight", ShadowsLight_Program, "" #lcname_)
#define GLSL_LOADATR(lcname_)  ShadowsLight_ ## lcname_ ## Loc  = glGetAttrLoc("ShadowsLight", ShadowsLight_Program, "" #lcname_)
  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_surf_light.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_surf_light.fs");
  ShadowsLight_Program = CreateProgram("ShadowsLight", VertexShader, FragmentShader);
  GLSL_LOADLOC(LightPos);
  GLSL_LOADLOC(LightRadius);
  GLSL_LOADLOC(LightColour);
  GLSL_LOADATR(SurfNormal);
  GLSL_LOADATR(SurfDist);
  GLSL_LOADLOC(ViewOrigin);
  ShadowsLight_Locs.setupProg(this, "ShadowsLight", ShadowsLight_Program);
  ShadowsLight_Locs.setupTexture();
#undef GLSL_LOADATR
#undef GLSL_LOADLOC


#define GLSL_LOADLOC(lcname_)  ShadowsLightDbg_ ## lcname_ ## Loc  = glGetUniLoc("ShadowsLight", ShadowsLightDbg_Program, "" #lcname_)
#define GLSL_LOADATR(lcname_)  ShadowsLightDbg_ ## lcname_ ## Loc  = glGetAttrLoc("ShadowsLight", ShadowsLightDbg_Program, "" #lcname_)
  // reuse vertex shader
  //VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_surf_light.vs", MSA("VV_DEBUG_LIGHT", nullptr));
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_surf_light.fs", MSA("VV_DEBUG_LIGHT", nullptr));
  ShadowsLightDbg_Program = CreateProgram("ShadowsLightDbg", VertexShader, FragmentShader);
  GLSL_LOADLOC(LightPos);
  GLSL_LOADLOC(LightRadius);
  GLSL_LOADLOC(LightColour);
  GLSL_LOADATR(SurfNormal);
  GLSL_LOADATR(SurfDist);
  GLSL_LOADLOC(ViewOrigin);
  ShadowsLightDbg_Locs.setupProg(this, "ShadowsLightDbg", ShadowsLightDbg_Program);
  ShadowsLightDbg_Locs.setupTexture();
#undef GLSL_LOADATR
#undef GLSL_LOADLOC


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_surf_texture.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_surf_texture.fs");
  ShadowsTexture_Program = CreateProgram("ShadowsTexture", VertexShader, FragmentShader);
  ShadowsTexture_Locs.setupProg(this, "ShadowsTexture", ShadowsTexture_Program);
  ShadowsTexture_Locs.setupTexture();


#define GLSL_LOADLOC(lcname_)  ShadowsModelAmbient_ ## lcname_ ## Loc  = glGetUniLoc("ShadowsModelAmbient", ShadowsModelAmbient_Program, "" #lcname_)
#define GLSL_LOADATR(lcname_)  ShadowsModelAmbient_ ## lcname_ ## Loc  = glGetAttrLoc("ShadowsModelAmbient", ShadowsModelAmbient_Program, "" #lcname_)
  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_model_ambient.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_model_ambient.fs");
  ShadowsModelAmbient_Program = CreateProgram("ShadowsModelAmbient", VertexShader, FragmentShader);
  GLSL_LOADLOC(Inter);
  GLSL_LOADLOC(Texture);
  GLSL_LOADLOC(Light);
  GLSL_LOADLOC(ModelToWorldMat);
  GLSL_LOADLOC(NormalToWorldMat);
  GLSL_LOADATR(Vert2);
  GLSL_LOADATR(VertNormal);
  GLSL_LOADATR(Vert2Normal);
  GLSL_LOADATR(TexCoord);
  GLSL_LOADLOC(InAlpha);
  GLSL_LOADLOC(ViewOrigin);
  GLSL_LOADLOC(AllowTransparency);
#undef GLSL_LOADATR
#undef GLSL_LOADLOC


#define GLSL_LOADLOC(lcname_)  ShadowsModelTextures_ ## lcname_ ## Loc  = glGetUniLoc("ShadowsModelTextures", ShadowsModelTextures_Program, "" #lcname_)
#define GLSL_LOADATR(lcname_)  ShadowsModelTextures_ ## lcname_ ## Loc  = glGetAttrLoc("ShadowsModelTextures", ShadowsModelTextures_Program, "" #lcname_)
  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_model_textures.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_model_textures.fs");
  ShadowsModelTextures_Program = CreateProgram("ShadowsModelTextures", VertexShader, FragmentShader);
  GLSL_LOADLOC(Inter);
  GLSL_LOADLOC(Texture);
  GLSL_LOADLOC(ModelToWorldMat);
  //GLSL_LOADLOC(NormalToWorldMat);
  GLSL_LOADATR(Vert2);
  GLSL_LOADATR(TexCoord);
  //GLSL_LOADATR(VertNormal);
  //GLSL_LOADATR(Vert2Normal);
  GLSL_LOADLOC(InAlpha);
  GLSL_LOADLOC(ViewOrigin);
  //GLSL_LOADLOC(AllowTransparency);
  GLSL_LOADLOC(AmbLightTexture);
  GLSL_LOADLOC(ScreenSize);
#undef GLSL_LOADATR
#undef GLSL_LOADLOC

#define GLSL_LOADLOC(lcname_)  ShadowsModelLight_ ## lcname_ ## Loc  = glGetUniLoc("ShadowsModelLight", ShadowsModelLight_Program, "" #lcname_)
#define GLSL_LOADATR(lcname_)  ShadowsModelLight_ ## lcname_ ## Loc  = glGetAttrLoc("ShadowsModelLight", ShadowsModelLight_Program, "" #lcname_)
  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_model_light.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_model_light.fs");
  ShadowsModelLight_Program = CreateProgram("ShadowsModelLight", VertexShader, FragmentShader);
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


#define GLSL_LOADLOC(lcname_)  ShadowsModelShadow_ ## lcname_ ## Loc  = glGetUniLoc("ShadowsModelShadow", ShadowsModelShadow_Program, "" #lcname_)
#define GLSL_LOADATR(lcname_)  ShadowsModelShadow_ ## lcname_ ## Loc  = glGetAttrLoc("ShadowsModelShadow", ShadowsModelShadow_Program, "" #lcname_)
  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_model_shadow.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_model_shadow.fs");
  ShadowsModelShadow_Program = CreateProgram("ShadowsModelShadow", VertexShader, FragmentShader);
  GLSL_LOADLOC(Inter);
  GLSL_LOADLOC(LightPos);
  GLSL_LOADLOC(ModelToWorldMat);
  GLSL_LOADATR(Vert2);
  GLSL_LOADATR(Offset);
  //GLSL_LOADLOC(ViewOrigin);
#undef GLSL_LOADATR
#undef GLSL_LOADLOC


  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_surf_fog.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_surf_fog.fs");
  ShadowsFog_Program = CreateProgram("ShadowsFog", VertexShader, FragmentShader);
  ShadowsFog_Locs.setupProg(this, "ShadowsFog", ShadowsFog_Program);
  ShadowsFog_Locs.setupFog(false);


#define GLSL_LOADLOC(lcname_)  ShadowsModelFog_ ## lcname_ ## Loc  = glGetUniLoc("ShadowsModelFog", ShadowsModelFog_Program, "" #lcname_)
#define GLSL_LOADATR(lcname_)  ShadowsModelFog_ ## lcname_ ## Loc  = glGetAttrLoc("ShadowsModelFog", ShadowsModelFog_Program, "" #lcname_)
  VertexShader = LoadShader(GL_VERTEX_SHADER_ARB, "glshaders/shadows_model_fog.vs");
  FragmentShader = LoadShader(GL_FRAGMENT_SHADER_ARB, "glshaders/shadows_model_fog.fs");
  ShadowsModelFog_Program = CreateProgram("ShadowsModelFog", VertexShader, FragmentShader);
  GLSL_LOADLOC(Inter);
  GLSL_LOADLOC(ModelToWorldMat);
  GLSL_LOADLOC(Texture);
  //GLSL_LOADLOC(FogType);
  GLSL_LOADLOC(FogColour);
  //GLSL_LOADLOC(FogDensity);
  GLSL_LOADLOC(FogStart);
  GLSL_LOADLOC(FogEnd);
  GLSL_LOADATR(Vert2);
  GLSL_LOADATR(TexCoord);
  GLSL_LOADLOC(InAlpha);
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
  return (HaveStencilWrap && p_glStencilFuncSeparate && HaveDrawRangeElements && HaveDepthClamp);
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
  //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  // turn off anisotropy
  //glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), 1); // 1 is minimum, i.e. "off"

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
    if (p_glBlitFramebuffer) {
      glBindTexture(GL_TEXTURE_2D, 0);

      glBindFramebuffer(GL_READ_FRAMEBUFFER, mainFBO);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // default FBO

      int realw, realh;
      GetRealWindowSize(&realw, &realh);

      if (realw == ScreenWidth && realh == ScreenHeight) {
        glViewport(0, 0, ScreenWidth, ScreenHeight);
        p_glBlitFramebuffer(0, 0, ScreenWidth, ScreenHeight, 0, 0, ScreenWidth, ScreenHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
      } else {
        glViewport(0, 0, realw, realh);
        p_glBlitFramebuffer(0, 0, ScreenWidth, ScreenHeight, 0, 0, realw, realh, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glViewport(0, 0, ScreenWidth, ScreenHeight);
      }

      glOrtho(0, ScreenWidth, ScreenHeight, 0, -666, 666);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
    } else {
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
//  VOpenGLDrawer::SetupLightScissor
//
//  returns:
//   0 if scissor is empty
//  -1 if scissor has no sense (should not be used)
//   1 if scissor is set
//
//==========================================================================
int VOpenGLDrawer::SetupLightScissor (const TVec &org, float radius, int scoord[4], const TVec *geobbox) {
  int tmpscoord[4];
  VMatrix4 pmat, mmat;
  glGetFloatv(GL_PROJECTION_MATRIX, pmat[0]);
  glGetFloatv(GL_MODELVIEW_MATRIX, mmat[0]);

  if (!scoord) scoord = tmpscoord;

  if (radius < 4) {
    scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
    glScissor(0, 0, 0, 0);
    return 0;
  }

  // transform into world coords
  TVec inworld = mmat*org;

  // the thing that should not be (completely behind)
  if (inworld.z-radius > -1.0f) {
    scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
    glDisable(GL_SCISSOR_TEST);
    return 0;
  }

  // create light bbox
  float bbox[6];
  bbox[0+0] = inworld.x-radius;
  bbox[0+1] = inworld.y-radius;
  bbox[0+2] = inworld.z-radius;

  bbox[3+0] = inworld.x+radius;
  bbox[3+1] = inworld.y+radius;
  bbox[3+2] = MIN(-1.0f, inworld.z+radius); // clamp to znear

  // clamp it with geometry bbox, if there is any
#if 1
  if (geobbox) {
    float gbb[6];
    gbb[0] = geobbox[0].x;
    gbb[1] = geobbox[0].y;
    gbb[2] = geobbox[0].z;
    gbb[3] = geobbox[1].x;
    gbb[4] = geobbox[1].y;
    gbb[5] = geobbox[1].z;
    float trbb[6] = { FLT_MAX, FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (unsigned f = 0; f < 8; ++f) {
      TVec vtx = mmat*TVec(gbb[BBoxVertexIndex[f][0]], gbb[BBoxVertexIndex[f][1]], gbb[BBoxVertexIndex[f][2]]);
      trbb[0] = MIN(trbb[0], vtx.x);
      trbb[1] = MIN(trbb[1], vtx.y);
      trbb[2] = MIN(trbb[2], vtx.z);
      trbb[3] = MAX(trbb[3], vtx.x);
      trbb[4] = MAX(trbb[4], vtx.y);
      trbb[5] = MAX(trbb[5], vtx.z);
    }

    if (trbb[0] >= trbb[3+0] || trbb[1] >= trbb[3+1] || trbb[2] >= trbb[3+2]) {
      scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
      glDisable(GL_SCISSOR_TEST);
      return 0;
    }

    trbb[2] = MIN(-1.0f, trbb[2]);
    trbb[5] = MIN(-1.0f, trbb[5]);

    /*
    if (trbb[0] > bbox[0] || trbb[1] > bbox[1] || trbb[2] > bbox[2] ||
        trbb[3] < bbox[3] || trbb[4] < bbox[4] || trbb[5] < bbox[5])
    {
      GCon->Logf("GEOCLAMP: (%f,%f,%f)-(%f,%f,%f) : (%f,%f,%f)-(%f,%f,%f)", bbox[0], bbox[1], bbox[2], bbox[3], bbox[4], bbox[5], trbb[0], trbb[1], trbb[2], trbb[3], trbb[4], trbb[5]);
    }
    */

    bbox[0] = MAX(bbox[0], trbb[0]);
    bbox[1] = MAX(bbox[1], trbb[1]);
    bbox[2] = MAX(bbox[2], trbb[2]);
    bbox[3] = MIN(bbox[3], trbb[3]);
    bbox[4] = MIN(bbox[4], trbb[4]);
    bbox[5] = MIN(bbox[5], trbb[5]);
    if (bbox[0] >= bbox[3+0] || bbox[1] >= bbox[3+1] || bbox[2] >= bbox[3+2]) {
      scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
      glDisable(GL_SCISSOR_TEST);
      return 0;
    }

    /*
    TVec bc0 = mmat*geobbox[0];
    TVec bc1 = mmat*geobbox[1];
    TVec bmin = TVec(MIN(bc0.x, bc1.x), MIN(bc0.y, bc1.y), MIN(-1.0f, MIN(bc0.z, bc1.z)));
    TVec bmax = TVec(MAX(bc0.x, bc1.x), MAX(bc0.y, bc1.y), MIN(-1.0f, MAX(bc0.z, bc1.z)));
    if (bmin.x > bbox[0] || bmin.y > bbox[1] || bmin.z > bbox[2] ||
        bmax.x < bbox[3] || bmax.y < bbox[4] || bmax.z < bbox[5])
    {
      GCon->Logf("GEOCLAMP: (%f,%f,%f)-(%f,%f,%f) : (%f,%f,%f)-(%f,%f,%f)", bbox[0], bbox[1], bbox[2], bbox[3], bbox[4], bbox[5], bmin.x, bmin.y, bmin.z, bmax.x, bmax.y, bmax.z);
    }
    bbox[0] = MAX(bbox[0], bmin.x);
    bbox[1] = MAX(bbox[1], bmin.y);
    bbox[2] = MAX(bbox[2], bmin.z);
    bbox[3] = MIN(bbox[3], bmax.x);
    bbox[4] = MIN(bbox[4], bmax.y);
    bbox[5] = MIN(bbox[5], bmax.z);
    if (bbox[0] >= bbox[3+0] || bbox[1] >= bbox[3+1] || bbox[2] >= bbox[3+2]) {
      scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
      glDisable(GL_SCISSOR_TEST);
      return 0;
    }
    */
  }
#endif

  // setup depth bounds
  if (hasBoundsTest && gl_enable_depth_bounds) {
    const bool zeroZ = (gl_enable_clip_control && p_glClipControl);
    const bool revZ = CanUseRevZ();

    //const float ofsz0 = MIN(-1.0f, inworld.z+radius);
    //const float ofsz1 = inworld.z-radius;
    const float ofsz0 = bbox[5];
    const float ofsz1 = bbox[2];
    check(ofsz1 <= -1.0f);

    float pjwz0 = -1.0f/ofsz0;
    float pjwz1 = -1.0f/ofsz1;

    // for reverse z, projz is always 1, so we can simply use pjw
    if (!revZ) {
      pjwz0 *= pmat.Transform2OnlyZ(TVec(inworld.x, inworld.y, ofsz0));
      pjwz1 *= pmat.Transform2OnlyZ(TVec(inworld.x, inworld.y, ofsz1));
    }

    // transformation for [-1..1] z range
    if (!zeroZ) {
      pjwz0 = (1.0f+pjwz0)*0.5f;
      pjwz1 = (1.0f+pjwz1)*0.5f;
    }

    if (revZ) {
      p_glDepthBoundsEXT(pjwz1, pjwz0);
    } else {
      p_glDepthBoundsEXT(pjwz0, pjwz1);
    }
    glEnable(GL_DEPTH_BOUNDS_TEST_EXT);
  }

  GLint vport[4];
  glGetIntegerv(GL_VIEWPORT, vport);
  if (vport[2] < 1 || vport[3] < 1) {
    scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
    glDisable(GL_SCISSOR_TEST);
    return 0;
  }
  //GCon->Logf("vport: (%d,%d)-(%d,%d)", vport[0], vport[1], vport[2], vport[3]);

  int scrx1 = vport[0]+vport[2]-1;
  int scry1 = vport[1]+vport[3]-1;

  const float scrw = vport[2]*0.5f;
  const float scrh = vport[3]*0.5f;

  int minx = scrx1+64, miny = scry1+64;
  int maxx = -(vport[0]-64), maxy = -(vport[1]-64);

  // transform points, get min and max
  for (unsigned f = 0; f < 8; ++f) {
    TVec vtx = TVec(bbox[BBoxVertexIndex[f][0]], bbox[BBoxVertexIndex[f][1]], bbox[BBoxVertexIndex[f][2]]);
    TVec proj = pmat.Transform2OnlyXY(vtx); // we don't care about z here
    const float pjw = -1.0f/vtx.z;
    proj.x *= pjw;
    proj.y *= pjw;
    int winx = vport[0]+(int)((1.0f+proj.x)*scrw);
    int winy = vport[1]+(int)((1.0f+proj.y)*scrh);
    //GCon->Logf("x=%f; y=%f; win=(%d,%d)", proj.x, proj.y, winx, winy);

    if (minx > winx) minx = winx;
    if (miny > winy) miny = winy;
    if (maxx < winx) maxx = winx;
    if (maxy < winy) maxy = winy;
  }

#if 0
  //GCon->Logf("  radius=%f; (%d,%d)-(%d,%d)", radius, minx, miny, maxx, maxy);
  if (minx >= ScreenWidth || miny >= ScreenHeight || maxx < 0 || maxy < 0) {
    scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
    glDisable(GL_SCISSOR_TEST);
    if (hasBoundsTest && gl_enable_depth_bounds) glDisable(GL_DEPTH_BOUNDS_TEST_EXT);
    return 0;
  }

  minx = MID(0, minx, ScreenWidth-1);
  miny = MID(0, miny, ScreenHeight-1);
  maxx = MID(0, maxx, ScreenWidth-1);
  maxy = MID(0, maxy, ScreenHeight-1);
#else
  if (minx > scrx1 || miny > scry1 || maxx < vport[0] || maxy < vport[1]) {
    scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
    glDisable(GL_SCISSOR_TEST);
    if (hasBoundsTest && gl_enable_depth_bounds) glDisable(GL_DEPTH_BOUNDS_TEST_EXT);
    return 0;
  }

  minx = MID(vport[0], minx, scrx1);
  miny = MID(vport[1], miny, scry1);
  maxx = MID(vport[0], maxx, scrx1);
  maxy = MID(vport[1], maxy, scry1);
#endif

  /*
  int cx = (minx+maxx)/2;
  int cy = (minx+maxx)/2;
  minx = cx-32;
  miny = cy-32;
  maxx = cx+32;
  maxy = cy+32;
  */

  //GCon->Logf("  radius=%f; (%d,%d)-(%d,%d)", radius, minx, miny, maxx, maxy);
  const int wdt = maxx-minx+1;
  const int hgt = maxy-miny+1;

  // drop very small lights, why not?
  if (wdt <= 4 || hgt <= 4) {
    scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
    glDisable(GL_SCISSOR_TEST);
    if (hasBoundsTest && gl_enable_depth_bounds) glDisable(GL_DEPTH_BOUNDS_TEST_EXT);
    return 0;
  }

  glEnable(GL_SCISSOR_TEST);
  glScissor(minx, miny, wdt, hgt);
  scoord[0] = minx;
  scoord[1] = miny;
  scoord[2] = maxx;
  scoord[3] = maxy;

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

  VMatrix4 ProjMat;
  if (!CanUseRevZ()) {
    // normal
    glClearDepth(1.0f);
    glDepthFunc(GL_LEQUAL);
    ProjMat.SetIdentity();
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
    // see https://nlguillemot.wordpress.com/2016/12/07/reversed-z-in-opengl/
    glClearDepth(0.0f);
    glDepthFunc(GL_GEQUAL);
    ProjMat.SetZero();
    ProjMat[0][0] = 1.0f/rd->fovx;
    ProjMat[1][1] = 1.0f/rd->fovy;
    ProjMat[2][3] = -1.0f;
    ProjMat[3][2] = 1.0f; // zNear
  }
  //RestoreDepthFunc();

  glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
  stencilBufferDirty = false;

  glViewport(rd->x, ScreenHeight-rd->height-rd->y, rd->width, rd->height);

  glMatrixMode(GL_PROJECTION);
  glLoadMatrixf(ProjMat[0]);

  glMatrixMode(GL_MODELVIEW);

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_FRONT);

  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_ALPHA_TEST);
  if (RendLev && RendLev->NeedsInfiniteFarClip && HaveDepthClamp) glEnable(GL_DEPTH_CLAMP);
  //k8: there is no reason to not do it
  //if (HaveDepthClamp) glEnable(GL_DEPTH_CLAMP);
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
    DrawFixedCol.Activate();
    DrawFixedCol.SetColour(
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
    //static GLenum fogMode[4] = { GL_LINEAR, GL_LINEAR, GL_EXP, GL_EXP2 };
    float fogColour[4];

    fogColour[0] = float((NewFade>>16)&255)/255.0f;
    fogColour[1] = float((NewFade>>8)&255)/255.0f;
    fogColour[2] = float(NewFade&255)/255.0f;
    fogColour[3] = float((NewFade>>24)&255)/255.0f;
    //glFogi(GL_FOG_MODE, fogMode[r_fog&3]);
    glFogi(GL_FOG_MODE, GL_LINEAR);
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
    //glHint(GL_FOG_HINT, r_fog < 4 ? GL_DONT_CARE : GL_NICEST);
    glHint(GL_FOG_HINT, GL_DONT_CARE);
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

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    if (anisotropyExists) glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), 1.0f);

    // empty texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ScreenWidth, ScreenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, secondFBOColorTid, 0);
  }


  if (p_glBlitFramebuffer) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, mainFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, secondFBO);
    p_glBlitFramebuffer(0, 0, ScreenWidth, ScreenHeight, 0, 0, ScreenWidth, ScreenHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, mainFBO);
  } else {
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
}


//==========================================================================
//
//  VOpenGLDrawer::DebugRenderScreenRect
//
//==========================================================================
void VOpenGLDrawer::DebugRenderScreenRect (int x0, int y0, int x1, int y1, vuint32 color) {
  glPushAttrib(/*GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_ENABLE_BIT|GL_VIEWPORT_BIT|GL_TRANSFORM_BIT*/GL_ALL_ATTRIB_BITS);

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();

  //glColor4f(((color>>16)&0xff)/255.0f, ((color>>8)&0xff)/255.0f, (color&0xff)/255.0f, ((color>>24)&0xff)/255.0f);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  //glDisable(GL_STENCIL_TEST);
  //glDisable(GL_SCISSOR_TEST);
  glDisable(GL_TEXTURE_2D);
  glDepthMask(GL_FALSE); // no z-buffer writes
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  //p_glUseProgramObjectARB(0);

  DrawFixedCol.Activate();
  DrawFixedCol.SetColour(
    (GLfloat)(((color>>16)&255)/255.0f),
    (GLfloat)(((color>>8)&255)/255.0f),
    (GLfloat)((color&255)/255.0f), ((color>>24)&0xff)/255.0f);

  glOrtho(0, ScreenWidth, ScreenHeight, 0, -666, 666);
  glBegin(GL_QUADS);
    glVertex2i(x0, y0);
    glVertex2i(x1, y0);
    glVertex2i(x1, y1);
    glVertex2i(x0, y1);
  glEnd();

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();

  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  glPopAttrib();
  p_glUseProgramObjectARB(0);
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
GLhandleARB VOpenGLDrawer::LoadShader (GLenum Type, const VStr &FileName, const TArray<VStr> &defines) {
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
        // add other defines
        for (int didx = 0; didx < defines.length(); ++didx) {
          const VStr &def = defines[didx];
          if (def.isEmpty()) continue;
          res += "#define ";
          res += def;
          res += "\n";
        }
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
  //if (defines.length()) GCon->Logf("%s", *res);
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
GLhandleARB VOpenGLDrawer::CreateProgram (const char *progname, GLhandleARB VertexShader, GLhandleARB FragmentShader) {
  // create program object
  (void)glGetError();
  GLhandleARB Program = p_glCreateProgramObjectARB();
  if (!Program) Sys_Error("Failed to create program object");
  CreatedShaderObjects.Append(Program);

  // attach shaders
  p_glAttachObjectARB(Program, VertexShader);
  p_glAttachObjectARB(Program, FragmentShader);

  // link program
  (void)glGetError();
  p_glLinkProgramARB(Program);

  // check if it was linked successfully
  GLint Status;
  p_glGetObjectParameterivARB(Program, GL_OBJECT_LINK_STATUS_ARB, &Status);
  if (!Status) {
    GLcharARB LogText[1024];
    GLsizei LogLen;
    p_glGetInfoLogARB(Program, sizeof(LogText)-1, &LogLen, LogText);
    LogText[LogLen] = 0;
    Sys_Error("Failed to link program '%s'", LogText);
  }

  if (glGetError() != 0) Sys_Error("Failed to link program '%s' for unknown reason", progname);

  return Program;
}
