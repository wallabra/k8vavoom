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

#include "vc_local.h"


// ////////////////////////////////////////////////////////////////////////// //
FBuiltinInfo *FBuiltinInfo::Builtins;


//==========================================================================
//
//  PF_Fixme
//
//==========================================================================
#if !defined(IN_VCC)
static void PF_Fixme () {
  VObject::VMDumpCallStack();
  Sys_Error("unimplemented bulitin");
}
#endif


//==========================================================================
//
//  VMethodParam::VMethodParam
//
//==========================================================================
VMethodParam::VMethodParam () : TypeExpr(nullptr), Name(NAME_None) {
}


/*
//==========================================================================
//
//  VMethodParam::VMethodParam
//
//==========================================================================
VMethodParam::VMethodParam (const VMethodParam &v)
  : TypeExpr(v.TypeExpr ? v.TypeExpr->SyntaxCopy() : nullptr)
  , Name(v.Name)
  , Loc(v.Loc)
{
}


//==========================================================================
//
//  VMethodParam::VMethodParam
//
//==========================================================================
VMethodParam &VMethodParam::operator = (const VMethodParam &v) {
  if (&v != this) {
    TypeExpr = (v.TypeExpr ? v.TypeExpr->SyntaxCopy() : nullptr);
    Name = v.Name;
    Loc = v.Loc;
  }
}
*/


//==========================================================================
//
//  VMethodParam::~VMethodParam
//
//==========================================================================
VMethodParam::~VMethodParam () {
  if (TypeExpr) { delete TypeExpr; TypeExpr = nullptr; }
}


//==========================================================================
//
//  VMethod::VMethod
//
//==========================================================================
VMethod::VMethod (VName AName, VMemberBase *AOuter, TLocation ALoc)
  : VMemberBase(MEMBER_Method, AName, AOuter, ALoc)
  , mPostLoaded(false)
  , NumLocals(0)
  , Flags(0)
  , ReturnType(TYPE_Void)
  , NumParams(0)
  , ParamsSize(0)
  , SuperMethod(nullptr)
  , ReplCond(nullptr)
  , ReturnTypeExpr(nullptr)
  , Statement(nullptr)
  , SelfTypeName(NAME_None)
  , lmbCount(0)
  , Profile1(0)
  , Profile2(0)
  , NativeFunc(0)
  , VTableIndex(0)
  , NetIndex(0)
  , NextNetMethod(0)
{
  memset(ParamFlags, 0, sizeof(ParamFlags));
}


//==========================================================================
//
//  VMethod::~VMethod
//
//==========================================================================
VMethod::~VMethod() {
  //guard(VMethod::~VMethod);
  if (ReturnTypeExpr) { delete ReturnTypeExpr; ReturnTypeExpr = nullptr; }
  if (Statement) { delete Statement; Statement = nullptr; }
  //unguard;
}


//==========================================================================
//
//  VMethod::Serialise
//
//==========================================================================
void VMethod::Serialise (VStream &Strm) {
  guard(VMethod::Serialise);
  VMemberBase::Serialise(Strm);

  Strm << SuperMethod
    << STRM_INDEX(NumLocals)
    << STRM_INDEX(Flags)
    << ReturnType
    << STRM_INDEX(NumParams)
    << STRM_INDEX(ParamsSize);
  for (int i = 0; i < NumParams; ++i) Strm << ParamTypes[i] << ParamFlags[i];
  Strm << ReplCond << Instructions;
  unguard;
}


//==========================================================================
//
//  VMethod::Define
//
//==========================================================================
bool VMethod::Define () {
  guard(VMethod::Define);
  bool Ret = true;

  if (Flags&FUNC_Static) {
    if ((Flags&FUNC_Final) == 0) {
      ParseError(Loc, "Currently static methods must be final");
      Ret = false;
    }
  }

  if ((Flags&FUNC_VarArgs) != 0 && (Flags&FUNC_Native) == 0) {
    ParseError(Loc, "Only native methods can have varargs");
    Ret = false;
  }

  if ((Flags&(FUNC_Iterator|FUNC_Native)) == FUNC_Iterator) {
    ParseError(Loc, "Iterators can only be native");
    Ret = false;
  }

  VEmitContext ec(this);

  if (ReturnTypeExpr) ReturnTypeExpr = ReturnTypeExpr->ResolveAsType(ec);
  if (ReturnTypeExpr) {
    VFieldType t = ReturnTypeExpr->Type;
    if (t.Type != TYPE_Void) {
      // function's return type must be void, vector or with size 4
      t.CheckPassable(ReturnTypeExpr->Loc);
    }
    ReturnType = t;
  } else {
    Ret = false;
  }

  // resolve parameters types
  ParamsSize = (Flags&FUNC_Static ? 0 : 1); // first is `self`
  for (int i = 0; i < NumParams; ++i) {
    VMethodParam &P = Params[i];

    if (P.TypeExpr) P.TypeExpr = P.TypeExpr->ResolveAsType(ec);
    if (!P.TypeExpr) { Ret = false; continue; }
    VFieldType type = P.TypeExpr->Type;

    if (type.Type == TYPE_Void) {
      ParseError(P.TypeExpr->Loc, "Bad variable type");
      Ret = false;
      continue;
    }

    ParamTypes[i] = type;
    if ((ParamFlags[i]&FPARM_Optional) != 0 && (ParamFlags[i]&FPARM_Out) != 0) ParseError(P.Loc, "Modifiers `optional` and `out` are mutually exclusive");
    if ((ParamFlags[i]&FPARM_Optional) != 0 && (ParamFlags[i]&FPARM_Ref) != 0) ParseError(P.Loc, "Modifiers `optional` and `ref` are mutually exclusive");
    if ((ParamFlags[i]&FPARM_Out) != 0 && (ParamFlags[i]&FPARM_Ref) != 0) ParseError(P.Loc, "Modifiers `out` and `ref` are mutually exclusive");

    if (ParamFlags[i]&(FPARM_Out|FPARM_Ref)) {
      ++ParamsSize;
    } else {
      type.CheckPassable(P.TypeExpr->Loc);
      ParamsSize += type.GetStackSize()/4;
    }
    if (ParamFlags[i]&FPARM_Optional) ++ParamsSize;
  }

  // if this is a overriden method, verify that return type and argument types match
  SuperMethod = nullptr;
  if (Outer->MemberType == MEMBER_Class && Name != NAME_None && ((VClass *)Outer)->ParentClass) {
    SuperMethod = ((VClass *)Outer)->ParentClass->FindMethod(Name);
  }

  if (SuperMethod) {
    if ((Flags&FUNC_Override) == 0) {
      ParseError(Loc, "Overriding virtual method without `override` keyword");
      Ret = false;
    }
    if (Ret && (SuperMethod->Flags&FUNC_Private) != 0) {
      ParseError(Loc, "Overriding private method is not allowed");
      Ret = false;
    }
    if (Ret && (Flags&FUNC_Private) != 0) {
      ParseError(Loc, "Overriding with private method is not allowed");
      Ret = false;
    }
    if (Ret && (SuperMethod->Flags&FUNC_Protected) != (Flags&FUNC_Protected)) {
      if ((SuperMethod->Flags&FUNC_Protected)) {
        ParseError(Loc, "Cannot override protected method with public");
        Ret = false;
      } else {
        //FIXME: not yet implemented
        ParseError(Loc, "Cannot override public method with protected");
        Ret = false;
      }
    }
    if (Ret && (SuperMethod->Flags&FUNC_Final)) {
      ParseError(Loc, "Method already has been declared as final and cannot be overriden");
      Ret = false;
    }
    if (!SuperMethod->ReturnType.Equals(ReturnType)) {
      if (Ret) ParseError(Loc, "Method redefined with different return type");
      Ret = false;
    } else if (SuperMethod->NumParams != NumParams) {
      if (Ret) ParseError(Loc, "Method redefined with different number of arguments");
      Ret = false;
    } else {
      for (int i = 0; i < NumParams; ++i) {
        if (!SuperMethod->ParamTypes[i].Equals(ParamTypes[i])) {
          if (Ret) {
            ParseError(Loc, "Type of argument #%d differs from base class (expected `%s`, got `%s`)", i+1,
              *SuperMethod->ParamTypes[i].GetName(), *ParamTypes[i].GetName());
          }
          Ret = false;
        }
        if ((SuperMethod->ParamFlags[i]^ParamFlags[i])&(FPARM_Optional|FPARM_Out|FPARM_Ref)) {
          if (Ret) ParseError(Loc, "Modifiers of argument #%d differs from base class", i+1);
          Ret = false;
        }
      }
    }

    // inherit network flags
    Flags |= SuperMethod->Flags&FUNC_NetFlags;
  } else {
    if ((Flags&FUNC_Override) != 0) {
      ParseError(Loc, "Trying to override non-existing method");
      Ret = false;
    }
  }

  if (Flags&FUNC_Spawner) {
    // verify that it's a valid spawner method
    if (NumParams < 1) {
      ParseError(Loc, "Spawner method must have at least 1 argument");
    } else if (ParamTypes[0].Type != TYPE_Class) {
      ParseError(Loc, "Spawner method must have class as first argument");
    } else if (ReturnType.Type != TYPE_Reference) {
      ParseError(Loc, "Spawner method must return an object reference");
    } else if (ReturnType.Class != ParamTypes[0].Class) {
      // hack for `SpawnObject (class)`
      if (ParamTypes[0].Class || ReturnType.Class->Name != "Object") {
        ParseError(Loc, "Spawner method must return an object of the same type as class");
      }
    }
  }

  return Ret;
  unguard;
}


//==========================================================================
//
//  VMethod::Emit
//
//==========================================================================
void VMethod::Emit () {
  guard(VMethod::Emit);
  if (Flags&FUNC_Native) {
    if (Statement) ParseError(Loc, "Native methods can't have a body");
    return;
  }

  if (Outer->MemberType == MEMBER_Field) return; // delegate

  if (!Statement) { ParseError(Loc, "Method body missing"); return; }

  if (ReturnTypeExpr && ReturnTypeExpr->Type.Type != TYPE_Void) {
    if (!Statement->IsEndsWithReturn()) {
      ParseError(Loc, "Missing `return` in one of the pathes of function `%s`", *GetFullName());
      return;
    }
  }

  VEmitContext ec(this);

  ec.ClearLocalDefs();
  ec.localsofs = (Flags&FUNC_Static ? 0 : 1); // first is `self`
  if (Outer->MemberType == MEMBER_Class && this == ((VClass*)Outer)->DefaultProperties) {
    ec.InDefaultProperties = true;
  }

  for (int i = 0; i < NumParams; ++i) {
    VMethodParam &P = Params[i];
    if (P.Name != NAME_None) {
      auto oldlofs = ec.localsofs;
      if (ec.CheckForLocalVar(P.Name) != -1) ParseError(P.Loc, "Redefined identifier %s", *P.Name);
      VLocalVarDef &L = ec.AllocLocal(P.Name, ParamTypes[i], P.Loc);
      ec.localsofs = oldlofs;
      L.Offset = ec.localsofs;
      L.Visible = true;
      L.ParamFlags = ParamFlags[i];
    }
    if (ParamFlags[i]&(FPARM_Out|FPARM_Ref)) {
      ++ec.localsofs;
    } else {
      ec.localsofs += ParamTypes[i].GetStackSize()/4;
    }
    if (ParamFlags[i]&FPARM_Optional) {
      if (P.Name != NAME_None) {
        auto oldlofs = ec.localsofs;
        VLocalVarDef &L = ec.AllocLocal(va("specified_%s", *P.Name), TYPE_Int, P.Loc);
        ec.localsofs = oldlofs;
        L.Offset = ec.localsofs;
        L.Visible = true;
        L.ParamFlags = 0;
      }
      ++ec.localsofs;
    }
  }

  for (int i = 0; i < ec.GetLocalDefCount(); ++i) {
    VLocalVarDef &loc = ec.GetLocalByIndex(i);
    if (loc.Type.Type == TYPE_Vector && (ParamFlags[i]&(FPARM_Out|FPARM_Ref)) == 0) {
      ec.AddStatement(OPC_VFixParam, loc.Offset, Loc);
    }
  }

  if (!Statement->Resolve(ec)) return;

  Statement->Emit(ec);

  if (ReturnType.Type == TYPE_Void) {
    ec.EmitClearStrings(0, ec.GetLocalDefCount(), Loc);
    ec.AddStatement(OPC_Return, Loc);
  }
  NumLocals = ec.localsofs;
  ec.EndCode();
  if (VMemberBase::doAsmDump) DumpAsm();
  unguard;
}


//==========================================================================
//
//  VMethod::DumpAsm
//
//  Disassembles a method.
//
//==========================================================================
void VMethod::DumpAsm () {
  guard(VMethod::DumpAsm);
  VMemberBase *PM = Outer;
  while (PM->MemberType != MEMBER_Package) PM = PM->Outer;
  VPackage *Package = (VPackage *)PM;

  dprintf("--------------------------------------------\n");
  dprintf("Dump ASM function %s.%s (%d instructions)\n\n", *Outer->Name, *Name, (Flags&FUNC_Native ? 0 : Instructions.Num()));
  if (Flags&FUNC_Native) {
    //  Builtin function
    dprintf("Builtin function.\n");
    return;
  }
  for (int s = 0; s < Instructions.Num(); ++s) {
    // opcode
    int st = Instructions[s].Opcode;
    dprintf("%6d: %s", s, StatementInfo[st].name);
    switch (StatementInfo[st].Args) {
      case OPCARGS_None:
        break;
      case OPCARGS_Member:
        // name of the object
        dprintf(" %s", *Instructions[s].Member->GetFullName());
        break;
      case OPCARGS_BranchTarget:
        dprintf(" %6d", Instructions[s].Arg1);
        break;
      case OPCARGS_ByteBranchTarget:
      case OPCARGS_ShortBranchTarget:
      case OPCARGS_IntBranchTarget:
        dprintf(" %6d, %6d", Instructions[s].Arg1, Instructions[s].Arg2);
        break;
      case OPCARGS_Byte:
      case OPCARGS_Short:
      case OPCARGS_Int:
        dprintf(" %6d (%x)", Instructions[s].Arg1, Instructions[s].Arg1);
        break;
      case OPCARGS_Name:
        // name
        dprintf(" \'%s\'", *Instructions[s].NameArg);
        break;
      case OPCARGS_String:
        // string
        dprintf(" \"%s\"", &Package->Strings[Instructions[s].Arg1]);
        break;
      case OPCARGS_FieldOffset:
        dprintf(" %s", *Instructions[s].Member->Name);
        break;
      case OPCARGS_VTableIndex:
        dprintf(" %s", *Instructions[s].Member->Name);
        break;
      case OPCARGS_VTableIndex_Byte:
      case OPCARGS_FieldOffset_Byte:
        dprintf(" %s %d", *Instructions[s].Member->Name, Instructions[s].Arg2);
        break;
      case OPCARGS_TypeSize:
      case OPCARGS_Type:
        dprintf(" %s", *Instructions[s].TypeArg.GetName());
        break;
    }
    dprintf("\n");
  }
  unguard;
}


//==========================================================================
//
//  VMethod::PostLoad
//
//==========================================================================
void VMethod::PostLoad () {
  guard(VMethod::PostLoad);
  //k8: it should be called only once, but let's play safe here
  if (mPostLoaded) return;

#if !defined(IN_VCC)
  // set up builtins
  if (NumParams > VMethod::MAX_PARAMS) Sys_Error("Function has more than %i params", VMethod::MAX_PARAMS);
  for (FBuiltinInfo *B = FBuiltinInfo::Builtins; B; B = B->Next) {
    if (Outer == B->OuterClass && !VStr::Cmp(*Name, B->Name)) {
      if (Flags&FUNC_Native) {
        NativeFunc = B->Func;
        break;
      } else {
        Sys_Error("PR_LoadProgs: Builtin %s redefined", B->Name);
      }
    }
  }
  if (!NativeFunc && (Flags&FUNC_Native) != 0) {
    // default builtin
    NativeFunc = PF_Fixme;
#if defined(VCC_STANDALONE_EXECUTOR)
    // don't abort with error, because it will be done, when this
    // function will be called (if it will be called)
    fprintf(stderr, "*** WARNING: Builtin `%s` not found!\n", *GetFullName());
#elif defined(CLIENT) && defined(SERVER)
    // don't abort with error, because it will be done, when this
    // function will be called (if it will be called)
    GCon->Logf(NAME_Dev, "WARNING: Builtin `%s` not found!", *GetFullName());
#endif
  }
#endif

  CompileCode();

  mPostLoaded = true;
  unguard;
}


//==========================================================================
//
//  VMethod::CompileCode
//
//==========================================================================
#define WriteUInt8(p)  Statements.Append(p)
#define WriteInt16(p)  Statements.SetNum(Statements.Num()+2); *(vint16*)&Statements[Statements.Num()-2] = (p)
#define WriteInt32(p)  Statements.SetNum(Statements.Num()+4); *(vint32*)&Statements[Statements.Num()-4] = (p)
#define WritePtr(p)    Statements.SetNum(Statements.Num()+sizeof(void*)); *(void**)&Statements[Statements.Num()-sizeof(void*)] = (p)
#define WriteType(T) \
  WriteUInt8(T.Type); \
  WriteUInt8(T.ArrayInnerType); \
  WriteUInt8(T.InnerType); \
  WriteUInt8(T.PtrLevel); \
  WriteInt32(T.ArrayDim); \
  WritePtr(T.Class);


void VMethod::CompileCode () {
  guard(VMethod::CompileCode);
  Statements.Clear();
  if (!Instructions.Num()) return;

  OptimiseInstructions();
  for (int i = 0; i < Instructions.Num()-1; ++i) {
    Instructions[i].Address = Statements.Num();
    Statements.Append(Instructions[i].Opcode);
    switch (StatementInfo[Instructions[i].Opcode].Args) {
      case OPCARGS_None: break;
      case OPCARGS_Member: WritePtr(Instructions[i].Member); break;
      case OPCARGS_BranchTargetB: WriteUInt8(0); break;
      case OPCARGS_BranchTargetNB: WriteUInt8(0); break;
      case OPCARGS_BranchTargetS: WriteInt16(0); break;
      case OPCARGS_BranchTarget: WriteInt32(0); break;
      case OPCARGS_ByteBranchTarget: WriteUInt8(Instructions[i].Arg1); WriteInt16(0); break;
      case OPCARGS_ShortBranchTarget: WriteInt16(Instructions[i].Arg1); WriteInt16(0); break;
      case OPCARGS_IntBranchTarget: WriteInt32(Instructions[i].Arg1); WriteInt16(0); break;
      case OPCARGS_Byte: WriteUInt8(Instructions[i].Arg1); break;
      case OPCARGS_Short: WriteInt16(Instructions[i].Arg1); break;
      case OPCARGS_Int: WriteInt32(Instructions[i].Arg1); break;
      case OPCARGS_Name: WriteInt32(Instructions[i].NameArg.GetIndex()); break;
      case OPCARGS_NameS: WriteInt16(Instructions[i].NameArg.GetIndex()); break;
      case OPCARGS_NameB: WriteUInt8(Instructions[i].NameArg.GetIndex()); break;
      case OPCARGS_String: WritePtr(&GetPackage()->Strings[Instructions[i].Arg1]); break;
      case OPCARGS_FieldOffset:
        // make sure struct / class field offsets have been calculated
        Instructions[i].Member->Outer->PostLoad();
        WriteInt32(((VField *)Instructions[i].Member)->Ofs);
        break;
      case OPCARGS_FieldOffsetS:
        // make sure struct / class field offsets have been calculated
        Instructions[i].Member->Outer->PostLoad();
        WriteInt16(((VField *)Instructions[i].Member)->Ofs);
        break;
      case OPCARGS_FieldOffsetB:
        // make sure struct / class field offsets have been calculated
        Instructions[i].Member->Outer->PostLoad();
        WriteUInt8(((VField *)Instructions[i].Member)->Ofs);
        break;
      case OPCARGS_VTableIndex:
        // make sure class virtual table has been calculated
        Instructions[i].Member->Outer->PostLoad();
        WriteInt16(((VMethod *)Instructions[i].Member)->VTableIndex);
        break;
      case OPCARGS_VTableIndexB:
        // make sure class virtual table has been calculated
        Instructions[i].Member->Outer->PostLoad();
        WriteUInt8(((VMethod *)Instructions[i].Member)->VTableIndex);
        break;
      case OPCARGS_VTableIndex_Byte:
        // make sure class virtual table has been calculated
        Instructions[i].Member->Outer->PostLoad();
        WriteInt16(((VMethod *)Instructions[i].Member)->VTableIndex);
        WriteUInt8(Instructions[i].Arg2);
        break;
      case OPCARGS_VTableIndexB_Byte:
        // make sure class virtual table has been calculated
        Instructions[i].Member->Outer->PostLoad();
        WriteUInt8(((VMethod *)Instructions[i].Member)->VTableIndex);
        WriteUInt8(Instructions[i].Arg2);
        break;
      case OPCARGS_FieldOffset_Byte:
        // make sure struct / class field offsets have been calculated
        Instructions[i].Member->Outer->PostLoad();
        WriteInt32(((VField *)Instructions[i].Member)->Ofs);
        WriteUInt8(Instructions[i].Arg2);
        break;
      case OPCARGS_FieldOffsetS_Byte:
        // make sure struct / class field offsets have been calculated
        Instructions[i].Member->Outer->PostLoad();
        WriteInt16(((VField *)Instructions[i].Member)->Ofs);
        WriteUInt8(Instructions[i].Arg2);
        break;
      case OPCARGS_FieldOffsetB_Byte:
        // make sure struct / class field offsets have been calculated
        Instructions[i].Member->Outer->PostLoad();
        WriteUInt8(((VField *)Instructions[i].Member)->Ofs);
        WriteUInt8(Instructions[i].Arg2);
        break;
      case OPCARGS_TypeSize: WriteInt32(Instructions[i].TypeArg.GetSize()); break;
      case OPCARGS_TypeSizeS: WriteInt16(Instructions[i].TypeArg.GetSize()); break;
      case OPCARGS_TypeSizeB: WriteUInt8(Instructions[i].TypeArg.GetSize()); break;
      case OPCARGS_Type: WriteType(Instructions[i].TypeArg); break;
    }
    while (StatLocs.length() < Statements.length()) StatLocs.Append(Instructions[i].loc);
  }
  Instructions[Instructions.Num()-1].Address = Statements.Num();

  for (int i = 0; i < Instructions.Num()-1; ++i) {
    switch (StatementInfo[Instructions[i].Opcode].Args) {
      case OPCARGS_BranchTargetB:
        Statements[Instructions[i].Address+1] = Instructions[Instructions[i].Arg1].Address-Instructions[i].Address;
        break;
      case OPCARGS_BranchTargetNB:
        Statements[Instructions[i].Address+1] = Instructions[i].Address-Instructions[Instructions[i].Arg1].Address;
        break;
      case OPCARGS_BranchTargetS:
        *(vint16 *)&Statements[Instructions[i].Address+1] = Instructions[Instructions[i].Arg1].Address-Instructions[i].Address;
        break;
      case OPCARGS_BranchTarget:
        *(vint32 *)&Statements[Instructions[i].Address+1] = Instructions[Instructions[i].Arg1].Address-Instructions[i].Address;
        break;
      case OPCARGS_ByteBranchTarget:
        *(vint16 *)&Statements[Instructions[i].Address+2] = Instructions[Instructions[i].Arg2].Address-Instructions[i].Address;
        break;
      case OPCARGS_ShortBranchTarget:
        *(vint16 *)&Statements[Instructions[i].Address+3] = Instructions[Instructions[i].Arg2].Address-Instructions[i].Address;
        break;
      case OPCARGS_IntBranchTarget:
        *(vint16 *)&Statements[Instructions[i].Address+5] = Instructions[Instructions[i].Arg2].Address-Instructions[i].Address;
        break;
    }
  }

  // we don't need instructions anymore
  Instructions.Clear();
  unguard;
}


//==========================================================================
//
//  VMethod::OptimiseInstructions
//
//==========================================================================
void VMethod::OptimiseInstructions () {
  guard(VMethod::OptimiseInstructions);
  int Addr = 0;
  for (int i = 0; i < Instructions.Num()-1; ++i) {
    switch (Instructions[i].Opcode) {
      case OPC_PushVFunc:
        // make sure class virtual table has been calculated
        Instructions[i].Member->Outer->PostLoad();
        if (((VMethod *)Instructions[i].Member)->VTableIndex < 256) Instructions[i].Opcode = OPC_PushVFuncB;
        break;
      case OPC_VCall:
        // make sure class virtual table has been calculated
        Instructions[i].Member->Outer->PostLoad();
        if (((VMethod *)Instructions[i].Member)->VTableIndex < 256) Instructions[i].Opcode = OPC_VCallB;
        break;
      case OPC_DelegateCall:
        // make sure struct / class field offsets have been calculated
        Instructions[i].Member->Outer->PostLoad();
             if (((VField *)Instructions[i].Member)->Ofs < 256) Instructions[i].Opcode = OPC_DelegateCallB;
        else if (((VField *)Instructions[i].Member)->Ofs <= MAX_VINT16) Instructions[i].Opcode = OPC_DelegateCallS;
        break;
      case OPC_Offset:
      case OPC_FieldValue:
      case OPC_VFieldValue:
      case OPC_PtrFieldValue:
      case OPC_StrFieldValue:
      case OPC_ByteFieldValue:
      case OPC_Bool0FieldValue:
      case OPC_Bool1FieldValue:
      case OPC_Bool2FieldValue:
      case OPC_Bool3FieldValue:
        // make sure struct / class field offsets have been calculated
        Instructions[i].Member->Outer->PostLoad();
             if (((VField *)Instructions[i].Member)->Ofs < 256) Instructions[i].Opcode += 2;
        else if (((VField *)Instructions[i].Member)->Ofs <= MAX_VINT16) ++Instructions[i].Opcode;
        break;
      case OPC_ArrayElement:
             if (Instructions[i].TypeArg.GetSize() < 256) Instructions[i].Opcode = OPC_ArrayElementB;
        else if (Instructions[i].TypeArg.GetSize() < MAX_VINT16) Instructions[i].Opcode = OPC_ArrayElementS;
        break;
      case OPC_PushName:
             if (Instructions[i].NameArg.GetIndex() < 256) Instructions[i].Opcode = OPC_PushNameB;
        else if (Instructions[i].NameArg.GetIndex() < MAX_VINT16) Instructions[i].Opcode = OPC_PushNameS;
        break;
    }

    // calculate approximate addresses for jump instructions
    Instructions[i].Address = Addr;
    switch (StatementInfo[Instructions[i].Opcode].Args) {
      case OPCARGS_None:
        ++Addr;
        break;
      case OPCARGS_Member:
      case OPCARGS_String:
        Addr += 1+sizeof(void*);
        break;
      case OPCARGS_BranchTargetB:
      case OPCARGS_BranchTargetNB:
      case OPCARGS_Byte:
      case OPCARGS_NameB:
      case OPCARGS_FieldOffsetB:
      case OPCARGS_VTableIndexB:
      case OPCARGS_TypeSizeB:
        Addr += 2;
        break;
      case OPCARGS_BranchTargetS:
      case OPCARGS_Short:
      case OPCARGS_NameS:
      case OPCARGS_FieldOffsetS:
      case OPCARGS_VTableIndex:
      case OPCARGS_VTableIndexB_Byte:
      case OPCARGS_FieldOffsetB_Byte:
      case OPCARGS_TypeSizeS:
        Addr += 3;
        break;
      case OPCARGS_ByteBranchTarget:
      case OPCARGS_VTableIndex_Byte:
      case OPCARGS_FieldOffsetS_Byte:
        Addr += 4;
        break;
      case OPCARGS_BranchTarget:
      case OPCARGS_ShortBranchTarget:
      case OPCARGS_Int:
      case OPCARGS_Name:
      case OPCARGS_FieldOffset:
      case OPCARGS_TypeSize:
        Addr += 5;
        break;
      case OPCARGS_FieldOffset_Byte:
        Addr += 6;
        break;
      case OPCARGS_IntBranchTarget:
        Addr += 7;
        break;
      case OPCARGS_Type:
        Addr += 9+sizeof(void*);
        break;
    }
  }

  // now do jump instructions
  vint32 Offs;
  for (int i = 0; i < Instructions.Num()-1; ++i) {
    switch (StatementInfo[Instructions[i].Opcode].Args) {
    case OPCARGS_BranchTarget:
      Offs = Instructions[Instructions[i].Arg1].Address-Instructions[i].Address;
           if (Offs >= 0 && Offs < 256) Instructions[i].Opcode -= 3;
      else if (Offs < 0 && Offs > -256) Instructions[i].Opcode -= 2;
      else if (Offs >= MIN_VINT16 && Offs <= MAX_VINT16) Instructions[i].Opcode -= 1;
      break;
    }
  }
  Instructions[Instructions.Num()-1].Address = Addr;
  unguard;
}


//==========================================================================
//
//  VMethod::FindPCLocation
//
//==========================================================================
TLocation VMethod::FindPCLocation (const vuint8 *pc) {
  if (!pc || Statements.length() == 0) return TLocation();
  if (pc < Statements.Ptr()) return TLocation();
  size_t stidx = (size_t)(pc-Statements.Ptr());
  if (stidx >= (size_t)Statements.length()) return TLocation();
  if (stidx >= (size_t)StatLocs.length()) return TLocation(); // just in case
  return StatLocs[(int)stidx];
}


//==========================================================================
//
//  operator <<
//
//==========================================================================
VStream &operator << (VStream &Strm, FInstruction &Instr) {
  vuint8 Opc;
  if (Strm.IsLoading()) {
    Strm << Opc;
    Instr.Opcode = Opc;
  } else {
    Opc = Instr.Opcode;
    Strm << Opc;
  }
  Strm << Instr.loc;
  switch (StatementInfo[Opc].Args) {
    case OPCARGS_None:
      break;
    case OPCARGS_Member:
    case OPCARGS_FieldOffset:
    case OPCARGS_VTableIndex:
      Strm << Instr.Member;
      break;
    case OPCARGS_VTableIndex_Byte:
    case OPCARGS_FieldOffset_Byte:
      Strm << Instr.Member;
      Strm << STRM_INDEX(Instr.Arg2);
      break;
    case OPCARGS_BranchTarget:
      Strm << Instr.Arg1;
      break;
    case OPCARGS_ByteBranchTarget:
    case OPCARGS_ShortBranchTarget:
    case OPCARGS_IntBranchTarget:
      Strm << STRM_INDEX(Instr.Arg1);
      Strm << Instr.Arg2;
      break;
    case OPCARGS_Byte:
    case OPCARGS_Short:
    case OPCARGS_Int:
      Strm << STRM_INDEX(Instr.Arg1);
      break;
    case OPCARGS_Name:
      Strm << Instr.NameArg;
      break;
    case OPCARGS_String:
      Strm << Instr.Arg1;
      break;
    case OPCARGS_TypeSize:
    case OPCARGS_Type:
      Strm << Instr.TypeArg;
      break;
  }
  return Strm;
}
