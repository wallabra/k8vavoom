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

// this is used to compare floats like ints which is faster
#define FASI(var) (*(const int32_t *)&var)

//#define GOZZO_3DSHIT_TEMP_2S_HACK


// ////////////////////////////////////////////////////////////////////////// //
extern VCvarB w_update_clip_bsp;
extern VCvarB w_update_clip_region;
extern VCvarB w_update_in_renderer;


//**************************************************************************
//
//  Scaling
//
//**************************************************************************
static inline __attribute__((const)) float TextureSScale (const VTexture *pic) { return pic->SScale; }
static inline __attribute__((const)) float TextureTScale (const VTexture *pic) { return pic->TScale; }
static inline __attribute__((const)) float TextureOffsetSScale (const VTexture *pic) { return (pic->bWorldPanning ? pic->SScale : 1.0f); }
static inline __attribute__((const)) float TextureOffsetTScale (const VTexture *pic) { return (pic->bWorldPanning ? pic->TScale : 1.0f); }


//**************************************************************************
//**
//**  Sector surfaces
//**
//**************************************************************************

//==========================================================================
//
//  VRenderLevelShared::SetupSky
//
//==========================================================================
void VRenderLevelShared::SetupSky () {
  skyheight = -99999.0f;
  for (int i = 0; i < Level->NumSectors; ++i) {
    if (Level->Sectors[i].ceiling.pic == skyflatnum &&
        Level->Sectors[i].ceiling.maxz > skyheight)
    {
      skyheight = Level->Sectors[i].ceiling.maxz;
    }
  }
  // make it a bit higher to avoid clipping of the sprites
  skyheight += 8*1024;
  memset((void *)&sky_plane, 0, sizeof(sky_plane));
  sky_plane.Set(TVec(0, 0, -1), -skyheight);
  sky_plane.pic = skyflatnum;
  sky_plane.Alpha = 1.1f;
  sky_plane.LightSourceSector = -1;
  sky_plane.MirrorAlpha = 1.0f;
  sky_plane.XScale = 1.0f;
  sky_plane.YScale = 1.0f;
}


//==========================================================================
//
//  VRenderLevelShared::FlushSurfCaches
//
//==========================================================================
void VRenderLevelShared::FlushSurfCaches (surface_t *InSurfs) {
  surface_t *surfs = InSurfs;
  while (surfs) {
    if (surfs->CacheSurf) FreeSurfCache(surfs->CacheSurf);
    surfs = surfs->next;
  }
}


//==========================================================================
//
//  VRenderLevelShared::CreateSecSurface
//
//  this is used to create floor and ceiling surfaces
//
//  `ssurf` can be `nullptr`, and it will be allocated, otherwise changed
//
//==========================================================================
sec_surface_t *VRenderLevelShared::CreateSecSurface (sec_surface_t *ssurf, subsector_t *sub, TSecPlaneRef InSplane) {
  TSecPlaneRef spl(InSplane);
  int vcount = sub->numlines;

  if (vcount < 3) {
    GCon->Logf(NAME_Warning, "CreateSecSurface: subsector #%d has only #%d vertices", (int)(ptrdiff_t)(sub-Level->Subsectors), vcount);
    if (vcount < 1) Sys_Error("ONE VERTEX. WTF?!");
    if (ssurf) return ssurf;
  }
  //check(vcount >= 3);

  // if we're simply changing sky, and already have surface created, do not recreate it, it is pointless
  bool isSkyFlat = (spl.splane->pic == skyflatnum);
  bool recalcSurface = true;
  bool updateZ = false;

  // fix plane
  if (isSkyFlat && spl.GetNormalZ() < 0.0f) spl.set(&sky_plane, false);

  surface_t *surf = nullptr;
  if (!ssurf) {
    // new sector surface
    ssurf = new sec_surface_t;
    memset((void *)ssurf, 0, sizeof(sec_surface_t));
    surf = (surface_t *)Z_Calloc(sizeof(surface_t)+(vcount-1)*sizeof(TVec));
  } else {
    // change sector surface
    // we still may have to recreate it if it was a "sky <-> non-sky" change, so check for it
    recalcSurface = !isSkyFlat || ((ssurf->esecplane.splane->pic == skyflatnum) != isSkyFlat);
    if (recalcSurface) {
      surf = ReallocSurface(ssurf->surfs, vcount);
    } else {
      updateZ = (FASI(ssurf->edist) != FASI(spl.splane->dist));
      surf = ssurf->surfs;
    }
    ssurf->surfs = nullptr; // just in case
  }

  // this is required to calculate static lightmaps, and for other business
  for (surface_t *ss = surf; ss; ss = ss->next) ss->subsector = sub; // this is required to calculate static lightmaps, and for other business

  ssurf->esecplane = spl;
  ssurf->edist = spl.splane->dist;

  // setup texture
  VTexture *Tex = GTextureManager(spl.splane->pic);
  check(Tex);
  if (fabsf(spl.GetNormalZ()) > 0.1f) {
    float s, c;
    msincos(spl.splane->BaseAngle-spl.splane->Angle, &s, &c);
    ssurf->texinfo.saxis = TVec(c,  s, 0)*TextureSScale(Tex)*spl.splane->XScale;
    ssurf->texinfo.taxis = TVec(s, -c, 0)*TextureTScale(Tex)*spl.splane->YScale;
  } else {
    ssurf->texinfo.taxis = TVec(0, 0, -1)*TextureTScale(Tex)*spl.splane->YScale;
    ssurf->texinfo.saxis = Normalise(CrossProduct(spl.GetNormal(), ssurf->texinfo.taxis))*TextureSScale(Tex)*spl.splane->XScale;
  }
  ssurf->texinfo.soffs = spl.splane->xoffs;
  ssurf->texinfo.toffs = spl.splane->yoffs+spl.splane->BaseYOffs;
  ssurf->texinfo.Tex = Tex;
  ssurf->texinfo.noDecals = (Tex ? Tex->noDecals : true);
  ssurf->texinfo.Alpha = (spl.splane->Alpha < 1.0f ? spl.splane->Alpha : 1.1f);
  ssurf->texinfo.Additive = !!(spl.splane->flags&SPF_ADDITIVE);
  ssurf->texinfo.ColourMap = 0;
  ssurf->XScale = spl.splane->XScale;
  ssurf->YScale = spl.splane->YScale;
  ssurf->Angle = spl.splane->BaseAngle-spl.splane->Angle;

  if (recalcSurface) {
    if (spl.flipped) surf->drawflags |= surface_t::DF_FLIP_PLANE; else surf->drawflags &= ~surface_t::DF_FLIP_PLANE;
    surf->count = vcount;
    const seg_t *seg = &Level->Segs[sub->firstline];
    TVec *dptr = surf->verts;
    if (spl.GetNormalZ() < 0.0f) {
      // backward
      for (seg += (vcount-1); vcount--; ++dptr, --seg) {
        const TVec &v = *seg->v1;
        *dptr = TVec(v.x, v.y, spl.GetPointZ(v.x, v.y));
      }
    } else {
      // forward
      for (; vcount--; ++dptr, ++seg) {
        const TVec &v = *seg->v1;
        *dptr = TVec(v.x, v.y, spl.GetPointZ(v.x, v.y));
      }
    }

    if (isSkyFlat) {
      // don't subdivide sky, as it cannot have lightmap
      ssurf->surfs = surf;
      surf->texinfo = &ssurf->texinfo;
      surf->eplane = spl.splane;
    } else {
      ssurf->surfs = SubdivideFace(surf, ssurf->texinfo.saxis, &ssurf->texinfo.taxis);
      InitSurfs(ssurf->surfs, &ssurf->texinfo, spl.splane, sub);
    }
  } else {
    // update z coords, if necessary
    if (updateZ) {
      for (; surf; surf = surf->next) {
        TVec *svert = surf->verts;
        for (int i = surf->count; i--; ++svert) svert->z = spl.GetPointZ(svert->x, svert->y);
      }
    }
  }


  return ssurf;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateSecSurface
//
//  this is used to update floor and ceiling surfaces
//
//==========================================================================
void VRenderLevelShared::UpdateSecSurface (sec_surface_t *ssurf, TSecPlaneRef RealPlane, subsector_t *sub) {
  if (!ssurf->esecplane.splane->pic) return; // no texture? nothing to do

  TSecPlaneRef splane(ssurf->esecplane);

  if (splane.splane != RealPlane.splane) {
    // check for sky changes
    if ((splane.splane->pic == skyflatnum) != (RealPlane.splane->pic == skyflatnum)) {
      // sky <-> non-sky, simply recreate it
      sec_surface_t *newsurf = CreateSecSurface(ssurf, sub, RealPlane);
      check(newsurf == ssurf); // sanity check
      ssurf->texinfo.ColourMap = ColourMap; // just in case
      // nothing more to do
      return;
    }
    // substitute real plane with sky plane if necessary
    if (RealPlane.splane->pic == skyflatnum && RealPlane.GetNormalZ() < 0.0f) {
      if (splane.splane != &sky_plane) {
        // recreate it, just in case
        sec_surface_t *newsurf = CreateSecSurface(ssurf, sub, RealPlane);
        check(newsurf == ssurf); // sanity check
        ssurf->texinfo.ColourMap = ColourMap; // just in case
        // nothing more to do
        return;
      }
      splane.set(&sky_plane, false);
    }
  }

  ssurf->texinfo.soffs = splane.splane->xoffs;
  ssurf->texinfo.toffs = splane.splane->yoffs+splane.splane->BaseYOffs;

  // if scale/angle was changed, we should update everything, and possibly rebuild the surface
  // our general surface creation function will take care of everything
  if (FASI(ssurf->XScale) != FASI(splane.splane->XScale) ||
      FASI(ssurf->YScale) != FASI(splane.splane->YScale) ||
      ssurf->Angle != splane.splane->BaseAngle-splane.splane->Angle)
  {
    // this will update texture, offsets, and everything
    sec_surface_t *newsurf = CreateSecSurface(ssurf, sub, RealPlane);
    check(newsurf == ssurf); // sanity check
    ssurf->texinfo.ColourMap = ColourMap; // just in case
    // nothing more to do
    return;
  }

  ssurf->texinfo.ColourMap = ColourMap; // just in case

  // ok, we still may need to update texture or z coords
  // update texture?
  VTexture *Tex = GTextureManager(splane.splane->pic);
  if (ssurf->texinfo.Tex != Tex) {
    ssurf->texinfo.Tex = Tex;
    ssurf->texinfo.noDecals = (Tex ? Tex->noDecals : true);
  }

  // update z coords?
  if (FASI(ssurf->edist) != FASI(splane.splane->dist)) {
    ssurf->edist = splane.splane->dist;
    for (surface_t *surf = ssurf->surfs; surf; surf = surf->next) {
      TVec *svert = surf->verts;
      for (int i = surf->count; i--; ++svert) svert->z = splane.GetPointZ(svert->x, svert->y);
    }
    // force lightmap recalculation
    if (splane.splane->pic != skyflatnum) {
      FlushSurfCaches(ssurf->surfs);
      InitSurfs(ssurf->surfs, &ssurf->texinfo, nullptr, sub);
    }
  }
}


//**************************************************************************
//**
//**  Seg surfaces
//**
//**************************************************************************

//==========================================================================
//
//  VRenderLevelShared::NewWSurf
//
//==========================================================================
surface_t *VRenderLevelShared::NewWSurf () {
  enum { WSURFSIZE = sizeof(surface_t)+sizeof(TVec)*(surface_t::MAXWVERTS-1) };
#if 1
  if (!free_wsurfs) {
    // allocate some more surfs
    vuint8 *tmp = (vuint8 *)Z_Calloc(WSURFSIZE*128+sizeof(void *));
    *(void **)tmp = AllocatedWSurfBlocks;
    AllocatedWSurfBlocks = tmp;
    tmp += sizeof(void *);
    for (int i = 0; i < 128; ++i) {
      ((surface_t *)tmp)->next = free_wsurfs;
      free_wsurfs = (surface_t *)tmp;
      tmp += WSURFSIZE;
    }
  }
  surface_t *surf = free_wsurfs;
  free_wsurfs = surf->next;

  memset((void *)surf, 0, WSURFSIZE);
#else
  surface_t *surf = (surface_t *)Z_Calloc(WSURFSIZE);
#endif

  return surf;
}


//==========================================================================
//
//  VRenderLevelShared::FreeWSurfs
//
//==========================================================================
void VRenderLevelShared::FreeWSurfs (surface_t *InSurfs) {
#if 1
  surface_t *surfs = InSurfs;
  FlushSurfCaches(surfs);
  while (surfs) {
    if (surfs->lightmap) Z_Free(surfs->lightmap);
    if (surfs->lightmap_rgb) Z_Free(surfs->lightmap_rgb);
    surface_t *next = surfs->next;
    surfs->next = free_wsurfs;
    free_wsurfs = surfs;
    surfs = next;
  }
#else
  while (InSurfs) {
    surface_t *surf = InSurfs;
    InSurfs = InSurfs->next;
    if (surf->CacheSurf) FreeSurfCache(surf->CacheSurf);
    if (surf->lightmap) Z_Free(surf->lightmap);
    if (surf->lightmap_rgb) Z_Free(surf->lightmap_rgb);
    Z_Free(surf);
  }
#endif
}


//==========================================================================
//
//  VRenderLevelShared::CreateWSurf
//
//  this is used to create world/wall surface
//
//==========================================================================
surface_t *VRenderLevelShared::CreateWSurf (TVec *wv, texinfo_t *texinfo, seg_t *seg, subsector_t *sub, int wvcount) {
  if (wvcount < 3 || (wv[1].z <= wv[0].z && wv[2].z <= wv[3].z)) return nullptr;
  if (wvcount > surface_t::MAXWVERTS) Sys_Error("cannot create huge world surface (the thing that should not be)");

  if (!texinfo->Tex || texinfo->Tex->Type == TEXTYPE_Null) return nullptr;

  surface_t *surf = NewWSurf();
  surf->subsector = sub;
  surf->seg = seg;
  surf->next = nullptr;
  surf->count = wvcount;
  memcpy(surf->verts, wv, wvcount*sizeof(TVec));

  if (texinfo->Tex == GTextureManager[skyflatnum]) {
    // never split sky surfaces
    surf->texinfo = texinfo;
    surf->eplane = seg;
    return surf;
  }

  surf = SubdivideSeg(surf, texinfo->saxis, &texinfo->taxis, seg);
  InitSurfs(surf, texinfo, seg, sub);
  return surf;
}


//==========================================================================
//
//  VRenderLevelShared::CountSegParts
//
//==========================================================================
int VRenderLevelShared::CountSegParts (const seg_t *seg) {
  if (!seg->linedef) return 0; // miniseg
  if (!seg->backsector) return 2;
  int count = 4;
  for (const sec_region_t *reg = seg->backsector->topregion; reg->prev; reg = reg->prev) ++count;
  return count;
}


//==========================================================================
//
//  FixTexturePeg
//
//==========================================================================
static inline void FixTexturePegMid (const seg_t *seg, segpart_t *sp, VTexture *MTex, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  const line_t *linedef = seg->linedef;
  if (linedef->flags&ML_DONTPEGBOTTOM) {
    // bottom of texture at bottom
    sp->texinfo.toffs = r_floor.splane->TexZ+(MTex->GetScaledHeight()*seg->sidedef->MidScaleY);
  } else if (linedef->flags&ML_DONTPEGTOP) {
    // top of texture at top of top region
    sp->texinfo.toffs = seg->front_sub->sector->topregion->eceiling.splane->TexZ;
  } else {
    // top of texture at top
    sp->texinfo.toffs = r_ceiling.splane->TexZ;
  }
  sp->texinfo.toffs *= TextureTScale(MTex)*seg->sidedef->MidScaleY;
  sp->texinfo.toffs += seg->sidedef->MidRowOffset*(TextureOffsetTScale(MTex)*seg->sidedef->MidScaleY);
}


//==========================================================================
//
//  FixTexturePegTop
//
//==========================================================================
static inline void FixTexturePegTop (const seg_t *seg, segpart_t *sp, VTexture *TTex, const sec_plane_t *back_ceiling, float top_TexZ) {
  if (seg->linedef->flags&ML_DONTPEGTOP) {
    // top of texture at top
    sp->texinfo.toffs = top_TexZ;
  } else {
    // bottom of texture
    sp->texinfo.toffs = back_ceiling->TexZ+(TTex->GetScaledHeight()*seg->sidedef->MidScaleY);
  }
  sp->texinfo.toffs *= TextureTScale(TTex)*seg->sidedef->MidScaleY;
  sp->texinfo.toffs += seg->sidedef->TopRowOffset*(TextureOffsetTScale(TTex)*seg->sidedef->MidScaleY);
}


//==========================================================================
//
//  FixTexturePegBot
//
//==========================================================================
static inline void FixTexturePegBot (const seg_t *seg, segpart_t *sp, VTexture *BTex, const sec_plane_t *back_floor, float top_TexZ) {
  if (seg->linedef->flags&ML_DONTPEGBOTTOM) {
    // bottom of texture at bottom
    // top of texture at top
    sp->texinfo.toffs = top_TexZ;
  } else {
    // top of texture at top
    sp->texinfo.toffs = back_floor->TexZ;
  }
  sp->texinfo.toffs *= TextureTScale(BTex)*seg->sidedef->BotScaleY;
  sp->texinfo.toffs += seg->sidedef->BotRowOffset*(TextureOffsetTScale(BTex)*seg->sidedef->BotScaleY);
}


//==========================================================================
//
//  FixPegZOrgMid
//
//==========================================================================
static inline float FixPegZOrgMid (const seg_t *seg, segpart_t *sp, VTexture *MTex, const float texh) {
  float z_org;
  if (seg->linedef->flags&ML_DONTPEGBOTTOM) {
    // bottom of texture at bottom
    // top of texture at top
    z_org = MAX(seg->frontsector->floor.TexZ, seg->backsector->floor.TexZ)+texh;
  } else {
    // top of texture at top
    z_org = MIN(seg->frontsector->ceiling.TexZ, seg->backsector->ceiling.TexZ);
  }
  z_org += seg->sidedef->MidRowOffset*(!MTex->bWorldPanning ? 1.0f : 1.0f/MTex->TScale);
  sp->texinfo.toffs = z_org*(TextureTScale(MTex)*seg->sidedef->MidScaleY);
  return z_org;
}


//==========================================================================
//
//  VRenderLevelShared::SetupOneSidedWSurf
//
//==========================================================================
void VRenderLevelShared::SetupOneSidedWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, VTexture *MTex, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  TVec wv[4];

  FixTexturePegMid(seg, sp, MTex, r_floor, r_ceiling);

  wv[0].x = wv[1].x = seg->v1->x;
  wv[0].y = wv[1].y = seg->v1->y;
  wv[2].x = wv[3].x = seg->v2->x;
  wv[2].y = wv[3].y = seg->v2->y;

  const float topz1 = r_ceiling.GetPointZ(*seg->v1);
  const float topz2 = r_ceiling.GetPointZ(*seg->v2);
  const float botz1 = r_floor.GetPointZ(*seg->v1);
  const float botz2 = r_floor.GetPointZ(*seg->v2);

  wv[0].z = botz1;
  wv[1].z = topz1;
  wv[2].z = topz2;
  wv[3].z = botz2;

  sp->surfs = CreateWSurf(wv, &sp->texinfo, seg, sub);

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->RowOffset = seg->sidedef->MidRowOffset;
}


//==========================================================================
//
//  VRenderLevelShared::SetupOneSidedSkyWSurf
//
//==========================================================================
void VRenderLevelShared::SetupOneSidedSkyWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  TVec wv[4];

  const float topz1 = r_ceiling.GetPointZ(*seg->v1);
  const float topz2 = r_ceiling.GetPointZ(*seg->v2);

  wv[0].x = wv[1].x = seg->v1->x;
  wv[0].y = wv[1].y = seg->v1->y;
  wv[2].x = wv[3].x = seg->v2->x;
  wv[2].y = wv[3].y = seg->v2->y;

  wv[0].z = topz1;
  wv[1].z = wv[2].z = skyheight;
  wv[3].z = topz2;

  sp->surfs = CreateWSurf(wv, &sp->texinfo, seg, sub);

  sp->frontTopDist = r_ceiling.splane->dist;
}


//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedSkyWSurf
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedSkyWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  TVec wv[4];

  const float topz1 = r_ceiling.GetPointZ(*seg->v1);
  const float topz2 = r_ceiling.GetPointZ(*seg->v2);

  sp->texinfo.Tex = GTextureManager[skyflatnum];
  sp->texinfo.noDecals = (sp->texinfo.Tex ? sp->texinfo.Tex->noDecals : true);
  sp->texinfo.Alpha = 1.1f;
  sp->texinfo.Additive = false;
  sp->texinfo.ColourMap = 0;

  wv[0].x = wv[1].x = seg->v1->x;
  wv[0].y = wv[1].y = seg->v1->y;
  wv[2].x = wv[3].x = seg->v2->x;
  wv[2].y = wv[3].y = seg->v2->y;

  wv[0].z = topz1;
  wv[1].z = wv[2].z = skyheight;
  wv[3].z = topz2;

  sp->surfs = CreateWSurf(wv, &sp->texinfo, seg, sub);

  sp->frontTopDist = r_ceiling.splane->dist;
}


//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedTopWSurf
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedTopWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, VTexture *TTex, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  check(TTex);
  TVec wv[4];

  sec_plane_t *back_floor = &seg->backsector->floor;
  sec_plane_t *back_ceiling = &seg->backsector->ceiling;
  if (seg->backsector->fakefloors) {
    if (back_floor == &seg->backsector->floor) back_floor = &seg->backsector->fakefloors->floorplane;
    if (back_ceiling == &seg->backsector->ceiling) back_ceiling = &seg->backsector->fakefloors->ceilplane;
  }

  const float topz1 = r_ceiling.GetPointZ(*seg->v1);
  const float topz2 = r_ceiling.GetPointZ(*seg->v2);
  const float botz1 = r_floor.GetPointZ(*seg->v1);
  const float botz2 = r_floor.GetPointZ(*seg->v2);

  const float back_topz1 = back_ceiling->GetPointZ(*seg->v1);
  const float back_topz2 = back_ceiling->GetPointZ(*seg->v2);

  // hack to allow height changes in outdoor areas
  float top_topz1 = topz1;
  float top_topz2 = topz2;
  float top_TexZ = r_ceiling.splane->TexZ;
  if (IsSky(r_ceiling.splane) && IsSky(back_ceiling) && r_ceiling.splane->SkyBox == back_ceiling->SkyBox) {
    top_topz1 = back_topz1;
    top_topz2 = back_topz2;
    top_TexZ = back_ceiling->TexZ;
  }

  FixTexturePegTop(seg, sp, TTex, back_ceiling, top_TexZ);

  wv[0].x = wv[1].x = seg->v1->x;
  wv[0].y = wv[1].y = seg->v1->y;
  wv[2].x = wv[3].x = seg->v2->x;
  wv[2].y = wv[3].y = seg->v2->y;

  wv[0].z = MAX(back_topz1, botz1);
  wv[1].z = top_topz1;
  wv[2].z = top_topz2;
  wv[3].z = MAX(back_topz2, botz2);

  sp->surfs = CreateWSurf(wv, &sp->texinfo, seg, sub);

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = back_ceiling->dist;
  sp->RowOffset = seg->sidedef->TopRowOffset;
}


//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedBotWSurf
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedBotWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, VTexture *BTex, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  check(BTex);
  TVec wv[4];

  sec_plane_t *back_floor = &seg->backsector->floor;
  sec_plane_t *back_ceiling = &seg->backsector->ceiling;
  if (seg->backsector->fakefloors) {
    if (back_floor == &seg->backsector->floor) back_floor = &seg->backsector->fakefloors->floorplane;
    if (back_ceiling == &seg->backsector->ceiling) back_ceiling = &seg->backsector->fakefloors->ceilplane;
  }

  float topz1 = r_ceiling.GetPointZ(*seg->v1);
  float topz2 = r_ceiling.GetPointZ(*seg->v2);
  float botz1 = r_floor.GetPointZ(*seg->v1);
  float botz2 = r_floor.GetPointZ(*seg->v2);
  float top_TexZ = r_ceiling.splane->TexZ;

  float back_botz1 = back_floor->GetPointZ(*seg->v1);
  float back_botz2 = back_floor->GetPointZ(*seg->v2);

  // hack to allow height changes in outdoor areas
  if (IsSky(r_ceiling.splane) && IsSky(back_ceiling)) {
    topz1 = back_ceiling->GetPointZ(*seg->v1);
    topz2 = back_ceiling->GetPointZ(*seg->v2);
    top_TexZ = back_ceiling->TexZ;
  }

  FixTexturePegBot(seg, sp, BTex, back_floor, top_TexZ);

  wv[0].x = wv[1].x = seg->v1->x;
  wv[0].y = wv[1].y = seg->v1->y;
  wv[2].x = wv[3].x = seg->v2->x;
  wv[2].y = wv[3].y = seg->v2->y;

  wv[0].z = botz1;
  wv[1].z = MIN(back_botz1, topz1);
  wv[2].z = MIN(back_botz2, topz2);
  wv[3].z = botz2;

  sp->surfs = CreateWSurf(wv, &sp->texinfo, seg, sub);

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backBotDist = back_floor->dist;
  sp->RowOffset = seg->sidedef->BotRowOffset;
}


//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedMidWSurf
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedMidWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, VTexture *MTex, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  check(MTex);
  TVec wv[4];

  sec_plane_t *back_floor = &seg->backsector->floor;
  sec_plane_t *back_ceiling = &seg->backsector->ceiling;
  if (seg->backsector->fakefloors) {
    if (back_floor == &seg->backsector->floor) back_floor = &seg->backsector->fakefloors->floorplane;
    if (back_ceiling == &seg->backsector->ceiling) back_ceiling = &seg->backsector->fakefloors->ceilplane;
  }

  float topz1 = r_ceiling.GetPointZ(*seg->v1);
  float topz2 = r_ceiling.GetPointZ(*seg->v2);
  float botz1 = r_floor.GetPointZ(*seg->v1);
  float botz2 = r_floor.GetPointZ(*seg->v2);

  float back_topz1 = back_ceiling->GetPointZ(*seg->v1);
  float back_topz2 = back_ceiling->GetPointZ(*seg->v2);
  float back_botz1 = back_floor->GetPointZ(*seg->v1);
  float back_botz2 = back_floor->GetPointZ(*seg->v2);

  float midtopz1 = topz1;
  float midtopz2 = topz2;
  float midbotz1 = botz1;
  float midbotz2 = botz2;

  if (topz1 > back_topz1 && seg->sidedef->TopTexture > 0) {
    midtopz1 = back_topz1;
    midtopz2 = back_topz2;
  }

  if (botz1 < back_botz1 && seg->sidedef->BottomTexture > 0) {
    midbotz1 = back_botz1;
    midbotz2 = back_botz2;
  }

  float texh = MTex->GetScaledHeight();

  TVec segdir;
  if (seg->length <= 0.0f) {
    GCon->Logf(NAME_Warning, "Seg #%d for linedef #%d has zero length", (int)(ptrdiff_t)(seg-Level->Segs), (int)(ptrdiff_t)(seg->linedef-Level->Lines));
    segdir = TVec(1, 0, 0); // arbitrary
  } else {
    //segdir = (*seg->v2-*seg->v1)/seg->length;
    //segdir = (seg->v2->sub2D(*seg->v1))/seg->length;
    segdir = (*seg->v2-*seg->v1).normalised2D();
  }

  sp->texinfo.saxis = segdir*(TextureSScale(MTex)*seg->sidedef->MidScaleX);
  sp->texinfo.taxis = TVec(0, 0, -1)*(TextureTScale(MTex)*seg->sidedef->MidScaleY);
  sp->texinfo.soffs = -DotProduct(*seg->v1, sp->texinfo.saxis)+
                      seg->offset*(TextureSScale(MTex)*seg->sidedef->MidScaleX)+
                      seg->sidedef->MidTextureOffset*(TextureOffsetSScale(MTex)*seg->sidedef->MidScaleX);
  sp->texinfo.Alpha = seg->linedef->alpha;
  sp->texinfo.Additive = !!(seg->linedef->flags&ML_ADDITIVE);

  float z_org = FixPegZOrgMid(seg, sp, MTex, texh);

  wv[0].x = wv[1].x = seg->v1->x;
  wv[0].y = wv[1].y = seg->v1->y;
  wv[2].x = wv[3].x = seg->v2->x;
  wv[2].y = wv[3].y = seg->v2->y;

  float hgts[4];

  if ((seg->linedef->flags&ML_WRAP_MIDTEX) || (seg->sidedef->Flags&SDF_WRAPMIDTEX)) {
    if ((seg->linedef->flags&ML_CLIP_MIDTEX) || (seg->sidedef->Flags&SDF_CLIPMIDTEX)) {
      //k8: this is totally wrong
      hgts[0] = MAX(midbotz1, z_org-texh);
      hgts[1] = MIN(midtopz1, z_org);
      hgts[2] = MIN(midtopz2, z_org);
      hgts[3] = MAX(midbotz2, z_org-texh);
    } else {
      hgts[0] = midbotz1;
      hgts[1] = midtopz1;
      hgts[2] = midtopz2;
      hgts[3] = midbotz2;
    }
  } else {
    hgts[0] = MAX(midbotz1, z_org-texh);
    hgts[1] = MIN(midtopz1, z_org);
    hgts[2] = MIN(midtopz2, z_org);
    hgts[3] = MAX(midbotz2, z_org-texh);
  }

  wv[0].z = hgts[0];
  wv[1].z = hgts[1];
  wv[2].z = hgts[2];
  wv[3].z = hgts[3];

  sp->surfs = CreateWSurf(wv, &sp->texinfo, seg, sub);

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = back_ceiling->dist;
  sp->backBotDist = back_floor->dist;
  sp->RowOffset = seg->sidedef->MidRowOffset;
}


//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedMidExtraWSurf
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedMidExtraWSurf (sec_region_t *reg, subsector_t *sub, seg_t *seg, segpart_t *sp, VTexture *MTextr,
                                                     TSecPlaneRef r_floor, TSecPlaneRef r_ceiling,
                                                     TSecPlaneRef extratop, TSecPlaneRef extrabot, bool createES)
{
  check(MTextr);

  if (createES) {
    TVec wv[4];

    const float topz1 = r_ceiling.GetPointZ(*seg->v1);
    const float topz2 = r_ceiling.GetPointZ(*seg->v2);
    const float botz1 = r_floor.GetPointZ(*seg->v1);
    const float botz2 = r_floor.GetPointZ(*seg->v2);

    const float extratopz1 = extratop.GetPointZ(*seg->v1);
    const float extratopz2 = extratop.GetPointZ(*seg->v2);
    const float extrabotz1 = extrabot.GetPointZ(*seg->v1);
    const float extrabotz2 = extrabot.GetPointZ(*seg->v2);

    wv[0].x = wv[1].x = seg->v1->x;
    wv[0].y = wv[1].y = seg->v1->y;
    wv[2].x = wv[3].x = seg->v2->x;
    wv[2].y = wv[3].y = seg->v2->y;

    wv[0].z = MAX(extrabotz1, botz1);
    wv[1].z = MIN(extratopz1, topz1);
    wv[2].z = MIN(extratopz2, topz2);
    wv[3].z = MAX(extrabotz2, botz2);

#if defined(GOZZO_3DSHIT_TEMP_2S_HACK)
    TVec wv2[4];
    wv2[0] = wv[3];
    wv2[1] = wv[2];
    wv2[2] = wv[1];
    wv2[3] = wv[0];
#endif

    /*
    GCon->Logf("extra: %f, %f, %f, %f", wv[0].z, wv[1].z, wv[2].z, wv[3].z);
    GCon->Logf("       ez1=(%f,%f); ez2=(%f,%f); bz1=(%f,%f); bz2=(%f,%f)", extrabotz1, extratopz1, extrabotz2, extratopz2, botz1, topz1, botz2, topz2);
    for (int f = 0; f < 4; ++f) GCon->Logf("       %d: (%f,%f,%f)", f, wv[f].x, wv[f].y, wv[f].z);
    */

    if (wv[0].z == wv[1].z && wv[1].z == wv[2].z && wv[2].z == wv[3].z) {
      // degenerate side surface, no need to create it
    } if (wv[0].z == wv[1].z && wv[2].z == wv[3].z) {
      // degenerate side surface (thin line), cannot create it (no render support)
    } if (wv[0].z == wv[1].z) {
      // can reduce to triangle
      sp->surfs = CreateWSurf(wv+1, &sp->texinfo, seg, sub, 3);
    } if (wv[2].z == wv[3].z) {
      // can reduce to triangle
      sp->surfs = CreateWSurf(wv, &sp->texinfo, seg, sub, 3);
    } else {
      sp->surfs = CreateWSurf(wv, &sp->texinfo, seg, sub, 4);
    }

#if defined(GOZZO_3DSHIT_TEMP_2S_HACK)
    surface_t *s2 = nullptr;
    if (wv2[0].z == wv2[1].z && wv2[1].z == wv2[2].z && wv2[2].z == wv2[3].z) {
      // degenerate side surface, no need to create it
    } if (wv2[0].z == wv2[1].z && wv2[2].z == wv2[3].z) {
      // degenerate side surface (thin line), cannot create it (no render support)
    } if (wv2[0].z == wv2[1].z) {
      // can reduce to triangle
      s2 = CreateWSurf(wv2+1, &sp->texinfo, seg, sub, 3);
    } if (wv2[2].z == wv2[3].z) {
      // can reduce to triangle
      s2 = CreateWSurf(wv2, &sp->texinfo, seg, sub, 3);
    } else {
      s2 = CreateWSurf(wv2, &sp->texinfo, seg, sub, 4);
    }

    if (s2) {
      surface_t *slast = sp->surfs;
      if (slast) {
        while (slast->next) slast = slast->next;
        slast->next = s2;
      } else {
        sp->surfs = s2;
      }
    }
#else
    for (surface_t *sf = sp->surfs; sf; sf = sf->next) sf->drawflags |= surface_t::DF_NO_FACE_CULL;
#endif
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = extratop.splane->dist;
  sp->backBotDist = extrabot.splane->dist;
  sp->RowOffset = seg->sidedef->MidRowOffset;
}


//==========================================================================
//
//  GetExtraTopBot
//
//==========================================================================
static inline void GetExtraTopBot (VLevel *Level, sec_region_t *reg, TSecPlaneRef &extratop, TSecPlaneRef &extrabot, side_t *&extraside, bool fromtop) {
  if (reg->regflags&sec_region_t::RF_NonSolid) {
    extratop = reg->eceiling; // new floor
    extrabot = reg->efloor; // new ceiling
    extraside = (reg->extraline ? &Level->Sides[reg->extraline->sidenum[0]] : nullptr);
    //if (extraside && reg->extraline->sidenum[1] != -1) GCon->Logf("EXTRA WITH TWO SIDES!");
  } else if (fromtop) {
    // creating
    extratop = reg->efloor; // new floor
    extrabot = reg->prev->eceiling; // new ceiling
    extraside = (reg->prev->extraline ? &Level->Sides[reg->prev->extraline->sidenum[0]] : nullptr);
    //if (extraside && reg->prev->extraline->sidenum[1] != -1) GCon->Logf("EXTRA WITH TWO SIDES!");
  } else {
    // updating
    extratop = reg->next->efloor; // new floor
    extrabot = reg->eceiling; // new ceiling
    extraside = (reg->extraline ? &Level->Sides[reg->extraline->sidenum[0]] : nullptr);
    //if (extraside && reg->extraline->sidenum[1] != -1) GCon->Logf("EXTRA WITH TWO SIDES!");
  }
}


//==========================================================================
//
//  VRenderLevelShared::CreateSegParts
//
//  create world/wall surfaces
//
//==========================================================================
void VRenderLevelShared::CreateSegParts (subsector_t *sub, drawseg_t *dseg, seg_t *seg, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  segpart_t *sp;

  dseg->seg = seg;
  dseg->next = seg->drawsegs;
  seg->drawsegs = dseg;

  const line_t *linedef = seg->linedef;
  if (!linedef) return; // miniseg
  const side_t *sidedef = seg->sidedef;

  TVec segdir;
  if (seg->length <= 0.0f) {
    GCon->Logf(NAME_Warning, "Seg #%d for linedef #%d has zero length", (int)(ptrdiff_t)(seg-Level->Segs), (int)(ptrdiff_t)(linedef-Level->Lines));
    segdir = TVec(1, 0, 0); // arbitrary
  } else {
    //segdir = (*seg->v2-*seg->v1)/seg->length;
    //segdir = (seg->v2->sub2D(*seg->v1))/seg->length;
    segdir = (*seg->v2-*seg->v1).normalised2D();
  }

  if (!seg->backsector) {
    dseg->mid = pspart++;
    sp = dseg->mid;

    VTexture *MTex = GTextureManager(sidedef->MidTexture);
    // k8: one-sided line should have a midtex
    if (!MTex) {
      GCon->Logf(NAME_Warning, "Sidedef #%d should have midtex, but it hasn't (%d)", (int)(ptrdiff_t)(sidedef-Level->Sides), sidedef->MidTexture.id);
      MTex = GTextureManager[GTextureManager.DefaultTexture];
    }
    sp->texinfo.saxis = segdir*(TextureSScale(MTex)*seg->sidedef->MidScaleX);
    sp->texinfo.taxis = TVec(0, 0, -1)*(TextureTScale(MTex)*seg->sidedef->MidScaleY);
    sp->texinfo.soffs = -DotProduct(*seg->v1, sp->texinfo.saxis)+
                        seg->offset*(TextureSScale(MTex)*seg->sidedef->MidScaleX)+
                        sidedef->MidTextureOffset*(TextureOffsetSScale(MTex)*seg->sidedef->MidScaleX);
    sp->texinfo.Tex = MTex;
    sp->texinfo.noDecals = (MTex ? MTex->noDecals : true);
    sp->texinfo.Alpha = 1.1f;
    sp->texinfo.Additive = false;
    sp->texinfo.ColourMap = 0;

    sp->TextureOffset = sidedef->MidTextureOffset;

    SetupOneSidedWSurf(sub, seg, sp, MTex, r_floor, r_ceiling);

    // sky above line
    dseg->topsky = pspart++;
    sp = dseg->topsky;
    sp->texinfo.Tex = GTextureManager[skyflatnum];
    sp->texinfo.noDecals = (sp->texinfo.Tex ? sp->texinfo.Tex->noDecals : true);
    sp->texinfo.Alpha = 1.1f;
    sp->texinfo.Additive = false;
    sp->texinfo.ColourMap = 0;
    if (IsSky(r_ceiling.splane)) SetupOneSidedSkyWSurf(sub, seg, sp, r_floor, r_ceiling);
  } else {
    // two sided line
    sec_plane_t *back_floor = &seg->backsector->floor;
    sec_plane_t *back_ceiling = &seg->backsector->ceiling;
    if (seg->backsector->fakefloors) {
      if (back_floor == &seg->backsector->floor) back_floor = &seg->backsector->fakefloors->floorplane;
      if (back_ceiling == &seg->backsector->ceiling) back_ceiling = &seg->backsector->fakefloors->ceilplane;
    }

    VTexture *TTex = GTextureManager(sidedef->TopTexture);
    if (IsSky(r_ceiling.splane) && IsSky(back_ceiling) && r_ceiling.splane->SkyBox != back_ceiling->SkyBox) {
      TTex = GTextureManager[skyflatnum];
    }
    check(TTex);

    // top wall
    dseg->top = pspart++;
    sp = dseg->top;


    sp->texinfo.saxis = segdir*(TextureSScale(TTex)*seg->sidedef->TopScaleX);
    sp->texinfo.taxis = TVec(0, 0, -1)*(TextureTScale(TTex)*seg->sidedef->TopScaleY);
    sp->texinfo.soffs = -DotProduct(*seg->v1, sp->texinfo.saxis)+
                        seg->offset*(TextureSScale(TTex)*seg->sidedef->TopScaleX)+
                        sidedef->TopTextureOffset*(TextureOffsetSScale(TTex)*seg->sidedef->TopScaleX);
    sp->texinfo.Tex = TTex;
    sp->texinfo.noDecals = (TTex ? TTex->noDecals : true);
    sp->texinfo.Alpha = 1.1f;
    sp->texinfo.Additive = false;
    sp->texinfo.ColourMap = 0;

    SetupTwoSidedTopWSurf(sub, seg, sp, TTex, r_floor, r_ceiling);
    sp->backBotDist = back_floor->dist;
    sp->TextureOffset = sidedef->TopTextureOffset;

    // sky above top
    dseg->topsky = pspart++;
    if (IsSky(r_ceiling.splane) && !IsSky(back_ceiling)) {
      sp = dseg->topsky;
      SetupTwoSidedSkyWSurf(sub, seg, sp, r_floor, r_ceiling);
    }

    // bottom wall
    dseg->bot = pspart++;
    sp = dseg->bot;

    VTexture *BTex = GTextureManager(sidedef->BottomTexture);
    check(BTex);
    sp->texinfo.saxis = segdir*(TextureSScale(BTex)*seg->sidedef->BotScaleX);
    sp->texinfo.taxis = TVec(0, 0, -1)*(TextureTScale(BTex)*seg->sidedef->BotScaleY);
    sp->texinfo.soffs = -DotProduct(*seg->v1, sp->texinfo.saxis)+
                        seg->offset*(TextureSScale(BTex)*seg->sidedef->BotScaleX)+
                        sidedef->BotTextureOffset*(TextureOffsetSScale(BTex)*seg->sidedef->BotScaleX);
    sp->texinfo.Tex = BTex;
    sp->texinfo.noDecals = (BTex ? BTex->noDecals : true);
    sp->texinfo.Alpha = 1.1f;
    sp->texinfo.Additive = false;
    sp->texinfo.ColourMap = 0;


    SetupTwoSidedBotWSurf(sub, seg, sp, BTex, r_floor, r_ceiling);
    sp->backTopDist = back_ceiling->dist;
    sp->TextureOffset = sidedef->BotTextureOffset;


    dseg->mid = pspart++;
    sp = dseg->mid;

    // middle wall
    VTexture *MTex = GTextureManager(sidedef->MidTexture);
    check(MTex);
    sp->texinfo.Tex = MTex;
    sp->texinfo.noDecals = (MTex ? MTex->noDecals : true);
    sp->texinfo.ColourMap = 0;
    if (MTex->Type != TEXTYPE_Null) {
      // masked MidTexture
      SetupTwoSidedMidWSurf(sub, seg, sp, MTex, r_floor, r_ceiling);
    }
    sp->TextureOffset = sidedef->MidTextureOffset;

    for (sec_region_t *reg = seg->backsector->topregion; reg->prev; reg = reg->prev) {
      TSecPlaneRef extratop, extrabot;
      side_t *extraside;

      GetExtraTopBot(Level, reg, extratop, extrabot, extraside, true); // from top
      if (!extraside) continue; // no need to create extra side

      sp = pspart++;
      sp->next = dseg->extra;
      dseg->extra = sp;

      VTexture *MTextr = GTextureManager(extraside->MidTexture);
      sp->texinfo.saxis = segdir*(TextureSScale(MTextr)*seg->sidedef->MidScaleX);
      sp->texinfo.taxis = TVec(0, 0, -1)*(TextureTScale(MTextr)*seg->sidedef->MidScaleY);
      sp->texinfo.soffs = -DotProduct(*seg->v1, sp->texinfo.saxis)+
                          seg->offset*(TextureSScale(MTextr)*seg->sidedef->MidScaleX)+
                          sidedef->MidTextureOffset*(TextureOffsetSScale(MTextr)*seg->sidedef->MidScaleX);
      sp->texinfo.toffs = extratop.splane->TexZ*(TextureTScale(MTextr)*seg->sidedef->MidScaleY)+
                          sidedef->MidRowOffset*(TextureOffsetTScale(MTextr)*seg->sidedef->MidScaleY);
      sp->texinfo.Tex = MTextr;
      sp->texinfo.noDecals = (MTextr ? MTextr->noDecals : true);
      sp->texinfo.Alpha = (extrabot.splane->Alpha < 1.0f ? extrabot.splane->Alpha : 1.1f);
      sp->texinfo.Additive = !!(extrabot.splane->flags&SPF_ADDITIVE);
      sp->texinfo.ColourMap = 0;

      SetupTwoSidedMidExtraWSurf(reg, sub, seg, sp, MTextr, r_floor, r_ceiling, extratop, extrabot, true);
      sp->TextureOffset = sidedef->MidTextureOffset;
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::UpdateRowOffset
//
//==========================================================================
void VRenderLevelShared::UpdateRowOffset (subsector_t *sub, segpart_t *sp, float RowOffset, float Scale) {
  sp->texinfo.toffs += (RowOffset-sp->RowOffset)*(TextureOffsetTScale(sp->texinfo.Tex)*Scale);
  sp->RowOffset = RowOffset*Scale;
  FlushSurfCaches(sp->surfs);
  InitSurfs(sp->surfs, &sp->texinfo, nullptr, sub);
}


//==========================================================================
//
//  VRenderLevelShared::UpdateTextureOffset
//
//==========================================================================
void VRenderLevelShared::UpdateTextureOffset (subsector_t *sub, segpart_t *sp, float TextureOffset, float Scale) {
  sp->texinfo.soffs += (TextureOffset-sp->TextureOffset)*(TextureOffsetSScale(sp->texinfo.Tex)*Scale);
  sp->TextureOffset = TextureOffset*Scale;
  FlushSurfCaches(sp->surfs);
  InitSurfs(sp->surfs, &sp->texinfo, nullptr, sub);
}


//==========================================================================
//
//  VRenderLevelShared::UpdateDrawSeg
//
//==========================================================================
void VRenderLevelShared::UpdateDrawSeg (subsector_t *sub, drawseg_t *dseg, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling/*, bool ShouldClip*/) {
  seg_t *seg = dseg->seg;
  segpart_t *sp;

  if (!seg->linedef) return; // miniseg

  if (w_update_clip_region /*ShouldClip*/) {
    /*
    k8: i don't know what Janis wanted to accomplish with this, but it actually
        makes clipping WORSE due to limited precision
    // clip sectors that are behind rendered segs
    TVec v1 = *seg->v1;
    TVec v2 = *seg->v2;
    TVec r1 = vieworg-v1;
    TVec r2 = vieworg-v2;
    float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), vieworg);
    float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), vieworg);

    // there might be a better method of doing this, but this one works for now...
         if (D1 > 0.0f && D2 < 0.0f) v2 += (v2-v1)*D1/(D1-D2);
    else if (D2 > 0.0f && D1 < 0.0f) v1 += (v1-v2)*D2/(D2-D1);

    if (!ViewClip.IsRangeVisible(ViewClip.PointToClipAngle(v2), ViewClip.PointToClipAngle(v1))) return;
    */
    if (!seg->PointOnSide(vieworg)) {
      if (!ViewClip.IsRangeVisible(*seg->v2, *seg->v1)) return;
    } else {
      if (!ViewClip.IsRangeVisible(*seg->v1, *seg->v2)) return;
    }
  }

  side_t *sidedef = seg->sidedef;
  line_t *linedef = seg->linedef;

  if (!seg->backsector) {
    sp = dseg->mid;
    sp->texinfo.ColourMap = ColourMap;
    VTexture *MTex = GTextureManager(sidedef->MidTexture);
    if (FASI(sp->frontTopDist) != FASI(r_ceiling.splane->dist) ||
        FASI(sp->frontBotDist) != FASI(r_floor.splane->dist) ||
        sp->texinfo.Tex->SScale != MTex->SScale ||
        sp->texinfo.Tex->TScale != MTex->TScale ||
        sp->texinfo.Tex->GetHeight() != MTex->GetHeight())
    {
      FreeWSurfs(sp->surfs);
      sp->surfs = nullptr;

      sp->texinfo.Tex = MTex;
      sp->texinfo.noDecals = (MTex ? MTex->noDecals : true);

      SetupOneSidedWSurf(sub, seg, sp, MTex, r_floor, r_ceiling);
    } else if (FASI(sp->RowOffset) != FASI(sidedef->MidRowOffset)) {
      sp->texinfo.Tex = MTex;
      sp->texinfo.noDecals = (MTex ? MTex->noDecals : true);
      UpdateRowOffset(sub, sp, sidedef->MidRowOffset, seg->sidedef->MidScaleY);
    } else {
      sp->texinfo.Tex = MTex;
      sp->texinfo.noDecals = (MTex ? MTex->noDecals : true);
    }

    if (FASI(sp->TextureOffset) != FASI(sidedef->MidTextureOffset)) {
      UpdateTextureOffset(sub, sp, sidedef->MidTextureOffset, seg->sidedef->MidScaleX);
    }

    sp = dseg->topsky;
    sp->texinfo.ColourMap = ColourMap;
    if (IsSky(r_ceiling.splane) && FASI(sp->frontTopDist) != FASI(r_ceiling.splane->dist)) {
      FreeWSurfs(sp->surfs);
      sp->surfs = nullptr;

      SetupOneSidedSkyWSurf(sub, seg, sp, r_floor, r_ceiling);
    }
  } else {
    sec_plane_t *back_floor = &seg->backsector->floor;
    sec_plane_t *back_ceiling = &seg->backsector->ceiling;
    if (seg->backsector->fakefloors) {
      if (back_floor == &seg->backsector->floor) back_floor = &seg->backsector->fakefloors->floorplane;
      if (back_ceiling == &seg->backsector->ceiling) back_ceiling = &seg->backsector->fakefloors->ceilplane;
    }

    // top wall
    sp = dseg->top;
    sp->texinfo.ColourMap = ColourMap;
    VTexture *TTex = GTextureManager(sidedef->TopTexture);
    if (IsSky(r_ceiling.splane) && IsSky(back_ceiling) && r_ceiling.splane->SkyBox != back_ceiling->SkyBox) {
      TTex = GTextureManager[skyflatnum];
    }

    if (FASI(sp->frontTopDist) != FASI(r_ceiling.splane->dist) ||
        FASI(sp->frontBotDist) != FASI(r_floor.splane->dist) ||
        FASI(sp->backTopDist) != FASI(back_ceiling->dist) ||
        sp->texinfo.Tex->SScale != TTex->SScale ||
        sp->texinfo.Tex->TScale != TTex->TScale)
    {
      FreeWSurfs(sp->surfs);
      sp->surfs = nullptr;

      sp->texinfo.Tex = TTex;
      sp->texinfo.noDecals = (TTex ? TTex->noDecals : true);

      SetupTwoSidedTopWSurf(sub, seg, sp, TTex, r_floor, r_ceiling);
    } else if (FASI(sp->RowOffset) != FASI(sidedef->TopRowOffset)) {
      sp->texinfo.Tex = TTex;
      sp->texinfo.noDecals = (TTex ? TTex->noDecals : true);
      UpdateRowOffset(sub, sp, sidedef->TopRowOffset, seg->sidedef->TopScaleY);
    } else {
      sp->texinfo.Tex = TTex;
      sp->texinfo.noDecals = (TTex ? TTex->noDecals : true);
    }

    if (FASI(sp->TextureOffset) != FASI(sidedef->TopTextureOffset)) {
      UpdateTextureOffset(sub, sp, sidedef->TopTextureOffset, seg->sidedef->TopScaleX);
    }

    // sky above top
    sp = dseg->topsky;
    sp->texinfo.ColourMap = ColourMap;
    if (IsSky(r_ceiling.splane) && !IsSky(back_ceiling) && FASI(sp->frontTopDist) != FASI(r_ceiling.splane->dist)) {
      FreeWSurfs(sp->surfs);
      sp->surfs = nullptr;

      SetupTwoSidedSkyWSurf(sub, seg, sp, r_floor, r_ceiling);
    }

    // bottom wall
    sp = dseg->bot;
    sp->texinfo.ColourMap = ColourMap;
    VTexture *BTex = GTextureManager(sidedef->BottomTexture);
    sp->texinfo.Tex = BTex;
    sp->texinfo.noDecals = (sp->texinfo.Tex ? sp->texinfo.Tex->noDecals : true);

    if (FASI(sp->frontTopDist) != FASI(r_ceiling.splane->dist) ||
        FASI(sp->frontBotDist) != FASI(r_floor.splane->dist) ||
        FASI(sp->backBotDist) != FASI(back_floor->dist))
    {
      FreeWSurfs(sp->surfs);
      sp->surfs = nullptr;

      SetupTwoSidedBotWSurf(sub, seg, sp, BTex, r_floor, r_ceiling);
    } else if (FASI(sp->RowOffset) != FASI(sidedef->BotRowOffset)) {
      UpdateRowOffset(sub, sp, sidedef->BotRowOffset, seg->sidedef->BotScaleY);
    }

    if (FASI(sp->TextureOffset) != FASI(sidedef->BotTextureOffset)) {
      UpdateTextureOffset(sub, sp, sidedef->BotTextureOffset, seg->sidedef->BotScaleX);
    }

    // masked MidTexture
    sp = dseg->mid;
    sp->texinfo.ColourMap = ColourMap;
    VTexture *MTex = GTextureManager(sidedef->MidTexture);
    check(MTex);

    if (FASI(sp->frontTopDist) != FASI(r_ceiling.splane->dist) ||
        FASI(sp->frontBotDist) != FASI(r_floor.splane->dist) ||
        FASI(sp->backTopDist) != FASI(back_ceiling->dist) ||
        FASI(sp->backBotDist) != FASI(back_floor->dist) ||
        FASI(sp->RowOffset) != FASI(sidedef->MidRowOffset) ||
        sp->texinfo.Tex->SScale != MTex->SScale ||
        sp->texinfo.Tex->TScale != MTex->TScale ||
        sp->texinfo.Tex->GetHeight() != MTex->GetHeight() ||
        (sp->texinfo.Tex->Type == TEXTYPE_Null) != (MTex->Type == TEXTYPE_Null))
    {
      FreeWSurfs(sp->surfs);
      sp->surfs = nullptr;

      sp->texinfo.Tex = MTex;
      sp->texinfo.noDecals = (sp->texinfo.Tex ? sp->texinfo.Tex->noDecals : true);
      if (MTex->Type != TEXTYPE_Null) {
        // masked MidTexture
        SetupTwoSidedMidWSurf(sub, seg, sp, MTex, r_floor, r_ceiling);
      } else {
        sp->texinfo.Alpha = 1.1f;
        sp->texinfo.Additive = false;
      }

      sp->frontTopDist = r_ceiling.splane->dist;
      sp->frontBotDist = r_floor.splane->dist;
      sp->backTopDist = back_ceiling->dist;
      sp->backBotDist = back_floor->dist;
      sp->RowOffset = sidedef->MidRowOffset;
    } else {
      sp->texinfo.Tex = MTex;
      sp->texinfo.noDecals = (sp->texinfo.Tex ? sp->texinfo.Tex->noDecals : true);
      if (sidedef->MidTexture) {
        sp->texinfo.Alpha = linedef->alpha;
        sp->texinfo.Additive = !!(linedef->flags&ML_ADDITIVE);
      }
    }

    if (FASI(sp->TextureOffset) != FASI(sidedef->MidTextureOffset)) {
      UpdateTextureOffset(sub, sp, sidedef->MidTextureOffset, seg->sidedef->MidScaleX);
    }

    segpart_t *spp = dseg->extra;
    for (sec_region_t *reg = seg->backsector->botregion; reg->next; reg = reg->next) {
      TSecPlaneRef extratop, extrabot;
      side_t *extraside;
      GetExtraTopBot(Level, reg, extratop, extrabot, extraside, false); // from bottom
      if (!extraside) continue; // no need to create extra side (and no extra seg parts were created)

      spp->texinfo.ColourMap = ColourMap;
      VTexture *ETex = GTextureManager(extraside->MidTexture);
      spp->texinfo.Tex = ETex;
      spp->texinfo.noDecals = (spp->texinfo.Tex ? spp->texinfo.Tex->noDecals : true);

      if (FASI(spp->frontTopDist) != FASI(r_ceiling.splane->dist) ||
          FASI(spp->frontBotDist) != FASI(r_floor.splane->dist) ||
          FASI(spp->backTopDist) != FASI(extratop.splane->dist) ||
          FASI(spp->backBotDist) != FASI(extrabot.splane->dist))
      {
        FreeWSurfs(spp->surfs);
        spp->surfs = nullptr;

        spp->texinfo.toffs = extratop.splane->TexZ*(TextureTScale(ETex)*seg->sidedef->MidScaleY)+
                             sidedef->MidRowOffset*(TextureOffsetTScale(ETex)*seg->sidedef->MidScaleY);

        SetupTwoSidedMidExtraWSurf(reg, sub, seg, spp, ETex, r_floor, r_ceiling, extratop, extrabot, true);
      } else if (FASI(spp->RowOffset) != FASI(sidedef->MidRowOffset)) {
        UpdateRowOffset(sub, spp, sidedef->MidRowOffset, seg->sidedef->MidScaleY);
      }

      if (FASI(spp->TextureOffset) != FASI(sidedef->MidTextureOffset)) {
        UpdateTextureOffset(sub, spp, sidedef->MidTextureOffset, seg->sidedef->MidScaleX);
      }

      spp = spp->next;
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::SegMoved
//
//==========================================================================
void VRenderLevelShared::SegMoved (seg_t *seg) {
  if (!seg->drawsegs) return; // drawsegs not created yet
  if (!seg->linedef) Sys_Error("R_SegMoved: miniseg");

  VTexture *Tex = seg->drawsegs->mid->texinfo.Tex;
  seg->drawsegs->mid->texinfo.saxis = (*seg->v2-*seg->v1)/seg->length*(TextureSScale(Tex)*seg->sidedef->MidScaleX);
  seg->drawsegs->mid->texinfo.soffs = -DotProduct(*seg->v1, seg->drawsegs->mid->texinfo.saxis)+
                                      seg->offset*(TextureSScale(Tex)*seg->sidedef->MidScaleX)+
                                      seg->sidedef->MidTextureOffset*(TextureOffsetSScale(Tex)*seg->sidedef->MidScaleX);

  // force update
  seg->drawsegs->mid->frontTopDist += 0.346f;
}


//==========================================================================
//
//  VRenderLevelShared::CreateWorldSurfaces
//
//==========================================================================
void VRenderLevelShared::CreateWorldSurfaces () {
  check(!free_wsurfs);
  SetupSky();

  // set up fake floors
  for (int i = 0; i < Level->NumSectors; ++i) {
    if (Level->Sectors[i].heightsec || Level->Sectors[i].deepref) {
      SetupFakeFloors(&Level->Sectors[i]);
    }
  }

  if (showCreateWorldSurfProgress) {
    if (IsAdvancedRenderer()) {
      R_LdrMsgShowSecondary("CREATING WORLD SURFACES...");
    } else {
      R_LdrMsgShowSecondary("CALCULATING LIGHTMAPS...");
    }
    R_PBarReset();
  }

  // count regions in all subsectors
  int count = 0;
  int dscount = 0;
  int spcount = 0;
  subsector_t *sub = &Level->Subsectors[0];
  for (int i = Level->NumSubsectors; i--; ++sub) {
    if (!sub->sector->linecount) continue; // skip sectors containing original polyobjs
    for (const sec_region_t *reg = sub->sector->botregion; reg; reg = reg->next) {
      ++count;
      dscount += sub->numlines;
      if (sub->poly) dscount += sub->poly->numsegs; // polyobj
      for (int j = 0; j < sub->numlines; ++j) spcount += CountSegParts(&Level->Segs[sub->firstline+j]);
      if (sub->poly) {
        seg_t *const *polySeg = sub->poly->segs;
        for (int polyCount = sub->poly->numsegs; polyCount--; ++polySeg) spcount += CountSegParts(*polySeg);
      }
    }
  }

  // get some memory
  subregion_t *sreg = new subregion_t[count+1];
  drawseg_t *pds = new drawseg_t[dscount+1];
  pspart = new segpart_t[spcount+1];

  memset((void *)sreg, 0, sizeof(subregion_t)*(count+1));
  memset((void *)pds, 0, sizeof(drawseg_t)*(dscount+1));
  memset((void *)pspart, 0, sizeof(segpart_t)*(spcount+1));

  AllocatedSubRegions = sreg;
  AllocatedDrawSegs = pds;
  AllocatedSegParts = pspart;

  // create sector surfaces
  sub = &Level->Subsectors[0];
  for (int i = Level->NumSubsectors; i--; ++sub) {
    if (!sub->sector->linecount) continue; // skip sectors containing original polyobjs

    for (sec_region_t *reg = sub->sector->botregion; reg; reg = reg->next) {
      TSecPlaneRef r_floor, r_ceiling;
      r_floor = reg->efloor;
      r_ceiling = reg->eceiling;

      if (sub->sector->fakefloors) {
        if (r_floor.splane == &sub->sector->floor) r_floor.set(&sub->sector->fakefloors->floorplane, false);
        if (r_ceiling.splane == &sub->sector->ceiling) r_ceiling.set(&sub->sector->fakefloors->ceilplane, false);
      }

      sreg->secregion = reg;
      sreg->floorplane = r_floor;
      sreg->ceilplane = r_ceiling;
      sreg->floor = CreateSecSurface(nullptr, sub, r_floor);
      sreg->ceil = CreateSecSurface(nullptr, sub, r_ceiling);

      sreg->count = sub->numlines;
      if (sub->poly) sreg->count += sub->poly->numsegs; // polyobj
      sreg->lines = pds;
      pds += sreg->count;
      for (int j = 0; j < sub->numlines; ++j) CreateSegParts(sub, &sreg->lines[j], &Level->Segs[sub->firstline+j], r_floor, r_ceiling);
      if (sub->poly) {
        // polyobj
        int j = sub->numlines;
        seg_t **polySeg = sub->poly->segs;
        for (int polyCount = sub->poly->numsegs; polyCount--; ++polySeg, ++j) {
          CreateSegParts(sub, &sreg->lines[j], *polySeg, r_floor, r_ceiling);
        }
      }

      sreg->next = sub->regions;
      sub->regions = sreg;
      ++sreg;
    }

    if (showCreateWorldSurfProgress) R_PBarUpdate("Lighting", Level->NumSubsectors-i, Level->NumSubsectors);
  }

  if (showCreateWorldSurfProgress) R_PBarUpdate("Lighting", Level->NumSubsectors, Level->NumSubsectors, true);
  showCreateWorldSurfProgress = false;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateSubRegion
//
//==========================================================================
void VRenderLevelShared::UpdateSubRegion (subsector_t *sub, subregion_t *region, bool updatePoly) {
  TSecPlaneRef r_floor = region->floorplane;
  TSecPlaneRef r_ceiling = region->ceilplane;

  if (sub->sector->fakefloors) {
    if (r_floor.splane == &sub->sector->floor) r_floor.set(&sub->sector->fakefloors->floorplane, false);
    if (r_ceiling.splane == &sub->sector->ceiling) r_ceiling.set(&sub->sector->fakefloors->ceilplane, false);
  }

  drawseg_t *ds = region->lines;
  for (int count = sub->numlines; count--; ++ds) {
    UpdateDrawSeg(sub, ds, r_floor, r_ceiling/*, ClipSegs*/);
  }

  UpdateSecSurface(region->floor, region->floorplane, sub);
  UpdateSecSurface(region->ceil, region->ceilplane, sub);

  if (updatePoly && sub->poly) {
    // update the polyobj
    updatePoly = false;
    seg_t **polySeg = sub->poly->segs;
    for (int polyCount = sub->poly->numsegs; polyCount--; ++polySeg) {
      UpdateDrawSeg(sub, (*polySeg)->drawsegs, r_floor, r_ceiling/*, ClipSegs*/);
    }
  }

  if (region->next) {
    if (w_update_clip_region && !w_update_in_renderer /*ClipSegs*/) {
      if (!ViewClip.ClipCheckRegion(region->next, sub)) return;
    }
    UpdateSubRegion(sub, region->next, updatePoly);
  }
}


//==========================================================================
//
//  VRenderLevelShared::CopyPlaneIfValid
//
//==========================================================================
bool VRenderLevelShared::CopyPlaneIfValid (sec_plane_t *dest, const sec_plane_t *source, const sec_plane_t *opp) {
  bool copy = false;

  // if the planes do not have matching slopes, then always copy them
  // because clipping would require creating new sectors
  if (source->normal != dest->normal) {
    copy = true;
  } else if (opp->normal != -dest->normal) {
    if (source->dist < dest->dist) copy = true;
  } else if (source->dist < dest->dist && source->dist > -opp->dist) {
    copy = true;
  }

  if (copy) *(TPlane *)dest = *(TPlane *)source;

  return copy;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateFakeFlats
//
//  killough 3/7/98: Hack floor/ceiling heights for deep water etc.
//
//  If player's view height is underneath fake floor, lower the
//  drawn ceiling to be just under the floor height, and replace
//  the drawn floor and ceiling textures, and light level, with
//  the control sector's.
//
//  Similar for ceiling, only reflected.
//
//  killough 4/11/98, 4/13/98: fix bugs, add 'back' parameter
//
//==========================================================================
void VRenderLevelShared::UpdateFakeFlats (sector_t *sec) {
  const sector_t *s = sec->heightsec;
  sector_t *heightsec = r_viewleaf->sector->heightsec;
  bool underwater = /*r_fakingunderwater ||*/
    //(heightsec && vieworg.z <= heightsec->floor.GetPointZ(vieworg));
    (s && vieworg.z <= s->floor.GetPointZ(vieworg));
  bool doorunderwater = false;
  bool diffTex = !!(s && s->SectorFlags&sector_t::SF_ClipFakePlanes);

  // replace sector being drawn with a copy to be hacked
  fakefloor_t *ff = sec->fakefloors;
  if (!ff) return; //k8:just in case
  ff->floorplane = sec->floor;
  ff->ceilplane = sec->ceiling;
  ff->params = sec->params;

  //if (s) ff->params.Fade = s->params.Fade;
  //if (s) { ff->floorplane = s->floor; return; }

  /*
  if (s) {
    GCon->Logf("hs->flags=0x%08x; opic=%d; fpic=%d; diffTex=%s; uw=%s; vz=%f; sz=%f", s->SectorFlags, sec->floor.pic.id, s->floor.pic.id, (diffTex ? "tan" : "ona"), (underwater ? "tan" : "ona"),
      vieworg.z, s->floor.GetPointZ(vieworg));
    GCon->Logf("  (%d) hs->floorz=(%f,%f); hs->ceilz=(%f,%f)", (int)(ptrdiff_t)(s-Level->Sectors), s->floor.minz, s->floor.maxz, s->ceiling.minz, s->ceiling.maxz);
    GCon->Logf("  (%d) sec->flags=0x%08x; opic=%d; fpic=%d", (int)(ptrdiff_t)(sec-Level->Sectors), sec->SectorFlags, sec->floor.pic.id, sec->floor.pic.id);
    if (heightsec) GCon->Logf("  (%d) hs2->flags=0x%08x; opic=%d; fpic=%d", (int)(ptrdiff_t)(heightsec-Level->Sectors), heightsec->SectorFlags, heightsec->floor.pic.id, heightsec->floor.pic.id);
    //return;
  }
  */

  // replace floor and ceiling height with control sector's heights
  if (diffTex && !(s->SectorFlags&sector_t::SF_FakeCeilingOnly)) {
    if (CopyPlaneIfValid(&ff->floorplane, &s->floor, &sec->ceiling)) {
      ff->floorplane.pic = s->floor.pic;
      //GCon->Logf("opic=%d; fpic=%d", sec->floor.pic.id, s->floor.pic.id);
    } else if (s && (s->SectorFlags&sector_t::SF_FakeFloorOnly)) {
      /*
      GCon->Logf("sec=%d; hs=%d; src:(%f,%f,%f:%f); dest=(%f,%f,%f:%f); opp:(%f,%f,%f:%f)",
        (int)(ptrdiff_t)(sec-Level->Sectors),
        (int)(ptrdiff_t)(s-Level->Sectors),
        s->floor.normal.x, s->floor.normal.y, s->floor.normal.z, s->floor.dist,
        ff->floorplane.normal.x, ff->floorplane.normal.y, ff->floorplane.normal.z, ff->floorplane.dist,
        sec->ceiling.normal.x, sec->ceiling.normal.y, sec->ceiling.normal.z, sec->ceiling.dist
        );
      */
      if (underwater) {
        //GCon->Logf("heightsec=%s", (heightsec ? "tan" : "ona"));
        //tempsec->ColourMap = s->ColourMap;
        ff->params.Fade = s->params.Fade;
        if (!(s->SectorFlags&sector_t::SF_NoFakeLight)) {
          ff->params.lightlevel = s->params.lightlevel;
          ff->params.LightColour = s->params.LightColour;
          /*
          if (floorlightlevel != nullptr) *floorlightlevel = GetFloorLight (s);
          if (ceilinglightlevel != nullptr) *ceilinglightlevel = GetFloorLight (s);
          */
          //ff->floorplane = (heightsec ? heightsec->floor : sec->floor);
        }
      }
      return;
    }
  } else {
    if (s && !(s->SectorFlags&sector_t::SF_FakeCeilingOnly)) {
      //ff->floorplane.normal = s->floor.normal;
      //ff->floorplane.dist = s->floor.dist;
      //GCon->Logf("  000");
      *(TPlane *)&ff->floorplane = *(TPlane *)&s->floor;
    }
  }

  if (s && !(s->SectorFlags&sector_t::SF_FakeFloorOnly)) {
    if (diffTex) {
      if (CopyPlaneIfValid(&ff->ceilplane, &s->ceiling, &sec->floor)) {
        ff->ceilplane.pic = s->ceiling.pic;
      }
    } else {
      //ff->ceilplane.normal = s->ceiling.normal;
      //ff->ceilplane.dist = s->ceiling.dist;
      //GCon->Logf("  001");
      *(TPlane *)&ff->ceilplane = *(TPlane *)&s->ceiling;
    }
  }

  //float refflorz = s->floor.GetPointZ(viewx, viewy);
  float refceilz = (s ? s->ceiling.GetPointZ(vieworg) : 0); // k8: was `nullptr` -- wtf?!
  //float orgflorz = sec->floor.GetPointZ(viewx, viewy);
  float orgceilz = sec->ceiling.GetPointZ(vieworg);

#if 0
  // [RH] Allow viewing underwater areas through doors/windows that
  // are underwater but not in a water sector themselves.
  // Only works if you cannot see the top surface of any deep water
  // sectors at the same time.
  if (s) {
    for (int i = 0; i < sec->linecount; ++i) {
      float rw_frontcz1 = sec->ceiling.GetPointZ (sec->lines[i]->v1->x, sec->lines[i]->v1->y);
      float rw_frontcz2 = sec->ceiling.GetPointZ (sec->lines[i]->v2->x, sec->lines[i]->v2->y);

      if (/*back && !r_fakingunderwater &&*/ !s->lines[i]->frontsector->heightsec) {
        if (rw_frontcz1 <= s->floor.GetPointZ (sec->lines[i]->v1->x, sec->lines[i]->v1->y) &&
            rw_frontcz2 <= s->floor.GetPointZ (sec->lines[i]->v2->x, sec->lines[i]->v2->y))
        {
          // check that the window is actually visible
          /*
          for (int z = WallSX1; z < WallSX2; ++z) {
            if (floorclip[z] > ceilingclip[z])
            bool val = (heightsec && ((vieworg.z <= heightsec->floor.GetPointZ(sec->lines[i]->v1->x, sec->lines[i]->v1->y) &&
              vieworg.z <= heightsec->floor.GetPointZ(sec->lines[i]->v2->x, sec->lines[i]->v2->y))));
            doorunderwater &= val;
          }
          */
        }
      }
    }
  }
#endif

  if (underwater || doorunderwater /*||
      (heightsec && vieworg.z <= heightsec->floor.GetPointZ(vieworg))*/
     )
  {
    //!ff->floorplane.normal = sec->floor.normal;
    //!ff->floorplane.dist = sec->floor.dist;
    //!ff->ceilplane.normal = -s->floor.normal;
    //!ff->ceilplane.dist = -s->floor.dist/* - -s->floor.normal.z*/;
    *(TPlane *)&ff->floorplane = *(TPlane *)&s->floor;
    *(TPlane *)&ff->ceilplane = *(TPlane *)&s->ceiling;
    //ff->ColourMap = s->ColourMap;
    ff->params.Fade = s->params.Fade;
  }

  // killough 11/98: prevent sudden light changes from non-water sectors:
  if ((underwater /*&& !back*/) || doorunderwater ||
      (heightsec && vieworg.z <= heightsec->floor.GetPointZ(vieworg))
     )
  {
    //GCon->Logf("  002");
    // head-below-floor hack
    ff->floorplane.pic = diffTex ? sec->floor.pic : s->floor.pic;
    ff->floorplane.xoffs = s->floor.xoffs;
    ff->floorplane.yoffs = s->floor.yoffs;
    ff->floorplane.XScale = s->floor.XScale;
    ff->floorplane.YScale = s->floor.YScale;
    ff->floorplane.Angle = s->floor.Angle;
    ff->floorplane.BaseAngle = s->floor.BaseAngle;
    ff->floorplane.BaseYOffs = s->floor.BaseYOffs;

    ff->ceilplane.normal = -s->floor.normal;
    ff->ceilplane.dist = -s->floor.dist/* - -s->floor.normal.z*/;
    if (s->ceiling.pic == skyflatnum) {
      ff->floorplane.normal = -ff->ceilplane.normal;
      ff->floorplane.dist = -ff->ceilplane.dist/* - ff->ceilplane.normal.z*/;
      ff->ceilplane.pic = ff->floorplane.pic;
      ff->ceilplane.xoffs = ff->floorplane.xoffs;
      ff->ceilplane.yoffs = ff->floorplane.yoffs;
      ff->ceilplane.XScale = ff->floorplane.XScale;
      ff->ceilplane.YScale = ff->floorplane.YScale;
      ff->ceilplane.Angle = ff->floorplane.Angle;
      ff->ceilplane.BaseAngle = ff->floorplane.BaseAngle;
      ff->ceilplane.BaseYOffs = ff->floorplane.BaseYOffs;
    } else {
      ff->ceilplane.pic = diffTex ? s->floor.pic : s->ceiling.pic;
      ff->ceilplane.xoffs = s->ceiling.xoffs;
      ff->ceilplane.yoffs = s->ceiling.yoffs;
      ff->ceilplane.XScale = s->ceiling.XScale;
      ff->ceilplane.YScale = s->ceiling.YScale;
      ff->ceilplane.Angle = s->ceiling.Angle;
      ff->ceilplane.BaseAngle = s->ceiling.BaseAngle;
      ff->ceilplane.BaseYOffs = s->ceiling.BaseYOffs;
    }

    if (!(s->SectorFlags&sector_t::SF_NoFakeLight)) {
      ff->params.lightlevel = s->params.lightlevel;
      ff->params.LightColour = s->params.LightColour;
      /*
      if (floorlightlevel != nullptr) *floorlightlevel = GetFloorLight (s);
      if (ceilinglightlevel != nullptr) *ceilinglightlevel = GetFloorLight (s);
      */
    }
  } else if (((s && vieworg.z > s->ceiling.GetPointZ(vieworg)) || //k8: dunno, it was `floor` there, and it seems to be a typo
              (heightsec && vieworg.z > heightsec->ceiling.GetPointZ(vieworg))) &&
             orgceilz > refceilz && !(s->SectorFlags&sector_t::SF_FakeFloorOnly))
  {
    //GCon->Logf("  003");
    // above-ceiling hack
    ff->ceilplane.normal = s->ceiling.normal;
    ff->ceilplane.dist = s->ceiling.dist;
    ff->floorplane.normal = -s->ceiling.normal;
    ff->floorplane.dist = -s->ceiling.dist/* - s->ceiling.normal.z*/;
    ff->params.Fade = s->params.Fade;
    //ff->params.ColourMap = s->params.ColourMap;

    ff->ceilplane.pic = diffTex ? sec->ceiling.pic : s->ceiling.pic;
    ff->floorplane.pic = s->ceiling.pic;
    ff->floorplane.xoffs = ff->ceilplane.xoffs = s->ceiling.xoffs;
    ff->floorplane.yoffs = ff->ceilplane.yoffs = s->ceiling.yoffs;
    ff->floorplane.XScale = ff->ceilplane.XScale = s->ceiling.XScale;
    ff->floorplane.YScale = ff->ceilplane.YScale = s->ceiling.YScale;
    ff->floorplane.Angle = ff->ceilplane.Angle = s->ceiling.Angle;
    ff->floorplane.BaseAngle = ff->ceilplane.BaseAngle = s->ceiling.BaseAngle;
    ff->floorplane.BaseYOffs = ff->ceilplane.BaseYOffs = s->ceiling.BaseYOffs;

    if (s->floor.pic != skyflatnum) {
      ff->ceilplane.normal = sec->ceiling.normal;
      ff->ceilplane.dist = sec->ceiling.dist;
      ff->floorplane.pic = s->floor.pic;
      ff->floorplane.xoffs = s->floor.xoffs;
      ff->floorplane.yoffs = s->floor.yoffs;
      ff->floorplane.XScale = s->floor.XScale;
      ff->floorplane.YScale = s->floor.YScale;
      ff->floorplane.Angle = s->floor.Angle;
    }

    if (!(s->SectorFlags&sector_t::SF_NoFakeLight)) {
      ff->params.lightlevel  = s->params.lightlevel;
      ff->params.LightColour = s->params.LightColour;
      /*
      if (floorlightlevel != nullptr) *floorlightlevel = GetFloorLight (s);
      if (ceilinglightlevel != nullptr) *ceilinglightlevel = GetCeilingLight (s);
      */
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::UpdateDeepWater
//
//==========================================================================
void VRenderLevelShared::UpdateDeepWater (sector_t *sec) {
  if (!sec) return; // just in case
  const sector_t *s = sec->deepref;

  if (!s) return; // just in case

  // replace sector being drawn with a copy to be hacked
  fakefloor_t *ff = sec->fakefloors;
  if (!ff) return; //k8:just in case
  ff->floorplane = sec->floor;
  ff->ceilplane = sec->ceiling;
  ff->params = sec->params;

  ff->floorplane.normal = s->floor.normal;
  ff->floorplane.dist = s->floor.dist;

  //sec->heightsec = sec->deepref;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateFloodBug
//
//  emulate floodfill bug
//
//==========================================================================
void VRenderLevelShared::UpdateFloodBug (sector_t *sec) {
  if (!sec) return; // just in case
  fakefloor_t *ff = sec->fakefloors;
  if (!ff) return; // just in case
  // replace sector being drawn with a copy to be hacked
  ff->floorplane = sec->floor;
  ff->ceilplane = sec->ceiling;
  ff->params = sec->params;
  // floor
  if (sec->othersecFloor && sec->floor.minz < sec->othersecFloor->floor.minz) {
    ff->floorplane = sec->othersecFloor->floor;
    ff->params = sec->othersecFloor->params;
    ff->floorplane.LightSourceSector = (int)(ptrdiff_t)(sec->othersecFloor-Level->Sectors);
  }
  if (sec->othersecCeiling && sec->ceiling.minz > sec->othersecCeiling->ceiling.minz) {
    ff->ceilplane = sec->othersecCeiling->ceiling;
    ff->params = sec->othersecCeiling->params;
    ff->ceilplane.LightSourceSector = (int)(ptrdiff_t)(sec->othersecCeiling-Level->Sectors);
  }
}


//==========================================================================
//
//  VRenderLevelShared::SetupFakeFloors
//
//==========================================================================
void VRenderLevelShared::SetupFakeFloors (sector_t *Sec) {
  if (!Sec->deepref) {
    sector_t *HeightSec = Sec->heightsec;
    if (HeightSec->SectorFlags&sector_t::SF_IgnoreHeightSec) return;
  }

  if (!Sec->fakefloors) Sec->fakefloors = new fakefloor_t;
  memset((void *)Sec->fakefloors, 0, sizeof(fakefloor_t));
  Sec->fakefloors->floorplane = Sec->floor;
  Sec->fakefloors->ceilplane = Sec->ceiling;
  Sec->fakefloors->params = Sec->params;

  Sec->topregion->params = &Sec->fakefloors->params;
}


//==========================================================================
//
//  VRenderLevelShared::ReallocSurface
//
//  free all surfaces except the first one, clear first, set
//  number of vertices to vcount
//
//==========================================================================
surface_t *VRenderLevelShared::ReallocSurface (surface_t *surfs, int vcount) {
  check(vcount > 2); // just in case
  surface_t *surf = surfs;
  if (surf) {
    // clear first surface
    if (surf->CacheSurf) FreeSurfCache(surf->CacheSurf);
    if (surf->lightmap) Z_Free(surf->lightmap);
    if (surf->lightmap_rgb) Z_Free(surf->lightmap_rgb);
    // free extra surfaces
    surface_t *next;
    for (surface_t *s = surfs->next; s; s = next) {
      if (s->CacheSurf) FreeSurfCache(s->CacheSurf);
      if (s->lightmap) Z_Free(s->lightmap);
      if (s->lightmap_rgb) Z_Free(s->lightmap_rgb);
      next = s->next;
      Z_Free(s);
    }
    // realloc first surface (if necessary)
    if (surf->count != vcount) {
      const size_t msize = sizeof(surface_t)+(vcount-1)*sizeof(TVec);
      surf = (surface_t *)Z_Realloc(surf, msize);
      memset((void *)surf, 0, msize);
      surf->count = vcount;
    }
  } else {
    surf = (surface_t *)Z_Calloc(sizeof(surface_t)+(vcount-1)*sizeof(TVec));
    surf->count = vcount;
  }
  return surf;
}


//==========================================================================
//
//  VRenderLevelShared::FreeSurfaces
//
//==========================================================================
void VRenderLevelShared::FreeSurfaces (surface_t *InSurf) {
  surface_t *next;
  for (surface_t *s = InSurf; s; s = next) {
    if (s->CacheSurf) FreeSurfCache(s->CacheSurf);
    if (s->lightmap) Z_Free(s->lightmap);
    if (s->lightmap_rgb) Z_Free(s->lightmap_rgb);
    next = s->next;
    Z_Free(s);
  }
}


//==========================================================================
//
//  VRenderLevelShared::FreeSegParts
//
//==========================================================================
void VRenderLevelShared::FreeSegParts (segpart_t *ASP) {
  for (segpart_t *sp = ASP; sp; sp = sp->next) FreeWSurfs(sp->surfs);
}
