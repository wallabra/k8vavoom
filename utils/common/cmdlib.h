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
#ifndef CMDLIB_H
#define CMDLIB_H

#include "../../libs/core/core.h"

#include <ctype.h>

#if !defined _WIN32 && !defined DJGPP
#undef stricmp  //  Allegro defines them
#undef strnicmp
#define stricmp   strcasecmp
#define strnicmp  strncasecmp
#endif

namespace VavoomUtils {

void Error (const char *error, ...) __attribute__((noreturn)) __attribute__((format(printf, 1, 2)));

void DefaultPath (char *path, size_t pathsize, const char *basepath);
void DefaultExtension (char *path, size_t pathsize, const char *extension);
void StripFilename (char *path);
void StripExtension (char *path);
void ExtractFilePath (const char *path, char *dest, size_t destsize);
void ExtractFileBase (const char *path, char *dest, size_t destsize);
void ExtractFileExtension (const char *path, char *dest, size_t destsize); // with dot
void FixFileSlashes (char *path);
int LoadFile (const char *name, void **bufferptr);

} // namespace VavoomUtils

#endif
