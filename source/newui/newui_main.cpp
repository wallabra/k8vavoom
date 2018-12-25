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
#include "../gamedefs.h"

#ifdef CLIENT
#include "../cl_local.h"
#include "../drawer.h"
#include "newui.h"


// ////////////////////////////////////////////////////////////////////////// //
struct NewUIICB {
  NewUIICB () {
    VDrawer::RegisterICB(&drawerICB);
  }

  static void drawerICB (int phase) {
    //GCon->Logf("NEWUI: phase=%d", phase);
  }
};

static NewUIICB newuiicb;


// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  NUI_IsPaused
//
//==========================================================================
bool NUI_IsPaused () {
  return false;
}


//==========================================================================
//
//  NUI_Responder
//
//==========================================================================
bool NUI_Responder (event_t *ev) {
  return false;
}


#endif
