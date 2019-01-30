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

class VViewClipper {
private:
  struct VClipNode;

  VClipNode *FreeClipNodes;
  VClipNode *ClipHead;
  VClipNode *ClipTail;
  TVec Origin;
  VLevel *Level;

  VClipNode *NewClipNode ();
  void RemoveClipNode (VClipNode *Node);
  void DoAddClipRange (float From, float To);
  bool DoIsRangeVisible (float From, float To);

public:
  VViewClipper ();
  ~VViewClipper ();
  void ClearClipNodes (const TVec &AOrigin, VLevel *ALevel);
  void ClipInitFrustrumRange (const TAVec &viewangles, const TVec &viewforward,
                              const TVec &viewright, const TVec &viewup,
                              float fovx, float fovy);
  void ClipToRanges (const VViewClipper &Range);
  void AddClipRange (float From, float To);
  bool IsRangeVisible (float From, float To);
  bool ClipIsFull ();
  inline float PointToClipAngle (const TVec &Pt) const { float Ret = matan(Pt.y-Origin.y, Pt.x-Origin.x); if (Ret < 0.0f) Ret += 360.0f; return Ret; }
  bool ClipIsBBoxVisible (const float *BBox, bool shadowslight, const TVec &CurrLightPos = TVec(0, 0, 0), float CurrLightRadius=0);
  bool ClipCheckRegion (subregion_t *region, subsector_t *sub, bool shadowslight, const TVec &CurrLightPos = TVec(0, 0, 0), float CurrLightRadius=0);
  bool ClipCheckSubsector (subsector_t *Sub, bool shadowslight, const TVec &CurrLightPos = TVec(0, 0, 0), float CurrLightRadius=0);
  void ClipAddSubsectorSegs (subsector_t *Sub, bool shadowslight, TPlane *Mirror=nullptr, const TVec &CurrLightPos = TVec(0, 0, 0), float CurrLightRadius=0);

private:
  void CheckAddClipSeg (const seg_t *line, bool shadowslight, TPlane *Mirror, const TVec &CurrLightPos, float CurrLightRadius);
};
