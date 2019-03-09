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
static VCvarB dbg_show_name_remap("dbg_show_name_remap", false, "Show hacky name remapping", CVAR_PreInit);
#endif


// ////////////////////////////////////////////////////////////////////////// //
class VScriptsParser : public VObject {
  DECLARE_CLASS(VScriptsParser, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VScriptsParser)

  VScriptParser *Int;

  virtual void Destroy () override;
  void CheckInterface ();

#if !defined(VCC_STANDALONE_EXECUTOR)
  DECLARE_FUNCTION(OpenLumpName)
  DECLARE_FUNCTION(OpenLumpIndex)
#endif
  DECLARE_FUNCTION(OpenLumpFullName)
  DECLARE_FUNCTION(OpenString)
  DECLARE_FUNCTION(get_String)
  DECLARE_FUNCTION(get_Number)
  DECLARE_FUNCTION(get_Float)
  DECLARE_FUNCTION(get_Crossed)
  DECLARE_FUNCTION(get_Quoted)
  DECLARE_FUNCTION(IsText)
  DECLARE_FUNCTION(IsAtEol)
  DECLARE_FUNCTION(IsCMode)
  DECLARE_FUNCTION(SetCMode)
  DECLARE_FUNCTION(IsEscape)
  DECLARE_FUNCTION(SetEscape)
  DECLARE_FUNCTION(AtEnd)
  DECLARE_FUNCTION(GetString)
  DECLARE_FUNCTION(ExpectColor)
  DECLARE_FUNCTION(ExpectString)
  DECLARE_FUNCTION(ExpectLoneChar)
  DECLARE_FUNCTION(Check)
  DECLARE_FUNCTION(CheckStartsWith)
  DECLARE_FUNCTION(Expect)
  DECLARE_FUNCTION(CheckIdentifier)
  DECLARE_FUNCTION(ExpectIdentifier)
  DECLARE_FUNCTION(CheckNumber)
  DECLARE_FUNCTION(ExpectNumber)
  DECLARE_FUNCTION(CheckFloat)
  DECLARE_FUNCTION(ExpectFloat)
  DECLARE_FUNCTION(ResetQuoted)
  DECLARE_FUNCTION(ResetCrossed)
  DECLARE_FUNCTION(SkipBracketed)
  DECLARE_FUNCTION(UnGet)
  DECLARE_FUNCTION(SkipLine)
  DECLARE_FUNCTION(FileName)
  DECLARE_FUNCTION(CurrLine)
  DECLARE_FUNCTION(ScriptError)
  DECLARE_FUNCTION(ScriptMessage)
};

IMPLEMENT_CLASS(V, ScriptsParser)


static vuint32 cidterm[256/32]; // c-like identifier terminator
static vuint32 cnumterm[256/32]; // c-like number terminator
static vuint32 ncidterm[256/32]; // non-c-like identifier terminator

struct CharClassifier {
  static inline bool isCIdTerm (char ch) { return !!(cidterm[(ch>>5)&0x07]&(1U<<(ch&0x1f))); }
  static inline bool isCNumTerm (char ch) { return !!(cnumterm[(ch>>5)&0x07]&(1U<<(ch&0x1f))); }
  static inline bool isNCIdTerm (char ch) { return !!(ncidterm[(ch>>5)&0x07]&(1U<<(ch&0x1f))); }

  static inline void setCharBit (vuint32 *set, char ch) { set[(ch>>5)&0x07] |= (1U<<(ch&0x1f)); }

  CharClassifier () {
    static const char *cIdTerm = "`~!#$%^&*(){}[]/=\\?-+|;:<>,\"'"; // was with '@'
    static const char *ncIdTerm = "{}|=,;\"'";
    memset(cidterm, 0, sizeof(cidterm));
    memset(cnumterm, 0, sizeof(cnumterm));
    memset(ncidterm, 0, sizeof(ncidterm));
    for (const char *s = cIdTerm; *s; ++s) {
      setCharBit(cidterm, *s);
      setCharBit(cnumterm, *s);
    }
    for (const char *s = ncIdTerm; *s; ++s) setCharBit(ncidterm, *s);
    // blanks will terminate too
    for (int ch = 0; ch <= 32; ++ch) {
      setCharBit(cidterm, ch);
      setCharBit(cnumterm, ch);
      setCharBit(ncidterm, ch);
    }
    // c identified is terminated with dot
    setCharBit(cidterm, '.');
    // sanity check
    for (int f = 0; f < 256; ++f) {
      if (f <= 32 || f == '.' || strchr(cIdTerm, f)) {
        check(isCIdTerm(f));
        if (f == '.') { check(!isCNumTerm(f)); } else { check(isCNumTerm(f)); }
      } else {
        check(!isCIdTerm(f));
        check(!isCNumTerm(f));
      }
    }
  }
};
CharClassifier charClassifierInit;




//==========================================================================
//
//  VScriptParser::VScriptParser
//
//==========================================================================
VScriptParser::VScriptParser (const VStr &name, VStream *Strm)
  : Line(1)
  , TokLine(1)
  , End(false)
  , Crossed(false)
  , QuotedString(false)
  , ScriptName(name)
  , SrcIdx(-1)
  //, AlreadyGot(false)
  , CMode(false)
  , Escape(true)
{
  ScriptSize = Strm->TotalSize();
  ScriptBuffer = new char[ScriptSize+1];
  Strm->Serialise(ScriptBuffer, ScriptSize);
  ScriptBuffer[ScriptSize] = 0;
  delete Strm;

  ScriptPtr = ScriptBuffer;
  ScriptEndPtr = ScriptPtr+ScriptSize;

  TokStartPtr = ScriptPtr;
  TokStartLine = Line;

  // skip garbage some editors add in the begining of UTF-8 files
  if (*(const vuint8 *)ScriptPtr == 0xef && *(const vuint8 *)(ScriptPtr+1) == 0xbb && *(const vuint8 *)(ScriptPtr+2) == 0xbf) ScriptPtr += 3;
}


//==========================================================================
//
//  VScriptParser::VScriptParser
//
//==========================================================================
VScriptParser::VScriptParser (const VStr &name, const char *atext)
  : Line(1)
  , TokLine(1)
  , End(false)
  , Crossed(false)
  , QuotedString(false)
  , ScriptName(name)
  , SrcIdx(-1)
  //, AlreadyGot(false)
  , CMode(false)
  , Escape(true)
{
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

  TokStartPtr = ScriptPtr;
  TokStartLine = Line;

  // skip garbage some editors add in the begining of UTF-8 files
  if (*(const vuint8 *)ScriptPtr == 0xef && *(const vuint8 *)(ScriptPtr+1) == 0xbb && *(const vuint8 *)(ScriptPtr+2) == 0xbf) ScriptPtr += 3;
}


//==========================================================================
//
//  VScriptParser::~VScriptParser
//
//==========================================================================
VScriptParser::~VScriptParser () {
  delete[] ScriptBuffer;
  ScriptBuffer = nullptr;
}


//==========================================================================
//
//  VScriptParser::clone
//
//==========================================================================
VScriptParser *VScriptParser::clone () const {
  VScriptParser *res = new VScriptParser();

  res->ScriptBuffer = new char[ScriptSize+1];
  if (ScriptSize) memcpy(res->ScriptBuffer, ScriptBuffer, ScriptSize);
  res->ScriptBuffer[ScriptSize] = 0;

  res->ScriptPtr = res->ScriptBuffer+(ScriptPtr-ScriptBuffer);
  res->ScriptEndPtr = res->ScriptBuffer+(ScriptEndPtr-ScriptBuffer);

  res->TokStartPtr = res->ScriptBuffer+(TokStartPtr-ScriptBuffer);
  res->TokStartLine = res->TokStartLine;

  res->Line = Line;
  res->TokLine = TokLine;
  res->End = End;
  res->Crossed = Crossed;
  res->QuotedString = QuotedString;
  res->String = String;
  res->Name8 = Name8;
  res->Name = Name;
  res->Number = Number;
  res->Float = Float;

  res->ScriptName = ScriptName;
  res->ScriptSize = ScriptSize;
  res->SrcIdx = SrcIdx;
  //res->AlreadyGot = AlreadyGot;
  res->CMode = CMode;
  res->Escape = Escape;

  return res;
}


//==========================================================================
//
// VScriptParser::IsText
//
//==========================================================================
bool VScriptParser::IsText () {
  int i = 0;
  while (i < ScriptSize) {
    vuint8 ch = *(const vuint8 *)(ScriptBuffer+(i++));
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
  if (GetString()) {
    //fprintf(stderr, "<%s>\n", *String);
    UnGet();
    return false;
  }
  return true;
}


//==========================================================================
//
//  VScriptParser::IsAtEol
//
//==========================================================================
bool VScriptParser::IsAtEol () {
  int commentLevel = 0;
  for (const char *s = ScriptPtr; s < ScriptEndPtr; ++s) {
    const vuint8 ch = *(const vuint8 *)s;
    if (ch == '\r' && s[1] == '\n') return true;
    if (ch == '\n') return true;
    if (!commentLevel) {
      if (!CMode && ch == ';') return true; // this is single-line comment, it always ends with EOL
      const char c1 = s[1];
      if (ch == '/' && c1 == '/') return true; // this is single-line comment, it always ends with EOL
      if (ch == '/' && c1 == '*') {
        // multiline comment
        ++s; // skip slash
        commentLevel = -1;
        continue;
      }
      if (ch == '/' && c1 == '+') {
        // multiline comment
        ++s; // skip slash
        commentLevel = 1;
        continue;
      }
      if (ch > ' ') return false;
    } else {
      // in multiline comment
      const char c1 = s[1];
      if (commentLevel < 0) {
        if (ch == '*' && c1 == '/') {
          ++s; // skip star
          commentLevel = 0;
        }
      } else {
        if (ch == '/' && c1 == '+') {
          ++s; // skip slash
          ++commentLevel;
        } else if (ch == '+' && c1 == '/') {
          ++s; // skip plus
          --commentLevel;
        }
      }
    }
  }
  return true;
}


//==========================================================================
//
//  VScriptParser::SkipComments
//
//  this is moved out of `SkipBlanks()`, so i can use it in `SkipLine()`
//
//==========================================================================
void VScriptParser::SkipComments (bool changeFlags) {
  while (ScriptPtr < ScriptEndPtr) {
    const char c0 = *ScriptPtr;
    const char c1 = (ScriptPtr+1 < ScriptEndPtr ? ScriptPtr[1] : 0);
    // single-line comment?
    if ((!CMode && c0 == ';') || (c0 == '/' && c1 == '/')) {
      while (*ScriptPtr++ != '\n') if (ScriptPtr >= ScriptEndPtr) break;
      if (changeFlags) { ++Line; Crossed = true; }
      continue;
    }
    // multiline comment?
    if (c0 == '/' && c1 == '*') {
      ScriptPtr += 2;
      while (ScriptPtr < ScriptEndPtr) {
        if (ScriptPtr[0] == '*' && ScriptPtr[1] == '/') { ScriptPtr += 2; break; }
        // check for new-line character
        if (changeFlags && *ScriptPtr == '\n') { ++Line; Crossed = true; }
        ++ScriptPtr;
      }
      continue;
    }
    // multiline nesting comment?
    if (c0 == '/' && c1 == '+') {
      int level = 1;
      ScriptPtr += 2;
      while (ScriptPtr < ScriptEndPtr) {
        if (ScriptPtr[0] == '/' && ScriptPtr[1] == '+') { ScriptPtr += 2; ++level; continue; }
        if (ScriptPtr[0] == '+' && ScriptPtr[1] == '/') {
          ScriptPtr += 2;
          if (--level == 0) break;
          continue;
        }
        // check for new-line character
        if (changeFlags && *ScriptPtr == '\n') { ++Line; Crossed = true; }
        ++ScriptPtr;
      }
      continue;
    }
    // not a comment, stop skipping
    break;
  }
  if (ScriptPtr >= ScriptEndPtr) {
    ScriptPtr = ScriptEndPtr;
    if (changeFlags) End = true;
  }
}


//==========================================================================
//
//  VScriptParser::SkipBlanks
//
//==========================================================================
void VScriptParser::SkipBlanks (bool changeFlags) {
  while (ScriptPtr < ScriptEndPtr) {
    SkipComments(changeFlags);
    if (*(const vuint8 *)ScriptPtr <= ' ') {
      if (changeFlags && *ScriptPtr == '\n') { ++Line; Crossed = true; }
      ++ScriptPtr;
      continue;
    }
    break;
  }
  if (ScriptPtr >= ScriptEndPtr) {
    ScriptPtr = ScriptEndPtr;
    if (changeFlags) End = true;
  }
}


//==========================================================================
//
//  VScriptParser::PeekChar
//
//==========================================================================
char VScriptParser::PeekOrSkipChar (bool doSkip) {
  char res = 0;
  char *oldSPtr = ScriptPtr;
  int oldLine = Line;
  bool oldCross = Crossed;
  bool oldEnd = End;
  SkipBlanks(true); // change flags
  if (ScriptPtr < ScriptEndPtr) {
    res = *ScriptPtr;
    if (doSkip) ++ScriptPtr;
  }
  if (!doSkip) {
    ScriptPtr = oldSPtr;
    Line = oldLine;
    Crossed = oldCross;
    End = oldEnd;
  }
  return res;
}


//==========================================================================
//
//  VScriptParser::GetString
//
//==========================================================================
bool VScriptParser::GetString () {
  TokStartPtr = ScriptPtr;
  TokStartLine = Line;

  Crossed = false;
  QuotedString = false;

  SkipBlanks(true); // change flags
  // check for end of script
  if (ScriptPtr >= ScriptEndPtr) {
    TokStartPtr = ScriptEndPtr;
    TokStartLine = Line;
    End = true;
    return false;
  }

  TokLine = Line;

  String.Clean();
  if (*ScriptPtr == '\"' || *ScriptPtr == '\'') {
    // quoted string
    const char qch = *ScriptPtr++;
    QuotedString = true;
    while (ScriptPtr < ScriptEndPtr) {
      char ch = *ScriptPtr++;
      if (ch == qch) break;
      if (ch == '\r' && *ScriptPtr == '\n') {
        // convert from DOS format to UNIX format
        ch = '\n';
        ++ScriptPtr;
      } else if (Escape && ch == '\\') {
        const char c1 = *ScriptPtr;
        if (c1 == '\\' || c1 == '\"' || c1 == '\'' || c1 == '\r' || c1 == '\n') {
          ch = c1;
          ++ScriptPtr;
          if (ch == '\r') {
            ch = '\n';
            if (*ScriptPtr == '\n') ++ScriptPtr; // convert from DOS format to UNIX format
          }
        }
      }
      if (ch == '\n') {
        if (CMode) {
          if (!Escape || String.length() == 0 /*|| String[String.length()-1] != '\\'*/) {
            Error("Unterminated string constant");
          } else {
            // remove the \ character
            //String.chopRight(1);
          }
        }
        ++Line;
        Crossed = true;
      }
      String += ch;
    }
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
      while (ScriptPtr < ScriptEndPtr) {
        const char ch = *ScriptPtr++;
        if (CharClassifier::isCNumTerm(ch)) { --ScriptPtr; break; }
        String += ch;
      }
    } else if (CharClassifier::isCIdTerm(*ScriptPtr)) {
      // special single-character token
      String += *ScriptPtr++;
    } else {
      // normal string
      while (ScriptPtr < ScriptEndPtr) {
        const char ch = *ScriptPtr++;
        if (CharClassifier::isCIdTerm(ch)) { --ScriptPtr; break; }
        String += ch;
      }
    }
  } else {
    // special single-character tokens
    if (CharClassifier::isNCIdTerm(*ScriptPtr)) {
      String += *ScriptPtr++;
    } else {
      // normal string
      while (ScriptPtr < ScriptEndPtr) {
        const char ch = *ScriptPtr++;
        if (CharClassifier::isNCIdTerm(ch)) { --ScriptPtr; break; }
        // check for comments
        if (ch == '/') {
          const char c1 = *ScriptPtr;
          if (c1 == '/' || c1 == '*' || c1 == '+') {
            --ScriptPtr;
            break;
          }
        }
        String += ch;
      }
    }
  }
  return true;
}


//==========================================================================
//
//  VScriptParser::SkipLine
//
//==========================================================================
void VScriptParser::SkipLine () {
  Crossed = false;
  QuotedString = false;
  while (ScriptPtr < ScriptEndPtr) {
    SkipComments(true);
    if (Crossed || ScriptPtr >= ScriptEndPtr) break;
    if (*ScriptPtr++ == '\n') {
      ++Line;
      break;
    }
  }
  Crossed = false;

  if (ScriptPtr >= ScriptEndPtr) {
    ScriptPtr = ScriptEndPtr;
    End = true;
  }

  TokStartPtr = ScriptEndPtr;
  TokStartLine = Line;
}


//==========================================================================
//
//  VScriptParser::ExpectString
//
//==========================================================================
void VScriptParser::ExpectString () {
  if (!GetString()) Error("Missing string.");
}


//==========================================================================
//
//  VScriptParser::ExpectLoneChar
//
//==========================================================================
void VScriptParser::ExpectLoneChar () {
  UnGet();
  char ch = PeekOrSkipChar(true); // skip
  if (ch && ScriptPtr < ScriptEndPtr) {
    vuint8 nch = *(const vuint8 *)ScriptPtr;
    if (nch > ' ' && nch == '/' && ScriptEndPtr-ScriptPtr > 1) {
      if (ScriptPtr[1] != '/' && ScriptPtr[1] != '*' && ScriptPtr[1] != '+') ch = 0;
    }
  }
  if (!ch) Error("Missing char.");
  String.clear();
  String += ch;
}


//==========================================================================
//
//  VScriptParser::ExpectName8
//
//==========================================================================
void VScriptParser::ExpectName8 () {
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
    GCon->Logf(NAME_Warning, "%s: Name '%s' is too long", *GetLoc().toStringNoCol(), *String);
#endif
    Error("Name is too long");
  }
  Name8 = VName(*String, VName::AddLower8);
}


//==========================================================================
//
//  VScriptParser::ExpectName8Warn
//
//==========================================================================
void VScriptParser::ExpectName8Warn () {
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

#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  if (String.Length() > 8) {
    GCon->Logf(NAME_Warning, "%s: Name '%s' is too long", *GetLoc().toStringNoCol(), *String);
  }
#endif

  //Name = VName(*String, VName::AddLower);
  Name8 = VName(*String, VName::AddLower8);
}


//==========================================================================
//
//  VScriptParser::ExpectName8Def
//
//==========================================================================
void VScriptParser::ExpectName8Def (VName def) {
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
#else
    GLog.WriteLine("Name '%s' is too long", *String);
#endif
    Name8 = def;
  } else {
    Name8 = VName(*String, VName::AddLower8);
  }
}


//==========================================================================
//
//  VScriptParser::ExpectName
//
//==========================================================================
void VScriptParser::ExpectName () {
  ExpectString();
  Name = VName(*String, VName::AddLower);
}


//==========================================================================
//
//  ExpectName
//
//  returns parsed color, either in string form, or r,g,b triplet
//
//==========================================================================
vuint32 VScriptParser::ExpectColor () {
  if (!GetString()) Error("color expected");
  //vuint32 clr = M_LookupColourName(String);
  //if (clr) return clr;
  // hack to allow numbers like "008000"
  if (QuotedString || String.length() > 3) {
    //GCon->Logf("COLOR(0): <%s> (0x%08x)", *String, M_ParseColour(String));
    return M_ParseColour(String);
  }
  // should be r,g,b triplet
  UnGet();
  //ExpectNumber();
  if (!CheckNumber()) {
    ExpectString();
    //GCon->Logf("COLOR(1): <%s> (0x%08x)", *String, M_ParseColour(String));
    return M_ParseColour(String);
  }
  int r = clampToByte(Number);
  Check(",");
  ExpectNumber();
  int g = clampToByte(Number);
  Check(",");
  ExpectNumber();
  int b = clampToByte(Number);
  //GCon->Logf("COLOR: rgb(%d,%d,%d)", r, g, b);
  return 0xff000000u|(r<<16)|(g<<8)|b;
}


//==========================================================================
//
//  VScriptParser::Check
//
//==========================================================================
bool VScriptParser::Check (const char *str) {
  if (GetString()) {
    if (!String.ICmp(str)) return true;
    UnGet();
  }
  return false;
}


//==========================================================================
//
//  VScriptParser::CheckStartsWith
//
//==========================================================================
bool VScriptParser::CheckStartsWith (const char *str) {
  if (GetString()) {
    VStr s = VStr(str);
    if (String.length() < s.length()) { UnGet(); return false; }
    VStr s2 = String.left(s.length());
    if (s2.ICmp(s) != 0) { UnGet(); return false; }
    return true;
  }
  return false;
}


//==========================================================================
//
//  VScriptParser::Expect
//
//==========================================================================
void VScriptParser::Expect (const char *name) {
  ExpectString();
  if (String.ICmp(name)) Error(va("Bad syntax, \"%s\" expected", name));
}


//==========================================================================
//
//  VScriptParser::CheckQuotedString
//
//==========================================================================
bool VScriptParser::CheckQuotedString () {
  if (!GetString()) return false;
  if (!QuotedString) {
    UnGet();
    return false;
  }
  return true;
}


//==========================================================================
//
//  VScriptParser::CheckIdentifier
//
//==========================================================================
bool VScriptParser::CheckIdentifier () {
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
}


//==========================================================================
//
//  VScriptParser::ExpectIdentifier
//
//==========================================================================
void VScriptParser::ExpectIdentifier () {
  if (!CheckIdentifier()) Error(va("Identifier expected, got \"%s\".", *String));
}


//==========================================================================
//
//  VScriptParser::CheckNumber
//
//==========================================================================
bool VScriptParser::CheckNumber () {
  if (GetString()) {
    if (String.length() > 0) {
      /*
      char *stopper;
      Number = strtol(*String, &stopper, 0);
      if (*stopper == 0) return true;
      */
      if (String.convertInt(&Number)) {
        //GCon->Logf("VScriptParser::CheckNumber: <%s> is %d", *String, Number);
        return true;
      }
    }
    UnGet();
  }
  return false;
}


//==========================================================================
//
//  VScriptParser::ExpectNumber
//
//==========================================================================
void VScriptParser::ExpectNumber (bool allowFloat, bool truncFloat) {
  if (GetString() && String.length() > 0) {
    char *stopper;
    Number = strtol(*String, &stopper, 0);
    if (*stopper != 0) {
      if (allowFloat && *stopper == '.') {
        if (truncFloat) {
          Message(va("Bad numeric constant \"%s\" (integer expected; truncated to %d).", *String, (int)Number));
        } else {
          if (stopper[1] >= '5') ++Number;
          if (Number == 0) Number = 1; // just in case
          Message(va("Bad numeric constant \"%s\" (integer expected; rounded to %d).", *String, (int)Number));
        }
        //fprintf(stderr, "%d\n", (int)Number);
        //Error(va("Bad numeric constant \"%s\".", *String));
      } else {
        Error(va("Bad numeric constant \"%s\".", *String));
      }
    }
  } else {
    Error("Missing integer.");
  }
}


//==========================================================================
//
//  VScriptParser::CheckNumberWithSign
//
//==========================================================================
bool VScriptParser::CheckNumberWithSign () {
  if (Check("-")) {
    ExpectNumber();
    Number = -Number;
    return true;
  } else {
    return CheckNumber();
  }
}


//==========================================================================
//
//  VScriptParser::ExpectNumberWithSign
//
//==========================================================================
void VScriptParser::ExpectNumberWithSign () {
  if (Check("-")) {
    ExpectNumber();
    Number = -Number;
  } else {
    ExpectNumber();
  }
}


//==========================================================================
//
//  VScriptParser::CheckFloat
//
//==========================================================================
bool VScriptParser::CheckFloat () {
  if (GetString()) {
    if (String.length() > 0) {
      float ff = 0;
      if (String.convertFloat(&ff)) {
        Float = ff;
        return true;
      }
      /*
      char *stopper;
      Float = strtod(*String, &stopper);
      if (*stopper == 0) return true;
      */
    }
    UnGet();
  }
  return false;
}


//==========================================================================
//
//  VScriptParser::ExpectFloat
//
//==========================================================================
void VScriptParser::ExpectFloat () {
  if (GetString() && String.length() > 0) {
    //FIXME: detect when we want to use a really big number
    VStr sl = String.ToLower();
    if (sl.StartsWith("0x7f") || sl.StartsWith("0xff")) {
      Float = 99999.0f;
    } else {
      /*
      char *stopper;
      Float = strtod(*String, &stopper);
      if (*stopper != 0) Error(va("Bad floating point constant \"%s\".", *String));
      */
      float ff = 0;
      if (!String.convertFloat(&ff)) {
        // fuckin' morons from LCA loves numbers like "90000000000000000000000000000000000000000000000000"
        const char *s = *String;
        while (*s && *(const vuint8 *)s <= ' ') ++s;
        bool neg = false;
        switch (*s) {
          case '-':
            neg = true;
            /* fallthrough */
          case '+':
            ++s;
            break;
        }
        while (*s >= '0' && *s <= '9') ++s;
        while (*s && *(const vuint8 *)s <= ' ') ++s;
        if (*s) Error(va("Bad floating point constant \"%s\".", *String));
        GLog.WriteLine(NAME_Warning, "%s: DON'T BE IDIOTS, THIS IS TOO MUCH FOR A FLOAT: '%s'", *GetLoc().toStringNoCol(), *String);
        ff = 1e14;
        if (neg) ff = -ff;
      }
      Float = ff;
    }
  } else {
    Error("Missing float.");
  }
}


//==========================================================================
//
//  VScriptParser::CheckFloatWithSign
//
//==========================================================================
bool VScriptParser::CheckFloatWithSign () {
  if (Check("-")) {
    ExpectFloat();
    Float = -Float;
    return true;
  } else {
    return CheckFloat();
  }
}


//==========================================================================
//
//  VScriptParser::ExpectFloatWithSign
//
//==========================================================================
void VScriptParser::ExpectFloatWithSign () {
  if (Check("-")) {
    ExpectFloat();
    Float = -Float;
  } else {
    ExpectFloat();
  }
}


//==========================================================================
//
//  VScriptParser::ResetQuoted
//
//==========================================================================
void VScriptParser::ResetQuoted () {
  /*if (TokStartPtr != ScriptPtr)*/ QuotedString = false;
}


//==========================================================================
//
//  VScriptParser::ResetCrossed
//
//==========================================================================
void VScriptParser::ResetCrossed () {
  /*if (TokStartPtr != ScriptPtr)*/ Crossed = false;
}


//==========================================================================
//
//  VScriptParser::UnGet
//
//  Assumes there is a valid string in sc_String.
//
//==========================================================================
void VScriptParser::UnGet () {
  //AlreadyGot = true;
  ScriptPtr = TokStartPtr;
  Line = TokStartLine;
  //Crossed = false;
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
  const char *Msg = (message ? message : "Bad syntax.");
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  GCon->Logf("\"%s\" line %d: %s", *ScriptName, TokLine, Msg);
#else
  GLog.WriteLine("\"%s\" line %d: %s", *ScriptName, TokLine, Msg);
#endif
}


//==========================================================================
//
//  VScriptParser::Error
//
//==========================================================================
void VScriptParser::Error (const char *message) {
  const char *Msg = (message ? message : "Bad syntax.");
  Sys_Error("Script error, \"%s\" line %d: %s", *ScriptName, TokLine, Msg);
}


//==========================================================================
//
//  VScriptParser::GetLoc
//
//==========================================================================
TLocation VScriptParser::GetLoc () {
  if (SrcIdx == -1) SrcIdx = TLocation::AddSourceFile(ScriptName);
  return TLocation(SrcIdx, TokLine, 1);
}



// ////////////////////////////////////////////////////////////////////////// //
//  VScriptsParser
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VScriptsParser::Destroy
//
//==========================================================================
void VScriptsParser::Destroy () {
  if (Int) {
    delete Int;
    Int = nullptr;
  }
  Super::Destroy();
}


//==========================================================================
//
//  VScriptsParser::CheckInterface
//
//==========================================================================
void VScriptsParser::CheckInterface () {
  if (!Int) Sys_Error("No script currently open");
}



//==========================================================================
//
//  VScriptsParser natives
//
//==========================================================================

#if !defined(VCC_STANDALONE_EXECUTOR)
IMPLEMENT_FUNCTION(VScriptsParser, OpenLumpName) {
  P_GET_NAME(Name);
  P_GET_SELF;
#if !defined(IN_VCC)
  if (Self->Int) {
    delete Self->Int;
    Self->Int = nullptr;
  }
  Self->Int = new VScriptParser(*Name, W_CreateLumpReaderName(Name));
#endif
}

IMPLEMENT_FUNCTION(VScriptsParser, OpenLumpIndex) {
  P_GET_INT(lump);
  P_GET_SELF;
#if !defined(IN_VCC)
  if (Self->Int) {
    delete Self->Int;
    Self->Int = nullptr;
  }
  if (lump < 0) Sys_Error("cannot open non-existing lump");
  Self->Int = new VScriptParser(W_FullLumpName(lump), W_CreateLumpReaderNum(lump));
#endif
}
#endif

IMPLEMENT_FUNCTION(VScriptsParser, OpenLumpFullName) {
  P_GET_STR(Name);
  P_GET_SELF;
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  if (Self->Int) {
    delete Self->Int;
    Self->Int = nullptr;
  }
  int num = W_GetNumForFileName(Name);
  //int num = W_IterateFile(-1, *Name);
  if (num < 0) Sys_Error("file '%s' not found", *Name);
  Self->Int = new VScriptParser(*Name, W_CreateLumpReaderNum(num));
#elif defined(VCC_STANDALONE_EXECUTOR)
  if (Self->Int) {
    delete Self->Int;
    Self->Int = nullptr;
  }
  VStream *st = fsysOpenFile(*Name);
  if (!st) Sys_Error("file '%s' not found", *Name);
  bool ok = true;
  VStr s;
  if (st->TotalSize() > 0) {
    s.setLength(st->TotalSize());
    st->Serialise(s.getMutableCStr(), s.length());
    ok = !st->IsError();
  }
  delete st;
  if (!ok) Sys_Error("cannot read file '%s'", *Name);
  Self->Int = new VScriptParser(*Name, *s);
#else
  Sys_Error("file '%s' not found", *Name);
#endif
}

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

IMPLEMENT_FUNCTION(VScriptsParser, get_Quoted) {
  P_GET_SELF;
  Self->CheckInterface();
  RET_BOOL(Self->Int->QuotedString);
}

IMPLEMENT_FUNCTION(VScriptsParser, IsText) {
  P_GET_SELF;
  Self->CheckInterface();
  RET_BOOL(Self->Int->IsText());
}

IMPLEMENT_FUNCTION(VScriptsParser, IsAtEol) {
  P_GET_SELF;
  Self->CheckInterface();
  RET_BOOL(Self->Int->IsAtEol());
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

IMPLEMENT_FUNCTION(VScriptsParser, IsEscape) {
  P_GET_SELF;
  Self->CheckInterface();
  RET_BOOL(Self->Int->IsEscape());
}

IMPLEMENT_FUNCTION(VScriptsParser, SetEscape) {
  P_GET_BOOL(On);
  P_GET_SELF;
  Self->CheckInterface();
  Self->Int->SetEscape(On);
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

IMPLEMENT_FUNCTION(VScriptsParser, ExpectLoneChar) {
  P_GET_SELF;
  Self->CheckInterface();
  Self->Int->ExpectLoneChar();
}

IMPLEMENT_FUNCTION(VScriptsParser, ExpectColor) {
  P_GET_SELF;
  Self->CheckInterface();
  RET_INT(Self->Int->ExpectColor());
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

IMPLEMENT_FUNCTION(VScriptsParser, CheckStartsWith) {
  P_GET_STR(Text);
  P_GET_SELF;
  Self->CheckInterface();
  RET_BOOL(Self->Int->CheckStartsWith(*Text));
}

IMPLEMENT_FUNCTION(VScriptsParser, Expect) {
  P_GET_STR(Text);
  P_GET_SELF;
  Self->CheckInterface();
  Self->Int->Expect(*Text);
}

IMPLEMENT_FUNCTION(VScriptsParser, CheckIdentifier) {
  P_GET_SELF;
  Self->CheckInterface();
  RET_BOOL(Self->Int->CheckIdentifier());
}

IMPLEMENT_FUNCTION(VScriptsParser, ExpectIdentifier) {
  P_GET_SELF;
  Self->CheckInterface();
  Self->Int->ExpectIdentifier();
}

IMPLEMENT_FUNCTION(VScriptsParser, CheckNumber) {
  P_GET_BOOL_OPT(withSign, false);
  P_GET_SELF;
  Self->CheckInterface();
  RET_BOOL(withSign ? Self->Int->CheckNumberWithSign() : Self->Int->CheckNumber());
}

IMPLEMENT_FUNCTION(VScriptsParser, ExpectNumber) {
  P_GET_BOOL_OPT(withSign, false);
  P_GET_SELF;
  Self->CheckInterface();
  if (withSign) Self->Int->ExpectNumberWithSign(); else Self->Int->ExpectNumber();
}

IMPLEMENT_FUNCTION(VScriptsParser, CheckFloat) {
  P_GET_BOOL_OPT(withSign, false);
  P_GET_SELF;
  Self->CheckInterface();
  RET_BOOL(withSign ? Self->Int->CheckFloatWithSign() : Self->Int->CheckFloat());
}

IMPLEMENT_FUNCTION(VScriptsParser, ExpectFloat) {
  P_GET_BOOL_OPT(withSign, false);
  P_GET_SELF;
  Self->CheckInterface();
  if (withSign) Self->Int->ExpectFloatWithSign(); else Self->Int->ExpectFloat();
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

IMPLEMENT_FUNCTION(VScriptsParser, SkipLine) {
  P_GET_SELF;
  Self->CheckInterface();
  Self->Int->SkipLine();
}

IMPLEMENT_FUNCTION(VScriptsParser, UnGet) {
  P_GET_SELF;
  Self->CheckInterface();
  Self->Int->UnGet();
}

IMPLEMENT_FUNCTION(VScriptsParser, FileName) {
  P_GET_SELF;
  Self->CheckInterface();
  RET_STR(Self->Int->GetScriptName());
}

IMPLEMENT_FUNCTION(VScriptsParser, CurrLine) {
  P_GET_SELF;
  Self->CheckInterface();
  RET_INT(Self->Int->TokLine);
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
