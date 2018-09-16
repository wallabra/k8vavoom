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
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
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


class TVec {
public:
  float x;
  float y;
  float z;

  TVec () {}

  TVec (float Ax, float Ay, float Az=0.0f) : x(Ax), y(Ay), z(Az) {}

  TVec (const float f[3]) { x = f[0]; y = f[1]; z = f[2]; }

  inline const float &operator [] (int i) const { return (&x)[i]; }
  inline float &operator [] (int i) { return (&x)[i]; }

  inline TVec &operator += (const TVec &v) {
    x += v.x;
    y += v.y;
    z += v.z;
    return *this;
  }

  inline TVec &operator -= (const TVec &v) {
    x -= v.x;
    y -= v.y;
    z -= v.z;
    return *this;
  }

  inline TVec &operator *= (float scale) {
    x *= scale;
    y *= scale;
    z *= scale;
    return *this;
  }

  inline TVec &operator /= (float scale) {
    scale = 1.0f/scale;
    x *= scale;
    y *= scale;
    z *= scale;
    return *this;
  }

  inline TVec operator + (void) const { return *this; }

  inline TVec operator - (void) const { return TVec(-x, -y, -z); }

  inline float Length () const { return sqrt(x*x+y*y+z*z); }
  inline float length () const { return sqrt(x*x+y*y+z*z); }

  inline float Length2D () const { return sqrt(x*x+y*y); }
  inline float length2D () const { return sqrt(x*x+y*y); }

  inline float LengthSquared () const { return x*x+y*y+z*z; }
  inline float lengthSquared () const { return x*x+y*y+z*z; }

  inline float Length2DSquared () const { return x*x+y*y; }
  inline float length2DSquared () const { return x*x+y*y; }
};


static __attribute__((unused)) inline TVec operator + (const TVec &v1, const TVec &v2) { return TVec(v1.x+v2.x, v1.y+v2.y, v1.z+v2.z); }
static __attribute__((unused)) inline TVec operator - (const TVec &v1, const TVec &v2) { return TVec(v1.x-v2.x, v1.y-v2.y, v1.z-v2.z); }

static __attribute__((unused)) inline TVec operator * (const TVec &v, float s) { return TVec(s*v.x, s*v.y, s*v.z); }
static __attribute__((unused)) inline TVec operator * (float s, const TVec &v) { return TVec(s*v.x, s*v.y, s*v.z); }
static __attribute__((unused)) inline TVec operator / (const TVec &v, float s) { s = 1.0f/s; return TVec(v.x*s, v.y*s, v.z*s); }

static __attribute__((unused)) inline bool operator == (const TVec &v1, const TVec &v2) { return (v1.x == v2.x && v1.y == v2.y && v1.z == v2.z); }
static __attribute__((unused)) inline bool operator != (const TVec &v1, const TVec &v2) { return (v1.x != v2.x || v1.y != v2.y || v1.z != v2.z); }

static __attribute__((unused)) inline float Length (const TVec &v) { return sqrt(v.x*v.x+v.y*v.y+v.z*v.z); }
static __attribute__((unused)) inline float length (const TVec &v) { return sqrt(v.x*v.x+v.y*v.y+v.z*v.z); }
static __attribute__((unused)) inline float Length2D (const TVec &v) { return sqrt(v.x*v.x+v.y*v.y); }
static __attribute__((unused)) inline float length2D (const TVec &v) { return sqrt(v.x*v.x+v.y*v.y); }

static __attribute__((unused)) inline float LengthSquared (const TVec &v) { return v.x*v.x+v.y*v.y+v.z*v.z; }
static __attribute__((unused)) inline float lengthSquared (const TVec &v) { return v.x*v.x+v.y*v.y+v.z*v.z; }
static __attribute__((unused)) inline float Length2DSquared (const TVec &v) { return v.x*v.x+v.y*v.y; }
static __attribute__((unused)) inline float length2DSquared (const TVec &v) { return v.x*v.x+v.y*v.y; }

static __attribute__((unused)) inline TVec Normalise (const TVec &v) { return v/v.Length(); }
static __attribute__((unused)) inline TVec normalise (const TVec &v) { return v/v.Length(); }

static __attribute__((unused)) inline TVec NormaliseSafe (const TVec &v) { const float lensq = v.LengthSquared(); return (lensq >= 0.0001 ? v/sqrt(lensq) : TVec(0, 0, 0)); }
static __attribute__((unused)) inline TVec normaliseSafe (const TVec &v) { const float lensq = v.LengthSquared(); return (lensq >= 0.0001 ? v/sqrt(lensq) : TVec(0, 0, 0)); }

static __attribute__((unused)) inline TVec normalise2D (const TVec &v) { const float invlen = 1.0f/v.length2D(); return TVec(v.x*invlen, v.y*invlen, v.z); }

static __attribute__((unused)) inline float DotProduct (const TVec &v1, const TVec &v2) { return v1.x*v2.x+v1.y*v2.y+v1.z*v2.z; }
static __attribute__((unused)) inline float dot (const TVec &v1, const TVec &v2) { return v1.x*v2.x+v1.y*v2.y+v1.z*v2.z; }

static __attribute__((unused)) inline float DotProduct2D (const TVec &v1, const TVec &v2) { return v1.x*v2.x+v1.y*v2.y; }
static __attribute__((unused)) inline float dot2D (const TVec &v1, const TVec &v2) { return v1.x*v2.x+v1.y*v2.y; }

static __attribute__((unused)) inline TVec CrossProduct (const TVec &v1, const TVec &v2) { return TVec(v1.y*v2.z-v1.z*v2.y, v1.z*v2.x-v1.x*v2.z, v1.x*v2.y-v1.y*v2.x); }
static __attribute__((unused)) inline TVec cross (const TVec &v1, const TVec &v2) { return TVec(v1.y*v2.z-v1.z*v2.y, v1.z*v2.x-v1.x*v2.z, v1.x*v2.y-v1.y*v2.x); }

// returns signed magnitude of cross-product (z, as x and y are effectively zero in 2d)
static __attribute__((unused)) inline float CrossProduct2D (const TVec &v1, const TVec &v2) { return (v1.x*v2.y)-(v1.y*v2.x); }
static __attribute__((unused)) inline float cross2D (const TVec &v1, const TVec &v2) { return (v1.x*v2.y)-(v1.y*v2.x); }

static __attribute__((unused)) inline VStream &operator << (VStream &Strm, TVec &v) { return Strm << v.x << v.y << v.z; }
