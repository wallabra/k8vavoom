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


// ////////////////////////////////////////////////////////////////////////// //
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
  vuint8 ParamFlags;
  // internal index; DO NOT CHANGE!
  int ldindex;
  bool reused;

private:
  int compindex; // for enter/exit compound
  int stackSize; // for reusing

public:
  VLocalVarDef () {}

  inline int GetCompIndex () const { return compindex; } // for debugging

  friend class VEmitContext; // it needs access to `compindex`
};


// ////////////////////////////////////////////////////////////////////////// //
class VEmitContext {
  friend class VAutoFin;

private:
  struct VBreakCont;

  struct VFinalizer {
    int rc;
    VEmitContext *ec;
    VFinalizer *prev;
    VStatement *st;
    VBreakCont *bc;

    VFinalizer () : rc(0), ec(nullptr), prev(nullptr), st(nullptr), bc(nullptr) {}

    inline void incRef () { ++rc; }
    inline void decRef () { if (--rc == 0) die(); }

    void die ();
    void emit ();
  };

  enum BCType {
    Break,
    Continue,
    Block,
  };

  struct VBreakCont {
    int rc;
    VEmitContext *ec;
    VBreakCont *prev;
    VLabel lbl;
    BCType type;

    VBreakCont () : rc(0), ec(nullptr), prev(nullptr), lbl(), type(BCType::Break) {}

    inline void incRef () { ++rc; }
    inline void decRef () { if (--rc == 0) die(); }

    void die ();
    void emitFinalizers (); // not including ours
  };

  struct VLabelFixup {
    int Pos;
    int LabelIdx;
    int Arg;
  };

  TArray<int> Labels;
  TArray<VLabelFixup> Fixups;
  TArray<VLocalVarDef> LocalDefs;
  int compindex;

  struct VGotoListItem {
    VLabel jlbl;
    VName name;
    TLocation loc;
    bool defined;
  };

  TArray<VGotoListItem> GotoLabels;

  VFinalizer *lastFin;
  VBreakCont *lastBC;

public:
  class VAutoFin {
    friend class VEmitContext;

  private:
    VFinalizer *fin;

  private:
    VAutoFin (VFinalizer *afin);

  public:
    VAutoFin () : fin(nullptr) {}
    VAutoFin (const VAutoFin &);
    void operator = (const VAutoFin &);

    ~VAutoFin ();
  };

  class VAutoBreakCont {
    friend class VEmitContext;

  private:
    VBreakCont *bc;

  private:
    VAutoBreakCont (VBreakCont *abc);

    void emitOurFins ();
    void emitFins (); // without ours

  public:
    VAutoBreakCont () : bc(nullptr) {}
    VAutoBreakCont (const VAutoBreakCont &);
    void operator = (const VAutoBreakCont &);

    ~VAutoBreakCont ();

    // calls `MarkLabel()`
    void Mark ();

    // returns label, doesn't generate finalizing code
    VLabel GetLabelNoFinalizers ();

    // emit finalizers, so you can safely jump to the returned label
    // note that finalizers, registered with this object, will *NOT* be emited!
    VLabel GetLabel ();
  };

private:
  VAutoBreakCont DefineBreakCont (BCType atype);

public:
  VMethod *CurrentFunc;
  VClass *SelfClass;
  VPackage *Package;
  VArrayElement *IndArray;

  VFieldType FuncRetType;

  int localsofs;

  //VLabel LoopStart;
  //VLabel LoopEnd;

  bool InDefaultProperties;

  VEmitContext (VMemberBase *Member);
  void EndCode ();

  // this doesn't modify `localsofs`
  void ClearLocalDefs ();

  // allocates new local, sets offset
  VLocalVarDef &AllocLocal (VName aname, const VFieldType &atype, const TLocation &aloc);

  VLocalVarDef &GetLocalByIndex (int idx);

  inline int GetLocalDefCount () const { return LocalDefs.length(); }

  // compound statement will call these functions; exiting will mark all allocated vars for reusing
  int EnterCompound (); // returns compound index
  void ExitCompound (int cidx); // pass result of `EnterCompound()` to this

  inline int GetCurrCompIndex () const { return compindex; } // for debugging

  // returns index in `LocalDefs`
  int CheckForLocalVar (VName Name);
  VFieldType GetLocalVarType (int idx);

  VLabel DefineLabel ();
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
  void EmitLocalDtors (int Start, int End, const TLocation &aloc, bool zeroIt=false);

  // returns `true` if dtor was emited
  void EmitOneLocalDtor (int locidx, const TLocation &aloc, bool zeroIt=false);

  void EmitGotoTo (VName lblname, const TLocation &aloc);
  void EmitGotoLabel (VName lblname, const TLocation &aloc);

  VArrayElement *SetIndexArray (VArrayElement *el); // returns previous

  // use this to register block finalizer that will be called on `return`, or
  // when `VAutoFin` object is destroyed
  VAutoFin RegisterFinalizer (VStatement *st);

  VAutoFin RegisterLoopFinalizer (VStatement *st);

  // emit all currently registered finalizers, from last to first; used in `return`
  void EmitFinalizers ();

  // the flow is like that:
  //   each registered finalizer is marked with the current break/cont label
  //   emiting `break` will emit all finalizers NOT including finalizers
  //   registered for the given label.
  //   i.e. each loop block should call `MarkXXX()` *BEFORE* autodestruction of `VAutoFin`
  //   that is:
  //     {
  //       auto brk = ec.DefineBreak();
  //       brk.RegisterLoopFinalizer(this);
  //       <generate some code>
  //       brk.MarkBreak();
  //     } // here, `brk` will be destroyed, and "break finalizer" code will be generated

  // autodestructible
  VAutoBreakCont DefineBreak ();
  VAutoBreakCont DefineContinue ();

  VAutoBreakCont BlockBreakContReturn ();

  // returns success flag
  bool EmitBreak (const TLocation &loc);
  // returns success flag
  bool EmitContinue (const TLocation &loc);

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
