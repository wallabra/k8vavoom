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

enum {
  CVAR_None       = 0,
  CVAR_Archive    = 0x0001, // set to cause it to be saved to config.cfg
  CVAR_UserInfo   = 0x0002, // added to userinfo when changed
  CVAR_ServerInfo = 0x0004, // added to serverinfo when changed
  CVAR_Init       = 0x0008, // don't allow change from console at all, but can be set from the command line
  CVAR_Latch      = 0x0010, // save changes until server restart
  CVAR_Rom        = 0x0020, // display only, cannot be set by user at all
  CVAR_Cheat      = 0x0040, // can not be changed if cheats are disabled
  CVAR_Modified   = 0x0080, // set each time the cvar is changed
  CVAR_FromMod    = 0x0100, // this cvar came from cvarinfo
  CVAR_PreInit    = 0x4000, // CLI change for this cvar should be processed before initializing the main game
  //
  CVAR_AlwaysArchive = 0x8000, // always write to config
};


// ////////////////////////////////////////////////////////////////////////// //
// console variable
class VCvar {
protected:
  const char *Name; // variable name
  const char *DefaultString; // default value
  const char *HelpString; // this *can* be owned, but as we never deleting cvar objects, it doesn't matter
  bool defstrOwned; // `true` if `DefaultString` is owned and should be deleted
  VStr StringValue; // current value
  int Flags; // `CVAR_XXX` flags
  int IntValue; // atoi(string)
  float FloatValue; // atof(string)
  bool BoolValue; // interprets various "true" strings
  VStr LatchedString; // for `CVAR_Latch` variables
  VCvar *nextInBucket; // next cvar in this bucket
  vuint32 lnhash; // hash of lo-cased variable name

public:
  VCvar (const char *AName, const char *ADefault, const char *AHelp, int AFlags=CVAR_None);
  VCvar (const char *AName, const VStr &ADefault, const VStr &AHelp, int AFlags=CVAR_None);

  static void AddAllVarsToAutocomplete (void (*addfn) (const char *name));

  void Set (int value);
  void Set (float value);
  void Set (const VStr &value);

  // this clears modified flag on call
  inline bool IsModified () {
    bool ret = !!(Flags&CVAR_Modified);
    // clear modified flag
    Flags &= ~CVAR_Modified;
    return ret;
  }

  inline bool IsModifiedNoClear () const { return !!(Flags&CVAR_Modified); }

  inline bool IsPreInit () const { return !!(Flags&CVAR_PreInit); }
  inline bool IsModVar () const { return ((Flags&CVAR_FromMod) != 0); }

  inline bool asBool () const { return BoolValue; }
  inline int asInt () const { return IntValue; }
  inline float asFloat () const { return FloatValue; }
  inline const VStr &asStr () const { return StringValue; }

  inline int GetFlags () const { return Flags; }
  inline const char *GetName () const { return Name; }
  inline const char *GetHelp () const { return (HelpString ? HelpString : "no help yet: FIXME!!!"); }
  inline const char *GetDefault () const { return DefaultString; }

  static void Init ();
  static void Shutdown ();

  static bool HasVar (const char *var_name);
  static bool HasModVar (const char *var_name);
  static bool HasModUserVar (const char *var_name); // non-server

  static bool CanBeModified (const char *var_name, bool modonly=false, bool noserver=true);

  static void CreateNew (VName var_name, const VStr &ADefault, const VStr &AHelp, int AFlags);

  static int GetInt (const char *var_name);
  static float GetFloat (const char *var_name);
  static bool GetBool (const char *var_name);
  static const char *GetCharp (const char *var_name);
  static const VStr &GetString (const char *var_name);
  static const char *GetHelp (const char *var_name); // returns nullptr if there is no such cvar
  static int GetVarFlags (const char *var_name); // -1: not found

  static void Set (const char *var_name, int value);
  static void Set (const char *var_name, float value);
  static void Set (const char *var_name, const VStr &value);

  static bool Command (const TArray<VStr> &Args);
  static void WriteVariablesToStream (VStream *st, bool saveDefaultValues=false);

  static void Unlatch ();

  static inline void SetCheating (bool newState) { Cheating = newState; }
  static inline bool GetCheating () { return Cheating; }

  static VCvar *FindVariable (const char *name);

  static bool ParseBool (const char *s);

  // call this to disable setting `CVAR_Init` cvars
  static void HostInitComplete ();

public:
  // required for various console commands
  static vuint32 countCVars ();
  static VCvar **getSortedList (); // contains `countCVars()` elements, must be `delete[]`d

private:
  VCvar (const VCvar &);
  void operator = (const VCvar &);

  static void dumpHashStats ();

  void insertIntoList ();
  VCvar *insertIntoHash ();
  void DoSet(const VStr &value);

  static bool Initialised;
  static bool Cheating;

public:
  // called on any change, before other callbacks
  static void (*ChangedCB) (VCvar *cvar, const VStr &oldValue);

  // the following callbacks are called only after `Init() was called`
  // called when cvar with `CVAR_UserInfo` changed
  static void (*UserInfoSetCB) (VCvar *cvar);
  // called when cvar with `CVAR_ServerInfo` changed
  static void (*ServerInfoSetCB) (VCvar *cvar);
};


class VCvarI;
class VCvarF;
class VCvarS;
class VCvarB;


// ////////////////////////////////////////////////////////////////////////// //
// cvar that can be used as int variable
class VCvarI : public VCvar {
public:
  VCvarI (const char *AName, const char *ADefault, const char *AHelp, int AFlags=0) : VCvar(AName, ADefault, AHelp, AFlags) {}
  VCvarI (const VCvar &);

  inline operator int () const { return IntValue; }
  //inline operator bool () const { return !!IntValue; }
  inline VCvarI &operator = (int AValue) { Set(AValue); return *this; }
  VCvarI &operator = (const VCvar &v);
  VCvarI &operator = (const VCvarB &v);
  VCvarI &operator = (const VCvarI &v);
};


// ////////////////////////////////////////////////////////////////////////// //
// cvar that can be used as float variable
class VCvarF : public VCvar {
public:
  VCvarF (const char *AName, const char *ADefault, const char *AHelp, int AFlags=0) : VCvar(AName, ADefault, AHelp, AFlags) {}
  VCvarF (const VCvar &);

  inline operator float () const { return FloatValue; }
  //inline operator bool () const { return (isFiniteF(FloatValue) && FloatValue == 0); }
  inline VCvarF &operator = (float AValue) { Set(AValue); return *this; }
  VCvarF &operator = (const VCvar &v);
  VCvarF &operator = (const VCvarB &v);
  VCvarF &operator = (const VCvarI &v);
  VCvarF &operator = (const VCvarF &v);
};


// ////////////////////////////////////////////////////////////////////////// //
// cvar that can be used as `char *` variable
class VCvarS : public VCvar {
public:
  VCvarS (const char *AName, const char *ADefault, const char *AHelp, int AFlags=0) : VCvar(AName, ADefault, AHelp, AFlags) {}
  VCvarS (const VCvar &);

  inline operator const char * () const { return *StringValue; }
  //inline operator bool () const { return ParseBool(*StringValue); }
  inline VCvarS &operator = (const char *AValue) { Set(AValue); return *this; }
  VCvarS &operator = (const VCvar &v);
};


// ////////////////////////////////////////////////////////////////////////// //
// cvar that can be used as bool variable
class VCvarB : public VCvar {
public:
  VCvarB (const char *AName, bool ADefault, const char *AHelp, int AFlags=0) : VCvar(AName, (ADefault ? "1" : "0"), AHelp, AFlags) {}

  inline operator bool () const { return BoolValue; }
  VCvarB &operator = (const VCvar &v);
  inline VCvarB &operator = (bool v) { Set(v ? 1 : 0); return *this; }
  VCvarB &operator = (const VCvarB &v);
  VCvarB &operator = (const VCvarI &v);
  VCvarB &operator = (const VCvarF &v);
};
