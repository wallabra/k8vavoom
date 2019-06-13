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
class VLevelInfo : public VThinker {
  DECLARE_CLASS(VLevelInfo, VThinker, 0)
  NO_DEFAULT_CONSTRUCTOR(VLevelInfo)

  enum { TID_HASH_SIZE = 128 };

  VGameInfo *Game;
  VWorldInfo *World;

  VStr LevelName;
  vint32 LevelNum;
  vuint8 Cluster;

  VName NextMap;
  VName SecretMap;

  vint32 ParTime;
  vint32 SuckTime;

  vint32 Sky1Texture;
  vint32 Sky2Texture;
  float Sky1ScrollDelta;
  float Sky2ScrollDelta;
  VName SkyBox;

  VName FadeTable;
  vuint32 Fade;
  vuint32 OutsideFog;

  VName SongLump;

  enum {
    LIF_DoubleSky                 = 0x00000001u, // parallax sky: sky2 behind sky1
    LIF_Lightning                 = 0x00000002u, // use of lightning on the level flashes from sky1 to sky2
    LIF_Map07Special              = 0x00000004u,
    LIF_BaronSpecial              = 0x00000008u,
    LIF_CyberDemonSpecial         = 0x00000010u,
    LIF_SpiderMastermindSpecial   = 0x00000020u,
    LIF_MinotaurSpecial           = 0x00000040u,
    LIF_DSparilSpecial            = 0x00000080u,
    LIF_IronLichSpecial           = 0x00000100u,
    LIF_SpecialActionOpenDoor     = 0x00000200u,
    LIF_SpecialActionLowerFloor   = 0x00000400u,
    LIF_SpecialActionKillMonsters = 0x00000800u,
    LIF_NoIntermission            = 0x00001000u,
    LIF_AllowMonsterTelefrags     = 0x00002000u,
    LIF_NoAllies                  = 0x00004000u,
    LIF_DeathSlideShow            = 0x00008000u,
    LIF_ForceNoSkyStretch         = 0x00010000u,
    LIF_LookupName                = 0x00020000u,
    LIF_FallingDamage             = 0x00040000u,
    LIF_OldFallingDamage          = 0x00080000u,
    LIF_StrifeFallingDamage       = 0x00100000u,
    LIF_MonsterFallingDamage      = 0x00200000u,
    LIF_NoFreelook                = 0x00400000u,
    LIF_NoJump                    = 0x00800000u,
    LIF_NoAutoSndSeq              = 0x01000000u,
    LIF_ActivateOwnSpecial        = 0x02000000u,
    LIF_MissilesActivateImpact    = 0x04000000u,
    LIF_FilterStarts              = 0x08000000u,
    LIF_InfiniteFlightPowerup     = 0x10000000u,
    LIF_ClipMidTex                = 0x20000000u,
    LIF_WrapMidTex                = 0x40000000u,
    LIF_KeepFullInventory         = 0x80000000u,
  };
  vuint32 LevelInfoFlags;
  enum {
    LIF2_CompatShortTex        = 0x00000001u,
    LIF2_CompatStairs          = 0x00000002u,
    LIF2_CompatLimitPain       = 0x00000004u,
    LIF2_CompatNoPassOver      = 0x00000008u,
    LIF2_CompatNoTossDrops     = 0x00000010u,
    LIF2_CompatUseBlocking     = 0x00000020u,
    LIF2_CompatNoDoorLight     = 0x00000040u,
    LIF2_CompatRavenScroll     = 0x00000080u,
    LIF2_CompatSoundTarget     = 0x00000100u,
    LIF2_CompatDehHealth       = 0x00000200u,
    LIF2_CompatTrace           = 0x00000400u,
    LIF2_CompatDropOff         = 0x00000800u,
    LIF2_CompatBoomScroll      = 0x00001000u,
    LIF2_CompatInvisibility    = 0x00002000u,
    LIF2_LaxMonsterActivation  = 0x00004000u,
    LIF2_HaveMonsterActivation = 0x00008000u,
    LIF2_ClusterHub            = 0x00010000u,
    LIF2_BegunPlay             = 0x00020000u,
    LIF2_Frozen                = 0x00040000u,
    LIF2_NoCrouch              = 0x00080000u,
    LIF2_ResetHealth           = 0x00100000u,
    LIF2_ResetInventory        = 0x00200000u,
    LIF2_ResetItems            = 0x00400000u,
  };
  vuint32 LevelInfoFlags2;

  int TotalKills;
  int TotalItems;
  int TotalSecret; // for intermission
  int CurrentKills;
  int CurrentItems;
  int CurrentSecret;

  float CompletitionTime; // for intermission

  // maintain single and multi player starting spots
  TArray<mthing_t> DeathmatchStarts; // player spawn spots for deathmatch
  TArray<mthing_t> PlayerStarts; // player spawn spots

  VEntity *TIDHash[TID_HASH_SIZE];

  float Gravity; // level gravity
  float AirControl;
  int Infighting;
  TArray<VMapSpecialAction> SpecialActions;
  TArray<TVec> MapMarkers;

public:
  //VLevelInfo ();
  virtual void PostCtor () override;

  void SetMapInfo (const mapInfo_t &);

  void SectorStartSound (const sector_t *, int, int, float, float);
  void SectorStopSound (const sector_t *, int);
  void SectorStartSequence (const sector_t *, VName, int);
  void SectorStopSequence (const sector_t *);
  void PolyobjStartSequence (const polyobj_t *, VName, int);
  void PolyobjStopSequence (const polyobj_t *);

  void ExitLevel (int Position);
  void SecretExitLevel (int Position);
  void Completed (int Map, int Position, int SaveAngle);

  bool ChangeSwitchTexture (int, bool, VName, bool &);
  bool StartButton (int, vuint8, int, VName, bool);

  VEntity *FindMobjFromTID (int, VEntity *);

  void ChangeMusic (VName);

  inline VStr GetLevelName() const { return (LevelInfoFlags & LIF_LookupName ? GLanguage[*LevelName] : LevelName); }

  int FindFreeTID (int tidstart, int limit=0) const;
  bool IsTIDUsed (int tid) const;

  // static lights
  DECLARE_FUNCTION(AddStaticLight)
  DECLARE_FUNCTION(AddStaticLightRGB)
  DECLARE_FUNCTION(MoveStaticLightByOwner)

  // sound sequences
  DECLARE_FUNCTION(SectorStartSequence)
  DECLARE_FUNCTION(SectorStopSequence)
  DECLARE_FUNCTION(PolyobjStartSequence)
  DECLARE_FUNCTION(PolyobjStopSequence)

  // exiting the level
  DECLARE_FUNCTION(ExitLevel)
  DECLARE_FUNCTION(SecretExitLevel)
  DECLARE_FUNCTION(Completed)

  // special thinker utilites
  DECLARE_FUNCTION(ChangeSwitchTexture)
  DECLARE_FUNCTION(FindMobjFromTID)
  DECLARE_FUNCTION(AutoSave)

  DECLARE_FUNCTION(ChangeMusic)

  DECLARE_FUNCTION(FindFreeTID)
  DECLARE_FUNCTION(IsTIDUsed)

  // EntityEx PickActor (optional TVec Origin, TVec dir, float distance, optional int actorMask, optional int wallMask) {
  // final bool CheckLock (Entity user, int lock, bool door)
  bool eventCheckLock (VEntity *user, int lock, bool door) { static VMethodProxy method("CheckLock"); vobjPutParamSelf(user, lock, door); VMT_RET_BOOL(method); }
  void eventSpawnSpecials () { static VMethodProxy method("SpawnSpecials"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventUpdateSpecials () { static VMethodProxy method("UpdateSpecials"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventAfterUnarchiveThinkers () { static VMethodProxy method("AfterUnarchiveThinkers"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventPolyThrustMobj (VEntity *A, TVec thrustDir, polyobj_t *po) { static VMethodProxy method("PolyThrustMobj"); vobjPutParamSelf(A, thrustDir, po); VMT_RET_VOID(method); }
  void eventPolyCrushMobj (VEntity *A, polyobj_t *po) { static VMethodProxy method("PolyCrushMobj"); vobjPutParamSelf(A, po); VMT_RET_VOID(method); }
  bool eventTagBusy (int tag) { static VMethodProxy method("TagBusy"); vobjPutParamSelf(tag); VMT_RET_BOOL(method); }
  bool eventPolyBusy (int polyobj) { static VMethodProxy method("PolyBusy"); vobjPutParamSelf(polyobj); VMT_RET_BOOL(method); }
  int eventThingCount (int type, VName TypeName, int tid, int SectorTag) { static VMethodProxy method("ThingCount"); vobjPutParamSelf(type, TypeName, tid, SectorTag); VMT_RET_INT(method); }
  int eventExecuteActionSpecial (int Special, int Arg1, int Arg2, int Arg3,
                                 int Arg4, int Arg5, line_t *Line, int Side, VEntity *A)
  {
    static VMethodProxy method("ExecuteActionSpecial");
    vobjPutParamSelf(Special, Arg1, Arg2, Arg3, Arg4, Arg5, Line, Side, A);
    VMT_RET_INT(method);
  }
  int eventEV_ThingProjectile (int tid, int type, int angle, int speed,
                               int vspeed, int gravity, int newtid,
                               VName TypeName, VEntity *Activator)
  {
    static VMethodProxy method("EV_ThingProjectile");
    vobjPutParamSelf(tid, type, angle, speed, vspeed, gravity, newtid, TypeName, Activator);
    VMT_RET_INT(method);
  }
  void eventStartPlaneWatcher (VEntity *it, line_t *line, int lineSide,
                               bool ceiling, int tag, int height, int special,
                               int arg1, int arg2, int arg3, int arg4, int arg5)
  {
    static VMethodProxy method("StartPlaneWatcher");
    vobjPutParamSelf(it, line, lineSide, ceiling, tag, height, special, arg1, arg2, arg3, arg4, arg5);
    VMT_RET_VOID(method);
  }
  void eventSpawnMapThing (mthing_t *mthing) { static VMethodProxy method("SpawnMapThing"); vobjPutParamSelf(mthing); VMT_RET_VOID(method); }
  void eventUpdateParticle (particle_t *p, float DeltaTime) { if (DeltaTime <= 0.0f) return; static VMethodProxy method("UpdateParticle"); vobjPutParamSelf(p, DeltaTime); VMT_RET_VOID(method); }
  //final override int AcsSpawnThing(name Name, TVec Org, int Tid, float Angle, bool forced)
  int eventAcsSpawnThing (VName Name, TVec Org, int Tid, float Angle, bool forced=false) {
    static VMethodProxy method("AcsSpawnThing");
    vobjPutParamSelf(Name, Org, Tid, Angle, forced);
    VMT_RET_INT(method);
  }
  int eventAcsSpawnSpot (VName Name, int SpotTid, int Tid, float Angle, bool forced=false) {
    static VMethodProxy method("AcsSpawnSpot");
    vobjPutParamSelf(Name, SpotTid, Tid, Angle, forced);
    VMT_RET_INT(method);
  }
  int eventAcsSpawnSpotFacing (VName Name, int SpotTid, int Tid, bool forced=false) {
    static VMethodProxy method("AcsSpawnSpotFacing");
    vobjPutParamSelf(Name, SpotTid, Tid, forced);
    VMT_RET_INT(method);
  }
  void eventSectorDamage (int Tag, int Amount, VName DamageType, VName ProtectionType, int Flags) {
    static VMethodProxy method("SectorDamage");
    vobjPutParamSelf(Tag, Amount, DamageType, ProtectionType, Flags);
    VMT_RET_VOID(method);
  }
  int eventDoThingDamage (int Tid, int Amount, VName DmgType, VEntity *Activator) {
    static VMethodProxy method("DoThingDamage");
    vobjPutParamSelf(Tid, Amount, DmgType, Activator);
    VMT_RET_INT(method);
  }
  void eventSetMarineWeapon (int Tid, int Weapon, VEntity *Activator) {
    static VMethodProxy method("SetMarineWeapon");
    vobjPutParamSelf(Tid, Weapon, Activator);
    VMT_RET_VOID(method);
  }
  void eventSetMarineSprite (int Tid, VName SrcClass, VEntity *Activator) {
    static VMethodProxy method("SetMarineSprite");
    vobjPutParamSelf(Tid, SrcClass, Activator);
    VMT_RET_VOID(method);
  }
  void eventAcsFadeRange (float BlendR1, float BlendG1, float BlendB1,
                          float BlendA1, float BlendR2, float BlendG2, float BlendB2,
                          float BlendA2, float Duration, VEntity *Activator)
  {
    static VMethodProxy method("AcsFadeRange");
    vobjPutParamSelf(BlendR1, BlendG1, BlendB1, BlendA1, BlendR2, BlendG2, BlendB2, BlendA2, Duration, Activator);
    VMT_RET_VOID(method);
  }
  void eventAcsCancelFade (VEntity *Activator) {
    static VMethodProxy method("AcsCancelFade");
    vobjPutParamSelf(Activator);
    VMT_RET_VOID(method);
  }
  void eventAcsRadiusQuake2 (VEntity *Activator, int tid, int intensity, int duration, int damrad, int tremrad, VName sound) {
    static VMethodProxy method("AcsRadiusQuake2");
    vobjPutParamSelf(Activator, tid, intensity, duration, damrad, tremrad, sound);
    VMT_RET_VOID(method);
  }
};
