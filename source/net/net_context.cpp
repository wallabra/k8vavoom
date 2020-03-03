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
  if (!Th) return; // just in case
  if (IsClient()) {
    // client; have connection with a server
    VThinkerChannel *chan = ServerConnection->ThinkerChannels.FindPtr(Th);
    if (chan) chan->Close();
    // remove from detached list (just in case)
    ServerConnection->DetachedThinkers.remove(Th);
  } else {
    for (auto &&it : ClientConnections) {
      VThinkerChannel *chan = it->ThinkerChannels.FindPtr(Th);
      if (chan) chan->Close();
      if (it->DetachedThinkers.has(Th)) GCon->Logf(NAME_Debug, "%s:%u: removed from detached list", Th->GetClass()->GetName(), Th->GetUniqueId());
      // remove from detached list
      it->DetachedThinkers.remove(Th);
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
  for (int i = 0; i < ClientConnections.length(); ++i) {
    VNetConnection *Conn = ClientConnections[i];
    if (!Conn) continue; // just in case
    //!GCon->Logf(NAME_DevNet, "#%d: %s (oms:%d)", i, *Conn->GetAddress(), (int)Conn->ObjMapSent);
    // don't update level if the player isn't totally in the game yet
    if (Conn->State != NETCON_Closed && Conn->GetGeneralChannel() && (Conn->Owner->PlayerFlags&VBasePlayer::PF_Spawned)) {
      //!GCon->Logf(NAME_DevNet, "  spawned: #%d: %s", i, *Conn->GetAddress());
      if (Conn->NeedsUpdate) {
        // reset update flag; it will be set again if we'll get any packet from the client
        //!GCon->Logf(NAME_DevNet, "  sending level update: #%d: %s", i, *Conn->GetAddress());
        //Conn->NeedsUpdate = false; // nope, this is set in `UpdateLevel()`
        Conn->UpdateLevel();
      }
      if (Conn->State != NETCON_Closed) {
        //!GCon->Logf(NAME_DevNet, "  sending player update: #%d: %s", i, *Conn->GetAddress());
        Conn->GetPlayerChannel()->Update();
      }
    }
    if (Conn->State != NETCON_Closed && Conn->ObjMapSent && !Conn->LevelInfoSent) {
      GCon->Logf(NAME_DevNet, "Sending server info for %s", *Conn->GetAddress());
      Conn->SendServerInfo();
    }
    /*
    if (Conn->State != NETCON_Closed) {
      //!GCon->Logf(NAME_DevNet, "  checking messages: #%d: %s", i, *Conn->GetAddress());
      Conn->GetMessages(); // why not?
    }
    */
    if (Conn->State != NETCON_Closed) {
      //!GCon->Logf(NAME_DevNet, "  ticking: #%d: %s", i, *Conn->GetAddress());
      Conn->Tick();
    }
    if (Conn->State != NETCON_Closed && Conn->GetGeneralChannel()->Closing) {
      GCon->Logf(NAME_DevNet, "Client %s closed the connection", *Conn->GetAddress());
      Conn->State = NETCON_Closed;
    }
    if (Conn->State == NETCON_Closed) {
      GCon->Logf(NAME_DevNet, "Dropping client %s", *Conn->GetAddress());
      SV_DropClient(Conn->Owner, true);
    }
  }
}


//==========================================================================
//
//  VNetContext::KeepaliveTick
//
//==========================================================================
void VNetContext::KeepaliveTick () {
  for (int i = 0; i < ClientConnections.Num(); ++i) {
    VNetConnection *Conn = ClientConnections[i];
    if (!Conn) continue; // just in case
    if (Conn->State != NETCON_Closed) Conn->KeepaliveTick();
  }
}
