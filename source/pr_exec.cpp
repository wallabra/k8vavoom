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
//**
//**    Execution of PROGS.
//**
//**************************************************************************
//#define VCC_STUPID_TRACER
//#define VMEXEC_RUNDUMP

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

#define DICTDISPATCH_OPCODE_INFO
#include "progdefs.h"

#define DYNARRDISPATCH_OPCODE_INFO
#include "progdefs.h"


#define MAX_PROG_STACK  (10000)
#define STACK_ID  (0x45f6cd4b)

#define CHECK_STACK_OVERFLOW
#define CHECK_STACK_UNDERFLOW
#define CHECK_FOR_EMPTY_STACK

#define CHECK_FOR_INF_NAN_DIV

// debug feature, nan and inf can be used for various purposes
// but meh...
//#define CHECK_FOR_NANS_INFS
#define CHECK_FOR_NANS_INFS_RETURN


#ifdef VMEXEC_RUNDUMP
static int k8edIndent = 0;

static void printIndent () { for (int f = k8edIndent; f > 0; --f) fputc(' ', stderr); }
static void enterIndent () { ++k8edIndent; }
static void leaveIndent () { if (--k8edIndent < 0) abort(); }
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
    callStack = (CallStackItem *)Z_Realloc(callStack, sizeof(callStack[0])*cstSize);
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


static VScriptIterator **iterStack = nullptr;
static int iterStackUsed = 0;
static int iterStackSize = 0;


//==========================================================================
//
//  pushOldIterator
//
//==========================================================================
static void pushOldIterator (VScriptIterator *iter) {
  if (iterStackUsed == iterStackSize) {
    // grow
    iterStackSize = ((iterStackSize+1)|0x3ff)+1;
    iterStack = (VScriptIterator **)Z_Realloc(iterStack, iterStackSize*sizeof(iterStack[0]));
    if (!iterStack) { cstDump(nullptr); Sys_Error("popOldIterator: out of memory for iterator stack"); }
    if (iterStackUsed >= iterStackSize) { cstDump(nullptr); Sys_Error("popOldIterator: WTF?!"); }
  }
  iterStack[iterStackUsed++] = iter;
}


//==========================================================================
//
//  popOldIterator
//
//==========================================================================
static void popOldIterator () {
  if (iterStackUsed == 0) { cstDump(nullptr); Sys_Error("popOldIterator: iterator stack underflow"); }
  VScriptIterator *it = iterStack[--iterStackUsed];
  if (it) it->Finished();
}


//==========================================================================
//
//  ExecDictOperator
//
//==========================================================================
static void ExecDictOperator (vuint8 *origip, vuint8 *&ip, VStack *&sp, VFieldType &KType, VFieldType &VType, vuint8 dcopcode) {
  VScriptDict *ht;
  VScriptDictElem e, v, *r;
  switch (dcopcode) {
    // clear
    // [-1]: VScriptDict
    case OPC_DictDispatch_Clear:
      ht = (VScriptDict *)sp[-1].p;
      if (!ht) { cstDump(origip); Sys_Error("uninitialized dictionary"); }
      ht->clear();
      --sp;
      return;
    // reset
    // [-1]: VScriptDict
    case OPC_DictDispatch_Reset:
      ht = (VScriptDict *)sp[-1].p;
      if (!ht) { cstDump(origip); Sys_Error("uninitialized dictionary"); }
      ht->reset();
      --sp;
      return;
    // length
    // [-1]: VScriptDict
    case OPC_DictDispatch_Length:
      ht = (VScriptDict *)sp[-1].p;
      if (!ht) { cstDump(origip); Sys_Error("uninitialized dictionary"); }
      sp[-1].i = ht->length();
      return;
    // capacity
    // [-1]: VScriptDict
    case OPC_DictDispatch_Capacity:
      ht = (VScriptDict *)sp[-1].p;
      if (!ht) { cstDump(origip); Sys_Error("uninitialized dictionary"); }
      sp[-1].i = ht->capacity();
      return;
    // find
    // [-2]: VScriptDict
    // [-1]: keyptr
    case OPC_DictDispatch_Find:
      ht = (VScriptDict *)sp[-2].p;
      if (!ht) { cstDump(origip); Sys_Error("uninitialized dictionary"); }
      VScriptDictElem::CreateFromPtr(e, sp[-1].p, KType, true); // calc hash
      r = ht->find(e);
      if (r) {
        if (VType.Type == TYPE_String || VScriptDictElem::isSimpleType(VType)) {
          sp[-2].p = &r->value;
        } else {
          sp[-2].p = r->value;
        }
      } else {
        sp[-2].p = nullptr;
      }
      --sp;
      return;
    // put
    // [-3]: VScriptDict
    // [-2]: keyptr
    // [-1]: valptr
    case OPC_DictDispatch_Put:
      {
        ht = (VScriptDict *)sp[-3].p;
        if (!ht) { cstDump(origip); Sys_Error("uninitialized dictionary"); }
        VScriptDictElem::CreateFromPtr(e, sp[-2].p, KType, true); // calc hash
        VScriptDictElem::CreateFromPtr(v, sp[-1].p, VType, false); // no hash
        sp[-3].i = (ht->put(e, v) ? 1 : 0);
        sp -= 2;
      }
      return;
    // delete
    // [-2]: VScriptDict
    // [-1]: keyptr
    case OPC_DictDispatch_Delete:
      ht = (VScriptDict *)sp[-2].p;
      if (!ht) { cstDump(origip); Sys_Error("uninitialized dictionary"); }
      VScriptDictElem::CreateFromPtr(e, sp[-1].p, KType, true); // calc hash
      sp[-2].i = (ht->del(e) ? 1 : 0);
      --sp;
      return;
    // clear by ptr
    // [-1]: VScriptDict*
    case OPC_DictDispatch_ClearPointed:
      ht = (VScriptDict *)sp[-1].p;
      if (!ht) { cstDump(origip); Sys_Error("uninitialized dictionary"); }
      ht->clear();
      --sp;
      return;
    // first index
    // [-1]: VScriptDict*
    case OPC_DictDispatch_FirstIndex:
      ht = (VScriptDict *)sp[-1].p;
      if (!ht) { cstDump(origip); Sys_Error("uninitialized dictionary"); }
      sp[-1].i = (ht->map ? ht->map->getFirstIIdx() : -1);
      return;
    // is valid index?
    // [-2]: VScriptDict*
    // [-1]: index
    case OPC_DictDispatch_IsValidIndex:
      ht = (VScriptDict *)sp[-2].p;
      if (!ht) { cstDump(origip); Sys_Error("uninitialized dictionary"); }
      sp[-2].i = ((ht->map ? ht->map->isValidIIdx(sp[-1].i) : false) ? 1 : 0);
      --sp;
      return;
    // next index
    // [-2]: VScriptDict*
    // [-1]: index
    case OPC_DictDispatch_NextIndex:
      ht = (VScriptDict *)sp[-2].p;
      if (!ht) { cstDump(origip); Sys_Error("uninitialized dictionary"); }
      sp[-2].i = (ht->map ? ht->map->getNextIIdx(sp[-1].i) : -1);
      --sp;
      return;
    // delete current and next index
    // [-2]: VScriptDict*
    // [-1]: index
    case OPC_DictDispatch_DelAndNextIndex:
      ht = (VScriptDict *)sp[-2].p;
      if (!ht) { cstDump(origip); Sys_Error("uninitialized dictionary"); }
      sp[-2].i = (ht->map ? ht->map->removeCurrAndGetNextIIdx(sp[-1].i) : -1);
      --sp;
      return;
    // key at index
    // [-2]: VScriptDict
    // [-1]: index
    case OPC_DictDispatch_GetKeyAtIndex:
      ht = (VScriptDict *)sp[-2].p;
      if (!ht) { cstDump(origip); Sys_Error("uninitialized dictionary"); }
      {
        const VScriptDictElem *ep = (ht->map ? ht->map->getKeyIIdx(sp[-1].i) : nullptr);
        if (ep) {
          if (KType.Type == TYPE_String) {
            sp[-2].p = nullptr;
            *((VStr *)&sp[-2].p) = *((VStr *)&ep->value);
          } else {
            sp[-2].p = ep->value;
          }
        } else {
          sp[-2].p = nullptr;
        }
      }
      --sp;
      return;
    // value at index
    // [-2]: VScriptDict
    // [-1]: index
    case OPC_DictDispatch_GetValueAtIndex:
      ht = (VScriptDict *)sp[-2].p;
      if (!ht) { cstDump(origip); Sys_Error("uninitialized dictionary"); }
      {
        VScriptDictElem *ep = (ht->map ? ht->map->getValueIIdx(sp[-1].i) : nullptr);
        if (ep) {
          if (VType.Type == TYPE_String || VScriptDictElem::isSimpleType(VType)) {
            sp[-2].p = &ep->value;
          } else {
            sp[-2].p = ep->value;
          }
        } else {
          sp[-2].p = nullptr;
        }
      }
      --sp;
      return;
    // compact
    // [-1]: VScriptDict
    case OPC_DictDispatch_Compact:
      ht = (VScriptDict *)sp[-1].p;
      if (!ht) { cstDump(origip); Sys_Error("uninitialized dictionary"); }
      if (ht->map) ht->map->compact();
      --sp;
      return;
    // rehash
    // [-1]: VScriptDict
    case OPC_DictDispatch_Rehash:
      ht = (VScriptDict *)sp[-1].p;
      if (!ht) { cstDump(origip); Sys_Error("uninitialized dictionary"); }
      if (ht->map) ht->map->rehash();
      --sp;
      return;
  }
  cstDump(origip);
  Sys_Error("Dictionary opcode %d is not implemented", dcopcode);
}


//==========================================================================
//
//  RunFunction
//
//==========================================================================
static void RunFunction (VMethod *func) {
  vuint8 *ip = nullptr;
  VStack *sp;
  VStack *local_vars;
  float ftemp;
  vint32 itemp;

  guard(RunFunction);
  current_func = func;

  if (!func) { cstDump(nullptr); Sys_Error("Trying to execute null function"); }

#if defined(VCC_STANDALONE_EXECUTOR)
  ++(current_func->Profile1);
  ++(current_func->Profile2);
#endif

  //fprintf(stderr, "FN(%d): <%s>\n", cstUsed, *func->GetFullName());

  if (func->Flags&FUNC_Net) {
    VStack *Params = pr_stackPtr-func->ParamsSize;
    //if (!(current_func->Flags&FUNC_Native)) ++(current_func->Profile2);
    if (((VObject *)Params[0].p)->ExecuteNetMethod(func)) return;
  }

  if (func->Flags&FUNC_Native) {
    // native function, first statement is pointer to function
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
    if (developer) cstPush(func);
#endif
    func->NativeFunc();
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
    if (developer) cstPop();
#endif
    return;
  }

  cstPush(func);

  // cache stack pointer in register
  sp = pr_stackPtr;

  // setup local vars
  //fprintf(stderr, "FUNC: <%s> (%s) ParamsSize=%d; NumLocals=%d; NumParams=%d\n", *func->GetFullName(), *func->Loc.toStringNoCol(), func->ParamsSize, func->NumLocals, func->NumParams);
  if (func->NumLocals < func->ParamsSize) { cstDump(nullptr); Sys_Error("Miscompiled function (locals=%d, params=%d)", func->NumLocals, func->ParamsSize); }
  local_vars = sp-func->ParamsSize;
  if (func->NumLocals-func->ParamsSize != 0) memset(sp, 0, (func->NumLocals-func->ParamsSize)*sizeof(VStack));
  sp += func->NumLocals-func->ParamsSize;

  ip = func->Statements.Ptr();

#ifdef VMEXEC_RUNDUMP
  enterIndent(); printIndent(); fprintf(stderr, "ENTERING VC FUNCTION `%s`; sp=%d\n", *func->GetFullName(), (int)(sp-local_vars));
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
    fprintf(stderr, "*** %s: %6u: %s (sp=%d)\n", *func->GetFullName(), (unsigned)(ip-func->Statements.Ptr()), StatementInfo[*ip].name, (int)(sp-local_vars));
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
          if (!pDelegate[1]) { cstDump(ip); Sys_Error("Delegate is not initialised (empty method)"); }
          if ((uintptr_t)pDelegate[1] < 65536) { cstDump(ip); Sys_Error("Delegate is completely fucked"); }
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
          if (!pDelegate[1]) { cstDump(ip); Sys_Error("Delegate is not initialised (empty method)"); }
          if ((uintptr_t)pDelegate[1] < 65536) { cstDump(ip); Sys_Error("Delegate is completely fucked"); }
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
          if (!pDelegate[1]) { cstDump(ip); Sys_Error("Delegate is not initialised (empty method)"); }
          if ((uintptr_t)pDelegate[1] < 65536) { cstDump(ip); Sys_Error("Delegate is completely fucked"); }
          sp[-sofs].p = pDelegate[0];
          pr_stackPtr = sp;
          cstFixTopIPSP(ip);
          RunFunction((VMethod *)pDelegate[1]);
        }
        current_func = func;
        sp = pr_stackPtr;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Return)
        checkSlow(sp == local_vars+func->NumLocals);
#ifdef VMEXEC_RUNDUMP
        printIndent(); fprintf(stderr, "LEAVING VC FUNCTION `%s`; sp=%d\n", *func->GetFullName(), (int)(sp-pr_stack)); leaveIndent();
#endif
        pr_stackPtr = local_vars;
        cstPop();
        return;

      PR_VM_CASE(OPC_ReturnL)
        checkSlow(sp == local_vars+func->NumLocals+1);
#ifdef VMEXEC_RUNDUMP
        printIndent(); fprintf(stderr, "LEAVING VC FUNCTION `%s`; sp=%d\n", *func->GetFullName(), (int)(sp-pr_stack)); leaveIndent();
#endif
        ((VStack *)local_vars)[0] = sp[-1];
        pr_stackPtr = local_vars+1;
        cstPop();
        return;

      PR_VM_CASE(OPC_ReturnV)
        checkSlow(sp == local_vars+func->NumLocals+3);
#ifdef VMEXEC_RUNDUMP
        printIndent(); fprintf(stderr, "LEAVING VC FUNCTION `%s`; sp=%d\n", *func->GetFullName(), (int)(sp-pr_stack)); leaveIndent();
#endif
#ifdef CHECK_FOR_NANS_INFS_RETURN
        if (!isFiniteF(sp[-3].f) || !isFiniteF(sp[-2].f) || !isFiniteF(sp[-1].f)) { cstDump(ip); Sys_Error("returning NAN/INF vector"); }
#endif
        ((VStack *)local_vars)[0] = sp[-3];
        ((VStack *)local_vars)[1] = sp[-2];
        ((VStack *)local_vars)[2] = sp[-1];
        pr_stackPtr = local_vars+3;
        cstPop();
        return;

      PR_VM_CASE(OPC_GotoB)
        ip += ip[1];
        PR_VM_BREAK;

      PR_VM_CASE(OPC_GotoNB)
        ip -= ip[1];
        PR_VM_BREAK;

      /*
      PR_VM_CASE(OPC_GotoS)
        ip += ReadInt16(ip+1);
        PR_VM_BREAK;
      */

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

      /*
      PR_VM_CASE(OPC_IfGotoS)
        if (sp[-1].i) ip += ReadInt16(ip+1); else ip += 3;
        --sp;
        PR_VM_BREAK;
      */

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

      /*
      PR_VM_CASE(OPC_IfNotGotoS)
        if (!sp[-1].i) ip += ReadInt16(ip+1); else ip += 3;
        --sp;
        PR_VM_BREAK;
      */

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

      // yeah, exactly the same as normal `OPC_CaseGoto`
      PR_VM_CASE(OPC_CaseGotoN)
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

      /*
      PR_VM_CASE(OPC_PushNumberS)
        sp->i = ReadInt16(ip+1);
        ip += 3;
        ++sp;
        PR_VM_BREAK;
      */

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
        {
          sp->p = ReadPtr(ip+1);
          ip += 1+sizeof(void *);
          ++sp;
          const VStr *S = (const VStr *)sp[-1].p;
          sp[-1].p = nullptr;
          *(VStr *)&sp[-1].p = *S;
#if defined(VCC_OLD_PACKAGE_STRING_POOL)
          const char *S = (const char *)sp[-1].p;
          sp[-1].p = nullptr;
          *(VStr *)&sp[-1].p = S;
#endif
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
#ifdef CHECK_FOR_NANS_INFS
        if (!isFiniteF(sp[0].f) || !isFiniteF(sp[1].f) || !isFiniteF(sp[2].f)) { cstDump(ip); Sys_Error("creating NAN/INF vector"); }
#endif
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
#ifdef CHECK_FOR_NANS_INFS
          if (!isFiniteF(sp[1].f) || !isFiniteF(sp[0].f) || !isFiniteF(sp[-1].f)) { cstDump(ip); Sys_Error("loading NAN/INF vector"); }
#endif
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

      /*
      PR_VM_CASE(OPC_OffsetPtr)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Cannot offset null pointer"); }
        sp[-1].p = (vuint8 *)sp[-1].p+ReadInt32(ip+1);
        ip += 5;
        PR_VM_BREAK;
      */

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
#ifdef CHECK_FOR_NANS_INFS
        if (!isFiniteF(sp[1].f) || !isFiniteF(sp[0].f) || !isFiniteF(sp[-1].f)) { cstDump(ip); Sys_Error("pushing NAN/INF vector"); }
#endif
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

#ifdef CHECK_FOR_NANS_INFS
    #define BINOP(mem, op) \
      if (!isFiniteF(sp[-2].mem)) { cstDump(ip); Sys_Error("op '%s' first arg is NAN/INF", #op); } \
      if (!isFiniteF(sp[-1].mem)) { cstDump(ip); Sys_Error("op '%s' second arg is NAN/INF", #op); } \
      sp[-2].mem = sp[-2].mem op sp[-1].mem; \
      if (!isFiniteF(sp[-2].mem)) { cstDump(ip); Sys_Error("op '%s' result is NAN/INF", #op); } \
      --sp; \
      ++ip;
#else
    #define BINOP(mem, op) \
      ++ip; \
      sp[-2].mem = sp[-2].mem op sp[-1].mem; \
      --sp;
#endif
#ifdef CHECK_FOR_NANS_INFS
    #define BINOP_Q(mem, op) \
      if (!isFiniteF(sp[-2].mem)) { cstDump(ip); Sys_Error("op '%s' first arg is NAN/INF", #op); } \
      if (!isFiniteF(sp[-1].mem)) { cstDump(ip); Sys_Error("op '%s' second arg is NAN/INF", #op); } \
      sp[-2].mem op sp[-1].mem; \
      if (!isFiniteF(sp[-2].mem)) { cstDump(ip); Sys_Error("op '%s' result is NAN/INF", #op); } \
      --sp; \
      ++ip;
#else
    #define BINOP_Q(mem, op) \
      ++ip; \
      sp[-2].mem op sp[-1].mem; \
      --sp;
#endif

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
        sp[-2].u >>= sp[-1].i;
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

#ifdef CHECK_FOR_NANS_INFS
    #define ASSIGNOP(type, mem, op) \
      if (!isFiniteF(*(type *)sp[-2].p)) { cstDump(ip); Sys_Error("assign '%s' with NAN/INF (0)", #op); } \
      if (!isFiniteF(sp[-1].mem)) { cstDump(ip); Sys_Error("assign '%s' with NAN/INF (1)", #op); } \
      *(type *)sp[-2].p op sp[-1].mem; \
      if (!isFiniteF(*(type *)sp[-2].p)) { cstDump(ip); Sys_Error("assign '%s' with NAN/INF (3)", #op); } \
      sp -= 2; \
      ++ip;
#else
    #define ASSIGNOP(type, mem, op) \
      ++ip; \
      *(type *)sp[-2].p op sp[-1].mem; \
      sp -= 2;
#endif
    #define ASSIGNOPNN(type, mem, op) \
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
        ASSIGNOP(vuint32, i, &=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_OrVarDrop)
        ASSIGNOP(vuint32, i, |=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_XOrVarDrop)
        ASSIGNOP(vuint32, i, ^=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LShiftVarDrop)
        ASSIGNOP(vuint32, i, <<=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_RShiftVarDrop)
        ASSIGNOP(vint32, i, >>=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_URShiftVarDrop)
        ASSIGNOP(vuint32, u, >>=);
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

      // [-1] src
      // [-2] dest
      PR_VM_CASE(OPC_CatAssignVarDrop)
        ++ip;
        *(VStr *)sp[-2].p += *(VStr *)&sp[-1].p;
        ((VStr *)&sp[-1].p)->Clean();
        sp -= 2;
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
#ifdef CHECK_FOR_INF_NAN_DIV
        if (!isFiniteF(sp[-1].f)) { cstDump(ip); if (isNaNF(sp[-1].f)) Sys_Error("Division by NAN"); Sys_Error("Division by INF"); }
#endif
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
#ifdef CHECK_FOR_NANS_INFS
        if (!isFiniteF(sp[-1].f)) { cstDump(ip); Sys_Error("unary with NAN/INF"); }
#endif
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
        if (!sp[-1].f) { cstDump(ip); Sys_Error("Division by 0"); }
#ifdef CHECK_FOR_INF_NAN_DIV
        if (!isFiniteF(sp[-1].f)) { cstDump(ip); if (isNaNF(sp[-1].f)) Sys_Error("Division by NAN"); Sys_Error("Division by INF"); }
#endif
        ASSIGNOP(float, f, /=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VAdd)
#ifdef CHECK_FOR_NANS_INFS
        if (!isFiniteF(sp[-6].f) || !isFiniteF(sp[-5].f) || !isFiniteF(sp[-4].f)) { cstDump(ip); Sys_Error("vec+ op0 is NAN/INF"); }
        if (!isFiniteF(sp[-3].f) || !isFiniteF(sp[-2].f) || !isFiniteF(sp[-1].f)) { cstDump(ip); Sys_Error("vec+ op1 is NAN/INF"); }
#endif
        sp[-6].f += sp[-3].f;
        sp[-5].f += sp[-2].f;
        sp[-4].f += sp[-1].f;
        sp -= 3;
        ++ip;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VSubtract)
#ifdef CHECK_FOR_NANS_INFS
        if (!isFiniteF(sp[-6].f) || !isFiniteF(sp[-5].f) || !isFiniteF(sp[-4].f)) { cstDump(ip); Sys_Error("vec- op0 is NAN/INF"); }
        if (!isFiniteF(sp[-3].f) || !isFiniteF(sp[-2].f) || !isFiniteF(sp[-1].f)) { cstDump(ip); Sys_Error("vec- op1 is NAN/INF"); }
#endif
        sp[-6].f -= sp[-3].f;
        sp[-5].f -= sp[-2].f;
        sp[-4].f -= sp[-1].f;
        sp -= 3;
        ++ip;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VPreScale)
        {
          float scale = sp[-4].f;
#ifdef CHECK_FOR_INF_NAN_DIV
          if (!isFiniteF(scale)) { cstDump(ip); Sys_Error("vecprescale scale is NAN/INF"); }
#endif
#ifdef CHECK_FOR_NANS_INFS
          if (!isFiniteF(sp[-4].f) || !isFiniteF(sp[-3].f) || !isFiniteF(sp[-2].f)) { cstDump(ip); Sys_Error("vecprescale vec is NAN/INF"); }
#endif
          sp[-4].f = scale*sp[-3].f;
          sp[-3].f = scale*sp[-2].f;
          sp[-2].f = scale*sp[-1].f;
          --sp;
          ++ip;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VPostScale)
#ifdef CHECK_FOR_INF_NAN_DIV
        if (!isFiniteF(sp[-1].f)) { cstDump(ip); Sys_Error("vecscale scale is NAN/INF"); }
#endif
#ifdef CHECK_FOR_NANS_INFS
        if (!isFiniteF(sp[-4].f) || !isFiniteF(sp[-3].f) || !isFiniteF(sp[-2].f)) { cstDump(ip); Sys_Error("vecscale vec is NAN/INF"); }
#endif
        sp[-4].f *= sp[-1].f;
        sp[-3].f *= sp[-1].f;
        sp[-2].f *= sp[-1].f;
#ifdef CHECK_FOR_NANS_INFS
        if (!isFiniteF(sp[-1].f)) { cstDump(ip); Sys_Error("vecscale scale is NAN/INF (exit)"); }
        if (!isFiniteF(sp[-4].f) || !isFiniteF(sp[-3].f) || !isFiniteF(sp[-2].f)) { cstDump(ip); Sys_Error("vecscale vec is NAN/INF (exit)"); }
#endif
        --sp;
        ++ip;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VIScale)
        if (!isFiniteF(sp[-1].f)) { cstDump(ip); Sys_Error("veciscale scale is NAN/INF"); }
#ifdef CHECK_FOR_NANS_INFS
        if (!isFiniteF(sp[-4].f) || !isFiniteF(sp[-3].f) || !isFiniteF(sp[-2].f)) { cstDump(ip); Sys_Error("veciscale vec is NAN/INF"); }
#endif
        sp[-4].f /= sp[-1].f;
        sp[-3].f /= sp[-1].f;
        sp[-2].f /= sp[-1].f;
#ifdef CHECK_FOR_NANS_INFS
        if (!isFiniteF(sp[-1].f)) { cstDump(ip); Sys_Error("veciscale scale is NAN/INF (exit)"); }
        if (!isFiniteF(sp[-4].f) || !isFiniteF(sp[-3].f) || !isFiniteF(sp[-2].f)) { cstDump(ip); Sys_Error("veciscale vec is NAN/INF (exit)"); }
#endif
        --sp;
        ++ip;
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
#ifdef CHECK_FOR_NANS_INFS
        if (!isFiniteF(sp[-3].f) || !isFiniteF(sp[-2].f) || !isFiniteF(sp[-1].f)) { cstDump(ip); Sys_Error("vecunary vec is NAN/INF"); }
#endif
        sp[-3].f = -sp[-3].f;
        sp[-2].f = -sp[-2].f;
        sp[-1].f = -sp[-1].f;
        ++ip;
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

#ifdef CHECK_FOR_NANS_INFS
    #define VASSIGNOP(op) \
      { \
        TVec *ptr = (TVec *)sp[-4].p; \
        if (!isFiniteF(ptr->x) || !isFiniteF(ptr->y) || !isFiniteF(ptr->z)) { cstDump(ip); Sys_Error("vassign op '%s' op0 is NAN/INF", #op); } \
        if (!isFiniteF(sp[-3].f) || !isFiniteF(sp[-2].f) || !isFiniteF(sp[-1].f)) { cstDump(ip); Sys_Error("vassign op '%s' op1 is NAN/INF", #op); } \
        ptr->x op sp[-3].f; \
        ptr->y op sp[-2].f; \
        ptr->z op sp[-1].f; \
        if (!isFiniteF(ptr->x) || !isFiniteF(ptr->y) || !isFiniteF(ptr->z)) { cstDump(ip); Sys_Error("vassign op '%s' op0 is NAN/INF (exit)", #op); } \
        if (!isFiniteF(sp[-3].f) || !isFiniteF(sp[-2].f) || !isFiniteF(sp[-1].f)) { cstDump(ip); Sys_Error("vassign op '%s' op1 is NAN/INF (exit)", #op); } \
        sp -= 4; \
        ++ip; \
      }
#else
    #define VASSIGNOP(op) \
      { \
        ++ip; \
        TVec *ptr = (TVec *)sp[-4].p; \
        ptr->x op sp[-3].f; \
        ptr->y op sp[-2].f; \
        ptr->z op sp[-1].f; \
        sp -= 4; \
      }
#endif

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
#ifdef CHECK_FOR_INF_NAN_DIV
        if (!isFiniteF(sp[-1].f)) { cstDump(ip); Sys_Error("vecscaledrop scale is NAN/INF"); }
#endif
        ++ip;
        *(TVec *)sp[-2].p *= sp[-1].f;
        sp -= 2;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VIScaleVarDrop)
        if (!sp[-1].f) { cstDump(ip); Sys_Error("Vector division by 0"); }
#ifdef CHECK_FOR_INF_NAN_DIV
        if (!isFiniteF(sp[-1].f)) { cstDump(ip); if (isNaNF(sp[-1].f)) Sys_Error("Vector division by NAN"); Sys_Error("Vector division by INF"); }
#endif
        ++ip;
        *(TVec *)sp[-2].p /= sp[-1].f;
        sp -= 2;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FloatToBool)
        sp[-1].i = (sp[-1].f == 0 ? 0 : 1);
        ++ip;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VectorToBool)
        //sp[-3].i = (sp[-1].f == 0 && sp[-2].f == 0 && sp[-3].f == 0 ? 0 : 1);
        sp[-3].i = (isFiniteF(sp[-1].f) && isFiniteF(sp[-2].f) && isFiniteF(sp[-3].f) &&
                    (sp[-1].f != 0 || sp[-2].f != 0 || sp[-3].f != 0) ? 1 : 0);
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

      PR_VM_CASE(OPC_PtrSubtract)
        {
          if (!sp[-2].p) { cstDump(ip); Sys_Error("Invalid pointer math (first operand is `nullptr`)"); }
          if (!sp[-1].p) { cstDump(ip); Sys_Error("Invalid pointer math (second operand is `nullptr`)"); }
          //if ((uintptr_t)sp[-2].p < (uintptr_t)sp[-1].p) { cstDump(ip); Sys_Error("Invalid pointer math (first operand is out of range)"); }
          int tsize = ReadInt32(ip+1);
          ptrdiff_t diff = ((intptr_t)sp[-2].p-(intptr_t)sp[-1].p)/tsize;
          if (diff < -0x7fffffff || diff > 0x7fffffff) { cstDump(ip); Sys_Error("Invalid pointer math (difference is too big)"); }
          sp -= 1;
          sp[-1].i = (int)diff;
        }
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IntToFloat)
        ++ip;
        ftemp = (float)sp[-1].i;
        if (!isFiniteF(ftemp)) { cstDump(ip); Sys_Error("Invalid int->float conversion"); }
        sp[-1].f = ftemp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FloatToInt)
        ++ip;
        if (!isFiniteF(sp[-1].f)) { cstDump(ip); Sys_Error("Invalid float->int conversion"); }
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
        ((VStruct *)ReadPtr(ip+1))->DestructObject((vuint8 *)sp[-1].p);
        ip += 1+sizeof(void *);
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ZeroPointedStruct)
        ((VStruct *)ReadPtr(ip+1))->ZeroObject((vuint8 *)sp[-1].p);
        ip += 1+sizeof(void *);
        --sp;
        PR_VM_BREAK;

      // [-2] destptr
      // [-1] srcptr
      PR_VM_CASE(OPC_TypeDeepCopy)
        {
          vuint8 *origip = ip++;
          VFieldType stp = VFieldType::ReadTypeMem(ip);
          if (!sp[-2].p && !sp[-1].p) {
            sp -= 2;
          } else {
            if (!sp[-2].p) { cstDump(origip); Sys_Error("destination is nullptr"); }
            if (!sp[-1].p) { cstDump(origip); Sys_Error("source is nullptr"); }
            VField::CopyFieldValue((const vuint8 *)sp[-1].p, (vuint8 *)sp[-2].p, stp);
            sp -= 2;
          }
        }
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
        ASSIGNOPNN(void *, p, =);
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
          ++ip;
          VFieldType Type = VFieldType::ReadTypeMem(ip);
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

      // this is always followed by type and opcode
      PR_VM_CASE(OPC_DynArrayDispatch)
        {
          vuint8 *origip = ip++;
          VFieldType Type = VFieldType::ReadTypeMem(ip);
          switch (*ip++) {
            // [-2]: *dynarray
            // [-1]: length
            case OPC_DynArrDispatch_DynArraySetNum:
              {
                VScriptArray &A = *(VScriptArray *)sp[-2].p;
                int newsize = sp[-1].i;
                // allow clearing for 2d arrays
                if (A.Is2D() && newsize != 0) { cstDump(origip); Sys_Error("Cannot resize 2D array"); }
                if (newsize < 0) { cstDump(origip); Sys_Error("Array index %d is negative", newsize); }
                if (newsize > MaxDynArrayLength) { cstDump(origip); Sys_Error("Array index %d is too big", newsize); }
                A.SetNum(newsize, Type);
                sp -= 2;
              }
              break;
            // [-2]: *dynarray
            // [-1]: delta
            case OPC_DynArrDispatch_DynArraySetNumMinus:
              {
                VScriptArray &A = *(VScriptArray *)sp[-2].p;
                int newsize = sp[-1].i;
                // allow clearing for 2d arrays
                if (A.Is2D() && newsize != 0 && newsize != A.length()) { cstDump(origip); Sys_Error("Cannot resize 2D array"); }
                if (newsize < 0) { cstDump(origip); Sys_Error("Array shrink delta %d is negative", newsize); }
                if (newsize > MaxDynArrayLength) { cstDump(origip); Sys_Error("Array shrink delta %d is too big", newsize); }
                if (A.length() < newsize) { cstDump(origip); Sys_Error("Array shrink delta %d is too big (%d)", newsize, A.length()); }
                if (newsize > 0) A.SetNumMinus(newsize, Type);
                sp -= 2;
              }
              break;
            // [-2]: *dynarray
            // [-1]: delta
            case OPC_DynArrDispatch_DynArraySetNumPlus:
              {
                VScriptArray &A = *(VScriptArray *)sp[-2].p;
                int newsize = sp[-1].i;
                // allow clearing for 2d arrays
                if (A.Is2D() && newsize != 0 && newsize != A.length()) { cstDump(origip); Sys_Error("Cannot resize 2D array"); }
                if (newsize < 0) { cstDump(origip); Sys_Error("Array grow delta %d is negative", newsize); }
                if (newsize > MaxDynArrayLength) { cstDump(origip); Sys_Error("Array grow delta %d is too big", newsize); }
                if (A.length() > MaxDynArrayLength || MaxDynArrayLength-A.length() < newsize) { cstDump(origip); Sys_Error("Array grow delta %d is too big (%d)", newsize, A.length()); }
                if (newsize > 0) A.SetNumPlus(newsize, Type);
                sp -= 2;
              }
              break;
            // [-3]: *dynarray
            // [-2]: index
            // [-1]: count
            case OPC_DynArrDispatch_DynArrayInsert:
              {
                VScriptArray &A = *(VScriptArray *)sp[-3].p;
                if (A.Is2D()) { cstDump(origip); Sys_Error("Cannot insert into 2D array"); }
                int index = sp[-2].i;
                int count = sp[-1].i;
                if (count < 0) { cstDump(origip); Sys_Error("Array count %d is negative", count); }
                if (index < 0) { cstDump(origip); Sys_Error("Array index %d is negative", index); }
                if (index > A.length()) { cstDump(origip); Sys_Error("Index %d outside the bounds of an array (%d)", index, A.length()); }
                if (A.length() > MaxDynArrayLength || MaxDynArrayLength-A.length() < count) { cstDump(origip); Sys_Error("Out of memory for dynarray"); }
                if (count > 0) A.Insert(index, count, Type);
                sp -= 3;
              }
              break;
            // [-3]: *dynarray
            // [-2]: index
            // [-1]: count
            case OPC_DynArrDispatch_DynArrayRemove:
              {
                VScriptArray &A = *(VScriptArray *)sp[-3].p;
                if (A.Is2D()) { cstDump(origip); Sys_Error("Cannot insert into 2D array"); }
                int index = sp[-2].i;
                int count = sp[-1].i;
                if (count < 0) { cstDump(origip); Sys_Error("Array count %d is negative", count); }
                if (index < 0) { cstDump(origip); Sys_Error("Array index %d is negative", index); }
                if (index > A.length()) { cstDump(origip); Sys_Error("Index %d outside the bounds of an array (%d)", index, A.length()); }
                if (count > A.length()-index) { cstDump(origip); Sys_Error("Array count %d is too big at %d (%d)", count, index, A.length()); }
                if (count > 0) A.Remove(index, count, Type);
                sp -= 3;
              }
              break;
            // [-1]: *dynarray
            case OPC_DynArrDispatch_DynArrayClear:
              {
                VScriptArray &A = *(VScriptArray *)sp[-1].p;
                A.Reset(Type);
                --sp;
              }
              break;
            // [-1]: delegate
            // [-2]: self
            // [-3]: *dynarray
            // in code: type
            case OPC_DynArrDispatch_DynArraySort:
              //fprintf(stderr, "sp=%p\n", sp);
              {
                VScriptArray &A = *(VScriptArray *)sp[-3].p;
                if (A.Is2D()) { cstDump(origip); Sys_Error("Cannot sort non-flat arrays"); }
                // get self
                VObject *dgself = (VObject *)sp[-2].p;
                // get pointer to the delegate
                VMethod *dgfunc = (VMethod *)sp[-1].p;
                if (!dgself) {
                  if (!dgfunc || (dgfunc->Flags&FUNC_Static) == 0) { cstDump(origip); Sys_Error("Delegate is not initialised"); }
                }
                if (!dgfunc) { cstDump(origip); Sys_Error("Delegate is not initialised (empty method)"); }
                // fix stack, so we can call a delegate properly
                pr_stackPtr = sp;
                cstFixTopIPSP(ip);
                if (!A.Sort(Type, dgself, dgfunc)) { cstDump(origip); Sys_Error("Internal error in array sorter"); }
              }
              current_func = func;
              sp = pr_stackPtr;
              //fprintf(stderr, "sp=%p\n", sp);
              sp -= 3;
              break;
            // [-1]: idx1
            // [-2]: idx0
            // [-3]: *dynarray
            case OPC_DynArrDispatch_DynArraySwap1D:
              {
                VScriptArray &A = *(VScriptArray *)sp[-3].p;
                if (A.Is2D()) { cstDump(origip); Sys_Error("Cannot swap items of non-flat arrays"); }
                int idx0 = sp[-2].i;
                int idx1 = sp[-1].i;
                if (idx0 < 0 || idx0 >= A.length()) { cstDump(origip); Sys_Error("Index %d outside the bounds of an array (%d)", idx0, A.length()); }
                if (idx1 < 0 || idx1 >= A.length()) { cstDump(origip); Sys_Error("Index %d outside the bounds of an array (%d)", idx1, A.length()); }
                A.SwapElements(idx0, idx1, Type);
                sp -= 3;
              }
              break;
            // [-2]: *dynarray
            // [-1]: size
            case OPC_DynArrDispatch_DynArraySetSize1D:
              {
                VScriptArray &A = *(VScriptArray *)sp[-2].p;
                int newsize = sp[-1].i;
                if (newsize < 0) { cstDump(origip); Sys_Error("Array size %d is negative", newsize); }
                if (newsize > MaxDynArrayLength) { cstDump(origip); Sys_Error("Array size %d is too big", newsize); }
                A.SetNum(newsize, Type); // this will flatten it
                sp -= 2;
              }
              break;
            // [-3]: *dynarray
            // [-2]: size1
            // [-1]: size2
            case OPC_DynArrDispatch_DynArraySetSize2D:
              {
                VScriptArray &A = *(VScriptArray *)sp[-3].p;
                int newsize1 = sp[-2].i;
                int newsize2 = sp[-1].i;
                if (newsize1 < 1) { cstDump(origip); Sys_Error("Array size %d is too small", newsize1); }
                if (newsize1 > MaxDynArrayLength) { cstDump(origip); Sys_Error("Array size %d is too big", newsize1); }
                if (newsize2 < 1) { cstDump(origip); Sys_Error("Array size %d is too small", newsize2); }
                if (newsize2 > MaxDynArrayLength) { cstDump(origip); Sys_Error("Array size %d is too big", newsize2); }
                A.SetSize2D(newsize1, newsize2, Type);
                sp -= 3;
              }
              break;
            // [-1]: *dynarray
            case OPC_DynArrDispatch_DynArrayAlloc:
              {
                VScriptArray &A = *(VScriptArray *)sp[-1].p;
                sp[-1].p = A.Alloc(Type);
              }
              break;
            default:
              cstDump(origip);
              Sys_Error("Dictionary opcode %d is not implemented", ip[-1]);
          }
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
        ++ip;
        pushOldIterator((VScriptIterator *)sp[-1].p);
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IteratorNext)
        if (iterStackUsed == 0) { cstDump(ip); Sys_Error("VM: No active iterators (but we should have one)"); }
        ++ip;
        sp->i = iterStack[iterStackUsed-1]->GetNext();
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IteratorPop)
        if (iterStackUsed == 0) { cstDump(ip); Sys_Error("VM: No active iterators (but we should have one)"); }
        popOldIterator();
        ++ip;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DoWriteOne)
        {
          ++ip;
          VFieldType Type = VFieldType::ReadTypeMem(ip);
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
        (void)VFieldType::ReadTypeMem(ip);
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
          case OPC_Builtin_Sqrt: sp[-1].f = sqrtf(sp[-1].f); break;
          case OPC_Builtin_ATan2: sp[-2].f = matan(sp[-2].f, sp[-1].f); --sp; break;
          case OPC_Builtin_VecLength:
            if (!isFiniteF(sp[-1].f) || !isFiniteF(sp[-2].f) || !isFiniteF(sp[-3].f)) { cstDump(ip); Sys_Error("vector is INF/NAN"); }
            sp[-3].f = sqrtf(sp[-1].f*sp[-1].f+sp[-2].f*sp[-2].f+sp[-3].f*sp[-3].f);
            sp -= 2;
            if (!isFiniteF(sp[-1].f)) { cstDump(ip); Sys_Error("vector length is INF/NAN"); }
            break;
          case OPC_Builtin_VecLength2D:
            if (!isFiniteF(sp[-1].f) || !isFiniteF(sp[-2].f) || !isFiniteF(sp[-3].f)) { cstDump(ip); Sys_Error("vector is INF/NAN"); }
            sp[-3].f = sqrtf(sp[-2].f*sp[-2].f+sp[-3].f*sp[-3].f);
            sp -= 2;
            if (!isFiniteF(sp[-1].f)) { cstDump(ip); Sys_Error("vector length2D is INF/NAN"); }
            break;
          case OPC_Builtin_VecNormalize:
            {
              TVec v(sp[-3].f, sp[-2].f, sp[-1].f);
              v.normaliseInPlace();
              // normalizing zero vector should produce zero, not nan/inf
              if (!v.isValid()) v = TVec(0, 0, 0);
              sp[-1].f = v.z;
              sp[-2].f = v.y;
              sp[-3].f = v.x;
              break;
            }
          case OPC_Builtin_VecNormalize2D:
            {
              TVec v(sp[-3].f, sp[-2].f, sp[-1].f);
              v = normalise2D(v);
              // normalizing zero vector should produce zero, not nan/inf
              if (!v.isValid()) v = TVec(0, 0, 0);
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
              if (!isFiniteF(sp[-1].f)) { cstDump(ip); Sys_Error("dotproduct result is INF/NAN"); }
              break;
            }
          case OPC_Builtin_VecDot2D:
            {
              TVec v2(sp[-3].f, sp[-2].f, sp[-1].f);
              sp -= 3;
              TVec v1(sp[-3].f, sp[-2].f, sp[-1].f);
              sp -= 2;
              sp[-1].f = DotProduct2D(v1, v2);
              if (!isFiniteF(sp[-1].f)) { cstDump(ip); Sys_Error("dotproduct2d result is INF/NAN"); }
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
              if (!v1.isValid()) { cstDump(ip); Sys_Error("crossproduct result is INF/NAN"); }
              break;
            }
          case OPC_Builtin_VecCross2D:
            {
              TVec v2(sp[-3].f, sp[-2].f, sp[-1].f);
              sp -= 3;
              TVec v1(sp[-3].f, sp[-2].f, sp[-1].f);
              sp -= 2;
              sp[-1].f = CrossProduct2D(v1, v2);
              if (!isFiniteF(sp[-1].f)) { cstDump(ip); Sys_Error("crossproduct2d result is INF/NAN"); }
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
          case OPC_Builtin_NameToIIndex: break; // no, really, it is THAT easy
          case OPC_Builtin_VectorClamp:
            {
              TVec v(sp[-5].f, sp[-4].f, sp[-3].f);
              float vmin = sp[-2].f;
              float vmax = sp[-1].f;
              if (!isFiniteF(vmin)) vmin = 0;
              if (!isFiniteF(vmax)) vmax = 0;
              if (!v.isValid()) {
                v.x = v.y = v.z = vmin;
              } else {
                v.x = MID(vmin, v.x, vmax);
                v.y = MID(vmin, v.y, vmax);
                v.z = MID(vmin, v.z, vmax);
              }
              sp -= 2;
              sp[-1].f = v.z;
              sp[-2].f = v.y;
              sp[-3].f = v.x;
              break;
            }
          default: cstDump(ip); Sys_Error("Unknown builtin");
        }
        ip += 2;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DictDispatch)
        {
          vuint8 *origip = ip;
          ++ip;
          VFieldType KType = VFieldType::ReadTypeMem(ip);
          VFieldType VType = VFieldType::ReadTypeMem(ip);
          vuint8 dcopcode = *ip++;
          ExecDictOperator(origip, ip, sp, KType, VType, dcopcode);
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DupPOD)
        //{ fprintf(stderr, "OPC_DupPOD at %6u in FUNCTION `%s`; sp=%d\n", (unsigned)(ip-func->Statements.Ptr()), *func->GetFullName(), (int)(sp-pr_stack)); cstDump(ip); }
        ++ip;
        sp->p = sp[-1].p; // pointer copies everything
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_SwapPOD)
        ++ip;
        {
          void *tmp = sp[-1].p;
          sp[-1].p = sp[-2].p;
          sp[-2].p = tmp;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DropPOD)
        ++ip;
        --sp;
        PR_VM_BREAK;

      PR_VM_DEFAULT
        cstDump(ip);
        Sys_Error("Invalid opcode %d", *ip);
    }
  }
  goto func_loop;

  #ifdef VMEXEC_RUNDUMP
  printIndent(); fprintf(stderr, "LEAVING VC FUNCTION `%s`; sp=%d\n", *func->GetFullName(), (int)(sp-pr_stack)); leaveIndent();
  #endif
  unguardf(("(%s %d)", *func->GetFullName(), (int)(ip-func->Statements.Ptr())));
}


struct CurrFuncHolder {
  VMethod **place;
  VMethod *prevfunc;

  CurrFuncHolder (VMethod **aplace, VMethod *newfunc) : place(aplace), prevfunc(*aplace) { *aplace = newfunc; }
  ~CurrFuncHolder () { *place = prevfunc; place = nullptr; }
  CurrFuncHolder &operator = (const CurrFuncHolder &);
};


//==========================================================================
//
//  VObject::ExecuteFunction
//
//  ALL arguments must be pushed
//
//==========================================================================
VFuncRes VObject::ExecuteFunction (VMethod *func) {
  guard(VObject::ExecuteFunction);

  //fprintf(stderr, "*** VObject::ExecuteFunction: <%s>\n", *func->GetFullName());

  VFuncRes ret;

  // run function
  {
    CurrFuncHolder cfholder(&current_func, func);
#ifdef VMEXEC_RUNDUMP
    enterIndent(); printIndent(); fprintf(stderr, "***ENTERING `%s` (RETx); sp=%d (MAX:%u)\n", *func->GetFullName(), (int)(pr_stackPtr-pr_stack), (unsigned)MAX_PROG_STACK);
#endif
    RunFunction(func);
  }

  // get return value
  if (func->ReturnType.Type) {
    const int tsz = func->ReturnType.GetStackSize()/4;
    switch (func->ReturnType.Type) {
      case TYPE_Void: abort(); // the thing that should not be
      case TYPE_Int: check(tsz == 1); ret = VFuncRes(pr_stackPtr[-1].i); break;
      case TYPE_Byte: check(tsz == 1); ret = VFuncRes(pr_stackPtr[-1].i); break;
      case TYPE_Bool: check(tsz == 1); ret = VFuncRes(pr_stackPtr[-1].i); break;
      case TYPE_Float: check(tsz == 1); ret = VFuncRes(pr_stackPtr[-1].f); break;
      case TYPE_Name: check(tsz == 1); ret = VFuncRes(*(VName *)(&pr_stackPtr[-1])); break;
      case TYPE_String: check(tsz == 1); ret = VFuncRes(*(VStr *)(&pr_stackPtr[-1].p)); ((VStr *)(&pr_stackPtr[-1].p))->clear(); break;
      case TYPE_Reference: check(tsz == 1); ret = VFuncRes((VClass *)(pr_stackPtr[-1].p)); break;
      case TYPE_Class: check(tsz == 1); ret = VFuncRes((VObject *)(pr_stackPtr[-1].p)); break;
      case TYPE_State: check(tsz == 1); ret = VFuncRes((VState *)(pr_stackPtr[-1].p)); break;
      case TYPE_Vector: check(tsz == 3); ret = VFuncRes(pr_stackPtr[-3].f, pr_stackPtr[-2].f, pr_stackPtr[-1].f); break;
      default: break;
    }
    pr_stackPtr -= tsz;
    /*
    if (tsz == 1) {
      --pr_stackPtr;
      ret = *pr_stackPtr;
      //FIXME
      pr_stackPtr->p = nullptr; // for strings, caller should take care of releasing a string
    } else {
      pr_stackPtr -= tsz;
    }
    */
  }
#ifdef VMEXEC_RUNDUMP
  printIndent(); fprintf(stderr, "***LEAVING `%s` (RETx); sp=%d, (MAX:%u)\n", *func->GetFullName(), (int)(pr_stackPtr-pr_stack), (unsigned)MAX_PROG_STACK); leaveIndent();
#endif

#ifdef CHECK_FOR_EMPTY_STACK
  // after executing base function stack must be empty
  if (!current_func && pr_stackPtr != pr_stack+1) {
    cstDump(nullptr);
    Sys_Error("ExecuteFunction: Stack is not empty after executing function:\n%s\nstack=%p, oldsp=%p, diff=%d", *func->Name, pr_stack, pr_stackPtr, (int)(ptrdiff_t)(pr_stack-pr_stackPtr));
    #ifdef VMEXEC_RUNDUMP
    abort();
    #endif
  }
#endif

#ifdef CHECK_STACK_UNDERFLOW
  // check if stack wasn't underflowed
  if (pr_stack[0].i != STACK_ID) {
    cstDump(nullptr);
    Sys_Error("ExecuteFunction: Stack underflow in %s", *func->Name);
    #ifdef VMEXEC_RUNDUMP
    abort();
    #endif
  }
#endif

#ifdef CHECK_STACK_OVERFLOW
  // check if stack wasn't overflowed
  if (pr_stack[MAX_PROG_STACK-1].i != STACK_ID) {
    cstDump(nullptr);
    Sys_Error("ExecuteFunction: Stack overflow in `%s`", *func->Name);
    #ifdef VMEXEC_RUNDUMP
    abort();
    #endif
  }
#endif

  // all done
  return ret;
  unguardf(("(%s)", *func->GetFullName()));
}


//==========================================================================
//
//  VObject::ExecuteFunctionNoArgs
//
//  `self` must be pushed
//
//==========================================================================
VFuncRes VObject::ExecuteFunctionNoArgs (VMethod *func) {
  if (!func) Sys_Error("ExecuteFunctionNoArgs: null func!");

  // placeholders for "ref" args
  int rints[VMethod::MAX_PARAMS];
  float rfloats[VMethod::MAX_PARAMS*3]; // for vectors too
  VStr rstrs[VMethod::MAX_PARAMS];
  //VScriptArray *rdarrays[VMethod::MAX_PARAMS];
  VName rnames[VMethod::MAX_PARAMS];
  void *rptrs[VMethod::MAX_PARAMS*2]; // various pointers (including delegates)
  int rintUsed = 0;
  int rfloatUsed = 0;
  int rstrUsed = 0;
  //int rdarrayUsed = 0;
  int rnameUsed = 0;
  int rptrUsed = 0;

  memset(rints, 0, sizeof(rints));
  memset(rfloats, 0, sizeof(rfloats));
  memset((void *)(&rstrs[0]), 0, sizeof(rstrs));
  //memset((void *)(&rdarrays[0]), 0, sizeof(rdarrays));
  memset((void *)(&rnames[0]), 0, sizeof(rnames));
  memset(rptrs, 0, sizeof(rptrs));

  if (func->NumParams > VMethod::MAX_PARAMS) Sys_Error("ExecuteFunctionNoArgs: function `%s` has too many parameters (%d)", *func->Name, func->NumParams); // sanity check
  // push default values
  for (int f = 0; f < func->NumParams; ++f) {
    // out/ref arg
    if ((func->ParamFlags[f]&(FPARM_Out|FPARM_Ref)) != 0) {
      if (func->ParamTypes[f].IsAnyArray()) Sys_Error("ExecuteFunctionNoArgs: function `%s`, argument #%d is ref/out array, this is not supported yet", *func->Name, f+1);
      switch (func->ParamTypes[f].Type) {
        case TYPE_Int:
        case TYPE_Byte:
        case TYPE_Bool:
          P_PASS_PTR(&rints[rintUsed]);
          ++rintUsed;
          break;
        case TYPE_Float:
          P_PASS_PTR(&rfloats[rfloatUsed]);
          ++rfloatUsed;
          break;
        case TYPE_Name:
          P_PASS_PTR(&rnames[rnameUsed]);
          ++rnameUsed;
          break;
        case TYPE_String:
          P_PASS_PTR(&rstrs[rstrUsed]);
          ++rstrUsed;
          break;
        case TYPE_Pointer:
        case TYPE_Reference:
        case TYPE_Class:
        case TYPE_State:
          P_PASS_PTR(&rptrs[rptrUsed]);
          ++rptrUsed;
          break;
        case TYPE_Vector:
          P_PASS_PTR(&rfloats[rfloatUsed]);
          rfloatUsed += 3;
          break;
        case TYPE_Delegate:
          P_PASS_PTR(&rptrs[rptrUsed]);
          rptrUsed += 2;
          break;
        default:
          Sys_Error("%s", va("ExecuteFunctionNoArgs: function `%s`, argument #%d is of bad type `%s`", *func->Name, f+1, *func->ParamTypes[f].GetName()));
          break;
      }
      if ((func->ParamFlags[f]&FPARM_Optional) != 0) P_PASS_BOOL(false); // "specified" flag
    } else {
      if ((func->ParamFlags[f]&FPARM_Optional) == 0) Sys_Error("ExecuteFunctionNoArgs: function `%s`, argument #%d is not optional!", *func->Name, f+1);
      // push empty values
      switch (func->ParamTypes[f].Type) {
        case TYPE_Int:
        case TYPE_Byte:
        case TYPE_Bool:
          P_PASS_INT(0);
          break;
        case TYPE_Float:
          P_PASS_FLOAT(0);
          break;
        case TYPE_Name:
          P_PASS_NAME(NAME_None);
          break;
        case TYPE_String:
          P_PASS_STR(VStr());
          break;
        case TYPE_Pointer:
        case TYPE_Reference:
        case TYPE_Class:
        case TYPE_State:
          P_PASS_PTR(nullptr);
          break;
        case TYPE_Vector:
          P_PASS_VEC(TVec(0, 0, 0));
          break;
        case TYPE_Delegate:
          P_PASS_PTR(nullptr);
          P_PASS_PTR(nullptr);
          break;
        default:
          Sys_Error("%s", va("ExecuteFunctionNoArgs: function `%s`, argument #%d is of bad type `%s`", *func->Name, f+1, *func->ParamTypes[f].GetName()));
          break;
      }
      P_PASS_BOOL(false); // "specified" flag
    }
  }

  VFuncRes res = ExecuteFunction(func);
  for (int f = rstrUsed-1; f >= 0; --f) rstrs[f].clear();
  return res;
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
  DumpProfileInternal(-1);
  DumpProfileInternal(1);
}


//==========================================================================
//
//  VObject::DumpProfileInternal
//
//  <0: only native; >0: only script; 0: everything
//
//==========================================================================
void VObject::DumpProfileInternal (int type) {
  const int MAX_PROF = 100;
  int profsort[MAX_PROF+1];
  int totalcount = 0;
  memset(profsort, 0, sizeof(profsort));
  for (int i = 0; i < VMemberBase::GMembers.Num(); ++i) {
    if (VMemberBase::GMembers[i]->MemberType != MEMBER_Method) continue;
    VMethod *Func = (VMethod *)VMemberBase::GMembers[i];
    if (!Func->Profile1) continue; // never called
    totalcount += Func->Profile2;
    if (type < 0 && (Func->Flags&FUNC_Native) == 0) continue;
    if (type > 0 && (Func->Flags&FUNC_Native) != 0) continue;
    int dpos = 0;
    while (dpos < MAX_PROF) {
      if (!profsort[dpos]) break;
      VMethod *f2 = (VMethod *)VMemberBase::GMembers[profsort[dpos]];
      if (f2->Profile2 < Func->Profile2) break;
      ++dpos;
    }
    if (dpos < MAX_PROF) {
      if (profsort[dpos]) memmove(profsort+dpos+1, profsort+dpos, (MAX_PROF-dpos)*sizeof(profsort[0]));
      profsort[dpos] = i;
    }
  }
  if (!totalcount) return;
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
    GCon->Logf("====== PROFILE ======");
#else
    fprintf(stderr, "====== PROFILE ======\n");
#endif
  for (int i = 0; i < MAX_PROF && profsort[i]; ++i) {
    VMethod *Func = (VMethod *)VMemberBase::GMembers[profsort[i]];
    if (!Func) continue;
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
    GCon->Logf("%6.2f%% (%9d) %9d %s",
      (double)Func->Profile2*100.0/(double)totalcount,
      (int)Func->Profile2, (int)Func->Profile1, *Func->GetFullName());
#else
    fprintf(stderr, "%6.2f%% (%9d) %9d %s\n",
      (double)Func->Profile2*100.0/(double)totalcount,
      (int)Func->Profile2, (int)Func->Profile1, *Func->GetFullName());
#endif
  }
}
