////////////////////////////////////////////////////////////////////////////////
// WARNING! KEEP THIS IN SYNC WITH VaVoom C file!
////////////////////////////////////////////////////////////////////////////////
#ifndef DFMAP_STRUCTS_HEADER
#define DFMAP_STRUCTS_HEADER


////////////////////////////////////////////////////////////////////////////////
// special texture identifiers, used to generate pascal sources
enum /*TextureSpecial*/ {
  TEXTURE_SPECIAL_WATER = -1,
  TEXTURE_SPECIAL_ACID1 = -2,
  TEXTURE_SPECIAL_ACID2 = -3,
  TEXTURE_NONE = -4,
};

// directions
enum /*DirType*/ {
  DIR_LEFT, // 0
  DIR_RIGHT, // 1
  DIR_SOMETHING2, // 2
};

// triggers
enum /*TriggerType*/ {
  TRIGGER_NONE, // 0
  TRIGGER_EXIT, // 1
  TRIGGER_TELEPORT, // 2
  TRIGGER_OPENDOOR, // 3
  TRIGGER_CLOSEDOOR, // 4
  TRIGGER_DOOR, // 5
  TRIGGER_DOOR5, // 6
  TRIGGER_CLOSETRAP, // 7
  TRIGGER_TRAP, // 8
  TRIGGER_PRESS, // 9
  TRIGGER_SECRET, // 10
  TRIGGER_LIFTUP, // 11
  TRIGGER_LIFTDOWN, // 12
  TRIGGER_LIFT, // 13
  TRIGGER_TEXTURE, // 14
  TRIGGER_ON, // 15
  TRIGGER_OFF, // 16
  TRIGGER_ONOFF, // 17
  TRIGGER_SOUND, // 18
  TRIGGER_SPAWNMONSTER, // 19
  TRIGGER_SPAWNITEM, // 20
  TRIGGER_MUSIC, // 21
  TRIGGER_PUSH, // 22
  TRIGGER_SCORE, // 23
  TRIGGER_MESSAGE, // 24
  TRIGGER_DAMAGE, // 25
  TRIGGER_HEALTH, // 26
  TRIGGER_SHOT, // 27
  TRIGGER_EFFECT, // 28
  TRIGGER_SCRIPT, // 29
  //
  TRIGGER_MAX,
};

// "as XXX" means "generate this identifier for pascal sources
enum /*PanelType*/ {
  PANEL_NONE = 0,
  PANEL_WALL = 1,
  PANEL_BACK = 2,
  PANEL_FORE = 4,
  PANEL_WATER = 8,
  PANEL_ACID1 = 16,
  PANEL_ACID2 = 32,
  PANEL_STEP = 64,
  PANEL_LIFTUP = 128,
  PANEL_LIFTDOWN = 256,
  PANEL_OPENDOOR = 512,
  PANEL_CLOSEDOOR = 1024,
  PANEL_BLOCKMON = 2048,
  PANEL_LIFTLEFT = 4096,
  PANEL_LIFTRIGHT = 8192,
};

enum /*PanelFlag*/ {
  PANEL_FLAG_NONE = 0,
  PANEL_FLAG_BLENDING = 1,
  PANEL_FLAG_HIDE = 2,
  PANEL_FLAG_WATERTEXTURES = 4,
};

enum /*EffectAction*/ {
  EFFECT_NONE, // 0
  EFFECT_TELEPORT, // 1
  EFFECT_RESPAWN, // 2
  EFFECT_FIRE, // 3
};

//WARNING! max allowed items types is 127
enum /*Item*/ {
  ITEM_NONE, // 0
  ITEM_MEDKIT_SMALL, // 1
  ITEM_MEDKIT_LARGE, // 2
  ITEM_MEDKIT_BLACK, // 3
  ITEM_ARMOR_GREEN, // 4
  ITEM_ARMOR_BLUE, // 5
  ITEM_SPHERE_BLUE, // 6
  ITEM_SPHERE_WHITE, // 7
  ITEM_SUIT, // 8
  ITEM_OXYGEN, // 9
  ITEM_INVUL, // 10
  ITEM_WEAPON_SAW, // 11
  ITEM_WEAPON_SHOTGUN1, // 12
  ITEM_WEAPON_SHOTGUN2, // 13
  ITEM_WEAPON_CHAINGUN, // 14
  ITEM_WEAPON_ROCKETLAUNCHER, // 15
  ITEM_WEAPON_PLASMA, // 16
  ITEM_WEAPON_BFG, // 17
  ITEM_WEAPON_SUPERPULEMET, // 18
  ITEM_AMMO_BULLETS, // 19
  ITEM_AMMO_BULLETS_BOX, // 20
  ITEM_AMMO_SHELLS, // 21
  ITEM_AMMO_SHELLS_BOX, // 22
  ITEM_AMMO_ROCKET, // 23
  ITEM_AMMO_ROCKET_BOX, // 24
  ITEM_AMMO_CELL, // 25
  ITEM_AMMO_CELL_BIG, // 26
  ITEM_AMMO_BACKPACK, // 27
  ITEM_KEY_RED, // 28
  ITEM_KEY_GREEN, // 29
  ITEM_KEY_BLUE, // 30
  ITEM_WEAPON_KASTET, // 31
  ITEM_WEAPON_PISTOL, // 32
  ITEM_BOTTLE, // 33
  ITEM_HELMET, // 34
  ITEM_JETPACK, // 35
  ITEM_INVIS, // 36
  ITEM_WEAPON_FLAMETHROWER, // 37
  ITEM_AMMO_FUELCAN, // 38
  //
  ITEM_MAX, // store the last item's id in here use this in for loops
};

enum /*ItemOption*/ {
  ITEM_OPTION_NONE = 0,
  ITEM_OPTION_ONLYDM = 1,
  ITEM_OPTION_FALL = 2,
};

enum /*AreaType*/ {
  AREA_NONE, // 0
  AREA_PLAYERPOINT1, // 1
  AREA_PLAYERPOINT2, // 2
  AREA_DMPOINT, // 3
  AREA_REDFLAG, // 4
  AREA_BLUEFLAG, // 5
  AREA_DOMFLAG, // 6
  AREA_REDTEAMPOINT, // 7
  AREA_BLUETEAMPOINT, // 8
};

enum /*Monster*/ {
  MONSTER_NONE, // 0
  MONSTER_DEMON, // 1
  MONSTER_IMP, // 2
  MONSTER_ZOMBY, // 3
  MONSTER_SERG, // 4
  MONSTER_CYBER, // 5
  MONSTER_CGUN, // 6
  MONSTER_BARON, // 7
  MONSTER_KNIGHT, // 8
  MONSTER_CACO, // 9
  MONSTER_SOUL, // 10
  MONSTER_PAIN, // 11
  MONSTER_SPIDER, // 12
  MONSTER_BSP, // 13
  MONSTER_MANCUB, // 14
  MONSTER_SKEL, // 15
  MONSTER_VILE, // 16
  MONSTER_FISH, // 17
  MONSTER_BARREL, // 18
  MONSTER_ROBO, // 19
  MONSTER_MAN, // 20
  // aliases
  MONSTER_ZOMBIE = MONSTER_ZOMBY,
};

enum /*MonsterBehaviour*/ {
  BH_NORMAL, // 0
  BH_KILLER, // 1
  BH_MANIAC, // 2
  BH_INSANE, // 3
  BH_CANNIBAL, // 4
  BH_GOOD, // 5
};

enum /*TriggerShot*/ {
  TRIGGER_SHOT_PISTOL, // 0
  TRIGGER_SHOT_BULLET, // 1
  TRIGGER_SHOT_SHOTGUN, // 2
  TRIGGER_SHOT_SSG, // 3
  TRIGGER_SHOT_IMP, // 4
  TRIGGER_SHOT_PLASMA, // 5
  TRIGGER_SHOT_SPIDER, // 6
  TRIGGER_SHOT_CACO, // 7
  TRIGGER_SHOT_BARON, // 8
  TRIGGER_SHOT_MANCUB, // 9
  TRIGGER_SHOT_REV, // 10
  TRIGGER_SHOT_ROCKET, // 11
  TRIGGER_SHOT_BFG, // 12
  TRIGGER_SHOT_EXPL, // 13
  TRIGGER_SHOT_BFGEXPL, // 14
  //
  TRIGGER_SHOT_MAX,
};

enum /*TriggerShotTarget*/ {
  TRIGGER_SHOT_TARGET_NONE, // 0
  TRIGGER_SHOT_TARGET_MON, // 1
  TRIGGER_SHOT_TARGET_PLR, // 2
  TRIGGER_SHOT_TARGET_RED, // 3
  TRIGGER_SHOT_TARGET_BLUE, // 4
  TRIGGER_SHOT_TARGET_MONPLR, // 5
  TRIGGER_SHOT_TARGET_PLRMON, // 6
};

enum /*TriggerShotAim*/ {
  TRIGGER_SHOT_AIM_DEFAULT, // 0
  TRIGGER_SHOT_AIM_ALLMAP, // 1
  TRIGGER_SHOT_AIM_TRACE, // 2
  TRIGGER_SHOT_AIM_TRACEALL, // 3
};

enum /*TriggerEffect*/ {
  TRIGGER_EFFECT_PARTICLE, // 0
  TRIGGER_EFFECT_ANIMATION, // 1
};

enum /*TriggerEffectType*/ {
  TRIGGER_EFFECT_SLIQUID, // 0
  TRIGGER_EFFECT_LLIQUID, // 1
  TRIGGER_EFFECT_DLIQUID, // 2
  TRIGGER_EFFECT_BLOOD, // 3
  TRIGGER_EFFECT_SPARK, // 4
  TRIGGER_EFFECT_BUBBLE, // 5
  //
  TRIGGER_EFFECT_MAX,
};

enum /*TriggerEffectPos*/ {
  TRIGGER_EFFECT_POS_CENTER, // 0
  TRIGGER_EFFECT_POS_AREA, // 1
};

enum /*TriggerMusicAction*/ {
  TRIGGER_MUSIC_ACTION_STOP, // 0
  TRIGGER_MUSIC_ACTION_PLAY, // 1; unpause or restart
};

enum /*TriggerScoreAction*/ {
  TRIGGER_SCORE_ACTION_ADD, // 0
  TRIGGER_SCORE_ACTION_SUB, // 1
  TRIGGER_SCORE_ACTION_WIN, // 2
  TRIGGER_SCORE_ACTION_LOOSE, // 3
};

enum /*TriggerMessageDest*/ {
  TRIGGER_MESSAGE_DEST_ME, // 0
  TRIGGER_MESSAGE_DEST_MY_TEAM, // 1
  TRIGGER_MESSAGE_DEST_ENEMY_TEAM, // 2
  TRIGGER_MESSAGE_DEST_RED_TEAM, // 3
  TRIGGER_MESSAGE_DEST_BLUE_TEAM, // 4
  TRIGGER_MESSAGE_DEST_EVERYONE, // 5
};

enum /*TriggerMessageKind*/ {
  TRIGGER_MESSAGE_KIND_CHAT, // 0
  TRIGGER_MESSAGE_KIND_GAME, // 1
};

enum /*TriggerScoreTeam*/ {
  TRIGGER_SCORE_TEAM_MINE_RED, // 0
  TRIGGER_SCORE_TEAM_MINE_BLUE, // 1
  TRIGGER_SCORE_TEAM_FORCE_RED, // 2
  TRIGGER_SCORE_TEAM_FORCE_BLUE, // 3
};

enum /*ActivateType*/ {
  ACTIVATE_NONE = 0,
  ACTIVATE_PLAYERCOLLIDE = 1,
  ACTIVATE_MONSTERCOLLIDE = 2,
  ACTIVATE_PLAYERPRESS = 4,
  ACTIVATE_MONSTERPRESS = 8,
  ACTIVATE_SHOT = 16,
  ACTIVATE_NOMONSTER = 32,
  ACTIVATE_CUSTOM = 255,
};

enum /*Key*/ {
  KEY_NONE = 0,
  KEY_RED = 1,
  KEY_GREEN = 2,
  KEY_BLUE = 4,
  KEY_REDTEAM = 8,
  KEY_BLUETEAM = 16,
};

enum /*HitType*/ {
  HIT_SOME, // 0
  HIT_ROCKET, // 1
  HIT_BFG, // 2
  HIT_TRAP, // 3
  HIT_FALL, // 4
  HIT_WATER, // 5
  HIT_ACID, // 6
  HIT_ELECTRO, // 7
  HIT_FLAME, // 8
  HIT_SELF, // 9
  HIT_DISCON, // 10
};


////////////////////////////////////////////////////////////////////////////////
struct /*readonly*/ Header {
  VStr mapName; // type char[32] offset 0 writedefault tip "map name";
  VStr author; // type char[32] offset 32 default "" writedefault tip "map author";
  VStr desc; // type char[256] offset 64 default "" writedefault tip "map description";
  VStr music; // type char[64] offset 320 default 'Standart.wad:D2DMUS\ПРОСТОТА' writedefault tip "music resource";
  VStr sky; // type char[64] offset 384 default 'Standart.wad:D2DSKY\RSKY1' writedefault tip "sky resource";
  int width; // type short offset 448 as wh writedefault;
  int height; // type short offset 450 as wh writedefault;
};


struct /*readonly*/ Texture {
  VStr path; // char[64] offset 0
  /*bool*/int animated; // bytebool offset 64
};


struct /*readonly*/ Panel /*size 18 bytes binblock 2*/ {
  int x; // type int offset 0 as xy writedefault;
  int y; // type int offset 4 as xy writedefault;
  int width; // type short offset 8 as wh as wh writedefault;
  int height; // type short offset 10 as wh as wh writedefault;
  int texture; // type ushort offset 12 texture writedefault;
  int type; // type ushort offset 14 bitenum unique PanelType writedefault;
  int alpha; // type ubyte offset 16 default 0;
  int flags; // type ubyte offset 17 bitenum PanelFlag default PANEL_FLAG_NONE;
  /*
  bool bBlending; // PANEL_FLAG_BLENDING
  bool bHidden; // PANEL_FLAG_HIDE
  bool bLiquid; // PANEL_FLAG_WATERTEXTURES
  */
  // moving platform options, not in binary
  /*
  "move_speed" type point default (0 0);
  "size_speed" type point default (0 0); // alas, `size` cannot be negative
  "move_start" type point default (0 0);
  "move_end" type point default (0 0);
  "size_end" type size default (0 0);
  "move_active" type bool default false;
  "move_once" type bool default false;
  "end_pos_trigger" trigger default null;
  "end_size_trigger" trigger default null;
  */
};


struct /*readonly*/ Item /*size 10 bytes binblock 3*/ {
  int x; // type int offset 0 as xy writedefault;
  int y; // type int offset 4 as xy writedefault;
  int type; // type ubyte offset 8 enum Item writedefault;
  //"options" type ubyte offset 9 bitenum ItemOption default ITEM_OPTION_NONE;
  int flags;
  /*
  bool bDMOnly; // ITEM_OPTION_ONLYDM
  bool bGravity; // ITEM_OPTION_FALL
  */
};


struct /*readonly*/ Monster /*size 10 bytes binblock 5*/ {
  int x; // type int offset 0 as xy writedefault;
  int y; // type int offset 4 as xy writedefault;
  int type; // type ubyte offset 8 enum Monster writedefault;
  int dir; // type ubyte offset 9 enum DirType default DIR_LEFT;
};


struct /*readonly*/ Area /*size 10 bytes binblock 4*/ {
  int x; // type int offset 0 as xy writedefault;
  int y; // type int offset 4 as xy writedefault;
  int type; // type ubyte offset 8 enum AreaType writedefault;
  int dir; // type ubyte offset 9 enum DirType default DIR_LEFT;
};


struct /*readonly*/ Trigger /*size 148 bytes binblock 6*/ {
  int x; // type int offset 0 as xy writedefault;
  int y; // type int offset 4 as xy writedefault;
  int width; // type short offset 8 as wh as wh writedefault;
  int height; // type short offset 10 as wh as wh writedefault;
  /*bool*/int enabled; //  type bytebool offset 12 default true;
  int texturePanel; // type int offset 13 panel default null;
  int type; // type ubyte offset 17 enum TriggerType writedefault;
  // "activate_type" type ubyte offset 18 bitenum ActivateType;
  int actFlags;
  /*
  bool bOnPlayerCollide; // ACTIVATE_PLAYERCOLLIDE
  bool bOnMonsterCollide; // ACTIVATE_MONSTERCOLLIDE
  bool bOnPlayerPress; // ACTIVATE_PLAYERPRESS
  bool bOnMonsterPress; // ACTIVATE_MONSTERPRESS
  bool bOnShot; // ACTIVATE_SHOT
  bool bNoMonster; // ACTIVATE_NOMONSTER
  */
  //"keys" type ubyte offset 19 bitenum Key default KEY_NONE;
  int keyFlags;
  /*
  bool bKeyRed; // KEY_RED
  bool bKeyGreen; // KEY_GREEN
  bool bKeyBlue; // KEY_BLUE
  bool bRedTeam; // KEY_REDTEAM
  bool bBlueTeam; // KEY_BLUETEAM
  */
  //"triggerdata" type trigdata[128] offset 20; // the only special nested structure
  //DO NOT USE! experimental feature! will be removed!
  /*
  "exoma_init" type string default "" tip "will be called on trigger creation";
  "exoma_think" type string default "" tip "will be called on each think step";
  "exoma_check" type string default "" tip "will be called before activation";
  "exoma_action" type string default "" tip "will be called on activation";
  */
};


////////////////////////////////////////////////////////////////////////////////
// various triggers

// TRIGGER_EXIT
struct /*readonly*/ TriggerExit : Trigger {
  VStr map; // type char[16] offset 0 writedefault;
};

// TRIGGER_TELEPORT
struct /*readonly*/ TriggerTeleport : Trigger {
  int destx; // type int offset 0 as xy writedefault;
  int desty; // type int offset 4 as xy writedefault;
  int flags;
  /*
  bool bD2D; // type bytebool offset 8 default false;
  bool bSilent; // type bytebool offset 9 default false;
  */
  int dir; // type ubyte offset 10 enum DirType default DIR_LEFT;
};

// TRIGGER_OPENDOOR, TRIGGER_CLOSEDOOR, TRIGGER_DOOR, TRIGGER_DOOR5, TRIGGER_CLOSETRAP, TRIGGER_TRAP, TRIGGER_LIFTUP, TRIGGER_LIFTDOWN, TRIGGER_LIFT
struct /*readonly*/ TriggerPanel : Trigger {
  int panelId; // type int offset 0 panel writedefault;
  int flags;
  /*
  bool bD2D; // type bool offset 5 default false;
  bool bSilent; // type bool offset 4 default false;
  */
};

// TRIGGER_PRESS, TRIGGER_ON, TRIGGER_OFF, TRIGGER_ONOFF
struct /*readonly*/ TriggerPress : Trigger {
  int destx; // type int offset 0 as xy writedefault;
  int desty; // type int offset 4 as xy writedefault;
  int destw; // type short offset 8 as wh as wh writedefault;
  int desth; // type short offset 10 as wh as wh writedefault;
  int wait; // type ushort offset 12 default 0;
  int pressCount; // alias pressCount type ushort offset 14 default 0;
  int monsterId; // type int offset 16 monster as monsterid default null;
  /*bool*/int extRandom; // type bool offset 20 default false;
  // this one is for moving platforms
  /*
  "panelid" panel default null;
  "silent" type bool default true;
  "sound" type string default "";
  */
  //alias count = pressCount;
};

// TRIGGER_SECRET: no data
struct /*readonly*/ TriggerSecret : Trigger {
};

// TRIGGER_TEXTURE
struct /*readonly*/ TriggerTexture : Trigger {
  int flags;
  /*
  bool activateOnce; // type bool offset 0 default false writedefault;
  bool animateOnce; // type bool offset 1 default false writedefault;
  */
};

// TRIGGER_SOUND
struct /*readonly*/ TriggerSound : Trigger {
  VStr soundName; // "sound_name" type char[64] offset 0 writedefault;
  int volume; // type ubyte offset 64 default 0 writedefault; //??? default ???
  int pan; // type ubyte offset 65 default 0;
  int playCount; // type ubyte offset 67 default 1;
  int flags;
  /*
  bool local; // type bool offset 66 default true; //??? default ???
  bool soundSwitch; // type bool offset 68 default false; //??? default ???
  */
};

// TRIGGER_SPAWNMONSTER
struct /*readonly*/ TriggerSpawnMonster : Trigger {
  int destx; // type int offset 0 as xy writedefault;
  int desty; // type int offset 4 as xy writedefault;
  int spawnMonsType; // type ubyte offset 8 enum Monster default MONSTER_IMP writedefault;
  int health; // type int offset 12 writedefault;
  int dir; // type ubyte offset 16 enum DirType default DIR_LEFT writedefault;
  /*bool*/int active; // type bool offset 17 default true;
  int monsCount; // alias count type int offset 20 default 1 writedefault;
  int effect; // type ubyte offset 24 enum EffectAction default EFFECT_NONE writedefault;
  int max; // type ushort offset 26 default 1 writedefault;
  int delay; // type ushort offset 28 default 1000 writedefault;
  int behaviour; // type ubyte offset 30 enum MonsterBehaviour default BH_NORMAL;
};

// TRIGGER_SPAWNITEM
struct /*readonly*/ TriggerSpawnItem : Trigger {
  int destx; // type int offset 0 as xy writedefault;
  int desty; // type int offset 4 as xy writedefault;
  int spawnItemType; // type ubyte offset 8 enum Item default ITEM_NONE writedefault;
  int flags;
  /*
  bool bGravity; // type bool offset 9 default true;
  bool bDMOnly; // type bool offset 10 default false;
  */
  int itemCount; // type int offset 12 default 1;
  int effect; // type ubyte offset 16 enum EffectAction default EFFECT_NONE writedefault;
  int max; // type ushort offset 18 default 1;
  int delay; // type ushort offset 20 default 1000 writedefault;
};

// TRIGGER_MUSIC
struct /*readonly*/ TriggerMusic : Trigger {
  VStr musicName; // type char[64] offset 0 writedefault;
  int musicAction; // type ubyte offset 64 enum TriggerMusicAction writedefault;
};

// TRIGGER_PUSH
struct /*readonly*/ TriggerPush : Trigger {
  int angle; // type ushort offset 0 writedefault;
  int force; // type ubyte offset 2 writedefault;
  /*bool*/int resetVelocity; // type bool offset 3 default false writedefault;
};

// TRIGGER_SCORE
struct /*readonly*/ TriggerScore : Trigger {
  int scoreAction; // type ubyte offset 0 enum TriggerScoreAction default TRIGGER_SCORE_ACTION_ADD writedefault;
  int scoreCount; // type ubyte offset 1 default 1 writedefault;
  int scoreTeam; // type ubyte offset 2 enum TriggerScoreTeam writedefault;
  int flags;
  /*
  bool bScoreCon; // type bool offset 3 default false writedefault;
  bool bScoreMsg; // type bool offset 4 default true writedefault;
  */
};

// TRIGGER_MESSAGE
struct /*readonly*/ TriggerMessage : Trigger {
  int kind; // type ubyte offset 0 enum TriggerMessageKind default TRIGGER_MESSAGE_KIND_GAME writedefault;
  int msgDest; // type ubyte enum TriggerMessageDest offset 1;
  VStr text; // type char[100] offset 2 writedefault;
  int msgTime; // type ushort offset 102 writedefault;
};

// TRIGGER_DAMAGE
struct /*readonly*/ TriggerDamage : Trigger {
  int amount; // type ushort offset 0 writedefault;
  int interval; // type ushort offset 2 writedefault;
};

// TRIGGER_HEALTH
struct /*readonly*/ TriggerHealth : Trigger {
  int amount; // type ushort offset 0 writedefault;
  int interval; // type ushort offset 2 writedefault;
  int flags;
  /*
  bool bHealMax; // type bool offset 4 writedefault;
  bool bSilent; // type bool offset 5 writedefault;
  */
};

// TRIGGER_SHOT
struct /*readonly*/ TriggerShot : Trigger {
  int destx; // type int offset 0 as xy writedefault;
  int desty; // type int offset 4 as xy writedefault;
  int shotType; // type ubyte offset 8 enum TriggerShot writedefault;
  int shotTarget; // type ubyte offset 9 enum TriggerShotTarget writedefault;
  /*bool*/int bSilent; // type negbool offset 10; // negbool!
  int aim; // type byte offset 11 enum TriggerShotAim default TRIGGER_SHOT_AIM_DEFAULT;
  int panelId; // type int offset 12 panel default null writedefault;
  int sight; // type ushort offset 16;
  int angle; // type ushort offset 18;
  int wait; // type ushort offset 20;
  int accuracy; // type ushort offset 22;
  int ammo; // type ushort offset 24;
  int reload; // type ushort offset 26;
};

// TRIGGER_EFFECT
struct /*readonly*/ TriggerEffect : Trigger {
  int fxCount; // type ubyte offset 0 writedefault;
  int fxType; // type ubyte offset 1 enum TriggerEffect default TRIGGER_EFFECT_PARTICLE writedefault;
  int fxSubType; // type ubyte offset 2 enum TriggerEffectType default TRIGGER_EFFECT_SPARK writedefault;
  int fxRed; // type ubyte offset 3 writedefault;
  int fxGreen; // type ubyte offset 4 writedefault;
  int fxBlue; // type ubyte offset 5 writedefault;
  int fxPos; // type ubyte offset 6 enum TriggerEffectPos default TRIGGER_EFFECT_POS_CENTER writedefault;
  int wait; // type ushort offset 8 writedefault;
  int velX; // type byte offset 10 writedefault;
  int velY; // type byte offset 11 writedefault;
  int spreadL; // type ubyte offset 12 writedefault;
  int spreadR; // type ubyte offset 13 writedefault;
  int spreadU; // type ubyte offset 14 writedefault;
  int spreadD; // type ubyte offset 15 writedefault;
};


#endif // DFMAP_STRUCTS_HEADER
