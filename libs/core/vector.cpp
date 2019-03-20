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
  /*
  const float ay = DEG2RADF(angles.yaw);
  const float ap = DEG2RADF(angles.pitch);
  const float ar = DEG2RADF(angles.roll);

  const float sy = sinf(ay);
  const float cy = cosf(ay);
  const float sp = sinf(ap);
  const float cp = cosf(ap);
  const float sr = sinf(ar);
  const float cr = cosf(ar);
  */

  float sy, cy;
  float sp, cp;
  float sr, cr;

  msincos(angles.yaw, &sy, &cy);
  if (angles.pitch) {
    msincos(angles.pitch, &sp, &cp);
    if (angles.roll) {
      msincos(angles.roll, &sr, &cr);

      forward.x = cp*cy;
      forward.y = cp*sy;
      forward.z = -sp;
      right.x = VSUM2(-sr*sp*cy, cr*sy);
      right.y = VSUM2(-sr*sp*sy, -(cr*cy));
      right.z = -sr*cp;
      up.x = VSUM2(cr*sp*cy, sr*sy);
      up.y = VSUM2(cr*sp*sy, -(sr*cy));
      up.z = cr*cp;
    } else {
      // no roll
      forward.x = cp*cy;
      forward.y = cp*sy;
      forward.z = -sp;
      right.x = sy;
      right.y = -cy;
      right.z = 0.0f;
      up.x = sp*cy;
      up.y = sp*sy;
      up.z = cp;
    }
  } else {
    // no pitch, no roll
    forward.x = cy;
    forward.y = sy;
    forward.z = 0.0f;
    right.x = sy;
    right.y = -cy;
    right.z = 0.0f;
    up.x = 0.0f;
    up.y = 0.0f;
    up.z = 1.0f;
  }
}


//==========================================================================
//
//  AngleVector
//
//==========================================================================
void AngleVector (const TAVec &angles, TVec &forward) {
  /*
  const float ay = DEG2RADF(angles.yaw);
  const float ap = DEG2RADF(angles.pitch);

  const float sy = sinf(ay);
  const float cy = cosf(ay);
  const float sp = sinf(ap);
  const float cp = cosf(ap);
  */
  if (angles.pitch) {
    float sy, cy;
    float sp, cp;
    msincos(angles.yaw, &sy, &cy);
    msincos(angles.pitch, &sp, &cp);

    forward.x = cp*cy;
    forward.y = cp*sy;
    forward.z = -sp;
  } else {
    // no pitch
    msincos(angles.yaw, &forward.y, &forward.x);
    forward.z = 0;
  }
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



#ifdef FRUSTUM_BOX_OPTIMISATION
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
#endif


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
#ifdef FRUSTUM_BOX_OPTIMISATION
  // check reject point
  return !PointOnSide(TVec(bbox[pindex[0]], bbox[pindex[1]], bbox[pindex[2]]));
#else
  for (unsigned j = 0; j < 8; ++j) {
    if (!PointOnSide(TVec(bbox[BBoxVertexIndex[j][0]], bbox[BBoxVertexIndex[j][1]], bbox[BBoxVertexIndex[j][2]]))) {
      return true;
    }
  }
  return false;
#endif
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
#ifdef FRUSTUM_BOX_OPTIMISATION
  // check reject point
  if (PointOnSide(TVec(bbox[pindex[0]], bbox[pindex[1]], bbox[pindex[2]]))) return TFrustum::OUTSIDE; // completely outside
  // check accept point
  return (PointOnSide(TVec(bbox[pindex[3+0]], bbox[pindex[3+1]], bbox[pindex[3+2]])) ? TFrustum::PARTIALLY : TFrustum::INSIDE);
#else
  unsigned passed = 0;
  for (unsigned j = 0; j < 8; ++j) {
    if (!PointOnSide(TVec(bbox[BBoxVertexIndex[j][0]], bbox[BBoxVertexIndex[j][1]], bbox[BBoxVertexIndex[j][2]]))) {
      ++passed;
      break;
    }
  }
  return (passed ? (passed == 8 ? TFrustum::INSIDE : TFrustum::PARTIALLY) : TFrustum::OUTSIDE);
#endif
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
    // move back plane forward a little
    // it should be moved to match projection, but our line tracing code
    // is using `0.1f` tolerance, and it should be lower than that
    // our `zNear` is `1.0f` for normal z, and `0.02f` for reverse z
    planes[4].SetPointNormal3D(fp.origin+fp.vforward*0.02f, fp.vforward);
    // sanity check: camera shouldn't be in frustum
    //check(planes[4].PointOnSide(fp.origin));
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
//  TFrustum::checkPoint
//
//  returns `false` is sphere is out of frustum (or frustum is not valid)
//
//==========================================================================
bool TFrustum::checkPoint (const TVec &point, const unsigned mask) const {
  if (!planeCount || !mask) return true;
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
bool TFrustum::checkSphere (const TVec &center, const float radius, const unsigned mask) const {
  if (!planeCount || !mask) return true;
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
bool TFrustum::checkBox (const float bbox[6], const unsigned mask) const {
  if (!planeCount || !mask) return true;
#ifdef FRUSTUM_BBOX_CHECKS
  check(bbox[0] <= bbox[3+0]);
  check(bbox[1] <= bbox[3+1]);
  check(bbox[2] <= bbox[3+2]);
#endif
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!(cp->clipflag&mask)) continue; // don't need to clip against it
    // check reject point
#ifdef FRUSTUM_BOX_OPTIMISATION
    if (cp->PointOnSide(TVec(bbox[cp->pindex[0]], bbox[cp->pindex[1]], bbox[cp->pindex[2]]))) {
      // on a back side (or on a plane)
      return false;
    }
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
int TFrustum::checkBoxEx (const float bbox[6], const unsigned mask) const {
  if (!planeCount || !mask) return INSIDE;
#ifdef FRUSTUM_BBOX_CHECKS
  check(bbox[0] <= bbox[3+0]);
  check(bbox[1] <= bbox[3+1]);
  check(bbox[2] <= bbox[3+2]);
#endif
  int res = INSIDE; // assume that the aabb will be inside the frustum
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!(cp->clipflag&mask)) continue; // don't need to clip against it
#ifdef FRUSTUM_BOX_OPTIMISATION
    // check reject point
    if (cp->PointOnSide(TVec(bbox[cp->pindex[0]], bbox[cp->pindex[1]], bbox[cp->pindex[2]]))) {
      // on a back side (or on a plane)
      //check(cp->PointOnSide(TVec(bbox[cp->pindex[3+0]], bbox[cp->pindex[3+1]], bbox[cp->pindex[3+2]])));
      return OUTSIDE;
    }
    if (res == INSIDE) {
      // check accept point
      if (cp->PointOnSide(TVec(bbox[cp->pindex[3+0]], bbox[cp->pindex[3+1]], bbox[cp->pindex[3+2]]))) res = PARTIALLY;
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
//  TFrustum::checkVerts
//
//==========================================================================
bool TFrustum::checkVerts (const TVec *verts, const unsigned vcount, const unsigned mask) const {
  if (!planeCount || !mask || !vcount) return true;
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!(cp->clipflag&mask)) continue; // don't need to clip against it
    bool passed = false;
    for (unsigned j = 0; j < vcount; ++j) {
      if (!cp->PointOnSide(verts[j])) {
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
//  TFrustum::checkVertsEx
//
//==========================================================================
int TFrustum::checkVertsEx (const TVec *verts, const unsigned vcount, const unsigned mask) const {
  if (!planeCount || !mask || !vcount) return true;
  int res = INSIDE; // assume that the aabb will be inside the frustum
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!(cp->clipflag&mask)) continue; // don't need to clip against it
    if (res == INSIDE) {
      unsigned passed = 0;
      for (unsigned j = 0; j < vcount; ++j) {
        if (!cp->PointOnSide(verts[j])) {
          ++passed;
        }
      }
      if (!passed) return OUTSIDE;
      if (passed != vcount) res = PARTIALLY;
    } else {
      // partially
      bool passed = false;
      for (unsigned j = 0; j < vcount; ++j) {
        if (!cp->PointOnSide(verts[j])) {
          passed = true;
          break;
        }
      }
      if (!passed) return OUTSIDE;
    }
  }
  return res;
}



//==========================================================================
//
//  R_ClipSurface
//
//  clip convex surface to the given plane
//  returns number of new vertices
//  `dest` should have room for at least `vcount+1` vertices
//  precondition: vcount >= 3
//
//  WARNING! not thread-safe, not reentrant!
//
//==========================================================================
int R_ClipSurface (TVec *dest, const TVec *src, int vcount, const TPlane &plane) {
#define ON_EPSILON  (0.1f)

  enum {
    PlaneBack = -1,
    PlaneCoplanar = 0,
    PlaneFront = 1,
  };

  check(dest);
  check(src);
  check(dest != src);
  check(vcount >= 3);

  static int *sides = nullptr;
  static float *dots = nullptr;
  static int tbsize = 0;

  if (tbsize < vcount+1) {
    tbsize = (vcount|0x7f)+1;
    sides = (int *)Z_Realloc(sides, tbsize*sizeof(sides[0]));
    dots = (float *)Z_Realloc(dots, tbsize*sizeof(dots[0]));
  }

  // determine sides for each point
  bool hasFrontSomething = false;
  for (int i = 0; i < vcount; ++i) {
    const float dot = DotProduct(src[i], plane.normal)-plane.dist;
    dots[i] = dot;
         if (dot < -ON_EPSILON) sides[i] = PlaneBack;
    else if (dot > ON_EPSILON) { sides[i] = PlaneFront; hasFrontSomething = true; }
    else sides[i] = PlaneCoplanar;
  }

  if (!hasFrontSomething) return 0; // completely clipped away

  dots[vcount] = dots[0];
  sides[vcount] = sides[0];

  int dcount = 0;

  for (int i = 0; i < vcount; ++i) {
    if (sides[i] == PlaneCoplanar) {
      dest[dcount++] = src[i];
      continue;
    }
    if (sides[i] == PlaneFront) dest[dcount++] = src[i];
    if (sides[i+1] == PlaneCoplanar || sides[i] == sides[i+1]) continue;

    // generate a split point
    const TVec &p1 = src[i];
    const TVec &p2 = src[(i+1)%vcount];
    const float dist = dots[i]/(dots[i]-dots[i+1]);
    TVec &mid = dest[dcount++];
    for (int j = 0; j < 3; ++j) {
      // avoid round off error when possible
           if (plane.normal[j] == 1) mid[j] = plane.dist;
      else if (plane.normal[j] == -1) mid[j] = -plane.dist;
      else mid[j] = p1[j]+dist*(p2[j]-p1[j]);
    }
  }

  return dcount;
#undef ON_EPSILON
}
