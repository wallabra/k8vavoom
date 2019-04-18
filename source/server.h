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
struct server_t {
  int intermission;
  int intertime;
};

struct server_static_t {
  int max_clients;
  int num_connected;

  VStr serverinfo;
};

void SV_Init ();
void SV_Shutdown ();
void ServerFrame (int realtics);
void SV_ShutdownGame ();

// set all player fields to defaults (calls `ResetToDefaults()` method)
void SV_ResetPlayers ();

void SV_SendLoadedEvent ();
void SV_SendBeforeSaveEvent (bool isAutosave, bool isCheckpoint);
void SV_SendAfterSaveEvent (bool isAutosave, bool isCheckpoint);

// loading mods, take list from modlistfile
// `modtypestr` is used to show loading messages
void G_LoadVCMods (VName modlistfile, const char *modtypestr); // in "sv_main.cpp"


extern server_t sv;
extern server_static_t svs;
