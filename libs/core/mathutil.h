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
// results are undefined if edge0 � edge1
static __attribute__((unused)) inline float smoothstep (float edge0, float edge1, float x) {
  // scale, bias and saturate x to 0..1 range
  x = (x-edge0)/(edge1-edge0);
  if (!isFiniteF(x)) return 1;
  if (x < 0) x = 0; else if (x > 1) x = 1;
  // evaluate polynomial
  return x*x*(3-2*x); // GLSL reference
  //return x*x*x*(x*(x*6-15)+10); // Ken Perlin version
}

// `smoothstep` performs smooth Hermite interpolation between 0 and 1 when edge0 < x < edge1
// results are undefined if edge0 � edge1
static __attribute__((unused)) inline float smoothstepPerlin (float edge0, float edge1, float x) {
  // scale, bias and saturate x to 0..1 range
  x = (x-edge0)/(edge1-edge0);
  if (!isFiniteF(x)) return 1;
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
//#define M_PI  (3.14159265358979323846)
#define M_PI  (0x1.921fb54442d18p+1)
#endif

//#define _DEG2RAD  (0.017453292519943296)
#define _DEG2RAD  (0x1.1df46a2529d39p-6)
//#define _RAD2DEG  (57.2957795130823209)
#define _RAD2DEG  (0x1.ca5dc1a63c1f8p+5)

#define DEG2RAD(a)  ((a)*_DEG2RAD)
#define RAD2DEG(a)  ((a)*_RAD2DEG)


//int mlog2 (int val);
//int mround (float);
static __attribute__((unused)) inline int mround (float Val) { return (int)floor(Val+0.5); }

static __attribute__((unused)) inline int ToPowerOf2 (int val) {
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

static __attribute__((unused)) inline float AngleMod (float angle) {
#if 1
  angle = fmodf(angle, 360.0f);
  while (angle < 0.0) angle += 360.0f;
  while (angle >= 360.0f) angle -= 360.0f;
#else
  angle = (360.0/65536)*((int)(angle*(65536/360.0))&65535);
#endif
  return angle;
}

static __attribute__((unused)) inline float AngleMod180 (float angle) {
#if 1
  angle = fmodf(angle, 360.0f);
  while (angle < -180.0f) angle += 360.0f;
  while (angle >= 180.0f) angle -= 360.0f;
#else
  angle += 180;
  angle = (360.0/65536)*((int)(angle*(65536/360.0))&65535);
  angle -= 180;
#endif
  return angle;
}


static __attribute__((unused)) inline float msin (float angle) { return sinf(DEG2RAD(angle)); }
static __attribute__((unused)) inline float mcos (float angle) { return cosf(DEG2RAD(angle)); }
static __attribute__((unused)) inline float mtan (float angle) { return tanf(DEG2RAD(angle)); }
static __attribute__((unused)) inline float masin (float x) { return RAD2DEG(asinf(x)); }
static __attribute__((unused)) inline float macos (float x) { return RAD2DEG(acosf(x)); }
static __attribute__((unused)) inline float matan (float y, float x) { return RAD2DEG(atan2f(y, x)); }


static __attribute__((unused)) inline float ByteToAngle (vuint8 angle) { return (float)(angle*360.0/256.0); }
static __attribute__((unused)) inline vuint8 AngleToByte (float angle) { return (vuint8)(AngleMod(angle)*256.0/360.0); }


// this is actually branch-less for ints on x86, and even for longs on x86_64
static __attribute__((unused)) inline vuint8 clampToByte (vint32 n) {
  n &= -(vint32)(n >= 0);
  return (vuint8)(n|((255-(vint32)n)>>31));
  //return (n < 0 ? 0 : n > 255 ? 255 : n);
}

static __attribute__((unused)) inline vuint8 clampToByteU (vuint32 n) {
  return (vuint8)((n&0xff)|(255-((-(vint32)(n < 256))>>24)));
}


// Neumaier-Kahan algorithm
static __attribute__((unused)) inline float neumsum2 (float v0, const float v1) {
  // one iteration
  const float t = v0+v1;
  return t+(fabsf(v0) >= fabsf(v1) ? (v0-t)+v1 : (v1-t)+v0);
}

// Neumaier-Kahan algorithm
static __attribute__((unused)) inline float neumsum3 (float v0, const float v1, const float v2) {
  // first iteration
  const float t = v0+v1;
  const float c = (fabsf(v0) >= fabsf(v1) ? (v0-t)+v1 : (v1-t)+v0);
  // second iteration
  const float t1 = t+v2;
  return t1+c+(fabsf(t) >= fabsf(v2) ? (t-t1)+v2 : (v2-t1)+t);
}

// Neumaier-Kahan algorithm
static __attribute__((unused)) inline float neumsum4 (float v0, const float v1, const float v2, const float v3) {
  // first iteration
  float t = v0+v1;
  float c = (fabsf(v0) >= fabsf(v1) ? (v0-t)+v1 : (v1-t)+v0);
  v0 = t;
  // second iteration
  t = v0+v2;
  c += (fabsf(v0) >= fabsf(v2) ? (v0-t)+v2 : (v2-t)+v0);
  v0 = t;
  // third iteration
  t = v0+v3;
  c += (fabsf(v0) >= fabsf(v3) ? (v0-t)+v3 : (v3-t)+v0);
  return t+c;
}


// Neumaier-Kahan algorithm
static __attribute__((unused)) inline double neumsum2D (double v0, const double v1) {
  // one iteration
  const double t = v0+v1;
  return t+(fabs(v0) >= fabs(v1) ? (v0-t)+v1 : (v1-t)+v0);
}

// Neumaier-Kahan algorithm
static __attribute__((unused)) inline double neumsum3D (double v0, const double v1, const double v2) {
  // first iteration
  const double t = v0+v1;
  const double c = (fabs(v0) >= fabs(v1) ? (v0-t)+v1 : (v1-t)+v0);
  // second iteration
  const double t1 = t+v2;
  return t1+c+(fabs(t) >= fabs(v2) ? (t-t1)+v2 : (v2-t1)+t);
}


#endif
