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
//**
//**  OpenGL driver, main module
//**
//**************************************************************************
// not done yet
#define VV_SHADER_COMPILING_PROGRESS

#include <limits.h>
#include <float.h>
#include <stdarg.h>

#include "gl_local.h"
#include "../r_local.h" /* for VRenderLevelShared */

#ifdef VV_SHADER_COMPILING_PROGRESS
#include "splashfont.inc"
#endif


// ////////////////////////////////////////////////////////////////////////// //
extern VCvarB r_bloom;

VCvarB gl_pic_filtering("gl_pic_filtering", false, "Filter interface pictures.", CVAR_Archive);
VCvarB gl_font_filtering("gl_font_filtering", false, "Filter 2D interface.", CVAR_Archive);

VCvarB gl_enable_clip_control("gl_enable_clip_control", true, "Allow using `glClipControl()`?", CVAR_Archive|CVAR_PreInit);
static VCvarB gl_enable_reverse_z("gl_enable_reverse_z", true, "Allow using \"reverse z\" trick?", CVAR_Archive|CVAR_PreInit);
static VCvarB gl_dbg_force_reverse_z("gl_dbg_force_reverse_z", false, "Force-enable reverse z when fp depth buffer is not available.", CVAR_PreInit);
static VCvarB gl_dbg_ignore_gpu_blacklist("gl_dbg_ignore_gpu_blacklist", false, "Ignore GPU blacklist, and don't turn off features?", CVAR_PreInit);
static VCvarB gl_dbg_force_gpu_blacklisting("gl_dbg_force_gpu_blacklisting", false, "Force GPU to be blacklisted.", CVAR_PreInit);
static VCvarB gl_dbg_disable_depth_clamp("gl_dbg_disable_depth_clamp", false, "Disable depth clamping.", CVAR_PreInit);

VCvarB gl_letterbox("gl_letterbox", true, "Use letterbox for scaled FS mode?", CVAR_Archive);
VCvarI gl_letterbox_filter("gl_letterbox_filter", "0", "Image filtering for letterbox mode (0:nearest; 1:linear).", CVAR_Archive);
VCvarS gl_letterbox_color("gl_letterbox_color", "00 00 00", "Letterbox color", CVAR_Archive);
VCvarF gl_letterbox_scale("gl_letterbox_scale", "1", "Letterbox scaling factor in range (0..1].", CVAR_Archive);

VCvarI VOpenGLDrawer::texture_filter("gl_texture_filter", "0", "Texture filtering mode.", CVAR_Archive);
VCvarI VOpenGLDrawer::sprite_filter("gl_sprite_filter", "0", "Sprite filtering mode.", CVAR_Archive);
VCvarI VOpenGLDrawer::model_filter("gl_model_filter", "0", "Model filtering mode.", CVAR_Archive);
VCvarI VOpenGLDrawer::gl_texture_filter_anisotropic("gl_texture_filter_anisotropic", "1", "Texture anisotropic filtering (<=1 is off).", CVAR_Archive);
VCvarB VOpenGLDrawer::clear("gl_clear", true, "Clear screen before rendering new frame?", CVAR_Archive);
VCvarB VOpenGLDrawer::ext_anisotropy("gl_ext_anisotropy", true, "Use OpenGL anisotropy extension (if present)?", CVAR_Archive|CVAR_PreInit);
VCvarI VOpenGLDrawer::multisampling_sample("gl_multisampling_sample", "1", "Multisampling mode.", CVAR_Archive);
VCvarB VOpenGLDrawer::gl_smooth_particles("gl_smooth_particles", false, "Draw smooth particles?", CVAR_Archive);

VCvarB VOpenGLDrawer::gl_dump_vendor("gl_dump_vendor", false, "Dump OpenGL vendor?", CVAR_PreInit);
VCvarB VOpenGLDrawer::gl_dump_extensions("gl_dump_extensions", false, "Dump available OpenGL extensions?", CVAR_PreInit);

// was 0.333
VCvarF gl_alpha_threshold("gl_alpha_threshold", "0.01", "Alpha threshold (less than this will not be drawn).", CVAR_Archive);

static VCvarI gl_max_anisotropy("gl_max_anisotropy", "1", "Maximum anisotropy level (r/o).", CVAR_Rom);
static VCvarB gl_is_shitty_gpu("gl_is_shitty_gpu", true, "Is shitty GPU detected (r/o)?", CVAR_Rom);

VCvarB gl_enable_depth_bounds("gl_enable_depth_bounds", true, "Use depth bounds extension if found?", CVAR_Archive);

VCvarB gl_sort_textures("gl_sort_textures", true, "Sort surfaces by their textures (slightly faster on huge levels; affects only lightmapped renderer)?", CVAR_Archive|CVAR_PreInit);

VCvarB r_decals_wall_masked("r_decals_wall_masked", true, "Render decals on masked walls?", CVAR_Archive);
VCvarB r_decals_wall_alpha("r_decals_wall_alpha", true, "Render decals on translucent walls?", CVAR_Archive);

VCvarB gl_decal_debug_nostencil("gl_decal_debug_nostencil", false, "Don't touch this!", 0);
VCvarB gl_decal_debug_noalpha("gl_decal_debug_noalpha", false, "Don't touch this!", 0);
VCvarB gl_decal_dump_max("gl_decal_dump_max", false, "Don't touch this!", 0);
VCvarB gl_decal_reset_max("gl_decal_reset_max", false, "Don't touch this!", 0);

VCvarB gl_dbg_adv_render_surface_textures("gl_dbg_adv_render_surface_textures", true, "Render surface textures in advanced renderer?", CVAR_PreInit);
VCvarB gl_dbg_adv_render_surface_fog("gl_dbg_adv_render_surface_fog", true, "Render surface fog in advanced renderer?", CVAR_PreInit);

VCvarB gl_dbg_render_stack_portal_bounds("gl_dbg_render_stack_portal_bounds", false, "Render sector stack portal bounds.", 0);

VCvarB gl_use_stencil_quad_clear("gl_use_stencil_quad_clear", false, "Draw quad to clear stencil buffer instead of 'glClear'?", CVAR_Archive|CVAR_PreInit);

// 1: normal; 2: 1-skewed
VCvarI gl_dbg_use_zpass("gl_dbg_use_zpass", "0", "DO NOT USE!", CVAR_PreInit);

//VCvarB gl_dbg_advlight_debug("gl_dbg_advlight_debug", false, "Draw non-fading lights?", CVAR_PreInit);
//VCvarS gl_dbg_advlight_color("gl_dbg_advlight_color", "0xff7f7f", "Color for debug lights (only dec/hex).", CVAR_PreInit);

VCvarB gl_dbg_wireframe("gl_dbg_wireframe", false, "Render wireframe level?", CVAR_PreInit);

#ifdef GL4ES_HACKS
# define FBO_WITH_TEXTURE_DEFAULT  true
#else
# define FBO_WITH_TEXTURE_DEFAULT  false
#endif
VCvarB gl_dbg_fbo_blit_with_texture("gl_dbg_fbo_blit_with_texture", FBO_WITH_TEXTURE_DEFAULT, "Always blit FBOs using texture mapping?", CVAR_PreInit);

VCvarB r_brightmaps("r_brightmaps", true, "Allow brightmaps?", CVAR_Archive);
VCvarB r_brightmaps_sprite("r_brightmaps_sprite", true, "Allow sprite brightmaps?", CVAR_Archive);
VCvarB r_brightmaps_additive("r_brightmaps_additive", true, "Are brightmaps additive, or max?", CVAR_Archive);
VCvarB r_brightmaps_filter("r_brightmaps_filter", false, "Do bilinear filtering on brightmaps?", CVAR_Archive);

VCvarB r_glow_flat("r_glow_flat", true, "Allow glowing flats?", CVAR_Archive);

VCvarB gl_lmap_allow_partial_updates("gl_lmap_allow_partial_updates", true, "Allow partial updates of lightmap atlases (this is usually faster than full updates)?", CVAR_Archive);

VCvarI gl_release_ram_textures_mode("gl_release_ram_textures_mode", "0", "When the engine should release RAM (non-GPU) texture storage (0:never; 1:after map unload; 2:immediately)?", CVAR_Archive);

// 0: 128
// 1: 256
// 2: 512
// 3: 1024
VCvarI gl_shadowmap_size("gl_shadowmap_size", "0", "Shadowmap size (0:128; 1:256; 2:512; 3:1024).", CVAR_PreInit|CVAR_Archive);
VCvarI gl_shadowmap_precision("gl_shadowmap_precision", "0", "Shadowmap precision (0:16; 1:32).", CVAR_PreInit|CVAR_Archive);
//!VCvarB gl_shadowmap_gbuffer("gl_shadowmap_gbuffer", false, "Emulate G-buffer (allocate all three color channels).", CVAR_PreInit|CVAR_Archive);
VCvarI gl_shadowmap_faster_check("gl_shadowmap_faster_check", "3", "Use slightly faster, but less precise shadowmap texel chechs? [0..3]", CVAR_PreInit|CVAR_Archive);


static VCvarB gl_s3tc_present("__gl_s3tc_present", false, "Use S3TC texture compression, if supported?", CVAR_Rom);


// ////////////////////////////////////////////////////////////////////////// //
#ifdef VV_SHADER_COMPILING_PROGRESS

#define PROG_BUF_WIDTH   (512)
#define PROG_BUF_HEIGHT  (64)

static uint8_t *tempProgBuffer = nullptr;
static GLuint shadMsgTexture = 0;
static double shadMsgStartTime;
static bool shadMsgActive = false;


//==========================================================================
//
//  progBufClear
//
//==========================================================================
static void progBufClear () {
  if (!tempProgBuffer) tempProgBuffer = (uint8_t *)Z_Malloc(PROG_BUF_WIDTH*PROG_BUF_HEIGHT*4);
  //memset(tempProgBuffer, 0, PROG_BUF_WIDTH*PROG_BUF_HEIGHT*4);
  uint32_t *da = (uint32_t *)tempProgBuffer;
  for (unsigned f = 0; f < PROG_BUF_WIDTH*PROG_BUF_HEIGHT; ++f, ++da) *da = 0xff000000;
}


//==========================================================================
//
//  progBufPutCharAt
//
//==========================================================================
static void progBufPutCharAt (int x0, int y0, char ch) {
  if (!tempProgBuffer) return;
  /*
  const unsigned colors[3] = { 0xff7f00u, 0xdf5f00u, 0xbf3f00u };
  const unsigned conColor = colors[lnum%3];
  int r = (conColor>>16)&0xff;
  int g = (conColor>>8)&0xff;
  int b = conColor&0xff;
  const int rr = r, gg = g, bb = b;
  */
  const unsigned color = 0xff007fff;
  for (int y = CONFONT_HEIGHT-1; y >= 0; --y) {
    if (y0+y < 0 || y0+y >= PROG_BUF_HEIGHT) break;
    vuint16 v = glConFont10[(ch&0xff)*10+y];
    uint32_t *da = (uint32_t *)(tempProgBuffer+(PROG_BUF_WIDTH*(y0+y))*4+x0*4);
    for (int x = 0; x < CONFONT_WIDTH; ++x, ++da) {
      if (x0+x < 0) continue;
      if (x0+x >= PROG_BUF_WIDTH) break;
      if (v&0x8000) {
        *da = color;
      } else {
        *da = 0xff000000;
      }
      v <<= 1;
    }
  }
}


//==========================================================================
//
//  progBufPutTextAt
//
//==========================================================================
static int progBufPutTextAt (int x0, int y0, const char *s) {
  if (!s || !s[0] || !tempProgBuffer) return 0;
  int wdt = 0;
  while (*s) {
    progBufPutCharAt(x0, y0, *s++);
    x0 += CONFONT_WIDTH;
    wdt += CONFONT_WIDTH;
  }
  return wdt;
}
#endif


//==========================================================================
//
//  VOpenGLDrawer::InitShaderProgress
//
//==========================================================================
void VOpenGLDrawer::InitShaderProgress () {
#ifdef VV_SHADER_COMPILING_PROGRESS
  vassert(shadMsgTexture == 0);

  shadMsgStartTime = Sys_Time();
  shadMsgActive = false;

  p_glBindFramebuffer(GL_FRAMEBUFFER, 0);

  GLSetViewport(0, 0, ScreenWidth, ScreenHeight);
  SetOrthoProjection(0, ScreenWidth, ScreenHeight, 0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glDisable(GL_ALPHA_TEST);
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  if (HaveDepthClamp) glDisable(GL_DEPTH_CLAMP);
  glDisable(GL_CLIP_PLANE0);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);
  GLDRW_CHECK_ERROR("progress preparation");

  glEnable(GL_TEXTURE_2D);
  glGenTextures(1, &shadMsgTexture);
  glBindTexture(GL_TEXTURE_2D, shadMsgTexture);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  GLDRW_CHECK_ERROR("create progress texture");

  //glClearColor(1.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  progBufClear();

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,  PROG_BUF_WIDTH, PROG_BUF_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, tempProgBuffer);
  GLDRW_CHECK_ERROR("upload texture image");
#endif
}


//==========================================================================
//
//  VOpenGLDrawer::DoneShaderProgress
//
//==========================================================================
void VOpenGLDrawer::DoneShaderProgress () {
#ifdef VV_SHADER_COMPILING_PROGRESS
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glBindTexture(GL_TEXTURE_2D, 0);
  glDeleteTextures(1, &shadMsgTexture);
  shadMsgTexture = 0;
  ReactivateCurrentFBO();
  /*
  GCon->Logf(NAME_Debug, "waiting... (%dx%d, %u)", ScreenWidth, ScreenHeight, shadMsgTexture);
  double stt = Sys_Time();
  while (Sys_Time()-stt < 2.0) Sys_Yield();
  */
#endif
}


//==========================================================================
//
//  VOpenGLDrawer::ShowShaderProgress
//
//==========================================================================
void VOpenGLDrawer::ShowShaderProgress (int curr, int total) {
#ifdef VV_SHADER_COMPILING_PROGRESS
  const double ctt = Sys_Time();
  if (!shadMsgActive) {
    if (ctt-shadMsgStartTime < 0.666) return;
    shadMsgActive = true;
  } else {
    // update once per 100 msec
    if (ctt-shadMsgStartTime < 100.0/100.0) return;
  }

  if (total < 1) total = 1;
  if (curr < 0) curr = 0;
  if (curr > total) curr = total;

  int wdt;
  if (curr < total) {
    wdt = progBufPutTextAt(0, 0, va("compiling shaders [%d/%d]", curr, total));
  } else {
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    progBufClear();
    wdt = progBufPutTextAt(0, 0, "done compiling shaders");
  }
  glTexSubImage2D(GL_TEXTURE_2D, 0,  0, 0, PROG_BUF_WIDTH, PROG_BUF_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, tempProgBuffer);

  glEnable(GL_TEXTURE_2D);
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

  const int texOfs = (ScreenWidth-wdt)/2;
  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2i(texOfs, 0);
    glTexCoord2f(1.0f, 0.0f); glVertex2i(texOfs+PROG_BUF_WIDTH, 0);
    glTexCoord2f(1.0f, 1.0f); glVertex2i(texOfs+PROG_BUF_WIDTH, PROG_BUF_HEIGHT);
    glTexCoord2f(0.0f, 1.0f); glVertex2i(texOfs, PROG_BUF_HEIGHT);
  glEnd();

  const int pbarHeight = 14;
  const int pbarWidth = ScreenWidth-12;
  const int pbarXOfs = (ScreenWidth-pbarWidth)/2;
  const int pbarYOfs = CONFONT_HEIGHT+4;

  glColor4f(1.0f, 0.5f, 0.0f, 1.0f);
  glDisable(GL_TEXTURE_2D);

  glBegin(GL_QUADS);
    glVertex2i(pbarXOfs-2, pbarYOfs);
    glVertex2i(pbarXOfs+pbarWidth+2, pbarYOfs);
    glVertex2i(pbarXOfs+pbarWidth+2, pbarYOfs+pbarHeight);
    glVertex2i(pbarXOfs-2, pbarYOfs+pbarHeight);
  glEnd();

  glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
  glBegin(GL_QUADS);
    glVertex2i(pbarXOfs-1, pbarYOfs+1);
    glVertex2i(pbarXOfs+pbarWidth+1, pbarYOfs+1);
    glVertex2i(pbarXOfs+pbarWidth+1, pbarYOfs+pbarHeight-1);
    glVertex2i(pbarXOfs-1, pbarYOfs+pbarHeight-1);
  glEnd();

  const int bwdt = pbarWidth*curr/total;
  glColor4f(0.9f, 0.4f, 0.0f, 1.0f);
  glBegin(GL_QUADS);
    glVertex2i(pbarXOfs, pbarYOfs+2);
    glVertex2i(pbarXOfs+bwdt, pbarYOfs+2);
    glVertex2i(pbarXOfs+bwdt, pbarYOfs+pbarHeight-2);
    glVertex2i(pbarXOfs, pbarYOfs+pbarHeight-2);
  glEnd();

  Update(false);

  shadMsgStartTime = ctt;
#endif
}


//==========================================================================
//
//  getShadowmapPOT
//
//==========================================================================
static unsigned int getShadowmapPOT () noexcept {
  int ss = gl_shadowmap_size.asInt()+1;
  if (ss < 0) ss = 0; else if (ss > 4) ss = 4;
  return (unsigned int)ss;
}


//==========================================================================
//
//  MSA
//
//==========================================================================
/*
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
*/


//==========================================================================
//
//  CheckVendorString
//
//  both strings should be lower-cased
//  `vs` is what we got from OpenGL
//  `fuckedName` is what we are looking for
//
//==========================================================================
static VVA_OKUNUSED bool CheckVendorString (VStr vs, const char *fuckedName) {
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



//**************************************************************************
//
//  VOpenGLDrawer
//
//**************************************************************************

//==========================================================================
//
//  VOpenGLDrawer::VOpenGLDrawer
//
//==========================================================================
VOpenGLDrawer::VOpenGLDrawer ()
  : VDrawer()
  , shaderHead(nullptr)
  , surfList()
  , mainFBO()
  , ambLightFBO()
  , wipeFBO()
  , cameraFBOList()
  , currMainFBO(-1)
{
  glVerMajor = glVerMinor = 0;

  currentActiveShader = nullptr;
  lastgamma = 0;
  CurrentFade = 0;

  useReverseZ = false;
  hasNPOT = false;
  hasBoundsTest = false;
  canIntoBloomFX = false;

  tmpImgBuf0 = nullptr;
  tmpImgBuf1 = nullptr;
  tmpImgBufSize = 0;

  readBackTempBuf = nullptr;
  readBackTempBufSize = 0;

  decalUsedStencil = false;
  decalStcVal = 255; // next value for stencil buffer (clear on the first use, and clear on each wrap)
  stencilBufferDirty = true; // just in case
  isShittyGPU = true; // let's play safe
  shittyGPUCheckDone = false; // just in case
  atlasesGenerated = false;
  currentActiveFBO = nullptr;

  blendEnabled = false;

  offsetEnabled = false;
  offsetFactor = offsetUnits = 0;

  lastOverbrightEnable = !gl_regular_disable_overbright;
  //cameraFBO[0].mOwner = nullptr;
  //cameraFBO[1].mOwner = nullptr;

  depthMaskSP = 0;
  currDepthMaskState = 0;

  cubeTexId = 0;
  cubeFBO = 0;
  memset(&cubeDepthTexId[0], 0, sizeof(cubeDepthTexId));
  shadowmapPOT = getShadowmapPOT();
  shadowmapSize = 64<<shadowmapPOT;
  cubemapLinearFiltering = false;
  smapDirty = 0x3f;
  smapCurrentFace = 0;

  memset(currentViewport, 0, sizeof(currentViewport));
  currentViewport[0] = -666;
}


//==========================================================================
//
//  VOpenGLDrawer::~VOpenGLDrawer
//
//==========================================================================
VOpenGLDrawer::~VOpenGLDrawer () {
  if (mInitialized) DeinitResolution();
  currentActiveFBO = nullptr;
  surfList.clear();
  if (tmpImgBuf0) { Z_Free(tmpImgBuf0); tmpImgBuf0 = nullptr; }
  if (tmpImgBuf1) { Z_Free(tmpImgBuf1); tmpImgBuf1 = nullptr; }
  tmpImgBufSize = 0;
  if (readBackTempBuf) { Z_Free(readBackTempBuf); readBackTempBuf = nullptr; }
  readBackTempBufSize = 0;
}


//==========================================================================
//
//  VOpenGLDrawer::DestroyShadowCube
//
//==========================================================================
void VOpenGLDrawer::DestroyShadowCube () {
  UnloadShadowMapShaders();
  // delete cubemap
  if (cubeTexId) {
    p_glBindFramebuffer(GL_FRAMEBUFFER, cubeFBO);
    p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
    p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteTextures(1, &cubeTexId);
    for (unsigned fc = 0; fc < 6; ++fc) glDeleteTextures(1, &cubeDepthTexId[fc]);
    p_glDeleteFramebuffers(1, &cubeFBO);

    cubeTexId = 0;
    cubeFBO = 0;
    memset(&cubeDepthTexId[0], 0, sizeof(cubeDepthTexId));
    smapDirty = 0x3f;
    smapCurrentFace = 0;
  }
}


//==========================================================================
//
//  VOpenGLDrawer::CreateShadowCube
//
//==========================================================================
void VOpenGLDrawer::CreateShadowCube () {
  DeactivateShader();
  DestroyShadowCube();
  if (!canRenderShadowmaps) return;

  shadowmapPOT = getShadowmapPOT();
  shadowmapSize = 64<<shadowmapPOT;
  smapDirty = 0x3f;
  smapCurrentFace = 0;

  GLDRW_RESET_ERROR();

  // create cubemap for shadowmapping
  p_glGenFramebuffers(1, &cubeFBO);
  GLDRW_CHECK_ERROR("create shadowmap FBO");
  vassert(cubeFBO);
  p_glBindFramebuffer(GL_FRAMEBUFFER, cubeFBO);
  GLDRW_CHECK_ERROR("bind shadowmap FBO");
  p_glObjectLabelVA(GL_FRAMEBUFFER, cubeFBO, "Shadowmap FBO");

  for (unsigned int fc = 0; fc < 6; ++fc) {
    glGenTextures(1, &cubeDepthTexId[fc]);
    GLDRW_CHECK_ERROR("create shadowmap depth texture");
    vassert(cubeDepthTexId[fc]);
    glBindTexture(GL_TEXTURE_2D, cubeDepthTexId[fc]);
    GLDRW_CHECK_ERROR("bind shadowmap depth texture");
    p_glObjectLabelVA(GL_TEXTURE, cubeDepthTexId[fc], "ShadowCube depth texture #%u", fc);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, shadowmapSize, shadowmapSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    GLDRW_CHECK_ERROR("initialize shadowmap depth texture");
    /*
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GLDRW_CHECK_ERROR("set shadowmap depth texture min filter");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLDRW_CHECK_ERROR("set shadowmap depth texture mag filter");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GLDRW_CHECK_ERROR("set shadowmap depth texture s");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLDRW_CHECK_ERROR("set shadowmap depth texture t");
    */
    glBindTexture(GL_TEXTURE_2D, 0);
    GLDRW_CHECK_ERROR("unbind shadowmap depth texture");
  }
  p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, cubeDepthTexId[0], 0);
  GLDRW_CHECK_ERROR("set framebuffer depth texture");

  glGenTextures(1, &cubeTexId);
  vassert(cubeTexId);
  GLDRW_CHECK_ERROR("create shadowmap cubemap");
  glBindTexture(GL_TEXTURE_CUBE_MAP, cubeTexId);
  GLDRW_CHECK_ERROR("bind shadowmap cubemap");
  p_glObjectLabelVA(GL_TEXTURE, cubeTexId, "ShadowCube cubemap texture");

  /*
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
  GLDRW_CHECK_ERROR("set shadowmap compare func");
  */
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, (cubemapLinearFiltering ? GL_LINEAR : GL_NEAREST));
  GLDRW_CHECK_ERROR("set shadowmap mag filter");
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, (cubemapLinearFiltering ? GL_LINEAR : GL_NEAREST));
  GLDRW_CHECK_ERROR("set shadowmap min filter");
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  GLDRW_CHECK_ERROR("set shadowmap wrap r");
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  GLDRW_CHECK_ERROR("set shadowmap wrap s");
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  GLDRW_CHECK_ERROR("set shadowmap wrap t");
  /*
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
  GLDRW_CHECK_ERROR("set shadowmap compare mode");
  */

  for (unsigned int fc = 0; fc < 6; ++fc) {
    //glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+fc, 0, GL_DEPTH_COMPONENT, shadowmapSize, shadowmapSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    //glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+fc, 0, GL_R16F, shadowmapSize, shadowmapSize, 0, GL_RED, GL_FLOAT, 0);
    //!glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+fc, 0, GL_RGBA, shadowmapSize, shadowmapSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    #if 1
    //glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+fc, 0, GL_R32F, shadowmapSize, shadowmapSize, 0, GL_RED, GL_FLOAT, 0);
    //glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+fc, 0, GL_RGB16F, shadowmapSize, shadowmapSize, 0, GL_RGB, GL_FLOAT, 0);
    if (gl_shadowmap_precision.asInt() > 0) {
      /*!
      if (gl_shadowmap_gbuffer) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+fc, 0, GL_RGB32F, shadowmapSize, shadowmapSize, 0, GL_RGB, GL_FLOAT, 0);
      } else
      */
      {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+fc, 0, GL_R32F, shadowmapSize, shadowmapSize, 0, GL_RED, GL_FLOAT, 0);
      }
    } else {
      /*!
      if (gl_shadowmap_gbuffer) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+fc, 0, GL_RGB16F, shadowmapSize, shadowmapSize, 0, GL_RGB, GL_FLOAT, 0);
      } else
      */
      {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+fc, 0, GL_R16F, shadowmapSize, shadowmapSize, 0, GL_RED, GL_FLOAT, 0);
      }
    }
    #else
    VTexture *tx = nullptr;
    switch (fc) {
      case 0: tx = GTextureManager[gtxRight]; break;
      case 1: tx = GTextureManager[gtxLeft]; break;
      case 2: tx = GTextureManager[gtxTop]; break;
      case 3: tx = GTextureManager[gtxBottom]; break;
      case 4: tx = GTextureManager[gtxBack]; break;
      case 5: tx = GTextureManager[gtxFront]; break;
    }
    //GCon->Logf(NAME_Init, "fc=%u; tx=%p", fc, tx);
    //vassert(tx);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+fc, 0, GL_RGBA, shadowmapSize, shadowmapSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, (tx ? tx->GetPixels() : nullptr));
    #endif
    GLDRW_CHECK_ERROR("init cubemap texture");
    //!p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X+fc, cubeTexId, 0);
    GLDRW_CHECK_ERROR("set framebuffer cubemap texture");
    //glDrawBuffer(GL_NONE);
    GLDRW_CHECK_ERROR("set framebuffer draw buffer");
    //glReadBuffer(GL_NONE);
    GLDRW_CHECK_ERROR("set framebuffer read buffer");
  }

  if (p_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) Sys_Error("OpenGL: cannot initialise shadowmap FBO");
  p_glBindFramebuffer(GL_FRAMEBUFFER, 0);

  GCon->Logf(NAME_Init, "OpenGL: created cubemap %u, fbo %u; shadowmap size: %ux%u", cubeTexId, cubeFBO, shadowmapSize, shadowmapSize);

  if (CheckExtension("GL_ARB_seamless_cube_map")) {
    GCon->Log(NAME_Init, "OpenGL: enabling seamless cubemaps.");
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
  }

  ReactivateCurrentFBO();
}


//==========================================================================
//
//  VOpenGLDrawer::EnsureShadowMapCube
//
//==========================================================================
void VOpenGLDrawer::EnsureShadowMapCube () {
  if (!cubeTexId) CreateShadowCube();
}


//==========================================================================
//
//  VOpenGLDrawer::DeinitResolution
//
//==========================================================================
void VOpenGLDrawer::DeinitResolution () {
  DestroyShadowCube();
  // delete VBOs
  vboSprite.destroy();
  vboSky.destroy();
  vboMaskedSurf.destroy();
  vboAdvSurf.destroy();
  // FBOs
  if (currentActiveFBO != nullptr) {
    currentActiveFBO = nullptr;
    ReactivateCurrentFBO();
  }
  // unload shaders
  DestroyShaders();
  // delete all created shader objects
  /*
  for (int i = CreatedShaderObjects.length()-1; i >= 0; --i) {
    p_glDeleteObjectARB(CreatedShaderObjects[i]);
  }
  CreatedShaderObjects.Clear();
  */
  // destroy FBOs
  DestroyCameraFBOList();
  mainFBO.destroy();
  //secondFBO.destroy();
  ambLightFBO.destroy();
  wipeFBO.destroy();
  BloomDeinit();
  DeleteLightmapAtlases();
  depthMaskSP = 0;

  memset(currentViewport, 0, sizeof(currentViewport));
  currentViewport[0] = -666;
}


//==========================================================================
//
//  VOpenGLDrawer::InitResolution
//
//==========================================================================
void VOpenGLDrawer::InitResolution () {
  if (currentActiveFBO != nullptr) {
    currentActiveFBO = nullptr;
    ReactivateCurrentFBO();
  }

  const int calcWidth = ScreenWidth;
  const int calcHeight = ScreenHeight;

  GCon->Logf(NAME_Init, "Setting up new resolution: %dx%d (%dx%d)", RealScreenWidth, RealScreenHeight, calcWidth, calcHeight);

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

  memset(currentViewport, 0, sizeof(currentViewport));
  currentViewport[0] = -666;

#ifdef GL4ES_NO_CONSTRUCTOR
  // fake version for GL4ES
  GLint major = 2, minor = 1;
#else
  GLint major = 30000, minor = 30000;
  glGetIntegerv(GL_MAJOR_VERSION, &major);
  glGetIntegerv(GL_MINOR_VERSION, &minor);
  if (major > 8 || minor > 16 || major < 1 || minor < 0) {
    GCon->Log(NAME_Error, "OpenGL: your GPU drivers are absolutely broken.");
    GCon->Logf(NAME_Error, "OpenGL: reported OpenGL version is v%d.%d, which is nonsence.", major, minor);
    GCon->Log(NAME_Error, "OpenGL: expect crashes and visual glitches (if the engine will run at all).");
    major = minor = 0;
    if (!isShittyGPU) {
      isShittyGPU = true;
      shittyGPUCheckDone = true;
      if (gl_dbg_ignore_gpu_blacklist) {
        GCon->Log(NAME_Init, "User command is to ignore blacklist; I shall obey!");
        isShittyGPU = false;
      }
    }
  } else {
    GCon->Logf(NAME_Init, "OpenGL v%d.%d found", major, minor);
  }
#endif
  glVerMajor = major;
  glVerMinor = minor;
  shadowmapPOT = getShadowmapPOT();
  shadowmapSize = 64<<shadowmapPOT;
  smapDirty = 0x3f;
  smapCurrentFace = 0;

  if (!shittyGPUCheckDone) {
    shittyGPUCheckDone = true;
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

  useReverseZ = false;
  DepthZeroOne = false;
  canRenderShadowmaps = false;
  p_glClipControl = nullptr;

  if ((major > 4 || (major == 4 && minor >= 5)) || CheckExtension("GL_ARB_clip_control")) {
    p_glClipControl = glClipControl_t(GetExtFuncPtr("glClipControl"));
  }
  if (p_glClipControl) {
    if (gl_enable_clip_control) {
      GCon->Log(NAME_Init, "OpenGL: `glClipControl()` found");
    } else {
      p_glClipControl = nullptr;
      GCon->Log(NAME_Init, "OpenGL: `glClipControl()` found, but disabled by user; i shall obey");
    }
  }

  /*
  if (!CheckExtension("GL_ARB_multitexture")) {
    Sys_Error("OpenGL FATAL: Multitexture extensions not found.");
  } else {
    GLint tmp = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &tmp);
    GCon->Logf(NAME_Init, "Max texture samplers: %d", tmp);
  }
  */

  // check for shader extensions
  if (CheckExtension("GL_ARB_shader_objects") && CheckExtension("GL_ARB_shading_language_100") &&
      CheckExtension("GL_ARB_vertex_shader") && CheckExtension("GL_ARB_fragment_shader"))
  {
    GLint tmp;
    GCon->Logf(NAME_Init, "Shading language version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION_ARB));
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max texture image units: %d", tmp);
    if (tmp < 4) Sys_Error("OpenGL: your GPU must support at least 4 texture samplers, but it has only %d", tmp);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max vertex attribs: %d", tmp);
    #ifndef GL4ES_HACKS
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max vertex uniform components: %d", tmp);
    glGetIntegerv(GL_MAX_VARYING_FLOATS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max varying floats: %d", tmp);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max fragment uniform components: %d", tmp);
    #endif
  } else {
    Sys_Error("OpenGL FATAL: no shader support");
  }

  if (!CheckExtension("GL_ARB_vertex_buffer_object")) Sys_Error("OpenGL FATAL: VBO not found.");
  if (!CheckExtension("GL_EXT_draw_range_elements")) Sys_Error("OpenGL FATAL: GL_EXT_draw_range_elements not found");

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

  glClipControl_t savedClipControl = p_glClipControl;
#ifdef GL4ES_NO_CONSTRUCTOR
  #define VV_GLIMPORTS
  #define VV_GLIMPORTS_PROC
  #define VGLAPIPTR(x,required)  do { \
    p_##x = &x; \
  } while(0)
  #include "gl_imports.h"
  #undef VGLAPIPTR
  #undef VV_GLIMPORTS_PROC
  #undef VV_GLIMPORTS
#else
  #define VV_GLIMPORTS
  #define VGLAPIPTR(x,required)  do { \
    p_##x = x##_t(GetExtFuncPtr(#x)); \
    if (!p_##x /*|| strstr(#x, "Framebuffer")*/) { \
      p_##x = nullptr; \
      VStr extfn(#x); \
      extfn += "EXT"; \
      /*extfn += "ARB";*/ \
      GCon->Logf(NAME_Init, "OpenGL: trying `%s` instead of `%s`", *extfn, #x); \
      p_##x = x##_t(GetExtFuncPtr(*extfn)); \
      if (p_##x) GCon->Logf(NAME_Init, "OpenGL: ...found `%s`.", *extfn); \
    } \
    if (required && !p_##x) Sys_Error("OpenGL: `%s()` not found!", ""#x); \
  } while (0)
  #include "gl_imports.h"
  #undef VGLAPIPTR
  #undef VV_GLIMPORTS
#endif
  p_glClipControl = savedClipControl;

  if (p_glBlitFramebuffer) GCon->Log(NAME_Init, "OpenGL: `glBlitFramebuffer()` found");

  if (!isShittyGPU && p_glClipControl) {
    // normal GPUs
    useReverseZ = true;
    if (!gl_enable_reverse_z) {
      GCon->Log(NAME_Init, "OpenGL: oops, user disabled reverse z, i shall obey");
      useReverseZ = false;
    }
  } else {
    GCon->Log(NAME_Init, "OpenGL: reverse z is turned off for your GPU");
    useReverseZ = false;
  }

  if (hasBoundsTest && !p_glDepthBounds) {
    hasBoundsTest = false;
    GCon->Log(NAME_Init, "OpenGL: GL_EXT_depth_bounds_test found, but no `glDepthBounds()` exported");
  }

  if (/*p_glStencilFuncSeparate &&*/ p_glStencilOpSeparate) {
    GCon->Log(NAME_Init, "Found OpenGL 2.0 separate stencil methods");
  } else if (CheckExtension("GL_ATI_separate_stencil")) {
    //p_glStencilFuncSeparate = glStencilFuncSeparate_t(GetExtFuncPtr("glStencilFuncSeparateATI"));
    p_glStencilOpSeparate = glStencilOpSeparate_t(GetExtFuncPtr("glStencilOpSeparateATI"));
    if (/*p_glStencilFuncSeparate &&*/ p_glStencilOpSeparate) {
      GCon->Log(NAME_Init, "Found GL_ATI_separate_stencil");
    } else {
      GCon->Log(NAME_Init, "No separate stencil methods found");
      //p_glStencilFuncSeparate = nullptr;
      p_glStencilOpSeparate = nullptr;
    }
  } else {
    GCon->Log(NAME_Init, "No separate stencil methods found");
    //p_glStencilFuncSeparate = nullptr;
    p_glStencilOpSeparate = nullptr;
  }

  if (!gl_dbg_disable_depth_clamp && CheckExtension("GL_ARB_depth_clamp")) {
    GCon->Log(NAME_Init, "Found GL_ARB_depth_clamp");
    HaveDepthClamp = true;
  } else if (!gl_dbg_disable_depth_clamp && CheckExtension("GL_NV_depth_clamp")) {
    GCon->Log(NAME_Init, "Found GL_NV_depth_clamp");
    HaveDepthClamp = true;
  } else {
    GCon->Log(NAME_Init, "Symbol not found, depth clamp extensions disabled.");
    HaveDepthClamp = false;
  }

  if (CheckExtension("GL_EXT_stencil_wrap")) {
    GCon->Log(NAME_Init, "Found GL_EXT_stencil_wrap");
    HaveStencilWrap = true;
  } else {
    GCon->Log(NAME_Init, "Symbol not found, stencil wrap extensions disabled.");
    HaveStencilWrap = false;
  }

  if (CheckExtension("GL_EXT_texture_compression_s3tc")) {
    GCon->Log(NAME_Init, "S3TC (texture compression) is supported");
    HaveS3TC = true;
  } else {
    GCon->Log(NAME_Init, "Symbol not found, stencil wrap extensions disabled.");
    HaveS3TC = false;
  }
  gl_s3tc_present = HaveS3TC;

  if (!HaveStencilWrap) GCon->Log(NAME_Init, "*** no stencil wrap --> no shadow volumes");
  if (!HaveDepthClamp) GCon->Log(NAME_Init, "*** no depth clamp --> no shadow volumes");
  //if (!p_glStencilFuncSeparate) GCon->Log(NAME_Init, "*** no separate stencil ops --> no shadow volumes");
  if (!p_glStencilOpSeparate) GCon->Log(NAME_Init, "*** no separate stencil ops --> no shadow volumes");

  if (!p_glGenerateMipmap || gl_dbg_fbo_blit_with_texture) {
    GCon->Log(NAME_Init, "OpenGL: bloom postprocessing effect disabled due to missing API");
    r_bloom = false;
    canIntoBloomFX = false;
  } else {
    canIntoBloomFX = true;
  }

  if (hasBoundsTest) GCon->Log(NAME_Init, "Found GL_EXT_depth_bounds_test");

  DepthZeroOne = !!p_glClipControl;

  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glPixelStorei(GL_PACK_ROW_LENGTH, 0);
  glPixelStorei(GL_PACK_IMAGE_HEIGHT, 0);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);

  // allocate main FBO object
  mainFBO.createDepthStencil(this, calcWidth, calcHeight);
  mainFBO.scrScaled = (RealScreenWidth != ScreenWidth || RealScreenHeight != ScreenHeight);
  GCon->Logf(NAME_Init, "OpenGL: reverse z is %s", (useReverseZ ? "enabled" : "disabled"));
  p_glObjectLabelVA(GL_FRAMEBUFFER, mainFBO.getFBOid(), "Main FBO");

  mainFBO.activate();
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // black background
  glClearDepth(!useReverseZ ? 1.0f : 0.0f);
  if (p_glClipControl) p_glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); // actually, this is better even for "normal" cases
  RestoreDepthFunc();
  glDepthRange(0.0f, 1.0f);

  glClearStencil(0);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
  stencilBufferDirty = false;

  // recreate camera FBOs
  for (auto &&cf : cameraFBOList) {
    cf->fbo.createDepthStencil(this, cf->camwidth, cf->camheight); // create depthstencil
    cf->fbo.activate();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // black background
    glClearDepth(!useReverseZ ? 1.0f : 0.0f);
    if (p_glClipControl) p_glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); // actually, this is better even for "normal" cases
    RestoreDepthFunc();
    glDepthRange(0.0f, 1.0f);

    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
  }

  // allocate ambient light FBO object
  ambLightFBO.createTextureOnly(this, calcWidth, calcHeight);
  p_glObjectLabelVA(GL_FRAMEBUFFER, ambLightFBO.getFBOid(), "Ambient light FBO");

  // allocate wipe FBO object
  wipeFBO.createTextureOnly(this, calcWidth, calcHeight);
  p_glObjectLabelVA(GL_FRAMEBUFFER, wipeFBO.getFBOid(), "Wipe FBO");

  //if (major >= 3) canRenderShadowmaps = true;
  canRenderShadowmaps = true;

  // check extensions
  if (canRenderShadowmaps) {
    if (!CheckExtension("GL_ARB_texture_cube_map") && !CheckExtension("GL_EXT_texture_cube_map")) {
      canRenderShadowmaps = false;
      GCon->Log(NAME_Init, "OpenGL: no cubemap support, shadowmaps disabled.");
    }
  }

  if (canRenderShadowmaps) {
    if (!CheckExtension("GL_ARB_texture_float")) {
      canRenderShadowmaps = false;
      GCon->Log(NAME_Init, "OpenGL: no floating point textures, shadowmaps disabled.");
    }
  }

  mainFBO.activate();

  // create VBO for sprites
  vassert(!vboSprite.isValid());
  vassert(!vboSky.isValid());
  vassert(!vboMaskedSurf.isValid());
  vassert(!vboAdvSurf.isValid());

  vboSprite.setOwner(this);
  vboSprite.ensure(4); // sprite is always a quad, so we can allocate it right here

  vboSky.setOwner(this);
  vboMaskedSurf.setOwner(this);
  vboAdvSurf.setOwner(this, true); // streaming

  // init some defaults
  glBindTexture(GL_TEXTURE_2D, 0);
  p_glBindFramebuffer(GL_FRAMEBUFFER, 0);

  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClearDepth(!useReverseZ ? 1.0f : 0.0f);
  if (p_glClipControl) p_glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); // actually, this is better even for "normal" cases
  RestoreDepthFunc();
  glDepthRange(0.0f, 1.0f);

  glClearStencil(0);

  glClear(GL_COLOR_BUFFER_BIT);
  Update(false); // only swap
  glClear(GL_COLOR_BUFFER_BIT);

  glEnable(GL_TEXTURE_2D);
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
  glHint(GL_GENERATE_MIPMAP_HINT, GL_FASTEST); // this seems to affect only `glGenerateMipmap()`

  vassert(!atlasesGenerated);
  GenerateLightmapAtlasTextures();

  //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  //glAlphaFunc(GL_GREATER, getAlphaThreshold());
  glShadeModel(GL_FLAT); // we're doing our own lighting anyway
  glDisable(GL_ALPHA_TEST);
  //glDisable(GL_BLEND);
  glDisable(GL_DITHER);
  glDisable(GL_FOG);
  glDisable(GL_LIGHTING);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  SelectTexture(0);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);

  glDepthMask(GL_FALSE); // no z-buffer writes
  currDepthMaskState = 0;

  #ifndef GL4ES_HACKS
  glDisable(GL_POLYGON_SMOOTH);
  #endif

  // shaders
  shaderHead = nullptr; // just in case

  GCon->Log(NAME_Init, "OpenGL: loading shaders");
  LoadAllShaders();
  LoadShadowmapShaders();

  p_glUseProgramObjectARB(0);
  currentActiveShader = nullptr;

#ifdef VV_SHADER_COMPILING_PROGRESS
  InitShaderProgress();
#endif

  GCon->Log(NAME_Init, "OpenGL: compiling shaders");
  CompileShaders(major, minor, canRenderShadowmaps);

#ifdef VV_SHADER_COMPILING_PROGRESS
  DoneShaderProgress();
#endif

  GLDRW_CHECK_ERROR("finish OpenGL initialization");

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  blendEnabled = true;

  glDisable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(0, 0);
  offsetEnabled = false;
  offsetFactor = offsetUnits = 0;

  mInitialized = true;

  currMainFBO = -1;
  currentActiveFBO = nullptr;
  ReactivateCurrentFBO();
  SetMainFBO(true); // forced

  callICB(VCB_InitResolution);

  /*
  if (canIntoBloomFX && r_bloom) Posteffect_Bloom(0, 0, calcWidth, calcHeight);
  currentActiveFBO = nullptr;
  ReactivateCurrentFBO();
  */
  GCon->Log(NAME_Init, "OpenGL: finished initialization.");
}

#undef gl_
#undef glc_
#undef glg_


//==========================================================================
//
//  VOpenGLDrawer::setupSpotLight
//
//==========================================================================
void VOpenGLDrawer::setupSpotLight (const TVec &LightPos, const float Radius, const TVec &aconeDir, const float aconeAngle) noexcept {
  spotLight = false;
  coneDir = aconeDir;
  coneAngle = (aconeAngle <= 0.0f || aconeAngle > 180.0f ? 0.0f : aconeAngle);
  if (coneAngle && aconeDir.isValid() && !aconeDir.isZero()) {
    coneDir.normaliseInPlace();
    if (coneDir.isValid()) {
      spotLight = true;
      //spotPlane.SetPointNormal3D(LightPos, coneDir);
      coneFrustum.setupSimpleDir(LightPos, aconeDir, clampval(aconeAngle*2.0f, 1.0f, 179.0f), Radius);
    }
  }
}
