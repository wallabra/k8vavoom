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
#include "core.h"


// fuck you, c standard!
#ifdef VAVOOM_LITTLE_ENDIAN

vint16 LittleShort (vint16 x) { return x; }
vint32 LittleLong (vint32 x) { return x; }
float LittleFloat (float x) { return x; }

vint16 BigShort (vint16 x) { return ((vuint16)x>>8)|((vuint16)x<<8); }
vint32 BigLong (vint32 x) { return ((vuint32)x>>24)|(((vuint32)x>>8)&0xff00U)|(((vuint32)x<<8)&0xff0000U)|((vuint32)x<<24); }
float BigFloat (float x) { union { float f; vint32 l; } a; a.f = x; a.l = BigLong(a.l); return a.f; }

#else

vint16 BigShort (vint16 x) { return x; }
vint32 BigLong (vint32 x) { return x; }
float BigFloat (float x) { return x; }

vint16 LittleShort (vint16 x) { return ((vuint16)x>>8)|((vuint16)x<<8); }
vint32 LittleLong (vint32 x) { return ((vuint32)x>>24)|(((vuint32)x>>8)&0xff00U)|(((vuint32)x<<8)&0xff0000U)|((vuint32)x<<24); }
float LittleFloat (float x) { union { float f; vint32 l; } a; a.f = x; a.l = LittleLong(a.l); return a.f; }

#endif
