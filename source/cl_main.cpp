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
#include "net/network.h"
#include "cl_local.h"
#include "ui/ui.h"
#include "neoui/neoui.h"
#include "sv_local.h"

#define VAVOOM_DEMO_VERSION  (1)


void CL_SetUpNetClient (VSocketPublic *);
void SV_ConnectClient (VBasePlayer *);
void CL_Clear ();
void CL_ReadFromServerInfo ();


client_static_t cls;
VBasePlayer *cl;
VClientNetContext *ClientNetContext;

VClientGameBase *GClGame;

bool UserInfoSent;

VCvarS cl_name("name", "PLAYER", "Player name.", CVAR_Archive | CVAR_UserInfo);
VCvarI cl_color("color", "0", "Player color.", CVAR_Archive | CVAR_UserInfo);
VCvarI cl_class("class", "0", "Player class.", CVAR_Archive | CVAR_UserInfo);
VCvarS cl_model("model", "", "Player model.", CVAR_Archive | CVAR_UserInfo);

static VCvarB d_attraction_mode("d_attraction_mode", false, "Allow demo playback (won't work with non-k8vavoom demos)?", CVAR_Archive);

IMPLEMENT_CLASS(V, ClientGameBase);

static VName CurrentSongLump;
static double LastKeepAliveTime = 0.0;


//==========================================================================
//
//  CL_Init
//
//==========================================================================
void CL_Init () {
  cl_name.Set(Sys_GetUserName()); // why not?
  cl_name.SetDefault(cl_name.asStr());
  if (developer) GCon->Logf(NAME_Dev, "Default user name is '%s'", *cl_name.asStr());
  VMemberBase::StaticLoadPackage(NAME_cgame, TLocation());
  // load user-specified Vavoom C script files
  G_LoadVCMods("loadvcc", "client");
  //!TLocation::ClearSourceFiles();
  ClientNetContext = new VClientNetContext();
  GClGame = (VClientGameBase *)VObject::StaticSpawnWithReplace(VClass::FindClass("ClientGame"));
  GClGame->Game = GGameInfo;
  GClGame->eventPostSpawn();
  CurrentSongLump = NAME_None;
}


//==========================================================================
//
//  CL_ResetLastSong
//
//==========================================================================
static void CL_ResetLastSong () {
  CurrentSongLump = NAME_None;
}


//==========================================================================
//
//  CL_Ticker
//
//==========================================================================
void CL_Ticker () {
  // do main actions
  switch (GClGame->intermission) {
    case 0:
      SB_Ticker();
      AM_Ticker();
      break;
  }
  R_AnimateSurfaces();
}


//==========================================================================
//
//  CL_Shutdown
//
//==========================================================================
void CL_Shutdown () {
  if (cl) SV_ShutdownGame(); // disconnect
  // free up memory
  if (GClGame) GClGame->ConditionalDestroy();
  if (GRoot) GRoot->ConditionalDestroy();
  cls.userinfo.Clean();
  delete ClientNetContext;
  ClientNetContext = nullptr;
}


//==========================================================================
//
//  CL_DecayLights
//
//==========================================================================
void CL_DecayLights () {
  if (GClLevel && GClLevel->RenderData) GClLevel->RenderData->DecayLights(host_frametime);
}


//==========================================================================
//
//  CL_UpdateMobjs
//
//==========================================================================
void CL_UpdateMobjs () {
  if (!GClLevel) return; //k8: this is wrong!
  for (TThinkerIterator<VThinker> Th(GClLevel); Th; ++Th) {
    Th->eventClientTick(host_frametime);
  }
}


//==========================================================================
//
//  CL_ReadFromServer
//
//  Read all incoming data from the server
//
//==========================================================================
void CL_ReadFromServer () {
  if (!cl) return;

  if (cl->Net) {
    LastKeepAliveTime = Sys_Time();
    cl->Net->GetMessages();
    if (cl->Net->State == NETCON_Closed) Host_EndGame("Server disconnected");
  }

  if (cls.signon) {
    if (GGameInfo->NetMode == NM_Client) {
      GClLevel->Time += host_frametime;
      GClLevel->TicTime = (int)(GClLevel->Time*35.0f);
    }

    CL_UpdateMobjs();
    cl->eventClientTick(host_frametime);
    CL_Ticker();
  }

  if (GClLevel && GClLevel->LevelInfo) {
    if (CurrentSongLump != GClLevel->LevelInfo->SongLump) {
      CurrentSongLump = GClLevel->LevelInfo->SongLump;
      GAudio->MusicChanged();
    }
  }
}


//==========================================================================
//
//  SendKeepaliveInternal
//
//==========================================================================
static void SendKeepaliveInternal (double currTime, bool forced) {
  /*
  if (!GDemoRecordingContext) {
    if (GGameInfo->NetMode != NM_Client) return; // no need if server is local
    //if (cls.demoplayback) return;
    if (!cl->Net) return;
  }
  */
  if (currTime < 0) currTime = 0;
  if (LastKeepAliveTime > currTime) LastKeepAliveTime = currTime; // wtf?!
  if (!forced && currTime-LastKeepAliveTime < 1.0/35.0) return;
  LastKeepAliveTime = currTime;
  // write out a nop
  /*
  if (GDemoRecordingContext) {
    for (int f = 0; f < GDemoRecordingContext->ClientConnections.length(); ++f) {
      GDemoRecordingContext->ClientConnections[f]->Driver->NetTime = 0; //Sys_Time()+1000;
      //GDemoRecordingContext->ClientConnections[f]->Flush();
    }
  }
  */
  if (cl->Net) cl->Net->Flush();
}


//==========================================================================
//
//  CL_KeepaliveMessage
//
//  when the client is taking a long time to load stuff, send keepalive
//  messages so the server doesn't disconnect
//
//==========================================================================
void CL_KeepaliveMessage () {
  //if (!GDemoRecordingContext)
  {
    if (GGameInfo->NetMode != NM_Client) return; // no need if server is local
    if (cls.demoplayback) return;
    if (!cl->Net) return;
  }
  SendKeepaliveInternal(Sys_Time(), false);
  /*
  double currTime = Sys_Time();
  if (currTime-LastKeepAliveTime < 1.0/35.0) return;
  LastKeepAliveTime = currTime;
  // write out a nop
  if (GDemoRecordingContext) {
    for (int f = 0; f < GDemoRecordingContext->ClientConnections.length(); ++f) {
      GDemoRecordingContext->ClientConnections[f]->Flush();
    }
  }
  if (cl->Net) cl->Net->Flush();
  */
}


//==========================================================================
//
//  CL_KeepaliveMessageEx
//
//  pass `Sys_Time()` here
//
//==========================================================================
void CL_KeepaliveMessageEx (double currTime, bool forced) {
  //if (!GDemoRecordingContext)
  {
    if (GGameInfo->NetMode != NM_Client) return; // no need if server is local
    if (cls.demoplayback) return;
    if (!cl->Net) return;
  }
  SendKeepaliveInternal(currTime, forced);
  /*
  if (currTime < 0) currTime = 0;
  if (LastKeepAliveTime > currTime) LastKeepAliveTime = currTime; // wtf?!
  if (!forced && currTime-LastKeepAliveTime < 1.0/35.0) return;
  LastKeepAliveTime = currTime;
  // write out a nop
  cl->Net->Flush();
  */
}


//==========================================================================
//
//  CL_EstablishConnection
//
//  host should be a net address to be passed on
//
//==========================================================================
void CL_EstablishConnection (const char *host) {
  if (GGameInfo->NetMode == NM_DedicatedServer) return;

  SV_ShutdownGame();

  VSocketPublic *Sock = GNet->Connect(host);
  if (!Sock) {
    GCon->Log("Failed to connect to the server");
    return;
  }

  CL_SetUpNetClient(Sock);
  GCon->Logf(NAME_Dev, "CL_EstablishConnection: connected to %s", host);
  GGameInfo->NetMode = NM_Client;

  UserInfoSent = false;

  GClGame->eventConnected();
  cls.signon = 0; // need all the signon messages before playing
  LastKeepAliveTime = Sys_Time();

  MN_DeactivateMenu();
}


//==========================================================================
//
//  CL_SetUpLocalPlayer
//
//==========================================================================
void CL_SetUpLocalPlayer () {
  if (GGameInfo->NetMode == NM_DedicatedServer) return;

  VBasePlayer *Player = GPlayersBase[0];
  SV_ConnectClient(Player);
  ++svs.num_connected;

  cl = Player;
  cl->ClGame = GClGame;
  GClGame->cl = cl;

  cl->eventServerSetUserInfo(cls.userinfo);

  GClGame->eventConnected();
  cls.signon = 0; // need all the signon messages before playing

  MN_DeactivateMenu();

  CL_SetUpStandaloneClient();
}


//==========================================================================
//
//  CL_SetUpStandaloneClient
//
//==========================================================================
void CL_SetUpStandaloneClient () {
  CL_Clear();

  GClGame->serverinfo = svs.serverinfo;
  CL_ReadFromServerInfo();

  GClGame->maxclients = svs.max_clients;
  GClGame->deathmatch = deathmatch;

  const mapInfo_t &LInfo = P_GetMapInfo(*GLevel->MapName);
  GCon->Log("---------------------------------------");
  GCon->Log(LInfo.GetName());
  GCon->Log("");

  GClLevel = GLevel;
  GClGame->GLevel = GClLevel;

  R_Start(GClLevel);
  GAudio->Start();

  SB_Start();

  for (int i = 0; i < GClLevel->NumStaticLights; ++i) {
    rep_light_t &L = GClLevel->StaticLights[i];
    GClLevel->RenderData->AddStaticLightRGB(L.Owner, L.Origin, L.Radius, L.Color);
  }
  GClLevel->RenderData->PreRender();

  cl->SpawnClient();
  cls.signon = 1;

  GCon->Log(NAME_Dev, "Client level loaded");
  GCmdBuf << "HideConsole\n";

  Host_ResetSkipFrames();
}


//==========================================================================
//
//  CL_SendMove
//
//==========================================================================
void CL_SendMove () {
  if (!cl) return;

  if (cls.demoplayback || GGameInfo->NetMode == NM_TitleMap) return;

  if (cls.signon) {
    if (!GGameInfo->IsPaused()) cl->HandleInput();
    if (cl->Net) ((VPlayerChannel *)cl->Net->Channels[CHANIDX_Player])->Update();
  }

  if (cl->Net) cl->Net->Tick();
}


//==========================================================================
//
//  CL_Responder
//
//  get info needed to make ticcmd_ts for the players
//
//==========================================================================
bool CL_Responder (event_t *ev) {
  if (GGameInfo->NetMode == NM_TitleMap) return false;
  if (cl) return cl->Responder(ev);
  return false;
}


//==========================================================================
//
//  CL_Clear
//
//==========================================================================
void CL_Clear () {
  GClGame->serverinfo.Clean();
  GClGame->intermission = 0;
  if (cl) cl->ClearInput();
  if (GGameInfo->NetMode == NM_None || GGameInfo->NetMode == NM_Client) GAudio->StopAllSound(); // make sure all sounds are stopped
  cls.signon = 0;
}


//==========================================================================
//
//  CL_ReadFromServerInfo
//
//==========================================================================
void CL_ReadFromServerInfo () {
  VCvar::SetCheating(!!VStr::atoi(*Info_ValueForKey(GClGame->serverinfo, "sv_cheats")));
}


//==========================================================================
//
//  CL_DoLoadLevel
//
//==========================================================================
void CL_ParseServerInfo (const ClientServerInfo *sinfo) {
  check(sinfo);
  CL_Clear();

  //msg << GClGame->serverinfo;
  GClGame->serverinfo = sinfo->sinfo;
  CL_ReadFromServerInfo();

  //VStr TmpStr;
  //msg << TmpStr;
  //VName MapName = *TmpStr;
  VName MapName = *sinfo->mapname;

  //GClGame->maxclients = msg.ReadInt(/*MAXPLAYERS + 1*/);
  //GClGame->deathmatch = msg.ReadInt(/*256*/);
  GClGame->maxclients = sinfo->maxclients;
  GClGame->deathmatch = sinfo->deathmatch;

  const mapInfo_t &LInfo = P_GetMapInfo(MapName);
  GCon->Log("---------------------------------------");
  GCon->Log(LInfo.GetName());
  GCon->Log("");

  CL_LoadLevel(MapName);
  GClLevel->NetContext = ClientNetContext;

  ((VLevelChannel*)cl->Net->Channels[CHANIDX_Level])->SetLevel(GClLevel);

  R_Start(GClLevel);
  GAudio->Start();

  Host_ResetSkipFrames();

  SB_Start();

  GCon->Log(NAME_Dev, "Client level loaded");
}


//==========================================================================
//
//  VClientNetContext::GetLevel
//
//==========================================================================
VLevel *VClientNetContext::GetLevel () {
  return GClLevel;
}


//==========================================================================
//
//  CL_SetUpNetClient
//
//==========================================================================
void CL_SetUpNetClient (VSocketPublic *Sock) {
  // create player structure
  cl = (VBasePlayer *)VObject::StaticSpawnWithReplace(VClass::FindClass("Player"));
  cl->PlayerFlags |= VBasePlayer::PF_IsClient;
  cl->ClGame = GClGame;
  GClGame->cl = cl;

  if (cls.demorecording) {
    cl->Net = new VDemoRecordingNetConnection(Sock, ClientNetContext, cl);
  } else {
    cl->Net = new VNetConnection(Sock, ClientNetContext, cl);
  }
  ClientNetContext->ServerConnection = cl->Net;
  ((VPlayerChannel *)cl->Net->Channels[CHANIDX_Player])->SetPlayer(cl);
}


//==========================================================================
//
//  CL_PlayDemo
//
//==========================================================================
void CL_PlayDemo (const VStr &DemoName, bool IsTimeDemo) {
  char magic[8];

  // open the demo file
  VStr name = VStr("demos/")+DemoName.DefaultExtension(".dem");

  GCon->Logf("Playing demo from '%s'...", *name);
  VStream *Strm = FL_OpenFileReadInCfgDir(name);
  if (!Strm) {
    GCon->Logf("ERROR: couldn't open '%s'.", *name);
    return;
  }

  Strm->Serialise(magic, 4);
  magic[4] = 0;
  if (VStr::Cmp(magic, "VDEM")) {
    delete Strm;
    GCon->Logf("ERROR: '%s' is not a k8vavoom demo.", *name);
    return;
  }

  vuint32 ver = -1;
  *Strm << ver;
  if (ver != VAVOOM_DEMO_VERSION) {
    delete Strm;
    GCon->Logf("ERROR: '%s' has invalid version.", *name);
    return;
  }

  auto wadlist = GetWadPk3List();
  int wadlen = wadlist.length();
  int dmwadlen = -1;
  *Strm << STRM_INDEX(dmwadlen);
  if (dmwadlen != wadlen) {
    delete Strm;
    GCon->Logf("ERROR: '%s' was recorded with differrent mod set.", *name);
    return;
  }
  for (int f = 0; f < wadlen; ++f) {
    VStr s;
    *Strm << s;
    if (s != wadlist[f]) {
      delete Strm;
      GCon->Logf("ERROR: '%s' was recorded with differrent mod set.", *name);
      return;
    }
  }

  // disconnect from server
  SV_ShutdownGame();

  cls.demoplayback = true;

  // create player structure
  cl = (VBasePlayer *)VObject::StaticSpawnWithReplace(VClass::FindClass("Player"));
  cl->PlayerFlags |= VBasePlayer::PF_IsClient;
  cl->ClGame = GClGame;
  GClGame->cl = cl;

  cl->Net = new VDemoPlaybackNetConnection(ClientNetContext, cl, Strm, IsTimeDemo);
  ClientNetContext->ServerConnection = cl->Net;
  ((VPlayerChannel *)cl->Net->Channels[CHANIDX_Player])->SetPlayer(cl);

  GGameInfo->NetMode = NM_Client;
  GClGame->eventDemoPlaybackStarted();
}


//==========================================================================
//
//  CL_StopRecording
//
//==========================================================================
void CL_StopRecording () {
  // finish up
  delete cls.demofile;
  cls.demofile = nullptr;
  cls.demorecording = false;
  if (GDemoRecordingContext) {
    delete GDemoRecordingContext;
    GDemoRecordingContext = nullptr;
  }
  GCon->Log("Completed demo");
}


//==========================================================================
//
//  COMMAND Connect
//
//==========================================================================
COMMAND(Connect) {
  CL_EstablishConnection(Args.Num() > 1 ? *Args[1] : "");
}


//==========================================================================
//
//  COMMAND Disconnect
//
//==========================================================================
COMMAND(Disconnect) {
  CL_ResetLastSong();
  SV_ShutdownGame();
}


//==========================================================================
//
//  COMMAND StopDemo
//
//  stop recording a demo
//
//==========================================================================
COMMAND(StopDemo) {
  if (Source != SRC_Command) return;
  if (!cls.demorecording) {
    GCon->Log("Not recording a demo.");
    return;
  }
  CL_StopRecording();
}


//==========================================================================
//
//  COMMAND RecordDemo
//
//  RecordDemo <demoname> <map>
//
//==========================================================================
COMMAND(RecordDemo) {
  if (Source != SRC_Command) return;

  int c = Args.Num();
  if (c != 2 && c != 3) {
    GCon->Log("RecordDemo <demoname> [<map>]");
    return;
  }

  if (Args[1] == "?" || Args[1].ICmp("-h") == 0 || Args[1].ICmp("-help") == 0  || Args[1].ICmp("--help") == 0) {
    GCon->Log("RecordDemo <demoname> [<map>]");
    return;
  }

  if (strstr(*Args[1], "..") || strstr(*Args[1], "/") || strstr(*Args[1], "\\")) {
    GCon->Log("Relative pathnames are not allowed.");
    return;
  }

  if (c == 2 && GGameInfo->NetMode == NM_Client) {
    GCon->Log("Can not record demo -- already connected to server.");
    GCon->Log("Client demo recording must be started before connecting.");
    return;
  }

  if (cls.demorecording) {
    GCon->Log("Already recording a demo.");
    return;
  }

  VStr name = VStr("demos/")+Args[1].DefaultExtension(".dem");

  // start the map up
  if (c > 2) VCommand::ExecuteString(VStr("map ")+Args[2], SRC_Command, nullptr);

  // open the demo file
  GCon->Logf("recording to '%s'...", *name);
  cls.demofile = FL_OpenFileWriteInCfgDir(name);
  if (!cls.demofile) {
    GCon->Logf("ERROR: couldn't create '%s'.", *name);
    return;
  }

  cls.demofile->Serialise(const_cast<char *>("VDEM"), 4);

  vuint32 ver = VAVOOM_DEMO_VERSION;
  *cls.demofile << ver;

  auto wadlist = GetWadPk3List();
  int wadlen = wadlist.length();
  *cls.demofile << STRM_INDEX(wadlen);
  for (int f = 0; f < wadlen; ++f) *cls.demofile << wadlist[f];

  cls.demorecording = true;

  if (GGameInfo->NetMode == NM_Standalone || GGameInfo->NetMode == NM_ListenServer) {
    GDemoRecordingContext = new VServerNetContext();
    VSocketPublic *Sock = new VDemoRecordingSocket();
    VNetConnection *Conn = new VNetConnection(Sock, GDemoRecordingContext, cl);
    Conn->AutoAck = true;
    GDemoRecordingContext->ClientConnections.Append(Conn);
    Conn->ObjMap->SetUpClassLookup();
    VObjectMapChannel *Chan = (VObjectMapChannel *)Conn->CreateChannel(CHANNEL_ObjectMap, -1);
    (void)Chan; //k8:???
    while (!Conn->ObjMapSent) Conn->Tick();
    Conn->SendServerInfo();
    ((VPlayerChannel *)Conn->Channels[CHANIDX_Player])->SetPlayer(cl);
  }
}


//==========================================================================
//
//  COMMAND PlayDemo
//
//  play [demoname]
//
//==========================================================================
COMMAND(PlayDemo) {
  if (Source != SRC_Command) return;
  if (Args.Num() != 2) {
    GCon->Log("play <demoname> : plays a demo");
    return;
  }
  CL_PlayDemo(Args[1], false);
}


//==========================================================================
//
//  COMMAND TimeDemo
//
//  timedemo [demoname]
//
//==========================================================================
COMMAND(TimeDemo) {
  if (Source != SRC_Command) return;
  if (Args.Num() != 2) {
    GCon->Log("timedemo <demoname> : gets demo speeds");
    return;
  }
  CL_PlayDemo(Args[1], true);
}
