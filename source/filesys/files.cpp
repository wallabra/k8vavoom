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
#include "gamedefs.h"
#include "../../libs/core/fsys/fsys_local.h"


// from corelib, sorry!
extern TArray<VSearchPath *> fsysSearchPaths;

extern VCvarB game_release_mode;
//extern VCvarI game_override_mode;
extern int cli_NoZMapinfo; // from mapinfo.cpp
int cli_NoExternalDeh = 0;

static VCvarB dbg_dump_gameinfo("dbg_dump_gameinfo", false, "Dump parsed game.txt?", CVAR_PreInit);
static VCvarB gz_skip_menudef("_gz_skip_menudef", false, "Skip gzdoom menudef parsing?", CVAR_PreInit);

static VCvarB __dbg_debug_preinit("__dbg_debug_preinit", false, "Dump preinits?", CVAR_PreInit);

VCvarS game_name("game_name", "unknown", "The Name Of The Game.", CVAR_Rom);

int cli_WAll = 0;

bool fsys_hasPwads = false; // or paks
bool fsys_hasMapPwads = false; // or paks
bool fsys_DisableBloodReplacement = false; // for custom modes


int fsys_warp_n0 = -1;
int fsys_warp_n1 = -1;
VStr fsys_warp_cmd;

static bool fsys_onlyOneBaseFile = false;

static VStr cliGameMode;
static const char *cliGameCStr = nullptr;
static TArray<VStr> autoloadDirList;

GameOptions game_options;

extern VCvarB respawnparm;
extern VCvarI fastparm;
extern VCvarB NoMonsters;
extern VCvarI Skill;

int cli_NoMonsters = -1; // not specified
int cli_CompileAndExit = 0;
static int cli_FastMonsters = -1; // not specified
static int cli_Respawn = -1; // not specified
static int cli_NoMenuDef = -1; // not specified
/*static*/ int cli_GoreMod = -1; // not specified
static int cli_GoreModForce = 0;
static int cli_BDWMod = -1; // not specified
static int cli_SkeeHUD = -1; // not specified

static const char *cli_BaseDir = nullptr;
static const char *cli_IWadName = nullptr;
static const char *cli_IWadDir = nullptr;
static const char *cli_SaveDir = nullptr;
static const char *cli_ConfigDir = nullptr;

static int reportIWads = 0;
static int reportPWads = 1;
static int cli_NakedBase = -1;


struct MainWadFiles {
  VStr main;
  TArray<VStr> aux; // list of additinal files (if fname starts with "?", the file is optional)
  VStr description;
};


struct version_t {
  VStr description; // game description (may be empty)
  VStr gamename; // cannot be empty
  TArray<MainWadFiles> mainWads; // list of wads (ony one is required)
  TArray<VStr> params; // list of CLI params (always contains at least one item)
  TArray<VStr> defines; // VavoomC defines to add
  VStr GameDir; // main game dir
  TArray<VStr> BaseDirs; // common base dirs
  //int ParmFound;
  bool FixVoices;
  VStr warp; // warp template
  TArray<VStr> filters;
  GameOptions options;
};


VStr fl_configdir;
VStr fl_basedir;
VStr fl_savedir;
VStr fl_gamedir;

static TArray<VStr> IWadDirs;
static int IWadIndex;
static VStr warpTpl;
static TArray<ArgVarValue> preinitCV;
static ArgVarValue emptyAV;

static TArray<VStr> cliModesList;
static int cli_oldSprites = 0;


//==========================================================================
//
//  getNetPath
//
//==========================================================================
static VStr getNetPath (VSearchPath *sp) {
  if (!sp) return VStr::EmptyString;
  if (sp->cosmetic) return VStr::EmptyString;
  if (sp->IsNonPak()) return VStr::EmptyString;
  VStr fname = sp->GetPrefix();
  VStr bname = fname.extractFileName();
  int colidx = bname.lastIndexOf(':');
  if (colidx >= 0) bname = bname.mid(colidx+1, bname.length());
  if (bname.isEmpty()) return VStr::EmptyString;
  if (bname.strEquCI("basepak.pk3")) {
    while (!fname.isEmpty() && fname[fname.length()-1] != '/' && fname[fname.length()-1] != '\\') fname.chopRight(1);
    fname.chopRight(1);
    VStr xname = fname.extractFileName();
    if (!xname.isEmpty()) bname = xname+"/"+bname;
  }
  return bname;
}


// ////////////////////////////////////////////////////////////////////////// //
extern "C" {
  int cliHelpSorter (const void *aa, const void *bb, void *) {
    if (aa == bb) return 0;
    const VParsedArgs::ArgHelp *a = (const VParsedArgs::ArgHelp *)aa;
    const VParsedArgs::ArgHelp *b = (const VParsedArgs::ArgHelp *)bb;
    // if any of it doesn't have help...
    if (!!(a->arghelp) != !!(b->arghelp)) {
      if (a->arghelp) {
        vassert(!b->arghelp);
        return -1;
      }
      if (b->arghelp) {
        vassert(!a->arghelp);
        return 1;
      }
      vassert(0);
    }
    return VStr::ICmp(a->argname, b->argname);
  }
}


void FL_CollectPreinits () {
  if (GArgs.CheckParm("-help") || GArgs.CheckParm("--help")) {
    TArray<VParsedArgs::ArgHelp> list;
    VParsedArgs::GetArgList(list);
    if (list.length()) {
      timsort_r(list.ptr(), list.length(), sizeof(VParsedArgs::ArgHelp), &cliHelpSorter, nullptr);
      int maxlen = 0;
      for (auto &&ainfo : list) {
        int len = (int)strlen(ainfo.argname);
        if (maxlen < len) maxlen = len;
      }
      for (auto &&ainfo : list) {
        #ifdef _WIN32
        fprintf(stderr, "%*s -- %s\n", -maxlen, ainfo.argname, ainfo.arghelp);
        #else
        GLog.Logf("%*s -- %s", -maxlen, ainfo.argname, ainfo.arghelp);
        #endif
      }
    }
    Z_Exit(0);
  }

  if (GArgs.CheckParm("-help-developer") || GArgs.CheckParm("--help-developer") ||
      GArgs.CheckParm("-help-dev") || GArgs.CheckParm("--help-dev"))
  {
    TArray<VParsedArgs::ArgHelp> list;
    VParsedArgs::GetArgList(list, true);
    if (list.length()) {
      timsort_r(list.ptr(), list.length(), sizeof(VParsedArgs::ArgHelp), &cliHelpSorter, nullptr);
      int maxlen = 0;
      for (auto &&ainfo : list) {
        int len = (int)strlen(ainfo.argname);
        if (maxlen < len) maxlen = len;
      }
      for (auto &&ainfo : list) {
        #ifdef _WIN32
        fprintf(stderr, "%*s -- %s\n", -maxlen, ainfo.argname, (ainfo.arghelp ? ainfo.arghelp : "undocumented"));
        #else
        GLog.Logf("%*s -- %s", -maxlen, ainfo.argname, (ainfo.arghelp ? ainfo.arghelp : "undocumented"));
        #endif
      }
    }
    Z_Exit(0);
  }

  int aidx = 1;
  bool inMinus = false;
  while (aidx < GArgs.Count()) {
    const char *args = GArgs[aidx];
    //GLog.Logf("FL_CollectPreinits: argc=%d; aidx=%d; inminus=%d; args=<%s>", GArgs.Count(), aidx, (int)inMinus, args);
    if (!inMinus) {
      if (!args[0]) {
        GArgs.removeAt(aidx);
        continue;
      }
    }
    ++aidx;
    // skip "-" commands
    if (args[0] == '-') {
      inMinus = true;
      continue;
    }
    if (args[0] != '+') {
      continue;
    }
    // stray "+"?
    if (!args[1]) {
      inMinus = true;
      continue;
    }
    inMinus = false;
    // extract command name
    int spos = 2;
    while (args[spos] && (vuint8)(args[spos]) > ' ') ++spos;
    VStr vname = VStr(args+1, spos-1);
    int flg = VCvar::GetVarFlags(*vname);
    if (flg < 0 || !(flg&CVAR_PreInit)) {
      inMinus = true;
      continue;
    }
    // collect value
    VStr val;
    while (args[spos] && (vuint8)(args[spos]) <= ' ') ++spos;
    if (args[spos]) {
      // in the same arg
      /*
      if (aidx < GArgs.Count()) {
        const char *a2 = GArgs[aidx];
        if (a2[0] != '+' && a2[0] != '-') {
          inMinus = true;
          continue;
        }
      }
      */
      val = VStr(args+spos);
      // remove this arg
      --aidx;
      GArgs.removeAt(aidx);
    } else {
      // in another arg
      if (aidx >= GArgs.Count()) break;
      /*
      if (aidx+1 < GArgs.Count()) {
        const char *a2 = GArgs[aidx+1];
        if (a2[0] != '+' && a2[0] != '-') {
          inMinus = true;
          continue;
        }
      }
      */
      val = VStr(GArgs[aidx]);
      // remove two args
      --aidx;
      GArgs.removeAt(aidx);
      GArgs.removeAt(aidx);
    }
    // save it
    ArgVarValue &vv = preinitCV.alloc();
    vv.varname = vname;
    vv.value = val;
    //GLog.Logf("FL_CollectPreinits:   new var '%s' with value '%s'", *vname, *val);
    //HACK!
    if (vname.ICmp("__dbg_debug_preinit") == 0) VCvar::Set(*vname, val);
  }
  if (__dbg_debug_preinit && preinitCV.length()) {
    for (int f = 0; f < GArgs.Count(); ++f) GCon->Logf("::: %d: <%s>", f, GArgs[f]);
    for (int f = 0; f < preinitCV.length(); ++f) {
      const ArgVarValue &vv = preinitCV[f];
      GCon->Logf(":DOSET:%d: <%s> = <%s>", f, *vv.varname.quote(), *vv.value.quote());
    }
  }
}


void FL_ProcessPreInits () {
  if (__dbg_debug_preinit) {
    if (preinitCV.length()) for (int f = 0; f < GArgs.Count(); ++f) GCon->Logf("::: %d: <%s>", f, GArgs[f]);
  }
  for (int f = 0; f < preinitCV.length(); ++f) {
    const ArgVarValue &vv = preinitCV[f];
    VCvar::Set(*vv.varname, vv.value);
    if (__dbg_debug_preinit) GCon->Logf(":SET:%d: <%s> = <%s>", f, *vv.varname.quote(), *vv.value.quote());
  }
}


bool FL_HasPreInit (VStr varname) {
  if (varname.length() == 0 || preinitCV.length() == 0) return false;
  for (int f = 0; f < preinitCV.length(); ++f) {
    if (preinitCV[f].varname.ICmp(varname) == 0) return true;
  }
  return false;
}


void FL_ClearPreInits () {
  preinitCV.clear();
}


// used to set "preinit" cvars
int FL_GetPreInitCount () {
  return preinitCV.length();
}


const ArgVarValue &FL_GetPreInitAt (int idx) {
  if (idx < 0 || idx >= preinitCV.length()) return emptyAV;
  return preinitCV[idx];
}


// ////////////////////////////////////////////////////////////////////////// //
struct GroupMask {
  VStr mask;
  bool enabled;

  bool isGlob () const { return (mask.indexOf('*') >= 0 || mask.indexOf('?') >= 0 || mask.indexOf('[') >= 0); }
  bool match (VStr s) const { return s.globmatch(mask, false); }
};


struct GroupPwadInfo {
  VStr filename; // with path, maybe
  bool cosmetic; // is this pwad a cosmetic one (i.e. doesn't affect saves?)
};


struct CustomModeInfo {
  VStr name;
  TArray<VStr> aliases;
  TArray<GroupPwadInfo> pwads;
  TArray<GroupPwadInfo> postpwads;
  TArray<GroupMask> autoskips;
  bool disableBloodReplacement;
  bool disableGoreMod;
  bool disableBDW;
  VStr basedir;
  // has any sense only for modes loaded from "~/.k8vavoom/modes.rc"
  TArray<VStr> basedirglob;
  bool reported;

  CustomModeInfo () : name(), aliases(), pwads(), postpwads(), autoskips(), disableBloodReplacement(false), disableGoreMod(false), disableBDW(false), basedir(), basedirglob(), reported(false) {}

  CustomModeInfo (const CustomModeInfo &src) : name(), aliases(), pwads(), postpwads(), autoskips(), disableBloodReplacement(false), disableGoreMod(false), disableBDW(false), basedir(), basedirglob(), reported(false) { copyFrom(src); }

  CustomModeInfo &operator = (const CustomModeInfo &src) { copyFrom(src); return *this; }

  void clear () {
    name.clear();
    aliases.clear();
    pwads.clear();
    postpwads.clear();
    autoskips.clear();
    disableBloodReplacement = false;
    disableGoreMod = false;
    disableBDW = false;
    basedirglob.clear();
    reported = false;
  }

  void appendPWad (VStr str, bool asCosmetic) {
    if (str.isEmpty()) return;
    for (auto &&s : pwads) if (s.filename.strEqu(str)) return;
    GroupPwadInfo &pi = pwads.alloc();
    pi.filename = str;
    pi.cosmetic = asCosmetic;
  }

  void appendPostPWad (VStr str, bool asCosmetic) {
    if (str.isEmpty()) return;
    for (auto &&s : postpwads) if (s.filename.strEqu(str)) return;
    GroupPwadInfo &pi = postpwads.alloc();
    pi.filename = str;
    pi.cosmetic = asCosmetic;
  }

  void copyFrom (const CustomModeInfo &src) {
    if (&src == this) return;
    clear();
    name = src.name;
    // copy aliases
    aliases.setLength(src.aliases.length());
    for (auto &&it : src.aliases.itemsIdx()) aliases[it.index()] = it.value();
    // copy pwads
    pwads.setLength(src.pwads.length());
    for (auto &&it : src.pwads.itemsIdx()) pwads[it.index()] = it.value();
    // copy postwads
    postpwads.setLength(src.postpwads.length());
    for (auto &&it : src.postpwads.itemsIdx()) postpwads[it.index()] = it.value();
    // copy autoskips
    autoskips.setLength(src.autoskips.length());
    for (auto &&it : src.autoskips.itemsIdx()) autoskips[it.index()] = it.value();
    // copy flags
    disableBloodReplacement = src.disableBloodReplacement;
    disableGoreMod = src.disableGoreMod;
    disableBDW = src.disableBDW;
    // copy other things
    basedir = src.basedir;
    basedirglob.setLength(src.basedirglob.length());
    for (auto &&it : src.basedirglob.itemsIdx()) basedirglob[it.index()] = it.value();
    reported = src.reported;
  }

  // used to build final mode definition
  void merge (const CustomModeInfo &src) {
    if (&src == this) return;
    for (auto &&s : src.pwads) appendPWad(s.filename, s.cosmetic);
    for (auto &&s : src.postpwads) appendPostPWad(s.filename, s.cosmetic);
    for (auto &&s : src.autoskips) autoskips.append(s);
    if (src.disableBloodReplacement) disableBloodReplacement = true;
    if (src.disableGoreMod) disableGoreMod = true;
    if (src.disableBDW) disableBDW = true;
  }

  void dump () const {
    GLog.Logf(NAME_Debug, "==== MODE: %s ====", *name);
    GLog.Logf(NAME_Debug, "basedir: <%s>", *basedir);
    GLog.Logf(NAME_Debug, "basedirglobs: %d", basedirglob.length());
    for (auto &&s : basedirglob) GLog.Logf(NAME_Debug, "  <%s>", *s);
    GLog.Logf(NAME_Debug, "pwads: %d", pwads.length());
    for (auto &&s : pwads) GLog.Logf(NAME_Debug, "  <%s>%s", *s.filename, (s.cosmetic ? " (cosmetic)" : ""));
    GLog.Logf(NAME_Debug, "postpwads: %d", postpwads.length());
    for (auto &&s : postpwads) GLog.Logf(NAME_Debug, "  <%s>%s", *s.filename, (s.cosmetic ? " (cosmetic)" : ""));
    GLog.Logf(NAME_Debug, "autoskips: %d", autoskips.length());
    for (auto &&s : autoskips) GLog.Logf(NAME_Debug, "  <%s> (%d)", *s.mask, (int)s.enabled);
    GLog.Logf(NAME_Debug, "disableBloodReplacement: %s", (disableBloodReplacement ? "tan" : "ona"));
    GLog.Logf(NAME_Debug, "disableGoreMod: %s", (disableGoreMod ? "tan" : "ona"));
    GLog.Logf(NAME_Debug, "disableBDW: %s", (disableBDW ? "tan" : "ona"));
    GLog.Log(NAME_Debug, "========");
  }

  bool isMyAlias (VStr s) const {
    s = s.xstrip();
    if (s.isEmpty()) return false;
    for (auto &a : aliases) if (a.strEquCI(s)) return true;
    return false;
  }

  static VStr stripBaseDirShit (VStr dirname) {
    while (!dirname.isEmpty() && (dirname[0] == '/' || dirname[0] == '\\')) dirname.chopLeft(1);
    while (!dirname.isEmpty() && (dirname[dirname.length()-1] == '/' || dirname[dirname.length()-1] == '\\')) dirname.chopRight(1);
    if (dirname.startsWithCI("basev/")) dirname.chopLeft(6);
    return dirname;
  }

  bool isGoodBaseDir (VStr dirname) const {
    if (basedirglob.length() == 0) return true;
    dirname = stripBaseDirShit(dirname);
    if (dirname.isEmpty()) return true;
    for (auto &g : basedirglob) if (dirname.globMatchCI(g)) return true;
    return false;
  }

  // `mode` keyword skipped, expects mode name
  // mode must be cleared
  void parse (VScriptParser *sc) {
    //clear();
    sc->ExpectString();
    name = sc->String;
    sc->Expect("{");
    while (!sc->Check("}")) {
      if (sc->Check("pwad")) {
        bool cosmetic = true;
        for (;;) {
          sc->ExpectString();
          if (sc->String.isEmpty()) continue;
          if (sc->String[0] == '!') {
            sc->String.chopLeft(1);
            if (sc->String.isEmpty()) continue;
            cosmetic = false;
          }
          appendPWad(sc->String, cosmetic);
          cosmetic = true;
          if (!sc->Check(",")) break;
        }
      } else if (sc->Check("postpwad")) {
        bool cosmetic = true;
        for (;;) {
          sc->ExpectString();
          if (sc->String.isEmpty()) continue;
          if (sc->String[0] == '!') {
            sc->String.chopLeft(1);
            if (sc->String.isEmpty()) continue;
            cosmetic = false;
          }
          appendPostPWad(sc->String, cosmetic);
          cosmetic = true;
          if (!sc->Check(",")) break;
        }
      } else if (sc->Check("skipauto")) {
        for (;;) {
          sc->ExpectString();
          sc->String = sc->String.xstrip();
          if (!sc->String.isEmpty()) {
            GroupMask &gi = autoskips.alloc();
            gi.mask = sc->String;
            gi.enabled = false;
          }
          if (!sc->Check(",")) break;
        }
      } else if (sc->Check("forceauto")) {
        for (;;) {
          sc->ExpectString();
          GroupMask &gi = autoskips.alloc();
          gi.mask = sc->String;
          gi.enabled = true;
          if (!sc->Check(",")) break;
        }
      } else if (sc->Check("DisableBloodReplacement")) {
        disableBloodReplacement = true;
      } else if (sc->Check("DisableGoreMod")) {
        disableGoreMod = true;
      } else if (sc->Check("DisableBDW")) {
        disableBDW = true;
      } else if (sc->Check("alias")) {
        sc->ExpectString();
        VStr k = sc->String.xstrip();
        if (!k.isEmpty()) aliases.append(k);
      } else if (sc->Check("basedir")) {
        sc->ExpectString();
        VStr k = stripBaseDirShit(sc->String.xstrip());
        if (!k.isEmpty()) basedirglob.append(k);
      } else {
        sc->Error(va("unknown command '%s'", *sc->String));
      }
    }
  }
};


static CustomModeInfo customMode;
static TArray<CustomModeInfo> userModes; // from "~/.k8vavoom/modes.rc"
static TArray<GroupPwadInfo> postPWads; // from autoload
static bool modDetectorDisabledIWads = false;
static bool modNoBaseSprOfs = false;
static TArray<VStr> modAddMods;


//==========================================================================
//
//  mdetect_AddMod
//
//==========================================================================
VVA_OKUNUSED static void mdetect_AddMod (VStr s) {
  if (s.isEmpty()) return;
  for (auto &&mm : modAddMods) if (mm.strEquCI(s)) return;
  modAddMods.append(s);
}


//==========================================================================
//
//  mdetect_ClearAndBlockCustomModes
//
//==========================================================================
VVA_OKUNUSED static void mdetect_ClearAndBlockCustomModes () {
  customMode.clear();
  userModes.clear();
  postPWads.clear();
  cli_NakedBase = 1; // ignore autoloads
  fsys_onlyOneBaseFile = true;
}


//==========================================================================
//
//  mdetect_DisableBDW
//
//==========================================================================
VVA_OKUNUSED static void mdetect_DisableBDW () {
  if (!fsys_DisableBDW) GCon->Logf(NAME_Init, "BDW mod disabled.");
  fsys_DisableBDW = true;
}


//==========================================================================
//
//  mdetect_DisableGore
//
//==========================================================================
VVA_OKUNUSED static void mdetect_DisableGore () {
  if (cli_GoreMod != 0) GCon->Logf(NAME_Init, "Gore mod disabled.");
  cli_GoreMod = 0;
}


//==========================================================================
//
//  mdetect_DisableIWads
//
//==========================================================================
VVA_OKUNUSED static void mdetect_DisableIWads () {
  if (!modDetectorDisabledIWads) GCon->Logf(NAME_Init, "IWAD disabled.");
  modDetectorDisabledIWads = true;
}


//==========================================================================
//
//  mdetect_SetGameName
//
//==========================================================================
VVA_OKUNUSED static void mdetect_SetGameName (VStr gname) {
  cliGameMode = gname;
}


#include "fsmoddetect.cpp"


//==========================================================================
//
//  LoadModesFromStream
//
//  deletes stream
//
//==========================================================================
static void LoadModesFromStream (VStream *rcstrm, TArray<CustomModeInfo> &modes, VStr basedir=VStr::EmptyString) {
  if (!rcstrm) return;
  VScriptParser *sc = new VScriptParser(rcstrm->GetName(), rcstrm);
  while (!sc->AtEnd()) {
    if (sc->Check("alias")) {
      sc->ExpectString();
      VStr k = sc->String.xstrip();
      sc->Expect("is");
      sc->ExpectString();
      VStr v = sc->String.xstrip();
      if (!k.isEmpty() && !v.isEmpty() && !k.strEquCI(v)) {
        bool found = false;
        for (int f = 0; f < modes.length(); ++f) {
          if (modes[f].name.strEquCI(v)) {
            found = true;
            modes[f].aliases.append(k);
            break;
          }
        }
        if (!found) sc->Message(va("cannot set alias '%s', because mode '%s' is unknown", *k, *v));
      }
      continue;
    }
    sc->Expect("mode");
    CustomModeInfo mode;
    mode.clear();
    mode.basedir = basedir;
    mode.parse(sc);
    // append mode
    bool found = false;
    for (int f = 0; f < modes.length(); ++f) {
      if (modes[f].name.strEquCI(mode.name)) {
        // i found her!
        found = true;
        modes[f] = mode;
        break;
      }
    }
    if (!found) modes.append(mode);
  }
  delete sc;
}


//==========================================================================
//
//  ParseUserModes
//
//  "game_name" cvar must be set
//
//==========================================================================
static void ParseUserModes () {
  if (game_name.asStr().isEmpty()) return; // just in case
  VStream *rcstrm = FL_OpenFileReadInCfgDir("modes.rc");
  if (!rcstrm) return;
  GCon->Logf(NAME_Init, "parsing user mode definitions from '%s'...", *rcstrm->GetName());

  // load modes
  LoadModesFromStream(rcstrm, userModes);
}


//==========================================================================
//
//  ApplyUserModes
//
//==========================================================================
static void ApplyUserModes (VStr basedir) {
  // apply modes
  for (auto &&mname : cliModesList) {
    CustomModeInfo *nfo = nullptr;
    for (auto &&mode : userModes) {
      if (mode.isGoodBaseDir(basedir) && mode.name.strEquCI(mname)) {
        nfo = &mode;
        break;
      }
    }
    if (!nfo) {
      for (auto &&mode : userModes) {
        if (mode.isGoodBaseDir(basedir) && mode.isMyAlias(mname)) {
          nfo = &mode;
          break;
        }
      }
    }
    if (nfo) {
      if (!nfo->reported) {
        nfo->reported = true;
        GCon->Logf(NAME_Init, "activating user mode '%s'", *nfo->name);
      }
      customMode.merge(*nfo);
    }
  }
}


//==========================================================================
//
//  SetupCustomMode
//
//==========================================================================
static void SetupCustomMode (VStr basedir) {
  //customMode.clear();

  if (!basedir.isEmpty() && !basedir.endsWith("/")) basedir += "/";
  VStream *rcstrm = FL_OpenSysFileRead(basedir+"modes.rc");
  if (!rcstrm) return;
  GCon->Logf(NAME_Init, "parsing mode definitions from '%s'...", *rcstrm->GetName());

  // load modes
  TArray<CustomModeInfo> modes;
  LoadModesFromStream(rcstrm, modes, basedir);
  if (modes.length() == 0) return; // nothing to do

  // build active mode
  customMode.basedir = basedir;
  for (auto &&mname : cliModesList) {
    const CustomModeInfo *nfo = nullptr;
    for (auto &&mode : modes) {
      if (mode.name.strEquCI(mname)) {
        nfo = &mode;
        break;
      }
    }
    if (!nfo) {
      for (auto &&mode : modes) {
        if (mode.isMyAlias(mname)) {
          nfo = &mode;
          break;
        }
      }
    }
    if (nfo) {
      GCon->Logf(NAME_Init, "activating mode '%s'", *nfo->name);
      customMode.merge(*nfo);
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
struct PWadFile {
  VStr fname;
  bool skipSounds;
  bool skipSprites;
  bool skipDehacked;
  bool storeInSave;
  bool asDirectory;

  PWadFile ()
    : fname()
    , skipSounds(false)
    , skipSprites(false)
    , skipDehacked(false)
    , storeInSave(false)
    , asDirectory(false)
  {}
};


static TArray<PWadFile> pwadList;
static int doStartMap = 0;


static int pwflag_SkipSounds = 0;
static int pwflag_SkipSprites = 0;
static int pwflag_SkipDehacked = 0;
static int pwflag_SkipSaveList = 0;


//**************************************************************************
//
// pwad scanner (used to pre-scan pwads to extract various things)
//
//**************************************************************************

struct PWadScanInfo {
  bool processed;
  VStr iwad; // guessed from gameinfo; no path
  VStr mapname; // name of the arbitrary map from pwads
  int episode; // 0: doom2
  int mapnum;
  bool hasMapinfo;

  PWadScanInfo () noexcept : processed(false), iwad(), mapname(), episode(-1), mapnum(-1), hasMapinfo(false) {}
  inline void clear () noexcept { processed = false; iwad.clear(); mapname.clear(); episode = -1; mapnum = -1; hasMapinfo = false; }

  inline bool isMapIndexValid () const noexcept { return (episode >= 0 && mapnum >= 0); }

  inline int getMapIndex () const noexcept { return (episode > 0 ? episode*10+mapnum : episode == 0 ? mapnum : 0); }

  static inline int exmxToIndex (int e, int m) noexcept { return (e*10+m); }
};


static PWadScanInfo pwadScanInfo;
TArray<PWadMapLump> fsys_PWadMaps; // sorted

extern "C" {
  static int cmpPWadMapLump (const void *aa, const void *bb, void *) {
    if (aa == bb) return 0;
    const PWadMapLump *a = (const PWadMapLump *)aa;
    const PWadMapLump *b = (const PWadMapLump *)bb;
    if (a->episode && b->episode) return a->getMapIndex()-b->getMapIndex();
    if (!a->episode && !b->episode) return a->getMapIndex()-b->getMapIndex();
    if (a->episode) return -1; // b is D2 map
    if (b->episode) return 1; // a is D2 map
    // just in case
    return a->getMapIndex()-b->getMapIndex();
  }
}


//==========================================================================
//
//  appendPWadMapLump
//
//==========================================================================
static void appendPWadMapLump (const PWadMapLump &wlmp) {
  if (!wlmp.isValid()) return;
  for (auto &&l : fsys_PWadMaps) {
    if (l.isEqual(wlmp)) {
      l = wlmp;
      return;
    }
  }
  fsys_PWadMaps.append(wlmp);
}


//==========================================================================
//
//  PWadMapLump::parseMapName
//
//==========================================================================
bool PWadMapLump::parseMapName (const char *name) noexcept {
  clear();
  if (!name || !name[0]) return false;

  // doom1, kdizd
  if ((name[0] == 'e' || name[0] == 'z') && name[1] && name[2] == 'm' && name[3] && !name[4]) {
    int e = VStr::digitInBase(name[1], 10);
    int m = VStr::digitInBase(name[3], 10);
    if (e < 0 || m < 0) return false;
    if (e >= 1 && e <= 9 && m >= 1 && m <= 9) {
      mapname = VStr(name);
      episode = e;
      mapnum = m;
      return true;
    }
  }

  // doom2
  int n = -1;
  // try to detect things like "aaa<digit>"
  int dpos = 0;
  while (name[dpos] && VStr::digitInBase(name[dpos], 10) < 0) ++dpos;
  if (dpos == 0) return false; // must start from a lettter
  if (VStr::digitInBase(name[dpos], 10) < 0) return false; // nope
  for (const char *t = name+dpos; *t; ++t) if (VStr::digitInBase(*t, 10) < 0) return false;
  if (!VStr::convertInt(name+dpos, &n)) return false;
  if (n < 1 || n > 99) return false;
  // i found her!
  mapname = VStr(name);
  episode = 0;
  mapnum = n;
  return true;
}


//==========================================================================
//
//  processMapName
//
//==========================================================================
static void processMapName (const char *name, int lump) {
  if (!name || !name[0]) return;
  //GCon->Logf(NAME_Debug, "*** checking map: name=<%s>", name);

  //TODO: use `PWadMapLump::parseMapName()` here, and remove pasta
  PWadMapLump wlmp;
  if (wlmp.parseMapName(name)) {
    wlmp.lump = lump;
    appendPWadMapLump(wlmp);
  }

  // if we have maps for both D1 and D2 (Maps Of Chaos, for example), use D2 maps

  // doom1 (or kdizd)
  if (pwadScanInfo.episode != 0) {
    if ((name[0] == 'e' || name[0] == 'z') && name[1] && name[2] == 'm' && name[3] && !name[4]) {
      int e = VStr::digitInBase(name[1], 10);
      int m = VStr::digitInBase(name[3], 10);
      if (e < 0 || m < 0) return;
      if (e >= 1 && e <= 9 && m >= 1 && m <= 9) {
        //if (pwadScanInfo.episode == 0) return; // oops, mixed maps
        const int newidx = PWadScanInfo::exmxToIndex(e, m);
        if (pwadScanInfo.episode < 0 || pwadScanInfo.getMapIndex() > newidx) {
          pwadScanInfo.episode = e;
          pwadScanInfo.mapnum = m;
          pwadScanInfo.mapname = VStr(name);
          //GCon->Logf(NAME_Debug, "*** D1 MAP; episode=%d; map=%d; index=%d", e, m, pwadScanInfo.getMapIndex());
        }
      }
      return; // continue
    }
  }

  // doom2
  /*if (pwadScanInfo.episode <= 0)*/ {
    int n = -1;
    // try to detect things like "aaa<digit>"
    int dpos = 0;
    while (name[dpos] && VStr::digitInBase(name[dpos], 10) < 0) ++dpos;
    if (dpos == 0) return; // must start from a lettter
    if (VStr::digitInBase(name[dpos], 10) < 0) return; // nope
    for (const char *t = name+dpos; *t; ++t) if (VStr::digitInBase(*t, 10) < 0) return;
    if (!VStr::convertInt(name+dpos, &n)) return;
    if (n < 1) return;
    //GCon->Logf(NAME_Debug, "  n=%d", n);
    // if D1 map were found, perform some hackery
    // only "MAPxx" is allowed to override it
    if (pwadScanInfo.episode > 0) {
      VStr pfx(name);
      pfx = pfx.left(dpos);
      //GCon->Logf(NAME_Debug, "  pfx=<%s>; len=%d", *pfx, VStr::Length(name));
      if (!pfx.startsWithCI("MAP") || VStr::Length(name) != 5) return;
      pwadScanInfo.clear();
    }
    pwadScanInfo.episode = 0;
    if (pwadScanInfo.mapnum < 0 || pwadScanInfo.mapnum > n) {
      pwadScanInfo.mapnum = n;
      pwadScanInfo.mapname = VStr(name);
      //GCon->Logf(NAME_Debug, "*** D2 MAP; index=%d", pwadScanInfo.getMapIndex());
    }
    return; // continue
  }
}


//==========================================================================
//
//  findMapChecker
//
//==========================================================================
static void findMapChecker (int lump) {
  VName lumpname = W_LumpName(lump);
  if (lumpname == NAME_None) return;
  processMapName(*lumpname, lump);
}


//==========================================================================
//
//  performPWadScan
//
//  it is safe to call this several times
//  results will be put into `pwadScanInfo`
//
//==========================================================================
static void performPWadScan () {
  if (pwadScanInfo.processed) return;

  pwadScanInfo.clear();
  pwadScanInfo.processed = true;

  auto milump = (cli_NoZMapinfo >= 0 ? W_CheckNumForName("zmapinfo") : -1);
  if (milump < 0 /*!!! || !W_IsAuxLump(milump)*/) milump = W_CheckNumForName("mapinfo");
  pwadScanInfo.hasMapinfo = (milump >= 0 /*!!! && W_IsAuxLump(milump)*/);

  //GCon->Log("**********************************");
  // try "GAMEINFO" first
  auto gilump = W_CheckNumForName("gameinfo");
  if (gilump >= 0/*!!! W_IsAuxLump(gilump)*/) {
    VScriptParser *gsc = new VScriptParser(W_FullLumpName(gilump), W_CreateLumpReaderNum(gilump));
    gsc->SetCMode(true);
    while (gsc->GetString()) {
      if (gsc->QuotedString) continue; // just in case
      if (gsc->String.strEquCI("iwad")) {
        gsc->Check("=");
        if (gsc->GetString()) pwadScanInfo.iwad = gsc->String;
        continue;
      }
    }
    pwadScanInfo.iwad = pwadScanInfo.iwad.ExtractFileBaseName();
    if (!pwadScanInfo.iwad.isEmpty()) {
      if (!pwadScanInfo.iwad.extractFileExtension().strEquCI(".wad")) pwadScanInfo.iwad += ".wad";
    }
  }

  // guess the name of the first map
  GCon->Log(NAME_Init, "detecting pwad maps...");
  for (auto &&it : WadMapIterator::FromWadFile(0)/*!!! FromFirstAuxFile()*/) findMapChecker(it.lump);
  // if no ordinary maps, try to find "maps/xxx.wad" files
  if (!pwadScanInfo.isMapIndexValid()) {
    //for (auto &&it : WadFileIterator::FromFirstAuxFile()) {
    for (auto &&it : WadNSIterator::FromWadFile(0/*!!! W_GetFirstAuxFile()*/, WADNS_AllFiles)) {
      VStr fname = it.getRealName();
      // check for "maps/xxx.wad"
      //GCon->Logf(NAME_Debug, ":: <%s>", *fname);
      if (!fname.startsWithCI("maps/") || !fname.endsWithCI(".wad")) continue;
      bool fucked = false;
      for (const char *s = *fname+5; *s; ++s) if (*s == '/') { fucked = true; break; }
      if (fucked) continue;
      fname.chopLeft(5);
      fname.chopRight(4);
      processMapName(*fname, it.lump);
    }
  }
  // sort collected maps
  timsort_r(fsys_PWadMaps.ptr(), fsys_PWadMaps.length(), sizeof(PWadMapLump), &cmpPWadMapLump, nullptr);
  GCon->Log(NAME_Init, "pwad map detection complete.");

  fsys_hasMapPwads = (fsys_PWadMaps.length() > 0);
}


// ////////////////////////////////////////////////////////////////////////// //
static TArray<VStr> wpklist; // everything is lowercased


//==========================================================================
//
//  FL_GetWadPk3List
//
//==========================================================================
const TArray<VStr> &FL_GetWadPk3List () {
  return wpklist;
}


//==========================================================================
//
//  wpkAppend
//
//==========================================================================
static void wpkAppend (VStr fname, bool asystem) {
  if (fname.length() == 0) return;
  VStr fn = fname.toLowerCase();
  if (!asystem) {
    fn = fn.extractFileName();
    if (fn.length() == 0) fn = fname.toLowerCase();
  }
  //if (fname.endsWithCI(".zip")) return; // ignore zip containers
  //GCon->Logf(NAME_Debug, "WPK: %s", *fn);
  // check for duplicates
  for (int f = wpklist.length()-1; f >= 0; --f) {
    if (wpklist[f] != fn) continue;
    // i found her!
    return;
  }
  /*
  WadPakInfo &wi = wpklist.Alloc();
  wi.path = fn;
  wi.isSystem = asystem;
  */
  wpklist.append(fn);
}


//==========================================================================
//
//  wpkMark
//
//  call this before adding archives
//
//==========================================================================
static int wpkMark (bool cosmetic) {
  return (cosmetic ? -1 : fsysSearchPaths.length());
}


//==========================================================================
//
//  wpkAddMarked
//
//==========================================================================
static void wpkAddMarked (int idx) {
  if (idx < 0) return;
  for (; idx < fsysSearchPaths.length(); ++idx) {
    VSearchPath *sp = fsysSearchPaths[idx];
    /*
    if (sp->cosmetic) continue; // just in case
    if (sp->IsNonPak()) continue;
    VStr fn = sp->GetPrefix().extractFileName().toLowerCase();
    */
    VStr fn = getNetPath(sp);
    if (fn.isEmpty()) continue;
    //GCon->Logf(NAME_Debug, "*** <%s> (%s) ***", *fn, *sp->GetPrefix());
    wpklist.append(fn);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
VVA_OKUNUSED static int cmpfuncCI (const void *v1, const void *v2) {
  return ((VStr *)v1)->ICmp((*(VStr *)v2));
}

VVA_OKUNUSED static int cmpfuncCINoExt (const void *v1, const void *v2) {
  return ((VStr *)v1)->StripExtension().ICmp(((VStr *)v2)->StripExtension());
}


//==========================================================================
//
//  AddAnyFile
//
//==========================================================================
static bool AddAnyFile (VStr fname, bool allowFail, bool fixVoices=false) {
  if (fname.length() == 0) {
    if (!allowFail) Sys_Error("cannot add empty file");
    return false;
  }
  if (!Sys_FileExists(fname)) {
    if (!allowFail) Sys_Error("cannot add file \"%s\"", *fname);
    GCon->Logf(NAME_Warning, "cannot add file \"%s\"", *fname);
    return false;
  }
  if (!W_AddDiskFileOptional(fname, (allowFail ? false : fixVoices))) {
    if (!allowFail) Sys_Error("cannot add file \"%s\"", *fname);
    GCon->Logf(NAME_Warning, "cannot add file \"%s\"", *fname);
    return false;
  }
  return true;
}


//==========================================================================
//
//  AddAnyUserFile
//
//==========================================================================
static void AddAnyUserFile (VStr fname, bool cosmetic) {
  auto mark = wpkMark(cosmetic);
  if (!AddAnyFile(fname, true/*allowFail*/)) return;
  //if (!cosmetic) wpkAppend(fname, false);
  wpkAddMarked(mark);
}


// ////////////////////////////////////////////////////////////////////////// //
enum { CM_PRE_PWADS, CM_POST_PWADS };

//==========================================================================
//
//  CustomModeLoadPwads
//
//==========================================================================
static void CustomModeLoadPwads (int type) {
  TArray<GroupPwadInfo> &list = (type == CM_PRE_PWADS ? customMode.pwads : customMode.postpwads);
  //GCon->Logf(NAME_Init, "CustomModeLoadPwads: type=%d; len=%d", type, list.length());

  // load post-file pwads from autoload here too
  if (type == CM_POST_PWADS && postPWads.length() > 0) {
    GCon->Logf(NAME_Init, "loading autoload post-pwads");
    for (auto &&wn : postPWads) {
      GCon->Logf(NAME_Init, "autoload %spost-pwad: %s...", (wn.cosmetic ? "cosmetic " : ""), *wn.filename);
      AddAnyUserFile(wn.filename, wn.cosmetic); // allow fail
    }
  }

  for (auto &&ww : list) {
    if (ww.filename.isEmpty()) continue;
    VStr fname = ww.filename;
    if (!fname.startsWith("/")) fname = customMode.basedir+fname;
    GCon->Logf(NAME_Init, "%smode pwad: %s...", (ww.cosmetic ? "cosmetic " : ""), *fname);
    AddAnyUserFile(fname, ww.cosmetic); // allow fail
  }
}


static TMap<VStr, bool> cliGroupMap; // true: enabled; false: disabled
static TArray<GroupMask> cliGroupMask;


//==========================================================================
//
//  AddAutoloadRC
//
//==========================================================================
void AddAutoloadRC (VStr aubasedir) {
  VStream *aurc = FL_OpenSysFileRead(aubasedir+"autoload.rc");
  if (!aurc) return;

  // collect autoload groups to skip
  // add skips from custom mode
  for (int f = 0; f < customMode.autoskips.length(); ++f) {
    GroupMask &gi = customMode.autoskips[f];
    if (gi.isGlob()) {
      cliGroupMask.append(gi);
    } else {
      cliGroupMap.put(gi.mask.toLowerCase(), false);
    }
  }

  GCon->Logf(NAME_Init, "parsing autoload groups from '%s'...", *aurc->GetName());
  VScriptParser *sc = new VScriptParser(aurc->GetName(), aurc);

  while (!sc->AtEnd()) {
    sc->Expect("group");
    sc->ExpectString();
    VStr grpname = sc->String;
    bool enabled = !sc->Check("disabled");
    sc->Expect("{");
    // exact matches has precedence
    auto gmp = cliGroupMap.find(grpname.toLowerCase());
    if (gmp) {
      enabled = *gmp;
    } else {
      // process masks; backwards, so latest mask has precedence
      for (int f = cliGroupMask.length()-1; f >= 0; --f) {
        if (grpname.globmatch(cliGroupMask[f].mask, false)) {
          enabled = cliGroupMask[f].enabled;
          break;
        }
      }
    }
    if (!enabled) {
      GCon->Logf(NAME_Init, "skipping autoload group '%s'", *grpname);
      sc->SkipBracketed(true); // bracket eaten
      continue;
    }
    GCon->Logf(NAME_Init, "processing autoload group '%s'", *grpname);
    // get file list
    while (!sc->Check("}")) {
      if (sc->Check(",")) continue;
      bool postPWad = false;
      bool cosmetic = true;
      if (sc->Check("postpwad")) postPWad = true;
      if (!sc->GetString()) sc->Error("wad/pk3 path expected");
      if (sc->String.isEmpty()) continue;
      if (sc->String[0] == '!') {
        sc->String.chopLeft(1);
        if (sc->String.isEmpty()) continue;
        cosmetic = false;
      }
      VStr fname = ((*sc->String)[0] == '/' ? sc->String : aubasedir+sc->String);
      if (postPWad) {
        GroupPwadInfo &wi = postPWads.alloc();
        wi.filename = fname;
        wi.cosmetic = cosmetic;
      } else {
        AddAnyUserFile(fname, cosmetic);
      }
    }
  }

  delete sc;
}


//==========================================================================
//
//  AddGameAutoloads
//
//==========================================================================
static void AddGameAutoloads (VStr basedir, bool addAutoload=true) {
  if (fsys_onlyOneBaseFile) return;

  basedir = basedir.fixSlashes();
  if (basedir.length() == 0 || basedir == "/") return;
  if (addAutoload) basedir = basedir.appendPath("autoload");
  //GCon->Logf(NAME_Debug, "::: <%s> :::", *basedir);

  TArray<VStr> WadFiles;
  TArray<VStr> ZipFiles;

  // find all .wad/.pk3 files in that directory
  auto dirit = Sys_OpenDir(basedir);
  if (dirit) {
    for (VStr test = Sys_ReadDir(dirit); test.IsNotEmpty(); test = Sys_ReadDir(dirit)) {
      //fprintf(stderr, "  <%s>\n", *test);
      if (test[0] == '_' || test[0] == '.') continue; // skip it
      VStr ext = test.ExtractFileExtension();
           if (ext.strEquCI(".wad")) WadFiles.Append(test);
      else if (ext.strEquCI(".pk3")) ZipFiles.Append(test);
    }
    Sys_CloseDir(dirit);
    qsort(WadFiles.Ptr(), WadFiles.length(), sizeof(VStr), cmpfuncCINoExt);
    qsort(ZipFiles.Ptr(), ZipFiles.length(), sizeof(VStr), cmpfuncCINoExt);
  }

  basedir = basedir.appendTrailingSlash();

  if (WadFiles.length() || ZipFiles.length()) {
    GCon->Logf(NAME_Init, "adding game autoloads from '%s'...", *basedir);
    // now add wads, then pk3s
    for (auto &&fn : WadFiles) W_AddDiskFile(basedir.appendPath(fn), false);
    for (auto &&fn : ZipFiles) W_AddDiskFile(basedir.appendPath(fn));
  }

  AddAutoloadRC(basedir);
}


//==========================================================================
//
//  AddGameDir
//
//==========================================================================
static void AddGameDir (VStr basedir, VStr dir) {
  GCon->Logf(NAME_Init, "adding game dir '%s'...", *dir);

  VStr bdx = basedir;
  if (bdx.length() == 0) bdx = "./";
  bdx = bdx.appendPath(dir);

  if (!Sys_DirExists(bdx)) return;

  fsys_hide_sprofs = modNoBaseSprOfs;

  TArray<VStr> WadFiles;
  TArray<VStr> ZipFiles;

  // find all .wad/.pk3 files in that directory
  auto dirit = Sys_OpenDir(bdx);
  if (dirit) {
    for (VStr test = Sys_ReadDir(dirit); test.IsNotEmpty(); test = Sys_ReadDir(dirit)) {
      //fprintf(stderr, "  <%s>\n", *test);
      if (test[0] == '_' || test[0] == '.') continue; // skip it
      if (test.extractFileName().strEquCI("basepak.pk3")) continue; // it will be explicitly added later
      VStr ext = test.ExtractFileExtension();
           if (ext.strEquCI(".wad")) WadFiles.Append(test);
      else if (ext.strEquCI(".pk3")) ZipFiles.Append(test);
    }
    Sys_CloseDir(dirit);
    qsort(WadFiles.Ptr(), WadFiles.length(), sizeof(VStr), cmpfuncCINoExt);
    qsort(ZipFiles.Ptr(), ZipFiles.length(), sizeof(VStr), cmpfuncCINoExt);
  }

  // use `VStdFileStreamRead` so android port can override it
  auto bps = FL_OpenSysFileRead(bdx.appendPath("basepak.pk3"));
  if (bps) {
    delete bps;
    ZipFiles.insert(0, "basepak.pk3");
    GCon->Logf(NAME_Init, "found basepak at '%s'", *bdx.appendPath("basepak.pk3"));
  }

  // add system dir, if it has any files
  if (ZipFiles.length() || WadFiles.length()) wpkAppend(dir+"/", true); // don't strip path

  // now add wads, then pk3s
  for (int i = 0; i < WadFiles.length(); ++i) {
    //if (i == 0 && ZipFiles.length() == 0) wpkAppend(dir+"/"+WadFiles[i], true); // system pak
    W_AddDiskFile(bdx.appendPath(WadFiles[i]), false);
  }

  for (int i = 0; i < ZipFiles.length(); ++i) {
    //if (i == 0) wpkAppend(dir+"/"+ZipFiles[i], true); // system pak
    bool isBPK = ZipFiles[i].extractFileName().strEquCI("basepak.pk3");
    int spl = fsysSearchPaths.length();
    W_AddDiskFile(bdx.appendPath(ZipFiles[i]));
    if (isBPK) {
      // mark "basepak" flags
      for (int cc = spl; cc < fsysSearchPaths.length(); ++cc) {
        fsysSearchPaths[cc]->basepak = true;
      }
    }
  }

  fsys_hide_sprofs = false;

  // custom mode
  SetupCustomMode(bdx);
  ApplyUserModes(dir);

  AddGameAutoloads(bdx);
  VStr gdn = dir.extractFileName();

  if (!gdn.isEmpty()) {
    #ifndef _WIN32
    const char *hdir = getenv("HOME");
    if (hdir && hdir[0] && hdir[1]) {
      VStr hbd = VStr(hdir);
      if (!gdn.isEmpty()) {
        hbd = hbd.appendPath(".k8vavoom");
        hbd = hbd.appendPath("autoload");
        hbd = hbd.appendPath(gdn);
        AddGameAutoloads(hbd, false);
      }
    }
    #endif
    for (auto &&audir : autoloadDirList) {
      VStr hbd = VStr(audir);
      if (hbd.isEmpty()) continue;
      if (hbd[0] == '!') {
        hbd.chopLeft(1);
        hbd = GParsedArgs.getBinDir()+hbd;
      } else if (hbd[0] == '~') {
        hbd.chopLeft(1);
        const char *homdir = getenv("HOME");
        if (homdir && homdir[0]) {
          hbd = VStr(homdir)+hbd;
        } else {
          continue;
        }
      }
      hbd = hbd.appendPath(gdn);
      //GCon->Logf(NAME_Debug, "!!! <%s> : <%s> : <%s>", *audir, *gdn, *hbd);
      AddGameAutoloads(hbd, false);
    }
  }

  // finally add directory itself
  // k8: nope
  /*
  VFilesDir *info = new VFilesDir(bdx);
  fsysSearchPaths.Append(info);
  */
}


//==========================================================================
//
//  AddGameDir
//
//==========================================================================
static void AddGameDir (VStr dir) {
  AddGameDir(fl_basedir, dir);
  //k8:wtf?!
  //if (fl_savedir.IsNotEmpty()) AddGameDir(fl_savedir, dir);
  fl_gamedir = dir;
}


//==========================================================================
//
//  FindMainWad
//
//==========================================================================
static VStr FindMainWad (VStr MainWad) {
  if (MainWad.length() == 0) return VStr();

  //GLog.Logf(NAME_Debug, "trying to find iwad '%s'...", *MainWad);

  // if we have path separators, try relative path first
  bool hasSep = false;
  for (const char *s = *MainWad; *s; ++s) {
#ifdef _WIN32
    if (*s == '/' || *s == '\\') { hasSep = true; break; }
#else
    if (*s == '/') { hasSep = true; break; }
#endif
  }
#ifdef _WIN32
  if (!hasSep && MainWad.length() >= 2 && MainWad[1] == ':') hasSep = true;
#endif

  if (hasSep) {
    VStr nwadfname = Sys_FindFileCI(MainWad);
    if (nwadfname.length() != 0) return nwadfname;
  }

  // first check in IWAD directories
  for (auto &&dir : IWadDirs) {
    //GLog.Logf(NAME_Debug, "  looking for iwad '%s/%s'...", *dir, *MainWad);
    VStr wadfname = Sys_FindFileCI(dir+"/"+MainWad);
    if (wadfname.length() != 0) {
      //GLog.Logf(NAME_Debug, "    FOUND iwad '%s/%s'!", *dir, *MainWad);
      return wadfname;
    }
  }

  // then look in the save directory
  //if (fl_savedir.IsNotEmpty() && Sys_FileExists(fl_savedir+"/"+MainWad)) return fl_savedir+"/"+MainWad;

  // finally in base directory
  VStr bdwadfname = Sys_FindFileCI(fl_basedir+"/"+MainWad);
  if (bdwadfname.length() != 0) return bdwadfname;

  // just in case, check it as-is
  bdwadfname = Sys_FindFileCI(MainWad);
  if (bdwadfname.length() != 0) return bdwadfname;

  return VStr();
}


//==========================================================================
//
//  SetupGameDir
//
//==========================================================================
static void SetupGameDir (VStr dirname) {
  AddGameDir(dirname);
}


//==========================================================================
//
//  ParseStringValueOrList
//
//==========================================================================
static void ParseStringValueOrList (VScriptParser *sc, TArray<VStr> &list, VStr *dsc=nullptr) {
  sc->Expect("=");
  if (sc->Check("{")) {
    // parse list
    while (!sc->Check("}")) {
      sc->ExpectString();
      list.append(sc->String);
      if (!sc->Check(",")) {
        sc->Expect("}");
        break;
      }
    }
    if (dsc && sc->Check(":")) {
      sc->ExpectString();
      *dsc = sc->String;
    }
    sc->Check(";"); // optional
  } else {
    // single value
    sc->ExpectString();
    list.append(sc->String);
    if (dsc && sc->Check(":")) {
      sc->ExpectString();
      *dsc = sc->String;
    }
    sc->Expect(";");
  }
}


//==========================================================================
//
//  ParseStringValue
//
//==========================================================================
static VStr ParseStringValue (VScriptParser *sc) {
  sc->Expect("=");
  sc->ExpectString();
  VStr res = sc->String;
  sc->Expect(";");
  return res;
}


//==========================================================================
//
//  ParseBoolValue
//
//==========================================================================
static bool ParseBoolValue (VScriptParser *sc) {
  sc->Expect("=");
  bool res = false;
       if (sc->Check("true") || sc->Check("tan")) res = true;
  else if (sc->Check("false") || sc->Check("ona")) res = false;
  else sc->Error("boolean expected");
  sc->Expect(";");
  return res;
}


//==========================================================================
//
//  ParseGameDef
//
//  "{" already eaten
//
//==========================================================================
static void ParseGameDef (VScriptParser *sc, version_t &game) {
  while (!sc->Check("}")) {
    // description
    if (sc->Check("description")) { game.description = ParseStringValue(sc); continue; }
    // game directory
    if (sc->Check("game")) { game.GameDir = ParseStringValue(sc); continue; }
    // base dirs
    if (sc->Check("base")) {
      game.BaseDirs.clear();
      ParseStringValueOrList(sc, game.BaseDirs);
      continue;
    }
    // iwad
    if (sc->Check("iwad")) {
      TArray<VStr> iwads; // 2nd and next iwads will be added to addfiles
      VStr dsc;
      ParseStringValueOrList(sc, iwads, &dsc);
      if (iwads.length() == 0) continue;
      if (iwads[0].isEmpty()) sc->Error(va("game '%s' has empty main wad", *game.gamename));
      MainWadFiles &wf = game.mainWads.alloc();
      wf.main = iwads[0];
      wf.description = dsc;
      for (int f = 1; f < iwads.length(); ++f) {
        VStr s = iwads[f];
        if (s.isEmpty()) continue;
        if (s.length() == 1 && s[0] == '?') continue;
        VStr nfn = s;
        if (nfn[0] == '?') nfn.chopLeft(1);
        bool found = false;
        for (auto &&ks : wf.aux) {
          if (ks[0] == '?') {
            if (ks.mid(1, ks.length()).strEquCI(nfn)) { found = true; break; }
          } else {
            if (ks.strEquCI(nfn)) { found = true; break; }
          }
        }
        if (found) sc->Error(va("game '%s' has duplicate additional wad \"%s\"", *game.gamename, *s));
        wf.aux.append(s);
      }
      continue;
    }
    // CLI params
    if (sc->Check("param")) {
      game.params.clear();
      ParseStringValueOrList(sc, game.params);
      continue;
    }
    // fix voices?
    if (sc->Check("fixvoices")) {
      game.FixVoices = ParseBoolValue(sc);
      continue;
    }
    // warp template
    if (sc->Check("warp")) {
      game.warp = ParseStringValue(sc);
      continue;
    }
    // filters
    if (sc->Check("filter")) {
      TArray<VStr> filters;
      ParseStringValueOrList(sc, filters);
      game.filters.clear();
      for (auto &&fs : filters) game.filters.append(VStr("filter/")+fs.toLowerCase());
      continue;
    }
    // special flag
    if (sc->Check("ashexen")) {
      game.options.hexenGame = ParseBoolValue(sc);
      continue;
    }
    // list of defines
    if (sc->Check("define")) {
      game.defines.clear();
      ParseStringValueOrList(sc, game.defines);
      continue;
    }
    // unknown shit
    if (!sc->GetString()) sc->Error("unexpected end of file");
    sc->Error(va("unknown command: '%s'", *sc->String));
  }

  if (game.params.length() == 0) game.params.append(game.gamename);
  if (game.mainWads.length() == 0) sc->Error(va("game '%s' has no iwads", *game.gamename));
  if (game.GameDir.isEmpty()) sc->Error(va("game '%s' has no game dir", *game.gamename));

  // fix iwad descritions
  for (auto &&wfi : game.mainWads) {
    if (wfi.description.isEmpty()) wfi.description = game.description;
  }
}


//==========================================================================
//
//  ParseGamesDefinition
//
//==========================================================================
static void ParseGamesDefinition (VScriptParser *sc, TArray<version_t> &games) {
  sc->SetCMode(true);
  while (!sc->AtEnd()) {
    if (sc->Check(";")) continue;
    if (sc->Check("game")) {
      sc->ExpectString();
      VStr gname = sc->String;
      if (gname.isEmpty()) sc->Error("game name is empty");
      sc->Expect("{");
      version_t &game = games.Alloc();
      game.FixVoices = false;
      game.gamename = gname;
      ParseGameDef(sc, game);
      continue;
    }
  }
  delete sc;
}


//==========================================================================
//
//  ProcessBaseGameDefs
//
//==========================================================================
static void ProcessBaseGameDefs (VStr name, VStr mainiwad) {
  TArray<version_t> games;
  version_t *selectedGame = nullptr;
  VStr UseName;

       if (fl_savedir.IsNotEmpty() && Sys_FileExists(fl_savedir+"/"+name)) UseName = fl_savedir+"/"+name;
  else if (Sys_FileExists(fl_basedir+"/"+name)) UseName = fl_basedir+"/"+name;
  else return;

  if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, "Parsing game definition file \"%s\"...", *UseName);
  VScriptParser *sc = new VScriptParser(UseName, FL_OpenSysFileRead(UseName));
  ParseGamesDefinition(sc, games);
  if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, "Done parsing game definition file \"%s\"...", *UseName);
  if (games.length() == 0) Sys_Error("No game definitions found!");

  for (auto &&game : games) {
    for (auto &&arg : game.params) {
      if (arg.isEmpty()) continue;
      if (cliGameMode.strEquCI(arg)) {
        selectedGame = &game;
        break;
      }
    }
    if (selectedGame) break;
  }

  if (selectedGame) {
    VStr gn = selectedGame->gamename;
    if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, "SELECTED GAME: \"%s\"", *gn);
  } else {
    if (games.length() > 1) {
      // try to detect game by custom iwad
      if (!selectedGame && mainiwad.length() > 0) {
        for (auto &&game : games) {
          VStr mw = mainiwad.extractFileBaseName();
          for (auto &&mwi : game.mainWads) {
            VStr gw = mwi.main.extractFileBaseName();
            if (gw.strEquCI(mw)) {
              GCon->Logf(NAME_Init, "Detected game is '%s' (from iwad)", *mwi.description);
              selectedGame = &game;
              break;
            }
          }
          if (selectedGame) break;
        }
      }

      // try to select DooM or DooM II automatically
      if (!selectedGame) {
        //!performPWadScan();
        if (!pwadScanInfo.iwad.isEmpty()) {
          for (auto &&game : games) {
            for (auto &&mwi : game.mainWads) {
              VStr gw = mwi.main.extractFileBaseName();
              if (gw.strEquCI(pwadScanInfo.iwad)) {
                GCon->Logf(NAME_Init, "Detected game is '%s' (from gameinfo)", *mwi.description);
                selectedGame = &game;
                break;
              }
            }
            if (selectedGame) break;
          }
        }

        // try to guess from map name
        if (!selectedGame) {
          //!performPWadScan();
          //GCon->Logf(NAME_Debug, "*** hasMapinfo: %d; mapname=<%s>; episode=%d; map=%d; index=%d", (int)pwadScanInfo.hasMapinfo, *pwadScanInfo.mapname, pwadScanInfo.episode, pwadScanInfo.mapnum, pwadScanInfo.getMapIndex());
          if (pwadScanInfo.getMapIndex() > 0) {
            //GCon->Logf("MNAME: <%s>", *mname);
            // found map, find DooM or DooM II game definition
            VStr gn1 = (pwadScanInfo.episode == 0 ? "doom2" : "doom");
            VStr gn2 = (pwadScanInfo.episode == 0 ? "freedoom2" : "freedoom");
            for (auto &&game : games) {
              if (!gn1.strEquCI(game.gamename) && !gn2.strEquCI(game.gamename)) continue;
              // check if we have the corresponding iwad
              VStr mwp;
              for (auto &&mwi : game.mainWads) {
                mwp = FindMainWad(mwi.main);
                if (!mwp.isEmpty()) {
                  GCon->Logf(NAME_Init, "Detected game is '%s' (from map lump '%s')", *mwi.description, *pwadScanInfo.mapname);
                  selectedGame = &game;
                  break;
                }
              }
              if (selectedGame) break;
            }
          }
        }
      }

      // try to find game iwad
      if (!selectedGame) {
        for (auto &&game : games) {
          VStr mainWadPath;
          for (auto &&mwi : game.mainWads) {
            mainWadPath = FindMainWad(mwi.main);
            if (!mainWadPath.isEmpty()) {
              GCon->Logf(NAME_Init, "Detected game is '%s' (iwad search)", *mwi.description);
              selectedGame = &game;
              break;
            }
          }
          if (selectedGame) break;
        }
      }

      /*
      if (selectedGame >= 0) {
        game_name = *games[selectedGame].gamename;
        GCon->Logf(NAME_Init, "detected game: \"%s\"", *games[selectedGame].gamename);
      }
      */
    } else {
      selectedGame = &games[0];
    }
    if (!selectedGame) Sys_Error("Looks like I cannot find any IWADs. Did you forgot to specify -iwaddir?");
  }
  // set game name variable
  game_name = *selectedGame->gamename;

  vassert(selectedGame);
  version_t &game = *selectedGame;
  game_options = game.options;

  for (auto &&ds : game.defines) {
    if (!ds.isEmpty()) {
      VMemberBase::StaticAddDefine(*ds);
      GCon->Logf(NAME_Init, "added define '%s' for game '%s'", *ds, *game.gamename);
    }
  }

  // look for the main wad file
  VStr mainWadPath;

  int iwadidx = -1;
  VStr gameDsc;
  if (!modDetectorDisabledIWads) {
    // try user-specified iwad
    if (mainiwad.length() > 0) {
      GCon->Logf(NAME_Init, "trying custom IWAD '%s'...", *mainiwad);
      mainWadPath = FindMainWad(mainiwad);
      if (mainWadPath.isEmpty()) Sys_Error("custom IWAD '%s' not found!", *mainiwad);
      GCon->Logf(NAME_Init, "found custom IWAD '%s'...", *mainWadPath);
      gameDsc = game.description;
    } else {
      // try default iwads
      for (iwadidx = 0; iwadidx < game.mainWads.length(); ++iwadidx) {
        mainWadPath = FindMainWad(game.mainWads[iwadidx].main);
        if (!mainWadPath.isEmpty()) {
          gameDsc = game.mainWads[iwadidx].description;
          break;
        }
      }
      if (mainWadPath.isEmpty()) Sys_Error("Main wad file \"%s\" not found.", *game.mainWads[0].main);
      vassert(iwadidx >= 0 && iwadidx < game.mainWads.length());
    }
  } else {
    gameDsc = "custom TC";
  }

  // process filters and warp template
  FL_ClearGameFilters();
  for (auto &&flt : game.filters) {
    int res = FL_AddGameFilter(flt);
    switch (res) {
      case FL_ADDFILTER_OK: break;
      case FL_ADDFILTER_INVALID: GCon->Logf(NAME_Error, "Invalid game filter '%s', ignored", *flt); break;
      case FL_ADDFILTER_DUPLICATE: GCon->Logf(NAME_Warning, "Duplicate game filter '%s', ignored", *flt); break;
      default: GCon->Logf(NAME_Error, "Unknown game filter error %d for filter '%s', ignored", res, *flt); break;
    }
  }
  warpTpl = game.warp;

  // append iwad
  //GCon->Logf("MAIN WAD(1): '%s'", *MainWadPath);

  GCon->Logf(NAME_Init, "loading game %s", *gameDsc);
  bool iwadAdded = false;
  if (!modDetectorDisabledIWads) {
    // if iwad is pk3, add it last
    if (mainWadPath.endsWithCI("wad")) {
      GCon->Logf(NAME_Init, "adding iwad \"%s\"...", *mainWadPath);
      iwadAdded = true;
      IWadIndex = fsysSearchPaths.length();
      wpkAppend(mainWadPath, false); // mark iwad as "non-system" file, so path won't be stored in savegame
      AddAnyFile(mainWadPath, false, game.FixVoices);
    } else {
      GCon->Logf(NAME_Init, "using iwad \"%s\"...", *mainWadPath);
    }
  }

  // add optional files
  if (iwadidx >= 0) {
    for (auto &&xfn : game.mainWads[iwadidx].aux) {
      vassert(!xfn.isEmpty());
      bool optional = (xfn[0] == '?');
      VStr fname = FindMainWad(optional ? xfn.mid(1, xfn.length()) : xfn);
      if (fname.isEmpty()) {
        if (optional) continue;
        Sys_Error("Required file \"%s\" not found", *xfn);
      }
      GCon->Logf(NAME_Init, "additing game file \"%s\"...", *fname);
      wpkAppend(fname, false); // mark additional files as "non-system", so path won't be stored in savegame
      AddAnyFile(fname, false);
    }
  }

  //GCon->Logf(NAME_Debug, "BDIRS: %d", game.BaseDirs.length());
  for (auto &&bdir : game.BaseDirs) if (!bdir.isEmpty()) AddGameDir(bdir);

  SetupGameDir(game.GameDir);

  // add iwad here
  if (!modDetectorDisabledIWads) {
    if (!iwadAdded) {
      GCon->Logf(NAME_Init, "adding iwad \"%s\"...", *mainWadPath);
      IWadIndex = fsysSearchPaths.length();
      wpkAppend(mainWadPath, false); // mark iwad as "non-system" file, so path won't be stored in savegame
      AddAnyFile(mainWadPath, false, game.FixVoices);
    }
  }
}


//==========================================================================
//
//  RenameSprites
//
//==========================================================================
static void RenameSprites () {
  VStream *Strm = FL_OpenFileRead("sprite_rename.txt");
  if (!Strm) return;

  VScriptParser *sc = new VScriptParser("sprite_rename.txt", Strm);
  TArray<VSpriteRename> Renames;
  TArray<VSpriteRename> AlwaysRenames;
  TArray<VLumpRename> LumpRenames;
  TArray<VLumpRename> AlwaysLumpRenames;
  while (!sc->AtEnd()) {
    bool Always = sc->Check("always");

    if (sc->Check("lump")) {
      sc->ExpectString();
      VStr Old = sc->String.ToLower();
      sc->ExpectString();
      VStr New = sc->String.ToLower();
      VLumpRename &R = (Always ? AlwaysLumpRenames.Alloc() : LumpRenames.Alloc());
      R.Old = *Old;
      R.New = *New;
      continue;
    }

    sc->ExpectString();
    if (sc->String.Length() != 4) sc->Error("Sprite name must be 4 chars long");
    VStr Old = sc->String.ToLower();

    sc->ExpectString();
    if (sc->String.Length() != 4) sc->Error("Sprite name must be 4 chars long");
    VStr New = sc->String.ToLower();

    VSpriteRename &R = Always ? AlwaysRenames.Alloc() : Renames.Alloc();
    R.Old[0] = Old[0];
    R.Old[1] = Old[1];
    R.Old[2] = Old[2];
    R.Old[3] = Old[3];
    R.New[0] = New[0];
    R.New[1] = New[1];
    R.New[2] = New[2];
    R.New[3] = New[3];
  }
  delete sc;

  bool RenameAll = !!cli_oldSprites;
  for (int i = 0; i < fsysSearchPaths.length(); ++i) {
    if (RenameAll || i == IWadIndex) fsysSearchPaths[i]->RenameSprites(Renames, LumpRenames);
    fsysSearchPaths[i]->RenameSprites(AlwaysRenames, AlwaysLumpRenames);
  }
}


// ////////////////////////////////////////////////////////////////////////// //


//==========================================================================
//
//  countFmtHash
//
//==========================================================================
static int countFmtHash (VStr str) {
  if (str.length() == 0) return 0;
  int count = 0;
  bool inHash = false;
  for (const char *s = *str; *s; ++s) {
    if (*s == '#') {
      if (!inHash) ++count;
      inHash = true;
    } else {
      inHash = false;
    }
  }
  return count;
}


//==========================================================================
//
//  cliFnameCollector
//
//==========================================================================
static int cliFnameCollector (VArgs &args, int idx, bool first) {
  //pwflag_SkipSaveList = 0;
  //bool first = true;
  //++idx; // done by the caller
  while (idx < args.Count()) {
    if (VStr::strEqu(args[idx], "-cosmetic")) {
      pwflag_SkipSaveList = 1;
      ++idx;
      continue;
    }
    if (VParsedArgs::IsArgBreaker(args, idx)) break;
    VStr fname = args[idx++];
    const bool sts = !pwflag_SkipSaveList;
    pwflag_SkipSaveList = 0; // autoreset
    //GCon->Logf(NAME_Debug, "idx=%d; fname=<%s>; sts=%d", idx-1, *fname, (int)sts);
    if (fname.isEmpty()) { first = false; continue; }

    PWadFile pwf;
    pwf.fname = fname;
    pwf.skipSounds = !!pwflag_SkipSounds;
    pwf.skipSprites = !!pwflag_SkipSprites;
    pwf.skipDehacked = !!pwflag_SkipDehacked;
    pwf.storeInSave = sts;
    pwf.asDirectory = false;
    //GCon->Logf(NAME_Debug, "<%s>: sdh=%d", *fname, pwflag_SkipDehacked);

    if (Sys_DirExists(fname)) {
      pwf.asDirectory = true;
      if (!first) {
        GCon->Logf(NAME_Warning, "To mount directory '%s' as emulated PK3 file, you should use \"-file\".", *fname);
      } else {
        pwadList.append(pwf);
      }
    } else if (Sys_FileExists(fname)) {
      pwadList.append(pwf);
    } else {
      GCon->Logf(NAME_Warning, "File \"%s\" doesn't exist.", *fname);
    }
    first = false;
  }
  return idx;
}


//==========================================================================
//
//  FL_InitOptions
//
//==========================================================================
void FL_InitOptions () {
  fsys_IgnoreZScript = 0;

  GArgs.AddFileOption("!1-game"); // '!' means "has args, and breaking" (number is argc)
  GArgs.AddFileOption("!1-logfile"); // don't register log file in saves
  GArgs.AddFileOption("!1-iwad");
  GArgs.AddFileOption("!1-iwaddir");
  GArgs.AddFileOption("!1-basedir");
  GArgs.AddFileOption("!1-savedir");
  GArgs.AddFileOption("!1-autoloaddir");
  GArgs.AddFileOption("!1-deh");
  GArgs.AddFileOption("!1-vc-decorate-ignore-file");

  FSYS_InitOptions(GParsedArgs);

  GParsedArgs.RegisterFlagSet("-c", "compile VavoomC/decorate code and immediately exit", &cli_CompileAndExit);

  GParsedArgs.RegisterFlagSet("-cosmetic", "!do not store next pwad in save list", &pwflag_SkipSaveList);

  GParsedArgs.RegisterFlagSet("-skip-sounds", "skip sounds in the following pwads", &pwflag_SkipSounds);
  GParsedArgs.RegisterFlagReset("-allow-sounds", "allow sounds in the following pwads", &pwflag_SkipSounds);
  // aliases
  GParsedArgs.RegisterAlias("-skipsounds", "-skip-sounds");
  GParsedArgs.RegisterAlias("-allowsounds", "-allow-sounds");

  GParsedArgs.RegisterFlagSet("-skip-sprites", "skip sprites in the following pwads", &pwflag_SkipSprites);
  GParsedArgs.RegisterFlagReset("-allow-sprites", "allow sprites in the following pwads", &pwflag_SkipSprites);
  // aliases
  GParsedArgs.RegisterAlias("-skipsprites", "-skip-sprites");
  GParsedArgs.RegisterAlias("-allowsprites", "-allow-sprites");

  GParsedArgs.RegisterFlagSet("-skip-dehacked", "skip dehacked in the following pwads", &pwflag_SkipDehacked);
  GParsedArgs.RegisterFlagReset("-allow-dehacked", "allow dehacked in the following pwads", &pwflag_SkipDehacked);
  // aliases
  GParsedArgs.RegisterAlias("-skipdehacked", "-skip-dehacked");
  GParsedArgs.RegisterAlias("-allowdehacked", "-allow-dehacked");

  GParsedArgs.RegisterFlagSet("-oldsprites", "!some sprite renaming crap (do not bother)", &cli_oldSprites);
  GParsedArgs.RegisterAlias("-old-sprites", "-oldsprites");

  GParsedArgs.RegisterFlagSet("-no-external-deh", "disable external (out-of-wad) .deh loading", &cli_NoExternalDeh);
  // aliases
  GParsedArgs.RegisterAlias("--no-extern-deh", "-no-external-deh");

  // filename collector
  GParsedArgs.RegisterCallback(nullptr, nullptr, [] (VArgs &args, int idx) -> int { return cliFnameCollector(args, idx, false); });
  GParsedArgs.RegisterCallback("-file", "add the following arguments as pwads", [] (VArgs &args, int idx) -> int { return cliFnameCollector(args, ++idx, true); });


  // modes collector
  GParsedArgs.RegisterCallback("-mode", "activate game mode from 'modes.rc'", [] (VArgs &args, int idx) -> int {
    ++idx;
    if (!VParsedArgs::IsArgBreaker(args, idx)) {
      VStr mn = args[idx++];
      if (!mn.isEmpty()) cliModesList.append(mn);
    }
    return idx;
  });


  // automatic groups collector
  GParsedArgs.RegisterCallback("-autoload", "activate game mode from 'autoload.rc'", [] (VArgs &args, int idx) -> int {
    ++idx;
    if (!VParsedArgs::IsArgBreaker(args, idx)) {
      VStr sg = args[idx++];
      if (!sg.isEmpty()) {
        GroupMask gi;
        gi.mask = sg;
        gi.enabled = true;
        if (gi.isGlob()) {
          cliGroupMask.append(gi);
        } else {
          cliGroupMap.put(sg.toLowerCase(), gi.enabled);
        }
      }
    }
    return idx;
  });
  GParsedArgs.RegisterAlias("-auto", "-autoload");
  GParsedArgs.RegisterAlias("-auto-load", "-autoload");

  GParsedArgs.RegisterStringArrayOption("-autoloaddir", "add autoload directory to the list of autoload dirs", &autoloadDirList);

  GParsedArgs.RegisterCallback("-skip-autoload", "skip game mode from 'autoload.rc'", [] (VArgs &args, int idx) -> int {
    ++idx;
    if (!VParsedArgs::IsArgBreaker(args, idx)) {
      VStr sg = args[idx++];
      if (!sg.isEmpty()) {
        GroupMask gi;
        gi.mask = sg;
        gi.enabled = false;
        if (gi.isGlob()) {
          cliGroupMask.append(gi);
        } else {
          cliGroupMap.put(sg.toLowerCase(), gi.enabled);
        }
      }
    }
    return idx;
  });
  GParsedArgs.RegisterAlias("-skipauto", "-skip-autoload");
  GParsedArgs.RegisterAlias("-skip-auto-load", "-skip-autoload");

  // game type
  GParsedArgs.RegisterStringOption("-game", "select game type", &cliGameCStr);

  // add known game aliases
  //FIXME: unhardcode this!
  GParsedArgs.RegisterCallback("-doom", "select Doom/Ultimate Doom game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "doom"; return 0; });
  GParsedArgs.RegisterAlias("-doom1", "-doom");
  GParsedArgs.RegisterCallback("-doom2", "select Doom II game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "doom2"; return 0; });
  GParsedArgs.RegisterCallback("-tnt", "select TNT Evilution game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "tnt"; return 0; });
  GParsedArgs.RegisterAlias("-evilution", "-tnt");
  GParsedArgs.RegisterCallback("-plutonia", "select Plutonia game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "plutonia"; return 0; });
  GParsedArgs.RegisterCallback("-nerve", "select Doom II + No Rest for the Living game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "nerve"; return 0; });

  GParsedArgs.RegisterCallback("-freedoom", "select Freedoom Phase I game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "freedoom"; return 0; });
  GParsedArgs.RegisterAlias("-freedoom1", "-freedoom");
  GParsedArgs.RegisterCallback("-freedoom2", "select Freedoom Phase II game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "freedoom2"; return 0; });

  GParsedArgs.RegisterCallback("-heretic", "select Heretic game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "heretic"; return 0; });
  GParsedArgs.RegisterCallback("-hexen", "select Hexen game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "hexen"; return 0; });
  GParsedArgs.RegisterCallback("-hexendd", "select Hexen:Deathkings game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "hexendd"; return 0; });

  GParsedArgs.RegisterCallback("-strife", "select Strife game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "strife"; return 0; });
  GParsedArgs.RegisterCallback("-strifeteaser", "select Strife Teaser game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "strifeteaser"; return 0; });

  // hidden
  GParsedArgs.RegisterCallback("-complete", "!DooM Complete game (broken for now)", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "complete"; return 0; });

  GParsedArgs.RegisterCallback("-chex", "Chex Quest game (semi-broken)", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "chex"; return 0; });
  GParsedArgs.RegisterAlias("-chex1", "-chex");
  GParsedArgs.RegisterCallback("-chex2", "Chex Quest 2 game (semi-broken)", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "chex2"; return 0; });
  GParsedArgs.RegisterCallback("-chex3", "!Chex Quest 3 game (semi-broken)", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "chex3"; return 0; });

  GParsedArgs.RegisterFlagSet("-k8runmap", "try to detect and run first pwad map automatically", &doStartMap);

  GParsedArgs.RegisterFlagSet("-fast", "fast monsters", &cli_FastMonsters);
  GParsedArgs.RegisterFlagReset("-slow", "slow monsters", &cli_FastMonsters);

  GParsedArgs.RegisterFlagSet("-respawn", "turn on respawning", &cli_Respawn);
  GParsedArgs.RegisterFlagReset("-no-respawn", "!reverse of \"-respawn\"", &cli_Respawn);

  GParsedArgs.RegisterFlagSet("-nomonsters", "disable monsters", &cli_NoMonsters);
  GParsedArgs.RegisterAlias("-no-monsters", "-nomonsters");
  GParsedArgs.RegisterFlagReset("-monsters", "!reverse of \"monsters\"", &cli_NoMonsters);

  GParsedArgs.RegisterFlagSet("-nomenudef", "disable gzdoom MENUDEF parsing", &cli_NoMenuDef);
  GParsedArgs.RegisterAlias("-no-menudef", "-nomenudef");
  GParsedArgs.RegisterFlagReset("-allow-menudef", "!reverse of \"-nomenudef\"", &cli_NoMenuDef);

  GParsedArgs.RegisterFlagSet("-gore", "enable gore mod", &cli_GoreMod);
  GParsedArgs.RegisterFlagReset("-nogore", "disable gore mod", &cli_GoreMod);
  GParsedArgs.RegisterAlias("-no-gore", "-nogore");

  GParsedArgs.RegisterFlagSet("-force-gore", "force gore mod", &cli_GoreModForce);

  GParsedArgs.RegisterFlagSet("-bdw", "enable BDW (weapons) mod", &cli_BDWMod);
  GParsedArgs.RegisterFlagReset("-nobdw", "disable BDW (weapons) mod", &cli_BDWMod);
  GParsedArgs.RegisterAlias("-no-bdw", "-nobdw");

  // hidden
  GParsedArgs.RegisterFlagSet("-skeehud", "!force SkullDash EE HUD", &cli_SkeeHUD);

  // "-skill"
  GParsedArgs.RegisterCallback("-skill", "select starting skill (3 is HMP; default is UV aka 4)", [] (VArgs &args, int idx) -> int {
    ++idx;
    if (!VParsedArgs::IsArgBreaker(args, idx)) {
      int skn = M_SkillFromName(args[idx]);
      if (skn < 0) skn = 4-1; // default is UV
      Skill = skn;
      ++idx;
    } else {
      GCon->Log(NAME_Warning, "skill name expected!");
    }
    return idx;
  });

  GParsedArgs.RegisterStringOption("-iwad", "override iwad file name", &cli_IWadName);
  GParsedArgs.RegisterStringOption("-iwaddir", "set directory to look for iwads", &cli_IWadDir);
  GParsedArgs.RegisterStringOption("-basedir", "set directory to look for base k8vavoom pk3s", &cli_BaseDir);
  GParsedArgs.RegisterStringOption("-savedir", "set directory to store save files", &cli_SaveDir);
  GParsedArgs.RegisterStringOption("-configdir", "set directory to store config file (and save files)", &cli_ConfigDir);

  // "-warp"
  GParsedArgs.RegisterCallback("-warp", "warp to map number", [] (VArgs &args, int idx) -> int {
    ++idx;
    int wmap1 = -1, wmap2 = -1;
    bool mapok = false, epiok = false;
    if (!VParsedArgs::IsArgBreaker(args, idx)) {
      mapok = VStr::convertInt(args[idx], &wmap1);
      if (mapok) {
        ++idx;
        if (!VParsedArgs::IsArgBreaker(args, idx)) {
          epiok = VStr::convertInt(args[idx], &wmap2);
          if (epiok) ++idx;
        }
      }
      if (!mapok) wmap1 = -1;
      if (!epiok) wmap2 = -1;
    }
    if (wmap1 < 0) wmap1 = -1;
    if (wmap2 < 0) wmap2 = -1;
    fsys_warp_n0 = wmap1;
    fsys_warp_n1 = wmap2;
    return idx;
  });

  GParsedArgs.RegisterFlagSet("-reportiwad", "report loaded iwads (disabled by default)", &reportIWads);
  GParsedArgs.RegisterAlias("-reportiwads", "-reportiwad");

  GParsedArgs.RegisterFlagSet("-silentpwads", "do not report loaded iwads (report by default)", &reportPWads);
  GParsedArgs.RegisterAlias("-silentpwad", "-silentpwads");
  GParsedArgs.RegisterAlias("-silencepwad", "-silentpwads");
  GParsedArgs.RegisterAlias("-silencepwads", "-silentpwads");

  GParsedArgs.RegisterFlagSet("-nakedbase", "skip all autoloads", &cli_NakedBase);
  GParsedArgs.RegisterAlias("-naked-base", "-nakedbase");

  // hidden
  GParsedArgs.RegisterFlagSet("-Wall", "!turn on various useless warnings", &cli_WAll);

  #ifdef _WIN32
  autoloadDirList.append("!/autoload");
  #endif
}


//==========================================================================
//
//  FL_Init
//
//==========================================================================
void FL_Init () {
  const char *p;
  VStr mainIWad = VStr();

  // check for cmake dir
  #ifndef VAVOOM_K8_DEVELOPER
  {
    VStr crapmakedir(GParsedArgs.getBinDir());
    if (Sys_DirExists(crapmakedir.appendPath("CMakeFiles")) ||
        Sys_FileExists(crapmakedir.appendPath("CMakeCache.txt")) ||
        Sys_FileExists(crapmakedir.appendPath("cmake_install.cmake")))
    {
      Sys_Error("Please, do not run k8vavoom from build dir! Without proper `make install` k8vavoom will not work!");
    }
  }
  #endif

  FL_RegisterModDetectors();

  fsys_warp_n0 = -1;
  fsys_warp_n1 = -1;
  fsys_warp_cmd = VStr();

  // if it is set, than it was the latest
  if (cliGameCStr) cliGameMode = cliGameCStr;

       if (cli_FastMonsters == 1) fastparm = 1;
  else if (cli_FastMonsters == 0) fastparm = -1;

  if (cli_Respawn == 1) respawnparm = true;
  if (cli_NoMonsters == 1) NoMonsters = true;
  if (cli_NoMenuDef == 1) gz_skip_menudef = true;

  /*
  bool isChex = false;
  if (cliGameMode.strEquCI("chex")) {
    cliGameMode = "doom2"; // arbitrary
    //game_override_mode = GAME_Chex;
    fsys_onlyOneBaseFile = true; // disable autoloads
    isChex = true;
  }
  */

  if (cli_IWadName && cli_IWadName[0]) mainIWad = cli_IWadName;


  fsys_report_added_paks = !!reportIWads;

  //if (!isChex) fsys_onlyOneBaseFile = (cli_NakedBase > 0);
  fsys_onlyOneBaseFile = (cli_NakedBase > 0);

  // set up base directory (main data files)
  fl_basedir = ".";
  p = cli_BaseDir;
  if (p) {
    fl_basedir = p;
    if (fl_basedir.isEmpty()) fl_basedir = ".";
  } else {
    /*static*/ const char *defaultBaseDirs[] = {
#ifdef __SWITCH__
      ".",
#elif !defined(_WIN32)
      "/opt/vavoom/share/k8vavoom",
      "/opt/k8vavoom/share/k8vavoom",
      "/usr/local/share/k8vavoom",
      "/usr/share/k8vavoom",
      "!/../share/k8vavoom",
      ".",
#else
      "!/share",
      "!/.",
      ".",
#endif
      nullptr,
    };
    for (const char **tbd = defaultBaseDirs; *tbd; ++tbd) {
      VStr dir = VStr(*tbd);
      if (dir[0] == '!') {
        dir.chopLeft(1);
        dir = GParsedArgs.getBinDir()+dir;
      } else if (dir[0] == '~') {
        dir.chopLeft(1);
        const char *hdir = getenv("HOME");
        if (hdir && hdir[0]) {
          dir = VStr(hdir)+dir;
        } else {
          continue;
        }
      }
      if (Sys_DirExists(dir)) {
        fl_basedir = dir;
        break;
      }
    }
    if (fl_basedir.isEmpty()) Sys_Error("cannot find basedir; use \"-basedir dir\" to set it");
  }

  // set up config directory (files written by engine)
  if (cli_ConfigDir && cli_ConfigDir[0]) {
    fl_configdir = VStr(cli_ConfigDir).fixSlashes();
  } else {
    #if !defined(_WIN32)
    const char *HomeDir = getenv("HOME");
    if (HomeDir && HomeDir[0]) fl_configdir = VStr(HomeDir)+"/.k8vavoom";
    #else
    fl_configdir = ".";
    #endif
  }
  fl_configdir = fl_configdir.removeTrailingSlash();

  // set up save directory (files written by engine)
  p = cli_SaveDir;
  if (p && p[0]) {
    fl_savedir = p;
  } else {
    fl_savedir = fl_configdir.appendPath("saves");
  }
  fl_savedir = fl_savedir.removeTrailingSlash();

  // set up additional directories where to look for IWAD files
  p = cli_IWadDir;
  if (p) {
    if (p[0]) IWadDirs.Append(p);
  } else {
    /*static*/ const char *defaultIwadDirs[] = {
      ".",
      "!/.",
#ifdef __SWITCH__
      "./iwads",
#elif !defined(_WIN32)
      "~/.k8vavoom/iwads",
      "~/.k8vavoom/iwad",
      "~/.k8vavoom",
      "~/.vavoom/iwads",
      "~/.vavoom/iwad",
      "~/.vavoom",
      "/opt/vavoom/share/k8vavoom",
      "/opt/k8vavoom/share/k8vavoom",
      "/usr/local/share/k8vavoom",
      "/usr/share/k8vavoom",
      "!/../share/k8vavoom",
#else
      "~",
      "!/iwads",
      "!/iwad",
#endif
      nullptr,
    };
    for (const char **tbd = defaultIwadDirs; *tbd; ++tbd) {
      VStr dir = VStr(*tbd);
      if (dir[0] == '!') {
        dir.chopLeft(1);
        dir = GParsedArgs.getBinDir()+dir;
      } else if (dir[0] == '~') {
        dir.chopLeft(1);
        const char *hdir = getenv("HOME");
        if (hdir && hdir[0]) {
          dir = VStr(hdir)+dir;
        } else {
          continue;
        }
      }
      if (Sys_DirExists(dir)) {
        //GLog.Logf(NAME_Debug, "found iwad dir '%s'", *dir);
        IWadDirs.Append(dir);
      }
    }
  }
  // envvars: DOOMWADDIR
  {
    const char *dwd = getenv("DOOMWADDIR");
    if (dwd && dwd[0]) IWadDirs.Append(dwd);
  }
  // envvars: DOOMWADPATH ( https://doomwiki.org/wiki/Environment_variables )
  {
    const char *dwp = getenv("DOOMWADPATH");
    if (dwp) {
      VStr s(dwp);
      s = s.trimAll();
      while (s.length()) {
        int p0 = s.indexOf(':');
        int p1 = s.indexOf(';');
        #ifdef _WIN32
        if (p0 == 1 && VStr::isAlphaAscii(s[0])) p0 = -1; // looks like 'a:'
        #endif
             if (p0 >= 0 && p1 >= 0) p0 = min2(p0, p1);
        else if (p1 >= 0) { vassert(p0 < 0); p0 = p1; }
        if (p0 < 0) p0 = s.length();
        VStr pt = s.left(p0).trimAll();
        s.chopLeft(p0+1);
        s = s.trimLeft();
        if (pt.length()) {
          //GLog.Logf(NAME_Debug, "DWP: <%.*s>", pt.length(), *pt);
          IWadDirs.Append(pt);
        }
      }
    }
  }
#ifdef _WIN32
  // home dir (if any)
  /*
  {
    const char *hd = getenv("HOME");
    if (hd && hd[0]) IWadDirs.Append(hd);
  }
  */
#endif
  // and current dir
  //IWadDirs.Append(".");

  ParseDetectors("basev/detectors.txt");
  // parse detectors from home directory
  #ifndef _WIN32
  {
    const char *hdir = getenv("HOME");
    if (hdir && hdir[0]) {
      ParseDetectors(VStr(va("%s/.k8vavoom/detectors.rc", hdir)));;
    }
  }
  #endif

  int mapnum = -1;
  VStr mapname;
  bool mapinfoFound = false;

  // mount pwads
  FL_StartUserWads(); // start marking
  for (int pwidx = 0; pwidx < pwadList.length(); ++pwidx) {
    PWadFile &pwf = pwadList[pwidx];
    fsys_skipSounds = pwf.skipSounds;
    fsys_skipSprites = pwf.skipSprites;
    fsys_skipDehacked = pwf.skipDehacked;
    //!int nextfid = W_NextMountFileId();

    //GCon->Logf(NAME_Debug, "::: %d : <%s>", nextfid, *pwf.fname);
    int currFCount = fsysSearchPaths.length();

    auto mark = wpkMark(!pwf.storeInSave);

    if (pwf.asDirectory) {
      //if (pwf.storeInSave) wpkAppend(pwf.fname, false); // non-system pak
      GCon->Logf(NAME_Init, "Mounting directory '%s' as emulated PK3 file.", *pwf.fname);
      //AddPakDir(pwf.fname);
      W_MountDiskDir(pwf.fname);
    } else {
      //if (pwf.storeInSave) wpkAppend(pwf.fname, false); // non-system pak
      AddAnyFile(pwf.fname, true);
    }

    // ignore cosmetic pwads
    if (!pwf.storeInSave) {
      for (int f = currFCount; f < fsysSearchPaths.length(); ++f) fsysSearchPaths[f]->cosmetic = true;
    }

    wpkAddMarked(mark);

    fsys_hasPwads = true;
  }
  FL_EndUserWads(); // stop marking
  fsys_skipSounds = false;
  fsys_skipSprites = false;
  fsys_skipDehacked = false;

  // scan for user maps
  performPWadScan();
  if (pwadScanInfo.hasMapinfo) {
    mapinfoFound = true;
    fsys_hasMapPwads = true;
  } else if (!pwadScanInfo.mapname.isEmpty()) {
    mapnum = pwadScanInfo.getMapIndex();
    mapname = pwadScanInfo.mapname;
    fsys_hasMapPwads = true;
  }

  // save pwads to be added later
  FSysSavedState pwadsSaved;
  pwadsSaved.save();
  TArray<VStr> wpklistSaved = wpklist;
  wpklist.clear();


  ParseUserModes();

  AddGameDir("basev/common");

  //collectPWads();

  ProcessBaseGameDefs("basev/games.txt", mainIWad);
#ifdef DEVELOPER
  // i need progs to be loaded from files
  //fl_devmode = true;
#endif

  // process "warp", do it here, so "+var" will be processed after "map nnn"
  // postpone, and use `P_TranslateMapEx()` in host initialization
  if (warpTpl.length() > 0 && fsys_warp_n0 >= 0) {
    int fmtc = countFmtHash(warpTpl);
    if (fmtc >= 1 && fmtc <= 2) {
      if (fmtc == 2 && fsys_warp_n1 == -1) { fsys_warp_n1 = fsys_warp_n0; fsys_warp_n0 = 1; } // "-warp n" is "-warp 1 n" for ExMx
      VStr cmd = "map ";
      int spos = 0;
      int numidx = 0;
      while (spos < warpTpl.length()) {
        if (warpTpl[spos] == '#') {
          int len = 0;
          while (spos < warpTpl.length() && warpTpl[spos] == '#') { ++len; ++spos; }
          char tbuf[64];
          snprintf(tbuf, sizeof(tbuf), "%d", (numidx == 0 ? fsys_warp_n0 : fsys_warp_n1));
          VStr n = VStr(tbuf);
          while (n.length() < len) n = VStr("0")+n;
          cmd += n;
          ++numidx;
        } else {
          cmd += warpTpl[spos++];
        }
      }
      cmd += "\n";
      //GCmdBuf.Insert(cmd);
      fsys_warp_cmd = cmd;
      fsys_warp_n0 = -1;
      fsys_warp_n1 = -1;
    }
  }

  //customMode.dump();

  if (cli_GoreModForce != 0) {
    GCon->Logf(NAME_Init, "Forcing gore mod.");
    AddGameDir("basev/mods/gore"); // not disabled
  } else if (!customMode.disableGoreMod) {
    #if 0
    if (/*game_release_mode ||*/ isChex) {
      if (cli_GoreMod == 1) AddGameDir("basev/mods/gore"); // explicitly enabled
    } else {
      if (cli_GoreMod != 0) AddGameDir("basev/mods/gore"); // not disabled
    }
    #else
    if (cli_GoreMod != 0) AddGameDir("basev/mods/gore"); // not disabled
    #endif
  } else {
    GCon->Logf(NAME_Init, "Gore mod disabled.");
  }

  //if (isChex) AddGameDir("basev/mods/chex");

  // mark "iwad" flags
  for (int i = 0; i < fsysSearchPaths.length(); ++i) {
    fsysSearchPaths[i]->iwad = true;
  }

  // load custom mode pwads
  if (customMode.disableBloodReplacement) fsys_DisableBloodReplacement = true;
  if (customMode.disableBDW) fsys_DisableBDW = true;

  fsys_report_added_paks = !!reportPWads;
  //GCon->Logf(NAME_Debug, "!!!: %d", (fsys_report_added_paks ? 1 : 0));

  CustomModeLoadPwads(CM_PRE_PWADS);

  // mount pwads (actually, add already mounted ones)
  pwadsSaved.restore();
  for (auto &&it : wpklistSaved) wpklist.append(it);

  // load custom mode pwads
  CustomModeLoadPwads(CM_POST_PWADS);

  fsys_report_added_paks = !!reportIWads;

  if (!fsys_DisableBDW && cli_BDWMod > 0) AddGameDir("basev/mods/bdw");

  if (cli_SkeeHUD > 0) mdetect_AddMod("skeehud");

  for (auto &&xmod : modAddMods) {
    GCon->Logf(NAME_Init, "adding special built-in mod '%s'...", *xmod);
    AddGameDir(va("basev/mods/%s", *xmod));
  }
  modAddMods.clear(); // don't need it anymore

  fsys_report_added_paks = !!reportPWads;

  RenameSprites();

  if (doStartMap && fsys_warp_cmd.isEmpty() && !mapinfoFound && mapnum > 0 && mapname.length()) {
    GCon->Logf(NAME_Init, "FOUND MAP: %s", *mapname);
    mapname = va("map \"%s\"\n", *mapname.quote());
    //GCmdBuf.Insert(mapname);
    fsys_warp_cmd = mapname;
  } else if (doStartMap && fsys_warp_cmd.isEmpty() && mapinfoFound) {
    fsys_warp_cmd = "__k8_run_first_map";
  }

  // look for "+map"
  if (fsys_warp_cmd.length() == 0) {
    if (GArgs.CheckParm("+map") != 0) Host_CLIMapStartFound();
  }

  FreeDetectors(); // we don't need them
}


//==========================================================================
//
//  FL_Shutdown
//
//==========================================================================
void FL_Shutdown () {
  fl_basedir.Clean();
  fl_savedir.Clean();
  fl_gamedir.Clean();
  fl_configdir.Clean();
  IWadDirs.Clear();
  FSYS_Shutdown();
}


//==========================================================================
//
//  FL_OpenFileWrite
//
//==========================================================================
VStream *FL_OpenFileWrite (VStr Name, bool isFullName) {
  VStr tmpName;
  if (isFullName) {
    tmpName = Name;
  } else {
    if (fl_savedir.IsNotEmpty()) {
      tmpName = fl_savedir+"/"+fl_gamedir+"/"+Name;
    } else {
      tmpName = fl_basedir+"/"+fl_gamedir+"/"+Name;
    }
  }
  return FL_OpenSysFileWrite(tmpName);
}


//==========================================================================
//
//  FL_OpenFileReadInCfgDir
//
//==========================================================================
VStream *FL_OpenFileReadInCfgDir (VStr Name) {
  if (Name.isEmpty()) return nullptr;
  VStr diskName = FL_GetConfigDir().appendPath(Name);
  VStream *strm = FL_OpenSysFileRead(diskName);
  if (strm) return strm;
  return FL_OpenFileRead(Name);
}


//==========================================================================
//
//  FL_OpenFileWriteInCfgDir
//
//==========================================================================
VStream *FL_OpenFileWriteInCfgDir (VStr Name) {
  if (Name.isEmpty()) return nullptr;
  VStr diskName = FL_GetConfigDir().appendPath(Name);
  return FL_OpenSysFileWrite(diskName);
}


//==========================================================================
//
//  FL_GetConfigDir
//
//==========================================================================
VStr FL_GetConfigDir () {
  VStr res = fl_configdir;
  if (res.isEmpty()) res = fl_savedir;
  if (res.isEmpty()) {
    #if !defined(_WIN32)
    const char *HomeDir = getenv("HOME");
    res = (HomeDir && HomeDir[0] ? VStr(HomeDir)+"/.k8vavoom" : VStr("."));
    #else
    res = ".";
    #endif
  }
  Sys_CreateDirectory(res);
  return res;
}


//==========================================================================
//
//  FL_GetCacheDir
//
//==========================================================================
VStr FL_GetCacheDir () {
  VStr res = FL_GetConfigDir();
  if (res.isEmpty()) return res;
  res += "/.mapcache";
  Sys_CreateDirectory(res);
  return res;
}


//==========================================================================
//
//  FL_GetSavesDir
//
//==========================================================================
VStr FL_GetSavesDir () {
  VStr res = fl_savedir;
  if (res.isEmpty()) {
    #if !defined(_WIN32)
    const char *HomeDir = getenv("HOME");
    res = (HomeDir && HomeDir[0] ? VStr(HomeDir)+"/.k8vavoom/saves" : VStr("./saves"));
    #else
    res = "./saves";
    #endif
  }
  Sys_CreateDirectory(res);
  return res;
}


//==========================================================================
//
//  FL_GetScreenshotsDir
//
//==========================================================================
VStr FL_GetScreenshotsDir () {
  VStr res = FL_GetConfigDir();
  if (res.isEmpty()) return res;
  res += "/sshots";
  Sys_CreateDirectory(res);
  return res;
}


//==========================================================================
//
//  FL_GetUserDataDir
//
//==========================================================================
VStr FL_GetUserDataDir (bool shouldCreate) {
  VStr res = FL_GetConfigDir();
  res += "/userdata";
  //res += '/'; res += game_name;
  if (shouldCreate) Sys_CreateDirectory(res);
  return res;
}


//==========================================================================
//
//  FL_GetNetWadsCount
//
//==========================================================================
int FL_GetNetWadsCount () {
  int res = 0;
  for (auto &&sp : fsysSearchPaths) {
    if (!getNetPath(sp).isEmpty()) ++res;
  }
  return res;
}


//==========================================================================
//
//  FL_GetNetWadsHash
//
//==========================================================================
vuint32 FL_GetNetWadsHash () {
  int count = 0;
  VStr modlist;
  for (auto &&sp : fsysSearchPaths) {
    VStr s = getNetPath(sp);
    if (s.isEmpty()) continue;
    modlist += s;
    modlist += "\n";
    ++count;
  }
  return XXH32(*modlist, (vint32)modlist.length(), (vuint32)count);
}


//==========================================================================
//
//  FL_GetNetWads
//
//==========================================================================
void FL_GetNetWads (TArray<VStr> &list) {
  list.reset();
  for (auto &&sp : fsysSearchPaths) {
    VStr s = getNetPath(sp);
    if (s.isEmpty()) continue;
    //GCon->Logf(NAME_Debug, ":: <%s> (basepak=%d; iwad=%d; userwad=%d)", *s, (int)sp->basepak, (int)sp->iwad, (int)sp->userwad);
    list.append(s);
  }
}


//==========================================================================
//
//  FL_BuildRequiredWads
//
//==========================================================================
void FL_BuildRequiredWads () {
  #if 0
  // scan all textures, and mark any archive with textures as "required"
  GCon->Logf(NAME_Debug, "scanning %d textures...", GTextureManager.GetNumTextures());
  for (int f = 0; f < GTextureManager.GetNumTextures(); ++f) {
    VTexture *tex = GTextureManager[f];
    if (tex && tex->SourceLump >= 0) {
      int fl = W_LumpFile(tex->SourceLump);
      if (fl >= 0 && fl < fsysSearchPaths.length()) fsysSearchPaths[fl]->required = true;
    }
  }

  // scan all archives, and mark any archives with important files as "required"
  GCon->Log(NAME_Debug, "scanning archives...");
  for (auto &&arc : fsysSearchPaths) {
    if (arc->required) continue; // already marked
    for (int fn = arc->IterateNS(0, WADNS_Global); fn >= 0; fn = arc->IterateNS(fn+1, WADNS_Global)) {
      //GCon->Logf(NAME_Debug, "  %d: %s : %s", fn, *arc->GetPrefix(), *arc->LumpName(fn));
      if (arc->LumpName(fn) == "decorate" || arc->LumpName(fn) == "dehacked") {
        arc->required = true;
        break;
      }
    }
  }

  for (auto &&arc : fsysSearchPaths) if (arc->required) GCon->Logf(NAME_Debug, "rq: <%s>", *arc->GetPrefix());
  #endif
}
