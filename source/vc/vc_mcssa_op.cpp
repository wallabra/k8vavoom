//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
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
// SSA instruction can have up to 3 operands (except for phis and calls)

// special opcode
DECLARE_SSA(Phi, None, None, None),

// call / return (argc is implied)
DECLARE_SSA(Call, RegDest, Class, FIndex),
DECLARE_SSA(VCall, RegDest, Class, FIndex),

// builtins (can use up to three registers)
DECLARE_SSA(Builtin, Builtin, None, None),

DECLARE_SSA(Return, None, None, None),
DECLARE_SSA(ReturnL, None, RegSrc, None),
DECLARE_SSA(ReturnV, None, RegSrc, None),

// delegate creation
DECLARE_SSA(LoadVFuncPtr, RegDest, Class, FIndex), // virtual
DECLARE_SSA(LoadFuncPtr, RegDest, Class, FIndex), // non-virtual
DECLARE_SSA(DelegateCall, RegDest, RegSrc, FieldOfs), // regsrc is `self`
DECLARE_SSA(DelegateCallPtr, RegDest, RegSrc, RegSrc), // 2nd regsrc is pointer

// branching
DECLARE_SSA(Goto, GotoDest, None, None),
DECLARE_SSA(IfGoto, GotoDest, RegSrc, None),
DECLARE_SSA(IfNotGoto, GotoDest, RegSrc, None),
DECLARE_SSA(CaseGoto, GotoDest, RegSrc, INumSrc),

// constants
DECLARE_SSA(LoadINum, RegDest, INumSrc, None),
DECLARE_SSA(LoadFNum, RegDest, FNumSrc, None),
DECLARE_SSA(LoadName, RegDest, Name, None),
DECLARE_SSA(LoadString, RegDest, String, None),
DECLARE_SSA(LoadClassId, RegDest, Class, None),
DECLARE_SSA(LoadState, RegDest, State, None),
DECLARE_SSA(LoadNull, RegDest, None, None),

// local variables
DECLARE_SSA(LoadLocal, RegDest, LocOfs, None),
DECLARE_SSA(StoreLocal, RegDest, LocOfs, None),
DECLARE_SSA(LoadLocalPtr, RegDest, LocOfs, None),

// fields
DECLARE_SSA(LoadField, RegDest, FieldOfs, None),
DECLARE_SSA(StoreField, RegDest, FieldOfs, None),
DECLARE_SSA(LoadFieldPtr, RegDest, FieldOfs, None),

// integer opeartors
DECLARE_SSA(AddI, RegDest, RegSrc, RegSrc),
DECLARE_SSA(SubI, RegDest, RegSrc, RegSrc),
DECLARE_SSA(MulI, RegDest, RegSrc, RegSrc),
DECLARE_SSA(ModI, RegDest, RegSrc, RegSrc),
DECLARE_SSA(DivI, RegDest, RegSrc, RegSrc),
DECLARE_SSA(NegI, RegDest, RegSrc, RegSrc),

DECLARE_SSA(EqualI, RegDest, RegSrc, RegSrc),
DECLARE_SSA(NotEqualI, RegDest, RegSrc, RegSrc),
DECLARE_SSA(LessI, RegDest, RegSrc, RegSrc),
DECLARE_SSA(LessEqualI, RegDest, RegSrc, RegSrc),
DECLARE_SSA(GreatI, RegDest, RegSrc, RegSrc),
DECLARE_SSA(GreatEqualI, RegDest, RegSrc, RegSrc),

DECLARE_SSA(BitAndI, RegDest, RegSrc, RegSrc),
DECLARE_SSA(BitOrI, RegDest, RegSrc, RegSrc),
DECLARE_SSA(BitXorI, RegDest, RegSrc, RegSrc),
DECLARE_SSA(BitShlI, RegDest, RegSrc, RegSrc),
DECLARE_SSA(BitShrI, RegDest, RegSrc, RegSrc),

DECLARE_SSA(LogNotI, RegDest, RegSrc, None),
DECLARE_SSA(BitNotI, RegDest, RegSrc, None),

DECLARE_SSA(LogAndI, RegDest, RegSrc, RegSrc),
DECLARE_SSA(LogOrI, RegDest, RegSrc, RegSrc),

// float opeartors
DECLARE_SSA(AddF, RegDest, RegSrc, RegSrc),
DECLARE_SSA(SubF, RegDest, RegSrc, RegSrc),
DECLARE_SSA(MulF, RegDest, RegSrc, RegSrc),
DECLARE_SSA(ModF, RegDest, RegSrc, RegSrc),
DECLARE_SSA(DivF, RegDest, RegSrc, RegSrc),
DECLARE_SSA(NegF, RegDest, RegSrc, RegSrc),

DECLARE_SSA(EqualF, RegDest, RegSrc, RegSrc),
DECLARE_SSA(NotEqualF, RegDest, RegSrc, RegSrc),
DECLARE_SSA(LessF, RegDest, RegSrc, RegSrc),
DECLARE_SSA(LessEqualF, RegDest, RegSrc, RegSrc),
DECLARE_SSA(GreatF, RegDest, RegSrc, RegSrc),
DECLARE_SSA(GreatEqualF, RegDest, RegSrc, RegSrc),

// string opeartors
DECLARE_SSA(AddS, RegDest, RegSrc, RegSrc),

DECLARE_SSA(EqualS, RegDest, RegSrc, RegSrc),
DECLARE_SSA(NotEqualS, RegDest, RegSrc, RegSrc),
DECLARE_SSA(LessS, RegDest, RegSrc, RegSrc),
DECLARE_SSA(LessEqualS, RegDest, RegSrc, RegSrc),
DECLARE_SSA(GreatS, RegDest, RegSrc, RegSrc),
DECLARE_SSA(GreatEqualS, RegDest, RegSrc, RegSrc),

// vector operators
/*
DECLARE_SSA(VAdd, RegDest, RegSrc, RegSrc),
DECLARE_SSA(VSub, RegDest, RegSrc, RegSrc),
DECLARE_SSA(VPreScale, RegDest, RegSrc, RegSrc),
DECLARE_SSA(VPostScale, RegDest, RegSrc, RegSrc),
DECLARE_SSA(VIScale, RegDest, RegSrc, RegSrc), // inverse scale
DECLARE_SSA(VEquals, RegDest, RegSrc, RegSrc),
DECLARE_OPC(VNotEquals, RegDest, RegSrc, RegSrc),
DECLARE_OPC(VUnaryMinus, RegDest, RegSrc, RegSrc),
DECLARE_OPC(VFixParam, RegDest, RegSrc, RegSrc),
*/

DECLARE_SSA(FloatToBool, RegDest, RegSrc, None),
DECLARE_SSA(VectorToBool, RegDest, RegSrc, None),

DECLARE_SSA(StrGetChar, RegDest, RegSrc, RegSrc), // op1[op2]
DECLARE_SSA(StrSetChar, RegSrc, RegSrc, RegSrc), // op1[op2] = op0

// string slicing and slice slicing
//DECLARE_OPC(StrSlice, None),
//DECLARE_OPC(StrSliceAssign, None),
//DECLARE_OPC(SliceSlice, TypeSize),

// pointer opeartors
DECLARE_SSA(PtrEquals, RegDest, RegSrc, RegSrc),
DECLARE_SSA(PtrNotEquals, RegDest, RegSrc, RegSrc),
DECLARE_SSA(PtrToBool, RegDest, RegSrc, None),

// conversions
DECLARE_SSA(IntToFloat, RegDest, RegSrc, None),
DECLARE_SSA(FloatToInt, RegDest, RegSrc, None),
DECLARE_SSA(StrToName, RegDest, RegSrc, None),
DECLARE_SSA(NameToStr, RegDest, RegSrc, None),

// cleanup of local variables.
DECLARE_SSA(ClearPointedStr, None, RegSrc, None),
DECLARE_SSA(ClearPointedStruct, Struct, None, None),
DECLARE_SSA(ZeroPointedStruct, Struct, None, None),

// drop result
//DECLARE_OPC(Drop, None),
//DECLARE_OPC(VDrop, None),
//DECLARE_OPC(DropStr, None),

// dynamic arrays
DECLARE_SSA(DynArrayElement, RegDest, TypeSize, None),
DECLARE_SSA(DynArrayElementGrow, RegDest, Type, None),
DECLARE_SSA(DynArrayGetNum, RegDest, RegSrc, None),
DECLARE_SSA(DynArraySetNum, RegDest, RegSrc, None),
DECLARE_SSA(DynArraySetNumMinus, RegDest, Type, RegSrc),
DECLARE_SSA(DynArraySetNumPlus, RegDest, Type, RegSrc),
DECLARE_SSA(DynArrayInsert, RegDest, Type, RegSrc),
DECLARE_SSA(DynArrayRemove, RegDest, Type, RegSrc),

// dynamic cast
DECLARE_SSA(DynamicCast, RegDest, Class, None),
DECLARE_SSA(DynamicClassCast, RegDest, Class, None),

// access to the default object
DECLARE_SSA(GetDefaultObj, RegDest, RegSrc, None),
DECLARE_SSA(GetClassDefaultObj, RegDest, RegSrc, None),

// iterators
DECLARE_SSA(IteratorInit, None, None, None),
DECLARE_SSA(IteratorNext, None, None, None),
DECLARE_SSA(IteratorPop, None, None, None),

// scripted iterators
DECLARE_SSA(IteratorDtorAt, GotoDest, None, None),
DECLARE_SSA(IteratorFinish, None, None, None),

// `write` and `writeln`
DECLARE_SSA(DoWriteOne, None, Type, RegSrc),
DECLARE_SSA(DoWriteFlush, None, None, None),

// for printf-like varargs
DECLARE_SSA(LoadTypePtr, RegDest, Type, None),

// fill [-1] pointer with zeroes; 2nd int is length
DECLARE_SSA(ZeroByPtr, None, RegSrc, RegSrc),

// get class pointer from pushed object
DECLARE_SSA(GetObjClassPtr, RegDest, RegSrc, None),
// [-2]: classptr; [-1]: classptr
DECLARE_SSA(ClassIsAClass, RegDest, RegSrc, RegSrc),
DECLARE_SSA(ClassIsNotAClass, RegDest, RegSrc, RegSrc),
