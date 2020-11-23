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
  // k8
  CHANGELEVEL_REMOVEKEYS      = 0x00100000,
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
  SCRIPT_Event       = 16, // not implemented
  SCRIPT_Kill        = 17,
  SCRIPT_Reopen      = 18, // not implemented
};


class VAcs;
class VAcsObject;
struct VAcsInfo;


// ////////////////////////////////////////////////////////////////////////// //
class VAcsLevel {
private:
  TMap<VStr, int> stringMapByStr;
  TArray<VStr> stringList;
  TMapNC<int, bool> unknownScripts;

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
  int FindScriptNumberByName (VStr aname, VAcsObject *&Object);
  VAcsInfo *FindScriptByNameStr (VStr aname, VAcsObject *&Object);
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

public: // debug
  static VStr GenScriptName (int Number);
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


/*
struct cptrace_t {
  TVec Pos;
  float BBox[4];
  float FloorZ;
  float CeilingZ;
  float DropOffZ;
  sec_plane_t *EFloor;
  sec_plane_t *ECeiling;
};
*/

struct tmtrace_t {
  VEntity *StepThing; // not for cptrace_t
  TVec End; // not for cptrace_t
  float BBox[4]; // valid for cptrace_t
  float FloorZ; // valid for cptrace_t
  float CeilingZ; // valid for cptrace_t
  float DropOffZ; // valid for cptrace_t

  // WARNING! keep in sync with VEntity fcflags
  /*
  enum {
    FC_FlipFloor = 1u<<0,
    FC_FlipCeiling = 1u<<1,
  };
  vuint32 fcflags; // valid for cptrace_t
  */
  TSecPlaneRef EFloor; // valid for cptrace_t
  TSecPlaneRef ECeiling; // valid for cptrace_t

  enum {
    TF_FloatOk = 0x01u, // if true, move would be ok if within tmtrace.FloorZ - tmtrace.CeilingZ
  };
  vuint32 TraceFlags;

  // keep track of the line that lowers the ceiling,
  // so missiles don't explode against sky hack walls
  line_t *CeilingLine;
  line_t *FloorLine;
  // also keep track of the blocking line, for checking
  // against doortracks
  line_t *BlockingLine; // only lines without backsector

  // keep track of special lines as they are hit,
  // but don't process them until the move is proven valid
  TArray<line_t *> SpecHit;

  VEntity *BlockingMobj;
  // any blocking line (including passable two-sided!); only has any sense if trace returned `false`
  // note that this is really *any* line, not necessarily first or last crossed!
  line_t *AnyBlockingLine;

  // from cptrace_t
  TVec Pos; // valid for cptrace_t

  /*
  inline void CopyRegFloor (sec_region_t *r, const TVec *Origin) {
    EFloor = r->efloor;
    if (Origin) FloorZ = EFloor.GetPointZClamped(*Origin);
  }

  inline void CopyRegCeiling (sec_region_t *r, const TVec *Origin) {
    ECeiling = r->eceiling;
    if (Origin) CeilingZ = ECeiling.GetPointZClamped(*Origin);
  }
  */

  inline void CopyOpenFloor (opening_t *o, bool setz=true) {
    EFloor = o->efloor;
    if (setz) FloorZ = o->bottom;
  }

  inline void CopyOpenCeiling (opening_t *o, bool setz=true) {
    ECeiling = o->eceiling;
    if (setz) CeilingZ = o->top;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
int SV_PointContents (sector_t *sector, const TVec &p, bool dbgDump=false);

// returns region to use as light param source, and additionally glow flags (`glowFlags` can be nullptr)
// glowFlags: bit 0: floor glow allowed; bit 1: ceiling glow allowed
sec_region_t *SV_PointRegionLight (sector_t *sector, const TVec &p, unsigned *glowFlags=nullptr);

// the one that is lower
//sec_region_t *SV_GetPrevRegion (sector_t *sector, sec_region_t *srcreg);
// the one that is higher
sec_region_t *SV_GetNextRegion (sector_t *sector, sec_region_t *srcreg);

sec_surface_t *SV_DebugFindNearestFloor (subsector_t *sub, const TVec &p);

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
float SV_GetLowestSolidPointZ (sector_t *sector, const TVec &point, bool ignore3dFloors=true);
float SV_GetHighestSolidPointZ (sector_t *sector, const TVec &point, bool ignore3dFloors=true);

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

struct VTerrainInfo;
void P_InitTerrainTypes ();
VTerrainInfo *SV_TerrainType (int pic);
VTerrainInfo *SV_GetDefaultTerrain ();
void P_FreeTerrainTypes ();


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

// call after texture manager updated a flat
void SV_UpdateSkyFlat ();

extern int LeavePosition;
extern bool completed;


//==========================================================================
//
//  ????
//
//==========================================================================

void G_TeleportNewMap (int map, int position);
void G_WorldDone ();
//void G_PlayerReborn (int player);
void G_StartNewInit ();

bool G_CheckWantExitText ();
bool G_CheckFinale ();
bool G_StartClientFinale ();


extern VBasePlayer *GPlayersBase[MAXPLAYERS]; // Bookkeeping on players state

extern bool sv_loading;
extern bool sv_map_travel;
extern int sv_load_num_players;
extern bool run_open_scripts;


//==========================================================================
//
//  inlines
//
//==========================================================================
static inline VVA_OKUNUSED int SV_GetPlayerNum (VBasePlayer *player) {
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (player == GPlayersBase[i]) return i;
  }
  return 0;
}

static inline VVA_OKUNUSED VBasePlayer *SV_GetPlayerByNum (int pidx) {
  if (pidx < 0 || pidx >= MAXPLAYERS) return nullptr;
  return GPlayersBase[pidx];
}


#endif
