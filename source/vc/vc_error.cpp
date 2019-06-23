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
#define VC_PUBLIC_WANT_CORE
#include "vc_public.h"


// ////////////////////////////////////////////////////////////////////////// //
int vcErrorCount = 0;
int vcGagErrorCount = 0;
int vcGagErrors = 0; // !0: errors are gagged
bool vcErrorIncludeCol = true;


static const char *ErrorNames[NUM_ERRORS] = {
  "No error",
  //  File errors
  "Couldn't open file",
  "Couldn't open debug file",
  //  Tokenizer errors
  "Radix out of range in integer constant",
  "String too long",
  "End of file inside quoted string",
  "New line inside quoted string",
  "Unknown escape char",
  "Identifier too long",
  "Bad character",
  //  Syntactic errors
  "Missing '('",
  "Missing ')'",
  "Missing '{'",
  "Missing '}'",
  "Missing colon",
  "Missing semicolon",
  "Unexpected end of file",
  "Do statement not followed by 'while'",
  "Invalid identifier",
  "Function redeclared",
  "Missing ']'",
  "Invalid operation with array",
  "Expression type mismatch",
  "Missing comma",
};


//==========================================================================
//
//  ParseWarning
//
//==========================================================================
__attribute__((format(printf, 2, 3))) void ParseWarning (const TLocation &l, const char *text, ...) {
  if (vcGagErrors) return;

  char Buffer[2048];
  va_list argPtr;

  va_start(argPtr, text);
  vsnprintf(Buffer, sizeof(Buffer), text, argPtr);
  va_end(argPtr);
#if !defined(IN_VCC)
  GLog.Logf(NAME_Warning, "%s: warning: %s", *(vcErrorIncludeCol ? l.toString(): l.toStringNoCol()), Buffer);
#else
  fprintf(stderr, "%s: warning: %s\n", *(vcErrorIncludeCol ? l.toString(): l.toStringNoCol()), Buffer);
#endif
}


//==========================================================================
//
//  ParseError
//
//==========================================================================
__attribute__((format(printf, 2, 3))) void ParseError (const TLocation &l, const char *text, ...) {
  if (vcGagErrors) { ++vcGagErrorCount; return; }

  char Buffer[2048];
  va_list argPtr;

  ++vcErrorCount;

  va_start(argPtr, text);
  vsnprintf(Buffer, sizeof(Buffer), text, argPtr);
  va_end(argPtr);
#if !defined(IN_VCC)
  GLog.Logf(NAME_Error, "%s: %s", *(vcErrorIncludeCol ? l.toString(): l.toStringNoCol()), Buffer);
#else
  fprintf(stderr, "%s: %s\n", *(vcErrorIncludeCol ? l.toString(): l.toStringNoCol()), Buffer);
#endif

  if (vcErrorCount >= 16) Sys_Error("Too many errors");
}


//==========================================================================
//
//  ParseError
//
//==========================================================================
void ParseError (const TLocation &l, ECompileError error) {
  if (vcGagErrors) { ++vcGagErrorCount; return; }
  ParseError(l, "Error #%d - %s", error, ErrorNames[error]);
}


//==========================================================================
//
//  ParseError
//
//==========================================================================
__attribute__((format(printf, 3, 4))) void ParseError (const TLocation &l, ECompileError error, const char *text, ...) {
  if (vcGagErrors) { ++vcGagErrorCount; return; }

  if (text && text[0]) {
    char Buffer[2048];
    va_list argPtr;

    va_start(argPtr, text);
    vsnprintf(Buffer, sizeof(Buffer), text, argPtr);
    va_end(argPtr);
    //ParseError(l, "Error #%d - %s, %s", error, ErrorNames[error], Buffer);
    ParseError(l, "%s, %s", ErrorNames[error], Buffer);
  } else {
    ParseError(l, "%s", ErrorNames[error]);
  }
}


//==========================================================================
//
//  BailOut
//
//==========================================================================
__attribute__((noreturn)) void BailOut () {
  Sys_Error("Confused by previous errors, bailing out");
}

#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)

 // nothing

#else
#if defined(WIN32)
# include <windows.h>
#endif

//==========================================================================
//
//  FatalError
//
//==========================================================================
__attribute__((noreturn, format(printf, 1, 2))) void FatalError (const char *text, ...) {
  static char workString[1024];
  va_list argPtr;

  va_start(argPtr, text);
  vsnprintf(workString, sizeof(workString), text, argPtr);
  va_end(argPtr);

  Sys_Error("%s", workString);
}

#endif
