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
#ifndef VCCRUN_VC_HEADER_FILE
#define VCCRUN_VC_HEADER_FILE

#include <stdio.h>

#include "../libs/core/core.h"
#include "../source/common.h"

#include "vcc_netobj.h"

#include "convars.h"
#include "filesys/fsys.h"

#include "../source/misc.h"


extern int devprintf (const char *text, ...) __attribute__((format(printf, 1, 2)));
extern char *va (const char *text, ...) __attribute__((format(printf, 1, 2)));

void Host_Error (const char *error, ...) __attribute__((noreturn, format(printf, 1, 2)));

//extern VStream *OpenFile (const VStr &Name);


#endif
