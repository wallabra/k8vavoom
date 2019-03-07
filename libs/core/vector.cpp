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
#include "core.h"

#define FRUSTUM_BBOX_CHECKS

//==========================================================================
//
//  AngleVectors
//
//==========================================================================
void AngleVectors (const TAVec &angles, TVec &forward, TVec &right, TVec &up) {
#if 0
  const double ay = DEG2RADD(angles.yaw);
  const double ap = DEG2RADD(angles.pitch);
  const double ar = DEG2RADD(angles.roll);

  const double sy = sin(ay);
  const double cy = cos(ay);
  const double sp = sin(ap);
  const double cp = cos(ap);
  const double sr = sin(ar);
  const double cr = cos(ar);
#else
  const float ay = DEG2RADF(angles.yaw);
  const float ap = DEG2RADF(angles.pitch);
  const float ar = DEG2RADF(angles.roll);

  const float sy = sinf(ay);
  const float cy = cosf(ay);
  const float sp = sinf(ap);
  const float cp = cosf(ap);
  const float sr = sinf(ar);
  const float cr = cosf(ar);
#endif

  forward.x = cp*cy;
  forward.y = cp*sy;
  forward.z = -sp;
  right.x = VSUM2(-sr*sp*cy, cr*sy);
  right.y = VSUM2(-sr*sp*sy, -(cr*cy));
  right.z = -sr*cp;
  up.x = VSUM2(cr*sp*cy, sr*sy);
  up.y = VSUM2(cr*sp*sy, -(sr*cy));
  up.z = cr*cp;
}


//==========================================================================
//
//  AngleVector
//
//==========================================================================
void AngleVector (const TAVec &angles, TVec &forward) {
  const float ay = DEG2RADF(angles.yaw);
  const float ap = DEG2RADF(angles.pitch);

  const float sy = sinf(ay);
  const float cy = cosf(ay);
  const float sp = sinf(ap);
  const float cp = cosf(ap);

  forward.x = cp*cy;
  forward.y = cp*sy;
  forward.z = -sp;
}


//==========================================================================
//
//  VectorAngles
//
//==========================================================================
void VectorAngles (const TVec &vec, TAVec &angles) {
  const float fx = vec.x;
  const float fy = vec.y;
  const float len2d = VSUM2(fx*fx, fy*fy);
  if (len2d < 0.0001f) {
    angles.pitch = (vec.z > 0 ? 90 : 270);
    angles.yaw = 0;
  } else {
    angles.pitch = -matan(vec.z, sqrtf(len2d));
    angles.yaw = matan(fy, fx);
  }
  angles.roll = 0;
}


//==========================================================================
//
//  VectorsAngles
//
//==========================================================================
void VectorsAngles (const TVec &forward, const TVec &right, const TVec &up, TAVec &angles) {
  const float fx = forward.x;
  const float fy = forward.y;
  float len2d = VSUM2(fx*fx, fy*fy);
  if (len2d < 0.0001f) {
    angles.yaw = 0;
    if (forward.z > 0) {
      angles.pitch = 90;
      angles.roll = matan(-up.y, -up.x);
    } else {
      angles.pitch = 270;
      angles.roll = matan(-up.y, up.x);
    }
  } else {
    len2d = sqrtf(len2d);
    angles.pitch = matan(-forward.z, len2d); // up/down
    angles.yaw = matan(fy, fx); // left/right
    angles.roll = (right.z || up.z ? matan(-right.z/len2d, up.z/len2d) : 0);
  }
}


//==========================================================================
//
//  RotateVectorAroundVector
//
//==========================================================================
TVec RotateVectorAroundVector (const TVec &Vector, const TVec &Axis, const float Angle) {
  VRotMatrix M(Axis, Angle);
  return Vector*M;
}


//==========================================================================
//
//  PerpendicularVector
//
//  assumes "src" is normalised
//
//==========================================================================
void PerpendicularVector (TVec &dst, const TVec &src) {
  unsigned pos;
  unsigned i;
  float minelem = 1.0f;
  TVec tempvec;

  // find the smallest magnitude axially aligned vector
  for (pos = 0, i = 0; i < 3; ++i) {
    if (fabsf(src[i]) < minelem) {
      pos = i;
      minelem = fabsf(src[i]);
    }
  }
  tempvec[0] = tempvec[1] = tempvec[2] = 0.0f;
  tempvec[pos] = 1.0f;

  // project the point onto the plane defined by src
  if (ProjectPointOnPlane(dst, tempvec, src)) {
    // normalise the result
    //dst = NormaliseSafe(dst);
    dst.normaliseInPlace();
  }
}



//==========================================================================
//
//  TClipBase::setupFromFOVs
//
//==========================================================================
void TClipBase::setupFromFOVs (const float afovx, const float afovy) {
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
//  TClipBase::setupViewport
//
//==========================================================================
void TClipBase::setupViewport (int awidth, int aheight, float afov, float apixelAspect) {
  if (awidth < 1 || aheight < 1 || afov <= 0.01f || apixelAspect <= 0) {
    clear();
    return;
  }
  float afovx, afovy;
  CalcFovXY(&afovx, &afovy, awidth, aheight, afov, apixelAspect);
  setupFromFOVs(afovx, afovy);
}


//==========================================================================
//
//  TClipBase::setupViewport
//
//==========================================================================
void TClipBase::setupViewport (const TClipParam &cp) {
  if (!cp.isValid()) {
    clear();
    return;
  }
  float afovx, afovy;
  CalcFovXY(&afovx, &afovy, cp);
  setupFromFOVs(afovx, afovy);
}



//==========================================================================
//
//  TClipPlane::setupBoxIndicies
//
//  setup indicies for box checking
//
//==========================================================================
void TClipPlane::setupBoxIndicies () {
  if (!clipflag) return;
  for (unsigned j = 0; j < 3; ++j) {
    if (normal[j] < 0) {
      pindex[j] = j;
      pindex[j+3] = j+3;
    } else {
      pindex[j] = j+3;
      pindex[j+3] = j;
    }
  }
}


//==========================================================================
//
//  TClipPlane::checkBox
//
//  returns `false` is box is on the back of the plane (or clipflag is 0)
//  bbox:
//    [0] is minx
//    [1] is miny
//    [2] is minz
//    [3] is maxx
//    [4] is maxy
//    [5] is maxz
//
//==========================================================================
bool TClipPlane::checkBox (const float bbox[6]) const {
  if (!clipflag) return true; // don't need to clip against it
#ifdef FRUSTUM_BBOX_CHECKS
  check(bbox[0] <= bbox[3+0]);
  check(bbox[1] <= bbox[3+1]);
  check(bbox[2] <= bbox[3+2]);
#endif
  // check reject point
  return !PointOnBackTh(TVec(bbox[pindex[0]], bbox[pindex[1]], bbox[pindex[2]]));
}


//==========================================================================
//
//  TClipPlane::checkBoxEx
//
//  0: completely outside; >0: completely inside; <0: partially inside
//
//==========================================================================
int TClipPlane::checkBoxEx (const float bbox[6]) const {
  if (!clipflag) return 1; // don't need to clip against it
#ifdef FRUSTUM_BBOX_CHECKS
  check(bbox[0] <= bbox[3+0]);
  check(bbox[1] <= bbox[3+1]);
  check(bbox[2] <= bbox[3+2]);
#endif
  // check reject point
  if (PointOnBackTh(TVec(bbox[pindex[0]], bbox[pindex[1]], bbox[pindex[2]]))) return TFrustum::OUTSIDE; // completely outside
  // check accept point
  return (PointOnBackTh(TVec(bbox[pindex[3+0]], bbox[pindex[3+1]], bbox[pindex[3+2]])) ? TFrustum::PARTIALLY : TFrustum::INSIDE);
}



//==========================================================================
//
//  TFrustum::setupBoxIndicies
//
//  setup indicies for box checking
//
//==========================================================================
void TFrustum::setupBoxIndicies () {
  for (unsigned i = 0; i < 6; ++i) {
    if (!planes[i].clipflag) continue;
    setupBoxIndiciesForPlane(i);
  }
}


//==========================================================================
//
//  TFrustum::setupBoxIndicies
//
//==========================================================================
void TFrustum::setupBoxIndiciesForPlane (unsigned pidx) {
  if (pidx < 6 && planes[pidx].clipflag) {
    planes[pidx].setupBoxIndicies();
  }
}


//==========================================================================
//
//  TFrustum::setup
//
//  `clip_base` is from engine's `SetupFrame()` or `SetupCameraFrame()`
//
//==========================================================================
void TFrustum::setup (const TClipBase &clipbase, const TFrustumParam &fp, bool createbackplane, const float farplanez) {
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
    planes[4].SetPointNormal3D(fp.origin, fp.vforward);
    planes[4].clipflag = 1U<<4;
    planeCount = 5;
  } else {
    planes[4].clipflag = 0;
  }
  // create far plane
  if (isFiniteF(farplanez) && farplanez > 0) {
    planes[5].SetPointNormal3D(fp.origin+fp.vforward*farplanez, -fp.vforward);
    planes[5].clipflag = 1U<<5;
    planeCount = 6;
  } else {
    planes[5].clipflag = 0;
  }
  setupBoxIndicies();
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
bool TFrustum::checkBox (const float bbox[6]) const {
  if (!planeCount) return true;
#ifdef FRUSTUM_BBOX_CHECKS
  check(bbox[0] <= bbox[3+0]);
  check(bbox[1] <= bbox[3+1]);
  check(bbox[2] <= bbox[3+2]);
#endif
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!cp->clipflag) continue; // don't need to clip against it
    // check reject point
    if (cp->PointOnBackTh(TVec(bbox[cp->pindex[0]], bbox[cp->pindex[1]], bbox[cp->pindex[2]]))) {
      // on a back side (or on a plane)
      return false;
    }
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
int TFrustum::checkBoxEx (const float bbox[6]) const {
  if (!planeCount) return INSIDE;
#ifdef FRUSTUM_BBOX_CHECKS
  check(bbox[0] <= bbox[3+0]);
  check(bbox[1] <= bbox[3+1]);
  check(bbox[2] <= bbox[3+2]);
#endif
  int res = INSIDE; // assume that the aabb will be inside the frustum
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!cp->clipflag) continue; // don't need to clip against it
    // check reject point
    if (cp->PointOnBackTh(TVec(bbox[cp->pindex[0]], bbox[cp->pindex[1]], bbox[cp->pindex[2]]))) {
      // on a back side (or on a plane)
      check(cp->PointOnBackTh(TVec(bbox[cp->pindex[3+0]], bbox[cp->pindex[3+1]], bbox[cp->pindex[3+2]])));
      return OUTSIDE;
    }
    if (res == INSIDE) {
      // check accept point
      if (cp->PointOnBackTh(TVec(bbox[cp->pindex[3+0]], bbox[cp->pindex[3+1]], bbox[cp->pindex[3+2]]))) res = PARTIALLY;
    }
  }
  return res;
}


//==========================================================================
//
//  TFrustum::checkPoint
//
//  returns `false` is sphere is out of frustum (or frustum is not valid)
//
//==========================================================================
bool TFrustum::checkPoint (const TVec &point) const {
  if (!planeCount) return true;
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!cp->clipflag) continue; // don't need to clip against it
    if (cp->PointOnBackTh(point)) return false; // viewer is in back side or on plane
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
bool TFrustum::checkSphere (const TVec &center, const float radius) const {
  if (!planeCount) return true;
  if (radius <= 0) return checkPoint(center);
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!cp->clipflag) continue; // don't need to clip against it
    if (cp->SphereOnBackTh(center, radius)) {
      // on a back side (or on a plane)
      return false;
    }
  }
  return true;
}


//==========================================================================
//
//  TFrustum::checkBoxBack
//
//==========================================================================
bool TFrustum::checkBoxBack (const float bbox[6]) const {
  if (planeCount < 5) return true;
  const TClipPlane *cp = &planes[Near];
  if (!cp->clipflag) return true; // don't need to clip against it
#ifdef FRUSTUM_BBOX_CHECKS
  check(bbox[0] <= bbox[3+0]);
  check(bbox[1] <= bbox[3+1]);
  check(bbox[2] <= bbox[3+2]);
#endif
  // check reject point
  if (cp->PointOnBackTh(TVec(bbox[cp->pindex[0]], bbox[cp->pindex[1]], bbox[cp->pindex[2]]))) {
    check(cp->PointOnBackTh(TVec(bbox[cp->pindex[3+0]], bbox[cp->pindex[3+1]], bbox[cp->pindex[3+2]])));
    return false;
  }
  return true;
}


//==========================================================================
//
//  TFrustum::checkBoxExBack
//
//==========================================================================
int TFrustum::checkBoxExBack (const float bbox[6]) const {
  if (planeCount < 5) return INSIDE;
  const TClipPlane *cp = &planes[Near];
  if (!cp->clipflag) return true; // don't need to clip against it
#ifdef FRUSTUM_BBOX_CHECKS
  check(bbox[0] <= bbox[3+0]);
  check(bbox[1] <= bbox[3+1]);
  check(bbox[2] <= bbox[3+2]);
#endif
  // check reject point
  if (cp->PointOnBackTh(TVec(bbox[cp->pindex[0]], bbox[cp->pindex[1]], bbox[cp->pindex[2]]))) {
    // on a back side (or on a plane)
    if (!cp->PointOnBackTh(TVec(bbox[cp->pindex[3+0]], bbox[cp->pindex[3+1]], bbox[cp->pindex[3+2]]))) {
      GLog.Logf("plane:(%f,%f,%f) : %f (%d,%d,%d)-(%d,%d,%d); bbox:(%f,%f,%f)-(%f,%f,%f) (%f : %f)",
        cp->normal.x, cp->normal.y, cp->normal.z, cp->dist,
        cp->pindex[0], cp->pindex[1], cp->pindex[2],
        cp->pindex[3], cp->pindex[4], cp->pindex[5],
        bbox[0], bbox[1], bbox[2], bbox[3], bbox[4], bbox[5],
        DotProduct(TVec(bbox[cp->pindex[0]], bbox[cp->pindex[1]], bbox[cp->pindex[2]]), cp->normal)-cp->dist,
        DotProduct(TVec(bbox[cp->pindex[3+0]], bbox[cp->pindex[3+1]], bbox[cp->pindex[3+2]]), cp->normal)-cp->dist);
      abort();
    }
    //check(cp->PointOnBackTh(TVec(bbox[cp->pindex[3+0]], bbox[cp->pindex[3+1]], bbox[cp->pindex[3+2]])));
    return OUTSIDE;
  }
  // check accept point
  return (cp->PointOnBackTh(TVec(bbox[cp->pindex[3+0]], bbox[cp->pindex[3+1]], bbox[cp->pindex[3+2]])) ? PARTIALLY : INSIDE);
}


//==========================================================================
//
//  TFrustum::checkPointBack
//
//==========================================================================
bool TFrustum::checkPointBack (const TVec &point) const {
  if (planeCount < 5) return true;
  const TClipPlane *cp = &planes[Near];
  if (!cp->clipflag) return true; // don't need to clip against it
  return !cp->PointOnBackTh(point);
}


//==========================================================================
//
//  TFrustum::checkSphereBack
//
//==========================================================================
bool TFrustum::checkSphereBack (const TVec &center, const float radius) const {
  if (planeCount < 5) return true;
  const TClipPlane *cp = &planes[Near];
  if (!cp->clipflag) return true; // don't need to clip against it
  return !(radius > 0 ? cp->SphereOnBackTh(center, radius) : cp->PointOnBackTh(center));
}
