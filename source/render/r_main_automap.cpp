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
// directly included from "r_main.cpp"

extern VCvarB am_draw_texture_with_bsp;


// ////////////////////////////////////////////////////////////////////////// //
extern "C" {
  static int ssurfCmp (const void *aa, const void *bb, void *) {
    if (aa == bb) return 0;
    const sec_surface_t *a = *(const sec_surface_t **)aa;
    const sec_surface_t *b = *(const sec_surface_t **)bb;
    const auto atx = (uintptr_t)(a->texinfo.Tex);
    const auto btx = (uintptr_t)(b->texinfo.Tex);
    return (atx < btx ? -1 : atx > btx ? 1 : 0);
  }
}


//==========================================================================
//
//  VRenderLevelShared::AM_getFlatSurface
//
//==========================================================================
sec_surface_t *VRenderLevelShared::AM_getFlatSurface (subregion_t *reg, bool doFloors) {
  if (!reg) return nullptr;
  sec_surface_t *flatsurf;
  if (doFloors) {
    // get floor
    flatsurf = reg->realfloor;
    if (!flatsurf) {
      flatsurf = reg->fakefloor;
    } else if (reg->fakefloor && reg->fakefloor->esecplane.GetDist() < flatsurf->esecplane.GetDist()) {
      flatsurf = reg->fakefloor;
    }
  } else {
    // get ceiling
    flatsurf = reg->realceil;
    if (!flatsurf) {
      flatsurf = reg->fakeceil;
    } else if (reg->fakeceil && reg->fakeceil->esecplane.GetDist() > flatsurf->esecplane.GetDist()) {
      flatsurf = reg->fakeceil;
    }
  }
  return flatsurf;
}


//==========================================================================
//
//  VRenderLevelShared::amFlatsCheckSubsector
//
//==========================================================================
void VRenderLevelShared::amFlatsCheckSubsector (int num) {
  subsector_t *sub = &Level->Subsectors[num];
  if (!sub->sector->linecount) return; // skip sectors containing original polyobjs
  // first subregion is main sector subregion
  subregion_t *reg = sub->regions;
  if (!reg) return; // just in case
  if (!amCheckSubsector(sub)) return; // invisible on automap
  // get flat surface
  sec_surface_t *flatsurf = AM_getFlatSurface(reg, amDoFloors);
  if (!flatsurf || !flatsurf->texinfo.Tex || flatsurf->texinfo.Tex->Type == TEXTYPE_Null) return; // just in case
  // if this is a sky, and we're rendering ceiling, render floor instead
  if (/*!amDoFloors &&*/ flatsurf->texinfo.Tex == amSkyTex) {
    //if (amDoFloors) return;
    flatsurf = AM_getFlatSurface(reg, !amDoFloors);
    if (!flatsurf || !flatsurf->texinfo.Tex || flatsurf->texinfo.Tex->Type == TEXTYPE_Null || flatsurf->texinfo.Tex == amSkyTex) return; // just in case
  }
  if (!flatsurf->surfs) return;
  //vassert(flatsurf->surfs->subsector == sub);
  // update textures (why not? this updates floor animation)
  UpdateSubsectorFlatSurfaces(sub, amDoFloors, !amDoFloors);
  amSurfList.append(flatsurf);
}


//==========================================================================
//
//  VRenderLevelShared::amFlatsCheckNode
//
//==========================================================================
void VRenderLevelShared::amFlatsCheckNode (int bspnum) {
  // found a subsector?
  if (!(bspnum&NF_SUBSECTOR)) {
    // nope, this is a normal node
    node_t *bsp = &Level->Nodes[bspnum];
    // decide which side the view point is on
    for (unsigned side = 0; side < 2; ++side) {
      if (AM_isBBox3DVisible(bsp->bbox[side])) {
        amFlatsCheckNode(bsp->children[side]);
        //++amFlatsAcc;
      }/* else {
        ++amFlatsRej;
      }*/
    }
  } else {
    // leaf node (subsector)
    amFlatsCheckSubsector(bspnum&(~NF_SUBSECTOR));
  }
}


//==========================================================================
//
//  VRenderLevelShared::amFlatsCollectSurfaces
//
//==========================================================================
void VRenderLevelShared::amFlatsCollectSurfaces () {
  amSurfList.resetNoDtor();
  //amFlatsRej = amFlatsAcc = 0;
  if (Level->NumSectors == 0 || Level->NumSubsectors == 0) return; // just in case
  amSkyTex = GTextureManager.getIgnoreAnim(skyflatnum);
  // for "view whole map"
  if (am_draw_texture_with_bsp) {
    if (Level->NumNodes == 0) {
      amFlatsCheckSubsector(0);
    } else {
      amFlatsCheckNode(Level->NumNodes-1);
    }
  } else {
    for (int f = 0; f < Level->NumSubsectors; ++f) amFlatsCheckSubsector(f);
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderTexturedAutomap
//
//==========================================================================
void VRenderLevelShared::RenderTexturedAutomap (float m_x, float m_y, float m_x2, float m_y2,
                                                bool doFloors, // floors or ceiling?
                                                float alpha,
                                                AMCheckSubsectorCB CheckSubsector,
                                                AMIsHiddenSubsectorCB IsHiddenSubsector,
                                                AMMapXYtoFBXYCB MapXYtoFBXY
                                              )
{
  if (alpha <= 0.0f) return;
  if (alpha > 1.0f) alpha = 1.0f;
  vassert(CheckSubsector);
  vassert(IsHiddenSubsector);
  vassert(MapXYtoFBXY);

  amDoFloors = doFloors;
  amCheckSubsector = CheckSubsector;
  amX = m_x;
  amY = m_y;
  amX2 = m_x2;
  amY2 = m_y2;

  amFlatsCollectSurfaces();
  if (amSurfList.length() == 0) return; // nothing to do

  //GCon->Logf("am: nodes reject=%d; accept=%d", amFlatsRej, amFlatsAcc);

  // sort surfaces by texture
  timsort_r(amSurfList.ptr(), amSurfList.length(), sizeof(sec_surface_t *), &ssurfCmp, nullptr);

  // render surfaces
  Drawer->BeginTexturedPolys();

  const sec_surface_t *const *css = amSurfList.ptr();
  for (int csscount = amSurfList.length(); csscount--; ++css) {
    const subsector_t *subsec = (*css)->surfs->subsector;
    const sector_t *sector = subsec->sector;
    // calculate lighting
    const float lev = clampval(sector->params.lightlevel/255.0f, 0.0f, 1.0f);
    const vuint32 light = sector->params.LightColor;
    TVec vlight(
      ((light>>16)&255)*lev/255.0f,
      ((light>>8)&255)*lev/255.0f,
      (light&255)*lev/255.0f);
    // draw hidden parts bluish
    if (IsHiddenSubsector(subsec)) {
      const float intensity = colorIntensity((light>>16)&255, (light>>8)&255, light&255)/255.0f;
      vlight = TVec(0.1f, 0.1f, intensity);
    }
    // render surfaces
    for (surface_t *surf = (*css)->surfs; surf; surf = surf->next) {
      if (surf->count < 3) continue;
      amTmpVerts.resetNoDtor();
      float sx, sy;
      for (int vn = 0; vn < surf->count; ++vn) {
        MapXYtoFBXY(&sx, &sy, surf->verts[vn].x, surf->verts[vn].y);
        amTmpVerts.append(TVec(sx, sy, 0));
      }
      Drawer->DrawTexturedPoly(&(*css)->texinfo, vlight, alpha, amTmpVerts.length(), amTmpVerts.ptr(), surf->verts);
    }
  }

  Drawer->EndTexturedPolys();
}
