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
#ifndef SV_LOCAL_HEADER
#define SV_LOCAL_HEADER

#define MAXHEALTH        (100)
#define DEFAULT_GRAVITY  (1225.0f)

//#define REBORN_DESCRIPTION    "TEMP GAME"

struct tmtrace_t;
class VMessageOut;

extern VLevelInfo *GLevelInfo;


// ////////////////////////////////////////////////////////////////////////// //
// flags for SV_MapTeleport
enum {
  CHANGELEVEL_KEEPFACING      = 0x00000001,
  CHANGELEVEL_RESETINVENTORY  = 0x00000002,
  CHANGELEVEL_NOMONSTERS      = 0x00000004,
  CHANGELEVEL_CHANGESKILL     = 0x00000008,
  CHANGELEVEL_NOINTERMISSION  = 0x00000010,
  CHANGELEVEL_RESETHEALTH     = 0x00000020,
  CHANGELEVEL_PRERAISEWEAPON  = 0x00000040,
};


// ////////////////////////////////////////////////////////////////////////// //
// WARNING! keep in sync with script code!
enum {
  CONTENTS_EMPTY,
  CONTENTS_WATER,
  CONTENTS_LAVA,
  CONTENTS_NUKAGE,
  CONTENTS_SLIME,
  CONTENTS_HELLSLIME,
  CONTENTS_BLOOD,
  CONTENTS_SLUDGE,
  CONTENTS_HAZARD,
  CONTENTS_BOOMWATER,

  CONTENTS_SOLID = -1
};


//==========================================================================
//
//  sv_acs
//
//  Action code scripts
//
//==========================================================================

// script types
enum {
  SCRIPT_Closed      = 0,
  SCRIPT_Open        = 1,
  SCRIPT_Respawn     = 2,
  SCRIPT_Death       = 3,
  SCRIPT_Enter       = 4,
  SCRIPT_Pickup      = 5,
  SCRIPT_BlueReturn  = 6,
  SCRIPT_RedReturn   = 7,
  SCRIPT_WhiteReturn = 8,
  SCRIPT_Lightning   = 12,
  SCRIPT_Unloading   = 13,
  SCRIPT_Disconnect  = 14,
  SCRIPT_Return      = 15,
};


class VAcs;
class VAcsObject;
struct VAcsInfo;


// ////////////////////////////////////////////////////////////////////////// //
class VAcsLevel {
private:
  TMap<VStr, int> stringMapByStr;
  TArray<VStr> stringList;

private:
  bool AddToACSStore (int Type, VName Map, int Number, int Arg1, int Arg2, int Arg3, int Arg4, VEntity *Activator);

public:
  VLevel *XLevel;

  TArray<VAcsObject *> LoadedObjects;
  //TArray<int> LoadedStaticObjects; // index in `LoadedObjects`

public:
  VAcsLevel (VLevel *ALevel);
  ~VAcsLevel ();

  VAcsObject *LoadObject (int Lump);
  VAcsInfo *FindScript (int Number, VAcsObject *&Object);
  VAcsInfo *FindScriptByName (int Number, VAcsObject *&Object);
  int FindScriptNumberByName (const VStr &aname, VAcsObject *&Object);
  VAcsInfo *FindScriptByNameStr (const VStr &aname, VAcsObject *&Object);
  VStr GetString (int Index);
  VName GetNameLowerCase (int Index);
  VAcsObject *GetObject (int Index);
  void StartTypedACScripts (int Type, int Arg1, int Arg2, int Arg3,
                            VEntity *Activator, bool Always, bool RunNow);
  void Serialise (VStream &Strm);
  void CheckAcsStore ();
  bool Start (int Number, int MapNum, int Arg1, int Arg2, int Arg3, int Arg4,
              VEntity *Activator, line_t *Line, int Side, bool Always,
              bool WantResult, bool Net = false, int *realres=nullptr);
  bool Terminate (int Number, int MapNum);
  bool Suspend (int Number, int MapNum);
  VAcs *SpawnScript (VAcsInfo *Info, VAcsObject *Object, VEntity *Activator,
                     line_t *Line, int Side, int Arg1, int Arg2, int Arg3, int Arg4,
                     bool Always, bool Delayed, bool ImmediateRun);

  VStr GetNewString (int idx);
  VName GetNewLowerName (int idx);
  int PutNewString (VStr str);
};


// ////////////////////////////////////////////////////////////////////////// //
class VAcsGrowingArray {
private:
  TMapNC<int, int> values; // index, value

public:
  VAcsGrowingArray () : values() {}

  inline void clear () { values.clear(); }

  inline void SetElemVal (int index, int value) { values.put(index, value); }
  inline int GetElemVal (int index) const { auto vp = values.find(index); return (vp ? *vp : 0); }

  void Serialise (VStream &Strm);
};

static inline __attribute((unused)) VStream &operator << (VStream &strm, VAcsGrowingArray &arr) { arr.Serialise(strm); return strm; }


// ////////////////////////////////////////////////////////////////////////// //
struct VAcsStore {
  enum {
    Start,
    StartAlways,
    Terminate,
    Suspend
  };

  VName Map; // target map
  vuint8 Type; // type of action
  vint8 PlayerNum; // player who executes this script
  vint32 Script; // script number on target map
  vint32 Args[4]; // arguments

  void Serialise (VStream &Strm);
};

static inline __attribute((unused)) VStream &operator << (VStream &Strm, VAcsStore &Store) { Store.Serialise(Strm); return Strm; }


class VAcsGlobal {
private:
  enum {
    MAX_ACS_WORLD_VARS  = 256,
    MAX_ACS_GLOBAL_VARS = 64,
  };

  VAcsGrowingArray WorldVars;
  VAcsGrowingArray GlobalVars;
  VAcsGrowingArray WorldArrays[MAX_ACS_WORLD_VARS];
  VAcsGrowingArray GlobalArrays[MAX_ACS_GLOBAL_VARS];

public:
  TArray<VAcsStore> Store;

public:
  VAcsGlobal ();

  void Serialise (VStream &Strm);

  VStr GetGlobalVarStr (VAcsLevel *level, int index) const;

  int GetGlobalVarInt (int index) const;
  float GetGlobalVarFloat (int index) const;
  void SetGlobalVarInt (int index, int value);
  void SetGlobalVarFloat (int index, float value);

  int GetWorldVarInt (int index) const;
  float GetWorldVarFloat (int index) const;
  void SetWorldVarInt (int index, int value);
  void SetWorldVarFloat (int index, float value);

  int GetGlobalArrayInt (int aidx, int index) const;
  float GetGlobalArrayFloat (int aidx, int index) const;
  void SetGlobalArrayInt (int aidx, int index, int value);
  void SetGlobalArrayFloat (int aidx, int index, float value);

  int GetWorldArrayInt (int aidx, int index) const;
  float GetWorldArrayFloat (int aidx, int index) const;
  void SetWorldArrayInt (int aidx, int index, int value);
  void SetWorldArrayFloat (int aidx, int index, float value);
};


//==========================================================================
//
//  sv_world
//
//  Map utilites
//
//==========================================================================
struct opening_t {
  float top;
  float bottom;
  float range; // top-bottom, to avoid calculations
  float lowfloor; // this is used for dropoffs: floor height on the other side (always lower then, or equal to bottom)
  float highceiling; // ceiling height on the other side (always higher than, or equal to top)
  TSecPlaneRef efloor;
  TSecPlaneRef eceiling;
  TSecPlaneRef elowfloor;
  TSecPlaneRef ehighceiling;
  // for this list
  opening_t *next;
  //opening_t *prev;
  // allocated list is double-linked, free list only using `listnext`
  opening_t *listprev;
  opening_t *listnext;

  inline void copyFrom (const opening_t *op) {
    if (op == this) return;
    if (op) {
      top = op->top;
      bottom = op->bottom;
      range = op->range;
      lowfloor = op->lowfloor;
      highceiling = op->highceiling;
      efloor = op->efloor;
      eceiling = op->eceiling;
      elowfloor = op->elowfloor;
      ehighceiling = op->ehighceiling;
    } else {
      top = bottom = range = lowfloor = 0.0f;
      efloor.splane = eceiling.splane = elowfloor.splane = ehighceiling.splane = nullptr;
    }
  }
};


// ////////////////////////////////////////////////////////////////////////// //
TVec P_SectorClosestPoint (sector_t *sec, TVec in);
int P_BoxOnLineSide (float *tmbox, line_t *ld);

// used only in debug code
bool P_GetMidTexturePosition (const line_t *line, int sideno, float *ptextop, float *ptexbot);


// ////////////////////////////////////////////////////////////////////////// //
int SV_PointContents (sector_t *sector, const TVec &p);

// this is used to get region lighting
sec_region_t *SV_PointRegionLight (sector_t *sector, const TVec &p, bool dbgDump=false);

// find region for thing, and return best floor/ceiling
// `p.z` is bottom
void SV_FindGapFloorCeiling (sector_t *sector, const TVec point, float height, TSecPlaneRef &floor, TSecPlaneRef &ceiling, bool debugDump=false);

// find sector gap that contains the given point, and return its floor and ceiling
void SV_GetSectorGapCoords (sector_t *sector, const TVec point, float &floorz, float &ceilz);

// build list of openings for the given line and point
// note that returned list can be reused on next call to `SV_LineOpenings()`
opening_t *SV_LineOpenings (const line_t *linedef, const TVec point, unsigned NoBlockFlags, bool do3dmidtex=false, bool usePoint=true);

// used in surface creator
opening_t *SV_SectorOpenings (sector_t *sector, bool skipNonSolid=false);
opening_t *SV_SectorOpenings2 (sector_t *sector, bool skipNonSolid=false);

// it is used to find lowest sector point for silent teleporters
float SV_GetLowestSolidPointZ (sector_t *sector, const TVec &point);

// find "best fit" opening for the given coordz
// `z1` is feet, `z2` is head
opening_t *SV_FindOpening (opening_t *gaps, float z1, float z2);

// find "rel best fit" opening for the given coordz
// `z1` is feet, `z2` is head
// used in sector movement, so it tries hard to not leave current opening
opening_t *SV_FindRelOpening (opening_t *gaps, float z1, float z2);


//==========================================================================
//
//  sv_switch
//
//  Switches
//
//==========================================================================
void P_InitSwitchList ();

void P_InitTerrainTypes ();
struct VTerrainInfo *SV_TerrainType (int pic);
void P_FreeTerrainTypes ();


//==========================================================================
//
//  sv_tick
//
//  Handling thinkers, running tics
//
//==========================================================================
extern int TimerGame; // tic countdown for deathmatch


//==========================================================================
//
//  sv_main
//
//==========================================================================
void SV_ReadMove ();

void Draw_TeleportIcon ();

void SV_DropClient (VBasePlayer *Player, bool crash);
void SV_SpawnServer (const char *mapname, bool spawn_thinkers, bool titlemap=false);
void SV_SendServerInfoToClients ();


extern int LeavePosition;
extern bool completed;


//==========================================================================
//
//  sv_user
//
//==========================================================================

//==========================================================================
//
//  ????
//
//==========================================================================

void G_TeleportNewMap (int map, int position);
void G_WorldDone ();
//void G_PlayerReborn (int player);
void G_StartNewInit ();


extern VBasePlayer *GPlayersBase[MAXPLAYERS]; // Bookkeeping on players state

extern vuint8 deathmatch; // only if started as net death

extern bool sv_loading;
extern bool sv_map_travel;
extern int sv_load_num_players;
extern bool run_open_scripts;


//==========================================================================
//
//  inlines
//
//==========================================================================
static inline __attribute__((unused)) int SV_GetPlayerNum (VBasePlayer *player) {
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (player == GPlayersBase[i]) return i;
  }
  return 0;
}

static inline __attribute__((unused)) VBasePlayer *SV_GetPlayerByNum (int pidx) {
  if (pidx < 0 || pidx >= MAXPLAYERS) return nullptr;
  return GPlayersBase[pidx];
}

#endif
