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


extern VCvarB gl_enable_depth_bounds;
extern VCvarB gl_dbg_advlight_debug;
extern VCvarI gl_dbg_advlight_color;

static VCvarB gl_smart_dirty_rects("gl_smart_dirty_rects", true, "Use dirty rectangles list to check for stencil buffer dirtyness?", CVAR_Archive);
static VCvarB gl_smart_reject_shadow_surfaces("gl_smart_reject_shadow_surfaces", true, "Reject some surfaces that cannot possibly produce shadows?", CVAR_Archive);

static VCvarB gl_smart_reject_shadow_segs("gl_smart_reject_shadow_segs", true, "Reject some surfaces that cannot possibly produce shadows?", CVAR_Archive);
static VCvarB gl_smart_reject_shadow_flats("gl_smart_reject_shadow_flats", true, "Reject some surfaces that cannot possibly produce shadows?", CVAR_Archive);


/* TODO
  clear stencil buffer before first shadow shadow rendered.
  also, check if the given surface really can cast shadow.
  note that solid segs that has no non-solid neighbours cannot cast any shadow.
  also, flat surfaces in subsectors whose neighbours doesn't change height can't cast any shadow.
*/

// ////////////////////////////////////////////////////////////////////////// //
struct DRect {
  int x0, y0;
  int x1, y1; // inclusive
};

static TArray<DRect> dirtyRects;


static bool isDirtyRect (const GLint arect[4]) {
  for (auto &&r : dirtyRects) {
    if (arect[VOpenGLDrawer::SCS_MAXX] < r.x0 || arect[VOpenGLDrawer::SCS_MAXY] < r.y0 ||
        arect[VOpenGLDrawer::SCS_MINX] > r.x1 || arect[VOpenGLDrawer::SCS_MINY] > r.y1)
    {
      continue;
    }
    return true;
  }
  return false;
}


static void appendDirtyRect (const GLint arect[4]) {
  // remove all rects that are inside our new one
  /*
  int ridx = 0;
  while (ridx < dirtyRects.length()) {
    const DRect r = dirtyRects[ridx];
    // if new rect is inside some old one, do nothing
    if (arect[VOpenGLDrawer::SCS_MINX] >= r.x0 && arect[VOpenGLDrawer::SCS_MINY] >= r.y0 &&
        arect[VOpenGLDrawer::SCS_MAXX] <= r.x1 && arect[VOpenGLDrawer::SCS_MAXY] <= r.y1)
    {
      return;
    }
    // if old rect is inside a new one, remove old rect
    if (r.x0 >= arect[VOpenGLDrawer::SCS_MINX] && r.y0 >= arect[VOpenGLDrawer::SCS_MINY] &&
        r.x1 <= arect[VOpenGLDrawer::SCS_MAXX] && r.y1 <= arect[VOpenGLDrawer::SCS_MAXY])
    {
      dirtyRects.removeAt(ridx);
      continue;
    }
    // check next rect
    ++ridx;
  }
  */

  // append new one
  DRect &rc = dirtyRects.alloc();
  rc.x0 = arect[VOpenGLDrawer::SCS_MINX];
  rc.y0 = arect[VOpenGLDrawer::SCS_MINY];
  rc.x1 = arect[VOpenGLDrawer::SCS_MAXX];
  rc.y1 = arect[VOpenGLDrawer::SCS_MAXY];
}


// ////////////////////////////////////////////////////////////////////////// //
extern "C" {
  static inline int compareSurfaces (const surface_t *sa, const surface_t *sb) {
    if (sa == sb) return 0;
    const texinfo_t *ta = sa->texinfo;
    const texinfo_t *tb = sb->texinfo;
    // put steamlined masked textures on bottom (this is useful in fog rendering)
    if (sa->drawflags&surface_t::DF_MASKED) {
      if (!(sb->drawflags&surface_t::DF_MASKED)) return 1;
    } else if (sb->drawflags&surface_t::DF_MASKED) {
      if (!(sa->drawflags&surface_t::DF_MASKED)) return -1;
    }
    if ((uintptr_t)ta->Tex < (uintptr_t)ta->Tex) return -1;
    if ((uintptr_t)tb->Tex > (uintptr_t)tb->Tex) return 1;
    return ((int)ta->ColorMap)-((int)tb->ColorMap);
  }

  static int drawListItemCmp (const void *a, const void *b, void *udata) {
    return compareSurfaces(*(const surface_t **)a, *(const surface_t **)b);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::DrawWorldZBufferPass
//
//  fill GPU z buffer, so we can limit overdraw later
//
//  TODO: walls with masked textures should be rendered with another shader
//
//==========================================================================
void VOpenGLDrawer::DrawWorldZBufferPass () {
  if (!RendLev->DrawSurfList.length()) return;
  SurfZBuf.Activate();
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

  //zfillMasked.reset();
  surface_t **surfptr = RendLev->DrawSurfList.ptr();
  for (int count = RendLev->DrawSurfList.length(); count--; ++surfptr) {
    const surface_t *surf = *surfptr;
    if (surf->count < 3) continue;
    if (surf->drawflags&surface_t::DF_MASKED) continue;

    // don't render translucent surfaces
    // they should not end up here, but...
    const texinfo_t *currTexinfo = surf->texinfo;
    if (!currTexinfo || !currTexinfo->Tex || currTexinfo->Tex->Type == TEXTYPE_Null) continue;
    if (currTexinfo->Alpha < 1.0f || currTexinfo->Additive) continue;

    if (!surf->plvisible) continue; // viewer is in back side or on plane

    /*
    if (surf->drawflags&surface_t::DF_MASKED) {
      zfillMasked.append((surface_t *)surf;
    } else
    */
    {
      if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
      glBegin(GL_TRIANGLE_FAN);
        for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
      glEnd();
      if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);
    }
  }

  // render masked walls
  /*
  if (zfillMasked.length()) {
    const surface_t **surfp = zfillMasked.ptr();
    for (int f = zfillMasked.length(); f--; ++surfp) {
      const surface_t *surf = *surfp;
    }
  }
  */

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
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
  if (!gl_dbg_wireframe) {
    surface_t **surfptr = RendLev->DrawHorizonList.ptr();
    for (int count = RendLev->DrawHorizonList.length(); count--; ++surfptr) {
      surface_t *surf = *surfptr;
      if (!surf->plvisible) continue; // viewer is in back side or on plane
      DoHorizonPolygon(surf);
    }
  }

  // set z-buffer for skies
  if (RendLev->DrawSkyList.length() && !gl_dbg_wireframe) {
    SurfZBuf.Activate();
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    surface_t **surfptr = RendLev->DrawSkyList.ptr();
    for (int count = RendLev->DrawSkyList.length(); count--; ++surfptr) {
      surface_t *surf = *surfptr;
      if (!surf->plvisible) continue; // viewer is in back side or on plane
      if (surf->count < 3) continue;
      //glBegin(GL_POLYGON);
      glBegin(GL_TRIANGLE_FAN);
        for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
      glEnd();
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }

  // draw normal surfaces
  if (RendLev->DrawSurfList.length()) {
    enum {
      SOLID = 0,
      MASKED,
      BRIGHTMAP,
    };
    unsigned currShader = SOLID;

    if (gl_prefill_zbuffer) {
      DrawWorldZBufferPass();
      //glDepthFunc(GL_EQUAL);
    }

    // setup samplers for all shaders
    // masked
    ShadowsAmbientMasked.Activate();
    ShadowsAmbientMasked.SetTexture(0);
    VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbientMasked);
    // brightmap
    ShadowsAmbientBrightmap.Activate();
    ShadowsAmbientBrightmap.SetBrightMapAdditive(r_brightmaps_additive ? 1.0f : 0.0f);
    ShadowsAmbientBrightmap.SetTexture(0);
    ShadowsAmbientBrightmap.SetTextureBM(1);
    VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbientBrightmap);

    if (gl_dbg_wireframe) {
      DrawAutomap.Activate();
      glEnable(GL_BLEND);
      glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    } else {
      ShadowsAmbient.Activate();
      VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbient);
    }

    // do not sort surfaces by texture here, because
    // textures will be put later, and BSP sorted them by depth for us
    // other passes can skip surface sorting
    if (gl_sort_textures) timsort_r(RendLev->DrawSurfList.ptr(), RendLev->DrawSurfList.length(), sizeof(surface_t *), &drawListItemCmp, nullptr);

    float prevsflight = -666;
    vuint32 prevlight = 0;
    const texinfo_t *lastTexinfo = nullptr;
    surface_t **surfptr = RendLev->DrawSurfList.ptr();

    bool prevGlowActive[3] = { false, false, false };
    GlowParams gp;

    for (int count = RendLev->DrawSurfList.length(); count--; ++surfptr) {
      const surface_t *surf = *surfptr;
      if (!surf->plvisible) continue; // viewer is in back side or on plane
      if (surf->count < 3) continue;

      // don't render translucent surfaces
      // they should not end up here, but...
      const texinfo_t *currTexinfo = surf->texinfo;
      if (!currTexinfo || !currTexinfo->Tex || currTexinfo->Tex->Type == TEXTYPE_Null) continue;
      if (currTexinfo->Alpha < 1.0f || currTexinfo->Additive) continue;

      if (!gl_dbg_wireframe) {
        CalcGlow(gp, surf);

        if (r_brightmaps && currTexinfo->Tex->Brightmap) {
          // texture with brightmap
          //GCon->Logf("WALL BMAP: wall texture is '%s', brightmap is '%s'", *currTexinfo->Tex->Name, *currTexinfo->Tex->Brightmap->Name);
          if (currShader != BRIGHTMAP) {
            currShader = BRIGHTMAP;
            ShadowsAmbientBrightmap.Activate();
            prevsflight = -666; // force light setup
          }
          p_glActiveTextureARB(GL_TEXTURE0+1);
          SetBrightmapTexture(currTexinfo->Tex->Brightmap);
          p_glActiveTextureARB(GL_TEXTURE0);
          // set normal texture
          SetTexture(currTexinfo->Tex, currTexinfo->ColorMap);
          ShadowsAmbientBrightmap.SetTex(currTexinfo);
          // glow
          if (gp.isActive()) {
            prevGlowActive[currShader] = true;
            VV_GLDRAWER_ACTIVATE_GLOW(ShadowsAmbientBrightmap, gp);
          } else {
            if (prevGlowActive[currShader]) {
              prevGlowActive[currShader] = false;
              VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbientBrightmap);
            }
          }
          lastTexinfo = nullptr;
        } else if (surf->drawflags&surface_t::DF_MASKED) {
          //GCon->Logf("MASKED WALL: wall texture is '%s'", *currTexinfo->Tex->Name);
          // masked wall
          bool textureChanded =
            !lastTexinfo ||
            lastTexinfo != currTexinfo ||
            lastTexinfo->Tex != currTexinfo->Tex ||
            lastTexinfo->ColorMap != currTexinfo->ColorMap;
          lastTexinfo = currTexinfo;

          if (currShader != MASKED) {
            /*
            if (currShader == BRIGHTMAP) {
              p_glActiveTextureARB(GL_TEXTURE0+1);
              glBindTexture(GL_TEXTURE_2D, 0);
              p_glActiveTextureARB(GL_TEXTURE0);
            }
            */
            currShader = MASKED;
            ShadowsAmbientMasked.Activate();
            VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbientMasked);
            prevsflight = -666; // force light setup
            textureChanded = true;
          }

          if (textureChanded) {
            SetTexture(currTexinfo->Tex, currTexinfo->ColorMap);
            ShadowsAmbientMasked.SetTex(currTexinfo);
          }

          // glow
          if (gp.isActive()) {
            prevGlowActive[currShader] = true;
            VV_GLDRAWER_ACTIVATE_GLOW(ShadowsAmbientMasked, gp);
          } else {
            if (prevGlowActive[currShader]) {
              prevGlowActive[currShader] = false;
              VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbientMasked);
            }
          }
        } else {
          // normal wall
          //GCon->Logf("SOLID WALL: wall texture is '%s' (%p:%p)", *currTexinfo->Tex->Name, currTexinfo->Tex, currTexinfo->Tex->Brightmap);
          if (currShader != SOLID) {
            /*
            if (currShader == BRIGHTMAP) {
              p_glActiveTextureARB(GL_TEXTURE0+1);
              glBindTexture(GL_TEXTURE_2D, 0);
              p_glActiveTextureARB(GL_TEXTURE0);
            }
            */
            currShader = SOLID;
            ShadowsAmbient.Activate();
            VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbient);
            prevsflight = -666; // force light setup
          }

          // glow
          if (gp.isActive()) {
            prevGlowActive[currShader] = true;
            VV_GLDRAWER_ACTIVATE_GLOW(ShadowsAmbient, gp);
          } else {
            if (prevGlowActive[currShader]) {
              prevGlowActive[currShader] = false;
              VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbient);
            }
          }
        }

        float lev = getSurfLightLevel(surf);
        if (prevlight != surf->Light || FASI(lev) != FASI(prevsflight)) {
          prevsflight = lev;
          prevlight = surf->Light;
          switch (currShader) {
            case SOLID:
              ShadowsAmbient.SetLight(
                ((prevlight>>16)&255)*lev/255.0f,
                ((prevlight>>8)&255)*lev/255.0f,
                (prevlight&255)*lev/255.0f, 1.0f);
              break;
            case MASKED:
              ShadowsAmbientMasked.SetLight(
                ((prevlight>>16)&255)*lev/255.0f,
                ((prevlight>>8)&255)*lev/255.0f,
                (prevlight&255)*lev/255.0f, 1.0f);
              break;
            case BRIGHTMAP:
              ShadowsAmbientBrightmap.SetLight(
                ((prevlight>>16)&255)*lev/255.0f,
                ((prevlight>>8)&255)*lev/255.0f,
                (prevlight&255)*lev/255.0f, 1.0f);
              break;
          }
        }
      } else {
        float clr = (float)(count+1)/RendLev->DrawSurfList.length();
        if (clr < 0.1f) clr = 0.1f;
        glColor4f(clr, clr, clr, 1.0f);
      }

      if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
      if (!gl_dbg_wireframe) {
        // normal
        glBegin(GL_TRIANGLE_FAN);
          for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
        glEnd();
      } else {
        // wireframe
        //FIXME: this is wrong for "quality mode", where we'll subdivide surface to more triangles
        glBegin(GL_LINE_LOOP);
          for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
        glEnd();
      }
      if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);
    }
  }

  // restore depth function
  //if (gl_prefill_zbuffer) RestoreDepthFunc();

  p_glActiveTextureARB(GL_TEXTURE0+1);
  glBindTexture(GL_TEXTURE_2D, 0);
  p_glActiveTextureARB(GL_TEXTURE0);
}


//==========================================================================
//
//  VOpenGLDrawer::BeginShadowVolumesPass
//
//  setup general rendering parameters for shadow volume rendering
//
//==========================================================================
void VOpenGLDrawer::BeginShadowVolumesPass () {
  //glEnable(GL_STENCIL_TEST);
  glDisable(GL_STENCIL_TEST);
  glDepthMask(GL_FALSE); // no z-buffer writes
  // reset last known scissor
  glGetIntegerv(GL_VIEWPORT, lastSVVport);
  memcpy(lastSVScissor, lastSVVport, sizeof(lastSVScissor));
  if (gl_smart_dirty_rects) dirtyRects.reset();
}


//==========================================================================
//
//  VOpenGLDrawer::BeginLightShadowVolumes
//
//  setup rendering parameters for shadow volume rendering
//
//==========================================================================
void VOpenGLDrawer::BeginLightShadowVolumes (const TVec &LightPos, const float Radius, bool useZPass, bool hasScissor, const int scoords[4], const TVec &aconeDir, const float aconeAngle) {
  wasRenderedShadowSurface = false;
  if (gl_dbg_wireframe) return;
  //GCon->Logf("*** VOpenGLDrawer::BeginLightShadowVolumes(): stencil_dirty=%d", (int)IsStencilBufferDirty());
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
      if (!IsStencilBufferDirty()) {
        // check if current scissor rect is not inside the previous one
        // if it is not inside, we still have to clear stencil buffer
        if (currentSVScissor[SCS_MINX] < lastSVScissor[SCS_MINX] ||
            currentSVScissor[SCS_MINY] < lastSVScissor[SCS_MINY] ||
            currentSVScissor[SCS_MAXX] > lastSVScissor[SCS_MAXX] ||
            currentSVScissor[SCS_MAXY] > lastSVScissor[SCS_MAXY])
        {
          //GCon->Log("*** VOpenGLDrawer::BeginLightShadowVolumes(): force scissor clrear");
          if (gl_smart_dirty_rects) {
            if (isDirtyRect(currentSVScissor)) {
              //GCon->Log("*** VOpenGLDrawer::BeginLightShadowVolumes(): force scissor clrear");
              NoteStencilBufferDirty();
            }
          } else {
            NoteStencilBufferDirty();
          }
        }
      }
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
  glEnable(GL_STENCIL_TEST);

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

  coneDir = aconeDir;
  coneAngle = (aconeAngle <= 0.0f || aconeAngle >= 360.0f ? 0.0f : aconeAngle);

  if (coneAngle && aconeDir.isValid() && !aconeDir.isZero()) {
    spotLight = true;
    coneDir.normaliseInPlace();
  } else {
    spotLight = false;
  }
  SurfShadowVolume.Activate();
  SurfShadowVolume.SetLightPos(LightPos);

  // remember current scissor rect
  memcpy(lastSVScissor, currentSVScissor, sizeof(lastSVScissor));
}


//==========================================================================
//
//  VOpenGLDrawer::EndLightShadowVolumes
//
//==========================================================================
void VOpenGLDrawer::EndLightShadowVolumes () {
  //GCon->Logf("*** VOpenGLDrawer::EndLightShadowVolumes(): stencil_dirty=%d", (int)IsStencilBufferDirty());
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
//  R_GlobalPointToLocal
//
//==========================================================================
static __attribute__((unused)) inline TVec R_GlobalPointToLocal (const VMatrix4 &modelMatrix, const TVec &v) {
  TVec tmp = v-modelMatrix.GetCol(3);
  return modelMatrix.RotateVector(tmp);
}


//==========================================================================
//
//  R_LightProjectionMatrix
//
//==========================================================================
static __attribute__((unused)) void R_LightProjectionMatrix (VMatrix4 &mat, const TVec &origin, const TPlane &rearPlane) {
  float lv[4]; // TVec4

  // calculate the homogenious light vector
  lv[0] = origin.x;
  lv[1] = origin.y;
  lv[2] = origin.z;
  lv[3] = 1.0f;

  const TVec norm = rearPlane.normal;
  const float dist = -rearPlane.dist;

  //lg = rearPlane.ToVec4() * lv;
  const float lg = norm.x*lv[0]+norm.y*lv[1]+norm.z*lv[2]+dist*lv[3];

  // outer product
  mat.m[0][0] = lg-norm.x*lv[0];
  mat.m[1][0] = -norm.y*lv[0];
  mat.m[2][0] = -norm.z*lv[0];
  mat.m[3][0] = -dist*lv[0];

  mat.m[0][1] = -norm.x*lv[1];
  mat.m[1][1] = lg-norm.y*lv[1];
  mat.m[2][1] = -norm.z*lv[1];
  mat.m[3][1] = -dist*lv[1];

  mat.m[0][2] = -norm.x*lv[2];
  mat.m[1][2] = -norm.y*lv[2];
  mat.m[2][2] = lg-norm.z*lv[2];
  mat.m[3][2] = -dist*lv[2];

  mat.m[0][3] = -norm.x*lv[3];
  mat.m[1][3] = -norm.y*lv[3];
  mat.m[2][3] = -norm.z*lv[3];
  mat.m[3][3] = lg-dist*lv[3];
}


//==========================================================================
//
//  R_ProjectPointsToPlane
//
//==========================================================================
static __attribute__((unused)) void R_ProjectPointsToPlane (TVec *dest, const TVec *src, unsigned vcount,
                                    VMatrix4 &mmat, const TVec &LightPos, const TPlane &destPlane)
{
  VMatrix4 mat;
  TVec lv = R_GlobalPointToLocal(mmat, LightPos);
  R_LightProjectionMatrix(mat, lv, destPlane);
  for (unsigned f = 0; f < vcount; ++f, ++dest, ++src) {
    const float w = VSUM4(mat.m[0][3]*src->x, mat.m[1][3]*src->y, mat.m[2][3]*src->z, mat.m[3][3]);
    if (w == 0.0f) {
      *dest = *src;
    } else {
      const float oow = 1.0f/w;
      *dest = mat*(*src);
      *dest *= oow;
    }
  }
}


//==========================================================================
//
//  CanSurfaceSegCastShadow
//
//==========================================================================
static bool CanSurfaceSegCastShadow (const surface_t *surf, const TVec LightPos, float Radius) {
  if (!gl_smart_reject_shadow_segs) return true;

  // solid segs that has no non-solid neighbours cannot cast any shadow
  const seg_t *seg = surf->seg;
  const line_t *ldef = seg->linedef;
  if (!ldef) {
    // miniseg; wutafuck? it should not have any surface!
    GCon->Log(NAME_Error, "miniseg should not have any surfaces!");
    return true;
  }

  // we cannot do anything sane for 3D floors
  const subsector_t *sub = surf->subsector;
  if (!sub) return true;

  const sector_t *sector = sub->sector;
  if (sector->SectorFlags&sector_t::SF_ExtrafloorSource) return true; // sadly, i cannot reject 3D floors yet

  // if this is a two-sided line, don't reject it
  if (ldef->flags&ML_TWOSIDED) {
    /*
    if (!seg->partner) return false; // just in case
    const sector_t *backsec = seg->partner->frontsub->sector;
    check(backsec);

    // here we can check if this is top/bottom texture, and if it can cast shadow
    // to check this, see if light can touch surface edge, and consider this seg one-sided, if it isn't

    // calculate coordinates of bottom texture (if any)
    if (surf->typeFlags&surface_t::TF_BOTTOM) {
      // just in case: if back sector floor should be higher that than our floor
      float minz = sector->floor.minz;
      float maxz = backsec->floor.maxz;
      if (maxz <= minz) return false; // bottom texture shouldn't be visible anyway
      GCon->Logf("*** BOTTOM CHECK! minz=%g; maxz=%g", minz, maxz);
      GCon->Logf("   lz=%g; llow=%g; lhigh=%g", LightPos.z, LightPos.z-Radius, LightPos.z+Radius);
      // if light is fully inside or outside, this seg cannot cast shadow
      // fully outside?
      if (LightPos.z+Radius <= minz || LightPos.z-Radius >= maxz) return false;
      // fully inside?
      if (LightPos.z+Radius > maxz) {
        return true;
      } else {
        GCon->Logf("*** BOTTOM REJECT!");
      }
    } else {
      return true;
    }
    */
    return true;
  }

  // if this is not a two-sided line, only first and last segs can cast shadows
  //!!!if ((int)(ptrdiff_t)(ldef-GLevel->Lines) == 42) GCon->Log("********* 42 ************");
  if (*seg->v1 != *ldef->v1 && *seg->v2 != *ldef->v2 &&
      *seg->v2 != *ldef->v1 && *seg->v1 != *ldef->v2)
  {
    //!!!GCon->Log("*** skipped useless shadow segment (0)");
    return true;
  }

  // if all neighbour lines are one-sided, and doesn't make a sharp turn, this seg cannot cast a shadow

  // check v1
  const line_t *const *lnx = ldef->v1lines;
  for (int cc = ldef->v1linesCount; cc--; ++lnx) {
    const line_t *l2 = *lnx;
    if (!l2->SphereTouches(LightPos, Radius)) continue;
    if (l2->flags&ML_TWOSIDED) return true;
    if (PlaneAngles2D(ldef, l2) <= 180.0f && PlaneAngles2DFlipTo(ldef, l2) <= 180.0f) {
      //!!!GCon->Logf("::: %d vs %d: %g : %g", (int)(ptrdiff_t)(ldef-GLevel->Lines), (int)(ptrdiff_t)(l2-GLevel->Lines), PlaneAngles2D(ldef, l2), PlaneAngles2DFlipTo(ldef, l2));
      continue;
    } else {
      //!!!GCon->Logf("::: %d vs %d: %g : %g", (int)(ptrdiff_t)(ldef-GLevel->Lines), (int)(ptrdiff_t)(l2-GLevel->Lines), PlaneAngles2D(ldef, l2), PlaneAngles2DFlipTo(ldef, l2));
    }
    return true;
  }

  // check v2
  lnx = ldef->v2lines;
  for (int cc = ldef->v2linesCount; cc--; ++lnx) {
    const line_t *l2 = *lnx;
    if (!l2->SphereTouches(LightPos, Radius)) continue;
    if (l2->flags&ML_TWOSIDED) return true;
    if (PlaneAngles2D(ldef, l2) <= 180.0f && PlaneAngles2DFlipTo(ldef, l2) <= 180.0f) {
      //!!!GCon->Logf("::: %d vs %d: %g : %g", (int)(ptrdiff_t)(ldef-GLevel->Lines), (int)(ptrdiff_t)(l2-GLevel->Lines), PlaneAngles2D(ldef, l2), PlaneAngles2DFlipTo(ldef, l2));
      continue;
    } else {
      //!!!GCon->Logf("::: %d vs %d: %g : %g", (int)(ptrdiff_t)(ldef-GLevel->Lines), (int)(ptrdiff_t)(l2-GLevel->Lines), PlaneAngles2D(ldef, l2), PlaneAngles2DFlipTo(ldef, l2));
    }
    return true;
  }

  //!!!GCon->Log("*** skipped useless shadow segment (1)");
  // done, it passed all checks, and cannot cast shadow (i hope)
  return false;
}


//==========================================================================
//
//  CanSurfaceFlatCastShadow
//
//==========================================================================
static bool CanSurfaceFlatCastShadow (const surface_t *surf, const TVec LightPos, float Radius) {
  if (!gl_smart_reject_shadow_flats) return true;

  // flat surfaces in subsectors whose neighbours doesn't change height can't cast any shadow
  const subsector_t *sub = surf->subsector;
  if (sub->numlines == 0) return true; // just in case

  const sector_t *sector = sub->sector;
  // sadly, we cannot optimise for sectors with 3D (extra) floors
  if (sector->SectorFlags&sector_t::SF_ExtrafloorSource) return true; // sadly, i cannot reject 3D floors yet

  // do we have any 3D floors in this sector?
  if (sector->SectorFlags&sector_t::SF_HasExtrafloors) {
    // check if we're doing top ceiling, or bottom floor
    // (this should always be the case, but...)
    if (surf->plane.normal == sector->floor.normal) {
      if (surf->plane.dist != sector->floor.dist) return true;
    } else if (surf->plane.normal == sector->ceiling.normal) {
      if (surf->plane.dist != sector->ceiling.dist) return true;
    } else {
      return true;
    }
  }

  const seg_t *seg = sub->firstseg;
  for (int cnt = sub->numlines; cnt--; ++seg) {
    const seg_t *s2 = seg->partner;
    if (!s2) continue;
    const subsector_t *sub2 = s2->frontsub;
    if (sub2 == sub) continue;
    // different subsector
    const sector_t *sec2 = sub2->sector;
    if (sec2 == sector) continue;
    // different sector
    if (!s2->SphereTouches(LightPos, Radius)) continue;
    // and light sphere touches it, check heights
    if (surf->typeFlags&surface_t::TF_FLOOR) {
      // if current sector floor is lower than the neighbour sector floor,
      // it means that our current floor cannot cast a shadow there
      if (sector->floor.minz <= sec2->floor.maxz) continue;
    } else if (surf->typeFlags&surface_t::TF_CEILING) {
      // if current sector ceiling is higher than the neighbour sector ceiling,
      // it means that our current ceiling cannot cast a shadow there
      if (sector->ceiling.maxz >= sec2->ceiling.minz) continue;
    } else {
      GCon->Log("oops; non-floor and non-ceiling flat surface");
    }
    /*
    if (FASI(sec2->floor.minz) == FASI(sector->floor.minz) &&
        FASI(sec2->floor.maxz) == FASI(sector->floor.maxz) &&
        FASI(sec2->ceiling.minz) == FASI(sector->ceiling.minz) &&
        FASI(sec2->ceiling.maxz) == FASI(sector->ceiling.maxz))
    {
      continue;
    }
    */
    return true;
  }

  // done, it passed all checks, and cannot cast shadow (i hope)
  return false;
}


//==========================================================================
//
//  CanSurfaceCastShadow
//
//==========================================================================
static bool CanSurfaceCastShadow (const surface_t *surf, const TVec &LightPos, float Radius) {
  if (surf->seg) {
    return CanSurfaceSegCastShadow(surf, LightPos, Radius);
  } else if (surf->subsector) {
    return CanSurfaceFlatCastShadow(surf, LightPos, Radius);
  }
  // just in case
  return true;
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
//  FIXME: gozzo 3d-shit extra should be rendered in both directions?
//
//==========================================================================
void VOpenGLDrawer::RenderSurfaceShadowVolume (const surface_t *surf, const TVec &LightPos, float Radius) {
  if (gl_dbg_wireframe) return;
  if (surf->count < 3) return; // just in case

  if (gl_smart_reject_shadow_surfaces && !CanSurfaceCastShadow(surf, LightPos, Radius)) return;

  const unsigned vcount = (unsigned)surf->count;
  const TVec *sverts = surf->verts;
  const TVec *v = sverts;

  /*
  if (spotLight) {
    const TVec *vv = sverts;
    bool splhit = false;
    for (unsigned f = vcount; f--; ++vv) {
      if (vv->isInSpotlight(LightPos, coneDir, coneAngle)) { splhit = true; break; }
    }
    if (!splhit) return;
  }
  */
  if (spotLight) {
    TPlane pl;
    pl.SetPointNormal3D(LightPos, coneDir);
    const TVec *vv = sverts;
    bool splhit = false;
    for (unsigned f = vcount; f--; ++vv) {
      if (!pl.PointOnSide(*vv)) { splhit = true; break; }
    }
    if (!splhit) return;
  }

  //GCon->Logf("***   VOpenGLDrawer::RenderSurfaceShadowVolume()");
  if (!wasRenderedShadowSurface && gl_smart_dirty_rects) {
    appendDirtyRect(currentSVScissor);
  }
  wasRenderedShadowSurface = true;
  NoteStencilBufferDirty();


  // OpenGL renders vertices with zero `w` as infinitely far -- this is exactly what we want
  // just do it in vertex shader

  if (!usingZPass && !gl_dbg_use_zpass) {
    // render far cap
    //glBegin(GL_POLYGON);
    glBegin(GL_TRIANGLE_FAN);
      for (unsigned i = vcount; i--; ) glVertex4(v[i], 0);
    glEnd();

    // render near cap
    //glBegin(GL_POLYGON);
    glBegin(GL_TRIANGLE_FAN);
      for (unsigned i = 0; i < vcount; ++i) glVertex(sverts[i]);
    glEnd();
  } else {
    static TVec *dest = nullptr;
    static unsigned destSize = 0;
    if (destSize < vcount+1) {
      destSize = (vcount|0x7f)+1;
      dest = (TVec *)Z_Realloc(dest, destSize*sizeof(TVec));
    }

    // zpass, project to near clip plane
    VMatrix4 pmat, mmat;
    glGetFloatv(GL_PROJECTION_MATRIX, pmat[0]);
    glGetFloatv(GL_MODELVIEW_MATRIX, mmat[0]);

    VMatrix4 comb;
    comb.ModelProjectCombine(mmat, pmat);
    TPlane znear;
    comb.ExtractFrustumNear(znear);

    bool doProject = false;
    const float ldist = znear.Distance(LightPos); // from light to znear
    if (ldist <= 0.0f) {
      // on the back, project only if some surface vertices are on the back too
      for (unsigned f = 0; f < vcount; ++f) {
        const float sdist = znear.Distance(sverts[f]);
        if (sdist >= ldist && sdist <= 0.0f) { doProject = true; break; }
      }
    } else {
      // before camera, project only if some surface vertices are nearer (but not on the back)
      for (unsigned f = 0; f < vcount; ++f) {
        const float sdist = znear.Distance(sverts[f]);
        if (sdist <= ldist && sdist >= 0.0f) { doProject = true; break; }
      }
    }
    doProject = true;

    if (doProject) {
      //if (hasBoundsTest && gl_enable_depth_bounds) glDisable(GL_DEPTH_BOUNDS_TEST_EXT);

#if 0
      R_ProjectPointsToPlane(dest, sverts, vcount, mmat, LightPos, znear);

      p_glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);
      p_glStencilOpSeparate(GL_BACK,  GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);

      // render near cap (it should be front-facing, so it will do "teh right thing")
      //glBegin(GL_POLYGON);
      glBegin(GL_TRIANGLE_FAN);
        for (unsigned f = 0; f < vcount; ++f) glVertex(dest[f]);
      glEnd();

      p_glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);
      p_glStencilOpSeparate(GL_BACK,  GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);

      //glBegin(GL_POLYGON);
      glBegin(GL_TRIANGLE_FAN);
        for (unsigned f = 0; f < vcount; ++f) glVertex(sverts[f]);
      glEnd();
#else
      // it is always "front"
      p_glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);
      p_glStencilOpSeparate(GL_BACK,  GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);

      // render near cap (it should be front-facing, so it will do "teh right thing")
      //glBegin(GL_POLYGON);
      glBegin(GL_TRIANGLE_FAN);
        for (unsigned f = 0; f < vcount; ++f) {
          //glVertex(dest[f]);
          TVec vv = (sverts[f]-LightPos).normalised();
          vv *= 32767.0f; // kind of infinity
          vv += LightPos;
          glVertex(vv);
        }
      glEnd();

      p_glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);
      p_glStencilOpSeparate(GL_BACK,  GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);

      //glBegin(GL_POLYGON);
      glBegin(GL_TRIANGLE_FAN);
        for (unsigned f = 0; f < vcount; ++f) {
          glVertex(sverts[f]);
        }
      glEnd();
#endif

      //if (hasBoundsTest && gl_enable_depth_bounds) glEnable(GL_DEPTH_BOUNDS_TEST_EXT);

      p_glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);
      p_glStencilOpSeparate(GL_BACK,  GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);
    }
  }
#if 0
  else if (!usingZPass) {
    // zp+ implementation
    // this is "forced" zpass, where we may need a projected near cap
    // let's do it: translate, and rotate if the light is in positive half-space of znear
    VMatrix4 pmat, mmat;
    glGetFloatv(GL_PROJECTION_MATRIX, pmat[0]);
    glGetFloatv(GL_MODELVIEW_MATRIX, mmat[0]);
    // create new model->world matrix
    float alpha;
    VMatrix4 trans = VMatrix4::Translate(-LightPos);
    VMatrix4 newmmat = trans*mmat;
    //GCon->Logf("=== %u", view_frustum.planes[TFrustum::Near].clipflag);
    if (view_frustum.planes[TFrustum::Near].PointOnSide(LightPos)) {
      newmmat = mmat*VMatrix4::RotateY(180.0f);
      alpha = -1.0f;
    } else {
      alpha = 1.0f;
    }
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(newmmat[0]);
    // create new projection matrix
    VMatrix4 newpmat = pmat;

    //GCon->Logf("alpha=%f", alpha);

    newpmat[0][0] *= alpha;
    /*
    newpmat.SetIdentity();
    newpmat[0][0] = alpha/rd->fovx;
    newpmat[1][1] = 1.0f/rd->fovy;
    newpmat[2][3] = -1.0f;
    newpmat[3][3] = 0.0f;
    if (RendLev && RendLev->NeedsInfiniteFarClip && !HaveDepthClamp) {
      newpmat[2][2] = -1.0f;
      newpmat[3][2] = -2.0f;
    } else {
      newpmat[2][2] = -(gl_maxdist+1.0f)/(gl_maxdist-1.0f);
      newpmat[3][2] = -2.0f*gl_maxdist/(gl_maxdist-1.0f);
    }
    */

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(newpmat[0]);

    if (hasBoundsTest && gl_enable_depth_bounds) glDisable(GL_DEPTH_BOUNDS_TEST_EXT);

    // render near cap (it should be front-facing, so it will do "teh right thing")
    //glBegin(GL_POLYGON);
    glBegin(GL_TRIANGLE_FAN);
      for (unsigned i = 0; i < vcount; ++i) glVertex(sverts[i]);
    glEnd();
    {
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
      p_glUseProgramObjectARB(DrawAutomap_Program);
      //glEnable(GL_LINE_SMOOTH);
      //glEnable(GL_BLEND);
      glColor3f(1.0f, 1.0f, 0.0f);
      glBegin(GL_LINES);
        for (unsigned i = 0; i < vcount; ++i) {
          glVertex(sverts[i]);
          glVertex(sverts[(i+1)%vcount]);
        }
      glEnd();
      glColor3f(1.0f, 1.0f, 1.0f);
      p_glUseProgramObjectARB(SurfShadowVolume_Program);
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    }

    if (hasBoundsTest && gl_enable_depth_bounds) glEnable(GL_DEPTH_BOUNDS_TEST_EXT);

    // restore matrices
    glLoadMatrixf(pmat[0]);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(mmat[0]);
  }
#endif

  glBegin(GL_TRIANGLE_STRIP);
    for (unsigned i = 0; i < vcount; ++i) {
      glVertex(sverts[i]);
      glVertex4(v[i], 0);
    }
    glVertex(sverts[0]);
    glVertex4(v[0], 0);
  glEnd();
}

#define SETUP_LIGHT_SHADER(shad_)  do { \
  (shad_).Activate(); \
  (shad_).SetLightPos(LightPos); \
  (shad_).SetLightRadius(Radius); \
  (shad_).SetViewOrigin(vieworg.x, vieworg.y, vieworg.z); \
  (shad_).SetTexture(0); \
  if (!gl_dbg_advlight_debug) { \
    (shad_).SetLightMin(LightMin); \
  } else { \
    Color = gl_dbg_advlight_color; \
  } \
  (shad_).SetLightColor(((Color>>16)&255)/255.0f, ((Color>>8)&255)/255.0f, (Color&255)/255.0f); \
} while (0)


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

  /*
  if (doShadow && !wasRenderedShadowSurface) {
    Color = 0xffff0000u;
  }
  */

  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glEnable(GL_BLEND);
  //glBlendFunc(GL_SRC_COLOR, GL_DST_COLOR);
  //glBlendEquation(GL_MAX_EXT);

  glDepthFunc(GL_EQUAL);

  if (spotLight) {
    if (!gl_dbg_advlight_debug) {
      SETUP_LIGHT_SHADER(ShadowsLightSpot);
      ShadowsLightSpot.SetConeDirection(coneDir);
      ShadowsLightSpot.SetConeAngle(coneAngle);
    } else {
      SETUP_LIGHT_SHADER(ShadowsLightSpotDbg);
      ShadowsLightSpotDbg.SetConeDirection(coneDir);
      ShadowsLightSpotDbg.SetConeAngle(coneAngle);
    }
  } else {
    if (!gl_dbg_advlight_debug) {
      SETUP_LIGHT_SHADER(ShadowsLight);
    } else {
      SETUP_LIGHT_SHADER(ShadowsLightDbg);
    }
  }
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
  if (gl_dbg_wireframe) return;
  if (!surf->plvisible) return; // viewer is in back side or on plane
  if (surf->count < 3) return;

  const texinfo_t *tex = surf->texinfo;
  SetTexture(tex->Tex, tex->ColorMap);

  if (spotLight) {
    if (!gl_dbg_advlight_debug) {
      ShadowsLightSpot.SetTex(tex);
      ShadowsLightSpot.SetSurfNormal(surf->GetNormal());
      ShadowsLightSpot.SetSurfDist(surf->GetDist());
    } else {
      ShadowsLightSpotDbg.SetTex(tex);
      ShadowsLightSpotDbg.SetSurfNormal(surf->GetNormal());
      ShadowsLightSpotDbg.SetSurfDist(surf->GetDist());
    }
  } else {
    if (!gl_dbg_advlight_debug) {
      ShadowsLight.SetTex(tex);
      ShadowsLight.SetSurfNormal(surf->GetNormal());
      ShadowsLight.SetSurfDist(surf->GetDist());
    } else {
      ShadowsLightDbg.SetTex(tex);
      ShadowsLightDbg.SetSurfNormal(surf->GetNormal());
      ShadowsLightDbg.SetSurfDist(surf->GetDist());
    }
  }

  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
  //glBegin(GL_POLYGON);
  glBegin(GL_TRIANGLE_FAN);
    for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
  glEnd();
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);
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
  if (gl_dbg_wireframe) return;
  // stop stenciling now
  glDisable(GL_STENCIL_TEST);
  glDepthMask(GL_FALSE); // no z-buffer writes
  glEnable(GL_TEXTURE_2D);
  //glBlendEquation(GL_FUNC_ADD);

  // copy ambient light texture to FBO, so we can use it to light decals
  mainFBO.blitTo(&ambLightFBO, 0, 0, mainFBO.getWidth(), mainFBO.getHeight(), 0, 0, ambLightFBO.getWidth(), ambLightFBO.getHeight(), GL_NEAREST);
  mainFBO.activate();

  glDepthMask(GL_FALSE); // no z-buffer writes
  glEnable(GL_TEXTURE_2D);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_SCISSOR_TEST);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDisable(GL_POLYGON_OFFSET_FILL);
  glEnable(GL_CULL_FACE);
  RestoreDepthFunc();


  glBlendFunc(GL_DST_COLOR, GL_ZERO);
  glEnable(GL_BLEND);

  if (!gl_dbg_adv_render_textures_surface || RendLev->DrawSurfList.length() == 0) return;

  bool lastWasMasked = false;

  ShadowsTextureMasked.Activate();
  ShadowsTextureMasked.SetTexture(0);

  ShadowsTexture.Activate();
  ShadowsTexture.SetTexture(0);

  //glDisable(GL_BLEND);

  // no need to sort surfaces there, it is already done in ambient pass
  const texinfo_t *lastTexinfo = nullptr;
  surface_t **surfptr = RendLev->DrawSurfList.ptr();
  for (int count = RendLev->DrawSurfList.length(); count--; ++surfptr) {
    surface_t *surf = *surfptr;
    if (!surf->plvisible) continue; // viewer is in back side or on plane
    if (surf->count < 3) continue;

    // don't render translucent surfaces
    // they should not end up here, but...
    const texinfo_t *currTexinfo = surf->texinfo;
    if (!currTexinfo || !currTexinfo->Tex || currTexinfo->Tex->Type == TEXTYPE_Null) continue;
    if (currTexinfo->Alpha < 1.0f || currTexinfo->Additive) continue;

    bool textureChanded =
      !lastTexinfo ||
      lastTexinfo != currTexinfo ||
      lastTexinfo->Tex != currTexinfo->Tex ||
      lastTexinfo->ColorMap != currTexinfo->ColorMap;
    lastTexinfo = currTexinfo;

    if (surf->drawflags&surface_t::DF_MASKED) {
      // masked wall
      if (!lastWasMasked) {
        // switch shader
        ShadowsTextureMasked.Activate();
        lastWasMasked = true;
        textureChanded = true; //FIXME: hold two of those
      }
    } else {
      // normal wall
      if (lastWasMasked) {
        // switch shader
        ShadowsTexture.Activate();
        lastWasMasked = false;
        textureChanded = true; //FIXME: hold two of those
      }
    }

    if (textureChanded) {
      SetTexture(currTexinfo->Tex, currTexinfo->ColorMap);
      if (lastWasMasked) ShadowsTextureMasked.SetTex(currTexinfo); else ShadowsTexture.SetTex(currTexinfo);
    }

    bool doDecals = (currTexinfo->Tex && !currTexinfo->noDecals && surf->seg && surf->seg->decals);

    // fill stencil buffer for decals
    if (doDecals) RenderPrepareShaderDecals(surf);

    if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
    //glBegin(GL_POLYGON);
    glBegin(GL_TRIANGLE_FAN);
      for (unsigned i = 0; i < (unsigned)surf->count; ++i) {
        /*
        p_glVertexAttrib2fARB(ShadowsTexture_TexCoordLoc,
          (DotProduct(surf->verts[i], currTexinfo->saxis)+currTexinfo->soffs)*tex_iw,
          (DotProduct(surf->verts[i], currTexinfo->taxis)+currTexinfo->toffs)*tex_ih);
        */
        glVertex(surf->verts[i]);
      }
    glEnd();
    if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);

    if (doDecals) {
      if (RenderFinishShaderDecals(DT_ADVANCED, surf, nullptr, currTexinfo->ColorMap)) {
        if (lastWasMasked) ShadowsTextureMasked.Activate(); else ShadowsTexture.Activate();
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
  if (gl_dbg_wireframe) return;
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // fog is not premultiplied
  glDepthMask(GL_FALSE); // no z-buffer writes

  // draw surfaces
  //ShadowsFog.Activate();
  //ShadowsFog.SetFogType();

  if (RendLev->DrawSurfList.length() == 0) return;

  enum {
    ShaderNone = -1,
    ShaderSolid = 0,
    ShaderMasked = 1,
    ShaderMax,
  };

  int activated = ShaderNone;
  bool fadeSet[ShaderMax] = { false, false };
  vuint32 lastFade[ShaderMax] = { 0, 0 }; //RendLev->DrawSurfList[0]->Fade;
  bool firstMasked = true;

  /*
  ShadowsFog.SetTexture(0);
  ShadowsFog.SetFogFade(lastFade, 1.0f);
  */

  surface_t **surfptr = RendLev->DrawSurfList.ptr();
  const texinfo_t *lastTexinfo = nullptr;
  for (int count = RendLev->DrawSurfList.length(); count--; ++surfptr) {
    surface_t *surf = *surfptr;
    if (!surf->Fade) continue;
    if (!surf->plvisible) continue; // viewer is in back side or on plane
    if (surf->count < 3) continue;

    // don't render translucent surfaces
    // they should not end up here, but...
    const texinfo_t *currTexinfo = surf->texinfo;
    if (!currTexinfo || !currTexinfo->Tex || currTexinfo->Tex->Type == TEXTYPE_Null) continue;
    if (currTexinfo->Alpha < 1.0f || currTexinfo->Additive) continue;

    if (surf->drawflags&surface_t::DF_MASKED) {
      if (activated != ShaderMasked) {
        activated = ShaderMasked;
        ShadowsFogMasked.Activate();
        if (firstMasked) {
          ShadowsFogMasked.SetTexture(0);
          firstMasked = false;
        }
      }
      if (!fadeSet[activated] || lastFade[activated] != surf->Fade) {
        fadeSet[activated] = true;
        lastFade[activated] = surf->Fade;
        ShadowsFogMasked.SetFogFade(surf->Fade, 1.0f);
      }

      bool textureChanded =
        !lastTexinfo ||
        lastTexinfo != currTexinfo ||
        lastTexinfo->Tex != currTexinfo->Tex ||
        lastTexinfo->ColorMap != currTexinfo->ColorMap;
      lastTexinfo = currTexinfo;

      if (textureChanded) {
        SetTexture(currTexinfo->Tex, currTexinfo->ColorMap);
        ShadowsFogMasked.SetTex(currTexinfo);
      }
    } else {
      if (activated != ShaderSolid) {
        activated = ShaderSolid;
        ShadowsFog.Activate();
      }
      if (!fadeSet[activated] || lastFade[activated] != surf->Fade) {
        fadeSet[activated] = true;
        lastFade[activated] = surf->Fade;
        ShadowsFog.SetFogFade(surf->Fade, 1.0f);
      }
    }

    if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
    //glBegin(GL_POLYGON);
    glBegin(GL_TRIANGLE_FAN);
      for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
    glEnd();
    if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);
  }

  //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // for premultiplied
}


//==========================================================================
//
//  VOpenGLDrawer::EndFogPass
//
//==========================================================================
void VOpenGLDrawer::EndFogPass () {
  //glDisable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // for premultiplied
  // back to normal z-buffering
  glDepthMask(GL_TRUE); // allow z-buffer writes
  RestoreDepthFunc();
}
