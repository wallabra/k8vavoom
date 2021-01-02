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

  // client flags
  enum {
    CLF_RUN_DISABLED    = 1u<<0,
    CLF_MLOOK_DISABLED  = 1u<<1,
    CLF_CROUCH_DISABLED = 1u<<2,
    CLF_JUMP_DISABLED   = 1u<<3,
  };

  VName AcsHelper;
  VName GenericConScript;

  vuint8 NetMode;
  vuint8 deathmatch;
  vuint8 respawn;
  vuint8 nomonsters;
  vuint8 fastparm; // 0:normal; 1:fast; 2:slow
  // the following flags are valid only for `NM_Client`
  vuint32 clientFlags; // see `CLF_XXX`

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
    GIF_DefaultLaxMonsterActivation = 1u<<0,
    GIF_DefaultBloodSplatter        = 1u<<1,
    GIF_Paused                      = 1u<<2,
    GIF_ForceKillScripts            = 1u<<3,
  };
  vuint32 Flags;

  TArray<VDamageFactor> CustomDamageFactors;

public:
  enum { PlrAll = 0, PlrOnlySpawned = 1 };

  struct PlayersIter {
  private:
    VGameInfo *gi;
    int plridx;
    int type;

    inline void nextByType () {
      for (++plridx; plridx < MAXPLAYERS; ++plridx) {
        if (!gi->Players[plridx]) continue;
        switch (type) {
          case PlrAll: return;
          default: // just in case
          case PlrOnlySpawned: if (gi->Players[plridx]->PlayerFlags&VBasePlayer::PF_Spawned) return; break;
        }
      }
      plridx = MAXPLAYERS;
    }

  public:
    PlayersIter (int atype, VGameInfo *agi) : gi(agi), plridx(-1), type(atype) { nextByType(); }
    PlayersIter (const PlayersIter &src) : gi(src.gi), plridx(src.plridx), type(src.type) {}
    PlayersIter (const PlayersIter &src, bool atEnd) : gi(src.gi), plridx(MAXPLAYERS), type(src.type) {}

    inline PlayersIter begin () { return PlayersIter(*this); }
    inline PlayersIter end () { return PlayersIter(*this, true); }
    inline bool operator == (const PlayersIter &b) const { return (gi == b.gi && plridx == b.plridx && type == b.type); }
    inline bool operator != (const PlayersIter &b) const { return (gi != b.gi || plridx != b.plridx || type != b.type); }
    inline PlayersIter operator * () const { return PlayersIter(*this); } /* required for iterator */
    inline void operator ++ () { if (plridx < MAXPLAYERS) nextByType(); } /* this is enough for iterator */
    // accessors
    inline int index () const { return plridx; }
    inline VBasePlayer *value () const { return gi->Players[plridx]; }
    inline VBasePlayer *player () const { return gi->Players[plridx]; }
  };

  PlayersIter playersAll () { return PlayersIter(PlrAll, this); }
  PlayersIter playersSpawned () { return PlayersIter(PlrOnlySpawned, this); }

public:
  //VGameInfo ();

  bool IsPaused (bool ignoreOpenConsole=false);
  bool IsInWipe ();
  bool IsWipeAllowed ();

  void eventInit () { static VMethodProxy method("Init"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventPostDecorateInit () { static VMethodProxy method("PostDecorateInit"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventInitNewGame (int skill) { static VMethodProxy method("InitNewGame"); vobjPutParamSelf(skill); VMT_RET_VOID(method); }
  VWorldInfo *eventCreateWorldInfo () { static VMethodProxy method("CreateWorldInfo"); vobjPutParamSelf(); VMT_RET_REF(VWorldInfo, method); }
  void eventTranslateLevel (VLevel *InLevel) { static VMethodProxy method("TranslateLevel"); vobjPutParamSelf(InLevel); VMT_RET_VOID(method); }
  void eventSpawnWorld (VLevel *InLevel) { static VMethodProxy method("SpawnWorld"); vobjPutParamSelf(InLevel); VMT_RET_VOID(method); }
  VName eventGetConScriptName (VName LevelName) { static VMethodProxy method("GetConScriptName"); vobjPutParamSelf(LevelName); VMT_RET_NAME(method); }
  void eventCmdWeaponSection (VStr Section) { static VMethodProxy method("CmdWeaponSection"); vobjPutParamSelf(Section); VMT_RET_VOID(method); }
  void eventCmdSetSlot (TArray<VStr> *Args, bool asKeyconf) { static VMethodProxy method("CmdSetSlot"); vobjPutParamSelf((void *)Args, asKeyconf); VMT_RET_VOID(method); }
  void eventCmdAddSlotDefault (TArray<VStr> *Args, bool asKeyconf) { static VMethodProxy method("CmdAddSlotDefault"); vobjPutParamSelf((void *)Args, asKeyconf); VMT_RET_VOID(method); }

  DECLARE_FUNCTION(get_isPaused)
  DECLARE_FUNCTION(get_isInWipe)
};


extern VGameInfo *GGameInfo;
