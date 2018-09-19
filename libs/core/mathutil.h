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
#ifndef VAVOOM_CORELIB_MATHUTIL_H
#define VAVOOM_CORELIB_MATHUTIL_H

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


//int mlog2 (int val);
//int mround (float);
static __attribute((unused)) inline int mround (float Val) { return (int)floor(Val+0.5); }

static __attribute((unused)) inline int ToPowerOf2 (int val) {
  /*
  int answer = 1;
  while (answer < val) answer <<= 1;
  return answer;
  */
  if (val < 1) val = 1;
  --val;
  val |= val>>1;
  val |= val>>2;
  val |= val>>4;
  val |= val>>8;
  val |= val>>16;
  ++val;
  return val;
}

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


static __attribute((unused)) inline float msin (float angle) { return sin(DEG2RAD(angle)); }
static __attribute((unused)) inline float mcos (float angle) { return cos(DEG2RAD(angle)); }
static __attribute((unused)) inline float mtan (float angle) { return tan(DEG2RAD(angle)); }
static __attribute((unused)) inline float masin (float x) { return RAD2DEG(asin(x)); }
static __attribute((unused)) inline float macos (float x) { return RAD2DEG(acos(x)); }
static __attribute((unused)) inline float matan (float y, float x) { return RAD2DEG(atan2(y, x)); }


static __attribute__((unused)) inline float ByteToAngle (vuint8 angle) { return (float)angle*360.0/256.0; }
static __attribute__((unused)) inline vuint8 AngleToByte (float angle) { return (vuint8)(angle*256.0/360.0); }


#endif
