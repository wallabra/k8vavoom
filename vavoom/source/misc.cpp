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
//**	Copyright (C) 1999-2001 J�nis Legzdi��
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

#define MAXARGVS        100

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

#ifndef DJGPP
static void M_FindResponseFile(void);
#endif

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int 	myargc;
char**	myargv;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char va_buffer[4][1024];
static int va_bufnum;

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	M_InitArgs
//
//==========================================================================

void M_InitArgs(int argc, char **argv)
{
	//	Save args
	myargc = argc;
	myargv = argv;

#ifndef DJGPP
	M_FindResponseFile();
#endif
}

//==========================================================================
//
//	M_FindResponseFile
//
// 	Find a Response File. We don't do this in DJGPP because it does this
// in startup code.
//
//==========================================================================

#ifndef DJGPP
static void M_FindResponseFile(void)
{
    int         i;
	
    for (i = 1;i < myargc;i++)
	  if (myargv[i][0] == '@')
	  {
	    FILE *  handle;
	    int     size;
	    int     k;
	    int     index;
	    int    	indexinfile;
	    char    *infile;
	    char    *file;
	    char    *moreargs[20];
	    char    *firstargv;
			
	    // READ THE RESPONSE FILE INTO MEMORY
	    handle = fopen(&myargv[i][1], "rb");
	    if (!handle)
	    {
			printf("\nNo such response file %s!", &myargv[i][1]);
			exit(1);
	    }
	    dprintf("Found response file %s!\n", &myargv[i][1]);
	    fseek(handle, 0, SEEK_END);
	    size = ftell(handle);
	    fseek(handle, 0, SEEK_SET);
	    file = (char*)malloc(size);
	    fread(file, size, 1, handle);
	    fclose(handle);
			
	    // KEEP ALL CMDLINE ARGS FOLLOWING @RESPONSEFILE ARG
	    for (index = 0,k = i+1; k < myargc; k++)
			moreargs[index++] = myargv[k];
			
	    firstargv = myargv[0];
	    myargv = (char**)malloc(sizeof(char *) * MAXARGVS);
	    memset(myargv, 0, sizeof(char *) * MAXARGVS);
	    myargv[0] = firstargv;
			
	    infile = file;
	    indexinfile = k = 0;
	    indexinfile++;  // SKIP PAST ARGV[0] (KEEP IT)
	    do
	    {
			myargv[indexinfile++] = infile+k;
			while(k < size &&
		      ((*(infile+k)>= ' '+1) && (*(infile+k)<='z')))
		    k++;
			*(infile+k) = 0;
			while(k < size &&
		      ((*(infile+k)<= ' ') || (*(infile+k)>'z')))
		    k++;
	    } while(k < size);
			
	    for (k = 0;k < index;k++)
			myargv[indexinfile++] = moreargs[k];
	    myargc = indexinfile;
	
	    // DISPLAY ARGS
		if (M_CheckParm("-debug"))
		{
		    dprintf("%d command-line args:\n",myargc);
		    for (k=1;k<myargc;k++)
				dprintf("%s\n",myargv[k]);
		}
	    break;
	  }
}
#endif

//==========================================================================
//
//  M_CheckParm
//
//  Checks for the given parameter in the program's command line arguments.
//  Returns the argument number (1 to argc-1) or 0 if not present
//
//==========================================================================

int M_CheckParm(const char *check)
{
	int		i;

	for (i = 1; i < myargc; i++)
	{
		if (!stricmp(check, myargv[i]) )
			return i;
	}

	return 0;
}

//==========================================================================
//
//	Endianess handling, swapping 16bit and 32bit.
//	WAD files are stored little endian.
//
//==========================================================================

short (*LittleShort)(short);
short (*BigShort)(short);
int (*LittleLong)(int);
int (*BigLong)(int);
float (*LittleFloat)(float);
float (*BigFloat)(float);

//	Swaping
static short ShortSwap(short x)
{
	return  ((word)x >> 8) |
			((word)x << 8);
}
static short ShortNoSwap(short x)
{
	return x;
}
static int LongSwap(int x)
{
	return 	((dword)x >> 24) |
			(((dword)x >> 8) & 0xff00) |
			(((dword)x << 8) & 0xff0000) |
			((dword)x << 24);
}
static int LongNoSwap(int x)
{
	return x;
}
static float FloatSwap(float x)
{
	union { float f; long l; } a;
	a.f = x;
	a.l = LongSwap(a.l);
	return a.f;
}
static float FloatNoSwap(float x)
{
	return x;
}

void M_InitByteOrder(void)
{
	byte    swaptest[2] = {1, 0};

	// set the byte swapping variables in a portable manner
	if (*(short *)swaptest == 1)
	{
		BigShort = ShortSwap;
		LittleShort = ShortNoSwap;
		BigLong = LongSwap;
		LittleLong = LongNoSwap;
		BigFloat = FloatSwap;
		LittleFloat = FloatNoSwap;
	}
	else
	{
		BigShort = ShortNoSwap;
		LittleShort = ShortSwap;
		BigLong = LongNoSwap;
		LittleLong = LongSwap;
		BigFloat = FloatNoSwap;
		LittleFloat = FloatSwap;
	}
}

//==========================================================================
//
//	M_ReadFile
//
//==========================================================================

int M_ReadFile(const char* name, byte** buffer)
{
	int			handle;
	int			count;
	int			length;
	byte		*buf;
	
	handle = Sys_FileOpenRead(name);
	if (handle == -1)
	{
		Sys_Error("Couldn't open file %s", name);
	}
	length = Sys_FileSize(handle);
	buf = (byte*)Z_Malloc(length + 1, PU_STATIC, NULL);
	count = Sys_FileRead(handle, buf, length);
	buf[length] = 0;
	Sys_FileClose(handle);
	
	if (count < length)
	{
		Sys_Error("Couldn't read file %s", name);
	}

	*buffer = buf;
	return length;
}

//==========================================================================
//
//	M_WriteFile
//
//==========================================================================

boolean M_WriteFile(const char* name, const void* source, int length)
{
	int		handle;
	int		count;
	
	handle = Sys_FileOpenWrite(name);
	if (handle == -1)
	{
		return false;
	}

	count = Sys_FileWrite(handle, source, length);
	Sys_FileClose(handle);
	
	if (count < length)
	{
		return false;
	}
		
	return true;
}

//==========================================================================
//
//  superatoi
//
//==========================================================================

int superatoi(const char *s)
{
	int n=0, r=10, x, mul=1;
	const char *c=s;

	for (; *c; c++)
	{
		x = (*c & 223) - 16;

		if (x == -3)
		{
			mul = -mul;
		}
		else if (x == 72 && r == 10)
		{
			n -= (r=n);
			if (!r) r=16;
			if (r<2 || r>36) return -1;
		}
		else
		{
			if (x>10) x-=39;
			if (x >= r) return -1;
			n = (n*r) + x;
		}
	}
	return(mul*n);
}

//==========================================================================
//
//	va
//
//	Very usefull function from QUAKE
//	Does a varargs printf into a temp buffer, so I don't need to have
// varargs versions of all text functions.
//	FIXME: make this buffer size safe someday
//
//==========================================================================

char *va(const char *text, ...)
{
	va_list args;

	va_bufnum = (va_bufnum + 1) & 3;
	va_start(args, text);
	vsprintf(va_buffer[va_bufnum], text, args);
	va_end(args);

	return va_buffer[va_bufnum];
}

//==========================================================================
//
//	PassFloat
//
//==========================================================================

int PassFloat(float f)
{
	union
	{
		float	f;
		int		i;
	} v;

	v.f = f;
	return v.i;
}

//**************************************************************************
//
//	$Log$
//	Revision 1.7  2001/10/08 17:33:40  dj_jl
//	Four va buffers
//
//	Revision 1.6  2001/09/05 12:21:42  dj_jl
//	Release changes
//	
//	Revision 1.5  2001/08/30 17:42:04  dj_jl
//	Cleanup
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
