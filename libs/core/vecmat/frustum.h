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
// sometimes subsector bbox has invalid z; this fixes it
void FixBBoxZ (float bbox[6]) noexcept;

// make sure that bbox min is lesser than bbox max
// UB if bbox coords are not finite (no checks!)
void SanitizeBBox3D (float bbox[6]) noexcept;

// check to see if the sphere overlaps the AABB
VVA_CHECKRESULT bool CheckSphereVsAABB (const float bbox[6], const TVec &lorg, const float radius) noexcept;

// check to see if the sphere overlaps the AABB (ignore z coords)
VVA_CHECKRESULT bool CheckSphereVsAABBIgnoreZ (const float bbox[6], const TVec &lorg, const float radius) noexcept;

// check to see if the sphere overlaps the 2D AABB
VVA_CHECKRESULT bool CheckSphereVs2dAABB (const float bbox[4], const TVec &lorg, const float radius) noexcept;

// considers the line to be infinite
// check the relationship between the given box and the partition
// line.  Returns -1 if box is on left side, +1 if box is on right
// size, or 0 if the line intersects the box.
VVA_CHECKRESULT int BoxOnLineSide2D (const float tmbox[4], TVec v1, TVec v2) noexcept;

VVA_CHECKRESULT bool IsCircleTouchBox2D (const float cx, const float cy, float radius, const float bbox2d[4]) noexcept;
