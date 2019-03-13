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
#include "gamedefs.h"
#include "sv_local.h"


// polyobj line start special
#define PO_LINE_START     (1)
#define PO_LINE_EXPLICIT  (5)

//#define PO_MAXPOLYSEGS    (64)
#define PO_MAXPOLYSEGS    (512)


static VCvarB pobj_allow_several_in_subsector("pobj_allow_several_in_subsector", false, "Allow several polyobjs in one subsector (WARNING! THE ENGINE MAY CRASH!)?", CVAR_PreInit|CVAR_Archive);
int pobj_allow_several_in_subsector_override = 0; // <0: disable; >0: enable


//==========================================================================
//
//  VLevel::SpawnPolyobj
//
//==========================================================================
void VLevel::SpawnPolyobj (float x, float y, int tag, bool crush, bool hurt) {
  int index = NumPolyObjs++;
  polyobj_t *Temp = PolyObjs;
  PolyObjs = new polyobj_t[NumPolyObjs];
  if (Temp) {
    for (int i = 0; i < NumPolyObjs-1; ++i) PolyObjs[i] = Temp[i];
    delete[] Temp;
    Temp = nullptr;
  }
  memset((void *)(&PolyObjs[index]), 0, sizeof(polyobj_t));

  PolyObjs[index].startSpot.x = x;
  PolyObjs[index].startSpot.y = y;
  for (int i = 0; i < NumSegs; ++i) {
    if (!Segs[i].linedef) continue;
    if (Segs[i].linedef->special == PO_LINE_START && Segs[i].linedef->arg1 == tag) {
      Segs[i].linedef->special = 0;
      Segs[i].linedef->arg1 = 0;
      int PolySegCount = 1;
      TVec PolyStart = *Segs[i].v1;
      IterFindPolySegs(*Segs[i].v2, nullptr, PolySegCount, PolyStart);

      PolyObjs[index].numsegs = PolySegCount;
      PolyObjs[index].segs = new seg_t*[PolySegCount];
      *(PolyObjs[index].segs) = &Segs[i]; // insert the first seg
      // set sector's line count to 0 to force it not to be
      // rendered even if we do a no-clip into it
      // -- FB -- I'm disabling this behavior
      // k8: and i am enabling it again
      Segs[i].frontsector->linecount = 0;
      IterFindPolySegs(*Segs[i].v2, PolyObjs[index].segs+1, PolySegCount, PolyStart);
      if (crush) {
        PolyObjs[index].PolyFlags |= polyobj_t::PF_Crush;
      } else {
        PolyObjs[index].PolyFlags &= ~polyobj_t::PF_Crush;
      }
      if (hurt) {
        PolyObjs[index].PolyFlags |= polyobj_t::PF_HurtOnTouch;
      } else {
        PolyObjs[index].PolyFlags &= ~polyobj_t::PF_HurtOnTouch;
      }
      PolyObjs[index].tag = tag;
      PolyObjs[index].seqType = Segs[i].linedef->arg3;
      //if (PolyObjs[index].seqType < 0 || PolyObjs[index].seqType >= SEQTYPE_NUMSEQ) PolyObjs[index].seqType = 0;
      break;
    }
  }
  if (!PolyObjs[index].segs) {
    // didn't find a polyobj through PO_LINE_START
    int psIndex = 0;
    seg_t *polySegList[PO_MAXPOLYSEGS];
    PolyObjs[index].numsegs = 0;
    for (int j = 1; j < PO_MAXPOLYSEGS; ++j) {
      int psIndexOld = psIndex;
      for (int i = 0; i < NumSegs; ++i) {
        if (!Segs[i].linedef) continue;
        if (Segs[i].linedef->special == PO_LINE_EXPLICIT && Segs[i].linedef->arg1 == tag) {
          if (!Segs[i].linedef->arg2) Sys_Error("Explicit line missing order number (probably %d) in poly %d.", j+1, tag);
          if (Segs[i].linedef->arg2 == j) {
            polySegList[psIndex] = &Segs[i];
            // set sector's line count to 0 to force it not to be
            // rendered even if we do a no-clip into it
            // -- FB -- I'm disabling this behavior
            // k8: and i am enabling it again
            Segs[i].frontsector->linecount = 0;
            PolyObjs[index].numsegs++;
            ++psIndex;
            check(psIndex <= PO_MAXPOLYSEGS);
          }
        }
      }
      // clear out any specials for these segs
      // we cannot clear them out in the above loop,
      // since we aren't guaranteed one seg per linedef
      for (int i = 0; i < NumSegs; ++i) {
        if (!Segs[i].linedef) continue;
        if (Segs[i].linedef->special == PO_LINE_EXPLICIT &&
            Segs[i].linedef->arg1 == tag &&
            Segs[i].linedef->arg2 == j)
        {
          Segs[i].linedef->special = 0;
          Segs[i].linedef->arg1 = 0;
        }
      }
      if (psIndex == psIndexOld) {
        // check if an explicit line order has been skipped
        // a line has been skipped if there are any more explicit
        // lines with the current tag value
        for (int i = 0; i < NumSegs; ++i) {
          if (!Segs[i].linedef) continue;
          if (Segs[i].linedef->special == PO_LINE_EXPLICIT && Segs[i].linedef->arg1 == tag) {
            Sys_Error("Missing explicit line %d for poly %d\n", j, tag);
          }
        }
      }
    }
    if (PolyObjs[index].numsegs) {
      if (crush) {
        PolyObjs[index].PolyFlags |= polyobj_t::PF_Crush;
      } else {
        PolyObjs[index].PolyFlags &= ~polyobj_t::PF_Crush;
      }
      PolyObjs[index].tag = tag;
      PolyObjs[index].segs = new seg_t*[PolyObjs[index].numsegs];
      for (int i = 0; i < PolyObjs[index].numsegs; ++i) {
        PolyObjs[index].segs[i] = polySegList[i];
      }
      PolyObjs[index].seqType = (*PolyObjs[index].segs)->linedef->arg4;
    }
    // next, change the polyobjs first line to point to a mirror if it exists
    (*PolyObjs[index].segs)->linedef->arg2 = (*PolyObjs[index].segs)->linedef->arg3;
  }
}


//==========================================================================
//
//  VLevel::IterFindPolySegs
//
//  Passing nullptr for segList will cause IterFindPolySegs to count the number
//  of segs in the polyobj
//
//==========================================================================
void VLevel::IterFindPolySegs (const TVec &From, seg_t **segList,
                               int &PolySegCount, const TVec &PolyStart)
{
  if (From == PolyStart) return; // reached starting vertex
  for (int i = 0; i < NumSegs; i++) {
    if (!Segs[i].linedef) continue; // skip minisegs
    if (*Segs[i].v1 == From) {
      if (!segList) {
        // count segs
        ++PolySegCount;
      } else {
        // add to the list
        *segList++ = &Segs[i];
        // set sector's line count to 0 to force it not to be
        // rendered even if we do a no-clip into it
        // -- FB -- I'm disabling this behavior
        // k8: and i am enabling it again
        Segs[i].frontsector->linecount = 0;
      }
      return IterFindPolySegs(*Segs[i].v2, segList, PolySegCount, PolyStart);
    }
  }
  Host_Error("Non-closed Polyobj located.\n");
}


//==========================================================================
//
//  VLevel::AddPolyAnchorPoint
//
//==========================================================================
void VLevel::AddPolyAnchorPoint (float x, float y, int tag) {
  ++NumPolyAnchorPoints;
  PolyAnchorPoint_t *Temp = PolyAnchorPoints;
  PolyAnchorPoints = new PolyAnchorPoint_t[NumPolyAnchorPoints];
  if (Temp) {
    for (int i = 0; i < NumPolyAnchorPoints-1; ++i) PolyAnchorPoints[i] = Temp[i];
    delete[] Temp;
    Temp = nullptr;
  }

  PolyAnchorPoint_t &A = PolyAnchorPoints[NumPolyAnchorPoints-1];
  A.x = x;
  A.y = y;
  A.tag = tag;
}


//==========================================================================
//
//  VLevel::InitPolyobjs
//
//==========================================================================
void VLevel::InitPolyobjs () {
  for (int i = 0; i < NumPolyAnchorPoints; ++i) {
    TranslatePolyobjToStartSpot(PolyAnchorPoints[i].x, PolyAnchorPoints[i].y, PolyAnchorPoints[i].tag);
  }

  // check for a startspot without an anchor point
  for (int i = 0; i < NumPolyObjs; ++i) {
    if (!PolyObjs[i].originalPts) {
      Sys_Error("StartSpot located without an Anchor point: %d", PolyObjs[i].tag);
    }
  }

  InitPolyBlockMap();
}


//==========================================================================
//
//  VLevel::TranslatePolyobjToStartSpot
//
//==========================================================================
void VLevel::TranslatePolyobjToStartSpot (float originX, float originY, int tag) {
  polyobj_t *po = nullptr;
  for (int i = 0; i < NumPolyObjs; ++i) {
    if (PolyObjs[i].tag == tag) {
      po = &PolyObjs[i];
      break;
    }
  }
  if (!po) Host_Error("Unable to match polyobj tag: %d", tag); // didn't match the tag with a polyobj tag
  if (po->segs == nullptr) Host_Error("Anchor point located without a StartSpot point: %d", tag);
  po->originalPts = new vertex_t[po->numsegs];
  po->prevPts = new vertex_t[po->numsegs];
  float deltaX = originX-po->startSpot.x;
  float deltaY = originY-po->startSpot.y;

  seg_t **tempSeg = po->segs;
  vertex_t *tempPt = po->originalPts;
  vertex_t avg(0, 0, 0); // used to find a polyobj's centre, and hence subsector

  for (int i = 0; i < po->numsegs; ++i, ++tempSeg, ++tempPt) {
    seg_t **veryTempSeg = po->segs;
    for (; veryTempSeg != tempSeg; ++veryTempSeg) {
      if ((*veryTempSeg)->v1 == (*tempSeg)->v1) break;
    }
    if (veryTempSeg == tempSeg) {
      // the point hasn't been translated yet
      (*tempSeg)->v1->x -= deltaX;
      (*tempSeg)->v1->y -= deltaY;
    }
    avg.x += (*tempSeg)->v1->x;
    avg.y += (*tempSeg)->v1->y;
    // the original Pts are based off the startSpot Pt, and are unique to each seg, not each linedef
    *tempPt = *(*tempSeg)->v1-po->startSpot;
  }
  avg.x /= po->numsegs;
  avg.y /= po->numsegs;
  subsector_t *sub = PointInSubsector(avg);
  if (sub->poly != nullptr && sub->poly != po) {
    bool allowed = false;
    if (pobj_allow_several_in_subsector_override) {
      allowed = (pobj_allow_several_in_subsector_override > 0); // <0: disable; >0: enable
    } else {
      allowed = pobj_allow_several_in_subsector;
    }
    if (allowed) {
      GCon->Logf(NAME_Error, "Multiple polyobjs in a single subsector.");
    } else {
      Sys_Error("Multiple polyobjs in a single subsector.");
    }
    //FIXME!
    sub->poly->subsector = nullptr;
  }
  sub->poly = po;
  po->subsector = sub;

  UpdatePolySegs(po);
}


//==========================================================================
//
//  VLevel::UpdatePolySegs
//
//==========================================================================
void VLevel::UpdatePolySegs (polyobj_t *po) {
  IncrementValidCount();
  seg_t **segList = po->segs;
  for (int count = po->numsegs; count; --count, ++segList) {
    if ((*segList)->linedef->validcount != validcount) {
      // recalc lines's slope type, bounding box, normal and dist
      CalcLine((*segList)->linedef);
      (*segList)->linedef->validcount = validcount;
    }
    // recalc seg's normal and dist
    CalcSeg(*segList);
    if (RenderData) RenderData->SegMoved(*segList);
  }
}


//==========================================================================
//
//  VLevel::InitPolyBlockMap
//
//==========================================================================
void VLevel::InitPolyBlockMap () {
  PolyBlockMap = new polyblock_t*[BlockMapWidth*BlockMapHeight];
  memset((void *)PolyBlockMap, 0, sizeof(polyblock_t *)*BlockMapWidth*BlockMapHeight);
  for (int i = 0; i < NumPolyObjs; ++i) LinkPolyobj(&PolyObjs[i]);
}


//==========================================================================
//
//  VLevel::LinkPolyobj
//
//==========================================================================
void VLevel::LinkPolyobj (polyobj_t *po) {
  // calculate the polyobj bbox
  seg_t **tempSeg = po->segs;
  float rightX = (*tempSeg)->v1->x;
  float leftX = (*tempSeg)->v1->x;
  float topY = (*tempSeg)->v1->y;
  float bottomY = (*tempSeg)->v1->y;

  for (int i = 0; i < po->numsegs; ++i, ++tempSeg) {
    if ((*tempSeg)->v1->x > rightX) rightX = (*tempSeg)->v1->x;
    if ((*tempSeg)->v1->x < leftX) leftX = (*tempSeg)->v1->x;
    if ((*tempSeg)->v1->y > topY) topY = (*tempSeg)->v1->y;
    if ((*tempSeg)->v1->y < bottomY) bottomY = (*tempSeg)->v1->y;
  }
  po->bbox[BOXRIGHT] = MapBlock(rightX-BlockMapOrgX);
  po->bbox[BOXLEFT] = MapBlock(leftX-BlockMapOrgX);
  po->bbox[BOXTOP] = MapBlock(topY-BlockMapOrgY);
  po->bbox[BOXBOTTOM] = MapBlock(bottomY-BlockMapOrgY);
  // add the polyobj to each blockmap section
  for (int j = po->bbox[BOXBOTTOM]*BlockMapWidth; j <= po->bbox[BOXTOP]*BlockMapWidth; j += BlockMapWidth) {
    for (int i = po->bbox[BOXLEFT]; i <= po->bbox[BOXRIGHT]; ++i) {
      if (i >= 0 && i < BlockMapWidth && j >= 0 && j < BlockMapHeight*BlockMapWidth) {
        polyblock_t **link = &PolyBlockMap[j+i];
        if (!(*link)) {
          // create a new link at the current block cell
          *link = new polyblock_t;
          (*link)->next = nullptr;
          (*link)->prev = nullptr;
          (*link)->polyobj = po;
          continue;
        }

        polyblock_t *tempLink = *link;
        while (tempLink->next != nullptr && tempLink->polyobj != nullptr) {
          tempLink = tempLink->next;
        }
        if (tempLink->polyobj == nullptr) {
          tempLink->polyobj = po;
          continue;
        } else {
          tempLink->next = new polyblock_t;
          tempLink->next->next = nullptr;
          tempLink->next->prev = tempLink;
          tempLink->next->polyobj = po;
        }
      }
      // else, don't link the polyobj, since it's off the map
    }
  }
}


//==========================================================================
//
//  VLevel::UnLinkPolyobj
//
//==========================================================================
void VLevel::UnLinkPolyobj (polyobj_t *po) {
  // remove the polyobj from each blockmap section
  for (int j = po->bbox[BOXBOTTOM]; j <= po->bbox[BOXTOP]; ++j) {
    int index = j*BlockMapWidth;
    for (int i = po->bbox[BOXLEFT]; i <= po->bbox[BOXRIGHT]; ++i) {
      if (i >= 0 && i < BlockMapWidth && j >= 0 && j < BlockMapHeight) {
        polyblock_t *link = PolyBlockMap[index+i];
        while (link != nullptr && link->polyobj != po) {
          link = link->next;
        }
        if (link == nullptr) {
          // polyobj not located in the link cell
          continue;
        }
        link->polyobj = nullptr;
      }
    }
  }
}


//==========================================================================
//
//  VLevel::GetPolyobj
//
//==========================================================================
polyobj_t *VLevel::GetPolyobj (int polyNum) {
  //FIXME: make this faster!
  for (int i = 0; i < NumPolyObjs; ++i) {
    if (PolyObjs[i].tag == polyNum) return &PolyObjs[i];
  }
  return nullptr;
}


//==========================================================================
//
//  VLevel::GetPolyobjMirror
//
//==========================================================================
int VLevel::GetPolyobjMirror (int poly) {
  //FIXME: make this faster!
  for (int i = 0; i < NumPolyObjs; ++i) {
    if (PolyObjs[i].tag == poly) return ((*PolyObjs[i].segs)->linedef->arg2);
  }
  return 0;
}


//==========================================================================
//
//  VLevel::MovePolyobj
//
//==========================================================================
bool VLevel::MovePolyobj (int num, float x, float y, bool forced) {
  int count;
  seg_t **segList;
  seg_t **veryTempSeg;
  polyobj_t *po;
  vertex_t *prevPts;
  bool blocked;

  po = GetPolyobj(num);
  if (!po) Sys_Error("Invalid polyobj number: %d", num);

  if (IsForServer()) UnLinkPolyobj(po);

  segList = po->segs;
  prevPts = po->prevPts;
  blocked = false;

  for (count = po->numsegs; count; --count, ++segList, ++prevPts) {
    for (veryTempSeg = po->segs; veryTempSeg != segList; ++veryTempSeg) {
      if ((*veryTempSeg)->v1 == (*segList)->v1) break;
    }
    if (veryTempSeg == segList) {
      (*segList)->v1->x += x;
      (*segList)->v1->y += y;
    }
    if (IsForServer()) {
      // previous points are unique for each seg
      (*prevPts).x += x;
      (*prevPts).y += y;
    }
  }
  UpdatePolySegs(po);
  if (!forced && IsForServer()) {
    segList = po->segs;
    for (count = po->numsegs; count; --count, ++segList) {
      if (PolyCheckMobjBlocking(*segList, po)) blocked = true; //k8: break here?
    }
  }
  if (blocked) {
    count = po->numsegs;
    segList = po->segs;
    prevPts = po->prevPts;
    while (count--) {
      for (veryTempSeg = po->segs; veryTempSeg != segList; ++veryTempSeg) {
        if ((*veryTempSeg)->v1 == (*segList)->v1) break;
      }
      if (veryTempSeg == segList) {
        (*segList)->v1->x -= x;
        (*segList)->v1->y -= y;
      }
      (*prevPts).x -= x;
      (*prevPts).y -= y;
      ++segList;
      ++prevPts;
    }
    UpdatePolySegs(po);
    LinkPolyobj(po);
    return false;
  }

  po->startSpot.x += x;
  po->startSpot.y += y;
  if (IsForServer()) LinkPolyobj(po);

  return true;
}


//==========================================================================
//
//  VLevel::RotatePolyobj
//
//==========================================================================
bool VLevel::RotatePolyobj (int num, float angle) {
  // get the polyobject
  polyobj_t *po = GetPolyobj(num);
  if (!po) Sys_Error("Invalid polyobj number: %d", num);

  // calculate the angle
  float an = po->angle+angle;
  float msinAn = msin(an);
  float mcosAn = mcos(an);

  if (IsForServer()) UnLinkPolyobj(po);

  seg_t **segList = po->segs;
  vertex_t *originalPts = po->originalPts;
  vertex_t *prevPts = po->prevPts;

  for (int count = po->numsegs; count; --count, ++segList, ++originalPts, ++prevPts) {
    if (IsForServer()) {
      // save the previous points
      prevPts->x = (*segList)->v1->x;
      prevPts->y = (*segList)->v1->y;
    }

    // get the original X and Y values
    float tr_x = originalPts->x;
    float tr_y = originalPts->y;

    // calculate the new X and Y values
    (*segList)->v1->x = (tr_x*mcosAn-tr_y*msinAn)+po->startSpot.x;
    (*segList)->v1->y = (tr_y*mcosAn+tr_x*msinAn)+po->startSpot.y;
  }
  UpdatePolySegs(po);

  bool blocked = false;
  if (IsForServer()) {
    segList = po->segs;
    for (int count = po->numsegs; count; --count, ++segList) {
      if (PolyCheckMobjBlocking(*segList, po)) blocked = true; //k8: break here?
    }
  }

  // if we are blocked then restore the previous points
  if (blocked) {
    segList = po->segs;
    prevPts = po->prevPts;
    for (int count = po->numsegs; count; --count, ++segList, ++prevPts) {
      (*segList)->v1->x = prevPts->x;
      (*segList)->v1->y = prevPts->y;
    }
    UpdatePolySegs(po);
    LinkPolyobj(po);
    return false;
  }

  po->angle = AngleMod(po->angle+angle);
  if (IsForServer()) LinkPolyobj(po);
  return true;
}


//==========================================================================
//
//  VLevel::PolyCheckMobjBlocking
//
//==========================================================================
bool VLevel::PolyCheckMobjBlocking (seg_t *seg, polyobj_t *po) {
  VEntity *mobj;
  int i, j;
  int left, right, top, bottom;
  float tmbbox[4];
  line_t *ld;
  bool blocked;

  ld = seg->linedef;

  top = MapBlock(ld->bbox[BOXTOP]-BlockMapOrgY+MAXRADIUS);
  bottom = MapBlock(ld->bbox[BOXBOTTOM]-BlockMapOrgY-MAXRADIUS);
  left = MapBlock(ld->bbox[BOXLEFT]-BlockMapOrgX-MAXRADIUS);
  right = MapBlock(ld->bbox[BOXRIGHT]-BlockMapOrgX+MAXRADIUS);

  blocked = false;

  bottom = (bottom < 0 ? 0 : bottom);
  bottom = (bottom >= BlockMapHeight ? BlockMapHeight-1 : bottom);
  top = (top < 0 ? 0 : top);
  top = (top >= BlockMapHeight ? BlockMapHeight-1 : top);
  left = (left < 0 ? 0 : left);
  left = (left >= BlockMapWidth ? BlockMapWidth-1 : left);
  right = (right < 0 ? 0 : right);
  right = (right >= BlockMapWidth ? BlockMapWidth-1 : right);

  for (j = bottom*BlockMapWidth; j <= top*BlockMapWidth; j += BlockMapWidth) {
    for (i = left; i <= right; ++i) {
      for (mobj = BlockLinks[j+i]; mobj; mobj = mobj->BlockMapNext) {
        if (mobj->EntityFlags&VEntity::EF_ColideWithWorld) {
          if (mobj->EntityFlags&(VEntity::EF_Solid|VEntity::EF_Corpse)) {
            bool isSolid = !!(mobj->EntityFlags&VEntity::EF_Solid);

            tmbbox[BOXTOP] = mobj->Origin.y+mobj->Radius;
            tmbbox[BOXBOTTOM] = mobj->Origin.y-mobj->Radius;
            tmbbox[BOXLEFT] = mobj->Origin.x-mobj->Radius;
            tmbbox[BOXRIGHT] = mobj->Origin.x+mobj->Radius;

            if (tmbbox[BOXRIGHT] <= ld->bbox[BOXLEFT] ||
                tmbbox[BOXLEFT] >= ld->bbox[BOXRIGHT] ||
                tmbbox[BOXTOP] <= ld->bbox[BOXBOTTOM] ||
                tmbbox[BOXBOTTOM] >= ld->bbox[BOXTOP])
            {
              continue;
            }
            if (P_BoxOnLineSide(tmbbox, ld) != -1) continue;

            if (isSolid) {
              mobj->Level->eventPolyThrustMobj(mobj, seg->normal, po);
              blocked = true;
            } else {
              mobj->Level->eventPolyCrushMobj(mobj, po);
            }
          }
        }
      }
    }
  }
  return blocked;
}


//==========================================================================
//
//  Script polyobject methods
//
//==========================================================================
IMPLEMENT_FUNCTION(VLevel, SpawnPolyobj) {
  P_GET_BOOL(hurt);
  P_GET_BOOL(crush);
  P_GET_INT(tag);
  P_GET_FLOAT(y);
  P_GET_FLOAT(x);
  P_GET_SELF;
  Self->SpawnPolyobj(x, y, tag, crush, hurt);
}

IMPLEMENT_FUNCTION(VLevel, AddPolyAnchorPoint) {
  P_GET_INT(tag);
  P_GET_FLOAT(y);
  P_GET_FLOAT(x);
  P_GET_SELF;
  Self->AddPolyAnchorPoint(x, y, tag);
}

IMPLEMENT_FUNCTION(VLevel, GetPolyobj) {
  P_GET_INT(polyNum);
  P_GET_SELF;
  RET_PTR(Self->GetPolyobj(polyNum));
}

IMPLEMENT_FUNCTION(VLevel, GetPolyobjMirror) {
  P_GET_INT(polyNum);
  P_GET_SELF;
  RET_INT(Self->GetPolyobjMirror(polyNum));
}

IMPLEMENT_FUNCTION(VLevel, MovePolyobj) {
  P_GET_BOOL_OPT(forced, false);
  P_GET_FLOAT(y);
  P_GET_FLOAT(x);
  P_GET_INT(num);
  P_GET_SELF;
  RET_BOOL(Self->MovePolyobj(num, x, y, forced));
}

IMPLEMENT_FUNCTION(VLevel, RotatePolyobj) {
  P_GET_FLOAT(angle);
  P_GET_INT(num);
  P_GET_SELF;
  RET_BOOL(Self->RotatePolyobj(num, angle));
}
