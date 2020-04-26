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


//==========================================================================
//
//  VOpenGLDrawer::DrawMaskedPolygon
//
//==========================================================================
void VOpenGLDrawer::DrawMaskedPolygon (surface_t *surf, float Alpha, bool Additive) {
  if (!surf->plvisible) return; // viewer is in back side or on plane
  if (surf->count < 3 || Alpha < 0.004f) return;

  texinfo_t *tex = surf->texinfo;
  if (!tex->Tex || tex->Tex->Type == TEXTYPE_Null) return;
  if (Alpha > 1.0f) Alpha = 1.0f; // just in case

  GlowParams gp;
  CalcGlow(gp, surf);

  SetTexture(tex->Tex, tex->ColorMap);

  const bool doBrightmap = (r_brightmaps && tex->Tex->Brightmap);
  const bool isAlphaTrans = (Alpha < 1.0f || tex->Tex->isTranslucent());
  //k8: non-translucent walls should not end here, so there is no need to recalc/check lightmaps
  const float lightLevel = getSurfLightLevel(surf);
  const bool zbufferWriteDisabled = (Additive || Alpha < 1.0f); // translucent things should not modify z-buffer

  bool doDecals = (tex->Tex && !tex->noDecals && surf->seg && surf->seg->decalhead && r_decals_enabled);
  if (doDecals) {
    if (Additive || isAlphaTrans) {
      if (!r_decals_wall_alpha) doDecals = false;
    } else {
      if (!r_decals_wall_masked) doDecals = false;
    }
  }

  GLuint attribPosition;

  if (doBrightmap) {
    SurfMaskedPolyBrightmapGlow.Activate();
    SurfMaskedPolyBrightmapGlow.SetBrightMapAdditive(r_brightmaps_additive ? 1.0f : 0.0f);
    SurfMaskedPolyBrightmapGlow.SetTexture(0);
    SurfMaskedPolyBrightmapGlow.SetTextureBM(1);
    SelectTexture(1);
    SetBrightmapTexture(tex->Tex->Brightmap);
    SelectTexture(0);
    if (gp.isActive()) {
      VV_GLDRAWER_ACTIVATE_GLOW(SurfMaskedPolyBrightmapGlow, gp);
    } else {
      VV_GLDRAWER_DEACTIVATE_GLOW(SurfMaskedPolyBrightmapGlow);
    }
    SurfMaskedPolyBrightmapGlow.SetAlphaRef(Additive || isAlphaTrans ? getAlphaThreshold() : 0.666f);
    SurfMaskedPolyBrightmapGlow.SetLight(
      ((surf->Light>>16)&255)*lightLevel/255.0f,
      ((surf->Light>>8)&255)*lightLevel/255.0f,
      (surf->Light&255)*lightLevel/255.0f, Alpha);
    SurfMaskedPolyBrightmapGlow.SetFogFade(surf->Fade, Alpha);
    SurfMaskedPolyBrightmapGlow.SetSpriteTex(TVec(tex->soffs, tex->toffs), tex->saxis, tex->taxis, tex_iw, tex_ih);
    attribPosition = SurfMaskedPolyBrightmapGlow.loc_Position;
  } else {
    SurfMaskedPolyGlow.Activate();
    SurfMaskedPolyGlow.SetTexture(0);
    //SurfMaskedPolyGlow.SetFogType();
    if (gp.isActive()) {
      VV_GLDRAWER_ACTIVATE_GLOW(SurfMaskedPolyGlow, gp);
    } else {
      VV_GLDRAWER_DEACTIVATE_GLOW(SurfMaskedPolyGlow);
    }
    SurfMaskedPolyGlow.SetAlphaRef(Additive || isAlphaTrans ? getAlphaThreshold() : 0.666f);
    SurfMaskedPolyGlow.SetLight(
      ((surf->Light>>16)&255)*lightLevel/255.0f,
      ((surf->Light>>8)&255)*lightLevel/255.0f,
      (surf->Light&255)*lightLevel/255.0f, Alpha);
    SurfMaskedPolyGlow.SetFogFade(surf->Fade, Alpha);
    SurfMaskedPolyGlow.SetSpriteTex(TVec(tex->soffs, tex->toffs), tex->saxis, tex->taxis, tex_iw, tex_ih);
    attribPosition = SurfMaskedPolyGlow.loc_Position;
  }

  if (Additive) glBlendFunc(GL_ONE, GL_ONE); // our source rgb is already premultiplied

  if (zbufferWriteDisabled) {
    PushDepthMask();
    glDepthMask(GL_FALSE); // no z-buffer writes
  }

  // fill stencil buffer for decals
  if (doDecals) RenderPrepareShaderDecals(surf);

  currentActiveShader->UploadChangedUniforms();
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
#if 0
  //glBegin(GL_POLYGON);
  glBegin(GL_TRIANGLE_FAN);
  for (int i = 0; i < surf->count; ++i) {
    if (doBrightmap) {
      SurfMaskedPolyBrightmapGlow.SetTexCoordAttr(
        (DotProduct(surf->verts[i].vec(), tex->saxis)+tex->soffs)*tex_iw,
        (DotProduct(surf->verts[i].vec(), tex->taxis)+tex->toffs)*tex_ih);
      //SurfMaskedPolyBrightmapGlow.UploadChangedAttrs();
    } else {
      SurfMaskedPolyGlow.SetTexCoordAttr(
        (DotProduct(surf->verts[i].vec(), tex->saxis)+tex->soffs)*tex_iw,
        (DotProduct(surf->verts[i].vec(), tex->taxis)+tex->toffs)*tex_ih);
      //SurfMaskedPolyGlow.UploadChangedAttrs();
    }
    glVertex(surf->verts[i].vec());
  }
  glEnd();
#else
  //vboMaskedSurf.activate();
  //vboMaskedSurf.ensure(surf->count);
  vboMaskedSurf.uploadData(surf->count, surf->verts);

  p_glEnableVertexAttribArrayARB(attribPosition);
  p_glVertexAttribPointerARB(attribPosition, 3, GL_FLOAT, false, sizeof(TVec), (void *)(0*sizeof(float)));
  p_glDrawArrays(GL_TRIANGLE_FAN, 0, surf->count);
  p_glDisableVertexAttribArrayARB(attribPosition);

  vboMaskedSurf.deactivate();
#endif
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);

  // draw decals
  if (doDecals) {
    if (RenderFinishShaderDecals(DT_SIMPLE, surf, nullptr, tex->ColorMap)) {
      Additive = true; // restore blending
    }
  }

  if (zbufferWriteDisabled) PopDepthMask();
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
//  WARNING! it may modify `cv`!
//
//==========================================================================
void VOpenGLDrawer::DrawSpritePolygon (float time, const TVec *cv, VTexture *Tex,
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
    ShaderFuzzy,
  };

  // ignore translucent textures here: some idiots are trying to make "smoothed" sprites in this manner
  bool resetDepthMask = (ri.alpha < 1.0f || ri.isAdditive()); // `true` means "depth write disabled"
  ShaderType shadtype = ShaderMasked;
  if (ri.isStenciled()) {
    shadtype = (ri.stencilColor&0x00ffffffu ? ShaderStencil : ShaderFakeShadow);
    if (ri.translucency != RenderStyleInfo::Normal && ri.translucency != RenderStyleInfo::Translucent) resetDepthMask = true;
  } else {
    if (r_brightmaps && r_brightmaps_sprite && Tex->Brightmap) shadtype = ShaderMaskedBM;
    if (ri.isTranslucent()) resetDepthMask = true;
    if (ri.translucency == RenderStyleInfo::Fuzzy) shadtype = ShaderFuzzy;
  }

  const bool trans = (ri.translucency || ri.alpha < 1.0f || Tex->isTranslucent());

  SetSpriteLump(Tex, Translation, CMap, true, (ri.isShaded() ? ri.stencilColor : 0u));
  SetupTextureFiltering(sprite_filter);

  GLuint attribPosition;
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
      SurfMasked.SetSpriteTex(texorg, saxis, taxis, tex_iw, tex_ih);
      attribPosition = SurfMasked.loc_Position;
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
      SurfMaskedBrightmap.SetSpriteTex(texorg, saxis, taxis, tex_iw, tex_ih);
      attribPosition = SurfMaskedBrightmap.loc_Position;
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
      SurfMaskedStencil.SetSpriteTex(texorg, saxis, taxis, tex_iw, tex_ih);
      attribPosition = SurfMaskedStencil.loc_Position;
      break;
    case ShaderFakeShadow:
      SurfMaskedFakeShadow.Activate();
      SurfMaskedFakeShadow.SetTexture(0);
      SurfMaskedFakeShadow.SetLight(
        ((ri.light>>16)&255)/255.0f,
        ((ri.light>>8)&255)/255.0f,
        (ri.light&255)/255.0f, ri.alpha);
      SurfMaskedFakeShadow.SetFogFade(ri.fade, ri.alpha);
      SurfMaskedFakeShadow.SetAlphaRef(trans ? getAlphaThreshold() : 0.666f);
      SurfMaskedFakeShadow.SetSpriteTex(texorg, saxis, taxis, tex_iw, tex_ih);
      attribPosition = SurfMaskedFakeShadow.loc_Position;
      break;
    case ShaderFuzzy:
      SurfMaskedFuzzy.Activate();
      SurfMaskedFuzzy.SetTexture(0);
      /*
      SurfMaskedFuzzy.SetLight(
        ((ri.light>>16)&255)/255.0f,
        ((ri.light>>8)&255)/255.0f,
        (ri.light&255)/255.0f, ri.alpha);
      */
      SurfMaskedFuzzy.SetTime(time);
      SurfMaskedFuzzy.SetFogFade(ri.fade, ri.alpha);
      SurfMaskedFuzzy.SetAlphaRef(trans ? getAlphaThreshold() : 0.666f);
      SurfMaskedFuzzy.SetSpriteTex(texorg, saxis, taxis, tex_iw, tex_ih);
      attribPosition = SurfMaskedFuzzy.loc_Position;
      break;
    default: Sys_Error("ketmar forgot some shader type in `VOpenGLDrawer::DrawSpritePolygon()`");
  }
  currentActiveShader->UploadChangedUniforms();

  // setup hangups
  if (ri.flags) {
    // no z-buffer?
    if (ri.flags&RenderStyleInfo::FlagNoDepthWrite) resetDepthMask = true;
    // offset?
    if (ri.flags&RenderStyleInfo::FlagOffset) {
      const float updir = (!CanUseRevZ() ? -1.0f : 1.0f);
      GLPolygonOffset(updir, updir);
    }
    // no cull?
    if (ri.flags&RenderStyleInfo::FlagNoCull) glDisable(GL_CULL_FACE);
  }

  // translucent things should not modify z-buffer
  // no
  /*
  if (!resetDepthMask && trans) {
    resetDepthMask = true;
  }
  */

  SetupBlending(ri);

  if (resetDepthMask) {
    PushDepthMask();
    glDepthMask(GL_FALSE); // no z-buffer writes
  }

  //vboSprite.activate();
  vboSprite.ensure(4);
  vboSprite.uploadData(4, cv);

  p_glEnableVertexAttribArrayARB(attribPosition);
  p_glVertexAttribPointerARB(attribPosition, 3, GL_FLOAT, false, sizeof(TVec), (void *)(0*sizeof(float)));
  /*
  p_glEnableVertexAttribArrayARB(attribTexCoord);
  p_glVertexAttribPointerARB(attribTexCoord, 2, GL_FLOAT, false, sizeof(SurfVertex), (void *)(3*sizeof(float)));
  */
  //glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, indices);
  #if 0
  const vuint8 indices[6] = { 0, 1, 2,  0, 2, 3 };
  p_glDrawRangeElements(GL_TRIANGLES, 0, 5, 6, GL_UNSIGNED_BYTE, indices);
  #else
  p_glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  #endif
  p_glDisableVertexAttribArrayARB(attribPosition);
  //p_glDisableVertexAttribArrayARB(attribTexCoord);

  vboSprite.deactivate();

  if (resetDepthMask) PopDepthMask();
  if (ri.flags&RenderStyleInfo::FlagOffset) GLDisableOffset();
  if (ri.flags&RenderStyleInfo::FlagNoCull) glEnable(GL_CULL_FACE);
  RestoreBlending(ri);

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

  currentActiveShader->UploadChangedUniforms();
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
  glBegin(GL_POLYGON);
    for (int i = 0; i < surf->count; ++i) glVertex(surf->verts[i].vec());
  glEnd();
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);

  // draw decals
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  GLEnableBlend();
  (void)RenderFinishShaderDecals(DT_ADVANCED, surf, nullptr, tex->ColorMap);
}
