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


//==========================================================================
//
//  AngleVectors
//
//==========================================================================
void AngleVectors (const TAVec &angles, TVec &forward, TVec &right, TVec &up) {
  const double ay = DEG2RADD(angles.yaw);
  const double ap = DEG2RADD(angles.pitch);
  const double ar = DEG2RADD(angles.roll);

  const double sy = sin(ay);
  const double cy = cos(ay);
  const double sp = sin(ap);
  const double cp = cos(ap);
  const double sr = sin(ar);
  const double cr = cos(ar);

  forward.x = cp*cy;
  forward.y = cp*sy;
  forward.z = -sp;
#ifdef USE_NEUMAIER_KAHAN
  right.x = neumsum2(-sr*sp*cy, cr*sy);
  right.y = neumsum2(-sr*sp*sy, -(cr*cy));
#else /* USE_NEUMAIER_KAHAN */
  right.x = -sr*sp*cy+cr*sy;
  right.y = -sr*sp*sy-cr*cy;
#endif /* USE_NEUMAIER_KAHAN */
  right.z = -sr*cp;
#ifdef USE_NEUMAIER_KAHAN
  up.x = neumsum2(cr*sp*cy, sr*sy);
  up.y = neumsum2(cr*sp*sy, -(sr*cy));
#else /* USE_NEUMAIER_KAHAN */
  up.x = cr*sp*cy+sr*sy;
  up.y = cr*sp*sy-sr*cy;
#endif /* USE_NEUMAIER_KAHAN */
  up.z = cr*cp;
}


//==========================================================================
//
//  AngleVector
//
//==========================================================================
void AngleVector (const TAVec &angles, TVec &forward) {
  const float sy = msin(angles.yaw);
  const float cy = mcos(angles.yaw);
  const float sp = msin(angles.pitch);
  const float cp = mcos(angles.pitch);

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
#if 0
  const double fx = vec.x;
  const double fy = vec.y;
#ifdef USE_NEUMAIER_KAHAN
  const double len2d = sqrt(neumsum2D(fx*fx, fy*fy));
#else /* USE_NEUMAIER_KAHAN */
  const double len2d = sqrt(fx*fx+fy*fy);
#endif /* USE_NEUMAIER_KAHAN */
  if (fabs(len2d) < 0.00001)
#else
  const float fx = vec.x;
  const float fy = vec.y;
  const float len2d = sqrtf(TVEC_SUM2(fx*fx, fy*fy));
  if (fabsf(len2d) < 0.0001f)
#endif
  {
    angles.pitch = (vec.z > 0 ? 90 : 270);
    angles.yaw = 0;
  } else {
    angles.pitch = -matan(vec.z, len2d);
    angles.yaw = matan(vec.y, vec.x);
  }
  angles.roll = 0;
}


//==========================================================================
//
//  VectorsAngles
//
//==========================================================================
void VectorsAngles (const TVec &forward, const TVec &right, const TVec &up, TAVec &angles) {
  /*
  if (fabsf(forward.x) < 0.00001 && fabsf(forward.y) < 0.00001) {
    angles.yaw = 0;
    if (forward.z > 0) {
      angles.pitch = 90;
      angles.roll = matan(-up.y, -up.x);
    } else {
      angles.pitch = 270;
      angles.roll = matan(-up.y, up.x);
    }
    return;
  }
  */
#if 0
  const double fx = forward.x;
  const double fy = forward.y;
#ifdef USE_NEUMAIER_KAHAN
  const double len2d = sqrt(neumsum2D(fx*fx, fy*fy));
#else /* USE_NEUMAIER_KAHAN */
  const double len2d = sqrt(fx*fx+fy*fy);
#endif /* USE_NEUMAIER_KAHAN */
  if (fabs(len2d) < 0.00001)
#else
  const float fx = forward.x;
  const float fy = forward.y;
  const float len2d = sqrtf(TVEC_SUM2(fx*fx, fy*fy));
  if (fabsf(len2d) < 0.0001f)
#endif
  {
    angles.yaw = 0;
    if (forward.z > 0) {
      angles.pitch = 90;
      angles.roll = matan(-up.y, -up.x);
    } else {
      angles.pitch = 270;
      angles.roll = matan(-up.y, up.x);
    }
  } else {
    angles.pitch = matan(-forward.z, len2d); // up/down
    angles.yaw = matan(forward.y, forward.x); // left/right
    angles.roll = (right.z || up.z ? matan(-right.z/len2d, up.z/len2d) : 0);
  }
}


//==========================================================================
//
//  RotateVectorAroundVector
//
//==========================================================================
TVec RotateVectorAroundVector (const TVec &Vector, const TVec &Axis, float Angle) {
  guard(RotateVectorAroundVector);
  VRotMatrix M(Axis, Angle);
  return Vector*M;
  unguard;
}


//==========================================================================
//
//  PerpendicularVector
//
//  assumes "src" is normalised
//
//==========================================================================
void PerpendicularVector (TVec &dst, const TVec &src) {
  int pos;
  int i;
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
    clipbase[0] = Normalise(TVec(1.0f, 1.0f/afovx, 0.0f)); // left side clip
    clipbase[1] = Normalise(TVec(1.0f, -1.0f/afovx, 0.0f)); // right side clip
    clipbase[2] = Normalise(TVec(1.0f, 0.0f, -1.0f/afovy)); // top side clip
    clipbase[3] = Normalise(TVec(1.0f, 0.0f, 1.0f/afovy)); // bottom side clip
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

  // store initial args
  /*
  width = awidth;
  height = aheight;
  fov = afov;
  pixelAspect = apixelAspect;
  */

  // create clipbase
  const float afovx = tanf(DEG2RADF(afov)/2.0f);
  const float afovy = afovx*aheight/awidth/apixelAspect;
  setupFromFOVs(afovx, afovy);
}



//==========================================================================
//
//  TFrustum::setup
//
//  `clip_base` is from engine's `SetupFrame()` or `SetupCameraFrame()`
//
//==========================================================================
void TFrustum::setup (const TClipBase &clipbase, const TVec &aorg, const TAVec &aangles, bool createbackplane, const float farplanez) {
  if (!clipbase.isValid() ||
      !isFiniteF(aorg.x) || !isFiniteF(aorg.y) || !isFiniteF(aorg.z) ||
      !isFiniteF(aangles.pitch) || !isFiniteF(aangles.roll) || !isFiniteF(aangles.yaw))
  {
    clear();
    return;
  }
  planeCount = 4; // anyway
  origin = aorg;
  angles = aangles;
  // create direction vectors
  AngleVectors(aangles, vforward, vright, vup);
  // create side planes
  for (unsigned i = 0; i < 4; ++i) {
    const TVec &v = clipbase.clipbase[i];
    const TVec v2(
      TVEC_SUM3(v.y*vright.x, v.z*vup.x, v.x*vforward.x),
      TVEC_SUM3(v.y*vright.y, v.z*vup.y, v.x*vforward.y),
      TVEC_SUM3(v.y*vright.z, v.z*vup.z, v.x*vforward.z));
    planes[i].SetPointDir3D(aorg, v2);
    planes[i].clipflag = 1U<<i;
  }
  // create back plane
  if (createbackplane) {
    planes[4].SetPointDir3D(aorg, vforward);
    planes[4].clipflag = 1U<<4;
    ++planeCount;
  } else {
    planes[4].clipflag = 0;
  }
  // create far plane
  if (isFiniteF(farplanez) && farplanez > 0) {
    planes[5].SetPointDir3D(aorg+vforward*farplanez, -vforward);
    planes[5].clipflag = 1U<<5;
    ++planeCount;
  } else {
    planes[5].clipflag = 0;
  }
  // setup indicies for box checking
  for (unsigned i = 0; i < 6; ++i) {
    if (!planes[i].clipflag) continue;
    unsigned *pindex = bindex[i];
    for (unsigned j = 0; j < 3; ++j) {
      if (planes[i].normal[j] < 0) {
        pindex[j] = j;
        pindex[j+3] = j+3;
      } else {
        pindex[j] = j+3;
        pindex[j+3] = j;
      }
    }
  }
}


//==========================================================================
//
//  TFrustum::setupFromFOVs
//
//==========================================================================
void TFrustum::setupFromFOVs (const float afovx, const float afovy, const TVec &aorg, const TAVec &aangles, bool createbackplane, const float farplanez) {
  if (!isFiniteF(afovx) || !isFiniteF(afovy) ||
      !isFiniteF(aangles.pitch) || !isFiniteF(aangles.roll) || !isFiniteF(aangles.yaw)) {
    clear();
    return;
  }
  TClipBase cb(afovx, afovy);
  setup(cb, aorg, aangles, createbackplane, farplanez);
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
bool TFrustum::checkBox (const float *bbox) const {
  if (!planeCount) return true;
  for (unsigned i = 0; i < 6; ++i) {
    if (!planes[i].clipflag) continue; // don't need to clip against it

    // generate reject point
    const unsigned *pindex = bindex[i];

    TVec rejectpt(bbox[pindex[0]], bbox[pindex[1]], bbox[pindex[2]]);
    if (planes[i].PointOnSide(rejectpt)) {
      // on a back side (or on a plane)
      return false;
    }

    /*
    // generate accept point
    TVec acceptpt;
    acceptpt[0] = bbox[pindex[3+0]];
    acceptpt[1] = bbox[pindex[3+1]];
    acceptpt[2] = bbox[pindex[3+2]];

    // we can reset clipflag bit here if accept point is on a good side
    if (!planes[i].PointOnSide(acceptpt)) {}
    */
  }
  return true;
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
  for (unsigned i = 0; i < 6; ++i) {
    if (!planes[i].clipflag)  continue; // don't need to clip against it
    if (planes[i].PointOnSide(point)) return false; // viewer is in back side or on plane
  }
  return true;
}


//==========================================================================
//
//  TFrustum::checkSphere
//
//  returns `false` is sphere is out of frustum (or frustum is not valid)
//
//==========================================================================
bool TFrustum::checkSphere (const TVec &center, float radius) const {
  if (!planeCount) return true;
  if (radius <= 0) return checkPoint(center);
  for (unsigned i = 0; i < 6; ++i) {
    if (!planes[i].clipflag) continue; // don't need to clip against it
    if (planes[i].SphereOnSide(center, radius)) {
      // on a back side (or on a plane)
      return false;
    }
  }
  return true;
}
