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

// network mode
enum {
  NM_None, // not running a game
  NM_TitleMap, // playing a titlemap
  NM_Standalone, // standalone single player game
  NM_DedicatedServer, // dedicated server, no local client
  NM_ListenServer, // server with local client
  NM_Client, // client only, no local server
};


class VGameInfo : public VGameObject {
  DECLARE_CLASS(VGameInfo, VGameObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VGameInfo)

  VName AcsHelper;
  VName GenericConScript;

  vuint8 NetMode;
  vuint8 deathmatch;
  vuint8 respawn;
  vuint8 nomonsters;
  vuint8 fastparm; // 0:normal; 1:fast; 2:slow

  vint32 *validcount;
  vint32 skyflatnum;

  VWorldInfo *WorldInfo;

  VBasePlayer *Players[MAXPLAYERS]; // bookkeeping on players (state)

  vint32 RebornPosition;

  float frametime;

  float FloatBobOffsets[64];
  vint32 PhaseTable[64];

  VClass *LevelInfoClass;
  VClass *PlayerReplicationInfoClass;

  vint32 GameFilterFlag;

  TArray<VClass *> PlayerClasses;

  enum {
    GIF_DefaultLaxMonsterActivation = 0x00000001,
    GIF_DefaultBloodSplatter        = 0x00000002,
    GIF_Paused                      = 0x00000004,
  };
  vuint32 Flags;

public:
  //VGameInfo ();

  bool IsPaused ();

  void eventInit () {
    P_PASS_SELF;
    EV_RET_VOID("Init");
  }

  void eventPostDecorateInit () {
    P_PASS_SELF;
    EV_RET_VOID("PostDecorateInit");
  }

  void eventInitNewGame (int skill) {
    P_PASS_SELF;
    P_PASS_INT(skill);
    EV_RET_VOID("InitNewGame");
  }

  VWorldInfo *eventCreateWorldInfo () {
    P_PASS_SELF;
    EV_RET_REF(VWorldInfo, "CreateWorldInfo");
  }

  void eventTranslateLevel (VLevel *InLevel) {
    P_PASS_SELF;
    P_PASS_REF(InLevel);
    EV_RET_VOID("TranslateLevel");
  }

  void eventSpawnWorld (VLevel *InLevel) {
    P_PASS_SELF;
    P_PASS_REF(InLevel);
    EV_RET_VOID("SpawnWorld");
  }

  VName eventGetConScriptName (VName LevelName) {
    P_PASS_SELF;
    P_PASS_NAME(LevelName);
    EV_RET_NAME("GetConScriptName");
  }

  void eventCmdWeaponSection (const VStr &Section) {
    P_PASS_SELF;
    P_PASS_STR(Section);
    EV_RET_VOID("CmdWeaponSection");
  }

  void eventCmdSetSlot (TArray<VStr> *Args) {
    P_PASS_SELF;
    P_PASS_PTR(Args);
    EV_RET_VOID("CmdSetSlot");
  }

  void eventCmdAddSlotDefault (TArray<VStr> *Args) {
    P_PASS_SELF;
    P_PASS_PTR(Args);
    EV_RET_VOID("CmdAddSlotDefault");
  }

  DECLARE_FUNCTION(get_isPaused)
};


extern VGameInfo *GGameInfo;
