//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
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
#define VAVOOM_CLIPPER_USE_FLOAT
//#define VAVOOM_CLIPPER_USE_REAL_ANGLES
#define VAVOOM_CLIPPER_USE_PSEUDO_INT

#ifdef VAVOOM_CLIPPER_USE_FLOAT
# define VVC_matan        matan
# define VVC_AngleMod     AngleMod
# define VVC_AngleMod180  AngleMod180
#else
# define VVC_matan        matand
# define VVC_AngleMod     AngleModD
# define VVC_AngleMod180  AngleMod180D
#endif

#ifdef VAVOOM_CLIPPER_USE_REAL_ANGLES
# define PointToClipAngleZeroOrigin  PointToRealAngleZeroOrigin
# define VV_CLIPPER_FULL_CHECK
#else
# define PointToClipAngleZeroOrigin  PointToPseudoAngleZeroOrigin
/*# define ClipperPseudoResolution  (0x4000000u)*/
/* k8: we don't really need alot of precision here */
/*     actually, as we're using segs, we can get away with *LESS* precision */
/*     but i am not sure */
/*     this will prolly break hard with retinas (or something with 2k+ resolution), but idc */
# define ClipperPseudoResolution  (0x3000u)
# ifdef VV_CLIPPER_FULL_CHECK
#  error "oops"
# endif
#endif

// sanitise defines
#if defined(VAVOOM_CLIPPER_USE_PSEUDO_INT) && !defined(VAVOOM_CLIPPER_USE_REAL_ANGLES)
# undef VAVOOM_CLIPPER_USE_PSEUDO_INT
#endif


class VViewClipper {
public:
#ifdef VAVOOM_CLIPPER_USE_FLOAT
  typedef float VFloat;
#else
  typedef double VFloat;
#endif

#ifdef VAVOOM_CLIPPER_USE_PSEUDO_INT
  static_assert(sizeof(unsigned int) == 4, "Oops! `unsigned int` should be at least 32 bits!");
  typedef unsigned int FromTo;
  typedef double Angle2FixIn;
#else
  typedef VFloat FromTo;
  typedef VFloat Angle2FixIn;
#endif

private:
  //struct VClipNode;
  struct VClipNode {
    FromTo From;
    FromTo To;
    VClipNode *Prev;
    VClipNode *Next;
  };

  VClipNode *FreeClipNodes;
  VClipNode *ClipHead;
  VClipNode *ClipTail;
  TVec Origin;
  float Radius; // for light clipper
  VLevel *Level;
  TFrustum Frustum; // why not?
  bool ClearClipNodesCalled;

  VClipNode *NewClipNode () noexcept;
  void RemoveClipNode (VClipNode *Node) noexcept;
  void DoAddClipRange (FromTo From, FromTo To) noexcept;

  bool DoIsRangeVisible (const FromTo From, const FromTo To) const noexcept;

  bool IsRangeVisibleAngle (const FromTo From, const FromTo To) const noexcept;

  void AddClipRangeAngle (const FromTo From, const FromTo To) noexcept;

  void DoRemoveClipRange (FromTo From, FromTo To) noexcept;
  void RemoveClipRangeAngle (const FromTo From, const FromTo To) noexcept;

protected:
  // we need them both!
  static inline VFloat PointToRealAngleZeroOrigin (const VFloat dx, const VFloat dy) noexcept {
    VFloat res = VVC_matan(dy, dx);
    if (res < (VFloat)0) res += (VFloat)360;
    return res;
  }

  // returns the pseudoangle between the line p1 to (infinity, p1.y) and the
  // line from p1 to p2. the pseudoangle has the property that the ordering of
  // points by true angle around p1 and ordering of points by pseudoangle are the
  // same. for clipping exact angles are not needed, only the ordering matters.
  // k8: i found this code in GZDoom, and changed it a little, to make it return [0..4]
#ifndef VAVOOM_CLIPPER_USE_REAL_ANGLES
  static inline FromTo PointToPseudoAngleZeroOrigin (const Angle2FixIn dx, const Angle2FixIn dy) noexcept {
    if (dx == 0 && dy == 0) return 0+1;
    #ifdef VAVOOM_CLIPPER_USE_PSEUDO_INT
      double res = dy/(fabs(dx)+fabs(dy));
      res = (dx < 0 ? 2-res : res)+1;
      return (FromTo)(res*(ClipperPseudoResolution-1u));
    #else
      const VFloat res = dy/(fabsf(dx)+fabsf(dy));
      return (dx < 0 ? 2-res : res)+1;
    #endif
  }
#endif

  static inline FromTo AngleToClipperAngle (const VFloat angle) noexcept {
    #ifdef VAVOOM_CLIPPER_USE_REAL_ANGLES
      #ifdef VAVOOM_CLIPPER_USE_FLOAT
        return AngleMod(angle);
      #else
        return AngleModD(angle);
      #endif
    #else
      #if defined(VAVOOM_CLIPPER_USE_PSEUDO_INT) || !defined(VAVOOM_CLIPPER_USE_FLOAT)
        double py, px;
        msincosd(angle, &py, &px);
        return PointToPseudoAngleZeroOrigin(px, py);
      #else
        float py, px;
        msincos(angle, &py, &px);
        return PointToPseudoAngleZeroOrigin(px, py);
      #endif
    #endif
  }

  inline FromTo PointToClipAngle (const TVec &p) const noexcept {
    return PointToClipAngleZeroOrigin((Angle2FixIn)p.x-(Angle2FixIn)Origin.x, (Angle2FixIn)p.y-(Angle2FixIn)Origin.y);
  }

public:
  rep_sector_t *RepSectors; // non-null for server

public:
  VViewClipper () noexcept;
  ~VViewClipper () noexcept;

  inline VLevel *GetLevel () noexcept { return Level; }
  inline VLevel *GetLevel () const noexcept { return Level; }

  inline const TVec &GetOrigin () const noexcept { return Origin; }
  inline const TFrustum &GetFrustum () const noexcept { return Frustum; }

  // 0: completely outside; >0: completely inside; <0: partially inside
  int CheckSubsectorFrustum (subsector_t *sub, const unsigned mask=~0u) const noexcept;
  // we have to pass subsector here, because of polyobjects
  bool CheckSegFrustum (const subsector_t *sub, const seg_t *seg, const unsigned mask=~0u) const noexcept;

  void ClearClipNodes (const TVec &AOrigin, VLevel *ALevel, float aradius=0.0f) noexcept;


  void ClipResetFrustumPlanes () noexcept; // call this after setting up frustum to disable height clipping

  // this is for the case when you already have direction vectors, to speed up things a little
  void ClipInitFrustumPlanes (const TAVec &viewangles, const TVec &viewforward, const TVec &viewright, const TVec &viewup,
                              const float fovx, const float fovy) noexcept;

  void ClipInitFrustumPlanes (const TAVec &viewangles, const float fovx, const float fovy) noexcept;

  // call this only on empty clipper (i.e. either new, or after calling `ClearClipNodes()`)
  // this is for the case when you already have direction vectors, to speed up things a little
  void ClipInitFrustumRange (const TAVec &viewangles, const TVec &viewforward,
                             const TVec &viewright, const TVec &viewup,
                             const float fovx, const float fovy) noexcept;

  void ClipInitFrustumRange (const TAVec &viewangles, const float fovx, const float fovy) noexcept;


#ifdef VV_CLIPPER_FULL_CHECK
  inline bool ClipIsFull () const noexcept {
    return (ClipHead && ClipHead->From == (VFloat)0 && ClipHead->To == (VFloat)360);
  }
#endif

  inline bool ClipIsEmpty () const noexcept {
    return !ClipHead;
  }

  inline bool IsRangeVisible (const TVec &vfrom, const TVec &vto) const noexcept {
    return IsRangeVisibleAngle(PointToClipAngle(vfrom), PointToClipAngle(vto));
  }

  void ClipToRanges (const VViewClipper &Range) noexcept;

  inline void AddClipRange (const TVec &vfrom, const TVec &vto) noexcept {
    AddClipRangeAngle(PointToClipAngle(vfrom), PointToClipAngle(vto));
  }

  bool ClipIsBBoxVisible (const float BBox[6]) const noexcept;
  bool ClipCheckRegion (const subregion_t *region, const subsector_t *sub) const noexcept;
  bool ClipCheckSubsector (const subsector_t *sub) noexcept;

#ifdef CLIENT
  bool ClipLightIsBBoxVisible (const float BBox[6]) const noexcept;
  bool ClipLightCheckRegion (const subregion_t *region, subsector_t *sub, bool asShadow) const noexcept;
  bool ClipLightCheckSubsector (subsector_t *sub, bool asShadow) const noexcept;
  // this doesn't do raduis and subsector checks: this is done in `CalcLightVisCheckNode()`
  bool ClipLightCheckSeg (const seg_t *seg, bool asShadow) const noexcept;
#endif

  void ClipAddLine (const TVec v1, const TVec v2) noexcept;
  void ClipAddBBox (const float BBox[6]) noexcept;
  void ClipAddSubsectorSegs (const subsector_t *sub, const TPlane *Mirror=nullptr, bool clipAll=false) noexcept;

#ifdef CLIENT
  // this doesn't check for radius
  void ClipLightAddSubsectorSegs (const subsector_t *sub, bool asShadow, const TPlane *Mirror=nullptr) noexcept;
#endif

  // debug
  void Dump () const noexcept;

public:
  void CheckAddClipSeg (const seg_t *line, const TPlane *Mirror=nullptr, bool clipAll=false) noexcept;
#ifdef CLIENT
  void CheckLightAddClipSeg (const seg_t *line, const TPlane *Mirror, bool asShadow) noexcept;
  // light radius should be valid
  int CheckSubsectorLight (subsector_t *sub) const noexcept;
#endif

public:
  static bool IsSegAClosedSomething (VLevel *level, const TFrustum *Frustum, const seg_t *seg, const TVec *lorg=nullptr, const float *lrad=nullptr) noexcept;
  static bool IsSegAClosedSomethingServer (VLevel *level, rep_sector_t *repsecs, const seg_t *seg) noexcept;
};
