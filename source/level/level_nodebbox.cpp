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
#include "../gamedefs.h"


static VCvarB r_bsp_loose_bbox_height("r_bsp_loose_bbox_height", false, "If `true`, the engine will try to calculate proper bbox heights.", CVAR_Archive);
static int lastLooseBBoxHeight = -1; // unknown yet


//==========================================================================
//
//  VLevel::CalcSkyHeight
//
//==========================================================================
float VLevel::CalcSkyHeight () const {
  if (NumSectors == 0) return 0.0f; // just in case
  // calculate sky height
  float skyheight = -99999.0f;
  for (unsigned i = 0; i < (unsigned)NumSectors; ++i) {
    if (Sectors[i].ceiling.pic == skyflatnum &&
        Sectors[i].ceiling.maxz > skyheight)
    {
      skyheight = Sectors[i].ceiling.maxz;
    }
  }
  // make it a bit higher to avoid clipping of the sprites
  skyheight += 8*1024;
  return skyheight;
}


//==========================================================================
//
//  VLevel::UpdateSectorHeightCache
//
//  some sectors (like doors) has floor and ceiling on the same level, so
//  we have to look at neighbour sector to get height.
//  note that if neighbour sector is closed door too, we can safely use
//  our zero height, as camera cannot see through top/bottom textures.
//
//==========================================================================
void VLevel::UpdateSectorHeightCache (sector_t *sector) {
  if (!sector || sector->ZExtentsCacheId == validcountSZCache) return;

  sector->ZExtentsCacheId = validcountSZCache;

  float minz = sector->floor.minz;
  float maxz = sector->ceiling.maxz;
  if (minz > maxz) { const float tmp = minz; minz = maxz; maxz = tmp; }

  if (!lastLooseBBoxHeight && sector->linecount) {
    sector_t *const *nbslist = sector->nbsecs;
    for (int nbc = sector->nbseccount; nbc--; ++nbslist) {
      const sector_t *bsec = *nbslist;
      /*
      // self-referencing deepwater is usually deeper than the surrounding sector
      if (bsec == sector) {
        //FIXME: this is deepwater, make in infinitely high
        minz = -32767.0f;
        maxz = 32767.0f;
        break;
      }
      */
      float zmin = min2(bsec->floor.minz, bsec->ceiling.maxz);
      float zmax = max2(bsec->floor.minz, bsec->ceiling.maxz);
      minz = min2(minz, zmin);
      maxz = max2(maxz, zmax);
    }
  } else {
    minz = -32768.0f;
    maxz = +32767.0f;
  }

  sector->LastMinZ = minz;
  sector->LastMaxZ = maxz;

  // update BSP
  for (subsector_t *sub = sector->subsectors; sub; sub = sub->seclink) {
    node_t *node = sub->parent;
    //GCon->Logf("  sub %d; pc=%d; nodeparent=%d; next=%d", (int)(ptrdiff_t)(sub-Subsectors), sub->parentChild, (int)(ptrdiff_t)(node-Nodes), (sub->seclink ? (int)(ptrdiff_t)(sub->seclink-Subsectors) : -1));
    if (!node) continue;
    int childnum = sub->parentChild;
    if (node->bbox[childnum][2] <= minz && node->bbox[childnum][5] >= maxz) continue;
    // fix bounding boxes
    float currMinZ = min2(node->bbox[childnum][2], minz);
    float currMaxZ = max2(node->bbox[childnum][5], maxz);
    if (currMinZ > currMaxZ) { float tmp = currMinZ; currMinZ = currMaxZ; currMaxZ = tmp; } // just in case
    node->bbox[childnum][2] = currMinZ;
    node->bbox[childnum][5] = currMaxZ;
    for (; node->parent; node = node->parent) {
      node_t *pnode = node->parent;
           if (pnode->children[0] == node->index) childnum = 0;
      else if (pnode->children[1] == node->index) childnum = 1;
      else Sys_Error("invalid BSP tree");
      const float parCMinZ = pnode->bbox[childnum][2];
      const float parCMaxZ = pnode->bbox[childnum][5];
      if (parCMinZ <= currMinZ && parCMaxZ >= currMaxZ) continue; // we're done here
      pnode->bbox[childnum][2] = min2(parCMinZ, currMinZ);
      pnode->bbox[childnum][5] = max2(parCMaxZ, currMaxZ);
      FixBBoxZ(pnode->bbox[childnum]);
      currMinZ = min2(min2(parCMinZ, pnode->bbox[childnum^1][2]), currMinZ);
      currMaxZ = max2(max2(parCMaxZ, pnode->bbox[childnum^1][5]), currMaxZ);
      if (currMinZ > currMaxZ) { float tmp = currMinZ; currMinZ = currMaxZ; currMaxZ = tmp; } // just in case
    }
  }
}


//==========================================================================
//
//  VLevel::GetSubsectorBBox
//
//==========================================================================
void VLevel::GetSubsectorBBox (subsector_t *sub, float bbox[6]) {
  // min
  bbox[0+0] = sub->bbox2d[BOX2D_LEFT];
  bbox[0+1] = sub->bbox2d[BOX2D_BOTTOM];
  // max
  bbox[3+0] = sub->bbox2d[BOX2D_RIGHT];
  bbox[3+1] = sub->bbox2d[BOX2D_TOP];

  sector_t *sector = sub->sector;
  UpdateSectorHeightCache(sector);
  bbox[0+2] = sector->LastMinZ;
  bbox[3+2] = sector->LastMaxZ;
  //FixBBoxZ(bbox);
}


//==========================================================================
//
//  VLevel::CalcSecMinMaxs
//
//==========================================================================
void VLevel::CalcSecMinMaxs (sector_t *sector) {
  if (!sector) return; // k8: just in case

  unsigned slopedFC = 0;

  if (sector->floor.normal.z == 1.0f || sector->linecount == 0) {
    // horizontal floor
    sector->floor.minz = sector->floor.dist;
    sector->floor.maxz = sector->floor.dist;
  } else {
    // sloped floor
    slopedFC |= 1;
  }

  if (sector->ceiling.normal.z == -1.0f || sector->linecount == 0) {
    // horisontal ceiling
    sector->ceiling.minz = -sector->ceiling.dist;
    sector->ceiling.maxz = -sector->ceiling.dist;
  } else {
    // sloped ceiling
    slopedFC |= 2;
  }

  // calculate extents for sloped flats
  if (slopedFC) {
    float minzf = +99999.0f;
    float maxzf = -99999.0f;
    float minzc = +99999.0f;
    float maxzc = -99999.0f;
    line_t **llist = sector->lines;
    for (int cnt = sector->linecount; cnt--; ++llist) {
      line_t *ld = *llist;
      if (slopedFC&1) {
        float z = sector->floor.GetPointZ(*ld->v1);
        minzf = min2(minzf, z);
        maxzf = max2(maxzf, z);
        z = sector->floor.GetPointZ(*ld->v2);
        minzf = min2(minzf, z);
        maxzf = max2(maxzf, z);
      }
      if (slopedFC&2) {
        float z = sector->ceiling.GetPointZ(*ld->v1);
        minzc = min2(minzc, z);
        maxzc = max2(maxzc, z);
        z = sector->ceiling.GetPointZ(*ld->v2);
        minzc = min2(minzc, z);
        maxzc = max2(maxzc, z);
      }
    }
    if (slopedFC&1) {
      sector->floor.minz = minzf;
      sector->floor.maxz = maxzf;
    }
    if (slopedFC&2) {
      sector->ceiling.minz = minzc;
      sector->ceiling.maxz = maxzc;
    }
  }

  sector->ZExtentsCacheId = 0; // force update
  UpdateSectorHeightCache(sector); // this also updates BSP bounding boxes
}


//==========================================================================
//
//  VLevel::UpdateSubsectorBBox
//
//==========================================================================
void VLevel::UpdateSubsectorBBox (int num, float bbox[6], const float skyheight) {
  subsector_t *sub = &Subsectors[num];
  // nope, don't ignore it
  //if (sub->sector->linecount == 0) return; // original polyobj sector

  float ssbbox[6];
  GetSubsectorBBox(sub, ssbbox);
  FixBBoxZ(ssbbox);
  ssbbox[2] = min2(ssbbox[2], sub->sector->floor.minz);
  ssbbox[5] = max2(ssbbox[5], (R_IsAnySkyFlatPlane(&sub->sector->ceiling) ? skyheight : sub->sector->ceiling.maxz));
  FixBBoxZ(ssbbox);

  /*
  FixBBoxZ(bbox);
  for (unsigned f = 0; f < 3; ++f) {
    bbox[0+f] = min2(bbox[0+f], ssbbox[0+f]);
    bbox[3+f] = max2(bbox[3+f], ssbbox[3+f]);
  }
  */
  memcpy(bbox, ssbbox, sizeof(ssbbox));
  FixBBoxZ(bbox);

  //bbox[2] = sub->sector->floor.minz;
  //bbox[5] = (R_IsAnySkyFlatPlane(&sub->sector->ceiling) ? skyheight : sub->sector->ceiling.maxz);
  //FixBBoxZ(bbox);
}


//==========================================================================
//
//  VLevel::RecalcWorldNodeBBox
//
//==========================================================================
void VLevel::RecalcWorldNodeBBox (int bspnum, float bbox[6], const float skyheight) {
  if (bspnum == -1) {
    UpdateSubsectorBBox(0, bbox, skyheight);
    return;
  }
  // found a subsector?
  if (!(bspnum&NF_SUBSECTOR)) {
    // nope, this is a normal node
    node_t *bsp = &Nodes[bspnum];
    // decide which side the view point is on
    for (unsigned side = 0; side < 2; ++side) {
      RecalcWorldNodeBBox(bsp->children[side], bsp->bbox[side], skyheight);
      FixBBoxZ(bsp->bbox[side]);
      for (unsigned f = 0; f < 3; ++f) {
        if (bbox[0+f] <= -99990.0f) bbox[0+f] = bsp->bbox[side][0+f];
        if (bbox[3+f] >= +99990.0f) bbox[3+f] = bsp->bbox[side][3+f];
        bbox[0+f] = min2(bbox[0+f], bsp->bbox[side][0+f]);
        bbox[3+f] = max2(bbox[3+f], bsp->bbox[side][3+f]);
      }
      FixBBoxZ(bbox);
    }
    //bbox[2] = min2(bsp->bbox[0][2], bsp->bbox[1][2]);
    //bbox[5] = max2(bsp->bbox[0][5], bsp->bbox[1][5]);
  } else {
    // leaf node (subsector)
    UpdateSubsectorBBox(bspnum&(~NF_SUBSECTOR), bbox, skyheight);
  }
}


/*
#define ITER_CHECKER(arrname_,itname_,typename_) \
  { \
    int count = 0; \
    for (auto &&it : itname_()) { \
      vassert(it.index() == count); \
      typename_ *tp = it.value(); \
      vassert(tp == &arrname_[count]); \
      ++count; \
    } \
  }
*/

//==========================================================================
//
//  VLevel::RecalcWorldBBoxes
//
//==========================================================================
void VLevel::RecalcWorldBBoxes () {
  /*
  ITER_CHECKER(Vertexes, allVerticesIdx, TVec)
  ITER_CHECKER(Sectors, allSectorsIdx, sector_t)
  ITER_CHECKER(Sides, allSidesIdx, side_t)
  ITER_CHECKER(Lines, allLinesIdx, line_t)
  ITER_CHECKER(Segs, allSegsIdx, seg_t)
  ITER_CHECKER(Subsectors, allSubsectorsIdx, subsector_t)
  ITER_CHECKER(Nodes, allNodesIdx, node_t)
  ITER_CHECKER(Things, allThingsIdx, mthing_t)
  */
  ResetSZValidCount();
  lastLooseBBoxHeight = (r_bsp_loose_bbox_height ? 1 : 0);
  if (NumSectors == 0 || NumSubsectors == 0) return; // just in case
  const float skyheight = CalcSkyHeight();
  for (auto &&node : allNodes()) {
    // special values
    node.bbox[0][0+2] = -99999.0f;
    node.bbox[0][3+2] = +99999.0f;
    node.bbox[1][0+2] = -99999.0f;
    node.bbox[1][3+2] = +99999.0f;
  }
  float dummy_bbox[6] = { -99999.0f, -99999.0f, -99999.0f, 99999.0f, 99999.0f, 99999.0f };
  RecalcWorldNodeBBox(NumNodes-1, dummy_bbox, skyheight);
}


//==========================================================================
//
//  VLevel::CheckAndRecalcWorldBBoxes
//
//  recalcs world bboxes if some cvars changed
//
//==========================================================================
void VLevel::CheckAndRecalcWorldBBoxes () {
  int nbbh = (r_bsp_loose_bbox_height ? 1 : 0);
  if (lastLooseBBoxHeight != nbbh) {
    double stime = -Sys_Time();
    RecalcWorldBBoxes();
    stime += Sys_Time();
    GCon->Logf("Recalculated world BSP bounding boxes in %g seconds", stime);
  }
}
