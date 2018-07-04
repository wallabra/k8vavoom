//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**  Copyright (C) 2018 Ketmar Dark
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
// ////////////////////////////////////////////////////////////////////////// //
// OPTIMIZER
//
//#define VCMOPT_DISABLE_OPTIMIZER

#define VCMOPT_DISABLE_ALL_OUTPUT

#ifndef VCMOPT_DISABLE_ALL_OUTPUT

//#define VCMCOPT_DUMP_FUNC_NAMES
//#define VCMCOPT_DISASM_FINAL_RESULT
//#define VCMCOPT_DISASM_FINAL_RESULT_ANYWAY

#define VCMCOPT_DEBUG_RETURN_CHECKER
#define VCMCOPT_DEBUG_RETURN_CHECKER_EXTRA

//#define VCMCOPT_DEBUG_DEAD_JUMP_KILLER
//#define VCMCOPT_NOTIFY_DEAD_JUMP_KILLER

//#define VCMCOPT_DEBUG_REDUNANT_JUMPS
//#define VCMCOPT_NOTIFY_REDUNANT_JUMPS

//#define VCMCOPT_DEBUG_SIMPLIFY_JUMP_CHAINS
//#define VCMCOPT_NOTIFY_SIMPLIFY_JUMP_CHAINS

//#define VCMCOPT_DEBUG_SIMPLIFY_JUMP_JUMP
//#define VCMCOPT_NOTIFY_SIMPLIFY_JUMP_JUMP

//#define VCMCOPT_DEBUG_DEADIF_SIMPLIFIER
//#define VCMCOPT_NOTIFY_DEADIF_SIMPLIFIER

#endif


// builtin codes
#define BUILTIN_OPCODE_INFO
#include "../progdefs.h"


#include "vc_mcssa.cpp"


// ////////////////////////////////////////////////////////////////////////// //
struct Instr;


// ////////////////////////////////////////////////////////////////////////// //
// main optimizer class (k8: well, `MC` stands for `machine code`, lol)
class VMCOptimizer {
friend struct Instr;

public:
  VMethod *func;
  TArray<FInstruction> *origInstrList;
  // instructions list
  Instr *ilistHead, *ilistTail;
  // all known jump instructions
  Instr *jplistHead, *jplistTail;
  int instrCount;
  // support list to ease indexed access
  TArray<Instr *> instrList;

private:
  Instr *getInstrAtSlow (int idx) const;

  Instr *getInstrAt (int idx) const;

  void disasmAll () const;

  void recalcJumpTargetCacheFor (Instr *it);

  void fixJumpTargetCache ();

  void appendToList (Instr *i);
  void appendToJPList (Instr *i);

  void removeInstr (Instr *it);
  void killInstr (Instr *it);

  //WARNING: copies contents of `src` to `dest`, but does no list reordering!
  void replaceInstr (Instr *dest, Instr *src);

  // range is inclusive
  bool canRemoveRange (int idx0, int idx1, Instr *ignoreThis=nullptr, Instr *ignoreThis1=nullptr);

  // range is inclusive
  void killRange (int idx0, int idx1);

public:
  VMCOptimizer (VMethod *afunc, TArray<FInstruction> &aorig);
  ~VMCOptimizer ();

  void clear ();

  void setupFrom (VMethod *afunc, TArray<FInstruction> *aorig);

  // this will copy result back to `aorig`, and will clear everything
  void finish ();

  // this does flood-fill search to see if all execution pathes are finished with `return`
  void checkReturns ();

  void optimizeAll ();
  void shortenInstructions ();

  inline int countInstrs () const { return instrCount; }

protected:
  // returns `true` if this path (and all its possible branches) reached `return` instruction
  // basically, it just marks all reachable instructions, and fails if it reached end-of-function
  // note that we don't try to catch endless loops, but simple endless loops are considered ok
  // (due to an accident)
  bool isPathEndsWithReturn (int iidx);

  void optimizeLoads ();
  void optimizeJumps ();

  bool removeDeadBranches ();

  bool removeRedunantJumps ();
  bool simplifyIfJumpJump ();
  bool simplifyIfJumps ();

  bool removeDeadIfs ();
  bool removeDeadWhiles ();

  void calcStackDepth ();
};


// ////////////////////////////////////////////////////////////////////////// //
// this is used to hold original instruction, and various extra optimizer data
struct Instr {
  // optimizer will fill those
  VMCOptimizer *owner;
  Instr *prev, *next; // in main list
  Instr *jpprev, *jpnext; // in "jump" list
  // fields from the original
  vint32 Address;
  vint32 Opcode;
  vint32 Arg1;
  vint32 Arg2;
  bool Arg1IsFloat;
  vint32 spcur; // current stack offset (from 0th parameter)
  vint32 spdelta;
  VMemberBase *Member;
  VName NameArg;
  VFieldType TypeArg;
  TLocation loc;
  // copied from statement info list
  int opcArgType;
  // optimizer internal data
  int idx; // instruction index, used to check/fix jumps
  int origIdx; // index in original instruction array
  bool meJumpTarget; // `true` if this instr is a jump target
  // used for return checking
  int retflag; // 0: not visited; -1: processing it right now; 2: determined to return

  Instr (VMCOptimizer *aowner, const FInstruction &i)
    : owner(aowner)
    , prev(nullptr), next(nullptr)
    , jpprev(nullptr), jpnext(nullptr)
    , Address(i.Address)
    , Opcode(i.Opcode)
    , Arg1(i.Arg1)
    , Arg2(i.Arg2)
    , Arg1IsFloat(i.Arg1IsFloat)
    , spcur(-1) // unknown
    , spdelta(0)
    , Member(i.Member)
    , NameArg(i.NameArg)
    , TypeArg(i.TypeArg)
    , loc(i.loc)
    , opcArgType(StatementInfo[Opcode].Args)
    , idx(-1)
    , origIdx(-1)
    , meJumpTarget(false)
    , retflag(0)
  {
  }

  inline void copyTo (FInstruction &dest) const {
    //dest.Address = Address; // no need to do this
    dest.Address = 0;
    dest.Opcode = Opcode;
    dest.Arg1 = Arg1;
    dest.Arg2 = Arg2;
    dest.Arg1IsFloat = Arg1IsFloat;
    dest.Member = Member;
    dest.NameArg = NameArg;
    dest.TypeArg = TypeArg;
    dest.loc = loc;
  }

  inline bool isSelfJump () const {
    switch (opcArgType) {
      case OPCARGS_BranchTargetB:
      case OPCARGS_BranchTargetNB:
      case OPCARGS_BranchTargetS:
      case OPCARGS_BranchTarget:
        return (Arg1 == idx);
      case OPCARGS_ByteBranchTarget:
      case OPCARGS_ShortBranchTarget:
      case OPCARGS_IntBranchTarget:
        return (Arg2 == idx);
    }
    return false;
  }

  inline bool isAnyBranch () const {
    switch (opcArgType) {
      case OPCARGS_BranchTargetB:
      case OPCARGS_BranchTargetNB:
      case OPCARGS_BranchTargetS:
      case OPCARGS_BranchTarget:
      case OPCARGS_ByteBranchTarget:
      case OPCARGS_ShortBranchTarget:
      case OPCARGS_IntBranchTarget:
        return true;
    }
    return false;
  }

  inline bool isCaseBranch () const {
    switch (opcArgType) {
      case OPCARGS_ByteBranchTarget:
      case OPCARGS_ShortBranchTarget:
      case OPCARGS_IntBranchTarget:
        return true;
    }
    return false;
  }

  inline int getBranchDest () const {
    switch (opcArgType) {
      case OPCARGS_BranchTargetB:
      case OPCARGS_BranchTargetNB:
      case OPCARGS_BranchTargetS:
      case OPCARGS_BranchTarget:
        return Arg1;
      case OPCARGS_ByteBranchTarget:
      case OPCARGS_ShortBranchTarget:
      case OPCARGS_IntBranchTarget:
        return Arg2;
    }
    return -1;
  }

  inline void setBranchDest (int newidx, bool dofix=true) {
    int oldidx = -1;
    switch (opcArgType) {
      case OPCARGS_BranchTargetB:
      case OPCARGS_BranchTargetNB:
      case OPCARGS_BranchTargetS:
      case OPCARGS_BranchTarget:
        oldidx = Arg1;
        Arg1 = newidx;
        break;
      case OPCARGS_ByteBranchTarget:
      case OPCARGS_ShortBranchTarget:
      case OPCARGS_IntBranchTarget:
        oldidx = Arg2;
        Arg2 = newidx;
        break;
      default:
        return;
    }
    if (!dofix || oldidx == newidx) return;
    // fix old target "has branch to me" flag
    if (oldidx >= 0 && oldidx < owner->countInstrs()) {
      owner->recalcJumpTargetCacheFor(owner->getInstrAt(oldidx));
    }
    // fix new target "has branch to me" flag
    if (newidx >= 0 && newidx < owner->countInstrs()) {
      owner->getInstrAt(newidx)->meJumpTarget = true;
    }
  }

  inline bool isReturn () const {
    switch (Opcode) {
      case OPC_Return:
      case OPC_ReturnL:
      case OPC_ReturnV:
        return true;
    }
    return false;
  }

  inline bool isGoto () const {
    switch (Opcode) {
      case OPC_GotoB:
      case OPC_GotoNB:
      case OPC_GotoS:
      case OPC_Goto:
        return true;
    }
    return false;
  }

  inline bool isMeJumpTarget () const {
    return meJumpTarget;
  }

  inline bool isRemovable () const {
    return !meJumpTarget;
  }

  inline bool isPushInt () const {
    switch (Opcode) {
      case OPC_PushNumber0:
      case OPC_PushNumber1:
      case OPC_PushNumberB:
      case OPC_PushNumberS:
      case OPC_PushNumber:
        return true;
    }
    return false;
  }

  inline int getPushIntValue () const {
    switch (Opcode) {
      case OPC_PushNumber0:
        return 0;
      case OPC_PushNumber1:
        return 1;
      case OPC_PushNumberB:
        return Arg1&0xff;
      case OPC_PushNumberS:
        return (vint32)(vint16)(Arg1&0xffff);
      case OPC_PushNumber:
        return Arg1;
    }
    return false;
  }

  // returns sp delta
  void setStackOffsets (int cursp) {
    spcur = cursp;
    spdelta = 0;
    switch (Opcode) {
      case OPC_Done:
        return;

      // call / return
      case OPC_Call:
      case OPC_VCall:
      case OPC_VCallB:
        spdelta = -(Arg2-0); // pop arguments
        spdelta += ((VMethod *)Member)->ReturnType.GetStackSize()/4;
        //if (((VMethod *)Member)->Flags&FUNC_VarArgs) spdelta += 1;
        if (((VMethod *)Member)->Flags&FUNC_Static) spdelta += 1;
        return;
      case OPC_PushVFunc:
      case OPC_PushFunc:
        spdelta = 1;
        return;
      case OPC_DelegateCall:
      case OPC_DelegateCallS:
      case OPC_DelegateCallB:
        spdelta = -Arg2;
        spdelta += ((VField *)Member)->Type.Function->ReturnType.GetStackSize()/4;
        return;
      case OPC_DelegateCallPtr:
        spdelta = -Arg2-1; // delegate pointer
        spdelta += TypeArg.Function->ReturnType.GetStackSize()/4;
        return;
      case OPC_Return:
        return;
      case OPC_ReturnL:
        spdelta = -1;
        return;
      case OPC_ReturnV:
        spdelta = -3;
        return;

      // branching
      case OPC_GotoB:
      case OPC_GotoNB:
      case OPC_GotoS:
      case OPC_Goto:
        return;
      case OPC_IfGotoB:
      case OPC_IfGotoNB:
      case OPC_IfGotoS:
      case OPC_IfGoto:
      case OPC_IfNotGotoB:
      case OPC_IfNotGotoNB:
      case OPC_IfNotGotoS:
      case OPC_IfNotGoto:
        spdelta = -1;
        return;
      case OPC_CaseGotoB:
      case OPC_CaseGotoS:
      case OPC_CaseGoto:
        return;

      // push constants
      case OPC_PushNumber0:
      case OPC_PushNumber1:
      case OPC_PushNumberB:
      case OPC_PushNumberS:
      case OPC_PushNumber:
      case OPC_PushName:
      case OPC_PushNameS:
      case OPC_PushNameB:
      case OPC_PushString:
      case OPC_PushClassId:
      case OPC_PushState:
      case OPC_PushNull:
        spdelta = 1;
        return;

      // loading of variables
      case OPC_LocalAddress0:
      case OPC_LocalAddress1:
      case OPC_LocalAddress2:
      case OPC_LocalAddress3:
      case OPC_LocalAddress4:
      case OPC_LocalAddress5:
      case OPC_LocalAddress6:
      case OPC_LocalAddress7:
      case OPC_LocalAddressB:
      case OPC_LocalAddressS:
      case OPC_LocalAddress:
      case OPC_LocalValue0:
      case OPC_LocalValue1:
      case OPC_LocalValue2:
      case OPC_LocalValue3:
      case OPC_LocalValue4:
      case OPC_LocalValue5:
      case OPC_LocalValue6:
      case OPC_LocalValue7:
      case OPC_LocalValueB:
        spdelta = 1;
        return;
      case OPC_VLocalValueB:
        spdelta = 3;
        return;
      case OPC_StrLocalValueB:
        spdelta = 1;
        return;
      case OPC_Offset:
      case OPC_OffsetS:
      case OPC_OffsetB:
        return;
      case OPC_FieldValue:
      case OPC_FieldValueS:
      case OPC_FieldValueB:
        return;
      case OPC_VFieldValue:
      case OPC_VFieldValueS:
      case OPC_VFieldValueB:
        spdelta = 2;
        return;
      case OPC_PtrFieldValue:
      case OPC_PtrFieldValueS:
      case OPC_PtrFieldValueB:
      case OPC_StrFieldValue:
      case OPC_StrFieldValueS:
      case OPC_StrFieldValueB:
        return;
      case OPC_SliceFieldValue:
        spdelta = 1;
        return;
      case OPC_ByteFieldValue:
      case OPC_ByteFieldValueS:
      case OPC_ByteFieldValueB:
      case OPC_Bool0FieldValue:
      case OPC_Bool0FieldValueS:
      case OPC_Bool0FieldValueB:
      case OPC_Bool1FieldValue:
      case OPC_Bool1FieldValueS:
      case OPC_Bool1FieldValueB:
      case OPC_Bool2FieldValue:
      case OPC_Bool2FieldValueS:
      case OPC_Bool2FieldValueB:
      case OPC_Bool3FieldValue:
      case OPC_Bool3FieldValueS:
      case OPC_Bool3FieldValueB:
        return;
      case OPC_CheckArrayBounds: // won't pop index
        return;
      case OPC_ArrayElement:
      case OPC_ArrayElementS:
      case OPC_ArrayElementB:
      case OPC_SliceElement:
        spdelta = -1;
        return;
      case OPC_OffsetPtr:
        spdelta = 0;
        return;
      case OPC_PushPointed:
        spdelta = 0;
        return;
      case OPC_PushPointedSlice:
        spdelta = 1;
        return;
      case OPC_PushPointedSliceLen:
        spdelta = 0;
        return;
      case OPC_VPushPointed:
        spdelta = 2;
        return;
      case OPC_PushPointedPtr:
      case OPC_PushPointedByte:
      case OPC_PushBool0:
      case OPC_PushBool1:
      case OPC_PushBool2:
      case OPC_PushBool3:
      case OPC_PushPointedStr:
        return;
      case OPC_PushPointedDelegate:
        spdelta = 1;
        return;

      // integer opeartors
      case OPC_Add:
      case OPC_Subtract:
      case OPC_Multiply:
      case OPC_Divide:
      case OPC_Modulus:
      case OPC_Equals:
      case OPC_NotEquals:
      case OPC_Less:
      case OPC_Greater:
      case OPC_LessEquals:
      case OPC_GreaterEquals:
        spdelta = -1;
        return;
      case OPC_NegateLogical:
        return;
      case OPC_AndBitwise:
      case OPC_OrBitwise:
      case OPC_XOrBitwise:
      case OPC_LShift:
      case OPC_RShift:
        spdelta = -1;
        return;
      case OPC_UnaryMinus:
      case OPC_BitInverse:
        return;

      // increment / decrement
      case OPC_PreInc:
      case OPC_PreDec:
      case OPC_PostInc:
      case OPC_PostDec:
        return;
      case OPC_IncDrop:
      case OPC_DecDrop:
        spdelta = -1;
        return;

      // integer assignment operators
      case OPC_AssignDrop:
      case OPC_AddVarDrop:
      case OPC_SubVarDrop:
      case OPC_MulVarDrop:
      case OPC_DivVarDrop:
      case OPC_ModVarDrop:
      case OPC_AndVarDrop:
      case OPC_OrVarDrop:
      case OPC_XOrVarDrop:
      case OPC_LShiftVarDrop:
      case OPC_RShiftVarDrop:
        spdelta = -2;
        return;

      // increment / decrement byte
      case OPC_BytePreInc:
      case OPC_BytePreDec:
      case OPC_BytePostInc:
      case OPC_BytePostDec:
        return;
      case OPC_ByteIncDrop:
      case OPC_ByteDecDrop:
        spdelta = -1;
        return;

      // byte assignment operators
      case OPC_ByteAssignDrop:
      case OPC_ByteAddVarDrop:
      case OPC_ByteSubVarDrop:
      case OPC_ByteMulVarDrop:
      case OPC_ByteDivVarDrop:
      case OPC_ByteModVarDrop:
      case OPC_ByteAndVarDrop:
      case OPC_ByteOrVarDrop:
      case OPC_ByteXOrVarDrop:
      case OPC_ByteLShiftVarDrop:
      case OPC_ByteRShiftVarDrop:
        spdelta = -2;
        return;

      // floating point operators
      case OPC_FAdd:
      case OPC_FSubtract:
      case OPC_FMultiply:
      case OPC_FDivide:
      case OPC_FEquals:
      case OPC_FNotEquals:
      case OPC_FLess:
      case OPC_FGreater:
      case OPC_FLessEquals:
      case OPC_FGreaterEquals:
        spdelta = -1;
        return;
      case OPC_FUnaryMinus:
        return;

      // floating point assignment operators
      case OPC_FAddVarDrop:
      case OPC_FSubVarDrop:
      case OPC_FMulVarDrop:
      case OPC_FDivVarDrop:
        spdelta = -2;
        return;

      // vector operators
      case OPC_VAdd:
      case OPC_VSubtract:
        spdelta = -3;
        return;
      case OPC_VPreScale:
      case OPC_VPostScale:
      case OPC_VIScale:
        spdelta = -1;
        return;
      case OPC_VEquals:
      case OPC_VNotEquals:
        spdelta = -5;
        return;
      case OPC_VUnaryMinus:
      case OPC_VFixParam:
        return;

      // vector assignment operators
      case OPC_VAssignDrop:
      case OPC_VAddVarDrop:
      case OPC_VSubVarDrop:
        spdelta = -4; // ptr and vector
        return;
      case OPC_VScaleVarDrop:
      case OPC_VIScaleVarDrop:
        spdelta = -2;
        return;

      case OPC_FloatToBool:
        return;
      case OPC_VectorToBool:
        spdelta = -2;
        return;

      // string operators
      case OPC_StrToBool:
        return;
      case OPC_StrEquals:
      case OPC_StrNotEquals:
      case OPC_StrLess:
      case OPC_StrLessEqu:
      case OPC_StrGreat:
      case OPC_StrGreatEqu:
        spdelta = -1;
        return;
      case OPC_StrLength:
        return;
      case OPC_StrCat:
        spdelta = -1;
        return;

      case OPC_StrGetChar:
        spdelta = -1;
        return;
      case OPC_StrSetChar:
        spdelta = -3;
        return;

      // string slicing and slice slicing
      case OPC_StrSlice:
        spdelta = -2;
        return;
      case OPC_StrSliceAssign:
        spdelta = -4;
        return;
      case OPC_SliceSlice:
        spdelta = -1;
        return;

      // string assignment operators
      case OPC_AssignStrDrop:
        spdelta = -2;
        return;

      // pointer opeartors
      case OPC_PtrEquals:
      case OPC_PtrNotEquals:
        spdelta = -1;
        return;
      case OPC_PtrToBool:
        return;

      // conversions
      case OPC_IntToFloat:
      case OPC_FloatToInt:
      case OPC_StrToName:
      case OPC_NameToStr:
        return;

      // cleanup of local variables
      case OPC_ClearPointedStr:
      case OPC_ClearPointedStruct:
      case OPC_ZeroPointedStruct:
        spdelta = -1;
        return;

      // drop result
      case OPC_Drop:
        spdelta = -1;
        return;
      case OPC_VDrop:
        spdelta = -3;
        return;
      case OPC_DropStr:
        spdelta = -1;
        return;

      // special assignment operators
      case OPC_AssignPtrDrop:
      case OPC_AssignBool0:
      case OPC_AssignBool1:
      case OPC_AssignBool2:
      case OPC_AssignBool3:
        spdelta = -2;
        return;
      case OPC_AssignDelegate:
        spdelta = -3;
        return;

      // dynamic arrays
      case OPC_DynArrayElement:
      case OPC_DynArrayElementS:
      case OPC_DynArrayElementB:
        spdelta = -1;
        return;
      case OPC_DynArrayElementGrow:
        spdelta = -1;
        return;
      case OPC_DynArrayGetNum:
        return;
      case OPC_DynArraySetNum:
      case OPC_DynArraySetNumMinus:
      case OPC_DynArraySetNumPlus:
        spdelta = -2;
        return;
      case OPC_DynArrayInsert:
      case OPC_DynArrayRemove:
        spdelta = -3;
        return;

      // dynamic cast
      case OPC_DynamicCast:
      case OPC_DynamicClassCast:
        return;

      // access to the default object
      case OPC_GetDefaultObj:
      case OPC_GetClassDefaultObj:
        return;

      // iterators
      case OPC_IteratorInit:
        spdelta = -1;
        return;
      case OPC_IteratorNext:
        spdelta = 1;
        return;
      case OPC_IteratorPop:

      // scripted iterators
      case OPC_IteratorDtorAt:
      case OPC_IteratorFinish:

      // `write` and `writeln`
      case OPC_DoWriteOne:
        switch (TypeArg.Type) {
          case TYPE_Struct:
          case TYPE_Array:
          case TYPE_DynamicArray:
            spdelta = -1; // ptr
            break;
          default:
            spdelta = -(TypeArg.GetStackSize()/4);
            break;
        }
        return;
      case OPC_DoWriteFlush:
        return;

      // for printf-like varargs
      case OPC_DoPushTypePtr:
        spdelta = 1;
        return;

      // fill [-1] pointer with zeroes; int is length
      case OPC_ZeroByPtr:
        spdelta = -1;
        return;

      // get class pointer from pushed object
      case OPC_GetObjClassPtr:
        return;
      // [-2]: classptr; [-1]: classptr
      case OPC_ClassIsAClass:
        spdelta = -1;
        return;

      // builtins (k8: i'm short of opcodes, so...)
      case OPC_Builtin:
        switch (Arg1) {
          case OPC_Builtin_IntAbs:
          case OPC_Builtin_FloatAbs:
          case OPC_Builtin_IntSign:
          case OPC_Builtin_FloatSign:
            return;
          case OPC_Builtin_IntMin:
          case OPC_Builtin_IntMax:
          case OPC_Builtin_FloatMin:
          case OPC_Builtin_FloatMax:
            spdelta = -1;
            return;
          case OPC_Builtin_IntClamp:
          case OPC_Builtin_FloatClamp:
            spdelta = -2;
            return;
          case OPC_Builtin_FloatIsNaN:
          case OPC_Builtin_FloatIsInf:
          case OPC_Builtin_FloatIsFinite:
          case OPC_Builtin_DegToRad:
          case OPC_Builtin_RadToDeg:
          case OPC_Builtin_Sin:
          case OPC_Builtin_Cos:
          case OPC_Builtin_Tan:
          case OPC_Builtin_ASin:
          case OPC_Builtin_ACos:
          case OPC_Builtin_ATan:
          case OPC_Builtin_Sqrt:
            return;
          case OPC_Builtin_ATan2:
            spdelta = -1;
            return;
          case OPC_Builtin_VecLength:
          case OPC_Builtin_VecLength2D:
            spdelta = -2;
            return;
          case OPC_Builtin_VecNormalize:
          case OPC_Builtin_VecNormalize2D:
            return;
          case OPC_Builtin_VecDot:
          case OPC_Builtin_VecDot2D:
            spdelta = -5;
            return;
          case OPC_Builtin_VecCross:
            spdelta = -3;
            return;
          case OPC_Builtin_VecCross2D:
            spdelta = -5;
            return;
          case OPC_Builtin_RoundF2I:
          case OPC_Builtin_RoundF2F:
          case OPC_Builtin_TruncF2I:
          case OPC_Builtin_TruncF2F:
          case OPC_Builtin_FloatCeil:
          case OPC_Builtin_FloatFloor:
            return;
          // [-3]: a; [-2]: b, [-1]: delta
          case OPC_Builtin_FloatLerp:
          case OPC_Builtin_IntLerp:
          case OPC_Builtin_FloatSmoothStep:
          case OPC_Builtin_FloatSmoothStepPerlin:
            spdelta = -2;
            return;
          default: FatalError("Unknown builtin");
        }
    }
    FatalError("setStackOffsets: unhandled opcode %d", Opcode);
  }

  void disasm () const {
    // opcode
    fprintf(stderr, "(%6d) %c%6d: %s", origIdx, (meJumpTarget ? '*' : ' '), idx, StatementInfo[Opcode].name);
    switch (opcArgType) {
      case OPCARGS_None:
        break;
      case OPCARGS_Member:
        // name of the object
        fprintf(stderr, " %s", *Member->GetFullName());
        break;
      case OPCARGS_BranchTargetB:
      case OPCARGS_BranchTargetNB:
      case OPCARGS_BranchTargetS:
      case OPCARGS_BranchTarget:
        fprintf(stderr, " %d", Arg1);
        break;
      case OPCARGS_ByteBranchTarget:
      case OPCARGS_ShortBranchTarget:
      case OPCARGS_IntBranchTarget:
        fprintf(stderr, " %d, %d", Arg1, Arg2);
        break;
      case OPCARGS_Byte:
        fprintf(stderr, " %d (0x%02x)", Arg1, (unsigned)Arg1);
        break;
      case OPCARGS_Short:
        fprintf(stderr, " %d (0x%04x)", Arg1, (unsigned)Arg1);
        break;
      case OPCARGS_Int:
        if (Arg1IsFloat) {
          fprintf(stderr, " %f", *(const float *)&Arg1);
        } else {
          fprintf(stderr, " %d (0x%08x)", Arg1, (unsigned)Arg1);
        }
        break;
      case OPCARGS_Name:
        fprintf(stderr, " \'%s\'", *NameArg);
        break;
      case OPCARGS_String:
        // string
        {
          VMemberBase *PM = owner->func->Outer;
          while (PM->MemberType != MEMBER_Package) PM = PM->Outer;
          VPackage *Package = (VPackage *)PM;
          fprintf(stderr, " \"%s\"", &Package->Strings[Arg1]);
          //fprintf(stderr, " (%d)", Arg1);
        }
        break;
      case OPCARGS_FieldOffset:
        fprintf(stderr, " %s", *Member->Name);
        break;
      case OPCARGS_VTableIndex:
        fprintf(stderr, " %s", *Member->Name);
        break;
      case OPCARGS_VTableIndex_Byte:
      case OPCARGS_FieldOffset_Byte:
        fprintf(stderr, " %s %d", *Member->Name, Arg2);
        break;
      case OPCARGS_TypeSize:
      case OPCARGS_Type:
        fprintf(stderr, " %s", *TypeArg.GetName());
        break;
      case OPCARGS_Builtin:
        fprintf(stderr, " %s", StatementBuiltinInfo[Arg1].name);
        break;
      case OPCARGS_Member_Int:
        fprintf(stderr, " %s (%d)", *Member->GetFullName(), Arg2);
        break;
      case OPCARGS_Type_Int:
        fprintf(stderr, " %s (%d)", *TypeArg.GetName(), Arg2);
        break;
    }
    if (spcur >= 0) fprintf(stderr, " (sp:%d; delta:%d)", spcur, spdelta);
    fprintf(stderr, "\n");
  }
};


// ////////////////////////////////////////////////////////////////////////// //
VMCOptimizer::VMCOptimizer (VMethod *afunc, TArray<FInstruction> &aorig)
  : func(nullptr)
  , origInstrList(nullptr)
  , ilistHead(nullptr), ilistTail(nullptr)
  , jplistHead(nullptr), jplistTail(nullptr)
  , instrCount(0)
  , instrList()
{
  setupFrom(afunc, &aorig);
}


VMCOptimizer::~VMCOptimizer () {
  clear();
}


void VMCOptimizer::clear () {
  func = nullptr;
  origInstrList = nullptr;
  Instr *c = ilistHead;
  while (c) {
    Instr *x = c;
    c = c->next;
    delete x;
  }
  ilistHead = ilistTail = nullptr;
  jplistHead = jplistTail = nullptr;
  instrCount = 0;
  instrList.clear();
}


void VMCOptimizer::setupFrom (VMethod *afunc, TArray<FInstruction> *aorig) {
  clear();
  func = afunc;
  origInstrList = aorig;
  TArray<FInstruction> &olist = *aorig;
  // build ilist
  for (int f = 0; f < olist.length(); ++f) {
    // done must be the last one, and we aren't interested in it at all
    if (olist[f].Opcode == OPC_Done) {
      if (f != olist.length()-1) FatalError("VCOPT: OPC_Done is not a last one");
      break;
    }
    auto i = new Instr(this, olist[f]);
    i->origIdx = f;
    appendToList(i);
    // append to jplist (if necessary)
    if (i->isAnyBranch()) appendToJPList(i);
  }
  fixJumpTargetCache();
}


void VMCOptimizer::finish () {
  TArray<FInstruction> &olist = *origInstrList;
  olist.setLength(countInstrs()+1); // one for `Done`
  int iofs = 0;
  for (Instr *it = ilistHead; it; it = it->next, ++iofs) {
    if (iofs != it->idx) FatalError("VCOPT: internal optimizer inconsistency");
    it->copyTo(olist[iofs]);
  }
  // append `Done`
  olist[iofs].Address = 0;
  olist[iofs].Opcode = OPC_Done;
  olist[iofs].Arg1 = 0;
  olist[iofs].Arg2 = 0;
  olist[iofs].Arg1IsFloat = false;
  olist[iofs].Member = nullptr;
  olist[iofs].NameArg = NAME_None;
  olist[iofs].TypeArg = VFieldType(TYPE_Void);
  if (iofs > 0) {
    olist[iofs].loc = olist[iofs-1].loc;
  } else {
    olist[iofs].loc = TLocation();
  }
  // done
  clear();
}


// ////////////////////////////////////////////////////////////////////////// //
//FIXME: optimize!
Instr *VMCOptimizer::getInstrAtSlow (int idx) const {
  if (idx < 0 || idx >= instrCount) return nullptr;
  for (Instr *it = ilistHead; it; it = it->next) if (idx-- == 0) return it;
  return nullptr;
}


Instr *VMCOptimizer::getInstrAt (int idx) const {
  /*
  int num = instrCount;
  if (idx < 0 || idx >= num) return nullptr;
  if (idx <= num/2) {
    for (Instr *it = ilistHead; it; it = it->next) if (idx-- == 0) return it;
  } else {
    --num;
    for (Instr *it = ilistTail; it; it = it->prev, --num) {
      if (idx == num) {
        //!for (Instr *i2 = ilistHead; i2; i2 = i2->next) if (idx-- == 0) { if (i2 != it) abort(); break; }
        return it;
      }
    }
  }
  return nullptr;
  */
  if (idx < 0 || idx >= instrCount) return nullptr;
  //!if (instrList[idx] != getInstrAtSlow(idx)) abort();
  return instrList[idx];
}


void VMCOptimizer::disasmAll () const {
  for (const Instr *it = ilistHead; it; it = it->next) it->disasm();
}


//FIXME: optimize this!
void VMCOptimizer::recalcJumpTargetCacheFor (Instr *it) {
  if (!it) return;
  it->meJumpTarget = false;
  for (Instr *jit = jplistHead; jit; jit = jit->jpnext) {
    if (jit->getBranchDest() == it->idx) {
      it->meJumpTarget = true;
      return;
    }
  }
}


//FIXME: optimize this!
void VMCOptimizer::fixJumpTargetCache () {
  for (Instr *it = ilistHead; it; it = it->next) recalcJumpTargetCacheFor(it);
}


void VMCOptimizer::appendToList (Instr *i) {
  if (!i) return; // jist in case
  i->idx = instrCount++;
  i->next = nullptr;
  i->prev = ilistTail;
  if (ilistTail) ilistTail->next = i; else ilistHead = i;
  ilistTail = i;
  if (i->idx != instrList.length()) abort();
  instrList.append(i);
}


void VMCOptimizer::appendToJPList (Instr *i) {
  if (!i) return; // jist in case
  i->jpnext = nullptr;
  i->jpprev = jplistTail;
  if (jplistTail) jplistTail->jpnext = i; else jplistHead = i;
  jplistTail = i;
}


void VMCOptimizer::removeInstr (Instr *it) {
  if (!it) return; // just in case
  // fix jump indicies
  bool selfJump = it->isSelfJump();
  for (Instr *jit = jplistHead; jit; jit = jit->jpnext) {
    if (jit->getBranchDest() > it->idx) jit->setBranchDest(jit->getBranchDest()-1, false); // don't fix flags
  }
  // remove from index list
  instrList.removeAt(it->idx);
  // fix instruction indicies
  for (Instr *c = it->next; c; c = c->next) --c->idx;
  // remove from main list
  --instrCount;
  if (it->prev) it->prev->next = it->next; else ilistHead = it->next;
  if (it->next) it->next->prev = it->prev; else ilistTail = it->prev;
  // remove from jump list
  if (it->isAnyBranch()) {
    if (it->jpprev) it->jpprev->jpnext = it->jpnext; else jplistHead = it->jpnext;
    if (it->jpnext) it->jpnext->jpprev = it->jpprev; else jplistTail = it->jpprev;
    // there is no need to fix anything except our jump target
    if (!selfJump) {
      Instr *tgt = getInstrAt(it->getBranchDest());
      recalcJumpTargetCacheFor(tgt);
    }
  }
  it->prev = it->next = nullptr;
  it->jpprev = it->jpnext = nullptr;
}


void VMCOptimizer::killInstr (Instr *it) {
  if (!it) return; // just in case
  removeInstr(it);
  delete it;
}


// range is inclusive
void VMCOptimizer::killRange (int idx0, int idx1) {
  //fprintf(stderr, "  KILLING: (%d:%d)\n", idx0, idx1);
  while (idx0 <= idx1) {
    killInstr(getInstrAt(idx0));
    --idx1; // one less left
  }
}


void VMCOptimizer::replaceInstr (Instr *dest, Instr *src) {
  if (!dest || !src || dest == src) return; // sanity checks
  bool isDestJmp = dest->isAnyBranch();
  bool isSrcJmp = src->isAnyBranch();

  // are we replacing branch with non-branch?
  if (isDestJmp && !isSrcJmp) {
    // remove `dest` from jump list
    Instr *it = dest;
    if (it->jpprev) it->jpprev->jpnext = it->jpnext; else jplistHead = it->jpnext;
    if (it->jpnext) it->jpnext->jpprev = it->jpprev; else jplistTail = it->jpprev;
    // there is no need to fix anything except our jump target
    if (!it->isSelfJump()) {
      Instr *tgt = getInstrAt(it->getBranchDest());
      recalcJumpTargetCacheFor(tgt);
    }
    it->jpprev = it->jpnext = nullptr;
  }

  // copy data
  //dest->Address = src->Address; // no need to
  dest->Opcode = src->Opcode;
  dest->Arg1 = src->Arg1;
  dest->Arg2 = src->Arg2;
  dest->Arg1IsFloat = src->Arg1IsFloat;
  dest->Member = src->Member;
  dest->NameArg = src->NameArg;
  dest->TypeArg = src->TypeArg;
  //dest->loc = src->loc; // don't replace location, to get better debug info
  dest->opcArgType = src->opcArgType;
  dest->origIdx = src->origIdx; // so we can show some motion in debug dumps
  // don't touch index and jump target

  // are we replacing non-branch with branch?
  if (!isDestJmp && isSrcJmp) {
    // add new branch to a jump list
    Instr *it = dest;
    it->jpnext = nullptr;
    it->jpprev = jplistTail;
    if (jplistTail) jplistTail->jpnext = it; else jplistHead = it;
    jplistTail = it;
    // there is no need to fix jumptarget cache, as src is still in place,
    // so one more jump doesn't matter
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// range can be removed if there are no jumps *into* it
// range is inclusive
bool VMCOptimizer::canRemoveRange (int idx0, int idx1, Instr *ignoreThis, Instr *ignoreThis1) {
  if (idx0 < 0 || idx1 < 0 || idx0 > idx1 || idx0 >= instrCount || idx1 >= instrCount) return false;
  for (int f = idx0; f <= idx1; ++f) {
    Instr *it = getInstrAt(f);
    if (!it) return false;
    // is this a jump target?
    if (!it->isMeJumpTarget()) continue;
    // check if we have something outside that jumps here
    for (Instr *jp = jplistHead; jp; jp = jp->jpnext) {
      if (jp == ignoreThis || jp == ignoreThis1) continue;
      // if this jump is inside our region, ignore it
      if (jp->idx >= idx0 && jp->idx <= idx1) continue;
      // check destination
      int dest = jp->getBranchDest();
      if (dest >= idx0 && dest <= idx1) return false; // oops
    }
  }
  return true;
}


// ////////////////////////////////////////////////////////////////////////// //
/*
struct Exiter {
  VStr msg;

  Exiter (const VStr &amsg) { msg = amsg; }
  ~Exiter () { fprintf(stderr, "%s", *msg); }
};
*/


// returns `true` if this path (and all its possible branches) reached `return` instruction
// basically, it just marks all reachable instructions, and fails if it reached end-of-function
// note that we don't try to catch endless loops, but simple endless loops are considered ok
// (due to an accident)
// note that arriving at already marked instructions means "success"
bool VMCOptimizer::isPathEndsWithReturn (int iidx) {
  while (iidx >= 0 && iidx < instrCount) {
    Instr *it = getInstrAt(iidx);
    if (!it) return false; // oops
    if (it->idx != iidx) FatalError("VCOPT: internal inconsisitency (VMCOptimizer::isPathEndsWithReturn)");
    if (it->retflag) return true; // anyway
    // mark it as visited
    it->retflag = 1;
    // if this is return, we're done too
    if (it->isReturn()) return true;
    // follow branches
    if (it->isAnyBranch()) {
      // for goto, follow it unconditionally
      if (it->isGoto()) {
        iidx = it->getBranchDest();
        continue;
      }
      // for other branches, recurse on destination, then follow
      if (!isPathEndsWithReturn(it->getBranchDest())) return false; // oops
    }
    // go on
    ++iidx;
  }
  // don't bother rewinding, as we won't do more work anyway
  return false;
}


// this does flood-fill search to see if all execution pathes are finished with `return`
void VMCOptimizer::checkReturns () {
  // reset `visited` flag on each instruction
  for (int f = 0; f < instrCount; ++f) getInstrAt(f)->retflag = false;
  if (!isPathEndsWithReturn(0)) {
    ParseError(func->Loc, "Missing `return` in one of the pathes of function `%s`", *func->GetFullName());
#ifdef VCMCOPT_DEBUG_RETURN_CHECKER
    disasmAll();
#endif
  }
}


// ////////////////////////////////////////////////////////////////////////// //
void VMCOptimizer::shortenInstructions () {
  calcStackDepth();
  // two required steps
  optimizeLoads();
  optimizeJumps();
}


void VMCOptimizer::optimizeAll () {
#ifdef VCMOPT_DISABLE_OPTIMIZER
  return;
#else
#if defined(VCMCOPT_DUMP_FUNC_NAMES) || defined(VCMCOPT_DISASM_FINAL_RESULT) || defined(VCMCOPT_DISASM_FINAL_RESULT_ANYWAY)
  bool shown = false;
  shown =
#endif
  // this will remove "out-of-code" unreachable jumps (compiler can insert 'em on occasion)
  removeDeadBranches();
  // main optimizer loop: stop if nothing was optimized on each step
  for (;;) {
    // do this first, 'cause branch replacer can break if detection logic
    if (removeDeadIfs() || removeDeadWhiles()) {
#if defined(VCMCOPT_DUMP_FUNC_NAMES) || defined(VCMCOPT_DISASM_FINAL_RESULT)
      shown = true;
#endif
      continue;
    }
    if (!removeRedunantJumps() &&
        !simplifyIfJumpJump() &&
        !simplifyIfJumps() &&
        !removeDeadBranches()) break;
#if defined(VCMCOPT_DUMP_FUNC_NAMES) || defined(VCMCOPT_DISASM_FINAL_RESULT)
    shown = true;
#endif
  }
#if defined(VCMCOPT_DISASM_FINAL_RESULT_ANYWAY)
  shown = true;
#endif
#if defined(VCMCOPT_DUMP_FUNC_NAMES) || defined(VCMCOPT_DISASM_FINAL_RESULT) || defined(VCMCOPT_DISASM_FINAL_RESULT_ANYWAY)
  if (shown) {
#if defined(VCMCOPT_DISASM_FINAL_RESULT) || defined(VCMCOPT_DISASM_FINAL_RESULT_ANYWAY)
    fprintf(stderr, "####### OPTIMIZING: %s #######\n", *func->GetFullName());
    TArray<FInstruction> &olist = *origInstrList;
    TArray<Instr *> xilist;
    for (int f = 0; f < olist.length(); ++f) {
      if (olist[f].Opcode == OPC_Done) break;
      auto it = new Instr(this, olist[f]);
      it->origIdx = f;
      it->idx = f;
      //it->disasm();
      //delete it;
      xilist.append(it);
    }
    // build jump target cache
    for (int f = 0; f < xilist.length(); ++f) {
      Instr *it = xilist[f];
      it->meJumpTarget = false;
      for (int c = 0; c < xilist.length(); ++c) {
        Instr *j = xilist[c];
        if (j->isAnyBranch() && j->getBranchDest() == it->idx) {
          it->meJumpTarget = true;
          break;
        }
      }
    }
    for (int f = 0; f < xilist.length(); ++f) xilist[f]->disasm();
    for (int f = 0; f < xilist.length(); ++f) {
      delete xilist[f];
      xilist[f] = nullptr;
    }
    fprintf(stderr, "  === after ===\n");
    disasmAll();
#endif
    fprintf(stderr, "####### DONE OPTIMIZING: %s #######\n", *func->GetFullName());
  }
#endif
#endif
}


// ////////////////////////////////////////////////////////////////////////// //
// optimize various load/call instructions
void VMCOptimizer::optimizeLoads () {
  for (Instr *it = ilistHead; it; it = it->next) {
    Instr &insn = *it;
    switch (insn.Opcode) {
      case OPC_PushVFunc:
        // make sure class virtual table has been calculated
        insn.Member->Outer->PostLoad();
        //if (((VMethod *)insn.Member)->VTableIndex < 256) insn.Opcode = OPC_PushVFuncB;
        break;
      case OPC_VCall:
        // make sure class virtual table has been calculated
        insn.Member->Outer->PostLoad();
        if (((VMethod *)insn.Member)->VTableIndex < 256) insn.Opcode = OPC_VCallB;
        break;
      case OPC_DelegateCall:
        // make sure struct / class field offsets have been calculated
        insn.Member->Outer->PostLoad();
             if (((VField *)insn.Member)->Ofs < 256) insn.Opcode = OPC_DelegateCallB;
        else if (((VField *)insn.Member)->Ofs <= MAX_VINT16) insn.Opcode = OPC_DelegateCallS;
        break;
      case OPC_Offset:
      case OPC_FieldValue:
      case OPC_VFieldValue:
      case OPC_PtrFieldValue:
      case OPC_StrFieldValue:
      case OPC_SliceFieldValue:
      case OPC_ByteFieldValue:
      case OPC_Bool0FieldValue:
      case OPC_Bool1FieldValue:
      case OPC_Bool2FieldValue:
      case OPC_Bool3FieldValue:
        // no short form for slices
        if (insn.Opcode != OPC_SliceFieldValue) {
          // make sure struct / class field offsets have been calculated
          insn.Member->Outer->PostLoad();
               if (((VField *)insn.Member)->Ofs < 256) insn.Opcode += 2;
          else if (((VField *)insn.Member)->Ofs <= MAX_VINT16) ++insn.Opcode;
        }
        break;
      case OPC_ArrayElement:
             if (insn.TypeArg.GetSize() < 256) insn.Opcode = OPC_ArrayElementB;
        else if (insn.TypeArg.GetSize() < MAX_VINT16) insn.Opcode = OPC_ArrayElementS;
        break;
      case OPC_PushName:
             if (insn.NameArg.GetIndex() < 256) insn.Opcode = OPC_PushNameB;
        else if (insn.NameArg.GetIndex() < MAX_VINT16) insn.Opcode = OPC_PushNameS;
        break;
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// optimize various branch instructions (convert to a short form, if we can)
void VMCOptimizer::optimizeJumps () {
  // calculate approximate addresses for jump instructions
  int addr = 0;
  TArray<int> iaddrs; // to avoid constant list searches
  for (Instr *it = ilistHead; it; it = it->next) {
    iaddrs.append(addr);
    Instr &insn = *it;
    insn.Address = addr;
    addr += 1; // opcode itself
    switch (insn.opcArgType) {
      case OPCARGS_Member:
      case OPCARGS_String:
        addr += sizeof(void *);
        break;
      case OPCARGS_BranchTargetB:
      case OPCARGS_BranchTargetNB:
      case OPCARGS_Byte:
      case OPCARGS_NameB:
      case OPCARGS_FieldOffsetB:
      case OPCARGS_VTableIndexB:
      case OPCARGS_TypeSizeB:
        addr += 1;
        break;
      case OPCARGS_BranchTargetS:
      case OPCARGS_Short:
      case OPCARGS_NameS:
      case OPCARGS_FieldOffsetS:
      case OPCARGS_VTableIndex:
      case OPCARGS_VTableIndexB_Byte:
      case OPCARGS_FieldOffsetB_Byte:
      case OPCARGS_TypeSizeS:
        addr += 2;
        break;
      case OPCARGS_ByteBranchTarget:
      case OPCARGS_VTableIndex_Byte:
      case OPCARGS_FieldOffsetS_Byte:
        addr += 3;
        break;
      case OPCARGS_BranchTarget:
      case OPCARGS_ShortBranchTarget:
      case OPCARGS_Int:
      case OPCARGS_Name:
      case OPCARGS_FieldOffset:
      case OPCARGS_TypeSize:
        addr += 4;
        break;
      case OPCARGS_FieldOffset_Byte:
        addr += 5;
        break;
      case OPCARGS_IntBranchTarget:
        addr += 6;
        break;
      case OPCARGS_Type:
        addr += 8+sizeof(void*);
        break;
      case OPCARGS_Builtin:
        addr += 1;
        break;
      case OPCARGS_Member_Int:
        addr += sizeof(void *);
        break;
      case OPCARGS_Type_Int:
        addr += 4;
        break;
    }
  }

  // now optimize jump instructions
  for (Instr *it = jplistHead; it; it = it->jpnext) {
    Instr &insn = *it;
    if (insn.Opcode != OPC_IteratorDtorAt && insn.opcArgType == OPCARGS_BranchTarget && insn.getBranchDest() < iaddrs.length()) {
      vint32 ofs = iaddrs[insn.getBranchDest()]-insn.Address;
           if (ofs >= 0 && ofs < 256) insn.Opcode -= 3;
      else if (ofs < 0 && ofs > -256) insn.Opcode -= 2;
      else if (ofs >= MIN_VINT16 && ofs <= MAX_VINT16) insn.Opcode -= 1;
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// remove unreachable branches (those after ret/goto, and not a jump target)
// codegen can generate those, and optimizer can left those
bool VMCOptimizer::removeDeadBranches () {
  bool res = false;
#ifdef VCMCOPT_DEBUG_DEAD_JUMP_KILLER
  bool firstRemove = true;
#endif
  Instr *jit = jplistHead;
  while (jit) {
    Instr *it = jit;
    jit = jit->jpnext;
    // it should not be a first instruction, and should not be a jump target
    if (!it->prev || it->isMeJumpTarget()) continue; // just in case
    // previous should be `return` or `goto`
    if (!it->prev->isReturn() && !it->prev->isGoto()) continue;
    // ok, this is jump right after `return` or `goto`, and it is not a jump target
    // it is effectively dead, so remove it
#ifdef VCMCOPT_NOTIFY_DEAD_JUMP_KILLER
    fprintf(stderr, "::: %s ::: KILLING DEAD BRANCH at %d (orig:%d)\n", *func->GetFullName(), it->idx, it->origIdx); it->disasm();
#endif
    res = true;
#ifdef VCMCOPT_DEBUG_DEAD_JUMP_KILLER
    if (firstRemove) { disasmAll(); firstRemove = false; }
#endif
    killInstr(it);
  }
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
// remove `goto $+1;`
bool VMCOptimizer::removeRedunantJumps () {
  bool res = false;
  Instr *jit = jplistHead;
  while (jit) {
    Instr *it = jit;
    jit = jit->jpnext;
    // it should be `goto`, 'cause other branches can affect VM stack
    if (!it->isGoto() || it->getBranchDest() != it->idx+1) continue;
    // i found her!
    res = true;
#ifdef VCMCOPT_NOTIFY_REDUNANT_JUMPS
    fprintf(stderr, "removing jump $+1 at %d (orig:%d); jumptarget=%d\n", it->idx, it->origIdx, (int)(it->isMeJumpTarget()));
    it->disasm(); getInstrAt(it->getBranchDest())->disasm();
#endif
#ifdef VCMCOPT_DEBUG_REDUNANT_JUMPS
    fprintf(stderr, "=========== BEFORE ===========\n");
    disasmAll();
#endif
    // if it is a jump target, we can safely jump to the next instruction
    // (as this is what this code does anyway)
    if (it->isMeJumpTarget()) {
      // mark next instruction as jump target
      if (it->next->idx != it->idx+1) FatalError("VCOPT: internal error in (VMCOptimizer::removeRedunantJumps)");
      it->next->meJumpTarget = true;
      // fix jumps to `it`
      for (Instr *jit = jplistHead; jit; jit = jit->jpnext) {
        if (jit->getBranchDest() == it->idx) {
          // fix jump destination
          jit->setBranchDest(it->idx+1, false); // don't fix flags
        }
      }
    }
    killInstr(it);
#ifdef VCMCOPT_DEBUG_REDUNANT_JUMPS
    fprintf(stderr, "=========== AFTER ===========\n");
    disasmAll();
#endif
  }
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
// replace `brn op1; op1: goto op2;` with `brn op2;`
bool VMCOptimizer::simplifyIfJumps () {
  bool res = false;
  Instr *jit = jplistHead;
  while (jit) {
    Instr *it = jit;
    jit = jit->jpnext;
    // don't touch self-jumps
    if (it->isSelfJump()) continue;
    // get target instruction
    Instr *tgt = getInstrAt(it->getBranchDest());
    if (!tgt) {
      // this can happen for function without return
      // such function will be rejected by return checker, so don't bother
      continue;
      /*
      fprintf(stderr, "===================== %s =====================\n", *func->GetFullName());
      disasmAll();
      FatalError("VCOPT: failed to get destination for instruction %d (destination is %d)", it->idx, it->getBranchDest());
      */
    }
    // `goto` to `return`?
    if (it->isGoto() && tgt->isReturn()) {
      // yes, replace it with direct return
      res = true;
#ifdef VCMCOPT_NOTIFY_SIMPLIFY_JUMP_CHAINS
      fprintf(stderr, "replacing jump-to-return at %d (orig:%d)\n", it->idx, it->origIdx);
      it->disasm(); tgt->disasm();
#endif
#ifdef VCMCOPT_DEBUG_SIMPLIFY_JUMP_CHAINS
      fprintf(stderr, "=========== BEFORE ===========\n");
      //disasmAll();
      it->disasm();
      tgt->disasm();
#endif
      replaceInstr(it, tgt);
#ifdef VCMCOPT_DEBUG_SIMPLIFY_JUMP_CHAINS
      fprintf(stderr, "=========== AFTER ===========\n");
      //disasmAll();
      it->disasm();
      tgt->disasm();
#endif
      continue;
    }
    // from this point on, target must be `goto`
    if (!tgt->isGoto()) continue;
    // change destination
    res = true;
#ifdef VCMCOPT_NOTIFY_SIMPLIFY_JUMP_CHAINS
    fprintf(stderr, "replacing jump to %d at position %d to jump at %d (orig:%d)\n", it->getBranchDest(), it->idx, tgt->getBranchDest(), it->origIdx);
    it->disasm(); tgt->disasm();
#endif
#ifdef VCMCOPT_DEBUG_SIMPLIFY_JUMP_CHAINS
    fprintf(stderr, "=========== BEFORE ===========\n");
    //disasmAll();
    it->disasm();
    tgt->disasm();
#endif
    it->setBranchDest(tgt->getBranchDest());
#ifdef VCMCOPT_DEBUG_SIMPLIFY_JUMP_CHAINS
    fprintf(stderr, "=========== AFTER ===========\n");
    //disasmAll();
    it->disasm();
    tgt->disasm();
#endif
  }
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
// replace `If[Not]Goto $+2; Goto m;` to `If[!Not]Goto m;`, and remove `Goto m;`
bool VMCOptimizer::simplifyIfJumpJump () {
  bool res = false;
  Instr *jit = jplistHead;
  while (jit) {
    Instr *it = jit;
    jit = jit->jpnext;
    Instr *itnext = it->next;
    // this should be a kind of `if-jump`
    if (it->Opcode != OPC_IfNotGoto && it->Opcode != OPC_IfGoto) continue;
    // next should be valid, and current should not be a self-jump
    if (!itnext || it->isSelfJump()) continue;
    // next should be removable (i.e. it should not be a jump target)
    if (!itnext->isRemovable()) continue;
    // next should be `goto`
    if (!itnext->isGoto()) continue;
    // next-next should exist
    if (!itnext->next) continue;
    // jump should be at next+1
    if (it->getBranchDest() != itnext->next->idx) continue;
    // ok, replace it
    res = true;
#ifdef VCMCOPT_NOTIFY_SIMPLIFY_JUMP_JUMP
    fprintf(stderr, "replacing if+goto at %d (orig:%d)\n", it->idx, it->origIdx);
    it->disasm(); itnext->disasm(); getInstrAt(it->getBranchDest())->disasm();
#endif
#ifdef VCMCOPT_DEBUG_SIMPLIFY_JUMP_JUMP
    fprintf(stderr, "=========== BEFORE ===========\n");
    //disasmAll();
    it->disasm();
    itnext->disasm();
#endif
    // negate opcode
    it->Opcode = (it->Opcode == OPC_IfNotGoto ? OPC_IfGoto : OPC_IfNotGoto);
    // replace jump target
    it->setBranchDest(itnext->getBranchDest());
    // remove next jump (do some magic to continue iterating)
    if (itnext == jit) jit = jit->jpnext; // `jit` will be destroyed, so skip it
    killInstr(itnext);
#ifdef VCMCOPT_NOTIFY_SIMPLIFY_JUMP_JUMP
    fprintf(stderr, "  replaced if+goto at %d (orig:%d)\n", it->idx, it->origIdx);
    it->disasm(); it->next->disasm(); getInstrAt(it->getBranchDest())->disasm();
#endif
#ifdef VCMCOPT_DEBUG_SIMPLIFY_JUMP_JUMP
    fprintf(stderr, "=========== AFTER(KILL) ===========\n");
    //disasmAll();
    it->disasm();
#endif
    //return true;
  }
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
// if we have `PushNumber; If[Not]Goto m;`, remove if, and remove possible "else" branch
bool VMCOptimizer::removeDeadIfs () {
  Instr *jit = jplistHead;
  while (jit) {
    Instr *it = jit;
    jit = jit->jpnext;
    // should be `If[Not]Goto`
    if (it->Opcode != OPC_IfGoto && it->Opcode != OPC_IfNotGoto) continue;
    // should have previous instr
    Instr *itprev = it->prev;
    if (!itprev) continue;
    // it should be "push int literal"
    if (!itprev->isPushInt()) continue;
    // jump should be forward (and has some code inside)
    if (it->getBranchDest() <= it->idx+1) continue;
    // if [dest-1] is `goto`, this looks like `if/else`
    Instr *jdm1 = getInstrAt(it->getBranchDest()-1);
    if (!jdm1) {
      // this should not happen, but for invalid code it can happen nevertheless
      continue;
    }
    if (jdm1->isGoto()) {
      // check if that `goto` leads further down
      if (jdm1->getBranchDest() <= jdm1->idx+1) continue;
      int estart = jdm1->idx; // it starts from `goto endif`
      if (estart+1 != it->getBranchDest()) FatalError("VCOPT: internal error (0) at (VMCOptimizer::removeDeadIfs) (%d, %d)", estart, it->getBranchDest());
      int eend = jdm1->getBranchDest()-1; // range is inclusive
      if (estart > eend) FatalError("VCOPT: internal error (1) at (VMCOptimizer::removeDeadIfs) (%d,%d)", estart, eend);
      // check if we can remove `push` and `ifgoto` (we won't need 'em in any case)
      // moved down, to avoid some false positives
      //if (!canRemoveRange(itprev->idx, it->idx)) continue; // alas
      // yeah, it does; check what branch we should keep
      if ((it->Opcode == OPC_IfNotGoto && itprev->getPushIntValue() != 0) ||
          (it->Opcode == OPC_IfGoto && itprev->getPushIntValue() == 0))
      {
        // this is `true` branch
#ifdef VCMCOPT_DEBUG_DEADIF_SIMPLIFIER
        disasmAll();
#endif
#ifdef VCMCOPT_NOTIFY_DEADIF_SIMPLIFIER
        fprintf(stderr, "removing `else` branch (%d:%d), and cond (%d:%d)\n", estart, eend, itprev->idx, it->idx);
#endif
        // check if we can remove `push` and `ifgoto` (we won't need 'em in any case)
        if (!canRemoveRange(itprev->idx, it->idx)) continue; // alas
        // check if we can remove `else` (but ignore `If[Not]Goto`)
        if (!canRemoveRange(estart, eend, it)) continue;
        // yeah, we can!
        // first, remove `else` branch ('cause if we'll remove `IfXXX` first, indicies will break)
        killRange(estart, eend);
        // second, remove push and ifgoto
        killRange(itprev->idx, it->idx);
        // and exit, 'cause i don't want to bother with `jit` resyncing
        return true;
      } else {
        // this is `false` branch
#ifdef VCMCOPT_DEBUG_DEADIF_SIMPLIFIER
        disasmAll();
#endif
#ifdef VCMCOPT_NOTIFY_DEADIF_SIMPLIFIER
        fprintf(stderr, "removing `then` branch (%d:%d)\n", itprev->idx, estart);
#endif
        // check if we can remove `then` (estart is `goto endif`, we don't need it too)
        if (!canRemoveRange(itprev->idx, estart)) continue;
        // remove the whole `then` branch
        killRange(itprev->idx, estart);
        // and exit, 'cause i don't want to bother with `jit` resyncing
        return true;
      }
    } else {
      // no `goto` there, this looks like an `if` without `else`
      // should we remove `then` branch?
      if ((it->Opcode == OPC_IfNotGoto && itprev->getPushIntValue() == 0) ||
          (it->Opcode == OPC_IfGoto && itprev->getPushIntValue() != 0))
      {
        // yeah, check if it is possible at all
        int tend = it->getBranchDest()-1;
        if (!canRemoveRange(itprev->idx, tend)) continue;
        // kill it with fire!
#ifdef VCMCOPT_DEBUG_DEADIF_SIMPLIFIER
        disasmAll();
#endif
#ifdef VCMCOPT_NOTIFY_DEADIF_SIMPLIFIER
        fprintf(stderr, "removing lone `push/if/else` (%d:%d)\n", itprev->idx, tend);
#endif
        killRange(itprev->idx, tend);
        // and exit, 'cause i don't want to bother with `jit` resyncing
        return true;
      } else {
        // remove only `push` and `IfXXX`, as we have no `else` branch
        // check if we can remove `push` and `ifgoto`
        if (!canRemoveRange(itprev->idx, it->idx)) continue; // alas
        // kill it with fire!
#ifdef VCMCOPT_DEBUG_DEADIF_SIMPLIFIER
        disasmAll();
#endif
#ifdef VCMCOPT_NOTIFY_DEADIF_SIMPLIFIER
        fprintf(stderr, "removing lone `push/if` (%d:%d)\n", itprev->idx, it->idx);
#endif
        killRange(itprev->idx, it->idx);
        // and exit, 'cause i don't want to bother with `jit` resyncing
        return true;
      }
    }
  }
  return false;
}


// ////////////////////////////////////////////////////////////////////////// //
// remove/optimize dead `while {}` loops
// while loop looks like this:
//   goto lbl
//  start:
//   body
//  lbl:
//   cond
//   If[Not]Goto start
bool VMCOptimizer::removeDeadWhiles () {
  bool res = false;
  Instr *jit = jplistHead;
  while (jit) {
    Instr *it = jit;
    jit = jit->jpnext;
    // should be `If[Not]Goto`
    if (it->Opcode != OPC_IfGoto && it->Opcode != OPC_IfNotGoto) continue;
    // should have previous instr
    Instr *itprev = it->prev;
    if (!itprev) continue;
    // it should be "push int literal"
    if (!itprev->isPushInt()) continue;
    // jump should be backward
    if (it->getBranchDest() >= it->idx-1) continue;
    // if [dest-1] is `goto`, this looks like `if/else`
    Instr *jdm1 = getInstrAt(it->getBranchDest()-1);
    if (!jdm1) {
      // this should not happen, but for invalid code it can happen nevertheless
      continue;
    }
    // if this is not a `goto`, don't bother (such jumps will be optimized out by another pass)
    if (!jdm1->isGoto()) continue;
    // should we convert the jump into unconditional, or drop the whole loop?
    if ((it->Opcode == OPC_IfGoto && itprev->getPushIntValue() != 0) ||
        (it->Opcode == OPC_IfNotGoto && itprev->getPushIntValue() == 0))
    {
      // the easy case: convert to unconditional
      // check if we can remove `push`
      if (!canRemoveRange(itprev->idx, itprev->idx, it, jdm1)) continue; // alas, cannot remove `push`
      res = true;
      // convert opcode
      it->Opcode = OPC_Goto;
      // and remove push
      killInstr(itprev);
      // we can safely continue here
      continue;
    } else {
      // the hard case: remove the whole `while`
      // check if we can remove it (starting from the first jump, and down to check)
      if (!canRemoveRange(jdm1->idx, it->idx)) continue; // alas, cannot remove
      killRange(jdm1->idx, it->idx);
      // and don't continue, 'cause we can possibly lost `jit`
      return true;
    }
  }
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
void VMCOptimizer::calcStackDepth () {
  // reset
  for (Instr *it = ilistHead; it; it = it->next) { it->spcur = -1; it->spdelta = 0; }
  int cursp = 0;
  for (int f = 0; f < instrCount; ++f) {
    Instr *it = getInstrAt(f);
    if (it->spcur >= 0) cursp = it->spcur; // possibly set by branch
    it->setStackOffsets(cursp);
    cursp += it->spdelta;
    if (cursp < 0) {
      fprintf(stderr, "==== %s ====\n", *func->GetFullName()); disasmAll();
      FatalError("unbalanced stack(0) (f=%d; spcur=%d; cursp=%d; delta=%d)", f, it->spcur, cursp, it->spdelta);
    }
    if (it->isAnyBranch()) {
      int dest = it->getBranchDest();
      if (dest < 0 || dest >= instrCount) continue;
      int newsp = cursp-(it->isCaseBranch() ? 1 : 0);
      if (dest < f) {
        if (getInstrAt(dest)->spcur != newsp) {
          fprintf(stderr, "==== %s ====\n", *func->GetFullName()); disasmAll();
          FatalError("unbalanced stack(1) (f=%d; dest=%d; spcur=%d; newsp=%d)", f, dest, getInstrAt(dest)->spcur, newsp);
        }
      } else if (dest > f) {
        getInstrAt(dest)->spcur = newsp;
      }
    }
  }
  //fprintf(stderr, "==== %s ====\n", *func->GetFullName()); disasmAll();
}
