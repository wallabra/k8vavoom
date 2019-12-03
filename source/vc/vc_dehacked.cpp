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
//**  Dehacked patch parsing
//**
//**************************************************************************
#include "gamedefs.h"


static TArray<VStr> cli_DehList;
static int cli_NoAnyDehacked = 0;

/*static*/ bool cliRegister_dehacked_args =
  VParsedArgs::RegisterFlagSet("-disable-dehacked", "disable all and any dehacked patches", &cli_NoAnyDehacked) &&
  GParsedArgs.RegisterCallback("-deh", "load next files as dehacked patch", [] (VArgs &args, int idx) -> int {
    for (++idx; !VParsedArgs::IsArgBreaker(args, idx); ++idx) {
      VStr mn = args[idx];
      if (!mn.isEmpty() && Sys_FileExists(mn)) {
        bool found = false;
        for (auto &&s : cli_DehList) {
          #ifdef _WIN32
          if (s.strEquCI(mn)) { found = true; break; }
          #else
          if (s.strEqu(mn)) { found = true; break; }
          #endif
        }
        if (!found) cli_DehList.append(mn);
      }
    }
    return idx;
  });


struct VCodePtrInfo {
  VStr Name;
  VMethod *Method;
};


struct VDehFlag {
  int Which;
  const char *Name;
  unsigned int Mask;
};


VCvarI Infighting("infighting", 0, "Allow infighting for all monster types?", CVAR_ServerInfo);


static char *Patch;
static char *PatchPtr;
static char *String;
static char *ValueString;
static int value;


static TArray<int> VanillaThingHeights;
static TArray<VName> Sprites;
static TArray<VClass *> EntClasses;
static TArray<bool> EntClassTouched; // by some dehacked definition
static TArray<VClass *> WeaponClasses;
static TArray<VClass *> AmmoClasses;
static TArray<VState *> States;
static TArray<VState *> CodePtrStates;
static TArray<VMethod *> StateActions;
static TArray<VCodePtrInfo> CodePtrs;
static TArray<VName> Sounds;

static VClass *GameInfoClass;
static VClass *DoomPlayerClass;
static VClass *BfgClass;
static VClass *HealthBonusClass;
static VClass *SoulsphereClass;
static VClass *MegaHealthClass;
static VClass *GreenArmorClass;
static VClass *BlueArmorClass;
static VClass *ArmorBonusClass;

static TArray<FReplacedString> SfxNames;
static TArray<FReplacedString> MusicNames;
static TArray<FReplacedString> SpriteNames;
static VLanguage *EngStrings;

static const VDehFlag DehFlags[] = {
  { 0, "SPECIAL", 0x00000001 },
  { 0, "SOLID", 0x00000002 },
  { 0, "SHOOTABLE", 0x00000004 },
  { 0, "NOSECTOR", 0x00000008 },
  { 0, "NOBLOCKMAP", 0x00000010 },
  { 0, "AMBUSH", 0x00000020 },
  { 0, "JUSTHIT", 0x00000040 },
  { 0, "JUSTATTACKED", 0x00000080 },
  { 0, "SPAWNCEILING", 0x00000100 },
  { 0, "NOGRAVITY", 0x00000200 },
  { 0, "DROPOFF", 0x00000400 },
  { 0, "PICKUP", 0x00000800 },
  { 0, "NOCLIP", 0x00001000 },
  { 0, "FLOAT", 0x00004000 },
  { 0, "TELEPORT", 0x00008000 },
  { 0, "MISSILE", 0x00010000 },
  { 0, "DROPPED", 0x00020000 },
  { 0, "SHADOW", 0x00040000 },
  { 0, "NOBLOOD", 0x00080000 },
  { 0, "CORPSE", 0x00100000 },
  { 0, "INFLOAT", 0x00200000 },
  { 0, "COUNTKILL", 0x00400000 },
  { 0, "COUNTITEM", 0x00800000 },
  { 0, "SKULLFLY", 0x01000000 },
  { 0, "NOTDMATCH", 0x02000000 },
  { 0, "TRANSLATION1", 0x04000000 },
  { 0, "TRANSLATION", 0x04000000 },
  { 0, "TRANSLATION2", 0x08000000 },
  { 0, "UNUSED1", 0x08000000 },
  { 0, "TRANSLUC25", 0x10000000 },
  { 0, "TRANSLUC75", 0x20000000 },
  { 0, "STEALTH", 0x40000000 },
  { 0, "UNUSED4", 0x40000000 },
  { 0, "TRANSLUCENT", 0x80000000 },
  { 0, "TRANSLUC50", 0x80000000 },
  { 1, "LOGRAV", 0x00000001 },
  { 1, "WINDTHRUST", 0x00000002 },
  { 1, "FLOORBOUNCE", 0x00000004 },
  { 1, "BLASTED", 0x00000008 },
  { 1, "FLY", 0x00000010 },
  { 1, "FLOORCLIP", 0x00000020 },
  { 1, "SPAWNFLOAT", 0x00000040 },
  { 1, "NOTELEPORT", 0x00000080 },
  { 1, "RIP", 0x00000100 },
  { 1, "PUSHABLE", 0x00000200 },
  { 1, "CANSLIDE", 0x00000400 },
  { 1, "ONMOBJ", 0x00000800 },
  { 1, "PASSMOBJ", 0x00001000 },
  { 1, "CANNOTPUSH", 0x00002000 },
  { 1, "THRUGHOST", 0x00004000 },
  { 1, "BOSS", 0x00008000 },
  { 1, "FIREDAMAGE", 0x00010000 },
  { 1, "NODMGTHRUST", 0x00020000 },
  { 1, "TELESTOMP", 0x00040000 },
  { 1, "DONTDRAW", 0x00080000 },
  { 1, "FLOATBOB", 0x00100000 },
  { 1, "IMPACT", 0x00200000 },
  { 1, "PUSHWALL", 0x00400000 },
  { 1, "MCROSS", 0x00800000 },
  { 1, "PCROSS", 0x01000000 },
  { 1, "CANTLEAVEFLOORPIC", 0x02000000 },
  { 1, "NONSHOOTABLE", 0x04000000 },
  { 1, "INVULNERABLE", 0x08000000 },
  { 1, "DORMANT", 0x10000000 },
  { 1, "ICEDAMAGE", 0x20000000 },
  { 1, "SEEKERMISSILE", 0x40000000 },
  { 1, "REFLECTIVE", 0x80000000 },
  //  Ignored flags
  { 0, "SLIDE", 0 },
  { 0, "UNUSED2", 0 },
  { 0, "UNUSED3", 0 },
};


static int dehCurrLine = 0;
static bool strEOL = false;
static bool bexMode = false;
static VStr dehFileName;


//==========================================================================
//
//  GetLine
//
//==========================================================================
static __attribute__((format(printf, 1, 2))) void Warning (const char *fmt, ...) {
  if (vcWarningsSilenced) return;
  va_list ap;
  va_start(ap, fmt);
  char *res = vavarg(fmt, ap);
  va_end(ap);
  if (!dehFileName.isEmpty()) {
    GLog.Logf(NAME_Warning, "%s:%d: %s", *dehFileName, dehCurrLine, res);
  } else {
    GLog.Logf(NAME_Warning, "DEHACKED: %s", res);
  }
}


//==========================================================================
//
//  GetLine
//
//==========================================================================
static bool GetLine () {
  do {
    ++dehCurrLine;

    if (!*PatchPtr) {
      String = nullptr;
      strEOL = true;
      return false;
    }

    String = PatchPtr;

    while (*PatchPtr && *PatchPtr != '\n') {
      if (*(vuint8 *)PatchPtr < ' ') *PatchPtr = ' ';
      ++PatchPtr;
    }

    if (*PatchPtr == '\n') {
      *PatchPtr = 0;
      ++PatchPtr;
    }

    while (*String && *(vuint8 *)String <= ' ') ++String;

    // comments
    if (*String == '#') { *String = 0; continue; }
    if (bexMode && String[0] == '/' && String[1] == '/') { *String = 0; continue; }

    char *End = String+VStr::Length(String);
    while (End > String && (vuint8)End[-1] <= ' ') {
      End[-1] = 0;
      --End;
    }

    // strip comments
    /*
    if (bexMode) {
      char *s = String;
      //char qch = 0;
      while (*s) {
        if (qch) {
          if (s[0] == '\\') {
            ++s;
            if (*s) ++s;
            continue;
          }
          if (s[0] == qch) qch = 0;
          ++s;
          continue;
        }
        // not in string
        if ((vuint8)s[0] <= ' ') { ++s; continue; }
        if (s[0] == '\'' || s[0] == '"') { qch = *s++; continue; }
        if (s[0] == '/' && s[1] == '/') { *s = 0; break; }
        ++s;
      }
    }
    */
  } while (!*String);

  strEOL = false;
  return true;
}


//==========================================================================
//
//  GetToken
//
//==========================================================================
/*
static VStr GetToken () {
  if (strEOL) return VStr();
  while (*String && *(vuint8 *)String <= ' ') ++String;
  if (!String[0]) { strEOL = true; return VStr(); }
  VStr res;
  while (*String && *(vuint8 *)String <= ' ') res += *String++;
  return res;
}
*/


//==========================================================================
//
//  ParseParam
//
//==========================================================================
static bool ParseParam () {
  char *val;

  if (!GetLine()) return false;

  val = strchr(String, '=');
  if (!val) return false;

  ValueString = val+1;
  while (*ValueString && *(vuint8 *)ValueString <= ' ') ++ValueString;
  value = VStr::atoi(ValueString);

  do {
    *val = 0;
    --val;
  } while (val >= String && *(vuint8 *)val <= ' ');

  return true;
}


//==========================================================================
//
//  GetClassFieldInt
//
//==========================================================================
static int GetClassFieldInt (VClass *Class, const char *FieldName) {
  VField *F = Class->FindFieldChecked(FieldName);
  int *Ptr = (int *)(Class->Defaults+F->Ofs);
  return *Ptr;
}


//==========================================================================
//
//  GetClassFieldClass
//
//==========================================================================
static VClass *GetClassFieldClass (VClass *Class, const char *FieldName) {
  VField *F = Class->FindFieldChecked(FieldName);
  VClass **Ptr = (VClass **)(Class->Defaults+F->Ofs);
  return *Ptr;
}


//==========================================================================
//
//  SetClassFieldInt
//
//==========================================================================
static void SetClassFieldInt (VClass *Class, const char *FieldName, int Value, int Idx=0) {
  VField *F = Class->FindFieldChecked(FieldName);
  vint32 *Ptr = (vint32 *)(Class->Defaults+F->Ofs);
  Ptr[Idx] = Value;
}


//==========================================================================
//
//  SetClassFieldByte
//
//==========================================================================
static void SetClassFieldByte (VClass *Class, const char *FieldName, vuint8 Value) {
  VField *F = Class->FindFieldChecked(FieldName);
  vuint8 *Ptr = Class->Defaults+F->Ofs;
  *Ptr = Value;
}


//==========================================================================
//
//  SetClassFieldFloat
//
//==========================================================================
static void SetClassFieldFloat (VClass *Class, const char *FieldName, float Value) {
  VField *F = Class->FindFieldChecked(FieldName);
  float *Ptr = (float *)(Class->Defaults+F->Ofs);
  *Ptr = Value;
}


//==========================================================================
//
//  SetClassFieldName
//
//==========================================================================
static void SetClassFieldName (VClass *Class, const char *FieldName, VName Value) {
  VField *F = Class->FindFieldChecked(FieldName);
  VName *Ptr = (VName *)(Class->Defaults+F->Ofs);
  *Ptr = Value;
}


//==========================================================================
//
//  SetClassFieldClass
//
//==========================================================================
static void SetClassFieldClass (VClass *Class, const char *FieldName, VClass *Value) {
  VField *F = Class->FindFieldChecked(FieldName);
  VClass **Ptr = (VClass **)(Class->Defaults+F->Ofs);
  *Ptr = Value;
}


//==========================================================================
//
//  SetClassFieldBool
//
//==========================================================================
static void SetClassFieldBool (VClass *Class, const char *FieldName, int Value) {
  VField *F = Class->FindFieldChecked(FieldName);
  vuint32 *Ptr = (vuint32 *)(Class->Defaults+F->Ofs);
  if (Value) {
    *Ptr |= F->Type.BitMask;
  } else {
    *Ptr &= ~F->Type.BitMask;
  }
}


//==========================================================================
//
//  ParseFlag
//
//==========================================================================
static void ParseFlag (VStr FlagName, int *Values, bool *Changed) {
  if (FlagName.Length() == 0) return;
  if (FlagName[0] >= '0' && FlagName[0] <= '9') {
    // clear flags that were not used by Doom engine as well as SLIDE flag which desn't exist anymore
    Values[0] = VStr::atoi(*FlagName)&0x0fffdfff;
    Changed[0] = true;
    return;
  }
  for (size_t i = 0; i < ARRAY_COUNT(DehFlags); ++i) {
    if (FlagName == DehFlags[i].Name) {
      Values[DehFlags[i].Which] |= DehFlags[i].Mask;
      Changed[DehFlags[i].Which] = true;
      return;
    }
  }
  Warning("Unknown flag '%s'", *FlagName);
}


//==========================================================================
//
//  ParseRenderStyle
//
//==========================================================================
static int ParseRenderStyle () {
  int RenderStyle = STYLE_Normal;
       if (!VStr::ICmp(ValueString, "STYLE_None")) RenderStyle = STYLE_None;
  else if (!VStr::ICmp(ValueString, "STYLE_Normal")) RenderStyle = STYLE_Normal;
  else if (!VStr::ICmp(ValueString, "STYLE_Fuzzy")) RenderStyle = STYLE_Fuzzy;
  else if (!VStr::ICmp(ValueString, "STYLE_SoulTrans")) RenderStyle = STYLE_SoulTrans;
  else if (!VStr::ICmp(ValueString, "STYLE_OptFuzzy")) RenderStyle = STYLE_OptFuzzy;
  else if (!VStr::ICmp(ValueString, "STYLE_Translucent")) RenderStyle = STYLE_Translucent;
  else if (!VStr::ICmp(ValueString, "STYLE_Add")) RenderStyle = STYLE_Add;
  else Warning("Bad render style '%s'", ValueString);
  return RenderStyle;
}


//==========================================================================
//
//  DoThingState
//
//==========================================================================
static void DoThingState (VClass *Ent, const char *StateLabel) {
  if (value < 0 || value >= States.length()) {
    Warning("Bad state '%d' for thing '%s'", value, (Ent ? Ent->GetName() : "<undefined>"));
  } else {
    Ent->SetStateLabel(StateLabel, States[value]);
  }
}


//==========================================================================
//
//  DoThingSound
//
//==========================================================================
static void DoThingSound (VClass *Ent, const char *FieldName) {
  //  If it's not a number, treat it like a sound defined in SNDINFO
       if (ValueString[0] < '0' || ValueString[0] > '9') SetClassFieldName(Ent, FieldName, ValueString);
  else if (value < 0 || value >= Sounds.length()) Warning("Bad sound index %d for '%s'", value, (Ent ? Ent->GetName() : "<undefined>"));
  else SetClassFieldName(Ent, FieldName, Sounds[value]);
}


//==========================================================================
//
//  ReadThing
//
//==========================================================================
static void ReadThing (int num) {
  if (num < 1 || num > EntClasses.length()) {
    Warning("Invalid thing num %d", num);
    while (ParseParam()) {}
    return;
  }
  bool gotHeight = false;
  bool gotSpawnCeiling = false;
  bool hasSomeDefine = false;
  VClass *Ent = EntClasses[num-1];
  while (ParseParam()) {
    if (VStr::ICmp(String, "ID #") == 0) {
      if (value) {
        // for info output
        vint32 oldid = -1;
        mobjinfo_t *nfo = VClass::FindMObjIdByClass(Ent, GGameInfo->GameFilterFlag);
        if (nfo) oldid = nfo->DoomEdNum;
        // actual work
        VClass::ReplaceMObjIdByClass(Ent, value, GGameInfo->GameFilterFlag);
        // info output
        nfo = VClass::FindMObjIdByClass(Ent, GGameInfo->GameFilterFlag);
        if (nfo && nfo->DoomEdNum == value) {
          GCon->Logf(NAME_Init, "DEHACKED: replaced `%s` DoomEdNum #%d with #%d", Ent->GetName(), oldid, value);
        }
      } else {
        VClass::RemoveMObjIdByClass(Ent, GGameInfo->GameFilterFlag);
      }
    } else if (!VStr::ICmp(String, "Hit points")) {
      hasSomeDefine = true;
      SetClassFieldInt(Ent, "Health", value);
      SetClassFieldInt(Ent, "GibsHealth", -value);
    } else if (!VStr::ICmp(String, "Reaction time")) {
      hasSomeDefine = true;
      SetClassFieldInt(Ent, "ReactionCount", value);
    } else if (!VStr::ICmp(String, "Missile damage")) {
      hasSomeDefine = true;
      SetClassFieldInt(Ent, "MissileDamage", value);
    } else if (!VStr::ICmp(String, "Width")) {
      hasSomeDefine = true;
      SetClassFieldFloat(Ent, "Radius", value/65536.0f);
    } else if (!VStr::ICmp(String, "Height")) {
      gotHeight = true; // height changed
      hasSomeDefine = true;
      SetClassFieldFloat(Ent, "Height", value/65536.0f);
    } else if (!VStr::ICmp(String, "Mass")) {
      hasSomeDefine = true;
      SetClassFieldFloat(Ent, "Mass", (value == 0x7fffffff ? 99999.0f : value));
    } else if (!VStr::ICmp(String, "Speed")) {
      hasSomeDefine = true;
      if (value < 100) {
        SetClassFieldFloat(Ent, "Speed", 35.0f*value);
      } else {
        SetClassFieldFloat(Ent, "Speed", 35.0f*value/65536.0f);
      }
    } else if (!VStr::ICmp(String, "Pain chance")) {
      hasSomeDefine = true;
      SetClassFieldFloat(Ent, "PainChance", value/256.0f);
    } else if (!VStr::ICmp(String, "Translucency")) {
      //hasSomeDefine = true;
      SetClassFieldFloat(Ent, "Alpha", value/65536.0f);
      SetClassFieldByte(Ent, "RenderStyle", STYLE_Translucent);
    } else if (!VStr::ICmp(String, "Alpha")) {
      //hasSomeDefine = true;
      SetClassFieldFloat(Ent, "Alpha", VStr::atof(ValueString, 1));
    } else if (!VStr::ICmp(String, "Render Style")) {
      //hasSomeDefine = true;
      SetClassFieldByte(Ent, "RenderStyle", ParseRenderStyle());
    } else if (!VStr::ICmp(String, "Scale")) {
      hasSomeDefine = true;
      float Scale = VStr::atof(ValueString, 1);
      Scale = midval(0.0001f, Scale, 256.0f);
      SetClassFieldFloat(Ent, "ScaleX", Scale);
      SetClassFieldFloat(Ent, "ScaleY", Scale);
    } else if (!VStr::ICmp(String, "Bits")) {
      hasSomeDefine = true;
      TArray<VStr> Flags;
      VStr Tmp(ValueString);
      Tmp.Split(" ,+|\t\f\r", Flags);
      int Values[2] = { 0, 0 };
      bool Changed[2] = { false, false };
      for (int i = 0; i < Flags.length(); ++i) ParseFlag(Flags[i].ToUpper(), Values, Changed);
      if (Changed[0]) {
        gotSpawnCeiling = ((Values[0]&0x00000100) != 0);
        SetClassFieldBool(Ent, "bSpecial", Values[0]&0x00000001);
        SetClassFieldBool(Ent, "bSolid", Values[0]&0x00000002);
        SetClassFieldBool(Ent, "bShootable", Values[0]&0x00000004);
        SetClassFieldBool(Ent, "bNoSector", Values[0]&0x00000008);
        SetClassFieldBool(Ent, "bNoBlockmap", Values[0]&0x00000010);
        SetClassFieldBool(Ent, "bAmbush", Values[0]&0x00000020);
        SetClassFieldBool(Ent, "bJustHit", Values[0]&0x00000040);
        SetClassFieldBool(Ent, "bJustAttacked", Values[0]&0x00000080);
        SetClassFieldBool(Ent, "bSpawnCeiling", Values[0]&0x00000100);
        SetClassFieldBool(Ent, "bNoGravity", Values[0]&0x00000200);
        SetClassFieldBool(Ent, "bDropOff", Values[0]&0x00000400);
        SetClassFieldBool(Ent, "bPickUp", Values[0]&0x00000800);
        SetClassFieldBool(Ent, "bColideWithThings", !(Values[0]&0x00001000));
        SetClassFieldBool(Ent, "bColideWithWorld", !(Values[0]&0x00001000));
        SetClassFieldBool(Ent, "bFloat", Values[0]&0x00004000);
        SetClassFieldBool(Ent, "bTeleport", Values[0]&0x00008000);
        SetClassFieldBool(Ent, "bMissile", Values[0]&0x00010000);
        SetClassFieldBool(Ent, "bDropped", Values[0]&0x00020000);
        SetClassFieldBool(Ent, "bShadow", Values[0]&0x00040000);
        SetClassFieldBool(Ent, "bNoBlood", Values[0]&0x00080000);
        SetClassFieldBool(Ent, "bCorpse", Values[0]&0x00100000);
        SetClassFieldBool(Ent, "bInFloat", Values[0]&0x00200000);
        SetClassFieldBool(Ent, "bCountKill", Values[0]&0x00400000);
        SetClassFieldBool(Ent, "bCountItem", Values[0]&0x00800000);
        SetClassFieldBool(Ent, "bSkullFly", Values[0]&0x01000000);
        SetClassFieldBool(Ent, "bNoDeathmatch", Values[0]&0x02000000);
        SetClassFieldBool(Ent, "bStealth", Values[0]&0x40000000);

        // set additional flags for missiles
        SetClassFieldBool(Ent, "bActivatePCross", Values[0]&0x00010000);
        SetClassFieldBool(Ent, "bNoTeleport", Values[0]&0x00010000);

        // set additional flags for monsters
        SetClassFieldBool(Ent, "bMonster", Values[0]&0x00400000);
        SetClassFieldBool(Ent, "bActivateMCross", Values[0]&0x00400000);
        // set push wall for both monsters and the player
        SetClassFieldBool(Ent, "bActivatePushWall", (Values[0]&0x00400000) || num == 1);
        // also set pass mobj flag
        SetClassFieldBool(Ent, "bPassMobj", num == 1 || (Values[0]&0x00400000) || (Values[0]&0x00010000));

        // translation
        SetClassFieldInt(Ent, "Translation", Values[0]&0x0c000000 ? (TRANSL_Standard<<TRANSL_TYPE_SHIFT)+((Values[0]&0x0c000000)>>26)-1 : 0);

        // alpha and render style
        SetClassFieldFloat(Ent, "Alpha", (Values[0]&0x00040000) ? 0.1f :
          (Values[0]&0x80000000) ? 0.5f :
          (Values[0]&0x10000000) ? 0.25f :
          (Values[0]&0x20000000) ? 0.75f : 1.0f);
        SetClassFieldByte(Ent, "RenderStyle", (Values[0]&0x00040000) ?
          STYLE_OptFuzzy : (Values[0]&0xb0000000) ? STYLE_Translucent : STYLE_Normal);
      }
      if (Changed[1]) {
        SetClassFieldBool(Ent, "bWindThrust", Values[1]&0x00000002);
        SetClassFieldBool(Ent, "bBlasted", Values[1]&0x00000008);
        SetClassFieldBool(Ent, "bFly", Values[1]&0x00000010);
        SetClassFieldBool(Ent, "bFloorClip", Values[1]&0x00000020);
        SetClassFieldBool(Ent, "bSpawnFloat", Values[1]&0x00000040);
        SetClassFieldBool(Ent, "bNoTeleport", Values[1]&0x00000080);
        SetClassFieldBool(Ent, "bRip", Values[1]&0x00000100);
        SetClassFieldBool(Ent, "bPushable", Values[1]&0x00000200);
        SetClassFieldBool(Ent, "bSlide", Values[1]&0x00000400);
        SetClassFieldBool(Ent, "bOnMobj", Values[1]&0x00000800);
        SetClassFieldBool(Ent, "bPassMobj", Values[1]&0x00001000);
        SetClassFieldBool(Ent, "bCannotPush", Values[1]&0x00002000);
        SetClassFieldBool(Ent, "bThruGhost", Values[1]&0x00004000);
        SetClassFieldBool(Ent, "bBoss", Values[1]&0x00008000);
        SetClassFieldBool(Ent, "bNoDamageThrust", Values[1]&0x00020000);
        SetClassFieldBool(Ent, "bTelestomp", Values[1]&0x00040000);
        SetClassFieldBool(Ent, "bInvisible", Values[1]&0x00080000);
        SetClassFieldBool(Ent, "bFloatBob", Values[1]&0x00100000);
        SetClassFieldBool(Ent, "bActivateImpact", Values[1]&0x00200000);
        SetClassFieldBool(Ent, "bActivatePushWall", Values[1]&0x00400000);
        SetClassFieldBool(Ent, "bActivateMCross", Values[1]&0x00800000);
        SetClassFieldBool(Ent, "bActivatePCross", Values[1]&0x01000000);
        SetClassFieldBool(Ent, "bCantLeaveFloorpic", Values[1]&0x02000000);
        SetClassFieldBool(Ent, "bNonShootable", Values[1]&0x04000000);
        SetClassFieldBool(Ent, "bInvulnerable", Values[1]&0x08000000);
        SetClassFieldBool(Ent, "bDormant", Values[1]&0x10000000);
        SetClassFieldBool(Ent, "bSeekerMissile", Values[1]&0x40000000);
        SetClassFieldBool(Ent, "bReflective", Values[1]&0x80000000);
        //  Things that used to be flags before.
        if (Values[1]&0x00000001) SetClassFieldFloat(Ent, "Gravity", 0.125f);
             if (Values[1]&0x00010000) SetClassFieldName(Ent, "DamageType", "Fire");
        else if (Values[1]&0x20000000) SetClassFieldName(Ent, "DamageType", "Ice");
        if (Values[1]&0x00000004) SetClassFieldByte(Ent, "BounceType", 1);
      }
    }
    // States
    else if (!VStr::ICmp(String, "Initial frame")) {
      hasSomeDefine = true;
      DoThingState(Ent, "Spawn");
    } else if (!VStr::ICmp(String, "First moving frame")) {
      hasSomeDefine = true;
      DoThingState(Ent, "See");
    } else if (!VStr::ICmp(String, "Close attack frame")) {
      // don't change melee state for players
      hasSomeDefine = true;
      if (num != 1) DoThingState(Ent, "Melee");
    } else if (!VStr::ICmp(String, "Far attack frame")) {
      // don't change missile state for players
      hasSomeDefine = true;
      if (num != 1) DoThingState(Ent, "Missile");
    } else if (!VStr::ICmp(String, "Injury frame")) {
      hasSomeDefine = true;
      DoThingState(Ent, "Pain");
    } else if (!VStr::ICmp(String, "Death frame")) {
      hasSomeDefine = true;
      DoThingState(Ent, "Death");
    } else if (!VStr::ICmp(String, "Exploding frame")) {
      hasSomeDefine = true;
      DoThingState(Ent, "XDeath");
    } else if (!VStr::ICmp(String, "Respawn frame")) {
      hasSomeDefine = true;
      DoThingState(Ent, "Raise");
    }
    // sounds
    else if (!VStr::ICmp(String, "Alert sound")) {
      DoThingSound(Ent, "SightSound");
    } else if (!VStr::ICmp(String, "Action sound")) {
      DoThingSound(Ent, "ActiveSound");
    } else if (!VStr::ICmp(String, "Attack sound")) {
      DoThingSound(Ent, "AttackSound");
    } else if (!VStr::ICmp(String, "Pain sound")) {
      DoThingSound(Ent, "PainSound");
    } else if (!VStr::ICmp(String, "Death sound")) {
      DoThingSound(Ent, "DeathSound");
    } else {
      Warning("Invalid mobj param '%s'", String);
    }
  }
  // reset heights for things hanging from the ceiling that don't specify a new height
  if (!gotHeight && gotSpawnCeiling) {
    if (((VEntity *)Ent->Defaults)->Height != VanillaThingHeights[num-1]) {
      Warning("forced vanilla thing height for '%s' (our: %d, vanilla: %d) (this is harmless)", *Ent->GetFullName(), (int)(((VEntity *)Ent->Defaults)->Height), VanillaThingHeights[num-1]);
    }
    ((VEntity *)Ent->Defaults)->Height = VanillaThingHeights[num-1];
  }
  if (hasSomeDefine) EntClassTouched[num-1] = true;
}


//==========================================================================
//
//  ReadSound
//
//==========================================================================
static void ReadSound (int) {
  while (ParseParam()) {
         if (!VStr::ICmp(String, "Offset"));     // lump name offset - can't handle
    else if (!VStr::ICmp(String, "Zero/One"));   // singularity - removed
    else if (!VStr::ICmp(String, "Value"));      // priority
    else if (!VStr::ICmp(String, "Zero 1"));     // lump num - can't be set
    else if (!VStr::ICmp(String, "Zero 2"));     // data pointer - can't be set
    else if (!VStr::ICmp(String, "Zero 3"));     // usefulness - removed
    else if (!VStr::ICmp(String, "Zero 4"));     // link - removed
    else if (!VStr::ICmp(String, "Neg. One 1")); // link pitch - removed
    else if (!VStr::ICmp(String, "Neg. One 2")); // link volume - removed
    else Warning("Invalid sound param '%s'", String);
  }
}


//==========================================================================
//
//  ReadState
//
//==========================================================================
static void ReadState (int num) {
  // check index
  if (num >= States.length() || num < 0) {
    Warning("Invalid state num %d", num);
    while (ParseParam()) {}
    return;
  }

  // state 0 is a special state
  if (num == 0) {
    while (ParseParam()) {}
    return;
  }

  //TODO: do we need to set `EntClassTouched` here?
  while (ParseParam()) {
    if (!VStr::ICmp(String, "Sprite number")) {
      if (value < 0 || value >= Sprites.length()) {
        Warning("Bad sprite index %d for frame #%d", value, num);
      } else {
        //GCon->Logf(NAME_Debug, "DEHACKED: frame #%d; prev sprite is '%s' (%d)", num, *States[num]->SpriteName, States[num]->SpriteIndex);
        States[num]->SpriteName = Sprites[value];
        States[num]->SpriteIndex = (Sprites[value] != NAME_None ? VClass::FindSprite(Sprites[value]) : 1);
        //GCon->Logf(NAME_Debug, "DEHACKED: frame #%d; NEW sprite is '%s' (%d)", num, *States[num]->SpriteName, States[num]->SpriteIndex);
      }
    } else if (!VStr::ICmp(String, "Sprite subnumber")) {
      if (value&0x8000) {
        value &= 0x7fff;
        value |= VState::FF_FULLBRIGHT;
      }
      States[num]->Frame = value;
    } else if (!VStr::ICmp(String, "Duration")) {
      States[num]->Time = (value < 0 ? value : value/35.0f);
    } else if (!VStr::ICmp(String, "Next frame")) {
      if (value >= States.length() || value < 0) {
        Warning("Invalid next state %d", value);
      } else {
        States[num]->NextState = States[value];
      }
    } else if (!VStr::ICmp(String, "Unknown 1")) {
      States[num]->Misc1 = value;
    } else if (!VStr::ICmp(String, "Unknown 2")) {
      States[num]->Misc2 = value;
    } else if (!VStr::ICmp(String, "Action pointer")) {
      Warning("Tried to set action pointer.");
    } else {
      Warning("Invalid state param '%s'", String);
    }
  }
}


//==========================================================================
//
//  ReadSpriteName
//
//==========================================================================
static void ReadSpriteName (int) {
  while (ParseParam()) {
    if (!VStr::ICmp(String, "Offset")) {} // can't handle
    else Warning("Invalid sprite name param '%s'", String);
  }
}


//==========================================================================
//
//  ReadAmmo
//
//==========================================================================
static void ReadAmmo (int num) {
  // check index
  if (num >= AmmoClasses.length() || num < 0) {
    Warning("Invalid ammo num %d", num);
    while (ParseParam()) {}
    return;
  }

  VClass *Ammo = AmmoClasses[num];
  while (ParseParam()) {
    if (!VStr::ICmp(String, "Max ammo")) {
      SetClassFieldInt(Ammo, "MaxAmount", value);
      SetClassFieldInt(Ammo, "BackpackMaxAmount", value*2);
    } else if (!VStr::ICmp(String, "Per ammo")) {
      SetClassFieldInt(Ammo, "Amount", value);
      SetClassFieldInt(Ammo, "BackpackAmount", value);
    } else {
      Warning("Invalid ammo param '%s'", String);
    }
  }

  // fix up amounts in derived classes
  for (VClass *C = VMemberBase::GClasses; C; C = C->LinkNext) {
    if (!C->IsChildOf(Ammo)) continue;
    if (C == Ammo) continue;
    SetClassFieldInt(C, "Amount", GetClassFieldInt(Ammo, "Amount")*5);
    SetClassFieldInt(C, "MaxAmount", GetClassFieldInt(Ammo, "MaxAmount"));
    SetClassFieldInt(C, "BackpackAmount", GetClassFieldInt(Ammo, "BackpackAmount")*5);
    SetClassFieldInt(C, "BackpackMaxAmount", GetClassFieldInt(Ammo, "BackpackMaxAmount"));
  }

  // fix up amounts in weapon classes
  for (int i = 0; i < WeaponClasses.length(); ++i) {
    VClass *C = WeaponClasses[i];
    if (GetClassFieldClass(C, "AmmoType1") == Ammo) SetClassFieldInt(C, "AmmoGive1", GetClassFieldInt(Ammo, "Amount")*2);
    if (GetClassFieldClass(C, "AmmoType2") == Ammo) SetClassFieldInt(C, "AmmoGive2", GetClassFieldInt(Ammo, "Amount")*2);
  }
}


//==========================================================================
//
//  DoWeaponState
//
//==========================================================================
static void DoWeaponState (VClass *Weapon, const char *StateLabel) {
  if (value < 0 || value >= States.length()) {
    Warning("Invalid weapon state %d for weapon '%s'", value, (Weapon ? Weapon->GetName() : "<undefined>"));
  } else {
    Weapon->SetStateLabel(StateLabel, States[value]);
  }
}


//==========================================================================
//
//  ReadWeapon
//
//==========================================================================
static void ReadWeapon (int num) {
  // check index
  if (num < 0 || num >= WeaponClasses.length()) {
    Warning("Invalid weapon num %d", num);
    while (ParseParam()) {}
    return;
  }

  VClass *Weapon = WeaponClasses[num];
  while (ParseParam()) {
    if (!VStr::ICmp(String, "Ammo type")) {
      if (value < AmmoClasses.length()) {
        SetClassFieldClass(Weapon, "AmmoType1", AmmoClasses[value]);
        SetClassFieldInt(Weapon, "AmmoGive1", GetClassFieldInt(AmmoClasses[value], "Amount")*2);
        if (GetClassFieldInt(Weapon, "AmmoUse1") == 0) SetClassFieldInt(Weapon, "AmmoUse1", 1);
      } else {
        SetClassFieldClass(Weapon, "AmmoType1", nullptr);
      }
    } else if (!VStr::ICmp(String, "Ammo use") || !VStr::ICmp(String, "Ammo per shot")) {
      SetClassFieldInt(Weapon, "AmmoUse1", value);
    } else if (!VStr::ICmp(String, "Min Ammo")) {
      // unused
    } else if (!VStr::ICmp(String, "Deselect frame")) {
      DoWeaponState(Weapon, "Select");
    } else if (!VStr::ICmp(String, "Select frame")) {
      DoWeaponState(Weapon, "Deselect");
    } else if (!VStr::ICmp(String, "Bobbing frame")) {
      DoWeaponState(Weapon, "Ready");
    } else if (!VStr::ICmp(String, "Shooting frame")) {
      DoWeaponState(Weapon, "Fire");
    } else if (!VStr::ICmp(String, "Firing frame")) {
      DoWeaponState(Weapon, "Flash");
    } else {
      Warning("Invalid weapon param '%s' for weapon '%s'", String, (Weapon ? Weapon->GetName() : "<undefined>"));
    }
  }
}


//==========================================================================
//
//  ReadPointer
//
//==========================================================================
static void ReadPointer (int num) {
  if (num < 0 || num >= CodePtrStates.length()) {
    Warning("Invalid pointer");
    while (ParseParam()) {}
    return;
  }

  while (ParseParam()) {
    if (!VStr::ICmp(String, "Codep Frame")) {
      if (value < 0 || value >= States.length()) {
        Warning("Invalid source state %d", value);
      } else {
        CodePtrStates[num]->Function = StateActions[value];
      }
    } else {
      Warning("Invalid pointer param '%s'", String);
    }
  }
}


//==========================================================================
//
//  ReadCodePtr
//
//==========================================================================
static void ReadCodePtr (int) {
  while (ParseParam()) {
    if (!VStr::NICmp(String, "Frame", 5) && (vuint8)String[5] <= ' ') {
      int Index = VStr::atoi(String+6);
      if (Index < 0 || Index >= States.length()) {
        Warning("Bad frame index %d", Index);
        continue;
      }

      VState *State = States[Index];
      if ((ValueString[0] == 'A' || ValueString[0] == 'a') && ValueString[1] == '_') ValueString += 2;

      bool Found = false;
      for (int i = 0; i < CodePtrs.length(); ++i) {
        if (!CodePtrs[i].Name.ICmp(ValueString)) {
          State->Function = CodePtrs[i].Method;
          Found = true;
          break;
        }
      }
      if (!Found) Warning("Invalid code pointer '%s'", ValueString);
    } else {
      Warning("Invalid code pointer param '%s'", String);
    }
  }
}


//==========================================================================
//
//  ReadCheats
//
//==========================================================================
static void ReadCheats (int) {
  // old cheat handling is removed
  while (ParseParam()) {}
}


//==========================================================================
//
//  DoPowerupColor
//
//==========================================================================
static void DoPowerupColor (const char *ClassName) {
  VClass *Power = VClass::FindClass(ClassName);
  if (!Power) {
    Warning("Can't find powerup class '%s'", ClassName);
    return;
  }

  int r, g, b;
  float a;
  if (sscanf(ValueString, "%d %d %d %f", &r, &g, &b, &a) != 4) {
    Warning("Bad powerup color '%s' for class '%s'", ValueString, ClassName);
    return;
  }
  r = midval(0, r, 255);
  g = midval(0, g, 255);
  b = midval(0, b, 255);
  a = midval(0.0f, a, 1.0f);
  SetClassFieldInt(Power, "BlendColor", (r<<16)|(g<<8)|b|int(a*255)<<24);
}


//==========================================================================
//
//  ReadMisc
//
//==========================================================================
static void ReadMisc (int) {
  while (ParseParam()) {
    if (!VStr::ICmp(String, "Initial Health")) {
      SetClassFieldInt(GameInfoClass, "INITIAL_HEALTH", value);
    } else if (!VStr::ICmp(String, "Initial Bullets")) {
      TArray<VDropItemInfo>& List = *(TArray<VDropItemInfo>*)(DoomPlayerClass->Defaults+DoomPlayerClass->FindFieldChecked("DropItemList")->Ofs);
      for (int i = 0; i < List.length(); ++i) if (List[i].Type && List[i].Type->Name == "Clip") List[i].Amount = value;
    } else if (!VStr::ICmp(String, "Max Health")) {
      SetClassFieldInt(HealthBonusClass, "MaxAmount", 2*value);
    } else if (!VStr::ICmp(String, "Max Armor")) {
      SetClassFieldInt(ArmorBonusClass, "MaxSaveAmount", value);
    } else if (!VStr::ICmp(String, "Green Armor Class")) {
      SetClassFieldInt(GreenArmorClass, "SaveAmount", 100*value);
      SetClassFieldFloat(GreenArmorClass, "SavePercent", value == 1 ? 1.0f/3.0f : 1.0f/2.0f);
    } else if (!VStr::ICmp(String, "Blue Armor Class")) {
      SetClassFieldInt(BlueArmorClass, "SaveAmount", 100*value);
      SetClassFieldFloat(BlueArmorClass, "SavePercent", value == 1 ? 1.0f/3.0f : 1.0f/2.0f);
    } else if (!VStr::ICmp(String, "Max Soulsphere")) {
      SetClassFieldInt(SoulsphereClass, "MaxAmount", value);
    } else if (!VStr::ICmp(String, "Soulsphere Health")) {
      SetClassFieldInt(SoulsphereClass, "Amount", value);
    } else if (!VStr::ICmp(String, "Megasphere Health")) {
      SetClassFieldInt(MegaHealthClass, "Amount", value);
      SetClassFieldInt(MegaHealthClass, "MaxAmount", value);
    } else if (!VStr::ICmp(String, "God Mode Health")) {
      SetClassFieldInt(GameInfoClass, "GOD_HEALTH", value);
    }
    else if (!VStr::ICmp(String, "IDFA Armor")) {} // cheat removed
    else if (!VStr::ICmp(String, "IDFA Armor Class")) {} // cheat removed
    else if (!VStr::ICmp(String, "IDKFA Armor")) {} // cheat removed
    else if (!VStr::ICmp(String, "IDKFA Armor Class")) {} // cheat removed
    else if (!VStr::ICmp(String, "BFG Cells/Shot")) {
      SetClassFieldInt(BfgClass, "AmmoUse1", value);
    } else if (!VStr::ICmp(String, "Monsters Infight")) {
      Infighting = value;
    } else if (!VStr::ICmp(String, "Monsters Ignore Each Other")) {
      Infighting = value ? -1 : 0;
    } else if (!VStr::ICmp(String, "Powerup Color Invulnerability")) {
      DoPowerupColor("PowerInvulnerable");
    } else if (!VStr::ICmp(String, "Powerup Color Berserk")) {
      DoPowerupColor("PowerStrength");
    } else if (!VStr::ICmp(String, "Powerup Color Invisibility")) {
      DoPowerupColor("PowerInvisibility");
    } else if (!VStr::ICmp(String, "Powerup Color Radiation Suit")) {
      DoPowerupColor("PowerIronFeet");
    } else if (!VStr::ICmp(String, "Powerup Color Infrared")) {
      DoPowerupColor("PowerLightAmp");
    } else if (!VStr::ICmp(String, "Powerup Color Tome of Power")) {
      DoPowerupColor("PowerWeaponLevel2");
    } else if (!VStr::ICmp(String, "Powerup Color Wings of Wrath")) {
      DoPowerupColor("PowerFlight");
    } else if (!VStr::ICmp(String, "Powerup Color Speed")) {
      DoPowerupColor("PowerSpeed");
    } else if (!VStr::ICmp(String, "Powerup Color Minotaur")) {
      DoPowerupColor("PowerMinotaur");
    } else if (!VStr::ICmp(String, "Rocket Explosion Style")) {
      SetClassFieldInt(GameInfoClass, "DehExplosionStyle", ParseRenderStyle());
    } else if (!VStr::ICmp(String, "Rocket Explosion Alpha")) {
      SetClassFieldFloat(GameInfoClass, "DehExplosionAlpha", VStr::atof(ValueString, 1));
    } else {
      Warning("Invalid misc '%s'", String);
    }
  }

  // 0xdd means "enable infighting"
       if (Infighting == 0xdd) Infighting = 1;
  else if (Infighting != -1) Infighting = 0;
}


//==========================================================================
//
//  ReadPars
//
//==========================================================================
static void ReadPars (int) {
  while (GetLine()) {
    if (strchr(String, '=')) {
      Warning("Unknown key in Pars section (%s)", String);
      continue;
    }
    TArray<VStr> cmda;
    VStr line = VStr(String);
    line.splitOnBlanks(cmda);
    //GLog.Logf(":::<%s>\n=== len: %d", *line, cmda.length()); for (int f = 0; f < cmda.length(); ++f) GLog.Logf("  %d: <%s>", f, *cmda[f]);
    if (cmda.length() < 1) return;
    if (cmda[0].ICmp("par") != 0) return;
    if (cmda.length() < 3 || cmda.length() > 4) {
      Warning("Bad par time");
      continue;
    }
    int nums[4];
    bool ok = true;
    memset(nums, 0, sizeof(nums));
    for (int f = 1; f < cmda.length(); ++f) {
      if (!cmda[f].convertInt(&nums[f]) || nums[f] < 0) {
        Warning("Bad par time");
        ok = false;
        break;
      }
    }
    if (!ok) continue;
    if (cmda.length() == 4) {
      VName MapName = va("e%dm%d", nums[1], nums[2]);
      P_SetParTime(MapName, nums[3]);
    } else {
      VName MapName = va("map%02d", nums[1]%100);
      P_SetParTime(MapName, nums[2]);
    }
  }
}


//==========================================================================
//
//  FindString
//
//==========================================================================
static void FindString (const char *oldStr, const char *newStr) {
  // sounds
  bool SoundFound = false;
  for (int i = 0; i < SfxNames.length(); ++i) {
    if (SfxNames[i].Old == oldStr) {
      SfxNames[i].New = newStr;
      SfxNames[i].Replaced = true;
      // continue, because other sounds can use the same sound
      SoundFound = true;
    }
  }
  if (SoundFound) return;

  // music
  bool SongFound = false;
  for (int i = 0; i < MusicNames.length(); ++i) {
    if (MusicNames[i].Old == oldStr) {
      MusicNames[i].New = newStr;
      MusicNames[i].Replaced = true;
      // there could be duplicates
      SongFound = true;
    }
  }
  if (SongFound) return;

  // sprite names
  for (int i = 0; i < SpriteNames.length(); ++i) {
    if (SpriteNames[i].Old == oldStr) {
      SpriteNames[i].New = newStr;
      SpriteNames[i].Replaced = true;
      return;
    }
  }

  VName Id = EngStrings->GetStringId(oldStr);
  if (Id != NAME_None) {
    GLanguage.ReplaceString(Id, newStr);
    return;
  }

  vuint32 hash = XXH32(oldStr, strlen(oldStr), 0x29a);
  if (hash != 0xde390ed6u && /* startup header line, doom2 */
      hash != 0x414bc626u && /* "modified" text, doom2 */
      hash != 0xd254973du && /* "Degreelessness Mode On" */
      hash != 0x41c08d13u)   /* "Degreelessness Mode Off" */
  {
    Warning("Not found old (0x%08x) \"%s\" new \"%s\"", hash, oldStr, newStr);
  }
}


//==========================================================================
//
//  ReadText
//
//==========================================================================
static void ReadText (int oldSize) {
  char *lenPtr;
  int newSize;
  char *oldStr;
  char *newStr;
  int len;

  lenPtr = strtok(nullptr, " \t");
  if (!lenPtr) return;
  newSize = VStr::atoi(lenPtr);

  oldStr = new char[oldSize+1];
  newStr = new char[newSize+1];

  len = 0;
  while (*PatchPtr && len < oldSize) {
    if (*PatchPtr == '\r') { ++PatchPtr; continue; }
    if (*PatchPtr == '\n') ++dehCurrLine;
    oldStr[len] = *PatchPtr;
    ++PatchPtr;
    ++len;
  }
  oldStr[len] = 0;

  len = 0;
  while (*PatchPtr && len < newSize) {
    if (*PatchPtr == '\r') { ++PatchPtr; continue; }
    if (*PatchPtr == '\n') ++dehCurrLine;
    newStr[len] = *PatchPtr;
    ++PatchPtr;
    ++len;
  }
  newStr[len] = 0;

  FindString(oldStr, newStr);

  delete[] oldStr;
  delete[] newStr;

  GetLine();
}


//==========================================================================
//
//  ReplaceSpecialChars
//
//==========================================================================
static VStr ReplaceSpecialChars (VStr &In) {
  VStr Ret;
  const char *pStr = *In;
  while (*pStr) {
    char c = *pStr++;
    if (c != '\\') {
      Ret += c;
    } else {
      switch (*pStr) {
        case 'n':
        case 'N':
          Ret += '\n';
          break;
        case 't':
        case 'T':
          Ret += '\t';
          break;
        case 'r':
        case 'R':
          Ret += '\r';
          break;
        case 'x':
        case 'X':
          c = 0;
          ++pStr;
          for (int i = 0; i < 2; ++i) {
            c <<= 4;
                 if (*pStr >= '0' && *pStr <= '9') c += *pStr-'0';
            else if (*pStr >= 'a' && *pStr <= 'f') c += 10+*pStr-'a';
            else if (*pStr >= 'A' && *pStr <= 'F') c += 10+*pStr-'A';
            else break;
            ++pStr;
          }
          Ret += c;
          break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
          c = 0;
          for (int i = 0; i < 3; ++i) {
            c <<= 3;
            if (*pStr >= '0' && *pStr <= '7') c += *pStr-'0'; else break;
            ++pStr;
          }
          Ret += c;
          break;
        default:
          Ret += *pStr;
          break;
      }
      ++pStr;
    }
  }
  return Ret;
}


//==========================================================================
//
//  ReadStrings
//
//==========================================================================
static void ReadStrings (int) {
  while (ParseParam()) {
    VName Id = *VStr(String).ToLower();
    VStr Val;
    do {
      char *End = ValueString+VStr::Length(ValueString)-1;
      while (End >= ValueString && (vuint8)*End <= ' ') --End;
      End[1] = 0;
      if (End >= ValueString && *End == '\\') {
        *End = 0;
        Val += ValueString;
        ValueString = PatchPtr;
        while (*PatchPtr && *PatchPtr != '\n') ++PatchPtr;
        if (*PatchPtr == '\n') {
          ++dehCurrLine;
          *PatchPtr = 0;
          ++PatchPtr;
        }
        while (*ValueString && *ValueString <= ' ') ++ValueString;
      } else {
        Val += ValueString;
        ValueString = nullptr;
      }
    } while (ValueString && *ValueString);
    Val = ReplaceSpecialChars(Val);
    GLanguage.ReplaceString(Id, Val);
  }
}


//==========================================================================
//
//  LoadDehackedFile
//
//==========================================================================
static void LoadDehackedFile (VStream *Strm) {
  dehCurrLine = 0;
  Patch = new char[Strm->TotalSize()+1];
  Strm->Serialise(Patch, Strm->TotalSize());
  Patch[Strm->TotalSize()] = 0;
  delete Strm;
  Strm = nullptr;
  PatchPtr = Patch;

  if (!VStr::NCmp(Patch, "Patch File for DeHackEd v", 25)) {
    if (Patch[25] < '3') {
      delete[] Patch;
      Patch = nullptr;
      Warning("DeHackEd patch is too old");
      return;
    }
    GetLine();
    int DVer = -1;
    int PFmt = -1;
    while (ParseParam()) {
           if (!VStr::ICmp(String, "Doom version")) DVer = value;
      else if (!VStr::ICmp(String, "Patch format")) PFmt = value;
      else Warning("Unknown parameter '%s'", String);
    }
    if (!String || DVer == -1 || PFmt == -1) {
      delete[] Patch;
      Patch = nullptr;
      Warning("Not a DeHackEd patch file");
      return;
    }
  } else {
    Warning("Missing DeHackEd header, assuming BEX file");
    bexMode = true;
    GetLine();
  }

  while (String) {
    char *Section = strtok(String, " \t");
    if (!Section) { GetLine(); continue; }

    char *numStr = strtok(nullptr, " \t");
    int i = 0;
    if (numStr) i = VStr::atoi(numStr);

         if (!VStr::ICmp(Section, "Thing")) ReadThing(i);
    else if (!VStr::ICmp(Section, "Sound")) ReadSound(i);
    else if (!VStr::ICmp(Section, "Frame")) ReadState(i);
    else if (!VStr::ICmp(Section, "Sprite")) ReadSpriteName(i);
    else if (!VStr::ICmp(Section, "Ammo")) ReadAmmo(i);
    else if (!VStr::ICmp(Section, "Weapon")) ReadWeapon(i);
    else if (!VStr::ICmp(Section, "Pointer")) ReadPointer(i);
    else if (!VStr::ICmp(Section, "Cheat")) ReadCheats(i);
    else if (!VStr::ICmp(Section, "Misc")) ReadMisc(i);
    else if (!VStr::ICmp(Section, "Text")) ReadText(i);
    else if (!VStr::ICmp(Section, "[Strings]")) ReadStrings(i);
    else if (!VStr::ICmp(Section, "[Pars]")) ReadPars(i);
    else if (!VStr::ICmp(Section, "[CodePtr]")) ReadCodePtr(i);
    else if (!VStr::ICmp(Section, "Include")) {
      VStr LumpName = numStr;
      int Lump = W_CheckNumForFileName(LumpName);
      // check WAD lump only if it's no longer than 8 characters and has no path separator
      if (Lump < 0 && LumpName.Length() <= 8 && LumpName.IndexOf('/') < 0) {
        Lump = W_CheckNumForName(VName(*LumpName, VName::AddLower8));
      }
      if (Lump < 0) {
        Warning("Lump '%s' not found", *LumpName);
      } else {
        char *SavedPatch = Patch;
        char *SavedPatchPtr = PatchPtr;
        char *SavedString = String;
        LoadDehackedFile(W_CreateLumpReaderNum(Lump));
        Patch = SavedPatch;
        PatchPtr = SavedPatchPtr;
        String = SavedString;
      }
      GetLine();
    } else {
      Warning("Don't know how to handle \"%s\"", String);
      GetLine();
    }
  }
  delete[] Patch;
  Patch = nullptr;
}


//==========================================================================
//
//  LoadDehackedDefinitions
//
//==========================================================================
static void LoadDehackedDefinitions () {
  // open dehinfo script
  VStream *Strm = FL_OpenFileReadBaseOnly("dehinfo.txt");
  if (!Strm) Sys_Error("dehinfo.txt is required to parse dehacked patches");

  VScriptParser *sc = new VScriptParser("dehinfo.txt", Strm);

  // read sprite names
  sc->Expect("sprites");
  sc->Expect("{");
  while (!sc->Check("}")) {
    sc->ExpectString();
    if (sc->String.Length() != 4) sc->Error("Sprite name must be 4 characters long");
    Sprites.Append(VName(*sc->String, VName::AddLower)); //k8: was upper, why? sprite names are lowercased
  }

  // read states
  sc->Expect("states");
  sc->Expect("{");
  States.Append(nullptr);
  StateActions.Append(nullptr);
  VState **StatesTail = &VClass::FindClass("Entity")->NetStates;
  while (*StatesTail) StatesTail = &(*StatesTail)->NetNext;
  while (!sc->Check("}")) {
    // class name
    sc->ExpectString();
    VClass *StatesClass = VClass::FindClass(*sc->String);
    if (!StatesClass) Sys_Error("No such class %s", *sc->String);
    // starting state specifier
    VState *S = nullptr;
    VState **pState = nullptr;
    if (sc->Check("First")) {
      S = StatesClass->NetStates;
      pState = &StatesClass->NetStates;
    } else {
      sc->ExpectString();
      VStateLabel *Lbl = StatesClass->FindStateLabel(*sc->String);
      if (!Lbl) sc->Error(va("No such state `%s` in class `%s`", *sc->String, StatesClass->GetName()));
      S = Lbl->State;
      pState = &StatesClass->NetStates;
      while (*pState && *pState != S) pState = &(*pState)->NetNext;
      if (!pState[0]) sc->Error("Bad state");
    }
    // number of states
    sc->ExpectNumber();
    //GCon->Logf(NAME_Debug, "STATES: class=`%s`; count=%d", StatesClass->GetName(), sc->Number);
    for (int i = 0; i < sc->Number; ++i) {
      if (!S) sc->Error(va("class `%s` doen't have that many states (%d, aborted at %d)", StatesClass->GetName(), sc->Number, i));
      States.Append(S);
      StateActions.Append(S->Function);
      // move net links to actor class
      *StatesTail = S;
      StatesTail = &S->NetNext;
      *pState = S->NetNext;
      S->NetNext = nullptr;
      S = S->Next;
    }
  }

  // read code pointer state mapping
  sc->Expect("code_pointer_states");
  sc->Expect("{");
  while (!sc->Check("}")) {
    sc->ExpectNumber();
    if (sc->Number < 0 || sc->Number >= States.length()) sc->Error(va("Bad state index %d", sc->Number));
    //GCon->Logf(NAME_Debug, "DEHACKED: cpsidx=%d; cpnum=%d; state=%s", CodePtrStates.length(), sc->Number, *States[sc->Number]->Loc.toString());
    CodePtrStates.Append(States[sc->Number]);
  }

  // read code pointers
  sc->Expect("code_pointers");
  sc->Expect("{");
  VCodePtrInfo &ANull = CodePtrs.Alloc();
  ANull.Name = "NULL";
  ANull.Method = nullptr;
  while (!sc->Check("}")) {
    sc->ExpectString();
    VStr Name = sc->String;
    sc->ExpectString();
    VStr ClassName = sc->String;
    sc->ExpectString();
    VStr MethodName = sc->String;
    VClass *Class = VClass::FindClass(*ClassName);
    if (Class == nullptr) sc->Error(va("No such class `%s`", *ClassName));
    VMethod *Method = Class->FindMethod(*MethodName);
    if (Method == nullptr) sc->Error(va("No such method `%s` in class `%s`", *MethodName, *ClassName));
    VCodePtrInfo &P = CodePtrs.Alloc();
    P.Name = Name;
    P.Method = Method;
  }

  // read sound names
  sc->Expect("sounds");
  sc->Expect("{");
  Sounds.Append(NAME_None);
  while (!sc->Check("}")) {
    sc->ExpectString();
    Sounds.Append(*sc->String);
  }

  // create list of thing classes
  sc->Expect("things");
  sc->Expect("{");
  while (!sc->Check("}")) {
    sc->ExpectString();
    VClass *C = VClass::FindClass(*sc->String);
    if (!C) sc->Error(va("No such class %s", *sc->String));
    EntClasses.Append(C);
    EntClassTouched.append(false);
  }
  vassert(EntClasses.length() == EntClassTouched.length());

  // create list of weapon classes
  sc->Expect("weapons");
  sc->Expect("{");
  while (!sc->Check("}")) {
    sc->ExpectString();
    VClass *C = VClass::FindClass(*sc->String);
    if (!C) sc->Error(va("No such class %s", *sc->String));
    WeaponClasses.Append(C);
  }

  // create list of ammo classes
  sc->Expect("ammo");
  sc->Expect("{");
  while (!sc->Check("}")) {
    sc->ExpectString();
    VClass *C = VClass::FindClass(*sc->String);
    if (!C) sc->Error(va("No such class %s", *sc->String));
    AmmoClasses.Append(C);
  }

  // set original thing heights
  sc->Expect("heights");
  sc->Expect("{");
  int HIdx = 0;
  while (!sc->Check("}")) {
    if (HIdx >= EntClasses.length()) sc->Error("Too many heights");
    sc->ExpectNumber();
    //((VEntity *)EntClasses[HIdx]->Defaults)->Height = sc->Number;
    VanillaThingHeights.append(sc->Number);
    //if (((VEntity *)EntClasses[HIdx]->Defaults)->Height != sc->Number) GCon->Logf(NAME_Debug, "  VanillaHeight = %d; // %s", sc->Number, EntClasses[HIdx]->GetName());
    #if 0
    {
      VEntity *e = (VEntity *)EntClasses[HIdx]->Defaults;
      if (!e->VanillaHeight) {
        if (e->Height != sc->Number) {
          GCon->Logf(NAME_Debug, "  VanillaHeight = %d; // %s  -- Height is %g", sc->Number, EntClasses[HIdx]->GetName(), e->Height);
        }
      }
    }
    #endif
    ++HIdx;
  }

  GameInfoClass = VClass::FindClass("MainGameInfo");
  DoomPlayerClass = VClass::FindClass("DoomPlayer");
  BfgClass = VClass::FindClass("BFG9000");
  HealthBonusClass = VClass::FindClass("HealthBonus");
  SoulsphereClass = VClass::FindClass("Soulsphere");
  MegaHealthClass = VClass::FindClass("MegasphereHealth");
  GreenArmorClass = VClass::FindClass("GreenArmor");
  BlueArmorClass = VClass::FindClass("BlueArmor");
  ArmorBonusClass = VClass::FindClass("ArmorBonus");

  delete sc;
  sc = nullptr;

  // get lists of strings to replace
  GSoundManager->GetSoundLumpNames(SfxNames);
  P_GetMusicLumpNames(MusicNames);
  VClass::GetSpriteNames(SpriteNames);
  EngStrings = new VLanguage();
  EngStrings->LoadStrings("en");
}


//==========================================================================
//
//  getFileWadName
//
//==========================================================================
static VStr getFileWadName (int fidx) {
  VStr pkname = W_FullPakNameByFile(fidx);
  if (!pkname.endsWithCI(".wad")) return VStr::EmptyString;
  auto lpos = pkname.lastIndexOf(':');
  if (lpos >= 0) pkname.chopLeft(lpos+1);
  return pkname.extractFileName();
}


//==========================================================================
//
//  FindDehackedLump
//
//  this is a hack for older wads that comes with "wadname.deh" in
//  their zip archive. it checks if the wad has built-in dehacked
//  lump, and if it isn't, looks for "wadname.deh" instead
//
//==========================================================================
static void FindDehackedLumps (TArray<int> &lumplist) {
  // build list of wad files
  struct WadInfo {
    VStr wadname;
    VStr dehname;
    int wadfidx;
    int dlump;
  };

  TArray<WadInfo> wadlist;
  TMap<VStrCI, int> dehmap; // key: "name.deh"; value: index in wadlist

  if (cli_NoExternalDeh <= 0) {
    for (int fnum = 0; fnum < W_NextMountFileId(); ++fnum) {
      VStr pkname = getFileWadName(fnum);
      if (pkname.isEmpty()) continue;
      VStr dhname = pkname.stripExtension();
      if (dhname.isEmpty()) continue;
      dhname += ".deh";
      // remove duplicates
      for (int f = 0; f < wadlist.length(); ++f) {
        if (wadlist[f].wadname.strEquCI(pkname)) {
          wadlist.removeAt(f);
          --f;
        }
      }
      WadInfo &wnfo = wadlist.alloc();
      wnfo.wadname = pkname;
      wnfo.dehname = dhname;
      wnfo.wadfidx = fnum;
      wnfo.dlump = -1;
      //GCon->Logf(NAME_Debug, "WAD: name=<%s>; deh=<%s>; fidx=%d", *wnfo.wadname, *wnfo.dehname, wnfo.wadfidx);
    }

    for (auto &&it : wadlist.itemsIdx()) dehmap.put(it.value().dehname, it.index());
  }

  // scan all files, put "dehacked" lumps, and "wadname.deh" lumps
  for (auto &&it : WadNSIterator(WADNS_Global)) {
    // normal dehacked lump?
    if (it.getName() == "dehacked") {
      //GCon->Logf(NAME_Debug, "dehacked lump: <%s>", *it.getFullName());
      if (cli_NoExternalDeh <= 0) {
        // remove this wad from the list for "wadname.deh" candidates
        VStr pkname = getFileWadName(it.getFile());
        if (pkname.length()) {
          VStr rname = pkname.stripExtension()+".deh";
          auto dl = dehmap.find(rname);
          if (dl) {
            //GCon->Logf(NAME_Debug, "removed wad <%s> from named deh search list", *wadlist[*dl].wadname);
            WadInfo &wi = wadlist[*dl];
            // find and remove it from registered lumps list
            // this is required, because supporting .deh lumps are found first (due to how archive mounting works)
            if (wi.dlump >= 0) {
              for (int f = 0; f < lumplist.length(); ++f) {
                if (lumplist[f] == wi.dlump) {
                  GCon->Logf(NAME_Init, "Dropping standalone '%s' for '%s' (replaced with '%s')", *W_FullLumpName(wi.dlump), *wi.wadname, *W_FullLumpName(it.lump));
                  lumplist.removeAt(f);
                  --f;
                }
              }
            }
            wi.wadfidx = -1;
            dehmap.remove(rname);
          }
        }
      }
      lumplist.append(it.lump);
      continue;
    }
    // "wadname.deh"?
    if (cli_NoExternalDeh <= 0) {
      VStr rname = it.getRealName();
      if (rname.endsWithCI(".deh")) {
        rname = rname.extractFileName();
        auto dl = dehmap.find(rname);
        if (dl) {
          // use the fact that wads from zips/pk3s are mounted after the respective zip/pk3
          // this way, it is safe to add this dehacked file
          WadInfo &wi = wadlist[*dl];
          if (wi.wadfidx > it.getFile() && wi.dlump < it.lump) {
            //GCon->Logf(NAME_Debug, "found named deh lump <%s> for wad <%s>", *it.getFullName(), *wi.wadname);
            //GCon->Logf(NAME_Debug, "found named deh lump <%s> for wad <%s> (wi.wadfidx=%d; wi.dlump=%d; fidx=%d; lump=%d)", *it.getFullName(), *wi.wadname, wi.wadfidx, wi.dlump, it.getFile(), it.lump);
            GCon->Logf(NAME_Init, "Found named deh lump \"%s\" for wad \"%s\".", *it.getFullName(), *wi.wadname);
            lumplist.append(it.lump);
            wi.dlump = it.lump;
          } else {
            //GCon->Logf(NAME_Debug, "skipped named deh lump <%s> for wad <%s> (wi.wadfidx=%d; wi.dlump=%d; fidx=%d; lump=%d)", *it.getFullName(), *wi.wadname, wi.wadfidx, wi.dlump, it.getFile(), it.lump);
          }
        } else {
          //GCon->Logf(NAME_Debug, "skipped named deh lump <%s>", *it.getFullName());
        }
      }
    }
  }
}


//==========================================================================
//
//  ProcessDehackedFiles
//
//==========================================================================
void ProcessDehackedFiles () {
  if (cli_NoAnyDehacked) return;

  TArray<int> dehlumps;
  FindDehackedLumps(dehlumps);
  if (cli_DehList.length() == 0 && dehlumps.length() == 0) return;

  LoadDehackedDefinitions();

  // parse dehacked patches
  for (auto &&dlump : dehlumps) {
    dehFileName = W_FullLumpName(dlump);
    GLog.Logf(NAME_Init, "Processing dehacked patch lump '%s'", *dehFileName);
    LoadDehackedFile(W_CreateLumpReaderNum(dlump));
  }

  for (auto &&dhs : cli_DehList) {
    GLog.Logf(NAME_Init, "Processing dehacked patch '%s'", *dhs);
    VStream *AStrm = FL_OpenSysFileRead(dhs);
    if (!AStrm) { GLog.Logf(NAME_Init, "No dehacked file '%s'", *dhs); continue; }
    dehFileName = dhs;
    LoadDehackedFile(AStrm);
  }

  dehFileName.clear();

  for (int i = 0; i < EntClasses.length(); ++i) {
    // set all classes to use old style pickup handling
    if (EntClassTouched[i]) {
      //GCon->Logf(NAME_Debug, "dehacked touched `%s`", EntClasses[i]->GetName());
      SetClassFieldBool(EntClasses[i], "bDehackedSpecial", true);
    }
  }
  SetClassFieldBool(GameInfoClass, "bDehacked", true);

  // do string replacement
  GSoundManager->ReplaceSoundLumpNames(SfxNames);
  P_ReplaceMusicLumpNames(MusicNames);
  VClass::ReplaceSpriteNames(SpriteNames);

  VClass::StaticReinitStatesLookup();

  // clean up
  Sprites.Clear();
  EntClasses.Clear();
  WeaponClasses.Clear();
  AmmoClasses.Clear();
  States.Clear();
  CodePtrStates.Clear();
  StateActions.Clear();
  CodePtrs.Clear();
  Sounds.Clear();
  SfxNames.Clear();
  MusicNames.Clear();
  SpriteNames.Clear();
  delete EngStrings;
  EngStrings = nullptr;
}
