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

extern VCvarS game_name;

extern GameOptions game_options;

extern int cli_WAll;
extern int cli_NoMonsters;
extern int cli_CompileAndExit;
