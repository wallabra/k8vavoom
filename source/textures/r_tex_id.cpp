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
//**  Copyright (C) 2018-2021 Ketmar Dark
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
#include "r_tex_id.h"
#include "../gamedefs.h"

//#define VV_DEBUG_VTEXTUREID_IO


//==========================================================================
//
//  VTextureID::save
//
//==========================================================================
void VTextureID::save (VStream &strm) const {
  vassert(!strm.IsLoading());
  #ifdef VV_DEBUG_VTEXTUREID_IO
  GCon->Logf(NAME_Debug, "+++ ::: +++ id=%d", id);
  #endif
  // check for empty texture, and use short format
  if (this->id <= 0) {
    vint32 aid = this->id;
    strm << STRM_INDEX(aid);
    return;
  }
  // get texture
  VTexture *tex = GTextureManager.getIgnoreAnim(this->id);
  // is it missing?
  if (!tex) {
    vint32 aid = -1; // special "missing texture" flag
    strm << STRM_INDEX(aid);
    return;
  }
  // is it empty?
  if (tex->Type == TEXTYPE_Null) {
    vint32 aid = 0;
    strm << STRM_INDEX(aid);
    return;
  }
  // id (for fast check)
  vint32 aid = this->id;
  strm << STRM_INDEX(aid);
  // type
  vint32 ttype = tex->Type;
  strm << STRM_INDEX(ttype);
  // short name (note: we cannot write `VName` here)
  VStr txname(tex->Name);
  strm << txname;
  // full name
  VStr fullName = (tex->SourceLump >= 0 ? W_RealLumpName(tex->SourceLump) : VStr::EmptyString);
  strm << fullName;
}


//==========================================================================
//
//  VTextureID::load
//
//==========================================================================
void VTextureID::load (VStream &strm) {
  vassert(strm.IsLoading());
  // id
  strm << STRM_INDEX(this->id);
  #ifdef VV_DEBUG_VTEXTUREID_IO
  GCon->Logf(NAME_Debug, "+++ *** +++ id=%d", this->id);
  #endif
  // empty texture?
  if (this->id == 0) return; // yeah, no more data
  if (this->id < 0) {
    // "missing texture"
    this->id = GTextureManager.DefaultTexture;
    // no more data
    return;
  }
  // type
  vint32 ttype = 0;
  strm << STRM_INDEX(ttype);
  // short name
  VStr txname;
  strm << txname;
  // full name
  VStr fullName;
  strm << fullName;
  #ifdef VV_DEBUG_VTEXTUREID_IO
  GCon->Logf(NAME_Debug, "*** ::: *** id=%d; ttype=%s; short=<%s>; long=<%s>", this->id, VTexture::TexTypeToStr(ttype), *txname, *fullName);
  #endif
  // check if it is right
  VTexture *tx = GTextureManager.getIgnoreAnim(this->id);
  if (tx) {
    VStr txFullName = (tx->SourceLump >= 0 ? W_RealLumpName(tx->SourceLump) : VStr::EmptyString);
    // try to compare by a full name
    if (!fullName.isEmpty()) {
      if (fullName.strEqu(txFullName)) {
        #ifdef VV_DEBUG_VTEXTUREID_IO
        GCon->Logf(NAME_Debug, "TEXLOAD: hit with full name '%s'", *fullName);
        #endif
        return;
      }
    } else if (!txname.isEmpty()) {
      // by a short name
      if (tx->Type == ttype && txname.strEqu(*tx->Name)) {
        #ifdef VV_DEBUG_VTEXTUREID_IO
        GCon->Logf(NAME_Debug, "TEXLOAD: hit with short name '%s'", *txname);
        #endif
        return;
      }
    } else {
      GCon->Log(NAME_Warning, "TEXLOAD: no texture full name, no texture short name; replaced with checkerboard (0)");
      this->id = GTextureManager.DefaultTexture;
      return;
    }
  }
  // either no such texture, or name mismatch
  // try to load map texture with long name
  if (!fullName.isEmpty()) {
    this->id = GTextureManager.FindOrLoadFullyNamedTextureAsMapTexture(fullName, nullptr, ttype, true/*bOverload*/, false/*silent*/);
    if (this->id < 0) this->id = 0; else GCon->Logf(NAME_Debug, "TEXLOAD: replaced with full name '%s'", *fullName);
  } else if (!txname.isEmpty()) {
    auto lock = GTextureManager.LockMapLocalTextures();
    this->id = GTextureManager.CheckNumForNameAndForce(VName(*txname), ttype, true, false);
    if (this->id < 0) {
      GCon->Logf(NAME_Warning, "TEXLOAD: cannot find texture with by the short name '%s' (%s)", *txname, VTexture::TexTypeToStr(ttype));
      this->id = GTextureManager.DefaultTexture;
    } else {
      #ifdef VV_DEBUG_VTEXTUREID_IO
      GCon->Logf(NAME_Debug, "TEXLOAD: replaced with short name '%s'", *txname);
      #endif
    }
  } else {
    GCon->Log(NAME_Warning, "TEXLOAD: no texture full name, no texture short name; replaced with checkerboard (1)");
    this->id = GTextureManager.DefaultTexture;
  }
}


//==========================================================================
//
//  VTextureID::Serialise
//
//==========================================================================
void VTextureID::Serialise (VStream &strm) const {
  save(strm);
}


//==========================================================================
//
//  VTextureID::Serialise
//
//==========================================================================
void VTextureID::Serialise (VStream &strm) {
  if (strm.IsLoading()) load(strm); else save(strm);
}
