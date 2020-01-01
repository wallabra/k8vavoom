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
//**  Copyright (C) 2018-2020 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, version 3 of the License ONLY.
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
#define CONST_BBoxVertexIndex  \
  const unsigned BBoxVertexIndex[8][3] = { \
    {0+0, 0+1, 0+2}, \
    {3+0, 0+1, 0+2}, \
    {0+0, 3+1, 0+2}, \
    {3+0, 3+1, 0+2}, \
    {0+0, 0+1, 3+2}, \
    {3+0, 0+1, 3+2}, \
    {0+0, 3+1, 3+2}, \
    {3+0, 3+1, 3+2}, \
  }

// this is for 2d line/node bboxes
// bounding box
enum {
  BOX2D_TOP,
  BOX2D_BOTTOM,
  BOX2D_LEFT,
  BOX2D_RIGHT,
  // or this
  BOX2D_MAXY = 0,
  BOX2D_MINY = 1,
  BOX2D_MINX = 2,
  BOX2D_MAXX = 3,
};

// this is for 3d bboxes
// bounding box
enum {
  BOX3D_MINX = 0,
  BOX3D_MINY = 1,
  BOX3D_MINZ = 2,
  BOX3D_MAXX = 3,
  BOX3D_MAXY = 4,
  BOX3D_MAXZ = 5,
  // various constants
  BOX3D_MINIDX = 0,
  BOX3D_MAXIDX = 3,
  // add those to MINIDX/MAXIDX to get the corresponding element
  BOX3D_X = 0,
  BOX3D_Y = 1,
  BOX3D_Z = 2,
};


// ////////////////////////////////////////////////////////////////////////// //
class /*__attribute__((packed))*/ TAVec {
public:
  float pitch; // up/down
  float yaw; // left/right
  float roll; // around screen center

  inline TAVec () noexcept {}
  inline TAVec (ENoInit) noexcept {}
  //nope;TAVec () noexcept : pitch(0.0f), yaw(0.0f), roll(0.0f) {}
  inline TAVec (float APitch, float AYaw, float ARoll=0.0f) noexcept : pitch(APitch), yaw(AYaw), roll(ARoll) {}

  inline bool isValid () const noexcept { return (isFiniteF(pitch) && isFiniteF(yaw) && isFiniteF(roll)); }
  inline bool isZero () const noexcept { return (pitch == 0.0f && yaw == 0.0f && roll == 0.0f); }
  inline bool isZeroSkipRoll () const noexcept { return (pitch == 0.0f && yaw == 0.0f); }

  friend VStream &operator << (VStream &Strm, TAVec &v) {
    return Strm << v.pitch << v.yaw << v.roll;
  }
};

static_assert(__builtin_offsetof(TAVec, yaw) == __builtin_offsetof(TAVec, pitch)+sizeof(float), "TAVec layout fail (0)");
static_assert(__builtin_offsetof(TAVec, roll) == __builtin_offsetof(TAVec, yaw)+sizeof(float), "TAVec layout fail (1)");
static_assert(sizeof(TAVec) == sizeof(float)*3, "TAVec layout fail (2)");

static inline VVA_OKUNUSED vuint32 GetTypeHash (const TAVec &v) noexcept { return joaatHashBuf(&v, 3*sizeof(float)); }

static VVA_OKUNUSED inline bool operator == (const TAVec &v1, const TAVec &v2) noexcept { return (v1.pitch == v2.pitch && v1.yaw == v2.yaw && v1.roll == v2.roll); }
static VVA_OKUNUSED inline bool operator != (const TAVec &v1, const TAVec &v2) noexcept { return (v1.pitch != v2.pitch || v1.yaw != v2.yaw || v1.roll != v2.roll); }


// ////////////////////////////////////////////////////////////////////////// //
class /*__attribute__((packed))*/ TVec {
public:
  float x, y, z;

public:
  static const TVec ZeroVector;

public:
  inline TVec () noexcept {}
  inline TVec (ENoInit) noexcept {}
  //nope;TVec () : x(0.0f), y(0.0f), z(0.0f) {}
  inline TVec (float Ax, float Ay, float Az=0.0f) noexcept : x(Ax), y(Ay), z(Az) {}
  inline TVec (const float f[3]) noexcept { x = f[0]; y = f[1]; z = f[2]; }

  static inline VVA_CHECKRESULT TVec Invalid () noexcept { return TVec(NAN, NAN, NAN); }

  inline VVA_CHECKRESULT const float &operator [] (size_t i) const noexcept { vassert(i < 3); return (&x)[i]; }
  inline VVA_CHECKRESULT float &operator [] (size_t i) noexcept { vassert(i < 3); return (&x)[i]; }

  inline VVA_CHECKRESULT bool isValid () const noexcept { return (isFiniteF(x) && isFiniteF(y) && isFiniteF(z)); }
  inline VVA_CHECKRESULT bool isZero () const noexcept { return (x == 0.0f && y == 0.0f && z == 0.0f); }
  inline VVA_CHECKRESULT bool isZero2D () const noexcept { return (x == 0.0f && y == 0.0f); }

  inline TVec &operator += (const TVec &v) noexcept { x += v.x; y += v.y; z += v.z; return *this; }
  inline TVec &operator -= (const TVec &v) noexcept { x -= v.x; y -= v.y; z -= v.z; return *this; }
  inline TVec &operator *= (float scale) noexcept { x *= scale; y *= scale; z *= scale; return *this; }
  inline TVec &operator /= (float scale) noexcept {
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

  inline TVec operator + (void) const noexcept { return *this; }
  inline TVec operator - (void) const noexcept { return TVec(-x, -y, -z); }

#ifdef USE_FAST_INVSQRT
  inline VVA_CHECKRESULT float invlength () const noexcept { return fastInvSqrtf(VSUM3(x*x, y*y, z*z)); }
  inline VVA_CHECKRESULT float invlength2D () const noexcept { return fastInvSqrtf(VSUM2(x*x, y*y)); }
#else
  inline VVA_CHECKRESULT float invlength () const noexcept { return 1.0f/sqrtf(VSUM3(x*x, y*y, z*z)); }
  inline VVA_CHECKRESULT float invlength2D () const noexcept { return 1.0f/sqrtf(VSUM2(x*x, y*y)); }
#endif

  inline VVA_CHECKRESULT float Length () const noexcept { return sqrtf(VSUM3(x*x, y*y, z*z)); }
  inline VVA_CHECKRESULT float length () const noexcept { return sqrtf(VSUM3(x*x, y*y, z*z)); }

  inline VVA_CHECKRESULT float Length2D () const noexcept { return sqrtf(VSUM2(x*x, y*y)); }
  inline VVA_CHECKRESULT float length2D () const noexcept { return sqrtf(VSUM2(x*x, y*y)); }

  inline VVA_CHECKRESULT float LengthSquared () const noexcept { return VSUM3(x*x, y*y, z*z); }
  inline VVA_CHECKRESULT float lengthSquared () const noexcept { return VSUM3(x*x, y*y, z*z); }

  inline VVA_CHECKRESULT float Length2DSquared () const noexcept { return VSUM2(x*x, y*y); }
  inline VVA_CHECKRESULT float length2DSquared () const noexcept { return VSUM2(x*x, y*y); }

  inline VVA_CHECKRESULT float DistanceTo (const TVec &v) const noexcept { return sqrtf(VSUM3((x-v.x)*(x-v.x), (y-v.y)*(y-v.y), (z-v.z)*(z-v.z))); }
  inline VVA_CHECKRESULT float DistanceTo2D (const TVec &v) const noexcept { return sqrtf(VSUM2((x-v.x)*(x-v.x), (y-v.y)*(y-v.y))); }

  inline VVA_CHECKRESULT float distanceTo (const TVec &v) const noexcept { return sqrtf(VSUM3((x-v.x)*(x-v.x), (y-v.y)*(y-v.y), (z-v.z)*(z-v.z))); }
  inline VVA_CHECKRESULT float distanceTo2D (const TVec &v) const noexcept { return sqrtf(VSUM2((x-v.x)*(x-v.x), (y-v.y)*(y-v.y))); }

  inline void normaliseInPlace () noexcept { const float invlen = invlength(); x *= invlen; y *= invlen; z *= invlen; }
  inline void normalise2DInPlace () noexcept { const float invlen = invlength2D(); x *= invlen; y *= invlen; z = 0.0f; }

  inline VVA_CHECKRESULT TVec Normalised () const noexcept { const float invlen = invlength(); return TVec(x*invlen, y*invlen, z*invlen); }
  inline VVA_CHECKRESULT TVec normalised () const noexcept { const float invlen = invlength(); return TVec(x*invlen, y*invlen, z*invlen); }

  inline VVA_CHECKRESULT TVec Normalised2D () const noexcept { const float invlen = invlength2D(); return TVec(x*invlen, y*invlen, 0.0f); }
  inline VVA_CHECKRESULT TVec normalised2D () const noexcept { const float invlen = invlength2D(); return TVec(x*invlen, y*invlen, 0.0f); }

  inline VVA_CHECKRESULT float Dot (const TVec &v2) const noexcept { return VSUM3(x*v2.x, y*v2.y, z*v2.z); }
  inline VVA_CHECKRESULT float dot (const TVec &v2) const noexcept { return VSUM3(x*v2.x, y*v2.y, z*v2.z); }

  inline VVA_CHECKRESULT float DotV2Neg (const TVec &v2) const noexcept { return VSUM3(x*(-v2.x), y*(-v2.y), z*(-v2.z)); }
  inline VVA_CHECKRESULT float dotv2neg (const TVec &v2) const noexcept { return VSUM3(x*(-v2.x), y*(-v2.y), z*(-v2.z)); }

  inline VVA_CHECKRESULT float Dot2D (const TVec &v2) const noexcept { return VSUM2(x*v2.x, y*v2.y); }
  inline VVA_CHECKRESULT float dot2D (const TVec &v2) const noexcept { return VSUM2(x*v2.x, y*v2.y); }

  inline VVA_CHECKRESULT TVec Cross (const TVec &v2) const noexcept { return TVec(VSUM2(y*v2.z, -(z*v2.y)), VSUM2(z*v2.x, -(x*v2.z)), VSUM2(x*v2.y, -(y*v2.x))); }
  inline VVA_CHECKRESULT TVec cross (const TVec &v2) const noexcept { return TVec(VSUM2(y*v2.z, -(z*v2.y)), VSUM2(z*v2.x, -(x*v2.z)), VSUM2(x*v2.y, -(y*v2.x))); }

  // 2d cross product (z, as x and y are effectively zero in 2d)
  inline VVA_CHECKRESULT float Cross2D (const TVec &v2) const noexcept { return VSUM2(x*v2.y, -(y*v2.x)); }
  inline VVA_CHECKRESULT float cross2D (const TVec &v2) const noexcept { return VSUM2(x*v2.y, -(y*v2.x)); }

  // z is zero
  inline VVA_CHECKRESULT TVec mul2 (const float s) const noexcept { return TVec(x*s, y*s, 0.0f); }
  inline VVA_CHECKRESULT TVec mul3 (const float s) const noexcept { return TVec(x*s, y*s, z*s); }

  // returns projection of this vector onto `v`
  inline VVA_CHECKRESULT TVec projectTo (const TVec &v) const noexcept { return v.mul3(dot(v)/v.lengthSquared()); }
  inline VVA_CHECKRESULT TVec projectTo2D (const TVec &v) const noexcept { return v.mul2(dot2D(v)/v.length2DSquared()); }

  inline VVA_CHECKRESULT TVec sub2D (const TVec &v) const noexcept { return TVec(x-v.x, y-v.y, 0.0f); }

  // dir must be normalised, angle must be valid
  inline VVA_CHECKRESULT bool IsInSpotlight (const TVec &origin, const TVec &dir, const float angle) const noexcept {
    TVec surfaceToLight = TVec(-(origin.x-x), -(origin.y-y), -(origin.z-z));
    if (surfaceToLight.lengthSquared() <= 8.0f) return true;
    surfaceToLight.normaliseInPlace();
    const float ltangle = macos(surfaceToLight.dot(dir));
    return (ltangle < angle);
  }

  // dir must be normalised, angle must be valid
  // returns cone light attenuation multiplier in range [0..1]
  inline VVA_CHECKRESULT float CalcSpotlightAttMult (const TVec &origin, const TVec &dir, const float angle) const noexcept {
    TVec surfaceToLight = TVec(-(origin.x-x), -(origin.y-y), -(origin.z-z));
    if (surfaceToLight.lengthSquared() <= 8.0f) { return 1.0f; }
    surfaceToLight.normaliseInPlace();
    const float ltangle = macos(surfaceToLight.dot(dir));
    return (ltangle < angle ? sinf(midval(0.0f, (angle-ltangle)/angle, 1.0f)*((float)M_PI/2.0f)) : 0.0f);
  }

  // range must be valid
  inline void clampScaleInPlace (float fabsmax) noexcept {
    if (isValid()) {
      if (fabsmax > 0.0f && (fabs(x) > fabsmax || fabs(y) > fabsmax || fabs(z) > fabsmax)) {
        // need to rescale
        // find abs of the longest axis
        float vv;
        float absmax = fabs(x);
        // y
        vv = fabs(y);
        if (vv > absmax) absmax = vv;
        // z
        vv = fabs(z);
        if (vv > absmax) absmax = vv;
        // now rescale to range size
        const float rngscale = fabsmax/absmax;
        x *= rngscale;
        y *= rngscale;
        z *= rngscale;
      }
    } else {
      x = y = z = 0.0f;
    }
  }
};

static_assert(__builtin_offsetof(TVec, y) == __builtin_offsetof(TVec, x)+sizeof(float), "TVec layout fail (0)");
static_assert(__builtin_offsetof(TVec, z) == __builtin_offsetof(TVec, y)+sizeof(float), "TVec layout fail (1)");
static_assert(sizeof(TVec) == sizeof(float)*3, "TVec layout fail (2)");

static inline VVA_OKUNUSED vuint32 GetTypeHash (const TVec &v) noexcept { return joaatHashBuf(&v, 3*sizeof(float)); }


static VVA_OKUNUSED inline TVec operator + (const TVec &v1, const TVec &v2) noexcept { return TVec(VSUM2(v1.x, v2.x), VSUM2(v1.y, v2.y), VSUM2(v1.z, v2.z)); }
static VVA_OKUNUSED inline TVec operator - (const TVec &v1, const TVec &v2) noexcept { return TVec(VSUM2(v1.x, -(v2.x)), VSUM2(v1.y, -(v2.y)), VSUM2(v1.z, -(v2.z))); }

static VVA_OKUNUSED inline TVec operator * (const TVec &v, float s) noexcept { return TVec(s*v.x, s*v.y, s*v.z); }
static VVA_OKUNUSED inline TVec operator * (float s, const TVec &v) noexcept { return TVec(s*v.x, s*v.y, s*v.z); }
static VVA_OKUNUSED inline TVec operator / (const TVec &v, float s) noexcept { s = 1.0f/s; if (!isFiniteF(s)) s = 0.0f; return TVec(v.x*s, v.y*s, v.z*s); }

static VVA_OKUNUSED inline bool operator == (const TVec &v1, const TVec &v2) noexcept { return (v1.x == v2.x && v1.y == v2.y && v1.z == v2.z); }
static VVA_OKUNUSED inline bool operator != (const TVec &v1, const TVec &v2) noexcept { return (v1.x != v2.x || v1.y != v2.y || v1.z != v2.z); }

//static VVA_OKUNUSED inline float operator * (const TVec &a, const TVec &b) noexcept { return a.dot(b); }
//static VVA_OKUNUSED inline TVec operator ^ (const TVec &a, const TVec &b) noexcept { return a.cross(b); }
//static VVA_OKUNUSED inline TVec operator % (const TVec &a, const TVec &b) noexcept { return a.cross(b); }

static VVA_OKUNUSED VVA_CHECKRESULT inline float Length (const TVec &v) noexcept { return v.length(); }
static VVA_OKUNUSED VVA_CHECKRESULT inline float length (const TVec &v) noexcept { return v.length(); }
static VVA_OKUNUSED VVA_CHECKRESULT inline float Length2D (const TVec &v) noexcept { return v.length2D(); }
static VVA_OKUNUSED VVA_CHECKRESULT inline float length2D (const TVec &v) noexcept { return v.length2D(); }

static VVA_OKUNUSED VVA_CHECKRESULT inline float LengthSquared (const TVec &v) noexcept { return v.lengthSquared(); }
static VVA_OKUNUSED VVA_CHECKRESULT inline float lengthSquared (const TVec &v) noexcept { return v.lengthSquared(); }
static VVA_OKUNUSED VVA_CHECKRESULT inline float Length2DSquared (const TVec &v) noexcept { return v.length2DSquared(); }
static VVA_OKUNUSED VVA_CHECKRESULT inline float length2DSquared (const TVec &v) noexcept { return v.length2DSquared(); }

static VVA_OKUNUSED VVA_CHECKRESULT inline TVec Normalise (const TVec &v) noexcept { return v.normalised(); }
static VVA_OKUNUSED VVA_CHECKRESULT inline TVec normalise (const TVec &v) noexcept { return v.normalised(); }

static VVA_OKUNUSED VVA_CHECKRESULT inline TVec normalise2D (const TVec &v) noexcept { return v.normalised2D(); }

static VVA_OKUNUSED VVA_CHECKRESULT inline float DotProduct (const TVec &v1, const TVec &v2) noexcept { return v1.dot(v2); }
static VVA_OKUNUSED VVA_CHECKRESULT inline float dot (const TVec &v1, const TVec &v2) noexcept { return v1.dot(v2); }

static VVA_OKUNUSED VVA_CHECKRESULT inline float DotProductV2Neg (const TVec &v1, const TVec &v2) noexcept { return v1.dotv2neg(v2); }
static VVA_OKUNUSED VVA_CHECKRESULT inline float dotv2neg (const TVec &v1, const TVec &v2) noexcept { return v1.dotv2neg(v2); }

static VVA_OKUNUSED VVA_CHECKRESULT inline float DotProduct2D (const TVec &v1, const TVec &v2) noexcept { return v1.dot2D(v2); }
static VVA_OKUNUSED VVA_CHECKRESULT inline float dot2D (const TVec &v1, const TVec &v2) noexcept { return v1.dot2D(v2); }

static VVA_OKUNUSED VVA_CHECKRESULT inline TVec CrossProduct (const TVec &v1, const TVec &v2) noexcept { return v1.cross(v2); }
static VVA_OKUNUSED VVA_CHECKRESULT inline TVec cross (const TVec &v1, const TVec &v2) noexcept { return v1.cross(v2); }

// returns signed magnitude of cross-product (z, as x and y are effectively zero in 2d)
static VVA_OKUNUSED VVA_CHECKRESULT inline float CrossProduct2D (const TVec &v1, const TVec &v2) noexcept { return v1.cross2D(v2); }
static VVA_OKUNUSED VVA_CHECKRESULT inline float cross2D (const TVec &v1, const TVec &v2) noexcept { return v1.cross2D(v2); }

static VVA_OKUNUSED inline VStream &operator << (VStream &Strm, TVec &v) { return Strm << v.x << v.y << v.z; }


void AngleVectors (const TAVec &angles, TVec &forward, TVec &right, TVec &up) noexcept;
void AngleRightVector (const TAVec &angles, TVec &right) noexcept;
void AngleVector (const TAVec &angles, TVec &forward) noexcept;
void YawVectorRight (float yaw, TVec &right) noexcept;
void VectorAngles (const TVec &vec, TAVec &angles) noexcept;
void VectorsAngles (const TVec &forward, const TVec &right, const TVec &up, TAVec &angles) noexcept;

VVA_CHECKRESULT TVec RotateVectorAroundVector (const TVec &Vector, const TVec &Axis, float Angle) noexcept;

void RotatePointAroundVector (TVec &dst, const TVec &dir, const TVec &point, float degrees) noexcept;
// sets axis[1] and axis[2]
void RotateAroundDirection (TVec axis[3], float yaw) noexcept;

// given a normalized forward vector, create two other perpendicular vectors
void MakeNormalVectors (const TVec &forward, TVec &right, TVec &up) noexcept;

static inline VVA_OKUNUSED void AngleVectorPitch (const float pitch, TVec &forward) noexcept {
  msincos(pitch, &forward.z, &forward.x);
  forward.y = 0.0f;
  forward.z = -forward.z;
  /*
  forward.x = mcos(pitch);
  forward.y = 0.0f;
  forward.z = -msin(pitch);
  */
}

static inline VVA_OKUNUSED TVec AngleVectorYaw (const float yaw) noexcept {
  float sy, cy;
  msincos(yaw, &sy, &cy);
  return TVec(cy, sy, 0.0f);
}

static inline VVA_OKUNUSED float VectorAngleYaw (const TVec &vec) noexcept {
  const float fx = vec.x;
  const float fy = vec.y;
  const float len2d = VSUM2(fx*fx, fy*fy);
  return (len2d < 0.0001f ? 0.0f : matan(fy, fx));
}

static inline VVA_OKUNUSED float VectorAnglePitch (const TVec &vec) noexcept {
  const float fx = vec.x;
  const float fy = vec.y;
  const float len2d = VSUM2(fx*fx, fy*fy);
  return (len2d < 0.0001f ? (vec.z > 0.0f ? 90 : 270) : -matan(vec.z, sqrtf(len2d)));
}


// ////////////////////////////////////////////////////////////////////////// //
// Ax+By+Cz=D (ABC is normal, D is distance)
// this the same as in Quake engine, so i can... borrow code from it ;-)
class /*__attribute__((packed))*/ TPlane {
public:
  TVec normal;
  float dist;

public:
  // put here because i canot find a better place for it yet
  static VVA_OKUNUSED inline void CreateBBox (float bbox[6], const TVec &v0, const TVec &v1) noexcept {
    if (v0.x < v1.x) {
      bbox[BOX3D_MINX] = v0.x;
      bbox[BOX3D_MAXX] = v1.x;
    } else {
      bbox[BOX3D_MINX] = v1.x;
      bbox[BOX3D_MAXX] = v0.x;
    }
    if (v0.y < v1.y) {
      bbox[BOX3D_MINY] = v0.y;
      bbox[BOX3D_MAXY] = v1.y;
    } else {
      bbox[BOX3D_MINY] = v1.y;
      bbox[BOX3D_MAXY] = v0.y;
    }
    if (v0.z < v1.z) {
      bbox[BOX3D_MINZ] = v0.z;
      bbox[BOX3D_MAXZ] = v1.z;
    } else {
      bbox[BOX3D_MINZ] = v1.z;
      bbox[BOX3D_MAXZ] = v0.z;
    }
  }

public:
  //TPlane () : TVec(1.0f, 0.0f, 0.0f), dist(0.0f) {}
  //TPlane (ENoInit) {}

  inline VVA_CHECKRESULT bool isValid () const noexcept { return (normal.isValid() && !normal.isZero() && isFiniteF(dist)); }
  inline VVA_CHECKRESULT bool isVertical () const noexcept { return (normal.z == 0.0f); }

  inline void Set (const TVec &Anormal, float Adist) noexcept {
    normal = Anormal;
    dist = Adist;
  }

  inline void SetAndNormalise (const TVec &Anormal, float Adist) noexcept {
    normal = Anormal;
    dist = Adist;
    Normalise();
    if (!normal.isValid() || normal.isZero()) {
      //k8: what to do here?!
      normal = TVec(1.0f, 0.0f, 0.0f);
      dist = 0.0f;
      return;
    }
  }

  // initialises vertical plane from point and direction
  inline void SetPointDirXY (const TVec &point, const TVec &dir) noexcept {
    normal = TVec(dir.y, -dir.x, 0.0f);
    // use some checks to avoid floating point inexactness on axial planes
    if (!normal.x) {
      if (!normal.y) {
        //k8: what to do here?!
        normal = TVec(1.0f, 0.0f, 0.0f);
        dist = 0.0f;
        return;
      }
      // vertical
      normal.y = (normal.y < 0 ? -1.0f : 1.0f);
    } else if (!normal.y) {
      // horizontal
      normal.x = (normal.x < 0 ? -1.0f : 1.0f);
    } else {
      // sloped
      normal.normaliseInPlace();
    }
    dist = DotProduct(point, normal);
  }

  // initialises "full" plane from point and direction
  // `norm` must be normalized, both vectors must be valid
  inline void SetPointNormal3D (const TVec &point, const TVec &norm) noexcept {
    normal = norm;
    dist = DotProduct(point, normal);
  }

  // initialises "full" plane from point and direction
  inline void SetPointNormal3DSafe (const TVec &point, const TVec &norm) noexcept {
    if (norm.isValid() && point.isValid() && !norm.isZero()) {
      normal = norm.normalised();
      if (normal.isValid() && !normal.isZero()) {
        dist = DotProduct(point, normal);
      } else {
        //k8: what to do here?!
        normal = TVec(1.0f, 0.0f, 0.0f);
        dist = 0.0f;
      }
    } else {
      //k8: what to do here?!
      normal = TVec(1.0f, 0.0f, 0.0f);
      dist = 0.0f;
    }
  }

  // initialises vertical plane from 2 points
  inline void Set2Points (const TVec &v1, const TVec &v2) noexcept {
    SetPointDirXY(v1, v2-v1);
  }

  // the normal will point out of the clock for clockwise ordered points
  inline void SetFromTriangle (const TVec &a, const TVec &b, const TVec &c) noexcept {
    normal = (c-a).cross(b-a).normalised();
    dist = DotProduct(a, normal);
  }

  // WARNING! do not call this repeatedly, or on normalized plane!
  //          due to floating math inexactness, you will accumulate errors.
  inline void Normalise () noexcept {
    const float mag = normal.invlength();
    normal *= mag;
    // multiply by mag too, because we're doing "dot-dist", so
    // if our vector becomes smaller, our dist should become smaller too
    dist *= mag;
  }

  inline void flipInPlace () noexcept {
    normal = -normal;
    dist = -dist;
  }

  // *signed* distance from point to plane
  // plane must be normalized
  inline VVA_CHECKRESULT float PointDistance (const TVec &p) const noexcept {
    return DotProduct(p, normal)-dist;
  }

  // get z of point with given x and y coords
  // don't try to use it on a vertical plane
  inline VVA_CHECKRESULT float GetPointZ (float x, float y) const noexcept {
    return (VSUM3(dist, -(normal.x*x), -(normal.y*y))/normal.z);
  }

  inline VVA_CHECKRESULT float GetPointZRev (float x, float y) const noexcept {
    return (VSUM3(-dist, -(-normal.x*x), -(-normal.y*y))/(-normal.z));
  }

  inline VVA_CHECKRESULT float GetPointZ (const TVec &v) const noexcept {
    return GetPointZ(v.x, v.y);
  }

  inline VVA_CHECKRESULT float GetPointZRev (const TVec &v) const noexcept {
    return GetPointZRev(v.x, v.y);
  }


  // "land" point onto the plane
  // plane must be normalized
  inline VVA_CHECKRESULT TVec landAlongNormal (const TVec &point) const noexcept {
    const float pdist = DotProduct(point, normal)-dist;
    return (fabs(pdist) > 0.0001f ? point-normal*pdist : point);
  }

  // plane must be normalized
  inline VVA_CHECKRESULT TVec Project (const TVec &v) const noexcept {
    return v-(v-normal*dist).dot(normal)*normal;
  }


  // returns the point where the line p0-p1 intersects this plane
  // `p0` and `p1` must not be the same
  inline VVA_CHECKRESULT float LineIntersectTime (const TVec &p0, const TVec &p1, const float eps=0.0001f) const noexcept {
    const float dv = normal.dot(p1-p0);
    return (fabsf(dv) > eps ? (dist-normal.dot(p0))/dv : -666.0f);
  }

  // returns the point where the line p0-p1 intersects this plane
  // `p0` and `p1` must not be the same
  inline VVA_CHECKRESULT TVec LineIntersect (const TVec &p0, const TVec &p1, const float eps=0.0001f) const noexcept {
    const TVec dif = p1-p0;
    const float dv = normal.dot(dif);
    const float t = (fabsf(dv) > eps ? (dist-normal.dot(p0))/dv : 0.0f);
    return p0+(dif*t);
  }

  // returns the point where the line p0-p1 intersects this plane
  // `p0` and `p1` must not be the same
  inline VVA_CHECKRESULT bool LineIntersectEx (TVec &res, const TVec &p0, const TVec &p1, const float eps=0.0001f) const noexcept {
    const TVec dif = p1-p0;
    const float dv = normal.dot(dif);
    if (fabsf(dv) > eps) {
      const float t = (dist-normal.dot(p0))/normal.dot(dif);
      if (t < 0.0f) { res = p0; return false; }
      if (t > 1.0f) { res = p1; return false; }
      res = p0+(dif*t);
      return true;
    } else {
      res = p0;
      return false;
    }
  }


  // intersection of 3 planes, Graphics Gems 1 pg 305
  // not sure if it should be `dist` or `-dist` here for vavoom planes
  inline VVA_CHECKRESULT TVec IntersectionPoint (const TPlane &plane2, const TPlane &plane3) const noexcept {
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
  bool sweepSphere (const TVec &origin, const float radius, const TVec &amove, TVec *hitpos=nullptr, float *u=nullptr) const noexcept {
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
  inline VVA_CHECKRESULT int PointOnSide (const TVec &point) const noexcept {
    return (DotProduct(point, normal)-dist <= 0.0f);
  }

  // returns side 0 (front) or 1 (back, or on plane)
  inline VVA_CHECKRESULT int PointOnSideThreshold (const TVec &point) const noexcept {
    return (DotProduct(point, normal)-dist < 0.1f);
  }

  // returns side 0 (front) or 1 (back, or on plane)
  inline VVA_CHECKRESULT int PointOnSideThreshold (const TVec &point, const float thre) const noexcept {
    return (DotProduct(point, normal)-dist < thre);
  }

  // returns side 0 (front, or on plane) or 1 (back)
  // "fri" means "front inclusive"
  inline VVA_CHECKRESULT int PointOnSideFri (const TVec &point) const noexcept {
    return (DotProduct(point, normal)-dist < 0.0f);
  }

  // returns side 0 (front), 1 (back), or 2 (on)
  // used in line tracing (only)
  inline VVA_CHECKRESULT int PointOnSide2 (const TVec &point) const noexcept {
    const float dot = DotProduct(point, normal)-dist;
    return (dot < -0.1f ? 1 : dot > 0.1f ? 0 : 2);
  }

  // returns side 0 (front), 1 (back)
  // if at least some part of the sphere is on a front side, it means "front"
  inline VVA_CHECKRESULT int SphereOnSide (const TVec &center, float radius) const noexcept {
    return (DotProduct(center, normal)-dist <= -radius);
  }

  /*
  // returns side 0 (front), 1 (back)
  // if at least some part of the sphere is on a front side, it means "front"
  inline int SphereOnBackTh (const TVec &center, float radius) const {
    return (DotProduct(center, normal)-dist < -(radius+0.1f));
  }
  */

  inline VVA_CHECKRESULT bool SphereTouches (const TVec &center, float radius) const noexcept {
    return (fabsf(DotProduct(center, normal)-dist) < radius);
  }

  // returns side 0 (front), 1 (back), or 2 (collides)
  inline VVA_CHECKRESULT int SphereOnSide2 (const TVec &center, float radius) const noexcept {
    const float d = DotProduct(center, normal)-dist;
    return (d < -radius ? 1 : d > radius ? 0 : 2);
  }

  // returns "AABB reject point"
  // i.e. box point that is furthest from the plane
  inline VVA_CHECKRESULT TVec get3DBBoxRejectPoint (const float bbox[6]) const noexcept {
    return TVec(
      bbox[BOX3D_X+(normal.x < 0 ? BOX3D_MINIDX : BOX3D_MAXIDX)],
      bbox[BOX3D_Y+(normal.y < 0 ? BOX3D_MINIDX : BOX3D_MAXIDX)],
      bbox[BOX3D_Z+(normal.z < 0 ? BOX3D_MINIDX : BOX3D_MAXIDX)]);
  }

  // returns "AABB accept point"
  // i.e. box point that is closest to the plane
  inline VVA_CHECKRESULT TVec get3DBBoxAcceptPoint (const float bbox[6]) const noexcept {
    return TVec(
      bbox[BOX3D_X+(normal.x < 0 ? BOX3D_MAXIDX : BOX3D_MINIDX)],
      bbox[BOX3D_Y+(normal.y < 0 ? BOX3D_MAXIDX : BOX3D_MINIDX)],
      bbox[BOX3D_Z+(normal.z < 0 ? BOX3D_MAXIDX : BOX3D_MINIDX)]);
  }

  inline VVA_CHECKRESULT TVec get2DBBoxRejectPoint (const float bbox2d[4], const float minz=0.0f, const float maxz=0.0f) const noexcept {
    return TVec(
      bbox2d[normal.x < 0 ? BOX2D_LEFT : BOX2D_RIGHT],
      bbox2d[normal.y < 0 ? BOX2D_BOTTOM : BOX2D_TOP],
      (normal.z < 0 ? minz : maxz));
  }

  inline VVA_CHECKRESULT TVec get2DBBoxAcceptPoint (const float bbox2d[4], const float minz=0.0f, const float maxz=0.0f) const noexcept {
    return TVec(
      bbox2d[normal.x < 0 ? BOX2D_RIGHT : BOX2D_LEFT],
      bbox2d[normal.y < 0 ? BOX2D_TOP : BOX2D_BOTTOM],
      (normal.z < 0 ? maxz : minz));
  }

  // returns `false` if the box fully is on the back side of the plane
  inline VVA_CHECKRESULT bool checkBox (const float bbox[6]) const noexcept {
    // check reject point
    return (DotProduct(normal, get3DBBoxRejectPoint(bbox))-dist > 0.0f); // at least partially on a front side?
  }

  // WARNING! make sure that the following constants are in sync with `TFrustum` ones!
  enum { OUTSIDE = 0, INSIDE = 1, PARTIALLY = -1 };

  // returns one of TFrustum::OUTSIDE, TFrustum::INSIDE, TFrustum::PARIALLY
  // if the box is touching the plane from inside, it is still assumed to be inside
  inline VVA_CHECKRESULT int checkBoxEx (const float bbox[6]) const noexcept {
    // check reject point
    if (DotProduct(normal, get3DBBoxRejectPoint(bbox))-dist <= 0.0f) return OUTSIDE; // entire box on a back side
    // check accept point
    // if accept point on another side (or on plane), assume intersection
    return (DotProduct(normal, get3DBBoxAcceptPoint(bbox))-dist < 0.0f ? PARTIALLY : INSIDE);
  }

  // returns `false` if the rect is on the back side of the plane
  inline VVA_CHECKRESULT bool checkRect (const TVec &v0, const TVec &v1) const noexcept {
    //FIXME: this can be faster
    float bbox[6];
    CreateBBox(bbox, v0, v1);
    return checkBox(bbox);
  }

  // returns one of OUTSIDE, INSIDE, PARIALLY
  inline VVA_CHECKRESULT int checkRectEx (const TVec &v0, const TVec &v1) const noexcept {
    //FIXME: this can be faster
    float bbox[6];
    CreateBBox(bbox, v0, v1);
    return checkBoxEx(bbox);
  }

  // this is the slow, general version
  // it does the same accept/reject check, but returns this:
  //   0: Quake source says that this can't happen
  //   1: in front
  //   2: in back
  //   3: in both
  // i.e.
  //   bit 0 is set if some part of the cube is in front, and
  //   bit 1 is set if some part of the cube is in back
  VVA_CHECKRESULT unsigned BoxOnPlaneSide (const TVec &emins, const TVec &emaxs) const noexcept;

  // this is used in `ClipPoly`
  // all data is malloced, so you'd better keep this between calls to avoid excessive allocations
  struct ClipWorkData {
  private:
    enum { INLINE_SIZE = 42 };
    int inlsides[INLINE_SIZE];
    float inldots[INLINE_SIZE];

  public:
    int *sides;
    float *dots;
    int tbsize;

    inline ClipWorkData () noexcept : sides(&inlsides[0]), dots(&inldots[0]), tbsize(INLINE_SIZE) {}
    inline ~ClipWorkData () noexcept { clear(); }
    // no copies
    ClipWorkData (const ClipWorkData &) = delete;
    ClipWorkData & operator = (const ClipWorkData &) = delete;

    inline void clear () noexcept {
      if (sides && sides != &inlsides[0]) Z_Free(sides);
      sides = &inlsides[0];
      if (dots && dots != &inldots[0]) Z_Free(dots);
      dots = &inldots[0];
      tbsize = INLINE_SIZE;
    }

    inline void ensure (int newsize) noexcept {
      if (tbsize < newsize) {
        tbsize = (newsize|0x7f)+1;
        sides = (int *)Z_Realloc(sides, tbsize*sizeof(sides[0]));
        dots = (float *)Z_Realloc(dots, tbsize*sizeof(dots[0]));
      }
    }
  };

  // clip convex polygon to this plane
  // returns number of new vertices (it can be 0 if the poly is completely clipped away)
  // `dest` should have room for at least `vcount+1` vertices, and should not be equal to `src`
  // precondition: vcount >= 3
  int ClipPoly (ClipWorkData &wdata, TVec *dest, const TVec *src, int vcount, const float eps=0.1f) const noexcept;
};

static_assert(__builtin_offsetof(TPlane, dist) == __builtin_offsetof(TPlane, normal.z)+sizeof(float), "TPlane layout fail (0)");
static_assert(sizeof(TPlane) == sizeof(float)*4, "TPlane layout fail (1)");

static inline VVA_OKUNUSED vuint32 GetTypeHash (const TPlane &v) noexcept { return joaatHashBuf(&v, 4*sizeof(float)); }


// ////////////////////////////////////////////////////////////////////////// //
class TClipPlane : public TPlane {
public:
  unsigned clipflag;

public:
  //TClipPlane () : TPlane(E_NoInit) { clipflag = 0; }
  //TClipPlane (ENoInit) : TPlane(E_NoInit) {}

  inline bool isValid () const noexcept { return !!clipflag; }
  inline void invalidate () noexcept { clipflag = 0; }

  inline TClipPlane &operator = (const TPlane &p) noexcept {
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
  inline TClipParam () noexcept : width(0), height(0), fov(0.0f), pixelAspect(1.0f) {}
  inline TClipParam (int awidth, int aheight, float afov, float apixelAspect=1.0f) noexcept : width(awidth), height(aheight), fov(afov), pixelAspect(apixelAspect) {}

  inline bool isValid () const noexcept { return (width > 0 && height > 0 && isFiniteF(fov) && fov > 0.0f && isFiniteF(pixelAspect) && pixelAspect > 0.0f); }

  inline bool operator == (const TClipParam &b) const noexcept {
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
  inline TFrustumParam () noexcept : origin(0.0f, 0.0f, 0.0f), angles(0.0f, 0.0f, 0.0f), vforward(0.0f, 0.0f, 0.0f), vright(0.0f, 0.0f, 0.0f), vup(0.0f, 0.0f, 0.0f) {}
  inline TFrustumParam (const TVec &aorigin, const TAVec &aangles, const TVec &vf, const TVec &vr, const TVec &vu) noexcept : origin(aorigin), angles(aangles), vforward(vf), vright(vr), vup(vu) {}
  inline TFrustumParam (const TVec &aorigin, const TAVec &aangles) noexcept : origin(aorigin), angles(aangles) {
    if (aangles.isValid()) {
      AngleVectors(aangles, vforward, vright, vup);
    } else {
      vforward = TVec(0.0f, 0.0f, 0.0f);
      vright = TVec(0.0f, 0.0f, 0.0f);
      vup = TVec(0.0f, 0.0f, 0.0f);
    }
  }

  inline VVA_CHECKRESULT bool isValid () const noexcept {
    return
      origin.isValid() && angles.isValid() && vforward.isValid() && vright.isValid() && vup.isValid() &&
      !vforward.isZero() && !vright.isZero() && !vright.isZero();
  }

  inline VVA_CHECKRESULT bool needUpdate (const TVec &aorg, const TAVec &aangles) const noexcept {
    if (!isValid()) return true;
    return (aorg != origin || aangles != angles);
  }

  inline bool operator == (const TFrustumParam &b) const noexcept {
    if (!isValid() || !b.isValid()) return false; // never equal
    return (origin == b.origin && angles == b.angles && vforward == b.vforward && vright == b.vright && vup == b.vup);
  }

  inline void setup (const TVec &aorigin, const TAVec &aangles, const TVec &vf, const TVec &vr, const TVec &vu) noexcept {
    origin = aorigin;
    angles = aangles;
    vforward = vf;
    vright = vr;
    vup = vu;
  }

  inline void setup (const TVec &aorigin, const TAVec &aangles) noexcept {
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
  inline TClipBase () noexcept : fovx(0.0f), fovy(0.0f) {}
  inline TClipBase (int awidth, int aheight, float afov, float apixelAspect=1.0f) noexcept { setupViewport(awidth, aheight, afov, apixelAspect); }
  inline TClipBase (const float afovx, const float afovy) noexcept { setupFromFOVs(afovx, afovy); }
  inline TClipBase (const TClipParam &cp) noexcept { setupViewport(cp); }

  inline VVA_CHECKRESULT bool isValid () const noexcept { return (fovx != 0.0f); }

  inline void clear () noexcept { fovx = fovy = 0.0f; }

  inline VVA_CHECKRESULT const TVec &operator [] (size_t idx) const noexcept { vassert(idx < 4); return clipbase[idx]; }

  void setupFromFOVs (const float afovx, const float afovy) noexcept;

  void setupViewport (const TClipParam &cp) noexcept;
  void setupViewport (int awidth, int aheight, float afov, float apixelAspect=1.0f) noexcept;

  // WARNING! no checks!
  static inline void CalcFovXY (float *outfovx, float *outfovy, const int width, const int height, const float fov, const float pixelAspect=1.0f) noexcept {
    const float fovx = tanf(DEG2RADF(fov)/2.0f);
    if (outfovx) *outfovx = fovx;
    if (outfovy) *outfovy = fovx*height/width/pixelAspect;
  }

  static inline void CalcFovXY (float *outfovx, float *outfovy, const TClipParam &cp) noexcept {
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
  inline TFrustum () noexcept : planeCount(0) { clear(); }
  inline TFrustum (const TClipBase &clipbase, const TFrustumParam &fp, bool createbackplane=true, const float farplanez=0.0f) noexcept : planeCount(0) {
    setup(clipbase, fp, createbackplane, farplanez);
  }

  inline VVA_CHECKRESULT bool isValid () const noexcept { return (planeCount > 0); }

  inline void clear () noexcept { planeCount = 0; planes[0].clipflag = planes[1].clipflag = planes[2].clipflag = planes[3].clipflag = planes[4].clipflag = planes[5].clipflag = 0; }

  // for speed; direction vectors should correspond to angles
  void setup (const TClipBase &clipbase, const TFrustumParam &fp, bool createbackplane=true, const float farplanez=0.0f) noexcept;

  // if `farplanez` is non finite, or <= 0, remove far plane
  void setFarPlane (const TFrustumParam &fp, float farplanez) noexcept;

  // automatically called by `setup*()`
  void setupBoxIndicies () noexcept;

  void setupBoxIndiciesForPlane (unsigned pidx) noexcept;


  // returns `false` if the point is out of frustum
  VVA_CHECKRESULT bool checkPoint (const TVec &point, const unsigned mask=~0u) const noexcept;

  // returns `false` if the sphere is out of frustum
  VVA_CHECKRESULT bool checkSphere (const TVec &center, const float radius, const unsigned mask=~0u) const noexcept;

  // returns `false` if the box is out of frustum (or frustum is not valid)
  // bbox:
  //   [0] is minx
  //   [1] is miny
  //   [2] is minz
  //   [3] is maxx
  //   [4] is maxy
  //   [5] is maxz
  VVA_CHECKRESULT bool checkBox (const float bbox[6], const unsigned mask=~0u) const noexcept;

  // WARNING! make sure that the following constants are in sync with `TPlane` ones!
  enum { OUTSIDE = 0, INSIDE = 1, PARTIALLY = -1 };

  // 0: completely outside; >0: completely inside; <0: partially inside
  VVA_CHECKRESULT int checkBoxEx (const float bbox[6], const unsigned mask=~0u) const noexcept;

  VVA_CHECKRESULT bool checkVerts (const TVec *verts, const unsigned vcount, const unsigned mask=~0u) const noexcept;
  VVA_CHECKRESULT int checkVertsEx (const TVec *verts, const unsigned vcount, const unsigned mask=~0u) const noexcept;

  VVA_CHECKRESULT int checkQuadEx (const TVec &v1, const TVec &v2, const TVec &v3, const TVec &v4, const unsigned mask=~0u) const noexcept;
};


// ////////////////////////////////////////////////////////////////////////// //
// returns `false` on error (and zero `dst`)
static VVA_OKUNUSED inline bool ProjectPointOnPlane (TVec &dst, const TVec &p, const TVec &normal) noexcept {
  const float inv_denom = 1.0f/DotProduct(normal, normal);
  if (!isFiniteF(inv_denom)) { dst = TVec(0.0f, 0.0f, 0.0f); return false; } //k8: what to do here?
  const float d = DotProduct(normal, p)*inv_denom;
  dst = p-d*(normal*inv_denom);
  return true;
}

void PerpendicularVector (TVec &dst, const TVec &src) noexcept; // assumes "src" is normalised


// ////////////////////////////////////////////////////////////////////////// //
// sometimes subsector bbox has invalid z; this fixes it
static VVA_OKUNUSED inline void FixBBoxZ (float bbox[6]) noexcept {
  vassert(isFiniteF(bbox[2]));
  vassert(isFiniteF(bbox[3+2]));
  if (bbox[2] > bbox[3+2]) {
    const float tmp = bbox[2];
    bbox[2] = bbox[3+2];
    bbox[3+2] = tmp;
  }
}


// make sure that bbox min is lesser than bbox max
// UB if bbox coords are not finite (no checks!)
static VVA_OKUNUSED inline void SanitizeBBox3D (float bbox[6]) noexcept {
  if (bbox[BOX3D_MINX] > bbox[BOX3D_MAXX]) {
    const float tmp = bbox[BOX3D_MINX];
    bbox[BOX3D_MINX] = bbox[BOX3D_MAXX];
    bbox[BOX3D_MAXX] = tmp;
  }
  if (bbox[BOX3D_MINY] > bbox[BOX3D_MAXY]) {
    const float tmp = bbox[BOX3D_MINY];
    bbox[BOX3D_MINY] = bbox[BOX3D_MAXY];
    bbox[BOX3D_MAXY] = tmp;
  }
  if (bbox[BOX3D_MINZ] > bbox[BOX3D_MAXZ]) {
    const float tmp = bbox[BOX3D_MINZ];
    bbox[BOX3D_MINZ] = bbox[BOX3D_MAXZ];
    bbox[BOX3D_MAXZ] = tmp;
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// check to see if the sphere overlaps the AABB
static VVA_OKUNUSED VVA_CHECKRESULT inline bool CheckSphereVsAABB (const float bbox[6], const TVec &lorg, const float radius) noexcept {
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
static VVA_OKUNUSED VVA_CHECKRESULT inline bool CheckSphereVsAABBIgnoreZ (const float bbox[6], const TVec &lorg, const float radius) noexcept {
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


// considers the line to be infinite
// check the relationship between the given box and the partition
// line.  Returns -1 if box is on left side, +1 if box is on right
// size, or 0 if the line intersects the box.
int BoxOnLineSide2D (const float tmbox[4], TVec v1, TVec v2) noexcept;


//==========================================================================
//
//  PlaneAngles2D
//
//==========================================================================
static inline VVA_OKUNUSED VVA_CHECKRESULT
float PlaneAngles2D (const TPlane *from, const TPlane *to) noexcept {
  float afrom = VectorAngleYaw(from->normal);
  float ato = VectorAngleYaw(to->normal);
  return AngleMod(AngleMod(ato-afrom+180)-180);
}


//==========================================================================
//
//  PlaneAngles2DFlipTo
//
//==========================================================================
static inline VVA_OKUNUSED VVA_CHECKRESULT
float PlaneAngles2DFlipTo (const TPlane *from, const TPlane *to) noexcept {
  float afrom = VectorAngleYaw(from->normal);
  float ato = VectorAngleYaw(-to->normal);
  return AngleMod(AngleMod(ato-afrom+180)-180);
}


//==========================================================================
//
//  IsCircleTouchBox2D
//
//==========================================================================
static inline VVA_OKUNUSED VVA_CHECKRESULT
bool IsCircleTouchBox2D (const float cx, const float cy, float radius, const float bbox2d[4]) noexcept {
  if (radius < 1.0f) return false;

  const float bbwHalf = (bbox2d[BOX2D_RIGHT]+bbox2d[BOX2D_LEFT])*0.5f;
  const float bbhHalf = (bbox2d[BOX2D_TOP]+bbox2d[BOX2D_BOTTOM])*0.5f;

  // the distance between the center of the circle and the center of the box
  // not a const, because we'll modify the variables later
  float cdistx = fabsf(cx-(bbox2d[BOX2D_LEFT]+bbwHalf));
  float cdisty = fabsf(cy-(bbox2d[BOX2D_BOTTOM]+bbhHalf));

  // easy cases: either completely outside, or completely inside
  if (cdistx > bbwHalf+radius || cdisty > bbhHalf+radius) return false;
  if (cdistx <= bbwHalf || cdisty <= bbhHalf) return true;

  // hard case: touching a corner
  cdistx -= bbwHalf;
  cdisty -= bbhHalf;
  const float cdistsq = cdistx*cdistx+cdisty*cdisty;
  return (cdistsq <= radius*radius);
}
