//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš, dj_jl
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
#include "r_light_adv.h"


static VCvarF r_light_filter_static_coeff("r_light_filter_static_coeff", "0.2", "How close static lights should be to be filtered out?\n(0.1-0.3 is usually ok).", CVAR_Archive);
static VCvarB r_allow_static_light_filter("r_allow_static_light_filter", true, "Allow filtering of static lights?", CVAR_Archive);
static VCvarI r_static_light_filter_mode("r_static_light_filter_mode", "0", "Filter only decorations(0), or all lights(1)?", CVAR_Archive);

static VCvarB r_shadowvol_optimise_flats("r_shadowvol_optimise_flats", false, "Drop some floors/ceilings that can't possibly cast shadow?", CVAR_Archive);
#ifdef VV_CHECK_1S_CAST_SHADOW
static VCvarB r_shadowvol_optimise_lines_1s("r_shadowvol_optimise_lines_1s", true, "Drop some 1s walls that can't possibly cast shadow? (glitchy)");
#endif


//==========================================================================
//
//  VRenderLevelShadowVolume::RefilterStaticLights
//
//==========================================================================
void VRenderLevelShadowVolume::RefilterStaticLights () {
  staticLightsFiltered = true;

  float coeff = r_light_filter_static_coeff;

  int llen = Lights.length();
  int actlights = 0;
  for (int currlidx = 0; currlidx < llen; ++currlidx) {
    light_t &cl = Lights[currlidx];
    if (coeff > 0) {
      cl.active = (cl.radius > 6); // arbitrary limit
    } else {
      cl.active = true;
    }
    if (cl.active) ++actlights;
  }
  if (actlights < 6) return; // arbitrary limit

  if (!r_allow_static_light_filter) return; // no filtering
  if (coeff <= 0) return; // no filtering
  if (coeff > 8) coeff = 8;

  const bool onlyDecor = (r_static_light_filter_mode.asInt() == 0);

  for (int currlidx = 0; currlidx < llen; ++currlidx) {
    light_t &cl = Lights[currlidx];
    if (!cl.active) continue; // already filtered out
    if (onlyDecor && !cl.ownerUId) continue;
    // remove nearby lights with radius less than ours (or ourself if we'll hit bigger light)
    float radsq = (cl.radius*cl.radius)*coeff;
    for (int nlidx = currlidx+1; nlidx < llen; ++nlidx) {
      light_t &nl = Lights[nlidx];
      if (!nl.active) continue; // already filtered out
      if (onlyDecor && !nl.ownerUId) continue;
      const float distsq = length2DSquared(cl.origin-nl.origin);
      if (distsq >= radsq) continue;

      // check potential visibility
      /*
      subsector_t *sub = Level->PointInSubsector(nl.origin);
      const vuint8 *dyn_facevis = Level->LeafPVS(sub);
      if (!(dyn_facevis[nl.leafnum>>3]&(1<<(nl.leafnum&7)))) continue;
      */

      // if we cannot trace a line between two lights, they are prolly divided by a wall or a flat
      linetrace_t Trace;
      if (!Level->TraceLine(Trace, nl.origin, cl.origin, SPF_NOBLOCKSIGHT)) continue;

      if (nl.radius <= cl.radius) {
        // deactivate nl
        nl.active = false;
      } else /*if (nl.radius > cl.radius)*/ {
        // deactivate cl
        cl.active = false;
        // there is no sense to continue
        break;
      }
    }
  }

  actlights = 0;
  for (int currlidx = 0; currlidx < llen; ++currlidx) {
    light_t &cl = Lights[currlidx];
    if (cl.active) {
      ++actlights;
    } else {
      //if (cl.owner) GCon->Logf(NAME_Debug, "ADVR: filtered static light from `%s`; org=(%g,%g,%g); radius=%g", cl.owner->GetClass()->GetName(), cl.origin.x, cl.origin.y, cl.origin.z, cl.radius);
    }
    CalcStaticLightTouchingSubs(currlidx, cl);
  }

  if (actlights < llen) GCon->Logf("filtered %d static lights out of %d (%d left)", llen-actlights, llen, actlights);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::BuildLightMap
//
//==========================================================================
void VRenderLevelShadowVolume::BuildLightMap (surface_t *surf) {
}


//==========================================================================
//
//  VRenderLevelShadowVolume::IsShadowMapRenderer
//
//==========================================================================
bool VRenderLevelShadowVolume::IsShadowMapRenderer () const noexcept {
  return (r_shadowmaps.asBool() && Drawer && Drawer->CanRenderShadowMaps());
}


#ifdef VV_CHECK_1S_CAST_SHADOW
//==========================================================================
//
//  VRenderLevelShadowVolume::CheckCan1SCastShadow
//
//==========================================================================
bool VRenderLevelShadowVolume::CheckCan1SCastShadow (line_t *line) {
  if (!r_shadowvol_optimise_lines_1s) return true;
  const int lidx = (int)(ptrdiff_t)(line-&Level->Lines[0]);
  Line1SShadowInfo &nfo = flineCheck[lidx];
  if (nfo.frametag != fsecCounter) {
    nfo.frametag = fsecCounter; // mark as processed
    // check if all adjacent walls to this one are in front of it
    // as this is one-sided wall, we can assume that if everything is in front,
    // there is no passage that we can cast shadow into

    // check lines at v1
    line_t **llist = line->v1lines;
    for (int f = line->v1linesCount; f--; ++llist) {
      line_t *l2 = *llist;
      if ((l2->flags&ML_TWOSIDED) != 0) return (nfo.canShadow = true); // has two-sided line as a neighbour, oops
      TVec v = (*l2->v1 == *line->v1 ? *l2->v2 : *l2->v1);
      const int side = line->PointOnSide2(v);
      if (side == 1) return (nfo.canShadow = true); // this point is behind, can cast a shadow
      // perform recursive check for coplanar lines
      if (side == 2) {
        // check if the light can touch it
        if (fabsf(l2->PointDistance(CurrLightPos)) < CurrLightRadius) {
          if (CheckCan1SCastShadow(l2)) return (nfo.canShadow = true); // there is a turn, oops
        }
      }
    }

    // check lines at v2
    llist = line->v2lines;
    for (int f = line->v2linesCount; f--; ++llist) {
      line_t *l2 = *llist;
      if ((l2->flags&ML_TWOSIDED) != 0) return (nfo.canShadow = true); // has two-sided line as a neighbour, oops
      TVec v = (*l2->v1 == *line->v2 ? *l2->v2 : *l2->v1);
      const int side = line->PointOnSide2(v);
      if (side == 1) return (nfo.canShadow = true); // this point is behind, can cast a shadow
      // perform recursive check for coplanar lines
      if (side == 2) {
        // check if the light can touch it
        if (fabsf(l2->PointDistance(CurrLightPos)) < CurrLightRadius) {
          if (CheckCan1SCastShadow(l2)) return (nfo.canShadow = true); // there is a turn, oops
        }
      }
    }

    // no vertices are on the back side, shadow casting is impossible
    nfo.canShadow = false;
  }
  return nfo.canShadow;
}
#endif


//==========================================================================
//
//  VRenderLevelShadowVolume::CheckShadowingFlats
//
//  THIS IS OUDATED!
//  note that we can throw away main floors and ceilings (i.e. for the base region),
//  but only if this subsector either doesn't have any lines (i.e. consists purely of minisegs),
//  nope: not any such subregion; just avoid checking neighbouring sectors if shared line
//        cannot be touched by the light
//  or:
//    drop floor if all neighbour floors are higher or equal (we cannot cast any shadow to them),
//    drop ceiling if all neighbour ceilings are lower or equal (we cannot cast any shadow to them)
//
//==========================================================================
unsigned VRenderLevelShadowVolume::CheckShadowingFlats (subsector_t *sub) {
  /*
     this has the bug:
     we will optimise out flats that touches the borders. like this:

       **********
       **********
       **********
       ##########
       #........#
       #........#
       ##########

     here, "*" and "#" are on the same height, and "." is a deep hole.
     with optimisations turned on, "*" will be optimised away, and only
     "#" will cast a shadow into a hole, creating light bleed.
     to avoid this, we should do recursive check to find possible holes,
     which is quite expensive (and will prolly hit more corner cases).
     so i turned it off by default.
  */
  if (!sub || !sub->sector) return (FlatSectorShadowInfo::NoFloor|FlatSectorShadowInfo::NoCeiling);
  if (!r_shadowvol_optimise_flats.asBool()) return 0; // no reason to do anything, because surface collector already did it for us
  if (r_shadowmaps.asBool() && Drawer->CanRenderShadowMaps()) return 0;
  //if (floorz > ceilingz) return 0;
  sector_t *sector = sub->sector; // our main sector
  int sidx = (int)(ptrdiff_t)(sector-&Level->Sectors[0]);
  FlatSectorShadowInfo &nfo = fsecCheck[sidx];
  // check if we need to calculate info
  if (nfo.frametag != fsecCounter) {
    // yeah, calculate it
    nfo.frametag = fsecCounter; // mark as updated
    unsigned allowed = 0u; // set bits means "allowed"
    /*
    if (CurrLightPos.z > sector->floor.minz && CurrLightPos.z-CurrLightRadius < sector->floor.maxz) allowed |= FlatSectorShadowInfo::NoFloor; // too high or too low
    if (CurrLightPos.z < sector->ceiling.maxz && CurrLightPos.z+CurrLightRadius > sector->ceiling.minz) allowed |= FlatSectorShadowInfo::NoCeiling; // too high or too low
    */
    float dist = sector->floor.PointDistance(CurrLightPos);
    if (dist > 0.0f && dist < CurrLightRadius) allowed |= FlatSectorShadowInfo::NoFloor; // light can touch
    dist = sector->ceiling.PointDistance(CurrLightPos);
    if (dist > 0.0f && dist < CurrLightRadius) allowed |= FlatSectorShadowInfo::NoCeiling; // light can touch
    //allowed = (FlatSectorShadowInfo::NoFloor|FlatSectorShadowInfo::NoCeiling);
    /* no reason to do this, because surface collector already did this for us
    if (!r_shadowvol_optimise_flats) {
      // no checks, return inverted `allowed`
      return (nfo.renderFlag = allowed^(FlatSectorShadowInfo::NoFloor|FlatSectorShadowInfo::NoCeiling));
    }
    */
    if (!allowed) {
      // nothing is allowed, oops
      return (nfo.renderFlag = (FlatSectorShadowInfo::NoFloor|FlatSectorShadowInfo::NoCeiling));
    }
    // check all 2s walls of this sector
    // note that polyobjects are not interested, because they're always as high as their sector
    // TODO: optimise this by checking walls only once?
    // calculate blockmap coordinates
    bool checkFloor = (allowed&FlatSectorShadowInfo::NoFloor);
    bool checkCeiling = (allowed&FlatSectorShadowInfo::NoCeiling);
    bool renderFloor = false;
    bool renderCeiling = false;
    // check blockmap
    const int lpx = (int)CurrLightPos.x;
    const int lpy = (int)CurrLightPos.y;
    const int lrad = (int)CurrLightRadius;
    const int bmapx0 = (int)Level->BlockMapOrgX;
    const int bmapy0 = (int)Level->BlockMapOrgY;
    const int bmapw = (int)Level->BlockMapWidth;
    const int bmaph = (int)Level->BlockMapHeight;
    int bmx0 = (lpx-lrad-bmapx0)/MAPBLOCKUNITS;
    int bmy0 = (lpy-lrad-bmapy0)/MAPBLOCKUNITS;
    int bmx1 = (lpx+lrad-bmapx0)/MAPBLOCKUNITS;
    int bmy1 = (lpy+lrad-bmapy0)/MAPBLOCKUNITS;
    // check if we're inside a blockmap
    if (bmx1 < 0 || bmy1 < 0 || bmx0 >= bmapw || bmy0 >= bmaph) {
      // nothing is allowed, oops
      return (nfo.renderFlag = (FlatSectorShadowInfo::NoFloor|FlatSectorShadowInfo::NoCeiling));
    }
    // at least partially inside, perform checks
    if (bmx0 < 0) bmx0 = 0;
    if (bmy0 < 0) bmy0 = 0;
    if (bmx1 >= bmapw) bmx1 = bmapw-1;
    if (bmy1 >= bmaph) bmy1 = bmaph-1;
    const unsigned secSeenTag = fsecSeenSectorsGen();
    for (int by = bmy0; by <= bmy1; ++by) {
      for (int bx = bmx0; bx <= bmx1; ++bx) {
        int offset = by*bmapw+bx;
        offset = *(Level->BlockMap+offset);
        for (const vint32 *list = Level->BlockMapLump+offset+1; *list != -1; ++list) {
          line_t *l = &Level->Lines[*list];
          // ignore one-sided lines
          if ((l->flags&ML_TWOSIDED) == 0) continue;
          // ignore lines that don't touch our sector
          if (l->frontsector != sector && l->backsector != sector) continue;
          // get other sector
          sector_t *other = (l->frontsector == sector ? l->backsector : l->frontsector);
          if (!other) continue; // just in case
          const int othersecidx = (int)(ptrdiff_t)(other-&Level->Sectors[0]);
          if (fsecSeenSectors[othersecidx] == secSeenTag) continue; // already checked
          // ignore lines that cannot be lit
          if (fabsf(l->PointDistance(CurrLightPos)) >= CurrLightRadius) continue;
          // mark as checked
          fsecSeenSectors[othersecidx] = secSeenTag;
          // check other sector floor
          if (checkFloor && other->floor.minz < sector->floor.maxz) {
            // other sector floor is lower, cannot drop our
            renderFloor = true;
            if (renderCeiling || !checkCeiling) goto lhackdone; // no need to perform any more checks
            checkFloor = false;
          }
          // check other sector ceiling
          if (checkCeiling && other->ceiling.maxz > sector->ceiling.minz) {
            // other sector ceiling is higher, cannot drop our
            renderCeiling = true;
            if (renderFloor || !checkFloor) goto lhackdone; // no need to perform any more checks
            checkCeiling = false;
          }
        }
      }
    }
    lhackdone:
    // set flag
    if ((allowed&FlatSectorShadowInfo::NoFloor) == 0) renderFloor = false;
    if ((allowed&FlatSectorShadowInfo::NoCeiling) == 0) renderCeiling = false;
    nfo.renderFlag =
      (renderFloor ? 0u : FlatSectorShadowInfo::NoFloor)|
      (renderCeiling ? 0u : FlatSectorShadowInfo::NoCeiling);
  }
  return nfo.renderFlag;
}
