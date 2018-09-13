//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 JƒÅnis Legzdi≈Ü≈°
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

//#define VC_USE_LIBC_FLOAT_CHECKERS
#include <math.h>
#ifdef VC_USE_LIBC_FLOAT_CHECKERS
# define isFiniteF  isfinite
# define isNaNF     isnan
# define isInfF     isinf
#else
static __attribute__((unused)) inline bool isFiniteF (float v) {
  union { float f; vuint32 x; } u = {v};
  return ((u.x&0x7f800000u) != 0x7f800000u);
}

static __attribute__((unused)) inline bool isNaNF (float v) {
  union { float f; vuint32 x; } u = {v};
  return ((u.x<<1) > 0xff000000u);
}

static __attribute__((unused)) inline bool isInfF (float v) {
  union { float f; vuint32 x; } u = {v};
  return ((u.x<<1) == 0xff000000u);
}
#endif


// `smoothstep` performs smooth Hermite interpolation between 0 and 1 when edge0 < x < edge1
// results are undefined if edge0 ô edge1
static __attribute__((unused)) inline float smoothstep (float edge0, float edge1, float x) {
  // scale, bias and saturate x to 0..1 range
  x = (x-edge0)/(edge1-edge0);
  if (x < 0) x = 0; else if (x > 1) x = 1;
  // evaluate polynomial
  return x*x*(3-2*x); // GLSL reference
  //return x*x*x*(x*(x*6-15)+10); // Ken Perlin version
}

// `smoothstep` performs smooth Hermite interpolation between 0 and 1 when edge0 < x < edge1
// results are undefined if edge0 ô edge1
static __attribute__((unused)) inline float smoothstepPerlin (float edge0, float edge1, float x) {
  // scale, bias and saturate x to 0..1 range
  x = (x-edge0)/(edge1-edge0);
  if (x < 0) x = 0; else if (x > 1) x = 1;
  // evaluate polynomial
  return x*x*x*(x*(x*6-15)+10); // Ken Perlin version
}


#undef MIN
#undef MAX
#undef MID
#define MIN(x, y)   ((x) <= (y) ? (x) : (y))
#define MAX(x, y)   ((x) >= (y) ? (x) : (y))
#define MID(min, val, max)  MAX(min, MIN(val, max))


// bounding box
enum {
  BOXTOP,
  BOXBOTTOM,
  BOXLEFT,
  BOXRIGHT,
};

//==========================================================================
//
//  Angles
//
//==========================================================================

#ifndef M_PI
#define M_PI  (3.14159265358979323846)
#endif

#define _DEG2RAD  (0.017453292519943296)
#define _RAD2DEG  (57.2957795130823209)

#define DEG2RAD(a)    ((a) * _DEG2RAD)
#define RAD2DEG(a)    ((a) * _RAD2DEG)


class TAVec {
public:
  float pitch;
  float yaw;
  float roll;

  TAVec () {}
  TAVec (float APitch, float AYaw, float ARoll) : pitch(APitch), yaw(AYaw), roll(ARoll) {}

  friend VStream &operator << (VStream &Strm, TAVec &v) {
    return Strm << v.pitch << v.yaw << v.roll;
  }
};


//int mlog2 (int val);
//int mround (float);
static __attribute((unused)) inline int mround (float Val) { return (int)floor(Val+0.5); }

int ToPowerOf2 (int val);

//float AngleMod (float angle);
//float AngleMod180 (float angle);

static __attribute((unused)) inline float AngleMod (float angle) {
#if 1
  angle = fmodf(angle, 360.0f);
  while (angle < 0.0) angle += 360.0;
  while (angle >= 360.0) angle -= 360.0;
#else
  angle = (360.0/65536)*((int)(angle*(65536/360.0))&65535);
#endif
  return angle;
}

static __attribute((unused)) inline float AngleMod180 (float angle) {
#if 1
  angle = fmodf(angle, 360.0f);
  while (angle < -180.0) angle += 360.0;
  while (angle >= 180.0) angle -= 360.0;
#else
  angle += 180;
  angle = (360.0/65536)*((int)(angle*(65536/360.0))&65535);
  angle -= 180;
#endif
  return angle;
}

void AngleVectors (const TAVec &angles, TVec &forward, TVec &right, TVec &up);
void AngleVector (const TAVec &angles, TVec &forward);
void VectorAngles (const TVec &vec, TAVec &angles);
void VectorsAngles (const TVec &forward, const TVec &right, const TVec &up, TAVec &angles);
TVec RotateVectorAroundVector (const TVec &, const TVec &, float);


static __attribute((unused)) inline float msin (float angle) { return sin(DEG2RAD(angle)); }
static __attribute((unused)) inline float mcos (float angle) { return cos(DEG2RAD(angle)); }
static __attribute((unused)) inline float mtan (float angle) { return tan(DEG2RAD(angle)); }
static __attribute((unused)) inline float masin (float x) { return RAD2DEG(asin(x)); }
static __attribute((unused)) inline float macos (float x) { return RAD2DEG(acos(x)); }
static __attribute((unused)) inline float matan (float y, float x) { return RAD2DEG(atan2(y, x)); }


//==========================================================================
//
//                PLANES
//
//==========================================================================

enum {
  PLANE_X,
  PLANE_Y,
  PLANE_Z,
  PLANE_NEG_X,
  PLANE_NEG_Y,
  PLANE_NEG_Z,
  PLANE_ANY,
};


class TPlane {
 public:
  TVec normal;
  float dist;
  int type;
  int signbits;
  int reserved1;
  int reserved2;

  void CalcBits () {
         if (normal.x == 1.0) type = PLANE_X;
    else if (normal.y == 1.0) type = PLANE_Y;
    else if (normal.z == 1.0) type = PLANE_Z;
    else if (normal.x == -1.0) type = PLANE_NEG_X;
    else if (normal.y == -1.0) type = PLANE_NEG_Y;
    else if (normal.z == -1.0) type = PLANE_NEG_Z;
    else type = PLANE_ANY;

    signbits = 0;
    if (normal.x < 0.0) signbits |= 1;
    if (normal.y < 0.0) signbits |= 2;
    if (normal.z < 0.0) signbits |= 4;
  }

  inline void Set (const TVec &Anormal, float Adist) {
    normal = Anormal;
    dist = Adist;
    CalcBits();
  }

  // initialises vertical plane from point and direction
  inline void SetPointDir (const TVec &point, const TVec &dir) {
    normal = Normalise(TVec(dir.y, -dir.x, 0));
    dist = DotProduct(point, normal);
    CalcBits();
  }

  // initialises vertical plane from 2 points
  inline void Set2Points (const TVec &v1, const TVec &v2) {
    SetPointDir(v1, v2 - v1);
  }

  // get z of point with given x and y coords
  // don't try to use it on a vertical plane
  inline float GetPointZ (float x, float y) const {
    return (dist-normal.x*x-normal.y*y)/normal.z;
  }

  inline float GetPointZ (const TVec &v) const {
    return GetPointZ(v.x, v.y);
  }

  // returns side 0 (front) or 1 (back).
  inline int PointOnSide(const TVec &point) const {
    return (DotProduct(point, normal)-dist <= 0);
  }

  // returns side 0 (front), 1 (back), or 2 (on).
  inline int PointOnSide2(const TVec &point) const {
    float dot = DotProduct(point, normal) - dist;
    return (dot < -0.1 ? 1 : dot > 0.1 ? 0 : 2);
  }
};


static __attribute__((unused)) inline float ByteToAngle (vuint8 angle) { return (float)angle*360.0/256.0; }
static __attribute__((unused)) inline vuint8 AngleToByte (float angle) { return (vuint8)(angle*256.0/360.0); }
