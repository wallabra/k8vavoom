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

#define DO_GUARD    0
#define DO_CHECK    1

#ifdef PARANOID
# warning "PARANOID MODE"
# define DO_GUARD_SLOW  1
# define DO_CHECK_SLOW  1
#endif


//==========================================================================
//
//  Exceptions
//
//==========================================================================
class VException : VInterface {
public:
  virtual const char *What() const = 0;
};


#define MAX_ERROR_TEXT_SIZE   (1024)

class VavoomError : public VException {
public:
  char message[MAX_ERROR_TEXT_SIZE];

  explicit VavoomError (const char *text);
  const char *What() const;
};


class RecoverableError : public VavoomError {
public:
  explicit RecoverableError (const char *text) : VavoomError(text) {}
};

class ZoneError : public VavoomError
{
public:
  explicit ZoneError(const char *text) : VavoomError(text) {}
};


//==========================================================================
//
//  Guard macros
//
//==========================================================================

// turn on usage of context in guard macros on platforms where it's not
// safe to throw an exception in signal handler
#ifdef USE_SIGNAL_HANDLER
# if DO_GUARD && defined(__linux__)
#  define USE_GUARD_SIGNAL_CONTEXT
# elif defined(USE_GUARD_SIGNAL_CONTEXT)
#  undef USE_GUARD_SIGNAL_CONTEXT
# endif
#elif defined(USE_GUARD_SIGNAL_CONTEXT)
# undef USE_GUARD_SIGNAL_CONTEXT
#endif


#ifdef USE_GUARD_SIGNAL_CONTEXT
#include <setjmp.h>
// stack control
class __Context {
public:
  __Context () { memcpy(&Last, &Env, sizeof(jmp_buf)); }
  ~__Context () { memcpy(&Env, &Last, sizeof(jmp_buf)); }
  static jmp_buf Env;
  static const char *ErrToThrow;

protected:
  jmp_buf Last;
};
#endif


/* #if defined(_DEBUG) || !DO_GUARD */


#if !DO_GUARD
# define guard(name)    {
# define unguard        }
# define unguardf(msg)  }
#elif defined(USE_GUARD_SIGNAL_CONTEXT)
# define guard(name)    { \
    static const char __FUNC_NAME__[] = #name; \
    __Context __LOCAL_CONTEXT__; try { if (setjmp(__Context::Env)) { \
    throw VavoomError(__Context::ErrToThrow); } else {
# define unguard        }} catch (RecoverableError &e) { throw e; } \
    catch (...) { Host_CoreDump(__FUNC_NAME__); throw; }}
# define unguardf(msg)  }} catch (RecoverableError &e) { throw e; } \
    catch (...) { Host_CoreDump(__FUNC_NAME__); Host_CoreDump msg; throw; }}
#else
# define guard(name)   {static const char __FUNC_NAME__[] = #name; try {
# define unguard     } catch (RecoverableError &e) { throw e; } \
    catch (...) { Host_CoreDump(__FUNC_NAME__); throw; }}
# define unguardf(msg) } catch (RecoverableError &e) { throw e; } \
    catch (...) { Host_CoreDump(__FUNC_NAME__); Host_CoreDump msg; throw; }}
#endif

#if !defined(_DEBUG) && DO_GUARD_SLOW
# define guardSlow(name)    guard(name)
# define unguardSlow        unguard
# define unguardfSlow(msg)  unguardf(msg)
#else
# define guardSlow(name)    {
# define unguardSlow        }
# define unguardfSlow(msg)  }
#endif


void Host_CoreDump (const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void Sys_Error (const char *, ...) __attribute__((noreturn, format(printf, 1, 2)));


//==========================================================================
//
//  Assertion macros
//
//==========================================================================

#if DO_CHECK
//# define check(e)  if (!(e)) throw VavoomError("Assertion failed: " #e)
//# define verify(e) if (!(e)) throw VavoomError("Assertion failed: " #e)
# define check(e)  if (!(e)) do { fprintf(stderr, "Assertion failed: %s\n", #e); *(int *)0 = 0; } while (0)
# define verify(e) if (!(e)) do { fprintf(stderr, "Assertion failed: %s\n", #e); *(int *)0 = 0; } while (0)
#else
#define check(e)
#define verify(e) (e)
#endif

#if DO_CHECK_SLOW
# define checkSlow(e)  check(e)
# define verifySlow(e) verify(e)
#else
# define checkSlow(e)
# define verifySlow(e) (e)
#endif
