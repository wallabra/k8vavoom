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


//==========================================================================
//
//  VOpenGLDrawer::StartParticles
//
//==========================================================================
void VOpenGLDrawer::StartParticles () {
  GLEnableBlend();
  if (gl_smooth_particles) SurfPartSm.Activate(); else SurfPartSq.Activate();
  currentActiveShader->UploadChangedUniforms();
  glBegin(GL_QUADS);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawParticle
//
//==========================================================================
void VOpenGLDrawer::DrawParticle (particle_t *p) {
  const float r = ((p->color>>16)&255)/255.0f;
  const float g = ((p->color>>8)&255)/255.0f;
  const float b = (p->color&255)/255.0f;
  const float a = ((p->color>>24)&255)/255.0f;

  //GLint lvLoc, tcLoc;
  if (gl_smooth_particles) {
    SurfPartSm.SetLightValAttr(r, g, b, a);
    SurfPartSm.SetTexCoordAttr(-1, -1);
    //SurfPartSm.UploadChangedAttrs();
    glVertex(p->org-viewright*p->Size+viewup*p->Size);

    SurfPartSm.SetLightValAttr(r, g, b, a);
    SurfPartSm.SetTexCoordAttr(1, -1);
    //SurfPartSm.UploadChangedAttrs();
    glVertex(p->org+viewright*p->Size+viewup*p->Size);

    SurfPartSm.SetLightValAttr(r, g, b, a);
    SurfPartSm.SetTexCoordAttr(1, 1);
    //SurfPartSm.UploadChangedAttrs();
    glVertex(p->org+viewright*p->Size-viewup*p->Size);

    SurfPartSm.SetLightValAttr(r, g, b, a);
    SurfPartSm.SetTexCoordAttr(-1, 1);
    //SurfPartSm.UploadChangedAttrs();
    glVertex(p->org-viewright*p->Size-viewup*p->Size);
  } else {
    SurfPartSq.SetLightValAttr(r, g, b, a);
    SurfPartSq.SetTexCoordAttr(-1, -1);
    //SurfPartSq.UploadChangedAttrs();
    glVertex(p->org-viewright*p->Size+viewup*p->Size);

    SurfPartSq.SetLightValAttr(r, g, b, a);
    SurfPartSq.SetTexCoordAttr(1, -1);
    //SurfPartSq.UploadChangedAttrs();
    glVertex(p->org+viewright*p->Size+viewup*p->Size);

    SurfPartSq.SetLightValAttr(r, g, b, a);
    SurfPartSq.SetTexCoordAttr(1, 1);
    //SurfPartSq.UploadChangedAttrs();
    glVertex(p->org+viewright*p->Size-viewup*p->Size);

    SurfPartSq.SetLightValAttr(r, g, b, a);
    SurfPartSq.SetTexCoordAttr(-1, 1);
    //SurfPartSq.UploadChangedAttrs();
    glVertex(p->org-viewright*p->Size-viewup*p->Size);
  }
  /*
  p_glVertexAttrib4fARB(lvLoc, r, g, b, a);
  p_glVertexAttrib2fARB(tcLoc, -1, -1);
  glVertex(p->org-viewright*p->Size+viewup*p->Size);
  p_glVertexAttrib4fARB(lvLoc, r, g, b, a);
  p_glVertexAttrib2fARB(tcLoc, 1, -1);
  glVertex(p->org+viewright*p->Size+viewup*p->Size);
  p_glVertexAttrib4fARB(lvLoc, r, g, b, a);
  p_glVertexAttrib2fARB(tcLoc, 1, 1);
  glVertex(p->org+viewright*p->Size-viewup*p->Size);
  p_glVertexAttrib4fARB(lvLoc, r, g, b, a);
  p_glVertexAttrib2fARB(tcLoc, -1, 1);
  glVertex(p->org-viewright*p->Size-viewup*p->Size);
  */
}


//==========================================================================
//
//  VOpenGLDrawer::EndParticles
//
//==========================================================================
void VOpenGLDrawer::EndParticles () {
  glEnd();
  //GLDisableBlend();
}
