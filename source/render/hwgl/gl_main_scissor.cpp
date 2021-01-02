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
//**
//**  OpenGL driver, main module
//**
//**************************************************************************
#include <limits.h>
#include <float.h>
#include <stdarg.h>

#include "gl_local.h"
#include "../r_local.h" /* for VRenderLevelShared */


//==========================================================================
//
//  VOpenGLDrawer::SetupLightScissor
//
//  returns:
//   0 if scissor is empty
//  -1 if scissor has no sense (should not be used)
//   1 if scissor is set
//
//==========================================================================
int VOpenGLDrawer::SetupLightScissor (const TVec &org, float radius, int scoord[4], const TVec *geobbox) {
  int tmpscoord[4];

  if (!scoord) scoord = tmpscoord;

  ForceClearScissorState();

  if (radius < 4.0f) {
    scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
    currentSVScissor[SCS_MINX] = currentSVScissor[SCS_MINY] = currentSVScissor[SCS_MAXX] = currentSVScissor[SCS_MAXY] = 0;
    glScissor(0, 0, 0, 0);
    return 0;
  }

  // just in case
  if (!vpmats.vport.isValid()) {
    scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
    currentSVScissor[SCS_MINX] = currentSVScissor[SCS_MINY] = currentSVScissor[SCS_MAXX] = currentSVScissor[SCS_MAXY] = 0;
    glDisable(GL_SCISSOR_TEST);
    return 0;
  }

  // transform into world coords
  TVec inworld = vpmats.toWorld(org);

  // the thing that should not be (completely behind)
  if (inworld.z-radius > -1.0f) {
    scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
    currentSVScissor[SCS_MINX] = currentSVScissor[SCS_MINY] = currentSVScissor[SCS_MAXX] = currentSVScissor[SCS_MAXY] = 0;
    glDisable(GL_SCISSOR_TEST);
    return 0;
  }

  CONST_BBoxVertexIndex;

  // create light bbox
  float bbox[6];
  bbox[0+0] = inworld.x-radius;
  bbox[0+1] = inworld.y-radius;
  bbox[0+2] = inworld.z-radius;

  bbox[3+0] = inworld.x+radius;
  bbox[3+1] = inworld.y+radius;
  bbox[3+2] = min2(-1.0f, inworld.z+radius); // clamp to znear

  // clamp it with geometry bbox, if there is any
  if (geobbox) {
    float gbb[6];
    gbb[0] = geobbox[0].x;
    gbb[1] = geobbox[0].y;
    gbb[2] = geobbox[0].z;
    gbb[3] = geobbox[1].x;
    gbb[4] = geobbox[1].y;
    gbb[5] = geobbox[1].z;
    float trbb[6] = { FLT_MAX, FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (unsigned f = 0; f < 8; ++f) {
      const TVec vtx = vpmats.toWorld(TVec(gbb[BBoxVertexIndex[f][0]], gbb[BBoxVertexIndex[f][1]], gbb[BBoxVertexIndex[f][2]]));
      trbb[0] = min2(trbb[0], vtx.x);
      trbb[1] = min2(trbb[1], vtx.y);
      trbb[2] = min2(trbb[2], vtx.z);
      trbb[3] = max2(trbb[3], vtx.x);
      trbb[4] = max2(trbb[4], vtx.y);
      trbb[5] = max2(trbb[5], vtx.z);
    }

    if (trbb[0] >= trbb[3+0] || trbb[1] >= trbb[3+1] || trbb[2] >= trbb[3+2]) {
      scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
      currentSVScissor[SCS_MINX] = currentSVScissor[SCS_MINY] = currentSVScissor[SCS_MAXX] = currentSVScissor[SCS_MAXY] = 0;
      glDisable(GL_SCISSOR_TEST);
      return 0;
    }

    trbb[2] = min2(-1.0f, trbb[2]);
    trbb[5] = min2(-1.0f, trbb[5]);

    /*
    if (trbb[0] > bbox[0] || trbb[1] > bbox[1] || trbb[2] > bbox[2] ||
        trbb[3] < bbox[3] || trbb[4] < bbox[4] || trbb[5] < bbox[5])
    {
      GCon->Logf("GEOCLAMP: (%f,%f,%f)-(%f,%f,%f) : (%f,%f,%f)-(%f,%f,%f)", bbox[0], bbox[1], bbox[2], bbox[3], bbox[4], bbox[5], trbb[0], trbb[1], trbb[2], trbb[3], trbb[4], trbb[5]);
    }
    */

    bbox[0] = max2(bbox[0], trbb[0]);
    bbox[1] = max2(bbox[1], trbb[1]);
    bbox[2] = max2(bbox[2], trbb[2]);
    bbox[3] = min2(bbox[3], trbb[3]);
    bbox[4] = min2(bbox[4], trbb[4]);
    bbox[5] = min2(bbox[5], trbb[5]);
    if (bbox[0] >= bbox[3+0] || bbox[1] >= bbox[3+1] || bbox[2] >= bbox[3+2]) {
      scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
      currentSVScissor[SCS_MINX] = currentSVScissor[SCS_MINY] = currentSVScissor[SCS_MAXX] = currentSVScissor[SCS_MAXY] = 0;
      glDisable(GL_SCISSOR_TEST);
      return 0;
    }

    /*
    const TVec bc0 = vpmats.toWorld(geobbox[0]);
    const TVec bc1 = vpmats.toWorld(geobbox[1]);
    const TVec bmin = TVec(min2(bc0.x, bc1.x), min2(bc0.y, bc1.y), min2(-1.0f, min2(bc0.z, bc1.z)));
    const TVec bmax = TVec(max2(bc0.x, bc1.x), max2(bc0.y, bc1.y), min2(-1.0f, max2(bc0.z, bc1.z)));
    if (bmin.x > bbox[0] || bmin.y > bbox[1] || bmin.z > bbox[2] ||
        bmax.x < bbox[3] || bmax.y < bbox[4] || bmax.z < bbox[5])
    {
      GCon->Logf("GEOCLAMP: (%f,%f,%f)-(%f,%f,%f) : (%f,%f,%f)-(%f,%f,%f)", bbox[0], bbox[1], bbox[2], bbox[3], bbox[4], bbox[5], bmin.x, bmin.y, bmin.z, bmax.x, bmax.y, bmax.z);
    }
    bbox[0] = max2(bbox[0], bmin.x);
    bbox[1] = max2(bbox[1], bmin.y);
    bbox[2] = max2(bbox[2], bmin.z);
    bbox[3] = min2(bbox[3], bmax.x);
    bbox[4] = min2(bbox[4], bmax.y);
    bbox[5] = min2(bbox[5], bmax.z);
    if (bbox[0] >= bbox[3+0] || bbox[1] >= bbox[3+1] || bbox[2] >= bbox[3+2]) {
      scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
      currentSVScissor[SCS_MINX] = currentSVScissor[SCS_MINY] = currentSVScissor[SCS_MAXX] = currentSVScissor[SCS_MAXY] = 0;
      glDisable(GL_SCISSOR_TEST);
      return 0;
    }
    */
  }

  // setup depth bounds
  if (hasBoundsTest && gl_enable_depth_bounds) {
    const bool zeroZ = (gl_enable_clip_control && p_glClipControl);
    const bool revZ = CanUseRevZ();

    //const float ofsz0 = min2(-1.0f, inworld.z+radius);
    //const float ofsz1 = inworld.z-radius;
    const float ofsz0 = bbox[5];
    const float ofsz1 = bbox[2];
    vassert(ofsz1 <= -1.0f);

    float pjwz0 = -1.0f/ofsz0;
    float pjwz1 = -1.0f/ofsz1;

    // for reverse z, projz is always 1, so we can simply use pjw
    if (!revZ) {
      pjwz0 *= vpmats.projMat.Transform2OnlyZ(TVec(inworld.x, inworld.y, ofsz0));
      pjwz1 *= vpmats.projMat.Transform2OnlyZ(TVec(inworld.x, inworld.y, ofsz1));
    }

    // transformation for [-1..1] z range
    if (!zeroZ) {
      pjwz0 = (1.0f+pjwz0)*0.5f;
      pjwz1 = (1.0f+pjwz1)*0.5f;
    }

    if (revZ) {
      p_glDepthBounds(pjwz1, pjwz0);
    } else {
      p_glDepthBounds(pjwz0, pjwz1);
    }
    glEnable(GL_DEPTH_BOUNDS_TEST_EXT);
  }

  const int scrx0 = vpmats.vport.x0;
  const int scry0 = vpmats.vport.y0;
  const int scrx1 = vpmats.vport.getX1();
  const int scry1 = vpmats.vport.getY1();

  int minx = scrx1+64, miny = scry1+64;
  int maxx = -(scrx0-64), maxy = -(scry0-64);

  // transform points, get min and max
  for (unsigned f = 0; f < 8; ++f) {
    int winx, winy;
    vpmats.project(TVec(bbox[BBoxVertexIndex[f][0]], bbox[BBoxVertexIndex[f][1]], bbox[BBoxVertexIndex[f][2]]), &winx, &winy);

    if (minx > winx) minx = winx;
    if (miny > winy) miny = winy;
    if (maxx < winx) maxx = winx;
    if (maxy < winy) maxy = winy;
  }

  if (minx > scrx1 || miny > scry1 || maxx < scrx0 || maxy < scry0) {
    scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
    currentSVScissor[SCS_MINX] = currentSVScissor[SCS_MINY] = currentSVScissor[SCS_MAXX] = currentSVScissor[SCS_MAXY] = 0;
    glDisable(GL_SCISSOR_TEST);
    if (hasBoundsTest && gl_enable_depth_bounds) glDisable(GL_DEPTH_BOUNDS_TEST_EXT);
    return 0;
  }

  minx = midval(scrx0, minx, scrx1);
  miny = midval(scry0, miny, scry1);
  maxx = midval(scrx0, maxx, scrx1);
  maxy = midval(scry0, maxy, scry1);

  //GCon->Logf("  radius=%f; (%d,%d)-(%d,%d)", radius, minx, miny, maxx, maxy);
  const int wdt = maxx-minx+1;
  const int hgt = maxy-miny+1;

  // drop very small lights, why not?
  if (wdt <= 4 || hgt <= 4) {
    scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
    currentSVScissor[SCS_MINX] = currentSVScissor[SCS_MINY] = currentSVScissor[SCS_MAXX] = currentSVScissor[SCS_MAXY] = 0;
    glDisable(GL_SCISSOR_TEST);
    if (hasBoundsTest && gl_enable_depth_bounds) glDisable(GL_DEPTH_BOUNDS_TEST_EXT);
    return 0;
  }

  glEnable(GL_SCISSOR_TEST);
  glScissor(minx, miny, wdt, hgt);
  scoord[0] = minx;
  scoord[1] = miny;
  scoord[2] = maxx;
  scoord[3] = maxy;
  currentSVScissor[SCS_MINX] = scoord[0];
  currentSVScissor[SCS_MINY] = scoord[1];
  currentSVScissor[SCS_MAXX] = scoord[2];
  currentSVScissor[SCS_MAXY] = scoord[3];

  return 1;
}


//==========================================================================
//
//  VOpenGLDrawer::ResetScissor
//
//==========================================================================
void VOpenGLDrawer::ResetScissor () {
  glScissor(0, 0, getWidth(), getHeight());
  glDisable(GL_SCISSOR_TEST);
  if (hasBoundsTest) glDisable(GL_DEPTH_BOUNDS_TEST_EXT);
  currentSVScissor[SCS_MINX] = currentSVScissor[SCS_MINY] = 0;
  currentSVScissor[SCS_MAXX] = currentSVScissor[SCS_MAXY] = 32000;
}


//==========================================================================
//
//  FixScissorCoords
//
//  returns `false` if scissor is empty
//
//==========================================================================
static bool FixScissorCoords (int &x, int &y, int &w, int &h, const int ScrWdt, const int ScrHgt) {
  //TODO: proper overflow checks
  if (w == 0 || h == 0) {
    x = y = w = h = 0;
    return true;
  }
  if (w < 1 || h < 1 || x >= ScrWdt || y >= ScrHgt ||
      (x < 0 && x+w <= 0) ||
      (y < 0 && y+h <= 0) ||
      ScrWdt < 1 || ScrHgt < 1)
  {
    x = y = w = h = 0;
    return false;
  }
  if (x < 0) {
    if ((w += x) < 0) { x = y = w = h = 0; return false; }
    x = 0;
  }
  if (y < 0) {
    if ((h += y) < 0) { x = y = w = h = 0; return false; }
    y = 0;
  }
  if (w > ScrWdt) w = ScrWdt;
  if (h > ScrHgt) h = ScrHgt;
  if (x+w > ScrWdt) { if ((w = ScrWdt-x) < 0) { x = y = w = h = 0; return false; } }
  if (y+h > ScrHgt) { if ((h = ScrHgt-y) < 0) { x = y = w = h = 0; return false; } }
  return true;
}


//==========================================================================
//
//  VOpenGLDrawer::UploadCurrentScissorRect
//
//  returns `false` if scissor is empty
//
//==========================================================================
bool VOpenGLDrawer::UploadCurrentScissorRect () {
  int x = scissorX;
  int y = scissorY;
  int w = scissorW;
  int h = scissorH;
  if (!FixScissorCoords(x, y, w, h, ScrWdt, ScrHgt)) return false;
  const int y0 = ScrHgt-(y+h-1);
  //GCon->Logf(NAME_Debug, "scissor: (%d,%d)-(%d,%d)", x, y0, w, h);
  glScissor(x, y0, w, h);
  return true;
}


//==========================================================================
//
//  VOpenGLDrawer::ForceClearScissorState
//
//==========================================================================
void VOpenGLDrawer::ForceClearScissorState () {
  scissorEnabled = false;
  scissorX = scissorY = scissorW = scissorH = 0;
}


//==========================================================================
//
//  VOpenGLDrawer::SetScissorEnabled
//
//==========================================================================
void VOpenGLDrawer::SetScissorEnabled (bool v) {
  if (v == scissorEnabled) return;
  if (v) {
    if (!UploadCurrentScissorRect()) return;
  }
  scissorEnabled = v;
  if (v) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
}


//==========================================================================
//
//  VOpenGLDrawer::IsScissorEnabled
//
//==========================================================================
bool VOpenGLDrawer::IsScissorEnabled () {
  return scissorEnabled;
}


//==========================================================================
//
//  VOpenGLDrawer::GetScissor
//
//==========================================================================
bool VOpenGLDrawer::GetScissor (int *x, int *y, int *w, int *h) {
  if (x) *x = scissorX;
  if (y) *y = scissorY;
  if (w) *w = scissorW;
  if (h) *h = scissorH;
  return scissorEnabled;
}


//==========================================================================
//
//  VOpenGLDrawer::SetScissor
//
//==========================================================================
void VOpenGLDrawer::SetScissor (int x, int y, int w, int h) {
  if (x != scissorX || y != scissorY || w != scissorW || h != scissorH) {
    scissorX = x;
    scissorY = y;
    scissorW = w;
    scissorH = h;
    if (scissorEnabled) {
      if (!UploadCurrentScissorRect()) {
        scissorEnabled = false;
        glDisable(GL_SCISSOR_TEST);
      }
    }
  }
}
