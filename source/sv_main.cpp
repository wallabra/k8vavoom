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
#include "gamedefs.h"
#include "net/network.h"
#include "sv_local.h"
#include "cl_local.h"

bool sv_skipOneTitlemap = false;

/*
static double FrameTime = 1.0f/35.0f;
// round a little bit up to prevent "slow motion"
*(vuint64 *)&FrameTime += 1;
*/
static const double FrameTime = 0x1.d41d41d41d41ep-6; // same as above
/*
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
int main () {
  double FrameTime = 1.0/35.0;
  printf("%a %.21g 0x%08llx\n", FrameTime, FrameTime, *(const uint64_t *)&FrameTime);
  // round a little bit up to prevent "slow motion"
  *(uint64_t *)&FrameTime += 1;
  printf("%a %.21g 0x%08llx\n", FrameTime, FrameTime, *(const uint64_t *)&FrameTime);
  *(uint64_t *)&FrameTime += 1;
  printf("%a %.21g 0x%08llx\n", FrameTime, FrameTime, *(const uint64_t *)&FrameTime);
  printf("---\n");
  FrameTime = 0x1.d41d41d41d41dp-6;
  printf("%a %.21g 0x%08llx\n", FrameTime, FrameTime, *(const uint64_t *)&FrameTime);
  FrameTime = 0x1.d41d41d41d41ep-6;
  printf("%a %.21g 0x%08llx\n", FrameTime, FrameTime, *(const uint64_t *)&FrameTime);
  return 0;
}
*/


static void G_DoReborn (int playernum, bool cheatReborn);
static void G_DoCompleted ();

extern VCvarB dbg_vm_disable_thinkers;

static VCvarB dbg_skipframe_player_tick("dbg_skipframe_player_tick", true, "Run player ticks on skipped frames?", CVAR_PreInit);
static VCvarB dbg_skipframe_player_block_move("dbg_skipframe_player_block_move", false, "Keep moving on skipped player frames (this is wrong)?", CVAR_PreInit);

VCvarB real_time("real_time", true, "Run server in real time?");

static VCvarB sv_ignore_nojump("sv_ignore_nojump", false, "Ignore \"nojump\" flag in MAPINFO?", CVAR_ServerInfo|CVAR_Latch|CVAR_PreInit);
static VCvarB sv_ignore_nocrouch("sv_ignore_nocrouch", false, "Ignore \"nocrouch\" flag in MAPINFO?", CVAR_ServerInfo|CVAR_Latch|CVAR_PreInit);
/*static*/ VCvarB sv_ignore_nomlook("sv_ignore_nomlook", false, "Ignore \"nofreelook\" flag in MAPINFO?", CVAR_ServerInfo|CVAR_Latch|CVAR_PreInit);
static VCvarB sv_ignore_reset_health("sv_ignore_reset_health", false, "Ignore \"resethealth\" flag in MAPINFO?", CVAR_ServerInfo|CVAR_Latch|CVAR_PreInit);
static VCvarB sv_ignore_reset_inventory("sv_ignore_reset_inventory", false, "Ignore \"resetinventory\" flag in MAPINFO?", CVAR_ServerInfo|CVAR_Latch|CVAR_PreInit);
static VCvarB sv_ignore_reset_items("sv_ignore_reset_items", false, "Ignore \"resetitems\" flag in MAPINFO?", CVAR_ServerInfo|CVAR_Latch|CVAR_PreInit);

static VCvarB sv_force_pistol_start("sv_force_pistol_start", false, "Start each new map with default weapons?", CVAR_ServerInfo|CVAR_Latch|CVAR_PreInit);
static VCvarB sv_force_health_reset("sv_force_health_reset", false, "Start each new map with default health?", CVAR_ServerInfo|CVAR_Latch|CVAR_PreInit);

static VCvarB mod_allow_server_cvars("mod_allow_server_cvars", false, "Allow server cvars from CVARINFO?", CVAR_Archive|CVAR_PreInit);

static VCvarB cl_disable_crouch("cl_disable_crouch", false, "Disable crouching?", CVAR_Archive);
static VCvarB cl_disable_jump("cl_disable_jump", false, "Disable jumping?", CVAR_Archive);
static VCvarB cl_disable_mlook("cl_disable_mlook", false, "Disable mouselook?", CVAR_Archive);

extern VCvarI host_max_skip_frames;
extern VCvarB NoExit;

static VCvarB __dbg_cl_always_allow_pause("__dbg_cl_always_allow_pause", false, "Allow pausing in network games?", CVAR_PreInit);


server_t sv;
server_static_t svs;

// increment every time a check is made
int validcount = 1;

bool sv_loading = false;
bool sv_map_travel = false;
int sv_load_num_players;
bool run_open_scripts;

VBasePlayer *GPlayersBase[MAXPLAYERS];
vuint8 deathmatch = 0; // only if started as net death
int TimerGame = 0;
VLevelInfo *GLevelInfo;
int LeavePosition;
bool completed;
VNetContext *GDemoRecordingContext;

static int RebornPosition; // position indicator for cooperative net-play reborn

static bool mapteleport_issued = false;
static int mapteleport_flags = 0;
static int mapteleport_skill = -1;

static VCvarI TimeLimit("TimeLimit", "0", "TimeLimit mode?", CVAR_PreInit);
VCvarB NoExit("NoExit", false, "Disable exiting in deathmatch?", CVAR_PreInit);
static VCvarI DeathMatch("DeathMatch", "0", "DeathMatch mode.", CVAR_ServerInfo|CVAR_PreInit);
VCvarB NoMonsters("NoMonsters", false, "NoMonsters mode?", CVAR_PreInit);
VCvarI Skill("Skill", "3", "Skill level.", CVAR_PreInit);
static VCvarB sv_cheats("sv_cheats", false, "Allow cheats in network game?", CVAR_ServerInfo|CVAR_Latch|CVAR_PreInit);
static VCvarB sv_barrel_respawn("sv_barrel_respawn", false, "Respawn barrels in network game?", CVAR_Archive|CVAR_ServerInfo|CVAR_Latch|CVAR_PreInit);
static VCvarB sv_pushable_barrels("sv_pushable_barrels", true, "Pushable barrels?", CVAR_ServerInfo|CVAR_Latch|CVAR_PreInit);
static VCvarB sv_decoration_block_projectiles("sv_decoration_block_projectiles", true, "Should decoration things block projectiles?", CVAR_Archive|CVAR_ServerInfo|CVAR_Latch|CVAR_PreInit);
static VCvarB split_frame("split_frame", true, "Splitframe mode?", CVAR_Archive|CVAR_PreInit);
static VCvarI sv_maxmove("sv_maxmove", "400", "Maximum allowed network movement.", CVAR_Archive);
static VCvarF master_heartbeat_time("master_heartbeat_time", "300", "Master server heartbit interval.", CVAR_Archive|CVAR_PreInit);

static VServerNetContext *ServerNetContext;

static double LastMasterUpdate;


struct VCVFSSaver {
  void *userdata;
  VStream *(*dgOpenFile) (const VStr &filename, void *userdata);

  VCVFSSaver () : userdata(VMemberBase::userdata), dgOpenFile(VMemberBase::dgOpenFile) {}

  ~VCVFSSaver () {
    VMemberBase::userdata = userdata;
    VMemberBase::dgOpenFile = dgOpenFile;
  }
};


static int vcmodCurrFileLump = -1;

static VStream *vcmodOpenFile (const VStr &filename, void *userdata) {
  /*
  for (int flump = W_IterateFile(-1, filename); flump >= 0; flump = W_IterateFile(flump, filename)) {
    if (vcmodCurrFile >= 0 && (vcmodCurrFile != W_LumpFile(flump))) continue;
    //fprintf(stderr, "VC: found <%s> for <%s>\n", *W_FullLumpName(flump), *filename);
    return W_CreateLumpReaderNum(flump);
  }
  */
  int lmp = W_CheckNumForFileNameInSameFileOrLower(vcmodCurrFileLump, filename);
  if (lmp >= 0) return W_CreateLumpReaderNum(lmp);
  //fprintf(stderr, "VC: NOT found <%s>\n", *filename);
  return nullptr;
}


//==========================================================================
//
//  G_LoadVCMods
//
//  loading mods, take list from modlistfile
//  load user-specified Vavoom C script files
//
//==========================================================================
void G_LoadVCMods (VName modlistfile, const char *modtypestr) {
  if (modlistfile == NAME_None) return;
  if (!modtypestr && !modtypestr[0]) modtypestr = "common";
  VCVFSSaver saver;
  VMemberBase::dgOpenFile = &vcmodOpenFile;
  for (int ScLump = W_IterateNS(-1, WADNS_Global); ScLump >= 0; ScLump = W_IterateNS(ScLump, WADNS_Global)) {
    if (W_LumpName(ScLump) != modlistfile) continue;
    //vcmodCurrFile = W_LumpFile(ScLump);
    vcmodCurrFileLump = ScLump;
    VScriptParser *sc = new VScriptParser(W_FullLumpName(ScLump), W_CreateLumpReaderNum(ScLump));
    GCon->Logf(NAME_Init, "parsing Vavoom C mod list from '%s'...", *W_FullLumpName(ScLump));
    while (!sc->AtEnd()) {
      sc->ExpectString();
      //fprintf(stderr, "  <%s>\n", *sc->String.quote());
      while (sc->String.length() && (vuint8)sc->String[0] <= ' ') sc->String.chopLeft(1);
      while (sc->String.length() && (vuint8)sc->String[sc->String.length()-1] <= ' ') sc->String.chopRight(1);
      if (sc->String.length() == 0 || sc->String[0] == '#' || sc->String[0] == ';') continue;
      GCon->Logf(NAME_Init, "loading %s Vavoom C mod '%s'...", modtypestr, *sc->String);
      VMemberBase::StaticLoadPackage(VName(*sc->String), TLocation());
    }
    delete sc;
  }
}


//==========================================================================
//
//  SV_Init
//
//==========================================================================
void SV_Init () {
  svs.max_clients = 1;

  VMemberBase::StaticLoadPackage(NAME_game, TLocation());

  // load user-specified Vavoom C script files
  G_LoadVCMods("loadvcs", "server");

  GGameInfo = (VGameInfo *)VObject::StaticSpawnWithReplace(VClass::FindClass("MainGameInfo"));
  GCon->Logf(NAME_Init, "Spawned game info object of class '%s'", *GGameInfo->GetClass()->GetFullName());
  GGameInfo->eventInit();

  ProcessDecorateScripts();
  ProcessDecalDefs();
  ProcessDehackedFiles();

  GGameInfo->eventPostDecorateInit();

  if (GArgs.CheckParm("-dbg-dump-doomed")) VClass::StaticDumpMObjInfo();
  if (GArgs.CheckParm("-dbg-dump-scriptid")) VClass::StaticDumpScriptIds();

  for (int i = 0; i < VClass::GSpriteNames.Num(); ++i) R_InstallSprite(*VClass::GSpriteNames[i], i);

  ServerNetContext = new VServerNetContext();

  VClass *PlayerClass = VClass::FindClass("Player");
  for (int i = 0; i < MAXPLAYERS; ++i) {
    GPlayersBase[i] = (VBasePlayer *)VObject::StaticSpawnWithReplace(PlayerClass);
    if (developer) GCon->Logf(NAME_Dev, "spawned base player object for player #%d, with actual class <%s>", i, *GPlayersBase[i]->GetClass()->GetFullName());
  }

  GGameInfo->validcount = &validcount;
  GGameInfo->skyflatnum = skyflatnum;

  P_InitSwitchList();
  P_InitTerrainTypes();
  InitLockDefs();

  VMemberBase::StaticCompilerShutdown();
  CompilerReportMemory();
}


//==========================================================================
//
//  SV_ResetPlayers
//
//==========================================================================
void SV_ResetPlayers () {
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (GPlayersBase[i]) {
      GPlayersBase[i]->ResetButtons();
      GPlayersBase[i]->eventResetToDefaults();
    }
  }
}


//==========================================================================
//
//  SV_ResetPlayerButtons
//
//==========================================================================
void SV_ResetPlayerButtons () {
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (GPlayersBase[i]) GPlayersBase[i]->ResetButtons();
  }
}


//==========================================================================
//
//  SV_SendLoadedEvent
//
//==========================================================================
void SV_SendLoadedEvent () {
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (GPlayersBase[i]) GPlayersBase[i]->eventOnSaveLoaded();
  }
}


//==========================================================================
//
//  SV_SendBeforeSaveEvent
//
//==========================================================================
void SV_SendBeforeSaveEvent (bool isAutosave, bool isCheckpoint) {
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (GPlayersBase[i]) GPlayersBase[i]->eventOnBeforeSave(isAutosave, isCheckpoint);
  }
}


//==========================================================================
//
//  SV_SendAfterSaveEvent
//
//==========================================================================
void SV_SendAfterSaveEvent (bool isAutosave, bool isCheckpoint) {
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (GPlayersBase[i]) GPlayersBase[i]->eventOnAfterSave(isAutosave, isCheckpoint);
  }
}


//==========================================================================
//
//  P_InitThinkers
//
//==========================================================================
void P_InitThinkers () {
}


//==========================================================================
//
//  SV_Shutdown
//
//==========================================================================
void SV_Shutdown () {
  if (GGameInfo) {
    SV_ShutdownGame();
    GGameInfo->ConditionalDestroy();
  }
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (GPlayersBase[i]) {
      delete GPlayersBase[i]->Net;
      GPlayersBase[i]->Net = nullptr;
      GPlayersBase[i]->ConditionalDestroy();
    }
  }

  P_FreeTerrainTypes();
  ShutdownLockDefs();
  svs.serverinfo.Clean();

  delete ServerNetContext;
  ServerNetContext = nullptr;
}


//==========================================================================
//
//  SV_Clear
//
//==========================================================================
void SV_Clear () {
  if (GLevel) {
    for (int i = 0; i < svs.max_clients; ++i) {
      VBasePlayer *Player = GGameInfo->Players[i];
      if (!Player) continue;
      if (Player->Net) Player->Net->ResetLevel();
    }

    if (GDemoRecordingContext) {
      for (int f = 0; f < GDemoRecordingContext->ClientConnections.length(); ++f) {
        //GDemoRecordingContext->ClientConnections[f]->Driver->SetNetTime();
        GDemoRecordingContext->ClientConnections[f]->ResetLevel();
      }
    }

    GLevel->ConditionalDestroy();
    GLevel = nullptr;
    VObject::CollectGarbage();
  }
  memset(&sv, 0, sizeof(sv));
#ifdef CLIENT
  // make sure all sounds are stopped
  GAudio->StopAllSound();
#endif
}


//==========================================================================
//
//  SV_SendClientMessages
//
//==========================================================================
void SV_SendClientMessages () {
  // update player replication infos
  for (int i = 0; i < svs.max_clients; ++i) {
    VBasePlayer *Player = GGameInfo->Players[i];
    if (!Player) continue;
    if (Player->GetFlags()&_OF_Destroyed) continue;

    VPlayerReplicationInfo *RepInfo = Player->PlayerReplicationInfo;
    if (!RepInfo) continue;

    RepInfo->PlayerName = Player->PlayerName;
    RepInfo->UserInfo = Player->UserInfo;
    RepInfo->TranslStart = Player->TranslStart;
    RepInfo->TranslEnd = Player->TranslEnd;
    RepInfo->Color = Player->Color;
    RepInfo->Frags = Player->Frags;
    RepInfo->Deaths = Player->Deaths;
    RepInfo->KillCount = Player->KillCount;
    RepInfo->ItemCount = Player->ItemCount;
    RepInfo->SecretCount = Player->SecretCount;

    // update view angle if needed
    if (Player->PlayerFlags&VBasePlayer::PF_Spawned) Player->WriteViewData();
  }

  ServerNetContext->Tick();

  if (GDemoRecordingContext) {
    for (int i = 0; i < GDemoRecordingContext->ClientConnections.length(); ++i) {
      //GDemoRecordingContext->ClientConnections[i]->Driver->SetNetTime();
      GDemoRecordingContext->ClientConnections[i]->NeedsUpdate = true;
    }
    GDemoRecordingContext->Tick();
  }
}


//========================================================================
//
//  CheckForSkip
//
//  Check to see if any player hit a key
//
//========================================================================
static void CheckForSkip () {
  VBasePlayer *player;
  static bool triedToSkip;
  bool skip = false;

  /*
  if (GDemoRecordingContext) {
    skip = true;
  } else
  */
  {
    for (int i = 0; i < MAXPLAYERS; ++i) {
      player = GGameInfo->Players[i];
      if (player) {
        if (player->Buttons&BT_ATTACK) {
          if (!(player->PlayerFlags&VBasePlayer::PF_AttackDown)) skip = true;
          player->PlayerFlags |= VBasePlayer::PF_AttackDown;
        } else {
          player->PlayerFlags &= ~VBasePlayer::PF_AttackDown;
        }
        if (player->Buttons&BT_USE) {
          if (!(player->PlayerFlags&VBasePlayer::PF_UseDown)) skip = true;
          player->PlayerFlags |= VBasePlayer::PF_UseDown;
        } else {
          player->PlayerFlags &= ~VBasePlayer::PF_UseDown;
        }
      }
    }

    if (deathmatch && sv.intertime < 140) {
      // wait for 4 seconds before allowing a skip
      if (skip) {
        triedToSkip = true;
        skip = false;
      }
    } else {
      if (triedToSkip) {
        skip = true;
        triedToSkip = false;
      }
    }
  }

  if (skip) {
    for (int i = 0; i < svs.max_clients; ++i) {
      if (GGameInfo->Players[i]) {
        GGameInfo->Players[i]->eventClientSkipIntermission();
      }
    }
  }
}


//==========================================================================
//
//  SV_RunPlayerTick
//
//  do all necessary checks *BEFORE* calling this function
//
//==========================================================================
static void SV_RunPlayerTick (VBasePlayer *Player, bool skipFrame) {
  Player->ForwardMove = (skipFrame && dbg_skipframe_player_block_move ? 0 : Player->ClientForwardMove);
  Player->SideMove = (skipFrame && dbg_skipframe_player_block_move ? 0 : Player->ClientSideMove);
  //if (Player->ForwardMove) GCon->Logf("ffm: %f (%d)", Player->ClientForwardMove, (int)skipFrame);
  // don't move faster than maxmove
       if (Player->ForwardMove > sv_maxmove) Player->ForwardMove = sv_maxmove;
  else if (Player->ForwardMove < -sv_maxmove) Player->ForwardMove = -sv_maxmove;
       if (Player->SideMove > sv_maxmove) Player->SideMove = sv_maxmove;
  else if (Player->SideMove < -sv_maxmove) Player->SideMove = -sv_maxmove;
  // check for disabled freelook and jumping
  if (!sv_ignore_nomlook && (GLevelInfo->LevelInfoFlags&VLevelInfo::LIF_NoFreelook)) Player->ViewAngles.pitch = 0;
  if (!sv_ignore_nojump && (GLevelInfo->LevelInfoFlags&VLevelInfo::LIF_NoJump)) Player->Buttons &= ~BT_JUMP;
  if (!sv_ignore_nocrouch && (GLevelInfo->LevelInfoFlags&VLevelInfo::LIF2_NoCrouch)) Player->Buttons &= ~BT_JUMP;
  if (cl_disable_crouch) Player->Buttons &= ~BT_CROUCH;
  if (cl_disable_jump) Player->Buttons &= ~BT_JUMP;
  if (cl_disable_mlook) Player->ViewAngles.pitch = 0;
  //GCon->Logf("*** 000: PLAYER TICK(%p) ***: Buttons=0x%08x; OldButtons=0x%08x", Player, Player->Buttons, Player->OldButtons);
  Player->eventPlayerTick(host_frametime);
  //GCon->Logf("*** 001: PLAYER TICK(%p) ***: Buttons=0x%08x; OldButtons=0x%08x", Player, Player->Buttons, Player->OldButtons);
  // new logic for client buttons update
  //Player->OldButtons = Player->AcsButtons;
  //Player->OldViewAngles = Player->ViewAngles;
  // latch logic
  //if ((Player->AcsNextButtonUpdate -= host_frametime) <= 0.0f)
  if (Player->AcsNextButtonUpdate <= GLevel->TicTime) {
    //Player->AcsNextButtonUpdate = 1.0f/35.0f; // once per standard DooM frame
    //!Player->AcsNextButtonUpdate = 1.0f/36.0f; // once per standard DooM frame
    Player->AcsNextButtonUpdate = GLevel->TicTime+1;
    Player->OldButtons = Player->AcsButtons;
    // now create new acs buttons
    Player->AcsButtons = Player->AcsCurrButtonsPressed;
    Player->AcsCurrButtonsPressed = Player->AcsCurrButtons;
    // mouse movement
    Player->AcsMouseX = Player->AcsPrevMouseX;
    Player->AcsMouseY = Player->AcsPrevMouseY;
    Player->AcsPrevMouseX = 0;
    Player->AcsPrevMouseY = 0;
  }
}


//==========================================================================
//
//  SV_RunClients
//
//==========================================================================
static void SV_RunClients (bool skipFrame=false) {
  // get commands
  for (int i = 0; i < MAXPLAYERS; ++i) {
    VBasePlayer *Player = GGameInfo->Players[i];
    if (!Player) continue;

    if ((Player->PlayerFlags&VBasePlayer::PF_IsBot) &&
        !(Player->PlayerFlags&VBasePlayer::PF_Spawned))
    {
      Player->SpawnClient();
    }

    // do player reborns if needed
         if (Player->PlayerState == PST_REBORN) G_DoReborn(i, false);
    else if (Player->PlayerState == PST_CHEAT_REBORN) G_DoReborn(i, true);

    if (Player->Net) {
      Player->Net->NeedsUpdate = false;
      Player->Net->GetMessages();
    }

    // pause if in menu or console and at least one tic has been run
    if ((Player->PlayerFlags&VBasePlayer::PF_Spawned) && !sv.intermission && !GGameInfo->IsPaused()) {
      if (dbg_vm_disable_thinkers) {
        if (Player->PlayerFlags&VBasePlayer::PF_IsBot) continue;
      }
      SV_RunPlayerTick(Player, skipFrame);
    }
  }

  //GCon->Logf("*** IMS: %d (demo=%p : %d)", (int)sv.intermission, GDemoRecordingContext, (int)cls.demorecording);
  if (sv.intermission) {
#ifdef CLIENT
    if (GDemoRecordingContext) {
      if (cls.demorecording) {
        CL_KeepaliveMessage();
        for (int f = 0; f < GDemoRecordingContext->ClientConnections.length(); ++f) {
          //GDemoRecordingContext->ClientConnections[f]->Driver->NetTime = 0; //Sys_Time()+1000;
          //GDemoRecordingContext->ClientConnections[f]->Flush();
          GDemoRecordingContext->ClientConnections[f]->Intermission(true);
        }
      }
      /*
      for (int f = 0; f < GDemoRecordingContext->ClientConnections.length(); ++f) {
        //GDemoRecordingContext->ClientConnections[f]->Driver->NetTime = 0; //Sys_Time()+1000;
        //GDemoRecordingContext->ClientConnections[f]->Flush();
        GDemoRecordingContext->ClientConnections[f]->ResetLevel();
      }
      */
    }
#endif
    CheckForSkip();
    ++sv.intertime;
  }

  if (!sv.intermission) {
#ifdef CLIENT
    if (GDemoRecordingContext) {
      if (cls.demorecording) {
        for (int f = 0; f < GDemoRecordingContext->ClientConnections.length(); ++f) {
          //GDemoRecordingContext->ClientConnections[f]->Driver->NetTime = 0; //Sys_Time()+1000;
          //GDemoRecordingContext->ClientConnections[f]->Flush();
          GDemoRecordingContext->ClientConnections[f]->Intermission(false);
        }
      }
    }
#endif
  }
}


//==========================================================================
//
//  SV_Ticker
//
//==========================================================================
void SV_Ticker () {
  //double saved_frametime;

  if (GGameInfo->NetMode >= NM_DedicatedServer &&
      (!LastMasterUpdate || host_time-LastMasterUpdate > master_heartbeat_time))
  {
    GNet->UpdateMaster();
    LastMasterUpdate = host_time;
  }

  if (host_frametime <= 0) return;

  //saved_frametime = host_frametime;

  if (host_frametime < max_fps_cap_double) {
    host_framefrac += host_frametime;
    host_frametime = 0;
    return;
  }

  int scap = host_max_skip_frames;
  if (scap < 3) scap = 3;

  //exec_times = 1;
  if (!real_time) {
    // rounded a little bit up to prevent "slow motion"
    host_frametime = FrameTime; //0.02857142857142857142857142857143; //1.0 / 35.0;
  } else if (split_frame && host_frametime > FrameTime) {
    double i;
    /*double frc =*/ (void)modf(host_frametime/FrameTime, &i);
    //GCon->Logf("*** ft=%f; frt=%f; int=%f; frc=%f", host_frametime, FrameTime, i, frc);
    if (i < 1) { GCon->Logf(NAME_Error, "WTF?! i must be at least one, but it is %f", i); i = 1; }
    int exec_times = (i > 0x1fffffff ? 0x1fffffff : (int)i);
    {
      static int showExecTimes = -1;
      if (showExecTimes < 0) showExecTimes = (GArgs.CheckParm("-show-exec-times") ? 1 : 0);
      if (showExecTimes) {
        if (exec_times <= scap) GCon->Logf("exec_times=%d", exec_times); else GCon->Logf("exec_times=%d (capped to %d)", exec_times, scap);
      }
    }
    // cap
    if (exec_times > scap) {
      exec_times = scap;
      host_frametime = FrameTime*exec_times;
    }
  }

  if (sv_loading || sv.intermission) {
    GGameInfo->frametime = host_frametime;
    SV_RunClients();
  } else {
    double saved_frametime = host_frametime;
    bool frameSkipped = false;
    bool wasPaused = false;
    // do main actions
    double frametimeleft = host_frametime;
    int lastTick = GLevel->TicTime;
    while (!sv.intermission && !completed && frametimeleft >= max_fps_cap_double) {
      if (GLevel->TicTime != lastTick) {
        lastTick = GLevel->TicTime;
        VObject::CollectGarbage();
      }
      // calculate frame time
      // do small steps, it seems to work better this way
      double currframetime = (split_frame && frametimeleft >= FrameTime*0.4 ? 1.0/35.0*0.4 : frametimeleft);
      if (currframetime > frametimeleft) currframetime = frametimeleft;
      // do it this way, because of rounding
      GGameInfo->frametime = currframetime;
      host_frametime = GGameInfo->frametime;
      if (GGameInfo->IsPaused()) {
        // no need to do anything more if the game is paused
        if (!frameSkipped) SV_RunClients();
        wasPaused = true;
        break;
      }
      // advance player states, so weapons won't slow down on frame skip
      if (!frameSkipped || dbg_skipframe_player_tick) {
        SV_RunClients(frameSkipped); // have to make a full run, for demos/network (k8: is it really necessary?)
      }
      GLevel->TickWorld(host_frametime);
      //GCon->Logf("%d: ft=%f; ftleft=%f; Time=%f; tics=%d", (int)frameSkipped, host_frametime, oldft-GGameInfo->frametime, GLevel->Time, (int)GLevel->TicTime);
      // level timer
      if (TimerGame && TimerGame >= GLevel->TicTime) {
        TimerGame = 0;
        LeavePosition = 0;
        completed = true;
      }
      frametimeleft -= host_frametime /*currframetime*/; // next step
      frameSkipped = true;
    }
    if (completed) G_DoCompleted();
    // remember fractional frame time
    host_frametime = saved_frametime;
    if (!wasPaused) {
      if (!sv.intermission && !completed && frametimeleft > 0 && frametimeleft < max_fps_cap_double) {
        host_framefrac += frametimeleft;
        host_frametime -= frametimeleft;
      }
    }
  }

  //host_frametime = saved_frametime;
}


//==========================================================================
//
//  CheckRedirects
//
//==========================================================================
static VName CheckRedirects (VName Map) {
  const mapInfo_t &Info = P_GetMapInfo(Map);
  if (Info.RedirectType == NAME_None || Info.RedirectMap == NAME_None) return Map; // no redirect for this map
  // check all players
  for (int i = 0; i < MAXPLAYERS; ++i) {
    VBasePlayer *P = GGameInfo->Players[i];
    if (!P || !(P->PlayerFlags&VBasePlayer::PF_Spawned)) continue;
    if (P->MO->eventCheckInventory(Info.RedirectType) > 0) return CheckRedirects(Info.RedirectMap);
  }
  // none of the players have required item, no redirect
  return Map;
}


//==========================================================================
//
//  G_DoCompleted
//
//==========================================================================
static void G_DoCompleted () {
  completed = false;
  if (sv.intermission) return;

  if (NoExit /*&& deathmatch*/ && (GGameInfo->NetMode == NM_DedicatedServer || GGameInfo->NetMode == NM_ListenServer)) {
    return;
  }

  if (GGameInfo->NetMode < NM_DedicatedServer &&
      (!GGameInfo->Players[0] || !(GGameInfo->Players[0]->PlayerFlags&VBasePlayer::PF_Spawned)))
  {
    //FIXME: some ACS left from previous visit of the level
    return;
  }

  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (GGameInfo->Players[i]) GGameInfo->Players[i]->eventPlayerBeforeExitMap();
  }

#ifdef CLIENT
  SV_AutoSaveOnLevelExit();
#endif

  sv.intermission = 1;
  sv.intertime = 0;
  GLevelInfo->CompletitionTime = GLevel->Time;

  GLevel->Acs->StartTypedACScripts(SCRIPT_Unloading, 0, 0, 0, nullptr, false, true);

  GLevelInfo->NextMap = CheckRedirects(GLevelInfo->NextMap);

  const mapInfo_t &old_info = P_GetMapInfo(GLevel->MapName);
  const mapInfo_t &new_info = P_GetMapInfo(GLevelInfo->NextMap);
  const VClusterDef *ClusterD = P_GetClusterDef(old_info.Cluster);
  bool HubChange = (!old_info.Cluster || !(ClusterD->Flags&CLUSTERF_Hub) || old_info.Cluster != new_info.Cluster);

  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (GGameInfo->Players[i]) {
      GGameInfo->Players[i]->eventPlayerExitMap(HubChange);
      if (deathmatch || HubChange) {
        GGameInfo->Players[i]->eventClientIntermission(GLevelInfo->NextMap);
      }
    }
  }

  if (!deathmatch && !HubChange) GCmdBuf << "TeleportNewMap\n";
}


//==========================================================================
static const char *knownFinalesList[] = {
  "EndGameBunny",
  "EndGameCast",
  "EndGameChess",
  "EndGameDemon",
  "EndGamePic1",
  "EndGamePic2",
  "EndGamePic3",
  "EndGameStrife",
  "EndGameUnderwater",
  nullptr,
};


//==========================================================================
//
//  COMMAND TestFinale
//
//==========================================================================
COMMAND_WITH_AC(TestFinale) {
  if (Source == SRC_Command) {
    ForwardToServer();
    return;
  }

  if (Args.length() != 2) return;

  // normalise finale name
  VStr fname = Args[1];
  for (const char **fin = knownFinalesList; *fin; ++fin) {
    if (fname.strEquCI(*fin)) {
      fname = VStr(*fin);
      break;
    }
  }

  if (GGameInfo->NetMode == NM_Standalone && !deathmatch) {
    for (int i = 0; i < svs.max_clients; ++i) {
      if (GGameInfo->Players[i]) {
        GGameInfo->Players[i]->eventClientFinale(fname);
      }
    }
    sv.intermission = 2;
  }

  //if (GGameInfo->NetMode == NM_Standalone) SV_UpdateRebornSlot(); // copy the base slot to the reborn slot
}


//==========================================================================
//
//  COMMAND_AC TestFinale
//
//==========================================================================
COMMAND_AC(TestFinale) {
  TArray<VStr> list;
  VStr prefix = (aidx < args.length() ? args[aidx] : VStr());
  if (aidx == 1) {
    for (const char **fin = knownFinalesList; *fin; ++fin) list.append(VStr(*fin));
    return AutoCompleteFromList(prefix, list, true); // return unchanged as empty
  } else {
    return VStr::EmptyString;
  }
}


//==========================================================================
//
//  COMMAND TeleportNewMap
//
//  mapname [leaveposition]
//
//==========================================================================
COMMAND(TeleportNewMap) {
  if (Source == SRC_Command) {
    ForwardToServer();
    return;
  }

  if (GGameInfo->NetMode == NM_None || GGameInfo->NetMode == NM_Client) return;

  if (Args.Num() == 3) {
    GLevelInfo->NextMap = VName(*Args[1], VName::AddLower8);
    LeavePosition = VStr::atoi(*Args[2]);
  } else if (sv.intermission != 1) {
    return;
  }

  if (!deathmatch) {
    if (VStr(GLevelInfo->NextMap).StartsWith("EndGame")) {
      for (int i = 0; i < svs.max_clients; ++i) {
        if (GGameInfo->Players[i]) {
          GGameInfo->Players[i]->eventClientFinale(*GLevelInfo->NextMap);
        }
      }
      sv.intermission = 2;
      return;
    }
  }

#ifdef CLIENT
  Draw_TeleportIcon();
#endif

  RebornPosition = LeavePosition;
  GGameInfo->RebornPosition = RebornPosition;
  mapteleport_issued = true;
  mapteleport_flags = 0;
  mapteleport_skill = -1;
  //if (GGameInfo->NetMode == NM_Standalone) SV_UpdateRebornSlot(); // copy the base slot to the reborn slot
}


//==========================================================================
//
//  COMMAND TeleportNewMapEx
//
//  mapname posidx flags [skill]
//
//==========================================================================
COMMAND(TeleportNewMapEx) {
  if (Args.length() < 2) {
    GCon->Logf("TeleportNewMapEx mapname [posidx [flags [skill]]]");
    return;
  }

  if (Source == SRC_Command) {
    ForwardToServer();
    return;
  }

  if (GGameInfo->NetMode == NM_None || GGameInfo->NetMode == NM_Client) return;

  int posidx = 0, flags = 0, skill = -1;
  if (Args.length() > 2) Args[2].convertInt(&posidx);
  if (Args.length() > 3) Args[3].convertInt(&flags);
  if (Args.length() > 4) {
    if (Args[4].convertInt(&skill)) {
      if (skill >= 0) flags |= CHANGELEVEL_CHANGESKILL;
    }
  }

  //GCon->Logf("TeleportNewMapEx: name=<%s>; posidx=%d; flags=0x%04x; skill=%d", *Args[1], posidx, flags, skill);

  GLevelInfo->NextMap = VName(*Args[1], VName::AddLower8);

  if (!deathmatch) {
    if (VStr(GLevelInfo->NextMap).StartsWith("EndGame")) {
      for (int i = 0; i < svs.max_clients; ++i) {
        if (GGameInfo->Players[i]) {
          GGameInfo->Players[i]->eventClientFinale(*GLevelInfo->NextMap);
        }
      }
      sv.intermission = 2;
      return;
    }
  }

#ifdef CLIENT
  Draw_TeleportIcon();
#endif

  RebornPosition = posidx;
  GGameInfo->RebornPosition = RebornPosition;
  mapteleport_issued = true; // this will actually change the map
  mapteleport_flags = flags;
  mapteleport_skill = skill;
  //if (GGameInfo->NetMode == NM_Standalone) SV_UpdateRebornSlot(); // copy the base slot to the reborn slot
}


//==========================================================================
//
//  G_DoReborn
//
//==========================================================================
static void G_DoReborn (int playernum, bool cheatReborn) {
  if (!GGameInfo->Players[playernum] ||
      !(GGameInfo->Players[playernum]->PlayerFlags&VBasePlayer::PF_Spawned))
  {
    return;
  }
  if (GGameInfo->NetMode == NM_Standalone && !cheatReborn) {
    GCmdBuf << "Restart\n";
    GGameInfo->Players[playernum]->PlayerState = PST_LIVE;
  } else {
    GGameInfo->Players[playernum]->eventNetGameReborn();
  }
}


//==========================================================================
//
//  NET_SendToAll
//
//==========================================================================
int NET_SendToAll (int blocktime) {
  double start;
  int count = 0;
  bool state1[MAXPLAYERS];
  bool state2[MAXPLAYERS];

  for (int i = 0; i < svs.max_clients; ++i) {
    VBasePlayer *Player = GGameInfo->Players[i];
    if (Player && Player->Net) {
      if (Player->Net->IsLocalConnection()) {
        state1[i] = false;
        state2[i] = true;
        continue;
      }
      ++count;
      state1[i] = false;
      state2[i] = false;
    } else {
      state1[i] = true;
      state2[i] = true;
    }
  }

  start = Sys_Time();
  while (count) {
    count = 0;
    for (int i = 0; i < svs.max_clients; ++i) {
      VBasePlayer *Player = GGameInfo->Players[i];
      if (!state1[i]) {
        state1[i] = true;
        Player->Net->Channels[0]->Close();
        ++count;
        continue;
      }

      if (!state2[i]) {
        if (Player->Net->State == NETCON_Closed) {
          state2[i] = true;
        } else {
          Player->Net->GetMessages();
          Player->Net->Tick();
        }
        ++count;
        continue;
      }
    }
    if ((Sys_Time()-start) > blocktime) break;
  }

  return count;
}


//==========================================================================
//
//  SV_SendServerInfoToClients
//
//==========================================================================
void SV_SendServerInfoToClients () {
  for (int i = 0; i < svs.max_clients; ++i) {
    VBasePlayer *Player = GGameInfo->Players[i];
    if (!Player) continue;
    Player->Level = GLevelInfo;
    if (Player->Net) {
      Player->Net->LoadedNewLevel();
      Player->Net->SendServerInfo();
    } else {
      //GCon->Logf("SERVER: player #%d has no network connection", i);
    }
  }

  if (GDemoRecordingContext) {
    for (int f = 0; f < GDemoRecordingContext->ClientConnections.length(); ++f) {
      //GDemoRecordingContext->ClientConnections[f]->Driver->SetNetTime();
      GDemoRecordingContext->ClientConnections[f]->LoadedNewLevel();
      GDemoRecordingContext->ClientConnections[f]->SendServerInfo();
    }
  }
}


//==========================================================================
//
//  SV_SpawnServer
//
//==========================================================================
void SV_SpawnServer (const char *mapname, bool spawn_thinkers, bool titlemap) {
  GCon->Log("===============================================");
  GCon->Logf("Spawning server with \"%s\"", mapname);
  GGameInfo->Flags &= ~VGameInfo::GIF_Paused;
  mapteleport_issued = false;
  mapteleport_flags = 0;
  mapteleport_skill = -1;
  run_open_scripts = spawn_thinkers;

  if (GGameInfo->NetMode != NM_None) {
    //fprintf(stderr, "SV_SpawnServer!!!\n");
    // level change
    for (int i = 0; i < MAXPLAYERS; ++i) {
      if (!GGameInfo->Players[i]) continue;

      GGameInfo->Players[i]->KillCount = 0;
      GGameInfo->Players[i]->SecretCount = 0;
      GGameInfo->Players[i]->ItemCount = 0;

      GGameInfo->Players[i]->PlayerFlags &= ~VBasePlayer::PF_Spawned;
      GGameInfo->Players[i]->MO = nullptr;
      GGameInfo->Players[i]->Frags = 0;
      GGameInfo->Players[i]->Deaths = 0;
      if (GGameInfo->Players[i]->PlayerState == PST_DEAD) GGameInfo->Players[i]->PlayerState = PST_REBORN;
    }
  } else {
    // new game
    deathmatch = DeathMatch;
    GGameInfo->deathmatch = deathmatch;

    P_InitThinkers();

#ifdef CLIENT
    GGameInfo->NetMode = (titlemap ? NM_TitleMap : svs.max_clients == 1 ? NM_Standalone : NM_ListenServer);
#else
    GGameInfo->NetMode = NM_DedicatedServer;
#endif

    GGameInfo->WorldInfo = GGameInfo->eventCreateWorldInfo();

    GGameInfo->WorldInfo->SetSkill(Skill);
    GGameInfo->eventInitNewGame(GGameInfo->WorldInfo->GameSkill);
  }

  SV_Clear();
  //GCon->Log("*** UNLATCH ***");
  VCvar::Unlatch();

  // load it
  SV_LoadLevel(VName(mapname, VName::AddLower8));
  GLevel->NetContext = ServerNetContext;
  GLevel->WorldInfo = GGameInfo->WorldInfo;

  const mapInfo_t &info = P_GetMapInfo(GLevel->MapName);

  if (spawn_thinkers) {
    // create level info
    GLevelInfo = (VLevelInfo *)GLevel->SpawnThinker(GGameInfo->LevelInfoClass);
    GLevelInfo->Level = GLevelInfo;
    GLevelInfo->Game = GGameInfo;
    GLevelInfo->World = GGameInfo->WorldInfo;
    GLevel->LevelInfo = GLevelInfo;
    GLevelInfo->SetMapInfo(info);

    // spawn things
    for (int i = 0; i < GLevel->NumThings; ++i) GLevelInfo->eventSpawnMapThing(&GLevel->Things[i]);
    if (deathmatch && GLevelInfo->DeathmatchStarts.length() < 4) Host_Error("Level needs more deathmatch start spots");
  }

  if (deathmatch) {
    TimerGame = TimeLimit*35*60;
  } else {
    TimerGame = 0;
  }

  // set up world state
  Host_ResetSkipFrames();

  if (!spawn_thinkers) {
    SV_SendServerInfoToClients(); // anyway
    return;
  }

  // P_SpawnSpecials
  // after the map has been loaded, scan for specials that spawn thinkers
  GLevelInfo->eventSpawnSpecials();

  SV_SendServerInfoToClients();

  // call BeginPlay events
  for (TThinkerIterator<VEntity> Ent(GLevel); Ent; ++Ent) Ent->eventBeginPlay();
  GLevelInfo->LevelInfoFlags2 |= VLevelInfo::LIF2_BegunPlay;

  Host_ResetSkipFrames();

  if (GGameInfo->NetMode != NM_TitleMap && GGameInfo->NetMode != NM_Standalone) {
    GLevel->TickWorld(FrameTime);
    GLevel->TickWorld(FrameTime);
    // start open scripts
    GLevel->Acs->StartTypedACScripts(SCRIPT_Open, 0, 0, 0, nullptr, false, false);
  }

  GCon->Log(NAME_Dev, "Server spawned");
}


//==========================================================================
//
//  COMMAND PreSpawn
//
//==========================================================================
COMMAND(PreSpawn) {
  if (Source == SRC_Command) {
    GCon->Log("PreSpawn is not valid from console");
    return;
  }
  // make sure level info is spawned on client side, since there could be some RPCs that depend on it
  VThinkerChannel *Chan = Player->Net->ThinkerChannels.FindPtr(GLevelInfo);
  if (!Chan) {
    Chan = (VThinkerChannel *)Player->Net->CreateChannel(CHANNEL_Thinker, -1);
    if (Chan) {
      Chan->SetThinker(GLevelInfo);
      Chan->Update();
    }
  }
}


//==========================================================================
//
//  COMMAND Spawn
//
//==========================================================================
COMMAND(Client_Spawn) {
  if (Source == SRC_Command) {
    GCon->Log("Client_Spawn is not valid from console");
    return;
  }
  Player->SpawnClient();
}


//==========================================================================
//
//  SV_DropClient
//
//==========================================================================
void SV_DropClient (VBasePlayer *Player, bool crash) {
  if (!crash) {
    if (GLevel && GLevel->Acs) {
      GLevel->Acs->StartTypedACScripts(SCRIPT_Disconnect, SV_GetPlayerNum(Player), 0, 0, nullptr, true, false);
    }
    if (Player->PlayerFlags & VBasePlayer::PF_Spawned) Player->eventDisconnectClient();
  }
  Player->PlayerFlags &= ~VBasePlayer::PF_Active;
  GGameInfo->Players[SV_GetPlayerNum(Player)] = nullptr;
  Player->PlayerFlags &= ~VBasePlayer::PF_Spawned;

  if (Player->PlayerReplicationInfo) Player->PlayerReplicationInfo->DestroyThinker();

  delete Player->Net;
  Player->Net = nullptr;

  --svs.num_connected;
  Player->UserInfo = VStr();
}


//==========================================================================
//
//  SV_ShutdownGame
//
//  This only happens at the end of a game, not between levels
//  This is also called on Host_Error, so it shouldn't cause any errors
//
//==========================================================================
void SV_ShutdownGame () {
  if (GGameInfo->NetMode == NM_None) return;

#ifdef CLIENT
  if (GGameInfo->Flags&VGameInfo::GIF_Paused) {
    GGameInfo->Flags &= ~VGameInfo::GIF_Paused;
    GAudio->ResumeSound();
  }

  // stop sounds (especially looping!)
  GAudio->StopAllSound();

  if (cls.demorecording) CL_StopRecording();
#endif

  if (GGameInfo->NetMode == NM_Client) {
#ifdef CLIENT
    if (cls.demoplayback) GClGame->eventDemoPlaybackStopped();

    // sends a disconnect message to the server
    if (!cls.demoplayback) {
      GCon->Log(NAME_Dev, "Sending clc_disconnect");
      if (cl->Net) {
        if (cl->Net->Channels[0]) cl->Net->Channels[0]->Close();
        cl->Net->Flush();
      }
    }

    delete cl->Net;
    cl->Net = nullptr;
    cl->ConditionalDestroy();

    if (GClLevel) {
      delete GClLevel;
      GClLevel = nullptr;
    }
#endif
  } else {
    sv_loading = false;
    sv_map_travel = false;

    // make sure all the clients know we're disconnecting
    int count = NET_SendToAll(5);
    if (count) GCon->Logf("Shutdown server failed for %d clients", count);

    for (int i = 0; i < svs.max_clients; ++i) {
      if (GGameInfo->Players[i]) SV_DropClient(GGameInfo->Players[i], false);
    }

    // clear structures
    if (GLevel) {
      delete GLevel;
      GLevel = nullptr;
    }
    if (GGameInfo->WorldInfo) {
      delete GGameInfo->WorldInfo;
      GGameInfo->WorldInfo = nullptr;
    }
    for (int i = 0; i < MAXPLAYERS; ++i) {
      // save net pointer
      VNetConnection *OldNet = GPlayersBase[i]->Net;
      GPlayersBase[i]->GetClass()->DestructObject(GPlayersBase[i]);
      memset((vuint8 *)GPlayersBase[i]+sizeof(VObject), 0, GPlayersBase[i]->GetClass()->ClassSize-sizeof(VObject));
      // restore pointer
      GPlayersBase[i]->Net = OldNet;
    }
    memset(GGameInfo->Players, 0, sizeof(GGameInfo->Players));
    memset(&sv, 0, sizeof(sv));

    // tell master server that this server is gone
    if (GGameInfo->NetMode >= NM_DedicatedServer) {
      GNet->QuitMaster();
      LastMasterUpdate = 0;
    }
  }

#ifdef CLIENT
  GClLevel = nullptr;
  cl = nullptr;
  cls.demoplayback = false;
  cls.signon = 0;

  if (GGameInfo->NetMode != NM_DedicatedServer) GClGame->eventDisconnected();
#endif

  SV_InitBaseSlot();

  GGameInfo->NetMode = NM_None;
}


#ifdef CLIENT
//==========================================================================
//
//  COMMAND Restart
//
//==========================================================================
COMMAND(Restart) {
  //fprintf(stderr, "*****RESTART!\n");
  if (GGameInfo->NetMode != NM_Standalone) return;
  //if (!SV_LoadQuicksaveSlot())
  {
    // reload the level from scratch
    SV_SpawnServer(*GLevel->MapName, true/*spawn thinkers*/);
    if (GGameInfo->NetMode != NM_DedicatedServer) CL_SetUpLocalPlayer();
  }
}
#endif


//==========================================================================
//
//  COMMAND Pause
//
//==========================================================================
COMMAND(Pause) {
  if (Source == SRC_Command) {
    ForwardToServer();
    return;
  }

  if (!__dbg_cl_always_allow_pause) {
    if (GGameInfo->NetMode != NM_Standalone || svs.max_clients > 1) {
      if (Player) Player->Printf("%s", "You cannot pause in network game, sorry.");
      return;
    }
  }

  GGameInfo->Flags ^= VGameInfo::GIF_Paused;
  for (int i = 0; i < svs.max_clients; ++i) {
    if (GGameInfo->Players[i]) {
      GGameInfo->Players[i]->eventClientPause(!!(GGameInfo->Flags&VGameInfo::GIF_Paused));
    }
  }
}


//==========================================================================
//
//  COMMAND Stats
//
//==========================================================================
COMMAND(Stats) {
  if (Source == SRC_Command) {
    ForwardToServer();
    return;
  }
  if (Player) {
    Player->Printf("Kills: %d of %d", Player->KillCount, GLevelInfo->TotalKills);
    Player->Printf("Items: %d of %d", Player->ItemCount, GLevelInfo->TotalItems);
    Player->Printf("Secrets: %d of %d", Player->SecretCount, GLevelInfo->TotalSecret);
  }
}


//==========================================================================
//
//  SV_ConnectClient
//
//  initialises a client_t for a new net connection
//  this will only be called once for a player each game, not once
//  for each level change
//
//==========================================================================
void SV_ConnectClient (VBasePlayer *player) {
  if (player->Net) {
    GCon->Logf(NAME_Dev, "Client %s connected", *player->Net->GetAddress());
    ServerNetContext->ClientConnections.Append(player->Net);
    player->Net->NeedsUpdate = false;
  }

  GGameInfo->Players[SV_GetPlayerNum(player)] = player;
  player->ClientNum = SV_GetPlayerNum(player);
  player->PlayerFlags |= VBasePlayer::PF_Active;

  player->PlayerFlags &= ~VBasePlayer::PF_Spawned;
  player->Level = GLevelInfo;
  if (!sv_loading) {
    player->MO = nullptr;
    player->PlayerState = PST_REBORN;
    player->eventPutClientIntoServer();
  }
  player->Frags = 0;
  player->Deaths = 0;

  player->PlayerReplicationInfo = (VPlayerReplicationInfo *)GLevel->SpawnThinker(GGameInfo->PlayerReplicationInfoClass);
  player->PlayerReplicationInfo->Player = player;
  player->PlayerReplicationInfo->PlayerNum = SV_GetPlayerNum(player);
}


//==========================================================================
//
//  SV_CheckForNewClients
//
//==========================================================================
void SV_CheckForNewClients () {
  VSocketPublic *sock;
  int i;

  // check for new connections
  while (1) {
    sock = GNet->CheckNewConnections();
    if (!sock) break;

    // init a new client structure
    for (i = 0; i < svs.max_clients; ++i) if (!GGameInfo->Players[i]) break;
    if (i == svs.max_clients) Sys_Error("Host_CheckForNewClients: no free clients");

    VBasePlayer *Player = GPlayersBase[i];
    Player->Net = new VNetConnection(sock, ServerNetContext, Player);
    Player->Net->ObjMap->SetUpClassLookup();
    ((VPlayerChannel *)Player->Net->Channels[CHANIDX_Player])->SetPlayer(Player);
    Player->Net->CreateChannel(CHANNEL_ObjectMap, -1);
    SV_ConnectClient(Player);
    ++svs.num_connected;
  }
}


//==========================================================================
//
//  SV_ConnectBot
//
//==========================================================================
void SV_ConnectBot (const char *name) {
  int i;

  if (GGameInfo->NetMode == NM_None || GGameInfo->NetMode == NM_Client) {
    GCon->Log("Game is not running");
    return;
  }

  if (svs.num_connected >= svs.max_clients) {
    GCon->Log("Server is full");
    return;
  }

  // init a new client structure
  for (i = 0; i < svs.max_clients; ++i) if (!GGameInfo->Players[i]) break;
  if (i == svs.max_clients) Sys_Error("SV_ConnectBot: no free clients");

  VBasePlayer *Player = GPlayersBase[i];
  Player->PlayerFlags |= VBasePlayer::PF_IsBot;
  Player->PlayerName = name;
  SV_ConnectClient(Player);
  ++svs.num_connected;
  Player->SetUserInfo(Player->UserInfo);
  Player->SpawnClient();
}


//==========================================================================
//
//  COMMAND AddBot
//
//==========================================================================
COMMAND(AddBot) {
  SV_ConnectBot(Args.Num() > 1 ? *Args[1] : "");
}


//==========================================================================
//
//  Map
//
//==========================================================================
COMMAND(Map) {
  VStr mapname;

  if (Args.Num() != 2) {
    GCon->Log("map <mapname> : change level");
    return;
  }
  mapname = Args[1];

  SV_ShutdownGame();

  // default the player start spot group to 0
  RebornPosition = 0;
  GGameInfo->RebornPosition = RebornPosition;

       if ((int)Skill < 0) Skill = 0;
  else if ((int)Skill >= P_GetNumSkills()) Skill = P_GetNumSkills()-1;

  SV_ResetPlayers();
  SV_SpawnServer(*mapname, true/*spawn thinkers*/);

  if (mapLoaded == LMT_Unknown) {
    if (GLevel->MapName == "e1m1") {
      if (GLevel->MapHashMD5 == "b49f7a6c519757d390d52667db7d8793") {
        mapLoaded = LastLoadedMapType::LMT_E1M1;
      } else {
        mapLoaded = LastLoadedMapType::LMT_OtherFirstD1;
      }
    } else if (GLevel->MapName == "map01") {
      if (GLevel->MapHashMD5 == "3c9902e376cca1e9c3be8763bdc21df5") {
        mapLoaded = LastLoadedMapType::LMT_MAP01;
      } else {
        mapLoaded = LastLoadedMapType::LMT_OtherFirstD2;
      }
    } else {
      mapLoaded = LastLoadedMapType::LMT_Other;
    }
  } else {
    mapLoaded = LastLoadedMapType::LMT_Other;
  }

#ifdef CLIENT
  if (GGameInfo->NetMode != NM_DedicatedServer) CL_SetUpLocalPlayer();
#endif
}


//==========================================================================
//
//  Host_StartTitleMap
//
//==========================================================================
bool Host_StartTitleMap () {
  static bool loadingTitlemap = false;

  if (GArgs.CheckParm("-notitlemap") != 0) return false;
  if (sv_skipOneTitlemap) { sv_skipOneTitlemap = false; return false; }

  if (loadingTitlemap) {
    // it is completely fucked, ignore it
    static bool titlemapWarned = false;
    if (!titlemapWarned) {
      titlemapWarned = true;
      GCon->Log(NAME_Warning, "Your titlemap is fucked, I won't try to load it anymore.");
    }
    return false;
  }

  if (!FL_FileExists("maps/titlemap.wad") && W_CheckNumForName(NAME_titlemap) < 0) return false;

  loadingTitlemap = true;
  // default the player start spot group to 0
  RebornPosition = 0;
  GGameInfo->RebornPosition = RebornPosition;

  SV_ResetPlayers();
  SV_SpawnServer("titlemap", true/*spawn thinkers*/, true/*titlemap*/);
#ifdef CLIENT
  CL_SetUpLocalPlayer();
#endif
  loadingTitlemap = false;
  return true;
}


//==========================================================================
//
//  COMMAND MaxPlayers
//
//==========================================================================
COMMAND(MaxPlayers) {
  if (Args.Num() < 2 || Args.Num() > 3) {
    GCon->Logf("maxplayers is %d", svs.max_clients);
    return;
  }

  if (GGameInfo->NetMode != NM_None && GGameInfo->NetMode != NM_Client) {
    GCon->Log("maxplayers can not be changed while a server is running.");
    return;
  }

  int n = VStr::atoi(*Args[1]);
  if (n < 1) n = 1;
  if (n > MAXPLAYERS) {
    n = MAXPLAYERS;
    GCon->Logf("maxplayers set to %d", n);
  }
  svs.max_clients = n;

  int dmMode = 2;
  if (Args.length() > 2) {
    dmMode = VStr::atoi(*Args[2]);
    if (dmMode < 0 || dmMode > 2) dmMode = 2;
  }

  if (n == 1) {
#ifdef CLIENT
    GCmdBuf << "listen 0\n";
#endif
    DeathMatch = 0;
    NoMonsters = (GArgs.CheckParm("-nomonsters") != 0 ? 1 : 0);
  } else {
#ifdef CLIENT
    GCmdBuf << "listen 1\n";
#endif
    DeathMatch = dmMode;
    if (dmMode) {
      NoMonsters = 1;
    } else {
      //NoMonsters = (dmMode ? 1 : 0);
      NoMonsters = (GArgs.CheckParm("-nomonsters") != 0 ? 1 : 0);
    }
  }
}


//==========================================================================
//
//  ServerFrame
//
//==========================================================================
void ServerFrame (int realtics) {
  SV_CheckForNewClients();

  if (real_time) {
    SV_Ticker();
  } else {
    // run the count dics
    while (realtics--) SV_Ticker();
  }

  if (mapteleport_issued) SV_MapTeleport(GLevelInfo->NextMap, mapteleport_flags, mapteleport_skill);

  SV_SendClientMessages();
}


//==========================================================================
//
//  SV_FindClassFromEditorId
//
//==========================================================================
VClass *SV_FindClassFromEditorId (int Id, int GameFilter) {
  /*
  for (int i = VClass::GMobjInfos.length()-1; i >= 0; --i) {
    if ((!VClass::GMobjInfos[i].GameFilter ||
         (VClass::GMobjInfos[i].GameFilter & GameFilter)) &&
        Id == VClass::GMobjInfos[i].DoomEdNum)
    {
      return VClass::GMobjInfos[i].Class;
    }
  }
  */
  mobjinfo_t *nfo = VClass::FindMObjId(Id, GameFilter);
  //GCon->Logf("SV_FindClassFromEditorId: Id=%d; filter=0x%04x; class=<%s>", Id, GameFilter, (nfo && nfo->Class ? *nfo->Class->GetFullName() : "<oops>"));
  if (nfo) return nfo->Class;
  return nullptr;
}


//==========================================================================
//
//  SV_FindClassFromScriptId
//
//==========================================================================
VClass *SV_FindClassFromScriptId (int Id, int GameFilter) {
  /*
  for (int i = VClass::GScriptIds.length()-1; i >= 0; --i) {
    if ((!VClass::GScriptIds[i].GameFilter ||
         (VClass::GScriptIds[i].GameFilter & GameFilter)) &&
        Id == VClass::GScriptIds[i].DoomEdNum)
    {
      return VClass::GScriptIds[i].Class;
    }
  }
  */
  mobjinfo_t *nfo = VClass::FindScriptId(Id, GameFilter);
  if (nfo) return nfo->Class;
  return nullptr;
}


//==========================================================================
//
//  COMMAND Say
//
//==========================================================================
COMMAND(Say) {
  if (Source == SRC_Command) {
    ForwardToServer();
    return;
  }
  if (Args.Num() < 2) return;

  VStr Text = Player->PlayerName;
  Text += ":";
  for (int i = 1; i < Args.length(); ++i) {
    Text += " ";
    Text += Args[i];
  }
  GLevelInfo->BroadcastPrint(*Text);
  GLevelInfo->StartSound(TVec(0, 0, 0), 0, GSoundManager->GetSoundID("misc/chat"), 0, 1.0f, 0, false);
}


//==========================================================================
//
//  COMMAND gc_toggle_stats
//
//==========================================================================
COMMAND(gc_toggle_stats) {
  VObject::GGCMessagesAllowed = !VObject::GGCMessagesAllowed;
}


//==========================================================================
//
//  COMMAND gc_show_all_objects
//
//==========================================================================
COMMAND(gc_show_all_objects) {
  int total = VObject::GetObjectsCount();
  GCon->Log("===============");
  GCon->Logf("total array size: %d", total);
  for (int f = 0; f < total; ++f) {
    VObject *o = VObject::GetIndexObject(f);
    if (!o) continue;
         if (o->GetFlags()&_OF_Destroyed) GCon->Logf("  #%5d: %p: DESTROYED! (%s)", f, o, o->GetClass()->GetName());
    else if (o->GetFlags()&_OF_DelayedDestroy) GCon->Logf("  #%5d: %p: DELAYED! (%s)", f, o, o->GetClass()->GetName());
    else GCon->Logf("  #%5d: %p: `%s`", f, o, o->GetClass()->GetName());
  }
}


//==========================================================================
//
//  VServerNetContext::GetLevel
//
//==========================================================================
VLevel *VServerNetContext::GetLevel () {
  return GLevel;
}


//**************************************************************************
//
//  Dedicated server console streams
//
//**************************************************************************
#ifndef CLIENT
class FConsoleDevice : public FOutputDevice {
public:
  virtual void Serialise (const char *V, EName) override {
    printf("%s\n", V);
  }
};

FConsoleDevice Console;
FOutputDevice *GCon = &Console;
#endif
