//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2019 Ketmar Dark
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
//**
//**  extended NTValue-based I/O
//**
//**************************************************************************
#include "gamedefs.h"


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================

void VNTValueIOEx::io (VName vname, VTextureID &v) {
  if (IsError()) {
    if (IsLoading()) v.id = 0;
    return;
  }

  if (IsLoading()) {
    // reading
    VName tname;
    io(vname, tname);
    if (IsError()) {
      if (IsLoading()) v.id = 0;
      return;
    }
    if (tname == NAME_None) {
      //GCon->Log(NAME_Warning, "LOAD: save file is probably broken (empty texture name)");
      v.id = -1;
    } else {
      auto lock = GTextureManager.LockMapLocalTextures();
      int texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Wall, true, true, false);
      if (texid < 0) texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Flat, true, true, false);
      if (texid < 0) texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Sprite, true, true, false);
      if (texid < 0) {
        GCon->Logf(NAME_Warning, "LOAD: save file is broken (texture '%s' not found)", *tname);
        texid = GTextureManager.DefaultTexture;
      }
      v.id = texid;
    }
  } else {
    // writing
    VName tname = NAME_None;
    if (v.id >= 0) {
      if (!GTextureManager.getIgnoreAnim(v.id)) {
        GCon->Logf(NAME_Warning, "SAVE: trying to save inexisting texture with id #%d", v.id);
        tname = VName(" ! ");
      } else {
        tname = GTextureManager.GetTextureName(v.id);
      }
    }
    io(vname, tname);
  }
}
