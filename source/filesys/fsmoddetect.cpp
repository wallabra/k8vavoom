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
// directly included from "files.cpp"
enum {
  //AD_NONE,
  AD_SKULLDASHEE = 1,
  AD_HARMONY,
  AD_SQUARE,
  AD_HMOON,
};


//==========================================================================
//
//  detectSkullDashEE
//
//==========================================================================
static int detectSkullDashEE (FSysModDetectorHelper &hlp, int seenZScriptLump) {
  if (hlp.hasFile("maps/titlemap.wad", 286046, "def4f5e00c60727aeb3a25d1982cfcf1")) {
    GCon->Logf(NAME_Init, "Detected mod: SkullDash EE");
    return AD_SKULLDASHEE;
  }
  return AD_NONE;
}


//==========================================================================
//
//  detectBoringTernity
//
//==========================================================================
static int detectBoringTernity (FSysModDetectorHelper &hlp, int seenZScriptLump) {
  if (seenZScriptLump < 0) return AD_NONE;
  if (hlp.checkLump(seenZScriptLump, 13153, "9e53e2d46de1d0f6cfc004c74e1660cf")) {
    GLog.Log(NAME_Init, "Detected PWAD: boringternity");
    return -1; // no special actions needed, but enable zscript
  }
  return AD_NONE;
}


//==========================================================================
//
//  detectCzechbox
//
//==========================================================================
static int detectCzechbox (FSysModDetectorHelper &hlp, int seenZScriptLump) {
  if (hlp.hasLump("dehacked", 1066, "6bf56571d1f34d7cd7378b95556d67f8")) {
    GLog.Log(NAME_Init, "Detected PWAD: CzechBox release");
    return AD_NONE;
  }
  if (hlp.hasLump("dehacked", 1072, "b93dbb8163e0a512e7b76d60d885b41c")) {
    GLog.Log(NAME_Init, "Detected PWAD: CzechBox beta");
    return AD_NONE;
  }
  return AD_NONE;
}


//==========================================================================
//
//  detectHarmony
//
//  detect Harmony v1.1
//
//==========================================================================
static int detectHarmony (FSysModDetectorHelper &hlp, int seenZScriptLump) {
  if (hlp.hasLump("dehacked", 26287, "3446842b93dfa37075a238ccd5b0f29c")) {
    GLog.Log(NAME_Init, "Detected standalone TC: Harmony v1.1");
    mdetect_SetGameName("doom2");
    mdetect_ClearAndBlockCustomModes();
    mdetect_DisableBDW();
    mdetect_DisableIWads();
    return AD_HARMONY;
  }
  return AD_NONE;
}


//==========================================================================
//
//  detectSquare
//
//  detect Adventures Of Square
//
//==========================================================================
static int detectSquare (FSysModDetectorHelper &hlp, int seenZScriptLump) {
  if (seenZScriptLump >= 0 &&
      hlp.hasFile("SQU-SWE1.txt") &&
      hlp.hasFile("GAMEINFO.sq") &&
      hlp.hasFile("acs/sqcommon.o") &&
      hlp.hasFile("acs/sq_jump.o"))
  {
    GLog.Log(NAME_Init, "Detected TC: Adventures of Square");
    mdetect_ClearAndBlockCustomModes();
    mdetect_DisableBDW();
    mdetect_DisableGore();
    //rforce_disable_sprofs = true; // disable sprite offsets
    //fsys_DisableBloodReplacement = true;
    return AD_SQUARE;
  }
  return AD_NONE;
}


//==========================================================================
//
//  checkGameInfo
//
//==========================================================================
static bool checkGameInfo (FSysModDetectorHelper &hlp, const char *gameName) {
  if (!gameName || !gameName[0]) return false;
  int fidx = hlp.findFile("GAMEINFO");
  if (fidx < 0) return false;
  if (hlp.getLumpSize(fidx) > 16384) return false; // sanity check
  VStream *gi = hlp.createLumpReader(fidx);
  if (!gi) return false;
  VScriptParser *sc = new VScriptParser("gameinfo", gi);
  sc->SetCMode(true);
  while (sc->GetString()) {
    if (!sc->String.strEquCI("STARTUPTITLE")) continue;
    if (!sc->Check("=")) continue;
    if (!sc->GetString()) break;
    if (sc->String.strEquCI(gameName)) {
      delete sc;
      return true;
    }
  }
  delete sc;
  return false;
}


//==========================================================================
//
//  detectHuntersMoon
//
//  detect Hunter's Moon
//
//==========================================================================
static int detectHuntersMoon (FSysModDetectorHelper &hlp, int seenZScriptLump) {
  if (seenZScriptLump >= 0) return AD_NONE;
  if (!checkGameInfo(hlp, "Hunter's Moon")) return AD_NONE;
  GLog.Log(NAME_Init, "Detected mod: Hunter's Moon");
  mdetect_ClearAndBlockCustomModes();
  mdetect_DisableBDW();
  //mdetect_DisableGore();
  //return AD_HMOON;
  return AD_NONE; // no reason to block other detection
}


//==========================================================================
//
//  detectVariousShit
//
//==========================================================================
static int detectVariousShit (FSysModDetectorHelper &hlp, int seenZScriptLump) {
  if (seenZScriptLump >= 0) return AD_NONE;
  if (hlp.hasFile("MAPINFO.txt", 2306, "47b7e6bcc4024d428909a0f3f7b05126")) {
    GLog.Log(NAME_Init, "Detected PWAD: grinshit; switching to plutonia");
    mdetect_SetGameName("plutonia");
    return AD_NONE;
  }
  return AD_NONE; // no reason to block other detection
}


//==========================================================================
//
//  FL_RegisterModDetectors
//
//==========================================================================
static void FL_RegisterModDetectors () {
  fsysRegisterModDetector(&detectSkullDashEE);
  fsysRegisterModDetector(&detectBoringTernity);
  fsysRegisterModDetector(&detectCzechbox);
  fsysRegisterModDetector(&detectHarmony);
  fsysRegisterModDetector(&detectSquare);
  fsysRegisterModDetector(&detectHuntersMoon);
  fsysRegisterModDetector(&detectVariousShit);
}
