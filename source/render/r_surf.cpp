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
#include "../gamedefs.h"
#include "r_local.h"
#include "../server/sv_local.h"

  // it seems that `segsidedef` offset is in effect, but scaling is not
//#define VV_SURFCTOR_3D_USE_SEGSIDEDEF_SCALE


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB r_hack_transparent_doors("r_hack_transparent_doors", true, "Transparent doors hack.", CVAR_Archive);
static VCvarB r_hack_zero_sky("r_hack_zero_sky", true, "ZeroSky hack (Doom II MAP01 extra floor fix).", CVAR_Archive);

static VCvarB r_3dfloor_clip_both_sides("r_3dfloor_clip_both_sides", false, "Clip 3d floors with both sectors?", CVAR_Archive);

static VCvarB r_hack_fake_floor_decorations("r_hack_fake_floor_decorations", true, "Fake floor/ceiling decoration fix.", /*CVAR_Archive|*/CVAR_PreInit);

VCvarB r_fix_tjunctions("r_fix_tjunctions", true, "Fix t-junctions to avoid occasional white dots?", CVAR_Archive);


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
//==========================================================================
static void AppendSurfaces (segpart_t *sp, surface_t *newsurfs) {
  vassert(sp);
  if (!newsurfs) return; // nothing to do
  // new list will start with `newsurfs`
  surface_t *ss = sp->surfs;
  if (ss) {
    // append
    while (ss->next) ss = ss->next;
    ss->next = newsurfs;
  } else {
    sp->surfs = newsurfs;
  }
}


//==========================================================================
//
//  VRenderLevelShared::SetupSky
//
//==========================================================================
void VRenderLevelShared::SetupSky () {
  skyheight = -99999.0f;
  for (auto &&sec : Level->allSectors()) {
    if (sec.ceiling.pic == skyflatnum && sec.ceiling.maxz > skyheight) {
      skyheight = sec.ceiling.maxz;
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
//  IsZeroSkyFloorHack
//
//==========================================================================
static inline bool IsZeroSkyFloorHack (const subsector_t *sub, const sec_region_t *reg) {
  // if current sector has zero height, and its ceiling is sky, and its floor is not sky, skip floor creation
  return
    (reg->regflags&sec_region_t::RF_BaseRegion) &&
    reg->eceiling.splane->pic == skyflatnum &&
    reg->efloor.splane->pic != skyflatnum &&
    sub->sector->floor.normal.z == 1.0f && sub->sector->ceiling.normal.z == -1.0f &&
    sub->sector->floor.minz == sub->sector->ceiling.minz;
}


//==========================================================================
//
//  SurfRecalcFlatOffset
//
//  returns `true` if any offset was changed
//
//==========================================================================
static inline void SurfRecalcFlatOffset (sec_surface_t *surf, const TSecPlaneRef &spl, VTexture *Tex) {
  const float newsoffs = spl.splane->xoffs*(TextureSScale(Tex)*spl.splane->XScale);
  const float newtoffs = (spl.splane->yoffs+spl.splane->BaseYOffs)*(TextureTScale(Tex)*spl.splane->YScale);
  /*
  const bool offsChanged = (FASI(surf->texinfo.soffs) != FASI(newsoffs) ||
                           FASI(surf->texinfo.toffs) != FASI(newtoffs));
  */
  surf->texinfo.soffs = newsoffs;
  surf->texinfo.toffs = newtoffs;
  //return offsChanged;
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
sec_surface_t *VRenderLevelShared::CreateSecSurface (sec_surface_t *ssurf, subsector_t *sub, TSecPlaneRef InSplane, subregion_t *sreg, bool fake) {
  int vcount = sub->numlines;

  if (vcount < 3) {
    GCon->Logf(NAME_Warning, "CreateSecSurface: subsector #%d of sector #%d has only #%d vertices", (int)(ptrdiff_t)(sub-Level->Subsectors), (sub->sector ? (int)(ptrdiff_t)(sub->sector-Level->Sectors) : -1), vcount);
    if (vcount < 1) Sys_Error("ONE VERTEX. WTF?!");
    if (ssurf) return ssurf;
  }
  //vassert(vcount >= 3);

  // if current sector has zero height, and its ceiling is sky, and its floor is not sky, skip floor creation
  // this is what removes extra floors on Doom II MAP01, for example
  if (!fake && r_hack_zero_sky && IsZeroSkyFloorHack(sub, sreg->secregion)) {
    sreg->flags |= subregion_t::SRF_ZEROSKY_FLOOR_HACK;
    // we still need to create this floor, because it may be reactivated later
  } else {
    sreg->flags &= ~subregion_t::SRF_ZEROSKY_FLOOR_HACK;
  }

  // if we're simply changing sky, and already have surface created, do not recreate it, it is pointless
  bool isSkyFlat = (InSplane.splane->pic == skyflatnum);
  bool recreateSurface = true;
  bool updateZ = false;

  //if (R_IsStackedSectorPlane(InSplane.splane)) isSkyFlat = true;

  // fix plane
  TSecPlaneRef spl(InSplane);
  if (isSkyFlat && spl.GetNormalZ() < 0.0f) spl.set(&sky_plane, false);

  surface_t *surf = nullptr;
  if (!ssurf) {
    // new sector surface
    ssurf = new sec_surface_t;
    memset((void *)ssurf, 0, sizeof(sec_surface_t));
    surf = NewWSurf(vcount);
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

  vuint32 typeFlags = (spl.GetNormalZ() > 0.0f ? surface_t::TF_FLOOR : surface_t::TF_CEILING);

  // this is required to calculate static lightmaps, and for other business
  for (surface_t *ss = surf; ss; ss = ss->next) {
    ss->subsector = sub;
    ss->typeFlags = typeFlags;
  }

  ssurf->esecplane = spl;
  ssurf->edist = spl.splane->dist;

  // setup texture
  VTexture *Tex = GTextureManager(spl.splane->pic);
  if (!Tex) Tex = GTextureManager[GTextureManager.DefaultTexture];
  if (fabsf(spl.splane->normal.z) > 0.1f) {
    float s, c;
    msincos(spl.splane->BaseAngle-spl.splane->Angle, &s, &c);
    ssurf->texinfo.saxisLM = TVec(c,  s, 0);
    ssurf->texinfo.saxis = TVec(c,  s, 0)*(TextureSScale(Tex)*spl.splane->XScale);
    ssurf->texinfo.taxisLM = TVec(s, -c, 0);
    ssurf->texinfo.taxis = TVec(s, -c, 0)*(TextureTScale(Tex)*spl.splane->YScale);
  } else {
    ssurf->texinfo.taxisLM = TVec(0, 0, -1);
    ssurf->texinfo.taxis = TVec(0, 0, -1)*(TextureTScale(Tex)*spl.splane->YScale);
    ssurf->texinfo.saxisLM = Normalise(CrossProduct(spl.GetNormal(), ssurf->texinfo.taxisLM));
    ssurf->texinfo.saxis = Normalise(CrossProduct(spl.GetNormal(), ssurf->texinfo.taxis))*(TextureSScale(Tex)*spl.splane->XScale);
  }

  /*bool offsChanged = */SurfRecalcFlatOffset(ssurf, spl, Tex);

  ssurf->texinfo.Tex = Tex;
  ssurf->texinfo.noDecals = (Tex ? Tex->noDecals : true);
  ssurf->texinfo.Alpha = (spl.splane->Alpha < 1.0f ? spl.splane->Alpha : 1.1f);
  ssurf->texinfo.Additive = !!(spl.splane->flags&SPF_ADDITIVE);
  ssurf->texinfo.ColorMap = 0;
  ssurf->XScale = spl.splane->XScale;
  ssurf->YScale = spl.splane->YScale;
  ssurf->Angle = spl.splane->BaseAngle-spl.splane->Angle;

  TPlane plane = *(TPlane *)spl.splane;
  if (spl.flipped) plane.flipInPlace();

  if (recreateSurface) {
    surf->plane = plane;
    surf->count = vcount;
    const seg_t *seg = &Level->Segs[sub->firstline];
    SurfVertex *dptr = surf->verts;
    if (spl.GetNormalZ() < 0.0f) {
      // backward
      for (seg += (vcount-1); vcount--; ++dptr, --seg) {
        const TVec &v = *seg->v1;
        dptr->setVec(v.x, v.y, spl.GetPointZ(v.x, v.y));
      }
    } else {
      // forward
      for (; vcount--; ++dptr, ++seg) {
        const TVec &v = *seg->v1;
        dptr->setVec(v.x, v.y, spl.GetPointZ(v.x, v.y));
      }
    }

    if (isSkyFlat) {
      // don't subdivide sky, as it cannot have lightmap
      ssurf->surfs = surf;
      surf->texinfo = &ssurf->texinfo;
    } else {
      //!GCon->Logf(NAME_Debug, "sfcF:%p: saxis=(%g,%g,%g); taxis=(%g,%g,%g); saxisLM=(%g,%g,%g); taxisLM=(%g,%g,%g)", ssurf, ssurf->texinfo.saxis.x, ssurf->texinfo.saxis.y, ssurf->texinfo.saxis.z, ssurf->texinfo.taxis.x, ssurf->texinfo.taxis.y, ssurf->texinfo.taxis.z, ssurf->texinfo.saxisLM.x, ssurf->texinfo.saxisLM.y, ssurf->texinfo.saxisLM.z, ssurf->texinfo.taxisLM.x, ssurf->texinfo.taxisLM.y, ssurf->texinfo.taxisLM.z);
      ssurf->surfs = SubdivideFace(surf, ssurf->texinfo.saxisLM, &ssurf->texinfo.taxisLM);
      InitSurfs(true, ssurf->surfs, &ssurf->texinfo, &plane, sub); // recalc static lightmaps
    }
  } else if (updateZ) {
    // update z coords
    bool changed = false;
    for (; surf; surf = surf->next) {
      SurfVertex *svert = surf->verts;
      for (int i = surf->count; i--; ++svert) {
        const float oldZ = svert->z;
        svert->z = spl.GetPointZ(svert->x, svert->y);
        if (!changed && FASI(oldZ) != FASI(svert->z)) changed = true;
      }
      if (changed) InitSurfs(true, ssurf->surfs, &ssurf->texinfo, &plane, sub); // recalc static lightmaps
    }
  }
  /*k8: no, lightmap doesn't depend of texture axes anymore
  else if (offsChanged) {
    // still have to force it, because texture is scrolled, and lightmap s/t are invalid
    InitSurfs(true, ssurf->surfs, &ssurf->texinfo, &plane, sub);
  }
  */

  return ssurf;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateSecSurface
//
//  this is used to update floor and ceiling surfaces
//
//==========================================================================
void VRenderLevelShared::UpdateSecSurface (sec_surface_t *ssurf, TSecPlaneRef RealPlane, subsector_t *sub, subregion_t *sreg, bool ignoreColorMap, bool fake) {
  if (!ssurf->esecplane.splane->pic) return; // no texture? nothing to do

  TSecPlaneRef splane(ssurf->esecplane);

  if (splane.splane != RealPlane.splane) {
    // check for sky changes
    if ((splane.splane->pic == skyflatnum) != (RealPlane.splane->pic == skyflatnum)) {
      // sky <-> non-sky, simply recreate it
      sec_surface_t *newsurf = CreateSecSurface(ssurf, sub, RealPlane, sreg, fake);
      vassert(newsurf == ssurf); // sanity check
      ssurf->texinfo.ColorMap = ColorMap; // just in case
      // nothing more to do
      return;
    }
    // substitute real plane with sky plane if necessary
    if (RealPlane.splane->pic == skyflatnum && RealPlane.GetNormalZ() < 0.0f) {
      if (splane.splane != &sky_plane) {
        // recreate it, just in case
        sec_surface_t *newsurf = CreateSecSurface(ssurf, sub, RealPlane, sreg, fake);
        vassert(newsurf == ssurf); // sanity check
        ssurf->texinfo.ColorMap = ColorMap; // just in case
        // nothing more to do
        return;
      }
      splane.set(&sky_plane, false);
    }
  }

  enum { USS_Normal, USS_Force, USS_IgnoreCMap, USS_ForceIgnoreCMap };

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
    sec_surface_t *newsurf = CreateSecSurface(ssurf, sub, RealPlane, sreg, fake);
    vassert(newsurf == ssurf); // sanity check
    ssurf->texinfo.ColorMap = (!ignoreColorMap ? ColorMap : 0); // just in case
    // nothing more to do
    return;
  }

  if (!ignoreColorMap) ssurf->texinfo.ColorMap = ColorMap; // just in case
  // ok, we still may need to update texture or z coords
  // update texture?
  VTexture *Tex = GTextureManager(splane.splane->pic);
  if (!Tex) Tex = GTextureManager[GTextureManager.DefaultTexture];
  /*bool offsChanged = */SurfRecalcFlatOffset(ssurf, splane, Tex);
  ssurf->texinfo.Tex = Tex;
  ssurf->texinfo.noDecals = Tex->noDecals;

  // update z coords?
  if (FASI(ssurf->edist) != FASI(splane.splane->dist)) {
    TPlane plane = *(TPlane *)splane.splane;
    if (splane.flipped) plane.flipInPlace();
    bool changed = false;
    ssurf->edist = splane.splane->dist;
    for (surface_t *surf = ssurf->surfs; surf; surf = surf->next) {
      surf->plane = plane;
      SurfVertex *svert = surf->verts;
      for (int i = surf->count; i--; ++svert) {
        const float oldZ = svert->z;
        svert->z = splane.GetPointZ(svert->x, svert->y);
        if (!changed && FASI(oldZ) != FASI(svert->z)) changed = true;
      }
    }
    // force lightmap recalculation
    if (changed || splane.splane->pic != skyflatnum) {
      InitSurfs(true, ssurf->surfs, &ssurf->texinfo, &plane, sub); // recalc static lightmaps
    }
  }
  /*k8: no, lightmap doesn't depend of texture axes anymore
  else if (offsChanged && splane.splane->pic != skyflatnum) {
    // still have to force it, because texture is scrolled, and lightmap s/t are invalid
    TPlane plane = *(TPlane *)splane.splane;
    if (splane.flipped) plane.flipInPlace();
    InitSurfs(true, ssurf->surfs, &ssurf->texinfo, &plane, sub);
  }
  */
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
surface_t *VRenderLevelShared::NewWSurf (int vcount) {
  vassert(vcount >= 0);
  enum { WSURFSIZE = sizeof(surface_t)+sizeof(SurfVertex)*(surface_t::MAXWVERTS-1) };
  if (vcount > surface_t::MAXWVERTS) {
    const int vcnt = (vcount|3)+1;
    surface_t *res = (surface_t *)Z_Calloc(sizeof(surface_t)+(vcnt-1)*sizeof(SurfVertex));
    res->count = vcount;
    return res;
  }
  // fits into "standard" surface
  if (!free_wsurfs) {
    // allocate some more surfs
    vuint8 *tmp = (vuint8 *)Z_Calloc(WSURFSIZE*4096+sizeof(void *));
    *(void **)tmp = AllocatedWSurfBlocks;
    AllocatedWSurfBlocks = tmp;
    tmp += sizeof(void *);
    for (int i = 0; i < 4096; ++i, tmp += WSURFSIZE) {
      ((surface_t *)tmp)->next = free_wsurfs;
      free_wsurfs = (surface_t *)tmp;
    }
  }
  surface_t *surf = free_wsurfs;
  free_wsurfs = surf->next;

  memset((void *)surf, 0, WSURFSIZE);
  surf->allocflags = surface_t::ALLOC_WORLD;

  surf->count = vcount;
  return surf;
}


//==========================================================================
//
//  VRenderLevelShared::FreeWSurfs
//
//==========================================================================
void VRenderLevelShared::FreeWSurfs (surface_t *&InSurfs) {
  surface_t *surfs = InSurfs;
  FlushSurfCaches(surfs);
  while (surfs) {
    surfs->FreeLightmaps();
    surface_t *next = surfs->next;
    if (surfs->isWorldAllocated()) {
      surfs->next = free_wsurfs;
      free_wsurfs = surfs;
    } else {
      Z_Free(surfs);
    }
    surfs = next;
  }
  InSurfs = nullptr;
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
  vassert(vcount >= 0); // just in case
  surface_t *surf = surfs;
  if (surf) {
    const int maxcount = (surf->isWorldAllocated() ? surface_t::MAXWVERTS : surf->count);
    if (vcount > maxcount) {
      FreeWSurfs(surf);
      return NewWSurf(vcount);
    }
    // free surface chain
    if (surf->next) { FreeWSurfs(surf->next); surf->next = nullptr; }
    if (surf->CacheSurf) FreeSurfCache(surf->CacheSurf);
    surf->FreeLightmaps();
    memset((void *)surf, 0, sizeof(surface_t)+(vcount-1)*sizeof(SurfVertex));
    surf->count = vcount;
    return surf;
  } else {
    return NewWSurf(vcount);
  }
}


//==========================================================================
//
//  VRenderLevelShared::CreateWSurf
//
//  this is used to create world/wall surface
//
//==========================================================================
surface_t *VRenderLevelShared::CreateWSurf (TVec *wv, texinfo_t *texinfo, seg_t *seg, subsector_t *sub, int wvcount, vuint32 typeFlags) {
  if (wvcount < 3) return nullptr;
  if (wvcount == 4 && (wv[1].z <= wv[0].z && wv[2].z <= wv[3].z)) return nullptr;
  if (wvcount > surface_t::MAXWVERTS) Sys_Error("cannot create huge world surface (the thing that should not be)");

  if (!texinfo->Tex || texinfo->Tex->Type == TEXTYPE_Null) return nullptr;

  surface_t *surf = NewWSurf(wvcount);
  surf->subsector = sub;
  surf->seg = seg;
  surf->next = nullptr;
  surf->count = wvcount;
  surf->typeFlags = typeFlags;
  //memcpy(surf->verts, wv, wvcount*sizeof(SurfVertex));
  //memset((void *)surf->verts, 0, wvcount*sizeof(SurfVertex));
  for (int f = 0; f < wvcount; ++f) surf->verts[f].setVec(wv[f]);

  if (texinfo->Tex == GTextureManager[skyflatnum]) {
    // never split sky surfaces
    surf->texinfo = texinfo;
    surf->plane = *(TPlane *)seg;
    return surf;
  }

  //!GCon->Logf(NAME_Debug, "sfcS:%p: saxis=(%g,%g,%g); taxis=(%g,%g,%g); saxisLM=(%g,%g,%g); taxisLM=(%g,%g,%g)", surf, texinfo->saxis.x, texinfo->saxis.y, texinfo->saxis.z, texinfo->taxis.x, texinfo->taxis.y, texinfo->taxis.z, texinfo->saxisLM.x, texinfo->saxisLM.y, texinfo->saxisLM.z, texinfo->taxisLM.x, texinfo->taxisLM.y, texinfo->taxisLM.z);
  surf = SubdivideSeg(surf, texinfo->saxisLM, &texinfo->taxisLM, seg);
  InitSurfs(true, surf, texinfo, seg, sub); // recalc static lightmaps
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
void VRenderLevelShared::CreateWorldSurfFromWV (subsector_t *sub, seg_t *seg, segpart_t *sp, TVec wv[4], vuint32 typeFlags, bool doOffset) {
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

  AppendSurfaces(sp, CreateWSurf(wstart, &sp->texinfo, seg, sub, wcount, typeFlags));
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
  sp->texinfo.ColorMap = 0;

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

    CreateWorldSurfFromWV(sub, seg, sp, wv, 0); // sky texture, no type flags
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = 0.0f;
  sp->backBotDist = 0.0f;
  sp->frontFakeFloorDist = 0.0f;
  sp->frontFakeCeilDist = 0.0f;
  sp->backFakeFloorDist = 0.0f;
  sp->backFakeCeilDist = 0.0f;
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
  sp->texinfo.ColorMap = 0;

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

    CreateWorldSurfFromWV(sub, seg, sp, wv, 0); // sky texture, no type flags
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = 0.0f;
  sp->backBotDist = 0.0f;
  sp->frontFakeFloorDist = 0.0f;
  sp->frontFakeCeilDist = 0.0f;
  sp->backFakeFloorDist = 0.0f;
  sp->backFakeCeilDist = 0.0f;
}


//==========================================================================
//
//  SetupTextureAxesOffset
//
//  used only for normal wall textures: top, mid, bottom
//
//==========================================================================
static inline void SetupTextureAxesOffset (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam) {
  texinfo->Tex = tex;
  texinfo->noDecals = tex->noDecals;
  // can be fixed later
  texinfo->Alpha = 1.1f;
  texinfo->Additive = false;
  texinfo->ColorMap = 0;

  texinfo->saxisLM = seg->dir;
  texinfo->saxis = seg->dir*(TextureSScale(tex)*tparam->ScaleX);
  texinfo->taxisLM = TVec(0, 0, -1);
  texinfo->taxis = TVec(0, 0, -1)*(TextureTScale(tex)*tparam->ScaleY);

  texinfo->soffs = -DotProduct(*seg->v1, texinfo->saxis)+
                   seg->offset*(TextureSScale(tex)*tparam->ScaleX)+
                   tparam->TextureOffset*TextureOffsetSScale(tex);
  texinfo->toffs = 0.0f;
  // toffs is not calculated here, as its calculation depends of texture position and pegging
}


//==========================================================================
//
//  SetupTextureAxesOffsetEx
//
//  used for 3d floor extra midtextures
//
//==========================================================================
static inline void SetupTextureAxesOffsetEx (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam, const side_tex_params_t *tparam2) {
  texinfo->Tex = tex;
  texinfo->noDecals = tex->noDecals;
  // can be fixed later
  texinfo->Alpha = 1.1f;
  texinfo->Additive = false;
  texinfo->ColorMap = 0;

  #ifdef VV_SURFCTOR_3D_USE_SEGSIDEDEF_SCALE
  const float scale2X = tparam2->ScaleY;
  const float scale2Y = tparam2->ScaleY;
  #else
  // it seems that `segsidedef` offset is in effect, but scaling is not
  const float scale2X = 1.0f;
  const float scale2Y = 1.0f;
  #endif

  texinfo->saxisLM = seg->dir;
  texinfo->saxis = seg->dir*(TextureSScale(tex)*tparam->ScaleX*scale2X);
  texinfo->taxisLM = TVec(0, 0, -1);
  texinfo->taxis = TVec(0, 0, -1)*(TextureTScale(tex)*tparam->ScaleY*scale2Y);

  texinfo->soffs = -DotProduct(*seg->v1, texinfo->saxis)+
                   seg->offset*(TextureSScale(tex)*tparam->ScaleX*scale2X)+
                   (tparam->TextureOffset+tparam2->TextureOffset)*TextureOffsetSScale(tex);

  texinfo->toffs = 0.0f;
  // toffs is not calculated here, as its calculation depends of texture position and pegging
}


//==========================================================================
//
//  IsTransDoorHack
//
//  HACK: sector with height of 1, and only middle
//  masked texture is "transparent door"
//
//  actually, 2s "door" wall without top/bottom textures, and with masked
//  midtex is "transparent door"
//
//==========================================================================
static bool IsTransDoorHack (const seg_t *seg, bool fortop) {
  const sector_t *secs[2] = { seg->frontsector, seg->backsector };
  if (!secs[0] || !secs[1]) return false;
  const side_t *sidedef = seg->sidedef;
  if (!GTextureManager.IsEmptyTexture(fortop ? sidedef->TopTexture : sidedef->BottomTexture)) return false;
  // if we have don't have a midtex, it is not a door hack
  if (GTextureManager.IsEmptyTexture(sidedef->MidTexture)) return false;
  // check for slopes
  if (secs[0]->floor.normal.z != 1.0f || secs[0]->ceiling.normal.z != -1.0f) return false;
  if (secs[1]->floor.normal.z != 1.0f || secs[1]->ceiling.normal.z != -1.0f) return false;
  // check for door
  //!if (secs[0]->floor.minz >= secs[1]->ceiling.minz) return false;
  // check for middle texture
  // if we have midtex, it is door hack
  //VTexture *tex = GTextureManager[sidedef->MidTexture];
  //if (!tex || !tex->isTrasparent()) return false;
  //!return !GTextureManager.IsEmptyTexture(sidedef->MidTexture);
  //if (!mt || mt->Type == TEXTYPE_Null || !mt->isTransparent()) return false;
  // ok, looks like it
  return true;
}


//==========================================================================
//
//  IsTransDoorHackTop
//
//==========================================================================
static inline bool IsTransDoorHackTop (const seg_t *seg) {
  return IsTransDoorHack(seg, true);
}


//==========================================================================
//
//  IsTransDoorHackBot
//
//==========================================================================
static inline VVA_OKUNUSED bool IsTransDoorHackBot (const seg_t *seg) {
  return IsTransDoorHack(seg, false);
}


//==========================================================================
//
//  DivByScale
//
//==========================================================================
static inline float DivByScale (float v, float scale) {
  return (scale > 0 ? v/scale : v);
}


//==========================================================================
//
//  DivByScale2
//
//==========================================================================
static inline float DivByScale2 (float v, float scale, float scale2) {
  if (scale2 <= 0.0f) return DivByScale(v, scale);
  if (scale <= 0.0f) return DivByScale(v, scale2);
  return v/(scale*scale2);
}


//==========================================================================
//
//  GetMinFloorZWithFake
//
//  get max floor height for given floor and fake floor
//
//==========================================================================
// i haet shitpp templates!
#define GetFixedZWithFake(func_,plname_,v_,sec_,r_plane_)  \
  ((sec_) && (sec_)->heightsec ? func_((r_plane_).GetPointZ(v_), (sec_)->heightsec->plname_.GetPointZ(v_)) : (r_plane_).GetPointZ(v_))


//==========================================================================
//
//  SetupFakeDistances
//
//==========================================================================
static inline void SetupFakeDistances (const seg_t *seg, segpart_t *sp) {
  if (seg->frontsector->heightsec) {
    sp->frontFakeFloorDist = seg->frontsector->heightsec->floor.dist;
    sp->frontFakeCeilDist = seg->frontsector->heightsec->ceiling.dist;
  } else {
    sp->frontFakeFloorDist = 0.0f;
    sp->frontFakeCeilDist = 0.0f;
  }

  if (seg->backsector && seg->backsector->heightsec) {
    sp->backFakeFloorDist = seg->backsector->heightsec->floor.dist;
    sp->backFakeCeilDist = seg->backsector->heightsec->ceiling.dist;
  } else {
    sp->backFakeFloorDist = 0.0f;
    sp->backFakeCeilDist = 0.0f;
  }
}


//==========================================================================
//
//  CheckFakeDistances
//
//  returns `true` if something was changed
//
//==========================================================================
static inline bool CheckFakeDistances (const seg_t *seg, const segpart_t *sp) {
  if (seg->frontsector->heightsec) {
    if (FASI(sp->frontFakeFloorDist) != FASI(seg->frontsector->heightsec->floor.dist) ||
        FASI(sp->frontFakeCeilDist) != FASI(seg->frontsector->heightsec->ceiling.dist))
    {
      return true;
    }
  }

  if (seg->backsector && seg->backsector->heightsec) {
    return
      FASI(sp->backFakeFloorDist) != FASI(seg->backsector->heightsec->floor.dist) ||
      FASI(sp->backFakeCeilDist) != FASI(seg->backsector->heightsec->ceiling.dist);
  }

  return false;
}


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
  if (r_ceiling.splane->SkyBox != back_ceiling->SkyBox &&
      R_IsStrictlySkyFlatPlane(r_ceiling.splane) && R_IsStrictlySkyFlatPlane(back_ceiling))
  {
    TTex = GTextureManager[skyflatnum];
  }

  // HACK: sector with height of 1, and only middle masked texture is "transparent door"
  //       also, invert "upper unpegged" flag for this case
  int peghack = 0;
  unsigned hackflag = 0;
  //if (r_hack_transparent_doors && TTex->Type == TEXTYPE_Null) GCon->Logf(NAME_Debug, "line #%d, side #%d: transdoor check=%d", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(sidedef-&Level->Sides[0]), IsTransDoorHackTop(seg));
  if (r_hack_transparent_doors && TTex->Type == TEXTYPE_Null && IsTransDoorHackTop(seg)) {
    TTex = GTextureManager(sidedef->MidTexture);
    //peghack = ML_DONTPEGTOP;
    hackflag = surface_t::TF_TOPHACK;
  }

  SetupTextureAxesOffset(seg, &sp->texinfo, TTex, &sidedef->Top);

  if (TTex->Type != TEXTYPE_Null) {
    TVec wv[4];

    // see `SetupTwoSidedBotWSurf()` for explanation
    const float topz1 = (r_hack_fake_floor_decorations ? GetFixedZWithFake(max2, ceiling, *seg->v1, seg->frontsector, r_ceiling) : r_ceiling.GetPointZ(*seg->v1));
    const float topz2 = (r_hack_fake_floor_decorations ? GetFixedZWithFake(max2, ceiling, *seg->v2, seg->frontsector, r_ceiling) : r_ceiling.GetPointZ(*seg->v2));
    const float botz1 = r_floor.GetPointZ(*seg->v1);
    const float botz2 = r_floor.GetPointZ(*seg->v2);

    // see `SetupTwoSidedBotWSurf()` for explanation
    const float back_topz1 = (r_hack_fake_floor_decorations ? GetFixedZWithFake(min2, ceiling, *seg->v1, seg->backsector, (*back_ceiling)) : back_ceiling->GetPointZ(*seg->v1));
    const float back_topz2 = (r_hack_fake_floor_decorations ? GetFixedZWithFake(min2, ceiling, *seg->v2, seg->backsector, (*back_ceiling)) : back_ceiling->GetPointZ(*seg->v2));

    // hack to allow height changes in outdoor areas
    float top_topz1 = topz1;
    float top_topz2 = topz2;
    float top_TexZ = r_ceiling.splane->TexZ;
    if (r_ceiling.splane->SkyBox == back_ceiling->SkyBox &&
        R_IsStrictlySkyFlatPlane(r_ceiling.splane) && R_IsStrictlySkyFlatPlane(back_ceiling))
    {
      top_topz1 = back_topz1;
      top_topz2 = back_topz2;
      top_TexZ = back_ceiling->TexZ;
    }

    const float tscale = TextureTScale(TTex)*sidedef->Top.ScaleY;
    if ((linedef->flags&ML_DONTPEGTOP)^peghack) {
      // top of texture at top
      sp->texinfo.toffs = top_TexZ*tscale;
    } else {
      // bottom of texture
      //GCon->Logf(NAME_Debug, "line #%d, side #%d: tz=%g; sch=%d; scy=%g", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(sidedef-&Level->Sides[0]), back_ceiling->TexZ, TTex->GetScaledHeight(), sidedef->Top.ScaleY);
      sp->texinfo.toffs = back_ceiling->TexZ*tscale+TTex->Height;
    }
    sp->texinfo.toffs += sidedef->Top.RowOffset*TextureOffsetTScale(TTex);

    wv[0].x = wv[1].x = seg->v1->x;
    wv[0].y = wv[1].y = seg->v1->y;
    wv[2].x = wv[3].x = seg->v2->x;
    wv[2].y = wv[3].y = seg->v2->y;

    wv[0].z = max2(back_topz1, botz1);
    wv[1].z = top_topz1;
    wv[2].z = top_topz2;
    wv[3].z = max2(back_topz2, botz2);

    bool createSurf = true;

    //FIXME: this is totally wrong with slopes!
    if (seg->backsector->heightsec && r_hack_fake_floor_decorations) {
      // do not create outer top texture surface if our fake ceiling is higher than the surrounding ceiling
      // otherwise, make sure that it is not lower than the fake ceiling (simc)
      const sector_t *bsec = seg->backsector;
      const sector_t *fsec = seg->frontsector;
      const sector_t *hsec = bsec->heightsec;
      if (hsec->ceiling.minz >= fsec->ceiling.minz) {
        //GCon->Logf(NAME_Debug, "BSH: %d (%d) -- SKIP", (int)(ptrdiff_t)(bsec-&Level->Sectors[0]), (int)(ptrdiff_t)(hsec-&Level->Sectors[0]));
        createSurf = false;
      } else if (hsec->ceiling.minz >= bsec->ceiling.minz) {
        //GCon->Logf(NAME_Debug, "BSH: %d (%d) -- FIX", (int)(ptrdiff_t)(bsec-&Level->Sectors[0]), (int)(ptrdiff_t)(hsec-&Level->Sectors[0]));
        wv[0].z = max2(wv[0].z, hsec->ceiling.GetPointZ(*seg->v1));
        wv[3].z = max2(wv[3].z, hsec->ceiling.GetPointZ(*seg->v2));
        //createSurf = false;
      }
    }

    if (createSurf) {
      CreateWorldSurfFromWV(sub, seg, sp, wv, surface_t::TF_TOP|hackflag);
    }
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = back_ceiling->dist;
  sp->backBotDist = back_floor->dist;
  sp->TextureOffset = sidedef->Top.TextureOffset;
  sp->RowOffset = sidedef->Top.RowOffset;
  SetupFakeDistances(seg, sp);
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

  // HACK: sector with height of 1, and only middle masked texture is "transparent door"
  //       also, invert "lower unpegged" flag for this case
  int peghack = 0;
  unsigned hackflag = 0;
  //if (r_hack_transparent_doors && TTex->Type == TEXTYPE_Null) GCon->Logf(NAME_Debug, "line #%d, side #%d: transdoor check=%d", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(sidedef-&Level->Sides[0]), IsTransDoorHackTop(seg));
  if (r_hack_transparent_doors && BTex->Type == TEXTYPE_Null && IsTransDoorHackBot(seg)) {
    BTex = GTextureManager(sidedef->MidTexture);
    //peghack = ML_DONTPEGBOTTOM;
    hackflag = surface_t::TF_TOPHACK;
  }

  SetupTextureAxesOffset(seg, &sp->texinfo, BTex, &sidedef->Bot);

  if (BTex->Type != TEXTYPE_Null) {
    TVec wv[4];

    float topz1 = r_ceiling.GetPointZ(*seg->v1);
    float topz2 = r_ceiling.GetPointZ(*seg->v2);

    // some map authors are making floor decorations with height transer
    // (that is so player won't wobble walking on such floors)
    // so we should use minimum front height here (sigh)
    //FIXME: this is totally wrong, because we may have alot of different
    //       configurations here, and we should check for them all
    //FIXME: also, moving height sector should trigger surface recreation
    float botz1 = (r_hack_fake_floor_decorations ? GetFixedZWithFake(min2, floor, *seg->v1, seg->frontsector, r_floor) : r_floor.GetPointZ(*seg->v1));
    float botz2 = (r_hack_fake_floor_decorations ? GetFixedZWithFake(min2, floor, *seg->v2, seg->frontsector, r_floor) : r_floor.GetPointZ(*seg->v2));
    float top_TexZ = r_ceiling.splane->TexZ;

    // same height fix as above
    float back_botz1 = (r_hack_fake_floor_decorations ? GetFixedZWithFake(max2, floor, *seg->v1, seg->backsector, (*back_floor)) : back_floor->GetPointZ(*seg->v1));
    float back_botz2 = (r_hack_fake_floor_decorations ? GetFixedZWithFake(max2, floor, *seg->v2, seg->backsector, (*back_floor)) : back_floor->GetPointZ(*seg->v2));

    /* k8: boomedit.wad -- i can't make heads or tails of this crap; when it should be rendered, and when it shouldn't? */
    /*     this is total hack to make boomedit.wad underwater look acceptable; but why it is like this? */
    {
      const sector_t *fhsec = seg->frontsector->heightsec;
      const sector_t *bhsec = seg->backsector->heightsec;
      if (fhsec && bhsec && ((fhsec->SectorFlags|bhsec->SectorFlags)&(sector_t::SF_TransferSource|sector_t::SF_FakeBoomMask)) == sector_t::SF_TransferSource) {
        if (fhsec == bhsec || // same sector (boomedit.wad stairs)
            (fhsec->floor.dist == bhsec->floor.dist && fhsec->floor.normal == bhsec->floor.normal)) // or same floor height (boomedit.wad lift)
        {
          back_botz1 = seg->backsector->floor.GetPointZ(*seg->v1);
          back_botz2 = seg->backsector->floor.GetPointZ(*seg->v2);
        }
        else if (fhsec->floor.dist < bhsec->floor.dist) {
          // no, i don't know either, but this fixes sector 96 of boomedit.wad from the top
          botz1 = fhsec->floor.GetPointZ(*seg->v1);
          botz2 = fhsec->floor.GetPointZ(*seg->v2);
        }
        else if (fhsec->floor.dist > bhsec->floor.dist) {
          // no, i don't know either, but this fixes sector 96 of boomedit.wad from the bottom after the teleport
          back_botz1 = seg->backsector->floor.GetPointZ(*seg->v1);
          back_botz2 = seg->backsector->floor.GetPointZ(*seg->v2);
        }
      }
    }

    // hack to allow height changes in outdoor areas
    if (R_IsStrictlySkyFlatPlane(r_ceiling.splane) && R_IsStrictlySkyFlatPlane(back_ceiling)) {
      topz1 = back_ceiling->GetPointZ(*seg->v1);
      topz2 = back_ceiling->GetPointZ(*seg->v2);
      top_TexZ = back_ceiling->TexZ;
    }

    if ((linedef->flags&ML_DONTPEGBOTTOM)^peghack) {
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
    wv[1].z = min2(back_botz1, topz1);
    wv[2].z = min2(back_botz2, topz2);
    wv[3].z = botz2;

    /* k8: boomedit.wad -- debug crap */
    /*
    if (seg->frontsector->heightsec && r_hack_fake_floor_decorations) {
      const sector_t *fhsec = seg->frontsector->heightsec;
      const sector_t *bhsec = (seg->backsector ? seg->backsector->heightsec : nullptr);
      int lidx = (int)(ptrdiff_t)(linedef-&Level->Lines[0]);
      if (lidx == 589) {
        GCon->Logf(NAME_Debug, "seg #%d: bsec=%d; fsec=%d; bhsec=%d; fhsec=%d", (int)(ptrdiff_t)(seg-&Level->Segs[0]), (int)(ptrdiff_t)(seg->backsector-&Level->Sectors[0]), (int)(ptrdiff_t)(seg->frontsector-&Level->Sectors[0]), (bhsec ? (int)(ptrdiff_t)(bhsec-&Level->Sectors[0]) : -1), (int)(ptrdiff_t)(fhsec-&Level->Sectors[0]));
        GCon->Logf(NAME_Debug, "linedef #%d: botz=(%g : %g); topz=(%g : %g); back_botz=(%g : %g); fhsecbotz=(%g : %g); fhsectopz=(%g : %g); bhsecbotz=(%g : %g); bhsectopz=(%g : %g)",
          lidx,
          botz1, botz2, topz1, topz2,
          back_botz1, back_botz2,
          fhsec->floor.GetPointZ(*seg->v1), fhsec->floor.GetPointZ(*seg->v2),
          fhsec->ceiling.GetPointZ(*seg->v1), fhsec->ceiling.GetPointZ(*seg->v2),
          (bhsec ? bhsec->floor.GetPointZ(*seg->v1) : -666.999f), (bhsec ? bhsec->floor.GetPointZ(*seg->v2) : -666.999f),
          (bhsec ? bhsec->ceiling.GetPointZ(*seg->v1) : -666.999f), (bhsec ? bhsec->ceiling.GetPointZ(*seg->v2) : -666.999f)
          );
      }
    }
    */

    bool createSurf = true;

    //FIXME: this is totally wrong with slopes!
    if (seg->backsector->heightsec && r_hack_fake_floor_decorations) {
      // do not create outer bottom texture surface if our fake floor is lower than the surrounding floor
      // otherwise, make sure that it is not higher than the fake floor (simc)
      const sector_t *bsec = seg->backsector;
      const sector_t *fsec = seg->frontsector;
      const sector_t *hsec = bsec->heightsec;
      if (hsec->floor.minz <= fsec->floor.minz) {
        //GCon->Logf(NAME_Debug, "BSH: %d (%d) -- SKIP", (int)(ptrdiff_t)(bsec-&Level->Sectors[0]), (int)(ptrdiff_t)(hsec-&Level->Sectors[0]));
        createSurf = false;
      } else if (hsec->floor.minz <= bsec->floor.minz) {
        //GCon->Logf(NAME_Debug, "BSH: %d (%d) -- FIX", (int)(ptrdiff_t)(bsec-&Level->Sectors[0]), (int)(ptrdiff_t)(hsec-&Level->Sectors[0]));
        wv[1].z = min2(wv[1].z, hsec->floor.GetPointZ(*seg->v1));
        wv[2].z = min2(wv[2].z, hsec->floor.GetPointZ(*seg->v2));
        //createSurf = false;
      }
    }

    if (createSurf) {
      CreateWorldSurfFromWV(sub, seg, sp, wv, surface_t::TF_BOTTOM|hackflag);
    }
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backBotDist = back_floor->dist;
  sp->backTopDist = back_ceiling->dist;
  sp->TextureOffset = sidedef->Bot.TextureOffset;
  sp->RowOffset = sidedef->Bot.RowOffset;
  SetupFakeDistances(seg, sp);
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

  SetupTextureAxesOffset(seg, &sp->texinfo, MTex, &sidedef->Mid);

  if (linedef->flags&ML_DONTPEGBOTTOM) {
    // bottom of texture at bottom
    sp->texinfo.toffs = r_floor.splane->TexZ+(MTex->GetScaledHeight()*sidedef->Mid.ScaleY);
  } else if (linedef->flags&ML_DONTPEGTOP) {
    // top of texture at top of top region
    sp->texinfo.toffs = seg->frontsub->sector->ceiling.TexZ;
  } else {
    // top of texture at top
    sp->texinfo.toffs = r_ceiling.splane->TexZ;
  }
  sp->texinfo.toffs *= TextureTScale(MTex)*sidedef->Mid.ScaleY;
  sp->texinfo.toffs += sidedef->Mid.RowOffset*TextureOffsetTScale(MTex);

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

    CreateWorldSurfFromWV(sub, seg, sp, wv, surface_t::TF_MIDDLE);
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = 0.0f;
  sp->backBotDist = 0.0f;
  sp->TextureOffset = sidedef->Mid.TextureOffset;
  sp->RowOffset = sidedef->Mid.RowOffset;
  SetupFakeDistances(seg, sp);
}


//==========================================================================
//
//  DumpOpening
//
//==========================================================================
static VVA_OKUNUSED void DumpOpening (const opening_t *op) {
  GCon->Logf("  %p: floor=%g (%g,%g,%g:%g); ceil=%g (%g,%g,%g:%g); lowfloor=%g; range=%g",
    op,
    op->bottom, op->efloor.GetNormal().x, op->efloor.GetNormal().y, op->efloor.GetNormal().z, op->efloor.GetDist(),
    op->top, op->eceiling.GetNormal().x, op->eceiling.GetNormal().y, op->eceiling.GetNormal().z, op->eceiling.GetDist(),
    op->lowfloor, op->range);
}


//==========================================================================
//
//  FixMidTextureOffsetAndOrigin
//
//==========================================================================
static inline void FixMidTextureOffsetAndOrigin (float &z_org, const line_t *linedef, const side_t *sidedef, texinfo_t *texinfo, VTexture *MTex, const side_tex_params_t *tparam, bool forceWrapped=false) {
  if (forceWrapped || ((linedef->flags&ML_WRAP_MIDTEX)|(sidedef->Flags&SDF_WRAPMIDTEX))) {
    // it is wrapped, so just slide it
    texinfo->toffs = tparam->RowOffset*TextureOffsetTScale(MTex);
  } else {
    // move origin up/down, as this texture is not wrapped
    z_org += tparam->RowOffset*DivByScale(TextureOffsetTScale(MTex), tparam->ScaleY);
    // offset is done by origin, so we don't need to offset texture
    texinfo->toffs = 0.0f;
  }
  texinfo->toffs += z_org*(TextureTScale(MTex)*tparam->ScaleY);
}


//==========================================================================
//
//  FixMidTextureOffsetAndOriginEx
//
//==========================================================================
static inline void FixMidTextureOffsetAndOriginEx (float &z_org, const line_t *linedef, const side_t *sidedef, texinfo_t *texinfo, VTexture *MTex, const side_tex_params_t *tparam, const side_tex_params_t *tparam2) {
  #ifdef VV_SURFCTOR_3D_USE_SEGSIDEDEF_SCALE
  const float scale2Y = tparam2->ScaleY;
  #else
  // it seems that `segsidedef` offset is in effect, but scaling is not
  const float scale2Y = 1.0f;
  #endif
  // it is always wrapped, so just slide it
  texinfo->toffs = (tparam->RowOffset+tparam2->RowOffset)*TextureOffsetTScale(MTex);
  texinfo->toffs += z_org*(TextureTScale(MTex)*tparam->ScaleY*scale2Y);
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

  SetupTextureAxesOffset(seg, &sp->texinfo, MTex, &sidedef->Mid);

  const sec_plane_t *back_floor = &seg->backsector->floor;
  const sec_plane_t *back_ceiling = &seg->backsector->ceiling;

  if (MTex->Type != TEXTYPE_Null) {
    TVec wv[4];

    const sec_plane_t *bfloor = back_floor;
    const sec_plane_t *bceiling = back_ceiling;

    if (seg->backsector->heightsec && r_hack_fake_floor_decorations) {
      const sector_t *bsec = seg->backsector;
      const sector_t *fsec = seg->frontsector;
      const sector_t *hsec = bsec->heightsec;
      if (hsec->floor.minz > fsec->floor.minz) bfloor = &hsec->floor;
      if (hsec->ceiling.minz < fsec->ceiling.minz) bceiling = &hsec->ceiling;
    }

    const float back_topz1 = bceiling->GetPointZ(*seg->v1);
    const float back_topz2 = bceiling->GetPointZ(*seg->v2);
    const float back_botz1 = bfloor->GetPointZ(*seg->v1);
    const float back_botz2 = bfloor->GetPointZ(*seg->v2);

    const float exbotz = min2(back_botz1, back_botz2);
    const float extopz = max2(back_topz1, back_topz2);

    const float texh = DivByScale(MTex->GetScaledHeight(), sidedef->Mid.ScaleY);
    float z_org; // texture top
    if (linedef->flags&ML_DONTPEGBOTTOM) {
      // bottom of texture at bottom
      z_org = max2(seg->frontsector->floor.TexZ, seg->backsector->floor.TexZ)+texh;
    } else {
      // top of texture at top
      z_org = min2(seg->frontsector->ceiling.TexZ, seg->backsector->ceiling.TexZ);
    }
    FixMidTextureOffsetAndOrigin(z_org, linedef, sidedef, &sp->texinfo, MTex, &sidedef->Mid);

    sp->texinfo.Alpha = linedef->alpha;
    sp->texinfo.Additive = !!(linedef->flags&ML_ADDITIVE);

    //bool doDump = ((ptrdiff_t)(linedef-Level->Lines) == 7956);
    enum { doDump = 0 };
    if (doDump) { GCon->Logf("=== MIDSURF FOR LINE #%d (fs=%d; bs=%d; side=%d) ===", (int)(ptrdiff_t)(linedef-Level->Lines), (int)(ptrdiff_t)(seg->frontsector-Level->Sectors), (int)(ptrdiff_t)(seg->backsector-Level->Sectors), (int)(ptrdiff_t)(sidedef-Level->Sides)); }
    //if (linedef->alpha < 1.0f) GCon->Logf("=== MIDSURF FOR LINE #%d (fs=%d; bs=%d) ===", (int)(ptrdiff_t)(linedef-Level->Lines), (int)(ptrdiff_t)(seg->frontsector-Level->Sectors), (int)(ptrdiff_t)(seg->backsector-Level->Sectors));
    if (doDump) { GCon->Logf("   LINEWRAP=%u; SIDEWRAP=%u; ADDITIVE=%u; Alpha=%g; botpeg=%u; z_org=%g; texh=%g", (linedef->flags&ML_WRAP_MIDTEX), (sidedef->Flags&SDF_WRAPMIDTEX), (linedef->flags&ML_ADDITIVE), linedef->alpha, linedef->flags&ML_DONTPEGBOTTOM, z_org, texh); }
    if (doDump) { GCon->Logf("   tx is '%s'; size=(%d,%d); scale=(%g,%g)", *MTex->Name, MTex->GetWidth(), MTex->GetHeight(), MTex->SScale, MTex->TScale); }

    //k8: HACK! HACK! HACK!
    //    move middle wall backwards a little, so it will be hidden behind up/down surfaces
    //    this is required for sectors with 3d floors, until i wrote a proper texture clipping math
    const bool doOffset = seg->backsector->Has3DFloors();

    // another hack (Doom II MAP31)
    // if we have no 3d floors here, and the front sector can be covered with midtex, cover it
    bool bottomCheck = false;
    if (!doOffset && !seg->frontsector->Has3DFloors() && sidedef->BottomTexture < 1 &&
        seg->frontsector->floor.normal.z == 1.0f && seg->backsector->floor.normal.z == 1.0f &&
        seg->frontsector->floor.minz < seg->backsector->floor.minz)
    {
      //GCon->Logf(NAME_Debug, "BOO!");
      bottomCheck = true;
    }

    for (opening_t *cop = SV_SectorOpenings(seg->frontsector, true); cop; cop = cop->next) {
      if (extopz <= cop->bottom || exbotz >= cop->top) {
        if (doDump) { GCon->Log(" SKIP opening"); DumpOpening(cop); }
        //continue;
      }
      if (doDump) { GCon->Logf(" ACCEPT opening"); DumpOpening(cop); }
      // ok, we are at least partially in this opening

      wv[0].x = wv[1].x = seg->v1->x;
      wv[0].y = wv[1].y = seg->v1->y;
      wv[2].x = wv[3].x = seg->v2->x;
      wv[2].y = wv[3].y = seg->v2->y;

      const float topz1 = min2(back_topz1, cop->eceiling.GetPointZ(*seg->v1));
      const float topz2 = min2(back_topz2, cop->eceiling.GetPointZ(*seg->v2));
      const float botz1 = max2(back_botz1, cop->efloor.GetPointZ(*seg->v1));
      const float botz2 = max2(back_botz2, cop->efloor.GetPointZ(*seg->v2));

      float midtopz1 = topz1;
      float midtopz2 = topz2;
      float midbotz1 = botz1;
      float midbotz2 = botz2;

      if (doDump) { GCon->Logf(" zorg=(%g,%g); botz=(%g,%g); topz=(%g,%g)", z_org-texh, z_org, midbotz1, midbotz2, midtopz1, midtopz2); }

      if (sidedef->TopTexture > 0) {
        midtopz1 = min2(midtopz1, cop->eceiling.GetPointZ(*seg->v1));
        midtopz2 = min2(midtopz2, cop->eceiling.GetPointZ(*seg->v2));
      }

      if (sidedef->BottomTexture > 0) {
        midbotz1 = max2(midbotz1, cop->efloor.GetPointZ(*seg->v1));
        midbotz2 = max2(midbotz2, cop->efloor.GetPointZ(*seg->v2));
      }

      if (midbotz1 >= midtopz1 || midbotz2 >= midtopz2) continue;

      if (doDump) { GCon->Logf(" zorg=(%g,%g); botz=(%g,%g); topz=(%g,%g); backbotz=(%g,%g); backtopz=(%g,%g)", z_org-texh, z_org, midbotz1, midbotz2, midtopz1, midtopz2, back_botz1, back_botz2, back_topz1, back_topz2); }

      float hgts[4];

      // linedef->flags&ML_CLIP_MIDTEX, sidedef->Flags&SDF_CLIPMIDTEX
      // this clips texture to a floor, otherwise it goes beyound it
      // it seems that all modern OpenGL renderers just ignores clip flag, and
      // renders all midtextures as always clipped.
      if ((linedef->flags&ML_WRAP_MIDTEX)|(sidedef->Flags&SDF_WRAPMIDTEX)) {
        hgts[0] = midbotz1;
        hgts[1] = midtopz1;
        hgts[2] = midtopz2;
        hgts[3] = midbotz2;
      } else {
        if (z_org <= max2(midbotz1, midbotz2)) continue;
        if (z_org-texh >= max2(midtopz1, midtopz2)) continue;
        if (doDump) {
          GCon->Log(" === front regions ===");
          VLevel::dumpSectorRegions(seg->frontsector);
          GCon->Log(" === front openings ===");
          for (opening_t *bop = SV_SectorOpenings2(seg->frontsector, true); bop; bop = bop->next) DumpOpening(bop);
          GCon->Log(" === back regions ===");
          VLevel::dumpSectorRegions(seg->backsector);
          GCon->Log(" === back openings ===");
          for (opening_t *bop = SV_SectorOpenings2(seg->backsector, true); bop; bop = bop->next) DumpOpening(bop);
        }
        hgts[0] = max2(midbotz1, z_org-texh);
        hgts[1] = min2(midtopz1, z_org);
        hgts[2] = min2(midtopz2, z_org);
        hgts[3] = max2(midbotz2, z_org-texh);
        // cover bottom texture with this too (because why not?)
        if (bottomCheck && hgts[0] > z_org-texh) {
          //GCon->Logf(NAME_Debug, "BOO! hgts=%g, %g, %g, %g (fz=%g); texh=%g", hgts[0], hgts[1], hgts[2], hgts[3], seg->frontsector->floor.minz, texh);
          hgts[0] = hgts[3] = min2(hgts[0], max2(seg->frontsector->floor.minz, z_org-texh));
        }
      }

      wv[0].z = hgts[0];
      wv[1].z = hgts[1];
      wv[2].z = hgts[2];
      wv[3].z = hgts[3];

      if (doDump) {
        GCon->Logf("  z:(%g,%g,%g,%g)", hgts[0], hgts[1], hgts[2], hgts[3]);
        for (int wc = 0; wc < 4; ++wc) GCon->Logf("  wc #%d: (%g,%g,%g)", wc, wv[wc].x, wv[wc].y, wv[wc].z);
      }

      CreateWorldSurfFromWV(sub, seg, sp, wv, surface_t::TF_MIDDLE, doOffset);
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
  SetupFakeDistances(seg, sp);
}


// ////////////////////////////////////////////////////////////////////////// //
struct WallFace {
  float topz[2];
  float botz[2];
  const TVec *v[2];
  WallFace *next;

  inline WallFace () noexcept {
    topz[0] = topz[1] = 0;
    botz[0] = botz[1] = 0;
    v[0] = v[1] = nullptr;
    next = nullptr;
  }

  inline void setup (const sec_region_t *reg, const TVec *v1, const TVec *v2) noexcept {
    v[0] = v1;
    v[1] = v2;

    topz[0] = reg->eceiling.GetPointZ(*v1);
    topz[1] = reg->eceiling.GetPointZ(*v2);

    botz[0] = reg->efloor.GetPointZ(*v1);
    botz[1] = reg->efloor.GetPointZ(*v2);
  }

  inline void markInvalid () noexcept { topz[0] = topz[1] = -666; botz[0] = botz[1] = 666; }
  inline bool isValid () const noexcept { return (topz[0] > botz[0] && topz[1] > botz[1]); }
};


//==========================================================================
//
//  CutWallFace
//
//==========================================================================
static void CutWallFace (WallFace *face, sector_t *sec, sec_region_t *ignorereg, bool cutWithSwimmable, WallFace *&tail) {
  if (!face->isValid()) return;
  if (!sec) return;
  const vuint32 skipmask = sec_region_t::RF_OnlyVisual|(cutWithSwimmable ? 0u : sec_region_t::RF_NonSolid);
  for (sec_region_t *reg = sec->eregions; reg; reg = reg->next) {
    if (reg->regflags&skipmask) continue; // not interesting
    if (reg == ignorereg) continue; // ignore self
    if (reg->regflags&sec_region_t::RF_BaseRegion) {
      // base region, allow what is inside
      for (int f = 0; f < 2; ++f) {
        const float rtopz = reg->eceiling.GetPointZ(*face->v[f]);
        const float rbotz = reg->efloor.GetPointZ(*face->v[f]);
        // if above/below the sector, mark as invalid
        if (face->botz[f] >= rtopz || face->topz[f] <= rbotz) {
          face->markInvalid();
          return;
        }
        // cut with sector bounds
        face->topz[f] = min2(face->topz[f], rtopz);
        face->botz[f] = max2(face->botz[f], rbotz);
        if (face->topz[f] <= face->botz[f]) return; // everything was cut away
      }
    } else {
      //FIXME: for now, we can cut only by non-sloped 3d floors
      if (reg->eceiling.GetPointZ(*face->v[0]) != reg->eceiling.GetPointZ(*face->v[1]) ||
          reg->efloor.GetPointZ(*face->v[0]) != reg->efloor.GetPointZ(*face->v[1]))
      {
        continue;
      }
      //FIXME: ...and only non-sloped 3d floors
      if (ignorereg->eceiling.GetPointZ(*face->v[0]) != ignorereg->eceiling.GetPointZ(*face->v[1]) ||
          ignorereg->efloor.GetPointZ(*face->v[0]) != ignorereg->efloor.GetPointZ(*face->v[1]))
      {
        continue;
      }
      // 3d floor, allow what is outside
      for (int f = 0; f < 2; ++f) {
        const float rtopz = reg->eceiling.GetPointZ(*face->v[f]);
        const float rbotz = reg->efloor.GetPointZ(*face->v[f]);
        if (rtopz <= rbotz) continue; // invalid, or paper-thin, ignore
        // if completely above, or completely below, ignore
        if (rbotz >= face->topz[f] || rtopz <= face->botz[f]) continue;
        // if completely covered, mark as invalid and stop
        if (rbotz <= face->botz[f] && rtopz >= face->topz[f]) {
          face->markInvalid();
          return;
        }
        // if inside the face, split it to two faces
        if (rtopz > face->topz[f] && rbotz < face->botz[f]) {
          // split; top part
          {
            WallFace *ftop = new WallFace;
            *ftop = *face;
            ftop->botz[0] = ftop->botz[1] = rtopz;
            tail->next = ftop;
            tail = ftop;
          }
          // split; bottom part
          {
            WallFace *fbot = new WallFace;
            *fbot = *face;
            fbot->topz[0] = fbot->topz[1] = rbotz;
            tail->next = fbot;
            tail = fbot;
          }
          // mark this one as invalid, and stop
          face->markInvalid();
          return;
        }
        // partially covered; is it above?
        if (rbotz > face->botz[f]) {
          // it is above, cut top
          face->topz[f] = min2(face->topz[f], rbotz);
        } else if (rtopz < face->topz[f]) {
          // it is below, cut bottom
          face->botz[f] = max2(face->botz[f], rtopz);
        }
        if (face->topz[f] <= face->botz[f]) return; // everything was cut away
      }
    }
  }
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
                                                     TSecPlaneRef r_floor, TSecPlaneRef r_ceiling)
{
  FreeWSurfs(sp->surfs);

  //ops = SV_SectorOpenings(seg->frontsector); // skip non-solid

  const line_t *linedef = reg->extraline;
  /*const*/ side_t *sidedef = &Level->Sides[linedef->sidenum[0]];
  /*const*/ side_t *segsidedef = seg->sidedef;
  //const side_t *texsideparm = (segsidedef ? segsidedef : sidedef);

  VTexture *MTex = GTextureManager(sidedef->MidTexture);
  if (!MTex) MTex = GTextureManager[GTextureManager.DefaultTexture];

  // it seems that `segsidedef` offset is in effect, but scaling is not
  #ifdef VV_SURFCTOR_3D_USE_SEGSIDEDEF_SCALE
  const float scale2Y = segsidedef->Mid.ScaleY;
  #else
  // it seems that `segsidedef` offset is in effect, but scaling is not
  const float scale2Y = 1.0f;
  #endif

  /*
  if (sidedef->Mid.ScaleY != 1) GCon->Logf(NAME_Debug, "extra: line #%d (%d), side #%d: midscale=%g", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]), (int)(ptrdiff_t)(sidedef-&Level->Sides[0]), sidedef->Mid.ScaleY);
  if (segsidedef->Mid.ScaleY != 1) GCon->Logf(NAME_Debug, "seg: line #%d (%d), side #%d: midscale=%g", (int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]), (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(segsidedef-&Level->Sides[0]), segsidedef->Mid.ScaleY);
  if ((int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]) == 49) {
    //sidedef->Mid.ScaleY = segsidedef->Mid.ScaleY = 1;
    segsidedef->Mid.ScaleY = 1;
    //segsidedef->Mid.RowOffset = 0;
    //sidedef->Mid.RowOffset = 0;
  }
  */
  /*
   1. solid 3d floors should be cut only by other solids (including other sector)
   2. swimmable (water) 3d floors should be cut by all solids (including other sector), and
      by all swimmable (including other sector)
   */

  //bool doDump = false;
  enum { doDump = 0 };
  //bool doDump = (sidedef->MidTexture.id == 1713);
  //const bool doDump = (seg->linedef-&Level->Lines[0] == 33);
  //const bool doDump = true;

  /*
  if (seg->frontsector-&Level->Sectors[0] == 70) {
    doDump = true;
    GCon->Logf("::: SECTOR #70 (back #%d) EF: texture='%s'", (seg->backsector ? (int)(ptrdiff_t)(seg->backsector-&Level->Sectors[0]) : -1), *MTex->Name);
    GCon->Log(" === front regions ===");
    VLevel::dumpSectorRegions(seg->frontsector);
    GCon->Log(" === front openings ===");
    for (opening_t *bop = SV_SectorOpenings2(seg->frontsector, true); bop; bop = bop->next) DumpOpening(bop);
    GCon->Log(" === real openings ===");
    //for (opening_t *bop = ops; bop; bop = bop->next) DumpOpening(bop);
  }

  if (seg->backsector && seg->backsector-&Level->Sectors[0] == 70) {
    doDump = true;
    GCon->Logf("::: BACK-SECTOR #70 (front #%d) EF: texture='%s'", (seg->frontsector ? (int)(ptrdiff_t)(seg->frontsector-&Level->Sectors[0]) : -1), *MTex->Name);
    GCon->Log(" === front regions ===");
    VLevel::dumpSectorRegions(seg->frontsector);
    GCon->Log(" === front openings ===");
    for (opening_t *bop = SV_SectorOpenings2(seg->frontsector, true); bop; bop = bop->next) DumpOpening(bop);
    GCon->Log(" === real openings ===");
    for (opening_t *bop = SV_SectorOpenings(seg->frontsector); bop; bop = bop->next) DumpOpening(bop);
  }
  */

  if (doDump) GCon->Logf(NAME_Debug, "*** line #%d (extra line #%d); seg side #%d ***", (int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]), (int)(ptrdiff_t)(linedef-&Level->Lines[0]), seg->side);
  if (doDump) {
    GCon->Log(NAME_Debug, "=== REGIONS:FRONT ===");
    VLevel::dumpSectorRegions(seg->frontsector);
    GCon->Log(NAME_Debug, "=== REGIONS:BACK ===");
    VLevel::dumpSectorRegions(seg->backsector);
    GCon->Log(NAME_Debug, "=== OPENINGS:FRONT ===");
    for (opening_t *bop = SV_SectorOpenings(seg->frontsector); bop; bop = bop->next) DumpOpening(bop);
    GCon->Log(NAME_Debug, "=== OPENINGS:BACK ===");
    for (opening_t *bop = SV_SectorOpenings(seg->backsector); bop; bop = bop->next) DumpOpening(bop);
    //ops = SV_SectorOpenings(seg->frontsector); // this should be done to update openings
  }

  // apply offsets from seg side
  SetupTextureAxesOffsetEx(seg, &sp->texinfo, MTex, &sidedef->Mid, &segsidedef->Mid);

  //const float texh = DivByScale(MTex->GetScaledHeight(), texsideparm->Mid.ScaleY);
  const float texh = DivByScale2(MTex->GetScaledHeight(), sidedef->Mid.ScaleY, scale2Y);
  const float texhsc = MTex->GetHeight();
  float z_org; // texture top

  // (reg->regflags&sec_region_t::RF_SaneRegion) // vavoom 3d floor
  if (linedef->flags&ML_DONTPEGBOTTOM) {
    // bottom of texture at bottom
    z_org = reg->efloor.splane->TexZ+texh;
  } else if (linedef->flags&ML_DONTPEGTOP) {
    // top of texture at top of top region (???)
    z_org = seg->frontsub->sector->ceiling.TexZ;
    //z_org = reg->eceiling.splane->TexZ;
  } else {
    // top of texture at top
    z_org = reg->eceiling.splane->TexZ;
  }

  // apply offsets from both sides
  FixMidTextureOffsetAndOriginEx(z_org, linedef, sidedef, &sp->texinfo, MTex, &sidedef->Mid, &segsidedef->Mid);
  //FixMidTextureOffsetAndOrigin(z_org, linedef, sidedef, &sp->texinfo, MTex, &sidedef->Mid);

  sp->texinfo.Alpha = (reg->efloor.splane->Alpha < 1.0f ? reg->efloor.splane->Alpha : 1.1f);
  sp->texinfo.Additive = !!(reg->efloor.splane->flags&SPF_ADDITIVE);

  // hack: 3d floor with sky texture seems to be transparent in renderer
  if (MTex->Type != TEXTYPE_Null && sidedef->MidTexture.id != skyflatnum) {
    TVec wv[4];

    //const bool wrapped = !!((linedef->flags&ML_WRAP_MIDTEX)|(sidedef->Flags&SDF_WRAPMIDTEX));
    // side 3d floor midtex should always be wrapped
    enum { wrapped = 1 };

    WallFace *facehead = nullptr;
    WallFace *facetail = nullptr;
    WallFace *face = new WallFace;
    face->setup(reg, seg->v1, seg->v2);

    const bool isNonSolid = !!(reg->regflags&sec_region_t::RF_NonSolid);
    facehead = facetail = face;
    for (WallFace *cface = facehead; cface; cface = cface->next) {
      if (isNonSolid || r_3dfloor_clip_both_sides) CutWallFace(cface, seg->frontsector, reg, isNonSolid, facetail);
      CutWallFace(cface, seg->backsector, reg, isNonSolid, facetail);
    }

    // now create all surfaces
    for (WallFace *cface = facehead; cface; cface = cface->next) {
      if (!cface->isValid()) continue;

      float topz1 = cface->topz[0];
      float topz2 = cface->topz[1];
      float botz1 = cface->botz[0];
      float botz2 = cface->botz[1];

      // check texture limits
      if (!wrapped) {
        if (max2(topz1, topz2) <= z_org-texhsc) continue;
        if (min2(botz1, botz2) >= z_org) continue;
      }

      wv[0].x = wv[1].x = seg->v1->x;
      wv[0].y = wv[1].y = seg->v1->y;
      wv[2].x = wv[3].x = seg->v2->x;
      wv[2].y = wv[3].y = seg->v2->y;

      if (wrapped) {
        wv[0].z = botz1;
        wv[1].z = topz1;
        wv[2].z = topz2;
        wv[3].z = botz2;
      } else {
        wv[0].z = max2(botz1, z_org-texhsc);
        wv[1].z = min2(topz1, z_org);
        wv[2].z = min2(topz2, z_org);
        wv[3].z = max2(botz2, z_org-texhsc);
      }

      if (doDump) for (int wf = 0; wf < 4; ++wf) GCon->Logf("   wf #%d: (%g,%g,%g)", wf, wv[wf].x, wv[wf].y, wv[wf].z);

      CreateWorldSurfFromWV(sub, seg, sp, wv, surface_t::TF_MIDDLE);
    }

    if (sp->surfs && (sp->texinfo.Alpha < 1.0f || sp->texinfo.Additive || MTex->isTranslucent())) {
      for (surface_t *sf = sp->surfs; sf; sf = sf->next) sf->drawflags |= surface_t::DF_NO_FACE_CULL;
    }
  } else {
    if (sidedef->MidTexture.id != skyflatnum) {
      sp->texinfo.Alpha = 1.1f;
      sp->texinfo.Additive = false;
    } else {
      sp->texinfo.Alpha = 0.0f;
      sp->texinfo.Additive = true;
    }
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = reg->eceiling.splane->dist;
  sp->backBotDist = reg->efloor.splane->dist;
  /*
  sp->TextureOffset = texsideparm->Mid.TextureOffset;
  sp->RowOffset = texsideparm->Mid.RowOffset;
  */
  sp->TextureOffset = sidedef->Mid.TextureOffset+segsidedef->Mid.TextureOffset;
  sp->RowOffset = sidedef->Mid.RowOffset+segsidedef->Mid.RowOffset;
  SetupFakeDistances(seg, sp);
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

  if (!isMainRegion) return;

  if (!seg->backsector) {
    // one sided line
    // middle wall
    dseg->mid = SurfCreatorGetPSPart();
    sp = dseg->mid;
    sp->basereg = curreg;
    SetupOneSidedMidWSurf(sub, seg, sp, r_floor, r_ceiling);

    // sky above line
    dseg->topsky = SurfCreatorGetPSPart();
    sp = dseg->topsky;
    sp->basereg = curreg;
    if (R_IsStrictlySkyFlatPlane(r_ceiling.splane)) SetupOneSidedSkyWSurf(sub, seg, sp, r_floor, r_ceiling);
  } else {
    // two sided line
    // top wall
    dseg->top = SurfCreatorGetPSPart();
    sp = dseg->top;
    sp->basereg = curreg;
    SetupTwoSidedTopWSurf(sub, seg, sp, r_floor, r_ceiling);

    // sky above top
    dseg->topsky = SurfCreatorGetPSPart();
    dseg->topsky->basereg = curreg;
    if (R_IsStrictlySkyFlatPlane(r_ceiling.splane) && !R_IsStrictlySkyFlatPlane(&seg->backsector->ceiling)) {
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
    for (sec_region_t *reg = seg->backsector->eregions->next; reg; reg = reg->next) {
      if (!reg->extraline) continue; // no need to create extra side

      // hack: 3d floor with sky texture seems to be transparent in renderer
      const side_t *sidedef = &Level->Sides[reg->extraline->sidenum[0]];
      if (sidedef->MidTexture == skyflatnum) continue;

      sp = SurfCreatorGetPSPart();
      sp->basereg = reg;
      sp->next = dseg->extra;
      dseg->extra = sp;

      SetupTwoSidedMidExtraWSurf(reg, sub, seg, sp, r_floor, r_ceiling);
    }
  }
}


//==========================================================================
//
//  InvalidateSegPart
//
//==========================================================================
static inline void InvalidateSegPart (segpart_t *sp) {
  for (; sp; sp = sp->next) sp->fixTJunction = 1u;
}


//==========================================================================
//
//  MarkTJunctions
//
//==========================================================================
static inline void MarkTJunctions (VLevel *Level, seg_t *seg) {
  const line_t *line = seg->linedef;
  const sector_t *mysec = seg->frontsector;
  if (!line || !mysec) return; // just in case
  //GCon->Logf(NAME_Debug, "mark tjunctions for line #%d", (int)(ptrdiff_t)(line-&Level->Lines[0]));
  // simply mark all adjacents for recreation
  for (int lvidx = 0; lvidx < 2; ++lvidx) {
    for (int f = 0; f < line->vxCount(lvidx); ++f) {
      const line_t *ln = line->vxLine(lvidx, f);
      if (ln != line) {
        //GCon->Logf(NAME_Debug, "  ...marking line #%d", (int)(ptrdiff_t)(ln-&Level->Lines[0]));
        // for each seg
        for (seg_t *ns = ln->firstseg; ns; ns = ns->lsnext) {
          // for each drawseg
          for (drawseg_t *ds = ns->drawsegs; ds; ds = ds->next) {
            // for each segpart
            InvalidateSegPart(ds->top);
            InvalidateSegPart(ds->mid);
            InvalidateSegPart(ds->bot);
            InvalidateSegPart(ds->topsky);
            InvalidateSegPart(ds->extra);
          }
        }
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
    (sp->fixTJunction) ||
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
    // check for fake floors
    return
      CheckFakeDistances(seg, sp) ||
      CheckCommonRecreateEx(sp, NTex, floor, ceiling, &seg->backsector->floor, &seg->backsector->ceiling);
  } else {
    return CheckCommonRecreateEx(sp, NTex, floor, ceiling, nullptr, nullptr);
  }
}


//==========================================================================
//
//  CheckMidRecreate1S
//
//==========================================================================
static inline bool CheckMidRecreate1S (seg_t *seg, segpart_t *sp, const TPlane *floor, const TPlane *ceiling) {
  return CheckCommonRecreate(seg, sp, GTextureManager(seg->sidedef->MidTexture), floor, ceiling);
}


//==========================================================================
//
//  CheckMidRecreate2S
//
//==========================================================================
static inline bool CheckMidRecreate2S (seg_t *seg, segpart_t *sp, const TPlane *floor, const TPlane *ceiling) {
  return CheckCommonRecreate(seg, sp, GTextureManager(seg->sidedef->MidTexture), floor, ceiling);
}


//==========================================================================
//
//  CheckTopRecreate2S
//
//==========================================================================
static inline bool CheckTopRecreate2S (seg_t *seg, segpart_t *sp, sec_plane_t *floor, sec_plane_t *ceiling) {
  sec_plane_t *back_ceiling = &seg->backsector->ceiling;
  VTexture *TTex = GTextureManager(seg->sidedef->TopTexture);
  if (ceiling->SkyBox != back_ceiling->SkyBox && R_IsStrictlySkyFlatPlane(ceiling) && R_IsStrictlySkyFlatPlane(back_ceiling)) {
    TTex = GTextureManager[skyflatnum];
  }
  return CheckCommonRecreate(seg, sp, TTex, floor, ceiling);
}


//==========================================================================
//
//  CheckBopRecreate2S
//
//==========================================================================
static inline bool CheckBopRecreate2S (seg_t *seg, segpart_t *sp, const TPlane *floor, const TPlane *ceiling) {
  return CheckCommonRecreate(seg, sp, GTextureManager(seg->sidedef->BottomTexture), floor, ceiling);
}


//==========================================================================
//
//  VRenderLevelShared::UpdateTextureOffsets
//
//==========================================================================
void VRenderLevelShared::UpdateTextureOffsets (subsector_t *sub, seg_t *seg, segpart_t *sp, const side_tex_params_t *tparam, const TPlane *plane) {
  bool reinitSurfs = false;

  if (FASI(sp->TextureOffset) != FASI(tparam->TextureOffset)) {
    reinitSurfs = true;
    sp->texinfo.soffs += (tparam->TextureOffset-sp->TextureOffset)*TextureOffsetSScale(sp->texinfo.Tex);
    sp->TextureOffset = tparam->TextureOffset;
  }

  if (FASI(sp->RowOffset) != FASI(tparam->RowOffset)) {
    reinitSurfs = true;
    sp->texinfo.toffs += (tparam->RowOffset-sp->RowOffset)*TextureOffsetTScale(sp->texinfo.Tex);
    sp->RowOffset = tparam->RowOffset;
  }

  if (reinitSurfs) {
    // do not recalculate static lightmaps
    InitSurfs(false, sp->surfs, &sp->texinfo, (plane ? plane : seg), sub);
  }
}


//==========================================================================
//
//  VRenderLevelShared::UpdateTextureOffsetsEx
//
//==========================================================================
void VRenderLevelShared::UpdateTextureOffsetsEx (subsector_t *sub, seg_t *seg, segpart_t *sp, const side_tex_params_t *tparam, const side_tex_params_t *tparam2) {
  bool reinitSurfs = false;

  const float ctofs = tparam->TextureOffset+tparam2->TextureOffset;
  if (FASI(sp->TextureOffset) != FASI(ctofs)) {
    reinitSurfs = true;
    sp->texinfo.soffs += (ctofs-sp->TextureOffset)*TextureOffsetSScale(sp->texinfo.Tex);
    sp->TextureOffset = ctofs;
  }

  const float rwofs = tparam->RowOffset+tparam2->RowOffset;
  if (FASI(sp->RowOffset) != FASI(rwofs)) {
    reinitSurfs = true;
    sp->texinfo.toffs += (rwofs-sp->RowOffset)*TextureOffsetTScale(sp->texinfo.Tex);
    sp->RowOffset = rwofs;
  }

  if (reinitSurfs) {
    // do not recalculate static lightmaps
    InitSurfs(false, sp->surfs, &sp->texinfo, seg, sub);
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
  bool needTJ = false;

  if (!seg->backsector) {
    // one-sided seg
    // top sky
    segpart_t *sp = dseg->topsky;
    if (sp) {
      if (FASI(sp->frontTopDist) != FASI(r_ceiling.splane->dist) && R_IsStrictlySkyFlatPlane(r_ceiling.splane)) {
        SetupOneSidedSkyWSurf(sub, seg, sp, r_floor, r_ceiling);
      }
      sp->texinfo.ColorMap = ColorMap;
    }

    // midtexture
    sp = dseg->mid;
    if (sp) {
      //if (seg->pobj) GCon->Logf(NAME_Debug, "pobj #%d seg; UPDATING", seg->pobj->index);
      if (CheckMidRecreate1S(seg, sp, r_floor.splane, r_ceiling.splane)) {
        if (!sp->fixTJunction) needTJ = true; else sp->fixTJunction = 0;
        SetupOneSidedMidWSurf(sub, seg, sp, r_floor, r_ceiling);
      } else {
        UpdateTextureOffsets(sub, seg, sp, &seg->sidedef->Mid);
      }
      sp->texinfo.ColorMap = ColorMap;
    }
  } else {
    // two-sided seg
    sec_plane_t *back_ceiling = &seg->backsector->ceiling;

    // sky above top
    segpart_t *sp = dseg->topsky;
    if (sp) {
      if (FASI(sp->frontTopDist) != FASI(r_ceiling.splane->dist) &&
          R_IsStrictlySkyFlatPlane(r_ceiling.splane) && !R_IsStrictlySkyFlatPlane(back_ceiling))
      {
        if (!sp->fixTJunction) needTJ = true; else sp->fixTJunction = 0;
        SetupTwoSidedSkyWSurf(sub, seg, sp, r_floor, r_ceiling);
      }
      sp->texinfo.ColorMap = ColorMap;
    }

    //TODO: properly implement 2s transparent door hack (TNT MAP02)

    // top wall
    sp = dseg->top;
    if (sp) {
      if (CheckTopRecreate2S(seg, sp, r_floor.splane, r_ceiling.splane)) {
        if (!sp->fixTJunction) needTJ = true; else sp->fixTJunction = 0;
        SetupTwoSidedTopWSurf(sub, seg, sp, r_floor, r_ceiling);
      } else {
        UpdateTextureOffsets(sub, seg, sp, &seg->sidedef->Top);
      }
      sp->texinfo.ColorMap = ColorMap;
    }

    // bottom wall
    sp = dseg->bot;
    if (sp) {
      if (CheckBopRecreate2S(seg, sp, r_floor.splane, r_ceiling.splane)) {
        if (!sp->fixTJunction) needTJ = true; else sp->fixTJunction = 0;
        SetupTwoSidedBotWSurf(sub, seg, sp, r_floor, r_ceiling);
      } else {
        UpdateTextureOffsets(sub, seg, sp, &seg->sidedef->Bot);
      }
      sp->texinfo.ColorMap = ColorMap;
    }

    // masked MidTexture
    sp = dseg->mid;
    if (sp) {
      if (CheckMidRecreate2S(seg, sp, r_floor.splane, r_ceiling.splane)) {
        if (!sp->fixTJunction) needTJ = true; else sp->fixTJunction = 0;
        SetupTwoSidedMidWSurf(sub, seg, sp, r_floor, r_ceiling);
      } else {
        UpdateTextureOffsets(sub, seg, sp, &seg->sidedef->Mid);
      }
      sp->texinfo.ColorMap = ColorMap;
      if (sp->texinfo.Tex->Type != TEXTYPE_Null) {
        sp->texinfo.Alpha = seg->linedef->alpha;
        sp->texinfo.Additive = !!(seg->linedef->flags&ML_ADDITIVE);
      } else {
        sp->texinfo.Alpha = 1.1f;
        sp->texinfo.Additive = false;
      }
    }

    // update 3d floors
    for (sp = dseg->extra; sp; sp = sp->next) {
      sec_region_t *reg = sp->basereg;
      vassert(reg->extraline);
      const side_t *extraside = &Level->Sides[reg->extraline->sidenum[0]];

      // hack: 3d floor with sky texture seems to be transparent in renderer
      // it should not end here, though, so skip the check
      //if (extraside->MidTexture == skyflatnum) continue;

      VTexture *MTex = GTextureManager(extraside->MidTexture);
      if (!MTex) MTex = GTextureManager[GTextureManager.DefaultTexture];

      if (CheckCommonRecreateEx(sp, MTex, r_floor.splane, r_ceiling.splane, reg->efloor.splane, reg->eceiling.splane)) {
        if (!sp->fixTJunction) needTJ = true; else sp->fixTJunction = 0;
        SetupTwoSidedMidExtraWSurf(reg, sub, seg, sp, r_floor, r_ceiling);
      } else {
        UpdateTextureOffsetsEx(sub, seg, sp, &extraside->Mid, &seg->sidedef->Mid);
      }
      sp->texinfo.ColorMap = ColorMap;
    }
  }

  if (needTJ && r_fix_tjunctions.asBool()) MarkTJunctions(Level, seg);
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
  seg->drawsegs->mid->texinfo.saxisLM = seg->dir;
  seg->drawsegs->mid->texinfo.saxis = seg->dir*(TextureSScale(MTex)*sidedef->Mid.ScaleX);
  seg->drawsegs->mid->texinfo.soffs = -DotProduct(*seg->v1, seg->drawsegs->mid->texinfo.saxis)+
                                      seg->offset*(TextureSScale(MTex)*sidedef->Mid.ScaleX)+
                                      sidedef->Mid.TextureOffset*TextureOffsetSScale(MTex);

  // force update
  //if (seg->pobj) GCon->Logf(NAME_Debug, "pobj #%d seg; backsector=%p", seg->pobj->index, seg->backsector);
  seg->drawsegs->mid->frontTopDist += 0.346f;
}


//==========================================================================
//
//  VRenderLevelShared::CreateWorldSurfaces
//
//==========================================================================
void VRenderLevelShared::CreateWorldSurfaces () {
  const bool oldIWC = inWorldCreation;
  inWorldCreation = true;

  vassert(!free_wsurfs);
  SetupSky();

  // set up fake floors
  for (auto &&sec : Level->allSectors()) {
    if (sec.heightsec || sec.deepref) {
      SetupFakeFloors(&sec);
    }
  }

  if (inWorldCreation) {
    R_OSDMsgShowSecondary("CREATING WORLD SURFACES");
    R_PBarReset();
  }

  // count regions in all subsectors
  int count = 0;
  int dscount = 0;
  int spcount = 0;
  for (auto &&sub : Level->allSubsectors()) {
    if (!sub.sector->linecount) continue; // skip sectors containing original polyobjs
    count += 4*2; //k8: dunno
    for (sec_region_t *reg = sub.sector->eregions; reg; reg = reg->next) {
      ++count;
      dscount += sub.numlines;
      if (sub.HasPObjs()) {
        for (auto &&it : sub.PObjFirst()) dscount += it.value()->numsegs; // polyobj
      }
      for (int j = 0; j < sub.numlines; ++j) spcount += CountSegParts(&Level->Segs[sub.firstline+j]);
      if (sub.HasPObjs()) {
        for (auto &&it : sub.PObjFirst()) {
          polyobj_t *pobj = it.value();
          seg_t *const *polySeg = pobj->segs;
          for (int polyCount = pobj->numsegs; polyCount--; ++polySeg) spcount += CountSegParts(*polySeg);
        }
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
  for (auto &&it : Level->allSubsectorsIdx()) {
    subsector_t *sub = it.value();
    if (!sub->sector->linecount) continue; // skip sectors containing original polyobjs

    TSecPlaneRef main_floor = sub->sector->eregions->efloor;
    TSecPlaneRef main_ceiling = sub->sector->eregions->eceiling;

    subregion_t *lastsreg = sub->regions;

    int ridx = 0;
    for (sec_region_t *reg = sub->sector->eregions; reg; reg = reg->next, ++ridx) {
      if (sregLeft == 0) Sys_Error("out of subregions in surface creator");
      if (ridx == 0 && !(reg->regflags&sec_region_t::RF_BaseRegion)) Sys_Error("internal bug in region creation (base region is not marked as base)");

      TSecPlaneRef r_floor, r_ceiling;
      r_floor = reg->efloor;
      r_ceiling = reg->eceiling;

      bool skipFloor = !!(reg->regflags&sec_region_t::RF_SkipFloorSurf);
      bool skipCeil = !!(reg->regflags&sec_region_t::RF_SkipCeilSurf);

      if (ridx != 0 && reg->extraline) {
        // hack: 3d floor with sky texture seems to be transparent in renderer
        const side_t *extraside = &Level->Sides[reg->extraline->sidenum[0]];
        if (extraside->MidTexture == skyflatnum) {
          skipFloor = skipCeil = true;
        }
      }

      sreg->secregion = reg;
      sreg->floorplane = r_floor;
      sreg->ceilplane = r_ceiling;
      sreg->realfloor = (skipFloor ? nullptr : CreateSecSurface(nullptr, sub, r_floor, sreg));
      sreg->realceil = (skipCeil ? nullptr : CreateSecSurface(nullptr, sub, r_ceiling, sreg));

      // create fake floor and ceiling
      if (ridx == 0 && sub->sector->fakefloors) {
        TSecPlaneRef fakefloor, fakeceil;
        fakefloor.set(&sub->sector->fakefloors->floorplane, false);
        fakeceil.set(&sub->sector->fakefloors->ceilplane, false);
        if (!fakefloor.isFloor()) fakefloor.Flip();
        if (!fakeceil.isCeiling()) fakeceil.Flip();
        sreg->fakefloor = (skipFloor ? nullptr : CreateSecSurface(nullptr, sub, fakefloor, sreg, true));
        sreg->fakeceil = (skipCeil ? nullptr : CreateSecSurface(nullptr, sub, fakeceil, sreg, true));
      } else {
        sreg->fakefloor = nullptr;
        sreg->fakeceil = nullptr;
      }

      sreg->count = sub->numlines;
      if (ridx == 0 && sub->HasPObjs()) {
        for (auto &&poit : sub->PObjFirst()) sreg->count += poit.value()->numsegs; // polyobj
      }
      if (pdsLeft < sreg->count) Sys_Error("out of drawsegs in surface creator");
      sreg->lines = pds;
      pds += sreg->count;
      pdsLeft -= sreg->count;
      for (int j = 0; j < sub->numlines; ++j) CreateSegParts(sub, &sreg->lines[j], &Level->Segs[sub->firstline+j], main_floor, main_ceiling, reg, (ridx == 0));

      if (ridx == 0 && sub->HasPObjs()) {
        // polyobj
        int j = sub->numlines;
        for (auto &&poit : sub->PObjFirst()) {
          polyobj_t *pobj = poit.value();
          seg_t **polySeg = pobj->segs;
          for (int polyCount = pobj->numsegs; polyCount--; ++polySeg, ++j) {
            CreateSegParts(sub, &sreg->lines[j], *polySeg, main_floor, main_ceiling, nullptr, true);
          }
        }
      }

      // proper append
      sreg->next = nullptr;
      if (lastsreg) lastsreg->next = sreg; else sub->regions = sreg;
      lastsreg = sreg;

      ++sreg;
      --sregLeft;
    }

    if (inWorldCreation) R_PBarUpdate("Surfaces", it.index(), Level->NumSubsectors);
  }

  InitialWorldUpdate();

  if (inWorldCreation) R_PBarUpdate("Surfaces", Level->NumSubsectors, Level->NumSubsectors, true);

  inWorldCreation = oldIWC;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateSubRegion
//
//==========================================================================
void VRenderLevelShared::UpdateSubRegion (subsector_t *sub, subregion_t *region) {
  if (!region || !sub) return;

  // polyobj cannot be in subsector with 3d floors, so update it once
  if (sub->HasPObjs()) {
    // update the polyobj
    for (auto &&it : sub->PObjFirst()) {
      polyobj_t *pobj = it.value();
      seg_t **polySeg = pobj->segs;
      TSecPlaneRef po_floor = region->floorplane;
      TSecPlaneRef po_ceiling = region->ceilplane;
      sector_t *posec = pobj->originalSector;
      if (posec) {
        po_floor.set(&posec->floor, false);
        po_ceiling.set(&posec->ceiling, false);
      } else {
        po_floor = region->floorplane;
        po_ceiling = region->ceilplane;
      }
      for (int polyCount = pobj->numsegs; polyCount--; ++polySeg) {
        UpdateDrawSeg(sub, (*polySeg)->drawsegs, po_floor, po_ceiling);
      }
    }
  }

  for (; region; region = region->next) {
    TSecPlaneRef r_floor = region->floorplane;
    TSecPlaneRef r_ceiling = region->ceilplane;

    drawseg_t *ds = region->lines;
    for (int count = sub->numlines; count--; ++ds) {
      UpdateDrawSeg(sub, ds, r_floor, r_ceiling/*, ClipSegs*/);
    }

    if (region->realfloor) {
      // check if we have to remove zerosky flag
      // "zerosky" is set when the sector has zero height, and sky ceiling
      // this is what removes extra floors on Doom II MAP01, for example
      if (region->flags&subregion_t::SRF_ZEROSKY_FLOOR_HACK) {
        if (region->secregion->eceiling.splane->pic != skyflatnum ||
            region->secregion->efloor.splane->pic == skyflatnum ||
            sub->sector->floor.normal.z != 1.0f || sub->sector->ceiling.normal.z != -1.0f ||
            sub->sector->floor.minz != sub->sector->ceiling.minz)
        {
          // no more zerofloor
          //GCon->Logf(NAME_Debug, "deactivate ZEROSKY HACK: sub=%d; region=%p", (int)(ptrdiff_t)(sub-&Level->Subsectors[0]), region);
          region->flags &= ~subregion_t::SRF_ZEROSKY_FLOOR_HACK;
        }
      }
      UpdateSecSurface(region->realfloor, region->floorplane, sub, region);
    }
    if (region->fakefloor) {
      TSecPlaneRef fakefloor;
      fakefloor.set(&sub->sector->fakefloors->floorplane, false);
      if (!fakefloor.isFloor()) fakefloor.Flip();
      if (!region->fakefloor->esecplane.isFloor()) region->fakefloor->esecplane.Flip();
      UpdateSecSurface(region->fakefloor, fakefloor, sub, region, false/*allow cmap*/, true/*fake*/);
      //region->fakefloor->texinfo.Tex = GTextureManager[GTextureManager.DefaultTexture];
    }

    if (region->realceil) UpdateSecSurface(region->realceil, region->ceilplane, sub, region);
    if (region->fakeceil) {
      TSecPlaneRef fakeceil;
      fakeceil.set(&sub->sector->fakefloors->ceilplane, false);
      if (!fakeceil.isCeiling()) fakeceil.Flip();
      if (!region->fakeceil->esecplane.isCeiling()) region->fakeceil->esecplane.Flip();
      UpdateSecSurface(region->fakeceil, fakeceil, sub, region, false/*allow cmap*/, true/*fake*/);
      //region->fakeceil->texinfo.Tex = GTextureManager[GTextureManager.DefaultTexture];
    }

    /* polyobj cannot be in 3d floor
    if (updatePoly && sub->HasPObjs()) {
      // update the polyobj
      updatePoly = false;
      for (auto &&it : sub->PObjFirst()) {
        polyobj_t *pobj = it.value();
        seg_t **polySeg = pobj->segs;
        for (int polyCount = pobj->numsegs; polyCount--; ++polySeg) {
          UpdateDrawSeg(sub, (*polySeg)->drawsegs, r_floor, r_ceiling);
        }
      }
    }
    */
  }
}


//==========================================================================
//
//  VRenderLevelShared::UpdateSubsectorFloorSurfaces
//
//==========================================================================
void VRenderLevelShared::UpdateSubsectorFlatSurfaces (subsector_t *sub, bool dofloors, bool doceils, bool forced) {
  if (!sub || (!dofloors && !doceils)) return;
  if (!forced && sub->updateWorldFrame == updateWorldFrame) return;
  for (subregion_t *region = sub->regions; region; region = region->next) {
    if (dofloors) {
      if (region->realfloor) {
        // check if we have to remove zerosky flag
        // "zerosky" is set when the sector has zero height, and sky ceiling
        // this is what removes extra floors on Doom II MAP01, for example
        if (region->flags&subregion_t::SRF_ZEROSKY_FLOOR_HACK) {
          if (region->secregion->eceiling.splane->pic != skyflatnum ||
              region->secregion->efloor.splane->pic == skyflatnum ||
              sub->sector->floor.normal.z != 1.0f || sub->sector->ceiling.normal.z != -1.0f ||
              sub->sector->floor.minz != sub->sector->ceiling.minz)
          {
            // no more zerofloor
            //GCon->Logf(NAME_Debug, "deactivate ZEROSKY HACK: sub=%d; region=%p", (int)(ptrdiff_t)(sub-&Level->Subsectors[0]), region);
            region->flags &= ~subregion_t::SRF_ZEROSKY_FLOOR_HACK;
          }
        }
        UpdateSecSurface(region->realfloor, region->floorplane, sub, region, true/*no cmap*/); // ignore colormap
      }
      if (region->fakefloor) {
        TSecPlaneRef fakefloor;
        fakefloor.set(&sub->sector->fakefloors->floorplane, false);
        if (!fakefloor.isFloor()) fakefloor.Flip();
        if (!region->fakefloor->esecplane.isFloor()) region->fakefloor->esecplane.Flip();
        UpdateSecSurface(region->fakefloor, fakefloor, sub, region, true/*no cmap*/, true/*fake*/); // ignore colormap
      }
    }
    if (doceils) {
      if (region->realceil) UpdateSecSurface(region->realceil, region->ceilplane, sub, region);
      if (region->fakeceil) {
        TSecPlaneRef fakeceil;
        fakeceil.set(&sub->sector->fakefloors->ceilplane, false);
        if (!fakeceil.isCeiling()) fakeceil.Flip();
        if (!region->fakeceil->esecplane.isCeiling()) region->fakeceil->esecplane.Flip();
        UpdateSecSurface(region->fakeceil, fakeceil, sub, region, false/*allow cmap*/, true/*fake*/);
      }
    }
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
//  k8: this whole thing is a fuckin' mess. it should be rewritten.
//  we can simply create fakes, and let the renderer to the rest (i think).
//
//==========================================================================
void VRenderLevelShared::UpdateFakeFlats (sector_t *sector) {
  if (!r_viewleaf) return; // just in case

  // replace sector being drawn with a copy to be hacked
  fakefloor_t *ff = sector->fakefloors;
  if (!ff) return; //k8:just in case

  // sector_t::SF_ClipFakePlanes: improved texture control
  //   the real floor and ceiling will be drawn with the real sector's flats
  //   the fake floor and ceiling (from either side) will be drawn with the control sector's flats
  //   the real floor and ceiling will be drawn even when in the middle part, allowing lifts into and out of deep water to render correctly (not possible in Boom)
  //   this flag does not work properly with sloped floors, and, if flag 2 is not set, with sloped ceilings either

  const sector_t *hs = sector->heightsec;
  sector_t *viewhs = r_viewleaf->sector->heightsec;
  /*
  bool underwater = / *r_fakingunderwater ||* /
    //(viewhs && Drawer->vieworg.z <= viewhs->floor.GetPointZClamped(Drawer->vieworg));
    (hs && Drawer->vieworg.z <= hs->floor.GetPointZClamped(Drawer->vieworg));
  */
  //bool underwater = (viewhs && Drawer->vieworg.z <= viewhs->floor.GetPointZClamped(Drawer->vieworg));
  bool underwater = (hs && Drawer->vieworg.z <= hs->floor.GetPointZClamped(Drawer->vieworg));
  bool underwaterView = (viewhs && Drawer->vieworg.z <= viewhs->floor.GetPointZClamped(Drawer->vieworg));
  bool diffTex = !!(hs && hs->SectorFlags&sector_t::SF_ClipFakePlanes);

  ff->floorplane = sector->floor;
  ff->ceilplane = sector->ceiling;
  ff->params = sector->params;
  /*
  if (!underwater && diffTex && (hs->SectorFlags&sector_t::SF_FakeFloorOnly)) {
    ff->floorplane = hs->floor;
    ff->floorplane.pic = GTextureManager.DefaultTexture;
    return;
  }
  */
  /*
    ff->ceilplane.normal = -hs->floor.normal;
    ff->ceilplane.dist = -hs->floor.dist;
    ff->ceilplane.pic = GTextureManager.DefaultTexture;
    return;
  */
  //if (!underwater && diffTex) ff->floorplane = hs->floor;
  //return;

  //GCon->Logf("sector=%d; hs=%d", (int)(ptrdiff_t)(sector-&Level->Sectors[0]), (int)(ptrdiff_t)(hs-&Level->Sectors[0]));

  // replace floor and ceiling height with control sector's heights
  if (diffTex && !(hs->SectorFlags&sector_t::SF_FakeCeilingOnly)) {
    if (CopyPlaneIfValid(&ff->floorplane, &hs->floor, &sector->ceiling)) {
      ff->floorplane.pic = hs->floor.pic;
      //GCon->Logf("opic=%d; fpic=%d", sector->floor.pic.id, hs->floor.pic.id);
    } else if (hs && (hs->SectorFlags&sector_t::SF_FakeFloorOnly)) {
      if (underwater) {
        //GCon->Logf("viewhs=%s", (viewhs ? "tan" : "ona"));
        //tempsec->ColorMap = hs->ColorMap;
        ff->params.Fade = hs->params.Fade;
        if (!(hs->SectorFlags&sector_t::SF_NoFakeLight)) {
          ff->params.lightlevel = hs->params.lightlevel;
          ff->params.LightColor = hs->params.LightColor;
          /*
          if (floorlightlevel != nullptr) *floorlightlevel = GetFloorLight(hs);
          if (ceilinglightlevel != nullptr) *ceilinglightlevel = GetFloorLight(hs);
          */
          //ff->floorplane = (viewhs ? viewhs->floor : sector->floor);
        }
        ff->ceilplane = hs->floor;
        ff->ceilplane.flipInPlace();
        //ff->ceilplane.normal = -hs->floor.normal;
        //ff->ceilplane.dist = -hs->floor.dist;
        //ff->ceilplane.pic = GTextureManager.DefaultTexture;
        //ff->ceilplane.pic = hs->floor.pic;
      } else {
        ff->floorplane = hs->floor;
        //ff->floorplane.pic = hs->floor.pic;
        //ff->floorplane.pic = GTextureManager.DefaultTexture;
      }
      return;
    }
  } else {
    if (hs && !(hs->SectorFlags&sector_t::SF_FakeCeilingOnly)) {
      //ff->floorplane.normal = hs->floor.normal;
      //ff->floorplane.dist = hs->floor.dist;
      //GCon->Logf("  000");
      if (!underwater) *(TPlane *)&ff->floorplane = *(TPlane *)&hs->floor;
      //*(TPlane *)&ff->floorplane = *(TPlane *)&sector->floor;
      //CopyPlaneIfValid(&ff->floorplane, &hs->floor, &sector->ceiling);
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

  //float refflorz = hs->floor.GetPointZClamped(viewx, viewy);
  float refceilz = (hs ? hs->ceiling.GetPointZClamped(Drawer->vieworg) : 0); // k8: was `nullptr` -- wtf?!
  //float orgflorz = sector->floor.GetPointZClamped(viewx, viewy);
  float orgceilz = sector->ceiling.GetPointZClamped(Drawer->vieworg);

  if (underwater /*||(viewhs && Drawer->vieworg.z <= viewhs->floor.GetPointZClamped(Drawer->vieworg))*/) {
    //!ff->floorplane.normal = sector->floor.normal;
    //!ff->floorplane.dist = sector->floor.dist;
    //!ff->ceilplane.normal = -hs->floor.normal;
    //!ff->ceilplane.dist = -hs->floor.dist/* - -hs->floor.normal.z*/;
    *(TPlane *)&ff->floorplane = *(TPlane *)&sector->floor;
    *(TPlane *)&ff->ceilplane = *(TPlane *)&hs->ceiling;
    //ff->ColorMap = hs->ColorMap;
    if (underwaterView) ff->params.Fade = hs->params.Fade;
  }

  // killough 11/98: prevent sudden light changes from non-water sectors:
  if ((underwater /*&& !back*/) || underwaterView) {
    // head-below-floor hack
    ff->floorplane.pic = (diffTex ? sector->floor.pic : hs->floor.pic);
    ff->floorplane.xoffs = hs->floor.xoffs;
    ff->floorplane.yoffs = hs->floor.yoffs;
    ff->floorplane.XScale = hs->floor.XScale;
    ff->floorplane.YScale = hs->floor.YScale;
    ff->floorplane.Angle = hs->floor.Angle;
    ff->floorplane.BaseAngle = hs->floor.BaseAngle;
    ff->floorplane.BaseYOffs = hs->floor.BaseYOffs;
    //ff->floorplane = hs->floor;
    //*(TPlane *)&ff->floorplane = *(TPlane *)&sector->floor;
    //ff->floorplane.dist -= 42;
    //ff->floorplane.dist += 9;

    ff->ceilplane.normal = -hs->floor.normal;
    ff->ceilplane.dist = -hs->floor.dist/* - -hs->floor.normal.z*/;
    //ff->ceilplane.pic = GTextureManager.DefaultTexture;
    //GCon->Logf("!!!");
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
      ff->ceilplane.pic = (diffTex ? sector->floor.pic : hs->ceiling.pic);
      ff->ceilplane.xoffs = hs->ceiling.xoffs;
      ff->ceilplane.yoffs = hs->ceiling.yoffs;
      ff->ceilplane.XScale = hs->ceiling.XScale;
      ff->ceilplane.YScale = hs->ceiling.YScale;
      ff->ceilplane.Angle = hs->ceiling.Angle;
      ff->ceilplane.BaseAngle = hs->ceiling.BaseAngle;
      ff->ceilplane.BaseYOffs = hs->ceiling.BaseYOffs;
    }

    // k8: why underwaterView? because of kdizd bugs
    //     this seems to be totally wrong, though
    if (!(hs->SectorFlags&sector_t::SF_NoFakeLight) && /*underwaterView*/viewhs) {
      ff->params.lightlevel = hs->params.lightlevel;
      ff->params.LightColor = hs->params.LightColor;
      /*
      if (floorlightlevel != nullptr) *floorlightlevel = GetFloorLight(hs);
      if (ceilinglightlevel != nullptr) *ceilinglightlevel = GetFloorLight(hs);
      */
    }
  } else if (((hs && Drawer->vieworg.z > hs->ceiling.GetPointZClamped(Drawer->vieworg)) || //k8: dunno, it was `floor` there, and it seems to be a typo
              (viewhs && Drawer->vieworg.z > viewhs->ceiling.GetPointZClamped(Drawer->vieworg))) &&
             orgceilz > refceilz && !(hs->SectorFlags&sector_t::SF_FakeFloorOnly))
  {
    // above-ceiling hack
    ff->ceilplane.normal = hs->ceiling.normal;
    ff->ceilplane.dist = hs->ceiling.dist;
    ff->floorplane.normal = -hs->ceiling.normal;
    ff->floorplane.dist = -hs->ceiling.dist;
    ff->params.Fade = hs->params.Fade;
    //ff->params.ColorMap = hs->params.ColorMap;

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

    // k8: why underwaterView? because of kdizd bugs
    //     this seems to be totally wrong, though
    if (!(hs->SectorFlags&sector_t::SF_NoFakeLight) && viewhs) {
      ff->params.lightlevel = hs->params.lightlevel;
      ff->params.LightColor = hs->params.LightColor;
    }
  } else {
    if (diffTex) {
      ff->floorplane = hs->floor;
      ff->ceilplane = hs->floor;
      if (Drawer->vieworg.z < hs->floor.GetPointZClamped(Drawer->vieworg)) {
        // fake floor is actually a ceiling now
        ff->floorplane.flipInPlace();
      }
      if (Drawer->vieworg.z > hs->ceiling.GetPointZClamped(Drawer->vieworg)) {
        // fake ceiling is actually a floor now
        ff->ceilplane.flipInPlace();
      }
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
  //GCon->Logf("UpdateFloodBug: sector #%d (bridge: %s)", (int)(ptrdiff_t)(sector-&Level->Sectors[0]), (sector->SectorFlags&sector_t::SF_HangingBridge ? "tan" : "ona"));
  if (sector->SectorFlags&sector_t::SF_HangingBridge) {
    sector_t *sursec = sector->othersecFloor;
    ff->floorplane = sursec->floor;
    // ceiling must be current sector's floor, flipped
    ff->ceilplane = sector->floor;
    ff->ceilplane.flipInPlace();
    ff->params = sursec->params;
    //GCon->Logf("  floor: (%g,%g,%g : %g)", ff->floorplane.normal.x, ff->floorplane.normal.y, ff->floorplane.normal.z, ff->floorplane.dist);
    return;
  }
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
    if (sector->heightsec->SectorFlags&sector_t::SF_IgnoreHeightSec) return;
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
//  VRenderLevelShared::FreeSurfaces
//
//==========================================================================
void VRenderLevelShared::FreeSurfaces (surface_t *InSurf) {
  /*
  surface_t *next;
  for (surface_t *s = InSurf; s; s = next) {
    if (s->CacheSurf) FreeSurfCache(s->CacheSurf);
    s->FreeLightmaps();
    next = s->next;
    Z_Free(s);
  }
  */
  FreeWSurfs(InSurf);
}


//==========================================================================
//
//  VRenderLevelShared::FreeSegParts
//
//==========================================================================
void VRenderLevelShared::FreeSegParts (segpart_t *ASP) {
  for (segpart_t *sp = ASP; sp; sp = sp->next) FreeWSurfs(sp->surfs);
}
