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
#include <time.h>

#include "vcc.h"
#include "../../source/vc/vc_local.h"


// ////////////////////////////////////////////////////////////////////////// //
class VVccLog : public VLogListener {
public:
  virtual void Serialise (const char* text, EName event) override {
    devprintf("%s", text);
  }
};


// ////////////////////////////////////////////////////////////////////////// //
class VFileReader : public VStream {
private:
  FILE* mFile;

public:
  VFileReader (FILE* InFile) : mFile(InFile) { bLoading = true; }

  virtual ~VFileReader () { Close(); }

  // stream interface
  virtual void Serialise (void* V, int Length) override {
    if (bError) return;
    if (!mFile || fread(V, Length, 1, mFile) != 1) bError = true;
  }

  virtual void Seek (int InPos) override {
    if (bError) return;
    if (!mFile || fseek(mFile, InPos, SEEK_SET)) bError = true;
  }

  virtual int Tell() override {
    return (mFile ? ftell(mFile) : 0);
  }

  virtual int TotalSize () override {
    if (bError || !mFile) return 0;
    auto curpos = ftell(mFile);
    if (fseek(mFile, 0, SEEK_END)) { bError = true; return 0; }
    auto size = ftell(mFile);
    if (fseek(mFile, curpos, SEEK_SET)) { bError = true; return 0; }
    return (int)size;
  }

  virtual bool AtEnd () override {
    if (bError || !mFile) return true;
    return !!feof(mFile);
  }

  virtual void Flush () override {
    if (!mFile && fflush(mFile)) bError = true;
  }

  virtual bool Close() override {
    if (mFile) { fclose(mFile); mFile = nullptr; }
    return !bError;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
static VStr SourceFileName;
static VStr ObjectFileName;

static VPackage *CurrentPackage;

static bool DebugMode = false;
static FILE *DebugFile = nullptr;

static VLexer Lex;
static VVccLog VccLog;


//==========================================================================
//
//  devprintf
//
//==========================================================================
__attribute__((format(printf, 1, 2))) int devprintf (const char *text, ...) {
  if (!DebugMode) return 0;

  va_list argPtr;
  FILE* fp = (DebugFile ? DebugFile : stdout);
  va_start(argPtr, text);
  int ret = vfprintf(fp, text, argPtr);
  va_end(argPtr);
  fflush(fp);
  return ret;
}


//==========================================================================
//
//  Malloc
//
//==========================================================================
void* Malloc (size_t size) {
  if (!size) return nullptr;
  void *ptr = Z_Malloc(size);
  if (!ptr) FatalError("Couldn't alloc %d bytes", (int)size);
  memset(ptr, 0, size);
  return ptr;
}


//==========================================================================
//
//  Free
//
//==========================================================================
void Free (void* ptr) {
  if (ptr) Z_Free(ptr);
}


//==========================================================================
//
//  OpenFile
//
//==========================================================================
VStream* OpenFile (const VStr& Name) {
  FILE* file = fopen(*Name, "rb");
  return (file ? new VFileReader(file) : nullptr);
}


//==========================================================================
//
//  OpenDebugFile
//
//==========================================================================
static void OpenDebugFile (const VStr& name) {
  DebugFile = fopen(*name, "w");
  if (!DebugFile) FatalError("Can\'t open debug file \"%s\".", *name);
}


//==========================================================================
//
//  PC_Init
//
//==========================================================================
static void PC_Init () {
  CurrentPackage = new VPackage();
}


//==========================================================================
//
//  Init
//
//==========================================================================
static void Init () {
  DebugMode = false;
  DebugFile = nullptr;
  VName::StaticInit();
  VMemberBase::StaticInit();
  PC_Init();
}


//==========================================================================
//
//  DisplayUsage
//
//==========================================================================
static void DisplayUsage () {
  printf("\n");
  printf("VCC Copyright (c) 2000-2001 by JL, 2018,2019 by Ketmar Dark. (" __DATE__ " " __TIME__ ")\n");
  printf("Usage: vcc [options] source[.c] object[.dat]\n");
  //printf("    -d<file>     Output debugging information into specified file\n");
  //printf("    -a<function> Output function's ASM statements into debug file\n");
  printf("    -D<name>        Define macro\n");
  printf("    -I<directory>   Include files directory\n");
  printf("    -P<directory>   Package import files directory\n");
  exit(1);
}


//==========================================================================
//
//  ProcessArgs
//
//==========================================================================
static void ProcessArgs (int ArgCount, char **ArgVector) {
  int count = 0; // number of file arguments

  for (int i = 1; i < ArgCount; ++i) {
    char *text = ArgVector[i];
    if (*text == '-') {
      ++text;
      if (*text == 0) DisplayUsage();
      char option = *text++;
      switch (option) {
        case 'd':
          DebugMode = true;
          if (*text) OpenDebugFile(text);
          break;
        case 'I':
          Lex.AddIncludePath(text);
          break;
        case 'D':
          Lex.AddDefine(text);
          break;
        case 'P':
          VMemberBase::StaticAddPackagePath(text);
          break;
        default:
          DisplayUsage();
          break;
      }
      continue;
    }
    ++count;
    switch (count) {
      case 1: SourceFileName = VStr(text).DefaultExtension(".vc"); break;
      case 2: ObjectFileName = VStr(text).DefaultExtension(".dat"); break;
      default: DisplayUsage(); break;
    }
  }

  if (count == 0) DisplayUsage();

  if (count == 1) DisplayUsage();

  SourceFileName = SourceFileName.FixFileSlashes();
  devprintf("Main source file: %s\n", *SourceFileName);
}


// ////////////////////////////////////////////////////////////////////////// //
int main (int argc, char **argv) {
  try {
    GLog.AddListener(&VccLog);

    int starttime;
    int endtime;

    starttime = time(0);
    Init();
    ProcessArgs(argc, argv);
    Lex.AddDefine("IN_VCC");

    Lex.OpenSource(SourceFileName);
    VParser Parser(Lex, CurrentPackage);
    Parser.Parse();
    int parsetime = time(0);
    devprintf("Parsed in %02d:%02d\n", (parsetime-starttime)/60, (parsetime-starttime)%60);
    CurrentPackage->Emit();
    int compiletime = time(0);
    devprintf("Compiled in %02d:%02d\n", (compiletime-parsetime)/60, (compiletime-parsetime)%60);
    CurrentPackage->WriteObject(*ObjectFileName);
    //DumpAsm();
    //VName::StaticExit(); //k8: no reason to do this
    endtime = time(0);
    devprintf("Wrote in %02d:%02d\n", (endtime-compiletime)/60, (endtime-compiletime)%60);
    devprintf("Time elapsed: %02d:%02d\n", (endtime-starttime)/60, (endtime-starttime)%60);
    //VMemberBase::StaticExit(); //k8: no reason to do this
  } catch (VException& e) {
    FatalError("%s", e.What());
  }

  return 0;
}
