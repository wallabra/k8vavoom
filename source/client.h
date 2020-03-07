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

struct client_static_t {
  // personalization data sent to server
  VStr userinfo;

  // demo recording info must be here, because record is started before
  // entering a map (and clearing client_state_t)
  bool demorecording;
  bool demoplayback;
  VStream *demofile;

  // connection information
  // set to 1 when client got `VLevelInfo` object from the server
  // this means that we at least sent "Client_Spawn" to server
  int signon;
  // set to 1 when client loaded a map
  // set to 2 when client executed `PreRender()`
  int gotmap;

  inline void clearForClient () noexcept {
    signon = 0;
    gotmap = 0;
  }

  inline void clearForStandalone () noexcept {
    signon = 1;
    gotmap = 2; // yeah
  }

  inline void clearForDisconnect () noexcept {
    signon = 0;
    gotmap = 0;
    demorecording = false;
    demoplayback = false;
  }
};


void CL_Init ();
void CL_Shutdown ();
void CL_SendMove ();
void CL_NetInterframe ();
bool CL_Responder (event_t *ev);
void CL_ReadFromServer (float deltaTime);
void CL_SetupLocalPlayer ();
void CL_SetupStandaloneClient ();

extern client_static_t cls;
extern VBasePlayer *cl;

extern float clWipeTimer;
