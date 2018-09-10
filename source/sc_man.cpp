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

// HEADER FILES ------------------------------------------------------------

#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
# include "gamedefs.h"
#else
# if defined(IN_VCC)
#  include "../utils/vcc/vcc.h"
# elif defined(VCC_STANDALONE_EXECUTOR)
#  include "../vccrun/vcc_run.h"
# endif
#endif

#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
static VCvarB dbg_show_name_remap("dbg_show_name_remap", false, "Show hacky name remapping", 0);
#endif

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

class VScriptsParser : public VObject {
  DECLARE_CLASS(VScriptsParser, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VScriptsParser)

  VScriptParser *Int;

  void Destroy ();
  void CheckInterface ();

#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  DECLARE_FUNCTION(OpenLumpName)
#endif
  DECLARE_FUNCTION(OpenString)
  DECLARE_FUNCTION(get_String)
  DECLARE_FUNCTION(get_Number)
  DECLARE_FUNCTION(get_Float)
  DECLARE_FUNCTION(get_Crossed)
  DECLARE_FUNCTION(IsText)
  DECLARE_FUNCTION(IsCMode)
  DECLARE_FUNCTION(SetCMode)
  DECLARE_FUNCTION(AtEnd)
  DECLARE_FUNCTION(GetString)
  DECLARE_FUNCTION(ExpectString)
  DECLARE_FUNCTION(Check)
  DECLARE_FUNCTION(Expect)
  DECLARE_FUNCTION(CheckNumber)
  DECLARE_FUNCTION(ExpectNumber)
  DECLARE_FUNCTION(CheckFloat)
  DECLARE_FUNCTION(ExpectFloat)
  DECLARE_FUNCTION(CheckNumberWithSign)
  DECLARE_FUNCTION(ExpectNumberWithSign)
  DECLARE_FUNCTION(CheckFloatWithSign)
  DECLARE_FUNCTION(ExpectFloatWithSign)
  DECLARE_FUNCTION(ResetQuoted)
  DECLARE_FUNCTION(ResetCrossed)
  DECLARE_FUNCTION(SkipBracketed)
  DECLARE_FUNCTION(UnGet)
  DECLARE_FUNCTION(ScriptError)
  DECLARE_FUNCTION(ScriptMessage)
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

IMPLEMENT_CLASS(V, ScriptsParser)

// CODE --------------------------------------------------------------------


//==========================================================================
//
//  VScriptParser::VScriptParser
//
//==========================================================================
VScriptParser::VScriptParser (const VStr &name, VStream *Strm)
  : Line(1)
  , End(false)
  , Crossed(false)
  , QuotedString(false)
  , ScriptName(name)
  , SrcIdx(-1)
  , AlreadyGot(false)
  , CMode(false)
  , Escape(true)
{
  //guard(VScriptParser::VScriptParser);

  ScriptSize = Strm->TotalSize();
  ScriptBuffer = new char[ScriptSize+1];
  Strm->Serialise(ScriptBuffer, ScriptSize);
  ScriptBuffer[ScriptSize] = 0;
  delete Strm;

  ScriptPtr = ScriptBuffer;
  ScriptEndPtr = ScriptPtr+ScriptSize;

  // skip garbage some editors add in the begining of UTF-8 files
  if ((vuint8)ScriptPtr[0] == 0xef && (vuint8)ScriptPtr[1] == 0xbb && (vuint8)ScriptPtr[2] == 0xbf) ScriptPtr += 3;

  //unguard;
}


//==========================================================================
//
//  VScriptParser::VScriptParser
//
//==========================================================================
VScriptParser::VScriptParser (const VStr &name, const char *atext)
  : Line(1)
  , End(false)
  , Crossed(false)
  , QuotedString(false)
  , ScriptName(name)
  , SrcIdx(-1)
  , AlreadyGot(false)
  , CMode(false)
  , Escape(true)
{
  //guard(VScriptParser::VScriptParser);

  if (atext && atext[0]) {
    ScriptSize = (int)strlen(atext);
    ScriptBuffer = new char[ScriptSize+1];
    memcpy(ScriptBuffer, atext, ScriptSize+1);
  } else {
    ScriptSize = 1;
    ScriptBuffer = new char[ScriptSize+1];
    ScriptBuffer[0] = 0;
  }

  ScriptPtr = ScriptBuffer;
  ScriptEndPtr = ScriptPtr+ScriptSize;

  // skip garbage some editors add in the begining of UTF-8 files
  if ((vuint8)ScriptPtr[0] == 0xef && (vuint8)ScriptPtr[1] == 0xbb && (vuint8)ScriptPtr[2] == 0xbf) ScriptPtr += 3;

  //unguard;
}


//==========================================================================
//
//  VScriptParser::~VScriptParser
//
//==========================================================================
VScriptParser::~VScriptParser () {
  //guard(VScriptParser::~VScriptParser);
  delete[] ScriptBuffer;
  ScriptBuffer = nullptr;
  //unguard;
}


//==========================================================================
//
// VScriptParser::IsText
//
//==========================================================================
bool VScriptParser::IsText () {
  int i = 0;
  while (i < ScriptSize) {
    vuint8 ch = ScriptBuffer[i++];
    if (ch == 127) return false;
    if (ch < ' ' && ch != '\n' && ch != '\r' && ch != '\t') return false;
    if (ch < 128) continue;
    // utf8 check
    int cnt, val;
         if ((ch&0xe0) == 0xc0) { val = ch&0x1f; cnt = 1; }
    else if ((ch&0xf0) == 0xe0) { val = ch&0x0f; cnt = 2; }
    else if ((ch&0xf8) == 0xf0) { val = ch&0x07; cnt = 3; }
    else return false; // invalid utf8
    do {
      if (i >= ScriptSize) return false;
      ch = ScriptBuffer[i++];
      if ((ch&0xc0) != 0x80) return false; // invalid utf8
      val = (val<<6)|(ch&0x3f);
    } while (--cnt);
    // check for valid codepoint
    if (!(val < 0xD800 || (val > 0xDFFF && val <= 0x10FFFF))) return false; // invalid codepoint
  }
  return true;
}


//==========================================================================
//
//  VScriptParser::AtEnd
//
//==========================================================================
bool VScriptParser::AtEnd () {
  guard(VScriptParser::AtEnd);
  if (GetString()) {
    UnGet();
    return false;
  }
  return true;
  unguard;
}


//==========================================================================
//
//  VScriptParser::IsAtEol
//
//==========================================================================
bool VScriptParser::IsAtEol () {
  bool inComment = false;
  for (const char *s = ScriptPtr; s < ScriptEndPtr; ++s) {
    if (*s == '\n' || *s == '\r') return true;
    if (!inComment) {
      if (s[0] == '/' && s[1] == '/') return true; // this is single-line comment, it always ends with EOL
      if (s[0] == '/' && s[1] == '*') {
        // multiline comment
        ++s; // skip slash
        inComment = true;
        continue;
      }
      if ((vuint8)(*s) > ' ') return false;
    } else {
      // in multiline comment
      if (s[0] == '*' && s[1] == '/') {
        ++s; // skip star
        inComment = false;
      }
    }
  }
  return true;
}


//==========================================================================
//
//  VScriptParser::GetString
//
//==========================================================================
bool VScriptParser::GetString () {
  guard(VScriptParser::GetString);
  // check if we already have a token available
  if (AlreadyGot) {
    AlreadyGot = false;
    return true;
  }

  // check for end of script
  if (ScriptPtr >= ScriptEndPtr) {
    End = true;
    return false;
  }

  Crossed = false;
  QuotedString = false;
  bool foundToken = false;
  while (foundToken == false) {
    // skip whitespace
    while (*ScriptPtr <= 32) {
      if (ScriptPtr >= ScriptEndPtr) {
        End = true;
        return false;
      }
      // check for new-line character
      if (*ScriptPtr++ == '\n') {
        ++Line;
        Crossed = true;
      }
    }

    // check for end of script
    if (ScriptPtr >= ScriptEndPtr) {
      End = true;
      return false;
    }

    // check for comments
    if ((!CMode && *ScriptPtr == ';') || (ScriptPtr[0] == '/' && ScriptPtr[1] == '/')) {
      // skip comment
      while (*ScriptPtr++ != '\n') {
        if (ScriptPtr >= ScriptEndPtr) {
          End = true;
          return false;
        }
      }
      ++Line;
      Crossed = true;
    } else if (ScriptPtr[0] == '/' && ScriptPtr[1] == '*') {
      // skip comment
      ScriptPtr += 2;
      while (ScriptPtr[0] != '*' || ScriptPtr[1] != '/') {
        if (ScriptPtr >= ScriptEndPtr) {
          End = true;
          return false;
        }
        // check for new-line character
        if (*ScriptPtr == '\n') {
          ++Line;
          Crossed = true;
        }
        ++ScriptPtr;
      }
      ScriptPtr += 2;
    } else {
      // found a token
      foundToken = true;
    }
  }

  String.Clean();
  if (*ScriptPtr == '\"') {
    // quoted string
    QuotedString = true;
    ++ScriptPtr;
    while (ScriptPtr < ScriptEndPtr && *ScriptPtr != '\"') {
      if (Escape && ScriptPtr[0] == '\\' && (ScriptPtr[1] == '\\' || ScriptPtr[1] == '\"')) {
        ++ScriptPtr;
      } else if (ScriptPtr[0] == '\r' && ScriptPtr[1] == '\n') {
        // convert from DOS format to UNIX format
        ++ScriptPtr;
      }
      if (*ScriptPtr == '\n') {
        if (CMode) {
          if (!Escape || String.Length() == 0 || String[String.Length() - 1] != '\\') {
            Error("Unterminated string constant");
          } else {
            // remove the \ character
            String = VStr(String, 0, String.Length()-1);
          }
        }
        ++Line;
        Crossed = true;
      }
      String += *ScriptPtr++;
    }
    ++ScriptPtr;
  } else if (CMode) {
    if ((ScriptPtr[0] == '&' && ScriptPtr[1] == '&') ||
        (ScriptPtr[0] == '|' && ScriptPtr[1] == '|') ||
        (ScriptPtr[0] == '=' && ScriptPtr[1] == '=') ||
        (ScriptPtr[0] == '!' && ScriptPtr[1] == '=') ||
        (ScriptPtr[0] == '>' && ScriptPtr[1] == '=') ||
        (ScriptPtr[0] == '<' && ScriptPtr[1] == '=') ||
        (ScriptPtr[0] == '<' && ScriptPtr[1] == '<') ||
        (ScriptPtr[0] == '>' && ScriptPtr[1] == '>') ||
        (ScriptPtr[0] == ':' && ScriptPtr[1] == ':'))
    {
      // special double-character token
      String += *ScriptPtr++;
      String += *ScriptPtr++;
    } else if ((ScriptPtr[0] >= '0' && ScriptPtr[0] <= '9') ||
               (ScriptPtr[0] == '.' && ScriptPtr[1] >= '0' && ScriptPtr[1] <= '9'))
    {
      // number
      while (*ScriptPtr > 32 && !strchr("`~!@#$%^&*(){}[]/=\\?-+|;:<>,\"", *ScriptPtr)) {
        String += *ScriptPtr++;
        if (ScriptPtr == ScriptEndPtr) break;
      }
    } else if (strchr("`~!@#$%^&*(){}[]/=\\?-+|;:<>,.", *ScriptPtr)) {
      // special single-character token
      String += *ScriptPtr++;
    } else {
      // normal string
      while (*ScriptPtr > 32 && !strchr("`~!@#$%^&*(){}[]/=\\?-+|;:<>,\".", *ScriptPtr)) {
        String += *ScriptPtr++;
        if (ScriptPtr == ScriptEndPtr) break;
      }
    }
  } else {
    // special single-character tokens
    if (strchr("{}|=", *ScriptPtr)) {
      String += *ScriptPtr++;
    } else {
      // normal string
      while (*ScriptPtr > 32 && !strchr("{}|=;\"", *ScriptPtr) &&
             (ScriptPtr[0] != '/' || ScriptPtr[1] != '/') &&
             (ScriptPtr[0] != '/' || ScriptPtr[1] != '*'))
      {
        String += *ScriptPtr++;
        if (ScriptPtr == ScriptEndPtr) break;
      }
    }
  }
  return true;
  unguard;
}


//==========================================================================
//
//  VScriptParser::ExpectString
//
//==========================================================================
void VScriptParser::ExpectString () {
  guard(VScriptParser::ExpectString);
  if (!GetString()) Error("Missing string.");
  unguard;
}


//==========================================================================
//
//  VScriptParser::ExpectName8
//
//==========================================================================
void VScriptParser::ExpectName8 () {
  guard(VScriptParser::ExpectName8);
  ExpectString();

#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  // translate "$name" strings
  if (String.Length() > 1 && String[0] == '$') {
    VStr qs = String.mid(1, String.length()-1).toLowerCase();
    if (GLanguage.HasTranslation(*qs)) {
      qs = *GLanguage[*qs];
      if (dbg_show_name_remap) GCon->Logf("**** <%s>=<%s>\n", *String, *qs);
      String = qs;
    }
  }
#endif

  if (String.Length() > 8) {
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
    GCon->Logf("Name '%s' is too long", *String);
#endif
    Error("Name is too long");
  }
  Name8 = VName(*String, VName::AddLower8);
  unguard;
}


//==========================================================================
//
//  VScriptParser::ExpectName
//
//==========================================================================
void VScriptParser::ExpectName () {
  guard(VScriptParser::ExpectName);
  ExpectString();
  Name = VName(*String, VName::AddLower);
  unguard;
}


//==========================================================================
//
//  VScriptParser::Check
//
//==========================================================================
bool VScriptParser::Check (const char *str) {
  guard(VScriptParser::Check);
  if (GetString()) {
    if (!String.ICmp(str)) return true;
    UnGet();
  }
  return false;
  unguard;
}


//==========================================================================
//
//  VScriptParser::Expect
//
//==========================================================================
void VScriptParser::Expect (const char *name) {
  guard(VScriptParser::Expect);
  ExpectString();
  if (String.ICmp(name)) Error(va("Bad syntax, \"%s\" expected", name));
  unguard;
}


//==========================================================================
//
//  VScriptParser::CheckQuotedString
//
//==========================================================================
bool VScriptParser::CheckQuotedString () {
  guard(VScriptParser::CheckQuotedString);
  if (!GetString()) return false;
  if (!QuotedString) {
    UnGet();
    return false;
  }
  return true;
  unguard;
}


//==========================================================================
//
//  VScriptParser::CheckIdentifier
//
//==========================================================================
bool VScriptParser::CheckIdentifier () {
  guard(VScriptParser::CheckIdentifier);
  if (!GetString()) return false;

  // quoted strings are not valid identifiers
  if (QuotedString) {
    UnGet();
    return false;
  }

  if (String.Length() < 1) {
    UnGet();
    return false;
  }

  // identifier must start with a letter, a number or an underscore
  char c = String[0];
  if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
    UnGet();
    return false;
  }

  // it must be followed by letters, numbers and underscores
  for (int i = 1; i < String.Length(); ++i) {
    c = String[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
      UnGet();
      return false;
    }
  }
  return true;
  unguard;
}


//==========================================================================
//
//  VScriptParser::ExpectIdentifier
//
//==========================================================================
void VScriptParser::ExpectIdentifier () {
  guard(VScriptParser::ExpectIdentifier);
  if (!CheckIdentifier()) Error(va("Identifier expected, got \"%s\".", *String));
  unguard;
}


//==========================================================================
//
//  VScriptParser::CheckNumber
//
//==========================================================================
bool VScriptParser::CheckNumber () {
  guard(VScriptParser::CheckNumber);
  if (GetString()) {
    char *stopper;
    Number = strtol(*String, &stopper, 0);
    if (*stopper == 0) return true;
    UnGet();
  }
  return false;
  unguard;
}


//==========================================================================
//
//  VScriptParser::ExpectNumber
//
//==========================================================================
void VScriptParser::ExpectNumber (bool allowFloat) {
  guard(VScriptParser::ExpectNumber);
  if (GetString()) {
    char *stopper;
    Number = strtol(*String, &stopper, 0);
    if (*stopper != 0) {
      if (allowFloat && *stopper == '.') {
        if (stopper[1] >= '5') ++Number;
        if (Number == 0) Number = 1; // just in case
        Message(va("Bad numeric constant \"%s\" (integer expected; rounded to %d).", *String, (int)Number));
        //fprintf(stderr, "%d\n", (int)Number);
        //Error(va("Bad numeric constant \"%s\".", *String));
      } else {
        Error(va("Bad numeric constant \"%s\".", *String));
      }
    }
  } else {
    Error("Missing integer.");
  }
  unguard;
}


//==========================================================================
//
//  VScriptParser::CheckNumberWithSign
//
//==========================================================================
bool VScriptParser::CheckNumberWithSign () {
  guard(VScriptParser::CheckNumberWithSign);
  if (Check("-")) {
    ExpectNumber();
    Number = -Number;
    return true;
  } else {
    return CheckNumber();
  }
  unguard;
}


//==========================================================================
//
//  VScriptParser::ExpectNumberWithSign
//
//==========================================================================
void VScriptParser::ExpectNumberWithSign () {
  guard(VScriptParser::ExpectNumberWithSign);
  if (Check("-")) {
    ExpectNumber();
    Number = -Number;
  } else {
    ExpectNumber();
  }
  unguard;
}


//==========================================================================
//
//  VScriptParser::CheckFloat
//
//==========================================================================
bool VScriptParser::CheckFloat () {
  guard(VScriptParser::CheckFloat);
  if (GetString()) {
    char *stopper;
    Float = strtod(*String, &stopper);
    if (*stopper == 0) return true;
    UnGet();
  }
  return false;
  unguard;
}


//==========================================================================
//
//  VScriptParser::ExpectFloat
//
//==========================================================================
void VScriptParser::ExpectFloat () {
  guard(VScriptParser::ExpectFloat);
  if (GetString()) {
    //FIXME: detect when we want to use a really big number
    if (String.ToLower().StartsWith("0x7f") || String.ToLower().StartsWith("0xff")) {
      Float = 99999.0;
    } else {
      char *stopper;
      Float = strtod(*String, &stopper);
      if (*stopper != 0) Error(va("Bad floating point constant \"%s\".", *String));
    }
  } else {
    Error("Missing float.");
  }
  unguard;
}


//==========================================================================
//
//  VScriptParser::CheckFloatWithSign
//
//==========================================================================
bool VScriptParser::CheckFloatWithSign () {
  guard(VScriptParser::CheckFloatWithSign);
  if (Check("-")) {
    ExpectFloat();
    Float = -Float;
    return true;
  } else {
    return CheckFloat();
  }
  unguard;
}


//==========================================================================
//
//  VScriptParser::ExpectFloatWithSign
//
//==========================================================================
void VScriptParser::ExpectFloatWithSign () {
  guard(VScriptParser::ExpectFloatWithSign);
  if (Check("-")) {
    ExpectFloat();
    Float = -Float;
  } else {
    ExpectFloat();
  }
  unguard;
}


//==========================================================================
//
//  VScriptParser::ResetQuoted
//
//==========================================================================
void VScriptParser::ResetQuoted () {
  if (!AlreadyGot) QuotedString = false;
}


//==========================================================================
//
//  VScriptParser::ResetCrossed
//
//==========================================================================
void VScriptParser::ResetCrossed () {
  if (!AlreadyGot) Crossed = false;
}


//==========================================================================
//
//  VScriptParser::UnGet
//
//  Assumes there is a valid string in sc_String.
//
//==========================================================================
void VScriptParser::UnGet () {
  guard(VScriptParser::UnGet);
  AlreadyGot = true;
  unguard;
}


//==========================================================================
//
//  VScriptParser::SkipBracketed
//
//==========================================================================
void VScriptParser::SkipBracketed (bool bracketEaten) {
  if (!bracketEaten) {
    for (;;) {
      ResetQuoted();
      if (!GetString()) return;
      if (QuotedString) continue;
      if (String.length() == 1 && String[0] == '{') {
        break;
      }
    }
  }
  int level = 1;
  for (;;) {
    ResetQuoted();
    if (!GetString()) break;
    if (QuotedString) continue;
    if (String.length() == 1) {
      if (String[0] == '{') {
        ++level;
      } else if (String[0] == '}') {
        if (--level == 0) return;
      }
    }
  }
}


//==========================================================================
//
//  VScriptParser::Message
//
//==========================================================================
void VScriptParser::Message (const char *message) {
  guard(VScriptParser::Message)
  const char *Msg = (message ? message : "Bad syntax.");
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  GCon->Logf("\"%s\" line %d: %s", *ScriptName, Line, Msg);
#else
  printf("\"%s\" line %d: %s\n", *ScriptName, Line, Msg);
#endif
  unguard;
}


//==========================================================================
//
//  VScriptParser::Error
//
//==========================================================================
void VScriptParser::Error (const char *message) {
  guard(VScriptParser::Error)
  const char *Msg = (message ? message : "Bad syntax.");
  Sys_Error("Script error, \"%s\" line %d: %s", *ScriptName, Line, Msg);
  unguard;
}


//==========================================================================
//
//  VScriptParser::GetLoc
//
//==========================================================================
TLocation VScriptParser::GetLoc () {
  guardSlow(VScriptParser::GetLoc);
  if (SrcIdx == -1) SrcIdx = TLocation::AddSourceFile(ScriptName);
  return TLocation(SrcIdx, Line, 1);
  unguardSlow;
}


//==========================================================================
//
//  VScriptsParser::Destroy
//
//==========================================================================
void VScriptsParser::Destroy () {
  guard(VScriptsParser::Destroy);
  if (Int) {
    delete Int;
    Int = nullptr;
  }
  Super::Destroy();
  unguard;
}


//==========================================================================
//
//  VScriptsParser::CheckInterface
//
//==========================================================================
void VScriptsParser::CheckInterface () {
  guard(VScriptsParser::CheckInterface);
  if (!Int) Sys_Error("No script currently open");
  unguard;
}


//==========================================================================
//
//  VScriptsParser natives
//
//==========================================================================

#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
IMPLEMENT_FUNCTION(VScriptsParser, OpenLumpName) {
  P_GET_NAME(Name);
  P_GET_SELF;
  if (Self->Int)
  {
    delete Self->Int;
    Self->Int = nullptr;
  }
  Self->Int = new VScriptParser(*Name, W_CreateLumpReaderName(Name));
}
#endif

IMPLEMENT_FUNCTION(VScriptsParser, OpenString) {
  P_GET_STR(s);
  P_GET_NAME(Name);
  P_GET_SELF;
  if (Self->Int) {
    delete Self->Int;
    Self->Int = nullptr;
  }
  Self->Int = new VScriptParser(*Name, *s);
}

IMPLEMENT_FUNCTION(VScriptsParser, get_String) {
  P_GET_SELF;
  Self->CheckInterface();
  RET_STR(Self->Int->String);
}

IMPLEMENT_FUNCTION(VScriptsParser, get_Number) {
  P_GET_SELF;
  Self->CheckInterface();
  RET_INT(Self->Int->Number);
}

IMPLEMENT_FUNCTION(VScriptsParser, get_Float) {
  P_GET_SELF;
  Self->CheckInterface();
  RET_FLOAT(Self->Int->Float);
}

IMPLEMENT_FUNCTION(VScriptsParser, get_Crossed) {
  P_GET_SELF;
  Self->CheckInterface();
  RET_BOOL(Self->Int->Crossed);
}

IMPLEMENT_FUNCTION(VScriptsParser, IsCMode) {
  P_GET_SELF;
  Self->CheckInterface();
  RET_BOOL(Self->Int->IsCMode());
}

IMPLEMENT_FUNCTION(VScriptsParser, SetCMode) {
  P_GET_BOOL(On);
  P_GET_SELF;
  Self->CheckInterface();
  Self->Int->SetCMode(On);
}

IMPLEMENT_FUNCTION(VScriptsParser, AtEnd) {
  P_GET_SELF;
  Self->CheckInterface();
  RET_BOOL(Self->Int->AtEnd());
}

IMPLEMENT_FUNCTION(VScriptsParser, GetString) {
  P_GET_SELF;
  Self->CheckInterface();
  RET_BOOL(Self->Int->GetString());
}

IMPLEMENT_FUNCTION(VScriptsParser, ExpectString) {
  P_GET_SELF;
  Self->CheckInterface();
  Self->Int->ExpectString();
}

IMPLEMENT_FUNCTION(VScriptsParser, Check) {
  P_GET_STR(Text);
  P_GET_SELF;
  Self->CheckInterface();
  RET_BOOL(Self->Int->Check(*Text));
}

IMPLEMENT_FUNCTION(VScriptsParser, Expect) {
  P_GET_STR(Text);
  P_GET_SELF;
  Self->CheckInterface();
  Self->Int->Expect(*Text);
}

IMPLEMENT_FUNCTION(VScriptsParser, CheckNumber) {
  P_GET_SELF;
  Self->CheckInterface();
  RET_BOOL(Self->Int->CheckNumber());
}

IMPLEMENT_FUNCTION(VScriptsParser, ExpectNumber) {
  P_GET_SELF;
  Self->CheckInterface();
  Self->Int->ExpectNumber();
}

IMPLEMENT_FUNCTION(VScriptsParser, CheckFloat) {
  P_GET_SELF;
  Self->CheckInterface();
  RET_BOOL(Self->Int->CheckFloat());
}

IMPLEMENT_FUNCTION(VScriptsParser, ExpectFloat) {
  P_GET_SELF;
  Self->CheckInterface();
  Self->Int->ExpectFloat();
}

IMPLEMENT_FUNCTION(VScriptsParser, CheckNumberWithSign) {
  P_GET_SELF;
  Self->CheckInterface();
  RET_BOOL(Self->Int->CheckNumberWithSign());
}

IMPLEMENT_FUNCTION(VScriptsParser, ExpectNumberWithSign) {
  P_GET_SELF;
  Self->CheckInterface();
  Self->Int->ExpectNumberWithSign();
}

IMPLEMENT_FUNCTION(VScriptsParser, CheckFloatWithSign) {
  P_GET_SELF;
  Self->CheckInterface();
  RET_BOOL(Self->Int->CheckFloatWithSign());
}

IMPLEMENT_FUNCTION(VScriptsParser, ExpectFloatWithSign) {
  P_GET_SELF;
  Self->CheckInterface();
  Self->Int->ExpectFloatWithSign();
}

IMPLEMENT_FUNCTION(VScriptsParser, ResetQuoted) {
  P_GET_SELF;
  Self->CheckInterface();
  Self->Int->ResetQuoted();
}

IMPLEMENT_FUNCTION(VScriptsParser, ResetCrossed) {
  P_GET_SELF;
  Self->CheckInterface();
  Self->Int->ResetCrossed();
}

IMPLEMENT_FUNCTION(VScriptsParser, SkipBracketed) {
  P_GET_BOOL_OPT(bracketEaten, false);
  P_GET_SELF;
  Self->CheckInterface();
  Self->Int->SkipBracketed(bracketEaten);
}

IMPLEMENT_FUNCTION(VScriptsParser, UnGet) {
  P_GET_SELF;
  Self->CheckInterface();
  Self->Int->UnGet();
}

IMPLEMENT_FUNCTION(VScriptsParser, ScriptError) {
  VStr Msg = PF_FormatString();
  P_GET_SELF;
  Self->CheckInterface();
  Self->Int->Error(*Msg);
}

IMPLEMENT_FUNCTION(VScriptsParser, ScriptMessage) {
  VStr Msg = PF_FormatString();
  P_GET_SELF;
  Self->CheckInterface();
  Self->Int->Message(*Msg);
}
