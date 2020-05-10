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
//**  Self registering cheat commands.
//**
//**************************************************************************
#include "gamedefs.h"
#include "server/sv_local.h"


/*
extern "C" {
  static int sortCmpVStrCI (const void *a, const void *b, void *udata) {
    if (a == b) return 0;
    VStr *sa = (VStr *)a;
    VStr *sb = (VStr *)b;
    return sa->ICmp(*sb);
  }
}
*/


//==========================================================================
//
//  CheatAllowed
//
//==========================================================================
static bool CheatAllowed (VBasePlayer *Player, bool allowDead=false) {
  if (!Player) return false;
  if (sv.intermission) {
    Player->Printf("You are not in game!");
    return false;
  }
  if (GGameInfo->NetMode >= NM_DedicatedServer) {
    Player->Printf("You cannot cheat in a network game!");
    return false;
  }
  if (GGameInfo->WorldInfo->Flags&VWorldInfo::WIF_SkillDisableCheats) {
    Player->Printf("You are too good to cheat!");
    //return false;
    //k8: meh, if i want to cheat, i want to cheat!
    return true;
  }
  if (!allowDead && Player->Health <= 0) {
    // dead players can't cheat
    Player->Printf("You must be alive to cheat");
    return false;
  }
  return true;
}


#if 0
// all cheats are automatically collected from VC code
// left here as an example

//==========================================================================
//
//  Summon_f
//
//  Cheat code Summon
//
//==========================================================================
COMMAND_WITH_AC(Summon) {
  CMD_FORWARD_TO_SERVER();
  if (CheatAllowed(Player)) Player->eventCheat_Summon();
}

COMMAND_AC(Summon) {
  if (aidx != 1) return VStr::EmptyString;
  VClass *actor = VMemberBase::StaticFindClass("Actor");
  if (!actor) return VStr::EmptyString;
  VStr prefix = (aidx < args.length() ? args[aidx] : VStr());
  TArray<VStr> list;
  VMemberBase::StaticGetClassListNoCase(list, prefix, actor);
  // replace with replacements ;-)
  /*
  for (int f = 0; f < list.length(); ++f) {
    VClass *cls = VMemberBase::StaticFindClassNoCase(*list[f]);
    if (!cls) { list[f].clear(); continue; }
    VClass *c1 = cls->GetReplacement();
    if (!c1 || !c1->IsChildOf(actor)) { list[f].clear(); continue; }
    list[f] = VStr(c1->GetName());
  }
  */
  if (!list.length()) return VStr::EmptyString;
  // sort
  timsort_r(list.ptr(), list.length(), sizeof(VStr), &sortCmpVStrCI, nullptr);
  // drop unspawnable actors
  TArray<VStr> newlist;
  for (int f = 0; f < list.length(); ++f) {
    if (list.length() == 0 || (newlist.length() && newlist[newlist.length()-1] == list[f])) continue;
    VClass *cls = VMemberBase::StaticFindClassNoCase(*list[f]);
    if (!cls) continue;
    VClass *c1 = cls->GetReplacement();
    if (!c1 || !c1->IsChildOf(actor)) continue;
    // check state
    VStateLabel *lbl = cls->FindStateLabel("Spawn");
    if (!lbl || !lbl->State) continue; // no spawn state
    if (!R_AreSpritesPresent(lbl->State->SpriteIndex)) continue; // no sprite
    newlist.append(list[f]);
  }
  return AutoCompleteFromListCmd(prefix, newlist);
}
#endif


//==========================================================================
//
//  COMMAND VScript_Command
//
//==========================================================================
/*
COMMAND(VScript_Command) {
  CMD_FORWARD_TO_SERVER();
  if (Args.Num() < 2) return;
  if (!Player) return;
  if (CheatAllowed(Player)) {
    Args.removeAt(0); // remove command name
    Player->eventCheat_VScriptCommand(Args);
  }
}
*/


#ifdef CLIENT
//==========================================================================
//
//  COMMAND Resurrect
//
//==========================================================================
COMMAND(Resurrect) {
  CMD_FORWARD_TO_SERVER();
  if (!Player) return;
  if (CheatAllowed(Player, true)) {
    if (Player->PlayerState == PST_DEAD) Player->PlayerState = PST_CHEAT_REBORN;
  }
}
#endif


//==========================================================================
//
//  Script_f
//
//==========================================================================
COMMAND(Script) {
  if (GGameInfo->NetMode == NM_Client) return;
  CMD_FORWARD_TO_SERVER();
  if (CheatAllowed(Player)) {
    if (Args.Num() != 2) return;
    int script = VStr::atoi(*Args[1]);
    if (script < 1) return;
    if (script > 65535) return;
    if (Player->Level->XLevel->Acs->Start(script, 0, 0, 0, 0, 0, Player->MO, nullptr, 0, false, false)) {
      GCon->Logf("Running script %d", script);
    }
  }
}


//==========================================================================
//
//  MyPos_f
//
//==========================================================================
COMMAND(MyPos) {
  CMD_FORWARD_TO_SERVER();
  if (/*CheatAllowed(Player)*/true) {
    const VLevel *lvl = Player->Level->XLevel;
    const subsector_t *ss = lvl->PointInSubsector_Buggy(Player->MO->Origin);
    TArray<VStr> info;
    info.append(va("Map Hash (sha): %s", *Player->Level->XLevel->MapHash));
    info.append(va("Map Hash (md5): %s", *Player->Level->XLevel->MapHashMD5));
    info.append(va("sub: %d; sector: %d", (int)(ptrdiff_t)(ss-lvl->Subsectors), (int)(ptrdiff_t)(ss->sector-lvl->Sectors)));
    info.append(va("pos:(%g,%g,%g)  angles:(%g,%g)",
      Player->MO->Origin.x, Player->MO->Origin.y, Player->MO->Origin.z,
      Player->MO->Angles.yaw, Player->MO->Angles.pitch));
    info.append(va("+map %s \"+warpto %d %d %d\"", *GLevel->MapName, (int)Player->MO->Origin.x, (int)Player->MO->Origin.y, (int)Player->MO->Origin.z));
    // send to player if we're in a net
    if (Player->Net) {
      VStr pstr;
      for (auto &&s : info) {
        if (pstr.length()) pstr += "\n";
        pstr += s;
      }
      Player->Printf("%s", *pstr);
    }
    // print to console
    for (auto &&s : info) GCon->Logf("%s", *s);
  }
}


//==========================================================================
//
//  GetTexLumpName
//
//==========================================================================
static VStr GetTexLumpName (int tid) {
  VTexture *tex = GTextureManager[tid];
  return (tex ? W_FullLumpName(tex->SourceLump) : VStr("[none]"));
}


//==========================================================================
//
//  my_sector_info
//
//==========================================================================
COMMAND(my_sector_info) {
  CMD_FORWARD_TO_SERVER();
  if (!Player) { GCon->Log("NO PLAYER!"); return; }
  if (!Player->MO) { GCon->Log("NO PLAYER MOBJ!"); return; }
  if (!Player->MO->Sector) { GCon->Log("PLAYER MOBJ SECTOR IS UNKNOWN!"); return; }
  //if (!Player || !Player->MO || !Player->MO->Sector) return;

  sector_t *sec;

  /*
  if (Args.length() > 3) {
    int snum;
    if (!VStr::convertInt(*Args[1], &snum)) {
      Player->Printf("invalid sector number: '%s'", *Args[1]);
      return;
    }
    if (snum < 0 || snum >= Player->Level->XLevel->NumSectors) {
      Player->Printf("invalid sector number: %d (max is %d)", snum, Player->Level->XLevel->NumSectors-1);
      return;
    }
    sec = &Player->Level->XLevel->Sectors[snum];
  } else
  */
  {
    sec = Player->MO->Sector;
  }

  Player->Printf("Sector #%d (sub #%d); tag=%d; special=%d; damage=%d; seqtype=%d; sndtrav=%d; sky=%d",
    (int)(intptr_t)(sec-Player->Level->XLevel->Sectors),
    (int)(intptr_t)(Player->MO->SubSector-Player->Level->XLevel->Subsectors),
    sec->sectorTag, sec->special, sec->seqType, sec->soundtraversed, sec->Damage, sec->Sky
  );
  Player->Printf("  floor texture  : %s (%d) <%s>", *GTextureManager.GetTextureName(sec->floor.pic), sec->floor.pic.id, *GetTexLumpName(sec->floor.pic));
  Player->Printf("  ceiling texture: %s (%d) <%s>", *GTextureManager.GetTextureName(sec->ceiling.pic), sec->ceiling.pic.id, *GetTexLumpName(sec->ceiling.pic));
  Player->Printf("  ceil : %f %f", sec->ceiling.minz, sec->ceiling.maxz);
  Player->Printf("  floor: %f %f", sec->floor.minz, sec->floor.maxz);

  sec_region_t *reg = SV_PointRegionLight(sec, Player->MO->Origin);
  Player->Printf("  Fade : 0x%08x", reg->params->Fade);
  Player->Printf("  floor light source sector: %d", sec->floor.LightSourceSector);
  Player->Printf("  ceiling light source sector: %d", sec->ceiling.LightSourceSector);

  GCon->Log("=== contents ===");
  int ct = SV_PointContents(sec, Player->MO->Origin, true);
  Player->Printf("contents: %d", ct);

  if (Args.length() > 1) {
    GCon->Log("*** REGION DUMP START ***");
    Player->Level->XLevel->dumpSectorRegions(sec);
    GCon->Log("*** REGION DUMP END ***");
  }
  if (Args.length() > 2) {
    TSecPlaneRef floor, ceiling;
    SV_FindGapFloorCeiling(sec, Player->MO->Origin, Player->MO->Height, floor, ceiling, true);
    Player->Printf(" gap floor: %g (%g,%g,%g:%g)", floor.GetPointZClamped(Player->MO->Origin), floor.GetNormal().x, floor.GetNormal().y, floor.GetNormal().z, floor.GetDist());
    Player->Printf(" gap ceil : %g (%g,%g,%g:%g)", ceiling.GetPointZClamped(Player->MO->Origin), ceiling.GetNormal().x, ceiling.GetNormal().y, ceiling.GetNormal().z, ceiling.GetDist());
    /*
    sec_region_t *gap = SV_PointRegionLight(sec, Player->MO->Origin, true);
    if (gap) Player->Printf("=== PT0: %p", gap);
    gap = SV_PointRegionLight(sec, Player->MO->Origin+TVec(0.0f, 0.0f, Player->MO->Height*0.5f), true);
    if (gap) Player->Printf("=== PT1: %p", gap);
    gap = SV_PointRegionLight(sec, Player->MO->Origin+TVec(0.0f, 0.0f, Player->MO->Height), true);
    if (gap) Player->Printf("=== PT2: %p", gap);
    */
    //GCon->Log("=== light ===");
    //(void)SV_PointRegionLight(sec, Player->MO->Origin, true);
    //GCon->Log("=== light sub ===");
    //(void)SV_PointRegionLightSub(Player->MO->SubSector, Player->MO->Origin, nullptr, true);

    #ifdef CLIENT
      if (Args.length() > 3) {
        SV_DebugFindNearestFloor(Player->MO->SubSector, Player->MO->Origin);
      }
    #endif
  }
}


//==========================================================================
//
//  my_clear_automap
//
//==========================================================================
#ifdef CLIENT
COMMAND(my_clear_automap) {
  //CMD_FORWARD_TO_SERVER();
  if (!Player) { GCon->Log("NO PLAYER!"); return; }
  if (!Player->MO) { GCon->Log("NO PLAYER MOBJ!"); return; }
  AM_ClearAutomap();
  GCon->Logf("automap cleared");
}
#endif


//==========================================================================
//
//  vm_profile_start
//
//  any arg given: calc time with nested calls
//
//==========================================================================
COMMAND(vm_profile_start) {
  VObject::ProfilerEnabled = (Args.length() > 1 ? -1 : 1);
}


//==========================================================================
//
//  vm_profile_end
//
//==========================================================================
COMMAND(vm_profile_stop) {
  VObject::ProfilerEnabled = 0;
}


//==========================================================================
//
//  vm_profile_dump
//
//==========================================================================
COMMAND(vm_profile_dump) {
  VObject::DumpProfile();
}


//==========================================================================
//
//  vm_profile_clear
//
//==========================================================================
COMMAND(vm_profile_clear) {
  VObject::ClearProfiles();
}
