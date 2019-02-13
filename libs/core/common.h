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
#ifndef _WIN32
# define __declspec(whatever)
#endif

//==========================================================================
//
//  Basic types
//
//==========================================================================

#define MIN_VINT8   ((vint8)-128)
#define MIN_VINT16  ((vint16)-32768)
#define MIN_VINT32  ((vint32)-2147483648)

#define MAX_VINT8   ((vint8)0x7f)
#define MAX_VINT16  ((vint16)0x7fff)
#define MAX_VINT32  ((vint32)0x7fffffff)

#define MAX_VUINT8  ((vuint8)0xff)
#define MAX_VUINT16 ((vuint16)0xffff)
#define MAX_VUINT32 ((vuint32)0xffffffff)

//typedef unsigned char  byte;

//#define HAVE_INTTYPES_H

#include <inttypes.h>
//typedef uint8_t   __attribute__((__may_alias__)) ubyte;
typedef int8_t    __attribute__((__may_alias__)) vint8;
typedef uint8_t   __attribute__((__may_alias__)) vuint8;
typedef int16_t   __attribute__((__may_alias__)) vint16;
typedef uint16_t  __attribute__((__may_alias__)) vuint16;
typedef int32_t   __attribute__((__may_alias__)) vint32;
typedef uint32_t  __attribute__((__may_alias__)) vuint32;
typedef int64_t   __attribute__((__may_alias__)) vint64;
typedef uint64_t  __attribute__((__may_alias__)) vuint64;

//static_assert(sizeof(ubyte) == 1, "invalid ubyte");
static_assert(sizeof(vint8) == 1, "invalid vint8");
static_assert(sizeof(vuint8) == 1, "invalid vuint8");
static_assert(sizeof(vint16) == 2, "invalid vint16");
static_assert(sizeof(vuint16) == 2, "invalid vuint16");
static_assert(sizeof(vint32) == 4, "invalid vint32");
static_assert(sizeof(vuint32) == 4, "invalid vuint32");
static_assert(sizeof(vint64) == 8, "invalid vint64");
static_assert(sizeof(vuint64) == 8, "invalid vuint64");


enum ENoInit { E_NoInit };


//==========================================================================
//
//  Standard macros
//
//==========================================================================

// number of elements in an array
#define ARRAY_COUNT(array)  (sizeof(array)/sizeof((array)[0]))


//==========================================================================
//
//  VInterface
//
//==========================================================================

// base class for abstract classes that need virtual destructor
class VInterface {
public:
  virtual ~VInterface ();
};


//==========================================================================
//
//  Basic templates
//
//==========================================================================
template<class T> T Min(T val1, T val2) { return val1 < val2 ? val1 : val2; }
template<class T> T Max(T val1, T val2) { return val1 > val2 ? val1 : val2; }
template<class T> T Clamp(T val, T low, T high) { return val < low ? low : val > high ? high : val; }


//==========================================================================
//
//  Forward declarations
//
//==========================================================================
class VName;
class VMemberBase;
class VStruct;
class VObject;
