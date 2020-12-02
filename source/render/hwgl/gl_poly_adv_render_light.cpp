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

// this is for 128x128 shadowmaps
// divide max to (shadowmapPOT+1)
static VCvarF gl_shadowmap_bias_mul("gl_shadowmap_bias_mul", "0.0065", "Shadowmap bias multiplier.", CVAR_PreInit/*|CVAR_Archive*/);
static VCvarF gl_shadowmap_bias_min("gl_shadowmap_bias_min", "0.0015", "Shadowmap bias minimum.", CVAR_PreInit/*|CVAR_Archive*/);
static VCvarF gl_shadowmap_bias_max("gl_shadowmap_bias_max", "0.04", "Shadowmap bias maximum.", CVAR_PreInit/*|CVAR_Archive*/);
static VCvarB gl_shadowmap_bias_adjust("gl_shadowmap_bias_adjust", true, "Adjust shadowmap bias according to shadowmap size?", CVAR_PreInit/*|CVAR_Archive*/);

static bool lpassDoShadowMap;


#define SETUP_LIGHT_SHADER_NOTX(shad_)  \
  (shad_).SetLightPos(LightPos); \
  (shad_).SetLightRadius(Radius); \
  (shad_).SetViewOrigin(vieworg.x, vieworg.y, vieworg.z); \
  (shad_).SetLightMin(LightMin); \
  (shad_).SetLightColor(((Color>>16)&255)/255.0f, ((Color>>8)&255)/255.0f, (Color&255)/255.0f);

#define SETUP_LIGHT_SHADER_SMAP_ONLY(shad_)  \
  (shad_).SetLightView(lview2); \
  (shad_).SetLightPos2(lpp); \
  (shad_).SetShadowTexture(1); \
  (shad_).SetBiasMul(gl_shadowmap_bias_mul.asFloat()); \
  (shad_).SetBiasMin(gl_shadowmap_bias_min.asFloat()); \
  if (gl_shadowmap_bias_adjust) { \
    (shad_).SetBiasMax(gl_shadowmap_bias_max.asFloat()/(float)(shadowmapPOT+1)); \
  } else { \
    (shad_).SetBiasMax(gl_shadowmap_bias_max.asFloat()); \
  }

#define SETUP_LIGHT_SHADER_SPOT_ONLY(shad_)  do { \
  (shad_).SetConeDirection(coneDir); \
  (shad_).SetConeAngle(coneAngle); \
} while (0)


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
  SETUP_LIGHT_SHADER_NORMAL(shad_); \
  SETUP_LIGHT_SHADER_SMAP_ONLY(shad_); \
  SETUP_LIGHT_SHADER_SMAP_ONLY(shad_##Tex); \
  (shad_##Tex).SetTexture(0); \
} while (0)

#define SETUP_LIGHT_SHADER_SMAP_SPOT(shad_)  do { \
  SETUP_LIGHT_SHADER_SMAP_NORMAL(shad_); \
  SETUP_LIGHT_SHADER_SPOT_ONLY(shad_); \
  SETUP_LIGHT_SHADER_SPOT_ONLY(shad_##Tex); \
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
  if (gl_dbg_wireframe) return;
  RestoreDepthFunc();
  glDepthMask(GL_FALSE); // no z-buffer writes
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

    //VMatrix4 lview;
    //Drawer->CalcModelMatrix(lview, LightPos, TAVec(0.0f, 0.0f, 0.0f), false);
    //ShadowsLightSMap.SetLightView(lview);
    VMatrix4 lview2;
    Drawer->CalcModelMatrix(lview2, TVec(0, 0, 0), TAVec(0, 0, 0), false);
    //VMatrix4 lview = VMatrix4::TranslateNeg(LightPos);
    //ShadowsLightSMap.SetLightMPV(lview);
    TVec lpp = lview2*LightPos;

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

#define SETUP_SHADER_UNIS(shad_)  do { \
  if (lpassDoShadowMap) { \
    if (spotLight) { SETUP_SHADER_UNIS_MAIN(shad_##SMapSpot); } else { SETUP_SHADER_UNIS_MAIN(shad_##SMap); } \
  } else { \
    if (spotLight) { SETUP_SHADER_UNIS_MAIN(shad_##Spot); } else { SETUP_SHADER_UNIS_MAIN(shad_); } \
  } \
} while (0)

#define SETUP_SHADER_UNIS_TX(shad_)  do { \
  if (lpassDoShadowMap) { \
    if (spotLight) { SETUP_SHADER_UNIS_MAIN_TX(shad_##SMapSpotTex); } else { SETUP_SHADER_UNIS_MAIN_TX(shad_##SMapTex); } \
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
  currentActiveShader->UploadChangedUniforms();
  glBegin(GL_TRIANGLE_FAN);
    for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i].vec());
  glEnd();
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);
}
