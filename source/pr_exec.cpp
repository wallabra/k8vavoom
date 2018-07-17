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
//**    Execution of PROGS.
//**
//**************************************************************************
//#define VCC_STUPID_TRACER

#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
# include "gamedefs.h"
# include "progdefs.h"
#else
# if defined(IN_VCC)
#  include "../utils/vcc/vcc.h"
# elif defined(VCC_STANDALONE_EXECUTOR)
#  include "../vccrun/vcc_run.h"
# endif
#endif

// builtin codes
#define BUILTIN_OPCODE_INFO
#include "progdefs.h"


#define MAX_PROG_STACK  (10000)
#define STACK_ID  (0x45f6cd4b)

#define CHECK_STACK_OVERFLOW
#define CHECK_STACK_UNDERFLOW
#define CHECK_FOR_EMPTY_STACK

//#define VMEXEC_RUNDUMP

#ifdef VMEXEC_RUNDUMP
static int k8edIndent = 0;

static void printIndent () { for (int f = k8edIndent; f > 0; --f) fputc(' ', stdout); }
static void enterIndent () { ++k8edIndent; }
static void leaveIndent () { if (--k8edIndent < 0) *(int *)0 = 0; }
#endif

enum { MaxDynArrayLength = 1024*1024*512 };


// ////////////////////////////////////////////////////////////////////////// //
VStack *pr_stackPtr;

static VMethod *current_func = nullptr;
static VStack pr_stack[MAX_PROG_STACK];

struct VCSlice {
  vuint8 *ptr; // it is easier to index this way
  vint32 length;
};


//==========================================================================
//
//  PR_Init
//
//==========================================================================
void PR_Init () {
  // set stack ID for overflow / underflow checks
  pr_stack[0].i = STACK_ID;
  pr_stack[MAX_PROG_STACK-1].i = STACK_ID;
  pr_stackPtr = pr_stack+1;
}


//==========================================================================
//
//  PR_OnAbort
//
//==========================================================================
void PR_OnAbort () {
  current_func = nullptr;
  pr_stackPtr = pr_stack+1;
}


//==========================================================================
//
//  PR_Profile1
//
//==========================================================================
extern "C" void PR_Profile1 () {
  ++(current_func->Profile1);
}


//==========================================================================
//
//  PR_Profile2
//
//==========================================================================
extern "C" void PR_Profile2 () {
  if (current_func && (!(current_func->Flags&FUNC_Native))) ++(current_func->Profile2);
}

//==========================================================================
//
//  PR_Profile2_end
//
//==========================================================================
extern "C" void PR_Profile2_end () {}


// ////////////////////////////////////////////////////////////////////////// //
// stack trace utilities

struct CallStackItem {
  VMethod *func;
  const vuint8 *ip;
  VStack *sp;
};

static CallStackItem *callStack = nullptr;
static vuint32 cstUsed = 0, cstSize = 0;


static inline void cstFixTopIPSP (const vuint8 *ip) {
  if (cstUsed > 0) {
    callStack[cstUsed-1].ip = ip;
    callStack[cstUsed-1].sp = pr_stackPtr;
  }
}


static void cstPush (VMethod *func) {
  if (cstUsed == cstSize) {
    //FIXME: handle OOM here
    cstSize += 16384;
    callStack = (CallStackItem *)realloc(callStack, sizeof(callStack[0])*cstSize);
  }
  callStack[cstUsed].func = func;
  callStack[cstUsed].ip = nullptr;
  callStack[cstUsed].sp = pr_stackPtr;
  ++cstUsed;
}


static inline void cstPop () {
  if (cstUsed > 0) --cstUsed;
}


// `ip` can be null
static void cstDump (const vuint8 *ip) {
  //ip = func->Statements.Ptr();
  fprintf(stderr, "\n\n=== VaVoomScript Call Stack (%u) ===\n", cstUsed);
  if (cstUsed > 0) {
    // do the best thing we can
    if (!ip && callStack[cstUsed-1].ip) ip = callStack[cstUsed-1].ip;
    for (vuint32 sp = cstUsed; sp > 0; --sp) {
      VMethod *func = callStack[sp-1].func;
      TLocation loc = func->FindPCLocation(sp == cstUsed ? ip : callStack[sp-1].ip);
      if (!loc.isInternal()) {
        fprintf(stderr, "  %03u: %s (%s:%d)\n", cstUsed-sp, *func->GetFullName(), *loc.GetSource(), loc.GetLine());
      } else {
        fprintf(stderr, "  %03u: %s\n", cstUsed-sp, *func->GetFullName());
      }
    }
  }
  fprintf(stderr, "=============================\n\n");
}


//==========================================================================
//
//  RunFunction
//
//==========================================================================

#if !defined(VCC_STUPID_TRACER)
# define USE_COMPUTED_GOTO  1
# else
# warning "computed gotos are off"
#endif

#if USE_COMPUTED_GOTO
# define PR_VM_SWITCH(op)  goto *vm_labels[op];
# define PR_VM_CASE(x)   Lbl_ ## x:
# define PR_VM_BREAK     goto *vm_labels[*ip];
# define PR_VM_DEFAULT
#else
# define PR_VM_SWITCH(op)  switch(op)
# define PR_VM_CASE(x)   case x:
# define PR_VM_BREAK     break
# define PR_VM_DEFAULT   default:
#endif

#define ReadU8(ip)     (*(vuint8 *)(ip))
#define ReadInt16(ip)  (*(vint16 *)(ip))
#define ReadInt32(ip)  (*(vint32 *)(ip))
#define ReadPtr(ip)    (*(void **)(ip))
#define ReadType(T, ip)  do { \
  T.Type = (ip)[0]; \
  T.ArrayInnerType = (ip)[1]; \
  T.InnerType = (ip)[2]; \
  T.PtrLevel = (ip)[3]; \
  T.SetArrayDimIntr(ReadInt32((ip)+4)); \
  T.Class = (VClass *)ReadPtr((ip)+8); \
  ip += 8+sizeof(VClass *); \
} while (0)


#define MAX_ITER_STACK (32)

struct ItStackItem {
  VScriptIterator *it; // can be null
  vuint8 *doneip; // can be null
};


static void RunFunction (VMethod *func) {
  vuint8 *ip = nullptr;
  VStack *sp;
  VStack *local_vars;
  //VScriptIterator *ActiveIterators = nullptr;
  ItStackItem itstack[MAX_ITER_STACK];
  int itsp = 0;
  bool inReturn = false; // for iterator cleanup
  int retSize = 0;
  float ftemp;
  vint32 itemp;

  guard(RunFunction);
  current_func = func;

  if (!func) { cstDump(nullptr); Sys_Error("Trying to execute null function"); }

  if (func->Flags&FUNC_Net) {
    VStack *Params = pr_stackPtr-func->ParamsSize;
    if (((VObject *)Params[0].p)->ExecuteNetMethod(func)) return;
  }

  if (func->Flags&FUNC_Native) {
    // native function, first statement is pointer to function
    func->NativeFunc();
    return;
  }

  cstPush(func);

  // cache stack pointer in register
  sp = pr_stackPtr;

  // setup local vars
  local_vars = sp-func->ParamsSize;
  memset(sp, 0, (func->NumLocals-func->ParamsSize)*sizeof(VStack));
  sp += func->NumLocals-func->ParamsSize;

  ip = func->Statements.Ptr();

#ifdef VMEXEC_RUNDUMP
  enterIndent(); printIndent(); printf("ENTERING VC FUNCTION `%s`; sp=%d\n", *func->GetFullName(), (int)(sp-pr_stack));
#endif

  // the main execution loop
  for (;;) {
func_loop:

#if USE_COMPUTED_GOTO
    static void *vm_labels[] = {
# define DECLARE_OPC(name, args) &&Lbl_OPC_ ## name
# define OPCODE_INFO
# include "progdefs.h"
    0 };
#endif

#ifdef VCC_STUPID_TRACER
    fprintf(stderr, "*** %s: %6u: %s (sp=%d)\n", *func->GetFullName(), (unsigned)(ip-func->Statements.Ptr()), StatementInfo[*ip].name, (int)(sp-pr_stackPtr));
#endif

    PR_VM_SWITCH(*ip) {
      PR_VM_CASE(OPC_Done)
        cstDump(ip);
        Sys_Error("Empty function or invalid opcode");
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Call)
        pr_stackPtr = sp;
        cstFixTopIPSP(ip);
        //cstDump(ip);
        RunFunction((VMethod *)ReadPtr(ip+1));
        current_func = func;
        ip += 1+sizeof(void *);
        sp = pr_stackPtr;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushVFunc)
        sp[0].p = ((VObject *)sp[-1].p)->GetVFunctionIdx(ReadInt16(ip+1));
        ip += 3;
        ++sp;
        PR_VM_BREAK;

      /*
      PR_VM_CASE(OPC_PushVFuncB)
        sp[0].p = ((VObject *)sp[-1].p)->GetVFunctionIdx(ip[1]);
        ip += 2;
        ++sp;
        PR_VM_BREAK;
      */

      PR_VM_CASE(OPC_PushFunc)
        sp[0].p = ReadPtr(ip+1);
        ip += 1+sizeof(void *);
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VCall)
        pr_stackPtr = sp;
        if (!sp[-ip[3]].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        cstFixTopIPSP(ip);
        RunFunction(((VObject *)sp[-ip[3]].p)->GetVFunctionIdx(ReadInt16(ip+1)));
        ip += 4;
        current_func = func;
        sp = pr_stackPtr;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VCallB)
        pr_stackPtr = sp;
        if (!sp[-ip[2]].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        cstFixTopIPSP(ip);
        RunFunction(((VObject *)sp[-ip[2]].p)->GetVFunctionIdx(ip[1]));
        ip += 3;
        current_func = func;
        sp = pr_stackPtr;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DelegateCall)
        {
          // get pointer to the delegate
          void **pDelegate = (void **)((vuint8 *)sp[-ip[5]].p+ReadInt32(ip+1));
          // push proper self object
          if (!pDelegate[0]) { cstDump(ip); Sys_Error("Delegate is not initialised"); }
          sp[-ip[5]].p = pDelegate[0];
          pr_stackPtr = sp;
          cstFixTopIPSP(ip);
          RunFunction((VMethod *)pDelegate[1]);
        }
        ip += 6;
        current_func = func;
        sp = pr_stackPtr;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DelegateCallS)
        {
          // get pointer to the delegate
          void **pDelegate = (void **)((vuint8 *)sp[-ip[3]].p+ReadInt16(ip+1));
          // push proper self object
          if (!pDelegate[0]) { cstDump(ip); Sys_Error("Delegate is not initialised"); }
          sp[-ip[3]].p = pDelegate[0];
          pr_stackPtr = sp;
          cstFixTopIPSP(ip);
          RunFunction((VMethod *)pDelegate[1]);
        }
        ip += 4;
        current_func = func;
        sp = pr_stackPtr;
        PR_VM_BREAK;

      // call delegate by a pushed pointer to it
      PR_VM_CASE(OPC_DelegateCallPtr)
        {
          // get args size (to get `self` offset)
          int sofs = ReadInt32(ip+1);
          ip += 5;
          // get pointer to the delegate
          void **pDelegate = (void **)sp[-1].p;
          // drop delegate argument
          sp -= 1;
          // push proper self object
          if (!pDelegate[0]) { cstDump(ip); Sys_Error("Delegate is not initialised"); }
          sp[-sofs].p = pDelegate[0];
          pr_stackPtr = sp;
          cstFixTopIPSP(ip);
          RunFunction((VMethod *)pDelegate[1]);
        }
        current_func = func;
        sp = pr_stackPtr;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Return)
        if (itsp == 0) {
          checkSlow(sp == local_vars+func->NumLocals);
#ifdef VMEXEC_RUNDUMP
          printIndent(); printf("LEAVING VC FUNCTION `%s`; sp=%d\n", *func->GetFullName(), (int)(sp-pr_stack)); leaveIndent();
#endif
          pr_stackPtr = local_vars;
          cstPop();
          return;
        }
        //retSize = 0;
        goto doRealReturn;

      PR_VM_CASE(OPC_ReturnL)
        if (itsp == 0) {
          checkSlow(sp == local_vars+func->NumLocals+1);
#ifdef VMEXEC_RUNDUMP
          printIndent(); printf("LEAVING VC FUNCTION `%s`; sp=%d\n", *func->GetFullName(), (int)(sp-pr_stack)); leaveIndent();
#endif
          ((VStack *)local_vars)[0] = sp[-1];
          pr_stackPtr = local_vars+1;
          cstPop();
          return;
        }
        retSize = 1;
        goto doRealReturn;

      PR_VM_CASE(OPC_ReturnV)
        if (itsp == 0) {
          checkSlow(sp == local_vars+func->NumLocals+3);
#ifdef VMEXEC_RUNDUMP
          printIndent(); printf("LEAVING VC FUNCTION `%s`; sp=%d\n", *func->GetFullName(), (int)(sp-pr_stack)); leaveIndent();
#endif
          ((VStack *)local_vars)[0] = sp[-3];
          ((VStack *)local_vars)[1] = sp[-2];
          ((VStack *)local_vars)[2] = sp[-1];
          pr_stackPtr = local_vars+3;
          cstPop();
          return;
        }
        retSize = 3;
        goto doRealReturn;

      PR_VM_CASE(OPC_GotoB)
        ip += ip[1];
        PR_VM_BREAK;

      PR_VM_CASE(OPC_GotoNB)
        ip -= ip[1];
        PR_VM_BREAK;

      PR_VM_CASE(OPC_GotoS)
        ip += ReadInt16(ip+1);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Goto)
        ip += ReadInt32(ip+1);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IfGotoB)
        if (sp[-1].i) ip += ip[1]; else ip += 2;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IfGotoNB)
        if (sp[-1].i) ip -= ip[1]; else ip += 2;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IfGotoS)
        if (sp[-1].i) ip += ReadInt16(ip+1); else ip += 3;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IfGoto)
        if (sp[-1].i) ip += ReadInt32(ip+1); else ip += 5;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IfNotGotoB)
        if (!sp[-1].i) ip += ip[1]; else ip += 2;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IfNotGotoNB)
        if (!sp[-1].i) ip -= ip[1]; else ip += 2;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IfNotGotoS)
        if (!sp[-1].i) ip += ReadInt16(ip+1); else ip += 3;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IfNotGoto)
        if (!sp[-1].i) ip += ReadInt32(ip+1); else ip += 5;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_CaseGotoB)
        if (ip[1] == sp[-1].i) {
          ip += ReadInt16(ip+2);
          --sp;
        } else {
          ip += 4;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_CaseGotoS)
        if (ReadInt16(ip+1) == sp[-1].i) {
          ip += ReadInt16(ip+3);
          --sp;
        } else {
          ip += 5;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_CaseGoto)
        if (ReadInt32(ip+1) == sp[-1].i) {
          ip += ReadInt16(ip+5);
          --sp;
        } else {
          ip += 7;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushNumber0)
        ++ip;
        sp->i = 0;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushNumber1)
        ++ip;
        sp->i = 1;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushNumberB)
        sp->i = ip[1];
        ip += 2;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushNumberS)
        sp->i = ReadInt16(ip+1);
        ip += 3;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushNumber)
        sp->i = ReadInt32(ip+1);
        ip += 5;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushName)
        sp->i = ReadInt32(ip+1);
        ip += 5;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushNameS)
        sp->i = ReadInt16(ip+1);
        ip += 3;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushString)
        sp->p = ReadPtr(ip+1);
        ip += 1+sizeof(void *);
        ++sp; {
          const char *S = (const char *)sp[-1].p;
          sp[-1].p = nullptr;
          *(VStr *)&sp[-1].p = S;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushClassId)
      PR_VM_CASE(OPC_PushState)
        sp->p = ReadPtr(ip+1);
        ip += 1+sizeof(void *);
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushNull)
        ++ip;
        sp->p = nullptr;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalAddress0)
        ++ip;
        sp->p = &local_vars[0];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalAddress1)
        ++ip;
        sp->p = &local_vars[1];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalAddress2)
        ++ip;
        sp->p = &local_vars[2];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalAddress3)
        ++ip;
        sp->p = &local_vars[3];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalAddress4)
        ++ip;
        sp->p = &local_vars[4];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalAddress5)
        ++ip;
        sp->p = &local_vars[5];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalAddress6)
        ++ip;
        sp->p = &local_vars[6];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalAddress7)
        ++ip;
        sp->p = &local_vars[7];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalAddressB)
        sp->p = &local_vars[ip[1]];
        ip += 2;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalAddressS)
        sp->p = &local_vars[ReadInt16(ip+1)];
        ip += 3;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalAddress)
        sp->p = &local_vars[ReadInt32(ip+1)];
        ip += 5;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalValue0)
        ++ip;
        *sp = local_vars[0];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalValue1)
        ++ip;
        *sp = local_vars[1];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalValue2)
        ++ip;
        *sp = local_vars[2];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalValue3)
        ++ip;
        *sp = local_vars[3];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalValue4)
        ++ip;
        *sp = local_vars[4];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalValue5)
        ++ip;
        *sp = local_vars[5];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalValue6)
        ++ip;
        *sp = local_vars[6];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalValue7)
        ++ip;
        *sp = local_vars[7];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalValueB)
        *sp = local_vars[ip[1]];
        ip += 2;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VLocalValueB)
        sp[0].f = ((TVec *)&local_vars[ip[1]])->x;
        sp[1].f = ((TVec *)&local_vars[ip[1]])->y;
        sp[2].f = ((TVec *)&local_vars[ip[1]])->z;
        ip += 2;
        sp += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_StrLocalValueB)
        sp->p = nullptr;
        *(VStr *)&sp->p = *(VStr *)&local_vars[ip[1]];
        ip += 2;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Offset)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].p = (vuint8 *)sp[-1].p+ReadInt32(ip+1);
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_OffsetS)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].p = (vuint8 *)sp[-1].p+ReadInt16(ip+1);
        ip += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FieldValue)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = *(vint32 *)((vuint8 *)sp[-1].p+ReadInt32(ip+1));
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FieldValueS)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = *(vint32 *)((vuint8 *)sp[-1].p+ReadInt16(ip+1));
        ip += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VFieldValue)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        {
          TVec *vp = (TVec *)((vuint8 *)sp[-1].p+ReadInt32(ip+1));
          sp[1].f = vp->z;
          sp[0].f = vp->y;
          sp[-1].f = vp->x;
        }
        sp += 2;
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VFieldValueS)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        {
          TVec *vp = (TVec *)((vuint8 *)sp[-1].p+ReadInt16(ip+1));
          sp[1].f = vp->z;
          sp[0].f = vp->y;
          sp[-1].f = vp->x;
        }
        sp += 2;
        ip += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PtrFieldValue)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].p = *(void **)((vuint8 *)sp[-1].p+ReadInt32(ip+1));
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PtrFieldValueS)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].p = *(void **)((vuint8 *)sp[-1].p+ReadInt16(ip+1));
        ip += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_StrFieldValue)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        {
          VStr *Ptr = (VStr *)((vuint8 *)sp[-1].p+ReadInt32(ip+1));
          sp[-1].p = nullptr;
          *(VStr *)&sp[-1].p = *Ptr;
        }
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_StrFieldValueS)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        {
          VStr *Ptr = (VStr *)((vuint8 *)sp[-1].p+ReadInt16(ip+1));
          sp[-1].p = nullptr;
          *(VStr *)&sp[-1].p = *Ptr;
        }
        ip += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_SliceFieldValue)
        if (sp[-1].p) {
          vint32 ofs = ReadInt32(ip+1);
          VCSlice vs = *(VCSlice *)((vuint8 *)sp[-1].p+ofs);
          if (!vs.ptr) vs.length = 0; else if (vs.length < 0) vs.length = 0; // just in case
          sp[-1].p = vs.ptr;
          sp[0].i = vs.length;
          ++sp;
        } else {
          cstDump(ip); Sys_Error("Reference not set to an instance of an object");
        }
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteFieldValue)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = *((vuint8 *)sp[-1].p+ReadInt32(ip+1));
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteFieldValueS)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = *((vuint8 *)sp[-1].p+ReadInt16(ip+1));
        ip += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool0FieldValue)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt32(ip+1))&ip[5]);
        ip += 6;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool0FieldValueS)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt16(ip+1))&ip[3]);
        ip += 4;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool1FieldValue)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt32(ip+1))&(ip[5]<<8));
        ip += 6;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool1FieldValueS)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt16(ip+1))&(ip[3]<<8));
        ip += 4;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool2FieldValue)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt32(ip+1))&(ip[5]<<16));
        ip += 6;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool2FieldValueS)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt16(ip+1))&(ip[3]<<16));
        ip += 4;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool3FieldValue)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt32(ip+1))&(ip[5]<<24));
        ip += 6;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool3FieldValueS)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt16(ip+1))&(ip[3]<<24));
        ip += 4;
        PR_VM_BREAK;

      // won't pop index
      // [-2]: op
      // [-1]: idx
      PR_VM_CASE(OPC_CheckArrayBounds)
        if (sp[-1].i < 0 || sp[-1].i >= ReadInt32(ip+1)) {
          cstDump(ip);
          Sys_Error("Array index %d is out of bounds (%d)", sp[-1].i, ReadInt32(ip+1));
        }
        ip += 5;
        PR_VM_BREAK;

      // won't pop index
      // [-3]: op
      // [-2]: idx
      // [-1]: idx2
      PR_VM_CASE(OPC_CheckArrayBounds2D)
        if (sp[-2].i < 0 || sp[-2].i >= ReadInt16(ip+1)) { cstDump(ip); Sys_Error("First array index %d is out of bounds (%d)", sp[-2].i, ReadInt16(ip+1)); }
        if (sp[-1].i < 0 || sp[-1].i >= ReadInt16(ip+1+2)) { cstDump(ip); Sys_Error("Second array index %d is out of bounds (%d)", sp[-1].i, ReadInt16(ip+1+2)); }
        ip += 1+2+2+4;
        PR_VM_BREAK;

      // [-2]: op
      // [-1]: idx
      PR_VM_CASE(OPC_ArrayElement)
        sp[-2].p = (vuint8 *)sp[-2].p+sp[-1].i*ReadInt32(ip+1);
        ip += 5;
        --sp;
        PR_VM_BREAK;

      /*
      PR_VM_CASE(OPC_ArrayElementS)
        sp[-2].p = (vuint8 *)sp[-2].p+sp[-1].i*ReadInt16(ip+1);
        ip += 3;
        --sp;
        PR_VM_BREAK;
      */

      PR_VM_CASE(OPC_ArrayElementB)
        sp[-2].p = (vuint8 *)sp[-2].p+sp[-1].i*ip[1];
        ip += 2;
        --sp;
        PR_VM_BREAK;

      // [-3]: op
      // [-2]: idx
      // [-1]: idx2
      PR_VM_CASE(OPC_ArrayElement2D)
        sp[-3].p = (vuint8 *)sp[-3].p+(sp[-1].i*ReadInt16(ip+1)+sp[-2].i)*ReadInt32(ip+1+2+2);
        sp -= 2;
        ip += 1+2+2+4;
        PR_VM_BREAK;

      // [-1]: index
      // [-2]: ptr to VCSlice
      PR_VM_CASE(OPC_SliceElement)
        if (sp[-2].p) {
          int idx = sp[-1].i;
          VCSlice vs = *(VCSlice *)sp[-2].p;
          if (!vs.ptr) vs.length = 0; else if (vs.length < 0) vs.length = 0; // just in case
          if (idx < 0 || idx >= vs.length) { cstDump(ip); Sys_Error("Slice index %d is out of range (%d)", idx, vs.length); }
          sp[-2].p = vs.ptr+idx*ReadInt32(ip+1);
        } else {
          cstDump(ip);
          Sys_Error("Slice index %d is out of range (0)", sp[-1].i);
        }
        ip += 5;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_OffsetPtr)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Cannot offset null pointer"); }
        sp[-1].p = (vuint8 *)sp[-1].p+ReadInt32(ip+1);
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushPointed)
        ++ip;
        sp[-1].i = *(vint32 *)sp[-1].p;
        PR_VM_BREAK;

      // [-1]: ptr to VCSlice
      PR_VM_CASE(OPC_PushPointedSlice)
        if (sp[-1].p) {
          VCSlice vs = *(VCSlice *)sp[-1].p;
          if (!vs.ptr) vs.length = 0; else if (vs.length < 0) vs.length = 0; // just in case
          sp[-1].p = vs.ptr;
          sp[0].i = vs.length;
        } else {
          sp[-1].p = nullptr;
          sp[0].i = 0;
        }
        ip += 5;
        ++sp;
        PR_VM_BREAK;

      // [-1]: ptr to VCSlice
      PR_VM_CASE(OPC_PushPointedSliceLen)
        ++ip;
        if (sp[-1].p) {
          VCSlice vs = *(VCSlice *)sp[-1].p;
          if (!vs.ptr) vs.length = 0; else if (vs.length < 0) vs.length = 0; // just in case
          sp[-1].i = vs.length;
        } else {
          sp[-1].i = 0;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VPushPointed)
        ++ip;
        sp[1].f = ((TVec *)sp[-1].p)->z;
        sp[0].f = ((TVec *)sp[-1].p)->y;
        sp[-1].f = ((TVec *)sp[-1].p)->x;
        sp += 2;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushPointedPtr)
        ++ip;
        sp[-1].p = *(void **)sp[-1].p;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushPointedByte)
        ++ip;
        sp[-1].i = *(vuint8 *)sp[-1].p;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushBool0)
        {
          vuint32 mask = ip[1];
          ip += 2;
          sp[-1].i = !!(*(vint32 *)sp[-1].p&mask);
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushBool1)
        {
          vuint32 mask = ip[1]<<8;
          ip += 2;
          sp[-1].i = !!(*(vint32 *)sp[-1].p&mask);
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushBool2)
        {
          vuint32 mask = ip[1]<<16;
          ip += 2;
          sp[-1].i = !!(*(vint32 *)sp[-1].p&mask);
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushBool3)
        {
          vuint32 mask = ip[1]<<24;
          ip += 2;
          sp[-1].i = !!(*(vint32 *)sp[-1].p&mask);
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushPointedStr)
        {
          ++ip;
          VStr *Ptr = (VStr *)sp[-1].p;
          sp[-1].p = nullptr;
          *(VStr *)&sp[-1].p = *Ptr;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushPointedDelegate)
        ++ip;
        sp[0].p = ((void **)sp[-1].p)[1];
        sp[-1].p = ((void **)sp[-1].p)[0];
        ++sp;
        PR_VM_BREAK;

    #define BINOP(mem, op) \
      ++ip; \
      sp[-2].mem = sp[-2].mem op sp[-1].mem; \
      --sp;
    #define BINOP_Q(mem, op) \
      ++ip; \
      sp[-2].mem op sp[-1].mem; \
      --sp;

      PR_VM_CASE(OPC_Add)
        BINOP_Q(i, +=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Subtract)
        BINOP_Q(i, -=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Multiply)
        BINOP_Q(i, *=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Divide)
        if (!sp[-1].i) { cstDump(ip); Sys_Error("Division by 0"); }
        BINOP_Q(i, /=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Modulus)
        if (!sp[-1].i) { cstDump(ip); Sys_Error("Division by 0"); }
        BINOP_Q(i, %=);
        PR_VM_BREAK;

    #define BOOLOP(mem, op) \
      ++ip; \
      sp[-2].i = sp[-2].mem op sp[-1].mem; \
      --sp;

      PR_VM_CASE(OPC_Equals)
        BOOLOP(i, ==);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_NotEquals)
        BOOLOP(i, !=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Less)
        BOOLOP(i, <);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Greater)
        BOOLOP(i, >);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LessEquals)
        BOOLOP(i, <=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_GreaterEquals)
        BOOLOP(i, >=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_NegateLogical)
        ++ip;
        sp[-1].i = !sp[-1].i;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_AndBitwise)
        BINOP_Q(i, &=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_OrBitwise)
        BINOP_Q(i, |=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_XOrBitwise)
        BINOP_Q(i, ^=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LShift)
        BINOP_Q(i, <<=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_RShift)
        BINOP_Q(i, >>=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_URShift)
        ++ip;
        *(vuint32 *)&sp[-2].i >>= sp[-1].i;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_UnaryMinus)
        ++ip;
        sp[-1].i = -sp[-1].i;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_BitInverse)
        ++ip;
        sp[-1].i = ~sp[-1].i;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PreInc)
        ++ip;
        {
          vint32 *ptr = (vint32 *)sp[-1].p;
          ++(*ptr);
          sp[-1].i = *ptr;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PreDec)
        ++ip;
        {
          vint32 *ptr = (vint32 *)sp[-1].p;
          --(*ptr);
          sp[-1].i = *ptr;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PostInc)
        ++ip;
        {
          vint32 *ptr = (vint32 *)sp[-1].p;
          sp[-1].i = *ptr;
          (*ptr)++;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PostDec)
        ++ip;
        {
          vint32 *ptr = (vint32 *)sp[-1].p;
          sp[-1].i = *ptr;
          (*ptr)--;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IncDrop)
        ++ip;
        ++(*(vint32 *)sp[-1].p);
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DecDrop)
        ++ip;
        --(*(vint32 *)sp[-1].p);
        --sp;
        PR_VM_BREAK;

    #define ASSIGNOP(type, mem, op) \
      ++ip; \
      *(type *)sp[-2].p op sp[-1].mem; \
      sp -= 2;

      PR_VM_CASE(OPC_AssignDrop)
        ASSIGNOP(vint32, i, =);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_AddVarDrop)
        ASSIGNOP(vint32, i, +=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_SubVarDrop)
        ASSIGNOP(vint32, i, -=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_MulVarDrop)
        ASSIGNOP(vint32, i, *=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DivVarDrop)
        if (!sp[-1].i) { cstDump(ip); Sys_Error("Division by 0"); }
        ASSIGNOP(vint32, i, /=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ModVarDrop)
        if (!sp[-1].i) { cstDump(ip); Sys_Error("Division by 0"); }
        ASSIGNOP(vint32, i, %=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_AndVarDrop)
        ASSIGNOP(vint32, i, &=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_OrVarDrop)
        ASSIGNOP(vint32, i, |=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_XOrVarDrop)
        ASSIGNOP(vint32, i, ^=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LShiftVarDrop)
        ASSIGNOP(vint32, i, <<=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_RShiftVarDrop)
        ASSIGNOP(vint32, i, >>=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_URShiftVarDrop)
        ASSIGNOP(vuint32, i, >>=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_BytePreInc)
        ++ip;
        {
          vuint8 *ptr = (vuint8 *)sp[-1].p;
          ++(*ptr);
          sp[-1].i = *ptr;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_BytePreDec)
        ++ip;
        {
          vuint8 *ptr = (vuint8 *)sp[-1].p;
          --(*ptr);
          sp[-1].i = *ptr;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_BytePostInc)
        ++ip;
        {
          vuint8 *ptr = (vuint8 *)sp[-1].p;
          sp[-1].i = *ptr;
          (*ptr)++;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_BytePostDec)
        ++ip;
        {
          vuint8 *ptr = (vuint8 *)sp[-1].p;
          sp[-1].i = *ptr;
          (*ptr)--;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteIncDrop)
        ++ip;
        (*(vuint8 *)sp[-1].p)++;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteDecDrop)
        ++ip;
        (*(vuint8 *)sp[-1].p)--;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteAssignDrop)
        ASSIGNOP(vuint8, i, =);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteAddVarDrop)
        ASSIGNOP(vuint8, i, +=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteSubVarDrop)
        ASSIGNOP(vuint8, i, -=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteMulVarDrop)
        ASSIGNOP(vuint8, i, *=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteDivVarDrop)
        if (!sp[-1].i) { cstDump(ip); Sys_Error("Division by 0"); }
        ASSIGNOP(vuint8, i, /=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteModVarDrop)
        if (!sp[-1].i) { cstDump(ip); Sys_Error("Division by 0"); }
        ASSIGNOP(vuint8, i, %=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteAndVarDrop)
        ASSIGNOP(vuint8, i, &=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteOrVarDrop)
        ASSIGNOP(vuint8, i, |=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteXOrVarDrop)
        ASSIGNOP(vuint8, i, ^=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteLShiftVarDrop)
        ASSIGNOP(vuint8, i, <<=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteRShiftVarDrop)
        ASSIGNOP(vuint8, i, >>=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FAdd)
        BINOP_Q(f, +=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FSubtract)
        BINOP_Q(f, -=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FMultiply)
        BINOP_Q(f, *=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FDivide)
        if (!sp[-1].f) { cstDump(ip); Sys_Error("Division by 0"); }
        BINOP_Q(f, /=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FEquals)
        BOOLOP(f, ==);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FNotEquals)
        BOOLOP(f, !=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FLess)
        BOOLOP(f, <);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FGreater)
        BOOLOP(f, >);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FLessEquals)
        BOOLOP(f, <=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FGreaterEquals)
        BOOLOP(f, >=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FUnaryMinus)
        ++ip;
        sp[-1].f = -sp[-1].f;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FAddVarDrop)
        ASSIGNOP(float, f, +=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FSubVarDrop)
        ASSIGNOP(float, f, -=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FMulVarDrop)
        ASSIGNOP(float, f, *=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FDivVarDrop)
        ASSIGNOP(float, f, /=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VAdd)
        ++ip;
        sp[-6].f += sp[-3].f;
        sp[-5].f += sp[-2].f;
        sp[-4].f += sp[-1].f;
        sp -= 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VSubtract)
        ++ip;
        sp[-6].f -= sp[-3].f;
        sp[-5].f -= sp[-2].f;
        sp[-4].f -= sp[-1].f;
        sp -= 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VPreScale)
        {
          ++ip;
          float scale = sp[-4].f;
          sp[-4].f = scale*sp[-3].f;
          sp[-3].f = scale*sp[-2].f;
          sp[-2].f = scale*sp[-1].f;
          --sp;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VPostScale)
        ++ip;
        sp[-4].f *= sp[-1].f;
        sp[-3].f *= sp[-1].f;
        sp[-2].f *= sp[-1].f;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VIScale)
        ++ip;
        sp[-4].f /= sp[-1].f;
        sp[-3].f /= sp[-1].f;
        sp[-2].f /= sp[-1].f;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VEquals)
        ++ip;
        sp[-6].i = (sp[-6].f == sp[-3].f && sp[-5].f == sp[-2].f && sp[-4].f == sp[-1].f);
        sp -= 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VNotEquals)
        ++ip;
        sp[-6].i = (sp[-6].f != sp[-3].f || sp[-5].f != sp[-2].f || sp[-4].f != sp[-1].f);
        sp -= 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VUnaryMinus)
        ++ip;
        sp[-3].f = -sp[-3].f;
        sp[-2].f = -sp[-2].f;
        sp[-1].f = -sp[-1].f;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VFixParam)
        {
          vint32 Idx = ip[1];
          ip += 2;
          TVec *v = (TVec *)&local_vars[Idx];
          v->y = local_vars[Idx+1].f;
          v->z = local_vars[Idx+2].f;
        }
        PR_VM_BREAK;

    #define VASSIGNOP(op) \
      { \
        ++ip; \
        TVec *ptr = (TVec *)sp[-4].p; \
        ptr->x op sp[-3].f; \
        ptr->y op sp[-2].f; \
        ptr->z op sp[-1].f; \
        sp -= 4; \
      }

      PR_VM_CASE(OPC_VAssignDrop)
        VASSIGNOP(=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VAddVarDrop)
        VASSIGNOP(+=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VSubVarDrop)
        VASSIGNOP(-=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VScaleVarDrop)
        ++ip;
        *(TVec *)sp[-2].p *= sp[-1].f;
        sp -= 2;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VIScaleVarDrop)
        ++ip;
        *(TVec *)sp[-2].p /= sp[-1].f;
        sp -= 2;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FloatToBool)
        sp[-1].i = (sp[-1].f == 0 ? 0 : 1);
        ++ip;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VectorToBool)
        sp[-3].i = (sp[-1].f == 0 && sp[-2].f == 0 && sp[-3].f == 0 ? 0 : 1);
        sp -= 2;
        ++ip;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_StrToBool)
        {
          ++ip;
          bool Val = ((VStr *)&sp[-1].p)->IsNotEmpty();
          ((VStr *)&sp[-1].p)->Clean();
          sp[-1].i = Val;
        }
        PR_VM_BREAK;

    #define STRCMPOP(cmpop) \
      { \
        ++ip; \
        int cmp = ((VStr *)&sp[-2].p)->Cmp(*((VStr *)&sp[-1].p)); \
        ((VStr *)&sp[-2].p)->Clean(); \
        ((VStr *)&sp[-1].p)->Clean(); \
        sp -= 1; \
        sp[-1].i = (cmp cmpop 0); \
      }

      PR_VM_CASE(OPC_StrEquals)
        //STRCMPOP(==)
        // the following is slightly faster for the same strings
        {
          ++ip;
          bool cmp = *((VStr *)&sp[-2].p) == *((VStr *)&sp[-1].p);
          ((VStr *)&sp[-2].p)->Clean();
          ((VStr *)&sp[-1].p)->Clean();
          sp -= 1;
          sp[-1].i = int(cmp);
        }
        PR_VM_BREAK;
      PR_VM_CASE(OPC_StrNotEquals)
        //STRCMPOP(!=)
        // the following is slightly faster for the same strings
        {
          ++ip;
          bool cmp = *((VStr *)&sp[-2].p) != *((VStr *)&sp[-1].p);
          ((VStr *)&sp[-2].p)->Clean();
          ((VStr *)&sp[-1].p)->Clean();
          sp -= 1;
          sp[-1].i = int(cmp);
        }
        PR_VM_BREAK;
      PR_VM_CASE(OPC_StrLess)
        STRCMPOP(<)
        PR_VM_BREAK;
      PR_VM_CASE(OPC_StrLessEqu)
        STRCMPOP(<=)
        PR_VM_BREAK;
      PR_VM_CASE(OPC_StrGreat)
        STRCMPOP(>)
        PR_VM_BREAK;
      PR_VM_CASE(OPC_StrGreatEqu)
        STRCMPOP(>=)
        PR_VM_BREAK;

      PR_VM_CASE(OPC_StrLength)
        {
          ++ip;
          auto len = (*((VStr **)&sp[-1].p))->Length();
          //((VStr *)&sp[-1].p)->Clean();
          sp[-1].i = (int)len;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_StrCat)
        {
          ++ip;
          VStr s = *((VStr *)&sp[-2].p)+*((VStr *)&sp[-1].p);
          ((VStr *)&sp[-2].p)->Clean();
          ((VStr *)&sp[-1].p)->Clean();
          sp -= 1;
          *((VStr *)&sp[-1].p) = s;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_StrGetChar)
        {
          ++ip;
          int v = sp[-1].i;
          v = (v >= 0 && v < ((VStr *)&sp[-2].p)->length() ? (vuint8)(*((VStr *)&sp[-2].p))[v] : 0);
          ((VStr *)&sp[-2].p)->Clean();
          sp -= 1;
          sp[-1].i = v;
        }
        PR_VM_BREAK;

      // [-1]: char
      // [-2]: index
      // [-3]: *string
      PR_VM_CASE(OPC_StrSetChar)
        {
          ++ip;
          vuint8 ch = sp[-1].i&0xff;
          int idx = sp[-2].i;
          VStr *s = *(VStr **)&sp[-3].p;
          if (idx >= 0 && idx < (int)(s->length())) {
            char *data = s->GetMutableCharPointer(idx);
            *data = ch;
          }
          sp -= 3; // drop it all
        }
        PR_VM_BREAK;

      // [-1]: hi
      // [-2]: lo
      // [-3]: string
      PR_VM_CASE(OPC_StrSlice)
        {
          ++ip;
          int hi = sp[-1].i;
          int lo = sp[-2].i;
          VStr *s = (VStr *)&sp[-3].p;
          sp -= 2; // drop limits
          if (lo < 0 || hi <= lo || lo >= (int)s->length()) {
            s->clear();
          } else {
            if (hi > (int)s->length()) hi = (int)s->length();
            VStr ns = s->mid(lo, hi-lo);
            s->clear();
            *s = ns;
          }
        }
        PR_VM_BREAK;

      // [-1]: newstr
      // [-2]: hi
      // [-3]: lo
      // [-4]: *string
      // res: string
      PR_VM_CASE(OPC_StrSliceAssign)
        {
          ++ip;
          int hi = sp[-2].i;
          int lo = sp[-3].i;
          VStr ns = *(VStr *)&sp[-1].p;
          VStr *s = *(VStr **)&sp[-4].p;
          sp -= 4; // drop everything
          if (lo < 0 || hi <= lo || lo >= (int)s->length()) {
            // do nothing
          } else {
            if (hi > (int)s->length()) hi = (int)s->length();
            // get left part
            VStr ds = (lo > 0 ? s->mid(0, lo) : VStr());
            // append middle part
            ds += ns;
            // append right part
            if (hi < (int)s->length()) ds += s->mid(hi, (int)s->length()-hi);
            s->clear();
            *s = ds;
          }
        }
        PR_VM_BREAK;

      // [-1]: hi
      // [-2]: lo
      // [-3]: sliceptr
      // res: slice (ptr, len)
      PR_VM_CASE(OPC_SliceSlice)
        if (sp[-3].p) {
          int hi = sp[-1].i;
          int lo = sp[-2].i;
          VCSlice vs = *(VCSlice *)sp[-3].p;
          if (!vs.ptr) vs.length = 0; else if (vs.length < 0) vs.length = 0; // just in case
          if (lo < 0 || hi < lo || lo > vs.length || hi > vs.length) {
            cstDump(ip);
            Sys_Error("Slice [%d..%d] is out of range (%d)", lo, hi, vs.length);
          } else {
            sp -= 1; // drop one unused limit
            // push slice
            sp[-2].p = vs.ptr+lo*ReadInt32(ip+1);
            sp[-1].i = hi-lo;
          }
        } else {
          cstDump(ip); Sys_Error("Cannot operate on none-slice");
        }
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_AssignStrDrop)
        ++ip;
        *(VStr *)sp[-2].p = *(VStr *)&sp[-1].p;
        ((VStr *)&sp[-1].p)->Clean();
        sp -= 2;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PtrEquals)
        BOOLOP(p, ==);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PtrNotEquals)
        BOOLOP(p, !=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PtrToBool)
        ++ip;
        sp[-1].i = !!sp[-1].p;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IntToFloat)
        ++ip;
        ftemp = (float)sp[-1].i;
        sp[-1].f = ftemp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FloatToInt)
        ++ip;
        itemp = (vint32)sp[-1].f;
        sp[-1].i = itemp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_StrToName)
        {
          ++ip;
          VName newname = VName(**((VStr *)&sp[-1].p));
          ((VStr *)&sp[-1].p)->Clean();
          sp[-1].i = newname.GetIndex();
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_NameToStr)
        {
          ++ip;
          VName n = VName((EName)sp[-1].i);
          sp[-1].p = nullptr;
          *(VStr *)&sp[-1].p = VStr(*n);
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ClearPointedStr)
        ((VStr *)sp[-1].p)->Clean();
        ip += 1;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ClearPointedStruct)
        ((VStruct *)ReadPtr(ip+1))->DestructObject((byte *)sp[-1].p);
        ip += 1+sizeof(void *);
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ZeroPointedStruct)
        ((VStruct *)ReadPtr(ip+1))->ZeroObject((byte *)sp[-1].p);
        ip += 1+sizeof(void *);
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Drop)
        ++ip;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VDrop)
        ++ip;
        sp -= 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DropStr)
        ++ip;
        ((VStr *)&sp[-1].p)->Clean();
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_AssignPtrDrop)
        ASSIGNOP(void *, p, =);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_AssignBool0)
        {
          vuint32 mask = ip[1];
          if (sp[-1].i) {
            *(vint32 *)sp[-2].p |= mask;
          } else {
            *(vint32 *)sp[-2].p &= ~mask;
          }
          ip += 2;
          sp -= 2;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_AssignBool1)
        {
          vuint32 mask = ip[1]<<8;
          if (sp[-1].i) {
            *(vint32 *)sp[-2].p |= mask;
          } else {
            *(vint32 *)sp[-2].p &= ~mask;
          }
          ip += 2;
          sp -= 2;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_AssignBool2)
        {
          vuint32 mask = ip[1]<<16;
          if (sp[-1].i) {
            *(vint32 *)sp[-2].p |= mask;
          } else {
            *(vint32 *)sp[-2].p &= ~mask;
          }
          ip += 2;
          sp -= 2;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_AssignBool3)
        {
          vuint32 mask = ip[1]<<24;
          if (sp[-1].i) {
            *(vint32 *)sp[-2].p |= mask;
          } else {
            *(vint32 *)sp[-2].p &= ~mask;
          }
          ip += 2;
          sp -= 2;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_AssignDelegate)
        ++ip;
        ((void **)sp[-3].p)[0] = sp[-2].p;
        ((void **)sp[-3].p)[1] = sp[-1].p;
        sp -= 3;
        PR_VM_BREAK;

      // [-2]: *dynarray
      // [-1]: index
      PR_VM_CASE(OPC_DynArrayElement)
        if (sp[-1].i < 0 || sp[-1].i >= ((VScriptArray *)sp[-2].p)->Num()) {
          cstDump(ip);
          Sys_Error("Index %d outside the bounds of an array (%d)", sp[-1].i, ((VScriptArray *)sp[-2].p)->Num());
        }
        sp[-2].p = ((VScriptArray *)sp[-2].p)->Ptr()+sp[-1].i*ReadInt32(ip+1);
        ip += 5;
        --sp;
        PR_VM_BREAK;

      // [-2]: *dynarray
      // [-1]: index
      PR_VM_CASE(OPC_DynArrayElementB)
        if (sp[-1].i < 0 || sp[-1].i >= ((VScriptArray *)sp[-2].p)->Num()) {
          cstDump(ip);
          Sys_Error("Index %d outside the bounds of an array (%d)", sp[-1].i, ((VScriptArray *)sp[-2].p)->Num());
        }
        sp[-2].p = ((VScriptArray *)sp[-2].p)->Ptr()+sp[-1].i*ip[1];
        ip += 2;
        --sp;
        PR_VM_BREAK;

      // [-2]: *dynarray
      // [-1]: index
      // pointer to a new element left on the stack
      PR_VM_CASE(OPC_DynArrayElementGrow)
        {
          VFieldType Type;
          ++ip;
          ReadType(Type, ip);
          //ip += 9+sizeof(VClass *);
          VScriptArray &A = *(VScriptArray *)sp[-2].p;
          int idx = sp[-1].i;
          if (idx < 0) { cstDump(ip); Sys_Error("Array index %d is negative", idx); }
          if (idx >= A.Num()) {
            if (A.Is2D()) { cstDump(ip); Sys_Error("Cannot grow 2D array"); }
            if (idx >= MaxDynArrayLength) { cstDump(ip); Sys_Error("Array index %d is too big", idx); }
            A.SetNum(idx+1, Type);
          }
          sp[-2].p = A.Ptr()+idx*Type.GetSize();
          --sp;
        }
        PR_VM_BREAK;

      // [-1]: *dynarray
      PR_VM_CASE(OPC_DynArrayGetNum)
        ++ip;
        sp[-1].i = ((VScriptArray *)sp[-1].p)->Num();
        PR_VM_BREAK;

      // [-1]: *dynarray
      PR_VM_CASE(OPC_DynArrayGetNum1)
        ++ip;
        sp[-1].i = ((VScriptArray *)sp[-1].p)->length1();
        PR_VM_BREAK;

      // [-1]: *dynarray
      PR_VM_CASE(OPC_DynArrayGetNum2)
        ++ip;
        sp[-1].i = ((VScriptArray *)sp[-1].p)->length2();
        PR_VM_BREAK;

      // [-2]: *dynarray
      // [-1]: length
      PR_VM_CASE(OPC_DynArraySetNum)
        {
          VScriptArray &A = *(VScriptArray *)sp[-2].p;
          int newsize = sp[-1].i;
          // allow clearing for 2d arrays
          if (A.Is2D() && newsize != 0) { cstDump(ip); Sys_Error("Cannot resize 2D array"); }
          VFieldType Type;
          ++ip;
          ReadType(Type, ip);
          if (newsize < 0) { cstDump(ip); Sys_Error("Array index %d is negative", newsize); }
          if (newsize > MaxDynArrayLength) { cstDump(ip); Sys_Error("Array index %d is too big", newsize); }
          A.SetNum(newsize, Type);
          sp -= 2;
        }
        PR_VM_BREAK;

      // [-2]: *dynarray
      // [-1]: delta
      PR_VM_CASE(OPC_DynArraySetNumMinus)
        {
          VScriptArray &A = *(VScriptArray *)sp[-2].p;
          int newsize = sp[-1].i;
          // allow clearing for 2d arrays
          if (A.Is2D() && newsize != 0 && newsize != A.length()) { cstDump(ip); Sys_Error("Cannot resize 2D array"); }
          VFieldType Type;
          ++ip;
          ReadType(Type, ip);
          if (newsize < 0) { cstDump(ip); Sys_Error("Array shrink delta %d is negative", newsize); }
          if (newsize > MaxDynArrayLength) { cstDump(ip); Sys_Error("Array shrink delta %d is too big", newsize); }
          if (A.length() < newsize) { cstDump(ip); Sys_Error("Array shrink delta %d is too big (%d)", newsize, A.length()); }
          if (newsize > 0) A.SetNumMinus(newsize, Type);
          sp -= 2;
        }
        PR_VM_BREAK;

      // [-2]: *dynarray
      // [-1]: delta
      PR_VM_CASE(OPC_DynArraySetNumPlus)
        {
          VScriptArray &A = *(VScriptArray *)sp[-2].p;
          int newsize = sp[-1].i;
          // allow clearing for 2d arrays
          if (A.Is2D() && newsize != 0 && newsize != A.length()) { cstDump(ip); Sys_Error("Cannot resize 2D array"); }
          VFieldType Type;
          ++ip;
          ReadType(Type, ip);
          if (newsize < 0) { cstDump(ip); Sys_Error("Array grow delta %d is negative", newsize); }
          if (newsize > MaxDynArrayLength) { cstDump(ip); Sys_Error("Array grow delta %d is too big", newsize); }
          if (A.length() > MaxDynArrayLength || MaxDynArrayLength-A.length() < newsize) { cstDump(ip); Sys_Error("Array grow delta %d is too big (%d)", newsize, A.length()); }
          if (newsize > 0) A.SetNumPlus(newsize, Type);
          sp -= 2;
        }
        PR_VM_BREAK;

      // [-3]: *dynarray
      // [-2]: index
      // [-1]: count
      PR_VM_CASE(OPC_DynArrayInsert)
        {
          VScriptArray &A = *(VScriptArray *)sp[-3].p;
          if (A.Is2D()) { cstDump(ip); Sys_Error("Cannot insert into 2D array"); }
          int index = sp[-2].i;
          int count = sp[-1].i;
          VFieldType Type;
          ++ip;
          ReadType(Type, ip);
          if (count < 0) { cstDump(ip); Sys_Error("Array count %d is negative", count); }
          if (index < 0) { cstDump(ip); Sys_Error("Array index %d is negative", index); }
          if (index > A.length()) { cstDump(ip); Sys_Error("Index %d outside the bounds of an array (%d)", index, A.length()); }
          if (A.length() > MaxDynArrayLength || MaxDynArrayLength-A.length() < count) { cstDump(ip); Sys_Error("Out of memory for dynarray"); }
          if (count > 0) A.Insert(index, count, Type);
          sp -= 3;
        }
        PR_VM_BREAK;

      // [-3]: *dynarray
      // [-2]: index
      // [-1]: count
      PR_VM_CASE(OPC_DynArrayRemove)
        {
          VScriptArray &A = *(VScriptArray *)sp[-3].p;
          if (A.Is2D()) { cstDump(ip); Sys_Error("Cannot insert into 2D array"); }
          int index = sp[-2].i;
          int count = sp[-1].i;
          VFieldType Type;
          ++ip;
          ReadType(Type, ip);
          if (count < 0) { cstDump(ip); Sys_Error("Array count %d is negative", count); }
          if (index < 0) { cstDump(ip); Sys_Error("Array index %d is negative", index); }
          if (index > A.length()) { cstDump(ip); Sys_Error("Index %d outside the bounds of an array (%d)", index, A.length()); }
          if (count > A.length()-index) { cstDump(ip); Sys_Error("Array count %d is too big at %d (%d)", count, index, A.length()); }
          if (count > 0) A.Remove(index, count, Type);
          sp -= 3;
        }
        PR_VM_BREAK;

      // [-2]: *dynarray
      // [-1]: size
      PR_VM_CASE(OPC_DynArraySetSize1D)
        {
          VScriptArray &A = *(VScriptArray *)sp[-2].p;
          int newsize = sp[-1].i;
          VFieldType Type;
          ++ip;
          ReadType(Type, ip);
          if (newsize < 0) { cstDump(ip); Sys_Error("Array size %d is negative", newsize); }
          if (newsize > MaxDynArrayLength) { cstDump(ip); Sys_Error("Array size %d is too big", newsize); }
          A.SetNum(newsize, Type); // this will flatten it
          sp -= 2;
        }
        PR_VM_BREAK;

      // [-3]: *dynarray
      // [-2]: size1
      // [-1]: size2
      PR_VM_CASE(OPC_DynArraySetSize2D)
        {
          VScriptArray &A = *(VScriptArray *)sp[-3].p;
          int newsize1 = sp[-2].i;
          int newsize2 = sp[-1].i;
          VFieldType Type;
          ++ip;
          ReadType(Type, ip);
          if (newsize1 < 1) { cstDump(ip); Sys_Error("Array size %d is too small", newsize1); }
          if (newsize1 > MaxDynArrayLength) { cstDump(ip); Sys_Error("Array size %d is too big", newsize1); }
          if (newsize2 < 1) { cstDump(ip); Sys_Error("Array size %d is too small", newsize2); }
          if (newsize2 > MaxDynArrayLength) { cstDump(ip); Sys_Error("Array size %d is too big", newsize2); }
          A.SetSize2D(newsize1, newsize2, Type);
          sp -= 3;
        }
        PR_VM_BREAK;

      // [-3]: *dynarray
      // [-2]: index1
      // [-1]: index2
      // pointer to a new element left on the stack
      PR_VM_CASE(OPC_DynArrayElement2D)
        {
          VScriptArray &A = *(VScriptArray *)sp[-3].p;
          int idx1 = sp[-2].i;
          int idx2 = sp[-1].i;
          // 1d arrays can be accessed as 2d if second index is 0
          if (idx2 != 0 && !A.Is2D()) { cstDump(ip); Sys_Error("Cannot index 1D array as 2D"); }
          if (idx1 < 0) { cstDump(ip); Sys_Error("Array index %d is too small", idx1); }
          if (idx1 >= A.length1()) { cstDump(ip); Sys_Error("Array index %d is too big (%d)", idx1, A.length1()); }
          if (idx2 < 0) { cstDump(ip); Sys_Error("Array size %d is too small", idx2); }
          if (idx2 >= A.length2()) { cstDump(ip); Sys_Error("Array size %d is too big (%d)", idx2, A.length2()); }
          sp[-3].p = A.Ptr()+(idx2*A.length1()+idx1)*ReadInt32(ip+1);
          ip += 5;
          sp -= 2;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DynamicCast)
        sp[-1].p = (sp[-1].p && ((VObject *)sp[-1].p)->IsA((VClass *)ReadPtr(ip+1)) ? sp[-1].p : nullptr);
        ip += 1+sizeof(void *);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DynamicClassCast)
        sp[-1].p = (sp[-1].p && ((VClass *)sp[-1].p)->IsChildOf((VClass *)ReadPtr(ip+1)) ? sp[-1].p : nullptr);
        ip += 1+sizeof(void *);
        PR_VM_BREAK;

      // [-2]: what to cast
      // [-1]: destination class
      PR_VM_CASE(OPC_DynamicCastIndirect)
        sp[-2].p = (sp[-1].p && sp[-2].p && ((VObject *)sp[-2].p)->IsA((VClass *)sp[-1].p) ? sp[-2].p : nullptr);
        --sp;
        ++ip;
        PR_VM_BREAK;

      // [-2]: what to cast
      // [-1]: destination class
      PR_VM_CASE(OPC_DynamicClassCastIndirect)
        sp[-2].p = (sp[-1].p && sp[-2].p && ((VClass *)sp[-2].p)->IsChildOf((VClass *)sp[-1].p) ? sp[-2].p : nullptr);
        --sp;
        ++ip;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_GetDefaultObj)
        ++ip;
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].p = ((VObject *)sp[-1].p)->GetClass()->Defaults;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_GetClassDefaultObj)
        ++ip;
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].p = ((VClass *)sp[-1].p)->Defaults;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IteratorInit)
        if (itsp >= MAX_ITER_STACK) { cstDump(ip); Sys_Error("Too many nested `foreach`"); }
        ++ip;
        //((VScriptIterator *)sp[-1].p)->Next = ActiveIterators;
        //ActiveIterators = (VScriptIterator *)sp[-1].p;
        itstack[itsp].it = (VScriptIterator *)sp[-1].p;
        itstack[itsp].doneip = nullptr;
        ++itsp;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IteratorNext)
        if (itsp == 0) { cstDump(ip); Sys_Error("VM: No active iterators (but we should have one)"); }
        if (!itstack[itsp-1].it) { cstDump(ip); Sys_Error("VM: Active iterator is not native (but it should be)"); }
        ++ip;
        //checkSlow(ActiveIterators);
        //sp->i = ActiveIterators->GetNext();
        sp->i = itstack[itsp-1].it->GetNext();
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IteratorPop)
        if (itsp == 0) { cstDump(ip); Sys_Error("VM: No active iterators (but we should have one)"); }
        if (!itstack[itsp-1].it) { cstDump(ip); Sys_Error("VM: Active iterator is not native (but it should be)"); }
        //delete itstack[itsp-1].it;
        itstack[itsp-1].it->Finished();
        --itsp;
        ++ip;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IteratorDtorAt)
        if (itsp >= MAX_ITER_STACK) { cstDump(ip); Sys_Error("Too many nested `foreach`"); }
        itstack[itsp].it = nullptr;
        itstack[itsp].doneip = ip+ReadInt32(ip+1);
        ++itsp;
        ip += 1+4;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IteratorFinish)
        if (itsp == 0) { cstDump(ip); Sys_Error("VM: No active iterators (but we should have one)"); }
        if (!itstack[itsp-1].doneip) { cstDump(ip); Sys_Error("VM: Active iterator is native (but it should not be)"); }
        --itsp;
        if (inReturn) goto doRealReturnItDtorCont;
        ++ip;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DoWriteOne)
        {
          VFieldType Type;
          ++ip;
          ReadType(Type, ip);
          //ip += 9+sizeof(VClass *);
          pr_stackPtr = sp;
          PR_WriteOne(Type); // this will pop everything
          if (pr_stackPtr < pr_stack) { pr_stackPtr = pr_stack; cstDump(ip); Sys_Error("Stack underflow in `write`"); }
          sp = pr_stackPtr;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DoWriteFlush)
        ++ip;
        PR_WriteFlush();
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DoPushTypePtr)
        ++ip;
        sp->p = ip;
        sp += 1;
        // skip type
        //VFieldType Type;
        //ReadType(Type, ip);
        ip += 8+sizeof(VClass *);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ZeroByPtr)
        if (sp[-1].p) {
          int bcount = ReadInt32(ip+1);
          if (bcount > 0) memset(sp[-1].p, 0, bcount);
        }
        --sp;
        ip += 1+4;
        PR_VM_BREAK;

      // [-1]: obj
      PR_VM_CASE(OPC_GetObjClassPtr)
        if (sp[-1].p) sp[-1].p = ((VObject *)sp[-1].p)->GetClass();
        ++ip;
        PR_VM_BREAK;

      // [-2]: class
      // [-1]: class
      PR_VM_CASE(OPC_ClassIsAClass)
        if (sp[-2].p && sp[-1].p) {
          //VClass *c = ((VObject *)sp[-2].p)->GetClass();
          VClass *c = (VClass *)sp[-2].p;
          sp[-2].i = (c->IsChildOf((VClass *)sp[-1].p) ? 1 : 0);
        } else if (sp[-2].p || sp[-1].p) {
          // class isa none is false
          // none isa class is false
          sp[-2].i = 0;
        } else {
          // none isa none is true
          sp[-2].i = 1;
        }
        --sp;
        ++ip;
        PR_VM_BREAK;

      // [-2]: class
      // [-1]: class
      PR_VM_CASE(OPC_ClassIsNotAClass)
        if (sp[-2].p && sp[-1].p) {
          //VClass *c = ((VObject *)sp[-2].p)->GetClass();
          VClass *c = (VClass *)sp[-2].p;
          sp[-2].i = (c->IsChildOf((VClass *)sp[-1].p) ? 0 : 1);
        } else if (sp[-2].p || sp[-1].p) {
          // class !isa none is true
          // none !isa class is true
          sp[-2].i = 1;
        } else {
          // none !isa none is false
          sp[-2].i = 0;
        }
        --sp;
        ++ip;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Builtin)
        switch (ReadU8(ip+1)) {
          case OPC_Builtin_IntAbs: if (sp[-1].i < 0) sp[-1].i = -sp[-1].i; break;
          case OPC_Builtin_FloatAbs: if (sp[-1].f < 0) sp[-1].f = -sp[-1].f; break;
          case OPC_Builtin_IntSign: if (sp[-1].i < 0) sp[-1].i = -1; else if (sp[-1].i > 0) sp[-1].i = 1; break;
          case OPC_Builtin_FloatSign: if (sp[-1].f < 0) sp[-1].f = -1; else if (sp[-1].f > 0) sp[-1].f = 1; else sp[-1].f = 0; break;
          case OPC_Builtin_IntMin: if (sp[-2].i > sp[-1].i) sp[-2].i = sp[-1].i; sp -= 1; break;
          case OPC_Builtin_IntMax: if (sp[-2].i < sp[-1].i) sp[-2].i = sp[-1].i; sp -= 1; break;
          case OPC_Builtin_FloatMin: if (sp[-2].f > sp[-1].f) sp[-2].f = sp[-1].f; sp -= 1; break;
          case OPC_Builtin_FloatMax: if (sp[-2].f < sp[-1].f) sp[-2].f = sp[-1].f; sp -= 1; break;
          case OPC_Builtin_IntClamp: sp[-3].i = MID(sp[-2].i, sp[-3].i, sp[-1].i); sp -= 2; break;
          case OPC_Builtin_FloatClamp: sp[-3].f = MID(sp[-2].f, sp[-3].f, sp[-1].f); sp -= 2; break;
          case OPC_Builtin_FloatIsNaN: sp[-1].i = (isNaNF(sp[-1].f) ? 1 : 0); break;
          case OPC_Builtin_FloatIsInf: sp[-1].i = (isInfF(sp[-1].f) ? 1 : 0); break;
          case OPC_Builtin_FloatIsFinite: sp[-1].i = (isFiniteF(sp[-1].f) ? 1 : 0); break;
          case OPC_Builtin_DegToRad: sp[-1].f = DEG2RAD(sp[-1].f); break;
          case OPC_Builtin_RadToDeg: sp[-1].f = RAD2DEG(sp[-1].f); break;
          case OPC_Builtin_Sin: sp[-1].f = msin(sp[-1].f); break;
          case OPC_Builtin_Cos: sp[-1].f = mcos(sp[-1].f); break;
          case OPC_Builtin_Tan: sp[-1].f = mtan(sp[-1].f); break;
          case OPC_Builtin_ASin: sp[-1].f = masin(sp[-1].f); break;
          case OPC_Builtin_ACos: sp[-1].f = acos(sp[-1].f); break;
          case OPC_Builtin_ATan: sp[-1].f = RAD2DEG(atan(sp[-1].f)); break;
          case OPC_Builtin_Sqrt: sp[-1].f = sqrt(sp[-1].f); break;
          case OPC_Builtin_ATan2: sp[-2].f = matan(sp[-2].f, sp[-1].f); --sp; break;
          case OPC_Builtin_VecLength: sp[-3].f = sqrt(sp[-1].f*sp[-1].f+sp[-2].f*sp[-2].f+sp[-3].f*sp[-3].f); sp -= 2; break;
          case OPC_Builtin_VecLength2D: sp[-3].f = sqrt(sp[-2].f*sp[-2].f+sp[-3].f*sp[-3].f); sp -= 2; break;
          case OPC_Builtin_VecNormalize:
            {
              TVec v(sp[-3].f, sp[-2].f, sp[-1].f);
              v = normalise(v);
              sp[-1].f = v.z;
              sp[-2].f = v.y;
              sp[-3].f = v.x;
              break;
            }
          case OPC_Builtin_VecNormalize2D:
            {
              TVec v(sp[-3].f, sp[-2].f, sp[-1].f);
              v = normalise2D(v);
              sp[-1].f = v.z;
              sp[-2].f = v.y;
              sp[-3].f = v.x;
              break;
            }
          case OPC_Builtin_VecDot:
            {
              TVec v2(sp[-3].f, sp[-2].f, sp[-1].f);
              sp -= 3;
              TVec v1(sp[-3].f, sp[-2].f, sp[-1].f);
              sp -= 2;
              sp[-1].f = DotProduct(v1, v2);
              break;
            }
          case OPC_Builtin_VecDot2D:
            {
              TVec v2(sp[-3].f, sp[-2].f, sp[-1].f);
              sp -= 3;
              TVec v1(sp[-3].f, sp[-2].f, sp[-1].f);
              sp -= 2;
              sp[-1].f = DotProduct2D(v1, v2);
              break;
            }
          case OPC_Builtin_VecCross:
            {
              TVec v2(sp[-3].f, sp[-2].f, sp[-1].f);
              sp -= 3;
              TVec v1(sp[-3].f, sp[-2].f, sp[-1].f);
              v1 = CrossProduct(v1, v2);
              sp[-1].f = v1.z;
              sp[-2].f = v1.y;
              sp[-3].f = v1.x;
              break;
            }
          case OPC_Builtin_VecCross2D:
            {
              TVec v2(sp[-3].f, sp[-2].f, sp[-1].f);
              sp -= 3;
              TVec v1(sp[-3].f, sp[-2].f, sp[-1].f);
              sp -= 2;
              sp[-1].f = CrossProduct2D(v1, v2);
              break;
            }
          case OPC_Builtin_RoundF2I: sp[-1].i = (int)(roundf(sp[-1].f)); break;
          case OPC_Builtin_RoundF2F: sp[-1].f = roundf(sp[-1].f); break;
          case OPC_Builtin_TruncF2I: sp[-1].i = (int)(truncf(sp[-1].f)); break;
          case OPC_Builtin_TruncF2F: sp[-1].f = truncf(sp[-1].f); break;
          case OPC_Builtin_FloatCeil: sp[-1].f = ceilf(sp[-1].f); break;
          case OPC_Builtin_FloatFloor: sp[-1].f = floorf(sp[-1].f); break;
          // [-3]: a; [-2]: b, [-1]: delta
          case OPC_Builtin_FloatLerp: sp[-3].f = sp[-3].f+(sp[-2].f-sp[-3].f)*sp[-1].f; sp -= 2; break;
          case OPC_Builtin_IntLerp: sp[-3].i = (int)roundf(sp[-3].i+(sp[-2].i-sp[-3].i)*sp[-1].f); sp -= 2; break;
          case OPC_Builtin_FloatSmoothStep: sp[-3].f = smoothstep(sp[-3].f, sp[-2].f, sp[-1].f); sp -= 2; break;
          case OPC_Builtin_FloatSmoothStepPerlin: sp[-3].f = smoothstepPerlin(sp[-3].f, sp[-2].f, sp[-1].f); sp -= 2; break;
          case OPC_Builtin_NameToInt: break; // no, really, it is THAT easy
          default: cstDump(ip); Sys_Error("Unknown builtin");
        }
        ip += 2;
        PR_VM_BREAK;

      PR_VM_DEFAULT
        cstDump(ip);
        Sys_Error("Invalid opcode %d", *ip);
    }
  }
  goto func_loop;

doRealReturn:
  #ifdef VMEXEC_RUNDUMP
  printIndent(); printf("LEAVING VC FUNCTION `%s`; sp=%d\n", *func->GetFullName(), (int)(sp-pr_stack)); leaveIndent();
  #endif
  if (itsp == 0) { cstDump(ip); Sys_Error("VM: Return that should not be"); }
  inReturn = true;
  // kill iterators
doRealReturnItDtorCont:
  while (itsp > 0) {
    ItStackItem &it = itstack[itsp-1];
    // check iterator type
    if (it.it) {
      // native
      //delete it.it;
      it.it->Finished();
      --itsp;
    } else {
      // execute dtor code
      ip = it.doneip;
      //auto oldpstack = pr_stackPtr;
      //sp = pr_stackPtr;
      goto func_loop;
    }
  }

  // set return value
  switch (retSize) {
    case 0:
      checkSlow(sp == local_vars+func->NumLocals);
      pr_stackPtr = local_vars;
      break;
    case 1:
      checkSlow(sp == local_vars+func->NumLocals+1);
      ((VStack *)local_vars)[0] = sp[-1];
      pr_stackPtr = local_vars+1;
      break;
    case 3:
      checkSlow(sp == local_vars+func->NumLocals+3);
      ((VStack *)local_vars)[0] = sp[-3];
      ((VStack *)local_vars)[1] = sp[-2];
      ((VStack *)local_vars)[2] = sp[-1];
      pr_stackPtr = local_vars+3;
      break;
    default: { cstDump(ip); Sys_Error("VM: Invalid return size"); }
  }
  cstPop();

  unguardf(("(%s %d)", *func->GetFullName(), (int)(ip-func->Statements.Ptr())));
}


//==========================================================================
//
//  VObject::ExecuteFunction
//
//==========================================================================
VStack VObject::ExecuteFunction (VMethod *func) {
  guard(VObject::ExecuteFunction);

  VMethod *prev_func;
  VStack ret;

  ret.i = 0;
  ret.p = nullptr;
  // run function
  prev_func = current_func;
#ifdef VMEXEC_RUNDUMP
  enterIndent(); printIndent(); printf("***ENTERING `%s` (RETx); sp=%d (MAX:%u)\n", *func->GetFullName(), (int)(pr_stackPtr-pr_stack), (unsigned)MAX_PROG_STACK);
#endif
  RunFunction(func);
  current_func = prev_func;

  // get return value
  if (func->ReturnType.Type) {
    --pr_stackPtr;
    ret = *pr_stackPtr;
  }
#ifdef VMEXEC_RUNDUMP
  printIndent(); printf("***LEAVING `%s` (RETx); sp=%d, (MAX:%u)\n", *func->GetFullName(), (int)(pr_stackPtr-pr_stack), (unsigned)MAX_PROG_STACK); leaveIndent();
#endif

#ifdef CHECK_FOR_EMPTY_STACK
  // after executing base function stack must be empty
  if (!current_func && pr_stackPtr != pr_stack+1) {
    cstDump(nullptr);
    Sys_Error("ExecuteFunction: Stack is not empty after executing function:\n%s\nstack = %p, oldsp = %p", *func->Name, pr_stack, pr_stackPtr);
    #ifdef VMEXEC_RUNDUMP
    *(int *)0 = 0;
    #endif
  }
#endif

#ifdef CHECK_STACK_UNDERFLOW
  // check if stack wasn't underflowed
  if (pr_stack[0].i != STACK_ID) {
    cstDump(nullptr);
    Sys_Error("ExecuteFunction: Stack underflow in %s", *func->Name);
    #ifdef VMEXEC_RUNDUMP
    *(int *)0 = 0;
    #endif
  }
#endif

#ifdef CHECK_STACK_OVERFLOW
  // check if stack wasn't overflowed
  if (pr_stack[MAX_PROG_STACK-1].i != STACK_ID) {
    cstDump(nullptr);
    Sys_Error("ExecuteFunction: Stack overflow in %s", *func->Name);
    #ifdef VMEXEC_RUNDUMP
    *(int *)0 = 0;
    #endif
  }
#endif

  // all done
  return ret;
  unguardf(("(%s)", *func->GetFullName()));
}


//==========================================================================
//
//  VObject::VMDumpCallStack
//
//==========================================================================
void VObject::VMDumpCallStack () {
  cstDump(nullptr);
}


//==========================================================================
//
//  VObject::DumpProfile
//
//==========================================================================
void VObject::DumpProfile () {
  //#define MAX_PROF  100
  const int MAX_PROF = 100;
  int profsort[MAX_PROF];
  int totalcount = 0;
  memset(profsort, 0, sizeof(profsort));
  for (int i = 0; i < VMemberBase::GMembers.Num(); ++i) {
    if (VMemberBase::GMembers[i]->MemberType != MEMBER_Method) continue;
    VMethod *Func = (VMethod *)VMemberBase::GMembers[i];
    if (!Func->Profile1) continue; // never called
    for (int j = 0; j < MAX_PROF; ++j) {
      totalcount += Func->Profile2;
      if (((VMethod *)VMemberBase::GMembers[profsort[j]])->Profile2 <= Func->Profile2) {
        for (int k = MAX_PROF-1; k > j; --k) profsort[k] = profsort[k-1];
        profsort[j] = i;
        break;
      }
    }
  }
  if (!totalcount) return;
  for (int i = 0; i < MAX_PROF && profsort[i]; ++i) {
    VMethod *Func = (VMethod *)VMemberBase::GMembers[profsort[i]];
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
    GCon->Logf("%3.2f%% (%9d) %9d %s",
      (double)Func->Profile2*100.0/(double)totalcount,
      (int)Func->Profile2, (int)Func->Profile1, *Func->GetFullName());
#else
    fprintf(stderr, "%3.2f%% (%9d) %9d %s\n",
      (double)Func->Profile2*100.0/(double)totalcount,
      (int)Func->Profile2, (int)Func->Profile1, *Func->GetFullName());
#endif
  }
}
