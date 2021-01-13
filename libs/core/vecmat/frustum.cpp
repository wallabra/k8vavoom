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
#include "../core.h"

// debug checks
//#define FRUSTUM_BBOX_CHECKS
#define PLANE_BOX_USE_REJECT_ACCEPT


//**************************************************************************
//
// TClipBase
//
//**************************************************************************

//==========================================================================
//
//  TClipBase::CalcFovXY
//
//  WARNING! no checks!
//
//==========================================================================
void TClipBase::CalcFovXY (float *outfovx, float *outfovy, const int width, const int height, const float fov, const float pixelAspect) noexcept {
  const float fovx = tanf(DEG2RADF(fov)/2.0f);
  if (outfovx) *outfovx = fovx;
  if (outfovy) *outfovy = fovx*height/width/pixelAspect;
}


//==========================================================================
//
//  TClipBase::CalcFovXY
//
//  WARNING! no checks!
//
//==========================================================================
void TClipBase::CalcFovXY (float *outfovx, float *outfovy, const TClipParam &cp) noexcept {
  const float fovx = tanf(DEG2RADF(cp.fov)/2.0f);
  if (outfovx) *outfovx = fovx;
  if (outfovy) *outfovy = fovx*cp.height/cp.width/cp.pixelAspect;
}


//==========================================================================
//
//  TClipBase::setupFromCookedFOVs
//
//==========================================================================
void TClipBase::setupFromCookedFOVs (const float afovx, const float afovy) noexcept {
  if (afovx == 0.0f || afovy == 0.0f || !isFiniteF(afovx) || !isFiniteF(afovy)) {
    clear();
  } else {
    fovx = afovx;
    fovy = afovy;
    const float invfovx = 1.0f/afovx;
    const float invfovy = 1.0f/afovy;
    clipbase[0] = TVec(invfovx, 0.0f, 1.0f); // left side clip
    clipbase[1] = TVec(-invfovx, 0.0f, 1.0f); // right side clip
    clipbase[2] = TVec(0.0f, -invfovy, 1.0f); // top side clip
    clipbase[3] = TVec(0.0f, invfovy, 1.0f); // bottom side clip
  }
}


//==========================================================================
//
//  TClipBase::setupFromFOV
//
//  setup from FOV, ignore aspect
//
//==========================================================================
void TClipBase::setupFromFOV (float fov) noexcept {
  if (!isFiniteF(fov)) fov = 90.0f;
  fov = clampval(fov, 0.0001f, 179.90f);
  const float afov = tanf(DEG2RADF(fov)/2.0f);
  const float invafov = 1.0f/afov;
  fovx = fovy = afov;
  clipbase[0] = TVec(invafov, 0.0f, 1.0f); // left side clip
  clipbase[1] = TVec(-invafov, 0.0f, 1.0f); // right side clip
  clipbase[2] = TVec(0.0f, -invafov, 1.0f); // top side clip
  clipbase[3] = TVec(0.0f, invafov, 1.0f); // bottom side clip
}


//==========================================================================
//
//  TClipBase::setupViewport
//
//==========================================================================
void TClipBase::setupViewport (int awidth, int aheight, float afov, float apixelAspect) noexcept {
  if (awidth < 1 || aheight < 1 || afov <= 0.01f || apixelAspect <= 0) {
    clear();
    return;
  }
  float afovx, afovy;
  CalcFovXY(&afovx, &afovy, awidth, aheight, afov, apixelAspect);
  setupFromCookedFOVs(afovx, afovy);
}


//==========================================================================
//
//  TClipBase::setupViewport
//
//==========================================================================
void TClipBase::setupViewport (const TClipParam &cp) noexcept {
  if (!cp.isValid()) {
    clear();
    return;
  }
  float afovx, afovy;
  CalcFovXY(&afovx, &afovy, cp);
  setupFromCookedFOVs(afovx, afovy);
}



//**************************************************************************
//
// TFrustum
//
//**************************************************************************

//==========================================================================
//
//  TFrustum::setup
//
//  `clip_base` is from engine's `SetupFrame()` or `SetupCameraFrame()`
//
//==========================================================================
void TFrustum::setup (const TClipBase &clipbase, const TFrustumParam &fp, bool createbackplane, const float farplanez) noexcept {
  clear();
  if (!clipbase.isValid() || !fp.isValid()) return;
  planeCount = 4; // anyway
  // create side planes
  for (unsigned i = 0; i < 4; ++i) {
    const TVec &v = clipbase.clipbase[i];
    // v.z is always 1.0f
    const TVec v2(
      VSUM3(v.x*fp.vright.x, v.y*fp.vup.x, /*v.z* */fp.vforward.x),
      VSUM3(v.x*fp.vright.y, v.y*fp.vup.y, /*v.z* */fp.vforward.y),
      VSUM3(v.x*fp.vright.z, v.y*fp.vup.z, /*v.z* */fp.vforward.z));
    planes[i].SetPointNormal3D(fp.origin, v2.normalised());
    planes[i].clipflag = 1U<<i;
  }
  // create back plane
  if (createbackplane) {
    // move back plane forward a little; our `zNear` is `1.0f`, so just use it
    // k8: better be safe than sorry
    planes[4].SetPointNormal3D(fp.origin+fp.vforward*0.125f, fp.vforward);
    planes[4].clipflag = 1U<<4;
    planeCount = 5;
  } else {
    planes[4].clipflag = 0;
  }
  // create far plane
  if (isFiniteF(farplanez) && farplanez >= 0.2f) {
    planes[5].SetPointNormal3D(fp.origin+fp.vforward*farplanez, -fp.vforward);
    planes[5].clipflag = 1U<<5;
    planeCount = 6;
  } else {
    planes[5].clipflag = 0;
  }
}


//==========================================================================
//
//  TFrustum::setupSimpleAngles
//
//  WARNING! no checks!
//  ignores aspect
//  if `farplanez` is non finite, or <= 0, do not set far plane
//
//==========================================================================
void TFrustum::setupSimpleAngles (const TVec &org, const TAVec &angles, const float fov, const float farplanez) noexcept {
  TFrustumParam fp(org, angles);
  TClipBase cb(fov);
  setup(cb, fp, true, farplanez);
  planes[Near].SetPointNormal3D(fp.origin, fp.vforward);
}


//==========================================================================
//
//  TFrustum::setupSimpleDir
//
//  WARNING! no checks!
//  ignores aspect
//  if `farplanez` is non finite, or <= 0, do not set far plane
//
//==========================================================================
void TFrustum::setupSimpleDir (const TVec &org, const TVec &dir, const float fov, const float farplanez) noexcept {
  const TAVec angles = VectorAngles(dir);
  setupSimpleAngles(org, angles, fov, farplanez);
}


//==========================================================================
//
//  TFrustum::checkPoint
//
//==========================================================================
void TFrustum::setFarPlane (const TFrustumParam &fp, float farplanez) noexcept {
  // create far plane
  if (isFiniteF(farplanez) && farplanez >= 0.2f) {
    planes[5].SetPointNormal3D(fp.origin+fp.vforward*farplanez, -fp.vforward);
    planes[5].clipflag = 1U<<5;
    planeCount = 6;
  } else {
    planes[5].clipflag = 0;
    if (planeCount >= 6) planeCount = 5;
  }
}


//==========================================================================
//
//  TFrustum::checkPoint
//
//  returns `false` if point is out of frustum (or frustum is not valid)
//
//==========================================================================
bool TFrustum::checkPoint (const TVec &point, const unsigned mask) const noexcept {
  if (!(mask|planeCount)) return true;
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!(cp->clipflag&mask)) continue; // don't need to clip against it
    if (cp->PointOnSide(point)) return false; // viewer is in back side or on plane
  }
  return true;
}


//==========================================================================
//
//  TFrustum::checkSphere
//
//  returns `false` is sphere is out of frustum (or frustum is not valid)
//  note that this can give us false positives, see
//  https://stackoverflow.com/questions/37512308/
//
//==========================================================================
bool TFrustum::checkSphere (const TVec &center, const float radius, const unsigned mask) const noexcept {
  if (!(mask|planeCount)) return true;
  if (radius <= 0) return checkPoint(center, mask);
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!(cp->clipflag&mask)) continue; // don't need to clip against it
    if (cp->SphereOnSide(center, radius)) {
      // on a back side (or on a plane)
      return false;
    }
  }
  return true;
}


//==========================================================================
//
//  TFrustum::checkBox
//
//  returns `false` is box is out of frustum (or frustum is not valid)
//  bbox:
//    [0] is minx
//    [1] is miny
//    [2] is minz
//    [3] is maxx
//    [4] is maxy
//    [5] is maxz
//
//==========================================================================
bool TFrustum::checkBox (const float bbox[6], const unsigned mask) const noexcept {
  if (!(mask|planeCount)) return true;
#ifdef FRUSTUM_BBOX_CHECKS
  vassert(bbox[0] <= bbox[3+0]);
  vassert(bbox[1] <= bbox[3+1]);
  vassert(bbox[2] <= bbox[3+2]);
#endif
#ifndef PLANE_BOX_USE_REJECT_ACCEPT
  CONST_BBoxVertexIndex;
#endif
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!(cp->clipflag&mask)) continue; // don't need to clip against it
#ifdef PLANE_BOX_USE_REJECT_ACCEPT
    if (cp->PointDistance(cp->get3DBBoxRejectPoint(bbox)) <= 0.0f) return false; // on a back side?
#else
    bool passed = false;
    for (unsigned j = 0; j < 8; ++j) {
      if (!cp->PointOnSide(TVec(bbox[BBoxVertexIndex[j][0]], bbox[BBoxVertexIndex[j][1]], bbox[BBoxVertexIndex[j][2]]))) {
        passed = true;
        break;
      }
    }
    if (!passed) return false;
#endif
  }
  return true;
}


//==========================================================================
//
//  TFrustum::checkBoxEx
//
//  0: completely outside; >0: completely inside; <0: partially inside
//  note that this won't work for big boxes: we need to do more checks, see
//  http://iquilezles.org/www/articles/frustumcorrect/frustumcorrect.htm
//
//==========================================================================
int TFrustum::checkBoxEx (const float bbox[6], const unsigned mask) const noexcept {
  if (!planeCount || !mask) return INSIDE;
#ifdef FRUSTUM_BBOX_CHECKS
  vassert(bbox[0] <= bbox[3+0]);
  vassert(bbox[1] <= bbox[3+1]);
  vassert(bbox[2] <= bbox[3+2]);
#endif
#ifndef PLANE_BOX_USE_REJECT_ACCEPT
  CONST_BBoxVertexIndex;
#endif
  int res = INSIDE; // assume that the aabb will be inside the frustum
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!(cp->clipflag&mask)) continue; // don't need to clip against it
#ifdef PLANE_BOX_USE_REJECT_ACCEPT
    // check reject point
    if (cp->PointDistance(cp->get3DBBoxRejectPoint(bbox)) <= 0.0f) return OUTSIDE; // entire box on a back side
    // if the box is already determined to be partially inside, there is no need to check accept point anymore
    if (res == INSIDE) {
      // check accept point
      if (cp->PointDistance(cp->get3DBBoxAcceptPoint(bbox)) <= 0.0f) res = PARTIALLY;
    }
#else
    if (res == INSIDE) {
      unsigned passed = 0;
      for (unsigned j = 0; j < 8; ++j) {
        if (!cp->PointOnSide(TVec(bbox[BBoxVertexIndex[j][0]], bbox[BBoxVertexIndex[j][1]], bbox[BBoxVertexIndex[j][2]]))) {
          ++passed;
        }
      }
      if (!passed) return OUTSIDE;
      if (passed != 8) res = PARTIALLY;
    } else {
      // partially
      bool passed = false;
      for (unsigned j = 0; j < 8; ++j) {
        if (!cp->PointOnSide(TVec(bbox[BBoxVertexIndex[j][0]], bbox[BBoxVertexIndex[j][1]], bbox[BBoxVertexIndex[j][2]]))) {
          passed = true;
          break;
        }
      }
      if (!passed) return OUTSIDE;
    }
#endif
  }
  return res;
}


//==========================================================================
//
//  TFrustum::checkTriangle
//
//==========================================================================
bool TFrustum::checkTriangle (const TVec &v1, const TVec &v2, const TVec &v3, const unsigned mask) const noexcept {
  if (!(mask|planeCount)) return true;
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!(cp->clipflag&mask)) continue; // don't need to clip against it
    // if everything is on the back side, we're done here
    if (cp->PointOnSide(v1) && cp->PointOnSide(v2) && cp->PointOnSide(v3)) return false;
  }
  return true;
}


//==========================================================================
//
//  TFrustum::checkTriangleEx
//
//==========================================================================
int TFrustum::checkTriangleEx (const TVec &v1, const TVec &v2, const TVec &v3, const unsigned mask) const noexcept {
  if (!planeCount || !mask) return INSIDE;
  int res = INSIDE;
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!(cp->clipflag&mask)) continue; // don't need to clip against it
    const bool d1negz = (cp->PointDistance(v1) <= 0.0f);
    const bool d2negz = (cp->PointDistance(v2) <= 0.0f);
    const bool d3negz = (cp->PointDistance(v3) <= 0.0f);
    // if everything is on the back side, we're done here
    if (d1negz && d2negz && d3negz) return OUTSIDE;
    // if we're already hit "partial" case, no need to perform further checks
    if (res == INSIDE) {
      if (d1negz || d2negz || d3negz) res = PARTIALLY; // both sides touched
    }
  }
  return res;
}


//==========================================================================
//
//  TFrustum::checkPolyInterlaced
//
//==========================================================================
bool TFrustum::checkPolyInterlaced (const TVec *data, size_t memberSize, const unsigned count, const unsigned mask) const noexcept {
  if (!data || count < 3) return false;
  if (!(mask|planeCount)) return true;
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!(cp->clipflag&mask)) continue; // don't need to clip against it
    bool passed = false;
    const uint8_t *vv = (const uint8_t *)data;
    for (unsigned j = count; j--; vv += memberSize) {
      if (!cp->PointOnSide(*(const TVec *)vv)) {
        passed = true;
        break;
      }
    }
    if (!passed) return false;
  }
  return true;
}



//==========================================================================
//
//  TFrustum::checkQuad
//
//==========================================================================
bool TFrustum::checkQuad (const TVec &v1, const TVec &v2, const TVec &v3, const TVec &v4, const unsigned mask) const noexcept {
  if (!(mask|planeCount)) return true;
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!(cp->clipflag&mask)) continue; // don't need to clip against it
    // if everything is on the back side, we're done here
    if (cp->PointOnSide(v1) && cp->PointOnSide(v2) && cp->PointOnSide(v3) && cp->PointOnSide(v4)) return false;
  }
  return true;
}


//==========================================================================
//
//  TFrustum::checkQuadEx
//
//==========================================================================
int TFrustum::checkQuadEx (const TVec &v1, const TVec &v2, const TVec &v3, const TVec &v4, const unsigned mask) const noexcept {
  if (!planeCount || !mask) return INSIDE;
  int res = INSIDE;
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!(cp->clipflag&mask)) continue; // don't need to clip against it
    const bool d1negz = (cp->PointDistance(v1) <= 0.0f);
    const bool d2negz = (cp->PointDistance(v2) <= 0.0f);
    const bool d3negz = (cp->PointDistance(v3) <= 0.0f);
    const bool d4negz = (cp->PointDistance(v4) <= 0.0f);
    // if everything is on the back side, we're done here
    if (d1negz && d2negz && d3negz && d4negz) return OUTSIDE;
    // if we're already hit "partial" case, no need to perform further checks
    if (res == INSIDE) {
      if (d1negz || d2negz || d3negz || d4negz) res = PARTIALLY; // both sides touched
    }
  }
  return res;
}



//==========================================================================
//
//  BoxOnLineSide2D
//
//  check the relationship between the given box and the partition
//  line.  Returns -1 if box is on left side, +1 if box is on right
//  size, or 0 if the line intersects the box.
//
//==========================================================================
VVA_CHECKRESULT int BoxOnLineSide2D (const float tmbox[4], TVec v1, TVec v2) noexcept {
  v1.z = v2.z = 0;
  TVec dir = v2-v1;

  int p1, p2;

  if (!dir.y) {
    // horizontal
    p1 = (tmbox[BOX2D_TOP] > v1.y);
    p2 = (tmbox[BOX2D_BOTTOM] > v1.y);
    if (dir.x < 0) {
      p1 ^= 1;
      p2 ^= 1;
    }
  } else if (!dir.x) {
    // vertical
    p1 = (tmbox[BOX2D_RIGHT] < v1.x);
    p2 = (tmbox[BOX2D_LEFT] < v1.x);
    if (dir.y < 0) {
      p1 ^= 1;
      p2 ^= 1;
    }
  } else if (dir.y*dir.x >= 0.0f) {
    TPlane lpl;
    lpl.SetPointDirXY(v1, dir);
    // positive
    p1 = lpl.PointOnSide(TVec(tmbox[BOX2D_LEFT], tmbox[BOX2D_TOP], 0));
    p2 = lpl.PointOnSide(TVec(tmbox[BOX2D_RIGHT], tmbox[BOX2D_BOTTOM], 0));
  } else {
    // negative
    TPlane lpl;
    lpl.SetPointDirXY(v1, dir);
    p1 = lpl.PointOnSide(TVec(tmbox[BOX2D_RIGHT], tmbox[BOX2D_TOP], 0));
    p2 = lpl.PointOnSide(TVec(tmbox[BOX2D_LEFT], tmbox[BOX2D_BOTTOM], 0));
  }

  return (p1 == p2 ? p1 : -1);
}


//==========================================================================
//
//  FixBBoxZ
//
//==========================================================================
void FixBBoxZ (float bbox[6]) noexcept {
  vassert(isFiniteF(bbox[2]));
  vassert(isFiniteF(bbox[3+2]));
  if (bbox[2] > bbox[3+2]) {
    const float tmp = bbox[2];
    bbox[2] = bbox[3+2];
    bbox[3+2] = tmp;
  }
}


//==========================================================================
//
//  SanitizeBBox3D
//
//  make sure that bbox min is lesser than bbox max
//  UB if bbox coords are not finite (no checks!)
//
//==========================================================================
void SanitizeBBox3D (float bbox[6]) noexcept {
  if (bbox[BOX3D_MINX] > bbox[BOX3D_MAXX]) {
    const float tmp = bbox[BOX3D_MINX];
    bbox[BOX3D_MINX] = bbox[BOX3D_MAXX];
    bbox[BOX3D_MAXX] = tmp;
  }
  if (bbox[BOX3D_MINY] > bbox[BOX3D_MAXY]) {
    const float tmp = bbox[BOX3D_MINY];
    bbox[BOX3D_MINY] = bbox[BOX3D_MAXY];
    bbox[BOX3D_MAXY] = tmp;
  }
  if (bbox[BOX3D_MINZ] > bbox[BOX3D_MAXZ]) {
    const float tmp = bbox[BOX3D_MINZ];
    bbox[BOX3D_MINZ] = bbox[BOX3D_MAXZ];
    bbox[BOX3D_MAXZ] = tmp;
  }
}


//==========================================================================
//
//  CheckSphereVsAABB
//
//  check to see if the sphere overlaps the AABB
//
//==========================================================================
VVA_CHECKRESULT bool CheckSphereVsAABB (const float bbox[6], const TVec &lorg, const float radius) noexcept {
  float d = 0.0f;
  // find the square of the distance from the sphere to the box
  /*
  for (unsigned i = 0; i < 3; ++i) {
    const float li = lorg[i];
    // first check is min, second check is max
    if (li < bbox[i]) {
      const float s = li-bbox[i];
      d += s*s;
    } else if (li > bbox[i+3]) {
      const float s = li-bbox[i+3];
      d += s*s;
    }
  }
  */
  float s;
  const float *li = &lorg[0];

  s = (*li < bbox[0] ? (*li)-bbox[0] : *li > bbox[0+3] ? (*li)-bbox[0+3] : 0.0f);
  d += s*s;
  ++li;
  ++bbox;
  s = (*li < bbox[0] ? (*li)-bbox[0] : *li > bbox[0+3] ? (*li)-bbox[0+3] : 0.0f);
  d += s*s;
  ++li;
  ++bbox;
  s = (*li < bbox[0] ? (*li)-bbox[0] : *li > bbox[0+3] ? (*li)-bbox[0+3] : 0.0f);
  d += s*s;

  return (d < radius*radius); // or <= if you want exact touching
}


//==========================================================================
//
//  CheckSphereVsAABB
//
//  check to see if the sphere overlaps the AABB (ignore z coords)
//
//==========================================================================
VVA_CHECKRESULT bool CheckSphereVsAABBIgnoreZ (const float bbox[6], const TVec &lorg, const float radius) noexcept {
  float d = 0.0f, s;
  // find the square of the distance from the sphere to the box
  // first check is min, second check is max
  const float *li = &lorg[0];

  s = (*li < bbox[0] ? (*li)-bbox[0] : *li > bbox[0+3] ? (*li)-bbox[0+3] : 0.0f);
  d += s*s;
  ++li;
  ++bbox;
  s = (*li < bbox[0] ? (*li)-bbox[0] : *li > bbox[0+3] ? (*li)-bbox[0+3] : 0.0f);
  d += s*s;

  return (d < radius*radius); // or <= if you want exact touching
}


//==========================================================================
//
//  CheckSphereVsAABB
//
//  check to see if the sphere overlaps the 2D AABB
//
//==========================================================================
VVA_CHECKRESULT bool CheckSphereVs2dAABB (const float bbox[4], const TVec &lorg, const float radius) noexcept {
  float d = 0.0f, s;
  // find the square of the distance from the sphere to the box
  // first check is min, second check is max
  const float *li = &lorg[0];

  s = (*li < bbox[BOX2D_LEFT] ? (*li)-bbox[BOX2D_LEFT] : *li > bbox[BOX2D_RIGHT] ? (*li)-bbox[BOX2D_RIGHT] : 0.0f);
  d += s*s;
  ++li;
  s = (*li < bbox[BOX2D_BOTTOM] ? (*li)-bbox[BOX2D_BOTTOM] : *li > bbox[BOX2D_TOP] ? (*li)-bbox[BOX2D_TOP] : 0.0f);
  d += s*s;

  return (d < radius*radius); // or <= if you want exact touching
}


//==========================================================================
//
//  IsCircleTouchBox2D
//
//==========================================================================
VVA_CHECKRESULT bool IsCircleTouchBox2D (const float cx, const float cy, float radius, const float bbox2d[4]) noexcept {
  if (radius < 1.0f) return false;

  const float bbwHalf = (bbox2d[BOX2D_RIGHT]+bbox2d[BOX2D_LEFT])*0.5f;
  const float bbhHalf = (bbox2d[BOX2D_TOP]+bbox2d[BOX2D_BOTTOM])*0.5f;

  // the distance between the center of the circle and the center of the box
  // not a const, because we'll modify the variables later
  float cdistx = fabsf(cx-(bbox2d[BOX2D_LEFT]+bbwHalf));
  float cdisty = fabsf(cy-(bbox2d[BOX2D_BOTTOM]+bbhHalf));

  // easy cases: either completely outside, or completely inside
  if (cdistx > bbwHalf+radius || cdisty > bbhHalf+radius) return false;
  if (cdistx <= bbwHalf || cdisty <= bbhHalf) return true;

  // hard case: touching a corner
  cdistx -= bbwHalf;
  cdisty -= bbhHalf;
  const float cdistsq = cdistx*cdistx+cdisty*cdisty;
  return (cdistsq <= radius*radius);
}
