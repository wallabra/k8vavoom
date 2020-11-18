//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
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

// view matrices
class VViewportMats {
public:
  // usually bottom-to-up
  struct Viewport {
  public:
    int x0, y0;
    int width, height;
    float scrwmid, scrhmid; // width/2, height/2
  public:
    // default is "not initialised"
    Viewport () noexcept /*: x0(0), y0(0), width(640), height(480), scrwmid(320), scrhmid(240)*/ {}
    inline void setOrigin (int x, int y) noexcept { x0 = x; y0 = y; }
    inline void setSize (int w, int h) noexcept { width = w; height = h; scrwmid = w*0.5f; scrhmid = h*0.5f; }
    inline bool isValid () const noexcept { return (width > 0 && height > 0); }
    inline int getX1 () const noexcept { return x0+width-1; }
    inline int getY1 () const noexcept { return y0+height-1; }
    inline void getX1Y1 (int *x1, int *y1) const noexcept { *x1 = x0+width-1; *y1 = y0+height-1; }
  };

public:
  VMatrix4 projMat; // projection matrix, can be taken from OpenGL
  VMatrix4 modelMat; // model->world transformation matrix, can be taken from OpenGL
  Viewport vport;

public:
  // default is "not initialised"
  VViewportMats () noexcept : projMat(), modelMat(), vport() {}

  // transform model coords to world coords
  inline TVec toWorld (const TVec &point) const noexcept { return modelMat*point; }

  // transform world coords to viewport projected coords
  // you can get world coords from model coords with `toWorld()`
  // WARNING! `point` should not be behind near z clipping plane
  inline void project (const TVec &point, int *scrx, int *scry) const noexcept {
    TVec proj = projMat.Transform2OnlyXY(point); // we don't care about z here
    const float pjw = -1.0f/point.z;
    proj.x *= pjw;
    proj.y *= pjw;
    *scrx = vport.x0+(int)((1.0f+proj.x)*vport.scrwmid);
    *scry = vport.y0+(int)((1.0f+proj.y)*vport.scrhmid);
  }
};
