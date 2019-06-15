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
//**
//**  System driver for DOS, LINUX and UNIX dedicated servers.
//**
//**************************************************************************
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef _WIN32
# define ftime fucked_ftime
# include <io.h>
# undef ftime
# include <conio.h>
# include <sys/timeb.h>
# include <windows.h>
#endif
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#if defined(USE_FPU_MATH)
# define VAVOOM_ALLOW_FPU_DEBUG
#elif defined(__linux__)
# if defined(__x86_64__) || defined(__i386__)
#  define VAVOOM_ALLOW_FPU_DEBUG
# endif
#endif

#ifdef VAVOOM_ALLOW_FPU_DEBUG
# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif
# include <fenv.h>
#endif

#include "gamedefs.h"


//==========================================================================
//
//  Sys_ConsoleInput
//
//==========================================================================
char *Sys_ConsoleInput () {
  static char text[256];
#ifndef _WIN32
  int len;
  fd_set fdset;
  struct timeval timeout;

  FD_ZERO(&fdset);
  FD_SET(0, &fdset); // stdin
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  if (select(1, &fdset, nullptr, nullptr, &timeout) == -1 || !FD_ISSET(0, &fdset)) return nullptr;

  len = read(0, text, sizeof(text));
  if (len < 1) return nullptr;
  text[len-1] = 0; // rip off the /n and terminate

  return text;
#else
  static int len;
  int c;

  // read a line out
  while (kbhit()) {
    c = getch();
    putch(c);
    if (c == '\r') {
      text[len] = 0;
      putch('\n');
      len = 0;
      return text;
    }
    if (c == 8) {
      if (len) {
        putch(' ');
        putch(c);
        --len;
        text[len] = 0;
      }
      continue;
    }
    text[len] = c;
    ++len;
    text[len] = 0;
    if (len == sizeof(text)) len = 0;
  }

  return nullptr;
#endif
}


//==========================================================================
//
//  Sys_Quit
//
//  Shuts down net game, saves defaults, prints the exit text message,
//  goes to text mode, and exits.
//
//==========================================================================
void Sys_Quit (const char *msg) {
  Host_Shutdown();
  exit(0);
}


//==========================================================================
//
//  Sys_Shutdown
//
//==========================================================================
void Sys_Shutdown () {
}


//==========================================================================
//
//  signal_handler
//
//  Shuts down system, on error signal
//
//==========================================================================

#ifdef USE_SIGNAL_HANDLER

static void signal_handler (int s) {
  // Ignore future instances of this signal.
  signal(s, SIG_IGN);

  //  Exit with error message
#ifdef __linux__
  switch (s) {
    case SIGABRT: __Context::ErrToThrow = "Aborted"; break;
    case SIGFPE: __Context::ErrToThrow = "Floating Point Exception"; break;
    case SIGILL: __Context::ErrToThrow = "Illegal Instruction"; break;
    case SIGSEGV: __Context::ErrToThrow = "Segmentation Violation"; break;
    case SIGTERM: __Context::ErrToThrow = "Terminated"; break;
    case SIGINT: __Context::ErrToThrow = "Interrupted by User"; break;
    case SIGKILL: __Context::ErrToThrow = "Killed"; break;
    case SIGQUIT: __Context::ErrToThrow = "Quited"; break;
    default: __Context::ErrToThrow = "Terminated by signal";
  }
  longjmp(__Context::Env, 1);
#else
  switch (s) {
    case SIGABRT: throw VavoomError("Abnormal termination triggered by abort call");
    case SIGFPE: throw VavoomError("Floating Point Exception");
    case SIGILL: throw VavoomError("Illegal Instruction");
    case SIGINT: throw VavoomError("Interrupted by User");
    case SIGSEGV: throw VavoomError("Segmentation Violation");
    case SIGTERM: throw VavoomError("Software termination signal from kill");
#ifdef SIGKILL
    case SIGKILL: throw VavoomError("Killed");
#endif
#ifdef SIGQUIT
    case SIGQUIT: throw VavoomError("Quited");
#endif
#ifdef SIGNOFP
    case SIGNOFP: throw VavoomError("k8vavoom requires a floating-point processor");
#endif
    default: throw VavoomError("Terminated by signal");
  }
#endif
}

#else

static volatile int sigReceived = 0;

static void signal_handler (int s) {
  sigReceived = 1;
}

#endif


//==========================================================================
//
//  main
//
//  Main program
//
//==========================================================================
int main (int argc, char **argv) {
  try {
    printf("k8vavoom dedicated server " VERSION_TEXT "\n");

    FL_InitOptions();
    GArgs.Init(argc, argv, "-file");

    FL_CollectPreinits();

#ifdef USE_SIGNAL_HANDLER
    // install signal handlers
    signal(SIGABRT, signal_handler);
    signal(SIGFPE,  signal_handler);
    signal(SIGILL,  signal_handler);
    signal(SIGSEGV, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGINT,  signal_handler);
#ifdef SIGKILL
    signal(SIGKILL, signal_handler);
#endif
#ifdef SIGQUIT
    signal(SIGQUIT, signal_handler);
#endif
#ifdef SIGNOFP
    signal(SIGNOFP, signal_handler);
#endif

#else
# ifndef _WIN32
    // install signal handlers
    signal(SIGTERM, signal_handler);
    signal(SIGINT,  signal_handler);
    signal(SIGQUIT, signal_handler);
# else
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGBREAK,signal_handler);
    signal(SIGABRT, signal_handler);
# endif
#endif

#ifdef VAVOOM_ALLOW_FPU_DEBUG
    if (GArgs.CheckParm("-dev-fpu-alltraps") || GArgs.CheckParm("-dev-fpu-all-traps")) {
      feenableexcept(FE_DIVBYZERO|FE_INVALID|FE_OVERFLOW|FE_UNDERFLOW);
    } else if (GArgs.CheckParm("-dev-fpu-traps")) {
      feenableexcept(FE_DIVBYZERO|FE_INVALID);
    } else {
      //GCon->Logf("ROUND: %d (%d); EXCEPT: %d", fegetround(), FLT_ROUNDS, fegetexcept());
      feclearexcept(FE_ALL_EXCEPT);
    }
    // sse math can only round towards zero, so force it for FPU
    if (fesetround(0) != 0) GCon->Logf(NAME_Warning, "Cannot set float rounding mode (this is not fatal)");
#endif

    // initialise
    Host_Init();

    // play game
    for (;;) {
      Host_Frame();
      if (sigReceived) {
        GCon->Logf("*** SIGNAL RECEIVED ***");
        Host_Shutdown();
        fprintf(stderr, "*** TERMINATED BY SIGNAL ***\n");
        break;
      }
    }
  } catch (VavoomError &e) {
    Host_Shutdown();
    dprintf("\n\nERROR: %s\n", e.message);
    fprintf(stderr, "\n%s\n", e.message);
    exit(1);
  } catch (...) {
    Host_Shutdown();
    dprintf("\n\nExiting due to external exception\n");
    fprintf(stderr, "\nExiting due to external exception\n");
    throw;
  }

  return 0;
}
