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
class TVec {
public:
  float x;
  float y;
  float z;

  TVec () {}
  //nope;TVec () : x(0.0f), y(0.0f), z(0.0f) {}
  TVec (float Ax, float Ay, float Az=0.0f) : x(Ax), y(Ay), z(Az) {}
  TVec (const float f[3]) { x = f[0]; y = f[1]; z = f[2]; }

  inline const float &operator [] (int i) const { return (&x)[i]; }
  inline float &operator [] (int i) { return (&x)[i]; }

  inline bool isValid () const { return (isFiniteF(x) && isFiniteF(y) && isFiniteF(z)); }

  inline TVec &operator += (const TVec &v) { x += v.x; y += v.y; z += v.z; return *this; }
  inline TVec &operator -= (const TVec &v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
  inline TVec &operator *= (float scale) { x *= scale; y *= scale; z *= scale; return *this; }
  inline TVec &operator /= (float scale) {
    scale = 1.0f/scale;
    if (isFiniteF(scale)) {
      x *= scale;
      y *= scale;
      z *= scale;
    } else {
      x = y = z = 0.0f;
    }
    return *this;
  }

  inline TVec operator + (void) const { return *this; }
  inline TVec operator - (void) const { return TVec(-x, -y, -z); }

  inline float Length () const { return sqrtf(x*x+y*y+z*z); }
  inline float length () const { return sqrtf(x*x+y*y+z*z); }

  inline float Length2D () const { return sqrtf(x*x+y*y); }
  inline float length2D () const { return sqrtf(x*x+y*y); }

  inline float LengthSquared () const { return x*x+y*y+z*z; }
  inline float lengthSquared () const { return x*x+y*y+z*z; }

  inline float Length2DSquared () const { return x*x+y*y; }
  inline float length2DSquared () const { return x*x+y*y; }

  inline void normaliseInPlace () { const float invlen = 1.0f/length(); x *= invlen; y *= invlen; z *= invlen; }

  inline TVec Normalised () const { const float invlen = 1.0f/length(); return TVec(x*invlen, y*invlen, z*invlen); }
  inline TVec normalised () const { const float invlen = 1.0f/length(); return TVec(x*invlen, y*invlen, z*invlen); }

  inline TVec NormalisedSafe () const { const float invlen = 1.0f/length(); return (isFiniteF(invlen) ? TVec(x*invlen, y*invlen, z*invlen) : TVec(0, 0, 0)); }
  inline TVec normalisedSafe () const { const float invlen = 1.0f/length(); return (isFiniteF(invlen) ? TVec(x*invlen, y*invlen, z*invlen) : TVec(0, 0, 0)); }

  inline TVec normalised2D () const { const float invlen = 1.0f/length2D(); return TVec(x*invlen, y*invlen, z); }

  inline float dot (const TVec &v2) const { return x*v2.x+y*v2.y+z*v2.z; }
  inline float dot2D (const TVec &v2) const { return x*v2.x+y*v2.y; }

  inline TVec cross (const TVec &v2) const { return TVec(y*v2.z-z*v2.y, z*v2.x-x*v2.z, x*v2.y-y*v2.x); }
  // cross-product (z, as x and y are effectively zero in 2d)
  inline float cross2D (const TVec &v2) const { return (x*v2.y)-(y*v2.x); }
};


class TAVec {
public:
  float pitch;
  float yaw;
  float roll;

  TAVec () {}
  //nope;TAVec () : pitch(0.0f), yaw(0.0f), roll(0.0f) {}
  TAVec (float APitch, float AYaw, float ARoll) : pitch(APitch), yaw(AYaw), roll(ARoll) {}

  inline bool isValid () const { return (isFiniteF(pitch) && isFiniteF(yaw) && isFiniteF(roll)); }

  friend VStream &operator << (VStream &Strm, TAVec &v) {
    return Strm << v.pitch << v.yaw << v.roll;
  }
};


static __attribute__((unused)) inline TVec operator + (const TVec &v1, const TVec &v2) { return TVec(v1.x+v2.x, v1.y+v2.y, v1.z+v2.z); }
static __attribute__((unused)) inline TVec operator - (const TVec &v1, const TVec &v2) { return TVec(v1.x-v2.x, v1.y-v2.y, v1.z-v2.z); }

static __attribute__((unused)) inline TVec operator * (const TVec &v, float s) { return TVec(s*v.x, s*v.y, s*v.z); }
static __attribute__((unused)) inline TVec operator * (float s, const TVec &v) { return TVec(s*v.x, s*v.y, s*v.z); }
static __attribute__((unused)) inline TVec operator / (const TVec &v, float s) { s = 1.0f/s; if (!isFiniteF(s)) s = 0.0f; return TVec(v.x*s, v.y*s, v.z*s); }

static __attribute__((unused)) inline bool operator == (const TVec &v1, const TVec &v2) { return (v1.x == v2.x && v1.y == v2.y && v1.z == v2.z); }
static __attribute__((unused)) inline bool operator != (const TVec &v1, const TVec &v2) { return (v1.x != v2.x || v1.y != v2.y || v1.z != v2.z); }

#ifdef USE_NEUMAIER_KAHAN
static __attribute__((unused)) inline float Length (const TVec &v) { return sqrtf(neumsum3(v.x*v.x, v.y*v.y, v.z*v.z)); }
static __attribute__((unused)) inline float length (const TVec &v) { return sqrtf(neumsum3(v.x*v.x, v.y*v.y, v.z*v.z)); }
static __attribute__((unused)) inline float Length2D (const TVec &v) { return sqrtf(neumsum2(v.x*v.x, v.y*v.y)); }
static __attribute__((unused)) inline float length2D (const TVec &v) { return sqrtf(neumsum2(v.x*v.x, v.y*v.y)); }

static __attribute__((unused)) inline float LengthSquared (const TVec &v) { return neumsum3(v.x*v.x, v.y*v.y, v.z*v.z); }
static __attribute__((unused)) inline float lengthSquared (const TVec &v) { return neumsum3(v.x*v.x, v.y*v.y, v.z*v.z); }
static __attribute__((unused)) inline float Length2DSquared (const TVec &v) { return neumsum2(v.x*v.x, v.y*v.y); }
static __attribute__((unused)) inline float length2DSquared (const TVec &v) { return neumsum2(v.x*v.x, v.y*v.y); }
#else /* USE_NEUMAIER_KAHAN */
static __attribute__((unused)) inline float Length (const TVec &v) { return sqrtf(v.x*v.x+v.y*v.y+v.z*v.z); }
static __attribute__((unused)) inline float length (const TVec &v) { return sqrtf(v.x*v.x+v.y*v.y+v.z*v.z); }
static __attribute__((unused)) inline float Length2D (const TVec &v) { return sqrtf(v.x*v.x+v.y*v.y); }
static __attribute__((unused)) inline float length2D (const TVec &v) { return sqrtf(v.x*v.x+v.y*v.y); }

static __attribute__((unused)) inline float LengthSquared (const TVec &v) { return v.x*v.x+v.y*v.y+v.z*v.z; }
static __attribute__((unused)) inline float lengthSquared (const TVec &v) { return v.x*v.x+v.y*v.y+v.z*v.z; }
static __attribute__((unused)) inline float Length2DSquared (const TVec &v) { return v.x*v.x+v.y*v.y; }
static __attribute__((unused)) inline float length2DSquared (const TVec &v) { return v.x*v.x+v.y*v.y; }
#endif /* USE_NEUMAIER_KAHAN */

static __attribute__((unused)) inline TVec Normalise (const TVec &v) { return v/v.Length(); }
static __attribute__((unused)) inline TVec normalise (const TVec &v) { return v/v.Length(); }

static __attribute__((unused)) inline TVec NormaliseSafe (const TVec &v) { const float invlen = 1.0f/v.length(); return (isFiniteF(invlen) ? v*invlen : TVec(0, 0, 0)); }
static __attribute__((unused)) inline TVec normaliseSafe (const TVec &v) { const float invlen = 1.0f/v.length(); return (isFiniteF(invlen) ? v*invlen : TVec(0, 0, 0)); }

static __attribute__((unused)) inline TVec normalise2D (const TVec &v) { const float invlen = 1.0f/v.length2D(); return TVec(v.x*invlen, v.y*invlen, v.z); }

#ifdef USE_NEUMAIER_KAHAN
static __attribute__((unused)) inline float DotProduct (const TVec &v1, const TVec &v2) { return neumsum3(v1.x*v2.x, v1.y*v2.y, v1.z*v2.z); }
static __attribute__((unused)) inline float dot (const TVec &v1, const TVec &v2) { return neumsum3(v1.x*v2.x, v1.y*v2.y, v1.z*v2.z); }

static __attribute__((unused)) inline float DotProduct2D (const TVec &v1, const TVec &v2) { return neumsum2(v1.x*v2.x, v1.y*v2.y); }
static __attribute__((unused)) inline float dot2D (const TVec &v1, const TVec &v2) { return neumsum2(v1.x*v2.x, v1.y*v2.y); }

static __attribute__((unused)) inline TVec CrossProduct (const TVec &v1, const TVec &v2) { return TVec(neumsum2(v1.y*v2.z, -(v1.z*v2.y)), neumsum2(v1.z*v2.x, -(v1.x*v2.z)), neumsum2(v1.x*v2.y, -(v1.y*v2.x))); }
static __attribute__((unused)) inline TVec cross (const TVec &v1, const TVec &v2) { return TVec(neumsum2(v1.y*v2.z, -(v1.z*v2.y)), neumsum2(v1.z*v2.x, -(v1.x*v2.z)), neumsum2(v1.x*v2.y, -(v1.y*v2.x))); }

// returns signed magnitude of cross-product (z, as x and y are effectively zero in 2d)
static __attribute__((unused)) inline float CrossProduct2D (const TVec &v1, const TVec &v2) { return neumsum2((v1.x*v2.y), -(v1.y*v2.x)); }
static __attribute__((unused)) inline float cross2D (const TVec &v1, const TVec &v2) { return neumsum2((v1.x*v2.y), -(v1.y*v2.x)); }
#else /* USE_NEUMAIER_KAHAN */
static __attribute__((unused)) inline float DotProduct (const TVec &v1, const TVec &v2) { return v1.x*v2.x+v1.y*v2.y+v1.z*v2.z; }
static __attribute__((unused)) inline float dot (const TVec &v1, const TVec &v2) { return v1.x*v2.x+v1.y*v2.y+v1.z*v2.z; }

static __attribute__((unused)) inline float DotProduct2D (const TVec &v1, const TVec &v2) { return v1.x*v2.x+v1.y*v2.y; }
static __attribute__((unused)) inline float dot2D (const TVec &v1, const TVec &v2) { return v1.x*v2.x+v1.y*v2.y; }

static __attribute__((unused)) inline TVec CrossProduct (const TVec &v1, const TVec &v2) { return TVec(v1.y*v2.z-v1.z*v2.y, v1.z*v2.x-v1.x*v2.z, v1.x*v2.y-v1.y*v2.x); }
static __attribute__((unused)) inline TVec cross (const TVec &v1, const TVec &v2) { return TVec(v1.y*v2.z-v1.z*v2.y, v1.z*v2.x-v1.x*v2.z, v1.x*v2.y-v1.y*v2.x); }

// returns signed magnitude of cross-product (z, as x and y are effectively zero in 2d)
static __attribute__((unused)) inline float CrossProduct2D (const TVec &v1, const TVec &v2) { return (v1.x*v2.y)-(v1.y*v2.x); }
static __attribute__((unused)) inline float cross2D (const TVec &v1, const TVec &v2) { return (v1.x*v2.y)-(v1.y*v2.x); }
#endif /* USE_NEUMAIER_KAHAN */

static __attribute__((unused)) inline VStream &operator << (VStream &Strm, TVec &v) { return Strm << v.x << v.y << v.z; }


void AngleVectors (const TAVec &angles, TVec &forward, TVec &right, TVec &up);
void AngleVector (const TAVec &angles, TVec &forward);
void VectorAngles (const TVec &vec, TAVec &angles);
void VectorsAngles (const TVec &forward, const TVec &right, const TVec &up, TAVec &angles);
TVec RotateVectorAroundVector (const TVec &, const TVec &, float);


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
  /*
  int type;
  int signbits;
  int reserved1;
  int reserved2;
  */

  /*
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
  */

  inline void Set (const TVec &Anormal, float Adist) {
    normal = Anormal;
    dist = Adist;
    //CalcBits();
  }

  // initialises vertical plane from point and direction
  inline void SetPointDirXY (const TVec &point, const TVec &dir) {
#if 0
    if (dir.x != 0 || dir.y != 0) {
      normal = Normalise(TVec(dir.y, -dir.x, 0));
    } else {
      //k8: what to do here?!
      normal = TVec(0, 0, 1);
    }
#else
    normal = Normalise(TVec(dir.y, -dir.x, 0));
#endif
    dist = DotProduct(point, normal);
    //CalcBits();
  }

  // initialises vertical plane from 2 points
  inline void Set2Points (const TVec &v1, const TVec &v2) {
    SetPointDirXY(v1, v2-v1);
  }

  // get z of point with given x and y coords
  // don't try to use it on a vertical plane
  inline float GetPointZ (float x, float y) const {
#ifdef USE_NEUMAIER_KAHAN
    return (neumsum3(dist, -(normal.x*x), -(normal.y*y))/normal.z);
#else /* USE_NEUMAIER_KAHAN */
    return (dist-normal.x*x-normal.y*y)/normal.z;
#endif /* USE_NEUMAIER_KAHAN */
  }

  inline float GetPointZ (const TVec &v) const {
    return GetPointZ(v.x, v.y);
  }

  // returns side 0 (front) or 1 (back).
  inline int PointOnSide (const TVec &point) const {
    return (DotProduct(point, normal)-dist <= 0);
  }

  // returns side 0 (front), 1 (back), or 2 (on).
  inline int PointOnSide2 (const TVec &point) const {
    float dot = DotProduct(point, normal)-dist;
    return (dot < -0.1 ? 1 : dot > 0.1 ? 0 : 2);
  }
};


void ProjectPointOnPlane (TVec &dst, const TVec &p, const TVec &normal);
void PerpendicularVector (TVec &dst, const TVec &src); // assumes "src" is normalised
