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

//==========================================================================
//
//  EToken
//
//  Token types.
//
//==========================================================================
enum EToken {
#define VC_LEXER_DEFTOKEN(name,str)  TK_ ## name,
#include "vc_lexer_tokens.h"
#undef VC_LEXER_DEFTOKEN
  TK_TotalTokenCount,
};


//==========================================================================
//
//  VLexer
//
//  Lexer class.
//
//==========================================================================
class VLexer {
private:
  enum { MAX_QUOTED_LENGTH = 256 };
  enum { MAX_IDENTIFIER_LENGTH = 64 };
  enum { EOF_CHARACTER = 127 };
  enum { NON_HEX_DIGIT = 255 };

  enum {
    CHR_EOF,
    CHR_Letter,
    CHR_Number,
    CHR_Quote,
    CHR_SingleQuote,
    CHR_Special
  };

  enum {
    IF_False,     // skipping the content
    IF_True,      // parsing the content
    IF_ElseFalse, // else case, skipping content
    IF_ElseTrue,  // else case, parsing content
    IF_Skip,      // conditon inside curently skipped code
    IF_ElseSkip,  // else case inside curently skipped code
  };

  struct VSourceFile {
    VSourceFile *Next; // nesting stack
    VStr FileName;
    VStr Path;
    char *FileStart;
    char *FilePtr;
    char *FileEnd;
    char currCh;
    TLocation Loc;
    TLocation CurLoc;
    int SourceIdx;
    bool IncLineNumber;
    bool NewLine;
    TArray<int> IfStates;
    bool Skipping;
  };

  //k8: initialization is not thread-safe, but i don't care for now
  static char ASCIIToChrCode[256];
  static vuint8 ASCIIToHexDigit[256];
  static bool tablesInited;

  char tokenStringBuffer[MAX_QUOTED_LENGTH];
  bool sourceOpen;
  char currCh;
  TArray<VStr> defines;
  TArray<VStr> includePath;
  VSourceFile *src;

  inline bool checkStrTk (const char *tokname) const { return (strcmp(tokenStringBuffer, tokname) == 0); }

  void NextChr ();
  char Peek (int dist=0) const;
  void SkipWhitespaceAndComments ();
  void ProcessPreprocessor ();
  void ProcessDefine ();
  void ProcessIf (bool OnTrue);
  void ProcessElse ();
  void ProcessEndIf ();
  void ProcessInclude ();
  void PushSource (const VStr &FileName);
  void PushSource (VStream *Strm, const VStr &FileName); // takes ownership
  void PopSource ();
  void ProcessNumberToken ();
  void ProcessChar ();
  void ProcessQuoteToken ();
  void ProcessSingleQuoteToken ();
  void ProcessLetterToken (bool);
  void ProcessSpecialToken ();
  void ProcessFileName ();

private:
  // lookahead support
  bool isNStrEqu (int spos, int epos, const char *s) const;
  bool posAtEOS (int cpos) const;
  vuint8 peekChar (int cpos) const; // returns 0 on EOS
  bool skipBlanksFrom (int &cpos) const; // returns `false` on EOS
  EToken skipTokenFrom (int &cpos, VStr *str) const; // calls skipBlanksFrom, returns token type or TK_NoToken

public:
  EToken Token;
  TLocation Location; // of the current token
  TLocation CurLocation; // of the current token
  vint32 Number;
  float Float;
  char *String;
  VName Name;
  bool NewLine;

  static const char *TokenNames[];

  static inline bool isAlpha (char ch) { return ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')); }
  static inline bool isDigit (char ch) { return (ch >= '0' && ch <= '9'); }

public:
  // virtual file system callback (can be empty, and is empty by default)
  void *userdata; // arbitrary pointer, not used by the lexer
  // should return `null` if file not found
  // should NOT fail if file not found
  VStream *(*dgOpenFile) (VLexer *self, const VStr &filename);

public:
  VLexer ();
  ~VLexer ();

  // returns `null` if file not found
  // by default it tries to call `dgOpenFile()`, if it is specified,
  // otherwise falls back to standard vfs
  // should NOT fail if file not found
  virtual VStream *doOpenFile (const VStr &filename);

  void AddDefine (const VStr &CondNam, bool showWarning=true);
  void AddIncludePath (const VStr &DirName);
  void OpenSource (const VStr &FileName);
  void OpenSource (VStream *astream, const VStr &FileName); // takes ownership

  char peekNextNonBlankChar () const;

  // this is freakin' slow, and won't cross "include" boundaries
  // offset==0 means "current token"
  // this doesn't process conditional directives,
  // so it is useful only for limited lookups
  EToken peekTokenType (int offset=1, VStr *tkstr=nullptr) const;

  void NextToken ();
  bool Check (EToken tk);
  void Expect (EToken tk);
  void Expect (EToken tk, ECompileError error);

  // check for identifier (it cannot be a keyword)
  bool Check (const char *id, bool caseSensitive=true);
  // expect identifier (it cannot be a keyword)
  void Expect (const char *id, bool caseSensitive=true);
};
