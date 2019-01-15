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
#include "r_tex_id.h"
#include "gamedefs.h"


//==========================================================================
//
//  << (VTextureID)
//
//==========================================================================
VStream &operator << (VStream &strm, const VTextureID &tid) {
  check(!strm.IsLoading());
  vint32 aid = tid.id;
  if (aid > 0 && GTextureManager.getIgnoreAnim(aid)) {
    strm << STRM_INDEX(aid);
    // name
    VName txname = GTextureManager.GetTextureName(aid);
    strm << txname;
  } else {
    if (aid > 0) aid = 0;
    strm << STRM_INDEX(aid);
  }
  return strm;
}


//==========================================================================
//
//  << (VTextureID)
//
//==========================================================================
VStream &operator << (VStream &strm, VTextureID &tid) {
  if (strm.IsLoading()) {
    // loading
    strm << STRM_INDEX(tid.id);
    if (tid.id > 0) {
      // name
      VName txname = NAME_None;
      strm << txname;
      if (txname == NAME_None) {
        //Host_Error("save file is broken (empty texture)");
        GCon->Logf(NAME_Warning, "LOAD: save file is broken (empty texture with id %d)", tid.id);
        //R_DumpTextures();
        //abort();
        tid.id = GTextureManager.DefaultTexture;
      } else if (txname != GTextureManager.GetTextureName(tid.id)) {
        // try to fix texture
        auto lock = GTextureManager.LockMapLocalTextures();
        int texid = GTextureManager.CheckNumForNameAndForce(txname, TEXTYPE_Wall, true, true, false);
        if (texid <= 0) texid = GTextureManager.DefaultTexture;
        if (developer) GCon->Logf("LOAD: REPLACED texture '%s' (id %d) with '%s' (id %d)", *txname, tid.id, *GTextureManager.GetTextureName(texid), texid);
        tid.id = texid;
      } else {
        if (developer) GCon->Logf("LOAD: HIT texture '%s' (id %d)", *txname, tid.id);
      }
    }
  } else {
    // writing
    //bool errDump = false;
    vint32 aid = tid.id;
    if (aid > 0 && GTextureManager.getIgnoreAnim(aid)) {
      strm << STRM_INDEX(aid);
      // name
      VName txname = GTextureManager.GetTextureName(aid);
      strm << txname;
      if (txname == NAME_None) {
        GCon->Logf(NAME_Warning, "SAVE: trying to save empty texture with id #%d", aid);
        R_DumpTextures();
        //errDump = true;
        abort();
      }
    } else {
      if (aid > 0) {
        GCon->Logf(NAME_Warning, "SAVE: trying to save inexisting texture with id #%d", aid);
        aid = 0;
        R_DumpTextures();
        //errDump = true;
        abort();
      }
      strm << STRM_INDEX(aid);
    }
  }
  return strm;
}
