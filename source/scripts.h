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

class VScriptParser {
public:
  int Line;
  int TokLine;
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
  char *TokStartPtr;
  int TokStartLine;
  int ScriptSize;
  int SrcIdx;
  //bool AlreadyGot;
  bool CMode;
  bool Escape;
  bool AllowNumSign;

private:
  VScriptParser () {}

  // advances current position
  // if `changeFlags` is `true`, changes `Crossed` and `Line`
  void SkipComments (bool changeFlags);
  void SkipBlanks (bool changeFlags);

  // slow! returns 0 on EOF
  char PeekOrSkipChar (bool doSkip);

public:
  // deletes `Strm`
  VScriptParser (const VStr &name, VStream *Strm);
  VScriptParser (const VStr &name, const char *atext);

  ~VScriptParser ();

  VScriptParser (const VScriptParser &) = delete;
  VScriptParser &operator = (const VScriptParser &) = delete;

  VScriptParser *clone () const;

  bool IsText ();
  bool IsAtEol ();
  inline void SetCMode (bool val) { CMode = val; }
  inline void SetEscape (bool val) { Escape = val; }
  bool AtEnd ();
  bool GetString ();
#if !defined(VCC_STANDALONE_EXECUTOR)
  vuint32 ExpectColor (); // returns parsed color, either in string form, or r,g,b triplet
#endif
  void ExpectString ();
  void ExpectLoneChar (); // in `String`
  void ExpectName8 ();
  void ExpectName8Warn (); // this sets both `Name` and `Name8`, with warning
  void ExpectName8Def (VName def);
  void ExpectName ();
  void ExpectIdentifier ();
  bool Check (const char *);
  bool CheckStartsWith (const char *);
  void Expect (const char *);
  bool CheckQuotedString ();
  bool CheckIdentifier ();
  bool CheckNumber ();
  void ExpectNumber (bool allowFloat=false, bool truncFloat=true);
  bool CheckNumberWithSign ();
  void ExpectNumberWithSign ();
  bool CheckFloat ();
  void ExpectFloat ();
  bool CheckFloatWithSign ();
  void ExpectFloatWithSign ();
  void ResetQuoted ();
  void ResetCrossed ();
  void UnGet ();
  void SkipBracketed (bool bracketEaten=false);
  void SkipLine ();
  void Message (const char *);
  void Error (const char *);
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  void HostError (const char *);
#endif

  // slow! returns 0 on EOF
  // doesn't affect flags
  inline char PeekChar () { return PeekOrSkipChar(false); }
  // affects flags
  inline char SkipChar () { return PeekOrSkipChar(true); }

  TLocation GetLoc ();

  inline const VStr &GetScriptName () const { return ScriptName; }
  inline bool IsCMode () const { return CMode; }
  inline bool IsEscape () const { return Escape; }

  // for C mode
  inline bool IsAllowNumSign () const { return AllowNumSign; }
  inline void SetAllowNumSign (bool v) { AllowNumSign = v; }
};
