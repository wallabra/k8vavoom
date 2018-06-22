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


// ////////////////////////////////////////////////////////////////////////// //
class VLabel {
private:
  friend class VEmitContext;

  int Index;

  VLabel (int AIndex) : Index(AIndex) {}

public:
  VLabel () : Index(-1) {}
  inline bool IsDefined () const { return (Index != -1); }
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
private:
  struct VLabelFixup {
    int Pos;
    int LabelIdx;
    int Arg;
  };

  TArray<int> Labels;
  TArray<VLabelFixup> Fixups;
  TArray<VLocalVarDef> LocalDefs;
  int compindex;

public:
  VMethod *CurrentFunc;
  VClass *SelfClass;
  VPackage *Package;
  VArrayElement *IndArray;

  VFieldType FuncRetType;

  int localsofs;

  VLabel LoopStart;
  VLabel LoopEnd;

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
  void AddStatement (int statement, VLabel Lbl, const TLocation &aloc);
  void AddStatement (int statement, int p, VLabel Lbl, const TLocation &aloc);
  void AddBuiltin (int b, const TLocation &aloc);
  void EmitPushNumber (int Val, const TLocation &aloc);
  void EmitLocalAddress (int Ofs, const TLocation &aloc);
  void EmitClearStrings (int Start, int End, const TLocation &aloc);

  VArrayElement *SetIndexArray (VArrayElement *el); // returns previous
};


// ////////////////////////////////////////////////////////////////////////// //
struct VStatementInfo {
  const char *name;
  int Args;
  int usecount;
};

extern VStatementInfo StatementInfo[NUM_OPCODES];
