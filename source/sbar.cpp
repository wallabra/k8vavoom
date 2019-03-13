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
//**
//**  Status bar code.
//**
//**************************************************************************
#include "gamedefs.h"
#include "cl_local.h"
#include "drawer.h"


enum {
  SB_VIEW_NORMAL,
  SB_VIEW_AUTOMAP,
  SB_VIEW_FULLSCREEN
};


extern VCvarI screen_size;

int sb_height = 32;


//==========================================================================
//
//  SB_RealHeight
//
//==========================================================================
int SB_RealHeight () {
  //return (int)(ScreenHeight/640.0f*sb_height*R_GetAspectRatio());
  return (int)(ScreenHeight/480.0f*sb_height*R_GetAspectRatio());
}


//==========================================================================
//
//  SB_Init
//
//==========================================================================
void SB_Init () {
  sb_height = GClGame->sb_height;
}


//==========================================================================
//
//  SB_Ticker
//
//==========================================================================
void SB_Ticker () {
  if (cl && cls.signon && cl->MO) GClGame->eventStatusBarUpdateWidgets(host_frametime);
}


//==========================================================================
//
//  SB_Responder
//
//==========================================================================
bool SB_Responder (event_t *) {
  return false;
}


//==========================================================================
//
//  SB_Drawer
//
//==========================================================================
void SB_Drawer () {
  // update widget visibility
  if (automapactive > 0 && screen_size == 11) return;
  if (!GClLevel) return;
  GClGame->eventStatusBarDrawer(automapactive > 0 && screen_size < 11 ?
      SB_VIEW_AUTOMAP :
      GClLevel->RenderData->refdef.height == ScreenHeight ? SB_VIEW_FULLSCREEN : SB_VIEW_NORMAL);
}


//==========================================================================
//
//  SB_Start
//
//==========================================================================
void SB_Start () {
  GClGame->eventStatusBarStartMap();
}
