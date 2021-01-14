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
//**  Copyright (C) 2018-2021 Ketmar Dark
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
#include "../gamedefs.h"

// keep 1/8 unit away to avoid various numeric issues
//#define CD_CLIP_EPSILON  (0.125f)
// keep 1/16 unit away to avoid various numeric issues
#define CD_CLIP_EPSILON  (0.0625f)


//==========================================================================
//
//  SweepLinedefAABB
//
//  returns collision time, -1 if started inside, exactly 1 if no collision
//  in both such cases, outputs are undefined, as we have no hit plane
//  the moving thing is AABB
//  returns contact point in `contactPoint`
//  actually, `contactPoint` has little sense for non-point hits, and is
//  somewhat arbitrary
//  `hitplanenum` indicates which line side we hit:
//  front (0), back(1), or caps; can be -1
//
//==========================================================================
float VLevel::SweepLinedefAABB (const line_t *ld, TVec vstart, TVec vend, TVec bmin, TVec bmax,
                                TPlane *hitPlane, TVec *contactPoint, CD_HitType *hitType, int *hitplanenum)
{
  if (hitType) *hitType = CD_HT_None;
  if (hitplanenum) *hitplanenum = -1;
  if (!ld) return -1.0f;

  float ifrac = -1.0f;
  float ofrac = +1.0f;

  bool startsOut = false;
  //bool endsOut = false;
  int phit = -1;
  const unsigned pcount = (unsigned)ld->cdPlanesCount;

  for (unsigned pidx = 0; pidx < pcount; ++pidx) {
    const TPlane *plane = &ld->cdPlanesArray[pidx];
    // box
    // line plane normal z is always zero, so don't bother checking it
    const TVec offset = TVec((plane->normal.x < 0.0f ? bmax.x : bmin.x), (plane->normal.y < 0.0f ? bmax.y : bmin.y), /*(plane->normal.z < 0.0f ? bmax.z : bmin.z)*/bmin.z);
    // adjust the plane distance apropriately for mins/maxs
    const float dist = plane->dist-DotProduct(offset, plane->normal);
    const float idist = DotProduct(vstart, plane->normal)-dist;
    const float odist = DotProduct(vend, plane->normal)-dist;

    if (idist <= 0.0f && odist <= 0.0f) continue; // doesn't cross this plane, don't bother

    if (idist > 0.0f) {
      startsOut = true;
      // if completely in front of face, no intersection with the entire brush
      if (odist >= CD_CLIP_EPSILON || odist >= idist) return 1.0f;
    }
    //if (odist > 0.0f) endsOut = true;

    // crosses plane
    if (idist > odist) {
      // line is entering into the brush
      const float fr = fmax(0.0f, (idist-CD_CLIP_EPSILON)/(idist-odist));
      if (fr > ifrac) {
        ifrac = fr;
        phit = (int)pidx;
      }
    } else {
      // line is leaving the brush
      const float fr = fmin(1.0f, (idist+CD_CLIP_EPSILON)/(idist-odist));
      if (fr < ofrac) ofrac = fr;
    }
  }

  // all planes have been checked, and the trace was not completely outside the brush
  if (!startsOut) {
    // original point was inside brush
    return -1.0f;
  }

  if (ifrac > -1.0f && ifrac < ofrac) {
    // might be exact hit
    ifrac = clampval(ifrac, 0.0f, 1.0f); // just in case
    if (/*ifrac == 0.0f ||*/ ifrac == 1.0f) return ifrac; // just in case
    if (hitplanenum) *hitplanenum = phit;
    if (hitPlane || contactPoint || hitType) {
      vassert(phit >= 0);
      const TPlane *hpl = &ld->cdPlanesArray[(unsigned)phit];
      if (hitPlane) *hitPlane = *hpl;
      if (contactPoint || hitType) {
        CD_HitType httmp = CD_HT_None;
        if (!hitType) hitType = &httmp;
        // check what kind of hit this is
        if (phit > 1 && !hpl->normal.y) {
          // left or right side of the box
          *hitType = (hpl->normal.x < 0.0f ? CD_HT_Right : CD_HT_Left);
          if (contactPoint) {
            *contactPoint =
              (ld->v1->x < ld->v2->x ?
                (*hitType == CD_HT_Right ? *ld->v1 : *ld->v2) :
                (*hitType == CD_HT_Right ? *ld->v2 : *ld->v1))-(vend-vstart).normalised()*CD_CLIP_EPSILON;
          }
        } else if (phit > 1 && !hpl->normal.x) {
          // top or down side of the box
          *hitType = (hpl->normal.y < 0.0f ? CD_HT_Bottom : CD_HT_Top);
          if (contactPoint) {
            *contactPoint =
              (ld->v1->y < ld->v2->y ?
                (*hitType == CD_HT_Bottom ? *ld->v1 : *ld->v2) :
                (*hitType == CD_HT_Bottom ? *ld->v2 : *ld->v1))-(vend-vstart).normalised()*CD_CLIP_EPSILON;
          }
        } else {
          // point hit
          *hitType = CD_HT_Point;
          if (contactPoint) {
            //*contactPoint = TVec((hpl->normal.x < 0.0f ? bmax.x : hpl->normal.x > 0.0f ? bmin.x : 0.0f), (hpl->normal.y < 0.0f ? bmax.y : hpl->normal.y > 0.0f ? bmin.y : 0.0f), bmin.z);
            if (pcount == 4 && ld->v1->x == ld->v2->x && ld->v1->y == ld->v2->y) {
              // point
              *contactPoint = (*ld->v1)-(vend-vstart).normalised()*CD_CLIP_EPSILON;
            } else {
              bool fixX = false, fixY = false;
              if (!hpl->normal.x) {
                // horizontal line
                *contactPoint = TVec(0.0f, (hpl->normal.y < 0.0f ? bmax.y : hpl->normal.y > 0.0f ? bmin.y : 0.0f), bmin.z);
                fixX = true;
              } else if (!hpl->normal.y) {
                // vertical line
                *contactPoint = TVec((hpl->normal.x < 0.0f ? bmax.x : hpl->normal.x > 0.0f ? bmin.x : 0.0f), 0.0f, bmin.z);
                fixY = true;
              } else {
                *contactPoint = TVec((hpl->normal.x < 0.0f ? bmax.x : bmin.x), (hpl->normal.y < 0.0f ? bmax.y : bmin.y), bmin.z);
              }
              *contactPoint += vstart+(vend-vstart)*ifrac;
              if (fixX) {
                if (ld->v1->x < ld->v2->x) {
                  contactPoint->x = clampval(contactPoint->x, ld->v1->x, ld->v2->x);
                } else {
                  contactPoint->x = clampval(contactPoint->x, ld->v2->x, ld->v1->x);
                }
              } else if (fixY) {
                if (ld->v1->y < ld->v2->y) {
                  contactPoint->y = clampval(contactPoint->y, ld->v1->y, ld->v2->y);
                } else {
                  contactPoint->y = clampval(contactPoint->y, ld->v2->y, ld->v1->y);
                }
              }
            }
          }
        }
      }
    }
    return ifrac;
  }

  return 1.0f;
}


//==========================================================================
//
//  CheckPlanePass
//
//  WARNING: `currhit` should not be the same as `lineend`!
//
//  returns `true` if plane wasn't hit
//
//==========================================================================
bool VLevel::CheckPlanePass (const TSecPlaneRef &plane, const TVec &linestart, const TVec &lineend, TVec &currhit, bool &isSky) {
  const float d1 = plane.PointDistance(linestart);
  if (d1 < 0.0f) return true; // don't shoot back side

  const float d2 = plane.PointDistance(lineend);
  if (d2 >= 0.0f) return true; // didn't hit plane

  //if (d2 > 0.0f) return true; // didn't hit plane (was >=)
  //if (fabsf(d2-d1) < 0.0001f) return true; // too close to zero

  //frac = d1/(d1-d2); // [0..1], from start

  currhit = lineend;
  // sky?
  if (plane.splane->pic == skyflatnum) {
    // don't shoot the sky!
    isSky = true;
  } else {
    isSky = false;
    currhit -= (lineend-linestart)*d2/(d2-d1);
  }

  // don't go any farther
  return false;
}


#define UPDATE_PLANE_HIT(plane_)  do { \
  if (!CheckPlanePass((plane_), linestart, lineend, currhit, isSky)) { \
    const float dist = (currhit-linestart).lengthSquared(); \
    if (!wasHit || dist < besthdist) { \
      besthit = currhit; \
      bestIsSky = isSky; \
      besthdist = dist; \
      bestNormal = (plane_).GetNormal(); \
      if (outHitPlane) bestHitPlane = (plane_).GetPlane(); \
    } \
    wasHit = true; \
  } \
} while (0)


//==========================================================================
//
//  VLevel::CheckPassPlanes
//
//  checks all sector regions, returns `false` if any region plane was hit
//  sets `outXXX` arguments on hit (and only on hit!)
//  if `checkSectorBounds` is false, skip checking sector bounds
//  (and the first sector region)
//
//  any `outXXX` can be `nullptr`
//
//  returns `true` if no hit was detected
//
//==========================================================================
bool VLevel::CheckPassPlanes (sector_t *sector, TVec linestart, TVec lineend, unsigned flagmask,
                              TVec *outHitPoint, TVec *outHitNormal, bool *outIsSky, TPlane *outHitPlane)
{
  if (!sector) return true;

  TVec besthit = lineend;
  TVec bestNormal(0.0f, 0.0f, 0.0f);
  bool bestIsSky = false;
  TVec currhit(0.0f, 0.0f, 0.0f);
  bool wasHit = false;
  #ifdef INFINITY
  float besthdist = INFINITY;
  #else
  float besthdist = 9999999.0f;
  #endif
  bool isSky = false;
  TPlane bestHitPlane;

  const bool checkFakeFloors = !(flagmask&SPF_IGNORE_FAKE_FLOORS);
  const bool checkSectorBounds = !(flagmask&SPF_IGNORE_BASE_REGION);
  flagmask &= SPF_FLAG_MASK;

  if (checkSectorBounds) {
    // make fake floors and ceilings block view
    TSecPlaneRef bfloor, bceil;
    /*
    sector_t *hs = sector->heightsec;
    if (!hs) hs = sector;
    bfloor.set(&hs->floor, false);
    bceil.set(&hs->ceiling, false);
    // check sector floor
    UPDATE_PLANE_HIT(bfloor);
    // check sector ceiling
    UPDATE_PLANE_HIT(bceil);
    */
    if (checkFakeFloors) {
      sector_t *hs = sector->heightsec;
      if (hs) {
        bfloor.set(&hs->floor, false);
        bceil.set(&hs->ceiling, false);
        // check sector floor
        UPDATE_PLANE_HIT(bfloor);
        // check sector ceiling
        UPDATE_PLANE_HIT(bceil);
      }
    }
    bfloor.set(&sector->floor, false);
    bceil.set(&sector->ceiling, false);
    // check sector floor
    UPDATE_PLANE_HIT(bfloor);
    // check sector ceiling
    UPDATE_PLANE_HIT(bceil);
  }

  for (sec_region_t *reg = sector->eregions->next; reg; reg = reg->next) {
    if (reg->regflags&sec_region_t::RF_OnlyVisual) continue;
    if ((reg->efloor.splane->flags&flagmask) == 0) {
      UPDATE_PLANE_HIT(reg->efloor);
    }
    if ((reg->eceiling.splane->flags&flagmask) == 0) {
      UPDATE_PLANE_HIT(reg->eceiling);
    }
  }

  if (wasHit) {
    // hit floor or ceiling
    if (outHitPoint) *outHitPoint = besthit;
    if (outHitNormal) *outHitNormal = bestNormal;
    if (outIsSky) *outIsSky = bestIsSky;
    if (outHitPlane) *outHitPlane = bestHitPlane;
    return false;
  } else {
    return true;
  }
}


//==========================================================================
//
//  VLevel::CalcLineCDPlanes
//
//  create collision detection planes (line/reverse line plane, and caps)
//
//==========================================================================
void VLevel::CalcLineCDPlanes (line_t *line) {
  if (line->v1->y == line->v2->y) {
    // either horizontal line, or a point
    if (line->v1->x == line->v2->x) {
      // a point
      line->cdPlanesCount = 4;
      // point, create four axial planes to represent it as a box
      line->cdPlanesArray[0].normal = TVec( 0, -1, 0); line->cdPlanesArray[0].dist = -line->v1->y; // top
      line->cdPlanesArray[1].normal = TVec( 0,  1, 0); line->cdPlanesArray[1].dist = line->v1->y; // bottom
      line->cdPlanesArray[2].normal = TVec(-1,  0, 0); line->cdPlanesArray[2].dist = -line->v1->x; // left
      line->cdPlanesArray[3].normal = TVec( 1,  0, 0); line->cdPlanesArray[3].dist = line->v1->x; // right
    } else {
      // a horizontal line
      line->cdPlanesCount = 4;
      int botidx = (line->v1->x < line->v2->x);
      line->cdPlanesArray[1-botidx].normal = TVec( 0, -1, 0); line->cdPlanesArray[1-botidx].dist = -line->v1->y; // top
      line->cdPlanesArray[botidx].normal = TVec( 0,  1, 0); line->cdPlanesArray[botidx].dist = line->v1->y; // bottom
      // add left and right bevels
      line->cdPlanesArray[2].normal = TVec(-1,  0, 0); line->cdPlanesArray[2].dist = -min2(line->v1->x, line->v2->x); // left
      line->cdPlanesArray[3].normal = TVec( 1,  0, 0); line->cdPlanesArray[3].dist = max2(line->v1->x, line->v2->x); // right
    }
  } else if (line->v1->x == line->v2->x) {
    // a vertical line
    line->cdPlanesCount = 4;
    int rightidx = (line->v1->y > line->v2->y);
    line->cdPlanesArray[1-rightidx].normal = TVec(-1,  0, 0); line->cdPlanesArray[1-rightidx].dist = -line->v1->x; // left
    line->cdPlanesArray[rightidx].normal = TVec( 1,  0, 0); line->cdPlanesArray[rightidx].dist = line->v1->x; // right
    // add top and bottom bevels
    line->cdPlanesArray[2].normal = TVec( 0, -1, 0); line->cdPlanesArray[2].dist = -min2(line->v1->y, line->v2->y); // top
    line->cdPlanesArray[3].normal = TVec( 0,  1, 0); line->cdPlanesArray[3].dist = max2(line->v1->y, line->v2->y); // bottom
  } else {
    // ok, not an ortho-axis line, create line planes the old way
    line->cdPlanesCount = 6;
    // two line planes
    line->cdPlanesArray[0].normal = line->normal;
    line->cdPlanesArray[0].dist = line->dist;
    line->cdPlanesArray[1].normal = -line->cdPlanesArray[0].normal;
    line->cdPlanesArray[1].dist = -line->cdPlanesArray[0].dist;
    // caps
    line->cdPlanesArray[2].normal = TVec(-1,  0, 0); line->cdPlanesArray[2].dist = -min2(line->v1->x, line->v2->x); // left
    line->cdPlanesArray[3].normal = TVec( 1,  0, 0); line->cdPlanesArray[3].dist = max2(line->v1->x, line->v2->x); // right
    line->cdPlanesArray[4].normal = TVec( 0, -1, 0); line->cdPlanesArray[4].dist = -min2(line->v1->y, line->v2->y); // top
    line->cdPlanesArray[5].normal = TVec( 0,  1, 0); line->cdPlanesArray[5].dist = max2(line->v1->y, line->v2->y); // bottom
  }
  line->cdPlanes = &line->cdPlanesArray[0];
}


//native static final float CD_SweepLinedefAABB (const line_t *ld, TVec vstart, TVec vend, TVec bmin, TVec bmax,
//                                               optional out TPlane hitPlane, optional out TVec contactPoint,
//                                               optional out CD_HitType hitType, optional out int hitplanenum);
IMPLEMENT_FUNCTION(VLevel, CD_SweepLinedefAABB) {
  line_t *ld;
  TVec vstart, vend, bmin, bmax;
  VOptParamPtr<TPlane> hitPlane;
  VOptParamPtr<TVec> contactPoint;
  VOptParamPtr<CD_HitType> hitType;
  VOptParamPtr<vint32> hitplanenum;
  vobjGetParamSelf(ld, vstart, vend, bmin, bmax, hitPlane, contactPoint, hitType, hitplanenum);
  if (!hitPlane.specified) hitPlane = nullptr;
  if (!contactPoint.specified) contactPoint = nullptr;
  if (!hitType.specified) hitType = nullptr;
  if (!hitplanenum.specified) hitplanenum = nullptr;
  RET_FLOAT(SweepLinedefAABB(ld, vstart, vend, bmin, bmax, hitPlane, contactPoint, hitType, hitplanenum));
}
