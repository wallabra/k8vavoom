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
  if (surf->plane->PointOnSide(vieworg)) return; // viewer is in back side or on plane
  if (surf->count < 3) {
    if (developer) GCon->Logf(NAME_Dev, "trying to render masked surface with %d vertices", surf->count);
    return;
  }

  texinfo_t *tex = surf->texinfo;
  SetTexture(tex->Tex, tex->ColourMap);

  if (!gl_dbg_adv_render_textures_surface && RendLev->IsAdvancedRenderer()) return;

  p_glUseProgramObjectARB(SurfMasked_Program);
  p_glUniform1iARB(SurfMasked_TextureLoc, 0);
  //SurfMasked_Locs.storeFogType();

  bool zbufferWriteDisabled = false;
  bool decalsAllowed = false;
  bool restoreBlend = false;

  GLint oldDepthMask = 0;

  if (blend_sprites || Additive || Alpha < 1.0f) {
    //p_glUniform1fARB(SurfMaskedAlphaRefLoc, getAlphaThreshold());
    restoreBlend = true;
    p_glUniform1fARB(SurfMasked_AlphaRefLoc, (Additive ? getAlphaThreshold() : 0.666f));
    glEnable(GL_BLEND);
    if (Additive) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    // translucent things should not modify z-buffer
    if (Additive || Alpha < 1.0f) {
      zbufferWriteDisabled = true;
      glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);
      glDepthMask(GL_FALSE); // no z-buffer writes
    }
    if (r_decals_enabled && r_decals_wall_alpha && surf->dcseg && surf->dcseg->decals) {
      decalsAllowed = true;
    }
  } else {
    p_glUniform1fARB(SurfMasked_AlphaRefLoc, 0.666f);
    Alpha = 1.0f;
    if (r_decals_enabled && r_decals_wall_masked && surf->dcseg && surf->dcseg->decals) {
      decalsAllowed = true;
    }
  }

  if (surf->lightmap != nullptr || (!RendLev->IsAdvancedRenderer() && surf->dlightframe == RendLev->currDLightFrame)) {
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
    p_glUniform4fARB(SurfMasked_LightLoc, r*iscale, g*iscale, b*iscale, Alpha);
  } else {
    if (r_adv_masked_wall_vertex_light && RendLev->IsAdvancedRenderer()) {
      // collect vertex lighting
      //FIXME: this should be rendered in ambient pass instead
      //       also, we can subdivide surfaces for two-sided walls for
      //       better estimations
      int w = (surf->extents[0]>>4)+1;
      int h = (surf->extents[1]>>4)+1;
      float radius = MIN(w, h);
      if (radius < 0.0f) radius = 0.0f;
      int r = 0, g = 0, b = 0;
      // sector light
      if (r_allow_ambient) {
        int slins = (surf->Light>>24)&0xff;
        if (slins < r_ambient) slins = clampToByte(r_ambient);
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
      for (int i = 0; i < surf->count; ++i) {
        vuint32 lt0 = RendLev->LightPoint(surf->verts[i], radius, surf->plane);
        int lr = (lt0>>16)&255;
        int lg = (lt0>>8)&255;
        int lb = lt0&255;
        if (r < lr) r = lr;
        if (g < lg) g = lg;
        if (b < lb) b = lb;
      }
      p_glUniform4fARB(SurfMasked_LightLoc, r/255.0f, g/255.0f, b/255.0f, Alpha);
    } else {
      const float lev = getSurfLightLevel(surf);
      p_glUniform4fARB(SurfMasked_LightLoc,
        ((surf->Light>>16)&255)*lev/255.0f,
        ((surf->Light>>8)&255)*lev/255.0f,
        (surf->Light&255)*lev/255.0f, Alpha);
    }
  }

  SurfMasked_Locs.storeFogFade(surf->Fade, Alpha);

  bool doDecals = (decalsAllowed && tex->Tex && !tex->noDecals && surf->dcseg && surf->dcseg->decals);

  // fill stencil buffer for decals
  if (doDecals) RenderPrepareShaderDecals(surf);

  glBegin(GL_POLYGON);
  for (int i = 0; i < surf->count; ++i) {
    p_glVertexAttrib2fARB(SurfMasked_TexCoordLoc,
      (DotProduct(surf->verts[i], tex->saxis)+tex->soffs)*tex_iw,
      (DotProduct(surf->verts[i], tex->taxis)+tex->toffs)*tex_ih);
    glVertex(surf->verts[i]);
  }
  glEnd();

  // draw decals
  if (doDecals) {
    if (RenderFinishShaderDecals(DT_SIMPLE, surf, nullptr, tex->ColourMap)) {
      //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
      //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // decal renderer is using this too
      //p_glUseProgramObjectARB(SurfSimpleProgram);
      //return true;
    }
  }

  if (restoreBlend) {
    glDisable(GL_BLEND);
    if (zbufferWriteDisabled) glDepthMask(oldDepthMask); // restore z-buffer writes
  }
  if (Additive) {
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
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

  SetSpriteLump(Tex, Translation, CMap, true);
  //SetupTextureFiltering(noDepthChange ? 3 : sprite_filter);
  //SetupTextureFiltering(noDepthChange ? model_filter : sprite_filter);
  SetupTextureFiltering(sprite_filter);

  p_glUseProgramObjectARB(SurfMasked_Program);
  p_glUniform1iARB(SurfMasked_TextureLoc, 0);
  //SurfMasked_Locs.storeFogType();

  bool zbufferWriteDisabled = false;
  bool restoreBlend = false;

  GLint oldDepthMask = 0;

  if (blend_sprites || Additive || hangup || Alpha < 1.0f) {
    restoreBlend = true;
    p_glUniform1fARB(SurfMasked_AlphaRefLoc, (hangup || Additive ? getAlphaThreshold() : 0.666f));
    if (hangup) {
      zbufferWriteDisabled = true;
      glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);
      glDepthMask(GL_FALSE); // no z-buffer writes
      const float updir = (!CanUseRevZ() ? -1.0f : 1.0f);//*hangup;
      glPolygonOffset(updir, updir);
      glEnable(GL_POLYGON_OFFSET_FILL);
    }
    glEnable(GL_BLEND);
    // translucent things should not modify z-buffer
    if (!zbufferWriteDisabled && (Additive || Alpha < 1.0f)) {
      glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);
      glDepthMask(GL_FALSE); // no z-buffer writes
      zbufferWriteDisabled = true;
    }
    if (Additive) {
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    } else {
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
  } else {
    p_glUniform1fARB(SurfMasked_AlphaRefLoc, 0.666f);
    Alpha = 1.0f;
    glDisable(GL_BLEND);
  }

  //GCon->Logf("SPRITE: light=0x%08x; fade=0x%08x", light, Fade);
  //Fade ^= 0x00ffffff;
  //light = 0xffff0000;
  //Fade = 0x3f323232;
  /*
  if (Fade != FADE_LIGHT && RendLev->IsAdvancedRenderer()) {
    Fade ^= 0x00ffffff;
  }
  */

  p_glUniform4fARB(SurfMasked_LightLoc,
    ((light>>16)&255)/255.0f,
    ((light>>8)&255)/255.0f,
    (light&255)/255.0f, Alpha);

  SurfMasked_Locs.storeFogFade(Fade, Alpha);

  glBegin(GL_QUADS);
    texpt = cv[0]-texorg;
    p_glVertexAttrib2fARB(SurfMasked_TexCoordLoc,
      DotProduct(texpt, saxis)*tex_iw,
      DotProduct(texpt, taxis)*tex_ih);
    glVertex(cv[0]);

    texpt = cv[1]-texorg;
    p_glVertexAttrib2fARB(SurfMasked_TexCoordLoc,
      DotProduct(texpt, saxis)*tex_iw,
      DotProduct(texpt, taxis)*tex_ih);
    glVertex(cv[1]);

    texpt = cv[2]-texorg;
    p_glVertexAttrib2fARB(SurfMasked_TexCoordLoc,
      DotProduct(texpt, saxis)*tex_iw,
      DotProduct(texpt, taxis)*tex_ih);
    glVertex(cv[2]);

    texpt = cv[3]-texorg;
    p_glVertexAttrib2fARB(SurfMasked_TexCoordLoc,
      DotProduct(texpt, saxis)*tex_iw,
      DotProduct(texpt, taxis)*tex_ih);
    glVertex(cv[3]);
  glEnd();

  if (restoreBlend) {
    if (hangup) {
      glPolygonOffset(0.0f, 0.0f);
      glDisable(GL_POLYGON_OFFSET_FILL);
    }
    glDisable(GL_BLEND);
    if (zbufferWriteDisabled) glDepthMask(oldDepthMask); // restore z-buffer writes
    if (Additive) {
      //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
  }
}
