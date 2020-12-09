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
  if (!Tex || Alpha <= 0.0f || Tex->Type == TEXTYPE_Null) return;
  SetPic(Tex, Trans, CM_Default);
  DrawSimple.Activate();
  DrawSimple.SetTexture(0);
  DrawSimple.SetAlpha(Alpha);
  DrawSimple.UploadChangedUniforms();
  //GLEnableBlend();
  //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // premultiplied
  glBegin(GL_QUADS);
    glTexCoord2f(s1*tex_iw, t1*tex_ih); glVertex2f(x1, y1);
    glTexCoord2f(s2*tex_iw, t1*tex_ih); glVertex2f(x2, y1);
    glTexCoord2f(s2*tex_iw, t2*tex_ih); glVertex2f(x2, y2);
    glTexCoord2f(s1*tex_iw, t2*tex_ih); glVertex2f(x1, y2);
  glEnd();
  //GLDisableBlend();
}


//==========================================================================
//
//  VOpenGLDrawer::DrawPicShadow
//
//==========================================================================
void VOpenGLDrawer::DrawPicShadow (float x1, float y1, float x2, float y2,
  float s1, float t1, float s2, float t2, VTexture *Tex, float shade)
{
  if (!Tex || shade <= 0.0f || Tex->Type == TEXTYPE_Null) return;
  SetPic(Tex, nullptr, CM_Default);
  DrawShadow.Activate();
  DrawShadow.SetTexture(0);
  DrawShadow.SetAlpha(shade);
  DrawShadow.UploadChangedUniforms();
  //GLEnableBlend();
  glBegin(GL_QUADS);
    glTexCoord2f(s1*tex_iw, t1*tex_ih); glVertex2f(x1, y1);
    glTexCoord2f(s2*tex_iw, t1*tex_ih); glVertex2f(x2, y1);
    glTexCoord2f(s2*tex_iw, t2*tex_ih); glVertex2f(x2, y2);
    glTexCoord2f(s1*tex_iw, t2*tex_ih); glVertex2f(x1, y2);
  glEnd();
  //GLDisableBlend();
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
  if (!Tex || Tex->Type == TEXTYPE_Null) return;
  SetTexture(Tex, CM_Default);
  DrawSimple.Activate();
  DrawSimple.SetTexture(0);
  DrawSimple.SetAlpha(1.0f);
  DrawSimple.UploadChangedUniforms();
  //GLEnableBlend();
  //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // premultiplied
  glBegin(GL_QUADS);
    glTexCoord2f(s1*tex_iw_sc, t1*tex_ih_sc); glVertex2f(x1, y1);
    glTexCoord2f(s2*tex_iw_sc, t1*tex_ih_sc); glVertex2f(x2, y1);
    glTexCoord2f(s2*tex_iw_sc, t2*tex_ih_sc); glVertex2f(x2, y2);
    glTexCoord2f(s1*tex_iw_sc, t2*tex_ih_sc); glVertex2f(x1, y2);
  glEnd();
  //GLDisableBlend();
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
  if (!Tex || Tex->Type == TEXTYPE_Null) return;
  SetTexture(Tex, CM_Default);
  DrawSimple.Activate();
  DrawSimple.SetTexture(0);
  DrawSimple.SetAlpha(1.0f);
  DrawSimple.UploadChangedUniforms();
  float oldWS, oldWT;
  glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &oldWS);
  glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &oldWT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  //GLEnableBlend();
  //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // premultiplied
  glBegin(GL_QUADS);
    glTexCoord2f(s1*tex_iw_sc, t1*tex_ih_sc); glVertex2f(x1, y1);
    glTexCoord2f(s2*tex_iw_sc, t1*tex_ih_sc); glVertex2f(x2, y1);
    glTexCoord2f(s2*tex_iw_sc, t2*tex_ih_sc); glVertex2f(x2, y2);
    glTexCoord2f(s1*tex_iw_sc, t2*tex_ih_sc); glVertex2f(x1, y2);
  glEnd();
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, oldWS);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, oldWT);
  //GLDisableBlend();
}


//==========================================================================
//
//  VOpenGLDrawer::FillRect
//
//==========================================================================
void VOpenGLDrawer::FillRect (float x1, float y1, float x2, float y2, vuint32 color, float alpha) {
  if (alpha < 0.0f) return;
  DrawFixedCol.Activate();
  if (alpha < 1.0f) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
  DrawFixedCol.SetColor(
    (GLfloat)(((color>>16)&255)/255.0f),
    (GLfloat)(((color>>8)&255)/255.0f),
    (GLfloat)((color&255)/255.0f), min2(1.0f, alpha));
  DrawFixedCol.UploadChangedUniforms();
  glBegin(GL_QUADS);
    glVertex2f(x1, y1);
    glVertex2f(x2, y1);
    glVertex2f(x2, y2);
    glVertex2f(x1, y2);
  glEnd();
  if (alpha < 1.0f) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawHex
//
//==========================================================================
void VOpenGLDrawer::DrawHex (float x0, float y0, float w, float h, vuint32 color, float alpha) {
  if (alpha < 0.0f || w < 1.0f || h < 1.0f) return;
  DrawFixedCol.Activate();
  if (alpha < 1.0f) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  DrawFixedCol.SetColor(
    (GLfloat)(((color>>16)&255)/255.0f),
    (GLfloat)(((color>>8)&255)/255.0f),
    (GLfloat)((color&255)/255.0f), min2(1.0f, alpha));
  DrawFixedCol.UploadChangedUniforms();
  float vx[6];
  float vy[6];
  CalcHexVertices(vx, vy, x0, y0, w, h);
  glBegin(GL_LINE_LOOP);
    glVertex2f(vx[0], vy[0]);
    glVertex2f(vx[1], vy[1]);
    glVertex2f(vx[2], vy[2]);
    glVertex2f(vx[3], vy[3]);
    glVertex2f(vx[4], vy[4]);
    glVertex2f(vx[5], vy[5]);
  glEnd();
  if (alpha < 1.0f) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}


//==========================================================================
//
//  VOpenGLDrawer::FillHex
//
//==========================================================================
void VOpenGLDrawer::FillHex (float x0, float y0, float w, float h, vuint32 color, float alpha) {
  if (alpha < 0.0f || w < 1.0f || h < 1.0f) return;
  DrawFixedCol.Activate();
  if (alpha < 1.0f) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  DrawFixedCol.SetColor(
    (GLfloat)(((color>>16)&255)/255.0f),
    (GLfloat)(((color>>8)&255)/255.0f),
    (GLfloat)((color&255)/255.0f), min2(1.0f, alpha));
  DrawFixedCol.UploadChangedUniforms();
  float vx[6];
  float vy[6];
  CalcHexVertices(vx, vy, x0, y0, w, h);
  glBegin(GL_POLYGON);
    glVertex2f(vx[0], vy[0]);
    glVertex2f(vx[1], vy[1]);
    glVertex2f(vx[2], vy[2]);
    glVertex2f(vx[3], vy[3]);
    glVertex2f(vx[4], vy[4]);
    glVertex2f(vx[5], vy[5]);
  glEnd();
  if (alpha < 1.0f) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}


//==========================================================================
//
//  VOpenGLDrawer::ShadeHex
//
//==========================================================================
void VOpenGLDrawer::ShadeHex (float x0, float y0, float w, float h, float darkening) {
  if (w < 1.0f || h < 1.0f) return;
  DrawFixedCol.Activate();
  DrawFixedCol.SetColor(0.0f, 0.0f, 0.0f, darkening);
  DrawFixedCol.UploadChangedUniforms();
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  float vx[6];
  float vy[6];
  CalcHexVertices(vx, vy, x0, y0, w, h);
  glBegin(GL_POLYGON);
    glVertex2f(vx[0], vy[0]);
    glVertex2f(vx[1], vy[1]);
    glVertex2f(vx[2], vy[2]);
    glVertex2f(vx[3], vy[3]);
    glVertex2f(vx[4], vy[4]);
    glVertex2f(vx[5], vy[5]);
  glEnd();
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}


//==========================================================================
//
//  VOpenGLDrawer::ShadeRect
//
//  Fade all the screen buffer, so that the menu is more readable,
//  especially now that we use the small hudfont in the menus...
//
//==========================================================================
void VOpenGLDrawer::ShadeRect (float x1, float y1, float x2, float y2, float darkening) {
  DrawFixedCol.Activate();
  DrawFixedCol.SetColor(0.0f, 0.0f, 0.0f, darkening);
  DrawFixedCol.UploadChangedUniforms();
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
  //GLEnableBlend();
  glBegin(GL_QUADS);
    glVertex2f(x1, y1);
    glVertex2f(x2, y1);
    glVertex2f(x2, y2);
    glVertex2f(x1, y2);
  glEnd();
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  //GLDisableBlend();
}


//==========================================================================
//
//  VOpenGLDrawer::DrawLine
//
//==========================================================================
void VOpenGLDrawer::DrawLine (int x1, int y1, int x2, int y2, vuint32 color, float alpha) {
  if (alpha < 0.0f) return;
  if (x1 == x2 && y1 == y2) return;
  DrawFixedCol.Activate();
  if (alpha < 1.0f) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
  DrawFixedCol.SetColor(
    (GLfloat)(((color>>16)&255)/255.0f),
    (GLfloat)(((color>>8)&255)/255.0f),
    (GLfloat)((color&255)/255.0f), min2(1.0f, alpha));
  DrawFixedCol.UploadChangedUniforms();
  glBegin(GL_LINES);
    glVertex2f(x1+0.375f, y1+0.375f);
    glVertex2f(x2+0.375f, y2+0.375f);
  glEnd();
  if (alpha < 1.0f) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawRect
//
//==========================================================================
void VOpenGLDrawer::DrawRect (float x1, float y1, float x2, float y2, vuint32 color, float alpha) {
  if (alpha < 0.0f) return;
  DrawFixedCol.Activate();
  if (alpha < 1.0f) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
  DrawFixedCol.SetColor(
    (GLfloat)(((color>>16)&255)/255.0f),
    (GLfloat)(((color>>8)&255)/255.0f),
    (GLfloat)((color&255)/255.0f), min2(1.0f, alpha));
  DrawFixedCol.UploadChangedUniforms();
  glBegin(GL_LINE_LOOP);
    glVertex2f(x1, y1);
    glVertex2f(x2, y1);
    glVertex2f(x2, y2);
    glVertex2f(x1, y2);
    // last line point
         if (y2 < y1) glVertex2f(x1, y2-1);
    else if (y2 > y1) glVertex2f(x1, y2+1);
  glEnd();
  if (alpha < 1.0f) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawConsoleBackground
//
//==========================================================================
void VOpenGLDrawer::DrawConsoleBackground (int h) {
  DrawFixedCol.Activate();
  DrawFixedCol.SetColor(0.0f, 0.0f, 0.5f, 0.75f);
  DrawFixedCol.UploadChangedUniforms();
  //GLEnableBlend();
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
  glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(getWidth(), 0);
    glVertex2f(getWidth(), h);
    glVertex2f(0, h);
  glEnd();
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  //GLDisableBlend();
}


//==========================================================================
//
//  VOpenGLDrawer::DrawSpriteLump
//
//==========================================================================
void VOpenGLDrawer::DrawSpriteLump (float x1, float y1, float x2, float y2,
  VTexture *Tex, VTextureTranslation *Translation, bool flip)
{
  if (!Tex || Tex->Type == TEXTYPE_Null) return;
  SetSpriteTexture(sprite_filter, Tex, Translation, CM_Default, true);

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
  DrawSimple.UploadChangedUniforms();
  //GLEnableBlend();
  //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // premultiplied
  glBegin(GL_QUADS);
    glTexCoord2f(s1, 0); glVertex2f(x1, y1);
    glTexCoord2f(s2, 0); glVertex2f(x2, y1);
    glTexCoord2f(s2, texh); glVertex2f(x2, y2);
    glTexCoord2f(s1, texh); glVertex2f(x1, y2);
  glEnd();
  //GLDisableBlend();
}


//==========================================================================
//
//  VOpenGLDrawer::StartAutomap
//
//==========================================================================
void VOpenGLDrawer::StartAutomap (bool asOverlay) {
  DrawAutomap.Activate();
  DrawAutomap.UploadChangedUniforms();
  //glEnable(GL_LINE_SMOOTH); // this may cause problems
  if (asOverlay) {
    GLEnableBlend();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
  } else {
    // non-overlay
    GLDisableBlend();
  }
  glBegin(GL_LINES);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawLineAM
//
//==========================================================================
void VOpenGLDrawer::DrawLineAM (float x1, float y1, vuint32 c1, float x2, float y2, vuint32 c2) {
  SetColor(c1);
  glVertex2f(x1, y1);
  SetColor(c2);
  glVertex2f(x2, y2);
}


//==========================================================================
//
//  VOpenGLDrawer::EndAutomap
//
//==========================================================================
void VOpenGLDrawer::EndAutomap () {
  glEnd();
  GLEnableBlend();
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_LINE_SMOOTH);
}


//==========================================================================
//
//  VOpenGLDrawer::BeginTexturedPolys
//
//==========================================================================
void VOpenGLDrawer::BeginTexturedPolys () {
  texturedPolyLastTex = nullptr;
  texturedPolyLastAlpha = 1.0f;
  texturedPolyLastLight = TVec(1.0f, 1.0f, 1.0f);
  DrawSimpleLight.Activate();
  DrawSimpleLight.SetTexture(0);
  DrawSimpleLight.SetAlpha(texturedPolyLastAlpha);
  DrawSimpleLight.SetLight(texturedPolyLastLight.x, texturedPolyLastLight.y, texturedPolyLastLight.z, 1.0f);
  DrawSimpleLight.UploadChangedUniforms();
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // premultiplied
}


//==========================================================================
//
//  VOpenGLDrawer::EndTexturedPolys
//
//==========================================================================
void VOpenGLDrawer::EndTexturedPolys () {
  texturedPolyLastTex = nullptr;
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // non-premultiplied
}


//==========================================================================
//
//  VOpenGLDrawer::DrawTexturedPoly
//
//==========================================================================
void VOpenGLDrawer::DrawTexturedPoly (const texinfo_t *tinfo, TVec light, float alpha,
                                      int vcount, const TVec *verts, const SurfVertex *origverts)
{
  if (!tinfo || !tinfo->Tex || vcount < 3 || alpha < 0.0f || tinfo->Tex->Type == TEXTYPE_Null) return;
  if (tinfo->Tex != texturedPolyLastTex) {
    texturedPolyLastTex = tinfo->Tex;
    SetTexture(texturedPolyLastTex, CM_Default);
  }
  if (alpha != texturedPolyLastAlpha) {
    texturedPolyLastAlpha = alpha;
    DrawSimpleLight.SetAlpha(alpha);
  }
  if (light != texturedPolyLastLight) {
    texturedPolyLastLight = light;
    DrawSimpleLight.SetLight(light.x, light.y, light.z, 1.0f);
  }
  DrawSimpleLight.UploadChangedUniforms();
  glBegin(GL_TRIANGLE_FAN);
  if (origverts) {
    for (; vcount--; ++verts, ++origverts) {
      glTexCoord2f(
        (DotProduct(origverts->vec(), tinfo->saxis)+tinfo->soffs)*tex_iw,
        (DotProduct(origverts->vec(), tinfo->taxis)+tinfo->toffs)*tex_ih);
      glVertex2f(verts->x, verts->y);
    }
  } else {
    for (; vcount--; ++verts) {
      glTexCoord2f(
        (DotProduct(*verts, tinfo->saxis)+tinfo->soffs)*tex_iw,
        (DotProduct(*verts, tinfo->taxis)+tinfo->toffs)*tex_ih);
      glVertex2f(verts->x, verts->y);
    }
  }
  glEnd();
}
