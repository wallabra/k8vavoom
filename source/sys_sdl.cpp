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
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <SDL.h>
#if !defined(_WIN32) && !defined(__SWITCH__)
//# include <execinfo.h>
#elif defined(__SWITCH__)
# include <switch.h>
#endif

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
//  Sys_Shutdown
//
//==========================================================================
void Sys_Shutdown () {
}


//==========================================================================
//
//  PutEndText
//
//  Function to write the Doom end message text
//
//  Copyright (C) 1998 by Udo Munk <udo@umserver.umnet.de>
//
//  This code is provided AS IS and there are no guarantees, none.
//  Feel free to share and modify.
//
//==========================================================================
static void PutEndText (const char *text) {
  int i, j;
  int att = -1;
  int nlflag = 0;
  char *col;

  // if option -noendtxt is set, don't print the text.
  if (GArgs.CheckParm("-noendtxt")) return;

  // if the xterm has more then 80 columns we need to add nl's
  col = getenv("COLUMNS");
  if (col) {
    if (VStr::atoi(col) > 80) ++nlflag;
  } else {
    ++nlflag;
  }

  // print 80x25 text and deal with the attributes too
  for (i = 1; i <= 80 * 25; ++i, text += 2) {
    // attribute first
    j = (vuint8)text[1];
    // attribute changed?
    if (j != att) {
      static const char map[] = "04261537";
      // save current attribute
      att = j;
      // set new attribute: bright, foreground, background (we don't have bright background)
      printf("\033[0;%s3%c;4%cm", (j & 0x88) ? "1;" : "", map[j & 7], map[(j & 0x70) >> 4]);
    }

    // now the text
    if (*text < 32) putchar('.'); else putchar(*text);

    // do we need a nl?
    if (nlflag && !(i % 80)) {
      att = 0;
      puts("\033[0m");
    }
  }

  // all attributes off
  printf("\033[0m");

  if (nlflag) printf("\n");
}


//==========================================================================
//
//  Sys_Quit
//
//  Shuts down net game, saves defaults, prints the exit text message,
// goes to text mode, and exits.
//
//==========================================================================
void Sys_Quit (const char *EndText) {
  // shutdown system
  Host_Shutdown();
  SDL_Quit();
  // throw the end text at the screen
  if (EndText) PutEndText(EndText);
  // exit
  exit(0);
}


//==========================================================================
//
//  Sys_Error
//
//  Exits game and displays error message.
//
//==========================================================================

#ifdef USE_SIGNAL_HANDLER

#define MAX_STACK_ADDR  (40)

// __builtin_return_address needs a constant, so this cannot be in a loop

#define handle_stack_address(X) \
  if (continue_stack_trace && ((unsigned long)__builtin_frame_address((X)) != 0L) && ((X) < MAX_STACK_ADDR)) \
  { \
    stack_addr[(X)]= __builtin_return_address((X)); \
    dprintf("stack %d %8p frame %d %8p\n", \
      (X), __builtin_return_address((X)), (X), __builtin_frame_address((X))); \
  } \
  else if (continue_stack_trace) \
  { \
    continue_stack_trace = false; \
  }

static void stack_trace () {
  FILE *fff;
  int i;
  static void *stack_addr[MAX_STACK_ADDR];
  // can we still print entries on the calling stack or have we finished?
  static bool continue_stack_trace = true;

  // get void*'s for all entries on the stack
  void *array[10];
  size_t size = backtrace(array, 10);

  // print out all the frames to stderr
  backtrace_symbols_fd(array, size, STDERR_FILENO);

  // clean the stack addresses if necessary
  for (i = 0; i < MAX_STACK_ADDR; ++i) stack_addr[i] = 0;

  dprintf("STACK TRACE:\n\n");

  handle_stack_address(0);
  handle_stack_address(1);
  handle_stack_address(2);
  handle_stack_address(3);
  handle_stack_address(4);
  handle_stack_address(5);
  handle_stack_address(6);
  handle_stack_address(7);
  handle_stack_address(8);
  handle_stack_address(9);
  handle_stack_address(10);
  handle_stack_address(11);
  handle_stack_address(12);
  handle_stack_address(13);
  handle_stack_address(14);
  handle_stack_address(15);
  handle_stack_address(16);
  handle_stack_address(17);
  handle_stack_address(18);
  handle_stack_address(19);
  handle_stack_address(20);
  handle_stack_address(21);
  handle_stack_address(22);
  handle_stack_address(23);
  handle_stack_address(24);
  handle_stack_address(25);
  handle_stack_address(26);
  handle_stack_address(27);
  handle_stack_address(28);
  handle_stack_address(29);
  handle_stack_address(30);
  handle_stack_address(31);
  handle_stack_address(32);
  handle_stack_address(33);
  handle_stack_address(34);
  handle_stack_address(35);
  handle_stack_address(36);
  handle_stack_address(37);
  handle_stack_address(38);
  handle_stack_address(39);

  // Give a warning
  //fprintf(stderr, "You suddenly see a gruesome SOFTWARE BUG leap for your throat!\n");

  // Open the non-existing file
  fff = fopen("crash.txt", "w");

  // Invalid file
  if (fff) {
    // dump stack frame
    for (i = (MAX_STACK_ADDR - 1); i >= 0 ; --i) {
      fprintf(fff,"%8p\n", stack_addr[i]);
    }
    fclose(fff);
  }
}

#endif


//==========================================================================
//
//  Sys_ConsoleInput
//
//==========================================================================
char *Sys_ConsoleInput ()
{
  static char text[256];
  int     len;
  fd_set  fdset;
  struct timeval timeout;

  FD_ZERO(&fdset);
  FD_SET(0, &fdset); // stdin
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  if (select(1, &fdset, nullptr, nullptr, &timeout) == -1 || !FD_ISSET(0, &fdset)) return nullptr;

  len = read(0, text, sizeof(text));
  if (len < 1) return nullptr;
  text[len-1] = 0;    // rip off the /n and terminate

  return text;
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
  // ignore future instances of this signal
  signal(s, SIG_IGN);
  stack_trace();

  // exit with error message
#ifdef USE_GUARD_SIGNAL_CONTEXT
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
  dprintf("signal: %s\n", __Context::ErrToThrow);
  longjmp(__Context::Env, 1);
#else
  switch (s) {
    case SIGABRT: throw VavoomError("Abnormal termination triggered by abort call");
    case SIGFPE: throw VavoomError("Floating Point Exception");
    case SIGILL: throw VavoomError("Illegal Instruction");
    case SIGINT: throw VavoomError("Interrupted by User");
    case SIGSEGV: throw VavoomError("Segmentation Violation");
    case SIGTERM: throw VavoomError("Software termination signal from kill");
    case SIGKILL: throw VavoomError("Killed");
    case SIGQUIT: throw VavoomError("Quited");
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
//  mainloop
//
//==========================================================================
static void mainloop (int argc, char **argv) {
  try {
    FL_InitOptions();
    GArgs.Init(argc, argv, "-file");

    FL_CollectPreinits();

    // if( SDL_InitSubSystem(SDL_INIT_VIDEO) < 0 )
    if (SDL_Init(SDL_INIT_VIDEO) < 0) Sys_Error("SDL_InitSubSystem(): %s\n",SDL_GetError());
    //SDL_WM_SetCaption("k8vavoom", "k8vavoom");

#ifdef USE_SIGNAL_HANDLER
    // install signal handlers
    signal(SIGABRT, signal_handler);
    signal(SIGFPE,  signal_handler);
    signal(SIGILL,  signal_handler);
    signal(SIGSEGV, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGINT,  signal_handler);
    signal(SIGKILL, signal_handler);
    signal(SIGQUIT, signal_handler);
#else
    // install signal handlers
    signal(SIGTERM, signal_handler);
    signal(SIGINT,  signal_handler);
# ifndef _WIN32
    signal(SIGQUIT, signal_handler);
# endif
#endif

/*
#if defined(USE_FPU_MATH)
    fprintf(stderr, "DEBUG: setting FPU trap flags\n");
    //feenableexcept(FE_ALL_EXCEPT/ *FE_DIVBYZERO|FE_INVALID|FE_OVERFLOW* /);
    //feenableexcept(FE_DIVBYZERO|FE_INVALID|FE_OVERFLOW);
    feenableexcept(FE_DIVBYZERO|FE_INVALID);
#endif
*/

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

    Host_Init();
    /*
    GCon->Logf("COLOR 'dark slate gray' is 0x%08x", M_ParseColor(" dark slate  gray "));
    GCon->Logf("COLOR '#222' is 0x%08x", M_ParseColor("#222"));
    GCon->Logf("COLOR '#1234ef' is 0x%08x", M_ParseColor("#1234ef"));
    GCon->Logf("COLOR '12 34 ef' is 0x%08x", M_ParseColor("12 34 ef"));
    GCon->Logf("COLOR '2 3 4' is 0x%08x", M_ParseColor("  2 3  4 "));
    GCon->Logf("COLOR '255 255 255' is 0x%08x", M_ParseColor("255 255 255"));
    */

#ifdef __SWITCH__
    while (appletMainLoop()) {
#else
    for (;;) {
#endif
      Host_Frame();
      if (sigReceived) {
        GCon->Logf("*** SIGNAL RECEIVED ***");
        Host_Shutdown();
        fprintf(stderr, "*** TERMINATED BY SIGNAL ***\n");
        break;
      }
    }
  } catch (VavoomError &e) {
    //printf("\n%s\n", e.message);
    //dprintf("\n\nERROR: %s\n", e.message);
    GCon->Logf("\n\nERROR: %s", e.message);
    Host_Shutdown();
    SDL_Quit();
    exit(1);
  }
}


//==========================================================================
//
//  main
//
//  Main program
//
//==========================================================================
int main (int argc, char **argv) {
#ifdef __SWITCH__
  socketInitializeDefault();
# ifdef SWITCH_NXLINK
  nxlinkStdio();
# endif
#endif

  bool inGDB = false;
  for (int f = 1; f < argc; ++f) {
    if (strcmp(argv[f], "-gdb") == 0) {
      inGDB = true;
      for (int c = f+1; c < argc; ++c) argv[c-1] = argv[c];
      --argc;
      --f;
    }
  }

  if (inGDB) {
    mainloop(argc, argv);
  } else {
    try {
      mainloop(argc, argv);
    } catch (...) {
      //dprintf("\n\nExiting due to external exception\n");
      //fprintf(stderr, "\nExiting due to external exception\n");
      GCon->Logf("\nExiting due to external exception");
      Host_Shutdown();
      throw;
    }
  }
#ifdef _WIN32
  return 0;
#elif defined(__SWITCH__)
  socketExit();
  return 0;
#endif
}
