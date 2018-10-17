//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
#include "gamedefs.h"
#include "sv_local.h"

#include "render/r_shared.h"


struct FMapSongInfo {
  VName MapName;
  VName SongName;
};

struct ParTimeInfo {
  VName MapName;
  int par;
};


VName P_TranslateMap (int map);

static void ParseMapInfo(VScriptParser *sc);

static mapInfo_t DefaultMap;
static TArray<mapInfo_t> MapInfo;
static TArray<FMapSongInfo> MapSongList;
static VClusterDef DefaultClusterDef;
static TArray<VClusterDef> ClusterDefs;
static TArray<VEpisodeDef> EpisodeDefs;
static TArray<VSkillDef> SkillDefs;

static bool mapinfoParsed = false;
static TArray<ParTimeInfo> partimes; // not a hashmap, so i can use `ICmp`


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
static int loadSkyTexture (VName name) {
  if (name == NAME_None) return GTextureManager.DefaultTexture;
  //int Tex = GTextureManager.NumForName(sc->Name8, TEXTYPE_Wall, true, false);
  //info->Sky1Texture = GTextureManager.NumForName(sc->Name8, TEXTYPE_Wall, true, false);
  int Tex = GTextureManager.CheckNumForName(name, TEXTYPE_Wall, true, false);
  if (Tex >= 0) return Tex;
  Tex = GTextureManager.CheckNumForName(name, TEXTYPE_WallPatch, false, false);
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
  if (Tex < 0) {
    GCon->Logf("WARNING: sky '%s' not found; replaced with 'sky1'", *name);
    return GTextureManager.CheckNumForName("sky1", TEXTYPE_Wall, true, true);
    //return GTextureManager.DefaultTexture;
  }
  GCon->Logf("WARNING: force-loaded sky '%s'", *name);
  return Tex;
}


//==========================================================================
//
//  InitMapInfo
//
//==========================================================================
void InitMapInfo () {
  guard(InitMapInfo);
  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) == NAME_mapinfo) {
      GCon->Logf("mapinfo file: '%s'", *W_FullLumpName(Lump));
      ParseMapInfo(new VScriptParser(*W_LumpName(Lump), W_CreateLumpReaderNum(Lump)));
    }
  }
  mapinfoParsed = true;

  for (int i = 0; i < MapInfo.Num(); ++i) {
    if (VStr(MapInfo[i].NextMap).StartsWith("&wt@")) {
      MapInfo[i].NextMap = P_TranslateMap(atoi(*MapInfo[i].NextMap+4));
    }
    if (VStr(MapInfo[i].SecretMap).StartsWith("&wt@")) {
      MapInfo[i].SecretMap = P_TranslateMap(atoi(*MapInfo[i].SecretMap+4));
    }
  }

  for (int i = 0; i < EpisodeDefs.Num(); ++i) {
    if (VStr(EpisodeDefs[i].Name).StartsWith("&wt@")) {
      EpisodeDefs[i].Name = P_TranslateMap(atoi(*EpisodeDefs[i].Name+4));
    }
    if (VStr(EpisodeDefs[i].TeaserName).StartsWith("&wt@")) {
      EpisodeDefs[i].TeaserName = P_TranslateMap(atoi(*EpisodeDefs[i].TeaserName+4));
    }
  }

  // set up default map info returned for maps that have not defined in MAPINFO
  DefaultMap.Name = "Unnamed";
  DefaultMap.Sky1Texture = loadSkyTexture("sky1"); //GTextureManager.CheckNumForName("sky1", TEXTYPE_Wall, true, true);
  DefaultMap.Sky2Texture = DefaultMap.Sky1Texture;
  DefaultMap.FadeTable = NAME_colormap;
  DefaultMap.HorizWallShade = -8;
  DefaultMap.VertWallShade = 8;
  //GCon->Logf("*** DEFAULT MAP: Sky1Texture=%d", DefaultMap.Sky1Texture);
  unguard;
}


//==========================================================================
//
//  SetMapDefaults
//
//==========================================================================
static void SetMapDefaults (mapInfo_t &Info) {
  guard(SetMapDefaults);
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
  Info.Sky1Texture = loadSkyTexture("sky1"); //GTextureManager.CheckNumForName("sky1", TEXTYPE_Wall, true, true);
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

  if (GGameInfo->Flags & VGameInfo::GIF_DefaultLaxMonsterActivation) {
    Info.Flags2 |= MAPINFOF2_LaxMonsterActivation;
  }
  unguard;
}


//==========================================================================
//
//  ParseNextMapName
//
//==========================================================================
static VName ParseNextMapName (VScriptParser *sc, bool HexenMode) {
  guard(ParseNextMapName);
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
  unguard;
}


//==========================================================================
//
//  DoCompatFlag
//
//==========================================================================
static void DoCompatFlag (VScriptParser *sc, mapInfo_t *info, int Flag) {
  guard(DoCompatFlag);
  int Set = 1;
  sc->Check("=");
  if (sc->CheckNumber()) Set = sc->Number;
  if (Set) {
    info->Flags2 |= Flag;
  } else {
    info->Flags2 &= ~Flag;
  }
  unguard;
}


//==========================================================================
//
//  ParseMapCommon
//
//==========================================================================
static void ParseMapCommon (VScriptParser *sc, mapInfo_t *info, bool &HexenMode) {
  guard(ParseMapCommon);
  bool newFormat = sc->Check("{");
  if (newFormat) sc->SetCMode(true);
  // process optional tokens
  while (1) {
    if (sc->Check("levelnum")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
      info->LevelNum = sc->Number;
    } else if (sc->Check("cluster")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
      info->Cluster = sc->Number;
      if (P_GetClusterDef(info->Cluster) == &DefaultClusterDef)
      {
        //  Add empty cluster def if it doesn't exist yet.
        VClusterDef &C = ClusterDefs.Alloc();
        C.Cluster = info->Cluster;
        C.Flags = 0;
        C.EnterText = VStr();
        C.ExitText = VStr();
        C.Flat = NAME_None;
        C.Music = NAME_None;
        if (HexenMode) C.Flags |= CLUSTERF_Hub;
      }
    } else if (sc->Check("warptrans")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
      info->WarpTrans = sc->Number;
    } else if (sc->Check("next")) {
      if (newFormat) sc->Expect("=");
      info->NextMap = ParseNextMapName(sc, HexenMode);
      // hack for "complete"
      if (sc->Check("{")) {
        info->NextMap = "endgamec";
        for (;;) {
          if (sc->AtEnd()) { sc->Error("'}' not found"); break; }
          if (sc->Check("}")) break;
          sc->GetString();
        }
      } else if (newFormat && sc->Check(",")) {
        sc->ExpectString();
        // check for more commas?
      }
    } else if (sc->Check("secret") || sc->Check("secretnext")) {
      if (newFormat) sc->Expect("=");
      info->SecretMap = ParseNextMapName(sc, HexenMode);
    } else if (sc->Check("sky1")) {
      if (newFormat) sc->Expect("=");
      auto ocm = sc->IsCMode();
      sc->SetCMode(true); // we need this to properly parse commas
      sc->ExpectName8();
      //info->Sky1Texture = GTextureManager.NumForName(sc->Name8, TEXTYPE_Wall, true, false);
      VName skbname = R_HasNamedSkybox(sc->String);
      if (skbname != NAME_None) {
        info->SkyBox = skbname;
        info->Sky1Texture = GTextureManager.DefaultTexture;
        info->Sky2Texture = GTextureManager.DefaultTexture;
        info->Sky1ScrollDelta = 0;
        info->Sky2ScrollDelta = 0;
        GCon->Logf("MSG: using gz skybox '%s'", *skbname);
        if (!sc->IsAtEol()) {
          sc->Check(",");
          sc->ExpectFloatWithSign();
          if (HexenMode) sc->Float /= 256.0;
          if (sc->Float != 0) GCon->Logf("MSG: ignoring sky scroll for skybox (this is mostly harmless)");
        }
      } else {
        info->SkyBox = NAME_None;
        info->Sky1Texture = loadSkyTexture(sc->Name8);
        info->Sky1ScrollDelta = 0;
        if (newFormat) {
          if (!sc->IsAtEol()) {
            sc->Check(",");
            sc->ExpectFloatWithSign();
            if (HexenMode) sc->Float /= 256.0;
            info->Sky1ScrollDelta = sc->Float * 35.0;
          }
        } else {
          if (!sc->IsAtEol()) {
            sc->Check(",");
            sc->ExpectFloatWithSign();
            if (HexenMode) sc->Float /= 256.0;
            info->Sky1ScrollDelta = sc->Float * 35.0;
          }
        }
      }
      sc->SetCMode(ocm);
    } else if (sc->Check("sky2")) {
      if (newFormat) sc->Expect("=");
      auto ocm = sc->IsCMode();
      sc->SetCMode(true); // we need this to properly parse commas
      sc->ExpectName8();
      //info->Sky2Texture = GTextureManager.NumForName(sc->Name8, TEXTYPE_Wall, true, false);
      //info->SkyBox = NAME_None; //k8:required or not???
      info->Sky2Texture = loadSkyTexture(sc->Name8);
      info->Sky2ScrollDelta = 0;
      if (newFormat) {
        if (!sc->IsAtEol()) {
          sc->Check(",");
          sc->ExpectFloatWithSign();
          if (HexenMode) sc->Float /= 256.0;
          info->Sky1ScrollDelta = sc->Float * 35.0;
        }
      } else {
        if (!sc->IsAtEol()) {
          sc->Check(",");
          sc->ExpectFloatWithSign();
          if (HexenMode) sc->Float /= 256.0;
          info->Sky2ScrollDelta = sc->Float * 35.0;
        }
      }
      sc->SetCMode(ocm);
    } else if (sc->Check("skybox")){
      if (newFormat) sc->Expect("=");
      sc->ExpectString();
      info->SkyBox = *sc->String;
    } else if (sc->Check("doublesky")) {
      info->Flags |= MAPINFOF_DoubleSky;
    } else if (sc->Check("lightning")) {
      info->Flags |= MAPINFOF_Lightning;
    } else if (sc->Check("forcenoskystretch")) {
      info->Flags |= MAPINFOF_ForceNoSkyStretch;
    } else if (sc->Check("skystretch")) {
      info->Flags &= ~MAPINFOF_ForceNoSkyStretch;
    } else if (sc->Check("fadetable")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectName8();
      info->FadeTable = sc->Name8;
    } else if (sc->Check("fade")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectString();
      info->Fade = M_ParseColour(sc->String);
    } else if (sc->Check("outsidefog")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectString();
      info->OutsideFog = M_ParseColour(sc->String);
    } else if (sc->Check("music")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectName8();
      info->SongLump = sc->Name8;
    } else if (sc->Check("cdtrack")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
      //info->CDTrack = sc->Number;
    } else if (sc->Check("gravity")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
      info->Gravity = (float)sc->Number;
    } else if (sc->Check("aircontrol")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectFloat();
      info->AirControl = sc->Float;
    } else if (sc->Check("map07special")) {
      info->Flags |= MAPINFOF_Map07Special;
    } else if (sc->Check("baronspecial")) {
      info->Flags |= MAPINFOF_BaronSpecial;
    } else if (sc->Check("cyberdemonspecial")) {
      info->Flags |= MAPINFOF_CyberDemonSpecial;
    } else if (sc->Check("spidermastermindspecial")) {
      info->Flags |= MAPINFOF_SpiderMastermindSpecial;
    } else if (sc->Check("minotaurspecial")) {
      info->Flags |= MAPINFOF_MinotaurSpecial;
    } else if (sc->Check("dsparilspecial")) {
      info->Flags |= MAPINFOF_DSparilSpecial;
    } else if (sc->Check("ironlichspecial")) {
      info->Flags |= MAPINFOF_IronLichSpecial;
    } else if (sc->Check("specialaction_exitlevel")) {
      info->Flags &= ~(MAPINFOF_SpecialActionOpenDoor|MAPINFOF_SpecialActionLowerFloor);
    } else if (sc->Check("specialaction_opendoor")) {
      info->Flags &= ~MAPINFOF_SpecialActionLowerFloor;
      info->Flags |= MAPINFOF_SpecialActionOpenDoor;
    } else if (sc->Check("specialaction_lowerfloor")) {
      info->Flags |= MAPINFOF_SpecialActionLowerFloor;
      info->Flags &= ~MAPINFOF_SpecialActionOpenDoor;
    } else if (sc->Check("specialaction_killmonsters")) {
      info->Flags |= MAPINFOF_SpecialActionKillMonsters;
    } else if (sc->Check("intermission")) {
      info->Flags &= ~MAPINFOF_NoIntermission;
    } else if (sc->Check("nointermission")) {
      info->Flags |= MAPINFOF_NoIntermission;
    } else if (sc->Check("titlepatch")) {
      //FIXME: quoted string is a textual level name
      if (newFormat) sc->Expect("=");
      sc->ExpectName8Def(NAME_None);
      info->TitlePatch = sc->Name8;
    } else if (sc->Check("par")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
      info->ParTime = sc->Number;
    } else if (sc->Check("sucktime")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
      info->SuckTime = sc->Number;
    } else if (sc->Check("nosoundclipping")) {
      // ignored
    } else if (sc->Check("allowmonstertelefrags")) {
      info->Flags |= MAPINFOF_AllowMonsterTelefrags;
    } else if (sc->Check("noallies")) {
      info->Flags |= MAPINFOF_NoAllies;
    } else if (sc->Check("fallingdamage")) {
      info->Flags &= ~(MAPINFOF_OldFallingDamage|MAPINFOF_StrifeFallingDamage);
      info->Flags |= MAPINFOF_FallingDamage;
    } else if (sc->Check("oldfallingdamage") || sc->Check("forcefallingdamage")) {
      info->Flags &= ~(MAPINFOF_FallingDamage|MAPINFOF_StrifeFallingDamage);
      info->Flags |= MAPINFOF_OldFallingDamage;
    } else if (sc->Check("strifefallingdamage")) {
      info->Flags &= ~(MAPINFOF_OldFallingDamage|MAPINFOF_FallingDamage);
      info->Flags |= MAPINFOF_StrifeFallingDamage;
    } else if (sc->Check("nofallingdamage")) {
      info->Flags &= ~(MAPINFOF_OldFallingDamage|MAPINFOF_StrifeFallingDamage|MAPINFOF_FallingDamage);
    } else if (sc->Check("monsterfallingdamage")) {
      info->Flags |= MAPINFOF_MonsterFallingDamage;
    } else if (sc->Check("nomonsterfallingdamage")) {
      info->Flags &= ~MAPINFOF_MonsterFallingDamage;
    } else if (sc->Check("deathslideshow")) {
      info->Flags |= MAPINFOF_DeathSlideShow;
    } else if (sc->Check("allowfreelook")) {
      info->Flags &= ~MAPINFOF_NoFreelook;
    } else if (sc->Check("nofreelook")) {
      info->Flags |= MAPINFOF_NoFreelook;
    } else if (sc->Check("allowjump")) {
      info->Flags &= ~MAPINFOF_NoJump;
    } else if (sc->Check("nojump")) {
      info->Flags |= MAPINFOF_NoJump;
    } else if (sc->Check("noautosequences")) {
      info->Flags |= MAPINFOF_NoAutoSndSeq;
    } else if (sc->Check("activateowndeathspecials")) {
      info->Flags |= MAPINFOF_ActivateOwnSpecial;
    } else if (sc->Check("killeractivatesdeathspecials")) {
      info->Flags &= ~MAPINFOF_ActivateOwnSpecial;
    } else if (sc->Check("missilesactivateimpactlines")) {
      info->Flags |= MAPINFOF_MissilesActivateImpact;
    } else if (sc->Check("missileshootersactivetimpactlines")) {
      info->Flags &= ~MAPINFOF_MissilesActivateImpact;
    } else if (sc->Check("filterstarts")) {
      info->Flags |= MAPINFOF_FilterStarts;
    } else if (sc->Check("infiniteflightpowerup")) {
      info->Flags |= MAPINFOF_InfiniteFlightPowerup;
    } else if (sc->Check("noinfiniteflightpowerup")) {
      info->Flags &= ~MAPINFOF_InfiniteFlightPowerup;
    } else if (sc->Check("clipmidtextures")) {
      info->Flags |= MAPINFOF_ClipMidTex;
    } else if (sc->Check("wrapmidtextures")) {
      info->Flags |= MAPINFOF_WrapMidTex;
    } else if (sc->Check("keepfullinventory")) {
      info->Flags |= MAPINFOF_KeepFullInventory;
    } else if (sc->Check("compat_shorttex")) {
      DoCompatFlag(sc, info, MAPINFOF2_CompatShortTex);
    } else if (sc->Check("compat_stairs")) {
      DoCompatFlag(sc, info, MAPINFOF2_CompatStairs);
    } else if (sc->Check("compat_limitpain")) {
      DoCompatFlag(sc, info, MAPINFOF2_CompatLimitPain);
    } else if (sc->Check("compat_nopassover")) {
      DoCompatFlag(sc, info, MAPINFOF2_CompatNoPassOver);
    } else if (sc->Check("compat_notossdrops")) {
      DoCompatFlag(sc, info, MAPINFOF2_CompatNoTossDrops);
    } else if (sc->Check("compat_useblocking")) {
      DoCompatFlag(sc, info, MAPINFOF2_CompatUseBlocking);
    } else if (sc->Check("compat_nodoorlight")) {
      DoCompatFlag(sc, info, MAPINFOF2_CompatNoDoorLight);
    } else if (sc->Check("compat_ravenscroll")) {
      DoCompatFlag(sc, info, MAPINFOF2_CompatRavenScroll);
    } else if (sc->Check("compat_soundtarget")) {
      DoCompatFlag(sc, info, MAPINFOF2_CompatSoundTarget);
    } else if (sc->Check("compat_dehhealth")) {
      DoCompatFlag(sc, info, MAPINFOF2_CompatDehHealth);
    } else if (sc->Check("compat_trace")) {
      DoCompatFlag(sc, info, MAPINFOF2_CompatTrace);
    } else if (sc->Check("compat_dropoff")) {
      DoCompatFlag(sc, info, MAPINFOF2_CompatDropOff);
    } else if (sc->Check("compat_boomscroll") || sc->Check("additive_scrollers")) {
      DoCompatFlag(sc, info, MAPINFOF2_CompatBoomScroll);
    } else if (sc->Check("compat_invisibility")) {
      DoCompatFlag(sc, info, MAPINFOF2_CompatInvisibility);
    } else if (sc->Check("compat_sectorsounds")) {
      GCon->Logf("WARNING: %s: mapdef 'compat_sectorsounds' is not supported yet", *sc->GetLoc().toStringNoCol());
      //DoCompatFlag(sc, info, MAPINFOF2_CompatInvisibility);
      sc->Check("=");
      sc->CheckNumber();
    } else if (sc->Check("compat_missileclip")) {
      GCon->Logf("WARNING: %s: mapdef 'compat_missileclip' is not supported yet", *sc->GetLoc().toStringNoCol());
      //DoCompatFlag(sc, info, MAPINFOF2_CompatInvisibility);
      sc->Check("=");
      sc->CheckNumber();
    } else if (sc->Check("evenlighting")) {
      info->HorizWallShade = 0;
      info->VertWallShade = 0;
    } else if (sc->Check("vertwallshade")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
      info->VertWallShade = MID(-128, sc->Number, 127);
    } else if (sc->Check("horizwallshade")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
      info->HorizWallShade = MID(-128, sc->Number, 127);
    } else if (sc->Check("noinfighting")) {
      info->Infighting = -1;
    } else if (sc->Check("normalinfighting")) {
      info->Infighting = 0;
    } else if (sc->Check("totalinfighting")) {
      info->Infighting = 1;
    } else if (sc->Check("specialaction")) {
      if (newFormat) sc->Expect("=");
      VMapSpecialAction &A = info->SpecialActions.Alloc();
      sc->SetCMode(true);
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
      if (!A.Special) GCon->Logf("Unknown action special '%s'", *sc->String);
      memset(A.Args, 0, sizeof(A.Args));
      for (int i = 0; i < 5 && sc->Check(","); ++i) {
        sc->ExpectNumber();
        A.Args[i] = sc->Number;
      }
      if (!newFormat) sc->SetCMode(false);
    } else if (sc->Check("redirect")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectString();
      info->RedirectType = *sc->String.ToLower();
      info->RedirectMap = ParseNextMapName(sc, HexenMode);
    } else if (sc->Check("strictmonsteractivation")) {
      info->Flags2 &= ~MAPINFOF2_LaxMonsterActivation;
      info->Flags2 |= MAPINFOF2_HaveMonsterActivation;
    } else if (sc->Check("laxmonsteractivation")) {
      info->Flags2 |= MAPINFOF2_LaxMonsterActivation;
      info->Flags2 |= MAPINFOF2_HaveMonsterActivation;
    } else if (sc->Check("interpic") || sc->Check("exitpic")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectName8();
      info->ExitPic = *sc->String.ToLower();
    } else if (sc->Check("enterpic")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectName8();
      info->EnterPic = *sc->String.ToLower();
    } else if (sc->Check("intermusic")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectString();
      info->InterMusic = *sc->String.ToLower();
    } else if (sc->Check("cd_start_track")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
      //cd_NonLevelTracks[CD_STARTTRACK] = sc->Number;
    } else if (sc->Check("cd_end1_track")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
      //cd_NonLevelTracks[CD_END1TRACK] = sc->Number;
    } else if (sc->Check("cd_end2_track")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
      //cd_NonLevelTracks[CD_END2TRACK] = sc->Number;
    } else if (sc->Check("cd_end3_track")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
      //cd_NonLevelTracks[CD_END3TRACK] = sc->Number;
    } else if (sc->Check("cd_intermission_track")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
      //cd_NonLevelTracks[CD_INTERTRACK] = sc->Number;
    } else if (sc->Check("cd_title_track")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
      //cd_NonLevelTracks[CD_TITLETRACK] = sc->Number;
    }
    // these are stubs for now.
    else if (sc->Check("cdid")) {
      GCon->Logf("Unimplemented MAPINFO command cdid");
      if (newFormat) sc->Expect("=");
      sc->ExpectString();
    } else if (sc->Check("noinventorybar")) {
      GCon->Logf("Unimplemented MAPINFO command noinventorybar");
    } else if (sc->Check("airsupply")) {
      GCon->Logf("Unimplemented MAPINFO command airsupply");
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
    } else if (sc->Check("sndseq")) {
      GCon->Logf("Unimplemented MAPINFO command sndseq");
      if (newFormat) sc->Expect("=");
      sc->ExpectName8();
    } else if (sc->Check("sndinfo")) {
      GCon->Logf("Unimplemented MAPINFO command sndinfo");
      if (newFormat) sc->Expect("=");
      sc->ExpectName8();
    } else if (sc->Check("soundinfo")) {
      GCon->Logf("Unimplemented MAPINFO command soundinfo");
      if (newFormat) sc->Expect("=");
      sc->ExpectName8();
    } else if (sc->Check("allowcrouch")) {
      GCon->Logf("Unimplemented MAPINFO command allowcrouch");
    } else if (sc->Check("nocrouch")) {
      GCon->Logf("Unimplemented MAPINFO command nocrouch");
    } else if (sc->Check("pausemusicinmenus")) {
      GCon->Logf("Unimplemented MAPINFO command pausemusicinmenus");
    } else if (sc->Check("bordertexture")) {
      GCon->Logf("Unimplemented MAPINFO command bordertexture");
      if (newFormat) sc->Expect("=");
      sc->ExpectName8();
    } else if (sc->Check("f1")) {
      GCon->Logf("Unimplemented MAPINFO command f1");
      if (newFormat) sc->Expect("=");
      sc->ExpectString();
    } else if (sc->Check("allowrespawn")) {
      GCon->Logf("Unimplemented MAPINFO command allowrespawn");
    } else if (sc->Check("teamdamage")) {
      GCon->Logf("Unimplemented MAPINFO command teamdamage");
      if (newFormat) sc->Expect("=");
      sc->ExpectFloat();
    } else if (sc->Check("fogdensity")) {
      GCon->Logf("Unimplemented MAPINFO command fogdensity");
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
    } else if (sc->Check("outsidefogdensity")) {
      GCon->Logf("Unimplemented MAPINFO command outsidefogdensity");
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
    } else if (sc->Check("skyfog")) {
      GCon->Logf("Unimplemented MAPINFO command skyfog");
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
    } else if (sc->Check("teamplayon")) {
      GCon->Logf("Unimplemented MAPINFO command teamplayon");
    } else if (sc->Check("teamplayoff")) {
      GCon->Logf("Unimplemented MAPINFO command teamplayoff");
    } else if (sc->Check("checkswitchrange")) {
      GCon->Logf("Unimplemented MAPINFO command checkswitchrange");
    } else if (sc->Check("nocheckswitchrange")) {
      GCon->Logf("Unimplemented MAPINFO command nocheckswitchrange");
    } else if (sc->Check("translator")) {
      GCon->Logf("Unimplemented MAPINFO command translator");
      if (newFormat) sc->Expect("=");
      sc->ExpectString();
    } else if (sc->Check("resethealth")) {
      //sc->Message("WARNING: ignored ResetHealth");
    } else if (sc->Check("resetinventory")) {
      //sc->Message("WARNING: ignored ResetInventory");
    } else if (sc->Check("unfreezesingleplayerconversations")) {
      GCon->Logf("Unimplemented MAPINFO command unfreezesingleplayerconversations");
    } else if (sc->Check("smoothlighting")) {
      GCon->Logf("Unimplemented MAPINFO command SmoothLighting");
    } else if (sc->Check("lightmode")) {
      GCon->Logf("Unimplemented MAPINFO command 'LightMode'");
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
    } else if (sc->Check("Grinding_PolyObj")) {
      GCon->Logf("Unimplemented MAPINFO command 'Grinding_PolyObj'");
    } else {
      if (newFormat) {
        if (sc->Check("}")) break;
        sc->Error(va("invalid mapinfo command (%s)", *sc->String));
      }
      break;
    }
  }
  if (newFormat) sc->SetCMode(false);

  // second sky defaults to first sky
  if (info->Sky2Texture == GTextureManager.DefaultTexture) {
    info->Sky2Texture = info->Sky1Texture;
  }

  if (info->Flags&MAPINFOF_DoubleSky) {
    GTextureManager.SetFrontSkyLayer(info->Sky1Texture);
  }
  unguard;
}


static void ParseNameOrLookup (VScriptParser *sc, vuint32 lookupFlag, VStr *name, vuint32 *flags) {
  if (sc->Check("lookup")) {
    if (sc->IsCMode()) sc->Check(",");
    *flags |= lookupFlag;
    sc->ExpectString();
    *name = sc->String.ToLower();
  } else {
    sc->ResetQuoted();
    sc->ExpectString();
    if (sc->String.Length() > 1 && sc->String[0] == '$') {
      *flags |= lookupFlag;
      *name = VStr(*sc->String+1).ToLower();
    } else {
      *flags &= ~lookupFlag;
      *name = sc->String;
      //fprintf(stderr, "CMODE: %d; qs: %d\n", (int)sc->IsCMode(), (int)sc->QuotedString);
      while (sc->IsCMode() && sc->QuotedString) {
        sc->ResetCrossed();
        sc->ResetQuoted();
        if (!sc->GetString()) return;
        if (sc->Crossed) { sc->UnGet(); break; }
        if (sc->QuotedString) { sc->UnGet(); sc->Error("comma expected"); break; }
        if (sc->String != ",") { sc->UnGet(); break; }
        sc->ResetQuoted();
        if (!sc->GetString()) { sc->Error("unexpected EOF"); return; }
        if (!sc->QuotedString) { sc->UnGet(); sc->Error("quoted string expected"); break; }
        //fprintf(stderr, "  :::<%s>\n", *sc->String);
        *name += "\n";
        *name += sc->String;
      }
      //fprintf(stderr, "collected: <%s>\n", **name);
    }
  }
}

static void ParseNameOrLookup (VScriptParser *sc, vint32 lookupFlag, VStr *name, vint32 *flags) {
  vuint32 lf = (vuint32)lookupFlag;
  vuint32 flg = (vuint32)*flags;
  ParseNameOrLookup(sc, lf, name, &flg);
  *flags = (vint32)flg;
}


//==========================================================================
//
//  ParseMap
//
//==========================================================================
static void ParseMap(VScriptParser *sc, bool &HexenMode, mapInfo_t &Default) {
  guard(ParseMap);
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
    info->Flags |= MAPINFOF_NoIntermission |
      MAPINFOF_FallingDamage |
      MAPINFOF_MonsterFallingDamage |
      MAPINFOF_NoAutoSndSeq |
      MAPINFOF_ActivateOwnSpecial |
      MAPINFOF_MissilesActivateImpact |
      MAPINFOF_InfiniteFlightPowerup;
  }

  // set saved par time
  int par = findSavedPar(MapLumpName);
  if (par > 0) {
    //GCon->Logf(NAME_Init, "found dehacked par time for map '%s' (%d)", *MapLumpName, par);
    info->ParTime = par;
  }

  // map name must follow the number
  ParseNameOrLookup(sc, MAPINFOF_LookupName, &info->Name, &info->Flags);

  // set song lump name from SNDINFO script
  for (int i = 0; i < MapSongList.Num(); ++i) {
    if (MapSongList[i].MapName == info->LumpName) {
      info->SongLump = MapSongList[i].SongName;
    }
  }

  // set default levelnum for this map
  const char *mn = *MapLumpName;
  if (mn[0] == 'm' && mn[1] == 'a' && mn[2] == 'p' && mn[5] == 0) {
    int num = atoi(mn+3);
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
  unguard;
}


//==========================================================================
//
//  ParseClusterDef
//
//==========================================================================
static void ParseClusterDef (VScriptParser *sc) {
  guard(ParseClusterDef);
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

  bool newFormat = sc->Check("{");
  if (newFormat) sc->SetCMode(true);
  while (1) {
    /*
    if (sc->GetString()) {
      GCon->Logf("::: CLUSTER: <%s>", *sc->String);
      sc->UnGet();
    }
    */
    if (sc->Check("hub")) {
      CDef->Flags |= CLUSTERF_Hub;
    } else if (sc->Check("entertext")) {
      if (newFormat) sc->Expect("=");
      ParseNameOrLookup(sc, CLUSTERF_LookupEnterText, &CDef->EnterText, &CDef->Flags);
      //GCon->Logf("::: <%s>", *CDef->EnterText);
    } else if (sc->Check("entertextislump")) {
      CDef->Flags |= CLUSTERF_EnterTextIsLump;
    } else if (sc->Check("exittext")) {
      if (newFormat) sc->Expect("=");
      ParseNameOrLookup(sc, CLUSTERF_LookupExitText, &CDef->ExitText, &CDef->Flags);
    } else if (sc->Check("exittextislump")) {
      CDef->Flags |= CLUSTERF_ExitTextIsLump;
    } else if (sc->Check("flat")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectName8();
      CDef->Flat = sc->Name8;
      CDef->Flags &= ~CLUSTERF_FinalePic;
    } else if (sc->Check("pic")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectName8();
      CDef->Flat = sc->Name8;
      CDef->Flags |= CLUSTERF_FinalePic;
    } else if (sc->Check("music")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectName8();
      CDef->Music = sc->Name8;
    } else if (sc->Check("cdtrack")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
      //CDef->CDTrack = sc->Number;
    } else if (sc->Check("cdid")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
      //CDef->CDId = sc->Number;
    } else if (sc->Check("name")) {
      if (newFormat) sc->Expect("=");
      if (sc->Check("lookup")) {
        if (newFormat) sc->Expect(",");
      }
      sc->ExpectString();
      GCon->Logf("Unimplemented MAPINFO cluster command name");
    } else {
      if (newFormat) sc->Expect("}");
      break;
    }
  }
  if (newFormat) sc->SetCMode(false);

  // make sure text lump names are in lower case
  if (CDef->Flags&CLUSTERF_EnterTextIsLump) CDef->EnterText = CDef->EnterText.ToLower();
  if (CDef->Flags&CLUSTERF_ExitTextIsLump) CDef->ExitText = CDef->ExitText.ToLower();

  unguard;
}


//==========================================================================
//
//  ParseEpisodeDef
//
//==========================================================================
static void ParseEpisodeDef (VScriptParser *sc) {
  guard(ParseEpisodeDef);
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
  if (newFormat) sc->SetCMode(true);
  while (1) {
    if (sc->Check("name")) {
      if (newFormat) sc->Expect("=");
      ParseNameOrLookup(sc, EPISODEF_LookupText, &EDef->Text, &EDef->Flags);
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
  if (newFormat) sc->SetCMode(false);
  unguard;
}


//==========================================================================
//
//  ParseSkillDef
//
//==========================================================================
static void ParseSkillDef (VScriptParser *sc) {
  guard(ParseSkillDef);
  VSkillDef *SDef = nullptr;
  sc->ExpectString();

  // check for replaced skill
  for (int i = 0; i < SkillDefs.Num(); ++i) {
    if (!sc->String.ICmp(SkillDefs[i].Name)) {
      SDef = &SkillDefs[i];
      break;
    }
  }
  if (!SDef) {
    SDef = &SkillDefs.Alloc();
    SDef->Name = sc->String;
  }

  // set defaults
  SDef->AmmoFactor = 1.0;
  SDef->DoubleAmmoFactor = 2.0;
  SDef->DamageFactor = 1.0;
  SDef->RespawnTime = 0.0;
  SDef->RespawnLimit = 0;
  SDef->Aggressiveness = 1.0;
  SDef->SpawnFilter = 0;
  SDef->AcsReturn = SkillDefs.Num()-1;
  SDef->MenuName.Clean();
  SDef->PlayerClassNames.Clear();
  SDef->ConfirmationText.Clean();
  SDef->Key.Clean();
  SDef->TextColour.Clean();
  SDef->Flags = 0;

  while (1) {
    if (sc->Check("AmmoFactor")) {
      sc->ExpectFloat();
      SDef->AmmoFactor = sc->Float;
    } else if (sc->Check("DoubleAmmoFactor")) {
      sc->ExpectFloat();
      SDef->DoubleAmmoFactor = sc->Float;
    } else if (sc->Check("DamageFactor")) {
      sc->ExpectFloat();
      SDef->DamageFactor = sc->Float;
    } else if (sc->Check("FastMonsters")) {
      SDef->Flags |= SKILLF_FastMonsters;
    } else if (sc->Check("DisableCheats")) {
      SDef->Flags |= SKILLF_DisableCheats;
    } else if (sc->Check("EasyBossBrain")) {
      SDef->Flags |= SKILLF_EasyBossBrain;
    } else if (sc->Check("AutoUseHealth")) {
      SDef->Flags |= SKILLF_AutoUseHealth;
    } else if (sc->Check("RespawnTime")) {
      sc->ExpectFloat();
      SDef->RespawnTime = sc->Float;
    } else if (sc->Check("RespawnLimit")) {
      sc->ExpectNumber();
      SDef->RespawnLimit = sc->Number;
    } else if (sc->Check("Aggressiveness")) {
      sc->ExpectFloat();
      SDef->Aggressiveness = 1.0-MID(0.0, sc->Float, 1.0);
    } else if (sc->Check("SpawnFilter")) {
      if (sc->CheckNumber()) {
        if (sc->Number > 0 && sc->Number < 31) SDef->SpawnFilter = 1<<(sc->Number-1);
      } else {
             if (sc->Check("Baby")) SDef->SpawnFilter = 1;
        else if (sc->Check("Easy")) SDef->SpawnFilter = 2;
        else if (sc->Check("Normal")) SDef->SpawnFilter = 4;
        else if (sc->Check("Hard")) SDef->SpawnFilter = 8;
        else if (sc->Check("Nightmare")) SDef->SpawnFilter = 16;
        else sc->ExpectString();
      }
    } else if (sc->Check("ACSReturn")) {
      sc->ExpectNumber();
      SDef->AcsReturn = sc->Number;
    } else if (sc->Check("Name")) {
      sc->ExpectString();
      SDef->MenuName = sc->String;
      SDef->Flags &= ~SKILLF_MenuNameIsPic;
    } else if (sc->Check("PlayerClassName")) {
      VSkillPlayerClassName &CN = SDef->PlayerClassNames.Alloc();
      sc->ExpectString();
      CN.ClassName = sc->String;
      sc->ExpectString();
      CN.MenuName = sc->String;
    } else if (sc->Check("PicName")) {
      sc->ExpectString();
      SDef->MenuName = sc->String.ToLower();
      SDef->Flags |= SKILLF_MenuNameIsPic;
    } else if (sc->Check("MustConfirm")) {
      SDef->Flags |= SKILLF_MustConfirm;
      if (sc->CheckQuotedString()) SDef->ConfirmationText = sc->String;
    } else if (sc->Check("Key")) {
      sc->ExpectString();
      SDef->Key = sc->String;
    } else if (sc->Check("TextColor")) {
      sc->ExpectString();
      SDef->TextColour = sc->String;
    } else {
      break;
    }
  }
  unguard;
}


//==========================================================================
//
//  ParseMapInfo
//
//==========================================================================
static void ParseMapInfo (VScriptParser *sc) {
  guard(ParseMapInfo);
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
          sc = new VScriptParser(*sc->String, W_CreateLumpReaderNum(lmp));
          //ParseMapInfo(new VScriptParser(*sc->String, W_CreateLumpReaderNum(lmp)));
        } else {
          sc->Error(va("mapinfo include '%s' not found", *sc->String));
          error = true;
          break;
        }
      }
      // hack for "complete"
      else if (sc->Check("gameinfo")) {
        sc->Expect("{");
        for (;;) {
          if (sc->AtEnd()) { sc->Error("'}' not found"); break; }
          if (sc->Check("}")) break;
          sc->GetString();
        }
      } else if (sc->Check("intermission")) {
        GCon->Logf("Unimplemented MAPINFO command Intermission");
        sc->SkipBracketed();
      } else {
        sc->Error(va("Invalid command %s", *sc->String));
        error = true;
        break;
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
  unguard;
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
  guard(P_GetMapInfo);
  for (int i = 0; i < MapInfo.Num(); ++i) {
    if (map == MapInfo[i].LumpName) return MapInfo[i];
  }
  return DefaultMap;
  unguard;
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
  guard(P_GetMapNameByLevelNum);
  for (int i = 0; i < MapInfo.Num(); ++i) {
    if (MapInfo[i].LevelNum == map) return i;
  }
  // not found
  return 0;
  unguard;
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
  guard(P_TranslateMap);
  for (int i = MapInfo.length()-1; i >= 0; --i) {
    if (MapInfo[i].WarpTrans == map) return MapInfo[i].LumpName;
  }
  // not found
  return (MapInfo.length() > 0 ? MapInfo[0].LumpName : NAME_None);
  unguard;
}


//==========================================================================
//
//  P_TranslateMapEx
//
//  Returns the map lump name given a warp map number.
//
//==========================================================================
VName P_TranslateMapEx (int map) {
  guard(P_TranslateMap);
  for (int i = MapInfo.length()-1; i >= 0; --i) {
    if (MapInfo[i].WarpTrans == map) return MapInfo[i].LumpName;
  }
  // not found
  return NAME_None;
  unguard;
}


//==========================================================================
//
//  P_GetMapLumpNameByLevelNum
//
//  Returns the map lump name given a level number.
//
//==========================================================================
VName P_GetMapLumpNameByLevelNum (int map) {
  guard(P_GetMapNameByLevelNum);
  for (int i = 0; i < MapInfo.Num(); ++i) {
    if (MapInfo[i].LevelNum == map) return MapInfo[i].LumpName;
  }
  // not found, use map##
  return va("map%02d", map);
  unguard;
}


//==========================================================================
//
// P_PutMapSongLump
//
//==========================================================================
void P_PutMapSongLump (int map, VName lumpName) {
  guard(P_PutMapSongLump);
  FMapSongInfo &ms = MapSongList.Alloc();
  ms.MapName = va("map%02d", map);
  ms.SongName = lumpName;
  unguard;
}


//==========================================================================
//
//  P_GetClusterDef
//
//==========================================================================
const VClusterDef *P_GetClusterDef (int Cluster) {
  guard(P_GetClusterDef);
  for (int i = 0; i < ClusterDefs.Num(); ++i) {
    if (Cluster == ClusterDefs[i].Cluster) return &ClusterDefs[i];
  }
  return &DefaultClusterDef;
  unguard;
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
  guard(P_GetNumMaps);
  return MapInfo.Num();
  unguard;
}


//==========================================================================
//
//  P_GetMapInfo
//
//==========================================================================
mapInfo_t *P_GetMapInfoPtr (int mapidx) {
  guard(P_GetMapInfo);
  return (mapidx >= 0 && mapidx < MapInfo.Num() ? &MapInfo[mapidx] : nullptr);
  unguard;
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
  guard(P_GetMusicLumpNames);
  for (int i = 0; i < MapInfo.Num(); ++i) {
    const char *MName = *MapInfo[i].SongLump;
    if (MName[0] == 'd' && MName[1] == '_') {
      FReplacedString &R = List.Alloc();
      R.Index = i;
      R.Replaced = false;
      R.Old = MName+2;
    }
  }
  unguard;
}


//==========================================================================
//
//  P_ReplaceMusicLumpNames
//
//==========================================================================
void P_ReplaceMusicLumpNames (TArray<FReplacedString> &List) {
  guard(P_ReplaceMusicLumpNames);
  for (int i = 0; i < List.Num(); ++i) {
    if (List[i].Replaced) {
      MapInfo[List[i].Index].SongLump = VName(*(VStr("d_")+List[i].New), VName::AddLower8);
    }
  }
  unguard;
}


//==========================================================================
//
//  P_SetParTime
//
//==========================================================================
void P_SetParTime (VName Map, int Par) {
  guard(P_SetParTime);
  if (Map == NAME_None || Par < 0) return;
  if (mapinfoParsed) {
    for (int i = 0; i < MapInfo.length(); ++i) {
      if (MapInfo[i].LumpName == NAME_None) continue;
      if (VStr::ICmp(*MapInfo[i].LumpName, *Map) == 0) {
        MapInfo[i].ParTime = Par;
        return;
      }
    }
    GCon->Logf("WARNING! No such map '%s'", *Map);
  } else {
    ParTimeInfo &pi = partimes.alloc();
    pi.MapName = Map;
    pi.par = Par;
  }
  unguard;
}


//==========================================================================
//
//  IsMapPresent
//
//==========================================================================
bool IsMapPresent (VName MapName) {
  guard(IsMapPresent);
  if (W_CheckNumForName(MapName) >= 0) return true;
  VStr FileName = va("maps/%s.wad", *MapName);
  if (FL_FileExists(FileName)) return true;
  return false;
  unguard;
}


//==========================================================================
//
//  COMMAND MapList
//
//==========================================================================
COMMAND(MapList) {
  guard(COMMAND MapList);
  for (int i = 0; i < MapInfo.Num(); ++i) {
    if (IsMapPresent(MapInfo[i].LumpName)) {
      GCon->Log(VStr(MapInfo[i].LumpName)+" - "+(MapInfo[i].Flags&MAPINFOF_LookupName ? GLanguage[*MapInfo[i].Name] : MapInfo[i].Name));
    }
  }
  unguard;
}


//==========================================================================
//
//  ShutdownMapInfo
//
//==========================================================================
void ShutdownMapInfo () {
  guard(ShutdownMapInfo);
  DefaultMap.Name.Clean();
  MapInfo.Clear();
  MapSongList.Clear();
  ClusterDefs.Clear();
  EpisodeDefs.Clear();
  SkillDefs.Clear();
  unguard;
}
