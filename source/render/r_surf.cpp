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
#include "sv_local.h"

// this is used to compare floats like ints which is faster
#define FASI(var) (*(const int32_t *)&var)


// ////////////////////////////////////////////////////////////////////////// //
extern VCvarB w_update_clip_bsp;
//extern VCvarB w_update_clip_region;
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
//  AppendSurfaces
//
//  this actually prepends `newsurfs`, as the exact order doesn't matter
//
//==========================================================================
static void AppendSurfaces (segpart_t *sp, surface_t *newsurfs) {
  check(sp);
  if (!newsurfs) return; // nothing to do
  // new list will start with `newsurfs`
  surface_t *ss = sp->surfs;
  sp->surfs = newsurfs;
  // shoild join?
  if (ss) {
    // yes
    while (newsurfs->next) newsurfs = newsurfs->next;
    newsurfs->next = ss;
  }
}


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
  int vcount = sub->numlines;

  if (vcount < 3) {
    GCon->Logf(NAME_Warning, "CreateSecSurface: subsector #%d has only #%d vertices", (int)(ptrdiff_t)(sub-Level->Subsectors), vcount);
    if (vcount < 1) Sys_Error("ONE VERTEX. WTF?!");
    if (ssurf) return ssurf;
  }
  //check(vcount >= 3);

  // if we're simply changing sky, and already have surface created, do not recreate it, it is pointless
  bool isSkyFlat = (InSplane.splane->pic == skyflatnum);
  bool recreateSurface = true;
  bool updateZ = false;

  // fix plane
  TSecPlaneRef spl(InSplane);
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
    recreateSurface = !isSkyFlat || ((ssurf->esecplane.splane->pic == skyflatnum) != isSkyFlat);
    if (recreateSurface) {
      //GCon->Logf("***  RECREATING!");
      surf = ReallocSurface(ssurf->surfs, vcount);
    } else {
      updateZ = (FASI(ssurf->edist) != FASI(spl.splane->dist));
      surf = ssurf->surfs;
    }
    ssurf->surfs = nullptr; // just in case
  }

  // this is required to calculate static lightmaps, and for other business
  for (surface_t *ss = surf; ss; ss = ss->next) ss->subsector = sub;

  ssurf->esecplane = spl;
  ssurf->edist = spl.splane->dist;

  // setup texture
  VTexture *Tex = GTextureManager(spl.splane->pic);
  if (!Tex) Tex = GTextureManager[GTextureManager.DefaultTexture];
  if (fabsf(spl.splane->normal.z) > 0.1f) {
    float s, c;
    msincos(spl.splane->BaseAngle-spl.splane->Angle, &s, &c);
    ssurf->texinfo.saxis = TVec(c,  s, 0)*(TextureSScale(Tex)*spl.splane->XScale);
    ssurf->texinfo.taxis = TVec(s, -c, 0)*(TextureTScale(Tex)*spl.splane->YScale);
  } else {
    ssurf->texinfo.taxis = TVec(0, 0, -1)*(TextureTScale(Tex)*spl.splane->YScale);
    ssurf->texinfo.saxis = Normalise(CrossProduct(spl.GetNormal(), ssurf->texinfo.taxis))*(TextureSScale(Tex)*spl.splane->XScale);
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

  if (recreateSurface) {
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
      InitSurfs(true, ssurf->surfs, &ssurf->texinfo, spl.splane, sub);
    }
  } else {
    // update z coords, if necessary
    if (updateZ) {
      bool changed = false;
      for (; surf; surf = surf->next) {
        TVec *svert = surf->verts;
        for (int i = surf->count; i--; ++svert) {
          const float oldZ = svert->z;
          svert->z = spl.GetPointZ(svert->x, svert->y);
          if (!changed && FASI(oldZ) != FASI(svert->z)) changed = true;
        }
        if (changed) InitSurfs(true, ssurf->surfs, &ssurf->texinfo, spl.splane, sub);
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

  // if scale/angle was changed, we should update everything, and possibly rebuild the surface
  // our general surface creation function will take care of everything
  if (FASI(ssurf->XScale) != FASI(splane.splane->XScale) ||
      FASI(ssurf->YScale) != FASI(splane.splane->YScale) ||
      ssurf->Angle != splane.splane->BaseAngle-splane.splane->Angle)
  {
    // this will update texture, offsets, and everything
    /*
    GCon->Logf("*** SSF RECREATION! xscale=(%g:%g), yscale=(%g,%g); angle=(%g,%g)",
      ssurf->XScale, splane.splane->XScale,
      ssurf->YScale, splane.splane->YScale,
      ssurf->Angle, splane.splane->BaseAngle-splane.splane->Angle);
    */
    sec_surface_t *newsurf = CreateSecSurface(ssurf, sub, RealPlane);
    check(newsurf == ssurf); // sanity check
    ssurf->texinfo.ColourMap = ColourMap; // just in case
    // nothing more to do
    return;
  }

  ssurf->texinfo.ColourMap = ColourMap; // just in case
  ssurf->texinfo.soffs = splane.splane->xoffs;
  ssurf->texinfo.toffs = splane.splane->yoffs+splane.splane->BaseYOffs;

  //ssurf->texinfo.soffs = 10;
  //ssurf->texinfo.toffs = 10;

  // ok, we still may need to update texture or z coords
  // update texture?
  VTexture *Tex = GTextureManager(splane.splane->pic);
  if (!Tex) Tex = GTextureManager[GTextureManager.DefaultTexture];
  ssurf->texinfo.Tex = Tex;
  ssurf->texinfo.noDecals = Tex->noDecals;

  // update z coords?
  if (FASI(ssurf->edist) != FASI(splane.splane->dist)) {
    bool changed = false;
    ssurf->edist = splane.splane->dist;
    for (surface_t *surf = ssurf->surfs; surf; surf = surf->next) {
      TVec *svert = surf->verts;
      for (int i = surf->count; i--; ++svert) {
        const float oldZ = svert->z;
        svert->z = splane.GetPointZ(svert->x, svert->y);
        if (!changed && FASI(oldZ) != FASI(svert->z)) changed = true;
      }
    }
    // force lightmap recalculation
    if (changed || splane.splane->pic != skyflatnum) {
      InitSurfs(true, ssurf->surfs, &ssurf->texinfo, nullptr, sub);
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
void VRenderLevelShared::FreeWSurfs (surface_t *&InSurfs) {
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
  InSurfs = nullptr;
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
  InitSurfs(true, surf, texinfo, seg, sub);
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
  //k8: each backsector 3d floor can require segpart
  int count = 4;
  for (const sec_region_t *reg = seg->backsector->eregions; reg; reg = reg->next) count += 2; // just in case, reserve two
  return count;
}


//==========================================================================
//
//  VRenderLevelShared::CreateWorldSurfFromWV
//
//==========================================================================
void VRenderLevelShared::CreateWorldSurfFromWV (subsector_t *sub, seg_t *seg, segpart_t *sp, TVec wv[4], bool doOffset) {
  if (wv[0].z == wv[1].z && wv[1].z == wv[2].z && wv[2].z == wv[3].z) {
    // degenerate surface, no need to create it
    return;
  }

  if (wv[0].z == wv[1].z && wv[2].z == wv[3].z) {
    // degenerate surface (thin line), cannot create it (no render support)
    return;
  }

  if (wv[0].z == wv[2].z && wv[1].z == wv[3].z) {
    // degenerate surface (thin line), cannot create it (no render support)
    return;
  }

  TVec *wstart = wv;
  int wcount = 4;
  if (wv[0].z == wv[1].z) {
    // can reduce to triangle
    wstart = wv+1;
    wcount = 3;
  } else if (wv[2].z == wv[3].z) {
    // can reduce to triangle
    wstart = wv;
    wcount = 3;
  }

  //k8: HACK! HACK! HACK!
  //    move middle wall backwards a little, so it will be hidden behind up/down surfaces
  //    this is required for sectors with 3d floors, until i wrote a proper texture clipping math
  if (doOffset) for (unsigned f = 0; f < (unsigned)wcount; ++f) wstart[f] -= seg->normal*0.01f;

  AppendSurfaces(sp, CreateWSurf(wstart, &sp->texinfo, seg, sub, wcount));
}


//==========================================================================
//
//  VRenderLevelShared::SetupOneSidedSkyWSurf
//
//==========================================================================
void VRenderLevelShared::SetupOneSidedSkyWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  FreeWSurfs(sp->surfs);

  sp->texinfo.Tex = GTextureManager[skyflatnum];
  sp->texinfo.noDecals = sp->texinfo.Tex->noDecals;
  sp->texinfo.Alpha = 1.1f;
  sp->texinfo.Additive = false;
  sp->texinfo.ColourMap = 0;

  if (sp->texinfo.Tex->Type != TEXTYPE_Null) {
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

    CreateWorldSurfFromWV(sub, seg, sp, wv);
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = 0.0f;
  sp->backBotDist = 0.0f;
}


//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedSkyWSurf
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedSkyWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  FreeWSurfs(sp->surfs);

  sp->texinfo.Tex = GTextureManager[skyflatnum];
  sp->texinfo.noDecals = sp->texinfo.Tex->noDecals;
  sp->texinfo.Alpha = 1.1f;
  sp->texinfo.Additive = false;
  sp->texinfo.ColourMap = 0;

  if (sp->texinfo.Tex->Type != TEXTYPE_Null) {
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

    CreateWorldSurfFromWV(sub, seg, sp, wv);
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = 0.0f;
  sp->backBotDist = 0.0f;
}


//==========================================================================
//
//  SetupTextureAxesOffset
//
//==========================================================================
/*
static inline void SetupTextureAxesOffset (seg_t *seg, texinfo_t *texinfo, VTexture *tex) {
  sp->texinfo.Tex = Tex;
  sp->texinfo.noDecals = Tex->noDecals;

  sp->texinfo.saxis = seg->dir*(TextureSScale(Tex)*sidedef->Top.ScaleX);
  sp->texinfo.taxis = TVec(0, 0, -1)*(TextureTScale(Tex)*sidedef->Top.ScaleY);

  sp->texinfo.soffs = -DotProduct(*seg->v1, sp->texinfo.saxis)+
                      seg->offset*(TextureSScale(Tex)*sidedef->Top.ScaleX)+
                      sidedef->Top.TextureOffset*TextureOffsetSScale(Tex);
}
*/


//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedTopWSurf
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedTopWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  FreeWSurfs(sp->surfs);

  const line_t *linedef = seg->linedef;
  const side_t *sidedef = seg->sidedef;

  sec_plane_t *back_floor = &seg->backsector->floor;
  sec_plane_t *back_ceiling = &seg->backsector->ceiling;

  VTexture *TTex = GTextureManager(sidedef->TopTexture);
  if (!TTex) TTex = GTextureManager[GTextureManager.DefaultTexture];
  if (IsSky(r_ceiling.splane) && IsSky(back_ceiling) && r_ceiling.splane->SkyBox != back_ceiling->SkyBox) {
    TTex = GTextureManager[skyflatnum];
  }

  sp->texinfo.saxis = seg->dir*(TextureSScale(TTex)*sidedef->Top.ScaleX);
  sp->texinfo.taxis = TVec(0, 0, -1)*(TextureTScale(TTex)*sidedef->Top.ScaleY);
  sp->texinfo.soffs = -DotProduct(*seg->v1, sp->texinfo.saxis)+
                      seg->offset*(TextureSScale(TTex)*sidedef->Top.ScaleX)+
                      sidedef->Top.TextureOffset*TextureOffsetSScale(TTex);
  sp->texinfo.Tex = TTex;
  sp->texinfo.noDecals = TTex->noDecals;
  sp->texinfo.Alpha = 1.1f;
  sp->texinfo.Additive = false;
  sp->texinfo.ColourMap = 0;

  if (TTex->Type != TEXTYPE_Null) {
    TVec wv[4];

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

    if (linedef->flags&ML_DONTPEGTOP) {
      // top of texture at top
      sp->texinfo.toffs = top_TexZ;
    } else {
      // bottom of texture
      sp->texinfo.toffs = back_ceiling->TexZ+(TTex->GetScaledHeight()*sidedef->Top.ScaleY);
    }
    sp->texinfo.toffs *= TextureTScale(TTex)*sidedef->Top.ScaleY;
    sp->texinfo.toffs += sidedef->Top.RowOffset*TextureOffsetTScale(TTex);

    wv[0].x = wv[1].x = seg->v1->x;
    wv[0].y = wv[1].y = seg->v1->y;
    wv[2].x = wv[3].x = seg->v2->x;
    wv[2].y = wv[3].y = seg->v2->y;

    wv[0].z = MAX(back_topz1, botz1);
    wv[1].z = top_topz1;
    wv[2].z = top_topz2;
    wv[3].z = MAX(back_topz2, botz2);

    CreateWorldSurfFromWV(sub, seg, sp, wv);
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = back_ceiling->dist;
  sp->backBotDist = back_floor->dist;
  sp->TextureOffset = sidedef->Top.TextureOffset;
  sp->RowOffset = sidedef->Top.RowOffset;
}


//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedBotWSurf
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedBotWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  FreeWSurfs(sp->surfs);

  const line_t *linedef = seg->linedef;
  const side_t *sidedef = seg->sidedef;

  sec_plane_t *back_floor = &seg->backsector->floor;
  sec_plane_t *back_ceiling = &seg->backsector->ceiling;

  VTexture *BTex = GTextureManager(sidedef->BottomTexture);
  if (!BTex) BTex = GTextureManager[GTextureManager.DefaultTexture];
  sp->texinfo.saxis = seg->dir*(TextureSScale(BTex)*sidedef->Bot.ScaleX);
  sp->texinfo.taxis = TVec(0, 0, -1)*(TextureTScale(BTex)*sidedef->Bot.ScaleY);
  sp->texinfo.soffs = -DotProduct(*seg->v1, sp->texinfo.saxis)+
                      seg->offset*(TextureSScale(BTex)*sidedef->Bot.ScaleX)+
                      sidedef->Bot.TextureOffset*TextureOffsetSScale(BTex);
  sp->texinfo.Tex = BTex;
  sp->texinfo.noDecals = BTex->noDecals;
  sp->texinfo.Alpha = 1.1f;
  sp->texinfo.Additive = false;
  sp->texinfo.ColourMap = 0;

  if (BTex->Type != TEXTYPE_Null) {
    TVec wv[4];

    float topz1 = r_ceiling.GetPointZ(*seg->v1);
    float topz2 = r_ceiling.GetPointZ(*seg->v2);
    float botz1 = r_floor.GetPointZ(*seg->v1);
    float botz2 = r_floor.GetPointZ(*seg->v2);
    float top_TexZ = r_ceiling.splane->TexZ;

    const float back_botz1 = back_floor->GetPointZ(*seg->v1);
    const float back_botz2 = back_floor->GetPointZ(*seg->v2);

    // hack to allow height changes in outdoor areas
    if (IsSky(r_ceiling.splane) && IsSky(back_ceiling)) {
      topz1 = back_ceiling->GetPointZ(*seg->v1);
      topz2 = back_ceiling->GetPointZ(*seg->v2);
      top_TexZ = back_ceiling->TexZ;
    }

    if (linedef->flags&ML_DONTPEGBOTTOM) {
      // bottom of texture at bottom
      // top of texture at top
      sp->texinfo.toffs = top_TexZ;
    } else {
      // top of texture at top
      sp->texinfo.toffs = back_floor->TexZ;
    }
    sp->texinfo.toffs *= TextureTScale(BTex)*sidedef->Bot.ScaleY;
    sp->texinfo.toffs += sidedef->Bot.RowOffset*TextureOffsetTScale(BTex);

    wv[0].x = wv[1].x = seg->v1->x;
    wv[0].y = wv[1].y = seg->v1->y;
    wv[2].x = wv[3].x = seg->v2->x;
    wv[2].y = wv[3].y = seg->v2->y;

    wv[0].z = botz1;
    wv[1].z = MIN(back_botz1, topz1);
    wv[2].z = MIN(back_botz2, topz2);
    wv[3].z = botz2;

    CreateWorldSurfFromWV(sub, seg, sp, wv);
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backBotDist = back_floor->dist;
  sp->backTopDist = back_ceiling->dist;
  sp->TextureOffset = sidedef->Bot.TextureOffset;
  sp->RowOffset = sidedef->Bot.RowOffset;
}


//==========================================================================
//
//  VRenderLevelShared::SetupOneSidedMidWSurf
//
//==========================================================================
void VRenderLevelShared::SetupOneSidedMidWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  FreeWSurfs(sp->surfs);

  const line_t *linedef = seg->linedef;
  const side_t *sidedef = seg->sidedef;

  VTexture *MTex = GTextureManager(sidedef->MidTexture);
  // k8: one-sided line should have a midtex
  if (!MTex) {
    GCon->Logf(NAME_Warning, "Sidedef #%d should have midtex, but it hasn't (%d)", (int)(ptrdiff_t)(sidedef-Level->Sides), sidedef->MidTexture.id);
    MTex = GTextureManager[GTextureManager.DefaultTexture];
  }
  sp->texinfo.saxis = seg->dir*(TextureSScale(MTex)*sidedef->Mid.ScaleX);
  sp->texinfo.taxis = TVec(0, 0, -1)*(TextureTScale(MTex)*sidedef->Mid.ScaleY);
  sp->texinfo.soffs = -DotProduct(*seg->v1, sp->texinfo.saxis)+
                      seg->offset*(TextureSScale(MTex)*sidedef->Mid.ScaleX)+
                      sidedef->Mid.TextureOffset*TextureOffsetSScale(MTex);
  if (linedef->flags&ML_DONTPEGBOTTOM) {
    // bottom of texture at bottom
    sp->texinfo.toffs = r_floor.splane->TexZ+(MTex->GetScaledHeight()*sidedef->Mid.ScaleY);
  } else if (linedef->flags&ML_DONTPEGTOP) {
    // top of texture at top of top region
    sp->texinfo.toffs = seg->front_sub->sector->ceiling.TexZ;
  } else {
    // top of texture at top
    sp->texinfo.toffs = r_ceiling.splane->TexZ;
  }
  sp->texinfo.toffs *= TextureTScale(MTex)*sidedef->Mid.ScaleY;
  sp->texinfo.toffs += sidedef->Mid.RowOffset*TextureOffsetTScale(MTex);

  sp->texinfo.Tex = MTex;
  sp->texinfo.noDecals = MTex->noDecals;
  sp->texinfo.Alpha = 1.1f;
  sp->texinfo.Additive = false;
  sp->texinfo.ColourMap = 0;

  if (MTex->Type != TEXTYPE_Null) {
    TVec wv[4];

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

    CreateWorldSurfFromWV(sub, seg, sp, wv);
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = 0.0f;
  sp->backBotDist = 0.0f;
  sp->TextureOffset = sidedef->Mid.TextureOffset;
  sp->RowOffset = sidedef->Mid.RowOffset;
}


//==========================================================================
//
//  DumpOpening
//
//==========================================================================
static __attribute__((unused)) void DumpOpening (const opening_t *op) {
  GCon->Logf("  %p: floor=%g (%g,%g,%g:%g); ceil=%g (%g,%g,%g:%g); lowfloor=%g; range=%g",
    op,
    op->bottom, op->efloor.GetNormal().x, op->efloor.GetNormal().y, op->efloor.GetNormal().z, op->efloor.GetDist(),
    op->top, op->eceiling.GetNormal().x, op->eceiling.GetNormal().y, op->eceiling.GetNormal().z, op->eceiling.GetDist(),
    op->lowfloor, op->range);
}


//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedMidWSurf
//
//  create normal midtexture surface
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedMidWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  FreeWSurfs(sp->surfs);

  const line_t *linedef = seg->linedef;
  const side_t *sidedef = seg->sidedef;

  VTexture *MTex = GTextureManager(sidedef->MidTexture);
  if (!MTex) MTex = GTextureManager[GTextureManager.DefaultTexture];
  sp->texinfo.Tex = MTex;
  sp->texinfo.noDecals = MTex->noDecals;
  sp->texinfo.ColourMap = 0;
  sp->texinfo.saxis = seg->dir*(TextureSScale(MTex)*sidedef->Mid.ScaleX);
  sp->texinfo.taxis = TVec(0, 0, -1)*(TextureTScale(MTex)*sidedef->Mid.ScaleY);

  sec_plane_t *back_floor = &seg->backsector->floor;
  sec_plane_t *back_ceiling = &seg->backsector->ceiling;

  if (MTex->Type != TEXTYPE_Null) {
    TVec wv[4];

    const float back_topz1 = back_ceiling->GetPointZ(*seg->v1);
    const float back_topz2 = back_ceiling->GetPointZ(*seg->v2);
    const float back_botz1 = back_floor->GetPointZ(*seg->v1);
    const float back_botz2 = back_floor->GetPointZ(*seg->v2);

    // find opening for this side
    const float exbotz = MIN(back_botz1, back_botz2);
    const float extopz = MAX(back_topz1, back_topz2);

    sp->texinfo.soffs = -DotProduct(*seg->v1, sp->texinfo.saxis)+
                        seg->offset*(TextureSScale(MTex)*sidedef->Mid.ScaleX)+
                        sidedef->Mid.TextureOffset*TextureOffsetSScale(MTex);
    //if (sidedef->Mid.RowOffset) GCon->Logf("line #%d: midofs=%g; tex=<%s>; scaley=%g", (int)(ptrdiff_t)(linedef-Level->Lines), sidedef->Mid.RowOffset, *MTex->Name, sidedef->Mid.ScaleY);

    const float texh = MTex->GetScaledHeight();
    //const float z_org = FixPegZOrgMid(seg, sp, MTex, texh);

    float z_org;
    if (linedef->flags&ML_DONTPEGBOTTOM) {
      // bottom of texture at bottom
      z_org = MAX(seg->frontsector->floor.TexZ, seg->backsector->floor.TexZ)+texh;
    } else {
      // top of texture at top
      z_org = MIN(seg->frontsector->ceiling.TexZ, seg->backsector->ceiling.TexZ);
    }
    //z_org += sidedef->Mid.RowOffset*(!MTex->bWorldPanning ? 1.0f : 1.0f/MTex->TScale);
    //z_org += (sidedef->Mid.RowOffset+texh)*(sidedef->Mid.ScaleY*(!MTex->bWorldPanning ? 1.0f : 1.0f/MTex->TScale));
    //k8: dunno why
    /*
    if (sidedef->Mid.RowOffset < 0) {
      z_org += (sidedef->Mid.RowOffset+texh)*(!MTex->bWorldPanning ? 1.0f : 1.0f/MTex->TScale);
    } else {
      z_org += sidedef->Mid.RowOffset*(!MTex->bWorldPanning ? 1.0f : 1.0f/MTex->TScale);
    }
    sp->texinfo.toffs = z_org*TextureTScale(MTex);
    */

    //k8: this seems to be wrong
    if ((linedef->flags&ML_WRAP_MIDTEX) || (sidedef->Flags&SDF_WRAPMIDTEX)) {
      sp->texinfo.toffs = r_ceiling.splane->TexZ*(TextureTScale(MTex)*sidedef->Mid.ScaleY)+
                          sidedef->Mid.RowOffset*TextureOffsetTScale(MTex);
    }
    //sp->texinfo.toffs = 0;
    //if (sidedef->Mid.RowOffset) GCon->Logf("  toffs=%g", sp->texinfo.toffs);

    sp->texinfo.Alpha = linedef->alpha;
    sp->texinfo.Additive = !!(linedef->flags&ML_ADDITIVE);

    //bool doDump = ((ptrdiff_t)(linedef-Level->Lines) == 1707);
    enum { doDump = 0 };
    if (doDump) { GCon->Logf("=== MIDSURF FOR LINE #%d (fs=%d; bs=%d) ===", (int)(ptrdiff_t)(linedef-Level->Lines), (int)(ptrdiff_t)(seg->frontsector-Level->Sectors), (int)(ptrdiff_t)(seg->backsector-Level->Sectors)); }

    //k8: HACK! HACK! HACK!
    //    move middle wall backwards a little, so it will be hidden behind up/down surfaces
    //    this is required for sectors with 3d floors, until i wrote a proper texture clipping math
    bool doOffset = seg->backsector->Has3DFloors();

    for (opening_t *cop = SV_SectorOpenings(seg->frontsector, true); cop; cop = cop->next) {
      if (extopz <= cop->bottom || exbotz >= cop->top) {
        if (doDump) { GCon->Log(" SKIP opening"); DumpOpening(cop); }
        continue;
      }
      if (doDump) { GCon->Logf(" ACCEPT opening"); DumpOpening(cop); }
      // ok, we are at least partially in this opening

      wv[0].x = wv[1].x = seg->v1->x;
      wv[0].y = wv[1].y = seg->v1->y;
      wv[2].x = wv[3].x = seg->v2->x;
      wv[2].y = wv[3].y = seg->v2->y;

      const float topz1 = MIN(back_topz1, cop->eceiling.GetPointZ(*seg->v1));
      const float topz2 = MIN(back_topz2, cop->eceiling.GetPointZ(*seg->v2));
      const float botz1 = MAX(back_botz1, cop->efloor.GetPointZ(*seg->v1));
      const float botz2 = MAX(back_botz2, cop->efloor.GetPointZ(*seg->v2));

      float midtopz1 = topz1;
      float midtopz2 = topz2;
      float midbotz1 = botz1;
      float midbotz2 = botz2;

      if (doDump) { GCon->Logf(" zorg=(%g,%g); botz=(%g,%g); topz=(%g,%g)", z_org-texh, z_org, midbotz1, midbotz2, midtopz1, midtopz2); }

      if (sidedef->TopTexture > 0) {
        midtopz1 = MIN(midtopz1, cop->eceiling.GetPointZ(*seg->v1));
        midtopz2 = MIN(midtopz2, cop->eceiling.GetPointZ(*seg->v2));
      }

      if (sidedef->BottomTexture > 0) {
        midbotz1 = MAX(midbotz1, cop->efloor.GetPointZ(*seg->v1));
        midbotz2 = MAX(midbotz2, cop->efloor.GetPointZ(*seg->v1));
      }

      if (midbotz1 >= midtopz1 || midbotz2 >= midtopz2) continue;

      if (doDump) { GCon->Logf(" zorg=(%g,%g); botz=(%g,%g); topz=(%g,%g); backbotz=(%g,%g); backtopz=(%g,%g)", z_org-texh, z_org, midbotz1, midbotz2, midtopz1, midtopz2, back_botz1, back_botz2, back_topz1, back_topz2); }

      float hgts[4];

      // linedef->flags&ML_CLIP_MIDTEX, sidedef->Flags&SDF_CLIPMIDTEX
      // this clips texture to a floor, otherwise it goes beyound it
      // it seems that all modern OpenGL renderers just ignores clip flag, and
      // renders all midtextures as always clipped.
      if ((linedef->flags&ML_WRAP_MIDTEX) || (sidedef->Flags&SDF_WRAPMIDTEX)) {
        hgts[0] = midbotz1;
        hgts[1] = midtopz1;
        hgts[2] = midtopz2;
        hgts[3] = midbotz2;
      } else {
        if (z_org <= MAX(midbotz1, midbotz2)) continue;
        if (z_org-texh >= MAX(midtopz1, midtopz2)) continue;
        if (doDump) {
          midbotz1 = midbotz2 = 78;
          GCon->Log(" === front regions ===");
          VLevel::dumpSectorRegions(seg->frontsector);
          GCon->Log(" === front openings ===");
          for (opening_t *bop = SV_SectorOpenings2(seg->frontsector, true); bop; bop = bop->next) DumpOpening(bop);
          GCon->Log(" === back regions ===");
          VLevel::dumpSectorRegions(seg->backsector);
          GCon->Log(" === back openings ===");
          for (opening_t *bop = SV_SectorOpenings2(seg->backsector, true); bop; bop = bop->next) DumpOpening(bop);
        }
        hgts[0] = MAX(midbotz1, z_org-texh);
        hgts[1] = MIN(midtopz1, z_org);
        hgts[2] = MIN(midtopz2, z_org);
        hgts[3] = MAX(midbotz2, z_org-texh);
      }

      wv[0].z = hgts[0];
      wv[1].z = hgts[1];
      wv[2].z = hgts[2];
      wv[3].z = hgts[3];

      if (doDump) { GCon->Logf("  z:(%g,%g,%g,%g)", hgts[0], hgts[1], hgts[2], hgts[3]); }

      CreateWorldSurfFromWV(sub, seg, sp, wv, doOffset);
    }
  } else {
    // empty midtexture
    sp->texinfo.Alpha = 1.1f;
    sp->texinfo.Additive = false;
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = back_ceiling->dist;
  sp->backBotDist = back_floor->dist;
  sp->TextureOffset = sidedef->Mid.TextureOffset;
  sp->RowOffset = sidedef->Mid.RowOffset;
}


//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedMidExtraWSurf
//
//  create 3d-floors midtexture (side) surfaces
//  this creates surfaces not in 3d-floor sector, but in neighbouring one
//
//  do not create side surfaces if they aren't in openings
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedMidExtraWSurf (sec_region_t *reg, subsector_t *sub, seg_t *seg, segpart_t *sp,
                                                     TSecPlaneRef r_floor, TSecPlaneRef r_ceiling, opening_t *ops)
{
  FreeWSurfs(sp->surfs);

  const side_t *extraside = &Level->Sides[reg->extraline->sidenum[0]];

  VTexture *MTex = GTextureManager(extraside->MidTexture);
  if (!MTex) MTex = GTextureManager[GTextureManager.DefaultTexture];

  sp->texinfo.saxis = seg->dir*(TextureSScale(MTex)*extraside->Mid.ScaleX);
  sp->texinfo.taxis = TVec(0, 0, -1)*(TextureTScale(MTex)*extraside->Mid.ScaleY);
  sp->texinfo.soffs = -DotProduct(*seg->v1, sp->texinfo.saxis)+
                      seg->offset*(TextureSScale(MTex)*extraside->Mid.ScaleX)+
                      extraside->Mid.TextureOffset*TextureOffsetSScale(MTex);
  sp->texinfo.toffs = reg->eceiling.splane->TexZ*(TextureTScale(MTex)*extraside->Mid.ScaleY)+
                      extraside->Mid.RowOffset*TextureTScale(MTex);
  sp->texinfo.Tex = MTex;
  sp->texinfo.noDecals = MTex->noDecals;
  sp->texinfo.Alpha = (reg->efloor.splane->Alpha < 1.0f ? reg->efloor.splane->Alpha : 1.1f);
  sp->texinfo.Additive = !!(reg->efloor.splane->flags&SPF_ADDITIVE);
  sp->texinfo.ColourMap = 0;

  if (MTex->Type != TEXTYPE_Null) {
    TVec wv[4];

    const float extratopz1 = reg->eceiling.GetPointZ(*seg->v1);
    const float extratopz2 = reg->eceiling.GetPointZ(*seg->v2);
    const float extrabotz1 = reg->efloor.GetPointZ(*seg->v1);
    const float extrabotz2 = reg->efloor.GetPointZ(*seg->v2);

    // find opening for this side
    const float exbotz = MIN(extrabotz1, extrabotz2);
    const float extopz = MAX(extratopz1, extratopz2);

    for (opening_t *cop = ops; cop; cop = cop->next) {
      if (extopz <= cop->bottom || exbotz >= cop->top) continue;
      // ok, we are at least partially in this opening

      const float topz1 = cop->eceiling.GetPointZ(*seg->v1);
      const float topz2 = cop->eceiling.GetPointZ(*seg->v2);
      const float botz1 = cop->efloor.GetPointZ(*seg->v1);
      const float botz2 = cop->efloor.GetPointZ(*seg->v2);

      wv[0].x = wv[1].x = seg->v1->x;
      wv[0].y = wv[1].y = seg->v1->y;
      wv[2].x = wv[3].x = seg->v2->x;
      wv[2].y = wv[3].y = seg->v2->y;

      wv[0].z = MAX(extrabotz1, botz1);
      wv[1].z = MIN(extratopz1, topz1);
      wv[2].z = MIN(extratopz2, topz2);
      wv[3].z = MAX(extrabotz2, botz2);

      CreateWorldSurfFromWV(sub, seg, sp, wv);
    }

    if (sp->surfs && (sp->texinfo.Alpha < 1.0f || MTex->isTransparent())) {
      for (surface_t *sf = sp->surfs; sf; sf = sf->next) sf->drawflags |= surface_t::DF_NO_FACE_CULL;
    }
  } else {
    sp->texinfo.Alpha = 1.1f;
    sp->texinfo.Additive = false;
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = reg->eceiling.splane->dist;
  sp->backBotDist = reg->efloor.splane->dist;
  sp->TextureOffset = extraside->Mid.TextureOffset;
  sp->RowOffset = extraside->Mid.RowOffset;
}


//==========================================================================
//
//  VRenderLevelShared::CreateSegParts
//
//  create world/wall surfaces
//
//==========================================================================
void VRenderLevelShared::CreateSegParts (subsector_t *sub, drawseg_t *dseg, seg_t *seg, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling, sec_region_t *curreg, bool isMainRegion) {
  segpart_t *sp;

  dseg->seg = seg;
  dseg->next = seg->drawsegs;
  seg->drawsegs = dseg;

  if (!seg->linedef) return; // miniseg

  if (!seg->backsector) {
    if (isMainRegion) {
      // middle wall
      dseg->mid = SurfCreatorGetPSPart();
      sp = dseg->mid;
      sp->basereg = curreg;
      SetupOneSidedMidWSurf(sub, seg, sp, r_floor, r_ceiling);

      // sky above line
      dseg->topsky = SurfCreatorGetPSPart();
      sp = dseg->topsky;
      sp->basereg = curreg;
      if (IsSky(r_ceiling.splane)) SetupOneSidedSkyWSurf(sub, seg, sp, r_floor, r_ceiling);
    }
  } else {
    // two sided line
    if (isMainRegion) {
      // top wall
      dseg->top = SurfCreatorGetPSPart();
      sp = dseg->top;
      sp->basereg = curreg;
      SetupTwoSidedTopWSurf(sub, seg, sp, r_floor, r_ceiling);

      // sky above top
      dseg->topsky = SurfCreatorGetPSPart();
      dseg->topsky->basereg = curreg;
      if (IsSky(r_ceiling.splane) && !IsSky(&seg->backsector->ceiling)) {
        sp = dseg->topsky;
        SetupTwoSidedSkyWSurf(sub, seg, sp, r_floor, r_ceiling);
      }

      // bottom wall
      dseg->bot = SurfCreatorGetPSPart();
      sp = dseg->bot;
      sp->basereg = curreg;
      SetupTwoSidedBotWSurf(sub, seg, sp, r_floor, r_ceiling);

      // middle wall
      dseg->mid = SurfCreatorGetPSPart();
      sp = dseg->mid;
      sp->basereg = curreg;
      SetupTwoSidedMidWSurf(sub, seg, sp, r_floor, r_ceiling);

      // create region sides
      // this creates sides for neightbour 3d floors
      opening_t *ops = nullptr;
      bool opsCreated = false;
      for (sec_region_t *reg = seg->backsector->eregions->next; reg; reg = reg->next) {
        if (!reg->extraline) continue; // no need to create extra side

        sp = SurfCreatorGetPSPart();
        sp->basereg = reg;
        sp->next = dseg->extra;
        dseg->extra = sp;

        if (!opsCreated) {
          opsCreated = true;
          ops = SV_SectorOpenings(seg->frontsector);
        }
        SetupTwoSidedMidExtraWSurf(reg, sub, seg, sp, r_floor, r_ceiling, ops);
      }
    }
  }
}


//==========================================================================
//
//  CheckCommonRecreateEx
//
//==========================================================================
static inline bool CheckCommonRecreateEx (segpart_t *sp, VTexture *NTex, const TPlane *floor, const TPlane *ceiling,
                                          const TPlane *backfloor, const TPlane *backceiling)
{
  if (!NTex) NTex = GTextureManager[GTextureManager.DefaultTexture];
  bool res =
    (ceiling ? FASI(sp->frontTopDist) != FASI(ceiling->dist) : false) ||
    (floor ? FASI(sp->frontBotDist) != FASI(floor->dist) : false) ||
    (backceiling ? FASI(sp->backTopDist) != FASI(backceiling->dist) : false) ||
    (backfloor ? FASI(sp->backBotDist) != FASI(backfloor->dist) : false) ||
    FASI(sp->texinfo.Tex->SScale) != FASI(NTex->SScale) ||
    FASI(sp->texinfo.Tex->TScale) != FASI(NTex->TScale) ||
    (sp->texinfo.Tex->Type == TEXTYPE_Null) != (NTex->Type == TEXTYPE_Null) ||
    sp->texinfo.Tex->GetHeight() != NTex->GetHeight() ||
    sp->texinfo.Tex->GetWidth() != NTex->GetWidth();
  // update texture, why not
  sp->texinfo.Tex = NTex;
  sp->texinfo.noDecals = NTex->noDecals;
  return res;
}


//==========================================================================
//
//  CheckCommonRecreate
//
//==========================================================================
static inline bool CheckCommonRecreate (seg_t *seg, segpart_t *sp, VTexture *NTex, const TPlane *floor, const TPlane *ceiling) {
  if (seg->backsector) {
    return CheckCommonRecreateEx(sp, NTex, floor, ceiling, &seg->backsector->floor, &seg->backsector->ceiling);
  } else {
    return CheckCommonRecreateEx(sp, NTex, floor, ceiling, nullptr, nullptr);
  }
}


//==========================================================================
//
//  CheckMidRecreate
//
//==========================================================================
static inline bool CheckMidRecreate (seg_t *seg, segpart_t *sp, const TPlane *floor, const TPlane *ceiling) {
  return CheckCommonRecreate(seg, sp, GTextureManager(seg->sidedef->MidTexture), floor, ceiling);
}


//==========================================================================
//
//  CheckTopRecreate
//
//==========================================================================
static inline bool CheckTopRecreate (seg_t *seg, segpart_t *sp, sec_plane_t *floor, sec_plane_t *ceiling) {
  sec_plane_t *back_ceiling = &seg->backsector->ceiling;
  VTexture *TTex = GTextureManager(seg->sidedef->TopTexture);
  if (IsSky(ceiling) && IsSky(back_ceiling) && ceiling->SkyBox != back_ceiling->SkyBox) {
    TTex = GTextureManager[skyflatnum];
  }
  return CheckCommonRecreate(seg, sp, TTex, floor, ceiling);
}


//==========================================================================
//
//  CheckBopRecreate
//
//==========================================================================
static inline bool CheckBopRecreate (seg_t *seg, segpart_t *sp, const TPlane *floor, const TPlane *ceiling) {
  return CheckCommonRecreate(seg, sp, GTextureManager(seg->sidedef->BottomTexture), floor, ceiling);
}


//==========================================================================
//
//  VRenderLevelShared::UpdateTextureOffsets
//
//==========================================================================
void VRenderLevelShared::UpdateTextureOffsets (subsector_t *sub, seg_t *seg, segpart_t *sp, const float *soffs, const float *toffs) {
  bool reinitSurfs = false;

  if (FASI(sp->RowOffset) != FASI(*toffs)) {
    reinitSurfs = true;
    sp->texinfo.toffs += ((*toffs)-sp->RowOffset)*TextureOffsetTScale(sp->texinfo.Tex);
    sp->RowOffset = *toffs;
  }

  if (FASI(sp->TextureOffset) != FASI(*soffs)) {
    reinitSurfs = true;
    sp->texinfo.soffs += ((*soffs)-sp->TextureOffset)*TextureOffsetSScale(sp->texinfo.Tex);
    sp->TextureOffset = *soffs;
  }

  if (reinitSurfs) {
    // do not recalculate static lightmaps
    InitSurfs(false, sp->surfs, &sp->texinfo, nullptr, sub);
  }
}


//==========================================================================
//
//  VRenderLevelShared::UpdateDrawSeg
//
//==========================================================================
void VRenderLevelShared::UpdateDrawSeg (subsector_t *sub, drawseg_t *dseg, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  seg_t *seg = dseg->seg;

  if (!seg->linedef) return; // miniseg

#if 0
  if (w_update_clip_region) {
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
#endif

  if (!seg->backsector) {
    // one-sided seg
    // top sky
    segpart_t *sp = dseg->topsky;
    if (sp) {
      if (IsSky(r_ceiling.splane) && FASI(sp->frontTopDist) != FASI(r_ceiling.splane->dist)) {
        SetupOneSidedSkyWSurf(sub, seg, sp, r_floor, r_ceiling);
      }
      sp->texinfo.ColourMap = ColourMap;
    }

    // midtexture
    sp = dseg->mid;
    if (sp) {
      if (CheckMidRecreate(seg, sp, r_floor.splane, r_ceiling.splane)) {
        SetupOneSidedMidWSurf(sub, seg, sp, r_floor, r_ceiling);
      } else {
        UpdateTextureOffsets(sub, seg, sp, &seg->sidedef->Mid.TextureOffset, &seg->sidedef->Mid.RowOffset);
      }
      sp->texinfo.ColourMap = ColourMap;
    }
  } else {
    // two-sided seg
    sec_plane_t *back_ceiling = &seg->backsector->ceiling;

    // sky above top
    segpart_t *sp = dseg->topsky;
    if (sp) {
      if (IsSky(r_ceiling.splane) && !IsSky(back_ceiling) && FASI(sp->frontTopDist) != FASI(r_ceiling.splane->dist)) {
        SetupTwoSidedSkyWSurf(sub, seg, sp, r_floor, r_ceiling);
      }
      sp->texinfo.ColourMap = ColourMap;
    }

    // top wall
    sp = dseg->top;
    if (sp) {
      if (CheckTopRecreate(seg, sp, r_floor.splane, r_ceiling.splane)) {
        SetupTwoSidedTopWSurf(sub, seg, sp, r_floor, r_ceiling);
      } else {
        UpdateTextureOffsets(sub, seg, sp, &seg->sidedef->Top.TextureOffset, &seg->sidedef->Top.RowOffset);
      }
      sp->texinfo.ColourMap = ColourMap;
    }

    // bottom wall
    sp = dseg->bot;
    if (sp) {
      if (CheckBopRecreate(seg, sp, r_floor.splane, r_ceiling.splane)) {
        SetupTwoSidedBotWSurf(sub, seg, sp, r_floor, r_ceiling);
      } else {
        UpdateTextureOffsets(sub, seg, sp, &seg->sidedef->Bot.TextureOffset, &seg->sidedef->Bot.RowOffset);
      }
      sp->texinfo.ColourMap = ColourMap;
    }

    // masked MidTexture
    sp = dseg->mid;
    if (sp) {
      if (CheckMidRecreate(seg, sp, r_floor.splane, r_ceiling.splane)) {
        SetupTwoSidedMidWSurf(sub, seg, sp, r_floor, r_ceiling);
      } else {
        UpdateTextureOffsets(sub, seg, sp, &seg->sidedef->Mid.TextureOffset, &seg->sidedef->Mid.RowOffset);
      }
      sp->texinfo.ColourMap = ColourMap;
      if (sp->texinfo.Tex->Type != TEXTYPE_Null) {
        sp->texinfo.Alpha = seg->linedef->alpha;
        sp->texinfo.Additive = !!(seg->linedef->flags&ML_ADDITIVE);
      } else {
        sp->texinfo.Alpha = 1.1f;
        sp->texinfo.Additive = false;
      }
    }

    opening_t *ops = nullptr;
    bool opsCreated = false;
    // update 3d floors
    for (sp = dseg->extra; sp; sp = sp->next) {
      sec_region_t *reg = sp->basereg;
      check(reg->extraline);
      side_t *extraside = &Level->Sides[reg->extraline->sidenum[0]];

      VTexture *MTex = GTextureManager(extraside->MidTexture);
      if (!MTex) MTex = GTextureManager[GTextureManager.DefaultTexture];

      if (CheckCommonRecreateEx(sp, MTex, r_floor.splane, r_ceiling.splane, reg->efloor.splane, reg->eceiling.splane)) {
        if (!opsCreated) {
          opsCreated = true;
          ops = SV_SectorOpenings(seg->frontsector);
        }
        SetupTwoSidedMidExtraWSurf(reg, sub, seg, sp, r_floor, r_ceiling, ops);
      } else {
        UpdateTextureOffsets(sub, seg, sp, &extraside->Mid.TextureOffset, &extraside->Mid.RowOffset);
      }
      sp->texinfo.ColourMap = ColourMap;
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::SegMoved
//
//  called when polyobject moved
//
//==========================================================================
void VRenderLevelShared::SegMoved (seg_t *seg) {
  if (!seg->drawsegs) return; // drawsegs not created yet
  if (!seg->linedef) Sys_Error("R_SegMoved: miniseg");
  if (!seg->drawsegs->mid) return; // no midsurf

  //k8: just in case
  if (seg->length <= 0.0f) {
    seg->dir = TVec(1, 0, 0); // arbitrary
  } else {
    seg->dir = ((*seg->v2)-(*seg->v1)).normalised2D();
    if (!seg->dir.isValid() || seg->dir.isZero()) seg->dir = TVec(1, 0, 0); // arbitrary
  }

  const side_t *sidedef = seg->sidedef;

  VTexture *MTex = seg->drawsegs->mid->texinfo.Tex;
  seg->drawsegs->mid->texinfo.saxis = seg->dir*(TextureSScale(MTex)*sidedef->Mid.ScaleX);
  seg->drawsegs->mid->texinfo.soffs = -DotProduct(*seg->v1, seg->drawsegs->mid->texinfo.saxis)+
                                      seg->offset*(TextureSScale(MTex)*sidedef->Mid.ScaleX)+
                                      sidedef->Mid.TextureOffset*TextureOffsetSScale(MTex);

  // force update
  seg->drawsegs->mid->frontTopDist += 0.346f;
}


//==========================================================================
//
//  VRenderLevelShared::CreateWorldSurfaces
//
//==========================================================================
void VRenderLevelShared::CreateWorldSurfaces () {
  inWorldCreation = true;

  check(!free_wsurfs);
  SetupSky();

  // set up fake floors
  for (int i = 0; i < Level->NumSectors; ++i) {
    if (Level->Sectors[i].heightsec || Level->Sectors[i].deepref) {
      SetupFakeFloors(&Level->Sectors[i]);
    }
  }

  if (inWorldCreation) {
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
    count += 4*2; //k8: dunno
    for (sec_region_t *reg = sub->sector->eregions; reg; reg = reg->next) {
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

  pspartsLeft = spcount+1;
  int sregLeft = count+1;
  int pdsLeft = dscount+1;

  memset((void *)sreg, 0, sizeof(subregion_t)*sregLeft);
  memset((void *)pds, 0, sizeof(drawseg_t)*pdsLeft);
  memset((void *)pspart, 0, sizeof(segpart_t)*pspartsLeft);

  AllocatedSubRegions = sreg;
  AllocatedDrawSegs = pds;
  AllocatedSegParts = pspart;

  // create sector surfaces
  sub = &Level->Subsectors[0];
  for (int i = Level->NumSubsectors; i--; ++sub) {
    if (!sub->sector->linecount) continue; // skip sectors containing original polyobjs

    TSecPlaneRef main_floor = sub->sector->eregions->efloor;
    TSecPlaneRef main_ceiling = sub->sector->eregions->eceiling;

    int ridx = 0;
    for (sec_region_t *reg = sub->sector->eregions; reg; reg = reg->next, ++ridx) {
      if (sregLeft == 0) Sys_Error("out of subregions in surface creator");

      TSecPlaneRef r_floor, r_ceiling;
      r_floor = reg->efloor;
      r_ceiling = reg->eceiling;

      sreg->secregion = reg;
      sreg->floorplane = r_floor;
      sreg->ceilplane = r_ceiling;
      sreg->realfloor = (reg->regflags&sec_region_t::RF_SkipFloorSurf ? nullptr : CreateSecSurface(nullptr, sub, r_floor));
      sreg->realceil = (reg->regflags&sec_region_t::RF_SkipCeilSurf ? nullptr : CreateSecSurface(nullptr, sub, r_ceiling));

      // create fake floor and ceiling
      if (ridx == 0 && sub->sector->fakefloors) {
        TSecPlaneRef fakefloor, fakeceil;
        fakefloor.set(&sub->sector->fakefloors->floorplane, false);
        fakeceil.set(&sub->sector->fakefloors->ceilplane, false);
        if (!fakefloor.isFloor()) fakefloor.Flip();
        if (!fakeceil.isCeiling()) fakeceil.Flip();
        sreg->fakefloor = (reg->regflags&sec_region_t::RF_SkipFloorSurf ? nullptr : CreateSecSurface(nullptr, sub, fakefloor));
        sreg->fakeceil = (reg->regflags&sec_region_t::RF_SkipCeilSurf ? nullptr : CreateSecSurface(nullptr, sub, fakeceil));
      }

      sreg->count = sub->numlines;
      if (ridx == 0 && sub->poly) sreg->count += sub->poly->numsegs; // polyobj
      if (pdsLeft < sreg->count) Sys_Error("out of drawsegs in surface creator");
      sreg->lines = pds;
      pds += sreg->count;
      pdsLeft -= sreg->count;
      for (int j = 0; j < sub->numlines; ++j) CreateSegParts(sub, &sreg->lines[j], &Level->Segs[sub->firstline+j], main_floor, main_ceiling, reg, (ridx == 0));

      if (ridx == 0 && sub->poly) {
        // polyobj
        int j = sub->numlines;
        seg_t **polySeg = sub->poly->segs;
        for (int polyCount = sub->poly->numsegs; polyCount--; ++polySeg, ++j) {
          CreateSegParts(sub, &sreg->lines[j], *polySeg, main_floor, main_ceiling, nullptr, true);
        }
      }

      sreg->next = sub->regions;
      sub->regions = sreg;

      ++sreg;
      --sregLeft;
    }

    if (inWorldCreation) R_PBarUpdate("Lighting", Level->NumSubsectors-i, Level->NumSubsectors);
  }

  if (inWorldCreation) R_PBarUpdate("Lighting", Level->NumSubsectors, Level->NumSubsectors, true);

  inWorldCreation = false;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateSubRegion
//
//==========================================================================
void VRenderLevelShared::UpdateSubRegion (subsector_t *sub, subregion_t *region, bool updatePoly) {
  TSecPlaneRef r_floor = region->floorplane;
  TSecPlaneRef r_ceiling = region->ceilplane;

  drawseg_t *ds = region->lines;
  for (int count = sub->numlines; count--; ++ds) {
    UpdateDrawSeg(sub, ds, r_floor, r_ceiling/*, ClipSegs*/);
  }

  if (region->realfloor) UpdateSecSurface(region->realfloor, region->floorplane, sub);
  if (region->realceil) UpdateSecSurface(region->realceil, region->ceilplane, sub);

  if (region->fakefloor) {
    TSecPlaneRef fakefloor;
    fakefloor.set(&sub->sector->fakefloors->floorplane, false);
    if (!fakefloor.isFloor()) fakefloor.Flip();
    if (!region->fakefloor->esecplane.isFloor()) region->fakefloor->esecplane.Flip();
    UpdateSecSurface(region->fakefloor, fakefloor, sub);
  }

  if (region->fakeceil) {
    TSecPlaneRef fakeceil;
    fakeceil.set(&sub->sector->fakefloors->ceilplane, false);
    if (!fakeceil.isCeiling()) fakeceil.Flip();
    if (!region->fakeceil->esecplane.isCeiling()) region->fakeceil->esecplane.Flip();
    UpdateSecSurface(region->fakeceil, fakeceil, sub);
  }

  if (updatePoly && sub->poly) {
    // update the polyobj
    updatePoly = false;
    seg_t **polySeg = sub->poly->segs;
    for (int polyCount = sub->poly->numsegs; polyCount--; ++polySeg) {
      UpdateDrawSeg(sub, (*polySeg)->drawsegs, r_floor, r_ceiling/*, ClipSegs*/);
    }
  }

  if (region->next) {
#if 0
    if (w_update_clip_region && !w_update_in_renderer) {
      if (!ViewClip.ClipCheckRegion(region->next, sub)) return;
    }
#endif
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
void VRenderLevelShared::UpdateFakeFlats (sector_t *sector) {
  const sector_t *hs = sector->heightsec;
  sector_t *heightsec = r_viewleaf->sector->heightsec;
  bool underwater = /*r_fakingunderwater ||*/
    //(heightsec && vieworg.z <= heightsec->floor.GetPointZ(vieworg));
    (hs && vieworg.z <= hs->floor.GetPointZ(vieworg));
  bool diffTex = !!(hs && hs->SectorFlags&sector_t::SF_ClipFakePlanes);

  // replace sector being drawn with a copy to be hacked
  fakefloor_t *ff = sector->fakefloors;
  if (!ff) return; //k8:just in case
  ff->floorplane = sector->floor;
  ff->ceilplane = sector->ceiling;
  ff->params = sector->params;

  // replace floor and ceiling height with control sector's heights
  if (diffTex && !(hs->SectorFlags&sector_t::SF_FakeCeilingOnly)) {
    if (CopyPlaneIfValid(&ff->floorplane, &hs->floor, &sector->ceiling)) {
      ff->floorplane.pic = hs->floor.pic;
      //GCon->Logf("opic=%d; fpic=%d", sector->floor.pic.id, hs->floor.pic.id);
    } else if (hs && (hs->SectorFlags&sector_t::SF_FakeFloorOnly)) {
      if (underwater) {
        //GCon->Logf("heightsec=%hs", (heightsec ? "tan" : "ona"));
        //tempsec->ColourMap = hs->ColourMap;
        ff->params.Fade = hs->params.Fade;
        if (!(hs->SectorFlags&sector_t::SF_NoFakeLight)) {
          ff->params.lightlevel = hs->params.lightlevel;
          ff->params.LightColour = hs->params.LightColour;
          /*
          if (floorlightlevel != nullptr) *floorlightlevel = GetFloorLight(hs);
          if (ceilinglightlevel != nullptr) *ceilinglightlevel = GetFloorLight(hs);
          */
          //ff->floorplane = (heightsec ? heightsec->floor : sector->floor);
        }
      }
      return;
    }
  } else {
    if (hs && !(hs->SectorFlags&sector_t::SF_FakeCeilingOnly)) {
      //ff->floorplane.normal = hs->floor.normal;
      //ff->floorplane.dist = hs->floor.dist;
      //GCon->Logf("  000");
      *(TPlane *)&ff->floorplane = *(TPlane *)&hs->floor;
    }
  }

  if (hs && !(hs->SectorFlags&sector_t::SF_FakeFloorOnly)) {
    if (diffTex) {
      if (CopyPlaneIfValid(&ff->ceilplane, &hs->ceiling, &sector->floor)) {
        ff->ceilplane.pic = hs->ceiling.pic;
      }
    } else {
      //ff->ceilplane.normal = hs->ceiling.normal;
      //ff->ceilplane.dist = hs->ceiling.dist;
      //GCon->Logf("  001");
      *(TPlane *)&ff->ceilplane = *(TPlane *)&hs->ceiling;
    }
  }

  //float refflorz = hs->floor.GetPointZ(viewx, viewy);
  float refceilz = (hs ? hs->ceiling.GetPointZ(vieworg) : 0); // k8: was `nullptr` -- wtf?!
  //float orgflorz = sector->floor.GetPointZ(viewx, viewy);
  float orgceilz = sector->ceiling.GetPointZ(vieworg);

  if (underwater /*||(heightsec && vieworg.z <= heightsec->floor.GetPointZ(vieworg))*/) {
    //!ff->floorplane.normal = sector->floor.normal;
    //!ff->floorplane.dist = sector->floor.dist;
    //!ff->ceilplane.normal = -hs->floor.normal;
    //!ff->ceilplane.dist = -hs->floor.dist/* - -hs->floor.normal.z*/;
    *(TPlane *)&ff->floorplane = *(TPlane *)&hs->floor;
    *(TPlane *)&ff->ceilplane = *(TPlane *)&hs->ceiling;
    //ff->ColourMap = hs->ColourMap;
    ff->params.Fade = hs->params.Fade;
  }

  // killough 11/98: prevent sudden light changes from non-water sectors:
  if ((underwater /*&& !back*/) || (heightsec && vieworg.z <= heightsec->floor.GetPointZ(vieworg))) {
    // head-below-floor hack
    ff->floorplane.pic = diffTex ? sector->floor.pic : hs->floor.pic;
    ff->floorplane.xoffs = hs->floor.xoffs;
    ff->floorplane.yoffs = hs->floor.yoffs;
    ff->floorplane.XScale = hs->floor.XScale;
    ff->floorplane.YScale = hs->floor.YScale;
    ff->floorplane.Angle = hs->floor.Angle;
    ff->floorplane.BaseAngle = hs->floor.BaseAngle;
    ff->floorplane.BaseYOffs = hs->floor.BaseYOffs;

    ff->ceilplane.normal = -hs->floor.normal;
    ff->ceilplane.dist = -hs->floor.dist/* - -hs->floor.normal.z*/;
    if (hs->ceiling.pic == skyflatnum) {
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
      ff->ceilplane.pic = diffTex ? hs->floor.pic : hs->ceiling.pic;
      ff->ceilplane.xoffs = hs->ceiling.xoffs;
      ff->ceilplane.yoffs = hs->ceiling.yoffs;
      ff->ceilplane.XScale = hs->ceiling.XScale;
      ff->ceilplane.YScale = hs->ceiling.YScale;
      ff->ceilplane.Angle = hs->ceiling.Angle;
      ff->ceilplane.BaseAngle = hs->ceiling.BaseAngle;
      ff->ceilplane.BaseYOffs = hs->ceiling.BaseYOffs;
    }

    if (!(hs->SectorFlags&sector_t::SF_NoFakeLight)) {
      ff->params.lightlevel = hs->params.lightlevel;
      ff->params.LightColour = hs->params.LightColour;
      /*
      if (floorlightlevel != nullptr) *floorlightlevel = GetFloorLight(hs);
      if (ceilinglightlevel != nullptr) *ceilinglightlevel = GetFloorLight(hs);
      */
    }
  } else if (((hs && vieworg.z > hs->ceiling.GetPointZ(vieworg)) || //k8: dunno, it was `floor` there, and it seems to be a typo
              (heightsec && vieworg.z > heightsec->ceiling.GetPointZ(vieworg))) &&
             orgceilz > refceilz && !(hs->SectorFlags&sector_t::SF_FakeFloorOnly))
  {
    // above-ceiling hack
    ff->ceilplane.normal = hs->ceiling.normal;
    ff->ceilplane.dist = hs->ceiling.dist;
    ff->floorplane.normal = -hs->ceiling.normal;
    ff->floorplane.dist = -hs->ceiling.dist;
    ff->params.Fade = hs->params.Fade;
    //ff->params.ColourMap = hs->params.ColourMap;

    ff->ceilplane.pic = diffTex ? sector->ceiling.pic : hs->ceiling.pic;
    ff->floorplane.pic = hs->ceiling.pic;
    ff->floorplane.xoffs = ff->ceilplane.xoffs = hs->ceiling.xoffs;
    ff->floorplane.yoffs = ff->ceilplane.yoffs = hs->ceiling.yoffs;
    ff->floorplane.XScale = ff->ceilplane.XScale = hs->ceiling.XScale;
    ff->floorplane.YScale = ff->ceilplane.YScale = hs->ceiling.YScale;
    ff->floorplane.Angle = ff->ceilplane.Angle = hs->ceiling.Angle;
    ff->floorplane.BaseAngle = ff->ceilplane.BaseAngle = hs->ceiling.BaseAngle;
    ff->floorplane.BaseYOffs = ff->ceilplane.BaseYOffs = hs->ceiling.BaseYOffs;

    if (hs->floor.pic != skyflatnum) {
      ff->ceilplane.normal = sector->ceiling.normal;
      ff->ceilplane.dist = sector->ceiling.dist;
      ff->floorplane.pic = hs->floor.pic;
      ff->floorplane.xoffs = hs->floor.xoffs;
      ff->floorplane.yoffs = hs->floor.yoffs;
      ff->floorplane.XScale = hs->floor.XScale;
      ff->floorplane.YScale = hs->floor.YScale;
      ff->floorplane.Angle = hs->floor.Angle;
    }

    if (!(hs->SectorFlags&sector_t::SF_NoFakeLight)) {
      ff->params.lightlevel  = hs->params.lightlevel;
      ff->params.LightColour = hs->params.LightColour;
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::UpdateDeepWater
//
//==========================================================================
void VRenderLevelShared::UpdateDeepWater (sector_t *sector) {
  if (!sector) return; // just in case
  const sector_t *ds = sector->deepref;

  if (!ds) return; // just in case

  // replace sector being drawn with a copy to be hacked
  fakefloor_t *ff = sector->fakefloors;
  if (!ff) return; //k8:just in case
  ff->floorplane = sector->floor;
  ff->ceilplane = sector->ceiling;
  ff->params = sector->params;

  ff->floorplane.normal = ds->floor.normal;
  ff->floorplane.dist = ds->floor.dist;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateFloodBug
//
//  emulate floodfill bug
//
//==========================================================================
void VRenderLevelShared::UpdateFloodBug (sector_t *sector) {
  if (!sector) return; // just in case
  fakefloor_t *ff = sector->fakefloors;
  if (!ff) return; // just in case
  // replace sector being drawn with a copy to be hacked
  ff->floorplane = sector->floor;
  ff->ceilplane = sector->ceiling;
  ff->params = sector->params;
  // floor
  if (sector->othersecFloor && sector->floor.minz < sector->othersecFloor->floor.minz) {
    ff->floorplane = sector->othersecFloor->floor;
    ff->params = sector->othersecFloor->params;
    ff->floorplane.LightSourceSector = (int)(ptrdiff_t)(sector->othersecFloor-Level->Sectors);
  }
  if (sector->othersecCeiling && sector->ceiling.minz > sector->othersecCeiling->ceiling.minz) {
    ff->ceilplane = sector->othersecCeiling->ceiling;
    ff->params = sector->othersecCeiling->params;
    ff->ceilplane.LightSourceSector = (int)(ptrdiff_t)(sector->othersecCeiling-Level->Sectors);
  }
}


//==========================================================================
//
//  VRenderLevelShared::SetupFakeFloors
//
//==========================================================================
void VRenderLevelShared::SetupFakeFloors (sector_t *sector) {
  if (!sector->deepref) {
    sector_t *HeightSec = sector->heightsec;
    if (HeightSec->SectorFlags&sector_t::SF_IgnoreHeightSec) return;
  }

  if (!sector->fakefloors) sector->fakefloors = new fakefloor_t;
  memset((void *)sector->fakefloors, 0, sizeof(fakefloor_t));
  sector->fakefloors->floorplane = sector->floor;
  sector->fakefloors->ceilplane = sector->ceiling;
  sector->fakefloors->params = sector->params;

  sector->eregions->params = &sector->fakefloors->params;
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
    surf->next = nullptr;
    // realloc first surface (if necessary)
    if (surf->count != vcount) {
      const size_t msize = sizeof(surface_t)+(vcount-1)*sizeof(TVec);
      surf = (surface_t *)Z_Realloc(surf, msize);
      memset((void *)surf, 0, msize);
    } else {
      memset((void *)surf, 0, sizeof(surface_t)+(vcount-1)*sizeof(TVec));
    }
    surf->count = vcount;
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
