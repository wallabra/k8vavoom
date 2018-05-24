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

  VLocalVarDef () {}
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

  // returns index in `LocalDefs`
  int CheckForLocalVar (VName Name);

  VLabel DefineLabel ();
  void MarkLabel (VLabel l);

  void AddStatement (int statement);
  void AddStatement (int statement, int parm1);
  void AddStatement (int statement, float FloatArg);
  void AddStatement (int statement, VName NameArg);
  void AddStatement (int statement, VMemberBase *Member);
  void AddStatement (int statement, VMemberBase *Member, int Arg);
  void AddStatement (int statement, const VFieldType &TypeArg);
  void AddStatement (int statement, VLabel Lbl);
  void AddStatement (int statement, int p, VLabel Lbl);
  void EmitPushNumber (int Val);
  void EmitLocalAddress (int Ofs);
  void EmitClearStrings (int Start, int End);

  VArrayElement *SetIndexArray (VArrayElement *el); // returns previous
};


// ////////////////////////////////////////////////////////////////////////// //
struct VStatementInfo {
  const char *name;
  int Args;
  int usecount;
};

extern VStatementInfo StatementInfo[NUM_OPCODES];
