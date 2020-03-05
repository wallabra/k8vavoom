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
//**  Copyright (C) 2018-2020 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, version 3 of the License ONLY.
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


//==========================================================================
//
//  Sys_Quit
//
//  Shuts down net game, saves defaults, prints the exit text message,
//  goes to text mode, and exits.
//
//==========================================================================
void Sys_Quit (const char *msg) {
  //if (ttyIsGood()) ttySetRawMode(false);
  Host_Shutdown();
  Z_Exit(0);
}


//==========================================================================
//
//  Sys_Shutdown
//
//==========================================================================
void Sys_Shutdown () {
  /*
  if (ttyIsGood()) {
    ttySetRawMode(false);
    ttyRawWrite("\r\x1b[0m\x1b[K");
  }
  */
}


extern bool ttyRefreshInputLine;
extern bool ttyExtraDisabled;

static char text[8192];
static char text2[8192];
static int textpos = 0;


//==========================================================================
//
//  UpdateTTYText
//
//==========================================================================
static void UpdateTTYText () {
  if (!ttyRefreshInputLine) return;
  ttyRefreshInputLine = false;
  vassert(textpos < (int)ARRAY_COUNT(text));
  text[textpos] = 0;
  int wdt = ttyGetWidth();
  if (wdt < 3) return; // just in case
  char ssr[64];
  snprintf(ssr, sizeof(ssr), "\x1b[%d;1H\x1b[44m\x1b[37;1m>", ttyGetHeight());
  int stpos = textpos-(wdt-2);
  if (stpos < 0) stpos = 0;
  ttyRawWrite(ssr);
  ttyRawWrite(text+stpos);
  ttyRawWrite("\x1b[K"); // clear line
}


//==========================================================================
//
//  PutToTTYText
//
//==========================================================================
static void PutToTTYText (const char *s) {
  if (!s || !s[0]) return;
  ttyRefreshInputLine = true;
  while (*s) {
    if (textpos >= (int)ARRAY_COUNT(text)-3) {
      ttyBeep();
      return;
    }
    text[textpos++] = *s++;
  }
}


//==========================================================================
//
//  Sys_ConsoleInput
//
//==========================================================================
char *Sys_ConsoleInput () {
#ifdef _WIN32
  return nullptr;
#else
  if (!ttyIsAvailable()) return nullptr;
  if (ttyExtraDisabled || !ttyIsGood()) {
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

  UpdateTTYText();

  TTYEvent evt = ttyReadKey(0);
  if (evt.type <= TTYEvent::Type::Unknown) return nullptr;
  // C-smth
  if (evt.type == TTYEvent::Type::ModChar) {
    if (evt.ch == 'C') Sys_Quit("*** ABORTED ***");
    if (evt.ch == 'Y') { textpos = 0; ttyRefreshInputLine = true; UpdateTTYText(); return nullptr; }
  } else if (evt.type == TTYEvent::Type::Enter) {
    if (textpos == 0) return nullptr;
    strcpy(text2, text);
    textpos = 0;
    text[0] = 0;
    ttyRefreshInputLine = true;
    UpdateTTYText();
    GCon->Logf(">%s", text2);
    return text2;
  }

  if (evt.type == TTYEvent::Type::Backspace) {
    if (textpos == 0) return nullptr;
    text[--textpos] = 0;
    ttyRefreshInputLine = true;
  } else if (evt.type == TTYEvent::Type::Tab) {
    // autocompletion
    if (textpos == 0) return nullptr;
    //TODO: autocompletion with moved cursor
    const int curpos = textpos;
    VStr clineRest(text); // after cursor
    VStr cline = clineRest.left(curpos);
    clineRest.chopLeft(curpos);
    if (cline.length() && clineRest.length() && clineRest[0] == '"') {
      cline += clineRest[0];
      clineRest.chopLeft(1);
    }
    // find last command
    int cmdstart = cline.findNextCommand();
    for (;;) {
      const int cmdstnext = cline.findNextCommand(cmdstart);
      if (cmdstnext == cmdstart) break;
      cmdstart = cmdstnext;
    }
    VStr oldpfx = cline;
    oldpfx.chopLeft(cmdstart); // remove completed commands
    VStr newpfx = VCommand::GetAutoComplete(oldpfx);
    if (oldpfx != newpfx) {
      textpos = 0;
      PutToTTYText(*cline.left(cmdstart));
      PutToTTYText(*newpfx);
      // append rest of cline
      if (clineRest.length()) {
        //int cpos = textpos;
        PutToTTYText(*clineRest);
        //c_iline.setCurPos(cpos);
      }
    }
  } else if (evt.type == TTYEvent::Type::Char) {
    if (evt.ch >= ' ' && evt.ch < 127) {
      char tmp[2];
      tmp[0] = (char)evt.ch;
      tmp[1] = 0;
      PutToTTYText(tmp);
    }
  }

  UpdateTTYText();
  return nullptr;
#endif
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
  VObject::vmAbortBySignal += 1;
  // Ignore future instances of this signal.
  signal(s, SIG_IGN);

  //  Exit with error message
#ifdef __linux__
  switch (s) {
    case SIGABRT: VSigContextHack::ErrToThrow = "Aborted"; break;
    case SIGFPE: VSigContextHack::ErrToThrow = "Floating Point Exception"; break;
    case SIGILL: VSigContextHack::ErrToThrow = "Illegal Instruction"; break;
    case SIGSEGV: VSigContextHack::ErrToThrow = "Segmentation Violation"; break;
    case SIGTERM: VSigContextHack::ErrToThrow = "Terminated"; break;
    case SIGINT: VSigContextHack::ErrToThrow = "Interrupted by User"; break;
    case SIGKILL: VSigContextHack::ErrToThrow = "Killed"; break;
    case SIGQUIT: VSigContextHack::ErrToThrow = "Quited"; break;
    default: VSigContextHack::ErrToThrow = "Terminated by signal";
  }
  longjmp(VSigContextHack::Env, 1);
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
  VObject::vmAbortBySignal += 1;
}

#endif


#ifndef _WIN32
extern "C" {
  static void restoreTTYOnExit (void) {
    if (ttyIsGood()) {
      ttySetRawMode(false);
      //ttyRawWrite("\x1b[9999F\x1b[0m\x1b[K");
      ttyRawWrite("\r\x1b[0m\x1b[K");
    }
  }
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
    //printf("k8vavoom dedicated server " VERSION_TEXT "\n");

    VObject::StaticInitOptions(GParsedArgs);
    FL_InitOptions();
    GArgs.Init(argc, argv, "-file");
    FL_CollectPreinits();
    GParsedArgs.parse(GArgs);

    if (GArgs.CheckParm("-gdb")) ttyExtraDisabled = true;

    #ifndef _WIN32
    if (!ttyExtraDisabled && ttyIsGood()) {
      ttySetRawMode(true);
      atexit(&restoreTTYOnExit);
      //ttyRawWrite("\x1b[0m;\x1b[2J\x1b[9999F"); // clear, move cursor down
      UpdateTTYText();
    }
    #endif

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
    devprintf("\n\nERROR: %s\n", e.message);
    fprintf(stderr, "\n%s\n", e.message);
    Z_Exit(1);
  } catch (...) {
    Host_Shutdown();
    devprintf("\n\nExiting due to external exception\n");
    fprintf(stderr, "\nExiting due to external exception\n");
    throw;
  }

  Z_ShuttingDown();
  return 0;
}
