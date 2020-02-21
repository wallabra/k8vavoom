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
//  666: fake sprite shadow
//  bit 0 set: no z-buffer write
//  bit 1 set: do offsetting (used for flat-aligned sprites)
//  bit 2 set: don't cull faces
//
//==========================================================================
void VOpenGLDrawer::DrawSpritePolygon (const TVec *cv, VTexture *Tex,
                                       const RenderStyleInfo &ri,
                                       VTextureTranslation *Translation, int CMap,
                                       const TVec &sprnormal, float sprpdist,
                                       const TVec &saxis, const TVec &taxis, const TVec &texorg)
{
  if (!Tex || Tex->Type == TEXTYPE_Null) return; // just in case

  enum ShaderType {
    ShaderMasked,
    ShaderMaskedBM,
    ShaderStencil,
    ShaderFakeShadow,
  };

  ShaderType shadtype = ShaderMasked;
  if (ri.stencilColor&0xff000000u) {
    shadtype = (ri.stencilColor&0x00ffffffu ? ShaderStencil : ShaderFakeShadow);
  } else {
    if (r_brightmaps && r_brightmaps_sprite && Tex->Brightmap) shadtype = ShaderMaskedBM;
    //bool styleDark = (Alpha >= 1000.0f);
    //if (styleDark) Alpha -= 1666.0f;
  }

  //const bool fakeShadow = (hangup == 666);
  //if (fakeShadow) hangup = 1; // no z-buffer write

  //bool doBrightmap = (!fakeShadow && r_brightmaps && r_brightmaps_sprite && Tex->Brightmap);
  //bool styleDark = (Alpha >= 1000.0f);
  //if (styleDark) Alpha -= 1666.0f;

  const bool trans = (ri.translucency || ri.hangup || ri.alpha < 1.0f || Tex->isTranslucent());

  switch (shadtype) {
    case ShaderMasked:
      SurfMasked.Activate();
      SurfMasked.SetTexture(0);
      SurfMasked.SetLight(
        ((ri.light>>16)&255)/255.0f,
        ((ri.light>>8)&255)/255.0f,
        (ri.light&255)/255.0f, ri.alpha);
      SurfMasked.SetFogFade(ri.fade, ri.alpha);
      SurfMasked.SetAlphaRef(trans ? getAlphaThreshold() : 0.666f);
      SurfMasked.UploadChangedUniforms();
      break;
    case ShaderMaskedBM:
      SurfMaskedBrightmap.Activate();
      SurfMaskedBrightmap.SetBrightMapAdditive(r_brightmaps_additive ? 1.0f : 0.0f);
      SurfMaskedBrightmap.SetTexture(0);
      SurfMaskedBrightmap.SetTextureBM(1);
      SelectTexture(1);
      SetBrightmapTexture(Tex->Brightmap);
      SelectTexture(0);
      SurfMaskedBrightmap.SetLight(
        ((ri.light>>16)&255)/255.0f,
        ((ri.light>>8)&255)/255.0f,
        (ri.light&255)/255.0f, ri.alpha);
      SurfMaskedBrightmap.SetFogFade(ri.fade, ri.alpha);
      SurfMaskedBrightmap.SetAlphaRef(trans ? getAlphaThreshold() : 0.666f);
      SurfMaskedBrightmap.UploadChangedUniforms();
      break;
    case ShaderStencil:
      SurfMaskedStencil.Activate();
      SurfMaskedStencil.SetTexture(0);
      SurfMaskedStencil.SetStencilColor(
        ((ri.stencilColor>>16)&255)/255.0f,
        ((ri.stencilColor>>8)&255)/255.0f,
        (ri.stencilColor&255)/255.0f);
      SurfMaskedStencil.SetLight(
        ((ri.light>>16)&255)/255.0f,
        ((ri.light>>8)&255)/255.0f,
        (ri.light&255)/255.0f, ri.alpha);
      SurfMaskedStencil.SetFogFade(ri.fade, ri.alpha);
      SurfMaskedStencil.SetAlphaRef(trans ? getAlphaThreshold() : 0.666f);
      SurfMaskedStencil.UploadChangedUniforms();
      break;
    case ShaderFakeShadow:
      SurfMaskedFakeShadow.Activate();
      SurfMaskedFakeShadow.SetTexture(0);
      SurfMaskedFakeShadow.SetLight(
        ((ri.light>>16)&255)/255.0f,
        ((ri.light>>8)&255)/255.0f,
        (ri.light&255)/255.0f, ri.alpha);
      SurfMaskedFakeShadow.SetAlphaRef(trans ? getAlphaThreshold() : 0.666f);
      SurfMaskedFakeShadow.SetFogFade(ri.fade, ri.alpha);
      SurfMaskedFakeShadow.UploadChangedUniforms();
      break;
    default: Sys_Error("ketmar forgot some shader type in `VOpenGLDrawer::DrawSpritePolygon()`");
  }

  SetSpriteLump(Tex, Translation, CMap, true);
  SetupTextureFiltering(sprite_filter);

  bool restoreDepthMask = false; // `true` means "depth write disabled"
  bool restoreBlend = false;
  GLint oldDepthMask = 0;

  // setup hangups
  if (ri.hangup) {
    // no z-buffer?
    if (ri.hangup&1) {
      restoreDepthMask = true;
      glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);
      glDepthMask(GL_FALSE); // no z-buffer writes
    }
    // offset?
    if (ri.hangup&2) {
      const float updir = (!CanUseRevZ() ? -1.0f : 1.0f);
      GLPolygonOffset(updir, updir);
    }
    // no cull?
    if (ri.hangup&4) glDisable(GL_CULL_FACE);
  }

  // translucent things should not modify z-buffer
  if (!restoreDepthMask && trans) {
    glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);
    glDepthMask(GL_FALSE); // no z-buffer writes
    restoreDepthMask = true;
  }

  // setup blending
  switch (ri.translucency) {
    case 1: // normal translucency
      //restoreBlend = true; // default blending
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      break;
    case 2: // additive translucency
      restoreBlend = true;
      glBlendFunc(GL_ONE, GL_ONE); // our source rgb is already premultiplied
      break;
    case 3: // translucent-dark (k8vavoom special)
      restoreBlend = true;
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
      break;
    case -1: // subtractive translucency
      // not implemented yet, sorry
      //restoreBlend = true; // default blending
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      break;
    default: // normal
      if (trans) {
        //restoreBlend = true; // default blending
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      }
      break;
  }


  TVec texpt(0, 0, 0);

  #define SPRVTX(shdr_,cv_)  do { \
    texpt = (cv_)-texorg; \
    (shdr_).SetTexCoordAttr( \
      DotProduct(texpt, saxis)*tex_iw, \
      DotProduct(texpt, taxis)*tex_ih); \
    /*(shdr_).UploadChangedAttrs();*/ \
    glVertex(cv_); \
  } while (0)

  glBegin(GL_QUADS);
    switch (shadtype) {
      case ShaderMasked:
        SPRVTX(SurfMasked, cv[0]);
        SPRVTX(SurfMasked, cv[1]);
        SPRVTX(SurfMasked, cv[2]);
        SPRVTX(SurfMasked, cv[3]);
        break;
      case ShaderMaskedBM:
        SPRVTX(SurfMaskedBrightmap, cv[0]);
        SPRVTX(SurfMaskedBrightmap, cv[1]);
        SPRVTX(SurfMaskedBrightmap, cv[2]);
        SPRVTX(SurfMaskedBrightmap, cv[3]);
        break;
      case ShaderStencil:
        SPRVTX(SurfMaskedStencil, cv[0]);
        SPRVTX(SurfMaskedStencil, cv[1]);
        SPRVTX(SurfMaskedStencil, cv[2]);
        SPRVTX(SurfMaskedStencil, cv[3]);
        break;
      case ShaderFakeShadow:
        SPRVTX(SurfMaskedFakeShadow, cv[0]);
        SPRVTX(SurfMaskedFakeShadow, cv[1]);
        SPRVTX(SurfMaskedFakeShadow, cv[2]);
        SPRVTX(SurfMaskedFakeShadow, cv[3]);
        break;
      default: Sys_Error("ketmar forgot some shader type in `VOpenGLDrawer::DrawSpritePolygon()`");
    }
  glEnd();

  #undef SPRVTX

  if (restoreDepthMask) glDepthMask(oldDepthMask);
  if (ri.hangup&2) GLDisableOffset();
  if (ri.hangup&4) glEnable(GL_CULL_FACE);
  if (restoreBlend) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  if (shadtype == ShaderMaskedBM) {
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
