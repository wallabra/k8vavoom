//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
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
#if defined(__SWITCH__)
// we know this for sure
# define VAVOOM_LITTLE_ENDIAN
enum { GBigEndian = 0 };
#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN || \
    defined(__BIG_ENDIAN__) || \
    defined(__ARMEB__) || \
    defined(__THUMBEB__) || \
    defined(__AARCH64EB__) || \
    defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__)
# define VAVOOM_BIG_ENDIAN
enum { GBigEndian = 1 };
#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN || \
    defined(__LITTLE_ENDIAN__) || \
    defined(__ARMEL__) || \
    defined(__THUMBEL__) || \
    defined(__AARCH64EL__) || \
    defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__) || \
    defined(_WIN32)
# define VAVOOM_LITTLE_ENDIAN
enum { GBigEndian = 0 };
#else
# error "I don't know what architecture this is!"
#endif

// endianess handling

#ifdef VAVOOM_HIDE_ENDIANNES_CONVERSIONS
// fuck you, c standard!
// hide this from shitptimiser
vint16 LittleShort (vint16 x);
vint32 LittleLong (vint32 x);
float LittleFloat (float x);

vint16 BigShort (vint16 x);
vint32 BigLong (vint32 x);
float BigFloat (float x);

#else

#ifdef VAVOOM_LITTLE_ENDIAN

static __attribute__((unused)) inline vint16 LittleShort (vint16 x) { return x; }
static __attribute__((unused)) inline vint32 LittleLong (vint32 x) { return x; }
static __attribute__((unused)) inline float LittleFloat (float x) { return x; }

static __attribute__((unused)) inline vint16 BigShort (vint16 x) { return ((vuint16)x>>8)|((vuint16)x<<8); }
static __attribute__((unused)) inline vint32 BigLong (vint32 x) { return ((vuint32)x>>24)|(((vuint32)x>>8)&0xff00U)|(((vuint32)x<<8)&0xff0000U)|((vuint32)x<<24); }
static __attribute__((unused)) inline float BigFloat (float x) { union { float f; vint32 l; } a; a.f = x; a.l = BigLong(a.l); return a.f; }

#else

static __attribute__((unused)) inline vint16 BigShort (vint16 x) { return x; }
static __attribute__((unused)) inline vint32 BigLong (vint32 x) { return x; }
static __attribute__((unused)) inline float BigFloat (float x) { return x; }

static __attribute__((unused)) inline vint16 LittleShort (vint16 x) { return ((vuint16)x>>8)|((vuint16)x<<8); }
static __attribute__((unused)) inline vint32 LittleLong (vint32 x) { return ((vuint32)x>>24)|(((vuint32)x>>8)&0xff00U)|(((vuint32)x<<8)&0xff0000U)|((vuint32)x<<24); }
static __attribute__((unused)) inline float LittleFloat (float x) { union { float f; vint32 l; } a; a.f = x; a.l = LittleLong(a.l); return a.f; }

#endif

#endif // VAVOOM_HIDE_ENDIANNES_CONVERSIONS
