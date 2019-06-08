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
//  VOpenGLDrawer::StartPortal
//
//==========================================================================
bool VOpenGLDrawer::StartPortal (VPortal *Portal, bool UseStencil) {
  if (UseStencil) {
    ClearStencilBuffer();
    NoteStencilBufferDirty();

    if (Portal->stackedSector) {
      // doesn't work for now
      // k8: why? because this glitches in kdizd z1m1, for example
      //     stacked sector rendering should be rewritten
      if (Portal->stackedSector && RendLev->NeedsInfiniteFarClip) return false;

      // disable drawing
      SurfZBuf.Activate();
      glDisable(GL_TEXTURE_2D);
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      glDepthMask(GL_FALSE); // no z-buffer writes

      // set up stencil test
      /*if (!RendLev->PortalDepth)*/ glEnable(GL_STENCIL_TEST);
      glStencilFunc(GL_EQUAL, RendLev->PortalDepth, ~0);
      glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);

      // mark the portal area
      DrawPortalArea(Portal);

      // set up stencil test for portal
      glStencilFunc(GL_EQUAL, RendLev->PortalDepth+1, ~0);
      glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

      if (Portal->NeedsDepthBuffer()) {
        glDepthMask(GL_TRUE); // allow z-buffer writes
        // clear depth buffer
        if (CanUseRevZ()) glDepthRange(0, 0); else glDepthRange(1, 1);
        glDepthFunc(GL_ALWAYS);
        DrawPortalArea(Portal);
        //glDepthFunc(GL_LEQUAL);
        RestoreDepthFunc();
        glDepthRange(0, 1);
      } else {
        glDepthMask(GL_FALSE); // no z-buffer writes
        glDisable(GL_DEPTH_TEST);
      }
    } else {
      glDisable(GL_STENCIL_TEST);
    }

    // enable drawing
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glEnable(GL_TEXTURE_2D);
    ++RendLev->PortalDepth;
  } else {
    if (!Portal->NeedsDepthBuffer()) {
      glDepthMask(GL_FALSE); // no z-buffer writes
      glDisable(GL_DEPTH_TEST);
    }
  }
  return true;
}


//==========================================================================
//
//  VOpenGLDrawer::DrawPortalArea
//
//==========================================================================
void VOpenGLDrawer::DrawPortalArea (VPortal *Portal) {
  for (int i = 0; i < Portal->Surfs.Num(); ++i) {
    const surface_t *surf = Portal->Surfs[i];
    if (surf->count < 3) {
      if (developer) GCon->Logf(NAME_Dev, "trying to render portal surface with %d vertices", surf->count);
      continue;
    }
    glBegin(GL_POLYGON);
    for (unsigned j = 0; j < (unsigned)surf->count; ++j) glVertex(surf->verts[j]);
    glEnd();
  }
}


//==========================================================================
//
//  VSoftwareDrawer::EndPortal
//
//==========================================================================
void VOpenGLDrawer::EndPortal (VPortal *Portal, bool UseStencil) {
  SurfZBuf.Activate();
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glDisable(GL_TEXTURE_2D);

  if (UseStencil) {
    if (/*!Portal->stackedSector*/true) {
      if (gl_dbg_render_stack_portal_bounds && Portal->stackedSector) {
        p_glUseProgramObjectARB(0);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthFunc(GL_ALWAYS);
        glDepthMask(GL_FALSE); // no z-buffer writes
        glColor3f(1, 0, 0);
        glDisable(GL_BLEND);
        glDisable(GL_STENCIL_TEST);
        DrawPortalArea(Portal);

        glEnable(GL_STENCIL_TEST);
        SurfZBuf.Activate();
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_TRUE); // allow z-buffer writes
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
        glDepthMask(GL_TRUE); // allow z-buffer writes
        glEnable(GL_DEPTH_TEST);
      }

      glStencilOp(GL_KEEP, GL_KEEP, GL_DECR);

      // draw proper z-buffer for the portal area
      glDepthFunc(GL_ALWAYS);
      DrawPortalArea(Portal);
      //glDepthFunc(GL_LEQUAL);
      RestoreDepthFunc();

      glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
      glStencilFunc(GL_EQUAL, RendLev->PortalDepth, ~0);
      NoteStencilBufferDirty(); // just in case
    }

    --RendLev->PortalDepth;
    if (RendLev->PortalDepth == 0) glDisable(GL_STENCIL_TEST);
  } else {
    if (Portal->NeedsDepthBuffer()) {
      // clear depth buffer
      glClear(GL_DEPTH_BUFFER_BIT);
    } else {
      glDepthMask(GL_TRUE); // allow z-buffer writes
      glEnable(GL_DEPTH_TEST);
    }

    // draw proper z-buffer for the portal area
    DrawPortalArea(Portal);
  }

  glEnable(GL_TEXTURE_2D);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}
