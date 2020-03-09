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

extern VCvarB net_dbg_dump_thinker_detach; // from net_channel_thinker.cpp, sorry


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
    if (chan) {
      chan->SetThinker(nullptr); // notify channel that the thinker is already dead
      chan->Close();
    }
    // remove from detached list (just in case)
    ServerConnection->DetachedThinkers.remove(Th);
  } else {
    // server; remove thinker from all clients
    for (auto &&it : ClientConnections) {
      VThinkerChannel *chan = it->ThinkerChannels.FindPtr(Th);
      if (chan) {
        chan->SetThinker(nullptr); // notify channel that the thinker is already dead
        chan->Close();
      }
      if (net_dbg_dump_thinker_detach) {
        if (it->DetachedThinkers.has(Th)) GCon->Logf(NAME_Debug, "%s:%u: removed from detached list", Th->GetClass()->GetName(), Th->GetUniqueId());
      }
      // remove from detached list
      it->DetachedThinkers.remove(Th);
    }
  }
}


//==========================================================================
//
//  VNetContext::Tick
//
//  this is called directly from the server code
//
//==========================================================================
void VNetContext::Tick () {
  for (int i = 0; i < ClientConnections.length(); ++i) {
    VNetConnection *Conn = ClientConnections[i];
    if (!Conn) continue; // just in case
    if (Conn->IsOpen() && !Conn->GetGeneralChannel()) {
      GCon->Logf(NAME_DevNet, "Client %s closed the connection", *Conn->GetAddress());
      Conn->State = NETCON_Closed;
    }
    if (Conn->IsOpen()) {
      vassert(Conn->GetGeneralChannel());
      // send server info, if necessary
      if (Conn->ObjMapSent && !Conn->LevelInfoSent) {
        GCon->Logf(NAME_DevNet, "Sending server info for %s", *Conn->GetAddress());
        Conn->SendServerInfo();
      }
      // don't update level if the player isn't totally in the game yet
      if (Conn->IsOpen() && Conn->Owner->PlayerFlags&VBasePlayer::PF_Spawned) {
        // spam client with player updates
        Conn->GetPlayerChannel()->Update();
        if (Conn->IsOpen() && Conn->NeedsUpdate) Conn->UpdateLevel(); // `UpdateLevel()` will reset update flag
      }
      // tick the channel if it is still open
      if (Conn->IsOpen()) Conn->Tick();
    }
    if (Conn->IsClosed()) {
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
    if (Conn->IsOpen() && Conn->GetGeneralChannel()->Closing) {
      GCon->Logf(NAME_DevNet, "Client %s closed the connection", *Conn->GetAddress());
      Conn->State = NETCON_Closed;
    }
    if (Conn->IsOpen()) Conn->KeepaliveTick();
  }
}
