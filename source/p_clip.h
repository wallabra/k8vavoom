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
#define VAVOOM_CLIPPER_USE_FLOAT

#ifdef VAVOOM_CLIPPER_USE_FLOAT
# define  VVC_matan  matan
# define  VVC_AngleMod  AngleMod
# define  VVC_AngleMod180  AngleMod180
#else
# define  VVC_matan  matand
# define  VVC_AngleMod  AngleModD
# define  VVC_AngleMod180  AngleMod180D
#endif


class VViewClipper {
public:
#ifdef VAVOOM_CLIPPER_USE_FLOAT
  typedef float VFloat;
#else
  typedef double VFloat;
#endif

private:
  struct VClipNode;

  VClipNode *FreeClipNodes;
  VClipNode *ClipHead;
  VClipNode *ClipTail;
  TVec Origin;
  VLevel *Level;
  TFrustum Frustum; // why not?
  //TClipPlane FrustumTop;
  //TClipPlane FrustumBottom;

  VClipNode *NewClipNode ();
  void RemoveClipNode (VClipNode *Node);
  void DoAddClipRange (VFloat From, VFloat To);

  bool DoIsRangeVisible (const VFloat From, const VFloat To) const;

  bool IsRangeVisible (const VFloat From, const VFloat To) const;

  void AddClipRange (const VFloat From, const VFloat To);

  inline VFloat PointToClipAngle (const TVec &Pt) const {
    VFloat Ret = VVC_matan(Pt.y-Origin.y, Pt.x-Origin.x);
    if (Ret < (VFloat)0) Ret += (VFloat)360;
    return Ret;
  }

public:
  VViewClipper ();
  ~VViewClipper ();

  inline VLevel *GetLevel () { return Level; }
  inline VLevel *GetLevel () const { return Level; }

  inline const TVec &GetOrigin () const { return Origin; }
  inline const TFrustum &GetFrustum () const { return Frustum; }
  //inline const TClipPlane &GetFrustumTop () const { return FrustumTop; }
  //inline const TClipPlane &GetFrustumBottom () const { return FrustumBottom; }

  // 0: completely outside; >0: completely inside; <0: partially inside
  int CheckSubsectorFrustum (const subsector_t *sub) const;
  bool CheckSegFrustum (const seg_t *seg) const;

  void ClearClipNodes (const TVec &AOrigin, VLevel *ALevel);


  void ClipResetFrustumPlanes (); // call this after setting up frustum to disable height clipping

  // this is for the case when you already have direction vectors, to speed up things a little
  void ClipInitFrustumPlanes (const TAVec &viewangles, const TVec &viewforward, const TVec &viewright, const TVec &viewup,
                              const float fovx, const float fovy);

  void ClipInitFrustumPlanes (const TAVec &viewangles, const float fovx, const float fovy);

  // call this only on empty clipper (i.e. either new, or after calling `ClearClipNodes()`)
  // this is for the case when you already have direction vectors, to speed up things a little
  void ClipInitFrustumRange (const TAVec &viewangles, const TVec &viewforward,
                             const TVec &viewright, const TVec &viewup,
                             const float fovx, const float fovy);

  void ClipInitFrustumRange (const TAVec &viewangles, const float fovx, const float fovy);


  bool ClipIsFull () const;

  inline bool IsRangeVisible (const TVec &vfrom, const TVec &vto) const {
    return IsRangeVisible(PointToClipAngle(vfrom), PointToClipAngle(vto));
  }

  void ClipToRanges (const VViewClipper &Range);

  inline void AddClipRange (const TVec &vfrom, const TVec &vto) {
    AddClipRange(PointToClipAngle(vfrom), PointToClipAngle(vto));
  }

  bool ClipIsBBoxVisible (const float BBox[6]) const;
  bool ClipCheckRegion (const subregion_t *region, const subsector_t *sub) const;
  bool ClipCheckSubsector (const subsector_t *sub) const;
#ifdef CLIENT
  bool ClipLightIsBBoxVisible (const float BBox[6], const TVec &CurrLightPos, const float CurrLightRadius) const;
  bool ClipLightCheckRegion (const subregion_t *region, const subsector_t *sub, const TVec &CurrLightPos, const float CurrLightRadius) const;
  bool ClipLightCheckSubsector (const subsector_t *sub, const TVec &CurrLightPos, const float CurrLightRadius) const;
#endif

  void ClipAddSubsectorSegs (const subsector_t *sub, const TPlane *Mirror=nullptr);
#ifdef CLIENT
  void ClipLightAddSubsectorSegs (const subsector_t *sub, const TVec &CurrLightPos, const float CurrLightRadius, const TPlane *Mirror=nullptr);
#endif

private:
  void CheckAddClipSeg (const seg_t *line, const TPlane *Mirror=nullptr, bool skipSphereCheck=false);
#ifdef CLIENT
  void CheckLightAddClipSeg (const seg_t *line, const TVec &CurrLightPos, const float CurrLightRadius, const TPlane *Mirror, bool skipSphereCheck=false);
  // light radius should be valid
  int CheckSubsectorLight (const subsector_t *sub, const TVec &CurrLightPos, const float CurrLightRadius) const;
  //bool CheckSegLight (const seg_t *seg, const TVec &CurrLightPos, const float CurrLightRadius) const;
#endif
};
