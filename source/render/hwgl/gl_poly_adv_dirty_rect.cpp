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
// directly included from "gl_poly_adv.cpp"
//**************************************************************************

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
