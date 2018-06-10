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

#include <signal.h>
#include <fcntl.h>
#define ftime fucked_ftime
#include <io.h>
#undef ftime
#include <direct.h>
#include <conio.h>
#include <sys/timeb.h>
#include <sys/stat.h>
#include "winlocal.h"
#include "gamedefs.h"

//#define R_OK  (4)


//==========================================================================
//
//  Sys_Shutdown
//
//==========================================================================
void Sys_Shutdown () {
}


//==========================================================================
//
// Sys_Quit
//
// Shuts down net game, saves defaults, prints the exit text message,
// goes to text mode, and exits.
//
//==========================================================================
void Sys_Quit (const char*) {
  // shutdown system
  Host_Shutdown();
  // exit
  exit(0);
}


//==========================================================================
//
//  signal_handler
//
//  Shuts down system, on error signal
//
//==========================================================================
void signal_handler (int s) {
  signal(s, SIG_IGN); // ignore future instances of this signal
  switch (s) {
    case SIGINT: throw VavoomError("Interrupted by User");
    case SIGILL: throw VavoomError("Illegal Instruction");
    case SIGFPE: throw VavoomError("Floating Point Exception");
    case SIGSEGV: throw VavoomError("Segmentation Violation");
    case SIGTERM: throw VavoomError("Software termination signal from kill");
    case SIGBREAK: throw VavoomError("Ctrl-Break sequence");
    case SIGABRT: throw VavoomError("Abnormal termination triggered by abort call");
    default: throw VavoomError("Terminated by signal");
  }
}


//==========================================================================
//
//  Sys_Time
//
//==========================================================================
/*
double Sys_Time() {
  double t;
  struct timeb tstruct;
  static int  starttime;

  ftime(&tstruct);

  if (!starttime) starttime = tstruct.time;
  t = (tstruct.time-starttime)+tstruct.millitm*0.001;

  return t;
}
*/


//==========================================================================
//
//  Sys_ConsoleInput
//
//==========================================================================
char *Sys_ConsoleInput () {
  static char text[256];
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
}


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

    //Install signal handler
    signal(SIGINT,  signal_handler);
    signal(SIGILL,  signal_handler);
    signal(SIGFPE,  signal_handler);
    signal(SIGSEGV, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGBREAK,signal_handler);
    signal(SIGABRT, signal_handler);

    Host_Init();
    for (;;) Host_Frame();
  } catch (VavoomError &e) {
    Host_Shutdown();
    dprintf("\n\nERROR: %s\n", e.message);
    fprintf(stderr, "%s\n", e.message);
    return 1;
  } catch (...) {
    Host_Shutdown();
    dprintf("\n\nExiting due to external exception\n");
    fprintf(stderr, "\nExiting due to external exception\n");
    throw;
  }
}
