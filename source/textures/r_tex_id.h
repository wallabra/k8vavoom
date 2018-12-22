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
  inline operator int () const { return id; }
  friend VStream &operator << (VStream &strm, const VTextureID &tid);
  friend VStream &operator << (VStream &strm, VTextureID &tid);
};

static_assert(sizeof(VTextureID) == sizeof(vint32), "invalid VTextureID size");


#endif
