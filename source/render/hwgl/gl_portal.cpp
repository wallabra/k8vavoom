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


//==========================================================================
//
//  VOpenGLDrawer::DrawPortalArea
//
//==========================================================================
void VOpenGLDrawer::DrawPortalArea (VPortal *Portal) {
  for (auto &&surf : Portal->Surfs) {
    //const surface_t *surf = Portal->Surfs[i];
    if (surf->count < 3) continue;
    currentActiveShader->UploadChangedUniforms();
    glBegin(GL_POLYGON);
    for (unsigned j = 0; j < (unsigned)surf->count; ++j) glVertex(surf->verts[j].vec());
    glEnd();
  }
}


//==========================================================================
//
//  VOpenGLDrawer::DisableStenciling
//
//  call this to disable stencil tests instead of doing it directly
//  this is required for portals
//
//==========================================================================
void VOpenGLDrawer::DisableStenciling () {
  if (RendLev->/*PortalDepth*/PortalUsingStencil == 0) glDisable(GL_STENCIL_TEST);
}


//==========================================================================
//
//  VOpenGLDrawer::DisableScissoring
//
//  call this to disable stencil tests instead of doing it directly
//  this is required for portals
//
//==========================================================================
void VOpenGLDrawer::DisableScissoring () {
  //if (RendLev->/*PortalDepth*/PortalUsingStencil == 0) glDisable(GL_SCISSOR_TEST);
  glDisable(GL_SCISSOR_TEST);
}


//==========================================================================
//
//  VOpenGLDrawer::RestorePortalStenciling
//
//==========================================================================
void VOpenGLDrawer::RestorePortalStenciling () {
  if (RendLev->PortalUsingStencil > 0) glEnable(GL_STENCIL_TEST);
}


//==========================================================================
//
//  VOpenGLDrawer::StartPortal
//
//==========================================================================
bool VOpenGLDrawer::StartPortal (VPortal *Portal, bool UseStencil) {
  if (UseStencil) {
    // we need clean stencil buffer on the first portal
    if (RendLev->/*PortalDepth*/PortalUsingStencil == 0) {
      ClearStencilBuffer();
      NoteStencilBufferDirty();
    }

    /*
    if (Portal->IsStack()) {
      // doesn't work for now
      // k8: why? because this glitches in kdizd z1m1, for example
      //     stacked sector rendering should be rewritten
      if (RendLev->IsShadowVolumeRenderer()) return false;
    }
    */

    // disable drawing
    SurfZBuf.Activate();
    glDisable(GL_TEXTURE_2D);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    //glDepthMask(GL_FALSE); // no z-buffer writes
    GLDisableDepthWrite();
    GLDisableBlend();

    // set up stencil test
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, RendLev->PortalDepth+1, ~0u);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    /*
    if (RendLev->PortalDepth == 0) {
      // first portal
      glEnable(GL_STENCIL_TEST);
      glStencilFunc(GL_ALWAYS, RendLev->PortalDepth+1, ~0);
      glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    } else {
      // other portals
      glEnable(GL_STENCIL_TEST);
      glStencilFunc(GL_EQUAL, RendLev->PortalDepth, ~0);
      glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
    }
    */

    // mark the portal area
    DrawPortalArea(Portal);

    // set up stencil test for portal
    glStencilFunc(GL_EQUAL, RendLev->PortalDepth+1, ~0u);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    if (Portal->NeedsDepthBuffer()) {
      //glDepthMask(GL_TRUE); // allow z-buffer writes
      GLEnableDepthWrite();
      // clear depth buffer
      if (CanUseRevZ()) glDepthRange(0, 0); else glDepthRange(1, 1);
      glDepthFunc(GL_ALWAYS);
      DrawPortalArea(Portal);
      //glDepthFunc(GL_LEQUAL);
      RestoreDepthFunc();
      glDepthRange(0, 1);
    } else {
      //glDepthMask(GL_FALSE); // no z-buffer writes
      GLDisableDepthWrite();
      glDisable(GL_DEPTH_TEST);
    }

    // enable drawing
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    GLEnableBlend();

    glEnable(GL_TEXTURE_2D);

    ++RendLev->PortalUsingStencil;
    ++RendLev->PortalDepth;
  } else {
    if (!Portal->NeedsDepthBuffer()) {
      //glDepthMask(GL_FALSE); // no z-buffer writes
      GLDisableDepthWrite();
      glDisable(GL_DEPTH_TEST);
    }
  }
  return true;
}


//==========================================================================
//
//  VOpenGLDrawer::EndPortal
//
//==========================================================================
void VOpenGLDrawer::EndPortal (VPortal *Portal, bool UseStencil) {
  SurfZBuf.Activate();
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glDisable(GL_TEXTURE_2D);
  GLDisableBlend();

  if (UseStencil) {
    if (gl_dbg_render_stack_portal_bounds && Portal->IsStack()) {
      p_glUseProgramObjectARB(0);
      currentActiveShader = nullptr;
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      glDepthFunc(GL_ALWAYS);
      //glDepthMask(GL_FALSE); // no z-buffer writes
      GLDisableDepthWrite();
      glColor3f(1, 0, 0);
      //GLDisableBlend();
      glDisable(GL_STENCIL_TEST);
      DrawPortalArea(Portal);

      glEnable(GL_STENCIL_TEST);
      SurfZBuf.Activate();
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      //glDepthMask(GL_TRUE); // allow z-buffer writes
      GLEnableDepthWrite();
    }

    if (Portal->NeedsDepthBuffer()) {
      // clear depth buffer
      if (CanUseRevZ()) glDepthRange(0, 0); else glDepthRange(1, 1);
      glDepthFunc(GL_ALWAYS);
      DrawPortalArea(Portal);
      //glDepthFunc(GL_LEQUAL);
      RestoreDepthFunc();
      glDepthRange(0, 1);
    } else {
      //glDepthMask(GL_TRUE); // allow z-buffer writes
      GLEnableDepthWrite();
      glEnable(GL_DEPTH_TEST);
    }

    //k8: do not bother clearing stencil buffer
    // but not disable stencil writing, because portal may be partially obscured
    //glStencilOp(GL_KEEP, GL_KEEP, GL_DECR);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP); // this is marginally faster, and we don't care

    // set proper z-buffer values for the portal area
    glDepthFunc(GL_ALWAYS);
    DrawPortalArea(Portal);

    RestoreDepthFunc();

    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilFunc(GL_EQUAL, RendLev->PortalDepth, ~0);
    NoteStencilBufferDirty(); // just in case
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    --RendLev->PortalUsingStencil;
    --RendLev->PortalDepth;
    if (RendLev->PortalDepth == 0) glDisable(GL_STENCIL_TEST);
  } else {
    if (Portal->NeedsDepthBuffer()) {
      // clear depth buffer
      glClear(GL_DEPTH_BUFFER_BIT);
    } else {
      //glDepthMask(GL_TRUE); // allow z-buffer writes
      GLEnableDepthWrite();
      glEnable(GL_DEPTH_TEST);
    }

    // draw proper z-buffer for the portal area
    DrawPortalArea(Portal);
  }

  glEnable(GL_TEXTURE_2D);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  GLEnableBlend();
}
