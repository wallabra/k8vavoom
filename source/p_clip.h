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
#ifdef CLIENT
extern VCvarF r_lights_radius;
#endif


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

  inline bool ClipVerts (TVec &v1, TVec &v2, bool doClipVerts=true) const {
    // clip sectors that are behind rendered segs
    const TVec r1 = Origin-v1;
    const TVec r2 = Origin-v2;
    const float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), Origin);
    const float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), Origin);

    if (D1 < 0.0f && D2 < 0.0f) return false;
    if (doClipVerts) {
      // there might be a better method of doing this, but this one works for now...
           if (D1 > 0.0f && D2 <= 0.0f) v2 += (v2-v1)*D1/(D1-D2);
      else if (D2 > 0.0f && D1 <= 0.0f) v1 += (v1-v2)*D2/(D2-D1);
    }

    return true;
  }

  inline bool ClipVertsWithLight (TVec &v1, TVec &v2, const TVec &CurrLightPos, float CurrLightRadius, bool doClipVerts=true) const {
#ifdef CLIENT
    // clip sectors that are behind rendered segs
    const TVec r1 = Origin-v1;
    const TVec r2 = Origin-v2;
    const float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), Origin);
    const float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), Origin);
    if (D1 > r_lights_radius && D2 > r_lights_radius) return false;

    const TVec rLight1 = CurrLightPos-v1;
    const TVec rLight2 = CurrLightPos-v2;
    const float DLight1 = DotProduct(Normalise(CrossProduct(rLight1, rLight2)), CurrLightPos);
    const float DLight2 = DotProduct(Normalise(CrossProduct(rLight2, rLight1)), CurrLightPos);

    const TVec rView1 = Origin-v1-CurrLightPos;
    const TVec rView2 = Origin-v2-CurrLightPos;
    const float DView1 = DotProduct(Normalise(CrossProduct(rView1, rView2)), Origin);
    const float DView2 = DotProduct(Normalise(CrossProduct(rView2, rView1)), Origin);

    if (D1 < 0.0f && D2 < 0.0f && DView1 < -CurrLightRadius && DView2 < -CurrLightRadius) return false;

    if ((DLight1 > CurrLightRadius && DLight2 > CurrLightRadius) ||
        (DLight1 < -CurrLightRadius && DLight2 < -CurrLightRadius))
    {
      return false;
    }

    if (doClipVerts) {
      // there might be a better method of doing this, but this one works for now...
           if (DLight1 > 0.0f && DLight2 <= 0.0f) v2 += (v2-v1)*D1/(D1-D2);
      else if (DLight2 > 0.0f && DLight1 <= 0.0f) v1 += (v1-v2)*D2/(D2-D1);
    }
#endif
    return true;
  }
};
