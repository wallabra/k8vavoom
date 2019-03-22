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
  SetPic(Tex, Trans, CM_Default);
  DrawSimple.Activate();
  DrawSimple.SetTexture(0);
  DrawSimple.SetAlpha(Alpha);
  if (Alpha < 1.0f) glEnable(GL_BLEND);
  glBegin(GL_QUADS);
    glTexCoord2f(s1*tex_iw, t1*tex_ih); glVertex2f(x1, y1);
    glTexCoord2f(s2*tex_iw, t1*tex_ih); glVertex2f(x2, y1);
    glTexCoord2f(s2*tex_iw, t2*tex_ih); glVertex2f(x2, y2);
    glTexCoord2f(s1*tex_iw, t2*tex_ih); glVertex2f(x1, y2);
  glEnd();
  if (Alpha < 1.0f) glDisable(GL_BLEND);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawPicShadow
//
//==========================================================================
void VOpenGLDrawer::DrawPicShadow (float x1, float y1, float x2, float y2,
  float s1, float t1, float s2, float t2, VTexture *Tex, float shade)
{
  SetPic(Tex, nullptr, CM_Default);
  DrawShadow.Activate();
  DrawShadow.SetTexture(0);
  DrawShadow.SetAlpha(shade);
  glEnable(GL_BLEND);
  glBegin(GL_QUADS);
    glTexCoord2f(s1*tex_iw, t1*tex_ih); glVertex2f(x1, y1);
    glTexCoord2f(s2*tex_iw, t1*tex_ih); glVertex2f(x2, y1);
    glTexCoord2f(s2*tex_iw, t2*tex_ih); glVertex2f(x2, y2);
    glTexCoord2f(s1*tex_iw, t2*tex_ih); glVertex2f(x1, y2);
  glEnd();
  glDisable(GL_BLEND);
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
  SetTexture(Tex, CM_Default);
  DrawSimple.Activate();
  DrawSimple.SetTexture(0);
  DrawSimple.SetAlpha(1.0f);
  glBegin(GL_QUADS);
    glTexCoord2f(s1*tex_iw, t1*tex_ih); glVertex2f(x1, y1);
    glTexCoord2f(s2*tex_iw, t1*tex_ih); glVertex2f(x2, y1);
    glTexCoord2f(s2*tex_iw, t2*tex_ih); glVertex2f(x2, y2);
    glTexCoord2f(s1*tex_iw, t2*tex_ih); glVertex2f(x1, y2);
  glEnd();
}


//==========================================================================
//
//  VOpenGLDrawer::FillRectWithFlatRepeat
//
//  Fills rectangle with flat.
//
//==========================================================================
void VOpenGLDrawer::FillRectWithFlatRepeat (float x1, float y1, float x2, float y2,
  float s1, float t1, float s2, float t2, VTexture *Tex)
{
  SetTexture(Tex, CM_Default);
  DrawSimple.Activate();
  DrawSimple.SetTexture(0);
  DrawSimple.SetAlpha(1.0f);
  float oldWS, oldWT;
  glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &oldWS);
  glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &oldWT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glBegin(GL_QUADS);
    glTexCoord2f(s1*tex_iw, t1*tex_ih); glVertex2f(x1, y1);
    glTexCoord2f(s2*tex_iw, t1*tex_ih); glVertex2f(x2, y1);
    glTexCoord2f(s2*tex_iw, t2*tex_ih); glVertex2f(x2, y2);
    glTexCoord2f(s1*tex_iw, t2*tex_ih); glVertex2f(x1, y2);
  glEnd();
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, oldWS);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, oldWT);
}


//==========================================================================
//
//  VOpenGLDrawer::FillRect
//
//==========================================================================
void VOpenGLDrawer::FillRect (float x1, float y1, float x2, float y2, vuint32 colour) {
  DrawFixedCol.Activate();
  DrawFixedCol.SetColour(
    (GLfloat)(((colour>>16)&255)/255.0f),
    (GLfloat)(((colour>>8)&255)/255.0f),
    (GLfloat)((colour&255)/255.0f), 1.0f);
  glBegin(GL_QUADS);
    glVertex2f(x1, y1);
    glVertex2f(x2, y1);
    glVertex2f(x2, y2);
    glVertex2f(x1, y2);
  glEnd();
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
  DrawFixedCol.Activate();
  DrawFixedCol.SetColour(0.0f, 0.0f, 0.0f, darkening);
  glEnable(GL_BLEND);
  glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x+w, y);
    glVertex2f(x+w, y+h);
    glVertex2f(x, y+h);
  glEnd();
  glDisable(GL_BLEND);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawConsoleBackground
//
//==========================================================================
void VOpenGLDrawer::DrawConsoleBackground (int h) {
  DrawFixedCol.Activate();
  DrawFixedCol.SetColour(0.0f, 0.0f, 0.5f, 0.75f);
  glEnable(GL_BLEND);
  glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(ScreenWidth, 0);
    glVertex2f(ScreenWidth, h);
    glVertex2f(0, h);
  glEnd();
  glDisable(GL_BLEND);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawSpriteLump
//
//==========================================================================
void VOpenGLDrawer::DrawSpriteLump (float x1, float y1, float x2, float y2,
  VTexture *Tex, VTextureTranslation *Translation, bool flip)
{
  SetSpriteLump(Tex, Translation, CM_Default, true);

  float s1, s2;
  if (flip) {
    s1 = Tex->GetWidth()*tex_iw;
    s2 = 0;
  } else {
    s1 = 0;
    s2 = Tex->GetWidth()*tex_iw;
  }
  const float texh = Tex->GetHeight()*tex_ih;

  DrawSimple.Activate();
  DrawSimple.SetTexture(0);
  DrawSimple.SetAlpha(1.0f);
  glBegin(GL_QUADS);
    glTexCoord2f(s1, 0); glVertex2f(x1, y1);
    glTexCoord2f(s2, 0); glVertex2f(x2, y1);
    glTexCoord2f(s2, texh); glVertex2f(x2, y2);
    glTexCoord2f(s1, texh); glVertex2f(x1, y2);
  glEnd();
}


//==========================================================================
//
//  VOpenGLDrawer::StartAutomap
//
//==========================================================================
void VOpenGLDrawer::StartAutomap () {
  DrawAutomap.Activate();
  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_BLEND);
  glBegin(GL_LINES);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawLine
//
//==========================================================================
void VOpenGLDrawer::DrawLine (float x1, float y1, vuint32 c1, float x2, float y2, vuint32 c2) {
  SetColour(c1);
  glVertex2f(x1, y1);
  SetColour(c2);
  glVertex2f(x2, y2);
}


//==========================================================================
//
//  VOpenGLDrawer::EndAutomap
//
//==========================================================================
void VOpenGLDrawer::EndAutomap () {
  glEnd();
  glDisable(GL_BLEND);
  glDisable(GL_LINE_SMOOTH);
}
