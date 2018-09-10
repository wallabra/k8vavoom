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
//**  Potentially Visible Set
//**
//**************************************************************************
#include <string.h>
#include <stdarg.h>
#include <time.h>

//  When compiling with -ansi isatty() is not declared
#if defined __unix__ && !defined __STRICT_ANSI__
# include <unistd.h>
#endif
#include "glvis.h"


// ////////////////////////////////////////////////////////////////////////// //
class TConsoleGLVis : public TGLVis {
public:
  time_t prevtime;

public:
  void startTime () { prevtime = 0; }

  virtual void DisplayMessage (const char *text, ...) override __attribute__((format(printf, 2, 3)));
  virtual void DisplayStartMap (const char *levelname) override;
  virtual void DisplayBaseVisProgress (int count, int total) override;
  virtual void DisplayPortalVisProgress (int count, int total) override;
  virtual void DisplayMapDone (int accepts, int total) override;
};


// ////////////////////////////////////////////////////////////////////////// //
static bool silent_mode = false;
static bool show_progress = true;

static TConsoleGLVis GLVis;

static const char progress_chars[] = {'|', '/', '-', '\\'};


// ////////////////////////////////////////////////////////////////////////// //
void TConsoleGLVis::DisplayMessage (const char *text, ...) {
  va_list args;
  if (!silent_mode) {
    va_start(args, text);
    vfprintf(stderr, text, args);
    va_end(args);
  }
}


void TConsoleGLVis::DisplayStartMap (const char *) {
  if (!silent_mode) {
    fprintf(stderr, "Creating vis data ... ");
  }
  prevtime = 0;
}


void TConsoleGLVis::DisplayBaseVisProgress (int count, int) {
  if (show_progress && !(count&0x1f)) {
    time_t ctt = time(NULL);
    if (ctt > prevtime) {
      prevtime = ctt;
      fprintf(stderr, "%c\b", progress_chars[(count>>5)&3]);
    }
  }
}


void TConsoleGLVis::DisplayPortalVisProgress (int count, int total) {
  if (show_progress && (!count || (total-count)%10 == 0)) {
    time_t ctt = time(NULL);
    if (ctt > prevtime) {
      prevtime = ctt;
      fprintf(stderr, "%05d\b\b\b\b\b", total-count);
    }
  }
}


void TConsoleGLVis::DisplayMapDone (int accepts, int total) {
  if (!silent_mode) {
    fprintf(stderr, "%d accepts, %d rejects, %d%%\n", accepts, total - accepts, accepts * 100 / total);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
static void ShowUsage () {
  fprintf(stderr, "\nGLVIS version 1.6, Copyright (c)2000-2006 Janis Legzdinsh (" __DATE__ " " __TIME__ ")\n");
  fprintf(stderr, "Usage: glvis [options] file[.wad]\n");
  fprintf(stderr, "    -s            silent mode\n");
  fprintf(stderr, "    -f            fast mode\n");
  fprintf(stderr, "    -v            verbose mode\n");
  fprintf(stderr, "    -t#           specify test level\n");
  fprintf(stderr, "    -m<LEVELNAME> specifies a level to process, can be used multiple times\n");
  fprintf(stderr, "    -x<LEVELNAME> specifies a level to exclude, can be used multiple times\n");
  fprintf(stderr, "    -r<LEVELNAME> specifies a level to apply \"inversed\" processing mode, can be used multiple times\n");
  fprintf(stderr, "    -noreject     don't create reject\n");
  exit(1);
}


// ////////////////////////////////////////////////////////////////////////// //
bool getXXArg (const char *arg) {
  if (!arg[2]) { fprintf(stderr, "FATAL: no argument for \"-%c\"\n", arg[1]); return false; }
  if (strlen(arg+2) > 15) { fprintf(stderr, "FATAL: too long argument for \"-%c\"\n", arg[1]); return false; }
  switch (arg[1]) {
    case 'm':
      if (GLVis.num_specified_maps >= GLVis.MapNameArraySize) { fprintf(stderr, "FATAL: too many arguments for \"-%c\"\n", arg[1]); return false; }
      strcpy(GLVis.specified_maps[GLVis.num_specified_maps++], arg+2);
      break;
    case 'x':
      if (GLVis.num_skip_maps >= GLVis.MapNameArraySize) { fprintf(stderr, "FATAL: too many arguments for \"-%c\"\n", arg[1]); return false; }
      strcpy(GLVis.skip_maps[GLVis.num_skip_maps++], arg+2);
      break;
    case 'r':
      if (GLVis.num_inv_mode_maps >= GLVis.MapNameArraySize) { fprintf(stderr, "FATAL: too many arguments for \"-%c\"\n", arg[1]); return false; }
      strcpy(GLVis.inv_mode_maps[GLVis.num_inv_mode_maps++], arg+2);
      break;
    default:
      break;
  }
  return true;
}


// ////////////////////////////////////////////////////////////////////////// //
int main (int argc, char *argv[]) {
  char *srcfile = NULL;
  bool noMoreArgs = false;

  if (argc == 1) ShowUsage();

  for (int argnum = 1; argnum < argc; ++argnum) {
    char *arg = argv[argnum];
    if (!noMoreArgs && *arg == '-') {
      if (strcmp(arg, "--") == 0) { noMoreArgs = true; continue; }
      switch (arg[1]) {
        case 'h': ShowUsage(); break;
        case 's': silent_mode = true; break;
        case 'f': GLVis.fastvis = true; break;
        case 'v': GLVis.verbose = true; break;
        case 't': GLVis.testlevel = arg[2]-'0'; break;
        case 'm': if (!getXXArg(arg)) exit(1); break;
        case 'x': if (!getXXArg(arg)) exit(1); break;
        case 'r': if (!getXXArg(arg)) exit(1); break;
        case 'n':
          if (strcmp(arg, "-noreject") == 0 || strcmp(arg, "--noreject") == 0) {
            GLVis.no_reject = true;
            break;
          }
          // fallthrough
        default:
          ShowUsage();
      }
    } else {
      if (srcfile) { fprintf(stderr, "FATAL: too many file names!\n"); exit(1); }
      srcfile = arg;
    }
  }

  if (!srcfile || !srcfile[0]) { fprintf(stderr, "FATAL: file name?\n"); exit(1); }

  show_progress = !silent_mode;
#if defined __unix__ && !defined __STRICT_ANSI__
  // Unix: no whirling baton if stderr is redirected
  if (!isatty(2)) show_progress = false;
#endif

  int starttime = (int)time(NULL);
  GLVis.startTime();
  try {
    GLVis.Build(srcfile);
  } catch (GLVisError &e) {
    fprintf(stderr, "FATAL: %s", e.message);
    return 1;
  }

  if (!silent_mode) {
    int worktime = int(time(0)) - starttime;
    fprintf(stderr, "Time elapsed: %d:%02d:%02d\n", worktime/3600, (worktime/60)%60, worktime%60);
  }

  return 0;
}
