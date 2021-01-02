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
//**  update bboxes and fake floors
//**
//**************************************************************************
#include "../gamedefs.h"
#include "r_local.h"


//==========================================================================
//
//  VRenderLevelShared::UpdateBBoxWithSurface
//
//  `CheckSkyBoxAlways` is set for floors and ceilings
//
//==========================================================================
void VRenderLevelShared::UpdateBBoxWithSurface (TVec bbox[2], surface_t *surfs, const texinfo_t *texinfo,
                                                VEntity *SkyBox, bool CheckSkyBoxAlways)
{
  if (!surfs) return;

  if (!texinfo || texinfo->Tex->Type == TEXTYPE_Null) return;
  if (texinfo->Alpha < 1.0f) return;

  if (SkyBox && SkyBox->IsPortalDirty()) SkyBox = nullptr;

  if (texinfo->Tex == GTextureManager.getIgnoreAnim(skyflatnum) ||
      (CheckSkyBoxAlways && SkyBox && SkyBox->GetSkyBoxAlways()))
  {
    return;
  }

  for (surface_t *surf = surfs; surf; surf = surf->next) {
    if (surf->count < 3) continue; // just in case
    if (!surf->IsVisibleFor(Drawer->vieworg)) {
      // viewer is in back side or on plane
      /*
      if (!HasBackLit) {
        const float dist = surf->PointDistance(CurrLightPos);
        HasBackLit = (dist > 0.0f && dist < CurrLightRadius);
      }
      */
      continue;
    }
    const float dist = surf->PointDistance(CurrLightPos);
    if (dist <= 0.0f || dist >= CurrLightRadius) continue; // light is too far away, or surface is not lit
    LitSurfaceHit = true;
    const SurfVertex *vert = surf->verts;
    for (int vcount = surf->count; vcount--; ++vert) {
      bbox[0].x = min2(bbox[0].x, vert->x);
      bbox[0].y = min2(bbox[0].y, vert->y);
      bbox[0].z = min2(bbox[0].z, vert->z);
      bbox[1].x = max2(bbox[1].x, vert->x);
      bbox[1].y = max2(bbox[1].y, vert->y);
      bbox[1].z = max2(bbox[1].z, vert->z);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::UpdateBBoxWithLine
//
//==========================================================================
void VRenderLevelShared::UpdateBBoxWithLine (TVec bbox[2], VEntity *SkyBox, const drawseg_t *dseg) {
  const seg_t *seg = dseg->seg;
  if (!seg->linedef) return; // miniseg
  // if light sphere is not touching a plane, do nothing
  const float dist = seg->PointDistance(CurrLightPos);
  //if (dist <= -CurrLightRadius || dist > CurrLightRadius) return; // light sphere is not touching a plane
  if (fabsf(dist) >= CurrLightRadius) return;
  // check clipper
  if (!LightClip.IsRangeVisible(*seg->v2, *seg->v1)) return;
  // update bbox with surfaces
  if (!seg->backsector) {
    // single sided line
    if (dseg->mid) UpdateBBoxWithSurface(bbox, dseg->mid->surfs, &dseg->mid->texinfo, SkyBox, false);
    if (dseg->topsky) UpdateBBoxWithSurface(bbox, dseg->topsky->surfs, &dseg->topsky->texinfo, SkyBox, false);
  } else {
    // two sided line
    if (dseg->top) UpdateBBoxWithSurface(bbox, dseg->top->surfs, &dseg->top->texinfo, SkyBox, false);
    if (dseg->topsky) UpdateBBoxWithSurface(bbox, dseg->topsky->surfs, &dseg->topsky->texinfo, SkyBox, false);
    if (dseg->bot) UpdateBBoxWithSurface(bbox, dseg->bot->surfs, &dseg->bot->texinfo, SkyBox, false);
    if (dseg->mid) UpdateBBoxWithSurface(bbox, dseg->mid->surfs, &dseg->mid->texinfo, SkyBox, false);
    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      UpdateBBoxWithSurface(bbox, sp->surfs, &sp->texinfo, SkyBox, false);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::UpdateFakeSectors
//
//==========================================================================
void VRenderLevelShared::UpdateFakeSectors (subsector_t *viewleaf) {
  //TODO: camera renderer can change view origin, and this can change fake floors
  subsector_t *ovl = r_viewleaf;
  r_viewleaf = (viewleaf ? viewleaf : Level->PointInSubsector(Drawer->vieworg));
  // update fake sectors
  const vint32 *fksip = Level->FakeFCSectors.ptr();
  for (int i = Level->FakeFCSectors.length(); i--; ++fksip) {
    sector_t *sec = &Level->Sectors[*fksip];
         if (sec->deepref) UpdateDeepWater(sec);
    else if (sec->heightsec && !(sec->heightsec->SectorFlags&sector_t::SF_IgnoreHeightSec)) UpdateFakeFlats(sec);
    else if (sec->othersecFloor || sec->othersecCeiling) UpdateFloodBug(sec);
  }
  r_viewleaf = ovl;
}


//==========================================================================
//
//  VRenderLevelShared::InitialWorldUpdate
//
//==========================================================================
void VRenderLevelShared::InitialWorldUpdate () {
  subsector_t *sub = &Level->Subsectors[0];
  for (int scount = Level->NumSubsectors; scount--; ++sub) {
    if (!sub->sector->linecount) continue; // skip sectors containing original polyobjs
    UpdateSubRegion(sub, sub->regions);
  }
}


//==========================================================================
//
//  VRenderLevelShared::FullWorldUpdate
//
//==========================================================================
void VRenderLevelShared::FullWorldUpdate (bool forceClientOrigin) {
  TVec oldVO = Drawer->vieworg;
  if (forceClientOrigin && cl) {
    GCon->Log(NAME_Debug, "performing full world update with client view origin");
    Drawer->vieworg = cl->ViewOrg;
    //GCon->Logf(NAME_Debug, "*** vo=(%g,%g,%g)", Drawer->vieworg.x, Drawer->vieworg.y, Drawer->vieworg.z);
  } else {
    GCon->Log(NAME_Debug, "performing full world update");
  }
  UpdateFakeSectors();
  InitialWorldUpdate();
  Drawer->vieworg = oldVO;
}
