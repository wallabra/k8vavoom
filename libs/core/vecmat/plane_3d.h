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

  inline bool isFloor () const noexcept { return (normal.z > 0.0f); }
  inline bool isCeiling () const noexcept { return (normal.z < 0.0f); }
  inline bool isSlope () const noexcept { return (fabsf(normal.z) != 1.0f); }

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

  public:
    VV_DISABLE_COPY(ClipWorkData)

    inline ClipWorkData () noexcept : sides(&inlsides[0]), dots(&inldots[0]), tbsize(INLINE_SIZE) {}
    inline ~ClipWorkData () noexcept { clear(); }

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

inline vuint32 GetTypeHash (const TPlane &v) noexcept { return joaatHashBuf(&v, 4*sizeof(float)); }

static inline VVA_OKUNUSED VVA_CHECKRESULT
float PlaneAngles2D (const TPlane *from, const TPlane *to) noexcept {
  float afrom = VectorAngleYaw(from->normal);
  float ato = VectorAngleYaw(to->normal);
  return AngleMod(AngleMod(ato-afrom+180)-180);
}

static inline VVA_OKUNUSED VVA_CHECKRESULT
float PlaneAngles2DFlipTo (const TPlane *from, const TPlane *to) noexcept {
  float afrom = VectorAngleYaw(from->normal);
  float ato = VectorAngleYaw(-to->normal);
  return AngleMod(AngleMod(ato-afrom+180)-180);
}
