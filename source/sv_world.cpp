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
//**
//**  Movement/collision utility functions, as used by function in
//**  p_map.c. BLOCKMAP Iterator functions, and some PIT_* functions to use
//**  for iteration.
//**
//**************************************************************************
#include "gamedefs.h"
#include "sv_local.h"


//k8: this should be enough for everyone, lol
#define MAX_OPENINGS  (16384)
static opening_t openings[MAX_OPENINGS];

//#define VV_DUMP_OPENING_CREATION


//==========================================================================
//
//  DumpRegion
//
//==========================================================================
static VVA_OKUNUSED void DumpRegion (const sec_region_t *inregion) {
  GCon->Logf(NAME_Debug, "  %p: floor=(%g,%g,%g:%g); (%g : %g), flags=0x%04x; ceil=(%g,%g,%g:%g); (%g : %g), flags=0x%04x; eline=%d; rflags=0x%02x",
    inregion,
    inregion->efloor.GetNormal().x,
    inregion->efloor.GetNormal().y,
    inregion->efloor.GetNormal().z,
    inregion->efloor.GetDist(),
    inregion->efloor.splane->minz, inregion->efloor.splane->maxz,
    inregion->efloor.splane->flags,
    inregion->eceiling.GetNormal().x,
    inregion->eceiling.GetNormal().y,
    inregion->eceiling.GetNormal().z,
    inregion->eceiling.GetDist(),
    inregion->eceiling.splane->minz, inregion->eceiling.splane->maxz,
    inregion->eceiling.splane->flags,
    (inregion->extraline ? 1 : 0),
    inregion->regflags);
}


//==========================================================================
//
//  DumpOpening
//
//==========================================================================
static VVA_OKUNUSED void DumpOpening (const opening_t *op) {
  GCon->Logf(NAME_Debug, "  %p: floor=%g (%g,%g,%g:%g); ceil=%g (%g,%g,%g:%g); lowfloor=%g; range=%g",
    op,
    op->bottom, op->efloor.GetNormal().x, op->efloor.GetNormal().y, op->efloor.GetNormal().z, op->efloor.GetDist(),
    op->top, op->eceiling.GetNormal().x, op->eceiling.GetNormal().y, op->eceiling.GetNormal().z, op->eceiling.GetDist(),
    op->lowfloor, op->range);
}


//==========================================================================
//
//  DumpOpPlanes
//
//==========================================================================
static VVA_OKUNUSED void DumpOpPlanes (TArray<opening_t> &list) {
  GCon->Logf(NAME_Debug, " ::: count=%d :::", list.length());
  for (int f = 0; f < list.length(); ++f) DumpOpening(&list[f]);
}


//==========================================================================
//
//  InsertOpening
//
//  insert new opening, maintaing order and non-intersection invariants
//  it is faster to insert openings from bottom to top,
//  but it is not a requerement
//
//  note that this does joining logic for "solid" openings
//
//  in op: efloor, bottom, eceiling, top -- should be set and valid
//
//==========================================================================
static void InsertOpening (TArray<opening_t> &dest, const opening_t &op) {
  if (op.top < op.bottom) return; // shrinked to invalid size
  int dlen = dest.length();
  if (dlen == 0) {
    dest.append(op);
    return;
  }
  opening_t *ops = dest.ptr(); // to avoid range checks
  // find region that contains our floor
  const float opfz = op.bottom;
  // check if current is higher than the last
  if (opfz > ops[dlen-1].top) {
    // append and exit
    dest.append(op);
    return;
  }
  // starts where last ends
  if (opfz == ops[dlen-1].top) {
    // grow last and exit
    ops[dlen-1].top = op.top;
    ops[dlen-1].eceiling = op.eceiling;
    return;
  }
  /*
    7 -- max array index
    5
    3 -- here for 2 and 3
    1 -- min array index (0)
  */
  // find a place and insert
  // then join regions
  int cpos = dlen;
  while (cpos > 0 && opfz <= ops[cpos-1].bottom) --cpos;
  // now, we can safely insert it into cpos
  // if op floor is the same as cpos ceiling, join
  if (opfz == ops[cpos].bottom) {
    // join (but check if new region is bigger first)
    if (op.top <= ops[cpos].top) return; // completely inside
    ops[cpos].eceiling = op.eceiling;
    ops[cpos].top = op.top;
  } else {
    // check if new bottom is inside a previous region
    if (cpos > 0 && opfz <= ops[cpos-1].top) {
      // yes, join with previous region
      --cpos;
      ops[cpos].eceiling = op.eceiling;
      ops[cpos].top = op.top;
    } else {
      // no, insert, and let the following loop take care of joins
      dest.insert(cpos, op);
      ops = dest.ptr();
      ++dlen;
    }
  }
  // now check if `cpos` region intersects with upper regions, and perform joins
  int npos = cpos+1;
  while (npos < dlen) {
    // below?
    if (ops[cpos].top < ops[npos].bottom) break; // done
    // npos is completely inside?
    if (ops[cpos].top >= ops[npos].top) {
      // completely inside: eat it and continue
      dest.removeAt(npos);
      ops = dest.ptr();
      --dlen;
      continue;
    }
    // join cpos (floor) and npos (ceiling)
    ops[cpos].eceiling = ops[npos].eceiling;
    ops[cpos].top = ops[npos].top;
    dest.removeAt(npos);
    // done
    break;
  }
}


//==========================================================================
//
//  GetBaseSectorOpening
//
//==========================================================================
static void GetBaseSectorOpening (opening_t &op, sector_t *sector, const TVec point, bool usePoint) {
  op.efloor = sector->eregions->efloor;
  op.eceiling = sector->eregions->eceiling;
  if (usePoint) {
    // we cannot go out of sector heights
    op.bottom = op.efloor.GetPointZClamped(point);
    op.top = op.eceiling.GetPointZClamped(point);
  } else {
    op.bottom = op.efloor.splane->minz;
    op.top = op.eceiling.splane->maxz;
  }
  op.range = op.top-op.bottom;
  op.lowfloor = op.bottom;
  op.highceiling = op.top;
  op.elowfloor = op.efloor;
  op.ehighceiling = op.eceiling;
  op.next = nullptr;
}


//==========================================================================
//
//  Insert3DMidtex
//
//==========================================================================
static void Insert3DMidtex (TArray<opening_t> &dest, const sector_t *sector, const line_t *linedef) {
  if (!(linedef->flags&ML_3DMIDTEX)) return;
  // for 3dmidtex, create solid from midtex bottom to midtex top
  //   from floor to midtex bottom
  //   from midtex top to ceiling
  float cz, fz;
  if (!P_GetMidTexturePosition(linedef, 0, &cz, &fz)) return;
  if (fz > cz) return; // k8: is this right?
  opening_t op;
  op.efloor = sector->eregions->efloor;
  op.bottom = fz;
  op.eceiling = sector->eregions->eceiling;
  op.top = cz;
  // flip floor and ceiling if they are pointing outside a region
  // i.e. they should point *inside*
  // we will use this fact to create correct "empty" openings
  if (op.efloor.GetNormalZ() < 0.0f) op.efloor.Flip();
  if (op.eceiling.GetNormalZ() > 0.0f) op.eceiling.Flip();
  // inserter will join regions
  InsertOpening(dest, op);
}


//==========================================================================
//
//  BuildSectorOpenings
//
//  this function doesn't like regions with floors that has different flags
//
//==========================================================================
static void BuildSectorOpenings (const line_t *xldef, TArray<opening_t> &dest, sector_t *sector, const TVec point,
                                 unsigned NoBlockFlags, bool linkList, bool usePoint, bool skipNonSolid=false, bool forSurface=false)
{
  dest.reset();
  // if this sector has no 3d floors, we don't need to do any extra work
  if (!sector->Has3DFloors() /*|| !(sector->SectorFlags&sector_t::SF_Has3DMidTex)*/ && (!xldef || !(xldef->flags&ML_3DMIDTEX))) {
    opening_t &op = dest.alloc();
    GetBaseSectorOpening(op, sector, point, usePoint);
    return;
  }
  //if (thisIs3DMidTex) Insert3DMidtex(op1list, linedef->backsector, linedef);

  /* build list of closed regions (it doesn't matter if region is non-solid, as long as it satisfies flag mask).
     that list has all intersecting regions joined.
     then cut those solids from main empty region.
   */
  static TArray<opening_t> solids;
  solids.reset();
  opening_t cop;
  cop.lowfloor = 0.0f; // for now
  cop.highceiling = 0.0f; // for now
  //bool hadNonSolid;
  // have to do this, because this function can be called both in server and in client
  VLevel *level = (GClLevel ? GClLevel : GLevel);
  // skip base region for now
  for (const sec_region_t *reg = sector->eregions->next; reg; reg = reg->next) {
    if (reg->regflags&sec_region_t::RF_OnlyVisual) continue; // pure visual region, ignore it
    if (skipNonSolid && (reg->regflags&sec_region_t::RF_NonSolid)) continue;
    /*
    if (reg->regflags&sec_region_t::RF_NonSolid) {
      if (skipNonSolid) continue;
      hadNonSolid = true;
    }
    */
    if (((reg->efloor.splane->flags|reg->eceiling.splane->flags)&NoBlockFlags) != 0) continue; // bad flags
    // hack: 3d floor with sky texture seems to be transparent in renderer
    if (forSurface && reg->extraline) {
      const side_t *sidedef = &level->Sides[reg->extraline->sidenum[0]];
      if (sidedef->MidTexture == skyflatnum) continue;
    }
    // border points
    float fz = reg->efloor.splane->minz;
    float cz = reg->eceiling.splane->maxz;
    if (usePoint) {
      fz = reg->efloor.GetPointZClamped(point);
      cz = reg->eceiling.GetPointZClamped(point);
    }
    // k8: just in case
    //if (fz > cz && (reg->efloor.isSlope() || reg->eceiling.isSlope())) { float tmp = fz; fz = cz; cz = tmp; }
    if (fz > cz) fz = cz;
    cop.efloor = reg->efloor;
    cop.bottom = fz;
    cop.eceiling = reg->eceiling;
    cop.top = cz;
    // flip floor and ceiling if they are pointing outside a region
    // i.e. they should point *inside*
    // we will use this fact to create correct "empty" openings
    if (cop.efloor.GetNormalZ() < 0.0f) cop.efloor.Flip();
    if (cop.eceiling.GetNormalZ() > 0.0f) cop.eceiling.Flip();
    // inserter will join regions
    InsertOpening(solids, cop);
  }
  // add 3dmidtex, if there are any
  if (!forSurface) {
    if (xldef && (xldef->flags&ML_3DMIDTEX)) {
      Insert3DMidtex(solids, sector, xldef);
    } else if (usePoint && sector->SectorFlags&sector_t::SF_Has3DMidTex) {
      //FIXME: we need to know a radius here
      /*
      GCon->Logf("!!!!!");
      line_t *const *lparr = sector->lines;
      for (int lct = sector->linecount; lct--; ++lparr) {
        const line_t *ldf = *lparr;
        Insert3DMidtex(solids, sector, ldf);
      }
      */
    }
    //if (thisIs3DMidTex) Insert3DMidtex(op1list, linedef->backsector, linedef);
  }

  // if we have no openings, or openings are out of bounds, just use base sector region
  float secfz, seccz;
  if (usePoint) {
    secfz = sector->floor.GetPointZClamped(point);
    seccz = sector->ceiling.GetPointZClamped(point);
  } else {
    secfz = sector->floor.minz;
    seccz = sector->ceiling.maxz;
  }

  if (solids.length() == 0 || solids[solids.length()-1].top <= secfz || solids[0].bottom >= seccz) {
    opening_t &op = dest.alloc();
    GetBaseSectorOpening(op, sector, point, usePoint);
    return;
  }
  /* now we have to cut out all solid regions from base one
     this can be done in a simple loop, because all solids are non-intersecting
     paper-thin regions should be cutted too, as those can be real floors/ceilings

     the algorithm is simple:
       get sector floor as current floor.
       for each solid:
         if current floor if lower than solid floor:
           insert opening from current floor to solid floor
         set current floor to solid ceiling
     take care that "emptyness" floor and ceiling are pointing inside
   */
  TSecPlaneRef currfloor;
  currfloor.set(&sector->floor, false);
  float currfloorz = secfz;
  vassert(currfloor.isFloor());
  // HACK: if the whole sector is taken by some region, return sector opening
  //       this is required to proper 3d-floor backside creation
  //       alas, `hadNonSolid` hack is required to get rid of "nano-water walls"
  opening_t *cs = solids.ptr();
  if (!usePoint /*&& !hadNonSolid*/ && solids.length() == 1) {
    if (cs[0].bottom <= sector->floor.minz && cs[0].top >= sector->ceiling.maxz) {
      opening_t &op = dest.alloc();
      GetBaseSectorOpening(op, sector, point, usePoint);
      return;
    }
  }
  // main loop
  for (int scount = solids.length(); scount--; ++cs) {
    if (cs->bottom >= seccz) break; // out of base sector bounds, we are done here
    if (cs->top < currfloorz) continue; // below base sector bounds, nothing to do yet
    if (currfloorz <= cs->bottom) {
      // insert opening from current floor to solid floor
      cop.efloor = currfloor;
      cop.bottom = currfloorz;
      cop.eceiling = cs->efloor;
      cop.top = cs->bottom;
      cop.range = cop.top-cop.bottom;
      // flip ceiling if necessary, so it will point inside
      if (!cop.eceiling.isCeiling()) cop.eceiling.Flip();
      dest.append(cop);
    }
    // set current floor to solid ceiling (and flip, if necessary)
    currfloor = cs->eceiling;
    currfloorz = cs->top;
    if (!currfloor.isFloor()) currfloor.Flip();
  }
  // add top cap (to sector base ceiling) if necessary
  if (currfloorz <= seccz) {
    cop.efloor = currfloor;
    cop.bottom = currfloorz;
    cop.eceiling.set(&sector->ceiling, false);
    cop.top = seccz;
    cop.range = cop.top-cop.bottom;
    dest.append(cop);
  }
  // if we aren't using point, join openings with paper-thin edges
  if (!usePoint) {
    cs = dest.ptr();
    int dlen = dest.length();
    int dpos = 0;
    while (dpos < dlen-1) {
      if (cs[dpos].top == cs[dpos+1].bottom) {
        // join
        cs[dpos].top = cs[dpos+1].top;
        cs[dpos].eceiling = cs[dpos+1].eceiling;
        cs[dpos].range = cs[dpos].top-cs[dpos].bottom;
        dest.removeAt(dpos+1);
        cs = dest.ptr();
        --dlen;
      } else {
        // skip
        ++dpos;
      }
    }
  }
  // link list, if necessary
  if (linkList) {
    cs = dest.ptr();
    for (int f = dest.length()-1; f > 0; --f) cs[f-1].next = &cs[f];
    cs[dest.length()-1].next = nullptr;
  }
}


//==========================================================================
//
//  SV_SectorOpenings
//
//  used in surface creator
//
//==========================================================================
opening_t *SV_SectorOpenings (sector_t *sector, bool skipNonSolid) {
  vassert(sector);
  static TArray<opening_t> oplist;
  BuildSectorOpenings(nullptr, oplist, sector, TVec::ZeroVector, 0/*nbflags*/, true/*linkList*/, false/*usePoint*/, skipNonSolid, true/*forSurface*/);
  //vassert(oplist.length() > 0);
  if (oplist.length() == 0) {
    //k8: why, it is ok to have zero openings (it seems)
    //    i've seen this in Hurt.wad, around (7856 -2146)
    //    just take the armor, and wait until the pool will start filling with blood
    //    this seems to be a bug, but meh... at least there is no reason to crash.
    #ifdef CLIENT
    //GCon->Logf(NAME_Warning, "SV_SectorOpenings: zero openings for sector #%d", (int)(ptrdiff_t)(sector-&GClLevel->Sectors[0]));
    #endif
    return nullptr;
  }
  return oplist.ptr();
}


//==========================================================================
//
//  SV_SectorOpenings2
//
//  used in surface creator
//
//==========================================================================
opening_t *SV_SectorOpenings2 (sector_t *sector, bool skipNonSolid) {
  /*
  vassert(sector);
  static TArray<opening_t> oplist;
  BuildSectorOpenings(nullptr, oplist, sector, TVec::ZeroVector, 0, false/ *linkList* /, false/ *usePoint* /, skipNonSolid);
  vassert(oplist.length() > 0);
  if (oplist.length() > MAX_OPENINGS) Host_Error("too many sector openings");
  opening_t *dest = openings;
  opening_t *src = oplist.ptr();
  for (int count = oplist.length(); count--; ++dest, ++src) {
    *dest = *src;
    dest->next = dest+1;
  }
  openings[oplist.length()-1].next = nullptr;
  return openings;
  */
  vassert(sector);
  static TArray<opening_t> oplist;
  BuildSectorOpenings(nullptr, oplist, sector, TVec::ZeroVector, 0, true/*linkList*/, false/*usePoint*/, skipNonSolid, true/*forSurface*/);
  vassert(oplist.length() > 0);
  return oplist.ptr();
}


//==========================================================================
//
//  SV_LineOpenings
//
//  sets opentop and openbottom to the window through a two sided line
//
//==========================================================================
opening_t *SV_LineOpenings (const line_t *linedef, const TVec point, unsigned NoBlockFlags, bool do3dmidtex, bool usePoint) {
  if (linedef->sidenum[1] == -1 || !linedef->backsector) return nullptr; // single sided line

  NoBlockFlags &= (SPF_MAX_FLAG-1u);

  // fast algo for two sectors without 3d floors
  if (!linedef->frontsector->Has3DFloors() &&
      !linedef->backsector->Has3DFloors() &&
      //!thisIs3DMidTex &&
      !((linedef->frontsector->SectorFlags|linedef->backsector->SectorFlags)&sector_t::SF_Has3DMidTex))
  {
    opening_t fop, bop;
    GetBaseSectorOpening(fop, linedef->frontsector, point, usePoint);
    GetBaseSectorOpening(bop, linedef->backsector, point, usePoint);
    // no intersection?
    if (fop.top <= bop.bottom || bop.top <= fop.bottom ||
        fop.bottom >= bop.top || bop.bottom >= fop.top)
    {
      return nullptr;
    }
    // create opening
    opening_t *dop = &openings[0];
    // setup floor
    if (fop.bottom >= bop.bottom) {
      dop->efloor = fop.efloor;
      dop->bottom = fop.bottom;
      dop->lowfloor = bop.bottom;
      dop->elowfloor = bop.efloor;
    } else {
      dop->efloor = bop.efloor;
      dop->bottom = bop.bottom;
      dop->lowfloor = fop.bottom;
      dop->elowfloor = fop.efloor;
    }
    // setup ceiling
    if (fop.top <= bop.top) {
      dop->eceiling = fop.eceiling;
      dop->top = fop.top;
      dop->highceiling = bop.top;
      dop->ehighceiling = bop.eceiling;
    } else {
      dop->eceiling = bop.eceiling;
      dop->top = bop.top;
      dop->highceiling = fop.top;
      dop->ehighceiling = fop.eceiling;
    }
    dop->range = dop->top-dop->bottom;
    dop->next = nullptr;

    return dop;
  }

  // has 3d floors at least on one side, do full-featured intersection calculation
  static TArray<opening_t> op0list;
  static TArray<opening_t> op1list;

  BuildSectorOpenings(linedef, op0list, linedef->frontsector, point, NoBlockFlags, false/*linkList*/, usePoint);
  if (op0list.length() == 0) {
    // just in case: no front sector openings
    return nullptr;
  }

  BuildSectorOpenings(linedef, op1list, linedef->backsector, point, NoBlockFlags, false/*linkList*/, usePoint);
  if (op1list.length() == 0) {
    // just in case: no back sector openings
    return nullptr;
  }

#ifdef VV_DUMP_OPENING_CREATION
  GCon->Logf(NAME_Debug, "*** line: %p (0x%02x) ***", linedef, NoBlockFlags);
  GCon->Log(NAME_Debug, "::: before :::"); DumpOpPlanes(op0list); DumpOpPlanes(op1list);
#endif

  /* build intersections
     both lists are valid (sorted, without intersections -- but with possible paper-thin regions)
   */
  opening_t *dest = openings;
  unsigned destcount = 0;

  const opening_t *op0 = op0list.ptr();
  int op0left = op0list.length();

  const opening_t *op1 = op1list.ptr();
  int op1left = op1list.length();

  while (op0left && op1left) {
    // if op0 is below op1, skip op0
    if (op0->top < op1->bottom) {
#ifdef VV_DUMP_OPENING_CREATION
      GCon->Log(NAME_Debug, " +++ SKIP op0 (dump: op0, op1) +++");
      DumpOpening(op0);
      DumpOpening(op1);
#endif
      ++op0; --op0left;
      continue;
    }
    // if op1 is below op0, skip op1
    if (op1->top < op0->bottom) {
#ifdef VV_DUMP_OPENING_CREATION
      GCon->Log(NAME_Debug, " +++ SKIP op1 (dump: op0, op1) +++");
      DumpOpening(op0);
      DumpOpening(op1);
#endif
      ++op1; --op1left;
      continue;
    }
    // here op0 and op1 are intersecting
    vassert(op0->bottom <= op1->top);
    vassert(op1->bottom <= op0->top);
    if (destcount >= MAX_OPENINGS) Host_Error("too many line openings");
    // floor
    if (op0->bottom >= op1->bottom) {
      dest->efloor = op0->efloor;
      dest->bottom = op0->bottom;
      dest->lowfloor = op1->bottom;
      dest->elowfloor = op1->efloor;
    } else {
      dest->efloor = op1->efloor;
      dest->bottom = op1->bottom;
      dest->lowfloor = op0->bottom;
      dest->elowfloor = op0->efloor;
    }
    // ceiling
    if (op0->top <= op1->top) {
      dest->eceiling = op0->eceiling;
      dest->top = op0->top;
      dest->highceiling = op1->top;
      dest->ehighceiling = op1->eceiling;
    } else {
      dest->eceiling = op1->eceiling;
      dest->top = op1->top;
      dest->highceiling = op0->top;
      dest->ehighceiling = op0->eceiling;
    }
    dest->range = dest->top-dest->bottom;
#ifdef VV_DUMP_OPENING_CREATION
    GCon->Log(NAME_Debug, " +++ NEW opening (dump: op0, op1, new) +++");
    DumpOpening(op0);
    DumpOpening(op1);
    DumpOpening(dest);
#endif
    ++dest;
    ++destcount;
    // if both regions ends at the same height, skip both,
    // otherwise skip region with lesser top
    if (op0->top == op1->top) {
#ifdef VV_DUMP_OPENING_CREATION
      GCon->Log(NAME_Debug, " +++ SKIP BOTH +++");
#endif
      ++op0; --op0left;
      ++op1; --op1left;
    } else if (op0->top < op1->top) {
#ifdef VV_DUMP_OPENING_CREATION
      GCon->Log(NAME_Debug, " +++ SKIP OP0 +++");
#endif
      ++op0; --op0left;
    } else {
#ifdef VV_DUMP_OPENING_CREATION
      GCon->Log(NAME_Debug, " +++ SKIP OP1 +++");
#endif
      ++op1; --op1left;
    }
  }

#ifdef VV_DUMP_OPENING_CREATION
  GCon->Logf(NAME_Debug, "::: after (%u) :::", destcount);
  for (unsigned f = 0; f < destcount; ++f) {
    const opening_t *xop = &openings[f];
    DumpOpening(xop);
  }
  GCon->Log(NAME_Debug, "-----------------------------");
#endif

  // no intersections?
  if (destcount == 0) {
    // oops
    return nullptr;
  }

  if (destcount > 1) {
    for (unsigned f = 0; f < destcount-1; ++f) {
      openings[f].next = &openings[f+1];
    }
  }
  openings[destcount-1].next = nullptr;
  return openings;
}


//==========================================================================
//
//  SV_FindOpening
//
//  Find the best gap that the thing could fit in, given a certain Z
//  position (z1 is foot, z2 is head).  Assuming at least two gaps exist,
//  the best gap is chosen as follows:
//
//  1. if the thing fits in one of the gaps without moving vertically,
//     then choose that gap.
//
//  2. if there is only *one* gap which the thing could fit in, then
//     choose that gap.
//
//  3. if there is multiple gaps which the thing could fit in, choose
//     the gap whose floor is closest to the thing's current Z.
//
//  4. if there is no gaps which the thing could fit in, do the same.
//
//  Returns the gap number, or -1 if there are no gaps at all.
//
//==========================================================================
opening_t *SV_FindOpening (opening_t *InGaps, float z1, float z2) {
  // check for trivial gaps
  if (!InGaps) return nullptr;
  if (!InGaps->next) return InGaps;

  int fit_num = 0;
  opening_t *fit_last = nullptr;

  opening_t *fit_closest = nullptr;
  float fit_mindist = 99999.0f;

  opening_t *nofit_closest = nullptr;
  float nofit_mindist = 99999.0f;

  // there are 2 or more gaps; now it gets interesting :-)
  for (opening_t *gap = InGaps; gap; gap = gap->next) {
    const float f = gap->bottom;
    const float c = gap->top;

    if (z1 >= f && z2 <= c) return gap; // [1]

    const float dist = fabsf(z1-f);

    if (z2-z1 <= c-f) {
      // [2]
      ++fit_num;
      fit_last = gap;
      if (dist < fit_mindist) {
        // [3]
        fit_mindist = dist;
        fit_closest = gap;
      }
    } else {
      if (dist < nofit_mindist) {
        // [4]
        nofit_mindist = dist;
        nofit_closest = gap;
      }
    }
  }

  if (fit_num == 1) return fit_last;
  if (fit_num > 1) return fit_closest;
  return nofit_closest;
}


//==========================================================================
//
//  SV_FindRelOpening
//
//  used in sector movement, so it tries hard to not leave current opening
//
//==========================================================================
opening_t *SV_FindRelOpening (opening_t *InGaps, float z1, float z2) {
  // check for trivial gaps
  if (!InGaps) return nullptr;
  if (!InGaps->next) return InGaps;

  if (z2 < z1) z2 = z1;

  opening_t *fit_closest = nullptr;
  float fit_mindist = 99999.0f;

  opening_t *nofit_closest = nullptr;
  float nofit_mindist = 99999.0f;

  const float zmid = z1+(z2-z1)*0.5f;

  // there are 2 or more gaps; now it gets interesting :-)
  for (opening_t *gap = InGaps; gap; gap = gap->next) {
    const float f = gap->bottom;
    const float c = gap->top;

    if (z1 >= f && z2 <= c) return gap; // completely fits

    // can this gap contain us?
    if (z2-z1 <= c-f) {
      // this gap is big enough to contain us
      // if this gap's floor is higher than our feet, it is not interesting
      if (f > zmid) continue;
      // choose minimal distance to floor or ceiling
      const float dist = fabsf(z1-f);
      if (dist < fit_mindist) {
        fit_mindist = dist;
        fit_closest = gap;
      }
    } else {
      // not big enough
      const float dist = fabsf(z1-f);
      if (dist < nofit_mindist) {
        nofit_mindist = dist;
        nofit_closest = gap;
      }
    }
  }

  return (fit_closest ? fit_closest : nofit_closest ? nofit_closest : InGaps);
}


//==========================================================================
//
//  SV_FindGapFloorCeiling
//
//  find region for thing, and return best floor/ceiling
//  `p.z` is bottom
//
//  this is used mostly in sector movement, so we should try hard to stay
//  inside our current gap, so we won't teleport up/down from lifts, and
//  from crushers
//
//==========================================================================
void SV_FindGapFloorCeiling (sector_t *sector, const TVec point, float height, TSecPlaneRef &floor, TSecPlaneRef &ceiling, bool debugDump) {
  /*
  if (debugDump) {
    GCon->Logf("=== ALL OPENINGS: sector %p ===", sector);
    for (const sec_region_t *reg = sector->eregions; reg; reg = reg->next) DumpRegion(reg);
  }
  */

  if (!sector->Has3DFloors()) {
    // only one region, yay
    floor = sector->eregions->efloor;
    ceiling = sector->eregions->eceiling;
    return;
  }

  if (height < 0.0f) height = 0.0f;

  /* for multiple regions, we have some hard work to do.
     as region sorting is not guaranteed, we will force-sort regions.
     alas.
   */
  static TArray<opening_t> oplist;

  BuildSectorOpenings(nullptr, oplist, sector, point, SPF_NOBLOCKING, true/*linkList*/, true/*usePoint*/);
  if (oplist.length() == 0) {
    // something is very wrong here, so use sector boundaries
    floor = sector->eregions->efloor;
    ceiling = sector->eregions->eceiling;
    return;
  }

  if (debugDump) { GCon->Logf(NAME_Debug, "=== ALL OPENINGS (z=%g; height=%g) ===", point.z, height); DumpOpPlanes(oplist); }

#if 1
  opening_t *opres = SV_FindRelOpening(oplist.ptr(), point.z, point.z+height);
  if (opres) {
    if (debugDump) { GCon->Logf(NAME_Debug, " found result"); DumpOpening(opres); }
    floor = opres->efloor;
    ceiling = opres->eceiling;
  } else {
    if (debugDump) GCon->Logf(NAME_Debug, " NO result");
    floor = sector->eregions->efloor;
    ceiling = sector->eregions->eceiling;
  }
#else
  // now find best-fit region:
  //
  //  1. if the thing fits in one of the gaps without moving vertically,
  //     then choose that gap (one with less distance to the floor).
  //
  //  2. if there is only *one* gap which the thing could fit in, then
  //     choose that gap.
  //
  //  3. if there is multiple gaps which the thing could fit in, choose
  //     the gap whose floor is closest to the thing's current Z.
  //
  //  4. if there is no gaps which the thing could fit in, do the same.

  // one the thing can possibly fit
  const opening_t *bestGap = nullptr;
  float bestGapDist = 999999.0f;

  const opening_t *op = oplist.ptr();
  for (int opleft = oplist.length(); opleft--; ++op) {
    const float fz = op->bottom;
    const float cz = op->top;
    if (point.z >= fz && point.z <= cz) {
      // no need to move vertically
      if (debugDump) { GCon->Logf(NAME_Debug, " best fit"); DumpOpening(op); }
      return op;
    } else {
      const float fdist = fabsf(point.z-fz); // we don't care about sign here
      if (!bestGap || fdist < bestGapDist) {
        if (debugDump) { GCon->Logf(NAME_Debug, " gap fit"); DumpOpening(op); }
        bestGap = op;
        bestGapDist = fdist;
        //if (fdist == 0.0f) break; // there is no reason to look further
      } else {
        if (debugDump) { GCon->Logf(NAME_Debug, " REJECTED gap fit"); DumpOpening(op); }
      }
    }
  }

  if (bestFit) {
    if (debugDump) { GCon->Logf(NAME_Debug, " best result"); DumpOpening(bestFit); }
    floor = bestFit->efloor;
    ceiling = bestFit->eceiling;
  } else if (bestGap) {
    if (debugDump) { GCon->Logf(NAME_Debug, " gap result"); DumpOpening(bestGap); }
    floor = bestGap->efloor;
    ceiling = bestGap->eceiling;
  } else {
    // just fit into sector
    if (debugDump) { GCon->Logf(NAME_Debug, " no result"); }
    floor = sector->eregions->efloor;
    ceiling = sector->eregions->eceiling;
  }
#endif
}


//==========================================================================
//
//  SV_GetSectorGapCoords
//
//==========================================================================
void SV_GetSectorGapCoords (sector_t *sector, const TVec point, float &floorz, float &ceilz) {
  if (!sector) { floorz = 0.0f; ceilz = 0.0f; return; }
  if (!sector->Has3DFloors()) {
    floorz = sector->floor.GetPointZClamped(point);
    ceilz = sector->ceiling.GetPointZClamped(point);
    return;
  }
  TSecPlaneRef f, c;
  SV_FindGapFloorCeiling(sector, point, 0, f, c);
  floorz = f.GetPointZClamped(point);
  ceilz = c.GetPointZClamped(point);
}


//==========================================================================
//
//  SV_PointRegionLight
//
//  this is used to get lighting for the given point
//
//  glowFlags: bit 0: floor glow allowed; bit 1: ceiling glow allowed
//
//==========================================================================
sec_region_t *SV_PointRegionLight (sector_t *sector, const TVec &p, unsigned *glowFlags) {
  // early exit if we have no 3d floors
  if (!sector->Has3DFloors()) {
    if (glowFlags) *glowFlags = 3u; // both glows allowed
    return sector->eregions;
  }

  unsigned tmp = 0;
  if (glowFlags) *glowFlags = 0; else glowFlags = &tmp;

  /*
   (?) if the point is in a water (swimmable), use swimmable region
   otherwise, check distance to region floor (region is from ceiling down to floor), and use the smallest one
   also, ignore paper-thin regions

   additionally, calculate glow flags:
   floor glow is allowed if we are under the bottom-most solid region
   ceiling glow is allowed if we are above the top-most solid region
  */

  sec_region_t *best = nullptr;
  float bestDist = 99999.0f; // minimum distance to region floor
  *glowFlags = 3u; // will be reset when necessary
  for (sec_region_t *reg = sector->eregions->next; reg; reg = reg->next) {
    // ignore base (just in case) and visual-only regions
    if (reg->regflags&(sec_region_t::RF_OnlyVisual|sec_region_t::RF_BaseRegion)) continue;
    const float rtopz = reg->eceiling.GetPointZ(p);
    const float rbotz = reg->efloor.GetPointZ(p);
    // ignore paper-thin regions
    if (rtopz <= rbotz) continue; // invalid, or paper-thin, ignore
    float botDist;
    if (reg->regflags&sec_region_t::RF_NonSolid) {
      // swimmable, use region ceiling
      botDist = rtopz-p.z;
    } else {
      // solid, use region floor
      // calc glow flags
      if (p.z < rtopz) *glowFlags &= ~2u; // we have solid region above us, no ceiling glow
      if (p.z > rbotz) *glowFlags &= ~1u; // we have solid region below us, no floor glow
      // check best distance
      botDist = rbotz-p.z;
    }
    // if we're not in bounds, ignore
    // if further than the current best, ignore
    if (botDist <= 0.0f || botDist > bestDist) continue;
    // found new good region
    best = reg;
    bestDist = botDist;
  }

  if (!best) {
    // use base region
    best = sector->eregions;
  }

  return best;
}


//==========================================================================
//
//  SV_PointContents
//
//==========================================================================
int SV_PointContents (sector_t *sector, const TVec &p, bool dbgDump) {
  if (!sector) return 0;

  //dbgDump = true;
  if (sector->heightsec && (sector->heightsec->SectorFlags&sector_t::SF_UnderWater) &&
      p.z <= sector->heightsec->floor.GetPointZClamped(p))
  {
    if (dbgDump) GCon->Log(NAME_Debug, "SVP: case 0");
    return CONTENTS_BOOMWATER;
  }

  if (sector->SectorFlags&sector_t::SF_UnderWater) {
    if (dbgDump) GCon->Log(NAME_Debug, "SVP: case 1");
    return CONTENTS_BOOMWATER;
  }

  const sec_region_t *best = sector->eregions;

  if (sector->Has3DFloors()) {
    const float secfz = sector->floor.GetPointZClamped(p);
    const float seccz = sector->ceiling.GetPointZClamped(p);
    // out of sector's empty space?
    if (p.z < secfz || p.z > seccz) {
      if (dbgDump) GCon->Log(NAME_Debug, "SVP: case 2");
      return best->params->contents;
    }

    // ignore solid regions, as we cannot be inside them legally
    float bestDist = 999999.0f; // minimum distance to region floor
    for (const sec_region_t *reg = sector->eregions->next; reg; reg = reg->next) {
      if (reg->regflags&(sec_region_t::RF_OnlyVisual|sec_region_t::RF_BaseRegion)) continue;
      // "sane" regions may represent Vavoom water
      if ((reg->regflags&(sec_region_t::RF_SaneRegion|sec_region_t::RF_NonSolid)) == sec_region_t::RF_SaneRegion) {
        if (dbgDump) { GCon->Logf(NAME_Debug, "SVP: sane region..."); DumpRegion(reg); }
        if (!reg->params || (int)reg->params->contents <= 0) continue;
        if (dbgDump) { GCon->Logf(NAME_Debug, "SVP: sane non-solid region..."); DumpRegion(reg); }
        if (dbgDump) { GCon->Logf(NAME_Debug, "   botz=%g; topz=%g", reg->efloor.GetPointZ(p), reg->eceiling.GetPointZ(p)); }
        // it may be paper-thin
        const float topz = reg->eceiling.GetPointZ(p);
        const float botz = reg->efloor.GetPointZ(p);
        const float minz = min2(botz, topz);
        // everything beneath it is a water
        if (p.z <= minz) {
          best = reg;
          break;
        }
      } else if (!(reg->regflags&sec_region_t::RF_NonSolid)) {
        continue;
      }
      // non-solid
      if (dbgDump) { GCon->Logf(NAME_Debug, "SVP: checking region..."); DumpRegion(reg); }
      const float rtopz = reg->eceiling.GetPointZ(p);
      const float rbotz = reg->efloor.GetPointZ(p);
      // ignore paper-thin regions
      if (rtopz <= rbotz) continue; // invalid, or paper-thin, ignore
      // check if point is inside, and for best ceiling dist
      if (p.z >= rbotz && p.z < rtopz) {
        const float fdist = rtopz-p.z;
        if (dbgDump) GCon->Logf(NAME_Debug, "SVP: non-solid check: bestDist=%g; fdist=%g; p.z=%g; botz=%g; topz=%g", bestDist, fdist, p.z, rbotz, rtopz);
        if (fdist < bestDist) {
          if (dbgDump) GCon->Log(NAME_Debug, "SVP:   NON-SOLID HIT!");
          bestDist = fdist;
          best = reg;
          //wasHit = true;
        }
      } else {
        if (dbgDump) GCon->Logf(NAME_Debug, "SVP: non-solid SKIP: bestDist=%g; cdist=%g; p.z=%g; botz=%g; topz=%g", bestDist, rtopz-p.z, p.z, rbotz, rtopz);
      }
    }
  }

  if (dbgDump) { GCon->Logf(NAME_Debug, "SVP: best region"); DumpRegion(best); }
  return best->params->contents;
}


//==========================================================================
//
//  SV_GetNextRegion
//
//  the one that is higher
//  valid only if `srcreg` is solid and insane
//
//==========================================================================
sec_region_t *SV_GetNextRegion (sector_t *sector, sec_region_t *srcreg) {
  vassert(sector);
  if (!srcreg || !sector->eregions->next) return sector->eregions;
  // get distance to ceiling
  // we want the best sector that is higher
  const float srcrtopz = srcreg->eceiling.GetRealDist();
  float bestdist = 99999.0f;
  sec_region_t *bestreg = sector->eregions;
  for (sec_region_t *reg = sector->eregions->next; reg; reg = reg->next) {
    if (reg == srcreg || (reg->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_OnlyVisual|sec_region_t::RF_SaneRegion|sec_region_t::RF_BaseRegion)) != 0) continue;
    const float rtopz = reg->eceiling.GetRealDist();
    const float rbotz = reg->efloor.GetRealDist();
    // ignore paper-thin regions
    if (rtopz <= rbotz) continue; // invalid, or paper-thin, ignore
    const float dist = srcrtopz-rbotz;
    if (dist <= 0.0f) continue; // too low
    if (dist < bestdist) {
      bestdist = dist;
      bestreg = reg;
    }
  }
  return bestreg;
}


//==========================================================================
//
//  SV_GetLowestSolidPointZ
//
//  used for silent teleports
//
//  FIXME: this ignores 3d floors (this is prolly the right thing to do)
//
//==========================================================================
float SV_GetLowestSolidPointZ (sector_t *sector, const TVec &point, bool ignore3dFloors) {
  if (!sector) return 0.0f; // idc
  if (ignore3dFloors || !sector->Has3DFloors()) return sector->floor.GetPointZClamped(point); // cannot be lower than this
  // find best solid 3d ceiling
  vassert(sector->eregions);
  float bestz = sector->floor.GetPointZClamped(point);
  float bestdist = fabsf(bestz-point.z);
  for (sec_region_t *reg = sector->eregions; reg; reg = reg->next) {
    if ((reg->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_OnlyVisual|sec_region_t::RF_BaseRegion)) != 0) continue;
    // ceiling is at the top
    const float cz = reg->eceiling.GetPointZClamped(point);
    const float dist = fabsf(cz-point.z);
    if (dist < bestdist) {
      bestz = cz;
      bestdist = dist;
    }
  }
  return bestz;
}


//==========================================================================
//
//  SV_GetHighestSolidPointZ
//
//  used for silent teleports
//
//  FIXME: this ignores 3d floors (this is prolly the right thing to do)
//
//==========================================================================
float SV_GetHighestSolidPointZ (sector_t *sector, const TVec &point, bool ignore3dFloors) {
  if (!sector) return 0.0f; // idc
  if (ignore3dFloors || !sector->Has3DFloors()) return sector->ceiling.GetPointZClamped(point); // cannot be higher than this
  // find best solid 3d floor
  vassert(sector->eregions);
  float bestz = sector->ceiling.GetPointZClamped(point);
  float bestdist = fabsf(bestz-point.z);
  for (sec_region_t *reg = sector->eregions; reg; reg = reg->next) {
    if ((reg->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_OnlyVisual|sec_region_t::RF_BaseRegion)) != 0) continue;
    // ceiling is at the top
    const float fz = reg->efloor.GetPointZClamped(point);
    const float dist = fabsf(fz-point.z);
    if (dist < bestdist) {
      bestz = fz;
      bestdist = dist;
    }
  }
  return bestz;
}


//**************************************************************************
//
//  SECTOR HEIGHT CHANGING
//
//  After modifying a sectors floor or ceiling height, call this routine to
//  adjust the positions of all things that touch the sector.
//  If anything doesn't fit anymore, true will be returned. If crunch is
//  true, they will take damage as they are being crushed.
//  If Crunch is false, you should set the sector height back the way it was
//  and call P_ChangeSector again to undo the changes.
//
//**************************************************************************

//==========================================================================
//
//  VLevel::ChangeOneSectorInternal
//
//  resets all caches and such
//  used to "fake open" doors and move lifts in bot pathfinder
//
//  doesn't move things (so you *HAVE* to restore sector heights)
//
//==========================================================================
void VLevel::ChangeOneSectorInternal (sector_t *sector) {
  if (!sector) return;
  CalcSecMinMaxs(sector);
}


//==========================================================================
//
//  VLevel::nextVisitedCount
//
//==========================================================================
vint32 VLevel::nextVisitedCount () {
  if (tsVisitedCount == MAX_VINT32) {
    tsVisitedCount = 1;
    for (auto &&sector : allSectors()) {
      for (msecnode_t *n = sector.TouchingThingList; n; n = n->SNext) n->Visited = 0;
    }
  } else {
    ++tsVisitedCount;
  }
  return tsVisitedCount;
}

IMPLEMENT_FUNCTION(VLevel, GetNextVisitedCount) {
  vobjGetParamSelf();
  RET_INT(Self->nextVisitedCount());
}


//==========================================================================
//
//  VLevel::ChangeSectorInternal
//
//  jff 3/19/98 added to just check monsters on the periphery of a moving
//  sector instead of all in bounding box of the sector. Both more accurate
//  and faster.
//
//  `-1` for `crunch` means "ignore stuck mobj"
//
//==========================================================================
bool VLevel::ChangeSectorInternal (sector_t *sector, int crunch) {
  vassert(sector);
  int secnum = (int)(ptrdiff_t)(sector-Sectors);
  if ((csTouched[secnum]&0x7fffffffU) == csTouchCount) return !!(csTouched[secnum]&0x80000000U);
  csTouched[secnum] = csTouchCount;

  CalcSecMinMaxs(sector);

  bool ret = false;

  // killough 4/4/98: scan list front-to-back until empty or exhausted,
  // restarting from beginning after each thing is processed. Avoids
  // crashes, and is sure to examine all things in the sector, and only
  // the things which are in the sector, until a steady-state is reached.
  // Things can arbitrarily be inserted and removed and it won't mess up.
  //
  // killough 4/7/98: simplified to avoid using complicated counter
  //
  // ketmar: mostly rewritten, and reintroduced the counter

  // mark all things invalid
  //for (msecnode_t *n = sector->TouchingThingList; n; n = n->SNext) n->Visited = 0;

  if (sector->TouchingThingList) {
    const int visCount = nextVisitedCount();
    msecnode_t *n = nullptr;
    do {
      // go through list
      for (n = sector->TouchingThingList; n; n = n->SNext) {
        if (n->Visited != visCount) {
          // unprocessed thing found, mark thing as processed
          n->Visited = visCount;
          // process it
          //TVec oldOrg = n->Thing->Origin;
          if (!n->Thing->eventSectorChanged(crunch)) {
            // doesn't fit, keep checking (crush other things)
            // k8: no need to check flags, VC code does this for us
            /*if (!(n->Thing->EntityFlags&VEntity::EF_NoBlockmap))*/
            {
              //GCon->Logf("Thing '%s' hit (old: %g; new: %g)", *n->Thing->GetClass()->GetFullName(), oldOrg.z, n->Thing->Origin.z);
              if (ret) csTouched[secnum] |= 0x80000000U;
              ret = true;
            }
          }
          // exit and start over
          break;
        }
      }
    } while (n); // repeat from scratch until all things left are marked valid
  }

  // this checks the case when 3d control sector moved (3d lifts)
  for (auto it = IterControlLinks(sector); !it.isEmpty(); it.next()) {
    //GCon->Logf("*** src=%d; dest=%d", secnum, it.getDestSectorIndex());
    bool r2 = ChangeSectorInternal(it.getDestSector(), crunch);
    if (r2) { ret = true; csTouched[secnum] |= 0x80000000U; }
  }

  return ret;
}


//==========================================================================
//
//  VLevel::ChangeSector
//
//  `-1` for `crunch` means "ignore stuck mobj"
//
//==========================================================================
bool VLevel::ChangeSector (sector_t *sector, int crunch) {
  if (!sector || NumSectors == 0) return false; // just in case
  if (!csTouched) {
    csTouched = (vuint32 *)Z_Calloc(NumSectors*sizeof(csTouched[0]));
    csTouchCount = 0;
  }
  if (++csTouchCount == 0x80000000U) {
    memset(csTouched, 0, NumSectors*sizeof(csTouched[0]));
    csTouchCount = 1;
  }
  IncrementSZValidCount();
  return ChangeSectorInternal(sector, crunch);
}
