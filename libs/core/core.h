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
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
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
#ifndef _CORE_H
#define _CORE_H

// C headers
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cmath>

#ifdef _WIN32
# include <windows.h>
#endif

#include "mythreadlite.h"

#include "common.h" // common types and definitions
#include "propp.h"
#include "hashfunc.h"
#include "jh32.h"
#include "rg32.h"
#include "sha2.h"
#include "xxhash32.h"
#include "chacha20.h"
#include "poly1305-donna.h"
#include "ed25519.h"
#include "endian.h" // endianes handling
#include "exception.h" // exception handling
#include "zone.h" // zone memory allocation
#include "names.h" // built-in names
#include "log.h" // general logging interface
#include "stream.h" // streams
#include "array.h" // dynamic arrays
#include "map.h" // mapping of keys to values
#include "crc.h" // CRC calcuation
#include "name.h" // names
#include "str.h" // strings
#include "args.h" // command line arguments
#include "memorystream.h"// in-memory streams
#include "arraystream.h"// stream for reading from array
#include "bitstream.h" // streams for bit-data
#include "mathutil.h"
#include "vector.h" // vector math
#include "matrix.h" // matrices
#include "xml.h" // xml file parsing

#include "md5.h"

#include "hashset.h"
#include "wlist.h"

#include "minipng.h"

#include "syslow.h"

#include "timsort.h"

#endif
