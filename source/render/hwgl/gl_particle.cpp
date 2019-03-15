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
//  VOpenGLDrawer::StartParticles
//
//==========================================================================
void VOpenGLDrawer::StartParticles () {
  glEnable(GL_BLEND);
  p_glUseProgramObjectARB(gl_smooth_particles ? SurfPartSm_Program : SurfPartSq_Program);
  glBegin(GL_QUADS);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawParticle
//
//==========================================================================
void VOpenGLDrawer::DrawParticle (particle_t *p) {
  GLint lvLoc, tcLoc;
  if (gl_smooth_particles) {
    lvLoc = SurfPartSm_LightValLoc;
    tcLoc = SurfPartSm_TexCoordLoc;
  } else {
    lvLoc = SurfPartSq_LightValLoc;
    tcLoc = SurfPartSq_TexCoordLoc;
  }
  const float r = ((p->colour>>16)&255)/255.0f;
  const float g = ((p->colour>>8)&255)/255.0f;
  const float b = (p->colour&255)/255.0f;
  const float a = ((p->colour>>24)&255)/255.0f;
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
}


//==========================================================================
//
//  VOpenGLDrawer::EndParticles
//
//==========================================================================
void VOpenGLDrawer::EndParticles () {
  glEnd();
  glDisable(GL_BLEND);
}
