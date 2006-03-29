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

//============================================================================

class TSizeBuf
{
 public:
	TSizeBuf(void)
	{
		AllowOverflow = false;
		Overflowed = false;
		Data = NULL;
		MaxSize = 0;
		CurSize = 0;
	}
	TSizeBuf(byte* AData, int ASize)
	{
		AllowOverflow = false;
		Overflowed = false;
		Data = AData;
		MaxSize = ASize;
		CurSize = 0;
	}

	void Alloc(int startsize);
	void Free(void);
	void Clear(void);
	void *GetSpace(int length);
	void Write(const void *data, int length);
	bool CheckSpace(int length)
	{
		return CurSize + length <= MaxSize;
	}

	boolean	AllowOverflow;	// if false, do a Sys_Error
	boolean	Overflowed;		// set to true if the buffer size failed
	byte	*Data;
	int		MaxSize;
	int		CurSize;
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PUBLIC DATA DECLARATIONS ------------------------------------------------

//**************************************************************************
//
//	$Log$
//	Revision 1.6  2006/03/29 22:32:27  dj_jl
//	Changed console variables and command buffer to use dynamic strings.
//
//	Revision 1.5  2002/07/13 07:43:03  dj_jl
//	Added space checking.
//	
//	Revision 1.4  2002/01/07 12:16:43  dj_jl
//	Changed copyright year
//	
//	Revision 1.3  2001/07/31 17:16:31  dj_jl
//	Just moved Log to the end of file
//	
//	Revision 1.2  2001/07/27 14:27:54  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
