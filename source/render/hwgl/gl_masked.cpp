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
#include "gl_local.h"
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
  if (!tex->Tex) return;

  GlowParams gp;
  CalcGlow(gp, surf);

  bool doBrightmap = (r_brightmaps && tex->Tex->Brightmap);

  if (doBrightmap) {
    SurfMaskedBrightmapGlow.Activate();
    SurfMaskedBrightmapGlow.SetBrightMapAdditive(r_brightmaps_additive ? 1.0f : 0.0f);
    SurfMaskedBrightmapGlow.SetTexture(0);
    SurfMaskedBrightmapGlow.SetTextureBM(1);
    p_glActiveTextureARB(GL_TEXTURE0+1);
    SetBrightmapTexture(tex->Tex->Brightmap);
    p_glActiveTextureARB(GL_TEXTURE0);
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

  if (blend_sprites || Additive || Alpha < 1.0f) {
    restoreBlend = true;
    if (doBrightmap) {
      SurfMaskedBrightmapGlow.SetAlphaRef(Additive ? getAlphaThreshold() : 0.666f);
    } else {
      SurfMaskedGlow.SetAlphaRef(Additive ? getAlphaThreshold() : 0.666f);
    }
    //glEnable(GL_BLEND);
    //glDisable(GL_BLEND);
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
    if (r_decals_enabled && r_decals_wall_alpha && surf->seg && surf->seg->decals) {
      decalsAllowed = true;
    }
  } else {
    //glDisable(GL_BLEND);
    //glEnable(GL_BLEND); // our texture will be premultiplied
    //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (doBrightmap) {
      SurfMaskedBrightmapGlow.SetAlphaRef(0.666f);
    } else {
      SurfMaskedGlow.SetAlphaRef(0.666f);
    }
    Alpha = 1.0f;
    if (r_decals_enabled && r_decals_wall_masked && surf->seg && surf->seg->decals) {
      decalsAllowed = true;
    }
  }

  if (Alpha == 1.0f && !Additive && (surf->lightmap != nullptr || (!RendLev->IsAdvancedRenderer() && surf->dlightframe == RendLev->currDLightFrame))) {
    RendLev->BuildLightMap(surf);
    int w = (surf->extents[0]>>4)+1;
    int h = (surf->extents[1]>>4)+1;
    int size = w*h;
    int r = 0;
    int g = 0;
    int b = 0;
    for (int i = 0; i < size; ++i) {
      r += 255*256-blocklightsr[i];
      g += 255*256-blocklightsg[i];
      b += 255*256-blocklightsb[i];
    }
    double iscale = 1.0f/(size*255*256);
    if (doBrightmap) {
      SurfMaskedBrightmapGlow.SetLight(r*iscale, g*iscale, b*iscale, Alpha);
    } else {
      SurfMaskedGlow.SetLight(r*iscale, g*iscale, b*iscale, Alpha);
    }
  } else {
    /* k8: this seems to be stupid
    if (r_adv_masked_wall_vertex_light) {
      // collect vertex lighting
      //FIXME: this should be rendered in ambient pass instead
      //       also, we can subdivide surfaces for two-sided walls for
      //       better estimations
      int w = (surf->extents[0]>>4)+1;
      int h = (surf->extents[1]>>4)+1;
      float radius = min2(w, h);
      if (radius < 0.0f) radius = 0.0f;
      int r = 0, g = 0, b = 0;
      // sector light
      if (r_allow_ambient) {
        int slins = (surf->Light>>24)&0xff;
        if (slins < r_ambient_min) slins = clampToByte(r_ambient_min);
        int lr = (surf->Light>>16)&255;
        int lg = (surf->Light>>8)&255;
        int lb = surf->Light&255;
        lr = lr*slins/255;
        lg = lg*slins/255;
        lb = lb*slins/255;
        if (r < lr) r = lr;
        if (g < lg) g = lg;
        if (b < lb) b = lb;
      }
      TPlane pl;
      surf->GetPlane(&pl);
      for (int i = 0; i < surf->count; ++i) {
        vuint32 lt0 = RendLev->LightPoint(surf->verts[i], radius, 0, &pl);
        int lr = (lt0>>16)&255;
        int lg = (lt0>>8)&255;
        int lb = lt0&255;
        if (r < lr) r = lr;
        if (g < lg) g = lg;
        if (b < lb) b = lb;
      }
      if (doBrightmap) {
        SurfMaskedBrightmapGlow.SetLight(r/255.0f, g/255.0f, b/255.0f, Alpha);
      } else {
        SurfMaskedGlow.SetLight(r/255.0f, g/255.0f, b/255.0f, Alpha);
      }
    } else
    */
    {
      const float lev = getSurfLightLevel(surf);
      if (doBrightmap) {
        SurfMaskedBrightmapGlow.SetLight(
          ((surf->Light>>16)&255)*lev/255.0f,
          ((surf->Light>>8)&255)*lev/255.0f,
          (surf->Light&255)*lev/255.0f, Alpha);
      } else {
        SurfMaskedGlow.SetLight(
          ((surf->Light>>16)&255)*lev/255.0f,
          ((surf->Light>>8)&255)*lev/255.0f,
          (surf->Light&255)*lev/255.0f, Alpha);
      }
    }
  }

  if (doBrightmap) {
    SurfMaskedBrightmapGlow.SetFogFade(surf->Fade, Alpha);
  } else {
    SurfMaskedGlow.SetFogFade(surf->Fade, Alpha);
  }

  bool doDecals = (decalsAllowed && tex->Tex && !tex->noDecals && surf->seg && surf->seg->decals);

  // fill stencil buffer for decals
  if (doDecals) RenderPrepareShaderDecals(surf);

  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
  //glBegin(GL_POLYGON);
  glBegin(GL_TRIANGLE_FAN);
  for (int i = 0; i < surf->count; ++i) {
    if (doBrightmap) {
      SurfMaskedBrightmapGlow.SetTexCoord(
        (DotProduct(surf->verts[i], tex->saxis)+tex->soffs)*tex_iw,
        (DotProduct(surf->verts[i], tex->taxis)+tex->toffs)*tex_ih);
    } else {
      SurfMaskedGlow.SetTexCoord(
        (DotProduct(surf->verts[i], tex->saxis)+tex->soffs)*tex_iw,
        (DotProduct(surf->verts[i], tex->taxis)+tex->toffs)*tex_ih);
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
    //glDisable(GL_BLEND);
    if (zbufferWriteDisabled) glDepthMask(oldDepthMask); // restore z-buffer writes
  }
  if (Additive) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  if (doBrightmap) {
    p_glActiveTextureARB(GL_TEXTURE0+1);
    glBindTexture(GL_TEXTURE_2D, 0);
    p_glActiveTextureARB(GL_TEXTURE0);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::DrawSpritePolygon
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
  if (!Tex) return; // just in case

  TVec texpt(0, 0, 0);

  bool doBrightmap = (r_brightmaps && r_brightmaps_sprite && Tex->Brightmap);
  bool styleDark = (Alpha >= 1000.0f);
  if (styleDark) Alpha -= 1666.0f;

  if (doBrightmap) {
    //GCon->Logf("BRMAP for '%s' (%s)", *Tex->Name, *Tex->Brightmap->Name);
    SurfMaskedBrightmap.Activate();
    SurfMaskedBrightmap.SetBrightMapAdditive(r_brightmaps_additive ? 1.0f : 0.0f);
    SurfMaskedBrightmap.SetTexture(0);
    SurfMaskedBrightmap.SetTextureBM(1);
    p_glActiveTextureARB(GL_TEXTURE0+1);
    SetBrightmapTexture(Tex->Brightmap);
    p_glActiveTextureARB(GL_TEXTURE0);
  } else {
    SurfMasked.Activate();
    SurfMasked.SetTexture(0);
    //SurfMasked.SetFogType();
  }

  SetSpriteLump(Tex, Translation, CMap, true);
  //SetupTextureFiltering(noDepthChange ? 3 : sprite_filter);
  //SetupTextureFiltering(noDepthChange ? model_filter : sprite_filter);
  SetupTextureFiltering(sprite_filter);

  bool zbufferWriteDisabled = false;
  bool restoreBlend = false;

  GLint oldDepthMask = 0;

  if (blend_sprites || Additive || hangup || Alpha < 1.0f) {
    restoreBlend = true;
    if (doBrightmap) {
      SurfMaskedBrightmap.SetAlphaRef(hangup || Additive ? getAlphaThreshold() : 0.666f);
    } else {
      SurfMasked.SetAlphaRef(hangup || Additive ? getAlphaThreshold() : 0.666f);
    }
    if (hangup) {
      zbufferWriteDisabled = true;
      glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);
      glDepthMask(GL_FALSE); // no z-buffer writes
      const float updir = (!CanUseRevZ() ? -1.0f : 1.0f);//*hangup;
      glPolygonOffset(updir, updir);
      glEnable(GL_POLYGON_OFFSET_FILL);
    }
    //glEnable(GL_BLEND);
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
      SurfMaskedBrightmap.SetAlphaRef(0.666f);
    } else {
      SurfMasked.SetAlphaRef(0.666f);
    }
    Alpha = 1.0f;
    //glDisable(GL_BLEND);
    //glEnable(GL_BLEND);
  }

  //glEnable(GL_BLEND);
  if (styleDark) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied

  //GCon->Logf("SPRITE: light=0x%08x; fade=0x%08x", light, Fade);
  //Fade ^= 0x00ffffff;
  //light = 0xffff0000;
  //Fade = 0x3f323232;
  /*
  if (Fade != FADE_LIGHT && RendLev->IsAdvancedRenderer()) {
    Fade ^= 0x00ffffff;
  }
  */

  //GCon->Logf("Tex=%s; Fade=0x%08x; light=0x%08x; alpha=%f", *Tex->Name, Fade, light, Alpha);
  //Fade = 0xff505050;

  if (doBrightmap) {
    SurfMaskedBrightmap.SetLight(
      ((light>>16)&255)/255.0f,
      ((light>>8)&255)/255.0f,
      (light&255)/255.0f, Alpha);
    SurfMaskedBrightmap.SetFogFade(Fade, Alpha);

    glBegin(GL_QUADS);
      texpt = cv[0]-texorg;
      SurfMaskedBrightmap.SetTexCoord(
        DotProduct(texpt, saxis)*tex_iw,
        DotProduct(texpt, taxis)*tex_ih);
      glVertex(cv[0]);

      texpt = cv[1]-texorg;
      SurfMaskedBrightmap.SetTexCoord(
        DotProduct(texpt, saxis)*tex_iw,
        DotProduct(texpt, taxis)*tex_ih);
      glVertex(cv[1]);

      texpt = cv[2]-texorg;
      SurfMaskedBrightmap.SetTexCoord(
        DotProduct(texpt, saxis)*tex_iw,
        DotProduct(texpt, taxis)*tex_ih);
      glVertex(cv[2]);

      texpt = cv[3]-texorg;
      SurfMaskedBrightmap.SetTexCoord(
        DotProduct(texpt, saxis)*tex_iw,
        DotProduct(texpt, taxis)*tex_ih);
      glVertex(cv[3]);
    glEnd();
  } else {
    SurfMasked.SetLight(
      ((light>>16)&255)/255.0f,
      ((light>>8)&255)/255.0f,
      (light&255)/255.0f, Alpha);
    SurfMasked.SetFogFade(Fade, Alpha);

    glBegin(GL_QUADS);
      texpt = cv[0]-texorg;
      SurfMasked.SetTexCoord(
        DotProduct(texpt, saxis)*tex_iw,
        DotProduct(texpt, taxis)*tex_ih);
      glVertex(cv[0]);

      texpt = cv[1]-texorg;
      SurfMasked.SetTexCoord(
        DotProduct(texpt, saxis)*tex_iw,
        DotProduct(texpt, taxis)*tex_ih);
      glVertex(cv[1]);

      texpt = cv[2]-texorg;
      SurfMasked.SetTexCoord(
        DotProduct(texpt, saxis)*tex_iw,
        DotProduct(texpt, taxis)*tex_ih);
      glVertex(cv[2]);

      texpt = cv[3]-texorg;
      SurfMasked.SetTexCoord(
        DotProduct(texpt, saxis)*tex_iw,
        DotProduct(texpt, taxis)*tex_ih);
      glVertex(cv[3]);
    glEnd();
  }

  if (restoreBlend) {
    if (hangup) {
      glPolygonOffset(0.0f, 0.0f);
      glDisable(GL_POLYGON_OFFSET_FILL);
    }
    //glDisable(GL_BLEND);
    if (zbufferWriteDisabled) glDepthMask(oldDepthMask); // restore z-buffer writes
    if (Additive) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  }
  if (styleDark) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  if (doBrightmap) {
    p_glActiveTextureARB(GL_TEXTURE0+1);
    glBindTexture(GL_TEXTURE_2D, 0);
    p_glActiveTextureARB(GL_TEXTURE0);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::BeginTranslucentPolygonAmbient
//
//==========================================================================
void VOpenGLDrawer::BeginTranslucentPolygonAmbient () {
  glEnable(GL_BLEND);
  //glGetIntegerv(GL_DEPTH_WRITEMASK, &savedDepthMask);
  glDepthMask(GL_FALSE); // no z-buffer writes
  glEnable(GL_TEXTURE_2D);
  glDisable(GL_STENCIL_TEST);
  glStencilFunc(GL_ALWAYS, 0x0, 0xff);
  p_glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_KEEP);
  p_glStencilOpSeparate(GL_BACK,  GL_KEEP, GL_KEEP, GL_KEEP);
  glDisable(GL_SCISSOR_TEST);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDisable(GL_POLYGON_OFFSET_FILL);
  glEnable(GL_CULL_FACE);
  RestoreDepthFunc();
  //glDisable(GL_BLEND);
}


//==========================================================================
//
//  VOpenGLDrawer::EndTranslucentPolygonAmbient
//
//==========================================================================
/*
void VOpenGLDrawer::EndTranslucentPolygonAmbient () {
  glDisable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  //glDepthMask(savedDepthMask); // restore z-buffer writes
  glDisable(GL_TEXTURE_2D);
}
*/


//==========================================================================
//
//  VOpenGLDrawer::DrawTranslucentPolygonAmbient
//
//==========================================================================
void VOpenGLDrawer::DrawTranslucentPolygonAmbient (surface_t *surf, float Alpha, bool Additive) {
  if (!surf->plvisible) return; // viewer is in back side or on plane
  if (surf->count < 3) return;

  texinfo_t *tex = surf->texinfo;

  if (!tex->Tex) return;

  GlowParams gp;
  CalcGlow(gp, surf);

  bool doBrightmap = (r_brightmaps && tex->Tex->Brightmap);

  if (doBrightmap) {
    ShadowsAmbientTransBrightmap.Activate();
    ShadowsAmbientTransBrightmap.SetBrightMapAdditive(r_brightmaps_additive ? 1.0f : 0.0f);
    ShadowsAmbientTransBrightmap.SetTexture(0);
    ShadowsAmbientTransBrightmap.SetTextureBM(1);
    p_glActiveTextureARB(GL_TEXTURE0+1);
    SetBrightmapTexture(tex->Tex->Brightmap);
    p_glActiveTextureARB(GL_TEXTURE0);
    if (gp.isActive()) {
      VV_GLDRAWER_ACTIVATE_GLOW(ShadowsAmbientTransBrightmap, gp);
    } else {
      VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbientTransBrightmap);
    }
  } else {
    ShadowsAmbientTrans.Activate();
    ShadowsAmbientTrans.SetTexture(0);
    //ShadowsAmbientTrans.SetFogType();
    if (gp.isActive()) {
      VV_GLDRAWER_ACTIVATE_GLOW(ShadowsAmbientTrans, gp);
    } else {
      VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbientTrans);
    }
  }

  SetTexture(tex->Tex, tex->ColorMap);
  if (doBrightmap) ShadowsAmbientTransBrightmap.SetTex(tex); else ShadowsAmbientTrans.SetTex(tex);

  if (Additive) {
    glBlendFunc(GL_ONE, GL_ONE); // our source rgb is already premultiplied
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  } else {
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }

  //GCon->Logf("sl=0x%08x (%g : %d)", (unsigned)surf->Light, Alpha, (int)Additive);
  if (doBrightmap) {
    ShadowsAmbientTransBrightmap.SetLight(
      ((surf->Light>>16)&255)*255.0f,
      ((surf->Light>>8)&255)*255.0f,
      (surf->Light&255)*255.0f, Alpha);
  } else {
    ShadowsAmbientTrans.SetLight(
      ((surf->Light>>16)&255)*255.0f,
      ((surf->Light>>8)&255)*255.0f,
      (surf->Light&255)*255.0f, Alpha);
  }

  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
  glBegin(GL_POLYGON);
    for (int i = 0; i < surf->count; ++i) glVertex(surf->verts[i]);
  glEnd();
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);

  if (doBrightmap) {
    p_glActiveTextureARB(GL_TEXTURE0+1);
    glBindTexture(GL_TEXTURE_2D, 0);
    p_glActiveTextureARB(GL_TEXTURE0);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::BeginTranslucentPolygonDecals
//
//==========================================================================
void VOpenGLDrawer::BeginTranslucentPolygonDecals () {
  glEnable(GL_BLEND);
  //glGetIntegerv(GL_DEPTH_WRITEMASK, &savedDepthMask);
  glDepthMask(GL_FALSE); // no z-buffer writes
  glEnable(GL_TEXTURE_2D);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_SCISSOR_TEST);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDisable(GL_POLYGON_OFFSET_FILL);
  glEnable(GL_CULL_FACE);
  RestoreDepthFunc();
  //glDisable(GL_BLEND);
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
  bool doDecals = (r_decals_enabled && tex->Tex && !tex->noDecals && surf->seg && surf->seg->decals);
  if (!doDecals) return;

  // fill stencil buffer for decals
  RenderPrepareShaderDecals(surf);

  ShadowsSurfTransDecals.Activate();
  ShadowsSurfTransDecals.SetTexture(0);
  glDisable(GL_BLEND);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

  SetTexture(tex->Tex, tex->ColorMap);

  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
  glBegin(GL_POLYGON);
    for (int i = 0; i < surf->count; ++i) glVertex(surf->verts[i]);
  glEnd();
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);

  // draw decals
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glEnable(GL_BLEND);
  (void)RenderFinishShaderDecals(DT_ADVANCED, surf, nullptr, tex->ColorMap);
}
