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

#ifndef CMDLIB_H
#define CMDLIB_H

// HEADER FILES ------------------------------------------------------------

//	C headers
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

//	C++ headers
#include <iostream.h>

#ifndef __GNUC__
#define __attribute__(whatever)
#endif

#if defined __unix__ && !defined DJGPP
#undef stricmp	//	Allegro defines them
#undef strnicmp
#define stricmp		strcasecmp
#define strnicmp	strncasecmp
#endif

namespace VavoomUtils {

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef int					boolean;	//	Must be 4 bytes long
typedef unsigned char 		byte;
typedef unsigned short	 	word;
typedef unsigned long	 	dword;

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void Error(const char *error, ...) __attribute__ ((noreturn))
	__attribute__ ((format(printf, 1, 2)));

char *va(const char *text, ...) __attribute__ ((format(printf, 1, 2)));

short LittleShort(short val);
int LittleLong(int val);

void DefaultPath(char *path, const char *basepath);
void DefaultExtension(char *path, const char *extension);
void StripFilename(char *path);
void StripExtension(char *path);
void ExtractFilePath(const char *path, char *dest);
void ExtractFileBase(const char *path, char *dest);
void ExtractFileExtension(const char *path, char *dest);
void FixFileSlashes(char *path);
int LoadFile(const char *name, void **bufferptr);

// PUBLIC DATA DECLARATIONS ------------------------------------------------

extern void *(*Malloc)(size_t size);
extern void *(*Realloc)(void *data, size_t size);
extern void (*Free)(void *ptr);

template<class T> T* New(void)
{
	return (T*)Malloc(sizeof(T));
}

template<class T> T* New(int numel)
{
	return (T*)Malloc(numel * sizeof(T));
}

inline void Delete(void *ptr)
{
	Free(ptr);
}

} // namespace VavoomUtils

#endif

//**************************************************************************
//
//	$Log$
//	Revision 1.6  2001/12/27 17:42:07  dj_jl
//	Added FixupPath
//
//	Revision 1.5  2001/12/12 19:18:07  dj_jl
//	Added Realloc
//	
//	Revision 1.4  2001/09/12 17:28:38  dj_jl
//	Created glVIS plugin
//	
//	Revision 1.3  2001/08/21 17:51:21  dj_jl
//	Beautification
//	
//	Revision 1.2  2001/07/27 14:27:54  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
