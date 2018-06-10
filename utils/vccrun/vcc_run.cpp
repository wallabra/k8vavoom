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

#include "vcc_run.h"


// ////////////////////////////////////////////////////////////////////////// //
VObject *mainObject = nullptr;


// ////////////////////////////////////////////////////////////////////////// //
__attribute__((noreturn, format(printf, 1, 2))) void Host_Error (const char *error, ...) {
  fprintf(stderr, "FATAL: ");
  va_list argPtr;
  va_start(argPtr, error);
  vfprintf(stderr, error, argPtr);
  va_end(argPtr);
  fprintf(stderr, "\n");
  exit(1);
}


// ////////////////////////////////////////////////////////////////////////// //
void PR_WriteOne () {
  P_GET_INT(type);
  switch (type) {
    case TYPE_Int: case TYPE_Byte: printf("%d", PR_Pop()); break;
    case TYPE_Bool: printf("%s", (PR_Pop() ? "true" : "false")); break;
    case TYPE_Float: printf("%f", PR_Popf()); break;
    case TYPE_Name: printf("%s", *PR_PopName()); break;
    case TYPE_String: printf("%s", *PR_PopStr()); break;
    case TYPE_Pointer: printf("%p", PR_PopPtr()); break;
    case TYPE_Vector: { TVec v = PR_Popv(); printf("(%f,%f,%f)", v.x, v.y, v.z); } break;
    default: Sys_Error("Tried to print something strange...");
  }
}

void PR_WriteFlush () {
  printf("\n");
}


// ////////////////////////////////////////////////////////////////////////// //
class VVccLog : public VLogListener {
public:
  virtual void Serialise (const char* text, EName event) override {
    dprintf("%s", text);
  }
};


// ////////////////////////////////////////////////////////////////////////// //
/*
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
*/


// ////////////////////////////////////////////////////////////////////////// //
static VStr SourceFileName;
static TArray<VStr> scriptArgs;

static int num_dump_asm;
static const char *dump_asm_names[1024];
static bool DebugMode = false;
static FILE *DebugFile;

static VLexer Lex;
static VVccLog VccLog;


//==========================================================================
//
//  dprintf
//
//==========================================================================
__attribute__((format(printf, 1, 2))) int dprintf (const char *text, ...) {
  if (!DebugMode) return 0;

  va_list argPtr;
  FILE* fp = stderr; //(DebugFile ? DebugFile : stdout);
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
  if (!ptr) FatalError("FATAL: couldn't alloc %d bytes", (int)size);
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
  return fsysOpenFile(Name);
  /*
  FILE* file = fopen(*Name, "rb");
  return (file ? new VFileReader(file) : nullptr);
  */
}


//==========================================================================
//
//  OpenDebugFile
//
//==========================================================================
static void OpenDebugFile (const VStr& name) {
  DebugFile = fopen(*name, "w");
  if (!DebugFile) FatalError("FATAL: can\'t open debug file \"%s\".", *name);
}


//==========================================================================
//
//  PC_DumpAsm
//
//==========================================================================
static void PC_DumpAsm (const char* name) {
  char buf[1024];
  char *cname;
  char *fname;

  snprintf(buf, sizeof(buf), "%s", name);

  //FIXME! PATH WITH DOTS!
  if (strstr(buf, ".")) {
    cname = buf;
    fname = strstr(buf, ".")+1;
    fname[-1] = 0;
  } else {
    dprintf("Dump ASM: Bad name %s\n", name);
    return;
  }

  //printf("<%s>.<%s>\n", cname, fname);

  for (int i = 0; i < VMemberBase::GMembers.Num(); ++i) {
    //if (VMemberBase::GMembers[i]->MemberType == MEMBER_Method) printf("O:<%s>; N:<%s>\n", *VMemberBase::GMembers[i]->Outer->Name, *VMemberBase::GMembers[i]->Name);
    if (VMemberBase::GMembers[i]->MemberType == MEMBER_Method &&
        !VStr::Cmp(cname, *VMemberBase::GMembers[i]->Outer->Name) &&
        !VStr::Cmp(fname, *VMemberBase::GMembers[i]->Name))
    {
      ((VMethod*)VMemberBase::GMembers[i])->DumpAsm();
      return;
    }
  }

  dprintf("Dump ASM: %s not found!\n", name);
}


//==========================================================================
//
//  DumpAsm
//
//==========================================================================
static void DumpAsm () {
  for (int i = 0; i < num_dump_asm; ++i) PC_DumpAsm(dump_asm_names[i]);
}


//==========================================================================
//
//  DisplayUsage
//
//==========================================================================
static void DisplayUsage () {
  printf("\n");
  printf("VCC Version 1.%d. Copyright (c) 2000-2001 by JL, 2018 by Ketmar Dark. (" __DATE__ " " __TIME__ "; opcodes: %d)\n", PROG_VERSION, NUM_OPCODES);
  printf("Usage: vcc [options] source[.c] [object[.dat]]\n");
  printf("    -d<file>     Output debugging information into specified file\n");
  printf("    -a<function> Output function's ASM statements into debug file\n");
  printf("    -D<name>           Define macro\n");
  printf("    -I<directory>      Include files directory\n");
  printf("    -P<directory>      Package import files directory\n");
  printf("    -base <directory>  Set base directory\n");
  printf("    -file <name>       Add pak file\n");
  exit(1);
}


//==========================================================================
//
//  ProcessArgs
//
//==========================================================================
static void ProcessArgs (int ArgCount, char **ArgVector) {
  int count = 0; // number of file arguments
  bool nomore = false;

  TArray<VStr> paklist;

  for (int i = 1; i < ArgCount; ++i) {
    const char *text = ArgVector[i];
    if (count == 0 && !nomore && *text == '-') {
      ++text;
      if (*text == 0) DisplayUsage();
      if (text[0] == '-' && text[1] == 0) { nomore = true; continue; }
      const char option = *text++;
      switch (option) {
        case 'd': DebugMode = true; if (*text) OpenDebugFile(text); break;
        case 'a': /*if (!*text) DisplayUsage(); dump_asm_names[num_dump_asm++] = text;*/ VMemberBase::doAsmDump = true; break;
        case 'I': VMemberBase::StaticAddIncludePath(text); break;
        case 'D': VMemberBase::StaticAddDefine(text); break;
        case 'P': VMemberBase::StaticAddPackagePath(text); break;
        default:
          --text;
          if (VStr::Cmp(text, "base") == 0) {
            ++i;
            if (i >= ArgCount) DisplayUsage();
            fsysBaseDir = VStr(ArgVector[i]);
          } else if (VStr::Cmp(text, "file") == 0) {
            ++i;
            if (i >= ArgCount) DisplayUsage();
            paklist.Append(VStr(ArgVector[i]));
            //fprintf(stderr, "<%s>\n", ArgVector[i]);
          } else {
            //fprintf(stderr, "*<%s>\n", text);
            DisplayUsage();
          }
          break;
      }
      continue;
    }
    ++count;
    switch (count) {
      case 1: SourceFileName = VStr(text).DefaultExtension(".vc"); break;
      default: scriptArgs.Append(VStr(text)); break;
    }
  }

  if (count == 0) DisplayUsage();

  /*
  if (!DebugFile) {
    VStr DbgFileName;
    DbgFileName = ObjectFileName.StripExtension()+".txt";
    OpenDebugFile(DbgFileName);
    DebugMode = true;
  }
  */

  fsysInit();
  for (int f = 0; f < paklist.length(); ++f) {
    if (fsysAppendPak(paklist[f])) {
      dprintf("added pak file '%s'...\n", *paklist[f]);
    } else {
      fprintf(stderr, "CAN'T add pak file '%s'!\n", *paklist[f]);
    }
  }

  SourceFileName = SourceFileName.fixSlashes();
  dprintf("Main source file: %s\n", *SourceFileName);
}


//==========================================================================
//
//  initialize
//
//==========================================================================
static void initialize () {
  DebugMode = false;
  DebugFile = nullptr;
  num_dump_asm = 0;
  VName::StaticInit();
  //VMemberBase::StaticInit();
  VObject::StaticInit();
  VMemberBase::StaticAddDefine("VCC_STANDALONE_EXECUTOR");
#ifdef VCCRUN_HAS_SDL
  VMemberBase::StaticAddDefine("VCCRUN_HAS_SDL");
#endif
#ifdef VCCRUN_HAS_OPENGL
  VMemberBase::StaticAddDefine("VCCRUN_HAS_OPENGL");
#endif
#ifdef VCCRUN_HAS_OPENAL
  VMemberBase::StaticAddDefine("VCCRUN_HAS_OPENAL");
#endif
  VMemberBase::StaticAddDefine("VCCRUN_HAS_IMAGO");
}


// ////////////////////////////////////////////////////////////////////////// //
// <0: error; bit 0: has arg; bit 1: returns int
static int checkArg (VMethod *mmain) {
  if (!mmain) return -1;
  if ((mmain->Flags&FUNC_VarArgs) != 0) return -1;
  if (mmain->NumParams > 0 && mmain->ParamFlags[0] != 0) return -1;
  int res = 0;
  if (mmain->ReturnType.Type != TYPE_Void && mmain->ReturnType.Type != TYPE_Int) return -1;
  if (mmain->ReturnType.Type == TYPE_Int) res |= 0x02;
  if (mmain->NumParams != 0) {
    if (mmain->NumParams != 1) return -1;
    VFieldType atp = mmain->ParamTypes[0];
    dprintf("  ptype0: %s\n", *atp.GetName());
    if (atp.Type != TYPE_Pointer) return -1;
    atp = atp.GetPointerInnerType();
    if (atp.Type != TYPE_DynamicArray) return -1;
    atp = atp.GetArrayInnerType();
    if (atp.Type != TYPE_String) return -1;
    res |= 0x01;
  }
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
int main (int argc, char **argv) {
  VStack ret;
  ret.i = 0;

#if 0
  {
    TArray<VStr> pts;
    VStr s = "/this/is/path/";
    s.SplitPath(pts);
    for (int f = 0; f < pts.length(); ++f) printf("  %d: <%s>:%d\n", f, *pts[f], pts[f].length());
    printf("<%s>\n", *(s.ToUpper()));
    printf("<%s>\n", *(s.ToLower()));
    printf("<%s>\n", *(s.ToUpper().ToLower()));
    printf("<%s>\n", *s);
  }
#endif

  try {
    GLog.AddListener(&VccLog);

    int starttime;
    int endtime;

    starttime = time(0);

    initialize();

    ProcessArgs(argc, argv);

    /*{
      auto fl = fsysOpenFile("text/model");
      auto sz = fl->TotalSize();
      printf("SIZE: %d (%d)\n", sz, fl->Tell());
      auto buf = new char[sz];
      fl->Serialize(buf, sz);
      delete fl;
      auto fo = fsysOpenDiskFileWrite("_000.ini");
      fo->Serialize(buf, sz);
      delete fo;
    }*/

    /*{
      VStr s = fsysFileFindAnyExt("a.boo");
      fprintf(stderr, "%s\n", *s);
    }*/

    PR_Init();

    VMemberBase::StaticLoadPackage(VName("engine"), TLocation());
    //VMemberBase::StaticLoadPackage(VName("ui"), TLocation());

    VPackage *CurrentPackage = new VPackage(VName("vccrun"));

    dprintf("Compiling '%s'...\n", *SourceFileName);

    VStream *strm = OpenFile(SourceFileName);
    if (!strm) {
      FatalError("FATAL: cannot open file '%s'", *SourceFileName);
    }

    CurrentPackage->LoadSourceObject(strm, SourceFileName, TLocation());
    dprintf("Total memory used: %u\n", VExpression::TotalMemoryUsed);
    DumpAsm();
    endtime = time(0);
    dprintf("Time elapsed: %02d:%02d\n", (endtime-starttime)/60, (endtime-starttime)%60);

    VScriptArray scargs(scriptArgs);
    VClass *mklass = VClass::FindClass("Main");
    if (mklass) {
      dprintf("Found class 'Main'\n");
      VMethod *mmain = mklass->FindAccessibleMethod("main");
      if (mmain) {
        dprintf(" Found method 'main()' (return type: %u:%s)\n", mmain->ReturnType.Type, *mmain->ReturnType.GetName());
        int atp = checkArg(mmain);
        if (atp < 0) FatalError("Main::main() should be either arg-less, or have one `array!string*` argument, and should be either `void`, or return `int`!");
        auto sss = pr_stackPtr;
        mainObject = VObject::StaticSpawnObject(mklass);
        if ((mmain->Flags&FUNC_Static) == 0) {
          //auto imain = Spawn<VLevel>();
          P_PASS_REF((VObject *)mainObject);
        }
        if (atp&0x01) P_PASS_REF(&scargs);
        ret = VObject::ExecuteFunction(mmain);
        if ((atp&0x02) == 0) ret.i = 0;
        if (sss != pr_stackPtr) FatalError("FATAL: stack imbalance!");
      }
    }

    VObject::StaticExit();
    VName::StaticExit();
  } catch (VException& e) {
    ret.i = -1;
    FatalError("FATAL: %s", e.What());
  }

  return ret.i;
}
