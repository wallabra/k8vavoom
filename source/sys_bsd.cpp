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
//**
//**  System driver for DOS, LINUX and UNIX dedicated servers.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include "gamedefs.h"


//==========================================================================
//
//  Sys_ConsoleInput
//
//==========================================================================
char *Sys_ConsoleInput () {
  static char text[256];
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
  // Shutdown system
  Host_Shutdown();
  // Exit
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
    case SIGNOFP: throw VavoomError("VAVOOM requires a floating-point processor");
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
    printf("Vavoom dedicated server " VERSION_TEXT "\n");

    GArgs.Init(argc, argv);

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
    // install signal handlers
    signal(SIGTERM, signal_handler);
    signal(SIGINT,  signal_handler);
    signal(SIGQUIT, signal_handler);
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
}
