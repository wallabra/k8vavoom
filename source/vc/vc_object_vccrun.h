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
  DECLARE_FUNCTION(get_GC_ImmediateDelete)
  DECLARE_FUNCTION(set_GC_ImmediateDelete)

  // implemented in main vccrun source
  DECLARE_FUNCTION(CreateTimer)
  DECLARE_FUNCTION(CreateTimerWithId)
  DECLARE_FUNCTION(DeleteTimer)
  DECLARE_FUNCTION(IsTimerExists)
  DECLARE_FUNCTION(IsTimerOneShot)
  DECLARE_FUNCTION(GetTimerInterval)
  DECLARE_FUNCTION(SetTimerInterval)
  DECLARE_FUNCTION(GetTickCount)

  DECLARE_FUNCTION(fsysAppendDir)
  DECLARE_FUNCTION(fsysAppendPak)
  DECLARE_FUNCTION(fsysRemovePak)
  DECLARE_FUNCTION(fsysRemovePaksFrom)
  DECLARE_FUNCTION(fsysFindPakByPrefix)
  DECLARE_FUNCTION(fsysFileExists)
  DECLARE_FUNCTION(fsysFileFindAnyExt)
  DECLARE_FUNCTION(fsysGetPakPath)
  DECLARE_FUNCTION(fsysGetPakPrefix)
  DECLARE_FUNCTION(fsysGetLastPakId)

  DECLARE_FUNCTION(get_fsysKillCommonZipPrefix)
  DECLARE_FUNCTION(set_fsysKillCommonZipPrefix)

  DECLARE_FUNCTION(appSetName)
  DECLARE_FUNCTION(appSaveOptions)
  DECLARE_FUNCTION(appLoadOptions)

  DECLARE_FUNCTION(ccmdClearText)
  DECLARE_FUNCTION(ccmdClearCommand)
  DECLARE_FUNCTION(ccmdParseOne)
  DECLARE_FUNCTION(ccmdGetArgc)
  DECLARE_FUNCTION(ccmdGetArgv)
  DECLARE_FUNCTION(ccmdTextSize)
  DECLARE_FUNCTION(ccmdPrepend)
  DECLARE_FUNCTION(ccmdPrependQuoted)
  DECLARE_FUNCTION(ccmdAppend)
  DECLARE_FUNCTION(ccmdAppendQuoted)

  DECLARE_FUNCTION(SocketConnectUDP)
  DECLARE_FUNCTION(SocketConnectTCP)
  DECLARE_FUNCTION(SocketDisconnect)

  DECLARE_FUNCTION(SocketGetIOCTL)
  DECLARE_FUNCTION(SocketSetIOCTL)

  DECLARE_FUNCTION(SocketSendStr)
  DECLARE_FUNCTION(SocketSendBuf)

  DECLARE_FUNCTION(SocketRecvStr)
  DECLARE_FUNCTION(SocketRecvBuf)
