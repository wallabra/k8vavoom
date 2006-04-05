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

// HEADER FILES ------------------------------------------------------------

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void Host_Init(void);
void Host_Shutdown(void);
void Host_Frame(void);
void __attribute__((noreturn, format(printf, 1, 2))) __declspec(noreturn)
	Host_EndGame(const char *message, ...);
void __attribute__((noreturn, format(printf, 1, 2))) __declspec(noreturn)
	Host_Error(const char *error, ...);
void Host_CoreDump(const char *fmt, ...);
const char *Host_GetCoreDump(void);

// PUBLIC DATA DECLARATIONS ------------------------------------------------

extern VCvarI		developer;

extern boolean		host_initialized;

extern int			host_frametics;
extern double		host_frametime;
extern double		host_time;
extern double		realtime;
extern int			host_framecount;

//**************************************************************************
//
//	$Log$
//	Revision 1.11  2006/04/05 17:23:37  dj_jl
//	More dynamic string usage in console command class.
//	Added class for handling command line arguments.
//
//	Revision 1.10  2002/04/11 16:40:32  dj_jl
//	Added __declspec modifiers.
//	
//	Revision 1.9  2002/01/07 12:16:42  dj_jl
//	Changed copyright year
//	
//	Revision 1.8  2002/01/03 18:38:25  dj_jl
//	Added guard macros and core dumps
//	
//	Revision 1.7  2001/08/31 17:26:48  dj_jl
//	Attribute syntax change
//	
//	Revision 1.6  2001/08/30 17:46:21  dj_jl
//	Removed game dependency
//	
//	Revision 1.5  2001/08/21 17:41:33  dj_jl
//	Removed -devmaps option
//	
//	Revision 1.4  2001/08/04 17:25:14  dj_jl
//	Moved title / demo loop to progs
//	Removed shareware / ExtendedWAD from engine
//	
//	Revision 1.3  2001/07/31 17:16:30  dj_jl
//	Just moved Log to the end of file
//	
//	Revision 1.2  2001/07/27 14:27:54  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
