//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

class VViewClipper
{
private:
  struct VClipNode;

  VClipNode *FreeClipNodes;
  VClipNode *ClipHead;
  VClipNode *ClipTail;
  TVec      Origin;
  VLevel *Level;

  VClipNode *NewClipNode();
  void RemoveClipNode(VClipNode*);
  void DoAddClipRange(float, float);
  bool DoIsRangeVisible(float, float);

public:
  VViewClipper();
  ~VViewClipper();
  void ClearClipNodes(const TVec&, VLevel*);
  void ClipInitFrustrumRange(const TAVec&, const TVec&, const TVec&,
    const TVec&, float, float);
  void ClipToRanges(const VViewClipper&);
  void AddClipRange(float, float);
  bool IsRangeVisible(float, float);
  bool ClipIsFull();
  float PointToClipAngle(const TVec&);
  bool ClipIsBBoxVisible(const float*, bool, const TVec& = TVec(0, 0, 0), float = 0);
  bool ClipCheckRegion(subregion_t*, subsector_t*, bool, const TVec& = TVec(0, 0, 0), float = 0);
  bool ClipCheckSubsector(subsector_t*, bool, const TVec& = TVec(0, 0, 0), float = 0);
  void ClipAddSubsectorSegs(subsector_t*, bool, TPlane* = nullptr, const TVec& = TVec(0, 0, 0), float = 0);

private:
  static inline bool IsSegAClosedSomething (const seg_t *line) {
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
