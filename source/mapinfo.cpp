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
#include "gamedefs.h"
#include "sv_local.h"


static int cli_NoMapinfoPlrClasses = 0;
static int cli_NoZMapinfo = 0;
static int cli_MapperIsIdiot = 0;

/*static*/ bool cliRegister_mapinfo_args =
  VParsedArgs::RegisterFlagSet("-nomapinfoplayerclasses", "ignore player classes from MAPINFO", &cli_NoMapinfoPlrClasses) &&
  VParsedArgs::RegisterFlagSet("-nozmapinfo", "do not load ZMAPINFO", &cli_NoZMapinfo) &&
  VParsedArgs::RegisterFlagSet("-mapper-is-idiot", "!", &cli_MapperIsIdiot);


// ////////////////////////////////////////////////////////////////////////// //
// switches to C mode
struct SCParseModeSaver {
  VScriptParser *sc;
  bool oldCMode;
  bool oldEscape;

  SCParseModeSaver (VScriptParser *asc) : sc(asc) {
    oldCMode = sc->IsCMode();
    oldEscape = sc->IsEscape();
    sc->SetCMode(true);
    sc->SetEscape(true);
  }

  ~SCParseModeSaver () {
    sc->SetCMode(oldCMode);
    sc->SetEscape(oldEscape);
  }

  SCParseModeSaver (const SCParseModeSaver &);
  void operator = (const SCParseModeSaver &) const;
};


struct FMapSongInfo {
  VName MapName;
  VName SongName;
};

struct ParTimeInfo {
  VName MapName;
  int par;
};


struct SpawnEdFixup {
  VStr ClassName;
  int num;
  int flags;
  int special;
  int args[5];
};


// ////////////////////////////////////////////////////////////////////////// //
struct MapInfoCommand {
  const char *cmd;
  void (*handler) (VScriptParser *sc, bool newFormat, mapInfo_t *info, bool &HexenMode);
  MapInfoCommand *next;
};


static MapInfoCommand *mclist = nullptr;
static TMap<VStr, MapInfoCommand *> mcmap; // key is lowercase name


#define MAPINFOCMD(name_)  \
class MapInfoCommandImpl##name_ { \
public: \
  /*static*/ MapInfoCommand mci; \
  MapInfoCommandImpl##name_ (const char *aname) { \
    mci.cmd = aname; \
    mci.handler = &Handler; \
    mci.next = nullptr; \
    if (!mclist) { \
      mclist = &mci; \
    } else { \
      MapInfoCommand *last = mclist; \
      while (last->next) last = last->next; \
      last->next = &mci; \
    } \
  } \
  static void Handler (VScriptParser *sc, bool newFormat, mapInfo_t *info, bool &HexenMode); \
}; \
/*MapInfoCommand MapInfoCommandImpl##name_ mci;*/ \
MapInfoCommandImpl##name_ mpiprzintrnlz_mici_##name_(#name_); \
void MapInfoCommandImpl##name_::Handler (VScriptParser *sc, bool newFormat, mapInfo_t *info, bool &HexenMode)


//==========================================================================
//
//  ExpectBool
//
//==========================================================================
static bool ExpectBool (const char *optname, VScriptParser *sc) {
  if (sc->Check("true") || sc->Check("on") || sc->Check("tan")) return true;
  if (sc->CheckNumber()) return !!sc->Number;
  sc->Error(va("boolean value expected for option '%s'", optname));
  return false;
}


// ////////////////////////////////////////////////////////////////////////// //
VName P_TranslateMap (int map);
static void ParseMapInfo (VScriptParser *sc);


// ////////////////////////////////////////////////////////////////////////// //
static char miWarningBuf[16384];

__attribute__((unused)) __attribute__((format(printf, 2, 3))) static void miWarning (VScriptParser *sc, const char *fmt, ...) {
  va_list argptr;
  static char miWarningBuf[16384];
  va_start(argptr, fmt);
  vsnprintf(miWarningBuf, sizeof(miWarningBuf), fmt, argptr);
  va_end(argptr);
  if (sc) {
    GCon->Logf(NAME_Warning, "MAPINFO:%s: %s", *sc->GetLoc().toStringNoCol(), miWarningBuf);
  } else {
    GCon->Logf(NAME_Warning, "MAPINFO: %s", miWarningBuf);
  }
}


__attribute__((unused)) __attribute__((format(printf, 2, 3))) static void miWarning (const TLocation &loc, const char *fmt, ...) {
  va_list argptr;
  va_start(argptr, fmt);
  vsnprintf(miWarningBuf, sizeof(miWarningBuf), fmt, argptr);
  va_end(argptr);
  GCon->Logf(NAME_Warning, "MAPINFO:%s: %s", *loc.toStringNoCol(), miWarningBuf);
}


// ////////////////////////////////////////////////////////////////////////// //
static mapInfo_t DefaultMap;
static TArray<mapInfo_t> MapInfo;
static TArray<FMapSongInfo> MapSongList;
static VClusterDef DefaultClusterDef;
static TArray<VClusterDef> ClusterDefs;
static TArray<VEpisodeDef> EpisodeDefs;
static TArray<VSkillDef> SkillDefs;

static bool mapinfoParsed = false;
static TArray<ParTimeInfo> partimes; // not a hashmap, so i can use `ICmp`

static TArray<VName> MapInfoPlayerClasses;

static TMapDtor<int, SpawnEdFixup> SpawnNumFixups; // keyed by num
static TMapDtor<int, SpawnEdFixup> DoomEdNumFixups; // keyed by num


//==========================================================================
//
//  mapInfo_t::GetName
//
//==========================================================================
VStr mapInfo_t::GetName () const {
  return (Flags&VLevelInfo::LIF_LookupName ? GLanguage[*Name] : Name);
}


//==========================================================================
//
//  mapInfo_t::dump
//
//==========================================================================
void mapInfo_t::dump (const char *msg) const{
  if (msg && msg[0]) GCon->Logf("==== mapinfo: %s ===", msg); else GCon->Log("==== mapinfo ===");
  GCon->Logf("  LumpName: \"%s\"", *VStr(LumpName).quote());
  GCon->Logf("  Name: \"%s\"", *Name.quote());
  GCon->Logf("  LevelNum: %d", LevelNum);
  GCon->Logf("  Cluster: %d", Cluster);
  GCon->Logf("  WarpTrans: %d", WarpTrans);
  GCon->Logf("  NextMap: \"%s\"", *VStr(NextMap).quote());
  GCon->Logf("  SecretMap: \"%s\"", *VStr(SecretMap).quote());
  GCon->Logf("  SongLump: \"%s\"", *VStr(SongLump).quote());
  GCon->Logf("  Sky1Texture: %d", Sky1Texture);
  GCon->Logf("  Sky2Texture: %d", Sky2Texture);
  GCon->Logf("  Sky1ScrollDelta: %g", Sky1ScrollDelta);
  GCon->Logf("  Sky2ScrollDelta: %g", Sky2ScrollDelta);
  GCon->Logf("  SkyBox: \"%s\"", *VStr(SkyBox).quote());
  GCon->Logf("  FadeTable: \"%s\"", *VStr(FadeTable).quote());
  GCon->Logf("  Fade: 0x%08x", Fade);
  GCon->Logf("  OutsideFog: 0x%08x", OutsideFog);
  GCon->Logf("  Gravity: %g", Gravity);
  GCon->Logf("  AirControl: %g", AirControl);
  GCon->Logf("  Flags: 0x%08x", Flags);
  GCon->Logf("  Flags2: 0x%08x", Flags2);
  GCon->Logf("  TitlePatch: \"%s\"", *VStr(TitlePatch).quote());
  GCon->Logf("  ParTime: %d", ParTime);
  GCon->Logf("  SuckTime: %d", SuckTime);
  GCon->Logf("  HorizWallShade: %d", HorizWallShade);
  GCon->Logf("  VertWallShade: %d", VertWallShade);
  GCon->Logf("  Infighting: %d", Infighting);
  GCon->Logf("  RedirectType: \"%s\"", *VStr(RedirectType).quote());
  GCon->Logf("  RedirectMap: \"%s\"", *VStr(RedirectMap).quote());
  GCon->Logf("  ExitPic: \"%s\"", *VStr(ExitPic).quote());
  GCon->Logf("  EnterPic: \"%s\"", *VStr(EnterPic).quote());
  GCon->Logf("  InterMusic: \"%s\"", *VStr(InterMusic).quote());
  for (auto &&sac : SpecialActions) {
    GCon->Log("  --- special action ---");
    GCon->Logf("    TypeName: \"%s\"", *VStr(sac.TypeName).quote());
    GCon->Logf("    Special: %d (%d,%d,%d,%d,%d)", sac.Special, sac.Args[0], sac.Args[1], sac.Args[2], sac.Args[3], sac.Args[4]);
  }
}


//==========================================================================
//
//  P_SetupMapinfoPlayerClasses
//
//==========================================================================
void P_SetupMapinfoPlayerClasses () {
  if (cli_NoMapinfoPlrClasses > 0) return;
  if (MapInfoPlayerClasses.length() == 0) return;
  GCon->Logf("setting up %d player class%s from mapinfo...", MapInfoPlayerClasses.length(), (MapInfoPlayerClasses.length() != 1 ? "es" : ""));
  GGameInfo->PlayerClasses.Clear();
  for (int f = 0; f < MapInfoPlayerClasses.length(); ++f) {
    VClass *Class = VClass::FindClassNoCase(*MapInfoPlayerClasses[f]);
    if (!Class) {
      GCon->Logf(NAME_Warning, "No player class '%s'", *MapInfoPlayerClasses[f]);
      continue;
    }
    VClass *PPClass = VClass::FindClass("PlayerPawn");
    if (!PPClass) {
      GCon->Logf(NAME_Warning, "Can't find PlayerPawn class");
      return;
    }
    if (!Class->IsChildOf(PPClass)) {
      GCon->Logf(NAME_Warning, "'%s' is not a player pawn class", *MapInfoPlayerClasses[f]);
      continue;
    }
    GGameInfo->PlayerClasses.Append(Class);
  }
  if (GGameInfo->PlayerClasses.length() == 0) Sys_Error("no valid classes found in MAPINFO playerclass replacement");
}


//==========================================================================
//
//  appendNumFixup
//
//==========================================================================
static void appendNumFixup (TMapDtor<int, SpawnEdFixup> &arr, VStr className, int num, int flags=0, int special=0, int arg1=0, int arg2=0, int arg3=0, int arg4=0, int arg5=0) {
  SpawnEdFixup *fxp = arr.find(num);
  if (fxp) {
    fxp->ClassName = className;
    fxp->flags = flags;
    fxp->special = special;
    fxp->args[0] = arg1;
    fxp->args[1] = arg2;
    fxp->args[2] = arg3;
    fxp->args[3] = arg4;
    fxp->args[4] = arg5;
    return;
  }
  SpawnEdFixup fx;
  fx.num = num;
  fx.flags = flags;
  fx.special = special;
  fx.args[0] = arg1;
  fx.args[1] = arg2;
  fx.args[2] = arg3;
  fx.args[3] = arg4;
  fx.args[4] = arg5;
  fx.ClassName = className;
  arr.put(num, fx);
}


//==========================================================================
//
//  processNumFixups
//
//==========================================================================
static void processNumFixups (const char *errname, bool ismobj, TMapDtor<int, SpawnEdFixup> &fixups) {
  //GCon->Logf("fixing '%s'... (%d)", errname, fixups.count());
#if 0
  int f = 0;
  while (f < list.length()) {
    mobjinfo_t &nfo = list[f];
    SpawnEdFixup *fxp = fixups.find(nfo.DoomEdNum);
    if (fxp) {
      SpawnEdFixup fix = *fxp;
      VStr cname = fxp->ClassName;
      //GCon->Logf("    MAPINFO: class '%s' for %s got doomed num %d (got %d)", *cname, errname, fxp->num, nfo.DoomEdNum);
      fixups.del(nfo.DoomEdNum);
      /*
      if (cname.length() == 0 || cname.ICmp("none") == 0) {
        // remove it
        list.removeAt(f);
        continue;
      }
      */
      // set it
      VClass *cls;
      if (cname.length() == 0 || cname.ICmp("none") == 0) {
        cls = nullptr;
      } else {
        cls = VClass::FindClassNoCase(*cname);
        if (!cls) GCon->Logf(NAME_Warning, "MAPINFO: class '%s' for %s %d not found", *cname, errname, nfo.DoomEdNum);
      }
      nfo.Class = cls;
      nfo.GameFilter = GAME_Any;
      nfo.flags = fix.flags;
      nfo.special = fix.special;
      nfo.args[0] = fix.args[0];
      nfo.args[1] = fix.args[1];
      nfo.args[2] = fix.args[2];
      nfo.args[3] = fix.args[3];
      nfo.args[4] = fix.args[4];
    }
    ++f;
  }
#endif
  //GCon->Logf("  appending '%s'... (%d)", errname, fixups.count());
  // append new
  for (auto it = fixups.first(); it; ++it) {
    SpawnEdFixup *fxp = &it.getValue();
    if (fxp->num <= 0) continue;
    VStr cname = fxp->ClassName;
    //GCon->Logf("    MAPINFO0: class '%s' for %s got doomed num %d", *cname, errname, fxp->num);
    // set it
    VClass *cls;
    if (cname.length() == 0 || cname.ICmp("none") == 0) {
      cls = nullptr;
    } else {
      cls = VClass::FindClassNoCase(*cname);
      if (!cls) GCon->Logf(NAME_Warning, "MAPINFO: class '%s' for %s %d not found", *cname, errname, fxp->num);
    }
    //GCon->Logf("    MAPINFO1: class '%s' for %s got doomed num %d", *cname, errname, fxp->num);
    if (!cls) {
      if (ismobj) {
        VClass::RemoveMObjId(fxp->num, GGameInfo->GameFilterFlag);
      } else {
        VClass::RemoveScriptId(fxp->num, GGameInfo->GameFilterFlag);
      }
    } else {
      mobjinfo_t *nfo = (ismobj ? VClass::AllocMObjId(fxp->num, GGameInfo->GameFilterFlag, cls) : VClass::AllocScriptId(fxp->num, GGameInfo->GameFilterFlag, cls));
      if (nfo) {
        nfo->Class = cls;
        nfo->flags = fxp->flags;
        nfo->special = fxp->special;
        nfo->args[0] = fxp->args[0];
        nfo->args[1] = fxp->args[1];
        nfo->args[2] = fxp->args[2];
        nfo->args[3] = fxp->args[3];
        nfo->args[4] = fxp->args[4];
      }
    }
  }
  fixups.clear();
}


//==========================================================================
//
//  findSavedPar
//
//  returns -1 if not found
//
//==========================================================================
static int findSavedPar (VName map) {
  if (map == NAME_None) return -1;
  for (int f = partimes.length()-1; f >= 0; --f) {
    if (VStr::ICmp(*partimes[f].MapName, *map) == 0) return partimes[f].par;
  }
  return -1;
}


//==========================================================================
//
//  loadSkyTexture
//
//==========================================================================
static int loadSkyTexture (VScriptParser *sc, VName name, bool silent=false) {
  static TMapNC<VName, int> forceList;

  if (name == NAME_None) return GTextureManager.DefaultTexture;

  VName loname = VName(*name, VName::AddLower);
  auto tidp = forceList.find(loname);
  if (tidp) return *tidp;

  //int Tex = GTextureManager.NumForName(sc->Name8, TEXTYPE_Wall, false);
  //info->Sky1Texture = GTextureManager.NumForName(sc->Name8, TEXTYPE_Wall, false);
  int Tex = GTextureManager.CheckNumForName(name, TEXTYPE_SkyMap, true);
  if (Tex > 0) return Tex;

  Tex = GTextureManager.CheckNumForName(name, TEXTYPE_Wall, true);
  if (Tex >= 0) {
    forceList.put(loname, Tex);
    return Tex;
  }

  Tex = GTextureManager.CheckNumForName(name, TEXTYPE_WallPatch, false);
  if (Tex < 0) {
    int LumpNum = W_CheckNumForTextureFileName(*name);
    if (LumpNum >= 0) {
      Tex = GTextureManager.FindTextureByLumpNum(LumpNum);
      if (Tex < 0) {
        VTexture *T = VTexture::CreateTexture(TEXTYPE_WallPatch, LumpNum);
        if (!T) T = VTexture::CreateTexture(TEXTYPE_Any, LumpNum);
        if (T) {
          Tex = GTextureManager.AddTexture(T);
          T->Name = NAME_None;
        }
      }
    } else {
      LumpNum = W_CheckNumForName(name, WADNS_Patches);
      if (LumpNum < 0) LumpNum = W_CheckNumForTextureFileName(*name);
      if (LumpNum >= 0) {
        Tex = GTextureManager.AddTexture(VTexture::CreateTexture(TEXTYPE_WallPatch, LumpNum));
      } else {
        // DooM:Complete has some patches in "sprites/"
        LumpNum = W_CheckNumForName(name, WADNS_Sprites);
        if (LumpNum >= 0) Tex = GTextureManager.AddTexture(VTexture::CreateTexture(TEXTYPE_Any, LumpNum));
      }
    }
  }

  if (Tex < 0) Tex = GTextureManager.CheckNumForName(name, TEXTYPE_Any, false);

  if (Tex < 0) Tex = GTextureManager.AddPatch(name, TEXTYPE_WallPatch, true);

  if (Tex < 0) {
    if (silent) return -1;
    miWarning(sc, "sky '%s' not found; replaced with 'sky1'", *name);
    Tex = GTextureManager.CheckNumForName("sky1", TEXTYPE_SkyMap, true);
    if (Tex < 0) Tex = GTextureManager.CheckNumForName("sky1", TEXTYPE_Wall, true);
    forceList.put(loname, Tex);
    return Tex;
    //return GTextureManager.DefaultTexture;
  }

  forceList.put(loname, Tex);
  miWarning(sc, "force-loaded sky '%s'", *name);
  return Tex;
}


//==========================================================================
//
//  LoadMapInfoLump
//
//==========================================================================
static void LoadMapInfoLump (int Lump, bool doFixups=true) {
  GCon->Logf(NAME_Init, "mapinfo file: '%s'", *W_FullLumpName(Lump));
  ParseMapInfo(new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)));
  if (doFixups) {
    processNumFixups("DoomEdNum", true, DoomEdNumFixups);
    processNumFixups("SpawnNum", false, SpawnNumFixups);
  }
}


//==========================================================================
//
//  LoadAllMapInfoLumpsInFile
//
//  do this scanning fuckery, because some idiotic tools
//  loves duplicate lumps
//==========================================================================
static void LoadAllMapInfoLumpsInFile (int miLump, int zmiLump) {
  if (miLump < 0 && zmiLump < 0) return;
  VName milname;
  if (zmiLump >= 0) {
    milname = VName("zmapinfo", VName::Add);
  } else {
    vassert(miLump >= 0);
    zmiLump = miLump;
    milname = NAME_mapinfo;
  }
  vassert(zmiLump >= 0);
  int currFile = W_LumpFile(zmiLump);
  bool wasLoaded = false;
  for (; zmiLump >= 0; zmiLump = W_IterateNS(zmiLump, WADNS_Global)) {
    if (W_LumpFile(zmiLump) != currFile) break;
    if (W_LumpName(zmiLump) == milname) {
      wasLoaded = true;
      LoadMapInfoLump(zmiLump, false); // no fixups yet
    }
  }
  // do fixups if somethig was loaded
  if (wasLoaded) {
    processNumFixups("DoomEdNum", true, DoomEdNumFixups);
    processNumFixups("SpawnNum", false, SpawnNumFixups);
  }
}


//==========================================================================
//
//  InitMapInfo
//
//==========================================================================
void InitMapInfo () {
  // use "zmapinfo" if it is present?
  bool zmapinfoAllowed = (cli_NoZMapinfo >= 0);
  if (!zmapinfoAllowed) GCon->Logf(NAME_Init, "zmapinfo parsing disabled by user");
  int lastMapinfoFile = -1; // not seen yet
  int lastMapinfoLump = -1; // not seen yet
  int lastZMapinfoLump = -1; // not seen yet
  VName nameZMI = VName("zmapinfo", VName::Add);
  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    int currFile = W_LumpFile(Lump);
    // if we hit another file, load last seen [z]mapinfo lump
    if (currFile != lastMapinfoFile) {
      LoadAllMapInfoLumpsInFile(lastMapinfoLump, lastZMapinfoLump);
      // reset/update remembered lump indicies
      lastMapinfoFile = currFile;
      lastMapinfoLump = lastZMapinfoLump = -1; // not seen yet
    }
    // remember last seen [z]mapinfo lump
    if (lastMapinfoLump < 0 && W_LumpName(Lump) == NAME_mapinfo) lastMapinfoLump = Lump;
    if (zmapinfoAllowed && lastZMapinfoLump < 0 && W_LumpName(Lump) == nameZMI) lastZMapinfoLump = Lump;
  }
  // load last seen mapinfos
  LoadAllMapInfoLumpsInFile(lastMapinfoLump, lastZMapinfoLump);
  mapinfoParsed = true;

  for (int i = 0; i < MapInfo.Num(); ++i) {
    if (VStr(MapInfo[i].NextMap).StartsWith("&wt@")) {
      MapInfo[i].NextMap = P_TranslateMap(VStr::atoi(*MapInfo[i].NextMap+4));
    }
    if (VStr(MapInfo[i].SecretMap).StartsWith("&wt@")) {
      MapInfo[i].SecretMap = P_TranslateMap(VStr::atoi(*MapInfo[i].SecretMap+4));
    }
  }

  for (int i = 0; i < EpisodeDefs.Num(); ++i) {
    if (VStr(EpisodeDefs[i].Name).StartsWith("&wt@")) {
      EpisodeDefs[i].Name = P_TranslateMap(VStr::atoi(*EpisodeDefs[i].Name+4));
    }
    if (VStr(EpisodeDefs[i].TeaserName).StartsWith("&wt@")) {
      EpisodeDefs[i].TeaserName = P_TranslateMap(VStr::atoi(*EpisodeDefs[i].TeaserName+4));
    }
  }

  // set up default map info returned for maps that have not defined in MAPINFO
  memset((void *)&DefaultMap, 0, sizeof(DefaultMap));
  DefaultMap.Name = "Unnamed";
  DefaultMap.Sky1Texture = loadSkyTexture(nullptr, "sky1"); //GTextureManager.CheckNumForName("sky1", TEXTYPE_Wall, true, true);
  DefaultMap.Sky2Texture = DefaultMap.Sky1Texture;
  DefaultMap.FadeTable = NAME_colormap;
  DefaultMap.HorizWallShade = -8;
  DefaultMap.VertWallShade = 8;
  //GCon->Logf("*** DEFAULT MAP: Sky1Texture=%d", DefaultMap.Sky1Texture);

  // we don't need it anymore
  mcmap.clear();
}


//==========================================================================
//
//  SetMapDefaults
//
//==========================================================================
static void SetMapDefaults (mapInfo_t &Info) {
  Info.LumpName = NAME_None;
  Info.Name = VStr();
  Info.LevelNum = 0;
  Info.Cluster = 0;
  Info.WarpTrans = 0;
  Info.NextMap = NAME_None;
  Info.SecretMap = NAME_None;
  Info.SongLump = NAME_None;
  //Info.Sky1Texture = GTextureManager.DefaultTexture;
  //Info.Sky2Texture = GTextureManager.DefaultTexture;
  Info.Sky1Texture = loadSkyTexture(nullptr, "sky1"); //GTextureManager.CheckNumForName("sky1", TEXTYPE_Wall, true, true);
  Info.Sky2Texture = Info.Sky1Texture;
  //Info.Sky2Texture = GTextureManager.DefaultTexture;
  Info.Sky1ScrollDelta = 0;
  Info.Sky2ScrollDelta = 0;
  Info.SkyBox = NAME_None;
  Info.FadeTable = NAME_colormap;
  Info.Fade = 0;
  Info.OutsideFog = 0;
  Info.Gravity = 0;
  Info.AirControl = 0;
  Info.Flags = 0;
  Info.Flags2 = 0;
  Info.TitlePatch = NAME_None;
  Info.ParTime = 0;
  Info.SuckTime = 0;
  Info.HorizWallShade = -8;
  Info.VertWallShade = 8;
  Info.Infighting = 0;
  Info.SpecialActions.Clear();
  Info.RedirectType = NAME_None;
  Info.RedirectMap = NAME_None;
  Info.ExitPic = NAME_None;
  Info.EnterPic = NAME_None;
  Info.InterMusic = NAME_None;

  if (GGameInfo->Flags&VGameInfo::GIF_DefaultLaxMonsterActivation) {
    Info.Flags2 |= VLevelInfo::LIF2_LaxMonsterActivation;
  }
}


//==========================================================================
//
//  ParseNextMapName
//
//==========================================================================
static VName ParseNextMapName (VScriptParser *sc, bool HexenMode) {
  if (sc->CheckNumber()) {
    if (HexenMode) return va("&wt@%02d", sc->Number);
    return va("map%02d", sc->Number);
  }
  if (sc->Check("endbunny")) return "EndGameBunny";
  if (sc->Check("endcast")) return "EndGameCast";
  if (sc->Check("enddemon")) return "EndGameDemon";
  if (sc->Check("endchess")) return "EndGameChess";
  if (sc->Check("endunderwater")) return "EndGameUnderwater";
  if (sc->Check("endbuystrife")) return "EndGameBuyStrife";
  if (sc->Check("endpic")) {
    sc->Check(",");
    sc->ExpectName8();
    return va("EndGameCustomPic%s", *sc->Name8);
  }
  sc->ExpectString();
  if (sc->String.ToLower().StartsWith("endgame")) {
    switch (sc->String[7]) {
      case '1': return "EndGamePic1";
      case '2': return "EndGamePic2";
      case '3': return "EndGameBunny";
      case 'c': case 'C': return "EndGameCast";
      case 'w': case 'W': return "EndGameUnderwater";
      case 's': case 'S': return "EndGameStrife";
      default: return "EndGamePic3";
    }
  }
  return VName(*sc->String, VName::AddLower8);
}


//==========================================================================
//
//  DoCompatFlag
//
//==========================================================================
static void DoCompatFlag (VScriptParser *sc, mapInfo_t *info, int Flag) {
  int Set = 1;
  sc->Check("=");
  if (sc->CheckNumber()) Set = sc->Number;
  if (Flag) {
    if (Set) {
      info->Flags2 |= Flag;
    } else {
      info->Flags2 &= ~Flag;
    }
  }
}


//==========================================================================
//
//  skipUnimplementedCommand
//
//==========================================================================
static void skipUnimplementedCommand (VScriptParser *sc, bool wantArg) {
  VStr cmd = sc->String;
  if (sc->Check("=")) {
    miWarning(sc, "Unimplemented command '%s'", *cmd);
    sc->ExpectString();
    while (sc->Check(",")) {
      if (sc->Check("}")) { sc->UnGet(); break; }
      sc->ExpectString();
    }
  } else if (wantArg) {
    miWarning(sc, "Unimplemented old command '%s'", *cmd);
    sc->ExpectString();
  } else {
    miWarning(sc, "Unimplemented flag '%s'", *cmd);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(levelnum) {
  if (newFormat) sc->Expect("=");
  sc->ExpectNumber();
  info->LevelNum = sc->Number;
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(cluster) {
  if (newFormat) sc->Expect("=");
  sc->ExpectNumber();
  info->Cluster = sc->Number;
  if (P_GetClusterDef(info->Cluster) == &DefaultClusterDef) {
    // add empty cluster def if it doesn't exist yet
    VClusterDef &C = ClusterDefs.Alloc();
    C.Cluster = info->Cluster;
    C.Flags = 0;
    C.EnterText = VStr();
    C.ExitText = VStr();
    C.Flat = NAME_None;
    C.Music = NAME_None;
    if (HexenMode) C.Flags |= CLUSTERF_Hub;
  }
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(warptrans) {
  if (newFormat) sc->Expect("=");
  sc->ExpectNumber();
  info->WarpTrans = sc->Number;
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(next) {
  if (newFormat) sc->Expect("=");
  info->NextMap = ParseNextMapName(sc, HexenMode);
  // hack for "complete"
  if (sc->Check("{")) {
    info->NextMap = "endgamec";
    sc->SkipBracketed(true); // bracket eaten
  } else if (newFormat && sc->Check(",")) {
    sc->ExpectString();
    // check for more commas?
  }
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(secret) {
  if (newFormat) sc->Expect("=");
  info->SecretMap = ParseNextMapName(sc, HexenMode);
}

MAPINFOCMD(secretnext) {
  if (newFormat) sc->Expect("=");
  info->SecretMap = ParseNextMapName(sc, HexenMode);
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(sky1) {
  auto loc = sc->GetLoc();
  if (newFormat) sc->Expect("=");
  sc->ExpectName();
  //info->Sky1Texture = GTextureManager.NumForName(sc->Name, TEXTYPE_Wall, false);
  VName skbname = R_HasNamedSkybox(sc->String);
  if (skbname != NAME_None) {
    //k8: ok, this may be done to support sourceports that cannot into skyboxes
    miWarning(loc, "sky1 '%s' is actually a skybox (this is mostly harmless)", *sc->String);
    info->SkyBox = skbname;
    info->Sky1Texture = GTextureManager.DefaultTexture;
    info->Sky2Texture = GTextureManager.DefaultTexture;
    info->Sky1ScrollDelta = 0;
    info->Sky2ScrollDelta = 0;
    //GCon->Logf("MSG: using gz skybox '%s'", *skbname);
    if (!sc->IsAtEol()) {
      sc->Check(",");
      sc->ExpectFloatWithSign();
      if (HexenMode) sc->Float /= 256.0f;
      if (sc->Float != 0) miWarning(loc, "ignoring sky scroll for skybox '%s' (this is mostly harmless)", *skbname);
    }
  } else {
    info->SkyBox = NAME_None;
    info->Sky1Texture = loadSkyTexture(sc, sc->Name);
    info->Sky1ScrollDelta = 0;
    if (newFormat) {
      if (!sc->IsAtEol()) {
        sc->Check(",");
        sc->ExpectFloatWithSign();
        if (HexenMode) sc->Float /= 256.0f;
        info->Sky1ScrollDelta = sc->Float*35.0f;
      }
    } else {
      if (!sc->IsAtEol()) {
        sc->Check(",");
        sc->ExpectFloatWithSign();
        if (HexenMode) sc->Float /= 256.0f;
        info->Sky1ScrollDelta = sc->Float*35.0f;
      }
    }
  }
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(sky2) {
  if (newFormat) sc->Expect("=");
  sc->ExpectName8();
  //info->Sky2Texture = GTextureManager.NumForName(sc->Name8, TEXTYPE_Wall, false);
  //info->SkyBox = NAME_None; //k8:required or not???
  info->Sky2Texture = loadSkyTexture(sc, sc->Name8);
  info->Sky2ScrollDelta = 0;
  if (newFormat) {
    if (!sc->IsAtEol()) {
      sc->Check(",");
      sc->ExpectFloatWithSign();
      if (HexenMode) sc->Float /= 256.0f;
      info->Sky1ScrollDelta = sc->Float*35.0f;
    }
  } else {
    if (!sc->IsAtEol()) {
      sc->Check(",");
      sc->ExpectFloatWithSign();
      if (HexenMode) sc->Float /= 256.0f;
      info->Sky2ScrollDelta = sc->Float*35.0f;
    }
  }
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(skybox) {
  auto loc = sc->GetLoc();
  if (newFormat) sc->Expect("=");
  sc->ExpectString();
  info->Sky1ScrollDelta = 0;
  info->Sky2ScrollDelta = 0;
  VName skbname = R_HasNamedSkybox(sc->String);
  if (skbname != NAME_None) {
    info->SkyBox = skbname;
    info->Sky1Texture = GTextureManager.DefaultTexture;
    info->Sky2Texture = GTextureManager.DefaultTexture;
  } else {
    if (cli_MapperIsIdiot > 0) {
      miWarning(loc, "skybox '%s' not found (mapper is idiot)!", *sc->String);
    } else {
      sc->Error(va("skybox '%s' not found (this mapinfo is broken)", *sc->String));
    }
    info->SkyBox = NAME_None;
    info->Sky1Texture = loadSkyTexture(sc, VName(*sc->String, VName::AddLower8));
    info->Sky2Texture = info->Sky1Texture;
  }
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(skyrotate) {
  miWarning(sc, "\"skyrotate\" command is not supported yet");
  if (newFormat) sc->Expect("=");
  sc->ExpectFloatWithSign();
  if (sc->Check(",")) sc->ExpectFloatWithSign();
  if (sc->Check(",")) sc->ExpectFloatWithSign();
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(doublesky) { info->Flags |= VLevelInfo::LIF_DoubleSky; }
MAPINFOCMD(lightning) { info->Flags |= VLevelInfo::LIF_Lightning; }
MAPINFOCMD(forcenoskystretch) { info->Flags |= VLevelInfo::LIF_ForceNoSkyStretch; }
MAPINFOCMD(skystretch) { info->Flags &= ~VLevelInfo::LIF_ForceNoSkyStretch; }
MAPINFOCMD(map07special) { info->Flags |= VLevelInfo::LIF_Map07Special; }
MAPINFOCMD(baronspecial) { info->Flags |= VLevelInfo::LIF_BaronSpecial; }
MAPINFOCMD(cyberdemonspecial) { info->Flags |= VLevelInfo::LIF_CyberDemonSpecial; }
MAPINFOCMD(spidermastermindspecial) { info->Flags |= VLevelInfo::LIF_SpiderMastermindSpecial; }
MAPINFOCMD(minotaurspecial) { info->Flags |= VLevelInfo::LIF_MinotaurSpecial; }
MAPINFOCMD(dsparilspecial) { info->Flags |= VLevelInfo::LIF_DSparilSpecial; }
MAPINFOCMD(ironlichspecial) { info->Flags |= VLevelInfo::LIF_IronLichSpecial; }
MAPINFOCMD(specialaction_exitlevel) { info->Flags &= ~(VLevelInfo::LIF_SpecialActionOpenDoor|VLevelInfo::LIF_SpecialActionLowerFloor); }
MAPINFOCMD(specialaction_opendoor) { info->Flags &= ~VLevelInfo::LIF_SpecialActionLowerFloor; info->Flags |= VLevelInfo::LIF_SpecialActionOpenDoor; }
MAPINFOCMD(specialaction_lowerfloor) { info->Flags |= VLevelInfo::LIF_SpecialActionLowerFloor; info->Flags &= ~VLevelInfo::LIF_SpecialActionOpenDoor; }
MAPINFOCMD(specialaction_killmonsters) { info->Flags |= VLevelInfo::LIF_SpecialActionKillMonsters; }
MAPINFOCMD(intermission) { info->Flags &= ~VLevelInfo::LIF_NoIntermission; }
MAPINFOCMD(nointermission) { info->Flags |= VLevelInfo::LIF_NoIntermission; }
MAPINFOCMD(nosoundclipping) { /* ignored */ }
MAPINFOCMD(allowmonstertelefrags) { info->Flags |= VLevelInfo::LIF_AllowMonsterTelefrags; }
MAPINFOCMD(noallies) { info->Flags |= VLevelInfo::LIF_NoAllies; }
MAPINFOCMD(fallingdamage) { info->Flags &= ~(VLevelInfo::LIF_OldFallingDamage|VLevelInfo::LIF_StrifeFallingDamage); info->Flags |= VLevelInfo::LIF_FallingDamage; }
MAPINFOCMD(oldfallingdamage) { info->Flags &= ~(VLevelInfo::LIF_FallingDamage|VLevelInfo::LIF_StrifeFallingDamage); info->Flags |= VLevelInfo::LIF_OldFallingDamage; }
MAPINFOCMD(forcefallingdamage) { info->Flags &= ~(VLevelInfo::LIF_FallingDamage|VLevelInfo::LIF_StrifeFallingDamage); info->Flags |= VLevelInfo::LIF_OldFallingDamage; }
MAPINFOCMD(strifefallingdamage) { info->Flags &= ~(VLevelInfo::LIF_OldFallingDamage|VLevelInfo::LIF_FallingDamage); info->Flags |= VLevelInfo::LIF_StrifeFallingDamage; }
MAPINFOCMD(nofallingdamage) { info->Flags &= ~(VLevelInfo::LIF_OldFallingDamage|VLevelInfo::LIF_StrifeFallingDamage|VLevelInfo::LIF_FallingDamage); }
MAPINFOCMD(monsterfallingdamage) { info->Flags |= VLevelInfo::LIF_MonsterFallingDamage; }
MAPINFOCMD(nomonsterfallingdamage) { info->Flags &= ~VLevelInfo::LIF_MonsterFallingDamage; }
MAPINFOCMD(deathslideshow) { info->Flags |= VLevelInfo::LIF_DeathSlideShow; }
MAPINFOCMD(allowfreelook) { info->Flags &= ~VLevelInfo::LIF_NoFreelook; }
MAPINFOCMD(nofreelook) { info->Flags |= VLevelInfo::LIF_NoFreelook; }
MAPINFOCMD(allowjump) { info->Flags &= ~VLevelInfo::LIF_NoJump; }
MAPINFOCMD(nojump) { info->Flags |= VLevelInfo::LIF_NoJump; }
MAPINFOCMD(nocrouch) { info->Flags2 |= VLevelInfo::LIF2_NoCrouch; }
MAPINFOCMD(resethealth) { info->Flags2 |= VLevelInfo::LIF2_ResetHealth; }
MAPINFOCMD(resetinventory) { info->Flags2 |= VLevelInfo::LIF2_ResetInventory; }
MAPINFOCMD(resetitems) { info->Flags2 |= VLevelInfo::LIF2_ResetItems; }
MAPINFOCMD(noautosequences) { info->Flags |= VLevelInfo::LIF_NoAutoSndSeq; }
MAPINFOCMD(activateowndeathspecials) { info->Flags |= VLevelInfo::LIF_ActivateOwnSpecial; }
MAPINFOCMD(killeractivatesdeathspecials) { info->Flags &= ~VLevelInfo::LIF_ActivateOwnSpecial; }
MAPINFOCMD(missilesactivateimpactlines) { info->Flags |= VLevelInfo::LIF_MissilesActivateImpact; }
MAPINFOCMD(missileshootersactivetimpactlines) { info->Flags &= ~VLevelInfo::LIF_MissilesActivateImpact; }
MAPINFOCMD(filterstarts) { info->Flags |= VLevelInfo::LIF_FilterStarts; }
MAPINFOCMD(infiniteflightpowerup) { info->Flags |= VLevelInfo::LIF_InfiniteFlightPowerup; }
MAPINFOCMD(noinfiniteflightpowerup) { info->Flags &= ~VLevelInfo::LIF_InfiniteFlightPowerup; }
MAPINFOCMD(clipmidtextures) { info->Flags |= VLevelInfo::LIF_ClipMidTex; }
MAPINFOCMD(wrapmidtextures) { info->Flags |= VLevelInfo::LIF_WrapMidTex; }
MAPINFOCMD(keepfullinventory) { info->Flags |= VLevelInfo::LIF_KeepFullInventory; }
MAPINFOCMD(compat_shorttex) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatShortTex); }
MAPINFOCMD(compat_stairs) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatStairs); }
MAPINFOCMD(compat_limitpain) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatLimitPain); }
MAPINFOCMD(compat_nopassover) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatNoPassOver); }
MAPINFOCMD(compat_notossdrops) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatNoTossDrops); }
MAPINFOCMD(compat_useblocking) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatUseBlocking); }
MAPINFOCMD(compat_nodoorlight) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatNoDoorLight); }
MAPINFOCMD(compat_ravenscroll) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatRavenScroll); }
MAPINFOCMD(compat_soundtarget) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatSoundTarget); }
MAPINFOCMD(compat_dehhealth) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatDehHealth); }
MAPINFOCMD(compat_trace) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatTrace); }
MAPINFOCMD(compat_dropoff) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatDropOff); }
MAPINFOCMD(compat_boomscroll) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatBoomScroll); }
MAPINFOCMD(additive_scrollers) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatBoomScroll); }
MAPINFOCMD(compat_invisibility) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatInvisibility); }
MAPINFOCMD(compat_sectorsounds) { DoCompatFlag(sc, info, 0); }

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(noinfighting) { info->Infighting = -1; }
MAPINFOCMD(normalinfighting) { info->Infighting = 0; }
MAPINFOCMD(totalinfighting) { info->Infighting = 1; }

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(fadetable) {
  if (newFormat) sc->Expect("=");
  sc->ExpectName8();
  info->FadeTable = sc->Name8;
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(fade) {
  if (newFormat) sc->Expect("=");
  sc->ExpectString();
  info->Fade = M_ParseColor(*sc->String);
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(outsidefog) {
  if (newFormat) sc->Expect("=");
  sc->ExpectString();
  info->OutsideFog = M_ParseColor(*sc->String);
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(music) {
  if (newFormat) sc->Expect("=");
  //sc->ExpectName8();
  //info->SongLump = sc->Name8;
  sc->ExpectName();
  info->SongLump = sc->Name;
  const char *nn = *sc->Name;
  if (nn[0] == '$') {
    ++nn;
    if (nn[0] && GLanguage.HasTranslation(nn)) {
      info->SongLump = VName(*GLanguage[nn], VName::AddLower);
    }
  }
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(cdtrack) { if (newFormat) sc->Expect("="); sc->ExpectNumber(); /*info->CDTrack = sc->Number;*/ }
MAPINFOCMD(cd_start_track) { if (newFormat) sc->Expect("="); sc->ExpectNumber(); /*cd_NonLevelTracks[CD_STARTTRACK] = sc->Number;*/ }
MAPINFOCMD(cd_end1_track) { if (newFormat) sc->Expect("="); sc->ExpectNumber(); /*cd_NonLevelTracks[CD_END1TRACK] = sc->Number;*/ }
MAPINFOCMD(cd_end2_track) { if (newFormat) sc->Expect("="); sc->ExpectNumber(); /*cd_NonLevelTracks[CD_END2TRACK] = sc->Number;*/ }
MAPINFOCMD(cd_end3_track) { if (newFormat) sc->Expect("="); sc->ExpectNumber(); /*cd_NonLevelTracks[CD_END3TRACK] = sc->Number;*/ }
MAPINFOCMD(cd_intermission_track) { if (newFormat) sc->Expect("="); sc->ExpectNumber(); /*cd_NonLevelTracks[CD_INTERTRACK] = sc->Number;*/ }
MAPINFOCMD(cd_title_track) { if (newFormat) sc->Expect("="); sc->ExpectNumber(); /*cd_NonLevelTracks[CD_TITLETRACK] = sc->Number;*/ }
MAPINFOCMD(cdid) { skipUnimplementedCommand(sc, true); }

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(gravity) {
  if (newFormat) sc->Expect("=");
  sc->ExpectNumber();
  info->Gravity = (float)sc->Number;
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(aircontrol) {
  if (newFormat) sc->Expect("=");
  sc->ExpectFloat();
  info->AirControl = sc->Float;
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(titlepatch) {
  //FIXME: quoted string is a textual level name
  if (newFormat) sc->Expect("=");
  sc->ExpectName8Def(NAME_None);
  info->TitlePatch = sc->Name8;
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(par) {
  if (newFormat) sc->Expect("=");
  sc->ExpectNumber();
  info->ParTime = sc->Number;
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(sucktime) {
  if (newFormat) sc->Expect("=");
  sc->ExpectNumber();
  info->SuckTime = sc->Number;
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(evenlighting) {
  info->HorizWallShade = 0;
  info->VertWallShade = 0;
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(vertwallshade) {
  if (newFormat) sc->Expect("=");
  sc->ExpectNumber();
  info->VertWallShade = midval(-128, sc->Number, 127);
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(horizwallshade) {
  if (newFormat) sc->Expect("=");
  sc->ExpectNumber();
  info->HorizWallShade = midval(-128, sc->Number, 127);
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(specialaction) {
  if (newFormat) sc->Expect("=");
  VMapSpecialAction &A = info->SpecialActions.Alloc();
  //sc->SetCMode(true);
  sc->ExpectString();
  A.TypeName = *sc->String.ToLower();
  sc->Expect(",");
  sc->ExpectString();
  A.Special = 0;
  for (int i = 0; i < LineSpecialInfos.Num(); ++i) {
    if (!LineSpecialInfos[i].Name.ICmp(sc->String)) {
      A.Special = LineSpecialInfos[i].Number;
      break;
    }
  }
  if (!A.Special) miWarning(sc, "Unknown action special '%s'", *sc->String);
  memset(A.Args, 0, sizeof(A.Args));
  for (int i = 0; i < 5 && sc->Check(","); ++i) {
    sc->ExpectNumber();
    A.Args[i] = sc->Number;
  }
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(redirect) {
  if (newFormat) sc->Expect("=");
  sc->ExpectString();
  info->RedirectType = *sc->String.ToLower();
  info->RedirectMap = ParseNextMapName(sc, HexenMode);
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(strictmonsteractivation) {
  info->Flags2 &= ~VLevelInfo::LIF2_LaxMonsterActivation;
  info->Flags2 |= VLevelInfo::LIF2_HaveMonsterActivation;
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(laxmonsteractivation) {
  info->Flags2 |= VLevelInfo::LIF2_LaxMonsterActivation;
  info->Flags2 |= VLevelInfo::LIF2_HaveMonsterActivation;
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(interpic) {
  if (newFormat) sc->Expect("=");
  //sc->ExpectName8();
  sc->ExpectString();
  info->ExitPic = *sc->String.ToLower();
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(enterpic) {
  if (newFormat) sc->Expect("=");
  //sc->ExpectName8();
  sc->ExpectString();
  info->EnterPic = *sc->String.ToLower();
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(exitpic) {
  if (newFormat) sc->Expect("=");
  //sc->ExpectName8();
  sc->ExpectString();
  info->ExitPic = *sc->String.ToLower();
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(intermusic) {
  if (newFormat) sc->Expect("=");
  sc->ExpectString();
  info->InterMusic = *sc->String.ToLower();
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(background) {
  sc->Message("'background' mapinfo command is not supported");
  if (newFormat) sc->Expect("=");
  //sc->ExpectName8();
  sc->ExpectString();
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(airsupply) { skipUnimplementedCommand(sc, true); }
MAPINFOCMD(sndseq) { skipUnimplementedCommand(sc, true); }
MAPINFOCMD(sndinfo) { skipUnimplementedCommand(sc, true); }
MAPINFOCMD(soundinfo) { skipUnimplementedCommand(sc, true); }
MAPINFOCMD(bordertexture) { skipUnimplementedCommand(sc, true); }
MAPINFOCMD(f1) { skipUnimplementedCommand(sc, true); }
MAPINFOCMD(teamdamage) { skipUnimplementedCommand(sc, true); }
MAPINFOCMD(fogdensity) { skipUnimplementedCommand(sc, true); }
MAPINFOCMD(outsidefogdensity) { skipUnimplementedCommand(sc, true); }
MAPINFOCMD(skyfog) { skipUnimplementedCommand(sc, true); }
MAPINFOCMD(translator) { skipUnimplementedCommand(sc, true); skipUnimplementedCommand(sc, false); }
MAPINFOCMD(lightmode) { skipUnimplementedCommand(sc, true); }


//==========================================================================
//
//  FixSkyTexturesHack
//
//  another zdoom hack: check for "sky_maplump" sky texture
//
//==========================================================================
static void FixOneSkyTextureHack (VScriptParser *sc, mapInfo_t *info, int skynum, vint32 &tx) {
  if (tx < 1) return;

  //GCon->Logf(NAME_Debug, "map '%s': sky1 '%s'", *info->LumpName, *GTextureManager.GetTextureName(tx));
  VName skn = VName(*(VStr(*GTextureManager.GetTextureName(tx))+"_"+(*info->LumpName)), VName::AddLower);
  if (VStr::length(*skn) <= 8) {
    //GCon->Logf(NAME_Debug, "map '%s': trying sky1 '%s'", *info->LumpName, *skn);
    int tt = loadSkyTexture(sc, skn, true);
    if (tt > 0) {
      GCon->Logf(NAME_Debug, "map '%s': sky%d '%s' replaced with '%s'", *info->LumpName, skynum, *GTextureManager.GetTextureName(tx), *GTextureManager.GetTextureName(tt));
      tx = tt;
      return;
    }
  }

  skn = VName(*(VStr("sky_")+(*info->LumpName)), VName::AddLower);
  if (VStr::length(*skn) <= 8) {
    //GCon->Logf(NAME_Debug, "map '%s': trying sky1 '%s'", *info->LumpName, *skn);
    int tt = loadSkyTexture(sc, skn, true);
    if (tt > 0) {
      GCon->Logf(NAME_Debug, "map '%s': sky%d '%s' replaced with '%s'", *info->LumpName, skynum, *GTextureManager.GetTextureName(tx), *GTextureManager.GetTextureName(tt));
      tx = tt;
      return;
    }
  }
}


//==========================================================================
//
//  FixSkyTexturesHack
//
//  another zdoom hack: check for "sky_maplump" sky texture
//
//==========================================================================
static void FixSkyTexturesHack (VScriptParser *sc, mapInfo_t *info) {
  FixOneSkyTextureHack(sc, info, 1, info->Sky1Texture);
  //FixOneSkyTextureHack(sc, info, 2, info->Sky2Texture);
}


//==========================================================================
//
//  ParseMapCommon
//
//==========================================================================
static void ParseMapCommon (VScriptParser *sc, mapInfo_t *info, bool &HexenMode) {
  // build command map, if it is not built yet
  if (mcmap.length() == 0 && mclist) {
    for (MapInfoCommand *mcp = mclist; mcp; mcp = mcp->next) {
      VStr cn = VStr(mcp->cmd).toLowerCase().xstrip();
      if (cn.isEmpty()) Sys_Error("internal engine error: unnamed mapinfo command handler!");
      if (mcmap.put(cn, mcp)) Sys_Error("internal engine error: duplicate mapinfo command handler for '%s'!", mcp->cmd);
    }
  }

  bool newFormat = sc->Check("{");
  //if (newFormat) sc->SetCMode(true);
  // process optional tokens
  for (;;) {
    //sc->GetString(); sc->UnGet(); GCon->Logf(NAME_Debug, "%s: %s", *sc->GetLoc().toStringNoCol(), *sc->String);
    if (!sc->GetString()) break;
    auto mpp = mcmap.find(sc->String.toLowerCase());
    if (mpp) {
      (*(*mpp)->handler)(sc, newFormat, info, HexenMode);
    } else {
      //GCon->Logf(NAME_Debug, "%s: NOT FOUND cmd='%s' (new=%d)", *sc->GetLoc().toStringNoCol(), *sc->String, (int)newFormat);
      sc->UnGet();
      if (!newFormat) break;
      if (sc->Check("}")) break;
      //sc->Message(va("unknown mapinfo command '%s', skipping...", *sc->String));
      if (!sc->GetString()) break;
      skipUnimplementedCommand(sc, false); // don't force args, but skip them
      if (sc->Check("}")) break;
      continue;
    }
    /*
    if (sc->CheckStartsWith("compat_")) {
      GCon->Logf(NAME_Warning, "%s: mapdef '%s' is not supported yet", *sc->GetLoc().toStringNoCol(), *sc->String);
      sc->Check("=");
      sc->CheckNumber();
    }
    */
    // these are stubs for now
    //} else if (sc->Check("noinventorybar")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("allowcrouch")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("pausemusicinmenus")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("allowrespawn")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("teamplayon")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("teamplayoff")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("checkswitchrange")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("nocheckswitchrange")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("unfreezesingleplayerconversations")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("smoothlighting")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("Grinding_PolyObj")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("UsePlayerStartZ")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("spawnwithweaponraised")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("noautosavehint")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("PrecacheTextures")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("PrecacheSounds")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("PrecacheClasses")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("intermissionmusic")) { skipUnimplementedCommand(sc, false);
  }
  //if (newFormat) sc->SetCMode(false);

  // second sky defaults to first sky
  FixSkyTexturesHack(sc, info);
  if (info->Sky2Texture == GTextureManager.DefaultTexture) info->Sky2Texture = info->Sky1Texture;
  if (info->Flags&VLevelInfo::LIF_DoubleSky) GTextureManager.SetFrontSkyLayer(info->Sky1Texture);
}


//==========================================================================
//
//  ParseNameOrLookup
//
//==========================================================================
static void ParseNameOrLookup (VScriptParser *sc, vuint32 lookupFlag, VStr *name, vuint32 *flags, bool newStyle) {
  if (sc->Check("lookup")) {
    if (sc->GetString()) {
      if (sc->QuotedString || sc->String != ",") sc->UnGet();
    }
    //if (newStyle) sc->Check(",");
    *flags |= lookupFlag;
    sc->ExpectString();
    if (sc->String.length() > 1 && sc->String[0] == '$') {
      *name = VStr(*sc->String+1).ToLower();
    } else {
      *name = sc->String.ToLower();
    }
  } else {
    sc->ExpectString();
    if (sc->String.Length() > 1 && sc->String[0] == '$') {
      *flags |= lookupFlag;
      *name = VStr(*sc->String+1).ToLower();
    } else {
      *flags &= ~lookupFlag;
      *name = sc->String;
      if (lookupFlag == VLevelInfo::LIF_LookupName) return;
      if (newStyle) {
        while (!sc->AtEnd()) {
          if (!sc->Check(",")) break;
          sc->ExpectString();
          while (!sc->QuotedString) {
            if (sc->String == "}") { sc->UnGet(); break; } // stray comma
            if (sc->String != ",") { sc->UnGet(); sc->Error("comma expected"); break; }
            sc->ExpectString();
          }
          *name += "\n";
          *name += sc->String;
        }
      } else {
        while (!sc->AtEnd()) {
          sc->ExpectString();
          if (sc->Crossed) { sc->UnGet(); break; }
          while (!sc->QuotedString) {
            if (sc->String != ",") { sc->UnGet(); sc->Error("comma expected"); break; }
            sc->ExpectString();
          }
          *name += "\n";
          *name += sc->String;
        }
      }
      //GCon->Logf("COLLECTED: <%s>", **name);
    }
  }
}


//==========================================================================
//
//  ParseNameOrLookup
//
//==========================================================================
static void ParseNameOrLookup (VScriptParser *sc, vint32 lookupFlag, VStr *name, vint32 *flags, bool newStyle) {
  vuint32 lf = (vuint32)lookupFlag;
  vuint32 flg = (vuint32)*flags;
  ParseNameOrLookup(sc, lf, name, &flg, newStyle);
  *flags = (vint32)flg;
}


//==========================================================================
//
//  ParseMap
//
//==========================================================================
static void ParseMap (VScriptParser *sc, bool &HexenMode, mapInfo_t &Default) {
  mapInfo_t *info = nullptr;
  VName MapLumpName;
  if (sc->CheckNumber()) {
    // map number, for Hexen compatibility
    HexenMode = true;
    if (sc->Number < 1 || sc->Number > 99) sc->Error("Map number out or range");
    MapLumpName = va("map%02d", sc->Number);
  } else {
    // map name
    sc->ExpectName8();
    MapLumpName = sc->Name8;
  }

  // check for replaced map info
  bool replacement = false;
  for (int i = 0; i < MapInfo.Num(); ++i) {
    if (MapLumpName == MapInfo[i].LumpName) {
      info = &MapInfo[i];
      //GCon->Logf("replaced map '%s' (Sky1Texture=%d; default=%d)", *info->LumpName, info->Sky1Texture, Default.Sky1Texture);
      replacement = true;
      break;
    }
  }
  if (!info) info = &MapInfo.Alloc();

  // Copy defaults to current map definition
  info->LumpName = MapLumpName;
  if (!replacement) {
    info->LevelNum = Default.LevelNum;
    info->Cluster = Default.Cluster;
  }
  info->WarpTrans = Default.WarpTrans;
  if (!replacement) {
    info->NextMap = Default.NextMap;
    info->SecretMap = Default.SecretMap;
    info->SongLump = Default.SongLump;
  }
  if (!replacement) {
    info->Sky1Texture = Default.Sky1Texture;
    info->Sky2Texture = Default.Sky2Texture;
    info->Sky1ScrollDelta = Default.Sky1ScrollDelta;
    info->Sky2ScrollDelta = Default.Sky2ScrollDelta;
    info->SkyBox = Default.SkyBox;
  }
  info->FadeTable = Default.FadeTable;
  info->Fade = Default.Fade;
  info->OutsideFog = Default.OutsideFog;
  info->Gravity = Default.Gravity;
  info->AirControl = Default.AirControl;
  info->Flags = Default.Flags;
  info->Flags2 = Default.Flags2;
  info->TitlePatch = Default.TitlePatch;
  info->ParTime = Default.ParTime;
  info->SuckTime = Default.SuckTime;
  info->HorizWallShade = Default.HorizWallShade;
  info->VertWallShade = Default.VertWallShade;
  info->Infighting = Default.Infighting;
  info->SpecialActions = Default.SpecialActions;
  info->RedirectType = Default.RedirectType;
  info->RedirectMap = Default.RedirectMap;
  if (!replacement) {
    info->ExitPic = Default.ExitPic;
    info->EnterPic = Default.EnterPic;
    info->InterMusic = Default.InterMusic;
  }

  if (HexenMode) {
    info->Flags |= VLevelInfo::LIF_NoIntermission|
      VLevelInfo::LIF_FallingDamage|
      VLevelInfo::LIF_MonsterFallingDamage|
      VLevelInfo::LIF_NoAutoSndSeq|
      VLevelInfo::LIF_ActivateOwnSpecial|
      VLevelInfo::LIF_MissilesActivateImpact|
      VLevelInfo::LIF_InfiniteFlightPowerup;
  }

  // set saved par time
  int par = findSavedPar(MapLumpName);
  if (par > 0) {
    //GCon->Logf(NAME_Init, "found dehacked par time for map '%s' (%d)", *MapLumpName, par);
    info->ParTime = par;
  }

  // map name must follow the number
  ParseNameOrLookup(sc, VLevelInfo::LIF_LookupName, &info->Name, &info->Flags, false);

  // set song lump name from SNDINFO script
  for (int i = 0; i < MapSongList.Num(); ++i) {
    if (MapSongList[i].MapName == info->LumpName) {
      info->SongLump = MapSongList[i].SongName;
    }
  }

  // set default levelnum for this map
  const char *mn = *MapLumpName;
  if (mn[0] == 'm' && mn[1] == 'a' && mn[2] == 'p' && mn[5] == 0) {
    int num = VStr::atoi(mn+3);
    if (num >= 1 && num <= 99) info->LevelNum = num;
  } else if (mn[0] == 'e' && mn[1] >= '0' && mn[1] <= '9' &&
             mn[2] == 'm' && mn[3] >= '0' && mn[3] <= '9')
  {
    info->LevelNum = (mn[1]-'1')*10+(mn[3]-'0');
  }

  ParseMapCommon(sc, info, HexenMode);

  // avoid duplicate levelnums, later one takes precedance
  for (int i = 0; i < MapInfo.Num(); ++i) {
    if (MapInfo[i].LevelNum == info->LevelNum && &MapInfo[i] != info) {
      MapInfo[i].LevelNum = 0;
    }
  }
}


//==========================================================================
//
//  ParseClusterDef
//
//==========================================================================
static void ParseClusterDef (VScriptParser *sc) {
  VClusterDef *CDef = nullptr;
  sc->ExpectNumber();

  // check for replaced cluster def
  for (int i = 0; i < ClusterDefs.Num(); ++i) {
    if (sc->Number == ClusterDefs[i].Cluster) {
      CDef = &ClusterDefs[i];
      break;
    }
  }
  if (!CDef) CDef = &ClusterDefs.Alloc();

  // set defaults
  CDef->Cluster = sc->Number;
  CDef->Flags = 0;
  CDef->EnterText = VStr();
  CDef->ExitText = VStr();
  CDef->Flat = NAME_None;
  CDef->Music = NAME_None;

  //GCon->Logf("=== NEW CLUSTER %d ===", CDef->Cluster);
  bool newFormat = sc->Check("{");
  //if (newFormat) sc->SetCMode(true);
  for (;;) {
    //if (sc->GetString()) { GCon->Logf(":%s: CLUSTER(%d): <%s>", *sc->GetLoc().toStringNoCol(), (newFormat ? 1 : 0), *sc->String); sc->UnGet(); }
    if (sc->Check("hub")) {
      CDef->Flags |= CLUSTERF_Hub;
    } else if (sc->Check("entertext")) {
      if (newFormat) sc->Expect("=");
      ParseNameOrLookup(sc, CLUSTERF_LookupEnterText, &CDef->EnterText, &CDef->Flags, newFormat);
      //GCon->Logf("::: <%s>", *CDef->EnterText);
    } else if (sc->Check("entertextislump")) {
      CDef->Flags |= CLUSTERF_EnterTextIsLump;
    } else if (sc->Check("exittext")) {
      if (newFormat) sc->Expect("=");
      ParseNameOrLookup(sc, CLUSTERF_LookupExitText, &CDef->ExitText, &CDef->Flags, newFormat);
    } else if (sc->Check("exittextislump")) {
      CDef->Flags |= CLUSTERF_ExitTextIsLump;
    } else if (sc->Check("flat")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectName8();
      CDef->Flat = sc->Name8;
      CDef->Flags &= ~CLUSTERF_FinalePic;
    } else if (sc->Check("pic")) {
      if (newFormat) sc->Expect("=");
      //sc->ExpectName8();
      sc->ExpectName();
      CDef->Flat = sc->Name;
      CDef->Flags |= CLUSTERF_FinalePic;
    } else if (sc->Check("music")) {
      if (newFormat) sc->Expect("=");
      //sc->ExpectName8();
      sc->ExpectName();
      CDef->Music = sc->Name;
    } else if (sc->Check("cdtrack")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
      //CDef->CDTrack = sc->Number;
    } else if (sc->Check("cdid")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
      //CDef->CDId = sc->Number;
    } else if (sc->Check("name")) {
      auto loc = sc->GetLoc();
      if (newFormat) sc->Expect("=");
      if (sc->Check("lookup")) {
        if (newFormat) sc->Expect(",");
      }
      sc->ExpectString();
      miWarning(loc, "Unimplemented cluster command 'name'");
    } else {
      if (newFormat) {
        if (!sc->Check("}")) {
          auto loc = sc->GetLoc();
          sc->ExpectString();
          VStr cmd = sc->String;
          //fprintf(stderr, "!!!!!!\n");
          if (sc->Check("=")) {
            //fprintf(stderr, "******\n");
            for (;;) {
              sc->ExpectString();
              if (!sc->Check(",")) break;
            }
          }
          miWarning(loc, "unknown clusterdef command '%s'", *cmd);
        } else {
          break;
          //sc->Error(va("'}' expected in clusterdef, but got \"%s\"", *sc->String));
        }
      } else {
        break;
      }
    }
  }
  //if (newFormat) sc->SetCMode(false);

  // make sure text lump names are in lower case
  if (CDef->Flags&CLUSTERF_EnterTextIsLump) CDef->EnterText = CDef->EnterText.ToLower();
  if (CDef->Flags&CLUSTERF_ExitTextIsLump) CDef->ExitText = CDef->ExitText.ToLower();
}


//==========================================================================
//
//  ParseEpisodeDef
//
//==========================================================================
static void ParseEpisodeDef (VScriptParser *sc) {
  VEpisodeDef *EDef = nullptr;
  int EIdx = 0;
  sc->ExpectName8();

  // check for replaced episode
  for (int i = 0; i < EpisodeDefs.Num(); ++i) {
    if (sc->Name8 == EpisodeDefs[i].Name) {
      EDef = &EpisodeDefs[i];
      EIdx = i;
      break;
    }
  }
  if (!EDef) {
    EDef = &EpisodeDefs.Alloc();
    EIdx = EpisodeDefs.Num()-1;
  }

  // check for removal of an episode
  if (sc->Check("remove")) {
    EpisodeDefs.RemoveIndex(EIdx);
    return;
  }

  // set defaults
  EDef->Name = sc->Name8;
  EDef->TeaserName = NAME_None;
  EDef->Text = VStr();
  EDef->PicName = NAME_None;
  EDef->Flags = 0;
  EDef->Key = VStr();

  if (sc->Check("teaser")) {
    sc->ExpectName8();
    EDef->TeaserName = sc->Name8;
  }

  bool newFormat = sc->Check("{");
  //if (newFormat) sc->SetCMode(true);
  for (;;) {
    if (sc->Check("name")) {
      if (newFormat) sc->Expect("=");
      ParseNameOrLookup(sc, EPISODEF_LookupText, &EDef->Text, &EDef->Flags, newFormat);
    } else if (sc->Check("picname")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectName8();
      EDef->PicName = sc->Name8;
    } else if (sc->Check("key")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectString();
      EDef->Key = sc->String.ToLower();
    } else if (sc->Check("noskillmenu")) {
      EDef->Flags |= EPISODEF_NoSkillMenu;
    } else if (sc->Check("optional")) {
      EDef->Flags |= EPISODEF_Optional;
    } else {
      if (newFormat) sc->Expect("}");
      break;
    }
  }
  //if (newFormat) sc->SetCMode(false);
}


//==========================================================================
//
//  ParseSkillDefOld
//
//==========================================================================
static void ParseSkillDefOld (VScriptParser *sc, VSkillDef *sdef) {
  for (;;) {
    if (sc->Check("AmmoFactor")) {
      sc->ExpectFloat();
      sdef->AmmoFactor = sc->Float;
    } else if (sc->Check("DropAmmoFactor")) {
      sc->ExpectFloat();
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill setting 'DropAmmoFactor' is not implemented yet.", *sc->GetLoc().toStringNoCol());
    } else if (sc->Check("DoubleAmmoFactor")) {
      sc->ExpectFloat();
      sdef->DoubleAmmoFactor = sc->Float;
    } else if (sc->Check("DamageFactor")) {
      sc->ExpectFloat();
      sdef->DamageFactor = sc->Float;
    } else if (sc->Check("FastMonsters")) {
      sdef->Flags |= SKILLF_FastMonsters;
    } else if (sc->Check("DisableCheats")) {
      //k8: no, really?
      //sdef->Flags |= SKILLF_DisableCheats;
    } else if (sc->Check("EasyBossBrain")) {
      sdef->Flags |= SKILLF_EasyBossBrain;
    } else if (sc->Check("AutoUseHealth")) {
      sdef->Flags |= SKILLF_AutoUseHealth;
    } else if (sc->Check("RespawnTime")) {
      sc->ExpectFloat();
      sdef->RespawnTime = sc->Float;
    } else if (sc->Check("RespawnLimit")) {
      sc->ExpectNumber();
      sdef->RespawnLimit = sc->Number;
    } else if (sc->Check("NoPain")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'NoPain' is not implemented yet.", *sc->GetLoc().toStringNoCol());
    } else if (sc->Check("Aggressiveness")) {
      sc->ExpectFloatWithSign();
      if (sc->Float < 0) GCon->Logf(NAME_Warning, "%s:MAPINFO: \"Aggressiveness\" should be positive", *sc->GetLoc().toStringNoCol());
      sdef->Aggressiveness = 1.0f-midval(0.0f, (float)sc->Float, 1.0f);
    } else if (sc->Check("SpawnFilter")) {
      if (sc->CheckNumber()) {
        if (sc->Number > 0 && sc->Number < 31) sdef->SpawnFilter = 1<<(sc->Number-1);
      } else {
             if (sc->Check("Baby")) sdef->SpawnFilter = 1;
        else if (sc->Check("Easy")) sdef->SpawnFilter = 2;
        else if (sc->Check("Normal")) sdef->SpawnFilter = 4;
        else if (sc->Check("Hard")) sdef->SpawnFilter = 8;
        else if (sc->Check("Nightmare")) sdef->SpawnFilter = 16;
        else sc->ExpectString();
      }
    } else if (sc->Check("ACSReturn")) {
      sc->ExpectNumber();
      sdef->AcsReturn = sc->Number;
    } else if (sc->Check("Name")) {
      sc->ExpectString();
      sdef->MenuName = sc->String;
      sdef->Flags &= ~SKILLF_MenuNameIsPic;
    } else if (sc->Check("PlayerClassName")) {
      VSkillPlayerClassName &CN = sdef->PlayerClassNames.Alloc();
      sc->ExpectString();
      CN.ClassName = sc->String;
      sc->ExpectString();
      CN.MenuName = sc->String;
    } else if (sc->Check("PicName")) {
      sc->ExpectString();
      sdef->MenuName = sc->String.ToLower();
      sdef->Flags |= SKILLF_MenuNameIsPic;
    } else if (sc->Check("MustConfirm")) {
      sdef->Flags |= SKILLF_MustConfirm;
      if (sc->CheckQuotedString()) sdef->ConfirmationText = sc->String;
    } else if (sc->Check("Key")) {
      sc->ExpectString();
      sdef->Key = sc->String;
    } else if (sc->Check("TextColor")) {
      sc->ExpectString();
      sdef->TextColor = sc->String;
    } else {
      break;
    }
  }

  if (sdef->SpawnFilter == 0) {
    GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'SpawnFilter' is not set for skill '%s'; assume UV.", *sc->GetLoc().toStringNoCol(), *sdef->MenuName);
    sdef->SpawnFilter = 8; // UV
  }
}


//==========================================================================
//
//  ParseSkillDef
//
//==========================================================================
static void ParseSkillDef (VScriptParser *sc) {
  VSkillDef *sdef = nullptr;
  sc->ExpectString();

  // check for replaced skill
  for (int i = 0; i < SkillDefs.Num(); ++i) {
    if (sc->String.ICmp(SkillDefs[i].Name) == 0) {
      sdef = &SkillDefs[i];
      break;
    }
  }
  if (!sdef) {
    sdef = &SkillDefs.Alloc();
    sdef->Name = sc->String;
  }

  // set defaults
  sdef->AmmoFactor = 1.0f;
  sdef->DoubleAmmoFactor = 2.0f;
  sdef->DamageFactor = 1.0f;
  sdef->RespawnTime = 0.0f;
  sdef->RespawnLimit = 0;
  sdef->Aggressiveness = 1.0f;
  sdef->SpawnFilter = 0;
  sdef->AcsReturn = SkillDefs.Num()-1;
  sdef->MenuName.Clean();
  sdef->PlayerClassNames.Clear();
  sdef->ConfirmationText.Clean();
  sdef->Key.Clean();
  sdef->TextColor.Clean();
  sdef->Flags = 0;
  // if skill definition contains replacements, clear the old ones
  // k8: i am not sure if i should keep old replacements here, but why not?
  bool sdefClearReplacements = true;

  VClass *eexCls = VClass::FindClassNoCase("Actor"); // we'll need it later

  if (!sc->Check("{")) { ParseSkillDefOld(sc, sdef); return; }
  SCParseModeSaver msave(sc);

  while (!sc->Check("}")) {
    if (sc->Check("AmmoFactor")) {
      sc->Expect("=");
      sc->ExpectFloat();
      sdef->AmmoFactor = sc->Float;
    } else if (sc->Check("DoubleAmmoFactor")) {
      sc->Expect("=");
      sc->ExpectFloat();
      sdef->DoubleAmmoFactor = sc->Float;
    } else if (sc->Check("DropAmmoFactor")) {
      sc->Expect("=");
      sc->ExpectFloat();
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill setting 'DropAmmoFactor' is not implemented yet.", *sc->GetLoc().toStringNoCol());
    } else if (sc->Check("DamageFactor")) {
      sc->Expect("=");
      sc->ExpectFloat();
      sdef->DamageFactor = sc->Float;
    } else if (sc->Check("RespawnTime")) {
      sc->Expect("=");
      sc->ExpectFloat();
      sdef->RespawnTime = sc->Float;
    } else if (sc->Check("RespawnLimit")) {
      sc->Expect("=");
      sc->ExpectNumber();
      sdef->RespawnLimit = sc->Number;
    } else if (sc->Check("Aggressiveness")) {
      sc->Expect("=");
      sc->ExpectFloatWithSign();
      if (sc->Float < 0) GCon->Logf(NAME_Warning, "%s:MAPINFO: \"Aggressiveness\" should be positive", *sc->GetLoc().toStringNoCol());
      sdef->Aggressiveness = 1.0f-midval(0.0f, (float)sc->Float, 1.0f);
    } else if (sc->Check("SpawnFilter")) {
      sc->Expect("=");
      if (sc->CheckNumber()) {
        if (sc->Number > 0 && sc->Number < 31) {
          sdef->SpawnFilter = 1<<(sc->Number-1);
        } else {
          GCon->Logf(NAME_Warning, "MAPINFO:%s: invalid spawnfilter value %d", *sc->GetLoc().toStringNoCol(), sc->Number);
        }
      } else {
             if (sc->Check("Baby")) sdef->SpawnFilter = 1;
        else if (sc->Check("Easy")) sdef->SpawnFilter = 2;
        else if (sc->Check("Normal")) sdef->SpawnFilter = 4;
        else if (sc->Check("Hard")) sdef->SpawnFilter = 8;
        else if (sc->Check("Nightmare")) sdef->SpawnFilter = 16;
        else { sc->ExpectString(); GCon->Logf(NAME_Warning, "MAPINFO:%s: unknown spawnfilter '%s'", *sc->GetLoc().toStringNoCol(), *sc->String); }
      }
    } else if (sc->Check("ACSReturn")) {
      sc->Expect("=");
      sc->ExpectNumber();
      sdef->AcsReturn = sc->Number;
    } else if (sc->Check("Key")) {
      sc->Expect("=");
      sc->ExpectString();
      sdef->Key = sc->String;
    } else if (sc->Check("MustConfirm")) {
      sdef->Flags |= SKILLF_MustConfirm;
      if (sc->Check("=")) {
        sc->ExpectString();
        sdef->ConfirmationText = sc->String;
      }
    } else if (sc->Check("Name")) {
      sc->Expect("=");
      sc->ExpectString();
      sdef->MenuName = sc->String;
      sdef->Flags &= ~SKILLF_MenuNameIsPic;
    } else if (sc->Check("PlayerClassName")) {
      sc->Expect("=");
      VSkillPlayerClassName &CN = sdef->PlayerClassNames.Alloc();
      sc->ExpectString();
      CN.ClassName = sc->String;
      sc->Expect(",");
      sc->ExpectString();
      CN.MenuName = sc->String;
    } else if (sc->Check("PicName")) {
      sc->Expect("=");
      sc->ExpectString();
      sdef->MenuName = sc->String.ToLower();
      sdef->Flags |= SKILLF_MenuNameIsPic;
    } else if (sc->Check("TextColor")) {
      sc->Expect("=");
      sc->ExpectString();
      sdef->TextColor = sc->String;
    } else if (sc->Check("EasyBossBrain")) {
      sdef->Flags |= SKILLF_EasyBossBrain;
    } else if (sc->Check("EasyKey")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'EasyKey' is not implemented yet.", *sc->GetLoc().toStringNoCol());
    } else if (sc->Check("FastMonsters")) {
      sdef->Flags |= SKILLF_FastMonsters;
    } else if (sc->Check("SlowMonsters")) {
      sdef->Flags |= SKILLF_SlowMonsters;
    } else if (sc->Check("DisableCheats")) {
      //k8: no, really?
      //sdef->Flags |= SKILLF_DisableCheats;
    } else if (sc->Check("AutoUseHealth")) {
      sdef->Flags |= SKILLF_AutoUseHealth;
    } else if (sc->Check("ReplaceActor")) {
      //GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'ReplaceActor' is not implemented yet.", *sc->GetLoc().toStringNoCol());
      sc->Expect("=");
      sc->ExpectString();
      VStr oldCN = sc->String;
      sc->Expect(",");
      sc->ExpectString();
      VStr newCN = sc->String;
      if (sdefClearReplacements) { sdef->Replacements.clear(); sdefClearReplacements = false; }
      if (eexCls && !oldCN.isEmpty()) {
        VClass *oldCls = VClass::FindClassNoCase(*oldCN);
        if (!oldCls->IsChildOf(eexCls)) {
          GCon->Logf(NAME_Warning, "MAPINFO:%s: source class `%s` in 'ReplaceActor' is invalid.", *oldCN, *sc->GetLoc().toStringNoCol());
        } else {
          // source is ok, check destination
          VClass *newCls = nullptr;
          bool newIsValid = true;
          if (!newCN.isEmpty() && !newCN.strEquCI("none") && !newCN.strEquCI("null")) {
            newCls = VClass::FindClassNoCase(*newCN);
            if (!newCls || !newCls->IsChildOf(eexCls)) {
              GCon->Logf(NAME_Warning, "MAPINFO:%s: destination class `%s` in 'ReplaceActor' is invalid.", *newCN, *sc->GetLoc().toStringNoCol());
              newIsValid = false;
            }
          }
          if (newIsValid) {
            VSkillMonsterReplacement &rp = sdef->Replacements.alloc();
            rp.oldClass = oldCls;
            rp.newClass = newCls;
          }
        }
      }
    } else if (sc->Check("MonsterHealth")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'MonsterHealth' is not implemented yet.", *sc->GetLoc().toStringNoCol());
      sc->Expect("=");
      sc->ExpectFloat();
    } else if (sc->Check("FriendlyHealth")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'FriendlyHealth' is not implemented yet.", *sc->GetLoc().toStringNoCol());
      sc->Expect("=");
      sc->ExpectFloat();
    } else if (sc->Check("NoPain")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'NoPain' is not implemented yet.", *sc->GetLoc().toStringNoCol());
    } else if (sc->Check("DefaultSkill")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'DefaultSkill' is not implemented yet.", *sc->GetLoc().toStringNoCol());
    } else if (sc->Check("ArmorFactor")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'ArmorFactor' is not implemented yet.", *sc->GetLoc().toStringNoCol());
      sc->Expect("=");
      sc->ExpectFloat();
    } else if (sc->Check("NoInfighting")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'NoInfighting' is not implemented yet.", *sc->GetLoc().toStringNoCol());
    } else if (sc->Check("TotalInfighting")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'TotalInfighting' is not implemented yet.", *sc->GetLoc().toStringNoCol());
    } else if (sc->Check("HealthFactor")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'HealthFactor' is not implemented yet.", *sc->GetLoc().toStringNoCol());
      sc->Expect("=");
      sc->ExpectFloat();
    } else if (sc->Check("KickbackFactor")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'KickbackFactor' is not implemented yet.", *sc->GetLoc().toStringNoCol());
      sc->Expect("=");
      sc->ExpectFloat();
    } else if (sc->Check("NoMenu")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'NoMenu' is not implemented yet.", *sc->GetLoc().toStringNoCol());
    } else if (sc->Check("PlayerRespawn")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'PlayerRespawn' is not implemented yet.", *sc->GetLoc().toStringNoCol());
    } else if (sc->Check("ReplaceActor")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'ReplaceActor' is not implemented yet.", *sc->GetLoc().toStringNoCol());
      sc->Expect("=");
      for (;;) {
        sc->ExpectString();
        if (!sc->Check(",")) break;
      }
    } else {
      sc->ExpectString();
      sc->Error(va("unknown MAPINFO skill command '%s'", *sc->String));
      break;
    }
  }

  if (sdef->SpawnFilter == 0) {
    GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'SpawnFilter' is not set for skill '%s'; assume UV.", *sc->GetLoc().toStringNoCol(), *sdef->MenuName);
    sdef->SpawnFilter = 8; // UV
  }
}


//==========================================================================
//
//  ParseGameInfo
//
//==========================================================================
static void ParseGameInfo (VScriptParser *sc) {
  //auto cmode = sc->IsCMode();
  //sc->SetCMode(true);
  sc->Expect("{");
  //sc->SkipBracketed(true);
  for (;;) {
    if (sc->AtEnd()) { sc->Error("'}' not found"); break; }
    if (sc->Check("}")) break;
    if (sc->Check("PlayerClasses")) {
      MapInfoPlayerClasses.clear();
      sc->Expect("=");
      for (;;) {
        sc->ExpectString();
        if (sc->String.length()) MapInfoPlayerClasses.append(VName(*sc->String));
        if (!sc->Check(",")) break;
      }
    } else if (sc->Check("weaponslot")) {
      sc->Expect("=");
      sc->ExpectNumber();
      int slot = sc->Number;
      if (slot < 0 || slot > 10) sc->Message(va("ignoring gameinfo weaponslot %d", slot));
      TArray<VStr> clist;
      clist.append("CmdSetSlot"); // arg0
      clist.append(VStr(slot)); // arg1 is number
      while (sc->Check(",")) {
        if (!sc->GetString()) sc->Error("unexpected gameinfo end in mapinfo");
        if (!sc->String.isEmpty()) clist.append(sc->String);
      }
      // we only have so many slots
      if (slot >= 0 && slot <= 10) {
        GGameInfo->eventCmdSetSlot(&clist, false); // as gameinfo
      }
    } else if (sc->Check("ForceKillScripts")) {
      sc->Expect("=");
      bool bval = ExpectBool("ForceKillScripts", sc);
      //mapInfoGameInfoInitial.bForceKillScripts = bval;
      if (bval) GGameInfo->Flags |= VGameInfo::GIF_ForceKillScripts; else GGameInfo->Flags &= ~VGameInfo::GIF_ForceKillScripts;
    } else {
      sc->ExpectString();
      sc->Message(va("skipped gameinfo command '%s'", *sc->String));
      sc->Expect("=");
      sc->ExpectString();
      while (sc->Check(",")) {
        if (!sc->GetString()) sc->Error("unexpected gameinfo end in mapinfo");
      }
    }
  }
  //sc->SetCMode(cmode);
}


//==========================================================================
//
//  ParseMapInfo
//
//==========================================================================
static void ParseMapInfo (VScriptParser *sc) {
  const unsigned int MaxStack = 64;
  bool HexenMode = false;
  VScriptParser *scstack[MaxStack];
  unsigned int scsp = 0;
  bool error = false;

  // set up default map info
  mapInfo_t Default;
  SetMapDefaults(Default);

  for (;;) {
    while (!sc->AtEnd()) {
      if (sc->Check("map")) {
        ParseMap(sc, HexenMode, Default);
      } else if (sc->Check("defaultmap")) {
        SetMapDefaults(Default);
        ParseMapCommon(sc, &Default, HexenMode);
      } else if (sc->Check("adddefaultmap")) {
        ParseMapCommon(sc, &Default, HexenMode);
      } else if (sc->Check("clusterdef")) {
        ParseClusterDef(sc);
      } else if (sc->Check("cluster")) {
        ParseClusterDef(sc);
      } else if (sc->Check("episode")) {
        ParseEpisodeDef(sc);
      } else if (sc->Check("clearepisodes")) {
        EpisodeDefs.Clear();
      } else if (sc->Check("skill")) {
        ParseSkillDef(sc);
      } else if (sc->Check("clearskills")) {
        SkillDefs.Clear();
      } else if (sc->Check("include")) {
        sc->ExpectString();
        int lmp = W_CheckNumForFileName(sc->String);
        if (lmp >= 0) {
          if (scsp >= MaxStack) {
            sc->Error(va("mapinfo include nesting too deep"));
            error = true;
            break;
          }
          GCon->Logf("Including '%s'...", *sc->String);
          scstack[scsp++] = sc;
          sc = new VScriptParser(/**sc->String*/W_FullLumpName(lmp), W_CreateLumpReaderNum(lmp));
          //ParseMapInfo(new VScriptParser(*sc->String, W_CreateLumpReaderNum(lmp)));
        } else {
          sc->Error(va("mapinfo include '%s' not found", *sc->String));
          error = true;
          break;
        }
      }
      // hack for "complete"
      else if (sc->Check("gameinfo")) {
        ParseGameInfo(sc);
      } else if (sc->Check("damagetype")) {
        sc->Message("Unimplemented MAPINFO command `DamageType`");
        if (!sc->Check("{")) {
          sc->ExpectString();
          sc->SkipBracketed();
        } else {
          sc->SkipBracketed(true); // bracket eaten
        }
      } else if (sc->Check("intermission")) {
        sc->Message("Unimplemented MAPINFO command `Intermission`");
        if (!sc->Check("{")) {
          sc->ExpectString();
          sc->SkipBracketed();
        } else {
          sc->SkipBracketed(true); // bracket eaten
        }
      /*
      } else if (sc->Check("gamedefaults")) {
        GCon->Logf("WARNING: Unimplemented MAPINFO section gamedefaults");
        sc->SkipBracketed();
      } else if (sc->Check("automap")) {
        GCon->Logf("WARNING: Unimplemented MAPINFO command Automap");
        sc->SkipBracketed();
      } else if (sc->Check("automap_overlay")) {
        GCon->Logf("WARNING: Unimplemented MAPINFO command automap_overlay");
        sc->SkipBracketed();
      */
      } else if (sc->Check("DoomEdNums")) {
        //GCon->Logf("*** <%s> ***", *sc->String);
        //auto cmode = sc->IsCMode();
        //sc->SetCMode(true);
        sc->Expect("{");
        for (;;) {
          if (sc->Check("}")) break;
          sc->ExpectNumber();
          int num = sc->Number;
          sc->Expect("=");
          sc->ExpectString();
          VStr clsname = sc->String;
          int flags = 0;
          int special = 0;
          int args[5] = {0, 0, 0, 0, 0};
          if (sc->Check(",")) {
            auto loc = sc->GetLoc();
            bool doit = true;
            if (sc->Check("noskillflags")) { flags |= mobjinfo_t::FlagNoSkill; doit = sc->Check(","); }
            if (doit) {
              flags |= mobjinfo_t::FlagSpecial;
              sc->ExpectString();
              VStr spcname = sc->String;
              // no name?
              int argn = 0;
              int arg0 = 0;
              if (VStr::convertInt(*spcname, &arg0)) {
                spcname = VStr();
                special = -1;
                args[argn++] = arg0;
              }
              while (sc->Check(",")) {
                sc->ExpectNumber(true); // with sign, why not
                if (argn < 5) args[argn] = sc->Number;
                ++argn;
              }
              if (argn > 5) GCon->Logf(NAME_Warning, "MAPINFO:%s: too many arguments (%d) to special '%s'", *loc.toStringNoCol(), argn, *spcname);
              // find special number
              if (special == 0) {
                for (int sdx = 0; sdx < LineSpecialInfos.length(); ++sdx) {
                  if (LineSpecialInfos[sdx].Name.ICmp(spcname) == 0) {
                    special = LineSpecialInfos[sdx].Number;
                    break;
                  }
                }
              }
              if (!special) {
                flags &= ~mobjinfo_t::FlagSpecial;
                GCon->Logf(NAME_Warning, "MAPINFO:%s: special '%s' not found", *loc.toStringNoCol(), *spcname);
              }
              if (special == -1) special = 0; // special special ;-)
            }
          }
          //GCon->Logf("MAPINFO: DOOMED: '%s', %d (%d)", *clsname, num, flags);
          appendNumFixup(DoomEdNumFixups, clsname, num, flags, special, args[0], args[1], args[2], args[3], args[4]);
        }
        //sc->SetCMode(cmode);
      } else if (sc->Check("SpawnNums")) {
        //auto cmode = sc->IsCMode();
        //sc->SetCMode(true);
        sc->Expect("{");
        for (;;) {
          if (sc->Check("}")) break;
          sc->ExpectNumber();
          int num = sc->Number;
          sc->Expect("=");
          sc->ExpectString();
          appendNumFixup(SpawnNumFixups, VStr(sc->String), num);
        }
        //sc->SetCMode(cmode);
      } else if (sc->Check("author")) {
        sc->ExpectString();
      } else {
        VStr cmdname = sc->String;
        sc->ExpectString();
        if (sc->Check("{")) {
          GCon->Logf(NAME_Warning, "Unimplemented MAPINFO command '%s'", *cmdname);
          sc->SkipBracketed(true); // bracket eaten
        } else {
          sc->Error(va("Invalid command '%s'", *sc->String));
          error = true;
          break;
        }
      }
    }
    if (error) {
      while (scsp > 0) { delete sc; sc = scstack[--scsp]; }
      break;
    }
    if (scsp == 0) break;
    GCon->Logf("Finished included '%s'", *sc->GetLoc().GetSource());
    delete sc;
    sc = scstack[--scsp];
  }
  delete sc;
  sc = nullptr;
}


//==========================================================================
//
// QualifyMap
//
//==========================================================================
static int QualifyMap (int map) {
  return (map < 0 || map >= MapInfo.Num()) ? 0 : map;
}


//==========================================================================
//
//  P_GetMapInfo
//
//==========================================================================
const mapInfo_t &P_GetMapInfo (VName map) {
  for (int i = 0; i < MapInfo.Num(); ++i) {
    if (map == MapInfo[i].LumpName) return MapInfo[i];
  }
  return DefaultMap;
}


//==========================================================================
//
//  P_GetMapName
//
//==========================================================================
VStr P_GetMapName (int map) {
  return MapInfo[QualifyMap(map)].GetName();
}


//==========================================================================
//
//  P_GetMapInfoIndexByLevelNum
//
//  Returns map info index given a level number
//
//==========================================================================
int P_GetMapIndexByLevelNum (int map) {
  for (int i = 0; i < MapInfo.Num(); ++i) {
    if (MapInfo[i].LevelNum == map) return i;
  }
  // not found
  return 0;
}


//==========================================================================
//
//  P_GetMapLumpName
//
//==========================================================================
VName P_GetMapLumpName (int map) {
  return MapInfo[QualifyMap(map)].LumpName;
}


//==========================================================================
//
//  P_TranslateMap
//
//  Returns the map lump name given a warp map number.
//
//==========================================================================
VName P_TranslateMap (int map) {
  for (int i = MapInfo.length()-1; i >= 0; --i) {
    if (MapInfo[i].WarpTrans == map) return MapInfo[i].LumpName;
  }
  // not found
  return (MapInfo.length() > 0 ? MapInfo[0].LumpName : NAME_None);
}


//==========================================================================
//
//  P_TranslateMapEx
//
//  Returns the map lump name given a warp map number.
//
//==========================================================================
VName P_TranslateMapEx (int map) {
  for (int i = MapInfo.length()-1; i >= 0; --i) {
    if (MapInfo[i].WarpTrans == map) return MapInfo[i].LumpName;
  }
  // not found
  return NAME_None;
}


//==========================================================================
//
//  P_GetMapLumpNameByLevelNum
//
//  Returns the map lump name given a level number.
//
//==========================================================================
VName P_GetMapLumpNameByLevelNum (int map) {
  for (int i = 0; i < MapInfo.Num(); ++i) {
    if (MapInfo[i].LevelNum == map) return MapInfo[i].LumpName;
  }
  // not found, use map##
  return va("map%02d", map);
}


//==========================================================================
//
// P_PutMapSongLump
//
//==========================================================================
void P_PutMapSongLump (int map, VName lumpName) {
  FMapSongInfo &ms = MapSongList.Alloc();
  ms.MapName = va("map%02d", map);
  ms.SongName = lumpName;
}


//==========================================================================
//
//  P_GetClusterDef
//
//==========================================================================
const VClusterDef *P_GetClusterDef (int Cluster) {
  for (int i = 0; i < ClusterDefs.Num(); ++i) {
    if (Cluster == ClusterDefs[i].Cluster) return &ClusterDefs[i];
  }
  return &DefaultClusterDef;
}


//==========================================================================
//
//  P_GetNumEpisodes
//
//==========================================================================
int P_GetNumEpisodes () {
  return EpisodeDefs.Num();
}


//==========================================================================
//
//  P_GetNumMaps
//
//==========================================================================
int P_GetNumMaps () {
  return MapInfo.Num();
}


//==========================================================================
//
//  P_GetMapInfo
//
//==========================================================================
mapInfo_t *P_GetMapInfoPtr (int mapidx) {
  return (mapidx >= 0 && mapidx < MapInfo.Num() ? &MapInfo[mapidx] : nullptr);
}


//==========================================================================
//
//  P_GetEpisodeDef
//
//==========================================================================
VEpisodeDef *P_GetEpisodeDef (int Index) {
  return &EpisodeDefs[Index];
}


//==========================================================================
//
//  P_GetNumSkills
//
//==========================================================================
int P_GetNumSkills () {
  return SkillDefs.Num();
}


//==========================================================================
//
//  P_GetSkillDef
//
//==========================================================================
const VSkillDef *P_GetSkillDef (int Index) {
  return &SkillDefs[Index];
}


//==========================================================================
//
//  P_GetMusicLumpNames
//
//==========================================================================
void P_GetMusicLumpNames (TArray<FReplacedString> &List) {
  for (int i = 0; i < MapInfo.Num(); ++i) {
    const char *MName = *MapInfo[i].SongLump;
    if (MName[0] == 'd' && MName[1] == '_') {
      FReplacedString &R = List.Alloc();
      R.Index = i;
      R.Replaced = false;
      R.Old = MName+2;
    }
  }
}


//==========================================================================
//
//  P_ReplaceMusicLumpNames
//
//==========================================================================
void P_ReplaceMusicLumpNames (TArray<FReplacedString> &List) {
  for (int i = 0; i < List.Num(); ++i) {
    if (List[i].Replaced) {
      MapInfo[List[i].Index].SongLump = VName(*(VStr("d_")+List[i].New), VName::AddLower8);
    }
  }
}


//==========================================================================
//
//  P_SetParTime
//
//==========================================================================
void P_SetParTime (VName Map, int Par) {
  if (Map == NAME_None || Par < 0) return;
  if (mapinfoParsed) {
    for (int i = 0; i < MapInfo.length(); ++i) {
      if (MapInfo[i].LumpName == NAME_None) continue;
      if (VStr::ICmp(*MapInfo[i].LumpName, *Map) == 0) {
        MapInfo[i].ParTime = Par;
        return;
      }
    }
    GCon->Logf(NAME_Warning, "No such map '%s' for par time", *Map);
  } else {
    ParTimeInfo &pi = partimes.alloc();
    pi.MapName = Map;
    pi.par = Par;
  }
}


//==========================================================================
//
//  IsMapPresent
//
//==========================================================================
bool IsMapPresent (VName MapName) {
  if (W_CheckNumForName(MapName) >= 0) return true;
  VStr FileName = va("maps/%s.wad", *MapName);
  if (FL_FileExists(FileName)) return true;
  return false;
}


//==========================================================================
//
//  COMMAND MapList
//
//==========================================================================
COMMAND(MapList) {
  for (int i = 0; i < MapInfo.Num(); ++i) {
    if (IsMapPresent(MapInfo[i].LumpName)) {
      GCon->Log(VStr(MapInfo[i].LumpName)+" - "+(MapInfo[i].Flags&VLevelInfo::LIF_LookupName ? GLanguage[*MapInfo[i].Name] : MapInfo[i].Name));
    }
  }
}


//==========================================================================
//
//  ShutdownMapInfo
//
//==========================================================================
void ShutdownMapInfo () {
  DefaultMap.Name.Clean();
  MapInfo.Clear();
  MapSongList.Clear();
  ClusterDefs.Clear();
  EpisodeDefs.Clear();
  SkillDefs.Clear();
}
