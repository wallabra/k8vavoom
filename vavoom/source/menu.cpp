//**************************************************************************
//**
//**	##   ##    ##    ##   ##   ####     ####   ###     ###
//**	##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**	 ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**	 ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**	  ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**	   #    ##    ##    #      ####     ####   ##       ##
//**
//**	$Id$
//**
//**	Copyright (C) 1999-2002 J�nis Legzdi��
//**
//**	This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**	This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
//**	
//**	DOOM selection menu, options, episode etc.
//**	Sliders and icons. Kinda widget stuff.
//**	
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"
#include "cl_local.h"
#include "ui.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void SV_ShutdownServer(boolean crash);
void CL_Disconnect(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	COMMAND SetMenu
//
//==========================================================================

COMMAND(SetMenu)
{
	GClGame->eventSetMenu(Args.Num() > 1 ? *Args[1] : "");
}

//==========================================================================
//
//	MN_Init
//
//==========================================================================

void MN_Init()
{
#ifdef SERVER
	GClGame->ClientFlags |= VClientGameBase::CF_LocalServer;
#else
	GClGame->ClientFlags &= ~VClientGameBase::CF_LocalServer;
#endif
	VRootWindow::StaticInit();
	GClGame->eventRootWindowCreated();
}

//==========================================================================
//
//  MN_ActivateMenu
//
//==========================================================================

void MN_ActivateMenu()
{
    // intro might call this repeatedly
    if (!MN_Active())
	{
		GClGame->eventSetMenu("Main");
	}
}

//==========================================================================
//
//	MN_DeactivateMenu
//
//==========================================================================

void MN_DeactivateMenu()
{
	GClGame->eventDeactivateMenu();
}

//==========================================================================
//
//  MN_Responder
//
//==========================================================================

boolean MN_Responder(event_t* event)
{
	if (GClGame->eventMessageBoxResponder(event))
	{
		return true;
	}

    // Pop-up menu?
    if (!MN_Active() && event->type == ev_keydown && !C_Active() &&
		(cls.state != ca_connected || cls.demoplayback) &&
		event->data1 != '`' && (event->data1 < K_F1 || event->data1 > K_F12))
	{
		MN_ActivateMenu();
		return true;
	}

	return GClGame->eventMenuResponder(event);
}

//==========================================================================
//
//	MN_Drawer
//
//==========================================================================

void MN_Drawer()
{
	GRoot->TickWindows(host_frametime);
	GRoot->PaintWindows();
	GClGame->eventMessageBoxDrawer();
}

//==========================================================================
//
//	MN_Active
//
//==========================================================================

boolean MN_Active()
{
	return GClGame->eventMenuActive() || GClGame->eventMessageBoxActive();
}

//**************************************************************************
//
//	$Log$
//	Revision 1.16  2006/04/05 17:23:37  dj_jl
//	More dynamic string usage in console command class.
//	Added class for handling command line arguments.
//
//	Revision 1.15  2006/03/12 12:54:48  dj_jl
//	Removed use of bitfields for portability reasons.
//	
//	Revision 1.14  2006/02/09 22:35:54  dj_jl
//	Moved all client game code to classes.
//	
//	Revision 1.13  2005/12/25 19:20:02  dj_jl
//	Moved title screen into a class.
//	
//	Revision 1.12  2002/05/29 16:51:50  dj_jl
//	Started a work on native Window classes.
//	
//	Revision 1.11  2002/02/02 19:20:41  dj_jl
//	FFunction pointers used instead of the function numbers
//	
//	Revision 1.10  2002/01/07 12:16:42  dj_jl
//	Changed copyright year
//	
//	Revision 1.9  2001/12/27 17:36:47  dj_jl
//	Some speedup
//	
//	Revision 1.8  2001/10/09 17:25:02  dj_jl
//	Finished slist moving stuff
//	
//	Revision 1.7  2001/10/08 17:34:57  dj_jl
//	A lots of small changes and cleanups
//	
//	Revision 1.6  2001/09/25 17:04:45  dj_jl
//	Replaced menu commands with command SetMenu
//	
//	Revision 1.5  2001/08/30 17:39:51  dj_jl
//	Moved view border and message box to progs
//	
//	Revision 1.4  2001/08/07 16:49:26  dj_jl
//	Added C_Active
//	
//	Revision 1.3  2001/07/31 17:09:34  dj_jl
//	Removed seting shareware and ExtendedWAD in progs
//	
//	Revision 1.2  2001/07/27 14:27:54  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
