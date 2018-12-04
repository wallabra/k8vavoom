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
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
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
#include "core.h"

#if defined(WIN32)
# include <windows.h>
#endif


#ifdef USE_GUARD_SIGNAL_CONTEXT
jmp_buf __Context::Env;
const char *__Context::ErrToThrow;
#endif

static char *host_error_string;

// call `abort()` or `exit()` there to stop standard processing
void (*SysErrorCB) (const char *msg) = nullptr;


//==========================================================================
//
//  VavoomError::VavoomError
//
//==========================================================================
VavoomError::VavoomError (const char *text) {
  VStr::NCpy(message, text, MAX_ERROR_TEXT_SIZE-1);
  message[MAX_ERROR_TEXT_SIZE-1] = 0;
}


//==========================================================================
//
//  VavoomError::What
//
//==========================================================================
const char *VavoomError::What () const {
  return message;
}


//==========================================================================
//
//  Host_CoreDump
//
//==========================================================================
void Host_CoreDump (const char *fmt, ...) {
  bool first = false;

  if (!host_error_string) {
    host_error_string = new char[32];
    VStr::Cpy(host_error_string, "Stack trace: ");
    first = true;
  }

  va_list argptr;
  static char string[1024]; //WARNING! not thread-safe!

  va_start(argptr, fmt);
  vsnprintf(string, sizeof(string), fmt, argptr);
  va_end(argptr);

  GLog.WriteLine("- %s", string);

  char *new_string = new char[VStr::Length(host_error_string)+VStr::Length(string)+6];
  VStr::Cpy(new_string, host_error_string);
  if (first) first = false; else strcat(new_string, " <- ");
  strcat(new_string, string);
  delete[] host_error_string;
  host_error_string = nullptr;
  host_error_string = new_string;
}


//==========================================================================
//
//  Host_GetCoreDump
//
//==========================================================================
const char *Host_GetCoreDump () {
  return (host_error_string ? host_error_string : "");
}


//==========================================================================
//
//  Sys_Error
//
//  Exits game and displays error message.
//
//==========================================================================
void Sys_Error (const char *error, ...) {
  va_list argptr;
  static char buf[16384]; //WARNING! not thread-safe!

  va_start(argptr,error);
  vsnprintf(buf, sizeof(buf), error, argptr);
  va_end(argptr);

  if (SysErrorCB) SysErrorCB(buf);

#if defined(WIN32)
  MessageBox(NULL, buf, "VaVoom Fatal Error", MB_OK);
/*
#else //if defined(VCC_STANDALONE_EXECUTOR)
  fputs("FATAL: ", stderr);
  fputs(buf, stderr);
  fputc('\n', stderr);
*/
#endif
  GLog.WriteLine("Sys_Error: %s", buf);
  //throw VavoomError(buf);
  abort(); // abort here, so we can drop back to gdb
}
