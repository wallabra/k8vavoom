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
//  VArgs::GetBinaryDir
//
//==========================================================================
char *VArgs::GetBinaryDir () {
  static char mydir[8192];
  memset(mydir, 0, sizeof(mydir));
#ifdef __SWITCH__
  strncpy(mydir, "/switch/vavoom", sizeof(mydir)-1);
#elif !defined(_WIN32)
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
  char *res = (char *)::malloc(strlen(s)+1);
  strcpy(res, s);
  return res;
}


//==========================================================================
//
//  VArgs::Init
//
//==========================================================================
void VArgs::Init (int argc, char **argv, const char *filearg) {
  // save args
  if (argc < 0) argc = 0; else if (argc > MAXARGVS) argc = MAXARGVS;
  //Argc = argc;
  Argv = (char **)::malloc(sizeof(char *)*MAXARGVS); // memleak, but nobody cares
  memset(Argv, 0, sizeof(char*)*MAXARGVS);
  Argv[0] = xstrdup(GetBinaryDir());
  //if (Argc < 1) Argc = 1;
  Argc = 1;
  for (int f = 1; f < argc; ++f) {
    if (argv[f] && argv[f][0]) {
      if (Argc >= MAXARGVS-1) break;
      Argv[Argc++] = xstrdup(argv[f]);
    }
  }
#ifdef __SWITCH__
  // add static response file if it exists
  FILE *rf = fopen("/switch/vavoom/args.txt", "rb");
  if (rf) {
    fclose(rf);
    Argv[Argc++] = xstrdup("@/switch/vavoom/args.txt");
  }
#endif
  FindResponseFile();
  InsertFileArg(filearg);
}


//==========================================================================
//
//  VArgs::AddFileOption
//
//  "!1-game": '!' means "has args, and breaking" (number is argc)
//
//==========================================================================
void VArgs::AddFileOption (const char *optname) {
  if (!optname || !optname[0]) return;
  fopts = (char **)::realloc(fopts, (foptsCount+1)*sizeof(fopts[0]));
  fopts[foptsCount++] = xstrdup(optname);
}


//==========================================================================
//
//  VArgs::InsertArgAt
//
//==========================================================================
void VArgs::InsertArgAt (int idx, const char *arg) {
  if (Argc >= MAXARGVS) return;
  if (idx < 0) idx = 0;
  if (idx >= Argc) {
    Argv[Argc++] = xstrdup(arg);
    return;
  }
  for (int c = Argc-1; c >= idx; --c) Argv[c+1] = Argv[c];
  Argv[idx] = xstrdup(arg);
  ++Argc;
}


//==========================================================================
//
//  VArgs::InsertFileArg
//
//==========================================================================
void VArgs::InsertFileArg (const char *filearg) {
  if (!filearg || !filearg[0]) return;
  int pos = 1;
  bool inFile = false;
  while (pos < Argc) {
    const char *arg = Argv[pos++];
    // check for in-file options
    bool isOpt = false;
    for (int f = 0; f < foptsCount; ++f) {
      const char *fo = fopts[f];
      //fprintf(stderr, ":<%s>\n", fo);
      if (fo[0] == '!') {
        ++fo;
        int ac = 0;
        if (fo[0] >= '0' && fo[0] <= '9') { ac = fo[0]-'0'; ++fo; }
        if (!fo[0]) continue;
        if (strcmp(arg, fo) == 0) {
          //fprintf(stderr, "!! ac=%d; <%s>\n", ac, fo);
          isOpt = true;
          inFile = false;
          // skip args
          while (ac-- > 0 && pos < Argc) {
            arg = Argv[pos];
            if (arg[0] == '+' || arg[0] == '-') break;
            ++pos;
          }
          break;
        }
      } else {
        if (strcmp(arg, fo) == 0) { isOpt = true; break; }
      }
    }
    if (isOpt) continue;
    // file option?
    if (strcmp(arg, filearg) == 0) { inFile = true; continue; }
    // other options
    if (arg[0] == '-') { inFile = false; continue; }
    // console commands
    if (arg[0] == '+') {
      inFile = false;
      // skip console command
      while (pos < Argc) {
        arg = Argv[pos];
        if (arg[0] == '+' || arg[0] == '-') break;
        ++pos;
      }
      continue;
    }
    // non-option arg
    if (inFile) continue;
    // check if this is a disk file
    if (!Sys_FileExists(arg)) {
      //fprintf(stderr, "NO FILE: <%s>\n", arg);
      continue;
    }
    // disk file, insert file option (and check other plain options too, so we won't insert alot of fileopts)
    InsertArgAt(pos-1, filearg);
    ++pos; // skip filename
    inFile = true;
    while (pos < Argc) {
      arg = Argv[pos];
      if (arg[0] == '+' || arg[0] == '-') break;
      if (!Sys_FileExists(arg)) break;
      ++pos;
    }
  }
  //fprintf(stderr, "========\n"); for (int f = 0; f < Argc; ++f) fprintf(stderr, "  #%d: <%s>\n", f, Argv[f]);
}


//==========================================================================
//
//  VArgs::FindResponseFile
//
//  Find a Response File. We don't do this in DJGPP because it does this
//  in startup code.
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
    //GLog.WriteLine("Found response file %s!", &Argv[i][1]);
    fseek(handle, 0, SEEK_END);
    int size = ftell(handle);
    fseek(handle, 0, SEEK_SET);
    char *file = (char *)::malloc(size+1);
    fread(file, size, 1, handle);
    fclose(handle);
    file[size] = 0;

    // keep all other cmdline args
    char **oldargv = Argv; // memleak, but nobody cares

    Argv = (char **)::malloc(sizeof(char *)*MAXARGVS);
    memset(Argv, 0, sizeof(char *)*MAXARGVS);

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
        const char *argstartptr = infile+k;
        //Argv[indexinfile++] = xstrdup(infile+k);
        char CurChar;
        char *OutBuf = infile+k;
        do {
          CurChar = infile[k];
          if (CurChar == '\\' && (infile[k+1] == '\"' || infile[k+1] == '\'' || infile[k+1] == '\\')) {
            CurChar = infile[k+1];
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
        Argv[indexinfile++] = xstrdup(argstartptr);
      } else {
        // parse unquoted string
        //Argv[indexinfile++] = xstrdup(infile+k);
        const char *argstartptr = infile+k;
        while (k < size && infile[k] > ' ' && infile[k] != '\"') ++k;
        *(infile+k) = 0;
        Argv[indexinfile++] = xstrdup(argstartptr);
      }
    }

    // keep args following response file
    for (k = i+1; k < Argc; ++k) Argv[indexinfile++] = oldargv[k];
    Argc = indexinfile;

    // remove empty args
    for (int f = 1; f < Argc; ) {
      const char *arg = Argv[f];
      if (!arg[0]) {
        //memleak!
        for (int c = f+1; c < Argc; ++c) Argv[c-1] = Argv[c];
        --Argc;
      } else {
        ++f;
      }
    }

    // display args
    //GLog.WriteLine("%d command-line args:", Argc);
    //for (k = 1; k < Argc; ++k) GLog.WriteLine("%s", Argv[k]);
    --i;
  }
/*
#ifdef _WIN32
  fprintf(stderr, "%d command-line args:\n", Argc);
  for (int cc = 1; cc < Argc; ++cc) fprintf(stderr, "  %d: <%s>\n", cc, Argv[cc]);
#endif
*/
}


//==========================================================================
//
//  VArgs::CheckParm
//
//  Checks for the given parameter in the program's command line arguments.
//  Returns the argument number (1 to argc - 1) or 0 if not present
//
//==========================================================================
int VArgs::CheckParm (const char *check, bool takeFirst, bool startsWith) const {
  if (Argc <= 1) return 0;

  int i, end, dir;
  if (takeFirst) {
    i = 1;
    end = Argc;
    dir = 1;
  } else {
    i = Argc-1;
    end = 0;
    dir = -1;
  }

  while (i != end) {
    if (!startsWith) {
      if (VStr::strEquCI(Argv[i], check)) return i;
    } else {
      if (VStr::startsWithCI(Argv[i], check)) return i;
    }
    i += dir;
  }

  return 0;
}


//==========================================================================
//
//  VArgs::CheckParmFrom
//
//==========================================================================
int VArgs::CheckParmFrom (const char *check, int stidx, bool startsWith) const {
  if (stidx == 0) return 0;
  if (stidx < 0) stidx = 0;
  for (++stidx; stidx < Argc; ++stidx) {
    if (!startsWith) {
      if (VStr::strEquCI(Argv[stidx], check)) return stidx;
    } else {
      if (VStr::startsWithCI(Argv[stidx], check)) return stidx;
    }
  }
  return 0;
}


//==========================================================================
//
//  VArgs::CheckValue
//
//==========================================================================
const char *VArgs::CheckValue (const char *check, bool takeFirst, bool startsWith) const {
  int a = CheckParm(check, takeFirst, startsWith);
  //GLog.Logf(NAME_Debug, "VArgs::CheckValue: check=<%s>; takeFirst=%d; startsWith=%d; a=%d; <%s>", check, (int)takeFirst, (int)startsWith, a, Argv[a+1]);
  if (a && a+1 < Argc && Argv[a+1][0] != '-' && Argv[a+1][0] != '+') return Argv[a+1];
  return nullptr;
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


//==========================================================================
//
//  VArgs::removeAt
//
//==========================================================================
void VArgs::removeAt (int idx) {
  if (idx < 1 || idx >= Argc) return;
  //k8: nobody cares about memleaks here, so let's play safe
  //::free(Argv[idx]);
  for (int f = idx+1; f < Argc; ++f) Argv[f-1] = Argv[f];
  Argv[Argc-1] = nullptr;
  --Argc;
}



// ////////////////////////////////////////////////////////////////////////// //
VParsedArgs::ArgInfo *VParsedArgs::argInfoHead = nullptr;
VParsedArgs::ArgInfo *VParsedArgs::argInfoFileArg = nullptr;
VParsedArgs::ArgInfo *VParsedArgs::argInfoCmdArg = nullptr;

VParsedArgs GParsedArgs;


//==========================================================================
//
//  VParsedArgs::VParsedArgs
//
//==========================================================================
VParsedArgs::VParsedArgs () : mBinDir(nullptr) {}


//==========================================================================
//
//  VParsedArgs::clear
//
//==========================================================================
void VParsedArgs::clear () {
  if (mBinDir) { ::free(mBinDir); mBinDir = nullptr; }
}


//==========================================================================
//
//  VParsedArgs::allocArgInfo
//
//==========================================================================
VParsedArgs::ArgInfo *VParsedArgs::allocArgInfo (const char *argname, const char *shorthelp) {
  ArgInfo *res = (ArgInfo *)::malloc(sizeof(ArgInfo));
  memset((void *)res, 0, sizeof(ArgInfo));
  res->name = argname;
  res->help = shorthelp;
  res->flagptr = nullptr;
  res->strptr = nullptr;
  res->strarg = nullptr;
  res->type = AT_Ignore;
  res->isAlias = false;
  res->cb = nullptr;
  res->next = nullptr;
  // file argument handler?
  if (!argname) {
    if (argInfoFileArg) Sys_Error("duplicate handler for file arguments");
    argInfoFileArg = res;
    return res;
  }
  // command handler?
  if (argname[0] == '+' && !argname[1]) {
    if (argInfoCmdArg) Sys_Error("duplicate handler for command arguments");
    argInfoCmdArg = res;
    return res;
  }
  if (argname[0] != '-') Sys_Error("invalid CLI handler argument name '%s'", argname);
  // check for duplicates
  ArgInfo *prev = nullptr;
  ArgInfo *curr = argInfoHead;
  while (curr) {
    if (strcmp(curr->name, argname) == 0) Sys_Error("duplicate handler for '%s' CLI arg", argname);
    prev = curr;
    curr = curr->next;
  }
  if (prev) {
    // not first
    prev->next = res;
  } else {
    // first
    argInfoHead = res;
  }
  return res;
}


//==========================================================================
//
//  VParsedArgs::findNamedArgInfo
//
//==========================================================================
VParsedArgs::ArgInfo *VParsedArgs::findNamedArgInfo (const char *argname) {
  if (!argname) return argInfoFileArg;
  if (argname[0] == '+') return argInfoCmdArg;
  if (argname[0] != '-') return nullptr;
  for (ArgInfo *ai = argInfoHead; ai; ai = ai->next) {
    if (strcmp(argname, ai->name) == 0) return ai;
  }
  return nullptr;
}


//==========================================================================
//
//  VParsedArgs::RegisterAlias
//
//  `oldname` should be already registered
//
//==========================================================================
bool VParsedArgs::RegisterAlias (const char *argname, const char *oldname) {
  vassert(argname && argname[0]);
  vassert(oldname && oldname[0]);
  ArgInfo *ai = findNamedArgInfo(oldname);
  if (!ai) Sys_Error("cannot register alias for unknown cli argument '%s'", oldname);
  ArgInfo *als = allocArgInfo(argname, nullptr);
  als->flagptr = ai->flagptr;
  als->strptr = ai->strptr;
  als->type = ai->type;
  als->isAlias = true;
  als->cb = ai->cb;
  return true;
}


//==========================================================================
//
//  VParsedArgs::RegisterCallback
//
//  `nullptr` as name means "file argument handler"
//  "+" as name means "command handler"
//
//==========================================================================
bool VParsedArgs::RegisterCallback (const char *argname, const char *shorthelp, ArgCB acb) {
  ArgInfo *ai = allocArgInfo(argname, shorthelp);
  ai->type = AT_Callback;
  ai->cb = acb;
  return true;
}


//==========================================================================
//
//  VParsedArgs::RegisterStringOption
//
//==========================================================================
bool VParsedArgs::RegisterStringOption (const char *argname, const char *shorthelp, const char **strptr) {
  ArgInfo *ai = allocArgInfo(argname, shorthelp);
  ai->type = AT_StringOption;
  ai->strptr = strptr;
  return true;
}


//==========================================================================
//
//  VParsedArgs::RegisterFlagSet
//
//==========================================================================
bool VParsedArgs::RegisterFlagSet (const char *argname, const char *shorthelp, int *flagptr) {
  ArgInfo *ai = allocArgInfo(argname, shorthelp);
  ai->type = AT_FlagSet;
  ai->flagptr = flagptr;
  return true;
}


//==========================================================================
//
//  VParsedArgs::RegisterFlagReset
//
//==========================================================================
bool VParsedArgs::RegisterFlagReset (const char *argname, const char *shorthelp, int *flagptr) {
  ArgInfo *ai = allocArgInfo(argname, shorthelp);
  ai->type = AT_FlagReset;
  ai->flagptr = flagptr;
  return true;
}


//==========================================================================
//
//  VParsedArgs::RegisterFlagToggle
//
//==========================================================================
bool VParsedArgs::RegisterFlagToggle (const char *argname, const char *shorthelp, int *flagptr) {
  ArgInfo *ai = allocArgInfo(argname, shorthelp);
  ai->type = AT_FlagToggle;
  ai->flagptr = flagptr;
  return true;
}


//==========================================================================
//
//  VParsedArgs::IsArgBreaker
//
//==========================================================================
bool VParsedArgs::IsArgBreaker (VArgs &args, int idx) {
  if (idx < 1 || idx >= args.Count()) return true;
  const char *aname = args[idx];
  if (aname[0] == '+') return true;
  if (aname[0] != '-') return false;
  if (aname[1] == '-' && !aname[2]) return true;
  // perform some smart checks
  ArgInfo *ai = findNamedArgInfo(aname);
  if (ai) return true;
  // this may be "-arg=value" too
  const char *equ = strchr(aname, '=');
  if (!equ || equ == aname+1) return true;
  size_t slen = strlen(aname);
  char *tmp = (char *)::malloc(slen+4);
  strcpy(tmp, aname);
  char *etmp = (char *)strchr(tmp, '=');
  *etmp = 0;
  ai = findNamedArgInfo(tmp);
  ::free(tmp);
  return (ai ? true : false);
}


//==========================================================================
//
//  parseBoolArg
//
//==========================================================================
static int parseBoolArg (const char *avalue) {
  if (!avalue || !avalue[0]) return -1;
  if (VStr::strEquCI(avalue, "true") ||
      VStr::strEquCI(avalue, "tan") ||
      VStr::strEquCI(avalue, "on") ||
      VStr::strEquCI(avalue, "yes") ||
      VStr::strEquCI(avalue, "1"))
  {
    return 1;
  }
  if (VStr::strEquCI(avalue, "false") ||
      VStr::strEquCI(avalue, "ona") ||
      VStr::strEquCI(avalue, "off") ||
      VStr::strEquCI(avalue, "no") ||
      VStr::strEquCI(avalue, "0"))
  {
    return 0;
  }
  if (VStr::strEquCI(avalue, "toggle")) return 666;
  return -1;
}


//==========================================================================
//
//  VParsedArgs::parse
//
//==========================================================================
void VParsedArgs::parse (VArgs &args) {
  clear();
  if (args.Count() == 0) return;
  {
    const char *bpath = args[0];
    if (!bpath || !bpath[0]) {
      mBinDir = (char *)::malloc(4);
      strcpy(mBinDir, ".");
    } else {
      size_t plen = strlen(bpath);
      vassert(plen > 0);
      mBinDir = (char *)::malloc(plen+4);
      strcpy(mBinDir, bpath);
      #ifdef _WIN32
      for (size_t f = 0; f < plen; ++f) if (mBinDir[f] == '\\') mBinDir[f] = '/';
      #endif
    }
  }
  int aidx = 1;
  while (aidx < args.Count()) {
    const char *aname = args[aidx];
    if (!aname || !aname[0]) continue; // just in case
    // command?
    if (aname[0] == '+') {
      if (argInfoCmdArg && argInfoCmdArg->cb) {
        int inc = argInfoCmdArg->cb(args, aidx);
        vassert(inc >= 0);
        aidx = (inc ? inc : aidx+1);
        continue;
      }
      // no handler, just skip until the next arg
      for (++aidx; aidx < args.Count(); ++aidx) {
        aname = args[aidx];
        if (aname[0] == '-' || aname[0] == '+') break;
      }
      continue;
    }
    // no more args?
    if (aname[0] == '-' && aname[1] == '-' && !aname[2]) break;
    // argument?
    if (aname[0] == '-') {
      // find handler
      ArgInfo *ai = findNamedArgInfo(aname);
      // for callbacks, do no special parsing here
      if (ai && ai->type == AT_Callback) {
        if (!ai->cb) {
          ++aidx;
        } else {
          int inc = ai->cb(args, aidx);
          vassert(inc >= 0);
          aidx = (inc ? inc : aidx+1);
        }
        continue;
      }
      // not a callback, or not found
      const char *avalue = nullptr;
      if (!ai) {
        // this may be "-arg=value" too
        const char *equ = strchr(aname, '=');
        if (!equ || equ == aname+1) {
          GLog.Logf(NAME_Warning, "unknown CLI argument '%s', ignored", aname);
          ++aidx;
          continue;
        }
        size_t slen = strlen(aname);
        char *tmp = (char *)::malloc(slen+4);
        strcpy(tmp, aname);
        avalue = equ+1;
        char *etmp = (char *)strchr(tmp, '=');
        *etmp = 0;
        ai = findNamedArgInfo(tmp);
        if (!ai) {
          GLog.Logf(NAME_Warning, "unknown CLI argument '%s', ignored", tmp);
          ::free(tmp);
          ++aidx;
          continue;
        }
        if (ai->type == AT_Callback) {
          GLog.Logf(NAME_Warning, "unknown CLI argument '%s' (it cannot have a '='-value), ignored", tmp);
          ::free(tmp);
          ++aidx;
          continue;
        }
      }
      ++aidx; // skip this arg
      switch (ai->type) {
        case AT_StringOption: // just a string option, latest is used
          if (!avalue) {
            // no '=value' part
            if (aidx >= args.Count()) {
              GLog.Logf(NAME_Error, "option '%s' requires a value!", aname);
              break;
            }
            avalue = args[aidx];
            if (avalue[0] == '+') {
              GLog.Logf(NAME_Error, "option '%s' requires a value!", aname);
              break;
            } else if (avalue[0] == '-' && findNamedArgInfo(avalue)) {
              GLog.Logf(NAME_Error, "option '%s' requires a value!", aname);
              break;
            }
            ++aidx;
          }
          {
            if (ai->strarg) ::free(ai->strarg);
            size_t slen = strlen(avalue);
            ai->strarg = (char *)::malloc(slen+4);
            strcpy(ai->strarg, avalue);
            if (ai->strptr) *ai->strptr = ai->strarg;
          }
          break;
        case AT_FlagSet: // "set flag" argument
        case AT_FlagReset: // "reset flag" argument
        case AT_FlagToggle: // "toggle flag" arguments
          if (avalue) {
            int v = parseBoolArg(avalue);
            if (v < 0) {
              GLog.Logf(NAME_Error, "option '%s' requires a boolean value, not '%s'!", ai->name, avalue);
              break;
            }
            if (v == 666) {
              // toggle
              if (ai->flagptr) {
                v = *ai->flagptr;
                if (v < 0) v = 0;
                *ai->flagptr = (v ? 0 : 1);
              }
            } else {
              // change
              if (ai->type == AT_FlagReset) v = !v;
              if (ai->flagptr) *ai->flagptr = (v ? 1 : 0);
            }
          } else {
            switch (ai->type) {
              case AT_FlagToggle:
                if (ai->flagptr) {
                  int v = *ai->flagptr;
                  if (v < 0) v = 0;
                  *ai->flagptr = (v ? 0 : 1);
                }
                break;
              case AT_FlagReset:
                if (ai->flagptr) *ai->flagptr = 0;
                break;
              case AT_FlagSet:
                if (ai->flagptr) *ai->flagptr = 1;
                break;
            }
          }
        case AT_Ignore:
          break;
      }
      continue;
    }
    // file
    if (argInfoFileArg && argInfoFileArg->cb) {
      int inc = argInfoFileArg->cb(args, aidx);
      vassert(inc >= 0);
      aidx = (inc ? inc : aidx+1);
    } else {
      ++aidx;
    }
  }
  // if we got here, it means "all what is left are file names"
  if (argInfoFileArg && argInfoFileArg->cb) {
    while (aidx < args.Count()) {
      int inc = argInfoFileArg->cb(args, aidx);
      vassert(inc >= 0);
      aidx = (inc ? inc : aidx+1);
    }
  }
}


//==========================================================================
//
//  VParsedArgs::GetArgList
//
//  will not clear the list
//
//==========================================================================
void VParsedArgs::GetArgList (TArray<ArgHelp> &list, bool returnAll) {
  for (ArgInfo *ai = argInfoHead; ai; ai = ai->next) {
    if (ai->isAlias) continue;
    if (!ai->name || !ai->name[0]) continue;
    if (ai->help && ai->help[0] == '!' && !ai->help[1]) continue;
    if (returnAll || (ai->help && ai->help[0] && ai->help[0] != '!')) {
      ArgHelp &h = list.alloc();
      h.argname = ai->name;
      h.arghelp = (ai->help && ai->help[0] ? ai->help : nullptr);
      if (h.arghelp && h.arghelp[0] == '!') ++h.arghelp;
    }
  }
}
