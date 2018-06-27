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

//==========================================================================
//
//  EToken
//
//  Token types.
//
//==========================================================================
enum EToken {
  TK_NoToken,
  TK_EOF,           // reached end of file
  TK_Identifier,    // identifier, value: tk_String
  TK_NameLiteral,   // name constant, value: tk_Name
  TK_StringLiteral, // string, value: tk_String
  TK_IntLiteral,    // integer number, value: tk_Number
  TK_FloatLiteral,  // floating number, value: tk_Float

  // keywords
  TK_Abstract,
  TK_Alias,
  TK_Array,
  TK_Auto,
  TK_BitEnum,
  TK_Bool,
  TK_Break,
  TK_Byte,
  TK_Case,
  TK_Class,
  TK_Const,
  TK_Continue,
  TK_Decorate,
  TK_Default,
  TK_DefaultProperties,
  TK_Delegate,
  TK_Delete,
  TK_Do,
  TK_Else,
  TK_Enum,
  TK_False,
  TK_Final,
  TK_Float,
  TK_For,
  TK_Foreach,
  TK_Game,
  TK_Get,
  TK_Goto,
  TK_If,
  TK_Inline,
  TK_Import,
  TK_Int,
  TK_IsA,
  TK_Iterator,
  TK_Name,
  TK_Native,
  TK_None,
  TK_Null,
  TK_Optional,
  TK_Out,
  TK_Override,
  TK_Private,
  TK_Protected,
  TK_ReadOnly,
  TK_Ref,
  TK_Reliable,
  TK_Replication,
  TK_Return,
  TK_Scope,
  TK_Self,
  TK_Set,
  TK_Spawner,
  TK_State,
  TK_States,
  TK_Static,
  TK_String,
  TK_Struct,
  TK_Switch,
  TK_Transient,
  TK_True,
  TK_Unreliable,
  TK_Vector,
  TK_Void,
  TK_While,

  TK_MobjInfo,
  TK_ScriptId,

  // punctuation
  TK_VarArgs,
  TK_LShiftAssign,
  TK_RShiftAssign,
  TK_DotDot,
  TK_AddAssign,
  TK_MinusAssign,
  TK_MultiplyAssign,
  TK_DivideAssign,
  TK_ModAssign,
  TK_AndAssign,
  TK_OrAssign,
  TK_XOrAssign,
  TK_Equals,
  TK_NotEquals,
  TK_LessEquals,
  TK_GreaterEquals,
  TK_AndLog,
  TK_OrLog,
  TK_LShift,
  TK_RShift,
  TK_Inc,
  TK_Dec,
  TK_Arrow,
  TK_DColon,
  TK_Less,
  TK_Greater,
  TK_Quest,
  TK_And,
  TK_Or,
  TK_XOr,
  TK_Tilde,
  TK_Not,
  TK_Plus,
  TK_Minus,
  TK_Asterisk,
  TK_Slash,
  TK_Percent,
  TK_LParen,
  TK_RParen,
  TK_Dot,
  TK_Comma,
  TK_Semicolon,
  TK_Colon,
  TK_Assign,
  TK_LBracket,
  TK_RBracket,
  TK_LBrace,
  TK_RBrace,
  TK_Dollar,

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
    int SourceIdx;
    int Line;
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
  void PushSource (TLocation &Loc, const VStr &FileName);
  void PushSource (TLocation &Loc, VStream *Strm, const VStr &FileName); // takes ownership
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
  TLocation Location;
  vint32 Number;
  float Float;
  char *String;
  VName Name;
  bool NewLine;

  static const char *TokenNames[];

  static inline bool isAlpha (char ch) { return ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')); }
  static inline bool isDigit (char ch) { return (ch >= '0' && ch <= '9'); }

public:
  VLexer ();
  ~VLexer ();

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
};
