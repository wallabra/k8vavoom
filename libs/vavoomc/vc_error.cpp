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
#include "vc_public.h"


// ////////////////////////////////////////////////////////////////////////// //
int vcErrorCount = 0;
int vcGagErrorCount = 0;
int vcGagErrors = 0; // !0: errors are gagged
bool vcErrorIncludeCol = true;
int vcWarningsSilenced = 0;


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


// so we can show them again before bailing out
static TArray<VStr> vcParseErrors;


//==========================================================================
//
//  BailOut
//
//==========================================================================
__attribute__((noreturn)) void BailOut () {
  if (!VObject::standaloneExecutor && vcParseErrors.length()) {
    GLog.Log(NAME_Error, "");
    GLog.Log(NAME_Error, "Let me show you all the errors again...");
    GLog.Logf(NAME_Error, "%s", "=============================");
    for (auto &&s : vcParseErrors) GLog.Logf(NAME_Error, "%s", *s);
    GLog.Logf(NAME_Error, "%s", "=============================");
  }
  Sys_Error("Confused by previous errors, bailing out");
}


//==========================================================================
//
//  ParseWarning
//
//==========================================================================
__attribute__((format(printf, 2, 3))) void ParseWarning (const TLocation &l, const char *text, ...) {
  if (vcGagErrors || vcWarningsSilenced) return;
  va_list argPtr;
  va_start(argPtr, text);
  const char *buf = vavarg(text, argPtr);
  va_end(argPtr);
  GLog.Logf(NAME_Warning, "%s: warning: %s", *(vcErrorIncludeCol ? l.toString(): l.toStringNoCol()), buf);
}


//==========================================================================
//
//  ParseWarningAsError
//
//==========================================================================
__attribute__((format(printf, 2, 3))) void ParseWarningAsError (const TLocation &l, const char *text, ...) {
  if (vcGagErrors || vcWarningsSilenced) return;
  va_list argPtr;
  va_start(argPtr, text);
  const char *buf = vavarg(text, argPtr);
  va_end(argPtr);
  GLog.Logf(NAME_Error, "%s: shit! %s", *(vcErrorIncludeCol ? l.toString(): l.toStringNoCol()), buf);
}


//==========================================================================
//
//  ParseError
//
//==========================================================================
__attribute__((format(printf, 2, 3))) void ParseError (const TLocation &l, const char *text, ...) {
  if (vcGagErrors) { ++vcGagErrorCount; return; }

  ++vcErrorCount;

  va_list argPtr;
  va_start(argPtr, text);
  const char *buf = vavarg(text, argPtr);
  va_end(argPtr);

  VStr err = va("%s: %s", *(vcErrorIncludeCol ? l.toString(): l.toStringNoCol()), buf);
  vcParseErrors.append(err);

  GLog.Logf(NAME_Error, "%s", *err);

  if (vcErrorCount >= 128) BailOut();
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
    va_list argPtr;
    va_start(argPtr, text);
    const char *buf = vavarg(text, argPtr);
    va_end(argPtr);
    //ParseError(l, "Error #%d - %s, %s", error, ErrorNames[error], Buffer);
    ParseError(l, "%s, %s", ErrorNames[error], buf);
  } else {
    ParseError(l, "%s", ErrorNames[error]);
  }
}


//==========================================================================
//
//  VCFatalError
//
//==========================================================================
__attribute__((noreturn, format(printf, 1, 2))) void VCFatalError (const char *text, ...) {
  va_list argPtr;

  va_start(argPtr, text);
  const char *buf = vavarg(text, argPtr);
  va_end(argPtr);

  Sys_Error("%s", buf);
}
