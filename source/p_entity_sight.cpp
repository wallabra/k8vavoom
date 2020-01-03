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
//**
//**  LineOfSight/Visibility checks, uses REJECT Lookup Table.
//**
//**  This uses specialized forms of the maputils routines for optimized
//**  performance
//**
//**************************************************************************
#include "gamedefs.h"
#include "sv_local.h"


static VCvarB compat_better_sight("compat_better_sight", true, "Check more points in LOS calculations?", CVAR_Archive);
static VCvarB dbg_disable_cansee("dbg_disable_cansee", false, "Disable CanSee processing (for debug)?", CVAR_PreInit);

/*
  enum {
    CSE_ForShooting      = 1u<<0,
    CSE_AlwaysBetter     = 1u<<1,
    CSE_IgnoreBlockAll   = 1u<<2,
    CSE_IgnoreFakeFloors = 1u<<3,
  };
  bool CanSeeEx (VEntity *Ent, unsigned flags=0);
*/

bool VEntity::CanSee (VEntity *Other, bool forShooting, bool alwaysBetter) {
  return CanSeeEx(Other, (forShooting ? CSE_ForShooting : 0u)|(alwaysBetter ? CSE_AlwaysBetter : 0u));
}


//k8: for some reason, sight checks ignores base sector region
//==========================================================================
//
//  VEntity::CanSeeEx
//
//  LineOfSight/Visibility checks, uses REJECT Lookup Table. This uses
//  specialised forms of the maputils routines for optimized performance
//  Returns true if a straight line between t1 and t2 is unobstructed.
//
//==========================================================================
bool VEntity::CanSeeEx (VEntity *Other, unsigned flags) {
  if (dbg_disable_cansee) return false;

  if (!Other || !Other->Sector || !Other->SubSector) return false;
  if (GetFlags()&(_OF_Destroyed|_OF_DelayedDestroy)) return false;
  if (Other->GetFlags()&(_OF_Destroyed|_OF_DelayedDestroy)) return false;

  if (Other == this) return true; // it can see itself (obviously)

  // if we have no subsector for this object, it cannot see anything
  if (!SubSector) {
    if (developer) GCon->Logf(NAME_Dev, "EMPTY SUBSECTOR for '%s'", *GetClass()->GetFullName());
    return false;
  }

  // determine subsector entries in GL_PVS table
  // first check for trivial rejection
  if (!XLevel->IsLeafVisible(SubSector, Other->SubSector)) return false; // can't possibly be connected
  if (XLevel->IsRejectedVis(Sector, Other->Sector)) return false; // can't possibly be connected

  // killough 11/98: shortcut for melee situations
  // same subsector? obviously visible
  if (SubSector == Other->SubSector) return true;

  bool forShooting = !!(flags&CSE_ForShooting);
  bool alwaysBetter = !!(flags&CSE_AlwaysBetter);

  if (alwaysBetter) forShooting = false;

  bool cbs = (!forShooting && (alwaysBetter || compat_better_sight));

  if (cbs && !alwaysBetter) {
    // turn off "better sight" if it is not forced, and neither entity is monster
    //cbs = (IsPlayerOrMonster() && Other->IsPlayerOrMonster());
    cbs = (IsMonster() || Other->IsMonster());
    //if (!cbs) GCon->Logf(NAME_Debug, "%s: better sight forced to 'OFF', checking sight to '%s' (not a player, not a monster)", GetClass()->GetName(), Other->GetClass()->GetName());
  }

  // if too far, don't do "better sight" (it doesn't worth it anyway)
  if (cbs) {
    const float distSq = (Origin-Other->Origin).length2DSquared();
    cbs = (distSq < 680.0*680.0); // arbitrary number
    //if (!cbs) GCon->Logf(NAME_Debug, "%s: better sight forced to 'OFF', checking sight to '%s' (dist=%g)", GetClass()->GetName(), Other->GetClass()->GetName(), sqrtf(distSq));
  }

  TVec dirF, dirR;
  if (cbs) {
    //YawVectorRight(Angles.yaw, dirR);
    TVec dirU;
    TAVec ang;
    ang.yaw = Angles.yaw;
    ang.pitch = 0.0f;
    ang.roll = 0.0f;
    AngleVectors(ang, dirF, dirR, dirU);
  } else {
    dirF = dirR = TVec::ZeroVector;
  }
  //if (forShooting) dirR = TVec::ZeroVector; // just in case, lol
  return XLevel->CastCanSee(Sector, Origin, Height, dirF, dirR, Other->Origin, Other->Radius, Other->Height, !(flags&CSE_CheckBaseRegion)/*skip base region*/, Other->Sector, /*alwaysBetter*/cbs, !!(flags&CSE_IgnoreBlockAll), !!(flags&CSE_IgnoreFakeFloors));
}
