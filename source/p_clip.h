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
  inline float PointToClipAngle (const TVec &Pt) const { float Ret = matan(Pt.y-Origin.y, Pt.x-Origin.x); if (Ret < 0.0) Ret += 360.0; return Ret; }
  bool ClipIsBBoxVisible (const float *BBox, bool shadowslight, const TVec &CurrLightPos = TVec(0, 0, 0), float CurrLightRadius=0);
  bool ClipCheckRegion (subregion_t *region, subsector_t *sub, bool shadowslight, const TVec &CurrLightPos = TVec(0, 0, 0), float CurrLightRadius=0);
  bool ClipCheckSubsector (subsector_t *Sub, bool shadowslight, const TVec &CurrLightPos = TVec(0, 0, 0), float CurrLightRadius=0);
  void ClipAddSubsectorSegs (subsector_t *Sub, bool shadowslight, TPlane *Mirror=nullptr, const TVec &CurrLightPos = TVec(0, 0, 0), float CurrLightRadius=0);

private:
  void CheckAddClipSeg (const seg_t *line, bool shadowslight, TPlane *Mirror, const TVec &CurrLightPos, float CurrLightRadius);

  static inline bool IsSegAClosedSomething (const seg_t *line) {
    if (line->linedef->flags&ML_3DMIDTEX) return false; // 3dmidtex never blocks anything

    auto fsec = line->linedef->frontsector;
    auto bsec = line->linedef->backsector;

    //if (!fsec || !bsec) return false;

    // only apply this to sectors without slopes
    if (fsec->floor.normal.z == 1.0 && bsec->floor.normal.z == 1.0 &&
        fsec->ceiling.normal.z == -1.0 && bsec->ceiling.normal.z == -1.0)
    {
      if (line->sidedef->TopTexture != -1 || // a line without top texture isn't a door
          line->sidedef->BottomTexture != -1 || // a line without bottom texture isn't an elevator/plat
          line->sidedef->MidTexture != -1) // a line without mid texture isn't a polyobj door
      {
        const TVec vv1 = *line->linedef->v1;
        const TVec vv2 = *line->linedef->v2;

        const float frontcz1 = fsec->ceiling.GetPointZ(vv1);
        const float frontcz2 = fsec->ceiling.GetPointZ(vv2);
        const float frontfz1 = fsec->floor.GetPointZ(vv1);
        const float frontfz2 = fsec->floor.GetPointZ(vv2);

        const float backcz1 = bsec->ceiling.GetPointZ(vv1);
        const float backcz2 = bsec->ceiling.GetPointZ(vv2);
        const float backfz1 = bsec->floor.GetPointZ(vv1);
        const float backfz2 = bsec->floor.GetPointZ(vv2);

        if ((backcz2 <= frontfz2 && backcz2 <= frontfz1 && backcz1 <= frontfz2 && backcz1 <= frontfz1) &&
            (frontcz2 <= backfz2 && frontcz2 <= backfz1 && frontcz1 <= backfz2 && frontcz1 <= backfz1))
        {
          // it's a closed door/elevator/polydoor
          return true;
        }
      }
    } else {
      // sloped
      if (((fsec->floor.maxz > bsec->ceiling.minz && fsec->ceiling.maxz < bsec->floor.minz) ||
           (fsec->floor.minz > bsec->ceiling.maxz && fsec->ceiling.minz < bsec->floor.maxz)) ||
          ((bsec->floor.maxz > fsec->ceiling.minz && bsec->ceiling.maxz < fsec->floor.minz) ||
           (bsec->floor.minz > fsec->ceiling.maxz && bsec->ceiling.minz < fsec->floor.maxz)))
      {
        return true;
      }
    }

    return false;
  }

  static inline bool IsSegAnOpenedSomething (const seg_t *line) {
    if (line->linedef->flags&ML_3DMIDTEX) return true; // 3dmidtex never blocks anything

    auto fsec = line->linedef->frontsector;
    auto bsec = line->linedef->backsector;

    //if (!fsec || !bsec) return false;

    // only apply this to sectors without slopes
    if (fsec->floor.normal.z == 1.0 && bsec->floor.normal.z == 1.0 &&
        fsec->ceiling.normal.z == -1.0 && bsec->ceiling.normal.z == -1.0)
    {
      if (line->sidedef->TopTexture != -1 || // a line without top texture isn't a door
          line->sidedef->BottomTexture != -1 || // a line without bottom texture isn't an elevator/plat
          line->sidedef->MidTexture != -1) // a line without mid texture isn't a polyobj door
      {
        const TVec vv1 = *line->linedef->v1;
        const TVec vv2 = *line->linedef->v2;

        const float frontcz1 = fsec->ceiling.GetPointZ(vv1);
        const float frontcz2 = fsec->ceiling.GetPointZ(vv2);
        const float frontfz1 = fsec->floor.GetPointZ(vv1);
        const float frontfz2 = fsec->floor.GetPointZ(vv2);

        const float backcz1 = bsec->ceiling.GetPointZ(vv1);
        const float backcz2 = bsec->ceiling.GetPointZ(vv2);
        const float backfz1 = bsec->floor.GetPointZ(vv1);
        const float backfz2 = bsec->floor.GetPointZ(vv2);

        if ((backcz2 > frontfz2 && backcz2 > frontfz1 && backcz1 > frontfz2 && backcz1 > frontfz1) &&
            (frontcz2 > backfz2 && frontcz2 > backfz1 && frontcz1 > backfz2 && frontcz1 > backfz1))
        {
          // it's an opened door/elevator/polydoor
          return true;
        }
      }
    } else {
      // sloped
      if (((fsec->floor.maxz <= bsec->ceiling.minz && fsec->ceiling.maxz >= bsec->floor.minz) ||
           (fsec->floor.minz <= bsec->ceiling.maxz && fsec->ceiling.minz >= bsec->floor.maxz)) ||
          ((bsec->floor.maxz <= fsec->ceiling.minz && bsec->ceiling.maxz >= fsec->floor.minz) ||
           (bsec->floor.minz <= fsec->ceiling.maxz && bsec->ceiling.minz >= fsec->floor.maxz)))
      {
        return true;
      }
    }

    return false;
  }
};
