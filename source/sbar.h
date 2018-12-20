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

// status bar height at bottom of screen
//#define SB_REALHEIGHT ((int)(sb_height * fScaleY))
//#define SB_REALHEIGHT ((int)(ScreenHeight/640.0f*sb_height*R_GetAspectRatio()))


// ////////////////////////////////////////////////////////////////////////// //
void SB_Init ();
void SB_Drawer ();
void SB_Ticker ();
bool SB_Responder (event_t *ev);
void SB_Start (); // called when the console player is spawned on each level
int SB_RealHeight ();


// ////////////////////////////////////////////////////////////////////////// //
extern int sb_height;
