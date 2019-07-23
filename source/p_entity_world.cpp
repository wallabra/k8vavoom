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
//**
//**  VEntity collision, physics and related methods.
//**
//**************************************************************************
#include "gamedefs.h"
#include "sv_local.h"


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB gm_smart_z("gm_smart_z", true, "Fix Z position for some things, so they won't fall thru ledge edges?", /*CVAR_Archive|*/CVAR_PreInit);
#ifdef CLIENT
VCvarB r_interpolate_thing_movement("r_interpolate_thing_movement", true, "Interpolate mobj movement?", CVAR_Archive);
VCvarB r_interpolate_thing_angles_models("r_interpolate_thing_angles_models", true, "Interpolate mobj rotation for 3D models?", CVAR_Archive);
VCvarB r_interpolate_thing_angles_sprites("r_interpolate_thing_angles_sprites", false, "Interpolate mobj rotation for sprites?", CVAR_Archive);
#endif


// ////////////////////////////////////////////////////////////////////////// //
/*
struct cptrace_t {
  TVec Pos;
  float BBox[4];
  float FloorZ;
  float CeilingZ;
  float DropOffZ;
  sec_plane_t *EFloor;
  sec_plane_t *ECeiling;
};
*/

struct tmtrace_t {
  VEntity *StepThing; // not for cptrace_t
  TVec End; // not for cptrace_t
  float BBox[4]; // valid for cptrace_t
  float FloorZ; // valid for cptrace_t
  float CeilingZ; // valid for cptrace_t
  float DropOffZ; // valid for cptrace_t

  // WARNING! keep in sync with VEntity fcflags
  /*
  enum {
    FC_FlipFloor = 1u<<0,
    FC_FlipCeiling = 1u<<1,
  };
  vuint32 fcflags; // valid for cptrace_t
  */
  TSecPlaneRef EFloor; // valid for cptrace_t
  TSecPlaneRef ECeiling; // valid for cptrace_t

  enum {
    TF_FloatOk = 0x01u, // if true, move would be ok if within tmtrace.FloorZ - tmtrace.CeilingZ
  };
  vuint32 TraceFlags;

  // keep track of the line that lowers the ceiling,
  // so missiles don't explode against sky hack walls
  line_t *CeilingLine;
  line_t *FloorLine;
  // also keep track of the blocking line, for checking
  // against doortracks
  line_t *BlockingLine; // only lines without backsector

  // keep track of special lines as they are hit,
  // but don't process them until the move is proven valid
  TArray<line_t *> SpecHit;

  VEntity *BlockingMobj;
  line_t *AnyBlockingLine; // any blocking lines (including two-sided)

  // from cptrace_t
  TVec Pos; // valid for cptrace_t


  inline void CopyRegFloor (sec_region_t *r, const TVec *Origin) {
    EFloor = r->efloor;
    if (Origin) FloorZ = EFloor.GetPointZ(*Origin);
  }

  inline void CopyRegCeiling (sec_region_t *r, const TVec *Origin) {
    ECeiling = r->eceiling;
    if (Origin) CeilingZ = ECeiling.GetPointZ(*Origin);
  }

  inline void CopyOpenFloor (opening_t *o, bool setz=true) {
    EFloor = o->efloor;
    if (setz) FloorZ = o->bottom;
  }

  inline void CopyOpenCeiling (opening_t *o, bool setz=true) {
    ECeiling = o->eceiling;
    if (setz) CeilingZ = o->top;
  }
};


//==========================================================================
//
//  tmtSetupGap
//
//  the base floor / ceiling is from the subsector that contains the point
//  any contacted lines the step closer together will adjust them
//
//==========================================================================
static void tmtSetupGap (tmtrace_t *tmtrace, sector_t *sector, float Height, bool debugDump) {
  SV_FindGapFloorCeiling(sector, tmtrace->End, Height, tmtrace->EFloor, tmtrace->ECeiling, debugDump);
  tmtrace->FloorZ = tmtrace->EFloor.GetPointZ(tmtrace->End);
  tmtrace->DropOffZ = tmtrace->FloorZ;
  tmtrace->CeilingZ = tmtrace->ECeiling.GetPointZ(tmtrace->End);
}


//==========================================================================
//
//  VEntity::CopyTraceFloor
//
//==========================================================================
void VEntity::CopyTraceFloor (tmtrace_t *tr, bool setz) {
  EFloor = tr->EFloor;
  if (setz) FloorZ = tr->FloorZ;
}


//==========================================================================
//
//  VEntity::CopyTraceCeiling
//
//==========================================================================
void VEntity::CopyTraceCeiling (tmtrace_t *tr, bool setz) {
  ECeiling = tr->ECeiling;
  if (setz) CeilingZ = tr->CeilingZ;
}


//==========================================================================
//
//  VEntity::CopyRegFloor
//
//==========================================================================
void VEntity::CopyRegFloor (sec_region_t *r, bool setz) {
  EFloor = r->efloor;
  FloorZ = EFloor.GetPointZ(Origin);
}


//==========================================================================
//
//  VEntity::CopyRegCeiling
//
//==========================================================================
void VEntity::CopyRegCeiling (sec_region_t *r, bool setz) {
  ECeiling = r->eceiling;
  CeilingZ = ECeiling.GetPointZ(Origin);
}


// ////////////////////////////////////////////////////////////////////////// //
// searches though the surrounding mapblocks for monsters/players
// distance is in MAPBLOCKUNITS
class VRoughBlockSearchIterator : public VScriptIterator {
private:
  VEntity *Self;
  int Distance;
  VEntity *Ent;
  VEntity **EntPtr;

  int StartX;
  int StartY;
  int Count;
  int CurrentEdge;
  int BlockIndex;
  int FirstStop;
  int SecondStop;
  int ThirdStop;
  int FinalStop;

public:
  VRoughBlockSearchIterator (VEntity *, int, VEntity **);
  virtual bool GetNext () override;
};


extern VCvarB compat_nopassover;


//**************************************************************************
//
//  THING POSITION SETTING
//
//**************************************************************************

//=============================================================================
//
//  VEntity::CreateSecNodeList
//
//=============================================================================
void VEntity::Destroy () {
  UnlinkFromWorld();
  if (XLevel) XLevel->DelSectorList();
  if (TID && Level) RemoveFromTIDList(); // remove from TID list
  Super::Destroy();
}


//=============================================================================
//
//  VEntity::CreateSecNodeList
//
//  phares 3/14/98
//
//  Alters/creates the sector_list that shows what sectors the object resides in
//
//=============================================================================
void VEntity::CreateSecNodeList () {
  int xl, xh, yl, yh, bx, by;
  msecnode_t *Node;

  // first, clear out the existing Thing fields. as each node is
  // added or verified as needed, Thing will be set properly.
  // when finished, delete all nodes where Thing is still nullptr.
  // these represent the sectors the Thing has vacated.

  Node = XLevel->SectorList;
  while (Node) {
    Node->Thing = nullptr;
    Node = Node->TNext;
  }

  float tmbbox[4];
  tmbbox[BOX2D_TOP] = Origin.y+Radius;
  tmbbox[BOX2D_BOTTOM] = Origin.y-Radius;
  tmbbox[BOX2D_RIGHT] = Origin.x+Radius;
  tmbbox[BOX2D_LEFT] = Origin.x-Radius;

  //++validcount; // used to make sure we only process a line once
  XLevel->IncrementValidCount();

  xl = MapBlock(tmbbox[BOX2D_LEFT]-XLevel->BlockMapOrgX);
  xh = MapBlock(tmbbox[BOX2D_RIGHT]-XLevel->BlockMapOrgX);
  yl = MapBlock(tmbbox[BOX2D_BOTTOM]-XLevel->BlockMapOrgY);
  yh = MapBlock(tmbbox[BOX2D_TOP]-XLevel->BlockMapOrgY);

  for (bx = xl; bx <= xh; ++bx) {
    for (by = yl; by <= yh; ++by) {
      line_t *ld;
      for (VBlockLinesIterator It(XLevel, bx, by, &ld); It.GetNext(); ) {
        // locates all the sectors the object is in by looking at the lines that cross through it.
        // you have already decided that the object is allowed at this location, so don't
        // bother with checking impassable or blocking lines.
        if (tmbbox[BOX2D_RIGHT] <= ld->bbox2d[BOX2D_LEFT] ||
            tmbbox[BOX2D_LEFT] >= ld->bbox2d[BOX2D_RIGHT] ||
            tmbbox[BOX2D_TOP] <= ld->bbox2d[BOX2D_BOTTOM] ||
            tmbbox[BOX2D_BOTTOM] >= ld->bbox2d[BOX2D_TOP])
        {
          continue;
        }

        if (P_BoxOnLineSide(tmbbox, ld) != -1) continue;

        // this line crosses through the object

        // collect the sector(s) from the line and add to the SectorList you're examining.
        // if the Thing ends up being allowed to move to this position, then the sector_list will
        // be attached to the Thing's VEntity at TouchingSectorList.

        XLevel->SectorList = XLevel->AddSecnode(ld->frontsector, this, XLevel->SectorList);

        // don't assume all lines are 2-sided, since some Things like
        // MT_TFOG are allowed regardless of whether their radius
        // takes them beyond an impassable linedef.

        // killough 3/27/98, 4/4/98:
        // use sidedefs instead of 2s flag to determine two-sidedness

        if (ld->backsector && ld->backsector != ld->frontsector) {
          XLevel->SectorList = XLevel->AddSecnode(ld->backsector, this, XLevel->SectorList);
        }
      }
    }
  }

  // add the sector of the (x,y) point to sector_list
  XLevel->SectorList = XLevel->AddSecnode(Sector, this, XLevel->SectorList);

  // now delete any nodes that won't be used
  // these are the ones where Thing is still nullptr

  Node = XLevel->SectorList;
  while (Node) {
    if (Node->Thing == nullptr) {
      if (Node == XLevel->SectorList) XLevel->SectorList = Node->TNext;
      Node = XLevel->DelSecnode(Node);
    } else {
      Node = Node->TNext;
    }
  }
}


//==========================================================================
//
//  VEntity::UnlinkFromWorld
//
//  unlinks a thing from block map and sectors. on each position change,
//  BLOCKMAP and other lookups maintaining lists ot things inside these
//  structures need to be updated.
//
//==========================================================================
void VEntity::UnlinkFromWorld () {
  //!MoveFlags &= ~MVF_JustMoved;

  if (!SubSector) return;

  if (!(EntityFlags&EF_NoSector)) {
    // invisible things don't need to be in sector list
    // unlink from subsector
    if (SNext) SNext->SPrev = SPrev;
    if (SPrev) SPrev->SNext = SNext; else Sector->ThingList = SNext;
    SNext = nullptr;
    SPrev = nullptr;

    // phares 3/14/98
    //
    // Save the sector list pointed to by TouchingSectorList. In
    // LinkToWorld, we'll keep any nodes that represent sectors the Thing
    // still touches. We'll add new ones then, and delete any nodes for
    // sectors the Thing has vacated. Then we'll put it back into
    // TouchingSectorList. It's done this way to avoid a lot of
    // deleting/creating for nodes, when most of the time you just get
    // back what you deleted anyway.
    //
    // If this Thing is being removed entirely, then the calling routine
    // will clear out the nodes in sector_list.
    //
    XLevel->DelSectorList();
    XLevel->SectorList = TouchingSectorList;
    TouchingSectorList = nullptr; // to be restored by LinkToWorld
  }

  if (BlockMapCell /*&& !(EntityFlags&EF_NoBlockmap)*/) {
    // unlink from block map
    if (BlockMapNext) BlockMapNext->BlockMapPrev = BlockMapPrev;
    if (BlockMapPrev) {
      BlockMapPrev->BlockMapNext = BlockMapNext;
    } else {
      // we are the first entity in blockmap cell
      BlockMapCell -= 1; // real cell number
      // do some sanity checks
      check(XLevel->BlockLinks[BlockMapCell] == this);
      // fix list head
      XLevel->BlockLinks[BlockMapCell] = BlockMapNext;
    }
    BlockMapCell = 0;
  }
  SubSector = nullptr;
  Sector = nullptr;
}


//==========================================================================
//
//  VEntity::LinkToWorld
//
//  Links a thing into both a block and a subsector based on it's x y.
//  Sets thing->subsector properly
//
//==========================================================================
void VEntity::LinkToWorld (bool properFloorCheck) {
  if (SubSector) UnlinkFromWorld();

  // link into subsector
  subsector_t *ss = XLevel->PointInSubsector(Origin);
  SubSector = ss;
  Sector = ss->sector;

  if (!(EntityFlags&EF_IsPlayer)) {
    if (properFloorCheck) {
      if (Radius < 4 || (EntityFlags&(EF_ColideWithWorld|EF_NoSector|EF_NoBlockmap|EF_Invisible|EF_Missile|EF_ActLikeBridge)) != EF_ColideWithWorld) {
        properFloorCheck = false;
      }
    }
  } else {
    properFloorCheck = true;
  }

  if (!gm_smart_z) properFloorCheck = false;

  if (properFloorCheck) {
    //FIXME: this is copypasta from `CheckRelPos()`; factor it out
    tmtrace_t tmtrace;
    memset((void *)&tmtrace, 0, sizeof(tmtrace));
    subsector_t *newsubsec = ss;
    const TVec Pos = Origin;

    tmtrace.End = Pos;

    tmtrace.BBox[BOX2D_TOP] = Pos.y+Radius;
    tmtrace.BBox[BOX2D_BOTTOM] = Pos.y-Radius;
    tmtrace.BBox[BOX2D_RIGHT] = Pos.x+Radius;
    tmtrace.BBox[BOX2D_LEFT] = Pos.x-Radius;

    tmtSetupGap(&tmtrace, newsubsec->sector, Height, false);

    // check lines
    XLevel->IncrementValidCount();

    //tmtrace.FloorZ = tmtrace.DropOffZ;

    int xl = MapBlock(tmtrace.BBox[BOX2D_LEFT]-XLevel->BlockMapOrgX);
    int xh = MapBlock(tmtrace.BBox[BOX2D_RIGHT]-XLevel->BlockMapOrgX);
    int yl = MapBlock(tmtrace.BBox[BOX2D_BOTTOM]-XLevel->BlockMapOrgY);
    int yh = MapBlock(tmtrace.BBox[BOX2D_TOP]-XLevel->BlockMapOrgY);

    //float lastFZ, lastCZ;
    //sec_plane_t *lastFloor = nullptr;
    //sec_plane_t *lastCeiling = nullptr;

    for (int bx = xl; bx <= xh; ++bx) {
      for (int by = yl; by <= yh; ++by) {
        line_t *ld;
        for (VBlockLinesIterator It(XLevel, bx, by, &ld); It.GetNext(); ) {
          // we don't care about any blocking line info...
          (void)CheckRelLine(tmtrace, ld, true); // ...and we don't want to process any specials
        }
      }
    }

    CopyTraceFloor(&tmtrace);
    CopyTraceCeiling(&tmtrace);
  } else {
    // simplified check
    TSecPlaneRef floor, ceiling;
    SV_FindGapFloorCeiling(ss->sector, Origin, Height, EFloor, ECeiling);
    FloorZ = EFloor.GetPointZ(Origin);
    CeilingZ = ECeiling.GetPointZ(Origin);
  }

  // link into sector
  if (!(EntityFlags&EF_NoSector)) {
    // invisible things don't go into the sector links
    VEntity **Link = &Sector->ThingList;
    SPrev = nullptr;
    SNext = *Link;
    if (*Link) (*Link)->SPrev = this;
    *Link = this;

    // phares 3/16/98
    //
    // If sector_list isn't nullptr, it has a collection of sector
    // nodes that were just removed from this Thing.
    //
    // Collect the sectors the object will live in by looking at
    // the existing sector_list and adding new nodes and deleting
    // obsolete ones.
    //
    // When a node is deleted, its sector links (the links starting
    // at sector_t->touching_thinglist) are broken. When a node is
    // added, new sector links are created.
    CreateSecNodeList();
    TouchingSectorList = XLevel->SectorList; // attach to thing
    XLevel->SectorList = nullptr; // clear for next time
  } else {
    XLevel->DelSectorList();
  }

  // link into blockmap
  if (!(EntityFlags&EF_NoBlockmap)) {
    check(BlockMapCell == 0);
    // inert things don't need to be in blockmap
    int blockx = MapBlock(Origin.x-XLevel->BlockMapOrgX);
    int blocky = MapBlock(Origin.y-XLevel->BlockMapOrgY);

    if (blockx >= 0 && blockx < XLevel->BlockMapWidth &&
        blocky >= 0 && blocky < XLevel->BlockMapHeight)
    {
      BlockMapCell = ((unsigned)blocky*(unsigned)XLevel->BlockMapWidth+(unsigned)blockx);
      VEntity **link = &XLevel->BlockLinks[BlockMapCell];
      BlockMapPrev = nullptr;
      BlockMapNext = *link;
      if (*link) (*link)->BlockMapPrev = this;
      *link = this;
      BlockMapCell += 1;
    } else {
      // thing is off the map
      BlockMapNext = BlockMapPrev = nullptr;
    }
  }
}


//==========================================================================
//
//  VEntity::CheckWater
//
//==========================================================================
bool VEntity::CheckWater () {
  TVec point;
  int cont;

  point = Origin;
  point.z += 1.0f;

  WaterLevel = 0;
  WaterType = 0;
  cont = SV_PointContents(Sector, point);
  if (cont > 0) {
    WaterType = cont;
    WaterLevel = 1;
    point.z = Origin.z+Height*0.5f;
    cont = SV_PointContents(Sector, point);
    if (cont > 0) {
      WaterLevel = 2;
      if (EntityFlags&EF_IsPlayer) {
        point = Player->ViewOrg;
        cont = SV_PointContents(Sector, point);
        if (cont > 0) WaterLevel = 3;
      } else {
        point.z = Origin.z+Height*0.75f;
        cont = SV_PointContents(Sector, point);
        if (cont > 0) WaterLevel = 3;
      }
    }
  }
  return (WaterLevel > 1);
}


//**************************************************************************
//
//  CHECK ABSOLUTE POSITION
//
//**************************************************************************

//==========================================================================
//
//  VEntity::CheckPosition
//
//  This is purely informative, nothing is modified
//
// in:
//  a mobj_t (can be valid or invalid)
//  a position to be checked
//   (doesn't need to be related to the mobj_t->x,y)
//
//==========================================================================
bool VEntity::CheckPosition (TVec Pos) {
  //sec_region_t *gap;
  //sec_region_t *reg;
  tmtrace_t cptrace;
  bool good = true;

  memset((void *)&cptrace, 0, sizeof(cptrace));
  cptrace.Pos = Pos;

  cptrace.BBox[BOX2D_TOP] = Pos.y+Radius;
  cptrace.BBox[BOX2D_BOTTOM] = Pos.y-Radius;
  cptrace.BBox[BOX2D_RIGHT] = Pos.x+Radius;
  cptrace.BBox[BOX2D_LEFT] = Pos.x-Radius;

  subsector_t *newsubsec = XLevel->PointInSubsector(Pos);

  // The base floor / ceiling is from the subsector that contains the point.
  // Any contacted lines the step closer together will adjust them.
  SV_FindGapFloorCeiling(newsubsec->sector, Pos, Height, cptrace.EFloor, cptrace.ECeiling);
  cptrace.DropOffZ = cptrace.FloorZ = cptrace.EFloor.GetPointZClamped(Pos);
  cptrace.CeilingZ = cptrace.ECeiling.GetPointZClamped(Pos);
  /*
  gap = SV_FindThingGap(newsubsec->sector, Pos, Height);
  reg = gap;
  while (reg->prev && (reg->efloor.splane->flags&SPF_NOBLOCKING) != 0) reg = reg->prev;
  cptrace.CopyRegFloor(reg, &Pos);
  cptrace.DropOffZ = cptrace.FloorZ;
  reg = gap;
  while (reg->next && (reg->eceiling.splane->flags&SPF_NOBLOCKING) != 0) reg = reg->next;
  cptrace.CopyRegCeiling(reg, &Pos);
  */

  //++validcount;
  XLevel->IncrementValidCount();

  if (EntityFlags&EF_ColideWithThings) {
    // Check things first, possibly picking things up.
    // The bounding box is extended by MAXRADIUS
    // because mobj_ts are grouped into mapblocks
    // based on their origin point, and can overlap
    // into adjacent blocks by up to MAXRADIUS units.
    const int xl = MapBlock(cptrace.BBox[BOX2D_LEFT]-XLevel->BlockMapOrgX-MAXRADIUS);
    const int xh = MapBlock(cptrace.BBox[BOX2D_RIGHT]-XLevel->BlockMapOrgX+MAXRADIUS);
    const int yl = MapBlock(cptrace.BBox[BOX2D_BOTTOM]-XLevel->BlockMapOrgY-MAXRADIUS);
    const int yh = MapBlock(cptrace.BBox[BOX2D_TOP]-XLevel->BlockMapOrgY+MAXRADIUS);

    for (int bx = xl; bx <= xh; ++bx) {
      for (int by = yl; by <= yh; ++by) {
        for (VBlockThingsIterator It(XLevel, bx, by); It; ++It) {
          if (!CheckThing(cptrace, *It)) return false;
        }
      }
    }
  }

  if (EntityFlags&EF_ColideWithWorld) {
    // check lines
    const int xl = MapBlock(cptrace.BBox[BOX2D_LEFT]-XLevel->BlockMapOrgX);
    const int xh = MapBlock(cptrace.BBox[BOX2D_RIGHT]-XLevel->BlockMapOrgX);
    const int yl = MapBlock(cptrace.BBox[BOX2D_BOTTOM]-XLevel->BlockMapOrgY);
    const int yh = MapBlock(cptrace.BBox[BOX2D_TOP]-XLevel->BlockMapOrgY);

    for (int bx = xl; bx <= xh; ++bx) {
      for (int by = yl; by <= yh; ++by) {
        line_t *ld;
        for (VBlockLinesIterator It(XLevel, bx, by, &ld); It.GetNext(); ) {
          //good &= CheckLine(cptrace, ld);
          if (!CheckLine(cptrace, ld)) good = false; // no early exit!
        }
      }
    }

    if (!good) return false;
  }

  return true;
}


//==========================================================================
//
//  VEntity::CheckThing
//
//  returns `false` when blocked
//
//==========================================================================
bool VEntity::CheckThing (tmtrace_t &cptrace, VEntity *Other) {
  // don't clip against self
  if (Other == this) return true;
  // can't hit thing
  if (!(Other->EntityFlags&EF_ColideWithThings)) return true;
  if (!(Other->EntityFlags&EF_Solid)) return true;

  const float blockdist = Other->Radius+Radius;

  if (fabsf(Other->Origin.x-cptrace.Pos.x) >= blockdist ||
      fabsf(Other->Origin.y-cptrace.Pos.y) >= blockdist)
  {
    // didn't hit it
    return true;
  }

  if ((EntityFlags&(EF_PassMobj|EF_Missile)) || (Other->EntityFlags&EF_ActLikeBridge)) {
    // prevent some objects from overlapping
    if (EntityFlags&Other->EntityFlags&EF_DontOverlap) return false;
    // check if a mobj passed over/under another object
    if (cptrace.Pos.z >= Other->Origin.z+Other->Height) return true;
    if (cptrace.Pos.z+Height <= Other->Origin.z) return true; // under thing
  }

  return false;
}


//==========================================================================
//
//  VEntity::CheckLine
//
//  Adjusts cptrace.FloorZ and cptrace.CeilingZ as lines are contacted
//
//==========================================================================
bool VEntity::CheckLine (tmtrace_t &cptrace, line_t *ld) {
  if (cptrace.BBox[BOX2D_RIGHT] <= ld->bbox2d[BOX2D_LEFT] ||
      cptrace.BBox[BOX2D_LEFT] >= ld->bbox2d[BOX2D_RIGHT] ||
      cptrace.BBox[BOX2D_TOP] <= ld->bbox2d[BOX2D_BOTTOM] ||
      cptrace.BBox[BOX2D_BOTTOM] >= ld->bbox2d[BOX2D_TOP])
  {
    return true;
  }

  if (P_BoxOnLineSide(&cptrace.BBox[0], ld) != -1) return true;

  // a line has been hit
  if (!ld->backsector) return false; // one sided line

  if (!(ld->flags&ML_RAILING)) {
    if (ld->flags&ML_BLOCKEVERYTHING) return false; // explicitly blocking everything
    if ((EntityFlags&VEntity::EF_Missile) && (ld->flags&ML_BLOCKPROJECTILE)) return false; // blocks projectile
    if ((EntityFlags&VEntity::EF_CheckLineBlocking) && (ld->flags&ML_BLOCKING)) return false; // explicitly blocking everything
    if ((EntityFlags&VEntity::EF_CheckLineBlockMonsters) && (ld->flags&ML_BLOCKMONSTERS)) return false; // block monsters only
    if ((EntityFlags&VEntity::EF_IsPlayer) && (ld->flags&ML_BLOCKPLAYERS)) return false; // block players only
    if ((EntityFlags&VEntity::EF_Float) && (ld->flags&ML_BLOCK_FLOATERS)) return false; // block floaters only
  }

  // set openrange, opentop, openbottom
  TVec hit_point = cptrace.Pos-(DotProduct(cptrace.Pos, ld->normal)-ld->dist)*ld->normal;
  opening_t *open = SV_LineOpenings(ld, hit_point, SPF_NOBLOCKING, true); //!(EntityFlags&EF_Missile)); // missiles ignores 3dmidtex
  open = SV_FindOpening(open, cptrace.Pos.z, cptrace.Pos.z+Height);

  if (open) {
    // adjust floor / ceiling heights
    if (!(open->eceiling.splane->flags&SPF_NOBLOCKING) && open->top < cptrace.CeilingZ) {
      cptrace.CopyOpenCeiling(open);
    }

    if (!(open->efloor.splane->flags&SPF_NOBLOCKING) && open->bottom > cptrace.FloorZ) {
      cptrace.CopyOpenFloor(open);
    }

    if (open->lowfloor < cptrace.DropOffZ) cptrace.DropOffZ = open->lowfloor;

    if (ld->flags&ML_RAILING) cptrace.FloorZ += 32;
  } else {
    cptrace.CeilingZ = cptrace.FloorZ;
  }

  return true;
}


//**************************************************************************
//
//  MOVEMENT CLIPPING
//
//**************************************************************************

//==========================================================================
//
//  VEntity::CheckRelPosition
//
// This is purely informative, nothing is modified
// (except things picked up).
//
// in:
//  a mobj_t (can be valid or invalid)
//  a position to be checked
//   (doesn't need to be related to the mobj_t->x,y)
//
// during:
//  special things are touched if MF_PICKUP
//  early out on solid lines?
//
// out:
//  newsubsec
//  floorz
//  ceilingz
//  tmdropoffz
//   the lowest point contacted
//   (monsters won't move to a dropoff)
//  speciallines[]
//  numspeciallines
//  VEntity *BlockingMobj = pointer to thing that blocked position (nullptr if not
//   blocked, or blocked by a line).
//
//==========================================================================
bool VEntity::CheckRelPosition (tmtrace_t &tmtrace, TVec Pos, bool noPickups, bool ignoreMonsters, bool ignorePlayers) {
  tmtrace.End = Pos;

  tmtrace.BBox[BOX2D_TOP] = Pos.y+Radius;
  tmtrace.BBox[BOX2D_BOTTOM] = Pos.y-Radius;
  tmtrace.BBox[BOX2D_RIGHT] = Pos.x+Radius;
  tmtrace.BBox[BOX2D_LEFT] = Pos.x-Radius;

  subsector_t *newsubsec = XLevel->PointInSubsector(Pos);
  tmtrace.CeilingLine = nullptr;

  tmtSetupGap(&tmtrace, newsubsec->sector, Height, false);

  XLevel->IncrementValidCount();
  tmtrace.SpecHit.reset(); // was `Clear()`

  tmtrace.BlockingMobj = nullptr;
  tmtrace.StepThing = nullptr;
  VEntity *thingblocker = nullptr;

  // check things first, possibly picking things up.
  // the bounding box is extended by MAXRADIUS
  // because mobj_ts are grouped into mapblocks
  // based on their origin point, and can overlap
  // into adjacent blocks by up to MAXRADIUS units.
  if (EntityFlags&EF_ColideWithThings) {
    const int xl = MapBlock(tmtrace.BBox[BOX2D_LEFT]-XLevel->BlockMapOrgX-MAXRADIUS);
    const int xh = MapBlock(tmtrace.BBox[BOX2D_RIGHT]-XLevel->BlockMapOrgX+MAXRADIUS);
    const int yl = MapBlock(tmtrace.BBox[BOX2D_BOTTOM]-XLevel->BlockMapOrgY-MAXRADIUS);
    const int yh = MapBlock(tmtrace.BBox[BOX2D_TOP]-XLevel->BlockMapOrgY+MAXRADIUS);

    for (int bx = xl; bx <= xh; ++bx) {
      for (int by = yl; by <= yh; ++by) {
        for (VBlockThingsIterator It(XLevel, bx, by); It; ++It) {
          if (ignoreMonsters || ignorePlayers) {
            VEntity *ent = *It;
            if (ignorePlayers && ent->IsPlayer()) continue;
            if (ignoreMonsters && (ent->IsMissile() || ent->IsMonster())) continue;
          }
          if (!CheckRelThing(tmtrace, *It, noPickups)) {
            // continue checking for other things in to see if we hit something
            if (!tmtrace.BlockingMobj || compat_nopassover ||
                (Level->LevelInfoFlags2&VLevelInfo::LIF2_CompatNoPassOver))
            {
              // slammed into something
              return false;
            } else if (!tmtrace.BlockingMobj->Player &&
                       !(EntityFlags&(VEntity::EF_Float|VEntity::EF_Missile)) &&
                       tmtrace.BlockingMobj->Origin.z+tmtrace.BlockingMobj->Height-tmtrace.End.z <= MaxStepHeight)
            {
              if (!thingblocker || tmtrace.BlockingMobj->Origin.z > thingblocker->Origin.z) thingblocker = tmtrace.BlockingMobj;
              tmtrace.BlockingMobj = nullptr;
            } else if (Player && tmtrace.End.z+Height-tmtrace.BlockingMobj->Origin.z <= MaxStepHeight) {
              if (thingblocker) {
                // something to step up on, set it as the blocker so that we don't step up
                return false;
              }
              // nothing is blocking, but this object potentially could
              // if there is something else to step on
              tmtrace.BlockingMobj = nullptr;
            } else {
              // blocking
              return false;
            }
          }
        }
      }
    }
  }

  // check lines
  XLevel->IncrementValidCount();

  float thingdropoffz = tmtrace.FloorZ;
  tmtrace.FloorZ = tmtrace.DropOffZ;
  tmtrace.BlockingMobj = nullptr;

  if (EntityFlags&EF_ColideWithWorld) {
    const int xl = MapBlock(tmtrace.BBox[BOX2D_LEFT]-XLevel->BlockMapOrgX);
    const int xh = MapBlock(tmtrace.BBox[BOX2D_RIGHT]-XLevel->BlockMapOrgX);
    const int yl = MapBlock(tmtrace.BBox[BOX2D_BOTTOM]-XLevel->BlockMapOrgY);
    const int yh = MapBlock(tmtrace.BBox[BOX2D_TOP]-XLevel->BlockMapOrgY);
    bool good = true;

    line_t *fuckhit = nullptr;
    float lastFrac = 1e7f;
    for (int bx = xl; bx <= xh; ++bx) {
      for (int by = yl; by <= yh; ++by) {
        line_t *ld;
        for (VBlockLinesIterator It(XLevel, bx, by, &ld); It.GetNext(); ) {
          //good &= CheckRelLine(tmtrace, ld);
          // if we don't want pickups, don't activate specials
          if (!CheckRelLine(tmtrace, ld, noPickups)) {
            good = false;
            // find the fractional intercept point along the trace line
            const float den = DotProduct(ld->normal, tmtrace.End-Pos);
            if (den == 0) {
              fuckhit = ld;
              lastFrac = 0;
            } else {
              const float num = ld->dist-DotProduct(Pos, ld->normal);
              const float frac = num/den;
              if (fabsf(frac) < lastFrac) {
                fuckhit = ld;
                lastFrac = fabsf(frac);
              }
            }
          }
        }
      }
    }

    if (!good) {
      if (!tmtrace.AnyBlockingLine) tmtrace.AnyBlockingLine = fuckhit;
      return false;
    }

    if (tmtrace.CeilingZ-tmtrace.FloorZ < Height) {
      if (!tmtrace.AnyBlockingLine) tmtrace.AnyBlockingLine = fuckhit;
      return false;
    }
  }

  if (tmtrace.StepThing) tmtrace.DropOffZ = thingdropoffz;

  tmtrace.BlockingMobj = thingblocker;
  if (tmtrace.BlockingMobj) return false;

  return true;
}


//==========================================================================
//
//  VEntity::CheckRelThing
//
//  returns `false` when blocked
//
//==========================================================================
bool VEntity::CheckRelThing (tmtrace_t &tmtrace, VEntity *Other, bool noPickups) {
  // don't clip against self
  if (Other == this) return true;
  // can't hit thing
  if (!(Other->EntityFlags&EF_ColideWithThings)) return true;

  const float blockdist = Other->Radius+Radius;

  if (fabsf(Other->Origin.x-tmtrace.End.x) >= blockdist ||
      fabsf(Other->Origin.y-tmtrace.End.y) >= blockdist)
  {
    // didn't hit it
    return true;
  }

  tmtrace.BlockingMobj = Other;
  if (!(Level->LevelInfoFlags2&VLevelInfo::LIF2_CompatNoPassOver) &&
      !compat_nopassover &&
      !(EntityFlags&(EF_Float|EF_Missile|EF_NoGravity)) &&
      (Other->EntityFlags&(EF_Solid|EF_ActLikeBridge)) == (EF_Solid|EF_ActLikeBridge))
  {
    // allow actors to walk on other actors as well as floors
    if (fabsf(Other->Origin.x-tmtrace.End.x) < Other->Radius ||
        fabsf(Other->Origin.y-tmtrace.End.y) < Other->Radius)
    {
      if (Other->Origin.z+Other->Height >= tmtrace.FloorZ &&
          Other->Origin.z+Other->Height <= tmtrace.End.z+MaxStepHeight)
      {
        tmtrace.StepThing = Other;
        tmtrace.FloorZ = Other->Origin.z+Other->Height;
      }
    }
  }

  //if (!(tmtrace.Thing->EntityFlags & VEntity::EF_NoPassMobj) || Actor(Other).bSpecial)
  if ((((EntityFlags&EF_PassMobj) || (Other->EntityFlags&EF_ActLikeBridge)) &&
       !(Level->LevelInfoFlags2&VLevelInfo::LIF2_CompatNoPassOver) && !compat_nopassover) ||
      (EntityFlags&EF_Missile))
  {
    // prevent some objects from overlapping
    if (EntityFlags&Other->EntityFlags&EF_DontOverlap) return false;
    // check if a mobj passed over/under another object
    if (tmtrace.End.z >= Other->Origin.z+Other->Height) return true; // overhead
    if (tmtrace.End.z+Height <= Other->Origin.z) return true;  // underneath
  }

  //FIXME: call VC to determine blocking
  if (noPickups) return !(Other->EntityFlags&EF_Solid); // non-solids won't block

  return eventTouch(Other);
}


//==========================================================================
//
//  VEntity::CheckRelLine
//
//  Adjusts tmtrace.FloorZ and tmtrace.CeilingZ as lines are contacted
//
//==========================================================================
bool VEntity::CheckRelLine (tmtrace_t &tmtrace, line_t *ld, bool skipSpecials) {
  // check line bounding box for early out
  if (tmtrace.BBox[BOX2D_RIGHT] <= ld->bbox2d[BOX2D_LEFT] ||
      tmtrace.BBox[BOX2D_LEFT] >= ld->bbox2d[BOX2D_RIGHT] ||
      tmtrace.BBox[BOX2D_TOP] <= ld->bbox2d[BOX2D_BOTTOM] ||
      tmtrace.BBox[BOX2D_BOTTOM] >= ld->bbox2d[BOX2D_TOP])
  {
    return true;
  }

  if (P_BoxOnLineSide(&tmtrace.BBox[0], ld) != -1) return true;

  // a line has been hit

  // The moving thing's destination position will cross the given line.
  // If this should not be allowed, return false.
  // If the line is special, keep track of it to process later if the move is proven ok.
  // NOTE: specials are NOT sorted by order, so two special lines that are only 8 pixels apart
  //       could be crossed in either order.

  if (!ld->backsector) {
    // one sided line
    if (!skipSpecials) BlockedByLine(ld);
    // mark the line as blocking line
    tmtrace.BlockingLine = tmtrace.AnyBlockingLine = ld;
    return false;
  }

  if (!(ld->flags&ML_RAILING)) {
    if (ld->flags&ML_BLOCKEVERYTHING) {
      // explicitly blocking everything
      if (!skipSpecials) BlockedByLine(ld);
      tmtrace.AnyBlockingLine = ld;
      return false;
    }

    if ((EntityFlags&VEntity::EF_Missile) && (ld->flags&ML_BLOCKPROJECTILE)) {
      // blocks projectile
      if (!skipSpecials) BlockedByLine(ld);
      tmtrace.AnyBlockingLine = ld;
      return false;
    }

    if ((EntityFlags&VEntity::EF_CheckLineBlocking) && (ld->flags&ML_BLOCKING)) {
      // explicitly blocking everything
      if (!skipSpecials) BlockedByLine(ld);
      tmtrace.AnyBlockingLine = ld;
      return false;
    }

    if ((EntityFlags&VEntity::EF_CheckLineBlockMonsters) && (ld->flags&ML_BLOCKMONSTERS)) {
      // block monsters only
      if (!skipSpecials) BlockedByLine(ld);
      tmtrace.AnyBlockingLine = ld;
      return false;
    }

    if ((EntityFlags&VEntity::EF_IsPlayer) && (ld->flags&ML_BLOCKPLAYERS)) {
      // block players only
      if (!skipSpecials) BlockedByLine(ld);
      tmtrace.AnyBlockingLine = ld;
      return false;
    }

    if ((EntityFlags&VEntity::EF_Float) && (ld->flags&ML_BLOCK_FLOATERS)) {
      // block floaters only
      if (!skipSpecials) BlockedByLine(ld);
      tmtrace.AnyBlockingLine = ld;
      return false;
    }
  }

  // set openrange, opentop, openbottom
  const float hgt = (Height > 0 ? Height : 0.0f);
  TVec hit_point = tmtrace.End-(DotProduct(tmtrace.End, ld->normal)-ld->dist)*ld->normal;
  opening_t *open = SV_LineOpenings(ld, hit_point, SPF_NOBLOCKING, true); //!(EntityFlags&EF_Missile)); // missiles ignores 3dmidtex
  open = SV_FindRelOpening(open, tmtrace.End.z, tmtrace.End.z+hgt);

  if (open) {
    // adjust floor / ceiling heights
    if (!(open->eceiling.splane->flags&SPF_NOBLOCKING) && open->top < tmtrace.CeilingZ) {
      if (!skipSpecials || open->top+hgt >= Origin.z+hgt) {
        tmtrace.CopyOpenCeiling(open);
        tmtrace.CeilingLine = ld;
      }
    }

    if (!(open->efloor.splane->flags&SPF_NOBLOCKING) && open->bottom > tmtrace.FloorZ) {
      if (!skipSpecials || open->bottom <= Origin.z) {
        tmtrace.CopyOpenFloor(open);
        tmtrace.FloorLine = ld;
      }
    }

    if (open->lowfloor < tmtrace.DropOffZ) tmtrace.DropOffZ = open->lowfloor;

    if (ld->flags&ML_RAILING) tmtrace.FloorZ += 32;
  } else {
    tmtrace.CeilingZ = tmtrace.FloorZ;
  }

  // if contacted a special line, add it to the list
  if (!skipSpecials && ld->special) tmtrace.SpecHit.Append(ld);

  return true;
}


//==========================================================================
//
//  VEntity::BlockedByLine
//
//==========================================================================
void VEntity::BlockedByLine (line_t *ld) {
  if (EntityFlags&EF_Blasted) eventBlastedHitLine();
  if (ld->special) eventCheckForPushSpecial(ld, 0);
}


//==========================================================================
//
//  VEntity::GetDrawOrigin
//
//==========================================================================
TVec VEntity::GetDrawOrigin () {
  TVec sprorigin = Origin+GetDrawDelta();
  sprorigin.z -= FloorClip;
  return sprorigin;
}


//==========================================================================
//
//  VEntity::GetDrawDelta
//
//==========================================================================
TVec VEntity::GetDrawDelta () {
#ifdef CLIENT
  // movement interpolation
  if (r_interpolate_thing_movement && (MoveFlags&MVF_JustMoved)) {
    const float ctt = XLevel->Time-LastMoveTime;
    if (ctt >= 0.0f && ctt < LastMoveDuration && LastMoveDuration > 0.0f) {
      TVec delta = Origin-LastMoveOrigin;
      if (!delta.isZero()) {
        delta *= ctt/LastMoveDuration;
        return (LastMoveOrigin+delta)-Origin;
      } else {
        // reset if angles are equal
        if (LastMoveAngles.yaw == Angles.yaw) MoveFlags &= ~VEntity::MVF_JustMoved;
      }
    } else {
      // unconditional reset
      MoveFlags &= ~VEntity::MVF_JustMoved;
    }
  }
#endif
  return TVec(0.0f, 0.0f, 0.0f);
}


//==========================================================================
//
//  VEntity::GetInterpolatedDrawAngles
//
//==========================================================================
TAVec VEntity::GetInterpolatedDrawAngles () {
#ifdef CLIENT
  // movement interpolation
  if (MoveFlags&MVF_JustMoved) {
    const float ctt = XLevel->Time-LastMoveTime;
    if (ctt >= 0.0f && ctt < LastMoveDuration && LastMoveDuration > 0.0f) {
      float deltaYaw = AngleDiff(AngleMod(LastMoveAngles.yaw), AngleMod(Angles.yaw));
      if (deltaYaw) {
        deltaYaw *= ctt/LastMoveDuration;
        return TAVec(Angles.pitch, AngleMod(LastMoveAngles.yaw+deltaYaw), Angles.roll);
      } else {
        // reset if origins are equal
        if ((Origin-LastMoveOrigin).isZero()) MoveFlags &= ~VEntity::MVF_JustMoved;
      }
    } else {
      // unconditional reset
      MoveFlags &= ~VEntity::MVF_JustMoved;
    }
  }
#endif
  return Angles;
}


//==========================================================================
//
//  VEntity::TryMove
//
//  Attempt to move to a new position, crossing special lines.
//
//==========================================================================
bool VEntity::TryMove (tmtrace_t &tmtrace, TVec newPos, bool AllowDropOff, bool checkOnly) {
  bool check;
  TVec oldorg(0, 0, 0);
  line_t *ld;
  sector_t *OldSec = Sector;

  check = CheckRelPosition(tmtrace, newPos, checkOnly);
  tmtrace.TraceFlags &= ~tmtrace_t::TF_FloatOk;

  if (!check) {
    // cannot fit into destination point
    VEntity *O = tmtrace.BlockingMobj;
    //GCon->Logf("HIT! %s", O->GetClass()->GetName());

    if (!O || !(EntityFlags&EF_IsPlayer) ||
        (O->EntityFlags&EF_IsPlayer) ||
        O->Origin.z+O->Height-Origin.z > MaxStepHeight ||
        O->CeilingZ-(O->Origin.z+O->Height) < Height ||
        tmtrace.CeilingZ-(O->Origin.z+O->Height) < Height)
    {
      // can't step up or doesn't fit
      PushLine(tmtrace, checkOnly);
      return false;
    }

    if (!(EntityFlags&EF_PassMobj) || compat_nopassover ||
        (Level->LevelInfoFlags2&VLevelInfo::LIF2_CompatNoPassOver))
    {
      // can't go over
      return false;
    }
  }

  if (EntityFlags&EF_ColideWithWorld) {
    if (tmtrace.CeilingZ-tmtrace.FloorZ < Height) {
      // doesn't fit
      PushLine(tmtrace, checkOnly);
      //printf("*** WORLD(0)!\n");
      return false;
    }

    tmtrace.TraceFlags |= tmtrace_t::TF_FloatOk;

    if (tmtrace.CeilingZ-Origin.z < Height && !(EntityFlags&EF_Fly) && !(EntityFlags&EF_IgnoreCeilingStep)) {
      // mobj must lower itself to fit
      PushLine(tmtrace, checkOnly);
      //printf("*** WORLD(1)!\n");
      return false;
    }

    if (EntityFlags&EF_Fly) {
      // when flying, slide up or down blocking lines until the actor is not blocked
      if (Origin.z+Height > tmtrace.CeilingZ) {
        // if sliding down, make sure we don't have another object below
        if ((!tmtrace.BlockingMobj || !tmtrace.BlockingMobj->CheckOnmobj() ||
            (tmtrace.BlockingMobj->CheckOnmobj() &&
             tmtrace.BlockingMobj->CheckOnmobj() != this)) &&
            (!CheckOnmobj() || (CheckOnmobj() &&
             CheckOnmobj() != tmtrace.BlockingMobj)))
        {
          if (!checkOnly) Velocity.z = -8.0f*35.0f;
        }
        PushLine(tmtrace, checkOnly);
        return false;
      } else if (Origin.z < tmtrace.FloorZ && tmtrace.FloorZ-tmtrace.DropOffZ > MaxStepHeight) {
        // check to make sure there's nothing in the way for the step up
        if ((!tmtrace.BlockingMobj || !tmtrace.BlockingMobj->CheckOnmobj() ||
            (tmtrace.BlockingMobj->CheckOnmobj() &&
             tmtrace.BlockingMobj->CheckOnmobj() != this)) &&
            (!CheckOnmobj() || (CheckOnmobj() &&
             CheckOnmobj() != tmtrace.BlockingMobj)))
        {
          if (!checkOnly) Velocity.z = 8.0f*35.0f;
        }
        PushLine(tmtrace, checkOnly);
        return false;
      }
    }

    if (!(EntityFlags&EF_IgnoreFloorStep)) {
      if (tmtrace.FloorZ-Origin.z > MaxStepHeight) {
        // too big a step up
        if (EntityFlags&EF_CanJump && Health > 0.0f) {
          // check to make sure there's nothing in the way for the step up
          if (!Velocity.z || tmtrace.FloorZ-Origin.z > 48.0f ||
              (tmtrace.BlockingMobj && tmtrace.BlockingMobj->CheckOnmobj()) ||
              TestMobjZ(TVec(newPos.x, newPos.y, tmtrace.FloorZ)))
          {
            PushLine(tmtrace, checkOnly);
            //printf("*** WORLD(2)!\n");
            return false;
          }
        } else {
          PushLine(tmtrace, checkOnly);
          //printf("*** WORLD(3)!\n");
          return false;
        }
      }

      if ((EntityFlags&EF_Missile) && !(EntityFlags&EF_StepMissile) && tmtrace.FloorZ > Origin.z) {
        PushLine(tmtrace, checkOnly);
        //printf("*** WORLD(4)!\n");
        return false;
      }

      if (Origin.z < tmtrace.FloorZ) {
        if (EntityFlags&EF_StepMissile) {
          Origin.z = tmtrace.FloorZ;
          // if moving down, cancel vertical component of velocity
          if (Velocity.z < 0) Velocity.z = 0.0f;
        }
        // check to make sure there's nothing in the way for the step up
        if (TestMobjZ(TVec(newPos.x, newPos.y, tmtrace.FloorZ))) {
          PushLine(tmtrace, checkOnly);
          //printf("*** WORLD(5)!\n");
          return false;
        }
      }
    }

    // killough 3/15/98: Allow certain objects to drop off
    if ((!AllowDropOff && !(EntityFlags&EF_DropOff) &&
        !(EntityFlags&EF_Float) && !(EntityFlags&EF_Missile)) ||
        (EntityFlags&EF_NoDropOff))
    {
      if (!(EntityFlags&EF_AvoidingDropoff)) {
        float floorz = tmtrace.FloorZ;
        // [RH] If the thing is standing on something, use its current z as the floorz.
        // This is so that it does not walk off of things onto a drop off.
        if (EntityFlags&EF_OnMobj) floorz = max2(Origin.z, tmtrace.FloorZ);

        if ((floorz-tmtrace.DropOffZ > MaxDropoffHeight) && !(EntityFlags&EF_Blasted)) {
          // Can't move over a dropoff unless it's been blasted
          //printf("*** WORLD(6)!\n");
          return false;
        }
      } else {
        // special logic to move a monster off a dropoff
        // this intentionally does not check for standing on things
        if (FloorZ-tmtrace.FloorZ > MaxDropoffHeight || DropOffZ-tmtrace.DropOffZ > MaxDropoffHeight) {
          //printf("*** WORLD(7)!\n");
          return false;
        }
      }
    }

    if ((EntityFlags&EF_CantLeaveFloorpic) &&
        (tmtrace.EFloor.splane->pic != EFloor.splane->pic || tmtrace.FloorZ != Origin.z))
    {
      // must stay within a sector of a certain floor type
      //printf("*** WORLD(8)!\n");
      return false;
    }
  }

  bool OldAboveFakeFloor = false;
  bool OldAboveFakeCeiling = false;
  if (Sector->heightsec) {
    float EyeZ = (Player ? Player->ViewOrg.z : Origin.z+Height*0.5f);
    OldAboveFakeFloor = (EyeZ > Sector->heightsec->floor.GetPointZClamped(Origin));
    OldAboveFakeCeiling = (EyeZ > Sector->heightsec->ceiling.GetPointZClamped(Origin));
  }

  if (checkOnly) return true;

  // the move is ok, so link the thing into its new position
  UnlinkFromWorld();

  oldorg = Origin;
  Origin = newPos;

  LinkToWorld();

  FloorZ = tmtrace.FloorZ;
  CeilingZ = tmtrace.CeilingZ;
  DropOffZ = tmtrace.DropOffZ;
  CopyTraceFloor(&tmtrace, false);
  CopyTraceCeiling(&tmtrace, false);

  if (EntityFlags&EF_FloorClip) {
    eventHandleFloorclip();
  } else {
    FloorClip = 0.0f;
  }

  // if any special lines were hit, do the effect
  if (EntityFlags&EF_ColideWithWorld) {
    while (tmtrace.SpecHit.Num() > 0) {
      int side;
      int oldside;

      // see if the line was crossed
      ld = tmtrace.SpecHit[tmtrace.SpecHit.Num()-1];
      tmtrace.SpecHit.SetNum(tmtrace.SpecHit.Num()-1, false);
      side = ld->PointOnSide(Origin);
      oldside = ld->PointOnSide(oldorg);
      if (side != oldside) {
        if (ld->special) eventCrossSpecialLine(ld, oldside);
      }
    }
  }

  // do additional check here to avoid calling progs
  if ((OldSec->heightsec && Sector->heightsec && Sector->ActionList) ||
      (OldSec != Sector && (OldSec->ActionList || Sector->ActionList)))
  {
    eventCheckForSectorActions(OldSec, OldAboveFakeFloor, OldAboveFakeCeiling);
  }

  return true;
}


//==========================================================================
//
//  VEntity::PushLine
//
//==========================================================================
void VEntity::PushLine (const tmtrace_t &tmtrace, bool checkOnly) {
  if (checkOnly) return;
  if (EntityFlags&EF_ColideWithWorld) {
    if (EntityFlags&EF_Blasted) eventBlastedHitLine();
    int NumSpecHitTemp = tmtrace.SpecHit.Num();
    while (NumSpecHitTemp > 0) {
      --NumSpecHitTemp;
      // see if the line was crossed
      line_t *ld = tmtrace.SpecHit[NumSpecHitTemp];
      int side = ld->PointOnSide(Origin);
      eventCheckForPushSpecial(ld, side);
    }
  }
}


//**************************************************************************
//
//  SLIDE MOVE
//
//  Allows the player to slide along any angled walls.
//
//**************************************************************************

//==========================================================================
//
//  VEntity::ClipVelocity
//
//  Slide off of the impacting object
//
//==========================================================================
TVec VEntity::ClipVelocity (const TVec &in, const TVec &normal, float overbounce) {
  return in-normal*(DotProduct(in, normal)*overbounce);
}


//==========================================================================
//
//  VEntity::SlidePathTraverse
//
//==========================================================================
void VEntity::SlidePathTraverse (float &BestSlideFrac, line_t *&BestSlideLine, float x, float y, float StepVelScale) {
  TVec SlideOrg(x, y, Origin.z);
  TVec SlideDir = Velocity*StepVelScale;
  intercept_t *in;
  for (VPathTraverse It(this, &in, x, y, x+SlideDir.x, y+SlideDir.y, PT_ADDLINES); It.GetNext(); ) {
    if (!(in->Flags&intercept_t::IF_IsALine)) Host_Error("PTR_SlideTraverse: not a line?");

    line_t *li = in->line;

    bool IsBlocked = false;
    if (!(li->flags&ML_TWOSIDED) || !li->backsector) {
      if (li->PointOnSide(Origin)) continue; // don't hit the back side
      IsBlocked = true;
    } else if (li->flags&(ML_BLOCKING|ML_BLOCKEVERYTHING)) {
      IsBlocked = true;
    } else if ((EntityFlags&EF_IsPlayer) && (li->flags&ML_BLOCKPLAYERS)) {
      IsBlocked = true;
    } else if ((EntityFlags&EF_CheckLineBlockMonsters) && (li->flags&ML_BLOCKMONSTERS)) {
      IsBlocked = true;
    }

    if (!IsBlocked) {
      // set openrange, opentop, openbottom
      TVec hit_point = SlideOrg+in->frac*SlideDir;
      opening_t *open = SV_LineOpenings(li, hit_point, SPF_NOBLOCKING, true); //!(EntityFlags&EF_Missile)); // missiles ignores 3dmidtex
      open = SV_FindOpening(open, Origin.z, Origin.z+Height);

      if (open && open->range >= Height && // fits
          open->top-Origin.z >= Height && // mobj is not too high
          open->bottom-Origin.z <= MaxStepHeight) // not too big a step up
      {
        // this line doesn't block movement
        if (Origin.z < open->bottom) {
          // check to make sure there's nothing in the way for the step up
          TVec CheckOrg = Origin;
          CheckOrg.z = open->bottom;
          if (!TestMobjZ(CheckOrg)) continue;
        } else {
          continue;
        }
      }
    }

    // the line blocks movement, see if it is closer than best so far
    if (in->frac < BestSlideFrac) {
      BestSlideFrac = in->frac;
      BestSlideLine = li;
    }

    break;  // stop
  }
}


//==========================================================================
//
//  VEntity::SlideMove
//
//  The momx / momy move is bad, so try to slide along a wall.
//  Find the first line hit, move flush to it, and slide along it.
//  This is a kludgy mess.
//
//  k8: TODO: switch to beveled BSP!
//
//==========================================================================
void VEntity::SlideMove (float StepVelScale) {
  float leadx;
  float leady;
  float trailx;
  float traily;
  float newx;
  float newy;
  int hitcount;
  tmtrace_t tmtrace;
  memset((void *)&tmtrace, 0, sizeof(tmtrace)); // valgrind: AnyBlockingLine

  hitcount = 0;

  float XMove = Velocity.x*StepVelScale;
  float YMove = Velocity.y*StepVelScale;
  do {
    if (++hitcount == 3) {
      // don't loop forever
      if (!TryMove(tmtrace, TVec(Origin.x, Origin.y+YMove, Origin.z), true)) {
        TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y, Origin.z), true);
      }
      return;
    }

    // trace along the three leading corners
    if (XMove > 0.0f) {
      leadx = Origin.x+Radius;
      trailx = Origin.x-Radius;
    } else {
      leadx = Origin.x-Radius;
      trailx = Origin.x+Radius;
    }

    if (Velocity.y > 0.0f) {
      leady = Origin.y+Radius;
      traily = Origin.y-Radius;
    } else {
      leady = Origin.y-Radius;
      traily = Origin.y+Radius;
    }

    float BestSlideFrac = 1.00001f;
    line_t *BestSlideLine = nullptr;

    SlidePathTraverse(BestSlideFrac, BestSlideLine, leadx, leady, StepVelScale);
    SlidePathTraverse(BestSlideFrac, BestSlideLine, trailx, leady, StepVelScale);
    SlidePathTraverse(BestSlideFrac, BestSlideLine, leadx, traily, StepVelScale);

    // move up to the wall
    if (BestSlideFrac == 1.00001f) {
      // the move must have hit the middle, so stairstep
      if (!TryMove(tmtrace, TVec(Origin.x, Origin.y+YMove, Origin.z), true)) {
        TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y, Origin.z), true);
      }
      return;
    }

    // fudge a bit to make sure it doesn't hit
    BestSlideFrac -= 0.03125f;
    if (BestSlideFrac > 0.0f) {
      newx = XMove*BestSlideFrac;
      newy = YMove*BestSlideFrac;

      if (!TryMove(tmtrace, TVec(Origin.x+newx, Origin.y+newy, Origin.z), true)) {
        if (!TryMove(tmtrace, TVec(Origin.x, Origin.y+YMove, Origin.z), true)) {
          TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y, Origin.z), true);
        }
        return;
      }
    }

    // now continue along the wall
    // first calculate remainder
    BestSlideFrac = 1.0f-(BestSlideFrac+0.03125f);

    if (BestSlideFrac > 1.0f) BestSlideFrac = 1.0f;
    if (BestSlideFrac <= 0.0f) return;

    // clip the moves
    // k8: don't multiply z, 'cause it makes jumping against a wall unpredictably hard
    Velocity.x *= BestSlideFrac;
    Velocity.y *= BestSlideFrac;
    Velocity = ClipVelocity(Velocity, BestSlideLine->normal, 1.0f);
    //Velocity = ClipVelocity(Velocity*BestSlideFrac, BestSlideLine->normal, 1.0f);
    XMove = Velocity.x*StepVelScale;
    YMove = Velocity.y*StepVelScale;
  } while (!TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y+YMove, Origin.z), true));
}


//**************************************************************************
//
//  BOUNCING
//
//  Bounce missile against walls
//
//**************************************************************************

//============================================================================
//
//  VEntity::BounceWall
//
//============================================================================
void VEntity::BounceWall (float overbounce, float bouncefactor) {
  TVec SlideOrg;

  if (Velocity.x > 0.0f) SlideOrg.x = Origin.x+Radius; else SlideOrg.x = Origin.x-Radius;
  if (Velocity.y > 0.0f) SlideOrg.y = Origin.y+Radius; else SlideOrg.y = Origin.y-Radius;
  SlideOrg.z = Origin.z;
  TVec SlideDir = Velocity*host_frametime;
  line_t *BestSlideLine = nullptr;
  intercept_t *in;

  for (VPathTraverse It(this, &in, SlideOrg.x, SlideOrg.y, SlideOrg.x+SlideDir.x, SlideOrg.y+SlideDir.y, PT_ADDLINES); It.GetNext(); ) {
    if (!(in->Flags&intercept_t::IF_IsALine)) Host_Error("PTR_BounceTraverse: not a line?");
    line_t *li = in->line;
    TVec hit_point = SlideOrg+in->frac*SlideDir;

    if (li->flags&ML_TWOSIDED) {
      // set openrange, opentop, openbottom
      opening_t *open = SV_LineOpenings(li, hit_point, SPF_NOBLOCKING, true); //!(EntityFlags&EF_Missile)); // missiles ignores 3dmidtex
      open = SV_FindOpening(open, Origin.z, Origin.z+Height);

      if (open && open->range >= Height && // fits
          Origin.z+Height <= open->top &&
          Origin.z >= open->bottom) // mobj is not too high
      {
        continue; // this line doesn't block movement
      }
    } else {
      if (li->PointOnSide(Origin)) continue; // don't hit the back side
    }

    BestSlideLine = li;
    break; // don't bother going farther
  }

  if (BestSlideLine) {
    TAVec delta_ang;
    TAVec lineang;
    TVec delta(0, 0, 0);

    // convert BesSlideLine normal to an angle
    VectorAngles(BestSlideLine->normal, lineang);
    if (BestSlideLine->PointOnSide(Origin) == 1) lineang.yaw += 180.0f;

    // convert the line angle back to a vector, so that
    // we can use it to calculate the delta against
    // the Velocity vector
    AngleVector(lineang, delta);
    delta = (delta*2.0f)-Velocity;

    // finally get the delta angle to use
    VectorAngles(delta, delta_ang);

    Velocity.x = (Velocity.x*bouncefactor)*cos(delta_ang.yaw);
    Velocity.y = (Velocity.y*bouncefactor)*sin(delta_ang.yaw);
    Velocity = ClipVelocity(Velocity, BestSlideLine->normal, overbounce);
  }
}



//**************************************************************************
//
//  TEST ON MOBJ
//
//**************************************************************************

//=============================================================================
//
//  TestMobjZ
//
//  Checks if the new Z position is legal
//  returns blocking thing
//
//=============================================================================
VEntity *VEntity::TestMobjZ (const TVec &TryOrg) {
  // can't hit thing
  if (!(EntityFlags&EF_ColideWithThings)) return nullptr;
  // not solid
  if (!(EntityFlags&EF_Solid)) return nullptr;

  // the bounding box is extended by MAXRADIUS because mobj_ts are grouped
  // into mapblocks based on their origin point, and can overlap into adjacent
  // blocks by up to MAXRADIUS units
  const int xl = MapBlock(TryOrg.x-Radius-XLevel->BlockMapOrgX-MAXRADIUS);
  const int xh = MapBlock(TryOrg.x+Radius-XLevel->BlockMapOrgX+MAXRADIUS);
  const int yl = MapBlock(TryOrg.y-Radius-XLevel->BlockMapOrgY-MAXRADIUS);
  const int yh = MapBlock(TryOrg.y+Radius-XLevel->BlockMapOrgY+MAXRADIUS);

  // xl->xh, yl->yh determine the mapblock set to search
  for (int bx = xl; bx <= xh; ++bx) {
    for (int by = yl; by <= yh; ++by) {
      for (VBlockThingsIterator Other(XLevel, bx, by); Other; ++Other) {
        if (*Other == this) continue; // don't clip against self
        if (!(Other->EntityFlags&EF_ColideWithThings)) continue; // can't hit thing
        if (!(Other->EntityFlags&EF_Solid)) continue; // not solid
        if (TryOrg.z > Other->Origin.z+Other->Height) continue; // over thing
        if (TryOrg.z+Height < Other->Origin.z) continue; // under thing
        float blockdist = Other->Radius+Radius;
        if (fabsf(Other->Origin.x-TryOrg.x) >= blockdist ||
            fabsf(Other->Origin.y-TryOrg.y) >= blockdist)
        {
          // didn't hit thing
          continue;
        }
        return *Other;
      }
    }
  }

  return nullptr;
}


//=============================================================================
//
//  VEntity::FakeZMovement
//
//  Fake the zmovement so that we can check if a move is legal
//
//=============================================================================
TVec VEntity::FakeZMovement () {
  TVec Ret = TVec(0, 0, 0);
  eventCalcFakeZMovement(Ret, host_frametime);
  // clip movement
  if (Ret.z <= FloorZ) Ret.z = FloorZ; // hit the floor
  if (Ret.z+Height > CeilingZ) Ret.z = CeilingZ-Height; // hit the ceiling
  return Ret;
}


//=============================================================================
//
//  VEntity::CheckOnmobj
//
//  Checks if an object is above another object
//
//=============================================================================
VEntity *VEntity::CheckOnmobj () {
  return TestMobjZ(FakeZMovement());
}


//==========================================================================
//
//  VEntity::CheckSides
//
// This routine checks for Lost Souls trying to be spawned    // phares
// across 1-sided lines, impassible lines, or "monsters can't //   |
// cross" lines. Draw an imaginary line between the PE        //   V
// and the new Lost Soul spawn spot. If that line crosses
// a 'blocking' line, then disallow the spawn. Only search
// lines in the blocks of the blockmap where the bounding box
// of the trajectory line resides. Then check bounding box
// of the trajectory vs. the bounding box of each blocking
// line to see if the trajectory and the blocking line cross.
// Then check the PE and LS to see if they're on different
// sides of the blocking line. If so, return true, otherwise
// false.
//
//==========================================================================
bool VEntity::CheckSides (TVec lsPos) {
  int bx,by,xl,xh,yl,yh;

  // here is the bounding box of the trajectory
  float tmbbox[4];
  tmbbox[BOX2D_LEFT] = min2(Origin.x, lsPos.x);
  tmbbox[BOX2D_RIGHT] = max2(Origin.x, lsPos.x);
  tmbbox[BOX2D_TOP] = max2(Origin.y, lsPos.y);
  tmbbox[BOX2D_BOTTOM] = min2(Origin.y, lsPos.y);

  // determine which blocks to look in for blocking lines
  xl = MapBlock(tmbbox[BOX2D_LEFT]-XLevel->BlockMapOrgX);
  xh = MapBlock(tmbbox[BOX2D_RIGHT]-XLevel->BlockMapOrgX);
  yl = MapBlock(tmbbox[BOX2D_BOTTOM]-XLevel->BlockMapOrgY);
  yh = MapBlock(tmbbox[BOX2D_TOP]-XLevel->BlockMapOrgY);

  //k8: is this right?
  int projblk = (EntityFlags&VEntity::EF_Missile ? ML_BLOCKPROJECTILE : 0);

  // xl->xh, yl->yh determine the mapblock set to search
  //++validcount; // prevents checking same line twice
  XLevel->IncrementValidCount();
  for (bx = xl; bx <= xh; ++bx) {
    for (by = yl; by <= yh; ++by) {
      line_t *ld;
      for (VBlockLinesIterator It(XLevel, bx, by, &ld); It.GetNext(); ) {
        // Checks to see if a PE->LS trajectory line crosses a blocking
        // line. Returns false if it does.
        //
        // tmbbox holds the bounding box of the trajectory. If that box
        // does not touch the bounding box of the line in question,
        // then the trajectory is not blocked. If the PE is on one side
        // of the line and the LS is on the other side, then the
        // trajectory is blocked.
        //
        // Currently this assumes an infinite line, which is not quite
        // correct. A more correct solution would be to check for an
        // intersection of the trajectory and the line, but that takes
        // longer and probably really isn't worth the effort.

        if (ld->flags&(ML_BLOCKING|ML_BLOCKMONSTERS|ML_BLOCKEVERYTHING|projblk)) {
          if (tmbbox[BOX2D_LEFT] <= ld->bbox2d[BOX2D_RIGHT] &&
              tmbbox[BOX2D_RIGHT] >= ld->bbox2d[BOX2D_LEFT] &&
              tmbbox[BOX2D_TOP] >= ld->bbox2d[BOX2D_BOTTOM] &&
              tmbbox[BOX2D_BOTTOM] <= ld->bbox2d[BOX2D_TOP])
          {
            if (ld->PointOnSide(Origin) != ld->PointOnSide(lsPos)) return true; // line blocks trajectory
          }
        }

        // line doesn't block trajectory
      }
    }
  }

  return false;
}


//=============================================================================
//
//  CheckDropOff
//
//  killough 11/98:
//
//  Monsters try to move away from tall dropoffs.
//
//  In Doom, they were never allowed to hang over dropoffs, and would remain
//  stuck if involuntarily forced over one. This logic, combined with P_TryMove,
//  allows monsters to free themselves without making them tend to hang over
//  dropoffs.
//
//=============================================================================
void VEntity::CheckDropOff (float &DeltaX, float &DeltaY) {
  float t_bbox[4];
  int xl;
  int xh;
  int yl;
  int yh;
  int bx;
  int by;

  // try to move away from a dropoff
  DeltaX = 0;
  DeltaY = 0;

  t_bbox[BOX2D_TOP] = Origin.y+Radius;
  t_bbox[BOX2D_BOTTOM] = Origin.y-Radius;
  t_bbox[BOX2D_RIGHT] = Origin.x+Radius;
  t_bbox[BOX2D_LEFT] = Origin.x-Radius;

  xl = MapBlock(t_bbox[BOX2D_LEFT]-XLevel->BlockMapOrgX);
  xh = MapBlock(t_bbox[BOX2D_RIGHT]-XLevel->BlockMapOrgX);
  yl = MapBlock(t_bbox[BOX2D_BOTTOM]-XLevel->BlockMapOrgY);
  yh = MapBlock(t_bbox[BOX2D_TOP]-XLevel->BlockMapOrgY);

  // check lines
  //++validcount;
  XLevel->IncrementValidCount();
  for (bx = xl; bx <= xh; ++bx) {
    for (by = yl; by <= yh; ++by) {
      line_t *line;
      for (VBlockLinesIterator It(XLevel, bx, by, &line); It.GetNext(); ) {
        if (!line->backsector) continue; // ignore one-sided linedefs
        // linedef must be contacted
        if (t_bbox[BOX2D_RIGHT] > line->bbox2d[BOX2D_LEFT] &&
            t_bbox[BOX2D_LEFT] < line->bbox2d[BOX2D_RIGHT] &&
            t_bbox[BOX2D_TOP] > line->bbox2d[BOX2D_BOTTOM] &&
            t_bbox[BOX2D_BOTTOM] < line->bbox2d[BOX2D_TOP] &&
            P_BoxOnLineSide(t_bbox, line) == -1)
        {
          // new logic for 3D Floors
          /*
          sec_region_t *FrontReg = SV_FindThingGap(line->frontsector, Origin, Height);
          sec_region_t *BackReg = SV_FindThingGap(line->backsector, Origin, Height);
          float front = FrontReg->efloor.GetPointZClamped(Origin);
          float back = BackReg->efloor.GetPointZClamped(Origin);
          */
          TSecPlaneRef ffloor, fceiling;
          TSecPlaneRef bfloor, bceiling;
          SV_FindGapFloorCeiling(line->frontsector, Origin, Height, ffloor, fceiling);
          SV_FindGapFloorCeiling(line->backsector, Origin, Height, bfloor, bceiling);
          const float front = ffloor.GetPointZClamped(Origin);
          const float back = bfloor.GetPointZClamped(Origin);

          // the monster must contact one of the two floors, and the other must be a tall dropoff
          TVec Dir;
          if (back == Origin.z && front < Origin.z-MaxDropoffHeight) {
            // front side dropoff
            Dir = line->normal;
          } else if (front == Origin.z && back < Origin.z-MaxDropoffHeight) {
            // back side dropoff
            Dir = -line->normal;
          } else {
            continue;
          }
          // move away from dropoff at a standard speed
          // multiple contacted linedefs are cumulative (e.g. hanging over corner)
          DeltaX += Dir.x*32.0f;
          DeltaY += Dir.y*32.0f;
        }
      }
    }
  }
}


//==========================================================================
//
//  VRoughBlockSearchIterator
//
//==========================================================================
VRoughBlockSearchIterator::VRoughBlockSearchIterator (VEntity *ASelf, int ADistance, VEntity **AEntPtr)
  : Self(ASelf)
  , Distance(ADistance)
  , Ent(nullptr)
  , EntPtr(AEntPtr)
  , Count(1)
  , CurrentEdge(-1)
{
  StartX = MapBlock(Self->Origin.x-Self->XLevel->BlockMapOrgX);
  StartY = MapBlock(Self->Origin.y-Self->XLevel->BlockMapOrgY);

  // start with current block
  if (StartX >= 0 && StartX < Self->XLevel->BlockMapWidth &&
      StartY >= 0 && StartY < Self->XLevel->BlockMapHeight)
  {
    Ent = Self->XLevel->BlockLinks[StartY*Self->XLevel->BlockMapWidth+StartX];
  }
}


//==========================================================================
//
//  VRoughBlockSearchIterator::GetNext
//
//==========================================================================
bool VRoughBlockSearchIterator::GetNext () {
  int BlockX;
  int BlockY;

  for (;;) {
    if (Ent) {
      *EntPtr = Ent;
      Ent = Ent->BlockMapNext;
      return true;
    }

    switch (CurrentEdge) {
      case 0:
        // trace the first block section (along the top)
        if (BlockIndex <= FirstStop) {
          Ent = Self->XLevel->BlockLinks[BlockIndex];
          ++BlockIndex;
        } else {
          CurrentEdge = 1;
          --BlockIndex;
        }
        break;
      case 1:
        // trace the second block section (right edge)
        if (BlockIndex <= SecondStop) {
          Ent = Self->XLevel->BlockLinks[BlockIndex];
          BlockIndex += Self->XLevel->BlockMapWidth;
        } else {
          CurrentEdge = 2;
          BlockIndex -= Self->XLevel->BlockMapWidth;
        }
        break;
      case 2:
        // trace the third block section (bottom edge)
        if (BlockIndex >= ThirdStop) {
          Ent = Self->XLevel->BlockLinks[BlockIndex];
          --BlockIndex;
        } else {
          CurrentEdge = 3;
          ++BlockIndex;
        }
        break;
      case 3:
        // trace the final block section (left edge)
        if (BlockIndex > FinalStop) {
          Ent = Self->XLevel->BlockLinks[BlockIndex];
          BlockIndex -= Self->XLevel->BlockMapWidth;
        } else {
          CurrentEdge = -1;
        }
        break;
      default:
        if (Count > Distance) return false; // we are done
        BlockX = StartX-Count;
        BlockY = StartY-Count;

        if (BlockY < 0) {
          BlockY = 0;
        } else if (BlockY >= Self->XLevel->BlockMapHeight) {
          BlockY = Self->XLevel->BlockMapHeight-1;
        }
        if (BlockX < 0) {
          BlockX = 0;
        } else if (BlockX >= Self->XLevel->BlockMapWidth) {
          BlockX = Self->XLevel->BlockMapWidth-1;
        }
        BlockIndex = BlockY*Self->XLevel->BlockMapWidth+BlockX;
        FirstStop = StartX+Count;
        if (FirstStop < 0) { ++Count; break; }
        if (FirstStop >= Self->XLevel->BlockMapWidth) FirstStop = Self->XLevel->BlockMapWidth-1;
        SecondStop = StartY+Count;
        if (SecondStop < 0) { ++Count; break; }
        if (SecondStop >= Self->XLevel->BlockMapHeight) SecondStop = Self->XLevel->BlockMapHeight-1;
        ThirdStop = SecondStop*Self->XLevel->BlockMapWidth+BlockX;
        SecondStop = SecondStop*Self->XLevel->BlockMapWidth+FirstStop;
        FirstStop += BlockY*Self->XLevel->BlockMapWidth;
        FinalStop = BlockIndex;
        ++Count;
        CurrentEdge = 0;
        break;
    }
  }
  return false;
}


//==========================================================================
//
//  Script natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VEntity, CheckWater) {
  P_GET_SELF;
  RET_INT(Self->CheckWater());
}

IMPLEMENT_FUNCTION(VEntity, CheckDropOff) {
  P_GET_PTR(float, DeltaX);
  P_GET_PTR(float, DeltaY);
  P_GET_SELF;
  Self->CheckDropOff(*DeltaX, *DeltaY);
}

IMPLEMENT_FUNCTION(VEntity, CheckPosition) {
  P_GET_VEC(Pos);
  P_GET_SELF;
  RET_BOOL(Self->CheckPosition(Pos));
}

// native final bool CheckRelPosition (tmtrace_t *tmtrace, TVec Pos, optional bool noPickups/*=false*/, optional bool ignoreMonsters, optional bool ignorePlayers);
IMPLEMENT_FUNCTION(VEntity, CheckRelPosition) {
  P_GET_BOOL_OPT(ignorePlayers, false);
  P_GET_BOOL_OPT(ignoreMonsters, false);
  P_GET_BOOL_OPT(noPickups, false);
  P_GET_VEC(Pos);
  P_GET_PTR(tmtrace_t, tmtrace);
  P_GET_SELF;
  RET_BOOL(Self->CheckRelPosition(*tmtrace, Pos, noPickups, ignoreMonsters, ignorePlayers));
}

IMPLEMENT_FUNCTION(VEntity, CheckSides) {
  P_GET_VEC(lsPos);
  P_GET_SELF;
  RET_BOOL(Self->CheckSides(lsPos));
}

IMPLEMENT_FUNCTION(VEntity, TryMove) {
  P_GET_BOOL_OPT(checkOnly, false);
  P_GET_BOOL(AllowDropOff);
  P_GET_VEC(Pos);
  P_GET_SELF;
  tmtrace_t tmtrace;
  RET_BOOL(Self->TryMove(tmtrace, Pos, AllowDropOff, checkOnly));
}

IMPLEMENT_FUNCTION(VEntity, TryMoveEx) {
  P_GET_BOOL_OPT(checkOnly, false);
  P_GET_BOOL(AllowDropOff);
  P_GET_VEC(Pos);
  P_GET_PTR(tmtrace_t, tmtrace);
  P_GET_SELF;
  RET_BOOL(Self->TryMove(*tmtrace, Pos, AllowDropOff, checkOnly));
}

IMPLEMENT_FUNCTION(VEntity, TestMobjZ) {
  P_GET_SELF;
  RET_BOOL(!Self->TestMobjZ(Self->Origin));
}

IMPLEMENT_FUNCTION(VEntity, SlideMove) {
  P_GET_FLOAT(StepVelScale);
  P_GET_SELF;
  Self->SlideMove(StepVelScale);
}

IMPLEMENT_FUNCTION(VEntity, BounceWall) {
  P_GET_FLOAT(overbounce);
  P_GET_FLOAT(bouncefactor);
  P_GET_SELF;
  Self->BounceWall(overbounce, bouncefactor);
}

IMPLEMENT_FUNCTION(VEntity, CheckOnmobj) {
  P_GET_SELF;
  RET_REF(Self->CheckOnmobj());
}

IMPLEMENT_FUNCTION(VEntity, LinkToWorld) {
  P_GET_BOOL_OPT(properFloorCheck, false);
  P_GET_SELF;
  Self->LinkToWorld(properFloorCheck);
}

IMPLEMENT_FUNCTION(VEntity, UnlinkFromWorld) {
  P_GET_SELF;
  Self->UnlinkFromWorld();
}

IMPLEMENT_FUNCTION(VEntity, CanSee) {
  P_GET_BOOL_OPT(disableBetterSight, false);
  P_GET_REF(VEntity, Other);
  P_GET_SELF;
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("empty `self`!"); }
  RET_BOOL(Self->CanSee(Other, disableBetterSight));
}

IMPLEMENT_FUNCTION(VEntity, CanSeeAdv) {
  P_GET_REF(VEntity, Other);
  P_GET_SELF;
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("empty `self`!"); }
  RET_BOOL(Self->CanSee(Other, false, true));
}

IMPLEMENT_FUNCTION(VEntity, CanShoot) {
  P_GET_REF(VEntity, Other);
  P_GET_SELF;
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("empty `self`!"); }
  RET_BOOL(Self->CanShoot(Other));
}

IMPLEMENT_FUNCTION(VEntity, RoughBlockSearch) {
  P_GET_INT(Distance);
  P_GET_PTR(VEntity*, EntPtr);
  P_GET_SELF;
  RET_PTR(new VRoughBlockSearchIterator(Self, Distance, EntPtr));
}
