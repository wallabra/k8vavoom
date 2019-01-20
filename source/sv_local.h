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

inline VStream &operator << (VStream &strm, VAcsGrowingArray &arr) { arr.Serialise(strm); return strm; }


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

  friend VStream &operator << (VStream &Strm, VAcsStore &Store);
};


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
  float range; // negatve: this is 3dmidtex
  float lowfloor;
  float highceiling;
  sec_plane_t *floor;
  sec_plane_t *ceiling;
  sec_plane_t *lowfloorplane;
  sec_plane_t *highceilingplane;
  opening_t *next;
};


TVec P_SectorClosestPoint (sector_t *sec, TVec in);
int P_BoxOnLineSide (float *tmbox, line_t *ld);

bool P_GetMidTexturePosition (const line_t *line, int sideno, float *ptextop, float *ptexbot);

int SV_PointContents (const sector_t *sector, const TVec &p);
sec_region_t *SV_PointInRegion (sector_t *sector, const TVec &p);

opening_t *SV_LineOpenings (const line_t *linedef, const TVec &point, int NoBlockFlags, bool do3dmidtex=false);

sec_region_t *SV_FindThingGap (sec_region_t *gaps, const TVec &point, float z1, float z2);
opening_t *SV_FindOpening (opening_t *gaps, float z1, float z2);


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
void SV_SpawnServer (const char *, bool, bool);
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
