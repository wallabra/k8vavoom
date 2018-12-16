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


#ifdef BUILTIN_OPCODE_INFO
# ifndef DECLARE_OPC_BUILTIN
#  define BUILTIN_OPCODE_INFO_DEFAULT
#  define DECLARE_OPC_BUILTIN(name)  OPC_Builtin_##name
enum {
# endif
  DECLARE_OPC_BUILTIN(IntAbs),
  DECLARE_OPC_BUILTIN(FloatAbs),
  DECLARE_OPC_BUILTIN(IntSign),
  DECLARE_OPC_BUILTIN(FloatSign),
  DECLARE_OPC_BUILTIN(IntMin),
  DECLARE_OPC_BUILTIN(IntMax),
  DECLARE_OPC_BUILTIN(FloatMin),
  DECLARE_OPC_BUILTIN(FloatMax),
  DECLARE_OPC_BUILTIN(IntClamp),
  DECLARE_OPC_BUILTIN(FloatClamp),
  DECLARE_OPC_BUILTIN(FloatIsNaN),
  DECLARE_OPC_BUILTIN(FloatIsInf),
  DECLARE_OPC_BUILTIN(FloatIsFinite),
  DECLARE_OPC_BUILTIN(DegToRad),
  DECLARE_OPC_BUILTIN(RadToDeg),
  DECLARE_OPC_BUILTIN(Sin),
  DECLARE_OPC_BUILTIN(Cos),
  DECLARE_OPC_BUILTIN(Tan),
  DECLARE_OPC_BUILTIN(ASin),
  DECLARE_OPC_BUILTIN(ACos),
  DECLARE_OPC_BUILTIN(ATan), // slope
  DECLARE_OPC_BUILTIN(Sqrt),
  DECLARE_OPC_BUILTIN(ATan2), // y, x
  DECLARE_OPC_BUILTIN(VecLength),
  DECLARE_OPC_BUILTIN(VecLength2D),
  DECLARE_OPC_BUILTIN(VecNormalize),
  DECLARE_OPC_BUILTIN(VecNormalize2D),
  DECLARE_OPC_BUILTIN(VecDot),
  DECLARE_OPC_BUILTIN(VecDot2D),
  DECLARE_OPC_BUILTIN(VecCross),
  DECLARE_OPC_BUILTIN(VecCross2D),
  DECLARE_OPC_BUILTIN(RoundF2I),
  DECLARE_OPC_BUILTIN(RoundF2F),
  DECLARE_OPC_BUILTIN(TruncF2I),
  DECLARE_OPC_BUILTIN(TruncF2F),
  DECLARE_OPC_BUILTIN(FloatCeil),
  DECLARE_OPC_BUILTIN(FloatFloor),
  DECLARE_OPC_BUILTIN(FloatLerp),
  DECLARE_OPC_BUILTIN(IntLerp),
  DECLARE_OPC_BUILTIN(FloatSmoothStep),
  DECLARE_OPC_BUILTIN(FloatSmoothStepPerlin),
  DECLARE_OPC_BUILTIN(NameToInt),
# undef DECLARE_OPC_BUILTIN
# undef BUILTIN_OPCODE_INFO
# ifdef BUILTIN_OPCODE_INFO_DEFAULT
#  undef BUILTIN_OPCODE_INFO_DEFAULT
};
# endif

#elif defined(DICTDISPATCH_OPCODE_INFO)
# ifndef DECLARE_OPC_DICTDISPATCH
#  define DICTDISPATCH_OPCODE_INFO_DEFAULT
#  define DECLARE_OPC_DICTDISPATCH(name)  OPC_DictDispatch_##name
enum {
# endif
  DECLARE_OPC_DICTDISPATCH(Clear),
  DECLARE_OPC_DICTDISPATCH(Reset),
  DECLARE_OPC_DICTDISPATCH(Length),
  DECLARE_OPC_DICTDISPATCH(Capacity),
  DECLARE_OPC_DICTDISPATCH(Find),
  DECLARE_OPC_DICTDISPATCH(Put),
  DECLARE_OPC_DICTDISPATCH(Delete),
# undef DECLARE_OPC_DICTDISPATCH
# undef DICTDISPATCH_OPCODE_INFO
# ifdef DICTDISPATCH_OPCODE_INFO_DEFAULT
#  undef DICTDISPATCH_OPCODE_INFO_DEFAULT
};
# endif

#else

#ifndef OPCODE_INFO

#define PROG_MAGIC    "VPRG"
#define PROG_VERSION  (52)

enum {
  OPCARGS_None,
  OPCARGS_Member,
  OPCARGS_BranchTargetB,
  OPCARGS_BranchTargetNB,
  OPCARGS_BranchTargetS,
  OPCARGS_BranchTarget,
  OPCARGS_ByteBranchTarget,
  OPCARGS_ShortBranchTarget,
  OPCARGS_IntBranchTarget,
  OPCARGS_Byte,
  OPCARGS_Short,
  OPCARGS_Int,
  OPCARGS_Name,
  OPCARGS_NameS,
  OPCARGS_String,
  OPCARGS_FieldOffset,
  OPCARGS_FieldOffsetS,
  OPCARGS_VTableIndex,
  OPCARGS_VTableIndexB,
  OPCARGS_TypeSize,
  OPCARGS_TypeSizeB,
  OPCARGS_VTableIndex_Byte,
  OPCARGS_VTableIndexB_Byte,
  OPCARGS_FieldOffset_Byte,
  OPCARGS_FieldOffsetS_Byte,
  OPCARGS_Type,
  OPCARGS_A2DDimsAndSize,
  OPCARGS_Builtin,
  // used for call, int is argc
  OPCARGS_Member_Int,
  // used for delegate call, int is argc, type is delegate type
  OPCARGS_Type_Int,
  // used for dynarray sorting, int is delegate argc
  OPCARGS_ArrElemType_Int,
  OPCARGS_TypeDD, // dictdispatch
};


enum {
#define DECLARE_OPC(name, args)   OPC_##name
#endif

  DECLARE_OPC(Done, None),

  // call / return
  DECLARE_OPC(Call, Member_Int),
  DECLARE_OPC(PushVFunc, VTableIndex),
  DECLARE_OPC(PushFunc, Member),
  DECLARE_OPC(VCall, VTableIndex_Byte),
  DECLARE_OPC(VCallB, VTableIndexB_Byte),
  DECLARE_OPC(DelegateCall, FieldOffset_Byte),
  DECLARE_OPC(DelegateCallS, FieldOffsetS_Byte),
  DECLARE_OPC(DelegateCallPtr, Type_Int),
  DECLARE_OPC(Return, None),
  DECLARE_OPC(ReturnL, None),
  DECLARE_OPC(ReturnV, None),

  // branching
  DECLARE_OPC(GotoB, BranchTargetB),
  DECLARE_OPC(GotoNB, BranchTargetNB),
  DECLARE_OPC(GotoS, BranchTargetS),
  DECLARE_OPC(Goto, BranchTarget),
  DECLARE_OPC(IfGotoB, BranchTargetB),
  DECLARE_OPC(IfGotoNB, BranchTargetNB),
  DECLARE_OPC(IfGotoS, BranchTargetS),
  DECLARE_OPC(IfGoto, BranchTarget),
  DECLARE_OPC(IfNotGotoB, BranchTargetB),
  DECLARE_OPC(IfNotGotoNB, BranchTargetNB),
  DECLARE_OPC(IfNotGotoS, BranchTargetS),
  DECLARE_OPC(IfNotGoto, BranchTarget),
  DECLARE_OPC(CaseGotoB, ByteBranchTarget),
  DECLARE_OPC(CaseGotoS, ShortBranchTarget),
  DECLARE_OPC(CaseGoto, IntBranchTarget),

  // push constants
  DECLARE_OPC(PushNumber0, None),
  DECLARE_OPC(PushNumber1, None),
  DECLARE_OPC(PushNumberB, Byte),
  //DECLARE_OPC(PushNumberS, Short),
  DECLARE_OPC(PushNumber, Int),
  DECLARE_OPC(PushName, Name),
  DECLARE_OPC(PushNameS, NameS),
  DECLARE_OPC(PushString, String),
  DECLARE_OPC(PushClassId, Member),
  DECLARE_OPC(PushState, Member),
  DECLARE_OPC(PushNull, None),

  // loading of variables
  DECLARE_OPC(LocalAddress0, None),
  DECLARE_OPC(LocalAddress1, None),
  DECLARE_OPC(LocalAddress2, None),
  DECLARE_OPC(LocalAddress3, None),
  DECLARE_OPC(LocalAddress4, None),
  DECLARE_OPC(LocalAddress5, None),
  DECLARE_OPC(LocalAddress6, None),
  DECLARE_OPC(LocalAddress7, None),
  DECLARE_OPC(LocalAddressB, Byte),
  DECLARE_OPC(LocalAddressS, Short),
  DECLARE_OPC(LocalAddress, Int),
  DECLARE_OPC(LocalValue0, None),
  DECLARE_OPC(LocalValue1, None),
  DECLARE_OPC(LocalValue2, None),
  DECLARE_OPC(LocalValue3, None),
  DECLARE_OPC(LocalValue4, None),
  DECLARE_OPC(LocalValue5, None),
  DECLARE_OPC(LocalValue6, None),
  DECLARE_OPC(LocalValue7, None),
  DECLARE_OPC(LocalValueB, Byte),
  DECLARE_OPC(VLocalValueB, Byte),
  DECLARE_OPC(StrLocalValueB, Byte),
  DECLARE_OPC(Offset, FieldOffset),
  DECLARE_OPC(OffsetS, FieldOffsetS),
  DECLARE_OPC(FieldValue, FieldOffset),
  DECLARE_OPC(FieldValueS, FieldOffsetS),
  DECLARE_OPC(VFieldValue, FieldOffset),
  DECLARE_OPC(VFieldValueS, FieldOffsetS),
  DECLARE_OPC(PtrFieldValue, FieldOffset),
  DECLARE_OPC(PtrFieldValueS, FieldOffsetS),
  DECLARE_OPC(StrFieldValue, FieldOffset),
  DECLARE_OPC(StrFieldValueS, FieldOffsetS),
  DECLARE_OPC(SliceFieldValue, FieldOffset),
  DECLARE_OPC(ByteFieldValue, FieldOffset),
  DECLARE_OPC(ByteFieldValueS, FieldOffsetS),
  DECLARE_OPC(Bool0FieldValue, FieldOffset_Byte),
  DECLARE_OPC(Bool0FieldValueS, FieldOffsetS_Byte),
  DECLARE_OPC(Bool1FieldValue, FieldOffset_Byte),
  DECLARE_OPC(Bool1FieldValueS, FieldOffsetS_Byte),
  DECLARE_OPC(Bool2FieldValue, FieldOffset_Byte),
  DECLARE_OPC(Bool2FieldValueS, FieldOffsetS_Byte),
  DECLARE_OPC(Bool3FieldValue, FieldOffset_Byte),
  DECLARE_OPC(Bool3FieldValueS, FieldOffsetS_Byte),
  DECLARE_OPC(CheckArrayBounds, Int), /* won't pop index */
  DECLARE_OPC(CheckArrayBounds2D, A2DDimsAndSize), /* won't pop indicies */
  DECLARE_OPC(ArrayElement, TypeSize),
  DECLARE_OPC(ArrayElementB, TypeSizeB),
  DECLARE_OPC(ArrayElement2D, A2DDimsAndSize),
  DECLARE_OPC(SliceElement, TypeSize),
  /*DECLARE_OPC(OffsetPtr, Int),*/
  DECLARE_OPC(PushPointed, None),
  DECLARE_OPC(PushPointedSlice, None),
  DECLARE_OPC(PushPointedSliceLen, None),
  DECLARE_OPC(VPushPointed, None),
  DECLARE_OPC(PushPointedPtr, None),
  DECLARE_OPC(PushPointedByte, None),
  DECLARE_OPC(PushBool0, Byte),
  DECLARE_OPC(PushBool1, Byte),
  DECLARE_OPC(PushBool2, Byte),
  DECLARE_OPC(PushBool3, Byte),
  DECLARE_OPC(PushPointedStr, None),
  DECLARE_OPC(PushPointedDelegate, None),

  // integer opeartors
  DECLARE_OPC(Add, None),
  DECLARE_OPC(Subtract, None),
  DECLARE_OPC(Multiply, None),
  DECLARE_OPC(Divide, None),
  DECLARE_OPC(Modulus, None),
  DECLARE_OPC(Equals, None),
  DECLARE_OPC(NotEquals, None),
  DECLARE_OPC(Less, None),
  DECLARE_OPC(Greater, None),
  DECLARE_OPC(LessEquals, None),
  DECLARE_OPC(GreaterEquals, None),
  DECLARE_OPC(NegateLogical, None),
  DECLARE_OPC(AndBitwise, None),
  DECLARE_OPC(OrBitwise, None),
  DECLARE_OPC(XOrBitwise, None),
  DECLARE_OPC(LShift, None),
  DECLARE_OPC(RShift, None),
  DECLARE_OPC(URShift, None),
  DECLARE_OPC(UnaryMinus, None),
  DECLARE_OPC(BitInverse, None),

  // increment / decrement
  DECLARE_OPC(PreInc, None),
  DECLARE_OPC(PreDec, None),
  DECLARE_OPC(PostInc, None),
  DECLARE_OPC(PostDec, None),
  DECLARE_OPC(IncDrop, None),
  DECLARE_OPC(DecDrop, None),

  // integer assignment operators
  DECLARE_OPC(AssignDrop, None),
  DECLARE_OPC(AddVarDrop, None),
  DECLARE_OPC(SubVarDrop, None),
  DECLARE_OPC(MulVarDrop, None),
  DECLARE_OPC(DivVarDrop, None),
  DECLARE_OPC(ModVarDrop, None),
  DECLARE_OPC(AndVarDrop, None),
  DECLARE_OPC(OrVarDrop, None),
  DECLARE_OPC(XOrVarDrop, None),
  DECLARE_OPC(LShiftVarDrop, None),
  DECLARE_OPC(RShiftVarDrop, None),
  DECLARE_OPC(URShiftVarDrop, None),

  // increment / decrement byte
  DECLARE_OPC(BytePreInc, None),
  DECLARE_OPC(BytePreDec, None),
  DECLARE_OPC(BytePostInc, None),
  DECLARE_OPC(BytePostDec, None),
  DECLARE_OPC(ByteIncDrop, None),
  DECLARE_OPC(ByteDecDrop, None),

  // byte assignment operators
  DECLARE_OPC(ByteAssignDrop, None),
  DECLARE_OPC(ByteAddVarDrop, None),
  DECLARE_OPC(ByteSubVarDrop, None),
  DECLARE_OPC(ByteMulVarDrop, None),
  DECLARE_OPC(ByteDivVarDrop, None),
  DECLARE_OPC(ByteModVarDrop, None),
  DECLARE_OPC(ByteAndVarDrop, None),
  DECLARE_OPC(ByteOrVarDrop, None),
  DECLARE_OPC(ByteXOrVarDrop, None),
  DECLARE_OPC(ByteLShiftVarDrop, None),
  DECLARE_OPC(ByteRShiftVarDrop, None),

  DECLARE_OPC(CatAssignVarDrop, None),

  // floating point operators
  DECLARE_OPC(FAdd, None),
  DECLARE_OPC(FSubtract, None),
  DECLARE_OPC(FMultiply, None),
  DECLARE_OPC(FDivide, None),
  DECLARE_OPC(FEquals, None),
  DECLARE_OPC(FNotEquals, None),
  DECLARE_OPC(FLess, None),
  DECLARE_OPC(FGreater, None),
  DECLARE_OPC(FLessEquals, None),
  DECLARE_OPC(FGreaterEquals, None),
  DECLARE_OPC(FUnaryMinus, None),

  // floating point assignment operators
  DECLARE_OPC(FAddVarDrop, None),
  DECLARE_OPC(FSubVarDrop, None),
  DECLARE_OPC(FMulVarDrop, None),
  DECLARE_OPC(FDivVarDrop, None),

  // vector operators
  DECLARE_OPC(VAdd, None),
  DECLARE_OPC(VSubtract, None),
  DECLARE_OPC(VPreScale, None),
  DECLARE_OPC(VPostScale, None),
  DECLARE_OPC(VIScale, None),
  DECLARE_OPC(VEquals, None),
  DECLARE_OPC(VNotEquals, None),
  DECLARE_OPC(VUnaryMinus, None),
  DECLARE_OPC(VFixParam, Byte),

  // vector assignment operators
  DECLARE_OPC(VAssignDrop, None),
  DECLARE_OPC(VAddVarDrop, None),
  DECLARE_OPC(VSubVarDrop, None),
  DECLARE_OPC(VScaleVarDrop, None),
  DECLARE_OPC(VIScaleVarDrop, None),

  DECLARE_OPC(FloatToBool, None),
  DECLARE_OPC(VectorToBool, None),

  // string operators
  DECLARE_OPC(StrToBool, None),
  DECLARE_OPC(StrEquals, None),
  DECLARE_OPC(StrNotEquals, None),
  DECLARE_OPC(StrLess, None),
  DECLARE_OPC(StrLessEqu, None),
  DECLARE_OPC(StrGreat, None),
  DECLARE_OPC(StrGreatEqu, None),
  DECLARE_OPC(StrLength, None),
  DECLARE_OPC(StrCat, None),

  DECLARE_OPC(StrGetChar, None),
  DECLARE_OPC(StrSetChar, None),

  // string slicing and slice slicing
  DECLARE_OPC(StrSlice, None),
  DECLARE_OPC(StrSliceAssign, None),
  DECLARE_OPC(SliceSlice, TypeSize),

  // string assignment operators
  DECLARE_OPC(AssignStrDrop, None),

  // pointer opeartors
  DECLARE_OPC(PtrEquals, None),
  DECLARE_OPC(PtrNotEquals, None),
  DECLARE_OPC(PtrToBool, None),
  DECLARE_OPC(PtrSubtract, TypeSize),

  // conversions
  DECLARE_OPC(IntToFloat, None),
  DECLARE_OPC(FloatToInt, None),
  DECLARE_OPC(StrToName, None),
  DECLARE_OPC(NameToStr, None),

  // cleanup of local variables
  DECLARE_OPC(ClearPointedStr, None),
  DECLARE_OPC(ClearPointedStruct, Member),
  DECLARE_OPC(ZeroPointedStruct, Member),

  // drop result
  DECLARE_OPC(Drop, None),
  DECLARE_OPC(VDrop, None),
  DECLARE_OPC(DropStr, None),

  // special assignment operators
  DECLARE_OPC(AssignPtrDrop, None),
  DECLARE_OPC(AssignBool0, Byte),
  DECLARE_OPC(AssignBool1, Byte),
  DECLARE_OPC(AssignBool2, Byte),
  DECLARE_OPC(AssignBool3, Byte),
  DECLARE_OPC(AssignDelegate, None),

  // dynamic arrays
  DECLARE_OPC(DynArrayElement, TypeSize),
  //DECLARE_OPC(DynArrayElementS, TypeSizeS),
  DECLARE_OPC(DynArrayElementB, TypeSizeB),
  DECLARE_OPC(DynArrayElementGrow, Type),
  DECLARE_OPC(DynArrayGetNum, None),
  DECLARE_OPC(DynArrayGetNum1, None),
  DECLARE_OPC(DynArrayGetNum2, None), // second dimension
  DECLARE_OPC(DynArraySetNum, Type),
  DECLARE_OPC(DynArraySetNumMinus, Type),
  DECLARE_OPC(DynArraySetNumPlus, Type),
  DECLARE_OPC(DynArrayInsert, Type),
  DECLARE_OPC(DynArrayRemove, Type),
  DECLARE_OPC(DynArrayClear, Type),
  DECLARE_OPC(DynArraySort, /*ArrElemType_Int*/Type),
  DECLARE_OPC(DynArraySwap1D, Type),
  // 2d (and some 1d)
  DECLARE_OPC(DynArraySetSize1D, Type),
  DECLARE_OPC(DynArraySetSize2D, Type),
  DECLARE_OPC(DynArrayElement2D, TypeSize),

  // dynamic cast
  DECLARE_OPC(DynamicCast, Member),
  DECLARE_OPC(DynamicClassCast, Member),
  DECLARE_OPC(DynamicCastIndirect, None),
  DECLARE_OPC(DynamicClassCastIndirect, None),

  // access to the default object
  DECLARE_OPC(GetDefaultObj, None),
  DECLARE_OPC(GetClassDefaultObj, None),

  // iterators
  DECLARE_OPC(IteratorInit, None),
  DECLARE_OPC(IteratorNext, None),
  DECLARE_OPC(IteratorPop, None),

  // `write` and `writeln`
  DECLARE_OPC(DoWriteOne, Type),
  DECLARE_OPC(DoWriteFlush, None),

  // for printf-like varargs
  DECLARE_OPC(DoPushTypePtr, Type),

  // fill [-1] pointer with zeroes; int is length
  DECLARE_OPC(ZeroByPtr, Int),

  // get class pointer from pushed object
  DECLARE_OPC(GetObjClassPtr, None),
  // [-2]: classptr; [-1]: classptr
  DECLARE_OPC(ClassIsAClass, None),
  DECLARE_OPC(ClassIsNotAClass, None),

  // builtins (k8: i'm short of opcodes, so...)
  DECLARE_OPC(Builtin, Builtin),

  // universal dispatcher for dictionary type
  DECLARE_OPC(DictDispatch, TypeDD), // type followed by OPC_DictDispatch_XXX

#undef DECLARE_OPC
#ifndef OPCODE_INFO
  NUM_OPCODES
};

static_assert(NUM_OPCODES < 256, "VaVoomC: too many opcodes!");


#endif

#endif
