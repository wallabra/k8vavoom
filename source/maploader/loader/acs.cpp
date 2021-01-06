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
#include "../../gamedefs.h"
#include "../../server/sv_local.h"


//==========================================================================
//
//  VLevel::LoadLoadACS
//
//  load libraries from 'loadacs'
//
//==========================================================================
void VLevel::LoadLoadACS (int lacsLump, int XMapLump) {
  if (lacsLump < 0) return;
  GCon->Logf("Loading ACS libraries from '%s'", *W_FullLumpName(lacsLump));
  VScriptParser *sc = new VScriptParser(W_FullLumpName(lacsLump), W_CreateLumpReaderNum(lacsLump));
  while (!sc->AtEnd()) {
    sc->ExpectString();
    int AcsLump = W_FindACSObjectInFile(sc->String, W_LumpFile(lacsLump));
    if (AcsLump >= 0) {
      GCon->Logf("  loading ACS script from '%s'", *W_FullLumpName(AcsLump));
      Acs->LoadObject(AcsLump);
    } else {
      GCon->Logf(NAME_Warning, "ACS script '%s' not found", *sc->String);
    }
  }
  delete sc;
}


//==========================================================================
//
//  VLevel::LoadACScripts
//
//==========================================================================
void VLevel::LoadACScripts (int Lump, int XMapLump) {
  Acs = new VAcsLevel(this);

  if (developer) GCon->Logf(NAME_Dev, "ACS: BEHAVIOR lump: %d", Lump);

  // load level's BEHAVIOR lump if it has one
  if (Lump >= 0 && W_LumpLength(Lump) > 0) {
    GCon->Logf("found map behavior lump (%s)", *W_FullLumpName(Lump));
    Acs->LoadObject(Lump);
    //GCon->Logf("done loading map behavior lump (%s)", *W_FullLumpName(Lump));
  }

  // load ACS helper scripts if needed (for Strife)
  if (GGameInfo->AcsHelper != NAME_None) {
    GCon->Logf(NAME_Dev, "ACS: looking for helper script from '%s'", *GGameInfo->AcsHelper);
    int hlump = W_GetNumForName(GGameInfo->AcsHelper, WADNS_ACSLibrary);
    if (hlump >= 0) {
      GCon->Logf(NAME_Dev, "ACS: loading helper script from '%s'", *W_FullLumpName(hlump));
      Acs->LoadObject(hlump);
    }
  }

  // load user-specified default ACS libraries
  // first load all from map file and further, then all before map file
  // this is done so autoloaded acs won't interfere with pwad libraries
  if (XMapLump >= 0) {
    //GCon->Log("scanning for behavior scritps...");
    // from map file and further
    for (int ScLump = W_StartIterationFromLumpFileNS(W_LumpFile(XMapLump), WADNS_Global); ScLump >= 0; ScLump = W_IterateNS(ScLump, WADNS_Global)) {
      if (W_LumpName(ScLump) != NAME_loadacs) continue;
      //GCon->Logf("found behavior script lump (%s)", *W_FullLumpName(ScLump));
      LoadLoadACS(ScLump, XMapLump);
    }
    //GCon->Log("done scanning for behavior scritps...");
  }

  // before map file
  //GCon->Log("scanning for b-behavior scritps...");
  for (int ScLump = W_IterateNS(-1, WADNS_Global); ScLump >= 0; ScLump = W_IterateNS(ScLump, WADNS_Global)) {
    if (XMapLump >= 0 && W_LumpFile(ScLump) >= W_LumpFile(XMapLump)) break;
    if (W_LumpName(ScLump) != NAME_loadacs) continue;
    //GCon->Logf("found b-behavior script lump (%s)", *W_FullLumpName(ScLump));
    LoadLoadACS(ScLump, XMapLump);
  }
  //GCon->Log("done scanning for b-behavior scritps...");
}
