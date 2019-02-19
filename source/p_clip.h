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

  VClipNode *NewClipNode ();
  void RemoveClipNode (VClipNode *Node);
  void DoAddClipRange (VFloat From, VFloat To);

  bool DoIsRangeVisible (const VFloat From, const VFloat To) const;

public:
  VViewClipper ();
  ~VViewClipper ();

  void ClearClipNodes (const TVec &AOrigin, VLevel *ALevel);
  void ClipInitFrustrumRange (const TAVec &viewangles, const TVec &viewforward,
                              const TVec &viewright, const TVec &viewup,
                              const float fovx, const float fovy);

  void ClipToRanges (const VViewClipper &Range);
  void AddClipRange (const VFloat From, const VFloat To);

  bool IsRangeVisible (const VFloat From, const VFloat To) const;
  bool ClipIsFull () const;

  inline VFloat PointToClipAngle (const TVec &Pt) const {
    VFloat Ret = VVC_matan(Pt.y-Origin.y, Pt.x-Origin.x);
    if (Ret < (VFloat)0) Ret += (VFloat)360;
    return Ret;
  }

  bool ClipIsBBoxVisible (const float *BBox, bool shadowslight, const TVec &CurrLightPos = TVec(0, 0, 0), float CurrLightRadius=0) const;
  bool ClipCheckRegion (const subregion_t *region, const subsector_t *sub, bool shadowslight, const TVec &CurrLightPos = TVec(0, 0, 0), float CurrLightRadius=0) const;
  bool ClipCheckSubsector (const subsector_t *sub, bool shadowslight, const TVec &CurrLightPos = TVec(0, 0, 0), float CurrLightRadius=0) const;

  void ClipAddSubsectorSegs (const subsector_t *sub, bool shadowslight, const TPlane *Mirror=nullptr, const TVec &CurrLightPos = TVec(0, 0, 0), float CurrLightRadius=0);

  bool ClipCheckLine (const line_t *ldef) const;
  void ClipAddLine (const line_t *ldef);

private:
  void CheckAddClipSeg (const seg_t *line, bool shadowslight, const TPlane *Mirror, const TVec &CurrLightPos, float CurrLightRadius);
};
