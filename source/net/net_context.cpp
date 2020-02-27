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
  if (IsClient()) {
    // client; have connection with a server
    VThinkerChannel *chan = ServerConnection->ThinkerChannels.FindPtr(Th);
    if (chan) {
      #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
      GCon->Logf(NAME_Debug, "NET:CLIENT:%p: destroying thinker with class `%s`; chan %p: #%d", Th, Th->GetClass()->GetName(), chan, chan->Index);
      #endif
      chan->Close();
      #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
      GCon->Logf(NAME_Debug, "NET:CLIENT:%p: CLOSED thinker with class `%s`; chan %p: #%d", Th, Th->GetClass()->GetName(), chan, chan->Index);
      #endif
    }
  } else {
    for (auto &&it : ClientConnections) {
      VThinkerChannel *chan = it->ThinkerChannels.FindPtr(Th);
      if (chan) {
        #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
        GCon->Logf(NAME_Debug, "NET:SERVER:%p: destroyed thinker with class `%s`; chan %p: #%d", Th, Th->GetClass()->GetName(), chan, chan->Index);
        #endif
        chan->Close();
      }
    }
  }
}


//==========================================================================
//
//  VNetContext::Tick
//
//  this is directly called from server code
//
//==========================================================================
void VNetContext::Tick () {
  for (int i = 0; i < ClientConnections.Num(); ++i) {
    VNetConnection *Conn = ClientConnections[i];
    if (!Conn) continue; // just in case
    if (Conn->State != NETCON_Closed) {
      // don't update level if the player isn't totally in the game yet
      if (Conn->GetGeneralChannel() && (Conn->Owner->PlayerFlags&VBasePlayer::PF_Spawned)) {
        if (Conn->NeedsUpdate) {
          // reset update flag; it will be set again if we'll get any packet from the client
          Conn->NeedsUpdate = false;
          Conn->UpdateLevel();
        }
        Conn->GetPlayerChannel()->Update();
      }
      if (Conn->ObjMapSent && !Conn->LevelInfoSent) Conn->SendServerInfo();
      Conn->GetMessages(); // why not?
      Conn->Tick();
    }
    if (Conn->State == NETCON_Closed) {
      GCon->Logf(NAME_DevNet, "Dropping client %s", *Conn->GetAddress());
      SV_DropClient(Conn->Owner, true);
    }
  }
}
