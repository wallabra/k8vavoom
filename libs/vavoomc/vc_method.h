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
//**  the Free Software Foundation, version 3 of the License ONLY.
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

//==========================================================================
//
//  Method flags
//
//==========================================================================
enum {
  FUNC_Native      = 0x0001, // native method
  FUNC_Static      = 0x0002, // static method
  FUNC_VarArgs     = 0x0004, // variable argument count
  FUNC_Final       = 0x0008, // final version of a method
  FUNC_Spawner     = 0x0010, // automatic cast of return value
  FUNC_Net         = 0x0020, // method is network-replicated
  FUNC_NetReliable = 0x0040, // sent reliably over the network
  FUNC_Iterator    = 0x0080, // can be used in foreach statements

  FUNC_Decorate    = 0x0100, // can be called from decorate code
  // do not allow calls to virtual methods inside this one
  // this is used in synthesized state methods, because they can be called with arbitrary `self`
  FUNC_NoVCalls    = 0x0200,

  FUNC_Override    = 0x1000, // used to check overrides
  FUNC_Private     = 0x2000,
  FUNC_Protected   = 0x4000,

  // non-virtual method -- i.e. it has `FUNC_Final` set, and it is not in VMT
  //k8: i don't remember why i introduced this
  //FUNC_NonVirtual  = 0x8000, // set in postload processor

  FUNC_NetFlags = FUNC_Net|FUNC_NetReliable,
  FUNC_ProtectionFlags = FUNC_Override|FUNC_Private|FUNC_Protected/*|FUNC_NonVirtual*/,
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
  FBuiltinInfo (const char *InName, VClass *InClass, builtin_t InFunc);
};


//==========================================================================
//
//  FInstruction
//
//==========================================================================
struct FInstruction {
  //vint32 Address;
  vint32 Opcode;
  vint32 Arg1;
  vint32 Arg2;
  bool Arg1IsFloat;
  VMemberBase *Member;
  VName NameArg;
  VFieldType TypeArg;
  VFieldType TypeArg1;
  TLocation loc;

  FInstruction () : /*Address(0),*/ Opcode(0), Arg1(0), Arg2(0), Arg1IsFloat(false), Member(nullptr), NameArg(NAME_None), TypeArg(TYPE_Unknown), loc(TLocation()) {}

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
  TArray<VName> NamedFlags; // used in decorate

  inline VMethodParam () : TypeExpr(nullptr), Name(NAME_None), Loc() {}
  inline ~VMethodParam () { clear(); }

  // this has to be non-inline due to `delete TypeExpr`
  void clear ();
};


//==========================================================================
//
//  VMethod
//
//==========================================================================
class VMethod : public VMemberBase {
private:
  bool mPostLoaded;

  // used in codegen
  // write binary type info to `Statements`
  void WriteType (const VFieldType &tp);

public:
  enum { MAX_PARAMS = 32 };

  // persistent fields
  vint32 NumLocals;
  vint32 Flags;
  VFieldType ReturnType;
  vint32 NumParams;
  vint32 ParamsSize;
  VFieldType ParamTypes[MAX_PARAMS+1];
  vuint8 ParamFlags[MAX_PARAMS+1];
  TArray<FInstruction> Instructions;
  VMethod *SuperMethod;
  VMethod *ReplCond;

  // compiler fields
  VExpression *ReturnTypeExpr;
  VMethodParam Params[MAX_PARAMS+1]; // param name will be serialized
  VStatement *Statement;
  VName SelfTypeName; // used for `self(Class)`, required for some decorate methods
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
  vint16 VTableIndex; // -666 means "not determined yet"
  vint32 NetIndex;
  VMethod *NextNetMethod;
  VClass *SelfTypeClass; // set to non-nullptr if `SelfTypeName` is non-empty; set from `VEmitContext`

  // guard them, why not?
  int defineResult; // -1: not called yet
  bool emitCalled;

public:
  VMethod (VName, VMemberBase *, TLocation);
  virtual ~VMethod () override;
  virtual void CompilerShutdown () override;

  virtual void Serialise (VStream &) override;

  // this resolves return type, parameter types, sets `SuperMethod`, and fixes flags
  // must be called before `Emit()`
  bool Define ();

  // this resolves and emits IR code for statements, and optimises that code
  // must be called before `PostLoad()`
  void Emit ();

  // dumps generated IR code; can be called after `Emit()`, has no sense after `PostLoad()`
  void DumpAsm ();

  // this compiles IR instructions to VM opcodes
  // it also resolves builtins
  // this must be called last (i.e. after `Define()` and `Emit()`)
  // this fills `Statements`, and clears `Instructions`
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

  inline bool IsDefined () const { return (defineResult >= 0); }

  inline bool IsStatic () const { return !!(Flags&FUNC_Static); }
  // valid only after codegen phase
  //inline bool IsVirtual () const { return !(Flags&FUNC_NonVirtual); } // you can use `VTableIndex >= 0` too
  // valid only after `PostLoad()` call
  inline bool IsVirtual () const { return (VTableIndex >= 0); }
  inline bool IsNonVirtual () const { return (VTableIndex < 0); }

  inline bool IsPostLoaded () const { return (VTableIndex >= -1); }

  // is this method suitable for various "normal" calls?
  inline bool IsNormal () const { return ((Flags&(FUNC_VarArgs|FUNC_Spawner|FUNC_Iterator)) == 0); }

  inline bool IsNetwork () const { return ((Flags&(FUNC_Net|FUNC_NetReliable)) != 0); }

  // called from decorate parser, mostly
  // if we're calling a "good" method, there is no need to create a wrapper
  // not sure what to do with network methods, though
  // ok, state methods can be virtual and static now
  inline bool IsGoodStateMethod () const { return (NumParams == 0 && (Flags&~(FUNC_Native|FUNC_Spawner|FUNC_Net|FUNC_NetReliable|FUNC_Static/*|FUNC_NonVirtual*/)) == /*FUNC_Final*/0); }

  // this must be called on a postloaded method only
  inline VClass *GetSelfClass () {
    if (SelfTypeClass) return SelfTypeClass;
    for (VMemberBase *mt = this; mt; mt = mt->Outer) {
      if (mt->isClassMember()) return (VClass *)mt;
      if (mt->isStateMember()) return nullptr; // this is state wrapper
    }
    return nullptr;
  }

private:
  // this generates VM (or other) executable code (to `Statements`) from IR `Instructions`
  void GenerateCode ();
};
