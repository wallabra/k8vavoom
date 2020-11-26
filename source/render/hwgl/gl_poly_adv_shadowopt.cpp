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
// directly included from "gl_poly_adv.cpp"
//**************************************************************************

#define VVGL_SMART_SHADOW_REJECT

//TODO: re-check and reimplement smart rejects
//      also, "r_shadowvol_optimise_flats" seems to do the same as "gl_smart_reject_svol_flats"
static VCvarB gl_smart_reject_shadows("gl_smart_reject_shadows", false, "Reject some surfaces that cannot possibly produce shadows?", CVAR_Archive);

static VCvarB gl_smart_reject_svol_segs("gl_smart_reject_svol_segs", true, "Reject some surfaces that cannot possibly produce shadows?", CVAR_Archive);
static VCvarB gl_smart_reject_svol_flats("gl_smart_reject_svol_flats", true, "Reject some surfaces that cannot possibly produce shadows?", CVAR_Archive);


//==========================================================================
//
//  CanSurfaceSegCastShadow
//
//==========================================================================
static bool CanSurfaceSegCastShadow (const surface_t *surf, const TVec LightPos, float Radius) {
  if (!gl_smart_reject_svol_segs) return true;

  // solid segs that has no non-solid neighbours cannot cast any shadow
  const seg_t *seg = surf->seg;
  const line_t *ldef = seg->linedef;
  if (!ldef) {
    // miniseg; wutafuck? it should not have any surface!
    GCon->Log(NAME_Error, "miniseg should not have any surfaces!");
    return true;
  }

  // we cannot do anything sane for 3D floors
  const subsector_t *sub = surf->subsector;
  if (!sub) return true;

  const sector_t *sector = sub->sector;
  if (sector->SectorFlags&sector_t::SF_ExtrafloorSource) return true; // sadly, i cannot reject 3D floors yet

  // if this is a two-sided line, don't reject it
  if (ldef->flags&ML_TWOSIDED) {
    /*
    if (!seg->partner) return false; // just in case
    const sector_t *backsec = seg->partner->frontsub->sector;
    vassert(backsec);

    // here we can check if this is top/bottom texture, and if it can cast shadow
    // to check this, see if light can touch surface edge, and consider this seg one-sided, if it isn't

    // calculate coordinates of bottom texture (if any)
    if (surf->typeFlags&surface_t::TF_BOTTOM) {
      // just in case: if back sector floor should be higher that than our floor
      float minz = sector->floor.minz;
      float maxz = backsec->floor.maxz;
      if (maxz <= minz) return false; // bottom texture shouldn't be visible anyway
      GCon->Logf("*** BOTTOM CHECK! minz=%g; maxz=%g", minz, maxz);
      GCon->Logf("   lz=%g; llow=%g; lhigh=%g", LightPos.z, LightPos.z-Radius, LightPos.z+Radius);
      // if light is fully inside or outside, this seg cannot cast shadow
      // fully outside?
      if (LightPos.z+Radius <= minz || LightPos.z-Radius >= maxz) return false;
      // fully inside?
      if (LightPos.z+Radius > maxz) {
        return true;
      } else {
        GCon->Logf("*** BOTTOM REJECT!");
      }
    } else {
      return true;
    }
    */
    return true;
  }

  // if this is not a two-sided line, only first and last segs can cast shadows
  //!!!if ((int)(ptrdiff_t)(ldef-GClLevel->Lines) == 42) GCon->Log("********* 42 ************");
  if (*seg->v1 != *ldef->v1 && *seg->v2 != *ldef->v2 &&
      *seg->v2 != *ldef->v1 && *seg->v1 != *ldef->v2)
  {
    //!!!GCon->Log("*** skipped useless shadow segment (0)");
    return true;
  }

  // if all neighbour lines are one-sided, and doesn't make a sharp turn, this seg cannot cast a shadow

  // check v1
  const line_t *const *lnx = ldef->v1lines;
  for (int cc = ldef->v1linesCount; cc--; ++lnx) {
    const line_t *l2 = *lnx;
    if (!l2->SphereTouches(LightPos, Radius)) continue;
    if (l2->flags&ML_TWOSIDED) return true;
    if (PlaneAngles2D(ldef, l2) <= 180.0f && PlaneAngles2DFlipTo(ldef, l2) <= 180.0f) {
      //!!!GCon->Logf("::: %d vs %d: %g : %g", (int)(ptrdiff_t)(ldef-GClLevel->Lines), (int)(ptrdiff_t)(l2-GClLevel->Lines), PlaneAngles2D(ldef, l2), PlaneAngles2DFlipTo(ldef, l2));
      continue;
    } else {
      //!!!GCon->Logf("::: %d vs %d: %g : %g", (int)(ptrdiff_t)(ldef-GClLevel->Lines), (int)(ptrdiff_t)(l2-GClLevel->Lines), PlaneAngles2D(ldef, l2), PlaneAngles2DFlipTo(ldef, l2));
    }
    return true;
  }

  // check v2
  lnx = ldef->v2lines;
  for (int cc = ldef->v2linesCount; cc--; ++lnx) {
    const line_t *l2 = *lnx;
    if (!l2->SphereTouches(LightPos, Radius)) continue;
    if (l2->flags&ML_TWOSIDED) return true;
    if (PlaneAngles2D(ldef, l2) <= 180.0f && PlaneAngles2DFlipTo(ldef, l2) <= 180.0f) {
      //!!!GCon->Logf("::: %d vs %d: %g : %g", (int)(ptrdiff_t)(ldef-GClLevel->Lines), (int)(ptrdiff_t)(l2-GClLevel->Lines), PlaneAngles2D(ldef, l2), PlaneAngles2DFlipTo(ldef, l2));
      continue;
    } else {
      //!!!GCon->Logf("::: %d vs %d: %g : %g", (int)(ptrdiff_t)(ldef-GClLevel->Lines), (int)(ptrdiff_t)(l2-GClLevel->Lines), PlaneAngles2D(ldef, l2), PlaneAngles2DFlipTo(ldef, l2));
    }
    return true;
  }

  //!!!GCon->Log("*** skipped useless shadow segment (1)");
  // done, it passed all checks, and cannot cast shadow (i hope)
  return false;
}


//==========================================================================
//
//  CanSurfaceFlatCastShadow
//
//==========================================================================
static bool CanSurfaceFlatCastShadow (const surface_t *surf, const TVec LightPos, float Radius) {
  if (!gl_smart_reject_svol_flats) return true;

  // flat surfaces in subsectors whose neighbours doesn't change height can't cast any shadow
  const subsector_t *sub = surf->subsector;
  if (sub->numlines == 0) return true; // just in case

  const sector_t *sector = sub->sector;
  // sadly, we cannot optimise for sectors with 3D (extra) floors
  if (sector->SectorFlags&sector_t::SF_ExtrafloorSource) return true; // sadly, i cannot reject 3D floors yet

  // do we have any 3D floors in this sector?
  if (sector->SectorFlags&sector_t::SF_HasExtrafloors) {
    // check if we're doing top ceiling, or bottom floor
    // (this should always be the case, but...)
    if (surf->plane.normal == sector->floor.normal) {
      if (surf->plane.dist != sector->floor.dist) return true;
    } else if (surf->plane.normal == sector->ceiling.normal) {
      if (surf->plane.dist != sector->ceiling.dist) return true;
    } else {
      return true;
    }
  }

  const seg_t *seg = sub->firstseg;
  for (int cnt = sub->numlines; cnt--; ++seg) {
    const seg_t *backseg = seg->partner;
    if (!backseg) continue;
    const subsector_t *sub2 = backseg->frontsub;
    if (sub2 == sub) continue;
    // different subsector
    const sector_t *bsec = sub2->sector;
    if (bsec == sector) continue;
    // different sector
    if (!backseg->SphereTouches(LightPos, Radius)) continue;
    // and light sphere touches it, check heights
    if (surf->typeFlags&surface_t::TF_FLOOR) {
      // if current sector floor is lower than the neighbour sector floor,
      // it means that our current floor cannot cast a shadow there
      //if (sector->floor.minz <= bsec->floor.maxz) continue;
      if (bsec->floor.minz == sector->floor.minz &&
          bsec->floor.maxz == sector->floor.maxz)
      {
        continue;
      }
    } else if (surf->typeFlags&surface_t::TF_CEILING) {
      // if current sector ceiling is higher than the neighbour sector ceiling,
      // it means that our current ceiling cannot cast a shadow there
      //if (sector->ceiling.maxz >= bsec->ceiling.minz) continue;
      // this is wrong; see Doom2:MAP02, room with two holes -- shot a fireball inside one hole
      // this is wrong because we have two sectors with the same ceiling height, and then a hole
      // so first sector ceiling is lit, and should block the light, but it is ignored
      if (bsec->ceiling.minz == sector->ceiling.minz &&
          bsec->ceiling.maxz == sector->ceiling.maxz)
      {
        continue;
      }
    } else {
      GCon->Log("oops; non-floor and non-ceiling flat surface");
    }
    /*
    if (FASI(bsec->floor.minz) == FASI(sector->floor.minz) &&
        FASI(bsec->floor.maxz) == FASI(sector->floor.maxz) &&
        FASI(bsec->ceiling.minz) == FASI(sector->ceiling.minz) &&
        FASI(bsec->ceiling.maxz) == FASI(sector->ceiling.maxz))
    {
      continue;
    }
    */
    return true;
  }

  // done, it passed all checks, and cannot cast shadow (i hope)
  return false;
}


//==========================================================================
//
//  CanSurfaceCastShadow
//
//==========================================================================
static bool CanSurfaceCastShadow (const surface_t *surf, const TVec &LightPos, float Radius) {
  if (surf->seg) {
    return CanSurfaceSegCastShadow(surf, LightPos, Radius);
  } else if (surf->subsector) {
    return CanSurfaceFlatCastShadow(surf, LightPos, Radius);
  }
  // just in case
  return true;
}
