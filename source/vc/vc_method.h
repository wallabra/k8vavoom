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
//TODO: create separate optimizer module: it can be completely independent
//      of anything else.


//==========================================================================
//
//  Method flags
//
//==========================================================================
enum {
  FUNC_Native      = 0x0001, // Native method
  FUNC_Static      = 0x0002, // Static method
  FUNC_VarArgs     = 0x0004, // Variable argument count
  FUNC_Final       = 0x0008, // Final version of a method
  FUNC_Spawner     = 0x0010, // Automatic cast of return value
  FUNC_Net         = 0x0020, // Method is network-replicated
  FUNC_NetReliable = 0x0040, // Sent reliably over the network
  FUNC_Iterator    = 0x0080, // Can be used in foreach statements

  FUNC_Override    = 0x1000, // Used to check overrides
  FUNC_Private     = 0x2000,
  FUNC_Protected   = 0x4000,

  // "real final" method -- i.e. it has `FUNC_Final` set, and it is not in VMT
  FUNC_RealFinal   = 0x8000, // set in postload processor

  FUNC_NetFlags = FUNC_Net|FUNC_NetReliable,
  FUNC_ProtectionFlags = FUNC_Override|FUNC_Private|FUNC_Protected|FUNC_RealFinal,
};


//==========================================================================
//
//  Parameter flags
//
//==========================================================================
enum {
  FPARM_Optional = 0x01,
  FPARM_Out      = 0x02,
  FPARM_Ref      = 0x04,
  FPARM_Const    = 0x08,
};


//==========================================================================
//
//  builtin_t
//
//==========================================================================
typedef void (*builtin_t) ();


//==========================================================================
//
//  FBuiltinInfo
//
//==========================================================================
class FBuiltinInfo {
  const char *Name;
  VClass *OuterClass;
  builtin_t Func;
  FBuiltinInfo *Next;

  static FBuiltinInfo *Builtins;

  friend class VMethod;

public:
  FBuiltinInfo (const char *InName, VClass *InClass, builtin_t InFunc) : Name(InName), OuterClass(InClass), Func(InFunc) {
    Next = Builtins;
    Builtins = this;
  }
};


//==========================================================================
//
//  FInstruction
//
//==========================================================================
struct FInstruction {
  vint32 Address;
  vint32 Opcode;
  vint32 Arg1;
  vint32 Arg2;
  bool Arg1IsFloat;
  VMemberBase *Member;
  VName NameArg;
  VFieldType TypeArg;
  TLocation loc;

  FInstruction () : Address(0), Opcode(0), Arg1(0), Arg2(0), Arg1IsFloat(false), Member(nullptr), NameArg(NAME_None), TypeArg(TYPE_Unknown), loc(TLocation()) {}

  friend VStream &operator << (VStream &, FInstruction &);
};


//==========================================================================
//
//  VMethodParam
//
//==========================================================================
class VMethodParam {
public:
  VExpression *TypeExpr;
  VName Name;
  TLocation Loc;

  VMethodParam ();
  //VMethodParam (const VMethodParam &v);
  ~VMethodParam ();

  void clear ();

  // WARNING: assignment and copy ctors WILL CREATE syntax copy of `TypeExpr`!!!
  //VMethodParam &operator = (const VMethodParam &v);
};


//==========================================================================
//
//  VMethod
//
//==========================================================================
class VMethod : public VMemberBase {
private:
  bool mPostLoaded;

public:
  enum { MAX_PARAMS = 32 };

  // persistent fields
  vint32 NumLocals;
  vint32 Flags;
  VFieldType ReturnType;
  vint32 NumParams;
  vint32 ParamsSize;
  VFieldType ParamTypes[MAX_PARAMS];
  vuint8 ParamFlags[MAX_PARAMS];
  TArray<FInstruction> Instructions;
  VMethod *SuperMethod;
  VMethod *ReplCond;

  // compiler fields
  VExpression *ReturnTypeExpr;
  VMethodParam Params[MAX_PARAMS]; // param name will be serialized
  VStatement *Statement;
  VName SelfTypeName;
  vint32 lmbCount; // number of defined lambdas, used to create lambda names
  // native vararg method can have `printf` attribute
  vint32 printfFmtArgIdx; // -1 if no, or local index
  vint32 builtinOpc; // -1: not a builtin

  // run-time fields
  vuint32 Profile1;
  vuint32 Profile2;
  TArray<vuint8> Statements;
  TArray<TLocation> StatLocs; // locations for each code point
  builtin_t NativeFunc;
  vint16 VTableIndex;
  vint32 NetIndex;
  VMethod *NextNetMethod;

  VMethod (VName, VMemberBase *, TLocation);
  virtual ~VMethod () override;
  virtual void CompilerShutdown () override;

  virtual void Serialise (VStream &) override;
  bool Define ();
  void Emit ();
  void DumpAsm ();
  virtual void PostLoad () override;

  // this can be called in `ExecuteNetMethod()` to do cleanup after RPC
  // should not be called for vararg methods
  void CleanupParams () const;

  TLocation FindPCLocation (const vuint8 *pc);

  friend inline VStream &operator << (VStream &Strm, VMethod *&Obj) { return Strm << *(VMemberBase**)&Obj; }

  // this is public for VCC
  void OptimizeInstructions ();

  // <0: not found
  int FindArgByName (VName aname) const;

private:
  void CompileCode ();
};
