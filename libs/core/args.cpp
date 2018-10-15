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

#define MAXARGVS  (512)


VArgs GArgs;


#ifndef _WIN32
# include <sys/types.h>
# include <unistd.h>
#else
# include <windows.h>
#endif

//==========================================================================
//
//  getBinaryDir
//
//==========================================================================
static char *getBinaryDir () {
  static char mydir[8192];
  memset(mydir, 0, sizeof(mydir));
#ifndef _WIN32
  char buf[128];
  pid_t pid = getpid();
  snprintf(buf, sizeof(buf), "/proc/%u/exe", (unsigned int)pid);
  if (readlink(buf, mydir, sizeof(mydir)-1) < 0) {
    mydir[0] = '.';
    mydir[1] = '\0';
  } else {
    char *p = (char *)strrchr(mydir, '/');
    if (!p) {
      mydir[0] = '.';
      mydir[1] = '\0';
    } else {
      *p = '\0';
    }
  }
#else
  char *p;
  GetModuleFileName(GetModuleHandle(NULL), mydir, sizeof(mydir)-1);
  p = strrchr(mydir, '\\');
  if (!p) strcpy(mydir, "."); else *p = '\0';
  for (p = mydir; *p; ++p) if (*p == '\\') *p = '/';
#endif
  return mydir;
}


//==========================================================================
//
//  xstrdup
//
//==========================================================================
static char *xstrdup (const char *s) {
  if (!s) s = "";
  char *res = (char *)Z_Malloc(strlen(s)+1);
  strcpy(res, s);
  return res;
}


//==========================================================================
//
//  VArgs::Init
//
//==========================================================================
void VArgs::Init (int argc, char **argv) {
  guard(VArgs::Init);
  // save args
  if (argc < 0) argc = 0; else if (argc > MAXARGVS) argc = MAXARGVS;
  Argc = argc;
  Argv = (char **)Z_Malloc(sizeof(char *)*MAXARGVS); // memleak, but nobody cares
  memset(Argv, 0, sizeof(char*)*MAXARGVS);
  Argv[0] = xstrdup(getBinaryDir());
  for (int f = 1; f < argc; ++f) Argv[f] = xstrdup(argv[f]);
  FindResponseFile();
  unguard;
}


//==========================================================================
//
// VArgs::FindResponseFile
//
// Find a Response File. We don't do this in DJGPP because it does this
// in startup code.
//
//==========================================================================
void VArgs::FindResponseFile () {
  for (int i = 1; i < Argc; ++i) {
    if (Argv[i][0] != '@') continue;

    // read the response file into memory
    FILE *handle = fopen(&Argv[i][1], "rb");
    if (!handle) {
      printf("\nNo such response file %s!", &Argv[i][1]);
      exit(1);
    }
    GLog.WriteLine("Found response file %s!", &Argv[i][1]);
    fseek(handle, 0, SEEK_END);
    int size = ftell(handle);
    fseek(handle, 0, SEEK_SET);
    char *file = (char *)Z_Malloc(size+1);
    fread(file, size, 1, handle);
    fclose(handle);
    file[size] = 0;

    // keep all other cmdline args
    char **oldargv = Argv; // memleak, but nobody cares

    Argv = (char **)Z_Malloc(sizeof(char *)*MAXARGVS);
    memset(Argv, 0, sizeof(char*)*MAXARGVS);

    // keep args before response file
    int indexinfile;
    for (indexinfile = 0; indexinfile < i; ++indexinfile) {
      Argv[indexinfile] = oldargv[indexinfile];
    }

    // read response file
    char *infile = file;
    int k = 0;
    while (k < size) {
      // skip whitespace.
      if (infile[k] <= ' ') {
        ++k;
        continue;
      }

      if (infile[k] == '\"') {
        // parse quoted string
        ++k;
        Argv[indexinfile++] = infile+k;
        char CurChar;
        char *OutBuf = infile+k;
        do {
          CurChar = infile[k];
          if (CurChar == '\\' && infile[k + 1] == '\"') {
            CurChar = '\"';
            ++k;
          } else if (CurChar == '\"') {
            CurChar = 0;
          } else if (CurChar == 0) {
            --k;
          }
          *OutBuf = CurChar;
          ++k;
          ++OutBuf;
        } while (CurChar);
        *(infile+k) = 0;
      } else {
        // parse unquoted string
        Argv[indexinfile++] = infile+k;
        while (k < size && infile[k] > ' ' && infile[k] != '\"') ++k;
        *(infile+k) = 0;
      }
    }

    // keep args following response file
    for (k = i+1; k < Argc; ++k) Argv[indexinfile++] = oldargv[k];
    Argc = indexinfile;

    // display args
    GLog.WriteLine("%d command-line args:", Argc);
    for (k = 1; k < Argc; ++k) GLog.WriteLine("%s", Argv[k]);
    --i;
  }
}


//==========================================================================
//
//  VArgs::CheckParm
//
//  Checks for the given parameter in the program's command line arguments.
//  Returns the argument number (1 to argc - 1) or 0 if not present
//
//==========================================================================
int VArgs::CheckParm (const char *check, bool takeFirst) const {
  guard(VArgs::CheckParm);
  if (takeFirst) {
    for (int i = 1; i < Argc; ++i) if (!VStr::ICmp(check, Argv[i])) return i;
  } else {
    for (int i = Argc-1; i > 0; --i) if (!VStr::ICmp(check, Argv[i])) return i;
  }
  return 0;
  unguard;
}


//==========================================================================
//
//  VArgs::CheckValue
//
//==========================================================================
const char *VArgs::CheckValue (const char *check, bool takeFirst) const {
  guard(VArgs::CheckValue);
  int a = CheckParm(check, takeFirst);
  if (a && a < Argc-1 && Argv[a+1][0] != '-' && Argv[a+1][0] != '+') return Argv[a+1];
  return nullptr;
  unguard;
}


//==========================================================================
//
//  VArgs::IsCommand
//
//==========================================================================
bool VArgs::IsCommand (int idx) const {
  if (idx < 1 || idx >= Argc) return false;
  return (Argv[idx][0] == '-' || Argv[idx][0] == '+');
}
