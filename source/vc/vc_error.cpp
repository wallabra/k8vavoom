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

#include "vc_local.h"


// ////////////////////////////////////////////////////////////////////////// //
int vcErrorCount = 0;
int vcGagErrorCount = 0;
int vcGagErrors = 0; // !0: errors are gagged


static const char *ErrorNames[NUM_ERRORS] = {
  "No error.",
  //  File errors
  "Couldn't open file.",
  "Couldn't open debug file.",
  //  Tokenizer errors
  "Radix out of range in integer constant.",
  "String too long.",
  "End of file inside quoted string.",
  "New line inside quoted string.",
  "Unknown escape char.",
  "Identifier too long.",
  "Bad character.",
  //  Syntactic errors
  "Missing '('.",
  "Missing ')'.",
  "Missing '{'.",
  "Missing '}'.",
  "Missing colon.",
  "Missing semicolon.",
  "Unexpected end of file.",
  "Do statement not followed by 'while'.",
  "Invalid identifier.",
  "Function redeclared.",
  "Missing ']'.",
  "Invalid operation with array",
  "Expression type mismatch",
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
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  GCon->Logf("%s:%d: warning: %s", *l.GetSource(), l.GetLine(), Buffer);
#else
  fprintf(stderr, "%s:%d: warning: %s\n", *l.GetSource(), l.GetLine(), Buffer);
#endif
}


//==========================================================================
//
//  ParseError
//
//==========================================================================
__attribute__((format(printf, 2, 3))) void ParseError (const TLocation &l, const char *text, ...) {
  if (vcGagErrors) { ++vcGagErrors; return; }

  char Buffer[2048];
  va_list argPtr;

  ++vcErrorCount;

  va_start(argPtr, text);
  vsnprintf(Buffer, sizeof(Buffer), text, argPtr);
  va_end(argPtr);
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  GCon->Logf("%s:%d: %s", *l.GetSource(), l.GetLine(), Buffer);
#else
  fprintf(stderr, "%s:%d: %s\n", *l.GetSource(), l.GetLine(), Buffer);
#endif

  if (vcErrorCount >= 16) Sys_Error("Too many errors");
}


//==========================================================================
//
//  ParseError
//
//==========================================================================
void ParseError (const TLocation &l, ECompileError error) {
  if (vcGagErrors) { ++vcGagErrors; return; }
  ParseError(l, "Error #%d - %s", error, ErrorNames[error]);
}


//==========================================================================
//
//  ParseError
//
//==========================================================================
__attribute__((format(printf, 3, 4))) void ParseError (const TLocation &l, ECompileError error, const char *text, ...) {
  if (vcGagErrors) { ++vcGagErrors; return; }

  char Buffer[2048];
  va_list argPtr;

  va_start(argPtr, text);
  vsnprintf(Buffer, sizeof(Buffer), text, argPtr);
  va_end(argPtr);
  ParseError(l, "Error #%d - %s, %s", error, ErrorNames[error], Buffer);
}


//==========================================================================
//
//  BailOut
//
//==========================================================================
__attribute__((noreturn)) void BailOut () {
  Sys_Error("Confused by previous errors, bailing out\n");
}

#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)

 // nothing

#else

//==========================================================================
//
//  FatalError
//
//==========================================================================
__attribute__((noreturn, format(printf, 1, 2))) void FatalError (const char *text, ...) {
  char workString[256];
  va_list argPtr;

  va_start(argPtr, text);
  vsnprintf(workString, sizeof(workString), text, argPtr);
  va_end(argPtr);
  fputs(workString, stderr);
  fputc('\n', stderr);
  exit(1);
}

#endif
