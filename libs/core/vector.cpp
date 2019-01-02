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
  right.x = -sr*sp*cy+cr*sy;
  right.y = -sr*sp*sy-cr*cy;
  right.z = -sr*cp;
  up.x = cr*sp*cy+sr*sy;
  up.y = cr*sp*sy-sr*cy;
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
  double length = sqrt(vec.x*vec.x+vec.y*vec.y);
  if (!length) {
    angles.pitch = (vec.z > 0 ? 90 : 270);
    angles.yaw = 0;
    angles.roll = 0;
    return;
  }
  angles.pitch = -matan(vec.z, length);
  angles.yaw = matan(vec.y, vec.x);
  angles.roll = 0;
}


//==========================================================================
//
//  VectorsAngles
//
//==========================================================================
void VectorsAngles (const TVec &forward, const TVec &right, const TVec &up, TAVec &angles) {
  if (!forward.x && !forward.y) {
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
  const double length = sqrt(forward.x*forward.x+forward.y*forward.y);
  angles.pitch = matan(-forward.z, length);
  angles.yaw = matan(forward.y, forward.x);
  angles.roll = matan(-right.z/length, up.z/length);
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
  dst = Normalise(dst);
}
