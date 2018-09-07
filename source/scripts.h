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

class VScriptParser {
public:
  int Line;
  bool End;
  bool Crossed;
  bool QuotedString;
  VStr String;
  VName Name8;
  VName Name;
  int Number;
  double Float;

private:
  VStr ScriptName;
  char *ScriptBuffer;
  char *ScriptPtr;
  char *ScriptEndPtr;
  int ScriptSize;
  int SrcIdx;
  bool AlreadyGot;
  bool CMode;
  bool Escape;

public:
  VScriptParser (const VStr &name, VStream *Strm);
  VScriptParser (const VStr &name, const char *atext);
  ~VScriptParser ();

  bool IsText ();
  bool IsAtEol ();
  inline void SetCMode (bool val) { CMode = val; }
  inline void SetEscape (bool val) { Escape = val; }
  bool AtEnd ();
  bool GetString ();
  void ExpectString ();
  void ExpectName8 ();
  void ExpectName ();
  void ExpectIdentifier ();
  bool Check (const char *);
  void Expect (const char *);
  bool CheckQuotedString ();
  bool CheckIdentifier ();
  bool CheckNumber ();
  void ExpectNumber (bool allowFloat=false);
  bool CheckNumberWithSign ();
  void ExpectNumberWithSign ();
  bool CheckFloat ();
  void ExpectFloat ();
  bool CheckFloatWithSign ();
  void ExpectFloatWithSign ();
  void UnGet ();
  void Message (const char *);
  void Error (const char *);
  TLocation GetLoc ();
  inline const VStr &GetScriptName () const { return ScriptName; }
  inline bool IsCMode () const { return CMode; }
  inline bool IsEscape () const { return Escape; }
};
