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
//**  Copyright (C) 2018-2021 Ketmar Dark
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
struct server_t {
  enum {
    IM_No,
    IM_EndLevel,
    IM_Finale,
  };
  int intermission; // IM_xxx
  float intertime;
};

struct server_static_t {
  int max_clients;
  int num_connected;
  int deathmatch; // actually, vuint8

  VStr serverinfo;
};

void SV_Init ();
void SV_Shutdown ();
void ServerFrame (int realtics);
void SV_ShutdownGame ();

// call this to perform various network bookkeeping
void SV_ServerInterframeBK ();
void SV_AllClientsNeedsWorldUpdate ();

void SV_ReplaceCustomDamageFactors ();

// set all player fields to defaults (calls `ResetToDefaults()` method)
void SV_ResetPlayers ();

void SV_ResetPlayerButtons ();

void SV_SendLoadedEvent ();
void SV_SendBeforeSaveEvent (bool isAutosave, bool isCheckpoint);
void SV_SendAfterSaveEvent (bool isAutosave, bool isCheckpoint);

// call this on map loading
void NET_SendNetworkHeartbeat (bool forced=false);

// loading mods, take list from modlistfile
// `modtypestr` is used to show loading messages
void G_LoadVCMods (VName modlistfile, const char *modtypestr); // in "sv_main.cpp"

vuint32 SV_GetModListHash ();

extern server_t sv;
extern server_static_t svs;
// after spawning a server, skip several rendering frames, so
// ACS fadein effects works better
// this is GLevel->TicTime value at which we should start rendering
extern int serverStartRenderFramesTic;

extern double SV_GetFrameTimeConstant ();
