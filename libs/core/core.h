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
#ifndef VAVOOM_CORE_HEADER
#define VAVOOM_CORE_HEADER

#define USE_NEUMAIER_KAHAN
#define USE_FAST_INVSQRT


// C headers
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cmath>
#include <cstdint>

#ifdef _WIN32
# include <windows.h>
#else
# include <climits>
#endif


#ifdef USE_NEUMAIER_KAHAN
# define VSUM2(value0,value1)                 (neumsum2((value0), (value1)))
# define VSUM3(value0,value1,value2)          (neumsum3((value0), (value1), (value2)))
# define VSUM4(value0,value1,value2,value3)   (neumsum4((value0), (value1), (value2), (value3)))
# define VSUM2D(value0,value1)                (neumsum2d((value0), (value1)))
# define VSUM3D(value0,value1,value2)         (neumsum3d((value0), (value1), (value2)))
#else
# define VSUM2(value0,value1)                 ((value0)+(value1))
# define VSUM3(value0,value1,value2)          ((value0)+(value1)+(value2))
# define VSUM4(value0,value1,value2,value3)   ((value0)+(value1)+(value2)+(value3))
# define VSUM2D(value0,value1)                ((value0)+(value1))
# define VSUM3D(value0,value1,value2)         ((value0)+(value1)+(value2))
#endif


#include "mythreadlite.h"

#include "common.h" // common types and definitions
#include "strtod_plan9.h"
#include "propp.h"
#include "hashfunc.h"
#include "jh32.h"
#include "rg32.h"
#include "sha2.h"
#include "xxhash32.h"
#include "xxhash.h"
#include "chacha20.h"
#include "poly1305-donna.h"
#include "ed25519.h"
#include "endianness.h" // endianes handling
#include "exception.h" // exception handling
#include "zone.h" // zone memory allocation
#include "names.h" // built-in names
#include "log.h" // general logging interface
#include "name.h" // names
#include "stream.h" // streams
#include "array.h" // dynamic arrays
#include "map.h" // mapping of keys to values
#include "crc.h" // CRC calcuation
#include "str.h" // strings
#include "args.h" // command line arguments
#include "xstreams.h" // extended streams
#include "zipstreams.h" // extended streams
#include "mathutil.h"
#include "vector.h" // vector math
#include "matrix.h" // matrices
#include "xml.h" // xml file parsing
#include "ntvalue.h"

#include "md5.h"

#include "hashset.h"
#include "wlist.h"

#include "minipng.h"

#include "syslow.h"

#include "timsort.h"

#endif
