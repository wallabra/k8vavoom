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
  const double ay = DEG2RAD(angles.yaw);
  const double ap = DEG2RAD(angles.pitch);
  const double ar = DEG2RAD(angles.roll);

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
  const double fx = vec.x;
  const double fy = vec.y;
#ifdef USE_NEUMAIER_KAHAN
  const double len2d = sqrt(neumsum2D(fx*fx, fy*fy));
#else /* USE_NEUMAIER_KAHAN */
  const double len2d = sqrt(fx*fx+fy*fy);
#endif /* USE_NEUMAIER_KAHAN */
  if (fabs(len2d) < 0.00001) {
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
  const double fx = forward.x;
  const double fy = forward.y;
#ifdef USE_NEUMAIER_KAHAN
  const double len2d = sqrt(neumsum2D(fx*fx, fy*fy));
#else /* USE_NEUMAIER_KAHAN */
  const double len2d = sqrt(fx*fx+fy*fy);
#endif /* USE_NEUMAIER_KAHAN */
  if (fabs(len2d) < 0.00001) {
    angles.yaw = 0;
    if (forward.z > 0) {
      angles.pitch = 90;
      angles.roll = matan(-up.y, -up.x);
    } else {
      angles.pitch = 270;
      angles.roll = matan(-up.y, up.x);
    }
  } else {
    angles.pitch = matan(-forward.z, len2d);
    angles.yaw = matan(forward.y, forward.x);
    angles.roll = matan(-right.z/len2d, up.z/len2d);
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
//  ProjectPointOnPlane
//
//==========================================================================
void ProjectPointOnPlane (TVec &dst, const TVec &p, const TVec &normal) {
  const float inv_denom = 1.0f/DotProduct(normal, normal);
  if (!isFiniteF(inv_denom)) { dst = TVec(0, 0, 0); return; } //k8: what to do here?
  const float d = DotProduct(normal, p)*inv_denom;
  dst = p-d*(normal*inv_denom);
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
  float minelem = 1.0F;
  TVec tempvec;

  // find the smallest magnitude axially aligned vector
  for (pos = 0, i = 0; i < 3; ++i) {
    if (fabs(src[i]) < minelem) {
      pos = i;
      minelem = fabs(src[i]);
    }
  }
  tempvec[0] = tempvec[1] = tempvec[2] = 0.0f;
  tempvec[pos] = 1.0f;

  // project the point onto the plane defined by src
  ProjectPointOnPlane(dst, tempvec, src);

  // normalise the result
  dst = NormaliseSafe(dst);
}
