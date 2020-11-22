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
//**  This file includes code from ZDoom, copyright 1998-2004 Randy Heit,
//**  all rights reserved, with the following licence:
//**
//** Redistribution and use in source and binary forms, with or without
//** modification, are permitted provided that the following conditions
//** are met:
//**
//** 1. Redistributions of source code must retain the above copyright
//**    notice, this list of conditions and the following disclaimer.
//** 2. Redistributions in binary form must reproduce the above copyright
//**    notice, this list of conditions and the following disclaimer in the
//**    documentation and/or other materials provided with the distribution.
//** 3. The name of the author may not be used to endorse or promote products
//**    derived from this software without specific prior written permission.
//**
//** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
//** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
//** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
//** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
//** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
//** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
//** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//**
//**************************************************************************
#include "../gamedefs.h"
#include "../server/sv_local.h"
#ifdef CLIENT
# include "../client/cl_local.h"
#endif
#include "p_acs.h"

//#define ACS_DUMP_EXECUTION

// for PrintName
#define PRINTNAME_LEVELNAME  (-1)
#define PRINTNAME_LEVEL      (-2)
#define PRINTNAME_SKILL      (-3)


static VCvarI acs_screenblocks_override("acs_screenblocks_override", "-1", "Overrides 'screenblocks' variable for acs scripts (-1: don't).", CVAR_Archive);
static VCvarB acs_halt_on_unimplemented_opcode("acs_halt_on_unimplemented_opcode", false, "Halt ACS VM on unimplemented opdode?", CVAR_Archive);
static VCvarB acs_halt_on_unknown_opcode("acs_halt_on_unknown_opcode", true, "Halt ACS VM on unknown opdode?", CVAR_Archive);
static VCvarB acs_warning_console_commands("acs_warning_console_commands", true, "Show warning when ACS script tries to execute console command?", CVAR_Archive);
static VCvarB acs_dump_uservar_access("acs_dump_uservar_access", false, "Dump ACS uservar access?", CVAR_Archive);
static VCvarB acs_use_doomtic_granularity("acs_use_doomtic_granularity", false, "Should ACS use DooM tic granularity for delays?", CVAR_Archive);
static VCvarB acs_enabled("acs_enabled", true, "DEBUG: are ACS scripts enabled?", CVAR_PreInit);
static VCvarB acs_show_started_scripts("acs_show_started_scripts", false, "DEBUG: are ACS scripts enabled?", CVAR_PreInit);
static VCvarB acs_show_stopped_scripts("acs_show_stopped_scripts", false, "DEBUG: are ACS scripts enabled?", CVAR_PreInit);
static VCvarB acs_abort_on_unknown_acsf("acs_abort_on_unknown_acsf", true, "Abort on unknown ACSF function? (WARNING: setting this 'off' may break some maps)", CVAR_Archive);
static VCvarB acs_emulate_zandronum_acsf("acs_emulate_zandronum_acsf", false, "Emulate some Zandronum ACSF functions? (WARNING: setting this 'off' may break some maps)", CVAR_Archive);

static VCvarB dbg_acs_allow_unimplemented_opcodes("dbg_acs_allow_unimplemented_opcodes", false, "Override 'acs_halt_on_unimplemented_opcode', non-persistent", CVAR_PreInit);

extern VCvarF mouse_x_sensitivity;
extern VCvarF mouse_y_sensitivity;
extern VCvarF m_yaw;
extern VCvarF m_pitch;


static inline bool isZandroACS () noexcept {
       if (flACSType == FL_ACS_Zandronum) return true;
  else if (flACSType == FL_ACS_ZDoom) return false;
  else return acs_emulate_zandronum_acsf.asBool();
}


static bool acsReportedBadOpcodesInited = false;
static bool acsReportedBadOpcodes[65536];

#define SPECIAL_LOW_SCRIPT_NUMBER  (100000000)

#define ACS_GUARD_INSTRUCTION_COUNT  (1000000)


#define AAPTR_DEFAULT                0x00000000
#define AAPTR_NULL                   0x00000001
#define AAPTR_TARGET                 0x00000002
#define AAPTR_MASTER                 0x00000004
#define AAPTR_TRACER                 0x00000008
#define AAPTR_PLAYER_GETTARGET       0x00000010
#define AAPTR_PLAYER_GETCONVERSATION 0x00000020
#define AAPTR_PLAYER1                0x00000040
#define AAPTR_PLAYER2                0x00000080
#define AAPTR_PLAYER3                0x00000100
#define AAPTR_PLAYER4                0x00000200
#define AAPTR_PLAYER5                0x00000400
#define AAPTR_PLAYER6                0x00000800
#define AAPTR_PLAYER7                0x00001000
#define AAPTR_PLAYER8                0x00002000
#define AAPTR_FRIENDPLAYER           0x00004000
#define AAPTR_GET_LINETARGET         0x00008000

#define AAPTR_ANY_PLAYER  (AAPTR_PLAYER1|AAPTR_PLAYER2|AAPTR_PLAYER3|AAPTR_PLAYER4|AAPTR_PLAYER5|AAPTR_PLAYER6|AAPTR_PLAYER7|AAPTR_PLAYER8)


// GetArmorInfo
enum {
  ARMORINFO_CLASSNAME,
  ARMORINFO_SAVEAMOUNT,
  ARMORINFO_SAVEPERCENT,
  ARMORINFO_MAXABSORB,
  ARMORINFO_MAXFULLABSORB,
  ARMORINFO_ACTUALSAVEAMOUNT,
  ARMORINFO_MAX,
};

enum {
  // PAF_FORCETID,
  // PAF_RETURNTID
  PICKAF_FORCETID = 1,
  PICKAF_RETURNTID = 2,
};

enum { ACSLEVEL_INTERNAL_STRING_STORAGE_INDEX = 0xfffeu };

// internal engine limits
enum {
  MAX_ACS_SCRIPT_VARS = 32, // was 20
  MAX_ACS_MAP_VARS    = 128,
};

enum EAcsFormat {
  ACS_Old,
  ACS_Enhanced,
  ACS_LittleEnhanced,
  ACS_Unknown
};

// script flags
enum {
  SCRIPTF_Net = 0x0001, // safe to "puke" in multiplayer
};

struct VACSLocalArrayInfo {
  int Size;
  int Offset;
};

struct VACSLocalArrays {
  int Count;
  VACSLocalArrayInfo *Info;

  inline VACSLocalArrays () noexcept : Count(0), Info(nullptr) {}
  inline ~VACSLocalArrays () { Clear(); }

  inline void Clear () noexcept { if (Info) { delete[] Info; Info = nullptr; } }

  inline void SetCount (int acount) {
    if (acount < 0) acount = 0;
    Clear();
    Count = acount;
    if (acount > 0) Info = new VACSLocalArrayInfo[acount];
  }

  // bounds-checking Set and Get for local arrays
  inline void Set (vint32 *locals, int arraynum, int arrayentry, int value) {
    if ((unsigned)arraynum < (unsigned)Count &&
        (unsigned)arrayentry < (unsigned)Info[arraynum].Size)
    {
      locals[Info[arraynum].Offset+arrayentry] = value;
    }
  }

  inline int Get (const vint32 *locals, int arraynum, int arrayentry) const {
    if ((unsigned)arraynum < (unsigned)Count &&
        (unsigned)arrayentry < (unsigned)Info[arraynum].Size)
    {
      return locals[Info[arraynum].Offset+arrayentry];
    }
    return 0;
  }
};

struct __attribute__((packed)) VAcsHeader {
  char Marker[4];
  vint32 InfoOffset;
  vint32 Code;
};

struct VAcsInfo {
  vuint16 Number;
  vuint8 Type;
  vuint8 ArgCount;
  vuint8 *Address;
  vuint16 Flags;
  vuint16 VarCount;
  VACSLocalArrays LocalArrays;
  VName Name; // NAME_None for unnamed scripts; lowercased
  VAcs *RunningScript;

  VStr toString () const {
    return VStr(va("name:<%s>; number:%u; type:%u; argc:%u; flags:%u; varcount:%u; locarrays:%d",
      *Name, Number, Type, ArgCount, Flags, VarCount, LocalArrays.Count));
  }
};

//WARNING! this is what is stored in object file chunk. DO NOT MODIFY!
struct __attribute__((packed)) VAcsFunctionChunkData {
  vuint8 ArgCount;
  vuint8 LocalCount;
  vuint8 HasReturnValue;
  vuint8 ImportNum;
  vuint32 Address;
};


struct VAcsFunction {
  vuint8 ArgCount;
  vuint8 LocalCount;
  vuint8 HasReturnValue;
  vuint8 ImportNum;
  vuint32 Address;
  VACSLocalArrays LocalArrays;

  void SetupFrom (const VAcsFunctionChunkData &cd) noexcept {
    ArgCount = cd.ArgCount;
    LocalCount = cd.LocalCount;
    HasReturnValue = cd.HasReturnValue;
    ImportNum = cd.ImportNum;
    Address = cd.Address;
    LocalArrays.Clear();
  }
};


// ////////////////////////////////////////////////////////////////////////// //
// simple stack pool
#define ACS_MAX_STACKS  (32)
static vint32 *acsStackPool[ACS_MAX_STACKS] = {0};
static int acsStacksUsed = 0;
static int acsStackNextFree = -1; // acsStackPool[acsStackNextFree][0] is the index of the next free stack

#define  ACS_STACK_DEPTH  (32768)


struct ACSStack {
public:
  vint32 *stk;
  int stkPoolIdx; // -1: not in a pool

public:
  VV_DISABLE_COPY(ACSStack)
  ACSStack () : stk(nullptr), stkPoolIdx(acsStackNextFree) {
    if (stkPoolIdx >= 0) {
      // reuse stack
      stk = acsStackPool[stkPoolIdx];
      acsStackNextFree = stk[0];
      //GCon->Logf(NAME_Debug, "reused acs stack %d (next free is %d)", stkPoolIdx, acsStackNextFree);
    } else if (acsStacksUsed < ACS_MAX_STACKS) {
      // allocate a new free slot
      stkPoolIdx = acsStacksUsed++;
      stk = (vint32 *)Z_Calloc((ACS_STACK_DEPTH+512)*sizeof(vint32)); // why not?
      acsStackPool[stkPoolIdx] = stk;
      //GCon->Logf(NAME_Debug, "allocated new acs stack %d (next free is %d)", stkPoolIdx, acsStackNextFree);
    } else {
      // just allocate it here
      stkPoolIdx = -1; // just in case
      stk = (vint32 *)Z_Calloc((ACS_STACK_DEPTH+512)*sizeof(vint32)); // why not?
      //GCon->Logf(NAME_Debug, "allocated new TEMP acs stack %d (next free is %d)", stkPoolIdx, acsStackNextFree);
    }
  }
  ~ACSStack () {
    if (stkPoolIdx < 0) {
      Z_Free(stk);
      //GCon->Logf(NAME_Debug, "freed TEMP acs stack %d (prev free is %d)", stkPoolIdx, acsStackNextFree);
    } else {
      vassert(stkPoolIdx < ACS_MAX_STACKS);
      vassert(stk == acsStackPool[stkPoolIdx]);
      //GCon->Logf(NAME_Debug, "freed acs stack %d (prev free is %d)", stkPoolIdx, acsStackNextFree);
      stk[0] = acsStackNextFree;
      acsStackNextFree = stkPoolIdx;
    }
    // just in case
    stk = nullptr;
    stkPoolIdx = -1;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
// an action code scripts object module -- level's BEHAVIOR lump or library
class VAcsObject {
private:
  friend class VAcsLevel;

  struct VArrayInfo {
    vint32 Size;
    vint32 *Data;
  };

  EAcsFormat Format;

  vint32 LumpNum;
  vint32 LibraryID;

  vint32 DataSize;
  vuint8 *Data;

  vuint8 *Chunks;

  vint32 NumScripts;
  VAcsInfo *Scripts;

  TArray<VAcsFunction> Functions;
  //vint32 NumFunctions;

  vint32 NumStrings;
  char **Strings;
  VName *LowerCaseNames;

  vint32 MapVarStore[MAX_ACS_MAP_VARS];

  vint32 NumArrays;
  VArrayInfo *ArrayStore;
  vint32 NumTotalArrays;
  VArrayInfo **Arrays;

  TArray<VAcsObject *> Imports;

  void LoadOldObject ();
  void LoadEnhancedObject ();
  void UnencryptStrings ();
  int FindFunctionName (const char *Name) const;
  int FindMapVarName (const char *Name) const;
  int FindMapArray (const char *Name) const;
  int FindStringInChunk (vuint8 *Chunk, const char *Name) const;
  vuint8 *FindChunk (const char *id) const;
  vuint8 *NextChunk (vuint8 *prev) const;
  void Serialise (VStream &Strm);
  void StartTypedACScripts (int Type, int Arg1, int Arg2, int Arg3, VEntity *Activator, bool Always, bool RunNow);

public:
  VAcsLevel *Level;
  vint32 *MapVars[MAX_ACS_MAP_VARS];

public:
  VAcsObject (VAcsLevel *ALevel, int Lump);
  ~VAcsObject ();

  vuint8 *OffsetToPtr (int);
  int PtrToOffset (vuint8 *);
  inline EAcsFormat GetFormat () const { return Format; }
  inline int GetNumScripts () const { return NumScripts; }
  inline VAcsInfo &GetScriptInfo (int i) { return Scripts[i]; }

  inline VStr GetString (int i) const {
    if (i < 0 || i >= NumStrings) return "";
    VStr Ret = Strings[i];
    //if (!Ret.IsValidUtf8()) Ret = Ret.Latin1ToUtf8();//k8:???
    return Ret;
  }

  inline VName GetNameLowerCase (int i) {
    if (i < 0 || i >= NumStrings) return NAME_None;
    if (LowerCaseNames[i] == NAME_None) LowerCaseNames[i] = *GetString(i).ToLower();
    return LowerCaseNames[i];
  }

  inline int GetLibraryID () const { return LibraryID; }

  VAcsInfo *FindScript (int Number) const;
  VAcsInfo *FindScriptByName (int nameidx) const;
  int FindScriptNumberByName (VStr aname) const;
  VAcsInfo *FindScriptByNameStr (VStr aname) const;
  VAcsFunction *GetFunction (int funcnum, VAcsObject *&Object);
  int GetArrayVal (int ArrayIdx, int Index);
  void SetArrayVal (int ArrayIdx, int Index, int Value);

  inline int GetStringCount () const noexcept { return NumStrings; }
  inline const char *GetCString (int idx) const noexcept { return (idx >= 0 && idx < NumStrings ? Strings[idx] : "<invalid!>"); }
};


// ////////////////////////////////////////////////////////////////////////// //
struct __attribute__((packed)) VAcsCallReturn {
  int ReturnAddress;
  VAcsFunction *ReturnFunction;
  VAcsObject *ReturnObject;
  vint32 *ReturnLocals;
  //VACSLocalArrays *ReturnArrays; // no need to store this, it is always deterministic
  VAcsCallReturn *PrevFrame;
  vuint8 bDiscardResult;
};

static constexpr int GetRetStructStackSize () noexcept {
  return (int)((sizeof(VAcsCallReturn)+3)/4);
}


// ////////////////////////////////////////////////////////////////////////// //
class VAcs : public VLevelScriptThinker /*: public VThinker*/ {
  //DECLARE_CLASS(VAcs, VThinker, 0)
  //NO_DEFAULT_CONSTRUCTOR(VAcs)

public:
  enum {
    ASTE_Running,
    ASTE_Suspended,
    ASTE_WaitingForTag,
    ASTE_WaitingForPoly,
    ASTE_WaitingForScriptStart,
    ASTE_WaitingForScript,
    ASTE_Terminating
  };

  VEntity *Activator;
  line_t *line;
  vint32 side;
  vint32 number;
  VAcsInfo *info;
  vuint8 State;
  float DelayTime;
  vint32 DelayActivationTick; // used only when `acs_use_doomtic_granularity` is set
  vint32 WaitValue;
  vint32 InlineLocalVars[MAX_ACS_SCRIPT_VARS]; // why not?
  vint32 *LocalVars;
  int LocalVarsCount;
  vuint8 *InstructionPointer;
  VAcsObject *ActiveObject;
  int HudWidth;
  int HudHeight;
  VName Font;
  // we cannot delay with something on a stack (this is forbidden by the original ACS VM)
  // so there is no reason to have a separate stack here, and to save a stack pointer
  // `RunScript()` will take care of stack allocations
  // stored here, so we can check for stack consistency (and print stack traces)
  VAcsCallReturn *currRetFrame;

public:
  VAcs ()
    : VLevelScriptThinker()
    , Activator(nullptr)
    , line(nullptr)
    , side(0)
    , number(0)
    , info(nullptr)
    , State(0)
    , DelayTime(0.0f)
    , DelayActivationTick(0)
    , WaitValue(0)
    , LocalVars(nullptr)
    , LocalVarsCount(0)
    , InstructionPointer(nullptr)
    , ActiveObject(nullptr)
    , HudWidth(0)
    , HudHeight(0)
    , Font(NAME_None)
    , currRetFrame(nullptr)
  {
  }

  inline void ResetSaveds () noexcept {
    currRetFrame = nullptr;
  }

  inline void AllocateLocals (int count) {
    if (count < 8) count = 8; // why not?
    vassert(!LocalVars);
    vassert(LocalVarsCount == 0);
    LocalVarsCount = count;
    if (count <= MAX_ACS_SCRIPT_VARS) {
      LocalVars = InlineLocalVars;
      memset(InlineLocalVars, 0, sizeof(InlineLocalVars));
    } else {
      LocalVars = (vint32 *)Z_Calloc(count*sizeof(LocalVars[0]));
      //GCon->Logf(NAME_Debug, "%p: SCRIPT #%d allocated %d locals", (void *)this, info->Number, count);
    }
  }

  inline void FreeLocals () {
    if (LocalVars && LocalVars != InlineLocalVars) {
      //GCon->Logf(NAME_Debug, "%p: SCRIPT #%d freed %d locals", (void *)this, info->Number, LocalVarsCount);
      Z_Free(LocalVars);
    }
    LocalVars = nullptr;
    LocalVarsCount = 0;
  }

  //virtual ~VAcs () override { Destroy(); } // just in case

  virtual VName GetClassName () override { return VName("VAcs"); }

  virtual void Destroy () override;
  virtual void Serialise (VStream &) override;
  virtual void ClearReferences () override;

  virtual VName GetName () override;
  virtual int GetNumber () override;

  void TranslateSpecial (int &spec, int &arg1);
  int RunScript (float DeltaTime, bool immediate);
  virtual void Tick (float) override;
  int CallFunction (int argCount, int funcIndex, vint32 *args);

  virtual VStr DebugDumpToString () override {
    return VStr(va("number=%d; name=<%s>", number, *info->Name));
  }

  // it doesn't matter if there will be duplicates
  /*
  virtual void RegisterObjects (VStream &strm) override {
    strm.RegisterObject(Level); // just in case, ..
    strm.RegisterObject(XLevel); // ..cause why not?
    strm.RegisterObject(Activator);
  }
  */

private:
  enum EScriptAction {
    SCRIPT_Continue,
    SCRIPT_Stop,
    SCRIPT_Terminate,
  };

  // constants used by scripts
  enum EGameMode {
    GAME_SINGLE_PLAYER,
    GAME_NET_COOPERATIVE,
    GAME_NET_DEATHMATCH,
    GAME_TITLE_MAP
  };

  enum ETexturePosition {
    TEXTURE_TOP,
    TEXTURE_MIDDLE,
    TEXTURE_BOTTOM
  };

  enum {
    BLOCK_NOTHING,
    BLOCK_CREATURES,
    BLOCK_EVERYTHING,
    BLOCK_RAILING,
    BLOCK_PLAYERS,
  };

  enum {
    LEVELINFO_PAR_TIME,
    LEVELINFO_CLUSTERNUM,
    LEVELINFO_LEVELNUM,
    LEVELINFO_TOTAL_SECRETS,
    LEVELINFO_FOUND_SECRETS,
    LEVELINFO_TOTAL_ITEMS,
    LEVELINFO_FOUND_ITEMS,
    LEVELINFO_TOTAL_MONSTERS,
    LEVELINFO_KILLED_MONSTERS,
    LEVELINFO_SUCK_TIME
  };

  // flags for ReplaceTextures
  enum {
    NOT_BOTTOM  = 1,
    NOT_MIDDLE  = 2,
    NOT_TOP     = 4,
    NOT_FLOOR   = 8,
    NOT_CEILING = 16,
  };

  enum {
    HUDMSG_PLAIN,
    HUDMSG_FADEOUT,
    HUDMSG_TYPEON,
    HUDMSG_FADEINOUT,

    HUDMSG_LOG         = 0x80000000,
    HUDMSG_COLORSTRING = 0x40000000,
  };

  inline VStr GetStr (int Index) { return ActiveObject->Level->GetString(Index); }
  //inline int PutStr (VStr str) { return ActiveObject->Level->PutString(str); }
  inline VName GetName (int Index) { return *ActiveObject->Level->GetString(Index); }
  inline VName GetNameLowerCase (int Index) { return ActiveObject->Level->GetNameLowerCase(Index); }
  inline VName GetName8 (int Index) { return VName(*ActiveObject->Level->GetString(Index), VName::AddLower8); }

  inline VEntity *EntityFromTID (int TID, VEntity *Default) { return (!TID ? Default : Level->FindMobjFromTID(TID, nullptr)); }

  int FindSectorFromTag (sector_t *&sector, int tag, int start=-1);

  void BroadcastCenterPrint (const char *s) {
    if (destroyed) return; // just in case
    if (!s || !s[0]) return; // oops
    for (int i = 0; i < svs.max_clients; ++i) {
      //k8: check if player is spawned?
      if (Level->Game->Players[i]) Level->Game->Players[i]->eventClientCenterPrint(s);
    }
  }

  void StartSound (const TVec &origin, vint32 origin_id,
                   vint32 sound_id, vint32 channel, float volume, float Attenuation,
                   bool Loop, bool Local=false)
  {
    for (int i = 0; i < MAXPLAYERS; ++i) {
      if (!Level->Game->Players[i]) continue;
      if (!(Level->Game->Players[i]->PlayerFlags&VBasePlayer::PF_Spawned)) continue;
      Level->Game->Players[i]->eventClientStartSound(sound_id, origin, (Local ? -666 : origin_id), channel, volume, Attenuation, Loop);
    }
  }
};


//==========================================================================
//
//  VAcsGrowingArray::Serialise
//
//==========================================================================
void VAcsGrowingArray::Serialise (VStream &Strm) {
  vuint8 xver = 1;
  Strm << xver;
  if (xver != 1) Host_Error("invalid ACS growing array version in save file");
  if (Strm.IsLoading()) {
    values.clear();
    vint32 count = 0;
    Strm << STRM_INDEX(count);
    while (count-- > 0) {
      vint32 index = -1, value = 0;
      Strm << STRM_INDEX(index) << STRM_INDEX(value);
      values.put(index, value);
    }
  } else {
    vint32 count = 0;
    // count elements (we can't trust hashtable for now)
    for (auto it = values.first(); it; ++it) ++count;
    Strm << STRM_INDEX(count);
    // write elements
    for (auto it = values.first(); it; ++it) {
      vint32 index = it.getKey();
      vint32 value = it.getValue();
      Strm << STRM_INDEX(index) << STRM_INDEX(value);
    }
  }
}



//==========================================================================
//
//  VAcsObject::VAcsObject
//
//==========================================================================
VAcsObject::VAcsObject (VAcsLevel *ALevel, int Lump) : Functions(), Level(ALevel) {
  Format = ACS_Unknown;
  LumpNum = Lump;
  LibraryID = 0;
  DataSize = 0;
  Data = nullptr;
  Chunks = nullptr;
  NumScripts = 0;
  Scripts = nullptr;
  //NumFunctions = 0;
  //Functions = nullptr;
  NumStrings = 0;
  Strings = nullptr;
  LowerCaseNames = nullptr;
  NumArrays = 0;
  ArrayStore = nullptr;
  NumTotalArrays = 0;
  Arrays = nullptr;
  memset((void *)MapVarStore, 0, sizeof(MapVarStore));

  VAcsHeader *header;

  if (Lump < 0) return;

  DataSize = W_LumpLength(Lump);

  if (DataSize < (int)sizeof(VAcsHeader)) {
    GCon->Log(NAME_Warning, "Behavior lump too small");
    DataSize = 0;
    return;
  } else {
    VStream *Strm = W_CreateLumpReaderNum(Lump);
    vassert(Strm);
    int datasize = Strm->TotalSize();
    vassert(datasize >= (int)sizeof(VAcsHeader));
    Data = new vuint8[datasize];
    Strm->Serialise(Data, Strm->TotalSize());
    if (Strm->IsError()) memset(Data, 0, datasize);
    delete Strm;
    header = (VAcsHeader *)Data;
  }

  // determine format
  switch (header->Marker[3]) {
    case 0: Format = ACS_Old; break;
    case 'E': Format = ACS_Enhanced; break;
    case 'e': Format = ACS_LittleEnhanced; break;
    default: GCon->Logf(NAME_Warning, "Behavior lump \"%s\" has invalid signature (format)", *W_FullLumpName(Lump)); DataSize = 0; return;
  }
  //if (developer) GCon->Logf(NAME_Dev, "Behavior lump \"%s\" fmt id: %u; fmt=%d", *W_FullLumpName(Lump), (vuint8)header->Marker[3], Format);

  if (Format == ACS_Old) {
    vuint32 dirofs = LittleLong(header->InfoOffset);
    vuint8 *pretag = Data+dirofs-4;

    Chunks = Data+DataSize;
    // check for redesigned ACSE/ACSe
    if (dirofs >= 6*4 && pretag[0] == 'A' &&
        pretag[1] == 'C' && pretag[2] == 'S' &&
        (pretag[3] == 'e' || pretag[3] == 'E'))
    {
      Format = (pretag[3] == 'e' ? ACS_LittleEnhanced : ACS_Enhanced);
      Chunks = Data+LittleLong(*(vint32 *)(Data+dirofs-8));
      // forget about the compatibility cruft at the end of the lump
      DataSize = dirofs-8;
    }
  } else {
    Chunks = Data+LittleLong(header->InfoOffset);
  }

  switch (Format) {
    case ACS_Old: if (developer) GCon->Logf(NAME_Dev, "Behavior lump '%s': standard", *W_FullLumpName(Lump)); break;
    case ACS_Enhanced: if (developer) GCon->Logf(NAME_Dev, "Behavior lump '%s': enhanced", *W_FullLumpName(Lump)); break;
    case ACS_LittleEnhanced: if (developer) GCon->Logf(NAME_Dev, "Behavior lump '%s': enhanced-little", *W_FullLumpName(Lump)); break;
    default: break;
  }

  if (Format == ACS_Old) {
    LoadOldObject();
  } else {
    LoadEnhancedObject();
  }

  // dump all objects
  /*
  for (int i = 0; i < Level->LoadedObjects.length(); ++i) {
    VAcsObject *obj = Level->LoadedObjects[i];
    GCon->Logf(NAME_Debug, "== ACS OBJECT #%d (%s); libid:%d; scripts:%d; strings:%d ===", i, *W_FullLumpName(obj->LumpNum), obj->LibraryID, obj->NumScripts, obj->NumStrings);
    for (int sidx = 0; sidx < obj->NumStrings; ++sidx) GCon->Logf(NAME_Debug, "  string #%d: \"%s\"", sidx, *VStr(obj->Strings[sidx]).quote());
  }
  */
}


//==========================================================================
//
//  VAcsObject::~VAcsObject
//
//==========================================================================
VAcsObject::~VAcsObject () {
  delete[] Scripts;
  Scripts = nullptr;
  delete[] Strings;
  Strings = nullptr;
  delete[] LowerCaseNames;
  LowerCaseNames = nullptr;
  for (int i = 0; i < NumArrays; ++i) {
    delete[] ArrayStore[i].Data;
    ArrayStore[i].Data = nullptr;
  }
  if (ArrayStore) {
    delete[] ArrayStore;
    ArrayStore = nullptr;
  }
  if (Arrays) {
    delete[] Arrays;
    Arrays = nullptr;
  }
  delete[] Data;
  Data = nullptr;
}


//==========================================================================
//
//  VAcsObject::LoadOldObject
//
//==========================================================================
void VAcsObject::LoadOldObject () {
  int i;
  vint32 *buffer;
  VAcsInfo *info;
  VAcsHeader *header;

  // add to loaded objects
  LibraryID = Level->LoadedObjects.Append(this)<<16;

  header = (VAcsHeader *)Data;

  // load script info
  buffer = (vint32 *)(Data+LittleLong(header->InfoOffset));
  NumScripts = LittleLong(*buffer++);
  if (NumScripts == 0) return; // empty behavior lump
  Scripts = new VAcsInfo[NumScripts];
  memset((void *)Scripts, 0, NumScripts*sizeof(VAcsInfo));
  for (i = 0, info = Scripts; i < NumScripts; ++i, ++info) {
    info->Number = LittleLong(*buffer)%1000;
    info->Type = LittleLong(*buffer)/1000;
    ++buffer;
    info->Address = OffsetToPtr(LittleLong(*buffer++));
    info->ArgCount = LittleLong(*buffer++);
    info->Flags = 0;
    info->VarCount = MAX_ACS_SCRIPT_VARS;
    info->Name = NAME_None;
  }

  // load strings
  NumStrings = LittleLong(*buffer++);
  Strings = new char*[NumStrings];
  LowerCaseNames = new VName[NumStrings];
  for (i = 0; i < NumStrings; ++i) {
    Strings[i] = (char *)Data+LittleLong(buffer[i]);
    LowerCaseNames[i] = NAME_None;
  }

  // set up map vars
  memset((void *)MapVarStore, 0, sizeof(MapVarStore));
  for (i = 0; i < MAX_ACS_MAP_VARS; ++i) {
    MapVars[i] = &MapVarStore[i];
  }
}


//==========================================================================
//
//  ParseLocalArrayChunk
//
//==========================================================================
static int ParseLocalArrayChunk (const void *chunk, VACSLocalArrays *arrays, int offset) {
  int count = LittleUShort(((const vuint32 *)chunk)[1]-2)/4;
  arrays->SetCount(count);
  //GCon->Logf(NAME_Debug, " count=%d (%d)", count, arrays->Count);
  if (arrays->Count > 0) {
    const vint32 *sizes = (const vint32 *)((const vuint8 *)chunk+10);
    VACSLocalArrayInfo *info = arrays->Info;
    for (int i = 0; i < count; ++i, ++info, ++sizes) {
      info->Size = LittleLong(*sizes);
      info->Offset = offset;
      if (info->Size < 0 || info->Offset < 0) Sys_Error("invalid acs array descritption (offset=%d; size=%d)", info->Offset, info->Size);
      //GCon->Logf(NAME_Debug, "  array #%d, offset=%d, size=%d", i, info->Offset, info->Size);
      offset += info->Size;
    }
  }
  // return the new local variable size, with space for the arrays
  return offset;
}


//==========================================================================
//
//  VAcsObject::LoadEnhancedObject
//
//==========================================================================
void VAcsObject::LoadEnhancedObject () {
  int i;
  vint32 *buffer;
  VAcsInfo *info;

  /*
  { // dump all chunks
    const vuint8 *chunk = Chunks;
    while (chunk && chunk < Data+DataSize) {
      char tmp[5];
      memcpy(tmp, chunk, 4);
      tmp[4] = 0;
      GCon->Logf("   CHUNK: <%s> at 0x%08x", tmp, (unsigned)(ptrdiff_t)(chunk-Chunks));
      chunk = chunk+LittleLong(((vint32 *)chunk)[1])+8;
    }
  }
  */

  // load scripts
  buffer = (vint32 *)FindChunk("SPTR");
  if (buffer) {
    if (Data[3] != 0) {
      NumScripts = LittleLong(buffer[1])/12;
      Scripts = new VAcsInfo[NumScripts];
      memset((void *)Scripts, 0, NumScripts*sizeof(VAcsInfo));
      buffer += 2;

      for (i = 0, info = Scripts; i < NumScripts; ++i, ++info) {
        info->Number = LittleUShort(*(vuint16 *)buffer);
        info->Type = LittleUShort(((vuint16 *)buffer)[1]);
        ++buffer;
        info->Address = OffsetToPtr(LittleLong(*buffer++));
        info->ArgCount = LittleLong(*buffer++);
        info->Flags = 0;
        info->VarCount = MAX_ACS_SCRIPT_VARS;
        info->Name = NAME_None;
      }
    } else {
      NumScripts = LittleLong(buffer[1])/8;
      Scripts = new VAcsInfo[NumScripts];
      memset((void *)Scripts, 0, NumScripts*sizeof(VAcsInfo));
      buffer += 2;

      for (i = 0, info = Scripts; i < NumScripts; ++i, ++info) {
        info->Number = LittleUShort(*(vuint16 *)buffer);
        info->Type = ((vuint8 *)buffer)[2];
        info->ArgCount = ((vuint8 *)buffer)[3];
        ++buffer;
        info->Address = OffsetToPtr(LittleLong(*buffer++));
        info->Flags = 0;
        info->VarCount = MAX_ACS_SCRIPT_VARS;
        info->Name = NAME_None;
      }
    }
  } else {
    // wutafu? no scripts!
    //FIXME: better message
    GCon->Logf(NAME_Warning, "ACS file '%s' has no scripts (it is ok for library).", *W_FullLumpName(LumpNum));
    NumScripts = 0;
    Scripts = new VAcsInfo[1];
    memset((void *)Scripts, 0, 1*sizeof(VAcsInfo));
  }

  // load script flags
  buffer = (vint32 *)FindChunk("SFLG");
  if (buffer) {
    int count = LittleLong(buffer[1])/4;
    buffer += 2;
    for (i = 0; i < count; ++i, ++buffer) {
      info = FindScript(LittleUShort(((vuint16 *)buffer)[0]));
      if (info) {
        info->Flags = LittleUShort(((vuint16 *)buffer)[1]);
      }
    }
  }

  // load script var counts
  buffer = (vint32 *)FindChunk("SVCT");
  if (buffer) {
    int count = LittleLong(buffer[1])/4;
    buffer += 2;
    for (i = 0; i < count; ++i, ++buffer) {
      info = FindScript(LittleUShort(((vuint16 *)buffer)[0]));
      if (info) {
        info->VarCount = LittleUShort(((vuint16 *)buffer)[1]);
        // make sure it's at least 8 so in SpawnScript we can safely assign args to first 8 variables
        if (info->VarCount < 8) info->VarCount = 8;
      }
    }
  }

  // load functions
  buffer = (vint32 *)FindChunk("FUNC");
  if (buffer) {
    int NumFunctions = LittleLong(buffer[1])/8;
    if (NumFunctions < 0 || NumFunctions > 1024*1024) Sys_Error("ACS file '%s' has invalid function count (%d)", *W_FullLumpName(LumpNum), NumFunctions);
    Functions.setLength(NumFunctions);
    VAcsFunctionChunkData *funccd = (VAcsFunctionChunkData *)(buffer+2);
    for (i = 0; i < NumFunctions; ++i, ++funccd) {
      funccd->Address = LittleLong(funccd->Address);
      Functions[i].SetupFrom(*funccd);
    }
  }

  // unencrypt strings
  UnencryptStrings();

  // a temporary hack
  buffer = (vint32 *)FindChunk("STRL");
  if (buffer) {
    buffer += 2;
    NumStrings = LittleLong(buffer[1]);
    Strings = new char*[NumStrings];
    LowerCaseNames = new VName[NumStrings];
    for (i = 0; i < NumStrings; ++i) {
      Strings[i] = (char *)buffer+LittleLong(buffer[i+3]);
      LowerCaseNames[i] = NAME_None;
    }
  }

  // load local arrays for functions
  if (Functions.length() > 0) {
    for (buffer = (vint32 *)FindChunk("FARY"); buffer; buffer = (vint32 *)NextChunk((vuint8 *)buffer)) {
      int size = LittleLong(buffer[1]);
      if (size >= 6) {
        int funcnum = LittleUShort(((vuint16 *)buffer)[4]);
        if (funcnum >= 0 && funcnum < Functions.length()) {
          VAcsFunction *func = &Functions[funcnum];
          // unlike scripts, functions do not include their arg count in their local count
          GCon->Logf(NAME_Debug, "ACS file '%s', function #%d has local arrays", *W_FullLumpName(LumpNum), funcnum);
          func->LocalCount = ParseLocalArrayChunk(buffer, &func->LocalArrays, func->LocalCount+func->ArgCount)-func->ArgCount;
        }
      }
    }
  }

  // initialise this object's map variable pointers to defaults
  // they can be changed later once the imported modules are loaded
  for (i = 0; i < MAX_ACS_MAP_VARS; ++i) {
    MapVars[i] = &MapVarStore[i];
  }

  // initialise this object's map variables
  memset((void *)MapVarStore, 0, sizeof(MapVarStore));
  buffer = (vint32 *)FindChunk("MINI");
  while (buffer) {
    int numvars = LittleLong(buffer[1])/4-1;
    int firstvar = LittleLong(buffer[2]);
    for (i = 0; i < numvars; ++i) {
      MapVarStore[firstvar+i] = LittleLong(buffer[3+i]);
    }
    buffer = (vint32 *)NextChunk((vuint8 *)buffer);
  }

  // create arrays
  buffer = (vint32 *)FindChunk("ARAY");
  if (buffer) {
    NumArrays = LittleLong(buffer[1])/8;
    ArrayStore = new VArrayInfo[NumArrays];
    memset((void *)ArrayStore, 0, sizeof(*ArrayStore)*NumArrays);
    for (i = 0; i < NumArrays; ++i) {
      MapVarStore[LittleLong(buffer[2+i*2])] = i;
      ArrayStore[i].Size = LittleLong(buffer[3+i*2]);
      ArrayStore[i].Data = new vint32[ArrayStore[i].Size];
      memset((void *)ArrayStore[i].Data, 0, ArrayStore[i].Size*sizeof(vint32));
    }
  }

  // initialise arrays
  buffer = (vint32 *)FindChunk("AINI");
  while (buffer) {
    int arraynum = MapVarStore[LittleLong(buffer[2])];
    if ((unsigned)arraynum < (unsigned)NumArrays) {
      int initsize = (LittleLong(buffer[1])-4)/4;
      if (initsize > ArrayStore[arraynum].Size) initsize = ArrayStore[arraynum].Size;
      vint32 *elems = ArrayStore[arraynum].Data;
      /*
      for (i = 0; i < initsize; i++) {
        elems[i] = LittleLong(buffer[3 + i]);
      }
      */
#if 0
      // gcc wants to optimise this with SSE instructions, and for some reason it believes that the buffer is aligned.
      // most of the time it is, but no guarantees are given. fuck.
      memcpy(elems, buffer+3, initsize*4);
      for (i = 0; i < initsize; ++i) elems[i] = LittleLong(elems[i]);
#else
      for (i = 0; i < initsize; ++i) elems[i] = LittleLong(buffer[3+i]);
#endif
    }
    buffer = (vint32 *)NextChunk((vuint8 *)buffer);
  }

  // start setting up array pointers
  NumTotalArrays = NumArrays;
  buffer = (vint32 *)FindChunk("AIMP");
  if (buffer) NumTotalArrays += LittleLong(buffer[2]);
  if (NumTotalArrays) {
    Arrays = new VArrayInfo*[NumTotalArrays];
    for (i = 0; i < NumArrays; ++i) {
      Arrays[i] = &ArrayStore[i];
    }
  }

  // Now that everything is set up, record this object as being among
  // the loaded objects. We need to do this before resolving any imports,
  // because an import might (indirectly) need to resolve exports in this
  // module. The only things that can be exported are functions and map
  // variables, which must already be present if they're exported, so this
  // is okay.
  LibraryID = Level->LoadedObjects.Append(this)<<16;

  // tag the library ID to any map variables that are initialised with strings
  if (LibraryID) {
    buffer = (vint32 *)FindChunk("MSTR");
    if (buffer) {
      for (i = 0; i < LittleLong(buffer[1])/4; ++i) {
        //MapVarStore[LittleLong(buffer[i + 2])] |= LibraryID;
        int sidx = LittleLong(buffer[i+2]);
        const char *str = (sidx < 0 || sidx >= NumStrings ? "" : Strings[sidx]);
        MapVarStore[LittleLong(buffer[i+2])] = Level->PutNewString(str);
      }
    }

    buffer = (vint32 *)FindChunk("ASTR");
    if (buffer) {
      for (i = 0; i < LittleLong(buffer[1])/4; ++i) {
        int arraynum = MapVarStore[LittleLong(buffer[i+2])];
        if ((unsigned)arraynum < (unsigned)NumArrays) {
          vint32 *elems = ArrayStore[arraynum].Data;
          for (int j = ArrayStore[arraynum].Size; j > 0; --j, ++elems) {
            //*elems |= LibraryID;
            int sidx = *elems;
            const char *str = (sidx < 0 || sidx >= NumStrings ? "" : Strings[sidx]);
            *elems = Level->PutNewString(str);
          }
        }
      }
    }

    // [BL] Newer version of ASTR for structure aware compilers although we only have one array per chunk
    vuint32 *chunk = (vuint32 *)FindChunk("ATAG");
    while (chunk != nullptr) {
      const uint8_t* chunkData = (const uint8_t *)(chunk+2);
      // first byte is version, it should be 0
      if (*chunkData++ == 0) {
        int arraynum = MapVarStore[LittleLong(*(vint32 *)(chunkData))];
        chunkData += 4;
        if ((unsigned)arraynum < (unsigned)NumArrays) {
          vint32 *elems = ArrayStore[arraynum].Data;
          // ending zeros may be left out
          for (int j = min2(LittleLong(chunk[1])-5, ArrayStore[arraynum].Size); j > 0; --j, ++elems, ++chunkData) {
            // For ATAG, a value of 0 = Integer, 1 = String, 2 = FunctionPtr
            // Our implementation uses the same tags for both String and FunctionPtr
            if (*chunkData == 2) {
              //k8:FIXME: this is prolly wrong
              *elems |= LibraryID;
            } else if (*chunkData == 1) {
              //const char *str = LookupString(*elems);
              int sidx = *elems;
              const char *str = (sidx < 0 || sidx >= NumStrings ? "" : Strings[sidx]);
              *elems = Level->PutNewString(str);
            }
          }
        }
      }
      // next tag chunk
      chunk = (uint32_t *)NextChunk ((uint8_t *)chunk);
    }
  }

  // library loading
  buffer = (vint32 *)FindChunk("LOAD");
  if (buffer) {
    char *parse = (char *)&buffer[2];
    for (i = 0; i < LittleLong(buffer[1]); ++i) {
      if (parse[i]) {
        if (developer) GCon->Logf(NAME_Dev, "acs linked library '%s' (for object '%s')", &parse[i], *W_FullLumpName(LumpNum));
        VAcsObject *Object = nullptr;
        //int Lump = W_CheckNumForName(VName(&parse[i], VName::AddLower8), WADNS_ACSLibrary);
        int Lump = W_FindACSObjectInFile(VStr(&parse[i]), W_LumpFile(LumpNum));
        if (Lump < 0) {
          GCon->Logf(NAME_Warning, "Could not find ACS library '%s'.", &parse[i]);
        } else {
          GCon->Logf("  ACS linked library '%s' (%s) (for object '%s')", &parse[i], *W_FullLumpName(Lump), *W_FullLumpName(LumpNum));
          Object = Level->LoadObject(Lump);
        }
        Imports.Append(Object);
        do {} while (parse[++i]);
      }
    }

    // go through each imported object in order and resolve all imported functions and map variables
    for (i = 0; i < Imports.Num(); ++i) {
      VAcsObject *lib = Imports[i];
      int j;

      if (!lib) continue;

      // resolve functions
      buffer = (vint32 *)FindChunk("FNAM");
      for (j = 0; j < Functions.length(); ++j) {
        VAcsFunction *func = &Functions[j];
        if (func->Address != 0 || func->ImportNum != 0) continue;

        int libfunc = lib->FindFunctionName((char *)(buffer+2)+LittleLong(buffer[3+j]));
        if (libfunc < 0) continue;

        VAcsFunction *realfunc = &lib->Functions[libfunc];
        // make sure that the library really defines this function
        // it might simply be importing it itself
        if (realfunc->Address == 0 || realfunc->ImportNum != 0) continue;

        func->Address = libfunc;
        func->ImportNum = i+1;
        if (realfunc->ArgCount != func->ArgCount) {
          GCon->Logf(NAME_Warning, "ACS: Function %s in %s has %d arguments. %s expects it to have %d.",
            (char *)(buffer+2)+LittleLong(buffer[3+j]),
            *W_LumpName(lib->LumpNum), realfunc->ArgCount,
            *W_LumpName(LumpNum), func->ArgCount);
          Format = ACS_Unknown;
        }
        // the next two properties do not effect code compatibility,
        // so it is okay for them to be different in the imported
        // module than they are in this one, as long as we make sure
        // to use the real values
        func->LocalCount = realfunc->LocalCount;
        func->HasReturnValue = realfunc->HasReturnValue;
      }

      // resolve map variables
      buffer = (vint32 *)FindChunk("MIMP");
      if (buffer) {
        parse = (char *)&buffer[2];
        for (j = 0; j < LittleLong(buffer[1]); ++j) {
          int varNum = LittleLong(*(vint32 *)&parse[j]);
          j += 4;
          int impNum = lib->FindMapVarName(&parse[j]);
          if (impNum >= 0) {
            MapVars[varNum] = &lib->MapVarStore[impNum];
          }
          do {} while (parse[++j]);
        }
      }

      // resolve arrays
      if (NumTotalArrays > NumArrays) {
        buffer = (vint32 *)FindChunk("AIMP");
        parse = (char *)&buffer[3];
        for (j = 0; j < LittleLong(buffer[2]); ++j) {
          int varNum = LittleLong(*(vint32 *)parse);
          parse += 4;
          int expectedSize = LittleLong(*(vint32 *)parse);
          parse += 4;
          int impNum = lib->FindMapArray(parse);
          if (impNum >= 0) {
            Arrays[NumArrays+j] = &lib->ArrayStore[impNum];
            MapVarStore[varNum] = NumArrays+j;
            if (lib->ArrayStore[impNum].Size != expectedSize) {
              Format = ACS_Unknown;
              GCon->Logf(NAME_Warning, "ACS: The array %s in %s has %d elements, but %s expects it to only have %d.",
                parse, *W_LumpName(lib->LumpNum),
                (int)lib->ArrayStore[impNum].Size,
                *W_LumpName(LumpNum), (int)expectedSize);
            }
          }
          do {} while (*++parse);
          ++parse;
        }
      }
    }
  }

  // load script names (if any)
  buffer = (vint32 *)FindChunk("SNAM");
  if (buffer) {
    int size = LittleLong(buffer[1]);
    buffer += 2; // skip name and size
    int count = LittleLong(buffer[0]);
    if (count > 0 && size > 0) {
      bool valid = true;
      char **sbuf = new char *[count];
      for (int f = 0; f < count; ++f) {
        int ofs = LittleLong(buffer[1+f]);
        if (ofs < 0 || ofs >= size) { valid = false; break; }
        char *e = (char *)memchr(((vuint8 *)buffer)+ofs, 0, size-ofs);
        if (!e) { valid = false; break; }
        sbuf[f] = ((char *)buffer)+ofs;
      }
      if (valid) {
        //GCon->Logf("ACS SNAM: %d names found", count);
        //for (int scnum = 0; scnum < NumScripts; ++scnum) GCon->Logf("ACS: script #%d has index #%d (%d)", scnum, Scripts[scnum].Number, (vint16)Scripts[scnum].Number);
        for (int nameidx = 0; nameidx < count; ++nameidx) {
          //GCon->Logf("  #%d: <%s>", nameidx, sbuf[nameidx]);
          // ACC stores script names as an index into the SNAM chunk,
          // with the first index as -1 and counting down from there.
          // note that script number is 16 bit.
          // let's find a script and assign name to it
          int scnum;
          for (scnum = 0; scnum < NumScripts; ++scnum) {
            int num = (vint16)Scripts[scnum].Number;
            //GCon->Logf("ACS: is %d == %d? %d", -(nameidx+1), num, (num == -(nameidx+1) ? 1 : 0));
            if (num == -(nameidx+1)) break;
          }
          if (scnum >= NumScripts) {
            GCon->Logf(NAME_Warning, "ACS: cannot assign name '%s' to script %d", sbuf[nameidx], -(nameidx+1));
          } else {
            //GCon->Logf("ACS: assigned name '%s' to script %d (%d; index=%d (%d))", sbuf[nameidx], -(nameidx+1), scnum, Scripts[scnum].Number, (vint16)Scripts[scnum].Number);
            Scripts[scnum].Name = VName(sbuf[nameidx], VName::AddLower);
          }
        }
      } else {
        GCon->Logf(NAME_Error, "ACS ERROR: invalid `SNAM` chunk!");
      }
      delete [] sbuf;
    }
  }

  // load script array sizes (one chunk per script that uses arrays)
  for (buffer = (vint32 *)FindChunk("SARY"); buffer; buffer = (vint32 *)NextChunk((vuint8 *)buffer)) {
    int size = LittleLong(buffer[1]);
    if (size >= 6) {
      int scnum = LittleUShort(((vuint16 *)buffer)[4]);
      info = FindScript(scnum);
      if (info) {
        GCon->Logf(NAME_Debug, "SARY: found SARY for script #%d", scnum);
        info->VarCount = ParseLocalArrayChunk(buffer, &info->LocalArrays, info->VarCount);
        //if (info->VarCount >= VAcs::MAX_LOCAL_VARS-1) Sys_Error("too many locals in script with local arrays #%d", scnum);
      } else {
        GCon->Logf(NAME_Debug, "SARY: unknown SARY for script #%d", scnum);
      }
    }
  }
}


//==========================================================================
//
//  VAcsObject::UnencryptStrings
//
//==========================================================================
void VAcsObject::UnencryptStrings () {
  vuint8 *prevchunk = nullptr;
  vuint32 *chunk = (vuint32 *)FindChunk("STRE");
  while (chunk) {
    for (int strnum = 0; strnum < LittleLong(chunk[3]); ++strnum) {
      int ofs = LittleLong(chunk[5+strnum]);
      vuint8 *data = (vuint8 *)chunk+ofs+8;
      vuint8 last;
      int p = (vuint8)(ofs*157135);
      int i = 0;
      do {
        last = (data[i] ^= (vuint8)(p+(i>>1)));
        ++i;
      } while (last != 0);
    }
    prevchunk = (vuint8 *)chunk;
    chunk = (vuint32 *)NextChunk((vuint8 *)chunk);
    prevchunk[3] = 'L';
  }
  if (prevchunk) prevchunk[3] = 'L';
}


//==========================================================================
//
//  VAcsObject::FindFunctionName
//
//==========================================================================
int VAcsObject::FindFunctionName (const char *Name) const {
  return FindStringInChunk(FindChunk("FNAM"), Name);
}


//==========================================================================
//
//  VAcsObject::FindMapVarName
//
//==========================================================================
int VAcsObject::FindMapVarName (const char *Name) const {
  return FindStringInChunk(FindChunk("MEXP"), Name);
}


//==========================================================================
//
//  VAcsObject::FindMapArray
//
//==========================================================================
int VAcsObject::FindMapArray (const char *Name) const {
  int var = FindMapVarName(Name);
  if (var >= 0) return MapVarStore[var];
  return -1;
}


//==========================================================================
//
//  VAcsObject::FindStringInChunk
//
//==========================================================================
int VAcsObject::FindStringInChunk (vuint8 *Chunk, const char *Name) const {
  if (Chunk) {
    int count = LittleLong(((vint32 *)Chunk)[2]);
    for (int i = 0; i < count; ++i) {
      if (VStr::strEquCI(Name, (char *)(Chunk+8)+LittleLong(((vint32 *)Chunk)[3+i]))) {
        return i;
      }
    }
  }
  return -1;
}


//==========================================================================
//
//  VAcsObject::FindChunk
//
//==========================================================================
vuint8 *VAcsObject::FindChunk (const char *id) const {
  vuint8 *chunk = Chunks;
  while (chunk && chunk < Data+DataSize) {
    if (*(vint32 *)chunk == *(vint32 *)id) return chunk;
    chunk = chunk+LittleLong(((vint32 *)chunk)[1])+8;
  }
  return nullptr;
}


//==========================================================================
//
//  VAcsObject::NextChunk
//
//==========================================================================
vuint8 *VAcsObject::NextChunk (vuint8 *prev) const {
  int id = *(vint32 *)prev;
  vuint8 *chunk = prev+LittleLong(((vint32 *)prev)[1])+8;
  while (chunk && chunk < Data+DataSize) {
    if (*(vint32 *)chunk == id) return chunk;
    chunk = chunk+LittleLong(((vint32 *)chunk)[1])+8;
  }
  return nullptr;
}


//==========================================================================
//
//  VAcsObject::Serialise
//
//==========================================================================
void VAcsObject::Serialise (VStream &Strm) {
  vuint8 xver = 1; // current version is 1 (fuck v0)
  Strm << xver;
  if (xver != 1) Host_Error("invalid ACS object version in save file");

  //GCon->Logf("serializing ACS object");

  // scripts
  vint32 scriptCount = NumScripts;
  Strm << STRM_INDEX(scriptCount);
  if (Strm.IsLoading()) {
    if (scriptCount != NumScripts) Host_Error("invalid number of ACS scripts in save file");
  }

  //fprintf(stderr, "ACS COUNT=%d\n", scriptCount);
  for (int i = 0; i < scriptCount; ++i) {
    VSerialisable *vth = Scripts[i].RunningScript;
    Strm << vth;
    if (vth && vth->GetClassName() != "VAcs") Host_Error("Save is broken (loaded `%s` instead of `VAcs`)", *vth->GetClassName());
    Scripts[i].RunningScript = (VAcs *)vth;
  }

  // map variables
  int mapvarCount = MAX_ACS_MAP_VARS;
  Strm << STRM_INDEX(mapvarCount);
  if (Strm.IsLoading()) {
    //if (mapvarCount < 0 || mapvarCount > MAX_ACS_MAP_VARS) Host_Error("invalid number of ACS map variables in save file");
    memset((void *)MapVarStore, 0, sizeof(MapVarStore));
    while (mapvarCount-- > 0) {
      vint32 index, value;
      Strm << STRM_INDEX(index) << STRM_INDEX(value);
      if (index < 0 || index >= MAX_ACS_MAP_VARS) Host_Error("invalid ACS map variable index in save file");
      MapVarStore[index] = value;
    }
  } else {
    for (int i = 0; i < MAX_ACS_MAP_VARS; ++i) {
      vint32 index = i, value = MapVarStore[i];
      Strm << STRM_INDEX(index) << STRM_INDEX(value);
    }
  }

  // arrays
  vint32 arrCount = NumArrays;
  Strm << STRM_INDEX(arrCount);
  if (Strm.IsLoading()) {
    if (arrCount != NumArrays) Host_Error("invalid number of ACS arrays in save file");
  }
  for (int i = 0; i < NumArrays; ++i) {
    vint32 asize = ArrayStore[i].Size;
    Strm << STRM_INDEX(asize);
    if (Strm.IsLoading()) {
      if (asize != ArrayStore[i].Size) Host_Error("invalid ACS script array #%d size in save file", i);
    }
    for (int j = 0; j < ArrayStore[i].Size; ++j) Strm << STRM_INDEX(ArrayStore[i].Data[j]);
  }
}


//==========================================================================
//
//  VAcsObject::OffsetToPtr
//
//==========================================================================
vuint8 *VAcsObject::OffsetToPtr (int Offs) {
  if (Offs < 0 || Offs >= DataSize) Host_Error("Bad offset in ACS file");
  return Data+Offs;
}


//==========================================================================
//
//  VAcsObject::PtrToOffset
//
//==========================================================================
int VAcsObject::PtrToOffset (vuint8 *Ptr) {
  return Ptr-Data;
}


//==========================================================================
//
//  VAcsObject::FindScript
//
//==========================================================================
VAcsInfo *VAcsObject::FindScript (int Number) const {
  if (Number <= -SPECIAL_LOW_SCRIPT_NUMBER) {
    Number = -(Number+SPECIAL_LOW_SCRIPT_NUMBER);
    return &Scripts[Number];
  }
  for (int i = 0; i < NumScripts; ++i) {
    if (Scripts[i].Number == Number) return Scripts+i;
  }
  return nullptr;
}


//==========================================================================
//
//  VAcsObject::FindScriptByName
//
//==========================================================================
VAcsInfo *VAcsObject::FindScriptByName (int nameidx) const {
  if (nameidx == 0) return nullptr;
  if (nameidx < 0) {
    nameidx = -nameidx;
    if (nameidx < 0) return nullptr;
  }
  //for (int i = 0; i < NumScripts; i++) fprintf(stderr, "#%d: index=%d; name=<%s>\n", i, Scripts[i].Number, *Scripts[i].Name); abort();
  //FIXME: make this faster by using hash table
  for (int i = 0; i < NumScripts; ++i) {
    if (Scripts[i].Name.GetIndex() == nameidx) return Scripts+i;
  }
  return nullptr;
}


//==========================================================================
//
//  VAcsObject::FindScriptByNameStr
//
//==========================================================================
VAcsInfo *VAcsObject::FindScriptByNameStr (VStr aname) const {
  if (aname.length() == 0) return nullptr;
  VName nn = VName(*aname, VName::FindLower);
  if (nn == NAME_None) return nullptr;
  for (int i = 0; i < NumScripts; ++i) {
    if (Scripts[i].Name == nn) return Scripts+i;
  }
  return nullptr;
}


//==========================================================================
//
//  VAcsObject::FindScriptNumberByName
//
//==========================================================================
int VAcsObject::FindScriptNumberByName (VStr aname) const {
  if (aname.length() == 0) return -1;
  VName nn = VName(*aname, VName::FindLower);
  if (nn == NAME_None) return -1;
  for (int i = 0; i < NumScripts; ++i) {
    if (Scripts[i].Name == nn) return -SPECIAL_LOW_SCRIPT_NUMBER-i;
  }
  return -1;
}


//==========================================================================
//
//  VAcsObject::GetFunction
//
//==========================================================================
VAcsFunction *VAcsObject::GetFunction (int funcnum, VAcsObject *&Object) {
  if ((unsigned)funcnum >= (unsigned)Functions.length()) return nullptr;
  //GCon->Logf(NAME_Debug, "%p: GetFunction: funcnum=%d; count=%d", (void *)this, funcnum, Functions.length());
  VAcsFunction *Func = &Functions[funcnum];
  //GCon->Logf(NAME_Debug, "%p:   %d: addr=%u; import=%u", (void *)this, funcnum, Func->Address, Func->ImportNum);
  if (Func->ImportNum) return Imports[Func->ImportNum-1]->GetFunction(Func->Address, Object);
  Object = this;
  return Func;
}


//==========================================================================
//
//  VAcsObject::GetArrayVal
//
//==========================================================================
int VAcsObject::GetArrayVal (int ArrayIdx, int Index) {
  if ((unsigned)ArrayIdx >= (unsigned)NumTotalArrays) {
    //GCon->Logf(NAME_Debug, "ACS: VAcsObject::GetArrayVal: invalid array index %d (max is %d)", ArrayIdx, NumTotalArrays-1);
    return 0;
  }
  if ((unsigned)Index >= (unsigned)Arrays[ArrayIdx]->Size) {
    //GCon->Logf(NAME_Debug, "ACS: VAcsObject::GetArrayVal: invalid array %d element index %d (max is %d)", ArrayIdx, Index, Arrays[ArrayIdx]->Size-1);
    return 0;
  }
  //GCon->Logf(NAME_Debug, "ACS: VAcsObject::GetArrayVal: array %d element index %d: value=%d", ArrayIdx, Index, Arrays[ArrayIdx]->Data[Index]);
  return Arrays[ArrayIdx]->Data[Index];
}


//==========================================================================
//
//  VAcsObject::SetArrayVal
//
//==========================================================================
void VAcsObject::SetArrayVal (int ArrayIdx, int Index, int Value) {
  if ((unsigned)ArrayIdx >= (unsigned)NumTotalArrays) return;
  if ((unsigned)Index >= (unsigned)Arrays[ArrayIdx]->Size) return;
  Arrays[ArrayIdx]->Data[Index] = Value;
}


//==========================================================================
//
//  VAcsObject::StartTypedACScripts
//
//==========================================================================
void VAcsObject::StartTypedACScripts (int Type, int Arg1, int Arg2, int Arg3, /*int Arg4,*/
                                      VEntity *Activator, bool Always, bool RunNow)
{
  for (int i = 0; i < NumScripts; ++i) {
    if (Scripts[i].Type == Type) {
      // auto-activate
      VAcs *Script = Level->SpawnScript(&Scripts[i], this, Activator, nullptr, 0, Arg1, Arg2, Arg3, 0, Always, !RunNow, false); // always register thinker
      if (RunNow && Script) {
        VLevel *lvl = Script->XLevel;
        Script->RunScript(0/*host_frametime:doesn't matter*/, true);
        if (Script->destroyed) {
          lvl->RemoveScriptThinker(Script);
          delete Script;
        }
      }
    }
  }
}


//==========================================================================
//
//  VAcsLevel::VAcsLevel
//
//==========================================================================
VAcsLevel::VAcsLevel (VLevel *ALevel)
  : stringMapByStr()
  , stringList()
  , XLevel(ALevel)
{
}


//==========================================================================
//
//  VAcsLevel::~VAcsLevel
//
//==========================================================================
VAcsLevel::~VAcsLevel () {
  for (int i = 0; i < LoadedObjects.Num(); ++i) {
    delete LoadedObjects[i];
    LoadedObjects[i] = nullptr;
  }
  LoadedObjects.Clear();
}


//==========================================================================
//
//  VAcsLevel::GetNewString
//
//==========================================================================
VStr VAcsLevel::GetNewString (int idx) {
  idx &= 0xffff;
  return (idx < stringList.length() ? stringList[idx] : VStr());
}


//==========================================================================
//
//  VAcsLevel::GetNewLowerName
//
//==========================================================================
VName VAcsLevel::GetNewLowerName (int idx) {
  idx &= 0xffff;
  if (idx >= stringList.length()) return NAME_None;
  VStr s = stringList[idx];
  if (s.length() == 0) return NAME_None;
  return VName(*(s.toLowerCase()));
}


//==========================================================================
//
//  VAcsLevel::PutNewString
//
//==========================================================================
int VAcsLevel::PutNewString (VStr str) {
  //k8:this is wrong!:if (str.length() == 0) return 0; // string 0 is always empty, and scripts rely on this
  auto idxp = stringMapByStr.find(str);
  if (idxp) return (*idxp)|(ACSLEVEL_INTERNAL_STRING_STORAGE_INDEX<<16);
  // add string
  int idx = stringList.length();
  if (idx == 0xffff) Host_Error("ACS dynamic string storage overflow");
  stringList.append(str);
  stringMapByStr.put(str, idx);
  return idx|(ACSLEVEL_INTERNAL_STRING_STORAGE_INDEX<<16);
}


//==========================================================================
//
//  VAcsLevel::LoadObject
//
//==========================================================================
VAcsObject *VAcsLevel::LoadObject (int Lump) {
  for (int i = 0; i < LoadedObjects.Num(); ++i) {
    if (LoadedObjects[i]->LumpNum == Lump) {
      return LoadedObjects[i];
    }
  }
  return new VAcsObject(this, Lump);
}


//==========================================================================
//
//  VAcsLevel::FindScript
//
//==========================================================================
VAcsInfo *VAcsLevel::FindScript (int Number, VAcsObject *&Object) {
  for (int i = 0; i < LoadedObjects.Num(); ++i) {
    VAcsInfo *Found = LoadedObjects[i]->FindScript(Number);
    if (Found) {
      Object = LoadedObjects[i];
      return Found;
    }
  }
  return nullptr;
}


//==========================================================================
//
//  VAcsLevel::FindScriptByName
//
//==========================================================================
VAcsInfo *VAcsLevel::FindScriptByName (int Number, VAcsObject *&Object) {
  for (int i = 0; i < LoadedObjects.Num(); ++i) {
    VAcsInfo *Found = LoadedObjects[i]->FindScriptByName(Number);
    if (Found) {
      Object = LoadedObjects[i];
      return Found;
    }
  }
  return nullptr;
}


//==========================================================================
//
//  VAcsLevel::FindScriptByNameStr
//
//==========================================================================
VAcsInfo *VAcsLevel::FindScriptByNameStr (VStr aname, VAcsObject *&Object) {
  if (aname.length() == 0) return nullptr;
  VName nn = VName(*aname, VName::FindLower);
  if (nn == NAME_None) return nullptr;
  for (int i = 0; i < LoadedObjects.Num(); ++i) {
    VAcsInfo *Found = LoadedObjects[i]->FindScriptByName(-nn.GetIndex());
    if (Found) {
      Object = LoadedObjects[i];
      return Found;
    }
  }
  return nullptr;
}


//==========================================================================
//
//  VAcsLevel::FindScriptNumberByName
//
//==========================================================================
int VAcsLevel::FindScriptNumberByName (VStr aname, VAcsObject *&Object) {
  if (aname.length() == 0) return -1;
  for (int i = 0; i < LoadedObjects.length(); ++i) {
    int idx = LoadedObjects[i]->FindScriptNumberByName(aname);
    if (idx <= -SPECIAL_LOW_SCRIPT_NUMBER) {
      Object = LoadedObjects[i];
      return idx;
    }
  }
  return -1;
}


//==========================================================================
//
//  VAcsLevel::GetString
//
//==========================================================================
VStr VAcsLevel::GetString (int Index) {
  int ObjIdx = (vuint32)Index>>16;
  if (ObjIdx == ACSLEVEL_INTERNAL_STRING_STORAGE_INDEX) return GetNewString(Index&0xffff);
  if (ObjIdx >= LoadedObjects.Num()) return "";
  return LoadedObjects[ObjIdx]->GetString(Index&0xffff);
}


//==========================================================================
//
//  VAcsLevel::GetNameLowerCase
//
//==========================================================================
VName VAcsLevel::GetNameLowerCase (int Index) {
  //GCon->Logf(NAME_Debug, "VAcsLevel::GetNameLowerCase: index=0x%08x", (vuint32)Index);
  int ObjIdx = (vuint32)Index>>16;
  if (ObjIdx == ACSLEVEL_INTERNAL_STRING_STORAGE_INDEX) {
    //GCon->Logf(NAME_Debug, "VAcsLevel::GetNameLowerCase: INTERNAL: '%s'", *GetNewLowerName(Index&0xffff));
    return GetNewLowerName(Index&0xffff);
  }
  if (ObjIdx >= LoadedObjects.Num()) return NAME_None;
  //GCon->Logf(NAME_Debug, "VAcsLevel::GetNameLowerCase: object #%d: '%s'", ObjIdx, *LoadedObjects[ObjIdx]->GetNameLowerCase(Index&0xffff));
  return LoadedObjects[ObjIdx]->GetNameLowerCase(Index&0xffff);
}


//==========================================================================
//
//  VAcsLevel::GetObject
//
//==========================================================================
VAcsObject *VAcsLevel::GetObject (int Index) {
  if ((unsigned)Index >= (unsigned)LoadedObjects.Num()) return nullptr;
  return LoadedObjects[Index];
}


//==========================================================================
//
//  VAcsLevel::StartTypedACScripts
//
//==========================================================================
void VAcsLevel::StartTypedACScripts (int Type, int Arg1, int Arg2, int Arg3,
                                     VEntity *Activator, bool Always, bool RunNow)
{
  for (int i = 0; i < LoadedObjects.Num(); ++i) {
    LoadedObjects[i]->StartTypedACScripts(Type, Arg1, Arg2, Arg3, Activator, Always, RunNow);
  }
}


//==========================================================================
//
//  VAcsLevel::Serialise
//
//==========================================================================
void VAcsLevel::Serialise (VStream &Strm) {
  vuint8 xver = 1;
  Strm << xver;
  if (xver != 1) Host_Error("invalid ACS level version in save file");

  //GCon->Logf("serializing ACS level");

  // objects
  int objCount = LoadedObjects.length();
  Strm << STRM_INDEX(objCount);
  if (Strm.IsLoading()) {
    if (objCount != LoadedObjects.length()) Host_Error("invalid number of ACS loaded objects in save file");
  }

  for (int i = 0; i < objCount; ++i) LoadedObjects[i]->Serialise(Strm);

  // dynamically added strings
  vint32 sllen = stringList.length();
  Strm << STRM_INDEX(sllen);

  if (Strm.IsLoading()) stringList.setLength(sllen);
  for (int f = 0; f < sllen; ++f) Strm << stringList[f];

  if (Strm.IsLoading()) {
    stringMapByStr.clear();
    for (int f = 0; f < sllen; ++f) stringMapByStr.put(stringList[f], f);
  }
}


//==========================================================================
//
//  VAcsLevel::AddToACSStore
//
//==========================================================================
bool VAcsLevel::AddToACSStore (int Type, VName Map, int Number, int Arg1,
                               int Arg2, int Arg3, int Arg4, VEntity *Activator)
{
  VAcsStore &S = XLevel->WorldInfo->Acs->Store.Alloc();
  S.Map = Map;
  S.Type = Type;
  S.PlayerNum = (Activator && Activator->Player ? SV_GetPlayerNum(Activator->Player) : -1);
  S.Script = Number;
  S.Args[0] = Arg1;
  S.Args[1] = Arg2;
  S.Args[2] = Arg3;
  S.Args[3] = Arg4;
  return true;
}


//==========================================================================
//
//  VAcsLevel::CheckAcsStore
//
//  Scans the ACS store and executes all scripts belonging to the current
//  map.
//
//==========================================================================
void VAcsLevel::CheckAcsStore () {
  for (int i = XLevel->WorldInfo->Acs->Store.length()-1; i >= 0; --i) {
    VAcsStore *store = &XLevel->WorldInfo->Acs->Store[i];
    if (store->Map != XLevel->MapName) continue;

    VAcsObject *Object;
    VAcsInfo *Info = FindScript(store->Script, Object);
    if (!Info) {
      // script not found
      GCon->Logf(NAME_Dev, "Start ACS ERROR: Unknown script %d", store->Script);
    } else {
      switch (store->Type) {
        case VAcsStore::Start:
        case VAcsStore::StartAlways:
          SpawnScript(Info, Object, store->PlayerNum >= 0 &&
            GGameInfo->Players[store->PlayerNum] &&
            (GGameInfo->Players[store->PlayerNum]->PlayerFlags &
            VBasePlayer::PF_Spawned) ?
            GGameInfo->Players[store->PlayerNum]->MO : nullptr, nullptr, 0,
            store->Args[0], store->Args[1], store->Args[2], store->Args[3],
            (store->Type == VAcsStore::StartAlways), true, false); // always register thinker
          break;

        case VAcsStore::Terminate:
          if (!Info->RunningScript || Info->RunningScript->State == VAcs::ASTE_Terminating) {
            // states that disallow termination
            break;
          }
          Info->RunningScript->State = VAcs::ASTE_Terminating;
          break;

        case VAcsStore::Suspend:
          if (!Info->RunningScript ||
              Info->RunningScript->State == VAcs::ASTE_Suspended ||
              Info->RunningScript->State == VAcs::ASTE_Terminating)
          {
            // states that disallow suspension
            break;
          }
          Info->RunningScript->State = VAcs::ASTE_Suspended;
          break;
        }
    }
    XLevel->WorldInfo->Acs->Store.RemoveIndex(i);
  }
}


//==========================================================================
//
//  VAcsLevel::GenScriptName
//
//==========================================================================
VStr VAcsLevel::GenScriptName (int Number) {
  if (Number < 0) {
    // named
    int nidx = -Number;
    if (nidx < 0 || nidx >= VName::GetNumNames()) return VStr(va("<badnameidx:%d>", Number));
    return VStr(va("<%s>", *VName::CreateWithIndex(nidx)));
  } else {
    return VStr(va("#%d", Number));
  }
}


//==========================================================================
//
//  VAcsLevel::Start
//
//==========================================================================
bool VAcsLevel::Start (int Number, int MapNum, int Arg1, int Arg2, int Arg3, int Arg4,
                       VEntity *Activator, line_t *Line, int Side, bool Always, bool WantResult,
                       bool Net, int *realres)
{
  /*
  if (WantResult) {
    if (realres) *realres = 1;
    return true;
  }
  */

  if (MapNum) {
    if (WantResult) Host_Error("ACS: tried to get result from map script");
    VName Map = P_GetMapLumpNameByLevelNum(MapNum);
    if (Map != NAME_None && Map != XLevel->MapName) {
      // add to the script store
      if (realres) *realres = 0;
      return AddToACSStore((Always ? VAcsStore::StartAlways : VAcsStore::Start), Map, Number, Arg1, Arg2, Arg3, Arg4, Activator);
    }
  }

  VAcsObject *Object;
  VAcsInfo *Info;
  if (Number >= 0) {
    Info = FindScript(Number, Object);
  } else {
    vassert(Number < 0); // lol
    Info = FindScriptByName(Number, Object);
    //if (Info) GCon->Logf("ACS: Start: script=<%d>; found '%s'", Number, *Info->Name);
    //else GCon->Logf("ACS: Start: script=<%d> -- OOPS", Number);
  }

  if (!Info) {
    // script not found
    if (!unknownScripts.put(Number, true)) {
      GCon->Logf(NAME_Error, "ACS: unknown script %s", *GenScriptName(Number));
      //{ VObject::VMDumpCallStack(); GCon->Logf(NAME_Debug, "ACS named script '%s': res=%d", *Name, res); }
    }
    if (realres) *realres = 0;
    return false;
  }
  #if 0
  else {
    GCon->Logf(NAME_Debug, "ACS: executing script %s (Always=%d; WantResult=%d)", *GenScriptName(Number), (int)Always, (int)WantResult);
  }
  #endif

  if (Net && (GGameInfo->NetMode >= NM_DedicatedServer) && !(Info->Flags&SCRIPTF_Net)) {
    GCon->Logf(NAME_Warning, "%s tried to puke script %s", *Activator->Player->PlayerName, *GenScriptName(Number));
    if (realres) *realres = 0;
    return false;
  }

  VAcs *script = SpawnScript(Info, Object, Activator, Line, Side, Arg1, Arg2, Arg3, Arg4, Always, false, WantResult);

  if (WantResult) {
    int res = script->RunScript(0/*host_frametime:doesn't matter*/, true);
    //GCon->Logf(NAME_Debug, "*** CallACS: %s; res=%d; dead=%d", *GenScriptName(Number), res, (int)script->destroyed);
    if (!script->destroyed && script->XLevel) {
      //GCon->Logf(NAME_Debug, "***   promoted to continuous script");
      script->XLevel->PromoteImmediateScriptThinker(script);
    } else {
      //GCon->Logf(NAME_Debug, "***   COMPLETE, DESTROYING");
      if (acs_show_stopped_scripts) GCon->Logf(NAME_Debug, "ACS: wantresult complete: %s", *Info->toString());
      script->Destroy();
      delete script;
    }
    if (realres) *realres = res;
    return !!res;
  }

  if (realres) *realres = 0;
  return true;
}


//==========================================================================
//
//  VAcsLevel::Terminate
//
//==========================================================================
bool VAcsLevel::Terminate (int Number, int MapNum) {
  if (MapNum) {
    VName Map = P_GetMapLumpNameByLevelNum(MapNum);
    if (Map != NAME_None && Map != XLevel->MapName) {
      // add to the script store
      return AddToACSStore(VAcsStore::Terminate, Map, Number, 0, 0, 0, 0, nullptr);
    }
  }

  VAcsObject *Object;
  VAcsInfo *Info = FindScript(Number, Object);
  if (!Info) {
    // script not found
    return false;
  }
  if (!Info->RunningScript || Info->RunningScript->State == VAcs::ASTE_Terminating) {
    // states that disallow termination
    return false;
  }
  Info->RunningScript->State = VAcs::ASTE_Terminating;
  return true;
}


//==========================================================================
//
//  VAcsLevel::Suspend
//
//==========================================================================
bool VAcsLevel::Suspend (int Number, int MapNum) {
  if (MapNum) {
    VName Map = P_GetMapLumpNameByLevelNum(MapNum);
    if (Map != NAME_None && Map != XLevel->MapName) {
      // add to the script store
      return AddToACSStore(VAcsStore::Suspend, Map, Number, 0, 0, 0, 0, nullptr);
    }
  }

  VAcsObject *Object;
  VAcsInfo *Info = FindScript(Number, Object);
  if (!Info) {
    // script not found
    return false;
  }
  if (!Info->RunningScript ||
      Info->RunningScript->State == VAcs::ASTE_Suspended ||
      Info->RunningScript->State == VAcs::ASTE_Terminating)
  {
    // states that disallow suspension
    return false;
  }
  Info->RunningScript->State = VAcs::ASTE_Suspended;
  return true;
}


//==========================================================================
//
//  VAcsLevel::SpawnScript
//
//==========================================================================
VAcs *VAcsLevel::SpawnScript (VAcsInfo *Info, VAcsObject *Object,
                              VEntity *Activator, line_t *Line, int Side,
                              int Arg1, int Arg2, int Arg3, int Arg4,
                              bool Always, bool Delayed, bool ImmediateRun)
{
  if (!Always && Info->RunningScript) {
    if (Info->RunningScript->State == VAcs::ASTE_Suspended) {
      // resume a suspended script
      Info->RunningScript->State = VAcs::ASTE_Running;
    }
    //k8: force it
    Info->RunningScript->Level = XLevel->LevelInfo;
    Info->RunningScript->XLevel = XLevel;
    // script is already executing
    if (acs_show_started_scripts) GCon->Logf(NAME_Debug, "ACS: resumed: %s; activator:%s", *Info->toString(), (Activator ? Activator->GetClass()->GetName() : "<none>"));
    return Info->RunningScript;
  }

  //VAcs *script = (VAcs *)XLevel->SpawnThinker(VAcs::StaticClass());
  VAcs *script = new VAcs();
  // `XLevel->AddScriptThinker()` will do this
  //script->Level = XLevel->LevelInfo;
  //script->XLevel = XLevel;
  script->info = Info;
  script->number = Info->Number;
  script->InstructionPointer = Info->Address;
  script->State = VAcs::ASTE_Running;
  script->ActiveObject = Object;
  script->Activator = Activator;
  script->line = Line;
  script->side = Side;
  script->AllocateLocals(Info->VarCount);
  /*
  if (Info->VarCount > VAcs::MAX_LOCAL_VARS) {
    VStr sdn = script->DebugDumpToString();
    script->Destroy();
    delete script;
    Host_Error("ACS script %s has too many locals (%d)", *sdn, Info->VarCount);
  }
  */
  //script->LocalVars = new vint32[Info->VarCount];
  //script->Font = VName("smallfont");
  if (Info->VarCount > 0) script->LocalVars[0] = Arg1;
  if (Info->VarCount > 1) script->LocalVars[1] = Arg2;
  if (Info->VarCount > 2) script->LocalVars[2] = Arg3;
  if (Info->VarCount > 3) script->LocalVars[3] = Arg4;
  if (Info->VarCount > Info->ArgCount) memset((void *)(script->LocalVars+Info->ArgCount), 0, (Info->VarCount-Info->ArgCount)*4);
  script->DelayTime = 0;
  script->DelayActivationTick = 0;
  if (Delayed) {
    //k8: this was commented in the original
    // world objects are allotted 1 second for initialization
    //script->DelayTime = 1.0f;
    // as scripts are runned last in the frame, there is no need to delay them
    // but leave delay code for "tic granularity"
    script->DelayActivationTick = XLevel->TicTime+1; // on next tick
  }
  if (!Always) {
    Info->RunningScript = script;
  }
  XLevel->AddScriptThinker(script, ImmediateRun);
  if (acs_show_started_scripts) GCon->Logf(NAME_Debug, "ACS: started: %s; activator:%s", *Info->toString(), (Activator ? Activator->GetClass()->GetName() : "<none>"));
  return script;
}


//==========================================================================
//
//  AcsLoadScriptFromStream
//
//==========================================================================
/*
VLevelScriptThinker *AcsLoadScriptFromStream (VLevel *XLevel, VStream &strm) {
  vassert(strm.IsLoading());
  VAcs *script = new VAcs();
  //script->Level = XLevel->LevelInfo;
  //script->XLevel = XLevel;
  script->Serialise(strm);
  return script;
}
*/


//==========================================================================
//
//  AcsCreateEmptyThinker
//
//==========================================================================
VLevelScriptThinker *AcsCreateEmptyThinker () {
  return new VAcs();
}


//==========================================================================
//
//  AcsSuspendScript
//
//==========================================================================
void AcsSuspendScript (VAcsLevel *acslevel, int number, int map) {
  if (acslevel) acslevel->Suspend(number, map);
}


//==========================================================================
//
//  AcsTerminateScript
//
//==========================================================================
void AcsTerminateScript (VAcsLevel *acslevel, int number, int map) {
  if (acslevel) acslevel->Terminate(number, map);
}


//==========================================================================
//
//  AcsHasScripts
//
//==========================================================================
bool AcsHasScripts (VAcsLevel *acslevel) {
  if (!acslevel) return false;
  for (int i = 0; i < acslevel->LoadedObjects.length(); ++i) {
    if (acslevel->LoadedObjects[i]->GetNumScripts() > 0) return true;
  }
  return false;
}


//==========================================================================
//
//  VAcs::Destroy
//
//==========================================================================
void VAcs::Destroy () {
  if (!destroyed) {
    destroyed = true;
    Level = nullptr;
    XLevel = nullptr;
    FreeLocals();
    ResetSaveds();
  }
}


//==========================================================================
//
//  VAcs::Serialise
//
//==========================================================================
void VAcs::Serialise (VStream &Strm) {
  vint32 TmpInt;

  //Super::Serialise(Strm);
  vuint8 xver = 2;
  Strm << xver;
  if (xver != 2) Host_Error("invalid ACS script version in save file (%d)", xver);

  if (Strm.IsLoading()) {
    vuint8 isDead;
    Strm << isDead;
    if (isDead) {
      //GCon->Logf("serializing (load) destroyed VAcs");
      if (!destroyed) Destroy();
      return;
    }
    vassert(!destroyed);
  } else {
    vuint8 isDead = (destroyed ? 1 : 0);
    Strm << isDead;
    if (destroyed) {
      //GCon->Logf("serializing (save) destroyed VAcs");
      return;
    }
  }

  //GCon->Logf("*** serializing VAcs");

  Strm << Level;
  Strm << XLevel;

  vassert(Level);
  vassert(XLevel);

  Strm << Activator;

  //GCon->Logf("  VAcs: Level=%p; XLevel=%p; Activator=%p", (void *)Level, (void *)XLevel, (void *)Activator);

  if (Strm.IsLoading()) {
    Strm << STRM_INDEX(TmpInt);
    if (TmpInt < -1 || TmpInt >= XLevel->NumLines) Host_Error("invalid ACS linedef index in save file");
    line = (TmpInt == -1 ? nullptr : &XLevel->Lines[TmpInt]);
  } else {
    TmpInt = (line ? (int)(ptrdiff_t)(line-XLevel->Lines) : -1);
    Strm << STRM_INDEX(TmpInt);
  }

  Strm << side
    << number
    << State
    << DelayTime
    << STRM_INDEX(DelayActivationTick)
    << STRM_INDEX(WaitValue);

  if (Strm.IsLoading()) {
    // load
    Strm << STRM_INDEX(TmpInt);
    ActiveObject = XLevel->Acs->GetObject(TmpInt);
    Strm << STRM_INDEX(TmpInt);
    InstructionPointer = ActiveObject->OffsetToPtr(TmpInt);
    info = ActiveObject->FindScript(number);
    //LocalVars = new vint32[info->VarCount];
    //FIXME: memleak!
    /*
    if (info->VarCount > VAcs::MAX_LOCAL_VARS) {
      VStr sdn = DebugDumpToString();
      Destroy();
      Host_Error("ACS script %s has too many locals (%d)", *sdn, info->VarCount);
    }
    */
    ResetSaveds();
  } else {
    // save
    TmpInt = ActiveObject->GetLibraryID()>>16;
    Strm << STRM_INDEX(TmpInt);
    TmpInt = ActiveObject->PtrToOffset(InstructionPointer);
    Strm << STRM_INDEX(TmpInt);
  }

  // local vars
  int varCount = info->VarCount;
  Strm << STRM_INDEX(varCount);
  if (Strm.IsLoading()) {
    if (varCount > info->VarCount) Host_Error("invalid number of ACS script locals in save file");
    FreeLocals();
    AllocateLocals(varCount);
  }
  //GCon->Logf(NAME_Debug, "varCount=%d; LocalVarsCount=%d", varCount, LocalVarsCount);
  vassert(varCount <= LocalVarsCount);
  for (int i = 0; i < /*info->VarCount*/varCount; ++i) Strm << LocalVars[i];

  Strm << HudWidth
    << HudHeight
    << Font;
}


//==========================================================================
//
//  VAcs::ClearReferences
//
//==========================================================================
void VAcs::ClearReferences () {
  //Super::ClearReferences();
  if (!destroyed) {
    if (Activator && Activator->IsRefToCleanup()) Activator = nullptr;
    if (XLevel && XLevel->IsRefToCleanup()) XLevel = nullptr;
    if (Level && Level->IsRefToCleanup()) Level = nullptr;
  }
}


//==========================================================================
//
//  VAcs::Tick
//
//==========================================================================
void VAcs::Tick (float DeltaTime) {
  if (DeltaTime <= 0.0f) return;
  if (!destroyed) RunScript(DeltaTime, false);
}


//==========================================================================
//
//  VAcs::GetName
//
//==========================================================================
VName VAcs::GetName () {
  return (info ? info->Name : NAME_None);
}


//==========================================================================
//
//  VAcs::GetNumber
//
//==========================================================================
int VAcs::GetNumber () {
  return (info ? info->Number : -1);
}


//==========================================================================
//
//  doSetUserVarOrArray
//
//==========================================================================
static bool doSetUserVarOrArray (VEntity *ent, VName fldname, int value, bool isArray, int index=0) {
  if (!ent || fldname == NAME_None) return false;
  auto vtype = ent->_get_user_var_type(fldname);
  if (vtype == VGameObject::UserVarFieldType::Int || vtype == VGameObject::UserVarFieldType::IntArray) {
    if (acs_dump_uservar_access) {
      if (isArray) {
        GCon->Logf("ACS: setting `%s` int userarray[%d] '%s' to %d", *ent->GetClass()->GetFullName(), index, *fldname, value);
      } else {
        GCon->Logf("ACS: setting `%s` int uservar '%s' to %d", *ent->GetClass()->GetFullName(), *fldname, value);
      }
    }
    ent->_set_user_var_int(fldname, value, index);
    return true;
  }
  if (vtype == VGameObject::UserVarFieldType::Float || vtype == VGameObject::UserVarFieldType::FloatArray) {
    if (acs_dump_uservar_access) {
      if (isArray) {
        GCon->Logf("ACS: setting `%s` float userarray[%d] '%s' to %f", *ent->GetClass()->GetFullName(), index, *fldname, float(value)/65536.0f);
      } else {
        GCon->Logf("ACS: setting `%s` float uservar '%s' to %f", *ent->GetClass()->GetFullName(), *fldname, float(value)/65536.0f);
      }
    }
    ent->_set_user_var_float(fldname, float(value)/65536.0f, index);
    return true;
  }
  if (acs_dump_uservar_access) GCon->Logf("ACS: missing `%s` uservar '%s'", *ent->GetClass()->GetFullName(), *fldname);
  return false;
}


//==========================================================================
//
//  doGetUserVarOrArray
//
//==========================================================================
static int doGetUserVarOrArray (VEntity *ent, VName fldname, bool isArray, int index=0) {
  if (!ent) {
    GCon->Logf("ACS: cannot get user variable with name \"%s\" from nothing", *fldname);
    return 0;
  }
  auto vtype = ent->_get_user_var_type(fldname);
  if (vtype == VGameObject::UserVarFieldType::Int || vtype == VGameObject::UserVarFieldType::IntArray) {
    if (acs_dump_uservar_access) {
      if (isArray) {
        GCon->Logf("ACS: getting `%s` int userarray[%d] '%s' (%d)", *ent->GetClass()->GetFullName(), index, *fldname, ent->_get_user_var_int(fldname, index));
      } else {
        GCon->Logf("ACS: getting `%s` int uservar '%s' (%d)", *ent->GetClass()->GetFullName(), *fldname, ent->_get_user_var_int(fldname));
      }
    }
    return ent->_get_user_var_int(fldname);
  }
  if (vtype == VGameObject::UserVarFieldType::Float || vtype == VGameObject::UserVarFieldType::FloatArray) {
    if (acs_dump_uservar_access) {
      if (isArray) {
        GCon->Logf("ACS: getting `%s` float userarray[%d] '%s' (%f)", *ent->GetClass()->GetFullName(), index, *fldname, ent->_get_user_var_float(fldname, index));
      } else {
        GCon->Logf("ACS: getting `%s` float uservar '%s' (%f)", *ent->GetClass()->GetFullName(), *fldname, ent->_get_user_var_float(fldname));
      }
    }
    return (int)(ent->_get_user_var_float(fldname)*65536.0f);
  }
  GCon->Logf("ACS: missing `%s` uservar '%s'", *ent->GetClass()->GetFullName(), *fldname);
  return 0;
}



//==========================================================================
//
//  RunScript
//
//==========================================================================

#define STUB(cmd) GCon->Log(NAME_Dev, "Executing unimplemented ACS PCODE " #cmd);

#ifdef ACS_DUMP_EXECUTION
# define USE_COMPUTED_GOTO 0
#else
# define USE_COMPUTED_GOTO 1
#endif

//#undef USE_COMPUTED_GOTO
//#define USE_COMPUTED_GOTO 0

#if USE_COMPUTED_GOTO
#define ACSVM_SWITCH(op)  goto *vm_labels[op];
#define ACSVM_CASE(x)   Lbl_ ## x:
#define ACSVM_BREAK \
  if (--scountLeft == 0) { \
    double currtime = Sys_Time(); \
    if (currtime-sttime > 3.0f) { \
      Host_Error("ACS script #%d (%s) took too long to execute", number, *info->Name); \
    } \
    scountLeft = ACS_GUARD_INSTRUCTION_COUNT; \
  } \
  /* check stack */ \
  if ((uintptr_t)sp < (uintptr_t)mystack) Host_Error("ACS: stack underflow"); \
  if ((ptrdiff_t)(sp-mystack) >= ACS_STACK_DEPTH) Host_Error("ACS: stack overflow"); \
  if (fmt == ACS_LittleEnhanced) { \
    cmd = *ip; \
    if (cmd >= 240) { \
      cmd = 240+((cmd-240)<<8)+ip[1]; \
      ip += 2; \
    } else { \
      ++ip; \
    } \
  } else { \
    cmd = READ_INT32(ip); \
    ip += 4; \
  } \
  if ((vuint32)cmd >= PCODE_COMMAND_COUNT) { \
    goto LblDefault; \
  } \
  goto *vm_labels[cmd]

#define ACSVM_BREAK_STOP  goto LblFuncStop;
#define ACSVM_DEFAULT   LblDefault:
#else
#define ACSVM_SWITCH(op)  switch (cmd)
#define ACSVM_CASE(op)    case op:
#define ACSVM_BREAK \
  if (--scountLeft == 0) { \
    double currtime = Sys_Time(); \
    if (currtime-sttime > 3.0f) { \
      Host_Error("ACS script #%d (%s) took too long to execute", number, *info->Name); \
    } \
    scountLeft = ACS_GUARD_INSTRUCTION_COUNT; \
  } \
  break
#define ACSVM_BREAK_STOP  break
#define ACSVM_DEFAULT   default:
#endif

#define READ_INT16(p)   (vint32)(vint16)((p)[0]|((p)[1]<<8))
#define READ_INT32(p)   ((p)[0]|((p)[1]<<8)|((p)[2]<<16)|((p)[3]<<24))
#define READ_BYTE_OR_INT32  (fmt == ACS_LittleEnhanced ? *ip : READ_INT32(ip))
#define INC_BYTE_OR_INT32 if (fmt == ACS_LittleEnhanced) ++ip; else ip += 4
#define READ_SHORT_OR_INT32  (fmt == ACS_LittleEnhanced ? READ_INT16(ip) : READ_INT32(ip))
#define INC_SHORT_OR_INT32 if (fmt == ACS_LittleEnhanced) ip += 2; else ip += 4

// extfunction enum
#define ACS_EXTFUNC(fnname)             ACSF_##fnname,
#define ACS_EXTFUNC_NUM(fnname, fnidx)  ACSF_##fnname=fnidx,
enum {
#include "p_acs_extfunc.h"
};
#undef ACS_EXTFUNC_NUM
#undef ACS_EXTFUNC

// extfunction names
#define ACS_EXTFUNC(fnname)             { "" #fnname "", ACSF_##fnname },
#define ACS_EXTFUNC_NUM(fnname, fnidx)  { "" #fnname "", ACSF_##fnname },
struct ACSF_Info {
  const char *name;
  int index;
};

static const ACSF_Info ACSF_List[] = {
#include "p_acs_extfunc.h"
  { nullptr, -666 }
};
#undef ACS_EXTFUNC_NUM
#undef ACS_EXTFUNC


// pcd opcode names
#define DECLARE_PCD(name) { "" #name "", PCD_ ## name }
struct PCD_Info {
  const char *name;
  int index;
};

VVA_OKUNUSED static const PCD_Info PCD_List[] = {
#include "p_acs.h"
  { nullptr, -666 }
};
#undef DECLARE_PCD


//==========================================================================
//
//  NormHudTime
//
//==========================================================================
static inline float NormHudTime (float t, float DelayTime) {
  if (t <= 0.0f) return 0.0f;
  int tics = (int)(t*35.0f+0.5f);
  if (tics < 1) return 1.0f/35.0f+(0.9f/35.0f);
  return tics/35.0f+(DelayTime > 0.0f ? DelayTime : 0.0f)+(0.9f/35.0f);
  //return t;
}


//==========================================================================
//
//  VAcs::CallFunction
//
//==========================================================================
int VAcs::CallFunction (int argCount, int funcIndex, vint32 *args) {
#if !USE_COMPUTED_GOTO
  {
    VStr dstr;
    for (const ACSF_Info *nfo = ACSF_List; nfo->name; ++nfo) {
      if (nfo->index == funcIndex) {
        dstr = va("[%d] %s", funcIndex, nfo->name);
        break;
      }
    }
    if (!dstr.length()) dstr = va("OPC[%d]", funcIndex);
    dstr += va("; argCount=%d", argCount);
    for (int f = 0; f < argCount; ++f) dstr += va("; <#%d>:%d", f, args[f]);
    GCon->Logf("ACS:  ACSF: %s", *dstr);
  }
#endif

  switch (funcIndex) {
    // int SpawnSpotForced (str classname, int spottid, int tid, int angle)
    case ACSF_SpawnSpotForced:
      //GCon->Logf("ACS: SpawnSpotForced: classname=<%s>; spottid=%d; tid=%d; angle=%f", *GetNameLowerCase(args[0]), args[1], args[2], float(args[3])*45.0f/32.0f);
      if (argCount >= 4) {
        return Level->eventAcsSpawnSpot(GetNameLowerCase(args[0]), args[1], args[2], float(args[3])*45.0f/32.0f, true); // forced
      }
      return 0;

    // int SpawnSpotFacingForced (str classname, int spottid, int tid)
    case ACSF_SpawnSpotFacingForced:
      if (argCount >= 3) {
        return Level->eventAcsSpawnSpotFacing(GetNameLowerCase(args[0]), args[1], args[2], true);
      }
      return false;

    // int SpawnForced (str classname, fixed x, fixed y, fixed z [, int tid [, int angle]])
    case ACSF_SpawnForced:
      if (argCount >= 4) {
        //GCon->Logf(NAME_Debug, "ACSF_SpawnForced: name=%s; pos=(%g,%g,%g); tid=%d; angle=%g", *GetNameLowerCase(args[0]), float(args[1])/65536.0f, float(args[2])/65536.0f, float(args[3])/65536.0f, (argCount >= 4 ? args[4] : 0), (argCount >= 5 ? float(args[5])*45.0f/32.0f : 0));
        return Level->eventAcsSpawnThing(GetNameLowerCase(args[0]),
                        TVec(float(args[1])/65536.0f, float(args[2])/65536.0f, float(args[3])/65536.0f), // x, y, z
                        (argCount >= 4 ? args[4] : 0), // tid
                        (argCount >= 5 ? float(args[5])*45.0f/32.0f : 0), // angle
                        true); // forced
      }
      return 0;


    case ACSF_CheckActorClass:
      if (argCount >= 2) {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        VStr name = GetStr(args[1]);
        if (name.length() == 0) return 0;
        return (Ent ? (name.ICmp(*Ent->GetClass()->Name) == 0 ? 1 : 0) : 0);
      }
      return 0;

    case ACSF_GetActorClass:
      if (argCount > 0) {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (Ent) return ActiveObject->Level->PutNewString(*Ent->GetClass()->Name);
      }
      return ActiveObject->Level->PutNewString("");

    case ACSF_GetWeapon:
      if (Activator && Activator->IsPlayer() && Activator->Player) {
        VEntity *wpn = Activator->Player->eventGetReadyWeapon();
        if (wpn) return ActiveObject->Level->PutNewString(*wpn->GetClass()->Name);
      }
      return ActiveObject->Level->PutNewString("");

    // int GetArmorType (string armortype, int playernum)
    case ACSF_GetArmorType:
      if (argCount >= 2) {
        int pidx = args[1];
        VBasePlayer *plr = SV_GetPlayerByNum(pidx);
        if (!plr || !plr->MO) return 0;
        if (!(plr->PlayerFlags&VBasePlayer::PF_Spawned)) return 0;
        if (plr->MO->Health <= 0) return 0; // it is dead
        VName atype = GetNameLowerCase(args[0]);
        if (atype == NAME_None || atype == "none") return 0;
        return plr->MO->eventGetArmorPointsForType(atype);
      }
      return 0;

    // str GetArmorInfo (int infotype)
    // int GetArmorInfo (int infotype)
    // fixed GetArmorInfo (int infotype)
    case ACSF_GetArmorInfo:
      if (argCount > 0) {
        if (!Activator || !Activator->IsPlayer() || !Activator->Player) {
          // what to do here?
          if (args[0] == ARMORINFO_CLASSNAME) return ActiveObject->Level->PutNewString("None");
          return 0;
        }
        VBasePlayer *plr = Activator->Player;
        switch (args[0]) {
          case ARMORINFO_CLASSNAME: return ActiveObject->Level->PutNewString(*plr->GetCurrentArmorClassName());
          case ARMORINFO_SAVEAMOUNT: return plr->GetCurrentArmorSaveAmount();
          case ARMORINFO_SAVEPERCENT: return (int)(plr->GetCurrentArmorSavePercent()*65536.0f);
          case ARMORINFO_MAXABSORB: return plr->GetCurrentArmorMaxAbsorb();
          case ARMORINFO_MAXFULLABSORB: return plr->GetCurrentArmorFullAbsorb();
          case ARMORINFO_ACTUALSAVEAMOUNT: return plr->GetCurrentArmorActualSaveAmount();
          default: GCon->Logf(NAME_Warning, "ACS: GetArmorInfo got invalid requiest #%d", args[0]); break;
        }
      } else {
        Host_Error("ACS: GetArmorInfo requires argument");
      }
      return 0;

    // fixed GetActorViewHeight (int tid)
    case ACSF_GetActorViewHeight:
      if (argCount >= 1) {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (Ent) return (int)(Ent->GetViewHeight()*65536.0f);
      }
      return 0;


    // bool ACS_NamedTerminate (string script, int map) -- always returns `true`
    case ACSF_ACS_NamedTerminate:
      if (argCount >= 2) {
        VStr scname = VStr(GetNameLowerCase(args[0]));
        XLevel->TerminateNamedScriptThinkers(scname, args[1]);
      }
      return 1;

    // bool ACS_NamedSuspend (string script, int map) -- always returns `true`
    case ACSF_ACS_NamedSuspend:
      if (argCount >= 2) {
        VStr scname = VStr(GetNameLowerCase(args[0]));
        XLevel->SuspendNamedScriptThinkers(scname, args[1]);
      }
      return 1;

    // bool ACS_NamedExecute (string script, int map, int s_arg1, int s_arg2, int s_arg3)
    case ACSF_ACS_NamedExecute:
      if (argCount > 0) {
        //VAcsObject *ao = nullptr;
        VName name = GetNameLowerCase(args[0]);
        if (name == NAME_None) return 0;
        int ScArgs[4];
        ScArgs[0] = (argCount > 2 ? args[2] : 0);
        ScArgs[1] = (argCount > 3 ? args[3] : 0);
        ScArgs[2] = (argCount > 4 ? args[4] : 0);
        ScArgs[3] = (argCount > 5 ? args[5] : 0);
        if (!ActiveObject->Level->Start(-name.GetIndex(), (argCount > 1 ? args[1] : 0), ScArgs[0], ScArgs[1], ScArgs[2], ScArgs[3], Activator, line, side, false/*always*/, false/*wantresult*/, false/*net*/)) return 0;
        return 1;
      }
      return 0;

    // int ACS_NamedExecuteWithResult (string script, int s_arg1, int s_arg2, int s_arg3, int s_arg4)
    case ACSF_ACS_NamedExecuteWithResult:
      if (argCount > 0) {
        //VAcsObject *ao = nullptr;
        VName name = GetNameLowerCase(args[0]);
        /*
        if (argCount > 4) {
          if (args[4] != 0) Host_Error("ACS_NamedExecuteWithResult(%s): s_arg4=%d", *name, args[4]);
        }
        */
        if (name == NAME_None) return 0;
        int ScArgs[4];
        ScArgs[0] = (argCount > 1 ? args[1] : 0);
        ScArgs[1] = (argCount > 2 ? args[2] : 0);
        ScArgs[2] = (argCount > 3 ? args[3] : 0);
        ScArgs[3] = (argCount > 4 ? args[4] : 0);
        int res = 0;
        ActiveObject->Level->Start(-name.GetIndex(), 0, ScArgs[0], ScArgs[1], ScArgs[2], ScArgs[3], Activator, line, side, true/*always*/, true/*wantresult*/, false/*net*/, &res);
        return res;
      }
      return 0;

    // bool ACS_NamedExecuteAlways (string script, int map, int s_arg1, int s_arg2, int s_arg3)
    case ACSF_ACS_NamedExecuteAlways:
      if (argCount > 0) {
        //VAcsObject *ao = nullptr;
        VName name = GetNameLowerCase(args[0]);
        if (name == NAME_None) return 0;
        int ScArgs[4];
        ScArgs[0] = (argCount > 2 ? args[2] : 0);
        ScArgs[1] = (argCount > 3 ? args[3] : 0);
        ScArgs[2] = (argCount > 4 ? args[4] : 0);
        ScArgs[3] = (argCount > 5 ? args[5] : 0);
        //GCon->Logf(NAME_Debug, "ACSF_ACS_NamedExecuteAlways: script=<%s>; map=%d; args=(%d,%d,%d,%d)", *name, (argCount > 1 ? args[1] : 0), ScArgs[0], ScArgs[1], ScArgs[2], ScArgs[3]);
        if (!ActiveObject->Level->Start(-name.GetIndex(), (argCount > 1 ? args[1] : 0), ScArgs[0], ScArgs[1], ScArgs[2], ScArgs[3], Activator, line, side, true/*always*/, false/*wantresult*/, false/*net*/)) {
          //GCon->Logf(NAME_Debug, "   FAILED!");
          return 0;
        }
        //GCon->Logf(NAME_Debug, "   OK!");
        return 1;
      }
      return 0;


    case ACSF_ACS_NamedLockedExecute:
      if (argCount >= 5) {
        VName name = GetNameLowerCase(args[0]);
        if (name == NAME_None) return 0;
        if (!Level->eventCheckLock(Activator, args[4], false)) return 0;
        int ScArgs[4];
        ScArgs[0] = args[2];
        ScArgs[1] = args[3];
        ScArgs[2] = 0;
        ScArgs[3] = 0;
        if (!ActiveObject->Level->Start(-name.GetIndex(), (argCount > 1 ? args[1] : 0), ScArgs[0], ScArgs[1], ScArgs[2], ScArgs[3], Activator, line, side, false/*always*/, false/*wantresult*/, false/*net*/)) return 0;
        return 1;
      }
      return 0;

    case ACSF_ACS_NamedLockedExecuteDoor:
      if (argCount >= 5) {
        VName name = GetNameLowerCase(args[0]);
        if (name == NAME_None) return 0;
        if (!Level->eventCheckLock(Activator, args[4], true)) return 0;
        int ScArgs[4];
        ScArgs[0] = args[2];
        ScArgs[1] = args[3];
        ScArgs[2] = 0;
        ScArgs[3] = 0;
        if (!ActiveObject->Level->Start(-name.GetIndex(), (argCount > 1 ? args[1] : 0), ScArgs[0], ScArgs[1], ScArgs[2], ScArgs[3], Activator, line, side, false/*always*/, false/*wantresult*/, false/*net*/)) return 0;
        return 1;
      }
      return 0;


    // bool CheckFlag (int tid, str flag)
    case ACSF_CheckFlag:
      {
        VName name = GetNameLowerCase(args[1]);
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (!Ent) return 0;
        //sp[-1] = vint32(Ent->Origin.x * 0x10000);
        //return (Ent->eventCheckFlag(*name) ? 1 : 0);
        return Ent->GetDecorateFlag(*name);
      }

    // int SetActorFlag (int tid, str flagname, bool value);
    case ACSF_SetActorFlag:
      {
        VStr name = VStr(GetNameLowerCase(args[1]));
        int count = 0;
        if (name.length()) {
          if (args[0] == 0) {
            // activator
            if (Activator && Activator->SetDecorateFlag(name, !!args[2])) ++count;
          } else {
            for (VEntity *mobj = Level->FindMobjFromTID(args[0], nullptr); mobj; mobj = Level->FindMobjFromTID(args[0], mobj)) {
              //mobj->StartSound(sound, 0, sp[-1] / 127.0f, 1.0f, false);
              //if (mobj->eventSetFlag(name, !!args[2])) ++count;
              if (mobj->SetDecorateFlag(name, !!args[2])) ++count;
            }
          }
        }
        return count;
      }

    case ACSF_PlayActorSound:
      GCon->Logf(NAME_Error, "unimplemented ACSF function 'PlayActorSound'");
      return 0;

    // void PlaySound (int tid, str sound [, int channel [, fixed volume [, bool looping [, fixed attenuation [, bool local]]]]])
    case ACSF_PlaySound:
      {
        //GCon->Logf("ERROR: unimplemented ACSF function 'PlaySound'");
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (Ent) {
          VName name = GetName(args[1]);
          if (name != NAME_None) {
            int chan = (argCount > 2 ? args[2] : 4)&7;
            float volume = (argCount > 3 ? (double)args[3]/(double)0x10000 : 1.0f);
            if (volume <= 0) return 0;
            if (volume > 1) volume = 1;
            bool looping = (argCount > 4 ? !!args[4] : false);
            float attenuation = (argCount > 5 ? (double)args[5]/(double)0x10000 : 1.0f);
            bool local = (argCount > 6 ? !!args[6] : false);
            if (local) attenuation = 0;
            Ent->StartSound(name, chan, volume, attenuation, looping, local);
          }
        }
        return 0;
      }

    // void StopSound (int tid, int channel);
    case ACSF_StopSound:
      {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (Ent) {
          int chan = (argCount > 1 ? args[1] : 4)&7;
          Ent->StopSound(chan);
        }
        //GCon->Logf("ERROR: unimplemented ACSF function 'StopSound'");
        return 0;
      }

    case ACSF_SoundVolume:
      //GCon->Logf("ERROR: unimplemented ACSF function 'SoundVolume'");
      return 0;

    case ACSF_SetMusicVolume:
      //GCon->Logf("ERROR: unimplemented ACSF function 'SetMusicVolume'");
      return 0;

    case ACSF_GetActorVelX:
      {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (!Ent) return 0;
        return (int)(Ent->Velocity.x*(65536.0f/35.0f));
      }
    case ACSF_GetActorVelY:
      {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (!Ent) return 0;
        return (int)(Ent->Velocity.y*(65536.0f/35.0f));
      }
    case ACSF_GetActorVelZ:
      {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (!Ent) return 0;
        return (int)(Ent->Velocity.z*(65536.0f/35.0f));
      }

    //bool SetActorVelocity (int tid, fixed velx, fixed vely, fixed velz, bool add, bool setbob)
    //TODO: bob
    case ACSF_SetActorVelocity:
      //k8: all or one?
      {
        int count = 0;
        if (args[0] == 0) {
          // activator
          if (Activator) {
            ++count;
            if (args[4]) {
              // add
              Activator->Velocity.x += float(args[1])/65536.0f*35.0f;
              Activator->Velocity.y += float(args[2])/65536.0f*35.0f;
              Activator->Velocity.z += float(args[3])/65536.0f*35.0f;
            } else {
              Activator->Velocity.x = float(args[1])/65536.0f*35.0f;
              Activator->Velocity.y = float(args[2])/65536.0f*35.0f;
              Activator->Velocity.z = float(args[3])/65536.0f*35.0f;
            }
          }
        } else {
          for (VEntity *mobj = Level->FindMobjFromTID(args[0], nullptr); mobj; mobj = Level->FindMobjFromTID(args[0], mobj)) {
            ++count;
            if (args[4]) {
              // add
              mobj->Velocity.x += float(args[1])/65536.0f*35.0f;
              mobj->Velocity.y += float(args[2])/65536.0f*35.0f;
              mobj->Velocity.z += float(args[3])/65536.0f*35.0f;
            } else {
              mobj->Velocity.x = float(args[1])/65536.0f*35.0f;
              mobj->Velocity.y = float(args[2])/65536.0f*35.0f;
              mobj->Velocity.z = float(args[3])/65536.0f*35.0f;
            }
          }
        }
        return (count > 0 ? 1 : 0);
      }

    // int UniqueTID ([int tid[, int limit]])
    case ACSF_UniqueTID:
      {
        int tidstart = (argCount > 0 ? args[0] : 0);
        int limit = (argCount > 1 ? args[1] : 0);
        int res = Level->FindFreeTID(tidstart, limit);
        //GCon->Logf("UniqueTID: tidstart=%d; limit=%d; res=%d", tidstart, limit, res);
        return res;
      }

    // bool IsTIDUsed (int tid)
    case ACSF_IsTIDUsed:
      return (argCount > 0 && args[0] ? Level->IsTIDUsed(args[0]) : 1);

    case ACSF_GetActorPowerupTics:
      if (argCount > 0) {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (Ent) {
          VName name = GetName(args[1]);
          float ptime = Ent->eventFindActivePowerupTime(name);
          if (ptime == 0) return 0;
          return int(ptime/35.0f);
        }
        return 0;
      } else {
        return 0;
      }

    // void SetTranslation (int tid, str transname)
    case ACSF_SetTranslation:
      if (argCount >= 2) {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (Ent) {
          VStr name = GetStr(args[1]);
          Ent->Translation = R_FindTranslationByName(name);
        }
      }
      return 0;

    case ACSF_ChangeActorAngle:
    case ACSF_ChangeActorPitch:
    case ACSF_ChangeActorRoll:
    case ACSF_SetActorRoll:
      // ignores interpolation for now (args[2])
      if (argCount >= 2) {
        int count = 0;
        float newAngle = (float)(args[1]&0xffff)*360.0f/(float)0x10000;
        newAngle = (funcIndex == ACSF_ChangeActorPitch ? AngleMod180(newAngle) : AngleMod(newAngle));
        if (args[0] == 0) {
          VEntity *ent = EntityFromTID(args[0], Activator);
          if (ent) {
            switch (funcIndex) {
              case ACSF_ChangeActorAngle: ent->Angles.yaw = newAngle; break;
              case ACSF_ChangeActorPitch: ent->Angles.pitch = newAngle; break;
              case ACSF_ChangeActorRoll: case ACSF_SetActorRoll: ent->Angles.roll = newAngle; break;
            }
            ++count;
          }
        } else {
          for (VEntity *ent = Level->FindMobjFromTID(args[0], nullptr); ent; ent = Level->FindMobjFromTID(args[0], ent)) {
            if (ent) {
              switch (funcIndex) {
                case ACSF_ChangeActorAngle: ent->Angles.yaw = newAngle; break;
                case ACSF_ChangeActorPitch: ent->Angles.pitch = newAngle; break;
                case ACSF_ChangeActorRoll: case ACSF_SetActorRoll: ent->Angles.roll = newAngle; break;
              }
            }
          }
        }
        return count;
      }
      return 0;

    case ACSF_GetActorRoll:
      if (argCount >= 1) {
        VEntity *ent = EntityFromTID(args[0], Activator);
        if (ent) return (int)(AngleMod(ent->Angles.roll)*65536.0f/360.0f);
      }
      return 0;

    case ACSF_SetActivator:
      //GCon->Logf("ACSF_SetActivator: argc=%d; arg[0]=%d; arg[1]=%d", argCount, (argCount > 0 ? args[0] : 0), (argCount > 1 ? args[1] : 0));
      if (argCount == 0) { Activator = nullptr; return 0; } // to world
      // only tid was specified
      if (argCount == 1) {
        if (args[0] == 0) return (Activator ? 1 : 0); // to self
        Activator = EntityFromTID(args[0], Activator);
        //GCon->Logf("   new activator: <%s>", (Activator ? *Activator->GetClass()->GetFullName() : "none"));
        return (Activator ? 1 : 0);
      }
      // some flags was specified
      Activator = EntityFromTID(args[0], Activator);
      if (Activator) {
        Activator = Activator->eventDoAAPtr(args[1]);
      } else {
        if (args[1]&AAPTR_ANY_PLAYER) {
          for (int f = 0; f < 8; ++f) {
            if (!(args[1]&(AAPTR_PLAYER1<<f))) continue;
            if (!GGameInfo->Players[f]) continue;
            if (!(GGameInfo->Players[f]->PlayerFlags&VBasePlayer::PF_Spawned)) continue;
            if (!GGameInfo->Players[f]->MO) continue; // just in case
            Activator = GGameInfo->Players[f]->MO;
            break;
          }
        }
      }
      return (Activator ? 1 : 0);
      /*
      if (args[0] == 0 && argCount > 1 && args[1] == 1) {
        // to world
        Activator = nullptr;
        return 0; //???
      }
      */
      //GCon->Logf("ACSF_SetActivator: tid=%d; ptr=%d", args[0], (argCount > 1 ? args[1] : 0));

    // bool SetActivatorToTarget (int tid)
    case ACSF_SetActivatorToTarget:
      //GCon->Logf("ACSF_SetActivatorToTarget: argc=%d; arg[0]=%d", argCount, (argCount > 0 ? args[0] : 0));
      if (argCount > 0) {
        VEntity *src = EntityFromTID(args[0], Activator);
        if (!src) return 0; // oops
        VEntity *tgt = src->eventFindTargetForACS();
        if (!tgt) return 0;
        //GCon->Logf("ACSF_SetActivatorToTarget: new target is <%s>; tid=%d", *tgt->GetClass()->GetFullName(), tgt->TID);
        Activator = tgt;
        return 1;
      }
      return 0;


    case ACSF_SetLineActivation:
      if (argCount > 1) {
        int searcher = -1;
        line_t *line = XLevel->FindLine(args[0], &searcher);
        if (line) line->SpacFlags = args[2];
      }
      return 0;

    case ACSF_GetLineActivation:
      if (argCount > 0) {
        int searcher = -1;
        line_t *line = XLevel->FindLine(args[0], &searcher);
        if (line) return line->SpacFlags;
      }
      return 0;


    case ACSF_SetCVar:
      if (argCount >= 2) {
        VName name = GetName(args[0]);
        if (name == NAME_None) return 0;
        if (!VCvar::CanBeModified(*name, true, true)) return 0; // modonly, noserver
        //GCon->Logf("ACSF: set cvar '%s' (%f)", *name, args[1]/65536.0f);
        VCvar::Set(*name, args[1] /* /65536.0f */);
        return 1;
      }
      return 0;

    //k8: this should work over network, but meh
    case ACSF_GetUserCVar:
      if (argCount >= 2) {
        VName name = GetName(args[1]);
        if (name == NAME_None) return 0;
        //GCon->Logf("ACSF: get user cvar '%s' (%f)", *name, VCvar::GetFloat(*name));
        //FIXME:return (int)(VCvar::GetFloat(*name)*65536.0f);
        return VCvar::GetInt(*name);
      }
      return 0;

    //k8: this should work over network, but meh
    case ACSF_SetUserCVar:
      if (argCount >= 3) {
        VName name = GetName(args[1]);
        if (name == NAME_None) return 0;
        if (!VCvar::CanBeModified(*name, true, true)) return 0; // modonly, noserver
        //GCon->Logf("ACSF: set user cvar '%s' (%f)", *name, args[2]/65536.0f);
        VCvar::Set(*name, args[2] /* /65536.0f */);
        return 1;
      }
      return 0;

    // non-existent vars should return 0 here (yay, another ACS hack!)
    case ACSF_GetCVarString:
      if (argCount >= 1) {
        VName name = GetName(args[0]);
        //GCon->Logf(NAME_Debug, "GetCVARString: %s", *name);
        if (name == NAME_None) return 0; //ActiveObject->Level->PutNewString("");
        if (!VCvar::HasVar(*name)) return 0;
        //GCon->Logf("ACSF_GetCVarString: var=<%s>; value=<%s>", *name, *VCvar::GetString(*name));
        return ActiveObject->Level->PutNewString(VCvar::GetString(*name));
      }
      return 0; //ActiveObject->Level->PutNewString("");

    case ACSF_SetCVarString:
      //GCon->Logf("***ACSF_SetCVarString: var=<%s>", *GetName(args[0]));
      if (argCount >= 2) {
        VName name = GetName(args[0]);
        if (name == NAME_None) return 0;
        //GCon->Logf("ACSF_SetCVarString: var=<%s>; value=<%s>; allowed=%d", *name, *GetStr(args[1]), (int)VCvar::CanBeModified(*name, true, true));
        if (!VCvar::CanBeModified(*name, true, true)) return 0; // modonly, noserver
        VStr value = GetStr(args[1]);
        VCvar::Set(*name, value);
        return 1;
      }
      return 0;

    //k8: this should work over network, but meh
    // non-existent vars should return 0 here (yay, another ACS hack!)
    case ACSF_GetUserCVarString:
      if (argCount >= 2) {
        VName name = GetName(args[1]);
        if (name == NAME_None) return 0; //ActiveObject->Level->PutNewString("");
        if (!VCvar::HasVar(*name)) return 0;
        //GCon->Logf("ACSF_GetUserCVarString: var=<%s>; value=<%s>", *name, *VCvar::GetString(*name));
        return ActiveObject->Level->PutNewString(VCvar::GetString(*name));
      }
      return 0; //ActiveObject->Level->PutNewString("");

    //k8: this should work over network, but meh
    case ACSF_SetUserCVarString:
      //GCon->Logf("***ACSF_SetUserCVarString: var=<%s>", *GetName(args[1]));
      if (argCount >= 3) {
        VName name = GetName(args[1]);
        if (name == NAME_None) return 0;
        //GCon->Logf("ACSF_SetUserCVarString: var=<%s>; value=<%s>; allowed=%d", *name, *GetStr(args[2]), (int)VCvar::CanBeModified(*name, true, true));
        if (!VCvar::CanBeModified(*name, true, true)) return 0;
        VStr value = GetStr(args[2]);
        VCvar::Set(*name, value);
        return 1;
      }
      return 0;


    case ACSF_PlayerIsSpectator_Zandro:
      return 0;

    case ACSF_GetPlayerLivesLeft_Zandro:
    case ACSF_SetPlayerLivesLeft_Zandro:
      return 0;

    // https://zdoom.org/wiki/ConsolePlayerNumber
    //FIXME: disconnect?
    case ACSF_ConsolePlayerNumber_Zandro:
      //GCon->Logf(NAME_Warning, "ERROR: unimplemented ACSF function #%d'", 102);
      //if (!isZandroACS()) return 0;
      #ifdef CLIENT
      if (GGameInfo->NetMode == NM_Standalone ||
          GGameInfo->NetMode == NM_Client ||
          GGameInfo->NetMode == NM_ListenServer)
      {
        if (cl && cls.signon && cl->MO) {
          //GCon->Logf(NAME_Warning, "CONPLRNUM: %d", cl->ClientNum);
          return cl->ClientNum;
        }
      }
      #endif
      return -1;

    // int RequestScriptPuke (int script[, int arg0[, int arg1[, int arg2[, int arg3]]]])
    case ACSF_RequestScriptPuke_Zandro:
      //if (!isZandroACS()) return 0;
      {
        #ifdef CLIENT
        if (argCount < 1) return 0;
        if (GGameInfo->NetMode != NM_Client && GGameInfo->NetMode != NM_Standalone && GGameInfo->NetMode != NM_TitleMap) {
          GCon->Logf(NAME_Warning, "ACS: Zandro RequestScriptPuke can be executed only by clients (%d).", GGameInfo->NetMode);
          return 0;
        }
        // ignore this in demos
        //FIXME: check `net` flag first
        if (cls.demoplayback) {
          return 1;
        }
        // do it
        if (args[0] == 0) return 1;
        int ScArgs[4];
        ScArgs[0] = (argCount > 1 ? args[1] : 0);
        ScArgs[1] = (argCount > 2 ? args[2] : 0);
        ScArgs[2] = (argCount > 3 ? args[3] : 0);
        ScArgs[3] = (argCount > 4 ? args[4] : 0);
        VEntity *plr = nullptr;
        if (GGameInfo->NetMode == NM_Standalone || GGameInfo->NetMode == NM_Client || GGameInfo->NetMode == NM_TitleMap) {
          if (cl && cls.signon && cl->MO) {
            plr = cl->MO;
            //GCon->Logf("plr: %p", plr);
          }
        }
        if (!ActiveObject->Level->Start(abs(args[0]), 0/*map*/, ScArgs[0], ScArgs[1], ScArgs[2], ScArgs[3], plr, line, side, (args[0] < 0)/*always*/, false/*wantresult*/, true/*net*/)) return 0;
        return 1;
        #else
        GCon->Log(NAME_Warning, "ACS: Zandro RequestScriptPuke can be executed only by clients.");
        return 0;
        #endif
      }

    // int NamedRequestScriptPuke (str script[, int arg0[, int arg1[, int arg2[, int arg3]]]])
    case ACSF_NamedRequestScriptPuke_Zandro:
      //if (!isZandroACS()) return 0;
      {
        #ifdef CLIENT
        if (argCount < 1) return 0;
        if (GGameInfo->NetMode != NM_Client && GGameInfo->NetMode != NM_Standalone && GGameInfo->NetMode != NM_TitleMap) {
          GCon->Logf(NAME_Warning, "ACS: Zandro NamedRequestScriptPuke can be executed only by clients (%d).", GGameInfo->NetMode);
          return 0;
        }
        // ignore this in demos
        //FIXME: check `net` flag first
        if (cls.demoplayback) {
          return 1;
        }
        // do it
        VName name = GetNameLowerCase(args[0]);
        if (name == NAME_None) return 0;
        int ScArgs[4];
        ScArgs[0] = (argCount > 1 ? args[1] : 0);
        ScArgs[1] = (argCount > 2 ? args[2] : 0);
        ScArgs[2] = (argCount > 3 ? args[3] : 0);
        ScArgs[3] = (argCount > 4 ? args[4] : 0);
        VEntity *plr = nullptr;
        if (GGameInfo->NetMode == NM_Standalone || GGameInfo->NetMode == NM_Client || GGameInfo->NetMode == NM_TitleMap) {
          if (cl && cls.signon && cl->MO) plr = cl->MO;
        }
        if (!ActiveObject->Level->Start(-name.GetIndex(), 0/*map*/, ScArgs[0], ScArgs[1], ScArgs[2], ScArgs[3], plr, line, side, false/*always*/, false/*wantresult*/, true/*net*/)) return 0;
        return 1;
        #else
        GCon->Log(NAME_Warning, "ACS: Zandro NamedRequestScriptPuke can be executed only by clients.");
        return 0;
        #endif
      }

    // int PickActor (int source, fixed angle, fixed pitch, fixed distance, int tid [, int actorMask [, int wallMask [, int flags]]])
    case ACSF_PickActor:
      if (argCount < 5) return 0;
      {
        /*
        GCon->Logf("ACSF_PickActor: argc=%d; arg[0]=%d; arg[1]=%d; arg[2]=%d; arg[3]=%d; arg[4]=%d; arg[5]=%d; arg[6]=%d; arg[7]=%d", argCount,
          (argCount > 0 ? args[0] : 0), (argCount > 1 ? args[1] : 1),
          (argCount > 2 ? args[2] : 0), (argCount > 3 ? args[3] : 1),
          (argCount > 4 ? args[4] : 0), (argCount > 5 ? args[5] : 1),
          (argCount > 6 ? args[6] : 0), (argCount > 7 ? args[7] : 1));
        */
        VEntity *src = EntityFromTID(args[0], Activator);
        if (!src) return 0; // oops
        if (args[3] < 0) return 0;
        // create direction vector
        TAVec ang;
        ang.yaw = 360.0f*float(args[1])/65536.0f;
        ang.pitch = 360.0f*float(args[2])/65536.0f;
        ang.roll = 0;
        TVec dir(0, 0, 0);
        AngleVector(ang, dir);
        //dir = NormaliseSafe(dir);
        //dir.normaliseInPlace();
        VEntity *hit = src->eventPickActor(
          false, TVec(0, 0, 0), // origin
          dir, float(args[3])/65536.0f,
          argCount > 5, (argCount > 5 ? args[5] : 0), // actormask
          argCount > 6, (argCount > 6 ? args[6] : 0)); // wallmask
        if (!hit) return 0;
        //GCon->Logf("ACSF_PickActor: hit=<%s>; tid=%d", *hit->GetClass()->GetFullName(), hit->TID);
        // assign tid
        int flags = (argCount > 7 ? args[7] : 0);
        int newtid = args[4];
        if (newtid < 0) { GCon->Logf(NAME_Warning, "ACS: PickActor with negative tid (%d)", newtid); return (flags&PICKAF_RETURNTID ? hit->TID : 1); }
        // assign new tid
        if ((flags&PICKAF_FORCETID) != 0 || hit->TID == 0) {
          if (newtid != 0 || (flags&PICKAF_FORCETID) != 0) {
            //int oldtid = hit->TID;
            hit->SetTID(newtid);
            //GCon->Logf("ACSF_PickActor: TID change: hit=<%s>; oldtid=%d; newtid=%d", *hit->GetClass()->GetFullName(), oldtid, hit->TID);
          }
        }
        return (flags&PICKAF_RETURNTID ? hit->TID : 1);
      }

    // void LineAttack (int tid, fixed angle, fixed pitch, int damage [, str pufftype [, str damagetype [, fixed range [, int flags [, int pufftid]]]]]);
    case ACSF_LineAttack:
      if (argCount >= 4) {
        VEntity *src = EntityFromTID(args[0], Activator);
        if (!src) return 0; // oops
        // create direction vector
        VName pufft = VName("BulletPuff");
        if (argCount > 4) pufft = GetName(args[4]);
        VName dmgt = VName("None");
        if (argCount > 5) dmgt = GetName(args[5]);
        if (dmgt == NAME_None) dmgt = VName("None");
        TAVec ang;
        ang.yaw = 360.0f*float(args[1])/65536.0f;
        ang.pitch = 360.0f*float(args[2])/65536.0f;
        ang.roll = 0;
        TVec dir(0, 0, 0);
        AngleVector(ang, dir);
        //dir = NormaliseSafe(dir);
        //dir.normaliseInPlace();
        // dir, range, damage,
        // pufftype, damagetype,
        // flags, pufftid
        src->eventLineAttackACS(
          dir, (argCount > 6 ? float(args[6])/65536.0f : 2048.0f), float(args[3])/65536.0f,
          pufft, dmgt,
          (argCount > 7 ? args[7] : 0), (argCount > 8 ? args[8] : 0));
      }
      return 0;

    case ACSF_GetChar:
      if (argCount >= 2) {
        VStr s = GetStr(args[0]);
        int idx = args[1];
        if (idx >= 0 && idx < s.length()) {
          /*
          if ((vuint8)s[idx] > 32) {
            GCon->Logf("GetChar(%d:%s)=%d (%c)", idx, *s.quote(), s[idx], (vuint8)s[idx]);
          } else {
            GCon->Logf("GetChar(%d:%s)=%d", idx, *s.quote(), (vuint8)s[idx]);
          }
          */
          return (vuint8)s[idx];
        }
      }
      return 0;

    case ACSF_strcmp:
    case ACSF_stricmp:
      if (argCount >= 2) {
        int maxlen = (argCount > 2 ? args[2] : MAX_VINT32);
        VStr s0 = GetStr(args[0]);
        VStr s1 = GetStr(args[1]);
        int curpos = 0;
        while (curpos < maxlen) {
          if (curpos >= s0.length() || curpos >= s1.length()) {
            if (s0.length() == s1.length()) return 0; // equal
            return (s0.length() < s1.length() ? -1 : 1);
          }
          char c0 = s0[curpos];
          char c1 = s1[curpos];
          if (funcIndex == ACSF_stricmp) {
            c0 = VStr::upcase1251(c0);
            c1 = VStr::upcase1251(c1);
          }
          if (c0 < c1) return -1;
          if (c0 > c1) return 1;
          ++curpos;
        }
      }
      return 0;

    case ACSF_StrLeft:
      if (argCount >= 2) {
        VStr s = GetStr(args[0]);
        int newlen = args[2];
        if (newlen <= 0) return ActiveObject->Level->PutNewString("");
        if (newlen >= s.length()) return args[0];
        return ActiveObject->Level->PutNewString(s.left(newlen));
      }
      return ActiveObject->Level->PutNewString("");

    case ACSF_StrRight:
      if (argCount >= 2) {
        VStr s = GetStr(args[0]);
        int newlen = args[2];
        if (newlen <= 0) return ActiveObject->Level->PutNewString("");
        if (newlen >= s.length()) return args[0];
        return ActiveObject->Level->PutNewString(s.right(newlen));
      }
      return ActiveObject->Level->PutNewString("");

    case ACSF_StrMid:
      if (argCount >= 3) {
        VStr s = GetStr(args[0]);
        int pos = args[1];
        if (pos < 0) pos = 0;
        int newlen = args[2];
        //GCon->Logf("***StrMid: <%s> (pos=%d; newlen=%d): <%s>", *s.quote(), pos, newlen, *s.mid(pos, newlen).quote());
        if (newlen <= 0) return ActiveObject->Level->PutNewString("");
        if (pos == 0 && newlen >= s.length()) return args[0];
        return ActiveObject->Level->PutNewString(s.mid(pos, newlen));
      }
      return ActiveObject->Level->PutNewString("");


    // int SetUserVariable (int tid, str name, value)
    case ACSF_SetUserVariable:
      if (argCount >= 3) {
        VStr s = GetStr(args[1]).toLowerCase();
        if (!s.startsWith("user_")) {
          if (acs_dump_uservar_access) GCon->Logf(NAME_Warning, "ACS: cannot set user variable with name \"%s\"", *GetStr(args[1]));
          return 0;
        }
        VName fldname = VName(*s);
        int count = 0;
        if (args[0] == 0) {
          if (doSetUserVarOrArray(Activator, fldname, args[2], false)) ++count;
        } else {
          for (VEntity *mobj = Level->FindMobjFromTID(args[0], nullptr); mobj; mobj = Level->FindMobjFromTID(args[0], mobj)) {
            if (doSetUserVarOrArray(mobj, fldname, args[2], false)) ++count;
          }
        }
        return count;
      }
      return 0;

    // GetUserVariable (int tid, str name)
    case ACSF_GetUserVariable:
      if (argCount >= 2) {
        VStr s = GetStr(args[1]).toLowerCase();
        if (!s.startsWith("user_")) {
          GCon->Logf("ACS: cannot get user variable with name \"%s\"", *GetStr(args[1]));
          return 0;
        }
        VEntity *mobj = EntityFromTID(args[0], Activator);
        if (!mobj) {
          GCon->Logf("ACS: cannot get user variable with name \"%s\" from nothing", *GetStr(args[1]));
          return 0;
        }
        VName fldname = VName(*s);
        return doGetUserVarOrArray(mobj, fldname, false);
      }
      return 0;

    // int SetUserArray (int tid, str name, int pos, int value)
    case ACSF_SetUserArray:
      if (argCount >= 4) {
        VStr s = GetStr(args[1]).toLowerCase();
        if (!s.startsWith("user_")) {
          GCon->Logf(NAME_Warning, "ACS: cannot set user array with name \"%s\"", *GetStr(args[1]));
          return 0;
        }
        VName fldname = VName(*s);
        int count = 0;
        if (args[0] == 0) {
          if (doSetUserVarOrArray(Activator, fldname, args[3], true, args[2])) ++count;
        } else {
          for (VEntity *mobj = Level->FindMobjFromTID(args[0], nullptr); mobj; mobj = Level->FindMobjFromTID(args[0], mobj)) {
            if (doSetUserVarOrArray(mobj, fldname, args[3], true, args[2])) ++count;
          }
        }
        return count;
      }
      return 0;

    // GetUserArray (int tid, str name, int pos)
    case ACSF_GetUserArray:
      if (argCount >= 3) {
        VStr s = GetStr(args[1]).toLowerCase();
        if (!s.startsWith("user_")) {
          GCon->Logf("ACS: cannot get user array with name \"%s\"", *GetStr(args[1]));
          return 0;
        }
        VEntity *mobj = EntityFromTID(args[0], Activator);
        if (!mobj) {
          GCon->Logf("ACS: cannot get user array with name \"%s\" from nothing", *GetStr(args[1]));
          return 0;
        }
        VName fldname = VName(*s);
        return doGetUserVarOrArray(mobj, fldname, true, args[2]);
      }
      return 0;


    // void SetSkyScrollSpeed (int sky, fixed skyspeed);
    case ACSF_SetSkyScrollSpeed: return 0;
    // int GetAirSupply (int playernum) -- in tics
    case ACSF_GetAirSupply: return 35; // always one second
    case ACSF_SetAirSupply: return 1; // say that we did it

    case ACSF_AnnouncerSound: // ignored
      return 0;

    case ACSF_PlayerIsLoggedIn_Zandro:
      return 0; // player is never logged in
    //case ACSF_GetPlayerAccountName_Zandro: return 0; // `0` means "not implemented" //ActiveObject->Level->PutNewString(""); // always unnamed
    case ACSF_GetPlayerAccountName_Zandro:
      if (!isZandroACS()) return 0;
      return ActiveObject->Level->PutNewString(""); // always unnamed

    // bool SetPointer(int assign_slot, int tid[, int pointer_selector[, int flags]])
    case ACSF_SetPointer:
      if (argCount >= 2 && Activator) {
        return (Activator->eventSetPointerForACS(args[0], args[1], (argCount > 2 ? args[2] : 0), (argCount > 3 ? args[3] : 0)) ? 1 : 0);
      }
      return 0;

    // int Sqrt (int number)
    case ACSF_Sqrt:
      if (argCount > 0 && args[0] > 0) return (int)sqrtf((float)args[0]);
      return 0;

    // fixed FixedSqrt (fixed number)
    case ACSF_FixedSqrt:
      if (argCount > 0 && args[0] > 0) return (int)(sqrtf((float)args[0]/65536.0f)*65536.0f);
      return 0;

    //case ACSF_StrArg:
    //  return -FName(FBehavior::StaticLookupString(args[0]));

    case ACSF_Floor:
      return (argCount > 0 ? args[0]&~0xffff : 0);

    case ACSF_Ceil:
      return (argCount > 0 ? (args[0]&~0xffff)+0x10000 : 0);

    case ACSF_Round:
      return (argCount > 0 ? (args[0]+32768)&~0xffff : 0);

    case ACSF_CheckSight: //untested!
      if (argCount >= 2) {
        unsigned flags = VEntity::CSE_CheckBaseRegion;
        if (argCount >= 3) {
          if (args[2]&1) flags |= VEntity::CSE_IgnoreFakeFloors;
          if (args[2]&2) flags |= VEntity::CSE_IgnoreBlockAll;
        }
        //bool CanSeeEx (VEntity *Ent, unsigned flags=0);

        if (args[0] == 0) {
          if (args[1] == 0) return true;
          if (Activator) {
            for (VEntity *mobj = Level->FindMobjFromTID(args[1], nullptr); mobj; mobj = Level->FindMobjFromTID(args[1], mobj)) {
              if (Activator->CanSeeEx(mobj, flags)) return 1;
            }
          }
        } else {
          for (VEntity *src = Level->FindMobjFromTID(args[0], nullptr); src; src = Level->FindMobjFromTID(args[0], src)) {
            if (args[1] != 0) {
              for (VEntity *dest = Level->FindMobjFromTID(args[1], nullptr); dest; dest = Level->FindMobjFromTID(args[1], dest)) {
                if (src->CanSeeEx(dest, flags)) return 1;
              }
            } else {
              if (Activator && src->CanSeeEx(Activator, flags)) return 1;
            }
          }
        }
      }
      return 0;

    case ACSF_ScriptCall:
      if (argCount >= 2) {
        VStr clsname = GetStr(args[0]).toLowerCase();
        VStr funcname = GetStr(args[1]).toLowerCase();
        GCon->Logf(NAME_Error, "ACSF_ScriptCall(%s,%s,%d) -- unimplemented", *clsname, *funcname, argCount);
      }
      return 0;

    // fixed VectorLength (fixed x, fixed y)
    case ACSF_VectorLength:
      if (argCount >= 2) {
        TVec v = TVec((float)args[0]/65536.0f, (float)args[0]/65536.0f);
        float len = v.length2D();
        return (int)(len/65536.0f);
      }
      return 0;

    // bool CheckActorProperty (int tid, int property, int value)
    case ACSF_CheckActorProperty:
      if (argCount >= 3) {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (!Ent) return 0; // never equals
        //if (developer) GCon->Logf(NAME_Dev, "GetActorProperty: ent=<%s>, propid=%d", Ent->GetClass()->GetName(), args[1]);
        int val = Ent->eventGetActorProperty(args[1]);
        // convert special properties
        switch (args[1]) {
          case 20: //APROP_Species
          case 21: //APROP_NameTag
            val = ActiveObject->Level->PutNewString(*VName(EName(val)));
            break;
        }
        return (val == args[2]);
      }
      return 0;


    // void Radius_Quake2 (int tid, int intensity, int duration, int damrad, int tremrad, str sound)
    case ACSF_Radius_Quake2:
      if (argCount >= 5) {
        VName sndname = NAME_None;
        if (argCount > 5) sndname = GetNameLowerCase(args[5]);
        Level->eventAcsRadiusQuake2(Activator, args[0], args[1], args[2], args[3], args[4], sndname);
      }
      return 0;

    // void QuakeEx (int tid, int intensityX, int intensityY, int intensityZ, int duration, int damrad, int tremrad, sound sfx [, int flags [, fixed mulwavex [, fixed mulwavey [, fixed mulwavez [, int falloff [, int highpoint [, fixed rollintensity [, fixed rollwave]]]]]]]])
    case ACSF_QuakeEx:
      GCon->Logf(NAME_Warning, "partially implemented ACSF function '%s' (%d args)", "QuakeEx", argCount);
      if (argCount >= 7) {
        VName sndname = NAME_None;
        const int tid = args[0];
        //const int intensity = (args[1]+args[2]+args[3])/3; // for now
        const int intensity = max3(args[1], args[2], args[3]); // for now
        const int dur = args[4];
        const int damrad = args[5];
        const int tremrad = args[6];
        if (argCount > 7) sndname = GetNameLowerCase(args[7]);
        Level->eventAcsRadiusQuake2(Activator, tid, intensity, dur, damrad, tremrad, sndname);
      }
      return 0;

    // bool Warp (int tid, fixed xofs, fixed yofs, fixed zofs, fixed angle, int flags [, str success_state [, bool exact [, fixed heightoffset [, fixed radiusoffset [, fixed pitch]]]]])
    case ACSF_Warp:
      if (argCount >= 6) {
        const int tid = args[0];
        const float xofs = args[1]/float(0x10000);
        const float yofs = args[2]/float(0x10000);
        const float zofs = args[3]/float(0x10000);
        const float angle = args[4]/float(0x10000);
        const int flags = args[5];
        VName succState = (argCount > 6 ? GetNameLowerCase(args[6]) : NAME_None);
        const bool exact = (argCount > 7 ? !!args[7] : false);
        const float hofs = (argCount > 8 ? args[8]/float(0x10000) : 0.0f);
        const float rofs = (argCount > 9 ? args[9]/float(0x10000) : 0.0f);
        const float pitch = (argCount > 10 ? args[10]/float(0x10000) : 0.0f);
        Level->eventAcsWarp(Activator, tid, xofs, yofs, zofs, angle, flags, succState, exact, hofs, rofs, pitch);
      }
      return 0;

    case ACSF_SetHUDClipRect:
      GCon->Logf(NAME_Error, "unimplemented ACSF function '%s' (%d args)", "SetHUDClipRect", argCount);
      return 0;
    case ACSF_SetHUDWrapWidth:
      GCon->Logf(NAME_Error, "unimplemented ACSF function '%s' (%d args)", "SetHUDWrapWidth", argCount);
      return 0;

    case ACSF_CheckFont:
      if (argCount > 0) {
        #ifdef CLIENT
        return (T_IsFontExists(GetNameLowerCase(args[0])) ? 1 : 0);
        #else
        //FIXME
        //TODO
        return 1;
        #endif
      }
      return 0;

    case ACSF_CheckClass:
      if (argCount > 0) {
        VName cname = GetNameLowerCase(args[0]);
        if (cname != NAME_None) {
          VClass *cls = VClass::FindClassNoCase(*cname);
          while (cls) {
            if (VStr::strEqu(cls->GetName(), "Actor")) return 1;
            cls = cls->ParentClass;
          }
        }
      }
      return 0;

    //bool CheckProximity (int tid, str classname, float distance [, int count [, int flags [, int ptr]]])
    case ACSF_CheckProximity:
      if (argCount >= 3) {
        VEntity *ent = EntityFromTID(args[0], Activator);
        if (ent) {
          bool res = ent->doCheckProximity(GetNameLowerCase(args[1]),
                                           (float)(args[2])/65536.0f,
                                           (argCount > 3), (argCount > 3 ? args[3] : 0),
                                           (argCount > 4), (argCount > 4 ? args[4] : 0),
                                           (argCount > 5), (argCount > 5 ? args[5] : 0));
          return (res ? 1 : 0);
        }
      }
      return 0;

    case ACSF_DropItem:
      if (argCount >= 2) {
        VName itemName = GetNameLowerCase(args[1]);
        if (itemName == NAME_None || itemName == "null" || itemName == "none") return 0;
        int tid = args[0];
        int amount = (argCount > 2 ? args[2] : 0);
        float chance = (argCount > 3 ? (float)(args[3])/256.0f : 1.0f);
        int res = 0;
        for (VEntity *mobj = Level->FindMobjFromTID(tid, nullptr); mobj; mobj = Level->FindMobjFromTID(tid, mobj)) {
          if (mobj->eventDropItem(itemName, amount, chance)) ++res;
        }
        return res;
      }
      return 0;

    // void DropInventory (int tid, str itemtodrop);
    case ACSF_DropInventory:
      if (argCount >= 2) {
        VName itemName = GetNameLowerCase(args[1]);
        if (itemName == NAME_None || itemName == "null" || itemName == "none") return 0;
        int tid = args[0];
        for (VEntity *mobj = Level->FindMobjFromTID(tid, nullptr); mobj; mobj = Level->FindMobjFromTID(tid, mobj)) {
          mobj->eventACSDropInventory(itemName);
        }
      }
      return 0;

    case ACSF_GetPolyobjX:
    case ACSF_GetPolyobjY:
      if (argCount > 0) {
        polyobj_t *pobj = XLevel->GetPolyobj(args[0]);
        if (!pobj) return 0x7FFFFFFF; // doesn't exist
        if (funcIndex == ACSF_GetPolyobjX) {
          return (int)(pobj->startSpot.x*65536.0f);
        } else {
          return (int)(pobj->startSpot.y*65536.0f);
        }
      }
      return 0x7FFFFFFF; // doesn't exist

    case ACSF_SpawnParticle:
      return 0;

    case ACSF_SoundSequenceOnActor:
      GCon->Logf(NAME_Warning, "ignored ACSF `SoundSequenceOnActor`");
      return 0;
    case ACSF_SoundSequenceOnSector:
      GCon->Logf(NAME_Warning, "ignored ACSF `SoundSequenceOnSector`");
      return 0;
    case ACSF_SoundSequenceOnPolyobj:
      GCon->Logf(NAME_Warning, "ignored ACSF `SoundSequenceOnPolyobj`");
      return 0;

    // void SetSectorGlow (int tag, bool plane, int red, int green, int blue, int height)
    // plane: if true, the glow is applied on the ceiling, otherwise (i.e. if false) it is applied on the floor
    // red: if this is set to -1, the glow effect is removed (the other color compounds are ignored)
    case ACSF_SetSectorGlow:
      //GCon->Logf(NAME_Warning, "ignored ACSF `SetSectorGlow`");
      if (argCount >= 6) {
        sector_t *sector;
        const bool setIt = (args[2] >= 0 && args[5] > 0);
        const vuint32 clr = (setIt ? 0xff000000u|(clampToByte(args[2])<<16)|(clampToByte(args[3])<<8)|clampToByte(args[4]) : 0u);
        for (int sidx = FindSectorFromTag(sector, args[0]); sidx >= 0; sidx = FindSectorFromTag(sector, args[0], sidx)) {
          if (setIt) {
            // set glow
            if (args[1]) {
              // ceiling
              sector->params.lightFCFlags |= sec_params_t::LFC_CeilingLight_Glow;
              sector->params.glowCeiling = clr;
              sector->params.glowCeilingHeight = args[5];
            } else {
              // floor
              sector->params.lightFCFlags |= sec_params_t::LFC_FloorLight_Glow;
              sector->params.glowFloor = clr;
              sector->params.glowFloorHeight = args[5];
            }
          } else {
            // reset glow
            if (args[1]) {
              // ceiling
              sector->params.lightFCFlags &= ~sec_params_t::LFC_CeilingLight_Glow;
              sector->params.glowCeiling = 0; // reset color
              sector->params.glowCeilingHeight = 0; // reset height
            } else {
              // floor
              sector->params.lightFCFlags &= ~sec_params_t::LFC_FloorLight_Glow;
              sector->params.glowFloor = 0; // reset color
              sector->params.glowFloorHeight = 0; // reset height
            }
          }
        }
      }
      return 0;

    // void SetSectorDamage (int tag, int amount [, string damagetype [, int interval [, int leaky]]])
    case ACSF_SetSectorDamage:
      if (argCount >= 1) {
        const int amount = clampval((argCount > 1 ? args[1] : 0), 0, 10000);
        VName dmgType = (argCount > 2 ? GetNameLowerCase(args[2]) : NAME_None);
        const int interval = max2(0, (argCount > 3 ? args[3] : 32));
        const int leaky = clampval((argCount > 4 ? args[4] : 0), 0, 256);
        sector_t *sector;
        for (int sidx = FindSectorFromTag(sector, args[0]); sidx >= 0; sidx = FindSectorFromTag(sector, args[0], sidx)) {
          sector->Damage = amount;
          sector->DamageType = dmgType;
          sector->DamageInterval = interval;
          sector->DamageLeaky = leaky;
        }
      }
      return 0;

    case ACSF_SetSectorTerrain:
      GCon->Logf(NAME_Warning, "ACSF `SetSectorTerrain` is not implemented yet");
      return 0;

    case ACSF_SetFogDensity:
      GCon->Logf(NAME_Warning, "ignored ACSF `SetFogDensity`");
      return 0;

    case ACSF_SpawnDecal:
      GCon->Logf(NAME_Error, "unimplemented ACSF function '%s' (%d args)", "SpawnDecal", argCount);
      return 0;

    // bool IsPointerEqual (int ptr_select1, int ptr_select2 [, int tid1 [, int tid2]]);
    case ACSF_IsPointerEqual:
      if (argCount >= 2) {
        VEntity *ent1 = EntityFromTID((argCount > 2 ? args[2] : 0), Activator);
        VEntity *ent2 = EntityFromTID((argCount > 3 ? args[3] : 0), Activator);
        if (ent1 && ent2) return (ent1->eventACSIsPointerEqual(args[0], args[1], ent2) ? 1 : 0);
      }
      return 0;

    // bool CanRaiseActor (int tid);
    case ACSF_CanRaiseActor:
      if (argCount >= 0) {
        VEntity *ent = EntityFromTID((argCount > 0 ? args[1] : 0), Activator);
        if (ent) return (ent->eventCanRaise() ? 1 : 0);
      }
      return 0;
  }

  for (const ACSF_Info *nfo = ACSF_List; nfo->name; ++nfo) {
    if (nfo->index == funcIndex) {
      if (acs_abort_on_unknown_acsf) {
        Host_Error("unimplemented ACSF function '%s' (%d args)", nfo->name, argCount);
      } else {
        GCon->Logf(NAME_Error, "unimplemented ACSF function '%s' (%d args)", nfo->name, argCount);
      }
      return 0;
    }
  }

  if (acs_abort_on_unknown_acsf) {
    Host_Error("unimplemented ACSF function #%d (%d args)", funcIndex, argCount);
  } else {
    GCon->Logf(NAME_Error, "unimplemented ACSF function #%d (%d args)", funcIndex, argCount);
  }

  return 0;
}


#define SB_PUSH  do { \
  if (PrintStr.length() || PrintStrStack.length()) PrintStrStack.append(PrintStr); \
  PrintStr.clear(); \
} while (0)

#define SB_POP  do { \
  if (PrintStrStack.length()) { \
    int pstlast = PrintStrStack.length()-1; \
    PrintStr = PrintStrStack[pstlast]; \
    PrintStrStack[pstlast].clear(); \
    PrintStrStack.removeAt(pstlast); \
  } else { \
    PrintStr.clear(); \
  } \
} while (0) \


//==========================================================================
//
//  VAcs::TranslateSpecial
//
//==========================================================================
void VAcs::TranslateSpecial (int &spec, int &arg1) {
  if (spec >= 0) return;
  bool unknown = true;
  if (spec >= -45 && spec <= -39) {
    // this is "ACS_NamedXXX", first arg is script name
    VName name = GetNameLowerCase(arg1);
    if (spec != -45) {
      GCon->Logf(NAME_Error, "Trying to set unknown ACSF execute special! NAME: '%s'", *name);
    } else {
      if (developer) GCon->Logf(NAME_Dev, "ACS: replaced `ACS_NamedExecuteAlways` with `ACS_ExecuteAlways`, script name is '%s'", *name);
      unknown = false;
      if (name != NAME_None) {
        spec = 226; // "execute always"
        arg1 = -(int)name.GetIndex();
      } else {
        spec = 0; // oops
      }
    }
  }
  if (!unknown) return;
  for (const ACSF_Info *nfo = ACSF_List; nfo->name; ++nfo) {
    if (nfo->index == -spec) {
      GCon->Logf(NAME_Error, "unimplemented ACSF line special #%d: '%s'", nfo->index, nfo->name);
      return;
    }
  }
  GCon->Logf(NAME_Error, "unimplemented ACSF line special #%d", -spec);
}


//==========================================================================
//
//  VAcs::RunScript
//
//==========================================================================
int VAcs::RunScript (float DeltaTime, bool immediate) {
  VAcsObject *WaitObject = nullptr;

  //fprintf(stderr, "VAcs::RunScript:000: self name is '%s' (number is %d)\n", *info->Name, info->Number);
  //if (info->RunningScript) fprintf(stderr, "VAcs::RunScript:001: rs name is '%s' (number is %d)\n", *info->RunningScript->info->Name, info->RunningScript->info->Number);

  //if (info->Name != NAME_None) GCon->Logf(NAME_Dev, "ACS: running \"%s\"", *info->Name);

  if (!acs_enabled) State = ASTE_Terminating;

  if (State == ASTE_Terminating) {
    if (info->RunningScript == this) info->RunningScript = nullptr;
    ResetSaveds(); // just in case
    DestroyThinker();
    return 1;
  }

  if (State == ASTE_WaitingForTag && !Level->eventTagBusy(WaitValue)) State = ASTE_Running;
  if (State == ASTE_WaitingForPoly && !Level->eventPolyBusy(WaitValue)) State = ASTE_Running;

  VAcsInfo *scpr = (State == ASTE_WaitingForScriptStart || State == ASTE_WaitingForScript ? XLevel->Acs->FindScript(WaitValue, WaitObject) : nullptr);

  if (State == ASTE_WaitingForScriptStart &&
      /*XLevel->Acs->FindScript(WaitValue, WaitObject)*/scpr &&
      /*XLevel->Acs->FindScript(WaitValue, WaitObject)*/scpr->RunningScript)
  {
    State = ASTE_WaitingForScript;
  }

  if (State == ASTE_WaitingForScript && scpr && !/*XLevel->Acs->FindScript(WaitValue, WaitObject)*/scpr->RunningScript) {
    State = ASTE_Running;
  }

  if (State != ASTE_Running) return 1;

  bool doRunItDT = true;
  bool doRunItVT = true;

  if (immediate) {
    DelayActivationTick = 0;
    DelayTime = 0;
  }

  if (DeltaTime < 0.0f) DeltaTime = 0.0f;

  if (DelayActivationTick > XLevel->TicTime) {
    //GCon->Logf("DELAY: DelayActivationTick=%d; DeltaTime=%f; time=%f; tictime=%d", DelayActivationTick, DeltaTime*1000, (double)XLevel->Time, XLevel->TicTime);
    doRunItDT = false;
  }

  if (DelayTime) {
    /*
    if (info->Number == 2603) {
      GCon->Logf("VAcs::RunScript: self name is '%s' (number is %d)", *info->Name, info->Number);
      GCon->Logf("  DELAY: DelayTime=%f; DeltaTime=%f; time=%f; tictime=%f", DelayTime*1000, DeltaTime*1000, (double)XLevel->Time, (double)XLevel->TicTime);
    }
    */
    DelayTime -= DeltaTime;
    if (DelayTime > 0) {
      doRunItVT = false;
    } else {
      //DelayTime = 0;
    }
  }

  if (acs_use_doomtic_granularity) {
    if (!doRunItDT) return 1;
  } else {
    if (!doRunItVT) return 1;
  }
  //k8: why i decided to limit this?
  if (DelayTime > 0) DelayTime = 0; else if (DelayTime < -1.0f/35.0f) DelayTime = -1.0f/35.0f;

  //fprintf(stderr, "VAcs::RunScript:002: self name is '%s' (number is %d)\n", *info->Name, info->Number);
  //  Shortcuts
  //int *WorldVars = Level->World->Acs->WorldVars;
  //int *GlobalVars = Level->World->Acs->GlobalVars;
  //VAcsGrowingArray *WorldArrays = Level->World->Acs->WorldArrays;
  //VAcsGrowingArray *GlobalArrays = Level->World->Acs->GlobalArrays;
  VAcsGlobal *globals = Level->World->Acs;

  TArray<VStr> PrintStrStack; // string builders must be stacked
  VStr PrintStr;
  vint32 resultValue = 1;
  vint32 *optstart = nullptr;
  vint32 *locals = LocalVars;
  vassert(locals);
  VAcsFunction *activeFunction = nullptr;
  VACSLocalArrays *localarrays = &info->LocalArrays;
  EAcsFormat fmt = ActiveObject->GetFormat();
  int action = SCRIPT_Continue;
  vuint8 *ip = InstructionPointer;
  // get a fresh stack
  ACSStack vmstack;
  vint32 *mystack = vmstack.stk;
  vint32 *sp = mystack;
  VTextureTranslation *Translation = nullptr;
#if !USE_COMPUTED_GOTO
  GCon->Logf(NAME_Debug, "ACS: === ENTERING SCRIPT %d(%s) at ip: %p (%d) ===", info->Number, *info->Name, ip, (int)(ptrdiff_t)(ip-info->Address));
#endif

  // init watchcat
  double sttime = Sys_Time();
  int scountLeft = ACS_GUARD_INSTRUCTION_COUNT;

  do {
    vint32 cmd;

#if USE_COMPUTED_GOTO
    static void *vm_labels[] = {
#define DECLARE_PCD(name) &&Lbl_PCD_ ## name
#include "p_acs.h"
    0 };
#endif

    // check stack
    if ((uintptr_t)sp < (uintptr_t)mystack) Host_Error("ACS: stack underflow");
    if ((ptrdiff_t)(sp-mystack) >= ACS_STACK_DEPTH) Host_Error("ACS: stack overflow");

    if (fmt == ACS_LittleEnhanced) {
      cmd = *ip;
      if (cmd >= 240) {
        cmd = 240+((cmd-240)<<8)+ip[1];
        ip += 2;
      } else {
        ++ip;
      }
    } else {
      cmd = READ_INT32(ip);
      ip += 4;
    }

#if !USE_COMPUTED_GOTO
    //GCon->Logf("ACS: SCRIPT %d; cmd: %d", info->Number, cmd);
    {
      const PCD_Info *pi;
      for (pi = PCD_List; pi->name; ++pi) if (pi->index == cmd) break;
      if (pi->name) {
        GCon->Logf("ACS: SCRIPT %d; %p: %05d: %3d (%s)", info->Number, ip, (int)(ptrdiff_t)(ip-info->Address), cmd, pi->name);
      } else {
        GCon->Logf("ACS: SCRIPT %d; %p: %05d: %3d", info->Number, ip, (int)(ptrdiff_t)(ip-info->Address), cmd);
      }
    }
#endif

    ACSVM_SWITCH(cmd)
    {
    // standard P-Code commands
    ACSVM_CASE(PCD_Nop)
      ACSVM_BREAK;

    ACSVM_CASE(PCD_Terminate)
      //GCon->Logf("ACS: SCRIPT %d; TERMINATE REQUESTED", info->Number);
      action = SCRIPT_Terminate;
      ACSVM_BREAK_STOP;

    ACSVM_CASE(PCD_Suspend)
      State = ASTE_Suspended;
      action = SCRIPT_Stop;
      if (activeFunction) {
        GCon->Logf(NAME_Error, "ACS script #%d (named '%s') tried to suspend inside a function; terminated", info->Number, *info->Name);
        action = SCRIPT_Terminate;
      }
      ACSVM_BREAK_STOP;

    ACSVM_CASE(PCD_PushNumber)
      *sp = READ_INT32(ip);
#ifdef ACS_DUMP_EXECUTION
      //GCon->Logf("ACS:    push %d (%u %u %u %u)", *sp, ip[0], ip[1], ip[2], ip[3]);
#endif
      ip += 4;
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSpec1)
      {
        int special = READ_BYTE_OR_INT32;
        INC_BYTE_OR_INT32;
        //GCon->Logf(NAME_Debug, "***ACS:%d: LSPEC1: special=%d; args=(%d)", info->Number, special, sp[-1]);
        Level->eventExecuteActionSpecial(special, sp[-1], 0, 0, 0, 0, line, side, Activator);
        --sp;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSpec2)
      {
        int special = READ_BYTE_OR_INT32;
        INC_BYTE_OR_INT32;
        //GCon->Logf(NAME_Debug, "***ACS:%d: LSPEC2: special=%d; args=(%d,%d)", info->Number, special, sp[-2], sp[-1]);
        Level->eventExecuteActionSpecial(special, sp[-2], sp[-1], 0, 0, 0, line, side, Activator);
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSpec3)
      {
        int special = READ_BYTE_OR_INT32;
        INC_BYTE_OR_INT32;
        //GCon->Logf(NAME_Debug, "***ACS:%d: LSPEC3: special=%d; args=(%d,%d,%d)", info->Number, special, sp[-3], sp[-2], sp[-1]);
        Level->eventExecuteActionSpecial(special, sp[-3], sp[-2], sp[-1], 0, 0, line, side, Activator);
        sp -= 3;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSpec4)
      {
        int special = READ_BYTE_OR_INT32;
        INC_BYTE_OR_INT32;
        //GCon->Logf(NAME_Debug, "***ACS:%d: LSPEC4: special=%d; args=(%d,%d,%d,%d)", info->Number, special, sp[-4], sp[-3], sp[-2], sp[-1]);
        Level->eventExecuteActionSpecial(special, sp[-4], sp[-3], sp[-2], sp[-1], 0, line, side, Activator);
        sp -= 4;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSpec5)
      {
        int special = READ_BYTE_OR_INT32;
        INC_BYTE_OR_INT32;
        //GCon->Logf(NAME_Debug, "***ACS:%d: LSPEC5: special=%d; args=(%d,%d,%d,%d,%d)", info->Number, special, sp[-5], sp[-4], sp[-3], sp[-2], sp[-1]);
        Level->eventExecuteActionSpecial(special, sp[-5], sp[-4], sp[-3], sp[-2], sp[-1], line, side, Activator);
        sp -= 5;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSpec1Direct)
      {
        int special = READ_BYTE_OR_INT32;
        INC_BYTE_OR_INT32;
        Level->eventExecuteActionSpecial(special, READ_INT32(ip), 0, 0, 0, 0, line, side, Activator);
        ip += 4;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSpec2Direct)
      {
        int special = READ_BYTE_OR_INT32;
        INC_BYTE_OR_INT32;
        Level->eventExecuteActionSpecial(special, READ_INT32(ip), READ_INT32(ip+4), 0, 0, 0, line, side, Activator);
        ip += 8;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSpec3Direct)
      {
        int special = READ_BYTE_OR_INT32;
        INC_BYTE_OR_INT32;
        Level->eventExecuteActionSpecial(special, READ_INT32(ip), READ_INT32(ip+4), READ_INT32(ip+8), 0, 0, line, side, Activator);
        ip += 12;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSpec4Direct)
      {
        int special = READ_BYTE_OR_INT32;
        INC_BYTE_OR_INT32;
        Level->eventExecuteActionSpecial(special, READ_INT32(ip), READ_INT32(ip+4), READ_INT32(ip+8), READ_INT32(ip+12), 0, line, side, Activator);
        ip += 16;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSpec5Direct)
      {
        int special = READ_BYTE_OR_INT32;
        INC_BYTE_OR_INT32;
        Level->eventExecuteActionSpecial(special, READ_INT32(ip), READ_INT32(ip+4), READ_INT32(ip+8), READ_INT32(ip+12), READ_INT32(ip+16), line, side, Activator);
        ip += 20;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_Add)
      sp[-2] += sp[-1];
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_Subtract)
      sp[-2] -= sp[-1];
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_Multiply)
      sp[-2] *= sp[-1];
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_Divide)
      if (sp[-1] == 0) Host_Error("ACS: division by zero in `Divide`");
      sp[-2] /= sp[-1];
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_Modulus)
      if (sp[-1] == 0) Host_Error("ACS: division by zero in `Modulus`");
      sp[-2] %= sp[-1];
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_EQ)
      sp[-2] = sp[-2] == sp[-1];
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_NE)
      //if (info->Number == 31234) GCon->Logf("ACS: %d: NE(%d, %d)", info->Number, sp[-2], sp[-1]);
      sp[-2] = sp[-2] != sp[-1];
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LT)
      sp[-2] = sp[-2] < sp[-1];
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GT)
      sp[-2] = sp[-2] > sp[-1];
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LE)
      sp[-2] = sp[-2] <= sp[-1];
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GE)
      sp[-2] = sp[-2] >= sp[-1];
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AssignScriptVar)
      //GCon->Logf("ACS:%d: PCD_AssignScriptVar(%p:%d): %d (old is %d)", info->Number, locals, READ_BYTE_OR_INT32, sp[-1], locals[READ_BYTE_OR_INT32]);
      locals[READ_BYTE_OR_INT32] = sp[-1];
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AssignMapVar)
      *ActiveObject->MapVars[READ_BYTE_OR_INT32] = sp[-1];
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AssignWorldVar)
      //WorldVars[READ_BYTE_OR_INT32] = sp[-1];
      globals->SetWorldVarInt(READ_BYTE_OR_INT32, sp[-1]);
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PushScriptVar)
      *sp = locals[READ_BYTE_OR_INT32];
      //GCon->Logf("ACS:%d: PCD_PushScriptVar(%p:%d): %d", info->Number, locals, READ_BYTE_OR_INT32, *sp);
      INC_BYTE_OR_INT32;
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PushMapVar)
      *sp = *ActiveObject->MapVars[READ_BYTE_OR_INT32];
      INC_BYTE_OR_INT32;
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PushWorldVar)
      //*sp = WorldVars[READ_BYTE_OR_INT32];
      *sp = globals->GetWorldVarInt(READ_BYTE_OR_INT32);
      INC_BYTE_OR_INT32;
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AddScriptVar)
      locals[READ_BYTE_OR_INT32] += sp[-1];
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AddMapVar)
      *ActiveObject->MapVars[READ_BYTE_OR_INT32] += sp[-1];
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AddWorldVar)
      //WorldVars[READ_BYTE_OR_INT32] += sp[-1];
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetWorldVarInt(vidx, globals->GetWorldVarInt(vidx)+sp[-1]);
      }
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SubScriptVar)
      locals[READ_BYTE_OR_INT32] -= sp[-1];
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SubMapVar)
      *ActiveObject->MapVars[READ_BYTE_OR_INT32] -= sp[-1];
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SubWorldVar)
      //WorldVars[READ_BYTE_OR_INT32] -= sp[-1];
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetWorldVarInt(vidx, globals->GetWorldVarInt(vidx)-sp[-1]);
      }
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_MulScriptVar)
      locals[READ_BYTE_OR_INT32] *= sp[-1];
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_MulMapVar)
      *ActiveObject->MapVars[READ_BYTE_OR_INT32] *= sp[-1];
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_MulWorldVar)
      //WorldVars[READ_BYTE_OR_INT32] *= sp[-1];
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetWorldVarInt(vidx, globals->GetWorldVarInt(vidx)*sp[-1]);
      }
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_DivScriptVar)
      if (sp[-1] == 0) Host_Error("ACS: division by zero in `DivScriptVar`");
      locals[READ_BYTE_OR_INT32] /= sp[-1];
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_DivMapVar)
      if (sp[-1] == 0) Host_Error("ACS: division by zero in `DivMapVar`");
      *ActiveObject->MapVars[READ_BYTE_OR_INT32] /= sp[-1];
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_DivWorldVar)
      if (sp[-1] == 0) Host_Error("ACS: division by zero in `DivWorldVar`");
      //WorldVars[READ_BYTE_OR_INT32] /= sp[-1];
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetWorldVarInt(vidx, globals->GetWorldVarInt(vidx)/sp[-1]);
      }
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ModScriptVar)
      if (sp[-1] == 0) Host_Error("ACS: division by zero in `ModScriptVar`");
      locals[READ_BYTE_OR_INT32] %= sp[-1];
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ModMapVar)
      if (sp[-1] == 0) Host_Error("ACS: division by zero in `ModMapVar`");
      *ActiveObject->MapVars[READ_BYTE_OR_INT32] %= sp[-1];
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ModWorldVar)
      if (sp[-1] == 0) Host_Error("ACS: division by zero in `ModWorldVar`");
      //WorldVars[READ_BYTE_OR_INT32] %= sp[-1];
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetWorldVarInt(vidx, globals->GetWorldVarInt(vidx)%sp[-1]);
      }
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_IncScriptVar)
      locals[READ_BYTE_OR_INT32]++;
      INC_BYTE_OR_INT32;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_IncMapVar)
      (*ActiveObject->MapVars[READ_BYTE_OR_INT32])++;
      INC_BYTE_OR_INT32;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_IncWorldVar)
      //WorldVars[READ_BYTE_OR_INT32]++;
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetWorldVarInt(vidx, globals->GetWorldVarInt(vidx)+1);
      }
      INC_BYTE_OR_INT32;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_DecScriptVar)
      locals[READ_BYTE_OR_INT32]--;
      INC_BYTE_OR_INT32;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_DecMapVar)
      (*ActiveObject->MapVars[READ_BYTE_OR_INT32])--;
      INC_BYTE_OR_INT32;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_DecWorldVar)
      //WorldVars[READ_BYTE_OR_INT32]--;
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetWorldVarInt(vidx, globals->GetWorldVarInt(vidx)-1);
      }
      INC_BYTE_OR_INT32;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_Goto)
      ip = ActiveObject->OffsetToPtr(READ_INT32(ip));
      ACSVM_BREAK;

    ACSVM_CASE(PCD_IfGoto)
      if (sp[-1]) {
        ip = ActiveObject->OffsetToPtr(READ_INT32(ip));
      } else {
        ip += 4;
      }
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_Drop)
      --sp;
      ACSVM_BREAK;

    #define PERFORM_DELAY(tics_) \
      if (activeFunction) { \
        GCon->Logf(NAME_Error, "ACS script #%d (named '%s') tried to `Delay()` inside a function; terminated", info->Number, *info->Name); \
        action = SCRIPT_Terminate; \
      } else { \
        const int tc = (tics_); \
        DelayTime += float(tc)/35.0f; \
        DelayActivationTick = XLevel->TicTime+tc; \
        if (DelayActivationTick <= XLevel->TicTime) DelayActivationTick = XLevel->TicTime+1; \
        action = SCRIPT_Stop; \
      }

    ACSVM_CASE(PCD_Delay)
      PERFORM_DELAY(sp[-1])
      --sp;
      ACSVM_BREAK_STOP;

    ACSVM_CASE(PCD_DelayDirect)
      PERFORM_DELAY(READ_INT32(ip))
      ip += 4;
      ACSVM_BREAK_STOP;

    ACSVM_CASE(PCD_Random)
      sp[-2] = sp[-2]+(vint32)(Random()*(sp[-1]-sp[-2]+1));
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_RandomDirect)
      *sp = READ_INT32(ip)+(vint32)(Random()*(READ_INT32(ip+4)-READ_INT32(ip)+1));
      ip += 8;
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ThingCount)
      sp[-2] = Level->eventThingCount(sp[-2], NAME_None, sp[-1], -1);
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ThingCountDirect)
      *sp = Level->eventThingCount(READ_INT32(ip), NAME_None, READ_INT32(ip+4), -1);
      ip += 8;
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_TagWait)
      WaitValue = sp[-1];
      State = ASTE_WaitingForTag;
      --sp;
      action = SCRIPT_Stop;
      if (activeFunction) {
        GCon->Logf(NAME_Error, "ACS script #%d (named '%s') tried to suspend inside a function; terminated", info->Number, *info->Name);
        action = SCRIPT_Terminate;
      }
      ACSVM_BREAK_STOP;

    ACSVM_CASE(PCD_TagWaitDirect)
      WaitValue = READ_INT32(ip);
      State = ASTE_WaitingForTag;
      ip += 4;
      action = SCRIPT_Stop;
      if (activeFunction) {
        GCon->Logf(NAME_Error, "ACS script #%d (named '%s') tried to suspend inside a function; terminated", info->Number, *info->Name);
        action = SCRIPT_Terminate;
      }
      ACSVM_BREAK_STOP;

    ACSVM_CASE(PCD_PolyWait)
      WaitValue = sp[-1];
      State = ASTE_WaitingForPoly;
      --sp;
      action = SCRIPT_Stop;
      if (activeFunction) {
        GCon->Logf(NAME_Error, "ACS script #%d (named '%s') tried to suspend inside a function; terminated", info->Number, *info->Name);
        action = SCRIPT_Terminate;
      }
      ACSVM_BREAK_STOP;

    ACSVM_CASE(PCD_PolyWaitDirect)
      WaitValue = READ_INT32(ip);
      State = ASTE_WaitingForPoly;
      ip += 4;
      action = SCRIPT_Stop;
      if (activeFunction) {
        GCon->Logf(NAME_Error, "ACS script #%d (named '%s') tried to suspend inside a function; terminated", info->Number, *info->Name);
        action = SCRIPT_Terminate;
      }
      ACSVM_BREAK_STOP;

    ACSVM_CASE(PCD_ChangeFloor)
      {
        //int Flat = GTextureManager.NumForName(GetName8(sp[-1]), TEXTYPE_Flat, true);
        int Flat = GTextureManager.FindOrLoadFullyNamedTextureAsMapTexture(GetStr(sp[-1]), nullptr, TEXTYPE_Flat, /*overload*/true);
        if (Flat > 0) { //???
          sector_t *sector;
          for (int Idx = FindSectorFromTag(sector, sp[-2]); Idx >= 0; Idx = FindSectorFromTag(sector, sp[-2], Idx)) {
            sector->floor.pic = Flat;
          }
        }
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ChangeFloorDirect)
      {
        int Tag = READ_INT32(ip);
        //int Flat = GTextureManager.NumForName(GetName8(READ_INT32(ip+4)|ActiveObject->GetLibraryID()), TEXTYPE_Flat, true);
        int Flat = GTextureManager.FindOrLoadFullyNamedTextureAsMapTexture(GetStr(READ_INT32(ip+4)|ActiveObject->GetLibraryID()), nullptr, TEXTYPE_Flat, /*overload*/true);
        ip += 8;
        if (Flat > 0) { //???
          sector_t *sector;
          for (int Idx = FindSectorFromTag(sector, Tag); Idx >= 0; Idx = FindSectorFromTag(sector, Tag, Idx)) {
            sector->floor.pic = Flat;
          }
        }
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ChangeCeiling)
      {
        //int Flat = GTextureManager.NumForName(GetName8(sp[-1]), TEXTYPE_Flat, true);
        int Flat = GTextureManager.FindOrLoadFullyNamedTextureAsMapTexture(GetStr(sp[-1]), nullptr, TEXTYPE_Flat, /*overload*/true);
        if (Flat > 0) { //???
          sector_t *sector;
          for (int Idx = FindSectorFromTag(sector, sp[-2]); Idx >= 0; Idx = FindSectorFromTag(sector, sp[-2], Idx)) {
            sector->ceiling.pic = Flat;
          }
        }
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ChangeCeilingDirect)
      {
        int Tag = READ_INT32(ip);
        //int Flat = GTextureManager.NumForName(GetName8(READ_INT32(ip+4)), TEXTYPE_Flat, true);
        //int Flat = GTextureManager.NumForName(GetName8(READ_INT32(ip+4)|ActiveObject->GetLibraryID()), TEXTYPE_Flat, true);
        int Flat = GTextureManager.FindOrLoadFullyNamedTextureAsMapTexture(GetStr(READ_INT32(ip+4)|ActiveObject->GetLibraryID()), nullptr, TEXTYPE_Flat, /*overload*/true);
        ip += 8;
        if (Flat > 0) { //???
          sector_t *sector;
          for (int Idx = FindSectorFromTag(sector, Tag); Idx >= 0; Idx = FindSectorFromTag(sector, Tag, Idx)) {
            sector->ceiling.pic = Flat;
          }
        }
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_Restart)
      ip = info->Address;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AndLogical)
      sp[-2] = sp[-2] && sp[-1];
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_OrLogical)
      sp[-2] = sp[-2] || sp[-1];
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AndBitwise)
      sp[-2] = sp[-2]&sp[-1];
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_OrBitwise)
      sp[-2] = sp[-2]|sp[-1];
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_EorBitwise)
      sp[-2] = sp[-2]^sp[-1];
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_NegateLogical)
      sp[-1] = !sp[-1];
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LShift)
      sp[-2] = sp[-2]<<sp[-1];
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_RShift)
      sp[-2] = sp[-2]>>sp[-1];
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_UnaryMinus)
      sp[-1] = -sp[-1];
      ACSVM_BREAK;

    ACSVM_CASE(PCD_IfNotGoto)
      if (!sp[-1]) {
        ip = ActiveObject->OffsetToPtr(READ_INT32(ip));
      } else {
        ip += 4;
      }
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LineSide)
      *sp = side;
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ScriptWait)
      WaitValue = sp[-1];
      {
        VAcsInfo *scpt = XLevel->Acs->FindScript(WaitValue, WaitObject);
        if (!/*XLevel->Acs->FindScript(WaitValue, WaitObject)*/scpt ||
            !/*XLevel->Acs->FindScript(WaitValue, WaitObject)*/scpt->RunningScript)
        {
          State = ASTE_WaitingForScriptStart;
        } else {
          State = ASTE_WaitingForScript;
        }
      }
      --sp;
      action = SCRIPT_Stop;
      if (activeFunction) {
        GCon->Logf(NAME_Error, "ACS script #%d (named '%s') tried to suspend inside a function; terminated", info->Number, *info->Name);
        action = SCRIPT_Terminate;
      }
      ACSVM_BREAK_STOP;

    ACSVM_CASE(PCD_ScriptWaitDirect)
      WaitValue = READ_INT32(ip);
      {
        VAcsInfo *scpt = XLevel->Acs->FindScript(WaitValue, WaitObject);
        if (!/*XLevel->Acs->FindScript(WaitValue, WaitObject)*/scpt ||
            !/*XLevel->Acs->FindScript(WaitValue, WaitObject)*/scpt->RunningScript)
        {
          State = ASTE_WaitingForScriptStart;
        } else {
          State = ASTE_WaitingForScript;
        }
      }
      ip += 4;
      action = SCRIPT_Stop;
      if (activeFunction) {
        GCon->Logf(NAME_Error, "ACS script #%d (named '%s') tried to suspend inside a function; terminated", info->Number, *info->Name);
        action = SCRIPT_Terminate;
      }
      ACSVM_BREAK_STOP;

    ACSVM_CASE(PCD_ScriptWaitNamed)
      {
        VName name = GetNameLowerCase(sp[-1]);
        --sp;
        //GCon->Logf(NAME_Warning, "UNTESTED ACS OPCODE PCD_ScriptWaitNamed (script '%s')", *name);
        WaitValue = XLevel->Acs->FindScriptNumberByName(*name, WaitObject);
        if (WaitValue != -1 /*<= -SPECIAL_LOW_SCRIPT_NUMBER*/) {
          VAcsInfo *scpt = XLevel->Acs->FindScript(WaitValue, WaitObject);
          if (!/*XLevel->Acs->FindScript(WaitValue, WaitObject)*/scpt ||
              !/*XLevel->Acs->FindScript(WaitValue, WaitObject)*/scpt->RunningScript)
          {
            State = ASTE_WaitingForScriptStart;
          } else {
            State = ASTE_WaitingForScript;
          }
        } else {
          GCon->Logf(NAME_Warning, "ACS: PCD_ScriptWaitNamed wanted to wait for unknown script '%s'", *name);
        }
        action = SCRIPT_Stop;
        if (activeFunction) {
          GCon->Logf(NAME_Error, "ACS script #%d (named '%s') tried to suspend inside a function; terminated", info->Number, *info->Name);
          action = SCRIPT_Terminate;
        }
      }
      ACSVM_BREAK_STOP;

    ACSVM_CASE(PCD_ClearLineSpecial)
      if (line) line->special = 0;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_CaseGoto)
      if (sp[-1] == READ_INT32(ip)) {
        ip = ActiveObject->OffsetToPtr(READ_INT32(ip+4));
        --sp;
      } else {
        ip += 8;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_BeginPrint)
      //GCon->Logf("ACS: BeginPrint (old=<%s>)", *PrintStr.quote());
      SB_PUSH;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_EndPrint)
      PrintStr = PrintStr.EvalEscapeSequences();
      if (Activator && Activator->IsPlayer()) {
        Activator->Player->CenterPrintf("%s", *PrintStr);
      } else {
        BroadcastCenterPrint(*PrintStr);
      }
      SB_POP;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PrintString)
      //GCon->Logf("ACS: PrintString: <%s> <%s>", *PrintStr.quote(), *GetStr(sp[-1]).quote());
      PrintStr += GetStr(sp[-1]);
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PrintNumber)
      PrintStr += VStr(sp[-1]);
      //GCon->Logf("ACS: PrintNumber: res=<%s> <%d>", *PrintStr.quote(), sp[-1]);
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PrintCharacter)
      PrintStr += (char)sp[-1];
      //GCon->Logf("ACS: PrintCharacter: res=<%s> <%d>", *PrintStr.quote(), sp[-1]);
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PlayerCount)
      sp[0] = 0;
      for (int i = 0; i < MAXPLAYERS; ++i) {
        if (Level->Game->Players[i]) ++sp[0];
      }
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GameType)
           if (GGameInfo->NetMode == NM_TitleMap) *sp = GAME_TITLE_MAP;
      else if (GGameInfo->NetMode == NM_Standalone) *sp = GAME_SINGLE_PLAYER;
      else if (svs.deathmatch) *sp = GAME_NET_DEATHMATCH;
      else *sp = GAME_NET_COOPERATIVE;
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GameSkill)
      *sp = Level->World->SkillAcsReturn;
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_Timer)
      *sp = XLevel->TicTime;
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SectorSound)
      Level->SectorStartSound((line ? line->frontsector : nullptr), GSoundManager->GetSoundID(GetName(sp[-2])), 0, sp[-1]/127.0f, 1.0f);
      sp -= 2;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AmbientSound)
      StartSound(TVec(0, 0, 0), 0, GSoundManager->GetSoundID(GetName(sp[-2])), 0, sp[-1]/127.0f, 0.0f, false);
      sp -= 2;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SoundSequence)
      Level->SectorStartSequence((line ? line->frontsector : nullptr), GetName(sp[-1]), 0);
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetLineTexture)
      {
        //int Tex = GTextureManager.NumForName(GetName8(sp[-1]), TEXTYPE_Wall, true);
        int Tex = GTextureManager.FindOrLoadFullyNamedTextureAsMapTexture(GetStr(sp[-1]), nullptr, TEXTYPE_Wall, /*overload*/true);
        if (Tex < 0) {
          GCon->Logf(NAME_Warning, "ACS: texture '%s' not found!", *GetStr(sp[-1]));
        } else {
          int side = sp[-3];
          int ttype = sp[-2];
          int searcher = -1;
          for (line_t *line = XLevel->FindLine(sp[-4], &searcher); line != nullptr; line = XLevel->FindLine(sp[-4], &searcher)) {
            if (line->sidenum[side] == -1) continue;
                 if (ttype == TEXTURE_MIDDLE) GLevel->Sides[line->sidenum[side]].MidTexture = Tex;
            else if (ttype == TEXTURE_BOTTOM) GLevel->Sides[line->sidenum[side]].BottomTexture = Tex;
            else if (ttype == TEXTURE_TOP) GLevel->Sides[line->sidenum[side]].TopTexture = Tex;
          }
        }
        sp -= 4;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetLineBlocking)
      {
        int searcher = -1;
        for (line_t *line = XLevel->FindLine(sp[-2], &searcher); line != nullptr; line = XLevel->FindLine(sp[-2], &searcher)) {
          switch (sp[-1]) {
            case BLOCK_NOTHING:
              line->flags &= ~(ML_BLOCKING|ML_BLOCKEVERYTHING|ML_RAILING|ML_BLOCKPLAYERS);
              break;
            case BLOCK_CREATURES:
            default:
              line->flags &= ~(ML_BLOCKEVERYTHING|ML_RAILING|ML_BLOCKPLAYERS);
              line->flags |= ML_BLOCKING;
              break;
            case BLOCK_EVERYTHING:
              line->flags &= ~(ML_RAILING|ML_BLOCKPLAYERS);
              line->flags |= ML_BLOCKING|ML_BLOCKEVERYTHING;
              break;
            case BLOCK_RAILING:
              line->flags &= ~(ML_BLOCKEVERYTHING|ML_BLOCKPLAYERS);
              line->flags |= ML_BLOCKING|ML_RAILING;
              break;
            case BLOCK_PLAYERS:
              line->flags &= ~(ML_BLOCKING|ML_BLOCKEVERYTHING|ML_RAILING);
              line->flags |= ML_BLOCKPLAYERS;
              break;
          }
        }
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetLineSpecial)
      {
        TranslateSpecial(sp[-6], sp[-5]);
        int searcher = -1;
        for (line_t *line = XLevel->FindLine(sp[-7], &searcher); line != nullptr; line = XLevel->FindLine(sp[-7], &searcher)) {
          line->special = sp[-6];
          line->arg1 = sp[-5];
          line->arg2 = sp[-4];
          line->arg3 = sp[-3];
          line->arg4 = sp[-2];
          line->arg5 = sp[-1];
          //GCon->Logf("line #%d got special #%d (%d,%d,%d,%d,%d)", (int)(ptrdiff_t)(line-XLevel->Lines), line->special, line->arg1, line->arg2, line->arg3, line->arg4, line->arg5);
        }
        sp -= 7;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ThingSound)
      {
        VName sound = GetName(sp[-2]);
        for (VEntity *mobj = Level->FindMobjFromTID(sp[-3], nullptr); mobj; mobj = Level->FindMobjFromTID(sp[-3], mobj)) {
          mobj->StartSound(sound, 0, sp[-1]/127.0f, 1.0f, false);
        }
        sp -= 3;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_EndPrintBold)
      PrintStr = PrintStr.EvalEscapeSequences();
      BroadcastCenterPrint(*(VStr(TEXT_COLOR_ESCAPE)+"+"+PrintStr));
      SB_POP;
      ACSVM_BREAK;

    //  Extended P-Code commands.
    ACSVM_CASE(PCD_ActivatorSound)
      if (Activator) {
        Activator->StartSound(GetName(sp[-2]), 0, sp[-1]/127.0f, 1.0f, false);
      } else {
        StartSound(TVec(0, 0, 0), 0, GSoundManager->GetSoundID(GetName(sp[-2])), 0, sp[-1]/127.0f, 1.0f, false);
      }
      sp -= 2;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LocalAmbientSound)
      if (Activator) Activator->StartLocalSound(GetName(sp[-2]), 0, sp[-1]/127.0f, 1.0f);
      sp -= 2;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetLineMonsterBlocking)
      {
        int searcher = -1;
        for (line_t *line = XLevel->FindLine(sp[-2], &searcher); line != nullptr; line = XLevel->FindLine(sp[-2], &searcher)) {
          if (sp[-1]) {
            line->flags |= ML_BLOCKMONSTERS;
          } else {
            line->flags &= ~ML_BLOCKMONSTERS;
          }
        }
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PlayerHealth)
      *sp = (Activator ? Activator->Health : 0);
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PlayerArmorPoints)
      *sp = (Activator ? Activator->eventGetArmorPoints() : 0);
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PlayerFrags)
      *sp = (Activator && Activator->Player ? Activator->Player->Frags : 0);
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PrintName)
      {
        VBasePlayer *Plr;
        switch (sp[-1]) {
          case PRINTNAME_LEVELNAME:
            PrintStr += GClLevel->LevelInfo->GetLevelName();
            break;
          case PRINTNAME_LEVEL:
            PrintStr += XLevel->MapName;
            break;
          case PRINTNAME_SKILL:
            PrintStr += Level->World->GetCurrSkillName();
            break;
          default:
            if (sp[-1] <= 0 || sp[-1] > MAXPLAYERS) {
              Plr = Activator ? Activator->Player : nullptr;
            } else {
              Plr = Level->Game->Players[sp[-1]-1];
            }
                 if (Plr && (Plr->PlayerFlags&VBasePlayer::PF_Spawned)) PrintStr += Plr->PlayerName;
            else if (Plr && !(Plr->PlayerFlags&VBasePlayer::PF_Spawned)) PrintStr += VStr("Player ")+VStr(sp[-1]);
            else if (Activator) PrintStr += Activator->GetClass()->GetName();
            else PrintStr += "Unknown";
            break;
        }
        --sp;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_MusicChange)
      Level->ChangeMusic(GetName8(sp[-2]));
      sp -= 2;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SinglePlayer)
      sp[-1] = GGameInfo->NetMode < NM_DedicatedServer;
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_FixedMul)
      sp[-2] = vint32((double)sp[-2]/(double)0x10000*(double)sp[-1]);
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_FixedDiv)
      sp[-2] = vint32((double)sp[-2]/(double)sp[-1]*(double)0x10000);
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetGravity)
      Level->Gravity = ((float)sp[-1]/(float)0x10000)*DEFAULT_GRAVITY/800.0f;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetGravityDirect)
      Level->Gravity = ((float)READ_INT32(ip)/(float)0x10000)*DEFAULT_GRAVITY/800.0f;
      ip += 4;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetAirControl)
      Level->AirControl = float(sp[-1])/65536.0f;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetAirControlDirect)
      Level->AirControl = float(READ_INT32(ip))/65536.0f;
      ip += 4;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ClearInventory)
      if (Activator) {
        Activator->eventClearInventory();
      } else {
        for (int i = 0; i < MAXPLAYERS; ++i) {
          if (Level->Game->Players[i] && Level->Game->Players[i]->PlayerFlags&VBasePlayer::PF_Spawned) {
            Level->Game->Players[i]->MO->eventClearInventory();
          }
        }
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GiveInventory)
      if (Activator) {
        //GCon->Logf(NAME_Debug, "PCD_GiveInventory: activator=<%s>; class=<%s>; count=%d", Activator->GetClass()->GetName(), *GetNameLowerCase(sp[-2]), sp[-1]);
        Activator->eventGiveInventory(GetNameLowerCase(sp[-2]), sp[-1], false); // disable replacement
      } else {
        //GCon->Logf(NAME_Debug, "PCD_GiveInventory: activator=<NONE>; class=<%s>; count=%d", *GetNameLowerCase(sp[-2]), sp[-1]);
        for (auto &&it : Level->Game->playersSpawned()) {
          it.player()->MO->eventGiveInventory(GetNameLowerCase(sp[-2]), sp[-1], false); // disable replacement
        }
      }
      sp -= 2;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GiveInventoryDirect)
      if (Activator) {
        //GCon->Logf(NAME_Debug, "PCD_GiveInventory: activator=<%s>; class=<%s>; count=%d", Activator->GetClass()->GetName(), *GetNameLowerCase(READ_INT32(ip)|ActiveObject->GetLibraryID()), READ_INT32(ip+4));
        Activator->eventGiveInventory(GetNameLowerCase(READ_INT32(ip)|ActiveObject->GetLibraryID()), READ_INT32(ip+4), false); // disable replacement
      } else {
        //GCon->Logf(NAME_Debug, "PCD_GiveInventory: activator=<NONE>; class=<%s>; count=%d", *GetNameLowerCase(READ_INT32(ip)|ActiveObject->GetLibraryID()), READ_INT32(ip+4));
        for (auto &&it : Level->Game->playersSpawned()) {
          it.player()->MO->eventGiveInventory(GetNameLowerCase(READ_INT32(ip)|ActiveObject->GetLibraryID()), READ_INT32(ip+4), false); // disable replacement
        }
      }
      ip += 8;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_TakeInventory)
      if (Activator) {
        Activator->eventTakeInventory(GetNameLowerCase(sp[-2]), sp[-1], false); // disable replacement
      } else {
        for (auto &&it : Level->Game->playersSpawned()) {
          it.player()->MO->eventTakeInventory(GetNameLowerCase(sp[-2]), sp[-1], false); // disable replacement
        }
      }
      sp -= 2;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_TakeInventoryDirect)
      if (Activator) {
        Activator->eventTakeInventory(GetNameLowerCase(READ_INT32(ip)|ActiveObject->GetLibraryID()), READ_INT32(ip+4), false); // disable replacement
      } else {
        for (auto &&it : Level->Game->playersSpawned()) {
          it.player()->MO->eventTakeInventory(GetNameLowerCase(READ_INT32(ip)|ActiveObject->GetLibraryID()), READ_INT32(ip+4), false); // disable replacement
        }
      }
      ip += 8;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_CheckInventory)
      if (!Activator) {
        sp[-1] = 0;
      } else {
        VName clsName = GetNameLowerCase(sp[-1]);
        sp[-1] = Activator->eventCheckInventory(clsName, false); // disable replacement
        //GCon->Logf(NAME_Debug, "PCD_CheckInventory: <%s> = %d", *clsName, sp[-1]);
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_CheckInventoryDirect)
      if (!Activator) {
        *sp = 0;
      } else {
        VName clsName = GetNameLowerCase(READ_INT32(ip)|ActiveObject->GetLibraryID());
        *sp = Activator->eventCheckInventory(clsName, false); // disable replacement
        //GCon->Logf(NAME_Debug, "PCD_CheckInventoryDirect: <%s> = %d", *clsName, *sp);
      }
      ++sp;
      ip += 4;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_Spawn)
      //GCon->Logf("!!!!!!!! '%s'", *GetNameLowerCase(sp[-6]));
      sp[-6] = Level->eventAcsSpawnThing(GetNameLowerCase(sp[-6]),
        TVec(float(sp[-5])/float(0x10000),
        float(sp[-4])/float(0x10000),
        float(sp[-3])/float(0x10000)),
        sp[-2], float(sp[-1])*45.0f/32.0f);
      sp -= 5;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SpawnDirect)
      *sp = Level->eventAcsSpawnThing(GetNameLowerCase(READ_INT32(ip)|ActiveObject->GetLibraryID()),
        TVec(float(READ_INT32(ip+4))/float(0x10000),
        float(READ_INT32(ip+8))/float(0x10000),
        float(READ_INT32(ip+12))/float(0x10000)),
        READ_INT32(ip+16), float(READ_INT32(ip+20))*45.0f/32.0f);
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SpawnSpot)
      sp[-4] = Level->eventAcsSpawnSpot(GetNameLowerCase(sp[-4]), sp[-3], sp[-2], float(sp[-1])*45.0f/32.0f);
      sp -= 3;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SpawnSpotDirect)
      *sp = Level->eventAcsSpawnSpot(GetNameLowerCase(READ_INT32(ip)|ActiveObject->GetLibraryID()),
        READ_INT32(ip+4), READ_INT32(ip+8),
        float(READ_INT32(ip+12))*45.0f/32.0f);
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetMusic)
      Level->ChangeMusic(GetName8(sp[-3]));
      sp -= 3;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetMusicDirect)
      Level->ChangeMusic(GetName8(READ_INT32(ip)|ActiveObject->GetLibraryID()));
      ip += 12;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LocalSetMusic)
      if (Activator && Activator->IsPlayer() && Activator->Player) {
        Activator->Player->eventClientChangeMusic(GetNameLowerCase(sp[-3]));
      }
      sp -= 3;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LocalSetMusicDirect)
      if (Activator && Activator->IsPlayer() && Activator->Player) {
        Activator->Player->eventClientChangeMusic(GetNameLowerCase(READ_INT32(ip)|ActiveObject->GetLibraryID()));
      }
      ip += 12;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PrintFixed)
      PrintStr += VStr(float(sp[-1])/float(0x10000));
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PrintLocalised)
      PrintStr += GLanguage[GetName(sp[-1])];
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_MoreHudMessage)
      PrintStr = PrintStr.EvalEscapeSequences();
      optstart = nullptr;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_OptHudMessage)
      optstart = sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_EndHudMessage)
    ACSVM_CASE(PCD_EndHudMessageBold)
      if (!optstart) optstart = sp;
      {
        int Type = optstart[-6];
        int Id = optstart[-5];
        int Color = optstart[-4];
        VStr ColorName;
        if (Type&HUDMSG_COLORSTRING) {
          ColorName = GetStr(optstart[-4]);
          Color = -1;
        }
        float x = (float)optstart[-3]/float(0x10000);
        float y = (float)optstart[-2]/float(0x10000);
        float HoldTime = (float)optstart[-1]/float(0x10000);
        float Time1 = 0;
        float Time2 = 0;
        switch (Type&0xff) {
          case HUDMSG_PLAIN:
            if (HoldTime < 1.0f/35.0f) HoldTime = 0; // gozzo does this
            break;
          case HUDMSG_FADEOUT:
            Time1 = (optstart < sp ? (float)optstart[0]/float(0x10000) : 0.5f);
            break;
          case HUDMSG_TYPEON:
            Time1 = (optstart < sp ? (float)optstart[0]/float(0x10000) : 0.05f);
            Time2 = (optstart < sp-1 ? (float)optstart[1]/float(0x10000) : 0.5f);
            break;
          case HUDMSG_FADEINOUT:
            Time1 = (optstart < sp ? (float)optstart[0]/float(0x10000) : 0.5f);
            Time2 = (optstart < sp-1 ? (float)optstart[1]/float(0x10000) : 0.5f);
            break;
        }
        // normalize timings
        HoldTime = NormHudTime(HoldTime, DelayTime);
        Time1 = NormHudTime(Time1, DelayTime);
        Time2 = NormHudTime(Time2, DelayTime);
        /*
        GCon->Logf("VAcs::RunScript: self name is '%s' (number is %d)", *info->Name, info->Number);
        GCon->Logf("  HUDMSG(id=%d): ht=%f (0x%04x); t1=%f; t2=%f; type=0x%02x, msg=%s", Id, HoldTime, optstart[-1], Time1, Time2, Type, *PrintStr.quote());
        */

        if (cmd != PCD_EndHudMessageBold && Activator && Activator->IsPlayer()) {
          if (Activator->Player) {
            Activator->Player->eventClientHudMessage(PrintStr, Font,
              Type, Id, Color, ColorName, x, y, HudWidth,
              HudHeight, HoldTime, Time1, Time2);
          }
        } else {
          for (auto &&it : Level->Game->playersSpawned()) {
            it.player()->eventClientHudMessage(PrintStr, Font, Type, Id, Color, ColorName,
                                               x, y, HudWidth, HudHeight, HoldTime, Time1, Time2);
          }
        }
        sp = optstart-6;
      }
      SB_POP;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetFont)
      Font = GetNameLowerCase(sp[-1]);
      //GCon->Logf(NAME_Debug, "SETFONTI: '%s'", *Font);
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetFontDirect)
      Font = GetNameLowerCase(READ_INT32(ip)|ActiveObject->GetLibraryID());
      //GCon->Logf(NAME_Debug, "SETFONTD: '%s'", *Font);
      ip += 4;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PushByte)
      *sp = *ip;
      ++sp;
      ++ip;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSpec1DirectB)
      Level->eventExecuteActionSpecial(ip[0], ip[1], 0, 0, 0, 0, line, side, Activator);
      ip += 2;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSpec2DirectB)
      Level->eventExecuteActionSpecial(ip[0], ip[1], ip[2], 0, 0, 0, line, side, Activator);
      ip += 3;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSpec3DirectB)
      Level->eventExecuteActionSpecial(ip[0], ip[1], ip[2], ip[3], 0, 0, line, side, Activator);
      ip += 4;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSpec4DirectB)
      Level->eventExecuteActionSpecial(ip[0], ip[1], ip[2], ip[3], ip[4], 0, line, side, Activator);
      ip += 5;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSpec5DirectB)
      Level->eventExecuteActionSpecial(ip[0], ip[1], ip[2], ip[3], ip[4], ip[5], line, side, Activator);
      ip += 6;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_DelayDirectB)
      PERFORM_DELAY(*ip)
      ++ip;
      ACSVM_BREAK_STOP;

    ACSVM_CASE(PCD_RandomDirectB)
      *sp = ip[0]+(vint32)(Random()*(ip[1]-ip[0]+1));
      ip += 2;
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PushBytes)
      for (int i = 0; i < ip[0]; ++i) sp[i] = ip[i+1];
      sp += ip[0];
      ip += ip[0]+1;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_Push2Bytes)
      sp[0] = ip[0];
      sp[1] = ip[1];
      ip += 2;
      sp += 2;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_Push3Bytes)
      sp[0] = ip[0];
      sp[1] = ip[1];
      sp[2] = ip[2];
      ip += 3;
      sp += 3;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_Push4Bytes)
      sp[0] = ip[0];
      sp[1] = ip[1];
      sp[2] = ip[2];
      sp[3] = ip[3];
      ip += 4;
      sp += 4;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_Push5Bytes)
      sp[0] = ip[0];
      sp[1] = ip[1];
      sp[2] = ip[2];
      sp[3] = ip[3];
      sp[4] = ip[4];
      ip += 5;
      sp += 5;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetThingSpecial)
      {
        TranslateSpecial(sp[-6], sp[-5]);
        if (sp[-7] != 0) {
          for (VEntity *Ent = Level->FindMobjFromTID(sp[-7], nullptr); Ent; Ent = Level->FindMobjFromTID(sp[-7], Ent)) {
            Ent->Special = sp[-6];
            Ent->Args[0] = sp[-5];
            Ent->Args[1] = sp[-4];
            Ent->Args[2] = sp[-3];
            Ent->Args[3] = sp[-2];
            Ent->Args[4] = sp[-1];
          }
        } else if (Activator) {
          Activator->Special = sp[-6];
          Activator->Args[0] = sp[-5];
          Activator->Args[1] = sp[-4];
          Activator->Args[2] = sp[-3];
          Activator->Args[3] = sp[-2];
          Activator->Args[4] = sp[-1];
        }
        sp -= 7;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AssignGlobalVar)
      //GlobalVars[READ_BYTE_OR_INT32] = sp[-1];
      globals->SetGlobalVarInt(READ_BYTE_OR_INT32, sp[-1]);
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PushGlobalVar)
      //*sp = GlobalVars[READ_BYTE_OR_INT32];
      *sp = globals->GetGlobalVarInt(READ_BYTE_OR_INT32);
      INC_BYTE_OR_INT32;
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AddGlobalVar)
      //GlobalVars[READ_BYTE_OR_INT32] += sp[-1];
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetGlobalVarInt(vidx, globals->GetGlobalVarInt(vidx)+sp[-1]);
      }
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SubGlobalVar)
      //GlobalVars[READ_BYTE_OR_INT32] -= sp[-1];
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetGlobalVarInt(vidx, globals->GetGlobalVarInt(vidx)-sp[-1]);
      }
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_MulGlobalVar)
      //GlobalVars[READ_BYTE_OR_INT32] *= sp[-1];
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetGlobalVarInt(vidx, globals->GetGlobalVarInt(vidx)*sp[-1]);
      }
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_DivGlobalVar)
      if (sp[-1] == 0) Host_Error("ACS: division by zero in 'DivGlobalVar'");
      //GlobalVars[READ_BYTE_OR_INT32] /= sp[-1];
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetGlobalVarInt(vidx, globals->GetGlobalVarInt(vidx)/sp[-1]);
      }
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ModGlobalVar)
      if (sp[-1] == 0) Host_Error("ACS: division by zero in 'ModGlobalVar'");
      //GlobalVars[READ_BYTE_OR_INT32] %= sp[-1];
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetGlobalVarInt(vidx, globals->GetGlobalVarInt(vidx)%sp[-1]);
      }
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_IncGlobalVar)
      //GlobalVars[READ_BYTE_OR_INT32]++;
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetGlobalVarInt(vidx, globals->GetGlobalVarInt(vidx)+1);
      }
      INC_BYTE_OR_INT32;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_DecGlobalVar)
      //GlobalVars[READ_BYTE_OR_INT32]--;
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetGlobalVarInt(vidx, globals->GetGlobalVarInt(vidx)-1);
      }
      INC_BYTE_OR_INT32;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_FadeTo)
      Level->eventAcsFadeRange(0, 0, 0, -1, (float)sp[-5]/255.0f,
        (float)sp[-4]/255.0f, (float)sp[-3]/255.0f,
        (float)sp[-2]/65536.0f, (float)sp[-1]/65536.0f, Activator);
      sp -= 5;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_FadeRange)
      Level->eventAcsFadeRange((float)sp[-9]/255.0f,
        (float)sp[-8]/255.0f, (float)sp[-7]/255.0f,
        (float)sp[-6]/65536.0f, (float)sp[-5]/255.0f,
        (float)sp[-4]/255.0f, (float)sp[-3]/255.0f,
        (float)sp[-2]/65536.0f, (float)sp[-1]/65536.0f, Activator);
      sp -= 9;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_CancelFade)
      Level->eventAcsCancelFade(Activator);
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PlayMovie)
      STUB(PCD_PlayMovie)
      //FIXME implement this
      //sp[-1] - movie name, string
      //Pushes result
      sp[-1] = 0;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetFloorTrigger)
      Level->eventStartPlaneWatcher(Activator, line, side, false,
        sp[-8], sp[-7], sp[-6], sp[-5], sp[-4], sp[-3], sp[-2],
        sp[-1]);
      sp -= 8;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetCeilingTrigger)
      Level->eventStartPlaneWatcher(Activator, line, side, true,
        sp[-8], sp[-7], sp[-6], sp[-5], sp[-4], sp[-3], sp[-2],
        sp[-1]);
      sp -= 8;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GetActorX)
      {
        VEntity *Ent = EntityFromTID(sp[-1], Activator);
        sp[-1] = (Ent ? vint32(Ent->Origin.x*0x10000) : 0);
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GetActorY)
      {
        VEntity *Ent = EntityFromTID(sp[-1], Activator);
        sp[-1] = (Ent ? vint32(Ent->Origin.y*0x10000) : 0);
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GetActorZ)
      {
        VEntity *Ent = EntityFromTID(sp[-1], Activator);
        sp[-1] = (Ent ? vint32(Ent->Origin.z*0x10000) : 0);
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_StartTranslation)
      if (sp[-1] >= 1 && sp[-1] <= MAX_LEVEL_TRANSLATIONS) {
        while (XLevel->Translations.Num() < sp[-1]) {
          XLevel->Translations.Append(nullptr);
        }
        Translation = XLevel->Translations[sp[-1]-1];
        if (!Translation) {
          Translation = new VTextureTranslation;
          XLevel->Translations[sp[-1]-1] = Translation;
        } else {
          Translation->Clear();
        }
      }
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_TranslationRange1)
      if (Translation) Translation->MapToRange(sp[-4], sp[-3], sp[-2], sp[-1]);
      sp -= 4;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_TranslationRange2)
      if (Translation) Translation->MapToColors(sp[-8], sp[-7], sp[-6], sp[-5], sp[-4], sp[-3], sp[-2], sp[-1]);
      sp -= 8;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_EndTranslation)
      // nothing to do here
      ACSVM_BREAK;

    ACSVM_CASE(PCD_Call)
    ACSVM_CASE(PCD_CallDiscard)
    ACSVM_CASE(PCD_CallStack)
      {
        VAcsObject *object = ActiveObject;
        int funcnum;
        if (cmd != PCD_CallStack) {
          funcnum = READ_BYTE_OR_INT32;
          INC_BYTE_OR_INT32;
          if (funcnum < 0 || funcnum > 0xffff) Host_Error("ACS tried to call a function with invalid index");
        } else {
          funcnum = sp[-1];
          --sp;
          object = ActiveObject->Level->GetObject((funcnum>>16)&0xffff);
          if (!object) Host_Error("ACS tried to indirectly call a function from inexisting object");
        }
        VAcsFunction *func = /*ActiveObject*/object->GetFunction(funcnum&0xffff, object);
        if (!func) {
          GCon->Logf(NAME_Warning, "ACS: Function %d in script %d out of range", funcnum, number);
          action = SCRIPT_Terminate;
          ACSVM_BREAK_STOP;
        }
        if ((sp-mystack)+func->LocalCount+128 >= ACS_STACK_DEPTH) {
          // 128 is the margin for the function's working space
          GCon->Logf(NAME_Error, "ACS: Out of stack space in script %d (%d slots missing)", number, (int)(ptrdiff_t)(sp-mystack)+func->LocalCount+128-ACS_STACK_DEPTH);
          action = SCRIPT_Terminate;
          //ACSVM_BREAK_STOP;
          Host_Error("ACS: Out of stack space in script %d (%d slots missing)", number, (int)(ptrdiff_t)(sp-mystack)+func->LocalCount+128-ACS_STACK_DEPTH);
        }
        vint32 *oldlocals = locals;
        //VACSLocalArrays *oldarrays = localarrays;
        // the function's first argument is also its first local variable
        locals = sp-func->ArgCount;
        localarrays = &func->LocalArrays;
        vassert(localarrays);
        // make space on the stack for any other variables the function uses
        if (func->LocalCount > 0) {
          memset((void *)sp, 0, func->LocalCount*sizeof(sp[0]));
          sp += func->LocalCount;
        }
        // create return frame
        VAcsCallReturn *rf = (VAcsCallReturn *)sp;
        rf->ReturnAddress = ActiveObject->PtrToOffset(ip);
        rf->ReturnFunction = activeFunction;
        rf->ReturnObject = ActiveObject;
        rf->ReturnLocals = oldlocals;
        //rf->ReturnArrays = oldarrays;
        rf->bDiscardResult = (cmd == PCD_CallDiscard);
        rf->PrevFrame = currRetFrame;
        sp += GetRetStructStackSize();
        currRetFrame = rf;
        ActiveObject = object;
        fmt = ActiveObject->GetFormat();
        ip = ActiveObject->OffsetToPtr(func->Address);
        activeFunction = func;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_CallFunc)
      {
        int argCount = READ_BYTE_OR_INT32; INC_BYTE_OR_INT32;
        int funcIndex = READ_SHORT_OR_INT32; INC_SHORT_OR_INT32;
        int retval = CallFunction(argCount, funcIndex, sp-argCount);
        sp -= argCount-1;
        sp[-1] = retval;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ReturnVoid)
    ACSVM_CASE(PCD_ReturnVal)
      {
        int value;

        vassert(currRetFrame);

        // get return value
        if (cmd == PCD_ReturnVal) {
          value = sp[-1];
          --sp;
        } else {
          value = 0;
        }

        // get return state
        //sp -= GetRetStructStackSize();
        //VAcsCallReturn *retState = (VAcsCallReturn *)sp;
        vassert((const vuint8 *)sp >= (const vuint8 *)currRetFrame);
        VAcsCallReturn *retState = currRetFrame;
        sp = (vint32 *)retState;

        // remove locals and arguments
        sp -= activeFunction->ArgCount+activeFunction->LocalCount;
        vassert(sp == locals);

        ActiveObject = retState->ReturnObject;
        activeFunction = retState->ReturnFunction;
        ip = ActiveObject->OffsetToPtr(retState->ReturnAddress);
        fmt = ActiveObject->GetFormat();
        vassert((activeFunction ? (sp >= retState->ReturnLocals) : (sp >= mystack)));
        locals = retState->ReturnLocals;
        //localarrays = retState->ReturnArrays;
        localarrays = (activeFunction ? &activeFunction->LocalArrays : &info->LocalArrays);
        currRetFrame = retState->PrevFrame;

        if (!retState->bDiscardResult) {
          *sp = value;
          ++sp;
        }
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PushMapArray)
      sp[-1] = ActiveObject->GetArrayVal(*ActiveObject->MapVars[READ_BYTE_OR_INT32], sp[-1]);
      INC_BYTE_OR_INT32;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AssignMapArray)
      ActiveObject->SetArrayVal(*ActiveObject->MapVars[READ_BYTE_OR_INT32], sp[-2], sp[-1]);
      INC_BYTE_OR_INT32;
      sp -= 2;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AddMapArray)
      {
        int ANum = *ActiveObject->MapVars[READ_BYTE_OR_INT32];
        ActiveObject->SetArrayVal(ANum, sp[-2], ActiveObject->GetArrayVal(ANum, sp[-2])+sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SubMapArray)
      {
        int ANum = *ActiveObject->MapVars[READ_BYTE_OR_INT32];
        ActiveObject->SetArrayVal(ANum, sp[-2], ActiveObject->GetArrayVal(ANum, sp[-2])-sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_MulMapArray)
      {
        int ANum = *ActiveObject->MapVars[READ_BYTE_OR_INT32];
        ActiveObject->SetArrayVal(ANum, sp[-2], ActiveObject->GetArrayVal(ANum, sp[-2])*sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_DivMapArray)
      {
        if (sp[-1] == 0) Host_Error("ACS: division by zero in `DivMapArray`");
        int ANum = *ActiveObject->MapVars[READ_BYTE_OR_INT32];
        ActiveObject->SetArrayVal(ANum, sp[-2], ActiveObject->GetArrayVal(ANum, sp[-2])/sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ModMapArray)
      {
        if (sp[-1] == 0) Host_Error("ACS: division by zero in `ModMapArray`");
        int ANum = *ActiveObject->MapVars[READ_BYTE_OR_INT32];
        ActiveObject->SetArrayVal(ANum, sp[-2], ActiveObject->GetArrayVal(ANum, sp[-2])%sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_IncMapArray)
      {
        int ANum = *ActiveObject->MapVars[READ_BYTE_OR_INT32];
        ActiveObject->SetArrayVal(ANum, sp[-1], ActiveObject->GetArrayVal(ANum, sp[-1])+1);
        INC_BYTE_OR_INT32;
        --sp;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_DecMapArray)
      {
        int ANum = *ActiveObject->MapVars[READ_BYTE_OR_INT32];
        ActiveObject->SetArrayVal(ANum, sp[-1], ActiveObject->GetArrayVal(ANum, sp[-1])-1);
        INC_BYTE_OR_INT32;
        --sp;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_Dup)
      *sp = sp[-1];
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_Swap)
      {
        int tmp = sp[-2];
        sp[-2] = sp[-1];
        sp[-1] = tmp;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_Sin)
      sp[-1] = vint32(msin(float(sp[-1])*360.0f/0x10000)*0x10000);
      ACSVM_BREAK;

    ACSVM_CASE(PCD_Cos)
      sp[-1] = vint32(mcos(float(sp[-1])*360.0f/0x10000)*0x10000);
      ACSVM_BREAK;

    ACSVM_CASE(PCD_VectorAngle)
      sp[-2] = vint32(matan(float(sp[-1])/float(0x10000),
        float(sp[-2])/float(0x10000))/360.0f*0x10000);
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_CheckWeapon)
      sp[-1] = (Activator ? Activator->eventCheckNamedWeapon(GetNameLowerCase(sp[-1])) : 0);
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetWeapon)
      //GCon->Logf("SETWEAPON: '%s'", *GetNameLowerCase(sp[-1]));
      sp[-1] = (Activator ? Activator->eventSetNamedWeapon(GetNameLowerCase(sp[-1])) : 0);
      ACSVM_BREAK;

    ACSVM_CASE(PCD_TagString)
      //sp[-1] |= ActiveObject->GetLibraryID();
      //GCon->Logf("PCD_TagString: <%s> (0x%08x)", *ActiveObject->GetString(sp[-1]).quote(), (unsigned)sp[-1]);
      sp[-1] = ActiveObject->Level->PutNewString(ActiveObject->GetString(sp[-1]));
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PushWorldArray)
      //sp[-1] = WorldArrays[READ_BYTE_OR_INT32].GetElemVal(sp[-1]);
      sp[-1] = globals->GetWorldArrayInt(READ_BYTE_OR_INT32, sp[-1]);
      INC_BYTE_OR_INT32;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AssignWorldArray)
      //WorldArrays[READ_BYTE_OR_INT32].SetElemVal(sp[-2], sp[-1]);
      globals->SetWorldArrayInt(READ_BYTE_OR_INT32, sp[-2], sp[-1]);
      INC_BYTE_OR_INT32;
      sp -= 2;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AddWorldArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        //WorldArrays[ANum].SetElemVal(sp[-2], WorldArrays[ANum].GetElemVal(sp[-2]) + sp[-1]);
        globals->SetWorldArrayInt(ANum, sp[-2], globals->GetWorldArrayInt(ANum, sp[-2])+sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SubWorldArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        //WorldArrays[ANum].SetElemVal(sp[-2], WorldArrays[ANum].GetElemVal(sp[-2]) - sp[-1]);
        globals->SetWorldArrayInt(ANum, sp[-2], globals->GetWorldArrayInt(ANum, sp[-2])-sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_MulWorldArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        //WorldArrays[ANum].SetElemVal(sp[-2], WorldArrays[ANum].GetElemVal(sp[-2]) * sp[-1]);
        globals->SetWorldArrayInt(ANum, sp[-2], globals->GetWorldArrayInt(ANum, sp[-2])*sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_DivWorldArray)
      {
        if (sp[-1] == 0) Host_Error("ACS: division by zero in 'DivWorldArray'");
        int ANum = READ_BYTE_OR_INT32;
        //WorldArrays[ANum].SetElemVal(sp[-2], WorldArrays[ANum].GetElemVal(sp[-2]) / sp[-1]);
        globals->SetWorldArrayInt(ANum, sp[-2], globals->GetWorldArrayInt(ANum, sp[-2])/sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ModWorldArray)
      {
        if (sp[-1] == 0) Host_Error("ACS: division by zero in 'ModWorldArray'");
        int ANum = READ_BYTE_OR_INT32;
        //WorldArrays[ANum].SetElemVal(sp[-2], WorldArrays[ANum].GetElemVal(sp[-2]) % sp[-1]);
        globals->SetWorldArrayInt(ANum, sp[-2], globals->GetWorldArrayInt(ANum, sp[-2])%sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_IncWorldArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        //WorldArrays[ANum].SetElemVal(sp[-1], WorldArrays[ANum].GetElemVal(sp[-1]) + 1);
        globals->SetWorldArrayInt(ANum, sp[-1], globals->GetWorldArrayInt(ANum, sp[-1])+1);
        INC_BYTE_OR_INT32;
        --sp;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_DecWorldArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        //WorldArrays[ANum].SetElemVal(sp[-1], WorldArrays[ANum].GetElemVal(sp[-1]) - 1);
        globals->SetWorldArrayInt(ANum, sp[-1], globals->GetWorldArrayInt(ANum, sp[-1])-1);
        INC_BYTE_OR_INT32;
        --sp;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PushGlobalArray)
      //sp[-1] = GlobalArrays[READ_BYTE_OR_INT32].GetElemVal(sp[-1]);
      sp[-1] = globals->GetGlobalArrayInt(READ_BYTE_OR_INT32, sp[-1]);
      INC_BYTE_OR_INT32;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AssignGlobalArray)
#ifdef ACS_DUMP_EXECUTION
      //GCon->Logf("ACS:  AssignGlobalArray[%d] (%d, %d)", READ_BYTE_OR_INT32, sp[-2], sp[-1]);
#endif
      //GlobalArrays[READ_BYTE_OR_INT32].SetElemVal(sp[-2], sp[-1]);
      globals->SetGlobalArrayInt(READ_BYTE_OR_INT32, sp[-2], sp[-1]);
      INC_BYTE_OR_INT32;
      sp -= 2;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AddGlobalArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        //GlobalArrays[ANum].SetElemVal(sp[-2], GlobalArrays[ANum].GetElemVal(sp[-2])+sp[-1]);
        globals->SetGlobalArrayInt(ANum, sp[-2], globals->GetGlobalArrayInt(ANum, sp[-2])+sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SubGlobalArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        //GlobalArrays[ANum].SetElemVal(sp[-2], GlobalArrays[ANum].GetElemVal(sp[-2])-sp[-1]);
        globals->SetGlobalArrayInt(ANum, sp[-2], globals->GetGlobalArrayInt(ANum, sp[-2])-sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_MulGlobalArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        //GlobalArrays[ANum].SetElemVal(sp[-2], GlobalArrays[ANum].GetElemVal(sp[-2])*sp[-1]);
        globals->SetGlobalArrayInt(ANum, sp[-2], globals->GetGlobalArrayInt(ANum, sp[-2])*sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_DivGlobalArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        if (sp[-1] == 0) Host_Error("ACS: division by zero in `DivGlobalArray`");
        //GlobalArrays[ANum].SetElemVal(sp[-2], GlobalArrays[ANum].GetElemVal(sp[-2])/sp[-1]);
        globals->SetGlobalArrayInt(ANum, sp[-2], globals->GetGlobalArrayInt(ANum, sp[-2])/sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ModGlobalArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        if (sp[-1] == 0) Host_Error("ACS: division by zero in `ModGlobalArray`");
        //GlobalArrays[ANum].SetElemVal(sp[-2], GlobalArrays[ANum].GetElemVal(sp[-2])%sp[-1]);
        globals->SetGlobalArrayInt(ANum, sp[-2], globals->GetGlobalArrayInt(ANum, sp[-2])%sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_IncGlobalArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        //GlobalArrays[ANum].SetElemVal(sp[-1], GlobalArrays[ANum].GetElemVal(sp[-1])+1);
        globals->SetGlobalArrayInt(ANum, sp[-1], globals->GetGlobalArrayInt(ANum, sp[-1])+1);
        INC_BYTE_OR_INT32;
        --sp;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_DecGlobalArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        //GlobalArrays[ANum].SetElemVal(sp[-1], GlobalArrays[ANum].GetElemVal(sp[-1])-1);
        globals->SetGlobalArrayInt(ANum, sp[-1], globals->GetGlobalArrayInt(ANum, sp[-1])-1);
        INC_BYTE_OR_INT32;
        --sp;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetMarineWeapon)
      Level->eventSetMarineWeapon(sp[-2], sp[-1], Activator);
      sp -= 2;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetActorProperty)
      if (!sp[-3]) {
        if (Activator) {
          Activator->eventSetActorProperty(sp[-2], sp[-1], GetStr(sp[-1]));
          //if (developer) GCon->Logf(NAME_Dev, "SetActorProperty: ent=<%s>, propid=%d", Activator->GetClass()->GetName(), sp[-2]);
        }
      } else {
        for (VEntity *Ent = Level->FindMobjFromTID(sp[-3], nullptr); Ent; Ent = Level->FindMobjFromTID(sp[-3], Ent)) {
          Ent->eventSetActorProperty(sp[-2], sp[-1], GetStr(sp[-1]));
          //if (developer) GCon->Logf(NAME_Dev, "SetActorProperty: ent=<%s>, propid=%d", Ent->GetClass()->GetName(), sp[-2]);
        }
      }
      sp -= 3;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GetActorProperty)
      {
        VEntity *Ent = EntityFromTID(sp[-2], Activator);
        if (!Ent) {
          sp[-2] = 0;
        } else {
          //FIXME
          //if (developer) GCon->Logf(NAME_Dev, "GetActorProperty: ent=<%s>, propid=%d", Ent->GetClass()->GetName(), sp[-1]);
          sp[-2] = Ent->eventGetActorProperty(sp[-1]);
          // convert special properties
          switch (sp[-1]) {
            case 20: //APROP_Species
            case 21: //APROP_NameTag
              sp[-2] = ActiveObject->Level->PutNewString(*VName(EName(sp[-2])));
              break;
          }
        }
      }
      sp -= 1;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PlayerNumber)
      *sp = (Activator && Activator->IsPlayer() ? SV_GetPlayerNum(Activator->Player) : -1);
      //GCon->Logf("PCD_PlayerNumber: %d", *sp);
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ActivatorTID)
      *sp = Activator ? Activator->TID : 0;
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetMarineSprite)
      Level->eventSetMarineSprite(sp[-2], GetName(sp[-1]), Activator);
      sp -= 2;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GetScreenWidth)
#ifdef CLIENT
      *sp++ = VirtualWidth;
#else
      *sp++ = 640;
#endif
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GetScreenHeight)
#ifdef CLIENT
      *sp++ = VirtualHeight;
#else
      *sp++ = 480;
#endif
      ACSVM_BREAK;

    // Thing_Projectile2 (tid, type, angle, speed, vspeed, gravity, newtid);
    ACSVM_CASE(PCD_ThingProjectile2)
      Level->eventEV_ThingProjectile(sp[-7], sp[-6], sp[-5], sp[-4],
        sp[-3], sp[-2], sp[-1], NAME_None, Activator);
      sp -= 7;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_StrLen)
      sp[-1] = GetStr(sp[-1]).Utf8Length();
      ACSVM_BREAK;

    // void SetHudSize (int width, int height, bool statusbar);
    ACSVM_CASE(PCD_SetHudSize)
      HudWidth = (sp[-3] > 0 ? sp[-3] : 0);
      HudHeight = (sp[-2] > 0 ? sp[-2] : 0);
      //GCon->Logf("ACS: SetHudSize: (%d,%d)", HudWidth, HudHeight);
      if (sp[-1]) HudHeight = -HudHeight;
      sp -= 3;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GetCvar)
      {
        VStr cvname = GetStr(sp[-1]);
        int val;
             if (cvname.strEquCI("screenblocks")) { val = acs_screenblocks_override; if (val < 0) val = VCvar::GetInt(*cvname); }
        else if (cvname.strEquCI("vid_defwidth")) { /*val = (int)(VirtualWidth*65536.0f);*/ val = (int)(640.0f*65536.0f); }
        else if (cvname.strEquCI("vid_defheight")) { /*val = (int)(VirtualHeight*65536.0f);*/ val = (int)(480.0f*65536.0f); }
        else if (cvname.strEquCI("vid_aspect")) { val = 0; }
        else if (cvname.strEquCI("vid_nowidescreen")) { val = 1; }
        else if (cvname.strEquCI("tft")) { val = 0; }
        else if (cvname.strEquCI("m_yaw")) { val = (int)(m_yaw.asFloat()*65536.0f); }
        else if (cvname.strEquCI("m_pitch")) { val = (int)(m_pitch.asFloat()*65536.0f); }
        else if (cvname.strEquCI("mouse_sensitivity")) { val = (int)(max2(mouse_x_sensitivity.asFloat(), mouse_y_sensitivity.asFloat())*65536.0f); }
        else {
          VCvar *vc = VCvar::FindVariable(*cvname);
          if (vc) {
            val = (vc->GetType() == VCvar::Float ? int(vc->asFloat()*65536.0f) : vc->asInt());
          } else {
            val = 0;
          }
        }
        //GCon->Logf(NAME_Debug, "ACS: GetCvar(%s)=%d", *cvname, val);
        sp[-1] = val;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_CaseGotoSorted)
      //  The count and jump table are 4-byte aligned.
      if (ActiveObject->PtrToOffset(ip)&3)
      {
        ip += 4-(ActiveObject->PtrToOffset(ip)&3);
      }
      {
        int numcases = READ_INT32(ip);
        int min = 0, max = numcases-1;
        while (min <= max)
        {
          int mid = (min+max)/2;
          int caseval = READ_INT32(ip+4+mid*8);
          if (caseval == sp[-1])
          {
            ip = ActiveObject->OffsetToPtr(READ_INT32(ip+8+mid*8));
            --sp;
            ACSVM_BREAK;
          }
          else if (caseval < sp[-1])
          {
            min = mid+1;
          }
          else
          {
            max = mid-1;
          }
        }
        if (min > max)
        {
          // The case was not found, so go to the next instruction.
          ip += 4+numcases*8;
        }
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetResultValue)
      resultValue = sp[-1];
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GetLineRowOffset)
      *sp = line ? (vint32)XLevel->Sides[line->sidenum[0]].Mid.RowOffset : 0;
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GetActorFloorZ)
      {
        VEntity *Ent = EntityFromTID(sp[-1], Activator);
        sp[-1] = Ent ? vint32(Ent->FloorZ*0x10000) : 0;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GetActorAngle)
      {
        VEntity *Ent = EntityFromTID(sp[-1], Activator);
        sp[-1] = Ent ? vint32(Ent->Angles.yaw*0x10000/360) &
          0xffff : 0;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GetSectorFloorZ)
      {
        sector_t *sector;
        int SNum = FindSectorFromTag(sector, sp[-3]);
        sp[-3] = (SNum >= 0 ? vint32(/*XLevel->Sectors[SNum].*/sector->floor.
          GetPointZClamped(sp[-2], sp[-1])*0x10000) : 0);
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GetSectorCeilingZ)
      {
        sector_t *sector;
        int SNum = FindSectorFromTag(sector, sp[-3]);
        sp[-3] = (SNum >= 0 ? vint32(/*XLevel->Sectors[SNum].*/sector->ceiling.
          GetPointZClamped(sp[-2], sp[-1])*0x10000) : 0);
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSpec5Result)
      sp[-5] = Level->eventExecuteActionSpecial(READ_BYTE_OR_INT32,
        sp[-5], sp[-4], sp[-3], sp[-2], sp[-1], line, side,
        Activator);
      INC_BYTE_OR_INT32;
      sp -= 4;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GetSigilPieces)
      *sp = Activator ? Activator->eventGetSigilPieces() : 0;
      ++sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GetLevelInfo)
      switch (sp[-1]) {
        case LEVELINFO_PAR_TIME: sp[-1] = Level->ParTime; break;
        case LEVELINFO_SUCK_TIME: sp[-1] = Level->SuckTime; break;
        case LEVELINFO_CLUSTERNUM: sp[-1] = Level->Cluster; break;
        case LEVELINFO_LEVELNUM: sp[-1] = Level->LevelNum; break;
        case LEVELINFO_TOTAL_SECRETS: sp[-1] = Level->TotalSecret; break;
        case LEVELINFO_FOUND_SECRETS: sp[-1] = Level->CurrentSecret; break;
        case LEVELINFO_TOTAL_ITEMS: sp[-1] = Level->TotalItems; break;
        case LEVELINFO_FOUND_ITEMS: sp[-1] = Level->CurrentItems; break;
        case LEVELINFO_TOTAL_MONSTERS: sp[-1] = Level->TotalKills; break;
        case LEVELINFO_KILLED_MONSTERS: sp[-1] = Level->CurrentKills; break;
        default: sp[-1] = 0; break;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ChangeSky)
      Level->ChangeSky(GetStr(sp[-2]), GetStr(sp[-1]));
      sp -= 2;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PlayerInGame)
      sp[-1] = (sp[-1] < 0 || sp[-1] >= MAXPLAYERS) ? false :
        (Level->Game->Players[sp[-1]] && (Level->Game->Players[
        sp[-1]]->PlayerFlags&VBasePlayer::PF_Spawned));
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PlayerIsBot)
      sp[-1] = (sp[-1] < 0 || sp[-1] >= MAXPLAYERS) ? false :
        Level->Game->Players[sp[-1]] && Level->Game->Players[
        sp[-1]]->PlayerFlags&VBasePlayer::PF_Spawned &&
        Level->Game->Players[sp[-1]]->PlayerFlags &
        VBasePlayer::PF_IsBot;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetCameraToTexture)
      //FIXME: camera texture names are still limited to 8 chars
      XLevel->SetCameraToTexture(EntityFromTID(sp[-3], Activator), GetName8(sp[-2]), sp[-1]);
      sp -= 3;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_EndLog)
      PrintStr = PrintStr.EvalEscapeSequences();
      GCon->Log(NAME_Debug, PrintStr);
      SB_POP;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GetAmmoCapacity)
      sp[-1] = (Activator ? Activator->eventGetAmmoCapacity(GetNameLowerCase(sp[-1])) : 0);
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetAmmoCapacity)
      if (Activator) Activator->eventSetAmmoCapacity(GetNameLowerCase(sp[-2]), sp[-1]);
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PrintMapCharArray)
    ACSVM_CASE(PCD_PrintMapChRange)
      {
        int Idx = 0, count = 0x7fffffff;
        if (cmd == PCD_PrintMapChRange) {
          count = sp[-1];
          Idx = sp[-2];
          sp -= 2;
        }
        int ANum = *ActiveObject->MapVars[sp[-1]];
        Idx += sp[-2];
        sp -= 2;
        if (Idx >= 0 && count > 0) {
          for (int c = ActiveObject->GetArrayVal(ANum, Idx); c; c = ActiveObject->GetArrayVal(ANum, Idx)) {
            PrintStr += (char)c;
            if (Idx == 0x7fffffff) break;
            if (--count == 0) break;
            ++Idx;
          }
        }
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PrintWorldCharArray)
    ACSVM_CASE(PCD_PrintWorldChRange)
      {
        int Idx = 0, count = 0x7fffffff;
        if (cmd == PCD_PrintWorldChRange) {
          count = sp[-1];
          Idx = sp[-2];
          sp -= 2;
        }
        int ANum = *ActiveObject->MapVars[sp[-1]];
        Idx += sp[-2];
        sp -= 2;
        if (Idx >= 0 && count > 0) {
          for (int c = globals->GetWorldArrayInt(ANum, Idx); c; c = globals->GetWorldArrayInt(ANum, Idx)) {
            PrintStr += (char)c;
            if (Idx == 0x7fffffff) break;
            if (--count == 0) break;
            ++Idx;
          }
        }
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PrintGlobalCharArray)
    ACSVM_CASE(PCD_PrintGlobalChRange)
      {
        int Idx = 0, count = 0x7fffffff;
        if (cmd == PCD_PrintGlobalChRange) {
          count = sp[-1];
          Idx = sp[-2];
          sp -= 2;
        }
        int ANum = *ActiveObject->MapVars[sp[-1]];
        Idx += sp[-2];
        sp -= 2;
        if (Idx >= 0 && count > 0) {
          for (int c = globals->GetGlobalArrayInt(ANum, Idx); c; c = globals->GetGlobalArrayInt(ANum, Idx)) {
            PrintStr += (char)c;
            if (Idx == 0x7fffffff) break;
            if (--count == 0) break;
            ++Idx;
          }
        }
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetActorAngle)
      if (!sp[-2]) {
        if (Activator) Activator->Angles.yaw = (float)(sp[-1]&0xffff)*360.0f/(float)0x10000;
      } else {
        for (VEntity *Ent = Level->FindMobjFromTID(sp[-2], nullptr); Ent; Ent = Level->FindMobjFromTID(sp[-2], Ent)) {
          Ent->Angles.yaw = (float)(sp[-1]&0xffff)*360.0f/(float)0x10000;
        }
      }
      sp -= 2;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SpawnProjectile)
      Level->eventEV_ThingProjectile(sp[-7], 0, sp[-5], sp[-4], sp[-3], sp[-2], sp[-1], GetNameLowerCase(sp[-6]), Activator);
      sp -= 7;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GetSectorLightLevel)
      {
        sector_t *sector;
        int SNum = FindSectorFromTag(sector, sp[-1]);
        sp[-1] = (SNum >= 0 ? sector->params.lightlevel : 0);
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GetActorCeilingZ)
      {
        VEntity *Ent = EntityFromTID(sp[-1], Activator);
        sp[-1] = Ent ? vint32(Ent->CeilingZ*0x10000) : 0;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetActorPosition)
      {
        VEntity *Ent = EntityFromTID(sp[-5], Activator);
        sp[-5] = Ent ? Ent->eventMoveThing(TVec(
          (float)sp[-4]/(float)0x10000,
          (float)sp[-3]/(float)0x10000,
          (float)sp[-2]/(float)0x10000), !!sp[-1]) : 0;
        sp -= 4;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ClearActorInventory)
      {
        for (VEntity *mobj = Level->FindMobjFromTID(sp[-1], nullptr); mobj; mobj = Level->FindMobjFromTID(sp[-1], mobj)) {
          mobj->eventClearInventory();
        }
      }
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GiveActorInventory)
      {
        for (VEntity *mobj = Level->FindMobjFromTID(sp[-3], nullptr); mobj; mobj = Level->FindMobjFromTID(sp[-3], mobj)) {
          mobj->eventGiveInventory(GetNameLowerCase(sp[-2]), sp[-1], false); // disable replacement
        }
      }
      sp -= 3;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_TakeActorInventory)
      {
        //int searcher = -1;
        for (VEntity *mobj = Level->FindMobjFromTID(sp[-3], nullptr); mobj; mobj = Level->FindMobjFromTID(sp[-3], mobj)) {
          mobj->eventTakeInventory(GetNameLowerCase(sp[-2]), sp[-1], false); // disable replacement
        }
      }
      sp -= 3;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_CheckActorInventory)
      {
        VEntity *Ent = EntityFromTID(sp[-2], Activator);
        sp[-2] = (!Ent ? 0 : Ent->eventCheckInventory(GetNameLowerCase(sp[-1]), false)); // disable replacement
      }
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ThingCountName)
      sp[-2] = Level->eventThingCount(0, GetNameLowerCase(sp[-2]), sp[-1], -1);
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SpawnSpotFacing)
      sp[-3] = Level->eventAcsSpawnSpotFacing(GetNameLowerCase(sp[-3]), sp[-2], sp[-1]);
      sp -= 2;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PlayerClass)
      if (sp[-1] < 0 || sp[-1] >= MAXPLAYERS || !Level->Game->Players[sp[-1]] ||
          !(Level->Game->Players[sp[-1]]->PlayerFlags&VBasePlayer::PF_Spawned))
      {
        sp[-1] = -1;
      }
      else
      {
        sp[-1] = Level->Game->Players[sp[-1]]->PClass;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AndScriptVar)
      locals[READ_BYTE_OR_INT32] &= sp[-1];
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AndMapVar)
      *ActiveObject->MapVars[READ_BYTE_OR_INT32] &= sp[-1];
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AndWorldVar)
      //WorldVars[READ_BYTE_OR_INT32] &= sp[-1];
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetWorldVarInt(vidx, globals->GetWorldVarInt(vidx)&sp[-1]);
      }
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AndGlobalVar)
      //GlobalVars[READ_BYTE_OR_INT32] &= sp[-1];
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetGlobalVarInt(vidx, globals->GetGlobalVarInt(vidx)&sp[-1]);
      }
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AndMapArray)
      {
        int ANum = *ActiveObject->MapVars[READ_BYTE_OR_INT32];
        ActiveObject->SetArrayVal(ANum, sp[-2], ActiveObject->GetArrayVal(ANum, sp[-2])&sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AndWorldArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        //WorldArrays[ANum].SetElemVal(sp[-2], WorldArrays[ANum].GetElemVal(sp[-2]) & sp[-1]);
        globals->SetWorldArrayInt(ANum, sp[-2], globals->GetWorldArrayInt(ANum, sp[-2])&sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AndGlobalArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        //GlobalArrays[ANum].SetElemVal(sp[-2], GlobalArrays[ANum].GetElemVal(sp[-2]) & sp[-1]);
        globals->SetGlobalArrayInt(ANum, sp[-2], globals->GetGlobalArrayInt(ANum, sp[-2])&sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_EOrScriptVar)
      locals[READ_BYTE_OR_INT32] ^= sp[-1];
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_EOrMapVar)
      *ActiveObject->MapVars[READ_BYTE_OR_INT32] ^= sp[-1];
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_EOrWorldVar)
      //WorldVars[READ_BYTE_OR_INT32] ^= sp[-1];
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetWorldVarInt(vidx, globals->GetWorldVarInt(vidx)^sp[-1]);
      }
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_EOrGlobalVar)
      //GlobalVars[READ_BYTE_OR_INT32] ^= sp[-1];
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetGlobalVarInt(vidx, globals->GetGlobalVarInt(vidx)^sp[-1]);
      }
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_EOrMapArray)
      {
        int ANum = *ActiveObject->MapVars[READ_BYTE_OR_INT32];
        ActiveObject->SetArrayVal(ANum, sp[-2], ActiveObject->GetArrayVal(ANum, sp[-2])^sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_EOrWorldArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        //WorldArrays[ANum].SetElemVal(sp[-2], WorldArrays[ANum].GetElemVal(sp[-2]) ^ sp[-1]);
        globals->SetWorldArrayInt(ANum, sp[-2], globals->GetWorldArrayInt(ANum, sp[-2])^sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_EOrGlobalArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        //GlobalArrays[ANum].SetElemVal(sp[-2], GlobalArrays[ANum].GetElemVal(sp[-2]) ^ sp[-1]);
        globals->SetGlobalArrayInt(ANum, sp[-2], globals->GetGlobalArrayInt(ANum, sp[-2])^sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_OrScriptVar)
      locals[READ_BYTE_OR_INT32] |= sp[-1];
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_OrMapVar)
      *ActiveObject->MapVars[READ_BYTE_OR_INT32] |= sp[-1];
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_OrWorldVar)
      //WorldVars[READ_BYTE_OR_INT32] |= sp[-1];
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetWorldVarInt(vidx, globals->GetWorldVarInt(vidx)|sp[-1]);
      }
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_OrGlobalVar)
      //GlobalVars[READ_BYTE_OR_INT32] |= sp[-1];
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetGlobalVarInt(vidx, globals->GetGlobalVarInt(vidx)|sp[-1]);
      }
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_OrMapArray)
      {
        int ANum = *ActiveObject->MapVars[READ_BYTE_OR_INT32];
        ActiveObject->SetArrayVal(ANum, sp[-2], ActiveObject->GetArrayVal(ANum, sp[-2])|sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_OrWorldArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        //WorldArrays[ANum].SetElemVal(sp[-2], WorldArrays[ANum].GetElemVal(sp[-2]) | sp[-1]);
        globals->SetWorldArrayInt(ANum, sp[-2], globals->GetWorldArrayInt(ANum, sp[-2])|sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_OrGlobalArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        //GlobalArrays[ANum].SetElemVal(sp[-2], GlobalArrays[ANum].GetElemVal(sp[-2]) | sp[-1]);
        globals->SetGlobalArrayInt(ANum, sp[-2], globals->GetGlobalArrayInt(ANum, sp[-2])|sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSScriptVar)
      locals[READ_BYTE_OR_INT32] <<= sp[-1];
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSMapVar)
      *ActiveObject->MapVars[READ_BYTE_OR_INT32] <<= sp[-1];
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSWorldVar)
      //WorldVars[READ_BYTE_OR_INT32] <<= sp[-1];
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetWorldVarInt(vidx, globals->GetWorldVarInt(vidx)<<sp[-1]);
      }
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSGlobalVar)
      //GlobalVars[READ_BYTE_OR_INT32] <<= sp[-1];
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetGlobalVarInt(vidx, globals->GetGlobalVarInt(vidx)<<sp[-1]);
      }
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSMapArray)
      {
        int ANum = *ActiveObject->MapVars[READ_BYTE_OR_INT32];
        ActiveObject->SetArrayVal(ANum, sp[-2], ActiveObject->GetArrayVal(ANum, sp[-2])<<sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSWorldArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        //WorldArrays[ANum].SetElemVal(sp[-2], WorldArrays[ANum].GetElemVal(sp[-2]) << sp[-1]);
        globals->SetWorldArrayInt(ANum, sp[-2], globals->GetWorldArrayInt(ANum, sp[-2])<<sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSGlobalArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        //GlobalArrays[ANum].SetElemVal(sp[-2], GlobalArrays[ANum].GetElemVal(sp[-2]) << sp[-1]);
        globals->SetGlobalArrayInt(ANum, sp[-2], globals->GetGlobalArrayInt(ANum, sp[-2])<<sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_RSScriptVar)
      locals[READ_BYTE_OR_INT32] >>= sp[-1];
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_RSMapVar)
      *ActiveObject->MapVars[READ_BYTE_OR_INT32] >>= sp[-1];
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_RSWorldVar)
      //WorldVars[READ_BYTE_OR_INT32] >>= sp[-1];
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetWorldVarInt(vidx, globals->GetWorldVarInt(vidx)>>sp[-1]);
      }
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_RSGlobalVar)
      //GlobalVars[READ_BYTE_OR_INT32] >>= sp[-1];
      {
        int vidx = READ_BYTE_OR_INT32;
        globals->SetGlobalVarInt(vidx, globals->GetGlobalVarInt(vidx)>>sp[-1]);
      }
      INC_BYTE_OR_INT32;
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_RSMapArray)
      {
        int ANum = *ActiveObject->MapVars[READ_BYTE_OR_INT32];
        ActiveObject->SetArrayVal(ANum, sp[-2], ActiveObject->GetArrayVal(ANum, sp[-2])>>sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_RSWorldArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        //WorldArrays[ANum].SetElemVal(sp[-2], WorldArrays[ANum].GetElemVal(sp[-2]) >> sp[-1]);
        globals->SetWorldArrayInt(ANum, sp[-2], globals->GetWorldArrayInt(ANum, sp[-2])>>sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_RSGlobalArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        //GlobalArrays[ANum].SetElemVal(sp[-2], GlobalArrays[ANum].GetElemVal(sp[-2]) >> sp[-1]);
        globals->SetGlobalArrayInt(ANum, sp[-2], globals->GetGlobalArrayInt(ANum, sp[-2])>>sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GetPlayerInfo)
      STUB(PCD_GetPlayerInfo)
      //sp[-2] - Player num
      //sp[-1] - Info type
      //Pushes result.
      sp[-2] = 0; // "unknown info" should be 0
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ChangeLevel)
      //STUB(PCD_ChangeLevel)
      //sp[-4] - Level name
      //sp[-3] - Position
      //sp[-2] - Flags
      //sp[-1] - Skill
      GCmdBuf << va("ACS_TeleportNewMap \"%s\" %d %d %d\n", *GetStr(sp[-4]).quote(), sp[-3], sp[-2], sp[-1]);
      sp -= 4;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SectorDamage)
      Level->eventSectorDamage(sp[-5], sp[-4], GetName(sp[-3]), GetNameLowerCase(sp[-2]), sp[-1]);
      sp -= 5;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ReplaceTextures)
      // walls
      if ((sp[-1]&(NOT_TOP|NOT_MIDDLE|NOT_BOTTOM)) != (NOT_TOP|NOT_MIDDLE|NOT_BOTTOM)) {
        //int FromTex = GTextureManager.NumForName(GetName8(sp[-3]), TEXTYPE_Wall, true);
        //int ToTex = GTextureManager.NumForName(GetName8(sp[-2]), TEXTYPE_Wall, true);
        int FromTex = GTextureManager.FindFullyNamedTexture(GetStr(sp[-3]), nullptr, TEXTYPE_Wall, /*overload*/true);
        if (FromTex >= 0) {
          int ToTex = GTextureManager.FindOrLoadFullyNamedTextureAsMapTexture(GetStr(sp[-2]), nullptr, TEXTYPE_Wall, /*overload*/true);
          for (int i = 0; i < XLevel->NumSides; ++i) {
            if (!(sp[-1]&NOT_TOP) && XLevel->Sides[i].TopTexture == FromTex) XLevel->Sides[i].TopTexture = ToTex;
            if (!(sp[-1]&NOT_MIDDLE) && XLevel->Sides[i].MidTexture == FromTex) XLevel->Sides[i].MidTexture = ToTex;
            if (!(sp[-1]&NOT_BOTTOM) && XLevel->Sides[i].BottomTexture == FromTex) XLevel->Sides[i].BottomTexture = ToTex;
          }
        }
      }
      // flats
      if ((sp[-1]&(NOT_FLOOR|NOT_CEILING)) != (NOT_FLOOR|NOT_CEILING)) {
        //int FromTex = GTextureManager.NumForName(GetName8(sp[-3]), TEXTYPE_Flat, true);
        //int ToTex = GTextureManager.NumForName(GetName8(sp[-2]), TEXTYPE_Flat, true);
        int FromTex = GTextureManager.FindFullyNamedTexture(GetStr(sp[-3]), nullptr, TEXTYPE_Flat, /*overload*/true);
        if (FromTex >= 0) {
          int ToTex = GTextureManager.FindOrLoadFullyNamedTextureAsMapTexture(GetStr(sp[-2]), nullptr, TEXTYPE_Flat, /*overload*/true);
          for (int i = 0; i < XLevel->NumSectors; ++i) {
            if (!(sp[-1]&NOT_FLOOR) && XLevel->Sectors[i].floor.pic == FromTex) XLevel->Sectors[i].floor.pic = ToTex;
            if (!(sp[-1]&NOT_CEILING) && XLevel->Sectors[i].ceiling.pic == FromTex) XLevel->Sectors[i].ceiling.pic = ToTex;
          }
        }
      }
      sp -= 3;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_NegateBinary)
      sp[-1] = ~sp[-1];
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GetActorPitch)
      {
        VEntity *Ent = EntityFromTID(sp[-1], Activator);
        sp[-1] = (Ent ? vint32(Ent->Angles.pitch*0x10000/360)&0xffff : 0);
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetActorPitch)
      if (!sp[-2]) {
        if (Activator) Activator->Angles.pitch = AngleMod180((float)(sp[-1]&0xffff)*360.0f/(float)0x10000);
      } else {
        for (VEntity *Ent = Level->FindMobjFromTID(sp[-2], nullptr); Ent; Ent = Level->FindMobjFromTID(sp[-2], Ent)) {
          Ent->Angles.pitch = AngleMod180((float)(sp[-1]&0xffff)*360.0f/(float)0x10000);
        }
      }
      sp -= 2;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PrintBind)
      STUB(PCD_PrintBind)
      //sp[-1] - command (string)
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetActorState)
      {
        TArray<VName> Names;
        VMemberBase::StaticSplitStateLabel(GetStr(sp[-2]), Names);
        if (!sp[-3]) {
          VStateLabel *Lbl = (!Activator ? nullptr : Activator->GetClass()->FindStateLabel(Names, !!sp[-1]));
          if (Lbl && Lbl->State) {
            //GCon->Logf(NAME_Debug, "ACS: SetActorState; name=<%s> (%s)", *GetStr(sp[-2]), (Activator ? Activator->GetClass()->GetName() : "<>"));
            Activator->SetState(Lbl->State);
            sp[-3] = 1;
          } else {
            sp[-3] = 0;
          }
        } else {
          int Count = 0;
          for (VEntity *Ent = Level->FindMobjFromTID(sp[-3], nullptr); Ent; Ent = Level->FindMobjFromTID(sp[-3], Ent)) {
            VStateLabel *Lbl = Ent->GetClass()->FindStateLabel(Names, !!sp[-1]);
            if (Lbl && Lbl->State) {
              Ent->SetState(Lbl->State);
              ++Count;
            }
          }
          sp[-3] = Count;
        }
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ThingDamage2)
      sp[-3] = Level->eventDoThingDamage(sp[-3], sp[-2], GetName(sp[-1]), Activator);
      sp -= 2;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_UseInventory)
      if (Activator) {
        sp[-1] = Activator->eventUseInventoryName(GetNameLowerCase(sp[-1]), false); // disable replacement
      } else {
        sp[-1] = 0;
        for (auto &&it : Level->Game->playersSpawned()) {
          sp[-1] += it.player()->MO->eventUseInventoryName(GetNameLowerCase(sp[-1]), false); // disable replacement
        }
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_UseActorInventory)
      if (sp[-2]) {
        int Ret = 0;
        for (VEntity *Ent = Level->FindMobjFromTID(sp[-2], nullptr); Ent; Ent = Level->FindMobjFromTID(sp[-2], Ent)) {
          Ret += Ent->eventUseInventoryName(GetNameLowerCase(sp[-1]), false); // disable replacement
        }
        sp[-2] = Ret;
      } else {
        sp[-1] = 0;
        for (auto &&it : Level->Game->playersSpawned()) {
          sp[-1] += it.player()->MO->eventUseInventoryName(GetNameLowerCase(sp[-1]), false); // disable replacement
        }
      }
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_CheckActorCeilingTexture)
      {
        VEntity *Ent = EntityFromTID(sp[-2], Activator);
        if (Ent) {
          if (Ent->Sector) {
            //int Tex = GTextureManager.CheckNumForName(GetName8(sp[-1]), TEXTYPE_Wall, true);
            int Tex = GTextureManager.FindFullyNamedTexture(GetStr(sp[-1]), nullptr, TEXTYPE_Flat, /*overload*/true);
            sp[-2] = (Ent->Sector->ceiling.pic == Tex ? 1 : 0);
          } else {
            GCon->Logf(NAME_Warning, "ACS: CheckActorCeilingTexture for actor %s(%u) without a sector (flags:0x%08u; flagsEx:0x%08x) (txname=<%s>)", Ent->GetClass()->GetName(), Ent->GetUniqueId(), Ent->EntityFlags, Ent->FlagsEx, *GetStr(sp[-1]));
            sp[-2] = 0;
          }
        } else {
          sp[-2] = 0;
        }
      }
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_CheckActorFloorTexture)
      {
        VEntity *Ent = EntityFromTID(sp[-2], Activator);
        if (Ent) {
          if (Ent->Sector) {
            //int Tex = GTextureManager.CheckNumForName(GetName8(sp[-1]), TEXTYPE_Wall, true);
            int Tex = GTextureManager.FindFullyNamedTexture(GetStr(sp[-1]), nullptr, TEXTYPE_Flat, /*overload*/true);
            sp[-2] = (Ent->Sector->floor.pic == Tex ? 1 : 0);
          } else {
            GCon->Logf(NAME_Warning, "ACS: CheckActorFloorTexture for actor %s(%u) without a sector (flags:0x%08u; flagsEx:0x%08x) (txname=<%s>)", Ent->GetClass()->GetName(), Ent->GetUniqueId(), Ent->EntityFlags, Ent->FlagsEx, *GetStr(sp[-1]));
            sp[-2] = 0;
          }
        } else {
          sp[-2] = 0;
        }
      }
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GetActorLightLevel)
      {
        VEntity *Ent = EntityFromTID(sp[-1], Activator);
        sp[-1] = (Ent ? Ent->Sector->params.lightlevel : 0);
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SetMugShotState)
      STUB(PCD_SetMugShotState)
      //sp[-1] - state (string)
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ThingCountSector)
      sp[-3] = Level->eventThingCount(sp[-3], NAME_None, sp[-2], sp[-1]);
      sp -= 2;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ThingCountNameSector)
      sp[-3] = Level->eventThingCount(0, GetNameLowerCase(sp[-3]),
        sp[-2], sp[-1]);
      sp -= 2;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_CheckPlayerCamera)
      if (sp[-1] < 0 || sp[-1] >= MAXPLAYERS ||
          !Level->Game->Players[sp[-1]] ||
          !(Level->Game->Players[sp[-1]]->PlayerFlags&VBasePlayer::PF_Spawned) ||
          !Level->Game->Players[sp[-1]]->Camera)
      {
        sp[-1] = -1;
      } else {
        sp[-1] = Level->Game->Players[sp[-1]]->Camera->TID;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_MorphActor)
      if (sp[-7]) {
        //int searcher = -1;
        int Res = 0;
        for (VEntity *Ent = Level->FindMobjFromTID(sp[-7], nullptr); Ent; Ent = Level->FindMobjFromTID(sp[-7], Ent)) {
          Res += Ent->eventMorphActor(GetNameLowerCase(sp[-6]),
            GetNameLowerCase(sp[-5]), sp[-4]/35.0f, sp[-3],
            GetNameLowerCase(sp[-2]), GetNameLowerCase(sp[-1]));
        }
        sp[-7] = Res;
      } else if (Activator) {
        sp[-7] = Activator->eventMorphActor(GetNameLowerCase(sp[-6]),
            GetNameLowerCase(sp[-5]), sp[-4]/35.0f, sp[-3],
            GetNameLowerCase(sp[-2]), GetNameLowerCase(sp[-1]));
      } else {
        sp[-7] = 0;
      }
      sp -= 6;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_UnmorphActor)
      if (sp[-2]) {
        //int searcher = -1;
        int Res = 0;
        for (VEntity *Ent = Level->FindMobjFromTID(sp[-2], nullptr); Ent; Ent = Level->FindMobjFromTID(sp[-2], Ent)) {
          Res += Ent->eventUnmorphActor(Activator, sp[-1]);
        }
        sp[-2] = Res;
      } else if (Activator) {
        sp[-2] = Activator->eventUnmorphActor(Activator, sp[-1]);
      } else {
        sp[-2] = 0;
      }
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GetPlayerInput)
      if (sp[-2] < 0) {
        if (Activator && Activator->IsPlayer()) {
          sp[-2] = (Activator->Player ? Activator->Player->AcsGetInput(sp[-1]) : 0);
          //GCon->Logf(NAME_Debug, "PIP (0x%04x): 0x%08x", (unsigned)sp[-1], (unsigned)sp[-2]);
        } else {
          sp[-2] = 0;
        }
      } else if (sp[-2] < MAXPLAYERS && Level->Game->Players[sp[-2]] &&
                 (Level->Game->Players[sp[-2]]->PlayerFlags&VBasePlayer::PF_Spawned))
      {
        sp[-2] = Level->Game->Players[sp[-2]]->AcsGetInput(sp[-1]);
      } else {
        sp[-2] = 0;
      }
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ClassifyActor)
      if (sp[-1]) {
        VEntity *Ent = EntityFromTID(sp[-1], Activator);
        sp[-1] = (Ent ? Ent->eventClassifyActor() : 0);
      } else if (Activator) {
        sp[-1] = Activator->eventClassifyActor();
      } else {
        // world
        sp[-1] = 1;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PrintBinary)
      {
        vuint32 Val = sp[-1];
        do {
          PrintStr += (Val&1 ? "1" : "0");
          Val >>= 1;
        } while (Val);
      }
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PrintHex)
      PrintStr += va("%X", sp[-1]);
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ConsoleCommand)
      if (acs_warning_console_commands) GCon->Logf(NAME_Warning, "no console commands from ACS (%s)!", *GetStr(sp[-3]).quote());
      sp -= 3;
      ACSVM_BREAK;

    //ACSVM_CASE(PCD_Team2FragPoints)
    ACSVM_CASE(PCD_ConsoleCommandDirect)
      if (acs_warning_console_commands) GCon->Logf(NAME_Warning, "no console commands from ACS (%s)!", *GetStr(ip[0]|ActiveObject->GetLibraryID()).quote());
      ip += 3;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SaveString)
      //GCon->Logf("PCD_SaveString: <%s>", *PrintStr.quote());
      *sp = ActiveObject->Level->PutNewString(*PrintStr);
      ++sp;
      SB_POP;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PushFunction)
      {
        int funcnum = READ_BYTE_OR_INT32;
        if (funcnum < 0 || funcnum > 0xffff) Host_Error("invalid indirect function push in ACS code (%d)", funcnum);
        // tag it (library id already shifted)
        funcnum |= ActiveObject->GetLibraryID();
        *sp = funcnum;
        ++sp;
        INC_BYTE_OR_INT32;
      }
      ACSVM_BREAK;

    // various gzdoom translations
    ACSVM_CASE(PCD_TranslationRange3)
      GCon->Logf(NAME_Dev, "ACS: unimplemented gzdoom opcode 362 (TranslationRange3)");
      sp -= 8;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_TranslationRange4)
      GCon->Logf(NAME_Dev, "ACS: unimplemented gzdoom opcode 383 (TranslationRange4)");
      sp -= 5;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_TranslationRange5)
      GCon->Logf(NAME_Dev, "ACS: unimplemented gzdoom opcode 384 (TranslationRange5)");
      sp -= 6;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_GotoStack)
      //GCon->Logf(NAME_Dev, "ACS: unimplemented gzdoom opcode 363 (GotoStack)");
      ip = ActiveObject->OffsetToPtr(sp[-1]);
      --sp;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AssignScriptArray)
      localarrays->Set(locals, READ_BYTE_OR_INT32, sp[-2], sp[-1]);
      INC_BYTE_OR_INT32;
      sp -= 2;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PushScriptArray)
      sp[-1] = localarrays->Get(locals, READ_BYTE_OR_INT32, sp[-1]);
      INC_BYTE_OR_INT32;
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AddScriptArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        localarrays->Set(locals, ANum, sp[-2], localarrays->Get(locals, ANum, sp[-2])+sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_SubScriptArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        localarrays->Set(locals, ANum, sp[-2], localarrays->Get(locals, ANum, sp[-2])-sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_MulScriptArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        localarrays->Set(locals, ANum, sp[-2], localarrays->Get(locals, ANum, sp[-2])*sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_DivScriptArray)
      {
        if (sp[-1] == 0) Host_Error("ACS: division by zero!");
        int ANum = READ_BYTE_OR_INT32;
        localarrays->Set(locals, ANum, sp[-2], localarrays->Get(locals, ANum, sp[-2])/sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_ModScriptArray)
      {
        if (sp[-1] == 0) Host_Error("ACS: division by zero!");
        int ANum = READ_BYTE_OR_INT32;
        localarrays->Set(locals, ANum, sp[-2], localarrays->Get(locals, ANum, sp[-2])%sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_IncScriptArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        localarrays->Set(locals, ANum, sp[-1], localarrays->Get(locals, ANum, sp[-1])+1);
        INC_BYTE_OR_INT32;
        --sp;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_DecScriptArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        localarrays->Set(locals, ANum, sp[-1], localarrays->Get(locals, ANum, sp[-1])-1);
        INC_BYTE_OR_INT32;
        --sp;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_AndScriptArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        localarrays->Set(locals, ANum, sp[-2], localarrays->Get(locals, ANum, sp[-2])&sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_EOrScriptArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        localarrays->Set(locals, ANum, sp[-2], localarrays->Get(locals, ANum, sp[-2])^sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_OrScriptArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        localarrays->Set(locals, ANum, sp[-2], localarrays->Get(locals, ANum, sp[-2])|sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSScriptArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        localarrays->Set(locals, ANum, sp[-2], localarrays->Get(locals, ANum, sp[-2])<<sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_RSScriptArray)
      {
        int ANum = READ_BYTE_OR_INT32;
        localarrays->Set(locals, ANum, sp[-2], localarrays->Get(locals, ANum, sp[-2])>>sp[-1]);
        INC_BYTE_OR_INT32;
        sp -= 2;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_PrintScriptChArray)
    ACSVM_CASE(PCD_PrintScriptChRange)
      {
        int Idx = 0, count = 0x7fffffff;
        if (cmd == PCD_PrintScriptChRange) {
          count = sp[-1];
          Idx = sp[-2];
          sp -= 2;
        }
        int ANum = sp[-1]; //k8: is this right?
        Idx += sp[-2];
        sp -= 2;
        if (Idx >= 0 && count > 0) {
          for (int c = localarrays->Get(locals, ANum, Idx); c; c = localarrays->Get(locals, ANum, Idx)) {
            PrintStr += (char)c;
            if (Idx == 0x7fffffff) break;
            if (--count == 0) break;
            ++Idx;
          }
        }
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSpec5Ex)
      {
        int special = READ_INT32(ip);
        ip += 4;
        Level->eventExecuteActionSpecial(special, sp[-5], sp[-4], sp[-3], sp[-2], sp[-1], line, side, Activator);
        sp -= 5;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_LSpec5ExResult)
      {
        int special = READ_INT32(ip);
        ip += 4;
        sp[-5] = Level->eventExecuteActionSpecial(special, sp[-5], sp[-4], sp[-3], sp[-2], sp[-1], line, side, Activator);
        sp -= 4;
      }
      ACSVM_BREAK;

    ACSVM_CASE(PCD_IsOneFlagCTF)
      *sp = 0;
      ++sp;
      ACSVM_BREAK;

    // these p-codes are not supported; they will terminate script
    ACSVM_CASE(PCD_PlayerBlueSkull)
    ACSVM_CASE(PCD_PlayerRedSkull)
    ACSVM_CASE(PCD_PlayerYellowSkull)
    ACSVM_CASE(PCD_PlayerMasterSkull)
    ACSVM_CASE(PCD_PlayerBlueCard)
    ACSVM_CASE(PCD_PlayerRedCard)
    ACSVM_CASE(PCD_PlayerYellowCard)
    ACSVM_CASE(PCD_PlayerMasterCard)
    ACSVM_CASE(PCD_PlayerBlackSkull)
    ACSVM_CASE(PCD_PlayerSilverSkull)
    ACSVM_CASE(PCD_PlayerGoldSkull)
    ACSVM_CASE(PCD_PlayerBlackCard)
    ACSVM_CASE(PCD_PlayerSilverCard)
    ACSVM_CASE(PCD_PlayerOnTeam)
    ACSVM_CASE(PCD_PlayerTeam)
    ACSVM_CASE(PCD_PlayerExpert)
    ACSVM_CASE(PCD_BlueTeamCount)
    ACSVM_CASE(PCD_RedTeamCount)
    ACSVM_CASE(PCD_BlueTeamScore)
    ACSVM_CASE(PCD_RedTeamScore)
    ACSVM_CASE(PCD_LSpec6)
    ACSVM_CASE(PCD_LSpec6Direct)
    ACSVM_CASE(PCD_SetStyle)
    ACSVM_CASE(PCD_SetStyleDirect)
    ACSVM_CASE(PCD_WriteToIni)
    ACSVM_CASE(PCD_GetFromIni)
    ACSVM_CASE(PCD_GrabInput)
    ACSVM_CASE(PCD_SetMousePointer)
    ACSVM_CASE(PCD_MoveMousePointer)
    ACSVM_CASE(PCD_StrCpyToMapChRange)
    ACSVM_CASE(PCD_StrCpyToWorldChRange)
    ACSVM_CASE(PCD_StrCpyToGlobalChRange)
    ACSVM_CASE(PCD_StrCpyToScriptChRange)
      {
        const PCD_Info *pi;
        for (pi = PCD_List; pi->name; ++pi) if (pi->index == cmd) break;
        const char *opcname = (pi->name ? pi->name : "UNKNOWN");
        if (!dbg_acs_allow_unimplemented_opcodes && acs_halt_on_unimplemented_opcode) Host_Error("ACS: Unsupported p-code %s (%d), script %d terminated", opcname, cmd, info->Number);
        if (!acsReportedBadOpcodesInited) {
          acsReportedBadOpcodesInited = true;
          memset(acsReportedBadOpcodes, 0, sizeof(acsReportedBadOpcodes));
        }
        if (cmd >= 0 && cmd <= 65535) {
          if (!acsReportedBadOpcodes[cmd]) {
            acsReportedBadOpcodes[cmd] = true;
            GCon->Logf(NAME_Error, "ACS: Unsupported p-code %s %d, script %d terminated", opcname, cmd, info->Number);
          }
        } else {
          GCon->Logf(NAME_Error, "ACS: Unsupported p-code %s %d, script %d terminated", opcname, cmd, info->Number);
        }
        //GCon->Logf(NAME_Dev, "Unsupported ACS p-code %d", cmd);
        action = SCRIPT_Terminate;
      }
      ACSVM_BREAK_STOP;

    ACSVM_DEFAULT
      if (acs_halt_on_unknown_opcode) Host_Error("Illegal ACS opcode %d", cmd);
      GCon->Logf(NAME_Error, "ACS: Illegal ACS opcode %d", cmd);
      action = SCRIPT_Terminate;
      ACSVM_BREAK_STOP;
    }
  } while (action == SCRIPT_Continue);

#if USE_COMPUTED_GOTO
LblFuncStop:
#endif
  //fprintf(stderr, "VAcs::RunScript:003: self name is '%s' (number is %d)\n", *info->Name, info->Number);
  if (action == SCRIPT_Terminate) {
    if (info->RunningScript == this) info->RunningScript = nullptr;
    //GCon->Logf(NAME_Debug, "*** ACS TERMINATED: %s (%d)", *info->Name, info->Number);
    ResetSaveds(); // just in case
    DestroyThinker();
  } else {
    vassert(action == SCRIPT_Stop); // always
    if (activeFunction) {
      GCon->Logf(NAME_Error, "ACS script #%d (named '%s') tried to suspend inside a function; terminated", info->Number, *info->Name);
      action = SCRIPT_Terminate;
      if (info->RunningScript == this) info->RunningScript = nullptr;
      ResetSaveds(); // just in case
      DestroyThinker();
    } else if (sp != mystack) {
      GCon->Logf(NAME_Error, "ACS script #%d (named '%s') tried to delay/suspend with non-empty stack; terminated", info->Number, *info->Name);
      action = SCRIPT_Terminate;
      if (info->RunningScript == this) info->RunningScript = nullptr;
      ResetSaveds(); // just in case
      DestroyThinker();
    } else {
      InstructionPointer = ip;
      vassert(locals == LocalVars);
    }
  }
  return resultValue;
}


//==========================================================================
//
//  FindSectorFromTag
//
//  RETURN NEXT SECTOR # THAT LINE TAG REFERS TO
//
//==========================================================================
int VAcs::FindSectorFromTag (sector_t *&sector, int tag, int start) {
  /*
  for (int i = start + 1; i < XLevel->NumSectors; i++)
    if (XLevel->Sectors[i].tag == tag)
      return i;
  return -1;
  */
  return XLevel->FindSectorFromTag(sector, tag, start);
}


//==========================================================================
//
//  VAcsGlobal::VAcsGlobal
//
//==========================================================================
VAcsGlobal::VAcsGlobal () {
  //memset((void *)WorldVars, 0, sizeof(WorldVars));
  //memset((void *)GlobalVars, 0, sizeof(GlobalVars));
}

// get gvar
VStr VAcsGlobal::GetGlobalVarStr (VAcsLevel *level, int index) const {
  return (level && index >= 0 && index < MAX_ACS_GLOBAL_VARS ? level->GetString(GlobalVars.GetElemVal(index)) : VStr());
}

int VAcsGlobal::GetGlobalVarInt (int index) const {
  return (index >= 0 && index < MAX_ACS_GLOBAL_VARS ? GlobalVars.GetElemVal(index) : 0);
}

float VAcsGlobal::GetGlobalVarFloat (int index) const {
  return (index >= 0 && index < MAX_ACS_GLOBAL_VARS ? (float)GlobalVars.GetElemVal(index)/65536.0f : 0.0f);
}

// set gvar
void VAcsGlobal::SetGlobalVarInt (int index, int value) {
  if (index >= 0 && index < MAX_ACS_GLOBAL_VARS) GlobalVars.SetElemVal(index, value);
  else Sys_Error("invalid ACS global index %d", index);
}

void VAcsGlobal::SetGlobalVarFloat (int index, float value) {
  if (index >= 0 && index < MAX_ACS_GLOBAL_VARS) GlobalVars.SetElemVal(index, (int)(value*65536.0f));
  else Sys_Error("invalid ACS global index %d", index);
}

// get wvar
int VAcsGlobal::GetWorldVarInt (int index) const {
  return (index >= 0 && index < MAX_ACS_WORLD_VARS ? WorldVars.GetElemVal(index) : 0);
}

float VAcsGlobal::GetWorldVarFloat (int index) const {
  return (index >= 0 && index < MAX_ACS_WORLD_VARS ? (float)WorldVars.GetElemVal(index)/65536.0f : 0.0f);
}

// set wvar
void VAcsGlobal::SetWorldVarInt (int index, int value) {
  if (index >= 0 && index < MAX_ACS_WORLD_VARS) WorldVars.SetElemVal(index, value);
  else Sys_Error("invalid ACS world index %d", index);
}

void VAcsGlobal::SetWorldVarFloat (int index, float value) {
  if (index >= 0 && index < MAX_ACS_WORLD_VARS) WorldVars.SetElemVal(index, (int)(value*65536.0f));
  else Sys_Error("invalid ACS world index %d", index);
}

// get garr
int VAcsGlobal::GetGlobalArrayInt (int aidx, int index) const {
  if (aidx >= 0 && aidx < MAX_ACS_GLOBAL_VARS) return GlobalArrays[aidx].GetElemVal(index);
  return 0;
}

float VAcsGlobal::GetGlobalArrayFloat (int aidx, int index) const {
  if (aidx >= 0 && aidx < MAX_ACS_GLOBAL_VARS) return (float)GlobalArrays[aidx].GetElemVal(index)/65536.0f;
  return 0;
}

// set garr
void VAcsGlobal::SetGlobalArrayInt (int aidx, int index, int value) {
  if (aidx >= 0 && aidx < MAX_ACS_GLOBAL_VARS) GlobalArrays[aidx].SetElemVal(index, value);
  else Sys_Error("invalid ACS global array index %d", aidx);
}

void VAcsGlobal::SetGlobalArrayFloat (int aidx, int index, float value) {
  if (aidx >= 0 && aidx < MAX_ACS_GLOBAL_VARS) GlobalArrays[aidx].SetElemVal(index, (int)(value/65536.0f));
  else Sys_Error("invalid ACS global array index %d", aidx);
}


// get warr
int VAcsGlobal::GetWorldArrayInt (int aidx, int index) const {
  if (aidx >= 0 && aidx < MAX_ACS_WORLD_VARS) return WorldArrays[aidx].GetElemVal(index);
  return 0;
}

float VAcsGlobal::GetWorldArrayFloat (int aidx, int index) const {
  if (aidx >= 0 && aidx < MAX_ACS_WORLD_VARS) return (float)WorldArrays[aidx].GetElemVal(index)/65536.0f;
  return 0;
}

// set warr
void VAcsGlobal::SetWorldArrayInt (int aidx, int index, int value) {
  if (aidx >= 0 && aidx < MAX_ACS_WORLD_VARS) WorldArrays[aidx].SetElemVal(index, value);
  else Sys_Error("invalid ACS world array index %d", aidx);
}

void VAcsGlobal::SetWorldArrayFloat (int aidx, int index, float value) {
  if (aidx >= 0 && aidx < MAX_ACS_WORLD_VARS) WorldArrays[aidx].SetElemVal(index, (int)(value/65536.0f));
  else Sys_Error("invalid ACS world array index %d", aidx);
}


//==========================================================================
//
//  VAcsStore::Serialise
//
//==========================================================================
void VAcsStore::Serialise (VStream &Strm) {
  vuint8 xver = 1;
  Strm << xver;
  if (xver != 1) Host_Error("invalid ACS store version in save file");
  Strm << Map
       << Type
       << PlayerNum
       << STRM_INDEX(Script)
       << STRM_INDEX(Args[0])
       << STRM_INDEX(Args[1])
       << STRM_INDEX(Args[2])
       << STRM_INDEX(Args[3]);
}


//==========================================================================
//
//  VAcsGlobal::Serialise
//
//==========================================================================
void VAcsGlobal::Serialise (VStream &Strm) {
  vuint8 xver = 1;
  Strm << xver;
  if (xver != 1) Host_Error("invalid ACS global store version in save file");

  Strm << WorldVars;
  Strm << GlobalVars;

  // world arrays
  int worldArrCount = MAX_ACS_WORLD_VARS;
  Strm << STRM_INDEX(worldArrCount);
  if (Strm.IsLoading()) {
    for (int i = 0; i < MAX_ACS_WORLD_VARS; ++i) WorldArrays[i].clear();
    while (worldArrCount-- > 0) {
      int index = -1;
      Strm << STRM_INDEX(index);
      if (index < 0 || index >= MAX_ACS_WORLD_VARS) Host_Error("invalid ACS world array number in save file");
      WorldArrays[index].Serialise(Strm);
    }
  } else {
    for (int i = 0; i < MAX_ACS_WORLD_VARS; ++i) {
      Strm << STRM_INDEX(i);
      WorldArrays[i].Serialise(Strm);
    }
  }

  // world arrays
  int globalArrCount = MAX_ACS_GLOBAL_VARS;
  Strm << STRM_INDEX(globalArrCount);
  if (Strm.IsLoading()) {
    for (int i = 0; i < MAX_ACS_GLOBAL_VARS; ++i) GlobalArrays[i].clear();
    while (globalArrCount-- > 0) {
      int index = -1;
      Strm << STRM_INDEX(index);
      if (index < 0 || index >= MAX_ACS_GLOBAL_VARS) Host_Error("invalid ACS global array number in save file");
      GlobalArrays[index].Serialise(Strm);
    }
  } else {
    for (int i = 0; i < MAX_ACS_GLOBAL_VARS; ++i) {
      Strm << STRM_INDEX(i);
      GlobalArrays[i].Serialise(Strm);
    }
  }

  // store
  Strm << Store;
}


//==========================================================================
//
//  Script ACS methods
//
//==========================================================================
IMPLEMENT_FUNCTION(VLevel, StartACS) {
  int num, map, arg1, arg2, arg3;
  VEntity *activator;
  line_t *line;
  int side;
  bool Always, WantResult;
  vobjGetParamSelf(num, map, arg1, arg2, arg3, activator, line, side, Always, WantResult);
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in VLevel::StartACS"); }
  int res = 0;
  //fprintf(stderr, "000: activator=<%s>; line=%p; side=%d\n", (activator ? activator->GetClass()->GetName() : "???"), line, side);
  //GCon->Logf("StartACS: num=%d; map=%d; arg1=%d; arg2=%d; arg3=%d", num, map, arg1, arg2, arg3);
  bool br = Self->Acs->Start(num, map, arg1, arg2, arg3, 0, activator, line, side, Always, WantResult, false, &res);
  if (WantResult) RET_INT(res); else RET_INT(br ? 1 : 0);
}

IMPLEMENT_FUNCTION(VLevel, SuspendACS) {
  int number, map;
  vobjGetParamSelf(number, map);
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in VLevel::SuspendACS"); }
  RET_BOOL(Self->Acs->Suspend(number, map));
}

IMPLEMENT_FUNCTION(VLevel, TerminateACS) {
  int number, map;
  vobjGetParamSelf(number, map);
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in VLevel::TerminateACS"); }
  RET_BOOL(Self->Acs->Terminate(number, map));
}

IMPLEMENT_FUNCTION(VLevel, StartTypedACScripts) {
  int Type, Arg1, Arg2, Arg3;
  VEntity *Activator;
  bool Always, RunNow;
  vobjGetParamSelf(Type, Arg1, Arg2, Arg3, Activator, Always, RunNow);
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in VLevel::StartTypedACScripts"); }
  Self->Acs->StartTypedACScripts(Type, Arg1, Arg2, Arg3, Activator, Always, RunNow);
}

// bool RunACS (VEntity activator, int script, int map, int s_arg1, int s_arg2, int s_arg3, int s_arg4)
IMPLEMENT_FUNCTION(VLevel, RunACS) {
  VEntity *Activator;
  int Script, Map, Arg1, Arg2, Arg3, Arg4;
  vobjGetParamSelf(Activator, Script, Map, Arg1, Arg2, Arg3, Arg4);
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in VLevel::RunACS"); }
  if (Script < 0) { RET_BOOL(false); return; }
  RET_BOOL(Self->Acs->Start(Script, Map, Arg1, Arg2, Arg3, Arg4, Activator, nullptr/*line*/, 0/*side*/, false/*always*/, false/*wantresult*/, false/*net;k8:notsure*/));
}

// bool RunACSAlways (VEntity activator, int script, int map, int s_arg1, int s_arg2, int s_arg3, int s_arg4)
IMPLEMENT_FUNCTION(VLevel, RunACSAlways) {
  VEntity *Activator;
  int Script, Map, Arg1, Arg2, Arg3, Arg4;
  vobjGetParamSelf(Activator, Script, Map, Arg1, Arg2, Arg3, Arg4);
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in VLevel::RunACSAlways"); }
  if (Script < 0) { RET_BOOL(false); return; }
  RET_BOOL(Self->Acs->Start(Script, Map, Arg1, Arg2, Arg3, Arg4, Activator, nullptr/*line*/, 0/*side*/, true/*always*/, false/*wantresult*/, false/*net;k8:notsure*/));
}

// int RunACSWithResult (VEntity activator, int script, int s_arg1, int s_arg2, int s_arg3, int s_arg4)
IMPLEMENT_FUNCTION(VLevel, RunACSWithResult) {
  VEntity *Activator;
  int Script, Arg1, Arg2, Arg3, Arg4;
  vobjGetParamSelf(Activator, Script, Arg1, Arg2, Arg3, Arg4);
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in VLevel::RunACSWithResult"); }
  if (Script < 0) { RET_INT(0); return; }
  //fprintf(stderr, "001: activator=<%s>\n", (Activator ? Activator->GetClass()->GetName() : "???"));
  int res = 0;
  Self->Acs->Start(Script, 0/*Map*/, Arg1, Arg2, Arg3, Arg4, Activator, nullptr/*line*/, 0/*side*/, /*Script < 0*/true/*always*/, true/*wantresult*/, false/*net;k8:notsure*/, &res);
  RET_INT(res);
}

// bool RunNamedACS (VEntity activator, string script, int map, int s_arg1, int s_arg2, int s_arg3, int s_arg4)
IMPLEMENT_FUNCTION(VLevel, RunNamedACS) {
  VEntity *Activator;
  VStr Name;
  int Map, Arg1, Arg2, Arg3, Arg4;
  vobjGetParamSelf(Activator, Name, Map, Arg1, Arg2, Arg3, Arg4);
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in VLevel::RunNamedACS"); }
  //GCon->Logf("ACS: RunNamedACS000: script=<%s>; map=%d", *Name, Map);
  if (Name.length() == 0) { RET_BOOL(false); return; }
  VName Script = VName(*Name, VName::AddLower);
  if (Script == NAME_None) { RET_BOOL(false); return; }
  //GCon->Logf("ACS: RunNamedACS001: script=<%s>; map=%d", *Script, Map);
  RET_BOOL(Self->Acs->Start(-Script.GetIndex(), Map, Arg1, Arg2, Arg3, Arg4, Activator, nullptr/*line*/, 0/*side*/, /*Script < 0*/false/*always:wtf?*/, false/*wantresult*/, false/*net*/));
}

// bool RunNamedACSAlways (VEntity activator, string script, int map, int s_arg1, int s_arg2, int s_arg3, int s_arg4)
IMPLEMENT_FUNCTION(VLevel, RunNamedACSAlways) {
  VEntity *Activator;
  VStr Name;
  int Map, Arg1, Arg2, Arg3, Arg4;
  vobjGetParamSelf(Activator, Name, Map, Arg1, Arg2, Arg3, Arg4);
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in VLevel::RunNamedACSAlways"); }
  if (Name.length() == 0) { RET_BOOL(false); return; }
  VName Script = VName(*Name, VName::AddLower);
  if (Script == NAME_None) { RET_BOOL(false); return; }
  RET_BOOL(Self->Acs->Start(-Script.GetIndex(), Map, Arg1, Arg2, Arg3, Arg4, Activator, nullptr/*line*/, 0/*side*/, true/*always:wtf?*/, false/*wantresult*/, false/*net*/));
}

// int RunNamedACSWithResult (VEntity activator, string script, int s_arg1, int s_arg2, int s_arg3, int s_arg4)
IMPLEMENT_FUNCTION(VLevel, RunNamedACSWithResult) {
  VEntity *Activator;
  VStr Name;
  int Arg1, Arg2, Arg3, Arg4;
  vobjGetParamSelf(Activator, Name, Arg1, Arg2, Arg3, Arg4);
  if (!Self) { VObject::VMDumpCallStack(); Sys_Error("null self in VLevel::RunNamedACSWithResult"); }
  if (Name.length() == 0) { RET_INT(0); return; }
  VName Script = VName(*Name, VName::AddLower);
  //if (Script == NAME_None) { VObject::VMDumpCallStack(); GCon->Logf(NAME_Error, "no named script '%s'", *Name); }
  if (Script == NAME_None) { RET_INT(0); return; }
  int res = 0;
  Self->Acs->Start(-Script.GetIndex(), 0/*Map*/, Arg1, Arg2, Arg3, Arg4, Activator, nullptr/*line*/, 0/*side*/, /*Script < 0*/true/*always*/, true/*wantresult*/, false/*net;k8:notsure*/, &res);
  //{ VObject::VMDumpCallStack(); GCon->Logf(NAME_Debug, "ACS named script '%s': res=%d", *Name, res); }
  RET_INT(res);
}


//==========================================================================
//
//  Puke
//
//==========================================================================
COMMAND(Puke) {
  CMD_FORWARD_TO_SERVER();

  if (Args.Num() < 2) return;

  int Script = VStr::atoi(*Args[1]);
  if (Script == 0) return; // script 0 is special
  if (Script < 1 || Script > 65535) {
    GCon->Logf(NAME_Warning, "Puke: invalid script id: %d", Script);
    return;
  }

  int ScArgs[4];
  for (int i = 0; i < 4; ++i) {
    if (Args.Num() >= i+3) {
      ScArgs[i] = VStr::atoi(*Args[i+2]);
    } else {
      ScArgs[i] = 0;
    }
  }

  Player->Level->XLevel->Acs->Start(abs(Script), 0, ScArgs[0], ScArgs[1], ScArgs[2], ScArgs[3],
    GGameInfo->Players[0]->MO, nullptr, 0, Script < 0, false, true);
}


//==========================================================================
//
//  PukeName
//
//==========================================================================
COMMAND(PukeName) {
  CMD_FORWARD_TO_SERVER();

  if (!Player || sv.intermission || !GGameInfo || GGameInfo->NetMode < NM_Standalone) {
    GCon->Logf(NAME_Error, "cannot call named ACS script when no game is running!");
  }

  if (Args.Num() < 2 || Args[1].length() == 0) return;

  VName Script = VName(*Args[1], VName::AddLower);
  if (Script == NAME_None) return;

  int ScArgs[4];
  for (int i = 0; i < 4; ++i) {
    if (Args.Num() >= i+3) {
      ScArgs[i] = VStr::atoi(*Args[i+2]);
    } else {
      ScArgs[i] = 0;
    }
  }

  Player->Level->XLevel->Acs->Start(-Script.GetIndex(), 0, ScArgs[0], ScArgs[1], ScArgs[2], ScArgs[3],
    GGameInfo->Players[0]->MO, nullptr, 0, /*Script < 0*/false/*always:wtf?*/, false, true);
}
