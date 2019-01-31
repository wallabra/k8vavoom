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
//**
//**  Self registering cheat commands.
//**
//**************************************************************************
#include "gamedefs.h"
#include "sv_local.h"


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
    return false;
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
  if (Source == SRC_Command) {
    ForwardToServer();
    return;
  }
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
    VClass *cls = VMemberBase::StaticFindClass(*list[f]);
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
    VClass *cls = VMemberBase::StaticFindClass(*list[f]);
    if (!cls) continue;
    VClass *c1 = cls->GetReplacement();
    if (!c1 || !c1->IsChildOf(actor)) continue;
    // check state
    VStateLabel *lbl = cls->FindStateLabel("Spawn");
    if (!lbl || !lbl->State) continue; // no spawn state
    if (!R_AreSpritesPresent(lbl->State->SpriteIndex)) continue; // no sprite
    newlist.append(list[f]);
  }
  return AutoCompleteFromList(prefix, newlist, true); // return unchanged as empty
}
#endif


//==========================================================================
//
//  COMMAND VScript_Command
//
//==========================================================================
COMMAND(VScript_Command) {
  if (Args.Num() < 2) return;

  if (Source == SRC_Command) {
    ForwardToServer();
    return;
  }

  if (!Player) return;
  if (CheatAllowed(Player)) {
    Args.removeAt(0); // remove command name
    Player->eventCheat_VScriptCommand(Args);
  }
}


//==========================================================================
//
//  Script_f
//
//==========================================================================
COMMAND(Script) {
  if (Source == SRC_Command) {
    ForwardToServer();
    return;
  }

  if (CheatAllowed(Player)) {
    if (Args.Num() != 2) return;
    int script = atoi(*Args[1]);
    if (script < 1) return;
    if (script > 9999) return;
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
  if (Source == SRC_Command) {
    ForwardToServer();
    return;
  }
  if (CheatAllowed(Player)) {
    Player->Printf("MAP %s  X:%f  Y:%f  Z:%f  Yaw:%f Pitch:%f",
      *GLevel->MapName, Player->MO->Origin.x,
      Player->MO->Origin.y, Player->MO->Origin.z,
      Player->MO->Angles.yaw, Player->MO->Angles.pitch);
    GCon->Logf("warpto %d %d", (int)Player->MO->Origin.x, (int)Player->MO->Origin.y);
  }
}


//==========================================================================
//
//  my_sector_info
//
//==========================================================================
COMMAND(my_sector_info) {
  if (Source == SRC_Command) {
    ForwardToServer();
    return;
  }
  if (!Player) { GCon->Log("NO PLAYER!"); return; }
  if (!Player->MO) { GCon->Log("NO PLAYER MOBJ!"); return; }
  if (!Player->MO->Sector) { GCon->Log("PLAYER MOBJ SECTOR IS UNKNOWN!"); return; }
  //if (!Player || !Player->MO || !Player->MO->Sector) return;
  sector_t *sec = Player->MO->Sector;
  GCon->Logf("Sector #%d; tag=%d; special=%d; damage=%d; seqtype=%d; sndtrav=%d; sky=%d",
    (int)(intptr_t)(sec-Player->Level->XLevel->Sectors),
    sec->tag, sec->special, sec->seqType, sec->soundtraversed, sec->Damage, sec->Sky
  );
  GCon->Logf("  floor texture  : %s", *GTextureManager.GetTextureName(sec->floor.pic));
  GCon->Logf("  ceiling texture: %s", *GTextureManager.GetTextureName(sec->ceiling.pic));
  GCon->Logf("  ceil : %f %f", sec->ceiling.minz, sec->ceiling.maxz);
  GCon->Logf("  floor: %f %f", sec->floor.minz, sec->floor.maxz);
}


//==========================================================================
//
//  my_sector_info
//
//==========================================================================
COMMAND(my_clear_automap) {
  if (Source == SRC_Command) {
    ForwardToServer();
    return;
  }
  if (!Player) { GCon->Log("NO PLAYER!"); return; }
  if (!Player->MO) { GCon->Log("NO PLAYER MOBJ!"); return; }
  for (int i = 0; i < GClLevel->NumLines; ++i) {
    line_t &line = GClLevel->Lines[i];
    line.flags &= ~ML_MAPPED;
  }
  GCon->Logf("automap cleared");
}
