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

extern VCvarB r_shadowmaps;

static bool lpassDoShadowMap;
static unsigned int smapBShaderIndex;

// sorry for the pasta
// shitplusplus sux, macros sux

#define SETUP_LIGHT_SHADER_NOTX(shad_)  \
  (shad_).SetLightPos(LightPos); \
  (shad_).SetLightRadius(Radius); \
  (shad_).SetViewOrigin(vieworg.x, vieworg.y, vieworg.z); \
  (shad_).SetLightMin(LightMin); \
  (shad_).SetLightColor(((Color>>16)&255)/255.0f, ((Color>>8)&255)/255.0f, (Color&255)/255.0f);

#define SETUP_LIGHT_SHADER_SMAP_NOTX(shad_)  \
  (shad_##Blur)[smapBShaderIndex].SetLightPos(LightPos); \
  (shad_##Blur)[smapBShaderIndex].SetLightRadius(Radius); \
  (shad_##Blur)[smapBShaderIndex].SetViewOrigin(vieworg.x, vieworg.y, vieworg.z); \
  (shad_##Blur)[smapBShaderIndex].SetLightMin(LightMin); \
  (shad_##Blur)[smapBShaderIndex].SetLightColor(((Color>>16)&255)/255.0f, ((Color>>8)&255)/255.0f, (Color&255)/255.0f); \
  \
  (shad_##Blur)[smapBShaderIndex].SetShadowTexture(1); \
  /*(shad_##Blur)[smapBShaderIndex].SetBiasMul(advLightGetMulBias());*/ \
  /*(shad_##Blur)[smapBShaderIndex].SetBiasMin(advLightGetMinBias());*/ \
  /*(shad_##Blur)[smapBShaderIndex].SetBiasMax(advLightGetMaxBias(shadowmapPOT));*/ \
  /*(shad_##Blur)[smapBShaderIndex].SetCubeSize((float)(shadowmapSize));*/ \
  /*(shad_##Blur)[smapBShaderIndex].SetUseAdaptiveBias(cubemapLinearFiltering ? 0.0f : 1.0f);*/

#define SETUP_LIGHT_SHADER_SPOT_ONLY(shad_)  \
  (shad_).SetConeDirection(coneDir); \
  (shad_).SetConeAngle(coneAngle);

#define SETUP_LIGHT_SHADER_SMAP_SPOT_ONLY(shad_)  \
  (shad_##Blur)[smapBShaderIndex].SetConeDirection(coneDir); \
  (shad_##Blur)[smapBShaderIndex].SetConeAngle(coneAngle);


#define SETUP_LIGHT_SHADER_NORMAL(shad_)  do { \
  SETUP_LIGHT_SHADER_NOTX(shad_); \
  SETUP_LIGHT_SHADER_NOTX(shad_##Tex); \
  (shad_##Tex).SetTexture(0); \
} while (0)

#define SETUP_LIGHT_SHADER_SPOT(shad_)  do { \
  SETUP_LIGHT_SHADER_NORMAL(shad_); \
  SETUP_LIGHT_SHADER_SPOT_ONLY(shad_); \
  SETUP_LIGHT_SHADER_SPOT_ONLY(shad_##Tex); \
} while (0)

#define SETUP_LIGHT_SHADER(shad_)  \
  if (spotLight) { SETUP_LIGHT_SHADER_SPOT(shad_##Spot); } else { SETUP_LIGHT_SHADER_NORMAL(shad_); }


#define SETUP_LIGHT_SHADER_SMAP_NORMAL(shad_)  do { \
  SETUP_LIGHT_SHADER_SMAP_NOTX(shad_); \
  SETUP_LIGHT_SHADER_SMAP_NOTX(shad_##Tex); \
  (shad_##Tex##Blur)[smapBShaderIndex].SetTexture(0); \
} while (0)

#define SETUP_LIGHT_SHADER_SMAP_SPOT(shad_)  do { \
  SETUP_LIGHT_SHADER_SMAP_NORMAL(shad_); \
  SETUP_LIGHT_SHADER_SMAP_SPOT_ONLY(shad_); \
  SETUP_LIGHT_SHADER_SMAP_SPOT_ONLY(shad_##Tex); \
} while (0)

#define SETUP_LIGHT_SHADER_SMAP(shad_)  \
  if (spotLight) { SETUP_LIGHT_SHADER_SMAP_SPOT(shad_##Spot); } else { SETUP_LIGHT_SHADER_SMAP_NORMAL(shad_); }


//==========================================================================
//
//  VOpenGLDrawer::BeginLightPass
//
//  setup rendering parameters for lighted surface rendering
//
//==========================================================================
void VOpenGLDrawer::BeginLightPass (const TVec &LightPos, float Radius, float LightMin, vuint32 Color, bool doShadow) {
  smapBShaderIndex = (unsigned int)gl_shadowmap_blur.asInt();
  if (smapBShaderIndex >= SMAP_BLUR_MAX) smapBShaderIndex = SMAP_NOBLUR;

  if (gl_dbg_wireframe) return;
  RestoreDepthFunc();
  //glDepthMask(GL_FALSE); // no z-buffer writes
  glDisableDepthWrite();
  glDisable(GL_TEXTURE_2D);

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  GLEnableBlend();
  //glBlendFunc(GL_SRC_COLOR, GL_DST_COLOR);
  //p_glBlendEquation(GL_MAX_EXT);

  glDepthFunc(GL_EQUAL);

  if (doShadow && r_shadowmaps.asBool() && CanRenderShadowMaps()) {
    glDisable(GL_STENCIL_TEST);
    //glEnable(GL_TEXTURE_CUBE_MAP);
    SelectTexture(1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubeTexId);
    SelectTexture(0);
    SETUP_LIGHT_SHADER_SMAP(ShadowsLightSMap);
    lpassDoShadowMap = true;
  } else {
    lpassDoShadowMap = false;
    //glDisable(GL_TEXTURE_CUBE_MAP);
    SelectTexture(1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    SelectTexture(0);

    // do not use stencil test if we rendered no shadow surfaces
    if (doShadow && IsStencilBufferDirty()/*wasRenderedShadowSurface*/) {
      if (gl_dbg_use_zpass > 1) {
        glStencilFunc(GL_EQUAL, 0x1, 0xff);
      } else {
        glStencilFunc(GL_EQUAL, 0x0, 0xff);
      }
      glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
      glEnable(GL_STENCIL_TEST);
    } else {
      glDisable(GL_STENCIL_TEST);
    }

    //if (doShadow && !wasRenderedShadowSurface) Color = 0xffff0000u;

    SETUP_LIGHT_SHADER(ShadowsLight);
  }

  // reuse it, sorry
  smapLastTexinfo.initLastUsed();
}


//==========================================================================
//
//  VOpenGLDrawer::EndLightPass
//
//==========================================================================
void VOpenGLDrawer::EndLightPass () {
  if (lpassDoShadowMap) {
    //glDisable(GL_TEXTURE_CUBE_MAP);
    SelectTexture(1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    SelectTexture(0);
  }
}


#define SETUP_SHADER_UNIS_MAIN(shad_)  do { \
  (shad_).Activate(); \
  (shad_).SetSurfNormal(surf->GetNormal()); \
  (shad_).SetSurfDist(surf->GetDist()); \
} while (0)

#define SETUP_SHADER_UNIS_MAIN_TX(shad_)  do { \
  SETUP_SHADER_UNIS_MAIN(shad_); \
  if (textureChanged) (shad_).SetTex(currTexinfo); \
} while (0)

#define SETUP_SHADER_UNIS_MAIN_SMAP(shad_)  do { \
  (shad_##Blur)[smapBShaderIndex].Activate(); \
  (shad_##Blur)[smapBShaderIndex].SetSurfNormal(surf->GetNormal()); \
  (shad_##Blur)[smapBShaderIndex].SetSurfDist(surf->GetDist()); \
} while (0)

#define SETUP_SHADER_UNIS_MAIN_TX_SMAP(shad_)  do { \
  SETUP_SHADER_UNIS_MAIN_SMAP(shad_); \
  if (textureChanged) (shad_##Blur)[smapBShaderIndex].SetTex(currTexinfo); \
} while (0)

#define SETUP_SHADER_UNIS(shad_)  do { \
  if (lpassDoShadowMap) { \
    if (spotLight) { SETUP_SHADER_UNIS_MAIN_SMAP(shad_##SMapSpot); } else { SETUP_SHADER_UNIS_MAIN_SMAP(shad_##SMap); } \
  } else { \
    if (spotLight) { SETUP_SHADER_UNIS_MAIN(shad_##Spot); } else { SETUP_SHADER_UNIS_MAIN(shad_); } \
  } \
} while (0)

#define SETUP_SHADER_UNIS_TX(shad_)  do { \
  if (lpassDoShadowMap) { \
    if (spotLight) { SETUP_SHADER_UNIS_MAIN_TX_SMAP(shad_##SMapSpotTex); } else { SETUP_SHADER_UNIS_MAIN_TX_SMAP(shad_##SMapTex); } \
  } else { \
    if (spotLight) { SETUP_SHADER_UNIS_MAIN_TX(shad_##SpotTex); } else { SETUP_SHADER_UNIS_MAIN_TX(shad_##Tex); } \
  } \
} while (0)


//==========================================================================
//
//  VOpenGLDrawer::DrawSurfaceLight
//
//  this blends surfaces from light sources to ambient map.
//
//  `LightCanCross` means that light can span over this surface
//  light can span over two-sided midtex, for example, but not over
//  one-sided wall
//    <0: horizon
//    >0: two-sided wall
//    =0: one-sided wall
//
//  most checks are done in caller
//
//==========================================================================
void VOpenGLDrawer::DrawSurfaceLight (surface_t *surf) {
  if (gl_dbg_wireframe) return;
  if (!surf->IsPlVisible()) return; // viewer is in back side or on plane
  if (surf->count < 3) return;

  if (spotLight && !isSurfaceInSpotlight(surf)) return;

  const unsigned vcount = (unsigned)surf->count;
  const SurfVertex *sverts = surf->verts;
  const SurfVertex *v = sverts;

  const texinfo_t *currTexinfo = surf->texinfo;

  if (currTexinfo->Tex->isTransparent()) {
    const bool textureChanged = smapLastTexinfo.needChange(*currTexinfo, updateFrame);
    if (textureChanged) {
      smapLastTexinfo.updateLastUsed(*currTexinfo);
      SetTexture(currTexinfo->Tex, currTexinfo->ColorMap);
    }
    SETUP_SHADER_UNIS_TX(ShadowsLight);
  } else {
    SETUP_SHADER_UNIS(ShadowsLight);
  }

  //SetTexture(currTexinfo->Tex, currTexinfo->ColorMap);

  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
  //glBegin(GL_POLYGON);
  //GCon->Logf(NAME_Debug, "shader: %s", currentActiveShader->progname);
  currentActiveShader->UploadChangedUniforms();
  glBegin(GL_TRIANGLE_FAN);
    //for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i].vec());
    for (unsigned i = 0; i < vcount; ++i, ++v) glVertex(v->vec());
  glEnd();
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);
}
