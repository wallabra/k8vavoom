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
#include "gl_local.h"
#include "gl_poly_adv_render.h"


//==========================================================================
//
//  VOpenGLDrawer::DrawWorldAmbientPass
//
//  this renders sector ambient light based on sector light level
//  it can be optimised: we don't need to do any texture interpolation for
//  textures without transparent pixels
//
//==========================================================================
void VOpenGLDrawer::DrawWorldAmbientPass () {
  VRenderLevelDrawer::DrawLists &dls = RendLev->GetCurrentDLS();
  GLDisableBlend();

  // draw horizons and z-skies
  if (!gl_dbg_wireframe) {
    for (auto &&surf : dls.DrawHorizonList) {
      if (!surf->IsPlVisible()) continue; // viewer is in back side or on plane
      DoHorizonPolygon(surf);
    }

    // set z-buffer for skies
    if (dls.DrawSkyList.length()) {
      SurfZBuf.Activate();
      SurfZBuf.UploadChangedUniforms();
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      for (auto &&surf : dls.DrawSkyList) {
        if (!surf->IsPlVisible()) continue; // viewer is in back side or on plane
        if (surf->count < 3) continue;
        //glBegin(GL_POLYGON);
        glBegin(GL_TRIANGLE_FAN);
          for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i].vec());
        glEnd();
      }
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }
  }

  // draw normal surfaces
  if (dls.DrawSurfListSolid.length() != 0 || dls.DrawSurfListMasked.length() != 0) {
    // do not sort surfaces by texture here, because
    // textures will be put later, and BSP sorted them by depth for us
    // other passes can skip surface sorting

    // sort masked textures by shader class and texture
    timsort_r(dls.DrawSurfListMasked.ptr(), dls.DrawSurfListMasked.length(), sizeof(surface_t *), &glAdvRenderDrawListItemCmpByShaderTexture, nullptr);
    // sort solid textures too, so we can avoid shader switches
    // but do this only by shader class, to retain as much front-to-back order as possible
    timsort_r(dls.DrawSurfListSolid.ptr(), dls.DrawSurfListSolid.length(), sizeof(surface_t *), &glAdvRenderDrawListItemCmpByShaderBMTexture, nullptr);
    #if 0
    CheckListSortValidity(dls.DrawSurfListSolid, "solid");
    CheckListSortValidity(dls.DrawSurfListMasked, "masked");
    #endif

    //FIXME!
    if (gl_dbg_wireframe) {
      DrawAutomap.Activate();
      DrawAutomap.UploadChangedUniforms();
      GLEnableBlend();
      glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

      SelectTexture(1);
      glBindTexture(GL_TEXTURE_2D, 0);
      SelectTexture(0);
      return;
    }

    // setup samplers for all shaders
    // masked
    ShadowsAmbientMasked.Activate();
    ShadowsAmbientMasked.SetTexture(0);
    // brightmap
    ShadowsAmbientBrightmap.Activate();
    ShadowsAmbientBrightmap.SetBrightMapAdditive(r_brightmaps_additive ? 1.0f : 0.0f);
    ShadowsAmbientBrightmap.SetTexture(0);
    ShadowsAmbientBrightmap.SetTextureBM(1);
    // normal
    ShadowsAmbient.Activate();

    float prevsflight = -666;
    vuint32 prevlight = 0;
    texinfo_t lastTexinfo;
    lastTexinfo.initLastUsed();

    bool glTextureEnabled = false;
    glDisable(GL_TEXTURE_2D);

    bool lastCullFace = true;
    glEnable(GL_CULL_FACE);

    // activate VBO
    vboAdvSurf.activate();
    GLuint attribPosition = 0; /* shut up, gcc! */
    int vboCountIdx = 0; // element (counter) index

    //WARNING! don't forget to flush VBO on each shader uniform change! this includes glow changes (glow values aren't cached yet)

    #define SADV_DO_HEAD_LIGHT(shader_)  \
      const surface_t *surf = *sptr; \
      /* setup new light if necessary */ \
      const float lev = getSurfLightLevel(surf); \
      if (prevlight != surf->Light || FASI(lev) != FASI(prevsflight)) { \
        SADV_FLUSH_VBO(); \
        prevsflight = lev; \
        prevlight = surf->Light; \
        (shader_).SetLight( \
          ((prevlight>>16)&255)*lev/255.0f, \
          ((prevlight>>8)&255)*lev/255.0f, \
          (prevlight&255)*lev/255.0f, 1.0f); \
      }

    #define SADV_CHECK_TEXTURE_BM(shader_)  do { \
      const texinfo_t *currTexinfo = surf->texinfo; \
      const bool textureChanged = lastTexinfo.needChange(*currTexinfo, updateFrame); \
      if (textureChanged) { \
        SADV_FLUSH_VBO(); \
        lastTexinfo.updateLastUsed(*currTexinfo); \
        /* set brightmap texture */ \
        SelectTexture(1); \
        SetBrightmapTexture(currTexinfo->Tex->Brightmap); \
        /* set normal texture */ \
        SelectTexture(0); \
        SetTexture(currTexinfo->Tex, currTexinfo->ColorMap); \
        (shader_).SetTex(currTexinfo); \
      } \
    } while (0)

    // solid textures
    if (dls.DrawSurfListSolid.length() != 0) {
      const int len = dls.DrawSurfListSolid.length();
      const surface_t *const *sptr = dls.DrawSurfListSolid.ptr();
      // find first valid surface
      int idx;
      for (idx = 0; idx < len; ++idx, ++sptr) {
        const surface_t *surf = *sptr;
        if (surf->IsPlVisible()) break;
      }

      // normal textures
      prevsflight = -666; // force light setup
      if (idx < len && (*sptr)->shaderClass == SFST_Normal) {
        ShadowsAmbient.Activate();
        attribPosition = ShadowsAmbient.loc_Position;
        vboAdvSurf.enableAttrib(attribPosition);
        VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbient);
        if (glTextureEnabled) { glTextureEnabled = false; glDisable(GL_TEXTURE_2D); }
        for (; idx < len && (*sptr)->shaderClass == SFST_Normal; ++idx, ++sptr) {
          SADV_DO_HEAD_LIGHT(ShadowsAmbient)
          SADV_DO_RENDER();
        }
        SADV_FLUSH_VBO();
        vboAdvSurf.disableAttrib(attribPosition);
      }

      // normal glowing textures
      if (idx < len && (*sptr)->shaderClass == SFST_NormalGlow) {
        ShadowsAmbient.Activate();
        attribPosition = ShadowsAmbient.loc_Position;
        vboAdvSurf.enableAttrib(attribPosition);
        if (glTextureEnabled) { glTextureEnabled = false; glDisable(GL_TEXTURE_2D); }
        for (; idx < len && (*sptr)->shaderClass == SFST_NormalGlow; ++idx, ++sptr) {
          SADV_DO_HEAD_LIGHT(ShadowsAmbient)
          SADV_FLUSH_VBO();
          VV_GLDRAWER_ACTIVATE_GLOW(ShadowsAmbient, surf->gp);
          SADV_DO_RENDER();
        }
        SADV_FLUSH_VBO();
        vboAdvSurf.disableAttrib(attribPosition);
      }

      // brightmap textures
      prevsflight = -666; // force light setup
      lastTexinfo.resetLastUsed();
      if (idx < len && (*sptr)->shaderClass == SFST_BMap) {
        ShadowsAmbientBrightmap.Activate();
        attribPosition = ShadowsAmbientBrightmap.loc_Position;
        vboAdvSurf.enableAttrib(attribPosition);
        VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbientBrightmap);
        if (!glTextureEnabled) { glTextureEnabled = true; glEnable(GL_TEXTURE_2D); }
        for (; idx < len && (*sptr)->shaderClass == SFST_BMap; ++idx, ++sptr) {
          SADV_DO_HEAD_LIGHT(ShadowsAmbientBrightmap)
          SADV_CHECK_TEXTURE_BM(ShadowsAmbientBrightmap);
          SADV_DO_RENDER();
        }
        SADV_FLUSH_VBO();
        vboAdvSurf.disableAttrib(attribPosition);
      }

      // brightmap glow textures
      if (idx < len && (*sptr)->shaderClass == SFST_BMapGlow) {
        ShadowsAmbientBrightmap.Activate();
        attribPosition = ShadowsAmbientBrightmap.loc_Position;
        vboAdvSurf.enableAttrib(attribPosition);
        if (!glTextureEnabled) { glTextureEnabled = true; glEnable(GL_TEXTURE_2D); }
        for (; idx < len && (*sptr)->shaderClass == SFST_BMapGlow; ++idx, ++sptr) {
          SADV_DO_HEAD_LIGHT(ShadowsAmbientBrightmap)
          SADV_CHECK_TEXTURE_BM(ShadowsAmbientBrightmap);
          SADV_FLUSH_VBO();
          VV_GLDRAWER_ACTIVATE_GLOW(ShadowsAmbientBrightmap, surf->gp);
          SADV_DO_RENDER();
        }
        SADV_FLUSH_VBO();
        vboAdvSurf.disableAttrib(attribPosition);
      }
    }

    // masked textures
    if (dls.DrawSurfListMasked.length() != 0) {
      const int len = dls.DrawSurfListMasked.length();
      const surface_t *const *sptr = dls.DrawSurfListMasked.ptr();
      // find first valid surface
      int idx;
      for (idx = 0; idx < len; ++idx, ++sptr) {
        const surface_t *surf = *sptr;
        if (surf->IsPlVisible()) break;
      }

      // normal textures
      prevsflight = -666; // force light setup
      lastTexinfo.resetLastUsed();
      if (idx < len && (*sptr)->shaderClass == SFST_Normal) {
        ShadowsAmbientMasked.Activate();
        attribPosition = ShadowsAmbientMasked.loc_Position;
        vboAdvSurf.enableAttrib(attribPosition);
        VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbientMasked);
        if (!glTextureEnabled) { glTextureEnabled = true; glEnable(GL_TEXTURE_2D); }
        for (; idx < len && (*sptr)->shaderClass == SFST_Normal; ++idx, ++sptr) {
          SADV_DO_HEAD_LIGHT(ShadowsAmbientMasked)
          SADV_CHECK_TEXTURE(ShadowsAmbientMasked);
          SADV_DO_RENDER();
        }
        SADV_FLUSH_VBO();
        vboAdvSurf.disableAttrib(attribPosition);
      }

      // normal glowing textures
      if (idx < len && (*sptr)->shaderClass == SFST_NormalGlow) {
        ShadowsAmbientMasked.Activate();
        attribPosition = ShadowsAmbientMasked.loc_Position;
        vboAdvSurf.enableAttrib(attribPosition);
        if (!glTextureEnabled) { glTextureEnabled = true; glEnable(GL_TEXTURE_2D); }
        for (; idx < len && (*sptr)->shaderClass == SFST_NormalGlow; ++idx, ++sptr) {
          SADV_DO_HEAD_LIGHT(ShadowsAmbientMasked)
          SADV_CHECK_TEXTURE(ShadowsAmbientMasked);
          SADV_FLUSH_VBO();
          VV_GLDRAWER_ACTIVATE_GLOW(ShadowsAmbientMasked, surf->gp);
          SADV_DO_RENDER();
        }
        SADV_FLUSH_VBO();
        vboAdvSurf.disableAttrib(attribPosition);
      }

      // brightmap textures
      prevsflight = -666; // force light setup
      lastTexinfo.resetLastUsed();
      if (idx < len && (*sptr)->shaderClass == SFST_BMap) {
        ShadowsAmbientBrightmap.Activate();
        attribPosition = ShadowsAmbientBrightmap.loc_Position;
        vboAdvSurf.enableAttrib(attribPosition);
        VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbientBrightmap);
        if (!glTextureEnabled) { glTextureEnabled = true; glEnable(GL_TEXTURE_2D); }
        for (; idx < len && (*sptr)->shaderClass == SFST_BMap; ++idx, ++sptr) {
          SADV_DO_HEAD_LIGHT(ShadowsAmbientBrightmap)
          SADV_CHECK_TEXTURE_BM(ShadowsAmbientBrightmap);
          SADV_DO_RENDER();
        }
        SADV_FLUSH_VBO();
        vboAdvSurf.disableAttrib(attribPosition);
      }

      // brightmap glow textures
      if (idx < len && (*sptr)->shaderClass == SFST_BMapGlow) {
        ShadowsAmbientBrightmap.Activate();
        attribPosition = ShadowsAmbientBrightmap.loc_Position;
        vboAdvSurf.enableAttrib(attribPosition);
        if (!glTextureEnabled) { glTextureEnabled = true; glEnable(GL_TEXTURE_2D); }
        for (; idx < len && (*sptr)->shaderClass == SFST_BMapGlow; ++idx, ++sptr) {
          SADV_DO_HEAD_LIGHT(ShadowsAmbientBrightmap)
          SADV_CHECK_TEXTURE_BM(ShadowsAmbientBrightmap);
          SADV_FLUSH_VBO();
          VV_GLDRAWER_ACTIVATE_GLOW(ShadowsAmbientBrightmap, surf->gp);
          SADV_DO_RENDER();
        }
        SADV_FLUSH_VBO();
        vboAdvSurf.disableAttrib(attribPosition);
      }
    }

    // deactivate VBO
    vboAdvSurf.deactivate();

    if (!lastCullFace) glEnable(GL_CULL_FACE);
    if (!glTextureEnabled) glEnable(GL_TEXTURE_2D);

    #undef SADV_DO_HEAD_LIGHT
    #undef SADV_CHECK_TEXTURE_BM
  }

  // restore depth function
  //if (gl_prefill_zbuffer) RestoreDepthFunc();

  SelectTexture(1);
  glBindTexture(GL_TEXTURE_2D, 0);
  SelectTexture(0);
}
