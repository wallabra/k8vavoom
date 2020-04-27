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
#include "drawer.h"
#include "net/network.h"
#include "cl_local.h"
#include "ui/ui.h"
#include "sv_local.h"

#define VAVOOM_DEMO_VERSION  (1)


void CL_SetupNetClient (VSocketPublic *);
void SV_ConnectClient (VBasePlayer *);
void CL_Clear ();
void CL_ReadFromServerInfo ();


client_static_t cls;
VBasePlayer *cl = nullptr;
float clWipeTimer = -1.0f;
VClientNetContext *ClientNetContext = nullptr;

VClientGameBase *GClGame = nullptr;

bool UserInfoSent = false;

VCvarS cl_name("name", "PLAYER", "Player name.", CVAR_Archive|CVAR_UserInfo);
VCvarS cl_color("color", "00 ff 00", "Player color.", CVAR_Archive|CVAR_UserInfo);
VCvarI cl_class("class", "0", "Player class.", /*CVAR_Archive|*/CVAR_UserInfo); // do not save it, because it may interfere with other games
VCvarS cl_model("model", "", "Player model.", CVAR_Archive|CVAR_UserInfo);

static VCvarB cl_autonomous_proxy("cl_autonomous_proxy", false, "Is our client an autonomous proxy?", CVAR_PreInit);

static VCvarB d_attraction_mode("d_attraction_mode", false, "Allow demo playback (won't work with non-k8vavoom demos)?", CVAR_Archive);

extern VCvarB r_wipe_enabled;

IMPLEMENT_CLASS(V, ClientGameBase);

static VName CurrentSongLump;
static double ClientLastKeepAliveTime = 0.0;


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
  // this emits code for all `PackagesToEmit()`
  VPackage::StaticEmitPackages();
  //!TLocation::ClearSourceFiles();
  ClientNetContext = new VClientNetContext();
  GClGame = (VClientGameBase *)VObject::StaticSpawnWithReplace(VClass::FindClass("ClientGame"));
  GClGame->Game = GGameInfo;
  GClGame->eventPostSpawn();
  CurrentSongLump = NAME_None;
  // preload crosshairs, why not?
  for (int cnum = 1; cnum < 16; ++cnum) {
    int handle = GTextureManager.AddPatch(VName(va("croshai%x", cnum), VName::AddLower8), TEXTYPE_Pic, true/*silent*/);
    if (handle > 0 && developer) GCon->Logf(NAME_Debug, "found crosshair #%d", cnum);
  }
  R_FuckOffShitdoze();
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
static void CL_Ticker () {
  // do main actions
  if (!GClGame->InIntermission()) {
    SB_Ticker();
    AM_Ticker();
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
  if (GClLevel && GClLevel->Renderer) GClLevel->Renderer->DecayLights(host_frametime);
}


//==========================================================================
//
//  CL_UpdateMobjs
//
//==========================================================================
static void CL_UpdateMobjs (float deltaTime) {
  if (!GClLevel) return; //k8: this is wrong!
  if (!deltaTime) return;
  //GCon->Log(NAME_Debug, "====================== CL_UpdateMobjs ======================");
  // animate decals
  GClLevel->TickDecals(deltaTime);
  if (GGameInfo->NetMode == NM_Client) {
    // network client
    // cannot use thinker iterator here, because detached thinker may remove itself...
    VThinker *curr = GClLevel->ThinkerHead;
    while (curr) {
      VThinker *th = curr;
      curr = curr->Next;
      if (th->IsGoingToDie() || !th->Level || th->Level->IsGoingToDie()) continue;
      if (th->Role != ROLE_DumbProxy) {
        //GCon->Logf(NAME_Debug, "%s:%u: client-local!", th->GetClass()->GetName(), th->GetUniqueId());
        // for local thinkers, call their ticker method first
        if (th->ThinkerFlags&VThinker::TF_DetachComplete) {
          //GCon->Logf(NAME_Debug, "%s:%u: client-local TICK!", th->GetClass()->GetName(), th->GetUniqueId());
          // need to set this flag, so spawning could work
          const vuint32 oldLevelFlags = GClLevel->LevelFlags;
          GClLevel->LevelFlags |= VLevel::LF_ForServer;
          th->Tick(deltaTime);
          GClLevel->LevelFlags = oldLevelFlags;
          if (th->IsGoingToDie()) continue; // just in case
        }
      }
      th->eventClientTick(deltaTime);
    }
  } else {
    // other game types; don't use iterator too, because it does excessive class checks
    VThinker *curr = GClLevel->ThinkerHead;
    while (curr) {
      VThinker *th = curr;
      curr = curr->Next;
      if (!th->IsGoingToDie()) th->eventClientTick(deltaTime);
    }
  }
}


//==========================================================================
//
//  CL_ReadFromServer
//
//  Read all incoming data from the server
//
//==========================================================================
void CL_ReadFromServer (float deltaTime) {
  if (!cl) return;

  if (cl->Net) {
    ClientLastKeepAliveTime = Sys_Time();
    cl->Net->GetMessages();
    if (cl->Net->IsClosed()) Host_EndGame("Server disconnected");
  }

  if (cls.signon) {
    // for interpolator
    if (GGameInfo->NetMode == NM_Client && deltaTime) {
      GClLevel->Time += deltaTime;
      GClLevel->TicTime = (int)(GClLevel->Time*35.0f);
    }

    // bad for interpolator
    /*
    if (GGameInfo->NetMode == NM_Client) {
      GClLevel->Time = cl->GameTime;
      GClLevel->TicTime = (int)(GClLevel->Time*35.0f);
    }
    */

    /*
    if (cl->ClCurrGameTime < cl->ClLastGameTime) {
      GCon->Logf(NAME_Debug, ":RECV: WARP: GT=%g (d:%g); cltime=%g; svtime=%g; d=%g; deltaTime=%g; MO=%g (%g)", cl->GameTime, cl->LastDeltaTime, cl->ClCurrGameTime, cl->ClLastGameTime, cl->ClLastGameTime-cl->ClCurrGameTime, deltaTime,
        (cl->MO ? cl->MO->DataGameTime : 0), (cl->MO ? cl->ClLastGameTime-cl->MO->DataGameTime : 0));
      cl->ClCurrGameTime = cl->ClLastGameTime;
    } else {
      GCon->Logf(NAME_Debug, ":RECV: REPREDICT: GT=%g (d:%g); cltime=%g; svtime=%g; d=%g; deltaTime=%g MO=%g (%g)", cl->GameTime, cl->LastDeltaTime, cl->ClCurrGameTime, cl->ClLastGameTime, cl->ClCurrGameTime-cl->ClLastGameTime, deltaTime,
        (cl->MO ? cl->MO->DataGameTime : 0), (cl->MO ? cl->ClLastGameTime-cl->MO->DataGameTime : 0));
    }
    */

    CL_UpdateMobjs(deltaTime);
    // update world tick for client network games (copy from the server tic)
    if (deltaTime) {
      cl->ClCurrGameTime += deltaTime;
      cl->eventClientTick(deltaTime);
      CL_Ticker();
    }

    //GCon->Logf(NAME_Debug, ":RECV: camera is MO:%d; GameTime=%g; ClLastGameTime=%g; ClCurrGameTime=%g", (cl->Camera == cl->MO ? 1 : 0), cl->GameTime, cl->ClLastGameTime, cl->ClCurrGameTime);
  } else {
    /*
    if (GGameInfo->NetMode == NM_Client && deltaTime) {
      GCon->Logf(NAME_Debug, "client is not signed on yet... (%g)", Sys_Time());
    }
    */
  }

  if (deltaTime && GClLevel && GClLevel->LevelInfo) {
    if (CurrentSongLump != GClLevel->LevelInfo->SongLump) {
      CurrentSongLump = GClLevel->LevelInfo->SongLump;
      GAudio->MusicChanged();
    }
  }
}


//==========================================================================
//
//  CL_NetworkHeartbeat
//
//  when the client is taking a long time to load stuff, send keepalive
//  messages so the server doesn't disconnect
//
//==========================================================================
void CL_NetworkHeartbeat (bool forced) {
  if (GGameInfo->NetMode != NM_Client) return; // no need if server is local
  if (cls.demorecording || cls.demoplayback) return;
  if (!cl->Net) return;
  const double currTime = Sys_Time();
  if (ClientLastKeepAliveTime > currTime) ClientLastKeepAliveTime = currTime; // wtf?!
  if (!forced && currTime-ClientLastKeepAliveTime < 1.0/60.0) return;
  ClientLastKeepAliveTime = currTime;
  cl->Net->KeepaliveTick();
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

  GCon->Log("shutting down current game...");
  SV_ShutdownGame();

  //R_OSDMsgReset(OSD_Network);
  //R_OSDMsgShow(va("initiating connection to [%s]", (host ? host : "")));

  GCon->Logf("connecting to '%s'...", (host ? host : ""));
  VSocketPublic *Sock = GNet->Connect(host);
  if (!Sock) {
    GCon->Log(NAME_Error, "Failed to connect to the server");
    return;
  }

  GCon->Log("initialising net client...");
  CL_SetupNetClient(Sock);
  GCon->Logf(NAME_Dev, "CL_EstablishConnection: connected to '%s'", host);
  GGameInfo->NetMode = NM_Client;

  UserInfoSent = false;

  GClGame->eventConnected();
  // need all the signon messages before playing
  cls.signon = 0;
  ClientLastKeepAliveTime = Sys_Time();

  MN_DeactivateMenu();
}


//==========================================================================
//
//  CL_SetupLocalPlayer
//
//==========================================================================
void CL_SetupLocalPlayer () {
  if (GGameInfo->NetMode == NM_DedicatedServer) return;

  VBasePlayer *Player = GPlayersBase[0];
  SV_ConnectClient(Player);
  ++svs.num_connected;

  cl = Player;
  cl->ClGame = GClGame;
  GClGame->cl = cl;

  cl->eventServerSetUserInfo(cls.userinfo);

  GClGame->eventConnected();
  // need all the signon messages before playing
  // but the map may be aloready loaded
  cls.signon = 0;

  MN_DeactivateMenu();

  CL_SetupStandaloneClient();
}


//==========================================================================
//
//  CL_SetupStandaloneClient
//
//==========================================================================
void CL_SetupStandaloneClient () {
  CL_Clear();

  GClGame->serverinfo = svs.serverinfo;
  CL_ReadFromServerInfo();
  // cheating is always enabled in standalone client
  if (GGameInfo->NetMode == NM_TitleMap || GGameInfo->NetMode == NM_Standalone) VCvar::SetCheating(true);

  GClGame->maxclients = svs.max_clients;
  GClGame->deathmatch = svs.deathmatch;

  const VMapInfo &LInfo = P_GetMapInfo(*GLevel->MapName);
  GCon->Log("---------------------------------------");
  GCon->Log(LInfo.GetName());
  GCon->Log("");

  GClLevel = GLevel;
  GClGame->GLevel = GClLevel;

  R_Start(GClLevel);
  GAudio->Start();

  SB_Start();

  GClLevel->Renderer->ResetStaticLights();
  for (int i = 0; i < GClLevel->NumStaticLights; ++i) {
    rep_light_t &L = GClLevel->StaticLights[i];
    GClLevel->Renderer->AddStaticLightRGB(L.OwnerUId, L.Origin, L.Radius, L.Color, L.ConeDir, L.ConeAngle);
  }
  GClLevel->Renderer->PreRender();

  cl->SpawnClient();
  //cls.signon = 1;
  cls.clearForStandalone();

  // if wipe is enabled, tick the world once, so spawned things will fix themselves
  if (r_wipe_enabled) {
    if (developer) GCon->Log(NAME_Dev, "****** WIPE TICK ******");
    GLevel->TickWorld(SV_GetFrameTimeConstant());
  }

  GCon->Log(NAME_Dev, "Client level loaded");
  GCmdBuf << "HideConsole\n";

  Host_ResetSkipFrames();
}


//==========================================================================
//
//  CL_GotNetOrigin
//
//  returns `true` if not a network client, or if network
//  client got MO origin
//
//==========================================================================
bool CL_GotNetOrigin () {
  if (!cl->Net) return true;
  if (!cl->MO) return false;
  return cl->Net->GetPlayerChannel()->GotMOOrigin;
}


extern VCvarI sv_maxmove;


//==========================================================================
//
//  CL_RunSimulatedPlayerTick
//
//==========================================================================
static void CL_RunSimulatedPlayerTick (float deltaTime) {
  if (!cl || !cl->Net || !cl->MO || !cl->Net->GetPlayerChannel()->GotMOOrigin) return;
  //if (cl->MO->Role != ROLE_AutonomousProxy) return;
  if (!cl->isAutonomousProxy()) return;

  auto oldplr = cl->MO->Player;
  auto oldRole = cl->MO->Role;
  cl->MO->Player = cl;
  cl->MO->Role = ROLE_AutonomousProxy; // force it, why not?

  cl->ForwardMove = cl->ClientForwardMove;
  cl->SideMove = cl->ClientSideMove;

  // don't move faster than maxmove
       if (cl->ForwardMove > sv_maxmove) cl->ForwardMove = sv_maxmove;
  else if (cl->ForwardMove < -sv_maxmove) cl->ForwardMove = -sv_maxmove;
       if (cl->SideMove > sv_maxmove) cl->SideMove = sv_maxmove;
  else if (cl->SideMove < -sv_maxmove) cl->SideMove = -sv_maxmove;
  // check for disabled freelook and jumping
  /*
  if (!sv_ignore_nomlook && (GLevelInfo->LevelInfoFlags&VLevelInfo::LIF_NoFreelook)) cl->ViewAngles.pitch = 0;
  if (!sv_ignore_nojump && (GLevelInfo->LevelInfoFlags&VLevelInfo::LIF_NoJump)) cl->Buttons &= ~BT_JUMP;
  if (!sv_ignore_nocrouch && (GLevelInfo->LevelInfoFlags&VLevelInfo::LIF2_NoCrouch)) cl->Buttons &= ~BT_JUMP;
  if (cl_disable_crouch) cl->Buttons &= ~BT_CROUCH;
  if (cl_disable_jump) cl->Buttons &= ~BT_JUMP;
  if (cl_disable_mlook) cl->ViewAngles.pitch = 0;
  */

  //GCon->Logf(NAME_Debug, "SIMPL:000: org=(%g,%g,%g); vel=(%g,%g,%g); fwd=%g; side=%g", cl->MO->Origin.x, cl->MO->Origin.y, cl->MO->Origin.z, cl->MO->Velocity.x, cl->MO->Velocity.y, cl->MO->Velocity.z, cl->ForwardMove, cl->SideMove);
  //GCon->Logf(NAME_Debug, "000: ViewHeight=%g", cl->ViewHeight);
  cl->MO->Tick(deltaTime);
  cl->eventPlayerTick(deltaTime);
  cl->eventSetViewPos();
  //GCon->Logf(NAME_Debug, "001: ViewHeight=%g", cl->ViewHeight);
  //GCon->Logf(NAME_Debug, "SIMPL:001: org=(%g,%g,%g); vel=(%g,%g,%g); fwd=%g; side=%g", cl->MO->Origin.x, cl->MO->Origin.y, cl->MO->Origin.z, cl->MO->Velocity.x, cl->MO->Velocity.y, cl->MO->Velocity.z, cl->ForwardMove, cl->SideMove);
  cl->MO->Player = oldplr;
  cl->MO->Role = oldRole;
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
    if (cl->Net) {
      VPlayerChannel *pc = cl->Net->GetPlayerChannel();
      CL_RunSimulatedPlayerTick(host_frametime);
      if (pc) pc->Update();
      //GCon->Logf(NAME_Debug, ":SEND: camera is MO:%d; GameTime=%g; ClLastGameTime=%g; ClCurrGameTime=%g", (cl->Camera == cl->MO ? 1 : 0), cl->GameTime, cl->ClLastGameTime, cl->ClCurrGameTime);
    }
  }

  if (cl->Net) cl->Net->Tick();
}


//==========================================================================
//
//  CL_NetInterframe
//
//==========================================================================
void CL_NetInterframe () {
  if (!cl || !cl->Net) return;
  if (cls.demoplayback || GGameInfo->NetMode == NM_TitleMap) return;
  // no need to update channels here
  cl->Net->Tick();
}


//==========================================================================
//
//  CL_IsInGame
//
//==========================================================================
bool CL_IsInGame () {
  return (cl && !GClGame->InIntermission());
}


//==========================================================================
//
//  CL_GetNetState
//
//  returns `CLState_XXX`
//
//==========================================================================
int CL_GetNetState () {
  if (GGameInfo->NetMode != NM_Client || !cl || !cl->Net) return CLState_None;
  if (!cl->Net->ObjMapSent) return CLState_Init;
  if (!cls.signon) return CLState_Init;
  if (cls.gotmap < 2) return CLState_Init; //not sure
  return CLState_InGame;
}


//==========================================================================
//
//  CL_SendCommandToServer
//
//  returns `false` if client networking is not active
//
//==========================================================================
bool CL_SendCommandToServer (VStr cmd) {
  if (!cl || !cl->Net) return false;
  //GCon->Logf(NAME_Debug, "*** sending command over the network: <%s>\n", *Original);
  cl->Net->SendCommand(cmd);
  return true;
}


//==========================================================================
//
//  CL_SetNetAbortCallback
//
//==========================================================================
void CL_SetNetAbortCallback (bool (*cb) (void *udata), void *udata) {
  if (GNet) {
    GNet->CheckUserAbortCB = cb;
    GNet->CheckUserAbortUData = udata;
  }
}


//==========================================================================
//
//  CL_GetNetLag
//
//==========================================================================
int CL_GetNetLag () {
  return (cl && cl->Net ? (int)((cl->Net->PrevLag+1.2*(max2(cl->Net->InLoss, cl->Net->OutLoss)*0.01))*1000) : 0);
}


//==========================================================================
//
//  CL_IsDangerousTimeout
//
//==========================================================================
bool CL_IsDangerousTimeout () {
  return (cl && cl->Net ? cl->Net->IsDangerousTimeout() : false);
}


//==========================================================================
//
//  CL_GetNumberOfChannels
//
//==========================================================================
int CL_GetNumberOfChannels () {
  return (cl && cl->Net ? cl->Net->OpenChannels.length() : 0);
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
  GClGame->ResetIntermission();
  if (cl) cl->ClearInput();
  if (GGameInfo->NetMode == NM_None || GGameInfo->NetMode == NM_Client) GAudio->StopAllSound(); // make sure all sounds are stopped
  //cls.signon = 0;
  cls.clearForClient();
}


//==========================================================================
//
//  CL_ReadFromServerInfo
//
//==========================================================================
void CL_ReadFromServerInfo () {
  VCvar::SetCheating(!!VStr::atoi(*Info_ValueForKey(GClGame->serverinfo, "sv_cheats")));
  if (GGameInfo->NetMode == NM_Client) {
    GGameInfo->deathmatch = VStr::atoi(*Info_ValueForKey(GClGame->serverinfo, "DeathMatch"));
    //GCon->Logf(NAME_Debug, "deathmatch mode is %u", GGameInfo->deathmatch);
  }
}


//==========================================================================
//
//  CL_DoLoadLevel
//
//==========================================================================
void CL_ParseServerInfo (const VNetClientServerInfo *sinfo) {
  vassert(sinfo);
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

  const VMapInfo &LInfo = P_GetMapInfo(MapName);
  GCon->Log("---------------------------------------");
  GCon->Log(LInfo.GetName());
  GCon->Log("");

  CL_LoadLevel(MapName);
  GClLevel->NetContext = ClientNetContext;

  cl->Net->GetLevelChannel()->SetLevel(GClLevel);
  cl->Net->GetLevelChannel()->SendMapLoaded();

  R_Start(GClLevel);
  GAudio->Start();

  Host_ResetSkipFrames();

  SB_Start();

  GCon->Log(NAME_Dev, "Client level loaded");

  if (GClLevel->MapHash != sinfo->maphash) Host_Error("Server has different map data");

  cls.gotmap = 1;
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
//  CL_SetupNetClient
//
//==========================================================================
void CL_SetupNetClient (VSocketPublic *Sock) {
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
  cl->Net->GetPlayerChannel()->SetPlayer(cl);
  cl->eventClientSetAutonomousProxy(cl_autonomous_proxy.asBool());
  cl->setAutonomousProxy(cl_autonomous_proxy.asBool());
}


//==========================================================================
//
//  CL_PlayDemo
//
//==========================================================================
void CL_PlayDemo (VStr DemoName, bool IsTimeDemo) {
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

  auto wadlist = FL_GetWadPk3List();
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
  cl->Net->GetPlayerChannel()->SetPlayer(cl);

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
  if (cls.demofile) cls.demofile->Close();
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
#ifdef CLIENT
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
//  COMMAND_WITH_AC RecordDemo
//
//  RecordDemo <demoname> <map>
//
//==========================================================================
COMMAND_WITH_AC(RecordDemo) {
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

  auto wadlist = FL_GetWadPk3List();
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
    Conn->ObjMap->SetupClassLookup();
    (void)Conn->CreateChannel(CHANNEL_ObjectMap, -1, true); // local
    while (!Conn->ObjMapSent) Conn->Tick();
    Conn->LoadedNewLevel();
    Conn->SendServerInfo();
    Conn->GetPlayerChannel()->SetPlayer(cl);
  }
}


//==========================================================================
//
//  COMMAND_AC RecordDemo
//
//==========================================================================
COMMAND_AC(RecordDemo) {
  VStr prefix = (aidx < args.length() ? args[aidx] : VStr());
  if (aidx == 2) {
    TArray<VStr> list;
    // prefer pwad maps
    if (fsys_PWadMaps.length()) {
      list.resize(fsys_PWadMaps.length());
      for (auto &&lmp : fsys_PWadMaps) list.append(lmp.mapname);
    } else {
      int mapcount = P_GetNumMaps();
      list.resize(mapcount);
      for (int f = 0; f < mapcount; ++f) {
        VName mlump = P_GetMapLumpName(f);
        if (mlump != NAME_None) list.append(VStr(mlump));
      }
    }
    if (list.length()) return AutoCompleteFromListCmd(prefix, list);
  } else if (aidx < 2) {
    GCon->Log("RecordDemo <demoname> [<map>]");
  }
  return VStr::EmptyString;
}


//==========================================================================
//
//  COMMAND_WITH_AC PlayDemo
//
//  play [demoname]
//
//==========================================================================
COMMAND_WITH_AC(PlayDemo) {
  if (Source != SRC_Command) return;
  if (Args.Num() != 2) {
    GCon->Log("play <demoname> : plays a demo");
    return;
  }
  CL_PlayDemo(Args[1], false);
}


//==========================================================================
//
//  DoDemoCompletions
//
//==========================================================================
static VStr DoDemoCompletions (const TArray<VStr> &args, int aidx) {
  TArray<VStr> list;
  VStr prefix = (aidx < args.length() ? args[aidx] : VStr());
  if (aidx == 1) {
    void *dir = Sys_OpenDir(FL_GetConfigDir().appendPath("demos"));
    if (!dir) return VStr::EmptyString;
    for (;;) {
      VStr fname = Sys_ReadDir(dir);
      if (fname.isEmpty()) break;
      if (fname.endsWithCI(".dem")) list.append(fname);
    }
    Sys_CloseDir(dir);
    return VCommand::AutoCompleteFromListCmd(prefix, list);
  } else {
    return VStr::EmptyString;
  }
}


//==========================================================================
//
//  COMMAND_AC PlayDemo
//
//==========================================================================
COMMAND_AC(PlayDemo) {
  return DoDemoCompletions(args, aidx);
}


//==========================================================================
//
//  COMMAND_WITH_AC TimeDemo
//
//  timedemo [demoname]
//
//==========================================================================
COMMAND_WITH_AC(TimeDemo) {
  if (Source != SRC_Command) return;
  if (Args.Num() != 2) {
    GCon->Log("timedemo <demoname> : gets demo speeds");
    return;
  }
  CL_PlayDemo(Args[1], true);
}


//==========================================================================
//
//  COMMAND_AC TimeDemo
//
//==========================================================================
COMMAND_AC(TimeDemo) {
  return DoDemoCompletions(args, aidx);
}


//==========================================================================
//
//  COMMAND VidRendererRestart
//
//  VidRendererRestart
//
//==========================================================================
COMMAND(VidRendererRestart) {
  if (Source != SRC_Command) return;
  if (!GClLevel) return;
  if (!GClLevel->Renderer) return;
  R_OSDMsgReset(OSD_MapLoading);
  Drawer->SetMainFBO(); // just in case
  delete GClLevel->Renderer;
  GClLevel->Renderer = nullptr;
  GClLevel->cacheFileBase.clear(); // so we won't store stale lightmaps
  R_Start(GClLevel);
  GClLevel->Renderer->ResetStaticLights();
  for (int i = 0; i < GClLevel->NumStaticLights; ++i) {
    rep_light_t &L = GClLevel->StaticLights[i];
    GClLevel->Renderer->AddStaticLightRGB(L.OwnerUId, L.Origin, L.Radius, L.Color, L.ConeDir, L.ConeAngle);
  }
  GClLevel->Renderer->PreRender();
  Host_ResetSkipFrames();
}
#endif
