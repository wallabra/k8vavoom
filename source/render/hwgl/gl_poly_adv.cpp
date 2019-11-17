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


extern VCvarB gl_enable_depth_bounds;
extern VCvarB gl_dbg_advlight_debug;
extern VCvarI gl_dbg_advlight_color;

static VCvarB gl_smart_dirty_rects("gl_smart_dirty_rects", true, "Use dirty rectangles list to check for stencil buffer dirtyness?", CVAR_Archive);
static VCvarB gl_smart_reject_shadow_surfaces("gl_smart_reject_shadow_surfaces", false, "Reject some surfaces that cannot possibly produce shadows?", CVAR_Archive);

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


//==========================================================================
//
//  isDirtyRect
//
//==========================================================================
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


//==========================================================================
//
//  appendDirtyRect
//
//==========================================================================
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
  static inline int compareSurfacesByTexture (const surface_t *sa, const surface_t *sb) {
    if (sa == sb) return 0;
    const texinfo_t *ta = sa->texinfo;
    const texinfo_t *tb = sb->texinfo;
    // put masked textures on bottom (this is useful in fog rendering)
    // no need to do this, masked textures now lives in the separate list
    /*
    if (sa->drawflags&surface_t::DF_MASKED) {
      if (!(sb->drawflags&surface_t::DF_MASKED)) return 1;
    } else if (sb->drawflags&surface_t::DF_MASKED) {
      if (!(sa->drawflags&surface_t::DF_MASKED)) return -1;
    }
    */
    if ((uintptr_t)ta->Tex < (uintptr_t)ta->Tex) return -1;
    if ((uintptr_t)tb->Tex > (uintptr_t)tb->Tex) return 1;
    return ((int)ta->ColorMap)-((int)tb->ColorMap);
  }

  static int drawListItemCmpByTexture (const void *a, const void *b, void *udata) {
    return compareSurfacesByTexture(*(const surface_t **)a, *(const surface_t **)b);
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
  VRenderLevelDrawer::DrawLists &dls = RendLev->GetCurrentDLS();

  // draw horizons
  if (!gl_dbg_wireframe) {
    surface_t **surfptr = dls.DrawHorizonList.ptr();
    for (int count = dls.DrawHorizonList.length(); count--; ++surfptr) {
      surface_t *surf = *surfptr;
      if (!surf->plvisible) continue; // viewer is in back side or on plane
      DoHorizonPolygon(surf);
    }
  }

  // set z-buffer for skies
  if (dls.DrawSkyList.length() && !gl_dbg_wireframe) {
    SurfZBuf.Activate();
    SurfZBuf.UploadChangedUniforms();
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    surface_t **surfptr = dls.DrawSkyList.ptr();
    for (int count = dls.DrawSkyList.length(); count--; ++surfptr) {
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
  if (dls.DrawSurfListSolid.length() != 0 || dls.DrawSurfListMasked.length() != 0) {
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
    // normal
    ShadowsAmbient.Activate();
    VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbient);

    //FIXME!
    if (gl_dbg_wireframe) {
      DrawAutomap.Activate();
      DrawAutomap.UploadChangedUniforms();
      glEnable(GL_BLEND);
      glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

      SelectTexture(1);
      glBindTexture(GL_TEXTURE_2D, 0);
      SelectTexture(0);
      return;
    }

    //ShadowsAmbient.Activate();
    //VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbient);

    // do not sort surfaces by texture here, because
    // textures will be put later, and BSP sorted them by depth for us
    // other passes can skip surface sorting
    if (gl_sort_textures) {
      // do not sort surfaces with solid textures, as we don't care about textures here
      // this gives us front-to-back rendering
      //timsort_r(dls.DrawSurfListSolid.ptr(), dls.DrawSurfListSolid.length(), sizeof(surface_t *), &drawListItemCmpByTexture, nullptr);
      // but sort masked textures, because masked texture configuration can be arbitrary
      timsort_r(dls.DrawSurfListMasked.ptr(), dls.DrawSurfListMasked.length(), sizeof(surface_t *), &drawListItemCmpByTexture, nullptr);
    }

    float prevsflight = -666;
    vuint32 prevlight = 0;
    texinfo_t lastTexinfo;
    lastTexinfo.initLastUsed();

    //bool prevGlowActive[3] = { false, false, false };
    GlowParams gp;

    enum {
      BMAP_INACTIVE = 1u,
      BMAP_ACTIVE   = 2u,
    };

    bool glTextureEnabled = false;
    glDisable(GL_TEXTURE_2D);

    //GCon->Logf(NAME_Debug, "::: solid=%d; masked=%d", dls.DrawSurfListSolid.length(), dls.DrawSurfListMasked.length());

    // solid textures
    if (dls.DrawSurfListSolid.length() != 0) {
      // activate non-brightmap shader
      unsigned char brightmapActiveMask = BMAP_INACTIVE;
      unsigned char prevGlowActiveMask = 0u;
      //ShadowsAmbient.Activate();
      //VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbient);
      lastTexinfo.resetLastUsed();
      for (auto &&surf : dls.DrawSurfListSolid) {
        if (!surf->plvisible) continue; // viewer is in back side or on plane
        if (surf->count < 3) continue;
        if (surf->drawflags&surface_t::DF_MASKED) continue; // later

        // don't render translucent surfaces
        // they should not end up here, but...
        const texinfo_t *currTexinfo = surf->texinfo;
        if (!currTexinfo || currTexinfo->isEmptyTexture()) continue; // just in case
        if (currTexinfo->Alpha < 1.0f || currTexinfo->Additive) continue; // just in case

        // update all dunamic textures here
        UpdateAndUploadSurfaceTexture(surf);

        CalcGlow(gp, surf);

        if (r_brightmaps && currTexinfo->Tex->Brightmap) {
          // texture with brightmap
          //GCon->Logf("WALL BMAP: wall texture is '%s', brightmap is '%s'", *currTexinfo->Tex->Name, *currTexinfo->Tex->Brightmap->Name);
          if (brightmapActiveMask != BMAP_ACTIVE) {
            brightmapActiveMask = BMAP_ACTIVE;
            ShadowsAmbientBrightmap.Activate();
            prevsflight = -666; // force light setup
          }
          if (!glTextureEnabled) { glTextureEnabled = true; glEnable(GL_TEXTURE_2D); }
          SelectTexture(1);
          SetBrightmapTexture(currTexinfo->Tex->Brightmap);
          SelectTexture(0);
          // set normal texture
          SetTexture(currTexinfo->Tex, currTexinfo->ColorMap);
          ShadowsAmbientBrightmap.SetTex(currTexinfo);
          // glow
          if (gp.isActive()) {
            prevGlowActiveMask |= brightmapActiveMask;
            VV_GLDRAWER_ACTIVATE_GLOW(ShadowsAmbientBrightmap, gp);
          } else if (prevGlowActiveMask&brightmapActiveMask) {
            prevGlowActiveMask &= ~brightmapActiveMask;
            VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbientBrightmap);
          }
          lastTexinfo.resetLastUsed();
        } else {
          // normal wall
          //GCon->Logf("SOLID WALL: wall texture is '%s' (%p:%p)", *currTexinfo->Tex->Name, currTexinfo->Tex, currTexinfo->Tex->Brightmap);
          if (brightmapActiveMask != BMAP_INACTIVE) {
            brightmapActiveMask = BMAP_INACTIVE;
            ShadowsAmbient.Activate();
            //VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbient);
            prevsflight = -666; // force light setup
          }
          if (glTextureEnabled) { glTextureEnabled = false; glDisable(GL_TEXTURE_2D); }
          // glow
          if (gp.isActive()) {
            prevGlowActiveMask |= brightmapActiveMask;
            VV_GLDRAWER_ACTIVATE_GLOW(ShadowsAmbient, gp);
          } else if (prevGlowActiveMask&brightmapActiveMask) {
            prevGlowActiveMask &= ~brightmapActiveMask;
            VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbient);
          }
        }

        float lev = getSurfLightLevel(surf);
        if (prevlight != surf->Light || FASI(lev) != FASI(prevsflight)) {
          prevsflight = lev;
          prevlight = surf->Light;
          if (brightmapActiveMask == BMAP_INACTIVE) {
            ShadowsAmbient.SetLight(
              ((prevlight>>16)&255)*lev/255.0f,
              ((prevlight>>8)&255)*lev/255.0f,
              (prevlight&255)*lev/255.0f, 1.0f);
          } else {
            ShadowsAmbientBrightmap.SetLight(
              ((prevlight>>16)&255)*lev/255.0f,
              ((prevlight>>8)&255)*lev/255.0f,
              (prevlight&255)*lev/255.0f, 1.0f);
          }
        }

        if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
        currentActiveShader->UploadChangedUniforms();
        glBegin(GL_TRIANGLE_FAN);
          for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
        glEnd();
        if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);
      }
    }

    // masked textures
    if (dls.DrawSurfListMasked.length() != 0) {
      // activate non-brightmap shader
      unsigned char brightmapActiveMask = BMAP_ACTIVE;
      unsigned char prevGlowActiveMask = 0u;
      ShadowsAmbientBrightmap.Activate();
      VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbientBrightmap);
      lastTexinfo.resetLastUsed();
      if (!glTextureEnabled) { glTextureEnabled = true; glEnable(GL_TEXTURE_2D); }
      for (auto &&surf : dls.DrawSurfListMasked) {
        if (!surf->plvisible) continue; // viewer is in back side or on plane
        if (surf->count < 3) continue;
        if ((surf->drawflags&surface_t::DF_MASKED) == 0) continue; // not here

        // don't render translucent surfaces
        // they should not end up here, but...
        const texinfo_t *currTexinfo = surf->texinfo;
        if (!currTexinfo || currTexinfo->isEmptyTexture()) continue; // just in case
        if (currTexinfo->Alpha < 1.0f || currTexinfo->Additive) continue; // just in case

        // update all dunamic textures here
        UpdateAndUploadSurfaceTexture(surf);

        CalcGlow(gp, surf);

        if (r_brightmaps && currTexinfo->Tex->Brightmap) {
          // texture with brightmap
          //GCon->Logf("WALL BMAP: wall texture is '%s', brightmap is '%s'", *currTexinfo->Tex->Name, *currTexinfo->Tex->Brightmap->Name);
          if (brightmapActiveMask != BMAP_ACTIVE) {
            brightmapActiveMask = BMAP_ACTIVE;
            ShadowsAmbientBrightmap.Activate();
            prevsflight = -666; // force light setup
          }
          SelectTexture(1);
          SetBrightmapTexture(currTexinfo->Tex->Brightmap);
          SelectTexture(0);
          // set normal texture
          SetTexture(currTexinfo->Tex, currTexinfo->ColorMap);
          ShadowsAmbientBrightmap.SetTex(currTexinfo);
          // glow
          if (gp.isActive()) {
            prevGlowActiveMask |= brightmapActiveMask;
            VV_GLDRAWER_ACTIVATE_GLOW(ShadowsAmbientBrightmap, gp);
          } else if (prevGlowActiveMask&brightmapActiveMask) {
            prevGlowActiveMask &= ~brightmapActiveMask;
            VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbientBrightmap);
          }
          lastTexinfo.resetLastUsed();
        } else {
          //GCon->Logf("MASKED WALL: wall texture is '%s'", *currTexinfo->Tex->Name);
          // normal wall
          bool textureChanded = lastTexinfo.needChange(*currTexinfo);
          if (textureChanded) lastTexinfo.updateLastUsed(*currTexinfo);

          if (brightmapActiveMask != BMAP_INACTIVE) {
            brightmapActiveMask = BMAP_INACTIVE;
            ShadowsAmbientMasked.Activate();
            //VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbientMasked);
            prevsflight = -666; // force light setup
            textureChanded = true;
          }

          if (textureChanded) {
            SetTexture(currTexinfo->Tex, currTexinfo->ColorMap);
            ShadowsAmbientMasked.SetTex(currTexinfo);
          }

          // glow
          if (gp.isActive()) {
            prevGlowActiveMask |= brightmapActiveMask;
            VV_GLDRAWER_ACTIVATE_GLOW(ShadowsAmbientMasked, gp);
          } else if (prevGlowActiveMask&brightmapActiveMask) {
            prevGlowActiveMask &= ~brightmapActiveMask;
            VV_GLDRAWER_DEACTIVATE_GLOW(ShadowsAmbientMasked);
          }
        }

        float lev = getSurfLightLevel(surf);
        if (prevlight != surf->Light || FASI(lev) != FASI(prevsflight)) {
          prevsflight = lev;
          prevlight = surf->Light;
          if (brightmapActiveMask == BMAP_INACTIVE) {
            ShadowsAmbientMasked.SetLight(
              ((prevlight>>16)&255)*lev/255.0f,
              ((prevlight>>8)&255)*lev/255.0f,
              (prevlight&255)*lev/255.0f, 1.0f);
          } else {
            ShadowsAmbientBrightmap.SetLight(
              ((prevlight>>16)&255)*lev/255.0f,
              ((prevlight>>8)&255)*lev/255.0f,
              (prevlight&255)*lev/255.0f, 1.0f);
          }
        }

        if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
        currentActiveShader->UploadChangedUniforms();
        glBegin(GL_TRIANGLE_FAN);
          for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
        glEnd();
        if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);
      }
    }

    if (!glTextureEnabled) { glTextureEnabled = true; glEnable(GL_TEXTURE_2D); }
  }

  // restore depth function
  //if (gl_prefill_zbuffer) RestoreDepthFunc();

  SelectTexture(1);
  glBindTexture(GL_TEXTURE_2D, 0);
  SelectTexture(0);
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
      currentActiveShader = nullptr;
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
  SurfShadowVolume.UploadChangedUniforms();

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
    vassert(backsec);

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
    const seg_t *backseg = seg->partner;
    if (!backseg) continue;
    const subsector_t *sub2 = backseg->frontsub;
    if (sub2 == sub) continue;
    // different subsector
    const sector_t *bsec = sub2->sector;
    if (bsec == sector) continue;
    // different sector
    if (!backseg->SphereTouches(LightPos, Radius)) continue;
    // and light sphere touches it, check heights
    if (surf->typeFlags&surface_t::TF_FLOOR) {
      // if current sector floor is lower than the neighbour sector floor,
      // it means that our current floor cannot cast a shadow there
      //if (sector->floor.minz <= bsec->floor.maxz) continue;
      if (bsec->floor.minz == sector->floor.minz &&
          bsec->floor.maxz == sector->floor.maxz)
      {
        continue;
      }
    } else if (surf->typeFlags&surface_t::TF_CEILING) {
      // if current sector ceiling is higher than the neighbour sector ceiling,
      // it means that our current ceiling cannot cast a shadow there
      //if (sector->ceiling.maxz >= bsec->ceiling.minz) continue;
      // this is wrong; see Doom2:MAP02, room with two holes -- shot a fireball inside one hole
      // this is wrong because we have two sectors with the same ceiling height, and then a hole
      // so first sector ceiling is lit, and should block the light, but it is ignored
      if (bsec->ceiling.minz == sector->ceiling.minz &&
          bsec->ceiling.maxz == sector->ceiling.maxz)
      {
        continue;
      }
    } else {
      GCon->Log("oops; non-floor and non-ceiling flat surface");
    }
    /*
    if (FASI(bsec->floor.minz) == FASI(sector->floor.minz) &&
        FASI(bsec->floor.maxz) == FASI(sector->floor.maxz) &&
        FASI(bsec->ceiling.minz) == FASI(sector->ceiling.minz) &&
        FASI(bsec->ceiling.maxz) == FASI(sector->ceiling.maxz))
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


#include "gl_poly_adv_zpass.cpp"


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

  if (spotLight) {
    // reject all surfaces behind a spotlight
    //TODO: build spotlight frustum, and perform a rejection with it
    //      or even better: perform such rejection earilier
    TPlane pl;
    pl.SetPointNormal3D(LightPos, coneDir);
    const TVec *vv = sverts;
    bool splhit = false;
    /*
    for (unsigned f = vcount; f--; ++vv) {
      if (vv->isInSpotlight(LightPos, coneDir, coneAngle)) { splhit = true; break; }
    }
    */
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

  if (usingZPass || gl_dbg_use_zpass) {
    RenderSurfaceShadowVolumeZPassIntr(surf, LightPos, Radius);
  } else {
    // OpenGL renders vertices with zero `w` as infinitely far -- this is exactly what we want
    // just do it in vertex shader

    currentActiveShader->UploadChangedUniforms();
    //currentActiveShader->UploadChangedAttrs();

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

    // render side caps
    glBegin(GL_TRIANGLE_STRIP);
      for (unsigned i = 0; i < vcount; ++i) {
        glVertex(sverts[i]);
        glVertex4(v[i], 0);
      }
      glVertex(sverts[0]);
      glVertex4(v[0], 0);
    glEnd();
  }
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
  currentActiveShader->UploadChangedUniforms();
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
  auto mfbo = GetMainFBO();
  mfbo->blitTo(&ambLightFBO, 0, 0, mfbo->getWidth(), mfbo->getHeight(), 0, 0, ambLightFBO.getWidth(), ambLightFBO.getHeight(), GL_NEAREST);
  mfbo->activate();

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

  if (!gl_dbg_adv_render_surface_textures) return;

  VRenderLevelDrawer::DrawLists &dls = RendLev->GetCurrentDLS();
  if (dls.DrawSurfListSolid.length() == 0 && dls.DrawSurfListMasked.length() == 0) return;

  ShadowsTextureMasked.Activate();
  ShadowsTextureMasked.SetTexture(0);

  ShadowsTexture.Activate();
  ShadowsTexture.SetTexture(0);

  //glDisable(GL_BLEND);

  // sort by textures
  if (gl_sort_textures) {
    // sort surfaces with solid textures, because here we need them sorted
    timsort_r(dls.DrawSurfListSolid.ptr(), dls.DrawSurfListSolid.length(), sizeof(surface_t *), &drawListItemCmpByTexture, nullptr);
  }

  texinfo_t lastTexinfo;
  lastTexinfo.initLastUsed();

  // normal
  if (dls.DrawSurfListSolid.length() != 0) {
    lastTexinfo.resetLastUsed();
    ShadowsTexture.Activate();
    for (auto &&surf : dls.DrawSurfListSolid) {
      if (!surf->plvisible) continue; // viewer is in back side or on plane
      if (surf->count < 3) continue;
      if (surf->drawflags&surface_t::DF_MASKED) continue; // later

      // don't render translucent surfaces
      // they should not end up here, but...
      const texinfo_t *currTexinfo = surf->texinfo;
      if (!currTexinfo || currTexinfo->isEmptyTexture()) continue; // just in case
      if (currTexinfo->Alpha < 1.0f || currTexinfo->Additive) continue; // just in case

      const bool textureChanded = lastTexinfo.needChange(*currTexinfo);
      if (textureChanded) {
        lastTexinfo.updateLastUsed(*currTexinfo);
        SetTexture(currTexinfo->Tex, currTexinfo->ColorMap);
        ShadowsTexture.SetTex(currTexinfo);
      }

      bool doDecals = (currTexinfo->Tex && !currTexinfo->noDecals && surf->seg && surf->seg->decalhead);

      // fill stencil buffer for decals
      if (doDecals) RenderPrepareShaderDecals(surf);

      if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
      //glBegin(GL_POLYGON);
      currentActiveShader->UploadChangedUniforms();
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
          ShadowsTexture.Activate();
          glBlendFunc(GL_DST_COLOR, GL_ZERO);
          //glEnable(GL_BLEND);
          lastTexinfo.resetLastUsed(); // resetup texture
        }
      }
    }
  }

  // masked
  if (dls.DrawSurfListMasked.length() != 0) {
    lastTexinfo.resetLastUsed();
    ShadowsTextureMasked.Activate();
    for (auto &&surf : dls.DrawSurfListMasked) {
      if (!surf->plvisible) continue; // viewer is in back side or on plane
      if (surf->count < 3) continue;
      if ((surf->drawflags&surface_t::DF_MASKED) == 0) continue; // not here

      // don't render translucent surfaces
      // they should not end up here, but...
      const texinfo_t *currTexinfo = surf->texinfo;
      if (!currTexinfo || currTexinfo->isEmptyTexture()) continue; // just in case
      if (currTexinfo->Alpha < 1.0f || currTexinfo->Additive) continue; // just in case

      const bool textureChanded = lastTexinfo.needChange(*currTexinfo);
      if (textureChanded) {
        lastTexinfo.updateLastUsed(*currTexinfo);
        SetTexture(currTexinfo->Tex, currTexinfo->ColorMap);
        ShadowsTextureMasked.SetTex(currTexinfo);
      }

      bool doDecals = (currTexinfo->Tex && !currTexinfo->noDecals && surf->seg && surf->seg->decalhead);

      // fill stencil buffer for decals
      if (doDecals) RenderPrepareShaderDecals(surf);

      if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
      //glBegin(GL_POLYGON);
      currentActiveShader->UploadChangedUniforms();
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
          ShadowsTextureMasked.Activate();
          glBlendFunc(GL_DST_COLOR, GL_ZERO);
          //glEnable(GL_BLEND);
          lastTexinfo.resetLastUsed(); // resetup texture
        }
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

  if (!gl_dbg_adv_render_surface_fog) return;

  VRenderLevelDrawer::DrawLists &dls = RendLev->GetCurrentDLS();
  if (dls.DrawSurfListSolid.length() == 0 && dls.DrawSurfListMasked.length() == 0) return;

  /*
  ShadowsFog.SetTexture(0);
  ShadowsFog.SetFogFade(lastFade, 1.0f);
  */

  texinfo_t lastTexinfo;
  lastTexinfo.initLastUsed();

  // normal
  if (dls.DrawSurfListSolid.length() != 0) {
    lastTexinfo.resetLastUsed();
    ShadowsFog.Activate();
    ShadowsFog.SetFogFade(0, 1.0f);
    vuint32 lastFade = 0;
    glDisable(GL_TEXTURE_2D);
    for (auto &&surf : dls.DrawSurfListSolid) {
      if (!surf->Fade) continue;
      if (!surf->plvisible) continue; // viewer is in back side or on plane
      if (surf->count < 3) continue;
      if (surf->drawflags&surface_t::DF_MASKED) continue; // later

      // don't render translucent surfaces
      // they should not end up here, but...
      const texinfo_t *currTexinfo = surf->texinfo;
      if (!currTexinfo || currTexinfo->isEmptyTexture()) continue; // just in case
      if (currTexinfo->Alpha < 1.0f || currTexinfo->Additive) continue; // just in case

      if (lastFade != surf->Fade) {
        lastFade = surf->Fade;
        ShadowsFog.SetFogFade(surf->Fade, 1.0f);
      }

      if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
      //glBegin(GL_POLYGON);
      currentActiveShader->UploadChangedUniforms();
      glBegin(GL_TRIANGLE_FAN);
        for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
      glEnd();
      if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);
    }
    glEnable(GL_TEXTURE_2D);
  }

  // masked
  if (dls.DrawSurfListMasked.length() != 0) {
    lastTexinfo.resetLastUsed();
    ShadowsFogMasked.Activate();
    ShadowsFogMasked.SetFogFade(0, 1.0f);
    ShadowsFogMasked.SetTexture(0);
    vuint32 lastFade = 0;
    for (auto &&surf : dls.DrawSurfListMasked) {
      if (!surf->Fade) continue;
      if (!surf->plvisible) continue; // viewer is in back side or on plane
      if (surf->count < 3) continue;
      if ((surf->drawflags&surface_t::DF_MASKED) == 0) continue; // not here

      // don't render translucent surfaces
      // they should not end up here, but...
      const texinfo_t *currTexinfo = surf->texinfo;
      if (!currTexinfo || currTexinfo->isEmptyTexture()) continue; // just in case
      if (currTexinfo->Alpha < 1.0f || currTexinfo->Additive) continue; // just in case

      if (lastFade != surf->Fade) {
        lastFade = surf->Fade;
        ShadowsFogMasked.SetFogFade(surf->Fade, 1.0f);
      }

      const bool textureChanded = lastTexinfo.needChange(*currTexinfo);
      if (textureChanded) {
        lastTexinfo.updateLastUsed(*currTexinfo);
        SetTexture(currTexinfo->Tex, currTexinfo->ColorMap);
        ShadowsFogMasked.SetTex(currTexinfo);
      }

      if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
      //glBegin(GL_POLYGON);
      currentActiveShader->UploadChangedUniforms();
      glBegin(GL_TRIANGLE_FAN);
        for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i]);
      glEnd();
      if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);
    }
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
