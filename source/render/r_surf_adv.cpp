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

//==========================================================================
//
//  BinHeapLess
//
//  it has to be here, sorry
//
//==========================================================================
static inline bool BinHeapLess (float a, float b) noexcept {
  return (a < b);
}


#include "../gamedefs.h"
#include "r_local.h"


//#define VV_TJUNCTION_VERBOSE

#ifdef VV_TJUNCTION_VERBOSE
# define TJLOG(...)  if (dbg_fix_tjunctions) GCon->Logf(__VA_ARGS__)
#else
# define TJLOG(...)  (void)0
#endif


//==========================================================================
//
//  VRenderLevelShadowVolume::InitSurfs
//
//==========================================================================
void VRenderLevelShadowVolume::InitSurfs (bool recalcStaticLightmaps, surface_t *surfs, texinfo_t *texinfo, const TPlane *plane, subsector_t *sub) {
  if (!texinfo && !plane) return;
  for (; surfs; surfs = surfs->next) {
    if (texinfo) surfs->texinfo = texinfo;
    if (plane) surfs->plane = *plane;
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::SubdivideFace
//
//  this is used to subdivide flags (floors and ceilings)
//  axes are two lightmap axes
//
//==========================================================================
surface_t *VRenderLevelShadowVolume::SubdivideFace (surface_t *surf, const TVec &axis, const TVec *nextaxis) {
  // advanced renderer can draw whole surface
  return surf;
}


//==========================================================================
//
//  VRenderLevelShadowVolume::SubdivideSeg
//
//==========================================================================
surface_t *VRenderLevelShadowVolume::SubdivideSeg (surface_t *surf, const TVec &axis, const TVec *nextaxis, seg_t *seg) {
  // advanced renderer can draw whole surface
  return surf;
}


//==========================================================================
//
//  VRenderLevelShadowVolume::PreRender
//
//==========================================================================
void VRenderLevelShadowVolume::PreRender () {
  inWorldCreation = true;
  RegisterAllThinkers();
  CreateWorldSurfaces();
  inWorldCreation = false;
}


//==========================================================================
//
//  VRenderLevelShadowVolume::FixFaceTJunctions
//
//==========================================================================
surface_t *VRenderLevelShadowVolume::FixFaceTJunctions (surface_t *surf) {
  // not yet
  return surf;
}


//==========================================================================
//
//  VRenderLevelShadowVolume::FixSegTJunctions
//
//  this is used to subdivide wall segments
//  axes are two lightmap axes
//
//  we'll fix t-junctions here
//
//  WARNING! this is temporary solution, we should do it in renderer,
//  because moving flats can invalidate neighbour surfaces
//
//==========================================================================
surface_t *VRenderLevelShadowVolume::FixSegTJunctions (surface_t *surf, seg_t *seg) {
  // wall segment should always be a quad
  if (!lastRenderQuality || surf->count != 4) return surf; // just in case

  const line_t *line = seg->linedef;
  const sector_t *mysec = seg->frontsector;
  if (!line || !mysec) return surf; // just in case

  // invariant, actually
  if (surf->next) {
    GCon->Logf(NAME_Warning, "line #%d, seg #%d: has surface chain", (int)(ptrdiff_t)(line-&Level->Lines[0]), (int)(ptrdiff_t)(seg-&Level->Segs[0]));
    return surf;
  }

  // ignore paper-thin surfaces
  if (surf->verts[0].vec().z == surf->verts[1].vec().z &&
      surf->verts[2].vec().z == surf->verts[3].vec().z)
  {
    TJLOG(NAME_Debug, "line #%d, seg #%d: ignore due to being paper-thin", (int)(ptrdiff_t)(line-&Level->Lines[0]), (int)(ptrdiff_t)(seg-&Level->Segs[0]));
    return surf;
  }

  // good wall quad should consist of two vertical lines
  if (surf->verts[0].vec().x != surf->verts[1].vec().x || surf->verts[0].vec().y != surf->verts[1].vec().y ||
      surf->verts[2].vec().x != surf->verts[3].vec().x || surf->verts[2].vec().y != surf->verts[3].vec().y)
  {
    if (warn_fix_tjunctions) GCon->Logf(NAME_Warning, "line #%d, seg #%d: bad quad (0)", (int)(ptrdiff_t)(line-&Level->Lines[0]), (int)(ptrdiff_t)(seg-&Level->Segs[0]));
    if (warn_fix_tjunctions) {
      GCon->Logf(NAME_Debug, " (%g,%g,%g)-(%g,%g,%g)", seg->v1->x, seg->v1->y, seg->v1->z, seg->v2->x, seg->v2->y, seg->v2->z);
      for (int f = 0; f < surf->count; ++f) GCon->Logf(NAME_Debug, "  %d: (%g,%g,%g)", f, surf->verts[f].vec().x, surf->verts[f].vec().y, surf->verts[f].vec().z);
    }
    return surf;
  }

  //GCon->Logf(NAME_Debug, "*** checking line #%d...", (int)(ptrdiff_t)(line-&Level->Lines[0]));

  float minz[2];
  float maxz[2];
  int v0idx;
       if (fabsf(surf->verts[0].vec().x-seg->v1->x) < 0.1f && fabsf(surf->verts[0].vec().y-seg->v1->y) < 0.1f) v0idx = 0;
  else if (fabsf(surf->verts[0].vec().x-seg->v2->x) < 0.1f && fabsf(surf->verts[0].vec().y-seg->v2->y) < 0.1f) v0idx = 2;
  else {
    if (warn_fix_tjunctions) GCon->Logf(NAME_Warning, "line #%d, seg #%d: bad quad (1)", (int)(ptrdiff_t)(line-&Level->Lines[0]), (int)(ptrdiff_t)(seg-&Level->Segs[0]));
    if (warn_fix_tjunctions) {
      GCon->Logf(NAME_Debug, " (%g,%g,%g)-(%g,%g,%g)", seg->v1->x, seg->v1->y, seg->v1->z, seg->v2->x, seg->v2->y, seg->v2->z);
      for (int f = 0; f < surf->count; ++f) GCon->Logf(NAME_Debug, "  %d: (%g,%g,%g)", f, surf->verts[f].vec().x, surf->verts[f].vec().y, surf->verts[f].vec().z);
    }
    return surf;
  }
  minz[0] = min2(surf->verts[v0idx].vec().z, surf->verts[v0idx+1].vec().z);
  maxz[0] = max2(surf->verts[v0idx].vec().z, surf->verts[v0idx+1].vec().z);
  minz[1] = min2(surf->verts[2-v0idx].vec().z, surf->verts[2-v0idx+1].vec().z);
  maxz[1] = max2(surf->verts[2-v0idx].vec().z, surf->verts[2-v0idx+1].vec().z);

  // the surface will be split to two triangles, so rendering with triangle fans will work with added vertices
  tjunkTri0.resetNoDtor();
  tjunkTri1.resetNoDtor();
  //TJLOG(NAME_Debug, "*** minz=%g; maxz=%g", minz, maxz);
  // for each seg vertex
  for (int vidx = 0; vidx < 2; ++vidx) {
    // do not fix anything for seg vertex that doesn't touch line vertex
    // this is to avoid introducing cracks in the middle of the wall that was splitted by BSP
    int lvidx;
    if (vidx == 0) {
           if (seg->v1->x == line->v1->x && seg->v1->y == line->v1->y) lvidx = 0;
      else if (seg->v1->x == line->v2->x && seg->v1->y == line->v2->y) lvidx = 1;
      else continue;
    } else {
           if (seg->v2->x == line->v1->x && seg->v2->y == line->v1->y) lvidx = 0;
      else if (seg->v2->x == line->v2->x && seg->v2->y == line->v2->y) lvidx = 1;
      else continue;
    }

    // collect all possible height fixes
    const int lvxCount = line->vxCount(lvidx);
    if (!lvxCount) continue;
    const TVec lv = (lvidx ? *line->v2 : *line->v1);
    tjunkHList.resetNoDtor();
    for (int f = 0; f < lvxCount; ++f) {
      const line_t *ln = line->vxLine(lvidx, f);
      if (ln == line) continue;
      //TJLOG(NAME_Debug, "  vidx=%d; other line #%d...", vidx, (int)(ptrdiff_t)(ln-&Level->Lines[0]));
      for (int sn = 0; sn < 2; ++sn) {
        const sector_t *sec = (sn ? ln->backsector : ln->frontsector);
        if (!sec || sec == mysec) continue;
        if (sn && ln->frontsector == ln->backsector) continue; // self-referenced line
        const float fz = sec->floor.GetPointZClamped(lv);
        const float cz = sec->ceiling.GetPointZClamped(lv);
        //TJLOG(NAME_Debug, "  other line #%d: sec=%d; fz=%g; cz=%g", (int)(ptrdiff_t)(ln-&Level->Lines[0]), (int)(ptrdiff_t)(sec-&Level->Sectors[0]), fz, cz);
        if (fz > cz) continue; // just in case
        if (cz <= minz[vidx] || fz >= maxz[vidx]) continue; // no need to introduce any new vertices
        if (fz > minz[vidx]) tjunkHList.push(fz);
        if (cz != fz && cz < maxz[vidx]) tjunkHList.push(cz);
      }
    }
    if (!tjunkHList.length()) continue;

    TJLOG(NAME_Debug, "line #%d, vertex %d: at most %d additional %s", (int)(ptrdiff_t)(line-&Level->Lines[0]), lvidx, tjunkHList.length(), (tjunkHList.length() != 1 ? "vertices" : "vertex"));
    // split quad to two triangles if it wasn't done yet
    if (!tjunkTri1.length()) {
      // invariant: first triangle edge is a vertical line
      vassert(surf->count == 4);
      #ifdef VV_TJUNCTION_VERBOSE
        TJLOG(NAME_Debug, " minz=%g:%g; maxz=%g%g; (%g,%g,%g)-(%g,%g,%g)", minz[0], minz[1], maxz[0], maxz[1], seg->v1->x, seg->v1->y, seg->v1->z, seg->v2->x, seg->v2->y, seg->v2->z);
        for (int f = 0; f < surf->count; ++f) TJLOG(NAME_Debug, "  %d: (%g,%g,%g)", f, surf->verts[f].vec().x, surf->verts[f].vec().y, surf->verts[f].vec().z);
      #endif
      // first triangle
      tjunkTri0.append(surf->verts[(v0idx+0)&3].vec());
      tjunkTri0.append(surf->verts[(v0idx+1)&3].vec());
      tjunkTri0.append(surf->verts[(v0idx+2)&3].vec());
      // second triangle
      tjunkTri1.append(surf->verts[(v0idx+2)&3].vec());
      tjunkTri1.append(surf->verts[(v0idx+3)&3].vec());
      tjunkTri1.append(surf->verts[(v0idx+0)&3].vec());
    }

    // get triangle to fix
    TArray<TVec> &tri = (vidx ? tjunkTri1 : tjunkTri0);

    if (tri.length() == 3 && tri[0].z == tri[1].z) {
      TJLOG(NAME_Debug, "line #%d, seg #%d: ignore side due to being paper-thin", (int)(ptrdiff_t)(line-&Level->Lines[0]), (int)(ptrdiff_t)(seg-&Level->Segs[0]));
      continue;
    }

    // insert vertices
    float prevhz = -FLT_MAX;
    while (tjunkHList.length()) {
      const float hz = tjunkHList.pop();
      if (hz != prevhz) {
        #ifdef VV_TJUNCTION_VERBOSE
        TJLOG(NAME_Debug, " y=%g; minz=%g:%g; maxz=%g:%g; (%g,%g,%g)-(%g,%g,%g)", hz, minz[0], minz[1], maxz[0], maxz[1], seg->v1->x, seg->v1->y, seg->v1->z, seg->v2->x, seg->v2->y, seg->v2->z);
        for (int f = 0; f < tri.length(); ++f) TJLOG(NAME_Debug, "  %d: (%g,%g,%g)", f, tri[f].x, tri[f].y, tri[f].z);
        #endif
        prevhz = hz;
        // find the index to insert new vertex (before)
        int f = 0;
        if (tri[0].z < tri[1].z) {
          while (f < tri.length() && tri[f].z < hz) ++f;
        } else {
          while (f < tri.length() && hz < tri[f].z) ++f;
        }
        if (f < 1) {
          TJLOG(NAME_Debug, "  OOPS! (0)");
          continue;
        }
        if (f >= tri.length()) {
          TJLOG(NAME_Debug, "  OOPS! (1)");
          continue;
        }
        if (tri[0].x != tri[f].x || tri[0].y != tri[f].y) {
          TJLOG(NAME_Debug, "  OOPS! (2); f=%d", f);
          continue;
        }
        tri.Insert(f, TVec(tri[0].x, tri[0].y, hz));
      }
    }
  }

  // create new surfaces
  // starting from the last tri point is guaranteed to create a valid triangle fan
  // but don't bother if no new vertices were added
  if (tjunkTri0.length() > 3 || tjunkTri1.length() > 3) {
    // s0
    surface_t *s0 = NewWSurf(tjunkTri0.length());
    s0->copyRequiredFrom(*surf);
    s0->count = tjunkTri0.length();
    TJLOG(NAME_Debug, " *** tjunkTri0 ***");
    for (int f = 0; f < s0->count; ++f) {
      const int vn = (s0->count-1+f)%s0->count;
      TJLOG(NAME_Debug, "   %d(%d): (%g,%g,%g)", f, vn, tjunkTri0[vn].x, tjunkTri0[vn].y, tjunkTri0[vn].z);
      s0->verts[f].x = tjunkTri0[vn].x;
      s0->verts[f].y = tjunkTri0[vn].y;
      s0->verts[f].z = tjunkTri0[vn].z;
    }
    // s1
    surface_t *s1 = NewWSurf(tjunkTri1.length());
    s1->copyRequiredFrom(*surf);
    s1->count = tjunkTri1.length();
    TJLOG(NAME_Debug, " *** tjunkTri1 ***");
    for (int f = 0; f < s1->count; ++f) {
      const int vn = (s1->count-1+f)%s1->count;
      TJLOG(NAME_Debug, "   %d(%d): (%g,%g,%g)", f, vn, tjunkTri1[vn].x, tjunkTri1[vn].y, tjunkTri1[vn].z);
      s1->verts[f].x = tjunkTri1[vn].x;
      s1->verts[f].y = tjunkTri1[vn].y;
      s1->verts[f].z = tjunkTri1[vn].z;
    }
    // link them
    s0->next = s1;
    FreeWSurfs(surf);
    surf = s0;
    //
    if (dbg_fix_tjunctions.asBool()) GCon->Logf(NAME_Debug, "line #%d, seg #%d: fixed t-junctions", (int)(ptrdiff_t)(line-&Level->Lines[0]), (int)(ptrdiff_t)(seg-&Level->Segs[0]));
  }

  return surf;
}
