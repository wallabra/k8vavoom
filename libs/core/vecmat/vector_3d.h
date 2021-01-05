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
//**  Copyright (C) 2018-2021 Ketmar Dark
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

// ////////////////////////////////////////////////////////////////////////// //
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
  BOX2D_TOP, // the top is greater than the bottom
  BOX2D_BOTTOM, // the bottom is lesser than the top
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
// 3d point/vector
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

  // this is what VavoomC wants: false is either zero, or invalid vector
  inline VVA_CHECKRESULT bool toBool () const noexcept {
    return
      isFiniteF(x) && isFiniteF(y) && isFiniteF(z) &&
      (x != 0.0f || y != 0.0f || z != 0.0f);
  }

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

  inline VVA_CHECKRESULT TVec abs () const noexcept { return TVec(fabs(x), fabs(y), fabs(z)); }

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

inline vuint32 GetTypeHash (const TVec &v) noexcept { return joaatHashBuf(&v, 3*sizeof(float)); }


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

static VVA_OKUNUSED VVA_CHECKRESULT inline TVec abs (const TVec &v1) noexcept { return v1.abs(); }

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

// returns `false` on error (and zero `dst`)
bool ProjectPointOnPlane (TVec &dst, const TVec &p, const TVec &normal) noexcept;

void PerpendicularVector (TVec &dst, const TVec &src) noexcept; // assumes "src" is normalised


// origin is center
static inline VVA_OKUNUSED void Create2DBBox (float box[4], const TVec &origin, float radius) noexcept {
  box[BOX2D_MAXY] = origin.y+radius;
  box[BOX2D_MINY] = origin.y-radius;
  box[BOX2D_MINX] = origin.x-radius;
  box[BOX2D_MAXX] = origin.x+radius;
}

// origin is center, bottom
static inline VVA_OKUNUSED void Create3DBBox (float box[6], const TVec &origin, float radius, float height) noexcept {
  box[BOX3D_MINX] = origin.x-radius;
  box[BOX3D_MINY] = origin.y-radius;
  box[BOX3D_MINZ] = origin.z;
  box[BOX3D_MAXX] = origin.x+radius;
  box[BOX3D_MAXY] = origin.y+radius;
  box[BOX3D_MAXZ] = origin.z+height;
}
