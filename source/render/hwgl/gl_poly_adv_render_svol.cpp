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


static VCvarB gl_smart_dirty_rects("gl_smart_dirty_rects", true, "Use dirty rectangles list to check for stencil buffer dirtyness?", CVAR_Archive);


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
  //glDepthMask(GL_FALSE); // no z-buffer writes
  glDisableDepthWrite();
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
      /*
      GLint oldDepthMask;
      glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);
      */
      PushDepthMask();
      glDisableDepthWrite();

      //glDisable(GL_STENCIL_TEST);
      glEnable(GL_SCISSOR_TEST);
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_CULL_FACE);
      GLDisableBlend();
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

      glMatrixMode(GL_PROJECTION); glPushMatrix(); //glLoadIdentity();
      glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
      //glMatrixMode(GL_TEXTURE); glPushMatrix();
      //glMatrixMode(GL_COLOR); glPushMatrix();

      p_glUseProgramObjectARB(0);
      currentActiveShader = nullptr;
      glStencilFunc(GL_ALWAYS, 0x0, 0xff);
      glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);

      SetOrthoProjection(0, Drawer->getWidth(), Drawer->getHeight(), 0);
      //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glBegin(GL_QUADS);
        glVertex2i(0, 0);
        glVertex2i(Drawer->getWidth(), 0);
        glVertex2i(Drawer->getWidth(), Drawer->getHeight());
        glVertex2i(0, Drawer->getHeight());
      glEnd();
      //glBindTexture(GL_TEXTURE_2D, 0);

      //glDisable(GL_STENCIL_TEST);
      //if (oldStencilTest) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
      glMatrixMode(GL_PROJECTION); glPopMatrix();
      glMatrixMode(GL_MODELVIEW); glPopMatrix();
      glMatrixMode(glmatmode);
      //glDepthMask(oldDepthMask);
      PopDepthMask();
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

  GLDisableBlend();
  glDisable(GL_CULL_FACE);
  glStencilFunc(GL_ALWAYS, 0x0, 0xff);
  glEnable(GL_STENCIL_TEST);

  if (!CanUseRevZ()) {
    // normal
    // shadow volume offseting is done in the main renderer
    glDepthFunc(GL_LESS);
    //glDepthFunc(GL_LEQUAL);
  } else {
    // reversed
    // shadow volume offseting is done in the main renderer
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

  setupSpotLight(LightPos, Radius, aconeDir, aconeAngle);

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
  #if 0
  //FIXME: done in main renderer now
  /*if (gl_dbg_adv_render_offset_shadow_volume || !usingFPZBuffer)*/ {
    GLDisableOffset();
  }
  #endif
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
//  FIXME: gozzo 3d-shit extra should be rendered in both directions?
//
//==========================================================================
void VOpenGLDrawer::RenderSurfaceShadowVolume (const surface_t *surf, const TVec &LightPos, float Radius) {
  if (gl_dbg_wireframe) return;
  if (surf->count < 3) return; // just in case

  if (gl_smart_reject_shadows && !AdvRenderCanSurfaceCastShadow(surf, LightPos, Radius)) return;

  if (spotLight && !isSurfaceInSpotlight(surf)) return;

  const unsigned vcount = (unsigned)surf->count;
  const SurfVertex *sverts = surf->verts;
  const SurfVertex *v = sverts;

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
      for (unsigned i = vcount; i--; ) glVertex4(v[i].vec(), 0);
    glEnd();

    // render near cap
    //glBegin(GL_POLYGON);
    glBegin(GL_TRIANGLE_FAN);
      for (unsigned i = 0; i < vcount; ++i) glVertex(sverts[i].vec());
    glEnd();

    // render side caps
    glBegin(GL_TRIANGLE_STRIP);
      for (unsigned i = 0; i < vcount; ++i) {
        glVertex(sverts[i].vec());
        glVertex4(v[i].vec(), 0);
      }
      glVertex(sverts[0].vec());
      glVertex4(v[0].vec(), 0);
    glEnd();
  }
}
