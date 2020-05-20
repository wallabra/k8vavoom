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
  bool Reusable; // if this is `true`, and `Visible` is false, local can be reused
  bool Unused; // "releasing" local sets this
  bool WasRead; // reading from local will set this
  bool WasWrite; // writing to local will set this
  vuint8 ParamFlags;
  // internal index; DO NOT CHANGE!
  int ldindex;
  bool reused;

private:
  int compIndex; // for enter/exit compound
  int stackSize; // for reusing

public:
  VLocalVarDef () {}

  inline int GetCompIndex () const { return compIndex; } // for debugging

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

  int localsofs;

  bool InDefaultProperties;
  bool VCallsDisabled;

public:
  VEmitContext (VMemberBase *Member);
  void EndCode ();

  // this doesn't modify `localsofs`
  void ClearLocalDefs ();

  // allocates new local, sets offset
  VLocalVarDef &AllocLocal (VName aname, const VFieldType &atype, const TLocation &aloc);
  // mark this local as unused (and optionally mark it as "reusable")
  void ReleaseLocal (int idx, bool allowReuse=true);

  VLocalVarDef &GetLocalByIndex (int idx);

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

  // this assumes that dtor is called on the local
  // if `forced` is not specified, it will avoid zeroing something that should be zeroed by a dtor
  void EmitLocalZero (int locidx, const TLocation &aloc, bool forced=false);

  // this won't zero local
  void EmitLocalDtor (int locidx, const TLocation &aloc, bool zeroIt=false);

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
