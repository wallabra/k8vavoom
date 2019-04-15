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
static const unsigned BBoxVertexIndex[8][3] = {
  {0+0, 0+1, 0+2},
  {3+0, 0+1, 0+2},
  {0+0, 3+1, 0+2},
  {3+0, 3+1, 0+2},
  {0+0, 0+1, 3+2},
  {3+0, 0+1, 3+2},
  {0+0, 3+1, 3+2},
  {3+0, 3+1, 3+2},
};


// ////////////////////////////////////////////////////////////////////////// //
class TAVec {
public:
  float pitch; // up/down
  float yaw; // left/right
  float roll; // around screen center

  TAVec () {}
  TAVec (ENoInit) {}
  //nope;TAVec () : pitch(0.0f), yaw(0.0f), roll(0.0f) {}
  TAVec (float APitch, float AYaw, float ARoll=0.0f) : pitch(APitch), yaw(AYaw), roll(ARoll) {}

  inline bool isValid () const { return (isFiniteF(pitch) && isFiniteF(yaw) && isFiniteF(roll)); }
  inline bool isZero () const { return (pitch == 0.0f && yaw == 0.0f && roll == 0.0f); }
  inline bool isZeroSkipRoll () const { return (pitch == 0.0f && yaw == 0.0f); }

  friend VStream &operator << (VStream &Strm, TAVec &v) {
    return Strm << v.pitch << v.yaw << v.roll;
  }
};

static_assert(__builtin_offsetof(TAVec, yaw) == __builtin_offsetof(TAVec, pitch)+sizeof(float), "TAVec layout fail (0)");
static_assert(__builtin_offsetof(TAVec, roll) == __builtin_offsetof(TAVec, yaw)+sizeof(float), "TAVec layout fail (1)");

static inline __attribute__((unused)) vuint32 GetTypeHash (const TAVec &v) { return joaatHashBuf(&v, 3*sizeof(float)); }

static __attribute__((unused)) inline bool operator == (const TAVec &v1, const TAVec &v2) { return (v1.pitch == v2.pitch && v1.yaw == v2.yaw && v1.roll == v2.roll); }
static __attribute__((unused)) inline bool operator != (const TAVec &v1, const TAVec &v2) { return (v1.pitch != v2.pitch || v1.yaw != v2.yaw || v1.roll != v2.roll); }


// ////////////////////////////////////////////////////////////////////////// //
class TVec {
public:
  float x, y, z;

  TVec () {}
  TVec (ENoInit) {}
  //nope;TVec () : x(0.0f), y(0.0f), z(0.0f) {}
  TVec (float Ax, float Ay, float Az=0.0f) : x(Ax), y(Ay), z(Az) {}
  TVec (const float f[3]) { x = f[0]; y = f[1]; z = f[2]; }

  static inline __attribute__((warn_unused_result)) TVec Invalid () { return TVec(NAN, NAN, NAN); }

  inline __attribute__((warn_unused_result)) const float &operator [] (size_t i) const { check(i < 3); return (&x)[i]; }
  inline __attribute__((warn_unused_result)) float &operator [] (size_t i) { check(i < 3); return (&x)[i]; }

  inline __attribute__((warn_unused_result)) bool isValid () const { return (isFiniteF(x) && isFiniteF(y) && isFiniteF(z)); }
  inline __attribute__((warn_unused_result)) bool isZero () const { return (x == 0.0f && y == 0.0f && z == 0.0f); }
  inline __attribute__((warn_unused_result)) bool isZero2D () const { return (x == 0.0f && y == 0.0f); }

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

#ifdef USE_FAST_INVSQRT
  inline __attribute__((warn_unused_result)) float invlength () const { return fastInvSqrtf(VSUM3(x*x, y*y, z*z)); }
  inline __attribute__((warn_unused_result)) float invlength2D () const { return fastInvSqrtf(VSUM2(x*x, y*y)); }
#else
  inline __attribute__((warn_unused_result)) float invlength () const { return 1.0f/sqrtf(VSUM3(x*x, y*y, z*z)); }
  inline __attribute__((warn_unused_result)) float invlength2D () const { return 1.0f/sqrtf(VSUM2(x*x, y*y)); }
#endif

  inline __attribute__((warn_unused_result)) float Length () const { return sqrtf(VSUM3(x*x, y*y, z*z)); }
  inline __attribute__((warn_unused_result)) float length () const { return sqrtf(VSUM3(x*x, y*y, z*z)); }

  inline __attribute__((warn_unused_result)) float Length2D () const { return sqrtf(VSUM2(x*x, y*y)); }
  inline __attribute__((warn_unused_result)) float length2D () const { return sqrtf(VSUM2(x*x, y*y)); }

  inline __attribute__((warn_unused_result)) float LengthSquared () const { return VSUM3(x*x, y*y, z*z); }
  inline __attribute__((warn_unused_result)) float lengthSquared () const { return VSUM3(x*x, y*y, z*z); }

  inline __attribute__((warn_unused_result)) float Length2DSquared () const { return VSUM2(x*x, y*y); }
  inline __attribute__((warn_unused_result)) float length2DSquared () const { return VSUM2(x*x, y*y); }

  inline __attribute__((warn_unused_result)) float DistanceTo (const TVec &v) const { return sqrtf(VSUM3((x-v.x)*(x-v.x), (y-v.y)*(y-v.y), (z-v.z)*(z-v.z))); }
  inline __attribute__((warn_unused_result)) float DistanceTo2D (const TVec &v) const { return sqrtf(VSUM2((x-v.x)*(x-v.x), (y-v.y)*(y-v.y))); }

  inline __attribute__((warn_unused_result)) float distanceTo (const TVec &v) const { return sqrtf(VSUM3((x-v.x)*(x-v.x), (y-v.y)*(y-v.y), (z-v.z)*(z-v.z))); }
  inline __attribute__((warn_unused_result)) float distanceTo2D (const TVec &v) const { return sqrtf(VSUM2((x-v.x)*(x-v.x), (y-v.y)*(y-v.y))); }

  inline void normaliseInPlace () { const float invlen = invlength(); x *= invlen; y *= invlen; z *= invlen; }
  inline void normalise2DInPlace () { const float invlen = invlength2D(); x *= invlen; y *= invlen; }

  inline __attribute__((warn_unused_result)) TVec Normalised () const { const float invlen = invlength(); return TVec(x*invlen, y*invlen, z*invlen); }
  inline __attribute__((warn_unused_result)) TVec normalised () const { const float invlen = invlength(); return TVec(x*invlen, y*invlen, z*invlen); }

  inline __attribute__((warn_unused_result)) TVec Normalised2D () const { const float invlen = invlength2D(); return TVec(x*invlen, y*invlen, z); }
  inline __attribute__((warn_unused_result)) TVec normalised2D () const { const float invlen = invlength2D(); return TVec(x*invlen, y*invlen, z); }

  inline __attribute__((warn_unused_result)) float Dot (const TVec &v2) const { return VSUM3(x*v2.x, y*v2.y, z*v2.z); }
  inline __attribute__((warn_unused_result)) float dot (const TVec &v2) const { return VSUM3(x*v2.x, y*v2.y, z*v2.z); }

  inline __attribute__((warn_unused_result)) float Dot2D (const TVec &v2) const { return VSUM2(x*v2.x, y*v2.y); }
  inline __attribute__((warn_unused_result)) float dot2D (const TVec &v2) const { return VSUM2(x*v2.x, y*v2.y); }

  inline __attribute__((warn_unused_result)) TVec Cross (const TVec &v2) const { return TVec(VSUM2(y*v2.z, -(z*v2.y)), VSUM2(z*v2.x, -(x*v2.z)), VSUM2(x*v2.y, -(y*v2.x))); }
  inline __attribute__((warn_unused_result)) TVec cross (const TVec &v2) const { return TVec(VSUM2(y*v2.z, -(z*v2.y)), VSUM2(z*v2.x, -(x*v2.z)), VSUM2(x*v2.y, -(y*v2.x))); }

  // 2d cross product (z, as x and y are effectively zero in 2d)
  inline __attribute__((warn_unused_result)) float Cross2D (const TVec &v2) const { return VSUM2(x*v2.y, -(y*v2.x)); }
  inline __attribute__((warn_unused_result)) float cross2D (const TVec &v2) const { return VSUM2(x*v2.y, -(y*v2.x)); }

  // z is zero
  inline __attribute__((warn_unused_result)) TVec mul2 (const float s) const { return TVec(x*s, y*s, 0.0f); }
  inline __attribute__((warn_unused_result)) TVec mul3 (const float s) const { return TVec(x*s, y*s, z*s); }

  // returns projection of this vector onto `v`
  inline __attribute__((warn_unused_result)) TVec projectTo (const TVec &v) const { return v.mul3(dot(v)/v.lengthSquared()); }
  inline __attribute__((warn_unused_result)) TVec projectTo2D (const TVec &v) const { return v.mul2(dot2D(v)/v.length2DSquared()); }

  inline __attribute__((warn_unused_result)) TVec sub2D (const TVec &v) const { return TVec(x-v.x, y-v.y, 0.0f); }

  // dir must be normalised, angle must be valid
  inline __attribute__((warn_unused_result)) bool IsInSpotlight (const TVec &origin, const TVec &dir, const float angle) const {
    TVec surfaceToLight = TVec(-(origin.x-x), -(origin.y-y), -(origin.z-z));
    if (surfaceToLight.lengthSquared() <= 8.0f) return true;
    surfaceToLight.normaliseInPlace();
    const float ltangle = macos(surfaceToLight.dot(dir));
    return (ltangle < angle);
  }

  // dir must be normalised, angle must be valid
  // returns cone light attenuation multiplier in range [0..1]
  inline __attribute__((warn_unused_result)) float CalcSpotlightAttMult (const TVec &origin, const TVec &dir, const float angle) const {
    TVec surfaceToLight = TVec(-(origin.x-x), -(origin.y-y), -(origin.z-z));
    if (surfaceToLight.lengthSquared() <= 8.0f) { return 1.0f; }
    surfaceToLight.normaliseInPlace();
    const float ltangle = macos(surfaceToLight.dot(dir));
    return (ltangle < angle ? sinf(MID(0.0f, (angle-ltangle)/angle, 1.0f)*((float)M_PI/2.0f)) : 0.0f);
  }
};

static_assert(__builtin_offsetof(TVec, y) == __builtin_offsetof(TVec, x)+sizeof(float), "TVec layout fail (0)");
static_assert(__builtin_offsetof(TVec, z) == __builtin_offsetof(TVec, y)+sizeof(float), "TVec layout fail (1)");

static inline __attribute__((unused)) vuint32 GetTypeHash (const TVec &v) { return joaatHashBuf(&v, 3*sizeof(float)); }


static __attribute__((unused)) inline TVec operator + (const TVec &v1, const TVec &v2) { return TVec(VSUM2(v1.x, v2.x), VSUM2(v1.y, v2.y), VSUM2(v1.z, v2.z)); }
static __attribute__((unused)) inline TVec operator - (const TVec &v1, const TVec &v2) { return TVec(VSUM2(v1.x, -(v2.x)), VSUM2(v1.y, -(v2.y)), VSUM2(v1.z, -(v2.z))); }

static __attribute__((unused)) inline TVec operator * (const TVec &v, float s) { return TVec(s*v.x, s*v.y, s*v.z); }
static __attribute__((unused)) inline TVec operator * (float s, const TVec &v) { return TVec(s*v.x, s*v.y, s*v.z); }
static __attribute__((unused)) inline TVec operator / (const TVec &v, float s) { s = 1.0f/s; if (!isFiniteF(s)) s = 0.0f; return TVec(v.x*s, v.y*s, v.z*s); }

static __attribute__((unused)) inline bool operator == (const TVec &v1, const TVec &v2) { return (v1.x == v2.x && v1.y == v2.y && v1.z == v2.z); }
static __attribute__((unused)) inline bool operator != (const TVec &v1, const TVec &v2) { return (v1.x != v2.x || v1.y != v2.y || v1.z != v2.z); }

//static __attribute__((unused)) inline float operator * (const TVec &a, const TVec &b) { return a.dot(b); }
//static __attribute__((unused)) inline TVec operator ^ (const TVec &a, const TVec &b) { return a.cross(b); }
//static __attribute__((unused)) inline TVec operator % (const TVec &a, const TVec &b) { return a.cross(b); }

static __attribute__((unused)) __attribute__((warn_unused_result)) inline float Length (const TVec &v) { return v.length(); }
static __attribute__((unused)) __attribute__((warn_unused_result)) inline float length (const TVec &v) { return v.length(); }
static __attribute__((unused)) __attribute__((warn_unused_result)) inline float Length2D (const TVec &v) { return v.length2D(); }
static __attribute__((unused)) __attribute__((warn_unused_result)) inline float length2D (const TVec &v) { return v.length2D(); }

static __attribute__((unused)) __attribute__((warn_unused_result)) inline float LengthSquared (const TVec &v) { return v.lengthSquared(); }
static __attribute__((unused)) __attribute__((warn_unused_result)) inline float lengthSquared (const TVec &v) { return v.lengthSquared(); }
static __attribute__((unused)) __attribute__((warn_unused_result)) inline float Length2DSquared (const TVec &v) { return v.length2DSquared(); }
static __attribute__((unused)) __attribute__((warn_unused_result)) inline float length2DSquared (const TVec &v) { return v.length2DSquared(); }

static __attribute__((unused)) __attribute__((warn_unused_result)) inline TVec Normalise (const TVec &v) { return v.normalised(); }
static __attribute__((unused)) __attribute__((warn_unused_result)) inline TVec normalise (const TVec &v) { return v.normalised(); }

static __attribute__((unused)) __attribute__((warn_unused_result)) inline TVec normalise2D (const TVec &v) { return v.normalised2D(); }

static __attribute__((unused)) __attribute__((warn_unused_result)) inline float DotProduct (const TVec &v1, const TVec &v2) { return v1.dot(v2); }
static __attribute__((unused)) __attribute__((warn_unused_result)) inline float dot (const TVec &v1, const TVec &v2) { return v1.dot(v2); }

static __attribute__((unused)) __attribute__((warn_unused_result)) inline float DotProduct2D (const TVec &v1, const TVec &v2) { return v1.dot2D(v2); }
static __attribute__((unused)) __attribute__((warn_unused_result)) inline float dot2D (const TVec &v1, const TVec &v2) { return v1.dot2D(v2); }

static __attribute__((unused)) __attribute__((warn_unused_result)) inline TVec CrossProduct (const TVec &v1, const TVec &v2) { return v1.cross(v2); }
static __attribute__((unused)) __attribute__((warn_unused_result)) inline TVec cross (const TVec &v1, const TVec &v2) { return v1.cross(v2); }

// returns signed magnitude of cross-product (z, as x and y are effectively zero in 2d)
static __attribute__((unused)) __attribute__((warn_unused_result)) inline float CrossProduct2D (const TVec &v1, const TVec &v2) { return v1.cross2D(v2); }
static __attribute__((unused)) __attribute__((warn_unused_result)) inline float cross2D (const TVec &v1, const TVec &v2) { return v1.cross2D(v2); }

static __attribute__((unused)) inline VStream &operator << (VStream &Strm, TVec &v) { return Strm << v.x << v.y << v.z; }


void AngleVectors (const TAVec &angles, TVec &forward, TVec &right, TVec &up);
void AngleVector (const TAVec &angles, TVec &forward);
void VectorAngles (const TVec &vec, TAVec &angles);
void VectorsAngles (const TVec &forward, const TVec &right, const TVec &up, TAVec &angles);

__attribute__((warn_unused_result)) TVec RotateVectorAroundVector (const TVec &Vector, const TVec &Axis, float Angle);

static inline __attribute__((unused)) void AngleVectorPitch (const float pitch, TVec &forward) {
  msincos(pitch, &forward.z, &forward.x);
  forward.y = 0.0f;
  forward.z = -forward.z;
  /*
  forward.x = mcos(pitch);
  forward.y = 0.0f;
  forward.z = -msin(pitch);
  */
}


// ////////////////////////////////////////////////////////////////////////// //
// Ax+By+Cz=D (ABC is normal, D is distance); i.e. "general form" (with negative D)
class TPlane {
public:
  TVec normal;
  float dist;

public:
  //TPlane () : TVec(1.0f, 0.0f, 0.0f), dist(0.0f) {}
  //TPlane (ENoInit) {}

  inline __attribute__((warn_unused_result)) bool isValid () const { return (normal.isValid() && !normal.isZero() && isFiniteF(dist)); }
  inline __attribute__((warn_unused_result)) bool isVertical () const { return (normal.z == 0.0f); }

  inline void Set (const TVec &Anormal, float Adist) {
    normal = Anormal;
    dist = Adist;
  }

  inline void SetAndNormalise (const TVec &Anormal, float Adist) {
    normal = Anormal;
    dist = Adist;
    const float mag = normal.invlength();
    normal *= mag;
    dist *= mag;
  }

  // initialises vertical plane from point and direction
  inline void SetPointDirXY (const TVec &point, const TVec &dir) {
    normal = TVec(dir.y, -dir.x, 0.0f);
    normal.normaliseInPlace();
    if (normal.isValid() && !normal.isZero()) {
      dist = DotProduct(point, normal);
    } else {
      //k8: what to do here?!
      normal = TVec(0.0f, 0.0f, 1.0f);
      dist = 1.0f;
    }
  }

  // initialises "full" plane from point and direction
  // `norm` must be normalized, both vectors must be valid
  inline void SetPointNormal3D (const TVec &point, const TVec &norm) {
    normal = norm;
    dist = DotProduct(point, normal);
  }

  // initialises "full" plane from point and direction
  inline void SetPointNormal3DSafe (const TVec &point, const TVec &norm) {
    if (norm.isValid() && point.isValid() && !norm.isZero()) {
      normal = norm.normalised();
      if (normal.isValid() && !normal.isZero()) {
        dist = DotProduct(point, normal);
      } else {
        //k8: what to do here?!
        normal = TVec(0.0f, 0.0f, 1.0f);
        dist = 1.0f;
      }
    } else {
      //k8: what to do here?!
      normal = TVec(0.0f, 0.0f, 1.0f);
      dist = 1.0f;
    }
  }

  // initialises vertical plane from 2 points
  inline void Set2Points (const TVec &v1, const TVec &v2) {
    SetPointDirXY(v1, v2-v1);
  }

  void SetFromTriangle (const TVec &a, const TVec &b, const TVec &c) {
    normal = (b-a).cross(c-a).normalised();
    dist = DotProduct(a, normal);
  }

  // WARNING! do not call this repeatedly, or on normalized plane!
  //          due to floating math inexactness, you will accumulate errors.
  inline void Normalise () {
    const float mag = normal.invlength();
    normal *= mag;
    dist *= mag;
  }

  inline void flipInPlace () {
    normal = -normal;
    dist = -dist;
  }

  // get z of point with given x and y coords
  // don't try to use it on a vertical plane
  inline __attribute__((warn_unused_result)) float GetPointZ (float x, float y) const {
    return (VSUM3(dist, -(normal.x*x), -(normal.y*y))/normal.z);
  }

  inline __attribute__((warn_unused_result)) float GetPointZRev (float x, float y) const {
    return (VSUM3(-dist, -(-normal.x*x), -(-normal.y*y))/(-normal.z));
  }

  inline __attribute__((warn_unused_result)) float GetPointZ (const TVec &v) const {
    return GetPointZ(v.x, v.y);
  }

  inline __attribute__((warn_unused_result)) float GetPointZRev (const TVec &v) const {
    return GetPointZRev(v.x, v.y);
  }

  // "land" point onto the plane
  // plane must be normalized
  inline __attribute__((warn_unused_result)) TVec landAlongNormal (const TVec &point) const {
    const float pdist = DotProduct(point, normal)-dist;
    return (fabs(pdist) > 0.0001f ? point-normal*pdist : point);
  }

  // plane must be normalized
  inline __attribute__((warn_unused_result)) TVec Project (const TVec &v) const {
    return v-(v-normal*dist).dot(normal)*normal;
  }

  /*
  // returns the point where the line p0-p1 intersects this plane
  // `p0` and `p1` must not be the same
  inline float LineIntersectTime (const TVec &p0, const TVec &p1) const {
    return (dist-normal.dot(p0))/normal.dot(p1-p0);
  }

  // returns the point where the line p0-p1 intersects this plane
  // `p0` and `p1` must not be the same
  inline TVec LineIntersect (const TVec &p0, const TVec &p1) const {
    const TVec dif = p1-p0;
    const float t = (dist-normal.dot(p0))/normal.dot(dif);
    return p0+(dif*t);
  }
  */

  // intersection of 3 planes, Graphics Gems 1 pg 305
  // not sure if it should be `dist` or `-dist` here for vavoom planes
  __attribute__((warn_unused_result)) TVec IntersectionPoint (const TPlane &plane2, const TPlane &plane3) const {
    const float det = normal.cross(plane2.normal).dot(plane3.normal);
    // if the determinant is 0, that means parallel planes, no intersection
    if (fabs(det) < 0.001f) return TVec::Invalid();
    return
      (plane2.normal.cross(plane3.normal)*(-dist)+
       plane3.normal.cross(normal)*(-plane2.dist)+
       normal.cross(plane2.normal)*(-plane3.dist))/det;
  }

  // sphere sweep test; if `true` (hit), `hitpos` will be sphere position when it hits this plane, and `u` will be normalized collision time
  // not sure if it should be `dist` or `-dist` here for vavoom planes
  bool sweepSphere (const TVec &origin, const float radius, const TVec &amove, TVec *hitpos=nullptr, float *u=nullptr) const {
    const TVec c1 = origin+amove;
    const float d0 = normal.dot(origin)-dist;
    // check if the sphere is touching the plane
    if (fabsf(d0) <= radius) {
      if (hitpos) *hitpos = origin;
      if (u) *u = 0.0f;
      return true;
    }
    const float d1 = normal.dot(c1)-dist;
    // check if the sphere penetrated during movement
    if (d0 > radius && d1 < radius) {
      if (u || hitpos) {
        const float uu = (d0-radius)/(d0-d1); // normalized time
        if (u) *u = uu;
        if (hitpos) *hitpos = (1.0f-uu)*origin+uu*c1; // point of first contact
      }
      return true;
    }
    // no collision
    return false;
  }


  // returns side 0 (front) or 1 (back, or on plane)
  inline __attribute__((warn_unused_result)) int PointOnSide (const TVec &point) const {
    return (DotProduct(point, normal)-dist <= 0.0f);
  }

  // returns side 0 (front) or 1 (back, or on plane)
  inline __attribute__((warn_unused_result)) int PointOnSideThreshold (const TVec &point) const {
    return (DotProduct(point, normal)-dist < 0.1f);
  }

  // returns side 0 (front, or on plane) or 1 (back)
  // "fri" means "front inclusive"
  inline __attribute__((warn_unused_result)) int PointOnSideFri (const TVec &point) const {
    return (DotProduct(point, normal)-dist < 0.0f);
  }

  // returns side 0 (front), 1 (back), or 2 (on)
  // used in line tracing (only)
  inline __attribute__((warn_unused_result)) int PointOnSide2 (const TVec &point) const {
    const float dot = DotProduct(point, normal)-dist;
    return (dot < -0.1f ? 1 : dot > 0.1f ? 0 : 2);
  }

  // returns side 0 (front), 1 (back)
  // if at least some part of the sphere is on a front side, it means "front"
  inline __attribute__((warn_unused_result)) int SphereOnSide (const TVec &center, float radius) const {
    return (DotProduct(center, normal)-dist <= -radius);
  }

  /*
  // returns side 0 (front), 1 (back)
  // if at least some part of the sphere is on a front side, it means "front"
  inline int SphereOnBackTh (const TVec &center, float radius) const {
    return (DotProduct(center, normal)-dist < -(radius+0.1f));
  }
  */

  inline __attribute__((warn_unused_result)) bool SphereTouches (const TVec &center, float radius) const {
    return (fabsf(DotProduct(center, normal)-dist) < radius);
  }

  // returns side 0 (front), 1 (back), or 2 (collides)
  inline __attribute__((warn_unused_result)) int SphereOnSide2 (const TVec &center, float radius) const {
    const float d = DotProduct(center, normal)-dist;
    return (d < -radius ? 1 : d > radius ? 0 : 2);
  }

  // distance from point to plane
  // plane must be normalized
  inline __attribute__((warn_unused_result)) float Distance (const TVec &p) const {
    //return (cast(double)normal.x*p.x+cast(double)normal.y*p.y+cast(double)normal.z*cast(double)p.z)/normal.dbllength;
    //return VSUM3(normal.x*p.x, normal.y*p.y, normal.z*p.z); // plane normal has length 1
    return DotProduct(p, normal)-dist;
  }

  // returns `false` is box is on the back of the plane (or clipflag is 0)
  __attribute__((warn_unused_result)) bool checkBox (const float *bbox) const;

  // 0: completely outside; >0: completely inside; <0: partially inside
  __attribute__((warn_unused_result)) int checkBoxEx (const float *bbox) const;

  // returns `false` is rect is on the back of the plane
  __attribute__((warn_unused_result)) bool checkRect (const TVec &v0, const TVec &v1) const;

  // 0: completely outside; >0: completely inside; <0: partially inside
  __attribute__((warn_unused_result)) int checkRectEx (const TVec &v0, const TVec &v1) const;
};

static_assert(__builtin_offsetof(TPlane, dist) == __builtin_offsetof(TPlane, normal.z)+sizeof(float), "TPlane layout fail");

static inline __attribute__((unused)) vuint32 GetTypeHash (const TPlane &v) { return joaatHashBuf(&v, 4*sizeof(float)); }


// ////////////////////////////////////////////////////////////////////////// //
class TClipPlane : public TPlane {
public:
  unsigned clipflag;

public:
  //TClipPlane () : TPlane(E_NoInit) { clipflag = 0; }
  //TClipPlane (ENoInit) : TPlane(E_NoInit) {}

  inline bool isValid () const { return !!clipflag; }
  inline void invalidate () { clipflag = 0; }

  inline TClipPlane &operator = (const TPlane &p) {
    normal = p.normal;
    dist = p.dist;
    return *this;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
class TClipParam {
public:
  int width;
  int height;
  float fov;
  float pixelAspect;

public:
  //TClipParam (ENoInit) {}
  TClipParam () : width(0), height(0), fov(0.0f), pixelAspect(1.0f) {}
  TClipParam (int awidth, int aheight, float afov, float apixelAspect=1.0f) : width(awidth), height(aheight), fov(afov), pixelAspect(apixelAspect) {}

  inline bool isValid () const { return (width > 0 && height > 0 && isFiniteF(fov) && fov > 0.0f && isFiniteF(pixelAspect) && pixelAspect > 0.0f); }

  inline bool operator == (const TClipParam &b) const {
    if (!isValid() || !b.isValid()) return false; // never equal
    return (width == b.width && height == b.height && fov == b.fov && pixelAspect == b.pixelAspect);
  }
};


// ////////////////////////////////////////////////////////////////////////// //
class TFrustumParam {
public:
  TVec origin;
  TAVec angles;
  TVec vforward, vright, vup;

public:
  //TFrustumParam (ENoInit) {}
  TFrustumParam () : origin(0.0f, 0.0f, 0.0f), angles(0.0f, 0.0f, 0.0f), vforward(0.0f, 0.0f, 0.0f), vright(0.0f, 0.0f, 0.0f), vup(0.0f, 0.0f, 0.0f) {}
  TFrustumParam (const TVec &aorigin, const TAVec &aangles, const TVec &vf, const TVec &vr, const TVec &vu) : origin(aorigin), angles(aangles), vforward(vf), vright(vr), vup(vu) {}
  TFrustumParam (const TVec &aorigin, const TAVec &aangles) : origin(aorigin), angles(aangles) {
    if (aangles.isValid()) {
      AngleVectors(aangles, vforward, vright, vup);
    } else {
      vforward = TVec(0.0f, 0.0f, 0.0f);
      vright = TVec(0.0f, 0.0f, 0.0f);
      vup = TVec(0.0f, 0.0f, 0.0f);
    }
  }

  inline __attribute__((warn_unused_result)) bool isValid () const {
    return
      origin.isValid() && angles.isValid() && vforward.isValid() && vright.isValid() && vup.isValid() &&
      !vforward.isZero() && !vright.isZero() && !vright.isZero();
  }

  inline __attribute__((warn_unused_result)) bool needUpdate (const TVec &aorg, const TAVec &aangles) const {
    if (!isValid()) return true;
    return (aorg != origin || aangles != angles);
  }

  inline bool operator == (const TFrustumParam &b) const {
    if (!isValid() || !b.isValid()) return false; // never equal
    return (origin == b.origin && angles == b.angles && vforward == b.vforward && vright == b.vright && vup == b.vup);
  }

  inline void setup (const TVec &aorigin, const TAVec &aangles, const TVec &vf, const TVec &vr, const TVec &vu) {
    origin = aorigin;
    angles = aangles;
    vforward = vf;
    vright = vr;
    vup = vu;
  }

  inline void setup (const TVec &aorigin, const TAVec &aangles) {
    origin = aorigin;
    angles = aangles;
    if (aangles.isValid()) {
      AngleVectors(aangles, vforward, vright, vup);
    } else {
      vforward = TVec(0.0f, 0.0f, 0.0f);
      vright = TVec(0.0f, 0.0f, 0.0f);
      vup = TVec(0.0f, 0.0f, 0.0f);
    }
  }
};


// ////////////////////////////////////////////////////////////////////////// //
class TClipBase {
public:
  // calculated
  // [0] is left
  // [1] is right
  // [2] is top
  // [3] is bottom
  TVec clipbase[4];
  float fovx, fovy;

public:
  //TClipBase (ENoInit) {}
  TClipBase () : fovx(0.0f), fovy(0.0f) {}
  TClipBase (int awidth, int aheight, float afov, float apixelAspect=1.0f) { setupViewport(awidth, aheight, afov, apixelAspect); }
  TClipBase (const float afovx, const float afovy) { setupFromFOVs(afovx, afovy); }
  TClipBase (const TClipParam &cp) { setupViewport(cp); }

  inline __attribute__((warn_unused_result)) bool isValid () const { return (fovx != 0.0f); }

  inline void clear () { fovx = fovy = 0.0f; }

  inline __attribute__((warn_unused_result)) const TVec &operator [] (size_t idx) const { check(idx < 4); return clipbase[idx]; }

  void setupFromFOVs (const float afovx, const float afovy);

  void setupViewport (const TClipParam &cp);
  void setupViewport (int awidth, int aheight, float afov, float apixelAspect=1.0f);

  // WARNING! no checks!
  static inline void CalcFovXY (float *outfovx, float *outfovy, const int width, const int height, const float fov, const float pixelAspect=1.0f) {
    const float fovx = tanf(DEG2RADF(fov)/2.0f);
    if (outfovx) *outfovx = fovx;
    if (outfovy) *outfovy = fovx*height/width/pixelAspect;
  }

  static inline void CalcFovXY (float *outfovx, float *outfovy, const TClipParam &cp) {
    const float fovx = tanf(DEG2RADF(cp.fov)/2.0f);
    if (outfovx) *outfovx = fovx;
    if (outfovy) *outfovy = fovx*cp.height/cp.width/cp.pixelAspect;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
class TFrustum {
public:
  enum {
    Left = 0,
    Right = 1,
    Top = 2,
    Bottom = 3,
    Back = 4,
    Forward = 5,
    Near = Back,
    Far = Forward,
  };

  enum {
    LeftBit = 1U<<Left,
    RightBit = 1U<<Right,
    TopBit = 1U<<Top,
    BottomBit = 1U<<Bottom,
    BackBit = 1U<<Back,
    ForwardBit = 1U<<Forward,
    NearBit = BackBit,
    FarBit = ForwardBit,
  };

public:
  // [0] is left, [1] is right, [2] is top, [3] is bottom
  // [4] is back (if `clipflag` is set)
  // [5] is forward (if `clipflag` is set)
  TClipPlane planes[6];
  unsigned planeCount; // total number of valid planes

public:
  //TFrustum (ENoInit) {}
  TFrustum () : planeCount(0) { clear(); }
  TFrustum (const TClipBase &clipbase, const TFrustumParam &fp, bool createbackplane=true, const float farplanez=0.0f) : planeCount(0) {
    setup(clipbase, fp, createbackplane, farplanez);
  }

  inline __attribute__((warn_unused_result)) bool isValid () const { return (planeCount > 0); }

  inline void clear () { planeCount = 0; planes[0].clipflag = planes[1].clipflag = planes[2].clipflag = planes[3].clipflag = planes[4].clipflag = planes[5].clipflag = 0; }

  // for speed; direction vectors should correspond to angles
  void setup (const TClipBase &clipbase, const TFrustumParam &fp, bool createbackplane=true, const float farplanez=0.0f);

  // automatically called by `setup*()`
  void setupBoxIndicies ();

  void setupBoxIndiciesForPlane (unsigned pidx);


  // returns `false` is point is out of frustum
  __attribute__((warn_unused_result)) bool checkPoint (const TVec &point, const unsigned mask=~0u) const;

  // returns `false` is sphere is out of frustum
  __attribute__((warn_unused_result)) bool checkSphere (const TVec &center, const float radius, const unsigned mask=~0u) const;

  // returns `false` is box is out of frustum (or frustum is not valid)
  // bbox:
  //   [0] is minx
  //   [1] is miny
  //   [2] is minz
  //   [3] is maxx
  //   [4] is maxy
  //   [5] is maxz
  __attribute__((warn_unused_result)) bool checkBox (const float bbox[6], const unsigned mask=~0u) const;

  enum { OUTSIDE = 0, INSIDE = 1, PARTIALLY = -1 };

  // 0: completely outside; >0: completely inside; <0: partially inside
  __attribute__((warn_unused_result)) int checkBoxEx (const float bbox[6], const unsigned mask=~0u) const;

  __attribute__((warn_unused_result)) bool checkVerts (const TVec *verts, const unsigned vcount, const unsigned mask=~0u) const;
  __attribute__((warn_unused_result)) int checkVertsEx (const TVec *verts, const unsigned vcount, const unsigned mask=~0u) const;
};


// ////////////////////////////////////////////////////////////////////////// //
// returns `false` on error (and zero `dst`)
static __attribute__((unused)) inline bool ProjectPointOnPlane (TVec &dst, const TVec &p, const TVec &normal) {
  const float inv_denom = 1.0f/DotProduct(normal, normal);
  if (!isFiniteF(inv_denom)) { dst = TVec(0.0f, 0.0f, 0.0f); return false; } //k8: what to do here?
  const float d = DotProduct(normal, p)*inv_denom;
  dst = p-d*(normal*inv_denom);
  return true;
}

void PerpendicularVector (TVec &dst, const TVec &src); // assumes "src" is normalised


// ////////////////////////////////////////////////////////////////////////// //
// sometimes subsector bbox has invalid z; this fixes it
static __attribute__((unused)) inline void FixBBoxZ (float bbox[6]) {
  check(isFiniteF(bbox[2]));
  check(isFiniteF(bbox[3+2]));
  if (bbox[2] > bbox[3+2]) {
    const float tmp = bbox[2];
    bbox[2] = bbox[3+2];
    bbox[3+2] = tmp;
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// check to see if the sphere overlaps the AABB
static __attribute__((unused)) __attribute__((warn_unused_result)) inline bool CheckSphereVsAABB (const float bbox[6], const TVec &lorg, const float radius) {
  float d = 0.0f;
  // find the square of the distance from the sphere to the box
  /*
  for (unsigned i = 0; i < 3; ++i) {
    const float li = lorg[i];
    // first check is min, second check is max
    if (li < bbox[i]) {
      const float s = li-bbox[i];
      d += s*s;
    } else if (li > bbox[i+3]) {
      const float s = li-bbox[i+3];
      d += s*s;
    }
  }
  */
  float s;
  const float *li = &lorg[0];

  s = (*li < bbox[0] ? (*li)-bbox[0] : *li > bbox[0+3] ? (*li)-bbox[0+3] : 0.0f);
  d += s*s;
  ++li;
  ++bbox;
  s = (*li < bbox[0] ? (*li)-bbox[0] : *li > bbox[0+3] ? (*li)-bbox[0+3] : 0.0f);
  d += s*s;
  ++li;
  ++bbox;
  s = (*li < bbox[0] ? (*li)-bbox[0] : *li > bbox[0+3] ? (*li)-bbox[0+3] : 0.0f);
  d += s*s;

  return (d < radius*radius); // or <= if you want exact touching
}


// check to see if the sphere overlaps the AABB (ignore z coords)
static __attribute__((unused)) __attribute__((warn_unused_result)) inline bool CheckSphereVsAABBIgnoreZ (const float bbox[6], const TVec &lorg, const float radius) {
  float d = 0.0f, s;
  // find the square of the distance from the sphere to the box
  // first check is min, second check is max
  const float *li = &lorg[0];

  s = (*li < bbox[0] ? (*li)-bbox[0] : *li > bbox[0+3] ? (*li)-bbox[0+3] : 0.0f);
  d += s*s;
  ++li;
  ++bbox;
  s = (*li < bbox[0] ? (*li)-bbox[0] : *li > bbox[0+3] ? (*li)-bbox[0+3] : 0.0f);
  d += s*s;

  return (d < radius*radius); // or <= if you want exact touching
}


//==========================================================================
//
//  R_ClipSurface
//
//  clip convex surface to the given plane
//  returns number of new vertices
//  `dest` should have room for at least `vcount+1` vertices
//  precondition: vcount >= 3
//
//  WARNING! not thread-safe, not reentrant!
//
//==========================================================================
int R_ClipSurface (TVec *dest, const TVec *src, int vcount, const TPlane &plane);
