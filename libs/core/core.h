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
//**  Copyright (C) 2018-2020 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, version 3 of the License ONLY.
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

#if (-1 & 3) != 3
# error "You need two's complement system to compile k8vavoom!"
#endif

// defines to include various parts
//#define CORE_CHACHAPRNG_C
//#define CORE_CHACHAPRNG_CPP
//#define CORE_MORE_HASHES



#ifndef VAVOOM_DEBUG_BUILD
# define VAVOOM_USE_MIMALLOC
#endif

#if defined(VAVOOM_DISABLE_MIMALLOC) && defined(VAVOOM_USE_MIMALLOC)
# undef VAVOOM_USE_MIMALLOC
#endif

// force shitdoze headers version
#ifdef _WIN32
# if !defined(_WIN32_WINNT)
#  define _WIN32_WINNT  0x0600
# elif _WIN32_WINNT < 0x0600
#  undef _WIN32_WINNT
#  define _WIN32_WINNT  0x0600
# endif
/*
# define WIN32_LEAN_AND_MEAN
*/
#endif
// define this to activate zone allocator counters
//#define VAVOOM_CORE_COUNT_ALLOCS

//#define VAVOOM_HIDE_ENDIANNES_CONVERSIONS


//#define USE_NEUMAIER_KAHAN
//#define USE_FAST_INVSQRT

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

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

#define VVA_OKUNUSED     __attribute__((unused))
#define VVA_CHECKRESULT  __attribute__((warn_unused_result))
#define VVA_PURE         __attribute__((pure))
#define VVA_CONST        __attribute__((const))

#ifdef VAVOOM_USE_MIMALLOC
# include "mimalloc/mimalloc.h"
#endif

#include "mythreadlite.h"
#include "crypto/prng_randombytes.h"

#include "common.h" // common types and definitions
#include "strtod_plan9.h"
#include "propp.h"

#include "hash/hashfunc.h"
#include "hash/crc.h" // CRC calcuation
#include "hash/xxhash.h"
#include "hash/md5.h"
#ifdef CORE_MORE_HASHES
# include "hash/rg32.h"
# include "hash/SpookyV2.h"
#endif

#include "crypto/sha2.h"
#include "crypto/hmac_sha2.h"
#include "crypto/chacha20.h"
#include "crypto/poly1305-donna.h"
#include "crypto/ed25519.h"
#include "crypto/curve25519-donna.h"
#ifdef CORE_MORE_HASHES
# include "crypto/jh32.h"
#endif

#include "lzmadec/LzmaDec.h"
#include "miniz/miniz.h"

#include "endianness.h" // endianes handling
#include "exception.h" // exception handling
#include "prngs.h"
#include "zone.h" // zone memory allocation
#include "names.h" // built-in names
#include "log.h" // general logging interface
#include "name.h" // names
#include "stream/stream.h" // streams
#include "array.h" // dynamic arrays
#include "map.h" // mapping of keys to values
#include "str.h" // strings
#include "vstrci.h"
#include "args.h" // command line arguments
#include "stream/streamsbit.h" // bitstreams
#include "stream/streamsex.h" // extended streams
#include "stream/streamszip.h" // packed streams

#include "mathutil.h"

// vector math
#include "vecmat/vector_avec.h"
#include "vecmat/vector_3d.h"
#include "vecmat/vertex2d.h"
#include "vecmat/plane_3d.h"
#include "vecmat/frustum.h"
// matrices
#include "vecmat/matrix4.h"
#include "vecmat/matrixrot.h"
#include "vecmat/matrixvp.h"

#include "colorutil.h"
#include "xml.h" // xml file parsing
#include "stream/ntvalue.h"

#include "wlist.h"

#include "minipng.h"

#include "syslow.h"

#include "timsort.h"

#include "micropather.h"

#include "cvarsys.h"

#ifdef CORE_CHACHAPRNG_C
# include "crypto/chachaprng_c.h"
#endif
#ifdef CORE_CHACHAPRNG_CPP
# include "crypto/chachaprng_cpp.h"
#endif


#include "fsys/fsys.h"

#include "rawtty.h"

#endif
