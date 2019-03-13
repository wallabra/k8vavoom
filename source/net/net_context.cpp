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
#include "gamedefs.h"
#include "network.h"
#include "sv_local.h"


//==========================================================================
//
//  VNetContext::VNetContext
//
//==========================================================================
VNetContext::VNetContext ()
  : RoleField(nullptr)
  , RemoteRoleField(nullptr)
  , ServerConnection(nullptr)
{
  RoleField = VThinker::StaticClass()->FindFieldChecked("Role");
  RemoteRoleField = VThinker::StaticClass()->FindFieldChecked("RemoteRole");
}


//==========================================================================
//
//  VNetContext::~VNetContext
//
//==========================================================================
VNetContext::~VNetContext () {
}


//==========================================================================
//
//  VNetContext::ThinkerDestroyed
//
//==========================================================================
void VNetContext::ThinkerDestroyed (VThinker *Th) {
  //GCon->Logf("NET:%p: destroyed thinker with class `%s`", Th, Th->GetClass()->GetName());
  if (ServerConnection) {
    VThinkerChannel *Chan = ServerConnection->ThinkerChannels.FindPtr(Th);
    if (Chan) Chan->Close();
  } else {
    for (int i = 0; i < ClientConnections.Num(); ++i) {
      VThinkerChannel *Chan = ClientConnections[i]->ThinkerChannels.FindPtr(Th);
      if (Chan) Chan->Close();
    }
  }
}


//==========================================================================
//
//  VNetContext::Tick
//
//==========================================================================
void VNetContext::Tick () {
  for (int i = 0; i < ClientConnections.Num(); ++i) {
    VNetConnection *Conn = ClientConnections[i];
    // don't update level if the player isn't totally in the game yet
    if (Conn->Channels[CHANIDX_General] && (Conn->Owner->PlayerFlags&VBasePlayer::PF_Spawned)) {
      if (Conn->NeedsUpdate) Conn->UpdateLevel();
      ((VPlayerChannel *)Conn->Channels[CHANIDX_Player])->Update();
    }
    if (Conn->ObjMapSent && !Conn->LevelInfoSent) Conn->SendServerInfo();
    Conn->Tick();
    if (Conn->State == NETCON_Closed) SV_DropClient(Conn->Owner, true);
  }
}
