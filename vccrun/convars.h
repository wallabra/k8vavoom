//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
#ifndef VCCRUN_CONVARS_HEADER_H
#define VCCRUN_CONVARS_HEADER_H

#include "../libs/core/core.h"
#include <stdarg.h>


enum {
  CVAR_Archive    = 0x0001, // Set to cause it to be saved to config.cfg
  CVAR_UserInfo   = 0x0002, // Added to userinfo  when changed
  CVAR_ServerInfo = 0x0004, // Added to serverinfo when changed
  CVAR_Init       = 0x0008, // Don't allow change from console at all, but can be set from the command line
  CVAR_Latch      = 0x0010, // Save changes until server restart
  CVAR_Rom        = 0x0020, // Display only, cannot be set by user at all
  CVAR_Cheat      = 0x0040, // Can not be changed if cheats are disabled
  CVAR_Modified   = 0x0080, // Set each time the cvar is changed
};


// ////////////////////////////////////////////////////////////////////////// //
// console variable
class VCvar {
protected:
  const char *cvname; // Variable's name
  const char *defaultString; // Default value
  const char *helpString; // this *can* be owned, but as we never deleting cvar objects, it doesn't matter
  bool defstrOwned; // `true` if `DefaultString` is owned and should be deleted
  VStr stringValue; // Current value
  int flags; // CVAR_ flags
  int intValue; // atoi(string)
  float floatValue; // atof(string)
  bool boolValue; // interprets various "true" strings
  VStr latchedString; // For CVAR_Latch variables
  VCvar *nextInBucket; // next cvar in this bucket
  vuint32 lnhash; // hash of lo-cased variable name

public:
  static void (*logfn) (const char *fmt, va_list ap);

public:
  VCvar (const char *AName, const char *ADefault, const char *AHelp, int AFlags=0);
  VCvar (const char *AName, const VStr &ADefault, const VStr &AHelp, int AFlags=0);

  void Register ();
  void Set (int value);
  void Set (float value);
  void Set (const VStr &value);

  inline bool asBool () const { return boolValue; }
  inline int asInt () const { return intValue; }
  inline float asFloat () const { return floatValue; }
  inline const VStr &asStr () const { return stringValue; }

  inline bool IsModifiedNoReset () const { return !!(flags&CVAR_Modified); }
  inline bool IsModified (bool dontReset=false) { bool ret = !!(flags&CVAR_Modified); flags &= ~CVAR_Modified; return ret; }
  inline void ResetModified () { flags &= ~CVAR_Modified; }

  inline const char *GetName () const { return cvname; }
  inline const char *GetHelp () const { return (helpString ? helpString : "no help yet."); }

  static void Init ();
  static void Shutdown ();

  static bool HasVar (const char *var_name);
  static void CreateNew (const char *var_name, const VStr &ADefault, const VStr &AHelp, int AFlags);

  static int GetInt (const char *var_name);
  static float GetFloat (const char *var_name);
  static bool GetBool (const char *var_name);
  static const char *GetCharp (const char *var_name);
  static VStr GetString (const char *var_name);
  static const char *GetHelp (const char *var_name); // returns nullptr if there is no such cvar

  static void Set (const char *var_name, int value);
  static void Set (const char *var_name, float value);
  static void Set (const char *var_name, const VStr &value);

  //static bool Command (const TArray<VStr> &Args);
  static void WriteVariablesToFile (FILE *f);

  static void Unlatch ();
  static void SetCheating (bool);

  static VCvar *FindVariable (const char *name);

  //friend class TCmdCvarList;

private:
  VCvar (const VCvar &);
  void operator = (const VCvar &);

  static void dumpHashStats ();
  static vuint32 countCVars ();
  static VCvar **getSortedList (); // contains `countCVars()` elements, must be `delete[]`d

  void insertIntoList ();
  VCvar *insertIntoHash ();
  void DoSet (const VStr &value);

  static bool Initialised;
  static bool Cheating;
};


class VCvarI;
class VCvarF;
class VCvarS;
class VCvarB;


// ////////////////////////////////////////////////////////////////////////// //
// cvar that can be used as `int` variable
class VCvarI : public VCvar {
public:
  VCvarI (const char *AName, const char *ADefault, const char *AHelp, int AFlags=0) : VCvar(AName, ADefault, AHelp, AFlags) {}
  VCvarI (const VCvar &);

  inline operator int () const { return intValue; }
  inline VCvarI &operator = (int AValue) { Set(AValue); return *this; }
  VCvarI &operator = (const VCvar &v);
  VCvarI &operator = (const VCvarB &v);
  VCvarI &operator = (const VCvarI &v);
};

// cvar that can be used as `float` variable
class VCvarF : public VCvar {
public:
  VCvarF (const char *AName, const char *ADefault, const char *AHelp, int AFlags=0) : VCvar(AName, ADefault, AHelp, AFlags) {}

  inline operator float () const { return floatValue; }
  inline VCvarF &operator = (float AValue) { Set(AValue); return *this; }
  VCvarF &operator = (const VCvar &v);
  VCvarF &operator = (const VCvarB &v);
  VCvarF &operator = (const VCvarI &v);
  VCvarF &operator = (const VCvarF &v);
};

// cvar that can be used as `char *` variable
class VCvarS : public VCvar {
public:
  VCvarS (const char *AName, const char *ADefault, const char *AHelp, int AFlags=0) : VCvar(AName, ADefault, AHelp, AFlags) {}

  inline operator const char *() const { return *stringValue; }
  inline VCvarS &operator = (const char *AValue) { Set(AValue); return *this; }
  VCvarS &operator = (const VCvar &v);
};

// cvar that can be used as `bool` variable
class VCvarB : public VCvar {
public:
  VCvarB (const char *AName, bool ADefault, const char *AHelp, int AFlags=0) : VCvar(AName, (ADefault ? "1" : "0"), AHelp, AFlags) {}

  inline operator bool () const { return boolValue; }
  inline VCvarB &operator = (bool v) { Set(v ? 1 : 0); return *this; }
  VCvarB &operator = (const VCvarB &v);
  VCvarB &operator = (const VCvarI &v);
  VCvarB &operator = (const VCvarF &v);
};


// ////////////////////////////////////////////////////////////////////////// //
// WARNING! thread-unsafe!

void ccmdClearText (); // clear command buffer
void ccmdClearCommand (); // clear current command (only)

// parse one command
enum CCResult {
  CCMD_EMPTY = -1, // no more commands (nothing was parsed)
  CCMD_NORMAL = 0, // one command parsed, line is not complete
  CCMD_EOL = 1, // no command parsed, line is complete
};

// this skips empty lines without notice
CCResult ccmdParseOne ();

int ccmdGetArgc (); // 0: nothing was parsed
const VStr &ccmdGetArgv (int idx); // 0: command; other: args, parsed and unquoted

// return number of unparsed bytes left in
int ccmdTextSize ();


void ccmdPrependStr (const char *str);
void ccmdPrependStr (const VStr &str);
void ccmdPrependStrf (const char *fmt, ...) __attribute__((format(printf,1,2)));

void ccmdPrependQuoted (const char *str);
void ccmdPrependQuoted (const VStr &str);
void ccmdPrependQuotdedf (const char *fmt, ...) __attribute__((format(printf,1,2)));

void ccmdAddStr (const char *str);
void ccmdAddStr (const VStr &str);
void ccmdAddStrf (const char *fmt, ...) __attribute__((format(printf,1,2)));

void ccmdAddQuoted (const char *str);
void ccmdAddQuoted (const VStr &str);
void ccmdAddQuotedf (const char *fmt, ...) __attribute__((format(printf,1,2)));


#endif
