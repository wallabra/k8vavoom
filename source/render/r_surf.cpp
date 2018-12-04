//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
#include "gamedefs.h"
#include "r_local.h"

#define USE_FASTER_SUBDIVIDER


#define ON_EPSILON      (0.1)
#define subdivide_size  (240)

#define MAXWVERTS  (8)
#define WSURFSIZE  (sizeof(surface_t)+sizeof(TVec)*(MAXWVERTS-1))

// this is used to compare floats like ints which is faster
#define FASI(var) (*(int *)&var)


// ////////////////////////////////////////////////////////////////////////// //
subsector_t *r_surf_sub;
static sec_plane_t *r_floor;
static sec_plane_t *r_ceiling;

static segpart_t *pspart;


//**************************************************************************
//
//  Scaling
//
//**************************************************************************

//==========================================================================
//
//  TextureSScale
//
//==========================================================================
static inline float TextureSScale (VTexture *pic) {
  return pic->SScale;
}


//==========================================================================
//
//  TextureTScale
//
//==========================================================================
static inline float TextureTScale (VTexture *pic) {
  return pic->TScale;
}


//==========================================================================
//
//  TextureOffsetSScale
//
//==========================================================================
static inline float TextureOffsetSScale (VTexture *pic) {
  return (pic->bWorldPanning ? pic->SScale : 1.0);
}


//==========================================================================
//
//  TextureOffsetTScale
//
//==========================================================================
static inline float TextureOffsetTScale (VTexture *pic) {
  return (pic->bWorldPanning ? pic->TScale : 1.0);
}


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
  guard(VRenderLevelShared::SetupSky);
  skyheight = -99999.0;
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
  sky_plane.Alpha = 1.1;
  sky_plane.LightSourceSector = -1;
  sky_plane.MirrorAlpha = 1.0;
  sky_plane.XScale = 1.0;
  sky_plane.YScale = 1.0;
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::FlushSurfCaches
//
//==========================================================================
void VRenderLevelShared::FlushSurfCaches (surface_t *InSurfs) {
  guard(VRenderLevelShared::FlushSurfCaches);
  surface_t *surfs = InSurfs;
  while (surfs) {
    if (surfs->CacheSurf) FreeSurfCache(surfs->CacheSurf);
    surfs = surfs->next;
  }
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::CreateSecSurface
//
//==========================================================================
sec_surface_t *VRenderLevelShared::CreateSecSurface (subsector_t *sub, sec_plane_t *InSplane) {
  guard(VRenderLevelShared::CreateSecSurface);
  sec_plane_t *splane = InSplane;

  sec_surface_t *ssurf = new sec_surface_t;
  memset((void *)ssurf, 0, sizeof(sec_surface_t));
  surface_t *surf = (surface_t *)Z_Calloc(sizeof(surface_t)+(sub->numlines-1)*sizeof(TVec));

  if (splane->pic == skyflatnum && splane->normal.z < 0.0) splane = &sky_plane;
  ssurf->secplane = splane;
  ssurf->dist = splane->dist;

  // setup texture
  VTexture *Tex = GTextureManager(splane->pic);
  if (fabs(splane->normal.z) > 0.1) {
    ssurf->texinfo.saxis = TVec(mcos(splane->BaseAngle-splane->Angle),
      msin(splane->BaseAngle-splane->Angle), 0)*TextureSScale(Tex)*splane->XScale;
    ssurf->texinfo.taxis = TVec(msin(splane->BaseAngle-splane->Angle),
      -mcos(splane->BaseAngle-splane->Angle), 0)*TextureTScale(Tex)*splane->YScale;
  } else {
    ssurf->texinfo.taxis = TVec(0, 0, -1)*TextureTScale(Tex)*splane->YScale;
    ssurf->texinfo.saxis = Normalise(CrossProduct(splane->normal, ssurf->texinfo.taxis))*TextureSScale(Tex)*splane->XScale;
  }
  ssurf->texinfo.soffs = splane->xoffs;
  ssurf->texinfo.toffs = splane->yoffs+splane->BaseYOffs;
  ssurf->texinfo.Tex = Tex;
  ssurf->texinfo.noDecals = (Tex ? Tex->noDecals : true);
  ssurf->texinfo.Alpha = (splane->Alpha < 1.0 ? splane->Alpha : 1.1);
  ssurf->texinfo.Additive = !!(splane->flags&SPF_ADDITIVE);
  ssurf->texinfo.ColourMap = 0;
  ssurf->XScale = splane->XScale;
  ssurf->YScale = splane->YScale;
  ssurf->Angle = splane->BaseAngle-splane->Angle;

  //surf->subsector = sub; // this is required to calculate static lightmaps, and done in `InitSurfs()`
  surf->count = sub->numlines;
  seg_t *line = &Level->Segs[sub->firstline];
  bool vlindex = (splane->normal.z < 0);
  //fprintf(stderr, "surf; first=%d; count=%d; num=%d; max=%d; endidx=%d\n", sub->firstline, sub->numlines, surf->count, Level->NumSegs-1, sub->firstline+sub->numlines-1);
  for (int i = 0; i < surf->count; ++i) {
    TVec &v = *line[vlindex ? surf->count-i-1 : i].v1;
    TVec &dst = surf->verts[i];
    dst = v;
    dst.z = splane->GetPointZ(dst);
  }

  if (splane->pic == skyflatnum) {
    // don't subdivide sky
    ssurf->surfs = surf;
    surf->texinfo = &ssurf->texinfo;
    surf->plane = splane;
  } else {
    ssurf->surfs = SubdivideFace(surf, ssurf->texinfo.saxis, &ssurf->texinfo.taxis);
    InitSurfs(ssurf->surfs, &ssurf->texinfo, splane, sub);
  }

  return ssurf;
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateSecSurface
//
//==========================================================================
void VRenderLevelShared::UpdateSecSurface (sec_surface_t *ssurf, sec_plane_t *RealPlane, subsector_t *sub) {
  guard(VRenderLevelShared::UpdateSecSurface);
  sec_plane_t *plane = ssurf->secplane;

  if (!plane->pic) return;

  if (plane != RealPlane) {
    // check for sky changes
    if (plane->pic == skyflatnum && RealPlane->pic != skyflatnum) {
      ssurf->secplane = RealPlane;
      plane = RealPlane;
      if (!ssurf->surfs->extents[0]) {
        ssurf->surfs = SubdivideFace(ssurf->surfs, ssurf->texinfo.saxis, &ssurf->texinfo.taxis);
        InitSurfs(ssurf->surfs, &ssurf->texinfo, plane, sub);
      }
    } else if (plane->pic != skyflatnum && RealPlane->pic == skyflatnum) {
      // don't subdivide sky
      ssurf->secplane = &sky_plane;
      plane = &sky_plane;
    }
  }

  ssurf->texinfo.ColourMap = ColourMap;
  VTexture *newtex = GTextureManager(plane->pic);
  if (ssurf->texinfo.Tex != newtex) {
    ssurf->texinfo.Tex = newtex;
    ssurf->texinfo.noDecals = (newtex ? newtex->noDecals : true);
  }
  if (FASI(ssurf->dist) != FASI(plane->dist)) {
    ssurf->dist = plane->dist;
    for (surface_t *surf = ssurf->surfs; surf; surf = surf->next) {
      for (int i = 0; i < surf->count; ++i) surf->verts[i].z = plane->GetPointZ(surf->verts[i]);
    }
    if (plane->pic != skyflatnum) {
      FlushSurfCaches(ssurf->surfs);
      InitSurfs(ssurf->surfs, &ssurf->texinfo, nullptr, sub);
    }
  }
  if (FASI(ssurf->XScale) != FASI(plane->XScale) ||
      FASI(ssurf->YScale) != FASI(plane->YScale) ||
      ssurf->Angle != plane->BaseAngle-plane->Angle)
  {
    if (fabs(plane->normal.z) > 0.1) {
      ssurf->texinfo.saxis = TVec(mcos(plane->BaseAngle-plane->Angle),
        msin(plane->BaseAngle-plane->Angle), 0)*TextureSScale(ssurf->texinfo.Tex)*plane->XScale;
      ssurf->texinfo.taxis = TVec(msin(plane->BaseAngle-plane->Angle),
        -mcos(plane->BaseAngle-plane->Angle), 0)*TextureTScale(ssurf->texinfo.Tex)*plane->YScale;
    } else {
      ssurf->texinfo.taxis = TVec(0, 0, -1)*TextureTScale(ssurf->texinfo.Tex)*plane->YScale;
      ssurf->texinfo.saxis = Normalise(CrossProduct(plane->normal, ssurf->texinfo.taxis))*TextureSScale(ssurf->texinfo.Tex)*plane->XScale;
    }
    ssurf->texinfo.soffs = plane->xoffs;
    ssurf->texinfo.toffs = plane->yoffs+plane->BaseYOffs;
    ssurf->XScale = plane->XScale;
    ssurf->YScale = plane->YScale;
    ssurf->Angle = plane->BaseAngle-plane->Angle;
    if (plane->pic != skyflatnum) {
      FreeSurfaces(ssurf->surfs);
      surface_t *surf = (surface_t *)Z_Calloc(sizeof(surface_t)+(sub->numlines-1)*sizeof(TVec));
      surf->count = sub->numlines;
      seg_t *line = &Level->Segs[sub->firstline];
      bool vlindex = (plane->normal.z < 0);
      for (int i = 0; i < surf->count; ++i) {
        TVec &v = *line[vlindex ? surf->count-i-1 : i].v1;
        TVec &dst = surf->verts[i];
        dst = v;
        dst.z = plane->GetPointZ(dst);
      }
      ssurf->surfs = SubdivideFace(surf, ssurf->texinfo.saxis, &ssurf->texinfo.taxis);
      InitSurfs(ssurf->surfs, &ssurf->texinfo, plane, sub);
    }
  } else if (FASI(ssurf->texinfo.soffs) != FASI(plane->xoffs) ||
             ssurf->texinfo.toffs != plane->yoffs+plane->BaseYOffs)
  {
    ssurf->texinfo.soffs = plane->xoffs;
    ssurf->texinfo.toffs = plane->yoffs+plane->BaseYOffs;
    if (plane->pic != skyflatnum) {
      FlushSurfCaches(ssurf->surfs);
      InitSurfs(ssurf->surfs, &ssurf->texinfo, nullptr, sub);
    }
  }
  unguard;
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
  guard(VRenderLevelShared::NewWSurf);
  if (!free_wsurfs) {
    // allocate some more surfs
    vuint8 *tmp = (vuint8 *)Z_Malloc(WSURFSIZE*128+sizeof(void *));
    memset(tmp, 0, WSURFSIZE*128+sizeof(void *));
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

  return surf;
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::FreeWSurfs
//
//==========================================================================
void VRenderLevelShared::FreeWSurfs (surface_t *InSurfs) {
  guard(VRenderLevelShared::FreeWSurfs);
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
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::CreateWSurfs
//
//==========================================================================
surface_t *VRenderLevelShared::CreateWSurfs (TVec *wv, texinfo_t *texinfo, seg_t *seg, subsector_t *sub) {
  guard(VRenderLevelShared::CreateWSurfs);
  if (wv[1].z <= wv[0].z && wv[2].z <= wv[3].z) return nullptr;

  if (texinfo->Tex->Type == TEXTYPE_Null) return nullptr;

  surface_t *surf = NewWSurf();
  surf->next = nullptr;
  surf->count = 4;
  memcpy(surf->verts, wv, surf->count*sizeof(TVec));

  if (texinfo->Tex == GTextureManager[skyflatnum]) {
    // never split sky surfaces
    surf->texinfo = texinfo;
    surf->plane = seg;
    return surf;
  }

  surf = SubdivideSeg(surf, texinfo->saxis, &texinfo->taxis);
  InitSurfs(surf, texinfo, seg, sub);
  return surf;
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::CountSegParts
//
//==========================================================================
int VRenderLevelShared::CountSegParts (seg_t *seg) {
  guard(VRenderLevelShared::CountSegParts);
  if (!seg->linedef) return 0; // miniseg
  int count;
  if (!seg->backsector) {
    count = 2;
  } else {
    count = 4;
    for (sec_region_t *reg = seg->backsector->topregion; reg->prev; reg = reg->prev) ++count;
  }
  return count;
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::CreateSegParts
//
//==========================================================================
void VRenderLevelShared::CreateSegParts (drawseg_t *dseg, seg_t *seg) {
  guard(VRenderLevelShared::CreateSegParts);
  TVec wv[4];
  segpart_t *sp;

  dseg->seg = seg;
  dseg->next = seg->drawsegs;
  seg->drawsegs = dseg;

  if (!seg->linedef) return; // miniseg

  side_t *sidedef = seg->sidedef;
  line_t *linedef = seg->linedef;

  TVec segdir = (*seg->v2-*seg->v1)/seg->length;

  float topz1 = r_ceiling->GetPointZ(*seg->v1);
  float topz2 = r_ceiling->GetPointZ(*seg->v2);
  float botz1 = r_floor->GetPointZ(*seg->v1);
  float botz2 = r_floor->GetPointZ(*seg->v2);

  if (!seg->backsector) {
    dseg->mid = pspart++;
    sp = dseg->mid;

    VTexture *MTex = GTextureManager(sidedef->MidTexture);
    // k8: one-sided line should have a midtex
    if (!MTex) {
      GCon->Logf(NAME_Warning, "Sidedef #%d should have midtex, but it hasn't (%d)", (int)(ptrdiff_t)(sidedef-Level->Sides), sidedef->MidTexture);
      MTex = GTextureManager[GTextureManager.DefaultTexture];
    }
    sp->texinfo.saxis = segdir*TextureSScale(MTex);
    sp->texinfo.taxis = TVec(0, 0, -1)*TextureTScale(MTex);
    sp->texinfo.soffs = -DotProduct(*seg->v1, sp->texinfo.saxis)+seg->offset*TextureSScale(MTex)+sidedef->MidTextureOffset*TextureOffsetSScale(MTex);
    sp->texinfo.Tex = MTex;
    sp->texinfo.noDecals = (MTex ? MTex->noDecals : true);
    sp->texinfo.Alpha = 1.1;
    sp->texinfo.Additive = false;
    sp->texinfo.ColourMap = 0;

    if (linedef->flags&ML_DONTPEGBOTTOM) {
      // bottom of texture at bottom
      sp->texinfo.toffs = r_floor->TexZ+MTex->GetScaledHeight();
    } else if (linedef->flags&ML_DONTPEGTOP) {
      // top of texture at top of top region
      sp->texinfo.toffs = r_surf_sub->sector->topregion->ceiling->TexZ;
    } else {
      // top of texture at top
      sp->texinfo.toffs = r_ceiling->TexZ;
    }
    sp->texinfo.toffs *= TextureTScale(MTex);
    sp->texinfo.toffs += sidedef->MidRowOffset*TextureOffsetTScale(MTex);

    wv[0].x = wv[1].x = seg->v1->x;
    wv[0].y = wv[1].y = seg->v1->y;
    wv[2].x = wv[3].x = seg->v2->x;
    wv[2].y = wv[3].y = seg->v2->y;

    wv[0].z = botz1;
    wv[1].z = topz1;
    wv[2].z = topz2;
    wv[3].z = botz2;

    sp->surfs = CreateWSurfs(wv, &sp->texinfo, seg, r_surf_sub);

    sp->frontTopDist = r_ceiling->dist;
    sp->frontBotDist = r_floor->dist;
    sp->TextureOffset = sidedef->MidTextureOffset;
    sp->RowOffset = sidedef->MidRowOffset;

    // sky above line.
    dseg->topsky = pspart++;
    sp = dseg->topsky;
    sp->texinfo.Tex = GTextureManager[skyflatnum];
    sp->texinfo.noDecals = (sp->texinfo.Tex ? sp->texinfo.Tex->noDecals : true);
    sp->texinfo.Alpha = 1.1;
    sp->texinfo.Additive = false;
    sp->texinfo.ColourMap = 0;
    if (IsSky(r_ceiling)) {
      wv[0].x = wv[1].x = seg->v1->x;
      wv[0].y = wv[1].y = seg->v1->y;
      wv[2].x = wv[3].x = seg->v2->x;
      wv[2].y = wv[3].y = seg->v2->y;

      wv[0].z = topz1;
      wv[1].z = wv[2].z = skyheight;
      wv[3].z = topz2;

      sp->surfs = CreateWSurfs(wv, &sp->texinfo, seg, r_surf_sub);

      sp->frontTopDist = r_ceiling->dist;
    }
  } else {
    // two sided line
    sec_plane_t *back_floor = &seg->backsector->floor;
    sec_plane_t *back_ceiling = &seg->backsector->ceiling;
    if (seg->backsector->fakefloors) {
      if (back_floor == &seg->backsector->floor) back_floor = &seg->backsector->fakefloors->floorplane;
      if (back_ceiling == &seg->backsector->ceiling) back_ceiling = &seg->backsector->fakefloors->ceilplane;
    }

    VTexture *TTex = GTextureManager(sidedef->TopTexture);

    float back_topz1 = back_ceiling->GetPointZ(*seg->v1);
    float back_topz2 = back_ceiling->GetPointZ(*seg->v2);
    float back_botz1 = back_floor->GetPointZ(*seg->v1);
    float back_botz2 = back_floor->GetPointZ(*seg->v2);

    // hack to allow height changes in outdoor areas
    float top_topz1 = topz1;
    float top_topz2 = topz2;
    float top_TexZ = r_ceiling->TexZ;
    if (IsSky(r_ceiling) && IsSky(back_ceiling)) {
      if (r_ceiling->SkyBox == back_ceiling->SkyBox) {
        top_topz1 = back_topz1;
        top_topz2 = back_topz2;
        top_TexZ = back_ceiling->TexZ;
      } else {
        TTex = GTextureManager[skyflatnum];
      }
    }

    // top wall
    dseg->top = pspart++;
    sp = dseg->top;

    sp->texinfo.saxis = segdir*TextureSScale(TTex);
    sp->texinfo.taxis = TVec(0, 0, -1)*TextureTScale(TTex);
    sp->texinfo.soffs = -DotProduct(*seg->v1, sp->texinfo.saxis)+seg->offset*TextureSScale(TTex)+sidedef->TopTextureOffset*TextureOffsetSScale(TTex);
    sp->texinfo.Tex = TTex;
    sp->texinfo.noDecals = (TTex ? TTex->noDecals : true);
    sp->texinfo.Alpha = 1.1;
    sp->texinfo.Additive = false;
    sp->texinfo.ColourMap = 0;

    if (linedef->flags&ML_DONTPEGTOP) {
      // top of texture at top
      sp->texinfo.toffs = top_TexZ;
    } else {
      // bottom of texture
      sp->texinfo.toffs = back_ceiling->TexZ+TTex->GetScaledHeight();
    }
    sp->texinfo.toffs *= TextureTScale(TTex);
    sp->texinfo.toffs += sidedef->TopRowOffset*TextureOffsetTScale(TTex);

    wv[0].x = wv[1].x = seg->v1->x;
    wv[0].y = wv[1].y = seg->v1->y;
    wv[2].x = wv[3].x = seg->v2->x;
    wv[2].y = wv[3].y = seg->v2->y;

    wv[0].z = MAX(back_topz1, botz1);
    wv[1].z = top_topz1;
    wv[2].z = top_topz2;
    wv[3].z = MAX(back_topz2, botz2);

    sp->surfs = CreateWSurfs(wv, &sp->texinfo, seg, r_surf_sub);

    sp->frontTopDist = r_ceiling->dist;
    sp->frontBotDist = r_floor->dist;
    sp->backTopDist = back_ceiling->dist;
    sp->backBotDist = back_floor->dist;
    sp->TextureOffset = sidedef->TopTextureOffset;
    sp->RowOffset = sidedef->TopRowOffset;

    // sky above top
    dseg->topsky = pspart++;
    if (IsSky(r_ceiling) && !IsSky(back_ceiling)) {
      sp = dseg->topsky;

      sp->texinfo.Tex = GTextureManager[skyflatnum];
      sp->texinfo.noDecals = (sp->texinfo.Tex ? sp->texinfo.Tex->noDecals : true);
      sp->texinfo.Alpha = 1.1;
      sp->texinfo.Additive = false;
      sp->texinfo.ColourMap = 0;

      wv[0].x = wv[1].x = seg->v1->x;
      wv[0].y = wv[1].y = seg->v1->y;
      wv[2].x = wv[3].x = seg->v2->x;
      wv[2].y = wv[3].y = seg->v2->y;

      wv[0].z = topz1;
      wv[1].z = wv[2].z = skyheight;
      wv[3].z = topz2;

      sp->surfs = CreateWSurfs(wv, &sp->texinfo, seg, r_surf_sub);

      sp->frontTopDist = r_ceiling->dist;
    }

    // bottom wall
    dseg->bot = pspart++;
    sp = dseg->bot;

    VTexture *BTex = GTextureManager(sidedef->BottomTexture);
    sp->texinfo.saxis = segdir*TextureSScale(BTex);
    sp->texinfo.taxis = TVec(0, 0, -1)*TextureTScale(BTex);
    sp->texinfo.soffs = -DotProduct(*seg->v1, sp->texinfo.saxis)+seg->offset*TextureSScale(BTex)+sidedef->BotTextureOffset*TextureOffsetSScale(BTex);
    sp->texinfo.Tex = BTex;
    sp->texinfo.noDecals = (BTex ? BTex->noDecals : true);
    sp->texinfo.Alpha = 1.1;
    sp->texinfo.Additive = false;
    sp->texinfo.ColourMap = 0;

    if (linedef->flags&ML_DONTPEGBOTTOM) {
      // bottom of texture at bottom
      // top of texture at top
      sp->texinfo.toffs = top_TexZ;
    } else {
      // top of texture at top
      sp->texinfo.toffs = back_floor->TexZ;
    }
    sp->texinfo.toffs *= TextureTScale(BTex);
    sp->texinfo.toffs += sidedef->BotRowOffset*TextureOffsetTScale(BTex);

    wv[0].x = wv[1].x = seg->v1->x;
    wv[0].y = wv[1].y = seg->v1->y;
    wv[2].x = wv[3].x = seg->v2->x;
    wv[2].y = wv[3].y = seg->v2->y;

    wv[0].z = botz1;
    wv[1].z = MIN(back_botz1, topz1);
    wv[2].z = MIN(back_botz2, topz2);
    wv[3].z = botz2;

    sp->surfs = CreateWSurfs(wv, &sp->texinfo, seg, r_surf_sub);

    sp->frontTopDist = r_ceiling->dist;
    sp->frontBotDist = r_floor->dist;
    sp->backTopDist = back_ceiling->dist;
    sp->backBotDist = back_floor->dist;
    sp->TextureOffset = sidedef->BotTextureOffset;
    sp->RowOffset = sidedef->BotRowOffset;

    float midtopz1 = topz1;
    float midtopz2 = topz2;
    float midbotz1 = botz1;
    float midbotz2 = botz2;
    if (topz1 > back_topz1 && sidedef->TopTexture > 0) {
      midtopz1 = back_topz1;
      midtopz2 = back_topz2;
    }
    if (botz1 < back_botz1 && sidedef->BottomTexture > 0) {
      midbotz1 = back_botz1;
      midbotz2 = back_botz2;
    }

    dseg->mid = pspart++;
    sp = dseg->mid;

    VTexture *MTex = GTextureManager(sidedef->MidTexture);
    sp->texinfo.Tex = MTex;
    sp->texinfo.noDecals = (MTex ? MTex->noDecals : true);
    sp->texinfo.ColourMap = 0;
    if (MTex->Type != TEXTYPE_Null) {
      // masked MidTexture
      float texh = MTex->GetScaledHeight();
      float z_org;

      sp->texinfo.saxis = segdir*TextureSScale(MTex);
      sp->texinfo.taxis = TVec(0, 0, -1)*TextureTScale(MTex);
      sp->texinfo.soffs = -DotProduct(*seg->v1, sp->texinfo.saxis)+seg->offset*TextureSScale(MTex)+sidedef->MidTextureOffset*TextureOffsetSScale(MTex);
      sp->texinfo.Alpha = linedef->alpha;
      sp->texinfo.Additive = !!(linedef->flags&ML_ADDITIVE);

      if (linedef->flags&ML_DONTPEGBOTTOM) {
        // bottom of texture at bottom
        // top of texture at top
        z_org = MAX(seg->frontsector->floor.TexZ, seg->backsector->floor.TexZ)+texh;
      } else {
        // top of texture at top
        z_org = MIN(seg->frontsector->ceiling.TexZ, seg->backsector->ceiling.TexZ);
      }
      z_org += sidedef->MidRowOffset*(!MTex->bWorldPanning ? 1.0 : 1.0/MTex->TScale);

      sp->texinfo.toffs = z_org*TextureTScale(MTex);

      wv[0].x = wv[1].x = seg->v1->x;
      wv[0].y = wv[1].y = seg->v1->y;
      wv[2].x = wv[3].x = seg->v2->x;
      wv[2].y = wv[3].y = seg->v2->y;

      if (linedef->flags&ML_WRAP_MIDTEX) {
        wv[0].z = MAX(midbotz1, z_org-texh);
        wv[1].z = MIN(midtopz1, z_org);
        wv[2].z = MIN(midtopz2, z_org);
        wv[3].z = MAX(midbotz2, z_org-texh);
        /*
        wv[0].z = midbotz1;
        wv[1].z = midtopz1;
        wv[2].z = midtopz2;
        wv[3].z = midbotz2;
        */
      } else {
        wv[0].z = MAX(midbotz1, z_org-texh);
        wv[1].z = MIN(midtopz1, z_org);
        wv[2].z = MIN(midtopz2, z_org);
        wv[3].z = MAX(midbotz2, z_org-texh);
      }

      sp->surfs = CreateWSurfs(wv, &sp->texinfo, seg, r_surf_sub);
    }

    sp->frontTopDist = r_ceiling->dist;
    sp->frontBotDist = r_floor->dist;
    sp->backTopDist = back_ceiling->dist;
    sp->backBotDist = back_floor->dist;
    sp->TextureOffset = sidedef->MidTextureOffset;
    sp->RowOffset = sidedef->MidRowOffset;

    sec_region_t *reg;
    for (reg = seg->backsector->topregion; reg->prev; reg = reg->prev) {
      sp = pspart++;
      sp->next = dseg->extra;
      dseg->extra = sp;

      sec_plane_t *extratop = reg->floor;
      sec_plane_t *extrabot = reg->prev->ceiling;
      side_t *extraside = &Level->Sides[reg->prev->extraline->sidenum[0]];

      float extratopz1 = extratop->GetPointZ(*seg->v1);
      float extratopz2 = extratop->GetPointZ(*seg->v2);
      float extrabotz1 = extrabot->GetPointZ(*seg->v1);
      float extrabotz2 = extrabot->GetPointZ(*seg->v2);

      VTexture *MTextr = GTextureManager(extraside->MidTexture);
      sp->texinfo.saxis = segdir*TextureSScale(MTextr);
      sp->texinfo.taxis = TVec(0, 0, -1)*TextureTScale(MTextr);
      sp->texinfo.soffs = -DotProduct(*seg->v1, sp->texinfo.saxis)+seg->offset*TextureSScale(MTextr)+sidedef->MidTextureOffset*TextureOffsetSScale(MTextr);

      sp->texinfo.toffs = extratop->TexZ*TextureTScale(MTextr)+sidedef->MidRowOffset*TextureOffsetTScale(MTextr);
      sp->texinfo.Tex = MTextr;
      sp->texinfo.noDecals = (MTextr ? MTextr->noDecals : true);
      sp->texinfo.Alpha = extrabot->Alpha < 1.0 ? extrabot->Alpha : 1.1;
      sp->texinfo.Additive = !!(extrabot->flags&SPF_ADDITIVE);
      sp->texinfo.ColourMap = 0;

      wv[0].x = wv[1].x = seg->v1->x;
      wv[0].y = wv[1].y = seg->v1->y;
      wv[2].x = wv[3].x = seg->v2->x;
      wv[2].y = wv[3].y = seg->v2->y;

      wv[0].z = MAX(extrabotz1, botz1);
      wv[1].z = MIN(extratopz1, topz1);
      wv[2].z = MIN(extratopz2, topz2);
      wv[3].z = MAX(extrabotz2, botz2);

      sp->surfs = CreateWSurfs(wv, &sp->texinfo, seg, r_surf_sub);

      sp->frontTopDist = r_ceiling->dist;
      sp->frontBotDist = r_floor->dist;
      sp->backTopDist = extratop->dist;
      sp->backBotDist = extrabot->dist;
      sp->TextureOffset = sidedef->MidTextureOffset;
      sp->RowOffset = sidedef->MidRowOffset;
    }
  }
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateRowOffset
//
//==========================================================================
void VRenderLevelShared::UpdateRowOffset (segpart_t *sp, float RowOffset) {
  guard(VRenderLevelShared::UpdateRowOffset);
  sp->texinfo.toffs += (RowOffset-sp->RowOffset)*TextureOffsetTScale(sp->texinfo.Tex);
  sp->RowOffset = RowOffset;
  FlushSurfCaches(sp->surfs);
  InitSurfs(sp->surfs, &sp->texinfo, nullptr, r_surf_sub);
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateTextureOffset
//
//==========================================================================
void VRenderLevelShared::UpdateTextureOffset (segpart_t *sp, float TextureOffset) {
  guard(VRenderLevelShared::UpdateTextureOffset);
  sp->texinfo.soffs += (TextureOffset-sp->TextureOffset)*TextureOffsetSScale(sp->texinfo.Tex);
  sp->TextureOffset = TextureOffset;
  FlushSurfCaches(sp->surfs);
  InitSurfs(sp->surfs, &sp->texinfo, nullptr, r_surf_sub);
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateDrawSeg
//
//==========================================================================
void VRenderLevelShared::UpdateDrawSeg (drawseg_t *dseg, bool ShouldClip) {
  guard(VRenderLevelShared::UpdateDrawSeg);
  seg_t *seg = dseg->seg;
  segpart_t *sp;
  TVec wv[4];

  if (!seg->linedef) return; // miniseg

  if (ShouldClip) {
    // clip sectors that are behind rendered segs
    TVec v1 = *seg->v1;
    TVec v2 = *seg->v2;
    TVec r1 = vieworg-v1;
    TVec r2 = vieworg-v2;
    float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), vieworg);
    float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), vieworg);

    // there might be a better method of doing this, but this one works for now...
         if (D1 > 0.0 && D2 < 0.0) v2 += (v2-v1)*D1/(D1-D2);
    else if (D2 > 0.0 && D1 < 0.0) v1 += (v1-v2)*D2/(D2-D1);

    if (!ViewClip.IsRangeVisible(ViewClip.PointToClipAngle(v2), ViewClip.PointToClipAngle(v1))) return;
  }

  side_t *sidedef = seg->sidedef;
  line_t *linedef = seg->linedef;

  if (!seg->backsector) {
    sp = dseg->mid;
    sp->texinfo.ColourMap = ColourMap;
    VTexture *MTex = GTextureManager(sidedef->MidTexture);
    if (FASI(sp->frontTopDist) != FASI(r_ceiling->dist) ||
        FASI(sp->frontBotDist) != FASI(r_floor->dist) ||
        sp->texinfo.Tex->SScale != MTex->SScale ||
        sp->texinfo.Tex->TScale != MTex->TScale ||
        sp->texinfo.Tex->GetHeight() != MTex->GetHeight())
    {
      float topz1 = r_ceiling->GetPointZ(*seg->v1);
      float topz2 = r_ceiling->GetPointZ(*seg->v2);
      float botz1 = r_floor->GetPointZ(*seg->v1);
      float botz2 = r_floor->GetPointZ(*seg->v2);

      FreeWSurfs(sp->surfs);
      sp->surfs = nullptr;

      if (linedef->flags&ML_DONTPEGBOTTOM) {
        // bottom of texture at bottom
        sp->texinfo.toffs = r_floor->TexZ+MTex->GetScaledHeight();
      } else if (linedef->flags&ML_DONTPEGTOP) {
        // top of texture at top of top region
        sp->texinfo.toffs = r_surf_sub->sector->topregion->ceiling->TexZ;
      } else {
        // top of texture at top
        sp->texinfo.toffs = r_ceiling->TexZ;
      }
      sp->texinfo.toffs *= TextureTScale(MTex);
      sp->texinfo.toffs += sidedef->MidRowOffset*TextureOffsetTScale(MTex);
      sp->texinfo.Tex = MTex;
      sp->texinfo.noDecals = (MTex ? MTex->noDecals : true);

      wv[0].x = wv[1].x = seg->v1->x;
      wv[0].y = wv[1].y = seg->v1->y;
      wv[2].x = wv[3].x = seg->v2->x;
      wv[2].y = wv[3].y = seg->v2->y;

      wv[0].z = botz1;
      wv[1].z = topz1;
      wv[2].z = topz2;
      wv[3].z = botz2;

      sp->surfs = CreateWSurfs(wv, &sp->texinfo, seg, r_surf_sub);

      sp->frontTopDist = r_ceiling->dist;
      sp->frontBotDist = r_floor->dist;
      sp->RowOffset = sidedef->MidRowOffset;
    } else if (FASI(sp->RowOffset) != FASI(sidedef->MidRowOffset)) {
      sp->texinfo.Tex = MTex;
      sp->texinfo.noDecals = (MTex ? MTex->noDecals : true);
      UpdateRowOffset(sp, sidedef->MidRowOffset);
    } else {
      sp->texinfo.Tex = MTex;
      sp->texinfo.noDecals = (MTex ? MTex->noDecals : true);
    }

    if (FASI(sp->TextureOffset) != FASI(sidedef->MidTextureOffset)) {
      UpdateTextureOffset(sp, sidedef->MidTextureOffset);
    }

    sp = dseg->topsky;
    sp->texinfo.ColourMap = ColourMap;
    if (IsSky(r_ceiling) && FASI(sp->frontTopDist) != FASI(r_ceiling->dist)) {
      float topz1 = r_ceiling->GetPointZ(*seg->v1);
      float topz2 = r_ceiling->GetPointZ(*seg->v2);

      FreeWSurfs(sp->surfs);
      sp->surfs = nullptr;

      wv[0].x = wv[1].x = seg->v1->x;
      wv[0].y = wv[1].y = seg->v1->y;
      wv[2].x = wv[3].x = seg->v2->x;
      wv[2].y = wv[3].y = seg->v2->y;

      wv[0].z = topz1;
      wv[1].z = wv[2].z = skyheight;
      wv[3].z = topz2;

      sp->surfs = CreateWSurfs(wv, &sp->texinfo, seg, r_surf_sub);

      sp->frontTopDist = r_ceiling->dist;
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
    if (IsSky(r_ceiling) && IsSky(back_ceiling) && r_ceiling->SkyBox != back_ceiling->SkyBox) {
      TTex = GTextureManager[skyflatnum];
    }
    if (FASI(sp->frontTopDist) != FASI(r_ceiling->dist) ||
        FASI(sp->frontBotDist) != FASI(r_floor->dist) ||
        FASI(sp->backTopDist) != FASI(back_ceiling->dist) ||
        sp->texinfo.Tex->SScale != TTex->SScale ||
        sp->texinfo.Tex->TScale != TTex->TScale)
    {
      float topz1 = r_ceiling->GetPointZ(*seg->v1);
      float topz2 = r_ceiling->GetPointZ(*seg->v2);
      float botz1 = r_floor->GetPointZ(*seg->v1);
      float botz2 = r_floor->GetPointZ(*seg->v2);

      float back_topz1 = back_ceiling->GetPointZ(*seg->v1);
      float back_topz2 = back_ceiling->GetPointZ(*seg->v2);

      // hack to allow height changes in outdoor areas
      float top_topz1 = topz1;
      float top_topz2 = topz2;
      float top_TexZ = r_ceiling->TexZ;
      if (IsSky(r_ceiling) && IsSky(back_ceiling) && r_ceiling->SkyBox == back_ceiling->SkyBox) {
        top_topz1 = back_topz1;
        top_topz2 = back_topz2;
        top_TexZ = back_ceiling->TexZ;
      }

      FreeWSurfs(sp->surfs);
      sp->surfs = nullptr;

      if (linedef->flags&ML_DONTPEGTOP) {
        // top of texture at top
        sp->texinfo.toffs = top_TexZ;
      } else {
        // bottom of texture
        sp->texinfo.toffs = back_ceiling->TexZ+TTex->GetScaledHeight();
      }
      sp->texinfo.toffs *= TextureTScale(TTex);
      sp->texinfo.toffs += sidedef->TopRowOffset*TextureOffsetTScale(TTex);
      sp->texinfo.Tex = TTex;
      sp->texinfo.noDecals = (TTex ? TTex->noDecals : true);

      wv[0].x = wv[1].x = seg->v1->x;
      wv[0].y = wv[1].y = seg->v1->y;
      wv[2].x = wv[3].x = seg->v2->x;
      wv[2].y = wv[3].y = seg->v2->y;

      wv[0].z = MAX(back_topz1, botz1);
      wv[1].z = top_topz1;
      wv[2].z = top_topz2;
      wv[3].z = MAX(back_topz2, botz2);

      sp->surfs = CreateWSurfs(wv, &sp->texinfo, seg, r_surf_sub);

      sp->frontTopDist = r_ceiling->dist;
      sp->frontBotDist = r_floor->dist;
      sp->backTopDist = back_ceiling->dist;
      sp->RowOffset = sidedef->TopRowOffset;
    } else if (FASI(sp->RowOffset) != FASI(sidedef->TopRowOffset)) {
      sp->texinfo.Tex = TTex;
      sp->texinfo.noDecals = (TTex ? TTex->noDecals : true);
      UpdateRowOffset(sp, sidedef->TopRowOffset);
    } else {
      sp->texinfo.Tex = TTex;
      sp->texinfo.noDecals = (TTex ? TTex->noDecals : true);
    }

    if (FASI(sp->TextureOffset) != FASI(sidedef->TopTextureOffset)) {
      UpdateTextureOffset(sp, sidedef->TopTextureOffset);
    }

    // sky above top
    sp = dseg->topsky;
    sp->texinfo.ColourMap = ColourMap;
    if (IsSky(r_ceiling) && !IsSky(back_ceiling) && FASI(sp->frontTopDist) != FASI(r_ceiling->dist)) {
      float topz1 = r_ceiling->GetPointZ(*seg->v1);
      float topz2 = r_ceiling->GetPointZ(*seg->v2);

      FreeWSurfs(sp->surfs);
      sp->surfs = nullptr;

      sp->texinfo.Tex = GTextureManager[skyflatnum];
      sp->texinfo.noDecals = (sp->texinfo.Tex ? sp->texinfo.Tex->noDecals : true);
      sp->texinfo.Alpha = 1.1;
      sp->texinfo.Additive = false;
      sp->texinfo.ColourMap = 0;

      wv[0].x = wv[1].x = seg->v1->x;
      wv[0].y = wv[1].y = seg->v1->y;
      wv[2].x = wv[3].x = seg->v2->x;
      wv[2].y = wv[3].y = seg->v2->y;

      wv[0].z = topz1;
      wv[1].z = wv[2].z = skyheight;
      wv[3].z = topz2;

      sp->surfs = CreateWSurfs(wv, &sp->texinfo, seg, r_surf_sub);

      sp->frontTopDist = r_ceiling->dist;
    }

    // bottom wall
    sp = dseg->bot;
    sp->texinfo.ColourMap = ColourMap;
    VTexture *BTex = GTextureManager(sidedef->BottomTexture);
    sp->texinfo.Tex = BTex;
    sp->texinfo.noDecals = (sp->texinfo.Tex ? sp->texinfo.Tex->noDecals : true);
    if (FASI(sp->frontTopDist) != FASI(r_ceiling->dist) ||
        FASI(sp->frontBotDist) != FASI(r_floor->dist) ||
        FASI(sp->backBotDist) != FASI(back_floor->dist))
    {
      float topz1 = r_ceiling->GetPointZ(*seg->v1);
      float topz2 = r_ceiling->GetPointZ(*seg->v2);
      float botz1 = r_floor->GetPointZ(*seg->v1);
      float botz2 = r_floor->GetPointZ(*seg->v2);
      float top_TexZ = r_ceiling->TexZ;

      float back_botz1 = back_floor->GetPointZ(*seg->v1);
      float back_botz2 = back_floor->GetPointZ(*seg->v2);

      // hack to allow height changes in outdoor areas
      if (IsSky(r_ceiling) && IsSky(back_ceiling)) {
        topz1 = back_ceiling->GetPointZ(*seg->v1);
        topz2 = back_ceiling->GetPointZ(*seg->v2);
        top_TexZ = back_ceiling->TexZ;
      }

      FreeWSurfs(sp->surfs);
      sp->surfs = nullptr;

      if (linedef->flags&ML_DONTPEGBOTTOM) {
        // bottom of texture at bottom
        // top of texture at top
        sp->texinfo.toffs = top_TexZ;
      } else {
        // top of texture at top
        sp->texinfo.toffs = back_floor->TexZ;
      }
      sp->texinfo.toffs *= TextureTScale(BTex);
      sp->texinfo.toffs += sidedef->BotRowOffset*TextureOffsetTScale(BTex);

      wv[0].x = wv[1].x = seg->v1->x;
      wv[0].y = wv[1].y = seg->v1->y;
      wv[2].x = wv[3].x = seg->v2->x;
      wv[2].y = wv[3].y = seg->v2->y;

      wv[0].z = botz1;
      wv[1].z = MIN(back_botz1, topz1);
      wv[2].z = MIN(back_botz2, topz2);
      wv[3].z = botz2;

      sp->surfs = CreateWSurfs(wv, &sp->texinfo, seg, r_surf_sub);

      sp->frontTopDist = r_ceiling->dist;
      sp->frontBotDist = r_floor->dist;
      sp->backBotDist = back_floor->dist;
      sp->RowOffset = sidedef->BotRowOffset;
    } else if (FASI(sp->RowOffset) != FASI(sidedef->BotRowOffset)) {
      UpdateRowOffset(sp, sidedef->BotRowOffset);
    }

    if (FASI(sp->TextureOffset) != FASI(sidedef->BotTextureOffset)) {
      UpdateTextureOffset(sp, sidedef->BotTextureOffset);
    }

    // masked MidTexture
    sp = dseg->mid;
    sp->texinfo.ColourMap = ColourMap;
    VTexture *MTex = GTextureManager(sidedef->MidTexture);
    if (FASI(sp->frontTopDist) != FASI(r_ceiling->dist) ||
        FASI(sp->frontBotDist) != FASI(r_floor->dist) ||
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
      if (sidedef->MidTexture) {
        float topz1 = r_ceiling->GetPointZ(*seg->v1);
        float topz2 = r_ceiling->GetPointZ(*seg->v2);
        float botz1 = r_floor->GetPointZ(*seg->v1);
        float botz2 = r_floor->GetPointZ(*seg->v2);

        float back_topz1 = back_ceiling->GetPointZ(*seg->v1);
        float back_topz2 = back_ceiling->GetPointZ(*seg->v2);
        float back_botz1 = back_floor->GetPointZ(*seg->v1);
        float back_botz2 = back_floor->GetPointZ(*seg->v2);

        float midtopz1 = topz1;
        float midtopz2 = topz2;
        float midbotz1 = botz1;
        float midbotz2 = botz2;
        if (topz1 > back_topz1 && sidedef->TopTexture > 0) {
          midtopz1 = back_topz1;
          midtopz2 = back_topz2;
        }
        if (botz1 < back_botz1 && sidedef->BottomTexture > 0) {
          midbotz1 = back_botz1;
          midbotz2 = back_botz2;
        }

        float texh = MTex->GetScaledHeight();
        float z_org;

        TVec segdir = (*seg->v2-*seg->v1)/seg->length;

        sp->texinfo.saxis = segdir*TextureSScale(MTex);
        sp->texinfo.taxis = TVec(0, 0, -1)*TextureTScale(MTex);
        sp->texinfo.soffs = -DotProduct(*seg->v1, sp->texinfo.saxis)+seg->offset*TextureSScale(MTex)+sidedef->MidTextureOffset*TextureOffsetSScale(MTex);
        sp->texinfo.Alpha = linedef->alpha;
        sp->texinfo.Additive = !!(linedef->flags&ML_ADDITIVE);

        if (linedef->flags&ML_DONTPEGBOTTOM) {
          // bottom of texture at bottom
          // top of texture at top
          z_org = MAX(seg->frontsector->floor.TexZ, seg->backsector->floor.TexZ)+texh;
        } else {
          // top of texture at top
          z_org = MIN(seg->frontsector->ceiling.TexZ, seg->backsector->ceiling.TexZ);
        }
        z_org += sidedef->MidRowOffset*(!MTex->bWorldPanning ? 1.0 : 1.0/MTex->TScale);

        sp->texinfo.toffs = z_org*TextureTScale(MTex);

        wv[0].x = wv[1].x = seg->v1->x;
        wv[0].y = wv[1].y = seg->v1->y;
        wv[2].x = wv[3].x = seg->v2->x;
        wv[2].y = wv[3].y = seg->v2->y;

        if (linedef->flags&ML_WRAP_MIDTEX)
        {
          wv[0].z = MAX(midbotz1, z_org-texh);
          wv[1].z = MIN(midtopz1, z_org);
          wv[2].z = MIN(midtopz2, z_org);
          wv[3].z = MAX(midbotz2, z_org-texh);
          /*
          wv[0].z = midbotz1;
          wv[1].z = midtopz1;
          wv[2].z = midtopz2;
          wv[3].z = midbotz2;
          */
        } else {
          wv[0].z = MAX(midbotz1, z_org-texh);
          wv[1].z = MIN(midtopz1, z_org);
          wv[2].z = MIN(midtopz2, z_org);
          wv[3].z = MAX(midbotz2, z_org-texh);
        }

        sp->surfs = CreateWSurfs(wv, &sp->texinfo, seg, r_surf_sub);
      } else {
        sp->texinfo.Alpha = 1.1;
        sp->texinfo.Additive = false;
      }

      sp->frontTopDist = r_ceiling->dist;
      sp->frontBotDist = r_floor->dist;
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
      UpdateTextureOffset(sp, sidedef->MidTextureOffset);
    }

    sec_region_t *reg;
    segpart_t *spp = dseg->extra;
    for (reg = seg->backsector->botregion; reg->next; reg = reg->next) {
      sec_plane_t *extratop = reg->next->floor;
      sec_plane_t *extrabot = reg->ceiling;
      side_t *extraside = &Level->Sides[reg->extraline->sidenum[0]];

      spp->texinfo.ColourMap = ColourMap;
      VTexture *ETex = GTextureManager(extraside->MidTexture);
      spp->texinfo.Tex = ETex;
      spp->texinfo.noDecals = (sp->texinfo.Tex ? sp->texinfo.Tex->noDecals : true);
      if (FASI(spp->frontTopDist) != FASI(r_ceiling->dist) ||
          FASI(spp->frontBotDist) != FASI(r_floor->dist) ||
          FASI(spp->backTopDist) != FASI(extratop->dist) ||
          FASI(spp->backBotDist) != FASI(extrabot->dist))
      {
        float topz1 = r_ceiling->GetPointZ(*seg->v1);
        float topz2 = r_ceiling->GetPointZ(*seg->v2);
        float botz1 = r_floor->GetPointZ(*seg->v1);
        float botz2 = r_floor->GetPointZ(*seg->v2);

        float extratopz1 = extratop->GetPointZ(*seg->v1);
        float extratopz2 = extratop->GetPointZ(*seg->v2);
        float extrabotz1 = extrabot->GetPointZ(*seg->v1);
        float extrabotz2 = extrabot->GetPointZ(*seg->v2);

        FreeWSurfs(spp->surfs);
        spp->surfs = nullptr;

        spp->texinfo.toffs = extratop->TexZ*TextureTScale(ETex)+sidedef->MidRowOffset*TextureOffsetTScale(ETex);

        wv[0].x = wv[1].x = seg->v1->x;
        wv[0].y = wv[1].y = seg->v1->y;
        wv[2].x = wv[3].x = seg->v2->x;
        wv[2].y = wv[3].y = seg->v2->y;

        wv[0].z = MAX(extrabotz1, botz1);
        wv[1].z = MIN(extratopz1, topz1);
        wv[2].z = MIN(extratopz2, topz2);
        wv[3].z = MAX(extrabotz2, botz2);

        spp->surfs = CreateWSurfs(wv, &spp->texinfo, seg, r_surf_sub);

        spp->frontTopDist = r_ceiling->dist;
        spp->frontBotDist = r_floor->dist;
        spp->backTopDist = extratop->dist;
        spp->backBotDist = extrabot->dist;
        spp->RowOffset = sidedef->MidRowOffset;
      } else if (FASI(spp->RowOffset) != FASI(sidedef->MidRowOffset)) {
        UpdateRowOffset(spp, sidedef->MidRowOffset);
      }

      if (FASI(spp->TextureOffset) != FASI(sidedef->MidTextureOffset)) {
        UpdateTextureOffset(spp, sidedef->MidTextureOffset);
      }
      spp = spp->next;
    }
  }
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::SegMoved
//
//==========================================================================
void VRenderLevelShared::SegMoved (seg_t *seg) {
  guard(VRenderLevelShared::SegMoved);
  if (!seg->drawsegs) return; // drawsegs not created yet
  if (!seg->linedef) Sys_Error("R_SegMoved: miniseg");

  VTexture *Tex = seg->drawsegs->mid->texinfo.Tex;
  seg->drawsegs->mid->texinfo.saxis = (*seg->v2-*seg->v1)/seg->length*TextureSScale(Tex);
  seg->drawsegs->mid->texinfo.soffs = -DotProduct(*seg->v1, seg->drawsegs->mid->texinfo.saxis)+seg->offset*TextureSScale(Tex)+seg->sidedef->MidTextureOffset*TextureOffsetSScale(Tex);

  // force update
  seg->drawsegs->mid->frontTopDist += 0.346;
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::CreateWorldSurfaces
//
//==========================================================================
void VRenderLevelShared::CreateWorldSurfaces () {
  guard(VRenderLevelShared::CreateWorldSurfaces);
  int count, dscount, spcount;
  subregion_t *sreg;
  subsector_t *sub;
  drawseg_t *pds;
  sec_region_t *reg;

  free_wsurfs = nullptr;
  SetupSky();

  // set up fake floors
  for (int i = 0; i < Level->NumSectors; ++i) {
    if (Level->Sectors[i].heightsec || Level->Sectors[i].deepref) {
      SetupFakeFloors(&Level->Sectors[i]);
    }
  }

  if (showCreateWorldSurfProgress) {
    R_LdrMsgShow("CALCULATING LIGHTS...");
    R_PBarReset();
  }

  // count regions in all subsectors
  count = 0;
  dscount = 0;
  spcount = 0;
  for (int i = 0; i < Level->NumSubsectors; ++i) {
    sub = &Level->Subsectors[i];
    if (!sub->sector->linecount) continue; // skip sectors containing original polyobjs
    for (reg = sub->sector->botregion; reg; reg = reg->next) {
      ++count;
      dscount += sub->numlines;
      if (sub->poly) dscount += sub->poly->numsegs; // polyobj
      for (int j = 0; j < sub->numlines; ++j) spcount += CountSegParts(&Level->Segs[sub->firstline+j]);
      if (sub->poly) {
        int polyCount = sub->poly->numsegs;
        seg_t **polySeg = sub->poly->segs;
        while (polyCount--) {
          spcount += CountSegParts(*polySeg);
          ++polySeg;
        }
      }
    }
  }

  // get some memory
  sreg = new subregion_t[count+1];
  pds = new drawseg_t[dscount+1];
  pspart = new segpart_t[spcount+1];
  memset((void *)sreg, 0, sizeof(subregion_t)*(count+1));
  memset((void *)pds, 0, sizeof(drawseg_t)*(dscount+1));
  memset((void *)pspart, 0, sizeof(segpart_t)*(spcount+1));
  AllocatedSubRegions = sreg;
  AllocatedDrawSegs = pds;
  AllocatedSegParts = pspart;

  // add dplanes
  for (int i = 0; i < Level->NumSubsectors; ++i) {
    if (!(i&7)) CL_KeepaliveMessage();
    sub = &Level->Subsectors[i];
    if (!sub->sector->linecount) continue; // skip sectors containing original polyobjs
    r_surf_sub = sub;
    for (reg = sub->sector->botregion; reg; reg = reg->next) {
      r_floor = reg->floor;
      r_ceiling = reg->ceiling;

      if (sub->sector->fakefloors) {
        if (r_floor == &sub->sector->floor) r_floor = &sub->sector->fakefloors->floorplane;
        if (r_ceiling == &sub->sector->ceiling) r_ceiling = &sub->sector->fakefloors->ceilplane;
      }

      sreg->secregion = reg;
      sreg->floorplane = r_floor;
      sreg->ceilplane = r_ceiling;
      sreg->floor = CreateSecSurface(sub, r_floor);
      sreg->ceil = CreateSecSurface(sub, r_ceiling);

      sreg->count = sub->numlines;
      if (sub->poly) sreg->count += sub->poly->numsegs; // polyobj
      sreg->lines = pds;
      pds += sreg->count;
      for (int j = 0; j < sub->numlines; ++j) CreateSegParts(&sreg->lines[j], &Level->Segs[sub->firstline+j]);
      if (sub->poly) {
        // polyobj
        int j = sub->numlines;
        int polyCount = sub->poly->numsegs;
        seg_t **polySeg = sub->poly->segs;
        while (polyCount--) {
          CreateSegParts(&sreg->lines[j], *polySeg);
          ++polySeg;
          ++j;
        }
      }

      sreg->next = sub->regions;
      sub->regions = sreg;
      ++sreg;
    }

    if (showCreateWorldSurfProgress) R_PBarUpdate("Lighting", i, Level->NumSubsectors);
  }
  if (showCreateWorldSurfProgress) R_PBarUpdate("Lighting", Level->NumSubsectors, Level->NumSubsectors, true);
  showCreateWorldSurfProgress = false;

  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateSubRegion
//
//==========================================================================
void VRenderLevelShared::UpdateSubRegion (subregion_t *region, bool ClipSegs) {
  guard(VRenderLevelShared::UpdateSubRegion);
  int count;
  seg_t **polySeg;

  r_floor = region->floorplane;
  r_ceiling = region->ceilplane;
  if (r_surf_sub->sector->fakefloors) {
    if (r_floor == &r_surf_sub->sector->floor) r_floor = &r_surf_sub->sector->fakefloors->floorplane;
    if (r_ceiling == &r_surf_sub->sector->ceiling) r_ceiling = &r_surf_sub->sector->fakefloors->ceilplane;
  }

  count = r_surf_sub->numlines;
  drawseg_t *ds = region->lines;
  while (count--) {
    UpdateDrawSeg(ds, ClipSegs);
    ++ds;
  }

  UpdateSecSurface(region->floor, region->floorplane, r_surf_sub);
  UpdateSecSurface(region->ceil, region->ceilplane, r_surf_sub);

  if (r_surf_sub->poly) {
    // update the polyobj
    int polyCount = r_surf_sub->poly->numsegs;
    polySeg = r_surf_sub->poly->segs;
    while (polyCount--) {
      UpdateDrawSeg((*polySeg)->drawsegs, ClipSegs);
      ++polySeg;
    }
  }

  if (region->next) {
    if (ClipSegs) {
      if (!ViewClip.ClipCheckRegion(region->next, r_surf_sub, false)) return;
    }
    UpdateSubRegion(region->next, ClipSegs);
  }
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::CopyPlaneIfValid
//
//==========================================================================
bool VRenderLevelShared::CopyPlaneIfValid (sec_plane_t *dest, const sec_plane_t *source, const sec_plane_t *opp) {
  guard(VRenderLevelShared::CopyPlaneIfValid);
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
  unguard;
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
  guard(VRenderLevelShared::UpdateFakeFlats);
  const sector_t *s = sec->heightsec;
  sector_t *heightsec = r_viewleaf->sector->heightsec;
  bool underwater = /*r_fakingunderwater ||*/
    //(heightsec && vieworg.z <= heightsec->floor.GetPointZ(vieworg));
    (s && vieworg.z <= s->floor.GetPointZ(vieworg));
  bool doorunderwater = false;
  bool diffTex = !!(s && s->SectorFlags&sector_t::SF_ClipFakePlanes);

  // replace sector being drawn with a copy to be hacked
  fakefloor_t *ff = sec->fakefloors;
  ff->floorplane = sec->floor;
  ff->ceilplane = sec->ceiling;
  ff->params = sec->params;

  // replace floor and ceiling height with control sector's heights
  if (diffTex) {
    if (CopyPlaneIfValid(&ff->floorplane, &s->floor, &sec->ceiling)) {
      ff->floorplane.pic = s->floor.pic;
    } else if (s && s->SectorFlags&sector_t::SF_FakeFloorOnly) {
      if (underwater) {
        //tempsec->ColourMap = s->ColourMap;
        ff->params.Fade = s->params.Fade;
        if (!(s->SectorFlags&sector_t::SF_NoFakeLight)) {
          ff->params.lightlevel = s->params.lightlevel;
          ff->params.LightColour = s->params.LightColour;
          /*
          if (floorlightlevel != nullptr) *floorlightlevel = GetFloorLight (s);
          if (ceilinglightlevel != nullptr) *ceilinglightlevel = GetFloorLight (s);
          */
        }
        return;
      }
      return;
    }
  } else {
    if (s && !(s->SectorFlags&sector_t::SF_FakeCeilingOnly)) {
      ff->floorplane.normal = s->floor.normal;
      ff->floorplane.dist = s->floor.dist;
    }
  }

  if (s && !(s->SectorFlags&sector_t::SF_FakeFloorOnly)) {
    if (diffTex) {
      if (CopyPlaneIfValid(&ff->ceilplane, &s->ceiling, &sec->floor)) {
        ff->ceilplane.pic = s->ceiling.pic;
      }
    } else {
      ff->ceilplane.normal = s->ceiling.normal;
      ff->ceilplane.dist = s->ceiling.dist;
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
    ff->floorplane.normal = sec->floor.normal;
    ff->floorplane.dist = sec->floor.dist;
    ff->ceilplane.normal = -s->floor.normal;
    ff->ceilplane.dist = -s->floor.dist/* - -s->floor.normal.z*/;
    //ff->ColourMap = s->ColourMap;
    ff->params.Fade = s->params.Fade;
  }

  // killough 11/98: prevent sudden light changes from non-water sectors:
  if ((underwater /*&& !back*/) || doorunderwater ||
      (heightsec && vieworg.z <= heightsec->floor.GetPointZ(vieworg))
     )
  {
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
  } else if (((s && vieworg.z > s->floor.GetPointZ(vieworg)) ||
              (heightsec && vieworg.z > heightsec->ceiling.GetPointZ(vieworg))) &&
             orgceilz > refceilz && !(s->SectorFlags&sector_t::SF_FakeFloorOnly))
  {
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
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateDeepWater
//
//==========================================================================
void VRenderLevelShared::UpdateDeepWater (sector_t *sec) {
  guard(VRenderLevelShared::UpdateFakeFlats);

  if (!sec) return; // just in case
  const sector_t *s = sec->deepref;

  if (!s) return; // just in case

  // replace sector being drawn with a copy to be hacked
  fakefloor_t *ff = sec->fakefloors;
  ff->floorplane = sec->floor;
  ff->ceilplane = sec->ceiling;
  ff->params = sec->params;

  ff->floorplane.normal = s->floor.normal;
  ff->floorplane.dist = s->floor.dist;

  //sec->heightsec = sec->deepref;

  unguard;
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
  }
  if (sec->othersecCeiling && sec->ceiling.minz > sec->othersecCeiling->ceiling.minz) {
    ff->ceilplane = sec->othersecCeiling->ceiling;
    ff->params = sec->othersecCeiling->params;
  }
}


//==========================================================================
//
//  VRenderLevelShared::SetupFakeFloors
//
//==========================================================================
void VRenderLevelShared::SetupFakeFloors (sector_t *Sec) {
  guard(VRenderLevelShared::SetupFakeFloors);

  if (!Sec->deepref) {
    sector_t *HeightSec = Sec->heightsec;
    if (HeightSec->SectorFlags&sector_t::SF_IgnoreHeightSec) return;
  }

  Sec->fakefloors = new fakefloor_t;
  memset((void *)Sec->fakefloors, 0, sizeof(fakefloor_t));
  Sec->fakefloors->floorplane = Sec->floor;
  Sec->fakefloors->ceilplane = Sec->ceiling;
  Sec->fakefloors->params = Sec->params;

  Sec->topregion->params = &Sec->fakefloors->params;
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::FreeSurfaces
//
//==========================================================================
void VRenderLevelShared::FreeSurfaces (surface_t *InSurf) {
  guard(VRenderLevelShared::FreeSurfaces);
  surface_t *next;
  for (surface_t *s = InSurf; s; s = next) {
    if (s->CacheSurf) FreeSurfCache(s->CacheSurf);
    if (s->lightmap) Z_Free(s->lightmap);
    if (s->lightmap_rgb) Z_Free(s->lightmap_rgb);
    next = s->next;
    Z_Free(s);
  }
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::FreeSegParts
//
//==========================================================================
void VRenderLevelShared::FreeSegParts (segpart_t *ASP) {
  guard(VRenderLevelShared::FreeSegParts);
  for (segpart_t *sp = ASP; sp; sp = sp->next) FreeWSurfs(sp->surfs);
  unguard;
}
