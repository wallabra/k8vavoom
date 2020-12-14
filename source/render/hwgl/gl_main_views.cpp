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
//**
//**  OpenGL driver, main module
//**
//**************************************************************************
#include <limits.h>
#include <float.h>
#include <stdarg.h>

#include "gl_local.h"
#include "../r_local.h" /* for VRenderLevelShared */


VCvarI dbg_shadowmaps("dbg_shadowmaps", "0", "Show shadowmap cubemap?", CVAR_PreInit);


//==========================================================================
//
//  VOpenGLDrawer::Setup2D
//
//==========================================================================
void VOpenGLDrawer::Setup2D () {
  GLSetViewport(0, 0, getWidth(), getHeight());

  //glMatrixMode(GL_PROJECTION);
  //glLoadIdentity();
  SetOrthoProjection(0, getWidth(), getHeight(), 0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_ALPHA_TEST);
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  //GLDisableBlend();
  GLEnableBlend();
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  if (HaveDepthClamp) glDisable(GL_DEPTH_CLAMP);

  glDisable(GL_CLIP_PLANE0);
}


//==========================================================================
//
//  VOpenGLDrawer::StartUpdate
//
//==========================================================================
void VOpenGLDrawer::StartUpdate () {
  //glFinish();
  VRenderLevelShared::ResetPortalPool();

  ActivateMainFBO();

  glBindTexture(GL_TEXTURE_2D, 0);
  //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  // turn off anisotropy
  //glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), 1); // 1 is minimum, i.e. "off"

  if (usegamma != lastgamma) {
    FlushTextures(true); // forced
    lastgamma = usegamma;
  }

  Setup2D();
}


//==========================================================================
//
//  VOpenGLDrawer::FinishUpdate
//
//==========================================================================
void VOpenGLDrawer::FinishUpdate () {
  //mainFBO.blitToScreen();
  GetMainFBO()->blitToScreen();
  ClearAllShadowMaps();
  glBindTexture(GL_TEXTURE_2D, 0);
  SetMainFBO(true); // forced
  glBindTexture(GL_TEXTURE_2D, 0);
  SetOrthoProjection(0, getWidth(), getHeight(), 0);
  //ActivateMainFBO();
  //glFlush();
}


//==========================================================================
//
//  VOpenGLDrawer::SetupView
//
//==========================================================================
void VOpenGLDrawer::SetupView (VRenderLevelDrawer *ARLev, const refdef_t *rd) {
  RendLev = ARLev;

  glColor4f(1.0f, 1.0f, 1.0f, 1.0f); // why not
  glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT|(rd->drawworld && !rd->DrawCamera && clear ? GL_COLOR_BUFFER_BIT : 0));
  stencilBufferDirty = false;

  if (!rd->DrawCamera && rd->drawworld && rd->width != ScreenWidth) {
    // draws the border around the view for different size windows
    R_DrawViewBorder();
  }
  glBindTexture(GL_TEXTURE_2D, 0);

  if (!CanUseRevZ()) {
    // normal
    glClearDepth(1.0f);
    glDepthFunc(GL_LEQUAL);
  } else {
    // reversed
    glClearDepth(0.0f);
    glDepthFunc(GL_GEQUAL);
  }
  //RestoreDepthFunc();

  GLSetViewport(rd->x, getHeight()-rd->height-rd->y, rd->width, rd->height);
  vpmats.vport.setOrigin(rd->x, getHeight()-rd->height-rd->y);
  vpmats.vport.setSize(rd->width, rd->height);
  /*
  {
    GLint vport[4];
    glGetIntegerv(GL_VIEWPORT, vport);
    //vpmats.vport.setOrigin(vport[0], vport[1]);
    //vpmats.vport.setSize(vport[2], vport[3]);
    GCon->Logf(NAME_Debug, "VP: (%d,%d);(%d,%d) -- (%d,%d);(%d,%d)", vpmats.vport.x0, vpmats.vport.y0, vpmats.vport.width, vpmats.vport.height, vport[0], vport[1], vport[2], vport[3]);
  }
  */

  CalcProjectionMatrix(vpmats.projMat, ARLev, rd);
  glMatrixMode(GL_PROJECTION);
  glLoadMatrixf(vpmats.projMat[0]);
  glDisable(GL_CLIP_PLANE0);

  //vpmats.projMat = ProjMat;
  vpmats.modelMat.SetIdentity();

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_FRONT);

  glEnable(GL_DEPTH_TEST);
  //GLDisableBlend();
  GLEnableBlend();
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_ALPHA_TEST);
  if (RendLev && RendLev->IsShadowVolumeRenderer() && HaveDepthClamp) glEnable(GL_DEPTH_CLAMP);
  //k8: there is no reason to not do it
  //if (HaveDepthClamp) glEnable(GL_DEPTH_CLAMP);

  glEnable(GL_TEXTURE_2D);
  glDisable(GL_STENCIL_TEST);
  //glDepthMask(GL_TRUE); // allow z-buffer writes
  glEnableDepthWrite();
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  glDisable(GL_SCISSOR_TEST);
  currentSVScissor[SCS_MINX] = currentSVScissor[SCS_MINY] = 0;
  currentSVScissor[SCS_MAXX] = currentSVScissor[SCS_MAXY] = 32000;

  // just in case
  glDisable(GL_CLIP_PLANE0);
}


//==========================================================================
//
//  VOpenGLDrawer::SetupViewOrg
//
//==========================================================================
void VOpenGLDrawer::SetupViewOrg () {
  glMatrixMode(GL_MODELVIEW);
  //glLoadIdentity();
  CalcModelMatrix(vpmats.modelMat, vieworg, viewangles, MirrorClip);
  glLoadMatrixf(vpmats.modelMat[0]);

  glCullFace(MirrorClip ? GL_BACK : GL_FRONT);

  SetupClipPlanes();
  //LoadVPMatrices();
}


//==========================================================================
//
//  VOpenGLDrawer::EndView
//
//==========================================================================
void VOpenGLDrawer::EndView (bool ignoreColorTint) {
  Setup2D();

  if (!ignoreColorTint && cl && cl->CShift) {
    DrawFixedCol.Activate();
    DrawFixedCol.SetColor(
      (float)((cl->CShift>>16)&255)/255.0f,
      (float)((cl->CShift>>8)&255)/255.0f,
      (float)(cl->CShift&255)/255.0f,
      (float)((cl->CShift>>24)&255)/255.0f);
    DrawFixedCol.UploadChangedUniforms();
    //GLEnableBlend();
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    glBegin(GL_QUADS);
      glVertex2f(0, 0);
      glVertex2f(getWidth(), 0);
      glVertex2f(getWidth(), getHeight());
      glVertex2f(0, getHeight());
    glEnd();

    //GLDisableBlend();
    glEnable(GL_TEXTURE_2D);
  }

#if 0
  if (r_shadowmaps.asBool() && CanRenderShadowMaps() && (dbg_shadowmaps.asInt()&0x01) != 0) {
    // right
    // left
    // top
    // bottom
    // back
    // front
    //   front, back
    //   left, right
    //   top, bottom

    T_SetFont(ConFont);
    T_SetAlign(hcenter, vtop);
    const char *cbnames[6] = { "POS-X", "NEG-X", "POS-Y", "NEG-Y", "POS-Z", "NEG-Z" };

    //const float ssize = shadowmapSize;
    const float ssize = 128.0f;
    const float fyofs = T_ToDrawerY(T_FontHeight()+2);
    const float ysize = (ssize+T_ToDrawerY(T_FontHeight()+4));
    GLDisableBlend();
    for (unsigned int face = 0; face < 6; ++face) {
      float xofs = (face%2)*(ssize+4);
      float yofs = (face/2%3)*ysize+fyofs*2;
      glDisable(GL_TEXTURE_2D);
      glEnable(GL_TEXTURE_2D);
      glEnable(GL_TEXTURE_CUBE_MAP);
      glBindTexture(GL_TEXTURE_CUBE_MAP, cubeTexId);
      DbgShadowMap.Activate();
      DbgShadowMap.SetTexture(0);
      DbgShadowMap.SetCubeFace(face+0.5f);
      DbgShadowMap.UploadChangedUniforms();
      glBegin(GL_QUADS);
        glTexCoord2f(0, 1); glVertex2f(xofs+0    , yofs+0);
        glTexCoord2f(1, 1); glVertex2f(xofs+ssize, yofs+0);
        glTexCoord2f(1, 0); glVertex2f(xofs+ssize, yofs+ssize);
        glTexCoord2f(0, 0); glVertex2f(xofs+0    , yofs+ssize);
      glEnd();
      glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
      glDisable(GL_TEXTURE_CUBE_MAP);
      glEnable(GL_TEXTURE_2D);
    }
    GLEnableBlend();

    for (unsigned int face = 0; face < 6; ++face) {
      int xpos = T_FromDrawerX((int)((face%2)*(ssize+4)+(ssize+4)/2));
      int ypos = T_FromDrawerY((int)((face/2%3)*ysize))+T_FontHeight()+2+1;
      T_DrawText(xpos, ypos, cbnames[face], CR_DARKBROWN);
    }
  }
#endif
}
