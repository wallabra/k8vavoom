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
#ifndef CL_LOCAL_HEADER
#define CL_LOCAL_HEADER

#include "iline.h"    //  Input line widget


class VRootWidget;
struct VModel;


struct dlight_t {
  TVec origin; // origin of the light
  float radius; // radius - how far light goes
  float die; // stop lighting after this time
  float decay; // drop this each second
  float minlight; // don't add when contributing less
  /*DLType*/vint32 type;
  vuint32 color; // for colored lights
  vuint32 ownerUId; // used to identify owner to reuse the same light
  vint32 lightid;
  TVec coneDirection;
  float coneAngle; // 0 means "point light", otherwise it is spotlight
  TVec origOrigin;
  // flags
  vuint32 flags;
  enum {
    PlayerLight  = 1u<<0, // set in alloc, player lights should survive
    NoSelfShadow = 1u<<1,
    NoShadow     = 1u<<2,
  };
};


struct IntermissionText {
  VStr Text;
  VName TextFlat;
  VName TextPic;
  VName TextMusic;

  enum {
    IMF_TextIsLump = 0x01,
  };
  vuint32 Flags;

  IntermissionText () : Text(), TextFlat(NAME_None), TextPic(NAME_None), TextMusic(NAME_None), Flags(0) {}
  inline void clear () { Text.clear(); TextFlat = NAME_None; TextPic = NAME_None; TextMusic = NAME_None; Flags = 0; }
  inline bool isActive () const { return !Text.isEmpty(); }
};


struct IntermissionInfo {
  VName LeaveMap;
  vint32 LeaveCluster;
  VStr LeaveName;
  VName LeaveTitlePatch;
  VName ExitPic;

  VName EnterMap;
  vint32 EnterCluster;
  VStr EnterName;
  VName EnterTitlePatch;
  VName EnterPic;

  VName InterMusic;

  IntermissionText LeaveText;
  IntermissionText EnterText;
};


// ////////////////////////////////////////////////////////////////////////// //
class VClientGameBase : public VObject {
  DECLARE_CLASS(VClientGameBase, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VClientGameBase)

  enum {
    CF_LocalServer      = 0x01,
    // used to accelerate or skip a stage
    CF_SkipIntermission = 0x02,
  };
  vuint32 ClientFlags;

  VGameInfo *Game;
  VBasePlayer *cl;
  VLevel *GLevel;

  IntermissionInfo im;

  VRootWidget *GRoot;

  vint32 sb_height;

  vint32 maxclients;
  vint32 deathmatch;

  VStr serverinfo;

  enum /*IM_Phase*/ {
    IM_Phase_None, // not in intermission
    IM_Phase_Leave,
    IM_Phase_Enter,
    IM_Phase_Finale,
  };

  vint32 intermissionPhase;

protected:
  void eventIntermissionStart () { static VMethodProxy method("IntermissionStart"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventStartFinale (VName FinaleType) { static VMethodProxy method("StartFinale"); vobjPutParamSelf(FinaleType); VMT_RET_VOID(method); }

public:
  //VClientGameBase () : serverinfo(E_NoInit) {}

  inline bool InIntermission () const { return (intermissionPhase != IM_Phase_None); }
  inline bool InFinale () const { return (intermissionPhase == IM_Phase_Finale); }

  inline void ResetIntermission () { intermissionPhase = IM_Phase_None; }

  inline void StartIntermissionLeave () { intermissionPhase = IM_Phase_Leave; eventIntermissionStart(); }
  inline void StartIntermissionEnter () { intermissionPhase = IM_Phase_Enter; eventIntermissionStart(); }
  inline void StartFinale (VName FinaleType) { intermissionPhase = IM_Phase_Finale; eventStartFinale(FinaleType); }

  void eventPostSpawn () { static VMethodProxy method("PostSpawn"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventRootWindowCreated () { static VMethodProxy method("RootWindowCreated"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventConnected () { static VMethodProxy method("Connected"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventDisconnected () { static VMethodProxy method("Disconnected"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventDemoPlaybackStarted () { static VMethodProxy method("DemoPlaybackStarted"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventDemoPlaybackStopped () { static VMethodProxy method("DemoPlaybackStopped"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventOnHostEndGame () { static VMethodProxy method("OnHostEndGame"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventOnHostError () { static VMethodProxy method("OnHostError"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventStatusBarStartMap () { static VMethodProxy method("StatusBarStartMap"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventStatusBarDrawer (int sb_type) { static VMethodProxy method("StatusBarDrawer"); vobjPutParamSelf(sb_type); VMT_RET_VOID(method); }
  void eventStatusBarUpdateWidgets (float DeltaTime) { if (DeltaTime <= 0.0f) return; static VMethodProxy method("StatusBarUpdateWidgets"); vobjPutParamSelf(DeltaTime); VMT_RET_VOID(method); }
  bool eventFinaleResponder (event_t *event) { static VMethodProxy method("FinaleResponder"); vobjPutParamSelf(event); VMT_RET_BOOL(method); }
  void eventDeactivateMenu () { static VMethodProxy method("DeactivateMenu"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  bool eventMenuResponder (event_t *event) { static VMethodProxy method("MenuResponder"); vobjPutParamSelf(event); VMT_RET_BOOL(method); }
  bool eventMenuActive () { static VMethodProxy method("MenuActive"); vobjPutParamSelf(); VMT_RET_BOOL(method); }
  void eventSetMenu (VStr Name) { static VMethodProxy method("SetMenu"); vobjPutParamSelf(Name); VMT_RET_VOID(method); }
  void eventGetAllMenuNames (TArray<VStr> &list, int mode) { static VMethodProxy method("GetAllMenuNames"); vobjPutParamSelf(&list, mode); VMT_RET_VOID(method); }

  void eventMessageBoxShowWarning (VStr text) { static VMethodProxy method("MessageBoxShowWarning"); vobjPutParamSelf(text); VMT_RET_VOID(method); }
  void eventMessageBoxDrawer () { static VMethodProxy method("MessageBoxDrawer"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  bool eventMessageBoxResponder (event_t *event) { static VMethodProxy method("MessageBoxResponder"); vobjPutParamSelf(event); VMT_RET_BOOL(method); }
  bool eventMessageBoxActive () { static VMethodProxy method("MessageBoxActive"); vobjPutParamSelf(); VMT_RET_BOOL(method); }

  void eventDrawViewBorder (int x, int y, int w, int h) { static VMethodProxy method("DrawViewBorder"); vobjPutParamSelf(x, y, w, h); VMT_RET_VOID(method); }
  void eventAddNotifyMessage (VStr Str) { static VMethodProxy method("AddNotifyMessage"); vobjPutParamSelf(Str); VMT_RET_VOID(method); }
  void eventAddChatMessage (VStr nick, VStr text) { static VMethodProxy method("AddChatMessage"); vobjPutParamSelf(nick, text); VMT_RET_VOID(method); }
  void eventAddCenterMessage (VStr Msg) { static VMethodProxy method("AddCenterMessage"); vobjPutParamSelf(Msg); VMT_RET_VOID(method); }

  void eventAddHudMessage (VStr Message, VName Font, int Type, int Id,
                           int Color, VStr ColorName, float x, float y,
                           int HudWidth, int HudHeight, float HoldTime,
                           float Time1, float Time2)
  {
    static VMethodProxy method("AddHudMessage");
    vobjPutParamSelf(Message, Font, Type, Id, Color, ColorName, x, y, HudWidth, HudHeight, HoldTime, Time1, Time2);
    VMT_RET_VOID(method);
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct VNetClientServerInfo;

void CL_DecayLights ();

void CL_NetworkHeartbeat (bool forced=false);
void CL_ParseServerInfo (const VNetClientServerInfo *sinfo);
void CL_ReadFromServerInfo ();
void CL_StopRecording ();

void R_DrawModelFrame (const TVec &, float, VModel *, int, int, const char *, int, int, int, float);

VModel *Mod_FindName (VStr);

void SCR_SetVirtualScreen (int, int);


// ////////////////////////////////////////////////////////////////////////// //
extern VClientGameBase *GClGame;

extern int VirtualWidth;
extern int VirtualHeight;

extern float fScaleX;
extern float fScaleY;

extern bool UserInfoSent;

#endif
