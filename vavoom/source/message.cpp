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
//**	Copyright (C) 1999-2006 Jānis Legzdiņš
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

#include "gamedefs.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
//  VMessageOut::Alloc
//
//==========================================================================

void VMessageOut::AllocBits(vint32 startsize)
{
	guard(VMessageOut::Alloc);
	if (Data)
		Z_Free(Data);
	Data = (vuint8*)Z_Malloc((startsize + 7) >> 3);
	MaxSizeBits = startsize;
	CurSizeBits = 0;
	Overflowed = false;
	unguard;
}

//==========================================================================
//
//  VMessageOut::Free
//
//==========================================================================

void VMessageOut::Free()
{
	guard(VMessageOut::Free);
	Z_Free(Data);
	Data = NULL;
	MaxSizeBits = 0;
	CurSizeBits = 0;
	unguard;
}

//==========================================================================
//
//  VMessageOut::Clear
//
//==========================================================================

void VMessageOut::Clear()
{
	CurSizeBits = 0;
}

//==========================================================================
//
//  VMessageOut::GetSpace
//
//==========================================================================

void* VMessageOut::GetSpaceBits(vint32 length)
{
	guard(VMessageOut::GetSpace);
	if (CurSizeBits + length > MaxSizeBits)
	{
		if (!AllowOverflow)
			Sys_Error("TSizeBuf::GetSpace: overflow without allowoverflow set");

		if (length > MaxSizeBits)
			Sys_Error("TSizeBuf::GetSpace: %i is > full buffer size", length);

		Overflowed = true;
		GCon->Log("TSizeBuf::GetSpace: overflow");
		Clear();
	}

	void* data = Data + ((CurSizeBits + 7) >> 3);
	CurSizeBits += length;

	return data;
	unguard;
}

//==========================================================================
//
//  VMessageOut::Serialise
//
//==========================================================================

void VMessageOut::Serialise(void* data, vint32 length)
{
	guard(VMessageOut::Serialise);
	memcpy(GetSpaceBits(length << 3), data, length);
	unguard;
}

//==========================================================================
//
//	VMessageOut::operator << VMessageOut
//
//==========================================================================

VMessageOut& VMessageOut::operator << (const VMessageOut &msg)
{
	guard(VMessageOut::operator << VMessageOut);
	Serialise(msg.Data, msg.GetCurSize());
	return *this;
	unguard;
}

//==========================================================================
//
//  VMessageIn::AllocBits
//
//==========================================================================

void VMessageIn::AllocBits(vint32 startsize)
{
	guard(VMessageIn::AllocBits);
	Data = (vuint8*)Z_Malloc((startsize + 7) >> 3);
	MaxSizeBits = startsize;
	CurSizeBits = 0;
	bLoading = true;
	unguard;
}

//==========================================================================
//
//  VMessageIn::Free
//
//==========================================================================

void VMessageIn::Free()
{
	guard(VMessageIn::Free);
	Z_Free(Data);
	Data = NULL;
	MaxSizeBits = 0;
	CurSizeBits = 0;
	unguard;
}

//==========================================================================
//
//  VMessageIn::SetDataBits
//
//==========================================================================

void VMessageIn::SetDataBits(const void* AData, vint32 ALength)
{
	guard(VMessageIn::SetDataBits);
	if (ALength > MaxSizeBits)
		Sys_Error("TSizeBuf::GetSpace: %i is > full buffer size", ALength);
	memcpy(Data, AData, (ALength + 7) >> 3);
	CurSizeBits = ALength;
	unguard;
}

//==========================================================================
//
//  VMessageIn::BeginReading
//
//==========================================================================

void VMessageIn::BeginReading()
{
	ReadCountBits = 0;
	BadRead = false;
}

//==========================================================================
//
//  VMessageIn::Serialise
//
//==========================================================================

void VMessageIn::Serialise(void* AData, int ALen)
{
	guard(VMessageIn::Serialise);
	if (ReadCountBits + (ALen << 3) > CurSizeBits)
	{
		BadRead = true;
		memset(AData, 0, ALen);
	}
	else
	{
		memcpy(AData, Data + ((ReadCountBits + 7) >> 3), ALen);
		ReadCountBits += ALen << 3;
	}
	unguard;
}
