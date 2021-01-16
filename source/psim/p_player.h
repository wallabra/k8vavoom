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
//**  Copyright (C) 2018-2021 Ketmar Dark
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
class VNetConnection;
class VClientGameBase;

// constants for FixedColormap
enum {
  NUMCOLORMAPS    = 32,
  INVERSECOLORMAP = 32,
  GOLDCOLORMAP    = 33,
  REDCOLORMAP     = 34,
  GREENCOLORMAP   = 35,
  MONOCOLORMAP    = 36,
  BEREDCOLORMAP   = 37,
  BLUECOLORMAP    = 38,
  INVERSXCOLORMAP = 39,
  COLORMAPS_MAX,
};


// Overlay psprites are scaled shapes
// drawn directly on the view screen,
// coordinates are given for a 320*200 view screen.
enum psprnum_t {
  PS_WEAPON, // this MUST be before overlays
  PS_FLASH, // only DOOM uses it
  PS_WEAPON_OVL, // temporary hack
  PS_WEAPON_OVL_BACK, // hack: this overlay will be drawn first
  NUMPSPRITES
};

extern const int VPSpriteRenderOrder[NUMPSPRITES];

// player states
enum playerstate_t {
  PST_LIVE, // playing or camping
  PST_DEAD, // dead on the ground, view follows killer
  PST_REBORN, // ready to restart/respawn
  PST_CHEAT_REBORN, // do not reload level, do not reset inventory
};

// button/action code definitions
enum {
  BT_ATTACK      = 1u<<0, // press "fire"
  BT_USE         = 1u<<1, // use button, to open doors, activate switches
  BT_JUMP        = 1u<<2,
  BT_ALT_ATTACK  = 1u<<3,
  BT_BUTTON_5    = 1u<<4,
  BT_BUTTON_6    = 1u<<5,
  BT_BUTTON_7    = 1u<<6,
  BT_BUTTON_8    = 1u<<7,
  BT_RELOAD      = 1u<<8,
  BT_SPEED       = 1u<<9,
  BT_STRAFE      = 1u<<10,
  BT_CROUCH      = 1u<<11,
  BT_MOVELEFT    = 1u<<12,
  BT_MOVERIGHT   = 1u<<13,
  BT_LEFT        = 1u<<14,
  BT_RIGHT       = 1u<<15,
  BT_FORWARD     = 1u<<16,
  BT_BACKWARD    = 1u<<17,
  BT_FLASHLIGHT  = 1u<<18,
  BT_SUPERBULLET = 1u<<19,
  BT_ZOOM        = 1u<<20,
};

struct VViewState {
  VState *State;
  float StateTime;
  float SX, SY;
  float OfsX, OfsY;
  float BobOfsX, BobOfsY;
};

// extended player object info: player_t
class VBasePlayer : public VGameObject {
  DECLARE_CLASS(VBasePlayer, VGameObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VBasePlayer)

  enum { TOCENTRE = -128 };

  VLevelInfo *Level;

  enum {
    PF_Active            = 1u<<0,
    PF_Spawned           = 1u<<1,
    PF_IsBot             = 1u<<2,
    PF_FixAngle          = 1u<<3,
    PF_AttackDown        = 1u<<4, // set if button was down last tic
    PF_UseDown           = 1u<<5,
    PF_DidSecret         = 1u<<6, // set if secret level has been done
    PF_Centering         = 1u<<7, // player view centering in progress
    PF_IsClient          = 1u<<8, // this player is on dumb client side
    PF_AutomapRevealed   = 1u<<9,
    PF_AutomapShowThings = 1u<<10,
    PF_ReloadQueued      = 1u<<11,
    PF_ReloadDown        = 1u<<12, // set if button was down last tic
    PF_ZoomDown          = 1u<<13,
    PF_AltAttackDown     = 1u<<14,
    // this flag is set for client games if the player is using prediction engine (not yet)
    PF_AutonomousProxy   = 1u<<15,
  };
  vuint32 PlayerFlags;

  VNetConnection *Net;

  // set in `A_WeaponReady()`, processed in player tick
  enum {
    WAF_ALLOW_SWITCH       = 1u<<0,
    WAF_ALLOW_PRIMARY_FIRE = 1u<<1,
    WAF_ALLOW_ALT_FIRE     = 1u<<2,
    WAF_ALLOW_RELOAD       = 1u<<3,
    WAF_ALLOW_ZOOM         = 1u<<4,
    WAF_ALLOW_USER1        = 1u<<5,
    WAF_ALLOW_USER2        = 1u<<6,
    WAF_ALLOW_USER3        = 1u<<7,
    WAF_ALLOW_USER4        = 1u<<8,
    WAF_REFIRE_FLAG        = 1u<<9,
  };
  vuint32 WeaponActionFlags;
  // set in `A_Refire()`
  VState *WeaponRefireState;

  VStr UserInfo;

  VStr PlayerName;
  vuint8 BaseClass;
  vuint8 PClass; // player class type
  vuint8 TranslStart;
  vuint8 TranslEnd;
  vint32 Color;

  float ClientForwardMove; // *2048 for move
  float ClientSideMove; // *2048 for move
  float ForwardMove; // *2048 for move
  float SideMove; // *2048 for move
  float FlyMove; // fly up/down/centering
  /*vuint8*/vuint32 Buttons; // fire, use
  /*vuint8*/vuint32 Impulse; // weapon changes, inventory, etc
  // for ACS
  vuint32 AcsCurrButtonsPressed; // was pressed after last copy
  vuint32 AcsCurrButtons; // current state
  vuint32 AcsButtons; // what ACS will see
  vuint32 OldButtons; // previous state ACS will see
  float AcsNextButtonUpdate; // time left before copying AcsCurrButtons to AcsButtons
  float AcsPrevMouseX, AcsPrevMouseY; // previous ACS mouse movement
  float AcsMouseX, AcsMouseY; // current ACS mouse movement

  VEntity *MO;
  VEntity *Camera;
  vint32 PlayerState;

  // determine POV, including viewpoint bobbing during movement
  // focal origin above r.z
  TVec ViewOrg;

  TAVec ViewAngles;

  // this is only used between levels
  // mo->health is used during levels
  vint32 Health;

  // frags, kills of other players
  vint32 Frags;
  vint32 Deaths;

  // for intermission stats
  vint32 KillCount;
  vint32 ItemCount;
  vint32 SecretCount;

  // so gun flashes light up areas
  vuint8 ExtraLight;

  // for lite-amp and invulnarability powers
  vuint8 FixedColormap;

  // color shifts for damage, powerups and content types
  vuint32 CShift;

  // overlay view sprites (gun, etc)
  VViewState ViewStates[NUMPSPRITES];
  vint32 DispSpriteFrame[NUMPSPRITES]; // see entity code for explanation
  VName DispSpriteName[NUMPSPRITES]; // see entity code for explanation
  float PSpriteSY;
  float PSpriteWeaponLowerPrev;
  float PSpriteWeaponLoweringStartTime;
  float PSpriteWeaponLoweringDuration;

  // see "BasePlayer.vc" for explanations
  float WorldTimer; // total time the player's been playing
  float GameTime;
  float LastDeltaTime;

  float ClLastGameTime;
  float ClCurrGameTime;

  vuint8 ClientNum;

  vint32 SoundEnvironment;

  VClientGameBase *ClGame;

  VPlayerReplicationInfo *PlayerReplicationInfo;

  VObject *LastViewObject;

  vuint32 setStateWatchCat[NUMPSPRITES];
  VState *setStateNewState[NUMPSPRITES]; // if we are inside of `SetState()`, just set this, and get out; cannot be `nullptr`

  static bool isCheckpointSpawn;

private:
  inline void UpdateDispFrameFrom (int idx, const VState *st) {
    if (st) {
      if ((st->Frame&VState::FF_KEEPSPRITE) == 0 && st->SpriteIndex != 1) {
        DispSpriteFrame[idx] = (DispSpriteFrame[idx]&~0x00ffffff)|(st->SpriteIndex&0x00ffffff);
        DispSpriteName[idx] = st->SpriteName;
      }
      if ((st->Frame&VState::FF_DONTCHANGE) == 0) DispSpriteFrame[idx] = (DispSpriteFrame[idx]&0x00ffffff)|((st->Frame&VState::FF_FRAMEMASK)<<24);
    }
  }

public:
  //VBasePlayer () : UserInfo(E_NoInit), PlayerName(E_NoInit) {}
  virtual void PostCtor () override;
  //virtual void ClearReferences () override;

  void ResetButtons ();

  bool IsRunEnabled () const noexcept;
  bool IsMLookEnabled () const noexcept;
  bool IsCrouchEnabled () const noexcept;
  bool IsJumpEnabled () const noexcept;

  inline int GetEffectiveSpriteIndex (int idx) const noexcept { return DispSpriteFrame[idx]&0x00ffffff; }
  inline int GetEffectiveSpriteFrame (int idx) const noexcept { return ((DispSpriteFrame[idx]>>24)&VState::FF_FRAMEMASK); }

  inline VAliasModelFrameInfo getMFI (int idx) const noexcept {
    VAliasModelFrameInfo res;
    res.sprite = DispSpriteName[idx];
    res.frame = GetEffectiveSpriteFrame(idx);
    res.index = (ViewStates[idx].State ? ViewStates[idx].State->InClassIndex : -1);
    res.spriteIndex = GetEffectiveSpriteIndex(idx);
    return res;
  }

  inline void setAutonomousProxy (bool v) noexcept { if (v) PlayerFlags |= PF_AutonomousProxy; else PlayerFlags &= ~PF_AutonomousProxy; }
  inline bool isAutonomousProxy () const noexcept { return !!(PlayerFlags&PF_AutonomousProxy); }

  //  VObject interface
  virtual bool ExecuteNetMethod(VMethod *) override;

  void SpawnClient();

  void Printf(const char *, ...) __attribute__((format(printf,2,3)));
  void CenterPrintf(const char *, ...) __attribute__((format(printf,2,3)));

  void SetViewState(int, VState *);
  void AdvanceViewStates(float);

  void SetUserInfo(VStr);
  void ReadFromUserInfo();

  //  Handling of player input.
  void StartPitchDrift();
  void StopPitchDrift();
  void AdjustAngles();
  void HandleInput();
  bool Responder(event_t *);
  void ClearInput();
  int AcsGetInput(int);

  //  Implementation of server to client events.
  void DoClientStartSound(int, TVec, int, int, float, float, bool);
  void DoClientStopSound(int, int);
  void DoClientStartSequence(TVec, int, VName, int);
  void DoClientAddSequenceChoice(int, VName);
  void DoClientStopSequence(int);
  void DoClientPrint(VStr);
  void DoClientChatPrint(VStr nick, VStr text);
  void DoClientCenterPrint(VStr);
  void DoClientSetAngles(TAVec);
  void DoClientIntermission(VName);
  void DoClientPause(bool);
  void DoClientSkipIntermission();
  void DoClientFinale(VStr);
  void DoClientChangeMusic(VName);
  void DoClientSetServerInfo(VStr, VStr);
  void DoClientHudMessage(VStr, VName, int, int, int, VStr, float, float, int, int, float, float, float);

  void DoClientFOV (float fov);

  void WriteViewData();

  // append player commands with the given prefix
  void ListConCommands (TArray<VStr> &list, VStr pfx);

  bool IsConCommand (VStr name);

  // returns `true` if command was found and executed
  // uses VCommand command line
  bool ExecConCommand ();

  // returns `true` if command was found (autocompleter may be still missing)
  // autocompleter should filter list
  bool ExecConCommandAC (TArray<VStr> &args, bool newArg, TArray<VStr> &aclist);

  void CallDumpInventory ();

  DECLARE_FUNCTION(get_IsCheckpointSpawn)

  DECLARE_FUNCTION(IsRunEnabled)
  DECLARE_FUNCTION(IsMLookEnabled)
  DECLARE_FUNCTION(IsCrouchEnabled)
  DECLARE_FUNCTION(IsJumpEnabled)

  DECLARE_FUNCTION(cprint)
  DECLARE_FUNCTION(centerprint)
  DECLARE_FUNCTION(GetPlayerNum)
  DECLARE_FUNCTION(ClearPlayer)
  DECLARE_FUNCTION(ResetWeaponActionFlags)
  DECLARE_FUNCTION(SetViewObject)
  DECLARE_FUNCTION(SetViewObjectIfNone)
  DECLARE_FUNCTION(SetViewState)
  DECLARE_FUNCTION(AdvanceViewStates)
  DECLARE_FUNCTION(DisconnectBot)

  DECLARE_FUNCTION(ClientStartSound)
  DECLARE_FUNCTION(ClientStopSound)
  DECLARE_FUNCTION(ClientStartSequence)
  DECLARE_FUNCTION(ClientAddSequenceChoice)
  DECLARE_FUNCTION(ClientStopSequence)
  DECLARE_FUNCTION(ClientPrint)
  DECLARE_FUNCTION(ClientChatPrint)
  DECLARE_FUNCTION(ClientCenterPrint)
  DECLARE_FUNCTION(ClientSetAngles)
  DECLARE_FUNCTION(ClientIntermission)
  DECLARE_FUNCTION(ClientPause)
  DECLARE_FUNCTION(ClientSkipIntermission)
  DECLARE_FUNCTION(ClientFinale)
  DECLARE_FUNCTION(ClientChangeMusic)
  DECLARE_FUNCTION(ClientSetServerInfo)
  DECLARE_FUNCTION(ClientHudMessage)
  DECLARE_FUNCTION(ClientFOV)

  DECLARE_FUNCTION(ServerSetUserInfo)

  DECLARE_FUNCTION(QS_PutInt);
  DECLARE_FUNCTION(QS_PutName);
  DECLARE_FUNCTION(QS_PutStr);
  DECLARE_FUNCTION(QS_PutFloat);

  DECLARE_FUNCTION(QS_GetInt);
  DECLARE_FUNCTION(QS_GetName);
  DECLARE_FUNCTION(QS_GetStr);
  DECLARE_FUNCTION(QS_GetFloat);

  bool IsCheckpointPossible () { static VMethodProxy method("IsCheckpointPossible"); vobjPutParamSelf(); VMT_RET_BOOL(method); }

  // player events
  void eventPutClientIntoServer () { static VMethodProxy method("PutClientIntoServer"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventSpawnClient () { static VMethodProxy method("SpawnClient"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventNetGameReborn () { static VMethodProxy method("NetGameReborn"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventDisconnectClient () { static VMethodProxy method("DisconnectClient"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventUserinfoChanged () { static VMethodProxy method("UserinfoChanged"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventPlayerExitMap (bool clusterChange) { static VMethodProxy method("PlayerExitMap"); vobjPutParamSelf(clusterChange); VMT_RET_VOID(method); }
  void eventPlayerBeforeExitMap () { static VMethodProxy method("PlayerBeforeExitMap"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventPlayerTick (float deltaTime) { static VMethodProxy method("PlayerTick"); vobjPutParamSelf(deltaTime); VMT_RET_VOID(method); }
  void eventClientTick (float deltaTime) { static VMethodProxy method("ClientTick"); vobjPutParamSelf(deltaTime); VMT_RET_VOID(method); }
  void eventSetViewPos () { static VMethodProxy method("SetViewPos"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventPreTravel () { static VMethodProxy method("PreTravel"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventUseInventory (VStr Inv) { static VMethodProxy method("UseInventory"); vobjPutParamSelf(Inv); VMT_RET_VOID(method); }
  bool eventCheckDoubleFiringSpeed () { static VMethodProxy method("CheckDoubleFiringSpeed"); vobjPutParamSelf(); VMT_RET_BOOL(method); }

  void eventResetInventory () { static VMethodProxy method("ResetInventory"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventResetHealth () { static VMethodProxy method("ResetHealth"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventRemoveKeys () { static VMethodProxy method("RemoveKeys"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventPreraiseWeapon () { static VMethodProxy method("PreraiseWeapon"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventResetToDefaults () { static VMethodProxy method("ResetToDefaults"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventOnSaveLoaded () { static VMethodProxy method("eventOnSaveLoaded"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventOnBeforeSave (bool autosave, bool checkpoint) { static VMethodProxy method("eventOnBeforeSave"); vobjPutParamSelf(autosave, checkpoint); VMT_RET_VOID(method); }
  void eventOnAfterSave (bool autosave, bool checkpoint) { static VMethodProxy method("eventOnAfterSave"); vobjPutParamSelf(autosave, checkpoint); VMT_RET_VOID(method); }

  void eventAfterUnarchiveThinkers () { static VMethodProxy method("eventAfterUnarchiveThinkers"); vobjPutParamSelf(); VMT_RET_VOID(method); }

  void eventClientFOV (float deltaTime) { static VMethodProxy method("ClientFOV"); vobjPutParamSelf(deltaTime); VMT_RET_VOID(method); }

  void eventInitWeaponSlots () { static VMethodProxy method("InitWeaponSlots"); vobjPutParamSelf(); VMT_RET_VOID(method); }

  bool IsNoclipActive () { static VMethodProxy method("GetIsNoclipActive"); vobjPutParamSelf(); VMT_RET_BOOL(method); }

  void QS_Save () { static VMethodProxy method("QS_Save"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void QS_Load () { static VMethodProxy method("QS_Load"); vobjPutParamSelf(); VMT_RET_VOID(method); }

  inline VEntity *ehGetSavedInventory () { static VMethodProxy method("EngineHelperGetSavedInventory"); vobjPutParamSelf(); VMT_RET_REF(VEntity, method); }

  // cheats
  /*
  void eventCheat_VScriptCommand (TArray<VStr> &args) {
    static VMethodProxy method("Cheat_VScriptCommand");
    vobjPutParamSelf((void *)&args);
    VMT_RET_VOID(method);
  }
  */

  // server to client events
  void eventClientStartSound (int SoundId, TVec Org, int OriginId,
                              int Channel, float Volume, float Attenuation, bool Loop)
  {
    static VMethodProxy method("ClientStartSound");
    vobjPutParamSelf(SoundId, Org, OriginId, Channel, Volume, Attenuation, Loop);
    VMT_RET_VOID(method);
  }
  void eventClientStopSound (int OriginId, int Channel) {
    static VMethodProxy method("ClientStopSound");
    vobjPutParamSelf(OriginId, Channel);
    VMT_RET_VOID(method);
  }
  void eventClientStartSequence (TVec Origin, int OriginId, VName Name, int ModeNum) {
    static VMethodProxy method("ClientStartSequence");
    vobjPutParamSelf(Origin, OriginId, Name, ModeNum);
    VMT_RET_VOID(method);
  }
  void eventClientAddSequenceChoice (int OriginId, VName Choice) {
    static VMethodProxy method("ClientAddSequenceChoice");
    vobjPutParamSelf(OriginId, Choice);
    VMT_RET_VOID(method);
  }
  void eventClientStopSequence (int OriginId) {
    static VMethodProxy method("ClientStopSequence");
    vobjPutParamSelf(OriginId);
    VMT_RET_VOID(method);
  }
  void eventClientPrint (VStr Str) {
    static VMethodProxy method("ClientPrint");
    vobjPutParamSelf(Str);
    VMT_RET_VOID(method);
  }
  void eventClientChatPrint (VStr nick, VStr str) {
    static VMethodProxy method("ClientChatPrint");
    vobjPutParamSelf(nick, str);
    VMT_RET_VOID(method);
  }
  void eventClientCenterPrint (VStr Str) {
    static VMethodProxy method("ClientCenterPrint");
    vobjPutParamSelf(Str);
    VMT_RET_VOID(method);
  }
  void eventClientSetAngles (TAVec Angles) {
    static VMethodProxy method("ClientSetAngles");
    vobjPutParamSelf(Angles);
    VMT_RET_VOID(method);
  }
  void eventClientIntermission (VName NextMap) {
    static VMethodProxy method("ClientIntermission");
    vobjPutParamSelf(NextMap);
    VMT_RET_VOID(method);
  }
  void eventClientPause (bool Paused) {
    static VMethodProxy method("ClientPause");
    vobjPutParamSelf(Paused);
    VMT_RET_VOID(method);
  }
  void eventClientSkipIntermission () {
    static VMethodProxy method("ClientSkipIntermission");
    vobjPutParamSelf();
    VMT_RET_VOID(method);
  }
  void eventClientFinale (VStr Type) {
    static VMethodProxy method("ClientFinale");
    vobjPutParamSelf(Type);
    VMT_RET_VOID(method);
  }
  void eventClientChangeMusic (VName Song) {
    static VMethodProxy method("ClientChangeMusic");
    vobjPutParamSelf(Song);
    VMT_RET_VOID(method);
  }
  void eventClientSetServerInfo (VStr Key, VStr Value) {
    static VMethodProxy method("ClientSetServerInfo");
    vobjPutParamSelf(Key, Value);
    VMT_RET_VOID(method);
  }
  void eventClientHudMessage (VStr Message, VName Font, int Type,
                              int Id, int Color, VStr ColorName, float x, float y,
                              int HudWidth, int HudHeight, float HoldTime, float Time1, float Time2)
  {
    static VMethodProxy method("ClientHudMessage");
    vobjPutParamSelf(Message, Font, Type, Id, Color, ColorName, x, y, HudWidth, HudHeight, HoldTime, Time1, Time2);
    VMT_RET_VOID(method);
  }

  void eventConsoleGiveInventory (VStr itemName, int amount=1) {
    static VMethodProxy method("ConsoleGiveInventory");
    vobjPutParamSelf(itemName, amount);
    VMT_RET_VOID(method);
  }

  void eventConsoleTakeInventory (VStr itemName, int amount=-1) {
    static VMethodProxy method("ConsoleTakeInventory");
    vobjPutParamSelf(itemName, amount);
    VMT_RET_VOID(method);
  }

  // client to server events
  void eventServerImpulse (int AImpulse) {
    static VMethodProxy method("ServerImpulse");
    vobjPutParamSelf(AImpulse);
    VMT_RET_VOID(method);
  }
  void eventServerSetUserInfo (VStr Info) {
    static VMethodProxy method("ServerSetUserInfo");
    vobjPutParamSelf(Info);
    VMT_RET_VOID(method);
  }

  bool eventIsReadyWeaponByName (VStr classname, bool allowReplace) {
    static VMethodProxy method("eventIsReadyWeaponByName");
    vobjPutParamSelf(classname, allowReplace);
    VMT_RET_BOOL(method);
  }

  VEntity *eventFindInventoryWeapon (VStr classname, bool allowReplace) {
    static VMethodProxy method("eventFindInventoryWeapon");
    vobjPutParamSelf(classname, allowReplace);
    VMT_RET_REF(VEntity, method);
  }

  VEntity *eventGetReadyWeapon () {
    static VMethodProxy method("eventGetReadyWeapon");
    vobjPutParamSelf();
    VMT_RET_REF(VEntity, method);
  }

  void eventSetReadyWeapon (VEntity *ent, bool instant) {
    static VMethodProxy method("eventSetReadyWeapon");
    vobjPutParamSelf(ent, instant);
    VMT_RET_VOID(method);
  }

  bool eventSetPendingWeapon (VEntity *ent) {
    static VMethodProxy method("eventSetPendingWeapon");
    vobjPutParamSelf(ent);
    VMT_RET_BOOL(method);
  }

  VStr GetCurrentArmorClassName () { static VMethodProxy method("GetCurrentArmorClassName"); vobjPutParamSelf(); VMT_RET_STR(method); }
  int GetCurrentArmorSaveAmount () { static VMethodProxy method("GetCurrentArmorSaveAmount"); vobjPutParamSelf(); VMT_RET_INT(method); }
  float GetCurrentArmorSavePercent () { static VMethodProxy method("GetCurrentArmorSavePercent"); vobjPutParamSelf(); VMT_RET_FLOAT(method); }
  int GetCurrentArmorMaxAbsorb () { static VMethodProxy method("GetCurrentArmorMaxAbsorb"); vobjPutParamSelf(); VMT_RET_INT(method); }
  int GetCurrentArmorFullAbsorb () { static VMethodProxy method("GetCurrentArmorFullAbsorb"); vobjPutParamSelf(); VMT_RET_INT(method); }
  int GetCurrentArmorActualSaveAmount () { static VMethodProxy method("GetCurrentArmorActualSaveAmount"); vobjPutParamSelf(); VMT_RET_INT(method); }

  void eventClientSetAutonomousProxy (bool value) { static VMethodProxy method("ClientSetAutonomousProxy"); vobjPutParamSelf(value); VMT_RET_VOID(method); }
};
