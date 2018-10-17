//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
//**
//**  Functions to draw patches (by post) directly to screen.
//**
//**************************************************************************
#include "gl_local.h"

extern VCvarB gl_pic_filtering;


//==========================================================================
//
//  VOpenGLDrawer::DrawPic
//
//==========================================================================
void VOpenGLDrawer::DrawPic (float x1, float y1, float x2, float y2,
  float s1, float t1, float s2, float t2, VTexture *Tex,
  VTextureTranslation *Trans, float Alpha)
{
  guard(VOpenGLDrawer::DrawPic);
  SetPic(Tex, Trans, CM_Default);
  /*
  int flt = (gl_pic_filtering ? GL_LINEAR : GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, flt);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, flt);
  if (max_anisotropy > 1.0) glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), 1.0f);
  */
  p_glUseProgramObjectARB(DrawSimpleProgram);
  p_glUniform1iARB(DrawSimpleTextureLoc, 0);
  p_glUniform1fARB(DrawSimpleAlphaLoc, Alpha);
  if (Alpha < 1.0) glEnable(GL_BLEND);
  glBegin(GL_QUADS);
    glTexCoord2f(s1*tex_iw, t1*tex_ih); glVertex2f(x1, y1);
    glTexCoord2f(s2*tex_iw, t1*tex_ih); glVertex2f(x2, y1);
    glTexCoord2f(s2*tex_iw, t2*tex_ih); glVertex2f(x2, y2);
    glTexCoord2f(s1*tex_iw, t2*tex_ih); glVertex2f(x1, y2);
  glEnd();
  if (Alpha < 1.0) glDisable(GL_BLEND);
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::DrawPicShadow
//
//==========================================================================
void VOpenGLDrawer::DrawPicShadow (float x1, float y1, float x2, float y2,
  float s1, float t1, float s2, float t2, VTexture *Tex, float shade)
{
  guard(VOpenGLDrawer::DrawPicShadow);
  SetPic(Tex, nullptr, CM_Default);
  /*
  int flt = (gl_pic_filtering ? GL_LINEAR : GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, flt);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, flt);
  if (max_anisotropy > 1.0) glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), 1.0f);
  */
  p_glUseProgramObjectARB(DrawShadowProgram);
  p_glUniform1iARB(DrawSimpleTextureLoc, 0);
  p_glUniform1fARB(DrawSimpleAlphaLoc, shade);
  glEnable(GL_BLEND);
  glBegin(GL_QUADS);
    glTexCoord2f(s1*tex_iw, t1*tex_ih); glVertex2f(x1, y1);
    glTexCoord2f(s2*tex_iw, t1*tex_ih); glVertex2f(x2, y1);
    glTexCoord2f(s2*tex_iw, t2*tex_ih); glVertex2f(x2, y2);
    glTexCoord2f(s1*tex_iw, t2*tex_ih); glVertex2f(x1, y2);
  glEnd();
  glDisable(GL_BLEND);
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::FillRectWithFlat
//
//  Fills rectangle with flat.
//
//==========================================================================
void VOpenGLDrawer::FillRectWithFlat (float x1, float y1, float x2, float y2,
  float s1, float t1, float s2, float t2, VTexture *Tex)
{
  guard(VOpenGLDrawer::FillRectWithFlat);
  SetTexture(Tex, CM_Default);
  p_glUseProgramObjectARB(DrawSimpleProgram);
  p_glUniform1iARB(DrawSimpleTextureLoc, 0);
  p_glUniform1fARB(DrawSimpleAlphaLoc, 1.0);
  glBegin(GL_QUADS);
    glTexCoord2f(s1*tex_iw, t1*tex_ih); glVertex2f(x1, y1);
    glTexCoord2f(s2*tex_iw, t1*tex_ih); glVertex2f(x2, y1);
    glTexCoord2f(s2*tex_iw, t2*tex_ih); glVertex2f(x2, y2);
    glTexCoord2f(s1*tex_iw, t2*tex_ih); glVertex2f(x1, y2);
  glEnd();
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::FillRect
//
//==========================================================================
void VOpenGLDrawer::FillRect (float x1, float y1, float x2, float y2, vuint32 colour) {
  guard(VOpenGLDrawer::FillRect);
  p_glUseProgramObjectARB(DrawFixedColProgram);
  p_glUniform4fARB(DrawFixedColColourLoc,
    (GLfloat)(((colour>>16)&255)/255.0),
    (GLfloat)(((colour>>8)&255)/255.0),
    (GLfloat)((colour&255)/255.0), 1.0);
  glBegin(GL_QUADS);
    glVertex2f(x1, y1);
    glVertex2f(x2, y1);
    glVertex2f(x2, y2);
    glVertex2f(x1, y2);
  glEnd();
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::ShadeRect
//
//  Fade all the screen buffer, so that the menu is more readable,
//  especially now that we use the small hudfont in the menus...
//
//==========================================================================
void VOpenGLDrawer::ShadeRect (int x, int y, int w, int h, float darkening) {
  guard(VOpenGLDrawer::ShadeRect);
  p_glUseProgramObjectARB(DrawFixedColProgram);
  p_glUniform4fARB(DrawFixedColColourLoc, 0, 0, 0, darkening);
  glEnable(GL_BLEND);
  glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x+w, y);
    glVertex2f(x+w, y+h);
    glVertex2f(x, y+h);
  glEnd();
  glDisable(GL_BLEND);
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::DrawConsoleBackground
//
//==========================================================================
void VOpenGLDrawer::DrawConsoleBackground (int h) {
  guard(VOpenGLDrawer::DrawConsoleBackground);
  p_glUseProgramObjectARB(DrawFixedColProgram);
  p_glUniform4fARB(DrawFixedColColourLoc, 0, 0, 0.5, 0.75);
  glEnable(GL_BLEND);
  glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(ScreenWidth, 0);
    glVertex2f(ScreenWidth, h);
    glVertex2f(0, h);
  glEnd();
  glDisable(GL_BLEND);
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::DrawSpriteLump
//
//==========================================================================
void VOpenGLDrawer::DrawSpriteLump (float x1, float y1, float x2, float y2,
  VTexture *Tex, VTextureTranslation *Translation, bool flip)
{
  guard(VOpenGLDrawer::DrawSpriteLump);
  SetSpriteLump(Tex, Translation, CM_Default);

  float s1, s2;
  if (flip) {
    s1 = Tex->GetWidth()*tex_iw;
    s2 = 0;
  } else {
    s1 = 0;
    s2 = Tex->GetWidth()*tex_iw;
  }
  const float texh = Tex->GetHeight()*tex_ih;

  p_glUseProgramObjectARB(DrawSimpleProgram);
  p_glUniform1iARB(DrawSimpleTextureLoc, 0);
  p_glUniform1fARB(DrawSimpleAlphaLoc, 1.0);
  glBegin(GL_QUADS);
    glTexCoord2f(s1, 0); glVertex2f(x1, y1);
    glTexCoord2f(s2, 0); glVertex2f(x2, y1);
    glTexCoord2f(s2, texh); glVertex2f(x2, y2);
    glTexCoord2f(s1, texh); glVertex2f(x1, y2);
  glEnd();
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::StartAutomap
//
//==========================================================================
void VOpenGLDrawer::StartAutomap () {
  guard(VOpenGLDrawer::StartAutomap);
  p_glUseProgramObjectARB(DrawAutomapProgram);
  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_BLEND);
  glBegin(GL_LINES);
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::DrawLine
//
//==========================================================================
void VOpenGLDrawer::DrawLine (int x1, int y1, vuint32 c1, int x2, int y2, vuint32 c2) {
  guard(VOpenGLDrawer::DrawLine);
  SetColour(c1);
  glVertex2f(x1, y1);
  SetColour(c2);
  glVertex2f(x2, y2);
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::EndAutomap
//
//==========================================================================
void VOpenGLDrawer::EndAutomap () {
  guard(VOpenGLDrawer::EndAutomap);
  glEnd();
  glDisable(GL_BLEND);
  glDisable(GL_LINE_SMOOTH);
  unguard;
}
