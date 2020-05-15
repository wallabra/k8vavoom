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
#include "gamedefs.h"
#include "server/sv_local.h"


static VCvarB r_lmap_texture_check("r_lmap_texture_check", true, "Check textures of two-sided lines?", /*CVAR_Archive|*/CVAR_PreInit);


//**************************************************************************
//
// blockmap light tracing
//
//**************************************************************************
static inline __attribute__((const)) float TextureSScale (const VTexture *pic) { return pic->SScale; }
static inline __attribute__((const)) float TextureTScale (const VTexture *pic) { return pic->TScale; }
static inline __attribute__((const)) float TextureOffsetSScale (const VTexture *pic) { return (pic->bWorldPanning ? pic->SScale : 1.0f); }
static inline __attribute__((const)) float TextureOffsetTScale (const VTexture *pic) { return (pic->bWorldPanning ? pic->TScale : 1.0f); }
static inline __attribute__((const)) float DivByScale (float v, float scale) { return (scale > 0 ? v/scale : v); }


// ////////////////////////////////////////////////////////////////////////// //
struct PlaneHitInfo {
  TVec linestart;
  TVec lineend;
  bool bestIsSky;
  bool wasHit;
  float besthtime;

  inline PlaneHitInfo (const TVec &alinestart, const TVec &alineend) noexcept
    : linestart(alinestart)
    , lineend(alineend)
    , bestIsSky(false)
    , wasHit(false)
    , besthtime(9999.0f)
  {}

  inline TVec getPointAtTime (const float time) const noexcept __attribute__((always_inline)) {
    return linestart+(lineend-linestart)*time;
  }

  inline TVec getHitPoint () const noexcept __attribute__((always_inline)) {
    return linestart+(lineend-linestart)*(wasHit ? besthtime : 0.0f);
  }

  inline void update (const TSecPlaneRef &plane) noexcept {
    const float d1 = plane.DotPointDist(linestart);
    if (d1 < 0.0f) return; // don't shoot back side

    const float d2 = plane.DotPointDist(lineend);
    if (d2 >= 0.0f) return; // didn't hit plane

    // d1/(d1-d2) -- from start
    // d2/(d2-d1) -- from end

    const float time = d1/(d1-d2);
    if (!wasHit || time < besthtime) {
      bestIsSky = (plane.splane->pic == skyflatnum);
      besthtime = time;
    }

    wasHit = true;
  }

  inline void update (sec_plane_t &plane, bool flip=false) noexcept __attribute__((always_inline)) {
    TSecPlaneRef pp(&plane, flip);
    update(pp);
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct LightTraceInfo {
  // the following should be set
  TVec Start;
  TVec End;
  sector_t *StartSector;
  sector_t *EndSector;
  VLevel *Level;
  // the following are working vars, and should not be set
  TVec Delta; // (End-Start)
  TPlane Plane;
  TVec LineStart;
  TVec LineEnd;

  inline void setup (VLevel *alevel, const TVec &org, const TVec &dest, sector_t *sstart, sector_t *send) {
    Level = alevel;
    Start = org;
    End = dest;
    StartSector = (sstart ?: alevel->PointInSubsector(org)->sector);
    EndSector = (send ?: alevel->PointInSubsector(dest)->sector);
  }
};


//==========================================================================
//
//  LightCheckRegions
//
//==========================================================================
static bool LightCheckRegions (const sector_t *sec, const TVec point) {
  for (sec_region_t *reg = sec->eregions->next; reg; reg = reg->next) {
    if (reg->regflags&(sec_region_t::RF_BaseRegion|sec_region_t::RF_OnlyVisual|sec_region_t::RF_NonSolid)) continue;
    // get opening points
    const float fz = reg->efloor.GetPointZClamped(point);
    const float cz = reg->eceiling.GetPointZClamped(point);
    if (fz >= cz) continue; // ignore paper-thin regions
    // if we are inside it, we cannot pass
    if (point.z >= fz && point.z <= cz) return false;
  }
  return true;
}


//==========================================================================
//
//  LightCanPassOpening
//
//  ignore 3d midtex here (for now)
//
//==========================================================================
static bool LightCanPassOpening (const line_t *linedef, const TVec point) {
  if (linedef->sidenum[1] == -1 || !linedef->backsector) return false; // single sided line

  const sector_t *fsec = linedef->frontsector;
  const sector_t *bsec = linedef->backsector;

  if (!fsec || !bsec) return false;

  // check base region first
  const float ffz = fsec->floor.GetPointZClamped(point);
  const float fcz = fsec->ceiling.GetPointZClamped(point);
  const float bfz = bsec->floor.GetPointZClamped(point);
  const float bcz = bsec->ceiling.GetPointZClamped(point);

  const float pfz = max2(ffz, bfz);
  const float pcz = max2(fcz, bcz);

  // TODO: check for transparent doors here
  if (pfz >= pcz) return false;

  if (point.z <= pfz || point.z >= pcz) return false;

  // fast algo for two sectors without 3d floors
  if (!linedef->frontsector->Has3DFloors() &&
      !linedef->backsector->Has3DFloors())
  {
    // no 3d regions, we're ok
    return true;
  }

  // has 3d floors at least on one side, do full-featured search

  // front sector
  if (!LightCheckRegions(fsec, point)) return false;
  // back sector
  if (!LightCheckRegions(bsec, point)) return false;

  // done
  return true;
}


//==========================================================================
//
//  LightCheckPlanes
//
//  returns `true` if no hit was detected
//  sets `trace.LineEnd` if hit was detected
//
//==========================================================================
static bool LightCheckPlanes (LightTraceInfo &trace, sector_t *sector) {
  PlaneHitInfo phi(trace.LineStart, trace.LineEnd);

  // make fake floors and ceilings block view
  sector_t *hs = sector->heightsec;
  if (hs) {
    if (GTextureManager.IsSightBlocking(hs->floor.pic)) phi.update(hs->floor);
    if (GTextureManager.IsSightBlocking(hs->ceiling.pic)) phi.update(hs->ceiling);
  }

  phi.update(sector->floor);
  phi.update(sector->ceiling);

  for (sec_region_t *reg = sector->eregions->next; reg; reg = reg->next) {
    if (reg->regflags&(sec_region_t::RF_BaseRegion|sec_region_t::RF_OnlyVisual|sec_region_t::RF_NonSolid)) continue;
    if ((reg->efloor.splane->flags&sec_region_t::RF_SkipFloorSurf) == 0) {
      if (GTextureManager.IsSightBlocking(reg->efloor.splane->pic)) {
        phi.update(reg->efloor);
      }
    }
    if ((reg->efloor.splane->flags&sec_region_t::RF_SkipCeilSurf) == 0) {
      if (GTextureManager.IsSightBlocking(reg->eceiling.splane->pic)) {
        phi.update(reg->eceiling);
      }
    }
  }

  if (phi.wasHit) trace.LineEnd = phi.getHitPoint();
  return !phi.wasHit;
}


//==========================================================================
//
//  LightTraverse
//
//  returns `false` if blocked
//
//==========================================================================
static bool LightTraverse (LightTraceInfo &trace, const line_t *line, const float frac) {
  const int s1 = line->PointOnSide2(trace.Start);
  sector_t *front = (s1 == 0 || s1 == 2 ? line->frontsector : line->backsector);
  TVec hitpoint = trace.Start+frac*trace.Delta;
  trace.LineEnd = hitpoint;
  if (!LightCheckPlanes(trace, front)) return false;
  trace.LineStart = trace.LineEnd;

  if (line->flags&ML_TWOSIDED) {
    if (LightCanPassOpening(line, hitpoint)) {
      if (line->alpha < 1.0f || (line->flags&ML_ADDITIVE)) return true;

      if (!r_lmap_texture_check) return true;

      // check texture
      int sidenum = line->PointOnSide2(trace.Start);
      if (sidenum == 2) return true; // on a line

      const side_t *sidedef = &trace.Level->Sides[line->sidenum[sidenum]];
      VTexture *MTex = GTextureManager(sidedef->MidTexture);
      if (!MTex || MTex->Type == TEXTYPE_Null) return true;

      //const sector_t *sec = (sidenum ? line->backsector : line->frontsector);

      const bool wrapped = ((line->flags&ML_WRAP_MIDTEX)|(sidedef->Flags&SDF_WRAPMIDTEX));
      if (wrapped && !MTex->isSeeThrough()) return true;

      const TVec taxis = TVec(0, 0, -1)*(TextureTScale(MTex)*sidedef->Mid.ScaleY);
      float toffs;

      float z_org; // texture top
      if (line->flags&ML_DONTPEGBOTTOM) {
        // bottom of texture at bottom
        const float texh = DivByScale(MTex->GetScaledHeight(), sidedef->Mid.ScaleY);
        z_org = max2(line->frontsector->floor.TexZ, line->backsector->floor.TexZ)+texh;
      } else {
        // top of texture at top
        z_org = min2(line->frontsector->ceiling.TexZ, line->backsector->ceiling.TexZ);
      }

      if (wrapped) {
        // it is wrapped, so just slide it
        toffs = sidedef->Mid.RowOffset*TextureOffsetTScale(MTex);
      } else {
        // move origin up/down, as this texture is not wrapped
        z_org += sidedef->Mid.RowOffset*DivByScale(TextureOffsetTScale(MTex), sidedef->Mid.ScaleY);
        // offset is done by origin, so we don't need to offset texture
        toffs = 0.0f;
      }
      toffs += z_org*(TextureTScale(MTex)*sidedef->Mid.ScaleY);

      const int texelT = (int)(DotProduct(hitpoint, taxis)+toffs); // /MTex->GetHeight();
      // check for wrapping
      if (!wrapped && (texelT < 0 || texelT >= MTex->GetHeight())) return true;
      if (!MTex->isSeeThrough()) return true;

      const TVec saxis = line->ndir*(TextureSScale(MTex)*sidedef->Mid.ScaleX);
      const float soffs = -DotProduct(*line->v1, saxis)+sidedef->Mid.TextureOffset*TextureOffsetSScale(MTex);

      const float texelS = (int)(DotProduct(hitpoint, saxis)+soffs)%MTex->GetWidth();

      auto pix = MTex->getPixel(texelS, texelT);
      return (pix.a < 128);
    }
  }

  return false; // stop
}


//==========================================================================
//
//  LightCheckLine
//
//  return `true` if line is not crossed or put into intercept list
//  return `false` to stop checking due to blocking
//
//==========================================================================
static bool LightCheckLine (LightTraceInfo &trace, line_t *ld) {
  if (ld->validcount == validcount) return true;

  ld->validcount = validcount;

  // signed distances from the line points to the trace line plane
  float dot1 = trace.Plane.PointDistance(*ld->v1);
  float dot2 = trace.Plane.PointDistance(*ld->v2);

  // do not use multiplication to check: zero speedup, lost accuracy
  //if (dot1*dot2 >= 0) return true; // line isn't crossed
  if (dot1 < 0.0f && dot2 < 0.0f) return true; // didn't reached back side
  // if the line is parallel to the trace plane, ignore it
  if (dot1 >= 0.0f && dot2 >= 0.0f) return true; // didn't reached front side

  // signed distances from the trace points to the line plane
  dot1 = ld->PointDistance(trace.Start);
  dot2 = ld->PointDistance(trace.End);

  // do not use multiplication to check: zero speedup, lost accuracy
  //if (dot1*dot2 >= 0) return true; // line isn't crossed
  if (dot1 < 0.0f && dot2 < 0.0f) return true; // didn't reached back side
  // if the trace is parallel to the line plane, ignore it
  if (dot1 >= 0.0f && dot2 >= 0.0f) return true; // didn't reached front side

  // try to early out the check
  if (!ld->backsector || !(ld->flags&ML_TWOSIDED)) {
    return false; // stop checking
  }

  // store the line for later intersection testing
  // signed distance
  const float den = DotProduct(ld->normal, trace.Delta);
  if (fabsf(den) < 0.00001f) return true; // wtf?!
  const float num = ld->dist-DotProduct(trace.Start, ld->normal);
  const float frac = num/den;

  // we don't care if our hits are in order, so don't bother collecting them
  return LightTraverse(trace, ld, frac);
}


//==========================================================================
//
//  LightBlockLinesIterator
//
//==========================================================================
static bool LightBlockLinesIterator (LightTraceInfo &trace, int x, int y) {
  int offset = y*trace.Level->BlockMapWidth+x;
  polyblock_t *polyLink = trace.Level->PolyBlockMap[offset];
  while (polyLink) {
    if (polyLink->polyobj) {
      // only check non-empty links
      if (polyLink->polyobj->validcount != validcount) {
        seg_t **segList = polyLink->polyobj->segs;
        for (int i = 0; i < polyLink->polyobj->numsegs; ++i, ++segList) {
          if (!LightCheckLine(trace, (*segList)->linedef)) return false;
        }
        polyLink->polyobj->validcount = validcount;
      }
    }
    polyLink = polyLink->next;
  }

  offset = *(trace.Level->BlockMap+offset);

  for (const vint32 *list = trace.Level->BlockMapLump+offset+1; *list != -1; ++list) {
    if (!LightCheckLine(trace, &trace.Level->Lines[*list])) return false;
  }

  return true; // everything was checked
}


//==========================================================================
//
//  LightPathTraverse
//
//  traces a light ray from `trace.Start` to `trace.End`
//  `trace.StartSector` and `trace.EndSector` must be set
//
//  returns `true` if no obstacle was hit
//  sets `trace.LineEnd` if something was hit
//
//==========================================================================
static bool LightPathTraverse (LightTraceInfo &trace) {
  VBlockMapWalker walker;

  trace.LineStart = trace.Start;
  trace.Delta = trace.End-trace.Start;

  if (fabs(trace.Delta.x) <= 1.0f && fabs(trace.Delta.y) <= 1.0f) {
    // vertical trace; check starting sector planes and get out
    trace.LineEnd = trace.End;
    // point cannot hit anything!
    if (fabsf(trace.Delta.z) <= 1.0f) return false;
    return LightCheckPlanes(trace, trace.StartSector);
  }

  if (walker.start(trace.Level, trace.Start.x, trace.Start.y, trace.End.x, trace.End.y)) {
    trace.Plane.SetPointDirXY(trace.Start, trace.Delta);
    trace.Level->IncrementValidCount();
    int mapx, mapy;
    while (walker.next(mapx, mapy)) {
      if (!LightBlockLinesIterator(trace, mapx, mapy)) return false; // hit found
    }
    // check ending sector planes
    trace.LineEnd = trace.End;
    return LightCheckPlanes(trace, trace.EndSector);
  }

  // out of map, see nothing
  return false;
}


//==========================================================================
//
//  isNotInsideBM
//
//  right edge is not included
//
//==========================================================================
static VVA_CHECKRESULT inline bool isNotInsideBM (const TVec &pos, const VLevel *level) {
  // horizontal check
  const int intx = (int)pos.x;
  const int intbx0 = (int)level->BlockMapOrgX;
  if (intx < intbx0 || intx >= intbx0+level->BlockMapWidth*MAPBLOCKUNITS) return true;
  // vertical checl
  const int inty = (int)pos.y;
  const int intby0 = (int)level->BlockMapOrgY;
  return (inty < intby0 || inty >= intby0+level->BlockMapHeight*MAPBLOCKUNITS);
}


//==========================================================================
//
//  VLevel::CastLightRay
//
//  doesn't check pvs or reject
//
//==========================================================================
bool VLevel::CastLightRay (sector_t *startSector, const TVec &org, const TVec &dest, sector_t *endSector) {
  // if starting or ending point is out of blockmap bounds, don't bother tracing
  // we can get away with this, because nothing can see anything beyound the map extents
  if (isNotInsideBM(org, this)) return false;
  if (isNotInsideBM(dest, this)) return false;

  if (lengthSquared(org-dest) <= 2.0f) return true;

  LightTraceInfo trace;
  trace.setup(this, org, dest, startSector, endSector);
  return LightPathTraverse(trace);
}
