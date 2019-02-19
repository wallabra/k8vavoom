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
#include "r_local.h"

extern subsector_t *r_surf_sub;


//==========================================================================
//
//  VAdvancedRenderLevel::InitSurfs
//
//==========================================================================
void VAdvancedRenderLevel::InitSurfs (surface_t *surfs, texinfo_t *texinfo, TPlane *plane, subsector_t *sub) {
  guard(VAdvancedRenderLevel::InitSurfs);
  // it's always one surface
  if (surfs && plane) {
    surfs->texinfo = texinfo;
    surfs->plane = plane;
  }
  unguard;
}


//==========================================================================
//
//  VAdvancedRenderLevel::SubdivideFace
//
//==========================================================================
surface_t *VAdvancedRenderLevel::SubdivideFace(surface_t *f, const TVec&, const TVec *) {
  // advanced renderer can draw whole surface
  return f;
}


//==========================================================================
//
//  VAdvancedRenderLevel::SubdivideSeg
//
//==========================================================================
surface_t *VAdvancedRenderLevel::SubdivideSeg(surface_t *surf, const TVec &, const TVec *) {
  // advanced renderer can draw whole surface
  return surf;
}


//==========================================================================
//
//  VAdvancedRenderLevel::PreRender
//
//==========================================================================
void VAdvancedRenderLevel::PreRender () {
  guard(VAdvancedRenderLevel::PreRender);
  CreateWorldSurfaces();
  unguard;
}


//==========================================================================
//
//  VAdvancedRenderLevel::UpdateSubsector
//
//==========================================================================
void VAdvancedRenderLevel::UpdateSubsector (int num, float *bbox) {
  guard(VAdvancedRenderLevel::UpdateSubsector);
  r_surf_sub = &Level->Subsectors[num];

  if (!r_surf_sub->sector->linecount) return; // skip sectors containing original polyobjs

  if (!ViewClip.ClipCheckSubsector(r_surf_sub)) return;

  bbox[2] = r_surf_sub->sector->floor.minz;
  if (IsSky(&r_surf_sub->sector->ceiling)) {
    bbox[5] = skyheight;
  } else {
    bbox[5] = r_surf_sub->sector->ceiling.maxz;
  }

  UpdateSubRegion(r_surf_sub->regions, true);

  ViewClip.ClipAddSubsectorSegs(r_surf_sub);
  unguard;
}


//==========================================================================
//
//  VAdvancedRenderLevel::UpdateBSPNode
//
//==========================================================================
void VAdvancedRenderLevel::UpdateBSPNode (int bspnum, float *bbox) {
  guard(VAdvancedRenderLevel::UpdateBSPNode);
  if (ViewClip.ClipIsFull()) return;

  if (!ViewClip.ClipIsBBoxVisible(bbox)) return;

  if (bspnum == -1) {
    UpdateSubsector(0, bbox);
    return;
  }

  // found a subsector?
  if (!(bspnum&NF_SUBSECTOR)) {
    node_t *bsp = &Level->Nodes[bspnum];

    // decide which side the view point is on
    int side = bsp->PointOnSide(vieworg);

    UpdateBSPNode(bsp->children[side], bsp->bbox[side]);
    bbox[2] = MIN(bsp->bbox[0][2], bsp->bbox[1][2]);
    bbox[5] = MAX(bsp->bbox[0][5], bsp->bbox[1][5]);
    if (!ViewClip.ClipIsBBoxVisible(bsp->bbox[side^1])) return;
    UpdateBSPNode(bsp->children[side^1], bsp->bbox[side^1]);
    return;
  }

  UpdateSubsector(bspnum&(~NF_SUBSECTOR), bbox);
  unguard;
}


//==========================================================================
//
//  VAdvancedRenderLevel::UpdateWorld
//
//==========================================================================
void VAdvancedRenderLevel::UpdateWorld (const refdef_t *rd, const VViewClipper *Range) {
  guard(VAdvancedRenderLevel::UpdateWorld);
  float dummy_bbox[6] = { -99999, -99999, -99999, 99999, 99999, 99999 };

  ViewClip.ClearClipNodes(vieworg, Level);
  ViewClip.ClipInitFrustrumRange(viewangles, viewforward, viewright, viewup, rd->fovx, rd->fovy);
  if (Range) ViewClip.ClipToRanges(*Range); // range contains a valid range, so we must clip away holes in it

  // update fake sectors
  for (int i = 0; i < Level->NumSectors; ++i) {
    sector_t *sec = &Level->Sectors[i];
         if (sec->deepref) UpdateDeepWater(sec);
    else if (sec->heightsec && !(sec->heightsec->SectorFlags&sector_t::SF_IgnoreHeightSec)) UpdateFakeFlats(sec);
    else if (sec->othersecFloor || sec->othersecCeiling) UpdateFloodBug(sec);
  }

  UpdateBSPNode(Level->NumNodes-1, dummy_bbox); // head node is the last node output
  unguard;
}
