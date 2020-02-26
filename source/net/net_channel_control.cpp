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


//==========================================================================
//
//  VControlChannel::VControlChannel
//
//==========================================================================
VControlChannel::VControlChannel (VNetConnection *AConnection, vint32 AIndex, vuint8 AOpenedLocally)
  : VChannel(AConnection, CHANNEL_Control, AIndex, AOpenedLocally)
{
}


//==========================================================================
//
//  VControlChannel::Suicide
//
//==========================================================================
void VControlChannel::Suicide () {
  #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
  GCon->Logf(NAME_Debug, "VControlChannel::Suicide:%p (#%d)", this, Index);
  #endif
  VChannel::Suicide();
  Closing = true; // just in case
  ClearAllQueues();
  if (Index >= 0 && Index < MAX_CHANNELS && Connection) {
    Connection->UnregisterChannel(this);
    Index = -1; // just in case
  }
}


//==========================================================================
//
//  VControlChannel::ParsePacket
//
//==========================================================================
void VControlChannel::ParsePacket (VMessageIn &msg) {
  while (!msg.AtEnd()) {
    VStr Cmd;
    msg << Cmd;
    if (msg.IsError()) break;
    if (Connection->Context->ServerConnection) {
      GCmdBuf << Cmd;
    } else {
      VCommand::ExecuteString(Cmd, VCommand::SRC_Client, Connection->Owner);
    }
  }
}
