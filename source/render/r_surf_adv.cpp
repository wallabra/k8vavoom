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

//#define VV_TJUNCTION_VERBOSE

#ifdef VV_TJUNCTION_VERBOSE
# define TJLOG(...)  GCon->Logf(__VA_ARGS__)
#else
# define TJLOG(...)  (void)0
#endif


//==========================================================================
//
//  BinHeapLess
//
//==========================================================================
static inline bool BinHeapLess (float a, float b) noexcept {
  return (a < b);
}


#include "../gamedefs.h"
#include "r_local.h"


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
//  this is used to subdivide wall segments
//  axes are two lightmap axes
//
//  we'll fix t-junctions here
//
//  WARNING! this is temporary solution, we should do it in renderer,
//  because moving flats can invalidate neighbour surfaces
//
//==========================================================================
surface_t *VRenderLevelShadowVolume::SubdivideSeg (surface_t *surf, const TVec &axis, const TVec *nextaxis, seg_t *seg) {
  if (surf->count < 3) return surf; // just in case

  const line_t *line = seg->linedef;
  const sector_t *mysec = seg->frontsector;
  if (!line || !mysec) return surf; // just in case

  if (surf->count != 4) {
    // wall segment should always be a quad
    GCon->Logf(NAME_Warning, "line #%d, seg #%d: not a quad (%d)", (int)(ptrdiff_t)(line-&Level->Lines[0]), (int)(ptrdiff_t)(seg-&Level->Segs[0]), surf->count);
    return surf;
  }
  if (surf->next) {
    GCon->Logf(NAME_Warning, "line #%d, seg #%d: has surface chain", (int)(ptrdiff_t)(line-&Level->Lines[0]), (int)(ptrdiff_t)(seg-&Level->Segs[0]));
    return surf;
  }

  //GCon->Logf(NAME_Debug, "*** checking line #%d...", (int)(ptrdiff_t)(line-&Level->Lines[0]));
  // build heights for neighbour sectors
  //TArray<float> hlist;
  TBinHeapNoDtor<float> hlist;
  //TMapNC<const sector_t *, bool> seensec;
  // determine surface miny and maxy
  float minz = surf->verts[0].vec().z;
  float maxz = minz;
  for (int f = 1; f < surf->count; ++f) {
    const float y = surf->verts[f].vec().z;
    minz = min2(minz, y);
    maxz = max2(maxz, y);
  }

  // the surface will be split to two triangles
  TArray<TVec> tri0, tri1;
  //TJLOG(NAME_Debug, "*** minz=%g; maxz=%g", minz, maxz);
  for (int vidx = 0; vidx < 2; ++vidx) {
    const int lvidx = (seg->side ? 1-vidx : vidx);
    if (!line->vxCount(lvidx)) continue;
    const TVec lv = (lvidx ? *line->v2 : *line->v1);
    hlist.resetNoDtor();
    //seensec.reset();
    for (int f = 0; f < line->vxCount(lvidx); ++f) {
      const line_t *ln = line->vxLine(lvidx, f);
      if (ln == line) continue;
      //TJLOG(NAME_Debug, "  other line #%d...", (int)(ptrdiff_t)(ln-&Level->Lines[0]));
      for (int sn = 0; sn < 2; ++sn) {
        const sector_t *sec = (sn ? ln->backsector : ln->frontsector);
        if (!sec || sec == mysec) continue;
        if (sn && ln->frontsector == ln->backsector) continue; // self-referenced line
        //if (seensec.put(sec, true)) continue;
        const float fz = sec->floor.GetPointZClamped(lv);
        const float cz = sec->ceiling.GetPointZClamped(lv);
        //TJLOG(NAME_Debug, "  other line #%d: sec=%d; fz=%g; cz=%g", (int)(ptrdiff_t)(ln-&Level->Lines[0]), (int)(ptrdiff_t)(sec-&Level->Sectors[0]), fz, cz);
        if (fz > cz) continue; // just in case
        if (cz <= minz || fz >= maxz) continue; // no need to introduce any new vertices
        if (fz > minz) hlist.push(fz);
        if (cz != fz && cz < maxz) hlist.push(cz);
      }
    }

    if (hlist.length()) {
      TJLOG(NAME_Debug, "line #%d, vertex %d: at most %d additional %s", (int)(ptrdiff_t)(line-&Level->Lines[0]), lvidx, hlist.length(), (hlist.length() != 1 ? "vertices" : "vertex"));

      /*
        insert points into surface side. as we are rendering with triangle fans, and inserting
        point can create a degenerate side triangle, so we'll need to post-process new surface.
        as our surface is always either a quad, or a triangle, we can split a quad into two
        triangles. this way we'll always be able to find a good starting point for triangle fan.
       */

      if (!tri1.length()) {
        vassert(surf->count == 4);
        #ifdef VV_TJUNCTION_VERBOSE
          TJLOG(NAME_Debug, " minz=%g; maxz=%g; (%g,%g,%g)-(%g,%g,%g)", minz, maxz, seg->v1->x, seg->v1->y, seg->v1->z, seg->v2->x, seg->v2->y, seg->v2->z);
          for (int f = 0; f < surf->count; ++f) TJLOG(NAME_Debug, "  %d: (%g,%g,%g)", f, surf->verts[f].vec().x, surf->verts[f].vec().y, surf->verts[f].vec().z);
        #endif
        if (surf->verts[0].vec().x != surf->verts[1].vec().x || surf->verts[0].vec().y != surf->verts[1].vec().y ||
            surf->verts[2].vec().x != surf->verts[3].vec().x || surf->verts[2].vec().y != surf->verts[3].vec().y)
        {
          GCon->Logf(NAME_Warning, "line #%d, seg #%d: bad quad (0)", (int)(ptrdiff_t)(line-&Level->Lines[0]), (int)(ptrdiff_t)(seg-&Level->Segs[0]));
          return surf;
        }
        if (surf->verts[0].vec().x == seg->v1->x && surf->verts[0].vec().y == seg->v1->y) {
          // first triangle
          tri0.append(surf->verts[0].vec());
          tri0.append(surf->verts[1].vec());
          tri0.append(surf->verts[2].vec());
          // second triangle
          tri1.append(surf->verts[2].vec());
          tri1.append(surf->verts[3].vec());
          tri1.append(surf->verts[0].vec());
        } else if (surf->verts[0].vec().x == seg->v2->x && surf->verts[0].vec().y == seg->v2->y) {
          // second triangle
          tri1.append(surf->verts[0].vec());
          tri1.append(surf->verts[1].vec());
          tri1.append(surf->verts[2].vec());
          // first triangle
          tri0.append(surf->verts[2].vec());
          tri0.append(surf->verts[3].vec());
          tri0.append(surf->verts[0].vec());
        } else {
          //FIXME: this should not happen, but...
          GCon->Logf(NAME_Warning, "line #%d, seg #%d: bad quad (1)", (int)(ptrdiff_t)(line-&Level->Lines[0]), (int)(ptrdiff_t)(seg-&Level->Segs[0]));
          return surf;
        }
      }

      // get triangle to fix
      TArray<TVec> &tri = (vidx ? tri1 : tri0);
      // insert vertices
      float prevhz = -FLT_MAX;
      while (hlist.length()) {
        const float hz = hlist.pop();
        if (hz != prevhz) {
          TJLOG(NAME_Debug, "  y=%g", hz);
          prevhz = hz;
          int f = 0;
          if (tri[0].z < tri[1].z) {
            while (f < tri.length() && tri[f].z < hz) ++f;
          } else {
            while (f < tri.length() && hz < tri[f].z) ++f;
          }
          vassert(f < tri.length());
          tri.Insert(f, TVec(tri[0].x, tri[0].y, hz));
        }
      }
    }
  }

  // create new surfaces
  if (tri0.length()) {
    // s0
    surface_t *s0 = NewWSurf(tri0.length());
    s0->copyRequiredFrom(*surf);
    s0->count = tri0.length();
    TJLOG(NAME_Debug, " *** tri0 ***");
    for (int f = 0; f < s0->count; ++f) {
      const int vn = (s0->count-1+f)%s0->count;
      TJLOG(NAME_Debug, "   %d(%d): (%g,%g,%g)", f, vn, tri0[vn].x, tri0[vn].y, tri0[vn].z);
      s0->verts[f].x = tri0[vn].x;
      s0->verts[f].y = tri0[vn].y;
      s0->verts[f].z = tri0[vn].z;
    }
    // s1
    surface_t *s1 = NewWSurf(tri1.length());
    s1->copyRequiredFrom(*surf);
    s1->count = tri1.length();
    TJLOG(NAME_Debug, " *** tri1 ***");
    for (int f = 0; f < s1->count; ++f) {
      const int vn = (s1->count-1+f)%s1->count;
      TJLOG(NAME_Debug, "   %d(%d): (%g,%g,%g)", f, vn, tri1[vn].x, tri1[vn].y, tri1[vn].z);
      s1->verts[f].x = tri1[vn].x;
      s1->verts[f].y = tri1[vn].y;
      s1->verts[f].z = tri1[vn].z;
    }
    // link them
    s0->next = s1;
    FreeWSurfs(surf);
    surf = s0;
    //
    GCon->Logf(NAME_Debug, "line #%d, seg #%d: fixed t-junctions", (int)(ptrdiff_t)(line-&Level->Lines[0]), (int)(ptrdiff_t)(seg-&Level->Segs[0]));
  }

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
