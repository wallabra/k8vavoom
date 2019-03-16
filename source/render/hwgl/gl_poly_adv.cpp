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


// ////////////////////////////////////////////////////////////////////////// //
extern "C" {
  static inline int compareSurfaces (const surface_t *sa, const surface_t *sb) {
    if (sa == sb) return 0;
    const texinfo_t *ta = sa->texinfo;
    const texinfo_t *tb = sb->texinfo;
    if ((uintptr_t)ta->Tex < (uintptr_t)ta->Tex) return -1;
    if ((uintptr_t)tb->Tex > (uintptr_t)tb->Tex) return 1;
    return ((int)ta->ColourMap)-((int)tb->ColourMap);
  }

  static int drawListItemCmp (const void *a, const void *b, void *udata) {
    return compareSurfaces(*(const surface_t **)a, *(const surface_t **)b);
  }
}


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
  // draw horizons
  {
    surface_t **surfptr = RendLev->DrawHorizonList.ptr();
    for (int count = RendLev->DrawHorizonList.length(); count--; ++surfptr) {
      surface_t *surf = *surfptr;
      if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
      DoHorizonPolygon(surf);
    }
  }

  // set z-buffer for skies
  if (RendLev->DrawSkyList.length()) {
    p_glUseProgramObjectARB(SurfZBuf_Program);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    surface_t **surfptr = RendLev->DrawSkyList.ptr();
    for (int count = RendLev->DrawSkyList.length(); count--; ++surfptr) {
      surface_t *surf = *surfptr;
      if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
      if (surf->count < 3) {
        if (developer) GCon->Logf(NAME_Dev, "trying to render sky portal surface with %d vertices", surf->count);
        continue;
      }
      glBegin(GL_POLYGON);
      for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
      glEnd();
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }

  // draw normal surfaces
  if (RendLev->DrawSurfList.length()) {
    p_glUseProgramObjectARB(ShadowsAmbient_Program);
    ShadowsAmbient_Locs.storeTexture(0);

    // other passes can skip surface sorting
    if (gl_sort_textures) timsort_r(RendLev->DrawSurfList.ptr(), RendLev->DrawSurfList.length(), sizeof(surface_t *), &drawListItemCmp, nullptr);

    const texinfo_t *lastTexinfo = nullptr;
    surface_t **surfptr = RendLev->DrawSurfList.ptr();
    for (int count = RendLev->DrawSurfList.length(); count--; ++surfptr) {
      surface_t *surf = *surfptr;
      if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
      if (surf->count < 3) {
        if (developer) GCon->Logf(NAME_Dev, "trying to render simple ambient surface with %d vertices", surf->count);
        continue;
      }

      // don't render translucent surfaces
      // they should not end up here, but...
      const texinfo_t *currTexinfo = surf->texinfo;
      if (!currTexinfo || !currTexinfo->Tex || currTexinfo->Tex->Type == TEXTYPE_Null) continue;
      if (currTexinfo->Alpha < 1.0f) continue;
      bool textureChanded =
        !lastTexinfo ||
        lastTexinfo != currTexinfo ||
        lastTexinfo->Tex != currTexinfo->Tex ||
        lastTexinfo->ColourMap != currTexinfo->ColourMap;
      lastTexinfo = currTexinfo;

      if (textureChanded) {
        SetTexture(currTexinfo->Tex, currTexinfo->ColourMap);
        ShadowsAmbient_Locs.storeTextureParams(currTexinfo);
      }

      const float lev = getSurfLightLevel(surf);
      p_glUniform4fARB(ShadowsAmbient_LightLoc,
        ((surf->Light>>16)&255)*lev/255.0f,
        ((surf->Light>>8)&255)*lev/255.0f,
        (surf->Light&255)*lev/255.0f, 1.0f);

      glBegin(GL_POLYGON);
      for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
      glEnd();
    }
  }
}


//==========================================================================
//
//  VOpenGLDrawer::BeginShadowVolumesPass
//
//  setup general rendering parameters for shadow volume rendering
//
//==========================================================================
void VOpenGLDrawer::BeginShadowVolumesPass () {
  glEnable(GL_STENCIL_TEST);
  glDepthMask(GL_FALSE); // no z-buffer writes
}


//==========================================================================
//
//  VOpenGLDrawer::BeginLightShadowVolumes
//
//  setup rendering parameters for shadow volume rendering
//
//==========================================================================
void VOpenGLDrawer::BeginLightShadowVolumes (const TVec &LightPos, const float Radius, bool useZPass, bool hasScissor, const int scoords[4]) {
  glDisable(GL_TEXTURE_2D);
  if (hasScissor) {
    if (gl_use_stencil_quad_clear) {
      //GLog.Logf("SCISSOR CLEAR: (%d,%d)-(%d,%d)", scoords[0], scoords[1], scoords[2], scoords[3]);
      //GLint oldStencilTest;
      //glGetIntegerv(GL_STENCIL_TEST, &oldStencilTest);
      GLint glmatmode;
      glGetIntegerv(GL_MATRIX_MODE, &glmatmode);
      GLint oldDepthTest;
      glGetIntegerv(GL_DEPTH_TEST, &oldDepthTest);
      GLint oldDepthMask;
      glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);

      //glDisable(GL_STENCIL_TEST);
      glEnable(GL_SCISSOR_TEST);
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_CULL_FACE);
      glDisable(GL_BLEND);
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

      glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
      glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
      //glMatrixMode(GL_TEXTURE); glPushMatrix();
      //glMatrixMode(GL_COLOR); glPushMatrix();

      p_glUseProgramObjectARB(0);
      glStencilFunc(GL_ALWAYS, 0x0, 0xff);
      glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);

      glOrtho(0, ScreenWidth, ScreenHeight, 0, -666, 666);
      //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glBegin(GL_QUADS);
        glVertex2i(0, 0);
        glVertex2i(ScreenWidth, 0);
        glVertex2i(ScreenWidth, ScreenHeight);
        glVertex2i(0, ScreenHeight);
      glEnd();
      //glBindTexture(GL_TEXTURE_2D, 0);

      //glDisable(GL_STENCIL_TEST);
      //if (oldStencilTest) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
      glMatrixMode(GL_PROJECTION); glPopMatrix();
      glMatrixMode(GL_MODELVIEW); glPopMatrix();
      glMatrixMode(glmatmode);
      glDepthMask(oldDepthMask);
      if (oldDepthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    } else {
      glEnable(GL_SCISSOR_TEST);
      ClearStencilBuffer();
    }
  } else {
    glDisable(GL_SCISSOR_TEST);
    ClearStencilBuffer();
  }
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  //!glEnable(GL_POLYGON_OFFSET_FILL);

  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glStencilFunc(GL_ALWAYS, 0x0, 0xff);

  if (!CanUseRevZ()) {
    // normal
    //k8: this seems to be unnecessary
    if (!gl_dbg_adv_render_never_offset_shadow_volume) {
      if (gl_dbg_adv_render_offset_shadow_volume || !usingFPZBuffer) {
        glPolygonOffset(1.0f, 10.0f);
        glEnable(GL_POLYGON_OFFSET_FILL);
      }
    }
    glDepthFunc(GL_LESS);
    //glDepthFunc(GL_LEQUAL);
  } else {
    // reversed
    //k8: this seems to be unnecessary
    if (!gl_dbg_adv_render_never_offset_shadow_volume) {
      if (gl_dbg_adv_render_offset_shadow_volume) {
        glPolygonOffset(-1.0f, -10.0f);
        glEnable(GL_POLYGON_OFFSET_FILL);
      }
    }
    glDepthFunc(GL_GREATER);
    //glDepthFunc(GL_GEQUAL);
  }
  // face, stencil-fail, depth-fail, depth-pass

  usingZPass = useZPass;

  if (gl_dbg_use_zpass || useZPass) {
    // a typical setup for z-pass method
    p_glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP_EXT);
    p_glStencilOpSeparate(GL_BACK,  GL_KEEP, GL_KEEP, GL_DECR_WRAP_EXT);
  } else {
    // a typical setup for z-fail method
    p_glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);
    p_glStencilOpSeparate(GL_BACK,  GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);
  }

  if (HaveDepthClamp) {
    p_glUseProgramObjectARB(SurfShadowVolume_Program);
    p_glUniform3fvARB(SurfShadowVolume_LightPosLoc, 1, &LightPos.x);
  } else {
    // manual...
    p_glUseProgramObjectARB(SurfZBuf_Program);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::EndLightShadowVolumes
//
//==========================================================================
void VOpenGLDrawer::EndLightShadowVolumes () {
  //RestoreDepthFunc(); // no need to do this, if will be modified anyway
  // meh, just turn if off each time
  /*if (gl_dbg_adv_render_offset_shadow_volume || !usingFPZBuffer)*/ {
    glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(0.0f, 0.0f);
  }
  //glDisable(GL_SCISSOR_TEST);
  //glEnable(GL_TEXTURE_2D);
}


//==========================================================================
//
//  VOpenGLDrawer::RenderSurfaceShadowVolume
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
void VOpenGLDrawer::RenderSurfaceShadowVolume (const surface_t *surf, const TVec &LightPos, float Radius) {
  if (surf->count < 3) return; // just in case

  //FIXME: move this to drawer class
  static TVec *poolVec = nullptr;
  static int poolVecSize = 0;

  if (!HaveDepthClamp) {
    if (poolVecSize < surf->count) {
      poolVecSize = (surf->count|0xfff)+1;
      poolVec = (TVec *)Z_Realloc(poolVec, poolVecSize*sizeof(TVec));
    }
  }

  const unsigned vcount = (unsigned)surf->count;
  const TVec *sverts = surf->verts;

  const TVec *v = (HaveDepthClamp ? sverts : poolVec);

  // OpenGL renders vertices with zero `w` as infinitely far -- this is exactly what we want
  if (!HaveDepthClamp) {
    // if we don't have depth clamping, use this approach (otherwise our vertex shader will do the work)
    for (unsigned i = 0; i < vcount; ++i) {
      poolVec[i] = (surf->verts[i]-LightPos).normalised();
      poolVec[i] *= M_INFINITY;
      poolVec[i] += LightPos;
    }
  }

  if (!usingZPass && !gl_dbg_use_zpass) {
    // far cap
    glBegin(GL_POLYGON);
    if (HaveDepthClamp) {
      for (unsigned i = vcount; i--; ) glVertex4(v[i], 0);
    } else {
      for (unsigned i = vcount; i--; ) glVertex(v[i]);
    }
    glEnd();

    // near cap
    glBegin(GL_POLYGON);
    for (unsigned i = 0; i < vcount; ++i) glVertex(sverts[i]);
    glEnd();
  }

  glBegin(GL_TRIANGLE_STRIP);
  if (HaveDepthClamp) {
    for (unsigned i = 0; i < vcount; ++i) {
      glVertex(sverts[i]);
      glVertex4(v[i], 0);
    }
    glVertex(sverts[0]);
    glVertex4(v[0], 0);
  } else {
    for (unsigned i = 0; i < vcount; ++i) {
      glVertex(sverts[i]);
      glVertex(v[i]);
    }
    glVertex(sverts[0]);
    glVertex(v[0]);
  }
  glEnd();

  NoteStencilBufferDirty();
}


//==========================================================================
//
//  VOpenGLDrawer::BeginLightPass
//
//  setup rendering parameters for lighted surface rendering
//
//==========================================================================
void VOpenGLDrawer::BeginLightPass (const TVec &LightPos, float Radius, vuint32 Colour, bool doShadow) {
  RestoreDepthFunc();
  glDepthMask(GL_FALSE); // no z-buffer writes
  glDisable(GL_TEXTURE_2D);

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  if (gl_dbg_use_zpass > 1) {
    glStencilFunc(GL_EQUAL, 0x1, 0xff);
  } else {
    glStencilFunc(GL_EQUAL, 0x0, 0xff);
  }
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

  // do not use stencil test if we rendered no shadow surfaces
  if (doShadow && IsStencilBufferDirty()) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glEnable(GL_BLEND);

  glDepthFunc(GL_EQUAL);

  p_glUseProgramObjectARB(ShadowsLight_Program);
  p_glUniform3fvARB(ShadowsLight_LightPosLoc, 1, &LightPos.x);
  p_glUniform1fARB(ShadowsLight_LightRadiusLoc, Radius);
  p_glUniform3fARB(ShadowsLight_LightColourLoc, ((Colour>>16)&255)/255.0f, ((Colour>>8)&255)/255.0f, (Colour&255)/255.0f);
}


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
  //if (surf->plane->PointOnSide(vieworg)) return; // viewer is in back side or on plane
  if (surf->count < 3) {
    if (developer) GCon->Logf(NAME_Dev, "trying to render light surface with %d vertices", surf->count);
    return;
  }

  const texinfo_t *tex = surf->texinfo;
  SetTexture(tex->Tex, tex->ColourMap);

  ShadowsLight_Locs.storeTexture(0);
  ShadowsLight_Locs.storeTextureParams(tex);
  p_glVertexAttrib3fvARB(ShadowsLight_SurfNormalLoc, &surf->plane->normal.x);
  p_glVertexAttrib1fvARB(ShadowsLight_SurfDistLoc, &surf->plane->dist);
  p_glUniform3fARB(ShadowsLight_ViewOriginLoc, vieworg.x, vieworg.y, vieworg.z);

  glBegin(GL_POLYGON);
  for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
  glEnd();
}


//==========================================================================
//
//  VOpenGLDrawer::DrawWorldTexturesPass
//
//  this renders textured level with ambient lighting applied
//  this is for advanced renderer only
//  depth mask should be off
//
//==========================================================================
void VOpenGLDrawer::DrawWorldTexturesPass () {
  // stop stenciling now
  glDisable(GL_STENCIL_TEST);
  glDepthMask(GL_FALSE); // no z-buffer writes
  glEnable(GL_TEXTURE_2D);

  // copy ambient light texture to FBO, so we can use it to light decals
  if (p_glBlitFramebuffer) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, mainFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ambLightFBO);
    p_glBlitFramebuffer(0, 0, ScreenWidth, ScreenHeight, 0, 0, ScreenWidth, ScreenHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, mainFBO);
  } else {
    glPushAttrib(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_ENABLE_BIT|GL_VIEWPORT_BIT|GL_TRANSFORM_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, ambLightFBO);
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


  glBlendFunc(GL_DST_COLOR, GL_ZERO);
  glEnable(GL_BLEND);

  if (!gl_dbg_adv_render_textures_surface || RendLev->DrawSurfList.length() == 0) return;

  p_glUseProgramObjectARB(ShadowsTexture_Program);
  //p_glUniform1iARB(ShadowsTexture_TextureLoc, 0);
  ShadowsTexture_Locs.storeTexture(0);

  // no need to sort surfaces there, it is already done in ambient pass
  const texinfo_t *lastTexinfo = nullptr;
  surface_t **surfptr = RendLev->DrawSurfList.ptr();
  for (int count = RendLev->DrawSurfList.length(); count--; ++surfptr) {
    surface_t *surf = *surfptr;
    if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
    if (surf->count < 3) {
      if (developer) GCon->Logf(NAME_Dev, "trying to render texture surface with %d vertices", surf->count);
      continue;
    }

    // don't render translucent surfaces
    // they should not end up here, but...
    const texinfo_t *currTexinfo = surf->texinfo;
    if (!currTexinfo || !currTexinfo->Tex || currTexinfo->Tex->Type == TEXTYPE_Null) continue;
    if (currTexinfo->Alpha < 1.0f) continue;

    bool textureChanded =
      !lastTexinfo ||
      lastTexinfo != currTexinfo ||
      lastTexinfo->Tex != currTexinfo->Tex ||
      lastTexinfo->ColourMap != currTexinfo->ColourMap;
    lastTexinfo = currTexinfo;

    if (textureChanded) {
      SetTexture(currTexinfo->Tex, currTexinfo->ColourMap);
      ShadowsTexture_Locs.storeTextureParams(currTexinfo);
    }

    bool doDecals = (currTexinfo->Tex && !currTexinfo->noDecals && surf->dcseg && surf->dcseg->decals);

    // fill stencil buffer for decals
    if (doDecals) RenderPrepareShaderDecals(surf);

    glBegin(GL_POLYGON);
    for (unsigned i = 0; i < (unsigned)surf->count; ++i) {
      /*
      p_glVertexAttrib2fARB(ShadowsTexture_TexCoordLoc,
        (DotProduct(surf->verts[i], currTexinfo->saxis)+currTexinfo->soffs)*tex_iw,
        (DotProduct(surf->verts[i], currTexinfo->taxis)+currTexinfo->toffs)*tex_ih);
      */
      glVertex(surf->verts[i]);
    }
    glEnd();

    if (doDecals) {
      if (RenderFinishShaderDecals(DT_ADVANCED, surf, nullptr, currTexinfo->ColourMap)) {
        p_glUseProgramObjectARB(ShadowsTexture_Program);
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
        //glEnable(GL_BLEND);
        lastTexinfo = nullptr; // resetup texture
      }
    }
  }
}


//==========================================================================
//
//  VOpenGLDrawer::DrawWorldFogPass
//
//==========================================================================
void VOpenGLDrawer::DrawWorldFogPass () {
  glEnable(GL_BLEND);
  //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE); // no z-buffer writes

  // draw surfaces
  p_glUseProgramObjectARB(ShadowsFog_Program);
  //ShadowsFog_Locs.storeFogType();

  surface_t **surfptr = RendLev->DrawSurfList.ptr();
  for (int count = RendLev->DrawSurfList.length(); count--; ++surfptr) {
    surface_t *surf = *surfptr;
    if (!surf->Fade) continue;
    if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
    if (surf->count < 3) {
      if (developer) GCon->Logf(NAME_Dev, "trying to render fog surface with %d vertices", surf->count);
      continue;
    }

    // don't render translucent surfaces
    // they should not end up here, but...
    const texinfo_t *currTexinfo = surf->texinfo;
    if (!currTexinfo || !currTexinfo->Tex || currTexinfo->Tex->Type == TEXTYPE_Null) continue;
    if (currTexinfo->Alpha < 1.0f) continue;

    ShadowsFog_Locs.storeFogFade(surf->Fade, 1.0f);

    glBegin(GL_POLYGON);
    for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
    glEnd();
  }
}


//==========================================================================
//
//  VOpenGLDrawer::EndFogPass
//
//==========================================================================
void VOpenGLDrawer::EndFogPass () {
  glDisable(GL_BLEND);
  // back to normal z-buffering
  glDepthMask(GL_TRUE); // allow z-buffer writes
  RestoreDepthFunc();
}
