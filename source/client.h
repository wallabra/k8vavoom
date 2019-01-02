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

struct client_static_t {
  // personalization data sent to server
  VStr userinfo;

  // demo recording info must be here, because record is started before
  // entering a map (and clearing client_state_t)
  bool demorecording;
  bool demoplayback;
  VStream *demofile;

  // connection information
  int signon; // 0 to SIGNONS
};


void CL_Init ();
void CL_Shutdown ();
void CL_SendMove ();
bool CL_Responder (event_t *ev);
void CL_ReadFromServer ();
void CL_SetUpLocalPlayer ();
void CL_SetUpStandaloneClient ();


extern client_static_t  cls;
extern VBasePlayer *cl;
