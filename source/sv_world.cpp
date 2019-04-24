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
//  P_SectorClosestPoint
//
//  Given a point (x,y), returns the point (ox,oy) on the sector's defining
//  lines that is nearest to (x,y).
//  Ignores `z`.
//
//==========================================================================
TVec P_SectorClosestPoint (sector_t *sec, TVec in) {
  if (!sec) return in;

  double x = in.x, y = in.y;
  double bestdist = /*HUGE_VAL*/1e200;
  double bestx = x, besty = y;

  for (int f = 0; f < sec->linecount; ++f) {
    const line_t *check = sec->lines[f];
    const vertex_t *v1 = check->v1;
    const vertex_t *v2 = check->v2;
    double a = v2->x-v1->x;
    double b = v2->y-v1->y;
    const double den = a*a+b*b;
    double ix, iy, dist;

    if (fabs(den) <= 0.01) {
      // line is actually a point!
      ix = v1->x;
      iy = v1->y;
    } else {
      double num = (x-v1->x)*a+(y-v1->y)*b;
      double u = num/den;
      if (u <= 0) {
        ix = v1->x;
        iy = v1->y;
      } else if (u >= 1) {
        ix = v2->x;
        iy = v2->y;
      } else {
        ix = v1->x+u*a;
        iy = v1->y+u*b;
      }
    }
    a = ix-x;
    b = iy-y;
    dist = a*a+b*b;
    if (dist < bestdist)  {
      bestdist = dist;
      bestx = ix;
      besty = iy;
    }
  }
  return TVec(bestx, besty, in.z);
}


//==========================================================================
//
//  P_BoxOnLineSide
//
//  considers the line to be infinite
//  returns side 0 or 1, -1 if box crosses the line
//
//==========================================================================
int P_BoxOnLineSide (float *tmbox, line_t *ld) {
  int p1 = 0;
  int p2 = 0;

  switch (ld->slopetype) {
    case ST_HORIZONTAL:
      p1 = tmbox[BOXTOP] > ld->v1->y;
      p2 = tmbox[BOXBOTTOM] > ld->v1->y;
      if (ld->dir.x < 0) {
        p1 ^= 1;
        p2 ^= 1;
      }
      break;
    case ST_VERTICAL:
      p1 = tmbox[BOXRIGHT] < ld->v1->x;
      p2 = tmbox[BOXLEFT] < ld->v1->x;
      if (ld->dir.y < 0) {
        p1 ^= 1;
        p2 ^= 1;
      }
      break;
    case ST_POSITIVE:
      p1 = ld->PointOnSide(TVec(tmbox[BOXLEFT], tmbox[BOXTOP], 0));
      p2 = ld->PointOnSide(TVec(tmbox[BOXRIGHT], tmbox[BOXBOTTOM], 0));
      break;
    case ST_NEGATIVE:
      p1 = ld->PointOnSide(TVec(tmbox[BOXRIGHT], tmbox[BOXTOP], 0));
      p2 = ld->PointOnSide(TVec(tmbox[BOXLEFT], tmbox[BOXBOTTOM], 0));
      break;
  }

  if (p1 == p2) return p1;
  return -1;
}


//============================================================================
//
//  P_GetMidTexturePosition
//
//  retrieves top and bottom of the current line's mid texture
//
//============================================================================
bool P_GetMidTexturePosition (const line_t *linedef, int sideno, float *ptextop, float *ptexbot) {
  if (sideno < 0 || sideno > 1 || !linedef || linedef->sidenum[0] == -1 || linedef->sidenum[1] == -1) {
    if (ptextop) *ptextop = 0;
    if (ptexbot) *ptexbot = 0;
    return false;
  }

  const side_t *sidedef = &GLevel->Sides[linedef->sidenum[sideno]];
  if (sidedef->MidTexture <= 0) {
    if (ptextop) *ptextop = 0;
    if (ptexbot) *ptexbot = 0;
    return false;
  }

  VTexture *MTex = GTextureManager(sidedef->MidTexture);
  if (!MTex) {
    if (ptextop) *ptextop = 0;
    if (ptexbot) *ptexbot = 0;
    return false;
  }
  //FTexture * tex= TexMan(texnum);
  //if (!tex) return false;

  const sector_t *sec = (sideno ? linedef->backsector : linedef->frontsector);

  //FIXME: use sector regions instead?
  //       wtf are sector regions at all?!

  const float mheight = MTex->GetScaledHeight();

  float toffs;
  if (linedef->flags&ML_DONTPEGBOTTOM) {
    // bottom of texture at bottom
    toffs = sec->floor.TexZ+mheight;
  } else if (linedef->flags&ML_DONTPEGTOP) {
    // top of texture at top of top region
    toffs = sec->ceiling.TexZ;
  } else {
    // top of texture at top
    toffs = sec->ceiling.TexZ;
  }
  toffs *= MTex->TScale;
  toffs += sidedef->Mid.RowOffset*(MTex->bWorldPanning ? MTex->TScale : 1.0f);

  if (ptextop) *ptextop = toffs;
  if (ptexbot) *ptexbot = toffs-mheight;

  /*
  float totalscale = fabsf(sidedef->GetTextureYScale(side_t::mid)) * tex->GetScaleY();
  float y_offset = sidedef->GetTextureYOffset(side_t::mid);
  float textureheight = tex->GetHeight() / totalscale;
  if (totalscale != 1. && !tex->bWorldPanning) y_offset /= totalscale;

  if (linedef->flags & ML_DONTPEGBOTTOM) {
    *ptexbot = y_offset+MAX(linedef->frontsector->GetPlaneTexZ(sector_t::floor), linedef->backsector->GetPlaneTexZ(sector_t::floor));
    *ptextop = *ptexbot+textureheight;
  } else {
    *ptextop = y_offset+MIN(linedef->frontsector->GetPlaneTexZ(sector_t::ceiling), linedef->backsector->GetPlaneTexZ(sector_t::ceiling));
    *ptexbot = *ptextop-textureheight;
  }
  */

  return true;
}


//==========================================================================
//
//  DumpRegion
//
//==========================================================================
static __attribute__((unused)) void DumpRegion (const sec_region_t *inregion) {
  GCon->Logf("  %p: floor=(%g,%g,%g:%g); (%g : %g), flags=0x%04x; ceil=(%g,%g,%g:%g); (%g : %g), flags=0x%04x; eline=%d; rflags=0x%02x",
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
static __attribute__((unused)) void DumpOpening (const opening_t *op) {
  GCon->Logf("  %p: floor=%g (%g,%g,%g:%g); ceil=%g (%g,%g,%g:%g); lowfloor=%g; range=%g",
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
static __attribute__((unused)) void DumpOpPlanes (TArray<opening_t> &list) {
  GCon->Logf(" ::: count=%d :::", list.length());
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
static void GetBaseSectorOpening (opening_t &op, sector_t *sector, const TVec point, bool *hasSlopes, bool usePoint) {
  op.efloor = sector->eregions->efloor;
  op.eceiling = sector->eregions->eceiling;
  if (usePoint) {
    op.bottom = op.efloor.GetPointZ(point);
    op.top = op.eceiling.GetPointZ(point);
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
  if (hasSlopes) *hasSlopes = (op.efloor.isSlope() || op.eceiling.isSlope());
}


//==========================================================================
//
//  BuildSectorOpenings
//
//  this function doesn't like regions with floors that has different flags
//
//==========================================================================
static void BuildSectorOpenings (TArray<opening_t> &dest, sector_t *sector, const TVec point, unsigned NoBlockFlags, bool *hasSlopes, bool linkList, bool usePoint, bool skipNonSolid=false) {
  dest.reset();
  // if this sector has no 3d floors, we don't need to do any extra work
  if (!sector->Has3DFloors()) {
    opening_t &op = dest.alloc();
    GetBaseSectorOpening(op, sector, point, hasSlopes, usePoint);
    return;
  }
  /* build list of closed regions (it doesn't matter if region is non-solid, as long as it satisfies flag mask).
     that list has all intersecting regions joined.
     then cut those solids from main empty region.
   */
  static TArray<opening_t> solids;
  solids.reset();
  bool slopeDetected = false;
  opening_t cop;
  cop.lowfloor = 0.0f; // for now
  cop.highceiling = 0.0f; // for now
  //bool hadNonSolid;
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
    // check for slopes
    if (!slopeDetected) slopeDetected = (reg->efloor.isSlope() || reg->eceiling.isSlope());
    // border points
    const float fz = (usePoint ? reg->efloor.GetPointZ(point) : reg->efloor.splane->minz);
    const float cz = (usePoint ? reg->eceiling.GetPointZ(point) : reg->eceiling.splane->maxz);
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
  if (hasSlopes) *hasSlopes = slopeDetected;
  // if we have no openings, or openings are out of bounds, just use base sector region
  const float secfz = (usePoint ? sector->floor.GetPointZ(point) : sector->floor.minz);
  const float seccz = (usePoint ? sector->ceiling.GetPointZ(point) : sector->ceiling.maxz);
  if (solids.length() == 0 || solids[solids.length()-1].top <= secfz || solids[0].bottom >= seccz) {
    opening_t &op = dest.alloc();
    GetBaseSectorOpening(op, sector, point, hasSlopes, usePoint);
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
  check(currfloor.isFloor());
  // HACK: if the whole sector is taken by some region, return sector opening
  //       this is required to proper 3d-floor backside creation
  //       alas, `hadNonSolid` hack is required to get rid of "nano-water walls"
  opening_t *cs = solids.ptr();
  if (!usePoint /*&& !hadNonSolid*/ && solids.length() == 1) {
    if (cs[0].bottom <= sector->floor.minz && cs[0].top >= sector->ceiling.maxz) {
      opening_t &op = dest.alloc();
      GetBaseSectorOpening(op, sector, point, hasSlopes, usePoint);
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
  check(sector);
  static TArray<opening_t> oplist;
  BuildSectorOpenings(oplist, sector, TVec::ZeroVector, 0, nullptr, true, false, skipNonSolid);
  check(oplist.length() > 0);
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
  check(sector);
  static TArray<opening_t> oplist;
  BuildSectorOpenings(oplist, sector, TVec::ZeroVector, 0, nullptr, false, false, skipNonSolid);
  check(oplist.length() > 0);
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
  check(sector);
  static TArray<opening_t> oplist;
  BuildSectorOpenings(oplist, sector, TVec::ZeroVector, 0, nullptr, true, false, skipNonSolid);
  check(oplist.length() > 0);
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

  NoBlockFlags &= (SPF_MAX_OPENINGS-1);

  opening_t *op = nullptr;
  int opsused = 0;

  //FIXME: this is wrong, it should insert opening into full list instead!
  //       move opening scan to separate function with top/bot limits instead
  if (do3dmidtex && (linedef->flags&ML_3DMIDTEX)) {
    // for 3dmidtex, create two gaps:
    //   from floor to midtex bottom
    //   from midtex top to ceiling
    float top, bot;
    if (P_GetMidTexturePosition(linedef, 0, &top, &bot)) {
      float floorz = linedef->frontsector->floor.GetPointZ(point);
      float ceilz = linedef->frontsector->ceiling.GetPointZ(point);
      if (floorz < ceilz) {
        // clamp to sector height
        if (bot <= floorz && top >= ceilz) return nullptr; // it is completely blocked
        // bottom opening
        if (bot > floorz) {
          // from floor to bot
          openings[opsused].next = op;
          op = &openings[opsused];
          ++opsused;
          // bot
          op->bottom = floorz;
          op->lowfloor = floorz;
          op->efloor.set(&linedef->frontsector->floor, false);
          op->elowfloor = op->efloor;
          // top
          op->top = bot;
          op->highceiling = ceilz;
          op->eceiling.set(&linedef->frontsector->ceiling, false);
          op->ehighceiling = op->eceiling;
          op->range = op->top-op->bottom;
        }
        // top opening
        if (top < ceilz) {
          // from top to ceiling
          openings[opsused].next = op;
          op = &openings[opsused];
          ++opsused;
          // bot
          op->bottom = top;
          op->lowfloor = floorz;
          op->efloor.set(&linedef->frontsector->floor, false);
          op->elowfloor = op->efloor;
          // top
          op->top = ceilz;
          op->highceiling = ceilz;
          op->eceiling.set(&linedef->frontsector->ceiling, false);
          op->ehighceiling = op->eceiling;
          op->range = op->top-op->bottom;
        }
        return op;
      }
    }
  }

  // check opening cache
  op = linedef->oplist[NoBlockFlags];
  if (op) {
    // opening without floor plane means "no openings available"
    //GCon->Logf("%p: opcache %d hit!", linedef, NoBlockFlags);
    return (op->efloor.splane ? op : nullptr);
  }

  // alas, we cannot cache openings with slopes, so don't even try
  // this is bad, but meh... i can live with it for now

  bool hasSlopes0 = false, hasSlopes1 = false;

  // fast algo for two sectors without 3d floors
  if (!linedef->frontsector->Has3DFloors() &&
      !linedef->backsector->Has3DFloors())
  {
    opening_t fop, bop;
    GetBaseSectorOpening(fop, linedef->frontsector, point, &hasSlopes0, usePoint);
    GetBaseSectorOpening(bop, linedef->backsector, point, &hasSlopes1, usePoint);
    // no intersection?
    if (fop.top <= bop.bottom || bop.top <= fop.bottom ||
        fop.bottom >= bop.top || bop.bottom >= fop.top)
    {
      if (hasSlopes0 || hasSlopes1) return nullptr;
      if (linedef->oplistUsed < (unsigned)(NoBlockFlags+1)) linedef->oplistUsed = (unsigned)(NoBlockFlags+1);
      linedef->oplist[NoBlockFlags] = VLevel::AllocOpening();
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

  BuildSectorOpenings(op0list, linedef->frontsector, point, NoBlockFlags, &hasSlopes0, false, usePoint);
  if (op0list.length() == 0) {
    // just in case: no front sector openings
    if (hasSlopes0) return nullptr;
    if (linedef->oplistUsed < (unsigned)(NoBlockFlags+1)) linedef->oplistUsed = (unsigned)(NoBlockFlags+1);
    linedef->oplist[NoBlockFlags] = VLevel::AllocOpening();
    return nullptr;
  }

  BuildSectorOpenings(op1list, linedef->backsector, point, NoBlockFlags, &hasSlopes1, false, usePoint);
  if (op1list.length() == 0) {
    // just in case: no back sector openings
    if (hasSlopes0 || hasSlopes1) return nullptr;
    if (linedef->oplistUsed < (unsigned)(NoBlockFlags+1)) linedef->oplistUsed = (unsigned)(NoBlockFlags+1);
    linedef->oplist[NoBlockFlags] = VLevel::AllocOpening();
    return nullptr;
  }

#ifdef VV_DUMP_OPENING_CREATION
  GCon->Logf("*** line: %p (0x%02x) ***", linedef, NoBlockFlags);
  GCon->Log("::: before :::"); DumpOpPlanes(op0list); DumpOpPlanes(op1list);
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
      GCon->Log(" +++ SKIP op0 (dump: op0, op1) +++");
      DumpOpening(op0);
      DumpOpening(op1);
#endif
      ++op0; --op0left;
      continue;
    }
    // if op1 is below op0, skip op1
    if (op1->top < op0->bottom) {
#ifdef VV_DUMP_OPENING_CREATION
      GCon->Log(" +++ SKIP op1 (dump: op0, op1) +++");
      DumpOpening(op0);
      DumpOpening(op1);
#endif
      ++op1; --op1left;
      continue;
    }
    // here op0 and op1 are intersecting
    check(op0->bottom <= op1->top);
    check(op1->bottom <= op0->top);
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
      dest->eceiling = op1->eceiling;
    } else {
      dest->eceiling = op1->eceiling;
      dest->top = op1->top;
      dest->highceiling = op0->top;
      dest->eceiling = op0->eceiling;
    }
    dest->range = dest->top-dest->bottom;
#ifdef VV_DUMP_OPENING_CREATION
    GCon->Log(" +++ NEW opening (dump: op0, op1, new) +++");
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
      GCon->Log(" +++ SKIP BOTH +++");
#endif
      ++op0; --op0left;
      ++op1; --op1left;
    } else if (op0->top < op1->top) {
#ifdef VV_DUMP_OPENING_CREATION
      GCon->Log(" +++ SKIP OP0 +++");
#endif
      ++op0; --op0left;
    } else {
#ifdef VV_DUMP_OPENING_CREATION
      GCon->Log(" +++ SKIP OP1 +++");
#endif
      ++op1; --op1left;
    }
  }

#ifdef VV_DUMP_OPENING_CREATION
  GCon->Logf("::: after (%u) :::", destcount);
  for (unsigned f = 0; f < destcount; ++f) {
    const opening_t *xop = &openings[f];
    DumpOpening(xop);
  }
  GCon->Log("-----------------------------");
#endif

  // no intersections?
  if (destcount == 0) {
    // oops
    if (hasSlopes0 || hasSlopes1) return nullptr;
    if (linedef->oplistUsed < (unsigned)(NoBlockFlags+1)) linedef->oplistUsed = (unsigned)(NoBlockFlags+1);
    linedef->oplist[NoBlockFlags] = VLevel::AllocOpening();
    return nullptr;
  }

  // has some intersections, create cache
  if (!hasSlopes0 && !hasSlopes1) {
    if (linedef->oplistUsed < (unsigned)(NoBlockFlags+1)) linedef->oplistUsed = (unsigned)(NoBlockFlags+1);

    opening_t *lastop = nullptr, *dp = openings;
    for (unsigned f = 0; f < destcount; ++f, ++dp) {
      opening_t *newop = VLevel::AllocOpening();
      if (lastop) lastop->next = newop; else linedef->oplist[NoBlockFlags] = newop;
      newop->copyFrom(dp);
      lastop = newop;
    }

    return linedef->oplist[NoBlockFlags];
  } else {
    // has slopes, cannot cache openings (oops)
    if (destcount > 1) {
      for (unsigned f = 0; f < destcount-1; ++f) {
        openings[f].next = &openings[f+1];
      }
    }
    openings[destcount-1].next = nullptr;
    return openings;
  }
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

  BuildSectorOpenings(oplist, sector, point, SPF_NOBLOCKING, nullptr, true, true);
  if (oplist.length() == 0) {
    // something is very wrong here, so use sector boundaries
    floor = sector->eregions->efloor;
    ceiling = sector->eregions->eceiling;
    return;
  }

  if (debugDump) { GCon->Logf("=== ALL OPENINGS (z=%g; height=%g) ===", point.z, height); DumpOpPlanes(oplist); }

#if 1
  opening_t *opres = SV_FindRelOpening(oplist.ptr(), point.z, point.z+height);
  if (opres) {
    if (debugDump) { GCon->Logf(" found result"); DumpOpening(opres); }
    floor = opres->efloor;
    ceiling = opres->eceiling;
  } else {
    if (debugDump) GCon->Logf(" NO result");
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
      if (debugDump) { GCon->Logf(" best fit"); DumpOpening(op); }
      return op;
    } else {
      const float fdist = fabsf(point.z-fz); // we don't care about sign here
      if (!bestGap || fdist < bestGapDist) {
        if (debugDump) { GCon->Logf(" gap fit"); DumpOpening(op); }
        bestGap = op;
        bestGapDist = fdist;
        //if (fdist == 0.0f) break; // there is no reason to look further
      } else {
        if (debugDump) { GCon->Logf(" REJECTED gap fit"); DumpOpening(op); }
      }
    }
  }

  if (bestFit) {
    if (debugDump) { GCon->Logf(" best result"); DumpOpening(bestFit); }
    floor = bestFit->efloor;
    ceiling = bestFit->eceiling;
  } else if (bestGap) {
    if (debugDump) { GCon->Logf(" gap result"); DumpOpening(bestGap); }
    floor = bestGap->efloor;
    ceiling = bestGap->eceiling;
  } else {
    // just fit into sector
    if (debugDump) { GCon->Logf(" no result"); }
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
    floorz = sector->floor.GetPointZ(point);
    ceilz = sector->ceiling.GetPointZ(point);
    return;
  }
  TSecPlaneRef f, c;
  SV_FindGapFloorCeiling(sector, point, 0, f, c);
  floorz = f.GetPointZ(point);
  ceilz = c.GetPointZ(point);
}


//==========================================================================
//
//  SV_PointInRegion
//
//  this is used to get region lighting
//
//==========================================================================
sec_region_t *SV_PointRegionLight (sector_t *sector, const TVec &p, bool dbgDump) {
  if (!sector->Has3DFloors()) return sector->eregions;
  const float secfz = sector->floor.GetPointZ(p);
  if (p.z <= secfz) return sector->eregions;
  //const float seccz = sector->ceiling.GetPointZ(p);

  sec_region_t *best = sector->eregions;
  float bestDist = p.z-secfz; // minimum distance to region floor

  // skip base region
  for (sec_region_t *reg = sector->eregions->next; reg; reg = reg->next) {
    if (reg->regflags&sec_region_t::RF_OnlyVisual) continue;
    // non-solid?
    if (reg->regflags&sec_region_t::RF_NonSolid) {
      // for non-solid regions calculate distance to ceiling
      const float cz = reg->eceiling.GetPointZ(p);
      if (p.z < cz) {
        const float fdist = cz-p.z;
        if (fdist < bestDist) {
          bestDist = fdist;
          best = reg;
        }
      }
    } else {
      // for solid regions calculate distance to floor
      const float fz = reg->efloor.GetPointZ(p);
      if (p.z < fz) {
        const float fdist = fz-p.z;
        if (fdist < bestDist) {
          bestDist = fdist;
          best = reg;
        }
      }
    }
  }

  return best;
}


//==========================================================================
//
//  SV_PointContents
//
//==========================================================================
int SV_PointContents (sector_t *sector, const TVec &p) {
  if (!sector) return 0;

  if (sector->heightsec && (sector->heightsec->SectorFlags&sector_t::SF_UnderWater) &&
      p.z <= sector->heightsec->floor.GetPointZ(p))
  {
    return CONTENTS_BOOMWATER;
  }

  if (sector->SectorFlags&sector_t::SF_UnderWater) return CONTENTS_BOOMWATER;

  const sec_region_t *best = sector->eregions;

  if (sector->Has3DFloors()) {
    const float secfz = sector->floor.GetPointZ(p);
    const float seccz = sector->ceiling.GetPointZ(p);
    if (p.z < secfz || p.z > seccz) return best->params->contents;

    // prefer regions with contents
    float bestDist = 999999.0f; // minimum distance to region floor
    // skip base region
    for (const sec_region_t *reg = sector->eregions->next; reg; reg = reg->next) {
      if (reg->regflags&sec_region_t::RF_OnlyVisual) continue;
      // non-solid?
      const float cz = MIN(seccz, reg->eceiling.GetPointZ(p));
      if (reg->regflags&sec_region_t::RF_NonSolid) {
        const float fz = MAX(secfz, reg->efloor.GetPointZ(p));
        // check if point is inside, and for best floor dist
        if (p.z >= fz && p.z <= cz) {
          const float fdist = p.z-fz;
          if (fdist < bestDist) {
            bestDist = fdist;
            best = reg;
          }
        }
      } else {
        // check if we are above it
        if (p.z >= cz) {
          const float fdist = p.z-cz;
          if (fdist < bestDist) {
            bestDist = fdist;
            best = reg;
          }
        }
      }
    }
  }

  return best->params->contents;
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
float SV_GetLowestSolidPointZ (sector_t *sector, const TVec &point) {
  if (!sector) return 0.0f; // idc
  return sector->floor.GetPointZ(point); // cannot be lower than this
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
//  VLevel::ChangeSectorInternal
//
//  jff 3/19/98 added to just check monsters on the periphery of a moving
//  sector instead of all in bounding box of the sector. Both more accurate
//  and faster.
//
//==========================================================================
bool VLevel::ChangeSectorInternal (sector_t *sector, int crunch) {
  check(sector);
  int secnum = (int)(ptrdiff_t)(sector-Sectors);
  if ((csTouched[secnum]&0x7fffffffU) == csTouchCount) return !!(csTouched[secnum]&0x80000000U);
  csTouched[secnum] = csTouchCount;

  CalcSecMinMaxs(sector);

  // reset opening cache
  {
    line_t **lptr = sector->lines;
    for (int lcount = sector->linecount; lcount--; ++lptr) {
      line_t *line = *lptr;
      if (line->oplistUsed) {
        //GCon->Logf("%p: opcache invalidated (%u)!", line, line->oplistUsed);
        for (unsigned f = 0; f < line->oplistUsed; ++f) FreeOpeningList(line->oplist[f]);
        line->oplistUsed = 0;
      }
    }
  }

  bool ret = false;

  // killough 4/4/98: scan list front-to-back until empty or exhausted,
  // restarting from beginning after each thing is processed. Avoids
  // crashes, and is sure to examine all things in the sector, and only
  // the things which are in the sector, until a steady-state is reached.
  // Things can arbitrarily be inserted and removed and it won't mess up.
  //
  // killough 4/7/98: simplified to avoid using complicated counter
  //
  // ketmar: mostly rewritten

  msecnode_t *n;

  // mark all things invalid
  for (n = sector->TouchingThingList; n; n = n->SNext) n->Visited = 0;

  do {
    // go through list
    for (n = sector->TouchingThingList; n; n = n->SNext) {
      if (!n->Visited) {
        // unprocessed thing found, mark thing as processed
        n->Visited = 1;
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

  // this checks the case when 3d control sector moved (3d lifts)
  for (auto it = IterControlLinks(sector); !it.isEmpty(); it.next()) {
    //GCon->Logf("*** src=%d; dest=%d", secnum, it.getDestSectorIndex());
    bool r2 = ChangeSectorInternal(it.getDestSector(), crunch);
    if (r2) { ret = true; csTouched[secnum] |= 0x80000000U; }
  }

  return ret;
}


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
  return ChangeSectorInternal(sector, crunch);
}
