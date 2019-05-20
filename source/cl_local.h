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
  VThinker *Owner; // used to identify owner to reuse the same light
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


struct im_t {
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

  VStr Text;
  VName TextFlat;
  VName TextPic;
  VName TextMusic;

  enum {
    IMF_TextIsLump = 0x01,
  };
  vint32 IMFlags;
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

  im_t im;

  VRootWidget *GRoot;

  int sb_height;

  int maxclients;
  int deathmatch;

  VStr serverinfo;

  int intermission;

public:
  //VClientGameBase () : serverinfo(E_NoInit) {}

  void eventPostSpawn () { static VMethodProxy method("PostSpawn"); vobjPutParam(this); VMT_RET_VOID(method); }
  void eventRootWindowCreated () { static VMethodProxy method("RootWindowCreated"); vobjPutParam(this); VMT_RET_VOID(method); }
  void eventConnected () { static VMethodProxy method("Connected"); vobjPutParam(this); VMT_RET_VOID(method); }
  void eventDisconnected () { static VMethodProxy method("Disconnected"); vobjPutParam(this); VMT_RET_VOID(method); }
  void eventDemoPlaybackStarted () { static VMethodProxy method("DemoPlaybackStarted"); vobjPutParam(this); VMT_RET_VOID(method); }
  void eventDemoPlaybackStopped () { static VMethodProxy method("DemoPlaybackStopped"); vobjPutParam(this); VMT_RET_VOID(method); }
  void eventOnHostEndGame () { static VMethodProxy method("OnHostEndGame"); vobjPutParam(this); VMT_RET_VOID(method); }
  void eventOnHostError () { static VMethodProxy method("OnHostError"); vobjPutParam(this); VMT_RET_VOID(method); }
  void eventStatusBarStartMap () { static VMethodProxy method("StatusBarStartMap"); vobjPutParam(this); VMT_RET_VOID(method); }
  void eventStatusBarDrawer (int sb_type) { static VMethodProxy method("StatusBarDrawer"); vobjPutParam(this, sb_type); VMT_RET_VOID(method); }
  void eventStatusBarUpdateWidgets (float DeltaTime) { if (DeltaTime <= 0.0f) return; static VMethodProxy method("StatusBarUpdateWidgets"); vobjPutParam(this, DeltaTime); VMT_RET_VOID(method); }
  void eventIintermissionStart () { static VMethodProxy method("IintermissionStart"); vobjPutParam(this); VMT_RET_VOID(method); }
  void eventStartFinale (VName FinaleType) { static VMethodProxy method("StartFinale"); vobjPutParam(this, FinaleType); VMT_RET_VOID(method); }
  bool eventFinaleResponder (event_t *event) { static VMethodProxy method("FinaleResponder"); vobjPutParam(this, event); VMT_RET_BOOL(method); }
  void eventDeactivateMenu () { static VMethodProxy method("DeactivateMenu"); vobjPutParam(this); VMT_RET_VOID(method); }
  bool eventMenuResponder (event_t *event) { static VMethodProxy method("MenuResponder"); vobjPutParam(this, event); VMT_RET_BOOL(method); }
  bool eventMenuActive () { static VMethodProxy method("MenuActive"); vobjPutParam(this); VMT_RET_BOOL(method); }
  void eventSetMenu (const VStr &Name) { static VMethodProxy method("SetMenu"); vobjPutParam(this, Name); VMT_RET_VOID(method); }

  void eventMessageBoxDrawer () { static VMethodProxy method("MessageBoxDrawer"); vobjPutParam(this); VMT_RET_VOID(method); }
  bool eventMessageBoxResponder (event_t *event) { static VMethodProxy method("MessageBoxResponder"); vobjPutParam(this, event); VMT_RET_BOOL(method); }
  bool eventMessageBoxActive () { static VMethodProxy method("MessageBoxActive"); vobjPutParam(this); VMT_RET_BOOL(method); }

  void eventDrawViewBorder (int x, int y, int w, int h) { static VMethodProxy method("DrawViewBorder"); vobjPutParam(this, x, y, w, h); VMT_RET_VOID(method); }
  void eventAddNotifyMessage (const VStr &Str) { static VMethodProxy method("AddNotifyMessage"); vobjPutParam(this, Str); VMT_RET_VOID(method); }
  void eventAddCentreMessage (const VStr &Msg) { static VMethodProxy method("AddCentreMessage"); vobjPutParam(this, Msg); VMT_RET_VOID(method); }

  void eventAddHudMessage (const VStr &Message, VName Font, int Type, int Id,
                           int Color, const VStr &ColorName, float x, float y,
                           int HudWidth, int HudHeight, float HoldTime,
                           float Time1, float Time2)
  {
    static VMethodProxy method("AddHudMessage");
    vobjPutParam(this, Message, Font, Type, Id, Color, ColorName, x, y, HudWidth, HudHeight, HoldTime, Time1, Time2);
    VMT_RET_VOID(method);
  }
};


// ////////////////////////////////////////////////////////////////////////// //
void CL_DecayLights ();

void CL_KeepaliveMessage ();
void CL_KeepaliveMessageEx (double currTime, bool forced=false);
void CL_ParseServerInfo (class VMessageIn &msg);
void CL_ReadFromServerInfo ();
void CL_StopRecording ();

void R_DrawModelFrame (const TVec&, float, VModel*, int, int, const char*, int, int, int, float);

VModel *Mod_FindName (const VStr&);

void SCR_SetVirtualScreen (int, int);


// ////////////////////////////////////////////////////////////////////////// //
extern VClientGameBase *GClGame;

extern int VirtualWidth;
extern int VirtualHeight;

extern float fScaleX;
extern float fScaleY;
//extern float fScaleXI;
//extern float fScaleYI;

extern bool UserInfoSent;

#endif
