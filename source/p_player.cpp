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
#include "sv_local.h"
#include "cl_local.h"


IMPLEMENT_CLASS(V, BasePlayer)

bool VBasePlayer::isCheckpointSpawn = false;


static VCvarF hud_notify_time("hud_notify_time", "5", "Notification timeout, in seconds.", CVAR_Archive);
static VCvarF centre_msg_time("hud_centre_message_time", "7", "Centered message timeout.", CVAR_Archive);
static VCvarB hud_msg_echo("hud_msg_echo", true, "Echo messages?", CVAR_Archive);
static VCvarI hud_font_color("hud_font_color", "11", "Font color.", CVAR_Archive);
static VCvarI hud_font_color_centered("hud_font_color_centered", "11", "Secondary font color.", CVAR_Archive);

//VField *VBasePlayer::fldPendingWeapon = nullptr;
//VField *VBasePlayer::fldReadyWeapon = nullptr;

#define WEAPONBOTTOM  (128.0)
#define WEAPONTOP     (32.0)


struct SavedVObjectPtr {
  VObject **ptr;
  VObject *saved;
  SavedVObjectPtr (VObject **aptr) : ptr(aptr), saved(*aptr) {}
  ~SavedVObjectPtr() { *ptr = saved; }
};


//==========================================================================
//
//  VBasePlayer::PostCtor
//
//==========================================================================
void VBasePlayer::PostCtor () {
  Super::PostCtor();
  /*
  GCon->Logf("********** BASEPLAYER POSTCTOR (%s) **********", *GetClass()->GetFullName());
  VField *field = GetClass()->FindField("k8ElvenGiftMessageTime");
  if (field) GCon->Logf("  k8ElvenGiftMessageTime=%g", field->GetFloat(this));
  abort();
  */
}


//==========================================================================
//
//  VBasePlayer::ExecuteNetMethod
//
//==========================================================================
bool VBasePlayer::ExecuteNetMethod (VMethod *Func) {
  if (GDemoRecordingContext) {
    // find initial version of the method
    VMethod *Base = Func;
    while (Base->SuperMethod) Base = Base->SuperMethod;
    // execute it's replication condition method
    check(Base->ReplCond);
    P_PASS_REF(this);
    vuint32 SavedFlags = PlayerFlags;
    PlayerFlags &= ~VBasePlayer::PF_IsClient;
    bool ShouldSend = false;
    if (VObject::ExecuteFunctionNoArgs(Base->ReplCond).getBool()) ShouldSend = true;
    PlayerFlags = SavedFlags;

    if (ShouldSend) {
      // replication condition is true, the method must be replicated
      GDemoRecordingContext->ClientConnections[0]->Channels[CHANIDX_Player]->SendRpc(Func, this);
    }
  }

#ifdef CLIENT
  if (GGameInfo->NetMode == NM_TitleMap ||
      GGameInfo->NetMode == NM_Standalone ||
      (GGameInfo->NetMode == NM_ListenServer && this == cl))
  {
    return false;
  }
#endif

  // find initial version of the method
  VMethod *Base = Func;
  while (Base->SuperMethod) Base = Base->SuperMethod;
  // execute it's replication condition method
  check(Base->ReplCond);
  P_PASS_REF(this);
  if (!VObject::ExecuteFunctionNoArgs(Base->ReplCond).getBool()) return false;

  if (Net) {
    // replication condition is true, the method must be replicated
    Net->Channels[CHANIDX_Player]->SendRpc(Func, this);
  }

  // clean up parameters
  Func->CleanupParams();

  // it's been handled here
  return true;
}


//==========================================================================
//
//  VBasePlayer::SpawnClient
//
//==========================================================================
void VBasePlayer::SpawnClient () {
  if (!sv_loading) {
    if (PlayerFlags&PF_Spawned) GCon->Log(NAME_Dev, "Already spawned");
    if (MO) GCon->Log(NAME_Dev, "Mobj already spawned");
    eventSpawnClient();
    for (int i = 0; i < Level->XLevel->ActiveSequences.Num(); ++i) {
      eventClientStartSequence(
        Level->XLevel->ActiveSequences[i].Origin,
        Level->XLevel->ActiveSequences[i].OriginId,
        Level->XLevel->ActiveSequences[i].Name,
        Level->XLevel->ActiveSequences[i].ModeNum);
      for (int j = 0; j < Level->XLevel->ActiveSequences[i].Choices.Num(); ++j) {
        eventClientAddSequenceChoice(
          Level->XLevel->ActiveSequences[i].OriginId,
          Level->XLevel->ActiveSequences[i].Choices[j]);
      }
    }
  } else {
    if (!MO) Host_Error("Player without Mobj\n");
  }

  ViewAngles.roll = 0;
  eventClientSetAngles(ViewAngles);
  PlayerFlags &= ~PF_FixAngle;

  PlayerFlags |= PF_Spawned;

  if ((GGameInfo->NetMode == NM_TitleMap || GGameInfo->NetMode == NM_Standalone) && run_open_scripts) {
    // start open scripts
    Level->XLevel->Acs->StartTypedACScripts(SCRIPT_Open, 0, 0, 0, nullptr, false, false);
  }

  if (!sv_loading) {
    Level->XLevel->Acs->StartTypedACScripts(SCRIPT_Enter, 0, 0, 0, MO, true, false);
  } else if (sv_map_travel) {
    Level->XLevel->Acs->StartTypedACScripts(SCRIPT_Return, 0, 0, 0, MO, true, false);
  }

  if (GGameInfo->NetMode < NM_DedicatedServer || svs.num_connected == sv_load_num_players) {
    sv_loading = false;
    sv_map_travel = false;
  }

  // for single play, save immediately into the reborn slot
  //!if (GGameInfo->NetMode < NM_DedicatedServer) SV_SaveGameToReborn();
}


//==========================================================================
//
//  VBasePlayer::Printf
//
//==========================================================================
__attribute__((format(printf,2,3))) void VBasePlayer::Printf (const char *s, ...) {
  va_list v;
  static char buf[4096];

  va_start(v, s);
  vsnprintf(buf, sizeof(buf), s, v);
  va_end(v);

  eventClientPrint(buf);
}


//==========================================================================
//
//  VBasePlayer::CentrePrintf
//
//==========================================================================
__attribute__((format(printf,2,3))) void VBasePlayer::CentrePrintf (const char *s, ...) {
  va_list v;
  static char buf[4096];

  va_start(v, s);
  vsnprintf(buf, sizeof(buf), s, v);
  va_end(v);

  eventClientCentrePrint(buf);
}


//===========================================================================
//
//  VBasePlayer::SetViewState
//
//===========================================================================
void VBasePlayer::SetViewState (int position, VState *stnum) {
  if (position < 0 || position >= NUMPSPRITES) return; // sanity check
  if (position == PS_WEAPON && !stnum) ViewStates[PS_WEAPON_OVL].State = nullptr;
  //if (position == PS_WEAPON_OVL /*&& stnum*/) GCon->Logf("ticking OVERLAY (%s)", (stnum ? "tan" : "ona"));
  /*
  if (!fldPendingWeapon) {
    fldPendingWeapon = GetClass()->FindFieldChecked("PendingWeapon");
    if (fldPendingWeapon->Type.Type != TYPE_Reference) Sys_Error("'PendingWeapon' in playerpawn should be a reference");
    //fprintf(stderr, "*** TP=<%s>\n", *fldPendingWeapon->Type.GetName());
  }
  SavedVObjectPtr svp(&_stateRouteSelf);
  _stateRouteSelf = fldPendingWeapon->GetObjectValue(this);
  */
  //fprintf(stderr, "VBasePlayer::SetViewState(%s): route=<%s>\n", GetClass()->GetName(), (_stateRouteSelf ? _stateRouteSelf->GetClass()->GetName() : "<fuck>"));
  //VField *VBasePlayer::fldPendingWeapon = nullptr;
  //fprintf(stderr, "  VBasePlayer::SetViewState: position=%d; stnum=%s\n", position, (stnum ? *stnum->GetFullName() : "<none>"));
#if 0
  if (position == PS_WEAPON && stnum) {
    static VClass *WeaponClass = nullptr;
    if (!WeaponClass) WeaponClass = VClass::FindClass("Weapon");
    if (!fldReadyWeapon) {
      fldReadyWeapon = GetClass()->FindFieldChecked("ReadyWeapon");
      if (fldReadyWeapon->Type.Type != TYPE_Reference) Sys_Error("'ReadyWeapon' in playerpawn should be a reference");
    }
    if (WeaponClass) {
      VObject *wobj = fldReadyWeapon->GetObjectValue(this);
      if (wobj != lastReadyWeapon) {
        lastReadyWeapon = wobj;
        if (wobj && wobj->GetClass()->IsChildOf(WeaponClass)) {
          VEntity *wpn = (VEntity *)wobj;
          lastReadyWeaponReadyState = wpn->FindState("Ready");
          //GCon->Logf("WEAPON CHANGED TO '%s'", *wpn->GetClass()->Name);
          SetViewState(PS_WEAPON_OVL, wpn->FindState("Display"));
          /*
          ViewStates[PS_WEAPON_OVL].State = wpn->FindState("Display");
          if (ViewStates[PS_WEAPON_OVL].State) {
            GCon->Logf("WEAPON CHANGED TO '%s', 'Display' substate found", *wpn->GetClass()->Name);
          }
          */
        } else {
          lastReadyWeaponReadyState = nullptr;
          ViewStates[PS_WEAPON_OVL].State = nullptr;
        }
      }
      /*
      if (lastReadyWeaponReadyState == stnum) {
        GCon->Logf("SetViewState: %s", *stnum->Name);
      }
      */
    }
  }
#else
  if (_stateRouteSelf != LastViewObject[position]) {
    LastViewObject[position] = _stateRouteSelf;
    ViewStates[position].SX = ViewStates[position].OfsY = 0;
    // "display" state
    if (position == PS_WEAPON) {
      LastViewObject[PS_WEAPON_OVL] = _stateRouteSelf;
      static VClass *WeaponClass = nullptr;
      if (!WeaponClass) WeaponClass = VClass::FindClass("Weapon");
      if (_stateRouteSelf && _stateRouteSelf->GetClass()->IsChildOf(WeaponClass)) {
        VEntity *wpn = (VEntity *)_stateRouteSelf;
        SetViewState(PS_WEAPON_OVL, wpn->FindState("Display"));
      } else {
        ViewStates[PS_WEAPON_OVL].State = nullptr;
        LastViewObject[PS_WEAPON_OVL] = nullptr;
      }
    }
  }
#endif
  VViewState &VSt = ViewStates[position];
  VState *state = stnum;
  int watchcatCount = 1024;
  do {
    if (--watchcatCount <= 0) {
      //k8: FIXME!
      GCon->Logf("ERROR: WatchCat interrupted `VBasePlayer::SetViewState`!");
      break;
    }
    if (!state) {
      // object removed itself
      //_stateRouteSelf = nullptr; // why not?
      DispSpriteFrame[position] = 0;
      DispSpriteName[position] = NAME_None;
      VSt.State = nullptr;
      VSt.StateTime = -1;
      break;
    }

    // remember current sprite and frame
    UpdateDispFrameFrom(position, state);

    VSt.State = state;
    VSt.StateTime = state->Time; // could be 0
    if (state->Misc1) VSt.SX = state->Misc1;
    VSt.OfsY = state->Misc2;
    // call action routine
    if (state->Function) {
      //fprintf(stderr, "    VBasePlayer::SetViewState: CALLING '%s'(%s): position=%d; stnum=%s\n", *state->Function->GetFullName(), *state->Function->Loc.toStringNoCol(), position, (stnum ? *stnum->GetFullName() : "<none>"));
      Level->XLevel->CallingState = state;
      if (!MO) Sys_Error("PlayerPawn is dead (wtf?!)");
      {
        SavedVObjectPtr svp(&MO->_stateRouteSelf);
        MO->_stateRouteSelf = _stateRouteSelf; // always, 'cause player is The Authority
        /*
        if (!MO->_stateRouteSelf) {
          MO->_stateRouteSelf = _stateRouteSelf;
        } else {
          //GCon->Logf("player(%s), MO(%s)-viewobject: `%s`, state `%s` (at %s)", *GetClass()->GetFullName(), *MO->GetClass()->GetFullName(), MO->_stateRouteSelf->GetClass()->GetName(), *state->GetFullName(), *state->Loc.toStringNoCol());
        }
        */
        if (!MO->_stateRouteSelf /*&& position == 0*/) GCon->Logf("Player: viewobject is not set!");
        /*
        VObject::VMDumpCallStack();
        GCon->Logf("player(%s), MO(%s)-viewobject: `%s`, state `%s` (at %s)", *GetClass()->GetFullName(), *MO->GetClass()->GetFullName(), MO->_stateRouteSelf->GetClass()->GetName(), *state->GetFullName(), *state->Loc.toStringNoCol());
        */
        P_PASS_REF(MO);
        ExecuteFunctionNoArgs(state->Function);
      }
      if (!VSt.State) {
        DispSpriteFrame[position] = 0;
        DispSpriteName[position] = NAME_None;
        break;
      }
    }
    state = VSt.State->NextState;
  } while (!VSt.StateTime); // an initial state of 0 could cycle through
  //fprintf(stderr, "  VBasePlayer::SetViewState: DONE: position=%d; stnum=%s\n", position, (stnum ? *stnum->GetFullName() : "<none>"));
  if (!VSt.State) {
    LastViewObject[position] = nullptr;
    if (position == PS_WEAPON) LastViewObject[PS_WEAPON_OVL] = nullptr;
  }
}


//==========================================================================
//
//  VBasePlayer::AdvanceViewStates
//
//==========================================================================
void VBasePlayer::AdvanceViewStates (float deltaTime) {
  if (deltaTime <= 0.0f) return;
  for (unsigned i = 0; i < NUMPSPRITES; ++i) {
    VViewState &St = ViewStates[i];
    // a null state means not active
    if (St.State) {
      // drop tic count and possibly change state
      // a -1 tic count never changes
      if (St.StateTime != -1.0f) {
        St.StateTime -= deltaTime;
        if (eventCheckDoubleFiringSpeed()) {
          // [BC] Apply double firing speed
          St.StateTime -= deltaTime;
        }
        if (St.StateTime <= 0.0f) {
          St.StateTime = 0.0f;
          SetViewState(i, St.State->NextState);
        }
      }
    }
  }
}


//==========================================================================
//
//  VBasePlayer::SetUserInfo
//
//==========================================================================
void VBasePlayer::SetUserInfo (const VStr &info) {
  if (!sv_loading) {
    UserInfo = info;
    ReadFromUserInfo();
  }
}


//==========================================================================
//
//  VBasePlayer::ReadFromUserInfo
//
//==========================================================================
void VBasePlayer::ReadFromUserInfo () {
  if (!sv_loading) BaseClass = VStr::atoi(*Info_ValueForKey(UserInfo, "class"));
  PlayerName = Info_ValueForKey(UserInfo, "name");
  VStr val = Info_ValueForKey(UserInfo, "colour");
  Colour = M_ParseColour(*val);
  eventUserinfoChanged();
}


//==========================================================================
//
//  VBasePlayer::DoClientStartSound
//
//==========================================================================
void VBasePlayer::DoClientStartSound (int SoundId, TVec Org, int OriginId,
  int Channel, float Volume, float Attenuation, bool Loop)
{
#ifdef CLIENT
  GAudio->PlaySound(SoundId, Org, TVec(0, 0, 0), OriginId, Channel, Volume, Attenuation, Loop);
#endif
}


//==========================================================================
//
//  VBasePlayer::DoClientStopSound
//
//==========================================================================
void VBasePlayer::DoClientStopSound (int OriginId, int Channel) {
#ifdef CLIENT
  GAudio->StopSound(OriginId, Channel);
#endif
}


//==========================================================================
//
//  VBasePlayer::DoClientStartSequence
//
//==========================================================================
void VBasePlayer::DoClientStartSequence (TVec Origin, int OriginId, VName Name, int ModeNum) {
#ifdef CLIENT
  GAudio->StartSequence(OriginId, Origin, Name, ModeNum);
#endif
}


//==========================================================================
//
//  VBasePlayer::DoClientAddSequenceChoice
//
//==========================================================================
void VBasePlayer::DoClientAddSequenceChoice (int OriginId, VName Choice) {
#ifdef CLIENT
  GAudio->AddSeqChoice(OriginId, Choice);
#endif
}


//==========================================================================
//
//  VBasePlayer::DoClientStopSequence
//
//==========================================================================
void VBasePlayer::DoClientStopSequence (int OriginId) {
#ifdef CLIENT
  GAudio->StopSequence(OriginId);
#endif
}


//==========================================================================
//
//  VBasePlayer::DoClientPrint
//
//==========================================================================
void VBasePlayer::DoClientPrint (VStr AStr) {
  VStr Str(AStr);

  if (Str.IsEmpty()) return;
  if (Str[0] == '$') Str = GLanguage[*VStr(Str.ToLower(), 1, Str.Length()-1)];
  if (hud_msg_echo) GCon->Logf("\034S%s", *Str);

  ClGame->eventAddNotifyMessage(Str);
}


//==========================================================================
//
//  VBasePlayer::DoClientCentrePrint
//
//==========================================================================
void VBasePlayer::DoClientCentrePrint (VStr Str) {
  VStr Msg(Str);

  if (Msg.IsEmpty()) return;
  if (Msg[0] == '$') Msg = GLanguage[*VStr(Msg.ToLower(), 1, Msg.Length()-1)];
  if (hud_msg_echo) {
    //GCon->Log("<-------------------------------->");
    GCon->Logf("\034X%s", *Msg);
    //GCon->Log("<-------------------------------->");
  }

  ClGame->eventAddCentreMessage(Msg);
}


//==========================================================================
//
//  VBasePlayer::DoClientSetAngles
//
//==========================================================================
void VBasePlayer::DoClientSetAngles (TAVec Angles) {
  ViewAngles = Angles;
  ViewAngles.pitch = AngleMod180(ViewAngles.pitch);
}


//==========================================================================
//
//  VBasePlayer::DoClientIntermission
//
//==========================================================================
void VBasePlayer::DoClientIntermission (VName NextMap) {
  im_t &im = ClGame->im;

  im.Text.Clean();
  im.IMFlags = 0;

  const mapInfo_t &linfo = P_GetMapInfo(Level->XLevel->MapName);
  im.LeaveMap = Level->XLevel->MapName;
  im.LeaveCluster = linfo.Cluster;
  im.LeaveName = linfo.GetName();
  im.LeaveTitlePatch = linfo.TitlePatch;
  im.ExitPic = linfo.ExitPic;
  im.InterMusic = linfo.InterMusic;

  const mapInfo_t &einfo = P_GetMapInfo(NextMap);
  im.EnterMap = NextMap;
  im.EnterCluster = einfo.Cluster;
  im.EnterName = einfo.GetName();
  im.EnterTitlePatch = einfo.TitlePatch;
  im.EnterPic = einfo.EnterPic;

  if (linfo.Cluster != einfo.Cluster) {
    if (einfo.Cluster) {
      const VClusterDef *CDef = P_GetClusterDef(einfo.Cluster);
      if (CDef->EnterText.Length()) {
        if (CDef->Flags & CLUSTERF_LookupEnterText) {
          im.Text = GLanguage[*CDef->EnterText];
        } else {
          im.Text = CDef->EnterText;
        }
        if (CDef->Flags & CLUSTERF_EnterTextIsLump) im.IMFlags |= im_t::IMF_TextIsLump;
        if (CDef->Flags & CLUSTERF_FinalePic) {
          im.TextFlat = NAME_None;
          im.TextPic = CDef->Flat;
        } else {
          im.TextFlat = CDef->Flat;
          im.TextPic = NAME_None;
        }
        im.TextMusic = CDef->Music;
      }
    }
    if (im.Text.Length() == 0 && linfo.Cluster) {
      const VClusterDef *CDef = P_GetClusterDef(linfo.Cluster);
      if (CDef->ExitText.Length()) {
        if (CDef->Flags & CLUSTERF_LookupExitText) {
          im.Text = GLanguage[*CDef->ExitText];
        } else {
          im.Text = CDef->ExitText;
        }
        if (CDef->Flags & CLUSTERF_ExitTextIsLump) im.IMFlags |= im_t::IMF_TextIsLump;
        if (CDef->Flags & CLUSTERF_FinalePic) {
          im.TextFlat = NAME_None;
          im.TextPic = CDef->Flat;
        } else {
          im.TextFlat = CDef->Flat;
          im.TextPic = NAME_None;
        }
        im.TextMusic = CDef->Music;
      }
    }
  }

  ClGame->intermission = 1;
#ifdef CLIENT
  AM_Stop();
  GAudio->StopAllSequences();
#endif

  ClGame->eventIintermissionStart();
}


//==========================================================================
//
//  VBasePlayer::DoClientPause
//
//==========================================================================
void VBasePlayer::DoClientPause (bool Paused) {
#ifdef CLIENT
  if (Paused) {
    GGameInfo->Flags |= VGameInfo::GIF_Paused;
    GAudio->PauseSound();
  } else {
    GGameInfo->Flags &= ~VGameInfo::GIF_Paused;
    GAudio->ResumeSound();
  }
#endif
}


//==========================================================================
//
//  VBasePlayer::DoClientSkipIntermission
//
//==========================================================================
void VBasePlayer::DoClientSkipIntermission () {
  ClGame->ClientFlags |= VClientGameBase::CF_SkipIntermission;
}


//==========================================================================
//
//  VBasePlayer::DoClientFinale
//
//==========================================================================
void VBasePlayer::DoClientFinale (VStr Type) {
  ClGame->intermission = 2;
#ifdef CLIENT
  AM_Stop();
#endif
  ClGame->eventStartFinale(*Type);
}


//==========================================================================
//
//  VBasePlayer::DoClientChangeMusic
//
//==========================================================================
void VBasePlayer::DoClientChangeMusic (VName Song) {
  Level->SongLump = Song;
#ifdef CLIENT
  GAudio->MusicChanged();
#endif
}


//==========================================================================
//
//  VBasePlayer::DoClientSetServerInfo
//
//==========================================================================
void VBasePlayer::DoClientSetServerInfo (VStr Key, VStr Value) {
  Info_SetValueForKey(ClGame->serverinfo, Key, Value);
#ifdef CLIENT
  CL_ReadFromServerInfo();
#endif
}


//==========================================================================
//
//  VBasePlayer::DoClientHudMessage
//
//==========================================================================
void VBasePlayer::DoClientHudMessage (const VStr &Message, VName Font, int Type,
  int Id, int Colour, const VStr &ColourName, float x, float y,
  int HudWidth, int HudHeight, float HoldTime, float Time1, float Time2)
{
  ClGame->eventAddHudMessage(Message, Font, Type, Id, Colour, ColourName,
    x, y, HudWidth, HudHeight, HoldTime, Time1, Time2);
}


//==========================================================================
//
//  VBasePlayer::WriteViewData
//
//==========================================================================
void VBasePlayer::WriteViewData () {
  // update bam_angles (after teleportation)
  if (PlayerFlags&PF_FixAngle) {
    PlayerFlags &= ~PF_FixAngle;
    eventClientSetAngles(ViewAngles);
  }
}


//==========================================================================
//
//  VBasePlayer::CallDumpInventory
//
//==========================================================================
void VBasePlayer::CallDumpInventory () {
  VStr name = "DumpInventory";
  VMethod *mt = GetClass()->FindConCommandMethod(name);
  if (!mt) return;
  // i found her!
  if ((mt->Flags&FUNC_Static) == 0) P_PASS_SELF;
  (void)ExecuteFunction(mt);
}


//==========================================================================
//
//  VBasePlayer::ListConCommands
//
//  append player commands with the given prefix
//
//==========================================================================
void VBasePlayer::ListConCommands (TArray<VStr> &list, const VStr &pfx) {
  for (auto it = GetClass()->ConCmdListMts.first(); it; ++it) {
    const char *mtname = *it.getValue()->Name;
    mtname += 6;
    if (VStr::endsWithNoCase(mtname, "_AC")) continue;
    if (!pfx.isEmpty()) {
      if (!VStr::startsWithNoCase(mtname, *pfx)) continue;
    }
    list.append(VStr(mtname));
  }
}


//==========================================================================
//
//  VBasePlayer::IsConCommand
//
//==========================================================================
bool VBasePlayer::IsConCommand (const VStr &name) {
  return !!GetClass()->FindConCommandMethod(name);
}


//==========================================================================
//
//  VBasePlayer::ExecConCommand
//
//  returns `true` if command was found and executed
//  uses VCommand command line
//
//==========================================================================
bool VBasePlayer::ExecConCommand () {
  if (VCommand::GetArgC() < 1) return false;
  VStr name = VCommand::GetArgV(0);
  VMethod *mt = GetClass()->FindConCommandMethod(name);
  if (!mt) return false;
  // i found her!
  if ((mt->Flags&FUNC_Static) == 0) P_PASS_SELF;
  (void)ExecuteFunction(mt);
  return true;
}


//==========================================================================
//
//  VBasePlayer::ExecConCommandAC
//
//  returns `true` if command was found (autocompleter may be still missing)
//  autocompleter should filter list
//
//==========================================================================
bool VBasePlayer::ExecConCommandAC (TArray<VStr> &args, bool newArg, TArray<VStr> &aclist) {
  if (args.length() < 1) return false;
  VStr name = args[0];
  if (name.isEmpty()) return false;
  VMethod *mt = GetClass()->FindConCommandMethodExact(name+"_AC");
  if (mt) {
    // i found her!
    // build command line
    //args.removeAt(0); // remove command name
    if ((mt->Flags&FUNC_Static) == 0) P_PASS_SELF;
    P_PASS_PTR((void *)&args);
    P_PASS_INT(newArg ? 1 : 0);
    P_PASS_PTR((void *)&aclist);
    (void)ExecuteFunction(mt);
    return true;
  }
  return !!GetClass()->FindConCommandMethod(name);  // has such cheat?
}


//==========================================================================
//
//  VBasePlayer::ResetButtons
//
//==========================================================================
void VBasePlayer::ResetButtons () {
  ForwardMove = 0;
  SideMove = 0;
  FlyMove = 0;
  Buttons = 0;
  Impulse = 0;
  AcsCurrButtonsPressed = 0;
  AcsCurrButtons = 0;
  AcsButtons = 0;
  OldButtons = 0;
  AcsNextButtonUpdate = 0;
  AcsPrevMouseX = 0;
  AcsPrevMouseY = 0;
  AcsMouseX = 0;
  AcsMouseY = 0;
  // reset "down" flags
  PlayerFlags &= ~VBasePlayer::PF_AttackDown;
  PlayerFlags &= ~VBasePlayer::PF_UseDown;
}


//==========================================================================
//
//  COMMAND SetInfo
//
//==========================================================================
COMMAND(SetInfo) {
  if (Source != SRC_Client) {
    GCon->Log("SetInfo is not valid from console");
    return;
  }

  if (Args.Num() != 3) return;

  Info_SetValueForKey(Player->UserInfo, *Args[1], *Args[2]);
  Player->ReadFromUserInfo();
}


//==========================================================================
//
//  Natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VBasePlayer, get_IsCheckpointSpawn) {
  RET_BOOL(isCheckpointSpawn);
}


IMPLEMENT_FUNCTION(VBasePlayer, cprint) {
  VStr msg = PF_FormatString();
  P_GET_SELF;
  Self->eventClientPrint(*msg);
}

IMPLEMENT_FUNCTION(VBasePlayer, centreprint) {
  VStr msg = PF_FormatString();
  P_GET_SELF;
  Self->eventClientCentrePrint(*msg);
}

IMPLEMENT_FUNCTION(VBasePlayer, GetPlayerNum) {
  P_GET_SELF;
  RET_INT(SV_GetPlayerNum(Self));
}

IMPLEMENT_FUNCTION(VBasePlayer, ClearPlayer) {
  P_GET_SELF;

  Self->PClass = 0;
  Self->ForwardMove = 0;
  Self->SideMove = 0;
  Self->FlyMove = 0;
  Self->Buttons = 0;
  Self->Impulse = 0;
  Self->AcsCurrButtonsPressed = 0;
  Self->AcsCurrButtons = 0;
  Self->AcsButtons = 0;
  Self->OldButtons = 0;
  Self->AcsNextButtonUpdate = 0;
  Self->AcsPrevMouseX = 0;
  Self->AcsPrevMouseY = 0;
  Self->AcsMouseX = 0;
  Self->AcsMouseY = 0;
  Self->MO = nullptr;
  Self->PlayerState = 0;
  Self->ViewOrg = TVec(0, 0, 0);
  Self->PlayerFlags &= ~VBasePlayer::PF_FixAngle;
  Self->Health = 0;
  Self->PlayerFlags &= ~VBasePlayer::PF_AttackDown;
  Self->PlayerFlags &= ~VBasePlayer::PF_UseDown;
  Self->PlayerFlags &= ~VBasePlayer::PF_AutomapRevealed;
  Self->PlayerFlags &= ~VBasePlayer::PF_AutomapShowThings;
  Self->ExtraLight = 0;
  Self->FixedColourmap = 0;
  Self->CShift = 0;
  Self->PSpriteSY = 0;
  Self->PSpriteWeaponLowerPrev = 0;
  Self->PSpriteWeaponLoweringStartTime = 0;
  Self->PSpriteWeaponLoweringDuration = 0;
  memset((void *)Self->LastViewObject, 0, sizeof(Self->LastViewObject));

  vuint8 *Def = Self->GetClass()->Defaults;
  for (VField *F = Self->GetClass()->Fields; F; F = F->Next)
  {
    VField::CopyFieldValue(Def + F->Ofs, (vuint8*)Self + F->Ofs, F->Type);
  }
}

IMPLEMENT_FUNCTION(VBasePlayer, SetViewObject) {
  P_GET_PTR(VObject, vobj);
  P_GET_SELF;
  //if (!vobj) GCon->Logf("RESET VIEW OBJECT; WTF?!");
  if (Self) Self->_stateRouteSelf = vobj;
}

IMPLEMENT_FUNCTION(VBasePlayer, SetViewObjectIfNone) {
  P_GET_PTR(VObject, vobj);
  P_GET_SELF;
  if (Self && !Self->_stateRouteSelf) Self->_stateRouteSelf = vobj;
}

IMPLEMENT_FUNCTION(VBasePlayer, SetViewState) {
  //fprintf(stderr, "*** SVS ***\n");
  P_GET_PTR(VState, stnum);
  P_GET_INT(position);
  P_GET_SELF;
  Self->SetViewState(position, stnum);
}

IMPLEMENT_FUNCTION(VBasePlayer, AdvanceViewStates) {
  P_GET_FLOAT(deltaTime);
  P_GET_SELF;
  Self->AdvanceViewStates(deltaTime);
}

IMPLEMENT_FUNCTION(VBasePlayer, DisconnectBot) {
  P_GET_SELF;
  check(Self->PlayerFlags & PF_IsBot);
  SV_DropClient(Self, false);
}

IMPLEMENT_FUNCTION(VBasePlayer, ClientStartSound) {
  P_GET_BOOL(Loop);
  P_GET_FLOAT(Attenuation);
  P_GET_FLOAT(Volume);
  P_GET_INT(Channel);
  P_GET_INT(OriginId);
  P_GET_VEC(Org);
  P_GET_INT(SoundId);
  P_GET_SELF;
  Self->DoClientStartSound(SoundId, Org, OriginId, Channel, Volume,
    Attenuation, Loop);
}

IMPLEMENT_FUNCTION(VBasePlayer, ClientStopSound) {
  P_GET_INT(Channel);
  P_GET_INT(OriginId);
  P_GET_SELF;
  Self->DoClientStopSound(OriginId, Channel);
}

IMPLEMENT_FUNCTION(VBasePlayer, ClientStartSequence) {
  P_GET_INT(ModeNum);
  P_GET_NAME(Name);
  P_GET_INT(OriginId);
  P_GET_VEC(Origin);
  P_GET_SELF;
  Self->DoClientStartSequence(Origin, OriginId, Name, ModeNum);
}

IMPLEMENT_FUNCTION(VBasePlayer, ClientAddSequenceChoice) {
  P_GET_NAME(Choice);
  P_GET_INT(OriginId);
  P_GET_SELF;
  Self->DoClientAddSequenceChoice(OriginId, Choice);
}

IMPLEMENT_FUNCTION(VBasePlayer, ClientStopSequence) {
  P_GET_INT(OriginId);
  P_GET_SELF;
  Self->DoClientStopSequence(OriginId);
}

IMPLEMENT_FUNCTION(VBasePlayer, ClientPrint) {
  P_GET_STR(Str);
  P_GET_SELF;
  Self->DoClientPrint(Str);
}

IMPLEMENT_FUNCTION(VBasePlayer, ClientCentrePrint) {
  P_GET_STR(Str);
  P_GET_SELF;
  Self->DoClientCentrePrint(Str);
}

IMPLEMENT_FUNCTION(VBasePlayer, ClientSetAngles) {
  P_GET_AVEC(Angles);
  P_GET_SELF;
  Self->DoClientSetAngles(Angles);
}

IMPLEMENT_FUNCTION(VBasePlayer, ClientIntermission) {
  P_GET_NAME(NextMap);
  P_GET_SELF;
  Self->DoClientIntermission(NextMap);
}

IMPLEMENT_FUNCTION(VBasePlayer, ClientPause) {
  P_GET_BOOL(Paused);
  P_GET_SELF;
  Self->DoClientPause(Paused);
}

IMPLEMENT_FUNCTION(VBasePlayer, ClientSkipIntermission) {
  P_GET_SELF;
  Self->DoClientSkipIntermission();
}

IMPLEMENT_FUNCTION(VBasePlayer, ClientFinale) {
  P_GET_STR(Type);
  P_GET_SELF;
  Self->DoClientFinale(Type);
}

IMPLEMENT_FUNCTION(VBasePlayer, ClientChangeMusic) {
  P_GET_NAME(Song);
  P_GET_SELF;
  Self->DoClientChangeMusic(Song);
}

IMPLEMENT_FUNCTION(VBasePlayer, ClientSetServerInfo) {
  P_GET_STR(Value);
  P_GET_STR(Key);
  P_GET_SELF;
  Self->DoClientSetServerInfo(Key, Value);
}

IMPLEMENT_FUNCTION(VBasePlayer, ClientHudMessage) {
  P_GET_FLOAT(Time2);
  P_GET_FLOAT(Time1);
  P_GET_FLOAT(HoldTime);
  P_GET_INT(HudHeight);
  P_GET_INT(HudWidth);
  P_GET_FLOAT(y);
  P_GET_FLOAT(x);
  P_GET_STR(ColourName);
  P_GET_INT(Colour);
  P_GET_INT(Id);
  P_GET_INT(Type);
  P_GET_NAME(Font);
  P_GET_STR(Message);
  P_GET_SELF;
  Self->DoClientHudMessage(Message, Font, Type, Id, Colour, ColourName,
    x, y, HudWidth, HudHeight, HoldTime, Time1, Time2);
}

IMPLEMENT_FUNCTION(VBasePlayer, ServerSetUserInfo) {
  P_GET_STR(Info);
  P_GET_SELF;
  Self->SetUserInfo(Info);
}


// native final void QS_PutInt (name fieldname, int value);
IMPLEMENT_FUNCTION(VBasePlayer, QS_PutInt) {
  P_GET_INT(value);
  P_GET_STR(name);
  P_GET_SELF;
  (void)Self;
  QS_PutValue(QSValue::CreateInt(nullptr, name, value));
}

// native final void QS_PutName (name fieldname, name value);
IMPLEMENT_FUNCTION(VBasePlayer, QS_PutName) {
  P_GET_NAME(value);
  P_GET_STR(name);
  P_GET_SELF;
  (void)Self;
  QS_PutValue(QSValue::CreateName(nullptr, name, value));
}

// native final void QS_PutStr (name fieldname, string value);
IMPLEMENT_FUNCTION(VBasePlayer, QS_PutStr) {
  P_GET_STR(value);
  P_GET_STR(name);
  P_GET_SELF;
  (void)Self;
  QS_PutValue(QSValue::CreateStr(nullptr, name, value));
}

// native final void QS_PutFloat (name fieldname, float value);
IMPLEMENT_FUNCTION(VBasePlayer, QS_PutFloat) {
  P_GET_FLOAT(value);
  P_GET_STR(name);
  P_GET_SELF;
  (void)Self;
  QS_PutValue(QSValue::CreateFloat(nullptr, name, value));
}


// native final int QS_GetInt (name fieldname, optional int defvalue);
IMPLEMENT_FUNCTION(VBasePlayer, QS_GetInt) {
  P_GET_INT_OPT(value, 0);
  P_GET_STR(name);
  P_GET_SELF;
  (void)Self;
  QSValue ret = QS_GetValue(nullptr, name);
  if (ret.type != QSType::QST_Int) {
    if (!specified_value) Host_Error("value '%s' not found for player", *name);
    ret.ival = value;
  }
  RET_INT(ret.ival);
}

// native final name QS_GetName (name fieldname, optional name defvalue);
IMPLEMENT_FUNCTION(VBasePlayer, QS_GetName) {
  P_GET_NAME_OPT(value, NAME_None);
  P_GET_STR(name);
  P_GET_SELF;
  (void)Self;
  QSValue ret = QS_GetValue(nullptr, name);
  if (ret.type != QSType::QST_Name) {
    if (!specified_value) Host_Error("value '%s' not found for player", *name);
    ret.nval = value;
  }
  RET_NAME(ret.nval);
}

// native final string QS_GetStr (name fieldname, optional string defvalue);
IMPLEMENT_FUNCTION(VBasePlayer, QS_GetStr) {
  P_GET_STR_OPT(value, VStr::EmptyString);
  P_GET_STR(name);
  P_GET_SELF;
  (void)Self;
  QSValue ret = QS_GetValue(nullptr, name);
  if (ret.type != QSType::QST_Str) {
    if (!specified_value) Host_Error("value '%s' not found for player", *name);
    ret.sval = value;
  }
  RET_STR(ret.sval);
}

// native final float QS_GetFloat (name fieldname, optional float defvalue);
IMPLEMENT_FUNCTION(VBasePlayer, QS_GetFloat) {
  P_GET_FLOAT_OPT(value, 0.0f);
  P_GET_STR(name);
  P_GET_SELF;
  (void)Self;
  QSValue ret = QS_GetValue(nullptr, name);
  if (ret.type != QSType::QST_Float) {
    if (!specified_value) Host_Error("value '%s' not found for player", *name);
    ret.fval = value;
  }
  RET_FLOAT(ret.fval);
}
