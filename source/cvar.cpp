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
#include "gamedefs.h"
#include "net/network.h"
#include "sv_local.h"
#include "cvar.h"


//==========================================================================
//
//  cv_userInfoSet
//
//==========================================================================
static void cv_userInfoSet (VCvar *cvar) {
#ifdef CLIENT
  Info_SetValueForKey(cls.userinfo, cvar->GetName(), *cvar->asStr());
  if (cl) {
    if (GGameInfo->NetMode == NM_TitleMap ||
        GGameInfo->NetMode == NM_Standalone ||
        GGameInfo->NetMode == NM_ListenServer)
    {
      VCommand::ExecuteString(VStr("setinfo \"")+cvar->GetName()+"\" \""+cvar->asStr()+"\"\n", VCommand::SRC_Client, cl);
    } else if (cl->Net) {
      cl->Net->SendCommand(VStr("setinfo \"")+cvar->GetName()+"\" \""+cvar->asStr()+"\"\n");
    }
  }
#endif
}


//==========================================================================
//
//  cv_serverInfoSet
//
//==========================================================================
static void cv_serverInfoSet (VCvar *cvar) {
#ifdef SERVER
  Info_SetValueForKey(svs.serverinfo, cvar->GetName(), *cvar->asStr());
  if (GGameInfo && GGameInfo->NetMode != NM_None && GGameInfo->NetMode != NM_Client) {
    for (int i = 0; i < MAXPLAYERS; ++i) {
      if (GGameInfo->Players[i]) {
        GGameInfo->Players[i]->eventClientSetServerInfo(cvar->GetName(), cvar->asStr());
      }
    }
  }
#endif
}


// ////////////////////////////////////////////////////////////////////////// //
//static TMapNC<VCvar *, CvarChangeHandler> chMap;
//static TMapNC<VStr, CvarChangeHandler> chMapPending;
static TMapNC<VCvar *, bool> acAdded; // already added to autocompletion list?


//==========================================================================
//
//  cv_created
//
//==========================================================================
static void cv_created (VCvar *cvar) {
  if (!acAdded.find(cvar)) {
    acAdded.put(cvar, true);
    VCommand::AddToAutoComplete(cvar->GetName());
  }
}


//==========================================================================
//
//  Cvars_Init
//
//==========================================================================
void Cvars_Init () {
  VCvar::Init();
  VCvar::AddAllVarsToAutocomplete(&VCommand::AddToAutoComplete);
  VCvar::CreatedCB = &cv_created; // this adds to autocompletion
  //VCvar::ChangedCB = &cv_changed;
  VCvar::UserInfoSetCB = &cv_userInfoSet;
  VCvar::ServerInfoSetCB = &cv_serverInfoSet;
  VCvar::SendAllUserInfos();
  VCvar::SendAllServerInfos();
}


// ////////////////////////////////////////////////////////////////////////// //
// COMMAND CvarList
//
// This is slightly more complicated, as we want nicely sorted list.
// It can be fairly slow, we don't care.
COMMAND(CvarList) {
  vuint32 count = VCvar::countCVars();
  VCvar **list = VCvar::getSortedList();
  bool showValues = (Args.length() > 1 && (Args[1].ICmp("values") == 0 || Args[1].ICmp("value") == 0));
  for (vuint32 n = 0; n < count; ++n) {
    VCvar *cvar = list[n];
    if (showValues) {
      GCon->Logf("%s = \"%s\"", cvar->GetName(), *VStr(cvar->asStr()).quote());
    } else {
      GCon->Logf("%s: %s", cvar->GetName(), cvar->GetHelp());
    }
  }
  GCon->Logf("%u variables.", count);
  delete[] list;
}


// ////////////////////////////////////////////////////////////////////////// //
// COMMAND whatis
//
// Show short description for a cvar.
COMMAND(WhatIs) {
  if (Args.Num() != 2) {
    GCon->Logf("Show short cvar description.");
    GCon->Logf("usage: whatis varname");
  } else {
    VCvar *cvar = VCvar::FindVariable(*(Args[1]));
    if (cvar) {
      GCon->Logf("%s: %s", cvar->GetName(), cvar->GetHelp());
    } else {
      GCon->Logf("Unknown cvar: '%s'", *(Args[1]));
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// COMMAND toggle
//
// Toggles boolean cvar
COMMAND(Toggle) {
  if (Args.Num() != 2) {
    GCon->Logf("Toggles boolean cvar.");
    GCon->Logf("usage: toggle varname");
  } else {
    VCvar *cvar = VCvar::FindVariable(*(Args[1]));
    if (cvar) {
      cvar->Set(cvar->asBool() ? 0 : 1);
    } else {
      GCon->Logf("Unknown cvar: '%s'", *(Args[1]));
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// COMMAND cvarinfovar
//
// create temp variable from user mod
COMMAND(CvarInfoVar) {
  if (Args.Num() < 3) {
    GCon->Logf("usage: cvarinfovar [flags-and-type] varname varvalue");
  } else {
    VStr vname = Args[Args.length()-2];
    VStr vvalue = Args[Args.length()-1];
    if (vname.length() == 0) return;
    VCvar *cvar = VCvar::FindVariable(*vname);
    if (cvar) {
      // just set value
      cvar->Set(vvalue);
      return;
    }
    int flags = CVAR_FromMod|CVAR_Archive|CVAR_AlwaysArchive;
    bool isint = false;
    bool isfloat = false;
    bool isstr = false;
    bool isbool = false;
    const char *cvtname = "int";
    for (int f = 1; f < Args.length()-2; ++f) {
           if (Args[f].ICmp("noarchive") == 0) flags &= ~(CVAR_Archive|CVAR_AlwaysArchive);
      else if (Args[f].ICmp("cheat") == 0) flags |= CVAR_Cheat;
      else if (Args[f].ICmp("latch") == 0) flags |= CVAR_Latch;
      else if (Args[f].ICmp("server") == 0) flags |= CVAR_ServerInfo;
      else if (Args[f].ICmp("user") == 0) {}
      else if (Args[f].strEquCI("int")) { cvtname = "int"; isint = true; }
      else if (Args[f].strEquCI("float")) { cvtname = "float"; isfloat = true; }
      else if (Args[f].strEquCI("bool")) { cvtname = "bool"; isbool = true; }
      else if (Args[f].strEquCI("string")) { cvtname = "string"; isstr = true; }
      else { GCon->Logf("invalid cvarinfo flag '%s'", *Args[f]); return; }
    }
    if (!isint && !isfloat && !isstr && !isbool) isint = true;
    VCvar *cv = VCvar::CreateNew(*vname, vvalue, va("cvarinfo %s variable", cvtname), flags);
    if (cv) {
           if (isstr) cv->SetType(VCvar::String);
      else if (isbool) cv->SetType(VCvar::Bool);
      else if (isfloat) cv->SetType(VCvar::Float);
      else if (isint) cv->SetType(VCvar::Int);
    }
  }
}
