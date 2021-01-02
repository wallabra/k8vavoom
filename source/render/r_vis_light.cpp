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
//**
//**  variouse light visibility methods
//**
//**************************************************************************
#include "../gamedefs.h"
#include "r_local.h"


extern VCvarB r_draw_pobj;


#define UPDATE_LIGHTVIS(ssindex)  do { \
  /*if (LitCalcBBox) LightSubs.append((int)ssindex);*/ \
  /*const vuint8 bvbit = (vuint8)(1u<<((unsigned)(ssindex)&7));*/ \
  /*const unsigned sid8 = (unsigned)(ssindex)>>3;*/ \
  /*LightVis[sid8] |= bvbit;*/ \
  LightVis[(unsigned)(ssindex)] = LightFrameNum; \
  LitVisSubHit = true; \
  if (BspVis[(unsigned)(ssindex)>>3]&((vuint8)(1u<<((unsigned)(ssindex)&7)))) { \
    /*LightBspVis[sid8] |= bvbit;*/ \
    LightBspVis[(unsigned)(ssindex)] = LightFrameNum; \
    HasLightIntersection = true; \
    /*if (LitCalcBBox) LightVisSubs.append((int)ssindex);*/ \
    if (LitCalcBBox) { \
      const subsector_t *vsub = &Level->Subsectors[ssindex]; \
      for (const subregion_t *region = vsub->regions; region; region = region->next) { \
        sec_region_t *curreg = region->secregion; \
        if (vsub->HasPObjs() && r_draw_pobj) { \
          for (auto &&it : vsub->PObjFirst()) { \
            polyobj_t *pobj = it.value(); \
            seg_t **polySeg = pobj->segs; \
            for (int count = pobj->numsegs; count--; ++polySeg) { \
              UpdateBBoxWithLine(LitBBox, curreg->eceiling.splane->SkyBox, (*polySeg)->drawsegs); \
            } \
          } \
        } \
        drawseg_t *ds = region->lines; \
        for (int count = vsub->numlines; count--; ++ds) UpdateBBoxWithLine(LitBBox, curreg->eceiling.splane->SkyBox, ds); \
        if (region->fakefloor) UpdateBBoxWithSurface(LitBBox, region->fakefloor->surfs, &region->fakefloor->texinfo, curreg->efloor.splane->SkyBox, true); \
        if (region->realfloor) UpdateBBoxWithSurface(LitBBox, region->realfloor->surfs, &region->realfloor->texinfo, curreg->efloor.splane->SkyBox, true); \
        if (region->fakeceil) UpdateBBoxWithSurface(LitBBox, region->fakeceil->surfs, &region->fakeceil->texinfo, curreg->eceiling.splane->SkyBox, true); \
        if (region->realceil) UpdateBBoxWithSurface(LitBBox, region->realceil->surfs, &region->realceil->texinfo, curreg->eceiling.splane->SkyBox, true); \
      } \
    } \
  } \
} while (0)


//==========================================================================
//
//  VRenderLevelShared::CalcLightVisCheckNode
//
//==========================================================================
void VRenderLevelShared::CalcLightVisCheckNode (int bspnum, const float *bbox, const float *lightbbox) {
#ifdef VV_CLIPPER_FULL_CHECK
  if (LightClip.ClipIsFull()) return;
#endif

  if (!LightClip.ClipLightIsBBoxVisible(bbox)) return;

  if (bspnum == -1) {
    const unsigned subidx = 0;
    subsector_t *sub = &Level->Subsectors[subidx];
    if (!sub->sector->linecount) return; // skip sectors containing original polyobjs
    if (!LightClip.ClipLightCheckSubsector(sub, false)) {
      LightClip.ClipLightAddSubsectorSegs(sub, false);
      return;
    }
    UPDATE_LIGHTVIS(subidx);
    if (CurrLightBit) {
      if (sub->dlightframe != currDLightFrame) {
        sub->dlightbits = CurrLightBit;
        sub->dlightframe = currDLightFrame;
      } else {
        sub->dlightbits |= CurrLightBit;
      }
    }
    LightClip.ClipLightAddSubsectorSegs(sub, false);
    return;
  }

  // found a subsector?
  if (BSPIDX_IS_NON_LEAF(bspnum)) {
    const node_t *bsp = &Level->Nodes[bspnum];
    // decide which side the view point is on
    const float dist = bsp->PointDistance(CurrLightPos);
    if (dist > CurrLightRadius) {
      // light is completely on front side
      if (!Are3DBBoxesOverlapIn2D(bsp->bbox[0], lightbbox)) return;
      return CalcLightVisCheckNode(bsp->children[0], bsp->bbox[0], lightbbox);
    } else if (dist < -CurrLightRadius) {
      // light is completely on back side
      if (!Are3DBBoxesOverlapIn2D(bsp->bbox[1], lightbbox)) return;
      return CalcLightVisCheckNode(bsp->children[1], bsp->bbox[1], lightbbox);
    } else {
      //unsigned side = (unsigned)bsp->PointOnSide(CurrLightPos);
      unsigned side = (unsigned)(dist <= 0.0f); //(unsigned)bsp->PointOnSide(CurrLightPos);
      // recursively divide front space
      if (Are3DBBoxesOverlapIn2D(bsp->bbox[side], lightbbox)) {
        CalcLightVisCheckNode(bsp->children[side], bsp->bbox[side], lightbbox);
      }
      // possibly divide back space
      side ^= 1;
      if (!Are3DBBoxesOverlapIn2D(bsp->bbox[side], lightbbox)) return;
      return CalcLightVisCheckNode(bsp->children[side], bsp->bbox[side], lightbbox);
    }
  } else {
    const unsigned subidx = (unsigned)(BSPIDX_LEAF_SUBSECTOR(bspnum));
    subsector_t *sub = &Level->Subsectors[subidx];
    if (!sub->sector->linecount) return; // skip sectors containing original polyobjs
    if (!LightClip.ClipLightCheckSubsector(sub, false)) {
      LightClip.ClipLightAddSubsectorSegs(sub, false);
      return;
    }

#if 0
    bool hasGoodSurf = false;
    if (!sub->HasPObjs()) {
      const seg_t *seg = &Level->Segs[sub->firstline];
      for (int count = sub->numlines; count--; ++seg) {
        if (seg->SphereTouches(CurrLightPos, CurrLightRadius)) {
          hasGoodSurf = true;
          break;
        }
      }
      if (!hasGoodSurf) {
        for (const subregion_t *region = sub->regions; region; region = region->next) {
          //const sec_region_t *curreg = region->secregion;
          //const drawseg_t *ds = region->lines;
          if (region->floorplane && region->floorplane->SphereTouches(CurrLightPos, CurrLightRadius)) {
            hasGoodSurf = true;
            break;
          }
          if (region->ceilplane && region->ceilplane->SphereTouches(CurrLightPos, CurrLightRadius)) {
            hasGoodSurf = true;
            break;
          }
        }
      }
      //if (!hasGoodSurf) GCon->Logf("skipped uninteresting subsector");
    } else {
      //FIXME!
      hasGoodSurf = true;
    }

    if (hasGoodSurf)
#endif
    {
      UPDATE_LIGHTVIS(subidx);
      if (CurrLightBit) {
        if (sub->dlightframe != currDLightFrame) {
          sub->dlightbits = CurrLightBit;
          sub->dlightframe = currDLightFrame;
        } else {
          sub->dlightbits |= CurrLightBit;
        }
      }
    }
    LightClip.ClipLightAddSubsectorSegs(sub, false);

    // check if the light is too close to a wall/floor, and calculate "move out" vector
    if (CurrLightCalcUnstuck) {
      const float mindist = 2.5f;
      //const sector_t *sec = sub->sector;

      /*
      const float fz = sec->floor.GetPointZClamped(CurrLightUnstuckPos);
      const float cz = sec->ceiling.GetPointZClamped(CurrLightUnstuckPos);
      */

      // check walls
      const seg_t *seg = &Level->Segs[sub->firstline];
      for (int count = sub->numlines; count--; ++seg) {
        const line_t *linedef = seg->linedef;
        if (!linedef) continue; // miniseg
        if (linedef->flags&ML_TWOSIDED) continue; // don't bother with two-sided lines for now
        //const float dist = sef->PointDistance(CurrLightUnstuckPos);
        /*
        if (seg->partner) {
          if (!seg->partner->frontsub) continue;
          const sector_t *osec = seg->partner->frontsub->sector;
          if (!osec) continue;
          // if this sector height is lower than the other sector height, and the light is above other height, bottex skip
          if ((sec->floor.maxz >= osec->floor.maxz || CurrLightUnstuckPos.z >= osec->floor.maxz) && // skip bottom?
              (sec->ceiling.minz <= osec->ceiling.minz || CurrLightUnstuckPos.z <= osec->ceiling.minz)) // skip top?
          {
            // check midtex presence
            const side_t *sidedef = seg->sidedef;
            if (!sidedef) continue; // just in case
            if (sidedef->MidTexture <= 0) continue;
          }
        }
        */
        const float dist = seg->PointDistance(CurrLightUnstuckPos);
        if (dist > 0.0f && dist < mindist) CurrLightUnstuckPos += seg->normal*(mindist-dist); // move away
      }
      // now check floors
      for (const subregion_t *region = sub->regions; region; region = region->next) {
        if (region->floorplane.splane) {
          const float dist = region->floorplane.PointDistance(CurrLightUnstuckPos);
          if (dist > 0.0f && dist < mindist) CurrLightUnstuckPos += region->floorplane.GetNormal()*(mindist-dist); // move away
        }
        if (region->ceilplane.splane) {
          const float dist = region->ceilplane.PointDistance(CurrLightUnstuckPos);
          if (dist > 0.0f && dist < mindist) CurrLightUnstuckPos += region->floorplane.GetNormal()*(mindist-dist); // move away
        }
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::CheckValidLightPosRough
//
//==========================================================================
bool VRenderLevelShared::CheckValidLightPosRough (TVec &lorg, const sector_t *sec) {
  if (!sec) return true;
  if (sec->floor.normal.z == 1.0f && sec->ceiling.normal.z == -1.0f) {
    // normal sector
    if (sec->floor.minz >= sec->ceiling.maxz) return false; // oops, it is closed
    const float lz = lorg.z;
    float lzdiff = lz-sec->floor.minz;
    if (lzdiff < 0) return false; // stuck
    if (lzdiff == 0) lorg.z += 2; // why not?
    lzdiff = lz-sec->ceiling.minz;
    if (lzdiff > 0) return false; // stuck
    if (lzdiff == 0) lorg.z -= 2; // why not?
  } else {
    // sloped sector
    const float lz = lorg.z;
    const float lfz = sec->floor.GetPointZClamped(lorg);
    const float lcz = sec->ceiling.GetPointZClamped(lorg);
    if (lfz >= lcz) return false; // closed
    float lzdiff = lz-lfz;
    if (lzdiff < 0) return false; // stuck
    if (lzdiff == 0) lorg.z += 2; // why not?
    lzdiff = lz-lcz;
    if (lzdiff > 0) return false; // stuck
    if (lzdiff == 0) lorg.z -= 2; // why not?
  }
  return true;
}


//==========================================================================
//
//  VRenderLevelShared::CalcLightVis
//
//  sets `CurrLightPos` and `CurrLightRadius`, and other lvis fields
//  returns `false` if the light is invisible
//
//  TODO: clip invisible geometry for spotlights
//
//==========================================================================
bool VRenderLevelShared::CalcLightVis (const TVec &org, const float radius, const TVec &aconeDir, const float aconeAngle, int dlnum) {
  if (CurrLightCalcUnstuck) CurrLightUnstuckPos = org;

  //if (dlnum >= 0) dlinfo[dlnum].touchedSubs.reset();
  if (radius < 2) return false;

  //bool skipShadowCheck = !r_light_opt_shadow;

  doShadows = (radius >= 8.0f);

  setupCurrentLight(org, radius, aconeDir, aconeAngle);
  //CurrLightPos = org;
  //CurrLightRadius = radius;
  CurrLightBit = (dlnum >= 0 ? 1u<<dlnum : 0u);

  /*LightSubs.reset();*/ // all affected subsectors
  /*LightVisSubs.reset();*/ // visible affected subsectors
  LitVisSubHit = false;
  LitSurfaceHit = false;
  //HasBackLit = false;

  LitBBox[0] = TVec(+FLT_MAX, +FLT_MAX, +FLT_MAX);
  LitBBox[1] = TVec(-FLT_MAX, -FLT_MAX, -FLT_MAX);

  float dummybbox[6] = { -99999, -99999, -99999, 99999, 99999, 99999 };

  // create light bounding box
  float lightbbox[6] = {
    org.x-radius,
    org.y-radius,
    0, // doesn't matter
    org.x+radius,
    org.y+radius,
    0, // doesn't matter
  };

  // build vis data for light
  IncLightFrameNum();
  LightClip.ClearClipNodes(CurrLightPos, Level, CurrLightRadius);
  HasLightIntersection = false;
  CalcLightVisCheckNode(Level->NumNodes-1, dummybbox, lightbbox);
  if (!HasLightIntersection) return false;

  return true;
}


//==========================================================================
//
//  VRenderLevelShared::RadiusCastRay
//
//==========================================================================
bool VRenderLevelShared::RadiusCastRay (bool textureCheck, sector_t *sector, const TVec &org, sector_t *destsector, const TVec &dest, float radius) {
#if 0
  // BSP tracing
  float dsq = length2DSquared(org-dest);
  if (dsq <= 1) return true;
  linetrace_t Trace;
  bool canHit = !!Level->TraceLine(Trace, org, dest, SPF_NOBLOCKSIGHT);
  if (canHit) return true;
  if (!advanced || radius < 12) return false;
  // check some more rays
  if (r_lights_cast_many_rays) {
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if ((dy|dx) == 0) continue;
        TVec np = org;
        np.x += radius*(0.73f*dx);
        np.y += radius*(0.73f*dy);
        canHit = !!Level->TraceLine(Trace, np, dest, SPF_NOBLOCKSIGHT);
        if (canHit) return true;
      }
    }
  } else {
    // check only "head" and "feet"
    TVec np = org;
    np.y += radius*0.73f;
    if (Level->TraceLine(Trace, np, dest, SPF_NOBLOCKSIGHT)) return true;
    np = org;
    np.y -= radius*0.73f;
    if (Level->TraceLine(Trace, np, dest, SPF_NOBLOCKSIGHT)) return true;
  }
  return false;
#else
  // blockmap tracing
  return Level->CastLightRay(textureCheck, sector, org, dest, destsector);
#endif
}
