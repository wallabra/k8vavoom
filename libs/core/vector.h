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
    x /= scale;
    y /= scale;
    z /= scale;
    return *this;
  }

  inline TVec operator + (void) const { return *this; }

  inline TVec operator - (void) const { return TVec(-x, -y, -z); }

  inline float Length () const { return sqrt(x*x+y*y+z*z); }
  inline float length () const { return sqrt(x*x+y*y+z*z); }

  inline float LengthSquared () const { return x*x+y*y+z*z; }
  inline float lengthSquared () const { return x*x+y*y+z*z; }

  inline float Length2DSquared () const { return x*x+y*y; }
  inline float length2DSquared () const { return x*x+y*y; }
};


inline TVec operator + (const TVec &v1, const TVec &v2) { return TVec(v1.x+v2.x, v1.y+v2.y, v1.z+v2.z); }
inline TVec operator - (const TVec &v1, const TVec &v2) { return TVec(v1.x-v2.x, v1.y-v2.y, v1.z-v2.z); }

inline TVec operator * (const TVec &v, float s) { return TVec(s*v.x, s*v.y, s*v.z); }
inline TVec operator * (float s, const TVec &v) { return TVec(s*v.x, s*v.y, s*v.z); }
inline TVec operator / (const TVec &v, float s) { return TVec(v.x/s, v.y/s, v.z/s); }

inline bool operator == (const TVec &v1, const TVec &v2) { return (v1.x == v2.x && v1.y == v2.y && v1.z == v2.z); }
inline bool operator != (const TVec &v1, const TVec &v2) { return (v1.x != v2.x || v1.y != v2.y || v1.z != v2.z); }

inline float Length (const TVec &v) { return sqrt(v.x*v.x+v.y*v.y+v.z*v.z); }
inline float length (const TVec &v) { return sqrt(v.x*v.x+v.y*v.y+v.z*v.z); }
inline float Length2D (const TVec &v) { return sqrt(v.x*v.x+v.y*v.y); }
inline float length2D (const TVec &v) { return sqrt(v.x*v.x+v.y*v.y); }

inline float LengthSquared (const TVec &v) { return v.x*v.x+v.y*v.y+v.z*v.z; }
inline float lengthSquared (const TVec &v) { return v.x*v.x+v.y*v.y+v.z*v.z; }
inline float Length2DSquared (const TVec &v) { return v.x*v.x+v.y*v.y; }
inline float length2DSquared (const TVec &v) { return v.x*v.x+v.y*v.y; }

inline TVec Normalise (const TVec &v) { return v/v.Length(); }
inline TVec normalise (const TVec &v) { return v/v.Length(); }

inline float DotProduct (const TVec &v1, const TVec &v2) { return v1.x*v2.x+v1.y*v2.y+v1.z*v2.z; }
inline float dot (const TVec &v1, const TVec &v2) { return v1.x*v2.x+v1.y*v2.y+v1.z*v2.z; }

inline float DotProduct2D (const TVec &v1, const TVec &v2) { return v1.x*v2.x+v1.y*v2.y; }
inline float dot2D (const TVec &v1, const TVec &v2) { return v1.x*v2.x+v1.y*v2.y; }

inline TVec CrossProduct (const TVec &v1, const TVec &v2) { return TVec(v1.y*v2.z-v1.z*v2.y, v1.z*v2.x-v1.x*v2.z, v1.x*v2.y-v1.y*v2.x); }
inline TVec cross (const TVec &v1, const TVec &v2) { return TVec(v1.y*v2.z-v1.z*v2.y, v1.z*v2.x-v1.x*v2.z, v1.x*v2.y-v1.y*v2.x); }

inline VStream &operator << (VStream &Strm, TVec &v) { return Strm << v.x << v.y << v.z; }
