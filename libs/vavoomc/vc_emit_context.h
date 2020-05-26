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
//**  Copyright (C) 2018-2020 Ketmar Dark
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

// ////////////////////////////////////////////////////////////////////////// //
class VEmitContext;
class VArrayElement;
class VStatement;


// ////////////////////////////////////////////////////////////////////////// //
class VLabel {
private:
  friend class VEmitContext;

  int Index;

  VLabel (int AIndex) : Index(AIndex) {}

public:
  VLabel () : Index(-1) {}
  inline bool IsDefined () const { return (Index != -1); }
  inline bool operator == (const VLabel &b) const { return (Index == b.Index); }
};


// ////////////////////////////////////////////////////////////////////////// //
class VLocalVarDef {
public:
  VName Name;
  TLocation Loc;
  int Offset;
  VFieldType Type;
  bool Visible;
  bool WasRead; // reading from local will set this
  bool WasWrite; // writing to local will set this
  vuint32 ParamFlags;
  // internal index; DO NOT CHANGE!
  int ldindex;
  bool reused;

private:
  int stackSize; // for reusing
  bool invalid; // can be set by allocator

public:
  VLocalVarDef () {}

  inline int GetIndex () const noexcept { return ldindex; }

  friend class VEmitContext; // it needs access to `compIndex`
};


// ////////////////////////////////////////////////////////////////////////// //
class VEmitContext {
  friend class VScopeGuard;
  friend class VAutoFin;

public:
  struct VCompExit {
    int lidx; // local index
    TLocation loc;
    bool inLoop;
  };

private:
  struct VLabelFixup {
    int Pos;
    int LabelIdx;
    int Arg;
  };

  TArray<int> Labels;
  TArray<VLabelFixup> Fixups;
  TArray<VLocalVarDef> LocalDefs;

  struct VGotoListItem {
    VLabel jlbl;
    VName name;
    TLocation loc;
    bool defined;
  };

  TArray<VGotoListItem> GotoLabels;

public:
  VMethod *CurrentFunc;
  // both class and struct can be null (package context), or one of them can be set (but not both)
  VClass *SelfClass;
  VStruct *SelfStruct;
  VPackage *Package;
  VArrayElement *IndArray;
  // this is set for structs
  VClass *OuterClass;

  VFieldType FuncRetType;

  //int localsofs;
  //FIXME: rewrite this!
  enum { MaxStackSlots = 1024 };
  enum {
    SlotUnused = 0u,
    SlotUsed = 1u,
    SlotFree = 2u,
  };
  unsigned char slotInfo[MaxStackSlots];

  bool InDefaultProperties;
  bool VCallsDisabled;

  // set by `VReturn`, to process `scope(return)`
  bool InReturn;

private:
  // called in ctor, and to reset locals
  void stackInit ();

  // size in stack slots; returns -1 on error
  int stackAlloc (int size, bool *reused);
  void stackFree (int pos, int size);

public:
  VEmitContext (VMemberBase *Member);
  void EndCode ();

  // this doesn't modify `localsofs`
  void ClearLocalDefs ();

  // allocates new local, don't set offset yet
  VLocalVarDef &NewLocal (VName aname, const VFieldType &atype, const TLocation &aloc, vuint32 pflags=0);

  // allocate stack slot for this local, and set local offset
  // sets `reused` flag in local
  void AllocateLocalSlot (int idx);
  // release stack slot for this local, and reset local offset
  void ReleaseLocalSlot (int idx);

  // reserve stack slots; used to setup function arguments
  int ReserveStack (int size);

  // reserve slot for this local; used to setup function arguments
  void ReserveLocalSlot (int idx);

  int CalcUsedStackSize () const noexcept;

  // allocates new local, sets offset
  //VLocalVarDef &AllocLocal (VName aname, const VFieldType &atype, const TLocation &aloc);

  VLocalVarDef &GetLocalByIndex (int idx) noexcept;

  void MarkLocalReadByIdx (int idx) noexcept;
  void MarkLocalWrittenByIdx (int idx) noexcept;
  void MarkLocalUsedByIdx (int idx) noexcept;

  bool IsLocalReadByIdx (int idx) const noexcept;
  bool IsLocalWrittenByIdx (int idx) const noexcept;
  bool IsLocalUsedByIdx (int idx) const noexcept;

  inline int GetLocalDefCount () const noexcept { return LocalDefs.length(); }

  // returns index in `LocalDefs`
  int CheckForLocalVar (VName Name);
  VFieldType GetLocalVarType (int idx);

  // this creates new label (without a destination yet)
  VLabel DefineLabel ();
  // this sets label destination
  void MarkLabel (VLabel l);

  void AddStatement (int statement, const TLocation &aloc);
  void AddStatement (int statement, int parm1, const TLocation &aloc);
  void AddStatement (int statement, float FloatArg, const TLocation &aloc);
  void AddStatement (int statement, VName NameArg, const TLocation &aloc);
  void AddStatement (int statement, VMemberBase *Member, const TLocation &aloc);
  void AddStatement (int statement, VMemberBase *Member, int Arg, const TLocation &aloc);
  void AddStatement (int statement, const VFieldType &TypeArg, const TLocation &aloc);
  void AddStatement (int statement, const VFieldType &TypeArg, int Arg, const TLocation &aloc);
  void AddStatement (int statement, const VFieldType &TypeArg, const VFieldType &TypeArg1, int OpCode, const TLocation &aloc);
  void AddStatement (int statement, VLabel Lbl, const TLocation &aloc);
  void AddStatement (int statement, int p, VLabel Lbl, const TLocation &aloc);
  void AddBuiltin (int b, const TLocation &aloc);

  void EmitPushNumber (int Val, const TLocation &aloc);
  void EmitLocalAddress (int Ofs, const TLocation &aloc);
  void EmitLocalValue (int lcidx, const TLocation &aloc, int xofs=0);
  void EmitLocalPtrValue (int lcidx, const TLocation &aloc, int xofs=0);
  void EmitPushPointedCode (VFieldType type, const TLocation &aloc);

  // this assumes that dtor is called on the local (i.e. no need to do any finalization)
  // if `forced` is not specified, it will avoid zeroing something that should be zeroed by a dtor
  void EmitLocalZero (int locidx, const TLocation &aloc);

  // this won't zero local
  void EmitLocalDtor (int locidx, const TLocation &aloc);

  void EmitGotoTo (VName lblname, const TLocation &aloc);
  void EmitGotoLabel (VName lblname, const TLocation &aloc);

  VArrayElement *SetIndexArray (VArrayElement *el); // returns previous

  bool IsReturnAllowed ();
};


// ////////////////////////////////////////////////////////////////////////// //
struct VStatementInfo {
  const char *name;
  int Args;
  int usecount;
};

struct VStatementBuiltinInfo {
  const char *name;
};

extern VStatementInfo StatementInfo[NUM_OPCODES];
extern VStatementBuiltinInfo StatementBuiltinInfo[];
extern VStatementBuiltinInfo StatementDictDispatchInfo[];
extern VStatementBuiltinInfo StatementDynArrayDispatchInfo[];
