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
#include "vc_local.h"

//#define VC_LEXER_DUMP_COLLECTED_NUMBERS


// ////////////////////////////////////////////////////////////////////////// //
const char *VLexer::TokenNames[] = {
#define VC_LEXER_DEFTOKEN(name,str)  str,
#include "vc_lexer_tokens.h"
#undef VC_LEXER_DEFTOKEN
  nullptr
};

char VLexer::ASCIIToChrCode[256];
vuint8 VLexer::ASCIIToHexDigit[256];
bool VLexer::tablesInited = false;


//==========================================================================
//
//  VLexer::VLexer
//
//==========================================================================
VLexer::VLexer ()
  : sourceOpen(false)
  , src(nullptr)
  , Token(TK_NoToken)
  , Number(0)
  , Float(0)
  , Name(NAME_None)
  , userdata(nullptr)
  , dgOpenFile(nullptr)
{
  memset(tokenStringBuffer, 0, sizeof(tokenStringBuffer));
  if (!tablesInited) {
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
    tablesInited = true;
  }
  String = tokenStringBuffer;
}


//==========================================================================
//
//  VLexer::~VLexer
//
//==========================================================================
VLexer::~VLexer () {
  while (src) PopSource();
  sourceOpen = false;
}


//==========================================================================
//
//  VLexer::doOpenFile
//
//  returns `null` if file not found
//  by default it tries to call `dgOpenFile()`, if it is specified,
//  otherwise falls back to standard vfs
//
//==========================================================================
VStream *VLexer::doOpenFile (const VStr &filename) {
  if (filename.length() == 0) return nullptr; // just in case
  VStr fname = filename;
#ifdef WIN32
  fname = fname.fixSlashes();
#endif
  if (dgOpenFile) return dgOpenFile(this, fname);
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  return FL_OpenFileRead(fname);
#else
  return fsysOpenFile(fname);
#endif
}


//==========================================================================
//
//  VLexer::OpenSource
//
//==========================================================================
void VLexer::OpenSource (const VStr &FileName) {
  // read file and prepare for compilation
  PushSource(FileName);
  sourceOpen = true;
  Token = TK_NoToken;
}


//==========================================================================
//
//  VLexer::OpenSource
//
//==========================================================================
void VLexer::OpenSource (VStream *astream, const VStr &FileName) {
  // read file and prepare for compilation
  PushSource(astream, FileName);
  sourceOpen = true;
  Token = TK_NoToken;
}


//==========================================================================
//
//  VLexer::PushSource
//
//==========================================================================
void VLexer::PushSource (const VStr &FileName) {
  PushSource(doOpenFile(FileName), FileName);
}


//==========================================================================
//
//  VLexer::PushSource
//
//==========================================================================
void VLexer::PushSource (VStream *Strm, const VStr &FileName) {
  if (!Strm) {
    FatalError("VC: Couldn't open '%s'", *FileName);
    return;
  }

  VSourceFile *NewSrc = new VSourceFile();
  NewSrc->Next = src;
  src = NewSrc;

  // copy file name
  NewSrc->FileName = FileName;

  // extract path to the file
  const char *PathEnd = *FileName+FileName.Length()-1;
#ifdef WIN32
  while (PathEnd >= *FileName && *PathEnd != '/' && *PathEnd != '\\') --PathEnd;
#else
  while (PathEnd >= *FileName && *PathEnd != '/') --PathEnd;
#endif
  if (PathEnd >= *FileName) NewSrc->Path = VStr(FileName, 0, (PathEnd-(*FileName))+1);

  // read the file
  int FileSize = Strm->TotalSize();
  if (Strm->IsError() || FileSize < 0) { delete Strm; FatalError("VC: Couldn't read '%s'", *FileName); return; }
  NewSrc->FileStart = new char[FileSize+1];
  Strm->Serialise(NewSrc->FileStart, FileSize);
  if (Strm->IsError() || FileSize < 0) { delete Strm; FatalError("VC: Couldn't read '%s'", *FileName); return; }
  Strm->Close();
  if (Strm->IsError() || FileSize < 0) { delete Strm; FatalError("VC: Couldn't read '%s'", *FileName); return; }
  delete Strm;

  NewSrc->FileStart[FileSize] = 0;
  NewSrc->FileEnd = NewSrc->FileStart+FileSize;
  NewSrc->FilePtr = NewSrc->FileStart;

  // skip garbage some editors add in the begining of UTF-8 files (BOM)
  if ((vuint8)NewSrc->FilePtr[0] == 0xef && (vuint8)NewSrc->FilePtr[1] == 0xbb && (vuint8)NewSrc->FilePtr[2] == 0xbf) NewSrc->FilePtr += 3;

  // save current character and location to be able to restore them
  NewSrc->currCh = currCh;
  NewSrc->Loc = Location;
  NewSrc->CurLoc = CurLocation;

  NewSrc->SourceIdx = TLocation::AddSourceFile(FileName);
  NewSrc->IncLineNumber = false;
  NewSrc->NewLine = true;
  NewSrc->Skipping = false;
  Location = TLocation(NewSrc->SourceIdx, 1, 1);
  CurLocation = TLocation(NewSrc->SourceIdx, 1, 0); // 0, 'cause `NextChr()` will do `ConsumeChar()`
  NextChr();
}


//==========================================================================
//
//  VLexer::PopSource
//
//==========================================================================
void VLexer::PopSource () {
  if (!src) return;
  if (src->IfStates.Num()) ParseError(Location, "#ifdef without a corresponding #endif");
  VSourceFile *Tmp = src;
  delete[] Tmp->FileStart;
  Tmp->FileStart = nullptr;
  src = Tmp->Next;
  currCh = Tmp->currCh;
  Location = Tmp->Loc;
  CurLocation = Tmp->CurLoc;
  delete Tmp;
}


//==========================================================================
//
//  VLexer::NextToken
//
//==========================================================================
void VLexer::NextToken () {
  if (!src) { NewLine = false; Token = TK_EOF; return; }
  NewLine = src->NewLine;
  do {
    tokenStringBuffer[0] = 0;
    SkipWhitespaceAndComments();
    Location = CurLocation;

    if (src->NewLine) {
      NewLine = true;
      // a new line has been started, check preprocessor directive
      src->NewLine = false;
      if (currCh == '#') {
        ProcessPreprocessor();
        continue;
      }
    }

    switch (ASCIIToChrCode[(vuint8)currCh]) {
      case CHR_EOF: PopSource(); Token = (src ? TK_NoToken : TK_EOF); break;
      case CHR_Letter: ProcessLetterToken(true); break;
      case CHR_Number: ProcessNumberToken(); break;
      case CHR_Quote: ProcessQuoteToken(); break;
      case CHR_SingleQuote: ProcessSingleQuoteToken(); break;
      default: ProcessSpecialToken(); break;
    }

    if (Token != TK_EOF && src->Skipping) Token = TK_NoToken;
  } while (Token == TK_NoToken);
}


//==========================================================================
//
//  VLexer::NextChr
//
//==========================================================================
void VLexer::NextChr () {
  if (src->FilePtr >= src->FileEnd) {
    currCh = EOF_CHARACTER;
    return;
  }
  CurLocation.ConsumeChar(src->IncLineNumber);
  src->IncLineNumber = false;
  currCh = *src->FilePtr++;
  if ((vuint8)currCh < ' ' || (vuint8)currCh == EOF_CHARACTER) {
    if (currCh == '\n') {
      src->IncLineNumber = true;
      src->NewLine = true;
    }
    currCh = ' ';
  }
}


//==========================================================================
//
//  VLexer::Peek
//
//==========================================================================
char VLexer::Peek (int dist) const {
  if (dist < 0) ParseError(Location, "VC INTERNAL COMPILER ERROR: peek dist is negative!");
  if (dist == 0) {
    return (src->FilePtr > src->FileStart ? src->FilePtr[-1] : src->FilePtr[0]);
  } else {
    --dist;
    if (src->FileEnd-src->FilePtr <= dist) return EOF_CHARACTER;
    return src->FilePtr[dist];
  }
}


//==========================================================================
//
//  VLexer::SkipWhitespaceAndComments
//
//==========================================================================
void VLexer::SkipWhitespaceAndComments () {
  Location = CurLocation;
  bool Done;
  do {
    Done = true;
    while (currCh == ' ') NextChr();
    if (currCh == '/' && *src->FilePtr == '*') {
      // block comment
      NextChr();
      do {
        NextChr();
        if (currCh == EOF_CHARACTER) {
          ParseError(Location, "End of file inside a comment");
          return;
        }
      } while (currCh != '*' || *src->FilePtr != '/');
      NextChr();
      NextChr();
      Done = false;
    } else if (currCh == '/' && *src->FilePtr == '+') {
      // nested block comment
      NextChr();
      int level = 1;
      for (;;) {
        NextChr();
        if (currCh == EOF_CHARACTER) {
          ParseError(Location, "End of file inside a comment");
          return;
        }
        if (currCh == '+' && *src->FilePtr == '/') {
          NextChr();
          if (--level == 0) { NextChr(); break; }
        } else if (currCh == '/' && *src->FilePtr == '+') {
          NextChr();
          ++level;
        }
      }
      Done = false;
    } else if (currCh == '/' && *src->FilePtr == '/') {
      // c++ style comment
      NextChr();
      do {
        NextChr();
        if (currCh == EOF_CHARACTER) {
          ParseError(Location, "End of file inside a comment");
          return;
        }
      } while (!src->IncLineNumber);
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
  const char *fpos = src->FilePtr;
  if (fpos > src->FileStart) --fpos; // unget last char
  while (fpos < src->FileEnd) {
    char ch = *fpos++;
    if ((vuint8)ch <= ' ') continue;
    // comment?
    if (ch == '/' && fpos < src->FileEnd) {
      ch = *fpos++;
      // single-line?
      if (ch == '/') {
        while (fpos < src->FileEnd && *fpos != '\n') ++fpos;
        continue;
      }
      // multiline?
      if (ch == '*') {
        while (fpos < src->FileEnd) {
          if (*fpos == '*' && src->FileEnd-fpos > 1 && fpos[1] == '/') { fpos += 2; break; }
          ++fpos;
        }
        continue;
      }
      // multiline nested?
      if (ch == '+') {
        int level = 1;
        while (fpos < src->FileEnd) {
          if (*fpos == '+' && src->FileEnd-fpos > 1 && fpos[1] == '/') {
            fpos += 2;
            if (--level == 0) break;
          } else if (*fpos == '/' && src->FileEnd-fpos > 1 && fpos[1] == '+') {
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
  while (currCh != EOF_CHARACTER && !src->NewLine && (vuint8)currCh <= ' ') NextChr();

  if (src->NewLine || currCh == EOF_CHARACTER) {
    ParseError(Location, "Compiler directive expected");
    return;
  }

  if (ASCIIToChrCode[(vuint8)currCh] != CHR_Letter) {
    ParseError(Location, "Compiler directive expected");
    while (!src->NewLine && currCh != EOF_CHARACTER) NextChr();
    return;
  }

  ProcessLetterToken(false);

  if (!VStr::Cmp(tokenStringBuffer, "line")) {
    // read line number
    SkipWhitespaceAndComments();
    if (ASCIIToChrCode[(vuint8)currCh] != CHR_Number) ParseError(Location, "`#line`: line number expected");
    ProcessNumberToken();
    if (Token != TK_IntLiteral) ParseError(Location, "`#line`: integer expected");
    auto lno = Number-1;

    // read file name
    SkipWhitespaceAndComments();
    if (ASCIIToChrCode[(vuint8)currCh] != CHR_Quote) ParseError(Location, "`#line`: file name expected");
    ProcessFileName();
    src->SourceIdx = TLocation::AddSourceFile(String);
    Location = TLocation(src->SourceIdx, lno, 0);
    CurLocation = Location;

    // ignore flags
    while (!src->NewLine) NextChr();
  } else if (!VStr::Cmp(tokenStringBuffer, "define")) {
    ProcessDefine();
  } else if (!VStr::Cmp(tokenStringBuffer, "ifdef")) {
    ProcessIf(true);
  } else if (!VStr::Cmp(tokenStringBuffer, "ifndef")) {
    ProcessIf(false);
  } else if (!VStr::Cmp(tokenStringBuffer, "else")) {
    ProcessElse();
  } else if (!VStr::Cmp(tokenStringBuffer, "endif")) {
    ProcessEndIf();
  } else if (!VStr::Cmp(tokenStringBuffer, "include")) {
    ProcessInclude();
    return;
  } else {
    ParseError(Location, "Unknown compiler directive `%s`", tokenStringBuffer);
    while (!src->NewLine && currCh != EOF_CHARACTER) NextChr();
  }
  Token = TK_NoToken;

  SkipWhitespaceAndComments();
  // a new-line is expected at the end of preprocessor directive.
  if (!src->NewLine) ParseError(Location, "Compiler directive contains extra code");
}


//==========================================================================
//
//  VLexer::ProcessDefine
//
//==========================================================================
void VLexer::ProcessDefine () {
  SkipWhitespaceAndComments();

  // argument to the #define must be on the same line.
  if (src->NewLine || currCh == EOF_CHARACTER) {
    ParseError(Location, "`#define`: missing argument");
    return;
  }

  // parse name to be defined
  if (ASCIIToChrCode[(vuint8)currCh] != CHR_Letter) {
    ParseError(Location, "`#define`: invalid define name");
    while (!src->NewLine && currCh != EOF_CHARACTER) NextChr();
    return;
  }
  ProcessLetterToken(false);

  if (src->Skipping) return;

  AddDefine(tokenStringBuffer);
}


//==========================================================================
//
//  VLexer::AddDefine
//
//==========================================================================
void VLexer::AddDefine (const VStr &CondName, bool showWarning) {
  if (CondName.length() == 0) return; // get lost!
  // check for redefined names
  for (int i = 0; i < defines.length(); ++i) {
    if (defines[i] == CondName) {
      if (showWarning) ParseWarning(Location, "Redefined conditional '%s'", *CondName);
      return;
    }
  }
  defines.Append(CondName);
}


//==========================================================================
//
//  VLexer::ProcessIf
//
//==========================================================================
void VLexer::ProcessIf (bool OnTrue) {
  SkipWhitespaceAndComments();

  // argument to the #ifdef must be on the same line
  if (src->NewLine || currCh == EOF_CHARACTER) {
    ParseError(Location, "`#if`: missing argument");
    return;
  }

  // parse condition name
  if (ASCIIToChrCode[(vuint8)currCh] != CHR_Letter) {
    ParseError(Location, "`#if`: invalid argument");
    while (!src->NewLine && currCh != EOF_CHARACTER) NextChr();
    return;
  }

  ProcessLetterToken(false);

  if (src->Skipping) {
    src->IfStates.Append(IF_Skip);
  } else {
    // check if the names has been defined
    bool Found = false;
    for (int i = 0; i < defines.Num(); ++i) {
      if (defines[i] == tokenStringBuffer) {
        Found = true;
        break;
      }
    }
    if (Found == OnTrue) {
      src->IfStates.Append(IF_True);
    } else {
      src->IfStates.Append(IF_False);
      src->Skipping = true;
    }
  }
}


//==========================================================================
//
//  VLexer::ProcessElse
//
//==========================================================================
void VLexer::ProcessElse () {
  if (!src->IfStates.Num()) {
    ParseError(Location, "`#else` without an `#ifdef`/`#ifndef`");
    return;
  }
  switch (src->IfStates[src->IfStates.Num()-1]) {
    case IF_True:
      src->IfStates[src->IfStates.Num()-1] = IF_ElseFalse;
      src->Skipping = true;
      break;
    case IF_False:
      src->IfStates[src->IfStates.Num()-1] = IF_ElseTrue;
      src->Skipping = false;
      break;
    case IF_Skip:
      src->IfStates[src->IfStates.Num()-1] = IF_ElseSkip;
      break;
    case IF_ElseTrue:
    case IF_ElseFalse:
    case IF_ElseSkip:
      ParseError(Location, "Multiple `#else` directives for a single `#ifdef`");
      src->Skipping = true;
      break;
  }
}


//==========================================================================
//
//  VLexer::ProcessEndIf
//
//==========================================================================
void VLexer::ProcessEndIf () {
  if (!src->IfStates.Num()) {
    ParseError(Location, "`#endif` without an `#ifdef`/`#ifndef`");
    return;
  }
  src->IfStates.RemoveIndex(src->IfStates.Num()-1);
  if (src->IfStates.Num() > 0) {
    switch (src->IfStates[src->IfStates.Num()-1]) {
      case IF_True:
      case IF_ElseTrue:
        src->Skipping = false;
        break;
      case IF_False:
      case IF_ElseFalse:
        src->Skipping = true;
        break;
      case IF_Skip:
      case IF_ElseSkip:
        break;
    }
  } else {
    src->Skipping = false;
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
  if (src->NewLine || currCh == EOF_CHARACTER) {
    ParseError(Location, "`#include`: file name expected");
    return;
  }

  // parse file name
  if (currCh != '\"') {
    ParseError(Location, "`#include`: string expected");
    while (!src->NewLine && currCh != EOF_CHARACTER) NextChr();
    return;
  }
  ProcessFileName();
  //TLocation Loc = Location;

  Token = TK_NoToken;
  SkipWhitespaceAndComments();
  // a new-line is expected at the end of preprocessor directive.
  if (!src->NewLine) ParseError(Location, "`#include`: extra arguments");

  if (src->Skipping) return;

  // check if it's an absolute path location.
  if (tokenStringBuffer[0] != '/' && tokenStringBuffer[0] != '\\') {
    // first try relative to the current source file
    if (src->Path.IsNotEmpty()) {
      VStr FileName = src->Path+VStr(tokenStringBuffer);
      VStream *Strm = doOpenFile(FileName);
      if (Strm) {
        PushSource(/*Loc,*/ Strm, FileName);
        return;
      }
    }

    for (int i = includePath.Num()-1; i >= 0; --i) {
      VStr FileName = includePath[i]+VStr(tokenStringBuffer);
      VStream *Strm = doOpenFile(FileName);
      if (Strm) {
        PushSource(/*Loc,*/ Strm, FileName);
        return;
      }
    }
  }

  // either it's relative to the current directory or absolute path
  PushSource(/*Loc,*/ tokenStringBuffer);
}


//==========================================================================
//
//  VLexer::AddIncludePath
//
//==========================================================================
void VLexer::AddIncludePath (const VStr &DirName) {
  if (DirName.length() == 0) return; // get lost
  VStr copy = DirName;
  // append trailing slash if needed
#ifndef WIN32
  if (!copy.EndsWith("/")) copy += '/';
#else
  if (!copy.EndsWith("/") && !copy.EndsWith("\\")) copy += '/';
  copy = copy.fixSlashes();
#endif
  // check for duplicate pathes
  for (int i = 0; i < includePath.length(); ++i) {
#ifndef WIN32
    if (includePath[i] == copy) return;
#else
    if (includePath[i].ICmp(copy) == 0) return;
#endif
  }
  includePath.Append(copy);
}


//==========================================================================
//
//  VLexer::ProcessNumberToken
//
//==========================================================================
void VLexer::ProcessNumberToken () {
  Token = TK_IntLiteral;

  char c = currCh;
  NextChr();

#if 0
  Number = c-'0';
  // hex/octal/decimal/binary constant?
  if (c == '0') {
    int base = 0;
    switch (currCh) {
      case 'x': case 'X': base = 16; break;
      case 'd': case 'D': base = 10; break;
      case 'o': case 'O': base = 8; break;
      case 'b': case 'B': base = 2; break;
    }
    if (base != 0) {
      NextChr();
      for (;;) {
        if (currCh != '_') {
          if (base == 16) {
            if (ASCIIToHexDigit[(vuint8)currCh] == NON_HEX_DIGIT) break;
            Number = (Number<<4)+ASCIIToHexDigit[(vuint8)currCh];
          } else {
            if (currCh < '0' || currCh >= '0'+base) break;
            Number = Number*base+(currCh-'0');
          }
        }
        NextChr();
      }
      if (isAlpha(currCh)) ParseError(Location, "Invalid number");
      return;
    }
  }

  for (;;) {
    if (currCh != '_') {
      if (ASCIIToChrCode[(vuint8)currCh] != CHR_Number) break;
      Number = 10*Number+(currCh-'0');
    }
    NextChr();
  }

  if (currCh == '.') {
    char nch = Peek(1);
    if (isAlpha(nch) || nch == '_' || nch == '.') {
      // num dot smth
      return; // so 10.seconds is allowed
    } else {
      Token = TK_FloatLiteral;
      NextChr(); // skip dot
      Float = Number;
      float fmul = 0.1f;
      for (;;) {
        if (currCh != '_') {
          if (ASCIIToChrCode[(vuint8)currCh] != CHR_Number) break;
          Float += (currCh-'0')*fmul;
          fmul /= 10.0f;
        }
        NextChr();
      }
      if (currCh == 'f') NextChr();
    }
  }

  if (isAlpha(currCh)) ParseError(Location, "Invalid number");
#else
  // collect number
  char numbuf[256];
  numbuf[0] = c;
  unsigned int nbpos = 1;
  bool isHex = false;
  Number = 0;
  if (c == '0') {
    int base = 0;
    switch (currCh) {
      case 'x': case 'X': isHex = true; numbuf[nbpos++] = 'x'; break;
      case 'd': case 'D': base = 10; break;
      case 'o': case 'O': base = 8; break;
      case 'b': case 'B': base = 2; break;
    }
    if (base != 0) {
      // other radixes, only integers
      NextChr();
      for (;;) {
        if (currCh != '_') {
          int d = VStr::digitInBase(currCh, base);
          if (d < 0) break;
          Number = Number*base+d;
        }
        NextChr();
      }
#ifdef VC_LEXER_DUMP_COLLECTED_NUMBERS
      fprintf(stderr, "*** CONVERTED INT(base=%d): %d\n", base, Number);
#endif
      if (isAlpha(currCh)) ParseError(Location, "Invalid number");
      return;
    }
    if (isHex) NextChr(); // skip 'x'
  }
  // collect integral part
  int xbase = (isHex ? 16 : 10);
  for (;;) {
    if (currCh != '_') {
      int d = VStr::digitInBase(currCh, xbase);
      if (d < 0) break;
      if (nbpos >= sizeof(numbuf)-1) ParseError(Location, "Invalid number");
      numbuf[nbpos++] = currCh;
    }
    NextChr();
  }
  if (currCh == '.') {
    char nch = Peek(1);
    //if (isAlpha(nch) || nch == '_' || nch == '.')
    if (VStr::digitInBase(nch, xbase) < 0)
    {
      // num dot smth
      numbuf[nbpos++] = 0;
#ifdef VC_LEXER_DUMP_COLLECTED_NUMBERS
      fprintf(stderr, "*** COLLECTED INT(0): <%s>\n", numbuf);
#endif
      if (!VStr::convertInt(numbuf, &Number)) ParseError(Location, "Invalid floating number");
      return; // so 10.seconds is allowed
    }
  }
  if (currCh == 'f' || currCh == '.' || (isHex && (currCh == 'p' || currCh == 'P')) || (!isHex && (currCh == 'e' || currCh == 'E'))) {
    // floating number
    Token = TK_FloatLiteral;
    // dotted part
    if (currCh == '.') {
      if (nbpos >= sizeof(numbuf)-1) ParseError(Location, "Invalid number");
      numbuf[nbpos++] = '.';
      NextChr(); // skip dot
      for (;;) {
        if (currCh != '_') {
          int d = VStr::digitInBase(currCh, xbase);
          if (d < 0) break;
          if (nbpos >= sizeof(numbuf)-1) ParseError(Location, "Invalid number");
          numbuf[nbpos++] = currCh;
        }
        NextChr();
      }
    }
    // exponent
    if ((isHex && (currCh == 'p' || currCh == 'P')) || (!isHex && (currCh == 'e' || currCh == 'E'))) {
      if (nbpos >= sizeof(numbuf)-1) ParseError(Location, "Invalid floating number exponent");
      numbuf[nbpos++] = (isHex ? 'p' : 'e');
      NextChr(); // skip e/p
      if (currCh == '+' || currCh == '-') {
        if (nbpos >= sizeof(numbuf)-1) ParseError(Location, "Invalid floating number exponent");
        numbuf[nbpos++] = currCh;
        NextChr(); // skip sign
      }
      for (;;) {
        if (currCh != '_') {
          int d = VStr::digitInBase(currCh, 10/*xbase*/); // it is decimal for both types
          if (d < 0) break;
          if (nbpos >= sizeof(numbuf)-1) ParseError(Location, "Invalid floating number exponent");
          numbuf[nbpos++] = currCh;
        }
        NextChr();
      }
    }
    // skip optional 'f'
    if (currCh == 'f') NextChr();
    if (isAlpha(currCh) || (currCh >= '0' && currCh <= '9')) ParseError(Location, "Invalid floating number");
    if (nbpos >= sizeof(numbuf)-1) ParseError(Location, "Invalid floating number");
    numbuf[nbpos++] = 0;
#ifdef VC_LEXER_DUMP_COLLECTED_NUMBERS
    fprintf(stderr, "*** COLLECTED FLOAT: <%s>\n", numbuf);
#endif
    if (!VStr::convertFloat(numbuf, &Float)) ParseError(Location, "Invalid floating number");
    Number = (int)Float;
  } else {
    if (isAlpha(currCh) || (currCh >= '0' && currCh <= '9')) ParseError(Location, "Invalid integer number");
    if (nbpos >= sizeof(numbuf)-1) ParseError(Location, "Invalid integer number");
    numbuf[nbpos++] = 0;
#ifdef VC_LEXER_DUMP_COLLECTED_NUMBERS
    fprintf(stderr, "*** COLLECTED INT(1): <%s>\n", numbuf);
#endif
    if (!VStr::convertInt(numbuf, &Number)) ParseError(Location, "Invalid integer number");
  }
#endif
}


//==========================================================================
//
//  VLexer::ProcessChar
//
//==========================================================================
void VLexer::ProcessChar () {
  if (currCh == EOF_CHARACTER) {
    ParseError(Location, ERR_EOF_IN_STRING);
    BailOut();
  }
  if (src->IncLineNumber) ParseError(Location, ERR_NEW_LINE_INSIDE_QUOTE);
  if (currCh == '\\') {
    // special symbol
    NextChr();
    if (currCh == EOF_CHARACTER) { ParseError(Location, ERR_EOF_IN_STRING); BailOut(); }
    if (src->IncLineNumber) ParseError(Location, ERR_NEW_LINE_INSIDE_QUOTE);
    switch (currCh) {
      case 'n': currCh = '\n'; break;
      case 'r': currCh = '\r'; break;
      case 't': currCh = '\t'; break;
      case '\'': currCh = '\''; break;
      case '"': currCh = '"'; break;
      case '\\': currCh = '\\'; break;
      case 'c': currCh = TEXT_COLOUR_ESCAPE; break;
      case 'e': currCh = '\x1b'; break;
      case 'x': case 'X':
        {
          NextChr();
          if (currCh == EOF_CHARACTER) { ParseError(Location, ERR_EOF_IN_STRING); BailOut(); }
          if (ASCIIToHexDigit[(vuint8)currCh] == NON_HEX_DIGIT) { ParseError(Location, ERR_EOF_IN_STRING); BailOut(); }
          int n = ASCIIToHexDigit[(vuint8)currCh];
          // second digit
          if (src->FilePtr < src->FileEnd && ASCIIToHexDigit[(vuint8)(*src->FilePtr)] != NON_HEX_DIGIT) {
            NextChr();
            n = n*16+ASCIIToHexDigit[(vuint8)currCh];
          }
          currCh = (char)n;
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
  while (currCh != '\"') {
    if (len >= MAX_QUOTED_LENGTH-1) {
      ParseError(Location, ERR_STRING_TOO_LONG);
      NextChr();
      continue;
    }
    ProcessChar();
    tokenStringBuffer[len] = currCh;
    NextChr();
    ++len;
  }
  tokenStringBuffer[len] = 0;
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
  while (currCh != '\'') {
    if (len >= MAX_IDENTIFIER_LENGTH-1) {
      ParseError(Location, ERR_STRING_TOO_LONG);
      NextChr();
      continue;
    }
    ProcessChar();
    tokenStringBuffer[len] = currCh;
    NextChr();
    ++len;
  }
  tokenStringBuffer[len] = 0;
  NextChr();
  Name = tokenStringBuffer;
}


//==========================================================================
//
//  VLexer::ProcessLetterToken
//
//==========================================================================
void VLexer::ProcessLetterToken (bool CheckKeywords) {
  Token = TK_Identifier;
  int len = 0;
  while (ASCIIToChrCode[(vuint8)currCh] == CHR_Letter || ASCIIToChrCode[(vuint8)currCh] == CHR_Number) {
    if (len == MAX_IDENTIFIER_LENGTH-1) {
      ParseError(Location, ERR_IDENTIFIER_TOO_LONG);
      NextChr();
      continue;
    }
    tokenStringBuffer[len] = currCh;
    ++len;
    NextChr();
  }
  tokenStringBuffer[len] = 0;

  if (!CheckKeywords) return;

  //k8: it was a giant `switch`, but meh... it is 2018 now!
  const char *s = tokenStringBuffer;
  for (unsigned tidx = TK_Abstract; tidx < TK_URShiftAssign; ++tidx) {
    if (s[0] == TokenNames[tidx][0] && strcmp(s, TokenNames[tidx]) == 0) {
      Token = (EToken)tidx;
      break;
    }
  }
  // hacks
  if (Token == TK_Identifier) {
         if (s[0] == 'n' && strcmp(s, "nullptr") == 0) Token = TK_Null;
    else if (s[0] == 'N' && strcmp(s, "NULL") == 0) Token = TK_Null;
  }

  if (Token == TK_Identifier) Name = tokenStringBuffer;
}


//==========================================================================
//
//  VLexer::ProcessSpecialToken
//
//==========================================================================
void VLexer::ProcessSpecialToken () {
  Token = TK_NoToken;
  char tkbuf[8]; // way too much
  size_t tkbpos = 0;
  for (;;) {
    tkbuf[tkbpos] = currCh;
    tkbuf[tkbpos+1] = 0;
    EToken ntk = TK_NoToken;
    for (unsigned tidx = TK_URShiftAssign; tidx < TK_TotalTokenCount; ++tidx) {
      if (tkbuf[0] == TokenNames[tidx][0] && strcmp(tkbuf, TokenNames[tidx]) == 0) {
        ntk = (EToken)tidx;
        break;
      }
    }
    // not found?
    if (ntk == TK_NoToken) {
      // use last found token
      if (Token == TK_NoToken) ParseError(Location, ERR_BAD_CHARACTER, "Unknown punctuation '%s'", tkbuf);
      return;
    }
    // new token found, eat one char and repeat
    Token = ntk;
    NextChr();
    if (++tkbpos >= sizeof(tkbuf)) FatalError("VC: something is very wrong with the lexer");
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
  while (currCh != '"') {
    if (len >= MAX_QUOTED_LENGTH-1) {
      ParseError(Location, ERR_STRING_TOO_LONG);
      NextChr();
      continue;
    }
    if (currCh == EOF_CHARACTER) {
      ParseError(Location, ERR_EOF_IN_STRING);
      break;
    }
    if (src->IncLineNumber) ParseError(Location, ERR_NEW_LINE_INSIDE_QUOTE);
    tokenStringBuffer[len] = currCh;
    NextChr();
    ++len;
  }
  tokenStringBuffer[len] = 0;
  NextChr();
}


//==========================================================================
//
//  VLexer::Check
//
//==========================================================================
bool VLexer::Check (EToken tk) {
  if (Token == tk) { NextToken(); return true; }
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
  if (Token != tk) ParseError(Location, "expected `%s`, found `%s`", TokenNames[tk], TokenNames[Token]);
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
  if (Token != tk) ParseError(Location, error, "expected `%s`, found `%s`", TokenNames[tk], TokenNames[Token]);
  NextToken();
}


//==========================================================================
//
//  VLexer::Check
//
//  check for identifier (it cannot be a keyword)
//
//==========================================================================
bool VLexer::Check (const char *id, bool caseSensitive) {
  check(id);
  check(id[0]);
  if (Token != TK_Identifier) return false;
  bool ok = ((caseSensitive ? VStr::Cmp(id, *Name) : VStr::ICmp(id, *Name)) == 0);
  if (!ok) return false;
  NextToken();
  return true;
}


//==========================================================================
//
//  VLexer::Expect
//
//  expect identifier (it cannot be a keyword)
//
//==========================================================================
void VLexer::Expect (const char *id, bool caseSensitive) {
  check(id);
  check(id[0]);
  if (Token != TK_Identifier) ParseError(Location, "expected `%s`, found `%s`", id, TokenNames[Token]);
  bool ok = ((caseSensitive ? VStr::Cmp(id, *Name) : VStr::ICmp(id, *Name)) == 0);
  if (!ok) ParseError(Location, "expected `%s`, found `%s`", id, *Name);
  NextToken();
}


//==========================================================================
//
// VLexer::isNStrEqu
//
//==========================================================================
bool VLexer::isNStrEqu (int spos, int epos, const char *s) const {
  if (!s) s = "";
  if (spos >= epos) return (s[0] == 0);
  if (spos < 0 || epos > src->FileEnd-src->FileStart) return false;
  auto slen = (int)strlen(s);
  if (epos-spos != slen) return false;
  return (memcmp(src->FileStart+spos, s, slen) == 0);
}


//==========================================================================
//
// VLexer::posAtEOS
//
//==========================================================================
bool VLexer::posAtEOS (int cpos) const {
  if (cpos < 0) cpos = 0;
  return (cpos >= src->FileEnd-src->FileStart);
}


//==========================================================================
//
// VLexer::peekChar
//
// returns 0 on EOS
//
//==========================================================================
vuint8 VLexer::peekChar (int cpos) const {
  if (cpos < 0) cpos = 0;
  if (cpos >= src->FileEnd-src->FileStart) return 0;
  vuint8 ch = src->FileStart[cpos];
  if (ch == 0) ch = ' ';
  return ch;
}


//==========================================================================
//
// VLexer::skipBlanksFrom
//
// returns `false` on EOS
//
//==========================================================================
bool VLexer::skipBlanksFrom (int &cpos) const {
  if (cpos < 0) cpos = 0; // just in case
  for (;;) {
    vuint8 ch = peekChar(cpos);
    if (!ch) break; // EOS
    if (ch <= ' ') { ++cpos; continue; }
    if (ch != '/') return true; // not a comment
    ch = peekChar(cpos+1);
    // block comment?
    if (ch == '*') {
      cpos += 2;
      for (;;) {
        ch = peekChar(cpos);
        if (!ch) return false;
        if (ch == '*' && peekChar(cpos+1) == '/') { cpos += 2; break; }
        ++cpos;
      }
      continue;
    }
    // nested block comment?
    if (ch == '+') {
      int level = 1;
      cpos += 2;
      for (;;) {
        ch = peekChar(cpos);
        if (!ch) return false;
        if (ch == '+' && peekChar(cpos+1) == '/') {
          cpos += 2;
          if (--level == 0) break;
        } else if (ch == '/' && peekChar(cpos+1) == '+') {
          cpos += 2;
          ++level;
        } else {
          ++cpos;
        }
      }
      continue;
    }
    // c++ style comment
    if (ch == '/') {
      for (;;) {
        ch = peekChar(cpos);
        if (!ch) return false;
        ++cpos;
        if (ch == '\n') break;
      }
      continue;
    }
    return true; // nonblank
  }
  return false;
}


//==========================================================================
//
// VLexer::skipTokenFrom
//
// calls skipBlanksFrom, returns token type or TK_NoToken
//
//==========================================================================
EToken VLexer::skipTokenFrom (int &cpos, VStr *str=nullptr) const {
  if (str) str->clear();
  if (!skipBlanksFrom(cpos)) return TK_NoToken;
  // classify token
  vuint8 ch = peekChar(cpos);
  if (!ch) return TK_NoToken; // just in case

  //fprintf(stderr, " tkstart=%d; ch=%c\n", cpos, ch);

  // quoted string?
  if (ch == '"' || ch == '\'') {
    if (str) (*str) += (char)ch;
    vuint8 ech = ch;
    ++cpos;
    for (;;) {
      ch = peekChar(cpos++);
      if (str) (*str) += (char)ch;
      if (ch == '\\') {
        ++cpos; // unconditionally skip next char
      } else {
        if (ch == ech) break;
      }
    }
    return (ech == '"' ? TK_StringLiteral : TK_NameLiteral);
  }

  // number?
  if (ch >= '0' && ch <= '9') {
    if (str) (*str) += (char)ch;
    int base = 0;
    if (ch == '0') {
      switch (peekChar(cpos+1)) {
        case 'b': case 'B': base = 2; break;
        case 'o': case 'O': base = 8; break;
        case 'd': case 'D': base = 10; break;
        case 'x': case 'X': base = 16; break;
      }
      if (base) {
        if (str) (*str) += (char)peekChar(cpos+1);
        cpos += 2;
      }
    }
    for (;;) {
      ch = peekChar(cpos);
      if (ch != '_' && VStr::digitInBase(ch, (base ? base : 10)) < 0) break;
      if (str) (*str) += (char)ch;
      ++cpos;
    }
    if (base != 0) return TK_IntLiteral; // for now, there is no non-decimal floating literals
    if (peekChar(cpos) == '.') {
      vuint8 nch = peekChar(cpos+1);
      if (isAlpha(nch) || nch == '_' || nch == '.' || nch == 0) return TK_IntLiteral; // num dot smth
      // floating literal
      if (str) (*str) += '.';
      ++cpos;
      for (;;) {
        ch = peekChar(cpos);
        if (ch != '_' && VStr::digitInBase(ch, 10) < 0) break;
        if (str) (*str) += (char)ch;
        ++cpos;
      }
      if (peekChar(cpos) == 'f') {
        if (str) (*str) += '.';
        ++cpos;
      }
      return TK_FloatLiteral;
    }
    return TK_IntLiteral;
  }

  // identifier?
  if (isAlpha(ch) || ch >= 128 || ch == '_') {
    // find identifier end
    int spos = cpos;
    for (;;) {
      ch = peekChar(cpos);
      if (!ch) break;
      if (isAlpha(ch) || ch >= 128 || ch == '_' || (ch >= '0' && ch <= '9')) {
        if (str) (*str) += (char)ch;
        ++cpos;
        continue;
      }
      break;
    }
    // check for synonyms
    if (isNStrEqu(spos, cpos, "NULL")) return TK_Null;
    if (isNStrEqu(spos, cpos, "null")) return TK_Null;
    // look in tokens
    for (unsigned f = TK_Abstract; f < TK_URShiftAssign; ++f) {
      if (isNStrEqu(spos, cpos, TokenNames[f])) return (EToken)f;
    }
    return TK_Identifier;
  }

  // now collect the longest punctuation
  EToken tkres = TK_NoToken;
  int spos = cpos;
  //fprintf(stderr, " delimstart=%d; ch=%c\n", cpos, ch);
  for (;;) {
    // look in tokens
    bool found = false;
    for (unsigned f = TK_URShiftAssign; f < TK_TotalTokenCount; ++f) {
      if (isNStrEqu(spos, cpos+1, TokenNames[f])) {
        tkres = (EToken)f;
        found = true;
        //fprintf(stderr, "  delimend=%d; tk=<%s>\n", cpos+1, TokenNames[f]);
        break;
      }
    }
    if (!found) {
      if (str) {
        while (spos < cpos) (*str) += (char)src->FileStart[spos++];
      }
      return tkres;
    }
    ++cpos;
  }
}


//==========================================================================
//
// VLexer::peekTokenType
//
// this is freakin' slow, and won't cross "include" boundaries
// offset==0 means "current token"
// this doesn't process conditional directives,
// so it is useful only for limited lookups
//
//==========================================================================
EToken VLexer::peekTokenType (int offset, VStr *tkstr) const {
  if (tkstr) tkstr->clear();
  if (offset < 0) return TK_NoToken;
  if (offset == 0) return Token;
  if (src->FilePtr >= src->FileEnd) return TK_NoToken; // no more
  EToken tkres = TK_NoToken;
  int cpos = (int)(src->FilePtr-src->FileStart)-1; // current char is eaten
  //fprintf(stderr, "cpos=%d\n", cpos);
  while (offset-- > 0) {
    tkres = skipTokenFrom(cpos, (offset == 0 ? tkstr : nullptr));
    //fprintf(stderr, "  cpos=%d; <%s>\n", cpos, TokenNames[tkres]);
    if (tkres == TK_NoToken) break; // EOS or some error
  }
  return tkres;
}
