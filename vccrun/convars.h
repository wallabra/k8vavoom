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
#ifndef VCCRUN_CONVARS_HEADER_H
#define VCCRUN_CONVARS_HEADER_H

#include "../libs/core/core.h"
#include <stdarg.h>


// ////////////////////////////////////////////////////////////////////////// //
// WARNING! thread-unsafe!

void ccmdClearText (); // clear command buffer
void ccmdClearCommand (); // clear current command (only)

// parse one command
enum CCResult {
  CCMD_EMPTY = -1, // no more commands (nothing was parsed)
  CCMD_NORMAL = 0, // one command parsed, line is not complete
  CCMD_EOL = 1, // no command parsed, line is complete
};

// this skips empty lines without notice
CCResult ccmdParseOne ();

int ccmdGetArgc (); // 0: nothing was parsed
const VStr &ccmdGetArgv (int idx); // 0: command; other: args, parsed and unquoted

// return number of unparsed bytes left
int ccmdTextSize ();


void ccmdPrepend (const char *str);
void ccmdPrepend (const VStr &str);
void ccmdPrependf (const char *fmt, ...) __attribute__((format(printf,1,2)));

void ccmdPrependQuoted (const char *str);
void ccmdPrependQuoted (const VStr &str);
void ccmdPrependQuotdedf (const char *fmt, ...) __attribute__((format(printf,1,2)));

void ccmdAppend (const char *str);
void ccmdAppend (const VStr &str);
void ccmdAppendf (const char *fmt, ...) __attribute__((format(printf,1,2)));

void ccmdAppendQuoted (const char *str);
void ccmdAppendQuoted (const VStr &str);
void ccmdAppendQuotedf (const char *fmt, ...) __attribute__((format(printf,1,2)));


// ////////////////////////////////////////////////////////////////////////// //
void conPutChar (char ch);

void conWriteStr (const VStr &str);
void conWriteStr (const char *str, size_t strlen);


#endif
