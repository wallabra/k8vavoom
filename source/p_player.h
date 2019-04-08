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
class VNetConnection;
class VClientGameBase;

// constants for FixedColourmap
enum {
  NUMCOLOURMAPS    = 32,
  INVERSECOLOURMAP = 32,
  GOLDCOLOURMAP    = 33,
  REDCOLOURMAP     = 34,
  GREENCOLOURMAP   = 35,
};


// Overlay psprites are scaled shapes
// drawn directly on the view screen,
// coordinates are given for a 320*200 view screen.
enum psprnum_t {
  PS_WEAPON,
  PS_FLASH, // only DOOM uses it
  NUMPSPRITES
};

// player states
enum playerstate_t {
  PST_LIVE, // playing or camping
  PST_DEAD, // dead on the ground, view follows killer
  PST_REBORN, // ready to restart/respawn
};

// button/action code definitions
enum {
  BT_ATTACK      = 0x00000001, // press "fire"
  BT_USE         = 0x00000002, // use button, to open doors, activate switches
  BT_JUMP        = 0x00000004,
  BT_ALT_ATTACK  = 0x00000008,
  BT_BUTTON_5    = 0x00000010,
  BT_BUTTON_6    = 0x00000020,
  BT_BUTTON_7    = 0x00000040,
  BT_BUTTON_8    = 0x00000080,
  BT_RELOAD      = 0x00000100,
  BT_SPEED       = 0x00000200,
  BT_STRAFE      = 0x00000400,
  BT_CROUCH      = 0x00000800,
  BT_MOVELEFT    = 0x00001000,
  BT_MOVERIGHT   = 0x00002000,
  BT_LEFT        = 0x00004000,
  BT_RIGHT       = 0x00008000,
  BT_FORWARD     = 0x00010000,
  BT_BACKWARD    = 0x00020000,
  BT_FLASHLIGHT  = 0x00100000,
  BT_SUPERBULLET = 0x00200000,
};

struct VViewState {
  VState *State;
  float StateTime;
  float SX;
  float SY;
};

// extended player object info: player_t
class VBasePlayer : public VGameObject {
  DECLARE_CLASS(VBasePlayer, VGameObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VBasePlayer)

  enum { TOCENTRE = -128 };

  static VField *fldPendingWeapon;

  VLevelInfo *Level;

  enum {
    PF_Active            = 0x0001,
    PF_Spawned           = 0x0002,
    PF_IsBot             = 0x0004,
    PF_FixAngle          = 0x0008,
    PF_AttackDown        = 0x0010, // True if button down last tic.
    PF_UseDown           = 0x0020,
    PF_DidSecret         = 0x0040, // True if secret level has been done.
    PF_Centreing         = 0x0080,
    PF_IsClient          = 0x0100, // Player on client side
    PF_AutomapRevealed   = 0x0200,
    PF_AutomapShowThings = 0x0400,
    PF_ReloadQueued      = 0x0800,
  };
  vuint32 PlayerFlags;

  VNetConnection *Net;

  VStr UserInfo;

  VStr PlayerName;
  vuint8 BaseClass;
  vuint8 PClass; // player class type
  vuint8 TranslStart;
  vuint8 TranslEnd;
  vint32 Colour;

  float ClientForwardMove; // *2048 for move
  float ClientSideMove; // *2048 for move
  float ForwardMove; // *2048 for move
  float SideMove; // *2048 for move
  float FlyMove; // fly up/down/centreing
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
  vuint8 FixedColourmap;

  // colour shifts for damage, powerups and content types
  vuint32 CShift;

  // overlay view sprites (gun, etc)
  VViewState ViewStates[NUMPSPRITES];
  vint32 DispSpriteFrame[NUMPSPRITES]; // see entity code for explanation
  VName DispSpriteName[NUMPSPRITES]; // see entity code for explanation
  float PSpriteSY;

  float WorldTimer; // total time the player's been playing

  vuint8 ClientNum;

  vint32 SoundEnvironment;

  VClientGameBase *ClGame;

  VPlayerReplicationInfo *PlayerReplicationInfo;

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

  const int GetEffectiveSpriteIndex (int idx) const { return DispSpriteFrame[idx]&0x00ffffff; }
  const int GetEffectiveSpriteFrame (int idx) const { return ((DispSpriteFrame[idx]>>24)&VState::FF_FRAMEMASK); }

  inline VAliasModelFrameInfo getMFI (int idx) const {
    VAliasModelFrameInfo res;
    res.sprite = DispSpriteName[idx];
    res.frame = GetEffectiveSpriteFrame(idx);
    res.index = (ViewStates[idx].State ? ViewStates[idx].State->InClassIndex : -1);
    res.spriteIndex = GetEffectiveSpriteIndex(idx);
    return res;
  }

  //  VObject interface
  virtual bool ExecuteNetMethod(VMethod*) override;

  void SpawnClient();

  void Printf(const char*, ...) __attribute__((format(printf,2,3)));
  void CentrePrintf(const char*, ...) __attribute__((format(printf,2,3)));

  void SetViewState(int, VState*);
  void AdvanceViewStates(float);

  void SetUserInfo(const VStr&);
  void ReadFromUserInfo();

  //  Handling of player input.
  void StartPitchDrift();
  void StopPitchDrift();
  void AdjustAngles();
  void HandleInput();
  bool Responder(event_t*);
  void ClearInput();
  int AcsGetInput(int);

  //  Implementation of server to client events.
  void DoClientStartSound(int, TVec, int, int, float, float, bool);
  void DoClientStopSound(int, int);
  void DoClientStartSequence(TVec, int, VName, int);
  void DoClientAddSequenceChoice(int, VName);
  void DoClientStopSequence(int);
  void DoClientPrint(VStr);
  void DoClientCentrePrint(VStr);
  void DoClientSetAngles(TAVec);
  void DoClientIntermission(VName);
  void DoClientPause(bool);
  void DoClientSkipIntermission();
  void DoClientFinale(VStr);
  void DoClientChangeMusic(VName);
  void DoClientSetServerInfo(VStr, VStr);
  void DoClientHudMessage(const VStr&, VName, int, int, int, const VStr&,
    float, float, int, int, float, float, float);

  void WriteViewData();

  // append player commands with the given prefix
  void ListConCommands (TArray<VStr> &list, const VStr &pfx);

  bool IsConCommand (const VStr &name);

  // returns `true` if command was found and executed
  // uses VCommand command line
  bool ExecConCommand ();

  // returns `true` if command was found (autocompleter may be still missing)
  // autocompleter should filter list
  bool ExecConCommandAC (TArray<VStr> &args, bool newArg, TArray<VStr> &aclist);

  void CallDumpInventory ();

  DECLARE_FUNCTION(cprint)
  DECLARE_FUNCTION(centreprint)
  DECLARE_FUNCTION(GetPlayerNum)
  DECLARE_FUNCTION(ClearPlayer)
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
  DECLARE_FUNCTION(ClientCentrePrint)
  DECLARE_FUNCTION(ClientSetAngles)
  DECLARE_FUNCTION(ClientIntermission)
  DECLARE_FUNCTION(ClientPause)
  DECLARE_FUNCTION(ClientSkipIntermission)
  DECLARE_FUNCTION(ClientFinale)
  DECLARE_FUNCTION(ClientChangeMusic)
  DECLARE_FUNCTION(ClientSetServerInfo)
  DECLARE_FUNCTION(ClientHudMessage)

  DECLARE_FUNCTION(ServerSetUserInfo)

  DECLARE_FUNCTION(QS_PutInt);
  DECLARE_FUNCTION(QS_PutName);
  DECLARE_FUNCTION(QS_PutStr);
  DECLARE_FUNCTION(QS_PutFloat);

  DECLARE_FUNCTION(QS_GetInt);
  DECLARE_FUNCTION(QS_GetName);
  DECLARE_FUNCTION(QS_GetStr);
  DECLARE_FUNCTION(QS_GetFloat);

  // player events
  void eventPutClientIntoServer () { P_PASS_SELF; EV_RET_VOID(NAME_PutClientIntoServer); }
  void eventSpawnClient () { P_PASS_SELF; EV_RET_VOID(NAME_SpawnClient); }
  void eventNetGameReborn () { P_PASS_SELF; EV_RET_VOID(NAME_NetGameReborn); }
  void eventDisconnectClient () { P_PASS_SELF; EV_RET_VOID(NAME_DisconnectClient); }
  void eventUserinfoChanged () { P_PASS_SELF; EV_RET_VOID(NAME_UserinfoChanged); }
  void eventPlayerExitMap (bool clusterChange) { P_PASS_SELF; P_PASS_BOOL(clusterChange); EV_RET_VOID(NAME_PlayerExitMap); }
  void eventPlayerBeforeExitMap () { P_PASS_SELF; EV_RET_VOID(NAME_PlayerBeforeExitMap); }
  void eventPlayerTick (float deltaTime) { P_PASS_SELF; P_PASS_FLOAT(deltaTime); EV_RET_VOID(NAME_PlayerTick); }
  void eventClientTick (float DeltaTime) { P_PASS_SELF; P_PASS_FLOAT(DeltaTime); EV_RET_VOID(NAME_ClientTick); }
  void eventSetViewPos () { P_PASS_SELF; EV_RET_VOID(NAME_SetViewPos); }
  void eventPreTravel () { P_PASS_SELF; EV_RET_VOID(NAME_PreTravel); }
  void eventUseInventory (const VStr &Inv) { P_PASS_SELF; P_PASS_STR(Inv); EV_RET_VOID(NAME_UseInventory); }
  bool eventCheckDoubleFiringSpeed () { P_PASS_SELF; EV_RET_BOOL(NAME_CheckDoubleFiringSpeed); }

  bool IsCheckpointPossible () { P_PASS_SELF; EV_RET_BOOL(VName("IsCheckpointPossible")); }

  //void QS_Save ();
  void QS_Save () {
    static int mtindex = -666;
    if (mtindex < 0) mtindex = StaticClass()->GetMethodIndex(VName("QS_Save"));
    P_PASS_SELF;
    EV_RET_VOID_IDX(mtindex);
  }

  //void QS_Load ();
  void QS_Load () {
    static int mtindex = -666;
    if (mtindex < 0) mtindex = StaticClass()->GetMethodIndex(VName("QS_Load"));
    P_PASS_SELF;
    EV_RET_VOID_IDX(mtindex);
  }

  // cheats
  void eventCheat_VScriptCommand (TArray<VStr> &args) { P_PASS_SELF; P_PASS_PTR((void *)&args); EV_RET_VOID(VName("Cheat_VScriptCommand")); }

  // server to client events
  void eventClientStartSound (int SoundId, TVec Org, int OriginId,
                              int Channel, float Volume, float Attenuation, bool Loop)
  {
    P_PASS_SELF;
    P_PASS_INT(SoundId);
    P_PASS_VEC(Org);
    P_PASS_INT(OriginId);
    P_PASS_INT(Channel);
    P_PASS_FLOAT(Volume);
    P_PASS_FLOAT(Attenuation);
    P_PASS_BOOL(Loop);
    EV_RET_VOID(NAME_ClientStartSound);
  }
  void eventClientStopSound (int OriginId, int Channel) {
    P_PASS_SELF;
    P_PASS_INT(OriginId);
    P_PASS_INT(Channel);
    EV_RET_VOID(NAME_ClientStopSound);
  }
  void eventClientStartSequence (TVec Origin, int OriginId, VName Name, int ModeNum) {
    P_PASS_SELF;
    P_PASS_VEC(Origin);
    P_PASS_INT(OriginId);
    P_PASS_NAME(Name);
    P_PASS_INT(ModeNum);
    EV_RET_VOID(NAME_ClientStartSequence);
  }
  void eventClientAddSequenceChoice (int OriginId, VName Choice) {
    P_PASS_SELF;
    P_PASS_INT(OriginId);
    P_PASS_NAME(Choice);
    EV_RET_VOID(NAME_ClientAddSequenceChoice);
  }
  void eventClientStopSequence (int OriginId) {
    P_PASS_SELF;
    P_PASS_INT(OriginId);
    EV_RET_VOID(NAME_ClientStopSequence);
  }
  void eventClientPrint (const VStr &Str) {
    P_PASS_SELF;
    P_PASS_STR(Str);
    EV_RET_VOID(NAME_ClientPrint);
  }
  void eventClientCentrePrint (const VStr &Str) {
    P_PASS_SELF;
    P_PASS_STR(Str);
    EV_RET_VOID(NAME_ClientCentrePrint);
  }
  void eventClientSetAngles (TAVec Angles) {
    P_PASS_SELF;
    P_PASS_AVEC(Angles);
    EV_RET_VOID(NAME_ClientSetAngles);
  }
  void eventClientIntermission (VName NextMap) {
    P_PASS_SELF;
    P_PASS_NAME(NextMap);
    EV_RET_VOID(NAME_ClientIntermission);
  }
  void eventClientPause (bool Paused) {
    P_PASS_SELF;
    P_PASS_BOOL(Paused);
    EV_RET_VOID(NAME_ClientPause);
  }
  void eventClientSkipIntermission () {
    P_PASS_SELF;
    EV_RET_VOID(NAME_ClientSkipIntermission);
  }
  void eventClientFinale (const VStr &Type) {
    P_PASS_SELF;
    P_PASS_STR(Type);
    EV_RET_VOID(NAME_ClientFinale);
  }
  void eventClientChangeMusic (VName Song) {
    P_PASS_SELF;
    P_PASS_NAME(Song);
    EV_RET_VOID(NAME_ClientChangeMusic);
  }
  void eventClientSetServerInfo (const VStr &Key, const VStr &Value) {
    P_PASS_SELF;
    P_PASS_STR(Key);
    P_PASS_STR(Value);
    EV_RET_VOID(NAME_ClientSetServerInfo);
  }
  void eventClientHudMessage (const VStr &Message, VName Font, int Type,
                              int Id, int Colour, const VStr &ColourName, float x, float y,
                              int HudWidth, int HudHeight, float HoldTime, float Time1, float Time2)
  {
    P_PASS_SELF;
    P_PASS_STR(Message);
    P_PASS_NAME(Font);
    P_PASS_INT(Type);
    P_PASS_INT(Id);
    P_PASS_INT(Colour);
    P_PASS_STR(ColourName);
    P_PASS_FLOAT(x);
    P_PASS_FLOAT(y);
    P_PASS_INT(HudWidth);
    P_PASS_INT(HudHeight);
    P_PASS_FLOAT(HoldTime);
    P_PASS_FLOAT(Time1);
    P_PASS_FLOAT(Time2);
    EV_RET_VOID(NAME_ClientHudMessage);
  }

  // client to server events
  void eventServerImpulse (int AImpulse) {
    P_PASS_SELF;
    P_PASS_INT(AImpulse);
    EV_RET_VOID(NAME_ServerImpulse);
  }
  void eventServerSetUserInfo (const VStr &Info) {
    P_PASS_SELF;
    P_PASS_STR(Info);
    EV_RET_VOID(NAME_ServerSetUserInfo);
  }

  VEntity *eventGetReadyWeapon () {
    static int mtindex = -666;
    if (mtindex < 0) mtindex = StaticClass()->GetMethodIndex(VName("eventGetReadyWeapon"));
    P_PASS_SELF;
    EV_RET_REF_IDX(VEntity, mtindex);
  }

  void eventSetReadyWeapon (VEntity *ent) {
    static int mtindex = -666;
    if (mtindex < 0) mtindex = StaticClass()->GetMethodIndex(VName("eventSetReadyWeapon"));
    P_PASS_SELF;
    P_PASS_REF(ent);
    EV_RET_VOID_IDX(mtindex);
  }
};
