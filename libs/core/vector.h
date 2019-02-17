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
#ifdef USE_NEUMAIER_KAHAN
# define TVEC_SUM2(value0,value1)  neumsum2(value0, value1)
# define TVEC_SUM3(value0,value1,value2)  neumsum3(value0, value1, value2)
#else
# define TVEC_SUM2(value0,value1)  ((value0)+(value1))
# define TVEC_SUM3(value0,value1,value2)  ((value0)+(value1)+(value2))
#endif

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

  inline float Length () const { return sqrtf(TVEC_SUM3(x*x, y*y, z*z)); }
  inline float length () const { return sqrtf(TVEC_SUM3(x*x, y*y, z*z)); }

  inline float Length2D () const { return sqrtf(TVEC_SUM2(x*x, y*y)); }
  inline float length2D () const { return sqrtf(TVEC_SUM2(x*x, y*y)); }

  inline float LengthSquared () const { return TVEC_SUM3(x*x, y*y, z*z); }
  inline float lengthSquared () const { return TVEC_SUM3(x*x, y*y, z*z); }

  inline float Length2DSquared () const { return TVEC_SUM2(x*x, y*y); }
  inline float length2DSquared () const { return TVEC_SUM2(x*x, y*y); }

  inline void normaliseInPlace () { const float invlen = 1.0f/length(); x *= invlen; y *= invlen; z *= invlen; }

  inline TVec Normalised () const { const float invlen = 1.0f/length(); return TVec(x*invlen, y*invlen, z*invlen); }
  inline TVec normalised () const { const float invlen = 1.0f/length(); return TVec(x*invlen, y*invlen, z*invlen); }

  inline TVec NormalisedSafe () const { const float invlen = 1.0f/length(); return (isFiniteF(invlen) ? TVec(x*invlen, y*invlen, z*invlen) : TVec(0, 0, 0)); }
  inline TVec normalisedSafe () const { const float invlen = 1.0f/length(); return (isFiniteF(invlen) ? TVec(x*invlen, y*invlen, z*invlen) : TVec(0, 0, 0)); }

  inline TVec normalised2D () const { const float invlen = 1.0f/length2D(); return TVec(x*invlen, y*invlen, z); }

  inline float dot (const TVec &v2) const { return TVEC_SUM3(x*v2.x, y*v2.y, z*v2.z); }
  inline float dot2D (const TVec &v2) const { return TVEC_SUM2(x*v2.x, y*v2.y); }

  inline TVec cross (const TVec &v2) const { return TVec(TVEC_SUM2(y*v2.z, -(z*v2.y)), TVEC_SUM2(z*v2.x, -(x*v2.z)), TVEC_SUM2(x*v2.y, -(y*v2.x))); }
  // cross-product (z, as x and y are effectively zero in 2d)
  inline float cross2D (const TVec &v2) const { return TVEC_SUM2(x*v2.y, -(y*v2.x)); }
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


static __attribute__((unused)) inline TVec operator + (const TVec &v1, const TVec &v2) { return TVec(TVEC_SUM2(v1.x, v2.x), TVEC_SUM2(v1.y, v2.y), TVEC_SUM2(v1.z, v2.z)); }
static __attribute__((unused)) inline TVec operator - (const TVec &v1, const TVec &v2) { return TVec(TVEC_SUM2(v1.x, -(v2.x)), TVEC_SUM2(v1.y, -(v2.y)), TVEC_SUM2(v1.z, -(v2.z))); }

static __attribute__((unused)) inline TVec operator * (const TVec &v, float s) { return TVec(s*v.x, s*v.y, s*v.z); }
static __attribute__((unused)) inline TVec operator * (float s, const TVec &v) { return TVec(s*v.x, s*v.y, s*v.z); }
static __attribute__((unused)) inline TVec operator / (const TVec &v, float s) { s = 1.0f/s; if (!isFiniteF(s)) s = 0.0f; return TVec(v.x*s, v.y*s, v.z*s); }

static __attribute__((unused)) inline bool operator == (const TVec &v1, const TVec &v2) { return (v1.x == v2.x && v1.y == v2.y && v1.z == v2.z); }
static __attribute__((unused)) inline bool operator != (const TVec &v1, const TVec &v2) { return (v1.x != v2.x || v1.y != v2.y || v1.z != v2.z); }

static __attribute__((unused)) inline float Length (const TVec &v) { return sqrtf(TVEC_SUM3(v.x*v.x, v.y*v.y, v.z*v.z)); }
static __attribute__((unused)) inline float length (const TVec &v) { return sqrtf(TVEC_SUM3(v.x*v.x, v.y*v.y, v.z*v.z)); }
static __attribute__((unused)) inline float Length2D (const TVec &v) { return sqrtf(TVEC_SUM2(v.x*v.x, v.y*v.y)); }
static __attribute__((unused)) inline float length2D (const TVec &v) { return sqrtf(TVEC_SUM2(v.x*v.x, v.y*v.y)); }

static __attribute__((unused)) inline float LengthSquared (const TVec &v) { return TVEC_SUM3(v.x*v.x, v.y*v.y, v.z*v.z); }
static __attribute__((unused)) inline float lengthSquared (const TVec &v) { return TVEC_SUM3(v.x*v.x, v.y*v.y, v.z*v.z); }
static __attribute__((unused)) inline float Length2DSquared (const TVec &v) { return TVEC_SUM2(v.x*v.x, v.y*v.y); }
static __attribute__((unused)) inline float length2DSquared (const TVec &v) { return TVEC_SUM2(v.x*v.x, v.y*v.y); }

static __attribute__((unused)) inline TVec Normalise (const TVec &v) { return v/v.Length(); }
static __attribute__((unused)) inline TVec normalise (const TVec &v) { return v/v.Length(); }

static __attribute__((unused)) inline TVec NormaliseSafe (const TVec &v) { const float invlen = 1.0f/v.length(); return (isFiniteF(invlen) ? v*invlen : TVec(0, 0, 0)); }
static __attribute__((unused)) inline TVec normaliseSafe (const TVec &v) { const float invlen = 1.0f/v.length(); return (isFiniteF(invlen) ? v*invlen : TVec(0, 0, 0)); }

static __attribute__((unused)) inline TVec normalise2D (const TVec &v) { const float invlen = 1.0f/v.length2D(); return TVec(v.x*invlen, v.y*invlen, v.z); }

static __attribute__((unused)) inline float DotProduct (const TVec &v1, const TVec &v2) { return TVEC_SUM3(v1.x*v2.x, v1.y*v2.y, v1.z*v2.z); }
static __attribute__((unused)) inline float dot (const TVec &v1, const TVec &v2) { return TVEC_SUM3(v1.x*v2.x, v1.y*v2.y, v1.z*v2.z); }

static __attribute__((unused)) inline float DotProduct2D (const TVec &v1, const TVec &v2) { return TVEC_SUM2(v1.x*v2.x, v1.y*v2.y); }
static __attribute__((unused)) inline float dot2D (const TVec &v1, const TVec &v2) { return TVEC_SUM2(v1.x*v2.x, v1.y*v2.y); }

static __attribute__((unused)) inline TVec CrossProduct (const TVec &v1, const TVec &v2) { return TVec(TVEC_SUM2(v1.y*v2.z, -(v1.z*v2.y)), TVEC_SUM2(v1.z*v2.x, -(v1.x*v2.z)), TVEC_SUM2(v1.x*v2.y, -(v1.y*v2.x))); }
static __attribute__((unused)) inline TVec cross (const TVec &v1, const TVec &v2) { return TVec(TVEC_SUM2(v1.y*v2.z, -(v1.z*v2.y)), TVEC_SUM2(v1.z*v2.x, -(v1.x*v2.z)), TVEC_SUM2(v1.x*v2.y, -(v1.y*v2.x))); }

// returns signed magnitude of cross-product (z, as x and y are effectively zero in 2d)
static __attribute__((unused)) inline float CrossProduct2D (const TVec &v1, const TVec &v2) { return TVEC_SUM2((v1.x*v2.y), -(v1.y*v2.x)); }
static __attribute__((unused)) inline float cross2D (const TVec &v1, const TVec &v2) { return TVEC_SUM2((v1.x*v2.y), -(v1.y*v2.x)); }

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

  inline void Set (const TVec &Anormal, float Adist) {
    normal = Anormal;
    dist = Adist;
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
    dist = DotProduct(point, normal);
#else
    normal = Normalise(TVec(dir.y, -dir.x, 0));
    if (!isFiniteF(dir.x) || !isFiniteF(dir.y) || !isFiniteF(dir.z)) {
      //k8: what to do here?!
      normal = TVec(0, 0, 1);
      dist = 1;
    } else {
      dist = DotProduct(point, normal);
    }
#endif
  }

  // initialises "full" plane from point and direction
  // `dir` must be normalized, both vectors must be valid
  inline void SetPointDir3D (const TVec &point, const TVec &dir) {
    normal = dir;
    dist = DotProduct(point, normal);
  }

  // initialises "full" plane from point and direction
  inline void SetPointDir3DSafe (const TVec &point, const TVec &dir) {
    if (!isFiniteF(dir.x) || !isFiniteF(dir.y) || !isFiniteF(dir.z) ||
        !isFiniteF(point.x) || !isFiniteF(point.y) || !isFiniteF(point.z))
    {
      //k8: what to do here?!
      normal = TVec(0, 0, 1);
      dist = 1;
    } else {
      normal = dir.normalisedSafe();
      if (normal.x || normal.y || normal.z) {
        dist = DotProduct(point, normal);
      } else {
        //k8: what to do here?!
        normal = TVec(0, 0, 1);
        dist = 1;
      }
    }
  }

  // initialises vertical plane from 2 points
  inline void Set2Points (const TVec &v1, const TVec &v2) {
    SetPointDirXY(v1, v2-v1);
  }

  // get z of point with given x and y coords
  // don't try to use it on a vertical plane
  inline float GetPointZ (float x, float y) const {
    return (TVEC_SUM3(dist, -(normal.x*x), -(normal.y*y))/normal.z);
  }

  inline float GetPointZ (const TVec &v) const {
    return GetPointZ(v.x, v.y);
  }

  // returns side 0 (front) or 1 (back)
  inline int PointOnSide (const TVec &point) const {
    return (DotProduct(point, normal)-dist <= 0);
  }

  // returns side 0 (front), 1 (back), or 2 (on)
  inline int PointOnSide2 (const TVec &point) const {
    const float dot = DotProduct(point, normal)-dist;
    return (dot < -0.1 ? 1 : dot > 0.1 ? 0 : 2);
  }

  // returns side 0 (front), 1 (back)
  // if at least some part of the sphere is on a front side, it means "front"
  inline int SphereOnSide (const TVec &center, float radius) const {
    return (DotProduct(center, normal)-dist < -radius);
  }

  // returns side 0 (front), 1 (back), or 2 (collides)
  inline int SphereOnSide2 (const TVec &center, float radius) const {
    const float dist = DotProduct(center, normal)-dist;
    return (dist < -radius ? 1 : dist > radius ? 0 : 2);
  }

  // distance from point to plane
  // plane must be normalized
  inline float Distance (const TVec &p) const {
    //return (cast(double)normal.x*p.x+cast(double)normal.y*p.y+cast(double)normal.z*cast(double)p.z)/normal.dbllength;
    return TVEC_SUM3(normal.x*p.x, normal.y*p.y, normal.z*p.z); // plane normal has length 1
  }
};


// ////////////////////////////////////////////////////////////////////////// //
class TClipPlane : public TPlane {
public:
  unsigned clipflag;
};


// ////////////////////////////////////////////////////////////////////////// //
class TFrustum {
public:
  // [0] is left, [1] is right, [2] is top, [3] is bottom
  // [4] is back (if `clipflag` is set)
  // [5] is forward (if `clipflag` is set)
  TClipPlane planes[6];
  unsigned planeCount; // total number of valid planes
  TVec origin;
  TAVec angles;
  TVec vforward, vright, vup;
  unsigned bindex[6][3];

public:
  TFrustum () : planeCount(0) {}

  inline bool isValid () const { return (planeCount > 0); }

  inline void clear () { planeCount = 0; }

  inline bool needUpdate (const TVec &aorg, const TAVec &aangles) const {
    return
      !planeCount ||
      origin.x != aorg.x ||
      origin.y != aorg.y ||
      origin.z != aorg.z ||
      aangles.pitch != angles.pitch ||
      aangles.roll != angles.roll ||
      aangles.yaw != angles.yaw;
  }

  inline void update (const TVec *clip_base, const TVec &aorg, const TAVec &aangles, bool createbackplane=true, const float farplanez=0.0f) {
    if (needUpdate(aorg, aangles)) setup(clip_base, aorg, aangles, createbackplane, farplanez);
  }

  // `clip_base` is from engine's `SetupFrame()` or `SetupCameraFrame()`
  void setup (const TVec *clip_base, const TVec &aorg, const TAVec &aangles, bool createbackplane=true, const float farplanez=0.0f);

  // returns `false` is box is out of frustum (or frustum is not valid)
  // bbox:
  //   [0] is minx
  //   [1] is miny
  //   [2] is minz
  //   [3] is maxx
  //   [4] is maxy
  //   [5] is maxz
  bool checkBox (const float *bbox) const;

  // returns `false` is point is out of frustum (or frustum is not valid)
  bool checkPoint (const TVec &point) const;

  // returns `false` is sphere is out of frustum (or frustum is not valid)
  bool checkSphere (const TVec &center, float radius) const;
};


// ////////////////////////////////////////////////////////////////////////// //
void ProjectPointOnPlane (TVec &dst, const TVec &p, const TVec &normal);
void PerpendicularVector (TVec &dst, const TVec &src); // assumes "src" is normalised
