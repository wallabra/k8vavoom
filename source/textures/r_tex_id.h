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
#ifndef VAVOOM_TEX_ID_HEADER
#define VAVOOM_TEX_ID_HEADER

#include "../../libs/core/core.h"
#include "../common.h"


// ////////////////////////////////////////////////////////////////////////// //
struct VTextureID {
public:
  vint32 id;
  VTextureID () : id (-1) {}
  VTextureID (const VTextureID &b) : id(b.id) {}
  // temp
  VTextureID (vint32 aid) : id(aid) {}

  inline VTextureID &operator = (const VTextureID &b) { id = b.id; return *this; }
  inline operator vint32 () const { return id; }

  void Serialise (VStream &strm) const;
  void Serialise (VStream &strm);
};

static_assert(sizeof(VTextureID) == sizeof(vint32), "invalid VTextureID size");

static inline __attribute__((unused)) VStream &operator << (VStream &strm, const VTextureID &tid) { tid.Serialise(strm); return strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &strm, VTextureID &tid) { tid.Serialise(strm); return strm; }


#endif
