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
struct ArgVarValue {
  VStr varname;
  VStr value;
};


void FL_InitOptions ();
void FL_Init ();
void FL_Shutdown ();

VStr FL_GetConfigDir ();

// set `isFullName` to `true` to prevent adding anything to file name
VStream *FL_OpenFileWrite (VStr Name, bool isFullName=false);

VStream *FL_OpenFileReadInCfgDir (VStr Name);
VStream *FL_OpenFileWriteInCfgDir (VStr Name);

VStream *FL_OpenSysFileRead (VStr Name);
VStream *FL_OpenSysFileWrite (VStr Name);


VStr FL_GetConfigDir ();
VStr FL_GetCacheDir ();
VStr FL_GetSavesDir ();
VStr FL_GetScreenshotsDir ();
VStr FL_GetUserDataDir (bool shouldCreate);


// not a real list, but something that can be used to check savegame validity
const TArray<VStr> &FL_GetWadPk3List ();


// used to set "preinit" cvars
int FL_GetPreInitCount ();
const ArgVarValue &FL_GetPreInitAt (int idx);
void FL_ClearPreInits ();
bool FL_HasPreInit (VStr varname);

void FL_CollectPreinits ();
void FL_ProcessPreInits ();


struct GameOptions {
  bool hexenGame;

  GameOptions () : hexenGame(false) {}
};


//extern bool fl_devmode;
extern VStr fl_basedir;
extern VStr fl_savedir;
extern VStr fl_gamedir;

extern bool fsys_hasPwads; // or paks
extern bool fsys_hasMapPwads; // or paks

extern bool fsys_DisableBloodReplacement;
// in fsys
//extern bool fsys_IgnoreZScript;
//extern bool fsys_DisableBDW;

struct PWadMapLump {
  int lump;
  VStr mapname;
  int episode; // 0: doom2
  int mapnum;

  PWadMapLump () noexcept : lump(-1), mapname(), episode(-1), mapnum(-1) {}
  inline void clear () noexcept { lump = -1; mapname.clear(); episode = -1; mapnum = -1; }
  inline bool isValid () const noexcept { return (lump >= 0); }
  inline int getMapIndex () const noexcept { return (episode > 0 ? episode*10+mapnum : episode == 0 ? mapnum : 0); }

  inline bool isEqual (const PWadMapLump &n) const noexcept { return (episode == n.episode && mapnum == n.mapnum && mapname == n.mapname); }

  // name must be lowercased
  bool parseMapName (const char *name) noexcept;
};

extern TArray<PWadMapLump> fsys_PWadMaps; // sorted


extern VCvarS game_name;

extern GameOptions game_options;

extern int cli_WAll;
extern int cli_NoMonsters;
extern int cli_CompileAndExit;
extern int cli_NoExternalDeh;
extern int cli_GoreMod; // !=0: enabled


int FL_GetNetWadsCount ();
vuint32 FL_GetNetWadsHash ();
// will clear the list
void FL_GetNetWads (TArray<VStr> &list);
