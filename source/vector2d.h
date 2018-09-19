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
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
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

// ////////////////////////////////////////////////////////////////////////// //
// 2d vector
class TVec2D {
public:
  double x;
  double y;

  TVec2D () {}
  TVec2D (double Ax, double Ay) { x = Ax; y = Ay; }
  TVec2D (const double f[2]) { x = f[0]; y = f[1]; }
  TVec2D (const TVec &v) { x = v.x; y = v.y; }

  inline const double &operator[] (int i) const { return (&x)[i]; }
  inline double &operator[] (int i) { return (&x)[i]; }

  inline TVec2D &operator += (const TVec2D &v) { x += v.x; y += v.y; return *this; }
  inline TVec2D &operator -= (const TVec2D &v) { x -= v.x; y -= v.y; return *this; }
  inline TVec2D &operator *= (double scale) { x *= scale; y *= scale; return *this; }
  inline TVec2D &operator /= (double scale) { x /= scale; y /= scale; return *this; }
  inline TVec2D operator + () const { return *this; }
  inline TVec2D operator - () const { return TVec2D(-x, -y); }
  inline double Length () const { return sqrt(x*x+y*y); }
};

inline TVec2D operator + (const TVec2D &v1, const TVec2D &v2) { return TVec2D(v1.x+v2.x, v1.y+v2.y); }
inline TVec2D operator - (const TVec2D &v1, const TVec2D &v2) { return TVec2D(v1.x-v2.x, v1.y-v2.y); }
inline TVec2D operator * (const TVec2D &v, double s) { return TVec2D(s*v.x, s*v.y); }
inline TVec2D operator * (double s, const TVec2D &v) { return TVec2D(s*v.x, s*v.y); }
inline TVec2D operator / (const TVec2D &v, double s) { return TVec2D(v.x/s, v.y/s); }
inline bool operator == (const TVec2D &v1, const TVec2D &v2) { return (v1.x == v2.x && v1.y == v2.y); }
inline bool operator != (const TVec2D &v1, const TVec2D &v2) { return (v1.x != v2.x || v1.y != v2.y); }
inline double Length (const TVec2D &v) { return sqrt(v.x*v.x+v.y*v.y); }
inline TVec2D Normalise (const TVec2D &v) { return v/v.Length(); }
inline double DotProduct (const TVec2D &v1, const TVec2D &v2) { return v1.x*v2.x+v1.y*v2.y; }


// ////////////////////////////////////////////////////////////////////////// //
// 2d plane (lol)
class TPlane2D {
public:
  TVec2D normal;
  double dist;

  inline void Set (const TVec2D &Anormal, double Adist) { normal = Anormal; dist = Adist; }

  // Initialises vertical plane from point and direction
  inline void SetPointDirXY (const TVec2D &point, const TVec2D &dir) {
    normal = Normalise(TVec2D(dir.y, -dir.x));
    dist = DotProduct(point, normal);
  }

  // Initialises vertical plane from 2 points
  inline void Set2Points (const TVec2D &v1, const TVec2D &v2) {
    SetPointDirXY(v1, v2-v1);
  }
};
