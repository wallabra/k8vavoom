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
//**  visibility by BSP
//**
//**************************************************************************
#include "../gamedefs.h"
#include "r_local.h"


extern VCvarB dbg_vischeck_time;


//==========================================================================
//
//  Are3DAnd2DBBoxesOverlap
//
//==========================================================================
static VVA_OKUNUSED VVA_CHECKRESULT inline bool Are3DAnd2DBBoxesOverlap (const float bbox0[6], const float bbox1[4]) {
  return !(
    bbox1[2+0] < bbox0[0+0] || bbox1[2+1] < bbox0[0+1] ||
    bbox1[0+0] > bbox0[3+0] || bbox1[0+1] > bbox0[3+1]
  );
}


//==========================================================================
//
//  VRenderLevelShared::CheckBSPVisibilityBoxSub
//
//==========================================================================
bool VRenderLevelShared::CheckBSPVisibilityBoxSub (int bspnum, const float *bbox) noexcept {
  if (bspnum == -1) return true;
  // found a subsector?
  if (BSPIDX_IS_NON_LEAF(bspnum)) {
    // nope
    const node_t *bsp = &Level->Nodes[bspnum];
    #ifndef VAVOOM_USE_SIMPLE_BSP_BBOX_VIS_CHECK
    // k8: this seems to be marginally slower than simple bbox check
    // k8: checking bbox before recurse into one node speeds it up
    // k8: checking bbox in two-node recursion doesn't do anything sensible (obviously)
    // decide which side the light is on
    const float dist = bsp->PointDistance(CurrLightPos);
    if (dist >= CurrLightRadius) {
      // light is completely on front side
      if (!Are3DBBoxesOverlapIn2D(bsp->bbox[0], bbox)) return false;
      return CheckBSPVisibilityBoxSub(bsp->children[0], bbox);
    } else if (dist <= -CurrLightRadius) {
      // light is completely on back side
      if (!Are3DBBoxesOverlapIn2D(bsp->bbox[1], bbox)) return false;
      return CheckBSPVisibilityBoxSub(bsp->children[1], bbox);
    } else {
      // it doesn't really matter which subspace we'll check first, but why not?
      unsigned side = (unsigned)(dist <= 0.0f);
      // recursively divide front space
      if (CheckBSPVisibilityBoxSub(bsp->children[side], bbox)) return true;
      // recursively divide back space
      side ^= 1;
      return CheckBSPVisibilityBoxSub(bsp->children[side], bbox);
    }
    #else
    // this is slower
    for (unsigned side = 0; side < 2; ++side) {
      if (!Are3DBBoxesOverlapIn2D(bsp->bbox[side], bbox)) continue;
      if (CheckBSPVisibilityBoxSub(bsp->children[side], bbox)) return true;
    }
    #endif
  } else {
    // check subsector
    const unsigned subidx = BSPIDX_LEAF_SUBSECTOR(bspnum);
    if (BspVis[subidx>>3]&(1<<(subidx&7))) {
      // no, this check is wrong
      /*if (Are3DAnd2DBBoxesOverlap(bbox, Level->Subsectors[subidx].bbox2d))*/
      {
        if (dbg_dlight_vis_check_messages) GCon->Logf(NAME_Debug, "***HIT VISIBLE SUBSECTOR #%u", subidx);
        return true;
      }
    }
  }
  return false;
}


//==========================================================================
//
//  VRenderLevelShared::CheckBSPVisibilityBox
//
//==========================================================================
bool VRenderLevelShared::CheckBSPVisibilityBox (const TVec &org, float radius, const subsector_t *sub) noexcept {
  if (!Level) return false; // just in case
  if (r_vis_check_flood) return CheckBSPFloodVisibility(org, radius, sub);

  if (sub) {
    const unsigned subidx = (unsigned)(ptrdiff_t)(sub-Level->Subsectors);
    // rendered means "visible"
    if (BspVis[subidx>>3]&(1<<(subidx&7))) return true;
  }

  // create light bounding box
  float lbbox[6] = {
    org.x-radius,
    org.y-radius,
    0, // doesn't matter
    org.x+radius,
    org.y+radius,
    0, // doesn't matter
  };

  #ifndef VAVOOM_USE_SIMPLE_BSP_BBOX_VIS_CHECK
  CurrLightPos = org;
  CurrLightRadius = radius;
  #endif

  if (!dbg_vischeck_time) {
    return CheckBSPVisibilityBoxSub(Level->NumNodes-1, lbbox);
  } else {
    const double stt = -Sys_Time_CPU();
    bool res = CheckBSPVisibilityBoxSub(Level->NumNodes-1, lbbox);
    dbgCheckVisTime += stt+Sys_Time_CPU();
    return res;
  }
}
