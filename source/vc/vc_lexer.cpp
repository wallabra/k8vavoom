////**************************************************************************
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

#include "vc_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

const char *VLexer::TokenNames[] = {
  "",
  "END OF FILE",
  "IDENTIFIER",
  "NAME LITERAL",
  "STRING LITERAL",
  "INTEGER LITERAL",
  "FLOAT LITERAL",
  // keywords
  "abstract",
  "array",
  "auto",
  "bool",
  "break",
  "byte",
  "case",
  "class",
  "const",
  "continue",
  "decorate",
  "default",
  "defaultproperties",
  "delegate",
  "do",
  "else",
  "enum",
  "false",
  "final",
  "float",
  "for",
  "foreach",
  "game",
  "get",
  "if",
  "import",
  "int",
  "iterator",
  "name",
  "native",
  "none",
  "nullptr",
  "optional",
  "out",
  "private",
  "readonly",
  "reliable",
  "replication",
  "return",
  "self",
  "set",
  "spawner",
  "state",
  "states",
  "static",
  "string",
  "struct",
  "switch",
  "transient",
  "true",
  "unreliable",
  "vector",
  "void",
  "while",
  "__mobjinfo__",
  "__scriptid__",
  // punctuation
  "...",
  "<<=",
  ">>=",
  "+=",
  "-=",
  "*=",
  "/=",
  "%=",
  "&=",
  "|=",
  "^=",
  "==",
  "!=",
  "<=",
  ">=",
  "&&",
  "||",
  "<<",
  ">>",
  "++",
  "--",
  "->",
  "::",
  "<",
  ">",
  "?",
  "&",
  "|",
  "^",
  "~",
  "!",
  "+",
  "-",
  "*",
  "/",
  "%",
  "(",
  ")",
  ".",
  ",",
  ";",
  ":",
  "=",
  "[",
  "]",
  "{",
  "}",
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
//  VLexer::VLexer
//
//==========================================================================
VLexer::VLexer ()
  : SourceOpen(false)
  , Src(nullptr)
  , Token(TK_NoToken)
  , Number(0)
  , Float(0)
  , Name(NAME_None)
{
  memset(TokenStringBuffer, 0, sizeof(TokenStringBuffer));
  for (int i = 0; i < 256; ++i) {
    ASCIIToChrCode[i] = CHR_Special;
    ASCIIToHexDigit[i] = NON_HEX_DIGIT;
  }
  for (int i = '0'; i <= '9'; ++i) {
    ASCIIToChrCode[i] = CHR_Number;
    ASCIIToHexDigit[i] = i-'0';
  }
  for (int i = 'A'; i <= 'F'; ++i) ASCIIToHexDigit[i] = 10+(i-'A');
  for (int i = 'a'; i <= 'f'; ++i) ASCIIToHexDigit[i] = 10+(i-'a');
  for (int i = 'A'; i <= 'Z'; ++i) ASCIIToChrCode[i] = CHR_Letter;
  for (int i = 'a'; i <= 'z'; ++i) ASCIIToChrCode[i] = CHR_Letter;
  ASCIIToChrCode[(int)'\"'] = CHR_Quote;
  ASCIIToChrCode[(int)'\''] = CHR_SingleQuote;
  ASCIIToChrCode[(int)'_'] = CHR_Letter;
  ASCIIToChrCode[0] = CHR_EOF;
  ASCIIToChrCode[EOF_CHARACTER] = CHR_EOF;
  String = TokenStringBuffer;
}


//==========================================================================
//
//  VLexer::~VLexer
//
//==========================================================================
VLexer::~VLexer () {
  while (Src) PopSource();
  SourceOpen = false;
}


//==========================================================================
//
//  VLexer::OpenSource
//
//==========================================================================
void VLexer::OpenSource (const VStr& FileName) {
  // read file and prepare for compilation
  PushSource(Location, FileName);
  SourceOpen = true;
  Token = TK_NoToken;
}


//==========================================================================
//
//  VLexer::OpenSource
//
//==========================================================================
void VLexer::OpenSource (VStream *astream, const VStr& FileName) {
  // read file and prepare for compilation
  PushSource(Location, astream, FileName);
  SourceOpen = true;
  Token = TK_NoToken;
}


//==========================================================================
//
//  VLexer::PushSource
//
//==========================================================================
void VLexer::PushSource (TLocation& Loc, const VStr& FileName) {
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  VStream *Strm = FL_OpenFileRead(FileName);
#else
  VStream *Strm = OpenFile(FileName);
#endif
  PushSource(Loc, Strm, FileName);
}


//==========================================================================
//
//  VLexer::PushSource
//
//==========================================================================
void VLexer::PushSource (TLocation& Loc, VStream *Strm, const VStr& FileName) {
  if (!Strm) {
    FatalError("Couldn't open %s", *FileName);
    return;
  }

  VSourceFile *NewSrc = new VSourceFile();
  NewSrc->Next = Src;
  Src = NewSrc;

  // copy file name
  NewSrc->FileName = FileName;

  // extract path to the file.
  const char *PathEnd = *FileName+FileName.Length()-1;
  while (PathEnd >= *FileName && *PathEnd != '/' && *PathEnd != '\\') --PathEnd;
  if (PathEnd >= *FileName) NewSrc->Path = VStr(FileName, 0, (PathEnd-(*FileName))+1);

  // read the file
  int FileSize = Strm->TotalSize();
  NewSrc->FileStart = new char[FileSize+1];
  Strm->Serialise(NewSrc->FileStart, FileSize);
  Strm->Close();
  delete Strm;
  //Strm = nullptr;

  NewSrc->FileStart[FileSize] = 0;
  NewSrc->FileEnd = NewSrc->FileStart+FileSize;
  NewSrc->FilePtr = NewSrc->FileStart;

  // skip garbage some editors add in the begining of UTF-8 files.
  if ((vuint8)NewSrc->FilePtr[0] == 0xef &&
      (vuint8)NewSrc->FilePtr[1] == 0xbb &&
      (vuint8)NewSrc->FilePtr[2] == 0xbf)
  {
    NewSrc->FilePtr += 3;
  }

  // Save current character and location to be able to restore them.
  NewSrc->Chr = Chr;
  NewSrc->Loc = Location;

  NewSrc->SourceIdx = TLocation::AddSourceFile(FileName);
  NewSrc->Line = 1;
  NewSrc->IncLineNumber = false;
  NewSrc->NewLine = true;
  NewSrc->Skipping = false;
  Location = TLocation(NewSrc->SourceIdx, NewSrc->Line);
  NextChr();
}


//==========================================================================
//
//  VLexer::PopSource
//
//==========================================================================
void VLexer::PopSource () {
  if (!Src) return;

  if (Src->IfStates.Num()) ParseError(Location, "#ifdef without a corresponding #endif");

  VSourceFile *Tmp = Src;
  delete[] Tmp->FileStart;
  Tmp->FileStart = nullptr;
  Src = Tmp->Next;
  Chr = Tmp->Chr;
  Location = Tmp->Loc;
  delete Tmp;
  //Tmp = nullptr;
}


//==========================================================================
//
//  VLexer::NextToken
//
//==========================================================================
void VLexer::NextToken () {
  if (!Src) { NewLine = false; Token = TK_EOF; return; }
  NewLine = Src->NewLine;
  do {
    TokenStringBuffer[0] = 0;
    SkipWhitespaceAndComments();

    if (Src->NewLine) {
      NewLine = true;
      // a new line has been started, check preprocessor directive
      Src->NewLine = false;
      if (Chr == '#') {
        ProcessPreprocessor();
        continue;
      }
    }

    switch (ASCIIToChrCode[(vuint8)Chr]) {
      case CHR_EOF: PopSource(); Token = (Src ? TK_NoToken : TK_EOF); break;
      case CHR_Letter: ProcessLetterToken(true); break;
      case CHR_Number: ProcessNumberToken(); break;
      case CHR_Quote: ProcessQuoteToken(); break;
      case CHR_SingleQuote: ProcessSingleQuoteToken(); break;
      default: ProcessSpecialToken(); break;
    }

    if (Token != TK_EOF && Src->Skipping) Token = TK_NoToken;
  } while (Token == TK_NoToken);
}


//==========================================================================
//
//  VLexer::NextChr
//
//==========================================================================
void VLexer::NextChr () {
  if (Src->FilePtr >= Src->FileEnd) {
    Chr = EOF_CHARACTER;
    return;
  }
  if (Src->IncLineNumber) {
    ++Src->Line;
    Location = TLocation(Src->SourceIdx, Src->Line);
    Src->IncLineNumber = false;
  }
  Chr = *Src->FilePtr++;
  if ((vuint8)Chr < ' ') {
    if (Chr == '\n') {
      Src->IncLineNumber = true;
      Src->NewLine = true;
    }
    Chr = ' ';
  }
}


//==========================================================================
//
//  VLexer::SkipWhitespaceAndComments
//
//==========================================================================
void VLexer::SkipWhitespaceAndComments () {
  bool Done;
  do {
    Done = true;
    while (Chr == ' ') NextChr();
    if (Chr == '/' && *Src->FilePtr == '*') {
      // block comment
      NextChr();
      do {
        NextChr();
        if (Chr == EOF_CHARACTER) {
          ParseError(Location, "End of file inside a comment");
          return;
        }
      } while (Chr != '*' || *Src->FilePtr != '/');
      NextChr();
      NextChr();
      Done = false;
    } else if (Chr == '/' && *Src->FilePtr == '+') {
      // nested block comment
      NextChr();
      int level = 1;
      for (;;) {
        NextChr();
        if (Chr == EOF_CHARACTER) {
          ParseError(Location, "End of file inside a comment");
          return;
        }
        if (Chr == '+' && *Src->FilePtr == '/') {
          NextChr();
          NextChr();
          if (--level == 0) break;
        } else if (Chr == '/' && *Src->FilePtr == '+') {
          NextChr();
          NextChr();
          ++level;
        } else {
          NextChr();
        }
      }
      Done = false;
    } else if (Chr == '/' && *Src->FilePtr == '/') {
      // c++ style comment
      NextChr();
      do {
        NextChr();
        if (Chr == EOF_CHARACTER) {
          ParseError(Location, "End of file inside a comment");
          return;
        }
      } while (!Src->IncLineNumber);
      Done = false;
    }
  } while (!Done);
}


//==========================================================================
//
//  VLexer::peekNextNonBlankChar
//
//==========================================================================
char VLexer::peekNextNonBlankChar () const {
  const char *fpos = Src->FilePtr;
  if (fpos > Src->FileStart) --fpos; // unget last char
  while (fpos < Src->FileEnd) {
    char ch = *fpos++;
    if ((vuint8)ch <= ' ') continue;
    // comment?
    if (ch == '/' && fpos < Src->FileEnd) {
      ch = *fpos++;
      // single-line?
      if (ch == '/') {
        while (fpos < Src->FileEnd && *fpos != '\n') ++fpos;
        continue;
      }
      // multiline?
      if (ch == '*') {
        while (fpos < Src->FileEnd) {
          if (*fpos == '*' && Src->FileEnd-fpos > 1 && fpos[1] == '/') { fpos += 2; break; }
          ++fpos;
        }
        continue;
      }
      // multiline nested?
      if (ch == '+') {
        int level = 1;
        while (fpos < Src->FileEnd) {
          if (*fpos == '+' && Src->FileEnd-fpos > 1 && fpos[1] == '/') {
            fpos += 2;
            if (--level == 0) break;
          } else if (*fpos == '/' && Src->FileEnd-fpos > 1 && fpos[1] == '+') {
            ++level;
          } else {
            ++fpos;
          }
        }
        continue;
      }
      return '/';
    }
    return ch;
  }
  return EOF_CHARACTER;
}


//==========================================================================
//
//  VLexer::ProcessPreprocessor
//
//==========================================================================
void VLexer::ProcessPreprocessor () {
  NextChr();

  if (Src->NewLine || Chr == EOF_CHARACTER) {
    ParseError(Location, "Bad directive.");
    return;
  }

  if (ASCIIToChrCode[(vuint8)Chr] != CHR_Letter) {
    ParseError(Location, "Bad directive.");
    while (!Src->NewLine && Chr != EOF_CHARACTER) NextChr();
    return;
  }

  ProcessLetterToken(false);

  if (!VStr::Cmp(TokenStringBuffer, "line")) {
    // read line number
    SkipWhitespaceAndComments();
    if (ASCIIToChrCode[(vuint8)Chr] != CHR_Number) ParseError(Location, "Bad directive.");
    ProcessNumberToken();
    /*if (!Src->Skipping)*/ Src->Line = Number-1;

    // read file name
    SkipWhitespaceAndComments();
    if (ASCIIToChrCode[(vuint8)Chr] != CHR_Quote) ParseError(Location, "Bad directive.");
    ProcessFileName();
    /*if (!Src->Skipping)*/ {
      Src->SourceIdx = TLocation::AddSourceFile(String);
      Location = TLocation(Src->SourceIdx, Src->Line);
    }

    // ignore flags
    while (!Src->NewLine) NextChr();
  } else if (!VStr::Cmp(TokenStringBuffer, "define")) {
    ProcessDefine();
  } else if (!VStr::Cmp(TokenStringBuffer, "ifdef")) {
    ProcessIf(true);
  } else if (!VStr::Cmp(TokenStringBuffer, "ifndef")) {
    ProcessIf(false);
  } else if (!VStr::Cmp(TokenStringBuffer, "else")) {
    ProcessElse();
  } else if (!VStr::Cmp(TokenStringBuffer, "endif")) {
    ProcessEndIf();
  } else if (!VStr::Cmp(TokenStringBuffer, "include")) {
    ProcessInclude();
    return;
  } else {
    ParseError(Location, "Bad directive.");
    while (!Src->NewLine && Chr != EOF_CHARACTER) NextChr();
  }
  Token = TK_NoToken;

  SkipWhitespaceAndComments();
  // a new-line is expected at the end of preprocessor directive.
  if (!Src->NewLine) ParseError(Location, "Bad directive.");
}


//==========================================================================
//
//  VLexer::ProcessDefine
//
//==========================================================================
void VLexer::ProcessDefine () {
  SkipWhitespaceAndComments();

  // argument to the #define must be on the same line.
  if (Src->NewLine || Chr == EOF_CHARACTER) {
    ParseError(Location, "Bad directive.");
    return;
  }

  // parse name to be defined
  if (ASCIIToChrCode[(vuint8)Chr] != CHR_Letter) {
    ParseError(Location, "Bad directive.");
    while (!Src->NewLine && Chr != EOF_CHARACTER) NextChr();
    return;
  }
  ProcessLetterToken(false);

  if (Src->Skipping) return;

  AddDefine(TokenStringBuffer);
}


//==========================================================================
//
//  VLexer::AddDefine
//
//==========================================================================
void VLexer::AddDefine (const VStr& CondName) {
  // check for redefined names
  bool Found = false;
  for (int i = 0; i < Defines.Num(); ++i) {
    if (Defines[i] == CondName) {
      ParseWarning(Location, "Redefined conditional");
      Found = true;
      break;
    }
  }
  if (!Found) Defines.Append(CondName); // add it
}


//==========================================================================
//
//  VLexer::ProcessIf
//
//==========================================================================
void VLexer::ProcessIf (bool OnTrue) {
  SkipWhitespaceAndComments();

  // argument to the #ifdef must be on the same line
  if (Src->NewLine || Chr == EOF_CHARACTER) {
    ParseError(Location, "Bad directive.");
    return;
  }

  // parse condition name
  if (ASCIIToChrCode[(vuint8)Chr] != CHR_Letter) {
    ParseError(Location, "Bad directive.");
    while (!Src->NewLine && Chr != EOF_CHARACTER) NextChr();
    return;
  }

  ProcessLetterToken(false);

  if (Src->Skipping) {
    Src->IfStates.Append(IF_Skip);
  } else {
    // check if the names has been defined
    bool Found = false;
    for (int i = 0; i < Defines.Num(); ++i) {
      if (Defines[i] == TokenStringBuffer) {
        Found = true;
        break;
      }
    }
    if (Found == OnTrue) {
      Src->IfStates.Append(IF_True);
    } else {
      Src->IfStates.Append(IF_False);
      Src->Skipping = true;
    }
  }
}


//==========================================================================
//
//  VLexer::ProcessElse
//
//==========================================================================
void VLexer::ProcessElse () {
  if (!Src->IfStates.Num()) {
    ParseError(Location, "#else without an #ifdef/#ifndef");
    return;
  }
  switch (Src->IfStates[Src->IfStates.Num()-1]) {
    case IF_True:
      Src->IfStates[Src->IfStates.Num()-1] = IF_ElseFalse;
      Src->Skipping = true;
      break;
    case IF_False:
      Src->IfStates[Src->IfStates.Num()-1] = IF_ElseTrue;
      Src->Skipping = false;
      break;
    case IF_Skip:
      Src->IfStates[Src->IfStates.Num()-1] = IF_ElseSkip;
      break;
    case IF_ElseTrue:
    case IF_ElseFalse:
    case IF_ElseSkip:
      ParseError(Location, "Multiple #else directives for a single #ifdef");
      Src->Skipping = true;
      break;
  }
}


//==========================================================================
//
//  VLexer::ProcessEndIf
//
//==========================================================================
void VLexer::ProcessEndIf () {
  if (!Src->IfStates.Num()) {
    ParseError(Location, "#endif without an #ifdef/#ifndef");
    return;
  }
  Src->IfStates.RemoveIndex(Src->IfStates.Num()-1);
  if (Src->IfStates.Num() > 0) {
    switch (Src->IfStates[Src->IfStates.Num()-1]) {
      case IF_True:
      case IF_ElseTrue:
        Src->Skipping = false;
        break;
      case IF_False:
      case IF_ElseFalse:
        Src->Skipping = true;
        break;
      case IF_Skip:
      case IF_ElseSkip:
        break;
    }
  } else {
    Src->Skipping = false;
  }
}


//==========================================================================
//
//  VLexer::ProcessInclude
//
//==========================================================================
void VLexer::ProcessInclude () {
  SkipWhitespaceAndComments();

  // file name must be on the same line
  if (Src->NewLine || Chr == EOF_CHARACTER) {
    ParseError(Location, "Bad directive.");
    return;
  }

  // parse file name
  if (Chr != '\"') {
    ParseError(Location, "Bad directive.");
    while (!Src->NewLine && Chr != EOF_CHARACTER) NextChr();
    return;
  }
  ProcessFileName();
  TLocation Loc = Location;

  Token = TK_NoToken;
  SkipWhitespaceAndComments();
  // a new-line is expected at the end of preprocessor directive.
  if (!Src->NewLine) ParseError(Location, "Bad directive.");

  if (Src->Skipping) return;

  // check if it's an absolute path location.
  if (TokenStringBuffer[0] != '/' && TokenStringBuffer[0] != '\\') {
    // first try relative to the current source file
    if (Src->Path.IsNotEmpty()) {
      VStr FileName = Src->Path+VStr(TokenStringBuffer);
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
      VStream *Strm = FL_OpenFileRead(FileName);
#else
      VStream *Strm = OpenFile(FileName);
#endif
      if (Strm) {
        PushSource(Loc, Strm, FileName);
        return;
      }
    }

    for (int i = IncludePath.Num() - 1; i >= 0; --i) {
      VStr FileName = IncludePath[i] + VStr(TokenStringBuffer);
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
      VStream *Strm = FL_OpenFileRead(FileName);
#else
      VStream *Strm = OpenFile(FileName);
#endif
      if (Strm) {
        PushSource(Loc, Strm, FileName);
        return;
      }
    }
  }

  // either it's relative to the current directory or absolute path
  PushSource(Loc, TokenStringBuffer);
}


//==========================================================================
//
//  VLexer::AddIncludePath
//
//==========================================================================
void VLexer::AddIncludePath (const VStr& DirName) {
  VStr Copy = DirName;
  // append trailing slash if needed
  if (!Copy.EndsWith("/") && !Copy.EndsWith("\\")) Copy += '/';
  IncludePath.Append(Copy);
}


//==========================================================================
//
//  VLexer::ProcessNumberToken
//
//==========================================================================
void VLexer::ProcessNumberToken () {
  Token = TK_IntLiteral;

  char c = Chr;
  NextChr();
  Number = c-'0';

  // hex/octal/decimal/binary constant?
  if (c == '0') {
    int base = 0;
    switch (Chr) {
      case 'x': case 'X': base = 16; break;
      case 'd': case 'D': base = 10; break;
      case 'o': case 'O': base = 8; break;
      case 'b': case 'B': base = 2; break;
    }
    if (base != 0) {
      NextChr();
      for (;;) {
        if (Chr != '_') {
          if (base == 16) {
            if (ASCIIToHexDigit[(vuint8)Chr] == NON_HEX_DIGIT) break;
            Number = (Number<<4)+ASCIIToHexDigit[(vuint8)Chr];
          } else {
            if (Chr < '0' || Chr >= '0'+base) break;
            Number = Number*base+(Chr-'0');
          }
        }
        NextChr();
      }
      return;
    }
  }

  for (;;) {
    if (Chr != '_') {
      if (ASCIIToChrCode[(vuint8)Chr] != CHR_Number) break;
      Number = 10*Number+(Chr-'0');
    } else {
      //ParseError(Location, "RADIX!");
    }
    NextChr();
  }

  if (Chr == '.') {
    Token = TK_FloatLiteral;
    NextChr(); // skip dot
    Float = Number;
    float fmul = 0.1;
    for (;;) {
      if (Chr != '_') {
        if (ASCIIToChrCode[(vuint8)Chr] != CHR_Number) break;
        Float += (Chr-'0')*fmul;
        fmul /= 10.0;
      } else {
        //ParseError(Location, "RADIX!");
      }
      NextChr();
    }
    return;
  }

  /*
  if (Chr == '_') {
    ParseError(Location, "RADIX!");

    int radix;
    int digit;

    NextChr(); // Underscore
    radix = Number;
    if (radix < 2 || radix > 36) {
      ParseError(Location, ERR_BAD_RADIX_CONSTANT);
      radix = 2;
    }
    Number = 0;
    do {
      digit = VStr::ToUpper(Chr);
           if (digit < '0' || (digit > '9' && digit < 'A') || digit > 'Z') digit = -1;
      else if(digit > '9') digit = 10 + digit - 'A';
      else digit -= '0';
      if (digit >= radix) digit = -1;
      if (digit != -1) {
        Number = radix * Number + digit;
        NextChr();
      }
    } while (digit != -1);
  }
  */
}


//==========================================================================
//
//  VLexer::ProcessChar
//
//==========================================================================
void VLexer::ProcessChar () {
  if (Chr == EOF_CHARACTER) {
    ParseError(Location, ERR_EOF_IN_STRING);
    BailOut();
  }
  if (Src->IncLineNumber) ParseError(Location, ERR_NEW_LINE_INSIDE_QUOTE);
  if (Chr == '\\') {
    // special symbol
    NextChr();
    if (Chr == EOF_CHARACTER) { ParseError(Location, ERR_EOF_IN_STRING); BailOut(); }
    if (Src->IncLineNumber) ParseError(Location, ERR_NEW_LINE_INSIDE_QUOTE);
    switch (Chr) {
      case 'n': Chr = '\n'; break;
      case 'r': Chr = '\r'; break;
      case 't': Chr = '\t'; break;
      case '\'': Chr = '\''; break;
      case '"': Chr = '"'; break;
      case '\\': Chr = '\\'; break;
      case 'c': Chr = TEXT_COLOUR_ESCAPE; break;
      case 'e': Chr = '\x1b'; break;
      case 'x': case 'X':
        {
          NextChr();
          if (Chr == EOF_CHARACTER) { ParseError(Location, ERR_EOF_IN_STRING); BailOut(); }
          if (ASCIIToHexDigit[(vuint8)Chr] == NON_HEX_DIGIT) { ParseError(Location, ERR_EOF_IN_STRING); BailOut(); }
          int n = ASCIIToHexDigit[(vuint8)Chr];
          // second digit
          if (Src->FilePtr < Src->FileEnd && ASCIIToHexDigit[(vuint8)(*Src->FilePtr)] != NON_HEX_DIGIT) {
            NextChr();
            n = n*16+ASCIIToHexDigit[(vuint8)Chr];
          }
          Chr = (char)n;
        }
        break;
      default: ParseError(Location, ERR_UNKNOWN_ESC_CHAR);
    }
  }
}


//==========================================================================
//
//  VLexer::ProcessQuoteToken
//
//==========================================================================
void VLexer::ProcessQuoteToken () {
  Token = TK_StringLiteral;
  int len = 0;
  NextChr();
  while (Chr != '\"') {
    if (len >= MAX_QUOTED_LENGTH-1) {
      ParseError(Location, ERR_STRING_TOO_LONG);
      NextChr();
      continue;
    }
    ProcessChar();
    TokenStringBuffer[len] = Chr;
    NextChr();
    ++len;
  }
  TokenStringBuffer[len] = 0;
  NextChr();
}


//==========================================================================
//
//  VLexer::ProcessSingleQuoteToken
//
//==========================================================================
void VLexer::ProcessSingleQuoteToken () {
  Token = TK_NameLiteral;
  int len = 0;
  NextChr();
  while (Chr != '\'') {
    if (len >= MAX_IDENTIFIER_LENGTH-1) {
      ParseError(Location, ERR_STRING_TOO_LONG);
      NextChr();
      continue;
    }
    ProcessChar();
    TokenStringBuffer[len] = Chr;
    NextChr();
    ++len;
  }
  TokenStringBuffer[len] = 0;
  NextChr();
  Name = TokenStringBuffer;
}


//==========================================================================
//
//  VLexer::ProcessLetterToken
//
//==========================================================================
void VLexer::ProcessLetterToken (bool CheckKeywords) {
  Token = TK_Identifier;
  int len = 0;
  while (ASCIIToChrCode[(vuint8)Chr] == CHR_Letter || ASCIIToChrCode[(vuint8)Chr] == CHR_Number) {
    if (len == MAX_IDENTIFIER_LENGTH-1) {
      ParseError(Location, ERR_IDENTIFIER_TOO_LONG);
      NextChr();
      continue;
    }
    TokenStringBuffer[len] = Chr;
    ++len;
    NextChr();
  }
  TokenStringBuffer[len] = 0;

  if (!CheckKeywords) return;

  const char *s = TokenStringBuffer;
  switch (s[0]) {
    case '_':
      if (s[1] == '_') {
        if (s[2] == 'm' && s[3] == 'o' && s[4] == 'b' && s[5] == 'j' &&
            s[6] == 'i' && s[7] == 'n' && s[8] == 'f' && s[9] == 'o' &&
            s[10] == '_' && s[11] == '_' && s[12] == 0)
        {
          Token = TK_MobjInfo;
        } else if (s[2] == 's' && s[3] == 'c' && s[4] == 'r' &&
                   s[5] == 'i' && s[6] == 'p' && s[7] == 't' && s[8] == 'i' &&
                   s[9] == 'd' && s[10] == '_' && s[11] == '_' && s[12] == 0)
        {
          Token = TK_ScriptId;
        }
      }
      break;
    case 'a':
      if (s[1] == 'b' && s[2] == 's' && s[3] == 't' && s[4] == 'r' &&
          s[5] == 'a' && s[6] == 'c' && s[7] == 't' && s[8] == 0)
      {
        Token = TK_Abstract;
      } else if (s[1] == 'r' && s[2] == 'r' && s[3] == 'a' && s[4] == 'y' && s[5] == 0)
      {
        Token = TK_Array;
      } else if (s[1] == 'u' && s[2] == 't' && s[3] == 'o' && s[4] == 0)
      {
        Token = TK_Auto;
      }
      break;
    case 'b':
      if (s[1] == 'o' && s[2] == 'o' && s[3] == 'l' && s[4] == 0)
      {
        Token = TK_Bool;
      } else if (s[1] == 'r' && s[2] == 'e' && s[3] == 'a' && s[4] == 'k' && s[5] == 0)
      {
        Token = TK_Break;
      } else if (s[1] == 'y' && s[2] == 't' && s[3] == 'e' && s[4] == 0)
      {
        Token = TK_Byte;
      }
      break;
    case 'c':
      if (s[1] == 'a' && s[2] == 's' && s[3] == 'e' && s[4] == 0)
      {
        Token = TK_Case;
      } else if (s[1] == 'l' && s[2] == 'a' && s[3] == 's' && s[4] == 's' && s[5] == 0)
      {
        Token = TK_Class;
      } else if (s[1] == 'o' && s[2] == 'n') {
        if (s[3] == 's' && s[4] == 't' && s[5] == 0)
        {
          Token = TK_Const;
        } else if (s[3] == 't' && s[4] == 'i' && s[5] == 'n' && s[6] == 'u' && s[7] == 'e' && s[8] == 0)
        {
          Token = TK_Continue;
        }
      }
      break;
    case 'd':
      if (s[1] == 'e') {
        if (s[2] == 'c' && s[3] == 'o' && s[4] == 'r' &&
            s[5] == 'a' && s[6] == 't' && s[7] == 'e' && s[8] == 0)
        {
          Token = TK_Decorate;
        } else if (s[2] == 'f' && s[3] == 'a' && s[4] == 'u' && s[5] == 'l' && s[6] == 't')
        {
          if (s[7] == 0)
          {
            Token = TK_Default;
          } else if (s[7] == 'p' && s[8] == 'r' && s[9] == 'o' &&
                     s[10] == 'p' && s[11] == 'e' && s[12] == 'r' &&
                     s[13] == 't' && s[14] == 'i' && s[15] == 'e' &&
                     s[16] == 's' && s[17] == 0)
          {
            Token = TK_DefaultProperties;
          }
        } else if (s[2] == 'l' && s[3] == 'e' && s[4] == 'g' && s[5] == 'a' &&
                   s[6] == 't' && s[7] == 'e' && s[8] == 0)
        {
          Token = TK_Delegate;
        }
      } else if (s[1] == 'o' && s[2] == 0)
      {
        Token = TK_Do;
      }
      break;
    case 'e':
      if (s[1] == 'l' && s[2] == 's' && s[3] == 'e' && s[4] == 0)
      {
        Token = TK_Else;
      } else if (s[1] == 'n' && s[2] == 'u' && s[3] == 'm' && s[4] == 0)
      {
        Token = TK_Enum;
      }
      break;
    case 'f':
      if (s[1] == 'a' && s[2] == 'l' && s[3] == 's' && s[4] == 'e' && s[5] == 0)
      {
        Token = TK_False;
      } else if (s[1] == 'i' && s[2] == 'n' && s[3] == 'a' && s[4] == 'l' && s[5] == 0)
      {
        Token = TK_Final;
      } else if (s[1] == 'l' && s[2] == 'o' && s[3] == 'a' && s[4] == 't' && s[5] == 0)
      {
        Token = TK_Float;
      } else if (s[1] == 'o' && s[2] == 'r')
      {
        if (s[3] == 0)
        {
          Token = TK_For;
        } else if (s[3] == 'e' && s[4] == 'a' && s[5] == 'c' && s[6] == 'h' && s[7] == 0)
        {
          Token = TK_Foreach;
        }
      }
      break;
    case 'g':
      if (s[1] == 'a' && s[2] == 'm' && s[3] == 'e' && s[4] == 0)
      {
        Token = TK_Game;
      } else if (s[1] == 'e' && s[2] == 't' && s[3] == 0)
      {
        Token = TK_Get;
      }
    case 'i':
      if (s[1] == 'f' && s[2] == 0)
      {
        Token = TK_If;
      } else if (s[1] == 'm' && s[2] == 'p' && s[3] == 'o' && s[4] == 'r' && s[5] == 't' && s[6] == 0)
      {
        Token = TK_Import;
      } else if (s[1] == 'n' && s[2] == 't' && s[3] == 0)
      {
        Token = TK_Int;
      } else if (s[1] == 't' && s[2] == 'e' && s[3] == 'r' && s[4] == 'a' &&
                 s[5] == 't' && s[6] == 'o' && s[7] == 'r' && s[8] == 0)
      {
        Token = TK_Iterator;
      }
      break;
    case 'n':
      if (s[1] == 'a')
      {
        if (s[2] == 'm' && s[3] == 'e' && s[4] == 0)
        {
          Token = TK_Name;
        } else if (s[2] == 't' && s[3] == 'i' && s[4] == 'v' && s[5] == 'e' && s[6] == 0)
        {
          Token = TK_Native;
        }
      } else if (s[1] == 'o' && s[2] == 'n' && s[3] == 'e' && s[4] == 0)
      {
        Token = TK_None;
      } else if (s[1] == 'u' && s[2] == 'l' && s[3] == 'l' && (s[4] == 0 || (s[4] == 'p' && s[5] == 't' && s[6] == 'r' && s[7] == 0)))
      {
        Token = TK_Null;
      }
      break;
    case 'o':
      if (s[1] == 'p' && s[2] == 't' && s[3] == 'i' && s[4] == 'o' &&
          s[5] == 'n' && s[6] == 'a' && s[7] == 'l' && s[8] == 0)
      {
        Token = TK_Optional;
      } else if (s[1] == 'u' && s[2] == 't' && s[3] == 0)
      {
        Token = TK_Out;
      }
      break;
    case 'p':
      if (s[1] == 'r' && s[2] == 'i' && s[3] == 'v' && s[4] == 'a' &&
          s[5] == 't' && s[6] == 'e' && s[7] == 0)
      {
        Token = TK_Private;
      }
      break;
    case 'r':
      if (s[1] == 'e')
      {
        if (s[2] == 'a' && s[3] == 'd' && s[4] == 'o' && s[5] == 'n' &&
            s[6] == 'l' && s[7] == 'y' && s[8] == 0)
        {
          Token = TK_ReadOnly;
        } else if (s[2] == 'l' && s[3] == 'i' && s[4] == 'a' &&
                   s[5] == 'b' && s[6] == 'l' && s[7] == 'e' && s[8] == 0)
        {
          Token = TK_Reliable;
        } else if (s[2] == 'p' && s[3] == 'l' && s[4] == 'i' &&
                   s[5] == 'c' && s[6] == 'a' && s[7] == 't' && s[8] == 'i' &&
                   s[9] == 'o' && s[10] == 'n' && s[11] == 0)
        {
          Token = TK_Replication;
        } else if (s[2] == 't' && s[3] == 'u' && s[4] == 'r' && s[5] == 'n' && s[6] == 0)
        {
          Token = TK_Return;
        }
      }
      break;
    case 's':
      if (s[1] == 'e')
      {
        if (s[2] == 'l' && s[3] == 'f' && s[4] == 0)
        {
          Token = TK_Self;
        } else if (s[2] == 't' && s[3] == 0)
        {
          Token = TK_Set;
        }
      } else if (s[1] == 'p' && s[2] == 'a' && s[3] == 'w' && s[4] == 'n' &&
                 s[5] == 'e' && s[6] == 'r' && s[7] == 0)
      {
        Token = TK_Spawner;
      } else if (s[1] == 't')
      {
        if (s[2] == 'a' && s[3] == 't')
        {
          if (s[4] == 'e')
          {
            if (s[5] == 0)
            {
              Token = TK_State;
            } else if (s[5] == 's' && s[6] == 0)
            {
              Token = TK_States;
            }
          } else if (s[4] == 'i' && s[5] == 'c' && s[6] == 0)
          {
            Token = TK_Static;
          }
        } else if (s[2] == 'r')
        {
          if (s[3] == 'i' && s[4] == 'n' && s[5] == 'g' && s[6] == 0)
          {
            Token = TK_String;
          } else if (s[3] == 'u' && s[4] == 'c' && s[5] == 't' &&
            s[6] == 0)
          {
            Token = TK_Struct;
          }
        }
      } else if (s[1] == 'w' && s[2] == 'i' && s[3] == 't' && s[4] == 'c' && s[5] == 'h' && s[6] == 0)
      {
        Token = TK_Switch;
      }
      break;
    case 't':
      if (s[1] == 'r')
      {
        if (s[2] == 'a' && s[3] == 'n' && s[4] == 's' && s[5] == 'i' &&
            s[6] == 'e' && s[7] == 'n' && s[8] == 't' && s[9] == 0)
        {
          Token = TK_Transient;
        } else if (s[2] == 'u' && s[3] == 'e' && s[4] == 0)
        {
          Token = TK_True;
        }
      }
      break;
    case 'u':
      if (s[1] == 'n' && s[2] == 'r' && s[3] == 'e' && s[4] == 'l' &&
          s[5] == 'i' && s[6] == 'a' && s[7] == 'b' && s[8] == 'l' &&
          s[9] == 'e' && s[10] == 0)
      {
        Token = TK_Unreliable;
      }
      break;
    case 'v':
      if (s[1] == 'e' && s[2] == 'c' && s[3] == 't' && s[4] == 'o' &&
          s[5] == 'r' && s[6] == 0)
      {
        Token = TK_Vector;
      } else if (s[1] == 'o' && s[2] == 'i' && s[3] == 'd' && s[4] == 0)
      {
        Token = TK_Void;
      }
      break;
    case 'w':
      if (s[1] == 'h' && s[2] == 'i' && s[3] == 'l' && s[4] == 'e' && s[5] == 0)
      {
        Token = TK_While;
      }
      break;
    case 'N':
      if (s[0] == 'N' && s[1] == 'U' && s[2] == 'L' && s[3] == 'L' && s[4] == 0)
      {
        Token = TK_Null;
      }
      break;
  } // switch

  if (Token == TK_Identifier) Name = TokenStringBuffer;
}


//==========================================================================
//
//  VLexer::ProcessSpecialToken
//
//==========================================================================
void VLexer::ProcessSpecialToken () {
  char c = Chr;
  NextChr();
  switch (c) {
    case '+':
      if (Chr == '=') {
        Token = TK_AddAssign;
        NextChr();
      } else if (Chr == '+') {
        Token = TK_Inc;
        NextChr();
      } else {
        Token = TK_Plus;
      }
      break;
    case '-':
      if (Chr == '=') {
        Token = TK_MinusAssign;
        NextChr();
      } else if (Chr == '-') {
        Token = TK_Dec;
        NextChr();
      } else if (Chr == '>') {
        Token = TK_Arrow;
        NextChr();
      } else {
        Token = TK_Minus;
      }
      break;
    case '*':
      if (Chr == '=') {
        Token = TK_MultiplyAssign;
        NextChr();
      } else {
        Token = TK_Asterisk;
      }
      break;
    case '/':
      if (Chr == '=') {
        Token = TK_DivideAssign;
        NextChr();
      } else {
        Token = TK_Slash;
      }
      break;
    case '%':
      if (Chr == '=') {
        Token = TK_ModAssign;
        NextChr();
      } else {
        Token = TK_Percent;
      }
      break;
    case '=':
      if (Chr == '=') {
        Token = TK_Equals;
        NextChr();
      } else {
        Token = TK_Assign;
      }
      break;
    case '<':
      if (Chr == '<') {
        NextChr();
        if (Chr == '=') {
          Token = TK_LShiftAssign;
          NextChr();
        } else {
          Token = TK_LShift;
        }
      } else if (Chr == '=') {
        Token = TK_LessEquals;
        NextChr();
      } else {
        Token = TK_Less;
      }
      break;
    case '>':
      if (Chr == '>') {
        NextChr();
        if (Chr == '=') {
          Token = TK_RShiftAssign;
          NextChr();
        } else {
          Token = TK_RShift;
        }
      } else if (Chr == '=') {
        Token = TK_GreaterEquals;
        NextChr();
      } else {
        Token = TK_Greater;
      }
      break;
    case '!':
      if (Chr == '=') {
        Token = TK_NotEquals;
        NextChr();
      } else {
        Token = TK_Not;
      }
      break;
    case '&':
      if (Chr == '=') {
        Token = TK_AndAssign;
        NextChr();
      } else if (Chr == '&') {
        Token = TK_AndLog;
        NextChr();
      } else {
        Token = TK_And;
      }
      break;
    case '|':
      if (Chr == '=') {
        Token = TK_OrAssign;
        NextChr();
      } else if (Chr == '|') {
        Token = TK_OrLog;
        NextChr();
      } else {
        Token = TK_Or;
      }
      break;
    case '^':
      if (Chr == '=') {
        Token = TK_XOrAssign;
        NextChr();
      } else {
        Token = TK_XOr;
      }
      break;
    case '.':
      if (Chr == '.' && Src->FilePtr[0] == '.') {
        Token = TK_VarArgs;
        NextChr();
        NextChr();
      } else {
        Token = TK_Dot;
      }
      break;
    case ':':
      if (Chr == ':') {
        Token = TK_DColon;
        NextChr();
      } else {
        Token = TK_Colon;
      }
      break;
    case '(':
      Token = TK_LParen;
      break;
    case ')':
      Token = TK_RParen;
      break;
    case '?':
      Token = TK_Quest;
      break;
    case '~':
      Token = TK_Tilde;
      break;
    case ',':
      Token = TK_Comma;
      break;
    case ';':
      Token = TK_Semicolon;
      break;
    case '[':
      Token = TK_LBracket;
      break;
    case ']':
      Token = TK_RBracket;
      break;
    case '{':
      Token = TK_LBrace;
      break;
    case '}':
      Token = TK_RBrace;
      break;
    default:
      ParseError(Location, ERR_BAD_CHARACTER, "Unknown punctuation \'%c\'", Chr);
      Token = TK_NoToken;
      break;
  }
}


//==========================================================================
//
//  VLexer::ProcessFileName
//
//==========================================================================
void VLexer::ProcessFileName () {
  int len = 0;
  NextChr();
  while (Chr != '\"') {
    if (len >= MAX_QUOTED_LENGTH-1) {
      ParseError(Location, ERR_STRING_TOO_LONG);
      NextChr();
      continue;
    }
    if (Chr == EOF_CHARACTER) {
      ParseError(Location, ERR_EOF_IN_STRING);
      break;
    }
    if (Src->IncLineNumber) ParseError(Location, ERR_NEW_LINE_INSIDE_QUOTE);
    TokenStringBuffer[len] = Chr;
    NextChr();
    ++len;
  }
  TokenStringBuffer[len] = 0;
  NextChr();
}


//==========================================================================
//
//  VLexer::Check
//
//==========================================================================
bool VLexer::Check (EToken tk) {
  if (Token == tk) {
    NextToken();
    return true;
  }
  return false;
}


//==========================================================================
//
//  VLexer::Expect
//
//  Report error, if current token is not equals to tk.
//  Take next token.
//
//==========================================================================
void VLexer::Expect (EToken tk) {
  if (Token != tk) {
    ParseError(Location, "expected %s, found %s", TokenNames[tk], TokenNames[Token]);
  }
  NextToken();
}


//==========================================================================
//
//  VLexer::Expect
//
//  Report error, if current token is not equals to tk.
//  Take next token.
//
//==========================================================================
void VLexer::Expect (EToken tk, ECompileError error) {
  if (Token != tk) {
    ParseError(Location, error, "expected %s, found %s", TokenNames[tk], TokenNames[Token]);
  }
  NextToken();
}
