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
// for `blocklightsr`, `blocklightsg`, `blocklightsb`
#include "render/r_local.h"


//==========================================================================
//
//  VOpenGLDrawer::DrawMaskedPolygon
//
//==========================================================================
void VOpenGLDrawer::DrawMaskedPolygon (surface_t *surf, float Alpha, bool Additive) {
  if (!surf->plvisible) return; // viewer is in back side or on plane
  if (surf->count < 3) return;

  texinfo_t *tex = surf->texinfo;
  if (!tex->Tex || tex->Tex->Type == TEXTYPE_Null) return;

  GlowParams gp;
  CalcGlow(gp, surf);

  bool doBrightmap = (r_brightmaps && tex->Tex->Brightmap);

  if (doBrightmap) {
    SurfMaskedBrightmapGlow.Activate();
    SurfMaskedBrightmapGlow.SetBrightMapAdditive(r_brightmaps_additive ? 1.0f : 0.0f);
    SurfMaskedBrightmapGlow.SetTexture(0);
    SurfMaskedBrightmapGlow.SetTextureBM(1);
    SelectTexture(1);
    SetBrightmapTexture(tex->Tex->Brightmap);
    SelectTexture(0);
    if (gp.isActive()) {
      VV_GLDRAWER_ACTIVATE_GLOW(SurfMaskedBrightmapGlow, gp);
    } else {
      VV_GLDRAWER_DEACTIVATE_GLOW(SurfMaskedBrightmapGlow);
    }
  } else {
    SurfMaskedGlow.Activate();
    SurfMaskedGlow.SetTexture(0);
    //SurfMaskedGlow.SetFogType();
    if (gp.isActive()) {
      VV_GLDRAWER_ACTIVATE_GLOW(SurfMaskedGlow, gp);
    } else {
      VV_GLDRAWER_DEACTIVATE_GLOW(SurfMaskedGlow);
    }
  }

  SetTexture(tex->Tex, tex->ColorMap);

  bool zbufferWriteDisabled = false;
  bool decalsAllowed = false;
  bool restoreBlend = false;

  GLint oldDepthMask = 0;

  if (Additive || Alpha < 1.0f || tex->Tex->isTranslucent()) {
    restoreBlend = true;
    if (doBrightmap) {
      //SurfMaskedBrightmapGlow.SetAlphaRef(Additive ? getAlphaThreshold() : 0.666f);
      SurfMaskedBrightmapGlow.SetAlphaRef(Additive || tex->Tex->isTranslucent() ? getAlphaThreshold() : 0.666f);
    } else {
      //SurfMaskedGlow.SetAlphaRef(Additive ? getAlphaThreshold() : 0.666f);
      SurfMaskedGlow.SetAlphaRef(Additive || tex->Tex->isTranslucent() ? getAlphaThreshold() : 0.666f);
    }
    //GLEnableBlend();
    //GLDisableBlend();
    if (Additive) {
      glBlendFunc(GL_ONE, GL_ONE); // our source rgb is already premultiplied
      //glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    } else {
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
    // translucent things should not modify z-buffer
    if (Additive || Alpha < 1.0f) {
      zbufferWriteDisabled = true;
      glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);
      glDepthMask(GL_FALSE); // no z-buffer writes
    }
    if (r_decals_enabled && r_decals_wall_alpha && surf->seg && surf->seg->decalhead) {
      decalsAllowed = true;
    }
  } else {
    //GLDisableBlend();
    //GLEnableBlend(); // our texture will be premultiplied
    //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (doBrightmap) {
      //SurfMaskedBrightmapGlow.SetAlphaRef(0.666f);
      SurfMaskedBrightmapGlow.SetAlphaRef(tex->Tex->isTranslucent() ? getAlphaThreshold() : 0.666f);
    } else {
      //SurfMaskedGlow.SetAlphaRef(0.666f);
      SurfMaskedGlow.SetAlphaRef(tex->Tex->isTranslucent() ? getAlphaThreshold() : 0.666f);
    }
    Alpha = 1.0f;
    if (r_decals_enabled && r_decals_wall_masked && surf->seg && surf->seg->decalhead) {
      decalsAllowed = true;
    }
  }

  //k8: non-translucent walls should not end here, so there is no need to recalc/check lightmaps
  const float lightLevel = getSurfLightLevel(surf);
  //GCon->Logf("Tex: %s; lightLevel=%g", *tex->Tex->Name, getSurfLightLevel(surf));
  //const float lightLevel = 1.0f;
  if (doBrightmap) {
    SurfMaskedBrightmapGlow.SetLight(
      ((surf->Light>>16)&255)*lightLevel/255.0f,
      ((surf->Light>>8)&255)*lightLevel/255.0f,
      (surf->Light&255)*lightLevel/255.0f, Alpha);
    SurfMaskedBrightmapGlow.SetFogFade(surf->Fade, Alpha);
  } else {
    SurfMaskedGlow.SetLight(
      ((surf->Light>>16)&255)*lightLevel/255.0f,
      ((surf->Light>>8)&255)*lightLevel/255.0f,
      (surf->Light&255)*lightLevel/255.0f, Alpha);
    SurfMaskedGlow.SetFogFade(surf->Fade, Alpha);
  }

  bool doDecals = (decalsAllowed && tex->Tex && !tex->noDecals && surf->seg && surf->seg->decalhead);

  // fill stencil buffer for decals
  if (doDecals) RenderPrepareShaderDecals(surf);

  currentActiveShader->UploadChangedUniforms();
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
  //glBegin(GL_POLYGON);
  glBegin(GL_TRIANGLE_FAN);
  for (int i = 0; i < surf->count; ++i) {
    if (doBrightmap) {
      SurfMaskedBrightmapGlow.SetTexCoordAttr(
        (DotProduct(surf->verts[i], tex->saxis)+tex->soffs)*tex_iw,
        (DotProduct(surf->verts[i], tex->taxis)+tex->toffs)*tex_ih);
      //SurfMaskedBrightmapGlow.UploadChangedAttrs();
    } else {
      SurfMaskedGlow.SetTexCoordAttr(
        (DotProduct(surf->verts[i], tex->saxis)+tex->soffs)*tex_iw,
        (DotProduct(surf->verts[i], tex->taxis)+tex->toffs)*tex_ih);
      //SurfMaskedGlow.UploadChangedAttrs();
    }
    glVertex(surf->verts[i]);
  }
  glEnd();
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);

  // draw decals
  if (doDecals) {
    if (RenderFinishShaderDecals(DT_SIMPLE, surf, nullptr, tex->ColorMap)) {
      //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
      //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // decal renderer is using this too
      //p_glUseProgramObjectARB(SurfSimpleProgram);
      //return true;
    }
  }

  if (restoreBlend) {
    //GLDisableBlend();
    if (zbufferWriteDisabled) glDepthMask(oldDepthMask); // restore z-buffer writes
  }
  if (Additive) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  if (doBrightmap) {
    SelectTexture(1);
    glBindTexture(GL_TEXTURE_2D, 0);
    SelectTexture(0);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::DrawSpritePolygon
//
//  hangup:
//    0: normal
//   -1: no z-buffer write, slightly offset (used for flat-aligned sprites)
//   -2: no z-buffer write
//  666: fake sprite shadow
//
//==========================================================================
void VOpenGLDrawer::DrawSpritePolygon (const TVec *cv, VTexture *Tex,
                                       float Alpha, bool Additive,
                                       VTextureTranslation *Translation, int CMap,
                                       vuint32 light, vuint32 Fade,
                                       const TVec &sprnormal, float sprpdist,
                                       const TVec &saxis, const TVec &taxis, const TVec &texorg,
                                       int hangup)
{
  if (!Tex || Tex->Type == TEXTYPE_Null) return; // just in case

  TVec texpt(0, 0, 0);
  const bool fakeShadow = (hangup == 666);

  bool doBrightmap = (!fakeShadow && r_brightmaps && r_brightmaps_sprite && Tex->Brightmap);
  bool styleDark = (Alpha >= 1000.0f);
  if (styleDark) Alpha -= 1666.0f;

  if (doBrightmap) {
    //GCon->Logf("BRMAP for '%s' (%s)", *Tex->Name, *Tex->Brightmap->Name);
    SurfMaskedBrightmap.Activate();
    SurfMaskedBrightmap.SetBrightMapAdditive(r_brightmaps_additive ? 1.0f : 0.0f);
    SurfMaskedBrightmap.SetTexture(0);
    SurfMaskedBrightmap.SetTextureBM(1);
    SelectTexture(1);
    SetBrightmapTexture(Tex->Brightmap);
    SelectTexture(0);
  } else if (!fakeShadow) {
    SurfMasked.Activate();
    SurfMasked.SetTexture(0);
    //SurfMasked.SetFogType();
  } else {
    SurfMaskedFakeShadow.Activate();
    SurfMaskedFakeShadow.SetTexture(0);
  }

  SetSpriteLump(Tex, Translation, CMap, true);
  //SetupTextureFiltering(noDepthChange ? 3 : sprite_filter);
  //SetupTextureFiltering(noDepthChange ? model_filter : sprite_filter);
  SetupTextureFiltering(sprite_filter);

  bool zbufferWriteDisabled = false;
  bool restoreBlend = false;

  GLint oldDepthMask = 0;

  if (Additive || hangup || Alpha < 1.0f || Tex->isTranslucent()) {
    restoreBlend = true;
    if (doBrightmap) {
      //SurfMaskedBrightmap.SetAlphaRef(hangup || Additive ? getAlphaThreshold() : 0.666f);
      SurfMaskedBrightmap.SetAlphaRef(hangup || Additive || Tex->isTranslucent() ? getAlphaThreshold() : 0.666f);
    } else {
      //SurfMasked.SetAlphaRef(hangup || Additive ? getAlphaThreshold() : 0.666f);
      if (!fakeShadow) {
        SurfMasked.SetAlphaRef(hangup || Additive || Tex->isTranslucent() ? getAlphaThreshold() : 0.666f);
      } else {
        SurfMaskedFakeShadow.SetAlphaRef(hangup || Additive || Tex->isTranslucent() ? getAlphaThreshold() : 0.666f);
      }
    }
    if (hangup) {
      zbufferWriteDisabled = true;
      glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);
      glDepthMask(GL_FALSE); // no z-buffer writes
      if (hangup == -1) {
        const float updir = (!CanUseRevZ() ? -1.0f : 1.0f);// *hangup;
        GLPolygonOffset(updir, updir);
      }
      /*
      switch (hangup) {
        case -1: // no z-buffer write, slightly offset (used for flat-aligned sprites)
          {
            const float updir = (!CanUseRevZ() ? -1.0f : 1.0f);// *hangup;
            GLPolygonOffset(updir, updir);
          }
          break;
        case -2: // no z-buffer write
          break;
        case -3: // no z-buffer write, negative offset
          GLPolygonOffset(-1.0f, -1.0f);
          break;
        case -4: // no z-buffer write, positive offset
          GLPolygonOffset(1.0f, 1.0f);
          break;
      }
      */
    }
    //GLEnableBlend();
    // translucent things should not modify z-buffer
    if (!zbufferWriteDisabled && (Additive || Alpha < 1.0f)) {
      glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);
      glDepthMask(GL_FALSE); // no z-buffer writes
      zbufferWriteDisabled = true;
    }
    if (Additive) {
      glBlendFunc(GL_ONE, GL_ONE); // our source rgb is already premultiplied
      //glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    } else {
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      //p_glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
  } else {
    if (doBrightmap) {
      SurfMaskedBrightmap.SetAlphaRef(Tex->isTranslucent() ? getAlphaThreshold() : 0.666f);
    } else {
      if (!fakeShadow) {
        SurfMasked.SetAlphaRef(Tex->isTranslucent() ? getAlphaThreshold() : 0.666f);
      } else {
        SurfMaskedFakeShadow.SetAlphaRef(Tex->isTranslucent() ? getAlphaThreshold() : 0.666f);
      }
    }
    Alpha = 1.0f;
    //GLDisableBlend();
    //GLEnableBlend();
  }

  //GLEnableBlend();
  if (styleDark) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied

  //GCon->Logf("SPRITE: light=0x%08x; fade=0x%08x", light, Fade);
  //Fade ^= 0x00ffffff;
  //light = 0xffff0000;
  //Fade = 0x3f323232;
  /*
  if (Fade != FADE_LIGHT && RendLev->IsShadowVolumeRenderer()) {
    Fade ^= 0x00ffffff;
  }
  */

  //GCon->Logf("Tex=%s; Fade=0x%08x; light=0x%08x; alpha=%f", *Tex->Name, Fade, light, Alpha);
  //Fade = 0xff505050;

  #define SPRVTX(shdr_,cv_)  do { \
    texpt = (cv_)-texorg; \
    (shdr_).SetTexCoordAttr( \
      DotProduct(texpt, saxis)*tex_iw, \
      DotProduct(texpt, taxis)*tex_ih); \
    /*(shdr_).UploadChangedAttrs();*/ \
    glVertex(cv_); \
  } while (0)

  if (doBrightmap) {
    SurfMaskedBrightmap.SetLight(
      ((light>>16)&255)/255.0f,
      ((light>>8)&255)/255.0f,
      (light&255)/255.0f, Alpha);
    SurfMaskedBrightmap.SetFogFade(Fade, Alpha);
    SurfMaskedBrightmap.UploadChangedUniforms();
    glBegin(GL_QUADS);
      SPRVTX(SurfMaskedBrightmap, cv[0]);
      SPRVTX(SurfMaskedBrightmap, cv[1]);
      SPRVTX(SurfMaskedBrightmap, cv[2]);
      SPRVTX(SurfMaskedBrightmap, cv[3]);
    glEnd();
  } else {
    if (!fakeShadow) {
      SurfMasked.SetLight(
        ((light>>16)&255)/255.0f,
        ((light>>8)&255)/255.0f,
        (light&255)/255.0f, Alpha);
      SurfMasked.SetFogFade(Fade, Alpha);
      SurfMasked.UploadChangedUniforms();
      glBegin(GL_QUADS);
        SPRVTX(SurfMasked, cv[0]);
        SPRVTX(SurfMasked, cv[1]);
        SPRVTX(SurfMasked, cv[2]);
        SPRVTX(SurfMasked, cv[3]);
      glEnd();
    } else {
      SurfMaskedFakeShadow.SetLight(
        ((light>>16)&255)/255.0f,
        ((light>>8)&255)/255.0f,
        (light&255)/255.0f, Alpha);
      SurfMaskedFakeShadow.SetFogFade(Fade, Alpha);
      SurfMaskedFakeShadow.UploadChangedUniforms();
      glBegin(GL_QUADS);
        SPRVTX(SurfMaskedFakeShadow, cv[0]);
        SPRVTX(SurfMaskedFakeShadow, cv[1]);
        SPRVTX(SurfMaskedFakeShadow, cv[2]);
        SPRVTX(SurfMaskedFakeShadow, cv[3]);
      glEnd();
    }
  }

  #undef SPRVTX

  if (restoreBlend) {
    if (hangup) GLDisableOffset();
    //GLDisableBlend();
    if (zbufferWriteDisabled) glDepthMask(oldDepthMask); // restore z-buffer writes
    if (Additive) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  }
  if (styleDark) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  if (doBrightmap) {
    SelectTexture(1);
    glBindTexture(GL_TEXTURE_2D, 0);
    SelectTexture(0);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::BeginTranslucentPolygonDecals
//
//==========================================================================
void VOpenGLDrawer::BeginTranslucentPolygonDecals () {
  GLEnableBlend();
  //glGetIntegerv(GL_DEPTH_WRITEMASK, &savedDepthMask);
  glDepthMask(GL_FALSE); // no z-buffer writes
  glEnable(GL_TEXTURE_2D);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_SCISSOR_TEST);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  GLDisableOffset();
  glEnable(GL_CULL_FACE);
  RestoreDepthFunc();
  //GLDisableBlend();
}


//==========================================================================
//
//  VOpenGLDrawer::DrawTranslucentPolygonDecals
//
//==========================================================================
void VOpenGLDrawer::DrawTranslucentPolygonDecals (surface_t *surf, float Alpha, bool Additive) {
  //if (!Additive && Alpha < 0.3f) return; //k8: dunno
  if (!surf->plvisible) return; // viewer is in back side or on plane
  if (surf->count < 3) return;

  texinfo_t *tex = surf->texinfo;

  if (!tex->Tex) return;

  //r_decals_wall_masked
  bool doDecals = (r_decals_enabled && tex->Tex && !tex->noDecals && surf->seg && surf->seg->decalhead);
  if (!doDecals) return;

  // fill stencil buffer for decals
  RenderPrepareShaderDecals(surf);

  ShadowsSurfTransDecals.Activate();
  ShadowsSurfTransDecals.SetTexture(0);
  GLDisableBlend();
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

  SetTexture(tex->Tex, tex->ColorMap);

  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
  ShadowsSurfTransDecals.UploadChangedUniforms();
  glBegin(GL_POLYGON);
    for (int i = 0; i < surf->count; ++i) glVertex(surf->verts[i]);
  glEnd();
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);

  // draw decals
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  GLEnableBlend();
  (void)RenderFinishShaderDecals(DT_ADVANCED, surf, nullptr, tex->ColorMap);
}
