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
#include "net_message.h"


//==========================================================================
//
//  VControlChannel::VControlChannel
//
//==========================================================================
VControlChannel::VControlChannel (VNetConnection *AConnection, vint32 AIndex, vuint8 AOpenedLocally)
  : VChannel(AConnection, CHANNEL_Control, AIndex, AOpenedLocally)
{
  OpenAcked = true; // this channel is pre-opened
}


//==========================================================================
//
//  VControlChannel::GetName
//
//==========================================================================
VStr VControlChannel::GetName () const noexcept {
  return VStr(va("ctlchan #%d(%s)", Index, GetTypeName()));
}


//==========================================================================
//
//  VControlChannel::ParseMessage
//
//==========================================================================
void VControlChannel::ParseMessage (VMessageIn &msg) {
  while (!msg.AtEnd()) {
    VStr Cmd;
    msg << Cmd;
    if (msg.IsError()) break;
    if (Connection->Context->IsClient()) {
      GCmdBuf << Cmd;
    } else {
      VCommand::ExecuteString(Cmd, VCommand::SRC_Client, Connection->Owner);
    }
  }
}
