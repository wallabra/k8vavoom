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


static int cli_WGozzo3D = 0;
static int cli_WVavoom3D = 0;
static int cli_WFloors3D = 0;

/*static*/ bool cliRegister_3dfloor_wargs =
  VParsedArgs::RegisterFlagSet("-Wgozzo-3d", "!GZDoom 3d floors warnings", &cli_WGozzo3D) &&
  VParsedArgs::RegisterFlagSet("-Wvavoom-3d", "!Vavoom 3d floors warnings", &cli_WVavoom3D) &&
  VParsedArgs::RegisterFlagSet("-W3dfloors", "!various 3d floors warnings", &cli_WFloors3D);


//==========================================================================
//
//  getTexName
//
//==========================================================================
static __attribute__((unused)) const char *getTexName (int txid) {
  if (txid == 0) return "<->";
  VTexture *tex = GTextureManager[txid];
  return (tex ? *tex->Name : "<none>");
}


//==========================================================================
//
//  VLevel::AddExtraFloorSane
//
//  k8vavoom
//
//==========================================================================
void VLevel::AddExtraFloorSane (line_t *line, sector_t *dst) {
  bool doDump = (cli_WAll || cli_WFloors3D || cli_WVavoom3D);

  sector_t *src = line->frontsector;

  const float floorz = src->floor.GetPointZ(dst->soundorg);
  const float ceilz = src->ceiling.GetPointZ(dst->soundorg);
  bool flipped = false;

  if (floorz < ceilz) {
    flipped = true;
    GCon->Logf("Swapped planes for k8vavoom 3d floor, tag: %d, floorz: %g, ceilz: %g", line->arg1, ceilz, floorz);
  }

  if (doDump) { GCon->Logf("k8vavoom 3d floor for tag %d (dst #%d, src #%d) (floorz=%g; ceilz=%g)", line->arg1, (int)(ptrdiff_t)(dst-Sectors), (int)(ptrdiff_t)(src-Sectors), floorz, ceilz); }
  if (doDump) { GCon->Logf("::: VAVOOM 3DF BEFORE"); dumpSectorRegions(dst); }

  // append link
  src->SectorFlags |= sector_t::SF_ExtrafloorSource;
  dst->SectorFlags |= sector_t::SF_HasExtrafloors;
  AppendControlLink(src, dst);

  // insert into region array
  // control must have negative height, so
  // region floor is ceiling, and region ceiling is floor
  sec_region_t *reg = dst->AllocRegion();
  if (flipped) {
    // flipped
    reg->efloor.set(&src->floor, true);
    reg->eceiling.set(&src->ceiling, true);
  } else {
    // normal
    reg->efloor.set(&src->ceiling, false);
    reg->eceiling.set(&src->floor, false);
  }
  reg->params = &src->params;
  reg->extraline = line;

  if (doDump) { GCon->Logf("::: VAVOOM 3DF AFTER"); dumpSectorRegions(dst); }
}


//==========================================================================
//
//  VLevel::AddExtraFloorShitty
//
//  gozzo
//
//==========================================================================
void VLevel::AddExtraFloorShitty (line_t *line, sector_t *dst) {
  enum {
    Invalid,
    Solid,
    Swimmable,
    NonSolid,
  };

  bool doDump = (cli_WAll || cli_WFloors3D || cli_WGozzo3D);

  //int eftype = (line->arg2&3);
  const bool isSolid = ((line->arg2&3) == Solid);

  sector_t *src = line->frontsector;

  if (doDump) { GCon->Logf("src sector #%d: floor=%s; ceiling=%s; (%g,%g); type=0x%02x, flags=0x%04x (solid=%d)", (int)(ptrdiff_t)(src-Sectors), getTexName(src->floor.pic), getTexName(src->ceiling.pic), min2(src->floor.minz, src->floor.maxz), max2(src->ceiling.minz, src->ceiling.maxz), line->arg2, line->arg3, (int)isSolid); }
  if (doDump) { GCon->Logf("dst sector #%d: soundorg=(%g,%g,%g); fc=(%g,%g)", (int)(ptrdiff_t)(dst-Sectors), dst->soundorg.x, dst->soundorg.y, dst->soundorg.z, min2(dst->floor.minz, dst->floor.maxz), max2(dst->ceiling.minz, dst->ceiling.maxz)); }

  const float floorz = src->floor.GetPointZ(dst->soundorg);
  const float ceilz = src->ceiling.GetPointZ(dst->soundorg);
  bool flipped = false;

  if (floorz > ceilz) {
    flipped = true;
    GCon->Logf("Swapped planes for tag: %d, floorz: %g, ceilz: %g", line->arg1, ceilz, floorz);
  }

  if (doDump) { GCon->Logf("3d floor for tag %d (dst #%d, src #%d) (floorz=%g; ceilz=%g)", line->arg1, (int)(ptrdiff_t)(dst-Sectors), (int)(ptrdiff_t)(src-Sectors), floorz, ceilz); }
  if (doDump) { GCon->Logf("::: BEFORE"); dumpSectorRegions(dst); }

  // append link
  src->SectorFlags |= sector_t::SF_ExtrafloorSource;
  dst->SectorFlags |= sector_t::SF_HasExtrafloors;
  AppendControlLink(src, dst);

  // insert into region array
  sec_region_t *reg = dst->AllocRegion();
  if (isSolid) {
    // solid region: floor points down, ceiling points up
    if (flipped) {
      // flipped
      reg->efloor.set(&src->ceiling, false);
      reg->eceiling.set(&src->floor, false);
    } else {
      // normal
      reg->efloor.set(&src->floor, true);
      reg->eceiling.set(&src->ceiling, true);
    }
  } else {
    // non-solid region: floor points up, ceiling points down
    if (flipped) {
      // flipped
      reg->efloor.set(&src->ceiling, true);
      reg->eceiling.set(&src->floor, true);
    } else {
      // normal
      reg->efloor.set(&src->floor, false);
      reg->eceiling.set(&src->ceiling, false);
    }
  }
  reg->params = &src->params;
  reg->extraline = line;
  if (!isSolid) {
    // if "restrict light inside" is set, this seems to be a legacy/3dge water
    if ((line->arg2&3) == Swimmable && (line->arg3&2)) reg->extraline = nullptr; //FIXME!
    reg->regflags |= sec_region_t::RF_NonSolid;
  }

  if (!isSolid) {
    // non-solid regions has visible floor and ceiling only when camera is inside
    // add the same region, but with flipped floor and ceiling (and mark it as visual only)
    sec_region_t *reg2 = dst->AllocRegion();
    reg2->efloor = reg->efloor;
    reg2->efloor.Flip();
    reg2->eceiling = reg->eceiling;
    reg2->eceiling.Flip();
    reg2->params = reg->params;
    reg2->extraline = nullptr;
    reg2->regflags = sec_region_t::RF_OnlyVisual;
  }
}


//==========================================================================
//
//  Level::AddExtraFloor
//
//  can return `nullptr`
//
//==========================================================================
void VLevel::AddExtraFloor (line_t *line, sector_t *dst) {
  return (line->arg2 == 0 ? AddExtraFloorSane(line, dst) : AddExtraFloorShitty(line, dst));
}
