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
#define MAX_OPENINGS  (65536)
static opening_t openings[MAX_OPENINGS];


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
    toffs = sec->topregion->eceiling.splane->TexZ;
  } else {
    // top of texture at top
    toffs = sec->ceiling.TexZ;
  }
  toffs *= MTex->TScale;
  toffs += sidedef->MidRowOffset*(MTex->bWorldPanning ? MTex->TScale : 1.0f);

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


extern "C" {
  // sort by floors
  static int compareOpenings (const void *aa, const void *bb, void *nbfxx) {
    if (aa == bb) return 0;
    const opening_t *a = (const opening_t *)aa;
    const opening_t *b = (const opening_t *)bb;
    const float dist = a->bottom-b->bottom;
    if (dist) return (dist < 0 ? -1 : 1);
    const unsigned nbf = (unsigned)(uintptr_t)nbfxx;
    // same bottom; solid floors must came first
    if ((a->efloor.splane->flags&nbf) == 0) {
      // `a` is solid
      // if `b` is solid, they are equal
      // if `b` is non-solid, `b` is lesser than `a`
      return ((b->efloor.splane->flags&nbf) == 0 ? 0 : 1);
    } else if ((b->efloor.splane->flags&nbf) == 0) {
      // `b` is solid
      // if `a` is solid, they are equal
      // if `a` is non-solid, `a` is lesser than `b`
      return ((a->efloor.splane->flags&nbf) == 0 ? 0 : -1);
    }
    return 0;
  }
}


//==========================================================================
//
//  CompressOpenings
//
//  join consecutive openings, remove paper-thin regions
//  put resulting set into `openings` array, return number of openings
//
//==========================================================================
static unsigned CompressOpenings (opening_t *oplistdest, opening_t *oplist, const unsigned count, const unsigned NoBlockFlags) {
  if (!count) return 0;
  unsigned cpos = 0;
  // find first solid floor: we cannot go lower than that
  while (cpos < count && (oplist[0].efloor.splane->flags&NoBlockFlags) != 0) ++cpos;
  if (cpos >= count) return 0; // oops
  // we have one
  unsigned opused = 1;
  // copy current opening
  oplistdest[0] = oplist[cpos];
  // now try to insert all openings one by one
  cpos = 0;
  while (cpos < count) {
    opening_t &currop = oplist[cpos];
    // ignore regions under first solid floor
    if (currop.top <= oplistdest[0].bottom) { ++cpos; continue; }
    // find resulting region that contains this floor
    unsigned dpos = 0;
    // if current region doesn't start below the first solid one, first one contains it
    if (currop.bottom > oplistdest[0].bottom) {
      while (dpos < opused) {
        if (currop.bottom >= oplistdest[dpos].bottom && currop.bottom <= oplistdest[dpos].top) break;
        ++dpos;
      }
      if (dpos >= opused) {
        // nothing to extend, this is new region
        oplistdest[opused++] = oplist[cpos++];
        continue;
      }
    }
    // found region to extend, do it
    opening_t &currdest = oplistdest[dpos];
    // first, replace floor if old one is non-solid, and new one is solid
    if (currdest.bottom == currop.bottom) {
      if ((currdest.efloor.splane->flags&NoBlockFlags) == 0 &&
          (currop.efloor.splane->flags&NoBlockFlags) != 0)
      {
        currdest.efloor = currop.efloor;
      }
    }
    // now check ceiling
    if (currop.top <= currdest.top) {
      // current region is completely inside destination, nothing to do
      ++cpos;
      continue;
    }
    // bump ceiling
    currdest.eceiling = currop.eceiling;
    currdest.top = currop.top;
    // check if we hit some next regions
    ++dpos; // move to next
    const unsigned cdpidx = dpos;
    while (dpos < opused && currdest.top >= oplistdest[dpos].bottom) {
      // join current destination and next one
      currdest.eceiling = oplistdest[dpos].eceiling;
      currdest.top = oplistdest[dpos].top;
      ++dpos; // move to next
    }
    // if our dpos is out of range here, it means that we eaten all possible regions
    if (dpos >= opused) {
      // everything was eaten
      opused = cdpidx;
    } else {
      // move uneaten regions
      memmove((void *)(oplistdest+cdpidx), (void *)(oplistdest+dpos), (opused-dpos)*sizeof(oplistdest[0]));
      opused -= dpos-cdpidx;
    }
    // move to next source region
    ++cpos;
  }

  return opused;
}


//==========================================================================
//
//  SV_LineOpenings
//
//  sets opentop and openbottom to the window through a two sided line
//
//==========================================================================
opening_t *SV_LineOpenings (const line_t *linedef, const TVec &point, int NoBlockFlags, bool do3dmidtex) {
  if (linedef->sidenum[1] == -1 || !linedef->backsector) return nullptr; // single sided line

  NoBlockFlags &= (SPF_MAX_OPENINGS-1);

  opening_t *op = nullptr;
  int opsused = 0;
  sec_region_t *frontreg = linedef->frontsector->botregion;
  sec_region_t *backreg = linedef->backsector->botregion;

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
          //op->lowfloorplane = &linedef->frontsector->floor;
          // top
          op->top = bot;
          //op->highceiling = ceilz;
          op->eceiling.set(&linedef->frontsector->ceiling, false);
          //op->highceilingplane = &linedef->frontsector->ceiling;
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
          //op->lowfloorplane = &linedef->frontsector->floor;
          // top
          op->top = ceilz;
          //op->highceiling = ceilz;
          op->eceiling.set(&linedef->frontsector->ceiling, false);
          //op->highceilingplane = &linedef->frontsector->ceiling;
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

#if 1
  /* build openings
     first, create openings for front sector
     second, insert openings for back sector, shrinking/removing unnecessary ones

     we will cache build list, so we can spend some time doing better work
   */

  //GCon->Logf("%p: caching for 0x%02x!", linedef, (unsigned)NoBlockFlags);

  // it will be cached anyway
  if (linedef->oplistUsed < (unsigned)(NoBlockFlags+1)) linedef->oplistUsed = (unsigned)(NoBlockFlags+1);

  // collect front sector openings
  unsigned fopused = 0;
  for (sec_region_t *reg = frontreg; reg; reg = reg->next) {
    if (opsused >= MAX_OPENINGS/2) Host_Error("too many openings for line!");
    opening_t *xop = &openings[MAX_OPENINGS/2+fopused++];
    xop->top = reg->eceiling.GetPointZ(point);
    xop->bottom = reg->efloor.GetPointZ(point);
    // ignore invalid and paper-thin regions
    if (xop->top <= xop->bottom) { --fopused; continue; }
    xop->eceiling = reg->eceiling;
    xop->efloor = reg->efloor;
  }
  // sort
  timsort_r(openings+MAX_OPENINGS/2, fopused, sizeof(openings[0]), &compareOpenings, (void *)(uintptr_t)NoBlockFlags);
  // compress
  opening_t *foplist = openings;
  fopused = CompressOpenings(foplist, openings+MAX_OPENINGS/2, fopused, (unsigned)NoBlockFlags);
  // check
  if (fopused == 0) {
    // just in case: no front sector openings
    linedef->oplist[NoBlockFlags] = VLevel::AllocOpening();
    return nullptr;
  }

  // collect back sector openings
  unsigned bopused = 0;
  for (sec_region_t *reg = backreg; reg; reg = reg->next) {
    if (opsused >= MAX_OPENINGS/2) Host_Error("too many openings for line!");
    opening_t *xop = &openings[MAX_OPENINGS/2+bopused++];
    xop->top = reg->eceiling.GetPointZ(point);
    xop->bottom = reg->efloor.GetPointZ(point);
    // ignore invalid and paper-thin regions
    if (xop->top <= xop->bottom) { --bopused; continue; }
    xop->eceiling = reg->eceiling;
    xop->efloor = reg->efloor;
  }
  // sort
  timsort_r(openings+MAX_OPENINGS/2, bopused, sizeof(openings[0]), &compareOpenings, (void *)(uintptr_t)NoBlockFlags);
  // compress
  opening_t *boplist = openings+fopused;
  bopused = CompressOpenings(boplist, openings+MAX_OPENINGS/2, bopused, (unsigned)NoBlockFlags);
  // check
  if (bopused == 0) {
    // just in case: no back sector openings
    linedef->oplist[NoBlockFlags] = VLevel::AllocOpening();
    return nullptr;
  }

  /* now we have two lists; build destination intersection list.
     walk by both lists, and save intersection in destination list.
   */
  opening_t *destlist = boplist+bopused;
  unsigned destcount = 0;

  unsigned fpos = 0, bpos = 0;
  // find first intersection
  while (fpos < fopused && bpos < bopused) {
    opening_t *fp = &foplist[fpos];
    opening_t *bp = &boplist[bpos];
    if (fp->top <= bp->bottom) {
      // first list region is lower than second list region, try next first list region
      ++fpos;
      continue;
    }
    if (bp->top <= fp->bottom) {
      // second list region is lower than first list region, try next second list region
      ++bpos;
      continue;
    }
    if (fp->bottom >= bp->top) {
      // first list region is higher than second list region, try next second list region
      ++bpos;
      continue;
    }
    if (bp->bottom >= fp->top) {
      // second list region is higher than first list region, try next first list region
      ++fpos;
      continue;
    }
    // this should be an intersection
    check(!(fp->top <= bp->bottom || fp->bottom >= bp->top));
    check(!(bp->top <= fp->bottom || bp->bottom >= fp->top));
    // store first intersection
    opening_t *dp = &destlist[destcount++];
    // floor
    if (fp->bottom >= bp->bottom) {
      // first floor is higher than second floor
      dp->efloor = fp->efloor;
      dp->bottom = fp->bottom;
      dp->lowfloor = bp->bottom; // for dropoffs
    } else {
      // second floor is higher than first floor
      dp->efloor = bp->efloor;
      dp->bottom = bp->bottom;
      dp->lowfloor = fp->bottom; // for dropoffs
    }
    // ceiling
    if (fp->top <= bp->top) {
      // first ceiling is lower than second ceiling
      dp->eceiling = fp->eceiling;
      dp->top = fp->top;
    } else {
      // second ceiling is lower than first ceiling
      dp->eceiling = bp->eceiling;
      dp->top = bp->top;
    }
    // skip one or both source regions
    if (fp->top == bp->top) {
      // both ceilings are the same, skip both regions
      ++fpos;
      ++bpos;
    } else if (fp->top < bp->top) {
      // first ceiling is lower than second, skip first
      ++fpos;
    } else {
      // second ceiling is lower than first, skip second
      check(bp->top < fp->top);
      ++bpos;
    }
  }

  // no intersections?
  if (destcount == 0) {
    // oops
    linedef->oplist[NoBlockFlags] = VLevel::AllocOpening();
    return nullptr;
  }

  // has some intersections, create cache
  {
    opening_t *lastop = nullptr, *dp = destlist;
    for (unsigned f = 0; f < destcount; ++f, ++dp) {
      // calc range
      dp->range = dp->top-dp->bottom;
      if (dp->range <= 0.0f) continue; // oops
      //GCon->Logf("  %p: bot=%g; top=%g", linedef, dp->bottom, dp->top);
      opening_t *newop = VLevel::AllocOpening();
      if (lastop) lastop->next = newop; else linedef->oplist[NoBlockFlags] = newop;
      newop->copyFrom(dp);
      lastop = newop;
    }
  }

  return linedef->oplist[NoBlockFlags];

#else
  TSecPlaneRef frontfloor;
  TSecPlaneRef backfloor;
  TSecPlaneRef frontceil;
  TSecPlaneRef backceil;

  float frontfloorz = 0.0f;
  float backfloorz = 0.0f;
  float frontceilz = 0.0f;
  float backceilz = 0.0f;

  while (frontreg && backreg) {
    if (backreg->efloor.GetPointZ(point) > backreg->eceiling.GetPointZ(point)) {
      /*
      GCon->Logf("WUTAFUCK-BACK: (%g,%g,%g:%g) - (%g,%g,%g:%g); fz=%g; cz=%g; xfz=%g; xcz=%g; pt=(%g,%g)",
        backreg->efloor.GetNormal().x, backreg->efloor.GetNormal().y, backreg->efloor.GetNormal().z, backreg->efloor.GetDist(),
        backreg->eceiling.GetNormal().x, backreg->eceiling.GetNormal().y, backreg->eceiling.GetNormal().z, backreg->eceiling.GetDist(),
        backreg->efloor.GetPointZ(point), backreg->eceiling.GetPointZ(point),
        backreg->efloor.splane->GetPointZ(point), backreg->eceiling.splane->GetPointZ(point),
        point.x, point.y);
      GCon->Logf("WUTAFUCK-BACK:   (%g,%g,%g:%g) - (%g,%g,%g:%g)",
        backreg->efloor.splane->normal.x, backreg->efloor.splane->normal.y, backreg->efloor.splane->normal.z, backreg->efloor.splane->dist,
        backreg->eceiling.splane->normal.x, backreg->eceiling.splane->normal.y, backreg->eceiling.splane->normal.z, backreg->eceiling.splane->dist);
      VLevel::dumpSectorRegions(linedef->backsector);
      */
      backreg = backreg->next;
      continue;
    }
    if (frontreg->efloor.GetPointZ(point) > frontreg->eceiling.GetPointZ(point)) {
      /*
      GCon->Logf("WUTAFUCK-FRONT: (%g,%g,%g:%g) - (%g,%g,%g:%g); fz=%g; cz=%g; xfz=%g; xcz=%g; pt=(%g,%g)",
        frontreg->efloor.GetNormal().x, frontreg->efloor.GetNormal().y, frontreg->efloor.GetNormal().z, frontreg->efloor.GetDist(),
        frontreg->eceiling.GetNormal().x, frontreg->eceiling.GetNormal().y, frontreg->eceiling.GetNormal().z, frontreg->eceiling.GetDist(),
        frontreg->efloor.GetPointZ(point), frontreg->eceiling.GetPointZ(point),
        frontreg->efloor.splane->GetPointZ(point), frontreg->eceiling.splane->GetPointZ(point),
        point.x, point.y);
      GCon->Logf("WUTAFUCK-FRONT:   (%g,%g,%g:%g) - (%g,%g,%g:%g)",
        frontreg->efloor.splane->normal.x, frontreg->efloor.splane->normal.y, frontreg->efloor.splane->normal.z, frontreg->efloor.splane->dist,
        frontreg->eceiling.splane->normal.x, frontreg->eceiling.splane->normal.y, frontreg->eceiling.splane->normal.z, frontreg->eceiling.splane->dist);
      VLevel::dumpSectorRegions(linedef->frontsector);
      */
      frontreg = frontreg->next;
      continue;
    }

    if (!(frontreg->efloor.splane->flags&NoBlockFlags)) {
      frontfloor = frontreg->efloor;
      frontfloorz = frontfloor.GetPointZ(point);
    }
    if (!(backreg->efloor.splane->flags&NoBlockFlags)) {
      backfloor = backreg->efloor;
      backfloorz = backfloor.GetPointZ(point);
    }
    if (!(frontreg->eceiling.splane->flags&NoBlockFlags)) {
      frontceil = frontreg->eceiling;
      frontceilz = frontceil.GetPointZ(point);
    }
    if (!(backreg->eceiling.splane->flags&NoBlockFlags)) {
      backceil = backreg->eceiling;
      backceilz = backceil.GetPointZ(point);
    }

    if (backreg->eceiling.splane->flags&NoBlockFlags) { backreg = backreg->next; continue; }
    if (frontreg->eceiling.splane->flags&NoBlockFlags) { frontreg = frontreg->next; continue; }

    if (frontfloorz >= backceilz) { backreg = backreg->next; continue; }
    if (backfloorz >= frontceilz) { frontreg = frontreg->next; continue; }

    if (opsused >= MAX_OPENINGS) { GCon->Logf("too many openings for line!"); break; }
    openings[opsused].next = op;
    op = &openings[opsused++];

    if (frontfloorz > backfloorz) {
      op->bottom = frontfloorz;
      op->lowfloor = backfloorz;
      op->efloor = frontfloor;
      //op->lowfloorplane = backfloor;
    } else {
      op->bottom = backfloorz;
      op->lowfloor = frontfloorz;
      op->efloor = backfloor;
      //op->lowfloorplane = frontfloor;
    }

    if (frontceilz < backceilz) {
      op->top = frontceilz;
      op->highceiling = backceilz;
      op->eceiling = frontceil;
      //op->highceilingplane = backceil;
      frontreg = frontreg->next;
    } else {
      op->top = backceilz;
      op->highceiling = frontceilz;
      op->eceiling = backceil;
      //op->highceilingplane = frontceil;
      backreg = backreg->next;
    }

    op->range = op->top-op->bottom;
  }

  // cache it
  if (linedef->oplistUsed < (unsigned)(NoBlockFlags+1)) linedef->oplistUsed = (unsigned)(NoBlockFlags+1);
  if (op) {
    opening_t *lastop = nullptr;
    for (opening_t *xop = op; xop; xop = xop->next) {
      opening_t *newop = VLevel::AllocOpening();
      if (lastop) lastop->next = newop; else linedef->oplist[NoBlockFlags] = newop;
      newop->copyFrom(xop);
      lastop = newop;
    }
  } else {
    // no openings found, cache dummy item
    linedef->oplist[NoBlockFlags] = VLevel::AllocOpening();
  }
#endif

  return op;
}


//==========================================================================
//
//  SV_FindFloorCeiling
//
//==========================================================================
static inline void SV_FindFloorCeiling (sec_region_t *gap, const TVec &point, sec_region_t *&floor, sec_region_t *&ceiling) {
  // find solid floor
  floor = gap;
  for (sec_region_t *reg = gap; reg; reg = reg->prev) {
    if ((reg->efloor.splane->flags&SPF_NOBLOCKING) == 0) {
      floor = reg;
      break;
    }
  }
  // find solid ceiling
  ceiling = gap;
  for (sec_region_t *reg = gap; reg; reg = reg->next) {
    if ((reg->eceiling.splane->flags&SPF_NOBLOCKING) == 0) {
      ceiling = reg;
      break;
    }
  }
}


//==========================================================================
//
//  SV_FindThingGap
//
//  Find the best gap that the thing could fit in, given a certain Z
//  position (z1 is foot, z2 is head). Assuming at least two gaps exist,
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
//  Returns the gap, or `nullptr` if there are no gaps at all.
//
//==========================================================================
sec_region_t *SV_FindThingGap (sec_region_t *InGaps, const TVec &point, float height, bool dbgDump) {
  /* k8: here, we should return gap we can fit in, even if we are partially in it.
         this is because sector height change is using this to do various checks
   */
  if (height < 0.0f) height = 0.0f;
  const float z1 = point.z;
  const float z2 = z1+height;

  sec_region_t *gaps = InGaps;

  // check for trivial gaps
  if (gaps == nullptr) return nullptr;
  if (gaps->next == nullptr) return gaps;

  // region we are at least partially inside, and can fit
  float bestFitFloorDist = 999999.0f;
  sec_region_t *bestFit = nullptr;

  // region we can possibly fit, but not even partially inside
  float bestPossibleFitFloorDist = 999999.0f;
  sec_region_t *bestPossibleFit = nullptr;

  // try to find a gap we can fit in
  for (sec_region_t *reg = gaps; reg; reg = reg->next) {
    float fz = reg->efloor.GetPointZ(point);
    float cz = reg->eceiling.GetPointZ(point);
    if (fz > cz) continue;
    float fdist = fabsf(z1-fz); // we don't care about sign
    // at least partially inside?
    if (z2 >= fz && z1 <= cz) {
      // at least partially inside, check if we can fit
      bool canFit = (z1 >= fz && z2 <= cz);
      if (!canFit) {
        // can't fit into current region, but it can be swimmable pool, so find solid floor and ceiling
        sec_region_t *rfloor, *rceil;
        SV_FindFloorCeiling(reg, point, rfloor, rceil);
        fz = rfloor->efloor.GetPointZ(point);
        cz = rceil->eceiling.GetPointZ(point);
        canFit = (z1 >= fz && z2 <= cz);
      }
      if (canFit) {
        // ok, we can fit here, check if this is a better candidate
        if (!bestFit || fdist < bestFitFloorDist) {
          bestFitFloorDist = fdist;
          bestFit = reg;
        }
      }
    } else {
      // do "can fit vertically" checks, and choose the best one
      // but only if we don't have "can fit" yet
      if (!bestFit) {
        sec_region_t *rfloor, *rceil;
        SV_FindFloorCeiling(reg, point, rfloor, rceil);
        fz = rfloor->efloor.GetPointZ(point);
        cz = rceil->eceiling.GetPointZ(point);
        if (z1 >= fz && z2 <= cz) {
          // can fit, choose closest floor
          if (!bestPossibleFit || fdist < bestPossibleFitFloorDist) {
            bestPossibleFitFloorDist = fdist;
            bestPossibleFit = reg;
          }
        }
      }
    }
  }

  if (bestFit) return bestFit;
  if (bestPossibleFit) return bestPossibleFit;

  // we don't have anything to fit into; this is something that should not happen
  // yet, it *may* happen for bad maps or noclip, so return region with closest floor
  bestFitFloorDist = 999999.0f;
  bestFit = nullptr;
  for (sec_region_t *reg = gaps; reg; reg = reg->next) {
    float fz = reg->efloor.GetPointZ(point);
    if (fz > reg->eceiling.GetPointZ(point)) continue;
    float fdist = fabsf(z1-fz); // we don't care about sign
    if (!bestFit || fdist < bestFitFloorDist) {
      bestFitFloorDist = fdist;
      bestFit = reg;
    }
  }
  return bestFit;

#if 0
  int fit_num = 0;
  sec_region_t *fit_last = nullptr;

  sec_region_t *fit_closest = nullptr;
  float fit_mindist = 200000.0f;

  sec_region_t *nofit_closest = nullptr;
  float nofit_mindist = 200000.0f;

  TSecPlaneRef floor;
  TSecPlaneRef ceil;
  sec_region_t *lastFloorGap = nullptr;

  float bestFloorDist = 999999.0f;
  sec_region_t *bestFloor = nullptr;

  // there are 2 or more gaps; now it gets interesting :-)
  while (gaps) {
    if (dbgDump) GCon->Logf("  svftg: checking gap=%p; z1=%g; z2=%g (regflags=0x%02x)", gaps, z1, z2, gaps->regflags);
    if ((gaps->efloor.splane->flags&SPF_NOBLOCKING) == 0) {
      if (dbgDump) GCon->Logf("  svftg: new floor gap=%p; z1=%g; z2=%g", gaps, z1, z2);
      floor = gaps->efloor;
      lastFloorGap = gaps;
    }
    if ((gaps->eceiling.splane->flags&SPF_NOBLOCKING) == 0) {
      if (dbgDump) GCon->Logf("  svftg: new ceiling gap=%p; z1=%g; z2=%g", gaps, z1, z2);
      ceil = gaps->eceiling;
    }
    if (gaps->eceiling.splane->flags&SPF_NOBLOCKING) {
      //if ((gaps->regflags&sec_region_t::RF_NonSolid))
      {
        if (dbgDump) GCon->Logf("  svftg: skip ceiling gap=%p; z1=%g; z2=%g", gaps, z1, z2);
        gaps = gaps->next;
        continue;
      }
    }
    /*
    if (gaps->efloor.splane->flags&SPF_NOBLOCKING) {
      //if ((gaps->regflags&sec_region_t::RF_NonSolid))
      {
        if (dbgDump) GCon->Logf("  svftg: skip floor gap=%p; z1=%g; z2=%g", gaps, z1, z2);
        gaps = gaps->next;
        continue;
      }
    }
    */

    const float f = floor.GetPointZ(point);
    const float c = ceil.GetPointZ(point);

    if (dbgDump) GCon->Logf("  svftg: gap=%p; f=%g; c=%g; z1=%g; z2=%g", gaps, f, c, z1, z2);

    if (z1 >= f && z2 <= c) {
      if (dbgDump) GCon->Logf("  svftg RES: gap=%p; f=%g; c=%g; z1=%g; z2=%g", gaps, f, c, z1, z2);
      //return gaps; // [1]
      return (lastFloorGap ? lastFloorGap : gaps);
    }

    const float dist = fabsf(z1-f);

    if (z2-z1 <= c-f) {
      // [2]
      ++fit_num;
      fit_last = lastFloorGap; //gaps;
      if (dbgDump) GCon->Logf("  svftg: fit_last=%p (%d); f=%g; c=%g; z1=%g; z2=%g", fit_last, fit_num, f, c, z1, z2);
      if (dist < fit_mindist) {
        // [3]
        fit_mindist = dist;
        fit_closest = lastFloorGap; //gaps;
        if (dbgDump) GCon->Logf("  svftg: fit_closest=%p (%d); f=%g; c=%g; z1=%g; z2=%g", fit_closest, fit_num, f, c, z1, z2);
      }
    } else {
      if (dist < nofit_mindist) {
        // [4]
        nofit_mindist = dist;
        nofit_closest = lastFloorGap; //gaps;
        if (dbgDump) GCon->Logf("  svftg: nofit_closest=%p (%d); f=%g; c=%g; z1=%g; z2=%g", nofit_closest, fit_num, f, c, z1, z2);
      }
    }
    gaps = gaps->next;
  }

  if (fit_num == 1) return fit_last;
  if (fit_num > 1) return fit_closest;
  return nofit_closest;
#endif
}


//==========================================================================
//
//  SV_FindGapFloorCeiling
//
//  this calls `SV_FindThingGap`, and returns blocking
//  floor and ceiling planes
//
//==========================================================================
void SV_FindGapFloorCeiling (const sector_t *sector, const TVec &p, float height, TSecPlaneRef &floor, TSecPlaneRef &ceiling) {
  sec_region_t *gap = SV_FindThingGap(sector->botregion, p, height);
  check(gap);
  // get floor
  floor = gap->efloor;
  for (sec_region_t *reg = gap; reg; reg = reg->prev) {
    if ((reg->efloor.splane->flags&SPF_NOBLOCKING) == 0) {
      floor = reg->efloor;
      break;
    }
  }
  // get ceiling
  ceiling = gap->eceiling;
  for (sec_region_t *reg = gap; reg; reg = reg->next) {
    if ((reg->eceiling.splane->flags&SPF_NOBLOCKING) == 0) {
      ceiling = reg->eceiling;
      break;
    }
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
  opening_t *gaps = InGaps;

  int fit_num = 0;
  opening_t *fit_last = nullptr;

  opening_t *fit_closest = nullptr;
  float fit_mindist = 99999.0f;

  opening_t *nofit_closest = nullptr;
  float nofit_mindist = 99999.0f;

  // check for trivial gaps
  if (!gaps) return nullptr;
  if (!gaps->next) return gaps;

  // there are 2 or more gaps; now it gets interesting :-)
  while (gaps) {
    const float f = gaps->bottom;
    const float c = gaps->top;

    if (z1 >= f && z2 <= c) return gaps; // [1]

    const float dist = fabsf(z1-f);

    if (z2 - z1 <= c - f) {
      // [2]
      ++fit_num;
      fit_last = gaps;
      if (dist < fit_mindist) {
        // [3]
        fit_mindist = dist;
        fit_closest = gaps;
      }
    } else {
      if (dist < nofit_mindist) {
        // [4]
        nofit_mindist = dist;
        nofit_closest = gaps;
      }
    }
    gaps = gaps->next;
  }

  if (fit_num == 1) return fit_last;
  if (fit_num > 1) return fit_closest;
  return nofit_closest;
}


//==========================================================================
//
//  SV_PointInRegion
//
//==========================================================================
sec_region_t *SV_PointInRegion (const sector_t *sector, const TVec &p) {
  sec_region_t *best = nullptr;
  float bestDist = 999999.0f; // minimum distance to region floor
  // logic: find matching region, otherwise return highest one
  for (sec_region_t *reg = sector->botregion; reg && reg->next; reg = reg->next) {
    const float fz = reg->efloor.GetPointZ(p);
    const float cz = reg->eceiling.GetPointZ(p);
    if (p.z >= fz && p.z <= cz) {
      const float fdist = p.z-fz;
      // prefer regions with contents
      if (best) {
        if (!reg->params->contents) {
          // no contents in current region
          if (best->params->contents) continue;
        } else {
          // current region has contents
          if (!best->params->contents) {
            best = reg;
            bestDist = fdist;
            continue;
          }
        }
      }
      if (!best || fdist < bestDist) {
        best = reg;
        bestDist = fdist;
      }
    }
    //if (p.z < reg->eceiling.GetPointZ(p)) break;
  }
  return (best ? best : sector->botregion);
}


//==========================================================================
//
//  SV_PointContents
//
//==========================================================================
int SV_PointContents (const sector_t *sector, const TVec &p) {
  check(sector);
  if (sector->heightsec &&
      (sector->heightsec->SectorFlags&sector_t::SF_UnderWater) &&
      p.z <= sector->heightsec->floor.GetPointZ(p))
  {
    return 9;
  }
  if (sector->SectorFlags&sector_t::SF_UnderWater) return 9;

  if (sector->SectorFlags&sector_t::SF_HasExtrafloors) {
    /*
    for (sec_region_t *reg = sector->botregion; reg; reg = reg->next) {
      if (p.z <= reg->eceiling.GetPointZ(p) && p.z >= reg->efloor.GetPointZ(p)) {
        return reg->params->contents;
      }
    }
    */
    sec_region_t *reg = SV_PointInRegion(sector, p);
    return (reg ? reg->params->contents : -1);
  } else {
    return sector->botregion->params->contents;
  }
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
        if (!n->Thing->eventSectorChanged(crunch)) {
          // doesn't fit, keep checking (crush other things)
          // k8: no need to check flags, VC code does this for us
          /*if (!(n->Thing->EntityFlags&VEntity::EF_NoBlockmap))*/
          {
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
  /*
  if (sector->SectorFlags&sector_t::SF_ExtrafloorSource) {
    for (int i = 0; i < NumSectors; ++i) {
      sector_t *sec2 = &Sectors[i];
      if ((sec2->SectorFlags&sector_t::SF_HasExtrafloors) != 0 && sec2 != sector) {
        for (sec_region_t *reg = sec2->botregion; reg; reg = reg->next) {
          if (reg->efloor.splane == &sector->floor || reg->eceiling.splane == &sector->ceiling) {
            ret |= ChangeSectorInternal(sec2, crunch);
            if (ret) csTouched[secnum] |= 0x80000000U;
            break;
          }
        }
      }
    }
  }
  */

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
