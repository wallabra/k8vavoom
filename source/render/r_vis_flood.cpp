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
//**  visibility by floodfill
//**
//**************************************************************************
#include "../gamedefs.h"
#include "r_local.h"


static VCvarB r_lightflood_check_plane_angles("r_lightflood_check_plane_angles", true, "Check seg planes angles in light floodfill?", CVAR_Archive);

extern VCvarB dbg_vischeck_time;


//==========================================================================
//
//  isCircleTouchingLine
//
//==========================================================================
static inline bool isCircleTouchingLine (const TVec &corg, const float radiusSq, const TVec &v0, const TVec &v1) noexcept {
  const TVec s0qp = corg-v0;
  if (s0qp.length2DSquared() <= radiusSq) return true;
  if ((corg-v1).length2DSquared() <= radiusSq) return true;
  const TVec s0s1 = v1-v0;
  const float a = s0s1.dot2D(s0s1);
  if (!a) return false; // if you haven't zero-length segments omit this, as it would save you 1 _mm_comineq_ss() instruction and 1 memory fetch
  const float b = s0s1.dot2D(s0qp);
  const float t = b/a; // length of projection of s0qp onto s0s1
  if (t >= 0.0f && t <= 1.0f) {
    const float c = s0qp.dot2D(s0qp);
    const float r2 = c-a*t*t;
    //print("a=%s; t=%s; r2=%s; rsq=%s", a, t, r2, radiusSq);
    return (r2 < radiusSq); // true if collides
  }
  return false;
}


//==========================================================================
//
//  VRenderLevelShared::NewBSPFloodVisibilityFrame
//
//==========================================================================
void VRenderLevelShared::NewBSPFloodVisibilityFrame () noexcept {
  if (bspVisRadius) {
    // bit 31 is used as "visible" mark
    if (++bspVisRadiusFrame >= 0x80000000u) {
      bspVisRadiusFrame = 1;
      memset(bspVisRadius, 0, sizeof(bspVisRadius[0])*Level->NumSubsectors);
    }
  } else {
    bspVisRadiusFrame = 0;
  }
  bspVisLastCheckRadius = -1.0f; // "unknown"
}


//==========================================================================
//
//  VRenderLevelShared::CheckBSPFloodVisibilitySub
//
//  `firsttravel` is used to reject invisible segs
//  it is set before first recursive call, and all segs whose planes are
//  angled with 190 or more relative to this first seg are rejected
//
//==========================================================================
bool VRenderLevelShared::CheckBSPFloodVisibilitySub (const TVec &org, const float radius, const subsector_t *currsub, const seg_t *firsttravel) noexcept {
  const unsigned csubidx = (unsigned)(ptrdiff_t)(currsub-Level->Subsectors);
  // rendered means "visible"
  if (BspVis[csubidx>>3]&(1<<(csubidx&7))) {
    bspVisRadius[csubidx].framecount = bspVisRadiusFrame|0x80000000u; // just in case
    return true;
  }
  // if we came into already visited subsector, abort flooding (and return failure)
  if ((bspVisRadius[csubidx].framecount&0x7fffffffu) == bspVisRadiusFrame) {
    //GCon->Logf(NAME_Debug, "   visited! %d (%u)", csubidx, bspVisRadius[csubidx].framecount>>31);
    return (bspVisRadius[csubidx].framecount >= 0x80000000u);
  }
  // recurse into neighbour subsectors
  bspVisRadius[csubidx].framecount = bspVisRadiusFrame; // mark as visited
  if (currsub->numlines == 0) return false;
  const float radiusSq = radius*radius;
  const seg_t *seg = &Level->Segs[currsub->firstline];
  for (int count = currsub->numlines; count--; ++seg) {
    // skip non-portals
    const line_t *ldef = seg->linedef;
    if (ldef) {
      // not a miniseg; check if linedef is passable
      if (!(ldef->flags&(ML_TWOSIDED|ML_3DMIDTEX))) continue; // solid line
      // check if we can touch midtex, 'cause why not?
      const sector_t *bsec = seg->backsector;
      if (!bsec) continue;
      if (org.z+radius <= bsec->floor.minz ||
          org.z-radius >= bsec->ceiling.maxz)
      {
        // cannot possibly leak through midtex, consider this wall solid
        continue;
      }
      // don't go through closed doors and raised lifts
      if (VViewClipper::IsSegAClosedSomething(Level, nullptr/*no frustum*/, seg, &org, &radius)) continue;
    } // minisegs are portals
    // we should have partner seg
    if (!seg->partner || seg->partner == seg || seg->partner->frontsub == currsub) continue;
    // check if this seg is touching our sphere
    {
      float distSq = DotProduct(org, seg->normal)-seg->dist;
      distSq *= distSq;
      if (distSq >= radiusSq) continue;
    }
    // precise check
    if (!isCircleTouchingLine(org, radiusSq, *seg->v1, *seg->v2)) continue;
    // check plane angles
    if (firsttravel && r_lightflood_check_plane_angles) {
      if (PlaneAngles2D(firsttravel, seg) >= 180.0f && PlaneAngles2DFlipTo(firsttravel, seg) >= 180.0f) continue;
    }
    // ok, it is touching, recurse
    if (CheckBSPFloodVisibilitySub(org, radius, seg->partner->frontsub, (firsttravel ? firsttravel : seg))) {
      //GCon->Logf("RECURSE HIT!");
      //GCon->Logf(NAME_Debug, "   RECURSE TRUE! %d", csubidx);
      bspVisRadius[csubidx].framecount |= 0x80000000u;
      return true;
    }
  }
  return false;
}


//==========================================================================
//
//  VRenderLevelShared::CheckBSPFloodVisibility
//
//==========================================================================
bool VRenderLevelShared::CheckBSPFloodVisibility (const TVec &org, float radius, const subsector_t *sub) noexcept {
  if (!Level) return false; // just in case
  if (!sub) {
    sub = Level->PointInSubsector(org);
    if (!sub) return false;
  }
  const unsigned subidx = (unsigned)(ptrdiff_t)(sub-Level->Subsectors);
  // check potential visibility
  /*
  if (hasPVS) {
    const vuint8 *dyn_facevis = Level->LeafPVS(sub);
    const unsigned leafnum = Level->PointInSubsector(l->origin)-Level->Subsectors;
    if (!(dyn_facevis[leafnum>>3]&(1<<(leafnum&7)))) continue;
  }
  */
/*
  // already checked?
  if (bspVisRadius[subidx].framecount == bspVisRadiusFrame) {
    if (bspVisRadius[subidx].radius <= radius) return !!bspVisRadius[subidx].vis;
  }
  // mark as "checked"
  bspVisRadius[subidx].framecount = bspVisRadiusFrame;
  bspVisRadius[subidx].radius = radius;
  // rendered means "visible"
  if (BspVis[subidx>>3]&(1<<(subidx&7))) {
    bspVisRadius[subidx].radius = 1e12; // big!
    bspVisRadius[subidx].vis = BSPVisInfo::VISIBLE;
    return true;
  }
*/
  // rendered means "visible"
  if (BspVis[subidx>>3]&(1<<(subidx&7))) return true;

  // use floodfill to determine (rough) potential visibility
  // nope, don't do it here, do it in scene renderer
  // this is so the checks from the same subsector won't do excess work
  // done in `PrepareWorldRender()`
  //NewBSPFloodVisibilityFrame();

  //GCon->Logf(NAME_Debug, "CheckBSPFloodVisibility(%u): subsector=%d; org=(%g,%g,%g); radius=%g", bspVisRadiusFrame, subidx, org.x, org.y, org.z, radius);

  if (!bspVisRadius) {
    bspVisRadiusFrame = 1;
    bspVisRadius = new BSPVisInfo[Level->NumSubsectors];
    memset(bspVisRadius, 0, sizeof(bspVisRadius[0])*Level->NumSubsectors);
    bspVisLastCheckRadius = radius;
  } else {
    bspVisLastCheckRadius = radius;
    NewBSPFloodVisibilityFrame();
  }

  if (!dbg_vischeck_time) {
    return CheckBSPFloodVisibilitySub(org, radius, sub, nullptr);
  } else {
    const double stt = -Sys_Time_CPU();
    bool res = CheckBSPFloodVisibilitySub(org, radius, sub, nullptr);
    dbgCheckVisTime += stt+Sys_Time_CPU();
    return res;
  }
}
